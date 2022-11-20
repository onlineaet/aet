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
#include "toplev.h"
#include "attribs.h"
#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "opts.h"

#include "c/c-tree.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"

#include "aet-c-parser-header.h"
#include "aetutils.h"
#include "aetprinttree.h"
#include "aetinfo.h"
#include "aetexpr.h"
#include "funcmgr.h"
#include "classinfo.h"
#include "varmgr.h"
#include "classutil.h"
#include "classmgr.h"
#include "classimpl.h"
#include "classbuild.h"

static void aetExprInit(AetExpr *self)
{
}

static tree createStringActualParm(location_t loc,char *rightSysName)
{
    size_t length=strlen(rightSysName);
    tree strCst = build_string (length, (const char *) rightSysName);
    tree type = build_array_type (char_type_node,build_index_type (size_int (length)));
    TREE_TYPE(strCst)=type;
     struct c_expr expr;
     set_c_expr_source_range (&expr, loc, loc);
     expr.value=strCst;
     expr = convert_lvalue_to_rvalue (expr.get_location (), expr, true, true);
     return expr.value;

}

static  vec<tree, va_gc> *createParm(location_t loc,tree firstParm,char *rightSysName)
{
       tree secondParm=createStringActualParm(loc,rightSysName);
       vec<tree, va_gc> *parmVec;
       parmVec = make_tree_vector ();
       vec_safe_push(parmVec,firstParm);
       vec_safe_push(parmVec,secondParm);
       return parmVec;
}

static tree buildExpr(location_t loc,tree leftValue,char *rightSysName)
{
    tree valueType=TREE_TYPE(leftValue);
    tree type=build_pointer_type(void_type_node);
    tree firstParm;
    if(TREE_CODE(valueType)!=POINTER_TYPE){
         tree pointer = build_unary_op (loc, ADDR_EXPR, leftValue, FALSE);//转指针
         firstParm = build1 (NOP_EXPR, type, pointer);//转成(void*)value
    }else{
         firstParm = build1 (NOP_EXPR, type, leftValue);//转成(void*)value
    }
    tree funcName=aet_utils_create_ident(AET_VAR_OF_FUNC_NAME);
    tree decl=lookup_name(funcName);
    vec<tree, va_gc> *parms=createParm(loc,firstParm,rightSysName);
    //aet_print_tree_skip_debug(decl);
    tree newCallExpr = c_build_function_call_vec (loc, vNULL, decl,parms, NULL);
    //aet_print_tree(newCallExpr);
    return newCallExpr;
}
/**
 * 加入新关键词 varof$ 判断变量是否是某个类或接口的实例。
 * varof$是一个二元运算符。左边是变量 右边是类或接口。返回TRUE或FALSE
 * 可以在编译时确定的报错。其它在运行时确定。为了实现RTTI（Run-Time Type Identification)
 * 在类或接口的第一个元素加入魔数magic。
 * xxx varof$ YYY 表达式
 * AObject varof$ Any ok 运行时确定
 * void * varof$  Any ok 运行时确定
 * AObject varof$ 接口 ok 运行时确定
 */
struct c_expr aet_expr_varof_parser(AetExpr *self,struct c_expr lhs)
{
        c_parser *parser=self->parser;
        struct c_expr expr;
        location_t startLoc=lhs.get_start();
        tree left=lhs.value;
        printf("aet_expr_varof_parser 00\n");
        aet_print_tree(left);
        aet_print_token(c_parser_peek_token (parser));
        c_parser_consume_token (parser);//consume varof$
        c_token *token = c_parser_peek_token (parser);
        location_t loc=c_parser_peek_token (parser)->location;
        if(token->id_kind != C_ID_TYPENAME){
            error_at(loc,"varof$关键字后不是一个AET类或接口。");
            return lhs;
        }
        tree typeName=token->value;
        if(TREE_CODE(typeName)!=IDENTIFIER_NODE){
            error_at(loc,"varof$关键字后不是一个AET类或接口。");
            return lhs;
        }
        char *rightSysName=IDENTIFIER_POINTER(typeName);
        nboolean  isClass= class_mgr_is_class(class_mgr_get(),rightSysName);
        if(!isClass){
            error_at(loc,"varof$关键字后不是一个AET类或接口。");
            return lhs;
        }
        ClassInfo *rightInfo=class_mgr_get_class_info(class_mgr_get(),rightSysName);
        c_parser_consume_token (parser);//consume AObject
        tree leftType=TREE_TYPE(left);
        char  *leftSysName=class_util_get_class_name(leftType);
        ClassInfo *leftInfo=class_mgr_get_class_info(class_mgr_get(),leftSysName);
        if(leftSysName!=NULL){
            ClassRelationship  ship= class_mgr_relationship(class_mgr_get(), leftSysName,rightSysName);
            printf("说明变量和比较的类型都是AET类或接口:%s %s ship:%d\n",leftSysName,rightSysName,ship);
            if(!strcmp(leftSysName,rightSysName) || ship==CLASS_RELATIONSHIP_CHILD || ship==CLASS_RELATIONSHIP_IMPL){
                lhs.value=build_int_cst(integer_type_node,1);
            }else{
                if(class_info_is_interface(rightInfo)){
                    printf("左边%s只能在运行时才能知道是否实现了右边%s接口。\n",leftSysName,rightSysName);
                    lhs.value=buildExpr(startLoc,left,rightSysName);
                }else if(!strcmp(AET_ROOT_OBJECT,leftInfo->className.userName)){
                    printf("左边%s是一个根类。只能在运行时才能知道是否是右边%s类型。\n",leftSysName,rightSysName);
                    lhs.value=buildExpr(startLoc,left,rightSysName);
                }else if(class_info_is_interface(leftInfo) && !class_info_is_interface(rightInfo)){
                    printf("左边%s是一个接口。只能在运行时才能知道接口是从右边%s类型转化而来的。\n",leftSysName,rightSysName);
                    lhs.value=buildExpr(startLoc,left,rightSysName);
                }else{
                   error_at(startLoc,"%qs不是%qs类型。",leftSysName,IDENTIFIER_POINTER(typeName));
                   return lhs;
                }
            }
        }else{
            aet_print_tree(leftType);
            printf("lefttype is --- %s\n",get_tree_code_name(TREE_CODE(leftType)));
            if(TREE_CODE(leftType)==POINTER_TYPE){
                printf("左边是指针类型，只能在运行时确定是否可以转化为右边%s类型.\n",rightSysName);
                lhs.value=buildExpr(startLoc,left,rightSysName);
            }else{
               error_at(startLoc,"不是%qs类型。",rightSysName);
            }
        }
        return lhs;
}

void aet_expr_set_parser(AetExpr *self,c_parser *parser)
{
   self->parser=parser;
}

AetExpr *aet_expr_new()
{
    AetExpr *self = n_slice_alloc0 (sizeof(AetExpr));
    aetExprInit(self);
    return self;
}





