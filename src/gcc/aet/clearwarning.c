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
#include "tree-core.h"
#include "toplev.h"
#include "c-family/c-pragma.h"
#include "c/c-tree.h"
#include "c/c-parser.h"
#include "opts.h"
#include "tree-iterator.h"
#include "c/c-lang.h"
#include "aet-c-parser-header.h"
#include "tree-pretty-print-markup.h"

#include "aetinfo.h"
#include "aetutils.h"
#include "classutil.h"
#include "aetprinttree.h"
#include "classmgr.h"
#include "c-aet.h"
#include "funcmgr.h"
#include "classimpl.h"
#include "funcpointer.h"
#include "genericutil.h"
#include "implicitlycall.h"
#include "aetparser.h"


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
   if(TREE_CODE(leftOrRightType)!=FUNCTION_TYPE
   && TREE_CODE(leftOrRightType)!=RECORD_TYPE
   && TREE_CODE(leftOrRightType)!=POINTER_TYPE)
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
   ClassFunc *classFunc=func_mgr_get_func_by_mangle(func_mgr_get(),fieldName);
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

static int atAetCompare(tree lhtype,tree rhs)
{
   if(aet_parser_get()->isAet){
      int parmNum=0;//出错在第几个参数
      int ok=func_pointer_check_two(lhtype,rhs,&parmNum);
      //printf("比较函数指针的参数======%d %d\n",ok,parmNum);
      if(ok==0)
         return 1;
   }
   return 0;
}

/**
 * 在ic_argpass:参数传递中
 * lhtype 函数的参数类型
 * rhs 实参
 */
