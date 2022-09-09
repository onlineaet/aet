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
#include "aetutils.h"
#include "classmgr.h"
#include "classfinal.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "classfinal.h"
#include "aetprinttree.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "aet-c-parser-header.h"
#include "varmgr.h"

static void classFinalFinal(ClassFinal *self)
{
	self->isFinal=FALSE;
}

void class_final_parser(ClassFinal *self,ClassParserState state,struct c_declspecs *specs)
{
	  c_parser *parser=self->parser;
      enum rid keyword=c_parser_peek_token (parser)->keyword;
	  location_t  loc = c_parser_peek_token (parser)->location;
	  self->loc=loc;//错误信息时需要用到位置
	  c_parser_consume_token (parser); //
	  if(state!=CLASS_STATE_STOP && state!=CLASS_STATE_FIELD){
		  if(state!=CLASS_STATE_FIELD)
		    error_at(loc,"访问final$关键字只能出现在类的第一个字符！");
		  else
			error_at(loc,"访问final$关键字只能出现在方法或变量的第一个字符！");
		  return;
	 }
	 if(state==CLASS_STATE_FIELD){
	   if(specs!=NULL && (specs->typespec_word!=cts_none || specs->storage_class!=csc_none || specs->typespec_kind != ctsk_none)){
		  error_at(loc,"访问final$关键字只能出现在方法或变量的第一个字符!!！");
		 return;
	   }
	 }
	 self->isFinal=TRUE;
}

static nboolean isFieldFunc(ClassFinal *self,tree field)
{
	//检查是不是field_decl
	if(TREE_CODE(field)!=FIELD_DECL)
		return FALSE;
	char *id=IDENTIFIER_POINTER(DECL_NAME(field));
	int len=IDENTIFIER_LENGTH(DECL_NAME(field));
	if(id==NULL || len<2 || id[0]!='_' || id[1]!='Z')
		return FALSE;
	tree type=TREE_TYPE(field);
	if(TREE_CODE(type)!=POINTER_TYPE)
		return FALSE;
	tree funtype=TREE_TYPE(type);
	if(TREE_CODE(funtype)!=FUNCTION_TYPE)
		return FALSE;
    return TRUE;
}

static nboolean findParentFinalField(ClassFinal *self,ClassName *className,ClassFunc *dest)
{
	ClassInfo *classInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
	if(classInfo->parentName.sysName){
		NPtrArray  *funcs=func_mgr_get_funcs(func_mgr_get(), &classInfo->parentName);
		if(funcs!=NULL){
			int i;
			for(i=0;i<funcs->len;i++){
				ClassFunc *item=(ClassFunc *)n_ptr_array_index(funcs,i);
				if(strcmp(item->rawMangleName,dest->rawMangleName)==0){
					if(class_func_is_final(item)){
						  error_at(self->loc,"类%qs的父类%qs已经声明方法%qs为final$,子类不能继承。",
								  className->userName,classInfo->parentName.userName,item->orgiName);
						  return FALSE;
					}
				}
			}
		}
		return findParentFinalField(self,&classInfo->parentName,dest);
	}
	return TRUE;
}

/**
 * 在类声明中调用，并检查父类有没有final声明的函数
 */
nboolean class_final_set_field(ClassFinal *self,tree decls,ClassName *className)
{
	nboolean ok=FALSE;
	nboolean is=isFieldFunc(self,decls);
	if(!is){
		var_mgr_set_final(var_mgr_get(),className,decls,self->isFinal);
	}else{
		ClassInfo *classInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
		ClassType classType=classInfo->type;
	   char *id=IDENTIFIER_POINTER(DECL_NAME(decls));
	   ClassFunc *entity=func_mgr_get_entity(func_mgr_get(), className,id);
	   if(entity==NULL){
		   error_at(self->loc,"在类:%qs，找不到mangle函数名。",className->userName);
		  goto out;
	   }
	   if(class_func_is_abstract(entity) && self->isFinal){
		   error_at(self->loc,"类:%qs中的抽象方法:%qs不能用final$修饰。",className->userName,entity->orgiName);
		   goto out;
	   }
	   if(findParentFinalField(self,className,entity)){
	      class_func_set_final(entity,self->isFinal);
		  ok=TRUE;
	   }else{
		  ok=FALSE;
  	   }

    }
out:
    class_final_set_final(self,FALSE);
	return ok;
}

nboolean class_final_is_final(ClassFinal *self)
{
	return self->isFinal;
}

void  class_final_set_final(ClassFinal *self,nboolean is)
{
	self->isFinal=is;
}


void class_final_check_and_set(ClassFinal *self,ClassParserState state)
{
	  c_parser *parser=self->parser;
      enum rid keyword=c_parser_peek_token (parser)->keyword;
	  location_t  loc = c_parser_peek_token (parser)->location;
	  self->loc=loc;//错误信息时需要用到位置
	  if(state!=CLASS_STATE_STOP && state!=CLASS_STATE_FIELD){
		  if(state!=CLASS_STATE_FIELD)
		    error_at(loc,"访问final$关键字只能出现在类的第一个字符！");
		  else
			error_at(loc,"访问final$关键字只能出现在方法或变量的第一个字符！");
		  return;
	 }
	 self->isFinal=TRUE;
}

ClassFinal *class_final_new()
{
	ClassFinal *self = n_slice_alloc0 (sizeof(ClassFinal));
	classFinalFinal(self);
	return self;
}




