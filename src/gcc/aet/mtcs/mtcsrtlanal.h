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

#ifndef __GCC_MTCS_RTLANAL__
#define __GCC_MTCS_RTLANAL__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcsmicro.h"


typedef struct _MtcsRtlanal MtcsRtlanal;
struct _MtcsRtlanal
{
    MtcsComponent parent;
    unsigned int  num_sign_bit_copies_in_rep[MAX_MAX_MODE_INT/*!MAX_MODE_INT*/ + 1][MAX_MAX_MODE_INT/*!MAX_MODE_INT*/ + 1];
};

/* The address space that the memory reference uses.  */
MtcsRtlanal *mtcs_rtlanal_new(MtcsMode *mtcsMode);
//原型 vec_series_lowpart_p rtlanal.h rtlanal.cc
bool mtcs_rtlanal_vec_series_lowpart_p (MtcsRtlanal *self,machine_mode result_mode, machine_mode op_mode, rtx sel);
//原型 vec_series_highpart_p rtlanal.h rtlanal.cc
bool mtcs_rtlanal_vec_series_highpart_p(MtcsRtlanal *self,machine_mode result_mode, machine_mode op_mode, rtx sel);
//原型 commutative_operand_precedence rtl.h rtlanal.cc
int mtcs_rtlanal_commutative_operand_precedence (MtcsRtlanal *self,rtx op);
//原型 insn_cost rtl.h rtlanal.cc
int mtcs_rtlanal_insn_cost (MtcsRtlanal *self,rtx_insn *insn, bool speed);
//原型 swap_commutative_operands_p rtl.h rtlanal.cc
bool mtcs_rtlanal_swap_commutative_operands_p (MtcsRtlanal *self,rtx x, rtx y);
//原型 add_args_size_note rtl.h rtlanal.cc
void mtcs_rtlanal_add_args_size_note (MtcsRtlanal *self,rtx_insn *insn, poly_int64 value);
//原型 rtx_cost rtl.h rtlanal.cc
int mtcs_rtlanal_rtx_cost (MtcsRtlanal *self,rtx x, machine_mode mode, enum rtx_code outer_code, int opno, bool speed);
//原型 set_src_cost rtl.h
int mtcs_rtlanal_set_src_cost ( MtcsRtlanal *self,rtx x, machine_mode mode, bool speed_p);
//原型 set_rtx_cost rtl.h
int mtcs_rtlanal_set_rtx_cost (MtcsRtlanal *self,rtx x, bool speed_p);
//原型 seq_cost rtl.h rtlanal.cc
unsigned mtcs_rtlanal_seq_cost (MtcsRtlanal *self,const rtx_insn *seq, bool speed);
//原型 split_double rtl.h rtlanal.cc
void mtcs_rtlanal_split_double (MtcsRtlanal *self,rtx value, rtx *first, rtx *second);
//原型 init_rtlanal rtl.h rtlanal.cc
void mtcs_rtlanal_init_rtlanal (MtcsRtlanal *self);
//原型 note_pattern_stores rtl.h rtlanal.cc
void mtcs_rtlanal_note_pattern_stores (MtcsRtlanal *self,const_rtx x, void (*fun)(rtx, const_rtx, void *), void *data);
//原型 note_stores rtl.h rtlanal.cc
void mtcs_rtlanal_note_stores (MtcsRtlanal *self,const rtx_insn *insn, void (*fun) (rtx, const_rtx, void *), void *data);
//原型 find_all_hard_reg_sets rtl.h rtlanal.c
void mtcs_rtlanal_find_all_hard_reg_sets (MtcsRtlanal *self,const rtx_insn *insn, HardRegSet *pset, bool implicit);
//原型 reg_overlap_mentioned_p rtl.h rtlanal.cc
bool mtcs_rtlanal_reg_overlap_mentioned_p (MtcsRtlanal *self,const_rtx x, const_rtx in);
//原型 refers_to_regno_p rtl.h rtlanal.cc
bool mtcs_rtlanal_refers_to_regno_p (MtcsRtlanal *self,unsigned int regno, unsigned int endregno, const_rtx x,
           rtx *loc);
//原型 set_noop_p rtl.h rtlanal.cc
bool mtcs_rtlanal_set_noop_p (MtcsRtlanal *self,const_rtx set);
//原型 subreg_regno_offset rtl.h rtlanal.cc
unsigned int mtcs_rtlanal_subreg_regno_offset (MtcsRtlanal *self,unsigned int xregno, machine_mode xmode,
           poly_uint64 offset, machine_mode ymode);
