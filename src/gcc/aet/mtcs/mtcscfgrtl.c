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
 * base on cfgrtl.cc
 */

/* This file contains low level functions to manipulate the CFG and analyze it
   that are aware of the RTL intermediate language.

   Available functionality:
     - Basic CFG/RTL manipulation API documented in cfghooks.h
     - CFG-aware instruction chain manipulation
     delete_insn, delete_insn_chain
     - Edge splitting and committing to edges
     insert_insn_on_edge, prepend_insn_to_edge, commit_edge_insertions
     - CFG updating after insn simplification
     purge_dead_edges, purge_all_dead_edges
     - CFG fixing after coarse manipulation
    fixup_abnormal_edges

   Functions not supposed for generic use:
     - Infrastructure to determine quickly basic block for insn
     compute_bb_for_insn, update_bb_for_insn, set_block_for_insn,
     - Edge redirection with updating and optimizing of insn chain
     block_label, tidy_fallthru_edge, force_nonfallthru  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "cfghooks.h"
#include "df.h"
#include "insn-config.h"
#include "memmodel.h"
#include "emit-rtl.h"
#include "cfgrtl.h"
#include "cfganal.h"
#include "cfgbuild.h"
#include "cfgcleanup.h"
#include "bb-reorder.h"
#include "rtl-error.h"
#include "insn-attr.h"
#include "dojump.h"
#include "expr.h"
#include "cfgloop.h"
#include "tree-pass.h"
#include "print-rtl.h"
#include "rtl-iter.h"
#include "gimplify.h"
#include "profile.h"
#include "sreal.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "gimple-expr.h"
#include "gimple-walk.h"
#include "gimple-pretty-print.h"

#include "aet/aetprintgimple.h"
#include "mtcscfgrtl.h"
#include "mtcstarget.h"
#include "mtcscompile.h"
#include "mtcsdfcore.h"
#include "mtcsdfproblems.h"
#include "mtcsprintrtl.h"


static rtx_insn *skip_insns_after_block (MtcsCfgRtl *self,basic_block);
static void record_effective_endpoints (MtcsCfgRtl *self);
static void fixup_reorder_chain (MtcsCfgRtl *self);

static void verify_insn_chain (MtcsCfgRtl *self);
static void fixup_fallthru_exit_predecessor (MtcsCfgRtl *self);
static bool can_delete_note_p (MtcsCfgRtl *self,const rtx_note *);
static bool can_delete_label_p (MtcsCfgRtl *self,const rtx_code_label *);

/* Return true if NOTE is not one of the ones that must be kept paired,
   so that we may simply delete it.  */

static bool can_delete_note_p (MtcsCfgRtl *self,const rtx_note *note)
{
    switch (NOTE_KIND (note)){
        case NOTE_INSN_DELETED:
        case NOTE_INSN_BASIC_BLOCK:
        case NOTE_INSN_EPILOGUE_BEG:
            return true;
        default:
        return false;
    }
}

/* True if a given label can be deleted.  */

static bool can_delete_label_p (MtcsCfgRtl *self,const rtx_code_label *label)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  return (!LABEL_PRESERVE_P (label)
      /* User declared labels must be preserved.  */
      && LABEL_NAME (label) == 0
      && !vec_safe_contains<rtx_insn *> (mtcsRtlData/*!crtl*/->expr.x_forced_labels/*!forced_labels*/,
                         const_cast<rtx_code_label *> (label)));
}

/* Delete INSN by patching it out.  */
//原型 delete_insn  cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_delete_insn (MtcsCfgRtl *self,rtx_insn *insn)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  rtx note;
  bool really_delete = true;
  if (LABEL_P (insn)){
      /* Some labels can't be directly removed from the INSN chain, as they
     might be references via variables, constant pool etc.
     Convert them to the special NOTE_INSN_DELETED_LABEL note.  */
      if (! can_delete_label_p(self,as_a <rtx_code_label *> (insn))){
          const char *name = LABEL_NAME (insn);
          basic_block bb = BLOCK_FOR_INSN (insn);
          rtx_insn *bb_note = NEXT_INSN (insn);
          n_debug("mtcscfgrtl.c delete_insn 00 %p bb:%p name:%s\n",insn,bb,name);

          really_delete = false;
          PUT_CODE (insn, NOTE);
          NOTE_KIND (insn) = NOTE_INSN_DELETED_LABEL;
          NOTE_DELETED_LABEL_NAME (insn) = name;
          /* If the note following the label starts a basic block, and the
             label is a member of the same basic block, interchange the two.  */
          if (bb_note != NULL_RTX
              && NOTE_INSN_BASIC_BLOCK_P (bb_note)
              && bb != NULL
              && bb == BLOCK_FOR_INSN (bb_note)){
             mtcs_rtl_reorder_insns_nobb/*!reorder_insns_nobb*/(mtcsRTL,insn, insn, bb_note);
              BB_HEAD (bb) = bb_note;
              if (BB_END (bb) == bb_note)
                  BB_END (bb) = insn;
          }
      }
      remove_node_from_insn_list (insn, &mtcsRtlData/*!nonlocal_goto_handler_labels*/->x_nonlocal_goto_handler_labels);
  }
  if (really_delete){
      /* If this insn has already been deleted, something is very wrong.  */
      gcc_assert (!insn->deleted ());
      n_debug("mtcscfgrtl.c delete_insn 11 %p bb:%p,INSN_P (insn):%d\n",insn,BLOCK_FOR_INSN (insn),INSN_P (insn));

      if (INSN_P (insn))
         mtcs_dfscan_df_insn_delete/*!df_insn_delete*/(mtcsDfscan,insn);
      n_debug("mtcscfgrtl.c delete_insn 22 %p bb:%p\n",insn,BLOCK_FOR_INSN (insn));
      mtcs_print_rtl_single(stderr,insn);
      mtcs_emit_remove_insn/*!remove_insn*/(mtcsEmit,insn);
      insn->set_deleted ();
  }
  /* If deleting a jump, decrement the use count of the label.  Deleting
     the label itself should happen in the normal course of block merging.  */
  if (JUMP_P (insn)){
      if (JUMP_LABEL (insn)  && LABEL_P (JUMP_LABEL (insn)))
          LABEL_NUSES (JUMP_LABEL (insn))--;

      /* If there are more targets, remove them too.  */
      while ((note = find_reg_note (insn, REG_LABEL_TARGET, NULL_RTX)) != NULL_RTX
         && LABEL_P (XEXP (note, 0))){
          LABEL_NUSES (XEXP (note, 0))--;
          mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,insn, note);
      }
  }
  /* Also if deleting any insn that references a label as an operand.  */
  while ((note = find_reg_note (insn, REG_LABEL_OPERAND, NULL_RTX)) != NULL_RTX
     && LABEL_P (XEXP (note, 0))){
      LABEL_NUSES (XEXP (note, 0))--;
      mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,insn, note);
  }

  if (rtx_jump_table_data *table = dyn_cast <rtx_jump_table_data *> (insn)){
      rtvec vec = table->get_labels ();
      int len = GET_NUM_ELEM (vec);
      int i;

      for (i = 0; i < len; i++){
          rtx label = XEXP (RTVEC_ELT (vec, i), 0);
          /* When deleting code in bulk (e.g. removing many unreachable
             blocks) we can delete a label that's a target of the vector
             before deleting the vector itself.  */
          if (!NOTE_P (label))
            LABEL_NUSES (label)--;
      }
  }
}

/* Like delete_insn but also purge dead edges from BB.
   Return true if any edges are eliminated.  */
//原型 delete_insn_and_edges cfgrtl.h cfgrtl.cc
bool mtcs_cfg_rtl_delete_insn_and_edges (MtcsCfgRtl *self,rtx_insn *insn)
{
    bool purge = false;
    if (NONDEBUG_INSN_P (insn) && BLOCK_FOR_INSN (insn)){
        basic_block bb = BLOCK_FOR_INSN (insn);
        if (BB_END (bb) == insn)
            purge = true;
        else if (DEBUG_INSN_P (BB_END (bb)))
            for (rtx_insn *dinsn = NEXT_INSN (insn); DEBUG_INSN_P (dinsn); dinsn = NEXT_INSN (dinsn))
            if (BB_END (bb) == dinsn){
                purge = true;
                break;
            }
    }
    mtcs_cfg_rtl_delete_insn/*!delete_insn*/(self,insn);
    if (purge)
        return mtcs_cfg_rtl_purge_dead_edges/*!purge_dead_edges*/(self,BLOCK_FOR_INSN (insn));
    return false;
}

/* Unlink a chain of insns between START and FINISH, leaving notes
   that must be paired.  If CLEAR_BB is true, we set bb field for
   insns that cannot be removed to NULL.  */
//原型 delete_insn_chain cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_delete_insn_chain (MtcsCfgRtl *self,rtx start, rtx_insn *finish, bool clear_bb)
{
  /* Unchain the insns one by one.  It would be quicker to delete all of these
     with a single unchaining, rather than one at a time, but we need to keep
     the NOTE's.  */
  rtx_insn *current = finish;
  while (1){
      rtx_insn *prev = PREV_INSN (current);
      if (NOTE_P (current) && !can_delete_note_p(self,as_a <rtx_note *> (current)))
          ;
      else
          mtcs_cfg_rtl_delete_insn/*!delete_insn*/(self,current);
      if (clear_bb && !current->deleted ())
          set_block_for_insn (current, NULL);
      if (current == start)
          break;
      current = prev;
  }
}

/* Create a new basic block consisting of the instructions between HEAD and END
   inclusive.  This function is designed to allow fast BB construction - reuses
   the note and basic block struct in BB_NOTE, if any and do not grow
   BASIC_BLOCK chain and should be used directly only by CFG construction code.
   END can be NULL in to create new empty basic block before HEAD.  Both END
   and HEAD can be NULL to create basic block at the end of INSN chain.
   AFTER is the basic block we should be put after.  */
//原型 create_basic_block_structure cfgrtl.h cfgrtl.cc
basic_block mtcs_cfg_rtl_create_basic_block_structure (MtcsCfgRtl *self,rtx_insn *head, rtx_insn *end,
        rtx_note *bb_note,basic_block after)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);

  basic_block bb;
  if (bb_note  && (bb = NOTE_BASIC_BLOCK (bb_note)) != NULL && bb->aux == NULL){
      /* If we found an existing note, thread it back onto the chain.  */
     n_debug("mtcscfgrtl.c create_basic_block_structure 00\n");

      rtx_insn *after;
      if (LABEL_P (head))
          after = head;
      else{
          after = PREV_INSN (head);
          head = bb_note;
      }

      if (after != bb_note && NEXT_INSN (after) != bb_note)
         mtcs_rtl_reorder_insns_nobb/*!reorder_insns_nobb*/(mtcsRTL,bb_note, bb_note, after);
  }else{
      /* Otherwise we must create a note and a basic block structure.  */
     n_debug("mtcscfgrtl.c create_basic_block_structure 11 head:%p end:%p\n",head,end);

      bb = alloc_block ();
      mtcs_cfg_rtl_init_rtl_bb_info/*!init_rtl_bb_info*/(self,bb);
      if (!head && !end){

         rtx_insn *rt=mtcs_rtl_data_get_last_insn (mtcsRtlData);
         n_debug("mtcscfgrtl.c create_basic_block_structure 11aa last rtx:%p\n",rt);
         mtcs_print_rtl_single(stderr,rt);

          head = end = bb_note   = mtcs_emit_emit_note_after/*!emit_note_after*/(mtcsEmit,NOTE_INSN_BASIC_BLOCK,
                  mtcs_rtl_data_get_last_insn/*!get_last_insn*/ (mtcsRtlData));
      }else if (LABEL_P (head) && end){
         n_debug("mtcscfgrtl.c create_basic_block_structure 22 head:%p end:%p\n",head,end);

          bb_note = mtcs_emit_emit_note_after/*!emit_note_after*/(mtcsEmit,NOTE_INSN_BASIC_BLOCK, head);
          if (head == end)
            end = bb_note;
      }else{
         n_debug("mtcscfgrtl.c create_basic_block_structure 33 head:%p end:%p\n",head,end);

          bb_note = mtcs_emit_emit_note_before/*!emit_note_before */(mtcsEmit,NOTE_INSN_BASIC_BLOCK, head);
          head = bb_note;
          if (!end)
            end = head;
      }
      NOTE_BASIC_BLOCK (bb_note) = bb;
  }
  /* Always include the bb note in the block.  */
  if (NEXT_INSN (end) == bb_note)
    end = bb_note;
  n_debug("mtcscfgrtl.c create_basic_block_structure 44\n");

  BB_HEAD (bb) = head;
  BB_END (bb) = end;
  bb->index = last_basic_block_for_fn (cfun)++;
  bb->flags = BB_NEW | BB_RTL;
  link_block (bb, after);
  SET_BASIC_BLOCK_FOR_FN (cfun, bb->index, bb);
  mtcs_dfscan_df_bb_refs_record/*!df_bb_refs_record*/(mtcsDfscan,bb->index, false);
  mtcs_cfg_rtl_update_bb_for_insn/*!update_bb_for_insn*/(self,bb);
  BB_SET_PARTITION (bb, BB_UNPARTITIONED);
  /* Tag the block so that we know it has been used when considering
     other basic block notes.  */
  n_debug("mtcscfgrtl.c create_basic_block_structure 55 bb:%p\n",bb);
   rtx_insn *insn;
   int i=0;
   FOR_BB_INSNS (bb, insn){
      if (!INSN_P (insn))
         continue;
      n_debug("mtcscfgrtl.c create_basic_block_structure  打印块中的指令 i:%d block:%p index:%d flags:%d insn:%p\n",
            i++,bb,bb->index,bb->flags,insn);
      mtcs_print_rtl_single(stderr,insn);
   }

  bb->aux = bb;
  return bb;
}

/* Create new basic block consisting of instructions in between HEAD and END
   and place it to the BB chain after block AFTER.  END can be NULL to
   create a new empty basic block before HEAD.  Both END and HEAD can be
   NULL to create basic block at the end of INSN chain.  */
//原型 rtl_create_basic_block cfgrtl.cc
static basic_block rtl_create_basic_block (MtcsCfgRtl *self,void *headp, void *endp, basic_block after)
{
  rtx_insn *head = (rtx_insn *) headp;
  rtx_insn *end = (rtx_insn *) endp;
  basic_block bb;
  /* Grow the basic block array if needed.  */
  if ((size_t) last_basic_block_for_fn (cfun)
      >= basic_block_info_for_fn (cfun)->length ())
    vec_safe_grow_cleared (basic_block_info_for_fn (cfun), last_basic_block_for_fn (cfun) + 1);

  n_basic_blocks_for_fn (cfun)++;

  bb = mtcs_cfg_rtl_create_basic_block_structure/*!create_basic_block_structure*/(self,head, end, NULL, after);
  bb->aux = NULL;
  return bb;
}

static basic_block cfg_layout_create_basic_block (MtcsCfgRtl *self,void *head, void *end, basic_block after)
{
  basic_block newbb = rtl_create_basic_block (self,head, end, after);
  return newbb;
}

/* Records the basic block struct in BLOCK_FOR_INSN for every insn.  */
//原型 compute_bb_for_insn cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_compute_bb_for_insn (MtcsCfgRtl *self)
{
  basic_block bb;
  FOR_EACH_BB_FN (bb, cfun){
      rtx_insn *end = BB_END (bb);
      rtx_insn *insn;
      for (insn = BB_HEAD (bb); ; insn = NEXT_INSN (insn)){
          BLOCK_FOR_INSN (insn) = bb;
          if (insn == end)
            break;
      }
  }
}

/* Release the basic_block_for_insn array.  */
//原型 free_bb_for_insn cfgrl.h cfgrtl.cc
void mtcs_cfg_rtl_free_bb_for_insn (MtcsCfgRtl *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  rtx_insn *insn;
  for (insn =mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = NEXT_INSN (insn))
    if (!BARRIER_P (insn))
      BLOCK_FOR_INSN (insn) = NULL;
}

/* Determine which partition the first basic block in the function
   belongs to, then find the first basic block in the current function
   that belongs to a different section, and insert a
   NOTE_INSN_SWITCH_TEXT_SECTIONS note immediately before it in the
   instruction stream.  When writing out the assembly code,
   encountering this note will make the compiler switch between the
   hot and cold text sections.  */
//原型 insert_section_boundary_note bb-reorder.h bb-reorder.cc
void mtcs_cfg_rtl_insert_section_boundary_note (MtcsCfgRtl *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   basic_block bb;
   bool switched_sections = false;
   int current_partition = 0;

   if (!mtcsRtlData/*!crtl*/->has_bb_partition)
      return;

   FOR_EACH_BB_FN (bb, cfun){
      if (!current_partition)
         current_partition = BB_PARTITION (bb);
      if (BB_PARTITION (bb) != current_partition){
         gcc_assert (!switched_sections);
         switched_sections = true;
         mtcs_emit_emit_note_before/*!emit_note_before*/(mtcsEmit,NOTE_INSN_SWITCH_TEXT_SECTIONS, BB_HEAD (bb));
         current_partition = BB_PARTITION (bb);
      }
   }

   /* Make sure crtl->has_bb_partition matches reality even if bbpart finds
   some hot and some cold basic blocks, but later one of those kinds is
   optimized away.  */
   mtcsRtlData/*!crtl*/->has_bb_partition = switched_sections;
}

/* Return RTX to emit after when we want to emit code on the entry of function.  */
//原型 entry_of_function cfgrtl.h cfgrtl.cc
rtx_insn *mtcs_cfg_rtl_entry_of_function (MtcsCfgRtl *self)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
    MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
    return (n_basic_blocks_for_fn (cfun) > NUM_FIXED_BLOCKS ?
            BB_HEAD (ENTRY_BLOCK_PTR_FOR_FN (cfun)->next_bb) : mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData));
}

/* Emit INSN at the entry point of the function, ensuring that it is only
   executed once per function.  */
//原型 emit_insn_at_entry rtl.h cfgrtl.cc
void mtcs_cfg_rtl_emit_insn_at_entry (MtcsCfgRtl *self,rtx insn)
{
  edge_iterator ei = ei_start (ENTRY_BLOCK_PTR_FOR_FN (cfun)->succs);
  edge e = ei_safe_edge (ei);
  gcc_assert (e->flags & EDGE_FALLTHRU);

  mtcs_cfg_rtl_insert_insn_on_edge/*!insert_insn_on_edge*/(self,insn, e);
  mtcs_cfg_rtl_commit_edge_insertions/*!commit_edge_insertions*/(self);
}

/* Update BLOCK_FOR_INSN of insns between BEGIN and END
   (or BARRIER if found) and notify df of the bb change.
   The insn chain range is inclusive
   (i.e. both BEGIN and END will be updated. */

static void update_bb_for_insn_chain (MtcsCfgRtl *self,rtx_insn *begin, rtx_insn *end, basic_block bb)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);

   rtx_insn *insn;
   end = NEXT_INSN (end);
   for (insn = begin; insn != end; insn = NEXT_INSN (insn))
      if (!BARRIER_P (insn))
         mtcs_dfscan_df_insn_change_bb/*!df_insn_change_bb*/(mtcsDfscan,insn, bb);
}

/* Update BLOCK_FOR_INSN of insns in BB to BB,
   and notify df of the change.  */
//原型 update_bb_for_insn cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_update_bb_for_insn (MtcsCfgRtl *self,basic_block bb)
{
  update_bb_for_insn_chain (self,BB_HEAD (bb), BB_END (bb), bb);
}


/* Like active_insn_p, except keep the return value use or clobber around
   even after reload.  */

static bool flow_active_insn_p (MtcsCfgRtl *self,const rtx_insn *insn)
{
  if (active_insn_p (insn))
    return true;

  /* A clobber of the function return value exists for buggy
     programs that fail to return a value.  Its effect is to
     keep the return value from being live across the entire
     function.  If we allow it to be skipped, we introduce the
     possibility for register lifetime confusion.
     Similarly, keep a USE of the function return value, otherwise
     the USE is dropped and we could fail to thread jump if USE
     appears on some paths and not on others, see PR90257.  */
  if ((GET_CODE (PATTERN (insn)) == CLOBBER
       || GET_CODE (PATTERN (insn)) == USE)
      && REG_P (XEXP (PATTERN (insn), 0))
      && REG_FUNCTION_VALUE_P (XEXP (PATTERN (insn), 0)))
    return true;

  return false;
}

/* Return true if the block has no effect and only forwards control flow to
   its single destination.  */
