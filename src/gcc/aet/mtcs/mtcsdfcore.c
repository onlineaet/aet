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
 * base on df-core.cc
 */
#define INCLUDE_ALGORITHM
#define INCLUDE_FUNCTIONAL
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "rtl.h"
#include "df.h"
#include "memmodel.h"
#include "emit-rtl.h"
#include "cfganal.h"
#include "tree-pass.h"
#include "cfgloop.h"
#include "rtl-ssa.h"

#include "aet/aetprinttree.h"
#include "mtcsdfcore.h"
#include "mtcstarget.h"
#include "mtcsprintrtl.h"
#include "mtcsdfscan.h"
#include "mtcsdfproblems.h"

static void *df_get_bb_info (MtcsDfcore *self,struct dataflow *, unsigned int);
static void df_set_bb_info (MtcsDfcore *self,struct dataflow *, unsigned int, void *);
static void df_clear_bb_info (MtcsDfcore *self,struct dataflow *, unsigned int);
#ifdef DF_DEBUG_CFG
static void df_set_clean_cfg (MtcsDfcore *self);
#endif

static void printuselink (rtx_insn *insn)
{
   return;
   struct df_link *defs;
   df_ref use;

   if (DEBUG_INSN_P (insn))
      return;
   fprintf (stderr, "mtcsdfcore.c printuselink 00 insn: %d\n", INSN_UID (insn));
   mtcs_print_rtl_single(stderr,insn);

   FOR_EACH_INSN_USE (use, insn){
      for (defs = DF_REF_CHAIN (use); defs; defs = defs->next){
         fprintf (stderr,"mtcsdfcore.c printuselink 11 %p insn:%p\n",defs,insn);
         fprintf (stderr,"mtcsdfcore.c printuselink 22 %p insn:%p\n",defs->ref,insn);
         fprintf (stderr,"mtcsdfcore.c printuselink 33 %p insn:%p\n",defs->ref->base,insn);
         fprintf (stderr,"mtcsdfcore.c printuselink 44 %p insn:%p\n",defs->ref->base.cl,insn);
         fprintf (stderr,"mtcsdfcore.c printuselink 55 %d insn:%p\n",DF_REF_IS_ARTIFICIAL (defs->ref),insn);
         fprintf (stderr,"mtcsdfcore.c printuselink 66 %p insn:%p\n",defs->ref->base.insn_info);
         if(defs->ref->base.insn_info)
            fprintf (stderr,"mtcsdfcore.c printuselink 77 %p insn:%p\n",defs->ref->base.insn_info->insn,insn);
      }
   }

}

static void printbb(basic_block block)
{
   rtx_insn *insn;
   int i=0;
   FOR_BB_INSNS (block, insn){
      if (!INSN_P (insn))
         continue;
      n_debug("mtcsdfcore.c 打印块中的指令 i:%d block:%p index:%d flags:%d insn:%p\n",i++,block,block->index,block->flags,insn);
      mtcs_print_rtl_single(stderr,insn);
      printuselink(insn);
   }
}

static unsigned int df_reg_chain_mark (df_ref refs, unsigned int regno,
         bool is_def, bool is_eq_use)
{
   if(!refs)
      return 0;
   unsigned int count = 0;
   df_ref ref;
   for (ref = refs; ref; ref = DF_REF_NEXT_REG (ref)){
     // gcc_assert (!DF_REF_IS_REG_MARKED (ref));
      fprintf(stderr,"re is xxx :%p %d\n",ref,ref==0xffffffff);
      if(ref==0xffffffff)
         break;
      if(DF_REF_IS_REG_MARKED (ref))
         n_debug("mtcsdfcore.c DF_REF_IS_REG_MARKED (ref)=true 错误\n");

      /* If there are no def-use or use-def chains, make sure that all
      of the chains are clear.  */
      n_debug("mtcsdfcore.c df_reg_chain_mark 00 ref:%p flags:%d is_def:%d is_eq_use:%d df_chain:%p regno:%d %d\n",
            ref,ref->base.flags,is_def,is_eq_use,df_chain,regno,DF_REF_REGNO (ref));

//      if (!df_chain)
//         gcc_assert (!DF_REF_CHAIN (ref));
//      /* Check to make sure the ref is in the correct chain.  */
//      gcc_assert (DF_REF_REGNO (ref) == regno);
//      if (is_def)
//         gcc_assert (DF_REF_REG_DEF_P (ref));
//      else
//         gcc_assert (!DF_REF_REG_DEF_P (ref));
//     // n_debug("mtcsdfscan.c df_reg_chain_mark 11 ref:%p flags:%d is_def:%d is_eq_use:%d\n",ref,ref->base.flags,is_def,is_eq_use);
//
//      if (is_eq_use)
//         gcc_assert ((DF_REF_FLAGS (ref) & DF_REF_IN_NOTE));
//      else
//         gcc_assert ((DF_REF_FLAGS (ref) & DF_REF_IN_NOTE) == 0);
//      //n_debug("mtcsdfscan.c df_reg_chain_mark 22 ref:%p flags:%d is_def:%d is_eq_use:%d\n",ref,ref->base.flags,is_def,is_eq_use);
//
//      if (DF_REF_NEXT_REG (ref))
//         gcc_assert (DF_REF_PREV_REG (DF_REF_NEXT_REG (ref)) == ref);
//      count++;
//     // n_debug("mtcsdfscan.c df_reg_chain_mark 33 ref:%p flags:%d is_def:%d is_eq_use:%d\n",ref,ref->base.flags,is_def,is_eq_use);
//      DF_REF_REG_MARK (ref);
//     // n_debug("mtcsdfscan.c df_reg_chain_mark 44 ref:%p flags:%d is_def:%d is_eq_use:%d\n",ref,ref->base.flags,is_def,is_eq_use);

   }
   return count;
}

/* Return true if df_ref information for all insns in all blocks are
   correct and complete.  */
//原型 df_scan_verify df.h df-scan.cc
static void dfScanVerify (struct function *fn)
{
   return;
   if(!n_log_is_debug_file(NULL,NULL))
      return;
   tree fndecl = fn->decl;
   char *name=IDENTIFIER_POINTER(DECL_NAME(fndecl));
   if(!strstr(name,"setdata"))
      return;
   char *passName="";

   unsigned int i;
   basic_block bb;
   if (!df)
      return;
   for (i = 0; i < DF_REG_SIZE (df); i++){
      fprintf(stderr,"mtcsdfcore.c dfScanVerify --- i:%d %p %d pass:%s\n",i,df->def_regs[i],DF_REG_DEF_COUNT (i),passName);
//      if(i==7){
//          break;
//      }
      df_reg_chain_mark (DF_REG_DEF_CHAIN (i), i, true, false);
     // gcc_assert (df_reg_chain_mark (DF_REG_DEF_CHAIN (i), i, true, false) == DF_REG_DEF_COUNT (i));
     // gcc_assert (df_reg_chain_mark (DF_REG_USE_CHAIN (i), i, false, false) == DF_REG_USE_COUNT (i));
     // gcc_assert (df_reg_chain_mark (DF_REG_EQ_USE_CHAIN (i), i, false, true) == DF_REG_EQ_USE_COUNT (i));
   }
}

static void testprint()
{
   return;
   if(!cfun )
      return;
   basic_block bb;
   FOR_ALL_BB_FN (bb, cfun)
      printbb(bb);
}


/* Add PROBLEM (and any dependent problems) to the DF instance.  */
//原型 df_add_problem df.h df-core.cc
void mtcs_dfcore_df_add_problem (MtcsDfcore *self,const struct df_problem *problem)
{
   struct dataflow *dflow;
   int i;
   n_debug("mtcsdfcore.c mtcs_dfcore_df_add_problem 00 dependent_problem:%p problem->id:%d dir:%d\n",
         problem->dependent_problem,problem->id,problem->dir);
   /* First try to add the dependent problem. */
   if (problem->dependent_problem)
      mtcs_dfcore_df_add_problem/*!df_add_problem*/(self,problem->dependent_problem);

   /* Check to see if this problem has already been defined.  If it
   has, just return that instance, if not, add it to the end of the
   vector.  */
   dflow = df->problems_by_index[problem->id];
   n_debug("mtcsdfcore.c mtcs_dfcore_df_add_problem 11 如果 dflow存在返回 dflow:%p problem->id:%d dir:%d num_problems_defined:%d\n",
         dflow,problem->id,problem->dir,df->num_problems_defined);
   if (dflow)
      return;

   /* Make a new one and add it to the end.  */
   dflow = XCNEW (struct dataflow);
   dflow->problem = problem;
   dflow->computed = false;
   dflow->solutions_dirty = true;
   df->problems_by_index[dflow->problem->id] = dflow;

   /* Keep the defined problems ordered by index.  This solves the
   problem that RI will use the information from UREC if UREC has
   been defined, or from LIVE if LIVE is defined and otherwise LR.
   However for this to work, the computation of RI must be pushed
   after which ever of those problems is defined, but we do not
   require any of those except for LR to have actually been
   defined.  */
   n_debug("mtcsdfcore.c mtcs_dfcore_df_add_problem 22 df->num_problems_defined:%d problem->id:%d dir:%d\n",
         df->num_problems_defined,problem->id,problem->dir);
   df->num_problems_defined++;
   for (i = df->num_problems_defined - 2; i >= 0; i--){
      if (problem->id < df->problems_in_order[i]->problem->id)
         df->problems_in_order[i+1] = df->problems_in_order[i];
      else{
         df->problems_in_order[i+1] = dflow;
         return;
      }
   }
   df->problems_in_order[0] = dflow;
}


/* Set the MASK flags in the DFLOW problem.  The old flags are
   returned.  If a flag is not allowed to be changed this will fail if
   checking is enabled.  */
//原型 df_set_flags df.h df-core.cc
int mtcs_dfcore_df_set_flags (MtcsDfcore *self,int changeable_flags)
{
   int old_flags = df->changeable_flags;
   df->changeable_flags |= changeable_flags;
   return old_flags;
}

/* Clear the MASK flags in the DFLOW problem.  The old flags are
   returned.  If a flag is not allowed to be changed this will fail if
   checking is enabled.  */
//原型 df_clear_flags df.h df-core.cc
int mtcs_dfcore_df_clear_flags (MtcsDfcore *self,int changeable_flags)
{
   int old_flags = df->changeable_flags;
   df->changeable_flags &= ~changeable_flags;
   return old_flags;
}


/* Set the blocks that are to be considered for analysis.  If this is
   not called or is called with null, the entire function in
   analyzed.  */
