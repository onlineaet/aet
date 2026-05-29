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
 * base on emit-rtl.cc
 */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "cfghooks.h"
#include "tree-pass.h"
#include "memmodel.h"
#include "tm_p.h"
#include "ssa.h"
#include "optabs.h"
#include "regs.h" /* For reg_renumber.  */
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "fold-const.h"
#include "varasm.h"
#include "stor-layout.h"
#include "stmt.h"
#include "print-tree.h"
#include "cfgrtl.h"
#include "cfganal.h"
#include "cfgbuild.h"
#include "cfgcleanup.h"
#include "dojump.h"
#include "explow.h"
#include "calls.h"
#include "expr.h"
#include "internal-fn.h"
#include "tree-eh.h"
#include "gimple-iterator.h"
#include "gimple-expr.h"
#include "gimple-walk.h"
#include "tree-cfg.h"
#include "tree-dfa.h"
#include "tree-ssa.h"
#include "except.h"
#include "gimple-pretty-print.h"
#include "toplev.h"
#include "debug.h"
#include "tree-inline.h"
#include "value-prof.h"
#include "tree-ssa-live.h"
#include "tree-outof-ssa.h"
#include "cfgloop.h"
#include "insn-attr.h" /* For INSN_SCHEDULING.  */
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "tree-ssa-address.h"
#include "output.h"
#include "builtins.h"
#include "opts.h"


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
#include "context.h"
#include "rtl-iter.h"

#include "mtcsemit.h"
#include "mtcstarget.h"
#include "mtcsdfcore.h"
#include "mtcsdfproblems.h"
#include "mtcsprintrtl.h"

void mtcs_emit_init(MtcsEmit *self)
{
    self->free_sequence_stack=NULL;
}


/* Add INSN to the end of the doubly-linked list, between PREV and NEXT.
   INSN may be any object that can appear in the chain: INSN_P and NOTE_P objects,
   but also BARRIERs and JUMP_TABLE_DATAs.  PREV and NEXT may be NULL.  */
//原型 link_insn_into_chain emit-rtl.cc
static inline void link_insn_into_chain (rtx_insn *insn, rtx_insn *prev, rtx_insn *next)
{
  SET_PREV_INSN (insn) = prev;
  SET_NEXT_INSN (insn) = next;
  if (prev != NULL){
      SET_NEXT_INSN (prev) = insn;
      if (NONJUMP_INSN_P (prev) && GET_CODE (PATTERN (prev)) == SEQUENCE){
          rtx_sequence *sequence = as_a <rtx_sequence *> (PATTERN (prev));
          SET_NEXT_INSN (sequence->insn (sequence->len () - 1)) = insn;
      }
  }
  if (next != NULL){
      SET_PREV_INSN (next) = insn;
      if (NONJUMP_INSN_P (next) && GET_CODE (PATTERN (next)) == SEQUENCE){
          rtx_sequence *sequence = as_a <rtx_sequence *> (PATTERN (next));
          SET_PREV_INSN (sequence->insn (0)) = insn;
      }
  }

  if (NONJUMP_INSN_P (insn) && GET_CODE (PATTERN (insn)) == SEQUENCE){
      rtx_sequence *sequence = as_a <rtx_sequence *> (PATTERN (insn));
      SET_PREV_INSN (sequence->insn (0)) = prev;
      SET_NEXT_INSN (sequence->insn (sequence->len () - 1)) = next;
  }
}

/* Like `make_insn_raw' but make a JUMP_INSN instead of an insn.  */
//原型 make_jump_insn_raw emit-rtl.cc
static rtx_insn *make_jump_insn_raw (rtx pattern,void *userData)
{
  MtcsEmit *self =(MtcsEmit *)userData;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  rtx_jump_insn *insn;

  insn = as_a <rtx_jump_insn *> (rtx_alloc (JUMP_INSN));
  n_debug("mtcsemit.c make_jump_insn_raw ---- xx cur_insn_uid:%d\n",mtcsRtlData->emit.x_cur_insn_uid);
  INSN_UID (insn) = mtcsRtlData->emit.x_cur_insn_uid/*!cur_insn_uid*/++;

  PATTERN (insn) = pattern;
  INSN_CODE (insn) = -1;
  REG_NOTES (insn) = NULL;
  JUMP_LABEL (insn) = NULL;
  INSN_LOCATION (insn) = mtcs_emit_curr_insn_location (self);
  BLOCK_FOR_INSN (insn) = NULL;

  return insn;
}

/* Like `make_insn_raw' but make a CALL_INSN instead of an insn.  */
static rtx_insn * make_call_insn_raw (rtx pattern,void *userData)
{
  MtcsEmit *self =(MtcsEmit *)userData;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  rtx_call_insn *insn;
  insn = as_a <rtx_call_insn *> (rtx_alloc (CALL_INSN));
  INSN_UID (insn) = mtcsRtlData->emit.x_cur_insn_uid/*!cur_insn_uid*/++;
  PATTERN (insn) = pattern;
  INSN_CODE (insn) = -1;
  REG_NOTES (insn) = NULL;
  CALL_INSN_FUNCTION_USAGE (insn) = NULL;
  INSN_LOCATION (insn) = mtcs_emit_curr_insn_location (self);
  BLOCK_FOR_INSN (insn) = NULL;
  return insn;
}

/* Like `make_insn_raw' but make a DEBUG_INSN instead of an insn.  */
static rtx_insn *make_debug_insn_raw (rtx pattern,void *userData)
{
  MtcsEmit *self =(MtcsEmit *)userData;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  rtx_debug_insn *insn;
  insn = as_a <rtx_debug_insn *> (rtx_alloc (DEBUG_INSN));
  INSN_UID (insn) =  mtcsRtlData->emit.x_cur_debug_insn_uid++;
  if (mtcsRtlData->emit.x_cur_debug_insn_uid > mtcsOptionsItem->x_param_min_nondebug_insn_uid)
    INSN_UID (insn) = mtcsRtlData->emit.x_cur_insn_uid/*!cur_insn_uid*/++;

  PATTERN (insn) = pattern;
  INSN_CODE (insn) = -1;
  REG_NOTES (insn) = NULL;
  INSN_LOCATION (insn) = mtcs_emit_curr_insn_location (self);
  BLOCK_FOR_INSN (insn) = NULL;

  return insn;
}

/* Like `make_insn_raw' but make a NOTE instead of an insn.  */
static rtx_note *make_note_raw (MtcsEmit *self,enum insn_note subtype)
{
  /* Some notes are never created this way at all.  These notes are
     only created by patching out insns.  */
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  gcc_assert (subtype != NOTE_INSN_DELETED_LABEL && subtype != NOTE_INSN_DELETED_DEBUG_LABEL);
  rtx_note *note = as_a <rtx_note *> (rtx_alloc (NOTE));
  INSN_UID (note) =  mtcsRtlData->emit.x_cur_insn_uid/*!cur_insn_uid*/++;
  NOTE_KIND (note) = subtype;
  BLOCK_FOR_INSN (note) = NULL;
  memset (&NOTE_DATA (note), 0, sizeof (NOTE_DATA (note)));
  return note;
}

/* Add INSN to the end of the doubly-linked list.
   INSN may be an INSN, JUMP_INSN, CALL_INSN, CODE_LABEL, BARRIER or NOTE.  */
//原型 add_insn rtl.h emit-rtl.cc
void mtcs_emit_add_insn (MtcsEmit *self,rtx_insn *insn)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  n_debug("mtcsemit.c mtcs_emit_add_insn -- %p  %d %s %s UID:%d\n",
        insn,GET_CODE(insn),GET_RTX_NAME(GET_CODE(insn)),GET_RTX_FORMAT (GET_CODE (insn)),INSN_UID(insn));
  rtx_insn *prev = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
  link_insn_into_chain(insn, prev, NULL);/*link_insn_into_chain (insn, prev, NULL);*/
  if (mtcs_rtl_data_get_insns (mtcsRtlData) == NULL)
      mtcs_rtl_data_set_first_insn/*!set_first_insn (insn)*/(mtcsRtlData,insn);
  mtcs_rtl_data_set_last_insn/*!set_last_insn (insn)*/(mtcsRtlData,insn);
}


/* Make an insn of code NOTE or type NOTE_NO
   and add it to the end of the doubly-linked list.  */
//原型 emit_note emit-rtl.h emit-rtl.cc rtx_note  *mtcs_emit_emit_note (MtcsEmit *self,enum insn_note kind);
//enum insn_note kind编译通不过，改成整形
rtx_note * mtcs_emit_emit_note (MtcsEmit *self,int insn_note_kind)
{
  rtx_note *note = make_note_raw (self,(enum insn_note)insn_note_kind);
  mtcs_emit_add_insn (self,note);
  return note;
}

void mtcs_emit_start_sequence (MtcsEmit *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  struct sequence_stack *tem;
  struct sequence_stack *currentSeq=mtcs_rtl_data_get_current_sequence(mtcsRtlData);
  if (self->free_sequence_stack != NULL){
      tem = self->free_sequence_stack;
      self->free_sequence_stack = tem->next;
  }else
    tem = ggc_alloc<sequence_stack> ();

  tem->next =currentSeq/* get_current_sequence ()*/->next;
  tem->first = mtcs_rtl_data_get_insns/*!get_insns*/ (mtcsRtlData);
  tem->last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/ (mtcsRtlData);
  currentSeq/*get_current_sequence ()*/->next = tem;
  mtcs_rtl_data_set_first_insn/*!set_first_insn*/ (mtcsRtlData,NULL);
  mtcs_rtl_data_set_last_insn/*!set_last_insn*/ (mtcsRtlData,NULL);
}

