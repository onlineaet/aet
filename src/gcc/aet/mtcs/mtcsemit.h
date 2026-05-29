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

#ifndef __GCC_MTCS_EMIT__
#define __GCC_MTCS_EMIT__

#include "../nlib.h"
#include "mtcscomponent.h"


typedef struct _MtcsEmit MtcsEmit;


struct _MtcsEmit
{
   MtcsComponent parent;
   location_t prologue_location;
   location_t epilogue_location;
   location_t curr_location;
   GTY ((deletable)) struct sequence_stack *free_sequence_stack;
   //原型 #define BRANCH_COST(speed_p, predictable_p) 1
   int (*brach_cost)(MtcsEmit *self,bool speed_p ,bool predictable_p);
   //原型 #define ADJUST_INSN_LENGTH(INSN, LENGTH)
   int (*adjust_insn_length)(MtcsEmit *self,rtx_insn *insn,int length);
};

void mtcs_emit_init(MtcsEmit *self);
//原型 emit_note emit-rtl.h emit-rtl.cc rtx_note  *mtcs_emit_emit_note (MtcsEmit *self,enum insn_note kind);//enum insn_note kind编译通不过，改成整形
rtx_note  *mtcs_emit_emit_note (MtcsEmit *self,int insn_note_kind);
//原型 start_sequence rtl.h emit-rtl.cc
void mtcs_emit_start_sequence (MtcsEmit *self);
//原型 end_sequence rtl.h emit-rtl.cc
void mtcs_emit_end_sequence (MtcsEmit *self);
//原型 gen_reg_rtx rtl.h emit-rtl.cc
rtx mtcs_emit_gen_reg_rtx (MtcsEmit *self,mtcs_mode mode);
//原型 emit_insn emit-rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_insn (MtcsEmit *self,rtx x);
//原型 emit rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit (MtcsEmit *self,rtx x, bool allow_barrier_p=true);
//原型 add_insn_before rtl.h emit-rtl.cc
void mtcs_emit_add_insn_before (MtcsEmit *self,rtx_insn *insn, rtx_insn *before, basic_block bb);
//原型 emit_label_before rtl.h emit-rtl.cc
rtx_code_label *mtcs_emit_emit_label_before (MtcsEmit *self,rtx_code_label *label, rtx_insn *before);
//原型 add_insn rtl.h emit-rtl.cc
void mtcs_emit_add_insn (MtcsEmit *self,rtx_insn *insn);
//原型 emit_label rtl.h emit-rtl.cc
rtx_code_label *mtcs_emit_emit_label (MtcsEmit *self,rtx uncast_label);
//原型 emit_jump_insn rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_jump_insn (MtcsEmit *self,rtx x);
//原型 emit_barrier_before rtl.h emit-rtl.cc
rtx_barrier * mtcs_emit_emit_barrier_before (MtcsEmit *self,rtx_insn *before);
//原型 emit_barrier rtl.h emit-rtl.cc
rtx_barrier *mtcs_emit_emit_barrier (MtcsEmit *self);
//原型 emit_call_insn rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_call_insn (MtcsEmit *self,rtx x);
//原型 emit_debug_insn rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_debug_insn (MtcsEmit *self,rtx x);
//原型 emit_clobber rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_clobber (MtcsEmit *self,rtx x);
//原型 emit_jump rtl.h stmt.cc
void mtcs_emit_emit_jump (MtcsEmit *self,rtx label);
//原型 emit_barrier_after rtl.h emit-rtl.cc
rtx_barrier *mtcs_emit_emit_barrier_after (MtcsEmit *self,rtx_insn *after);
//原型 add_insn_after rtl.h emit-rtl.cc
void mtcs_emit_add_insn_after (MtcsEmit *self,rtx_insn *insn, rtx_insn *after, basic_block bb);
//原型 push_to_sequence rtl.h emit-rtl.cc
void mtcs_emit_push_to_sequence (MtcsEmit *self,rtx_insn *first);
//原型 push_to_sequence2 rtl.h emit-rtl.cc
void mtcs_emit_push_to_sequence2 (MtcsEmit *self,rtx_insn *first, rtx_insn *last);
//原型 push_topmost_sequence rtl.h emit-rtl.cc
void mtcs_emit_push_topmost_sequence (MtcsEmit *self);
//原型 get_topmost_sequence emit-rtl.h
struct sequence_stack *mtcs_emit_get_topmost_sequence (MtcsEmit *self);
//原型 pop_topmost_sequence rtl.h emit-rtl.cc
void mtcs_emit_pop_topmost_sequence (MtcsEmit *self);
//原型 emit_jump_table_data rtl.h emit-rtl.cc
rtx_jump_table_data * mtcs_emit_emit_jump_table_data (MtcsEmit *self,rtx table);
//原型 remove_insn emit-rtl.h emit-rtl.cc
void mtcs_emit_remove_insn (MtcsEmit *self,rtx_insn *insn);
//原型 unshare_all_rtl rtl.h emit-rtl.cc
void mtcs_emit_unshare_all_rtl (MtcsEmit *self);
//原型 emit_use rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_use (MtcsEmit *self,rtx x);
//原型 gen_use rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_gen_use (MtcsEmit *self,rtx x);
//原型 emit_insn_after rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_insn_after (MtcsEmit *self,rtx pattern, rtx_insn *after);
//原型 emit_insn_before rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_insn_before (MtcsEmit *self,rtx pattern, rtx_insn *before);
//原型 emit_note_after rtl.h emit-rtl.cc
rtx_note * mtcs_emit_emit_note_after (MtcsEmit *self,enum insn_note subtype, rtx_insn *after);
//原型 emit_note_before rtl.h emit-rtl.cc
rtx_note * mtcs_emit_emit_note_before (MtcsEmit *self,enum insn_note subtype, rtx_insn *before);
//原型 verify_rtl_sharing rtl.h emit-rtl.cc
void mtcs_emit_verify_rtl_sharing (MtcsEmit *self);
//原型 emit_label_after rtl.h emit-rtl.cc
rtx_insn * mtcs_emit_emit_label_after (MtcsEmit *self,rtx_insn *label, rtx_insn *after);
//原型 try_split rtl.h emit-rtl.cc
rtx_insn * mtcs_emit_try_split (MtcsEmit *self,rtx pat, rtx_insn *trial, int last);
//原型 emit_insn_after_noloc rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_insn_after_noloc (MtcsEmit *self,rtx x, rtx_insn *after, basic_block bb);
//原型 emit_insn_after_setloc rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_insn_after_setloc (MtcsEmit *self,rtx pattern, rtx_insn *after, location_t loc);
//原型 copy_delay_slot_insn emit-rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_copy_delay_slot_insn (MtcsEmit *self,rtx_insn *insn);
//原型 emit_jump_insn_after emit-rtl.h emit-rtl.cc
rtx_jump_insn *mtcs_emit_emit_jump_insn_after (MtcsEmit *self,rtx pattern, rtx_insn *after);
//原型 emit_debug_insn_after emit-rtl.h emit-rtl.cc
rtx_insn * mtcs_emit_emit_debug_insn_after (MtcsEmit *self,rtx pattern, rtx_insn *after);
//原型 emit_debug_insn_after emit-rtl.h emit-rtl.cc
rtx_insn * mtcs_emit_emit_call_insn_after_noloc (MtcsEmit *self,rtx x, rtx_insn *after);
//原型 emit_call_insn_after emit-rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_call_insn_after (MtcsEmit *self,rtx pattern, rtx_insn *after);
//原型 emit_insn_before_noloc rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_insn_before_noloc (MtcsEmit *self,rtx x, rtx_insn *before, basic_block bb);
//原型 emit_insn_before_setloc rtl.h emit-rtl.cc
rtx_insn * mtcs_emit_emit_insn_before_setloc (MtcsEmit *self,rtx pattern, rtx_insn *before, location_t loc);
//原型 make_insn_raw rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_make_insn_raw (MtcsEmit *self,rtx pattern);
//原型 emit_copy_of_insn_after emit-rtl.h emit-rtl.cc
rtx_insn * mtcs_emit_emit_copy_of_insn_after (MtcsEmit *self,rtx_insn *insn, rtx_insn *after);
//原型 emit_jump_insn_after_setloc rtl.h emit-rtl.cc
rtx_jump_insn * mtcs_emit_emit_jump_insn_after_setloc (MtcsEmit *self,rtx pattern, rtx_insn *after, location_t loc);
//原型 emit_jump_insn_after_noloc rtl.h emit-rtl.cc
rtx_jump_insn * mtcs_emit_emit_jump_insn_after_noloc (MtcsEmit *self,rtx x, rtx_insn *after);
//原型 emit_jump_insn_before rtl.h emit-rtl.cc
rtx_jump_insn *mtcs_emit_emit_jump_insn_before (MtcsEmit *self,rtx pattern, rtx_insn *before);
//原型 insn_locations_init rtl.h emit-rtl.cc
void mtcs_emit_insn_locations_init (MtcsEmit *self);
//原型 set_curr_insn_location rtl.h emit-rtl.cc
void mtcs_emit_set_curr_insn_location (MtcsEmit *self,location_t location);
//原型 curr_insn_location rtl.h emit-rtl.cc
location_t mtcs_emit_curr_insn_location (MtcsEmit *self);
//原型 insn_locations_finalize rtl.h emit-rtl.cc
void mtcs_emit_insn_locations_finalize (MtcsEmit *self);
//原型 #define BRANCH_COST(speed_p, predictable_p) 1 分支消耗的指令数
int mtcs_emit_branch_cost(MtcsEmit *self,bool speed_p ,bool predictable_p);
void mtcs_emit_set_prologue_location(MtcsEmit *self,location_t location);

#endif
