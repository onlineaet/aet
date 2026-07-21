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
#include "tree.h"
#include "stringpool.h"
#include "attribs.h"
#include "toplev.h"
#include "asan.h"
#include "c-family/c-pragma.h"
#include "c/c-tree.h"
#include "opts.h"
#include "vec.h"
#include "c/c-parser.h"
#include "tree-iterator.h"
#include "fold-const.h"
#include "langhooks.h"

#include "aet-c-parser-header.h"
#include "aetutils.h"
#include "classmgr.h"
#include "aetinfo.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "genericcall.h"
#include "classutil.h"
#include "genericutil.h"
#include "classimpl.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "aetprinttoken.h"
#include "genericgraph.h"


static void genericCallInit(GenericCall *self)
{
}


/* class$ Hello<E> {
*	 E getData(E work);
* };
 *impl$ Hello{
 *   E getData(E work){
 *   	return NULL;
 *   }
*	public$  Hello() {
*		AObject *obj=NULL;
*		E dd=getData(obj); //这里不能编译通过，因为参数 E work不知道 AObject *obj 是什么。
*	}
 *};*/

static GenericUnit *getGenericRealType(ClassName *className,GenericModel *genericsdefine,char *genericStr)
{
    if(!genericsdefine)
    	return NULL;
    ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
    if(info==NULL)
    	return NULL;
    int index=class_info_get_generic_index(info,genericStr);
   // printf("getGenericRealType 00 index:%d class:%s genericStr:%s\n",index,className->sysName,genericStr);
    if(index<0)
    	return NULL;
    return genericsdefine->genUnits[index];
}

/**
 * 内部检查是实参否能转化到形参的类型。
 */
static tree internelConvert(tree realGenericType,tree realParmType,tree val)
{
   enum tree_code codel= TREE_CODE(realGenericType);
    enum tree_code coder = TREE_CODE (realParmType);
    tree rhstype = realParmType;
    tree rhs = val;
    if (coder == ERROR_MARK)
       return error_mark_node;
    if (TYPE_MAIN_VARIANT (realGenericType) == TYPE_MAIN_VARIANT (rhstype)){
       return val;
    }
    if (coder == VOID_TYPE){
       return error_mark_node;
    }

   if (codel == REFERENCE_TYPE && coder != REFERENCE_TYPE){
        if (!lvalue_p (rhs)){
           return error_mark_node;
        }
        if (!c_mark_addressable (rhs))
           return error_mark_node;
        n_debug("泛型参数检查 left=REFERENCE_TYPE coder != REFERENCE_TYPE");
        return rhs;
     }
     /* Some types can interconvert without explicit casts.  */
     else if (codel == VECTOR_TYPE && coder == VECTOR_TYPE
     && vector_types_convertible_p (realGenericType, realParmType, true)){
        n_debug("泛型参数检查 left=VECTOR_TYPE right == VECTOR_TYPE\
               vector_types_convertible_p (realGenericType, realParmType, true)=true");
        return rhs;
     /* Arithmetic types all interconvert, and enum is treated like int.  */
     }else if ((codel == INTEGER_TYPE || codel == REAL_TYPE
     || codel == FIXED_POINT_TYPE
     || codel == ENUMERAL_TYPE || codel == COMPLEX_TYPE
     || codel == BOOLEAN_TYPE || codel == BITINT_TYPE)
     && (coder == INTEGER_TYPE || coder == REAL_TYPE
     || coder == FIXED_POINT_TYPE
     || coder == ENUMERAL_TYPE || coder == COMPLEX_TYPE
     || coder == BOOLEAN_TYPE || coder == BITINT_TYPE)){
        n_debug("泛型参数检查 left=数字 coder == 数字");
        return rhs;
     }
}

/**
 * setData(E value)
 * E value实际编译为 void *value 这里void *=aet_void_E
 * 当对象的泛型定义为int 当实参是5，5必须翻译为
 */
