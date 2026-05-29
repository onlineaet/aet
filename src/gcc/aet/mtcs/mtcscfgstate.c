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
#include "bb-reorder.h"
#include "rtl-error.h"
#include "insn-attr.h"
#include "dojump.h"
#include "expr.h"
#include "cfgloop.h"
#include "tree-pass.h"
#include "print-rtl.h"
#include "gimplify.h"
#include "profile.h"
#include "sreal.h"

#include "tm_p.h"
#include "cselib.h"
#include "dce.h"
#include "dbgcnt.h"
#include "rtl-iter.h"
#include "regs.h"
#include "function-abi.h"


#include "aet/aetprinttree.h"
#include "mtcscfgstate.h"
#include "mtcstarget.h"
#include "mtcscompile.h"
#include "mtcsdfcore.h"
#include "mtcsdfproblems.h"
#include "mtcsprintrtl.h"

//原型 void (*dump_bb) (MtcsCfgState *self,FILE *, basic_block, int, dump_flags_t);
static void dump_bb_cb (MtcsCfgState *self,FILE *outf, basic_block bb, int indent, dump_flags_t flags);
//原型 void (*dump_bb_for_graph) (MtcsCfgState *self,pretty_printer *, basic_block);
// rtl_dump_bb_for_graph print-rtl.h print-rtl.cc
static void rtl_dump_bb_for_graph_cb(MtcsCfgState *self,pretty_printer *print, basic_block bb);
//原型  bool (*can_remove_branch_p) (MtcsCfgState *self,const_edge);
static bool can_remove_branch_p_cb(MtcsCfgState *self,const_edge e);
//原型 void (*predict_edge) (MtcsCfgState *self,edge e, enum br_predictor predictor, int probability);
static void predict_edge_cb(MtcsCfgState *self,edge e, enum br_predictor predictor, int probability);
//原型  bool (*predicted_by_p) (MtcsCfgState *self,const_basic_block bb, enum br_predictor predictor);
static bool predicted_by_p_cb(MtcsCfgState *self,const_basic_block bb, enum br_predictor predictor);
//原型 bool (*can_duplicate_block_p) (MtcsCfgState *self,const_basic_block a);cfg_layout_can_duplicate_bb_p cfgrtl.cc
static bool can_duplicate_bb_p_cb(MtcsCfgState *self,const_basic_block bb);
//原型 void (*make_forwarder_block) (MtcsCfgState *self,edge);
static void make_forwarder_block_cb (MtcsCfgRtl *self,edge fallthru ATTRIBUTE_UNUSED);
//原型 void (*tidy_fallthru_edge) (MtcsCfgState *self,edge);
static basic_block rtl_force_nonfallthru_cb (MtcsCfgState *self,edge e);
//原型 bool (*move_block_after) (MtcsCfgState *self,basic_block b, basic_block a);
static bool move_block_after_cb(MtcsCfgState *self,basic_block bb ATTRIBUTE_UNUSED,
              basic_block after ATTRIBUTE_UNUSED);
//原型 bool (*block_ends_with_call_p) (MtcsCfgState *self,basic_block);
static bool rtl_block_ends_with_call_p_cb(MtcsCfgState *self,basic_block bb);
//原型 bool (*block_ends_with_condjump_p) (MtcsCfgState *self,const_basic_block);
static bool rtl_block_ends_with_condjump_p_cb (MtcsCfgState *self,const_basic_block bb);
//原型 bool (*empty_block_p) (MtcsCfgState *self,basic_block);
static bool block_empty_p_cb (MtcsCfgState *self,basic_block bb);
//原型 basic_block (*split_block_before_cond_jump) (MtcsCfgState *self,basic_block);rtl_split_block_before_cond_jump cfgrtl.cc
static basic_block split_block_before_cond_jump_cb (MtcsCfgState *self,basic_block bb);
//原型 void (*account_profile_record) (MtcsCfgState *self,basic_block, struct profile_record *);rtl_account_profile_record_cb cfgrtl.cc
static void account_profile_record_cb (MtcsCfgState *self,basic_block bb, struct profile_record *record);
//原型 int (*flow_call_edges_add) (MtcsCfgState *self,sbitmap);rtl_flow_call_edges_add cfgrtl.cc
static int flow_call_edges_add_cb (MtcsCfgState *self,sbitmap blocks);

