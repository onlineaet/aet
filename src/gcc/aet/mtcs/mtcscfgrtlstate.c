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
#include "mtcscfgrtlstate.h"
#include "mtcstarget.h"
#include "mtcscompile.h"
#include "mtcsdfcore.h"
#include "mtcsdfproblems.h"
#include "mtcsprintrtl.h"

/* Implementation of CFG manipulation for linearized RTL.  */


static bool rtl_verify_flow_info (MtcsCfgState *self);
//原型 edge (*redirect_edge_and_branch) (MtcsCfgState *self,edge e, basic_block b);
static edge rtl_redirect_edge_and_branch (MtcsCfgState *self,edge e, basic_block target);
//原型 basic_block (*redirect_edge_and_branch_force) (MtcsCfgState *self,edge, basic_block);
static basic_block rtl_redirect_edge_and_branch_force (MtcsCfgState *self,edge e, basic_block target);
//原型  bool (*can_merge_blocks_p) (MtcsCfgState *self,basic_block a, basic_block b);
static bool rtl_can_merge_blocks (MtcsCfgState *self,basic_block a, basic_block b);
//原型  void (*merge_blocks) (MtcsCfgState *self,basic_block a, basic_block b);
static void rtl_merge_blocks (MtcsCfgState *self,basic_block a, basic_block b);
//原型 basic_block (*duplicate_block) (MtcsCfgState *self,basic_block a, copy_bb_data *);
static basic_block rtl_duplicate_bb (MtcsCfgState *self,basic_block bb, copy_bb_data *id);
//原型  basic_block (*split_edge) (MtcsCfgState *self,edge);
static basic_block rtl_split_edge_cb (MtcsCfgState *self,edge edge_in);
//原型  void (*tidy_fallthru_edge) (MtcsCfgState *self,edge);
static void rtl_tidy_fallthru_edge (MtcsCfgState *self,edge e);


static void mtcsCfgRtlStateInit(MtcsCfgRtlState *self)
{
   MtcsCfgState *mtcsCfgState=(MtcsCfgState *)self;
   mtcsCfgState->name=n_strdup("rtl");
   mtcsCfgState->stateType=IR_RTL_CFGRTL;
   mtcsCfgState->verify_flow_info = rtl_verify_flow_info;
   //mtcsCfgState->dump_bb=dump_bb_cb; 父类定义
   //mtcsCfgState->dump_bb_for_graph=rtl_dump_bb_for_graph_cb; 父类定义

   mtcsCfgState->create_basic_block = mtcs_cfg_state_create_basic_block;
   mtcsCfgState->redirect_edge_and_branch = rtl_redirect_edge_and_branch;
   mtcsCfgState->redirect_edge_and_branch_force = rtl_redirect_edge_and_branch_force;
   //mtcsCfgState->can_remove_branch_p=can_remove_branch_p_cb; 父类定义
   mtcsCfgState->delete_basic_block = mtcs_cfg_state_rtl_delete_block;
   mtcsCfgState->split_block = mtcs_cfg_state_split_block;
   //mtcsCfgState->move_block_after=move_block_after_cb;父类定义

   mtcsCfgState->can_merge_blocks_p = rtl_can_merge_blocks;
   mtcsCfgState->merge_blocks = rtl_merge_blocks;
   //mtcsCfgState->predict_edge = predict_edge_cb; 父类定义
   //mtcsCfgState->predicted_by_p = predicted_by_p_cb; 父类定义
   //mtcsCfgState->can_duplicate_bb_p = can_duplicate_bb_p_cb; 父类定义
   mtcsCfgState->duplicate_block = rtl_duplicate_bb;
   mtcsCfgState->split_edge = rtl_split_edge_cb;
   //mtcsCfgState->make_forwarder_block=make_forwarder_block_cb; 父类定义
   mtcsCfgState->tidy_fallthru_edge = rtl_tidy_fallthru_edge;
   //mtcsCfgState->force_nonfallthru=rtl_force_nonfallthru_cb;          父类定义
   //mtcsCfgState->block_ends_with_call_p=rtl_block_ends_with_call_p_cb; 父类定义
   //mtcsCfgState->block_ends_with_condjump_p=rtl_block_ends_with_condjump_p_cb; 父类定义
   //mtcsCfgState->flow_call_edges_add = rtl_flow_call_edges_add;            父类定义

   mtcsCfgState->execute_on_growing_pred=NULL; /* execute_on_growing_pred */
   mtcsCfgState->execute_on_shrinking_pred=NULL; /* execute_on_shrinking_pred */
   mtcsCfgState->cfg_hook_duplicate_loop_body_to_header_edge=NULL;/* duplicate loop for trees */
   mtcsCfgState->lv_add_condition_to_bb=NULL;                    /* lv_add_condition_to_bb */
   mtcsCfgState->lv_adjust_loop_header_phi=NULL; /* lv_adjust_loop_header_phi*/
   mtcsCfgState->extract_cond_bb_edges=NULL;    /* extract_cond_bb_edges */
   mtcsCfgState->flush_pending_stmts=NULL; /* flush_pending_stmts */
   //mtcsCfgState->empty_block_p=block_empty_p_cb;                            父类定义
   //mtcsCfgState->split_block_before_cond_jump=split_block_before_cond_jump_cb; 父类定义
   //mtcsCfgState->account_profile_record=account_profile_record_cb;             父类定义
}