//原型 contains_no_active_insn_p cfgrtl.h cfgrtl.cc
bool mtcs_cfg_rtl_contains_no_active_insn_p (MtcsCfgRtl *self, const_basic_block bb)
{
  rtx_insn *insn;

  if (bb == EXIT_BLOCK_PTR_FOR_FN (cfun)
      || bb == ENTRY_BLOCK_PTR_FOR_FN (cfun)
      || !single_succ_p (bb)
      || (single_succ_edge (bb)->flags & EDGE_FAKE) != 0)
    return false;

  for (insn = BB_HEAD (bb); insn != BB_END (bb); insn = NEXT_INSN (insn))
    if (INSN_P (insn) && flow_active_insn_p (self,insn))
      return false;

  return (!INSN_P (insn)
      || (JUMP_P (insn) && simplejump_p (insn))
      || !flow_active_insn_p (self,insn));
}

/* Likewise, but protect loop latches, headers and preheaders.  */
/* FIXME: Make this a cfg hook.  */
//原型 forwarder_block_p cfgrtl.h cfgrtl.cc
bool mtcs_cfg_rtl_forwarder_block_p (MtcsCfgRtl *self,const_basic_block bb)
{
  if (!mtcs_cfg_rtl_contains_no_active_insn_p/*!contains_no_active_insn_p*/(self,bb))
    return false;
  /* Protect loop latches, headers and preheaders.  */
  if (current_loops){
      basic_block dest;
      if (bb->loop_father->header == bb)
          return false;
      dest = EDGE_SUCC (bb, 0)->dest;
      if (dest->loop_father->header == dest)
          return false;
   }
   return true;
}

/* Return nonzero if we can reach target from src by falling through.  */
/* FIXME: Make this a cfg hook, the result is only valid in cfgrtl mode.  */
//原型 can_fallthru cfgrtl.h cfgrtl.cc
bool mtcs_cfg_rgl_can_fallthru (MtcsCfgRtl *self,basic_block src, basic_block target)
{
  rtx_insn *insn = BB_END (src);
  rtx_insn *insn2;
  edge e;
  edge_iterator ei;
  if (target == EXIT_BLOCK_PTR_FOR_FN (cfun))
    return true;
  if (src->next_bb != target)
    return false;
  /* ??? Later we may add code to move jump tables offline.  */
  if (tablejump_p (insn, NULL, NULL))
    return false;

  FOR_EACH_EDGE (e, ei, src->succs)
    if (e->dest == EXIT_BLOCK_PTR_FOR_FN (cfun)  && e->flags & EDGE_FALLTHRU)
      return false;

  insn2 = BB_HEAD (target);
  if (!active_insn_p (insn2))
    insn2 = next_active_insn (insn2);

  return next_active_insn (insn) == insn2;
}

/* Return nonzero if we could reach target from src by falling through,
   if the target was made adjacent.  If we already have a fall-through
   edge to the exit block, we can't do that.  */
static bool could_fall_through (MtcsCfgRtl *self,basic_block src, basic_block target)
{
  edge e;
  edge_iterator ei;
  if (target == EXIT_BLOCK_PTR_FOR_FN (cfun))
    return true;
  FOR_EACH_EDGE (e, ei, src->succs)
    if (e->dest == EXIT_BLOCK_PTR_FOR_FN (cfun)
    && e->flags & EDGE_FALLTHRU)
      return 0;
  return true;
}

/* Return the NOTE_INSN_BASIC_BLOCK of BB.  */
//原型 bb_note cfgrtl.h cfgrtl.cc
rtx_note * mtcs_cfg_rtl_bb_note (MtcsCfgRtl *self,basic_block bb)
{
  rtx_insn *note;
  note = BB_HEAD (bb);
  if (LABEL_P (note))
    note = NEXT_INSN (note);
  gcc_assert (NOTE_INSN_BASIC_BLOCK_P (note));
  return as_a <rtx_note *> (note);
}

/* Return the INSN immediately following the NOTE_INSN_BASIC_BLOCK
   note associated with the BLOCK.  */

static rtx_insn *first_insn_after_basic_block_note (MtcsCfgRtl *self,basic_block block)
{
  rtx_insn *insn;
  /* Get the first instruction in the block.  */
  insn = BB_HEAD (block);
  if (insn == NULL_RTX)
    return NULL;
  if (LABEL_P (insn))
    insn = NEXT_INSN (insn);
  gcc_assert (NOTE_INSN_BASIC_BLOCK_P (insn));
  return NEXT_INSN (insn);
}

/* Return true if LOC1 and LOC2 are equivalent for
   unique_locus_on_edge_between_p purposes.  */
static bool loc_equal (MtcsCfgRtl *self,location_t loc1, location_t loc2)
{
  if (loc1 == loc2)
    return true;

  expanded_location loce1 = expand_location (loc1);
  expanded_location loce2 = expand_location (loc2);

  if (loce1.line != loce2.line
      || loce1.column != loce2.column
      || loce1.data != loce2.data)
    return false;
  if (loce1.file == loce2.file)
    return true;
  return (loce1.file != NULL
      && loce2.file != NULL
      && filename_cmp (loce1.file, loce2.file) == 0);
}

/* Return true if the single edge between blocks A and B is the only place
   in RTL which holds some unique locus.  */

static bool unique_locus_on_edge_between_p (MtcsCfgRtl *self,basic_block a, basic_block b)
{
  const location_t goto_locus = EDGE_SUCC (a, 0)->goto_locus;
  rtx_insn *insn, *end;
  if (LOCATION_LOCUS (goto_locus) == UNKNOWN_LOCATION)
    return false;
  /* First scan block A backward.  */
  insn = BB_END (a);
  end = PREV_INSN (BB_HEAD (a));
  while (insn != end && (!NONDEBUG_INSN_P (insn) || !INSN_HAS_LOCATION (insn)))
    insn = PREV_INSN (insn);
  if (insn != end && loc_equal(self,INSN_LOCATION (insn), goto_locus))
    return false;
  /* Then scan block B forward.  */
  insn = BB_HEAD (b);
  if (insn){
      end = NEXT_INSN (BB_END (b));
      while (insn != end && !NONDEBUG_INSN_P (insn))
          insn = NEXT_INSN (insn);
      if (insn != end && INSN_HAS_LOCATION (insn)  && loc_equal(self,INSN_LOCATION (insn), goto_locus))
          return false;
  }
  return true;
}

/* If the single edge between blocks A and B is the only place in RTL which
   holds some unique locus, emit a nop with that locus between the blocks.  */

static void emit_nop_for_unique_locus_between (MtcsCfgRtl *self,basic_block a, basic_block b)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   if (!unique_locus_on_edge_between_p(self,a, b))
      return;
   BB_END (a) = mtcs_emit_emit_insn_after_noloc/*!emit_insn_after_noloc*/(mtcsEmit,gen_nop (), BB_END (a), a);
   INSN_LOCATION (BB_END (a)) = EDGE_SUCC (a, 0)->goto_locus;
}


static void printBB(basic_block bb)
{
   gimple_stmt_iterator gsi;

   for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
                gimple *stmt = gsi_stmt (gsi);
                fprintf(stderr,"mtcscfgrtl.c printbb 00 bb:%p\n",bb);
                aet_print_gimple(stmt);
    }
}

//原型 rtl_merge_blocks cfgrtl.cc
void mtcs_cfg_rtl_merge_blocks (MtcsCfgRtl *self,basic_block a, basic_block b)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *opts=mtcsOptions->global_options;

   n_debug("mtcscfgrtl.c mtcs_cfg_rtl_merge_blocks 00 打印BB a:%p b:%p\n",a,b);
   //printBB(a);
   //n_debug("mtcscfgrtl.c mtcs_cfg_rtl_merge_blocks 11 打印BB a:%p b:%p\n",a,b);
   //printBB(b);


   /* If B is a forwarder block whose outgoing edge has no location, we'll
      propagate the locus of the edge between A and B onto it.  */
   const bool forward_edge_locus = (b->flags & BB_FORWARDER_BLOCK) != 0
       && LOCATION_LOCUS (EDGE_SUCC (b, 0)->goto_locus) == UNKNOWN_LOCATION;
   rtx_insn *b_head = BB_HEAD (b), *b_end = BB_END (b), *a_end = BB_END (a);
   rtx_insn *del_first = NULL, *del_last = NULL;
   rtx_insn *b_debug_start = b_end, *b_debug_end = b_end;
   bool b_empty = false;

   if (dump_file)
     fprintf (dump_file, "Merging block %d into block %d...\n", b->index, a->index);

   while (DEBUG_INSN_P (b_end))
     b_end = PREV_INSN (b_debug_start = b_end);
   /* If there was a CODE_LABEL beginning B, delete it.  */
   if (LABEL_P (b_head)){
       /* Detect basic blocks with nothing but a label.  This can happen
      in particular at the end of a function.  */
       if (b_head == b_end)
           b_empty = true;
       del_first = del_last = b_head;
       b_head = NEXT_INSN (b_head);
   }
   n_debug("mtcscfgrtl.c mtcs_cfg_rtl_merge_blocks 11 del_first:%p del_last:%p forward_edge_locus:%d\n",
         del_first,del_last,forward_edge_locus);

   /* Delete the basic block note and handle blocks containing just that
      note.  */
   if (NOTE_INSN_BASIC_BLOCK_P (b_head)){
       if (b_head == b_end)
           b_empty = true;
       if (! del_last)
           del_first = b_head;
       del_last = b_head;
       b_head = NEXT_INSN (b_head);
   }
   n_debug("mtcscfgrtl.c mtcs_cfg_rtl_merge_blocks 22 del_first:%p del_last:%p\n",del_first,del_last);

   /* If there was a jump out of A, delete it.  */
   if (JUMP_P (a_end)){
       rtx_insn *prev;
       for (prev = PREV_INSN (a_end); ; prev = PREV_INSN (prev))
         if (!NOTE_P (prev) || NOTE_INSN_BASIC_BLOCK_P (prev)  || prev == BB_HEAD (a))
             break;
       del_first = a_end;
       a_end = PREV_INSN (del_first);
   }else if (BARRIER_P (NEXT_INSN (a_end)))
     del_first = NEXT_INSN (a_end);
   /* Delete everything marked above as well as crap that might be
      hanging out between the two blocks.  */
   BB_END (a) = a_end;
   BB_HEAD (b) = b_empty ? NULL : b_head;
   n_debug("mtcscfgrtl.c mtcs_cfg_rtl_merge_blocks 33 del_first:%p del_last:%p\n",del_first,del_last);

   mtcs_cfg_rtl_delete_insn_chain/*!delete_insn_chain */(self,del_first, del_last, true);
   /* If not optimizing, preserve the locus of the single edge between
      blocks A and B if necessary by emitting a nop.  */
   if (!opts->x_optimize   && !forward_edge_locus  && !DECL_IGNORED_P (current_function_decl)){
       emit_nop_for_unique_locus_between(self,a, b);
       a_end = BB_END (a);
   }
   /* Reassociate the insns of B with A.  */
   if (!b_empty){
       update_bb_for_insn_chain (self,a_end, b_debug_end, a);
       BB_END (a) = b_debug_end;
       BB_HEAD (b) = NULL;
   }else if (b_end != b_debug_end){
       /* Move any deleted labels and other notes between the end of A
      and the debug insns that make up B after the debug insns,
      bringing the debug insns into A while keeping the notes after
      the end of A.  */
       if (NEXT_INSN (a_end) != b_debug_start)
          mtcs_rtl_reorder_insns_nobb/*!reorder_insns_nobb*/(mtcsRTL,NEXT_INSN (a_end), PREV_INSN (b_debug_start),b_debug_end);
       update_bb_for_insn_chain (self,b_debug_start, b_debug_end, a);
       BB_END (a) = b_debug_end;
   }
   n_debug("mtcscfgrtl.c mtcs_cfg_rtl_merge_blocks 44 del_first:%p del_last:%p\n",del_first,del_last);

   mtcs_dfcore_df_bb_delete/*!df_bb_delete*/(mtcsDfcore,b->index);
   if (forward_edge_locus)
     EDGE_SUCC (b, 0)->goto_locus = EDGE_SUCC (a, 0)->goto_locus;
   if (dump_file)
     fprintf (dump_file, "Merged blocks %d and %d.\n", a->index, b->index);
}

/* Return true when block A and B can be merged.  */
//rtl_cfg_hooks member
static bool rtl_can_merge_blocks (MtcsCfgRtl *self,basic_block a, basic_block b)
{
  /* If we are partitioning hot/cold basic blocks, we don't want to
     mess up unconditional or indirect jumps that cross between hot
     and cold sections.

     Basic block partitioning may result in some jumps that appear to
     be optimizable (or blocks that appear to be mergeable), but which really
     must be left untouched (they are required to make it safely across
     partition boundaries).  See  the comments at the top of
     bb-reorder.cc:partition_hot_cold_basic_blocks for complete details.  */

  if (BB_PARTITION (a) != BB_PARTITION (b))
    return false;
  /* Protect the loop latches.  */
  if (cfun->x_current_loops/*!current_loops*/ && b->loop_father->latch == b)
    return false;

  /* There must be exactly one edge in between the blocks.  */
  return (single_succ_p (a)
      && single_succ (a) == b
      && single_pred_p (b)
      && a != b
      /* Must be simple edge.  */
      && !(single_succ_edge (a)->flags & EDGE_COMPLEX)
      && a->next_bb == b
      && a != ENTRY_BLOCK_PTR_FOR_FN (cfun)
      && b != EXIT_BLOCK_PTR_FOR_FN (cfun)
      /* If the jump insn has side effects,
         we can't kill the edge.  */
      && (!JUMP_P (BB_END (a))
          || (reload_completed
          ? simplejump_p (BB_END (a)) : onlyjump_p (BB_END (a)))));
}

/* Return the label in the head of basic block BLOCK.  Create one if it doesn't
   exist.  */
//原型 block_label cfgrtl.h cfgrtl.cc
rtx_code_label *mtcs_cfg_rtl_block_label (MtcsCfgRtl *self,basic_block block)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  if (block == EXIT_BLOCK_PTR_FOR_FN (cfun))
    return NULL;
  if (!LABEL_P (BB_HEAD (block))){
      BB_HEAD (block) = mtcs_emit_emit_label_before/*!emit_label_before*/(mtcsEmit,
            mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL), BB_HEAD (block));
  }
  return as_a <rtx_code_label *> (BB_HEAD (block));
}

/* Remove all barriers from BB_FOOTER of a BB.  */
//原型 remove_barriers_from_footer cfgrtl.cc
void mtcs_cfg_rtl_remove_barriers_from_footer (MtcsCfgRtl *self,basic_block bb)
{
  rtx_insn *insn = BB_FOOTER (bb);
  /* Remove barriers but keep jumptables.  */
  while (insn){
      if (BARRIER_P (insn)){
          if (PREV_INSN (insn))
            SET_NEXT_INSN (PREV_INSN (insn)) = NEXT_INSN (insn);
          else
            BB_FOOTER (bb) = NEXT_INSN (insn);
          if (NEXT_INSN (insn))
            SET_PREV_INSN (NEXT_INSN (insn)) = PREV_INSN (insn);
      }
      if (LABEL_P (insn))
          return;
      insn = NEXT_INSN (insn);
  }
}

/* Attempt to perform edge redirection by replacing possibly complex jump
   instruction by unconditional jump or removing jump completely.  This can
   apply only if all edges now point to the same block.  The parameters and
   return values are equivalent to redirect_edge_and_branch.  */
//原型 try_redirect_by_replacing_jump cfgrtl.h cfgrtl.cc
edge mtcs_cfg_rtl_try_redirect_by_replacing_jump (MtcsCfgRtl *self,edge e, basic_block target, bool in_cfglayout)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsCfg *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
  MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *opts=mtcsOptions->global_options;
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  basic_block src = e->src;
  rtx_insn *insn = BB_END (src);
  rtx set;
  bool fallthru = false;
  n_debug("mtcscfgrtl.c mtcs_cfg_rtl_try_redirect_by_replacing_jump 00 src:%p %p %p %d\n",src,e,target,in_cfglayout);
  /* If we are partitioning hot/cold basic blocks, we don't want to
     mess up unconditional or indirect jumps that cross between hot
     and cold sections.

     Basic block partitioning may result in some jumps that appear to
     be optimizable (or blocks that appear to be mergeable), but which really
     must be left untouched (they are required to make it safely across
     partition boundaries).  See  the comments at the top of
     bb-reorder.cc:partition_hot_cold_basic_blocks for complete details.  */
  if (BB_PARTITION (src) != BB_PARTITION (target))
    return NULL;
  /* We can replace or remove a complex jump only when we have exactly
     two edges.  Also, if we have exactly one outgoing edge, we can
     redirect that.  */
  if (EDGE_COUNT (src->succs) >= 3
      /* Verify that all targets will be TARGET.  Specifically, the
     edge that is not E must also go to TARGET.  */
      || (EDGE_COUNT (src->succs) == 2
      && EDGE_SUCC (src, EDGE_SUCC (src, 0) == e)->dest != target))
    return NULL;
  if (!onlyjump_p (insn))
    return NULL;
  if ((!opts->x_optimize || reload_completed) && tablejump_p (insn, NULL, NULL))
    return NULL;
  /* Avoid removing branch with side effects.  */
  set = single_set (insn);
  if (!set || side_effects_p (set))
    return NULL;
  /* See if we can create the fallthru edge.  */
  if (in_cfglayout || mtcs_cfg_rgl_can_fallthru/*!can_fallthru*/(self,src, target)){
      if (dump_file)
          fprintf (dump_file, "Removing jump %i.\n", INSN_UID (insn));
      fallthru = true;
      /* Selectively unlink whole insn chain.  */
      if (in_cfglayout){
          mtcs_cfg_rtl_delete_insn_chain/*!delete_insn_chain */(self,insn, BB_END (src), false);
          mtcs_cfg_rtl_remove_barriers_from_footer(self,src);
      }else
          mtcs_cfg_rtl_delete_insn_chain/*!delete_insn_chain */(self,insn, PREV_INSN (BB_HEAD (target)), false);
  }
  /* If this already is simplejump, redirect it.  */
  else if (simplejump_p (insn)){
      if (e->dest == target)
          return NULL;
      if (dump_file)
          fprintf (dump_file, "Redirecting jump %i from %i to %i.\n",INSN_UID (insn), e->dest->index, target->index);
      if (!mtcs_dojump_redirect_jump/*!redirect_jump*/(mtcsDojump,
            as_a <rtx_jump_insn *> (insn), mtcs_cfg_rtl_block_label/*!block_label*/(self,target), 0)){
          gcc_assert (target == EXIT_BLOCK_PTR_FOR_FN (cfun));
          return NULL;
      }
  }
  /* Cannot do anything for target exit block.  */
  else if (target == EXIT_BLOCK_PTR_FOR_FN (cfun))
    return NULL;
  /* Or replace possibly complicated jump insn by simple jump insn.  */
  else{
      rtx_code_label *target_label = mtcs_cfg_rtl_block_label/*!block_label*/(self,target);
      rtx_insn *barrier;
      rtx_insn *label;
      rtx_jump_table_data *table;

      mtcs_emit_emit_jump_insn_after_noloc/*!emit_jump_insn_after_noloc*/(mtcsEmit,
            target_rtx_gen_jump/*!targetm.gen_jump*/(mtcsMachine->tmrtx,target_label), insn);
      JUMP_LABEL (BB_END (src)) = target_label;
      LABEL_NUSES (target_label)++;
      if (dump_file)
          fprintf (dump_file, "Replacing insn %i by jump %i\n",INSN_UID (insn), INSN_UID (BB_END (src)));
      mtcs_cfg_rtl_delete_insn_chain/*!delete_insn_chain */(self,insn, insn, false);
      /* Recognize a tablejump that we are converting to a
     simple jump and remove its associated CODE_LABEL
     and ADDR_VEC or ADDR_DIFF_VEC.  */
      if (tablejump_p (insn, &label, &table))
          mtcs_cfg_rtl_delete_insn_chain/*!delete_insn_chain */(self,label, table, false);

      barrier = next_nonnote_nondebug_insn (BB_END (src));
      if (!barrier || !BARRIER_P (barrier))
         mtcs_emit_emit_barrier_after/*!emit_barrier_after*/(mtcsEmit,BB_END (src));
      else{
          if (barrier != NEXT_INSN (BB_END (src))){
              /* Move the jump before barrier so that the notes
             which originally were or were created before jump table are
             inside the basic block.  */
              rtx_insn *new_insn = BB_END (src);
              update_bb_for_insn_chain (self,NEXT_INSN (BB_END (src)),PREV_INSN (barrier), src);
              SET_NEXT_INSN (PREV_INSN (new_insn)) = NEXT_INSN (new_insn);
              SET_PREV_INSN (NEXT_INSN (new_insn)) = PREV_INSN (new_insn);
              SET_NEXT_INSN (new_insn) = barrier;
              SET_NEXT_INSN (PREV_INSN (barrier)) = new_insn;
              SET_PREV_INSN (new_insn) = PREV_INSN (barrier);
              SET_PREV_INSN (barrier) = new_insn;
         }
      }
  }
  /* Keep only one edge out and set proper flags.  */
  if (!single_succ_p (src))
     mtcs_cfg_context_remove_edge/*!remove_edge*/(mtcsCfgContext,e);
  gcc_assert (single_succ_p (src));
  e = single_succ_edge (src);
  if (fallthru)
    e->flags = EDGE_FALLTHRU;
  else
    e->flags = 0;
  e->probability = profile_probability::always ();
  if (e->dest != target)
     mtcs_cfg_redirect_edge_succ/*!redirect_edge_succ*/(mtcsCfg,e, target);
  return e;
}

