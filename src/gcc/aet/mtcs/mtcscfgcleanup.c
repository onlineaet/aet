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
 * base on cfgcleanup.cc
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
#include "aet/aetprintgimple.h"
#include "mtcscfgcleanup.h"
#include "mtcstarget.h"
#include "mtcscompile.h"
#include "mtcsdfcore.h"
#include "mtcsdfproblems.h"
#include "mtcsprintrtl.h"

#define FORWARDER_BLOCK_P(BB) ((BB)->flags & BB_FORWARDER_BLOCK)



static bool try_crossjump_bb (int, basic_block);
static enum replace_direction old_insns_match_p (MtcsCfgCleanup *self,int, rtx_insn *, rtx_insn *);

static void merge_blocks_move_predecessor_nojumps (MtcsCfgCleanup *self,basic_block, basic_block);
static void merge_blocks_move_successor_nojumps (MtcsCfgCleanup *self,basic_block, basic_block);
static bool try_optimize_cfg (int);
static bool try_forward_edges (int, basic_block);
static edge thread_jump (MtcsCfgCleanup *self,edge, basic_block);
static bool mark_effect (MtcsCfgCleanup *self,rtx, bitmap);
static void notice_new_block (MtcsCfgCleanup *self,basic_block);
static void update_forwarder_flag (MtcsCfgCleanup *self,basic_block);

/* Set flags for newly created block.  */

static void notice_new_block (MtcsCfgCleanup *self,basic_block bb)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);

   if (!bb)
      return;
   if (mtcs_cfg_rtl_forwarder_block_p/*!forwarder_block_p*/(mtcsCfgRtl,bb))
      bb->flags |= BB_FORWARDER_BLOCK;
}

/* Recompute forwarder flag after block has been modified.  */

static void update_forwarder_flag (MtcsCfgCleanup *self,basic_block bb)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);

   if (mtcs_cfg_rtl_forwarder_block_p/*!forwarder_block_p*/(mtcsCfgRtl,bb))
      bb->flags |= BB_FORWARDER_BLOCK;
   else
      bb->flags &= ~BB_FORWARDER_BLOCK;
}

/* Simplify a conditional jump around an unconditional jump.
   Return true if something changed.  */

static bool try_simplify_condjump (MtcsCfgCleanup *self,basic_block cbranch_block)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);

  basic_block jump_block, jump_dest_block, cbranch_dest_block;
  edge cbranch_jump_edge, cbranch_fallthru_edge;
  rtx_insn *cbranch_insn;

  /* Verify that there are exactly two successors.  */
  if (EDGE_COUNT (cbranch_block->succs) != 2)
    return false;

  /* Verify that we've got a normal conditional branch at the end
     of the block.  */
  cbranch_insn = BB_END (cbranch_block);
  if (!any_condjump_p (cbranch_insn))
    return false;

  cbranch_fallthru_edge = FALLTHRU_EDGE (cbranch_block);
  cbranch_jump_edge = BRANCH_EDGE (cbranch_block);

  /* The next block must not have multiple predecessors, must not
     be the last block in the function, and must contain just the
     unconditional jump.  */
  jump_block = cbranch_fallthru_edge->dest;
  if (!single_pred_p (jump_block)
      || jump_block->next_bb == EXIT_BLOCK_PTR_FOR_FN (cfun)
      || !FORWARDER_BLOCK_P (jump_block))
    return false;
  jump_dest_block = single_succ (jump_block);

  /* If we are partitioning hot/cold basic blocks, we don't want to
     mess up unconditional or indirect jumps that cross between hot
     and cold sections.

     Basic block partitioning may result in some jumps that appear to
     be optimizable (or blocks that appear to be mergeable), but which really
     must be left untouched (they are required to make it safely across
     partition boundaries).  See the comments at the top of
     bb-reorder.cc:partition_hot_cold_basic_blocks for complete details.  */

  if (BB_PARTITION (jump_block) != BB_PARTITION (jump_dest_block)
      || (cbranch_jump_edge->flags & EDGE_CROSSING))
    return false;

  /* The conditional branch must target the block after the
     unconditional branch.  */
  cbranch_dest_block = cbranch_jump_edge->dest;

  if (cbranch_dest_block == EXIT_BLOCK_PTR_FOR_FN (cfun)
      || jump_dest_block == EXIT_BLOCK_PTR_FOR_FN (cfun)
      || !can_fallthru (jump_block, cbranch_dest_block))
    return false;

  /* Invert the conditional branch.  */
  if (!mtcs_dojump_invert_jump/*!invert_jump*/(mtcsDojump,as_a <rtx_jump_insn *> (cbranch_insn),
        mtcs_cfg_rtl_block_label/*!block_label*/(mtcsCfgRtl,jump_dest_block), 0))
    return false;

  if (dump_file)
    fprintf (dump_file, "Simplifying condjump %i around jump %i\n",
        INSN_UID (cbranch_insn), INSN_UID (BB_END (jump_block)));

  /* Success.  Update the CFG to match.  Note that after this point
     the edge variable names appear backwards; the redirection is done
     this way to preserve edge profile data.  */
  cbranch_jump_edge = mtcs_cfg_context_redirect_edge_succ_nodup/*!redirect_edge_succ_nodup*/(mtcsCfgContext,
        cbranch_jump_edge,cbranch_dest_block);
  cbranch_fallthru_edge =  mtcs_cfg_context_redirect_edge_succ_nodup/*!redirect_edge_succ_nodup*/(mtcsCfgContext,
        cbranch_fallthru_edge,jump_dest_block);
  cbranch_jump_edge->flags |= EDGE_FALLTHRU;
  cbranch_fallthru_edge->flags &= ~EDGE_FALLTHRU;
  mtcs_cfg_rtl_update_br_prob_note/*!update_br_prob_note*/(mtcsCfgRtl,cbranch_block);

  /* Delete the block with the unconditional jump, and clean up the mess.  */
  mtcs_cfg_context_delete_basic_block/*!delete_basic_block*/(mtcsCfgContext,jump_block);
  mtcs_cfg_context_tidy_fallthru_edge/*!tidy_fallthru_edge*/(mtcsCfgContext,cbranch_jump_edge);
  update_forwarder_flag(self,cbranch_block);

  return true;
}

/* Attempt to prove that operation is NOOP using CSElib or mark the effect
   on register.  Used by jump threading.  */

static bool mark_effect (MtcsCfgCleanup *self,rtx exp, regset nonequal)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCseLib *mtcsCseLib=mtcs_target_get_cse_lib(mtcsTarget);

   rtx dest;
   switch (GET_CODE (exp)){
      /* In case we do clobber the register, mark it as equal, as we know the
      value is dead so it don't have to match.  */
      case CLOBBER:
      dest = XEXP (exp, 0);
      if (REG_P (dest))
         bitmap_clear_range (nonequal, REGNO (dest), REG_NREGS (dest));
      return false;

      case SET:
         if (mtcs_cse_lib_cselib_redundant_set_p/*!cselib_redundant_set_p*/(mtcsCseLib,exp))
            return false;
         dest = SET_DEST (exp);
         if (dest == pc_rtx)
            return false;
         if (!REG_P (dest))
            return true;
         bitmap_set_range (nonequal, REGNO (dest), REG_NREGS (dest));
         return false;

      default:
         return false;
   }
}

/* Return true if X contains a register in NONEQUAL.  */
static bool mentions_nonequal_regs (const_rtx x, regset nonequal)
{
   subrtx_iterator::array_type array;
   FOR_EACH_SUBRTX (iter, array, x, NONCONST){
      const_rtx x = *iter;
      if (REG_P (x)){
         unsigned int end_regno = END_REGNO (x);
         for (unsigned int regno = REGNO (x); regno < end_regno; ++regno)
            if (REGNO_REG_SET_P (nonequal, regno))
               return true;
      }
   }
   return false;
}

/* Attempt to prove that the basic block B will have no side effects and
   always continues in the same edge if reached via E.  Return the edge
   if exist, NULL otherwise.  */

static edge thread_jump (MtcsCfgCleanup *self,edge e, basic_block b)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCseLib *mtcsCseLib=mtcs_target_get_cse_lib(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);

   rtx set1, set2, cond1, cond2;
   rtx_insn *insn;
   enum rtx_code code1, code2, reversed_code2;
   bool reverse1 = false;
   unsigned i;
   regset nonequal;
   bool failed = false;

   /* Jump threading may cause fixup_partitions to introduce new crossing edges,
   which is not allowed after reload.  */
   gcc_checking_assert (!reload_completed || !mtcsRtlData/*!crtl*/->has_bb_partition);

   if (b->flags & BB_NONTHREADABLE_BLOCK)
      return NULL;

   /* At the moment, we do handle only conditional jumps, but later we may
   want to extend this code to tablejumps and others.  */
   if (EDGE_COUNT (e->src->succs) != 2)
      return NULL;
   if (EDGE_COUNT (b->succs) != 2){
      b->flags |= BB_NONTHREADABLE_BLOCK;
      return NULL;
   }

   /* Second branch must end with onlyjump, as we will eliminate the jump.  */
   if (!any_condjump_p (BB_END (e->src)))
      return NULL;

   if (!any_condjump_p (BB_END (b)) || !onlyjump_p (BB_END (b))){
      b->flags |= BB_NONTHREADABLE_BLOCK;
      return NULL;
   }

   set1 = pc_set (BB_END (e->src));
   set2 = pc_set (BB_END (b));
   if (((e->flags & EDGE_FALLTHRU) != 0) != (XEXP (SET_SRC (set1), 1) == pc_rtx))
      reverse1 = true;

   cond1 = XEXP (SET_SRC (set1), 0);
   cond2 = XEXP (SET_SRC (set2), 0);
   if (reverse1)
      code1 = mtcs_dojump_reversed_comparison_code/*!reversed_comparison_code*/(mtcsDojump,cond1, BB_END (e->src));
   else
      code1 = GET_CODE (cond1);

   code2 = GET_CODE (cond2);
   reversed_code2 = mtcs_dojump_reversed_comparison_code/*!reversed_comparison_code*/(mtcsDojump,cond2, BB_END (b));

   if (!comparison_dominates_p (code1, code2)  && !comparison_dominates_p (code1, reversed_code2))
      return NULL;

   /* Ensure that the comparison operators are equivalent.
   ??? This is far too pessimistic.  We should allow swapped operands,
   different CCmodes, or for example comparisons for interval, that
   dominate even when operands are not equivalent.  */
   if (!rtx_equal_p (XEXP (cond1, 0), XEXP (cond2, 0))  || !rtx_equal_p (XEXP (cond1, 1), XEXP (cond2, 1)))
      return NULL;

   /* Punt if BB_END (e->src) is doloop-like conditional jump that modifies
   the registers used in cond1.  */
   if (modified_in_p (cond1, BB_END (e->src)))
      return NULL;

   /* Short circuit cases where block B contains some side effects, as we can't
   safely bypass it.  */
   for (insn = NEXT_INSN (BB_HEAD (b)); insn != NEXT_INSN (BB_END (b));
      insn = NEXT_INSN (insn))
      if (INSN_P (insn) && side_effects_p (PATTERN (insn))){
         b->flags |= BB_NONTHREADABLE_BLOCK;
      return NULL;
   }

   mtcs_cse_lib_cselib_init/*!cselib_init*/(mtcsCseLib,0);

   /* First process all values computed in the source basic block.  */
   for (insn = NEXT_INSN (BB_HEAD (e->src)); insn != NEXT_INSN (BB_END (e->src)); insn = NEXT_INSN (insn))
      if (INSN_P (insn))
         mtcs_cse_lib_cselib_process_insn/*!cselib_process_insn*/(mtcsCseLib,insn);

   nonequal = BITMAP_ALLOC (NULL);
   CLEAR_REG_SET (nonequal);

   /* Now assume that we've continued by the edge E to B and continue
   processing as if it were same basic block.
   Our goal is to prove that whole block is an NOOP.  */

   for (insn = NEXT_INSN (BB_HEAD (b)); insn != NEXT_INSN (BB_END (b)) && !failed; insn = NEXT_INSN (insn)){
      /* cond2 must not mention any register that is not equal to the
      former block.  Check this before processing that instruction,
      as BB_END (b) could contain also clobbers.  */
      if (insn == BB_END (b) && mentions_nonequal_regs (cond2, nonequal))
         goto failed_exit;

      if (INSN_P (insn)){
         rtx pat = PATTERN (insn);

      if (GET_CODE (pat) == PARALLEL){
         for (i = 0; i < (unsigned)XVECLEN (pat, 0); i++)
            failed |= mark_effect(self,XVECEXP (pat, 0, i), nonequal);
      }else
         failed |= mark_effect(self,pat, nonequal);
      }

      mtcs_cse_lib_cselib_process_insn/*!cselib_process_insn*/(mtcsCseLib,insn);
   }

   /* Later we should clear nonequal of dead registers.  So far we don't
   have life information in cfg_cleanup.  */
   if (failed){
      b->flags |= BB_NONTHREADABLE_BLOCK;
      goto failed_exit;
   }

   if (!REG_SET_EMPTY_P (nonequal))
      goto failed_exit;

   BITMAP_FREE (nonequal);
   mtcs_cse_lib_cselib_finish/*!cselib_finish*/(mtcsCseLib);
   if ((comparison_dominates_p (code1, code2) != 0) != (XEXP (SET_SRC (set2), 1) == pc_rtx))
      return BRANCH_EDGE (b);
   else
      return FALLTHRU_EDGE (b);

failed_exit:
   BITMAP_FREE (nonequal);
   mtcs_cse_lib_cselib_finish/*!cselib_finish*/(mtcsCseLib);
   return NULL;
}

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
         n_debug("mtcscfgcleanup.c rtl_verify_edges 00 bb:%p index:%d n_branch:%d BB_END (bb):%p any_uncondjump_p:%d\n",
               bb,bb->index,n_branch,BB_END (bb),re);
      }
  }
}

static void printbb(basic_block block)
{
   if(!n_log_is_debug())
      return;
//   basic_block initblock;
//   FOR_EACH_BB_FN (initblock, cfun){
//     break;
//   }
//   if(block!=initblock)
//      return;
//   rtx_insn *insn;
//   int i=0;
//   FOR_BB_INSNS (block, insn){
//      if (!INSN_P (insn))
//         continue;
//      fprintf(stderr,"mtcscfgcleanup.c 打印块中的指令 i:%d block:%p index:%d flags:%d insn:%p\n",i++,block,block->index,block->flags,insn);
//      mtcs_print_rtl_single(stderr,insn);
//   }
/*
   int countbb = 0;
   basic_block bb;
    FOR_EACH_BB_REVERSE_FN (bb, cfun){
       fprintf(stderr,"mtcscfgcleanup bb info cfun:%p i:%d bb:%p bb->index:%d \n",cfun,countbb++,bb,bb->index);
    }
    */
    test_rtl_verify_edges();
}

/* Attempt to forward edges leaving basic block B.
   Return true if successful.  */
