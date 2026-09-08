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
#include "tree-iterator.h"
#include "asan.h"

#include "c-aet.h"
#include "nlib.h"
#include "aetutils.h"
#include "aetprinttree.h"
#include "genericutil.h"
#include "classutil.h"

static int tempVarCount = 0;

/**
 * 常数赋值给变量，并插入赋值语名到函数第一条语句的位置
 */
static tree modifyCst(tree fndecl, tree realOrIntCst)
{
   tree body = DECL_SAVED_TREE (fndecl);
   gcc_assert (body != NULL_TREE);
  // tree var = make_const_var (fndecl, realOrIntCst);
   char varname[255];
   sprintf(varname,"_temp_generic_const_var_%d",tempVarCount++);
   tree var = build_decl (DECL_SOURCE_LOCATION (fndecl),VAR_DECL, get_identifier (varname), TREE_TYPE(realOrIntCst));
   tree decl_expr = build1 (DECL_EXPR, void_type_node, var);
   tree init_stmt = build2_loc(DECL_SOURCE_LOCATION (fndecl), MODIFY_EXPR,
                               TREE_TYPE(var),var,realOrIntCst);
   if (TREE_CODE (body) == STATEMENT_LIST){
      /* 已有语句列表：插到最前面 */
      tree_stmt_iterator i = tsi_start (body);
      tsi_link_before (&i, decl_expr, TSI_SAME_STMT);
      tsi_link_before (&i, init_stmt, TSI_SAME_STMT);

   }else if (TREE_CODE (body) == BIND_EXPR){
      /* 少数情况已经是 BIND：挂 vars，并插到 BIND 的 body 最前 */
      TREE_CHAIN (var) = BIND_EXPR_VARS (body);
      BIND_EXPR_VARS (body) = var;
      if (BIND_EXPR_BLOCK (body) != NULL_TREE)
         BLOCK_VARS (BIND_EXPR_BLOCK (body)) = BIND_EXPR_VARS (body);

      tree *stmt_p = &BIND_EXPR_BODY (body);
      gcc_assert (*stmt_p != NULL_TREE);
      if (TREE_CODE (*stmt_p) == STATEMENT_LIST){
         tree_stmt_iterator i = tsi_start (*stmt_p);
         tsi_link_before (&i, decl_expr, TSI_SAME_STMT);
         tsi_link_before (&i, init_stmt, TSI_SAME_STMT);
      }else{
         /* body 是单条语句：收成 list，声明在前 */
         tree sl = alloc_stmt_list ();
         append_to_statement_list (decl_expr, &sl);
         append_to_statement_list (init_stmt, &sl);
         append_to_statement_list (*stmt_p, &sl);
         *stmt_p = sl;
      }
   }else{
      /* 单条语句（例如目前只有一个 RETURN_EXPR）：收成 list，声明在前 */
      tree sl = alloc_stmt_list ();
      append_to_statement_list (decl_expr, &sl);
      append_to_statement_list (init_stmt, &sl);
      append_to_statement_list (body, &sl);
      DECL_SAVED_TREE (fndecl) = sl;
   }
   return var;
}


/**
 * setData(3.1)
 * 把实参3.1转成地址:({float axt=3.1;&axt;})
 * 用在函数选择，如果建call_expr,用临时变量，并插在函数开头
 */
