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

#include "config.h"
#define INCLUDE_UNIQUE_PTR
#include "system.h"
#include "coretypes.h"
#include "c-family/c-pragma.h"
#include "c/c-lang.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"
#include "options.h"

#include "aetutils.h"
#include "aetinfo.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "classinfo.h"
#include "genericutil.h"
#include "aetprinttree.h"
#include "funcmgr.h"
#include "classmgr.h"
#include "classutil.h"

ClassInfo *class_info_new()
{
	ClassInfo *self = n_slice_alloc0 (sizeof(ClassInfo));
	self->type=CLASS_TYPE_NORMAL;
	self->className.sysName=NULL;
	self->className.userName=NULL;
	self->className.package=NULL;
	self->parentName.sysName=NULL;
	self->parentName.userName=NULL;
	self->parentName.package=NULL;
	int i;
	for(i=0;i<AET_MAX_INTERFACE;i++){
	  self->ifaces[i].sysName=NULL;
	  self->ifaces[i].userName=NULL;
	  self->ifaces[i].package=NULL;
	}
	self->ifaceCount=0; //父类名
	self->record=NULL_TREE;
	self->genericModel=NULL;
	self->parentGenericModel=NULL;
	self->ifaceGenModelsCount=0;
    self->isFinal=FALSE;
    self->file=NULL;
	return self;
}

nboolean   class_info_is_class(ClassInfo *self)
{
	return self->type==CLASS_TYPE_NORMAL;
}

nboolean   class_info_is_abstract_class(ClassInfo *self)
{
	return self->type==CLASS_TYPE_ABSTRACT_CLASS;

}

nboolean   class_info_is_interface(ClassInfo *self)
{
	return self->type==CLASS_TYPE_INTERFACE;
}

nboolean   class_info_is_final(ClassInfo *self)
{
	return self->isFinal;
}

nboolean   class_info_is_field(ClassInfo *self,tree component)
{
     tree record=self->record;
     return class_util_have_field(record,component);
}

nboolean   class_info_is_field_by_name(ClassInfo *self,char *fieldName)
{
     tree record=self->record;
     tree  component=aet_utils_create_ident(fieldName);
     return class_util_have_field(record,component);
}

tree  class_info_get_field_by_component(ClassInfo *self,tree ident)
{
    tree chain;
	tree record=self->record;
    tree fieldList=TYPE_FIELDS(record);
	for (chain = fieldList; chain; chain = DECL_CHAIN (chain)){
        tree name=DECL_NAME(chain);
        n_info("class_info_get_field_by_component 查找field val name::%s code:%s %d",IDENTIFIER_POINTER(name),get_tree_code_name(TREE_CODE(name)),name==ident);
        if(name==ident)
        	return chain;
	}
	return NULL_TREE;
}

tree  class_info_get_field_by_name(ClassInfo *self,char *idName)
{
	tree field;
	tree record=self->record;
    tree fieldList=TYPE_FIELDS(record);
	for (field = fieldList; field; field = DECL_CHAIN (field)){
        tree name=DECL_NAME(field);
        tree type=TREE_TYPE(field);
        if(!strcmp(idName,IDENTIFIER_POINTER(name))){
            n_info("class_info_get_field_by_name 找到了field class:%s val name::%s",self->className.sysName,IDENTIFIER_POINTER(name));
        	return field;
        }
	}
	return NULL_TREE;
}


static void copyClassName(ClassName *dest,ClassName *src)
{
	dest->sysName=n_strdup(src->sysName);
	dest->userName=n_strdup(src->userName);
	dest->package=n_strdup(src->package);
}

void   class_info_set_parent(ClassInfo *self,ClassName *parent)
{
	//printf("class_info_set_parent ---- %s %s\n",self->parentName.sysName,parent->userName);
   if(self->parentName.sysName!=NULL){
	  n_warning("父类已设 %s %s",self->parentName.userName,parent->userName);
	  return;
   }
   copyClassName(&(self->parentName),parent);
}

void       class_info_set_ifaces(ClassInfo *self,ClassName **ifaces,int count)
{
	  if(self->ifaceCount>0){
		  n_warning("接口已设 %d %d",self->ifaceCount,count);
		  return;
	  }
	  int i;
	  for(i=0;i<count;i++){
		  copyClassName(&(self->ifaces[i]),ifaces[i]);
	  }
	  self->ifaceCount=count;
}

