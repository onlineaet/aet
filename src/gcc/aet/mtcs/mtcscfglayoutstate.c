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
#include "mtcscfglayoutstate.h"
#include "mtcstarget.h"
#include "mtcscompile.h"
#include "mtcsdfcore.h"
#include "mtcsdfproblems.h"
#include "mtcsprintrtl.h"

/* Implementation of CFG manipulation for linearized RTL.  */
//struct cfg_hooks mtcs_rtl_cfg_hooks = {
//  "rtl",
//  wrapper_rtl_verify_flow_info,                     //local
//  wrapper_rtl_dump_bb,                              //local
//  rtl_dump_bb_for_graph,                    //print-rtl.h
//  wrapper_rtl_create_basic_block,                   //local
//  wrapper_rtl_redirect_edge_and_branch,             //local
//  wrapper_rtl_redirect_edge_and_branch_force,       //local
//  wrapper_rtl_can_remove_branch_p,//local
//  wrapper_rtl_delete_block,//local
//  wrapper_rtl_split_block, //local
//  wrapper_rtl_move_block_after,//local 2
//  wrapper_rtl_can_merge_blocks,  //local /* can_merge_blocks_p */
//  wrapper_rtl_merge_blocks, //local
//  rtl_predict_edge,  //predict.h
//  rtl_predicted_by_p,//predict.h
//  wrapper_cfg_layout_can_duplicate_bb_p,//local 2
//  wrapper_rtl_duplicate_bb,//local
//  wrapper_rtl_split_edge,//local
//  wrapper_rtl_make_forwarder_block,//local
//  wrapper_rtl_tidy_fallthru_edge, //local
//  wrapper_rtl_force_nonfallthru,//local 2
//  wrapper_rtl_block_ends_with_call_p,//local 2
//  wrapper_rtl_block_ends_with_condjump_p,//local
//  wrapper_rtl_flow_call_edges_add,//local

//  NULL, /* execute_on_growing_pred */
//  NULL, /* execute_on_shrinking_pred */
//  NULL, /* duplicate loop for trees */
//  NULL, /* lv_add_condition_to_bb */
//  NULL, /* lv_adjust_loop_header_phi*/
//  NULL, /* extract_cond_bb_edges */
//  NULL, /* flush_pending_stmts */
//  wrapper_rtl_block_empty_p, //local /* block_empty_p */
//  wrapper_rtl_split_block_before_cond_jump,//local /* split_block_before_cond_jump */
//  wrapper_rtl_account_profile_record,//local
//};

//原型      basic_block (*create_basic_block) (MtcsCfgState *self,void *head, void *end, basic_block after);
static basic_block create_basic_block_cb (MtcsCfgState *self,void *head, void *end, basic_block after);
//原型      edge (*redirect_edge_and_branch) (MtcsCfgState *self,edge e, basic_block b);
static edge redirect_edge_and_branch_cb (MtcsCfgState *self, edge e, basic_block dest);
//原型      basic_block (*redirect_edge_and_branch_force) (MtcsCfgState *self,edge, basic_block);
//cfg_layout_redirect_edge_and_branch_force cfgrtl.cc
static basic_block redirect_edge_and_branch_force_cb(MtcsCfgState *self,edge e, basic_block dest);
//原型  void (*delete_basic_block) (MtcsCfgState *self,basic_block);cfg_layout_delete_block cfgrtl.cc
static void delete_basic_block_cb (MtcsCfgState *self,basic_block bb);
//原型 basic_block (*split_block) (MtcsCfgState *self,basic_block b, void * i);cfg_layout_split_block cfgrtl.cc
static basic_block split_block_cb(MtcsCfgState *self,basic_block bb, void *insnp);
//原型 bool (*can_merge_blocks_p) (MtcsCfgState *self,basic_block a, basic_block b);cfg_layout_can_merge_blocks_p cfgrtl.cc
static bool can_merge_blocks_p_cb (MtcsCfgState *self,basic_block a, basic_block b);
//原型 void (*merge_blocks) (MtcsCfgState *self,basic_block a, basic_block b);
static void merge_blocks_cb(MtcsCfgState *self, basic_block a, basic_block b);
//原型      basic_block (*duplicate_block) (MtcsCfgState *self,basic_block a, copy_bb_data *);cfg_layout_duplicate_bb cfgrtl.cc
static basic_block duplicate_block_cb (MtcsCfgState *self,basic_block bb, copy_bb_data *id);
//原型      basic_block (*split_edge) (MtcsCfgState *self,edge);cfg_layout_split_edge cfgrtl.cc
static basic_block split_edge_cb (MtcsCfgState *self,edge e);
//原型 bool (*cfg_hook_duplicate_loop_body_to_header_edge) (MtcsCfgState *self,class loop *, edge,.....
static bool cfg_hook_duplicate_loop_body_to_header_edge_cb(MtcsCfgState *self,class loop *loop, edge e,
      unsigned int ndupl, sbitmap wont_exit, edge orig, vec<edge> *to_remove, int flags);