static tree convertGeneric_1(tree realGenericType,location_t ploc, tree function, tree fundecl,
                        tree type, tree origtype, tree val, tree valtype,
                        bool npc, tree rname, int parmnum, int argnum,
                        bool excess_precision, int warnopt,nboolean replace)
{
//   printf("convertGeneric 00 打印泛型定义\n");
//   aet_print_tree(realGenericType); //泛型定义
//   printf("convertGeneric 11 打印等待转化的表达式\n");
//   aet_print_tree(val); //实参
//   printf("convertGeneric 22 打印泛型的void *类型可能是aet_generic_T，aet_generic_E等\n");
//   aet_print_tree(type);//函数声明的类型setdata(E data) E 是 aet_generic_E void *
   location_t loc=ploc;
   tree parmval=error_mark_node;
   tree realParmType=valtype;//实参类型
   //printf("convertGeneric 33 --从实参转为用户设的类型:%s\n",get_tree_code_name(TREE_CODE(realParmType)));
   //aet_print_tree(realParmType);
   if(TREE_CODE(realGenericType)==POINTER_TYPE){
      //printf("convertGeneric 44 --泛型定义是一个指针:\n");
      if(TREE_CODE(realParmType)!=POINTER_TYPE){
         inform(loc,"泛型定义为指针%qE，但实参%qE类型并不是指针。",realGenericType,realParmType);
         n_warning("泛型定义为指针，但实参类型并不是指针。");
         return parmval;
      }else{
         //inform(loc,"泛型定义为指针%qE，实参%qE类型也是指针。",realGenericType,realParmType);
         return val;
      }
   }
   if(TREE_CODE(realParmType)==POINTER_TYPE){
      error_at(loc,"泛型定义不是指针%qE，但实参%qE类型是指针。直接返回参数:%qE",realGenericType,realParmType,val);
      return val;
   }


   //如果需要转化，再转化一次，否则返回由系统转
   //n_debug("convertGeneric 77 打印void *\n");
   //1.val转定义的泛型(realGenericType=int或其它） 2.转化后的实参再转成type类型 type=void *=a_generic_E

   parmval = aet_convert_argument (ploc, function, fundecl, realGenericType,
         origtype,val, valtype, npc, rname, parmnum, argnum, excess_precision, warnopt);
   if(parmval==error_mark_node)
      return parmval;
   aet_print_tree(parmval);
   parmval= generic_convert(ploc,type,parmval,replace);
   return parmval;

}


/**
 * 由funcall调用
 * 在aet-typeck中检查参数时使用。
 * setData(E data)
 * 当没有实例化对象时 globalGenericsDefine是空的，这时传进来的实参类型也必须是E
 * 不要抛出例外。只是选择函数时所用。
 * 处于函数选择的状态static CheckParamCallback *checkParamCallback！=NULL;
 */
