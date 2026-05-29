/*
 * Copyright (C) 2026  zclei
 * This file is part of AET.

 * AET is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3, or (at your option) any later
 * version.

 * AET is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.

 * You should have received a copy of the GNU General Public License
 * along with GCC Exception along with this program; see the file COPYING3.
 * If not see <http://www.gnu.org/licenses/>.
 * AET was originally developed  by the onlineaet@163.com
 */

#ifndef __GCC_MTCS_CONST__
#define __GCC_MTCS_CONST__

#include "../nlib.h"
#include "mtcscomponent.h"

/*
 * 原型 fold-const.h fold-const.cc
 */
typedef struct _MtcsConst MtcsConst;
struct _MtcsConst
{
    MtcsComponent parent;
};

MtcsConst *mtcs_const_new(MtcsMode *mtcsMode);
//原型 #define build_fold_addr_expr(T)         build_fold_addr_expr_loc (UNKNOWN_LOCATION, (T))
tree mtcs_const_build_fold_addr_expr (MtcsConst *self,tree t);
//原型  build_fold_addr_expr_loc fold-const.h fold-const.cc
tree mtcs_const_build_fold_addr_expr_loc (MtcsConst *self,location_t loc, tree t);
//原型  build_fold_addr_expr_with_type_loc fold-const.h fold-const.cc
tree mtcs_const_build_fold_addr_expr_with_type_loc (MtcsConst *self,location_t loc, tree t, tree ptrtype);
//原型 fold_build1_loc fold-const.h fold-const.cc
tree mtcs_const_fold_build1_loc (MtcsConst *self,location_t loc,enum tree_code code, tree type, tree op0 MEM_STAT_DECL);
//原型 fold_unary_loc fold-const.h fold-const.cc
tree mtcs_const_fold_unary_loc (MtcsConst *self,location_t loc, enum tree_code code, tree type, tree op0);
//原型 const_unop fold-const.h fold-const.cc
tree mtcs_const_const_unop (MtcsConst *self,enum tree_code code, tree type, tree arg0);
//原型 const_binop fold-const.h fold-const.cc
tree mtcs_const_const_binop (MtcsConst *self,enum tree_code code, tree type, tree arg1, tree arg2);
//原型 size_binop_loc fold-const.h fold-const.cc
tree mtcs_const_size_binop_loc (MtcsConst *self,location_t loc, enum tree_code code, tree arg0, tree arg1);
//原型 #define size_binop(CODE,T1,T2) size_binop_loc (UNKNOWN_LOCATION, CODE, T1, T2) fold-const.h
static inline tree mtcs_const_size_binop(MtcsConst *self,enum tree_code code, tree arg0, tree arg1)
{
   return mtcs_const_size_binop_loc(self,UNKNOWN_LOCATION,code,arg0,arg1);
}
//原型 int_const_binop fold-const.h fold-const.cc
tree mtcs_const_int_const_binop (MtcsConst *self,enum tree_code code, const_tree arg1, const_tree arg2,int overflowable=1);
//原型 fold_convert_loc fold-const.h fold-const.cc
tree mtcs_const_fold_convert_loc (MtcsConst *self,location_t loc, tree type, tree arg);
//原型  #define fold_convert(T1,T2)   fold_convert_loc (UNKNOWN_LOCATION, T1, T2) fold-const.h
tree mtcs_const_fold_convert(MtcsConst *self,tree type, tree arg);
//原型 constant_boolean_node fold-const.h fold-const.cc
tree mtcs_const_constant_boolean_node (MtcsConst *self,bool value, tree type);
//原型 native_encode_expr fold-const.h fold-const.cc
int mtcs_const_native_encode_expr (MtcsConst *self,const_tree expr, unsigned char *ptr, int len, int off = -1);
//原型 native_interpret_expr fold-const.h fold-const.cc
tree mtcs_const_native_interpret_expr (MtcsConst *self,tree type, const unsigned char *ptr, int len);
//原型 native_interpret_real fold-const.h fold-const.cc
tree mtcs_const_native_interpret_real (MtcsConst *self,tree type, const unsigned char *ptr, int len);
//原型 fold_build2_loc fold-const.h fold-const.cc
tree mtcs_const_fold_build2_loc (MtcsConst *self,location_t loc,
            enum tree_code code, tree type, tree op0, tree op1  MEM_STAT_DECL);
//原型 fold_build2 fold-const.h
//#define fold_build2(c,t1,t2,t3)  fold_build2_loc (UNKNOWN_LOCATION, c, t1, t2, t3 MEM_STAT_INFO)
static inline tree mtcs_const_fold_build2(MtcsConst *self,enum tree_code code, tree t1,tree t2, tree t3)
{
   return mtcs_const_fold_build2_loc(self,UNKNOWN_LOCATION,code,t1,t2,t3);
}
//原型 fold_binary_loc fold-const.h fold-const.cc
tree mtcs_const_fold_binary_loc (MtcsConst *self,location_t loc, enum tree_code code, tree type,
       tree op0, tree op1);
//原型 #define fold_binary(CODE,T1,T2,T3)\
   fold_binary_loc (UNKNOWN_LOCATION, CODE, T1, T2, T3)
static inline tree mtcs_const_fold_binary(MtcsConst *self,enum tree_code code, tree type,tree op0, tree op1)
{
   return mtcs_const_fold_binary_loc(self,UNKNOWN_LOCATION,code,type,op0,op1);
}
//原型 fold_build_pointer_plus_loc fold-const.h fold-const.cc
tree mtcs_const_build_pointer_plus_loc (MtcsConst *self,location_t loc, tree ptr, tree off);
//原型 fold_build_pointer_plus fold-const.h
//#define fold_build_pointer_plus(p,o)  fold_build_pointer_plus_loc (UNKNOWN_LOCATION, p, o)
static inline tree mtcs_const_build_pointer_plus (MtcsConst *self, tree ptr, tree off)
{
   return mtcs_const_build_pointer_plus_loc(self,UNKNOWN_LOCATION,ptr,off);
}
//原型 convert_to_ptrofftype_loc fold-const.h fold-const.cc
tree mtcs_const_convert_to_ptrofftype_loc (MtcsConst *self,location_t loc, tree off);
//原型 convert_to_ptrofftype fold-const.h
//#define convert_to_ptrofftype(t) convert_to_ptrofftype_loc (UNKNOWN_LOCATION, t)
static inline tree mtcs_const_convert_to_ptrofftype (MtcsConst *self, tree t)
{
   return mtcs_const_convert_to_ptrofftype_loc(self,UNKNOWN_LOCATION,t);
}
//原型 fold_binary_to_constant fold-const.h fold-const.cc
tree mtcs_const_fold_binary_to_constant (MtcsConst *self,enum tree_code code, tree type, tree op0, tree op1);
//原型 fold_build_call_array_loc fold-const.h fold-const.cc
tree mtcs_const_fold_build_call_array_loc (MtcsConst *self,location_t loc, tree type, tree fn,
            int nargs, tree *argarray);
#endif

