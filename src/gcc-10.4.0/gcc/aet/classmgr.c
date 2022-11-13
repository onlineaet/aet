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
#include <cstdio>
#define INCLUDE_UNIQUE_PTR
#include "system.h"
#include "coretypes.h"
#include "target.h"
#include "function.h"
#include "tree.h"
#include "timevar.h"
#include "stringpool.h"
#include "cgraph.h"
#include "attribs.h"
#include "toplev.h"
#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "c-family/c-common.h"
#include "gimple-expr.h"
#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"
#include "../libcpp/include/cpplib.h"
#include "opts.h"

#include "c-aet.h"
#include "classmgr.h"
#include "aetutils.h"
#include "funcmgr.h"
#include "classparser.h"
#include "classimpl.h"
#include "aet-c-parser-header.h"


static void classMgrInit(ClassMgr *self)
{
	self->mgrHash=n_hash_table_new(n_str_hash,n_str_equal);
}

nboolean class_mgr_add(ClassMgr *self,char *sysClassName,char *userClassName,char *package)
{
   	if(n_hash_table_contains(self->mgrHash,sysClassName)){
   	    error("重复的class %qs",userClassName);
   	    return FALSE;
   	}
	ClassInfo *info=class_info_new();
	info->className.sysName=n_strdup(sysClassName);
	info->className.userName=n_strdup(userClassName);
	if(package)
	  info->className.package=n_strdup(package);
	else
	  info->className.package=NULL;
    const char *file = LOCATION_FILE (input_location);
    if(file!=NULL)
        class_info_set_file(info,file);
    n_hash_table_insert (self->mgrHash, n_strdup(sysClassName),info);
	return TRUE;
}

nboolean class_mgr_set_type(ClassMgr *self,location_t loc,char *sysClassName,ClassType classType,ClassPermissionType permission,nboolean isFinal)
{
   	if(!n_hash_table_contains(self->mgrHash,sysClassName)){
   	    error ("没找到class %qs",sysClassName);
   	    return FALSE;
   	}
    ClassInfo *info=n_hash_table_lookup(self->mgrHash,sysClassName);
	info->type=classType;
	info->permission=permission;
	info->isFinal=isFinal;
  	if(isFinal){
      if(class_info_is_abstract_class(info)){
     	 error_at(loc,"类%qs是抽象类，不能用final$修饰。",sysClassName);
     	 return FALSE;
      }
      if(class_info_is_interface(info)){
		 error_at(loc,"类%qs是接口，不能用final$修饰。",sysClassName);
		 return FALSE;
      }
   	}
  	NString *checkStr=n_string_new(sysClassName);
  	nboolean reverName=(n_string_ends_with(checkStr,AET_ROOT_OBJECT) || n_string_ends_with(checkStr,AET_ROOT_CLASS));
  	n_string_free(checkStr,TRUE);
  	if(info->type==CLASS_TYPE_INTERFACE && reverName){
  	    error_at(loc,"接口%qs的名字不能含有%qs或%qs。",sysClassName,AET_ROOT_OBJECT,AET_ROOT_CLASS);
  	    return FALSE;
  	}
	return TRUE;
}

tree  class_mgr_get_field_by_component(ClassMgr *self,char *className,tree ident)
{
	tree chain;
	ClassInfo *info=n_hash_table_lookup(self->mgrHash,className);
	if(info==NULL){
		printf("class_mgr_get_field_by_component 00 找不到classinfo %s %s\n",className,IDENTIFIER_POINTER(ident));
		return NULL_TREE;
	}
	return class_info_get_field_by_component(info,ident);
}

/**
 * 可能是class absclass或接口interface
 */
nboolean   class_mgr_is_class(ClassMgr *self,char *sysClassName)
{
	if(sysClassName==NULL)
		return FALSE;
	ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysClassName);
	return info!=NULL;
}

/**
 * 如果.h文件和编译文件在同一目录，以.h中的声明的类为准。
 * 如果不在同一目录，找与编译文件的目录接近度最高的头文件中声明的为准。
 * 接近度(proximity), 如何把目录转向量来计算接近度?。
 */
