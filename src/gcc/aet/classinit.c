/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

This file is part of AET.

AET is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

AET is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC Exception along with this program; see the file COPYING3.
If not see <http://www.gnu.org/licenses/>.
AET was originally developed  by the zclei@sina.com at guiyang china .
*/

#include "config.h"
#include <cstdio>
#define INCLUDE_UNIQUE_PTR
#define INCLUDE_MEMORY
#include "system.h"
#include "coretypes.h"
#include "target.h"
#include "c-family/c-pragma.h"
#include "c/c-tree.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "aet-c-parser-header.h"

#include "aetutils.h"
#include "classmgr.h"
#include "classinit.h"
#include "aetinfo.h"
#include "c-aet.h"
#include "classinit.h"
#include "aetprinttree.h"
#include "aetprinttoken.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "classbuild.h"
#include "classinterface.h"
#include "classimpl.h"
#include "mtcsparser.h"
#include "classparser.h"


/**
 * 重要的改变：2022-02-26之前
 * 给父类的域方法或父类实现的接口方法赋值都是在newstrategy.c的
 * addMiddleCodes方法调用class_init_override_parent_ref.
 * 但这会丢失在子类中定义的fromImplDefine函数(见ClassFunc.c)
 * 所以给父类赋值改成在子类的实现后的初始化方法中。就不会出现函数“丢失”
 * 的问题。
 */

static char *modifyParentMethod(ClassInit *self,ClassName *childName);
static void optimizeGenericCall(ClassInit *self,ClassName *className,NString *codes);

static void classInitInit(ClassInit *self)
{
}

static nboolean isValidDeclOrDefine(tree func)
{
	return (func && func!=NULL_TREE && func!=error_mark_node);
}


//_Z10HomeOffice10HomeOfficeEPN11_HomeOfficeE
static int getRemainParmString(char *newName,char *className,char *funcName,char *remainParm)
{
	if(newName[0]!='_')
		return 0;
	if(newName[1]!='Z')
		return 0;
    if(!n_ascii_isdigit(newName[2]))
    	return 0;
    char classNameLen[strlen(newName)];
    int i=2;
    int count=0;
    while(n_ascii_isdigit(newName[i])){
    	classNameLen[count++]=newName[i];
        i++;
    }
    classNameLen[count]='\0';
    int len=atoi((const char*)classNameLen);
    memcpy(className,newName+i,len);
    className[len]='\0';
    i+=len;
    count=0;
    while(n_ascii_isdigit(newName[i])){
    	classNameLen[count++]=newName[i];
        i++;
    }
    classNameLen[count]='\0';
    len=atoi((const char*)classNameLen);
    memcpy(funcName,newName+i,len);
    funcName[len]='\0';
    i+=len;
    char *remain=newName+i;
    i+=3;//跳过EPN
    char *remain1=newName+i;
    count=0;
    while(n_ascii_isdigit(newName[i])){
    	classNameLen[count++]=newName[i];
        i++;
    }
    classNameLen[count]='\0';
    len=atoi((const char*)classNameLen);
    i+=len;
    char *last=newName+i;
    int remainStrLen=strlen(newName)-i;
    memcpy(remainParm,newName+i,remainStrLen);
    remainParm[remainStrLen]='\0';
    return len;
}

static nboolean compareMangle(char *childMangle,char *parentMangle)
{
	char childName[255];
	char childOrgiName[255];
	char childRemainParm[255];
	int len=getRemainParmString(childMangle,childName,childOrgiName,childRemainParm);
	if(len==0)
		return FALSE;
	char parentName[255];
	char parentOrgiName[255];
	char parentRemainParm[255];
	len=getRemainParmString(parentMangle,parentName,parentOrgiName,parentRemainParm);
	if(len==0)
		return FALSE;
	n_debug("class_init compareMangle %s %s %s %s %s %s\n",childName,childOrgiName,childRemainParm,parentName,parentOrgiName,parentRemainParm);
	return strcmp(childRemainParm,parentRemainParm)==0;
}

/**
 * 比较两个域的返回值是否相同
 */
static nboolean compareFunctionType(tree define,tree field)
{
    tree returnType=NULL_TREE;
    if(TREE_CODE(define)!=FIELD_DECL){
        tree funType=TREE_TYPE(define);
        returnType=TREE_TYPE(funType);
    }else{
        tree fieldType0=TREE_TYPE(define);
        tree fieldFunType0=TREE_TYPE(fieldType0);
        returnType=TREE_TYPE(fieldFunType0);
    }
    tree fieldType=TREE_TYPE(field);
    tree fieldFunType=TREE_TYPE(fieldType);
    tree fieldReturnType=TREE_TYPE(fieldFunType);
    bool re=c_tree_equal (returnType,fieldReturnType);
    return re;
}

typedef struct _OverrideFuncs{
	 ClassName *childName;
	 ClassName *belongChildClass;
	 char *childMangleName;
	 ClassName *parentName;
	 ClassName *belongParentClass;
	 char *parentOrgiName;
	 char *parentMangleName;
	 int   error;
	 tree  childFieldDecl;
}OverrideFuncs;

static void freeOverrideFuncData_cb(OverrideFuncs *item)
{
	if(item->childName!=NULL){
		class_name_free(item->childName);
	}
	if(item->belongChildClass!=NULL){
		class_name_free(item->belongChildClass);
	}
	if(item->childMangleName!=NULL){
		n_free(item->childMangleName);
	}
	if(item->parentName!=NULL){
		class_name_free(item->parentName);
	}
	if(item->belongParentClass!=NULL){
		class_name_free(item->belongParentClass);
	}
	if(item->parentOrgiName!=NULL){
		n_free(item->parentOrgiName);
	}
	if(item->parentMangleName!=NULL){
		n_free(item->parentMangleName);
	}
	n_slice_free(OverrideFuncs,item);
}

