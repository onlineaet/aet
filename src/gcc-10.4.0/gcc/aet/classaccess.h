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

#ifndef __GCC_CLASS_ACCESS_H__
#define __GCC_CLASS_ACCESS_H__

#include "nlib.h"
#include "classcast.h"
#include "classctor.h"
#include "classinfo.h"
#include "supercall.h"
#include "varcall.h"
#include "funccall.h"

typedef struct _ClassAccess ClassAccess;
/* --- structures --- */
struct _ClassAccess
{
	c_parser *parser;
    VarCall   *varCall;
    SuperCall *superCall;
    FuncCall  *funcCall;
};


char           *class_access_get_class_name(ClassAccess *self,tree exprValue);
int             class_access_get_last_class_and_var(ClassAccess *self,location_t loc,tree component,tree exprValue,char **result);
void            class_access_set_parser(ClassAccess *self, c_parser *parser,VarCall *varCall,SuperCall *superCall,FuncCall *funcCall);
VarRefRelation *class_access_create_relation_ship(ClassAccess *self,location_t loc,tree component,tree exprValue);
tree            class_access_build_field_ref(ClassAccess *self,location_t loc, tree datum, tree field);
tree            class_access_create_temp_field(ClassAccess *self,char *className, tree component,location_t loc);

#endif