void mtcs_cfg_state_init(MtcsCfgState *self)
{
   /* Debugging.  */
   self->verify_flow_info=NULL;
   self->dump_bb=dump_bb_cb;
   self->dump_bb_for_graph=rtl_dump_bb_for_graph_cb;
   /* Basic CFG manipulation.  */

   /* Return new basic block.  */
   self->create_basic_block=NULL;
   self->redirect_edge_and_branch=NULL;
   self->redirect_edge_and_branch_force=NULL;
   self->can_remove_branch_p=can_remove_branch_p_cb;
   self->delete_basic_block=NULL;
   self->split_block=NULL;
   self->move_block_after=move_block_after_cb;
   self->can_merge_blocks_p=NULL;
   self->merge_blocks=NULL;
   self->predict_edge=predict_edge_cb;
   self->predicted_by_p=predicted_by_p_cb;

   self->can_duplicate_block_p=can_duplicate_bb_p_cb;

   self->duplicate_block=NULL;
   self->split_edge=NULL;
   self->make_forwarder_block=make_forwarder_block_cb;
   self->tidy_fallthru_edge=NULL;
   self->force_nonfallthru=rtl_force_nonfallthru_cb;
   self->block_ends_with_call_p=rtl_block_ends_with_call_p_cb;
   self->block_ends_with_condjump_p=rtl_block_ends_with_condjump_p_cb;

   self->flow_call_edges_add=flow_call_edges_add_cb;
   self->execute_on_growing_pred=NULL;
   self->execute_on_shrinking_pred=NULL;
   self->cfg_hook_duplicate_loop_body_to_header_edge=NULL;
   self->extract_cond_bb_edges=NULL;
   self->flush_pending_stmts=NULL;
   self->empty_block_p=block_empty_p_cb;
   self->split_block_before_cond_jump=split_block_before_cond_jump_cb;
   self->account_profile_record=account_profile_record_cb;
}

enum ir_type mtcs_cfg_state_get_state_type(MtcsCfgState *self)
{
   return (enum ir_type)self->stateType;
}

/* Do postprocessing after making a forwarder block joined by edge FALLTHRU.  */
//原型 void (*make_forwarder_block) (MtcsCfgState *self,edge);
static void make_forwarder_block_cb (MtcsCfgRtl *self,edge fallthru ATTRIBUTE_UNUSED)
{
}

//原型 void (*tidy_fallthru_edge) (MtcsCfgState *self,edge);
static basic_block rtl_force_nonfallthru_cb (MtcsCfgState *self,edge e)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   return mcs_cfg_rtl_force_nonfallthru_and_redirect/*!force_nonfallthru_and_redirect*/(mtcsCfgRtl,e, e->dest, NULL_RTX);
}

/* Say whether a block ends with a call, possibly followed by some
    other code that must stay with the call.  */
//原型 bool (*block_ends_with_call_p) (MtcsCfgState *self,basic_block);
static bool rtl_block_ends_with_call_p_cb(MtcsCfgState *self,basic_block bb)
{
  rtx_insn *insn = BB_END (bb);
  while (!CALL_P (insn)
     && insn != BB_HEAD (bb)
     && (keep_with_call_p (insn)
         || NOTE_P (insn)
         || DEBUG_INSN_P (insn)))
    insn = PREV_INSN (insn);
  return (CALL_P (insn));
}

/* Return true if BB ends with a conditional branch, false otherwise.  */
//原型 bool (*block_ends_with_condjump_p) (MtcsCfgState *self,const_basic_block);
static bool rtl_block_ends_with_condjump_p_cb (MtcsCfgState *self,const_basic_block bb)
{
  return any_condjump_p (BB_END (bb));
}

/* Should move basic block BB after basic block AFTER.  NIY.  */
//原型 bool (*move_block_after) (MtcsCfgState *self,basic_block b, basic_block a);
static bool move_block_after_cb(MtcsCfgState *self,basic_block bb ATTRIBUTE_UNUSED,
              basic_block after ATTRIBUTE_UNUSED)
{
  return false;
}


//原型 void (*predict_edge) (MtcsCfgState *self,edge e, enum br_predictor predictor, int probability);
static void predict_edge_cb(MtcsCfgState *self,edge e, enum br_predictor predictor, int probability)
{
   rtl_predict_edge (e,predictor,probability); //原型 predict.h

}
//原型  bool (*predicted_by_p) (MtcsCfgState *self,const_basic_block bb, enum br_predictor predictor);
static bool predicted_by_p_cb(MtcsCfgState *self,const_basic_block bb, enum br_predictor predictor)
{
   return rtl_predicted_by_p(bb,predictor);//原型 predict.h
}

static bool rtl_bb_info_initialized_p (MtcsCfgState *self,basic_block bb)
{
  return bb->il.x.rtl;
}