static bool try_forward_edges (MtcsCfgCleanup *self,int mode, basic_block b)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   bool changed = false;
   edge_iterator ei;
   edge e, *threaded_edges = NULL;

   for (ei = ei_start (b->succs); (e = ei_safe_edge (ei)); ){
      basic_block target, first;
      location_t goto_locus;
      int counter;
      bool threaded = false;
      int nthreaded_edges = 0;
      bool may_thread = self->first_pass || (b->flags & BB_MODIFIED) != 0;
      bool new_target_threaded = false;
      n_debug("mtcscfgcleanup.c try_forward_edges 00 b:%p\n",b);

      /* Skip complex edges because we don't know how to update them.

      Still handle fallthru edges, as we can succeed to forward fallthru
      edge to the same place as the branch edge of conditional branch
      and turn conditional branch to an unconditional branch.  */
      if (e->flags & EDGE_COMPLEX){
         ei_next (&ei);
         continue;
      }

      target = first = e->dest;
      counter = NUM_FIXED_BLOCKS;
      goto_locus = e->goto_locus;

      while (counter < n_basic_blocks_for_fn (cfun)){
         basic_block new_target = NULL;
         may_thread |= (target->flags & BB_MODIFIED) != 0;
         n_debug("mtcscfgcleanup.c try_forward_edges 11 may_thread:%d %d\n",may_thread,FORWARDER_BLOCK_P (target));
         printbb(target);
         printbb(b);

         if (FORWARDER_BLOCK_P (target) && single_succ (target) != EXIT_BLOCK_PTR_FOR_FN (cfun)){
            /* Bypass trivial infinite loops.  */
            new_target = single_succ (target);
            n_debug("mtcscfgcleanup.c try_forward_edges 22 may_thread:%d new_target:%p target:%p\n",may_thread,new_target,target);

            if (target == new_target)
               counter = n_basic_blocks_for_fn (cfun);
            else if (!mtcsOptionsItem->x_optimize){
               /* When not optimizing, ensure that edges or forwarder
               blocks with different locus are not optimized out.  */
               location_t new_locus = single_succ_edge (target)->goto_locus;
               location_t locus = goto_locus;

               if (LOCATION_LOCUS (new_locus) != UNKNOWN_LOCATION
               && LOCATION_LOCUS (locus) != UNKNOWN_LOCATION
               && new_locus != locus){
                  n_debug("mtcscfgcleanup.c try_forward_edges 33 设 new_target为空\n");
                  new_target = NULL;
               }else{
                  if (LOCATION_LOCUS (new_locus) != UNKNOWN_LOCATION)
                     locus = new_locus;

                  rtx_insn *last = BB_END (target);
                  if (DEBUG_INSN_P (last))
                     last = prev_nondebug_insn (last);
                  if (last && INSN_P (last))
                     new_locus = INSN_LOCATION (last);
                  else
                     new_locus = UNKNOWN_LOCATION;

                  if (LOCATION_LOCUS (new_locus) != UNKNOWN_LOCATION
                  && LOCATION_LOCUS (locus) != UNKNOWN_LOCATION
                  && new_locus != locus){
                    n_debug("mtcscfgcleanup.c try_forward_edges 44 设 new_target为空\n");
                     new_target = NULL;
                  }else{
                     if (LOCATION_LOCUS (new_locus) != UNKNOWN_LOCATION)
                        locus = new_locus;
                     goto_locus = locus;
                  }
               }
            }
         }

         /* Allow to thread only over one edge at time to simplify updating
         of probabilities.  */
         else if ((mode & CLEANUP_THREADING) && may_thread){
            n_debug("mtcscfgcleanup.c try_forward_edges 33xx \n");
            printbb(b);
            edge t = thread_jump(self,e, target);
            if (t){
               if (!threaded_edges)
                  threaded_edges = XNEWVEC (edge, n_basic_blocks_for_fn (cfun));
               else{
                  int i;

                  /* Detect an infinite loop across blocks not
                  including the start block.  */
                  for (i = 0; i < nthreaded_edges; ++i)
                     if (threaded_edges[i] == t)
                        break;
                  if (i < nthreaded_edges){
                     counter = n_basic_blocks_for_fn (cfun);
                     break;
                  }
               }

               /* Detect an infinite loop across the start block.  */
               if (t->dest == b)
                  break;

               gcc_assert (nthreaded_edges < (n_basic_blocks_for_fn (cfun)- NUM_FIXED_BLOCKS));
               threaded_edges[nthreaded_edges++] = t;

               new_target = t->dest;
               new_target_threaded = true;
               n_debug("mtcscfgcleanup.c try_forward_edges 55 设 new_target= t->dest :%p\n",new_target);
               printbb(b);

            }
         }

         if (!new_target)
            break;

         counter++;
         /* Do not turn non-crossing jump to crossing.  Depending on target
         it may require different instruction pattern.  */
         if ((e->flags & EDGE_CROSSING) || BB_PARTITION (first) == BB_PARTITION (new_target)){
            //n_debug("mtcscfgcleanup.c try_forward_edges 66 first:%p target:%p,new_target:%p\n",first,target,new_target);

            target = new_target;
            threaded |= new_target_threaded;
         }
      }//end while
      n_debug("mtcscfgcleanup.c try_forward_edges 77 %d %d\n",counter >= n_basic_blocks_for_fn (cfun),target == first);

      if (counter >= n_basic_blocks_for_fn (cfun)){
         if (dump_file)
            fprintf (dump_file, "Infinite loop in BB %i.\n",target->index);
      }else if (target == first)
         ; /* We didn't do anything.  */
      else{
         /* Save the values now, as the edge may get removed.  */
         profile_count edge_count = e->count ();
         int n = 0;

         e->goto_locus = goto_locus;

         /* Don't force if target is exit block.  */
         if (threaded && target != EXIT_BLOCK_PTR_FOR_FN (cfun)){
            n_debug("mtcscfgcleanup.c try_forward_edges 88\n");
            notice_new_block(self,mtcs_cfg_context_redirect_edge_and_branch_force/*!redirect_edge_and_branch_force*/
                  (mtcsCfgContext,e, target));
            if (dump_file)
               fprintf (dump_file, "Conditionals threaded.\n");
         }else if (!mtcs_cfg_context_redirect_edge_and_branch/*!redirect_edge_and_branch*/(mtcsCfgContext,e, target)){
            n_debug("mtcscfgcleanup.c try_forward_edges 88xcc\n");
            printbb(b);

            if (dump_file)
               fprintf (dump_file,"Forwarding edge %i->%i to %i failed.\n",b->index, e->dest->index, target->index);
            ei_next (&ei);
            continue;
         }
         n_debug("mtcscfgcleanup.c try_forward_edges 88xcceeee\n");
         /* We successfully forwarded the edge.  Now update profile
         data: for each edge we traversed in the chain, remove
         the original edge's execution count.  */
         do{
            edge t;

            if (!single_succ_p (first)){
               gcc_assert (n < nthreaded_edges);
               t = threaded_edges [n++];
               gcc_assert (t->src == first);
               n_debug("mtcscfgcleanup.c try_forward_edges 99xx\n");

               update_bb_profile_for_threading (first, edge_count, t);
               mtcs_cfg_rtl_update_br_prob_note/*!update_br_prob_note*/(mtcsCfgRtl,first);
            }else{
               first->count -= edge_count;
               /* It is possible that as the result of
               threading we've removed edge as it is
               threaded to the fallthru edge.  Avoid
               getting out of sync.  */
               if (n < nthreaded_edges && first == threaded_edges [n]->src)
                  n++;
               t = single_succ_edge (first);
            }

            first = t->dest;
         }while (first != target);

         changed = true;
         continue;
      }
      ei_next (&ei);
   }

   free (threaded_edges);
   return changed;
}


/* Blocks A and B are to be merged into a single block.  A has no incoming
   fallthru edge, so it can be moved before B without adding or modifying
   any jumps (aside from the jump from A to B).  */

static void merge_blocks_move_predecessor_nojumps (MtcsCfgCleanup *self,basic_block a, basic_block b)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);

  rtx_insn *barrier;
  /* If we are partitioning hot/cold basic blocks, we don't want to
     mess up unconditional or indirect jumps that cross between hot
     and cold sections.
     Basic block partitioning may result in some jumps that appear to
     be optimizable (or blocks that appear to be mergeable), but which really
     must be left untouched (they are required to make it safely across
     partition boundaries).  See the comments at the top of
     bb-reorder.cc:partition_hot_cold_basic_blocks for complete details.  */

  if (BB_PARTITION (a) != BB_PARTITION (b))
    return;

  barrier = next_nonnote_insn (BB_END (a));
  gcc_assert (BARRIER_P (barrier));
  mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,barrier);

  /* Scramble the insn chain.  */
  if (BB_END (a) != PREV_INSN (BB_HEAD (b)))
     mtcs_rtl_reorder_insns_nobb/*!reorder_insns_nobb*/(mtcsRTL,BB_HEAD (a), BB_END (a), PREV_INSN (BB_HEAD (b)));
  mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,a);

  if (dump_file)
    fprintf (dump_file, "Moved block %d before %d and merged.\n",a->index, b->index);
  /* Swap the records for the two blocks around.  */
  unlink_block (a);
  link_block (a, b->prev_bb);
  /* Now blocks A and B are contiguous.  Merge them.  */
  mtcs_cfg_context_merge_blocks/*!merge_blocks*/(mtcsCfgContext,a, b);
}

/* Blocks A and B are to be merged into a single block.  B has no outgoing
   fallthru edge, so it can be moved after A without adding or modifying
   any jumps (aside from the jump from A to B).  */

static void merge_blocks_move_successor_nojumps (MtcsCfgCleanup *self,basic_block a, basic_block b)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);

   rtx_insn *barrier, *real_b_end;
   rtx_insn *label;
   rtx_jump_table_data *table;

   /* If we are partitioning hot/cold basic blocks, we don't want to
   mess up unconditional or indirect jumps that cross between hot
   and cold sections.

   Basic block partitioning may result in some jumps that appear to
   be optimizable (or blocks that appear to be mergeable), but which really
   must be left untouched (they are required to make it safely across
   partition boundaries).  See the comments at the top of
   bb-reorder.cc:partition_hot_cold_basic_blocks for complete details.  */

   if (BB_PARTITION (a) != BB_PARTITION (b))
      return;

   real_b_end = BB_END (b);

   /* If there is a jump table following block B temporarily add the jump table
   to block B so that it will also be moved to the correct location.  */
   if (tablejump_p (BB_END (b), &label, &table) && prev_active_insn (label) == BB_END (b)){
      BB_END (b) = table;
   }

   /* There had better have been a barrier there.  Delete it.  */
   barrier = NEXT_INSN (BB_END (b));
   if (barrier && BARRIER_P (barrier))
      mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,barrier);


   /* Scramble the insn chain.  */
   mtcs_rtl_reorder_insns_nobb/*!reorder_insns_nobb*/(mtcsRTL,BB_HEAD (b), BB_END (b), BB_END (a));

   /* Restore the real end of b.  */
   BB_END (b) = real_b_end;

   if (dump_file)
      fprintf (dump_file, "Moved block %d after %d and merged.\n",b->index, a->index);

   /* Now blocks A and B are contiguous.  Merge them.  */
   mtcs_cfg_context_merge_blocks/*!merge_blocks*/(mtcsCfgContext,a, b);
}

/* Attempt to merge basic blocks that are potentially non-adjacent.
   Return NULL iff the attempt failed, otherwise return basic block
   where cleanup_cfg should continue.  Because the merging commonly
   moves basic block away or introduces another optimization
   possibility, return basic block just before B so cleanup_cfg don't
   need to iterate.

   It may be good idea to return basic block before C in the case
   C has been moved after B and originally appeared earlier in the
   insn sequence, but we have no information available about the
   relative ordering of these two.  Hopefully it is not too common.  */
//原型 merge_blocks_move cfgcleanup.cc
static basic_block merge_blocks_move (MtcsCfgCleanup *self,edge e, basic_block b, basic_block c, int mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);


   basic_block next;

   /* If we are partitioning hot/cold basic blocks, we don't want to
   mess up unconditional or indirect jumps that cross between hot
   and cold sections.

   Basic block partitioning may result in some jumps that appear to
   be optimizable (or blocks that appear to be mergeable), but which really
   must be left untouched (they are required to make it safely across
   partition boundaries).  See the comments at the top of
   bb-reorder.cc:partition_hot_cold_basic_blocks for complete details.  */
   n_debug("mtcscfgcleanup.c merge_blocks_move 00 bb:%p cc:%p mode:%d\n",b,c,mode);
   if (BB_PARTITION (b) != BB_PARTITION (c))
      return NULL;

   /* If B has a fallthru edge to C, no need to move anything.  */
   if (e->flags & EDGE_FALLTHRU){
      int b_index = b->index, c_index = c->index;

      /* Protect the loop latches.  */
      if (current_loops && c->loop_father->latch == c)
         return NULL;
      n_debug("mtcscfgcleanup.c merge_blocks_move 11 bb:%p cc:%p mode:%d\n",b,c,mode);
      printbb(b);
      n_debug("mtcscfgcleanup.c merge_blocks_move 11xx bb:%p cc:%p mode:%d\n",b,c,mode);
      printbb(c);
      mtcs_cfg_context_merge_blocks/*!merge_blocks*/(mtcsCfgContext,b, c);
      printbb(b);

      update_forwarder_flag(self,b);

      if (dump_file)
         fprintf (dump_file, "Merged %d and %d without moving.\n",b_index, c_index);

      return b->prev_bb == ENTRY_BLOCK_PTR_FOR_FN (cfun) ? b : b->prev_bb;
   }

   /* Otherwise we will need to move code around.  Do that only if expensive
   transformations are allowed.  */
   else if (mode & CLEANUP_EXPENSIVE){
      edge tmp_edge, b_fallthru_edge;
      bool c_has_outgoing_fallthru;
      bool b_has_incoming_fallthru;

      /* Avoid overactive code motion, as the forwarder blocks should be
      eliminated by edge redirection instead.  One exception might have
      been if B is a forwarder block and C has no fallthru edge, but
      that should be cleaned up by bb-reorder instead.  */
      if (FORWARDER_BLOCK_P (b) || FORWARDER_BLOCK_P (c))
         return NULL;

      /* We must make sure to not munge nesting of lexical blocks,
      and loop notes.  This is done by squeezing out all the notes
      and leaving them there to lie.  Not ideal, but functional.  */

      tmp_edge = find_fallthru_edge (c->succs);
      c_has_outgoing_fallthru = (tmp_edge != NULL);

      tmp_edge = find_fallthru_edge (b->preds);
      b_has_incoming_fallthru = (tmp_edge != NULL);
      b_fallthru_edge = tmp_edge;
      next = b->prev_bb;
      if (next == c)
         next = next->prev_bb;

      /* Otherwise, we're going to try to move C after B.  If C does
      not have an outgoing fallthru, then it can be moved
      immediately after B without introducing or modifying jumps.  */
      if (! c_has_outgoing_fallthru){
         merge_blocks_move_successor_nojumps(self,b, c);
         return next == ENTRY_BLOCK_PTR_FOR_FN (cfun) ? next->next_bb : next;
      }

      /* If B does not have an incoming fallthru, then it can be moved
      immediately before C without introducing or modifying jumps.
      C cannot be the first block, so we do not have to worry about
      accessing a non-existent block.  */

      if (b_has_incoming_fallthru){
         basic_block bb;

         if (b_fallthru_edge->src == ENTRY_BLOCK_PTR_FOR_FN (cfun))
            return NULL;
         bb = mtcs_cfg_context_force_nonfallthru/*!force_nonfallthru*/(mtcsCfgContext,b_fallthru_edge);
         if (bb)
            notice_new_block(self,bb);
      }
      n_debug("mtcscfgcleanup.c merge_blocks_move 22 bb:%p cc:%p mode:%d\n",b,c,mode);
      printbb(b);
      n_debug("mtcscfgcleanup.c merge_blocks_move 33 bb:%p cc:%p mode:%d\n",b,c,mode);
      printbb(c);

      merge_blocks_move_predecessor_nojumps(self,b, c);
      return next == ENTRY_BLOCK_PTR_FOR_FN (cfun) ? next->next_bb : next;
   }

   return NULL;
}


/* Removes the memory attributes of MEM expression
   if they are not equal.  */

