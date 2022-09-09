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
#include "attribs.h"
#include "c-family/c-pragma.h"
#include "c-family/c-common.h"
#include "tree-iterator.h"

#include "c/c-tree.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"

#include "aetutils.h"
#include "classmgr.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "genericimpl.h"
#include "aet-c-parser-header.h"
#include "newfield.h"
#include "classutil.h"
#include "genericutil.h"
#include "genericfile.h"
#include "blockmgr.h"
#include "genericinfo.h"
#include "genericcommon.h"
#include "genericexpand.h"



static void newFieldInit(NewField *self)
{
	   ((NewStrategy*)self)->nest=0;
}

static void setVarProperties(NewField *self,NewObjectType varType)
{
	c_parser *parser=((NewStrategy *)self)->parser;
    new_strategy_set_var_type((NewStrategy*)self,varType);
}


static nboolean initStackFieldObject(NewField *self,tree var)
{
	c_parser *parser=((NewStrategy *)self)->parser;
	char *declName=IDENTIFIER_POINTER(DECL_NAME(var));
	char *sysClassName=class_util_get_class_name(TREE_TYPE(var));
	ClassName *className=class_mgr_clone_class_name(class_mgr_get(),sysClassName);
	new_strategy_add_new((NewStrategy*)self,className,declName);
    NString *codes=n_string_new("");
    n_string_append(codes,"{\n");
    new_strategy_new_object_from_field_stack((NewStrategy *)self,var,className,codes);
    n_string_append(codes,"}\n");
    location_t ctorLoc= c_aet_get_ctor_location(var);
    if(ctorLoc==0){
        error_at(input_location,"initStackFieldObject 不应该出现的错误，构造函数的位置是零，报告此错误。");
        return FALSE;
    }
    aet_utils_add_token_with_location(parse_in,codes->str,codes->len,ctorLoc);
	n_debug("initStackFieldObject 11 原代码:\n%s\n",codes->str);
	n_string_free(codes,TRUE);
	setVarProperties(self,NEW_OBJECT_LOCAL);
	return TRUE;
}

static nboolean initHeapFieldObject(NewField *self,tree var)
{
	c_parser *parser=((NewStrategy *)self)->parser;
	tree field=TREE_OPERAND (var, 1);
	char *declName=IDENTIFIER_POINTER(DECL_NAME(field));
	tree ctorSysClassNameTree=c_aet_get_ctor_sys_class_name(field);
	if(!aet_utils_valid_tree(ctorSysClassNameTree)){
		error_at(input_location,"initHeapFieldObject 不应该出现的错误，报告。");
		return FALSE;
	}
	char *varSysClassName=class_util_get_class_name(TREE_TYPE(field));
	char *ctorSysClassName=IDENTIFIER_POINTER(ctorSysClassNameTree);
	if(strcmp(varSysClassName,ctorSysClassName)){
		n_warning("变量的类型与new$的类型不一样。%s %s",varSysClassName,ctorSysClassName);
	}
	ClassName *className=class_mgr_clone_class_name(class_mgr_get(),ctorSysClassName);
	new_strategy_add_new((NewStrategy*)self,className,declName);
	char *tempVarName=class_util_create_new_object_temp_var_name(className->sysName,CREATE_OBJECT_METHOD_FIELD_HEAP);
	GenericModel *genericDefine= c_aet_get_generics_model(field);
	tree ctor=c_aet_get_ctor_codes(field);
	location_t ctorLoc= c_aet_get_ctor_location(field);
	char *ctorStr=NULL;
	if(aet_utils_valid_tree(ctor)){
		ctorStr=IDENTIFIER_POINTER(ctor);
	}
	if(ctorLoc==0){
        error_at(input_location,"initHeapFieldObject 不应该出现的错误，构造函数的位置是零，报告此错误。");
        return FALSE;
	}
    NString *codes=n_string_new("");
    n_string_append(codes,"=");
    new_strategy_new_object((NewStrategy *)self,tempVarName,genericDefine,className,ctorStr,codes,TRUE);
    aet_utils_add_token_with_location(parse_in,codes->str,codes->len,ctorLoc);
	n_debug("initHeapFieldObject 11 原代码:\n%s\n",codes->str);
	n_string_free(codes,TRUE);
	setVarProperties(self,NEW_OBJECT_LOCAL);
	return TRUE;
}