/* Return true in case it is possible to duplicate the basic block BB.  */
//原型 bool (*can_duplicate_block_p) (MtcsCfgState *self,const_basic_block a);cfg_layout_can_duplicate_bb_p cfgrtl.cc
static bool can_duplicate_bb_p_cb (MtcsCfgState *self,const_basic_block bb)
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

/* Return true if BB contains only labels or non-executable
   instructions.  */
//原型 bool (*empty_block_p) (MtcsCfgState *self,basic_block);
static bool block_empty_p_cb (MtcsCfgState *self,basic_block bb)
{
  rtx_insn *insn;
  if (bb == ENTRY_BLOCK_PTR_FOR_FN (cfun) || bb == EXIT_BLOCK_PTR_FOR_FN (cfun))
      return true;

  FOR_BB_INSNS (bb, insn)
      if (NONDEBUG_INSN_P (insn)  && (!any_uncondjump_p (insn) || !onlyjump_p (insn)))
          return false;

  return true;
}



/* Print out RTL-specific basic block information (live information
   at start and end with TDF_DETAILS).  FLAGS are the TDF_* masks
   documented in dumpfile.h.  */
//原型 void (*dump_bb) (MtcsCfgState *self,FILE *, basic_block, int, dump_flags_t);
static void dump_bb_cb (MtcsCfgState *self,FILE *outf, basic_block bb, int indent, dump_flags_t flags)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);

   char *s_indent;
   s_indent = (char *) alloca ((size_t) indent + 1);
   memset (s_indent, ' ', (size_t) indent);
   s_indent[indent] = '\0';
   if (df && (flags & TDF_DETAILS)){
      mtcs_dfcore_df_dump_top/*!df_dump_top*/(mtcsDfcore,bb, outf);
      putc ('\n', outf);
   }
   if (bb->index != ENTRY_BLOCK && bb->index != EXIT_BLOCK && rtl_bb_info_initialized_p(self,bb)){
      rtx_insn *last = BB_END (bb);
      if (last)
         last = NEXT_INSN (last);
      for (rtx_insn *insn = BB_HEAD (bb); insn != last; insn = NEXT_INSN (insn)){
         if (flags & TDF_DETAILS)
            mtcs_dfcore_df_dump_insn_top/*!df_dump_insn_top*/(mtcsDfcore,insn, outf);
         if (! (flags & TDF_SLIM))
            print_rtl_single (outf, insn);
         else
            dump_insn_slim (outf, insn);
         if (flags & TDF_DETAILS)
            mtcs_dfcore_df_dump_insn_bottom/*!df_dump_insn_bottom*/(mtcsDfcore,insn, outf);
      }
   }
   if (df && (flags & TDF_DETAILS)){
      mtcs_dfcore_df_dump_bottom/*!df_dump_bottom*/(mtcsDfcore,bb, outf);
      putc ('\n', outf);
   }
}

/* Split a basic block if it ends with a conditional branch and if
   the other part of the block is not empty.  */
//原型 basic_block (*split_block_before_cond_jump) (MtcsCfgState *self,basic_block);rtl_split_block_before_cond_jump cfgrtl.cc
static basic_block split_block_before_cond_jump_cb (MtcsCfgState *self,basic_block bb)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);

   rtx_insn *insn;
   rtx_insn *split_point = NULL;
   rtx_insn *last = NULL;
   bool found_code = false;
   FOR_BB_INSNS (bb, insn) {
      if (any_condjump_p (insn))
         split_point = last;
      else if (NONDEBUG_INSN_P (insn))
         found_code = true;
      last = insn;
   }
   /* Did not find everything.  */
   if (found_code && split_point)
      return mtcs_cfg_context_split_block/*!split_block*/(mtcsCfgContext,bb, split_point)->dest;
   else
      return NULL;
}

/* Do book-keeping of basic block BB for the profile consistency checker.
   Store the counting in RECORD.  */
//原型 void (*account_profile_record) (MtcsCfgState *self,basic_block, struct profile_record *);rtl_account_profile_record_cb cfgrtl.cc
static void account_profile_record_cb (MtcsCfgState *self,basic_block bb, struct profile_record *record)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  rtx_insn *insn;
  FOR_BB_INSNS (bb, insn)
    if (INSN_P (insn)){
        record->size += mtcs_rtlanal_insn_cost/*!insn_cost*/(mtcsRtlanal,insn, false);
        if (profile_info){
            if (ENTRY_BLOCK_PTR_FOR_FN (cfun)->count.ipa ().initialized_p ()
            && ENTRY_BLOCK_PTR_FOR_FN (cfun)->count.ipa ().nonzero_p ()
            && bb->count.ipa ().initialized_p ())
              record->time  += mtcs_rtlanal_insn_cost/*!insn_cost*/(mtcsRtlanal,insn, true) * bb->count.ipa ().to_gcov_type ();
        }else if (bb->count.initialized_p ()   && ENTRY_BLOCK_PTR_FOR_FN (cfun)->count.initialized_p ())
          record->time += mtcs_rtlanal_insn_cost/*!insn_cost*/(mtcsRtlanal,insn, true)
               * bb->count.to_sreal_scale(ENTRY_BLOCK_PTR_FOR_FN (cfun)->count).to_double ();
        else
          record->time += mtcs_rtlanal_insn_cost/*!insn_cost*/(mtcsRtlanal,insn, true);
    }
}


