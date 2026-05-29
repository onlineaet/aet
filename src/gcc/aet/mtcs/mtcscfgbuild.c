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
 * base on cfgbuild.cc
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
#include "cfganal.h"
#include "except.h"
#include "stmt.h"

#include "aet/aetprinttree.h"
#include "mtcscfgbuild.h"
#include "mtcstarget.h"
#include "mtcscompile.h"
#include "mtcsprintrtl.h"

/* States of basic block as seen by find_many_sub_basic_blocks.  */
enum state {
  /* Basic blocks created via split_block belong to this state.
     make_edges will examine these basic blocks to see if we need to
     create edges going out of them.  */
  BLOCK_NEW = 0,

  /* Basic blocks that do not need examining belong to this state.
     These blocks will be left intact.  In particular, make_edges will
     not create edges going out of these basic blocks.  */
  BLOCK_ORIGINAL,

  /* Basic blocks that may need splitting (due to a label appearing in
     the middle, etc) belong to this state.  After splitting them,
     make_edges will create edges going out of them as needed.  */
  BLOCK_TO_SPLIT
};

#define STATE(BB) (enum state) ((size_t) (BB)->aux)
#define SET_STATE(BB, STATE) ((BB)->aux = (void *) (size_t) (STATE))

/* Used internally by purge_dead_tablejump_edges, ORed into state.  */
#define BLOCK_USED_BY_TABLEJUMP     32
#define FULL_STATE(BB) ((size_t) (BB)->aux)

static void test_rtl_verify_edges ()
{
   if(!n_log_is_debug())
      return;
   basic_block bb;

   FOR_EACH_BB_REVERSE_FN (bb, cfun){
      int  n_branch = 0;
      int n_eh = 0, n_abnormal = 0;
      edge e, fallthru = NULL;
      edge_iterator ei;


      if(bb->index==13 || bb->index==14 || bb->index==15 || bb->index==16 || bb->index==17) {
         FOR_EACH_EDGE (e, ei, bb->succs){
            if ((e->flags & ~(EDGE_DFS_BACK
                    | EDGE_CAN_FALLTHRU
                    | EDGE_IRREDUCIBLE_LOOP
                    | EDGE_LOOP_EXIT
                    | EDGE_CROSSING
                    | EDGE_PRESERVE)) == 0)
               n_branch++;
         }
         bool re= BB_END (bb)?any_uncondjump_p (BB_END (bb)):false;
         n_debug("mtcscfgbuild.c rtl_verify_edges 00 bb:%p index:%d n_branch:%d BB_END (bb):%p any_uncondjump_p:%d\n",
         bb,bb->index,n_branch,BB_END (bb),re);
      }
   }
}


/* Create an edge between two basic blocks.  FLAGS are auxiliary information
   about the edge that is accumulated between calls.  */

/* Create an edge from a basic block to a label.  */

static void make_label_edge (MtcsCfgBuild *self,sbitmap edge_cache, basic_block src, rtx label, int flags)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfg  *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);

   gcc_assert (LABEL_P (label));
   /* If the label was never emitted, this insn is junk, but avoid a
   crash trying to refer to BLOCK_FOR_INSN (label).  This can happen
   as a result of a syntax error and a diagnostic has already been
   printed.  */
   if (INSN_UID (label) == 0){
      n_debug("mtcscfgbuild.c make_label_edge INSN_UID (label) == 0 返回 src:%p %d flags:%d label:%p\n",src,src->index,flags,label);
      return;
   }
   n_debug("mtcscfgbuild.c make_label_edge 00 src:%p %d flags:%d\n",src,src->index,flags);
   mtcs_cfg_cached_make_edge/*!cached_make_edge*/(mtcsCfg,edge_cache, src, BLOCK_FOR_INSN (label), flags);
}

/* Identify the edges going out of basic blocks between MIN and MAX,
   inclusive, that have their states set to BLOCK_NEW or
   BLOCK_TO_SPLIT.

   UPDATE_P should be nonzero if we are updating CFG and zero if we
   are building CFG from scratch.  */
