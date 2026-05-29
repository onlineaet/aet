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


#ifndef __GCC_MTCS_EXPMED__
#define __GCC_MTCS_EXPMED__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcsmicro.h"
#include "expmed.h"

//原型
/*
struct expmed_op_cheap {
  bool cheap[2][NUM_MODE_IPV_INT];
};

struct expmed_op_costs {
  int cost[2][NUM_MODE_IPV_INT];
};
*/

struct mtcs_expmed_op_cheap {
  bool cheap[2][MAX_NUM_MODE_IPV_INT/*!NUM_MODE_IPV_INT*/];
};

struct mtcs_expmed_op_costs {
  int cost[2][MAX_NUM_MODE_IPV_INT/*!NUM_MODE_IPV_INT*/];
};


typedef struct _MtcsExpmed MtcsExpmed;
struct _MtcsExpmed
{
    MtcsComponent parent;
    /* Nonzero means divides or modulus operations are relatively cheap for
       powers of two, so don't use branches; emit the operation instead.
       Usually, this will mean that the MD file will emit non-branch
       sequences.  */
    struct mtcs_expmed_op_cheap x_sdiv_pow2_cheap;
    struct mtcs_expmed_op_cheap x_smod_pow2_cheap;
    /* Cost of various pieces of RTL.  */
    int x_zero_cost[2];
    struct mtcs_expmed_op_costs x_add_cost;
    struct mtcs_expmed_op_costs x_neg_cost;
    struct mtcs_expmed_op_costs x_mul_cost;
    struct mtcs_expmed_op_costs x_sdiv_cost;
    struct mtcs_expmed_op_costs x_udiv_cost;
    int x_shift_cost[2][MAX_NUM_MODE_IPV_INT/*!NUM_MODE_IPV_INT*/][MAX_BITS_PER_WORD];
    int x_shiftadd_cost[2][MAX_NUM_MODE_IPV_INT/*!NUM_MODE_IPV_INT*/][MAX_BITS_PER_WORD];
    int x_shiftsub0_cost[2][MAX_NUM_MODE_IPV_INT/*!NUM_MODE_IPV_INT*/][MAX_BITS_PER_WORD];
    int x_shiftsub1_cost[2][MAX_NUM_MODE_IPV_INT/*!NUM_MODE_IPV_INT*/][MAX_BITS_PER_WORD];
    int x_mul_widen_cost[2][MAX_NUM_MODE_INT/*!NUM_MODE_INT*/];
    int x_mul_highpart_cost[2][MAX_NUM_MODE_INT/*!NUM_MODE_INT*/];
    int x_convert_cost[2][MAX_NUM_MODE_IP_INT/*!NUM_MODE_IP_INT*/][MAX_NUM_MODE_IP_INT/*!NUM_MODE_IP_INT*/];
    /* True if x_alg_hash might already have been used.  */
    bool x_alg_hash_used_p;
    /* Each entry of ALG_HASH caches alg_code for some integer.  This is
       actually a hash table.  If we have a collision, that the older
       entry is kicked out.  */
    struct alg_hash_entry x_alg_hash[NUM_ALG_HASH_ENTRIES];

    /* Whether reverse storage order is supported on the target.  */
    int reverse_storage_order_supported;//原型 expmed.cc 缺省=-1
    /* Whether reverse FP storage order is supported on the target.  */
    int reverse_float_storage_order_supported ;//原型 expmed.cc 缺省=-1

};

MtcsExpmed *mtcs_expmed_new(MtcsMode *mtcsMode);
//原型 expand_divmod expmed.h expmed.cc
rtx mtcs_expmed_expand_divmod (MtcsExpmed *self,int rem_flag, enum tree_code code, machine_mode mode,
           rtx op0, rtx op1, rtx target, int unsignedp, int optab_methods=3/*!enum optab_methods methods编译不过*/);//enum optab_methods = OPTAB_LIB_WIDEN
//原型 canonicalize_comparison expmed.h expmed.cc
void mtcs_expmed_canonicalize_comparison (MtcsExpmed *self ,machine_mode mode, enum rtx_code *code, rtx *imm);
//原型 make_tree tree.h expmed.cc
tree mtcs_expmed_make_tree (MtcsExpmed *self,tree type, rtx x);
//原型 emit_store_flag_force expmed.h expmed.cc
rtx mtcs_expmed_emit_store_flag_force (MtcsExpmed *self,rtx target, enum rtx_code code, rtx op0, rtx op1,
               machine_mode mode, int unsignedp, int normalizep);