/* Subroutine of redirect_branch_edge that tries to patch the jump
   instruction INSN so that it reaches block NEW.  Do this
   only when it originally reached block OLD.  Return true if this
   worked or the original target wasn't OLD, return false if redirection
   doesn't work.  */

static bool patch_jump_insn (MtcsCfgRtl *self,rtx_insn *insn, rtx_insn *old_label, basic_block new_bb)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpand  *mtcsExpand=mtcs_target_get_expand(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);

  rtx_jump_table_data *table;
  rtx tmp;
  /* Recognize a tablejump and adjust all matching cases.  */
  if (tablejump_p (insn, NULL, &table)){
      rtvec vec;
      int j;
      rtx_code_label *new_label = mtcs_cfg_rtl_block_label/*!block_label*/(self,new_bb);
      if (new_bb == EXIT_BLOCK_PTR_FOR_FN (cfun))
          return false;
      vec = table->get_labels ();

      for (j = GET_NUM_ELEM (vec) - 1; j >= 0; --j)
        if (XEXP (RTVEC_ELT (vec, j), 0) == old_label){
            RTVEC_ELT (vec, j) = gen_rtx_LABEL_REF (mtcs_mode_get_Pmode(mtcsMode), new_label);
            --LABEL_NUSES (old_label);
            ++LABEL_NUSES (new_label);
        }

      /* Handle casesi dispatch insns.  */
      if ((tmp = tablejump_casesi_pattern (insn)) != NULL_RTX
        && label_ref_label (XEXP (SET_SRC (tmp), 2)) == old_label){
          XEXP (SET_SRC (tmp), 2) = gen_rtx_LABEL_REF (mtcs_mode_get_Pmode(mtcsMode),new_label);
          --LABEL_NUSES (old_label);
          ++LABEL_NUSES (new_label);
      }
  }else if ((tmp = extract_asm_operands (PATTERN (insn))) != NULL){
      int i, n = ASM_OPERANDS_LABEL_LENGTH (tmp);
      rtx note;
      if (new_bb == EXIT_BLOCK_PTR_FOR_FN (cfun))
          return false;
      rtx_code_label *new_label = mtcs_cfg_rtl_block_label/*!block_label*/(self,new_bb);
      for (i = 0; i < n; ++i){
          rtx old_ref = ASM_OPERANDS_LABEL (tmp, i);
          gcc_assert (GET_CODE (old_ref) == LABEL_REF);
          if (XEXP (old_ref, 0) == old_label){
              ASM_OPERANDS_LABEL (tmp, i) = gen_rtx_LABEL_REF (Pmode, new_label);
              --LABEL_NUSES (old_label);
              ++LABEL_NUSES (new_label);
          }
      }

      if (JUMP_LABEL (insn) == old_label){
          JUMP_LABEL (insn) = new_label;
          note = find_reg_note (insn, REG_LABEL_TARGET, new_label);
          if (note)
            mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,insn, note);
      }else{
          note = find_reg_note (insn, REG_LABEL_TARGET, old_label);
          if (note)
            mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,insn, note);
          if (JUMP_LABEL (insn) != new_label
              && !find_reg_note (insn, REG_LABEL_TARGET, new_label))
            add_reg_note (insn, REG_LABEL_TARGET, new_label);
      }
      while ((note = find_reg_note (insn, REG_LABEL_OPERAND, old_label)) != NULL_RTX)
          XEXP (note, 0) = new_label;
  }else{
      /* ?? We may play the games with moving the named labels from
     one basic block to the other in case only one computed_jump is
     available.  */
      if (computed_jump_p (insn)
      /* A return instruction can't be redirected.  */
      || returnjump_p (insn))
          return false;

      if (!mtcsExpand->currently_expanding_to_rtl || JUMP_LABEL (insn) == old_label){
          /* If the insn doesn't go where we think, we're confused.  */
          gcc_assert (JUMP_LABEL (insn) == old_label);
          /* If the substitution doesn't succeed, die.  This can happen
             if the back end emitted unrecognizable instructions or if
             target is exit block on some arches.  Or for crossing
             jumps.  */
          if (!mtcs_dojump_redirect_jump/*!redirect_jump*/(mtcsDojump,
                as_a <rtx_jump_insn *> (insn),mtcs_cfg_rtl_block_label/*!block_label*/(self,new_bb), 0)){
              gcc_assert (new_bb == EXIT_BLOCK_PTR_FOR_FN (cfun) || CROSSING_JUMP_P (insn));
              return false;
          }
      }
  }
  return true;
}


/* Redirect edge representing branch of (un)conditional jump or tablejump,
   NULL on failure  */
static edge redirect_branch_edge (MtcsCfgRtl *self,edge e, basic_block target)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpand  *mtcsExpand=mtcs_target_get_expand(mtcsTarget);
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);

  rtx_insn *old_label = BB_HEAD (e->dest);
  basic_block src = e->src;
  rtx_insn *insn = BB_END (src);
  /* We can only redirect non-fallthru edges of jump insn.  */
  if (e->flags & EDGE_FALLTHRU)
    return NULL;
  else if (!JUMP_P (insn) && !mtcsExpand->currently_expanding_to_rtl)
    return NULL;

  if (!mtcsExpand->currently_expanding_to_rtl){
      if (!patch_jump_insn(self,as_a <rtx_jump_insn *> (insn), old_label, target))
          return NULL;
  }else
    /* When expanding this BB might actually contain multiple
       jumps (i.e. not yet split by find_many_sub_basic_blocks).
       Redirect all of those that match our label.  */
    FOR_BB_INSNS (src, insn)
      if (JUMP_P (insn) && !patch_jump_insn(self,as_a <rtx_jump_insn *> (insn), old_label, target))
          return NULL;

  if (dump_file)
    fprintf (dump_file, "Edge %i->%i redirected to %i\n", e->src->index, e->dest->index, target->index);
  if (e->dest != target)
    e = mtcs_cfg_context_redirect_edge_succ_nodup/*!redirect_edge_succ_nodup*/(mtcsCfgContext,e, target);
  return e;
}

/* Called when edge E has been redirected to a new destination,
   in order to update the region crossing flag on the edge and
   jump.  */
static void fixup_partition_crossing (MtcsCfgRtl *self,edge e)
{
  if (e->src == ENTRY_BLOCK_PTR_FOR_FN (cfun) || e->dest  == EXIT_BLOCK_PTR_FOR_FN (cfun))
    return;
  /* If we redirected an existing edge, it may already be marked
     crossing, even though the new src is missing a reg crossing note.
     But make sure reg crossing note doesn't already exist before
     inserting.  */
  if (BB_PARTITION (e->src) != BB_PARTITION (e->dest)){
      e->flags |= EDGE_CROSSING;
      if (JUMP_P (BB_END (e->src)))
          CROSSING_JUMP_P (BB_END (e->src)) = 1;
  }else if (BB_PARTITION (e->src) == BB_PARTITION (e->dest)){
      e->flags &= ~EDGE_CROSSING;
      /* Remove the section crossing note from jump at end of
         src if it exists, and if no other successors are
         still crossing.  */
      if (JUMP_P (BB_END (e->src)) && CROSSING_JUMP_P (BB_END (e->src))){
          bool has_crossing_succ = false;
          edge e2;
          edge_iterator ei;
          FOR_EACH_EDGE (e2, ei, e->src->succs){
              has_crossing_succ |= (e2->flags & EDGE_CROSSING);
              if (has_crossing_succ)
                break;
          }
          if (!has_crossing_succ)
              CROSSING_JUMP_P (BB_END (e->src)) = 0;
      }
  }
}

/* Called when block BB has been reassigned to the cold partition,
   because it is now dominated by another cold block,
   to ensure that the region crossing attributes are updated.  */
static void fixup_new_cold_bb (MtcsCfgRtl *self,basic_block bb)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfg *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);

  edge e;
  edge_iterator ei;
  /* This is called when a hot bb is found to now be dominated
     by a cold bb and therefore needs to become cold. Therefore,
     its preds will no longer be region crossing. Any non-dominating
     preds that were previously hot would also have become cold
     in the caller for the same region. Any preds that were previously
     region-crossing will be adjusted in fixup_partition_crossing.  */
  FOR_EACH_EDGE (e, ei, bb->preds){
      fixup_partition_crossing(self,e);
  }
  /* Possibly need to make bb's successor edges region crossing,
     or remove stale region crossing.  */
  FOR_EACH_EDGE (e, ei, bb->succs){
      /* We can't have fall-through edges across partition boundaries.
         Note that force_nonfallthru will do any necessary partition
         boundary fixup by calling fixup_partition_crossing itself.  */
      if ((e->flags & EDGE_FALLTHRU)  && BB_PARTITION (bb) != BB_PARTITION (e->dest)
          && e->dest != EXIT_BLOCK_PTR_FOR_FN (cfun))
         mtcs_cfg_context_force_nonfallthru/*!force_nonfallthru*/(mtcsCfgContext,e);
      else
        fixup_partition_crossing(self,e);
  }
}


/* Emit a barrier after BB, into the footer if we are in CFGLAYOUT mode.  */
//原型 emit_barrier_after_bb cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_emit_barrier_after_bb (MtcsCfgRtl *self,basic_block bb)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);

   rtx_barrier *barrier =mtcs_emit_emit_barrier_after/*!emit_barrier_after*/(mtcsEmit,BB_END (bb));
   gcc_assert (mtcs_cfg_context_get_state/*!current_ir_type ()*/(mtcsCfgContext) == IR_RTL_CFGRTL
         || mtcs_cfg_context_get_state/*!current_ir_type ()*/(mtcsCfgContext) == IR_RTL_CFGLAYOUT);
   if (mtcs_cfg_context_get_state/*!current_ir_type ()*/(mtcsCfgContext) == IR_RTL_CFGLAYOUT){
      rtx_insn *insn = unlink_insn_chain (barrier, barrier);
      if (BB_FOOTER (bb)){
         rtx_insn *footer_tail = BB_FOOTER (bb);
         while (NEXT_INSN (footer_tail))
            footer_tail = NEXT_INSN (footer_tail);
         if (!BARRIER_P (footer_tail)){
            SET_NEXT_INSN (footer_tail) = insn;
            SET_PREV_INSN (insn) = footer_tail;
         }
      }else
         BB_FOOTER (bb) = insn;
   }
}

/* Like force_nonfallthru below, but additionally performs redirection
   Used by redirect_edge_and_branch_force.  JUMP_LABEL is used only
   when redirecting to the EXIT_BLOCK, it is either ret_rtx or
   simple_return_rtx, indicating which kind of returnjump to create.
   It should be NULL otherwise.  */
//原型 force_nonfallthru_and_redirect cfgrtl.h cfgrtl.cc
basic_block mcs_cfg_rtl_force_nonfallthru_and_redirect (MtcsCfgRtl *self,edge e, basic_block target, rtx jump_label)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsCfg *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  basic_block jump_block, new_bb = NULL, src = e->src;
  rtx note;
  edge new_edge;
  int abnormal_edge_flags = 0;
  bool asm_goto_edge = false;
  int loc;
  /* In the case the last instruction is conditional jump to the next
     instruction, first redirect the jump itself and then continue
     by creating a basic block afterwards to redirect fallthru edge.  */
  if (e->src != ENTRY_BLOCK_PTR_FOR_FN (cfun)
      && e->dest != EXIT_BLOCK_PTR_FOR_FN (cfun)
      && any_condjump_p (BB_END (e->src))
      && JUMP_LABEL (BB_END (e->src)) == BB_HEAD (e->dest)){
      rtx note;
      edge b = mtcs_cfg_unchecked_make_edge/*!unchecked_make_edge*/(mtcsCfg,e->src, target, 0);
      bool redirected;
      redirected = mtcs_dojump_redirect_jump/*!redirect_jump*/(mtcsDojump,
            as_a <rtx_jump_insn *> (BB_END (e->src)), mtcs_cfg_rtl_block_label/*!block_label*/(self,target), 0);
      gcc_assert (redirected);
      note = find_reg_note (BB_END (e->src), REG_BR_PROB, NULL_RTX);
      if (note){
          int prob = XINT (note, 0);
          b->probability = profile_probability::from_reg_br_prob_note (prob);
          e->probability -= e->probability;
      }
  }

  if (e->flags & EDGE_ABNORMAL){
      /* Irritating special case - fallthru edge to the same block as abnormal
     edge.
     We can't redirect abnormal edge, but we still can split the fallthru
     one and create separate abnormal edge to original destination.
     This allows bb-reorder to make such edge non-fallthru.  */
      gcc_assert (e->dest == target);
      abnormal_edge_flags = e->flags & ~EDGE_FALLTHRU;
      e->flags &= EDGE_FALLTHRU;
  }else{
      gcc_assert (e->flags & EDGE_FALLTHRU);
      if (e->src == ENTRY_BLOCK_PTR_FOR_FN (cfun)){
          /* We can't redirect the entry block.  Create an empty block
             at the start of the function which we use to add the new
             jump.  */
          edge tmp;
          edge_iterator ei;
          bool found = false;

          basic_block bb = mtcs_cfg_context_create_basic_block/*!create_basic_block*/(mtcsCfgContext,
                BB_HEAD (e->dest), NULL,ENTRY_BLOCK_PTR_FOR_FN (cfun));
          bb->count = ENTRY_BLOCK_PTR_FOR_FN (cfun)->count;
          /* Make sure new block ends up in correct hot/cold section.  */
          BB_COPY_PARTITION (bb, e->dest);
          /* Change the existing edge's source to be the new block, and add
             a new edge from the entry block to the new block.  */
          e->src = bb;
          for (ei = ei_start (ENTRY_BLOCK_PTR_FOR_FN (cfun)->succs);(tmp = ei_safe_edge (ei)); ){
              if (tmp == e){
                  ENTRY_BLOCK_PTR_FOR_FN (cfun)->succs->unordered_remove (ei.index);
                  found = true;
                  break;
              }else
                  ei_next (&ei);
          }
          gcc_assert (found);
          vec_safe_push (bb->succs, e);
          mtcs_cfg_make_single_succ_edge/*!make_single_succ_edge*/(mtcsCfg,ENTRY_BLOCK_PTR_FOR_FN (cfun), bb,EDGE_FALLTHRU);
      }
  }
  /* If e->src ends with asm goto, see if any of the ASM_OPERANDS_LABELs
     don't point to the target or fallthru label.  */
  if (JUMP_P (BB_END (e->src))
      && target != EXIT_BLOCK_PTR_FOR_FN (cfun)
      && (e->flags & EDGE_FALLTHRU)
      && (note = extract_asm_operands (PATTERN (BB_END (e->src))))){
      int i, n = ASM_OPERANDS_LABEL_LENGTH (note);
      bool adjust_jump_target = false;
      for (i = 0; i < n; ++i){
          if (XEXP (ASM_OPERANDS_LABEL (note, i), 0) == BB_HEAD (e->dest)){
              LABEL_NUSES (XEXP (ASM_OPERANDS_LABEL (note, i), 0))--;
              XEXP (ASM_OPERANDS_LABEL (note, i), 0) = mtcs_cfg_rtl_block_label/*!block_label*/(self,target);
              LABEL_NUSES (XEXP (ASM_OPERANDS_LABEL (note, i), 0))++;
              adjust_jump_target = true;
          }
          if (XEXP (ASM_OPERANDS_LABEL (note, i), 0) == BB_HEAD (target))
            asm_goto_edge = true;
      }
      if (adjust_jump_target){
          rtx_insn *insn = BB_END (e->src);
          rtx note;
          rtx_insn *old_label = BB_HEAD (e->dest);
          rtx_insn *new_label = BB_HEAD (target);

          if (JUMP_LABEL (insn) == old_label){
              JUMP_LABEL (insn) = new_label;
              note = find_reg_note (insn, REG_LABEL_TARGET, new_label);
              if (note)
                  mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,insn, note);
          }else{
              note = find_reg_note (insn, REG_LABEL_TARGET, old_label);
              if (note)
                  mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,insn, note);
              if (JUMP_LABEL (insn) != new_label  && !find_reg_note (insn, REG_LABEL_TARGET, new_label))
                  add_reg_note (insn, REG_LABEL_TARGET, new_label);
          }
          while ((note = find_reg_note (insn, REG_LABEL_OPERAND, old_label)) != NULL_RTX)
            XEXP (note, 0) = new_label;
      }
  }

  if (EDGE_COUNT (e->src->succs) >= 2 || abnormal_edge_flags || asm_goto_edge){
      rtx_insn *new_head;
      profile_count count = e->count ();
      profile_probability probability = e->probability;
      /* Create the new structures.  */

      /* If the old block ended with a tablejump, skip its table
     by searching forward from there.  Otherwise start searching
     forward from the last instruction of the old block.  */
      rtx_jump_table_data *table;
      if (tablejump_p (BB_END (e->src), NULL, &table))
          new_head = table;
      else
          new_head = BB_END (e->src);
      new_head = NEXT_INSN (new_head);

      jump_block = mtcs_cfg_context_create_basic_block/*!create_basic_block*/(mtcsCfgContext,new_head, NULL, e->src);
      jump_block->count = count;
      /* Make sure new block ends up in correct hot/cold section.  */
      BB_COPY_PARTITION (jump_block, e->src);
      /* Wire edge in.  */
      new_edge = mtcs_cfg_make_edge/*!make_edge*/(mtcsCfg,e->src, jump_block, EDGE_FALLTHRU);
      new_edge->probability = probability;
      /* Redirect old edge.  */
      redirect_edge_pred (e, jump_block);
      e->probability = profile_probability::always ();
      /* If e->src was previously region crossing, it no longer is
         and the reg crossing note should be removed.  */
      fixup_partition_crossing(self,new_edge);
      /* If asm goto has any label refs to target's label,
     add also edge from asm goto bb to target.  */
      if (asm_goto_edge){
          new_edge->probability /= 2;
          jump_block->count /= 2;
          edge new_edge2 =mtcs_cfg_make_edge/*!make_edge*/(mtcsCfg,new_edge->src, target,e->flags & ~EDGE_FALLTHRU);
          new_edge2->probability = probability - new_edge->probability;
      }

      new_bb = jump_block;
  }else
    jump_block = e->src;

  loc = e->goto_locus;
  e->flags &= ~EDGE_FALLTHRU;
  if (target == EXIT_BLOCK_PTR_FOR_FN (cfun)){
      if (jump_label == ret_rtx)
         mtcs_emit_emit_jump_insn_after_setloc/*!emit_jump_insn_after_setloc*/(mtcsEmit,
               target_rtx_gen_return/*!targetm.gen_return*/(mtcsMachine->tmrtx),BB_END (jump_block), loc);
      else{
          gcc_assert (jump_label == mtcsRTL->simple_return_rtx);
          mtcs_emit_emit_jump_insn_after_setloc/*!emit_jump_insn_after_setloc*/(mtcsEmit,
                target_rtx_gen_simple_return/*!targetm.gen_simple_return*/(mtcsMachine->tmrtx),BB_END (jump_block), loc);
      }
      set_return_jump_label (BB_END (jump_block));
  }else{
      rtx_code_label *label = mtcs_cfg_rtl_block_label/*!block_label*/(self,target);
      mtcs_emit_emit_jump_insn_after_setloc/*!emit_jump_insn_after_setloc*/(mtcsEmit,
            target_rtx_gen_jump/*!targetm.gen_jump*/(mtcsMachine->tmrtx,label),BB_END (jump_block), loc);
      JUMP_LABEL (BB_END (jump_block)) = label;
      LABEL_NUSES (label)++;
  }
  /* We might be in cfg layout mode, and if so, the following routine will
     insert the barrier correctly.  */
  mtcs_cfg_rtl_emit_barrier_after_bb/*!emit_barrier_after_bb*/(self,jump_block);
  mtcs_cfg_context_redirect_edge_succ_nodup/*!redirect_edge_succ_nodup*/(mtcsCfgContext,e, target);
  if (abnormal_edge_flags)
     mtcs_cfg_make_edge/*!make_edge*/(mtcsCfg,src, target, abnormal_edge_flags);
  mtcs_dfcore_df_mark_solutions_dirty/*!df_mark_solutions_dirty*/(mtcsDfcore);
  fixup_partition_crossing(self,e);
  return new_bb;
}