//原型 void (*dump_bb_for_graph) (MtcsCfgState *self,pretty_printer *, basic_block);
// rtl_dump_bb_for_graph print-rtl.h print-rtl.cc
static void rtl_dump_bb_for_graph_cb(MtcsCfgState *self,pretty_printer *print, basic_block bb)
{
   rtl_dump_bb_for_graph(print,bb);
}


/* Return true if we need to add fake edge to exit.
   Helper function for rtl_flow_call_edges_add.  */
static bool need_fake_edge_p (MtcsCfgState *self,const rtx_insn *insn)
{
  if (!INSN_P (insn))
    return false;

  if ((CALL_P (insn)
       && !SIBLING_CALL_P (insn)
       && !find_reg_note (insn, REG_NORETURN, NULL)
       && !(RTL_CONST_OR_PURE_CALL_P (insn))))
    return true;

  return ((GET_CODE (PATTERN (insn)) == ASM_OPERANDS
       && MEM_VOLATILE_P (PATTERN (insn)))
      || (GET_CODE (PATTERN (insn)) == PARALLEL
          && asm_noperands (insn) != -1
          && MEM_VOLATILE_P (XVECEXP (PATTERN (insn), 0, 0)))
      || GET_CODE (PATTERN (insn)) == ASM_INPUT);
}

/* Add fake edges to the function exit for any non constant and non noreturn
   calls, volatile inline assembly in the bitmap of blocks specified by
   BLOCKS or to the whole CFG if BLOCKS is zero.  Return the number of blocks
   that were split.

   The goal is to expose cases in which entering a basic block does not imply
   that all subsequent instructions must be executed.  */
//原型 int (*flow_call_edges_add) (MtcsCfgState *self,sbitmap);rtl_flow_call_edges_add cfgrtl.cc
static int flow_call_edges_add_cb (MtcsCfgState *self,sbitmap blocks)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsCfg  *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);

  int i;
  int blocks_split = 0;
  int last_bb = last_basic_block_for_fn (cfun);
  bool check_last_block = false;

  if (n_basic_blocks_for_fn (cfun) == NUM_FIXED_BLOCKS)
      return 0;

  if (! blocks)
      check_last_block = true;
  else
      check_last_block = bitmap_bit_p (blocks,
                     EXIT_BLOCK_PTR_FOR_FN (cfun)->prev_bb->index);

  /* In the last basic block, before epilogue generation, there will be
     a fallthru edge to EXIT.  Special care is required if the last insn
     of the last basic block is a call because make_edge folds duplicate
     edges, which would result in the fallthru edge also being marked
     fake, which would result in the fallthru edge being removed by
     remove_fake_edges, which would result in an invalid CFG.

     Moreover, we can't elide the outgoing fake edge, since the block
     profiler needs to take this into account in order to solve the minimal
     spanning tree in the case that the call doesn't return.

     Handle this by adding a dummy instruction in a new last basic block.  */
  if (check_last_block){
      basic_block bb = EXIT_BLOCK_PTR_FOR_FN (cfun)->prev_bb;
      rtx_insn *insn = BB_END (bb);
      /* Back up past insns that must be kept in the same block as a call.  */
      while (insn != BB_HEAD (bb)  && keep_with_call_p (insn))
          insn = PREV_INSN (insn);

      if (need_fake_edge_p(self,insn)){
          edge e;
          e = find_edge (bb, EXIT_BLOCK_PTR_FOR_FN (cfun));
          if (e){
              mtcs_cfg_rtl_insert_insn_on_edge/*!insert_insn_on_edge*/(mtcsCfgRtl,gen_use (const0_rtx), e);
              mtcs_cfg_rtl_commit_edge_insertions/*!commit_edge_insertions*/(mtcsCfgRtl);
          }
      }
  }
  /* Now add fake edges to the function exit for any non constant
     calls since there is no way that we can determine if they will
     return or not...  */
  for (i = NUM_FIXED_BLOCKS; i < last_bb; i++){
      basic_block bb = BASIC_BLOCK_FOR_FN (cfun, i);
      rtx_insn *insn;
      rtx_insn *prev_insn;
      if (!bb)
          continue;
      if (blocks && !bitmap_bit_p (blocks, i))
          continue;
      for (insn = BB_END (bb); ; insn = prev_insn){
          prev_insn = PREV_INSN (insn);
          if (need_fake_edge_p(self,insn)){
              edge e;
              rtx_insn *split_at_insn = insn;

              /* Don't split the block between a call and an insn that should
             remain in the same block as the call.  */
              if (CALL_P (insn))
                  while (split_at_insn != BB_END (bb)  && keep_with_call_p (NEXT_INSN (split_at_insn)))
                      split_at_insn = NEXT_INSN (split_at_insn);

              /* The handling above of the final block before the epilogue
              should be enough to verify that there is no edge to the exit
               block in CFG already.  Calling make_edge in such case would
               cause us to mark that edge as fake and remove it later.  */
              if (flag_checking && split_at_insn == BB_END (bb)){
                  e = find_edge (bb, EXIT_BLOCK_PTR_FOR_FN (cfun));
                  gcc_assert (e == NULL);
              }
              /* Note that the following may create a new basic block
               and renumber the existing basic blocks.  */
              if (split_at_insn != BB_END (bb)){
                  e = mtcs_cfg_context_split_block/*!split_block*/(mtcsCfgContext,bb, split_at_insn);
                  if (e)
                    blocks_split++;
              }
              edge ne = mtcs_cfg_make_edge/*!make_edge*/(mtcsCfg,bb, EXIT_BLOCK_PTR_FOR_FN (cfun), EDGE_FAKE);
              ne->probability = profile_probability::guessed_never ();
          }
          if (insn == BB_HEAD (bb))
              break;
      }
  }
  if (blocks_split)
     mtcs_cfg_context_verify_flow_info/*!verify_flow_info*/(mtcsCfgContext);

  return blocks_split;
}