//原型 emit_store_flag expmed.h expmed.cc
rtx mtcs_expmed_emit_store_flag (MtcsExpmed *self,rtx target, enum rtx_code code, rtx op0, rtx op1,
         machine_mode mode, int unsignedp, int normalizep);

//原型 expand_shift expmed.h expmed.cc
rtx mtcs_expmed_expand_shift (MtcsExpmed *self,enum tree_code code, machine_mode mode, rtx shifted,
          poly_int64 amount, rtx target, int unsignedp);
//原型 emit_cstore expmed.h expmed.cc
rtx mtcs_expmed_emit_cstore (MtcsExpmed *self,rtx target, enum insn_code icode, enum rtx_code code,
         machine_mode mode, machine_mode compare_mode,int unsignedp, rtx x, rtx y, int normalizep,machine_mode target_mode);

//原型 expand_and expmed.h expmed.cc
rtx mtcs_expmed_expand_and (MtcsExpmed *self,machine_mode mode, rtx op0, rtx op1, rtx target);
//原型 maybe_expand_shift expmed.h expmed.cc
rtx mtcs_expmed_maybe_expand_shift (MtcsExpmed *self,enum tree_code code, machine_mode mode, rtx shifted,int amount, rtx target, int unsignedp);

//原型 emit_store_flag_int expmed.h expmed.cc
rtx emit_store_flag_int (MtcsExpmed *self,rtx target, rtx subtarget, enum rtx_code code, rtx op0,
             rtx op1, scalar_int_mode mode, int unsignedp, int normalizep, rtx trueval);

//原型 emit_store_flag_int expmed.h expmed.cc
rtx mtcs_expmed_emit_store_flag_int (MtcsExpmed *self,rtx target, rtx subtarget, enum rtx_code code, rtx op0,
             rtx op1, scalar_int_mode mode, int unsignedp,int normalizep, rtx trueval);

//原型 expand_inc rtl.h expmed.cc
void mtcs_expmed_expand_inc (MtcsExpmed *self,rtx target, rtx inc);
//原型 expand_dec rtl.h expmed.cc
void mtcs_expmed_expand_dec (MtcsExpmed *self,rtx target, rtx dec);

//原型 expand_mult_highpart_adjust expmed.h expmed.cc
rtx mtcs_expmed_expand_mult_highpart_adjust (MtcsExpmed *self,scalar_int_mode mode, rtx adj_operand, rtx op0,
                 rtx op1, rtx target, int unsignedp);

//原型 choose_mult_variant expmed.h expmed.cc
bool mtcs_expmed_choose_mult_variant (MtcsExpmed *self,machine_mode mode, HOST_WIDE_INT val,
             struct algorithm *alg, int /*!enum mult_variant*/ *variant,int mult_cost);
