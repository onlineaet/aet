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
#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "c-family/c-common.h"
#include "tree-iterator.h"

#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "c-aet.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"
#include "opts.h"

#include "../libcpp/include/cpplib.h"
#include "aetutils.h"
#include "classmgr.h"
#include "classcast.h"
#include "aetinfo.h"
#include "aetprinttree.h"
#include "aet-c-parser-header.h"
#include "classutil.h"
#include "genericutil.h"
#include "genericmodel.h"
#include "genericimpl.h"
#include "classfunc.h"
#include "funcmgr.h"


/**
 * 类转接口
 * 第一种模式：调用函数  finish_decl -->NOP_EXPR--->CALL_EXPR
 * static HomeOffice *homeOffice=0;
 * static HomeOffice *getHomeOffice()
 * {
 *	if(homeOffice==0)
 *		homeOffice= newnew HomeOffice();
 *	return homeOffice;
 * }
 *
 * OpenDoor *ifaceValue=getHomeOffice() 或 OpenDoor *ifaceValue=(OpenDoor *)getHomeOffice()
 *
 * 第二种模式：  finish_decl -->NOP_EXPR--->TARGET_EXPR
 * OpenDoor *ifaceValue=newnew HomeOffice(); (OpenDoor *)(newnew HomeOffice()); (OpenDoor *)newnew HomeOffice();
 *
 *第三种模式   finish_decl -->NOP_EXPR--->VAR_DECL
 *	HomeOffice *homeOffice=newnew HomeOffice();
 *	OpenDoor *ifaceValue=homeOffice;
 *第四种模式  finish_decl -->NOP_EXPR--->PARM_DECL
 * static void setxx(HomeOffice *self)
 * {
 *	  OpenDoor *ifaceValue=self;
 * }
 * 第五种模式 finish_decl -->NOP_EXPR--->VAR_DECL-->NOP_EXPR-->TARGET_EXPR
 * 此种是void变量在强转后强转的类型在 TARGET_EXPR中
 * 	 void *homeOffice=newnew HomeOffice();
 * 	 OpenDoor *ifaceValue=(OpenDoor*)homeOffice;或	 OpenDoor *ifaceValue=(HomeOffice*)homeOffice;
 * 	 TARGET_EXPR里存的都是HomeOffice
 *
 *第六种模式 finish_stmt -->left=VAR_DECL right=NOP_EXPR
 *OpenDoor *ifaceValue;
 *ifaceValue=newnew HomeOffice();
 */
static tree ifaceToClass(tree actualParm,tree formulaType);


static void classCastInit(ClassCast *self)
{
}

static char* getClassNameInVarDeclOrModifyOrComponentRefExpr(ClassCast *self,tree declOrModify)
{
	if(TREE_CODE(declOrModify)!=VAR_DECL && TREE_CODE(declOrModify)!=MODIFY_EXPR &&  TREE_CODE(declOrModify)!=COMPONENT_REF)
		return NULL;
	tree pointerType=TREE_TYPE(declOrModify);
	return class_util_get_class_name_by_pointer(pointerType);
}

static tree createOffsetof(char *className,char *face,location_t loc,nboolean minus)
{
	   ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),className);
	   printf("class_cast createOffsetof 00 接口所属类:%s iface:%s info:%p minus:%d\n",className,face,info,minus);
	   if(info==NULL)
		   return NULL_TREE;
	   tree id=aet_utils_create_ident(className);
	   tree typeDecl=lookup_name(id);
	   if(!typeDecl || typeDecl==error_mark_node)
		   return NULL_TREE;
	   tree classRecord=TREE_TYPE(typeDecl);
	   tree name=TYPE_NAME(classRecord);
	   if(TREE_CODE(name)==TYPE_DECL){
		   tree id=DECL_NAME (name);
	   }
	   tree offsetof_ref;
	   offsetof_ref = build1 (INDIRECT_REF, classRecord, null_pointer_node);
	   SET_EXPR_LOCATION (offsetof_ref, loc);
	   tree faceId=aet_utils_create_ident(face);
	   offsetof_ref = build_component_ref(loc, offsetof_ref, faceId, loc);
	   tree value = fold_offsetof (offsetof_ref);
	   if(minus){
	       value = wide_int_to_tree (TREE_TYPE (value), -wi::to_wide (value));
	   }
	   return value;
}

static tree createPointerPlusExpr(ClassName *className,char *ifaceVarInClass,location_t loc,tree nopexpr,nboolean minus)
{
	 tree offset=createOffsetof(className->sysName,ifaceVarInClass,loc,minus);
	 if(offset==NULL_TREE)
		 return NULL_TREE;
	 tree newExpr=build2_loc (loc, POINTER_PLUS_EXPR, TREE_TYPE (nopexpr), nopexpr,offset);
	 return newExpr;
}

/**
 * 转化类到接口
 */