/* Edge E is assumed to be fallthru edge.  Emit needed jump instruction
   (and possibly create new basic block) to make edge non-fallthru.
   Return newly created BB or NULL if none.  */
//rtl_cfg_hooks member
static basic_block rtl_force_nonfallthru (MtcsCfgRtl *self,edge e)
{
  return mcs_cfg_rtl_force_nonfallthru_and_redirect/*!force_nonfallthru_and_redirect*/(self,e, e->dest, NULL_RTX);
}



/* The given edge should potentially be a fallthru edge.  If that is in
   fact true, delete the jump and barriers that are in the way.  */
//rtl_cfg_hooks member
static void rtl_tidy_fallthru_edge (MtcsCfgRtl *self,edge e)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  rtx_insn *q;
  basic_block b = e->src, c = b->next_bb;
  /* ??? In a late-running flow pass, other folks may have deleted basic
     blocks by nopping out blocks, leaving multiple BARRIERs between here
     and the target label. They ought to be chastised and fixed.

     We can also wind up with a sequence of undeletable labels between
     one block and the next.

     So search through a sequence of barriers, labels, and notes for
     the head of block C and assert that we really do fall through.  */
  for (q = NEXT_INSN (BB_END (b)); q != BB_HEAD (c); q = NEXT_INSN (q))
      if (NONDEBUG_INSN_P (q))
          return;
  /* Remove what will soon cease being the jump insn from the source block.
     If block B consisted only of this single jump, turn it into a deleted
     note.  */
  q = BB_END (b);
  if (JUMP_P (q)  && onlyjump_p (q)  && (any_uncondjump_p (q)  || single_succ_p (b))){
      rtx_insn *label;
      rtx_jump_table_data *table;

      if (tablejump_p (q, &label, &table)){
          /* The label is likely mentioned in some instruction before
             the tablejump and might not be DCEd, so turn it into
             a note instead and move before the tablejump that is going to
             be deleted.  */
          const char *name = LABEL_NAME (label);
          PUT_CODE (label, NOTE);
          NOTE_KIND (label) = NOTE_INSN_DELETED_LABEL;
          NOTE_DELETED_LABEL_NAME (label) = name;
          mtcs_rtl_reorder_insns/*!reorder_insns*/(mtcsRTL,label, label, PREV_INSN (q));
          mtcs_cfg_rtl_delete_insn/*!delete_insn*/(self,table);
      }

      q = PREV_INSN (q);
  }
  /* Unconditional jumps with side-effects (i.e. which we can't just delete
     together with the barrier) should never have a fallthru edge.  */
  else if (JUMP_P (q) && any_uncondjump_p (q))
    return;
  /* Selectively unlink the sequence.  */
  if (q != PREV_INSN (BB_HEAD (c)))
      mtcs_cfg_rtl_delete_insn_chain/*!delete_insn_chain */(self,NEXT_INSN (q), PREV_INSN (BB_HEAD (c)), false);

  e->flags |= EDGE_FALLTHRU;
}

/* Should move basic block BB after basic block AFTER.  NIY.  */
//rtl_cfg_hooks member
static bool rtl_move_block_after (MtcsCfgRtl *self,basic_block bb ATTRIBUTE_UNUSED,
              basic_block after ATTRIBUTE_UNUSED)
{
  return false;
}

/* Locate the last bb in the same partition as START_BB.  */
static basic_block last_bb_in_partition (MtcsCfgRtl *self,basic_block start_bb)
{
  basic_block bb;
  FOR_BB_BETWEEN (bb, start_bb, EXIT_BLOCK_PTR_FOR_FN (cfun), next_bb){
      if (BB_PARTITION (start_bb) != BB_PARTITION (bb->next_bb))
        return bb;
  }
  /* Return bb before the exit block.  */
  return bb->prev_bb;
}



/* Queue instructions for insertion on an edge between two basic blocks.
   The new instructions and basic blocks (if any) will not appear in the
   CFG until commit_edge_insertions is called.  If there are already
   queued instructions on the edge, PATTERN is appended to them.  */
//原型 insert_insn_on_edge cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_insert_insn_on_edge (MtcsCfgRtl *self,rtx pattern, edge e)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   /* We cannot insert instructions on an abnormal critical edge.
   It will be easier to find the culprit if we die now.  */
   gcc_assert (!((e->flags & EDGE_ABNORMAL) && EDGE_CRITICAL_P (e)));
   if (e->insns.r == NULL_RTX)
      mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
   else
      mtcs_emit_push_to_sequence/*!push_to_sequence*/(mtcsEmit,e->insns.r);

   mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,pattern);
   e->insns.r =  mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
}

/* Like insert_insn_on_edge, but if there are already queued instructions
   on the edge, PATTERN is prepended to them.  */
//原型 prepend_insn_to_edge cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_prepend_insn_to_edge (MtcsCfgRtl *self,rtx pattern, edge e)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  /* We cannot insert instructions on an abnormal critical edge.
     It will be easier to find the culprit if we die now.  */
  gcc_assert (!((e->flags & EDGE_ABNORMAL) && EDGE_CRITICAL_P (e)));
  mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,pattern);
  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,e->insns.r);
  e->insns.r = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
  mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
}

/* Update the CFG for the instructions queued on edge E.  */
//原型 commit_one_edge_insertion cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_commit_one_edge_insertion (MtcsCfgRtl *self,edge e)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpand *mtcsExpand=mtcs_target_get_expand(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
  MtcsCfgBuild *mtcsCfgBuild = mtcs_target_get_cfg_build(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);

  rtx_insn *before = NULL, *after = NULL, *insns, *tmp, *last;
  basic_block bb;
  /* Pull the insns off the edge now since the edge might go away.  */
  insns = e->insns.r;
  e->insns.r = NULL;
  /* Allow the sequence to contain internal jumps, such as a memcpy loop
     or an allocation loop.  If such a sequence is emitted during RTL
     expansion, we'll create the appropriate basic blocks later,
     at the end of the pass.  But if such a sequence is emitted after
     initial expansion, we'll need to find the subblocks ourselves.  */
  bool contains_jump = false;
  if (!mtcsExpand->currently_expanding_to_rtl)
      for (rtx_insn *insn = insns; insn; insn = NEXT_INSN (insn))
          if (JUMP_P (insn)){
              mtcs_dojump_rebuild_jump_labels_chain/*!rebuild_jump_labels_chain*/(mtcsDojump,insns);
              contains_jump = true;
              break;
          }

  /* Figure out where to put these insns.  If the destination has
     one predecessor, insert there.  Except for the exit block.  */
  if (single_pred_p (e->dest) && e->dest != EXIT_BLOCK_PTR_FOR_FN (cfun)){
      bb = e->dest;
      /* Get the location correct wrt a code label, and "nice" wrt
     a basic block note, and before everything else.  */
      tmp = BB_HEAD (bb);
      if (LABEL_P (tmp))
          tmp = NEXT_INSN (tmp);
      if (NOTE_INSN_BASIC_BLOCK_P (tmp))
          tmp = NEXT_INSN (tmp);
      if (tmp == BB_HEAD (bb))
          before = tmp;
      else if (tmp)
          after = PREV_INSN (tmp);
      else
          after =mtcs_rtl_data_get_last_insn/*!get_last_insn*/ (mtcsRtlData);
  }
  /* If the source has one successor and the edge is not abnormal,
     insert there.  Except for the entry block.
     Don't do this if the predecessor ends in a jump other than
     unconditional simple jump.  E.g. for asm goto that points all
     its labels at the fallthru basic block, we can't insert instructions
     before the asm goto, as the asm goto can have various of side effects,
     and can't emit instructions after the asm goto, as it must end
     the basic block.  */
  else if ((e->flags & EDGE_ABNORMAL) == 0
       && single_succ_p (e->src)
       && e->src != ENTRY_BLOCK_PTR_FOR_FN (cfun)
       && (!JUMP_P (BB_END (e->src))
           || simplejump_p (BB_END (e->src)))){
      bb = e->src;

      /* It is possible to have a non-simple jump here.  Consider a target
     where some forms of unconditional jumps clobber a register.  This
     happens on the fr30 for example.

     We know this block has a single successor, so we can just emit
     the queued insns before the jump.  */
      if (JUMP_P (BB_END (bb)))
          before = BB_END (bb);
      else{
          /* We'd better be fallthru, or we've lost track of what's what.  */
          gcc_assert (e->flags & EDGE_FALLTHRU);

          after = BB_END (bb);
      }
  }
  /* Otherwise we must split the edge.  */
  else {
      bb = mtcs_cfg_context_split_edge/*!split_edge*/(mtcsCfgContext,e);
      /* If E crossed a partition boundary, we needed to make bb end in
         a region-crossing jump, even though it was originally fallthru.  */
      if (JUMP_P (BB_END (bb)))
          before = BB_END (bb);
      else
          after = BB_END (bb);
  }
  /* Now that we've found the spot, do the insertion.  */
  if (before){
      mtcs_emit_emit_insn_before_noloc/*!emit_insn_before_noloc*/(mtcsEmit,insns, before, bb);
      last = prev_nonnote_insn (before);
  }else
    last = mtcs_emit_emit_insn_after_noloc/*!emit_insn_after_noloc*/(mtcsEmit,insns, after, bb);

  if (returnjump_p (last)){
      /* ??? Remove all outgoing edges from BB and add one for EXIT.
     This is not currently a problem because this only happens
     for the (single) epilogue, which already has a fallthru edge
     to EXIT.  */

      e = single_succ_edge (bb);
      gcc_assert (e->dest == EXIT_BLOCK_PTR_FOR_FN (cfun)
          && single_succ_p (bb) && (e->flags & EDGE_FALLTHRU));

      e->flags &= ~EDGE_FALLTHRU;
      mtcs_emit_emit_barrier_after/*!emit_barrier_after*/(mtcsEmit,last);

      if (before)
          mtcs_cfg_rtl_delete_insn/*!delete_insn*/(self,before);
  }else
    /* Sequences inserted after RTL expansion are expected to be SESE,
       with only internal branches allowed.  If the sequence jumps outside
       itself then we do not know how to add the associated edges here.  */
    gcc_assert (!JUMP_P (last) || currently_expanding_to_rtl);

  if (contains_jump)
     mtcs_cfg_build_find_sub_basic_blocks/*!find_sub_basic_blocks*/(mtcsCfgBuild,bb);
}

/* Update the CFG for all queued instructions.  */
//原型 commit_edge_insertions cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_commit_edge_insertions (MtcsCfgRtl *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpand *mtcsExpand=mtcs_target_get_expand(mtcsTarget);
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
  MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);

  basic_block bb;
  /* Optimization passes that invoke this routine can cause hot blocks
     previously reached by both hot and cold blocks to become dominated only
     by cold blocks. This will cause the verification below to fail,
     and lead to now cold code in the hot section. In some cases this
     may only be visible after newly unreachable blocks are deleted,
     which will be done by fixup_partitions.  */
  mtcs_cfg_rtl_fixup_partitions/*!fixup_partitions*/(self);
  n_debug("mtcscfgrtl.c mtcs_cfg_rtl_commit_edge_insertions 00 %d\n",mtcsExpand->currently_expanding_to_rtl);
  if (!mtcsExpand->currently_expanding_to_rtl)
     mtcs_cfg_context_checking_verify_flow_info/*!checking_verify_flow_info*/(mtcsCfgContext);
  n_debug("mtcscfgrtl.c mtcs_cfg_rtl_commit_edge_insertions 11 %d\n",mtcsExpand->currently_expanding_to_rtl);

  FOR_BB_BETWEEN (bb, ENTRY_BLOCK_PTR_FOR_FN (cfun),EXIT_BLOCK_PTR_FOR_FN (cfun), next_bb){
      edge e;
      edge_iterator ei;
      FOR_EACH_EDGE (e, ei, bb->succs)
        if (e->insns.r){
            if (mtcsExpand->currently_expanding_to_rtl)
               mtcs_dojump_rebuild_jump_labels_chain/*!rebuild_jump_labels_chain*/(mtcsDojump,e->insns.r);
            mtcs_cfg_rtl_commit_one_edge_insertion/*!commit_one_edge_insertion*/(self,e);
        }
  }
}

/* Like dump_function_to_file, but for RTL.  Print out dataflow information
   for the start of each basic block.  FLAGS are the TDF_* masks documented
   in dumpfile.h.  */
//原型 print_rtl_with_bb cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_print_rtl_with_bb (MtcsCfgRtl *self,FILE *outf, const rtx_insn *rtx_first, dump_flags_t flags)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  const rtx_insn *tmp_rtx;
  if (rtx_first == 0)
    fprintf (outf, "(nil)\n");
  else{
      enum bb_state { NOT_IN_BB, IN_ONE_BB, IN_MULTIPLE_BB };
      int max_uid = mtcs_rtl_data_get_max_uid/*!get_max_uid*/(mtcsRtlData);
      basic_block *start = XCNEWVEC (basic_block, max_uid);
      basic_block *end = XCNEWVEC (basic_block, max_uid);
      enum bb_state *in_bb_p = XCNEWVEC (enum bb_state, max_uid);
      basic_block bb;
      /* After freeing the CFG, we still have BLOCK_FOR_INSN set on most
     insns, but the CFG is not maintained so the basic block info
     is not reliable.  Therefore it's omitted from the dumps.  */
      if (! (cfun->curr_properties & PROP_cfg))
        flags &= ~TDF_BLOCKS;

      if (df)
         mtcs_dfcore_df_dump_start/*!df_dump_start*/(mtcsDfcore,outf);

      if (cfun->curr_properties & PROP_cfg){
          FOR_EACH_BB_REVERSE_FN (bb, cfun){
              rtx_insn *x;
              start[INSN_UID (BB_HEAD (bb))] = bb;
              end[INSN_UID (BB_END (bb))] = bb;
              if (flags & TDF_BLOCKS){
                  for (x = BB_HEAD (bb); x != NULL_RTX; x = NEXT_INSN (x)){
                      enum bb_state state = IN_MULTIPLE_BB;
                      if (in_bb_p[INSN_UID (x)] == NOT_IN_BB)
                          state = IN_ONE_BB;
                      in_bb_p[INSN_UID (x)] = state;
                      if (x == BB_END (bb))
                          break;
                  }
              }
          }
      }

      for (tmp_rtx = rtx_first; tmp_rtx != NULL; tmp_rtx = NEXT_INSN (tmp_rtx)){
          if (flags & TDF_BLOCKS){
              bb = start[INSN_UID (tmp_rtx)];
              if (bb != NULL){
                  dump_bb_info (outf, bb, 0, dump_flags, true, false);
                  if (df && (flags & TDF_DETAILS))
                     mtcs_dfcore_df_dump_top/*!df_dump_top*/(mtcsDfcore,bb, outf);
              }

              if (in_bb_p[INSN_UID (tmp_rtx)] == NOT_IN_BB
              && !NOTE_P (tmp_rtx)
              && !BARRIER_P (tmp_rtx))
                  fprintf (outf, ";; Insn is not within a basic block\n");
              else if (in_bb_p[INSN_UID (tmp_rtx)] == IN_MULTIPLE_BB)
                  fprintf (outf, ";; Insn is in multiple basic blocks\n");
          }

          if (flags & TDF_DETAILS)
             mtcs_dfcore_df_dump_insn_top/*!df_dump_insn_top*/(mtcsDfcore,tmp_rtx, outf);
          if (! (flags & TDF_SLIM))
            print_rtl_single (outf, tmp_rtx);
          else
            dump_insn_slim (outf, tmp_rtx);
          if (flags & TDF_DETAILS)
             mtcs_dfcore_df_dump_insn_bottom/*!df_dump_insn_bottom*/(mtcsDfcore,tmp_rtx, outf);

          bb = end[INSN_UID (tmp_rtx)];
          if (bb != NULL){
              if (flags & TDF_BLOCKS){
                  dump_bb_info (outf, bb, 0, dump_flags, false, true);
                  if (df && (flags & TDF_DETAILS))
                     mtcs_dfcore_df_dump_bottom/*!df_dump_bottom*/(mtcsDfcore,bb, outf);
                  putc ('\n', outf);
              }
              /* Emit a hint if the fallthrough target of current basic block
                 isn't the one placed right next.  */
              else if (EDGE_COUNT (bb->succs) > 0){
                  gcc_assert (BB_END (bb) == tmp_rtx);
                  const rtx_insn *ninsn = NEXT_INSN (tmp_rtx);
                  /* Bypass intervening deleted-insn notes and debug insns.  */
                  while (ninsn && !NONDEBUG_INSN_P (ninsn) && !start[INSN_UID (ninsn)])
                      ninsn = NEXT_INSN (ninsn);
                  edge e = find_fallthru_edge (bb->succs);
                  if (e && ninsn){
                      basic_block dest = e->dest;
                      if (start[INSN_UID (ninsn)] != dest)
                          fprintf (outf, "%s      ; pc falls through to BB %d\n",print_rtx_head, dest->index);
                  }
              }
          }
      }

      free (start);
      free (end);
      free (in_bb_p);
  }
}

/* Update the branch probability of BB if a REG_BR_PROB is present.  */
//原型 update_br_prob_note cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_update_br_prob_note (MtcsCfgRtl *self,basic_block bb)
{
  rtx note;
  note = find_reg_note (BB_END (bb), REG_BR_PROB, NULL_RTX);
  if (!JUMP_P (BB_END (bb)) || !BRANCH_EDGE (bb)->probability.initialized_p ()){
      if (note){
          rtx *note_link, this_rtx;
          note_link = &REG_NOTES (BB_END (bb));
          for (this_rtx = *note_link; this_rtx; this_rtx = XEXP (this_rtx, 1))
              if (this_rtx == note){
                  *note_link = XEXP (this_rtx, 1);
                  break;
              }
       }
       return;
  }
  if (!note  || XINT (note, 0) == BRANCH_EDGE (bb)->probability.to_reg_br_prob_note ())
      return;
  XINT (note, 0) = BRANCH_EDGE (bb)->probability.to_reg_br_prob_note ();
}

/* Get the last insn associated with block BB (that includes barriers and
   tablejumps after BB).  */
//原型 get_last_bb_insn cfgrtl.h cfgrtl.cc
rtx_insn * mtcs_cfg_rtl_get_last_bb_insn (MtcsCfgRtl *self,basic_block bb)
{
  rtx_jump_table_data *table;
  rtx_insn *tmp;
  rtx_insn *end = BB_END (bb);
  /* Include any jump table following the basic block.  */
  if (tablejump_p (end, NULL, &table))
    end = table;
  /* Include any barriers that may follow the basic block.  */
  tmp = next_nonnote_nondebug_insn_bb (end);
  while (tmp && BARRIER_P (tmp)){
      end = tmp;
      tmp = next_nonnote_nondebug_insn_bb (end);
  }
  return end;
}

/* Add all BBs reachable from entry via hot paths into the SET.  */
//原型 find_bbs_reachable_by_hot_paths cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_find_bbs_reachable_by_hot_paths (MtcsCfgRtl *self,hash_set<basic_block> *set)
{
  auto_vec<basic_block, 64> worklist;
  set->add (ENTRY_BLOCK_PTR_FOR_FN (cfun));
  worklist.safe_push (ENTRY_BLOCK_PTR_FOR_FN (cfun));
  while (worklist.length () > 0){
      basic_block bb = worklist.pop ();
      edge_iterator ei;
      edge e;

      FOR_EACH_EDGE (e, ei, bb->succs)
          if (BB_PARTITION (e->dest) != BB_COLD_PARTITION && !set->add (e->dest))
              worklist.safe_push (e->dest);
  }
}

