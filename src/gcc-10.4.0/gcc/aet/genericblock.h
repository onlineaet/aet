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

#ifndef __GCC_GENERIC_BLOCK_H__
#define __GCC_GENERIC_BLOCK_H__

#include "nlib.h"
#include "c-aet.h"
#include "classinfo.h"




typedef struct _GenericBlock GenericBlock;
/* --- structures --- */
struct _GenericBlock
{
	char *name;
	char *parms;
	char *body;
	char *returnType;
	ClassName *className;
	nboolean isFuncGeneric;
	char *belongFunc;
	location_t startLoc;
	location_t endLoc;
	nboolean isCompile;
	tree funcTypeDecl;
	tree callTree;//等同于(* (setData_1_typedecl)self->_gen_blocks_array_897[0])(self,5);
};


GenericBlock  *generic_block_new(char *name,ClassName *className,char *belongFunc,nboolean isFuncGeneric);
void           generic_block_set_parm(GenericBlock *self,vec<tree, va_gc> *exprlist);
void           generic_block_set_body(GenericBlock *self,char *source);
char          *generic_block_get_name(GenericBlock *self);
void           generic_block_set_loc(GenericBlock *self,location_t start,location_t end);
void           generic_block_print(GenericBlock *self);
char          *generic_block_create_codes(GenericBlock *self);
void           generic_block_set_return_type(GenericBlock *self,tree lhs);
void           generic_block_set_compile(GenericBlock *self,nboolean isCompile);
nboolean       generic_block_is_compile(GenericBlock *self);
char          *generic_block_create_save_codes(GenericBlock *self);
nboolean       generic_block_match_field_and_func(GenericBlock *self,tree funcdel,tree field);
void           generic_block_create_type_decl(GenericBlock *self,int index,tree lhs,vec<tree, va_gc> *exprlist);
void           generic_block_create_call(GenericBlock *self,int index,tree lhs,vec<tree, va_gc> *exprlist);
tree           generic_block_get_call(GenericBlock *self);




#endif

