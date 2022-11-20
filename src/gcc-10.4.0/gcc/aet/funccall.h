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

#ifndef __GCC_FUNC_CALL_H__
#define __GCC_FUNC_CALL_H__

#include "c-aet.h"
#include "funchelp.h"
#include "classutil.h"
#include "selectfield.h"


typedef struct _FuncCall FuncCall;
/* --- structures --- */
struct _FuncCall
{
	c_parser *parser;
    FuncHelp *funcHelp;
};




FuncCall      *func_call_new();
FuncAndVarMsg  func_call_get_process_express_method(FuncCall *self,tree id, ClassName *className);
tree           func_call_create_temp_func(FuncCall *self,ClassName *className,tree id);

tree           func_call_deref_select(FuncCall *self,tree func,vec<tree, va_gc> *exprlist,
		         vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,FuncPointerError **errors );

tree           func_call_select(FuncCall *self,tree func,vec<tree, va_gc> *exprlist,
		         vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc ,FuncPointerError **errors);

tree           func_call_super_select(FuncCall *self,tree func,vec<tree, va_gc> *exprlist,
		         vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,FuncPointerError **errors );

tree           func_call_static_select(FuncCall *self,tree func,vec<tree, va_gc> *exprlist,
		         vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,FuncPointerError **errors );

int            fun_call_find_func(FuncCall *self,ClassName *className,tree id);
void           func_call_set_parser(FuncCall *self,c_parser *parser);

#endif


