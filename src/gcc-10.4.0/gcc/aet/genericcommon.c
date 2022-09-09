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
#include "attribs.h"

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

#include "tree-pretty-print.h"
#include "tree-dump.h"
#include "langhooks.h"
#include "tree-iterator.h"
#include "c/c-tree.h"


#include "aetutils.h"
#include "classmgr.h"
#include "classinfo.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "aetprinttree.h"
#include "genericimpl.h"
#include "c-aet.h"
#include "aet-c-parser-header.h"
#include "classutil.h"
#include "genericutil.h"
#include "genericcommon.h"
#include "classimpl.h"
#include "funcmgr.h"
#include "classfunc.h"


static void reloveFuncCall(char *content,NPtrArray *array)
{
		char **genStrs=n_strsplit(content,NEW_OBJECT_GENERIC_SPERATOR,-1);
		int genCount=n_strv_length(genStrs);
		if(genCount==0)
			return;
		char *sysName=genStrs[0];
		char *funcName=genStrs[1];
		char *atClass=genStrs[2];
		char *atFuncName=genStrs[3];
		char *gen=genStrs[4];
		char **getModelStr=n_strsplit(gen,",",-1);
		genCount=n_strv_length(getModelStr);
		GenericCallFunc *func=n_slice_new(GenericCallFunc);
		func->sysName=n_strdup(sysName);
		func->funcName=n_strdup(funcName);
		func->atClass=n_strdup(atClass);
		func->atFunc=n_strdup(atFuncName);
		func->id=n_strdup(content);
		int i=0;
		GenericModel *model=generic_model_new_from_file();
		for(i=0;i<genCount;i++){
			char *gen=getModelStr[i];
	        int strLen=strlen(gen);
			if(gen!=NULL && strLen>0){
			   char rr[strlen(gen)-1];
			   memcpy(rr,gen,strlen(gen)-2);
			   rr[strlen(gen)-2]='\0';
			   char p=gen[strlen(gen)-1];
			   int pointer=p-48;
			   generic_model_add(model,rr,pointer);
			}
		}
		func->model=model;
		n_ptr_array_add(array,func);
}


NPtrArray *generic_storage_relove_func_call(char *varName,char *value)
{
	  char **items=n_strsplit(value,"\n",-1);
	  int len=n_strv_length(items);
	  if(len<=0){
		  n_error("获取当前文件所有的new$确定泛型的类时，出现空的 %s %s\n",varName,value);
	       return NULL;
	  }
	  NPtrArray *array=NULL;
	  array=n_ptr_array_new_with_free_func(generic_call_func_free);
      int i;
      for(i=0;i<len;i++){
    	  reloveFuncCall(items[i],array);
      }
      if(array->len==0){
    	  n_ptr_array_unref(array);
    	  return NULL;
      }
      return array;
}

static void reloveNewObject(char *content,NPtrArray *array)
{
		char **genStrs=n_strsplit(content,NEW_OBJECT_GENERIC_SPERATOR,-1);
		int genCount=n_strv_length(genStrs);
		if(genCount==0)
			return;
		char *sysName=genStrs[0];
		char *atClass=genStrs[1];
		char *atFuncName=genStrs[2];
		char *gen=genStrs[3];
		char **getModelStr=n_strsplit(gen,",",-1);
		genCount=n_strv_length(getModelStr);
	    GenericNewClass *obj=n_slice_new(GenericNewClass);
	    obj->sysName=n_strdup(sysName);
	    obj->atClass=n_strdup(atClass);
	    obj->atFunc=n_strdup(atFuncName);
	    obj->id=n_strdup(content);
		int i=0;
		GenericModel *model=generic_model_new_from_file();
		for(i=0;i<genCount;i++){
			char *gen=getModelStr[i];
	        int strLen=strlen(gen);
			if(gen!=NULL && strLen>0){
			   char rr[strlen(gen)-1];
			   memcpy(rr,gen,strlen(gen)-2);
			   rr[strlen(gen)-2]='\0';
			   char p=gen[strlen(gen)-1];
			   int pointer=p-48;
			   generic_model_add(model,rr,pointer);
			}
		}
		obj->model=model;
		n_ptr_array_add(array,obj);
}

NPtrArray *generic_storage_relove_new_object(char *varName,char *value)
{
	  char **items=n_strsplit(value,"\n",-1);
	  int len=n_strv_length(items);
	  if(len<=0){
		  n_error("获取当前文件所有的new$确定泛型的类时，出现空的 %s %s\n",varName,value);
	       return NULL;
	  }
	  NPtrArray *array=NULL;
	  array=n_ptr_array_new_with_free_func(generic_new_class_free);
      int i;
      for(i=0;i<len;i++){
    	  reloveNewObject(items[i],array);
      }
      if(array->len==0){
    	  n_ptr_array_unref(array);
    	  return NULL;
      }
      return array;
}


