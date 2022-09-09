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

#ifndef __GCC_GENERIC_EXPAND_H__
#define __GCC_GENERIC_EXPAND_H__

#include "nlib.h"
#include "c-aet.h"
#include "classinfo.h"
#include "classfunc.h"
#include "genericclass.h"
#include "genericcommon.h"
#include "genericdefine.h"
#include "genericparser.h"


typedef struct _GenericExpand GenericExpand;
/* --- structures --- */
struct _GenericExpand
{
	c_parser *parser;
	GenericParser *genericParser;
	nboolean genericNeedFileEnd;
	nboolean ifaceNeedFileEnd;
	GenericDefine *genericDefine;
};


GenericExpand  *generic_expand_get();
void            generic_expand_compile_decl(GenericExpand *self);
void            generic_expand_compile_define(GenericExpand *self);
void            generic_expand_produce_cond_define(GenericExpand *self);
void            generic_expand_cast_by_token(GenericExpand *self,c_token *token);
void            generic_expand_replace(GenericExpand *self,char *genStr);
void            generic_expand_produce_set_block_func_decl(GenericExpand *self,nboolean start_attr_ok);
NPtrArray      *generic_expand_get_ref_block_class_name(GenericExpand *self);

void            generic_expand_add_define_object(GenericExpand *self,ClassName *className,GenericModel * genericDefines);
void            generic_expand_add_define_func_call(GenericExpand *self,ClassName *className,char *funcName,GenericModel * genericDefines);

void            generic_expand_create_fggb_var_and_setblock_func(GenericExpand *self);

void            generic_expand_parser_typeof(GenericExpand *self);

void            generic_expand_add_eof_tag_for_iface(GenericExpand *self);
void            generic_expand_add_eof_tag(GenericExpand *self);

void            generic_expand_set_parser(GenericExpand *self,c_parser *parser);




#endif