/* Sanity check partition hotness to ensure that basic blocks in
   the cold partition don't dominate basic blocks in the hot partition.
   If FLAG_ONLY is true, report violations as errors. Otherwise
   re-mark the dominated blocks as cold, since this is run after
   cfg optimizations that may make hot blocks previously reached
   by both hot and cold blocks now only reachable along cold paths.  */

static auto_vec<basic_block> find_partition_fixes (MtcsCfgRtl *self,bool flag_only)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  basic_block bb;
  auto_vec<basic_block> bbs_to_fix;
  hash_set<basic_block> set;
  /* Callers check this.  */
  gcc_checking_assert (mtcsRtlData/*!crtl*/->has_bb_partition);
  mtcs_cfg_rtl_find_bbs_reachable_by_hot_paths/*!find_bbs_reachable_by_hot_paths*/(self,&set);
  FOR_EACH_BB_FN (bb, cfun)
    if (!set.contains (bb)  && BB_PARTITION (bb) != BB_COLD_PARTITION){
        if (flag_only)
          error ("non-cold basic block %d reachable only "
             "by paths crossing the cold partition", bb->index);
        else
          BB_SET_PARTITION (bb, BB_COLD_PARTITION);
        bbs_to_fix.safe_push (bb);
    }

  return bbs_to_fix;
}

/* Perform cleanup on the hot/cold bb partitioning after optimization
   passes that modify the cfg.  */
//原型 fixup_partitions cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_fixup_partitions (MtcsCfgRtl *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsCfgCleanup *mtcsCfgCleanup=mtcs_target_get_cfg_cleanup(mtcsTarget);
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  if (!mtcsRtlData/*!crtl*/->has_bb_partition)
    return;
  /* Delete any blocks that became unreachable and weren't
     already cleaned up, for example during edge forwarding
     and convert_jumps_to_returns. This will expose more
     opportunities for fixing the partition boundaries here.
     Also, the calculation of the dominance graph during verification
     will assert if there are unreachable nodes.  */
  mtcs_cfg_cleanup_delete_unreachable_blocks/*!delete_unreachable_blocks*/(mtcsCfgCleanup);
  /* If there are partitions, do a sanity check on them: A basic block in
     a cold partition cannot dominate a basic block in a hot partition.
     Fixup any that now violate this requirement, as a result of edge
     forwarding and unreachable block deletion.  */
  auto_vec<basic_block> bbs_to_fix = find_partition_fixes(self,false);
  /* Do the partition fixup after all necessary blocks have been converted to
     cold, so that we only update the region crossings the minimum number of
     places, which can require forcing edges to be non fallthru.  */
  if (! bbs_to_fix.is_empty ()){
      do{
          basic_block bb = bbs_to_fix.pop ();
          fixup_new_cold_bb(self,bb);
      }while (! bbs_to_fix.is_empty ());

      /* Fix up hot cold block grouping if needed.  */
      if (mtcsRtlData/*!crtl*/->bb_reorder_complete
            && mtcs_cfg_context_get_state/*!current_ir_type ()*/(mtcsCfgContext) == IR_RTL_CFGRTL){
          basic_block bb, first = NULL, second = NULL;
          int current_partition = BB_UNPARTITIONED;

          FOR_EACH_BB_FN (bb, cfun){
              if (current_partition != BB_UNPARTITIONED  && BB_PARTITION (bb) != current_partition){
                  if (first == NULL)
                    first = bb;
                  else if (second == NULL)
                    second = bb;
                  else{
                      /* If we switch partitions for the 3rd, 5th etc. time,
                      move bbs first (inclusive) .. second (exclusive) right
                      before bb.  */
                      basic_block prev_first = first->prev_bb;
                      basic_block prev_second = second->prev_bb;
                      basic_block prev_bb = bb->prev_bb;
                      prev_first->next_bb = second;
                      second->prev_bb = prev_first;
                      prev_second->next_bb = bb;
                      bb->prev_bb = prev_second;
                      prev_bb->next_bb = first;
                      first->prev_bb = prev_bb;
                      rtx_insn *prev_first_insn = PREV_INSN (BB_HEAD (first));
                      rtx_insn *prev_second_insn = PREV_INSN (BB_HEAD (second));
                      rtx_insn *prev_bb_insn = PREV_INSN (BB_HEAD (bb));
                      SET_NEXT_INSN (prev_first_insn) = BB_HEAD (second);
                      SET_PREV_INSN (BB_HEAD (second)) = prev_first_insn;
                      SET_NEXT_INSN (prev_second_insn) = BB_HEAD (bb);
                      SET_PREV_INSN (BB_HEAD (bb)) = prev_second_insn;
                      SET_NEXT_INSN (prev_bb_insn) = BB_HEAD (first);
                      SET_PREV_INSN (BB_HEAD (first)) = prev_bb_insn;
                      second = NULL;
                  }
              }
              current_partition = BB_PARTITION (bb);
          }
          gcc_assert (!second);
      }
  }
}

/* Verify, in the basic block chain, that there is at most one switch
   between hot/cold partitions. This condition will not be true until
   after reorder_basic_blocks is called.  */
static bool verify_hot_cold_block_grouping (MtcsCfgRtl *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  basic_block bb;
  bool err = false;
  bool switched_sections = false;
  int current_partition = BB_UNPARTITIONED;

  /* Even after bb reordering is complete, we go into cfglayout mode
     again (in compgoto). Ensure we don't call this before going back
     into linearized RTL when any layout fixes would have been committed.  */
  if (!mtcsRtlData/*!crtl*/->bb_reorder_complete
        || mtcs_cfg_context_get_state/*!current_ir_type ()*/(mtcsCfgContext) != IR_RTL_CFGRTL)
    return err;

  FOR_EACH_BB_FN (bb, cfun){
      if (current_partition != BB_UNPARTITIONED  && BB_PARTITION (bb) != current_partition){
          if (switched_sections){
              error ("multiple hot/cold transitions found (bb %i)",bb->index);
              err = true;
          }else
               switched_sections = true;

              if (!mtcsRtlData/*!crtl*/->has_bb_partition)
                error ("partition found but function partition flag not set");
      }
      current_partition = BB_PARTITION (bb);
  }

  return err;
}


/* Perform several checks on the edges out of each block, such as
   the consistency of the branch probabilities, the correctness
   of hot/cold partition crossing edges, and the number of expected
   successor edges.  Also verify that the dominance relationship
   between hot/cold blocks is sane.  */

static bool rtl_verify_edges (MtcsCfgRtl *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  bool err = false;
  basic_block bb;

  FOR_EACH_BB_REVERSE_FN (bb, cfun){
      int n_fallthru = 0, n_branch = 0, n_abnormal_call = 0, n_sibcall = 0;
      int n_eh = 0, n_abnormal = 0;
      edge e, fallthru = NULL;
      edge_iterator ei;
      rtx note;
      bool has_crossing_edge = false;

      if (JUMP_P (BB_END (bb))
        && (note = find_reg_note (BB_END (bb), REG_BR_PROB, NULL_RTX))
        && EDGE_COUNT (bb->succs) >= 2
        && any_condjump_p (BB_END (bb))){
          if (!BRANCH_EDGE (bb)->probability.initialized_p ()){
              if (profile_status_for_fn (cfun) != PROFILE_ABSENT){
                  error ("verify_flow_info: REG_BR_PROB is set but cfg probability is not");
                  err = true;
              }
          }else if (XINT (note, 0) != BRANCH_EDGE (bb)->probability.to_reg_br_prob_note ()
                   && profile_status_for_fn (cfun) != PROFILE_ABSENT){
              error ("verify_flow_info: REG_BR_PROB does not match cfg %i %i",
                 XINT (note, 0),BRANCH_EDGE (bb)->probability.to_reg_br_prob_note ());
              err = true;
          }
      }

      FOR_EACH_EDGE (e, ei, bb->succs){
          bool is_crossing;

          if (e->flags & EDGE_FALLTHRU)
            n_fallthru++, fallthru = e;

          is_crossing = (BB_PARTITION (e->src) != BB_PARTITION (e->dest)
                 && e->src != ENTRY_BLOCK_PTR_FOR_FN (cfun)
                 && e->dest != EXIT_BLOCK_PTR_FOR_FN (cfun));
              has_crossing_edge |= is_crossing;
          if (e->flags & EDGE_CROSSING){
              if (!is_crossing){
                  error ("EDGE_CROSSING incorrectly set across same section");
                  err = true;
              }
              if (e->flags & EDGE_FALLTHRU){
                  error ("fallthru edge crosses section boundary in bb %i",e->src->index);
                  err = true;
              }
              if (e->flags & EDGE_EH){
                  error ("EH edge crosses section boundary in bb %i",e->src->index);
                  err = true;
              }
              if (JUMP_P (BB_END (bb)) && !CROSSING_JUMP_P (BB_END (bb))){
                  error ("No region crossing jump at section boundary in bb %i",bb->index);
                  err = true;
              }
          }else if (is_crossing){
              error ("EDGE_CROSSING missing across section boundary");
              err = true;
          }

          if ((e->flags & ~(EDGE_DFS_BACK
                    | EDGE_CAN_FALLTHRU
                    | EDGE_IRREDUCIBLE_LOOP
                    | EDGE_LOOP_EXIT
                    | EDGE_CROSSING
                    | EDGE_PRESERVE)) == 0)
            n_branch++;

          if (e->flags & EDGE_ABNORMAL_CALL)
            n_abnormal_call++;

          if (e->flags & EDGE_SIBCALL)
            n_sibcall++;

          if (e->flags & EDGE_EH)
            n_eh++;

          if (e->flags & EDGE_ABNORMAL)
            n_abnormal++;
      }

      if (!has_crossing_edge  && JUMP_P (BB_END (bb))  && CROSSING_JUMP_P (BB_END (bb))){
          mtcs_cfg_rtl_print_rtl_with_bb/*!print_rtl_with_bb*/(self,stderr,
                  mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData), TDF_BLOCKS | TDF_DETAILS);
          error ("Region crossing jump across same section in bb %i", bb->index);
          err = true;
      }

      if (n_eh && !find_reg_note (BB_END (bb), REG_EH_REGION, NULL_RTX)){
          error ("missing REG_EH_REGION note at the end of bb %i", bb->index);
          err = true;
      }
      if (n_eh > 1){
          error ("too many exception handling edges in bb %i", bb->index);
          err = true;
      }
      if (n_branch && (!JUMP_P (BB_END (bb))
          || (n_branch > 1 && (any_uncondjump_p (BB_END (bb))
                   || any_condjump_p (BB_END (bb)))))){
          error ("too many outgoing branch edges from bb %i", bb->index);
          err = true;
      }
      if (n_fallthru && any_uncondjump_p (BB_END (bb))){
          error ("fallthru edge after unconditional jump in bb %i", bb->index);
          err = true;
      }
      if (n_branch != 1 && any_uncondjump_p (BB_END (bb))){
          error ("mtcscfgstate.c wrong number of branch edges after unconditional jump in bb %i", bb->index);
          err = true;
      }
      if (n_branch != 1 && any_condjump_p (BB_END (bb))
        && JUMP_LABEL (BB_END (bb)) != BB_HEAD (fallthru->dest)){
          error ("wrong amount of branch edges after conditional jump in bb %i", bb->index);
          err = true;
      }
      if (n_abnormal_call && !CALL_P (BB_END (bb))){
          error ("abnormal call edges for non-call insn in bb %i", bb->index);
          err = true;
      }
      if (n_sibcall && !CALL_P (BB_END (bb))){
          error ("sibcall edges for non-call insn in bb %i", bb->index);
          err = true;
      }
      if (n_abnormal > n_eh  && !(CALL_P (BB_END (bb))
           && n_abnormal == n_abnormal_call + n_sibcall)
           && (!JUMP_P (BB_END (bb)) || any_condjump_p (BB_END (bb)) || any_uncondjump_p (BB_END (bb))))
      {
          error ("abnormal edges for no purpose in bb %i", bb->index);
          err = true;
      }

      int has_eh = -1;
      FOR_EACH_EDGE (e, ei, bb->preds){
          if (has_eh == -1)
            has_eh = (e->flags & EDGE_EH);
          if ((e->flags & EDGE_EH) == has_eh)
            continue;
          error ("EH incoming edge mixed with non-EH incoming edges in bb %i", bb->index);
          err = true;
          break;
      }
  }

  /* If there are partitions, do a sanity check on them: A basic block in
     a cold partition cannot dominate a basic block in a hot partition.  */
  if (mtcsRtlData/*!crtl*/->has_bb_partition && !err
        && mtcs_cfg_context_get_state/*!current_ir_type ()*/(mtcsCfgContext) == IR_RTL_CFGLAYOUT){
      auto_vec<basic_block> bbs_to_fix = find_partition_fixes(self,true);
      err = !bbs_to_fix.is_empty ();
  }
  /* Clean up.  */
  return err;
}

/* Checks on the instructions within blocks. Currently checks that each
   block starts with a basic block note, and that basic block notes and
   control flow jumps are not found in the middle of the block.  */
static bool rtl_verify_bb_insns (MtcsCfgRtl *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgBuild *mtcsCfgBuild=mtcs_target_get_cfg_build(mtcsTarget);


  rtx_insn *x;
  bool err = false;
  basic_block bb;

  FOR_EACH_BB_REVERSE_FN (bb, cfun){
      /* Now check the header of basic
     block.  It ought to contain optional CODE_LABEL followed
     by NOTE_BASIC_BLOCK.  */
      x = BB_HEAD (bb);
      if (LABEL_P (x)){
          if (BB_END (bb) == x){
              error ("NOTE_INSN_BASIC_BLOCK is missing for block %d",bb->index);
              err = true;
          }
          x = NEXT_INSN (x);
      }
      if (!NOTE_INSN_BASIC_BLOCK_P (x) || NOTE_BASIC_BLOCK (x) != bb){
          error ("NOTE_INSN_BASIC_BLOCK is missing for block %d",bb->index);
          err = true;
      }
      if (BB_END (bb) == x)
        /* Do checks for empty blocks here.  */
        ;
      else
        for (x = NEXT_INSN (x); x; x = NEXT_INSN (x)){
            if (NOTE_INSN_BASIC_BLOCK_P (x)){
                error ("NOTE_INSN_BASIC_BLOCK %d in middle of basic block %d",INSN_UID (x), bb->index);
                err = true;
            }
            if (x == BB_END (bb))
              break;

            if (mtcs_cfg_build_control_flow_insn_p/*!control_flow_insn_p*/(mtcsCfgBuild,x)){
                error ("in basic block %d:", bb->index);
                fatal_insn ("flow control insn inside a basic block", x);
            }
        }
  }
  /* Clean up.  */
  return err;
}

/* Verify that block pointers for instructions in basic blocks, headers and
   footers are set appropriately.  */
static bool rtl_verify_bb_pointers (MtcsCfgRtl *self)
{
  bool err = false;
  basic_block bb;
  /* Check the general integrity of the basic blocks.  */
  FOR_EACH_BB_REVERSE_FN (bb, cfun){
      rtx_insn *insn;
      if (!(bb->flags & BB_RTL)){
          error ("BB_RTL flag not set for block %d", bb->index);
          err = true;
      }

      FOR_BB_INSNS (bb, insn)
          if (BLOCK_FOR_INSN (insn) != bb){
              error ("insn %d basic block pointer is %d, should be %d",  INSN_UID (insn),
                    BLOCK_FOR_INSN (insn) ? BLOCK_FOR_INSN (insn)->index : 0, bb->index);
              err = true;
          }

      for (insn = BB_HEADER (bb); insn; insn = NEXT_INSN (insn))
          if (!BARRIER_P (insn) && BLOCK_FOR_INSN (insn) != NULL){
              error ("insn %d in header of bb %d has non-NULL basic block",
                 INSN_UID (insn), bb->index);
              err = true;
         }
      for (insn = BB_FOOTER (bb); insn; insn = NEXT_INSN (insn))
          if (!BARRIER_P (insn)  && BLOCK_FOR_INSN (insn) != NULL){
              error ("insn %d in footer of bb %d has non-NULL basic block",
               INSN_UID (insn), bb->index);
              err = true;
          }
  }
  /* Clean up.  */
  return err;
}

/* Verify that blocks are laid out in consecutive order. While walking the
   instructions, verify that all expected instructions are inside the basic
   blocks, and that all returns are followed by barriers.  */
static bool rtl_verify_bb_layout (MtcsCfgRtl *self)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
    MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  basic_block bb;
  bool err = false;
  rtx_insn *x, *y;
  int num_bb_notes;
  rtx_insn * const rtx_first = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
  basic_block last_bb_seen = ENTRY_BLOCK_PTR_FOR_FN (cfun), curr_bb = NULL;
  num_bb_notes = 0;
  for (x = rtx_first; x; x = NEXT_INSN (x)){
      if (NOTE_INSN_BASIC_BLOCK_P (x)){
          bb = NOTE_BASIC_BLOCK (x);
          num_bb_notes++;
          if (bb != last_bb_seen->next_bb)
              internal_error ("basic blocks not laid down consecutively");
          curr_bb = last_bb_seen = bb;
      }
      if (!curr_bb){
          switch (GET_CODE (x)){
            case BARRIER:
            case NOTE:
              break;
            case CODE_LABEL:
              /* An ADDR_VEC is placed outside any basic block.  */
              if (NEXT_INSN (x) && JUMP_TABLE_DATA_P (NEXT_INSN (x)))
                  x = NEXT_INSN (x);
              /* But in any case, non-deletable labels can appear anywhere.  */
              break;
            default:
              fatal_insn ("insn outside basic block", x);
         }
      }

      if (JUMP_P (x)  && returnjump_p (x) && ! condjump_p (x)
        && ! ((y = next_nonnote_nondebug_insn (x))  && BARRIER_P (y)))
        fatal_insn ("return not followed by barrier", x);

      if (curr_bb && x == BB_END (curr_bb))
          curr_bb = NULL;
  }

  if (num_bb_notes != n_basic_blocks_for_fn (cfun) - NUM_FIXED_BLOCKS)
    internal_error("number of bb notes in insn chain (%d) != n_basic_blocks (%d)",
       num_bb_notes, n_basic_blocks_for_fn (cfun));

   return err;
}

/* Assume that the preceding pass has possibly eliminated jump instructions
   or converted the unconditional jumps.  Eliminate the edges from CFG.
   Return true if any edges are eliminated.  */
