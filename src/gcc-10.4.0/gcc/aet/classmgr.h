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

#ifndef __GCC_CLASS_MGR_H__
#define __GCC_CLASS_MGR_H__

#include "nlib.h"
#include "classinfo.h"
#include "c-aet.h"


typedef struct _ClassMgr ClassMgr;
/* --- structures --- */
struct _ClassMgr
{
   NHashTable *mgrHash;
};


ClassMgr * class_mgr_get();
nboolean   class_mgr_is_class(ClassMgr *self,char *className);
nboolean   class_mgr_add(ClassMgr *self,char *sysClassName,char *userClassName,char *package);
nboolean   class_mgr_set_type(ClassMgr *self,location_t loc,char *sysClassName,ClassType classType,ClassPermissionType permission,nboolean isFinal);
ClassName *class_mgr_get_class_name_by_user(ClassMgr *self,char *userClassName);
ClassName *class_mgr_get_class_name_by_sys(ClassMgr *self,char *sysClassName);
ClassInfo *class_mgr_get_class_info(ClassMgr *self,char *sysClassName);
ClassInfo *class_mgr_get_class_info_by_underline_sys_name(ClassMgr *self,char *_sysClassName);
ClassInfo *class_mgr_get_class_info_by_class_name(ClassMgr *self,ClassName *className);
ClassName *class_mgr_clone_class_name(ClassMgr *self,char *sysClassName);
nboolean   class_mgr_is_interface(ClassMgr *self,ClassName *className);
nboolean   class_mgr_set_record(ClassMgr *self,ClassName *className,tree record);
ClassName *class_mgr_get_class_by_component(ClassMgr *self,ClassName *srcName,tree component);
ClassRelationship   class_mgr_relationship(ClassMgr *self,char *my,char *other);
tree       class_mgr_get_field_by_name(ClassMgr *self,ClassName *className,char *idName);
ClassName *class_mgr_find_interface(ClassMgr *self,ClassName *className,ClassName *interface);
nboolean   class_mgr_about_generic(ClassMgr *self,ClassName *className);
int        class_mgr_get_distance(ClassMgr *self,char *sysName);
char      *class_mgr_find_field(ClassMgr *self,char *fromSysName,char *fieldName);
NPtrArray *class_mgr_get_all_iface_info(ClassMgr *self);


#endif
