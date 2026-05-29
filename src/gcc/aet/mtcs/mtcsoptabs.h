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

#ifndef __GCC_MTCS_OPTABS__
#define __GCC_MTCS_OPTABS__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcsmicro.h"
#include "optabs-query.h"
#include "vec-perm-indices.h"
#include "memmodel.h"


typedef struct _MtcsOptabs MtcsOptabs;


struct _MtcsOptabs
{
    MtcsComponent parent;
};

MtcsOptabs *mtcs_optabs_new(MtcsMode *mtcsMode);
//原型 expand_unop optabs.h optabs.cc
rtx mtcs_optabs_expand_unop (MtcsOptabs *self,machine_mode mode, optab unoptab, rtx op0, rtx target,int unsignedp);
//原型 expand_binop optabs.h optabs.cc
rtx mtcs_optabs_expand_binop (MtcsOptabs *self,machine_mode mode, optab binoptab, rtx op0,
        rtx op1,rtx target, int unsignedp, int /*!enum optab_methods methods 编译通不过*/ methods);

//原型 find_widening_optab_handler_and_mode optabs-query.h optabs-query.cc
enum insn_code mtcs_optabs_find_widening_optab_handler_and_mode (MtcsOptabs *self,optab op, machine_mode to_mode,
                      machine_mode from_mode, machine_mode *found_mode);

//原型 find_widening_optab_handler optabs-query.h
//#define find_widening_optab_handler(A, B, C) \
  //find_widening_optab_handler_and_mode (A, B, C, NULL)
enum insn_code mtcs_optabs_find_widening_optab_handler (MtcsOptabs *self,optab op, machine_mode to_mode,machine_mode from_mode);
//原型 emit_unop_insn optabs.h optabs.cc
void mtcs_optabs_emit_unop_insn (MtcsOptabs *self,enum insn_code icode, rtx target, rtx op0, enum rtx_code code);
//原型 maybe_emit_unop_insn optabs.h optabs.cc
bool mtcs_optabs_maybe_emit_unop_insn (MtcsOptabs *self,enum insn_code icode, rtx target, rtx op0,enum rtx_code code);
//原型 maybe_gen_insn optabs.h optabs.cc
rtx_insn *mtcs_optabs_maybe_gen_insn (MtcsOptabs *self,enum insn_code icode, unsigned int nops,class expand_operand *ops);
//原型 maybe_legitimize_operands optabs.h optabs.cc
bool mtcs_optabs_maybe_legitimize_operands (MtcsOptabs *self,enum insn_code icode, unsigned int opno,
               unsigned int nops, class expand_operand *ops);
//原型 insn_operand_matches optabs.h optabs.cc
bool mtcs_optabs_insn_operand_matches (MtcsOptabs *self,enum insn_code icode, unsigned int opno, rtx operand);
//原型 expand_vector_broadcast optabs.h optabs.cc
rtx mtcs_optabs_expand_vector_broadcast (MtcsOptabs *self,machine_mode vmode, rtx op);
//原型 expand_insn optabs.h optabs.cc
void mtcs_optabs_expand_insn (MtcsOptabs *self,enum insn_code icode, unsigned int nops,class expand_operand *ops);
//原型 maybe_expand_insn optabs.h optabs.cc
bool mtcs_optabs_maybe_expand_insn (MtcsOptabs *self,enum insn_code icode, unsigned int nops,class expand_operand *ops);
//原型 emit_cmp_and_jump_insns optabs.h optabs.cc 重载函数
void mtcs_optabs_emit_cmp_and_jump_insns (MtcsOptabs *self,rtx x, rtx y, enum rtx_code comparison, rtx size,
             machine_mode mode, int unsignedp, rtx label, profile_probability prob= profile_probability::uninitialized ());