static void createSourceCode(OverrideFuncs *item,NPtrArray *codes,char *varName)
{
	NString *str=n_string_new("");
	nboolean parentIsFace=class_mgr_is_interface(class_mgr_get(),item->parentName);
	if(parentIsFace){
		 char ifaceVar[255];
	     aet_utils_create_in_class_iface_var(item->parentName->userName,ifaceVar);//一定与classparser中创建的接口变量名相同
	     //把self转成parentClassName 形式:(parentClassName *)self,再转接口
	     //(&((parentClassName *)self)->ifaceVar)->parentMangleName=((childName *)self)->childMangleName;
	 	 nboolean childIsFace=class_mgr_is_interface(class_mgr_get(),item->childName);
	 	 if(!childIsFace){
	 	    printf("class_init createSourceCode 00 父类%s是接口，所在的类是:%s,子类是类:%s\n",
	 	    		item->parentName->sysName,item->belongParentClass->sysName,item->childName->sysName);
	        n_string_append_printf(str,"(&((%s *)%s)->%s)->%s=",item->belongParentClass->sysName,varName,ifaceVar,item->parentMangleName);
	        n_string_append_printf(str,"((%s *)%s)->%s;\n",item->childName->sysName,varName,item->childMangleName);
	 	 }else{
	 		char ifaceChildVar[255];
	 	    aet_utils_create_in_class_iface_var(item->childName->userName,ifaceChildVar);//一定与classparser中创建的接口变量名相同
		 	printf("class_init createSourceCode 11 父类%s是接口,belong:%s 子类也是接口:%s belong:%s\n",
		 			item->parentName->sysName,item->belongParentClass->sysName,item->childName->sysName,item->belongChildClass->sysName);
	 	    n_string_append_printf(str,"(&((%s *)%s)->%s)->%s=",item->belongParentClass->sysName,varName,ifaceVar,item->parentMangleName);
	 	    n_string_append_printf(str,"(&((%s *)%s)->%s)->%s;\n",item->belongChildClass->sysName,varName,ifaceChildVar,item->childMangleName);
	 	 }

	}else{
		 //把self转成parentClassName 形式:(parentClassName *)self,再赋值
		//((parentClassName *)self)->ifaceVar)->parentMangleName==((childName *)self)->childMangleName;
	 	 nboolean childIsFace=class_mgr_is_interface(class_mgr_get(),item->childName);
	 	 if(!childIsFace){
		 	printf("class_init createSourceCode 22 父类是类%s,子类是类:%s\n",item->parentName->sysName,item->childName->sysName);
	        n_string_append_printf(str,"((%s *)%s)->%s=",item->parentName->sysName,varName,item->parentMangleName);
		    n_string_append_printf(str,"((%s *)%s)->%s;\n",item->childName->sysName,varName,item->childMangleName);
	 	 }else{
	 		char ifaceChildVar[255];
	 		aet_utils_create_in_class_iface_var(item->childName->userName,ifaceChildVar);//一定与classparser中创建的接口变量名相同
		 	printf("class_init createSourceCode 33 父类是类%s,子类是接口:%s belong:%s\n",
		 			item->parentName->sysName,item->childName->sysName,item->belongChildClass->sysName);
	        n_string_append_printf(str,"((%s *)%s)->%s=",item->parentName->sysName,varName,item->parentMangleName);
	 	    n_string_append_printf(str,"(&((%s *)%s)->%s)->%s;\n",item->belongChildClass->sysName,varName,ifaceChildVar,item->childMangleName);
	 	 }
	}
	n_ptr_array_add(codes,n_string_free(str,FALSE));
}


static void addResult(NPtrArray *array,int error,ClassName *childName,ClassName *belongChildClass,
		ClassName *parentName,ClassName *belongParentClass,ClassFunc *parentItem,tree childFieldDecl,char *childMangleName)
{
	OverrideFuncs *data=(OverrideFuncs *)n_slice_new0(OverrideFuncs);
	data->childName=class_name_clone(childName);
	data->belongChildClass=class_name_clone(belongChildClass);
	if(childMangleName)
		data->childMangleName=n_strdup(childMangleName);
	data->parentName=class_name_clone(parentName);
	data->belongParentClass=class_name_clone(belongParentClass);
	data->parentOrgiName=n_strdup(parentItem->orgiName);
	data->parentMangleName=n_strdup(parentItem->mangleFunName);
	data->error=error;
	data->childFieldDecl=childFieldDecl;
	n_ptr_array_add(array,data);
}


static tree findFieldFromChildFuns(NPtrArray *childFuncs,ClassName *childName,ClassFunc *parentItem,int *error,char **childMangleName)
{
    int i=0;
    for(i=0;i<childFuncs->len;i++){
    	ClassFunc *childItem=(ClassFunc *)n_ptr_array_index(childFuncs,i);
	   if(strcmp(childItem->orgiName,parentItem->orgiName)==0 && isValidDeclOrDefine(childItem->fieldDecl)
	                && !childItem->isAbstract && !childItem->isCtor && !parentItem->isCtor){
		  location_t loc;
		  nboolean equal=compareMangle(childItem->mangleFunName,parentItem->mangleFunName);
		  n_debug("class_init 类的func定义与接口的函数名是相同的。 index:%d %s %s 参数是否相同:%d\n",
				  i,parentItem->mangleFunName,childItem->mangleFunName,equal);
		  if(equal){
			 nboolean returnValue=compareFunctionType(childItem->fieldDecl,parentItem->fieldDecl);
			 if(!returnValue){
				 *error= -1;
			 }else{
				 //成功一个了
				 *error=0;
			 }
			 *childMangleName=n_strdup(childItem->mangleFunName);
			 return childItem->fieldDecl;
		  }
	   }
    }
   *error=-2;
	return NULL_TREE;
}


/**
 * 如果childName是接口，belongChildClass就是childName的实现类
 * 当childName不是接口时，belongChildClass与childName相同
 * parentClass,belongParentClass同上解释
 */
static void fillReplaceParentFuncInfo(ClassInit *self,NPtrArray *array,ClassName *childName,ClassName *belongChildClass,
		ClassName *parentClass,ClassName *belongParentClass)
{
   ClassInfo *parentInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),parentClass);
   NPtrArray *parentFuncs=func_mgr_get_funcs(func_mgr_get(),parentClass);
   if(parentFuncs==NULL || parentFuncs->len==0)
      return;
   NPtrArray *childFuncs=func_mgr_get_funcs(func_mgr_get(),childName);
   int i=0;
   for(i=0;i<parentFuncs->len;i++){
      ClassFunc *parentItem=(ClassFunc *)n_ptr_array_index(parentFuncs,i);
      if(isValidDeclOrDefine(parentItem->fieldDecl)){
         n_debug("class_init fillReplaceParentFuncInfo index:%d orgiFun:%s mangleFun:%s child:%s parent:%s\n",
         i,parentItem->orgiName,parentItem->mangleFunName,childName->sysName,parentClass->sysName);
         int error=0;
         char *childMangleName=NULL;
         tree childFieldDecl=findFieldFromChildFuns(childFuncs,childName,parentItem,&error,&childMangleName);
         addResult(array,error,childName,belongChildClass,parentClass,belongParentClass,parentItem,childFieldDecl,childMangleName);
      }
   }
}


static void twoStepRelace(ClassInit *self,ClassName *childName,ClassName *parent,NPtrArray *codes,char *varName)
{
   if(!strcmp(childName->sysName,parent->sysName)){
	   n_debug("class_init twoStepRelace %s类不能替换同类%s的方法\n",childName->sysName,parent->sysName);
	   return ;
   }
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),childName);
   if(info==NULL){
	   return ;
   }
   ClassInfo *parentInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),parent);
   if(parentInfo==NULL){
	   return ;
   }
   NPtrArray *array=n_ptr_array_new();
   //有4种情况
   //1.用子类的field方法替换父类的方法
   //2.用子类实现的接口的field方法替换父类的方法
   //3.用子类的field方法替换父类的接口的方法
   //4.用子类实现的接口的field方法替换父类的接口方法
   n_debug("class_init replace 00 index:%d  用子类%s,替换父类%s的方法\n",childName->sysName,parent->sysName);
	fillReplaceParentFuncInfo(self,array,childName,childName,parent,parent);
   int i;
   for(i=0;i<info->ifaceCount;i++){
      ClassName iface=info->ifaces[i];
      n_debug("class_init replace 11 index:%d  用子类%s的接口%s,替换父类%s的方法\n",i,childName->sysName,iface.sysName,parent->sysName);
	  fillReplaceParentFuncInfo(self,array,&iface,childName,parent,parent);
   }
   //用子类和子类实现的接口的field方法替换父类的接口方法
   for(i=0;i<parentInfo->ifaceCount;i++){
	   ClassName iface=parentInfo->ifaces[i];
	   n_debug("class_init replace 22 index:%d 用子类%s,替换父类%s的接口%s的方法\n",i,childName->sysName,parent->sysName,iface.sysName);
	  fillReplaceParentFuncInfo(self,array,childName,childName,&iface,parent);
   }
   for(i=0;i<parentInfo->ifaceCount;i++){
	  ClassName iface=parentInfo->ifaces[i];
      int j;
      for(j=0;j<info->ifaceCount;j++){
          n_debug("class_init replace 33 i:%d j:%d 用子类%s的接口%s,替换父类%s的接口%s的方法\n",
    			  i,j,childName->sysName,info->ifaces[j].sysName,parent->sysName,iface.sysName);
    	  fillReplaceParentFuncInfo(self,array,&info->ifaces[j],childName,&iface,parent);
      }
   }
   for(i=0;i<array->len;i++){
	   OverrideFuncs *item=(OverrideFuncs *)n_ptr_array_index(array,i);
	   if(item->error==-1){
		  location_t ploc = DECL_SOURCE_LOCATION (item->childFieldDecl);
		  error_at(ploc,"bb--父类%qs声明的方法%qs与子类%qs实现的方法相同，但返回值不同。",item->parentName->sysName,item->parentOrgiName,item->childName->sysName);
	   }else if(item->error==-2){
	   }else{
		   createSourceCode(item,codes,varName);
	   }
   }
  	n_ptr_array_set_free_func(array,freeOverrideFuncData_cb);
  	n_ptr_array_unref(array);
}