/* After emitting to a sequence, restore previous saved state.

   To get the contents of the sequence just made, you must call
   `get_insns' *before* calling here.

   If the compiler might have deferred popping arguments while
   generating this sequence, and this sequence will not be immediately
   inserted into the instruction stream, use do_pending_stack_adjust
   before calling get_insns.  That will ensure that the deferred
   pops are inserted into this sequence, and not into some random
   location in the instruction stream.  See INHIBIT_DEFER_POP for more
   information about deferred popping of arguments.  */
//原型 end_sequence rtl.h emit-rtl.cc
void mtcs_emit_end_sequence (MtcsEmit *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  struct sequence_stack *tem = mtcs_rtl_data_get_current_sequence/*!get_current_sequence*/ (mtcsRtlData)->next;

  mtcs_rtl_data_set_first_insn (mtcsRtlData,tem->first);
  mtcs_rtl_data_set_last_insn (mtcsRtlData,tem->last);
  mtcs_rtl_data_get_current_sequence (mtcsRtlData)->next = tem->next;

  memset (tem, 0, sizeof (*tem));
  tem->next = self->free_sequence_stack;
  self->free_sequence_stack = tem;
}


/* Generate a REG rtx for a new pseudo register of mode MODE.
   This pseudo is assigned the next sequential register number.  */
//原型 gen_reg_rtx rtl.h emit-rtl.cc
rtx mtcs_emit_gen_reg_rtx (MtcsEmit *self,mtcs_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
  MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  rtx val;

  unsigned int align =mtcs_mode_get_alignment(mtcsMode,mode);/* GET_MODE_ALIGNMENT (mode);*/

  gcc_assert (can_create_pseudo_p ());

  /* If a virtual register with bigger mode alignment is generated,
     increase stack alignment estimation because it might be spilled
     to stack later.  */
  if (mtcs_func_is_support_stack_alignment(mtcsFunc)/*!SUPPORTS_STACK_ALIGNMENT*/
          && mtcsRtlData->stack_alignment_estimated < align  && !mtcsRtlData->stack_realign_processed){
      unsigned int min_align = mtcs_mode_get_mininum_alignment/*MINIMUM_ALIGNMENT*/ (mtcsMode,NULL, mode, align);
      if (mtcsRtlData->stack_alignment_estimated < min_align)
          mtcsRtlData->stack_alignment_estimated = min_align;
  }
  //generating_concat_p rtl.h中声明 复数
  if (mtcsRTL->generating_concat_p && (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) == MODE_COMPLEX_FLOAT
        || mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) == MODE_COMPLEX_INT)){
      /* For complex modes, don't make a single pseudo.
     Instead, make a CONCAT of two pseudos.
     This allows noncontiguous allocation of the real and imaginary parts,
     which makes much better code.  Besides, allocating DCmode
     pseudos overstrains reload on some machines like the 386.  */
      rtx realpart, imagpart;
      mtcs_mode partmode=mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode);
      realpart = mtcs_emit_gen_reg_rtx (self,partmode);
      imagpart = mtcs_emit_gen_reg_rtx (self,partmode);
      return gen_rtx_CONCAT (mode, realpart, imagpart);
  }

  /* Do not call gen_reg_rtx with uninitialized crtl.  */
  gcc_assert (mtcsRtlData->emit.regno_pointer_align_length);
  mtcs_rtl_data_ensure_regno_capacity(mtcsRtlData);/*  crtl->emit.ensure_regno_capacity ();*/
  gcc_assert (mtcsRtlData->emit.x_reg_rtx_no < mtcsRtlData->emit.regno_pointer_align_length);

  val = mtcs_rtl_gen_raw_REG/*gen_raw_REG*/(mtcsRTL,mode, mtcsRtlData->emit.x_reg_rtx_no);
  mtcsRtlData->regno_reg_rtx[mtcsRtlData->emit.x_reg_rtx_no++] = val;
  n_debug("mtcsemit.c  gen_reg_rtx ----x_reg_rtx_no:%d val:%p mode:%d\n",mtcsRtlData->emit.x_reg_rtx_no,val,mode);
  return val;
}



/* Take X and emit it at the end of the doubly-linked
   INSN list.

   Returns the last insn emitted.  */
//原型 emit_insn emit-rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_insn (MtcsEmit *self,rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  rtx_insn *last = mtcs_rtl_data_get_last_insn (mtcsRtlData);
  rtx_insn *insn;
  n_debug("mtcsemit.c mtcs_emit_emit_insn 00 x:%p\n",x);
  if (x == NULL_RTX)
    return last;

  switch (GET_CODE (x)){
    case DEBUG_INSN:
    case INSN:
    case JUMP_INSN:
    case CALL_INSN:
    case CODE_LABEL:
    case BARRIER:
    case NOTE:
      n_debug("mtcsemit.c mtcs_emit_emit_insn 11 x:%p code:%d\n",x,GET_CODE (x));
      insn = as_a <rtx_insn *> (x);
      while (insn){
          rtx_insn *next = NEXT_INSN (insn);
          n_debug("mtcsemit.c mtcs_emit_emit_insn 22 x:%p code:%d %p\n",x,GET_CODE (x),next);

          mtcs_emit_add_insn (self,insn);
          last = insn;
          insn = next;
      }
      break;

#ifdef ENABLE_RTL_CHECKING
    case JUMP_TABLE_DATA:
    case SEQUENCE:
      gcc_unreachable ();
      break;
#endif

    default:
       n_debug("mtcsemit.c mtcs_emit_emit_insn 33 x:%p code:%d %s\n",x,GET_CODE (x),GET_RTX_NAME(GET_CODE (x)));

      last = mtcs_emit_make_insn_raw/*!make_insn_raw*/(self,x);
      mtcs_emit_add_insn (self,last);
      break;
  }
  return last;
}

/* Emit the rtl pattern X as an appropriate kind of insn.  Also emit a
   following barrier if the instruction needs one and if ALLOW_BARRIER_P
   is true.

   If X is a label, it is simply added into the insn chain.  */
//原型 emit rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit (MtcsEmit *self,rtx x, bool allow_barrier_p)
{
  enum rtx_code code = classify_insn (x);
  switch (code){
    case CODE_LABEL:
      return mtcs_emit_emit_label (self,x);
    case INSN:
      return mtcs_emit_emit_insn (self,x);
    case  JUMP_INSN:
      {
    rtx_insn *insn = mtcs_emit_emit_jump_insn (self,x);
    if (allow_barrier_p  && (any_uncondjump_p (insn) || GET_CODE (x) == RETURN))
      return mtcs_emit_emit_barrier (self);
    return insn;
      }
    case CALL_INSN:
      return mtcs_emit_emit_call_insn (self,x);
    case DEBUG_INSN:
      return mtcs_emit_emit_debug_insn (self,x);
    default:
      gcc_unreachable ();
  }
}

/* Make an insn of code CALL_INSN with pattern X
   and add it to the end of the doubly-linked list.  */
//原型 emit_call_insn rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_call_insn (MtcsEmit *self,rtx x)
{
  rtx_insn *insn;
  switch (GET_CODE (x)){
    case DEBUG_INSN:
    case INSN:
    case JUMP_INSN:
    case CALL_INSN:
    case CODE_LABEL:
    case BARRIER:
    case NOTE:
      insn = mtcs_emit_emit_insn (self,x);
      break;

#ifdef ENABLE_RTL_CHECKING
    case SEQUENCE:
    case JUMP_TABLE_DATA:
      gcc_unreachable ();
      break;
#endif
    default:
      insn = make_call_insn_raw (x,(void*)self);
      mtcs_emit_add_insn (self,insn);
      break;
  }
  return insn;
}

/* Make an insn of code DEBUG_INSN with pattern X
   and add it to the end of the doubly-linked list.  */
//原型 emit_debug_insn rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_debug_insn (MtcsEmit *self,rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  rtx_insn *last = mtcs_rtl_data_get_last_insn (mtcsRtlData);
  rtx_insn *insn;
  if (x == NULL_RTX)
    return last;
  switch (GET_CODE (x)){
    case DEBUG_INSN:
    case INSN:
    case JUMP_INSN:
    case CALL_INSN:
    case CODE_LABEL:
    case BARRIER:
    case NOTE:
      insn = as_a <rtx_insn *> (x);
      while (insn){
          rtx_insn *next = NEXT_INSN (insn);
          mtcs_emit_add_insn (self,insn);
          last = insn;
          insn = next;
      }
      break;

#ifdef ENABLE_RTL_CHECKING
    case JUMP_TABLE_DATA:
    case SEQUENCE:
      gcc_unreachable ();
      break;
#endif
    default:
      last = make_debug_insn_raw (x,(void*)self);
      mtcs_emit_add_insn (self,last);
      break;
  }
  return last;
}

/* Add INSN into the doubly-linked list before insn BEFORE.  */
//原型 add_insn_before_nobb emit-rtl.cc
static void add_insn_before_nobb (MtcsEmit *self,rtx_insn *insn, rtx_insn *before)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  rtx_insn *prev = PREV_INSN (before);
  gcc_assert (!optimize || !before->deleted ());
  link_insn_into_chain (insn, prev, before);
  if (prev == NULL){
      struct sequence_stack *seq;
      for (seq = mtcs_rtl_data_get_current_sequence (mtcsRtlData); seq; seq = seq->next)
         if (before == seq->first){
            seq->first = insn;
            break;
         }
      gcc_assert (seq);
  }
}