/* Verify, in the basic block chain, that there is at most one switch
   between hot/cold partitions. This condition will not be true until
   after reorder_basic_blocks is called.  */
static bool verify_hot_cold_block_grouping (MtcsCfgState *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  basic_block bb;
  bool err = false;
  bool switched_sections = false;
  int current_partition = BB_UNPARTITIONED;

  /* Even after bb reordering is complete, we go into cfglayout mode
     again (in compgoto). Ensure we don't call this before going back
     into linearized RTL when any layout fixes would have been committed.  */
  if (!mtcsRtlData/*!crtl*/->bb_reorder_complete  ||
        mtcs_cfg_state_get_state_type/*!current_ir_type ()*/(self) != IR_RTL_CFGRTL)
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



/* Verify that blocks are laid out in consecutive order. While walking the
   instructions, verify that all expected instructions are inside the basic
   blocks, and that all returns are followed by barriers.  */
static bool rtl_verify_bb_layout (MtcsCfgState *self)
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

/* Verify that fallthru edges point to adjacent blocks in layout order and
   that barriers exist after non-fallthru blocks.  */
static bool rtl_verify_fallthru (MtcsCfgState *self)
{
  basic_block bb;
  bool err = false;
  FOR_EACH_BB_REVERSE_FN (bb, cfun){
      edge e;
      e = find_fallthru_edge (bb->succs);
      if (!e){
          rtx_insn *insn;
          /* Ensure existence of barrier in BB with no fallthru edges.  */
          for (insn = NEXT_INSN (BB_END (bb)); ; insn = NEXT_INSN (insn)){
              if (!insn || NOTE_INSN_BASIC_BLOCK_P (insn)){
                  error ("missing barrier after block %i", bb->index);
                  err = true;
                  break;
              }
              if (BARRIER_P (insn))
                  break;
          }
      }else if (e->src != ENTRY_BLOCK_PTR_FOR_FN (cfun)  && e->dest != EXIT_BLOCK_PTR_FOR_FN (cfun)){
          rtx_insn *insn;
          if (e->src->next_bb != e->dest){
              error("verify_flow_info: Incorrect blocks for fallthru %i->%i",e->src->index, e->dest->index);
              err = true;
          }else
              for (insn = NEXT_INSN (BB_END (e->src)); insn != BB_HEAD (e->dest); insn = NEXT_INSN (insn))
                  if (BARRIER_P (insn) || NONDEBUG_INSN_P (insn)){
                      error ("verify_flow_info: Incorrect fallthru %i->%i",
                         e->src->index, e->dest->index);
                      error ("wrong insn in the fallthru edge");
                      debug_rtx (insn);
                      err = true;
                  }
      }
   }
   return err;
}



/* Walk the instruction chain and verify that bb head/end pointers
  are correct, and that instructions are in exactly one bb and have
  correct block pointers.  */
/* 遍历指令链并验证 bb 头/尾指针
是否正确，以及指令是否恰好位于一个 bb 中并具有
正确的块指针。 */
static bool rtl_verify_bb_insn_chain (MtcsCfgState *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  basic_block bb;
  bool err = false;
  rtx_insn *x;
  rtx_insn *last_head = mtcs_rtl_data_get_last_insn/*!get_last_insn*/ (mtcsRtlData);
  basic_block *bb_info;
  const int max_uid =mtcs_rtl_data_get_max_uid/*!get_max_uid*/(mtcsRtlData);
  bb_info = XCNEWVEC (basic_block, max_uid);
  FOR_EACH_BB_REVERSE_FN (bb, cfun){
      rtx_insn *head = BB_HEAD (bb);
      rtx_insn *end = BB_END (bb);
      for (x = last_head; x != NULL_RTX; x = PREV_INSN (x)){
          //mtcs_print_rtl(stderr,x);
          /* Verify the end of the basic block is in the INSN chain.  */
          if (x == end){
            //n_debug("mtcscfgrtlstate.c rtl_verify_bb_insn_chain 00\n");
            break;
          }
            /* And that the code outside of basic blocks has NULL bb field.  */
          if (!BARRIER_P (x) && BLOCK_FOR_INSN (x) != NULL){
              //n_debug("mtcscfgrtlstate.c rtl_verify_bb_insn_chain 11 %d %p\n",BARRIER_P (x),BLOCK_FOR_INSN (x));
              error ("insn %d outside of basic blocks has non-NULL bb field", INSN_UID (x));
              err = true;
          }
      }
      if (!x){
          error ("end insn %d for block %d not found in the insn stream",INSN_UID (end), bb->index);
          err = true;
      }
      /* Work backwards from the end to the head of the basic block
     to verify the head is in the RTL chain.  */
      for (; x != NULL_RTX; x = PREV_INSN (x)){
          /* While walking over the insn chain, verify insns appear
             in only one basic block.  */
          if (bb_info[INSN_UID (x)] != NULL){
              error ("insn %d is in multiple basic blocks (%d and %d)",INSN_UID (x), bb->index, bb_info[INSN_UID (x)]->index);
              err = true;
          }
          //n_debug("mtcscfgrtlstate.c rtl_verify_bb_insn_chain 22\n");

          bb_info[INSN_UID (x)] = bb;
          if (x == head){
             //n_debug("mtcscfgrtlstate.c rtl_verify_bb_insn_chain 33\n");
            break;
          }
      }
      if (!x){
         error ("head insn %d for block %d not found in the insn stream", INSN_UID (head), bb->index);
         err = true;
      }
      last_head = PREV_INSN (x);
  }
  for (x = last_head; x != NULL_RTX; x = PREV_INSN (x)){
      /* Check that the code before the first basic block has NULL
     bb field.  */
      if (!BARRIER_P (x) && BLOCK_FOR_INSN (x) != NULL){
         //n_debug("mtcscfgrtlstate.c rtl_verify_bb_insn_chain 44\n");
          error ("insn %d outside of basic blocks has non-NULL bb field", INSN_UID (x));
          err = true;
      }
  }
  free (bb_info);
  return err;
}

/* Verify the CFG and RTL consistency common for both underlying RTL and
   cfglayout RTL, plus consistency checks specific to linearized RTL mode.

   Currently it does following checks:
   - all checks of rtl_verify_flow_info_1
   - test head/end pointers
   - check that blocks are laid out in consecutive order
   - check that all insns are in the basic blocks
     (except the switch handling code, barriers and notes)
   - check that all returns are followed by barriers
   - check that all fallthru edge points to the adjacent blocks
   - verify that there is a single hot/cold partition boundary after bbro  */
static bool rtl_verify_flow_info (MtcsCfgState *self)
{
  bool err = false;
  if (mtcs_cfg_state_verify_flow_info_1/*!rtl_verify_flow_info_1*/(self))
    err = true;

  if (rtl_verify_bb_insn_chain(self))
    err = true;

  if (rtl_verify_fallthru(self))
    err = true;

  if (rtl_verify_bb_layout(self))
    err = true;

  if (verify_hot_cold_block_grouping(self))
    err = true;

  return err;
}

/* Attempt to change code to redirect edge E to TARGET.  Don't do that on
   expense of adding new instructions or reordering basic blocks.

   Function can be also called with edge destination equivalent to the TARGET.
   Then it should try the simplifications and do nothing if none is possible.

   Return edge representing the branch if transformation succeeded.  Return NULL
   on failure.
   We still return NULL in case E already destinated TARGET and we didn't
   managed to simplify instruction stream.  */
//原型 edge (*redirect_edge_and_branch) (MtcsCfgState *self,edge e, basic_block b);
static edge rtl_redirect_edge_and_branch (MtcsCfgState *self,edge e, basic_block target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);

   edge ret;
   basic_block src = e->src;
   basic_block dest = e->dest;
   if (e->flags & (EDGE_ABNORMAL_CALL | EDGE_EH))
      return NULL;
   if (dest == target)
      return e;
   n_debug("mtcscfgrtlstate.c rtl_redirect_edge_and_branch 00 bb:%p\n",target);
   if ((ret =mtcs_cfg_rtl_try_redirect_by_replacing_jump/*!try_redirect_by_replacing_jump*/(mtcsCfgRtl,
   e, target, false)) != NULL){
      mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,src);
      mtcs_cfg_state_fixup_partition_crossing/*!fixup_partition_crossing*/(self,ret);
      return ret;
   }
   n_debug("mtcscfgrtlstate.c rtl_redirect_edge_and_branch 11 bb:%p\n",target);

   ret = mtcs_cfg_state_redirect_branch_edge/*!redirect_branch_edge*/(self,e, target);
   if (!ret)
      return NULL;
   n_debug("mtcscfgrtlstate.c rtl_redirect_edge_and_branch 22 bb:%p\n",target);

   mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,src);
   mtcs_cfg_state_fixup_partition_crossing/*!fixup_partition_crossing*/(self,ret);
   return ret;
}

