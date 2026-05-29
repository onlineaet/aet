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

#include "function.h"
#include "tree.h"
#include "timevar.h"
#include "stringpool.h"
#include "cgraph.h"
#include "attribs.h"
#include "toplev.h"

#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "c-family/c-common.h"
#include "gimple-expr.h"

#include "c/c-tree.h"

#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "c-aet.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"

#include "../libcpp/include/cpplib.h"
#include "aet-c-parser-header.h"

#include "aetutils.h"
#include "classmgr.h"
#include "funcmgr.h"
#include "aetinfo.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "parserstatic.h"
#include "genericutil.h"
#include "classutil.h"
#include "varmgr.h"
#include "mtcsparser.h"
#include "makefileparm.h"


static void freeClass_cb(npointer userData)
{
  printf("funcmgr free free %p\n",userData);
}

static void funcMgrInit(FuncMgr *self)
{
	self->hashTable = n_hash_table_new_full (n_str_hash, n_str_equal,n_free, freeClass_cb);
	self->staticHashTable = n_hash_table_new_full (n_str_hash, n_str_equal,n_free, freeClass_cb);
	self->mangle=aet_mangle_new();
}


static nboolean existsStaticFuncAtField(FuncMgr *self,char *funcName,ClassName *className)
{
	if(!n_hash_table_contains(self->hashTable,className->sysName)){
		return FALSE;
	}
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,className->sysName);
	int i;
	for(i=0;i<array->len;i++){
	   ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
	   if(strcmp(item->mangleNoSelfName,funcName)==0){
		   printf("existsStaticFuncAtField 在class中的field函数声明中找到同名的: mangle：%s funcName:%s className:%s\n",
				   item->mangleFunName,funcName,className->sysName);
		   return TRUE;
	   }
	}
    return FALSE;
}

static nboolean existsSameStaticFunc(FuncMgr *self,char *newNameNoSelf,ClassName *className)
{
	if(!n_hash_table_contains(self->staticHashTable,className->sysName)){
		return NULL;
	}
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,className->sysName);
	int i;
	for(i=0;i<array->len;i++){
		ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
	   if(strcmp(item->mangleFunName,newNameNoSelf)==0){
		   n_debug("existsSameStaticFunc 已经存在的mangle  %s",item->mangleFunName);
		   return TRUE;
	   }
	}
    return FALSE;
}

static ClassFunc *getStaticFunc(FuncMgr *self,char *newNameNoSelf,ClassName *className)
{
   if(!n_hash_table_contains(self->staticHashTable,className->sysName))
      return NULL;
   NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,className->sysName);
   int i;
   for(i=0;i<array->len;i++){
      ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
      if(strcmp(item->mangleFunName,newNameNoSelf)==0)
         return item;
   }
   return NULL;
}

/**
 * location_t 在 libcpp/line-map.h中定义:typedef unsigned int location_t;
 */
static location_t getDeclaratorLocation(struct c_declarator *declarator)
{
   location_t id_loc=declarator->id_loc;
   if(id_loc==0){
      bool macroLoc=linemap_location_from_macro_expansion_p (line_table,id_loc);
      if(macroLoc){
         n_warning("getDeclaratorLocation is ----是一个宏位置 %u",id_loc);
         return id_loc;
      }
   }
   if(id_loc>2*input_location){
      n_warning("struct c_declarator 的位置大于当前输入位置，不正常!!! %u %u\n",id_loc,input_location);
      id_loc=input_location;
      declarator->id_loc=input_location;
   }
   expanded_location  xloc = expand_location(id_loc);
   if(xloc.line==0 && xloc.column==0){
      expanded_location  xloc1 = expand_location(input_location);
      n_info("getDeclaratorLocation 位置是 取input_location %d %d %d %d",xloc.line,xloc.column,xloc1.line,xloc1.column);
      id_loc=input_location;
   }
   return id_loc;
}


static ClassFunc *getEntity(NPtrArray *array,char *mangle)
{
   int i;
   for(i=0;i<array->len;i++){
	   ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
	   if(strcmp(item->mangleFunName,mangle)==0){
		   n_debug("getEntity 已经存在的mangle %s %s",item->mangleFunName,mangle);
		   return item;
	   }
   }
   return NULL;
}

static ClassFunc *getClassFunc(FuncMgr *self,tree decl,ClassName *className)
{
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,className->sysName);
	if(array==NULL){
		n_warning("找不到类%s的函数集合。",className->sysName);
		return NULL;
	}
	tree funName=DECL_NAME(decl); //函数名
	char *mangleName=IDENTIFIER_POINTER(funName);
	ClassFunc *my=getEntity(array,mangleName);
	if(my==NULL){
		n_debug("类%s中的函数%s不存在！。",className->sysName,mangleName);
		return NULL;
	}
	return my;
}

static ClassFunc *getClassFuncFromStatic(FuncMgr *self,tree decl,ClassName *className)
{
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,className->sysName);
	if(array==NULL){
		n_warning("找不到类%s的函数集合。",className->sysName);
		return NULL;
	}
	tree funName=DECL_NAME(decl); //函数名
	char *mangleName=IDENTIFIER_POINTER(funName);
	ClassFunc *my=getEntity(array,mangleName);
	if(my==NULL){
	    n_debug("类%s中的函数%s不存在！。",className->sysName,mangleName);
		return NULL;
	}
	return my;
}

static int getSerialNumber(FuncMgr *self,ClassName *className)
{
    int max1=var_mgr_get_max_serial_number(var_mgr_get(),className);
    int max2=func_mgr_get_max_serial_number(self,className);
    return max2>max1?(max2+1):(max1+1);
}

static ClassFunc *createEntity(tree decl,tree classTree,char *mangle,char *orgiName,enum func_from_code code,nboolean isCtor,
		                         nboolean isFinalized,nboolean isUnref,char *mangleNoSelfName,char *rawMangleName)
{
	ClassFunc *item=class_func_new();
	item->isAbstract=FALSE;
	item->isCtor=isCtor;
	item->isFinalized=isFinalized;
	item->isUnref=isUnref;
	item->orgiName=n_strdup(orgiName);
	item->mangleFunName=n_strdup(mangle);
	item->mangleNoSelfName=n_strdup(mangleNoSelfName);
	item->rawMangleName=n_strdup(rawMangleName);
    item->isStatic=(mangleNoSelfName==NULL)?TRUE:FALSE;
	item->fieldDecl=NULL_TREE;
	item->fromImplDefine=NULL_TREE;
    item->classTree=classTree;
	if(code==STRUCT_DECL){
	  item->fieldDecl=decl;
	}else if(code==CLASS_IMPL_DEFINE){
	  item->fromImplDefine=decl;
	}
	item->permission=CLASS_PERMISSION_DEFAULT;
	return item;
}