/* Create new basic block consisting of instructions in between HEAD and END
   and place it to the BB chain after block AFTER.  END can be NULL to
   create a new empty basic block before HEAD.  Both END and HEAD can be
   NULL to create basic block at the end of INSN chain.  */
//原型  basic_block (*create_basic_block) (MtcsCfgState *self,void *head, void *end, basic_block after);
basic_block mtcs_cfg_state_create_basic_block (MtcsCfgState *self,void *headp, void *endp, basic_block after)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);

   rtx_insn *head = (rtx_insn *) headp;
   rtx_insn *end = (rtx_insn *) endp;
   basic_block bb;
   /* Grow the basic block array if needed.  */
   if ((size_t) last_basic_block_for_fn (cfun) >= basic_block_info_for_fn (cfun)->length ())
      vec_safe_grow_cleared (basic_block_info_for_fn (cfun), last_basic_block_for_fn (cfun) + 1);

   n_basic_blocks_for_fn (cfun)++;

   bb = mtcs_cfg_rtl_create_basic_block_structure/*!create_basic_block_structure*/(mtcsCfgRtl,head, end, NULL, after);
   bb->aux = NULL;
   return bb;
}


/* Called when edge E has been redirected to a new destination,
   in order to update the region crossing flag on the edge and
   jump.  */
//原型 fixup_partition_crossing cfgrtl.cc
void mtcs_cfg_state_fixup_partition_crossing (MtcsCfgState *self,edge e)
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

/* Subroutine of redirect_branch_edge that tries to patch the jump
   instruction INSN so that it reaches block NEW.  Do this
   only when it originally reached block OLD.  Return true if this
   worked or the original target wasn't OLD, return false if redirection
   doesn't work.  */
