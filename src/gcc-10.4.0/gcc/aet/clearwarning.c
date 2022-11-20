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
#include "tree-core.h"
#include "toplev.h"
#include "c-family/c-pragma.h"
#include "c/c-tree.h"
#include "c/c-parser.h"
#include "opts.h"
#include "tree-iterator.h"
#include "c/c-lang.h"

#include "aetinfo.h"
#include "aetutils.h"
#include "classutil.h"
#include "aetprinttree.h"
#include "classmgr.h"
#include "c-aet.h"
#include "funcmgr.h"


static char *getIfaceCommonData123ByRecord(tree type)
{
    if(TREE_CODE(type)!=RECORD_TYPE){
        //n_warning("class_util_get_class_name_by_record 00 不是record_type %s",get_tree_code_name(TREE_CODE(type)));
        if(TREE_CODE(type)==POINTER_TYPE){
            //n_warning("class_util_get_class_name_by_record 11 可能是 Abc **obj %s",get_tree_code_name(TREE_CODE(type)));
            return getIfaceCommonData123ByRecord(TREE_TYPE(type));
        }
        return NULL;
    }
    tree next=TYPE_NAME(type);
    if(!aet_utils_valid_tree(next))
        return NULL;
    if(TREE_CODE(next)!=TYPE_DECL){
        //n_warning("class_util_get_class_name_by_record 11 不是TYPE_DECL %s",get_tree_code_name(TREE_CODE(next)));
        return NULL;
    }
    char *ifaceCommon=IDENTIFIER_POINTER(DECL_NAME (next));
    if(!strcmp(ifaceCommon,IFACE_COMMON_STRUCT_NAME)){
        return ifaceCommon;
    }
    return NULL;
}

static tree getTypeFromPointer(tree pointerType)
{
    if(TREE_CODE(pointerType)!=POINTER_TYPE)
        return pointerType;
    pointerType=TREE_TYPE(pointerType);
    return getTypeFromPointer(pointerType);
}

static char *getIfaceCommonData123ByPointer(tree pointerType)
{
    if(TREE_CODE(pointerType)!=POINTER_TYPE)
        return NULL;
    tree record=getTypeFromPointer(pointerType);
    return getIfaceCommonData123ByRecord(record);
}

static char *getIfaceCommonData123(tree type)
{
    if(!aet_utils_valid_tree(type))
        return NULL;
    if(TREE_CODE(type)==POINTER_TYPE)
        return getIfaceCommonData123ByPointer(type);
    else if(TREE_CODE(type)==RECORD_TYPE)
        return getIfaceCommonData123ByRecord(type);
    else if(TREE_CODE(type)==ARRAY_TYPE){
        tree vt=TREE_TYPE(type);
        if(TREE_CODE(vt)==POINTER_TYPE)
           return getIfaceCommonData123ByPointer(vt);
        else if(TREE_CODE(vt)==RECORD_TYPE)
           return getIfaceCommonData123ByRecord(vt);
    }
    return NULL;
}

static char *getClassName(tree leftOrRightType)
{
      char *className=NULL;
      if(TREE_CODE(leftOrRightType)!=FUNCTION_TYPE && TREE_CODE(leftOrRightType)!=RECORD_TYPE && TREE_CODE(leftOrRightType)!=POINTER_TYPE)
         return 0;
      if(TREE_CODE(leftOrRightType)==FUNCTION_TYPE){
          for (tree al = TYPE_ARG_TYPES (leftOrRightType); al; al = TREE_CHAIN (al)){
              tree type=TREE_VALUE(al);
              if(TREE_CODE(type)!=POINTER_TYPE)
                  return NULL;
              className=class_util_get_class_name(type);
              n_debug("从函数参数据中取第一个参数 第一个参数是:%s\n",className);
              if(className==NULL){
                  className=getIfaceCommonData123(type);
                  if(className==NULL)
                     return NULL;
              }
              break;
          }
      }else if(TREE_CODE(leftOrRightType)==RECORD_TYPE || TREE_CODE(leftOrRightType)==POINTER_TYPE){
             className=class_util_get_class_name(leftOrRightType);
            // printf("从类型pointer_tpe中取类名:%s\n",className);
             if(className==NULL){
                 className=getIfaceCommonData123(leftOrRightType);
                 if(className==NULL)
                    return NULL;
             }
      }
      return className;
}

/**
 * 检查右值是不是函数调用ref()。如果是不要出现警告。
 * ARandom *var=obj->ref();
 */