/* Add INSN into the doubly-linked list after insn AFTER.  */
//原型 add_insn_after_nobb emit-rtl.cc
static void add_insn_after_nobb (MtcsEmit *self,rtx_insn *insn, rtx_insn *after)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  rtx_insn *next = NEXT_INSN (after);
  gcc_assert (!optimize || !after->deleted ());
  link_insn_into_chain (insn, after, next);
  if (next == NULL){
      struct sequence_stack *seq;
      for (seq = mtcs_rtl_data_get_current_sequence (mtcsRtlData); seq; seq = seq->next)
        if (after == seq->last){
            seq->last = insn;
            break;
        }
  }
}

/* Like add_insn_before_nobb, but try to set BLOCK_FOR_INSN.
   If BB is NULL, an attempt is made to infer the bb from before.

   This and the previous function should be the only functions called
   to insert an insn once delay slots have been filled since only
   they know how to update a SEQUENCE. */
//原型 add_insn_before rtl.h emit-rtl.cc
void mtcs_emit_add_insn_before (MtcsEmit *self,rtx_insn *insn, rtx_insn *before, basic_block bb)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan *mtcsDfscan =mtcs_target_get_dfscan (mtcsTarget);

   add_insn_before_nobb (self,insn, before);
   if (!bb  && !BARRIER_P (before)  && !BARRIER_P (insn))
      bb = BLOCK_FOR_INSN (before);

   if (bb){
      set_block_for_insn (insn, bb);
      if (INSN_P (insn))
         mtcs_dfscan_df_insn_rescan/*!df_insn_rescan*/(mtcsDfscan,insn);
      /* Should not happen as first in the BB is always either NOTE or
      LABEL.  */
      gcc_assert (BB_HEAD (bb) != insn
         /* Avoid clobbering of structure when creating new BB.  */
         || BARRIER_P (insn)  || NOTE_INSN_BASIC_BLOCK_P (insn));
   }
}

/* Like add_insn_after_nobb, but try to set BLOCK_FOR_INSN.
   If BB is NULL, an attempt is made to infer the bb from before.

   This and the next function should be the only functions called
   to insert an insn once delay slots have been filled since only
   they know how to update a SEQUENCE. */
//原型 add_insn_after rtl.h emit-rtl.cc
void mtcs_emit_add_insn_after (MtcsEmit *self,rtx_insn *insn, rtx_insn *after, basic_block bb)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);

   add_insn_after_nobb (self,insn, after);
   if (!BARRIER_P (after)   && !BARRIER_P (insn)  && (bb = BLOCK_FOR_INSN (after))){
      set_block_for_insn (insn, bb);
      if (INSN_P (insn))
         mtcs_dfscan_df_insn_rescan/*!df_insn_rescan*/(mtcsDfscan,insn);
      /* Should not happen as first in the BB is always
      either NOTE or LABEL.  */
      if (BB_END (bb) == after
      /* Avoid clobbering of structure when creating new BB.  */
      && !BARRIER_P (insn)
      && !NOTE_INSN_BASIC_BLOCK_P (insn))
         BB_END (bb) = insn;
   }
}

/* Emit the label LABEL before the insn BEFORE.  */
//原型 emit_label_before rtl.h emit-rtl.cc
rtx_code_label *mtcs_emit_emit_label_before (MtcsEmit *self,rtx_code_label *label, rtx_insn *before)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  gcc_checking_assert (INSN_UID (label) == 0);
  n_debug("mtcsemit.c mtcs_emit_emit_label_before 00 label:%p after:%p\n",label,before);

  INSN_UID (label) = mtcsRtlData->emit.x_cur_insn_uid/*!cur_insn_uid*/++;
  mtcs_emit_add_insn_before (self,label, before, NULL);
  return label;
}

/* Add the label LABEL to the end of the doubly-linked list.  */
//原型 emit_label rtl.h emit-rtl.cc
rtx_code_label *mtcs_emit_emit_label (MtcsEmit *self,rtx uncast_label)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  rtx_code_label *label = as_a <rtx_code_label *> (uncast_label);
  gcc_checking_assert (INSN_UID (label) == 0);
  n_debug("mtcsemit.c mtcs_emit_emit_label 00 label:%p\n",label);

  INSN_UID (label) = mtcsRtlData->emit.x_cur_insn_uid/*!cur_insn_uid*/++;
  mtcs_emit_add_insn (self,label);
  return label;
}

/* Make an insn of code JUMP_TABLE_DATA
   and add it to the end of the doubly-linked list.  */
//原型 emit_jump_table_data rtl.h emit-rtl.cc
rtx_jump_table_data * mtcs_emit_emit_jump_table_data (MtcsEmit *self,rtx table)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  rtx_jump_table_data *jump_table_data =
    as_a <rtx_jump_table_data *> (rtx_alloc (JUMP_TABLE_DATA));
  INSN_UID (jump_table_data) =mtcsRtlData->emit.x_cur_insn_uid/*!cur_insn_uid*/++;
  PATTERN (jump_table_data) = table;
  BLOCK_FOR_INSN (jump_table_data) = NULL;
  mtcs_emit_add_insn/*!add_insn*/(self,jump_table_data);
  return jump_table_data;
}


/* Make an insn of code JUMP_INSN with pattern X
   and add it to the end of the doubly-linked list.  */
//原型 emit_jump_insn rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_jump_insn (MtcsEmit *self,rtx x)
{
  rtx_insn *last = NULL;
  rtx_insn *insn;
  switch (GET_CODE (x)){
    case DEBUG_INSN:
    case INSN:
    case JUMP_INSN:
    case CALL_INSN:
    case CODE_LABEL:
    case BARRIER:
    case NOTE:
      insn = as_a <rtx_insn *> (x);
      n_debug("mtcsemit.c mtcs_emit_emit_jump_insn x:%p insn:p\n",x,insn);
      while (insn){
          rtx_insn *next = NEXT_INSN (insn);
          n_debug("mtcsemit.c mtcs_emit_emit_jump_insn 00 x:%p insn:%p next:%p\n",x,insn,next);

          mtcs_emit_add_insn (self,insn);
          last = insn;
          insn = next;
      }
      break;
#ifdef ENABLE_RTL_CHECKING
    case JUMP_TABLE_DATA:
    case SEQUENCE:
      gcc_unreachable ();
      break;
#endif
    default:
      last = make_jump_insn_raw (x,(void*)self);
      n_debug("mtcsemit.c mtcs_emit_emit_jump_insn 11 x:%p last:%p\n",x,last, INSN_UID (last));
      mtcs_emit_add_insn/*!add_insn*/(self,last);
      break;
  }
  return last;
}

/* Make an insn of code BARRIER
   and output it before the insn BEFORE.  */
//原型 emit_barrier_before rtl.h emit-rtl.cc
rtx_barrier * mtcs_emit_emit_barrier_before (MtcsEmit *self,rtx_insn *before)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  rtx_barrier *insn = as_a <rtx_barrier *> (rtx_alloc (BARRIER));
  INSN_UID (insn) = mtcsRtlData->emit.x_cur_insn_uid/*!cur_insn_uid*/++;
  mtcs_emit_add_insn_before (self,insn, before, NULL);
  return insn;
}

/* Make an insn of code BARRIER
   and add it to the end of the doubly-linked list.  */
//原型 emit_barrier rtl.h emit-rtl.cc
rtx_barrier *mtcs_emit_emit_barrier (MtcsEmit *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  rtx_barrier *barrier = as_a <rtx_barrier *> (rtx_alloc (BARRIER));
  INSN_UID (barrier) = mtcsRtlData->emit.x_cur_insn_uid/*!cur_insn_uid*/++;
  mtcs_emit_add_insn (self,barrier);
  return barrier;
}

/* Emit a clobber of lvalue X.  */
//原型 emit_clobber rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_clobber (MtcsEmit *self,rtx x)
{
  /* CONCATs should not appear in the insn stream.  */
  if (GET_CODE (x) == CONCAT){
      mtcs_emit_emit_clobber (self,XEXP (x, 0));
      return mtcs_emit_emit_clobber (self,XEXP (x, 1));
  }
  return mtcs_emit_emit_insn (self,gen_rtx_CLOBBER (VOIDmode, x));
}

/* Add an unconditional jump to LABEL as the next sequential instruction.  */
//原型 emit_jump rtl.h stmt.cc
void mtcs_emit_emit_jump (MtcsEmit *self,rtx label)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  n_debug("mtcsemit.c mtcs_emit_emit_jump 00 %p\n",label);
  mtcs_dojump_do_pending_stack_adjust (mtcsDojump);
  mtcs_emit_emit_jump_insn (self,target_rtx_gen_jump/*!targetm.gen_jump*/(mtcsMachine->tmrtx,label));
  mtcs_emit_emit_barrier (self);
  n_debug("mtcsemit.c mtcs_emit_emit_jump 11 %p\n",label);

}

/* Make an insn of code BARRIER
   and output it after the insn AFTER.  */
//原型 emit_barrier_after rtl.h emit-rtl.cc
rtx_barrier *mtcs_emit_emit_barrier_after (MtcsEmit *self,rtx_insn *after)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  rtx_barrier *insn = as_a <rtx_barrier *> (rtx_alloc (BARRIER));
  INSN_UID (insn) =mtcsRtlData->emit.x_cur_insn_uid/*!cur_insn_uid*/++;
  mtcs_emit_add_insn_after (self,insn, after, NULL);
  return insn;
}

/* Set up the insn chain starting with FIRST as the current sequence,
   saving the previously current one.  See the documentation for
   start_sequence for more information about how to use this function.  */