//原型 df_set_blocks df.h df-core.cc
void mtcs_dfcore_df_set_blocks (MtcsDfcore *self,bitmap blocks)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);

   if (blocks){
      if (dump_file)
         bitmap_print (dump_file, blocks, "setting blocks to analyze ", "\n");
      if (df->blocks_to_analyze){
         /* This block is called to change the focus from one subset
         to another.  */
         int p;
         auto_bitmap diff (&self->df_bitmap_obstack);
         bitmap_and_compl (diff, df->blocks_to_analyze, blocks);
         for (p = 0; p < df->num_problems_defined; p++){
            struct dataflow *dflow = df->problems_in_order[p];
            if (dflow->optional_p && dflow->problem->reset_fun)
               dflow->problem->reset_fun (df->blocks_to_analyze);
            else if (dflow->problem->free_blocks_on_set_blocks){
               bitmap_iterator bi;
               unsigned int bb_index;

               EXECUTE_IF_SET_IN_BITMAP (diff, 0, bb_index, bi){
                  basic_block bb = BASIC_BLOCK_FOR_FN (cfun, bb_index);
                  if (bb){
                     void *bb_info = df_get_bb_info(self,dflow, bb_index);
                     dflow->problem->free_bb_fun (bb, bb_info);
                     df_clear_bb_info(self,dflow, bb_index);
                  }
               }
            }
         }
      }else{
         /* This block of code is executed to change the focus from
         the entire function to a subset.  */
         bitmap_head blocks_to_reset;
         bool initialized = false;
         int p;
         for (p = 0; p < df->num_problems_defined; p++){
            struct dataflow *dflow = df->problems_in_order[p];
            if (dflow->optional_p && dflow->problem->reset_fun){
               if (!initialized) {
                  basic_block bb;
                  bitmap_initialize (&blocks_to_reset, &self->df_bitmap_obstack);
                  FOR_ALL_BB_FN (bb, cfun){
                     bitmap_set_bit (&blocks_to_reset, bb->index);
                  }
               }
               dflow->problem->reset_fun (&blocks_to_reset);
            }
         }
         if (initialized)
            bitmap_clear (&blocks_to_reset);

         df->blocks_to_analyze = BITMAP_ALLOC (&self->df_bitmap_obstack);
      }
      bitmap_copy (df->blocks_to_analyze, blocks);
      df->analyze_subset = true;
   }else{
      /* This block is executed to reset the focus to the entire
      function.  */
      if (dump_file)
         fprintf (dump_file, "clearing blocks_to_analyze\n");
         if (df->blocks_to_analyze){
            BITMAP_FREE (df->blocks_to_analyze);
            df->blocks_to_analyze = NULL;
         }
         df->analyze_subset = false;
   }

   /* Setting the blocks causes the refs to be unorganized since only
   the refs in the blocks are seen.  */
   mtcs_dfscan_df_maybe_reorganize_def_refs/*!df_maybe_reorganize_def_refs*/(mtcsDfscan,DF_REF_ORDER_NO_TABLE);
   mtcs_dfscan_df_maybe_reorganize_use_refs/*!df_maybe_reorganize_use_refs*/(mtcsDfscan,DF_REF_ORDER_NO_TABLE);
   mtcs_dfcore_df_mark_solutions_dirty/*!df_mark_solutions_dirty*/(self);
}


/* Delete a DFLOW problem (and any problems that depend on this
   problem).  */
//原型 df_remove_problem df.h df-core.cc
void mtcs_dfcore_df_remove_problem (MtcsDfcore *self,struct dataflow *dflow)
{
   const struct df_problem *problem;
   int i;

   if (!dflow)
      return;

   problem = dflow->problem;
   gcc_assert (problem->remove_problem_fun);
   n_debug("mtcsdfcore.c mtcs_dfcore_df_remove_problem 00 dflow:%p df->num_problems_defined:%d\n",dflow,df->num_problems_defined);

   /* Delete any problems that depended on this problem first.  */
   for (i = 0; i < df->num_problems_defined; i++)
      if (df->problems_in_order[i]->problem->dependent_problem == problem)
         mtcs_dfcore_df_remove_problem/*!df_remove_problem*/(self,df->problems_in_order[i]);
   n_debug("mtcsdfcore.c mtcs_dfcore_df_remove_problem 11 dflow:%p df->num_problems_defined:%d\n",dflow,df->num_problems_defined);
   /* Now remove this problem.  */
   for (i = 0; i < df->num_problems_defined; i++)
      if (df->problems_in_order[i] == dflow){
         int j;
         for (j = i + 1; j < df->num_problems_defined; j++)
            df->problems_in_order[j-1] = df->problems_in_order[j];
         df->problems_in_order[j-1] = NULL;
         n_debug("mtcsdfcore.c mtcs_dfcore_df_remove_problem 22 dflow:%p df->num_problems_defined:%d\n",dflow,df->num_problems_defined);
         df->num_problems_defined--;
         break;
      }

   (problem->remove_problem_fun) ();
   df->problems_by_index[problem->id] = NULL;
}


/* Remove all of the problems that are not permanent.  Scanning, LR
   and (at -O2 or higher) LIVE are permanent, the rest are removable.
   Also clear all of the changeable_flags.  */
//原型 df_finish_pass df.h df-core.cc
void mtcs_dfcore_df_finish_pass (MtcsDfcore *self,bool verify ATTRIBUTE_UNUSED)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
   MtcsDfproblems *mtcsDfproblems=mtcs_target_get_dfproblems(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   n_debug("mtcsdfcore.c mtcs_dfcore_df_finish_pass 00 verify:%d\n",verify);
   int i;

#ifdef ENABLE_DF_CHECKING
   int saved_flags;
#endif

   if (!df)
      return;

   mtcs_dfscan_df_maybe_reorganize_def_refs/*!df_maybe_reorganize_def_refs*/(mtcsDfscan,DF_REF_ORDER_NO_TABLE);
   n_debug("mtcsdfcore.c mtcs_dfcore_df_finish_pass 11 verify:%d\n",verify);

   mtcs_dfscan_df_maybe_reorganize_use_refs/*!df_maybe_reorganize_use_refs*/(mtcsDfscan,DF_REF_ORDER_NO_TABLE);
   n_debug("mtcsdfcore.c mtcs_dfcore_df_finish_pass 22 verify:%d\n",verify);

#ifdef ENABLE_DF_CHECKING
   saved_flags = df->changeable_flags;
#endif

   /* We iterate over problems by index as each problem removed will
   lead to problems_in_order to be reordered.  */
   for (i = 0; i < DF_LAST_PROBLEM_PLUS1; i++){
      struct dataflow *dflow = df->problems_by_index[i];
      n_debug("mtcsdfcore.c mtcs_dfcore_df_finish_pass 33 verify:%d %d\n",verify,dflow && dflow->optional_p);

      if (dflow && dflow->optional_p)
         mtcs_dfcore_df_remove_problem/*!df_remove_problem*/(self,dflow);
   }

   /* Clear all of the flags.  */
   df->changeable_flags = 0;
   mtcs_dfscan_df_process_deferred_rescans/*!df_process_deferred_rescans*/(mtcsDfscan);

   /* Set the focus back to the whole function.  */
   if (df->blocks_to_analyze){
      n_debug("mtcsdfcore.c mtcs_dfcore_df_finish_pass 44 verify:%d \n",verify);

      BITMAP_FREE (df->blocks_to_analyze);
      df->blocks_to_analyze = NULL;
      mtcs_dfcore_df_mark_solutions_dirty/*!df_mark_solutions_dirty*/(self);
      df->analyze_subset = false;
   }

#ifdef ENABLE_DF_CHECKING
   /* Verification will fail in DF_NO_INSN_RESCAN.  */
   if (!(saved_flags & DF_NO_INSN_RESCAN)){
      n_debug("mtcsdfcore.c mtcs_dfcore_df_finish_pass 55 verify:%d \n",verify);

      mtcs_dfproblems_df_lr_verify_transfer_functions/*!df_lr_verify_transfer_functions*/(mtcsDfproblems);
      if (df_live)
         mtcs_dfproblems_df_live_verify_transfer_functions/*!df_live_verify_transfer_functions*/(mtcsDfproblems);
   }

#ifdef DF_DEBUG_CFG
   n_debug("mtcsdfcore.c mtcs_dfcore_df_finish_pass 66 verify:%d \n",verify);
   df_set_clean_cfg(self);
#endif
#endif

   if (mtcsOptionsItem->x_flag_checking && verify)
      df->changeable_flags |= DF_VERIFY_SCHEDULED;
}

