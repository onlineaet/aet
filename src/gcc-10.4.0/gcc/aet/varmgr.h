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


#ifndef __GCC_VAR_MGR_H__
#define __GCC_VAR_MGR_H__

#include "nlib.h"
#include "c-aet.h"
#include "classinfo.h"
#include "linkfile.h"
#include "xmlfile.h"


typedef struct _VarEntity
{
	tree decl;
	ClassPermissionType permission;
	nboolean isStatic;
	nboolean isConst;
	tree init;
	tree init_original_type;
	enum tree_code original_code;
	char *orgiName;
	char *mangleVarName;
	nboolean isFinal;
	int serialNumber;//在class中的序号
	char *sysName;//所属类
}VarEntity;

typedef struct _VarMgr VarMgr;
/* --- structures --- */
struct _VarMgr
{
	c_parser *parser;
    NHashTable *mgrHash;
    LinkFile *linkFile;
    XmlFile  *xmlFile;
};


VarMgr    *var_mgr_get();
nboolean   var_mgr_add(VarMgr *self,ClassName *className,tree varDecl);
nboolean   var_mgr_set_permission(VarMgr *self,ClassName *className,tree decl,ClassPermissionType permission);
nboolean   var_mgr_change_static_decl(VarMgr *self,ClassName *className,struct c_declspecs *specs,struct c_declarator *declarator);
nboolean   var_mgr_set_static_decl(VarMgr *self,ClassName *className,tree decl,struct c_expr *initExpr,ClassPermissionType permi,nboolean isFinal);
tree       var_mgr_get_static_var(VarMgr *self,ClassName *className,tree component);
ClassName *var_mgr_get_class_by_static_component(VarMgr *self,ClassName *srcName,tree component);
void       var_mgr_set_parser(VarMgr *self,c_parser *parser);
tree       var_mgr_find_func_pointer(VarMgr *self,ClassName *srcName,tree component,char **belongClass);
void       var_mgr_define_class_static_var(VarMgr *self,ClassName *implClassName,NString *codes);
void       var_mgr_set_final(VarMgr *self,ClassName *className,tree decl,nboolean isFinal);
NPtrArray *var_mgr_get_vars(VarMgr *self,char *sysName);
int        var_mgr_get_max_serial_number(VarMgr *self,ClassName *className);
VarEntity *var_mgr_get_var(VarMgr *self,char *sysName,char *varName);


VarEntity *var_mgr_get_static_entity(VarMgr *self,ClassName *className,tree component);
VarEntity *var_mgr_get_static_entity_by_recursion(VarMgr *self,ClassName *srcName,tree component);

char      *var_entity_get_sys_name(VarEntity *self);
tree       var_entity_get_tree(VarEntity *self);

#endif