//原型 push_to_sequence rtl.h emit-rtl.cc
void mtcs_emit_push_to_sequence (MtcsEmit *self,rtx_insn *first)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  rtx_insn *last;
  mtcs_emit_start_sequence/*!start_sequence*/(self);
  for (last = first; last && NEXT_INSN (last); last = NEXT_INSN (last))
    ;
  mtcs_rtl_data_set_first_insn/*!set_first_insn*/(mtcsRtlData,first);
  mtcs_rtl_data_set_last_insn/*!set_last_insn*/(mtcsRtlData,last);
}

/* Like push_to_sequence, but take the last insn as an argument to avoid
   looping through the list.  */
//原型 push_to_sequence2 rtl.h emit-rtl.cc
void mtcs_emit_push_to_sequence2 (MtcsEmit *self,rtx_insn *first, rtx_insn *last)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
    MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

    mtcs_emit_start_sequence/*!start_sequence*/(self);
    mtcs_rtl_data_set_first_insn/*!set_first_insn*/(mtcsRtlData,first);
    mtcs_rtl_data_set_last_insn/*!set_last_insn*/(mtcsRtlData,last);
}

/* Set up the outer-level insn chain
   as the current sequence, saving the previously current one.  */
//原型 push_topmost_sequence rtl.h emit-rtl.cc
void mtcs_emit_push_topmost_sequence (MtcsEmit *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  struct sequence_stack *top;
  mtcs_emit_start_sequence(self);
  top = mtcs_emit_get_topmost_sequence(self);
  mtcs_rtl_data_set_first_insn(mtcsRtlData,top->first);
  mtcs_rtl_data_set_last_insn(mtcsRtlData,top->last);
}

/* Return the outermost sequence.  */
//原型 get_topmost_sequence emit-rtl.h
struct sequence_stack *mtcs_emit_get_topmost_sequence (MtcsEmit *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  struct sequence_stack *seq, *top;
  seq = mtcs_rtl_data_get_current_sequence(mtcsRtlData);
  do{
      top = seq;
      seq = seq->next;
  } while (seq);
  return top;
}

/* After emitting to the outer-level insn chain, update the outer-level
   insn chain, and restore the previous saved state.  */
//原型 pop_topmost_sequence rtl.h emit-rtl.cc
void mtcs_emit_pop_topmost_sequence (MtcsEmit *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  struct sequence_stack *top;
  top = mtcs_emit_get_topmost_sequence(self);
  top->first = mtcs_rtl_data_get_insns (mtcsRtlData);
  top->last =  mtcs_rtl_data_get_last_insn(mtcsRtlData);
  mtcs_emit_end_sequence(self);
}


/* Unlink INSN from the insn chain.

   This function knows how to handle sequences.

   This function does not invalidate data flow information associated with
   INSN (i.e. does not call df_insn_delete).  That makes this function
   usable for only disconnecting an insn from the chain, and re-emit it
   elsewhere later.

   To later insert INSN elsewhere in the insn chain via add_insn and
   similar functions, PREV_INSN and NEXT_INSN must be nullified by
   the caller.  Nullifying them here breaks many insn chain walks.

   To really delete an insn and related DF information, use delete_insn.  */
//原型 remove_insn emit-rtl.h emit-rtl.cc
void mtcs_emit_remove_insn (MtcsEmit *self,rtx_insn *insn)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  rtx_insn *next = NEXT_INSN (insn);
  rtx_insn *prev = PREV_INSN (insn);
  basic_block bb;

  n_debug("mtcsemit.c mtcs_emit_remove_insn 00 insn:%p next:%p prev:%p\n",insn,next,prev);

  if (prev){
      SET_NEXT_INSN (prev) = next;
      if (NONJUMP_INSN_P (prev) && GET_CODE (PATTERN (prev)) == SEQUENCE){
          rtx_sequence *sequence = as_a <rtx_sequence *> (PATTERN (prev));
          SET_NEXT_INSN (sequence->insn (sequence->len () - 1)) = next;
      }
  }else{

      struct sequence_stack *seq;
      for (seq =mtcs_rtl_data_get_current_sequence(mtcsRtlData); seq; seq = seq->next)
          if (insn == seq->first){
            seq->first = next;
            break;
          }
      n_debug("mtcsemit.c mtcs_emit_remove_insn 11 insn:%p next:%p prev:%p seq:%p\n",insn,next,prev,seq);

      gcc_assert (seq);
  }

  if (next){
      SET_PREV_INSN (next) = prev;
      if (NONJUMP_INSN_P (next) && GET_CODE (PATTERN (next)) == SEQUENCE){
          rtx_sequence *sequence = as_a <rtx_sequence *> (PATTERN (next));
          SET_PREV_INSN (sequence->insn (0)) = prev;
      }
  }else{
      struct sequence_stack *seq;
      for (seq = mtcs_rtl_data_get_current_sequence(mtcsRtlData); seq; seq = seq->next){
         n_debug("mtcsemit.c mtcs_emit_remove_insn 22aa insn:%p next:%p prev:%p seq:%p seq->last:%p\n",
               insn,next,prev,seq,seq->last);

          if (insn == seq->last){
              seq->last = prev;
              break;
          }
      }
      n_debug("mtcsemit.c mtcs_emit_remove_insn 22 insn:%p next:%p prev:%p seq:%p\n",insn,next,prev,seq);

      gcc_assert (seq);
  }
  /* Fix up basic block boundaries, if necessary.  */
  if (!BARRIER_P (insn)  && (bb = BLOCK_FOR_INSN (insn))){
      if (BB_HEAD (bb) == insn){
          /* Never ever delete the basic block note without deleting whole
             basic block.  */
          gcc_assert (!NOTE_P (insn));
          BB_HEAD (bb) = next;
      }
      if (BB_END (bb) == insn)
          BB_END (bb) = prev;
  }
}



/* Go through all the RTL insn bodies and copy any invalid shared
   structure.  This routine should only be called once.  */
static void unshare_all_rtl_1 (MtcsEmit *self,rtx_insn *insn)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  /* Unshare just about everything else.  */
  mtcs_rtl_unshare_all_rtl_in_chain/*!unshare_all_rtl_in_chain*/(mtcsRTL,insn);
  /* Make sure the addresses of stack slots found outside the insn chain
     (such as, in DECL_RTL of a variable) are not shared
     with the insn chain.

     This special care is necessary when the stack slot MEM does not
     actually appear in the insn chain.  If it does appear, its address
     is unshared from all else at that point.  */
  unsigned int i;
  rtx temp;
  FOR_EACH_VEC_SAFE_ELT (mtcsRtlData->x_stack_slot_list/*!stack_slot_list*/, i, temp)
     (*mtcsRtlData->x_stack_slot_list/*!stack_slot_list*/)[i] = mtcs_rtl_copy_rtx_if_shared/*!copy_rtx_if_shared*/(mtcsRTL,temp);
}


//原型 unshare_all_rtl rtl.h emit-rtl.cc
void mtcs_emit_unshare_all_rtl (MtcsEmit *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsAsm *mtcsAsm =(MtcsAsm *)mtcs_target_get_asm(mtcsTarget);

  unshare_all_rtl_1(self,mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData));
  for (tree decl = DECL_ARGUMENTS (cfun->decl); decl; decl = DECL_CHAIN (decl)){
      if (DECL_RTL_SET_P (decl))
         mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,decl,
               mtcs_rtl_copy_rtx_if_shared/*!copy_rtx_if_shared*/(mtcsRTL,mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl)));
      DECL_INCOMING_RTL (decl) = mtcs_rtl_copy_rtx_if_shared/*!copy_rtx_if_shared*/(mtcsRTL,DECL_INCOMING_RTL (decl));
  }
}


/* Emit a use of rvalue X.  */
//原型 emit_use rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_use (MtcsEmit *self,rtx x)
{
  /* CONCATs should not appear in the insn stream.  */
  if (GET_CODE (x) == CONCAT){
      mtcs_emit_emit_use/*1emit_use*/(self,XEXP (x, 0));
      return mtcs_emit_emit_use/*1emit_use*/(self,XEXP (x, 1));
  }
  return mtcs_emit_emit_insn/*!emit_insn*/(self,gen_rtx_USE (VOIDmode, x));
}

/* Return a sequence of insns to use rvalue X.  */
//原型 gen_use rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_gen_use (MtcsEmit *self,rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   rtx_insn *seq;

   mtcs_emit_start_sequence/*!start_sequence*/(self);
   emit_use (x);
   seq = mtcs_rtl_data_get_insns/*!get_insns*/ (mtcsRtlData);
   mtcs_emit_end_sequence/*!end_sequence*/(self);
   return seq;
}

/* Helper for emit_insn_after, handles lists of instructions
   efficiently.  */
//原型 emit_insn_after_1 emit-rtl.cc
static rtx_insn * emit_insn_after_1 (MtcsEmit *self,rtx_insn *first, rtx_insn *after, basic_block bb)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   rtx_insn *last;
   rtx_insn *after_after;
   if (!bb && !BARRIER_P (after))
      bb = BLOCK_FOR_INSN (after);

   if (bb){
      mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,bb);
      for (last = first; NEXT_INSN (last); last = NEXT_INSN (last))
         if (!BARRIER_P (last)){
            set_block_for_insn (last, bb);
            mtcs_dfscan_df_insn_rescan/*!df_insn_rescan*/(mtcsDfscan,last);
         }
      if (!BARRIER_P (last)){
         set_block_for_insn (last, bb);
         mtcs_dfscan_df_insn_rescan/*!df_insn_rescan*/(mtcsDfscan,last);
      }
      if (BB_END (bb) == after)
         BB_END (bb) = last;
   }else
      for (last = first; NEXT_INSN (last); last = NEXT_INSN (last))
         continue;

   after_after = NEXT_INSN (after);

   SET_NEXT_INSN (after) = first;
   SET_PREV_INSN (first) = after;
   SET_NEXT_INSN (last) = after_after;
   if (after_after)
      SET_PREV_INSN (after_after) = last;

   if (after == mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData))
      mtcs_rtl_data_set_last_insn/*!set_last_insn*/(mtcsRtlData,last);

   return last;
}