static void fillAtClassAndFunc(char **atClass,char **atFunc)
{
	c_parser *parser=class_impl_get()->parser;
	char *atClassName="unknown";
	if(parser->isAet){
		ClassImpl  *impl=class_impl_get();
		atClassName=impl->className->sysName;
	}
	*atClass=n_strdup(atClassName);
	tree currentFunc=current_function_decl;
	char *currentFuncName=IDENTIFIER_POINTER(DECL_NAME(currentFunc));
	ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(), atClassName);
	if(className!=NULL){
		ClassFunc *func=func_mgr_get_entity(func_mgr_get(),className, currentFuncName);
		if(class_func_is_func_generic(func)){
			*atFunc=n_strdup(currentFuncName);
		}else{
			*atFunc=n_strdup("unknown");
		}
	}else{
		*atFunc=n_strdup("unknown");
	}
}

GenericCallFunc *generic_call_func_new(ClassName *className,char *funcName,GenericModel *defines)
{
   	GenericCallFunc *func=n_slice_new(GenericCallFunc);
	func->sysName=n_strdup(className->sysName);
	func->funcName=n_strdup(funcName);
	fillAtClassAndFunc(&func->atClass,&func->atFunc);
	func->model=defines;
	char *modelStr=generic_model_tostring(defines);
	NString *ids=n_string_new("");
	n_string_append(ids,func->sysName);
	n_string_append(ids,NEW_OBJECT_GENERIC_SPERATOR);
	n_string_append(ids,func->funcName);
	n_string_append(ids,NEW_OBJECT_GENERIC_SPERATOR);
	n_string_append(ids,func->atClass);
	n_string_append(ids,NEW_OBJECT_GENERIC_SPERATOR);
	n_string_append(ids,func->atFunc);
	n_string_append(ids,NEW_OBJECT_GENERIC_SPERATOR);
	n_string_append(ids,modelStr);
	n_string_append(ids,"\n");
	func->id=n_strdup(ids->str);
	n_string_free(ids,TRUE);
	return func;

}

void  generic_call_func_free(GenericCallFunc *self)
{

}

void generic_new_class_free(GenericNewClass *self)
{

}

GenericNewClass *generic_new_class_new(char *sysName,GenericModel *defines)
{
	GenericNewClass *obj=n_slice_new(GenericNewClass);
	obj->sysName=n_strdup(sysName);
	fillAtClassAndFunc(&obj->atClass,&obj->atFunc);
	obj->model=defines;
	char *modelStr=generic_model_tostring(defines);
	NString *ids=n_string_new("");
	n_string_append(ids,obj->sysName);
	n_string_append(ids,NEW_OBJECT_GENERIC_SPERATOR);
	n_string_append(ids,obj->atClass);
	n_string_append(ids,NEW_OBJECT_GENERIC_SPERATOR);
	n_string_append(ids,obj->atFunc);
	n_string_append(ids,NEW_OBJECT_GENERIC_SPERATOR);
	n_string_append(ids,modelStr);
	n_string_append(ids,"\n");
	obj->id=n_strdup(ids->str);
	n_string_free(ids,TRUE);
	return obj;
}

nboolean generic_new_class_equal(GenericNewClass *self,GenericNewClass *dest)
{
	return strcmp(self->id,dest->id)==0;
}

char  *generic_new_class_get_id(GenericNewClass *self)
{
	return self->id;
}

nboolean   generic_new_class_exists(NPtrArray *array, GenericNewClass *obj)
{
	int i;
	for(i=0;i<array->len;i++){
		GenericNewClass *item=(GenericNewClass *)n_ptr_array_index(array,i);
		if(strcmp(item->id,obj->id)==0){
			return TRUE;
		}
	}
	return FALSE;
}

GenericCM *class_and_method_get_root(GenericCM *self)
{
	GenericCM *root=(GenericCM *)self->parent;
	if(root==NULL)
		return self;
	n_debug("class_and_method_get_root root is 00  %p self type:%d\n",root,self->type);
	int count=0;
	while(root){
	    n_debug("class_and_method_get_root root is 11  %p %p type:%d\n",root,root->parent,root->type);
		if(!root->parent)
			return root;
		root=(GenericCM *)root->parent;
		count++;
		if(count>35)
			n_error("getRootGenericClassxxx");
	}
	return root;
}

void class_and_method_set_parent(GenericCM *self,GenericCM *parent)
{
	if(self->parent!=NULL){
		n_error("已经设过GenericClassAndMethodParent parent");
	}
	self->parent=parent;
}