//原型 make_edges cfgbuild.cc
static void make_edges (MtcsCfgBuild *self,basic_block min, basic_block max, int update_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfg  *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsExcept *mtcsExcept =mtcs_target_get_except(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   basic_block bb;
   sbitmap edge_cache = NULL;

   /* Heavy use of computed goto in machine-generated code can lead to
   nearly fully-connected CFGs.  In that case we spend a significant
   amount of time searching the edge lists for duplicates.  */
   if (!vec_safe_is_empty (mtcsRtlData/*!crtl*/->expr.x_forced_labels/*!forced_labels*/)
   || cfun->cfg->max_jumptable_ents > 100){
      n_debug("mtcscfgbuild.c make_edges 00 min:%p %d max:%p %d\n",min,min->index,max,max->index);

      edge_cache = sbitmap_alloc (last_basic_block_for_fn (cfun));
   }

   /* By nature of the way these get numbered, ENTRY_BLOCK_PTR->next_bb block
   is always the entry.  */
   if (min == ENTRY_BLOCK_PTR_FOR_FN (cfun)->next_bb){
      n_debug("mtcscfgbuild.c make_edges 11 min:%p %d max:%p %d\n",min,min->index,max,max->index);

      mtcs_cfg_make_edge/*!make_edge*/(mtcsCfg,ENTRY_BLOCK_PTR_FOR_FN (cfun), min, EDGE_FALLTHRU);
   }
   n_debug("mtcscfgbuild.c make_edges 22 min:%p %d max:%p %d\n",min,min->index,max,max->index);
   test_rtl_verify_edges();
   FOR_BB_BETWEEN (bb, min, max->next_bb, next_bb){
      rtx_insn *insn;
      enum rtx_code code;
      edge e;
      edge_iterator ei;
      n_debug("mtcscfgbuild.c make_edges 33 min:%p %d max:%p %d bb:%p %d STATE (bb) == BLOCK_ORIGINAL:%d\n",
            min,min->index,max,max->index,bb,bb->index,STATE (bb) == BLOCK_ORIGINAL);

      if (STATE (bb) == BLOCK_ORIGINAL)
         continue;

      /* If we have an edge cache, cache edges going out of BB.  */
      if (edge_cache){
         bitmap_clear (edge_cache);
         if (update_p){
            n_debug("mtcscfgbuild.c make_edges 44 min:%p %d max:%p %d bb:%p %d\n",min,min->index,max,max->index,bb,bb->index);

            FOR_EACH_EDGE (e, ei, bb->succs)
               if (e->dest != EXIT_BLOCK_PTR_FOR_FN (cfun))
                  bitmap_set_bit (edge_cache, e->dest->index);
         }
      }

      if (LABEL_P (BB_HEAD (bb)) && LABEL_ALT_ENTRY_P (BB_HEAD (bb))){
         n_debug("mtcscfgbuild.c make_edges 55 min:%p %d max:%p %d bb:%p %d\n",min,min->index,max,max->index,bb,bb->index);

         mtcs_cfg_cached_make_edge/*!cached_make_edge*/(mtcsCfg,NULL, ENTRY_BLOCK_PTR_FOR_FN (cfun), bb, 0);
      }
      /* Examine the last instruction of the block, and discover the
      ways we can leave the block.  */

      insn = BB_END (bb);
      code = GET_CODE (insn);

      /* A branch.  */
      if (code == JUMP_INSN){
         rtx tmp;
         rtx_jump_table_data *table;
         n_debug("mtcscfgbuild.c make_edges 66 min:%p %d max:%p %d bb:%p %d\n",min,min->index,max,max->index,bb,bb->index);

         /* Recognize a non-local goto as a branch outside the
         current function.  */
         if (find_reg_note (insn, REG_NON_LOCAL_GOTO, NULL_RTX))
            ;
         /* Recognize a tablejump and do the right thing.  */
         else if (tablejump_p (insn, NULL, &table)){
            rtvec vec = table->get_labels ();
            int j;
            n_debug("mtcscfgbuild.c make_edges 77 min:%p %d max:%p %d bb:%p %d\n",min,min->index,max,max->index,bb,bb->index);

            for (j = GET_NUM_ELEM (vec) - 1; j >= 0; --j)
               make_label_edge(self,edge_cache, bb,XEXP (RTVEC_ELT (vec, j), 0), 0);

            /* Some targets (eg, ARM) emit a conditional jump that also
            contains the out-of-range target.  Scan for these and
            add an edge if necessary.  */
            if ((tmp = single_set (insn)) != NULL
            && SET_DEST (tmp) == pc_rtx
            && GET_CODE (SET_SRC (tmp)) == IF_THEN_ELSE
            && GET_CODE (XEXP (SET_SRC (tmp), 2)) == LABEL_REF)
               make_label_edge(self,edge_cache, bb, label_ref_label (XEXP (SET_SRC (tmp), 2)), 0);
         }
         /* If this is a computed jump, then mark it as reaching
         everything on the forced_labels list.  */
         else if (computed_jump_p (insn)){
            rtx_insn *insn;
            unsigned int i;
            n_debug("mtcscfgbuild.c make_edges 88 min:%p %d max:%p %d bb:%p %d\n",min,min->index,max,max->index,bb,bb->index);

            FOR_EACH_VEC_SAFE_ELT (mtcsRtlData/*!crtl*/->expr.x_forced_labels/*!forced_labels*/, i, insn)
               make_label_edge(self,edge_cache, bb, insn, EDGE_ABNORMAL);
         }
         /* Returns create an exit out.  */
         else if (returnjump_p (insn)){
            n_debug("mtcscfgbuild.c make_edges 99 min:%p %d max:%p %d bb:%p %d\n",min,min->index,max,max->index,bb,bb->index);

            mtcs_cfg_cached_make_edge/*!cached_make_edge*/(mtcsCfg,edge_cache, bb, EXIT_BLOCK_PTR_FOR_FN (cfun), 0);
         /* Recognize asm goto and do the right thing.  */
         }else if ((tmp = extract_asm_operands (PATTERN (insn))) != NULL){
            int i, n = ASM_OPERANDS_LABEL_LENGTH (tmp);
            n_debug("mtcscfgbuild.c make_edges 100 min:%p %d max:%p %d bb:%p %d\n",min,min->index,max,max->index,bb,bb->index);

            for (i = 0; i < n; ++i)
               make_label_edge(self,edge_cache, bb, XEXP (ASM_OPERANDS_LABEL (tmp, i), 0), 0);
         }
         /* Otherwise, we have a plain conditional or unconditional jump.  */
         else{
            n_debug("mtcscfgbuild.c make_edges 101 min:%p %d max:%p %d bb:%p %d BB_END (bb):%p uid:%d label: %p uid:%d\n",
                  min,min->index,max,max->index,bb,bb->index,insn,INSN_UID(insn),JUMP_LABEL (insn),INSN_UID (JUMP_LABEL (insn)));
            mtcs_print_rtl_single(stderr,insn);
            mtcs_print_rtl_single(stderr,JUMP_LABEL (insn));

            gcc_assert (JUMP_LABEL (insn));
            make_label_edge(self,edge_cache, bb, JUMP_LABEL (insn), 0);
         }
      }
      n_debug("mtcscfgbuild.c make_edges 102 min:%p %d max:%p %d\n",min,min->index,max,max->index);
      test_rtl_verify_edges();
      /* If this is a sibling call insn, then this is in effect a combined call
      and return, and so we need an edge to the exit block.  No need to
      worry about EH edges, since we wouldn't have created the sibling call
      in the first place.  */
      if (code == CALL_INSN && SIBLING_CALL_P (insn))
         mtcs_cfg_cached_make_edge/*!cached_make_edge*/(mtcsCfg,
               edge_cache, bb, EXIT_BLOCK_PTR_FOR_FN (cfun),EDGE_SIBCALL | EDGE_ABNORMAL);
      /* If this is a CALL_INSN, then mark it as reaching the active EH
      handler for this CALL_INSN.  If we're handling non-call
      exceptions then any insn can reach any of the active handlers.
      Also mark the CALL_INSN as reaching any nonlocal goto handler.  */
      else if (code == CALL_INSN || cfun->can_throw_non_call_exceptions){
         /* Add any appropriate EH edges.  */
         mtcs_cfg_build_rtl_make_eh_edge/*!rtl_make_eh_edge*/(self,edge_cache, bb, insn);

         if (code == CALL_INSN){
            if (mtcs_except_can_nonlocal_goto/*!can_nonlocal_goto*/(mtcsExcept,insn)){
               /* ??? This could be made smarter: in some cases it's
               possible to tell that certain calls will not do a
               nonlocal goto.  For example, if the nested functions
               that do the nonlocal gotos do not have their addresses
               taken, then only calls to those functions or to other
               nested functions that use them could possibly do
               nonlocal gotos.  */
               for (rtx_insn_list *x = mtcsRtlData/*!nonlocal_goto_handler_labels*/->x_nonlocal_goto_handler_labels; x; x = x->next ())
                  make_label_edge(self,edge_cache, bb, x->insn (),  EDGE_ABNORMAL | EDGE_ABNORMAL_CALL);
            }

            if (mtcsOptionsItem->x_flag_tm) {
               rtx note;
               for (note = REG_NOTES (insn); note; note = XEXP (note, 1))
                  if (REG_NOTE_KIND (note) == REG_TM)
                     make_label_edge(self,edge_cache, bb, XEXP (note, 0), EDGE_ABNORMAL | EDGE_ABNORMAL_CALL);
            }
         }
      }
      n_debug("mtcscfgbuild.c make_edges 103 min:%p %d max:%p %d\n",min,min->index,max,max->index);
      test_rtl_verify_edges();
      /* Find out if we can drop through to the next block.  */
      insn = NEXT_INSN (insn);
      e = find_edge (bb, EXIT_BLOCK_PTR_FOR_FN (cfun));
      if (e && e->flags & EDGE_FALLTHRU)
         insn = NULL;

      while (insn  && NOTE_P (insn) && NOTE_KIND (insn) != NOTE_INSN_BASIC_BLOCK)
         insn = NEXT_INSN (insn);

      if (!insn)
         mtcs_cfg_cached_make_edge/*!cached_make_edge*/(mtcsCfg,edge_cache, bb, EXIT_BLOCK_PTR_FOR_FN (cfun),EDGE_FALLTHRU);
      else if (bb->next_bb != EXIT_BLOCK_PTR_FOR_FN (cfun)){
         if (insn == BB_HEAD (bb->next_bb))
            mtcs_cfg_cached_make_edge/*!cached_make_edge*/(mtcsCfg,edge_cache, bb, bb->next_bb, EDGE_FALLTHRU);
      }
   }
   n_debug("mtcscfgbuild.c make_edges 104 min:%p %d max:%p %d\n",min,min->index,max,max->index);
   test_rtl_verify_edges();
   if (edge_cache)
      sbitmap_free (edge_cache);
}

//原型 mark_tablejump_edge cfgbuild.cc
static void mark_tablejump_edge (rtx label)
{
   basic_block bb;

   gcc_assert (LABEL_P (label));
   /* See comment in make_label_edge.  */
   if (INSN_UID (label) == 0)
      return;
   bb = BLOCK_FOR_INSN (label);
   SET_STATE (bb, FULL_STATE (bb) | BLOCK_USED_BY_TABLEJUMP);
}

//原型 purge_dead_tablejump_edges cfgbuild.cc
static void purge_dead_tablejump_edges (MtcsCfgBuild *self,basic_block bb, rtx_jump_table_data *table)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);

   rtx_insn *insn = BB_END (bb);
   rtx tmp;
   rtvec vec;
   int j;
   edge_iterator ei;
   edge e;

   vec = table->get_labels ();

   for (j = GET_NUM_ELEM (vec) - 1; j >= 0; --j){
      n_debug("mtcscfgbuild.c purge_dead_tablejump_edges 00 bb:%p\n",bb);
      mark_tablejump_edge (XEXP (RTVEC_ELT (vec, j), 0));
   }
   /* Some targets (eg, ARM) emit a conditional jump that also
   contains the out-of-range target.  Scan for these and
   add an edge if necessary.  */
   if ((tmp = single_set (insn)) != NULL
   && SET_DEST (tmp) == pc_rtx
   && GET_CODE (SET_SRC (tmp)) == IF_THEN_ELSE
   && GET_CODE (XEXP (SET_SRC (tmp), 2)) == LABEL_REF){
      n_debug("mtcscfgbuild.c purge_dead_tablejump_edges 11 bb:%p\n",bb);
      mark_tablejump_edge (label_ref_label (XEXP (SET_SRC (tmp), 2)));
   }

   for (ei = ei_start (bb->succs); (e = ei_safe_edge (ei)); ){
      if (FULL_STATE (e->dest) & BLOCK_USED_BY_TABLEJUMP){
         n_debug("mtcscfgbuild.c purge_dead_tablejump_edges 22 bb:%p\n",bb);
         SET_STATE (e->dest, FULL_STATE (e->dest)  & ~(size_t) BLOCK_USED_BY_TABLEJUMP);
      }else if (!(e->flags & (EDGE_ABNORMAL | EDGE_EH))){
         n_debug("mtcscfgbuild.c purge_dead_tablejump_edges 33 bb:%p\n",bb);
         mtcs_cfg_context_remove_edge/*!remove_edge*/(mtcsCfgContext,e);
         continue;
      }
      ei_next (&ei);
   }
}


