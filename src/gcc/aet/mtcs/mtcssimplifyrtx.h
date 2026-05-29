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

#ifndef __GCC_MTCS_SIMPLIFY_RTX__
#define __GCC_MTCS_SIMPLIFY_RTX__

#include "../nlib.h"
#include "mtcscomponent.h"


typedef struct _MtcsSimplifyRtx MtcsSimplifyRtx;


struct _MtcsSimplifyRtx
{
    MtcsComponent parent;
    /* Tracks the level of MEM nesting for the value being simplified:
       0 means the value is not in a MEM, >0 means it is.  This is needed
       because the canonical representation of multiplication is different
       inside a MEM than outside.  */
    nuint mem_depth ;

    /* Tracks number of simplify_associative_operation calls performed during
       outermost simplify* call.  */
    nuint assoc_count;

    /* Limit for the above number, return NULL from
       simplify_associative_operation after we reach that assoc_count.  */
     nuint max_assoc_count;
};

typedef uint8_t target_unit;

MtcsSimplifyRtx *mtcs_simplify_rtx_new(MtcsMode *mtcsMode);
//原型 class simplify_context simplify_binary_operation rtl.h  simplify-rtx.cc
rtx mtcs_simplify_rtx_binary_operation (MtcsSimplifyRtx *self,enum rtx_code code, machine_mode mode,rtx op0, rtx op1);
//原型 delegitimize_mem_from_attrs rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_delegitimize_mem_from_attrs (MtcsSimplifyRtx *self,rtx x);
//原型 simplify_const_binary_operation rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_const_binary_operation (MtcsSimplifyRtx *self,enum rtx_code code, machine_mode mode,rtx op0, rtx op1);
//原型 avoid_constant_pool_referencertl.h  simplify-rtx.cc
rtx mtcs_simplify_rtx_avoid_constant_pool_reference (MtcsSimplifyRtx *self,rtx x);
//原型 native_encode_rtx rtl.h simplify-rtx.cc
bool mtcs_simplify_rtx_native_encode_rtx (MtcsSimplifyRtx *self,machine_mode mode, rtx x, vec<target_unit> &bytes,
           unsigned int first_byte, unsigned int num_bytes);
//原型 simplify_context::simplify_subreg rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_subreg (MtcsSimplifyRtx *self,machine_mode outermode, rtx op,machine_mode innermode, poly_uint64 byte);
//原型 simplify_context::simplify_gen_subreg rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_gen_subreg (MtcsSimplifyRtx *self,machine_mode outermode, rtx op,machine_mode innermode,poly_uint64 byte);
//原型  rtx simplify_binary_operation_1 (rtx_code, machine_mode, rtx, rtx, rtx, rtx); rtl.h simplify-rtx.cc
//2000多行
rtx mtcs_simplify_rtx_binary_operation_1 (MtcsSimplifyRtx *self,rtx_code code,machine_mode mode,rtx op0, rtx op1,rtx trueop0, rtx trueop1);
//原型  simplify_context::simplify_gen_binary rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_gen_binary (MtcsSimplifyRtx *self,rtx_code code, machine_mode mode,rtx op0, rtx op1);
//原型 simplify_context::lowpart_subreg rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_lowpart_subreg (MtcsSimplifyRtx *self,machine_mode outer_mode, rtx expr,machine_mode inner_mode);
//原型 rtx simplify_context::simplify_gen_relational rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_gen_relational (MtcsSimplifyRtx *self,rtx_code code, machine_mode mode, machine_mode cmp_mode,rtx op0, rtx op1);

/* Make a unary operation by first seeing if it folds and otherwise making
   the specified operation.  */
//原型 rtx simplify_context::simplify_gen_unary  rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_gen_unary (MtcsSimplifyRtx *self,rtx_code code, machine_mode mode, rtx op,machine_mode op_mode);
//原型 rtx simplify_context::simplify_unary_operation  rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_unary_operation (MtcsSimplifyRtx *self,rtx_code code, machine_mode mode,rtx op, machine_mode op_mode);
//原型 rtx simplify_const_unary_operation  rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_const_unary_operation (MtcsSimplifyRtx *self,enum rtx_code code, machine_mode mode, rtx op, machine_mode op_mode);
//原型 simplify_context::simplify_unary_operation_1 rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_unary_operation_1 (MtcsSimplifyRtx *self,rtx_code code, machine_mode mode,rtx op);

//原型 rtx simplify_context::simplify_gen_ternary rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_gen_ternary (MtcsSimplifyRtx *self,rtx_code code, machine_mode mode,
                    machine_mode op0_mode, rtx op0, rtx op1, rtx op2);

