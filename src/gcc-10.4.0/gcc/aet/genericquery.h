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

#ifndef __GCC_GENERIC_QUERY_H__
#define __GCC_GENERIC_QUERY_H__

#include "nlib.h"
#include "c-aet.h"
#include "classinfo.h"
#include "classfunc.h"


typedef struct _GenericQuery GenericQuery;
/* --- structures --- */
struct _GenericQuery
{
	c_parser *parser;
};


GenericQuery  *generic_query_get();
void           generic_query_set_parser(GenericQuery *self,c_parser *parser);
nboolean       generic_query_is_function_declarator(GenericQuery *self,ClassName *className,struct c_declarator *declarator);
void           generic_query_add_parm_to_declarator(GenericQuery *self,struct c_declarator *declarator);
nboolean       generic_query_have_query_caller(GenericQuery *self,ClassFunc *classFunc,tree callFunc);
GenericModel  *generic_query_get_call_generic(GenericQuery *self,tree callFunc);
nboolean       generic_query_is_generic_class(GenericQuery *self,tree callFunc);
void           generic_query_check_var_and_parm(GenericQuery *self,tree decl,tree init);



#endif