tree generic_call_check_parm(GenericCall *self,location_t ploc, tree function, tree fundecl,
                        tree type, tree origtype, tree val, tree valtype,
                        bool npc, tree rname, int parmnum, int argnum,
                        bool excess_precision, int warnopt,
                        ClassName *globalClassName,GenericModel *globalGenericsDefine)
{
    char *str=NULL;
    tree origGenericDecl=type;
    location_t loc=ploc;
    tree parmval=error_mark_node;
    tree realGenericType;
    char *funName=IDENTIFIER_POINTER(DECL_NAME(function));
    str=generic_util_get_generic_str(origGenericDecl);
    n_debug("generic_call_check_param 00 如果找不到泛型字符,返回str:%s globalClassName:%s function:%p %s\n",
            str,globalClassName->sysName,function,funName);
    aet_print_tree(origGenericDecl);
    if(str==NULL){
       return parmval;
    }
    GenericModel *funcGen=c_aet_get_func_generics_model(function);
    if(globalClassName!=NULL && !globalGenericsDefine){
        tree valtype=TREE_TYPE(val);
        bool equal=c_tree_equal (origGenericDecl,valtype);
        char *varStr=generic_util_get_generic_str(valtype);
        n_info("generic_call_check_param 11 在类实现中调用带有泛型参数的方法。这时类没有实例化。所以无泛型定义。%s 相等吗：？:%d %s",
              str,equal,varStr);
        if(equal && varStr!=NULL && !strcmp(str,varStr)){
             n_free(str);
             n_free(varStr);
             return val;
        }else if(equal && varStr!=NULL && strcmp(str,varStr)){
             n_free(str);
             n_free(varStr);
             return val;
        }else{
            // printf("generic_call_check_parm --- %s funName:%s 泛型函数的声明:%s\n",str,funName,generic_model_tostring(funcGen));
             nboolean ok=generic_model_exits_ident(funcGen,str);
            // printf("generic_call_check_param --22 在泛型函数%s中找:%s ok:%d\n",funName,str,ok);
             if(!ok){
                n_free(str);
//              Multiple markers at this line
//                  - aaxx cannot be resolved to a variable
//                  - The method put(T) in the type Hello2<T> is not applicable for the arguments
//                   (String)
               // error_at(loc,"在类%qs中的方法%qs不能使用参数%qE。",globalClassName?globalClassName->userName:"null",funName,valtype);
                n_warning("在类%s中的方法%s不能使用参数。",globalClassName?globalClassName->userName:"null",funName);
                return parmval;
             }
        }
    }
    //genericParm是Abc<int>中的int或者是 Abc<E>中的E或者是泛型函数中的T
    GenericUnit *genericParm=getGenericRealType(globalClassName,globalGenericsDefine,str);
    if(genericParm){
        n_debug("generic_call_check_param --33 generic_call_check_parm 中对象有泛型定义。\n");
       if(!genericParm->isDefine){
          n_debug("generic_call_check_param --44 中对象有泛型定义, 但定义的是泛型通用的字符:%s\n",genericParm->name);
          tree valtype=TREE_TYPE(val);
          char *parmStr=generic_util_get_generic_str(valtype);
          char *rg=genericParm->name;
          n_info("generic_call_check_param --55 函数参数是泛型:%s 实参是:%s",parmStr,rg);
          if(parmStr && !strcmp(rg,parmStr)){
             n_free(str);
             n_free(parmStr);
             return val;
          }else{
             n_free(str);
             //error_at(loc,"类%qs中的方法%qs的泛型参数是%qs,但传递的实参类型不匹配。",globalClassName?globalClassName->userName:"null",funName,rg);
             n_warning("类%s中的方法%s的泛型参数是%s,但传递的实参类型不匹配。",globalClassName?globalClassName->userName:"null",funName,rg);
             return parmval;
          }
       }else{
           char *parmTypeStr=generic_util_get_type_str(val);
           n_debug("generic_call_check_param --66 真正的泛型定义类型如下：参数是不是aet_void_E等：%s\n",parmTypeStr);
           if(parmTypeStr!=NULL && generic_util_is_generic_ident(parmTypeStr)){
                char *typeName=generic_util_get_type_str(genericParm->decl);
                error_at(loc,"类%qs定义的泛型是%qs，但参数是%qs,不匹配。",
                      globalClassName?globalClassName->userName:"null",typeName,str);
                return parmval;
           }
           realGenericType=TREE_TYPE(genericParm->decl);
           parmval=convertGeneric_1(realGenericType,ploc, function,  fundecl,type,  origtype,  val,
                             valtype, npc,rname,parmnum,argnum, excess_precision,warnopt,FALSE);
           n_free(str);
           return parmval;
       }
    }else{
       //printf("generic_call_check_param --77 泛型函数的泛型定义类型就是实参的类型：\n");
       tree realGenericType=TREE_TYPE(val);
       aet_print_tree(realGenericType);
       parmval=convertGeneric_1(realGenericType,ploc,  function,  fundecl,type,  origtype,  val,
               valtype, npc,  rname,  parmnum,  argnum, excess_precision,  warnopt,FALSE);
       n_free(str);
       return parmval;
    }
}

/**
 * 把泛型对应的参数转换后，替换向量中的实参
 */
tree generic_call_replace_parm(GenericCall *self,location_t ploc, tree function, tree fundecl,
                        tree type, tree origtype, tree val, tree valtype,
                        bool npc, tree rname, int parmnum, int argnum,
                        bool excess_precision, int warnopt,ClassName *globalClassName,GenericModel *globalGenericsDefine)
{
    tree parmval=NULL_TREE;
    nboolean isGenericType=generic_util_is_generic_pointer(type);
    n_debug("generic_conv_replace_param 22  parmnum:%d  是不是泛型:%d\n",parmnum,isGenericType);
    aet_print_tree(type);
    if(!isGenericType){
        error_at(ploc,"不是一个泛型参数%qE",type);
        return error_mark_node;
    }
    char *str=generic_util_get_generic_str(type);
    n_debug("generic_conv_replace_param 33 泛型声明是: %s\n",str);
    if(str!=NULL){
        GenericUnit *genericParm=getGenericRealType(globalClassName,globalGenericsDefine,str);
        if(genericParm && genericParm->isDefine){
            tree realGenericType=TREE_TYPE(genericParm->decl);
            //parmval=convertGeneric(ploc,function,realGenericType,type,val,npc,excess_precision,TRUE);
            parmval=convertGeneric_1(realGenericType,ploc,  function,  fundecl,type,  origtype,  val,  valtype,
                                    npc,  rname,  parmnum,  argnum, excess_precision,  warnopt,TRUE);
            n_debug("generic_conv_replace_param 44 是一个泛型对象，类型就是对象创建时所定义的类型 %s parmnum:%d ok:%d\n",str,parmnum,parmval!=error_mark_node);
            aet_print_tree(parmval);
        }else{
            GenericModel *funcGen=c_aet_get_func_generics_model(function);
            if(funcGen){
                tree realGenericType=TREE_TYPE(val);
                n_debug("generic_conv_replace_param ----realGenericType。");
                //parmval=convertGeneric(ploc,function,realGenericType,type,val,npc,excess_precision,TRUE);
                parmval=convertGeneric_1(realGenericType,ploc,  function,  fundecl,type,  origtype,  val,  valtype,
                        npc,  rname,  parmnum,  argnum, excess_precision,  warnopt,TRUE);
                n_debug("generic_conv_replace_param 55 是一个泛型函数，类型就用实参的类型 %s parmnum:%d ok:%d\n",str,parmnum,parmval!=error_mark_node);
            }
        }
        n_free(str);
    }
    return parmval;
}

