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
#include "classfinal.h"
#include "aetinfo.h"
#include "c-aet.h"
#include "classfinal.h"
#include "aetprinttree.h"
#include "classfunc.h"
#include "funcmgr.h"
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
      if(specs!=NULL && (specs->typespec_word!=cts_none
            || specs->storage_class!=csc_none || specs->typespec_kind != ctsk_none)){
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