/**
 * Abc abc;
 * abc=new$ Abc();
 * 创建新变量 Abc *_notv3_3Abc
 * 然后通过build_unary_op把 abc地址赋给_notv3_3Abc;
 * Abc *_notv3_3Abc=&abc;
 */
nboolean new_field_modify_object(NewField *self,tree var)
{
	location_t loc=input_location;
	tree type=TREE_TYPE(var);
	if(TREE_CODE(type)==RECORD_TYPE){
		tree field=TREE_OPERAND (var, 1);
		tree pointerType=build_pointer_type(type);
		tree ctorSysClassNameTree=c_aet_get_ctor_sys_class_name(field);
		char *ctorSysClassName=IDENTIFIER_POINTER(ctorSysClassNameTree);
		char *varSysClassName=class_util_get_class_name(TREE_TYPE(field));
		if(strcmp(varSysClassName,ctorSysClassName)){
			n_warning("new_field_modify_object 变量的类型与new$的类型不一样。%s %s",varSysClassName,ctorSysClassName);
		}
		ClassName *className=class_mgr_clone_class_name(class_mgr_get(),ctorSysClassName);
		char *tempVarName=class_util_create_new_object_temp_var_name(className->sysName,CREATE_OBJECT_METHOD_FIELD_STACK);
		tree id=aet_utils_create_ident(tempVarName);
		tree varDecl=build_decl(loc, VAR_DECL, id, pointerType);
		//转地址
		tree init = build_unary_op (loc, ADDR_EXPR, var, false);
		DECL_INITIAL(varDecl)=init;
		DECL_CONTEXT(varDecl)=current_function_decl;
		varDecl = pushdecl (varDecl);
		finish_decl (varDecl, loc, init,pointerType, NULL_TREE);
		GenericModel *genericDefineModel=c_aet_get_generics_model(field);
		c_aet_set_generics_model(varDecl,genericDefineModel);
		tree ctor=c_aet_get_ctor_codes(field);
		location_t ctorLoc=c_aet_get_ctor_location(field);
		if(ctorLoc==0){
		    error_at(input_location,"构造函数的位置是0 不应该出现这样的错误。%qs %qs",ctorSysClassName,varSysClassName);
		    n_error("new_field_modify_object 不应该出现这样的错误。%s %s",ctorSysClassName,varSysClassName);
		}
		c_aet_set_ctor(varDecl,ctor,ctorSysClassNameTree,ctorLoc);
		initStackFieldObject(self,varDecl);
		n_free(tempVarName);
	}else if(TREE_CODE(type)==POINTER_TYPE){
		initHeapFieldObject(self,var);
	}
	return TRUE;
}


/**
 * 调用构造函数完成了
 */
void  new_field_finish(NewField *self,CreateClassMethod method,tree func)
{
   c_parser *parser=((NewStrategy *)self)->parser;
   ClassName *className=new_strategy_get_class_name((NewStrategy*)self);
   char *varName=new_strategy_get_var_name((NewStrategy*)self);
   NewObjectType varType=new_strategy_get_var_type((NewStrategy*)self);
   if(((NewStrategy*)self)->nest==0){
	 n_warning("new_field_finish 没有初始化变量 返回");
	 return ;
   }
   n_debug("new_field_finish  %s %s varType:%d\n",className->sysName,varName,varType);
   new_strategy_recude_new((NewStrategy*)self);
}

NewField *new_field_new()
{
	NewField *self = n_slice_alloc0 (sizeof(NewField));
	newFieldInit(self);
	return self;
}



