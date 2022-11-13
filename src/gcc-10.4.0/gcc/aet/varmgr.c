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
#include "function.h"
#include "tree.h"
#include "timevar.h"
#include "stringpool.h"
#include "cgraph.h"
#include "toplev.h"
#include "attribs.h"
#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "opts.h"

#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"
#include "aetutils.h"
#include "aetprinttree.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "classmgr.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "makefileparm.h"
#include "classutil.h"

static void varMgrInit(VarMgr *self)
{
	  self->mgrHash=n_hash_table_new(n_str_hash,n_str_equal);
	  self->linkFile=NULL;
}

static int getSerialNumber(VarMgr *self,ClassName *className)
{
    int max1=var_mgr_get_max_serial_number(self,className);
    int max2=func_mgr_get_max_serial_number(func_mgr_get(),className);
    return max2>max1?(max2+1):(max1+1);
}

static VarEntity *createEntity(tree decl,char *orgiName,char *mangleVarName,nboolean isStatic,char *sysName)
{
	VarEntity *item=n_slice_new(VarEntity);
    item->decl=decl;
    item->isStatic=isStatic;
    item->isConst=FALSE;
    item->orgiName=n_strdup(orgiName);
    item->mangleVarName=NULL;
    item->init=NULL_TREE;
    item->init_original_type=NULL_TREE;
    item->serialNumber=0;
    item->sysName=n_strdup(sysName);
    if(mangleVarName!=NULL)
        item->mangleVarName=n_strdup(mangleVarName);
	return item;
}

static nboolean existsVar(VarMgr *self,char *orgiName,ClassName *className)
{
   if(!n_hash_table_contains(self->mgrHash,className->sysName)){
	  return FALSE;
   }
   NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->mgrHash,className->sysName);
   int i;
   for(i=0;i<array->len;i++){
	   VarEntity *item=(VarEntity *)n_ptr_array_index(array,i);
       if(strcmp(item->orgiName,orgiName)==0){
	      n_debug("existsVar 已经存在的mangle  %s class:%s",item->orgiName,className->sysName);
	      return TRUE;
       }
   }
   return FALSE;
}

static nboolean addVar(VarMgr *self,ClassName *className,tree decl,char *orgiName,char *mangleVarName,nboolean isStatic)
{
	 if(!n_hash_table_contains(self->mgrHash,className->sysName)){
		VarEntity *item=createEntity(decl,orgiName,mangleVarName,isStatic,className->sysName);
		item->serialNumber=getSerialNumber(self,className);
		NPtrArray *array=n_ptr_array_sized_new(2);
		n_ptr_array_add(array,item);
		n_debug("var_mgr_add 00 第一次加 类名:%s 变量原名:%s mangleName:%s\n",className->sysName,orgiName,mangleVarName);
		n_hash_table_insert (self->mgrHash, n_strdup(className->sysName),array);
	 }else{
		NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->mgrHash,className->sysName);
		VarEntity *item=createEntity(decl,orgiName,mangleVarName,isStatic,className->sysName);
        item->serialNumber=getSerialNumber(self,className);
		n_ptr_array_add(array,item);
		n_debug("var_mgr_add 11 类：%s的条目已建立，现在加新的变量进来:%s %s\n",className->sysName,orgiName,mangleVarName);
	 }
	 return TRUE;
}

nboolean var_mgr_add(VarMgr *self,ClassName *className,tree decl)
{
	char *varName=IDENTIFIER_POINTER(DECL_NAME(decl));
	nboolean exits=existsVar(self,varName,className);
	if(exits){
	   error_at (DECL_SOURCE_LOCATION (decl),"%qD 有同名的变量名 ！", decl);
       return FALSE;
	}
	return addVar(self,className,decl,varName,NULL,FALSE);
}