//原型 void (*lv_add_condition_to_bb) (MtcsCfgState *self,basic_block, basic_block, basic_block,void *);rtl_lv_add_condition_to_bb cfgrtl.cc
static void lv_add_condition_to_bb_cb (MtcsCfgState *self,basic_block first_head ,
                basic_block second_head ATTRIBUTE_UNUSED, basic_block cond_bb, void *comp_rtx);
//原型 void (*extract_cond_bb_edges) (MtcsCfgState *self,basic_block, edge *, edge *);rtl_extract_cond_bb_edges cfgrtl.cc
static void extract_cond_bb_edges_cb (MtcsCfgState *self,basic_block b, edge *branch_edge,edge *fallthru_edge);

static void mtcsCfgLayoutStateInit(MtcsCfgLayoutState *self)
{
   MtcsCfgState *mtcsCfgState=(MtcsCfgState *)self;
   mtcsCfgState->name=n_strdup("cfglayout mode");
   mtcsCfgState->stateType=IR_RTL_CFGLAYOUT;
   mtcsCfgState->verify_flow_info = mtcs_cfg_state_verify_flow_info_1;
   //mtcsCfgState->dump_bb=dump_bb_cb; 父类定义
   //mtcsCfgState->dump_bb_for_graph=rtl_dump_bb_for_graph_cb; 父类定义
   mtcsCfgState->create_basic_block = create_basic_block_cb;
   mtcsCfgState->redirect_edge_and_branch = redirect_edge_and_branch_cb;
   mtcsCfgState->redirect_edge_and_branch_force = redirect_edge_and_branch_force_cb;
   //mtcsCfgState->can_remove_branch_p=can_remove_branch_p_cb; 父类定义
   mtcsCfgState->delete_basic_block = delete_basic_block_cb;
   mtcsCfgState->split_block = split_block_cb;
   //mtcsCfgState->move_block_after=move_block_after_cb;父类定义
   mtcsCfgState->can_merge_blocks_p = can_merge_blocks_p_cb;
   mtcsCfgState->merge_blocks = merge_blocks_cb;
   //mtcsCfgState->predict_edge = predict_edge_cb; 父类定义
   //mtcsCfgState->predicted_by_p = predicted_by_p_cb; 父类定义
   //mtcsCfgState->can_duplicate_bb_p = can_duplicate_bb_p_cb; 父类定义
   mtcsCfgState->duplicate_block = duplicate_block_cb;
   mtcsCfgState->split_edge = split_edge_cb;
   //mtcsCfgState->make_forwarder_block=make_forwarder_block_cb; 父类定义
   mtcsCfgState->tidy_fallthru_edge = NULL;
   //mtcsCfgState->force_nonfallthru=rtl_force_nonfallthru_cb;          父类定义
   //mtcsCfgState->block_ends_with_call_p=rtl_block_ends_with_call_p_cb; 父类定义
   //mtcsCfgState->block_ends_with_condjump_p=rtl_block_ends_with_condjump_p_cb; 父类定义
   //mtcsCfgState->flow_call_edges_add=flow_call_edges_add_cb;                      父类定义
   mtcsCfgState->execute_on_growing_pred=NULL; /* execute_on_growing_pred */
   mtcsCfgState->execute_on_shrinking_pred=NULL; /* execute_on_shrinking_pred */
   mtcsCfgState->cfg_hook_duplicate_loop_body_to_header_edge=cfg_hook_duplicate_loop_body_to_header_edge_cb;
   mtcsCfgState->lv_add_condition_to_bb=lv_add_condition_to_bb_cb;
   mtcsCfgState->lv_adjust_loop_header_phi=NULL; /* lv_adjust_loop_header_phi*/
   mtcsCfgState->extract_cond_bb_edges=extract_cond_bb_edges_cb;
   mtcsCfgState->flush_pending_stmts=NULL; /* flush_pending_stmts */
   //mtcsCfgState->empty_block_p=block_empty_p_cb;                            父类定义
   //mtcsCfgState->split_block_before_cond_jump=split_block_before_cond_jump_cb; 父类定义
   //mtcsCfgState->account_profile_record=account_profile_record_cb;             父类定义

}

