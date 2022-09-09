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
#include "opts.h"
#include "c-family/c-pragma.h"
#include "c/c-tree.h"
#include "tree-iterator.h"
#include "c/c-parser.h"
#include "fold-const.h"
#include "langhooks.h"
#include "aetutils.h"
#include "classmgr.h"
#include "parserhelp.h"
#include "aetinfo.h"
#include "c-aet.h"
#include "aet-c-parser-header.h"
#include "aetprinttree.h"
#include "aet-typeck.h"
#include "aet-c-fold.h"
#include "classutil.h"
#include "genericutil.h"
#include "funcmgr.h"
#include "classimpl.h"
#include "genericcheck.h"
#include "genericmodel.h"


static GenericModel *getClassGenDecl(char *sysName)
{
    ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
    if(info==NULL ){
	   return NULL;
    }
    return info->genericModel;
}

static GenericModel *getClassGenDefine(tree caller)
{
   GenericModel *trueModel=c_aet_get_generics_model(caller);
   return trueModel;
}

/**
 * 调用泛型函数时定义的泛型存在
 */
static GenericModel *getGenericFuncDefine(tree initOrRhs)
{
	enum tree_code code=TREE_CODE(initOrRhs);
	//获取调用者，如果调用者是A A又属于B 调用变成 B->A->getName
	//收集A的泛型不可能，但可从B所在的文件收集B中的A的泛型是什么
	if(code==COMPONENT_REF){
		tree op0=TREE_OPERAND (initOrRhs, 0);
		if(TREE_CODE(op0)==INDIRECT_REF || TREE_CODE(op0)==VAR_DECL || TREE_CODE(op0)==PARM_DECL ){
			GenericModel *model=c_aet_get_func_generics_model(initOrRhs);
			//printf("getGenericFuncDefine --- %s\n",generic_model_tostring(model));
			return model;

		}else{
			return getGenericFuncDefine(op0);
		}
	}
	return NULL;
}

static GenericModel *getGenericFuncDecl(ClassFunc *func)
{
	  return class_func_get_func_generic(func);
}


static void refLink(tree func,NPtrArray *refArray)
{
	tree op0=TREE_OPERAND (func, 0);
	tree op1=TREE_OPERAND (func, 1);//域成员 函数名或变量名
	if(TREE_CODE(op0)==INDIRECT_REF){ //var->getName();
	   tree component=TREE_OPERAND(op0,0);
	   n_ptr_array_add(refArray,op1);
	   if(TREE_CODE(component)==COMPONENT_REF){
		   refLink(component,refArray);
	   }else{
		   n_ptr_array_add(refArray,component);
	   }
	}else if(TREE_CODE(op0)==VAR_DECL || TREE_CODE(op0)==PARM_DECL){ //var.getName();
		 n_ptr_array_add(refArray,op1);
		 n_ptr_array_add(refArray,op0);
	}else if(TREE_CODE(op0)==COMPONENT_REF){
		 refLink(op0,refArray);
	}
}


static tree getCaller(tree fn)
{
	enum tree_code code=TREE_CODE(fn);
	//获取调用者，如果调用者是A A又属于B 调用变成 B->A->getName
	if(code==COMPONENT_REF){
		NPtrArray *refArray=n_ptr_array_new();
		refLink(fn,refArray);
		tree field=n_ptr_array_index(refArray,1);//0 getName 1 A
		fn=field;
		n_ptr_array_unref(refArray);
	}else if(code==ADDR_EXPR){
		tree op0=TREE_OPERAND (fn, 0);
		if(TREE_CODE(op0)==FUNCTION_DECL){
           char *functionName=IDENTIFIER_POINTER(DECL_NAME(op0));
    	   ClassFunc *func=func_mgr_get_entity_by_mangle(func_mgr_get(),functionName);
    	   if(func){
    		   char *sysName= func_mgr_get_class_name_by_mangle(func_mgr_get(),functionName);
    		   fn=DECL_ARGUMENTS (op0);
    	   }
		}
	}
	return fn;
}

static tree getParm(tree fndecl,int index,nboolean isFieldParms)
{
   tree args=isFieldParms?fndecl:DECL_ARGUMENTS (fndecl);
   tree parm=NULL_TREE;
   int count=0;
   for (parm = args; parm; parm = DECL_CHAIN (parm)){
	   //printf("得到参数---%d index:%d\n\n",count,index);
	   if(count==index){
		  return parm;
	   }
	   count++;
   }
   return NULL_TREE;
}


