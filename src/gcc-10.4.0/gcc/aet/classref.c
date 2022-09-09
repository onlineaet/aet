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
#include "classref.h"
#include "classinfo.h"
#include "aetprinttree.h"
#include "classutil.h"
#include "aet-c-parser-header.h"
#include "genericimpl.h"
#include "genericmodel.h"
#include "genericutil.h"


static void classRefInit(ClassRef *self)
{
}

static nboolean exitsFunc(ClassRef *self,char *sysClassName,tree component)
{
	ClassName *className=class_mgr_get_class_name_by_sys(class_mgr_get(), sysClassName);
	if(className==NULL)
	   return FALSE;
	int re= fun_call_find_func(((ClassAccess*)self)->funcCall,className,component);
	return re==ISAET_FIND_FUNC;
}


static tree createNormalOrSuperRef(ClassRef *self,location_t loc,location_t component_loc,tree component,tree exprValue,nboolean isSuper)
{
	 c_parser *parser =((ClassAccess*)self)->parser;
	 char *lows[4];
	 int relationShip=class_access_get_last_class_and_var((ClassAccess*)self,loc,component,exprValue,lows);
	 if(relationShip<0)
		 return error_mark_node;
	 char *lowestClass=lows[2];
	 char *className=lows[3];
	 tree newComponent=aet_utils_create_temp_func_name(className,IDENTIFIER_POINTER(component));
	 n_debug("class_ref_build_deref 22 className:%s component:%s 根类:%s 根变量:%s lowestClass:%s 当前所在函数：%s 是否super调用:%d\n",
			 className,IDENTIFIER_POINTER(newComponent),lows[0],lows[1],lowestClass,IDENTIFIER_POINTER(DECL_NAME(current_function_decl)),isSuper);
	 //检查有没有component的函数名
	 nboolean haveFunc=exitsFunc(self,isSuper?className:lowestClass,component);
	 if(!haveFunc){
		 n_warning("找不到需要的函数 class:%s func:%s\n",isSuper?className:lowestClass,IDENTIFIER_POINTER(component));
		 return NULL_TREE;
	 }
	 //把函数名变成_A3Abc4open1。
	 tree tempField=class_access_create_temp_field((ClassAccess*)self,isSuper?className:lowestClass,newComponent,component_loc);
	 if(!tempField || tempField==error_mark_node){
		 error_at(loc,"创建临时的类%qs成员%qs失败。",className,IDENTIFIER_POINTER(component));
		 return error_mark_node;
	 }
	 tree datum=build_indirect_ref (loc,exprValue,RO_ARROW);
	 tree ref=class_access_build_field_ref((ClassAccess *)self,loc,datum,tempField);
	 if(!isSuper)
		 BINFO_FLAG_6(ref)=1;//这是通过类引用的方式调用函数的标记
	 else
		 BINFO_FLAG_5(ref)=1;//这是通过类引用的方式调用函数的标记并且是父调用
	 n_debug("class_ref_build_deref 33 %p %p\n",ref,DECL_LANG_SPECIFIC (ref));
	 return ref;
}

static tree processVar(ClassRef *self,location_t loc,location_t component_loc,tree component,tree exprValue)
{
	 VarRefRelation *item=class_access_create_relation_ship((ClassAccess *)self,loc,component,exprValue);
	 if(item==NULL)
		 return exprValue;
	 tree staticVar=NULL_TREE;
	 exprValue=var_call_pass_one_convert(((ClassAccess*)self)->varCall,component_loc,exprValue,component,item,&staticVar);
	 if(staticVar!=NULL_TREE){
		 //printf("class_ref_build_deref 变量处理 11 是静态变量----\n");
		 return staticVar;
	 }
	 tree datum=build_indirect_ref (loc,exprValue,RO_ARROW);
	 tree ref = build_component_ref (loc,datum,component, component_loc);
	 //printf("class_ref_build_deref 变量处理 22 是静态变量吗 %d ref:%p reftype:%p\n",staticVar!=NULL_TREE,ref,TREE_TYPE(ref));
	 return ref;
}