static nboolean setEntity(ClassFunc *item,tree decl,ClassName *className,tree classTree,char *mangle,char *orgiName,enum func_from_code code)
{
   nboolean repeat=FALSE;
   if(code==STRUCT_DECL){
      if(!aet_utils_valid_tree(item->fieldDecl)){
         item->fieldDecl=decl;
         item->classTree=classTree;
      }else{
         repeat=TRUE;
      }
   }else if(code==CLASS_IMPL_DEFINE){
      if(!aet_utils_valid_tree(item->fromImplDefine)){
         if(item->isAbstract){
            error_at(DECL_SOURCE_LOCATION(decl),"类%qs的抽象方法%qs，只能由子类实现。",className->userName,orgiName);
            return FALSE;
         }
         item->fromImplDefine=decl;
      }else{
         repeat=TRUE;
      }
   }
   return repeat;
}

static nboolean  setFieldDecl(FuncMgr *self,tree decl,ClassName *className,enum func_from_code code)
{
	ClassFunc *func=getClassFunc(self,decl,className);
	if(func==NULL)
		return FALSE;
	return class_func_set_decl(func,decl,code);
}

/**
 * 函数参数
 * 1.haveQueryParam 有问号泛型参数
 * 2.haveGenericClassParam 有泛型类参数
 * 3.allParmIsQuery 所有的参数是问号参数
 */
static void fillGenericFuncType(tree args,int *haveQueryParam,int *haveGenericClassParam,int *allParmIsQuery)
{
	tree parm=NULL_TREE;
	int count=0;
	int queryCount=0;
	int noQueryCount=0;
	for (parm = args; parm; parm = DECL_CHAIN (parm)){
		GenericModel *gen=c_aet_get_generics_model(parm);
		if(gen && count>0){ //跳过self
			nboolean find=generic_model_have_query(gen);
			n_debug("第%d个参数是问号参数吗?%s",count,find?"是":"否");
			if(find){
				*haveQueryParam=1;
				*haveGenericClassParam=1;
				 queryCount++;
				 //break;
			}else{
				tree type=TREE_TYPE(parm);
				char *className=class_util_get_class_name(type);
				if(className!=NULL){
					ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),className);
					if(class_info_is_generic_class(info)){
						*haveGenericClassParam=1;
						noQueryCount++;
					}
				}
			}
		}
		count++;
	}
	if(queryCount>0 && noQueryCount==0)
		*allParmIsQuery=1;
}

/**
 * 找类ClassName中的接口是否声明过指定的函数
 */
static nboolean findIntefaceFunc(FuncMgr *self,ClassName *className,char *rawMangleName)
{
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   if(!info)
      return FALSE;
   int i;
   for(i=0;i<info->ifaceCount;i++){
      ClassName *iface=&(info->ifaces[i]);
      NPtrArray *funcArray=func_mgr_get_funcs(self,iface);
      int j;
      if(funcArray){
         for(j=0;j<funcArray->len;j++){
            ClassFunc *f=n_ptr_array_index(funcArray,j);
            if(!strcmp(f->rawMangleName,rawMangleName))
               return TRUE;
         }
      }
   }
   return FALSE;
}

/**
 *  mangleNoself 生成mangle名时不带self
 */
static ClassFunc *addSubstitutions(FuncMgr *self,tree decl,tree classTree,ClassName *className,
		 char *mangle,char *orgiName,enum func_from_code code,char *mangleNoSelf,char *rawMangleName,tree parms,int *errInfo)
{
   int ret=0;
   nboolean isCtor=strcmp(className->userName,orgiName)==0;
   nboolean isFinalized= aet_utils_is_finalize_name(className->userName,orgiName);
   nboolean isUnref= aet_utils_is_unref_name(className->userName,orgiName);
   nboolean haveQueryParam=FALSE;
   nboolean haveGenericClassParam=FALSE;
   nboolean allParmIsQuery=0;
   fillGenericFuncType(parms,&haveQueryParam,&haveGenericClassParam,&allParmIsQuery);
   ClassFunc *item=NULL;
   n_debug("addSubstitutions通过参数判断函数是什么泛型类型的函数:%s 问号:%d 泛型类:%d 全是问号:%d decl:%p\n",
         orgiName,haveQueryParam,haveGenericClassParam,allParmIsQuery,decl);
   if(!n_hash_table_contains(self->hashTable,className->sysName)){
      item=createEntity(decl,classTree,mangle,orgiName,code,isCtor,isFinalized,isUnref,mangleNoSelf,rawMangleName);
      item->isQueryGenFunc=haveQueryParam;
      item->isGenericParmFunc=haveGenericClassParam;
      item->allParmIsQuery=allParmIsQuery;
      item->serialNumber=getSerialNumber(self,className);
      item->className = class_name_clone(className);
      if(code==STRUCT_DECL){
         class_func_save_generic_model_for_field_decl(item,parms);
      }
      NPtrArray *array=n_ptr_array_sized_new(2);
      n_ptr_array_add(array,item);
      n_debug("addSubstitutions 00 第一次加 class:%s mangle:%s org:%s rawMangleName:%s code:%d item:%p NPtrArray:%p self:%p parms:%p\n",
      className->sysName,mangle,orgiName,rawMangleName,code,item,array,self,parms);
      n_hash_table_insert (self->hashTable, n_strdup(className->sysName),array);
   }else{
      NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,className->sysName);
      item=getEntity(array,mangle);
      n_debug("addSubstitutions 11 第二次加 class:%s mangle:%s org:%s rawMangleName:%s 函数类型：code:%d item:%p array:%p parms:%p\n",
      className->sysName,mangle,orgiName,rawMangleName,code,item,array,parms);
      if(item==NULL){
         item=createEntity(decl,classTree,mangle,orgiName,code,isCtor,isFinalized,isUnref,mangleNoSelf,rawMangleName);
         item->isQueryGenFunc=haveQueryParam;
         item->isGenericParmFunc=haveGenericClassParam;
         item->allParmIsQuery=allParmIsQuery;
         item->serialNumber=getSerialNumber(self,className);
         item->className = class_name_clone(className);
         n_ptr_array_add(array,item);
      }else{
         nboolean repeat=setEntity(item,decl,className,classTree,mangle,orgiName,code);
         if(repeat){
            printf("is abstract classfun :%d %s error:%d\n",class_func_is_abstract(item),item->orgiName,ret);
            if(item->fromInterface)
               ret=-2;
            else
               ret=-1;
         }
         //检查是否与接口声明的方法相同。新的接口实现，是复制接口方法放到class的声明区，所以可以相同
         //if(ret!=-1 && findIntefaceFunc(self,className,rawMangleName)){
         //   ret=-2;
        // }
      }
      //如果item->fieldParms ==ggc_freed 说明被释放了（一般头文件中声明的类方法），现在有函数有了新的定义parms，
      //fieldParms已从classfunc中移走了。
      //替换，这项更改来自bug 见日志 为什么会变成ggc_freed呢？取出泛型genericmodel存入
      if(code==STRUCT_DECL){
         class_func_save_generic_model_for_field_decl(item,parms);
      }
   }
   *errInfo =ret;
   return item;
}

/**
 */
