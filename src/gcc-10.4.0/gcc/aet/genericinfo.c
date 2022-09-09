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
#include "opts.h"

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
#include "genericinfo.h"
#include "genericutil.h"
#include "genericcommon.h"
#include "funcmgr.h"

#define FIELD_FUNC_PREFIX "_inner_generic_func"
#define FIELD_SPERATOR  "!zclei@#_&"

static void genericInfoInit(GenericInfo *self)
{
	self->className=NULL;
	self->genStructDecl=NULL;
	self->blocksCount=0;
	self->undefinesCount=0;
	self->defineObjArray=n_ptr_array_new();
	self->undefineFuncCallCount=0;
	self->defineFuncCallArray=n_ptr_array_new();
}

static tree  getField(GenericInfo *self,char *name)
{
    tree chain;
	tree record=TREE_TYPE(self->genStructDecl);
    tree fieldList=TYPE_FIELDS(record);
	for (chain = fieldList; chain; chain = DECL_CHAIN (chain)){
        tree id=DECL_NAME(chain);
        char *fn=IDENTIFIER_POINTER(id);
        if(strcmp(fn,name)==0)
        	return chain;
	}
	return NULL_TREE;
}

/**
 * 泛型结构体的域函数名与block的名一样
 */
GenericBlock *generic_info_add_block(GenericInfo *self,tree lhs,vec<tree, va_gc> *exprlist,char *body,char *belongFunc,nboolean funcGenBlock)
{
    char *fname=generic_util_create_block_func_name(self->className->sysName,self->blocksCount+1);
    GenericBlock  *gblock=generic_block_new(fname,self->className,belongFunc,funcGenBlock);
    generic_block_set_return_type(gblock,lhs);
    generic_block_set_parm(gblock,exprlist);
    generic_block_set_body(gblock,body);
    generic_block_create_type_decl(gblock,self->blocksCount+1,lhs,exprlist);
    generic_block_create_call(gblock,self->blocksCount+1,lhs,exprlist);
    self->blocks[self->blocksCount++]=gblock;
    if(self->blocksCount>=AET_MAX_GENERIC_BLOCKS){
    	n_error("在一个类中泛型块不能超过:%d,类名:%s 泛型块数:%d",MAX_GEN_BLOCKS,self->className->sysName,self->blocksCount);
    	return NULL;
    }
    n_free(fname);
    return gblock;
}

int  generic_info_get_block_count(GenericInfo *self)
{
	return self->blocksCount;
}


char **generic_info_create_block_source_var_by_index(GenericInfo *self,int index)
{
       GenericBlock *item=self->blocks[index];
	   char *content=generic_block_create_save_codes(item);
	   char *varName=generic_util_create_global_var_for_block_func_name(self->className->sysName,index+1);
	   char **re=(char **)n_new(char,3);
	   re[0]=varName;
	   re[1]=content;
	   re[2]=NULL;
	   return re;
}



nboolean  generic_info_same(GenericInfo *self,ClassName *className)
{
	return strcmp(self->className->sysName,className->sysName)==0;
}


GenericBlock *generic_info_get_block(GenericInfo *self,char *name)
{
	int i;
	for(i=0;i<self->blocksCount;i++){
		GenericBlock *block=self->blocks[i];
		if(strcmp(block->name,name)==0)
			return block;
	}
	return NULL;
}

GenericBlock *generic_info_get_block_by_index(GenericInfo *self,int index)
{
	return self->blocks[index];
}

tree  generic_info_get_field(GenericInfo *self,char *name)
{
	tree field=getField(self,name);
	return field;
}


/**
 * 获取的字符串格式：
 * 类名+FIELD_SPERATOR+块数量+FIELD_SPERATOR+块1+FIELD_SPERATOR+...
 */