//原型 patch_jump_insn cfgrtl.cc
bool mtcs_cfg_state_patch_jump_insn (MtcsCfgState *self,rtx_insn *insn, rtx_insn *old_label, basic_block new_bb)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpand  *mtcsExpand=mtcs_target_get_expand(mtcsTarget);
  MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);

  rtx_jump_table_data *table;
  rtx tmp;
  n_debug("mtcscfgstate.c mtcs_cfg_state_patch_jump_insn 00\n");
  mtcs_print_rtl_single(stderr,insn);
  n_debug("mtcscfgstate.c mtcs_cfg_state_patch_jump_insn 11\n");
  mtcs_print_rtl_single(stderr,old_label);

  /* Recognize a tablejump and adjust all matching cases.  */
  if (tablejump_p (insn, NULL, &table)){
      rtvec vec;
      int j;
      n_debug("mtcscfgstate.c mtcs_cfg_state_patch_jump_insn 22\n");

      rtx_code_label *new_label = mtcs_cfg_rtl_block_label/*!block_label*/(mtcsCfgRtl,new_bb);
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
     n_debug("mtcscfgstate.c mtcs_cfg_state_patch_jump_insn 33\n");

      int i, n = ASM_OPERANDS_LABEL_LENGTH (tmp);
      rtx note;
      if (new_bb == EXIT_BLOCK_PTR_FOR_FN (cfun))
          return false;
      rtx_code_label *new_label = mtcs_cfg_rtl_block_label/*!block_label*/(mtcsCfgRtl,new_bb);
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
     n_debug("mtcscfgstate.c mtcs_cfg_state_patch_jump_insn 44\n");

      /* ?? We may play the games with moving the named labels from
     one basic block to the other in case only one computed_jump is
     available.  */
      if (computed_jump_p (insn)
      /* A return instruction can't be redirected.  */
      || returnjump_p (insn))
          return false;
      n_debug("mtcscfgstate.c mtcs_cfg_state_patch_jump_insn 55\n");

      if (!mtcsExpand->currently_expanding_to_rtl || JUMP_LABEL (insn) == old_label){
         n_debug("mtcscfgstate.c mtcs_cfg_state_patch_jump_insn 66\n");

          /* If the insn doesn't go where we think, we're confused.  */
          gcc_assert (JUMP_LABEL (insn) == old_label);
          /* If the substitution doesn't succeed, die.  This can happen
             if the back end emitted unrecognizable instructions or if
             target is exit block on some arches.  Or for crossing
             jumps.  */
          if (!mtcs_dojump_redirect_jump/*!redirect_jump*/(mtcsDojump,
                as_a <rtx_jump_insn *> (insn),mtcs_cfg_rtl_block_label/*!block_label*/(mtcsCfgRtl,new_bb), 0)){
             n_debug("mtcscfgstate.c patch_jump_insn 77\n");
              mtcs_print_rtl(stderr,insn);
              gcc_assert (new_bb == EXIT_BLOCK_PTR_FOR_FN (cfun) || CROSSING_JUMP_P (insn));
              return false;
          }
      }
  }
  return true;
}


/* Redirect edge representing branch of (un)conditional jump or tablejump,
   NULL on failure  */
//原型 redirect_branch_edge cfgrtl.cc
edge mtcs_cfg_state_redirect_branch_edge (MtcsCfgState *self,edge e, basic_block target)
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
      if (!mtcs_cfg_state_patch_jump_insn/*!patch_jump_insn*/(self,as_a <rtx_jump_insn *> (insn), old_label, target))
          return NULL;
  }else
    /* When expanding this BB might actually contain multiple
       jumps (i.e. not yet split by find_many_sub_basic_blocks).
       Redirect all of those that match our label.  */
    FOR_BB_INSNS (src, insn)
      if (JUMP_P (insn) && !mtcs_cfg_state_patch_jump_insn/*!patch_jump_insn*/(self,as_a <rtx_jump_insn *> (insn), old_label, target))
          return NULL;

  if (dump_file)
    fprintf (dump_file, "Edge %i->%i redirected to %i\n", e->src->index, e->dest->index, target->index);
  if (e->dest != target)
    e = mtcs_cfg_context_redirect_edge_succ_nodup/*!redirect_edge_succ_nodup*/(mtcsCfgContext,e, target);
  return e;
}

/* Returns true if it is possible to remove edge E by redirecting
   it to the destination of the other edge from E->src.  */
//原型  bool (*can_remove_branch_p) (MtcsCfgState *self,const_edge);
static bool can_remove_branch_p_cb(MtcsCfgState *self,const_edge e)
{
  const_basic_block src = e->src;
  const_basic_block target = EDGE_SUCC (src, EDGE_SUCC (src, 0) == e)->dest;
  const rtx_insn *insn = BB_END (src);
  rtx set;
  /* The conditions are taken from try_redirect_by_replacing_jump.  */
  if (target == EXIT_BLOCK_PTR_FOR_FN (cfun))
    return false;

  if (e->flags & (EDGE_ABNORMAL_CALL | EDGE_EH))
    return false;

  if (BB_PARTITION (src) != BB_PARTITION (target))
    return false;

  if (!onlyjump_p (insn)  || tablejump_p (insn, NULL, NULL))
    return false;

  set = single_set (insn);
  if (!set || side_effects_p (set))
    return false;

  return true;
}