static void castClassToInterface(char *rightName,char *leftName,tree nopexpr,tree op)
{
	   n_debug("class_cast_parser_decl castClassToInterface 00 %s %s \n",leftName,rightName);
	   if(rightName==NULL || leftName==NULL)
		   return;
	   ClassInfo *rightInfo=class_mgr_get_class_info(class_mgr_get(),rightName);
	   ClassInfo *leftInfo=class_mgr_get_class_info(class_mgr_get(),leftName);
	   n_debug("class_cast_parser_decl castClassToInterface 11 %s %s info:%p\n",leftName,rightName);
	   if(rightInfo==NULL || leftInfo==NULL)
		   return;
	   if(!strcmp(rightName,leftName))
		   return;
	   nboolean lhs=class_mgr_is_interface(class_mgr_get(),&leftInfo->className);
	   nboolean rhs=class_mgr_is_interface(class_mgr_get(),&rightInfo->className);
	   if(lhs && !rhs){
		   ClassName *belongClass=class_mgr_find_interface(class_mgr_get(),&rightInfo->className,&leftInfo->className);
		   n_debug("class_cast_parser_decl castClassToInterface 22 左边是接口，右边是类 类转接口 %s %s belongClass:%s\n",
				   leftName,rightName,belongClass?belongClass->sysName:"null");
		   if(belongClass!=NULL){
			   char ifaceVarInClass[255];
			   aet_utils_create_in_class_iface_var(leftInfo->className.userName,ifaceVarInClass);
			   tree value= createPointerPlusExpr(belongClass,ifaceVarInClass,EXPR_LOCATION (nopexpr),op,FALSE);
			   n_debug("class_cast_parser_decl castClassToInterface ---22 左边是接口，右边是类 类转接口 %s %s belongClass:%s\n",
					   leftName,rightName,belongClass->sysName);
			   TREE_OPERAND (nopexpr, 0)=value;
		   }
	   }else if(!lhs && rhs){
		   ClassName *belongClass=class_mgr_find_interface(class_mgr_get(),&leftInfo->className,&rightInfo->className);
		   n_debug("class_cast_parser_decl castClassToInterface 33 左边是类:%s，右边是接口:%s 接口转类 接口实现类是:%s\n",leftName,rightName,belongClass?belongClass->sysName:"null");
		   if(belongClass!=NULL){
			   //char ifaceVarInClass[255];
			  // aet_utils_create_in_class_iface_var(rightInfo->className.userName,ifaceVarInClass);
			   //tree value= createPointerPlusExpr(belongClass,ifaceVarInClass,EXPR_LOCATION (nopexpr),op,TRUE);
			  // n_debug("class_cast_parser_decl castClassToInterface 44 左边是类:%s，右边是接口:%s 接口转类 %s\n",leftName,rightName,belongClass?belongClass->sysName:"null");
               error_at(EXPR_LOCATION(nopexpr),"不能转化接口%qs到类qs。",rightName,leftName);
			   //TREE_OPERAND (nopexpr, 0)=value;
		   }
	   }
}

static char *getClassNameFromTargetExpr(tree targetexpr)
{
	   tree va=TREE_TYPE(targetexpr);
	   tree dec=TREE_OPERAND (targetexpr, 0);
	   tree init=TREE_OPERAND (targetexpr, 1);
	   if(TREE_CODE(va)!=POINTER_TYPE || TREE_CODE(init)!=BIND_EXPR)
		   return NULL;
	   //printf("getClassNameFromTargetExpr 00 %s %s\n",get_tree_code_name(TREE_CODE(va)),get_tree_code_name(TREE_CODE(init)));
	   char *className=class_util_get_class_name_by_pointer(va);
	   if(className==NULL)
		   return NULL;
	   tree vars=TREE_OPERAND (init, 0);
	   tree body=TREE_OPERAND (init, 1);
	   if(TREE_CODE(body)!=STATEMENT_LIST)
		   return NULL;
	   //printf("getClassNameFromTargetExpr 11 %s %s className:%s\n",get_tree_code_name(TREE_CODE(va)),get_tree_code_name(TREE_CODE(init)),className);
	   tree last= expr_last (body);
	   if(TREE_CODE(last)!=MODIFY_EXPR)
		  return NULL;
	   tree retn=TREE_OPERAND (last, 1);
	   //printf("getClassNameFromTargetExpr 22 %s %s\n",get_tree_code_name(TREE_CODE(retn)),className);
	   if(TREE_CODE(retn)==VAR_DECL){
		   //printf("getClassNameFromTargetExpr 33 %s %s\n",get_tree_code_name(TREE_CODE(retn)),className);
		   return className;
	   }
	   return NULL;
}

/**
 * 取变量的类型名
 */
static char *getClassNameFromVarDecl(tree varDecl)
{
	   tree pointerType=TREE_TYPE(varDecl);
	   tree name=DECL_NAME(varDecl);
	   if(TREE_CODE(pointerType)!=POINTER_TYPE)
		   return NULL;
	   tree record=TREE_TYPE(pointerType);
	  // printf("getClassNameFromVarDecl ----00 %s %s\n",get_tree_code_name(TREE_CODE(record)),IDENTIFIER_POINTER(name));
	   if(TREE_CODE(record)==RECORD_TYPE){
	       return class_util_get_class_name_by_pointer(pointerType);
	   }else if(TREE_CODE(record)==VOID_TYPE){
		  // printf("getClassNameFromVarDecl ----11 右边变量是一个void_type,检查是否有强转 %s %s\n",
				 //  get_tree_code_name(TREE_CODE(record)),IDENTIFIER_POINTER(name));
		   tree init=DECL_INITIAL(varDecl);
		   if(TREE_CODE(init)==NOP_EXPR){
			   enum tree_code  nopexprTypeCode=TREE_CODE(TREE_TYPE(init));
			   tree op=TREE_OPERAND (init, 0);
			   if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==TARGET_EXPR){
				   char *cast=getClassNameFromTargetExpr(op);
				   //printf("getClassNameFromVarDecl ----22 右边变量是一个void_type,检查是否有强转 %s\n",cast);
				   return cast;
			   }
		   }
	   }
	   return NULL;
}