/**
 * 该函数是从componentref找出引用的变量，参数或域中的泛型定义。
 * 1,无，2.有定义 3.只有泛型声明
 * 当AObject *var=new$ AObject<int>();
 * var->setData(15);
 * 是一个组件引用 componentref，在var中 struct lang_type 保存有泛型的定义int
 * 或：
 * self->var->setData(15)
 * 从INDIRECT_REF中找到COMPONENT_REF
 * 或
 * void setData(AArray<int> *ff)
 * ff是一个参数，并且帯有泛型 int
 * bug 096,描述了原来没有处理PARAM_DECL引起的错误。
 */
GenericModel * generic_call_get_generic_from_component_ref(GenericCall *self,tree componentRef)
{
   if(TREE_CODE (componentRef)!=COMPONENT_REF)
	   return NULL;
   tree indirect=TREE_OPERAND(componentRef,0);
   if(TREE_CODE (indirect)!=INDIRECT_REF
         && TREE_CODE (indirect)!=VAR_DECL
         && TREE_CODE (indirect)!=PARM_DECL)
	   return NULL;

   tree type=TREE_TYPE(indirect);
   if(TREE_CODE(type)!=RECORD_TYPE)
	   return NULL;
   tree var=NULL_TREE;
   if(TREE_CODE (indirect)==VAR_DECL || TREE_CODE (indirect)==PARM_DECL)
	   var=indirect;
   else
	   var=TREE_OPERAND(indirect,0);
   if(TREE_CODE(var)!=VAR_DECL && TREE_CODE (var)!=PARM_DECL && TREE_CODE(var)!=COMPONENT_REF)
  	   return NULL;
   if(TREE_CODE(var)==COMPONENT_REF){
	   tree fieldDecl=TREE_OPERAND(var,1);
	   var=fieldDecl;
   }
   char *sysName=class_util_get_class_name(TREE_TYPE(var));
   ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
   if(info==NULL)
	   return NULL;
   if(TREE_CODE(var)==VAR_DECL || TREE_CODE(var)==PARM_DECL || TREE_CODE(var)==FIELD_DECL){
      GenericModel *generic=c_aet_get_generics_model(var);
      return generic;
   }else {
	   n_warning("从componentRef找不到泛型");
	   aet_print_tree(var);
	   return NULL;
   }
}

/**
 * 检查返回值是不是一个泛型
 */
static GenericModel *getGenericDefineByCallExpr(tree expr,char **sysName)
{
    if(TREE_CODE(expr)!=CALL_EXPR)
    	return NULL;
    tree callType=TREE_TYPE(expr);
    if(TREE_CODE(callType)!=POINTER_TYPE)
    	return NULL;
    char *genericStr=generic_util_get_generic_str(callType);
    if(genericStr==NULL)
    	return NULL;
   //printf("getGenericDefineByCallExpr 11 %s\n",genericStr);

    //以上是call_expr 类型的判断,下面是调用函数的判断
//#define CALL_EXPR_FN(NODE) TREE_OPERAND (CALL_EXPR_CHECK (NODE), 1)

    tree fn=CALL_EXPR_FN(expr);
  //  printf("getGenericDefineByCallExpr ------11 %s\n",genericStr);
    if(!aet_utils_valid_tree(fn))
    	return NULL;
    int nargs = call_expr_nargs (expr);
    //printf("getGenericDefineByCallExpr 22 %s %s nargs:%d\n",genericStr,get_tree_code_name(TREE_CODE(fn)),nargs);
    if(TREE_CODE(fn)==COMPONENT_REF && nargs>0){
       //第一个参数应该是self,类型是class或interface
        tree arg = CALL_EXPR_ARG (expr, 0);
        aet_print_tree(arg);
        if(TREE_CODE(arg)==VAR_DECL || TREE_CODE(arg)==PARM_DECL || TREE_CODE(arg)==ADDR_EXPR){
        	char *className=class_util_get_class_name(TREE_TYPE(arg));
        	//printf("getGenericDefineByCallExpr 33 %s\n",className);
        	aet_print_tree(arg);
        	if(className!=NULL){
        		GenericModel *genericDefine=c_aet_get_generics_model(arg);
        		*sysName=n_strdup(className);
        		if(TREE_CODE(arg)==ADDR_EXPR && genericDefine==NULL)
        		    genericDefine=c_aet_get_generics_model(TREE_OPERAND (arg, 0));
                //printf("getGenericDefineByCallExpr 44 %s %p\n",className,genericDefine);

        		return genericDefine;
        	}
        }
     }
     return NULL;
}

