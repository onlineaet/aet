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


#ifndef __GCC_VAR_CALL_H__
#define __GCC_VAR_CALL_H__

#include "aetmangle.h"
#include "c-aet.h"
#include "classinfo.h"
#include "nlib.h"


typedef struct _VarCall VarCall;
/* --- structures --- */
struct _VarCall
{
	c_parser *parser;
};

typedef struct _VarRefRelation{
    char *refClass;
    char *lowClass;
    char *varName;
    int relationship;
}VarRefRelation;


VarCall      *var_call_new();
FuncAndVarMsg var_call_get_process_var_method(VarCall *self,tree id,ClassName *className);
void          var_call_add_deref(VarCall *self,tree id,ClassName *className);
tree          var_call_pass_one_convert(VarCall *self,location_t component_loc,tree exprValue,tree component,VarRefRelation *item,tree *staticVar);
tree          var_call_dot_visit(VarCall *self,tree exprValue,tree component,location_t component_loc,VarRefRelation *item,tree *staticVar);



#endif