//原型 init_expmed rtl.h expmed.cc
void mtcs_expmed_init_expmed (MtcsExpmed *self);
//原型 set_zero_cost expmed.h
int *mtcs_expmed_zero_cost_ptr (MtcsExpmed *self,bool speed);
//原型 zero_cost expmed.h
int mtcs_expmed_zero_cost (MtcsExpmed *self,bool speed);
//原型 set_zero_cost expmed.h
void mtcs_expmed_set_zero_cost (MtcsExpmed *self,bool speed, int cost);
//原型 expmed_mode_index expmed.h
int mtcs_expmed_mode_index (MtcsExpmed *self,machine_mode mode);
//原型 expmed_op_cheap_ptr expmed.h
bool *mtcs_expmed_op_cheap_ptr (MtcsExpmed *self,struct mtcs_expmed_op_cheap *eoc, bool speed, machine_mode mode);
//原型 expmed_op_cost_ptr expmed.h
int *mtcs_expmed_op_cost_ptr (MtcsExpmed *self,struct mtcs_expmed_op_costs *costs, bool speed,machine_mode mode);
//原型 add_cost_ptr expmed.h
int *mtcs_expmed_add_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode);
//原型 set_add_cost expmed.h
void mtcs_expmed_set_add_cost (MtcsExpmed *self,bool speed, machine_mode mode, int cost);
//原型 add_cost expmed.h
int mtcs_expmed_add_cost (MtcsExpmed *self,bool speed, machine_mode mode);
//原型 neg_cost_ptr expmed.h
int *mtcs_expmed_neg_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode);
//原型 set_neg_cost expmed.h
void mtcs_expmed_set_neg_cost (MtcsExpmed *self,bool speed, machine_mode mode, int cost);
//原型 neg_cost expmed.h
int mtcs_expmed_neg_cost (MtcsExpmed *self,bool speed, machine_mode mode);
//原型 expmed_mul_cost_ptr expmed.h
int *mtcs_expmed_mul_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode);
//原型 set_mul_cost expmed.h
void mtcs_expmed_set_mul_cost (MtcsExpmed *self,bool speed, machine_mode mode, int cost);
//原型 mul_cost expmed.h
int mtcs_expmed_mul_cost (MtcsExpmed *self,bool speed, machine_mode mode);
//原型 expmed_sdiv_cost_ptr expmed.h
int *mtcs_expmed_sdiv_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode);
//原型 set_sdiv_cost expmed.h
void mtcs_expmed_set_sdiv_cost (MtcsExpmed *self,bool speed, machine_mode mode, int cost);
//原型 sdiv_cost expmed.h
int mtcs_expmed_sdiv_cost (MtcsExpmed *self,bool speed, machine_mode mode);
//原型 udiv_cost_ptr expmed.h
int *mtcs_expmed_udiv_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode);
//原型 set_udiv_cost expmed.h
void mtcs_expmed_set_udiv_cost (MtcsExpmed *self,bool speed, machine_mode mode, int cost);
//原型 udiv_cost expmed.h
int mtcs_expmed_udiv_cost (MtcsExpmed *self,bool speed, machine_mode mode);
//原型 sdiv_pow2_cheap_ptr expmed.h
bool *mtcs_expmed_sdiv_pow2_cheap_ptr (MtcsExpmed *self,bool speed, machine_mode mode);
//原型 set_sdiv_pow2_cheap expmed.h
void mtcs_expmed_set_sdiv_pow2_cheap (MtcsExpmed *self,bool speed, machine_mode mode, bool cheap_p);
//原型 sdiv_pow2_cheap expmed.h
bool mtcs_expmed_sdiv_pow2_cheap (MtcsExpmed *self,bool speed, machine_mode mode);
//原型 smod_pow2_cheap_ptr expmed.h
bool *mtcs_expmed_smod_pow2_cheap_ptr (MtcsExpmed *self,bool speed, machine_mode mode);
//原型 set_smod_pow2_cheap expmed.h
void mtcs_expmed_set_smod_pow2_cheap (MtcsExpmed *self,bool speed, machine_mode mode, bool cheap);
//原型 smod_pow2_cheap expmed.h
bool mtcs_expmed_smod_pow2_cheap (MtcsExpmed *self,bool speed, machine_mode mode);
//原型 shift_cost_ptr expmed.h
int *mtcs_expmed_shift_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode, int bits);
//原型 set_shift_cost expmed.h
void mtcs_expmed_set_shift_cost (MtcsExpmed *self,bool speed, machine_mode mode, int bits, int cost);
//原型 shift_cost expmed.h
int mtcs_expmed_shift_cost (MtcsExpmed *self,bool speed, machine_mode mode, int bits);
//原型 shiftadd_cost_ptr expmed.h
int *mtcs_expmed_shiftadd_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode, int bits);
//原型 set_shiftadd_cost expmed.h
void mtcs_expmed_set_shiftadd_cost (MtcsExpmed *self,bool speed, machine_mode mode, int bits, int cost);
//原型 shiftadd_cost expmed.h
int mtcs_expmed_shiftadd_cost (MtcsExpmed *self,bool speed, machine_mode mode, int bits);
//原型 shiftsub0_cost_ptr expmed.h
int *mtcs_expmed_shiftsub0_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode, int bits);
//原型 set_shiftsub0_cost expmed.h
void mtcs_expmed_set_shiftsub0_cost (MtcsExpmed *self,bool speed, machine_mode mode, int bits, int cost);
//原型 shiftsub0_cost expmed.h
int mtcs_expmed_shiftsub0_cost (MtcsExpmed *self,bool speed, machine_mode mode, int bits);
//原型 shiftsub1_cost_ptr expmed.h
int *mtcs_expmed_shiftsub1_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode, int bits);
//原型 set_shiftsub1_cost expmed.h
void mtcs_expmed_set_shiftsub1_cost (MtcsExpmed *self,bool speed, machine_mode mode, int bits, int cost);
//原型 shiftsub1_cost expmed.h
int mtcs_expmed_shiftsub1_cost (MtcsExpmed *self,bool speed, machine_mode mode, int bits);
//原型 mul_widen_cost_ptr expmed.h
int *mtcs_expmed_mul_widen_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode);
//原型 set_mul_widen_cost expmed.h
void mtcs_expmed_set_mul_widen_cost (MtcsExpmed *self,bool speed, machine_mode mode, int cost);
//原型 mul_widen_cost expmed.h
int mtcs_expmed_mul_widen_cost (MtcsExpmed *self,bool speed, machine_mode mode);
//原型 mul_highpart_cost_ptr expmed.h
int *mtcs_expmed_mul_highpart_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode);
//原型 set_mul_highpart_cost expmed.h
void mtcs_expmed_set_mul_highpart_cost (MtcsExpmed *self,bool speed, machine_mode mode, int cost);
//原型 mul_highpart_cost expmed.h
int mtcs_expmed_mul_highpart_cost (MtcsExpmed *self,bool speed, machine_mode mode);
//原型 convert_cost_ptr expmed.h
int *mtcs_expmed_convert_cost_ptr (MtcsExpmed *self,machine_mode to_mode, machine_mode from_mode,bool speed);
//原型 set_convert_cost expmed.h
void mtcs_expmed_set_convert_cost (MtcsExpmed *self,machine_mode to_mode, machine_mode from_mode, bool speed, int cost);
//原型 convert_cost expmed.h
int mtcs_expmed_convert_cost (MtcsExpmed *self,machine_mode to_mode, machine_mode from_mode, bool speed);
//原型 alg_hash_used_p expmed.h
bool mtcs_expmed_alg_hash_used_p (MtcsExpmed *self);
//原型 alg_hash_entry_ptr expmed.h
struct alg_hash_entry *mtcs_expmed_alg_hash_entry_ptr (MtcsExpmed *self,int idx);
//原型 set_alg_hash_used_p expmed.h
void mtcs_expmed_set_alg_hash_used_p (MtcsExpmed *self,bool usedp);
//原型 negate_rtx expmed.h expmed.cc
rtx mtcs_expmed_negate_rtx (MtcsExpmed *self,machine_mode mode, rtx x);
//原型 store_bit_field expmed.h expmed.cc
void mtcs_expmed_store_bit_field (MtcsExpmed *self,rtx str_rtx, poly_uint64 bitsize, poly_uint64 bitnum,
         poly_uint64 bitregion_start, poly_uint64 bitregion_end,
         machine_mode fieldmode,  rtx value, bool reverse, bool undefined_p);