/* Redirect edge even at the expense of creating new jump insn or
   basic block.  Return new basic block if created, NULL otherwise.
   Conversion must be possible.  */
//原型 basic_block (*redirect_edge_and_branch_force) (MtcsCfgState *self,edge, basic_block);
static basic_block rtl_redirect_edge_and_branch_force (MtcsCfgState *self,edge e, basic_block target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);

  if (mtcs_cfg_context_redirect_edge_and_branch/*!redirect_edge_and_branch*/(mtcsCfgContext,e, target)
      || e->dest == target)
    return NULL;

  /* In case the edge redirection failed, try to force it to be non-fallthru
     and redirect newly created simplejump.  */
  mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,e->src);
  return mcs_cfg_rtl_force_nonfallthru_and_redirect/*!force_nonfallthru_and_redirect*/(mtcsCfgRtl,e, target, NULL_RTX);
}

/* Return true when block A and B can be merged.  */
//原型  bool (*can_merge_blocks_p) (MtcsCfgState *self,basic_block a, basic_block b);
static bool rtl_can_merge_blocks (MtcsCfgState *self,basic_block a, basic_block b)
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

/* Blocks A and B are to be merged into a single block A.  The insns
   are already contiguous.  */
//原型  void (*merge_blocks) (MtcsCfgState *self,basic_block a, basic_block b);
static void rtl_merge_blocks (MtcsCfgState *self,basic_block a, basic_block b)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
  mtcs_cfg_rtl_merge_blocks(mtcsCfgRtl,a,b);
}