//原型 emit_pattern_after_noloc emit-rtl.cc
static rtx_insn * emit_pattern_after_noloc (MtcsEmit *self,rtx x, rtx_insn *after, basic_block bb,
           rtx_insn *(*make_raw)(rtx,void *userData),void *userData)
{
   n_debug("mtcsemit.c emit-rtl.cc emit_pattern_after_noloc 00 x:%p after:%p bb:%p\n",x,after,bb);

   rtx_insn *last = after;
   gcc_assert (after);
   if (x == NULL_RTX)
      return last;
   switch (GET_CODE (x)){
      case DEBUG_INSN:
      case INSN:
      case JUMP_INSN:
      case CALL_INSN:
      case CODE_LABEL:
      case BARRIER:
      case NOTE:
         last = emit_insn_after_1(self,as_a <rtx_insn *> (x), after, bb);
         n_debug("mtcsemit.c emit-rtl.cc emit_pattern_after_noloc 11 x:%p after:%p bb:%p last:%p\n",x,after,bb,last);

         break;

#ifdef ENABLE_RTL_CHECKING
      case SEQUENCE:
         gcc_unreachable ();
         break;
#endif

      default:
         last = (*make_raw) (x,userData);
         mtcs_emit_add_insn_after/*!add_insn_after*/(self,last, after, bb);
         n_debug("mtcsemit.c emit-rtl.cc emit_pattern_after_noloc 22 x:%p after:%p bb:%p\n",x,after,bb);

         break;
   }
   return last;
}

/* Insert PATTERN after AFTER, setting its INSN_LOCATION to LOC.
   MAKE_RAW indicates how to turn PATTERN into a real insn.  */
//原型 emit_pattern_after_setloc emit-rtl.cc
static rtx_insn * emit_pattern_after_setloc (MtcsEmit *self,rtx pattern, rtx_insn *after, location_t loc,
            rtx_insn *(*make_raw) (rtx,void *),void *userData)
{
   rtx_insn *last = emit_pattern_after_noloc(self,pattern, after, NULL, make_raw,userData);

   if (pattern == NULL_RTX || !loc)
      return last;

   after = NEXT_INSN (after);
   while (1){
      if (active_insn_p (after) && !JUMP_TABLE_DATA_P (after) /* FIXME */ && !INSN_LOCATION (after))
         INSN_LOCATION (after) = loc;
      if (after == last)
         break;
      after = NEXT_INSN (after);
   }
   return last;
}


/* Insert PATTERN after AFTER.  MAKE_RAW indicates how to turn PATTERN
   into a real insn.  SKIP_DEBUG_INSNS indicates whether to insert after
   any DEBUG_INSNs.  */
//原型 emit_pattern_after emit-rtl.cc
static rtx_insn * emit_pattern_after (MtcsEmit *self,rtx pattern, rtx_insn *after, bool skip_debug_insns,
          rtx_insn *(*make_raw) (rtx,void *),void *userData)
{
   rtx_insn *prev = after;

   if (skip_debug_insns)
      while (DEBUG_INSN_P (prev))
         prev = PREV_INSN (prev);

   if (INSN_P (prev)){
      n_debug("mtcsemit.c emit-rtl.cc emit_pattern_after 00 pattern:%p after:%p prev:%p\n",pattern,after,prev);

      return emit_pattern_after_setloc(self,pattern, after, INSN_LOCATION (prev),make_raw,userData);
   }else{
      n_debug("mtcsemit.c emit-rtl.cc emit_pattern_after 11 pattern:%p after:%p\n",pattern,after);

      return emit_pattern_after_noloc(self,pattern, after, NULL, make_raw,userData);
   }
}

static rtx_insn *makeInsnRaw_cb(rtx pattern,void *userData)
{
   return mtcs_emit_make_insn_raw((MtcsEmit *)userData,pattern);
}
/* Like emit_insn_after_noloc, but set INSN_LOCATION according to AFTER.  */
//原型 emit_insn_after rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_insn_after (MtcsEmit *self,rtx pattern, rtx_insn *after)
{
  return emit_pattern_after (self,pattern, after, true, makeInsnRaw_cb,(void*)self);
}


/* Like emit_jump_insn_after_noloc, but set INSN_LOCATION according to AFTER.  */
//原型 emit_jump_insn_after emit-rtl.h emit-rtl.cc
rtx_jump_insn *mtcs_emit_emit_jump_insn_after (MtcsEmit *self,rtx pattern, rtx_insn *after)
{
  return as_a <rtx_jump_insn *> (emit_pattern_after(self,pattern, after, true, make_jump_insn_raw,(void*)self));
}

/* Like emit_debug_insn_after_noloc, but set INSN_LOCATION according to AFTER.  */
//原型 emit_debug_insn_after emit-rtl.h emit-rtl.cc
rtx_insn * mtcs_emit_emit_debug_insn_after (MtcsEmit *self,rtx pattern, rtx_insn *after)
{
  return emit_pattern_after(self,pattern, after, false, make_debug_insn_raw,(void*)self);
}

/* Make an instruction with body X and code CALL_INSN
   and output it after the instruction AFTER.  */
//原型 emit_debug_insn_after emit-rtl.h emit-rtl.cc
rtx_insn * mtcs_emit_emit_call_insn_after_noloc (MtcsEmit *self,rtx x, rtx_insn *after)
{
  return emit_pattern_after_noloc(self,x, after, NULL, make_call_insn_raw,(void*)self);
}

/* Like emit_call_insn_after_noloc, but set INSN_LOCATION according to AFTER.  */
//原型 emit_call_insn_after emit-rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_call_insn_after (MtcsEmit *self,rtx pattern, rtx_insn *after)
{
  return emit_pattern_after(self,pattern, after, true, make_call_insn_raw,(void*)self);
}

/* Emit insn(s) of given code and pattern
   at a specified place within the doubly-linked list.

   All of the emit_foo global entry points accept an object
   X which is either an insn list or a PATTERN of a single
   instruction.

   There are thus a few canonical ways to generate code and
   emit it at a specific place in the instruction stream.  For
   example, consider the instruction named SPOT and the fact that
   we would like to emit some instructions before SPOT.  We might
   do it like this:

   start_sequence ();
   ... emit the new instructions ...
   insns_head = get_insns ();
   end_sequence ();

   emit_insn_before (insns_head, SPOT);

   It used to be common to generate SEQUENCE rtl instead, but that
   is a relic of the past which no longer occurs.  The reason is that
   SEQUENCE rtl results in much fragmented RTL memory since the SEQUENCE
   generated would almost certainly die right after it was created.  */
//原型 emit_pattern_before_noloc emit-rtl.cc
static rtx_insn * emit_pattern_before_noloc (MtcsEmit *self,rtx x, rtx_insn *before, rtx_insn *last,
            basic_block bb,rtx_insn *(*make_raw) (rtx,void *),void *userData)
{
   rtx_insn *insn;
   gcc_assert (before);
   if (x == NULL_RTX)
      return last;
   switch (GET_CODE (x)){
      case DEBUG_INSN:
      case INSN:
      case JUMP_INSN:
      case CALL_INSN:
      case CODE_LABEL:
      case BARRIER:
      case NOTE:
         insn = as_a <rtx_insn *> (x);
         while (insn){
            rtx_insn *next = NEXT_INSN (insn);
            mtcs_emit_add_insn_before/*!add_insn_before*/(self,insn, before, bb);
            last = insn;
            insn = next;
         }
         break;
#ifdef ENABLE_RTL_CHECKING
      case SEQUENCE:
         gcc_unreachable ();
         break;
#endif
      default:
         last = (*make_raw) (x,userData);
         mtcs_emit_add_insn_before/*!add_insn_before*/(self,last, before, bb);
         break;
   }
   return last;
}

/* Insert PATTERN before BEFORE, setting its INSN_LOCATION to LOC.
   MAKE_RAW indicates how to turn PATTERN into a real insn.  INSNP
   indicates if PATTERN is meant for an INSN as opposed to a JUMP_INSN,
   CALL_INSN, etc.  */
//原型 emit_pattern_before_setloc emit-rtl.cc
static rtx_insn *emit_pattern_before_setloc (MtcsEmit *self,rtx pattern, rtx_insn *before, location_t loc,
             bool insnp, rtx_insn *(*make_raw) (rtx,void *),void *userData)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   rtx_insn *first = PREV_INSN (before);
   rtx_insn *last = emit_pattern_before_noloc(self,pattern, before,
                        insnp ? before : NULL,NULL, make_raw,userData);

   if (pattern == NULL_RTX || !loc)
      return last;

   if (!first)
      first = mtcs_rtl_data_get_insns/*!get_insns*/ (mtcsRtlData);
   else
      first = NEXT_INSN (first);
   while (1){
      if (active_insn_p (first) && !JUMP_TABLE_DATA_P (first) /* FIXME */ && !INSN_LOCATION (first))
         INSN_LOCATION (first) = loc;
      if (first == last)
         break;
      first = NEXT_INSN (first);
   }
   return last;
}