//原型 subreg_nregs rtl.h rtlanal.cc
unsigned int mtcs_rtlanal_subreg_nregs (MtcsRtlanal *self,const_rtx x);
//原型 subreg_nregs_with_regno rtl.h rtlanal.cc
unsigned int mtcs_rtlanal_subreg_nregs_with_regno (MtcsRtlanal *self,unsigned int regno, const_rtx x);
//原型 subreg_regno rtl.h rtlanal.cc
unsigned int mtcs_rtlanal_subreg_regno (MtcsRtlanal *self,const_rtx x);
//原型 may_trap_or_fault_p rtl.h rtlanal.cc
bool mtcs_rtlanal_may_trap_or_fault_p (MtcsRtlanal *self,const_rtx x);
//原型 may_trap_p rtl.h rtlanal.cc
bool mtcs_rtlanal_may_trap_p (MtcsRtlanal *self,const_rtx x);
//原型 may_trap_p_1 rtl.h rtlanal.cc
bool  mtcs_rtlanal_may_trap_p_1 (MtcsRtlanal *self,const_rtx x, unsigned flags);
//原型 set_of rtl.h rtlanal.cc
const_rtx mtcs_rtlanal_set_of (MtcsRtlanal *self,const_rtx pat, const_rtx insn);
//原型 reg_set_p rtl.h rtlanal.cc
bool mtcs_rtlanal_reg_set_p (MtcsRtlanal *self,const_rtx reg, const_rtx insn);
//原型 find_regno_fusage rtl.h rtlanal.cc
bool mtcs_rtlanal_find_regno_fusage (MtcsRtlanal *self,const_rtx insn, enum rtx_code code, unsigned int regno);
//原型 find_reg_fusage rtl.h rtlanal.cc
bool mtcs_rtlanal_find_reg_fusage (MtcsRtlanal *self,const_rtx insn, enum rtx_code code, const_rtx datum);
//原型 read_modify_subreg_p rtl.h rtlanal.cc
bool mtcs_rtlanal_read_modify_subreg_p (MtcsRtlanal *self,const_rtx x);
//原型 reg_referenced_p rtl.h rtlanal.cc
bool mtcs_rtlanal_reg_referenced_p (MtcsRtlanal *self,const_rtx x, const_rtx body);
//原型 modified_in_p rtl.h rtlanal.cc
bool mtcs_rtlanal_modified_in_p (MtcsRtlanal *self,const_rtx x, const_rtx insn);
//原型 address_cost rtl.h rtlanal.cc
int mtcs_rtlanal_address_cost (MtcsRtlanal *self,rtx x, machine_mode mode, addr_space_t as, bool speed);
//原型 contains_paradoxical_subreg_p rtlanal.h rtlanal.cc
bool mtcs_rtlanal_contains_paradoxical_subreg_p (MtcsRtlanal *self,rtx x);
//原型 simple_regno_set rtl.h rtlanal.cc
rtx mtcs_rtlanal_simple_regno_set (MtcsRtlanal *self,rtx pat, unsigned int regno);
//原型 rtx_varies_p rtl.h rtlanal.cc
bool mtcs_rtlanal_rtx_varies_p (MtcsRtlanal *self,const_rtx x, bool for_alias);
//原型 remove_note rtl.h rtlanal.cc
void mtcs_rtlanal_remove_note (MtcsRtlanal *self,rtx_insn *insn, const_rtx note);
//原型 remove_reg_equal_equiv_notes rtl.h rtlanal.cc
bool mtcs_rtlanal_remove_reg_equal_equiv_notes (MtcsRtlanal *self,rtx_insn *insn, bool no_rescan = false);
//原型 remove_reg_equal_equiv_notes_for_regno rtl.h rtlanal.cc
void mtcs_rtlanal_remove_reg_equal_equiv_notes_for_regno (MtcsRtlanal *self,unsigned int regno);
//原型 canonicalize_condition rtl.h rtlanal.cc
rtx mtcs_rtlanal_canonicalize_condition (MtcsRtlanal *self,rtx_insn *insn, rtx cond, int reverse,
         rtx_insn **earliest,rtx want_reg, int allow_cc_mode, int valid_at_insn_p);