/* Delete the insns in a (non-live) block.  We physically delete every
   non-deleted-note insn, and update the flow graph appropriately.
   Return nonzero if we deleted an exception handler.  */
/* ??? Preserving all such notes strikes me as wrong.  It would be nice
   to post-process the stream to remove empty blocks, loops, ranges, etc.  */
//原型 rtl_delete_block cfgrtl.cc
void mtcs_cfg_state_rtl_delete_block (MtcsCfgState *self,basic_block b)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);

   rtx_insn *insn, *end;
   /* If the head of this block is a CODE_LABEL, then it might be the
   label for an exception handler which can't be reached.  We need
   to remove the label from the exception_handler_label list.  */
   insn = BB_HEAD (b);
   end = get_last_bb_insn (b);
   /* Selectively delete the entire chain.  */
   BB_HEAD (b) = NULL;
   mtcs_cfg_rtl_delete_insn_chain/*!delete_insn_chain */(mtcsCfgRtl,insn, end, true);
   if (dump_file)
      fprintf (dump_file, "deleting block %d\n", b->index);
   mtcs_dfcore_df_bb_delete/*!df_bb_delete*/(mtcsDfcore,b->index);
}

/* Return the INSN immediately following the NOTE_INSN_BASIC_BLOCK
   note associated with the BLOCK.  */

static rtx_insn *first_insn_after_basic_block_note (MtcsCfgState *self,basic_block block)
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

/* Creates a new basic block just after basic block BB by splitting
   everything after specified instruction INSNP.  */
//原型 rtl_split_block cfgrtl.cc
basic_block mtcs_cfg_state_split_block (MtcsCfgState *self, basic_block bb, void *insnp)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
    MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
    MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
    MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
    MtcsEmit *mtcsEmit = mtcs_target_get_emit(mtcsTarget);

    basic_block new_bb;
    rtx_insn *insn = (rtx_insn *) insnp;
    edge e;
    edge_iterator ei;

    if (!insn){
        insn = first_insn_after_basic_block_note(self,bb);
        if (insn){
            rtx_insn *next = insn;
            insn = PREV_INSN (insn);
            /* If the block contains only debug insns, insn would have
            been NULL in a non-debug compilation, and then we'd end
            up emitting a DELETED note.  For -fcompare-debug
            stability, emit the note too.  */
            if (insn != BB_END (bb)  && DEBUG_INSN_P (next)  && DEBUG_INSN_P (BB_END (bb))){
            while (next != BB_END (bb) && DEBUG_INSN_P (next))
                next = NEXT_INSN (next);

            if (next == BB_END (bb))
               mtcs_emit_emit_note_after/*!emit_note_after*/(mtcsEmit,NOTE_INSN_DELETED, next);
            }
        }else
            insn = mtcs_rtl_data_get_last_insn/*!get_last_insn*/ (mtcsRtlData);
    }

    /* We probably should check type of the insn so that we do not create
    inconsistent cfg.  It is checked in verify_flow_info anyway, so do not
    bother.  */
    if (insn == BB_END (bb))
       mtcs_emit_emit_note_after/*!emit_note_after*/(mtcsEmit,NOTE_INSN_DELETED, insn);

    /* Create the new basic block.  */
    new_bb = mtcs_cfg_context_create_basic_block/*!create_basic_block*/(mtcsCfgContext,NEXT_INSN (insn), BB_END (bb), bb);
    BB_COPY_PARTITION (new_bb, bb);
    BB_END (bb) = insn;

    /* Redirect the outgoing edges.  */
    new_bb->succs = bb->succs;
    bb->succs = NULL;
    FOR_EACH_EDGE (e, ei, new_bb->succs)
        e->src = new_bb;

    /* The new block starts off being dirty.  */
    mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,bb);
    return new_bb;
}