//原型 purge_dead_edges cfgrtl.h cfgrtl.cc
bool mtcs_cfg_rtl_purge_dead_edges (MtcsCfgRtl *self,basic_block bb)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
  MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal =mtcs_target_get_rtlanal(mtcsTarget);
  MtcsExcept *mtcsExcept =mtcs_target_get_except(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *opts=mtcsOptions->global_options;

  edge e;
  rtx_insn *insn = BB_END (bb);
  rtx note;
  bool purged = false;
  bool found;
  edge_iterator ei;
  n_debug("mtcscfgrtl.c mtcs_cfg_rtl_purge_dead_edges 00 bb:%p lenght:%d\n",bb,EDGE_COUNT (bb->succs));
  if ((DEBUG_INSN_P (insn) || NOTE_P (insn)) && insn != BB_HEAD (bb))
    do
      insn = PREV_INSN (insn);
    while ((DEBUG_INSN_P (insn) || NOTE_P (insn)) && insn != BB_HEAD (bb));

  /* If this instruction cannot trap, remove REG_EH_REGION notes.  */
  if (NONJUMP_INSN_P (insn)  && (note = find_reg_note (insn, REG_EH_REGION, NULL))){
      rtx eqnote;
      if (! mtcs_rtlanal_may_trap_p/*!may_trap_p*/(mtcsRtlanal,PATTERN (insn)) || ((eqnote = find_reg_equal_equiv_note (insn))
          && ! mtcs_rtlanal_may_trap_p/*!may_trap_p*/(mtcsRtlanal,XEXP (eqnote, 0)))){
         n_debug("mtcscfgrtl.c mtcs_cfg_rtl_purge_dead_edges 11 remove_note bb:%p\n",bb);
          mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,insn, note);
      }
  }
  /* Cleanup abnormal edges caused by exceptions or non-local gotos.  */
  for (ei = ei_start (bb->succs); (e = ei_safe_edge (ei)); ){
      bool remove = false;

      /* There are three types of edges we need to handle correctly here: EH
     edges, abnormal call EH edges, and abnormal call non-EH edges.  The
     latter can appear when nonlocal gotos are used.  */
      if (e->flags & EDGE_ABNORMAL_CALL){
         n_debug("mtcscfgrtl.c mtcs_cfg_rtl_purge_dead_edges 22  ei_start (bb->succs) bb:%p\n",bb);

          if (!CALL_P (insn))
            remove = true;
          else if (mtcs_except_can_nonlocal_goto/*!can_nonlocal_goto*/(mtcsExcept,insn))
            ;
          else if ((e->flags & EDGE_EH) && can_throw_internal (insn))
            ;
          else if (flag_tm && find_reg_note (insn, REG_TM, NULL))
            ;
          else
            remove = true;
      }else if (e->flags & EDGE_EH){
         n_debug("mtcscfgrtl.c mtcs_cfg_rtl_purge_dead_edges 33  ei_start (bb->succs) bb:%p\n",bb);

          remove = !can_throw_internal (insn);
      }

      if (remove){
         n_debug("mtcscfgrtl.c mtcs_cfg_rtl_purge_dead_edges 44  ei_start (bb->succs) bb:%p %d\n",bb,EDGE_COUNT (bb->succs));
         mtcs_cfg_context_remove_edge/*!remove_edge*/(mtcsCfgContext,e);
          mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,bb);
          purged = true;
      }else
          ei_next (&ei);
  }

  if (JUMP_P (insn)){
     n_debug("mtcscfgrtl.c mtcs_cfg_rtl_purge_dead_edges 55  JUMP_P (insn) bb:%p %d\n",bb,EDGE_COUNT (bb->succs));
      rtx note;
      edge b,f;
      edge_iterator ei;
      /* We do care only about conditional jumps and simplejumps.  */
      if (!any_condjump_p (insn) && !returnjump_p (insn) && !simplejump_p (insn))
          return purged;
      n_debug("mtcscfgrtl.c mtcs_cfg_rtl_purge_dead_edges 66  JUMP_P (insn) bb:%p %d\n",bb,EDGE_COUNT (bb->succs));

      /* Branch probability/prediction notes are defined only for
     condjumps.  We've possibly turned condjump into simplejump.  */
      if (simplejump_p (insn)){
          note = find_reg_note (insn, REG_BR_PROB, NULL);
          if (note)
            mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,insn, note);
          while ((note = find_reg_note (insn, REG_BR_PRED, NULL)))
            mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,insn, note);
      }
      n_debug("mtcscfgrtl.c mtcs_cfg_rtl_purge_dead_edges 77  JUMP_P (insn) bb:%p %d\n",bb,EDGE_COUNT (bb->succs));

      for (ei = ei_start (bb->succs); (e = ei_safe_edge (ei)); ){
          /* Avoid abnormal flags to leak from computed jumps turned
             into simplejumps.  */
          e->flags &= ~EDGE_ABNORMAL;
          /* See if this edge is one we should keep.  */
          if ((e->flags & EDGE_FALLTHRU) && any_condjump_p (insn))
            /* A conditional jump can fall through into the next
               block, so we should keep the edge.  */
          {
              ei_next (&ei);
              continue;
          }else if (e->dest != EXIT_BLOCK_PTR_FOR_FN (cfun)
               && BB_HEAD (e->dest) == JUMP_LABEL (insn))
            /* If the destination block is the target of the jump,
               keep the edge.  */
          {
              ei_next (&ei);
              continue;
          }else if (e->dest == EXIT_BLOCK_PTR_FOR_FN (cfun)
               && returnjump_p (insn))
            /* If the destination block is the exit block, and this
               instruction is a return, then keep the edge.  */
           {
              ei_next (&ei);
              continue;
          }else if ((e->flags & EDGE_EH) && can_throw_internal (insn))
            /* Keep the edges that correspond to exceptions thrown by
               this instruction and rematerialize the EDGE_ABNORMAL
               flag we just cleared above.  */
          {
              e->flags |= EDGE_ABNORMAL;
              ei_next (&ei);
              continue;
          }
           /* We do not need this edge.  */
          mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,bb);
          purged = true;
          mtcs_cfg_context_remove_edge/*!remove_edge*/(mtcsCfgContext,e);
      }
      if (EDGE_COUNT (bb->succs) == 0 || !purged)
          return purged;
      if (dump_file)
          fprintf (dump_file, "Purged edges from bb %i\n", bb->index);
      if (!opts->x_optimize)
          return purged;

      /* Redistribute probabilities.  */
      if (single_succ_p (bb)){
          single_succ_edge (bb)->probability = profile_probability::always ();
      }else{
          note = find_reg_note (insn, REG_BR_PROB, NULL);
          if (!note)
              return purged;
          b = BRANCH_EDGE (bb);
          f = FALLTHRU_EDGE (bb);
          b->probability = profile_probability::from_reg_br_prob_note
                         (XINT (note, 0));
          f->probability = b->probability.invert ();
      }
      return purged;
  }else if (CALL_P (insn) && SIBLING_CALL_P (insn)){
     n_debug("mtcscfgrtl.c mtcs_cfg_rtl_purge_dead_edges 88 bb:%p %d\n",bb,EDGE_COUNT (bb->succs));

      /* First, there should not be any EH or ABCALL edges resulting
     from non-local gotos and the like.  If there were, we shouldn't
     have created the sibcall in the first place.  Second, there
     should of course never have been a fallthru edge.  */
      gcc_assert (single_succ_p (bb));
      gcc_assert (single_succ_edge (bb)->flags == (EDGE_SIBCALL | EDGE_ABNORMAL));
      return false;
  }
  /* If we don't see a jump insn, we don't know exactly why the block would
     have been broken at this point.  Look for a simple, non-fallthru edge,
     as these are only created by conditional branches.  If we find such an
     edge we know that there used to be a jump here and can then safely
     remove all non-fallthru edges.  */
  found = false;
  FOR_EACH_EDGE (e, ei, bb->succs)
      if (! (e->flags & (EDGE_COMPLEX | EDGE_FALLTHRU))){
          found = true;
          break;
      }
  n_debug("mtcscfgrtl.c mtcs_cfg_rtl_purge_dead_edges 99 bb:%p %d purged:%d found:%d\n",bb,EDGE_COUNT (bb->succs),purged,found);

  if (!found)
      return purged;
  /* Remove all but the fake and fallthru edges.  The fake edge may be
     the only successor for this block in the case of noreturn
     calls.  */
  for (ei = ei_start (bb->succs); (e = ei_safe_edge (ei)); ){
      if (!(e->flags & (EDGE_FALLTHRU | EDGE_FAKE))){
          mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,bb);
          mtcs_cfg_context_remove_edge/*!remove_edge*/(mtcsCfgContext,e);
          purged = true;
      } else
          ei_next (&ei);
  }
  n_debug("mtcscfgrtl.c mtcs_cfg_rtl_purge_dead_edges 100 bb:%p %d purged:%d\n",bb,EDGE_COUNT (bb->succs),purged);

  gcc_assert (single_succ_p (bb));
  single_succ_edge (bb)->probability = profile_probability::always ();
  if (dump_file)
    fprintf (dump_file, "Purged non-fallthru edges from bb %i\n",  bb->index);
  return purged;
}

/* Search all basic blocks for potentially dead edges and purge them.  Return
   true if some edge has been eliminated.  */
//原型 purge_all_dead_edges cfgrtl.h cfgrtl.cc
bool mtcs_cfg_rtl_purge_all_dead_edges (MtcsCfgRtl *self)
{
  bool purged = false;
  basic_block bb;
  FOR_EACH_BB_FN (bb, cfun)
      if (mtcs_cfg_rtl_purge_dead_edges/*!purge_dead_edges*/(self,bb))
          purged = true;
  return purged;
}

/* This is used by a few passes that emit some instructions after abnormal
   calls, moving the basic block's end, while they in fact do want to emit
   them on the fallthru edge.  Look for abnormal call edges, find backward
   the call in the block and insert the instructions on the edge instead.

   Similarly, handle instructions throwing exceptions internally.

   Return true when instructions have been found and inserted on edges.  */
//原型 fixup_abnormal_edges cfgrtl.h cfgrtl.cc
bool mtcs_cfg_rtl_fixup_abnormal_edges (MtcsCfgRtl *self)
{
  bool inserted = false;
  basic_block bb;
  FOR_EACH_BB_FN (bb, cfun){
      edge e;
      edge_iterator ei;
      /* Look for cases we are interested in - calls or instructions causing
         exceptions.  */
      FOR_EACH_EDGE (e, ei, bb->succs)
          if ((e->flags & EDGE_ABNORMAL_CALL)
            || ((e->flags & (EDGE_ABNORMAL | EDGE_EH))  == (EDGE_ABNORMAL | EDGE_EH)))
          break;

      if (e && !CALL_P (BB_END (bb)) && !can_throw_internal (BB_END (bb))){
          rtx_insn *insn;
          /* Get past the new insns generated.  Allow notes, as the insns
             may be already deleted.  */
          insn = BB_END (bb);
          while ((NONJUMP_INSN_P (insn) || NOTE_P (insn))
             && !can_throw_internal (insn)  && insn != BB_HEAD (bb))
            insn = PREV_INSN (insn);

          if (CALL_P (insn) || can_throw_internal (insn)){
              rtx_insn *stop, *next;
              e = find_fallthru_edge (bb->succs);
              stop = NEXT_INSN (BB_END (bb));
              BB_END (bb) = insn;
              for (insn = NEXT_INSN (insn); insn != stop; insn = next){
                  next = NEXT_INSN (insn);
                  if (INSN_P (insn)){
                      mtcs_cfg_rtl_delete_insn/*!delete_insn*/(self,insn);

                      /* Sometimes there's still the return value USE.
                     If it's placed after a trapping call (i.e. that
                     call is the last insn anyway), we have no fallthru
                     edge.  Simply delete this use and don't try to insert
                     on the non-existent edge.
                     Similarly, sometimes a call that can throw is
                     followed in the source with __builtin_unreachable (),
                     meaning that there is UB if the call returns rather
                     than throws.  If there weren't any instructions
                     following such calls before, supposedly even the ones
                     we've deleted aren't significant and can be
                     removed.  */
                      if (e){
                          /* We're not deleting it, we're moving it.  */
                          insn->set_undeleted ();
                          SET_PREV_INSN (insn) = NULL_RTX;
                          SET_NEXT_INSN (insn) = NULL_RTX;

                          mtcs_cfg_rtl_insert_insn_on_edge/*!insert_insn_on_edge*/(self,insn, e);
                          inserted = true;
                      }
                  }else if (!BARRIER_P (insn))
                    set_block_for_insn (insn, NULL);
              }
          }

          /* It may be that we don't find any trapping insn.  In this
             case we discovered quite late that the insn that had been
             marked as can_throw_internal in fact couldn't trap at all.
             So we should in fact delete the EH edges out of the block.  */
          else
              mtcs_cfg_rtl_purge_dead_edges/*!purge_dead_edges*/(self,bb);
      }
  }

  return inserted;
}

/* Delete the unconditional jump INSN and adjust the CFG correspondingly.
   Note that the INSN should be deleted *after* removing dead edges, so
   that the kept edge is the fallthrough edge for a (set (pc) (pc))
   but not for a (set (pc) (label_ref FOO)).  */
//原型 update_cfg_for_uncondjump cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_update_cfg_for_uncondjump (MtcsCfgRtl *self,rtx_insn *insn)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);

  basic_block bb = BLOCK_FOR_INSN (insn);
  gcc_assert (BB_END (bb) == insn);

  mtcs_cfg_rtl_purge_dead_edges/*!purge_dead_edges*/(self,bb);

  if (mtcs_cfg_context_get_state/*!current_ir_type ()*/(mtcsCfgContext) != IR_RTL_CFGLAYOUT){
      if (!find_fallthru_edge (bb->succs)){
          auto barrier = next_nonnote_nondebug_insn (insn);
          if (!barrier || !BARRIER_P (barrier))
             mtcs_emit_emit_barrier_after/*!emit_barrier_after*/(mtcsEmit,insn);
      }
      return;
  }

  mtcs_cfg_rtl_delete_insn/*!delete_insn*/(self,insn);
  if (EDGE_COUNT (bb->succs) == 1){
      rtx_insn *insn;
      single_succ_edge (bb)->flags |= EDGE_FALLTHRU;
      /* Remove barriers from the footer if there are any.  */
      for (insn = BB_FOOTER (bb); insn; insn = NEXT_INSN (insn))
          if (BARRIER_P (insn)){
              if (PREV_INSN (insn))
                  SET_NEXT_INSN (PREV_INSN (insn)) = NEXT_INSN (insn);
              else
                  BB_FOOTER (bb) = NEXT_INSN (insn);
              if (NEXT_INSN (insn))
                  SET_PREV_INSN (NEXT_INSN (insn)) = PREV_INSN (insn);
          }else if (LABEL_P (insn))
              break;
  }
}

/* Cut the insns from FIRST to LAST out of the insns stream.  */
//原型 unlink_insn_chain cfgrtl.h cfgrtl.cc
rtx_insn * mtcs_cfg_rtl_unlink_insn_chain (MtcsCfgRtl *self,rtx_insn *first, rtx_insn *last)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  rtx_insn *prevfirst = PREV_INSN (first);
  rtx_insn *nextlast = NEXT_INSN (last);
  SET_PREV_INSN (first) = NULL;
  SET_NEXT_INSN (last) = NULL;
  if (prevfirst)
    SET_NEXT_INSN (prevfirst) = nextlast;
  if (nextlast)
    SET_PREV_INSN (nextlast) = prevfirst;
  else{
      n_debug("mtcscfgrtl.c mtcs_cfg_rtl_unlink_insn_chain %p\n",last);
      mtcs_rtl_data_set_last_insn/*!set_last_insn*/(mtcsRtlData,prevfirst);
  }
  if (!prevfirst)
      mtcs_rtl_data_set_first_insn/*!set_first_insn*/(mtcsRtlData,nextlast);
  return first;
}

/* Skip over inter-block insns occurring after BB which are typically
   associated with BB (e.g., barriers). If there are any such insns,
   we return the last one. Otherwise, we return the end of BB.  */

static rtx_insn *skip_insns_after_block (MtcsCfgRtl *self,basic_block bb)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  rtx_insn *insn, *last_insn, *next_head, *prev;
  next_head = NULL;
  if (bb->next_bb != EXIT_BLOCK_PTR_FOR_FN (cfun))
    next_head = BB_HEAD (bb->next_bb);
  for (last_insn = insn = BB_END (bb); (insn = NEXT_INSN (insn)) != 0; ){
      if (insn == next_head)
          break;

      switch (GET_CODE (insn)){
        case BARRIER:
          last_insn = insn;
          continue;

        case NOTE:
          gcc_assert (NOTE_KIND (insn) != NOTE_INSN_BLOCK_END);
          continue;

        case CODE_LABEL:
          if (NEXT_INSN (insn)  && JUMP_TABLE_DATA_P (NEXT_INSN (insn))){
              insn = NEXT_INSN (insn);
              last_insn = insn;
              continue;
          }
          break;

        default:
          break;
      }
      break;
  }
  /* It is possible to hit contradictory sequence.  For instance:

     jump_insn
     NOTE_INSN_BLOCK_BEG
     barrier
     Where barrier belongs to jump_insn, but the note does not.  This can be
     created by removing the basic block originally following
     NOTE_INSN_BLOCK_BEG.  In such case reorder the notes.  */
  for (insn = last_insn; insn != BB_END (bb); insn = prev){
      prev = PREV_INSN (insn);
      if (NOTE_P (insn))
          switch (NOTE_KIND (insn)){
              case NOTE_INSN_BLOCK_END:
                  gcc_unreachable ();
                  break;
              case NOTE_INSN_DELETED:
              case NOTE_INSN_DELETED_LABEL:
              case NOTE_INSN_DELETED_DEBUG_LABEL:
                  continue;
              default:
                 mtcs_rtl_reorder_insns/*!reorder_insns*/(mtcsRTL,insn, insn, last_insn);
          }
  }
  return last_insn;
}

/* Locate or create a label for a given basic block.  */
static rtx_insn *label_for_bb (MtcsCfgRtl *self,basic_block bb)
{
  rtx_insn *label = BB_HEAD (bb);
  if (!LABEL_P (label)){
      if (dump_file)
          fprintf (dump_file, "Emitting label for block %d\n", bb->index);
      label = mtcs_cfg_rtl_block_label/*!block_label*/(self,bb);
  }
  return label;
}

/* Locate the effective beginning and end of the insn chain for each
   block, as defined by skip_insns_after_block above.  */
static void record_effective_endpoints (MtcsCfgRtl *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  rtx_insn *next_insn;
  basic_block bb;
  rtx_insn *insn;

  for (insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
       insn  && NOTE_P (insn) && NOTE_KIND (insn) != NOTE_INSN_BASIC_BLOCK;
       insn = NEXT_INSN (insn))
    continue;
  /* No basic blocks at all?  */
  gcc_assert (insn);

  if (PREV_INSN (insn))
      self->cfg_layout_function_header = mtcs_cfg_rtl_unlink_insn_chain/*!unlink_insn_chain*/(self,
              mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData), PREV_INSN (insn));
  else
      self->cfg_layout_function_header = NULL;

  next_insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
  FOR_EACH_BB_FN (bb, cfun){
      rtx_insn *end;
      if (PREV_INSN (BB_HEAD (bb)) && next_insn != BB_HEAD (bb))
          BB_HEADER (bb) =mtcs_cfg_rtl_unlink_insn_chain/*!unlink_insn_chain*/(self,
            next_insn, PREV_INSN (BB_HEAD (bb)));
      end = skip_insns_after_block(self,bb);
      if (NEXT_INSN (BB_END (bb)) && BB_END (bb) != end)
          BB_FOOTER (bb) = mtcs_cfg_rtl_unlink_insn_chain/*!unlink_insn_chain*/(self,
                  NEXT_INSN (BB_END (bb)), end);
      next_insn = NEXT_INSN (BB_END (bb));
  }

  self->cfg_layout_function_footer = next_insn;
  if (self->cfg_layout_function_footer)
      self->cfg_layout_function_footer = mtcs_cfg_rtl_unlink_insn_chain/*!unlink_insn_chain*/(self,
              self->cfg_layout_function_footer, mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData));
}

/* Link the basic blocks in the correct order, compacting the basic
   block queue while at it.  If STAY_IN_CFGLAYOUT_MODE is false, this
   function also clears the basic block header and footer fields.

   This function is usually called after a pass (e.g. tracer) finishes
   some transformations while in cfglayout mode.  The required sequence
   of the basic blocks is in a linked list along the bb->aux field.
   This functions re-links the basic block prev_bb and next_bb pointers
   accordingly, and it compacts and renumbers the blocks.

   FIXME: This currently works only for RTL, but the only RTL-specific
   bits are the STAY_IN_CFGLAYOUT_MODE bits.  The tracer pass was moved
   to GIMPLE a long time ago, but it doesn't relink the basic block
   chain.  It could do that (to give better initial RTL) if this function
   is made IR-agnostic (and moved to cfganal.cc or cfg.cc while at it).  */
//原型 relink_block_chain cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_relink_block_chain (MtcsCfgRtl *self,bool stay_in_cfglayout_mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsCfg *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);

  basic_block bb, prev_bb;
  int index;
  /* Maybe dump the re-ordered sequence.  */
  if (dump_file){
      fprintf (dump_file, "Reordered sequence:\n");
      for (bb = ENTRY_BLOCK_PTR_FOR_FN (cfun)->next_bb, index =  NUM_FIXED_BLOCKS;
       bb;  bb = (basic_block) bb->aux, index++){
          fprintf (dump_file, " %i ", index);
          if (mtcs_cfg_get_bb_original/*!get_bb_original*/(mtcsCfg,bb))
            fprintf (dump_file, "duplicate of %i\n", mtcs_cfg_get_bb_original/*!get_bb_original*/(mtcsCfg,bb)->index);
          else if (forwarder_block_p (bb) && !LABEL_P (BB_HEAD (bb)))
            fprintf (dump_file, "compensation\n");
          else
            fprintf (dump_file, "bb %i\n", bb->index);
      }
  }

  /* Now reorder the blocks.  */
  prev_bb = ENTRY_BLOCK_PTR_FOR_FN (cfun);
  bb = ENTRY_BLOCK_PTR_FOR_FN (cfun)->next_bb;
  for (; bb; prev_bb = bb, bb = (basic_block) bb->aux){
      bb->prev_bb = prev_bb;
      prev_bb->next_bb = bb;
  }
  prev_bb->next_bb = EXIT_BLOCK_PTR_FOR_FN (cfun);
  EXIT_BLOCK_PTR_FOR_FN (cfun)->prev_bb = prev_bb;

  /* Then, clean up the aux fields.  */
  FOR_ALL_BB_FN (bb, cfun){
      bb->aux = NULL;
      if (!stay_in_cfglayout_mode)
          BB_HEADER (bb) = BB_FOOTER (bb) = NULL;
  }
  /* Maybe reset the original copy tables, they are not valid anymore
     when we renumber the basic blocks in compact_blocks.  If we are
     are going out of cfglayout mode, don't re-allocate the tables.  */
  if (mtcs_cfg_original_copy_tables_initialized_p/*!original_copy_tables_initialized_p*/(mtcsCfg))
     mtcs_cfg_free_original_copy_tables/*!free_original_copy_tables*/(mtcsCfg);
  if (stay_in_cfglayout_mode)
     mtcs_cfg_initialize_original_copy_tables/*!initialize_original_copy_tables*/(mtcsCfg);
  /* Finally, put basic_block_info in the new order.  */
  mtcs_cfg_compact_blocks/*!compact_blocks*/(mtcsCfg);
}