//原型      basic_block (*create_basic_block) (MtcsCfgState *self,void *head, void *end, basic_block after);
static basic_block create_basic_block_cb (MtcsCfgState *self,void *head, void *end, basic_block after)
{
  basic_block newbb = mtcs_cfg_state_create_basic_block (self,head, end, after);
  return newbb;
}

static void printbb(basic_block block)
{
   rtx_insn *insn;
   int i=0;
   FOR_BB_INSNS (block, insn){
      if (!INSN_P (insn))
         continue;
      n_debug("mtcscfglayout.c 打印块中的指令 i:%d block:%p index:%d flags:%d insn:%p\n",i++,block,block->index,block->flags,insn);
      mtcs_print_rtl_single(stderr,insn);
   }
}

/* Redirect Edge to DEST.  */
//原型      edge (*redirect_edge_and_branch) (MtcsCfgState *self,edge e, basic_block b);
static edge redirect_edge_and_branch_cb (MtcsCfgState *self, edge e, basic_block dest)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsCfg *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   basic_block src = e->src;
   edge ret;
   n_debug("mtcscfglayoutstate.c redirect_edge_and_branch_cb 00 e:%p dest:%p\n",e,dest);

   if (e->flags & (EDGE_ABNORMAL_CALL | EDGE_EH))
      return NULL;
   n_debug("mtcscfglayoutstate.c redirect_edge_and_branch_cb 11 e:%p dest:%p\n",e,dest);

   if (e->dest == dest)
      return e;
   n_debug("mtcscfglayoutstate.c redirect_edge_and_branch_cb 22 e:%p dest:%p\n",e,dest);
   printbb(dest);
   if (e->flags & EDGE_CROSSING
   && BB_PARTITION (e->src) == BB_PARTITION (dest)
   && simplejump_p (BB_END (src))){
      if (dump_file)
         fprintf (dump_file, "Removing crossing jump while redirecting edge form %i to %i\n",
      e->src->index, dest->index);
      n_debug("mtcscfglayoutstate.c redirect_edge_and_branch_cb 33 e:%p dest:%p\n",e,dest);

      mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,BB_END (src));
      mtcs_cfg_rtl_remove_barriers_from_footer/*!remove_barriers_from_footer*/(mtcsCfgRtl,src);
      e->flags |= EDGE_FALLTHRU;
   }
   n_debug("mtcscfglayoutstate.c redirect_edge_and_branch_cb 44 e:%p dest:%p\n",e,dest);
   printbb(dest);

   if (e->src != ENTRY_BLOCK_PTR_FOR_FN (cfun)
   && (ret = mtcs_cfg_rtl_try_redirect_by_replacing_jump/*!try_redirect_by_replacing_jump*/(mtcsCfgRtl,e, dest, true))){
      n_debug("mtcscfglayoutstate.c redirect_edge_and_branch_cb 55 e:%p dest:%p\n",e,dest);
      mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,src);
      return ret;
   }

   if (e->src == ENTRY_BLOCK_PTR_FOR_FN (cfun)
   && (e->flags & EDGE_FALLTHRU) && !(e->flags & EDGE_COMPLEX)){
      if (dump_file)
         fprintf (dump_file, "Redirecting entry edge from bb %i to %i\n",
      e->src->index, dest->index);
      n_debug("mtcscfglayoutstate.c redirect_edge_and_branch_cb 66 e:%p dest:%p\n",e,dest);

      mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,e->src);
      mtcs_cfg_redirect_edge_succ/*!redirect_edge_succ*/(mtcsCfg,e, dest);
      return e;
   }

   /* Redirect_edge_and_branch may decide to turn branch into fallthru edge
   in the case the basic block appears to be in sequence.  Avoid this
   transformation.  */
   if (e->flags & EDGE_FALLTHRU){
      /* Redirect any branch edges unified with the fallthru one.  */
      if (JUMP_P (BB_END (src)) && label_is_jump_target_p (BB_HEAD (e->dest),BB_END (src))){
         edge redirected;

         if (dump_file)
            fprintf (dump_file, "Fallthru edge unified with branch %i->%i redirected to %i\n",
         e->src->index, e->dest->index, dest->index);
         e->flags &= ~EDGE_FALLTHRU;
         redirected = mtcs_cfg_state_redirect_branch_edge/*!redirect_branch_edge*/(self,e, dest);
         gcc_assert (redirected);
         redirected->flags |= EDGE_FALLTHRU;
         mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,redirected->src);
         return redirected;
      }
      /* In case we are redirecting fallthru edge to the branch edge
      of conditional jump, remove it.  */
      if (EDGE_COUNT (src->succs) == 2){
         /* Find the edge that is different from E.  */
         edge s = EDGE_SUCC (src, EDGE_SUCC (src, 0) == e);
         if (s->dest == dest && any_condjump_p (BB_END (src)) && onlyjump_p (BB_END (src)))
            mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,BB_END (src));
      }
      if (dump_file)
         fprintf (dump_file, "Redirecting fallthru edge %i->%i to %i\n",
      e->src->index, e->dest->index, dest->index);
      ret =mtcs_cfg_context_redirect_edge_succ_nodup/*!redirect_edge_succ_nodup*/(mtcsCfgContext,e, dest);
   }else
      ret = mtcs_cfg_state_redirect_branch_edge/*!redirect_branch_edge*/(self,e, dest);

   n_debug("mtcscfglayoutstate.c redirect_edge_and_branch_cb 77 e:%p dest:%p ret:%p\n",e,dest,ret);
   printbb(dest);

   if (!ret)
      return NULL;

   mtcs_cfg_state_fixup_partition_crossing/*!fixup_partition_crossing*/(self,ret);
   /* We don't want simplejumps in the insn stream during cfglayout.  */
   gcc_assert (!simplejump_p (BB_END (src)) || CROSSING_JUMP_P (BB_END (src)));
   mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,src);
   n_debug("mtcscfglayoutstate.c redirect_edge_and_branch_cb 88 e:%p dest:%p ret:%p\n",e,dest,ret);
   printbb(dest);
   return ret;
}