/* Create a duplicate of the basic block BB.  */
//原型 cfg_layout_duplicate_bb cfgrtl.cc
basic_block mtcs_cfg_state_duplicate_bb (MtcsCfgState *self,basic_block bb, copy_bb_data *id)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  rtx_insn *insn;
  basic_block new_bb;
  class loop *loop = (id && current_loops) ? bb->loop_father : NULL;
  insn = mtcs_cfg_rtl_duplicate_insn_chain/*!duplicate_insn_chain*/(mtcsCfgRtl,BB_HEAD (bb), BB_END (bb), loop, id);
  new_bb = mtcs_cfg_context_create_basic_block/*!create_basic_block*/(mtcsCfgContext,insn,
                   insn ? mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData) : NULL,
                   EXIT_BLOCK_PTR_FOR_FN (cfun)->prev_bb);

  BB_COPY_PARTITION (new_bb, bb);
  if (BB_HEADER (bb)){
      insn = BB_HEADER (bb);
      while (NEXT_INSN (insn))
          insn = NEXT_INSN (insn);
      insn = mtcs_cfg_rtl_duplicate_insn_chain/*!duplicate_insn_chain*/(mtcsCfgRtl,BB_HEADER (bb), insn, loop, id);
      if (insn)
          BB_HEADER (new_bb) = mtcs_cfg_rtl_unlink_insn_chain/*!unlink_insn_chain*/(mtcsCfgRtl,
                  insn, mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData));
  }
  if (BB_FOOTER (bb)){
      insn = BB_FOOTER (bb);
      while (NEXT_INSN (insn))
          insn = NEXT_INSN (insn);
      insn = mtcs_cfg_rtl_duplicate_insn_chain/*!duplicate_insn_chain*/(mtcsCfgRtl,BB_FOOTER (bb), insn, loop, id);
      if (insn)
          BB_FOOTER (new_bb) = mtcs_cfg_rtl_unlink_insn_chain/*!unlink_insn_chain*/(mtcsCfgRtl,
                  insn, mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData));
  }
  return new_bb;
}

/* Sanity check partition hotness to ensure that basic blocks in
   the cold partition don't dominate basic blocks in the hot partition.
   If FLAG_ONLY is true, report violations as errors. Otherwise
   re-mark the dominated blocks as cold, since this is run after
   cfg optimizations that may make hot blocks previously reached
   by both hot and cold blocks now only reachable along cold paths.  */
//mtcscfgrtl.c也调用该方法
static auto_vec<basic_block> find_partition_fixes (MtcsCfgState *self,bool flag_only)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);

  basic_block bb;
  auto_vec<basic_block> bbs_to_fix;
  hash_set<basic_block> set;
  /* Callers check this.  */
  gcc_checking_assert (mtcsRtlData/*!crtl*/->has_bb_partition);
  mtcs_cfg_rtl_find_bbs_reachable_by_hot_paths/*!find_bbs_reachable_by_hot_paths*/(mtcsCfgRtl,&set);
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


/* Verify that block pointers for instructions in basic blocks, headers and
   footers are set appropriately.  */
static bool rtl_verify_bb_pointers (MtcsCfgState *self)
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

/* Checks on the instructions within blocks. Currently checks that each
   block starts with a basic block note, and that basic block notes and
   control flow jumps are not found in the middle of the block.  */
static bool rtl_verify_bb_insns (MtcsCfgState *self)
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



/* Perform several checks on the edges out of each block, such as
   the consistency of the branch probabilities, the correctness
   of hot/cold partition crossing edges, and the number of expected
   successor edges.  Also verify that the dominance relationship
   between hot/cold blocks is sane.  */

static bool rtl_verify_edges (MtcsCfgState *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);

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
          mtcs_cfg_rtl_print_rtl_with_bb/*!print_rtl_with_bb*/(mtcsCfgRtl,stderr,
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
        && mtcs_cfg_state_get_state_type/*!current_ir_type ()*/(self) == IR_RTL_CFGLAYOUT){
      auto_vec<basic_block> bbs_to_fix = find_partition_fixes(self,true);
      err = !bbs_to_fix.is_empty ();
  }
  /* Clean up.  */
  return err;
}


/* Verify the CFG and RTL consistency common for both underlying RTL and
   cfglayout RTL.

   Currently it does following checks:

   - overlapping of basic blocks
   - insns with wrong BLOCK_FOR_INSN pointers
   - headers of basic blocks (the NOTE_INSN_BASIC_BLOCK note)
   - tails of basic blocks (ensure that boundary is necessary)
   - scans body of the basic block for JUMP_INSN, CODE_LABEL
     and NOTE_INSN_BASIC_BLOCK
   - verify that no fall_thru edge crosses hot/cold partition boundaries
   - verify that there are no pending RTL branch predictions
   - verify that hot blocks are not dominated by cold blocks

   In future it can be extended check a lot of other stuff as well
   (reachability of basic blocks, life information, etc. etc.).  */
//原型 cfg_layout_rtl_cfg_hooks member rtl_verify_flow_info_1 cfgrtl.cc
bool mtcs_cfg_state_verify_flow_info_1 (MtcsCfgState *self)
{
  bool err = false;
  if (rtl_verify_bb_pointers(self))
    err = true;
  if (rtl_verify_bb_insns(self))
    err = true;
  if (rtl_verify_edges(self))
    err = true;
  return err;
}


