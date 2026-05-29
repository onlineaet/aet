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

#ifndef __GCC_AET_MANGLE_H__
#define __GCC_AET_MANGLE_H__

#include "nlib.h"
#include "classinfo.h"
#include "c-aet.h"

typedef struct _AetMangle AetMangle;
/* --- structures --- */
struct _AetMangle
{
	tree GTY ((skip)) entity;
	vec<tree, va_gc> *substitutions;
	NString *strBuf;
	int parm_depth;
};

AetMangle *aet_mangle_new();
int        aet_mangle_get_orgi_func_and_class_name(AetMangle *self,char *newName,char *className,char *funcName);
char      *aet_mangle_create_static_var_name(AetMangle *self,ClassName *className,tree varName,tree type);
char      *aet_mangle_create_parm_string(AetMangle *self,tree funcType);
char      *aet_mangle_create(AetMangle *self,tree funName,tree argTypes,char *className);
char      *aet_mangle_create_skip(AetMangle *self,tree funName,tree argTypes,char *className,int skip);

#endif