/* Simple wrapper as we always can redirect fallthru edges.  */
//原型      basic_block (*redirect_edge_and_branch_force) (MtcsCfgState *self,edge, basic_block);
static basic_block redirect_edge_and_branch_force_cb(MtcsCfgState *self,edge e, basic_block dest)
{
  edge redirected = redirect_edge_and_branch_cb(self,e, dest);
  gcc_assert (redirected);
  return NULL;
}

/* Same as delete_basic_block but update cfg_layout structures.  */
//原型  void (*delete_basic_block) (MtcsCfgState *self,basic_block);cfg_layout_delete_block cfgrtl.cc
static void delete_basic_block_cb (MtcsCfgState *self,basic_block bb)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  rtx_insn *insn, *next, *prev = PREV_INSN (BB_HEAD (bb)), *remaints;
  rtx_insn **to;
  if (BB_HEADER (bb)){
      next = BB_HEAD (bb);
      if (prev)
          SET_NEXT_INSN (prev) = BB_HEADER (bb);
      else
          mtcs_rtl_data_set_first_insn/*!set_first_insn*/(mtcsRtlData,BB_HEADER (bb));
      SET_PREV_INSN (BB_HEADER (bb)) = prev;
      insn = BB_HEADER (bb);
      while (NEXT_INSN (insn))
          insn = NEXT_INSN (insn);
      SET_NEXT_INSN (insn) = next;
      SET_PREV_INSN (next) = insn;
  }
  next = NEXT_INSN (BB_END (bb));
  if (BB_FOOTER (bb)){
      insn = BB_FOOTER (bb);
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
            break;
          insn = NEXT_INSN (insn);
      }
      if (BB_FOOTER (bb)){
          insn = BB_END (bb);
          SET_NEXT_INSN (insn) = BB_FOOTER (bb);
          SET_PREV_INSN (BB_FOOTER (bb)) = insn;
          while (NEXT_INSN (insn))
              insn = NEXT_INSN (insn);
          SET_NEXT_INSN (insn) = next;
          if (next)
              SET_PREV_INSN (next) = insn;
          else{
             n_debug("mtcscfglayoutstate.c delete_basic_block_cb %p\n",insn);

              mtcs_rtl_data_set_last_insn/*!set_last_insn*/(mtcsRtlData,insn);
          }
      }
  }
  if (bb->next_bb != EXIT_BLOCK_PTR_FOR_FN (cfun))
    to = &BB_HEADER (bb->next_bb);
  else
    to = &mtcsCfgRtl->cfg_layout_function_footer;

  mtcs_cfg_state_rtl_delete_block/*!rtl_delete_block*/(self,bb);

  if (prev)
    prev = NEXT_INSN (prev);
  else
    prev = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
  if (next)
    next = PREV_INSN (next);
  else
    next = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);

  if (next && NEXT_INSN (next) != prev){
      remaints =mtcs_cfg_rtl_unlink_insn_chain/*!unlink_insn_chain*/(mtcsCfgRtl,prev, next);
      insn = remaints;
      while (NEXT_INSN (insn))
          insn = NEXT_INSN (insn);
      SET_NEXT_INSN (insn) = *to;
      if (*to)
          SET_PREV_INSN (*to) = insn;
      *to = remaints;
  }
}