static nboolean isRefCall(tree callExpr)
{
   tree fn= CALL_EXPR_FN (callExpr);
   if(TREE_CODE(fn)!=COMPONENT_REF){
     return FALSE;
   }
   tree field=TREE_OPERAND(fn,1);
   char *fieldName=IDENTIFIER_POINTER(DECL_NAME(field));
   ClassFunc *classFunc=func_mgr_get_entity_by_mangle(func_mgr_get(),fieldName);
   if(classFunc==NULL)
       return FALSE;
   tree type=TREE_TYPE(callExpr);
   char *sysName=class_util_get_class_name(type);
   nboolean isRootObject=class_mgr_is_root_object(class_mgr_get(),sysName);
   if(!isRootObject)
       return 0;
   ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),sysName);
   NPtrArray *array=func_mgr_get_funcs(func_mgr_get(),className);
   int i;
   for(i=0;i<array->len;i++){
       ClassFunc *item=n_ptr_array_index(array,i);
       if(strcmp(item->orgiName,"ref")==0 && item==classFunc)
           return TRUE;
   }
   return FALSE;
}

static nboolean isRef(tree rhs)
{
    if(TREE_CODE(rhs)==TARGET_EXPR){
       tree init=TREE_OPERAND (rhs,1);
       if(TREE_CODE(init)==BIND_EXPR){
           tree  body=TREE_OPERAND (init, 1);
           if(TREE_CODE(body)==STATEMENT_LIST){
               tree_stmt_iterator i = tsi_last (body);
               tree t = tsi_stmt (i);
               if(TREE_CODE(t)==MODIFY_EXPR){
                   tree callexpr=TREE_OPERAND (t,1);
                   if(TREE_CODE(callexpr)==CALL_EXPR){
                       //printf("call=--TARGET_EXPR-------\n");
                       return isRefCall(callexpr);
                   }
               }
           }
       }
    }else if(TREE_CODE(rhs)==CALL_EXPR){
        //printf("call=----CALL_EXPR-----\n");
       return  isRefCall(rhs);
    }
    return FALSE;

}



/**
 * 清除警告信息。
 * 只针对如下情况:
 * 一、
 * ((debug_AObject *)self)->_Z7AObject10free_childEPN7AObjectE=_Z7ARandom23ARandom_unref_429114846EPN7ARandomE;
 * _Z7AObject10free_childEPN7AObjectE的第一个参数是 AObject *self
 * _Z7ARandom23ARandom_unref_429114846EPN7ARandomE的第一个参数是 ARandom *self
 * 当build_modify_expr时:
 *  警告：assignment to ‘void (*)(debug_AObject *)’ {或称 ‘void (*)(struct _debug_AObject *)’}
 *  from incompatible pointer type ‘void (*)(debug_ARandom *)’ {或称 ‘void (*)(struct _debug_ARandom *)’} [-Wincompatible-pointer-types]
 *
 *  二、
 *  当编译 ARandom var=new$ ARandom();时
 *  警告：传递‘a_object_cleanup_local_object_from_static_or_stack’的第 1 个参数时在不兼容的指针类型间转换 [-Wincompatible-pointer-types]
 *
 *  三、
 *  当编译 getRandom(new$ ARandom());
 *  警告：传递‘a_object_cleanup_nameless_object’的第 1 个参数时在不兼容的指针类型间转换 [-Wincompatible-pointer-types]
 *  getRandom(new$ ARandom());
 *
 */