static ClassFunc *addFunc(FuncMgr *self,tree structTree,ClassName *className,enum func_from_code fromType,
		location_t id_loc,struct c_declarator *funcdecl,struct c_declspecs *specs,int *errInfo)
{
   struct c_declarator *funid=class_util_get_function_id(funcdecl);
   if(funid==NULL)
      return NULL;
   tree argTypes = funcdecl->u.arg_info->types;
   tree funName=funid->u.id.id;
   char *orgiName=n_strdup(IDENTIFIER_POINTER(funName));
   char *newName=aet_mangle_create(self->mangle,funName,argTypes,className->userName);
   //获取没有self的函数名，用在static函数
   tree noSelfArgTypes = TREE_CHAIN (argTypes);
   char *newNameNoSelf=aet_mangle_create(self->mangle,funName,noSelfArgTypes,className->userName);
   char *staticNewName=aet_mangle_create(self->mangle,funName,noSelfArgTypes,className->sysName);
   char *rawMangleName=aet_mangle_create(self->mangle,funName,noSelfArgTypes,"");

   //printf("addFunc dddd vvv %s %s %s %s %s fromType:%d\n",orgiName,newName,newNameNoSelf,staticNewName,rawMangleName,fromType);
   //检查与静态函数是否重名
   ClassFunc *func=NULL;
   func = getStaticFunc(self,staticNewName,className);
   if(func){
      if(fromType==CLASS_IMPL_DEFINE){
         n_debug("函数定义是实现了的静态声明函数 %s newNameNoSelf:%s\n",className->userName,staticNewName);
         //去除self参数，把函数名改为staticNewName
         tree value = aet_utils_create_ident (staticNewName);
         funid->u.id.id=value;
         struct c_arg_info *args=funcdecl->u.arg_info;
         args->types=noSelfArgTypes;
         args->parms=TREE_CHAIN (args->parms);
         specs->storage_class=csc_none;
         return func;
      }else{
         error_at(id_loc,"类%qs有重复的静态函数%qs。",className->userName,orgiName);
      }
      n_free(orgiName);
      n_free(newName);
      n_free(newNameNoSelf);
      n_free(rawMangleName);
      return NULL;
   }
   //printf("addFunc 0000 %s %p code:%d exitsAetGenericInfoParm:%d\n",orgiName,funcdecl->u.arg_info->parms,fromType,exitsAetGenericInfoParm);
   int ret=0;
   func=addSubstitutions(self,NULL_TREE,structTree,className,newName,orgiName,
         fromType,newNameNoSelf,rawMangleName,funcdecl->u.arg_info->parms,&ret);
   //printf("addFunc 1111 %s %p code:%d\n",orgiName,funcdecl->u.arg_info->parms,fromType);
   //printf("addFunc 00 sysName:%s userName:%s newFunName:%s newNameNoSelf:%s fromType:%d repeat:%d %s %s %d\n",
   //className->sysName,className->userName,newName,newNameNoSelf,fromType,repeat,__FILE__,__FUNCTION__, __LINE__);
   nboolean result=FALSE;
   if(ret==-1)
      error_at(id_loc,"类%qs有重复的函数名%qs。",className->userName,orgiName);
   else if(ret==-2){
      //error_at(id_loc,"类%qs声明了与接口重复的函数名%qs。",className->userName,orgiName);
      if(errInfo)
         *errInfo=-2;
   }else{
      tree value = aet_utils_create_ident (newName);
      funid->u.id.id=value;
   }
   n_free(orgiName);
   n_free(newName);
   n_free(newNameNoSelf);
   n_free(rawMangleName);
   return func;
}

static nboolean  setStaticDecl(FuncMgr *self,tree decl,ClassName *className,enum func_from_code code)
{
	ClassFunc *item=getClassFuncFromStatic(self,decl,className);
	if(item==NULL)
		return FALSE;
	if(code==STRUCT_DECL){
	   item->fieldDecl=decl;
	}else if(code==CLASS_IMPL_DEFINE){
	   item->fromImplDefine=decl;
	}
	return TRUE;
}

//0表示没有错误，但不创建classfunc
//1表示成功创建了classfunc
//2表示classfunc已存在。
ClassFunc *func_mgr_change_class_func_decl(FuncMgr *self,struct c_declarator *declarator,
      ClassName *className,tree structTree,int *errInfo)
{
   struct c_declarator *funcdel=class_util_get_function_declarator(declarator);
   if(!funcdel)
        return NULL;
   struct c_declarator *funid=class_util_get_function_id(funcdel);
   location_t id_loc=getDeclaratorLocation(funid);
   return addFunc(self,structTree,className,STRUCT_DECL,id_loc,funcdel,NULL,errInfo);
}


static nboolean checkSameFuncButRtnNotEqual(FuncMgr *self,tree decl,ClassName *className,ClassFunc *compare)
{
    ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
    ClassName *parent=&(info->parentName);
    if(parent==NULL || parent->sysName==NULL)
    	return FALSE;
    NPtrArray  *array=func_mgr_get_funcs(self,parent);
    if(array==NULL){
	   return checkSameFuncButRtnNotEqual(self,decl,parent,compare);
    }
    int i;
    for(i=0;i<array->len;i++){
	   ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
	   if(aet_utils_valid_tree(item->fieldDecl)){
		  nboolean re= class_func_is_same_but_rtn(item,compare,decl);
		  if(re){
             error_at(DECL_SOURCE_LOCATION(decl),"在类%s找到了函数名相同但返回值不同的函数%s。",parent->sysName,item->orgiName);
             return TRUE;
		  }
	   }
   }
   return checkSameFuncButRtnNotEqual(self,decl,parent,compare);
}

/**
 * 与 mtcs_parser_create_device_func_pointers_var 的变量元素位置是统一的。
 * mangleFunName在数组 deviceFuncPointers中的位置
 */
//mangleFunName在数组 deviceFuncPointers中的位置
int func_mgr_get_device_func_index(FuncMgr *self,ClassName *className,char *mangleFunName)
{
   NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,className->sysName);
   if(array==NULL)
      return -1;
   int i;
   int index=0;
   for(i=0;i<array->len;i++){
      ClassFunc *func=(ClassFunc *)n_ptr_array_index(array,i);
      if(class_func_is_device(func) && aet_utils_valid_tree(func->fromImplDefine)){
         if(!strcmp(func->mangleFunName,mangleFunName))
            return index;
         else
            index++;
      }
   }
   return -1;
}


NPtrArray  *func_mgr_get_funcs(FuncMgr *self,ClassName *className)
{
	if(className==NULL || className->sysName==NULL){
      return NULL;
	}
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,className->sysName);
	return array;
}

static ClassFunc *getClassFuncByMangleName(FuncMgr *self,char *sysName,char *mangle)
{
   if(sysName==NULL || mangle==NULL)
      return NULL;
   NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,sysName);
   if(array==NULL)
      return NULL;
   int i;
   NPtrArray* data=n_ptr_array_new();
   for(i=0;i<array->len;i++){
      ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
      if(strcmp(item->mangleFunName,mangle)==0){
         return item;
      }
   }
   return NULL;
}