/* Same as split_block but update cfg_layout structures.  */
//原型 basic_block (*split_block) (MtcsCfgState *self,basic_block b, void * i);cfg_layout_split_block cfgrtl.cc
static basic_block split_block_cb(MtcsCfgState *self,basic_block bb, void *insnp)
{
  rtx insn = (rtx) insnp;
  basic_block new_bb = mtcs_cfg_state_split_block/*!rtl_split_block*/(self,bb, insn);
  BB_FOOTER (new_bb) = BB_FOOTER (bb);
  BB_FOOTER (bb) = NULL;
  return new_bb;
}

/* Return true when blocks A and B can be safely merged.  */
//原型 bool (*can_merge_blocks_p) (MtcsCfgState *self,basic_block a, basic_block b);cfg_layout_can_merge_blocks_p cfgrtl.cc
static bool can_merge_blocks_p_cb (MtcsCfgState *self,basic_block a, basic_block b)
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
//原型 void (*merge_blocks) (MtcsCfgState *self,basic_block a, basic_block b);
static void merge_blocks_cb(MtcsCfgState *self, basic_block a, basic_block b)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   mtcs_cfg_rtl_layout_merge_blocks(mtcsCfgRtl,a,b);
}

/* Create a duplicate of the basic block BB.  */
//原型      basic_block (*duplicate_block) (MtcsCfgState *self,basic_block a, copy_bb_data *);cfg_layout_duplicate_bb cfgrtl.cc
static basic_block duplicate_block_cb (MtcsCfgState *self,basic_block bb, copy_bb_data *id)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);

  rtx_insn *insn;
  basic_block new_bb;
  class loop *loop = (id && current_loops) ? bb->loop_father : NULL;
  n_debug("mtcscfglayot.c cfg_layout_duplicate_bb 00 %p id:%p loop:%p current_loops:%p\n",bb,id,loop,current_loops);

  insn = mtcs_cfg_rtl_duplicate_insn_chain/*!duplicate_insn_chain*/(mtcsCfgRtl,BB_HEAD (bb), BB_END (bb), loop, id);
  n_debug("mtcscfglayot.c cfg_layout_duplicate_bb 11 %p id:%p loop:%p loopheader:%p,insn:%p\n",bb,id,loop,loop->header,insn);

  new_bb = mtcs_cfg_context_create_basic_block/*!create_basic_block*/(mtcsCfgContext,insn,
                   insn ? mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData) : NULL,
                   EXIT_BLOCK_PTR_FOR_FN (cfun)->prev_bb);
  n_debug("mtcscfglayot.c cfg_layout_duplicate_bb 22 %p id:%p loop:%p loopheader:%p,insn:%p\n",bb,id,loop,loop->header,insn);

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
  n_debug("mtcscfglayot.c cfg_layout_duplicate_bb 33 %p id:%p loop:%p loopheader:%p,insn:%p\n",bb,id,loop,loop->header,insn);

  if (BB_FOOTER (bb)){
      insn = BB_FOOTER (bb);
      while (NEXT_INSN (insn))
          insn = NEXT_INSN (insn);
      insn = mtcs_cfg_rtl_duplicate_insn_chain/*!duplicate_insn_chain*/(mtcsCfgRtl,BB_FOOTER (bb), insn, loop, id);
      if (insn)
          BB_FOOTER (new_bb) = mtcs_cfg_rtl_unlink_insn_chain/*!unlink_insn_chain*/(mtcsCfgRtl,
                  insn, mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData));
  }
  n_debug("mtcscfglayot.c cfg_layout_duplicate_bb 44 %p id:%p loop:%p loopheader:%p,insn:%p\n",bb,id,loop,loop->header,insn);

  return new_bb;
}