static void setNopExpr(ClassCast *self ,tree decl,tree nopexpr,char *declClassName)
{
	   enum tree_code  nopexprTypeCode=TREE_CODE(TREE_TYPE(nopexpr));
	   tree op=TREE_OPERAND (nopexpr, 0);
	   n_debug("class_cast_parser_decl setNopExpr ----sxx00 %s %s\n",get_tree_code_name(nopexprTypeCode),get_tree_code_name(TREE_CODE(op)));
	   if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==CALL_EXPR){
		   char *className=class_util_get_class_name(TREE_TYPE(op));
		   n_debug("从设setNopExpr 再设castClassToInterface----- %s\n",className);
		    castClassToInterface(className,declClassName,nopexpr,op);
	   }else  if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==TARGET_EXPR){
		   n_debug("从设setNopExpr 再设setTargetExpr-----\n");
		   char *className= getClassNameFromTargetExpr(op);
		   castClassToInterface(className,declClassName,nopexpr,op);
	   }else  if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==VAR_DECL){
		   n_debug("从设setNopExpr 再设setVarDecl---eee--\n");
		    char *className=getClassNameFromVarDecl(op);
			castClassToInterface(className,declClassName,nopexpr,op);
	   }else  if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==PARM_DECL){
		    char *className=class_util_get_class_name(TREE_TYPE(op));
		    n_debug("从设setNopExpr 再设setParmDecl----- %s %s\n",className,declClassName);
			castClassToInterface(className,declClassName,nopexpr,op);
       }else if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==COMPONENT_REF){
            char *className=class_util_get_class_name(TREE_TYPE(op));
            n_debug("从设setNopExpr COMPONENT_REF-----不处理 右边:%s declClassName:%s\n",className,declClassName);
            castClassToInterface(className,declClassName,nopexpr,op);
            aet_print_tree(nopexpr);
	   }else if(nopexprTypeCode==POINTER_TYPE && TREE_CODE(op)==POINTER_PLUS_EXPR){
		   n_debug("从设setNopExpr POINTER_PLUS_EXPR-----不处理 \n");
		    aet_print_tree(nopexpr);
	   }
}

static void setTargetExpr(ClassCast *self,tree decl,tree targetexpr,char *declClassName)
{
	   tree va=TREE_TYPE(targetexpr);
	   tree dec=TREE_OPERAND (targetexpr, 0);
	   tree init=TREE_OPERAND (targetexpr, 1);
	   if(TREE_CODE(va)!=POINTER_TYPE || TREE_CODE(init)!=BIND_EXPR)
		   return;
	   char *className=class_util_get_class_name_by_pointer(va);
	   if(className==NULL)
		   return;
	   n_debug("class_cast_add_ref_in_finish_decl setTargetExpr 00 %s %s %s\n",
			   get_tree_code_name(TREE_CODE(va)),get_tree_code_name(TREE_CODE(init)),className);
	   tree vars=TREE_OPERAND (init, 0);
	   tree body=TREE_OPERAND (init, 1);
	   if(TREE_CODE(body)!=STATEMENT_LIST)
		   return;
	   tree last= expr_last (body);
	   if(TREE_CODE(last)!=MODIFY_EXPR)
		  return;
	   tree retn=TREE_OPERAND (last, 1);
	   if(TREE_CODE(retn)==VAR_DECL){
		   char *id=IDENTIFIER_POINTER(DECL_NAME(retn));
		   n_debug("class_cast_add_ref_in_finish_decl setTargetExpr 11 %s %s\n",get_tree_code_name(TREE_CODE(retn)),id);
		   ClassInfo *leftValueInfo=class_mgr_get_class_info(class_mgr_get(),className);
		   if(class_info_is_generic_class(leftValueInfo)){
			   GenericModel *rgenerics=c_aet_get_generics_model(retn);
			   if(rgenerics){
			       n_debug("把右边的对象定义的泛型赋值给左边的变量。----\n");
				   GenericModel *lgenerics=c_aet_get_generics_model(decl);
				   if(!lgenerics){
					   c_aet_set_generics_model(decl,rgenerics);
				   }else{
                       //比较两个generics如果不相等是错的
					   nboolean result=generic_model_equal(rgenerics,lgenerics);
					   n_debug("把右边的对象定义的泛型赋值给左边的变量。---222 - %d\n",result);
					   if(!result){
						   error_at(DECL_SOURCE_LOCATION(decl),"变量%qD的泛型不能转换到另外一个定义的泛型。",decl);
					   }
				   }
			   }
		   }
	   }
}


/**
 * 有声明有初始值的情况有5种
 * (1)Abc *abc=new$ Abc()这种情况是
 *  调用setTargetExpr
 * (2)void *abc=new$ Abc();
 * 从设setNopExpr 再设setTargetExpr
 * (3) AObject *abc=new$ Abc();或NObject *abc=(AObject*)new$ Abc();
 *     从设setNopExpr 再设setTargetExpr-----
 * (4)Abc *abc=new$ Abc();
 *    Abc *ttcc=abc;
 *    调用setVarDecl
 * (5)Abc *abc=new$ Abc();
 *    void *ttcc=abc;
 *    从设setNopExpr 再设setVarDecl-----
 *
 *如果变量是void* 不管，如果是Class 如果赋值是接口，要处理，如果是接口 赋值是Class要处理
  */