/**
 * array 里是类名
 * 存放顺序如:
 * 0 AObject 1 Second 2 Abc,从根类到子类
 */
static void oneStep(ClassInit *self,NPtrArray *array,ClassName *childClass,NPtrArray *codes,char *varName)
{
   int i;
   for(i=0;i<array->len;i++){
      ClassName *parent=n_ptr_array_index(array,array->len-i-1);
      n_debug("class_init oneStep index:%d %s类替换%s类的方法\n",i,childClass->sysName,parent->sysName);
      twoStepRelace(self,childClass,parent,codes,varName);
   }
}

static void traversing(ClassName *className,NPtrArray *data)
{
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   if(info!=NULL){
      n_ptr_array_insert(data,0,className);
      traversing(&info->parentName,data);
   }
}

/**
 * 用子类的方法覆盖父类的方法。
 * Class A{
 *  go();
 * }
 * Class B Extends A{
 *   go();
 * }
 * void test(A){
 *   A->go()
 * }
 * void main(){
 *   b =new$ B
 *   test(b)
 * }
 * A->go就会调到B的go方法
 * 因为A的go函数指针在创建B对象时被指到b的go上了
 *
 */
char *class_init_override_parent_ref(ClassInit *self,ClassName *childName,char *varName)
{
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),childName);
   n_debug("class_init_override_parent_ref---- %s %p\n",childName->sysName,info);
   if(info==NULL){
	   return NULL;
   }
   NPtrArray *codes=n_ptr_array_new();
   NPtrArray *classArray=n_ptr_array_new();
   traversing(childName,classArray);
   int i;
   for(i=0;i<classArray->len;i++){
	   ClassName *topClass=n_ptr_array_index(classArray,i);
	   n_debug("class_init 00 开始覆盖父类的方法 index:%d 最低层的子类:%s i层的类:%s\n",i,childName->sysName,topClass->sysName);
	   NPtrArray *topArray=n_ptr_array_new();
	   traversing(topClass,topArray);
	   oneStep(self,topArray,topClass,codes,varName);
	   n_ptr_array_unref(topArray);
   }
 	n_ptr_array_unref(classArray);
 	n_debug("class_init 有代码吗 %d\n",codes->len);
 	if(codes->len==0){
 	    n_ptr_array_unref(codes);
 		return NULL;
 	}
   NString *source=n_string_new("");
   for(i=0;i<codes->len;i++){
      char *code=n_ptr_array_index(codes,i);
      n_debug("class_init 11 原代码是 index:%d childName:%s source:%s\n",i,childName->sysName,code);
      n_string_append(source,n_strdup(code));
   }
   char *result=  n_string_free(source,FALSE);
   n_ptr_array_set_free_func(codes,n_free);
   n_ptr_array_unref(codes);
   return result;
}

/**
 * 加入初始化函数声明 Abc *Abc_AET_INIT_GLOBAL_METHOD_STRING_Abc(Abc *self);
 */
void  class_init_create_init_decl(ClassInit *self,location_t loc,ClassName *className)
{
   char *funName=aet_utils_create_init_method(className->sysName);
   tree id = aet_utils_create_ident (funName);
   tree decl=lookup_name(id);
   if(decl){
      printf("class_init_create_init_decl 00 已经声明了函数:%s\n",funName);
      free(funName);
      return;
   }

   //tree objId = aet_utils_create_ident (className->sysName);
   tree objectDecl=lookup_name(aet_utils_create_ident(className->sysName));
   tree param_type_list = NULL;
   //第一个参数 类型是 AObject *
   tree parm1=build_pointer_type(TREE_TYPE(objectDecl));
   param_type_list = tree_cons (NULL_TREE, parm1, param_type_list);
    //结束参数
   param_type_list = tree_cons (NULL_TREE, void_type_node, param_type_list);//在函数声明的最后一个参数必须是void_type_node

   param_type_list = nreverse (param_type_list);
   tree rtntype=build_pointer_type(void_type_node);
   tree fntype = build_function_type (rtntype, param_type_list);
   tree fndecl = build_decl (loc, FUNCTION_DECL, id, fntype);
   TREE_STATIC (fndecl) = 0;
   DECL_ARTIFICIAL (fndecl) = 1;
   TREE_PUBLIC (fndecl) = 1;
   //DECL_EXTERNAL (fndecl) = 1;
   //pushdecl (fndecl); 不能调用 pushdecl否则出undefined reference to `_TSecond__superFuncAddressArray'
   c_c_decl_bind_file_scope(fndecl);//放在file_scope，c_c_decl_bind_file_scope是增加的,原本没有
   finish_decl (fndecl, loc, NULL_TREE,NULL_TREE, NULL_TREE);
   free(funName);
}

/**
 * 给AObject的 free_child 函数指针赋值
 * ((AObject*)self)->free_child=((Abc*)self)->Abc_unref_692658582
 */
char      *class_init_modify_root_object_free_child(ClassInit *self,ClassName *className,char *varName)
{
	ClassName *rootClassName=class_mgr_get_class_name_by_user(class_mgr_get(),AET_ROOT_OBJECT);
	if(rootClassName==NULL)
		return NULL;
	char *freeChildMangle=func_mgr_get_mangle_func_name(func_mgr_get(),rootClassName,"free_child");
	if(freeChildMangle==NULL)
		return NULL;
 	char unrefName[255];
 	aet_utils_create_unref_name(className->userName,unrefName);
	char *unrefMangle=func_mgr_get_mangle_func_name(func_mgr_get(),className,unrefName);
	if(unrefMangle==NULL)
		return NULL;

 	NString *source=n_string_new("");
 	n_string_append_printf(source,"((%s *)%s)->%s=%s->%s;\n",
 	      rootClassName->sysName,varName,freeChildMangle,varName,unrefMangle);
 	return n_string_free(source,FALSE);
}

static void fillFreeChildMethodForAObject(ClassName *className,NString *codes)
{
	ClassName *rootClassName=class_mgr_get_class_name_by_user(class_mgr_get(),AET_ROOT_OBJECT);
	if(rootClassName==NULL)
		return ;
	char *freeChildMangle=func_mgr_get_mangle_func_name(func_mgr_get(),rootClassName,"free_child");
	if(freeChildMangle==NULL)
		return ;
 	char unrefName[255];
 	aet_utils_create_unref_name(className->userName,unrefName);
	char *unrefMangle=func_mgr_get_mangle_func_name(func_mgr_get(),className,unrefName);
	if(unrefMangle==NULL)
		return ;
 	n_string_append_printf(codes,"\t((%s *)self)->%s=%s;\n",rootClassName->sysName,freeChildMangle,unrefMangle);
}