static void merge_memattrs (MtcsCfgCleanup *self,rtx x, rtx y)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   int i;
   int j;
   enum rtx_code code;
   const char *fmt;

   if (x == y)
      return;
   if (x == 0 || y == 0)
      return;

   code = GET_CODE (x);

   if (code != GET_CODE (y))
      return;

   if (GET_MODE (x) != GET_MODE (y))
      return;

   if (code == MEM && !mem_attrs_eq_p (MEM_ATTRS (x), MEM_ATTRS (y))){
      if (! MEM_ATTRS (x))
         MEM_ATTRS (y) = 0;
      else if (! MEM_ATTRS (y))
         MEM_ATTRS (x) = 0;
      else{
         if (mtcs_rtl_get_mem_alias/*!MEM_ALIAS_SET*/(mtcsRTL,x) != mtcs_rtl_get_mem_alias/*!MEM_ALIAS_SET*/(mtcsRTL,y)){
            mtcs_rtl_set_mem_alias_set/*!set_mem_alias_set*/(mtcsRTL,x, 0);
            mtcs_rtl_set_mem_alias_set/*!set_mem_alias_set*/(mtcsRTL,y, 0);
         }

         if (! mem_expr_equal_p (mtcs_rtl_get_mem_expr/*!MEM_EXPR*/(mtcsRTL,x),
               mtcs_rtl_get_mem_expr/*!MEM_EXPR*/(mtcsRTL,y))){
            mtcs_rtl_set_mem_expr/*!set_mem_expr*/(mtcsRTL,x, 0);
            mtcs_rtl_set_mem_expr/*!set_mem_expr*/(mtcsRTL,y, 0);
            mtcs_rtl_clear_mem_offset/*!clear_mem_offset*/(mtcsRTL,x);
            mtcs_rtl_clear_mem_offset/*!clear_mem_offset*/(mtcsRTL,y);
         }else if (mtcs_rtl_is_mem_offset_known_p/*!MEM_OFFSET_KNOWN_P*/(mtcsRTL,x) !=
               mtcs_rtl_is_mem_offset_known_p/*!MEM_OFFSET_KNOWN_P*/(mtcsRTL,y)
         || (mtcs_rtl_is_mem_offset_known_p/*!MEM_OFFSET_KNOWN_P*/(mtcsRTL,x)
               && maybe_ne (mtcs_rtl_get_mem_offset/*!MEM_OFFSET*/(mtcsRTL,x),
                     mtcs_rtl_get_mem_offset/*!MEM_OFFSET*/(mtcsRTL,y)))){
            mtcs_rtl_clear_mem_offset/*!clear_mem_offset*/(mtcsRTL,x);
            mtcs_rtl_clear_mem_offset/*!clear_mem_offset*/(mtcsRTL,y);
         }

         if (!mtcs_rtl_is_mem_size_known_p/*!MEM_SIZE_KNOWN_P*/(mtcsRTL,x))
            mtcs_rtl_clear_mem_size/*!clear_mem_size*/(mtcsRTL,y);
         else if (!mtcs_rtl_is_mem_size_known_p/*!MEM_SIZE_KNOWN_P*/(mtcsRTL,y))
            mtcs_rtl_clear_mem_size/*!clear_mem_size*/(mtcsRTL,x);
         else if (known_le (mtcs_rtl_get_mem_size/*!MEM_SIZE*/(mtcsRTL,x), mtcs_rtl_get_mem_size/*!MEM_SIZE*/(mtcsRTL,y)))
            mtcs_rtl_set_mem_size/*!set_mem_size*/(mtcsRTL,x, mtcs_rtl_get_mem_size/*!MEM_SIZE*/(mtcsRTL,y));
         else if (known_le (mtcs_rtl_get_mem_size/*!MEM_SIZE*/(mtcsRTL,y), mtcs_rtl_get_mem_size/*!MEM_SIZE*/(mtcsRTL,x)))
            mtcs_rtl_set_mem_size/*!set_mem_size*/(mtcsRTL,y, mtcs_rtl_get_mem_size/*!MEM_SIZE*/(mtcsRTL,x));
         else{
            /* The sizes aren't ordered, so we can't merge them.  */
            mtcs_rtl_clear_mem_size/*!clear_mem_size*/(mtcsRTL,x);
            mtcs_rtl_clear_mem_size/*!clear_mem_size*/(mtcsRTL,y);
         }

         mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,x,
               MIN (mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,x), mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,y)));
         mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,y, mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,x));
      }
   }
   if (code == MEM){
      if (MEM_READONLY_P (x) != MEM_READONLY_P (y)){
         MEM_READONLY_P (x) = 0;
         MEM_READONLY_P (y) = 0;
      }
      if (MEM_NOTRAP_P (x) != MEM_NOTRAP_P (y)){
         MEM_NOTRAP_P (x) = 0;
         MEM_NOTRAP_P (y) = 0;
      }
      if (MEM_VOLATILE_P (x) != MEM_VOLATILE_P (y)){
         MEM_VOLATILE_P (x) = 1;
         MEM_VOLATILE_P (y) = 1;
      }
   }

   fmt = GET_RTX_FORMAT (code);
   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--){
      switch (fmt[i]){
         case 'E':
         /* Two vectors must have the same length.  */
         if (XVECLEN (x, i) != XVECLEN (y, i))
            return;

         for (j = 0; j < XVECLEN (x, i); j++)
            merge_memattrs(self,XVECEXP (x, i, j), XVECEXP (y, i, j));

         break;

         case 'e':
            merge_memattrs(self,XEXP (x, i), XEXP (y, i));
      }
   }
   return;
}


 /* Checks if patterns P1 and P2 are equivalent, apart from the possibly
    different single sets S1 and S2.  */

static bool equal_different_set_p (rtx p1, rtx s1, rtx p2, rtx s2)
{
   int i;
   rtx e1, e2;

   if (p1 == s1 && p2 == s2)
      return true;

   if (GET_CODE (p1) != PARALLEL || GET_CODE (p2) != PARALLEL)
      return false;

   if (XVECLEN (p1, 0) != XVECLEN (p2, 0))
      return false;

   for (i = 0; i < XVECLEN (p1, 0); i++){
      e1 = XVECEXP (p1, 0, i);
      e2 = XVECEXP (p2, 0, i);
      if (e1 == s1 && e2 == s2)
         continue;
      if (reload_completed ? rtx_renumbered_equal_p (e1, e2) : rtx_equal_p (e1, e2))
         continue;

      return false;
   }

   return true;
}


/* NOTE1 is the REG_EQUAL note, if any, attached to an insn
   that is a single_set with a SET_SRC of SRC1.  Similarly
   for NOTE2/SRC2.

   So effectively NOTE1/NOTE2 are an alternate form of
   SRC1/SRC2 respectively.

   Return nonzero if SRC1 or NOTE1 has the same constant
   integer value as SRC2 or NOTE2.   Else return zero.  */
static int values_equal_p (rtx note1, rtx note2, rtx src1, rtx src2)
{
  if (note1
      && note2
      && CONST_INT_P (XEXP (note1, 0))
      && rtx_equal_p (XEXP (note1, 0), XEXP (note2, 0)))
    return 1;

  if (!note1
      && !note2
      && CONST_INT_P (src1)
      && CONST_INT_P (src2)
      && rtx_equal_p (src1, src2))
    return 1;

  if (note1
      && CONST_INT_P (src2)
      && rtx_equal_p (XEXP (note1, 0), src2))
    return 1;

  if (note2
      && CONST_INT_P (src1)
      && rtx_equal_p (XEXP (note2, 0), src1))
    return 1;

  return 0;
}

/* Examine register notes on I1 and I2 and return:
   - dir_forward if I1 can be replaced by I2, or
   - dir_backward if I2 can be replaced by I1, or
   - dir_both if both are the case.  */

static enum replace_direction can_replace_by (rtx_insn *i1, rtx_insn *i2)
{
   rtx s1, s2, d1, d2, src1, src2, note1, note2;
   bool c1, c2;

   /* Check for 2 sets.  */
   s1 = single_set (i1);
   s2 = single_set (i2);
   if (s1 == NULL_RTX || s2 == NULL_RTX)
      return dir_none;

   /* Check that the 2 sets set the same dest.  */
   d1 = SET_DEST (s1);
   d2 = SET_DEST (s2);
   if (!(reload_completed ? rtx_renumbered_equal_p (d1, d2) : rtx_equal_p (d1, d2)))
      return dir_none;

   /* Find identical req_equiv or reg_equal note, which implies that the 2 sets
   set dest to the same value.  */
   note1 = find_reg_equal_equiv_note (i1);
   note2 = find_reg_equal_equiv_note (i2);

   src1 = SET_SRC (s1);
   src2 = SET_SRC (s2);

   if (!values_equal_p (note1, note2, src1, src2))
      return dir_none;

   if (!equal_different_set_p (PATTERN (i1), s1, PATTERN (i2), s2))
      return dir_none;

   /* Although the 2 sets set dest to the same value, we cannot replace
   (set (dest) (const_int))
   by
   (set (dest) (reg))
   because we don't know if the reg is live and has the same value at the
   location of replacement.  */
   c1 = CONST_INT_P (src1);
   c2 = CONST_INT_P (src2);
   if (c1 && c2)
      return dir_both;
   else if (c2)
      return dir_forward;
   else if (c1)
      return dir_backward;

   return dir_none;
}

/* Merges directions A and B.  */

static enum replace_direction merge_dir (enum replace_direction a, enum replace_direction b)
{
  /* Implements the following table:
        |bo fw bw no
     ---+-----------
     bo |bo fw bw no
     fw |-- fw no no
     bw |-- -- bw no
     no |-- -- -- no.  */

  if (a == b)
    return a;

  if (a == dir_both)
    return b;
  if (b == dir_both)
    return a;

  return dir_none;
}

/* Array of flags indexed by reg note kind, true if the given
   reg note is CFA related.  */
static const bool reg_note_cfa_p[] = {
#undef REG_CFA_NOTE
#define DEF_REG_NOTE(NAME) false,
#define REG_CFA_NOTE(NAME) true,
#include "reg-notes.def"
#undef REG_CFA_NOTE
#undef DEF_REG_NOTE
  false
};

/* Return true if I1 and I2 have identical CFA notes (the same order
   and equivalent content).  */

static bool insns_have_identical_cfa_notes (rtx_insn *i1, rtx_insn *i2)
{
   rtx n1, n2;
   for (n1 = REG_NOTES (i1), n2 = REG_NOTES (i2); ; n1 = XEXP (n1, 1), n2 = XEXP (n2, 1)){
      /* Skip over reg notes not related to CFI information.  */
      while (n1 && !reg_note_cfa_p[REG_NOTE_KIND (n1)])
         n1 = XEXP (n1, 1);
      while (n2 && !reg_note_cfa_p[REG_NOTE_KIND (n2)])
      n2 = XEXP (n2, 1);
      if (n1 == NULL_RTX && n2 == NULL_RTX)
         return true;
      if (n1 == NULL_RTX || n2 == NULL_RTX)
         return false;
      if (XEXP (n1, 0) == XEXP (n2, 0))
         ;
      else if (XEXP (n1, 0) == NULL_RTX || XEXP (n2, 0) == NULL_RTX)
         return false;
      else if (!(reload_completed
      ? rtx_renumbered_equal_p (XEXP (n1, 0), XEXP (n2, 0))
      : rtx_equal_p (XEXP (n1, 0), XEXP (n2, 0))))
         return false;
   }
}

/* Examine I1 and I2 and return:
   - dir_forward if I1 can be replaced by I2, or
   - dir_backward if I2 can be replaced by I1, or
   - dir_both if both are the case.  */

static enum replace_direction old_insns_match_p (MtcsCfgCleanup *self,int mode ATTRIBUTE_UNUSED, rtx_insn *i1, rtx_insn *i2)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsFuncAbi *mtcsFuncAbi=mtcs_target_get_func_abi(mtcsTarget);

   rtx p1, p2;

   /* Verify that I1 and I2 are equivalent.  */
   if (GET_CODE (i1) != GET_CODE (i2))
      return dir_none;

   /* __builtin_unreachable() may lead to empty blocks (ending with
   NOTE_INSN_BASIC_BLOCK).  They may be crossjumped. */
   if (NOTE_INSN_BASIC_BLOCK_P (i1) && NOTE_INSN_BASIC_BLOCK_P (i2))
      return dir_both;

   /* ??? Do not allow cross-jumping between different stack levels.  */
   p1 = find_reg_note (i1, REG_ARGS_SIZE, NULL);
   p2 = find_reg_note (i2, REG_ARGS_SIZE, NULL);
   if (p1 && p2){
      p1 = XEXP (p1, 0);
      p2 = XEXP (p2, 0);
      if (!rtx_equal_p (p1, p2))
         return dir_none;

      /* ??? Worse, this adjustment had better be constant lest we
      have differing incoming stack levels.  */
      if (!mtcsRtlData->x_frame_pointer_needed/*!frame_pointer_needed*/
            && known_eq (find_args_size_adjust (i1), HOST_WIDE_INT_MIN))
         return dir_none;
   }else if (p1 || p2)
      return dir_none;

   /* Do not allow cross-jumping between frame related insns and other
   insns.  */
   if (RTX_FRAME_RELATED_P (i1) != RTX_FRAME_RELATED_P (i2))
      return dir_none;

   p1 = PATTERN (i1);
   p2 = PATTERN (i2);

   if (GET_CODE (p1) != GET_CODE (p2))
      return dir_none;

   /* If this is a CALL_INSN, compare register usage information.
   If we don't check this on stack register machines, the two
   CALL_INSNs might be merged leaving reg-stack.cc with mismatching
   numbers of stack registers in the same basic block.
   If we don't check this on machines with delay slots, a delay slot may
   be filled that clobbers a parameter expected by the subroutine.

   ??? We take the simple route for now and assume that if they're
   equal, they were constructed identically.

   Also check for identical exception regions.  */

   if (CALL_P (i1)){
      /* Ensure the same EH region.  */
      rtx n1 = find_reg_note (i1, REG_EH_REGION, 0);
      rtx n2 = find_reg_note (i2, REG_EH_REGION, 0);

      if (!n1 && n2)
         return dir_none;

      if (n1 && (!n2 || XEXP (n1, 0) != XEXP (n2, 0)))
         return dir_none;

      if (!rtx_equal_p (CALL_INSN_FUNCTION_USAGE (i1),
      CALL_INSN_FUNCTION_USAGE (i2)) || SIBLING_CALL_P (i1) != SIBLING_CALL_P (i2))
         return dir_none;

      /* For address sanitizer, never crossjump __asan_report_* builtins,
      otherwise errors might be reported on incorrect lines.  */
      if (flag_sanitize & SANITIZE_ADDRESS){
         rtx call = get_call_rtx_from (i1);
         if (call && GET_CODE (XEXP (XEXP (call, 0), 0)) == SYMBOL_REF){
            rtx symbol = XEXP (XEXP (call, 0), 0);
            if (SYMBOL_REF_DECL (symbol) && TREE_CODE (SYMBOL_REF_DECL (symbol)) == FUNCTION_DECL){
               if ((DECL_BUILT_IN_CLASS (SYMBOL_REF_DECL (symbol)) == BUILT_IN_NORMAL)
               && DECL_FUNCTION_CODE (SYMBOL_REF_DECL (symbol)) >= BUILT_IN_ASAN_REPORT_LOAD1
               && DECL_FUNCTION_CODE (SYMBOL_REF_DECL (symbol)) <= BUILT_IN_ASAN_STOREN)
                  return dir_none;
            }
         }
      }

      if (mtcs_func_abi_insn_callee_abi/*!insn_callee_abi*/(mtcsFuncAbi,i1) !=
            mtcs_func_abi_insn_callee_abi/*!insn_callee_abi*/(mtcsFuncAbi,i2))
         return dir_none;
   }

   /* If both i1 and i2 are frame related, verify all the CFA notes
   in the same order and with the same content.  */
   if (RTX_FRAME_RELATED_P (i1) && !insns_have_identical_cfa_notes (i1, i2))
      return dir_none;

//#ifdef STACK_REGS //host=1 nvptx=1
//   /* If cross_jump_death_matters is not 0, the insn's mode
//   indicates whether or not the insn contains any stack-like
//   regs.  */
//
//   if ((mode & CLEANUP_POST_REGSTACK) && stack_regs_mentioned (i1)){
//      /* If register stack conversion has already been done, then
//      death notes must also be compared before it is certain that
//      the two instruction streams match.  */
//
//      rtx note;
//      HARD_REG_SET i1_regset, i2_regset;
//
//      CLEAR_HARD_REG_SET (i1_regset);
//      CLEAR_HARD_REG_SET (i2_regset);
//
//      for (note = REG_NOTES (i1); note; note = XEXP (note, 1))
//         if (REG_NOTE_KIND (note) == REG_DEAD && STACK_REG_P (XEXP (note, 0)))
//            SET_HARD_REG_BIT (i1_regset, REGNO (XEXP (note, 0)));
//
//      for (note = REG_NOTES (i2); note; note = XEXP (note, 1))
//         if (REG_NOTE_KIND (note) == REG_DEAD && STACK_REG_P (XEXP (note, 0)))
//            SET_HARD_REG_BIT (i2_regset, REGNO (XEXP (note, 0)));
//
//      if (i1_regset != i2_regset)
//         return dir_none;
//   }
//#endif

   if (reload_completed ? rtx_renumbered_equal_p (p1, p2) : rtx_equal_p (p1, p2))
      return dir_both;

   return can_replace_by (i1, i2);
}