void   class_cast_in_finish_decl(ClassCast *self ,tree decl)
{
	if(TREE_CODE(decl)!=VAR_DECL && TREE_CODE(decl)!=PARM_DECL)
		return;
	if(TREE_CODE(decl)==VAR_DECL){
        char *declClassName=getClassNameInVarDeclOrModifyOrComponentRefExpr(self,decl);
        if(declClassName==NULL)
			return;
		//检查返回的是不是NClass
	    tree init=DECL_INITIAL(decl);
		n_debug("class_cast_add_ref_in_finish_decl 先声明再有初始值 00 declClassName:%s init tree:%p declClassName:%s init code:%s\n",
				declClassName,init,declClassName,aet_utils_valid_tree(init)?get_tree_code_name(TREE_CODE(init)):"null");
		if(!aet_utils_valid_tree(init))
			return;
	    if(TREE_CODE(init)==NOP_EXPR){
	    	setNopExpr(self,decl,init,declClassName);
	    }else if(TREE_CODE(init)==TARGET_EXPR){
	    	setTargetExpr(self,decl,init,declClassName);
			//aet_print_tree(init);
	    }else if(TREE_CODE(init)==VAR_DECL){
	    	  //setVarDecl(self,decl,init,declClassName);
	    	  GenericModel *generics=c_aet_get_generics_model(init);
	    	  if(generics)
	    		c_aet_set_generics_model(decl,generics);
	    }else if(TREE_CODE(init)==PARM_DECL){
	    	//setParmDecl(self,decl,init,declClassName);
	    }else if(TREE_CODE(init)==CALL_EXPR){
	    	//setCallExpr(self,decl,init,declClassName);
	    }
	}else if(TREE_CODE(decl)==PARM_DECL){
		printf("class_cast_add_ref_in_finish_decl 是参数还未实现 ！！！\n");
		error("是参数还未实现-----------------");

	}
}

 /**
 * 先声明再赋值
 * (1)Abc *abc;
 *    abc=new$ Abc();
 *    调setTargetExpr
 * (2) void *abc;
 *     abc=new$ Abc();
 *    从设setNopExpr 再设setTargetExpr-----
 * (3) AObject *abc;
 *     abc=new$ Abc();
 *     同（2）
 * (4) AObject *abc;
 *     abc=(AObject*)new$ Abc();
 *     同（2）
 * (5)Abc *abc=new$ Abc();
 *    void *ttcc;
 *    ttcc=abc;
 *    从设setNopExpr 再设setVarDecl-----
 */

void  class_cast_in_finish_stmt(ClassCast *self ,tree stmt)
{
	if(!aet_utils_valid_tree(stmt))
		return;
	tree lt;
	tree rt;
	char *declClassName=NULL;
	char *varClassName=NULL;
    declClassName=getClassNameInVarDeclOrModifyOrComponentRefExpr(self,stmt);
    if(declClassName==NULL)
		return;
	//n_debug("class_cast_add_ref_in_finish_stmt 语句 00 stmt:%p %s\n",stmt,declClassName);
	lt=TREE_OPERAND(stmt,0);
	rt=TREE_OPERAND(stmt,1);
    varClassName=getClassNameInVarDeclOrModifyOrComponentRefExpr(self,lt);
    if(varClassName==NULL)
    	return;
  //  n_debug("class_cast_add_ref_in_finish_stmt 语句 11 stmt:%p varClassName:%s  declClassName:%s rt:%s\n",
		//	stmt,varClassName,declClassName,get_tree_code_name(TREE_CODE(rt)));
    //aet_print_tree(lt);
    //printf("=======================rt==================\n");
   // aet_print_tree(rt);
    if((TREE_CODE(lt)==VAR_DECL || TREE_CODE(lt)==COMPONENT_REF) && TREE_CODE(rt)==NOP_EXPR){
    	setNopExpr(self,lt,rt,declClassName);
    }else if(TREE_CODE(lt)==COMPONENT_REF && TREE_CODE(rt)==TARGET_EXPR){
    	n_debug("class_cast_add_ref_in_finish_stmt 语句 22 stmt:%p %s\n",stmt,varClassName);
		tree field=TREE_OPERAND(lt,1);
		if(TREE_CODE(field)==FIELD_DECL){
			//n_debug("class_cast_add_ref_in_finish_stmt 语句 2xxx2 stmt:%p %s\n",stmt,varClassName);
			setTargetExpr(self,field,rt,declClassName);
		}
    }else  if(TREE_CODE(lt)==VAR_DECL && TREE_CODE(rt)==VAR_DECL){
        char *rVarClassName=getClassNameInVarDeclOrModifyOrComponentRefExpr(self,rt);
        if(rVarClassName!=NULL){
       	  GenericModel *generics=c_aet_get_generics_model(rt);
		  if(generics)
			c_aet_set_generics_model(lt,generics);
        }
	}else if(TREE_CODE(rt)==NOP_EXPR){
		n_debug("class_cast_add_ref_in_finish_stmt 语句 33 stmt:%p %s\n",stmt,varClassName);
		aet_print_tree_skip_debug(lt);
		n_error("还未处理此种类型。");
	}else if(TREE_CODE(rt)==VAR_DECL){
    	//setVarDecl(self,lt,rt,declClassName);
	}
}

///////////////////////////-----------------------------------------------------

