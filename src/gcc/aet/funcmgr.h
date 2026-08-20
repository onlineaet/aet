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

#ifndef __GCC_FUNC_MGR_H__
#define __GCC_FUNC_MGR_H__

#include "nlib.h"
#include "classfunc.h"
#include "classinfo.h"
#include "aetmangle.h"

typedef struct _FuncMgr FuncMgr;
/* --- structures --- */
struct _FuncMgr
{
	AetMangle *mangle;
	NHashTable *hashTable;
	NHashTable *staticHashTable;
};

typedef enum{
   FUNC_HOST_TYPE,
   FUNC_KERNEL_TYPE,
   FUNC_DEVICE_TYPE,
   FUNC_HOST_DEVICE_TYPE,
}AetFuncType;

FuncMgr      *func_mgr_get();
NPtrArray    *func_mgr_get_funcs(FuncMgr *self,ClassName *className);
ClassFunc    *func_mgr_get_entity(FuncMgr *self,ClassName *className,char *mangle);
ClassFunc    *func_mgr_get_entity_by_sys_name(FuncMgr *self,char *sysName,char *mangle);
ClassFunc    *func_mgr_get_func_by_mangle(FuncMgr *self,char *mangle);
ClassFunc    *func_mgr_get_func(FuncMgr *self,tree fndecl);
//返回mangle函数名定义或声明所在的类名
char         *func_mgr_get_class_name_by_mangle(FuncMgr *self,char *mangle);
ClassFunc    *func_mgr_get_func_by_raw_mangle(FuncMgr *self,char *sysName,char *rawMangle);
ClassFunc    *func_mgr_get_func_by_raw_mangle(FuncMgr *self,ClassName *className,char *rawMangle);

ClassFunc    *func_mgr_change_class_func_decl(FuncMgr *self,struct c_declarator *declarator,
                           ClassName *className,tree structTree,int *errInfo);
ClassFunc    *func_mgr_start_func_define(FuncMgr *self,struct c_declspecs *specs,
		                struct c_declarator *declarator,ClassName *className,int *errInfo);
nboolean      func_mgr_end_func_define(FuncMgr *self,tree decl,ClassName *className);
nboolean      func_mgr_func_exits(FuncMgr *self,ClassName *className,char *origName);
char         *func_mgr_get_mangle_func_name(FuncMgr *self,ClassName *className,char *origName);
NPtrArray    *func_mgr_get_constructors(FuncMgr *self,ClassName *className);
int           func_mgr_get_orig_func_and_class_name(FuncMgr *self,char *mangleName,char *className,char *funcName);

nboolean      func_mgr_set_static_func_premission(FuncMgr *self,ClassName *className,
                              tree funDecl,ClassPermissionType permission,nboolean isFinal);
nboolean      func_mgr_change_static_func_decl(FuncMgr *self,struct c_declarator *declarator,ClassName *className,tree structTree);
nboolean      func_mgr_set_static_func_decl(FuncMgr *self,tree decl,ClassName *className,nboolean define);
char         *func_mgr_create_static_var_name(FuncMgr *self,ClassName *className,tree varName,tree type);
nboolean      func_mgr_static_func_exits(FuncMgr *self,ClassName *className,char *origName);
NPtrArray    *func_mgr_get_static_funcs(FuncMgr *self,ClassName *className);
NPtrArray    *func_mgr_get_static_funcs_by_sys_name(FuncMgr *self,char *sysName);
nboolean      func_mgr_static_func_exits_by_recursion(FuncMgr *self,ClassName *srcName,tree component);
ClassFunc    *func_mgr_get_static_method(FuncMgr *self,char *sysName,char *mangle);
char         *func_mgr_get_static_class_name_by_mangle(FuncMgr *self,char *mangle);
ClassFunc    *func_mgr_get_static_entity_by_mangle(FuncMgr *self,char *mangle);
ClassFunc    *func_mgr_get_static_func(FuncMgr *self,tree fndecl);

char         *func_mgr_create_parm_string(FuncMgr *self,tree funcType);

nboolean      func_mgr_have_generic_func(FuncMgr *self,ClassName *className);
nboolean      func_mgr_is_generic_func(FuncMgr *self,ClassName *className,char *mangleFuncName);
//获得接口的实现类和方法
ClassFunc    *func_mgr_get_interface_impl(FuncMgr *self,ClassName *from,ClassFunc *interfaceMethod,char **atClass);
ClassFunc    *func_mgr_get_interface_impl_from_parent(FuncMgr *self,ClassName *parent,ClassFunc *interfaceMethod,char **atClass);
int           func_mgr_get_max_serial_number(FuncMgr *self,ClassName *className);
nboolean      func_mgr_is_mtcs_func(FuncMgr *self,tree fndecl);
nboolean      func_mgr_have_static_mtcs_func(FuncMgr *self,ClassName *className);
nboolean      func_mgr_have_mtcs_func(FuncMgr *self,ClassName *className);
//有没有类核函数和静态核函数
nboolean      func_mgr_have_kernel_func(FuncMgr *self,ClassName *className);
//有没有类设备函数和静态设备函数
nboolean      func_mgr_have_device_func(FuncMgr *self,ClassName *className);
//重要的两个方法，与class声明有关，实现无关。
int           func_mgr_get_func_index(FuncMgr *self,AetFuncType type,ClassFunc *need);
//重要的两个方法，与class声明有关，实现无关。
int           func_mgr_get_func_declaration_count(FuncMgr *self,AetFuncType type,ClassName *className);
int           func_mgr_get_func_declaration_index(FuncMgr *self,AetFuncType type,ClassName *className,ClassFunc *need);
//mangleFunName在数组 deviceFuncPointers中的位置
int           func_mgr_get_device_func_index(FuncMgr *self,ClassName *className,char *mangleFunName);
void          func_mgr_add(FuncMgr *self,ClassFunc *func);
void          func_mgr_create_mangle_name(FuncMgr *self,tree fieldDecl,ClassName *className,char **result);
//分裂host device类型的方法
tree          func_mgr_divide_host_device_func(FuncMgr *self,location_t loc,ClassInfo *info,tree decls);
//根据主机函数获取对应的分裂函数
ClassFunc    *func_mgr_get_divide(FuncMgr *self,ClassFunc *host);
/**
 * 查找dest的父类方法是否声明为final,如果是返回true,否则false
 */
nboolean      func_mgr_parent_have_final(FuncMgr *self,location_t loc,ClassFunc *dest);
/**
 * 引用的对象方法可以在外部使用吗
 */
nboolean func_mgr_can_use_outside(FuncMgr *self,tree decl);

#endif