char *generic_info_save(GenericInfo *self)
{
    int i;
    printf("generic_info_save ---- %d\n",self->blocksCount);
    if(self->blocksCount==0)
    	return NULL;
    NString *codes=n_string_new("");
	n_string_append_printf(codes,"%s%s%d%s",self->className->sysName,FIELD_SPERATOR,self->blocksCount,FIELD_SPERATOR);
    for(i=0;i<self->blocksCount;i++){
    	GenericBlock *item=self->blocks[i];
    	char *bc=generic_block_create_save_codes(item);
    	n_string_append(codes,bc);
    	if(i<self->blocksCount-1)
        	n_string_append(codes,FIELD_SPERATOR);
    	n_free(bc);
    }
    char *result=n_strdup(codes->str);
    n_string_free(codes,TRUE);
    return result;
}


void  generic_info_free_block_content(BlockContent *bc)
{
	n_free(bc->sysName);
	int i;
    for(i=0;i<bc->blocksCount;i++){
    	BlockDetail *item=bc->content[i];
    	if(item->codes)
    	  n_free(item->codes);
    	if(item->belongFunc)
    	  n_free(item->belongFunc);
    	n_slice_free(BlockDetail,item);
   	    bc->content[i]=NULL;
    }
	n_slice_free(BlockContent,bc);
}

GenericNewClass *generic_info_add_undefine_obj(GenericInfo *self,char *sysName,GenericModel *define)
{
	GenericNewClass *un=generic_new_class_new(sysName,define);
	int i;
	for(i=0;i<self->undefinesCount;i++){
		GenericNewClass *item=self->undefines[i];
		if(generic_new_class_equal(item,un)){
			generic_new_class_free(un);
			return item;
		}
	}
	self->undefines[self->undefinesCount++]=un;
	return un;
}


int   generic_info_get_undefine_obj_count(GenericInfo *self)
{
	return self->undefinesCount;
}


char  **generic_info_create_undefine_source_code(GenericInfo *self)
{
        if(self->undefinesCount==0)
        		return NULL;
  	    char *varName=generic_util_create_undefine_new_object_var_name(self->className->sysName);
		int i;
		NString *codes=n_string_new("");
		for(i=0;i<self->undefinesCount;i++){
			GenericNewClass *item=self->undefines[i];
		   char *id=generic_new_class_get_id(item);
		   n_string_append_printf(codes,"%s",id);
		}
		char *result=n_strdup(codes->str);
		n_string_free(codes,TRUE);
		char **re=(char**)n_new(char,3);
		re[0]=varName;
		re[1]=result;
		re[2]=NULL;
		return re;
}

void  generic_info_add_define_obj(GenericInfo *self,char *sysName,GenericModel *defines)
{
    if(defines==NULL){
    	n_warning("调用generic_info_add_define_obj时 GenericModel *define是空的。sysName:%s",sysName);
    	return;
    }
    GenericNewClass  *item=generic_new_class_new(sysName,defines);
	n_debug("generic_info_add_define_obj --- %s %s\n",in_fnames[0],item->id);
	nboolean find=generic_new_class_exists(self->defineObjArray,item);
	if(!find){
		n_ptr_array_add(self->defineObjArray,item);
	}else{
		generic_new_class_free(item);
	}
}

int   generic_info_get_define_obj_count(GenericInfo *self)
{
	return self->defineObjArray->len;
}

/**
 * outArray保存的是在类实现外的 new$对象
 */
char  **generic_info_create_define_source_code(GenericInfo *self,NPtrArray *outArray)
{
        if(self->defineObjArray->len==0)
        		return NULL;
  	    char *varName=generic_util_create_define_new_object_var_name(self->className->sysName);
		int i;
		NString *codes=n_string_new("");
		for(i=0;i<self->defineObjArray->len;i++){
			GenericNewClass *item=(GenericNewClass *)n_ptr_array_index(self->defineObjArray,i);
			char *id=generic_new_class_get_id(item);
		    n_string_append_printf(codes,"%s",id);
		}
		for(i=0;i<outArray->len;i++){
			GenericNewClass *item=(GenericNewClass *)n_ptr_array_index(outArray,i);
			char *id=generic_new_class_get_id(item);
			n_string_append_printf(codes,"%s",id);
		}
		char *result=n_strdup(codes->str);
		n_string_free(codes,TRUE);
		char **re=(char**)n_new(char,3);
		re[0]=varName;
		re[1]=result;
		re[2]=NULL;
		return re;
}

