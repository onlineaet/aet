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
#include "classcast.h"
#include "implicitlycall.h"
#include "cmprefopt.h"
#include "aetexpr.h"
#include "selectfield.h"
#include "aetparser.h"


typedef struct _ClassImpl ClassImpl;
/* --- structures --- */
struct _ClassImpl
{
   AetParser *parser;
   ClassRef *classRef;
   VarCall *varCall;
   ClassCtor *classCtor;
   SuperCall *superCall;
   FuncCall *funcCall;
   ClassInterface *classInterface;
   ClassFinalize *classFinalize;
   ClassDot   *classDot;
   ClassBuild *classBuild;
   ClassInit *classInit;
   ClassCast  *classCast;
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
   //记录编译时间
   struct{
      nuint64 start;
      nuint64 end;
   }compileTime;

   location_t implEndLoc;//impl{};中的分号位置
   int semicolonCount;//有几个分号
   GenericModel *currentFuncModel;//当前泛型函数的泛型模型
};

ClassImpl    *class_impl_get();
void          class_impl_parser(ClassImpl *self);
nboolean      class_impl_add_self_to_param(ClassImpl *self);
nboolean      class_impl_add_static_to_declspecs(ClassImpl *self,location_t loc,struct c_declspecs *specs);
struct c_expr class_impl_process_expression(ClassImpl *self,struct c_expr expr,location_t loc,
                     GenericModel *genericDefineModel,tree id,nboolean fun,int *action);
struct c_expr class_impl_replace_func_id(ClassImpl *self,struct c_expr expr,vec<tree, va_gc> *exprlist,
		            vec<tree, va_gc> *origtypes,vec<location_t> arg_loc,location_t expr_loc,SelectFunc *selectFunc);
tree          class_impl_build_function_call_vec(ClassImpl *self,location_t loc, vec<location_t> arg_loc,
                          tree function, vec<tree, va_gc> *params, vec<tree, va_gc> *origtypes,
                          SelectFunc *selectFunc,GenericModel **defineGenModel);
void           class_impl_end_function(ClassImpl *self,nboolean canChangeFuncName);
nboolean       class_impl_start_function(ClassImpl *self,struct c_declspecs *specs,struct c_declarator *declarator,
                    nboolean isFuncGeneric,location_t loc,nboolean havePermission,ClassPermissionType permission);
void           class_impl_nest_op (ClassImpl *self,bool add);
int            class_impl_parser_super(ClassImpl *self);
struct c_expr  class_impl_parser_super_at_postfix_expression(ClassImpl *self,struct c_expr expr);
nboolean       class_impl_next_tokens_is_class_and_dot (ClassImpl *self);
nboolean       class_impl_next_tokens_is_enum_and_dot (ClassImpl *self);
nboolean       class_impl_next_tokens_is_class_dot_enum (ClassImpl *self);
nboolean       class_impl_next_tokens_is_class_dot_enum_dot (ClassImpl *self);
void           class_impl_build_class_dot (ClassImpl *self, location_t loc,struct c_expr *expr);
void           class_impl_build_enum_dot (ClassImpl *self, location_t loc,struct c_expr *expr);
nboolean       class_impl_is_aet_class_component_ref_call(ClassImpl *self,struct c_expr expr);
GenericModel * class_impl_get_func_generic_model(ClassImpl *self,tree id);
void           class_impl_finish_function(ClassImpl *self,tree fndecl,location_t endLoc);
//如果是对象变量加入到objectreturn中处理，如果是转化由objectreturn处理并返回
tree           class_impl_add_return(ClassImpl *self,location_t loc,tree retExpr,tree exprOrigType);

void           class_impl_in_finish_decl(ClassImpl *self ,location_t loc,tree decl);
void           class_impl_in_finish_stmt(ClassImpl *self ,location_t loc,tree stmt);

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
tree            class_impl_modify_or_init_func_pointer(ClassImpl *self,location_t loc,tree lhs,
                  tree rhs,tree rhsOrigType,nboolean isModify);
/**
 * 是不是Abc.xxxx表达式
 */
nboolean        class_impl_is_class_dot_expression(ClassImpl *self);
ClassName      *class_impl_get_class_name(ClassImpl *self);

struct c_expr   class_impl_nameless_call(ClassImpl *self,struct c_expr expr);
nboolean        class_impl_replace_self_call_at_statement_after_labels(ClassImpl *self);
struct c_expr   class_impl_varof_parser(ClassImpl *self,struct c_expr lhs);//解析关键字varof$
nboolean        class_impl_parser_package_dot_class(ClassImpl *self);//解析com.ai.NLayer

void            class_impl_add_implicitly_actual_param(ClassImpl *self,tree funcCall,
                             vec<tree, va_gc> *exprlist,  vec<tree, va_gc> *origtypes);
/**
 * 参数 userCallFuncDecl 源程序中调用的函数声明，如 self->setData setData是一个函数的声明名称。
 * 参数 call 用函数声明setData生成的call语句。setData有可能会变成类中混淆过的函数名
 */
tree            class_impl_record_mtcs_call(ClassImpl *self,location_t loc,tree userCallFuncDecl,
                           tree call,ClassFunc *classFunc,int refObjectMethod,vec<tree, va_gc> *exprlist);

GenericModel   *class_impl_get_func_generic_mode(ClassImpl *self);

void            class_impl_test_target(tree target);
/**
 * 原型 push_parm_decl c-tree.h c-decl.cc
 * 原 push_parm_decl 没有返回decl
 */
tree class_impl_push_parm_decl(ClassImpl *self,const struct c_parm *parm, tree *expr);
/**
 * 是否需要必变类中的函数定义为全局函数
 */
nboolean class_impl_can_global(ClassImpl *self,tree fndecl);

#endif

