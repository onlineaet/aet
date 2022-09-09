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

#define NEW_OBJECT_GENERIC_SPERATOR "$WWtx$"


nboolean  generic_util_is_generic_ident(char *name);
nboolean  generic_util_is_generic_pointer(tree type);
char     *generic_util_get_generic_str(tree type);
int       generic_util_get_generic_type(tree type);
int       generic_util_get_generic_type_name(tree type,char *result);
nboolean  generic_util_valid_all(tree id);
nboolean  generic_util_valid_all_by_str(char *str);
nboolean  generic_util_valid_id(tree id);
nboolean  generic_util_valid_by_str(char *str);
tree      generic_util_get_generic_type_by_str(char *genericStr);
tree      generic_util_get_fggb_record_type();
nboolean  generic_util_is_generic_func(tree decl);
nboolean  generic_util_is_query_generic_func(tree decl);


tree      generic_conv_convert_argument (location_t ploc, tree fundecl,tree type, tree val,nboolean npc, nboolean excess_precision,nboolean replace);

char     *generic_util_create_block_func_type_decl_name(char *sysName,int index);
char     *generic_util_create_block_func_define_name(char *sysName,int index);
char     *generic_util_create_block_func_name(char *sysName,int index);
char     *generic_util_create_undefine_new_object_var_name(char *sysName);
char     *generic_util_create_define_new_object_var_name(char *sysName);
char     *generic_util_create_func_generic_call_var_name(char *sysName);
char     *generic_util_create_global_var_for_block_func_name(char *sysName,int index);

static   inline char *generic_util_create_class_key_in_file(char *sysName)
{
	char *name=n_strdup_printf("_%s_in_file",sysName);
	return name;
}

char     *generic_util_create_undefine_func_call_var_name(char *sysName);
char     *generic_util_create_define_func_call_var_name(char *sysName);

tree      generic_util_create_target(char *codes);

char     *generic_util_get_type_str(tree arg);//类型转成字符串
nboolean  generic_util_is_generic_var_or_parm(tree decl);//判断是不是T abc类型的声明
nboolean  generic_util_get_array_type_and_parm_name(tree arg,char **typeStr,char **parmName,char *oldParmName);


#endif