/* Given a reorder chain, rearrange the code to match.  */
static void fixup_reorder_chain (MtcsCfgRtl *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
  MtcsCfgCleanup *mtcsCfgCleanup=mtcs_target_get_cfg_cleanup(mtcsTarget);
  MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  basic_block bb;
  rtx_insn *insn = NULL;
  if (self->cfg_layout_function_header){
      mtcs_rtl_data_set_first_insn/*!set_first_insn*/(mtcsRtlData,self->cfg_layout_function_header);
      insn = self->cfg_layout_function_header;
      while (NEXT_INSN (insn))
          insn = NEXT_INSN (insn);
  }
  /* First do the bulk reordering -- rechain the blocks without regard to
     the needed changes to jumps and labels.  */
  for (bb = ENTRY_BLOCK_PTR_FOR_FN (cfun)->next_bb; bb; bb = (basic_block)bb->aux){
      if (BB_HEADER (bb)){
          if (insn)
              SET_NEXT_INSN (insn) = BB_HEADER (bb);
          else
              mtcs_rtl_data_set_first_insn/*!set_first_insn*/(mtcsRtlData,BB_HEADER (bb));
          SET_PREV_INSN (BB_HEADER (bb)) = insn;
          insn = BB_HEADER (bb);
          while (NEXT_INSN (insn))
            insn = NEXT_INSN (insn);
      }
      if (insn)
          SET_NEXT_INSN (insn) = BB_HEAD (bb);
      else
          mtcs_rtl_data_set_first_insn/*!set_first_insn*/(mtcsRtlData,BB_HEAD (bb));
      SET_PREV_INSN (BB_HEAD (bb)) = insn;
      insn = BB_END (bb);
      if (BB_FOOTER (bb)){
          SET_NEXT_INSN (insn) = BB_FOOTER (bb);
          SET_PREV_INSN (BB_FOOTER (bb)) = insn;
          while (NEXT_INSN (insn))
            insn = NEXT_INSN (insn);
      }
  }

  SET_NEXT_INSN (insn) = self->cfg_layout_function_footer;
  if (self->cfg_layout_function_footer)
    SET_PREV_INSN (self->cfg_layout_function_footer) = insn;

  while (NEXT_INSN (insn))
    insn = NEXT_INSN (insn);

  n_debug("mtcscfgrtl.c fixup_reorder_chain %p\n",insn);
  mtcs_rtl_data_set_last_insn/*!set_last_insn*/(mtcsRtlData,insn);
  if (mtcsOptionsItem->x_flag_checking)
    verify_insn_chain(self);
  /* Now add jumps and labels as needed to match the blocks new
     outgoing edges.  */
  bool remove_unreachable_blocks = false;
  for (bb = ENTRY_BLOCK_PTR_FOR_FN (cfun)->next_bb; bb ; bb = (basic_block)bb->aux){
      edge e_fall, e_taken, e;
      rtx_insn *bb_end_insn;
      rtx ret_label = NULL_RTX;
      basic_block nb;
      edge_iterator ei;
      bool asm_goto = false;

      if (EDGE_COUNT (bb->succs) == 0)
          continue;
      /* Find the old fallthru edge, and another non-EH edge for
       a taken jump.  */
      e_taken = e_fall = NULL;

      FOR_EACH_EDGE (e, ei, bb->succs)
          if (e->flags & EDGE_FALLTHRU)
              e_fall = e;
          else if (! (e->flags & EDGE_EH))
              e_taken = e;

      bb_end_insn = BB_END (bb);
      if (rtx_jump_insn *bb_end_jump = dyn_cast <rtx_jump_insn *> (bb_end_insn)){
          ret_label = JUMP_LABEL (bb_end_jump);
          if (any_condjump_p (bb_end_jump)){
              /* This might happen if the conditional jump has side
             effects and could therefore not be optimized away.
             Make the basic block to end with a barrier in order
             to prevent rtl_verify_flow_info from complaining.  */
              if (!e_fall){
                  gcc_assert (!onlyjump_p (bb_end_jump) || returnjump_p (bb_end_jump)
                          || (e_taken->flags & EDGE_CROSSING));
                  mtcs_emit_emit_barrier_after/*!emit_barrier_after*/(mtcsEmit,bb_end_jump);
                  continue;
              }
              /* If the old fallthru is still next, nothing to do.  */
              if (bb->aux == e_fall->dest || e_fall->dest == EXIT_BLOCK_PTR_FOR_FN (cfun))
                  continue;
              /* The degenerated case of conditional jump jumping to the next
             instruction can happen for jumps with side effects.  We need
             to construct a forwarder block and this will be done just
             fine by force_nonfallthru below.  */
              if (!e_taken)
                  ;
              /* There is another special case: if *neither* block is next,
               such as happens at the very end of a function, then we'll
               need to add a new unconditional jump.  Choose the taken
               edge based on known or assumed probability.  */
              else if (bb->aux != e_taken->dest){
                  rtx note = find_reg_note (bb_end_jump, REG_BR_PROB, 0);
                  if (note
                      && profile_probability::from_reg_br_prob_note
                         (XINT (note, 0)) < profile_probability::even ()
                      && mtcs_dojump_invert_jump/*!invert_jump*/(mtcsDojump,bb_end_jump,
                              (e_fall->dest == EXIT_BLOCK_PTR_FOR_FN (cfun)
                               ? NULL_RTX : label_for_bb(self,e_fall->dest)), 0)){
                      e_fall->flags &= ~EDGE_FALLTHRU;
                      gcc_checking_assert (could_fall_through(self,e_taken->src, e_taken->dest));
                      e_taken->flags |= EDGE_FALLTHRU;
                      mtcs_cfg_rtl_update_br_prob_note/*!update_br_prob_note*/(self,bb);
                      e = e_fall, e_fall = e_taken, e_taken = e;
                  }
              }
              /* If the "jumping" edge is a crossing edge, and the fall
             through edge is non-crossing, leave things as they are.  */
              else if ((e_taken->flags & EDGE_CROSSING) && !(e_fall->flags & EDGE_CROSSING))
                  continue;
              /* Otherwise we can try to invert the jump.  This will
             basically never fail, however, keep up the pretense.  */
              else if (mtcs_dojump_invert_jump/*!invert_jump*/(mtcsDojump,bb_end_jump,
                        (e_fall->dest == EXIT_BLOCK_PTR_FOR_FN (cfun)
                         ? NULL_RTX: label_for_bb(self,e_fall->dest)), 0)){
                  e_fall->flags &= ~EDGE_FALLTHRU;
                  gcc_checking_assert (could_fall_through(self,e_taken->src, e_taken->dest));
                  e_taken->flags |= EDGE_FALLTHRU;
                  mtcs_cfg_rtl_update_br_prob_note/*!update_br_prob_note*/(self,bb);
                  if (LABEL_NUSES (ret_label) == 0 && single_pred_p (e_taken->dest))
                    mtcs_cfg_rtl_delete_insn/*!delete_insn*/(self,as_a<rtx_insn *> (ret_label));
                  continue;
              }
          }else if (extract_asm_operands (PATTERN (bb_end_insn)) != NULL){
              /* If the old fallthru is still next or if
             asm goto doesn't have a fallthru (e.g. when followed by
             __builtin_unreachable ()), nothing to do.  */
              if (! e_fall || bb->aux == e_fall->dest || e_fall->dest == EXIT_BLOCK_PTR_FOR_FN (cfun))
                  continue;
              /* Otherwise we'll have to use the fallthru fixup below.
             But avoid redirecting asm goto to EXIT.  */
              asm_goto = true;
          }else{
              /* Otherwise we have some return, switch or computed
             jump.  In the 99% case, there should not have been a
             fallthru edge.  */
              gcc_assert (returnjump_p (bb_end_insn) || !e_fall);
              continue;
          }
      }else{
          /* No fallthru implies a noreturn function with EH edges, or
             something similarly bizarre.  In any case, we don't need to
             do anything.  */
          if (! e_fall)
            continue;
          /* If the fallthru block is still next, nothing to do.  */
          if (bb->aux == e_fall->dest)
            continue;
          /* A fallthru to exit block.  */
          if (e_fall->dest == EXIT_BLOCK_PTR_FOR_FN (cfun))
            continue;
      }

      /* If E_FALL->dest is just a return block, then we can emit a
     return rather than a jump to the return block.  */
      rtx_insn *ret, *use;
      basic_block dest;
      if (!asm_goto
          && mtcs_cfg_cleanup_bb_is_just_return/*!bb_is_just_return*/(mtcsCfgCleanup,e_fall->dest, &ret, &use)
          && ((PATTERN (ret) == simple_return_rtx && targetm.have_simple_return ())
              || (PATTERN (ret) == ret_rtx && targetm.have_return ()))){
          ret_label = PATTERN (ret);
          dest = EXIT_BLOCK_PTR_FOR_FN (cfun);

          e_fall->flags &= ~EDGE_CROSSING;
          /* E_FALL->dest might become unreachable as a result of
             replacing the jump with a return.  So arrange to remove
             unreachable blocks.  */
          remove_unreachable_blocks = true;
      }else{
          dest = e_fall->dest;
      }

      /* We got here if we need to add a new jump insn.
     Note force_nonfallthru can delete E_FALL and thus we have to
     save E_FALL->src prior to the call to force_nonfallthru.  */
      nb = mcs_cfg_rtl_force_nonfallthru_and_redirect/*!force_nonfallthru_and_redirect*/(self,e_fall, dest, ret_label);
      if (nb){
          nb->aux = bb->aux;
          bb->aux = nb;
          /* Don't process this new block.  */
          bb = nb;
      }
  }

  mtcs_cfg_rtl_relink_block_chain/*!relink_block_chain*/(self,/*stay_in_cfglayout_mode=*/false);
  /* Annoying special case - jump around dead jumptables left in the code.  */
  FOR_EACH_BB_FN (bb, cfun){
      edge e = find_fallthru_edge (bb->succs);
      if (e && !mtcs_cfg_rgl_can_fallthru/*!can_fallthru*/(self,e->src, e->dest))
         mtcs_cfg_context_force_nonfallthru/*!force_nonfallthru*/(mtcsCfgContext,e);
  }
  /* Ensure goto_locus from edges has some instructions with that locus in RTL
     when not optimizing.  */
  if (!mtcsOptionsItem->x_optimize && !DECL_IGNORED_P (current_function_decl))
    FOR_EACH_BB_FN (bb, cfun){
        edge e;
        edge_iterator ei;
        FOR_EACH_EDGE (e, ei, bb->succs)
          if (LOCATION_LOCUS (e->goto_locus) != UNKNOWN_LOCATION  && !(e->flags & EDGE_ABNORMAL)){
              edge e2;
              edge_iterator ei2;
              basic_block dest, nb;
              rtx_insn *end;

              insn = BB_END (e->src);
              end = PREV_INSN (BB_HEAD (e->src));
              while (insn != end && (!NONDEBUG_INSN_P (insn) || !INSN_HAS_LOCATION (insn)))
                  insn = PREV_INSN (insn);
              if (insn != end  && loc_equal(self,INSN_LOCATION (insn), e->goto_locus))
                  continue;
              if (simplejump_p (BB_END (e->src))  && !INSN_HAS_LOCATION (BB_END (e->src))){
                  INSN_LOCATION (BB_END (e->src)) = e->goto_locus;
                  continue;
              }
              dest = e->dest;
              if (dest == EXIT_BLOCK_PTR_FOR_FN (cfun)){
                  /* Non-fallthru edges to the exit block cannot be split.  */
                  if (!(e->flags & EDGE_FALLTHRU))
                    continue;
              }else{
                  insn = BB_HEAD (dest);
                  end = NEXT_INSN (BB_END (dest));
                  while (insn != end && !NONDEBUG_INSN_P (insn))
                    insn = NEXT_INSN (insn);
                  if (insn != end && INSN_HAS_LOCATION (insn) && loc_equal(self,INSN_LOCATION (insn), e->goto_locus))
                    continue;
              }
              nb = mtcs_cfg_context_split_edge/*!split_edge*/(mtcsCfgContext,e);
              if (!INSN_P (BB_END (nb)))
                  BB_END (nb) = mtcs_emit_emit_insn_after_noloc/*!emit_insn_after_noloc*/(mtcsEmit,gen_nop (), BB_END (nb),nb);
              INSN_LOCATION (BB_END (nb)) = e->goto_locus;

              /* If there are other incoming edges to the destination block
             with the same goto locus, redirect them to the new block as
             well, this can prevent other such blocks from being created
             in subsequent iterations of the loop.  */
              for (ei2 = ei_start (dest->preds); (e2 = ei_safe_edge (ei2)); )
                  if (LOCATION_LOCUS (e2->goto_locus) != UNKNOWN_LOCATION
                    && !(e2->flags & (EDGE_ABNORMAL | EDGE_FALLTHRU))
                    && e->goto_locus == e2->goto_locus)
                     mtcs_cfg_context_redirect_edge_and_branch/*!redirect_edge_and_branch*/(mtcsCfgContext,e2, nb);
                  else
                      ei_next (&ei2);
          }
    }

  /* Replacing a jump with a return may have exposed an unreachable
     block.  Conditionally remove them if such transformations were
     made.  */
  if (remove_unreachable_blocks)
     mtcs_cfg_cleanup_delete_unreachable_blocks/*!delete_unreachable_blocks*/(mtcsCfgCleanup);
}

/* Perform sanity checks on the insn chain.
   1. Check that next/prev pointers are consistent in both the forward and
      reverse direction.
   2. Count insns in chain, going both directions, and check if equal.
   3. Check that get_last_insn () returns the actual end of chain.  */

static DEBUG_FUNCTION void verify_insn_chain (MtcsCfgRtl *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  rtx_insn *x, *prevx, *nextx;
  int insn_cnt1, insn_cnt2;

  for (prevx = NULL, insn_cnt1 = 1, x = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
       x != 0;
       prevx = x, insn_cnt1++, x = NEXT_INSN (x))
    gcc_assert (PREV_INSN (x) == prevx);

  gcc_assert (prevx ==mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData));

  for (nextx = NULL, insn_cnt2 = 1, x = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
       x != 0;
       nextx = x, insn_cnt2++, x = PREV_INSN (x))
    gcc_assert (NEXT_INSN (x) == nextx);
  gcc_assert (insn_cnt1 == insn_cnt2);
}

/* If we have assembler epilogues, the block falling through to exit must
   be the last one in the reordered chain when we reach final.  Ensure
   that this condition is met.  */
static void fixup_fallthru_exit_predecessor (MtcsCfgRtl *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);

  edge e;
  basic_block bb = NULL;
  /* This transformation is not valid before reload, because we might
     separate a call from the instruction that copies the return
     value.  */
  gcc_assert (reload_completed);
  e = find_fallthru_edge (EXIT_BLOCK_PTR_FOR_FN (cfun)->preds);
  if (e)
      bb = e->src;
  if (bb && bb->aux){
      basic_block c = ENTRY_BLOCK_PTR_FOR_FN (cfun)->next_bb;
      /* If the very first block is the one with the fall-through exit
     edge, we have to split that block.  */
      if (c == bb){
          bb = mtcs_cfg_context_split_block_after_labels/*!split_block_after_labels*/(mtcsCfgContext,bb)->dest;
          bb->aux = c->aux;
          c->aux = bb;
          BB_FOOTER (bb) = BB_FOOTER (c);
          BB_FOOTER (c) = NULL;
      }
      while (c->aux != bb)
          c = (basic_block) c->aux;
      c->aux = bb->aux;
      while (c->aux)
          c = (basic_block) c->aux;
      c->aux = bb;
      bb->aux = NULL;
  }
}

/* In case there are more than one fallthru predecessors of exit, force that
   there is only one.  */
static void force_one_exit_fallthru (MtcsCfgRtl *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);

   edge e, predecessor = NULL;
   bool more = false;
   edge_iterator ei;
   basic_block forwarder, bb;
   FOR_EACH_EDGE (e, ei, EXIT_BLOCK_PTR_FOR_FN (cfun)->preds)
      if (e->flags & EDGE_FALLTHRU){
         if (predecessor == NULL)
            predecessor = e;
         else{
            more = true;
            break;
         }
      }

   if (!more)
      return;
   /* Exit has several fallthru predecessors.  Create a forwarder block for
   them.  */
   forwarder = mtcs_cfg_context_split_edge/*!split_edge*/(mtcsCfgContext,predecessor);
   for (ei = ei_start (EXIT_BLOCK_PTR_FOR_FN (cfun)->preds); (e = ei_safe_edge (ei)); ){
      if (e->src == forwarder || !(e->flags & EDGE_FALLTHRU))
         ei_next (&ei);
      else
         mtcs_cfg_context_redirect_edge_and_branch_force/*!redirect_edge_and_branch_force*/(mtcsCfgContext,e, forwarder);
   }
      /* Fix up the chain of blocks -- make FORWARDER immediately precede the
      exit block.  */
   FOR_EACH_BB_FN (bb, cfun){
      if (bb->aux == NULL && bb != forwarder){
         bb->aux = forwarder;
         break;
      }
   }
}

/* Return true in case it is possible to duplicate the basic block BB.  */
//rtl_cfg_hooks  cfg_layout_rtl_cfg_hooks member
static bool cfg_layout_can_duplicate_bb_p (MtcsCfgRtl *self,const_basic_block bb)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  /* Do not attempt to duplicate tablejumps, as we need to unshare
     the dispatch table.  This is difficult to do, as the instructions
     computing jump destination may be hoisted outside the basic block.  */
  if (tablejump_p (BB_END (bb), NULL, NULL))
    return false;
  /* Do not duplicate blocks containing insns that can't be copied.  */
  if (mtcsTarget->/*!targetm.cannot_copy_insn_p*/cannot_copy_insn_p){
      rtx_insn *insn = BB_HEAD (bb);
      while (1){
          if (INSN_P (insn) && mtcsTarget/*!targetm.cannot_copy_insn_p*/->cannot_copy_insn_p(mtcsTarget,insn))
              return false;
          if (insn == BB_END (bb))
              break;
          insn = NEXT_INSN (insn);
      }
  }
  return true;
}