static tree getTypeFromPointer(tree pointerType)
{
	if(TREE_CODE(pointerType)!=POINTER_TYPE)
		return pointerType;
	pointerType=TREE_TYPE(pointerType);
	return getTypeFromPointer(pointerType);
}

/**
 * 从tree op往上找，找到有接口castToType的类为止
 */
static tree buildClassToIface(tree op,tree castToType)
{
	 char *sysName=class_util_get_class_name(TREE_TYPE(op));
     char *interface=class_util_get_class_name(castToType);//转化的接口名
	 ClassInfo *rightInfo=class_mgr_get_class_info(class_mgr_get(),sysName);
	 ClassInfo *leftInfo=class_mgr_get_class_info(class_mgr_get(),interface);
	 ClassName *belongClass=class_mgr_find_interface(class_mgr_get(),&rightInfo->className,&leftInfo->className);
	 tree opop=op;
	 if(strcmp(sysName,belongClass->sysName)){
		     printf("有接口的不是类:%s,而是它的父类:%s\n",sysName,belongClass->sysName);
		    //转成父类再转调用接口
			  tree id=aet_utils_create_ident(belongClass->sysName);
			  tree recodType=lookup_name(id);
			  recodType=TREE_TYPE(recodType);
			  tree type=build_pointer_type(recodType);
		      opop = build1 (NOP_EXPR, type, op);
	 }

    char *beConvertedIntoSysName=belongClass->sysName;//class_util_get_class_name(TREE_TYPE(op));//被转化的类名
    n_debug("buildClassToIface --- %s beConvertedIntoSysName:%s\n",interface,beConvertedIntoSysName);
	ClassName *ifaceName=class_mgr_get_class_name_by_sys(class_mgr_get(),interface);
	ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),beConvertedIntoSysName);
	char ifaceVarInClass[255];
    aet_utils_create_in_class_iface_var(ifaceName->userName,ifaceVarInClass);
	location_t loc=input_location;
	tree  selfref=build_indirect_ref (loc,opop,RO_ARROW);
	tree ifaceVarName=aet_utils_create_ident(ifaceVarInClass);
    tree componetRef = build_component_ref (loc,selfref,ifaceVarName, loc);//
    tree addrExpr = build1 (ADDR_EXPR, castToType, componetRef);
	return addrExpr;
}

/**
 * 是不是从类转接口
 */
static nboolean isClassToIface(tree castToType,tree op)
{
    char *interface=class_util_get_class_name(castToType);//这是一个aet 类吗？
    char *sysName=class_util_get_class_name(TREE_TYPE(op));
    n_debug("isClassToIface --- %s %s\n",interface,sysName);
    if(sysName==NULL || interface==NULL)
	   return FALSE;
    //如果castToType 与op类型相同返回FALSE;
    if(c_tree_equal(castToType,TREE_TYPE(op))){
    	 printf("类型相同---- \n");
    	return FALSE;
    }

    ClassInfo *rightInfo=class_mgr_get_class_info(class_mgr_get(),sysName);
    ClassInfo *leftInfo=class_mgr_get_class_info(class_mgr_get(),interface);
    ClassName *belongClass=class_mgr_find_interface(class_mgr_get(),&rightInfo->className,&leftInfo->className);
	if(belongClass==NULL){
		error_at(input_location,"%qs没有实现接口%qs，不能强转。",rightInfo->className.userName,leftInfo->className.userName);
		return FALSE;
	}
	n_debug("isClassToIface --- 11 %s %s %s\n",interface,sysName,belongClass->sysName);
	return TRUE;
}

static nboolean isIfaceToClass(tree castToType,tree op)
{
    char *leftSysName=class_util_get_class_name(castToType);//这是一个aet 类吗？
    char *rightSysName=class_util_get_class_name(TREE_TYPE(op));
    if(leftSysName==NULL || rightSysName==NULL)
       return FALSE;
    //如果castToType 与op类型相同返回FALSE;
    if(c_tree_equal(castToType,TREE_TYPE(op))){
         printf("isIfaceToClass 类型相同---- \n");
        return FALSE;
    }

    ClassInfo *rightInfo=class_mgr_get_class_info(class_mgr_get(),rightSysName);
    ClassInfo *leftInfo=class_mgr_get_class_info(class_mgr_get(),leftSysName);
    if(!class_info_is_interface(leftInfo) && class_info_is_interface(rightInfo)){
        ClassName *belongClass=class_mgr_find_interface(class_mgr_get(),&leftInfo->className,&rightInfo->className);
        return belongClass!=NULL;
    }else{
        return FALSE;
    }

}

/**
 * 从类转接口
 * Abc *abc=new$ Abc();
 * AObject *temp=(AObject*)abc;
 * 接口:
 * Interface *face=(Interface *)temp;
 * 这时，从temp应该找到最下层的类abc。由abc再来找到接口。否则出错。
 */