/* Scan basic block BB for possible BB boundaries inside the block
   and create new basic blocks in the progress.  */
//原型 find_bb_boundaries cfgbuild.cc
static void find_bb_boundaries (MtcsCfgBuild *self,basic_block bb)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsCfg  *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   basic_block orig_bb = bb;
   rtx_insn *insn = BB_HEAD (bb);
   rtx_insn *end = BB_END (bb), *x;
   rtx_jump_table_data *table;
   rtx_insn *flow_transfer_insn = NULL;
   rtx_insn *debug_insn = NULL;
   edge fallthru = NULL;
   bool skip_purge;
   bool seen_note_after_debug = false;

   n_debug("mtcscfgbuild.c find_bb_boundaries 00 bb:%p %d\n",bb,insn==end);
   if (insn == end)
      return;

   if (DEBUG_INSN_P (insn) || DEBUG_INSN_P (end)){
      n_debug("mtcscfgbuild.c find_bb_boundaries 11 DEBUG_INSN_P (insn) || DEBUG_INSN_P (end) bb:%p\n",bb);
      /* Check whether, without debug insns, the insn==end test above
      would have caused us to return immediately, and behave the
      same way even with debug insns.  If we don't do this, debug
      insns could cause us to purge dead edges at different times,
      which could in turn change the cfg and affect codegen
      decisions in subtle but undesirable ways.  */
      while (insn != end && DEBUG_INSN_P (insn))
         insn = NEXT_INSN (insn);
      rtx_insn *e = end;
      while (insn != e && DEBUG_INSN_P (e))
         e = PREV_INSN (e);
      if (insn == e){
         n_debug("mtcscfgbuild.c find_bb_boundaries 22 bb:%p\n",bb);
         /* If there are debug insns after a single insn that is a
         control flow insn in the block, we'd have left right
         away, but we should clean up the debug insns after the
         control flow insn, because they can't remain in the same
         block.  So, do the debug insn cleaning up, but then bail
         out without purging dead edges as we would if the debug
         insns hadn't been there.  */
         if (e != end && !DEBUG_INSN_P (e) && mtcs_cfg_build_control_flow_insn_p/*!control_flow_insn_p*/(self,e)){
            n_debug("mtcscfgbuild.c find_bb_boundaries 33 bb:%p\n",bb);
            skip_purge = true;
            flow_transfer_insn = e;
            goto clean_up_debug_after_control_flow;
         }
         return;
      }
   }

   if (LABEL_P (insn))
      insn = NEXT_INSN (insn);

   /* Scan insn chain and try to find new basic block boundaries.  */
   while (1){
      enum rtx_code code = GET_CODE (insn);

      if (code == DEBUG_INSN){
         if (flow_transfer_insn && !debug_insn){
            n_debug("mtcscfgbuild.c find_bb_boundaries 44 bb:%p\n",bb);
            debug_insn = insn;
            seen_note_after_debug = false;
         }
      }
      /* In case we've previously seen an insn that effects a control
      flow transfer, split the block.  */
      else if ((flow_transfer_insn || code == CODE_LABEL) && inside_basic_block_p (insn)){
         n_debug("mtcscfgbuild.c find_bb_boundaries 55 bb:%p\n",bb);
         rtx_insn *prev = PREV_INSN (insn);

         /* If the first non-debug inside_basic_block_p insn after a control
         flow transfer is not a label, split the block before the debug
         insn instead of before the non-debug insn, so that the debug
         insns are not lost.  */
         if (debug_insn && code != CODE_LABEL && code != BARRIER){
            prev = PREV_INSN (debug_insn);
            if (seen_note_after_debug){
               /* Though, if there are NOTEs intermixed with DEBUG_INSNs,
               move the NOTEs before the DEBUG_INSNs and split after
               the last NOTE.  */
               rtx_insn *first = NULL, *last = NULL;
               for (x = debug_insn; x != insn; x = NEXT_INSN (x)){
                  if (NOTE_P (x)){
                     if (first == NULL)
                        first = x;
                     last = x;
                  }else{
                     gcc_assert (DEBUG_INSN_P (x));
                     if (first){
                        mtcs_rtl_reorder_insns_nobb/*!reorder_insns_nobb*/(mtcsRTL,first, last, prev);
                        prev = last;
                        first = last = NULL;
                     }
                  }
               }
               if (first){
                  mtcs_rtl_reorder_insns_nobb/*!reorder_insns_nobb*/(mtcsRTL,first, last, prev);
                  prev = last;
               }
            }
         }
         n_debug("mtcscfgbuild.c find_bb_boundaries 55aa bb:%p\n",bb);
         test_rtl_verify_edges();
         fallthru = mtcs_cfg_context_split_block/*!split_block*/(mtcsCfgContext,bb, prev);
         n_debug("mtcscfgbuild.c find_bb_boundaries 55bb bb:%p %d\n",bb,bb->index);
           test_rtl_verify_edges();
         if (flow_transfer_insn){
            n_debug("mtcscfgbuild.c find_bb_boundaries 55yyy bb:%p %d flow_transfer_insn:%p\n",bb,bb->index,flow_transfer_insn);
            mtcs_print_rtl_single(stderr,flow_transfer_insn);
            n_debug("mtcscfgbuild.c find_bb_boundaries 55yyyeeee bb:%p %d BB_END (bb):%p\n",bb,bb->index,BB_END (bb));
            mtcs_print_rtl_single(stderr, BB_END (bb));

            BB_END (bb) = flow_transfer_insn;

            rtx_insn *next;
            /* Clean up the bb field for the insns between the blocks.  */
            for (x = NEXT_INSN (flow_transfer_insn); x != BB_HEAD (fallthru->dest); x = next){
               next = NEXT_INSN (x);
               n_debug("mtcscfgbuild.c find_bb_boundaries 55xx\b");
               mtcs_print_rtl_single(stderr,x);
               /* Debug insns should not be in between basic blocks,
               drop them on the floor.  */
               if (DEBUG_INSN_P (x)){
                  n_debug("mtcscfgbuild.c find_bb_boundaries 55ttxx\b");
                  mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,x);
               }else if (!BARRIER_P (x)){
                  n_debug("mtcscfgbuild.c find_bb_boundaries 55xxee\b");
                  set_block_for_insn (x, NULL);
               }
            }
         }
         n_debug("mtcscfgbuild.c find_bb_boundaries 55cc bb:%p %d\n",bb,bb->index);
         test_rtl_verify_edges();
         bb = fallthru->dest;
         n_debug("mtcscfgbuild.c find_bb_boundaries 55cc 新创建的 bb:%p %d\n",bb,bb->index);
         test_rtl_verify_edges();

         mtcs_cfg_context_remove_edge/*!remove_edge*/(mtcsCfgContext,fallthru);
         /* BB is unreachable at this point - we need to determine its profile
         once edges are built.  */
         bb->count = profile_count::uninitialized ();
         flow_transfer_insn = NULL;
         debug_insn = NULL;
         n_debug("mtcscfgbuild.c find_bb_boundaries 55dd bb:%p\n",bb);
         test_rtl_verify_edges();
         if (code == CODE_LABEL && LABEL_ALT_ENTRY_P (insn))
            mtcs_cfg_make_edge/*!make_edge*/(mtcsCfg,ENTRY_BLOCK_PTR_FOR_FN (cfun), bb, 0);
         n_debug("mtcscfgbuild.cfind_bb_boundaries 55ee bb:%p\n",bb);
         test_rtl_verify_edges();
      }else if (code == BARRIER){
         n_debug("mtcscfgbuild.c find_bb_boundaries 66 bb:%p %d flow_transfer_insn:%p\n",bb,bb->index,flow_transfer_insn);
         /* __builtin_unreachable () may cause a barrier to be emitted in
         the middle of a BB.  We need to split it in the same manner as
         if the barrier were preceded by a control_flow_insn_p insn.  */
         if (!flow_transfer_insn)
            flow_transfer_insn = prev_nonnote_nondebug_insn_bb (insn);
         debug_insn = NULL;
      }else if (debug_insn){
         n_debug("mtcscfgbuild.c find_bb_boundaries 77 bb:%p\n",bb);
         if (code == NOTE)
            seen_note_after_debug = true;
         else
            /* Jump tables.  */
            debug_insn = NULL;
      }

      if (mtcs_cfg_build_control_flow_insn_p/*!control_flow_insn_p*/(self,insn))
         flow_transfer_insn = insn;
      if (insn == end)
         break;
      insn = NEXT_INSN (insn);
      n_debug("mtcscfgbuild.c find_bb_boundaries 88 bb:%p\n",bb);
   }

   /* In case expander replaced normal insn by sequence terminating by
   return and barrier, or possibly other sequence not behaving like
   ordinary jump, we need to take care and move basic block boundary.  */
   if (flow_transfer_insn && flow_transfer_insn != end){
      n_debug("mtcscfgbuild.c find_bb_boundaries 99 bb:%p\n",bb);
      skip_purge = false;

clean_up_debug_after_control_flow:
      BB_END (bb) = flow_transfer_insn;

      /* Clean up the bb field for the insns that do not belong to BB.  */
      rtx_insn *next;
      for (x = NEXT_INSN (flow_transfer_insn); ; x = next){
         next = NEXT_INSN (x);
         /* Debug insns should not be in between basic blocks,
         drop them on the floor.  */
         if (DEBUG_INSN_P (x))
            mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,x);
         else if (!BARRIER_P (x))
            set_block_for_insn (x, NULL);
         if (x == end)
            break;
      }

      if (skip_purge)
         return;
   }
   n_debug("mtcscfgbuild.c find_bb_boundaries 55ff bb:%p\n",bb);
   test_rtl_verify_edges();
   /* We've possibly replaced the conditional jump by conditional jump
   followed by cleanup at fallthru edge, so the outgoing edges may
   be dead.  */
   mtcs_cfg_rtl_purge_dead_edges/*!purge_dead_edges*/(mtcsCfgRtl,bb);
   n_debug("mtcscfgbuild.c find_bb_boundaries 55gg bb:%p\n",bb);
   test_rtl_verify_edges();
   /* purge_dead_edges doesn't handle tablejump's, but if we have split the
   basic block, we might need to kill some edges.  */
   if (bb != orig_bb && tablejump_p (BB_END (bb), NULL, &table)){
      n_debug("mtcscfgbuild.c find_bb_boundaries 100 bb:%p\n",bb);
      purge_dead_tablejump_edges(self,bb, table);
   }
   n_debug("mtcscfgbuild.c find_bb_boundaries 55hh bb:%p\n",bb);
   test_rtl_verify_edges();
}