ClassFunc *func_mgr_get_entity(FuncMgr *self,ClassName *className,char *mangle)
{
   if(className==NULL || mangle==NULL)
      return NULL;
    return getClassFuncByMangleName(self,className->sysName,mangle);
}

ClassFunc *func_mgr_get_entity_by_sys_name(FuncMgr *self,char *sysName,char *mangle)
{
    return getClassFuncByMangleName(self,sysName,mangle);
}

ClassFunc *func_mgr_get_func_by_mangle(FuncMgr *self,char *mangle)
{
	NHashTableIter iter;
	npointer key, value;
	n_hash_table_iter_init(&iter, self->hashTable);
	int count=0;
	while (n_hash_table_iter_next(&iter, &key, &value)) {
		char *sysName = (char *)key;
		ClassFunc *func=getClassFuncByMangleName(self,sysName,mangle);
		if(func!=NULL)
			return func;
	}
	return NULL;
}

/**
 * 返回mangle函数名定义或声明所在的类名
 */
char   *func_mgr_get_class_name_by_mangle(FuncMgr *self,char *mangle)
{
   if(mangle==NULL)
      return NULL;
   NHashTableIter iter;
   npointer key, value;
   n_hash_table_iter_init(&iter, self->hashTable);
   int count=0;
   while (n_hash_table_iter_next(&iter, &key, &value)) {
      char *sysName = (char *)key;
      ClassFunc *func=getClassFuncByMangleName(self,sysName,mangle);
      if(func!=NULL)
         return sysName;
   }
   return NULL;
}

/**
 * 开始类函数定义
 */
ClassFunc *func_mgr_start_func_define(FuncMgr *self,struct c_declspecs *specs,
      struct c_declarator *declarator,ClassName *className,int *errInfo)
{
   enum func_from_code fromType=CLASS_IMPL_DEFINE;
   struct c_declarator *funcdel=class_util_get_function_declarator(declarator);
   if(!funcdel)
      return NULL;
   struct c_declarator *funid=class_util_get_function_id(funcdel);
   location_t id_loc=getDeclaratorLocation(funid);
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   return addFunc(self,info->record,className,fromType,id_loc,funcdel,specs,errInfo);
}

/**
 * 完成类函数定义
 */
nboolean  func_mgr_end_func_define(FuncMgr *self,tree decl,ClassName *className)
{
	enum func_from_code fromType=CLASS_IMPL_DEFINE;
   enum tree_code code=TREE_CODE(decl);
   if(code!=FUNCTION_DECL){
        n_warning("不是一个函数声明! %s",get_tree_code_name(TREE_CODE(decl)));
        return FALSE;
   }
   if(className==NULL){
        n_warning("CLASS 没有名字 !");
        return FALSE;
   }
    //函数名的线索来自
   nboolean ok= setFieldDecl(self,decl,className,fromType);
   if(!ok){
      n_info("在静态表中找有没有对应的函数 %s",IDENTIFIER_POINTER(DECL_NAME(decl)));
      nboolean re=setStaticDecl(self,decl,className,fromType);
      return re;
   }
   ClassFunc *func=getClassFunc(self,decl,className);
   ok=class_func_is_same_generic(func);
   GenericModel *fungen=c_aet_get_func_generics_model(decl);
   if(fungen){
      if(!aet_utils_valid_tree(func->fieldDecl)){
         error_at(DECL_SOURCE_LOCATION(decl),"在类%s中没有声明泛型函数%qs。",className->sysName,func->orgiName);
         return FALSE;
      }
   }
   return ok;
}

/**
 * 是否存在函数的field或声明或定义，如果存在返回true
 */
nboolean func_mgr_func_exits(FuncMgr *self,ClassName *className,char *orgiName)
{
	if(className==NULL || orgiName==NULL)
		return FALSE;
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,className->sysName);
	if(array==NULL)
		return FALSE;
    int i;
    for(i=0;i<array->len;i++){
    	ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
      	if(strcmp(item->orgiName,orgiName)==0){
      		return TRUE;
      	}
    }
    return FALSE;
}

char * func_mgr_get_mangle_func_name(FuncMgr *self,ClassName *className,char *orgiName)
{
	if(className==NULL || orgiName==NULL)
		return NULL;
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,className->sysName);
	if(array==NULL)
		return NULL;
    int i;
    for(i=0;i<array->len;i++){
    	ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
      	if(strcmp(item->orgiName,orgiName)==0){
      		return item->mangleFunName;
      	}
    }
    return NULL;
}

NPtrArray *func_mgr_get_constructors(FuncMgr *self,ClassName *className)
{
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,className->sysName);
	if(array==NULL || array->len==0)
		return NULL;
	int i;
	NPtrArray* data=n_ptr_array_new();
	for(i=0;i<array->len;i++){
		ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
		if(strcmp(item->orgiName,className->userName)==0 && aet_utils_valid_tree(item->fieldDecl)){
			//printf("func_mgr_get_constructors %s %s\n",item->mangleFunName,className->sysName);
			n_ptr_array_add(data,item);
		}
	}
	return data;
}

int func_mgr_get_orig_func_and_class_name(FuncMgr *self,char *mangleName,char *className,char *funcName)
{
	return  aet_mangle_get_orgi_func_and_class_name(self->mangle,mangleName,className,funcName);
}


//--------------------------------以下是静态函数------------------------------------
nboolean func_mgr_set_static_func_premission(FuncMgr *self,ClassName *className,tree funDecl,ClassPermissionType permission,nboolean isFinal)
{
   NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,className->sysName);
   if(array==NULL)
	   return FALSE;
   int i;
   for(i=0;i<array->len;i++){
	   ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
	   if(item->fieldDecl==funDecl){
		   item->permission=permission==CLASS_PERMISSION_DEFAULT?CLASS_PERMISSION_PROTECTED:permission;
		   item->isFinal=isFinal;
		   return TRUE;
	   }
   }
   return FALSE;
}

/**
 * 用系统名（带包名的）生成函数名，而field生成函数名用的是用户名
 */
char *func_mgr_create_static_var_name(FuncMgr *self,ClassName *className,tree varName,tree type)
{
	return aet_mangle_create_static_var_name(self->mangle,className,varName,type);
}

