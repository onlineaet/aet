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

#ifndef __GCC_GENERIC_INFO_H__
#define __GCC_GENERIC_INFO_H__

#include "nlib.h"
#include "c-aet.h"
#include "classinfo.h"
#include "genericblock.h"
#include "genericcommon.h"




typedef struct _GenericInfo GenericInfo;
/* --- structures --- */
struct _GenericInfo
{
	c_parser *parser;
	ClassName *className;
	tree genStructDecl;
	GenericBlock *blocks[MAX_GEN_BLOCKS];
	int blocksCount;

	GenericNewClass *undefines[100];
	int undefinesCount;
	NPtrArray *defineObjArray;

	GenericCallFunc *undefineFuncCall[100];
	int undefineFuncCallCount;
	NPtrArray *defineFuncCallArray;



};


GenericInfo     *generic_info_new(ClassName *className);
GenericBlock    *generic_info_add_block(GenericInfo *self,tree lhs,vec<tree, va_gc> *exprlist,char *body,char *belongFunc,nboolean isFuncGeneric);
int              generic_info_get_block_count(GenericInfo *self);

void             generic_info_free_block_content(BlockContent *bc);
GenericBlock    *generic_info_get_block(GenericInfo *self,char *name);
GenericBlock    *generic_info_get_block_by_index(GenericInfo *self,int index);
char           **generic_info_create_block_source_var_by_index(GenericInfo *self,int index);
tree             generic_info_get_field(GenericInfo *self,char *name);

GenericNewClass *generic_info_add_undefine_obj(GenericInfo *self,char *sysClassName,GenericModel *define);
int              generic_info_get_undefine_obj_count(GenericInfo *self);
char           **generic_info_create_undefine_source_code(GenericInfo *self);


void             generic_info_add_define_obj(GenericInfo *self,char *sysClassName,GenericModel *define);
int              generic_info_get_define_obj_count(GenericInfo *self);
char           **generic_info_create_define_source_code(GenericInfo *self,NPtrArray *outArray);

nboolean         generic_info_same(GenericInfo *self,ClassName *className);
char            *generic_info_save(GenericInfo *self);


GenericCallFunc *generic_info_add_undefine_func_call(GenericInfo *self,ClassName *className,char *callFuncName,GenericModel *define);
int              generic_info_get_undefine_func_call_count(GenericInfo *self);
char           **generic_info_create_undefine_func_call_source_code(GenericInfo *self);

void             generic_info_add_define_func_call(GenericInfo *self,ClassName *className,char *callFuncName,GenericModel *define);
int              generic_info_get_define_func_call_count(GenericInfo *self);
char           **generic_info_create_define_func_call_source_code(GenericInfo *self,NPtrArray *outArray);



#endif