static tree convertRealOrIntegerCstToPointer_new(location_t loc,tree realOrIntCst)
{
   tree type= TREE_TYPE(realOrIntCst);
   tree pointerType=build_pointer_type(type);
   tree varDecl=build_decl(loc, VAR_DECL, NULL_TREE, pointerType);
   DECL_ARTIFICIAL (varDecl)=1;
   TREE_USED (varDecl)=1;
   tree indextype= build_index_type (size_int(0));
   tree arrayType=build_array_type(type,indextype);
   TYPE_SIZE (arrayType)=build_int_cst (integer_type_node, 0);
   tree bindVarDecl = build_decl (0,VAR_DECL,aet_utils_create_ident("realOrIntCstToPointer"), arrayType);

   TREE_USED (bindVarDecl)=1;
   DECL_EXTERNAL(bindVarDecl)=0;
   TREE_STATIC(bindVarDecl)=0;
   TREE_PUBLIC(bindVarDecl)=0;
   DECL_CONTEXT(bindVarDecl)=current_function_decl;

   tree stmtList=alloc_stmt_list();
   tree stmt0 = build_stmt (loc, DECL_EXPR, varDecl/*!或bindVarDecl*/);
   append_to_statement_list_force (stmt0, &stmtList);
   {
      unsigned HOST_WIDE_INT value;
      value=0;
      tree rvalue=realOrIntCst;//build_int_cst (integer_type_node, value);
      tree index=build_int_cst(integer_type_node,0);
      tree result = build4 (ARRAY_REF, type, bindVarDecl, index, NULL_TREE,NULL_TREE);
      tree stmt2 = build_modify_expr (loc, result, TREE_TYPE(rvalue),NOP_EXPR,loc,rvalue,TREE_TYPE(rvalue));
      append_to_statement_list_force (stmt2, &stmtList);
   }

   tree op0 = build1 (ADDR_EXPR, build_pointer_type(arrayType), bindVarDecl);// @104 op component_ref
   tree noexpr = build1 (NOP_EXPR, pointerType,op0);//@47 strcpy的第一个参数

   //tree stmt3 = build_modify_expr (loc, varDecl, void_type_node,NOP_EXPR,loc,bindVarDecl,TREE_TYPE(bindVarDecl));
   //tree stmt3 = build_modify_expr (loc, varDecl, void_type_node,NOP_EXPR,loc,noexpr,TREE_TYPE(noexpr));
   // tree stmt3 = build_modify_expr (loc, varDecl, void_type_node,NOP_EXPR,loc,noexpr,TREE_TYPE(noexpr));
   tree stmt3 = build_modify_expr (loc, varDecl, NULL_TREE,NOP_EXPR,loc,noexpr,NULL_TREE);
   //printf("这是临时的 TREE_TYPE(stmt3)=void_type_node\n");
   TREE_TYPE(stmt3)=void_type_node;
   append_to_statement_list_force (stmt3, &stmtList);
   tree bind = build3 (BIND_EXPR, void_type_node, bindVarDecl, stmtList, NULL_TREE);
   tree target = build4 (TARGET_EXPR, pointerType, varDecl, bind, NULL_TREE, NULL_TREE);
   return target;
}


/**
 * replace=true 创建真实的调用
 * replace=fale,只是用在选择函数
 * 如果是在文件中创建泛型对象
 * static TFirst *tempxd = new$ TFirst<int>(5);
 * 编译器生成一个constructor函数来包裹新的对象，代码如下：
 * static TFirst *tempxd = new$ TFirst<int>(5);
 * 变成这样：
 * static TFirst *tempxd =NULL;
 * static __attribute__((constructor)) void TFirst_tempxd_3460645734_ctor()
{
tempxd=({
   TFirst<int > *_notv2_6TFirst0;
   unsigned int _mtcsPlatType0=0;
   ...
 */
static tree convertRealOrIntegerCstToPointer(location_t loc,tree realOrIntCst,nboolean replace)
{
   if(replace){
      tree ret = modifyCst(current_function_decl,realOrIntCst);
      tree pointerType=build_pointer_type(TREE_TYPE(realOrIntCst));
      tree addExpr= build1 (ADDR_EXPR, pointerType, ret);
      n_debug("常数转地址\n");
      return addExpr;
   }else{
      return convertRealOrIntegerCstToPointer_new(loc,realOrIntCst);
   }
}


/**
 * 创建real转指针。
 * 从变量转指针，如:float value=5.1;setData(value);
 * 把实参转成形如：&value的地址;
 */
static tree convertRealorIntVarToPointer(location_t loc,tree var)
{
   tree realOrIntType=TREE_TYPE(var);
   tree pointerType=build_pointer_type(realOrIntType);
   tree addExpr= build1 (ADDR_EXPR, pointerType, var);
   return addExpr;
}

static tree convertNopExprToPointer(location_t loc,tree nopExpr)
{
   tree nopExprType=TREE_TYPE(nopExpr);
   tree op0=TREE_OPERAND (nopExpr, 0);
   if(TREE_CODE(nopExprType)==INTEGER_TYPE){
      if(TREE_CODE(op0)==VAR_DECL){
         tree vtype=TREE_TYPE(op0);
         if(TREE_CODE(vtype)==INTEGER_TYPE){
            tree pointerType=build_pointer_type(vtype);
            tree addExpr= build1 (ADDR_EXPR, pointerType, op0);
            return addExpr;
         }else{
            error_at(loc,"不能处理NOP_EXPR的OP是变量的类型。%qs",get_tree_code_name(TREE_CODE(vtype)));
         }
      }else{
         error_at(loc,"不能处理NOP_EXPR的OP。%qs",get_tree_code_name(TREE_CODE(op0)));
      }
   }else{
      error_at(loc,"不能处理NOP_EXPR的类型。%qs",get_tree_code_name(TREE_CODE(nopExprType)));
   }
   return NULL_TREE;
}

/**
 * 引用转指针
 */
static tree convertComponentRefToPointer(location_t loc,tree componetRef)
{
   tree type=TREE_TYPE(componetRef);
   if(TREE_CODE(type)==INTEGER_TYPE || TREE_CODE(type)==REAL_TYPE){
      tree pointerType=build_pointer_type(type);
      tree addExpr= build1 (ADDR_EXPR, pointerType, componetRef);
      return addExpr;
   }else{
      error_at(loc,"不能处理COMPONENT_REF的类型。%qs",get_tree_code_name(TREE_CODE(type)));
   }
   return NULL_TREE;
}