//原型 get_condition rtl.h rtlanal.cc
rtx mtcs_rtlanal_get_condition ( MtcsRtlanal *self,rtx_insn *jump, rtx_insn **earliest, int allow_cc_mode,
          int valid_at_insn_p);
//原型 pattern_cost rtl.h rtlanal.cc
int mtcs_rtlanal_pattern_cost (MtcsRtlanal *self,rtx pat, bool speed);
//原型 noop_move_p rtl.h rtlanal.cc
bool mtcs_rtlanal_noop_move_p (MtcsRtlanal *self,const rtx_insn *insn);
//原型 low_bitmask_len rtl.h rtlanal.cc
int mtcs_rtlanal_low_bitmask_len (MtcsRtlanal *self,machine_mode mode, unsigned HOST_WIDE_INT m);
//原型 num_sign_bit_copies rtl.h rtlanal.cc
unsigned int mtcs_rtlanal_num_sign_bit_copies (MtcsRtlanal *self,const_rtx x, machine_mode mode);
//原型 remove_death rtl.h combine.cc
rtx mtcs_rtlanal_remove_death (MtcsRtlanal *self,unsigned int regno, rtx_insn *insn);
//原型 get_full_rtx_cost rtl.h rtlanal.cc
void mtcs_rtlanal_get_full_rtx_cost (MtcsRtlanal *self,rtx x, machine_mode mode, enum rtx_code outer, int opno,
         struct full_rtx_costs *c);
//原型 get_full_set_rtx_cost rtl.h
void mtcs_rtlanal_get_full_set_rtx_cost (MtcsRtlanal *self,rtx x, struct full_rtx_costs *c);
//原型 get_full_set_src_cost rtl.h
void mtcs_rtlanal_get_full_set_src_cost (MtcsRtlanal *self,rtx x, machine_mode mode, struct full_rtx_costs *c);
//原型 replace_rtx rtl.h rtlanal.cc
rtx mtcs_rtlanal_replace_rtx (MtcsRtlanal *self,rtx x, rtx from, rtx to, bool all_regs=false);
//原型 replace_label rtl.h rtlanal.cc
void mtcs_rtlanal_replace_label (MtcsRtlanal *self,rtx *loc, rtx old_label, rtx new_label, bool update_label_nuses);
//原型 replace_label_in_insn rtl.h rtlanal.cc
void mtcs_rtlanal_replace_label_in_insn (MtcsRtlanal *self,rtx_insn *insn, rtx_insn *old_label,
             rtx_insn *new_label, bool update_label_nuses);
//原型 subreg_lsb rtl.h rtlanal.cc
poly_uint64 mtcs_rtlanal_subreg_lsb (MtcsRtlanal *self,const_rtx x);
//原型 subreg_lsb_1 rtl.h
poly_uint64 mtcs_rtlanal_subreg_lsb_1 (MtcsRtlanal *self,machine_mode outer_mode, machine_mode inner_mode,
         poly_uint64 subreg_byte);
//原型 nonzero_bits rtl.h rtlanal.cc
unsigned HOST_WIDE_INT mtcs_rtlanal_nonzero_bits (MtcsRtlanal *self,const_rtx x, machine_mode mode);
//原型 dead_or_set_p rtl.h rtlanal.cc
bool mtcs_rtlanal_dead_or_set_p (MtcsRtlanal *self,const rtx_insn *insn, const_rtx x);
//原型 dead_or_set_regno_p rtl.h rtlanal.cc
bool mtcs_rtlanal_dead_or_set_regno_p (MtcsRtlanal *self,const rtx_insn *insn, unsigned int test_regno);
//原型 for_each_inc_dec rtl.h rtlanal.cc
int mtcs_rtlanal_for_each_inc_dec (MtcsRtlanal *self,rtx x,for_each_inc_dec_fn fn, void *data);
//原型 modified_between_p rtl.h rtlanal.cc
bool mtcs_rtlanal_modified_between_p (MtcsRtlanal *self,const_rtx x, const rtx_insn *start, const rtx_insn *end);
//原型 reg_set_between_p rtl.h rtlanal.cc
bool mtcs_rtlanal_reg_set_between_p (MtcsRtlanal *self,const_rtx reg, const rtx_insn *from_insn,
         const rtx_insn *to_insn);

//原型 nonzero_address_p rtl.h rtlanal.cc
bool mtcs_rtlanal_nonzero_address_p (MtcsRtlanal *self,const_rtx x);

#endif