nboolean  func_mgr_change_static_func_decl(FuncMgr *self,struct c_declarator *declarator,ClassName *className,tree structTree)
{
   struct c_declarator *funcdel=class_util_get_function_declarator(declarator);
   if(!funcdel)
      return FALSE;
   struct c_declarator *funid=funcdel->declarator;
   if(funid==NULL)
      return FALSE;
   enum c_declarator_kind kind=funid->kind;
   if(kind!=cdk_id)
      return FALSE;
   location_t id_loc=getDeclaratorLocation(funid);
   struct c_arg_info *args=funcdel->u.arg_info;
   tree argTypes = args->types;
   tree funName=funid->u.id.id;
   char *orgiName=n_strdup(IDENTIFIER_POINTER(funName));
   char *newName=aet_mangle_create(self->mangle,funName,argTypes,className->userName);
   char *newSysName=aet_mangle_create(self->mangle,funName,argTypes,className->sysName);
   char *rawMangleName=aet_mangle_create(self->mangle,funName,argTypes,"");
   if(existsSameStaticFunc(self,newSysName,className)){
      n_debug("func_mgr_change_static_func_decl 11 existsStaticFunc %s %s class:%s\n",newName,newSysName,className->sysName);
      error_at (id_loc,"%qE 有同名的函数声明，并且参数也是一样的 ！", funName);
      n_free(orgiName);
      n_free(newName);
      n_free(newSysName);
      n_free(rawMangleName);
      return FALSE;
   }

   if(existsStaticFuncAtField(self,newName,className)){
      n_debug("func_mgr_change_static_func_decl 22 existsStaticFunc %s %s class:%s\n",newName,newSysName,className->sysName);
      error_at (id_loc,"%qE 有同名的函数声明，并且参数也是一样的 ！", funName);
      n_free(orgiName);
      n_free(newName);
      n_free(newSysName);
      n_free(rawMangleName);
      return FALSE;
   }
   nboolean haveQueryParam=0;
   nboolean haveGenericClassParam=0;
   nboolean allParmIsQuery=0;
   fillGenericFuncType(args->parms,&haveQueryParam,&haveGenericClassParam,&allParmIsQuery);
   n_debug("通过参数判断静态函数是什么泛型类型的函数:%s 问号:%d 泛型类:%d 全是问号:%d\n",orgiName,haveQueryParam,haveGenericClassParam,allParmIsQuery);
   nboolean result=TRUE;
   if(!n_hash_table_contains(self->staticHashTable,className->sysName)){
      ClassFunc *item=createEntity(NULL_TREE,structTree,newSysName,orgiName, STRUCT_DECL,FALSE,FALSE,FALSE,NULL,rawMangleName);
      item->isQueryGenFunc=haveQueryParam;
      item->isGenericParmFunc=haveGenericClassParam;
      item->allParmIsQuery=allParmIsQuery;
      item->serialNumber=getSerialNumber(self,className);
      item->className = class_name_clone(className);
      NPtrArray *array=n_ptr_array_sized_new(2);
      n_ptr_array_add(array,item);
      n_debug("func_mgr_change_static_func_decl 33 第一次加 class:%s mangle:%s org:%s code:%d item:%p NPtrArray:%p self:%p\n",
      className->sysName,newSysName,orgiName,STRUCT_DECL,item,array,self);
      n_hash_table_insert (self->staticHashTable, n_strdup(className->sysName),array);
   }else{
      NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,className->sysName);
      ClassFunc *item=getEntity(array,newSysName);
      n_debug("func_mgr_change_static_func_decl 44 第二次加 class:%s mangle:%s org:%s 函数类型：code:%d item:%p array:%p\n",
      className->sysName,newSysName,orgiName,STRUCT_DECL,item,array);
      if(item==NULL){
         item=createEntity(NULL_TREE,structTree,newSysName,orgiName, STRUCT_DECL,FALSE,FALSE,FALSE,NULL,rawMangleName);
         item->isQueryGenFunc=haveQueryParam;
         item->isGenericParmFunc=haveGenericClassParam;
         item->allParmIsQuery=allParmIsQuery;
         item->serialNumber=getSerialNumber(self,className);
         item->className = class_name_clone(className);
         n_ptr_array_add(array,item);
      }else{
         error_at (id_loc,"%qE 有同名的函数声明，并且参数也是一样的 ！", funName);
         result=FALSE;
      }
   }
   if(result){
      tree value = aet_utils_create_ident (newSysName);
      funid->u.id.id=value;
   }
   n_free(orgiName);
   n_free(newName);
   n_free(newSysName);
   n_free(rawMangleName);
   return result;
}

nboolean  func_mgr_set_static_func_decl(FuncMgr *self,tree funcDecl,ClassName *className,nboolean define)
{
   enum tree_code code=TREE_CODE(funcDecl);
   if(code!=FUNCTION_DECL){
      error_at (DECL_SOURCE_LOCATION (funcDecl),"%qD 不是函数声明 ！", funcDecl);
      return FALSE;
   }
   //函数名的线索来自
   tree funName=DECL_NAME(funcDecl); //函数名
   char *mangleName=IDENTIFIER_POINTER(funName);
   n_debug("func_mgr_set_static_func_decl 00 %s funcDecl:%p className:%s",mangleName,funcDecl,className->sysName);
   aet_print_tree(funcDecl);
   nboolean ok = setStaticDecl(self,funcDecl,className,STRUCT_DECL);
   if(define)
      ok= setStaticDecl(self,funcDecl,className,CLASS_IMPL_DEFINE);
   return ok;
}

nboolean func_mgr_static_func_exits(FuncMgr *self,ClassName *className,char *orgiName)
{
	if(className==NULL || orgiName==NULL)
		return FALSE;
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,className->sysName);
	if(array==NULL)
		return FALSE;
    int i;
    for(i=0;i<array->len;i++){
        ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
      	if(strcmp(item->orgiName,orgiName)==0){
      		return TRUE;
      	}
    }
    return FALSE;
}

NPtrArray    *func_mgr_get_static_funcs(FuncMgr *self,ClassName *className)
{
	if(className==NULL){
		return NULL;
	}
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,className->sysName);
	return array;
}

NPtrArray    *func_mgr_get_static_funcs_by_sys_name(FuncMgr *self,char *sysName)
{
    if(sysName==NULL){
        return NULL;
    }
    NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,sysName);
    return array;
}

nboolean func_mgr_static_func_exits_by_recursion(FuncMgr *self,ClassName *srcName,tree component)
{
	if(srcName==NULL || component==NULL_TREE)
		return FALSE;
	ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),srcName);
	if(info==NULL)
		return FALSE;
	nboolean exits=func_mgr_static_func_exits(self,&info->className,IDENTIFIER_POINTER(component));
	if(exits)
		return TRUE;
	return func_mgr_static_func_exits_by_recursion(self,&info->parentName,component);
}

/**
 * 在类声明中是否存在泛型函数
 */
nboolean  func_mgr_have_generic_func(FuncMgr *self,ClassName *className)
{
	if(className==NULL)
		return FALSE;
    NPtrArray  *array=func_mgr_get_funcs(self,className);
    if(array==NULL)
    	return FALSE;
    int i;
    for(i=0;i<array->len;i++){
  	   ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
  	   if(aet_utils_valid_tree(item->fieldDecl)){
  		  GenericModel *gen=c_aet_get_func_generics_model(item->fieldDecl);
  		  if(gen)
  			 return TRUE;
  	   }
    }
    return FALSE;
}

nboolean  func_mgr_is_generic_func(FuncMgr *self,ClassName *className,char *mangleFuncName)
{
	   if(className==NULL)
			return FALSE;
	    NPtrArray  *array=func_mgr_get_funcs(self,className);
	    if(array==NULL)
	    	return FALSE;
	    int i;
	    for(i=0;i<array->len;i++){
	  	   ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
	  	   if(!strcmp(item->mangleFunName,mangleFuncName)){
	  	      if(aet_utils_valid_tree(item->fieldDecl)){
	  		     GenericModel *gen=c_aet_get_func_generics_model(item->fieldDecl);
	  		     if(gen)
	  		   	   return TRUE;
	  	      }
	  	   }
	    }
	    return FALSE;
}

