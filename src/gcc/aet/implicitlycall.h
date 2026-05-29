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

#ifndef __GCC_IMPLICITLY_CALL_H__
#define __GCC_IMPLICITLY_CALL_H__

#include "c-aet.h"
#include "classutil.h"
#include "funchelp.h"
#include "aetparser.h"

/**
 * 在impl$中调用implicitly函数。
 */
typedef struct _ImplicitlyCall ImplicitlyCall;
/* --- structures --- */
struct _ImplicitlyCall
{
	AetParser *parser;
	NPtrArray *implicitlyArray;
   FuncHelp *funcHelp;
   NPtrArray *kernelArray;//需要替换核函数的数组,只能用在没有启动参数的隐藏核函数调用
};

//[ɪmˈplɪsətli]
ImplicitlyCall  *implicitly_call_new();
tree             implicitly_call_call(ImplicitlyCall *self,location_t loc,tree id);
tree             implicitly_call_call_skip_builtin(ImplicitlyCall *self,location_t loc,tree id);
tree             implicitly_call_call_from_static(ImplicitlyCall *self,struct c_expr *expr,vec<tree, va_gc> *exprlist,
                              vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,nboolean onlyStatic);
void             implicitly_call_set_call(ImplicitlyCall *self,tree callee,vec<location_t> arg_loc);

void             implicitly_call_link(ImplicitlyCall *self);
nboolean         implicitly_call_set_init_or_modify_impl_conv(ImplicitlyCall *self,tree lhsOrValue,tree rhs,tree rhsOrigType,
                                    char *sysName,nboolean isModify);
nboolean         implicitly_call_set_return_impl_conv(ImplicitlyCall *self,tree lhsOrValue,tree rhs,tree rhsOrigType,char *sysName);
nboolean         implicitly_call_set_funcall_impl_conv(ImplicitlyCall *self,tree funcCall,tree agr,tree argOrigType,int pos,char *sysName);

nboolean         implicitly_call_is_func_decl(ImplicitlyCall *self,tree funcdel);
nboolean         implicitly_call_is_builtin(ImplicitlyCall *self,tree funcdel);
/**
 * 是不是当前类中的标记的隐藏函数
 */
nboolean         implicitly_call_have_func(ImplicitlyCall *self,tree func);

#endif