void       class_name_free(ClassName *className)
{
	if(className!=NULL){
		if(className->sysName)
			n_free(className->sysName);
		if(className->userName)
		    n_free(className->userName);
		if(className->package)
		    n_free(className->package);
		n_slice_free(ClassName,className);
	}
}

ClassName *class_info_clone_class_name(ClassInfo *self)
{
	ClassName *dest=(ClassName *)n_slice_new0(ClassName);
	copyClassName(dest,&self->className);
	return dest;
}

ClassName *class_name_clone(ClassName *className)
{
	ClassName *dest=(ClassName *)n_slice_new0(ClassName);
	copyClassName(dest,className);
	return dest;
}

nboolean   class_info_is_belong(ClassInfo *self,char *className)
{
	nboolean n=strcmp(self->className.sysName,className)==0;
	if(!n){
		int i;
		for(i=0;i<self->ifaceCount;i++){
			if(strcmp(self->ifaces[i].sysName,className)==0)
				return TRUE;
		}
	}
	return n;
}

/*
 * 是不是一个泛型声明
 * id 内容是E、T等
 */
nboolean class_info_is_generic_decl(ClassInfo *self,tree id)
{
    if(!self->genericModel)
    	return FALSE;
    n_debug("class_info_is_generic_decl --- %s\n",IDENTIFIER_POINTER(id));
    return generic_model_exits_ident(self->genericModel,IDENTIFIER_POINTER(id));
}

int class_info_get_undefine_generic_count(ClassInfo *self)
{
	if(!self->genericModel)
		return -1;
	return generic_model_get_undefine_count(self->genericModel);
}

int class_info_get_generic_count(ClassInfo *self)
{
	if(!self->genericModel)
		return 0;
	return generic_model_get_count(self->genericModel);
}

int  class_info_get_generic_index(ClassInfo *self,char *genericName)
{
	if(self==NULL || self->genericModel==NULL)
		return -1;
	return generic_model_get_index(self->genericModel,genericName);
}

int  class_info_get_generic_index_by_char(ClassInfo *self,char genericName)
{
	char rr[2];
	rr[0]=genericName;
	rr[1]='\0';
	return class_info_get_generic_index(self,rr);
}

/**
 * 判断是不是泛型类
 */
nboolean class_info_is_generic_class(ClassInfo *self)
{
	if(self==NULL)
		return FALSE;
  return self->genericModel!=NULL;
}

nboolean   class_info_is_impl_by_recursion(ClassInfo *self,char *ifaceSysName)
{
    if(self==NULL)
        return FALSE;
	int i;
	for(i=0;i<self->ifaceCount;i++){
		if(strcmp(self->ifaces[i].sysName,ifaceSysName)==0)
			return TRUE;
	}
    ClassInfo *parentInfo=class_mgr_get_class_info(class_mgr_get(),self->parentName.sysName);
	return class_info_is_impl_by_recursion(parentInfo,ifaceSysName);
}


nboolean  class_info_have_generic_function(ClassInfo *self)
{
	tree field;
	tree record=self->record;
    tree fieldList=TYPE_FIELDS(record);
	for (field = fieldList; field; field = DECL_CHAIN (field)){
        tree type=TREE_TYPE(field);
        if(TREE_CODE(type)==POINTER_TYPE){
        	type=TREE_TYPE(type);
        	if(TREE_CODE(type)==FUNCTION_TYPE){
        	   GenericModel *gen=c_aet_get_func_generics_model(field);
        	   printf("class_info_have_generic_function --- %p %s\n",gen,IDENTIFIER_POINTER(DECL_NAME(field)));
        	   if(gen)
        		 return TRUE;
        	}
        }
	}
	return FALSE;
}

/**
 * 从本身，父类，以及父类的父类，以及接口判断是否与泛型有关
 */
nboolean   class_info_about_generic(ClassInfo *self)
{
	nboolean haveFunc=func_mgr_have_generic_func(func_mgr_get(),&self->className);
	nboolean re= (self->genericModel || self->parentGenericModel || self->ifaceGenModelsCount>0 || haveFunc);
	if(re)
		return TRUE;
    ClassInfo *parentInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),&self->parentName);
    if(parentInfo==NULL)
			return FALSE;
    return class_info_about_generic(parentInfo);
}

void  class_info_set_generic_model(ClassInfo *self,GenericModel *model)
{
      self->genericModel=model;
}


void  class_info_set_parent_generic_model(ClassInfo *self,GenericModel *model)
{
	  self->parentGenericModel=model;

}