/* Set up the dataflow instance for the entire back end.  */
//原型 rest_of_handle_df_initialize df-core.cc
static unsigned int rest_of_handle_df_initialize (MtcsDfcore *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
   MtcsDfproblems *mtcsDfproblems=mtcs_target_get_dfproblems(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   gcc_assert (!df);
   df = XCNEW (class df_d);
   df->changeable_flags = 0;
   n_debug("mtcsdfcore.c rest_of_handle_df_initialize 00 df->postorder:%p\n ",df->postorder);

   bitmap_obstack_initialize (&self->df_bitmap_obstack);

   /* Set this to a conservative value.  Stack_ptr_mod will compute it
   correctly later.  */
   mtcsRtlData/*!crtl*/->sp_is_unchanging = 0;
   mtcs_dfscan_df_scan_add_problem/*!df_scan_add_problem*/(mtcsDfscan);
   n_debug("mtcsdfcore.c rest_of_handle_df_initialize 11\n ");

   mtcs_dfscan_df_scan_alloc/*!df_scan_alloc*/(mtcsDfscan,NULL);
   n_debug("mtcsdfcore.c rest_of_handle_df_initialize 22\n ");

   /* These three problems are permanent.  */
   mtcs_dfproblems_df_lr_add_problem/*!df_lr_add_problem*/(mtcsDfproblems);
   n_debug("mtcsdfcore.c rest_of_handle_df_initialize 33 optimize:%d\n",mtcsOptionsItem->x_optimize);

   if (mtcsOptionsItem->x_optimize > 1)
      mtcs_dfproblems_df_live_add_problem/*!df_live_add_problem*/(mtcsDfproblems);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register(mtcsReg);

   n_debug("mtcsdfcore.c rest_of_handle_df_initialize 44 :%d\n ",firstPseudoRegister);

   df->hard_regs_live_count = XCNEWVEC (unsigned int, firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/);

   mtcs_dfscan_df_hard_reg_init/*!df_hard_reg_init*/(mtcsDfscan);
   n_debug("mtcsdfcore.c rest_of_handle_df_initialize 55\n ");

   /* After reload, some ports add certain bits to regs_ever_live so
   this cannot be reset.  */
   mtcs_dfscan_df_compute_regs_ever_live/*!df_compute_regs_ever_live*/(mtcsDfscan,true);
   n_debug("mtcsdfcore.c rest_of_handle_df_initialize 66\n ");

   mtcs_dfscan_df_scan_blocks/*!df_scan_blocks*/(mtcsDfscan);
   n_debug("mtcsdfcore.c rest_of_handle_df_initialize 77\n ");

   mtcs_dfscan_df_compute_regs_ever_live/*!df_compute_regs_ever_live*/(mtcsDfscan,false);
   return 0;
}

/* Free all the dataflow info and the DF structure.  This should be
   called from the df_finish macro which also NULLs the parm.  */
static unsigned int rest_of_handle_df_finish (MtcsDfcore *self)
{
   int i;
   gcc_assert (df);
   n_debug("mtcsdfcore.c rest_of_handle_df_finish 00 df->num_problems_defined:%d\n",df->num_problems_defined);
   for (i = 0; i < df->num_problems_defined; i++){
      struct dataflow *dflow = df->problems_in_order[i];
      n_debug("mtcsdfcore.c rest_of_handle_df_finish i:%d id:%d dir:%d %p\n",
            i,dflow->problem->id,dflow->problem->dir,dflow->problem->free_fun);


      if (dflow->problem->free_fun)
    dflow->problem->free_fun ();
       else
    free (dflow);
   }


   n_debug("mtcsdfcore.c rest_of_handle_df_finish 11\n");

   free (df->postorder);
   free (df->postorder_inverted);
   free (df->hard_regs_live_count);
   n_debug("mtcsdfcore.c rest_of_handle_df_finish 22\n");

   free (df);
   n_debug("mtcsdfcore.c rest_of_handle_df_finish 33\n");

   df = NULL;
   bitmap_obstack_release (&self->df_bitmap_obstack);
   n_debug("mtcsdfcore.c rest_of_handle_df_finish 44\n");

   return 0;
}

/*----------------------------------------------------------------------------
   The general data flow analysis engine.
----------------------------------------------------------------------------*/

/* Helper function for df_worklist_dataflow.
   Propagate the dataflow forward.
   Given a BB_INDEX, do the dataflow propagation
   and set bits on for successors in PENDING for earlier
   and WORKLIST for later in bbindex_to_postorder
   if the out set of the dataflow has changed.

   AGE specify time when BB was visited last time.
   AGE of 0 means we are visiting for first time and need to
   compute transfer function to initialize datastructures.
   Otherwise we re-do transfer function only if something change
   while computing confluence functions.
   We need to compute confluence only of basic block that are younger
   then last visit of the BB.

   Return true if BB info has changed.  This is always the case
   in the first visit.  */
static bool df_worklist_propagate_forward (MtcsDfcore *self,struct dataflow *dataflow,
                unsigned bb_index,
                unsigned *bbindex_to_postorder,
                bitmap worklist,
                bitmap pending,
                sbitmap considered,
                vec<int> &last_change_age,
                int age)
{
   edge e;
   edge_iterator ei;
   basic_block bb = BASIC_BLOCK_FOR_FN (cfun, bb_index);
   bool changed = !age;

   /*  Calculate <conf_op> of incoming edges.  */
   if (EDGE_COUNT (bb->preds) > 0){
      FOR_EACH_EDGE (e, ei, bb->preds){
         if ((!age || age <= last_change_age[e->src->index])
                     && bitmap_bit_p (considered, e->src->index)){
            n_debug("df-core.cc df_worklist_propagate_forward 00 con_fun_n bb:%p id:%d\n",bb,dataflow->problem->id);
            changed |= dataflow->problem->con_fun_n (e);
         }
      }
   }else if (dataflow->problem->con_fun_0){
      n_debug("mtcsdfcore.c df_worklist_propagate_forward 11 con_fun_n bb_index:%d bb:%p id:%d\n",
                      bb_index,bb,dataflow->problem->id);
      dataflow->problem->con_fun_0 (bb);
      dfScanVerify(cfun);

   }
   n_debug("mtcsdfcore.c df_worklist_propagate_forward 22 con_fun_0 change:%d bb_index:%d bb:%p id:%d\n",
         changed,bb_index,bb,dataflow->problem->id);
   dfScanVerify(cfun);
   if (changed  && dataflow->problem->trans_fun (bb_index)){
      /* The out set of this block has changed.
      Propagate to the outgoing blocks.  */
      FOR_EACH_EDGE (e, ei, bb->succs){
         unsigned ob_index = e->dest->index;

         if (bitmap_bit_p (considered, ob_index)){
            if (bbindex_to_postorder[bb_index]  < bbindex_to_postorder[ob_index]){
               n_debug("mtcsdfcore.c df_worklist_propagate_forward 33 con_fun_0 change:%d bb_index:%d bb:%p id:%d\n",
                     changed,bb_index,bb,dataflow->problem->id);
               if (worklist)
                  bitmap_set_bit (worklist, bbindex_to_postorder[ob_index]);
               dfScanVerify(cfun);

            }else{
               n_debug("mtcsdfcore.c df_worklist_propagate_forward 44 con_fun_0 change:%d bb_index:%d bb:%p id:%d %d %d\n",
                     changed,bb_index,bb,dataflow->problem->id,ob_index,bbindex_to_postorder[ob_index]);
               dfScanVerify(cfun);

               bitmap_set_bit (pending, bbindex_to_postorder[ob_index]);
               n_debug("mtcsdfcore.c df_worklist_propagate_forward 44aa con_fun_0 change:%d bb_index:%d bb:%p id:%d\n",
                          changed,bb_index,bb,dataflow->problem->id);
               dfScanVerify(cfun);

            }
         }
      }
      return true;
   }
   return false;
}


/* Helper function for df_worklist_dataflow.
   Propagate the dataflow backward.  */
static bool df_worklist_propagate_backward (MtcsDfcore *self,struct dataflow *dataflow,
            unsigned bb_index,
            unsigned *bbindex_to_postorder,
            bitmap worklist,
            bitmap pending,
            sbitmap considered,
            vec<int> &last_change_age,
            int age)
{
   edge e;
   edge_iterator ei;
   basic_block bb = BASIC_BLOCK_FOR_FN (cfun, bb_index);
   bool changed = !age;

   /*  Calculate <conf_op> of incoming edges.  */
   if (EDGE_COUNT (bb->succs) > 0){
      FOR_EACH_EDGE (e, ei, bb->succs){
//         if (bbindex_to_postorder[e->dest->index] < last_change_age.length ()
//         && age <= last_change_age[bbindex_to_postorder[e->dest->index]]
//         && bitmap_bit_p (considered, e->dest->index)){
//            n_debug("mtcsdfcore.c df_worklist_propagate_backward 00 执行 问题 id:%d的 con_fun_n bb:%p\n",
//                  dataflow->problem->id,bb);
//            changed |= dataflow->problem->con_fun_n (e);
//         }
         if ((!age || age <= last_change_age[e->dest->index])
             && bitmap_bit_p (considered, e->dest->index)){
            n_debug("df-core.cc df_worklist_propagate_backward 00 执行 问题 id:%d的 con_fun_n bb:%p\n",
                         dataflow->problem->id,bb);
                changed |= dataflow->problem->con_fun_n (e);
         }
      }
   }else if (dataflow->problem->con_fun_0){
      n_debug("mtcsdfcore.c df_worklist_propagate_backward 11 执行 问题 id:%d的 con_fun_0 bb:%p\n",dataflow->problem->id,bb);

      dataflow->problem->con_fun_0 (bb);
   }
   n_debug("mtcsdfcore.c df_worklist_propagate_backward 22 执行 问题 id:%d change:%d bb:%p\n",dataflow->problem->id,changed,bb);

   if (changed  && dataflow->problem->trans_fun (bb_index)){
      /* The out set of this block has changed.
      Propagate to the outgoing blocks.  */
      FOR_EACH_EDGE (e, ei, bb->preds){
         unsigned ob_index = e->src->index;

         if (bitmap_bit_p (considered, ob_index)){
            if (bbindex_to_postorder[bb_index] < bbindex_to_postorder[ob_index]){
               n_debug("mtcsdfcore.c df_worklist_propagate_backward 33 执行 问题 id:%d change:%d\n",dataflow->problem->id,changed);
               if (worklist)
                  bitmap_set_bit (worklist, bbindex_to_postorder[ob_index]);
            }else{
               n_debug("mtcsdfcore.c df_worklist_propagate_backward 44 执行 问题 id:%d change:%d\n",dataflow->problem->id,changed);
               bitmap_set_bit (pending, bbindex_to_postorder[ob_index]);
            }
         }
      }
      return true;
   }
   return false;
}

/* Main dataflow solver loop.

   DATAFLOW is problem we are solving, PENDING is worklist of basic blocks we
   need to visit.
   BLOCK_IN_POSTORDER is array of size N_BLOCKS specifying postorder in BBs and
   BBINDEX_TO_POSTORDER is array mapping back BB->index to postorder position.
   PENDING will be freed.

   The worklists are bitmaps indexed by postorder positions.

   The function implements standard algorithm for dataflow solving with two
   worklists (we are processing WORKLIST and storing new BBs to visit in
   PENDING).

   As an optimization we maintain ages when BB was changed (stored in
   last_change_age) and when it was last visited (stored in last_visit_age).
   This avoids need to re-do confluence function for edges to basic blocks
   whose source did not change since destination was visited last time.  */

static void df_worklist_dataflow_doublequeue (MtcsDfcore *self,struct dataflow *dataflow,
                                  sbitmap considered,
                                  int *blocks_in_postorder,
              unsigned *bbindex_to_postorder,
              unsigned n_blocks)
{
   enum df_flow_dir dir = dataflow->problem->dir;
   int dcount = 0;
   int age = 0;
   bool changed;
   vec<int> last_visit_age = vNULL;
   vec<int> last_change_age = vNULL;
   n_debug("mtcsdfcore.c df_worklist_dataflow_doublequeue 00aa dir:%d DF_FORWARD:%d\n",
             dir,DF_FORWARD);
   dfScanVerify(cfun);
   bitmap worklist = BITMAP_ALLOC (&self->df_bitmap_obstack);
   bitmap_tree_view (worklist);
   n_debug("mtcsdfcore.c df_worklist_dataflow_doublequeue 00bb dir:%d DF_FORWARD:%d\n",
             dir,DF_FORWARD);
   dfScanVerify(cfun);
   last_visit_age.safe_grow (n_blocks, true);
   last_change_age.safe_grow (last_basic_block_for_fn (cfun) + 1, true);
   n_debug("mtcsdfcore.c df_worklist_dataflow_doublequeue 00cc dir:%d DF_FORWARD:%d\n",
             dir,DF_FORWARD);
   dfScanVerify(cfun);
   /* Make last_change_age defined - we can access uninit values for not
   considered blocks but will make sure they are considered as well.  */
   VALGRIND_DISCARD (VALGRIND_MAKE_MEM_DEFINED(last_change_age.address (), sizeof (int) * last_basic_block_for_fn (cfun)));
   n_debug("mtcsdfcore.c df_worklist_dataflow_doublequeue 00dd dir:%d DF_FORWARD:%d\n",
             dir,DF_FORWARD);
   dfScanVerify(cfun);

   /* We start with processing all blocks, populating pending for the
   next iteration.  */
   bitmap pending = BITMAP_ALLOC (&self->df_bitmap_obstack);
   bitmap_tree_view (pending);
   for (unsigned index = 0; index < n_blocks; ++index){
      unsigned bb_index = blocks_in_postorder[index];
      dcount++;
      n_debug("mtcsdfcore.c df_worklist_dataflow_doublequeue 00 index:%d bb_index:%d dir:%d DF_FORWARD:%d\n",
               index,bb_index,dir,DF_FORWARD);
      dfScanVerify(cfun);
      if (dir == DF_FORWARD)
         changed = df_worklist_propagate_forward (self,dataflow, bb_index,
                              bbindex_to_postorder,
                              NULL, pending,
                              considered,
                              last_change_age, 0);
      else
         changed = df_worklist_propagate_backward (self,dataflow, bb_index,
                              bbindex_to_postorder,
                              NULL, pending,
                              considered,
                              last_change_age, 0);
      last_visit_age[index] = ++age;
      if (changed)
         last_change_age[bb_index] = age;
      else
         last_change_age[bb_index] = 0;
      n_debug("mtcsdfcore.c df_worklist_dataflow_doublequeue 00aa index:%d bb_index:%d dir:%d DF_FORWARD:%d\n",
               index,bb_index,dir,DF_FORWARD);
      dfScanVerify(cfun);
   }
   n_debug("mtcsdfcore.c df_worklist_dataflow_doublequeue 00bb\n");
   dfScanVerify(cfun);
   /* Double-queueing.  Worklist is for the current iteration,
   and pending is for the next.  */
   while (!bitmap_empty_p (pending)){
      std::swap (pending, worklist);

      do{
         unsigned index = bitmap_clear_first_set_bit (worklist);
         unsigned bb_index = blocks_in_postorder[index];
         dcount++;
         int prev_age = last_visit_age[index];
         n_debug("mtcsdfcore.c df_worklist_dataflow_doublequeue 11 index:%d bb_index:%d dir:%d DF_FORWARD:%d\n",
               index,bb_index,dir,DF_FORWARD);
         dfScanVerify(cfun);
         if (dir == DF_FORWARD)
            changed = df_worklist_propagate_forward (self,dataflow, bb_index,
                                 bbindex_to_postorder,
                                 worklist, pending,
                                 considered,
                                 last_change_age,
                                 prev_age);
         else
            changed = df_worklist_propagate_backward (self,dataflow, bb_index,
                                 bbindex_to_postorder,
                                 worklist, pending,
                                 considered,
                                 last_change_age,
                                 prev_age);
         n_debug("mtcsdfcore.c df_worklist_dataflow_doublequeue 11aa\n");
         dfScanVerify(cfun);
         last_visit_age[index] = ++age;
         if (changed)
            last_change_age[bb_index] = age;
      }while (!bitmap_empty_p (worklist));
   }

   BITMAP_FREE (worklist);
   BITMAP_FREE (pending);
   last_visit_age.release ();
   last_change_age.release ();

   /* Dump statistics. */
   //if (dump_file)
   n_debug ("df_worklist_dataflow_doublequeue:"
   " n_basic_blocks %d n_edges %d"
   " count %d (%5.2g)\n",
   n_basic_blocks_for_fn (cfun), n_edges_for_fn (cfun),
   dcount, dcount / (double)n_basic_blocks_for_fn (cfun));
}

/* Worklist-based dataflow solver. It uses sbitmap as a worklist,
   with "n"-th bit representing the n-th block in the reverse-postorder order.
   The solver is a double-queue algorithm similar to the "double stack" solver
   from Cooper, Harvey and Kennedy, "Iterative data-flow analysis, Revisited".
   The only significant difference is that the worklist in this implementation
   is always sorted in RPO of the CFG visiting direction.  */
//原型 df_worklist_dataflow df.h df-core.cc
void mtcs_dfcore_df_worklist_dataflow (MtcsDfcore *self,struct dataflow *dataflow,
                      bitmap blocks_to_consider,
                      int *blocks_in_postorder,
                      int n_blocks)
{
   bitmap_iterator bi;
   unsigned int *bbindex_to_postorder;
   int i;
   unsigned int index;
   enum df_flow_dir dir = dataflow->problem->dir;

   gcc_assert (dir != DF_NONE);
   n_debug("mtcsdfcore.c mtcs_dfcore_df_worklist_dataflow 00 问题 id:%d n_blocks:%d %d\n",
         dataflow->problem->id,n_blocks,last_basic_block_for_fn (cfun));

   /* BBINDEX_TO_POSTORDER maps the bb->index to the reverse postorder.  */
   bbindex_to_postorder = XNEWVEC (unsigned int,last_basic_block_for_fn (cfun));

   /* Initialize the array to an out-of-bound value.  */
   for (i = 0; i < last_basic_block_for_fn (cfun); i++)
      bbindex_to_postorder[i] = last_basic_block_for_fn (cfun);

   /* Initialize the considered map.  */
   auto_sbitmap considered (last_basic_block_for_fn (cfun));
   bitmap_clear (considered);
   EXECUTE_IF_SET_IN_BITMAP (blocks_to_consider, 0, index, bi){
      bitmap_set_bit (considered, index);
   }

   /* Initialize the mapping of block index to postorder.  */
   for (i = 0; i < n_blocks; i++)
      bbindex_to_postorder[blocks_in_postorder[i]] = i;

   n_debug("mtcsdfcore.c mtcs_dfcore_df_worklist_dataflow 11 问题 id:%d 初始化指定的数据 \n",dataflow->problem->id);
   dfScanVerify(cfun);
   /* Initialize the problem. */
   if (dataflow->problem->init_fun)
      dataflow->problem->init_fun (blocks_to_consider);
   n_debug("mtcsdfcore.c mtcs_dfcore_df_worklist_dataflow 11aa 问题 id:%d 初始化指定的数据 \n",dataflow->problem->id);

   dfScanVerify(cfun);

   /* Solve it.  */
   /* Solve it.  */
   df_worklist_dataflow_doublequeue (self,dataflow, considered, blocks_in_postorder,
                 bbindex_to_postorder, n_blocks);

   n_debug("mtcsdfcore.c mtcs_dfcore_df_worklist_dataflow 11bb 问题 id:%d 初始化指定的数据 \n",dataflow->problem->id);

   dfScanVerify(cfun);
//   df_worklist_dataflow_doublequeue(self,dataflow, pending, considered,
//                                       blocks_in_postorder,
//                                       bbindex_to_postorder,
//                                       n_blocks);
   free (bbindex_to_postorder);
}

/* Remove the entries not in BLOCKS from the LIST of length LEN, preserving
   the order of the remaining entries.  Returns the length of the resulting
   list.  */
static unsigned df_prune_to_subcfg (MtcsDfcore *self,int list[], unsigned len, bitmap blocks)
{
   unsigned act, last;

   for (act = 0, last = 0; act < len; act++)
      if (bitmap_bit_p (blocks, list[act]))
         list[last++] = list[act];

   return last;
}


static void printdl(MtcsDfcore *self,basic_block bb)
{
   bitmap local_live = BITMAP_ALLOC (&self->df_bitmap_obstack);
   bitmap_copy (local_live, DF_LR_OUT (bb));
   rtx_insn *insn;
   df_ref def;

   FOR_BB_INSNS_REVERSE (bb, insn)
     if (INSN_P (insn)){
        FOR_EACH_INSN_DEF (def, insn)
            n_debug("mtcsdfcore.c printdl bb:%p insn:%p %d\n",bb,insn,bitmap_bit_p (local_live, DF_REF_REGNO (def)));
     }
   BITMAP_FREE (local_live);
}

static void print_local_live(MtcsDfcore *self)
{
   return;
   if(!cfun )
      return;
   basic_block bb;
   FOR_ALL_BB_FN (bb, cfun)
      printdl(self,bb);
}

/* Execute dataflow analysis on a single dataflow problem.

   BLOCKS_TO_CONSIDER are the blocks whose solution can either be
   examined or will be computed.  For calls from DF_ANALYZE, this is
   the set of blocks that has been passed to DF_SET_BLOCKS.
*/
//原型 df_analyze_problem df.h df-core.cc
void mtcs_dfcore_df_analyze_problem (MtcsDfcore *self,struct dataflow *dflow,
          bitmap blocks_to_consider,int *postorder, int n_blocks)
{
   /* (Re)Allocate the datastructures necessary to solve the problem.  */
   if (dflow->problem->alloc_fun)
      dflow->problem->alloc_fun (blocks_to_consider);
#ifdef ENABLE_DF_CHECKING
   if (dflow->problem->verify_start_fun)
      dflow->problem->verify_start_fun ();
#endif
   print_local_live(self);
   /* Set up the problem and compute the local information.  */
   if (dflow->problem->local_compute_fun)
      dflow->problem->local_compute_fun (blocks_to_consider);
   n_debug("mtcsdfcore.c mtcs_dfcore_df_analyze_problem 11 n_blocks:%d id:%d\n",n_blocks,dflow->problem->id);
   testprint();
   print_local_live(self);
   dfScanVerify(cfun);
   /* Solve the equations.  */
   if (dflow->problem->dataflow_fun)
      dflow->problem->dataflow_fun (dflow, blocks_to_consider,postorder, n_blocks);
   n_debug("mtcsdfcore.c mtcs_dfcore_df_analyze_problem 22 n_blocks:%d id:%d\n",n_blocks,dflow->problem->id);
   testprint();
   print_local_live(self);
   dfScanVerify(cfun);

   /* Massage the solution.  */
   if (dflow->problem->finalize_fun)
      dflow->problem->finalize_fun (blocks_to_consider);
   n_debug("mtcsdfcore.c mtcs_dfcore_df_analyze_problem 33 n_blocks:%d id:%d\n",n_blocks,dflow->problem->id);
   testprint();
   dfScanVerify(cfun);

#ifdef ENABLE_DF_CHECKING
   if (dflow->problem->verify_end_fun)
      dflow->problem->verify_end_fun ();
#endif
   dflow->computed = true;
}

/* Analyze dataflow info.  */
static void df_analyze_1 (MtcsDfcore *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
   n_debug("mtcsdfcore.c df_analyze_1 00 df->num_problems_defined:%d\n",df->num_problems_defined);

   int i;
   /* We need to do this before the df_verify_all because this is
   not kept incrementally up to date.  */
   mtcs_dfscan_df_compute_regs_ever_live/*!df_compute_regs_ever_live*/(mtcsDfscan,false);
   mtcs_dfscan_df_process_deferred_rescans/*!df_process_deferred_rescans*/(mtcsDfscan);
   if (dump_file)
      fprintf (dump_file, "df_analyze called\n");


#ifndef ENABLE_DF_CHECKING
   if (df->changeable_flags & DF_VERIFY_SCHEDULED)
#endif
      mtcs_dfcore_df_verify/*!df_verify*/(self);
   /* Skip over the DF_SCAN problem. */
   n_debug("mtcsdfcore.c  df_analyze_1 22 df->num_problems_defined:%d\n",df->num_problems_defined);
   testprint();
   dfScanVerify(cfun);
   for (i = 1; i < df->num_problems_defined; i++){
      struct dataflow *dflow = df->problems_in_order[i];
      n_debug("mtcsdfcore.c  df_analyze_1 33 i:%d dflow:%p problem id:%d \n",i,dflow,dflow->problem->id);
      testprint();
      dfScanVerify(cfun);
      if (dflow->solutions_dirty){
         n_debug("mtcsdfcore.c df_analyze_1 44 i:%d dflow:%p dir:%d \n",i,dflow,dflow->problem->dir);
         dfScanVerify(cfun);

         if (dflow->problem->dir == DF_FORWARD){
            mtcs_dfcore_df_analyze_problem/*!df_analyze_problem*/(self,
                  dflow,df->blocks_to_analyze,df->postorder_inverted,df->n_blocks);
         }else{
            mtcs_dfcore_df_analyze_problem/*!df_analyze_problem*/(self,
                  dflow,df->blocks_to_analyze,df->postorder,df->n_blocks);
         }
         n_debug("mtcsdfcore.c df_analyze_1 55 i:%d dflow:%p dir:%d \n",i,dflow,dflow->problem->dir);
              dfScanVerify(cfun);
      }
   }


   if (!df->analyze_subset){
      BITMAP_FREE (df->blocks_to_analyze);
      df->blocks_to_analyze = NULL;
   }

#ifdef DF_DEBUG_CFG
   n_debug("mtcsdfcore.c df_analyze_1 55 df->analyze_subset:%d\n",df->analyze_subset);
   df_set_clean_cfg(self);
#endif
}



/* Analyze dataflow info.  */
//原型 df_analyze df.h df-core.cc
void mtcs_dfcore_df_analyze (MtcsDfcore *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   bitmap current_all_blocks = BITMAP_ALLOC (&self->df_bitmap_obstack);
   free (df->postorder);
   free (df->postorder_inverted);
   /* For DF_FORWARD use a RPO on the forward graph.  Since we want to
   have unreachable blocks deleted use post_order_compute and reverse
   the order.  */
   df->postorder_inverted = XNEWVEC (int, n_basic_blocks_for_fn (cfun));
   df->n_blocks = post_order_compute (df->postorder_inverted, true, true);
   for (int i = 0; i < df->n_blocks / 2; ++i)
      std::swap (df->postorder_inverted[i],df->postorder_inverted[df->n_blocks - 1 - i]);

   /* For DF_BACKWARD use a RPO on the reverse graph.  */
   df->postorder = XNEWVEC (int, n_basic_blocks_for_fn (cfun));
   int n = inverted_rev_post_order_compute (cfun, df->postorder);
   gcc_assert (n == df->n_blocks);
   for (int i = 0; i < df->n_blocks; i++)
      bitmap_set_bit (current_all_blocks, df->postorder[i]);

   if (mtcsOptionsItem->x_flag_checking){
      /* Verify that POSTORDER_INVERTED only contains blocks reachable from
      the ENTRY block.  */
      for (int i = 0; i < df->n_blocks; i++)
         gcc_assert (bitmap_bit_p (current_all_blocks,df->postorder_inverted[i]));
   }

   /* Make sure that we have pruned any unreachable blocks from these
   sets.  */
   if (df->analyze_subset){
      bitmap_and_into (df->blocks_to_analyze, current_all_blocks);
      unsigned int newlen = df_prune_to_subcfg(self,df->postorder, df->n_blocks,df->blocks_to_analyze);
      df_prune_to_subcfg(self,df->postorder_inverted, df->n_blocks, df->blocks_to_analyze);
      df->n_blocks = newlen;
      BITMAP_FREE (current_all_blocks);
   }else{
      df->blocks_to_analyze = current_all_blocks;
      current_all_blocks = NULL;
   }
   n_debug("mtcsdfcore.c mtcs_dfcore_df_analyze 00\n");
   df_analyze_1(self);
}

/* Compute the reverse top sort order of the sub-CFG specified by LOOP.
   Returns the number of blocks which is always loop->num_nodes.  */
static int loop_rev_post_order_compute (MtcsDfcore *self,int *post_order, class loop *loop)
{
   edge_iterator *stack;
   int sp;
   int post_order_num = loop->num_nodes - 1;

   /* Allocate stack for back-tracking up CFG.  */
   stack = XNEWVEC (edge_iterator, loop->num_nodes + 1);
   sp = 0;

   /* Allocate bitmap to track nodes that have been visited.  */
   auto_bitmap visited;

   /* Push the first edge on to the stack.  */
   stack[sp++] = ei_start (loop_preheader_edge (loop)->src->succs);

   while (sp){
      edge_iterator ei;
      basic_block src;
      basic_block dest;

      /* Look at the edge on the top of the stack.  */
      ei = stack[sp - 1];
      src = ei_edge (ei)->src;
      dest = ei_edge (ei)->dest;

      /* Check if the edge destination has been visited yet and mark it
      if not so.  */
      if (flow_bb_inside_loop_p (loop, dest) && bitmap_set_bit (visited, dest->index)){
         if (EDGE_COUNT (dest->succs) > 0)
            /* Since the DEST node has been visited for the first
            time, check its successors.  */
            stack[sp++] = ei_start (dest->succs);
         else
            post_order[post_order_num--] = dest->index;
      }else{
         if (ei_one_before_end_p (ei) && src != loop_preheader_edge (loop)->src)
            post_order[post_order_num--] = src->index;

         if (!ei_one_before_end_p (ei))
            ei_next (&stack[sp - 1]);
         else
            sp--;
      }
   }

   free (stack);

   return loop->num_nodes;
}

/* Compute the reverse top sort order of the inverted sub-CFG specified
   by LOOP.  Returns the number of blocks which is always loop->num_nodes.  */
static int loop_inverted_rev_post_order_compute (MtcsDfcore *self,int *post_order, class loop *loop)
{
   basic_block bb;
   edge_iterator *stack;
   int sp;
   int post_order_num = loop->num_nodes - 1;

   /* Allocate stack for back-tracking up CFG.  */
   stack = XNEWVEC (edge_iterator, loop->num_nodes + 1);
   sp = 0;

   /* Allocate bitmap to track nodes that have been visited.  */
   auto_bitmap visited;

   /* Put all latches into the initial work list.  In theory we'd want
   to start from loop exits but then we'd have the special case of
   endless loops.  It doesn't really matter for DF iteration order and
   handling latches last is probably even better.  */
   stack[sp++] = ei_start (loop->header->preds);
   bitmap_set_bit (visited, loop->header->index);

   /* The inverted traversal loop. */
   while (sp){
      edge_iterator ei;
      basic_block pred;

      /* Look at the edge on the top of the stack.  */
      ei = stack[sp - 1];
      bb = ei_edge (ei)->dest;
      pred = ei_edge (ei)->src;

      /* Check if the predecessor has been visited yet and mark it
      if not so.  */
      if (flow_bb_inside_loop_p (loop, pred)  && bitmap_set_bit (visited, pred->index)){
         if (EDGE_COUNT (pred->preds) > 0)
            /* Since the predecessor node has been visited for the first
            time, check its predecessors.  */
            stack[sp++] = ei_start (pred->preds);
         else
            post_order[post_order_num--] = pred->index;
      }else{
         if (flow_bb_inside_loop_p (loop, bb) && ei_one_before_end_p (ei))
            post_order[post_order_num--] = bb->index;

         if (!ei_one_before_end_p (ei))
            ei_next (&stack[sp - 1]);
         else
            sp--;
      }
   }

   free (stack);
   return loop->num_nodes;
}

/* Analyze dataflow info for the basic blocks contained in LOOP.  */
//原型 df_analyze_loop df.h df-core.cc
void mtcs_dfcore_df_analyze_loop (MtcsDfcore *self,class loop *loop)
{
   free (df->postorder);
   free (df->postorder_inverted);

   df->postorder = XNEWVEC (int, loop->num_nodes);
   df->postorder_inverted = XNEWVEC (int, loop->num_nodes);
   df->n_blocks = loop_rev_post_order_compute(self,df->postorder_inverted, loop);
   int n = loop_inverted_rev_post_order_compute(self,df->postorder, loop);
   gcc_assert ((unsigned) df->n_blocks == loop->num_nodes);
   gcc_assert ((unsigned) n == loop->num_nodes);

   bitmap blocks = BITMAP_ALLOC (&self->df_bitmap_obstack);
   for (int i = 0; i < df->n_blocks; ++i)
      bitmap_set_bit (blocks, df->postorder[i]);
   mtcs_dfcore_df_set_blocks/*!df_set_blocks*/(self,blocks);
   BITMAP_FREE (blocks);

   df_analyze_1(self);
}


/* Return the number of basic blocks from the last call to df_analyze.  */
//原型 df_get_n_blocks df.h df-core.cc
int mtcs_dfcore_df_get_n_blocks (MtcsDfcore *self,enum df_flow_dir dir)
{
   gcc_assert (dir != DF_NONE);
   if (dir == DF_FORWARD){
      gcc_assert (df->postorder_inverted);
      return df->n_blocks;
   }
   gcc_assert (df->postorder);
   return df->n_blocks;
}


/* Return a pointer to the array of basic blocks in the reverse postorder.
   Depending on the direction of the dataflow problem,
   it returns either the usual reverse postorder array
   or the reverse postorder of inverted traversal. */
//原型 df_get_postorder df.h df-core.cc
int *mtcs_dfcore_df_get_postorder (MtcsDfcore *self,enum df_flow_dir dir)
{
   gcc_assert (dir != DF_NONE);
   if (dir == DF_FORWARD){
      gcc_assert (df->postorder_inverted);
      return df->postorder_inverted;
   }
   gcc_assert (df->postorder);
   return df->postorder;
}


/* Interface for calling iterative dataflow with user defined
   confluence and transfer functions.  All that is necessary is to
   supply DIR, a direction, CONF_FUN_0, a confluence function for
   blocks with no logical preds (or NULL), CONF_FUN_N, the normal
   confluence function, TRANS_FUN, the basic block transfer function,
   and BLOCKS, the set of blocks to examine, POSTORDER the blocks in
   postorder, and N_BLOCKS, the number of blocks in POSTORDER. */
//原型 df_simple_dataflow df.h df-core.cc
void mtcs_dfcore_df_simple_dataflow (MtcsDfcore *self,enum df_flow_dir dir,
          df_init_function init_fun,
          df_confluence_function_0 con_fun_0,
          df_confluence_function_n con_fun_n,
          df_transfer_function trans_fun,
          bitmap blocks, int * postorder, int n_blocks)
{
  memset (&self->user_problem, 0, sizeof (struct df_problem));
  self->user_problem.dir = dir;
  self->user_problem.init_fun = init_fun;
  self->user_problem.con_fun_0 = con_fun_0;
  self->user_problem.con_fun_n = con_fun_n;
  self->user_problem.trans_fun = trans_fun;
  self->user_dflow.problem = &self->user_problem;
  mtcs_dfcore_df_worklist_dataflow/*!df_worklist_dataflow*/(self,&self->user_dflow, blocks, postorder, n_blocks);
}



/*----------------------------------------------------------------------------
   Functions to support limited incremental change.
----------------------------------------------------------------------------*/

/* Get basic block info.  */
static void *df_get_bb_info(MtcsDfcore *self,struct dataflow *dflow, unsigned int index)
{
   if (dflow->block_info == NULL)
      return NULL;
   if (index >= dflow->block_info_size)
      return NULL;
   return (void *)((char *)dflow->block_info + index * dflow->problem->block_info_elt_size);
}


/* Set basic block info.  */
static void df_set_bb_info (MtcsDfcore *self,struct dataflow *dflow, unsigned int index, void *bb_info)
{
   gcc_assert (dflow->block_info);
   memcpy ((char *)dflow->block_info+ index * dflow->problem->block_info_elt_size,
         bb_info, dflow->problem->block_info_elt_size);
}

/* Clear basic block info.  */
static void df_clear_bb_info (MtcsDfcore *self,struct dataflow *dflow, unsigned int index)
{
   gcc_assert (dflow->block_info);
   gcc_assert (dflow->block_info_size > index);
   memset ((char *)dflow->block_info + index * dflow->problem->block_info_elt_size,0,
         dflow->problem->block_info_elt_size);
}


/* Mark the solutions as being out of date.  */
//原型 df_mark_solutions_dirty df.h df-core.cc
void mtcs_dfcore_df_mark_solutions_dirty (MtcsDfcore *self)
{
   if (df){
      int p;
      for (p = 1; p < df->num_problems_defined; p++)
         df->problems_in_order[p]->solutions_dirty = true;
   }
}

/* Return true if BB needs it's transfer functions recomputed.  */
//原型 df_get_bb_dirty df.h df-core.cc
bool mtcs_dfcore_df_get_bb_dirty (MtcsDfcore *self,basic_block bb)
{
  return bitmap_bit_p ((df_live ? df_live : df_lr)->out_of_date_transfer_functions, bb->index);
}


/* Mark BB as needing it's transfer functions as being out of
   date.  */
//原型 df_set_bb_dirty df.h df-core.cc
void mtcs_dfcore_df_set_bb_dirty (MtcsDfcore *self,basic_block bb)
{
   bb->flags |= BB_MODIFIED;
   if (df){
      int p;
      for (p = 1; p < df->num_problems_defined; p++){
         struct dataflow *dflow = df->problems_in_order[p];
         if (dflow->out_of_date_transfer_functions)
            bitmap_set_bit (dflow->out_of_date_transfer_functions, bb->index);
      }
      mtcs_dfcore_df_mark_solutions_dirty/*!df_mark_solutions_dirty*/(self);
   }
}


/* Grow the bb_info array.  */
//原型 df_grow_bb_info df.h df-core.cc
void mtcs_dfcore_df_grow_bb_info (MtcsDfcore *self,struct dataflow *dflow)
{
   unsigned int new_size = last_basic_block_for_fn (cfun) + 1;
   if (dflow->block_info_size < new_size){
      new_size += new_size / 4;
      dflow->block_info = (void *)XRESIZEVEC (char, (char *)dflow->block_info,new_size* dflow->problem->block_info_elt_size);
      int ssi=(new_size - dflow->block_info_size) * dflow->problem->block_info_elt_size;
      memset ((char *)dflow->block_info + dflow->block_info_size * dflow->problem->block_info_elt_size,
            0, (new_size - dflow->block_info_size) * dflow->problem->block_info_elt_size);
      dflow->block_info_size = new_size;
   }
}

/* Clear the dirty bits.  This is called from places that delete
   blocks.  */
static void df_clear_bb_dirty (MtcsDfcore *self,basic_block bb)
{
   int p;
   for (p = 1; p < df->num_problems_defined; p++){
      struct dataflow *dflow = df->problems_in_order[p];
      if (dflow->out_of_date_transfer_functions)
         bitmap_clear_bit (dflow->out_of_date_transfer_functions, bb->index);
   }
}

/* Called from the rtl_compact_blocks to reorganize the problems basic
   block info.  */
//原型 df_compact_blocks df.h df-core.cc
void mtcs_dfcore_df_compact_blocks (MtcsDfcore *self)
{
   int i, p;
   basic_block bb;
   void *problem_temps;

   auto_bitmap tmp (&self->df_bitmap_obstack);
   for (p = 0; p < df->num_problems_defined; p++){
      struct dataflow *dflow = df->problems_in_order[p];

      /* Need to reorganize the out_of_date_transfer_functions for the
      dflow problem.  */
      if (dflow->out_of_date_transfer_functions){
         bitmap_copy (tmp, dflow->out_of_date_transfer_functions);
         bitmap_clear (dflow->out_of_date_transfer_functions);
         if (bitmap_bit_p (tmp, ENTRY_BLOCK))
            bitmap_set_bit (dflow->out_of_date_transfer_functions, ENTRY_BLOCK);
         if (bitmap_bit_p (tmp, EXIT_BLOCK))
            bitmap_set_bit (dflow->out_of_date_transfer_functions, EXIT_BLOCK);

         i = NUM_FIXED_BLOCKS;
         FOR_EACH_BB_FN (bb, cfun){
            if (bitmap_bit_p (tmp, bb->index))
               bitmap_set_bit (dflow->out_of_date_transfer_functions, i);
            i++;
         }
      }

      /* Now shuffle the block info for the problem.  */
      if (dflow->problem->free_bb_fun){
         int size = (last_basic_block_for_fn (cfun) * dflow->problem->block_info_elt_size);
         problem_temps = XNEWVAR (char, size);
         //n_debug("mtcsdfcore.c mtcs_dfcore_df_compact_blocks 扩大 dataflow中的blocksize %d %d\n",
             //  last_basic_block_for_fn (cfun),dflow->problem->block_info_elt_size);
         mtcs_dfcore_df_grow_bb_info/*!df_grow_bb_info*/(self,dflow);
         memcpy (problem_temps, dflow->block_info, size);

         /* Copy the bb info from the problem tmps to the proper
         place in the block_info vector.  Null out the copied
         item.  The entry and exit blocks never move.  */
         i = NUM_FIXED_BLOCKS;
         FOR_EACH_BB_FN (bb, cfun){
            df_set_bb_info(self,dflow, i, (char *)problem_temps + bb->index * dflow->problem->block_info_elt_size);
            i++;
         }
         memset ((char *)dflow->block_info + i * dflow->problem->block_info_elt_size, 0,
               (last_basic_block_for_fn (cfun) - i) * dflow->problem->block_info_elt_size);
         free (problem_temps);
      }
   }

   /* Shuffle the bits in the basic_block indexed arrays.  */

   if (df->blocks_to_analyze){
      if (bitmap_bit_p (tmp, ENTRY_BLOCK))
         bitmap_set_bit (df->blocks_to_analyze, ENTRY_BLOCK);
      if (bitmap_bit_p (tmp, EXIT_BLOCK))
         bitmap_set_bit (df->blocks_to_analyze, EXIT_BLOCK);
      bitmap_copy (tmp, df->blocks_to_analyze);
      bitmap_clear (df->blocks_to_analyze);
      i = NUM_FIXED_BLOCKS;
      FOR_EACH_BB_FN (bb, cfun){
         if (bitmap_bit_p (tmp, bb->index))
            bitmap_set_bit (df->blocks_to_analyze, i);
         i++;
      }
   }

   i = NUM_FIXED_BLOCKS;
   FOR_EACH_BB_FN (bb, cfun){
      SET_BASIC_BLOCK_FOR_FN (cfun, i, bb);
      bb->index = i;
      i++;
   }

   gcc_assert (i == n_basic_blocks_for_fn (cfun));

   for (; i < last_basic_block_for_fn (cfun); i++)
      SET_BASIC_BLOCK_FOR_FN (cfun, i, NULL);

#ifdef DF_DEBUG_CFG
   if (!df_lr->solutions_dirty)
      df_set_clean_cfg(self);
#endif
}


/* Shove NEW_BLOCK in at OLD_INDEX.  Called from ifcvt to hack a
   block.  There is no excuse for people to do this kind of thing.  */
//原型 df_bb_replace df.h df-core.cc
void mtcs_dfcore_df_bb_replace (MtcsDfcore *self,int old_index, basic_block new_block)
{
   int new_block_index = new_block->index;
   int p;

   if (dump_file)
      fprintf (dump_file, "shoving block %d into %d\n", new_block_index, old_index);

   gcc_assert (df);
   gcc_assert (BASIC_BLOCK_FOR_FN (cfun, old_index) == NULL);

   for (p = 0; p < df->num_problems_defined; p++){
      struct dataflow *dflow = df->problems_in_order[p];
      if (dflow->block_info){
         n_debug("mtcsdfcore.c mtcs_dfcore_df_bb_replace  dataflow中的blocksize %d %d\n",
               last_basic_block_for_fn (cfun),dflow->problem->block_info_elt_size);
         mtcs_dfcore_df_grow_bb_info/*!df_grow_bb_info*/(self,dflow);
         df_set_bb_info(self,dflow, old_index, df_get_bb_info(self,dflow, new_block_index));
      }
   }

   df_clear_bb_dirty(self,new_block);
   SET_BASIC_BLOCK_FOR_FN (cfun, old_index, new_block);
   new_block->index = old_index;
   mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(self,BASIC_BLOCK_FOR_FN (cfun, old_index));
   SET_BASIC_BLOCK_FOR_FN (cfun, new_block_index, NULL);
}


/* Free all of the per basic block dataflow from all of the problems.
   This is typically called before a basic block is deleted and the
   problem will be reanalyzed.  */
//原型 df_bb_delete df.h df-core.cc
void mtcs_dfcore_df_bb_delete (MtcsDfcore *self,int bb_index)
{
   basic_block bb = BASIC_BLOCK_FOR_FN (cfun, bb_index);
   int i;
   if (!df)
      return;
   for (i = 0; i < df->num_problems_defined; i++) {
      struct dataflow *dflow = df->problems_in_order[i];
      if (dflow->problem->free_bb_fun){
         void *bb_info = df_get_bb_info(self,dflow, bb_index);
         if (bb_info){
            dflow->problem->free_bb_fun (bb, bb_info);
            df_clear_bb_info(self,dflow, bb_index);
         }
      }
   }
   df_clear_bb_dirty(self,bb);
   df_mark_solutions_dirty ();
}


/* Verify that there is a place for everything and everything is in
   its place.  This is too expensive to run after every pass in the
   mainline.  However this is an excellent debugging tool if the
   dataflow information is not being updated properly.  You can just
   sprinkle calls in until you find the place that is changing an
   underlying structure without calling the proper updating
   routine.  */
//原型 df_verify df.h df-core.cc
void mtcs_dfcore_df_verify (MtcsDfcore *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfproblems *mtcsDfproblems=mtcs_target_get_dfproblems(mtcsTarget);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);

   mtcs_dfscan_df_scan_verify/*!df_scan_verify*/(mtcsDfscan);
#ifdef ENABLE_DF_CHECKING
   mtcs_dfproblems_df_lr_verify_transfer_functions/*!df_lr_verify_transfer_functions*/(mtcsDfproblems);
   if (df_live)
      mtcs_dfproblems_df_live_verify_transfer_functions/*!df_live_verify_transfer_functions*/(mtcsDfproblems);
#endif
   df->changeable_flags &= ~DF_VERIFY_SCHEDULED;
}