/**
 * 把function_type中的参数写成字符
 */
char *func_mgr_create_parm_string(FuncMgr *self,tree funcType)
{
   return aet_mangle_create_parm_string(self->mangle,funcType);
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

/**
 * 从类from中开始找是否实现了接口方法.
 * from找不到，从父类中找，一直找到AObject
 */
ClassFunc *func_mgr_get_interface_impl00(FuncMgr *self,ClassName *from,ClassFunc *interfaceMethod,char **atClass)
{
	 NPtrArray  *srcFuncs=func_mgr_get_funcs(func_mgr_get(),from);
	 char *praw=interfaceMethod->rawMangleName;
	 int i;
	 if(srcFuncs!=NULL){
		 for(i=0;i<srcFuncs->len;i++){
			 ClassFunc *compareFunc=(ClassFunc *)n_ptr_array_index(srcFuncs,i);
			 if(strcmp(compareFunc->rawMangleName,praw)==0){
			     n_debug("在类中找接口的方法。fieldDecl:%p %s %s public:%d\n",
			           compareFunc->fieldDecl,from->sysName,compareFunc->rawMangleName,class_func_is_public(compareFunc));
				 if(aet_utils_valid_tree(compareFunc->fromImplDefine)){
				     n_debug("在类中找接口的方法。找到定义:%p %s %s public:%d\n",
				           compareFunc->fromImplDefine,from->sysName,compareFunc->rawMangleName,class_func_is_public(compareFunc));
					 if(compareFunctionType(compareFunc->fromImplDefine,interfaceMethod->fieldDecl)){
						 *atClass=n_strdup(from->sysName);
						 return compareFunc;
					 }
				 }else if(aet_utils_valid_tree(compareFunc->fieldDecl) && !class_func_is_private(compareFunc)){
				     n_debug("在类中找接口的方法。找到域声明:%p %s %s public:%d\n",
				           compareFunc->fieldDecl,from->sysName,compareFunc->rawMangleName,class_func_is_public(compareFunc));
					 if(compareFunctionType(compareFunc->fieldDecl,interfaceMethod->fieldDecl)){
						 *atClass=n_strdup(from->sysName);
						 return compareFunc;
					 }
				 }
			 }
		 }
	 }
	 ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),from);
	 if(info==NULL)
		 return NULL;
	 if(info->parentName.sysName==NULL)
		 return NULL;
	 n_debug("在父类中找接口的方法。praw:%s from:%s parent:%s\n",praw,from->sysName,info->parentName.sysName);
	 return func_mgr_get_interface_impl(self,&info->parentName,interfaceMethod,atClass);
}

/**
 * 在接口在from中是否有实现。
 * findFieldDecl=true 找声明，并且from不是抽象类 只能是父类才能使用。
 */
static nboolean findIfaceImpl(ClassName *from,ClassFunc *compareFunc,
      ClassFunc *ifaceFunc,char **atClass,nboolean findFieldDecl)
{
   if(strcmp(compareFunc->rawMangleName,ifaceFunc->rawMangleName)==0){
      if(aet_utils_valid_tree(compareFunc->fromImplDefine)){
         if(compareFunctionType(compareFunc->fromImplDefine,ifaceFunc->fieldDecl)){
            n_debug("在类中找接口的方法。找到定义:%p %s %s public:%d\n",
                  compareFunc->fromImplDefine,from->sysName,compareFunc->rawMangleName,class_func_is_public(compareFunc));
            *atClass=n_strdup(from->sysName);
            return TRUE;
         }
      }
      if(findFieldDecl){
         ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),from);
         if(!class_info_is_abstract_class(info)
               && aet_utils_valid_tree(compareFunc->fieldDecl)
               && !class_func_is_private(compareFunc)
               && compareFunctionType(compareFunc->fieldDecl,ifaceFunc->fieldDecl)){
            n_debug("在父类中找接口的方法。fieldDecl:%p %s %s public:%d\n",
                  compareFunc->fieldDecl,from->sysName,compareFunc->rawMangleName,class_func_is_public(compareFunc));
               *atClass=n_strdup(from->sysName);
                return TRUE;
          }
      }
   }
   return FALSE;
}

/**
 * 从类from中开始找是否实现了接口方法.
 * from找不到，从父类中找，一直找到AObject
 * 如果父类与from的实现在同一个编译单元，可以找到实现，如果不在，父类的声明
 * 方法中有，并且不是私有的，并且父类不是抽象方法，则认为父类实现了该接口
 */
ClassFunc *getIfaceImpl(FuncMgr *self,ClassName *from,ClassFunc *interfaceMethod,char **atClass,nboolean fromParentClass)
{
   NPtrArray  *srcFuncs=func_mgr_get_funcs(self,from);
   int i;
   if(srcFuncs!=NULL){
      for(i=0;i<srcFuncs->len;i++){
         ClassFunc *compareFunc=(ClassFunc *)n_ptr_array_index(srcFuncs,i);
         if(findIfaceImpl(from,compareFunc,interfaceMethod,atClass,fromParentClass)){
            //在本类中找到接口实现。
            return compareFunc;
         }
      }
   }

   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),from);
   while(info->parentName.sysName){
      srcFuncs=func_mgr_get_funcs(func_mgr_get(),&info->parentName);
      if(srcFuncs!=NULL){
         for(i=0;i<srcFuncs->len;i++){
            ClassFunc *compareFunc=(ClassFunc *)n_ptr_array_index(srcFuncs,i);
            if(findIfaceImpl(&info->parentName,compareFunc,interfaceMethod,atClass,TRUE)){
               //在本类中找到接口实现。
               return compareFunc;
            }
         }
      }
      info=class_mgr_get_class_info_by_class_name(class_mgr_get(),&info->parentName);
   }
   return NULL;
}

ClassFunc *func_mgr_get_interface_impl(FuncMgr *self,ClassName *from,ClassFunc *interfaceMethod,char **atClass)
{
   return getIfaceImpl(self,from,interfaceMethod,atClass,FALSE);
}

ClassFunc *func_mgr_get_interface_impl_from_parent(FuncMgr *self,ClassName *parent,ClassFunc *interfaceMethod,char **atClass)
{
   return getIfaceImpl(self,parent,interfaceMethod,atClass,TRUE);
}

/**
 * 查找类中的静态函数 实现 AHashFunc var=Abc.strHashFunc功能。
* typedef auint (*AHashFunc) (aconstpointer  key);
 * AHashFunc var=Abc.strHashFunc;
 * class Abc{
 *   public$ static auint strHashFunc(aconstpointer key);
 * }
 */
ClassFunc *func_mgr_get_static_method(FuncMgr *self,char *sysName,char *mangle)
{
	if(sysName==NULL || mangle==NULL)
		return NULL;
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,sysName);
	if(array==NULL)
			return NULL;
	int i;
	for(i=0;i<array->len;i++){
		ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
		if(strcmp(item->mangleFunName,mangle)==0){
			return item;
		}
	}
	return NULL;
}