//原型 emit_cmp_and_jump_insns optabs.h optabs.cc 重载函数 多一个 tree val参数
void mtcs_optabs_emit_cmp_and_jump_insns (MtcsOptabs *self,rtx x, rtx y, enum rtx_code comparison, rtx size,
             machine_mode mode, int unsignedp, tree val, rtx label,profile_probability prob= profile_probability::uninitialized ());
//原型 can_compare_p optabs.h optabs.cc
bool mtcs_optabs_can_compare_p (MtcsOptabs *self,enum rtx_code code, machine_mode mode,int /*enum can_compare_purpose编译通不过*/ purpose);
//原型 can_float_p optabs-query.h optabs-query.cc
enum insn_code mtcs_optabs_can_float_p (MtcsOptabs *self,machine_mode fltmode, machine_mode fixmode,int unsignedp);

//原型 can_extend_p optabs-query.h optabs-query.cc
enum insn_code mtcs_optabs_can_extend_p (MtcsOptabs *self,machine_mode to_mode, machine_mode from_mode, int unsignedp);

//原型 expand_simple_binop optabs.h optabs.cc 不支持enum optab_methods methods编译不通过改成int
rtx mtcs_optabs_expand_simple_binop (MtcsOptabs *self,machine_mode mode, enum rtx_code code, rtx op0,
             rtx op1, rtx target, int unsignedp, int optab_methods /*!enum optab_methods methods*/);

//原型 prepare_operand optabs.h optabs.cc
rtx mtcs_optabs_prepare_operand (MtcsOptabs *self,enum insn_code icode, rtx x, int opnum, machine_mode mode,
         machine_mode wider_mode, int unsignedp);

//原型 emit_libcall_block optabs.h optabs.cc
void mtcs_optabs_emit_libcall_block (MtcsOptabs *self,rtx_insn *insns, rtx target, rtx result, rtx equiv);

//原型 expand_simple_unop optabs.h optabs.cc
rtx mtcs_optabs_expand_simple_unop (MtcsOptabs *self,machine_mode mode, enum rtx_code code, rtx op0,
            rtx target, int unsignedp);

//原型 emit_conditional_move optabs.h optabs.cc 重载函数
rtx mtcs_optabs_emit_conditional_move (MtcsOptabs *self,rtx target, struct rtx_comparison comp,
               rtx op2, rtx op3,  machine_mode mode, int unsignedp);
//原型 emit_conditional_move optabs.h optabs.cc 重载函数
rtx mtcs_optabs_emit_conditional_move(MtcsOptabs *self,rtx target, rtx comparison,
               rtx rev_comparison,rtx op2, rtx op3, machine_mode mode);
//原型 expand_doubleword_divmod optabs.h optabs.cc
rtx mtcs_optabs_expand_doubleword_divmod (MtcsOptabs *self,machine_mode mode, rtx op0, rtx op1, rtx *rem,bool unsignedp);

//原型 force_expand_binop optabs.h optabs.cc
bool mtcs_optabs_force_expand_binop ( MtcsOptabs *self,machine_mode mode, optab binoptab,
            rtx op0, rtx op1, rtx target, int unsignedp, int /*!enum optab_methods编译通不过*/ methods);

//原型 simplify_expand_binop optabs.h optabs.cc
rtx mtcs_optabs_simplify_expand_binop (MtcsOptabs *self,machine_mode mode, optab binoptab,
               rtx op0, rtx op1, rtx target, int unsignedp,  int /*!enum optab_methods编译通不过*/ methods);

//原型 valid_multiword_target_p optabs.h optabs.cc
bool mtcs_optabs_valid_multiword_target_p (MtcsOptabs *self,rtx target);
//原型 get_best_mem_extraction_insn optabs-query.h optabs-query.cc
bool mtcs_optabs_get_best_mem_extraction_insn (MtcsOptabs *self,extraction_insn *insn,
                  enum extraction_pattern pattern,
                  HOST_WIDE_INT bitsize, HOST_WIDE_INT bitnum,
                  machine_mode field_mode);