static void fillGetClassMethodForAObject(ClassName *className,char *funcName,NString *codes)
{
	ClassName *rootClassName=class_mgr_get_class_name_by_user(class_mgr_get(),AET_ROOT_OBJECT);
	if(rootClassName==NULL)
		return ;
	char *getClassMangle=func_mgr_get_mangle_func_name(func_mgr_get(),rootClassName,"getClass");
	if(getClassMangle==NULL)
		return;
 	n_string_append_printf(codes,"\t((%s *)self)->%s=%s;\n",rootClassName->sysName,getClassMangle,funcName);
}

/**
 * 接口类的初始化方法，内容如下:
 * void * debug_RandomGenerator_init_1234ergR5678_debug_RandomGenerator(debug_RandomGenerator *self)
 * {
 *    return (void *)_createAClass_debug_RandomGenerator_123((AObject *)self);
 * }
 */
void class_init_create_init_define_for_interface(ClassInit *self,char *sysName,NString *buf)
{
     c_parser *parser=self->parser->parser;
	 ClassInfo *info= class_mgr_get_class_info(class_mgr_get(),sysName);
	 if(info==NULL){
	    c_parser_error (parser, "没有classInfo");
        return ;
	 }
	 ClassName *className=class_mgr_clone_class_name(class_mgr_get(),sysName);
	 char *initMethod=aet_utils_create_init_method(sysName);
	 n_string_append_printf(buf,"void * %s(%s *self)\n{\n",initMethod,sysName);
	 n_free(initMethod);
	 char getAClassFuncName[255];
	 class_build_create_func_name(className,getAClassFuncName);
	 n_string_append_printf(buf,"\treturn (void *)%s((AObject *)self);\n",getAClassFuncName);
	 n_string_append(buf,"}\n");
}

////////////////////给父类的方法赋值 原来通过new对时也赋值，现在改为全部在子类impl$后的初始化方法中赋值-----------------------------------
////////////////////new 对象时调用的是class_init_override_parent_ref现在不再调用
static tree getRtn(tree funcOrField,nboolean isField)
{
	 tree rtn=NULL_TREE;
	 rtn=TREE_TYPE(funcOrField);
	 rtn=TREE_TYPE(rtn);
	 return isField?TREE_TYPE(rtn):rtn;
}


////////////////////////-------xxxxxxxxxxxxxxxxxxxx----------------------
//如果当前类有声明，并且是私有的，不能覆盖父类。
//进入这里父类或祖先都是有声明的
static char * getCanAssignFunc(NPtrArray *childFuncsArray,ClassFunc *parentItem,char *parentSysName,char *childSysName)
{
   int i=0;
   for(i=0;i<childFuncsArray->len;i++){
      ClassFunc *childItem=(ClassFunc *)n_ptr_array_index(childFuncsArray,i);
     // printf("class_init.c getCanAssignFunc :%d %s %s parentItem:%s childItem:%s\n",
          //  i,parentSysName,childSysName,parentItem->rawMangleName,childItem->rawMangleName);
      if(strcmp(childItem->rawMangleName,parentItem->rawMangleName)==0
            && aet_utils_valid_tree(childItem->fromImplDefine)
            && !childItem->isAbstract && !childItem->isCtor && !parentItem->isCtor){

         tree rtn=getRtn(childItem->fromImplDefine,FALSE);
         tree parentRtn=getRtn(parentItem->fieldDecl,TRUE);
         nboolean returnValue=c_tree_equal (rtn,parentRtn);
         n_debug("classinit.c getCanAssignFunc 类的func定义与接口的函数名是相同的。i:%d %s %s parent:%s child:%s returnValue:%d 父类方法:%p\n",
               i,parentItem->mangleFunName,childItem->mangleFunName,parentSysName,childSysName,returnValue,parentItem->fieldDecl);
         if(!returnValue){
            char    *rtnChild=class_util_get_class_name(rtn);
            char    *rtnParent=class_util_get_class_name(parentRtn);
            if(rtnChild && rtnParent){
               nboolean isClassParent=  class_mgr_is_parent(class_mgr_get(),childSysName,parentSysName);
               nboolean isClassAncestors=  class_mgr_is_ancestors(class_mgr_get(),childSysName,parentSysName);
               nboolean isParent=  class_mgr_is_parent(class_mgr_get(),rtnChild,rtnParent);
               nboolean isAncestors=  class_mgr_is_ancestors(class_mgr_get(),rtnChild,rtnParent);
               //语言规范 14
               if((isClassParent && isParent) || (isClassAncestors && (isParent || isAncestors)))
                  return childItem->mangleFunName;
            }
            location_t ploc = DECL_SOURCE_LOCATION (childItem->fromImplDefine);
            error_at(ploc,"子类%qs的%qs方法与父类%qs的返回类型不匹配。",childSysName,childItem->orgiName,parentSysName);
            return NULL;
         }
         //如果有声明，并且是私有的，不覆盖父类。
         if(childItem->fieldDecl && class_func_is_private(childItem))
            return NULL;
         return childItem->mangleFunName;
      }
   }
   return NULL;
}

/**
 * 为父和他实现的接口的域方法赋值
 * 比2022-02-25之前的方法要简单很多了。原来的复杂。
 * Assign values to parent object
 */