ClassName *class_mgr_get_class_name_by_user(ClassMgr *self,char *userClassName)
{
	NHashTableIter iter;
	npointer key, value;
	n_hash_table_iter_init(&iter, self->mgrHash);
	int count=0;
	ClassInfo *result=NULL;
	while (n_hash_table_iter_next(&iter, &key, &value)) {
		ClassInfo *info = (ClassInfo *)value;
		if(strcmp(info->className.userName,userClassName)==0){
		   result=info;
           count++;
		}
	}
	if(count==0)
		return NULL;
	if(count==1)
	    return &(result->className);
	//处理超过两个的情况。
	n_hash_table_iter_init(&iter, self->mgrHash);
	NFile *inFnames=n_file_new(in_fnames[0]);
	NFile  *inc=n_file_get_canonical_file(inFnames);
	const char *parentDir=n_file_get_parent(inc);
    while (n_hash_table_iter_next(&iter, &key, &value)) {
        ClassInfo *info = (ClassInfo *)value;
        if(strcmp(info->className.userName,userClassName)==0){
            printf("in_fnames[0] %s file:%s\n",in_fnames[0],info->file);
            NFile *f=n_file_new(info->file);
            if(n_file_equals(inFnames,f)){
                n_file_unref(f);
                return &(info->className);
            }
            NFile  *fc=n_file_get_canonical_file(f);
            const char *dir=n_file_get_parent(fc);
            if(!strcmp(dir,parentDir)){
                printf("in_fnames[0] 返回了-----%s file:%s\n",in_fnames[0],info->file);
                n_file_unref(fc);
                n_file_unref(f);
                return &(info->className);
            }
            n_file_unref(f);
            n_file_unref(fc);
        }
    }

    n_file_unref(inc);
    n_file_unref(inFnames);

	//如果在parsering中并且classUserName==userClassName返回 该ClassInfo;
	if(class_parser_is_parsering(class_parser_get())){
	    ClassName *className=class_parser_get_class_name(class_parser_get());
	    if(className!=NULL && !strcmp(className->userName,userClassName)){
	        ClassInfo *info=class_mgr_get_class_info(self,className->sysName);
	        if(info!=NULL){
	            return &(info->className);
	        }
	    }
	}
	//如果在aet中与aet的类名为准
    c_parser *parser=class_parser_get()->parser;
    if(parser->isAet){
        ClassName *className=class_impl_get_class_name(class_impl_get());
        if(className!=NULL && !strcmp(className->userName,userClassName)){
           ClassInfo *info=class_mgr_get_class_info(self,className->sysName);
           if(info!=NULL){
              return &(info->className);
           }
        }
    }
    //如果当前编译文件与头文件中同一个目录与该目录的为准

    NString *str=n_string_new("");
    n_string_append_printf(str,"找到多个相同的类名:%s ",userClassName);
	n_hash_table_iter_init(&iter, self->mgrHash);
	count=1;
	while (n_hash_table_iter_next(&iter, &key, &value)) {
		ClassInfo *info = (ClassInfo *)value;
		if(strcmp(info->className.userName,userClassName)==0){
		    n_string_append_printf(str,"%d 包名:%s 系统名:%s ",count,info->className.package,info->className.sysName);
		    count++;
		    result=info;
		}
	}
    n_string_append_printf(str," 最终选择的是:%s。 要精确的访问类，你可在类名前加上包名。如：com_ai_ConvOps。",result->className.sysName);
    warning_at (input_location,0,"%qs",str->str);
	n_string_free(str,TRUE);
    return &(result->className);
}

ClassName *class_mgr_get_class_name_by_sys(ClassMgr *self,char *sysClassName)
{
    ClassInfo *info = class_mgr_get_class_info(self,sysClassName);
    if(info==NULL)
    	return NULL;
    return &(info->className);
}

ClassInfo *class_mgr_get_class_info(ClassMgr *self,char *sysClassName)
{
	if(sysClassName==NULL)
		return NULL;
	ClassInfo *info=n_hash_table_lookup(self->mgrHash,sysClassName);
	return info;
}