/*  Assume that frequency of basic block B is known.  Compute frequencies
    and probabilities of outgoing edges.  */
static void compute_outgoing_frequencies (basic_block b)
{
   edge e, f;
   edge_iterator ei;

   if (EDGE_COUNT (b->succs) == 2){
      rtx note = find_reg_note (BB_END (b), REG_BR_PROB, NULL);
      int probability;

      if (note){
         probability = XINT (note, 0);
         e = BRANCH_EDGE (b);
         e->probability = profile_probability::from_reg_br_prob_note (probability);
         f = FALLTHRU_EDGE (b);
         f->probability = e->probability.invert ();
         return;
      }else{
         guess_outgoing_edge_probabilities (b);
      }
   }else if (single_succ_p (b)){
      e = single_succ_edge (b);
      e->probability = profile_probability::always ();
      return;
   }else{
      /* We rely on BBs with more than two successors to have sane probabilities
      and do not guess them here. For BBs terminated by switch statements
      expanded to jump-table jump, we have done the right thing during
      expansion. For EH edges, we still guess the probabilities here.  */
      bool complex_edge = false;
      FOR_EACH_EDGE (e, ei, b->succs)
         if (e->flags & EDGE_COMPLEX){
            complex_edge = true;
            break;
         }
      if (complex_edge)
         guess_outgoing_edge_probabilities (b);
   }
}