/**
 * 转化数组到指针
 */
static tree convertArrayRefToPointer(location_t loc,tree arrayRef)
{
   tree type=TREE_TYPE(arrayRef);
   if(TREE_CODE(type)==INTEGER_TYPE || TREE_CODE(type)==REAL_TYPE){
      tree pointerType=build_pointer_type(type);
      tree addExpr= build1 (ADDR_EXPR, pointerType, arrayRef);
      return addExpr;
   }else{
      error_at(loc,"不能处理 ARRAY_REF 的类型。%qs",get_tree_code_name(TREE_CODE(type)));
   }
   return NULL_TREE;
}

static inline bool div_or_mod_p (enum tree_code code)
{
  switch (code)
    {
    case TRUNC_DIV_EXPR:
    case FLOOR_DIV_EXPR:
    case CEIL_DIV_EXPR:
    case ROUND_DIV_EXPR:
    case EXACT_DIV_EXPR:
    case TRUNC_MOD_EXPR:
    case FLOOR_MOD_EXPR:
    case CEIL_MOD_EXPR:
    case ROUND_MOD_EXPR:
      return true;
    default:
      return false;
    }
}

/**
 * 转泛型参数
 * replace = FALE ，用在选择函数
 * replace =TRUE,创建调用
 */
tree generic_convert(location_t loc,tree type,tree rhs,nboolean replace)
{
   enum tree_code codel = TREE_CODE (type);
   tree rhstype = TREE_TYPE (rhs);
   enum tree_code  coder = TREE_CODE (rhstype);
   tree ret=NULL_TREE;
   if (codel == POINTER_TYPE && (coder == INTEGER_TYPE || coder == REAL_TYPE)){
      n_debug("generic_convert 00 泛型 从 %s转%s 转指针 替换吗:%d",
            get_tree_code_name(codel),get_tree_code_name(coder),replace);
      if(TREE_CODE(rhs)==INTEGER_CST || coder == REAL_TYPE){
         n_debug("generic_convert 00-11  从%s常数转指针",get_tree_code_name(TREE_CODE(rhs)));
         return convertRealOrIntegerCstToPointer(loc,rhs,replace);
      }else if(TREE_CODE(rhs)==VAR_DECL || TREE_CODE(rhs)==PARM_DECL){
         n_debug("generic_convert 00-22 从VAR_DECL类型的变量转指针",get_tree_code_name(TREE_CODE(rhs)));
         return convertRealorIntVarToPointer(loc,rhs);
      }else if(TREE_CODE(rhs)==NOP_EXPR){
         n_debug("generic_convert 00-33 从NOP_EXPR类型的变量转指针");
         return convertNopExprToPointer(loc,rhs);
      }else if(TREE_CODE(rhs)==COMPONENT_REF){
         n_debug("generic_convert 00-44 从component_ref类型的变量转指针");
         return convertComponentRefToPointer(loc,rhs);
      }else if(TREE_CODE(rhs)==ARRAY_REF){
         n_debug("generic_convert 00-55 从array_ref类型的变量转指针");
         return convertArrayRefToPointer(loc,rhs);
      }else if (TREE_CODE(rhs)==MULT_EXPR ||TREE_CODE(rhs)==PLUS_EXPR ||
            TREE_CODE(rhs)==MINUS_EXPR || div_or_mod_p(TREE_CODE(rhs))
            || TREE_CODE(rhs)==BIT_NOT_EXPR){
        // printf("出现了乘法 %s\n",get_tree_code_name(coder));
         n_debug("generic_convert 00-66 从 MULT_EXPR 类PLUS_EXPR MINUS_EXPR div_or_mod_p BIT_NOT_EXPR 型的变量转指针");
         tree type=TREE_TYPE(rhs);
         tree pointerType=build_pointer_type(type);
         tree addExpr= build1 (ADDR_EXPR, pointerType, rhs);
         return addExpr;
      }else if(TREE_CODE(rhs)==INDIRECT_REF){
         n_debug("generic_convert 00-77 从 INDIRECT_REF 类型的变量转指针");
         tree type=TREE_TYPE(rhs);
         tree pointerType=build_pointer_type(type);
         tree addExpr= build1 (ADDR_EXPR, pointerType, rhs);
         return addExpr;
      }else{
         aet_print_tree_skip_debug(rhs);
         error("不能从%qs转到整形指针",rhs);
         ret=error_mark_node;
      }
   }else if(codel == POINTER_TYPE && coder == RECORD_TYPE){
       n_debug("generic_convert  RECORD_TYPE 替换吗:%d",replace);
       if(TREE_CODE(rhs)==VAR_DECL){
           n_debug("generic_convert RECORD_TYPE 变量");
           return convertRealorIntVarToPointer(loc,rhs);
       }
   }
   return ret;
}