//原型 get_best_reg_extraction_insn optabs-query.h optabs-query.cc
bool mtcs_optabs_get_best_reg_extraction_insn (MtcsOptabs *self,extraction_insn *insn,
                  enum extraction_pattern pattern,
                  unsigned HOST_WIDE_INT struct_bits,
                  machine_mode field_mode);
//原型 expand_fix optabs.h optabs.cc
void mtcs_optabs_expand_fix (MtcsOptabs *self,rtx to, rtx from, int unsignedp);
//原型 expand_float optabs.h optabs.cc
void mtcs_optabs_expand_float (MtcsOptabs *self,rtx to, rtx from, int unsignedp);

//原型 can_fix_p optabs-query.h optabs-query.cc
enum insn_code mtcs_optabs_can_fix_p (MtcsOptabs *self,machine_mode fixmode, machine_mode fltmode,
       int unsignedp, bool *truncp_ptr);

//原型 gen_add2_insn optabs.h optabs.cc
rtx_insn *mtcs_optabs_gen_add2_insn (MtcsOptabs *self,rtx x, rtx y);
//原型 init_tree_optimization_optabs optabs-tree.h optabs-tree.cc
void mtcs_optabs_init_tree_optimization_optabs (MtcsOptabs *self,tree optnode);
//原型 gen_extend_insn optabs.h optabs.cc
rtx_insn *mtcs_optabs_gen_extend_insn (MtcsOptabs *self,rtx x, rtx y, machine_mode mto,
         machine_mode mfrom, int unsignedp);
//原型 maybe_expand_jump_insn optabs.h optabs.cc
bool mtcs_optabs_maybe_expand_jump_insn (MtcsOptabs *self,enum insn_code icode, unsigned int nops,
            class expand_operand *ops);
//原型 expand_jump_insn optabs.h optabs.cc
void mtcs_optabs_expand_jump_insn (MtcsOptabs *self,enum insn_code icode, unsigned int nops,
          class expand_operand *ops);
//原型 emit_indirect_jump optabs.h optabs.cc
void mtcs_optabs_emit_indirect_jump (MtcsOptabs *self,rtx loc);
//原型 expand_widen_pattern_expr optabs.h optabs.cc
rtx mtcs_optabs_expand_widen_pattern_expr (MtcsOptabs *self,sepops ops, rtx op0, rtx op1, rtx wide_op,
               rtx target, int unsignedp);
//原型 expand_mult_highpart optabs.h optabs.cc
rtx mtcs_optabs_expand_mult_highpart (MtcsOptabs *self,machine_mode mode, rtx op0, rtx op1,
              rtx target, bool uns_p);
//原型 can_mult_highpart_p optabs-query.h optabs-query.cc
int mtcs_optabs_can_mult_highpart_p (MtcsOptabs *self,machine_mode mode, bool uns_p);
//原型 can_vec_perm_const_p optabs-query.h optabs-query.cc
bool mtcs_optabs_can_vec_perm_const_p (MtcsOptabs *self,machine_mode mode, machine_mode op_mode,
              const vec_perm_indices &sel, bool allow_variable_p=true);
//原型 selector_fits_mode_p optabs-query.h optabs-query.cc
bool mtcs_optabs_selector_fits_mode_p (MtcsOptabs *self,machine_mode mode, const vec_perm_indices &sel);
//原型 qimode_for_vec_perm optabs-query.h optabs-query.cc
opt_machine_mode mtcs_optabs_qimode_for_vec_perm (MtcsOptabs *self,machine_mode mode);
//原型 expand_vec_perm_const optabs.h optabs.cc
rtx mtcs_optabs_expand_vec_perm_const (MtcsOptabs *self,machine_mode mode, rtx v0, rtx v1,
               const vec_perm_builder &sel, machine_mode sel_mode,rtx target);