/* Update the profile information for BB, which was created by splitting
   an RTL block that had a non-final jump.  */
static void update_profile_for_new_sub_basic_block (basic_block bb)
{
   edge e;
   edge_iterator ei;

   bool initialized_src = false, uninitialized_src = false;
   bb->count = profile_count::zero ();
   FOR_EACH_EDGE (e, ei, bb->preds){
      if (e->count ().initialized_p ()){
         bb->count += e->count ();
         initialized_src = true;
      }else
         uninitialized_src = true;
   }
   /* When some edges are missing with read profile, this is
   most likely because RTL expansion introduced loop.
   When profile is guessed we may have BB that is reachable
   from unlikely path as well as from normal path.

   TODO: We should handle loops created during BB expansion
   correctly here.  For now we assume all those loop to cycle
   precisely once.  */
   if (!initialized_src  || (uninitialized_src  && profile_status_for_fn (cfun) < PROFILE_GUESSED))
      bb->count = profile_count::uninitialized ();

   compute_outgoing_frequencies (bb);
}


/* Assume that some pass has inserted labels or control flow
   instructions within a basic block.  Split basic blocks as needed
   and create edges.  */
//原型 find_many_sub_basic_blocks cfgbuild.h  cfgbuild.cc
void mtcs_cfg_build_find_many_sub_basic_blocks (MtcsCfgBuild *self,sbitmap blocks)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);

   basic_block bb, min, max;
   bool found = false;
   auto_vec<unsigned int> n_succs;
   n_succs.safe_grow_cleared (last_basic_block_for_fn (cfun), true);

   FOR_EACH_BB_FN (bb, cfun)
      SET_STATE (bb, bitmap_bit_p (blocks, bb->index) ? BLOCK_TO_SPLIT : BLOCK_ORIGINAL);

   FOR_EACH_BB_FN (bb, cfun)
      if (STATE (bb) == BLOCK_TO_SPLIT) {
         int n = last_basic_block_for_fn (cfun);
         unsigned int ns = EDGE_COUNT (bb->succs);
         n_debug("mtcscfgbuild.c find_many_sub_basic_blocks state BLOCK_TO_SPLIT: bb:%p\n",bb);
         find_bb_boundaries(self,bb);
         if (n == last_basic_block_for_fn (cfun) && ns == EDGE_COUNT (bb->succs))
            n_succs[bb->index] = EDGE_COUNT (bb->succs);
      }

   FOR_EACH_BB_FN (bb, cfun)
      if (STATE (bb) != BLOCK_ORIGINAL){
         found = true;
         break;
      }

   if (!found)
      return;
   n_debug("mtcscfgbuild.c find_many_sub_basic_blocks 00 bb:%p %d\n",bb,bb->index);
   test_rtl_verify_edges();

   min = max = bb;
   for (; bb != EXIT_BLOCK_PTR_FOR_FN (cfun); bb = bb->next_bb)
      if (STATE (bb) != BLOCK_ORIGINAL)
         max = bb;

   /* Now re-scan and wire in all edges.  This expect simple (conditional)
   jumps at the end of each new basic blocks.  */
   make_edges(self,min, max, 1);
   n_debug("mtcscfgbuild.c find_many_sub_basic_blocks 11 min:%p :%d max:%p :%d\n",min,min->index,max,max->index);
   test_rtl_verify_edges();
   /* Update branch probabilities.  Expect only (un)conditional jumps
   to be created with only the forward edges.  */
   if (profile_status_for_fn (cfun) != PROFILE_ABSENT)
      FOR_BB_BETWEEN (bb, min, max->next_bb, next_bb){
         if (STATE (bb) == BLOCK_ORIGINAL)
            continue;
         if (STATE (bb) == BLOCK_NEW){
            update_profile_for_new_sub_basic_block (bb);
            continue;
         }
         /* If nothing changed, there is no need to create new BBs.  */
         if (EDGE_COUNT (bb->succs) == n_succs[bb->index]){
            /* In rare occassions RTL expansion might have mistakely assigned
            a probabilities different from what is in CFG.  This happens
            when we try to split branch to two but optimize out the
            second branch during the way. See PR81030.  */
            if (JUMP_P (BB_END (bb)) && any_condjump_p (BB_END (bb)) && EDGE_COUNT (bb->succs) >= 2)
               mtcs_cfg_rtl_update_br_prob_note/*!update_br_prob_note*/(mtcsCfgRtl,bb);
            continue;
         }
         compute_outgoing_frequencies (bb);
   }
   n_debug("mtcscfgbuild.c find_many_sub_basic_blocks 22 \n");
   test_rtl_verify_edges();
   FOR_EACH_BB_FN (bb, cfun)
      SET_STATE (bb, 0);
}