/* Split edge E.  */
//原型      basic_block (*split_edge) (MtcsCfgState *self,edge);cfg_layout_split_edge cfgrtl.cc
static basic_block split_edge_cb (MtcsCfgState *self,edge e)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
  MtcsCfg  *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
  n_debug("mtcscfglayoutstate.c split_edge_cb %p\n",e);
  basic_block new_bb = mtcs_cfg_context_create_basic_block/*!create_basic_block*/(mtcsCfgContext,
        e->src != ENTRY_BLOCK_PTR_FOR_FN (cfun)
            ? NEXT_INSN (BB_END (e->src)) :mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData),
            NULL_RTX, e->src);

  if (e->dest == EXIT_BLOCK_PTR_FOR_FN (cfun))
      BB_COPY_PARTITION (new_bb, e->src);
  else
      BB_COPY_PARTITION (new_bb, e->dest);
  mtcs_cfg_make_edge/*!make_edge*/(mtcsCfg,new_bb, e->dest, EDGE_FALLTHRU);
  n_debug("mtcscfglayoutstate.c  split_edge_cb --xx e:%p newbb:%p loopfather:%p\n",e,new_bb,new_bb->loop_father);

  mtcs_cfg_context_redirect_edge_and_branch_force/*!redirect_edge_and_branch_force*/(mtcsCfgContext,e, new_bb);
  n_debug("mtcscfglayoutstate.c  split_edge_cb --yy e:%p newbb:%p loopfather:%p\n",e,new_bb,new_bb->loop_father);
  printbb(new_bb);
  return new_bb;
}