//原型 expand_fixed_convert optabs.h optabs.cc
void mtcs_optabs_expand_fixed_convert (MtcsOptabs *self,rtx to, rtx from, int uintp, int satp);
//原型 expand_abs optabs.h optabs.cc
rtx mtcs_optabs_expand_abs (MtcsOptabs *self,machine_mode mode, rtx op0, rtx target,
        int result_unsignedp, int safe);
//原型 expand_abs_nojump optabs.h optabs.cc
rtx mtcs_optabs_expand_abs_nojump (MtcsOptabs *self,machine_mode mode, rtx op0, rtx target,
           int result_unsignedp);
//原型 can_conditionally_move_p optabs-query.h optabs-query.cc
bool mtcs_optabs_can_conditionally_move_p (MtcsOptabs *self,machine_mode mode);
//原型 have_insn_for optabs.h optabs.cc
bool mtcs_optabs_have_insn_for (MtcsOptabs *self,enum rtx_code code, machine_mode mode);
//原型 expand_vec_cmp_expr optabs.h optabs.cc
rtx mtcs_optabs_expand_vec_cmp_expr (MtcsOptabs *self,tree type, tree exp, rtx target);
//原型 get_vec_cmp_icode optabs-query.h
enum insn_code mtcs_optabs_get_vec_cmp_icode (MtcsOptabs *self,machine_mode vmode, machine_mode mask_mode, bool uns);
rtx mtcs_optabs_vector_compare_rtx (MtcsOptabs *self,machine_mode cmp_mode, enum tree_code tcode,
            tree t_op0, tree t_op1, bool unsignedp, enum insn_code icode, unsigned int opno);
//原型 get_vec_cmp_eq_icode optabs-query.h
enum insn_code mtcs_optabs_get_vec_cmp_eq_icode (MtcsOptabs *self, machine_mode vmode, machine_mode mask_mode);
//原型 expand_vec_perm_var optabs.h optabs.cc
rtx mtcs_optabs_expand_vec_perm_var (MtcsOptabs *self,machine_mode mode, rtx v0, rtx v1, rtx sel, rtx target);
//原型 expand_ternary_op optabs.h optabs.cc
rtx mtcs_optabs_expand_ternary_op (MtcsOptabs *self,machine_mode mode, optab ternary_optab, rtx op0,
           rtx op1, rtx op2, rtx target, int unsignedp);
//原型 expand_vec_series_expr optabs.h optabs.cc
rtx mtcs_optabs_expand_vec_series_expr (MtcsOptabs *self,machine_mode vmode, rtx op0, rtx op1, rtx target);
//原型 expand_copysign optabs.h optabs.cc
rtx mtcs_optabs_expand_copysign (MtcsOptabs *self,rtx op0, rtx op1, rtx target);
//原型 expand_sfix_optab optabs.h optabs.cc
bool mtcs_optabs_expand_sfix_optab (MtcsOptabs *self,rtx to, rtx from, convert_optab tab);
//原型 expand_twoval_unop optabs.h optabs.cc
bool mtcs_optabs_expand_twoval_unop (MtcsOptabs *self,optab unoptab, rtx op0, rtx targ0, rtx targ1,int unsignedp);
//原型 expand_atomic_store optabs.h optabs.cc
rtx mtcs_optabs_expand_atomic_store (MtcsOptabs *self,rtx mem, rtx val, enum memmodel model, bool use_release);
//原型 expand_atomic_exchange optabs.h optabs.cc
rtx mtcs_optabs_expand_atomic_exchange (MtcsOptabs *self,rtx target, rtx mem, rtx val, enum memmodel model);
//原型 can_atomic_load_p optabs-query.h optabs-query.cc
bool mtcs_optabs_can_atomic_load_p (MtcsOptabs *self,machine_mode mode);
//原型 can_compare_and_swap_p optabs-query.h optabs-query.cc
bool mtcs_optabs_can_compare_and_swap_p (MtcsOptabs *self,machine_mode mode, bool allow_libcall);
//原型 expand_mem_thread_fence optabs.h optabs.cc
void mtcs_optabs_expand_mem_thread_fence (MtcsOptabs *self,enum memmodel model);
//原型 expand_atomic_compare_and_swap optabs.h optabs.cc
bool mtcs_optabs_expand_atomic_compare_and_swap (MtcsOptabs *self,rtx *ptarget_bool, rtx *ptarget_oval,
            rtx mem, rtx expected, rtx desired, bool is_weak, enum memmodel succ_model,enum memmodel fail_model);
