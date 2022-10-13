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
#include "system.h"
#include "coretypes.h"
#include "target.h"
#include "c-family/c-pragma.h"
#include "c/c-tree.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"

#include "aetutils.h"
#include "classmgr.h"
#include "classinit.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "classinit.h"
#include "aetprinttree.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "aet-c-parser-header.h"
#include "classbuild.h"
#include "classinterface.h"


/**
 * 重要的改变：2022-02-26之前
 * 给父类的域方法或父类实现的接口方法赋值都是在newstrategy.c的
 * addMiddleCodes方法调用class_init_override_parent_ref.
 * 但这会丢失在子类中定义的fromImplDefine函数(见ClassFunc.c)
 * 所以给父类赋值改成在子类的实现后的初始化方法中。就不会出现函数“丢失”
 * 的问题。
 */

static char *modifyParentMethodNew(ClassInit *self,ClassName *childName);

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
 * 比较childItem->fieldDecl,parentItem->fieldDecl
 */
static nboolean compareFunctionType(tree childField,tree parentField)
{
	//printf("compareFunctionType 00 %p %p\n",childField,parentField);
	//aet_print_tree(childField);
	//printf("compareFunctionType 11\n");
	//aet_print_tree(parentField);

    tree fieldType0=TREE_TYPE(childField);
    tree fieldFunType0=TREE_TYPE(fieldType0);
    tree fieldReturnType0=TREE_TYPE(fieldFunType0);


    tree fieldType1=TREE_TYPE(parentField);
    tree fieldFunType1=TREE_TYPE(fieldType1);
    tree fieldReturnType1=TREE_TYPE(fieldFunType1);
    printf("compareFunctionType 00 is %s\n",get_tree_code_name(TREE_CODE(fieldReturnType1)));
    bool re=c_tree_equal (fieldReturnType0,fieldReturnType1);
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
	n_ptr_array_add(codes,n_strdup(str->str));
	n_string_free(str,TRUE);
}


static void addResult(NPtrArray *array,int error,ClassName *childName,ClassName *belongChildClass,
		ClassName *parentName,ClassName *belongParentClass,ClassFunc *parentItem,tree childFieldDecl,char *childMangleName)
{
	OverrideFuncs *data=n_slice_new0(OverrideFuncs);
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
		  error_at(ploc,"父类%qs声明的方法%qs与子类%qs实现的方法相同，但返回值不同。",item->parentName->sysName,item->parentOrgiName,item->childName->sysName);
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
 *   b =newnew B
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
    char *result=n_strdup(source->str);
    n_string_free(source,TRUE);
  	n_ptr_array_set_free_func(codes,n_free);
  	n_ptr_array_unref(codes);
    return result;
}

/**
 * 加入初始化函数声明 Abc *Abc_AET_INIT_GLOBAL_METHOD_STRING_Abc(Abc *self);
 */
void       class_init_create_init_decl(ClassInit *self,ClassName *className)
{
	  c_parser *parser=self->parser;
	  c_token *semicolon = c_parser_peek_token (parser);//
	  int tokenCount=parser->tokens_avail;
	  if(tokenCount+9>AET_MAX_TOKEN){
			error("token太多了");
			return FALSE;
	  }
	  int i;
	  for(i=tokenCount;i>0;i--){
			 aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+9]);
	  }
	  aet_utils_create_token(&parser->tokens[8],CPP_SEMICOLON,";",1);
	  aet_utils_create_token(&parser->tokens[7],CPP_CLOSE_PAREN,")",1);
	  aet_utils_create_token(&parser->tokens[6],CPP_NAME,"self",4);
	  aet_utils_create_token(&parser->tokens[5],CPP_MULT,"*",1);
	  aet_utils_create_token(&parser->tokens[4],CPP_NAME,className->sysName,strlen(className->sysName));
	  parser->tokens[4].id_kind=C_ID_TYPENAME;//关键
	  aet_utils_create_token(&parser->tokens[3],CPP_OPEN_PAREN,"(",1);
	  char *initMethod=aet_utils_create_init_method(className->sysName);
	  aet_utils_create_token(&parser->tokens[2],CPP_NAME,initMethod,strlen(initMethod));
	  aet_utils_create_token(&parser->tokens[1],CPP_MULT,"*",1);
	  //aet_utils_create_token(&parser->tokens[0],CPP_NAME,className->sysName,strlen(className->sysName));
	  //parser->tokens[0].id_kind=C_ID_TYPENAME;//关键
	  aet_utils_create_void_token(&parser->tokens[0],semicolon->location);

	  parser->tokens_avail=tokenCount+9;
	  aet_print_token_in_parser("class_init_create_init_decl addInitDecl  例如：Abc *Abc_init_1234ergR5678_Abc(Abc *self) %s",className->sysName);
	  n_free(initMethod);
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
	//printf("class_init_modify_root_object_free_child %s %s\n",rootClassName->sysName,rootClassName->userName);
	char *freeChildMangle=func_mgr_get_mangle_func_name(func_mgr_get(),rootClassName,"free_child");
	//printf("class_init_modify_root_object_free_child 11 %s %s\n",rootClassName->sysName,freeChildMangle);
	if(freeChildMangle==NULL)
		return NULL;
 	char unrefName[255];
 	aet_utils_create_unref_name(className->userName,unrefName);
	char *unrefMangle=func_mgr_get_mangle_func_name(func_mgr_get(),className,unrefName);
	if(unrefMangle==NULL)
		return NULL;

 	NString *source=n_string_new("");
 	n_string_append_printf(source,"((%s *)%s)->%s=",rootClassName->sysName,varName,freeChildMangle);
 	n_string_append_printf(source,"%s->%s;\n",varName,unrefMangle);
 	char *result=n_strdup(source->str);
 	n_string_free(source,TRUE);
 	return result;
}