nboolean var_mgr_set_permission(VarMgr *self,ClassName *className,tree decl,ClassPermissionType permission)
{
	 if(!n_hash_table_contains(self->mgrHash,className->sysName)){
  	    error ("找不到变量无法设置访问权限 %s",className->userName);
	 }else{
		NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->mgrHash,className->sysName);
		int i;
		for(i=0;i<array->len;i++){
		   VarEntity *item=n_ptr_array_index(array,i);
		   if(item->decl==decl){
			   item->permission=permission;
			   break;
		   }
		}
	 }
	 return TRUE;
}

void var_mgr_set_final(VarMgr *self,ClassName *className,tree decl,nboolean isFinal)
{
	 if(!n_hash_table_contains(self->mgrHash,className->sysName)){
  	    error ("找不到变量无法设置final属性 %s。",className->userName);
	 }else{
		NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->mgrHash,className->sysName);
		int i;
		for(i=0;i<array->len;i++){
		   VarEntity *item=n_ptr_array_index(array,i);
		   if(item->decl==decl){
			   //printf("var_mgr_set_permission %s permiss:%d\n",className->sysName,permission);
			   item->isFinal=isFinal;
			   break;
		   }
		}
	 }
}


static void warn_string_init (location_t loc, tree type, tree value,enum tree_code original_code)
{
  if (pedantic  && TREE_CODE (type) == ARRAY_TYPE  && TREE_CODE (value) == STRING_CST  && original_code != STRING_CST)
     warning_at(loc, OPT_Wpedantic,"array initialized from parenthesized string constant");
}

/**
 * 定义全局变量，声明extern 是在引入头文件时就做了
 * gotoStaticAndInit-->parser_static_compile
 */
static void defineGlobalVar(VarEntity *item)
{
	  tree decl;
	  tree org=item->decl;
	  tree init=item->init;
	  tree initOrginalTypes=item->init_original_type;
      tree name=DECL_NAME(org);
      tree type=TREE_TYPE(org);
      if(item->isConst){
    	  printf("varmgr.c defineGlobalVar is const :%s\n",item->orgiName);
    	  type = c_build_qualified_type (type, TYPE_QUAL_CONST);
      }
      decl = build_decl (DECL_SOURCE_LOCATION (org), VAR_DECL, name, type);
      DECL_EXTERNAL(decl)=0;
      TREE_PUBLIC(decl)=1;
      TREE_STATIC(decl)=1;
      decl=class_util_define_var_decl(decl,TRUE);
	  //printf("varmgr.c defineGlobalVar is -----11  :%s %p\n",item->orgiName,init);
//	  aet_print_tree(decl);
//      if(!aet_utils_valid_tree(init)){
//    	  tree type=TREE_TYPE(decl);
//    	  if(TREE_CODE(type)!=ARRAY_TYPE){
//    		  printf("inti ssssss\n");
//    	    init=build_int_cst (integer_type_node,0);
//    	  }
//      }
      location_t init_loc=DECL_SOURCE_LOCATION (org);
      warn_string_init (init_loc, TREE_TYPE (decl), init,item->original_code);
	 // printf("varmgr.c defineGlobalVar is -----22  :%s %p\n",item->orgiName,init);
	  if(aet_utils_valid_tree(init) && TREE_CODE(init)!=IDENTIFIER_NODE){
          finish_decl (decl, init_loc, init,initOrginalTypes, NULL_TREE);
	  }else{
	      finish_decl (decl, init_loc, NULL_TREE,NULL_TREE, NULL_TREE);
	  }
//	  if(!init)
//	      finish_decl (decl, init_loc, NULL_TREE,NULL_TREE, NULL_TREE);
//	  else
//          finish_decl (decl, init_loc, init,initOrginalTypes, NULL_TREE);
      n_debug("类中静态变量全局定义: %s %s context:%p extern:%d static:%d pub:%d\n",
    		  item->mangleVarName,item->orgiName,DECL_CONTEXT(decl),DECL_EXTERNAL(decl),TREE_STATIC(decl),TREE_PUBLIC(decl));
}