//原型 expand_atomic_fetch_op optabs.h optabs.cc
rtx mtcs_optabs_expand_atomic_fetch_op (MtcsOptabs *self,rtx target, rtx mem, rtx val, enum rtx_code code,
         enum memmodel model, bool after);

//原型 expand_sync_lock_test_and_set optabs.h optabs.cc
rtx mtcs_optabs_expand_sync_lock_test_and_set (MtcsOptabs *self,rtx target, rtx mem, rtx val);
//原型 expand_mem_signal_fence optabs.h optabs.cc
void mtcs_optabs_expand_mem_signal_fence (MtcsOptabs *self,enum memmodel model);

//原型 expand_atomic_test_and_set optabs.h optabs.cc
rtx mtcs_optabs_expand_atomic_test_and_set (MtcsOptabs *self,rtx target, rtx mem, enum memmodel model);
//原型 expand_atomic_load optabs.h optabs.cc
rtx mtcs_optabs_expand_atomic_load (MtcsOptabs *self,rtx target, rtx mem, enum memmodel model);
//原型 emit_conditional_neg_or_complement optabs.h optabs.cc
rtx mtcs_optabs_emit_conditional_neg_or_complement (MtcsOptabs *self,rtx target, rtx_code code,
                 machine_mode mode, rtx cond, rtx op1,rtx op2);
//原型 emit_conditional_add optabs.h optabs.cc
rtx mtcs_optabs_emit_conditional_add (MtcsOptabs *self,rtx target, enum rtx_code code, rtx op0, rtx op1,
            machine_mode cmode, rtx op2, rtx op3, machine_mode mode, int unsignedp);
//原型 expand_one_cmpl_abs_nojump optabs.h optabs.cc
rtx mtcs_optabs_expand_one_cmpl_abs_nojump (MtcsOptabs *self,machine_mode mode, rtx op0, rtx target);
//原型 gen_cond_trap optabs.h optabs.cc
rtx_insn * mtcs_optabs_gen_cond_trap (MtcsOptabs *self,enum rtx_code code, rtx op1, rtx op2, rtx tcode);
//原型 gen_add3_insn optabs.h optabs.cc
rtx_insn * mtcs_optabs_gen_add3_insn (MtcsOptabs *self,rtx r0, rtx r1, rtx c);
//原型 expand_asm_reg_clobber_mem_blockage optabs.h optabs.cc
void mtcs_optabs_expand_asm_reg_clobber_mem_blockage (MtcsOptabs *self,HardRegSet *regs);
//原型 expand_twoval_binop optabs.h optabs.cc
bool mtcs_optabs_expand_twoval_binop (MtcsOptabs *self,optab binoptab, rtx op0, rtx op1, rtx targ0, rtx targ1,
           int unsignedp);
//原型 sign_expand_binop optabs.h optabs.cc
rtx mtcs_optabs_sign_expand_binop (MtcsOptabs *self,machine_mode mode, optab uoptab, optab soptab,
         rtx op0, rtx op1, rtx target, int unsignedp,int optab_methods/*!enum optab_methods*/);

//原型 create_integer_operand optabs.h optabs.cc
void mtcs_optabs_create_integer_operand (MtcsOptabs *self,class expand_operand *op, poly_int64 intval);

#endif