/**
 * 是不是一个返回值是泛型的函数调用。
 */
static char *getGenericReturnStr(tree expr)
{
   if(TREE_CODE(expr)!=CALL_EXPR)
      return NULL;
   tree callType=TREE_TYPE(expr);
   if(TREE_CODE(callType)!=POINTER_TYPE)
      return NULL;
   char *genericStr=generic_util_get_generic_str(callType);
   if(genericStr==NULL)
      return NULL;
   return genericStr;
}


tree generic_call_convert_generic_to_user(GenericCall *self,tree expr)
{
   char *genericStr=getGenericReturnStr(expr);
   if(genericStr==NULL)
      return expr;
   char *sysClassName=NULL;
   GenericModel *genDefine=getGenericDefineByCallExpr(expr,&sysClassName);
   if(!genDefine)
      return expr;
   ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(), sysClassName);
   if(className==NULL)
      return expr;
   GenericUnit *realGen=getGenericRealType(className,genDefine,genericStr);
   if(realGen==NULL)
      return expr;
   n_debug("generic_call_convert_generic_to_user %s\n",generic_unit_tostring(realGen));
   aet_print_tree(realGen->decl);
   if(generic_util_valid_by_str(genericStr) && generic_unit_is_undefine(realGen)){
      return expr;
   }
   char *funName=IDENTIFIER_POINTER(DECL_NAME(current_function_decl));
   if(TREE_CODE(realGen->decl)==POINTER_TYPE || TREE_CODE(TREE_TYPE(realGen->decl))==POINTER_TYPE){
      n_debug("generic_conv_convert_pointer_to_user_by_call_expr 11 转指针 %s\n",genericStr);
      //		tree pointer=build_pointer_type(long_unsigned_type_node);
      //		tree ret = build1 (NOP_EXPR, pointer,expr);
      //		ret = build1 (INDIRECT_REF, long_unsigned_type_node, ret);
      //		ret = build1 (CONVERT_EXPR, TREE_TYPE(realGen->decl), ret);
      //		return ret;
      /*把返回值的类型是 aet_generic_E也就是(void *)换成用户声明的泛型*/
      tree pointer=build_pointer_type(TREE_TYPE(realGen->decl));
      TREE_TYPE(expr)=pointer;
      return expr;
   }else{
      n_debug("generic_conv_convert_pointer_to_user_by_call_expr 22 转非指针 %s\n",genericStr);
      if(generic_unit_is_undefine(realGen)){
         error_at(input_location,"在函数%qs中调用到一个不能进行的转化%qs。",funName,realGen->name);
         return expr;
      }
      tree realType=TREE_TYPE(realGen->decl);
      tree pointer=build_pointer_type(realType);
      tree ret = build1 (NOP_EXPR, pointer,expr);
      ret = build1 (INDIRECT_REF, realType, ret);
      return ret;
   }
}

//static void test_print_exprlist(vec<tree, va_gc> *exprlist)
//{
//        int ix;
//        tree arg;
//        int count=0;
//        for (ix = 0; exprlist->iterate (ix, &arg); ++ix){
//            printf("print_exprlist -- %d\n",count++);
//            aet_print_tree(arg);
//        }
//}



nboolean  generic_call_check(GenericCall *self,ClassFunc *func,
      ClassName *className,vec<tree, va_gc> *exprlist,GenericModel *funcGenericDefine,location_t loc)
{
   nboolean isFuncGen=class_func_is_func_generic(func);
   nboolean isQueryGen=class_func_have_query_param(func);
   if(isFuncGen && !isQueryGen){
      n_debug("generic_call_check 00 是泛型函数但没有问号参数。 %s genModel:%s\n",
            func->orgiName,generic_model_tostring(funcGenericDefine));
      return generic_func_check(generic_func_get(),func,className,exprlist,funcGenericDefine);
   }else if(!isFuncGen && isQueryGen){
      n_debug("generic_call_check 11 不是泛型函数，但有问号参数。 %s genModel:%s\n",
            func->orgiName,generic_model_tostring(funcGenericDefine));
      return TRUE;
   }else{
      n_warning("generic_call_check 22 检查参数中的泛型是不是与调用者的泛型相同。 %s genModel:%s\n",
            func->orgiName,generic_model_tostring(funcGenericDefine));
   }
   return TRUE;
}

