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

#ifndef GCC_CLASS_UTIL_H
#define GCC_CLASS_UTIL_H

#include "nlib.h"
#include "c-aet.h"

/**
 * 实参与形参比较后返回三种状态
  */
#define COMPARE_ARGUMENT_ORIG_PARM_SKIP   0
#define COMPARE_ARGUMENT_ORIG_PARM_ERROR -1
#define COMPARE_ARGUMENT_ORIG_PARM_OK     1


nboolean             class_util_add_self(struct c_expr expr,vec<tree, va_gc> **exprlist,vec<tree, va_gc> **porigtypes,vec<location_t> *arg_loc);
char                *class_util_get_class_name_by_record(tree record);
char                *class_util_get_class_name_by_pointer(tree pointerType);
char                *class_util_get_class_name(tree type);
CreateClassMethod    class_util_get_create_method(tree selfexpr);
nboolean             class_util_is_class(struct c_declspecs *declspecs);
CreateClassMethod    class_util_get_new_object_method(char *var);
char                *class_util_create_new_object_temp_var_name(char *sysClassName,CreateClassMethod method);
char                *class_util_get_class_name_from_expr(tree exprValue);
char                *class_util_get_option();
int                  class_util_get_pointers(tree type);
nboolean             class_util_is_void_pointer(tree type);
nboolean             class_util_is_function(struct c_declarator *declarator);
struct c_declarator *class_util_get_function_declarator(struct c_declarator *declarator);
tree                 class_util_create_null_tree();
nint                 class_util_get_random_number();
char                *class_util_get_o_file();
int                  class_util_check_param_type(tree orgi,tree realType);
int                  class_util_get_type_name(tree type,char **result);
tree                 class_util_define_var_decl(tree decl,nboolean initialized);
location_t           class_util_get_declspecs_first_location(struct c_declspecs *specs);
/**
 * 创建父类变量名
 */
char                 *class_util_create_parent_class_var_name(char *userName);
int                   class_util_has_nameless_call(tree value);
/**
 * 利用函数第一个参数是self可知该函数所属的类
 */
char                 *class_util_get_class_name_from_field_decl(tree fieldDecl);
int                   class_util_get_nameless_call_link(tree value,NPtrArray *array,char **link);
nboolean              class_util_is_function_field(tree field);

#endif /* ! GCC_C_AET_H */
