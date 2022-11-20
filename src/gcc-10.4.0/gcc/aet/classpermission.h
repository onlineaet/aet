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

#ifndef __GCC_CLASS_PERMISSION_H__
#define __GCC_CLASS_PERMISSION_H__


#include "c-aet.h"

/**
 * 类或域修饰
 */
typedef struct _FieldDecorate{
	ClassPermissionType permission;
	nboolean isStatic;
	nboolean isFinal;
	location_t loc;
}FieldDecorate;

typedef struct _ClassPermission ClassPermission;
/* --- structures --- */
struct _ClassPermission
{
	c_parser *parser;
    ClassPermissionType permission;
    location_t  loc ;
    FieldDecorate currentDecorate;
    nboolean running;//正在运行，等待分析完类和域后，重置为FALSE;
};




ClassPermission    *class_permission_new();
ClassPermissionType class_permission_get_permission_type(ClassPermission *self,char *sysClassName,ClassType classType);
void                class_permission_reset(ClassPermission *self);
nboolean            class_permission_set_field(ClassPermission *self,tree decls,ClassName *className);
void                class_permission_check_and_set(ClassPermission *self,ClassParserState state,ClassPermissionType type);

FieldDecorate       class_permission_try_parser(ClassPermission *self,int classType);
void                class_permission_set_decorate(ClassPermission *self,FieldDecorate *dr);
FieldDecorate       class_permission_get_decorate_by_class(ClassPermission *self,char *sysClassName,ClassType classType);
void                class_permission_stop(ClassPermission *self);
void                class_permission_set_field_decorate(ClassPermission *self,tree decls,ClassName *className);







#endif