/**
 * 调用的泛型函数，有未定义的泛型类型
 */
GenericCallFunc *generic_info_add_undefine_func_call(GenericInfo *self,ClassName *className,char *callFuncName,GenericModel *define)
{
	GenericCallFunc *func=generic_call_func_new(className,callFuncName,define);
	self->undefineFuncCall[self->undefineFuncCallCount++]=func;
	return func;
}

int  generic_info_get_undefine_func_call_count(GenericInfo *self)
{
	return self->undefineFuncCallCount;
}

char    **generic_info_create_undefine_func_call_source_code(GenericInfo *self)
{
    if(self->undefineFuncCallCount==0)
    		return NULL;
	 char *varName=generic_util_create_undefine_func_call_var_name(self->className->sysName);
	int i;
	NString *codes=n_string_new("");
	for(i=0;i<self->undefineFuncCallCount;i++){
		GenericCallFunc *item=self->undefineFuncCall[i];
	    n_string_append_printf(codes,"%s\n",item->id);
	}
	char *result=n_strdup(codes->str);
	n_string_free(codes,TRUE);
	char **re=(char**)n_new(char,3);
	re[0]=varName;
	re[1]=result;
	re[2]=NULL;
	return re;
}



void  generic_info_add_define_func_call(GenericInfo *self,ClassName *className,char *callFuncName,GenericModel *defines)
{
    if(defines==NULL){
    	n_warning("调用generic_info_add_define_func_call时 GenericModel *define是空的。sysName:%s",className->sysName);
    	return;
    }
	GenericCallFunc *func=generic_call_func_new(className,callFuncName,defines);
	int i;
	for(i=0;i<self->defineFuncCallArray->len;i++){
		GenericCallFunc *item=n_ptr_array_index(self->defineFuncCallArray,i);
        if(!strcmp(item->sysName,func->sysName) && !strcmp(item->funcName,func->funcName) && generic_model_equal(defines,item->model)){
        	generic_call_func_free(func);
        	return;
        }
	}
	n_ptr_array_add(self->defineFuncCallArray,func);
}

int    generic_info_get_define_func_call_count(GenericInfo *self)
{
	return self->defineFuncCallArray->len;
}

char   **generic_info_create_define_func_call_source_code(GenericInfo *self,NPtrArray *outArray)
{
    if(self->defineFuncCallArray->len==0)
    		return NULL;
	NString *codes=n_string_new("");
	char *varName=generic_util_create_define_func_call_var_name(self->className->sysName);
	int i;
	for(i=0;i<self->defineFuncCallArray->len;i++){
		GenericCallFunc *item=(GenericCallFunc *)n_ptr_array_index(self->defineFuncCallArray,i);
	    n_string_append_printf(codes,"%s\n",item->id);
	}
	for(i=0;i<outArray->len;i++){
		GenericCallFunc *item=(GenericCallFunc *)n_ptr_array_index(outArray,i);
	    n_string_append_printf(codes,"%s\n",item->id);
	}
	char *result=n_strdup(codes->str);
	n_string_free(codes,TRUE);
	char **re=(char**)n_new(char,3);
	re[0]=varName;
	re[1]=result;
	re[2]=NULL;
	return re;
}




GenericInfo  *generic_info_new(ClassName *className)
{
	 GenericInfo *self = n_slice_alloc0 (sizeof(GenericInfo));
	 genericInfoInit(self);
	 self->className=class_name_clone(className);
	 return self;
}