/* When comparing insns I1 and I2 in flow_find_cross_jump or
   flow_find_head_matching_sequence, ensure the notes match.  */

static void merge_notes (MtcsCfgCleanup *self,rtx_insn *i1, rtx_insn *i2)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   /* If the merged insns have different REG_EQUAL notes, then
   remove them.  */
   rtx equiv1 = find_reg_equal_equiv_note (i1);
   rtx equiv2 = find_reg_equal_equiv_note (i2);

   if (equiv1 && !equiv2)
      mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,i1, equiv1);
   else if (!equiv1 && equiv2)
      mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,i2, equiv2);
   else if (equiv1 && equiv2 && !rtx_equal_p (XEXP (equiv1, 0), XEXP (equiv2, 0))){
      mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,i1, equiv1);
      mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,i2, equiv2);
   }
}

 /* Walks from I1 in BB1 backward till the next non-debug insn, and returns the
    resulting insn in I1, and the corresponding bb in BB1.  At the head of a
    bb, if there is a predecessor bb that reaches this bb via fallthru, and
    FOLLOW_FALLTHRU, walks further in the predecessor bb and registers this in
    DID_FALLTHRU.  Otherwise, stops at the head of the bb.  */

static void walk_to_nondebug_insn (rtx_insn **i1, basic_block *bb1, bool follow_fallthru,
                       bool *did_fallthru)
{
   edge fallthru;

   *did_fallthru = false;

   /* Ignore notes.  */
   while (!NONDEBUG_INSN_P (*i1)){
      if (*i1 != BB_HEAD (*bb1)){
         *i1 = PREV_INSN (*i1);
         continue;
      }

      if (!follow_fallthru)
         return;

      fallthru = find_fallthru_edge ((*bb1)->preds);
      if (!fallthru || fallthru->src == ENTRY_BLOCK_PTR_FOR_FN (cfun)
      || !single_succ_p (fallthru->src))
         return;

      *bb1 = fallthru->src;
      *i1 = BB_END (*bb1);
      *did_fallthru = true;
   }
}

/* Look through the insns at the end of BB1 and BB2 and find the longest
   sequence that are either equivalent, or allow forward or backward
   replacement.  Store the first insns for that sequence in *F1 and *F2 and
   return the sequence length.

   DIR_P indicates the allowed replacement direction on function entry, and
   the actual replacement direction on function exit.  If NULL, only equivalent
   sequences are allowed.

   To simplify callers of this function, if the blocks match exactly,
   store the head of the blocks in *F1 and *F2.  */
//原型 flow_find_cross_jump cfgcleanup.h cfgcleanup.cc
int mtcs_cfg_cleanup_flow_find_cross_jump (MtcsCfgCleanup *self,basic_block bb1, basic_block bb2, rtx_insn **f1,
            rtx_insn **f2, enum replace_direction *dir_p)
{
   rtx_insn *i1, *i2, *last1, *last2, *afterlast1, *afterlast2;
   int ninsns = 0;
   enum replace_direction dir, last_dir, afterlast_dir;
   bool follow_fallthru, did_fallthru;

   if (dir_p)
      dir = *dir_p;
   else
      dir = dir_both;
   afterlast_dir = dir;
   last_dir = afterlast_dir;

   /* Skip simple jumps at the end of the blocks.  Complex jumps still
   need to be compared for equivalence, which we'll do below.  */

   i1 = BB_END (bb1);
   last1 = afterlast1 = last2 = afterlast2 = NULL;
   if (onlyjump_p (i1) || (returnjump_p (i1) && !side_effects_p (PATTERN (i1)))){
      last1 = i1;
      i1 = PREV_INSN (i1);
   }

   i2 = BB_END (bb2);
   if (onlyjump_p (i2) || (returnjump_p (i2) && !side_effects_p (PATTERN (i2)))){
      last2 = i2;
      /* Count everything except for unconditional jump as insn.
      Don't count any jumps if dir_p is NULL.  */
      if (!simplejump_p (i2) && !returnjump_p (i2) && last1 && dir_p)
         ninsns++;
      i2 = PREV_INSN (i2);
   }

   while (true){
      /* In the following example, we can replace all jumps to C by jumps to A.

      This removes 4 duplicate insns.
      [bb A] insn1            [bb C] insn1
      insn2                   insn2
      [bb B] insn3                   insn3
      insn4                   insn4
      jump_insn               jump_insn

      We could also replace all jumps to A by jumps to C, but that leaves B
      alive, and removes only 2 duplicate insns.  In a subsequent crossjump
      step, all jumps to B would be replaced with jumps to the middle of C,
      achieving the same result with more effort.
      So we allow only the first possibility, which means that we don't allow
      fallthru in the block that's being replaced.  */

      follow_fallthru = dir_p && dir != dir_forward;
      walk_to_nondebug_insn (&i1, &bb1, follow_fallthru, &did_fallthru);
      if (did_fallthru)
         dir = dir_backward;

      follow_fallthru = dir_p && dir != dir_backward;
      walk_to_nondebug_insn (&i2, &bb2, follow_fallthru, &did_fallthru);
      if (did_fallthru)
         dir = dir_forward;

      if (i1 == BB_HEAD (bb1) || i2 == BB_HEAD (bb2))
         break;

      /* Do not turn corssing edge to non-crossing or vice versa after
      reload. */
      if (BB_PARTITION (BLOCK_FOR_INSN (i1))
      != BB_PARTITION (BLOCK_FOR_INSN (i2))
      && reload_completed)
         break;

      dir = merge_dir (dir, old_insns_match_p(self,0, i1, i2));
      if (dir == dir_none || (!dir_p && dir != dir_both))
         break;

      merge_memattrs(self,i1, i2);

      /* Don't begin a cross-jump with a NOTE insn.  */
      if (INSN_P (i1)){
         merge_notes(self,i1, i2);

         afterlast1 = last1, afterlast2 = last2;
         last1 = i1, last2 = i2;
         afterlast_dir = last_dir;
         last_dir = dir;
         if (active_insn_p (i1))
            ninsns++;
      }

      i1 = PREV_INSN (i1);
      i2 = PREV_INSN (i2);
   }

   /* Include preceding notes and labels in the cross-jump.  One,
   this may bring us to the head of the blocks as requested above.
   Two, it keeps line number notes as matched as may be.  */
   if (ninsns){
      bb1 = BLOCK_FOR_INSN (last1);
      while (last1 != BB_HEAD (bb1) && !NONDEBUG_INSN_P (PREV_INSN (last1)))
         last1 = PREV_INSN (last1);

      if (last1 != BB_HEAD (bb1) && LABEL_P (PREV_INSN (last1)))
         last1 = PREV_INSN (last1);

      bb2 = BLOCK_FOR_INSN (last2);
      while (last2 != BB_HEAD (bb2) && !NONDEBUG_INSN_P (PREV_INSN (last2)))
         last2 = PREV_INSN (last2);

      if (last2 != BB_HEAD (bb2) && LABEL_P (PREV_INSN (last2)))
         last2 = PREV_INSN (last2);

      *f1 = last1;
      *f2 = last2;
   }

   if (dir_p)
      *dir_p = last_dir;
   return ninsns;
}

/* Like flow_find_cross_jump, except start looking for a matching sequence from
   the head of the two blocks.  Do not include jumps at the end.
   If STOP_AFTER is nonzero, stop after finding that many matching
   instructions.  If STOP_AFTER is zero, count all INSN_P insns, if it is
   non-zero, only count active insns.  */
//原型 flow_find_head_matching_sequence cfgcleanup.h cfgcleanup.cc
int mtcs_cfg_cleanup_flow_find_head_matching_sequence (MtcsCfgCleanup *self,basic_block bb1, basic_block bb2, rtx_insn **f1,
              rtx_insn **f2, int stop_after)
{
   rtx_insn *i1, *i2, *last1, *last2, *beforelast1, *beforelast2;
   int ninsns = 0;
   edge e;
   edge_iterator ei;
   int nehedges1 = 0, nehedges2 = 0;

   FOR_EACH_EDGE (e, ei, bb1->succs)
      if (e->flags & EDGE_EH)
         nehedges1++;
   FOR_EACH_EDGE (e, ei, bb2->succs)
      if (e->flags & EDGE_EH)
         nehedges2++;

   i1 = BB_HEAD (bb1);
   i2 = BB_HEAD (bb2);
   last1 = beforelast1 = last2 = beforelast2 = NULL;

   while (true){
      /* Ignore notes, except NOTE_INSN_EPILOGUE_BEG.  */
      while (!NONDEBUG_INSN_P (i1) && i1 != BB_END (bb1)){
         if (NOTE_P (i1) && NOTE_KIND (i1) == NOTE_INSN_EPILOGUE_BEG)
            break;
         i1 = NEXT_INSN (i1);
      }

      while (!NONDEBUG_INSN_P (i2) && i2 != BB_END (bb2)){
         if (NOTE_P (i2) && NOTE_KIND (i2) == NOTE_INSN_EPILOGUE_BEG)
            break;
         i2 = NEXT_INSN (i2);
      }

      if ((i1 == BB_END (bb1) && !NONDEBUG_INSN_P (i1)) || (i2 == BB_END (bb2) && !NONDEBUG_INSN_P (i2)))
         break;

      if (NOTE_P (i1) || NOTE_P (i2) || JUMP_P (i1) || JUMP_P (i2))
         break;

      /* A sanity check to make sure we're not merging insns with different
      effects on EH.  If only one of them ends a basic block, it shouldn't
      have an EH edge; if both end a basic block, there should be the same
      number of EH edges.  */
      if ((i1 == BB_END (bb1) && i2 != BB_END (bb2)
      && nehedges1 > 0)
      || (i2 == BB_END (bb2) && i1 != BB_END (bb1)
      && nehedges2 > 0)
      || (i1 == BB_END (bb1) && i2 == BB_END (bb2)
      && nehedges1 != nehedges2))
         break;

      if (old_insns_match_p(self,0, i1, i2) != dir_both)
         break;

      merge_memattrs(self,i1, i2);

      /* Don't begin a cross-jump with a NOTE insn.  */
      if (INSN_P (i1)){
         merge_notes(self,i1, i2);

         beforelast1 = last1, beforelast2 = last2;
         last1 = i1, last2 = i2;
         if (!stop_after || active_insn_p (i1))
            ninsns++;
      }

      if (i1 == BB_END (bb1) || i2 == BB_END (bb2)
      || (stop_after > 0 && ninsns == stop_after))
         break;

      i1 = NEXT_INSN (i1);
      i2 = NEXT_INSN (i2);
   }

   if (ninsns){
      *f1 = last1;
      *f2 = last2;
   }

   return ninsns;
}

/* Return true iff outgoing edges of BB1 and BB2 match, together with
   the branch instruction.  This means that if we commonize the control
   flow before end of the basic block, the semantic remains unchanged.

   We may assume that there exists one edge with a common destination.  */