/**
 * 调用泛型函数时，源代码中定义的变量保存在线程变量 _gen_func_block_addr_128347 中。
 * self->call<int>(5)
 * int 是泛型单元,保存在_gen_func_block_addr_128347中
 */
static void createModifyGenCodes(NString *codes,int i,char *varName,RunGenericInfo *info)
{
   char name[128];
   sprintf(name,"%s.%s[%d]",varName,AET_GENERIC_ARRAY,i);

   if(info->from==0){ //来自对象本身的定义
      GenericUnit *genUnit = info->genUnit;
      n_string_append_printf(codes,"strcpy(%s.typeName,\"%s\");\n",name,genUnit->name==NULL?"":genUnit->name);
      n_string_append_printf(codes,"%s.genericName=-1;\n",name);
      n_string_append_printf(codes,"%s.type=%d;\n",name,genUnit->genericType);
      n_string_append_printf(codes,"%s.pointerCount=%d;\n",name,genUnit->pointerCount);
      n_string_append_printf(codes,"%s.size=%d;\n",name,genUnit->size);
   }else if(info->from==1){ //来自泛型函数
      n_string_append_printf(codes,"memcpy(&(%s),&%s.%s[%d],sizeof(%s));\n",name,
      AET_GENERIC_FUNC_THREAD_BLOCK_ADDR,AET_GENERIC_ARRAY,info->fromPos,AET_GENERIC_INFO_STRUCT_NAME);
   }else { //来自self
      n_string_append_printf(codes,"memcpy(&(%s),&self->%s[%d],sizeof(%s));\n",name,
          AET_GENERIC_ARRAY,info->fromPos,AET_GENERIC_INFO_STRUCT_NAME);
   }
}

static int backupToken(c_parser *parser,c_token *backups)
{
     int tokenCount=parser->tokens_avail;
     int i;
     c_token *token;
     for(i=0;i<tokenCount;i++){
        token=c_parser_peek_token (parser);
        aet_utils_copy_token(token,&backups[i]);
        aet_print_token(token);

       c_parser_consume_token (parser);
     }
     return tokenCount;
}

static void restore(c_parser *parser,c_token *backups,int count)
{
     if(count==0)
         return;
     int tokenCount=parser->tokens_avail;
     if(tokenCount+count>AET_MAX_TOKEN){
         error("token太多了");
         return;
     }
     int i;
      for(i=0;i<count;i++){
         aet_utils_copy_token(&backups[i],&parser->tokens[i+tokenCount]);
     }
     parser->tokens_avail=tokenCount+count;
     aet_print_token_in_parser("generic_call restore ------");
}



//源代码：obj->setdata<int>()
//参数:call 源代码 setdata() 生成的函数调用
//该功能主要完成:
//1.检查调用泛型函数的环境是否正确，包括泛型模型检查，如果未定义泛型单元，调用函数是不是在类实现中。
//2.生成RunGenericInfo **
//3.给线程变量_gen_func_block_addr_128347 赋值(通过生成源代码的方式)。
//(1)保留老的变量值 创建以下代码生成bind_expr
//{
//   AetGenericFuncInfo save=_gen_func_block_addr_128347;
//   strcpy(_gen_func_block_addr_128347._generic_1234_array[0].typeName,"int");
//   _gen_func_block_addr_128347._generic_1234_array[0].genericName=-1;
//   _gen_func_block_addr_128347._generic_1234_array[0].type=5;
//   _gen_func_block_addr_128347._generic_1234_array[0].pointerCount=0;
//   _gen_func_block_addr_128347._generic_1234_array[0].size=4;
//   save;  无返回值的函数调用生成 _gen_func_block_addr_128347=save;
//}
//(2)用上面生成的bind_expr生成target_expr。通过删除最后一条语句 save;生成赋值语句 returnVar=save 并加入到bind_expr最后
//完成的备份老的 _gen_func_block_addr_128347 内容，并重新给 _gen_func_block_addr_128347赋值。
//      tree stmt1= build2 (MODIFY_EXPR, type, lhs, rhs);
//(3)如果call有返回值
//int rxy=({ AetGenericFuncInfo old_gen_func_addr=target_expr;
// return_type wyc=call;_gen_func_block_add_128347=old_gen_func_addr;wyc;});

