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
#include "c-family/c-spellcheck.h"
#include "c-aet.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"

#include "../libcpp/include/cpplib.h"
#include "aetutils.h"
#include "classmgr.h"
#include "classref.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "classdot.h"
#include "classinfo.h"
#include "aetprinttree.h"
#include "classutil.h"
#include "aet-c-parser-header.h"
#include "genericutil.h"
#include "genericimpl.h"


static void classDotInit(ClassDot *self)
{

}

static tree createNormalOrSuperRef(ClassDot *self,location_t loc,location_t component_loc,tree component,tree exprValue)
{
	 c_parser *parser =((ClassAccess*)self)->parser;
	 char *lows[4];
	 int relationShip=class_access_get_last_class_and_var((ClassAccess*)self,component_loc,component,exprValue,lows);
	 if(relationShip<0)
		 return error_mark_node;
	 char *lowestClass=lows[2];
	 char *className=lows[3];
	 tree newComponent=aet_utils_create_temp_func_name(className,IDENTIFIER_POINTER(component));
	 n_debug("class_dot_build_deref 22 className:%s component:%s 根类:%s 根变量:%s lowestClass:%s 当前所在函数：%s",
			 className,IDENTIFIER_POINTER(newComponent),lows[0],lows[1],lowestClass,IDENTIFIER_POINTER(DECL_NAME(current_function_decl)));
	 //把函数名变成_A3Abc4open1。
	 tree tempField=class_access_create_temp_field((ClassAccess*)self,lowestClass,newComponent,component_loc);
	 if(!aet_utils_valid_tree(tempField)){
		 error_at(loc,"创建临时的类%qs成员%qs失败。",className,IDENTIFIER_POINTER(component));
		 return error_mark_node;
	 }
	 tree datum=exprValue;
	 tree ref=class_access_build_field_ref((ClassAccess *)self,loc,datum, tempField);
     if(!aet_utils_valid_tree(ref)){
         error_at(loc,"创建临时的类%qs成员%qs失败。",className,IDENTIFIER_POINTER(component));
         return error_mark_node;
     }
	 BINFO_FLAG_6(ref)=1;//这是通过类引用的方式调用函数的标记
	 return ref;
}



/**
 * 1.找出引用的类或是接口
 * 2.找出参数或变量名，作为第一个参数
 * 3.原来的函数名改为了 _A3Abc4open1 Abc是引用该函数的类，
 * 4.临时的域类型是引用变量的类
 * 引用类和引用变量是指强转引用。如:((Abc*)var)-> var引用变量是另外一个类,如HomeOffice，被转成Abc
 * func组成：
 * build3 (COMPONENT_REF, subtype, datum, subdatum,NULL_TREE);
 */
tree  class_dot_build_ref(ClassDot *self,location_t loc,location_t component_loc,tree component,tree exprValue)
{
	 c_parser *parser =((ClassAccess*)self)->parser;
	 GenericModel *funcDefineModel=NULL;
	 if(c_parser_peek_token (parser)->type==CPP_LESS){
		 n_debug("class_dot_build_ref 是不是调用泛型函数 %s",IDENTIFIER_POINTER(component));
		 //不一定是泛型函数调用 比如:self->position<(int)var;
		 //所以要判断<后的内容，根据泛型定义产生式 --> <int,,,,> | <int> <符号应该是类型
		 c_token *token= c_parser_peek_2nd_token (parser);
		 if(c_token_starts_typename(token) || (token->type==CPP_NAME && generic_util_valid_id(token->value))){
			 funcDefineModel= generic_model_new(TRUE,GEN_FROM_CALL);
			 if(!generic_impl_check_func_at_call(generic_impl_get(),IDENTIFIER_POINTER(component),funcDefineModel)){
				 return NULL_TREE;
			 }
		 }
		 n_debug("class_dot_build_ref------funcDefineModel----- %p",funcDefineModel);
	 }
	 nboolean isFun=c_parser_peek_token (parser)->type==CPP_OPEN_PAREN;
	 if(isFun){
		 char *idStr=IDENTIFIER_POINTER(component);
	     ClassName *className=class_mgr_get_class_name_by_user(class_mgr_get(),idStr);
		 ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
		 n_debug("class_dot_build_deref 00 %s component:%s 当前所在函数：%s",
				 className==NULL?"null":className->sysName,idStr,IDENTIFIER_POINTER(DECL_NAME(current_function_decl)));
		 if(info!=NULL){
			 error_at(loc,"不能调用构造函数 %qs",className->userName);
			 return error_mark_node;
		 }else{
			 component=class_interface_transport_ref_and_unref(self->classInterface,component,exprValue);
			 tree ref=createNormalOrSuperRef(self,loc,component_loc,component,exprValue);
			 c_aet_set_func_generics_model(ref,funcDefineModel);
			 return ref;
		 }
	 }else{
	     n_debug("class_dot_build_deref 变量处理 还要重新处理，先这样 component:%s",IDENTIFIER_POINTER(component));
		 //从引用的类名开始查找引用的变量，取最近的，原来还要判断右边赋值情况，想多了。重新设计
		 VarRefRelation *item=class_access_create_relation_ship((ClassAccess *)self,loc,component,exprValue);
		 if(item==NULL)
			 return exprValue;
		 tree staticVar=NULL_TREE;
		 exprValue=var_call_dot_visit(((ClassAccess *)self)->varCall,exprValue,component,component_loc,item,&staticVar);
		 if(staticVar!=NULL_TREE){
			 printf("class_dot_build_ref 变量处理 是静态变量----\n");
			 return staticVar;
		 }
		 tree datum=exprValue;
         tree ref = build_component_ref (loc,datum,component, component_loc);
		 return ref;
	 }
	 return error_mark_node;
}

nboolean class_dot_is_class_ref(ClassDot *self,tree exprValue)
{
	char *className=class_access_get_class_name((ClassAccess*)self,exprValue);
	if(className==NULL){
		 n_debug("class_dot_is_class_ref 点访问不是一个Class %s",get_tree_code_name(TREE_CODE(exprValue)));
	     //aet_print_tree(exprValue);
	}
	return className!=NULL;
}

ClassDot *class_dot_new(ClassInterface *classInterface)
{
	ClassDot *self = n_slice_alloc0 (sizeof(ClassDot));
	classDotInit(self);
	self->classInterface=classInterface;
	return self;
}




