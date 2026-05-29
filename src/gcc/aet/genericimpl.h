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
#include "aetparser.h"
#include "genericmodel.h"
#include "classfunc.h"


typedef struct _GenericImpl GenericImpl;
/* --- structures --- */
struct _GenericImpl
{
	AetParser *parser;
	struct _modelOfSpecs{
	  struct c_declspecs *specs;
	  GenericModel *genericModel;
	}currentModelOfSpecs;

	struct _castModelOfSpecs{
	  GenericModel *genericModel;
	  tree expr;
	}castCurrentModelSpecs;
};
//new对象或调用泛型函数时，对象或函数的泛型单元来自
typedef enum
{
   UNIT_FROM_NEW_OBJECT,
   UNIT_FROM_GENERIC_FUNC,
   UNIT_FROM_SELF_PARM,
   UNIT_FROM_CHILD,
}GenericUnitFromType;

//保存源代码中的泛型定义或声明，最终的类型是在类中的AET_GENERIC_ARRAY 运行时类型是确定的
typedef struct _RunGenericInfo
{
   GenericUnit *genUnit;
   ClassFunc *fromFunction;
   ClassInfo *fromClass;
   int fromPos;//来自类或函数的泛型位置
   GenericUnitFromType from;//0本身 1 泛型函数 2 self对象 3来自子类
   char *file;//泛型单元声明所在的文件，或是.h或是.c
}RunGenericInfo;


GenericImpl  *generic_impl_get();

nboolean      generic_impl_calc_sizeof(GenericImpl *self,ClassName *implClassName);
tree          generic_impl_create_generic_info_array_field(GenericImpl *self,ClassName *className,int genericCount);

struct c_expr generic_impl_generic_info_expression (GenericImpl *self);

nboolean      generic_impl_is_cast_token(GenericImpl *self,c_token *token);
tree          generic_impl_cast(GenericImpl *self,struct c_type_name *type_name,tree expr);


void          generic_impl_replace_token(GenericImpl *self,c_token *token);
void          generic_impl_cast_by_token(GenericImpl *self,c_token *token);

tree          generic_impl_create_generic_block_array_field(GenericImpl *self);//生成void *_gen_blocks_array_897[AET_MAX_GENERIC_BLOCKS];


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
RunGenericInfo **generic_impl_collect_info(GenericImpl *self,GenericModel *genericDefine,
      ClassFunc *atFunc,ClassInfo *atInfo,int *errorUnitIndex);
RunGenericInfo **generic_impl_collect_parent_info(GenericImpl *self,ClassInfo *child,RunGenericInfo **childInfos);

#endif