/* A hook for duplicating loop in CFG, currently this is used
   in loop versioning.  */
//原型 bool (*cfg_hook_duplicate_loop_body_to_header_edge) (MtcsCfgState *self,class loop *, edge,.....
static bool cfg_hook_duplicate_loop_body_to_header_edge_cb(MtcsCfgState *self,class loop *loop, edge e,
      unsigned int ndupl, sbitmap wont_exit, edge orig, vec<edge> *to_remove, int flags)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgLoopManip *mtcsCfgLoopManip = mtcs_target_get_cfg_loop_manip(mtcsTarget);
   //原型 duplicate_loop_body_to_header_edge cfgloopmanip.h cfgloopmanip.cc /* duplicate loop for rtl */
   return mtcs_cfg_loop_manip_duplicate_loop_body_to_header_edge/*!duplicate_loop_body_to_header_edge*/(mtcsCfgLoopManip,
         loop, e,ndupl, wont_exit, orig, to_remove, flags);
}

/* Add COMP_RTX as a condition at end of COND_BB.  FIRST_HEAD is
   the conditional branch target, SECOND_HEAD should be the fall-thru
   there is no need to handle this here the loop versioning code handles
   this.  the reason for SECON_HEAD is that it is needed for condition
   in trees, and this should be of the same type since it is a hook.  */
//原型 void (*lv_add_condition_to_bb) (MtcsCfgState *self,basic_block, basic_block, basic_block,void *);rtl_lv_add_condition_to_bb cfgrtl.cc
static void lv_add_condition_to_bb_cb (MtcsCfgState *self,basic_block first_head ,
                basic_block second_head ATTRIBUTE_UNUSED, basic_block cond_bb, void *comp_rtx)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);

  rtx_code_label *label;
  rtx_insn *seq, *jump;
  rtx op0 = XEXP ((rtx)comp_rtx, 0);
  rtx op1 = XEXP ((rtx)comp_rtx, 1);
  enum rtx_code comp = GET_CODE ((rtx)comp_rtx);
  machine_mode mode;


  label = mtcs_cfg_rtl_block_label/*!block_label*/(mtcsCfgRtl,first_head);
  mode = GET_MODE (op0);
  if (mode == VOIDmode)
      mode = GET_MODE (op1);

  mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
  op0 = mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,op0, NULL_RTX);
  op1 = mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,op1, NULL_RTX);
  mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,op0, op1, comp, 0, mode, NULL_RTX, NULL, label,
               profile_probability::uninitialized ());
  jump = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
  JUMP_LABEL (jump) = label;
  LABEL_NUSES (label)++;
  seq =mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
  mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
  /* Add the new cond, in the new head.  */
  mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,seq, BB_END (cond_bb));
}

/* Given a block B with unconditional branch at its end, get the
   store the return the branch edge and the fall-thru edge in
   BRANCH_EDGE and FALLTHRU_EDGE respectively.  */
//原型 void (*extract_cond_bb_edges) (MtcsCfgState *self,basic_block, edge *, edge *);rtl_extract_cond_bb_edges cfgrtl.cc
static void extract_cond_bb_edges_cb (MtcsCfgState *self,basic_block b, edge *branch_edge,edge *fallthru_edge)
{
  edge e = EDGE_SUCC (b, 0);
  if (e->flags & EDGE_FALLTHRU){
      *fallthru_edge = e;
      *branch_edge = EDGE_SUCC (b, 1);
  }else{
      *branch_edge = e;
      *fallthru_edge = EDGE_SUCC (b, 1);
  }
}

MtcsCfgLayoutState *mtcs_cfg_layout_state_new(MtcsMode *mtcsMode)
{
    MtcsCfgLayoutState *self = n_slice_alloc0 (sizeof(MtcsCfgLayoutState));
    mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
    mtcs_cfg_state_init((MtcsCfgState *)self);
    mtcsCfgLayoutStateInit(self);
    return self;
}