static void assignToParent(ClassInit *self,ClassName *childName,ClassName *parent,NString *codes)
{
   if(parent==NULL)
      return ;
   NPtrArray    *childFuncsArray=func_mgr_get_funcs(func_mgr_get(),childName);
   ClassInfo    *parentInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),parent);
   NPtrArray    *parentFuncsArray=func_mgr_get_funcs(func_mgr_get(),parent);
   /**给父类的域声明赋值*/
   int i;
   for(i=0;i<parentFuncsArray->len;i++){
      ClassFunc *func=n_ptr_array_index(parentFuncsArray,i);
      if(aet_utils_valid_tree(func->fieldDecl) && !class_func_is_private(func)){
         char *implDefine=getCanAssignFunc(childFuncsArray,func,parent->sysName,childName->sysName);
         if(implDefine!=NULL){
            //n_string_append_printf(codes,"\t((%s *)self)->%s=%s;\n",parent->sysName,func->mangleFunName,implDefine);
            if(!class_func_is_mtcs(func))
               n_string_append_printf(codes,"\t((%s *)self)->%s=%s;\n",parent->sysName,func->mangleFunName,implDefine);
            else if(class_func_is_kernel(func))
               n_string_append_printf(codes,"\t((%s *)self)->%s=(void*)\"%s\";\n",parent->sysName,func->mangleFunName,implDefine);
            else if(class_func_is_device(func))
               n_string_append_printf(codes,"\t((%s *)self)->%s=%s;\n",parent->sysName,func->mangleFunName,implDefine);
         }else{
            //处理父类有分裂声明 函数名后缀带_device 如: _Z7TSecond14abcdf2f_deviceEPN7TSecondE
            //父方法是 host device 并且是分裂出来的一定不会找到实现。如果子类有实现并且名字与父的分裂源相同。就认为可以覆盖。
            if(class_func_is_host(func) && class_func_is_device(func) && class_func_is_divide(func)){
               ClassFunc *divideSrc=class_func_get_divide_src(func);
               if(!divideSrc){
                 n_error("找不到方法%s的源头。父类:%s 当前类:%s",func->orgiName,parent->sysName,childName->sysName);
               }
               printf("find ---- %s %s\n",childName->sysName,divideSrc->rawMangleName,func->rawMangleName);
               ClassFunc *childDivideFunc=func_mgr_get_func_by_raw_mangle(func_mgr_get(),childName,func->rawMangleName);
               /*父func是一个分裂函数，用它的rawMangleName在子类中找同名的函数，如果有，直接用子类的函数名赋值父类的函数地址，
               说明子类有如下声明
               Class A{
                       __host__ __device__ set();
                       ...
                };
                */
               if(childDivideFunc){
                  n_string_append_printf(codes,"\t((%s *)self)->%s=self->%s;\n",
                        parent->sysName,func->mangleFunName,childDivideFunc->mangleFunName);
               }else{
                 //在当前类找到与父类分裂函数rawMangleName相同的声明。divideSrc rawMangleName 是原始名字，没有加_device
                  ClassFunc *childDivideFunc=func_mgr_get_func_by_raw_mangle(func_mgr_get(),childName,divideSrc->rawMangleName);
                  if(childDivideFunc){
                     //说明在当前类，没有声明，只有实现。不是 mtcs
                     printf("说明在当前类，没有声明，只有实现。%s %p isMtcs:%d\n",
                           childName->sysName,childDivideFunc->fieldDecl,class_func_is_mtcs(childDivideFunc));
                     if(!class_func_is_device(childDivideFunc)){
                        n_error("出错了在 -----assignToParent 父类:%s 当前类:%s func:%s\n",
                              parent->sysName,childName->sysName,func->mangleFunName);
                     }
                     char devicePointerVarName[256];
                     mtcs_info_create_device_func_pointer_var_name(devicePointerVarName,childName->sysName);
                     int index= func_mgr_get_device_func_index(func_mgr_get(),childName,childDivideFunc->mangleFunName);
                     n_string_append_printf(codes,"\t%s(&((%s *)self)->%s,\"%s\",%d,self->%s);\n",
                           AET_MTCS_COPY_DEVICE_FUNC_ADDRESS_FUNC_NAME,
                           parent->sysName, func->mangleFunName,devicePointerVarName,index,AET_MTCS_PLATFORM_TYPE_VAR_NAME);

                  }

                  /*
                  Class A {
                     void __host__ __device__ set();
                  }
                  impl$ A{
                     void set(){

                     }
                  }
                  Class B extends$ A{
                  }
                  impl$ B{
                    void set(){
                    }
                  }
                  */
               }
            }
         }
      }
   }
   /**给父类的接口赋值*/
   for(i=0;i<parentInfo->ifaceCount;i++){
      ClassName *iface=&(parentInfo->ifaces[i]);
      NPtrArray    *ifaceFuncsArray=func_mgr_get_funcs(func_mgr_get(),iface);
      char ifaceVarName[255];
      aet_utils_create_in_class_iface_var(iface->userName,ifaceVarName);//一定与classparser中创建的接口变量名相同
      int j;
      for(j=0;j<ifaceFuncsArray->len;j++){
         ClassFunc *func=n_ptr_array_index(ifaceFuncsArray,j);
         if(aet_utils_valid_tree(func->fieldDecl)){
            char *implDefine=getCanAssignFunc(childFuncsArray,func,iface->sysName,childName->sysName);
            if(implDefine!=NULL){
               if(!class_func_is_mtcs(func))
                  n_string_append_printf(codes,"\t(&((%s *)self)->%s)->%s=%s;\n",
                        parent->sysName,ifaceVarName,func->mangleFunName,implDefine);
               else if(class_func_is_kernel(func))
                  n_string_append_printf(codes,"\t(&((%s *)self)->%s)->%s=(void*)\"%s\";\n",
                                         parent->sysName,ifaceVarName,func->mangleFunName,implDefine);
               else if(class_func_is_device(func))
                  n_string_append_printf(codes,"\t(&((%s *)self)->%s)->%s=%s;\n",
                                    parent->sysName,ifaceVarName,func->mangleFunName,implDefine);

            }else{

               if(class_func_is_host(func) && class_func_is_device(func) && class_func_is_divide(func)){
                    ClassFunc *divideSrc=class_func_get_divide_src(func);
                    if(!divideSrc){
                      n_error("找不到方法%s的源头。父类:%s 当前类:%s",func->orgiName,parent->sysName,childName->sysName);
                    }
                    printf("find ---- %s %s\n",childName->sysName,divideSrc->rawMangleName,func->rawMangleName);
                    ClassFunc *childDivideFunc=func_mgr_get_func_by_raw_mangle(func_mgr_get(),childName,func->rawMangleName);
                    if(childDivideFunc){
                        n_string_append_printf(codes,"\t(&((%s *)self)->%s)->%s=self->%s;\n",
                               parent->sysName,ifaceVarName,func->mangleFunName,childDivideFunc->mangleFunName);
                    }else{
                       //在当前类找到与父类分裂函数rawMangleName相同的声明。divideSrc rawMangleName 是原始名字，没有加_device
                       ClassFunc *childDivideFunc=func_mgr_get_func_by_raw_mangle(func_mgr_get(),childName,divideSrc->rawMangleName);
                       if(childDivideFunc){
                          //说明在当前类，没有声明，只有实现。不是 mtcs
                          printf("说明在当前类，没有声明，只有实现。%s %p isMtcs:%d\n",
                                childName->sysName,childDivideFunc->fieldDecl,class_func_is_mtcs(childDivideFunc));
                          if(!class_func_is_device(childDivideFunc)){
                             n_error("出错了在 -----assignToParent 父类:%s 当前类:%s func:%s\n",
                                   parent->sysName,childName->sysName,func->mangleFunName);
                          }
                          char devicePointerVarName[256];
                          mtcs_info_create_device_func_pointer_var_name(devicePointerVarName,childName->sysName);
                          int index= func_mgr_get_device_func_index(func_mgr_get(),childName,childDivideFunc->mangleFunName);
                          n_string_append_printf(codes,"\t%s((&((%s *)self)->%s)->%s,\"%s\",%d,self->%s);\n",
                                AET_MTCS_COPY_DEVICE_FUNC_ADDRESS_FUNC_NAME,  parent->sysName,ifaceVarName,
                            func->mangleFunName,devicePointerVarName,index,AET_MTCS_PLATFORM_TYPE_VAR_NAME);

                       }
                    }
               }
            }
         }
      }
   }
   if(parentInfo->parentName.sysName==NULL)
      return ;
   assignToParent(self,childName,&parentInfo->parentName,codes);
}

/**
 * 用当前类的方法给父类的方法赋值。
 */
static char *modifyParentMethod(ClassInit *self,ClassName *className)
{
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   if(info==NULL){
      return NULL;
   }
   if(info->parentName.sysName==NULL)
      return NULL;
   NString *codes=n_string_new("");
   assignToParent(self,className,&info->parentName,codes);
   if(codes->len==0){
      n_string_free(codes,TRUE);
      return NULL;
   }
   return  n_string_free(codes,FALSE);
}

/**
 * 给出一个函数名rawMangleName
 * 找出来自那个接口的ClassFunc
 */
static ClassFunc *getInterfaceFunc(ClassName *className,char *rawMangleName)
{
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   /*本类的接口方法个数*/
   int i;
   for(i=0;i<info->ifaceCount;i++){
      ClassName *iface=&(info->ifaces[i]);
      NPtrArray *funcArray=func_mgr_get_funcs(func_mgr_get(),iface);
      if(funcArray!=NULL){
         int j;
         for(j=0;j<funcArray->len;j++){
            ClassFunc *func=n_ptr_array_index(funcArray,j);
            if(!strcmp(func->rawMangleName,rawMangleName))
                  return func;
         }
      }
   }
   return NULL;
}