#ifdef DF_DEBUG_CFG

/* Compute an array of ints that describes the cfg.  This can be used
   to discover places where the cfg is modified by the appropriate
   calls have not been made to the keep df informed.  The internals of
   this are unexciting, the key is that two instances of this can be
   compared to see if any changes have been made to the cfg.  */

static int * df_compute_cfg_image (MtcsDfcore *self)
{
   basic_block bb;
   int size = 2 + (2 * n_basic_blocks_for_fn (cfun));
   int i;
   int * map;

   FOR_ALL_BB_FN (bb, cfun){
      size += EDGE_COUNT (bb->succs);
   }

   map = XNEWVEC (int, size);
   map[0] = size;
   i = 1;
   FOR_ALL_BB_FN (bb, cfun){
      edge_iterator ei;
      edge e;

      map[i++] = bb->index;
      FOR_EACH_EDGE (e, ei, bb->succs)
         map[i++] = e->dest->index;
      map[i++] = -1;
   }
   map[i] = -1;
   return map;
}

static int *saved_cfg = NULL;


/* This function compares the saved version of the cfg with the
   current cfg and aborts if the two are identical.  The function
   silently returns if the cfg has been marked as dirty or the two are
   the same.  */
//原型 df_check_cfg_clean df.h df-core.cc
void mtcs_dfcore_df_check_cfg_clean (MtcsDfcore *self)
{
  int *new_map;

  if (!df)
    return;

  if (df_lr->solutions_dirty)
    return;

  if (saved_cfg == NULL)
    return;

  new_map = df_compute_cfg_image(self);
  gcc_assert (memcmp (saved_cfg, new_map, saved_cfg[0] * sizeof (int)) == 0);
  free (new_map);
}