int  clear_warning_modify(tree lhtype,tree rhs)
{
    char *leftObjectName=NULL;
    char *rightObjectName=NULL;
    n_debug("clear_warning_modify 00");
    aet_print_tree(lhtype);
    n_debug("\n\n");
    aet_print_tree(rhs);
    if(TREE_CODE(lhtype)!=POINTER_TYPE)
       return 0;
    tree lhtype_2=TREE_TYPE (lhtype);
    if(TREE_CODE(lhtype_2)!=FUNCTION_TYPE && TREE_CODE(lhtype_2)!=RECORD_TYPE
            && TREE_CODE(lhtype_2)!=POINTER_TYPE/*匹配AObject **values */)
       return 0;
    leftObjectName=getClassName(lhtype_2);
    n_debug("class_util_erase_warning leftObjectName:%s",leftObjectName);
    if(leftObjectName==NULL)
        return 0;
    if(TREE_CODE(rhs)!=ADDR_EXPR && TREE_CODE(rhs)!=COMPONENT_REF
            && TREE_CODE(rhs)!=NOP_EXPR && TREE_CODE(rhs)!=VAR_DECL
            && TREE_CODE(rhs)!=PARM_DECL && TREE_CODE(rhs)!=TARGET_EXPR
            && TREE_CODE(rhs)!=CALL_EXPR)
         return 0;
    if(TREE_CODE(rhs)==ADDR_EXPR){
       tree funcOrVardecl = TREE_OPERAND(rhs,0);
       if(TREE_CODE(funcOrVardecl)!=FUNCTION_DECL && TREE_CODE(funcOrVardecl)!=VAR_DECL)
         return 0;
    }
    tree rhtype=TREE_TYPE(rhs);
    if(TREE_CODE(rhtype)!=POINTER_TYPE)
         return 0;
    tree rhtype_2=TREE_TYPE (rhtype);
    rightObjectName=getClassName(rhtype_2);
    if(rightObjectName==NULL)
        return 0;

    if(!strcmp(leftObjectName,IFACE_COMMON_STRUCT_NAME) && !strcmp(rightObjectName,IFACE_COMMON_STRUCT_NAME)){
        n_debug("接口关系 00 IfaceCommonData123 ------ %s %s\n",leftObjectName,rightObjectName);
        return 1;
    }else if(!strcmp(leftObjectName,IFACE_COMMON_STRUCT_NAME) && strcmp(rightObjectName,IFACE_COMMON_STRUCT_NAME)){
        n_debug("接口关系 11 IfaceCommonData123 ------ %s %s\n",leftObjectName,rightObjectName);
        ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),rightObjectName);
        return class_info_is_interface(info);
    }else if(strcmp(leftObjectName,IFACE_COMMON_STRUCT_NAME) && !strcmp(rightObjectName,IFACE_COMMON_STRUCT_NAME)){
        n_debug("接口关系 22 IfaceCommonData123 ------ %s %s\n",leftObjectName,rightObjectName);
        ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),leftObjectName);
        return class_info_is_interface(info);
    }else{
           ClassRelationship ship=  class_mgr_relationship(class_mgr_get(), leftObjectName,rightObjectName);
           n_debug("左右关系是 ------ %s %s ship:%d\n",leftObjectName,rightObjectName,ship);
           if(ship==CLASS_RELATIONSHIP_PARENT){
              // printf("左边%s是右边%s的父类。\n",leftObjectName,rightObjectName);
               return 1;
           }else if(ship==CLASS_RELATIONSHIP_OTHER_IMPL){
              // n_debug("右边%s是左边%s接口的实现。\n",rightObjectName,leftObjectName);
               return 1;
           }else if(ship==CLASS_RELATIONSHIP_IMPL){
               location_t loc=EXPR_LOCATION(rhs);
               loc=loc==0?input_location:loc;
                 /*在classcast.c class_cast_in_finish_decl方法中castClassToInterface方法也有同样的判断。*/
               error_at(loc,"不能转化接口%qs到类%qs。",rightObjectName,leftObjectName);
           }else if(ship==CLASS_RELATIONSHIP_CHILD){
               location_t loc=EXPR_LOCATION(rhs);
               loc=loc==0?input_location:loc;
               if(isRef(rhs))
                   return 1;
               //else
                  // warning_at(loc,0,"从父类%s转子类%s",rightObjectName,leftObjectName);
               //n_error("不未实现CLASS_RELATIONSHIP_CHILD---%s\n",get_tree_code_name(TREE_CODE(rhs)));
           }else{
               ClassInfo *leftInfo=class_mgr_get_class_info(class_mgr_get(),leftObjectName);
               if(leftInfo && class_info_is_interface(leftInfo)){
                   ClassInfo *rightInfo=class_mgr_get_class_info(class_mgr_get(),rightObjectName);
                   /*
                    * ARandom *objectVar=new$ ARandom();
                    * RandomGenerator *ifaceVar=(RandomGenerator *)objectVar;
                    * ifaceVar->evaluate();//这句引起
                    * 警告：传递‘ifaceVar->evaluate()’的第 1 个参数时在不兼容的指针类型间转换 [-Wincompatible-pointer-types]
                    * evaluate的型参是:RandomGenerator，实参是:(AObject *)(objectVar->ifaceRandomGenerator2066046634._iface_common_var._atClass123)
                    */
                    if(rightInfo!=NULL && class_info_is_root(rightInfo) && TREE_CODE(rhs)==NOP_EXPR)
                        return 1;
               }
           }
      }
      return 0;
}