ClassInfo *class_mgr_get_class_info_by_underline_sys_name(ClassMgr *self,char *_sysClassName)
{
	    NHashTableIter iter;
		npointer key, value;
		n_hash_table_iter_init(&iter, self->mgrHash);
		int count=0;
		while (n_hash_table_iter_next(&iter, &key, &value)) {
			ClassInfo *info = (ClassInfo *)value;
			char underline[256];
			sprintf(underline,"_%s",info->className.sysName);
			n_debug("class_mgr_get_class_info_by_underline_sys_name 00 %s",underline);
			if(strcmp(_sysClassName,underline)==0)
				return info;
		}
		return NULL;
}

ClassName *class_mgr_clone_class_name(ClassMgr *self,char *sysClassName)
{
	ClassInfo *info=class_mgr_get_class_info(self,sysClassName);
	if(info==NULL)
		return NULL;
    return class_info_clone_class_name(info);
}

nboolean   class_mgr_is_interface(ClassMgr *self,ClassName *className)
{
	ClassInfo *info=n_hash_table_lookup(self->mgrHash,className->sysName);
	if(info==NULL){
	   printf("class_mgr_is_interface 00 找不到classinfo %s\n",className->sysName);
	   return FALSE;
	}
	return class_info_is_interface(info);
}

ClassInfo *class_mgr_get_class_info_by_class_name(ClassMgr *self,ClassName *className)
{
	if(className==NULL)
		return NULL;
	return class_mgr_get_class_info(self,className->sysName);
}

nboolean   class_mgr_set_record(ClassMgr *self,ClassName *className,tree record)
{
    ClassInfo *info=n_hash_table_lookup(self->mgrHash,className->sysName);
	if(info==NULL){
		printf("class_mgr_set_record 00 找不到classinfo %s\n",className->sysName);
		return FALSE;
	}
	tree recordTypeDecl=lookup_name(aet_utils_create_ident(className->sysName));
	if(!aet_utils_valid_tree(recordTypeDecl)){
	    n_error("找不到类%s的声明。只有record_type。",className->sysName);
	    return NULL;
	}
	info->record=record;
	info->recordTypeDecl=recordTypeDecl;
	return TRUE;
}

ClassName *class_mgr_get_class_by_component(ClassMgr *self,ClassName *srcName,tree component)
{
	if(srcName==NULL || component==NULL_TREE)
		return NULL;
	ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),srcName);
	if(info==NULL)
		return NULL;
	nboolean isField=class_info_is_field(info,component);
	if(isField)
		return &info->className;
	return class_mgr_get_class_by_component(self,&info->parentName,component);
}

/**
 * 检查my的父类是不是other
 */
static int getParentByClassName(char *my,char *other)
{
	ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),my);
	if(info==NULL || info->type==CLASS_TYPE_INTERFACE)
		return CLASS_RELATIONSHIP_UNKNOWN;
	if(info->parentName.sysName!=NULL && strcmp(other,info->parentName.sysName)==0)
		return CLASS_RELATIONSHIP_CHILD;
	else{
		return getParentByClassName(info->parentName.sysName,other);
	}
}

/**
 * my与other的关系
 *
 */
ClassRelationship   class_mgr_relationship(ClassMgr *self,char *my,char *other)
{
	if(my==NULL || other==NULL)
		return CLASS_RELATIONSHIP_UNKNOWN;
	ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),my);
	ClassInfo *otherInfo=class_mgr_get_class_info(class_mgr_get(),other);
	if(info==NULL || otherInfo==NULL)
		return CLASS_RELATIONSHIP_UNKNOWN;
	ClassRelationship ship=getParentByClassName(my,other);
	n_debug("class_mgr_relationship 00 my:%s other:%s %d",my,other,ship);
	if(ship==CLASS_RELATIONSHIP_UNKNOWN){
		ship=getParentByClassName(other,my);
		if(ship==CLASS_RELATIONSHIP_UNKNOWN){
			if(class_info_is_interface(info) && !class_info_is_interface(otherInfo)){
				nboolean re=class_info_is_impl(otherInfo,my);
				if(re)
					return CLASS_RELATIONSHIP_OTHER_IMPL;
			}else if(!class_info_is_interface(info) && class_info_is_interface(otherInfo)){
				nboolean re=class_info_is_belong(info,other);
				if(re)
				  return CLASS_RELATIONSHIP_IMPL;
			}

		}else{
           return CLASS_RELATIONSHIP_PARENT;
		}
	}else{
		return ship;
	}
	return CLASS_RELATIONSHIP_UNKNOWN;
}

