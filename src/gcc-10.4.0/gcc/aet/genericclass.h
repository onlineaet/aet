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

#ifndef __GCC_GENERIC_CLASS_H__
#define __GCC_GENERIC_CLASS_H__

#include "nlib.h"
#include "c-aet.h"
#include "classinfo.h"
#include "classfunc.h"
#include "blockfunc.h"
#include "genericcommon.h"




typedef struct _GenericClass GenericClass;
/* --- structures --- */
struct _GenericClass
{
	GenericCM parent;
	NPtrArray *definesArray;
	NPtrArray *childs;
	NPtrArray *parents;

	char *sysName;
	char *belongSysName;
	NPtrArray *belongDefines;
	nboolean haveAddChilds; //是否加过childs
	nboolean haveAddParent; //是否加过parent

	NPtrArray *childsMethod;

};



GenericClass  *generic_class_new(char *sysName,GenericModel *defines);
nboolean       generic_class_equal(GenericClass *self,char *sysName);
nboolean       generic_class_add_defines(GenericClass *self,GenericModel *defines);
void           generic_class_free(GenericClass *self);



void           generic_class_collect_class(GenericClass *self,NPtrArray *collectArrray);


void  generic_class_create_node(GenericClass *self);
void  generic_class_recursion_node(GenericClass *self);
void  generic_class_recursion_method(GenericClass *self);
void  generic_class_collect_method_class(GenericClass *self,NPtrArray *collectArrray);
void  generic_class_collect_method(GenericClass *self,NPtrArray *collectArrray);
char *generic_class_create_fragment(GenericClass *self,NPtrArray *blockFuncsArray);
void  generic_class_add_defines_array(GenericClass *self,NPtrArray *defines);

void generic_class_collect_method_class99(GenericClass *self,NPtrArray *collectArrray);

nboolean       generic_class_have_define(GenericClass *self,GenericModel *define);
void           generic_class_dye_class(GenericClass *self,char *sysName,NPtrArray *defineArray);
void           generic_class_dye_method(GenericClass *self,char *sysName,char *funcName,NPtrArray *defineArray);


#endif


