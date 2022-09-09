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

#ifndef __GCC_CLASS_CAST_H__
#define __GCC_CLASS_CAST_H__

#include "nlib.h"


typedef struct _ClassCast ClassCast;
/* --- structures --- */
struct _ClassCast
{
	c_parser *parser;
};


ClassCast *class_cast_new();
void       class_cast_in_finish_decl(ClassCast *self ,tree decl);
void       class_cast_in_finish_stmt(ClassCast *self ,tree stmt);
tree       class_cast_cast(ClassCast *self,struct c_type_name *type_name,tree expr);
void       class_cast_parm_convert_from_ctor(ClassCast *self,tree func,vec<tree, va_gc> *exprlist);
void       class_cast_parm_convert_from_deref(ClassCast *self,tree func,vec<tree, va_gc> *exprlist);
tree       class_cast_cast_for_return(ClassCast *self,tree interfaceType,tree expr);//转化返回值 并且是对象转接口

#endif