tree  class_mgr_get_field_by_name(ClassMgr *self,ClassName *className,char *idName)
{
	tree field;
	ClassInfo *info=n_hash_table_lookup(self->mgrHash,className->sysName);
	if(info==NULL){
		printf("class_mgr_get_field_by_name 00 找不到classinfo %s %s\n",className,idName);
		return NULL_TREE;
	}
	return class_info_get_field_by_name(info,idName);
}

static ClassName *findInterface(ClassMgr *self,ClassName *className,ClassName *interface)
{
	 if(className==NULL || interface==NULL)
		 return NULL;
	 ClassInfo *info=class_mgr_get_class_info_by_class_name(self,className);
	 if(!info){
			return NULL;
	 }
	 int i;
	 for(i=0;i<info->ifaceCount;i++){
		 if(!strcmp(info->ifaces[i].sysName,interface->sysName))
			 return className;
	 }
	 return findInterface(self,&info->parentName,interface);
}


ClassName * class_mgr_find_interface(ClassMgr *self,ClassName *className,ClassName *interface)
{
	ClassName *belongClass=findInterface(self,className,interface);
	return belongClass;
}

nboolean class_mgr_about_generic(ClassMgr *self,ClassName *className)
{
	ClassInfo *info=class_mgr_get_class_info_by_class_name(self,className);
	return class_info_about_generic(info);
}

/**
 * 当前类到AObject的距离。
 * 把距离当作索引
 */
int class_mgr_get_distance(ClassMgr *self,char *sysName)
{
	ClassInfo *info=class_mgr_get_class_info(self,sysName);
	if(info==NULL)
		return -1;
	int count=0;
	while(info->parentName.sysName!=NULL){
		info=class_mgr_get_class_info(self,info->parentName.sysName);
		count++;
	}
	return count;
}

/**
 * 要判断权限 redo
 */
static char *findField(ClassMgr *self,char *sysName,char *fieldName)
{
     if(sysName==NULL || fieldName==NULL)
         return NULL;
     ClassInfo *info=class_mgr_get_class_info(self,sysName);
     if(!info){
         return NULL;
     }
     tree re= class_info_get_field_by_name(info,fieldName);
     if(aet_utils_valid_tree(re)){
         return sysName;
     }
     int i;
     for(i=0;i<info->ifaceCount;i++){
          ClassInfo *ifaceInfo=class_mgr_get_class_info(self, info->ifaces[i].sysName);
          tree re= class_info_get_field_by_name(ifaceInfo,fieldName);
          if(aet_utils_valid_tree(re)){
               return ifaceInfo->className.sysName;
           }
     }
     return findField(self,info->parentName.sysName,fieldName);
}

char  *class_mgr_find_field(ClassMgr *self,char *fromSysName,char *fieldName)
{
    char *belongClass=findField(self,fromSysName,fieldName);
    return belongClass;
}

NPtrArray *class_mgr_get_all_iface_info(ClassMgr *self)
{
        NPtrArray *array=n_ptr_array_new();
        NHashTableIter iter;
        npointer key, value;
        n_hash_table_iter_init(&iter, self->mgrHash);
        while (n_hash_table_iter_next(&iter, &key, &value)) {
            ClassInfo *info = (ClassInfo *)value;
            if(class_info_is_interface(info)){
                n_ptr_array_add(array,info);
            }
        }
        return array;
}


ClassMgr *class_mgr_get()
{
	static ClassMgr *singleton = NULL;
	if (!singleton){
		 singleton =n_slice_alloc0 (sizeof(ClassMgr));
		 classMgrInit(singleton);
	}
	return singleton;
}