tree  generic_call_build_call(GenericCall *self,ClassFunc *func,GenericModel *funcGenericDefine,location_t loc,tree call)
{
   AetParser *aetParser=aet_parser_get();
   if(funcGenericDefine==NULL){
      error_at(input_location,"没有为泛型函数%qs定义泛型。",func->orgiName);
      return NULL;
   }
   int undefine=generic_model_get_undefine_count(funcGenericDefine);
   if(undefine>0){ //有未定义的泛型
      if(!aetParser->isAet){
         error_at(input_location,"有未定义的泛型类型的泛型函数%qs，只能在类中使用。",func->orgiName);
         return NULL;
      }
   }

   tree currentFunc=current_function_decl;
   char *currentFuncName=IDENTIFIER_POINTER(DECL_NAME(currentFunc));
   ClassName *atClassName=class_impl_get()->className;
   ClassInfo *atInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),atClassName);
   ClassFunc *atFunc=func_mgr_get_entity(func_mgr_get(),atClassName, currentFuncName);
   if(undefine>0){ //有未定义的泛型
      if(atFunc==NULL){
         error_at(input_location,"只能在类函数中调用有未定义泛型的泛型函数。");
         return NULL;
      }
   }

   int errorUnit=-1;
   RunGenericInfo **infos=generic_impl_collect_info(generic_impl_get(),funcGenericDefine,atFunc,atInfo,&errorUnit);
   if(errorUnit>0){
      GenericUnit *unit=generic_model_get(funcGenericDefine,errorUnit);
      error_at(unit->loc,"未知的类型%qs。",unit->name);
   }
   int destCount=generic_model_get_count(funcGenericDefine);
   int i;
   //函数返回值
   tree type;
    if(func->fieldDecl){
       type=TREE_TYPE(func->fieldDecl);//函数指针 pointer
       type = TREE_TYPE(type); //function_type
       type = TREE_TYPE(type);// type
    }else{
       type=TREE_TYPE(func->fromImplDefine);
       type = TREE_TYPE(type);// type
    }

   NString *codes=n_string_new("");
   n_string_append(codes,"{\n");
   n_string_append_printf(codes,"AetGenericFuncInfo save=%s;\n",AET_GENERIC_FUNC_THREAD_BLOCK_ADDR);
   for(i=0;i<destCount;i++){
      RunGenericInfo *item=infos[i];
      createModifyGenCodes(codes,i,AET_GENERIC_FUNC_THREAD_BLOCK_ADDR,item);
   }
   n_string_append_printf(codes,"%s.unitCount=%d;\n",AET_GENERIC_FUNC_THREAD_BLOCK_ADDR,destCount);
   //泛型函数所在的类名
   char *sysName=func->className->sysName;
   n_string_append_printf(codes,"strcpy(%s.sysName,\"%s\");\n",AET_GENERIC_FUNC_THREAD_BLOCK_ADDR,sysName);
   if(type==void_type_node){ //调用的函数无返回值,直接恢复 AET_GENERIC_FUNC_THREAD_BLOCK_ADDR
      n_string_append_printf(codes,"%s=save;}\n",AET_GENERIC_FUNC_THREAD_BLOCK_ADDR);
   }else{
      n_string_append(codes,"save;}\n");
   }
   //用codes代码生成bind_expr 保存老的 _gen_func_block_add_128347 然后给_gen_func_block_add_128347 赋值。
   //printf("generic_call_build_call --- %s\n",codes->str);
   c_parser *parser=aetParser->parser;
   c_token backups[30];
   int backCount=backupToken(parser,backups);
   aet_utils_add_token(parse_in,codes->str,codes->len);
   n_string_free(codes,TRUE);
   location_t endloc;
   tree compound = aet_parser_c_parser_compound_statement/*!c_c_parser_compound_statement*/(aetParser, &endloc);
   restore(parser,backups,backCount);
   //如果调用的泛型函数没有返回值生成bind_expr 在 AET_GENERIC_FUNC_THREAD_BLOCK_ADDR=save前插入原调用函数的语句“call”
   if(type==void_type_node){
      tree stmtListx = BIND_EXPR_BODY(compound);//或者 TREE_OPERAND (compound, 1) //获取bind_expr的stmt_list;
      tree_stmt_iterator iterx = tsi_last (stmtListx);
      tsi_link_before (&iterx, call, TSI_SAME_STMT);
      aet_print_tree(compound);
      generic_graph_add_func_call(generic_graph_get(),infos,func,atFunc,atInfo);
      return compound;
   }

   //用compound创建target_expr
   //({AetGenericFuncInfo save;...save;})
   tree oldIAetGenericFuncInfoTarget;
   tree genFuncInfoRecord=lookup_name(aet_utils_create_ident("AetGenericFuncInfo"));
   {
      tree type = TREE_TYPE(genFuncInfoRecord);
      tree returnVar=build_decl(loc, VAR_DECL, NULL_TREE, type);
      DECL_ARTIFICIAL (returnVar)=1;
      TREE_USED (returnVar)=1;

      tree stmtList = BIND_EXPR_BODY(compound);//或者 TREE_OPERAND (compound, 1) //获取bind_expr的stmt_list;
      tree_stmt_iterator iter = tsi_last (stmtList);
      tree lastStmt = tsi_stmt (iter); //最后个是save nop_expr
      tree save= TREE_OPERAND (lastStmt, 0);
      tsi_delink(&iter);
      tree lhs = returnVar;
      tree rhs=save;
      tree stmt1= build2 (MODIFY_EXPR, type, lhs, rhs);
      append_to_statement_list_force (stmt1, &stmtList);
      oldIAetGenericFuncInfoTarget = build4 (TARGET_EXPR, type, returnVar, compound, NULL_TREE, NULL_TREE);
   }

   //TARGET_EXPR表达式的返回值变量 没有名字
   tree returnVar=build_decl(loc, VAR_DECL, NULL_TREE, type);
   DECL_ARTIFICIAL (returnVar)=1;
   TREE_USED (returnVar)=1;

   //创建有初始值的变量 AetGenericFuncInfo oldAddVar=({AetGenericFuncInfo save=_gen_func_block_add_128347...;save;});
   tree oldAddVar=build_decl(loc, VAR_DECL, aet_utils_create_ident("old_gen_func_addr"), TREE_TYPE(genFuncInfoRecord));
   DECL_CONTEXT(oldAddVar)=current_function_decl;
   TREE_USED (oldAddVar)=1;
   DECL_INITIAL (oldAddVar) = oldIAetGenericFuncInfoTarget;

   //int tempVar=setData/*<int>*/(5);
   tree tempVar=build_decl(loc, VAR_DECL, aet_utils_create_ident("tempVar"), type);
   DECL_CONTEXT(tempVar)=current_function_decl;
   TREE_USED (tempVar)=1;
   DECL_INITIAL (tempVar) = call;

   //bind_expr中的 body
   tree stmtList=alloc_stmt_list();
   //第一条语句 @20     decl_expr        type: @9       addr: 7f7229fd24a0 只要有内部变量就会加一条
   tree stmt0=build1 (DECL_EXPR, void_type_node, oldAddVar);
   append_to_statement_list_force (stmt0, &stmtList);
   //第二条 _gen_func_block_add_128347 = (unsigned long)testdata;