/* This function builds a cfg fingerprint and squirrels it away in
   saved_cfg.  */

static void df_set_clean_cfg (MtcsDfcore *self)
{
  free (saved_cfg);
  saved_cfg = df_compute_cfg_image(self);
}

#endif /* DF_DEBUG_CFG  */
/*----------------------------------------------------------------------------
   PUBLIC INTERFACES TO QUERY INFORMATION.
----------------------------------------------------------------------------*/

/* Return first def of REGNO within BB.  */
//原型 df_bb_regno_first_def_find df.h df-core.cc
df_ref mtcs_dfcore_df_bb_regno_first_def_find (MtcsDfcore *self,basic_block bb, unsigned int regno)
{
   rtx_insn *insn;
   df_ref def;

   FOR_BB_INSNS (bb, insn){
      if (!INSN_P (insn))
         continue;

      FOR_EACH_INSN_DEF (def, insn)
         if (DF_REF_REGNO (def) == regno)
            return def;
   }
   return NULL;
}

/* Return last def of REGNO within BB.  */
//原型 df_bb_regno_last_def_find df.h df-core.cc
df_ref mtcs_dfcore_df_bb_regno_last_def_find (MtcsDfcore *self,basic_block bb, unsigned int regno)
{
   rtx_insn *insn;
   df_ref def;
   FOR_BB_INSNS_REVERSE (bb, insn){
      if (!INSN_P (insn))
         continue;

      FOR_EACH_INSN_DEF (def, insn)
         if (DF_REF_REGNO (def) == regno)
            return def;
   }
   return NULL;
}

