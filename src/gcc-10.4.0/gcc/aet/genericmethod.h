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

#ifndef __GCC_GENERIC_METHOD_H__
#define __GCC_GENERIC_METHOD_H__

#include "nlib.h"
#include "c-aet.h"
#include "classinfo.h"
#include "classfunc.h"
#include "blockfunc.h"
#include "genericcommon.h"




typedef struct _GenericMethod GenericMethod;
/* --- structures --- */
struct _GenericMethod
{
	GenericCM parent;
	char *sysName;
	char *funcName;
	NPtrArray *definesArray;
	NPtrArray *childs;


	nboolean haveAddChilds; //是否加过childs

	NPtrArray *objectChilds;

};




GenericMethod *generic_method_new(char *sysName,char *funcName,GenericModel *defines);
nboolean       generic_method_equal(GenericMethod *self,char *sysName,char *funcName);
nboolean       generic_method_add_defines(GenericMethod *self,GenericModel *defines);
void           generic_method_add_defines_array(GenericMethod *self,NPtrArray *defines);
void           generic_method_free(GenericMethod *self);
void           generic_method_collect_class(GenericMethod *self,NPtrArray *collectClass);
void           generic_method_collect(GenericMethod *self,NPtrArray *collectArrray);
void           generic_method_create_node(GenericMethod *self);
void           generic_method_recursion_node(GenericMethod *self);
void           generic_method_recursion_class(GenericMethod *self);
nboolean       generic_method_have_define(GenericMethod *self,GenericModel *define);

void           generic_method_dye_class(GenericMethod *self,char *sysName,NPtrArray *defineArray);
void           generic_method_dye_method(GenericMethod *self,char *sysName,char *funcName,NPtrArray *defineArray);

#endif