//   tree lhs = genfunAddVar;
//   tree rhs=build_int_cst(long_unsigned_type_node,5);
//   tree stmt1= build2 (MODIFY_EXPR, long_unsigned_type_node, lhs, rhs);
//   append_to_statement_list_force (stmt1, &stmtList);
   //第三条 int wyc=setData/*<int>*/(5);为wyc变量加一条
   tree stmt2=build1 (DECL_EXPR, void_type_node, tempVar);
   append_to_statement_list_force (stmt2, &stmtList);
   //第四条 _gen_func_block_add_128347=oldAddVar;
   tree genfunAddVar=lookup_name(aet_utils_create_ident(AET_GENERIC_FUNC_THREAD_BLOCK_ADDR));
   tree lhs = genfunAddVar;
   tree rhs=oldAddVar;
   tree stmt3= build2 (MODIFY_EXPR, TREE_TYPE(genFuncInfoRecord), lhs, rhs);
   append_to_statement_list_force (stmt3, &stmtList);
   //第五条 wyc;赋值给target_expr的返回值变量(无名)
   lhs = returnVar;
   rhs=tempVar;
   tree stmt4= build2 (MODIFY_EXPR, type, lhs, rhs);
   append_to_statement_list_force (stmt4, &stmtList);

   //oldAddVar是第一个，tempVar是第二个，bindexpr的var是倒序。
   tree bindExpr = build3 (BIND_EXPR, void_type_node, tempVar, stmtList, NULL_TREE);
   tree vars = BIND_EXPR_VARS (bindExpr);
   DECL_CHAIN (oldAddVar) = vars;
   BIND_EXPR_VARS (bindExpr) = oldAddVar;

   tree target = build4 (TARGET_EXPR, type, returnVar, bindExpr, NULL_TREE, NULL_TREE);
   generic_graph_add_func_call(generic_graph_get(),infos,func,atFunc,atInfo);

   return target;
}

GenericCall *generic_call_get()
{
	static GenericCall *singleton = NULL;
	if (!singleton){
		 singleton =n_slice_alloc0 (sizeof(GenericCall));
		 genericCallInit(singleton);
	}
	return singleton;
}