//原型 basic_block (*duplicate_block) (MtcsCfgState *self,basic_block a, copy_bb_data *);
static basic_block rtl_duplicate_bb (MtcsCfgState *self,basic_block bb, copy_bb_data *id)
{
  bb = mtcs_cfg_state_duplicate_bb/*!cfg_layout_duplicate_bb*/(self,bb, id);
  bb->aux = NULL;
  return bb;
}

/* Locate the last bb in the same partition as START_BB.  */
static basic_block last_bb_in_partition (MtcsCfgState *self,basic_block start_bb)
{
  basic_block bb;
  FOR_BB_BETWEEN (bb, start_bb, EXIT_BLOCK_PTR_FOR_FN (cfun), next_bb){
      if (BB_PARTITION (start_bb) != BB_PARTITION (bb->next_bb))
        return bb;
  }
  /* Return bb before the exit block.  */
  return bb->prev_bb;
}

/* Split a (typically critical) edge.  Return the new block.
   The edge must not be abnormal.

   ??? The code generally expects to be called on critical edges.
   The case of a block ending in an unconditional jump to a
   block with multiple predecessors is not handled optimally.  */
//原型  basic_block (*split_edge) (MtcsCfgState *self,edge);
static basic_block rtl_split_edge_cb (MtcsCfgState *self,edge edge_in)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
  MtcsCfg *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
  MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
  n_debug("mtcscfgrtlstate.c rtl_split_edge_cb 00\n");
  basic_block bb, new_bb;
  rtx_insn *before;
  /* Abnormal edges cannot be split.  */
  gcc_assert (!(edge_in->flags & EDGE_ABNORMAL));
  /* We are going to place the new block in front of edge destination.
     Avoid existence of fallthru predecessors.  */
  if ((edge_in->flags & EDGE_FALLTHRU) == 0){
      edge e = find_fallthru_edge (edge_in->dest->preds);
      if (e)
         mtcs_cfg_context_force_nonfallthru/*!force_nonfallthru*/(mtcsCfgContext,e);
  }
  /* Create the basic block note.  */
  if (edge_in->dest != EXIT_BLOCK_PTR_FOR_FN (cfun))
      before = BB_HEAD (edge_in->dest);
  else
      before = NULL;
  /* If this is a fall through edge to the exit block, the blocks might be
     not adjacent, and the right place is after the source.  */
  if ((edge_in->flags & EDGE_FALLTHRU)  && edge_in->dest == EXIT_BLOCK_PTR_FOR_FN (cfun)){
      before = NEXT_INSN (BB_END (edge_in->src));
      bb = mtcs_cfg_context_create_basic_block/*!create_basic_block*/(mtcsCfgContext,before, NULL, edge_in->src);
      BB_COPY_PARTITION (bb, edge_in->src);
  }else{
      if (edge_in->src == ENTRY_BLOCK_PTR_FOR_FN (cfun)){
          bb = mtcs_cfg_context_create_basic_block/*!create_basic_block*/(mtcsCfgContext,before, NULL, edge_in->dest->prev_bb);
          BB_COPY_PARTITION (bb, edge_in->dest);
      }else{
          basic_block after = edge_in->dest->prev_bb;
          /* If this is post-bb reordering, and the edge crosses a partition
             boundary, the new block needs to be inserted in the bb chain
             at the end of the src partition (since we put the new bb into
             that partition, see below). Otherwise we may end up creating
             an extra partition crossing in the chain, which is illegal.
             It can't go after the src, because src may have a fall-through
             to a different block.  */
          if (mtcsRtlData/*!crtl*/->bb_reorder_complete && (edge_in->flags & EDGE_CROSSING)){
              after = last_bb_in_partition(self,edge_in->src);
              before = get_last_bb_insn (after);
              /* The instruction following the last bb in partition should
                 be a barrier, since it cannot end in a fall-through.  */
              gcc_checking_assert (BARRIER_P (before));
              before = NEXT_INSN (before);
          }
          bb = mtcs_cfg_context_create_basic_block/*!create_basic_block*/(mtcsCfgContext,before, NULL, after);
          /* Put the split bb into the src partition, to avoid creating
             a situation where a cold bb dominates a hot bb, in the case
             where src is cold and dest is hot. The src will dominate
             the new bb (whereas it might not have dominated dest).  */
          BB_COPY_PARTITION (bb, edge_in->src);
      }
  }
  mtcs_cfg_make_single_succ_edge/*!make_single_succ_edge*/(mtcsCfg,bb, edge_in->dest, EDGE_FALLTHRU);
  /* Can't allow a region crossing edge to be fallthrough.  */
  if (BB_PARTITION (bb) != BB_PARTITION (edge_in->dest)
      && edge_in->dest != EXIT_BLOCK_PTR_FOR_FN (cfun)){
      new_bb = force_nonfallthru (single_succ_edge (bb));
      gcc_assert (!new_bb);
  }
  /* For non-fallthru edges, we must adjust the predecessor's
     jump instruction to target our new block.  */
  if ((edge_in->flags & EDGE_FALLTHRU) == 0){
      edge redirected = mtcs_cfg_context_redirect_edge_and_branch/*!redirect_edge_and_branch*/(mtcsCfgContext,edge_in, bb);
      gcc_assert (redirected);
  }else{
      if (edge_in->src != ENTRY_BLOCK_PTR_FOR_FN (cfun)){
          /* For asm goto even splitting of fallthru edge might
             need insn patching, as other labels might point to the
             old label.  */
          rtx_insn *last = BB_END (edge_in->src);
          if (last
              && JUMP_P (last)
              && edge_in->dest != EXIT_BLOCK_PTR_FOR_FN (cfun)
              && (extract_asm_operands (PATTERN (last))
              || JUMP_LABEL (last) == before)
              && mtcs_cfg_state_patch_jump_insn/*!patch_jump_insn*/(self,last, before, bb))
            mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,edge_in->src);
      }
      mtcs_cfg_redirect_edge_succ/*!redirect_edge_succ*/(mtcsCfg,edge_in, bb);
  }

  return bb;
}