/* Insert PATTERN before BEFORE.  MAKE_RAW indicates how to turn PATTERN
   into a real insn.  SKIP_DEBUG_INSNS indicates whether to insert
   before any DEBUG_INSNs.  INSNP indicates if PATTERN is meant for an
   INSN as opposed to a JUMP_INSN, CALL_INSN, etc.  */
//原型 emit_pattern_before emit-rtl.cc
static rtx_insn *emit_pattern_before (MtcsEmit *self,rtx pattern, rtx_insn *before, bool skip_debug_insns,
           bool insnp, rtx_insn *(*make_raw) (rtx,void *),void *userData)
{
   rtx_insn *next = before;
   if (skip_debug_insns)
      while (DEBUG_INSN_P (next))
         next = PREV_INSN (next);

   if (INSN_P (next))
      return emit_pattern_before_setloc(self,pattern, before, INSN_LOCATION (next),insnp, make_raw,userData);
   else
      return emit_pattern_before_noloc(self,pattern, before,insnp ? before : NULL,NULL, make_raw,userData);
}


/* Like emit_insn_before_noloc, but set INSN_LOCATION according to BEFORE.  */
//原型 emit_insn_before rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_insn_before (MtcsEmit *self,rtx pattern, rtx_insn *before)
{
  return emit_pattern_before(self,pattern, before, true, true, makeInsnRaw_cb,(void *)self);
}

/* Notes require a bit of special handling: Some notes need to have their
   BLOCK_FOR_INSN set, others should never have it set, and some should
   have it set or clear depending on the context.   */

/* Return true iff a note of kind SUBTYPE should be emitted with routines
   that never set BLOCK_FOR_INSN on NOTE.  BB_BOUNDARY is true if the
   caller is asked to emit a note before BB_HEAD, or after BB_END.  */
//原型 note_outside_basic_block_p emit-rtl.cc
static bool note_outside_basic_block_p (enum insn_note subtype, bool on_bb_boundary_p)
{
   switch (subtype){
      /* NOTE_INSN_SWITCH_TEXT_SECTIONS only appears between basic blocks.  */
      case NOTE_INSN_SWITCH_TEXT_SECTIONS:
         return true;

      /* Notes for var tracking and EH region markers can appear between or
      inside basic blocks.  If the caller is emitting on the basic block
      boundary, do not set BLOCK_FOR_INSN on the new note.  */
      case NOTE_INSN_VAR_LOCATION:
      case NOTE_INSN_EH_REGION_BEG:
      case NOTE_INSN_EH_REGION_END:
         return on_bb_boundary_p;

      /* Otherwise, BLOCK_FOR_INSN must be set.  */
      default:
         return false;
   }
}

/* Emit a note of subtype SUBTYPE after the insn AFTER.  */
//原型 emit_note_after rtl.h emit-rtl.cc
rtx_note * mtcs_emit_emit_note_after (MtcsEmit *self,enum insn_note subtype, rtx_insn *after)
{
   rtx_note *note = make_note_raw(self,subtype);
   basic_block bb = BARRIER_P (after) ? NULL : BLOCK_FOR_INSN (after);
   bool on_bb_boundary_p = (bb != NULL && BB_END (bb) == after);

   if (note_outside_basic_block_p (subtype, on_bb_boundary_p))
      add_insn_after_nobb(self,note, after);
   else
      mtcs_emit_add_insn_after/*!add_insn_after*/(self,note, after, bb);
   return note;
}

/* Emit a note of subtype SUBTYPE before the insn BEFORE.  */
//原型 emit_note_before rtl.h emit-rtl.cc
rtx_note * mtcs_emit_emit_note_before (MtcsEmit *self,enum insn_note subtype, rtx_insn *before)
{
   rtx_note *note = make_note_raw(self,subtype);
   basic_block bb = BARRIER_P (before) ? NULL : BLOCK_FOR_INSN (before);
   bool on_bb_boundary_p = (bb != NULL && BB_HEAD (bb) == before);

   if (note_outside_basic_block_p (subtype, on_bb_boundary_p))
      add_insn_before_nobb(self,note, before);
   else
      mtcs_emit_add_insn_before/*!add_insn_before*/(self,note, before, bb);
   return note;
}


/* Reset used-flags for INSN.  */
//原型 reset_insn_used_flags emit-rtl.cc
static void reset_insn_used_flags (rtx insn)
{
   gcc_assert (INSN_P (insn));
   reset_used_flags (PATTERN (insn));
   reset_used_flags (REG_NOTES (insn));
   if (CALL_P (insn))
      reset_used_flags (CALL_INSN_FUNCTION_USAGE (insn));
}


/* Go through all the RTL insn bodies and clear all the USED bits.  */
//原型 reset_all_used_flags emit-rtl.cc
static void reset_all_used_flags (MtcsEmit *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   rtx_insn *p;
   for (p = mtcs_rtl_data_get_insns/*!get_insns*/ (mtcsRtlData); p; p = NEXT_INSN (p))
      if (INSN_P (p)){
         rtx pat = PATTERN (p);
         if (GET_CODE (pat) != SEQUENCE)
            reset_insn_used_flags (p);
         else{
            gcc_assert (REG_NOTES (p) == NULL);
            for (int i = 0; i < XVECLEN (pat, 0); i++){
               rtx insn = XVECEXP (pat, 0, i);
               if (INSN_P (insn))
                  reset_insn_used_flags (insn);
            }
         }
      }
}


/* Check that ORIG is not marked when it should not be and mark ORIG as in use,
   Recursively does the same for subexpressions.  */
//原型 verify_rtx_sharing emit-rtl.cc
static void verify_rtx_sharing (MtcsEmit *self,rtx orig, rtx insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg    *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   rtx x = orig;
   int i;
   enum rtx_code code;
   const char *format_ptr;
   if (x == 0)
      return;
   code = GET_CODE (x);
   /* These types may be freely shared.  */
   switch (code){
      case REG:
      case DEBUG_EXPR:
      case VALUE:
      CASE_CONST_ANY:
      case SYMBOL_REF:
      case LABEL_REF:
      case CODE_LABEL:
      case PC:
      case RETURN:
      case SIMPLE_RETURN:
      case SCRATCH:
         /* SCRATCH must be shared because they represent distinct values.  */
         return;
      case CLOBBER:
         /* Share clobbers of hard registers, but do not share pseudo reg
         clobbers or clobbers of hard registers that originated as pseudos.
         This is needed to allow safe register renaming.  */
         if (REG_P (XEXP (x, 0))
         && mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,REGNO (XEXP (x, 0)))
         && mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,ORIGINAL_REGNO (XEXP (x, 0))))
            return;
         break;
      case CONST:
         if (shared_const_p (orig))
            return;
         break;
      case MEM:
         /* A MEM is allowed to be shared if its address is constant.  */
         if (mtcs_rtl_constant_address_p/*!CONSTANT_ADDRESS_P*/(mtcsRTL,XEXP (x, 0))
         || reload_completed || reload_in_progress)
            return;
         break;
      default:
         break;
   }

   /* This rtx may not be shared.  If it has already been seen,
   replace it with a copy of itself.  */
   if (flag_checking && RTX_FLAG (x, used)){
      n_debug("mtcsemit.c verify_rtx_sharing 00 \n");
      mtcs_print_rtl(stderr,x);
      error ("invalid rtl sharing found in the insn");
      debug_rtx (insn);
      error ("shared rtx");
      debug_rtx (x);
      internal_error ("internal consistency failure");
   }
   gcc_assert (!RTX_FLAG (x, used));
   RTX_FLAG (x, used) = 1;
   /* Now scan the subexpressions recursively.  */
   format_ptr = GET_RTX_FORMAT (code);

   for (i = 0; i < GET_RTX_LENGTH (code); i++) {
      switch (*format_ptr++){
         case 'e':
            verify_rtx_sharing(self,XEXP (x, i), insn);
            break;

         case 'E':
            if (XVEC (x, i) != NULL){
               int j;
               int len = XVECLEN (x, i);

               for (j = 0; j < len; j++){
                  /* We allow sharing of ASM_OPERANDS inside single
                  instruction.  */
                  if (j && GET_CODE (XVECEXP (x, i, j)) == SET
                  && (GET_CODE (SET_SRC (XVECEXP (x, i, j))) == ASM_OPERANDS))
                     verify_rtx_sharing(self,SET_DEST (XVECEXP (x, i, j)), insn);
                  else
                     verify_rtx_sharing(self,XVECEXP (x, i, j), insn);
               }
            }
            break;
      }
   }
}


/* Verify sharing in INSN.  */
//原型 verify_insn_sharing emit-rtl.cc
static void verify_insn_sharing (MtcsEmit *self,rtx insn)
{
  gcc_assert (INSN_P (insn));
  verify_rtx_sharing(self,PATTERN (insn), insn);
  verify_rtx_sharing(self,REG_NOTES (insn), insn);
  if (CALL_P (insn))
    verify_rtx_sharing(self,CALL_INSN_FUNCTION_USAGE (insn), insn);
}

/* Go through all the RTL insn bodies and check that there is no unexpected
   sharing in between the subexpressions.  */
//原型 verify_rtl_sharing rtl.h emit-rtl.cc
void mtcs_emit_verify_rtl_sharing (MtcsEmit *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   rtx_insn *p;
   reset_all_used_flags(self);
   for (p = mtcs_rtl_data_get_insns/*!get_insns*/ (mtcsRtlData); p; p = NEXT_INSN (p))
      if (INSN_P (p)){
         rtx pat = PATTERN (p);
         if (GET_CODE (pat) != SEQUENCE)
            verify_insn_sharing(self,p);
         else
            for (int i = 0; i < XVECLEN (pat, 0); i++){
               rtx insn = XVECEXP (pat, 0, i);
               if (INSN_P (insn))
                  verify_insn_sharing(self,insn);
            }
      }

   reset_all_used_flags(self);
}

