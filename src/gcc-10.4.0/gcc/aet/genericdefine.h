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

#ifndef __GCC_GENERIC_DEFINE_H__
#define __GCC_GENERIC_DEFINE_H__

#include "nlib.h"
#include "c-aet.h"
#include "classinfo.h"
#include "classfunc.h"
#include "genericclass.h"
#include "genericcommon.h"
#include "genericparser.h"
#include "genericmethod.h"


typedef struct _GenericDefine GenericDefine;
/* --- structures --- */
struct _GenericDefine
{
	NPtrArray     *newObjectArray;
	NPtrArray     *genericClassArray;
	NPtrArray      *funcCallArray;
	NPtrArray      *genericMethodArray;
    SetBlockHelp  *help;
};


GenericDefine  *generic_define_new();
void            generic_define_add_define_object(GenericDefine *self,ClassName *className,GenericModel * genericDefines);
SetBlockHelp   *generic_define_get_help(GenericDefine *self);
char           *generic_define_create_generic_class(GenericDefine *self);
void            generic_define_add_define_func_call(GenericDefine *self,ClassName *className,char *funcName,GenericModel * defines);




#endif