static tree getClassTypeParm(char *sysName,tree arg,int index,ClassFunc *func)
{
	ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
	if(!class_info_is_generic_class(info) ){
		n_warning("方法%s中的参数不是泛型类%s，不检查该参数。\n",func->orgiName,sysName);
		return NULL_TREE;
	}
	//printf("processClassType ------%s %s %d\n",sysName,func->orgiName,index);
	tree parm=NULL_TREE;
	if(aet_utils_valid_tree(func->fromImplDefine)){
		//printf("processClassType ------00 %s %s %d\n",sysName,func->orgiName,index);
		parm=getParm(func->fromImplDefine,index,FALSE);
	}else if(aet_utils_valid_tree(func->fieldParms)){
		//printf("processClassType ------11 %s %s %d\n",sysName,func->orgiName,index);
		parm=getParm(func->fieldParms,index,TRUE);
	}else if(aet_utils_valid_tree(func->fromImplDecl)){
		//printf("processClassType ------22 %s %s %d\n",sysName,func->orgiName,index);
		parm=getParm(func->fromImplDecl,index,FALSE);
	}else{
		n_error("getClassTypeParm 未知错误。%s %s\n",sysName,func->orgiName);
	}
	if(!aet_utils_valid_tree(parm)){
		n_error("未知错误。不知是什么参数，请报告此错误。%s %s index:%d\n",sysName,func->orgiName,index);
	}
	return parm;
}

/**
 * 从泛型函数来判断参数是否匹配
 */
static int compare(GenericModel *genDecl,GenericModel *genDefine,GenericModel *formalModel,GenericModel *actualModel,
		             char *callObject,ClassFunc *func,char *parmClassName,nboolean isGenFunc)
{
	if(genDecl==NULL)
		return 0;
	int i;
	int count=generic_model_get_count(formalModel);
	for(i=0;i<count;i++){
		GenericUnit *unit=generic_model_get(formalModel,i);
		if(generic_unit_is_query(unit))
			continue;
		int index=generic_model_get_index_by_unit(genDecl,unit);
		if(index<0){
            n_warning("参数的声明%s在泛型函数的声明%s中没找到。 类:%s 函数:%s",
            		generic_model_tostring(genDecl),generic_model_tostring(formalModel),callObject,func->orgiName);
            return 0;
		}
	}
	if(isGenFunc && genDefine==NULL){
		n_warning("这里调用的泛型函数，并且没有定义泛型。由参数决定类型。");
		return 1;
	}


	//The method getxxx(Hello<E>) in the type Hello<E> is not applicable for the arguments (Hello<String>)
	//过了声明现在比实参
	for(i=0;i<count;i++){
		GenericUnit *unit=generic_model_get(formalModel,i);
		if(generic_unit_is_query(unit))
			continue;
		int index=generic_model_get_index_by_unit(genDecl,unit);
		GenericUnit *actualUnit=generic_model_get(actualModel,i);
		GenericUnit *defineUnit=generic_model_get(genDefine,index);
		//printf("compare ---dddd- %p %p %p %p %p %d %d\n",actualUnit,defineUnit,genDefine,actualModel,genDecl,i,index);
		//printf("实参是:%s %s isGenFunc:%d\n",actualUnit->name,defineUnit->name,isGenFunc);

        if(!generic_unit_equal(actualUnit,defineUnit)){
 		    nboolean q1=generic_unit_is_query(defineUnit);
 		    nboolean q2=generic_unit_is_query(actualUnit);
 		   if(q1 && !q2){
			   continue;
		   }else{
			  if(!generic_unit_is_undefine(actualUnit) && generic_unit_is_undefine(defineUnit))
				  return 1;
			  char temp[1024];
			  sprintf(temp,"方法%s中的参数%s<%s>不能从<%s>转<%s>",func->orgiName,parmClassName,
				generic_unit_tostring(unit),generic_unit_tostring(actualUnit),generic_unit_tostring(defineUnit));
			  error_at(input_location,"%qs",temp);
			  return -1;
		   }
        }
	}
	return 1;

}

/**
 * formal 形参
 * actual 实参
 */