/* The given edge should potentially be a fallthru edge.  If that is in
   fact true, delete the jump and barriers that are in the way.  */
//原型  void (*tidy_fallthru_edge) (MtcsCfgState *self,edge);
static void rtl_tidy_fallthru_edge (MtcsCfgState *self,edge e)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
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
          mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,table);
      }

      q = PREV_INSN (q);
  }
  /* Unconditional jumps with side-effects (i.e. which we can't just delete
     together with the barrier) should never have a fallthru edge.  */
  else if (JUMP_P (q) && any_uncondjump_p (q))
    return;
  /* Selectively unlink the sequence.  */
  if (q != PREV_INSN (BB_HEAD (c)))
      mtcs_cfg_rtl_delete_insn_chain/*!delete_insn_chain */(mtcsCfgRtl,NEXT_INSN (q), PREV_INSN (BB_HEAD (c)), false);

  e->flags |= EDGE_FALLTHRU;
}

MtcsCfgRtlState *mtcs_cfg_rtl_state_new(MtcsMode *mtcsMode)
{
    MtcsCfgRtlState *self = n_slice_alloc0 (sizeof(MtcsCfgRtlState));
    mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
    mtcs_cfg_state_init((MtcsCfgState *)self);
    mtcsCfgRtlStateInit(self);
    return self;
}