static char *FREE_CHILD_NAME="free_child";
static char *GET_CLASS="getClass";
/**
 * 不要检查保留的方法
 * 原型 reserveField funcheck.c
 */
static nboolean reserveField(ClassFunc *func)
{
    if(!strcmp(func->orgiName,FREE_CHILD_NAME))
      return TRUE;
    if(!strcmp(func->orgiName,GET_CLASS))
      return TRUE;
    return FALSE;
}


static ClassFunc *findParentImplInterface(ClassInit *self,ClassInfo *info,ClassName *className,ClassFunc *func,char **at)
{
   ClassFunc *interfaceMethod=getInterfaceFunc(className,func->rawMangleName);
   if(interfaceMethod==NULL){
      n_error("在类%s中不可能找不到接口方法:%s\n",className->sysName,func->rawMangleName);
   }
   char *atSysName=NULL;
   n_debug("find interface ---%s %s %s\n",className->sysName,func->rawMangleName,interfaceMethod->mangleFunName);

   //从父类找接口实现的方法或声明。
   ClassFunc *impl = func_mgr_get_interface_impl_from_parent(func_mgr_get(), &info->parentName, interfaceMethod,&atSysName);
   location_t  implLoc=class_mgr_get_impl_location(class_mgr_get(),className);

   //找父类是不是有该函数的实现,如果没有报错
   if(impl==NULL && !class_info_is_abstract_class(info)){
      error_at(implLoc,"类%qs 没有实现接口 %qs 的方法%qs !",className->userName,interfaceMethod->className->userName,func->orgiName);
   }else if(impl==NULL && class_info_is_abstract_class(info)){
      n_info("抽象类%s没有实现接口%s的方法%s。",className->userName,interfaceMethod->className->userName,interfaceMethod->orgiName);
   }else{
      if(class_func_is_mtcs(impl) && !class_func_is_mtcs(func)){
         error("类%qs声明的方法%qs就设备函数，但类%qs实现的接口函数不是设备函数。",atSysName,func->orgiName,className->sysName);
      }
      if(!class_func_is_mtcs(impl) && class_func_is_mtcs(func)){
         error_at(info->implLoc,"类%qs声明的方法%qs是主机函数，但类%qs实现的接口函数是设备函数。",atSysName,func->orgiName,className->sysName);
      }
      if(class_func_is_mtcs(impl) && class_func_is_mtcs(func)){
         if(class_func_get_mtcs_type(impl)!=class_func_get_mtcs_type(func))
           error("类%qs声明的方法%qs与类%qs实现的接口函数都是设备函数，但类型不一样。",atSysName,func->orgiName,className->sysName);
      }
      *at=atSysName;
      return impl;
   }
   return NULL;
}

static void createNormalMethodCodes(ClassInit *self,ClassName *className,ClassInfo *info,ClassFunc *item,NString *codes)
{
   FuncMgr *funcMgr=func_mgr_get();
   nboolean define = aet_utils_valid_tree(item->fromImplDefine);
   nboolean isAbstract=class_info_is_abstract_class(info);
   if(define){
      if(class_func_is_abstract(item)){
         error("抽象方法%s，不能在本类%s实现。",item->orgiName,className->sysName);
      }
      n_string_append_printf(codes,"\tself->%s=%s;\n",item->mangleFunName,item->mangleFunName);
   }else{
      if(class_func_is_abstract(item)){
         n_string_append_printf(codes,"\tself->%s=NULL;\n",item->mangleFunName);
      }else{
         if(item->fromInterface){
            //来自接口，但没有定义，到父组件中找
            ClassFunc *interfaceMethod=getInterfaceFunc(className,item->rawMangleName);
            if(interfaceMethod==NULL){
               n_error("在类%s中不可能找不到接口方法:%s\n",className->sysName,item->orgiName);
            }
            char *atSystem=NULL;
            ClassFunc *parentFunc=findParentImplInterface(self,info,className,item,&atSystem);
            if(parentFunc){
               if(parentFunc->fieldDecl && parentFunc->fromImplDefine) //说明父类与本类在同一个c文件中定义
                  n_string_append_printf(codes,"\tself->%s=%s;\n",item->mangleFunName,parentFunc->mangleFunName);
               else if(parentFunc->fieldDecl && !parentFunc->fromImplDefine)
                  n_string_append_printf(codes,"\tself->%s=((%s*)self)->%s;\n",item->mangleFunName,atSystem, parentFunc->mangleFunName);
               else if(!parentFunc->fieldDecl && parentFunc->fromImplDefine)
                  n_string_append_printf(codes,"\tself->%s=%s;\n",item->mangleFunName,parentFunc->mangleFunName);
            }else{
               if(!class_info_is_abstract_class(info)) //不是抽象类，必须实现接口
                  error_at(info->implLoc,"类%s没有实现接口%s中的方法%s。\n",
                        className->sysName,interfaceMethod->className->sysName,interfaceMethod->orgiName);
            }
         }else{
            if(!reserveField(item))
               error("类%s中的方法%s，没有实现。",className->sysName,item->orgiName);
         }
      }
   }
}

static void createKernelMethodCodes(ClassInit *self,ClassName *className,ClassInfo *info,ClassFunc *item,NString *codes)
{
   FuncMgr *funcMgr=func_mgr_get();
   nboolean define = aet_utils_valid_tree(item->fromImplDefine);
   nboolean isAbstract=class_info_is_abstract_class(info);
   if(define){
      if(class_func_is_abstract(item)){
         error("抽象方法%s，不能在本类%s实现。",item->orgiName,className->sysName);
      }
      n_string_append_printf(codes,"\tself->%s=(void*)\"%s\";\n",item->mangleFunName,item->mangleFunName);
   }else{
      if(class_func_is_abstract(item)){
         n_string_append_printf(codes,"\tself->%s=NULL;\n",item->mangleFunName);
      }else{
         if(item->fromInterface){
            //来自接口，但没有定义，到父组件中找
            ClassFunc *interfaceMethod=getInterfaceFunc(className,item->rawMangleName);
            if(interfaceMethod==NULL){
               n_error("在类%s中不可能找不到接口方法:%s\n",className->sysName,item->orgiName);
            }
            char *atSystem=NULL;
            ClassFunc *parentFunc=findParentImplInterface(self,info,className,item,&atSystem);
            if(parentFunc){
               n_string_append_printf(codes,"\tself->%s=((%s*)self)->%s;\n",item->mangleFunName,atSystem, parentFunc->mangleFunName);
            }else{
               error("类%s没有实现接口%s中的方法%s\n",className->sysName,interfaceMethod->className->sysName,interfaceMethod->orgiName);
            }
         }else{
            error("类%s中的方法%s，没有实现。",className->sysName,item->orgiName);
         }
      }
   }
}

