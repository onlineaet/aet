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


#ifndef __GCC_GENERIC_IMPL_H__
#define __GCC_GENERIC_IMPL_H__

#include "nlib.h"
#include "c-aet.h"
#include "classinfo.h"


typedef struct _GenericImpl GenericImpl;
/* --- structures --- */
struct _GenericImpl
{
	c_parser *parser;
	struct _modelOfSpecs{
	  struct c_declspecs *specs;
	  GenericModel *genericModel;
	}currentModelOfSpecs;

	struct _castModelOfSpecs{
	  GenericModel *genericModel;
	  tree expr;
	}castCurrentModelSpecs;
};


GenericImpl  *generic_impl_get();

nboolean      generic_impl_calc_sizeof(GenericImpl *self,ClassName *implClassName);
tree          generic_impl_create_generic_info_array_field(GenericImpl *self,ClassName *className,int genericCount);

struct c_expr generic_impl_generic_info_expression (GenericImpl *self);

nboolean      generic_impl_is_cast_token(GenericImpl *self,c_token *token);
tree          generic_impl_cast(GenericImpl *self,struct c_type_name *type_name,tree expr);


void          generic_impl_replace_token(GenericImpl *self,c_token *token);
void          generic_impl_cast_by_token(GenericImpl *self,c_token *token);



void          generic_impl_add_about_generic_field_to_class(GenericImpl *self);//加域_createAllGenericFunc567
void          generic_impl_add_about_generic_block_info_list(GenericImpl *self);


tree          generic_impl_create_generic_block_array_field(GenericImpl *self);//生成void *_gen_blocks_array_897[AET_MAX_GENERIC_BLOCKS];

void          generic_impl_set_parser(GenericImpl *self,c_parser *parser);

/////////////////////////////////////////GenericModel
nboolean      generic_impl_check_var(GenericImpl *self,tree decl,GenericModel *varGen);
void          generic_impl_check_and_set_func(GenericImpl *self,tree fndecl,GenericModel *genModel,GenericModel *funcGenModel);
void          generic_impl_check_func_at_field(GenericImpl *self,tree decl,struct c_declarator *declarator);
nboolean      generic_impl_check_func_at_call(GenericImpl *self,char *funcName,GenericModel *funcgen);

GenericModel *generic_impl_pop_generic_from_c_parm(GenericImpl *self,struct c_parm *parm);
void          generic_impl_push_generic_from_c_parm(GenericImpl *self,struct c_declspecs *specs);
void          generic_impl_push_generic_from_declspecs(GenericImpl *self,struct c_declspecs *specs);
GenericModel *generic_impl_pop_generic_from_declspecs(GenericImpl *self,struct c_declspecs *specs);


void          generic_impl_check_var_and_parm(GenericImpl *self,tree decl,tree init);
void          generic_impl_ready_check_cast(GenericImpl *self,struct c_type_name *type_name,tree expr);
GenericModel *generic_impl_get_cast_model(GenericImpl *self,tree expr);



#endif