static tree convertClassToInterface(tree castToType,tree expr)
{
   tree type=getTypeFromPointer(castToType);
   char *interface=class_util_get_class_name(castToType);//这是一个aet 类吗？
   if(TREE_CODE(expr)==NOP_EXPR){
   	      tree op=TREE_OPERAND (expr, 0);
   	      if(!isClassToIface(castToType,op)){
   	    	  return expr;
   	      }
   	      if(TREE_CODE(op)==VAR_DECL || TREE_CODE(op)==ADDR_EXPR || TREE_CODE(op)==PARM_DECL){
   	    	    //printf("convertClassToInterface --- %s\n",get_tree_code_name(TREE_CODE(op)));
   	    	    op=buildClassToIface(op,castToType);
		        TREE_OPERAND (expr, 0)=op;
   		        return expr;
   	      }else if(TREE_CODE(op)==CALL_EXPR){
   	       //printf("convertClassToInterface ssscalll--- %s\n",get_tree_code_name(TREE_CODE(op)));
   	                     op=buildClassToIface(op,castToType);
   	                     TREE_OPERAND (expr, 0)=op;
   	                     return expr;
   	      }else{
   	           aet_print_tree_skip_debug(expr);
   	      	   n_error("expr是 NOP_EXPR 它的类型与要转化的类型不一样。%s，还未处理!!!\n",interface);
   	      }
    	  return expr;
   }
   return expr;
}



/**
 * 在c_parser.c中遇到(Abc *)xxxx会调到这里
 * 如果被转的是指针类型的Class。
 */
tree class_cast_cast(ClassCast *self,struct c_type_name *type_name,tree expr)
{
      if (!type_name)
          return expr;
      tree type = groktypename (type_name, NULL, NULL);
      char *sysName=class_util_get_class_name(type);//这是一个aet 类吗？
      if(!sysName)
          return expr;
      if(TREE_CODE(type)!=POINTER_TYPE){
          n_warning("强制转化成类%s,但类型不是指针。不处理。",sysName);
          return expr;
      }
        ClassInfo *leftInfo=class_mgr_get_class_info(class_mgr_get(),sysName);
        if(leftInfo==NULL){
            n_error("从表达式转化到类%s时出错。原因：找不到类:%s\n",sysName,sysName);
            return expr;
        }
        if(class_info_is_class(leftInfo) || class_info_is_abstract_class(leftInfo)){
           if(TREE_CODE(expr)==NOP_EXPR){
              tree op=TREE_OPERAND (expr, 0);
              if(isIfaceToClass(type,op)){
                  char *rightSysName=class_util_get_class_name(TREE_TYPE(op));
                  ClassInfo *rightInfo=class_mgr_get_class_info(class_mgr_get(),rightSysName);
                  ClassName *belongClass=class_mgr_find_interface(class_mgr_get(),&leftInfo->className,&rightInfo->className);
                  if(belongClass==NULL){
                      location_t loc= type_name->declarator->id_loc;
                      error_at(loc,"类%qs并未实接口%qs。",leftInfo->className.sysName,rightInfo->className.sysName);
                      return expr;
                  }
                  n_debug("接口转类-----接口：%s 转化的类:%s\n",rightSysName,leftInfo->className.sysName);
                  tree pointerType=type;//等同 ARandom *;
                  tree value= ifaceToClass(op,pointerType);
                  TREE_OPERAND (expr,0)=value;
              }
           }
        }else{
            //printf("转接口 %s\n",destSysName);
            return convertClassToInterface(type,expr);
        }
        return expr;
}

////////////////////-----------------------

static tree  getFunctionType(tree type)
{
   if(TREE_CODE(type)==FUNCTION_TYPE)
	  return type;
   return getFunctionType(TREE_TYPE(type));
}

/**
 * 能转化的参数吗
 * 不是aet class不进行转化
 */
static nboolean isAetClass(tree type)
{
    if(TREE_CODE(type)!=POINTER_TYPE)
		return FALSE;
	char *sysName=class_util_get_class_name(type);
	if(sysName==NULL)
		return FALSE;
	return TRUE;
}

static nboolean isIface(tree type)
{
	char *sysName=class_util_get_class_name(type);
	ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
	return class_info_is_interface(info);
}

/**
 * 接口转类
 * actualParm->_iface_common_var.atClass123
 */
static tree ifaceToClass(tree actualParm,tree formulaType)
{
	location_t loc=input_location;
	tree  actualRef=build_indirect_ref (loc,actualParm,RO_ARROW);
	tree _iface_common_var=aet_utils_create_ident(IFACE_COMMON_STRUCT_VAR_NAME);
    tree componetTwo = build_component_ref (loc,actualRef,_iface_common_var, loc);//
	tree atClassVarName=aet_utils_create_ident(IFACE_AT_CLASS);
    tree componetRef = build_component_ref (loc,componetTwo,atClassVarName, loc);//
	tree nopExpr= build1 (NOP_EXPR, formulaType, componetRef);
	return nopExpr;
}


/**
 * 实参类转接口
 * 从 ARandom ttyydd转成如下格式的tree
 * (RandomGenerator *)&ttyydd->ifaceRandomGenerator2066046634
 * 如果atClass与actualSysName不一样，把actualSysName转成atClass后再引用接口
 */