/* Finds the reference corresponding to the definition of REG in INSN.
   DF is the dataflow object.  */
//原型 df_find_def df.h df-core.cc
df_ref mtcs_dfcore_df_find_def (MtcsDfcore *self,rtx_insn *insn, rtx reg)
{
  df_ref def;

  if (GET_CODE (reg) == SUBREG)
    reg = SUBREG_REG (reg);
  gcc_assert (REG_P (reg));

  FOR_EACH_INSN_DEF (def, insn)
    if (DF_REF_REGNO (def) == REGNO (reg))
      return def;

  return NULL;
}

/* Return true if REG is defined in INSN, zero otherwise.  */
//原型 df_reg_defined df.h df-core.cc
bool mtcs_dfcore_df_reg_defined (MtcsDfcore *self,rtx_insn *insn, rtx reg)
{
  return mtcs_dfcore_df_find_def/*!df_find_def*/(self,insn, reg) != NULL;
}

/* Finds the reference corresponding to the use of REG in INSN.
   DF is the dataflow object.  */
//原型 df_find_use df.h df-core.cc
df_ref mtcs_dfcore_df_find_use (MtcsDfcore *self,rtx_insn *insn, rtx reg)
{
   df_ref use;

   if (GET_CODE (reg) == SUBREG)
      reg = SUBREG_REG (reg);
   gcc_assert (REG_P (reg));

   df_insn_info *insn_info = DF_INSN_INFO_GET (insn);
   FOR_EACH_INSN_INFO_USE (use, insn_info)
      if (DF_REF_REGNO (use) == REGNO (reg))
         return use;
   if (df->changeable_flags & DF_EQ_NOTES)
      FOR_EACH_INSN_INFO_EQ_USE (use, insn_info)
         if (DF_REF_REGNO (use) == REGNO (reg))
            return use;
   return NULL;
}


