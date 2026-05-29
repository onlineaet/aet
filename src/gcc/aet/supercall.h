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


#ifndef __GCC_SUPER_CALL_H__
#define __GCC_SUPER_CALL_H__

#include "nlib.h"
#include "aetmangle.h"
#include "c-aet.h"
#include "classinfo.h"
#include "aetparser.h"
#include "classfunc.h"


typedef struct _SuperCall SuperCall;
/* --- structures --- */
struct _SuperCall
{
   AetParser *parser;
   NHashTable *fieldsTable;
   //调用super$->xxx 可能出错，应为funcAddress元素可能是空的。调用点的信息，在对象初始化时检查。
   NHashTable *recordErrorTable;
   NPtrArray *parentDeviceDeclArray;
};

SuperCall *super_call_new();
tree       super_call_parser_at_postfix_expression(SuperCall *self,ClassName *className);
//第三版
tree       super_call_replace_super_call(SuperCall *self,location_t expr_loc,tree exprValue,ClassFunc *func);
char      *super_call_create_func_codes(SuperCall *self,ClassName *className);
//创建函数声明
//void debug_AClass_init_global_super_data_1_debug_AClass(unsigned long *addr,char **names,unsigned long **parentAddr,char ***parentNames)
void       super_call_create_init_func_decl(SuperCall *self,location_t loc,ClassName *className);
/**
 * 变量vardecl的名字是不是 _TFirst_parent__superFuncAddressArray
 */
nboolean   super_call_is_parent_func_addr_var(SuperCall *self,tree vardecl,ClassName *className);
tree       super_call_get_parent_kernel_name_var(SuperCall *self,ClassName *className);

nboolean   super_call_valid_mtcs_parent_super_call_var_name(char *varName);
//获取_TSecond_parent__superFuncAddressArray变量
tree       super_call_get_parent_device_decl(SuperCall *self,char *sysName);

#endif