static ClassFunc *getStaticClassFuncByMangleName(FuncMgr *self,char *sysName,char *mangle)
{
   NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,sysName);
   if(array==NULL)
      return NULL;
   int i;
   NPtrArray* data=n_ptr_array_new();
   for(i=0;i<array->len;i++){
      ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
      if(strcmp(item->mangleFunName,mangle)==0){
         return item;
      }
   }
   return NULL;
}

/**
 * 通过mangle函数名获取静态方法所在的类名。
 */
char   *func_mgr_get_static_class_name_by_mangle(FuncMgr *self,char *mangle)
{
	NHashTableIter iter;
	npointer key, value;
	n_hash_table_iter_init(&iter, self->staticHashTable);
	int count=0;
	while (n_hash_table_iter_next(&iter, &key, &value)) {
		char *sysName = (char *)key;
		ClassFunc *func=getStaticClassFuncByMangleName(self,sysName,mangle);
		if(func!=NULL)
			return sysName;
	}
	return NULL;
}

ClassFunc   *func_mgr_get_static_entity_by_mangle(FuncMgr *self,char *mangle)
{
   NHashTableIter iter;
   npointer key, value;
   n_hash_table_iter_init(&iter, self->staticHashTable);
   int count=0;
   while (n_hash_table_iter_next(&iter, &key, &value)) {
      char *sysName = (char *)key;
      ClassFunc *func=getStaticClassFuncByMangleName(self,sysName,mangle);
      if(func!=NULL)
         return func;
   }
   return NULL;
}


int func_mgr_get_max_serial_number(FuncMgr *self,ClassName *className)
{
   NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,className->sysName);
   NPtrArray *staticArray=(NPtrArray *)n_hash_table_lookup(self->staticHashTable,className->sysName);
   int max=-1;
   if(array!=NULL){
      int len=array->len;
      int i;
      for(i=0;i<len;i++){
         ClassFunc *item=n_ptr_array_index(array,i);
         if(item->serialNumber>max)
            max=item->serialNumber;
      }
   }
   if(staticArray!=NULL){
      int len=staticArray->len;
      int i;
      for(i=0;i<len;i++){
         ClassFunc *item=n_ptr_array_index(staticArray,i);
         if(item->serialNumber>max)
            max=item->serialNumber;
      }
   }
   return max;
}

/**
 * 判断是否MTCS函数 只有两类，一种是类中静静态函数，一种是类函数
 */
nboolean    func_mgr_is_mtcs_func(FuncMgr *self,tree fndecl)
{
   if(!fndecl)
      return FALSE;
   if(TREE_CODE(fndecl)!=FUNCTION_DECL && TREE_CODE(fndecl)!=FIELD_DECL)
      return FALSE;
   ClassFunc    *func = func_mgr_get_func_by_mangle(self,IDENTIFIER_POINTER(DECL_NAME(fndecl)));
   if(!func){
      func = func_mgr_get_static_entity_by_mangle(self,IDENTIFIER_POINTER(DECL_NAME(fndecl)));
      if(!func)
         return FALSE;
   }
   return class_func_is_mtcs(func);
}

ClassFunc  *func_mgr_get_func(FuncMgr *self,tree fndecl)
{
   if(!fndecl)
      return FALSE;
   if(TREE_CODE(fndecl)!=FUNCTION_DECL && TREE_CODE(fndecl)!=FIELD_DECL)
      return FALSE;
   ClassFunc    *func = func_mgr_get_func_by_mangle(self,IDENTIFIER_POINTER(DECL_NAME(fndecl)));
   return func;
}

ClassFunc  *func_mgr_get_static_func(FuncMgr *self,tree fndecl)
{
   if(!fndecl)
      return FALSE;
   if(TREE_CODE(fndecl)!=FUNCTION_DECL)
      return FALSE;
   ClassFunc    *func = func_mgr_get_static_entity_by_mangle(self,IDENTIFIER_POINTER(DECL_NAME(fndecl)));
   return func;
}

/**
 * 是否有静态的MTCS函数
 */
static nboolean haveMtcsFunc(ClassName *className,NHashTable *hash)
{
   if(className==NULL)
      return FALSE;
   NPtrArray *array=(NPtrArray *)n_hash_table_lookup(hash,className->sysName);
   if(array==NULL)
      return FALSE;
   int i;
   for(i=0;i<array->len;i++){
      ClassFunc *func=n_ptr_array_index(array,i);
      if(class_func_is_mtcs(func))
         return TRUE;
   }
   return FALSE;
}
/**
 * 类中是否有静态的MTCS函数
 */
nboolean  func_mgr_have_static_mtcs_func(FuncMgr *self,ClassName *className)
{
   return haveMtcsFunc(className,self->staticHashTable);
}

/**
 * 类中是否有MTCS函数
 */
nboolean  func_mgr_have_mtcs_func(FuncMgr *self,ClassName *className)
{
   return haveMtcsFunc(className,self->hashTable);
}

//重要的两个方法，与class声明有关，实现无关。
int func_mgr_get_func_declaration_count(FuncMgr *self,AetFuncType type,ClassName *className)
{
   NPtrArray    *funcArray=func_mgr_get_funcs(func_mgr_get(),className);
   int i;
   int count=0;
   if(funcArray!=NULL){
      for(i=0;i<funcArray->len;i++){
         ClassFunc *func=n_ptr_array_index(funcArray,i);
         if(!aet_utils_valid_tree(func->fieldDecl))
            continue;
         if(type==FUNC_HOST_DEVICE_TYPE && class_func_is_host(func) && class_func_is_device(func))
            count++;
      }
   }

   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   /*本类的接口方法个数*/
   for(i=0;i<info->ifaceCount;i++){
      ClassName *iface=&(info->ifaces[i]);
      funcArray=func_mgr_get_funcs(func_mgr_get(),iface);
      if(funcArray!=NULL){
         int j;
         for(j=0;j<funcArray->len;j++){
            ClassFunc *func=n_ptr_array_index(funcArray,j);
            if(type==FUNC_HOST_DEVICE_TYPE && class_func_is_host(func) && class_func_is_device(func))
              count++;
         }
      }
   }
   return count;
}

int func_mgr_get_func_declaration_index(FuncMgr *self,AetFuncType type,ClassName *className,ClassFunc *need)
{
   NPtrArray    *funcArray=func_mgr_get_funcs(func_mgr_get(),className);
   int i;
   int pos=0;
   if(funcArray!=NULL){
      for(i=0;i<funcArray->len;i++){
         ClassFunc *func=n_ptr_array_index(funcArray,i);
         if(!aet_utils_valid_tree(func->fieldDecl))
            continue;
         if(type==FUNC_HOST_DEVICE_TYPE && class_func_is_host(func) && class_func_is_device(func)){
            if(func==need)
               return pos;
            else
               pos++;
         }
      }
   }

   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   /*本类的接口方法个数*/
   for(i=0;i<info->ifaceCount;i++){
      ClassName *iface=&(info->ifaces[i]);
      funcArray=func_mgr_get_funcs(func_mgr_get(),iface);
      if(funcArray!=NULL){
         int j;
         for(j=0;j<funcArray->len;j++){
            ClassFunc *func=n_ptr_array_index(funcArray,j);
            if(type==FUNC_HOST_DEVICE_TYPE && class_func_is_host(func) && class_func_is_device(func)){
               if(func==need)
                  return pos;
               else
                  pos++;
            }
         }
      }
   }
   return -1;
}