/* Emit the label LABEL after the insn AFTER.  */
//原型 emit_label_after rtl.h emit-rtl.cc
rtx_insn * mtcs_emit_emit_label_after (MtcsEmit *self,rtx_insn *label, rtx_insn *after)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   n_debug("mtcsemit.c mtcs_emit_emit_label_after 00 label:%p after:%p\n",label,after);
   gcc_checking_assert (INSN_UID (label) == 0);
   INSN_UID (label) = mtcsRtlData->emit.x_cur_insn_uid/*!cur_insn_uid*/++;
   mtcs_emit_add_insn_after/*!add_insn_after*/(self,label, after, NULL);
   return label;
}

/* Increment the label uses for all labels present in rtx.  */
//原型 mark_label_nuses emit-rtl.cc
static void mark_label_nuses (rtx x)
{
   enum rtx_code code;
   int i, j;
   const char *fmt;

   code = GET_CODE (x);
   if (code == LABEL_REF && LABEL_P (label_ref_label (x)))
      LABEL_NUSES (label_ref_label (x))++;

   fmt = GET_RTX_FORMAT (code);
   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--){
      if (fmt[i] == 'e')
         mark_label_nuses (XEXP (x, i));
      else if (fmt[i] == 'E')
         for (j = XVECLEN (x, i) - 1; j >= 0; j--)
            mark_label_nuses (XVECEXP (x, i, j));
   }
}

/* Find a RTX_AUTOINC class rtx which matches DATA.  */

static int find_auto_inc (const_rtx x, const_rtx reg)
{
   subrtx_iterator::array_type array;
   FOR_EACH_SUBRTX (iter, array, x, NONCONST){
      const_rtx x = *iter;
      if (GET_RTX_CLASS (GET_CODE (x)) == RTX_AUTOINC  && rtx_equal_p (reg, XEXP (x, 0)))
         return true;
   }
   return false;
}

/* Try splitting insns that can be split for better scheduling.
   PAT is the pattern which might split.
   TRIAL is the insn providing PAT.
   LAST is nonzero if we should return the last insn of the sequence produced.

   If this routine succeeds in splitting, it returns the first or last
   replacement insn depending on the value of LAST.  Otherwise, it
   returns TRIAL.  If the insn to be returned can be split, it will be.  */
//原型 try_split rtl.h emit-rtl.cc
rtx_insn * mtcs_emit_try_split (MtcsEmit *self,rtx pat, rtx_insn *trial, int last)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsRecog   *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsExcept   *mtcsExcept=mtcs_target_get_except(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);

   rtx_insn *before, *after;
   rtx note;
   rtx_insn *seq, *tem;
   profile_probability probability;
   rtx_insn *insn_last, *insn;
   int njumps = 0;
   rtx_insn *call_insn = NULL;

   if (any_condjump_p (trial) && (note = find_reg_note (trial, REG_BR_PROB, 0)))
      split_branch_probability = profile_probability::from_reg_br_prob_note (XINT (note, 0));
   else
      split_branch_probability = profile_probability::uninitialized ();

   probability = split_branch_probability;
   seq = mtcs_recog_split_insns/*!split_insns*/(mtcsRecog,pat, trial);
   split_branch_probability = profile_probability::uninitialized ();

   if (!seq)
      return trial;

   int split_insn_count = 0;
   /* Avoid infinite loop if any insn of the result matches
   the original pattern.  */
   insn_last = seq;
   while (1){
      if (INSN_P (insn_last)  && rtx_equal_p (PATTERN (insn_last), pat))
         return trial;
      split_insn_count++;
      if (!NEXT_INSN (insn_last))
         break;
      insn_last = NEXT_INSN (insn_last);
   }

   /* We're not good at redistributing frame information if
   the split occurs before reload or if it results in more
   than one insn.  */
   if (RTX_FRAME_RELATED_P (trial)){
      if (!reload_completed || split_insn_count != 1)
         return trial;

      rtx_insn *new_insn = seq;
      rtx_insn *old_insn = trial;
      mtcs_recog_copy_frame_info_to_split_insn/*!copy_frame_info_to_split_insn*/(mtcsRecog,old_insn, new_insn);
   }

   /* We will be adding the new sequence to the function.  The splitters
   may have introduced invalid RTL sharing, so unshare the sequence now.  */
   mtcs_rtl_unshare_all_rtl_in_chain/*!unshare_all_rtl_in_chain*/(mtcsRTL,seq);

   /* Mark labels and copy flags.  */
   for (insn = insn_last; insn ; insn = PREV_INSN (insn)){
      if (JUMP_P (insn)){
         if (JUMP_P (trial))
            CROSSING_JUMP_P (insn) = CROSSING_JUMP_P (trial);
         mtcs_dojump_mark_jump_label/*!mark_jump_label*/(mtcsDojump,PATTERN (insn), insn, 0);
         njumps++;
         if (probability.initialized_p () && any_condjump_p (insn) && !find_reg_note (insn, REG_BR_PROB, 0)){
            /* We can preserve the REG_BR_PROB notes only if exactly
            one jump is created, otherwise the machine description
            is responsible for this step using
            split_branch_probability variable.  */
            gcc_assert (njumps == 1);
            add_reg_br_prob_note (insn, probability);
         }
      }
   }

   /* If we are splitting a CALL_INSN, look for the CALL_INSN
   in SEQ and copy any additional information across.  */
   if (CALL_P (trial)){
      for (insn = insn_last; insn ; insn = PREV_INSN (insn))
         if (CALL_P (insn)){
            gcc_assert (call_insn == NULL_RTX);
            call_insn = insn;

            /* Add the old CALL_INSN_FUNCTION_USAGE to whatever the
            target may have explicitly specified.  */
            rtx *p = &CALL_INSN_FUNCTION_USAGE (insn);
            while (*p)
               p = &XEXP (*p, 1);
            *p = CALL_INSN_FUNCTION_USAGE (trial);

            /* If the old call was a sibling call, the new one must
            be too.  */
            SIBLING_CALL_P (insn) = SIBLING_CALL_P (trial);
         }
   }

   /* Copy notes, particularly those related to the CFG.  */
   for (note = REG_NOTES (trial); note; note = XEXP (note, 1)){
      switch (REG_NOTE_KIND (note)){
         case REG_EH_REGION:
            mtcs_except_copy_reg_eh_region_note_backward/*!copy_reg_eh_region_note_backward*/(mtcsExcept,note, insn_last, NULL);
            break;

         case REG_NORETURN:
         case REG_SETJMP:
         case REG_TM:
         case REG_CALL_NOCF_CHECK:
         case REG_CALL_ARG_LOCATION:
            for (insn = insn_last; insn != NULL_RTX; insn = PREV_INSN (insn)){
               if (CALL_P (insn))
                  add_reg_note (insn, REG_NOTE_KIND (note), XEXP (note, 0));
            }
            break;

         case REG_NON_LOCAL_GOTO:
         case REG_LABEL_TARGET:
            for (insn = insn_last; insn != NULL_RTX; insn = PREV_INSN (insn)){
               if (JUMP_P (insn))
                  add_reg_note (insn, REG_NOTE_KIND (note), XEXP (note, 0));
            }
            break;

         case REG_INC:
            if (!AUTO_INC_DEC)
               break;

            for (insn = insn_last; insn != NULL_RTX; insn = PREV_INSN (insn)){
               rtx reg = XEXP (note, 0);
               if (!FIND_REG_INC_NOTE (insn, reg)  && find_auto_inc (PATTERN (insn), reg))
                  add_reg_note (insn, REG_INC, reg);
            }
            break;

         case REG_ARGS_SIZE:
            mtcs_expr_fixup_args_size_notes/*!fixup_args_size_notes*/(mtcsExpr,NULL, insn_last, get_args_size (note));
            break;

         case REG_CALL_DECL:
         case REG_UNTYPED_CALL:
            gcc_assert (call_insn != NULL_RTX);
            add_reg_note (call_insn, REG_NOTE_KIND (note), XEXP (note, 0));
            break;

         default:
            break;
      }
   }

   /* If there are LABELS inside the split insns increment the
   usage count so we don't delete the label.  */
   if (INSN_P (trial)){
      insn = insn_last;
      while (insn != NULL_RTX){
         /* JUMP_P insns have already been "marked" above.  */
         if (NONJUMP_INSN_P (insn))
            mark_label_nuses (PATTERN (insn));
         insn = PREV_INSN (insn);
      }
   }

   before = PREV_INSN (trial);
   after = NEXT_INSN (trial);

   mtcs_emit_emit_insn_after_setloc/*!emit_insn_after_setloc*/(self,seq, trial, INSN_LOCATION (trial));

   mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,trial);

   /* Recursively call try_split for each new insn created; by the
   time control returns here that insn will be fully split, so
   set LAST and continue from the insn after the one returned.
   We can't use next_active_insn here since AFTER may be a note.
   Ignore deleted insns, which can be occur if not optimizing.  */
   for (tem = NEXT_INSN (before); tem != after; tem = NEXT_INSN (tem))
      if (! tem->deleted () && INSN_P (tem))
         tem = mtcs_emit_try_split/*!try_split*/(self,PATTERN (tem), tem, 1);

   /* Return either the first or the last insn, depending on which was
   requested.  */
   return last ? (after ? PREV_INSN (after) : mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData))
         : NEXT_INSN (before);
}