/* Create the edges generated by INSN in REGION.  */
//原型 rtl_make_eh_edge cfgbuild.h cfgbuild.cc
void mtcs_cfg_build_rtl_make_eh_edge (MtcsCfgBuild *self,sbitmap edge_cache, basic_block src, rtx insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsStmt *mtcsStmt=mtcs_target_get_stmt(mtcsTarget);

   eh_landing_pad lp = get_eh_landing_pad_from_rtx (insn);

   if (lp){
      rtx_insn *label = lp->landing_pad;
      /* During initial rtl generation, use the post_landing_pad.  */
      if (label == NULL){
         gcc_assert (lp->post_landing_pad);
         label = mtcs_stmt_label_rtx/*!label_rtx*/(mtcsStmt,lp->post_landing_pad);
      }
      make_label_edge(self,edge_cache, src, label, EDGE_ABNORMAL | EDGE_EH | (CALL_P (insn) ? EDGE_ABNORMAL_CALL : 0));
   }
}


/* Like find_many_sub_basic_blocks, but look only within BB.  */
//原型 find_sub_basic_blocks cfgbuild.h cfgbuild.cc
void mtcs_cfg_build_find_sub_basic_blocks (MtcsCfgBuild *self,basic_block bb)
{
   basic_block end_bb = bb->next_bb;
   find_bb_boundaries(self,bb);
   if (bb->next_bb == end_bb)
      return;

   /* Re-scan and wire in all edges.  This expects simple (conditional)
   jumps at the end of each new basic blocks.  */
   make_edges(self,bb, end_bb->prev_bb, 1);

   /* Update branch probabilities.  Expect only (un)conditional jumps
   to be created with only the forward edges.  */
   if (profile_status_for_fn (cfun) != PROFILE_ABSENT){
      compute_outgoing_frequencies (bb);
      for (bb = bb->next_bb; bb != end_bb; bb = bb->next_bb)
         update_profile_for_new_sub_basic_block (bb);
   }
}


