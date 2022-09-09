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

#ifndef __GCC_GENERIC_MODEL_H__
#define __GCC_GENERIC_MODEL_H__

#include "nlib.h"
#include "c-aet.h"
#include "aetmicro.h"


typedef struct _GenericUnit GenericUnit;

struct _GenericUnit{
	struct _parentData{
		char *name;
		tree decl;
	}parent;
	char *name;
	int size;
	int pointerCount;
	GenericType genericType;
	tree decl;
	nboolean isDefine;
	char toString[255];
};

typedef enum{
	GEN_FROM_CLASS_DECL,//来自类声明
	GEN_FROM_OBJECT_NEW$,//来自新建对象
	GEN_FROM_DECLARATION_OR_FNDEF,
	GEN_FROM_DECL_SPECS,
	GEN_FROM_CLASS_IMPL,
	GEN_FROM_CALL,
	GEN_FROM_FILE,
	GEN_FROM_C_PARSER,
}GenericFrom;

typedef struct _GenericModel GenericModel;
/* --- structures --- */
struct _GenericModel
{
	int unitCount;
	GenericUnit  *genUnits[5];
	GenericFrom from;
	char toString[255];
};


GenericModel *generic_model_new(nboolean needIdentifierNode,GenericFrom from);
GenericModel *generic_model_new_from_file();
GenericModel *generic_model_new_with_parm(char *typeName,int pointerCount);
int           generic_model_get_count(GenericModel *self);
nboolean      generic_model_equal(GenericModel *self,GenericModel *comp);
nboolean      generic_model_include_decl(GenericModel *self,GenericModel *dest);
nboolean      generic_model_include_decl_by_str(GenericModel *self,char *genDeclStr);
int           generic_model_get_undefine_count(GenericModel *self);
int           generic_model_get_count(GenericModel *self);
char         *generic_model_get_no_include_decl_str(GenericModel *self,GenericModel *dest);
char         *generic_model_get_first_decl_name(GenericModel *self);
void          generic_model_add(GenericModel *self,char *type,int pointer);
void          generic_model_add_unit(GenericModel *self,GenericUnit *unit);
int           generic_model_fill_undefine_str(GenericModel *self,int *strs);
int           generic_model_get_undefine_index(GenericModel *self,char genericName);
int           generic_model_get_index(GenericModel *self,char *genericName);
int           generic_model_get_index_by_unit(GenericModel *self,GenericUnit *unit);
char         *generic_model_define_str(GenericModel *self);
GenericUnit  *generic_model_get(GenericModel *self,int index);
void          generic_model_free(GenericModel *self);
nboolean      generic_model_exits_ident(GenericModel *self,char *genIdent);
nboolean      generic_model_have_query(GenericModel *self);
char         *generic_model_tostring(GenericModel *self);
GenericModel *generic_model_create_default(GenericModel *self);
GenericModel *generic_model_merge(GenericModel *self,GenericModel *other);
nboolean      generic_model_exits_unit(GenericModel *self,GenericUnit *unit);
GenericModel *generic_model_clone(GenericModel *self);
nboolean      generic_model_can_cast(GenericModel *self,GenericModel *dest);

GenericUnit  *generic_unit_new_undefine(char *idName);
GenericUnit  *generic_unit_clone(GenericUnit *unit);
GenericUnit  *generic_unit_new(char *type,int pointer);
nboolean      generic_unit_equal(GenericUnit *self,GenericUnit *dest);
nboolean      generic_unit_is_undefine(GenericUnit *self);
nboolean      generic_unit_is_query(GenericUnit *self);
char         *generic_unit_tostring(GenericUnit *self);

#endif

