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

#include "config.h"
#define INCLUDE_ALGORITHM /* reverse */
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "tree-pass.h"
#include "cfghooks.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "tree-pretty-print.h" /* for dump_function_header */
#include "varasm.h"
#include "insn-attr.h"
#include "conditions.h"
#include "flags.h"
#include "output.h"
#include "except.h"
#include "rtl-error.h"
#include "toplev.h" /* exact_log2, floor_log2 */
#include "reload.h"
#include "intl.h"
#include "cfgrtl.h"
#include "debug.h"
#include "tree-pass.h"
#include "tree-ssa.h"
#include "cfgloop.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "rtl-iter.h"
#include "print-rtl.h"
#include "function-abi.h"
#include "common/common-target.h"
#include "diagnostic.h"
#include "context.h"
#include "options.h"
#include "expmed.h"
#include "expr.h"
#include "fold-const.h"
#include "dwarf2out.h"
#include "common/common-targhooks.h"

#include "aet/aetprinttree.h"
#include "mtcsrtldata.h"
#include "mtcscompile.h"
#include "mtcstarget.h"


static void mtcsRtlDataInit(MtcsRtlData *self)
{
   self->asm_clobbers.count =1;//mtcs_reg_get_hard_reg_element_count(mtcsReg);
   self->must_be_zero_on_return.count=1;//mtcs_reg_get_hard_reg_element_count(mtcsReg);
}

nuint  mtcs_rtl_data_get_stack_alignment_needed(MtcsRtlData *self)
{
   return self->stack_alignment_needed;
}

rtx_insn *mtcs_rtl_data_get_last_insn (MtcsRtlData *self)
{
    return self->emit.seq.last;
}

rtx_insn *mtcs_rtl_data_get_insns (MtcsRtlData *self)
{
    return self->emit.seq.first;
}

/* Specify a new insn as the first in the chain.  */
//原型 set_first_insn emit-rtl.h 内联函数
void mtcs_rtl_data_set_first_insn (MtcsRtlData *self,rtx_insn *insn)
{
  gcc_checking_assert (!insn || !PREV_INSN (insn));
  self->emit.seq.first = insn;
}

/* Specify a new insn as the last in the chain.  */
//原型 set_last_insn emit-rtl.h 内联函数
void mtcs_rtl_data_set_last_insn (MtcsRtlData *self,rtx_insn *insn)
{
  gcc_checking_assert (!insn || !NEXT_INSN (insn));
  self->emit.seq.last = insn;
}

//原型 get_current_sequence emit-rtl.h 内联函数
struct sequence_stack *mtcs_rtl_data_get_current_sequence(MtcsRtlData *self)
{
    return &self->emit.seq;
}

/* Initialize fields of rtl_data related to stack alignment.  */
//原型 struct GTY(()) rtl_data {void init_stack_alignment ();... emit-rtl.h emit-rtl.cc
void mtcs_rtl_data_init_stack_alignment (MtcsRtlData *self)
{
  self->stack_alignment_needed = self->stackBoundary/*!STACK_BOUNDARY*/;
  self->max_used_stack_slot_alignment = self->stackBoundary/*!STACK_BOUNDARY*/;;
  self->stack_alignment_estimated = 0;
  self->preferred_stack_boundary = self->stackBoundary/*!STACK_BOUNDARY*/;;
}

//原型 STACK_BOUNDARY defaults.h 由mtcsfunc调用
void mtcs_rtl_data_set_stack_boundary(MtcsRtlData *self,int value)
{
    self->stackBoundary=value;
}


void mtcs_rtl_data_ensure_regno_capacity (MtcsRtlData *self)
{
  int old_size = self->emit.regno_pointer_align_length;
  if (self->emit.x_reg_rtx_no < old_size)
    return;

  int new_size = old_size * 2;
  while (self->emit.x_reg_rtx_no >= new_size)
    new_size *= 2;

  char *tmp = XRESIZEVEC (char, self->emit.regno_pointer_align, new_size);
  memset (tmp + old_size, 0, new_size - old_size);
  self->emit.regno_pointer_align = (unsigned char *) tmp;

  rtx *new1 = GGC_RESIZEVEC (rtx, self->regno_reg_rtx, new_size);
  memset (new1 + old_size, 0, (new_size - old_size) * sizeof (rtx));
  self->regno_reg_rtx = new1;

  self->emit.regno_pointer_align_length = new_size;
}

/* Delete all insns made since FROM.
   FROM becomes the new last instruction.  */
//原型 delete_insns_since rtl.h emit-rtl.cc
void mtcs_rtl_data_delete_insns_since (MtcsRtlData *self,rtx_insn *from)
{
  if (from == 0)
      mtcs_rtl_data_set_first_insn (self,0);
  else
    SET_NEXT_INSN (from) = 0;
  mtcs_rtl_data_set_last_insn (self,from);
}

/* Return the last CALL_INSN in the current list, or 0 if there is none.
   This routine does not look inside SEQUENCEs.  */
//原型 last_call_insn rtl.h emit-rtl.cc
rtx_call_insn *mtcs_rtl_data_last_call_insn (MtcsRtlData *self)
{
  rtx_insn *insn;
  for (insn = mtcs_rtl_data_get_last_insn (self); insn && !CALL_P (insn);insn = PREV_INSN (insn))
    ;
  return safe_as_a <rtx_call_insn *> (insn);
}

//原型 REGNO_POINTER_ALIGN #define REGNO_POINTER_ALIGN(REGNO) (crtl->emit.regno_pointer_align[REGNO]) function.h
unsigned mtcs_rtl_data_get_regno_pointer_align(MtcsRtlData *self,int regnum)
{
  return self->emit.regno_pointer_align[regnum];
}

