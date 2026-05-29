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

#ifndef __GCC_MTCS_DOJUMP__
#define __GCC_MTCS_DOJUMP__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcspass.h"
#include "dojump.h"



typedef struct _MtcsDojump MtcsDojump;


struct _MtcsDojump
{
    MtcsComponent parent;
   //原型 REVERSE_CONDITION
   enum rtx_code (*reverse_condition)(MtcsDojump *self,enum rtx_code code, machine_mode mode);

    GTY(()) rtx and_reg; //原型 dojump.cc
    GTY(()) rtx and_test;//原型 dojump.cc
    GTY(()) rtx shift_test;//原型 dojump.cc
};

MtcsDojump *mtcs_dojump_new(MtcsMode *mtcsMode);

//原型 do_compare_rtx_and_jump dojump.h dojump.cc
void mtcs_dojump_do_compare_rtx_and_jump (MtcsDojump *self,rtx op0, rtx op1, enum rtx_code code, int unsignedp,
             machine_mode mode, rtx size,
             rtx_code_label *if_false_label,
             rtx_code_label *if_true_label,
             profile_probability prob);

//原型 do_compare_rtx_and_jump dojump.h dojump.cc 重载函数
void mtcs_dojump_do_compare_rtx_and_jump (MtcsDojump *self,rtx op0, rtx op1, enum rtx_code code, int unsignedp,
             tree val, machine_mode mode, rtx size,
             rtx_code_label *if_false_label,
             rtx_code_label *if_true_label,
             profile_probability prob);

//原型 split_comparison dojump.h dojump.cc
bool mtcs_dojump_split_comparison (MtcsDojump *self,enum rtx_code code, machine_mode mode,enum rtx_code *code1, enum rtx_code *code2);
//原型 do_pending_stack_adjust dojump.h dojump.cc
void mtcs_dojump_do_pending_stack_adjust (MtcsDojump *self);

//原型 reversed_comparison_code_parts rtl.h jump.cc
enum rtx_code mtcs_dojump_reversed_comparison_code_parts (MtcsDojump *self,enum rtx_code code, const_rtx arg0,
                const_rtx arg1, const rtx_insn *insn);
//原型 reversed_comparison_code rtl.h jump.cc
enum rtx_code mtcs_dojump_reversed_comparison_code (MtcsDojump *self,const_rtx comparison, const rtx_insn *insn);
//原型 REVERSE_CONDITION
enum rtx_code mtcs_dojump_reverse_condition(MtcsDojump *self,enum rtx_code code, machine_mode mode);
//原型 discard_pending_stack_adjust dojump.h dojump.cc
void mtcs_dojump_discard_pending_stack_adjust (MtcsDojump *self);

//原型 restore_pending_stack_adjust dojump.j dojump.cc
void mtcs_dojump_restore_pending_stack_adjust (MtcsDojump *self,saved_pending_stack_adjust *save);
//原型 jumpif dojump.h dojump.cc
void mtcs_dojump_jumpif (MtcsDojump *self,tree exp, rtx_code_label *label, profile_probability prob);
//原型 jumpifnot dojump.h dojump.cc
void mtcs_dojump_jumpifnot (MtcsDojump *self,tree exp, rtx_code_label *label, profile_probability prob);
//原型 clear_pending_stack_adjust dojump.h dojump.cc
void mtcs_dojump_clear_pending_stack_adjust (MtcsDojump *self);
//原型 jumpifnot_1 dojump.h dojump.cc
void mtcs_dojump_jumpifnot_1 (MtcsDojump *self,enum tree_code code, tree op0, tree op1, rtx_code_label *label,
         profile_probability prob);
//原型 jumpif_1 dojump.h dojump.cc
void mtcs_dojump_jumpif_1 (MtcsDojump *self,enum tree_code code, tree op0, tree op1, rtx_code_label *label,
      profile_probability prob);
//原型 delete_related_insns rtl.h jump.cc
rtx_insn * mtcs_dojump_delete_related_insns (MtcsDojump *self,rtx uncast_insn);
//原型 redirect_jump rtl.h jump.cc
bool mtcs_dojump_redirect_jump (MtcsDojump *self,rtx_jump_insn *jump, rtx nlabel, int delete_unused);
//原型 redirect_jump_1 rtl.h jump.cc
bool mtcs_dojump_redirect_jump_1 (MtcsDojump *self,rtx_insn *jump, rtx nlabel);
//原型 redirect_jump_2 rtl.h jump.cc
void mtcs_dojump_redirect_jump_2 (MtcsDojump *self,rtx_jump_insn *jump, rtx olabel,
      rtx nlabel, int delete_unused,int invert);
//原型 invert_jump rtl.h jump.cc
bool mtcs_dojump_invert_jump (MtcsDojump *self,rtx_jump_insn *jump, rtx nlabel, int delete_unused);
//原型 invert_jump_1 rtl.h jump.cc
bool mtcs_dojump_invert_jump_1 (MtcsDojump *self,rtx_jump_insn *jump, rtx nlabel);
//原型 mark_jump_label rtl.h jump.cc
void mtcs_dojump_mark_jump_label (MtcsDojump *self,rtx x, rtx_insn *insn, int in_mem);
//原型 rebuild_jump_labels rtl.h jump.cc
void mtcs_dojump_rebuild_jump_labels (MtcsDojump *self,rtx_insn *f);
//原型 true_regnum rtl.h jump.cc
int mtcs_dojump_true_regnum (MtcsDojump *self,const_rtx x);
//原型 rebuild_jump_labels_chain rtl.h jump.cc
void mtcs_dojump_rebuild_jump_labels_chain (MtcsDojump *self,rtx_insn *chain);

//原型 NEXT_PASS (pass_cleanup_barriers, 1);  RTL_PASS  jump.cc  barriers   y 无条件执行 cleanup_barriers
typedef struct _MtcsPassCleanupBarriers MtcsPassCleanupBarriers;
struct _MtcsPassCleanupBarriers
{
   MtcsPass parent;
};
MtcsPassCleanupBarriers *mtcs_pass_cleanup_barriers_new (MtcsMode *mtcsMode);

#endif
