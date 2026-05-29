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



/**
 * setData(3.1)
 * 把实参3.1转成地址:({float axt=3.1;&axt;})
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

static int tempVarNameCount=0;

static tree convertRealOrIntegerCstToPointer(location_t loc,tree realOrIntCst,nboolean replace)
{
   if(replace){
      //		char varName[128];
      //		sprintf(varName,"realOrIntCstToPointer_%d",tempVarNameCount++);
      //		tree id=aet_utils_create_ident(varName);
      //		tree numberType=TREE_TYPE(realOrIntCst);
      //		tree varDecl=build_decl (loc, VAR_DECL, id, numberType);
      //		DECL_INITIAL(varDecl)=realOrIntCst;
      //		DECL_CONTEXT(varDecl)=current_function_decl;
      //		varDecl = pushdecl (varDecl);
      //		//add_stmt (build_stmt (DECL_SOURCE_LOCATION (varDecl),DECL_EXPR, varDecl));
      //		finish_decl (varDecl, loc, realOrIntCst,numberType, NULL_TREE);
      //		tree xx=lookup_name(id);
      //		 printf("convertRealOrIntegerCstToPointer 加入新语句 %p\n",xx);
      //		tree pointerType=build_pointer_type(numberType);
      //		tree addExpr= build1 (ADDR_EXPR, pointerType, varDecl);
      //		return addExpr;
      tree numberType=TREE_TYPE(realOrIntCst);
      char *typeName=NULL;
      class_util_get_type_name(numberType,&typeName);
      char *codes=n_strdup_printf("({%s realOrIntCstToPointer[0];realOrIntCstToPointer[0]=3;realOrIntCstToPointer;}))\n",typeName);
      tree target=generic_util_create_target(codes);
      tree bind=TREE_OPERAND (target, 1);
      tree body=TREE_OPERAND (bind, 1);
      tree_stmt_iterator it;
      int i=0;
      for (i = 0, it = tsi_start (body); !tsi_end_p (it); tsi_next (&it), i++){
         if(i==1){
            tree modify= tsi_stmt (it);
            TREE_OPERAND (modify, 1)=realOrIntCst;
         }
      }
      free(codes);
      return target;
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
	aet_print_tree(addExpr);
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
				aet_print_tree(addExpr);
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


tree generic_convert(location_t location,tree type,tree rhs,nboolean replace)
{
       enum tree_code codel = TREE_CODE (type);
       tree rhstype = TREE_TYPE (rhs);
       enum tree_code  coder = TREE_CODE (rhstype);
       tree ret=NULL_TREE;
        if (codel == POINTER_TYPE && coder == INTEGER_TYPE){
             n_debug("convertForAssignment 110 泛型 从 INTEGER_TYPE 转指针 替换吗:%d",replace);
             aet_print_tree(rhs);
             if(TREE_CODE(rhs)==INTEGER_CST){
                 n_debug("convertForAssignment 110 --XXX00 泛型 从常数转integer_type的指针");
                 return convertRealOrIntegerCstToPointer(location,rhs,replace);
             }else if(TREE_CODE(rhs)==VAR_DECL){
                 n_debug("convertForAssignment 110 --XXX11 从int类型的变量转integer_type的指针");
                 return convertRealorIntVarToPointer(location,rhs);
             }else if(TREE_CODE(rhs)==NOP_EXPR){
                 n_debug("convertForAssignment 111 --XXX11 从char short类型的变量转char short的指针");
                 return convertNopExprToPointer(location,rhs);
             }else{
                 error("不能从%qS转到指针",rhs);
                 ret=error_mark_node;
             }
     }else if (codel == POINTER_TYPE && coder == REAL_TYPE){
         n_debug("convertForAssignment 112从 real_type转指针 替换吗:%d",replace);
         if(TREE_CODE(rhs)==REAL_CST){
             n_debug("convertForAssignment 112 --XXX00 从常数转real_type的指针");
             return convertRealOrIntegerCstToPointer(location,rhs,replace);
         }else if(TREE_CODE(rhs)==VAR_DECL){
             n_debug("convertForAssignment 112 --XXX11 从real类型的变量转real_type的指针");
             return convertRealorIntVarToPointer(location,rhs);
         }else{
             error("不能从%qS转到指针",rhs);
             ret=error_mark_node;
         }
     }
     return ret;

}