/**
 * 给类中的设备函数赋值函数地址值，该值是从设备地址变为主机地址
 * 在cuda平台调用的是这个函数 cuMemcpyDtoH
 * 生成的源代码像这样:  mtcs_copy_device_func_address((void*)&self->_Z6TFirst7getdataEPN6TFirstEf,
 *                                         "_TFirst_deviceFuncPointers",0,self->mtcsPlatformType);
 * _Z6TFirst7getdataEPN6TFirstEf 是类中的设备函数指针。
 * _TFirst_deviceFuncPointers 是类中所有设备函数在某个平台的函数地址数组变量名。它保存所有设备函数地址。
 * 在cuda平台它定义的源代码像这样 .global .align 8 .u64 _TFirst_deviceFuncPointers[1] = { _Z6TFirst7getdataEPN6TFirstEf };
 * 0 是 _Z6TFirst7getdataEPN6TFirstEf在 _TFirst_deviceFuncPointers的索引值。
 */
static void createDeviceMethodCodes(ClassInit *self,char *devicePointerVarName,
      ClassName *className,ClassInfo *info,ClassFunc *item,NString *codes)
{
   FuncMgr *funcMgr=func_mgr_get();
   nboolean define = aet_utils_valid_tree(item->fromImplDefine);
   nboolean isAbstract=class_info_is_abstract_class(info);
   n_debug("createDeviceMethodCodes 00 define:%d isAbstract:%d %s %s\n",define,isAbstract,className->sysName,devicePointerVarName);
   if(define){
      if(class_func_is_abstract(item)){
         error("抽象方法%s，不能在本类%s实现。",item->orgiName,className->sysName);
      }
      int index=func_mgr_get_device_func_index(funcMgr,className,item->mangleFunName);
      if(index<0)
           n_error("在类%s中，设备函数%s没有实现。未知错误！",className->sysName,item->mangleFunName);
      n_string_append_printf(codes,"\t%s((void*)&self->%s,\"%s\",%d,self->%s);\n",AET_MTCS_COPY_DEVICE_FUNC_ADDRESS_FUNC_NAME,
              item->mangleFunName,devicePointerVarName,index,AET_MTCS_PLATFORM_TYPE_VAR_NAME);
   }else{
      if(class_func_is_abstract(item)){
         n_string_append_printf(codes,"\tself->%s=NULL;\n",item->mangleFunName);
      }else{
         if(item->fromInterface){
            //来自接口，但没有定义，到父组件中找
            ClassFunc *interfaceMethod=getInterfaceFunc(className,item->rawMangleName);
            if(interfaceMethod==NULL){
               n_error("在类%s中不可能找不到接口方法:%s\n",className->sysName,item->orgiName);
            }
            char *atSystem=NULL;
            ClassFunc *parentFunc=findParentImplInterface(self,info,className,item,&atSystem);
            if(parentFunc){
               n_string_append_printf(codes,"\tself->%s=((%s*)self)->%s;\n",item->mangleFunName,atSystem, parentFunc->mangleFunName);
            }else{
               error("类%s没有实现接口%s中的方法%s\n",className->sysName,interfaceMethod->className->sysName,interfaceMethod->orgiName);
            }
         }else{
            error("类%s中的方法%s，没有实现。",className->sysName,item->orgiName);
         }
      }
   }
}

static void createDivideDeviceMethodCodes(ClassInit *self,char *devicePointerVarName,
      ClassName *className,ClassInfo *info,ClassFunc *item,NString *codes)
{
   FuncMgr *funcMgr=func_mgr_get();
   nboolean define = aet_utils_valid_tree(item->fromImplDefine);
   nboolean isAbstract=class_info_is_abstract_class(info);
   ClassFunc *divideSrc=class_func_get_divide_src(item);
   if(!divideSrc){
      n_error("找不到分裂方法%s的源头。",item->orgiName);
   }
   int index=func_mgr_get_device_func_index(funcMgr,className,divideSrc->mangleFunName);
   printf("createDivideDeviceMethodCodes 00 %d\n",index);
   if(index>=0){
      if(class_func_is_abstract(item)){
         error("抽象方法%s，不能在本类%s实现。",item->orgiName,className->sysName);
      }
      n_string_append_printf(codes,"\t%s(&self->%s,\"%s\",%d,self->%s);\n",AET_MTCS_COPY_DEVICE_FUNC_ADDRESS_FUNC_NAME,
            item->mangleFunName,devicePointerVarName,index,AET_MTCS_PLATFORM_TYPE_VAR_NAME);
      return;
   }

   if(class_func_is_abstract(item)){
      n_string_append_printf(codes,"\tself->%s=NULL;\n",item->mangleFunName);
   }else{
      if(item->fromInterface){
         //来自接口，但没有定义，到父组件中找
         ClassFunc *interfaceMethod=getInterfaceFunc(className,item->rawMangleName);
         if(interfaceMethod==NULL){
            n_error("在类%s中不可能找不到接口方法:%s\n",className->sysName,item->orgiName);
         }
         char *atSystem=NULL;
         ClassFunc *parentFunc=findParentImplInterface(self,info,className,item,&atSystem);
         if(parentFunc){
            n_string_append_printf(codes,"\tself->%s=((%s*)self)->%s;\n",item->mangleFunName,atSystem, parentFunc->mangleFunName);
         }else{
            error("类%s没有实现接口%s中的方法%s\n",className->sysName,interfaceMethod->className->sysName,interfaceMethod->orgiName);
         }
      }else{
         error("类%s中的方法%s，没有实现。",className->sysName,item->orgiName);
      }
   }

}
/**
 * 生成给类中声明的函数赋值的代码。
 * self->_Z3Abc7getName=_Z3Abc7getName;
 */
static char *createFieldModifyCodes(ClassInit *self,ClassName *className)
{
   FuncMgr *funcMgr=func_mgr_get();
   NPtrArray *array=func_mgr_get_funcs(funcMgr,className);
   if(array==NULL)
      return NULL;
   char devicePointerVarName[256];
   mtcs_info_create_device_func_pointer_var_name(devicePointerVarName,className->sysName);
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   NString *codes=n_string_new("");
   int i;
   for(i=0;i<array->len;i++){
      ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
      if(aet_utils_valid_tree(item->fieldDecl)){
         if(!class_func_is_mtcs(item))
            createNormalMethodCodes(self,className,info,item,codes);
         else if(class_func_is_kernel(item))
             createKernelMethodCodes(self,className,info,item,codes);
         else if(class_func_is_device(item) && !class_func_is_host(item))
            createDeviceMethodCodes(self,devicePointerVarName,className,info,item,codes);
         else if(class_func_is_device(item) && class_func_is_host(item)){
            if(!class_func_is_divide(item)){
               createNormalMethodCodes(self,className,info,item,codes);
            }else{
               createDivideDeviceMethodCodes(self,devicePointerVarName,className,info,item,codes);
            }
         }else
            n_error("未知类型%s。",item->orgiName);
      }
   }
   return n_string_free(codes,FALSE);
}

/**
 * 初始化方法定义
 * void * debug_ASecond_init_1234ergR5678_debug_ASecond(debug_ASecond *self)
{
   if(self==NULL){
       return (void *)_createAClass_debug_ASecond_123((AObject *)self);
   }
   debug_ARandom_init_1234ergR5678_debug_ARandom((debug_ARandom *)self);
self->_Z7ASecond7ASecondEPN7ASecondE=_Z7ASecond7ASecondEPN7ASecondE;
self->_Z7ASecond26ASecond_finalize_485302655EPN7ASecondE=_Z7ASecond26ASecond_finalize_485302655EPN7ASecondE;
self->_Z7ASecond24ASecond_unref_1856585347EPN7ASecondE=_Z7ASecond24ASecond_unref_1856585347EPN7ASecondE;
((debug_AObject *)self)->_Z7AObject10free_childEPN7AObjectE=_Z7ASecond24ASecond_unref_1856585347EPN7ASecondE;
((debug_AObject *)self)->_Z7AObject8getClassEPN7AObjectE=_createAClass_debug_ASecond_123;
((debug_ARandom *)self)->_Z7ARandom6setvxdEPN7ARandomEf=_Z7ASecond6setvxdEPN7ASecondEf;
   return (void*)self;
}
 */
