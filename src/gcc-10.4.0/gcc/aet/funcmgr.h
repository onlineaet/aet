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

FuncMgr      *func_mgr_get();
char         *func_mgr_create_field_modify_token(FuncMgr *self,ClassName *className);
NPtrArray    *func_mgr_get_funcs(FuncMgr *self,ClassName *className);
ClassFunc    *func_mgr_get_entity(FuncMgr *self,ClassName *className,char *mangle);
ClassFunc    *func_mgr_get_entity_by_sys_name(FuncMgr *self,char *sysName,char *mangle);
ClassFunc    *func_mgr_get_entity_by_mangle(FuncMgr *self,char *mangle);
char         *func_mgr_get_class_name_by_mangle(FuncMgr *self,char *mangle);

nboolean      func_mgr_change_class_func_decl(FuncMgr *self,struct c_declarator *declarator,ClassName *className,tree structTree);
nboolean      func_mgr_set_class_func_decl(FuncMgr *self,tree decl,ClassName *className);
nboolean      func_mgr_change_class_impl_func_define(FuncMgr *self,struct c_declspecs *specs,
		                struct c_declarator *declarator,ClassName *className);
nboolean      func_mgr_change_class_impl_func_decl(FuncMgr *self,struct c_declarator *declarator,ClassName *className);
nboolean      func_mgr_set_class_impl_func_decl(FuncMgr *self,tree decl,ClassName *className);
nboolean      func_mgr_set_class_impl_func_define(FuncMgr *self,tree decl,ClassName *className);
nboolean      func_mgr_func_exits(FuncMgr *self,ClassName *className,char *orgiName);
char         *func_mgr_get_mangle_func_name(FuncMgr *self,ClassName *className,char *orgiName);
NPtrArray    *func_mgr_get_constructors(FuncMgr *self,ClassName *className);
int           func_mgr_get_orgi_func_and_class_name(FuncMgr *self,char *newName,char *className,char *funcName);

nboolean      func_mgr_set_static_func_premission(FuncMgr *self,ClassName *className,tree funDecl,ClassPermissionType permission,nboolean isFinal);
nboolean      func_mgr_change_static_func_decl(FuncMgr *self,struct c_declarator *declarator,ClassName *className,tree structTree);
nboolean      func_mgr_set_static_func_decl(FuncMgr *self,tree decl,ClassName *className);
char         *func_mgr_create_static_var_name(FuncMgr *self,ClassName *className,tree varName,tree type);
nboolean      func_mgr_static_func_exits(FuncMgr *self,ClassName *className,char *orgiName);
NPtrArray    *func_mgr_get_static_funcs(FuncMgr *self,ClassName *className);
NPtrArray    *func_mgr_get_static_funcs_by_sys_name(FuncMgr *self,char *sysName);
nboolean      func_mgr_static_func_exits_by_recursion(FuncMgr *self,ClassName *srcName,tree component);
ClassFunc    *func_mgr_get_static_method(FuncMgr *self,char *sysName,char *mangle);
char         *func_mgr_get_static_class_name_by_mangle(FuncMgr *self,char *mangle);

char         *func_mgr_create_parm_string(FuncMgr *self,tree funcType);

nboolean      func_mgr_have_generic_func(FuncMgr *self,ClassName *className);
nboolean      func_mgr_is_generic_func(FuncMgr *self,ClassName *className,char *mangleFuncName);
ClassFunc    *func_mgr_get_interface_impl(FuncMgr *self,ClassName *from,ClassFunc *interfaceMethod,char **atClass);//获得接口的实现类和方法
int           func_mgr_get_max_serial_number(FuncMgr *self,ClassName *className);

void          func_mgr_check_permission_decl_between_define(FuncMgr *self,location_t loc,ClassName *className,
                           char *mangle,nboolean havePermission,ClassPermissionType permission);

#endif