static bool outgoing_edges_match (MtcsCfgCleanup *self,int mode, basic_block bb1, basic_block bb2)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   int nehedges1 = 0, nehedges2 = 0;
   edge fallthru1 = 0, fallthru2 = 0;
   edge e1, e2;
   edge_iterator ei;

   /* If we performed shrink-wrapping, edges to the exit block can
   only be distinguished for JUMP_INSNs.  The two paths may differ in
   whether they went through the prologue.  Sibcalls are fine, we know
   that we either didn't need or inserted an epilogue before them.  */
   if (mtcsRtlData/*!crtl*/->shrink_wrapped
   && single_succ_p (bb1)
   && single_succ (bb1) == EXIT_BLOCK_PTR_FOR_FN (cfun)
   && (!JUMP_P (BB_END (bb1))
   /* Punt if the only successor is a fake edge to exit, the jump
   must be some weird one.  */
   || (single_succ_edge (bb1)->flags & EDGE_FAKE) != 0)
   && !(CALL_P (BB_END (bb1)) && SIBLING_CALL_P (BB_END (bb1))))
      return false;

   /* If BB1 has only one successor, we may be looking at either an
   unconditional jump, or a fake edge to exit.  */
   if (single_succ_p (bb1)
   && (single_succ_edge (bb1)->flags & (EDGE_COMPLEX | EDGE_FAKE)) == 0
   && (!JUMP_P (BB_END (bb1)) || simplejump_p (BB_END (bb1))))
      return (single_succ_p (bb2) && (single_succ_edge (bb2)->flags
                      & (EDGE_COMPLEX | EDGE_FAKE)) == 0 && (!JUMP_P (BB_END (bb2)) || simplejump_p (BB_END (bb2))));

   /* Match conditional jumps - this may get tricky when fallthru and branch
   edges are crossed.  */
   if (EDGE_COUNT (bb1->succs) == 2
   && any_condjump_p (BB_END (bb1))
   && onlyjump_p (BB_END (bb1))){
      edge b1, f1, b2, f2;
      bool reverse, match;
      rtx set1, set2, cond1, cond2;
      enum rtx_code code1, code2;

      if (EDGE_COUNT (bb2->succs) != 2
      || !any_condjump_p (BB_END (bb2))
      || !onlyjump_p (BB_END (bb2)))
         return false;

      b1 = BRANCH_EDGE (bb1);
      b2 = BRANCH_EDGE (bb2);
      f1 = FALLTHRU_EDGE (bb1);
      f2 = FALLTHRU_EDGE (bb2);

      /* Get around possible forwarders on fallthru edges.  Other cases
      should be optimized out already.  */
      if (FORWARDER_BLOCK_P (f1->dest))
         f1 = single_succ_edge (f1->dest);

      if (FORWARDER_BLOCK_P (f2->dest))
         f2 = single_succ_edge (f2->dest);

      /* To simplify use of this function, return false if there are
      unneeded forwarder blocks.  These will get eliminated later
      during cleanup_cfg.  */
      if (FORWARDER_BLOCK_P (f1->dest)
      || FORWARDER_BLOCK_P (f2->dest)
      || FORWARDER_BLOCK_P (b1->dest)
      || FORWARDER_BLOCK_P (b2->dest))
         return false;

      if (f1->dest == f2->dest && b1->dest == b2->dest)
         reverse = false;
      else if (f1->dest == b2->dest && b1->dest == f2->dest)
         reverse = true;
      else
         return false;

      set1 = pc_set (BB_END (bb1));
      set2 = pc_set (BB_END (bb2));
      if ((XEXP (SET_SRC (set1), 1) == pc_rtx) != (XEXP (SET_SRC (set2), 1) == pc_rtx))
         reverse = !reverse;

      cond1 = XEXP (SET_SRC (set1), 0);
      cond2 = XEXP (SET_SRC (set2), 0);
      code1 = GET_CODE (cond1);
      if (reverse)
         code2 = mtcs_dojump_reversed_comparison_code/*!reversed_comparison_code*/(mtcsDojump,cond2, BB_END (bb2));
      else
         code2 = GET_CODE (cond2);

      if (code2 == UNKNOWN)
         return false;

      /* Verify codes and operands match.  */
      match = ((code1 == code2
      && rtx_renumbered_equal_p (XEXP (cond1, 0), XEXP (cond2, 0))
      && rtx_renumbered_equal_p (XEXP (cond1, 1), XEXP (cond2, 1)))
      || (code1 == swap_condition (code2)
      && rtx_renumbered_equal_p (XEXP (cond1, 1),
      XEXP (cond2, 0))
      && rtx_renumbered_equal_p (XEXP (cond1, 0),
      XEXP (cond2, 1))));

      /* If we return true, we will join the blocks.  Which means that
      we will only have one branch prediction bit to work with.  Thus
      we require the existing branches to have probabilities that are
      roughly similar.  */
      if (match
      && optimize_bb_for_speed_p (bb1)
      && optimize_bb_for_speed_p (bb2)){
         profile_probability prob2;

         if (b1->dest == b2->dest)
            prob2 = b2->probability;
         else
            /* Do not use f2 probability as f2 may be forwarded.  */
            prob2 = b2->probability.invert ();

         /* Fail if the difference in probabilities is greater than 50%.
         This rules out two well-predicted branches with opposite
         outcomes.  */
         if (b1->probability.differs_lot_from_p (prob2)){
            if (dump_file){
               fprintf (dump_file,"Outcomes of branch in bb %i and %i differ too much (", bb1->index, bb2->index);
               b1->probability.dump (dump_file);
               prob2.dump (dump_file);
               fprintf (dump_file, ")\n");
            }
            return false;
         }
      }

      if (dump_file && match)
         fprintf (dump_file, "Conditionals in bb %i and %i match.\n",
      bb1->index, bb2->index);

      return match;
   }

   /* Generic case - we are seeing a computed jump, table jump or trapping
   instruction.  */

   /* Check whether there are tablejumps in the end of BB1 and BB2.
   Return true if they are identical.  */
   {
      rtx_insn *label1, *label2;
      rtx_jump_table_data *table1, *table2;

      if (tablejump_p (BB_END (bb1), &label1, &table1)
      && tablejump_p (BB_END (bb2), &label2, &table2)
      && GET_CODE (PATTERN (table1)) == GET_CODE (PATTERN (table2))){
         /* The labels should never be the same rtx.  If they really are same
         the jump tables are same too. So disable crossjumping of blocks BB1
         and BB2 because when deleting the common insns in the end of BB1
         by delete_basic_block () the jump table would be deleted too.  */
         /* If LABEL2 is referenced in BB1->END do not do anything
         because we would loose information when replacing
         LABEL1 by LABEL2 and then LABEL2 by LABEL1 in BB1->END.  */
         if (label1 != label2 && !rtx_referenced_p (label2, BB_END (bb1))){
            /* Set IDENTICAL to true when the tables are identical.  */
            bool identical = false;
            rtx p1, p2;

            p1 = PATTERN (table1);
            p2 = PATTERN (table2);
            if (GET_CODE (p1) == ADDR_VEC && rtx_equal_p (p1, p2)){
            identical = true;
            }else if (GET_CODE (p1) == ADDR_DIFF_VEC
            && (XVECLEN (p1, 1) == XVECLEN (p2, 1))
            && rtx_equal_p (XEXP (p1, 2), XEXP (p2, 2))
            && rtx_equal_p (XEXP (p1, 3), XEXP (p2, 3))){
               int i;

               identical = true;
               for (i = XVECLEN (p1, 1) - 1; i >= 0 && identical; i--)
                  if (!rtx_equal_p (XVECEXP (p1, 1, i), XVECEXP (p2, 1, i)))
                     identical = false;
            }

            if (identical){
               bool match;

               /* Temporarily replace references to LABEL1 with LABEL2
               in BB1->END so that we could compare the instructions.  */
               mtcs_rtlanal_replace_label_in_insn/*!replace_label_in_insn*/(mtcsRtlanal,BB_END (bb1), label1, label2, false);

               match = (old_insns_match_p(self,mode, BB_END (bb1), BB_END (bb2))== dir_both);
               if (dump_file && match)
                  fprintf (dump_file,"Tablejumps in bb %i and %i match.\n", bb1->index, bb2->index);

               /* Set the original label in BB1->END because when deleting
               a block whose end is a tablejump, the tablejump referenced
               from the instruction is deleted too.  */
               mtcs_rtlanal_replace_label_in_insn/*!replace_label_in_insn*/(mtcsRtlanal,BB_END (bb1), label2, label1, false);

               return match;
            }
         }
         return false;
      }
   }

   /* Find the last non-debug non-note instruction in each bb, except
   stop when we see the NOTE_INSN_BASIC_BLOCK, as old_insns_match_p
   handles that case specially. old_insns_match_p does not handle
   other types of instruction notes.  */
   rtx_insn *last1 = BB_END (bb1);
   rtx_insn *last2 = BB_END (bb2);
   while (!NOTE_INSN_BASIC_BLOCK_P (last1) && (DEBUG_INSN_P (last1) || NOTE_P (last1)))
      last1 = PREV_INSN (last1);
   while (!NOTE_INSN_BASIC_BLOCK_P (last2) && (DEBUG_INSN_P (last2) || NOTE_P (last2)))
      last2 = PREV_INSN (last2);
   gcc_assert (last1 && last2);

   /* First ensure that the instructions match.  There may be many outgoing
   edges so this test is generally cheaper.  */
   if (old_insns_match_p(self,mode, last1, last2) != dir_both)
      return false;

   /* Search the outgoing edges, ensure that the counts do match, find possible
   fallthru and exception handling edges since these needs more
   validation.  */
   if (EDGE_COUNT (bb1->succs) != EDGE_COUNT (bb2->succs))
      return false;

   bool nonfakeedges = false;
   FOR_EACH_EDGE (e1, ei, bb1->succs){
      e2 = EDGE_SUCC (bb2, ei.index);

      if ((e1->flags & EDGE_FAKE) == 0)
         nonfakeedges = true;

      if (e1->flags & EDGE_EH)
         nehedges1++;

      if (e2->flags & EDGE_EH)
         nehedges2++;

      if (e1->flags & EDGE_FALLTHRU)
         fallthru1 = e1;
      if (e2->flags & EDGE_FALLTHRU)
         fallthru2 = e2;
   }

   /* If number of edges of various types does not match, fail.  */
   if (nehedges1 != nehedges2 || (fallthru1 != 0) != (fallthru2 != 0))
      return false;

   /* If !ACCUMULATE_OUTGOING_ARGS, bb1 (and bb2) have no successors
   and the last real insn doesn't have REG_ARGS_SIZE note, don't
   attempt to optimize, as the two basic blocks might have different
   REG_ARGS_SIZE depths.  For noreturn calls and unconditional
   traps there should be REG_ARG_SIZE notes, they could be missing
   for __builtin_unreachable () uses though.  */
   if (!nonfakeedges  && !ACCUMULATE_OUTGOING_ARGS
   && (!INSN_P (last1) || !find_reg_note (last1, REG_ARGS_SIZE, NULL)))
      return false;

   /* fallthru edges must be forwarded to the same destination.  */
   if (fallthru1){
      basic_block d1 = (FORWARDER_BLOCK_P (fallthru1->dest)
      ? single_succ (fallthru1->dest): fallthru1->dest);
      basic_block d2 = (FORWARDER_BLOCK_P (fallthru2->dest)
      ? single_succ (fallthru2->dest): fallthru2->dest);

      if (d1 != d2)
         return false;
   }

   /* Ensure the same EH region.  */
   {
      rtx n1 = find_reg_note (last1, REG_EH_REGION, 0);
      rtx n2 = find_reg_note (last2, REG_EH_REGION, 0);

      if (!n1 && n2)
         return false;

      if (n1 && (!n2 || XEXP (n1, 0) != XEXP (n2, 0)))
         return false;
   }

   /* The same checks as in try_crossjump_to_edge. It is required for RTL
   version of sequence abstraction.  */
   FOR_EACH_EDGE (e1, ei, bb2->succs){
      edge e2;
      edge_iterator ei;
      basic_block d1 = e1->dest;

      if (FORWARDER_BLOCK_P (d1))
         d1 = EDGE_SUCC (d1, 0)->dest;

      FOR_EACH_EDGE (e2, ei, bb1->succs){
         basic_block d2 = e2->dest;
         if (FORWARDER_BLOCK_P (d2))
            d2 = EDGE_SUCC (d2, 0)->dest;
         if (d1 == d2)
            break;
      }

      if (!e2)
         return false;
   }

   return true;
}

/* Returns true if BB basic block has a preserve label.  */

static bool block_has_preserve_label (MtcsCfgCleanup *self,basic_block bb)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   return (bb
          && mtcs_cfg_rtl_block_label/*!block_label*/(mtcsCfgRtl,bb)
          && LABEL_PRESERVE_P (mtcs_cfg_rtl_block_label/*!block_label*/(mtcsCfgRtl,bb)));
}

/* E1 and E2 are edges with the same destination block.  Search their
   predecessors for common code.  If found, redirect control flow from
   (maybe the middle of) E1->SRC to (maybe the middle of) E2->SRC (dir_forward),
   or the other way around (dir_backward).  DIR specifies the allowed
   replacement direction.  */

static bool try_crossjump_to_edge (MtcsCfgCleanup *self,int mode, edge e1, edge e2,
                       enum replace_direction dir)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   int nmatch;
   basic_block src1 = e1->src, src2 = e2->src;
   basic_block redirect_to, redirect_from, to_remove;
   basic_block osrc1, osrc2, redirect_edges_to, tmp;
   rtx_insn *newpos1, *newpos2;
   edge s;
   edge_iterator ei;

   newpos1 = newpos2 = NULL;

   /* Search backward through forwarder blocks.  We don't need to worry
   about multiple entry or chained forwarders, as they will be optimized
   away.  We do this to look past the unconditional jump following a
   conditional jump that is required due to the current CFG shape.  */
   if (single_pred_p (src1) && FORWARDER_BLOCK_P (src1))
      e1 = single_pred_edge (src1), src1 = e1->src;

   if (single_pred_p (src2) && FORWARDER_BLOCK_P (src2))
      e2 = single_pred_edge (src2), src2 = e2->src;

   /* Nothing to do if we reach ENTRY, or a common source block.  */
   if (src1 == ENTRY_BLOCK_PTR_FOR_FN (cfun) || src2 == ENTRY_BLOCK_PTR_FOR_FN (cfun))
      return false;
   if (src1 == src2)
      return false;

   /* Seeing more than 1 forwarder blocks would confuse us later...  */
   if (FORWARDER_BLOCK_P (e1->dest) && FORWARDER_BLOCK_P (single_succ (e1->dest)))
      return false;

   if (FORWARDER_BLOCK_P (e2->dest) && FORWARDER_BLOCK_P (single_succ (e2->dest)))
      return false;

   /* Likewise with dead code (possibly newly created by the other optimizations
   of cfg_cleanup).  */
   if (EDGE_COUNT (src1->preds) == 0 || EDGE_COUNT (src2->preds) == 0)
      return false;

   /* Do not turn corssing edge to non-crossing or vice versa after reload.  */
   if (BB_PARTITION (src1) != BB_PARTITION (src2) && reload_completed)
      return false;

   /* Look for the common insn sequence, part the first ...  */
   if (!outgoing_edges_match(self,mode, src1, src2))
      return false;

   /* ... and part the second.  */
   nmatch =mtcs_cfg_cleanup_flow_find_cross_jump/*!flow_find_cross_jump*/(self,src1, src2, &newpos1, &newpos2, &dir);

   osrc1 = src1;
   osrc2 = src2;
   if (newpos1 != NULL_RTX)
      src1 = BLOCK_FOR_INSN (newpos1);
   if (newpos2 != NULL_RTX)
      src2 = BLOCK_FOR_INSN (newpos2);

   /* Check that SRC1 and SRC2 have preds again.  They may have changed
   above due to the call to flow_find_cross_jump.  */
   if (EDGE_COUNT (src1->preds) == 0 || EDGE_COUNT (src2->preds) == 0)
      return false;

   if (dir == dir_backward){
      std::swap (osrc1, osrc2);
      std::swap (src1, src2);
      std::swap (e1, e2);
      std::swap (newpos1, newpos2);
   }

   /* Don't proceed with the crossjump unless we found a sufficient number
   of matching instructions or the 'from' block was totally matched
   (such that its predecessors will hopefully be redirected and the
   block removed).  */
   if ((nmatch < param_min_crossjump_insns) && (newpos1 != BB_HEAD (src1)))
      return false;

   /* Avoid deleting preserve label when redirecting ABNORMAL edges.  */
   if (block_has_preserve_label(self,e1->dest) && (e1->flags & EDGE_ABNORMAL))
      return false;

   /* Here we know that the insns in the end of SRC1 which are common with SRC2
   will be deleted.
   If we have tablejumps in the end of SRC1 and SRC2
   they have been already compared for equivalence in outgoing_edges_match ()
   so replace the references to TABLE1 by references to TABLE2.  */
   {
      rtx_insn *label1, *label2;
      rtx_jump_table_data *table1, *table2;

      if (tablejump_p (BB_END (osrc1), &label1, &table1)
            && tablejump_p (BB_END (osrc2), &label2, &table2)   && label1 != label2){
         rtx_insn *insn;

         /* Replace references to LABEL1 with LABEL2.  */
         for (insn =mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = NEXT_INSN (insn)){
            /* Do not replace the label in SRC1->END because when deleting
            a block whose end is a tablejump, the tablejump referenced
            from the instruction is deleted too.  */
            if (insn != BB_END (osrc1))
               mtcs_rtlanal_replace_label_in_insn/*!replace_label_in_insn*/(mtcsRtlanal,insn, label1, label2, true);
         }
      }
   }

   /* Avoid splitting if possible.  We must always split when SRC2 has
   EH predecessor edges, or we may end up with basic blocks with both
   normal and EH predecessor edges.  */
   if (newpos2 == BB_HEAD (src2)
   && !(EDGE_PRED (src2, 0)->flags & EDGE_EH))
      redirect_to = src2;
   else{
      if (newpos2 == BB_HEAD (src2)){
         /* Skip possible basic block header.  */
         if (LABEL_P (newpos2))
            newpos2 = NEXT_INSN (newpos2);
         while (DEBUG_INSN_P (newpos2))
            newpos2 = NEXT_INSN (newpos2);
         if (NOTE_P (newpos2))
            newpos2 = NEXT_INSN (newpos2);
         while (DEBUG_INSN_P (newpos2))
            newpos2 = NEXT_INSN (newpos2);
      }

      if (dump_file)
         fprintf (dump_file, "Splitting bb %i before %i insns\n",src2->index, nmatch);
      redirect_to = mtcs_cfg_context_split_block/*!split_block*/(mtcsCfgContext,src2, PREV_INSN (newpos2))->dest;
   }

   if (dump_file)
      fprintf (dump_file,"Cross jumping from bb %i to bb %i; %i common insns\n",src1->index, src2->index, nmatch);

   /* We may have some registers visible through the block.  */
   mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,redirect_to);

   if (osrc2 == src2)
      redirect_edges_to = redirect_to;
   else
      redirect_edges_to = osrc2;

   /* Recompute the counts of destinations of outgoing edges.  */
   FOR_EACH_EDGE (s, ei, redirect_edges_to->succs){
      edge s2;
      edge_iterator ei;
      basic_block d = s->dest;

      if (FORWARDER_BLOCK_P (d))
         d = single_succ (d);

      FOR_EACH_EDGE (s2, ei, src1->succs){
         basic_block d2 = s2->dest;
         if (FORWARDER_BLOCK_P (d2))
            d2 = single_succ (d2);
         if (d == d2)
            break;
      }

      /* Take care to update possible forwarder blocks.  We verified
      that there is no more than one in the chain, so we can't run
      into infinite loop.  */
      if (FORWARDER_BLOCK_P (s->dest))
         s->dest->count += s->count ();

      if (FORWARDER_BLOCK_P (s2->dest))
         s2->dest->count -= s->count ();

      s->probability = s->probability.combine_with_count (redirect_edges_to->count,s2->probability, src1->count);
   }

   /* Adjust count for the block.  An earlier jump
   threading pass may have left the profile in an inconsistent
   state (see update_bb_profile_for_threading) so we must be
   prepared for overflows.  */
   tmp = redirect_to;
   do{
      tmp->count += src1->count;
      if (tmp == redirect_edges_to)
         break;
      tmp = find_fallthru_edge (tmp->succs)->dest;
   }while (true);
   mtcs_cfg_rtl_update_br_prob_note/*!update_br_prob_note*/(mtcsCfgRtl,redirect_edges_to);

   /* Edit SRC1 to go to REDIRECT_TO at NEWPOS1.  */

   /* Skip possible basic block header.  */
   if (LABEL_P (newpos1))
      newpos1 = NEXT_INSN (newpos1);

   while (DEBUG_INSN_P (newpos1))
      newpos1 = NEXT_INSN (newpos1);

   if (NOTE_INSN_BASIC_BLOCK_P (newpos1))
      newpos1 = NEXT_INSN (newpos1);

   /* Skip also prologue and function markers.  */
   while (DEBUG_INSN_P (newpos1)
   || (NOTE_P (newpos1)
   && (NOTE_KIND (newpos1) == NOTE_INSN_PROLOGUE_END
   || NOTE_KIND (newpos1) == NOTE_INSN_FUNCTION_BEG)))
      newpos1 = NEXT_INSN (newpos1);

   redirect_from = mtcs_cfg_context_split_block/*!split_block*/(mtcsCfgContext,src1, PREV_INSN (newpos1))->src;
   to_remove = single_succ (redirect_from);

   mtcs_cfg_context_redirect_edge_and_branch_force/*!redirect_edge_and_branch_force*/(mtcsCfgContext,
         single_succ_edge (redirect_from), redirect_to);
   mtcs_cfg_context_delete_basic_block/*!delete_basic_block*/(mtcsCfgContext,to_remove);

   update_forwarder_flag(self,redirect_from);
   if (redirect_to != src2)
      update_forwarder_flag(self,src2);

   return true;
}