/**
 * 从接口复制方法到它的实现类中，需要创建4个新的名字。
 * 给一个fieldDecl和所属的类，生成4个名字
 */
void func_mgr_create_mangle_name(FuncMgr *self,tree fieldDecl,ClassName *className,char **result)
{
   tree fieldType=TREE_TYPE(fieldDecl);
   tree fieldFunType=TREE_TYPE(fieldType);
   tree fieldReturnType=TREE_TYPE(fieldFunType);
   int count=0;
   nboolean exitsAetGenericInfoParm=FALSE;
   for (tree al = TYPE_ARG_TYPES (fieldFunType); al; al = TREE_CHAIN (al)){
      tree value=TREE_VALUE(al);
      if(count==1){
         exitsAetGenericInfoParm =FALSE;//isAetFgpiParm(value);
         break;
      }
      count++;
   }
   tree funName=DECL_NAME(fieldDecl);
   char *orgiName=n_strdup(IDENTIFIER_POINTER(funName));
   tree argTypes=TYPE_ARG_TYPES (fieldFunType);
   char *newName=aet_mangle_create_skip(self->mangle,funName,argTypes,className->userName,exitsAetGenericInfoParm?2:-1);
   //获取没有self的函数名，用在static函数
   tree noSelfArgTypes = TREE_CHAIN (argTypes);
   char *newNameNoSelf=aet_mangle_create_skip(self->mangle,funName,noSelfArgTypes,className->userName,exitsAetGenericInfoParm?1:-1);
   char *staticNewName=aet_mangle_create_skip(self->mangle,funName,noSelfArgTypes,className->sysName,exitsAetGenericInfoParm?1:-1);
   char *rawMangleName=aet_mangle_create_skip(self->mangle,funName,noSelfArgTypes,"",exitsAetGenericInfoParm?1:-1);
   result[0]=orgiName;
   result[1]=newName;
   result[2]=newNameNoSelf;
   result[3]=rawMangleName;
   result[4]=staticNewName;
}

void func_mgr_add(FuncMgr *self,ClassFunc *func)
{
   if(func->className==NULL){
      n_error("func_mgr_add func无所属类。");
   }
   NPtrArray *array=n_hash_table_lookup(self->hashTable,func->className->sysName);
   if(array==NULL){
      array=n_ptr_array_new();
      n_hash_table_insert(self->hashTable,func->className->sysName,array);
   }
   n_ptr_array_add(array,func);
}

/**
 * 在类或接口中声明 __host__ __device__ setData();
 * 分裂出新的声明 __host__ __device__ setData_device();
 * 处理时作为设备函数对待
 */
tree func_mgr_divide_host_device_func(FuncMgr *self,location_t loc,ClassInfo *info,tree decls)
{
   ClassFunc *func=  func_mgr_get_func(self,decls);
   if(func==NULL)
      return NULL_TREE;
   //这是一个 __host__ __device__ 接口方法，加入新的方法设为设备
   char newName[256];
   mtcs_info_create_host_device_peer_name(newName,func->orgiName);
   tree decl = build_decl (loc,FIELD_DECL,get_identifier(newName),TREE_TYPE(decls));
   printf("func_mgr_divide_host_device_func 00 分裂 host device 方法:%s newName:%s old:%s\n",info->className.sysName,newName,func->orgiName);
   char *result[5];
   func_mgr_create_mangle_name(func_mgr_get(),decl,&info->className,result);
   //printf("func_mgr_divide_host_device_func 11 分裂 host device 方法:%s newName:%s old:%s\n",info->className.sysName,newName,func->orgiName);
   DECL_NAME(decl)=get_identifier(result[1]);
   ClassFunc  *cloneFunc=class_func_clone(func,decl,result,info->record,&info->className);
   class_func_set_divide(cloneFunc,TRUE,func);
   func_mgr_add(self,cloneFunc);
   return decl;
}

ClassFunc  *func_mgr_get_func_by_raw_mangle(FuncMgr *self,char *sysName,char *rawMangle)
{
   if(sysName==NULL || rawMangle==NULL)
      return NULL;
   NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,sysName);
   if(array==NULL)
      return NULL;
   int i;
   for(i=0;i<array->len;i++){
      ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
      if(strcmp(item->rawMangleName,rawMangle)==0){
         return item;
      }
   }
   return NULL;
}

ClassFunc    *func_mgr_get_func_by_raw_mangle(FuncMgr *self,ClassName *className,char *rawMangle)
{
   if(className==NULL || rawMangle==NULL)
      return NULL;
   return func_mgr_get_func_by_raw_mangle(self,className->sysName,rawMangle);
}

//有没有类核函数和静态核函数
static nboolean  isKernelOrDevice(FuncMgr *self,NHashTable *hash,ClassName *className,nboolean matchKernel)
{
   if(className==NULL)
      return FALSE;
   NPtrArray *array=(NPtrArray *)n_hash_table_lookup(hash,className->sysName);
   if(array==NULL)
      return FALSE;
   int i;
   for(i=0;i<array->len;i++){
      ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
      if(matchKernel){
         if(class_func_is_kernel(item))
            return TRUE;
      }else{
         if(class_func_is_device(item))
            return TRUE;
      }
   }
   return FALSE;
}

nboolean      func_mgr_have_kernel_func(FuncMgr *self,ClassName *className)
{
   if(isKernelOrDevice(self,self->hashTable,className,TRUE))
      return TRUE;
   return isKernelOrDevice(self,self->staticHashTable,className,TRUE);
}

//有没有类设备函数和静态设备函数
nboolean      func_mgr_have_device_func(FuncMgr *self,ClassName *className)
{
   if(isKernelOrDevice(self,self->hashTable,className,FALSE))
      return TRUE;
   return isKernelOrDevice(self,self->staticHashTable,className,FALSE);
}

//根据主机函数获取对应的分裂函数
ClassFunc    *func_mgr_get_divide(FuncMgr *self,ClassFunc *host)
{
   NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->hashTable,host->className->sysName);
   int i;
   for(i=0;i<array->len;i++){
        ClassFunc *item=(ClassFunc *)n_ptr_array_index(array,i);
        if(item->isDivide && item->divideSrc==host)
           return item;
   }
   return NULL;
}

FuncMgr *func_mgr_get()
{
	static FuncMgr *singleton = NULL;
	if (!singleton){
		 singleton =n_slice_alloc0 (sizeof(FuncMgr));
		 funcMgrInit(singleton);
	}
	return singleton;
}
