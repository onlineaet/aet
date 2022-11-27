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
#include "aetutils.h"
#include "classmgr.h"
#include "aetinfo.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "genericcall.h"
#include "aet-typeck.h"
#include "aet-c-fold.h"
#include "classutil.h"
#include "genericutil.h"
#include "aet-c-parser-header.h"
#include "classimpl.h"
#include "genericexpand.h"


static void genericCallInit(GenericCall *self)
{
	self->genericFunc=generic_func_new();
}

static bool null_pointer_constant_p (const_tree expr)
{
  /* This should really operate on c_expr structures, but they aren't
     yet available everywhere required.  */
  tree type = TREE_TYPE (expr);
  return (TREE_CODE (expr) == INTEGER_CST
	  && !TREE_OVERFLOW (expr)
	  && integer_zerop (expr)
	  && (INTEGRAL_TYPE_P (type)
	      || (TREE_CODE (type) == POINTER_TYPE
		  && VOID_TYPE_P (TREE_TYPE (type))
		  && TYPE_QUALS (TREE_TYPE (type)) == TYPE_UNQUALIFIED)));
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


static tree convertGeneric(location_t loc,tree function,tree realGenericType,tree type,tree val,
		nboolean npc,nboolean excessrecision,nboolean replace)
{
//		printf("convertGeneric 00 打印泛型定义\n");
//	    aet_print_tree(realGenericType);
//		printf("convertGeneric 11 打印等待转化的表达式\n");
//	    aet_print_tree(val);
//		printf("convertGeneric 22 打印泛型的void *类型可能是aet_generic_T，aet_generic_E等\n");
//	    aet_print_tree(type);

	    tree parmval=error_mark_node;
	    tree realParmType=TREE_TYPE(val);
		//printf("convertGeneric 33 --从实参转为用户设的类型:%s\n",get_tree_code_name(TREE_CODE(realParmType)));
		//aet_print_tree(realParmType);
		if(TREE_CODE(realGenericType)==POINTER_TYPE){
			//printf("convertGeneric 44 --泛型定义是一个指针:\n");
			if(TREE_CODE(realParmType)!=POINTER_TYPE){
				inform(loc,"泛型定义为一个指针%qE，但实参%qE类型并不是指针。",realGenericType,realParmType);
				n_warning("泛型定义为一个指针，但实参类型并不是指针。");
				return parmval;
			}else{
				inform(loc,"泛型定义为一个指针%qE，实参%qE类型也是指针。",realGenericType,realParmType);
	            return val;
			}
		}
		if(TREE_CODE(realParmType)==POINTER_TYPE){
			inform(loc,"泛型定义不是指针%qE，但实参%qE类型是指针。直接返回参数:%qE",realGenericType,realParmType,val);
			return val;
		}
		//printf("convertGeneric 77 打印void *\n");

		parmval = generic_conv_convert_argument(loc, function,realGenericType,val,npc,excessrecision,replace);
		if(parmval==error_mark_node)
			return parmval;
		aet_print_tree(parmval);
		//printf("convertGeneric 55 --从用户设的类型转为void* \n");
		parmval = generic_conv_convert_argument (loc, function,type,parmval,npc,excessrecision,replace);
	    return parmval;
}


/**
 * 由funcall调用
 * 在aet-typeck中检查参数时使用。
 * setData(E data)
 * 当没有实例化对象时 globalGenericsDefine是空的，这时传进来的实参类型也必须是E
 * 不要抛出例外。只是选择函数时所用。
 */
tree  generic_call_check_parm(GenericCall *self,location_t loc,tree function,tree origGenericDecl,tree val,
        nboolean npc,nboolean excessrecision,ClassName *globalClassName,GenericModel *globalGenericsDefine)
{
    char *str=NULL;
    tree parmval=error_mark_node;
    tree realGenericType;
    char *funName=IDENTIFIER_POINTER(DECL_NAME(function));
    str=generic_util_get_generic_str(origGenericDecl);
    //printf("generic_call_check_param --00 如果找不到泛型字符,返回str:%s globalClassName:%s function:%p %s\n",str,globalClassName->sysName,function,funName);
    if(str==NULL){
       return parmval;
    }
    GenericModel *funcGen=c_aet_get_func_generics_model(function);
    if(globalClassName!=NULL && !globalGenericsDefine){
        tree valtype=TREE_TYPE(val);
        bool equal=c_tree_equal (origGenericDecl,valtype);
        char *varStr=generic_util_get_generic_str(valtype);
        n_info("generic_call_check_param --11 在类实现中调用带有泛型参数的方法。这时类没有实例化。所以无泛型定义。%s 相等吗：？:%d %s",str,equal,varStr);
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
        //printf("generic_call_check_param --33 generic_call_check_parm 中对象有泛型定义。\n");
       if(!genericParm->isDefine){
          // printf("generic_call_check_param --44 中对象有泛型定义, 但定义的是泛型通用的字符:%s\n",genericParm->name);
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
          // printf("generic_call_check_param --66 真正的泛型定义类型如下：参数是不是aet_void_E等：%s\n",parmTypeStr);
           if(parmTypeStr!=NULL && generic_util_is_generic_ident(parmTypeStr)){
                char *typeName=generic_util_get_type_str(genericParm->decl);
                error_at(loc,"类%qs定义的泛型是%qs，但参数是%qs,不匹配。",globalClassName?globalClassName->userName:"null",typeName,str);
                return parmval;
           }
           realGenericType=TREE_TYPE(genericParm->decl);
           parmval=convertGeneric(loc,function,realGenericType,origGenericDecl,val,npc,excessrecision,FALSE);
           n_free(str);
           return parmval;
       }
    }else{
       //printf("generic_call_check_param --77 泛型函数的泛型定义类型就是实参的类型：\n");
       tree realGenericType=TREE_TYPE(val);
       aet_print_tree(realGenericType);
       parmval=convertGeneric(loc,function,realGenericType,origGenericDecl,val,npc,excessrecision,FALSE);
       n_free(str);
       return parmval;
    }
}

/**
 * 把泛型对应的参数转换后，替换向量中的实参
 */
int  generic_call_replace_parm(GenericCall *self,location_t ploc,tree function,vec<tree, va_gc> *values,ClassName *globalClassName,GenericModel *globalGenericsDefine)
{
    tree  funcType = TREE_TYPE (function);
    int count=0;
    int varargs_p = 1;
    for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
        tree type=TREE_VALUE(al);
        if(type == void_type_node){
            varargs_p=0;
            break;
        }
        count++;
    }
    int tt=type_num_arguments (funcType);
   // printf("generic_call_replace_parm xx 不替换，参数个数不匹配! 形参：%d %d 实参:%d varargs_p:%d %p\n",count,tt,values->length(),varargs_p,values);

    if(count!=values->length() && varargs_p==0){
     // printf("generic_call_replace_parm 00 不替换，参数个数不匹配! 形参：%d 实参:%d\n",count,values->length());
     // printf("generic_call_replace_parm 00 不替换，参数个数不匹配! 形参：%d %d 实参:%d\n",count,tt,values->length());
      return -1;
    }
    int replaceCount=0;
    bool type_generic_overflow_p = false;
    bool type_generic_remove_excess_precision = false;
    const bool type_generic=FALSE;
    tree builtin_typelist = NULL_TREE;
    unsigned int parmnum;
    tree typelist=TYPE_ARG_TYPES (funcType);
    tree typetail, builtin_typetail, val;
    for (typetail = typelist,builtin_typetail = builtin_typelist,parmnum = 0; values && values->iterate (parmnum, &val);++parmnum){
            /* The type of the function parameter (if it was declared with one).  */
           tree type = typetail ? TREE_VALUE (typetail) : NULL_TREE;
            /* The type of the built-in function parameter (if the function
           is a built-in).  Used to detect type incompatibilities in
           calls to built-ins declared without a prototype.  */
           tree builtin_type = (builtin_typetail ? TREE_VALUE (builtin_typetail) : NULL_TREE);
            /* The original type of the argument being passed to the function.  */
           tree valtype = TREE_TYPE (val);
            /* The called function (or function selector in Objective C).  */
           tree rname = function;
           int argnum = parmnum + 1;
           const char *invalid_func_diag;
           /* Set for EXCESS_PRECISION_EXPR arguments.  */
           bool excess_precision = false;
           /* The value of the argument after conversion to the type
            of the function parameter it is passed to.  */
           tree parmval;


//          printf("generic_conv_replace_param 44 进入循环 parmnum:%d type:%p type:%s \n",
//                     parmnum,type,(type  && type!=error_mark_node)?get_tree_code_name(TREE_CODE(type)):"null");
//          printf("generic_conv_replace_param 44-- 进入循环 parmnum:%d val %p val:%s \n",
//                             parmnum,val,(val!=NULL_TREE && val  && val!=error_mark_node)?get_tree_code_name(TREE_CODE(val)):"null");
//          printf("generic_conv_replace_param 44--xx 进入循环  val %p reftype:%p \n",val,TREE_TYPE(val));

           if (type == void_type_node){
              printf("generic_call_replace_parm 11 进入循环 错误 type == void_type_node 直接返回 parmnum:%d\n",parmnum);
              return  -1 ;
           }
           if (builtin_type == void_type_node){

              //printf("generic_conv_replace_param 66 进入循环 错误 builtin_type == void_type_node parmnum:%d  %s %s %d\n",
                //             parmnum,__FILE__,__FUNCTION__,__LINE__);
              builtin_typetail = NULL_TREE;
           }


          /* Determine if VAL is a null pointer constant before folding it.  */
           bool npc = null_pointer_constant_p (val);

              /* If there is excess precision and a prototype, convert once to
             the required type rather than converting via the semantic
             type.  Likewise without a prototype a float value represented
             as long double should be converted once to double.  But for
             type-generic classification functions excess precision must
             be removed here.  */
           if (TREE_CODE (val) == EXCESS_PRECISION_EXPR && (type || !type_generic || !type_generic_remove_excess_precision)){
              val = TREE_OPERAND (val, 0);
              excess_precision = true;
           }
           tree tempval = aet_c_fully_fold (val, false, NULL);
           //printf(" generic_conv_replace_param aet_c_fully_fold 改变了吗? tempval:%p val:%p %d\n",tempval,val,tempval==val);
           val=tempval;
           STRIP_TYPE_NOPS (val);
           val = aet_require_complete_type (ploc, val);
           if(val==error_mark_node)
               return -1;

           if (type != NULL_TREE){
              nboolean isGenericType=generic_util_is_generic_pointer(type);
              n_debug("generic_conv_replace_param 22  parmnum:%d  是不是泛型:%d\n",parmnum,isGenericType);
              if(isGenericType){
                char *str=generic_util_get_generic_str(type);
                n_debug("generic_conv_replace_param 33 泛型声明是: %s\n",str);
                if(str!=NULL){
                    GenericUnit *genericParm=getGenericRealType(globalClassName,globalGenericsDefine,str);
                    if(genericParm && genericParm->isDefine){
                        tree realGenericType=TREE_TYPE(genericParm->decl);
                        parmval=convertGeneric(ploc,function,realGenericType,type,val,npc,excess_precision,TRUE);
                        n_debug("generic_conv_replace_param 44 是一个泛型对象，类型就是对象创建时所定义的类型 %s parmnum:%d ok:%d\n",str,parmnum,parmval!=error_mark_node);
                        if(parmval==error_mark_node)
                            return -1;
                        aet_print_tree(parmval);
                        (*values)[parmnum] = parmval;
                        replaceCount++;
                    }else{
                        GenericModel *funcGen=c_aet_get_func_generics_model(function);
                        if(funcGen){
                           tree realGenericType=TREE_TYPE(val);
                           n_debug("generic_conv_replace_param ----realGenericType。");
                           parmval=convertGeneric(ploc,function,realGenericType,type,val,npc,excess_precision,TRUE);
                           n_debug("generic_conv_replace_param 55 是一个泛型函数，类型就用实参的类型 %s parmnum:%d ok:%d\n",str,parmnum,parmval!=error_mark_node);
                           if(parmval==error_mark_node)
                                return -1;
                           (*values)[parmnum] = parmval;
                           replaceCount++;
                        }
                    }
                    n_free(str);
                }
              }
           }
           n_debug("generic_conv_replace_param 66 进入循环  结束一次循环 typetail:%p builtin_typetail:%p parmnum:%d\n",
                   typetail, builtin_typetail,parmnum);
           if (typetail)
              typetail = TREE_CHAIN (typetail);
           if (builtin_typetail)
              builtin_typetail = TREE_CHAIN (builtin_typetail);
    }//end for

    return replaceCount;
}