/***
 * 创建初始化函数定义的代码
 */
char *class_init_create_init_func(ClassInit *self,ClassName *className)
{
   NString *buf=n_string_new("");
   c_parser *parser=self->parser->parser;
   ClassImpl *classImpl=class_impl_get();
   ClassInfo *info= class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   if(info==NULL){
      c_parser_error (parser, "没有classInfo");
      n_string_free(buf,TRUE);
      return NULL ;
   }
   int i;

   char *initMethod=aet_utils_create_init_method(className->sysName);
   n_string_append_printf(buf,"void * %s(%s *self)\n{\n",initMethod,className->sysName);
   n_free(initMethod);
   n_string_append(buf,"\tif(self==NULL){\n");//说明要获取class类信息
   char getAClassFuncName[255];
   class_build_create_func_name(className,getAClassFuncName);
   n_string_append_printf(buf,"\t\treturn (void *)%s((AObject *)self);\n",getAClassFuncName);
   n_string_append(buf,"\t}\n");

   if(info->parentName.sysName){
      char *parentInitMethod=aet_utils_create_init_method(info->parentName.sysName);
      n_string_append_printf(buf,"\t%s((%s *)self);\n",parentInitMethod,info->parentName.sysName);
      n_free(parentInitMethod);
   }

//   {
//      //测试代码
//      if(strstr(className->sysName,"TFirst")){
//         n_string_append(buf,"printf(\"在TFirst中打印AObject构造函数 00 %p\\n\",((debug_AObject *)self)->_Z7AObject7AObjectEPN7AObjectE);\n");
//         n_string_append(buf,"printf(\"在TFirst中打印AObject构造函数 00aa %p %p\\n\",self->_Z6TFirst4runxEPN6TFirstE,_Z6TFirst4runxEPN6TFirstE);\n");
//         n_string_append(buf,"printf(\"在TFirst中打印AObject构造函数 00bb %p %p\\n\",self->_Z6TFirst6gohomeEPN6TFirstE,_Z6TFirst6gohomeEPN6TFirstE);\n");
//
//      }
//   }

   char *modify=createFieldModifyCodes(self,className);
   if(modify){
      n_string_append(buf,modify);
      n_free(modify);
   }

//   {
//      //测试代码
//      if(strstr(className->sysName,"TFirst")){
//
//         n_string_append(buf,"printf(\"在TFirst中打印AObject构造函数 11aa %p %p\\n\",self->_Z6TFirst4runxEPN6TFirstE,_Z6TFirst4runxEPN6TFirstE);\n");
//         n_string_append(buf,"printf(\"在TFirst中打印AObject构造函数 11bb %p %p\\n\",self->_Z6TFirst6gohomeEPN6TFirstE,_Z6TFirst6gohomeEPN6TFirstE);\n");
//
//         n_string_append(buf,"printf(\"在TFirst中打印AObject构造函数 11cc %p %p\\n\",self->_Z6TFirst6TFirstEPN6TFirstE,_Z6TFirst6TFirstEPN6TFirstE);\n");
//
//         n_string_append(buf,"printf(\"在TFirst中打印AObject构造函数 11 %p\\n\",((debug_AObject *)self)->_Z7AObject7AObjectEPN7AObjectE);\n");
//      }
//   }

   //接口
   char *ifaceCodes=class_interface_create_codes(classImpl->classInterface,className);
   //接口在这里加入
   if(ifaceCodes!=NULL){
      n_string_append(buf,ifaceCodes);
      n_free(ifaceCodes);
   }
//   {
//      //测试代码
//      if(strstr(className->sysName,"TFirst")){
//         n_string_append(buf,"printf(\"在TFirst中打印AObject构造函数 22 %p\\n\",((debug_AObject *)self)->_Z7AObject7AObjectEPN7AObjectE);\n");
//      }
//   }

   //生成  ((debug_AObject *)self)->_Z7AObject10free_childEPN7AObjectE=_Z6TFirst22TFirst_unref_290629480EPN6TFirstE;
   fillFreeChildMethodForAObject(className,buf);
   //生成  ((debug_AObject *)self)->_Z7AObject8getClassEPN7AObjectE=_createAClass_TFirst_123;
   fillGetClassMethodForAObject(className,getAClassFuncName,buf);
   //生成 覆盖父类方法的代码
   char *newCodes=modifyParentMethod(self,className);
   if(newCodes!=NULL){
      n_string_append(buf,newCodes);
      n_free(newCodes);
   }
//   {
//      //测试代码
//      if(strstr(className->sysName,"TFirst")){
//         n_string_append(buf,"printf(\"在TFirst中打印AObject构造函数 33 %p\\n\",((debug_AObject *)self)->_Z7AObject7AObjectEPN7AObjectE);\n");
//      }
//   }

   //调用第三版super内部函数
   char *innerSupperFuncName = aet_utils_create_init_inner_super_func_name(className->sysName);
   n_string_append_printf(buf,"\t%s();\n",innerSupperFuncName);
   free(innerSupperFuncName);

//   {
//      //测试代码
//      if(strstr(className->sysName,"TFirst")){
//         n_string_append(buf,"printf(\"在TFirst中打印AObject构造函数 44 %p\\n\",((debug_AObject *)self)->_Z7AObject7AObjectEPN7AObjectE);\n");
//      }
//   }

   optimizeGenericCall(self,className,buf);
   n_string_append(buf,"\treturn (void*)self;\n");
   n_string_append(buf,"}\n");
   return n_string_free(buf,FALSE);
}

/**
 * 优化带泛型块的函数的调用，主要是函数被复制一份，内部原泛型块的调用点（函数指针）
 * 被替换成了泛型块函数调用
 */
static void optimizeGenericCall(ClassInit *self,ClassName *className,NString *codes)
{
   //return;
   if(!className)
      return;
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   if(!class_info_is_generic_class(info))
      return;
   NPtrArray    *array = func_mgr_get_funcs(func_mgr_get(),className);
   if(!array || array->len==0)
      return;
   int modelCount = generic_model_get_count(info->genericModel);
   int i;
   int count = 0;
   for(i=0;i<array->len;i++){
      ClassFunc *func=n_ptr_array_index(array,i);
      if(!class_func_is_normal(func)
            || !class_func_have_generic_block(func)
            || !func->fromImplDefine || !func->fieldDecl)
         continue;
      //生成代码
      if(count == 0)
         n_string_append_printf(codes,"\tvoid *funcWithGbAdd = %s",AET_GENERIC_FUNC_WITH_GB_ADDRESS);
      else
         n_string_append_printf(codes,"\tfuncWithGbAdd = %s",AET_GENERIC_FUNC_WITH_GB_ADDRESS);

      n_string_append_printf(codes,"(self->_generic_1234_array,%d,\"%s\",\"%s\");\n",
            modelCount,className->sysName,func->mangleFunName);
      n_string_append(codes,"\tif(funcWithGbAdd)\n");
      n_string_append_printf(codes,"\t\tself->%s = funcWithGbAdd;\n",func->mangleFunName);
      count++;
   }
}

ClassInit *class_init_new()
{
	ClassInit *self = n_slice_alloc0 (sizeof(ClassInit));
	classInitInit(self);
	self->parser = aet_parser_get();
	return self;
}