/* Return true if REG is referenced in INSN, zero otherwise.  */
//原型 df_reg_used df.h df-core.cc
bool mtcs_dfcore_df_reg_used (MtcsDfcore *self,rtx_insn *insn, rtx reg)
{
  return mtcs_dfcore_df_find_use/*!df_find_use*/(self,insn, reg) != NULL;
}

/* If REG has a single definition, return its known value, otherwise return
   null.  */
//原型 df_find_single_def_src df.h df-core.cc
rtx mtcs_dfcore_df_find_single_def_src (MtcsDfcore *self,rtx reg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReload1 *mtcsReload1=mtcs_target_get_reload1(mtcsTarget);

   rtx src = NULL_RTX;

   /* Don't look through unbounded number of single definition REG copies,
   there might be loops for sources with uninitialized variables.  */
   for (int cnt = 0; cnt < 128; cnt++){
      df_ref adef = DF_REG_DEF_CHAIN (REGNO (reg));
      if (adef == NULL || DF_REF_NEXT_REG (adef) != NULL
      || DF_REF_IS_ARTIFICIAL (adef)
      || (DF_REF_FLAGS (adef)  & (DF_REF_PARTIAL | DF_REF_CONDITIONAL)))
         return NULL_RTX;

      rtx set = single_set (DF_REF_INSN (adef));
      if (set == NULL || !rtx_equal_p (SET_DEST (set), reg))
         return NULL_RTX;

      rtx note = find_reg_equal_equiv_note (DF_REF_INSN (adef));
      if (note && mtcs_reload1_function_invariant_p/*!function_invariant_p*/(mtcsReload1,XEXP (note, 0)))
         return XEXP (note, 0);
      src = SET_SRC (set);

      if (REG_P (src)){
         reg = src;
         continue;
      }
      break;
   }
   if (!mtcs_reload1_function_invariant_p/*!function_invariant_p*/(mtcsReload1,src))
      return NULL_RTX;

   return src;
}


/*----------------------------------------------------------------------------
   Debugging and printing functions.
----------------------------------------------------------------------------*/

/* Write information about registers and basic blocks into FILE.
   This is part of making a debugging dump.  */
//原型 dump_regset df.h df-core.cc
void mtcs_dfcore_dump_regset (MtcsDfcore *self,regset r, FILE *outf)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   unsigned i;
   reg_set_iterator rsi;

   if (r == NULL){
      fputs (" (nil)", outf);
      return;
   }
   EXECUTE_IF_SET_IN_REG_SET (r, 0, i, rsi){
      fprintf (outf, " %d", i);
      if (i < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg))
         fprintf (outf, " [%s]",mtcsReg->hardRegs.x_reg_names/*!reg_names*/[i]);
   }
}

/* Print a human-readable representation of R on the standard error
   stream.  This function is designed to be used from within the
   debugger.  */
extern void debug_regset (regset);
//原型 debug_regset df.h df-core.cc
DEBUG_FUNCTION void mtcs_dfcore_debug_regset (MtcsDfcore *self,regset r)
{
   mtcs_dfcore_dump_regset/*!dump_regset*/(self,r, stderr);
  putc ('\n', stderr);
}

/* Write information about registers and basic blocks into FILE.
   This is part of making a debugging dump.  */
//原型 df_print_regset df.h df-core.cc
void mtcs_dfcore_df_print_regset (MtcsDfcore *self,FILE *file, const_bitmap r)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   unsigned int i;
   bitmap_iterator bi;

   if (r == NULL)
      fputs (" (nil)", file);
   else{
      EXECUTE_IF_SET_IN_BITMAP (r, 0, i, bi){
         fprintf (file, " %d", i);
         if (i < mtcs_reg_get_first_pseudo_register(mtcsReg))
            fprintf (file, " [%s]", mtcsReg->hardRegs.x_reg_names/*!reg_names*/[i]);
      }
   }
   fprintf (file, "\n");
}


/* Write information about registers and basic blocks into FILE.  The
   bitmap is in the form used by df_byte_lr.  This is part of making a
   debugging dump.  */
//原型 df_print_word_regset df.h df-core.cc
void mtcs_dfcore_df_print_word_regset (MtcsDfcore *self,FILE *file, const_bitmap r)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   unsigned int max_reg = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);

   if (r == NULL)
      fputs (" (nil)", file);
   else{
      unsigned int i;
      for (i = mtcs_reg_get_first_pseudo_register(mtcsReg); i < max_reg; i++){
         bool found = (bitmap_bit_p (r, 2 * i) || bitmap_bit_p (r, 2 * i + 1));
         if (found){
            int word;
            const char * sep = "";
            fprintf (file, " %d", i);
            fprintf (file, "(");
            for (word = 0; word < 2; word++)
               if (bitmap_bit_p (r, 2 * i + word)){
                  fprintf (file, "%s%d", sep, word);
                  sep = ", ";
               }
            fprintf (file, ")");
         }
      }
   }
   fprintf (file, "\n");
}


/* Dump dataflow info.  */
//原型 df_dump df.h df-core.cc
void mtcs_dfcore_df_dump (MtcsDfcore *self,FILE *file)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfproblems *mtcsDfproblems=mtcs_target_get_dfproblems(mtcsTarget);

   basic_block bb;
   mtcs_dfcore_df_dump_start/*!df_dump_start*/(self,file);

   FOR_ALL_BB_FN (bb, cfun){
      mtcs_dfproblems_df_print_bb_index/*!df_print_bb_index*/(mtcsDfproblems,bb, file);
      mtcs_dfcore_df_dump_top/*!df_dump_top*/(self,bb, file);
      mtcs_dfcore_df_dump_bottom/*!df_dump_bottom*/(self,bb, file);
   }

   fprintf (file, "\n");
}


/* Dump dataflow info for df->blocks_to_analyze.  */
//原型 df_dump_region df.h df-core.cc
void mtcs_dfcore_df_dump_region (MtcsDfcore *self,FILE *file)
{
   if (df->blocks_to_analyze){
      bitmap_iterator bi;
      unsigned int bb_index;

      fprintf (file, "\n\nstarting region dump\n");
      mtcs_dfcore_df_dump_start/*!df_dump_start*/(self,file);

      EXECUTE_IF_SET_IN_BITMAP (df->blocks_to_analyze, 0, bb_index, bi){
         basic_block bb = BASIC_BLOCK_FOR_FN (cfun, bb_index);
         dump_bb (file, bb, 0, TDF_DETAILS);
      }
      fprintf (file, "\n");
   }else
      mtcs_dfcore_df_dump/*!df_dump*/(self,file);
}


/* Dump the introductory information for each problem defined.  */
//原型 df_dump_start df.h df-core.cc
void mtcs_dfcore_df_dump_start (MtcsDfcore *self,FILE *file)
{
   int i;

   if (!df || !file)
      return;

   fprintf (file, "\n\n%s\n", current_function_name ());
   fprintf (file, "\nDataflow summary:\n");
   if (df->blocks_to_analyze)
      fprintf (file, "def_info->table_size = %d, use_info->table_size = %d\n",
            DF_DEFS_TABLE_SIZE (), DF_USES_TABLE_SIZE ());

   for (i = 0; i < df->num_problems_defined; i++){
      struct dataflow *dflow = df->problems_in_order[i];
      if (dflow->computed){
         df_dump_problem_function fun = dflow->problem->dump_start_fun;
         if (fun)
            fun (file);
      }
   }
}

/* Dump the top or bottom of the block information for BB.  */
static void df_dump_bb_problem_data (MtcsDfcore *self,basic_block bb, FILE *file, bool top)
{
   int i;

   if (!df || !file)
   return;

   for (i = 0; i < df->num_problems_defined; i++){
      struct dataflow *dflow = df->problems_in_order[i];
      if (dflow->computed){
         df_dump_bb_problem_function bbfun;

         if (top)
            bbfun = dflow->problem->dump_top_fun;
         else
            bbfun = dflow->problem->dump_bottom_fun;

         if (bbfun)
            bbfun (bb, file);
      }
   }
}

/* Dump the top of the block information for BB.  */
//原型 df_dump_top df.h df-core.cc
void mtcs_dfcore_df_dump_top (MtcsDfcore *self,basic_block bb, FILE *file)
{
   df_dump_bb_problem_data(self,bb, file, /*top=*/true);
}

/* Dump the bottom of the block information for BB.  */
//原型 df_dump_bottom df.h df-core.cc
void mtcs_dfcore_df_dump_bottom (MtcsDfcore *self,basic_block bb, FILE *file)
{
   df_dump_bb_problem_data(self,bb, file, /*top=*/false);
}

/* Dump information about INSN just before or after dumping INSN itself.  */
static void df_dump_insn_problem_data (MtcsDfcore *self,const rtx_insn *insn, FILE *file, bool top)
{
   int i;

   if (!df || !file)
      return;

   for (i = 0; i < df->num_problems_defined; i++){
      struct dataflow *dflow = df->problems_in_order[i];
      if (dflow->computed){
         df_dump_insn_problem_function insnfun;

         if (top)
            insnfun = dflow->problem->dump_insn_top_fun;
         else
            insnfun = dflow->problem->dump_insn_bottom_fun;

         if (insnfun)
            insnfun (insn, file);
      }
   }
}

/* Dump information about INSN before dumping INSN itself.  */
//原型 df_dump_insn_top df.h df-core.cc
void mtcs_dfcore_df_dump_insn_top (MtcsDfcore *self,const rtx_insn *insn, FILE *file)
{
   df_dump_insn_problem_data(self,insn,  file, /*top=*/true);
}

/* Dump information about INSN after dumping INSN itself.  */
//原型 df_dump_insn_bottom df.h df-core.cc
void mtcs_dfcore_df_dump_insn_bottom (MtcsDfcore *self,const rtx_insn *insn, FILE *file)
{
   df_dump_insn_problem_data(self,insn,  file, /*top=*/false);
}

static void df_ref_dump (MtcsDfcore *self,df_ref ref, FILE *file)
{
  fprintf (file, "%c%d(%d)",
      DF_REF_REG_DEF_P (ref)
      ? 'd'
      : (DF_REF_FLAGS (ref) & DF_REF_IN_NOTE) ? 'e' : 'u',
      DF_REF_ID (ref),
      DF_REF_REGNO (ref));
}

//原型 df_refs_chain_dump df.h df-core.cc
void mtcs_dfcore_df_refs_chain_dump (MtcsDfcore *self,df_ref ref, bool follow_chain, FILE *file)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfproblems *mtcsDfproblems=mtcs_target_get_dfproblems(mtcsTarget);

   fprintf (file, "{ ");
   for (; ref; ref = DF_REF_NEXT_LOC (ref)){
      df_ref_dump(self,ref, file);
      if (follow_chain)
         mtcs_dfproblems_df_chain_dump/*!df_chain_dump*/(mtcsDfproblems,DF_REF_CHAIN (ref), file);
   }
   fprintf (file, "}");
}


/* Dump either a ref-def or reg-use chain.  */
//原型 df_regs_chain_dump df.h df-core.cc
void mtcs_dfcore_df_regs_chain_dump (MtcsDfcore *self,df_ref ref,  FILE *file)
{
   fprintf (file, "{ ");
   while (ref){
      df_ref_dump(self,ref, file);
      ref = DF_REF_NEXT_REG (ref);
   }
   fprintf (file, "}");
}