static void fillFreeChildMethodForAObject(ClassName *className,NString *codes)
{
	ClassName *rootClassName=class_mgr_get_class_name_by_user(class_mgr_get(),AET_ROOT_OBJECT);
	if(rootClassName==NULL)
		return ;
	//printf("class_init_modify_root_object_free_child %s %s\n",rootClassName->sysName,rootClassName->userName);
	char *freeChildMangle=func_mgr_get_mangle_func_name(func_mgr_get(),rootClassName,"free_child");
	//printf("class_init_modify_root_object_free_child 11 %s %s\n",rootClassName->sysName,freeChildMangle);
	if(freeChildMangle==NULL)
		return ;
 	char unrefName[255];
 	aet_utils_create_unref_name(className->userName,unrefName);
	char *unrefMangle=func_mgr_get_mangle_func_name(func_mgr_get(),className,unrefName);
	if(unrefMangle==NULL)
		return ;
 	n_string_append_printf(codes,"((%s *)self)->%s=%s;\n",rootClassName->sysName,freeChildMangle,unrefMangle);
}

static void fillGetClassMethodForAObject(ClassName *className,char *funcName,NString *codes)
{
	ClassName *rootClassName=class_mgr_get_class_name_by_user(class_mgr_get(),AET_ROOT_OBJECT);
	if(rootClassName==NULL)
		return ;
	//printf("class_init_modify_root_object_free_child %s %s\n",rootClassName->sysName,rootClassName->userName);
	char *getClassMangle=func_mgr_get_mangle_func_name(func_mgr_get(),rootClassName,"getClass");
	//printf("class_init_modify_root_object_free_child 11 %s %s\n",rootClassName->sysName,freeChildMangle);
	if(getClassMangle==NULL)
		return;
 	n_string_append_printf(codes,"((%s *)self)->%s=%s;\n",rootClassName->sysName,getClassMangle,funcName);
}

/**
 * 给父类的抽象方法赋值
 */
static void recursionModifyAbstractParentMethod(NString *codes,NPtrArray *childFuncs,ClassName *className,ClassName *orgi)
{
	 ClassInfo *info= class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
	 ClassName *parentClassName=&info->parentName;
	 NPtrArray *parentFuncs=func_mgr_get_funcs(func_mgr_get(),parentClassName);
	 if(parentFuncs==NULL)
		 return;
	 int i;
	 for(i=0;i<parentFuncs->len;i++){
		 ClassFunc *item=(ClassFunc *)n_ptr_array_index(parentFuncs,i);
		 if(item->isAbstract){
            //检果名字，参数返回值
			 char *praw=item->rawMangleName;
			 int j;
			 for(j=0;j<childFuncs->len;j++){
				 ClassFunc *child=(ClassFunc *)n_ptr_array_index(childFuncs,j);
				 if(strcmp(child->rawMangleName,praw)==0){
					 if(aet_utils_valid_tree(child->fromImplDefine))
					   n_string_append_printf(codes,"((%s *)self)->%s=%s;\n",parentClassName->sysName,item->mangleFunName,child->mangleFunName);
					 else
					   n_info("类%s中的抽象方法%s在类%s中没有找到实现。",parentClassName->sysName,item->orgiName,orgi->sysName);
					 break;
				 }
			 }
		 }
	 }
	 recursionModifyAbstractParentMethod(codes,childFuncs,parentClassName,orgi);
}

static void createModifyAbstractParentMethod(ClassName *className,NString *codes)
{
	 NPtrArray *childFuncs=func_mgr_get_funcs(func_mgr_get(),className);
	 recursionModifyAbstractParentMethod(codes,childFuncs,className,className);
}

/**
 * 初始化方法定义
 */