/**
 * 当AObject *var=new$ AObject<int>();
 * var->setData(15);
 * 是一个组件引用 componentref，在var中 struct lang_type 保存有泛型的定义int
 * 或：
 * self->var->setData(15)
 * 从INDIRECT_REF中找到COMPONENT_REF
 */
GenericModel * generic_call_get_generic_from_component_ref(GenericCall *self,tree componentRef)
{
   if(TREE_CODE (componentRef)!=COMPONENT_REF)
	   return NULL;
   tree indirect=TREE_OPERAND(componentRef,0);
   if(TREE_CODE (indirect)!=INDIRECT_REF && TREE_CODE (indirect)!=VAR_DECL)
	   return NULL;
   tree type=TREE_TYPE(indirect);
   if(TREE_CODE(type)!=RECORD_TYPE)
	   return NULL;
   tree var=NULL_TREE;
   if(TREE_CODE (indirect)==VAR_DECL)
	   var=indirect;
   else
	   var=TREE_OPERAND(indirect,0);
   if(TREE_CODE(var)!=VAR_DECL && TREE_CODE(var)!=COMPONENT_REF)
  	   return NULL;
   if(TREE_CODE(var)==COMPONENT_REF){
	   tree fieldDecl=TREE_OPERAND(var,1);
	   var=fieldDecl;
   }
   char *sysName=class_util_get_class_name(TREE_TYPE(var));
   ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
   if(info==NULL)
	   return NULL;
   if(TREE_CODE(var)==VAR_DECL || TREE_CODE(var)==FIELD_DECL){
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



static char *createCodes()
{
	  NString *codes=n_string_new("");
	  n_string_append_printf(codes,"({ %s info;\n",AET_FUNC_GENERIC_PARM_STRUCT_NAME);
	  n_string_append(codes,"info.count=0;\n");
	  n_string_append(codes,"info.excute=0;\n");
	  n_string_append_printf(codes,"info.globalBlockInfo=&%s;\n",FuncGenParmInfo_NAME);
	  n_string_append_printf(codes,"info.%s=NULL;\n",AET_SET_BLOCK_FUNC_VAR_NAME);
	  n_string_append(codes,"info;\n");
	  n_string_append(codes,"}))\n");
	  char *result=n_strdup(codes->str);
	  n_string_free(codes,TRUE);
	  return result;
}

static char *createZeroCodes()
{
	  NString *codes=n_string_new("");
	  n_string_append_printf(codes,"({ %s info;\n",AET_FUNC_GENERIC_PARM_STRUCT_NAME);
	  n_string_append(codes,"info.count=0;\n");
	  n_string_append(codes,"info.globalBlockInfo=NULL;\n");
	  n_string_append(codes,"info.excute=0;\n");
	  n_string_append(codes,"info;\n");
	  n_string_append(codes,"}))\n");
	  char *result=n_strdup(codes->str);
	  n_string_free(codes,TRUE);
	  return result;
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


/**
 * 比较实参，如果一个泛型对应多个实参。
 * 返回第一个实参的类型，即为这次调用泛型函数的类型。
 */

/**
 * 原来是用程序创建
 * tree target=createFuncGenParmInfo(count,types,ids);
 * ({FuncGenParmInfo info;temp.count=5;&info;})
 * 现改为用代码创建,原因如下:
 * 但是 -O2 优化后 info是乱的，所以加入volatile.
 * 但还是出错，不了解如何给变量加修饰 volatile ，改为用代码编译
 * 从genericcall.c移走到backupkey.backup
 * 原来的代码移到了backupkey.backup
 */
/**
 * 创建参数FuncGenParmInfo *tempFgpi1234
 * 用的是复合表达式:
 * ({FuncGenParmInfo info;&info;})
 * 如果所在函数中有参数 tempFgpi1234就用，没有，但有_inFileGlobalFuncGenParmInfo就用，最后用空的
 */
static void createFpgiForQueryGen(GenericCall *self,ClassFunc *func,vec<tree, va_gc> *exprlist)
{
	tree id=aet_utils_create_ident(AET_FUNC_GENERIC_PARM_NAME);
	tree tempFgpi1234=lookup_name(id);
	char *currentFunc="不在函数体内";
	tree current=current_function_decl;
	if(current){
		currentFunc=IDENTIFIER_POINTER(DECL_NAME(current));
	}
	if(aet_utils_valid_tree(tempFgpi1234)){
	    unsigned long pre=exprlist;
        nboolean room=vec_safe_space (exprlist, 1) ;

		 n_debug("createFpgiForQueryGen------- 00 所在函数有参数 tempFgpi1234 %s %s %s exprlist:%p %d space:%d\n",
		         func->orgiName,in_fnames[0],currentFunc,exprlist,exprlist->length(),room);
		 vec_safe_insert(exprlist,1,tempFgpi1234);

	       unsigned long next=exprlist;


	       n_debug("createFpgiForQueryGen------- xxx00 所在函数有参数 tempFgpi1234 %s %s %s exprlist:%p %d\n",
		                 func->orgiName,in_fnames[0],currentFunc,exprlist,exprlist->length());

	           if(next!=pre){
	               n_error("vec_safe_insert(exprlist,1,tempFgpi1234); 改变了 exprlist\n");
	           }
	}else{
		tree id=aet_utils_create_ident(FuncGenParmInfo_NAME);
		tree _inFileGlobalFuncGenParmInfo=lookup_name(id);
		if(!aet_utils_valid_tree(_inFileGlobalFuncGenParmInfo)){
		    n_debug("createFpgiForQueryGen------- 11 创建一个空的 FuncGenParmInfo %s %s %s \n",func->orgiName,in_fnames[0],currentFunc);
		   char *codes=createZeroCodes();
		   tree target=generic_util_create_target(codes);
		   vec_safe_insert(exprlist,1,target);
	   }else{
		    n_debug("createFpgiForQueryGen------- 22 用本文件中全局变量 _inFileGlobalFuncGenParmInfo %s %s %s \n",func->orgiName,in_fnames[0],currentFunc);
		 	char *codes=createCodes();
		 	n_debug("createFpgiForQueryGen --- %s\n",codes);
			tree target=generic_util_create_target(codes);
			vec_safe_insert(exprlist,1,target);
			n_free(codes);
	   }
   }
}

/**
 * 加参数FuncGenParmInfo tempFgpi1234到函数调用中
 * 1.如果是泛型函数但没有问号参数
 * 2.不是泛型函数，但有问号参数。
 */
void  generic_call_add_fpgi_parm(GenericCall *self,ClassFunc *func,ClassName *className,vec<tree, va_gc> *exprlist,GenericModel *funcGenericDefine)
{
	nboolean isFuncGen=class_func_is_func_generic(func);
	nboolean isQueryGen=class_func_is_query_generic(func);
	generic_expand_create_fggb_var_and_setblock_func(generic_expand_get());//生成static FuncGenParmInfo   _inFileGlobalFuncGenParmInfo={0};
	if(isFuncGen && !isQueryGen){
		n_debug("generic_call_add_fpgi_parm 00 是泛型函数但没有问号参数。 %s genModel:%s\n",func->orgiName,generic_model_tostring(funcGenericDefine));
		generic_func_add_fpgi_actual_parm(self->genericFunc,func,className,exprlist,funcGenericDefine);
	}else if(!isFuncGen && isQueryGen){
	    n_debug("generic_call_add_fpgi_parm 11 不是泛型函数，但有问号参数。 %s genModel:%s\n",func->orgiName,generic_model_tostring(funcGenericDefine));
		createFpgiForQueryGen(self,func,exprlist);
	}else{
	    n_warning("generic_call_add_fpgi_parm 22 检查参数中的泛型是不是与调用者的泛型相同。 %s genModel:%s\n",func->orgiName,generic_model_tostring(funcGenericDefine));
	}
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