static void check(GenericModel **models,tree formal,tree actual,char *callObject,ClassFunc *func)
{
    GenericModel *classGenDecl=models[0];
    GenericModel *classGenDefine=models[1];
    GenericModel *funcGenDecl=models[2];
    GenericModel *funcGenDefine=models[3];
    if(!aet_utils_valid_tree(formal))
	   return;
    GenericModel *formalModel=c_aet_get_generics_model(formal);
    GenericModel *actualModel=c_aet_get_generics_model(actual);
    if(actualModel==NULL){
 	   if(TREE_CODE(actual)==CALL_EXPR){
		   tree fn=CALL_EXPR_FN (actual);
		   tree caller=getCaller(fn);
		   actualModel=c_aet_get_generics_model(caller);
 	   }else if(TREE_CODE(actual)==PARM_DECL){
 		   char *parmName=IDENTIFIER_POINTER(DECL_NAME(actual));
 		   if(parmName && !strcmp(parmName,"self")){
 			   char *sysName=class_util_get_class_name(TREE_TYPE(actual));
 			   if(sysName && !strcmp(sysName,callObject)){
 				   //如果sysName是callObject的父类或子类，如何？
 				  actualModel=classGenDefine;
 			   }
 		   }
 	   }else if(TREE_CODE(actual)==COMPONENT_REF){
 		    NPtrArray *refArray=n_ptr_array_new();
 		    refLink(actual,refArray);
 		    tree field=n_ptr_array_index(refArray,0);
 		    n_ptr_array_unref(refArray);
 		    actualModel=c_aet_get_generics_model(field);
 	   }else if(TREE_CODE(actual)==ADDR_EXPR){
			tree val=TREE_OPERAND (actual, 0);
			if(TREE_CODE(val)==VAR_DECL || TREE_CODE(val)==PARM_DECL || TREE_CODE(val)==FUNCTION_DECL){
	 		    actualModel=c_aet_get_generics_model(val);
			}
 	   }else if(TREE_CODE(actual)==NOP_EXPR){
 	         tree val=TREE_OPERAND (actual, 0);
             if(TREE_CODE(val)==VAR_DECL || TREE_CODE(val)==PARM_DECL || TREE_CODE(val)==FUNCTION_DECL){
                actualModel=c_aet_get_generics_model(val);
             }else if(TREE_CODE(val)==ADDR_EXPR){
                 val=TREE_OPERAND (val, 0);
                 if(TREE_CODE(val)==VAR_DECL || TREE_CODE(val)==PARM_DECL || TREE_CODE(val)==FUNCTION_DECL){
                     actualModel=c_aet_get_generics_model(val);
                 }
             }
 	   }
 	   if(actualModel==NULL){
 	    	 aet_print_tree_skip_debug(actual);
 	         //n_error("实参中不能获取到泛型。");
 	        location_t loc=input_location;
            if(EXPR_P(actual)){
                loc=EXPR_LOCATION(actual);
                error_at(loc,"实参没有声明泛型%qE。",actual);
            }else if(DECL_P(actual)){
                loc=DECL_SOURCE_LOCATION(actual);
                error_at(loc,"实参没有声明泛型%qD。",actual);
            }else{
 	            error_at(loc,"实参没有声明泛型%s。",actual);
            }
 	         return;
 	   }
    }


    n_debug("generic_check_parm----00 最后检查 %s %s\n",generic_model_tostring(formalModel),generic_model_tostring(actualModel));

	tree type=TREE_TYPE(formal);
    char *sysName=class_util_get_class_name(type);
    int re=compare(funcGenDecl,funcGenDefine,formalModel,actualModel,callObject,func,sysName,TRUE);
    if(re==0){
        re=compare(classGenDecl,classGenDefine,formalModel,actualModel,callObject,func,sysName,FALSE);
    }
    n_debug("generic_check_parm----11 最后检查 %s %s\n",generic_model_tostring(formalModel),generic_model_tostring(actualModel));

}

/**
 * 不是泛型类，也不是泛型函数，如果参数中有泛型，检查单个参数中的泛型与类中声明的是符匹配。主要是E extends$ Number。
 * 如果是泛型函数或函数中有问号泛型返回值或问号泛型参数。跳过self与 FuncGenParmInfo tempFgpi1234，否则跳过self
 */
nboolean  generic_check_parm(char *callObject,ClassFunc *func,vec<tree, va_gc> *exprlist,tree expr)
{
	   ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),callObject);
	   if(!class_info_is_generic_class(info) && !class_func_is_func_generic(func)){
		   n_debug("类%s不是泛型类，函数%s也不是泛型函数。不检查参数。\n",callObject,func->orgiName);
	   		return TRUE;
	   }
	   if(!class_func_have_generic_parm(func)){
		   n_debug("类%s中方法%s没有泛型参数:不检查。\n",callObject,func->orgiName);
		   return TRUE;
	   }
	   int skip=(class_func_is_func_generic(func) || class_func_is_query_generic(func))?2:1;
	   tree caller=getCaller(expr);
	   GenericModel *classGenDecl=getClassGenDecl(callObject);
	   GenericModel *classGenDefine=getClassGenDefine(caller);
	   GenericModel *funcGenDecl=getGenericFuncDecl(func);
	   GenericModel *funcGenDefine=getGenericFuncDefine(expr);
	   char *belongFunc=IDENTIFIER_POINTER(DECL_NAME(current_function_decl));
	   n_debug("generic_check_parm----11 4个泛型:class:%s %s genfunc:%s %s skip:%d 所在函数:%s 检查的函数:%s\n",generic_model_tostring(classGenDecl),generic_model_tostring(classGenDefine),
			   generic_model_tostring(funcGenDecl),generic_model_tostring(funcGenDefine),skip,belongFunc,func->orgiName);
	   if(classGenDecl && classGenDefine==NULL){
		   classGenDefine=classGenDecl;
	   }
	   GenericModel *modes[4]={classGenDecl,classGenDefine,funcGenDecl,funcGenDefine};
	   tree arg;
	   int ix;
	   for (ix = 0;vec_safe_iterate (exprlist, ix, &arg);++ix){
		   tree type=TREE_TYPE(arg);
		   if(ix<skip)
			   continue;
		   char *sysName=class_util_get_class_name(type);
		   if(sysName){
			  tree declParm=getClassTypeParm(sysName,arg,ix,func);
			  check(modes,declParm,arg,callObject,func);
		   }
	   }
	   return FALSE;
}