//原型 flip_storage_order expmed.h expmed.cc
rtx mtcs_expmed_flip_storage_order(MtcsExpmed *self,machine_mode mode, rtx x);

//原型 extract_bit_field expmed.h expmed.cc
rtx mtcs_expmed_extract_bit_field (MtcsExpmed *self,rtx str_rtx, poly_uint64 bitsize, poly_uint64 bitnum,
           int unsignedp, rtx target, machine_mode mode,machine_mode tmode, bool reverse, rtx *alt_rtl);

//原型 expand_mult expmed.h expmed.cc
rtx mtcs_expmed_expand_mult (MtcsExpmed *self,machine_mode mode, rtx op0, rtx op1, rtx target,
         int unsignedp, bool no_libcall=false);

//原型 emit_store_flag_force expmed.h expmed.cc
rtx mtcs_expmed_emit_store_flag_force(MtcsExpmed *self,rtx target, enum rtx_code code, rtx op0, rtx op1,
               machine_mode mode, int unsignedp, int normalizep);
//原型 expand_widening_mult optabs.h expmed.cc
rtx mtcs_expmed_expand_widening_mult (MtcsExpmed *self,machine_mode mode, rtx op0, rtx op1, rtx target,
              int unsignedp, optab this_optab);

//原型 expand_variable_shift expmed.h expmed.cc
rtx mtcs_expmed_expand_variable_shift (MtcsExpmed *self,enum tree_code code, machine_mode mode, rtx shifted,
               tree amount, rtx target, int unsignedp);
//原型 extract_low_bits expmed.h expmed.cc
rtx mtcs_expmed_extract_low_bits (MtcsExpmed *self,machine_mode mode, machine_mode src_mode, rtx src);

#endif
