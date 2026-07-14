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

#ifndef __GCC_GENERIC_UTIL_H__
#define __GCC_GENERIC_UTIL_H__

#include "nlib.h"
#include "c-aet.h"
#include "classinfo.h"


nboolean  generic_util_is_generic_ident(char *name);
nboolean  generic_util_is_generic_pointer(tree type);
char     *generic_util_get_generic_str(tree type);
int       generic_util_get_generic_type(tree type);
int       generic_util_get_generic_type_name(tree type,char *result);
tree      generic_util_get_generic_type_by_str(const char *genericStr);


tree      generic_convert(location_t location,tree type,tree rhs,nboolean replace);

char     *generic_util_create_block_func_type_decl_name(char *sysName,int index);
char     *generic_util_create_block_func_name(char *sysName,int index);

static   inline nboolean  generic_util_valid_by_str(char *str)
{
      if(str==NULL || strlen(str)!=1 )
        return FALSE;
      char min='A';
      char max='Z';
      char v=str[0];
      return (v>=min && v<=max);
}

static   inline nboolean generic_util_valid_all_by_str(char *str)
{
   if(str==NULL || (strlen(str)!=1 && strlen(str)!=3))
     return FALSE;
   if(strlen(str)==3 && !strcmp(str,"all"))
      return TRUE;
   char min='A';
   char max='Z';
   char v=str[0];
   return (v>=min && v<=max);
}

static   inline nboolean generic_util_valid_all(tree id)
{
   char *str=IDENTIFIER_POINTER(id);
   return generic_util_valid_all_by_str(str);
}

static   inline nboolean generic_util_valid_id(tree id)
{
   char *str=IDENTIFIER_POINTER(id);
   return generic_util_valid_by_str(str);
}

tree      generic_util_create_target(char *codes);
tree      generic_util_create_target_loc(char *codes,location_t loc);

char     *generic_util_get_type_str(tree arg);//类型转成字符串
nboolean  generic_util_is_generic_var_or_parm(tree decl);//判断是不是T abc类型的声明
nboolean  generic_util_get_array_type_and_parm_name(tree arg,char **typeStr,char **parmName,char *oldParmName);
void      generic_util_parameter_declaration ();//解析函数定义中的形参
/**
 * 从3个函数名_com_ai_linear_MatrixOps__inner_generic_func_9_abc123
 * _com_ai_linear_MatrixOps__inner_generic_func_1_typedecl
 * _com_ai_linear_MatrixOps__inner_generic_func_2
 * 取出类名
 */
char   *generic_util_sys_name_from_block_func(char *funcName);

//新加11-10
//新加 11-10
//用类型名和指针数创建块函数的前缀
static inline void   generic_unit_create_block_func_prefix(char *typeName,int pointerCount,char *buffer)
{
     sprintf(buffer,"_%s_%d",typeName,pointerCount);
}

/**
 * str的开始部分是否是genericIdentifier
 * aet_generic_E * atcs
 */
nboolean generic_util_start_with_generic(char *str);
/**
 * 返回字符前的泛型类型
 * str 字符串 例如 "aet_generic_E * atcs"
 * 返回 aet_generic_E
 */
char * generic_util_get_start_with_generic(char *str);

//如果参数是泛型类型，需要改变为新的名字
char *generic_util_create_param_new_name(char *origName);
//_aetGenNewParamPrefix_atcs取出原来的名字
char *generic_util_get_orig_param_name(char *newName);

#endif