static tree classToIface(tree actualParm,tree formulaType)
{
	tree actualType=TREE_TYPE(actualParm);
	char *actualSysName=class_util_get_class_name(actualType);
	char *formulaSysName=class_util_get_class_name(formulaType);
	ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),actualSysName);
	ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(),actualSysName);
	ClassName *ifaceName=class_mgr_get_class_name_by_sys(class_mgr_get(),formulaSysName);
	ClassName *atClass= class_mgr_find_interface(class_mgr_get(), className,ifaceName);
	if(atClass==NULL){
		error_at(input_location,"从实参不能转到接口。因为类%qs没有实现接口%qs。",actualSysName,formulaSysName);
		return NULL_TREE;
	}
	printf("classToIface ---在类:%s中找到了接口:%s 实参的类型是:%s\n",atClass->sysName,ifaceName->sysName,actualSysName);
	char ifaceVarInClass[255];
    aet_utils_create_in_class_iface_var(ifaceName->userName,ifaceVarInClass);
	location_t loc=input_location;
	tree ref=actualParm;
	//如果atClass与actualSysName不一样，把actualSysName转成atClass后再引用接口
	if(strcmp(actualSysName,atClass->sysName)){
		tree record=lookup_name(aet_utils_create_ident(atClass->sysName));
		record=TREE_TYPE(record);
		tree type=build_pointer_type(record);
		ref = build1 (NOP_EXPR, type, actualParm);
	}
	//tree  selfref=build_indirect_ref (loc,actualParm,RO_ARROW);
	tree  selfref=build_indirect_ref (loc,ref,RO_ARROW);

	tree ifaceVarName=aet_utils_create_ident(ifaceVarInClass);
    tree componetRef = build_component_ref (loc,selfref,ifaceVarName, loc);//
    tree addrExpr = build1 (ADDR_EXPR, formulaType, componetRef);
	return addrExpr;
}


/**
 * value是否转到origType
 */
static tree convertActualParm(tree origType,tree actualParm,int index)
{
	if(!isAetClass(origType)){
		return NULL_TREE;
	}
	if(!isAetClass(TREE_TYPE(actualParm))){
		return NULL_TREE;
	}
	tree actualType=TREE_TYPE(actualParm);
    nboolean equal=c_tree_equal(origType,actualType);
    if(!equal){
    	nboolean is0=isIface(origType);
    	nboolean is1=isIface(actualType);
 		char *formulaSysName=class_util_get_class_name(origType);
    	char *actualSysName=class_util_get_class_name(actualType);
    	if(is0 && is1){
    		error_at(input_location,"实参类型%qs与形参类型:%qs都是接口，但不是同一个类型，不能转化。",actualSysName,formulaSysName);
    	}else if(!is0 && is1){
            printf("实参:%s是接口，形参%s是类。接口转类\n",actualSysName,formulaSysName);
            tree newActualParm=ifaceToClass(actualParm,origType);
            return newActualParm;
    	}else if(is0 && !is1){
            printf("实参:%s是类，形参%s是接口。类转接口。当前函数:%s\n",actualSysName,formulaSysName,IDENTIFIER_POINTER(DECL_NAME(current_function_decl)));
            tree newActualParm=classToIface(actualParm,origType);
            return newActualParm;
    	}
    }else{
        //如果是接口调用它声明的方法，接口要转到atClass,但这两个地方不转1._iface_reserve_ref_field_123 2._iface_reserve_unref_field_123
    }
    return NULL_TREE;

}
/**
 * 参数转化
 * 接口转类
 * 为转接口
 * formal parameter 形式参数 actual parameter
 */
void class_cast_parm_convert_from_ctor(ClassCast *self,tree func,vec<tree, va_gc> *exprlist)
{
   if(TREE_CODE(func)==COMPONENT_REF){
	    tree fieldDecl=TREE_OPERAND (func, 1);
		tree  funcType = getFunctionType(TREE_TYPE (fieldDecl));
	    int count=0;
		for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
			tree type=TREE_VALUE(al);
			if(type == void_type_node){
				break;
			}
			tree actualParm = (*exprlist)[count];
			tree cc=convertActualParm(type,actualParm,count);
			if(cc!=NULL_TREE)
				(*exprlist)[count]=cc;
			count++;
		}
   }else{
		n_warning("class_cast_parm_convert_from_ctor %s 还未处理！！！\n",get_tree_code_name(TREE_CODE(func)));

   }
}

static nboolean isIFaceCall(tree op)
{
    char *sysName=class_util_get_class_name(TREE_TYPE(op));
    if(sysName!=NULL){
    	ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
    	return class_info_is_interface(info);
    }
    return FALSE;
}

static nboolean isIFaceField(tree op,tree field)
{
    char *sysName=class_util_get_class_name(TREE_TYPE(op));
    ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
    char *fieldName=IDENTIFIER_POINTER(DECL_NAME(field));
    NPtrArray    *funcs=func_mgr_get_funcs(func_mgr_get(),&info->className);
    int i;
    for(i=0;i<funcs->len;i++){
       ClassFunc *func=n_ptr_array_index(funcs,i);
       //zclei super
//       char superName[255];
//       aet_utils_create_super_field_name(info->className.userName,func->mangleFunName,superName);
//       if(!strcmp(fieldName,func->mangleFunName) || !strcmp(fieldName,superName))
//    	   return TRUE;
       if(!strcmp(fieldName,func->mangleFunName))
        	   return TRUE;
    }
    return FALSE;
}