void class_init_create_init_define(ClassInit *self,ClassName *className,NPtrArray *ifaceCodes,char *superCodes,NString *buf)
{
     c_parser *parser=self->parser;
	 ClassInfo *info= class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
	 if(info==NULL){
	    c_parser_error (parser, "没有classInfo");
        return ;
	 }
	 int i;
	 if(ifaceCodes!=NULL){
	    for(i=0;i<ifaceCodes->len;i++){
		   IFaceSourceCode *code=(IFaceSourceCode *)n_ptr_array_index(ifaceCodes,i);
		   if(code->define)
		     n_string_append(buf,code->define);
	    }
	 }
	 char *initMethod=aet_utils_create_init_method(className->sysName);
	 n_string_append_printf(buf,"void * %s(%s *self)\n{\n",initMethod,className->sysName);
	 n_free(initMethod);
	 n_string_append(buf,"if(self==NULL)\n");//说明要获取class类信息
	 char getAClassFuncName[255];
	 class_build_create_func_name(className,getAClassFuncName);
	 n_string_append_printf(buf,"return (void *)%s(self);\n",getAClassFuncName);
	 if(info->parentName.sysName){
		 char *parentInitMethod=aet_utils_create_init_method(info->parentName.sysName);
		 n_string_append_printf(buf,"%s((%s *)self);\n",parentInitMethod,info->parentName.sysName);
		 n_free(parentInitMethod);
	 }else{
		 printf("说明是AObject 只能是aobject才有此功能。_init_super_data_123\n");
		 n_string_append_printf(buf,"%s((%s *)self);\n",SUPER_INIT_DATA_FUNC,className->sysName);
	 }
	 char *modify=func_mgr_create_field_modify_token(func_mgr_get(),className);
	 if(modify){
		 n_string_append(buf,modify);
		 n_free(modify);
	 }
	 fillFreeChildMethodForAObject(className,buf);
	 fillGetClassMethodForAObject(className,getAClassFuncName,buf);

	 //createModifyAbstractParentMethod(className,buf);
	 char *newCodes=modifyParentMethodNew(self,className);
	 //printf("newCodes0-----%s\n",newCodes0);
	 //printf("newCodes-----%s\n",newCodes);

	 if(newCodes!=NULL){
	    n_string_append(buf,newCodes);
	    n_free(newCodes);
	 }
	 //接口在这里加入
	 if(ifaceCodes!=NULL){
	    for(i=0;i<ifaceCodes->len;i++){
		   IFaceSourceCode *code=(IFaceSourceCode *)n_ptr_array_index(ifaceCodes,i);
		   n_string_append(buf,code->modify);
	    }
	 }

	 if(superCodes!=NULL){
	    n_string_append(buf,superCodes);
	 }
	 n_string_append(buf,"return (void*)self;\n}\n");
}

void class_init_create_init_define_for_interface(ClassInit *self,char *sysName,NString *buf)
{
     c_parser *parser=self->parser;
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
	 n_string_append_printf(buf,"return (void *)%s(self);\n",getAClassFuncName);
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

static char * getCanAssignFunc(NPtrArray *childFuncsArray,ClassFunc *parentItem,char *parentSysName,char *childSysName)
{
    int i=0;
    for(i=0;i<childFuncsArray->len;i++){
       ClassFunc *childItem=(ClassFunc *)n_ptr_array_index(childFuncsArray,i);
	   if(strcmp(childItem->rawMangleName,parentItem->rawMangleName)==0 &&
			   aet_utils_valid_tree(childItem->fromImplDefine) && !childItem->isAbstract && !childItem->isCtor && !parentItem->isCtor){
		     n_debug("class_init 类的func定义与接口的函数名是相同的。 index:%d %s %s\n", i,parentItem->mangleFunName,childItem->mangleFunName);
			 tree rtn=getRtn(childItem->fromImplDefine,FALSE);
			 tree parentRtn=getRtn(parentItem->fieldDecl,TRUE);
			 nboolean returnValue=c_tree_equal (rtn,parentRtn);
			 if(!returnValue){
				 location_t ploc = DECL_SOURCE_LOCATION (childItem->fromImplDefine);
				 error_at(ploc,"父类%qs声明的方法%qs与子类%qs实现的方法相同，但返回值不同。",parentSysName,parentItem->orgiName,childSysName);
				 return NULL;
			 }
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
      if(aet_utils_valid_tree(func->fieldDecl)){
    	  char *implDefine=getCanAssignFunc(childFuncsArray,func,parent->sysName,childName->sysName);
    	  if(implDefine!=NULL){
	           n_string_append_printf(codes,"((%s *)self)->%s=%s;\n",parent->sysName,func->mangleFunName,implDefine);
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
        	 	    n_string_append_printf(codes,"(&((%s *)self)->%s)->%s=%s;\n",parent->sysName,ifaceVarName,func->mangleFunName,implDefine);
           	    }
             }
        }
    }
   if(parentInfo->parentName.sysName==NULL)
      return ;
   assignToParent(self,childName,&parentInfo->parentName,codes);
}

static char *modifyParentMethodNew(ClassInit *self,ClassName *childName)
{
      ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),childName);
      if(info==NULL){
   	     return NULL;
      }
      if(info->parentName.sysName==NULL)
   	     return NULL;
      NString *codes=n_string_new("");
      assignToParent(self,childName,&info->parentName,codes);
      if(codes->len==0){
          n_string_free(codes,TRUE);
    	  return NULL;
      }
      char *result=n_strdup(codes->str);
      n_string_free(codes,TRUE);
      return result;
}

ClassInit *class_init_new()
{
	ClassInit *self = n_slice_alloc0 (sizeof(ClassInit));
	classInitInit(self);
	return self;
}