/**
 * 类和抽象类静态定义
 */
static void eachDefine(VarMgr *self,NPtrArray *array,char *sysName,NString *codes)
{
	int i;
	ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
	if(info==NULL){
		error("找不到类:%qs",sysName);
		return;
	}
	for(i=0;i<array->len;i++){
		VarEntity *entity=n_ptr_array_index(array,i);
		if(entity->isStatic){
			n_debug("类%s中的静态变量%s定义。i:%d\n",sysName,entity->orgiName,i);
			if(aet_utils_valid_tree(entity->init) && TREE_CODE(entity->init)==IDENTIFIER_NODE){
				//printf("静态变量的创建代码\n");
				n_string_append(codes,IDENTIFIER_POINTER(entity->init));
				n_string_append(codes,"\n");
			}
		    defineGlobalVar(entity);

		}
	}
}

/**
 * 定义全局变量，并赋值。
 * 这是在impl$编译完后做的事
 */
void var_mgr_define_class_static_var(VarMgr *self,ClassName *implClassName,NString *codes)
{
	NHashTableIter iter;
	npointer key, value;
	n_hash_table_iter_init(&iter, self->mgrHash);
	int count=0;
	while (n_hash_table_iter_next(&iter, &key, &value)) {
		NPtrArray *array = (NPtrArray *)value;
		char *sysName=(char *)key;
		if(strcmp(sysName,implClassName->sysName)==0){
			n_debug("定义类%s中的静态变量为全局变量并初始化",sysName);
		    eachDefine(self,array,(char*)key,codes);
		}
	}
	//在这里编译代码报错误：重复的‘static’
//	c_token *t=c_parser_peek_token (self->parser);
//	aet_print_token(t);
//	if(codes->len>0){
//		//要编译与下内容
//		printf("要编译与下内容 %s\n",codes->str);
//	    aet_utils_add_token(parse_in,codes->str,codes->len);
//	}
//	n_string_free(codes,TRUE);
}

//生成新的变量名
//格式_V
nboolean   var_mgr_change_static_decl(VarMgr *self,ClassName *className,struct c_declspecs *specs,struct c_declarator *declarator)
{
	int i;
	struct c_declarator *vardel=NULL;
	struct c_declarator *temp=declarator;
	location_t id_loc=declarator->id_loc;
	for(i=0;i<100;i++){
		if(temp!=NULL){
		   enum c_declarator_kind kind=temp->kind;
		   if(kind==cdk_id){
			   vardel=temp;
			   break;
		   }
		   temp=temp->declarator;
		}else{
			break;
		}
	}
	if(!vardel)
		return FALSE;
	tree declName=vardel->u.id.id;
	//printf("var_mgr_add_static 44 %s\n",IDENTIFIER_POINTER(declName));
	char *varName=n_strdup(IDENTIFIER_POINTER(declName));
	nboolean exits=existsVar(self,varName,className);
	if(exits){
	    error_at (id_loc,"%qD 有同名的变量名 ！", declName);
		n_free(varName);
        return FALSE;
	}
	char  *newVarName=func_mgr_create_static_var_name(func_mgr_get(),className,declName,specs->type);
	nboolean result= addVar(self,className,NULL_TREE,varName,newVarName,TRUE);
    if(result){
      tree value = aet_utils_create_ident (newVarName);
      vardel->u.id.id=value;
      n_debug("var_mgr_change_static_decl 变量名变成了:%s %s\n",varName,newVarName);
    }
	n_free(newVarName);
	n_free(varName);
	return result;
}

static VarEntity *getEntity(NPtrArray *array,char *mangle)
{
   int i;
   for(i=0;i<array->len;i++){
	   VarEntity *item=(VarEntity *)n_ptr_array_index(array,i);
	   if(item->mangleVarName!=NULL && strcmp(item->mangleVarName,mangle)==0){
		   return item;
	   }
   }
   return NULL;
}

