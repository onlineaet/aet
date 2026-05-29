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
#define INCLUDE_MEMORY
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

#include "aet-c-parser-header.h"
#include "c-aet.h"
#include "classmgr.h"
#include "aetutils.h"
#include "funcmgr.h"
#include "classparser.h"
#include "classimpl.h"
#include "middlefile.h"


static void classMgrInit(ClassMgr *self)
{
   self->mgrHash=n_hash_table_new(n_str_hash,n_str_equal);
   self->ifaceCheckCodes=n_string_new("");
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

nboolean class_mgr_set_type(ClassMgr *self,location_t loc,char *sysClassName,
      ClassType classType,ClassPermissionType permission,nboolean isFinal)
{
   if(!n_hash_table_contains(self->mgrHash,sysClassName)){
      error ("没找到class %qs",sysClassName);
      return FALSE;
   }
   ClassInfo *info=n_hash_table_lookup(self->mgrHash,sysClassName);
   info->type=classType;
   info->permission=permission;
   info->isFinal=isFinal;
   info->declLoc=loc;//声明的位置 class$ A
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
   nboolean reverName=(endswith(sysClassName,AET_ROOT_OBJECT) || endswith(sysClassName,AET_ROOT_CLASS));
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
   if(aet_parser_get()->isAet){
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
      //n_debug("class_mgr_get_class_info_by_underline_sys_name 00 %s",underline);
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
	    return FALSE;
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
 * my是other的父
 * my是other的子
 * my是ohter接口的实现。
 * ohter是my的接口实现。
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
				nboolean re=class_info_is_impl_by_recursion(otherInfo,my);
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

//取从className到AObject的类信息 (从大到小)
int class_mgr_get_class_info_desc(ClassMgr *self,ClassName *className,ClassInfo **infos)
{
   ClassInfo *info=class_mgr_get_class_info_by_class_name(self,className);
   int count=0;
   infos[count++]=info;
   while(info->parentName.sysName!=NULL){
      info=class_mgr_get_class_info(self,info->parentName.sysName);
      infos[count++]=info;
   }
   return count;
}

//取从AObject到 className的类信息 (从小到大)
int class_mgr_get_class_info_asc(ClassMgr *self,ClassName *className,ClassInfo **infos)
{
   ClassInfo *temps[30];
   int count = class_mgr_get_class_info_desc(self,className,temps);
   int i;
   int j=0;
   for(i=count-1;i>=0;i--)
      infos[j++]=temps[i];
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

nboolean   class_mgr_is_root_object(ClassMgr *self,char *sysName)
{
    ClassInfo *info=class_mgr_get_class_info(self,sysName);
    if(info==NULL)
        return FALSE;
    return class_info_is_root(info);
}

/**
 * 是不是mtcsclass
 */
nboolean class_mgr_is_mtcs_class(ClassMgr *self,ClassName *className)
{
   nboolean haveMtcsFunc=func_mgr_have_mtcs_func(func_mgr_get(),className);
   if(haveMtcsFunc)
      return TRUE;
   nboolean haveStaticMtcsFunc=func_mgr_have_static_mtcs_func(func_mgr_get(),className);
   if(haveStaticMtcsFunc)
      return TRUE;

   ClassInfo *info=class_mgr_get_class_info_by_class_name(self,className);
   /*本类的接口方法个数*/
   int i;
   for(i=0;i<info->ifaceCount;i++){
      ClassName *iface=&(info->ifaces[i]);
      nboolean haveMtcsFunc=func_mgr_have_mtcs_func(func_mgr_get(),className);
      if(haveMtcsFunc)
         return TRUE;
      nboolean haveStaticMtcsFunc=func_mgr_have_static_mtcs_func(func_mgr_get(),className);
      if(haveStaticMtcsFunc)
         return TRUE;
   }
   return FALSE;
}
//获取父类的ClassName;
ClassName *class_mgr_get_parent(ClassMgr *self,ClassName *className)
{
   if(className==NULL)
      return NULL;
   ClassInfo *info = class_mgr_get_class_info_by_class_name(self,className);
   if(info->parentName.sysName)
      return &info->parentName;
   return NULL;
}

ClassName *class_mgr_get_parent(ClassMgr *self,char *sysName){
   if(sysName==NULL)
      return NULL;
   ClassInfo *info = class_mgr_get_class_info(self,sysName);
   if(info->parentName.sysName)
      return &info->parentName;
   return NULL;
}

/**
 * parentSysName是childSysName父类吗
 */
nboolean class_mgr_is_parent(ClassMgr *self,char *childSysName,char *parentSysName)
{
   if(childSysName==NULL || parentSysName==NULL)
      return FALSE;
   ClassInfo *childInfo=class_mgr_get_class_info(self,childSysName);
   ClassInfo *parentInfo=class_mgr_get_class_info(self,parentSysName);
   if(childInfo==NULL || parentInfo==NULL)
      return FALSE;
   if(!class_info_is_class(childInfo) || !class_info_is_class(parentInfo))
         return FALSE;
   return childInfo->parentName.sysName && !strcmp(childInfo->parentName.sysName,parentSysName);
}

/**
 * ancestorsSysName是不是childSysName的祖先，爷爷以上称为祖先。
 */
nboolean   class_mgr_is_ancestors(ClassMgr *self,char *childSysName,char *ancestorsSysName)
{
   if(childSysName==NULL || ancestorsSysName==NULL)
      return FALSE;
   ClassInfo *childInfo=class_mgr_get_class_info(self,childSysName);
   ClassInfo *ancestorsInfo=class_mgr_get_class_info(self,ancestorsSysName);
   if(childInfo==NULL || ancestorsInfo==NULL)
      return FALSE;
   if(!class_info_is_class(childInfo) || !class_info_is_class(ancestorsInfo))
         return FALSE;
   ClassName *parentName = class_mgr_get_parent(self,childSysName);
   if(!parentName)
      return FALSE;
   ClassInfo *parentInfo=class_mgr_get_class_info_by_class_name(self,parentName);

   //跳过父类
   ClassInfo *a=parentInfo;
   while(a->parentName.sysName){
      if(!strcmp(a->parentName.sysName,ancestorsSysName))
         return TRUE;
      a=class_mgr_get_class_info(self,a->parentName.sysName);
   }
   return FALSE;
}



//保存impl$ A的位置
void class_mgr_set_impl_location(ClassMgr *self,ClassName *className,location_t implLoc)
{
   ClassInfo *info = class_mgr_get_class_info_by_class_name(self,className);
   if(!info)
      return;
   info->implLoc=implLoc;
}

//获取实现位置
location_t  class_mgr_get_impl_location(ClassMgr *self,ClassName *className)
{
   ClassInfo *info = class_mgr_get_class_info_by_class_name(self,className);
   return info->implLoc;
}

static char *FREE_CHILD_NAME="free_child";
static char *GET_CLASS="getClass";

/**
 * 不要检查保留的方法
 */
static nboolean reserveField(ClassFunc *func)
{
    if(!strcmp(func->orgiName,FREE_CHILD_NAME))
      return TRUE;
    if(!strcmp(func->orgiName,GET_CLASS))
      return TRUE;
    return FALSE;
}

static nboolean findDefine(ClassFunc *dest,ClassName *src)
{
   NPtrArray  *srcFuncs=func_mgr_get_funcs(func_mgr_get(),src);
   if(srcFuncs==NULL)
      return FALSE;
    char *praw=dest->rawMangleName;
    nboolean findDefine=FALSE;
    int i;
    for(i=0;i<srcFuncs->len;i++){
       ClassFunc *compareFunc=(ClassFunc *)n_ptr_array_index(srcFuncs,i);
       if(strcmp(compareFunc->rawMangleName,praw)==0){
          if(aet_utils_valid_tree(compareFunc->fromImplDefine)){
             findDefine=TRUE;
             break;
          }
       }
    }
    return findDefine;
}


/**
 * 检查在class$中声明的方法，是否已实现
 * 不包括抽象方法和接口
 */
static nboolean checkSelfFuncDefine(ClassMgr *self,ClassName *className)
{
   NPtrArray  *array=func_mgr_get_funcs(func_mgr_get(),className);
   ClassInfo *info=class_mgr_get_class_info_by_class_name(self,className);
   int i;
   for(i=0;i<array->len;i++){
      ClassFunc *func=(ClassFunc *)n_ptr_array_index(array,i);
      if(class_func_is_divide(func))
         continue;
      if(aet_utils_valid_tree(func->fieldDecl) && !reserveField(func)){
         //如果有声明在这里必须有实现
         nboolean define=aet_utils_valid_tree(func->fromImplDefine);
         if(!define){
            if(func->isAbstract){
               n_debug("检查类方法:类%s的方法%s是抽象方法，可以不实现。\n",className->sysName,func->orgiName);
            }else{
               if(!func->fromInterface){
                  error_at(DECL_SOURCE_LOCATION(func->fieldDecl),"类%qs的方法%qs没有实现。",className->userName,func->orgiName);
                  return FALSE;
               }
            }
         }
      }
   }
   return TRUE;
}

/**
 * 检果是否实现了接口，如果是抽象类可以不实现。
 */
static nboolean eachInterface(ClassMgr *self,ClassName *from,ClassName *iface)
{
   ClassInfo *info=class_mgr_get_class_info_by_class_name(self,from);
   ClassInfo *faceInfo=class_mgr_get_class_info_by_class_name(self,iface);
   NPtrArray *ifaceFuncsArray=func_mgr_get_funcs(func_mgr_get(),iface);
   if(ifaceFuncsArray==NULL || ifaceFuncsArray->len==0)
      return TRUE;
   int i=0;
   for(i=0;i<ifaceFuncsArray->len;i++){
      ClassFunc *interfaceMethod=(ClassFunc *)n_ptr_array_index(ifaceFuncsArray,i);
      if(class_func_is_interface_reserve(interfaceMethod)) //是接口需要保留的ref和unref
         continue;
      char *atSysName=NULL;
      ClassFunc *impl=func_mgr_get_interface_impl(func_mgr_get(),from, interfaceMethod,&atSysName);//获得接口的实现类和方法
      if(impl==NULL && !class_info_is_abstract_class(info)){
         error("类%qs没有实现接口%qs的方法%qs。",from->userName,iface->userName,interfaceMethod->orgiName);
         return FALSE;
      }else if(impl==NULL && class_info_is_abstract_class(info)){
          n_warning("抽象类%s没有实现接口%s的方法%s。",from->userName,iface->userName,interfaceMethod->orgiName);
      }
   }
   return TRUE;
}

static nboolean checkInterfaceFuncDefine(ClassMgr *self,ClassName *className)
{
   ClassInfo *info=class_mgr_get_class_info_by_class_name(self,className);
   if(info==NULL || info->ifaceCount<=0){
      n_debug("检查接口方法 类%s没有要实现的接口，不需要检查方法实现。",className->sysName);
      return TRUE;
   }
   int i;
   for(i=0;i<info->ifaceCount;i++){
      ClassName iface=info->ifaces[i];
      ClassInfo *faceInfo=class_mgr_get_class_info_by_class_name(self,&iface);
      if(faceInfo==NULL){
         error("在类%qs中实现接口%qs,但没有找到接口的声明。检查是否包含对应的头文件",className->userName,iface.userName);
         return FALSE;
      }
      if(!eachInterface(self,className,&iface)){
         return FALSE;
      }
   }
   return TRUE;
}

/**
 * 检果是否实现了父类的抽象方法，如果是抽象类可以不实现。
 */
static nboolean  checkParentAbstractFuncDefine(ClassMgr *self,ClassName *className)
{
   ClassInfo *info=class_mgr_get_class_info_by_class_name(self,className);
   if(info->parentName.sysName==NULL){
      n_debug("检查父类方法 类%s没有父类，不需要检查方法实现。\n",className->sysName);
      return TRUE;
   }
   ClassName *parentName=&info->parentName;
   ClassInfo *parentInfo=class_mgr_get_class_info_by_class_name(self,parentName);
   if(!class_info_is_abstract_class(parentInfo)){
      n_debug("检查父类方法 类%s继承的父类%s不是抽象类，不需要检查是否实现父类的方法。\n",className->sysName,parentName->sysName);
      return TRUE;
   }
   n_debug("检查父类方法 子类%s是%s，父类%s是抽象类，检查是否实现了父类中的抽象方法。\n",className->sysName,
               class_info_is_abstract_class(info)?"抽象类":"普通类",parentName->sysName);
   NPtrArray  *parentArray=func_mgr_get_funcs(func_mgr_get(),parentName);
   //抽象方法是否要实现
   int i;
   for(i=0;i<parentArray->len;i++){
      ClassFunc *func=(ClassFunc *)n_ptr_array_index(parentArray,i);
      if(aet_utils_valid_tree(func->fieldDecl) && func->isAbstract){
         n_debug("检查父类方法 父类%s的抽象方法:%s\n",parentName->sysName,func->orgiName);
         //检果名字，参数返回值
         nboolean find=findDefine(func,className);
         if(!find){
            if(class_info_is_abstract_class(info)){
               n_debug("检查父类方法 类%s是抽象类，父类%s有抽象方法%s,可以不实现父类的抽象方法。\n",
                           className->sysName,parentName->sysName,func->orgiName);
            }else{
               error_at(DECL_SOURCE_LOCATION(func->fieldDecl),"类%qs必须实现父类%qs的抽象方法%qs。",
                           className->userName,parentName->userName,func->orgiName);
               return FALSE;
            }
         }else{
            n_debug("检查父类方法 子类%s实现了父类%s的抽象方法%s。%s\n",
            className->sysName,parentName->sysName,func->orgiName,class_info_is_abstract_class(info)?"子类是抽象类":"子类是普通类");
         }
      }
   }
   return TRUE;
}


/**
 * 检查类className的方法实现
 * 1.类本身中声明的方法
 * 2.类本身的接口
 * 3.检查父类的抽象方法
 * 4.父类的接口
 *从原 funccheck.c中移植。
 */
nboolean  class_mgr_check(ClassMgr *self,ClassName *className)
{
   ClassInfo *info=class_mgr_get_class_info_by_class_name(self,className);
   nboolean re=checkSelfFuncDefine(self,className);
   if(!re)
     return FALSE;
   re=checkInterfaceFuncDefine(self,className);//class_interface_add_define(self->classInterface,self->className) 在做同样的事。
   if(!re)
      return FALSE;
   re=checkParentAbstractFuncDefine(self,className);
   return re;
}

/*
   如果本类(1)非抽象类(2)继承抽象类(3)抽象类有接口(4)本类没有实现抽象类引用的接口
   需要检查在继承链中的类是否实现了接口。
   保存的格式如下:
   CLASS FUNC START:
   A            类名
   B D...       继承关系
   IFACE:METHOD 检查的接口
   IFACE:METHOD 检查的接口
   ....
   CLASS FUNC END:
*/
static void creatNeedCheckFunc(ClassMgr *self,ClassName *className)
{
   ClassInfo *info=class_mgr_get_class_info_by_class_name(self,className);
   if(class_info_is_abstract_class(info) || class_info_is_interface(info))
      return;
   NString *codes=n_string_new("");
   ClassInfo *my=info;
   while(my->parentName.sysName!=NULL){
      ClassInfo *parent=class_mgr_get_class_info(self,my->parentName.sysName);
      if(!class_info_is_abstract_class(parent))
         //不是抽象类不需要检查
         break;
      else{
         int i;
         for(i=0;i<parent->ifaceCount;i++){
            ClassInfo *ifaceInfo=class_mgr_get_class_info_by_class_name(self,&parent->ifaces[i]);
            NPtrArray *ifaceFuncsArray=func_mgr_get_funcs(func_mgr_get(),&parent->ifaces[i]);
            if(ifaceFuncsArray==NULL || ifaceFuncsArray->len==0)
               continue;
            int j=0;
            for(j=0;j<ifaceFuncsArray->len;j++){
               ClassFunc *interfaceMethod=(ClassFunc *)n_ptr_array_index(ifaceFuncsArray,j);
               if(class_func_is_interface_reserve(interfaceMethod)) //是接口需要保留的ref和unref
                  continue;
               ClassFunc *needImpl=func_mgr_get_func_by_raw_mangle(func_mgr_get(), className,interfaceMethod->rawMangleName);
               if(!needImpl){
                  //肯实有定义的，因为不是抽象函数
                  n_string_append_printf(codes,"%s:%s\n",
                        ifaceInfo->className.sysName,interfaceMethod->rawMangleName);
               }
            }
         }
      }
      my=parent;
   }
   //说明 本类有未实现的接口。
   if(codes->len>0){
      my=info;
      NString *record=n_string_new("");
      n_string_append(record,CLASS_IFACE_NEED_CHECK_START"\n");
      n_string_append(record,my->className.sysName);
      n_string_append(record,"\n");
      while(my->parentName.sysName!=NULL){
           ClassInfo *parent=class_mgr_get_class_info(self,my->parentName.sysName);
           n_string_append(record,my->parentName.sysName);
           n_string_append(record," ");
           my=parent;
      }
      n_string_append(record,"\n");
      n_string_append(self->ifaceCheckCodes,record->str);
      n_string_append(self->ifaceCheckCodes,codes->str);
      n_string_append(self->ifaceCheckCodes,CLASS_IFACE_NEED_CHECK_END"\n");
      n_string_free(record,TRUE);
   }
   n_string_free(codes,TRUE);
}

/**
 * 保存实现的接口信息。
 * 当编译完类实现时调用该方法。
 */
static void saveInterfaceMethod(ClassMgr *self,ClassName *className)
{
   ClassInfo *info=class_mgr_get_class_info_by_class_name(self,className);
   if(class_info_is_interface(info) || info->ifaceCount==0)
      return;
   NString *codes=n_string_new("");
   int i;
   for(i=0;i<info->ifaceCount;i++){
      ClassInfo *ifaceInfo=class_mgr_get_class_info(self, info->ifaces[i].sysName);
      NPtrArray *ifaceFuncsArray=func_mgr_get_funcs(func_mgr_get(),&ifaceInfo->className);
      n_debug("classmgr.c saveInterfaceMethod 00 %s\n",ifaceInfo->className.sysName);
      if(ifaceFuncsArray==NULL || ifaceFuncsArray->len==0)
         continue;
      int j=0;
      for(j=0;j<ifaceFuncsArray->len;j++){
         ClassFunc *interfaceMethod=(ClassFunc *)n_ptr_array_index(ifaceFuncsArray,j);
         n_debug("classmgr.c saveInterfaceMethod 11 %s %s\n",ifaceInfo->className.sysName,interfaceMethod->orgiName);
         //是接口需要保留的ref和unref
         if(class_func_is_interface_reserve(interfaceMethod))
            continue;
         char *atSysName=NULL;
         //获得接口的实现类
         ClassFunc *impl=func_mgr_get_interface_impl(func_mgr_get(),className, interfaceMethod,&atSysName);
         n_debug("classmgr.c saveInterfaceMethod 22 %s %s %p\n",ifaceInfo->className.sysName,atSysName,impl);
         if(impl!=NULL){
            n_string_append_printf(codes,"%s:%s:%s\n",atSysName,ifaceInfo->className.sysName,interfaceMethod->rawMangleName);
         }
      }
   }

   if(codes->len>0){
      n_string_append(self->ifaceCheckCodes,CLASS_IFACE_INFO_START"\n");
      n_string_append(self->ifaceCheckCodes,className->sysName);
      n_string_append(self->ifaceCheckCodes,"\n");
      n_string_append(self->ifaceCheckCodes,codes->str);
      n_string_append(self->ifaceCheckCodes,CLASS_IFACE_INFO_END"\n");
   }
   n_string_free(codes,TRUE);
}

void class_mgr_save_func_check_info(ClassMgr *self,ClassName *className)
{
   saveInterfaceMethod(self,className);
   creatNeedCheckFunc(self,className);
}

/**
 * 接口ifaceInfo与类classInfo之间是否有间接关系
 */
nboolean class_mgr_have_indirect_relationship(ClassMgr *self,ClassInfo *ifaceInfo,ClassInfo *classInfo)
{
   if(ifaceInfo==NULL || classInfo==NULL)
      return FALSE;
   if(!class_info_is_interface(ifaceInfo) || !class_info_is_class(classInfo))
      return FALSE;
   //查出所有实现接口的类，如果其中有一个是classInfo的子类，定义为有间接关系
   NHashTableIter iter;
   npointer key, value;
   n_hash_table_iter_init(&iter, self->mgrHash);
   int count=0;
   ClassInfo *result=NULL;
   while (n_hash_table_iter_next(&iter, &key, &value)) {
      ClassInfo *info = (ClassInfo *)value;
      if(class_info_have_iface(info,&ifaceInfo->className)){
         //与class的有关系
         ClassRelationship ship=  class_mgr_relationship(self, classInfo->className.sysName,info->className.sysName);
         if(ship==CLASS_RELATIONSHIP_PARENT || ship==CLASS_RELATIONSHIP_CHILD)
            return TRUE;
      }
   }
   return FALSE;
}

ClassInfo *class_mgr_get_root(ClassMgr *self)
{
   ClassName *rootClassName=class_mgr_get_class_name_by_user(self,AET_ROOT_OBJECT);
   ClassInfo *info=class_mgr_get_class_info_by_class_name(self, rootClassName);
   return info;
}

//根据系统类名获得用户类名
char   *class_mgr_get_user_name(ClassMgr *self,char *sysName)
{
   ClassName *className=class_mgr_get_class_name_by_sys(self,sysName);
   return className->userName;

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