//原型 rtx simplify_context::simplify_ternary_operation rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_ternary_operation (MtcsSimplifyRtx *self,rtx_code code, machine_mode mode,
                          machine_mode op0_mode,rtx op0, rtx op1, rtx op2);

//原型 simplify_const_relational_operation rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_const_relational_operation (MtcsSimplifyRtx *self,enum rtx_code code,machine_mode mode,rtx op0, rtx op1);

//原型 rtx simplify_context::simplify_relational_operation rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_relational_operation (MtcsSimplifyRtx *self,rtx_code code,machine_mode mode, machine_mode cmp_mode,rtx op0, rtx op1);

//原型 simplify_context::simplify_plus_minus  rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_plus_minus (MtcsSimplifyRtx *self,rtx_code code, machine_mode mode, rtx op0, rtx op1);

//原型 rtx simplify_context::simplify_associative_operation rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_associative_operation ( MtcsSimplifyRtx *self,rtx_code code,machine_mode mode,rtx op0, rtx op1);

//原型rtx simplify_context::simplify_binary_operation_series rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_binary_operation_series (MtcsSimplifyRtx *self,rtx_code code,machine_mode mode, rtx op0, rtx op1);

//原型 rtx simplify_context::simplify_distributive_operation rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_distributive_operation ( MtcsSimplifyRtx *self,rtx_code code,machine_mode mode,rtx op0, rtx op1);
//原型 rtx simplify_context::simplify_byte_swapping_operation rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_byte_swapping_operation (MtcsSimplifyRtx *self,rtx_code code,machine_mode mode,rtx op0, rtx op1);
//原型 rtx simplify_context::simplify_logical_relational_operation rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_logical_relational_operation (MtcsSimplifyRtx *self,rtx_code code,machine_mode mode,rtx op0, rtx op1);
//原型 bool mode_signbit_p rtl.h simplify-rtx.cc
bool mtcs_simplify_rtx_mode_signbit_p (MtcsSimplifyRtx *self,machine_mode mode, const_rtx x);
//原型 simplify_truncation rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_truncation (MtcsSimplifyRtx *self,machine_mode mode, rtx op, machine_mode op_mode);

rtx mtcs_simplify_rtx_native_decode_vector_rtx (MtcsSimplifyRtx *self,machine_mode mode, const vec<target_unit> &bytes,
              unsigned int first_byte, unsigned int npatterns,unsigned int nelts_per_pattern);

//原型 native_decode_vector_rtx rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_native_decode_vector_rtx (MtcsSimplifyRtx *self,machine_mode mode, const vec<target_unit> &bytes,
              unsigned int first_byte, unsigned int npatterns,unsigned int nelts_per_pattern);

//原型 native_decode_rtx rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_native_decode_rtx (MtcsSimplifyRtx *self,machine_mode mode, const vec<target_unit> &bytes,unsigned int first_byte);

//原型 simplify_context::simplify_relational_operation_1 rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_relational_operation_1 (MtcsSimplifyRtx *self,rtx_code code,machine_mode mode,machine_mode cmp_mode,rtx op0, rtx op1);
//原型 simplify_rtx rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_simplify_rtx (MtcsSimplifyRtx *self,const_rtx x);
//原型 simplify_context::simplify_merge_mask rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_merge_mask (MtcsSimplifyRtx *self,rtx x, rtx mask, int op);
//原型 simplify_context::simplify_cond_clz_ctz rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_cond_clz_ctz (MtcsSimplifyRtx *self,rtx x, rtx_code cmp_code,rtx true_val, rtx false_val);

//原型 val_signbit_p rtl.h simplify-rtx.cc
bool mtcs_simplify_rtx_val_signbit_p (MtcsSimplifyRtx *self,machine_mode mode, unsigned HOST_WIDE_INT val);

//原型 reverse_rotate_by_imm_p rtl.h simplify-rtx.cc
bool mtcs_simplify_rtx_reverse_rotate_by_imm_p (MtcsSimplifyRtx *self,machine_mode mode, unsigned int left, rtx op1);
//原型 simplify_replace_fn_rtx rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_simplify_replace_fn_rtx (MtcsSimplifyRtx *self,rtx x, const_rtx old_rtx,
          rtx (*fn) (rtx, const_rtx, void *), void *data);
//原型 simplify_replace_rtx rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_simplify_replace_rtx (MtcsSimplifyRtx *self,rtx x, const_rtx old_rtx, rtx new_rtx);
//原型 simplify_context::simplify_gen_vec_select rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_simplify_gen_vec_select (MtcsSimplifyRtx *self,rtx op, unsigned int index);
//原型 val_signbit_known_set_p rtl.h simplify-rtx.cc
bool mtcs_simplify_rtx_val_signbit_known_set_p (MtcsSimplifyRtx *self,machine_mode mode, unsigned HOST_WIDE_INT val);

#endif
