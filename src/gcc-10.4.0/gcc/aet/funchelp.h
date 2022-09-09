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

#ifndef __GCC_FUNC_HELP_H__
#define __GCC_FUNC_HELP_H__

#include "c-aet.h"
#include "classinfo.h"
#include "nlib.h"
#include "classfunc.h"


typedef struct _FuncHelp FuncHelp;
/* --- structures --- */
struct _FuncHelp
{
	c_parser *parser;
};




FuncHelp  *func_help_new();
tree       func_help_create_itself_deref(FuncHelp *self,tree parmOrVar,tree field,location_t loc);
tree       func_help_component_ref_interface(FuncHelp *self,tree orgiComponentRef,tree parmOrVar,
		      ClassName *refVarClassName,ClassName *className,ClassFunc *selectedFunc,location_t loc,tree *firstParm);
tree       func_help_create_parent_iface_deref(FuncHelp *self,tree parmOrVar,ClassName *parentName,ClassName *iface,
		       tree ifaceField,location_t loc,tree *firstParm);
tree       func_help_cast_to_parent(FuncHelp *self,tree parmOrVar,ClassName *parentName);
tree       func_help_create_parent_deref(FuncHelp *self,tree parmOrVar,ClassName *parent,tree parentField,location_t loc,tree *firstParm);
tree       func_help_create_itself_iface_deref(FuncHelp *self,tree parmOrVar,ClassName *className,
		      ClassName *iface,tree ifaceField,location_t loc,tree *firstParm);
SelectedDecl *func_help_select_static_func(FuncHelp *self,ClassName *className,char *orgiName,vec<tree, va_gc> *exprlist,
                     vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc);
int        func_help_process_static_func_param(FuncHelp *self,ClassName *className,char *orgiName,vec<tree, va_gc> *exprlist,nboolean isStaic);


tree       func_help_create_parent_iface_deref_new(FuncHelp *self,tree func,ClassName *parentName,ClassName *iface,
                   tree ifaceField,location_t loc,tree *firstParm);

tree       func_help_create_parent_deref_new(FuncHelp *self,tree func,ClassName *parent,tree parentField,location_t loc,tree *firstParm);

tree       func_help_create_itself_iface_deref_new(FuncHelp *self,tree func,ClassName *className,
                        ClassName *iface,tree ifaceField,location_t loc,tree *firstParm);
tree       func_help_create_itself_deref_new(FuncHelp *self,tree func,tree field,location_t loc,tree *firstParm);



#endif


