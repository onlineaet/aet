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

#ifndef __GCC_CLASS_CTOR_H__
#define __GCC_CLASS_CTOR_H__

#include "nlib.h"
#include "aetmangle.h"
#include "classinfo.h"
#include "funchelp.h"

typedef struct _SuperInfo{
	nboolean sysCreate;
	char *funcName;
	char *sysName;
}SuperInfo;


typedef struct _ClassCtor ClassCtor;
/* --- structures --- */
struct _ClassCtor
{
	c_parser *parser;
	FuncHelp *funcHelp;
	SuperInfo superInfos[30];
	int superInfoCount;
	nboolean superOfSelfParseing; //正在解析self或super$()
	tree superOfSelf[10];
	int  superOfSelfCount;
};


ClassCtor *class_ctor_new();
void       class_ctor_parser_return(ClassCtor *self,nboolean isConstructor);
nboolean   class_ctor_add_return_stmt(ClassCtor *self);
nboolean   class_ctor_parser_constructor(ClassCtor *self,ClassName *className);
nboolean   class_ctor_have_default_field(ClassCtor *self,ClassName *className);
tree       class_ctor_create_default_decl(ClassCtor *self,ClassName *className,tree structType);
void       class_ctor_build_default_define(ClassCtor *self,ClassName *className);
nboolean   class_ctor_have_default_define(ClassCtor *self,ClassName *className);
nboolean   class_ctor_is(ClassCtor *self,struct c_declarator *declarator,ClassName *className);
void       class_ctor_add_super_keyword(ClassCtor *self,ClassName *className);
void       class_ctor_parser_super(ClassCtor *self,ClassName *className);
tree       class_ctor_select(ClassCtor *self,tree func,vec<tree, va_gc> *exprlist,
		         vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc );
tree       class_ctor_select_from_self(ClassCtor *self,tree func,vec<tree, va_gc> *exprlist,
		            vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc );
nboolean   class_ctor_self_is_first(ClassCtor *self,ClassName *className,int *error);
void       class_ctor_end_super_or_self_ctor_call(ClassCtor *self,tree expr);
void       class_ctor_process_self_call(ClassCtor *self,ClassName *className);
void       class_ctor_set_tag_for_self_and_super_call(ClassCtor *self,tree ref);


#endif