//原型 duplicate_insn_chain cfgrtl.h cfgrtl.cc
rtx_insn * mtcs_cfg_rtl_duplicate_insn_chain (MtcsCfgRtl *self,rtx_insn *from, rtx_insn *to,
              class loop *loop, copy_bb_data *id)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  rtx_insn *insn, *next, *copy;
  rtx_note *last;
  /* Avoid updating of boundaries of previous basic block.  The
     note will get removed from insn stream in fixup.  */
  last =mtcs_emit_emit_note/*!emit_note*/(mtcsEmit,NOTE_INSN_DELETED);
  /* Create copy at the end of INSN chain.  The chain will
     be reordered later.  */
  for (insn = from; insn != NEXT_INSN (to); insn = NEXT_INSN (insn)){
    switch (GET_CODE (insn)){
        case DEBUG_INSN:
            /* Don't duplicate label debug insns.  */
            if (DEBUG_BIND_INSN_P (insn)  && TREE_CODE (INSN_VAR_LOCATION_DECL (insn)) == LABEL_DECL)
                break;
        /* FALLTHRU */
        case INSN:
        case CALL_INSN:
        case JUMP_INSN:
            copy = mtcs_emit_emit_copy_of_insn_after/*!emit_copy_of_insn_after*/(mtcsEmit,
                  insn, mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData));
            if (JUMP_P (insn) && JUMP_LABEL (insn) != NULL_RTX && ANY_RETURN_P (JUMP_LABEL (insn)))
                JUMP_LABEL (copy) = JUMP_LABEL (insn);
            mtcs_func_maybe_copy_prologue_epilogue_insn/*!maybe_copy_prologue_epilogue_insn*/(mtcsFunc,insn, copy);
            /* If requested remap dependence info of cliques brought in
            via inlining.  */
            if (id){
                subrtx_iterator::array_type array;
                FOR_EACH_SUBRTX (iter, array, PATTERN (insn), ALL)
                    if (MEM_P (*iter) && mtcs_rtl_get_mem_expr/*!MEM_EXPR*/(mtcsRTL,*iter)){
                        tree op = mtcs_rtl_get_mem_expr/*!MEM_EXPR*/(mtcsRTL,*iter);
                        if (TREE_CODE (op) == WITH_SIZE_EXPR)
                            op = TREE_OPERAND (op, 0);
                        while (handled_component_p (op))
                            op = TREE_OPERAND (op, 0);

                        if ((TREE_CODE (op) == MEM_REF || TREE_CODE (op) == TARGET_MEM_REF)
                          && MR_DEPENDENCE_CLIQUE (op) > 1
                          && (!loop  || (MR_DEPENDENCE_CLIQUE (op)  != loop->owned_clique))){
                            if (!id->dependence_map)
                                id->dependence_map = new hash_map<dependence_hash,unsigned short>;
                            bool existed;
                            unsigned short &newc = id->dependence_map->get_or_insert(MR_DEPENDENCE_CLIQUE (op), &existed);
                            if (!existed){
                                gcc_assert(MR_DEPENDENCE_CLIQUE (op) <= cfun->last_clique);
                                newc = get_new_clique (cfun);
                            }
                            /* We cannot adjust MR_DEPENDENCE_CLIQUE in-place
                            since MEM_EXPR is shared so make a copy and
                            walk to the subtree again.  */
                            tree new_expr = unshare_expr (mtcs_rtl_get_mem_expr/*!MEM_EXPR*/(mtcsRTL,*iter));
                            if (TREE_CODE (new_expr) == WITH_SIZE_EXPR)
                                new_expr = TREE_OPERAND (new_expr, 0);
                            while (handled_component_p (new_expr))
                                new_expr = TREE_OPERAND (new_expr, 0);
                            MR_DEPENDENCE_CLIQUE (new_expr) = newc;
                            mtcs_rtl_set_mem_expr/*!set_mem_expr*/(mtcsRTL,const_cast <rtx> (*iter), new_expr);
                        }
                    }
            }
            break;

        case JUMP_TABLE_DATA:
            /* Avoid copying of dispatch tables.  We never duplicate
            tablejumps, so this can hit only in case the table got
            moved far from original jump.
            Avoid copying following barrier as well if any
            (and debug insns in between).  */
            for (next = NEXT_INSN (insn); next != NEXT_INSN (to); next = NEXT_INSN (next))
                if (!DEBUG_INSN_P (next))
                    break;
            if (next != NEXT_INSN (to) && BARRIER_P (next))
                insn = next;
            break;

        case CODE_LABEL:
            break;

        case BARRIER:
            emit_barrier ();
            break;

        case NOTE:
            switch (NOTE_KIND (insn)){
                /* In case prologue is empty and function contain label
                in first BB, we may want to copy the block.  */
                case NOTE_INSN_PROLOGUE_END:
                case NOTE_INSN_DELETED:
                case NOTE_INSN_DELETED_LABEL:
                case NOTE_INSN_DELETED_DEBUG_LABEL:
                /* No problem to strip these.  */
                case NOTE_INSN_FUNCTION_BEG:
                /* There is always just single entry to function.  */
                case NOTE_INSN_BASIC_BLOCK:
                /* We should only switch text sections once.  */
                case NOTE_INSN_SWITCH_TEXT_SECTIONS:
                    break;
                case NOTE_INSN_EPILOGUE_BEG:
                case NOTE_INSN_UPDATE_SJLJ_CONTEXT:
                    emit_note_copy (as_a <rtx_note *> (insn));
                    break;
                default:
                    /* All other notes should have already been eliminated.  */
                    gcc_unreachable ();
            }
            break;
        default:
            gcc_unreachable ();
    }
  }

  insn = NEXT_INSN (last);
  mtcs_cfg_rtl_delete_insn/*!delete_insn*/(self,last);
  return insn;
}
/* Main entry point to this module - initialize the datastructures for
   CFG layout changes.  It keeps LOOPS up-to-date if not null.

   FLAGS is a set of additional flags to pass to cleanup_cfg().  */
//原型 cfg_layout_initialize cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_cfg_layout_initialize (MtcsCfgRtl *self,int flags)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgCleanup *mtcsCfgCleanup=mtcs_target_get_cfg_cleanup(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsCfg *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   rtx_insn_list *x;
   basic_block bb;
   /* Once bb partitioning is complete, cfg layout mode should not be
   re-entered.  Entering cfg layout mode may require fixups.  As an
   example, if edge forwarding performed when optimizing the cfg
   layout required moving a block from the hot to the cold
   section. This would create an illegal partitioning unless some
   manual fixup was performed.  */
   gcc_assert (!mtcsRtlData/*!crtl*/->bb_reorder_complete || !mtcsRtlData/*!crtl*/->has_bb_partition);

   mtcs_cfg_initialize_original_copy_tables/*!initialize_original_copy_tables*/(mtcsCfg);

   mtcs_cfg_context_change_layout_state/*!cfg_layout_rtl_register_cfg_hooks*/(mtcsCfgContext);

   record_effective_endpoints(self);

   /* Make sure that the targets of non local gotos are marked.  */
   for (x = mtcsRtlData/*!nonlocal_goto_handler_labels*/->x_nonlocal_goto_handler_labels; x; x = x->next ()){
      bb = BLOCK_FOR_INSN (x->insn ());
      bb->flags |= BB_NON_LOCAL_GOTO_TARGET;
   }
   mtcs_cfg_cleanup_cleanup_cfg/*!cleanup_cfg*/(mtcsCfgCleanup,CLEANUP_CFGLAYOUT | flags);
}

/* Splits superblocks.  */
//原型 break_superblocks cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_break_superblocks (MtcsCfgRtl *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  bool need = false;
  basic_block bb;
  auto_sbitmap superblocks (last_basic_block_for_fn (cfun));
  bitmap_clear (superblocks);

  FOR_EACH_BB_FN (bb, cfun)
    if (bb->flags & BB_SUPERBLOCK){
        bb->flags &= ~BB_SUPERBLOCK;
        bitmap_set_bit (superblocks, bb->index);
        need = true;
    }

  if (need){
     mtcs_dojump_rebuild_jump_labels/*!rebuild_jump_labels*/(mtcsDojump,mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData));
     find_many_sub_basic_blocks (superblocks);
  }
}

/* Finalize the changes: reorder insn list according to the sequence specified
   by aux pointers, enter compensation code, rebuild scope forest.  */
//原型 cfg_layout_finalize cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_cfg_layout_finalize (MtcsCfgRtl *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsCfgCleanup *mtcsCfgCleanup=mtcs_target_get_cfg_cleanup(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   free_dominance_info (CDI_DOMINATORS);
   force_one_exit_fallthru(self);
   mtcs_cfg_context_change_rtl_state/*rtl_register_cfg_hooks*/(mtcsCfgContext);

   if (reload_completed && !target_rtx_have_epilogue/*!targetm.have_epilogue*/(mtcsMachine->tmrtx))
      fixup_fallthru_exit_predecessor(self);
   fixup_reorder_chain(self);

   mtcs_dojump_rebuild_jump_labels/*!rebuild_jump_labels*/(mtcsDojump,mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData));
   mtcs_cfg_cleanup_delete_dead_jumptables/*!delete_dead_jumptables*/(mtcsCfgCleanup);

   if (mtcsOptionsItem->x_flag_checking)
      verify_insn_chain(self);
   mtcs_cfg_context_checking_verify_flow_info/*!checking_verify_flow_info*/(mtcsCfgContext);
}

/* Return true when blocks A and B can be safely merged.  */
static bool cfg_layout_can_merge_blocks_p (MtcsCfgRtl *self,basic_block a, basic_block b)
{
  /* If we are partitioning hot/cold basic blocks, we don't want to
     mess up unconditional or indirect jumps that cross between hot
     and cold sections.

     Basic block partitioning may result in some jumps that appear to
     be optimizable (or blocks that appear to be mergeable), but which really
     must be left untouched (they are required to make it safely across
     partition boundaries).  See  the comments at the top of
     bb-reorder.cc:partition_hot_cold_basic_blocks for complete details.  */
  if (BB_PARTITION (a) != BB_PARTITION (b))
    return false;
  /* Protect the loop latches.  */
  if (cfun->x_current_loops/*!current_loops*/ && b->loop_father->latch == b)
    return false;

  /* If we would end up moving B's instructions, make sure it doesn't fall
     through into the exit block, since we cannot recover from a fallthrough
     edge into the exit block occurring in the middle of a function.  */
  if (NEXT_INSN (BB_END (a)) != BB_HEAD (b)){
      edge e = find_fallthru_edge (b->succs);
      if (e && e->dest == EXIT_BLOCK_PTR_FOR_FN (cfun))
          return false;
  }
  /* There must be exactly one edge in between the blocks.  */
  return (single_succ_p (a)
      && single_succ (a) == b
      && single_pred_p (b) == 1
      && a != b
      /* Must be simple edge.  */
      && !(single_succ_edge (a)->flags & EDGE_COMPLEX)
      && a != ENTRY_BLOCK_PTR_FOR_FN (cfun)
      && b != EXIT_BLOCK_PTR_FOR_FN (cfun)
      /* If the jump insn has side effects, we can't kill the edge.
         When not optimizing, try_redirect_by_replacing_jump will
         not allow us to redirect an edge by replacing a table jump.  */
      && (!JUMP_P (BB_END (a))
          || ((!optimize || reload_completed)
          ? simplejump_p (BB_END (a)) : onlyjump_p (BB_END (a)))));
}

/* Merge block A and B.  The blocks must be mergeable.  */
//实现 cfglayout的接口 void (*merge_blocks) (MtcsCfgState *self,basic_block a, basic_block b);
void mtcs_cfg_rtl_layout_merge_blocks (MtcsCfgRtl *self, basic_block a, basic_block b)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  /* If B is a forwarder block whose outgoing edge has no location, we'll
     propagate the locus of the edge between A and B onto it.  */
  const bool forward_edge_locus = (b->flags & BB_FORWARDER_BLOCK) != 0
      && LOCATION_LOCUS (EDGE_SUCC (b, 0)->goto_locus) == UNKNOWN_LOCATION;
  rtx_insn *insn;
  gcc_checking_assert (cfg_layout_can_merge_blocks_p(self,a, b));
  if (dump_file)
    fprintf (dump_file, "Merging block %d into block %d...\n", b->index,a->index);

  /* If there was a CODE_LABEL beginning B, delete it.  */
  if (LABEL_P (BB_HEAD (b))){
      mtcs_cfg_rtl_delete_insn/*!delete_insn*/(self,BB_HEAD (b));
  }
  /* We should have fallthru edge in a, or we can do dummy redirection to get
     it cleaned up.  */
  if (JUMP_P (BB_END (a)))
      mtcs_cfg_rtl_try_redirect_by_replacing_jump/*!try_redirect_by_replacing_jump*/(self,EDGE_SUCC (a, 0), b, true);
  gcc_assert (!JUMP_P (BB_END (a)));
  /* If not optimizing, preserve the locus of the single edge between
     blocks A and B if necessary by emitting a nop.  */
  if (!mtcsOptionsItem->x_optimize && !forward_edge_locus   && !DECL_IGNORED_P (current_function_decl))
      emit_nop_for_unique_locus_between(self,a, b);

  /* Move things from b->footer after a->footer.  */
  if (BB_FOOTER (b)){
      if (!BB_FOOTER (a))
          BB_FOOTER (a) = BB_FOOTER (b);
      else{
          rtx_insn *last = BB_FOOTER (a);
          while (NEXT_INSN (last))
              last = NEXT_INSN (last);
          SET_NEXT_INSN (last) = BB_FOOTER (b);
          SET_PREV_INSN (BB_FOOTER (b)) = last;
      }
      BB_FOOTER (b) = NULL;
  }

  /* Move things from b->header before a->footer.
     Note that this may include dead tablejump data, but we don't clean
     those up until we go out of cfglayout mode.  */
   if (BB_HEADER (b)){
      if (! BB_FOOTER (a))
          BB_FOOTER (a) = BB_HEADER (b);
      else{
          rtx_insn *last = BB_HEADER (b);

          while (NEXT_INSN (last))
            last = NEXT_INSN (last);
          SET_NEXT_INSN (last) = BB_FOOTER (a);
          SET_PREV_INSN (BB_FOOTER (a)) = last;
          BB_FOOTER (a) = BB_HEADER (b);
      }
      BB_HEADER (b) = NULL;
  }

  /* In the case basic blocks are not adjacent, move them around.  */
  if (NEXT_INSN (BB_END (a)) != BB_HEAD (b)){
      insn = mtcs_cfg_rtl_unlink_insn_chain/*!unlink_insn_chain*/(self,BB_HEAD (b), BB_END (b));
      mtcs_emit_emit_insn_after_noloc/*!emit_insn_after_noloc*/(mtcsEmit,insn, BB_END (a), a);
  }
  /* Otherwise just re-associate the instructions.  */
  else{
      insn = BB_HEAD (b);
      BB_END (a) = BB_END (b);
  }
  /* emit_insn_after_noloc doesn't call df_insn_change_bb.
     We need to explicitly call. */
  update_bb_for_insn_chain(self,insn, BB_END (b), a);
  /* Skip possible DELETED_LABEL insn.  */
  if (!NOTE_INSN_BASIC_BLOCK_P (insn))
    insn = NEXT_INSN (insn);
  gcc_assert (NOTE_INSN_BASIC_BLOCK_P (insn));
  BB_HEAD (b) = BB_END (b) = NULL;
  mtcs_cfg_rtl_delete_insn/*!delete_insn*/(self,insn);
  mtcs_dfcore_df_bb_delete/*!df_bb_delete*/(mtcsDfcore,b->index);
  if (forward_edge_locus)
      EDGE_SUCC (b, 0)->goto_locus = EDGE_SUCC (a, 0)->goto_locus;

  if (dump_file)
      fprintf (dump_file, "Merged blocks %d and %d.\n", a->index, b->index);
}


//原型 init_rtl_bb_info cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_init_rtl_bb_info (MtcsCfgRtl *self,basic_block bb)
{
  gcc_assert (!bb->il.x.rtl);
  bb->il.x.head_ = NULL;
  bb->il.x.rtl = ggc_cleared_alloc<rtl_bb_info> ();
}

static void mtcsCfgRtlInit(MtcsCfgRtl *self)
{

}


MtcsCfgRtl *mtcs_cfg_rtl_new(MtcsMode *mtcsMode)
{
      MtcsCfgRtl *self = n_slice_alloc0 (sizeof(MtcsCfgRtl));
      mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
      mtcsCfgRtlInit(self);
      return self;
}

static   basic_block split_edge_cb (edge e)
{
   MtcsTarget *mtcsTarget = mtcs_compile_get_current_target(mtcs_compile_get());
   MtcsCfgContext *mtcsCfgContext = mtcs_target_get_cfg_context(mtcsTarget);
   MtcsCfgState *mtcsCfgLayoutState = mtcsCfgContext->mtcsCfgLayoutState;

   n_debug("mtcscfgrtl.c split_edge_cb %p\n",e);
   return mtcsCfgLayoutState->split_edge(mtcsCfgLayoutState,e);
}


/*******************以下是基于MtcsCfgRtl的2个 rtl pass**************************************/
//原型 NEXT_PASS (pass_into_cfg_layout_mode, 1);    RTL_PASS   cfgrtl.cc   into_cfglayout   y  无条件执行 cfg_layout_initialize (0)
static nuint pass_into_cfg_layout_execute_cb(MtcsPass *mtcsPass,function *func)
{
   MtcsPassIntoCfgLayout *self=(MtcsPassIntoCfgLayout *)mtcsPass;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   mtcs_cfg_rtl_cfg_layout_initialize/*!cfg_layout_initialize*/(mtcsCfgRtl,0);
   return 0;
}

static void mtcsPassIntoCfgLayoutInit(MtcsPassIntoCfgLayout *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =pass_into_cfg_layout_execute_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            PROP_cfglayout, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassIntoCfgLayout *mtcs_pass_into_cfg_layout_new(MtcsMode *mtcsMode)
{
   MtcsPassIntoCfgLayout *self = n_slice_alloc0 (sizeof(MtcsPassIntoCfgLayout));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"into_cfglayout");
   mtcsPassIntoCfgLayoutInit(self);
   return self;
}

//原型 NEXT_PASS (pass_outof_cfg_layout_mode, 1);  RTL_PASS cfgrtl.cc outof_cfglayout   y  无条件执行 cfg_layout_finalize
static nuint pass_outof_cfg_layout_execute_cb(MtcsPass *mtcsPass,function *func)
{
      MtcsPassOutofCfgLayout *self=(MtcsPassOutofCfgLayout *)mtcsPass;
      MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
      MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
      MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
      n_debug("mtcscfgrtl.c pass_outof_cfg_layout_execute_cb 00\n");
      basic_block bb;
      FOR_EACH_BB_FN (bb, func)
         if (bb->next_bb != EXIT_BLOCK_PTR_FOR_FN (func))
            bb->aux = bb->next_bb;
      n_debug("mtcscfgrtl.c pass_outof_cfg_layout_execute_cb 11\n");

      mtcs_cfg_rtl_cfg_layout_finalize/*!cfg_layout_finalize*/(mtcsCfgRtl);
      n_debug("mtcscfgrtl.c pass_outof_cfg_layout_execute_cb 22\n");
      return 0;
}

static void mtcsPassOutofCfgLayoutInit(MtcsPassOutofCfgLayout *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =pass_outof_cfg_layout_execute_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            PROP_cfglayout /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassOutofCfgLayout *mtcs_pass_outof_cfg_layout_new(MtcsMode *mtcsMode)
{
   MtcsPassOutofCfgLayout *self = n_slice_alloc0 (sizeof(MtcsPassOutofCfgLayout));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"outof_cfglayout");
   mtcsPassOutofCfgLayoutInit(self);
   return self;
}

//原型 NEXT_PASS (pass_free_cfg, 1);  RTL_PASS cfgrtl.cc *free_cfg   y  无条件执行 df_note_add_problem..df_analyze
static nuint free_cfg_execute_cb(MtcsPass *mtcsPass,function *func)
{
      MtcsPassFreeCfg *self=(MtcsPassFreeCfg *)mtcsPass;
      MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
      MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
      MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
      MtcsDfproblems *mtcsDfproblems=mtcs_target_get_dfproblems(mtcsTarget);
      MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
      MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
      MtcsInsnAttr *mtcsInsnAttr=mtcs_target_get_insn_attr(mtcsTarget);
      MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
      MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
      MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
      /* The resource.cc machinery uses DF but the CFG isn't guaranteed to be
      valid at that point so it would be too late to call df_analyze.  */
      if (mtcs_insn_attr_get_delay_slots/*!DELAY_SLOTS*/(mtcsInsnAttr)
            && mtcsOptionsItem->x_optimize > 0 && mtcsOptionsItem->x_flag_delayed_branch){
          mtcs_dfproblems_df_note_add_problem/*!df_note_add_problem*/(mtcsDfproblems);
          mtcs_dfcore_df_analyze/*!df_analyze*/(mtcsDfcore);
      }

      if (mtcsRtlData/*!crtl*/->has_bb_partition)
         mtcs_cfg_rtl_insert_section_boundary_note/*!insert_section_boundary_note*/(mtcsCfgRtl);

      mtcs_cfg_rtl_free_bb_for_insn/*!free_bb_for_insn*/(mtcsCfgRtl);
      return 0;
}

static void mtcsPassFreeCfgInit(MtcsPassFreeCfg *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =free_cfg_execute_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            PROP_cfg /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassFreeCfg *mtcs_pass_free_cfg_new(MtcsMode *mtcsMode)
{
   MtcsPassFreeCfg *self = n_slice_alloc0 (sizeof(MtcsPassFreeCfg));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"*free_cfg");
   mtcsPassFreeCfgInit(self);
   return self;
}