static int funcPointer(tree lhtype,tree rhs)
{
    tree lhtype_2=TREE_TYPE (lhtype);
    if(TREE_CODE(lhtype_2)!=FUNCTION_TYPE)
        return 0;
    if(TREE_CODE(rhs)!=ADDR_EXPR && TREE_CODE(rhs)!=NOP_EXPR)
        return 0;
    if(TREE_CODE(rhs)==ADDR_EXPR){
        tree funcdecl = TREE_OPERAND(rhs,0);
        if(TREE_CODE(funcdecl)!=FUNCTION_DECL)
             return 0;
        char *funcName=IDENTIFIER_POINTER(DECL_NAME(funcdecl));
        char   *sysName=func_mgr_get_static_class_name_by_mangle(func_mgr_get(),funcName);
        //printf("说明实参是一个类静态函数:%s %s\n",sysName,funcName);
        if(sysName!=NULL){
            //进这里说明在selectfield.c中已能过了实参的检查，否则不会选取funcName的。
            return 1;
        }else{
            //说明给函数指针赋值的是一个外部函数，或一个结构体的field,如果当前是在aet中，等同于类静态函数处理。
            return atAetCompare(lhtype,rhs);
        }
    }else if(TREE_CODE(rhs)==NOP_EXPR){
        tree type=TREE_TYPE(rhs);
        if(TREE_CODE(type)==POINTER_TYPE){
            tree functype=TREE_TYPE(type);
            if(TREE_CODE(functype)==FUNCTION_TYPE){
                n_debug("进这里了------left ACompareDataFunc right ACompareFunc\n");
                return atAetCompare(lhtype,type);
            }
        }
    }
    return 0;

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
 *  四、
 *  调用ref()函数，总是返回AObject 与左值对应不上。但不能报警告。
 *
 *  五、
 *  说明ARandomGenerator是ARandom的儿子，同时也实现了接口RandomGenerator，但它的实现方法在父类ARandom中
 *   *  public$ class$ ARandomGenerator extends$ ARandom implements$ RandomGenerator{
 *  }
 *
 *  public$ class$ ARandom{
 *      public$ auint32 nextInt();
 *  }
 *  ARandom并未实现RandomGenerator的接口，但它的类方法与RandomGenerator接口的一样。如果调用
 *  ARandomGenerator的接口RandomGenerator的方法nextInt，则会调用到ARandom中的方法nextInt
 *  因为在ARandomGenerator初始化中给接口RandomGenerator的方法nextInt赋值的正是ARandom的方法
 *  的nextInt
 *  ((com_ai_probability_RandomGenerator *)&self->ifaceRandomGenerator2066046634)->
 *  _Z15RandomGenerator7nextIntEPN15RandomGeneratorE=((aet_util_ARandom *)self)->_Z7ARandom7nextIntEPN7ARandomE;
 *  所以会出现警告.....
 *  解决：判断当有所在的函数是不是初始化函数，从中找类名。如果类是继承ARandom并实现了接口，可以跳过
 *  这样的警告。
 *
 *  六、
 *  当右值是调用隐藏函数时，返回0
 */

static nboolean isImplicitlyCall(tree call)
{
    if(TREE_CODE(call)!=CALL_EXPR)
        return FALSE;
    tree addrExpr=CALL_EXPR_FN (call);
    if(TREE_CODE(addrExpr)!=ADDR_EXPR)
        return FALSE;
    tree fundecl=TREE_OPERAND (addrExpr, 0);
    if(TREE_CODE(fundecl)!=FUNCTION_DECL)
           return FALSE;
     ClassImpl *impl=class_impl_get();
     ImplicitlyCall *implicityly=impl->implicitlyCall;
     return implicitly_call_is_func_decl(implicityly,fundecl);
}

/**
 * c-typeck.cc 中调用，检查是否是aet类之间的转换。
 * 1.warn
 * -1 error
 * 0 ok
 */
enum impl_conv {
  ic_argpass,
  ic_assign,
  ic_init,
  ic_init_const,
  ic_return,
  ic_unknow
};

static int  matchTree(tree lhtype,tree rhs,tree origRhType,int errortype)
{
   char *leftObjectName=NULL;
   char *rightObjectName=NULL;
   //如果右值是调用隐藏函数时，返回0
   if(isImplicitlyCall(rhs)){
      n_debug("如果右值是调用隐藏函数时，返回1\n");
      return 1;
   }

   if(TREE_CODE(lhtype)!=POINTER_TYPE)
      return 0;
   tree lhtype_2=TREE_TYPE (lhtype);
   if(TREE_CODE(lhtype_2)!=FUNCTION_TYPE && TREE_CODE(lhtype_2)!=RECORD_TYPE
   && TREE_CODE(lhtype_2)!=POINTER_TYPE/*匹配AObject **values */)
      return 0;
   leftObjectName=getClassName(lhtype_2);
   n_debug("class_util_erase_warning leftObjectName:%s",leftObjectName);
   if(leftObjectName==NULL){ //如左值不是类，判断是不是函数指针。
      //按函数指针判断
      n_debug("clear_warning_modify 是不是函数指针\n");
      return funcPointer(lhtype,rhs);
   }
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
         if(isRef(rhs)){
            return 1;
         }else {
            if(errortype==ic_assign && TREE_CODE(origRhType)==POINTER_TYPE
                  && TREE_CODE(lhtype)==POINTER_TYPE
                  && TREE_CODE(TREE_TYPE(origRhType))==FUNCTION_TYPE
                  && TREE_CODE(TREE_TYPE(lhtype))==FUNCTION_TYPE){
               //匹配条件一
               return 1;
            }
         }
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
            tree currentDecl=current_function_decl;//匹配第五种情况
            if(aet_utils_valid_tree(currentDecl)){
               char *funcName=IDENTIFIER_POINTER(DECL_NAME(currentDecl));
               char *sysName=aet_utils_get_sys_name_from_init_method(funcName);
               if(sysName!=NULL){
                  ClassRelationship ship1=  class_mgr_relationship(class_mgr_get(),sysName,rightObjectName);
                  ClassRelationship ship2=  class_mgr_relationship(class_mgr_get(),sysName,leftObjectName);
                  n_free(sysName);
                  //说明sysName是rightObjectName的儿子，同时也实现了接口leftObjectName，但它的实现方法在父类rightObjectName中
                  if(ship1==CLASS_RELATIONSHIP_CHILD && ship2==CLASS_RELATIONSHIP_IMPL){
                     return 1;
                  }
               }
            }
         }
      }
   }
   return 0;
}



/**
 * 如果不是类之间的转换
 * 只支持 void * 转 XXX *
 * 或 XXX * 转 void *
 * 像 float *abc 转 AObject *报错。
 */