nboolean   var_mgr_set_static_decl(VarMgr *self,ClassName *className,tree decl,
		struct c_expr *initExpr,ClassPermissionType permi,nboolean isFinal)
{
	enum tree_code code=TREE_CODE(decl);
	if(code!=VAR_DECL){
        n_warning("不是一个变量声明! %s",get_tree_code_name(TREE_CODE(decl)));
        return FALSE;
	}
	if(className==NULL){
        n_warning("CLASS 没有名字 !");
        return FALSE;
	}
    //函数名的线索来自
	tree funName=DECL_NAME(decl); //变量名
	char *mangleName=IDENTIFIER_POINTER(funName);
	n_debug("var_mgr_set_static_decl 00 静态变量：%s 所在的类:%s",mangleName,className->sysName);
	aet_print_tree(decl);
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->mgrHash,className->sysName);
	if(array==NULL){
		return FALSE;
	}
	VarEntity *item=getEntity(array,mangleName);
	if(item==NULL){
		return FALSE;
	}
	item->decl=decl;
	item->permission=permi==CLASS_PERMISSION_DEFAULT?CLASS_PERMISSION_PROTECTED:permi;
	item->isFinal=isFinal;
	tree type=TREE_TYPE(decl);
	const unsigned int quals = TYPE_QUALS (type);
	if(quals & TYPE_QUAL_CONST){
	    n_debug("这是一个静态 const----- %s\n",item->orgiName);
		item->isConst=TRUE;
	}
	if(initExpr && aet_utils_valid_tree(initExpr->value)){
		item->init=initExpr->value;
		item->init_original_type=initExpr->original_type;
		item->original_code=initExpr->original_code;
	}
	return TRUE;
}


tree  var_mgr_get_static_var(VarMgr *self,ClassName *className,tree component)
{
	if(className==NULL || component==NULL_TREE)
		return NULL_TREE;
	 NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->mgrHash,className->sysName);
	 if(array==NULL){
		return NULL_TREE;
	 }
	 int i;
     for(i=0;i<array->len;i++){
	    VarEntity *item=(VarEntity *)n_ptr_array_index(array,i);
	    char *id=IDENTIFIER_POINTER(component);
	    if(strcmp(item->orgiName,id)==0 && item->isStatic){
		   return item->decl;
	    }
     }
	 return NULL_TREE;
}

VarEntity *var_mgr_get_static_entity(VarMgr *self,ClassName *className,tree component)
{
    if(className==NULL || component==NULL_TREE)
        return NULL;
     NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->mgrHash,className->sysName);
     if(array==NULL){
        return NULL;
     }
     int i;
     for(i=0;i<array->len;i++){
        VarEntity *item=(VarEntity *)n_ptr_array_index(array,i);
        char *id=IDENTIFIER_POINTER(component);
        if(strcmp(item->orgiName,id)==0 && item->isStatic){
           return item;
        }
     }
     return NULL;
}

ClassName *var_mgr_get_class_by_static_component(VarMgr *self,ClassName *srcName,tree component)
{
	if(srcName==NULL || component==NULL_TREE)
		return NULL;
	ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),srcName);
	if(info==NULL)
		return NULL;
	tree var=var_mgr_get_static_var(self,&info->className,component);
	if(var!=NULL_TREE)
		return &info->className;
	return var_mgr_get_class_by_static_component(self,&info->parentName,component);
}



VarEntity *var_mgr_get_static_entity_by_recursion(VarMgr *self,ClassName *srcName,tree component)
{
    if(srcName==NULL || !aet_utils_valid_tree(component))
        return NULL;
    ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),srcName);
    if(info==NULL)
        return NULL;
    VarEntity *var=var_mgr_get_static_entity(self,&info->className,component);
    if(var!=NULL)
        return var;
    return var_mgr_get_static_entity_by_recursion(self,&info->parentName,component);
}