/* Search the predecessors of BB for common insn sequences.  When found,
   share code between them by redirecting control flow.  Return true if
   any changes made.  */

static bool try_crossjump_bb (MtcsCfgCleanup *self,int mode, basic_block bb)
{
   edge e, e2, fallthru;
   bool changed;
   unsigned max, ix, ix2;

   /* Nothing to do if there is not at least two incoming edges.  */
   if (EDGE_COUNT (bb->preds) < 2)
      return false;

   /* Don't crossjump if this block ends in a computed jump,
   unless we are optimizing for size.  */
   if (optimize_bb_for_size_p (bb)
   && bb != EXIT_BLOCK_PTR_FOR_FN (cfun)
   && computed_jump_p (BB_END (bb)))
      return false;

   /* If we are partitioning hot/cold basic blocks, we don't want to
   mess up unconditional or indirect jumps that cross between hot
   and cold sections.

   Basic block partitioning may result in some jumps that appear to
   be optimizable (or blocks that appear to be mergeable), but which really
   must be left untouched (they are required to make it safely across
   partition boundaries).  See the comments at the top of
   bb-reorder.cc:partition_hot_cold_basic_blocks for complete details.  */

   if (BB_PARTITION (EDGE_PRED (bb, 0)->src) !=
   BB_PARTITION (EDGE_PRED (bb, 1)->src)
   || (EDGE_PRED (bb, 0)->flags & EDGE_CROSSING))
      return false;

   /* It is always cheapest to redirect a block that ends in a branch to
   a block that falls through into BB, as that adds no branches to the
   program.  We'll try that combination first.  */
   fallthru = NULL;
   max = param_max_crossjump_edges;

   if (EDGE_COUNT (bb->preds) > max)
      return false;

   fallthru = find_fallthru_edge (bb->preds);

   changed = false;
   for (ix = 0; ix < EDGE_COUNT (bb->preds);){
      e = EDGE_PRED (bb, ix);
      ix++;

      /* As noted above, first try with the fallthru predecessor (or, a
      fallthru predecessor if we are in cfglayout mode).  */
      if (fallthru){
         /* Don't combine the fallthru edge into anything else.
         If there is a match, we'll do it the other way around.  */
         if (e == fallthru)
            continue;
         /* If nothing changed since the last attempt, there is nothing
         we can do.  */
         if (!self->first_pass
         && !((e->src->flags & BB_MODIFIED)
         || (fallthru->src->flags & BB_MODIFIED)))
            continue;

         if (try_crossjump_to_edge(self,mode, e, fallthru, dir_forward)){
            changed = true;
            ix = 0;
            continue;
         }
      }

      /* Non-obvious work limiting check: Recognize that we're going
      to call try_crossjump_bb on every basic block.  So if we have
      two blocks with lots of outgoing edges (a switch) and they
      share lots of common destinations, then we would do the
      cross-jump check once for each common destination.

      Now, if the blocks actually are cross-jump candidates, then
      all of their destinations will be shared.  Which means that
      we only need check them for cross-jump candidacy once.  We
      can eliminate redundant checks of crossjump(A,B) by arbitrarily
      choosing to do the check from the block for which the edge
      in question is the first successor of A.  */
      if (EDGE_SUCC (e->src, 0) != e)
         continue;

      for (ix2 = 0; ix2 < EDGE_COUNT (bb->preds); ix2++){
         e2 = EDGE_PRED (bb, ix2);

         if (e2 == e)
            continue;

         /* We've already checked the fallthru edge above.  */
         if (e2 == fallthru)
            continue;

         /* The "first successor" check above only prevents multiple
         checks of crossjump(A,B).  In order to prevent redundant
         checks of crossjump(B,A), require that A be the block
         with the lowest index.  */
         if (e->src->index > e2->src->index)
            continue;

         /* If nothing changed since the last attempt, there is nothing
         we can do.  */
         if (!self->first_pass
         && !((e->src->flags & BB_MODIFIED)
         || (e2->src->flags & BB_MODIFIED)))
            continue;

         /* Both e and e2 are not fallthru edges, so we can crossjump in either
         direction.  */
         if (try_crossjump_to_edge(self,mode, e, e2, dir_both)){
            changed = true;
            ix = 0;
            break;
         }
      }
   }

   if (changed)
      self->crossjumps_occurred = true;

   return changed;
}

/* Search the successors of BB for common insn sequences.  When found,
   share code between them by moving it across the basic block
   boundary.  Return true if any changes made.  */

static bool try_head_merge_bb (MtcsCfgCleanup *self,basic_block bb)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsDfproblems *mtcsDfproblems=mtcs_target_get_dfproblems(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsCfgBuild *mtcsCfgBuild=mtcs_target_get_cfg_build(mtcsTarget);

   basic_block final_dest_bb = NULL;
   int max_match = INT_MAX;
   edge e0;
   rtx_insn **headptr, **currptr, **nextptr;
   bool changed, moveall;
   unsigned ix;
   rtx_insn *e0_last_head;
   rtx cond;
   rtx_insn *move_before;
   unsigned nedges = EDGE_COUNT (bb->succs);
   rtx_insn *jump = BB_END (bb);
   regset live, live_union;

   /* Nothing to do if there is not at least two outgoing edges.  */
   if (nedges < 2)
      return false;

   /* Don't crossjump if this block ends in a computed jump,
   unless we are optimizing for size.  */
   if (optimize_bb_for_size_p (bb)
   && bb != EXIT_BLOCK_PTR_FOR_FN (cfun)
   && computed_jump_p (BB_END (bb)))
      return false;

   cond = mtcs_rtlanal_get_condition/*!get_condition*/(mtcsRtlanal,jump, &move_before, true, false);
   if (cond == NULL_RTX)
      move_before = jump;

   for (ix = 0; ix < nedges; ix++)
      if (EDGE_SUCC (bb, ix)->dest == EXIT_BLOCK_PTR_FOR_FN (cfun))
         return false;

   for (ix = 0; ix < nedges; ix++){
      edge e = EDGE_SUCC (bb, ix);
      basic_block other_bb = e->dest;

      if (mtcs_dfcore_df_get_bb_dirty/*!df_get_bb_dirty*/(mtcsDfcore,other_bb)){
         self->block_was_dirty = true;
         return false;
      }

      if (e->flags & EDGE_ABNORMAL)
         return false;

      /* Normally, all destination blocks must only be reachable from this
      block, i.e. they must have one incoming edge.

      There is one special case we can handle, that of multiple consecutive
      jumps where the first jumps to one of the targets of the second jump.
      This happens frequently in switch statements for default labels.
      The structure is as follows:
      FINAL_DEST_BB
      ....
      if (cond) jump A;
      fall through
      BB
      jump with targets A, B, C, D...
      A
      has two incoming edges, from FINAL_DEST_BB and BB

      In this case, we can try to move the insns through BB and into
      FINAL_DEST_BB.  */
      if (EDGE_COUNT (other_bb->preds) != 1){
         edge incoming_edge, incoming_bb_other_edge;
         edge_iterator ei;

         if (final_dest_bb != NULL || EDGE_COUNT (other_bb->preds) != 2)
            return false;

         /* We must be able to move the insns across the whole block.  */
         move_before = BB_HEAD (bb);
         while (!NONDEBUG_INSN_P (move_before))
            move_before = NEXT_INSN (move_before);

         if (EDGE_COUNT (bb->preds) != 1)
            return false;
         incoming_edge = EDGE_PRED (bb, 0);
         final_dest_bb = incoming_edge->src;
         if (EDGE_COUNT (final_dest_bb->succs) != 2)
            return false;
         FOR_EACH_EDGE (incoming_bb_other_edge, ei, final_dest_bb->succs)
            if (incoming_bb_other_edge != incoming_edge)
               break;
         if (incoming_bb_other_edge->dest != other_bb)
            return false;
      }
   }

   e0 = EDGE_SUCC (bb, 0);
   e0_last_head = NULL;
   changed = false;

   for (ix = 1; ix < nedges; ix++){
      edge e = EDGE_SUCC (bb, ix);
      rtx_insn *e0_last, *e_last;
      int nmatch;

      nmatch = mtcs_cfg_cleanup_flow_find_head_matching_sequence/*!flow_find_head_matching_sequence*/
            (self,e0->dest, e->dest,&e0_last, &e_last, 0);
      if (nmatch == 0)
         return false;

      if (nmatch < max_match){
         max_match = nmatch;
         e0_last_head = e0_last;
      }
   }

   /* If we matched an entire block, we probably have to avoid moving the
   last insn.  */
   if (max_match > 0
   && e0_last_head == BB_END (e0->dest)
   && (find_reg_note (e0_last_head, REG_EH_REGION, 0)
   || mtcs_cfg_build_control_flow_insn_p/*!control_flow_insn_p*/(mtcsCfgBuild,e0_last_head))){
      max_match--;
      if (max_match == 0)
         return false;
      e0_last_head = prev_real_nondebug_insn (e0_last_head);
   }

   if (max_match == 0)
      return false;

   /* We must find a union of the live registers at each of the end points.  */
   live = BITMAP_ALLOC (NULL);
   live_union = BITMAP_ALLOC (NULL);

   currptr = XNEWVEC (rtx_insn *, nedges);
   headptr = XNEWVEC (rtx_insn *, nedges);
   nextptr = XNEWVEC (rtx_insn *, nedges);

   for (ix = 0; ix < nedges; ix++){
      int j;
      basic_block merge_bb = EDGE_SUCC (bb, ix)->dest;
      rtx_insn *head = BB_HEAD (merge_bb);

      while (!NONDEBUG_INSN_P (head))
         head = NEXT_INSN (head);
      headptr[ix] = head;
      currptr[ix] = head;

      /* Compute the end point and live information  */
      for (j = 1; j < max_match; j++)
         do
            head = NEXT_INSN (head);
         while (!NONDEBUG_INSN_P (head));
      mtcs_dfproblems_simulate_backwards_to_point/*!simulate_backwards_to_point*/(mtcsDfproblems,merge_bb, live, head);
      IOR_REG_SET (live_union, live);
   }

   /* If we're moving across two blocks, verify the validity of the
   first move, then adjust the target and let the loop below deal
   with the final move.  */
   if (final_dest_bb != NULL){
      rtx_insn *move_upto;

      moveall = mtcs_dfproblems_can_move_insns_across/*!can_move_insns_across*/(mtcsDfproblems,
            currptr[0], e0_last_head, move_before,jump, e0->dest, live_union, NULL, &move_upto);
      if (!moveall){
         if (move_upto == NULL_RTX)
            goto out;

         while (e0_last_head != move_upto){
            mtcs_dfproblems_df_simulate_one_insn_backwards/*!df_simulate_one_insn_backwards*/(mtcsDfproblems,
                  e0->dest, e0_last_head,live_union);
            e0_last_head = PREV_INSN (e0_last_head);
         }
      }
      if (e0_last_head == NULL_RTX)
         goto out;

      jump = BB_END (final_dest_bb);
      cond = mtcs_rtlanal_get_condition/*!get_condition*/(mtcsRtlanal,jump, &move_before, true, false);
      if (cond == NULL_RTX)
         move_before = jump;
   }

   do{
      rtx_insn *move_upto;
      moveall = mtcs_dfproblems_can_move_insns_across/*!can_move_insns_across*/(mtcsDfproblems,
            currptr[0], e0_last_head,move_before, jump, e0->dest, live_union, NULL, &move_upto);
      if (!moveall && move_upto == NULL_RTX){
         if (jump == move_before)
            break;

         /* Try again, using a different insertion point.  */
         move_before = jump;

         continue;
      }

      if (final_dest_bb && !moveall)
         /* We haven't checked whether a partial move would be OK for the first
         move, so we have to fail this case.  */
         break;

      changed = true;
      for (;;){
         if (currptr[0] == move_upto)
            break;
         for (ix = 0; ix < nedges; ix++){
            rtx_insn *curr = currptr[ix];
            do
               curr = NEXT_INSN (curr);
            while (!NONDEBUG_INSN_P (curr));
            currptr[ix] = curr;
         }
      }

      /* If we can't currently move all of the identical insns, remember
      each insn after the range that we'll merge.  */
      if (!moveall)
         for (ix = 0; ix < nedges; ix++){
            rtx_insn *curr = currptr[ix];
            do
               curr = NEXT_INSN (curr);
            while (!NONDEBUG_INSN_P (curr));
            nextptr[ix] = curr;
         }

      mtcs_rtl_reorder_insns/*!reorder_insns*/(mtcsRTL,headptr[0], currptr[0], PREV_INSN (move_before));
      mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,EDGE_SUCC (bb, 0)->dest);
      if (final_dest_bb != NULL)
         mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,final_dest_bb);
      mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,bb);
      for (ix = 1; ix < nedges; ix++){
         mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,EDGE_SUCC (bb, ix)->dest);
         delete_insn_chain (headptr[ix], currptr[ix], false);
      }
      if (!moveall){
         if (jump == move_before)
            break;

         /* For the unmerged insns, try a different insertion point.  */
         move_before = jump;

         for (ix = 0; ix < nedges; ix++)
            currptr[ix] = headptr[ix] = nextptr[ix];
      }
   }while (!moveall);

out:
   free (currptr);
   free (headptr);
   free (nextptr);

   self->crossjumps_occurred |= changed;

   return changed;
}

/* Return true if BB contains just bb note, or bb note followed
   by only DEBUG_INSNs.  */

static bool trivially_empty_bb_p (basic_block bb)
{
   rtx_insn *insn = BB_END (bb);

   while (1){
      if (insn == BB_HEAD (bb))
         return true;
      if (!DEBUG_INSN_P (insn))
         return false;
      insn = PREV_INSN (insn);
   }
}

/* Return true if BB contains just a return and possibly a USE of the
   return value.  Fill in *RET and *USE with the return and use insns
   if any found, otherwise NULL.  All CLOBBERs are ignored.  */
//原型 bb_is_just_return cfgcleanup.h cfgcleanup.cc
bool mtcs_cfg_cleanup_bb_is_just_return (MtcsCfgCleanup *self,basic_block bb, rtx_insn **ret, rtx_insn **use)
{
   *ret = *use = NULL;
   rtx_insn *insn;

   if (bb == EXIT_BLOCK_PTR_FOR_FN (cfun))
      return false;

   FOR_BB_INSNS_REVERSE (bb, insn)
      if (NONDEBUG_INSN_P (insn)){
         rtx pat = PATTERN (insn);

         if (!*ret && ANY_RETURN_P (pat))
            *ret = insn;
         else if (*ret && !*use && GET_CODE (pat) == USE
         && REG_P (XEXP (pat, 0))
         && REG_FUNCTION_VALUE_P (XEXP (pat, 0)))
            *use = insn;
         else if (GET_CODE (pat) != CLOBBER)
            return false;
      }

   return !!*ret;
}



/* Do simple CFG optimizations - basic block merging, simplifying of jump
   instructions etc.  Return nonzero if changes were made.  */
