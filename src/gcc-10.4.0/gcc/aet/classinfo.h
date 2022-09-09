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

#ifndef __GCC_CLASS_INFO_H__
#define __GCC_CLASS_INFO_H__

#include "nlib.h"
#include "c-aet.h"
#include "genericmodel.h"


typedef enum
{
  CLASS_TYPE_NORMAL,
  CLASS_TYPE_ABSTRACT_CLASS,
  CLASS_TYPE_INTERFACE,
}ClassType;

typedef enum
{
  CLASS_RELATIONSHIP_PARENT,
  CLASS_RELATIONSHIP_CHILD,
  CLASS_RELATIONSHIP_IMPL,
  CLASS_RELATIONSHIP_OTHER_IMPL,
  CLASS_RELATIONSHIP_UNKNOWN,
}ClassRelationship;


typedef struct _ClassName
{
	char *userName;//用户命名 如 AObject Abc
	char *sysName; //系统生成的名字 包名+"_"+用户名 如 com_aet_NObject com_run_Abc;
	char *package; //包名如 com_aet
}ClassName;

typedef struct InterfaceGenerics{
	tree generics;
	ClassName *className;
};

typedef struct _IfaceGenModel{
	GenericModel *model;
	ClassName *className;
}IfaceGenModel;


typedef struct _ClassInfo ClassInfo;
/* --- structures --- */
struct _ClassInfo
{
	ClassType type;
	tree record;//RECORD_TYPE
    tree recordTypeDecl;//TYPE_DECL
	ClassName parentName;//父类名
	ClassName className;//类名
	ClassName ifaces[AET_MAX_INTERFACE];
	nuint     ifaceCount;

	ClassPermissionType permission;
	//泛型改成model
	GenericModel  *genericModel;
	GenericModel  *parentGenericModel; //设父类的泛型

	IfaceGenModel ifaceGenModels[AET_MAX_INTERFACE];
	nuint ifaceGenModelsCount;
	nboolean isFinal;//是不是final类型的类或接口
	char *file;//声明在那个文件。
};

ClassInfo *class_info_new();
nboolean   class_info_is_class(ClassInfo *self);
nboolean   class_info_is_abstract_class(ClassInfo *self);
nboolean   class_info_is_interface(ClassInfo *self);
nboolean   class_info_is_final(ClassInfo *self);
nboolean   class_info_is_field(ClassInfo *self,tree component);
nboolean   class_info_is_field_by_name(ClassInfo *self,char *fieldName);
tree       class_info_get_field_by_component(ClassInfo *self,tree ident);
tree       class_info_get_field_by_name(ClassInfo *self,char *name);

void         class_info_set_parent(ClassInfo *self,ClassName *parent);
void         class_info_set_ifaces(ClassInfo *self,ClassName **ifaces,int count);
void         class_name_free(ClassName *className);
nboolean     class_info_is_belong(ClassInfo *self,char *className);
nboolean     class_info_is_impl(ClassInfo *self,char *className);
nboolean     class_info_is_generic_decl(ClassInfo *self,tree id);
int          class_info_get_undefine_generic_count(ClassInfo *self);
int          class_info_get_generic_index(ClassInfo *self,char *genericName);
int          class_info_get_generic_index_by_char(ClassInfo *self,char genericName);

int          class_info_get_generic_count(ClassInfo *self);
nboolean     class_info_have_generic_function(ClassInfo *self);
nboolean     class_info_is_generic_class(ClassInfo *self);

ClassName    *class_info_clone_class_name(ClassInfo *self);
ClassName    *class_name_clone(ClassName *className);
nboolean      class_info_about_generic(ClassInfo *self);

///////////////////改成genericmodel-------
void          class_info_set_generic_model(ClassInfo *self,GenericModel *model);
void          class_info_set_parent_generic_model(ClassInfo *self,GenericModel *model);
void          class_info_add_iface_generic_model(ClassInfo *self,ClassName *className,GenericModel *model);
GenericModel *class_info_create_default_generic_define(ClassInfo *self);
GenericModel *class_info_get_generic_model(ClassInfo *self);

nboolean      class_info_is_decl_file(ClassInfo *self,char *file);//判断classinfo是否声明在文件file中。

#endif