static void df_mws_dump (MtcsDfcore *self,struct df_mw_hardreg *mws, FILE *file)
{
   for (; mws; mws = DF_MWS_NEXT (mws))
      fprintf (file, "mw %c r[%d..%d]\n", DF_MWS_REG_DEF_P (mws) ? 'd' : 'u',mws->start_regno, mws->end_regno);
}

static void df_insn_uid_debug (MtcsDfcore *self,unsigned int uid,bool follow_chain, FILE *file)
{
   fprintf (file, "insn %d luid %d",uid, DF_INSN_UID_LUID (uid));

   if (DF_INSN_UID_DEFS (uid)){
      fprintf (file, " defs ");
      mtcs_dfcore_df_refs_chain_dump/*!df_refs_chain_dump*/(self,DF_INSN_UID_DEFS (uid), follow_chain, file);
   }

   if (DF_INSN_UID_USES (uid)){
      fprintf (file, " uses ");
      mtcs_dfcore_df_refs_chain_dump/*!df_refs_chain_dump*/(self,DF_INSN_UID_USES (uid), follow_chain, file);
   }

   if (DF_INSN_UID_EQ_USES (uid)){
      fprintf (file, " eq uses ");
      mtcs_dfcore_df_refs_chain_dump/*!df_refs_chain_dump*/(self,DF_INSN_UID_EQ_USES (uid), follow_chain, file);
   }

   if (DF_INSN_UID_MWS (uid)){
      fprintf (file, " mws ");
      df_mws_dump(self,DF_INSN_UID_MWS (uid), file);
   }
   fprintf (file, "\n");
}

//原型 df_insn_debug df.h df-core.cc
DEBUG_FUNCTION void mtcs_dfcore_df_insn_debug (MtcsDfcore *self,rtx_insn *insn, bool follow_chain, FILE *file)
{
   df_insn_uid_debug(self,INSN_UID (insn), follow_chain, file);
}

//原型 df_insn_debug_regno df.h df-core.cc
DEBUG_FUNCTION void mtcs_dfcore_df_insn_debug_regno (MtcsDfcore *self,rtx_insn *insn, FILE *file)
{
   struct df_insn_info *insn_info = DF_INSN_INFO_GET (insn);

   fprintf (file, "insn %d bb %d luid %d defs ",
         INSN_UID (insn), BLOCK_FOR_INSN (insn)->index,DF_INSN_INFO_LUID (insn_info));
   mtcs_dfcore_df_refs_chain_dump/*!df_refs_chain_dump*/(self,DF_INSN_INFO_DEFS (insn_info), false, file);

   fprintf (file, " uses ");
   mtcs_dfcore_df_refs_chain_dump/*!df_refs_chain_dump*/(self,DF_INSN_INFO_USES (insn_info), false, file);

   fprintf (file, " eq_uses ");
   mtcs_dfcore_df_refs_chain_dump/*!df_refs_chain_dump*/(self,DF_INSN_INFO_EQ_USES (insn_info), false, file);
   fprintf (file, "\n");
}

//原型 df_regno_debug df.h df-core.cc
DEBUG_FUNCTION void mtcs_dfcore_df_regno_debug (MtcsDfcore *self,unsigned int regno, FILE *file)
{
   fprintf (file, "reg %d defs ", regno);
   mtcs_dfcore_df_regs_chain_dump/*!df_regs_chain_dump*/(self,DF_REG_DEF_CHAIN (regno), file);
   fprintf (file, " uses ");
   mtcs_dfcore_df_regs_chain_dump/*!df_regs_chain_dump*/(self,DF_REG_USE_CHAIN (regno), file);
   fprintf (file, " eq_uses ");
   mtcs_dfcore_df_regs_chain_dump/*!df_regs_chain_dump*/(self,DF_REG_EQ_USE_CHAIN (regno), file);
   fprintf (file, "\n");
}

//原型 df_ref_debug  df.h df-core.cc
DEBUG_FUNCTION void mtcs_dfcore_df_ref_debug (MtcsDfcore *self,df_ref ref, FILE *file)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfproblems *mtcsDfproblems=mtcs_target_get_dfproblems(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   fprintf (file, "%c%d ",DF_REF_REG_DEF_P (ref) ? 'd' : 'u',DF_REF_ID (ref));
   fprintf (file, "reg %d bb %d insn %d flag %#x type %#x ",
                  DF_REF_REGNO (ref),
                  DF_REF_BBNO (ref),
                  DF_REF_IS_ARTIFICIAL (ref) ? -1 : DF_REF_INSN_UID (ref),
                  DF_REF_FLAGS (ref),
                  DF_REF_TYPE (ref));
   if (DF_REF_LOC (ref)){
      if (mtcsOptionsItem->x_flag_dump_noaddr)
         fprintf (file, "loc #(#) chain ");
      else
         fprintf (file, "loc %p(%p) chain ", (void *)DF_REF_LOC (ref), (void *)*DF_REF_LOC (ref));
   }else
      fprintf (file, "chain ");
   mtcs_dfproblems_df_chain_dump/*!df_chain_dump*/(mtcsDfproblems,DF_REF_CHAIN (ref), file);
   fprintf (file, "\n");
}

/* Functions for debugging from GDB.  */
//原型 debug_df_insn  df.h df-core.cc
DEBUG_FUNCTION void mtcs_dfcore_debug_df_insn (MtcsDfcore *self,rtx_insn *insn)
{
   mtcs_dfcore_df_insn_debug/*!df_insn_debug*/(self,insn, true, stderr);
   mtcs_debug_rtx/*!debug_rtx*/(insn);
}

//原型 debug_df_reg  df.h df-core.cc
DEBUG_FUNCTION void mtcs_dfcore_debug_df_reg (MtcsDfcore *self,rtx reg)
{
   mtcs_dfcore_df_regno_debug/*!df_regno_debug*/(self,REGNO (reg), stderr);
}

//原型 debug_df_regno  df.h df-core.cc
DEBUG_FUNCTION void mtcs_dfcore_debug_df_regno (MtcsDfcore *self,unsigned int regno)
{
   mtcs_dfcore_df_regno_debug/*!df_regno_debug*/(self,regno, stderr);
}

//原型 debug_df_ref   df.h df-core.cc
DEBUG_FUNCTION void mtcs_dfcore_debug_df_ref (MtcsDfcore *self,df_ref ref)
{
   mtcs_dfcore_df_ref_debug/*!df_ref_debug*/(self,ref, stderr);
}

//原型 debug_df_defno   df.h df-core.cc
DEBUG_FUNCTION void mtcs_dfcore_debug_df_defno (MtcsDfcore *self,unsigned int defno)
{
   mtcs_dfcore_df_ref_debug/*!df_ref_debug*/(self,DF_DEFS_GET (defno), stderr);
}

//原型 debug_df_useno   df.h df-core.cc
DEBUG_FUNCTION void mtcs_dfcore_debug_df_useno (MtcsDfcore *self,unsigned int defno)
{
   mtcs_dfcore_df_ref_debug/*!df_ref_debug*/(self,DF_USES_GET (defno), stderr);
}

//原型 debug_df_chain   df.h df-core.cc
DEBUG_FUNCTION void mtcs_dfcore_debug_df_chain (MtcsDfcore *self,struct df_link *link)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfproblems *mtcsDfproblems=mtcs_target_get_dfproblems(mtcsTarget);

   mtcs_dfproblems_df_chain_dump/*!df_chain_dump*/(mtcsDfproblems,link, stderr);
   fputc ('\n', stderr);
}

static void mtcsDfcoreInit(MtcsDfcore *self)
{

}

MtcsDfcore *mtcs_dfcore_new(MtcsMode *mtcsMode)
{
   MtcsDfcore *self = n_slice_alloc0 (sizeof(MtcsDfcore));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsDfcoreInit(self);
   return self;
}

//原型 NEXT_PASS (pass_df_initialize_opt, 1); RTL_PASS df-core.cc dfinit n 有条件执行 optimize > 0 rest_of_handle_df_initialize
static nboolean df_initialize_opt_gate_cb(MtcsPass *mtcsPass,function *fun)
{
    MtcsPassDfInitializeOpt *self=(MtcsPassDfInitializeOpt *)mtcsPass;
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
    MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
    return mtcsOptionsItem->x_optimize>0;
}

static nuint df_initialize_opt_execute_cb(MtcsPass *mtcsPass,function *func)
{
     MtcsPassDfInitializeOpt *self=(MtcsPassDfInitializeOpt *)mtcsPass;
     MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
     MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
     MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
     return rest_of_handle_df_initialize(mtcsDfcore);
}

static void mtcsPassDfInitializeOptInit(MtcsPassDfInitializeOpt *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =df_initialize_opt_execute_cb;
    mtcsPass->gate =df_initialize_opt_gate_cb;

    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassDfInitializeOpt *mtcs_pass_df_initialize_opt_new(MtcsMode *mtcsMode,int num)
{
     MtcsPassDfInitializeOpt *self = n_slice_alloc0 (sizeof(MtcsPassDfInitializeOpt));
     mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
     mtcs_pass_init((MtcsPass *)self,RTL_PASS,"dfinit");
     mtcsPassDfInitializeOptInit(self);
     return self;
}


//原型 NEXT_PASS (pass_df_initialize_no_opt, 1); RTL_PASS df-core.cc no-opt dfinit n 有条件执行 optimize == 0 rest_of_handle_df_initialize
static nboolean df_initialize_no_opt_gate_cb(MtcsPass *mtcsPass,function *fun)
{
    MtcsPassDfInitializeOpt *self=(MtcsPassDfInitializeOpt *)mtcsPass;
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
    MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
    return mtcsOptionsItem->x_optimize==0;
}

static nuint df_initialize_no_opt_execute_cb(MtcsPass *mtcsPass,function *func)
{
     MtcsPassDfInitializeOpt *self=(MtcsPassDfInitializeOpt *)mtcsPass;
     MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
     MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
     MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
     return rest_of_handle_df_initialize(mtcsDfcore);
}

static void mtcsPassDfInitializeNoOptInit(MtcsPassDfInitializeNoOpt *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =df_initialize_no_opt_execute_cb;
    mtcsPass->gate =df_initialize_no_opt_gate_cb;

    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassDfInitializeNoOpt *mtcs_pass_df_initialize_no_opt_new(MtcsMode *mtcsMode)
{
     MtcsPassDfInitializeNoOpt *self = n_slice_alloc0 (sizeof(MtcsPassDfInitializeNoOpt));
     mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
     mtcs_pass_init((MtcsPass *)self,RTL_PASS,"no-opt dfinit");
     mtcsPassDfInitializeNoOptInit(self);
     return self;
}

//原型 NEXT_PASS (pass_df_finish, 1); RTL_PASS df-core.cc dfinish  n 无条件执行 rest_of_handle_df_finish
static nuint df_finish_execute_cb(MtcsPass *mtcsPass,function *func)
{
     MtcsPassDfInitializeOpt *self=(MtcsPassDfInitializeOpt *)mtcsPass;
     MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
     MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
     MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
     return rest_of_handle_df_finish(mtcsDfcore);
}

static void mtcsPassDfFinishInit(MtcsPassDfFinish *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =df_finish_execute_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassDfFinish *mtcs_pass_df_finish_new(MtcsMode *mtcsMode)
{
    MtcsPassDfFinish *self = n_slice_alloc0 (sizeof(MtcsPassDfFinish));
     mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
     mtcs_pass_init((MtcsPass *)self,RTL_PASS,"dfinish");
     mtcsPassDfFinishInit(self);
     return self;
}