/* Return true if INSN may cause control flow transfer, so it should be last in
   the basic block.  */
//原型 control_flow_insn_p cfgbuild.h cfgbuild.cc
bool mtcs_cfg_build_control_flow_insn_p (MtcsCfgBuild *self,const rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExcept *mtcsExcept=mtcs_target_get_except(mtcsTarget);

   switch (GET_CODE (insn)){
      case NOTE:
      case CODE_LABEL:
      case DEBUG_INSN:
         return false;

      case JUMP_INSN:
         return true;

      case CALL_INSN:
         /* Noreturn and sibling call instructions terminate the basic blocks
         (but only if they happen unconditionally).  */
         if ((SIBLING_CALL_P (insn) || find_reg_note (insn, REG_NORETURN, 0))
         && GET_CODE (PATTERN (insn)) != COND_EXEC)
            return true;

         /* Call insn may return to the nonlocal goto handler.  */
         if (mtcs_except_can_nonlocal_goto/*!can_nonlocal_goto*/(mtcsExcept,insn))
            return true;
         break;

      case INSN:
         /* Treat trap instructions like noreturn calls (same provision).  */
         if (GET_CODE (PATTERN (insn)) == TRAP_IF  && XEXP (PATTERN (insn), 0) == const1_rtx)
            return true;
         if (!cfun->can_throw_non_call_exceptions)
            return false;
         break;

      case JUMP_TABLE_DATA:
      case BARRIER:
         /* It is nonsense to reach this when looking for the
         end of basic block, but before dead code is eliminated
         this may happen.  */
         return false;

      default:
         gcc_unreachable ();
   }

   return can_throw_internal (insn);
}



static void mtcsCfgBuildInit(MtcsCfgBuild *self)
{

}


MtcsCfgBuild *mtcs_cfg_build_new(MtcsMode *mtcsMode)
{
      MtcsCfgBuild *self = n_slice_alloc0 (sizeof(MtcsCfgBuild));
      mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
      mtcsCfgBuildInit(self);
      return self;
}