static bool try_optimize_cfg (MtcsCfgCleanup *self,int mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsCfg *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   bool changed_overall = false;
   bool changed;
   int iterations = 0;
   basic_block bb, b, next;

   if (mode & (CLEANUP_CROSSJUMP | CLEANUP_THREADING))
      clear_bb_flags ();

   self->crossjumps_occurred = false;

   FOR_EACH_BB_FN (bb, cfun)
      update_forwarder_flag(self,bb);

   if (! mtcsTarget/*!targetm.cannot_modify_jumps_p*/->cannot_modify_jumps_p(mtcsTarget)){
      self->first_pass = true;
      /* Attempt to merge blocks as made possible by edge removal.  If
      a block has only one successor, and the successor has only
      one predecessor, they may be combined.  */
      do{
         self->block_was_dirty = false;
         changed = false;
         iterations++;
         n_debug("mtcscfgcleanup.c  try_optimize_cfg 00 iterations:%d mode:%d changed:%d\n",iterations,mode,changed);

         if (dump_file)
            fprintf (dump_file,"\n\ntry_optimize_cfg iteration %i\n\n",iterations);

         for (b = ENTRY_BLOCK_PTR_FOR_FN (cfun)->next_bb; b != EXIT_BLOCK_PTR_FOR_FN (cfun);){
            basic_block c;
            edge s;
            bool changed_here = false;
            n_debug("mtcscfgcleanup.c  try_optimize_cfg 11 mode:%d b:%p ENTRY_BB:%p changed:%d\n",
                  mode,b,ENTRY_BLOCK_PTR_FOR_FN (cfun),changed);

            /* Delete trivially dead basic blocks.  This is either
            blocks with no predecessors, or empty blocks with no
            successors.  However if the empty block with no
            successors is the successor of the ENTRY_BLOCK, it is
            kept.  This ensures that the ENTRY_BLOCK will have a
            successor which is a precondition for many RTL
            passes.  Empty blocks may result from expanding
            __builtin_unreachable ().  */
            if (EDGE_COUNT (b->preds) == 0 || (EDGE_COUNT (b->succs) == 0
            && trivially_empty_bb_p (b) && single_succ_edge (ENTRY_BLOCK_PTR_FOR_FN (cfun))->dest != b)){
               c = b->prev_bb;
               n_debug("mtcscfgcleanup.c  try_optimize_cfg 22 mode:%d b:%p c:%p EDGE_COUNT (b->preds):%d\n",
                          mode,b,c,EDGE_COUNT (b->preds));
               if (EDGE_COUNT (b->preds) > 0){
                  edge e;
                  edge_iterator ei;
                  n_debug("mtcscfgcleanup.c  try_optimize_cfg 33 mode:%d b:%p c:%p \n",mode,b,c);

                  if (mtcs_cfg_context_get_state/*!current_ir_type ()*/(mtcsCfgContext) == IR_RTL_CFGLAYOUT){
                     n_debug("mtcscfgcleanup.c  try_optimize_cfg 44 mode:%d b:%p c:%p \n",mode,b,c);
                     rtx_insn *insn;
                     for (insn = BB_FOOTER (b);insn; insn = NEXT_INSN (insn))
                        if (BARRIER_P (insn))
                           break;
                     if (insn)
                        FOR_EACH_EDGE (e, ei, b->preds)
                           if ((e->flags & EDGE_FALLTHRU)){
                              if (BB_FOOTER (b) && BB_FOOTER (e->src) == NULL){
                                 BB_FOOTER (e->src) = BB_FOOTER (b);
                                 BB_FOOTER (b) = NULL;
                              }else
                                 mtcs_cfg_rtl_emit_barrier_after_bb/*!emit_barrier_after_bb*/(mtcsCfgRtl,e->src);
                           }
                  }else{
                     n_debug("mtcscfgcleanup.c  try_optimize_cfg 55 mode:%d b:%p c:%p \n",mode,b,c);
                     rtx_insn *last = get_last_bb_insn (b);
                     if (last && BARRIER_P (last))
                        FOR_EACH_EDGE (e, ei, b->preds)
                           if ((e->flags & EDGE_FALLTHRU))
                              mtcs_emit_emit_barrier_after/*!emit_barrier_after*/(mtcsEmit,BB_END (e->src));
                  }
               }
               mtcs_cfg_context_delete_basic_block/*!delete_basic_block*/(mtcsCfgContext,b);
               changed = true;
               /* Avoid trying to remove the exit block.  */
               b = (c == ENTRY_BLOCK_PTR_FOR_FN (cfun) ? c->next_bb : c);
               n_debug("mtcscfgcleanup.c  try_optimize_cfg 66 mode:%d b:%p c:%p \n",mode,b,c);
               continue;
            }
            //n_debug("mtcscfgcleanup.c  try_optimize_cfg 66 mode:%d b:%p c:%p \n",mode,b,c);

            /* Remove code labels no longer used.  */
            if (single_pred_p (b)
            && (single_pred_edge (b)->flags & EDGE_FALLTHRU)
            && !(single_pred_edge (b)->flags & EDGE_COMPLEX)
            && LABEL_P (BB_HEAD (b))
            && !LABEL_PRESERVE_P (BB_HEAD (b))
            /* If the previous block ends with a branch to this
            block, we can't delete the label.  Normally this
            is a condjump that is yet to be simplified, but
            if CASE_DROPS_THRU, this can be a tablejump with
            some element going to the same place as the
            default (fallthru).  */
            && (single_pred (b) == ENTRY_BLOCK_PTR_FOR_FN (cfun)
            || !JUMP_P (BB_END (single_pred (b)))
            || ! label_is_jump_target_p (BB_HEAD (b),
            BB_END (single_pred (b))))){
               n_debug("mtcscfgcleanup.c  try_optimize_cfg 77 mode:%d b:%p c:%p \n",mode,b,c);
               mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,BB_HEAD (b));
               if (dump_file)
                  fprintf (dump_file, "Deleted label in block %i.\n",b->index);
            }
            n_debug("mtcscfgcleanup.c  try_optimize_cfg 88 mode:%d b:%p c:%p \n",mode,b,c);
            /* If we fall through an empty block, we can remove it.  */
            if (!(mode & (CLEANUP_CFGLAYOUT | CLEANUP_NO_INSN_DEL))
            && single_pred_p (b)
            && (single_pred_edge (b)->flags & EDGE_FALLTHRU)
            && !LABEL_P (BB_HEAD (b))
            && FORWARDER_BLOCK_P (b)
            /* Note that forwarder_block_p true ensures that
            there is a successor for this block.  */
            && (single_succ_edge (b)->flags & EDGE_FALLTHRU)
            && n_basic_blocks_for_fn (cfun) > NUM_FIXED_BLOCKS + 1){
               n_debug("mtcscfgcleanup.c  try_optimize_cfg 99 mode:%d b:%p c:%p \n",mode,b,c);

               if (dump_file)
                  fprintf (dump_file,"Deleting fallthru block %i.\n", b->index);

               c = ((b->prev_bb == ENTRY_BLOCK_PTR_FOR_FN (cfun))? b->next_bb : b->prev_bb);
               mtcs_cfg_context_redirect_edge_succ_nodup/*!redirect_edge_succ_nodup*/(mtcsCfgContext,
                     single_pred_edge (b),single_succ (b));
               mtcs_cfg_context_delete_basic_block/*!delete_basic_block*/(mtcsCfgContext,b);
               changed = true;
               b = c;
               continue;
            }
            n_debug("mtcscfgcleanup.c  try_optimize_cfg 100 mode:%d b:%p c:%p edge_count:%d\n",mode,b,c,EDGE_COUNT (b->succs));
            /* Merge B with its single successor, if any.  */
            if (single_succ_p (b)
            && (s = single_succ_edge (b))
            && !(s->flags & EDGE_COMPLEX)
            && (c = s->dest) != EXIT_BLOCK_PTR_FOR_FN (cfun)
            && single_pred_p (c)
            && b != c){
               /* When not in cfg_layout mode use code aware of reordering
               INSN.  This code possibly creates new basic blocks so it
               does not fit merge_blocks interface and is kept here in
               hope that it will become useless once more of compiler
               is transformed to use cfg_layout mode.  */
               n_debug("mtcscfgcleanup.c  try_optimize_cfg 101 mode:%d b:%p c:%p \n",mode,b,c);

               if ((mode & CLEANUP_CFGLAYOUT)
               && mtcs_cfg_context_can_merge_blocks_p/*!can_merge_blocks_p*/(mtcsCfgContext,b, c)){
                  n_debug("mtcscfgcleanup.c  try_optimize_cfg 102 mode:%d b:%p c:%p \n",mode,b,c);

                  mtcs_cfg_context_merge_blocks/*!merge_blocks*/(mtcsCfgContext,b, c);
                  update_forwarder_flag(self,b);
                  changed_here = true;
               }else if (!(mode & CLEANUP_CFGLAYOUT)
               /* If the jump insn has side effects,
               we can't kill the edge.  */
               && (!JUMP_P (BB_END (b))
               || (reload_completed
               ? simplejump_p (BB_END (b))
               : (onlyjump_p (BB_END (b))
               && !tablejump_p (BB_END (b),
               NULL, NULL))))
               && (next = merge_blocks_move(self,s, b, c, mode))){
                  n_debug("mtcscfgcleanup.c  try_optimize_cfg 103 mode:%d b:%p c:%p \n",mode,b,c);
                  printbb(b);
                  n_debug("mtcscfgcleanup.c  try_optimize_cfg 103aa 打印c mode:%d b:%p c:%p \n",mode,b,c);
                  printbb(c);
                  b = next;
                  changed_here = true;
               }
            }
            n_debug("mtcscfgcleanup.c  try_optimize_cfg 104 mode:%d b:%p c:%p \n",mode,b,c);
            printbb(b);

            /* Try to change a branch to a return to just that return.  */
            rtx_insn *ret, *use;
            if (single_succ_p (b)
            && onlyjump_p (BB_END (b))
            && mtcs_cfg_cleanup_bb_is_just_return/*!bb_is_just_return*/(self,single_succ (b), &ret, &use)){
               if (mtcs_dojump_redirect_jump/*!redirect_jump*/(mtcsDojump,as_a <rtx_jump_insn *> (BB_END (b)),PATTERN (ret), 0)){
                  if (use)
                     mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,copy_insn (PATTERN (use)),
                  BB_END (b));
                  if (dump_file)
                     fprintf (dump_file, "Changed jump %d->%d to return.\n",b->index, single_succ (b)->index);
                  mtcs_cfg_redirect_edge_succ/*!redirect_edge_succ*/(mtcsCfg,
                        single_succ_edge (b), EXIT_BLOCK_PTR_FOR_FN (cfun));
                  single_succ_edge (b)->flags &= ~EDGE_CROSSING;
                  changed_here = true;
               }
            }
            n_debug("mtcscfgcleanup.c  try_optimize_cfg 105 mode:%d b:%p c:%p \n",mode,b,c);
            printbb(b);

            /* Try to change a conditional branch to a return to the
            respective conditional return.  */
            if (EDGE_COUNT (b->succs) == 2
            && any_condjump_p (BB_END (b))
            && mtcs_cfg_cleanup_bb_is_just_return/*!bb_is_just_return*/(self,BRANCH_EDGE (b)->dest, &ret, &use)){
               if (mtcs_dojump_redirect_jump/*!redirect_jump*/(mtcsDojump,as_a <rtx_jump_insn *> (BB_END (b)),PATTERN (ret), 0)){
                  if (use)
                     mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,copy_insn (PATTERN (use)),BB_END (b));
                  if (dump_file)
                     fprintf (dump_file, "Changed conditional jump %d->%d to conditional return.\n",EXIT_BLOCK_PTR_FOR_FN (cfun));
                  BRANCH_EDGE (b)->flags &= ~EDGE_CROSSING;
                  changed_here = true;
               }
            }
            n_debug("mtcscfgcleanup.c  try_optimize_cfg 106 mode:%d b:%p c:%p \n",mode,b,c);
            printbb(b);

            /* Try to flip a conditional branch that falls through to
            a return so that it becomes a conditional return and a
            new jump to the original branch target.  */
            if (EDGE_COUNT (b->succs) == 2
            && BRANCH_EDGE (b)->dest != EXIT_BLOCK_PTR_FOR_FN (cfun)
            && any_condjump_p (BB_END (b))
            && mtcs_cfg_cleanup_bb_is_just_return/*!bb_is_just_return*/(self,FALLTHRU_EDGE (b)->dest, &ret, &use)){
               if (mtcs_dojump_invert_jump/*!invert_jump*/(mtcsDojump,
                     as_a <rtx_jump_insn *> (BB_END (b)),JUMP_LABEL (BB_END (b)), 0)){
                  basic_block new_ft = BRANCH_EDGE (b)->dest;
                  if (mtcs_dojump_redirect_jump/*!redirect_jump*/(mtcsDojump,as_a <rtx_jump_insn *> (BB_END (b)),PATTERN (ret), 0)){
                     if (use)
                        mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,copy_insn (PATTERN (use)),BB_END (b));
                     if (dump_file)
                        fprintf (dump_file, "Changed conditional jump "
                        "%d->%d to conditional return, adding "
                        "fall-through jump.\n",
                        b->index, BRANCH_EDGE (b)->dest->index);
                     mtcs_cfg_redirect_edge_succ/*!redirect_edge_succ*/(mtcsCfg,BRANCH_EDGE (b),EXIT_BLOCK_PTR_FOR_FN (cfun));
                     BRANCH_EDGE (b)->flags &= ~EDGE_CROSSING;
                     std::swap (BRANCH_EDGE (b)->probability,FALLTHRU_EDGE (b)->probability);
                     mtcs_cfg_rtl_update_br_prob_note/*!update_br_prob_note*/(mtcsCfgRtl,b);
                     basic_block jb = mtcs_cfg_context_force_nonfallthru/*!force_nonfallthru*/(mtcsCfgContext,FALLTHRU_EDGE (b));
                     notice_new_block(self,jb);
                     if (!mtcs_dojump_redirect_jump/*!redirect_jump*/(mtcsDojump,as_a <rtx_jump_insn *> (BB_END (jb)),
                                     mtcs_cfg_rtl_block_label/*!block_label*/(mtcsCfgRtl,new_ft), 0))
                        gcc_unreachable ();
                     mtcs_cfg_redirect_edge_succ/*!redirect_edge_succ*/(mtcsCfg,single_succ_edge (jb), new_ft);
                     changed_here = true;
                  }else{
                     /* Invert the jump back to what it was.  This should
                     never fail.  */
                     if (!mtcs_dojump_invert_jump/*!invert_jump*/(mtcsDojump,
                           as_a <rtx_jump_insn *> (BB_END (b)),JUMP_LABEL (BB_END (b)), 0))
                        gcc_unreachable ();
                  }
               }
            }
            n_debug("mtcscfgcleanup.c try_optimize_cfg 107 mode:%d b:%p c:%p \n",mode,b,c);
            printbb(b);

            /* Simplify branch over branch.  */
            if ((mode & CLEANUP_EXPENSIVE) && !(mode & CLEANUP_CFGLAYOUT) && try_simplify_condjump(self,b))
               changed_here = true;

            /* If B has a single outgoing edge, but uses a
            non-trivial jump instruction without side-effects, we
            can either delete the jump entirely, or replace it
            with a simple unconditional jump.  */
            if (single_succ_p (b)
            && single_succ (b) != EXIT_BLOCK_PTR_FOR_FN (cfun)
            && onlyjump_p (BB_END (b))
            && !CROSSING_JUMP_P (BB_END (b))
            && mtcs_cfg_rtl_try_redirect_by_replacing_jump/*!try_redirect_by_replacing_jump*/(mtcsCfgRtl,
                  single_succ_edge (b),single_succ (b),(mode & CLEANUP_CFGLAYOUT) != 0)){
               n_debug("mtcscfgcleanup.c  try_optimize_cfg 108 mode:%d b:%p c:%p \n",mode,b,c);
               update_forwarder_flag(self,b);
               changed_here = true;
            }
            n_debug("mtcscfgcleanup.c try_optimize_cfg 107xx mode:%d b:%p c:%p \n",mode,b,c);
                 printbb(b);
            /* Simplify branch to branch.  */
            if (try_forward_edges(self,mode, b)){
               n_debug("mtcscfgcleanup.c  try_optimize_cfg 109 mode:%d b:%p c:%p \n",mode,b,c);
               printbb(b);

               update_forwarder_flag(self,b);

               changed_here = true;
            }

            /* Look for shared code between blocks.  */
            if ((mode & CLEANUP_CROSSJUMP) && try_crossjump_bb(self,mode, b))
               changed_here = true;

            if ((mode & CLEANUP_CROSSJUMP)
            /* This can lengthen register lifetimes.  Do it only after
            reload.  */
            && reload_completed  && try_head_merge_bb(self,b)){
               changed_here = true;
            n_debug("mtcscfgcleanup.c  try_optimize_cfg 110 mode:%d b:%p c:%p changed_here:%d b:%p changed:%d\n",
                  mode,b,c,changed_here,b,changed);

            }

            /* Don't get confused by the index shift caused by
            deleting blocks.  */
            if (!changed_here)
               b = b->next_bb;
            else
               changed = true;
            n_debug("mtcscfgcleanup.c  try_optimize_cfg 111 mode:%d b:%p c:%p changed_here:%d b:%p changed:%d\n",
                  mode,b,c,changed_here,b,changed);
            printbb(b);


         }
         n_debug("mtcscfgcleanup.c  try_optimize_cfg 112 (mode & CLEANUP_CROSSJUMP):%d changed:%d\n",
               (mode & CLEANUP_CROSSJUMP),changed);
         printbb(b);
         test_rtl_verify_edges();

         if ((mode & CLEANUP_CROSSJUMP) && try_crossjump_bb(self,mode, EXIT_BLOCK_PTR_FOR_FN (cfun))){
            changed = true;
            n_debug("mtcscfgcleanup.c  try_optimize_cfg 113 self->block_was_dirty:%d\n",self->block_was_dirty);
         }
         if (self->block_was_dirty){
            n_debug("mtcscfgcleanup.c  try_optimize_cfg 114 self->block_was_dirty:%d changed:%d\n",self->block_was_dirty,changed);
            /* This should only be set by head-merging.  */
            gcc_assert (mode & CLEANUP_CROSSJUMP);
            mtcs_dfcore_df_analyze/*!df_analyze*/(mtcsDfcore);
         }

         if (changed){
            /* Edge forwarding in particular can cause hot blocks previously
            reached by both hot and cold blocks to become dominated only
            by cold blocks. This will cause the verification below to fail,
            and lead to now cold code in the hot section. This is not easy
            to detect and fix during edge forwarding, and in some cases
            is only visible after newly unreachable blocks are deleted,
            which will be done in fixup_partitions.  */
            if ((mode & CLEANUP_NO_PARTITIONING) == 0){
               n_debug("mtcscfgcleanup.c  try_optimize_cfg 115 self->block_was_dirty:%d changed:%d\n",self->block_was_dirty,changed);
               test_rtl_verify_edges();
               mtcs_cfg_rtl_fixup_partitions/*!fixup_partitions*/(mtcsCfgRtl);
               test_rtl_verify_edges();
               mtcs_cfg_context_checking_verify_flow_info/*!checking_verify_flow_info*/(mtcsCfgContext);
            }
         }

         changed_overall |= changed;
         self->first_pass = false;
      }while (changed);
   }

   FOR_ALL_BB_FN (b, cfun)
      b->flags &= ~(BB_FORWARDER_BLOCK | BB_NONTHREADABLE_BLOCK);

   return changed_overall;
}

