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

#ifndef __GCC_CLASS_IMPL_H__
#define __GCC_CLASS_IMPL_H__

#include "nlib.h"
#include "classctor.h"
#include "classfinalize.h"
#include "classinfo.h"
#include "classinit.h"
#include "classinterface.h"
#include "classref.h"
#include "funccall.h"
#include "supercall.h"
#include "varcall.h"
#include "classdot.h"
#include "genericimpl.h"
#include "genericmodel.h"
#include "classbuild.h"
#include "funccheck.h"
#include "classcast.h"
#include "superdefine.h"
#include "implicitlycall.h"
#include "cmprefopt.h"
#include "aetexpr.h"


typedef struct _ClassImpl ClassImpl;
/* --- structures --- */
struct _ClassImpl
{
	c_parser *parser;
    ClassRef *classRef;
    VarCall *varCall;
    ClassCtor *classCtor;
    SuperCall *superCall;
    FuncCall *funcCall;
	ClassInterface *classInterface;
	ClassFinalize *classFinalize;
	ClassDot   *classDot;
	ClassBuild *classBuild;
	FuncCheck  *funcCheck;
    ClassInit *classInit;
    ClassCast  *classCast;
    SuperDefine  *superDefine;
    AetExpr      *aetExpr;
    ImplicitlyCall *implicitlyCall;
    CmpRefOpt      *cmpRefOpt;
    int nest;
    nboolean isConstructor;
    int readyEnd;
    ClassName *className;
    //解析__OBJECT__
    struct _builtMacroForObject{
    	char *names[30];
    	tree  varDecles[30];
    	int count;
    }objectMacro;
};


ClassImpl    *class_impl_get();
void          class_impl_parser(ClassImpl *self);
nboolean      class_impl_add_self_to_param(ClassImpl *self,nboolean isFuncGeneric);
nboolean      class_impl_add_static_to_declspecs(ClassImpl *self,location_t loc,struct c_declspecs *specs);
struct c_expr class_impl_process_expression(ClassImpl *self,struct c_expr expr,location_t loc,GenericModel *genericDefineModel,tree id,nboolean fun,int *action);
struct c_expr class_impl_replace_func_id(ClassImpl *self,struct c_expr expr,vec<tree, va_gc> *exprlist,
		            vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc);
void           class_impl_set_parser(ClassImpl *self, c_parser *parser);
void           class_impl_end_function(ClassImpl *self,nboolean canChangeFuncName);
nboolean       class_impl_start_function(ClassImpl *self,struct c_declspecs *specs,struct c_declarator *declarator,nboolean isFuncGeneric);
void           class_impl_nest_op (ClassImpl *self,bool add);
void           class_impl_parser_super(ClassImpl *self);
nboolean       class_impl_set_class_or_enum_type(ClassImpl *self,c_token *who);
struct c_expr  class_impl_parser_super_at_postfix_expression(ClassImpl *self,struct c_expr expr);
nboolean       class_impl_next_tokens_is_class_and_dot (ClassImpl *self);
nboolean       class_impl_next_tokens_is_enum_and_dot (ClassImpl *self);
nboolean       class_impl_next_tokens_is_class_dot_enum (ClassImpl *self);
nboolean       class_impl_next_tokens_is_class_dot_enum_dot (ClassImpl *self);
void           class_impl_build_class_dot (ClassImpl *self, location_t loc,struct c_expr *expr);
void           class_impl_build_enum_dot (ClassImpl *self, location_t loc,struct c_expr *expr);
nboolean       class_impl_is_aet_class_component_ref_call(ClassImpl *self,struct c_expr expr);
GenericModel * class_impl_get_func_generic_model(ClassImpl *self,tree id);
void           class_impl_finish_function(ClassImpl *self,tree fndecl);
tree           class_impl_add_return(ClassImpl *self,tree retExpr);//如果是对象变量加入到objectreturn中处理，如果是转化由objectreturn处理并返回

void           class_impl_in_finish_decl(ClassImpl *self ,tree decl);
void           class_impl_in_finish_stmt(ClassImpl *self ,tree stmt);
tree           class_impl_cast(ClassImpl *self,struct c_type_name *type_name,tree expr);

void           class_impl_compile_over(ClassImpl *self);
struct c_expr  class_impl_parser_object(ClassImpl *self);//解析__OBJECT__
tree           class_impl_build_deref(ClassImpl *self,location_t loc,location_t component_loc,tree component,tree exprValue);

/**
 * 检查是不是给函数变量赋值，如果右边是类中的静态函数。需要重新生成新的tree
 * typedef auint (*AHashFunc) (aconstpointer  key);
 * AHashFunc var=Abc.strHashFunc;
 * class Abc{
 *   public$ static auint strHashFunc(aconstpointer key);
 * }
 * 如何知道有多个strHashFunc静态函数时，该选择那一个呢？
 * 用var的参数和strHashFunc生成mangle的函数名，然后再通过funcmgr查找。
 */
tree            class_impl_modify_func_pointer(ClassImpl *self,tree lhs,tree rhs);
/**
 * 是不是Abc.xxxx表达式
 */
nboolean        class_impl_is_class_dot_expression(ClassImpl *self);
ClassName      *class_impl_get_class_name(ClassImpl *self);

struct c_expr   class_impl_nameles_call(ClassImpl *self,struct c_expr expr);
void            class_impl_replace_self_call_at_statement_after_labels(ClassImpl *self);
struct c_expr   class_impl_varof_parser(ClassImpl *self,struct c_expr lhs);//解析关键字varof$
nboolean        class_impl_parser_package_dot_class(ClassImpl *self);//解析com.ai.NLayer

void             class_impl_test_target(tree target);



#endif