void  class_info_add_iface_generic_model(ClassInfo *self,ClassName *className,GenericModel *model)
{
	int index=self->ifaceGenModelsCount;
	self->ifaceGenModels[index].className=className;
	self->ifaceGenModels[index].model=model;
	self->ifaceGenModelsCount++;
}

GenericModel  *class_info_get_generic_model(ClassInfo *self)
{
   return self->genericModel;
}

/**
 * 创建缺省的泛型定义
 */
GenericModel *class_info_create_default_generic_define(ClassInfo *self)
{
	if(!self->genericModel)
	   return NULL;
	GenericModel *model=generic_model_create_default(self->genericModel);
	return model;
}

/**
 * 判断classinfo是否声明在文件file中。
 */
nboolean  class_info_is_decl_file(ClassInfo *self,char *file)
{
    if(self==NULL || self->file==NULL || file==NULL)
        return FALSE;
    return !strcmp(self->file,file);
}

/**
 * 为AClass创建代码
 */
char *class_info_create_class_code(ClassInfo *self)
{
    ClassName *className=&self->className;
    NString *codes=n_string_new("");
    //给AClass.h中的属性赋值
    n_string_append_printf(codes,"obj->name=strdup(\"%s\");\n",className->userName);
    if(className->package)
       n_string_append_printf(codes,"obj->packageName=strdup(\"%s\");\n",className->package);
    if(self->parentName.sysName)
       n_string_append_printf(codes,"obj->parent=%s.class;\n",self->parentName.userName);
    if(!class_info_is_interface(self)){
        int i;
        for(i=0;i<self->ifaceCount;i++){
            n_string_append_printf(codes,"obj->interfaces[%d]=%s.class;\n",i,self->ifaces[i].userName);
            char ifaceVarName[255];
            aet_utils_create_in_class_iface_var(self->ifaces[i].userName,ifaceVarName);
            n_string_append_printf(codes,"obj->interfacesOffset[%d]=offsetof(%s,%s);\n",i,className->sysName,ifaceVarName);
        }
        if(self->ifaceCount>0){
            n_string_append_printf(codes,"obj->interfaceCount=%d;\n",i,self->ifaceCount);
        }
    }
    char *ret=n_strdup(codes->str);
    n_string_free(codes,TRUE);
    return ret;
}


nboolean      class_info_is_root(ClassInfo *self)
{
    return self->type==CLASS_TYPE_NORMAL && !strcmp(self->className.userName,AET_ROOT_OBJECT);
}

/**
 * 声明所在的文件
 */
void  class_info_set_file(ClassInfo *self,char *file)
{
    if(self->file!=NULL){
        n_error("类%s声明所在的文件%s已设置。",self->className.userName,self->file);
        return;
    }
    self->file=n_strdup(file);
}

char  *class_info_get_file(ClassInfo *self)
{
    if(self==NULL)
        return NULL;
    return self->file;
}

/**
 * 比较class$ Abx和impl$ Abx后继承的父类接口是否一样
 */
nboolean class_info_decl_equal_impl(ClassInfo *self,NPtrArray *implParent,NPtrArray *implIface)
{
    if(strcmp(self->className.userName,AET_ROOT_OBJECT)==0){//如果AObject
        if(implParent!=NULL && implParent->len>0)
            return FALSE;
        if(implIface!=NULL && implIface->len>0)
            return FALSE;
        return TRUE;
    }
    nboolean equalParent=FALSE;
    nboolean equalIface=FALSE;

   //比较父类
    if(implParent!=NULL && implParent->len==1){
        char *parent=n_ptr_array_index(implParent,0);
        if(!strcmp(parent,self->parentName.userName) || !strcmp(parent,self->parentName.sysName)){
            equalParent=TRUE;
        }
    }else if(implParent==NULL || implParent->len==0){
        equalParent=TRUE;
    }
    if(!equalParent)
        return FALSE;

    int implIfaceCount=implIface==NULL?0:implIface->len;
    if(implIfaceCount==0)
        return TRUE;
    if(implIfaceCount!=self->ifaceCount)
        return FALSE;
    int i;
    printf("ffdd %d %d\n",implIface->len,self->ifaceCount);
    for(i=0;i<self->ifaceCount;i++){
        char *iface=n_ptr_array_index(implIface,i);
        if(strcmp(iface,self->ifaces[i].userName) || strcmp(iface,self->ifaces[i].sysName)){
            return FALSE;
        }
    }
    return TRUE;

}