static int checktype(location_t loc,tree formulaType,tree actualType,int parmnum,tree rname,int *haveError)
{
   enum tree_code code1=TREE_CODE(formulaType);
   enum tree_code code2=TREE_CODE(actualType);
   if(!(code1==POINTER_TYPE && code2==POINTER_TYPE))
      return 0;
   char *formulaSysName=class_util_get_class_name(formulaType);
   char *actualSysName=class_util_get_class_name(actualType);
   n_debug("clearwqrning.c formulaSysName -- %s %s\n",formulaSysName,actualSysName);
   if(formulaSysName==NULL  && actualSysName!=NULL){
      tree type=TREE_TYPE(formulaType);
      if(type==void_type_node){
         //说明型参是 void *,实参是类的指针，类转成了 void *,可以，系统不再处理
         n_debug("clearwqrning.c 型参是 void *,实参是类的指针，类转成了 void *,可以，系统不再处理 %s %s\n",
               formulaSysName,actualSysName);
         return 1;
      }else{
         n_debug("clearwqrning.c 型参不是类也不是void指针，实参类不能转化 %s\n",
                  formulaSysName,actualSysName);
         *haveError=1;
         return 0;
      }
   }else if(formulaSysName!=NULL  && actualSysName==NULL){
      //型参是类指针，如果实参是void 提针，返回 1
      tree type=TREE_TYPE(actualType);
      if(type==void_type_node){
         //说明实参是 void *
         n_debug("clearwqrning.c 型参是类，实参是void，可以转 %s %s\n",formulaSysName,actualSysName);
         return 1;
      }else{
         if(rname)
            error_at(loc, "passing argument %d of %qE from incompatible pointer type", parmnum, rname);//zclei
         else
            error_at(loc, "passing argument %d of unknown from incompatible pointer type", parmnum);//zclei
         *haveError=1;
         return 0;
      }
   }else if(formulaSysName==NULL || actualSysName==NULL){


      n_debug("clearwqrning.c 不是AET类之间的转化，由系统处理 %s %s\n",formulaSysName,actualSysName);
      aet_print_tree(formulaType);
      aet_print_tree(actualType);
      return 0;
   }else{
      if(!strcmp(formulaSysName,actualSysName)){
         n_debug("实参%s与形参%s一样。",actualSysName,formulaSysName);
         return 1;
      }
      ClassRelationship  ship= class_mgr_relationship(class_mgr_get(),actualSysName,formulaSysName);
      if(ship==CLASS_RELATIONSHIP_CHILD || ship==CLASS_RELATIONSHIP_IMPL){
         n_debug("实参%s是形参%s的子类或接口实现。",actualSysName,formulaSysName);
         return 1;
      }
      n_debug("实参%s与形参%s的关系是:%d。是错的！！！\n",actualSysName,formulaSysName,ship);
      n_debug("再检查一次 。如果actualSysName的父类实现了formulaSysName接口，也认为actualSysName实现了formulaSysName接口:%s %s %d\n",actualSysName,formulaSysName,ship);
      ClassInfo *formulaInfo=class_mgr_get_class_info(class_mgr_get(), formulaSysName);
      ClassInfo *actualInfo=class_mgr_get_class_info(class_mgr_get(), actualSysName);
      if(class_info_is_interface(formulaInfo) && !class_info_is_interface(actualInfo)){
         ClassName *atClass= class_mgr_find_interface(class_mgr_get(), &actualInfo->className,&formulaInfo->className);
         if(atClass!=NULL){
             n_debug("在父类:%s找到了接口的实现。也可能不一定。\n",atClass->sysName,formulaSysName);
            return 1;
         }
      }
      return 0;
   }
}

#define ARGUMENT_SKIP   0
#define ARGUMENT_ERROR -1
#define ARGUMENT_OK     1

int  clear_warning_modify_new(int errtype,location_t loc,tree lhtype,tree rhs,tree rhstype,
      int parmnum,tree rname,nboolean needCheck)
{
   int  ret =  matchTree(lhtype,rhs,rhstype,errtype);
   n_debug("clear_warning_modify_new 00 errtype:%d ret:%d parmnum:%d needCheck:%d\n",errtype,ret,parmnum,needCheck);
   aet_print_tree(lhtype);
   aet_print_tree(rhs);
   aet_print_tree(rhstype);
   if(!needCheck || ret)
      return ret;
   ret = 0;
   pp_markup::element_expected_type e_type (lhtype);
   pp_markup::element_actual_type e_rhstype (rhstype);
   int haveError=0;
   switch(errtype){
      case ic_argpass:
         ret=checktype(loc,lhtype,rhstype,parmnum,rname,&haveError);
         break;
      case ic_assign:
         ret=checktype(loc,lhtype,rhstype,parmnum,rname,&haveError);
         break;
      default:
         break;
   }
   return ret;

}