/* Make X be output after the insn AFTER and set the BB of insn.  If
   BB is NULL, an attempt is made to infer the BB from AFTER.  */
//原型 emit_insn_after_noloc rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_insn_after_noloc (MtcsEmit *self,rtx x, rtx_insn *after, basic_block bb)
{
  return emit_pattern_after_noloc(self,x, after, bb, makeInsnRaw_cb,(void *)self);
}

/* Like emit_insn_after_noloc, but set INSN_LOCATION according to LOC.  */
//原型 emit_insn_after_setloc rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_insn_after_setloc (MtcsEmit *self,rtx pattern, rtx_insn *after, location_t loc)
{
  return emit_pattern_after_setloc(self,pattern, after, loc, makeInsnRaw_cb,(void *)self);
}

/* Return a copy of INSN that can be used in a SEQUENCE delay slot,
   on that assumption that INSN itself remains in its original place.  */
//原型 copy_delay_slot_insn emit-rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_copy_delay_slot_insn (MtcsEmit *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   /* Copy INSN with its rtx_code, all its notes, location etc.  */
   insn = as_a <rtx_insn *> (copy_rtx (insn));
   INSN_UID (insn) =  mtcsRtlData->emit.x_cur_insn_uid/*!cur_insn_uid*/++;
   return insn;
}

/* Produce exact duplicate of insn INSN after AFTER.
   Care updating of libcall regions if present.  */
//原型 emit_copy_of_insn_after emit-rtl.h emit-rtl.cc
rtx_insn * mtcs_emit_emit_copy_of_insn_after (MtcsEmit *self,rtx_insn *insn, rtx_insn *after)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);

   rtx_insn *new_rtx;
   rtx link;

   switch (GET_CODE (insn)){
      case INSN:
         new_rtx =mtcs_emit_emit_insn_after/*!emit_insn_after*/(self,copy_insn (PATTERN (insn)), after);
         break;

      case JUMP_INSN:
         new_rtx = mtcs_emit_emit_jump_insn_after/*!emit_jump_insn_after*/(self,copy_insn (PATTERN (insn)), after);
         CROSSING_JUMP_P (new_rtx) = CROSSING_JUMP_P (insn);
         break;

      case DEBUG_INSN:
         new_rtx = mtcs_emit_emit_debug_insn_after/*!emit_debug_insn_after*/(self,copy_insn (PATTERN (insn)), after);
         break;

      case CALL_INSN:
         new_rtx =mtcs_emit_emit_call_insn_after/*!emit_call_insn_after*/(self,copy_insn (PATTERN (insn)), after);
         if (CALL_INSN_FUNCTION_USAGE (insn))
            CALL_INSN_FUNCTION_USAGE (new_rtx)  = copy_insn (CALL_INSN_FUNCTION_USAGE (insn));
         SIBLING_CALL_P (new_rtx) = SIBLING_CALL_P (insn);
         RTL_CONST_CALL_P (new_rtx) = RTL_CONST_CALL_P (insn);
         RTL_PURE_CALL_P (new_rtx) = RTL_PURE_CALL_P (insn);
         RTL_LOOPING_CONST_OR_PURE_CALL_P (new_rtx) = RTL_LOOPING_CONST_OR_PURE_CALL_P (insn);
         break;

      default:
         gcc_unreachable ();
   }

   /* Update LABEL_NUSES.  */
   if (NONDEBUG_INSN_P (insn))
      mtcs_dojump_mark_jump_label/*!mark_jump_label*/(mtcsDojump,PATTERN (new_rtx), new_rtx, 0);

   INSN_LOCATION (new_rtx) = INSN_LOCATION (insn);

   /* If the old insn is frame related, then so is the new one.  This is
   primarily needed for IA-64 unwind info which marks epilogue insns,
   which may be duplicated by the basic block reordering code.  */
   RTX_FRAME_RELATED_P (new_rtx) = RTX_FRAME_RELATED_P (insn);

   /* Locate the end of existing REG_NOTES in NEW_RTX.  */
   rtx *ptail = &REG_NOTES (new_rtx);
   while (*ptail != NULL_RTX)
      ptail = &XEXP (*ptail, 1);

   /* Copy all REG_NOTES except REG_LABEL_OPERAND since mark_jump_label
   will make them.  REG_LABEL_TARGETs are created there too, but are
   supposed to be sticky, so we copy them.  */
   for (link = REG_NOTES (insn); link; link = XEXP (link, 1))
      if (REG_NOTE_KIND (link) != REG_LABEL_OPERAND){
         *ptail = duplicate_reg_note (link);
         ptail = &XEXP (*ptail, 1);
      }

   INSN_CODE (new_rtx) = INSN_CODE (insn);
   return new_rtx;
}

/* Make X be output before the instruction BEFORE.  */
//原型 emit_insn_before_noloc rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_emit_insn_before_noloc (MtcsEmit *self,rtx x, rtx_insn *before, basic_block bb)
{
  return emit_pattern_before_noloc(self,x, before, before, bb, makeInsnRaw_cb,(void*)self);
}

/* Like emit_insn_before_noloc, but set INSN_LOCATION according to LOC.  */
//原型 emit_insn_before_setloc rtl.h emit-rtl.cc
rtx_insn * mtcs_emit_emit_insn_before_setloc (MtcsEmit *self,rtx pattern, rtx_insn *before, location_t loc)
{
  return emit_pattern_before_setloc (self,pattern, before, loc, true,makeInsnRaw_cb,(void*)self);
}

/* Make and return an INSN rtx, initializing all its slots.
   Store PATTERN in the pattern slots.  */
//原型 make_insn_raw rtl.h emit-rtl.cc
rtx_insn *mtcs_emit_make_insn_raw (MtcsEmit *self,rtx pattern)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  rtx_insn *insn;

  insn = as_a <rtx_insn *> (rtx_alloc (INSN));

  INSN_UID (insn) = mtcsRtlData->emit.x_cur_insn_uid/*!cur_insn_uid*/++;
  PATTERN (insn) = pattern;
  INSN_CODE (insn) = -1;
  REG_NOTES (insn) = NULL;
  INSN_LOCATION (insn) = mtcs_emit_curr_insn_location/*!curr_insn_location*/(self);
  BLOCK_FOR_INSN (insn) = NULL;

#ifdef ENABLE_RTL_CHECKING
  if (insn
      && INSN_P (insn)
      && (returnjump_p (insn)
     || (GET_CODE (insn) == SET
         && SET_DEST (insn) == pc_rtx)))
    {
      warning (0, "ICE: %<emit_insn%> used where %<emit_jump_insn%> needed:");
      mtcs_debug_rtx (insn);
    }
#endif

  return insn;
}

/* Like emit_jump_insn_after_noloc, but set INSN_LOCATION according to LOC.  */
//原型 emit_jump_insn_after_setloc rtl.h emit-rtl.cc
rtx_jump_insn * mtcs_emit_emit_jump_insn_after_setloc (MtcsEmit *self,rtx pattern, rtx_insn *after, location_t loc)
{
  return as_a <rtx_jump_insn *> (
   emit_pattern_after_setloc(self,pattern, after, loc, make_jump_insn_raw,(void*)self));
}


/* Make an insn of code JUMP_INSN with body X
   and output it after the insn AFTER.  */
//原型 emit_jump_insn_after_noloc rtl.h emit-rtl.cc
rtx_jump_insn * mtcs_emit_emit_jump_insn_after_noloc (MtcsEmit *self,rtx x, rtx_insn *after)
{
  return as_a <rtx_jump_insn *> (
      emit_pattern_after_noloc(self,x, after, NULL, make_jump_insn_raw,(void*)self));
}

/* Like emit_jump_insn_before_noloc, but set INSN_LOCATION according to BEFORE.  */
//原型 emit_jump_insn_before rtl.h emit-rtl.cc
rtx_jump_insn *mtcs_emit_emit_jump_insn_before (MtcsEmit *self,rtx pattern, rtx_insn *before)
{
  return as_a <rtx_jump_insn *> (
   emit_pattern_before (self,pattern, before, true, false,make_jump_insn_raw,(void*)self));
}

/* Allocate insn location datastructure.  */
//原型 insn_locations_init rtl.h emit-rtl.cc
void mtcs_emit_insn_locations_init (MtcsEmit *self)
{
   self->prologue_location = self->epilogue_location = 0;
   self->curr_location = UNKNOWN_LOCATION;
}

/* Set current location.  */
//原型 set_curr_insn_location rtl.h emit-rtl.cc
void mtcs_emit_set_curr_insn_location (MtcsEmit *self,location_t location)
{
   self->curr_location = location;
}

/* Get current location.  */
//原型 curr_insn_location rtl.h emit-rtl.cc
location_t mtcs_emit_curr_insn_location (MtcsEmit *self)
{
  return self->curr_location;
}

void mtcs_emit_set_prologue_location(MtcsEmit *self,location_t location)
{
    self->prologue_location = location;
}

/* At the end of emit stage, clear current location.  */
//原型 insn_locations_finalize rtl.h emit-rtl.cc
void mtcs_emit_insn_locations_finalize (MtcsEmit *self)
{
  self->epilogue_location =self-> curr_location;
  self->curr_location = UNKNOWN_LOCATION;
}

//原型 #define BRANCH_COST(speed_p, predictable_p) 1 分支消耗的指令数
int mtcs_emit_branch_cost(MtcsEmit *self,bool speed_p ,bool predictable_p)
{
    return self->brach_cost(self,speed_p,predictable_p);
}