nboolean class_ref_is_class_ref(ClassRef *self,tree exprValue)
{
	char *className=class_access_get_class_name((ClassAccess*)self,exprValue);
	if(className==NULL){
		 n_debug("class_ref_is_class_ref 从exprValue中没有找到类！！！ %s",get_tree_code_name(TREE_CODE(exprValue)));
	}
	return className!=NULL;
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
tree  class_ref_build_deref(ClassRef *self,location_t loc,location_t component_loc,tree component,tree exprValue)
{
	 c_parser *parser =((ClassAccess*)self)->parser;
	 GenericModel *funcDefineModel=NULL;
	 if(c_parser_peek_token (parser)->type==CPP_LESS){
	     n_debug("class_ref_build_deref 是不是调用泛型函数 %s\n",IDENTIFIER_POINTER(component));
		 //不一定是泛型函数调用 比如:self->position<(int)var;
		 //所以要判断<后的内容，根据泛型定义产生式 --> <int,,,,> | <int> <符号应该是类型
		 c_token *token= c_parser_peek_2nd_token (parser);
		 if(c_token_starts_typename(token) || (token->type==CPP_NAME && generic_util_valid_id(token->value))){
			 funcDefineModel= generic_model_new(TRUE,GEN_FROM_CALL);
			 if(!generic_impl_check_func_at_call(generic_impl_get(),IDENTIFIER_POINTER(component),funcDefineModel)){
				 return NULL_TREE;
			 }
		 }
	 }
	 nboolean isFun=c_parser_peek_token (parser)->type==CPP_OPEN_PAREN;
	 if(isFun){
		 char *idStr=IDENTIFIER_POINTER(component);
	     ClassName *className=class_mgr_get_class_name_by_user(class_mgr_get(),idStr);
	     if(className==NULL)
		     className=class_mgr_get_class_name_by_sys(class_mgr_get(),idStr);
		 ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
		 n_debug("class_ref_build_deref 00 className:%s component:%s 当前所在函数：%s\n",
				 className==NULL?"null":className->sysName,idStr,IDENTIFIER_POINTER(DECL_NAME(current_function_decl)));
		 if(info!=NULL){
			 //这时构造函数
			 char *lows[4];
			 int relationShip=class_access_get_last_class_and_var((ClassAccess*)self,loc,component,exprValue,lows);
			 if(relationShip<0){
				 n_error("class_ref_build_deref 11 是构造函数 className:%s component:%s 根类:%s 根变量:%s 当前所在函数：%s\n",
										 className->sysName,IDENTIFIER_POINTER(component),lows[0],lows[1],IDENTIFIER_POINTER(DECL_NAME(current_function_decl)));
				 return error_mark_node;
			 }
			 tree tempField=class_access_create_temp_field((ClassAccess*)self,className->sysName,component,component_loc);
			 if(!tempField || tempField==error_mark_node){
				 error_at(loc,"创建临时的类%qs成员%qs失败。",className->sysName,IDENTIFIER_POINTER(component));
				 return error_mark_node;
			 }
			 tree datum=build_indirect_ref (loc,exprValue,RO_ARROW);
			 tree ref=class_access_build_field_ref((ClassAccess *)self,loc,datum, tempField);
			 n_debug("class_ref_build_deref 00-----%s\n",className->sysName);
			 BINFO_FLAG_4(ref)=1;//这时构造函数的标记
			 c_aet_set_func_generics_model(ref,funcDefineModel);
			 return ref;
		 }else{
			 /*
			  * 如果是接口调用ref或unref把component转成
              *#define IFACE_REF_FIELD_NAME            "_iface_reserve_ref_field_123"
              *#define IFACE_UNREF_FIELD_NAME          "_iface_reserve_unref_field_123"
              */
			 component=class_interface_transport_ref_and_unref(self->classInterface,component,exprValue);
			 //如果是调用外部的函数指针，把它当作变量处理
			 nboolean super=super_call_is_super_state(((ClassAccess*)self)->superCall);
			 tree ref=createNormalOrSuperRef(self,loc,component_loc,component,exprValue,super);
			 if(super)
				 super_call_minus(((ClassAccess*)self)->superCall);
			 if(ref==NULL_TREE){
				 //重新在变量中找 可能是函数指针
				// printf("重新在变量中找 可能是函数指针 00\n");
				 ref=processVar(self,loc,component_loc,component,exprValue);
				 //printf("重新在变量中找 可能是函数指针 11\n");
			 }
			 //printf("调用的泛型函数 %s\n",generic_model_tostring(funcDefineModel));
			 c_aet_set_func_generics_model(ref,funcDefineModel);
			 return ref;
		 }
	 }else{
		 nboolean super=super_call_is_super_state(((ClassAccess*)self)->superCall);
		 //printf("class_ref_build_deref  变量处理 00 还要重新处理，先这样 是不是super状态:%d\n",super);
		 //从引用的类名开始查找引用的变量，取最近的，原来还要判断右边赋值情况，想多了。重新设计
		 if(super)
			 super_call_minus(((ClassAccess*)self)->superCall);
		 tree ref=processVar(self,loc,component_loc,component,exprValue);
		 //设置input_location等于当前token
         c_c_parser_set_source_position_from_token (c_parser_peek_token (parser));
		 return ref;
	 }
	 return error_mark_node;
}


ClassRef *class_ref_new(ClassInterface *classInterface)
{
	ClassRef *self = n_slice_alloc0 (sizeof(ClassRef));
	classRefInit(self);
	self->classInterface=classInterface;
	return self;
}