/* Delete all unreachable basic blocks.  */
//原型 delete_unreachable_blocks cfgcleanup.h cfgcleanup.cc
bool mtcs_cfg_cleanup_delete_unreachable_blocks (MtcsCfgCleanup *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);

   bool changed = false;
   basic_block b, prev_bb;
   n_debug("mtcscfgcleanup.c mtcs_cfg_cleanup_delete_unreachable_blocks 00 \n");

   find_unreachable_blocks ();

   /* When we're in GIMPLE mode and there may be debug bind insns, we
   should delete blocks in reverse dominator order, so as to get a
   chance to substitute all released DEFs into debug bind stmts.  If
   we don't have dominators information, walking blocks backward
   gets us a better chance of retaining most debug information than
   otherwise.  */
   if (MAY_HAVE_DEBUG_BIND_INSNS && mtcs_cfg_context_get_state/*!current_ir_type ()*/(mtcsCfgContext) == IR_GIMPLE
         && dom_info_available_p (CDI_DOMINATORS)){
      for (b = EXIT_BLOCK_PTR_FOR_FN (cfun)->prev_bb; b != ENTRY_BLOCK_PTR_FOR_FN (cfun); b = prev_bb){
         prev_bb = b->prev_bb;

         if (!(b->flags & BB_REACHABLE)){
            /* Speed up the removal of blocks that don't dominate
            others.  Walking backwards, this should be the common
            case.  */
            if (!first_dom_son (CDI_DOMINATORS, b))
               mtcs_cfg_context_delete_basic_block/*!delete_basic_block*/(mtcsCfgContext,b);
            else{
               auto_vec<basic_block> h = get_all_dominated_blocks (CDI_DOMINATORS, b);

               while (h.length ()){
                  b = h.pop ();

                  prev_bb = b->prev_bb;

                  gcc_assert (!(b->flags & BB_REACHABLE));

                  mtcs_cfg_context_delete_basic_block/*!delete_basic_block*/(mtcsCfgContext,b);
               }
            }

            changed = true;
         }
      }
   }else{
      for (b = EXIT_BLOCK_PTR_FOR_FN (cfun)->prev_bb;b != ENTRY_BLOCK_PTR_FOR_FN (cfun); b = prev_bb){
         prev_bb = b->prev_bb;

         if (!(b->flags & BB_REACHABLE)){
            n_debug("mtcscfgcleanup.c mtcs_cfg_cleanup_delete_unreachable_blocks 11 bb:%p index:%d\n",b,b->index);
            mtcs_cfg_context_delete_basic_block/*!delete_basic_block*/(mtcsCfgContext,b);
            changed = true;
         }
      }
   }

   if (changed)
      mtcs_cfg_context_tidy_fallthru_edges/*!tidy_fallthru_edges*/(mtcsCfgContext);
   return changed;
}

/* Delete any jump tables never referenced.  We can't delete them at the
   time of removing tablejump insn as they are referenced by the preceding
   insns computing the destination, so we delay deleting and garbagecollect
   them once life information is computed.  */
//原型 delete_dead_jumptables cfgcleanup.h cfgcleanup.cc
void mtcs_cfg_cleanup_delete_dead_jumptables (MtcsCfgCleanup *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);

   basic_block bb;

   /* A dead jump table does not belong to any basic block.  Scan insns
   between two adjacent basic blocks.  */
   FOR_EACH_BB_FN (bb, cfun){
      rtx_insn *insn, *next;

      for (insn = NEXT_INSN (BB_END (bb)); insn && !NOTE_INSN_BASIC_BLOCK_P (insn); insn = next){
         next = NEXT_INSN (insn);
         if (LABEL_P (insn) && LABEL_NUSES (insn) == LABEL_PRESERVE_P (insn)  && JUMP_TABLE_DATA_P (next)){
            rtx_insn *label = insn, *jump = next;

            if (dump_file)
               fprintf (dump_file, "Dead jumptable %i removed\n",INSN_UID (insn));

            next = NEXT_INSN (next);
            mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,jump);
            mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,label);
         }
      }
   }
}


/* Tidy the CFG by deleting unreachable code and whatnot.  */
//原型 cleanup_cfg cfgcleanup.h cfgcleanup.cc
bool mtcs_cfg_cleanup_cleanup_cfg (MtcsCfgCleanup *self,int mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsCse *mtcsCse=mtcs_target_get_cse(mtcsTarget);
   MtcsDce *mtcsDce=mtcs_target_get_dce(mtcsTarget);
   MtcsCfg *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   bool changed = false;

   /* Set the cfglayout mode flag here.  We could update all the callers
   but that is just inconvenient, especially given that we eventually
   want to have cfglayout mode as the default.  */
   if (mtcs_cfg_context_get_state/*!current_ir_type ()*/(mtcsCfgContext) == IR_RTL_CFGLAYOUT){
      mode |= CLEANUP_CFGLAYOUT;
      n_debug("mtcscfgcleanup.c mtcs_cfg_cleanup_cleanup_cfg 00 mode:%d\n",mode);
   }

   if (mtcs_cfg_cleanup_delete_unreachable_blocks/*!delete_unreachable_blocks*/(self)){
      changed = true;
      n_debug("mtcscfgcleanup.c mtcs_cfg_cleanup_cleanup_cfg 11 mode:%d reload_completed:%d\n",mode,reload_completed);

      /* We've possibly created trivially dead code.  Cleanup it right
      now to introduce more opportunities for try_optimize_cfg.  */
      if (!(mode & (CLEANUP_NO_INSN_DEL)) && !reload_completed){
         n_debug("mtcscfgcleanup.c mtcs_cfg_cleanup_cleanup_cfg 22 mode:%d reload_completed:%d\n",mode,reload_completed);
         mtcs_cse_delete_trivially_dead_insns/*!delete_trivially_dead_insns*/(mtcsCse,
               mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData), mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc));
      }
   }
   n_debug("mtcscfgcleanup.c mtcs_cfg_cleanup_cleanup_cfg 00AA mode:%d\n",mode);
   printbb(NULL);

   mtcs_cfg_compact_blocks/*!compact_blocks*/(mtcsCfg);
   n_debug("mtcscfgcleanup.c mtcs_cfg_cleanup_cleanup_cfg 00BB mode:%d\n",mode);
   printbb(NULL);
   /* To tail-merge blocks ending in the same noreturn function (e.g.
   a call to abort) we have to insert fake edges to exit.  Do this
   here once.  The fake edges do not interfere with any other CFG
   cleanups.  */
   if (mode & CLEANUP_CROSSJUMP)
      add_noreturn_fake_exit_edges ();

   if (!dbg_cnt (cfg_cleanup))
      return changed;
   //n_debug("mtcscfgcleanup.c mtcs_cfg_cleanup_cleanup_cfg 00BBCCC mode:%d\n",mode);

   //printbb(NULL);
   while (try_optimize_cfg(self,mode)){
      n_debug("mtcscfgcleanup.c mtcs_cfg_cleanup_cleanup_cfg 33\n");
     // printbb(NULL);
      mtcs_cfg_cleanup_delete_unreachable_blocks/*!delete_unreachable_blocks*/(self), changed = true;
     // n_debug("mtcscfgcleanup.c mtcs_cfg_cleanup_cleanup_cfg mtcs_cfg_cleanup_delete_unreachable_blocks 33aa\n");

      //printbb(NULL);

      if (!(mode & CLEANUP_NO_INSN_DEL)){
         n_debug("mtcscfgcleanup.c mtcs_cfg_cleanup_cleanup_cfg 44\n");

         /* Try to remove some trivially dead insns when doing an expensive
         cleanup.  But delete_trivially_dead_insns doesn't work after
         reload (it only handles pseudos) and run_fast_dce is too costly
         to run in every iteration.

         For effective cross jumping, we really want to run a fast DCE to
         clean up any dead conditions, or they get in the way of performing
         useful tail merges.

         Other transformations in cleanup_cfg are not so sensitive to dead
         code, so delete_trivially_dead_insns or even doing nothing at all
         is good enough.  */
         if ((mode & CLEANUP_EXPENSIVE) && !reload_completed
         && ! mtcs_cse_delete_trivially_dead_insns/*!delete_trivially_dead_insns*/(mtcsCse,
               mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData), mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc)))
            break;
         n_debug("mtcscfgcleanup.c mtcs_cfg_cleanup_cleanup_cfg 55 self->crossjumps_occurred:%d\n",self->crossjumps_occurred);

         if ((mode & CLEANUP_CROSSJUMP) && self->crossjumps_occurred){
            mtcs_dce_run_fast_dce/*!run_fast_dce*/(mtcsDce);
            mode &= ~CLEANUP_FORCE_FAST_DCE;
         }
      }else
         break;
   }

   if (mode & CLEANUP_CROSSJUMP){
      n_debug("mtcscfgcleanup.c mtcs_cfg_cleanup_cleanup_cfg 66 \n");
      remove_fake_exit_edges ();
   }
   if (mode & CLEANUP_FORCE_FAST_DCE){
      n_debug("mtcscfgcleanup.c mtcs_cfg_cleanup_cleanup_cfg 77 \n");
      mtcs_dce_run_fast_dce/*!run_fast_dce*/(mtcsDce);
   }
   /* Don't call delete_dead_jumptables in cfglayout mode, because
   that function assumes that jump tables are in the insns stream.
   But we also don't _have_ to delete dead jumptables in cfglayout
   mode because we shouldn't even be looking at things that are
   not in a basic block.  Dead jumptables are cleaned up when
   going out of cfglayout mode.  */
   if (!(mode & CLEANUP_CFGLAYOUT)){
      n_debug("mtcscfgcleanup.c mtcs_cfg_cleanup_cleanup_cfg 88 \n");
      mtcs_cfg_cleanup_delete_dead_jumptables/*!delete_dead_jumptables*/(self);
   }
   /* ???  We probably do this way too often.  */
   if (current_loops && (changed || (mode & CLEANUP_CFG_CHANGED))){
      n_debug("mtcscfgcleanup.c mtcs_cfg_cleanup_cleanup_cfg 99 \n");
      /* The above doesn't preserve dominance info if available.  */
      gcc_assert (!dom_info_available_p (CDI_DOMINATORS));
      calculate_dominance_info (CDI_DOMINATORS);
      fix_loop_structure (NULL);
      free_dominance_info (CDI_DOMINATORS);
   }

   return changed;
}

static void mtcsCfgCleanupInit(MtcsCfgCleanup *self)
{

}


MtcsCfgCleanup *mtcs_cfg_cleanup_new(MtcsMode *mtcsMode)
{
      MtcsCfgCleanup *self = n_slice_alloc0 (sizeof(MtcsCfgCleanup));
      mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
      mtcsCfgCleanupInit(self);
      return self;
}




//原型 NEXT_PASS (pass_jump, 1);    RTL_PASS   cfgcleanup.cc   jump   y  无条件执行 ...cleanup_cfg....
static nuint pass_jump_execute_cb(MtcsPass *mtcsPass,function *func)
{
   MtcsPassJump *self=(MtcsPassJump *)mtcsPass;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgCleanup *mtcsCfgCleanup=mtcs_target_get_cfg_cleanup(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsCse *mtcsCse=mtcs_target_get_cse(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   mtcs_cse_delete_trivially_dead_insns/*!delete_trivially_dead_insns*/(mtcsCse,
         mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData),mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc));
   if (dump_file)
      mtcs_cfg_context_dump_flow_info/*!dump_flow_info*/(mtcsCfgContext,dump_file, dump_flags);
   mtcs_cfg_cleanup_cleanup_cfg/*!cleanup_cfg*/(mtcsCfgCleanup,(mtcsOptionsItem->x_optimize ? CLEANUP_EXPENSIVE : 0)
           | (mtcsOptionsItem->x_flag_thread_jumps && mtcsOptionsItem->x_flag_expensive_optimizations
         ? CLEANUP_THREADING : 0));
   return 0;
}

static void mtcsPassJumpInit(MtcsPassJump *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =pass_jump_execute_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassJump *mtcs_pass_jump_new(MtcsMode *mtcsMode)
{
   MtcsPassJump *self = n_slice_alloc0 (sizeof(MtcsPassJump));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"jump");
   mtcsPassJumpInit(self);
   return self;
}

//原型 NEXT_PASS (pass_jump_after_combine, 1); RTL_PASS cfgcleanup.cc jump_after_combine y 有条件执行
//flag_thread_jumps && flag_expensive_optimizations; cleanup_cfg
static nboolean jump_after_combine_gate_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   return mtcsOptionsItem->x_flag_thread_jumps && mtcsOptionsItem->x_flag_expensive_optimizations;
}

static nuint jump_after_combine_execute_cb(MtcsPass *mtcsPass,function *func)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgCleanup *mtcsCfgCleanup=mtcs_target_get_cfg_cleanup(mtcsTarget);
   /* Jump threading does not keep dominators up-to-date.  */
   free_dominance_info (CDI_DOMINATORS);
   mtcs_cfg_cleanup_cleanup_cfg/*!cleanup_cfg*/(mtcsCfgCleanup,CLEANUP_THREADING);
   return 0;
}

static void mtcsPassJumpAfterCombineInit(MtcsPassJumpAfterCombine *self)
{
   MtcsPass *mtcsPass=(MtcsPass *)self;
   mtcsPass->execute =jump_after_combine_execute_cb;
   mtcsPass->gate =jump_after_combine_gate_cb;
   mtcs_pass_set_properties(mtcsPass,
      0,/* properties_required */
      0, /* properties_provided */
      0 /* properties_destroyed */);
   mtcs_pass_set_todo_flags(mtcsPass,
      0, /* todo_flags_start */
      0 /*todo_flags_finish */);
}

MtcsPassJumpAfterCombine *mtcs_pass_jump_after_combine(MtcsMode *mtcsMode)
{
   MtcsPassJumpAfterCombine *self = n_slice_alloc0 (sizeof(MtcsPassJumpAfterCombine));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"jump_after_combine");
   mtcsPassJumpAfterCombineInit(self);
   return self;
}

