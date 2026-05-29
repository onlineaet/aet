/*
 * Copyright (C) 2022 , guiyang,wangyong co.,ltd.

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

#ifndef __GCC_MTCS_BUILTIN_TREE_H__
#define __GCC_MTCS_BUILTIN_TREE_H__

#include "nlib.h"
#include "aetparser.h"

//建内建变量和函数数据，并提供访问这些数据的方法。
#define MTCS_BUILTIN_VAR_COUNT 10

typedef struct _MtcsBuiltinTree MtcsBuiltinTree;
/* --- structures --- */
struct _MtcsBuiltinTree
{
    AetParser *parser;
    struct{
       char *name;
       int   region;
       int   elements;
    }builtinVars[MTCS_BUILTIN_VAR_COUNT];
    nboolean createBuiltinFuncs;
    nboolean createInternalFuncs;
    GTY(()) tree internalFunc[20];
};

MtcsBuiltinTree *mtcs_builtin_tree_new();
tree             mtcs_builtin_tree_parser(MtcsBuiltinTree *self,location_t loc,tree id);
char            *mtcs_builtin_tree_create_builtins_decl(MtcsBuiltinTree *self);
void             mtcs_builtin_tree_set_builtins_code(MtcsBuiltinTree *self);
tree             mtcs_builtin_tree_create_shared_fndecl(MtcsBuiltinTree *self);

#endif