//原型 REGNO_POINTER_ALIGN #define REGNO_POINTER_ALIGN(REGNO) (crtl->emit.regno_pointer_align[REGNO]) function.h
void    mtcs_rtl_data_set_regno_pointer_align(MtcsRtlData *self,int regnum,int align)
{
   n_debug("mtcsrtldata.c mtcs_rtl_data_set_regno_pointer_align 00  self->emit.regno_pointer_align:%p %d %d\n",
         self->emit.regno_pointer_align,regnum,align);
    self->emit.regno_pointer_align[regnum]=align;
}

//原型 PSEUDO_REGNO_MODE regs.h
machine_mode mtcs_rtl_data_get_pseudo_regno_mode(MtcsRtlData *self,int i)
{
   gcc_assert(i<self->emit.x_reg_rtx_no);
   return GET_MODE(self->regno_reg_rtx[i]);
}

//原型 int get_first_label_num (void) rtl.h emit-rtl.cc 在emit-rtl.cc定义宏#define first_label_num (crtl->emit.x_first_label_num)
//访问emit.x_first_label_num;
int mtcs_rtl_data_get_first_label_num (MtcsRtlData *self)
{
    return self->emit.x_first_label_num;
}

/* If the rtx for label was created during the expansion of a nested
   function, then first_label_num won't include this label number.
   Fix this now so that array indices work later.  */
//原型 maybe_set_first_label_num rtl.h emit-rtl.cc
void mtcs_rtl_data_maybe_set_first_label_num (MtcsRtlData *self,rtx_code_label *x)
{
  if (CODE_LABEL_NUMBER (x) < self->emit.x_first_label_num)
     self->emit.x_first_label_num = CODE_LABEL_NUMBER (x);
}

/* Return true if currently emitting into a sequence.  */
//原型 in_sequence_p rtl.h emit-rtl.cc
bool mtcs_rtl_data_in_sequence_p (MtcsRtlData *self)
{
  return mtcs_rtl_data_get_current_sequence/*!get_current_sequence*/(self)->next != 0;
}

//原型 free_after_compilation 中的  memset (crtl, 0, sizeof (struct rtl_data));
void mtcs_rtl_data_reset(MtcsRtlData *self)
{
    memset(&self->expr,0,sizeof( struct expr_status));
    memset(&self->emit,0,sizeof( struct emit_status));
    memset(&self->varasm,0,sizeof( struct varasm_status));
    memset(&self->args,0,sizeof( struct incoming_args));
    //memset(&self->subsections,0,sizeof( struct function_subsections));
    memset(&self->eh,0,sizeof( struct rtl_eh));
    self->abi=NULL;
    //self->ssa=NULL;
    self->outgoing_args_size=0;
    self->return_rtx=0;
    self->hard_reg_initial_vals=NULL;
    self->stack_protect_guard=NULL_TREE;
    self->stack_protect_guard_decl=NULL_TREE;
    //self->x_nonlocal_goto_handler_labels=0;
    self->x_return_label=NULL;
    self->x_naked_return_label=NULL;
    //self->x_stack_slot_list=NULL;
    self->frame_space_list=NULL;
    //self->x_stack_check_probe_note=NULL;
    self->x_arg_pointer_save_area=NULL;
    self->drap_reg=NULL;
    self->x_frame_offset=0;
    //self->x_function_beg_insn=NULL;
    //self->x_parm_birth_insn=NULL;
    self->x_used_temp_slots=NULL;
    self->x_avail_temp_slots=NULL;
    self->x_temp_slot_level=0;
    self->stack_alignment_needed=0;
    self->preferred_stack_boundary=0;
    self->parm_stack_boundary=0;
    self->max_used_stack_slot_alignment=0;
    self->stack_alignment_estimated=0;
    self->patch_area_size=0;
    self->patch_area_entry=0;
    self->accesses_prior_frames=false;
    self->calls_eh_return=false;
    self->saves_all_registers=false;
    self->has_nonlocal_goto=false;
    self->has_asm_statement=false;
    //self->all_throwers_are_sibcalls=false;
    self->limit_stack=false;
    self->profile=false;
    self->uses_const_pool=false;
    //self->uses_pic_offset_table=false;
    self->uses_eh_lsda=false;
    self->tail_call_emit=false;
    self->arg_pointer_save_area_init=false;
    self->x_frame_pointer_needed=false;
    self->maybe_hot_insn_p=false;
    self->stack_realign_needed=false;
    self->stack_realign_tried=false;
    self->need_drap=false;
    self->stack_realign_processed=false;
    self->stack_realign_finalized=false;
    self->dbr_scheduled_p=false;
    self->nothrow=false;
    //self->shrink_wrapped=false;
    //self->shrink_wrapped_separate=false;
    //self->sp_is_unchanging=false;
    self->sp_is_clobbered_by_asm=false;
    self->is_leaf=false;
    //self->uses_only_leaf_regs=false;
    self->has_bb_partition=false;
    self->bb_reorder_complete=false;
    memset(&self->asm_clobbers,0,sizeof( HardRegSet));
    //memset(&self->must_be_zero_on_return,0,sizeof( HardRegSet));
    //self->max_insn_address=0;
    self->asm_clobbers.count =1;//mtcs_reg_get_hard_reg_element_count(mtcsReg);
      self->must_be_zero_on_return.count=1;//mtcs_reg_get_hard_reg_element_count(mtcsReg);
}

MtcsRtlData *mtcs_rtl_data_new()
{
     MtcsRtlData *self = n_slice_alloc0 (sizeof(MtcsRtlData));
     mtcsRtlDataInit(self);
     return self;
}