static nboolean isIFaceRefOrUnref(tree op,tree field)
{
    char *sysName=class_util_get_class_name(TREE_TYPE(op));
    ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),sysName);
    char *fieldName=IDENTIFIER_POINTER(DECL_NAME(field));
    NPtrArray    *funcsArray=func_mgr_get_funcs(func_mgr_get(),&info->className);
    int i;
    for(i=0;i<funcsArray->len;i++){
       ClassFunc *func=n_ptr_array_index(funcsArray,i);
         //zcl super$
//       char superName[255];
//       aet_utils_create_super_field_name(info->className.userName,func->mangleFunName,superName);
//       printf("isIFaceRefOrUnref --- %s %s %s\n",fieldName,func->mangleFunName,func->orgiName);
//       if((!strcmp(fieldName,func->mangleFunName)  || !strcmp(fieldName,superName)) &&
//    		   (!strcmp(func->orgiName,IFACE_REF_FIELD_NAME) || !strcmp(func->orgiName,IFACE_UNREF_FIELD_NAME)))
//    	   return TRUE;
       if(!strcmp(fieldName,func->mangleFunName)  &&
        		   (!strcmp(func->orgiName,IFACE_REF_FIELD_NAME) || !strcmp(func->orgiName,IFACE_UNREF_FIELD_NAME)))
        	   return TRUE;
    }
    return FALSE;
}

static tree convertToIface(tree type,tree actualParm,nboolean isRefOrUnref)
{
	   if(!isAetClass(type)){
			return NULL_TREE;
		}
		if(!isAetClass(TREE_TYPE(actualParm))){
			return NULL_TREE;
		}
		tree actualType=TREE_TYPE(actualParm);
		nboolean isface=isIface(type);
		nboolean isActualFace=isIface(actualType);
		char *formulaSysName=class_util_get_class_name(type);
		char *actualSysName=class_util_get_class_name(actualType);
		n_debug("接口调用接口的方法:00 实参:%s 形参:%s\n",actualSysName,formulaSysName);
		if(isface && isActualFace){
			nboolean equal=c_tree_equal(type,actualType);
			if(!equal){
				error_at(input_location,"接口%qs不能转接口%qs。",actualSysName,formulaSysName);
				return NULL_TREE;
			}else{
                if(!isRefOrUnref){
                	//把是接口类型的实参转成AObject
                	ClassName *rootClassName=class_mgr_get_class_name_by_user(class_mgr_get(),AET_ROOT_OBJECT);
                	tree id=aet_utils_create_ident(rootClassName->sysName);
                	tree aobjectType=lookup_name(id);
                    aobjectType=TREE_TYPE(aobjectType);
                    tree pointerType=build_pointer_type(aobjectType);
                    n_debug("接口调用接口的方法:11 实参:%s 形参:%s\n",actualSysName,formulaSysName);
                	return ifaceToClass(actualParm,pointerType);
                }
			}
		}else if(isface && !isActualFace){

		}
		return NULL_TREE;
}

/**
 * 在引用类方法时，作转化
 */
void class_cast_parm_convert_from_deref(ClassCast *self,tree func,vec<tree, va_gc> *exprlist)
{
   if(TREE_CODE(func)==COMPONENT_REF){
		tree op0=TREE_OPERAND (func, 0);
		tree fieldDecl=TREE_OPERAND (func, 1);
		nboolean iface=isIFaceCall(op0);
		if(iface && isIFaceField(op0,fieldDecl)){
			char *fieldName=IDENTIFIER_POINTER(DECL_NAME(fieldDecl));
			n_debug("是一个接口调用的方法。第一个参数是接口，通过接口变量 _iface_common_var.atClass123把iface转为AObject:%s\n",fieldName);
			//转第一个参数
			tree  funcType = getFunctionType(TREE_TYPE (fieldDecl));
			int count=0;
			for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
				tree type=TREE_VALUE(al);
				if(type == void_type_node){
					break;
				}
				tree actualParm = (*exprlist)[count];
				n_debug("class_cast_parm_convert_from_deref 实参是:%d\n",count);
				if(count==0){
					nboolean isRefUnref=isIFaceRefOrUnref(op0,fieldDecl);
					tree cc=convertToIface(type,actualParm,isRefUnref);
					n_debug("class_cast_parm_convert_from_deref 11 实参是:%d cc:%p\n",count,cc);
					if(cc!=NULL_TREE)
					  (*exprlist)[count]=cc;
				}else{
				   tree cc=convertActualParm(type,actualParm,count);
				   n_debug("class_cast_parm_convert_from_deref 22 实参是:%d cc:%p\n",count,cc);

				   if(cc!=NULL_TREE)
					  (*exprlist)[count]=cc;
				}
				count++;
			}
		}else{
			char *fieldName=IDENTIFIER_POINTER(DECL_NAME(fieldDecl));
			tree  funcType = getFunctionType(TREE_TYPE (fieldDecl));
			int count=0;
			for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
				tree type=TREE_VALUE(al);
				//printf("class_cast_parm_convert %s count:%d\n",get_tree_code_name(TREE_CODE(func)),count);
				if(type == void_type_node){
					break;
				}
				tree actualParm = (*exprlist)[count];
				//printf("实际参数是\n");
				//aet_print_tree(actualParm);
				tree cc=convertActualParm(type,actualParm,count);
				if(cc!=NULL_TREE)
					(*exprlist)[count]=cc;
				count++;
			}
		}
   }
}

/**
 * 返回值是对象转成接口
 */
tree  class_cast_cast_for_return(ClassCast *self,tree interfaceType,tree expr)
{
	nboolean isObjectToIface=isClassToIface(interfaceType,expr);
	printf("class_cast_cast_for_return --- %d\n",isObjectToIface);
	if(isObjectToIface){
   	    expr=buildClassToIface(expr,interfaceType);
	}
	return expr;

}

ClassCast *class_cast_new()
{
	ClassCast *self = n_slice_alloc0 (sizeof(ClassCast));
	classCastInit(self);
	return self;
}