static nboolean isFuncPointer(tree field)
{
   if(TREE_CODE(field)!=FIELD_DECL)
	   return FALSE;
   tree pointerType=TREE_TYPE(field);
   if(TREE_CODE(pointerType)!=POINTER_TYPE)
  	   return FALSE;
   tree typeName=TYPE_NAME(pointerType);
   if(!typeName || typeName==NULL_TREE || typeName==error_mark_node)
	   return FALSE;
   if(TREE_CODE(typeName)!=TYPE_DECL)
      return FALSE;
   tree typeNameType=TREE_TYPE(typeName);
   if(TREE_CODE(typeNameType)!=POINTER_TYPE)
      return FALSE;
   tree functype=TREE_TYPE(pointerType);
   return TREE_CODE(functype)==FUNCTION_TYPE;
}

static tree  findFuncPointer(VarMgr *self,ClassName *className,tree component)
{
	if(className==NULL || component==NULL_TREE)
		return NULL_TREE;
	 NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->mgrHash,className->sysName);
	 if(array==NULL){
		return NULL_TREE;
	 }
	 int i;
     for(i=0;i<array->len;i++){
	    VarEntity *item=(VarEntity *)n_ptr_array_index(array,i);
	    char *id=IDENTIFIER_POINTER(component);
	    if(strcmp(item->orgiName,id)==0){
	    	printf("find findFuncPointer\n");
	    	aet_print_tree(item->decl);
	    	if(isFuncPointer(item->decl))
			   return item->decl;
	    }
     }
	 return NULL_TREE;
}

/**
 * 找函数指针变量
 */
tree  var_mgr_find_func_pointer(VarMgr *self,ClassName *srcName,tree component,char **belongClass)
{
	if(srcName==NULL || component==NULL_TREE)
		return NULL_TREE;
	ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),srcName);
	if(info==NULL)
		return NULL_TREE;
	tree var=findFuncPointer(self,&info->className,component);
	if(var!=NULL_TREE){
		*belongClass=n_strdup(info->className.sysName);
		return var;
	}
	return var_mgr_find_func_pointer(self,&info->parentName,component,belongClass);
}


void  var_mgr_set_parser(VarMgr *self,c_parser *parser)
{
   self->parser=parser;
}


VarEntity *var_mgr_get_var(VarMgr *self,char *sysName,char *varName)
{
    NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->mgrHash,sysName);
    if(array==NULL)
        return NULL;
    int len=array->len;
    int i;
    for(i=0;i<len;i++){
      VarEntity *item=n_ptr_array_index(array,i);
      if(strcmp(varName,item->orgiName)==0)
          return item;
    }
    return NULL;
}


NPtrArray *var_mgr_get_vars(VarMgr *self,char *sysName)
{
	NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->mgrHash,sysName);
	return array;
}

/**
 * 获取变量和方法的序号最大值
 */
int var_mgr_get_max_serial_number(VarMgr *self,ClassName *className)
{
       NPtrArray *array=(NPtrArray *)n_hash_table_lookup(self->mgrHash,className->sysName);
       if(array==NULL){
           return -1;
       }
       int len=array->len;
       int i;
       int max=-1;
       for(i=0;i<len;i++){
           VarEntity *item=n_ptr_array_index(array,i);
           if(item->serialNumber>max)
               max=item->serialNumber;
       }
       return max;
}

VarMgr *var_mgr_get()
{
	static VarMgr *singleton = NULL;
	if (!singleton){
		 singleton =n_slice_alloc0 (sizeof(VarMgr));
		 varMgrInit(singleton);
	}
	return singleton;
}


char      *var_entity_get_sys_name(VarEntity *self)
{
    if(self==NULL)
        return NULL;
    return self->sysName;
}

tree       var_entity_get_tree(VarEntity *self)
{
    if(self==NULL)
           return NULL_TREE;
       return self->decl;
}


