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
 * base on cfgcontext.cc
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

#include "pretty-print.h"
#include "diagnostic-core.h"
#include "dumpfile.h"
#include "cfganal.h"
#include "tree.h"
#include "tree-ssa.h"
#include "cfgloop.h"
#include "sreal.h"
#include "profile.h"


#include "aet/aetprinttree.h"
#include "mtcscfgcontext.h"
#include "mtcstarget.h"
#include "mtcscompile.h"
#include "mtcscfgrtlstate.h"
#include "mtcscfglayoutstate.h"
#include "mtcscfggimplestate.h"
#include "mtcsprintrtl.h"

static void test_rtl_verify_edges (basic_block bb)
{
   if(!n_log_is_debug())
      return;
   int  n_branch = 0;
   int n_eh = 0, n_abnormal = 0;
   edge e, fallthru = NULL;
   edge_iterator ei;

   if(bb->index==14 || bb->index==15 || bb->index==16 || bb->index==17) {
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
      n_debug("mtcscfgcontext.c split_block rtl_verify_edges 00 bb:%p index:%d n_branch:%d BB_END (bb):%p any_uncondjump_p:%d\n",
            bb,bb->index,n_branch,BB_END (bb),re);
      if(BB_END(bb)){
         rtx_insn *insn;
         enum rtx_code code;

         insn = BB_END (bb);
         code = GET_CODE (insn);
         /* A branch.  */
         if (code == JUMP_INSN){
            n_debug("mtcscfgcontext.csplit_block rtl_verify_edges 11 bb:%p index:%d BB_END (bb):%p uid:%d\n",
                  bb,bb->index,insn,INSN_UID (insn));
            mtcs_print_rtl_single(stderr,insn);
         }else{
            n_debug("mtcscfgcontext.c split_block rtl_verify_edges 22 不是 jump_insn bb:%p index:%d BB_END (bb):%p uid:%d\n",
                  bb,bb->index,insn,INSN_UID (insn) );
            mtcs_print_rtl_single(stderr,insn);
         }
      }
   }
}

/* Splits basic block BB after the specified instruction I (but at least after
   the labels).  If I is NULL, splits just after labels.  The newly created edge
   is returned.  The new basic block is created just after the old one.  */
//原型 split_block_1 cfghooks.cc
static edge split_block_1 (MtcsCfgContext *self,basic_block bb, void *i)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsCfg *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
   MtcsCfgState *mtcsCfgState=self->current;

   basic_block new_bb;
   edge res;

   if (!mtcsCfgState->split_block)
      internal_error ("%s does not support split_block", mtcsCfgState->name);
   n_debug("mtcscfgcontext.c split_block_1 00 aa bb:%p %d\n",bb,bb->index);
   test_rtl_verify_edges(bb);
   new_bb = mtcsCfgState->split_block(mtcsCfgState,bb, i);
   if (!new_bb)
      return NULL;

   new_bb->count = bb->count;
   n_debug("mtcscfgcontext.c split_block_1 00 new_bb:%p new_bb->count:%d index:%d bb:%p\n",new_bb,new_bb->count,new_bb->index,bb);
   test_rtl_verify_edges(new_bb);

   if (dom_info_available_p (CDI_DOMINATORS)){
      n_debug("mtcscfgcontext.c split_block_1 11 new_bb:%p new_bb->count:%d index:%d bb:%p\n",new_bb,new_bb->count,new_bb->index,bb);

      redirect_immediate_dominators (CDI_DOMINATORS, bb, new_bb);
      set_immediate_dominator (CDI_DOMINATORS, new_bb, bb);
   }

   if (current_loops != NULL){
      edge_iterator ei;
      edge e;
      add_bb_to_loop (new_bb, bb->loop_father);
      n_debug("mtcscfgcontext.c split_block_1 22 new_bb:%p new_bb->count:%d index:%d bb:%p\n",new_bb,new_bb->count,new_bb->index,bb);

      /* Identify all loops bb may have been the latch of and adjust them.  */
      FOR_EACH_EDGE (e, ei, new_bb->succs)
         if (e->dest->loop_father->latch == bb){
            n_debug("mtcscfgcontext.c split_block_1 33 new_bb:%p new_bb->count:%d index:%d bb:%p\n",new_bb,new_bb->count,new_bb->index,bb);

            e->dest->loop_father->latch = new_bb;
         }
   }

   res = mtcs_cfg_make_single_succ_edge/*!make_single_succ_edge*/(mtcsCfg,bb, new_bb, EDGE_FALLTHRU);
   n_debug("mtcscfgcontext.c split_block_1 44 new_bb:%p new_bb->count:%d index:%d bb:%p res:%p\n",
         new_bb,new_bb->count,new_bb->index,bb,res);
   test_rtl_verify_edges(new_bb);

   if (bb->flags & BB_IRREDUCIBLE_LOOP){
      new_bb->flags |= BB_IRREDUCIBLE_LOOP;
      res->flags |= EDGE_IRREDUCIBLE_LOOP;
      n_debug("mtcscfgcontext.c split_block_1 55 new_bb:%p new_bb->count:%d index:%d bb:%p res:%p\n",
            new_bb,new_bb->count,new_bb->index,bb,res);
      test_rtl_verify_edges(new_bb);

   }

   return res;
}


/* Creates a new basic block just after the basic block AFTER.
   HEAD and END are the first and the last statement belonging
   to the block.  If both are NULL, an empty block is created.  */
//原型 create_basic_block_1 cfghooks.cc
static basic_block create_basic_block_1 (MtcsCfgContext *self,void *head, void *end, basic_block after)
{
   MtcsCfgState *mtcsCfgState=self->current;

   basic_block ret;
   if (!mtcsCfgState->create_basic_block)
      internal_error ("%s does not support create_basic_block", mtcsCfgState->name);
   ret = mtcsCfgState->create_basic_block(mtcsCfgState,head, end, after);
   if (dom_info_available_p (CDI_DOMINATORS))
      add_to_dominance_info (CDI_DOMINATORS, ret);
   if (dom_info_available_p (CDI_POST_DOMINATORS))
      add_to_dominance_info (CDI_POST_DOMINATORS, ret);
   return ret;
}

/* Verify the CFG consistency.

   Currently it does following: checks edge and basic block list correctness
   and calls into IL dependent checking then.  */
//原型 verify_flow_info cfghooks.h cfghooks.cc
DEBUG_FUNCTION void mtcs_cfg_context_verify_flow_info (MtcsCfgContext *self)
{
   MtcsCfgState *mtcsCfgState=self->current;
   size_t *edge_checksum;
   bool err = false;
   basic_block bb, last_bb_seen;
   basic_block *last_visited;
  // n_debug("mtcscfgcontext.c mtcs_cfg_context_verify_flow_info 00xx mtcsCfgState:%p\n",mtcsCfgState);

   last_visited = XCNEWVEC (basic_block, last_basic_block_for_fn (cfun));
   edge_checksum = XCNEWVEC (size_t, last_basic_block_for_fn (cfun));
 //  n_debug("mtcscfgcontext.c mtcs_cfg_context_verify_flow_info 00:name:%s\n",mtcsCfgState->name);

   /* Check bb chain & numbers.  */
   last_bb_seen = ENTRY_BLOCK_PTR_FOR_FN (cfun);
   FOR_BB_BETWEEN (bb, ENTRY_BLOCK_PTR_FOR_FN (cfun)->next_bb, NULL, next_bb){
     // n_debug("mtcscfgcontext.c mtcs_cfg_context_verify_flow_info 11\n");

      if (bb != EXIT_BLOCK_PTR_FOR_FN (cfun)  && bb != BASIC_BLOCK_FOR_FN (cfun, bb->index)){
         error ("bb %d on wrong place", bb->index);
         err = true;
      }

      if (bb->prev_bb != last_bb_seen){
         error ("prev_bb of %d should be %d, not %d",bb->index, last_bb_seen->index, bb->prev_bb->index);
         err = true;
      }

      last_bb_seen = bb;
   }
  // n_debug("mtcscfgcontext.c mtcs_cfg_context_verify_flow_info 22\n");

   /* Now check the basic blocks (boundaries etc.) */
   FOR_EACH_BB_REVERSE_FN (bb, cfun){
      //n_debug("mtcscfgcontext.c mtcs_cfg_context_verify_flow_info 33\n");

      int n_fallthru = 0;
      edge e;
      edge_iterator ei;

      if (bb->loop_father != NULL && current_loops == NULL){
         error ("verify_flow_info: Block %i has loop_father, but there are no loops", bb->index);
         err = true;
      }
      if (bb->loop_father == NULL && current_loops != NULL){
         error ("verify_flow_info: Block %i lacks loop_father", bb->index);
         err = true;
      }

      if (!bb->count.verify ()){
         error ("verify_flow_info: Wrong count of block %i", bb->index);
         err = true;
      }
      /* FIXME: Graphite and SLJL and target code still tends to produce
      edges with no probability.  */
      if (profile_status_for_fn (cfun) >= PROFILE_GUESSED
      && !bb->count.initialized_p () && !flag_graphite && 0){
         error ("verify_flow_info: Missing count of block %i", bb->index);
         err = true;
      }

      if (bb->flags & ~cfun->cfg->bb_flags_allocated){
         error ("verify_flow_info: unallocated flag set on BB %d", bb->index);
         err = true;
      }

      FOR_EACH_EDGE (e, ei, bb->succs){
         if (last_visited [e->dest->index] == bb){
            error ("verify_flow_info: Duplicate edge %i->%i",e->src->index, e->dest->index);
            err = true;
         }
         /* FIXME: Graphite and SLJL and target code still tends to produce
         edges with no probability.  */
         if (profile_status_for_fn (cfun) >= PROFILE_GUESSED
         && !e->probability.initialized_p () && !flag_graphite && 0){
            error ("Uninitialized probability of edge %i->%i", e->src->index,e->dest->index);
            err = true;
         }
         if (!e->probability.verify ()){
            error ("verify_flow_info: Wrong probability of edge %i->%i",e->src->index, e->dest->index);
            err = true;
         }

         last_visited [e->dest->index] = bb;

         if (e->flags & EDGE_FALLTHRU)
            n_fallthru++;

         if (e->src != bb){
            error ("verify_flow_info: Basic block %d succ edge is corrupted",bb->index);
            fprintf (stderr, "Predecessor: ");
            dump_edge_info (stderr, e, TDF_DETAILS, 0);
            fprintf (stderr, "\nSuccessor: ");
            dump_edge_info (stderr, e, TDF_DETAILS, 1);
            fprintf (stderr, "\n");
            err = true;
         }

         if (e->flags & ~cfun->cfg->edge_flags_allocated){
            error ("verify_flow_info: unallocated edge flag set on %d -> %d",
            e->src->index, e->dest->index);
            err = true;
         }

         edge_checksum[e->dest->index] += (size_t) e;
      }
      if (n_fallthru > 1){
         error ("wrong amount of branch edges after unconditional jump %i", bb->index);
         err = true;
      }

      FOR_EACH_EDGE (e, ei, bb->preds){
         if (e->dest != bb){
            error ("basic block %d pred edge is corrupted", bb->index);
            fputs ("Predecessor: ", stderr);
            dump_edge_info (stderr, e, TDF_DETAILS, 0);
            fputs ("\nSuccessor: ", stderr);
            dump_edge_info (stderr, e, TDF_DETAILS, 1);
            fputc ('\n', stderr);
            err = true;
         }

         if (ei.index != e->dest_idx){
            error ("basic block %d pred edge is corrupted", bb->index);
            error ("its dest_idx should be %d, not %d",
            ei.index, e->dest_idx);
            fputs ("Predecessor: ", stderr);
            dump_edge_info (stderr, e, TDF_DETAILS, 0);
            fputs ("\nSuccessor: ", stderr);
            dump_edge_info (stderr, e, TDF_DETAILS, 1);
            fputc ('\n', stderr);
            err = true;
         }

         edge_checksum[e->dest->index] -= (size_t) e;
      }
   }
   //n_debug("mtcscfgcontext.c mtcs_cfg_context_verify_flow_info 44\n");

   /* Complete edge checksumming for ENTRY and EXIT.  */
   {
      edge e;
      edge_iterator ei;

      FOR_EACH_EDGE (e, ei, ENTRY_BLOCK_PTR_FOR_FN (cfun)->succs)
         edge_checksum[e->dest->index] += (size_t) e;

      FOR_EACH_EDGE (e, ei, EXIT_BLOCK_PTR_FOR_FN (cfun)->preds)
         edge_checksum[e->dest->index] -= (size_t) e;
   }
   //n_debug("mtcscfgcontext.c mtcs_cfg_context_verify_flow_info 66\n");

   FOR_BB_BETWEEN (bb, ENTRY_BLOCK_PTR_FOR_FN (cfun), NULL, next_bb)
      if (edge_checksum[bb->index]){
         error ("basic block %i edge lists are corrupted", bb->index);
         err = true;
      }

   /* Clean up.  */
   free (last_visited);
   free (edge_checksum);
  // n_debug("mtcscfgcontext.c mtcs_cfg_context_verify_flow_info 77 mtcsCfgState:%p %s\n",mtcsCfgState,mtcsCfgState->name);

   if (mtcsCfgState->verify_flow_info)
      if (mtcsCfgState->verify_flow_info(mtcsCfgState))
         err = true;

   if (err)
      internal_error ("verify_flow_info failed");
}

/* Print out one basic block BB to file OUTF.  INDENT is printed at the
   start of each new line.  FLAGS are the TDF_* flags in dumpfile.h.

   This function takes care of the purely graph related information.
   The cfg hook for the active representation should dump
   representation-specific information.  */
//原型 dump_bb cfghooks.h cfghooks.cc
void mtcs_cfg_context_dump_bb (MtcsCfgContext *self,FILE *outf, basic_block bb, int indent, dump_flags_t flags)
{
   MtcsCfgState *mtcsCfgState=self->current;

   if (flags & TDF_BLOCKS)
      dump_bb_info (outf, bb, indent, flags, true, false);
   if (mtcsCfgState->dump_bb)
      mtcsCfgState->dump_bb (mtcsCfgState,outf, bb, indent, flags);
   if (flags & TDF_BLOCKS)
      dump_bb_info (outf, bb, indent, flags, false, true);
   fputc ('\n', outf);
}

//原型 debug cfg.h cfghooks.cc
DEBUG_FUNCTION void mtcs_cfg_context_debug (MtcsCfgContext *self,basic_block_def &ref)
{
   mtcs_cfg_context_dump_bb/*!dump_bb*/(self,stderr, &ref, 0, TDF_NONE);
}
//原型 debug cfg.h cfghooks.cc
DEBUG_FUNCTION void mtcs_cfg_context_debug (MtcsCfgContext *self,basic_block_def *ptr)
{
  if (ptr)
     mtcs_cfg_context_debug/*!debug*/(self,*ptr);
  else
    fprintf (stderr, "<nil>\n");
}

//原型 debug_slim cfghooks.cc
static void debug_slim (basic_block ptr)
{
  fprintf (stderr, "<basic_block %p (%d)>", (void *) ptr, ptr->index);
}

//DEFINE_DEBUG_VEC (basic_block_def *)
//DEFINE_DEBUG_HASH_SET (basic_block_def *)

/* Dumps basic block BB to pretty-printer PP, for use as a label of
   a DOT graph record-node.  The implementation of this hook is
   expected to write the label to the stream that is attached to PP.
   Field separators between instructions are pipe characters printed
   verbatim.  Instructions should be written with some characters
   escaped, using pp_write_text_as_dot_label_to_stream().  */

//原型 dump_bb_for_graph cfghooks.h cfghooks.cc
void mtcs_cfg_context_dump_bb_for_graph (MtcsCfgContext *self,pretty_printer *pp, basic_block bb)
{
   MtcsCfgState *mtcsCfgState=self->current;

   if (!mtcsCfgState->dump_bb_for_graph)
      internal_error ("%s does not support dump_bb_for_graph",mtcsCfgState->name);
   /* TODO: Add pretty printer for counter.  */
   if (bb->count.initialized_p ())
      pp_printf (pp, "COUNT:" "%" PRId64, bb->count.to_gcov_type ());
   pp_write_text_to_stream (pp);
   if (!(dump_flags & TDF_SLIM))
      mtcsCfgState->dump_bb_for_graph(mtcsCfgState,pp, bb);
}

/* Dump the complete CFG to FILE.  FLAGS are the TDF_* flags in dumpfile.h.  */
//原型 dump_bb_for_graph cfghooks.h cfghooks.cc
void mtcs_cfg_context_dump_flow_info (MtcsCfgContext *self,FILE *file, dump_flags_t flags)
{
   basic_block bb;
   fprintf (file, "\n%d basic blocks, %d edges.\n", n_basic_blocks_for_fn (cfun),n_edges_for_fn (cfun));
   FOR_ALL_BB_FN (bb, cfun)
      mtcs_cfg_context_dump_bb/*!dump_bb*/(self,file, bb, 0, flags);
   putc ('\n', file);
}

/* Like above, but dump to stderr.  To be called from debuggers.  */
//原型 debug_flow_info  cfghooks.cc
DEBUG_FUNCTION void mtcs_cfg_context_debug_flow_info (MtcsCfgContext *self)
{
   mtcs_cfg_context_dump_flow_info/*!dump_flow_info*/(self,stderr, TDF_DETAILS);
}

/* Redirect edge E to the given basic block DEST and update underlying program
   representation.  Returns edge representing redirected branch (that may not
   be equivalent to E in the case of duplicate edges being removed) or NULL
   if edge is not easily redirectable for whatever reason.  */
//原型 redirect_edge_and_branch cfghooks.h cfghooks.cc
edge mtcs_cfg_context_redirect_edge_and_branch (MtcsCfgContext *self,edge e, basic_block dest)
{
   MtcsCfgState *mtcsCfgState=self->current;

   edge ret;
   if (!mtcsCfgState->redirect_edge_and_branch)
      internal_error ("%s does not support redirect_edge_and_branch",mtcsCfgState->name);
   n_debug("mtcscfgcontext.c mtcs_cfg_context_redirect_edge_and_branch 00 %s\n",mtcsCfgState->name);

   ret = mtcsCfgState->redirect_edge_and_branch (mtcsCfgState,e, dest);
   /* If RET != E, then either the redirection failed, or the edge E
   was removed since RET already lead to the same destination.  */
   if (current_loops != NULL && ret == e)
      rescan_loop_exit (e, false, false);
   return ret;
}

/* Returns true if it is possible to remove the edge E by redirecting it
   to the destination of the other edge going from its source.  */
//原型 can_remove_branch_p cfghooks.h cfghooks.cc
bool mtcs_cfg_context_can_remove_branch_p (MtcsCfgContext *self,const_edge e)
{
   MtcsCfgState *mtcsCfgState=self->current;

   if (!mtcsCfgState->can_remove_branch_p)
      internal_error ("%s does not support can_remove_branch_p",mtcsCfgState->name);

   if (EDGE_COUNT (e->src->succs) != 2)
      return false;
   return mtcsCfgState->can_remove_branch_p(mtcsCfgState,e);
}

/* Removes E, by redirecting it to the destination of the other edge going
   from its source.  Can_remove_branch_p must be true for E, hence this
   operation cannot fail.  */
//原型 remove_branch cfghooks.h cfghooks.cc
void mtcs_cfg_context_remove_branch (MtcsCfgContext *self,edge e)
{
   edge other;
   basic_block src = e->src;
   int irr;

   gcc_assert (EDGE_COUNT (e->src->succs) == 2);

   other = EDGE_SUCC (src, EDGE_SUCC (src, 0) == e);
   irr = other->flags & EDGE_IRREDUCIBLE_LOOP;

   e = mtcs_cfg_context_redirect_edge_and_branch/*!redirect_edge_and_branch*/(self,e, other->dest);
   gcc_assert (e != NULL);

   e->flags &= ~EDGE_IRREDUCIBLE_LOOP;
   e->flags |= irr;
}

/* Removes edge E from cfg.  Unlike remove_branch, it does not update IL.  */
//原型 remove_edge cfghooks.h cfghooks.cc
void mtcs_cfg_context_remove_edge (MtcsCfgContext *self,edge e)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsCfg *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
   MtcsCfgState *mtcsCfgState=self->current;

   if (current_loops != NULL){
      rescan_loop_exit (e, false, true);

      /* Removal of an edge inside an irreducible region or which leads
      to an irreducible region can turn the region into a natural loop.
      In that case, ask for the loop structure fixups.

      FIXME: Note that LOOPS_HAVE_MARKED_IRREDUCIBLE_REGIONS is not always
      set, so always ask for fixups when removing an edge in that case.  */
      if (!loops_state_satisfies_p (LOOPS_HAVE_MARKED_IRREDUCIBLE_REGIONS)
      || (e->flags & EDGE_IRREDUCIBLE_LOOP)
      || (e->dest->flags & BB_IRREDUCIBLE_LOOP))
         loops_state_set (LOOPS_NEED_FIXUP);
   }

   /* This is probably not needed, but it doesn't hurt.  */
   /* FIXME: This should be called via a remove_edge hook.  */
   if (mtcs_cfg_state_get_state_type/*!current_ir_type*/(mtcsCfgState) == IR_GIMPLE)
      redirect_edge_var_map_clear (e);

   mtcs_cfg_remove_edge_raw/*!remove_edge_raw*/(mtcsCfg,e);
}

/* Like redirect_edge_succ but avoid possible duplicate edge.  */
//原型 redirect_edge_succ_nodup cfghooks.h cfghooks.cc
edge mtcs_cfg_context_redirect_edge_succ_nodup (MtcsCfgContext *self,edge e, basic_block new_succ)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsCfg *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);

   edge s;

   s = find_edge (e->src, new_succ);
   if (s && s != e){
      s->flags |= e->flags;
      s->probability += e->probability;
      /* FIXME: This should be called via a hook and only for IR_GIMPLE.  */
      redirect_edge_var_map_dup (s, e);
      mtcs_cfg_context_remove_edge/*!remove_edge*/(self,e);
      e = s;
   }else
      mtcs_cfg_redirect_edge_succ/*!redirect_edge_succ*/(mtcsCfg,e, new_succ);

   return e;
}


/* Redirect the edge E to basic block DEST even if it requires creating
   of a new basic block; then it returns the newly created basic block.
   Aborts when redirection is impossible.  */
//原型 redirect_edge_and_branch_force cfghooks.h cfghooks.cc
basic_block mtcs_cfg_context_redirect_edge_and_branch_force (MtcsCfgContext *self,edge e, basic_block dest)
{
   MtcsCfgState *mtcsCfgState=self->current;

   basic_block ret, src = e->src;
   if (!mtcsCfgState->redirect_edge_and_branch_force)
      internal_error ("%s does not support redirect_edge_and_branch_force",mtcsCfgState->name);
   if (current_loops != NULL)
      rescan_loop_exit (e, false, true);
   ret = mtcsCfgState->redirect_edge_and_branch_force (mtcsCfgState,e, dest);
   if (ret != NULL && dom_info_available_p (CDI_DOMINATORS))
      set_immediate_dominator (CDI_DOMINATORS, ret, src);
   if (current_loops != NULL){
      if (ret != NULL){
         class loop *loop = find_common_loop (single_pred (ret)->loop_father,
         single_succ (ret)->loop_father);
         add_bb_to_loop (ret, loop);
      }else if (find_edge (src, dest) == e)
         rescan_loop_exit (e, true, false);
   }
   return ret;
}

//原型 split_block cfghooks.h cfghooks.cc
edge mtcs_cfg_context_split_block (MtcsCfgContext *self,basic_block bb, gimple *i)
{
  return split_block_1(self,bb, i);
}

//原型 split_block cfghooks.h cfghooks.cc
edge mtcs_cfg_context_split_block (MtcsCfgContext *self,basic_block bb, rtx i)
{
  return split_block_1(self,bb, i);
}


/* Splits block BB just after labels.  The newly created edge is returned.  */
//原型 split_block_after_labels cfghooks.h cfghooks.cc
edge mtcs_cfg_context_split_block_after_labels (MtcsCfgContext *self,basic_block bb)
{
  return split_block_1(self,bb, NULL);
}

/* Moves block BB immediately after block AFTER.  Returns false if the
   movement was impossible.  */
//原型 move_block_after cfghooks.h cfghooks.cc
bool mtcs_cfg_context_move_block_after (MtcsCfgContext *self,basic_block bb, basic_block after)
{
   MtcsCfgState *mtcsCfgState=self->current;

   bool ret;
   if (!mtcsCfgState->move_block_after)
      internal_error ("%s does not support move_block_after", mtcsCfgState->name);
   ret = mtcsCfgState->move_block_after(mtcsCfgState,bb, after);
   return ret;
}


/* Deletes the basic block BB.  */
//原型 delete_basic_block cfghooks.h cfghooks.cc
void  mtcs_cfg_context_delete_basic_block (MtcsCfgContext *self,basic_block bb)
{
   MtcsCfgState *mtcsCfgState=self->current;

   if (!mtcsCfgState->delete_basic_block)
      internal_error ("%s does not support delete_basic_block", mtcsCfgState->name);

   mtcsCfgState->delete_basic_block(mtcsCfgState,bb);

   if (current_loops != NULL){
      class loop *loop = bb->loop_father;

      /* If we remove the header or the latch of a loop, mark the loop for
      removal.  */
      if (loop->latch == bb || loop->header == bb)
         mark_loop_for_removal (loop);

      remove_bb_from_loops (bb);
   }

   /* Remove the edges into and out of this block.  Note that there may
   indeed be edges in, if we are removing an unreachable loop.  */
   while (EDGE_COUNT (bb->preds) != 0)
      mtcs_cfg_context_remove_edge/*!remove_edge*/(self,EDGE_PRED (bb, 0));
   while (EDGE_COUNT (bb->succs) != 0)
      mtcs_cfg_context_remove_edge/*!remove_edge*/(self,EDGE_SUCC (bb, 0));

   if (dom_info_available_p (CDI_DOMINATORS))
      delete_from_dominance_info (CDI_DOMINATORS, bb);
   if (dom_info_available_p (CDI_POST_DOMINATORS))
      delete_from_dominance_info (CDI_POST_DOMINATORS, bb);

   /* Remove the basic block from the array.  */
   expunge_block (bb);
}

static void printbb(basic_block block)
{
   rtx_insn *insn;
   int i=0;
   FOR_BB_INSNS (block, insn){
      if (!INSN_P (insn))
         continue;
      fprintf(stderr,"mtcscfgcontext.c 打印块中的指令 i:%d block:%p index:%d flags:%d insn:%p\n",i++,block,block->index,block->flags,insn);
      mtcs_print_rtl_single(stderr,insn);
   }
}

static void testprint()
{
   return;
   if(!n_log_is_debug_file(NULL,NULL))
      return;
   basic_block bb;
   FOR_ALL_BB_FN (bb, cfun)
      printbb(bb);
}

/* Splits edge E and returns the newly created basic block.  */
//原型 split_edge cfghooks.h cfghooks.cc
basic_block mtcs_cfg_context_split_edge (MtcsCfgContext *self,edge e)
{
   MtcsCfgState *mtcsCfgState=self->current;

   basic_block ret;
   profile_count count = e->count ();
   edge f;
   bool irr = (e->flags & EDGE_IRREDUCIBLE_LOOP) != 0;
   bool back = (e->flags & EDGE_DFS_BACK) != 0;
   class loop *loop;
   basic_block src = e->src, dest = e->dest;
   n_debug("mtcscfgcontext mtcs_cfg_context_split_edge 00 state:%s current_loops:%p\n",mtcsCfgState->name,current_loops);
   if (!mtcsCfgState->split_edge)
      internal_error ("%s does not support split_edge", mtcsCfgState->name);
   testprint();
   if (current_loops != NULL)
      rescan_loop_exit (e, false, true);

   ret = mtcsCfgState->split_edge(mtcsCfgState,e);
   n_debug("mtcscfgcontext mtcs_cfg_context_split_edge 11 state:%s current_loops:%p\n",mtcsCfgState->name,current_loops);
   //testprint();

   ret->count = count;
   single_succ_edge (ret)->probability = profile_probability::always ();

   if (irr){
      ret->flags |= BB_IRREDUCIBLE_LOOP;
      single_pred_edge (ret)->flags |= EDGE_IRREDUCIBLE_LOOP;
      single_succ_edge (ret)->flags |= EDGE_IRREDUCIBLE_LOOP;
   }
   if (back){
      single_pred_edge (ret)->flags &= ~EDGE_DFS_BACK;
      single_succ_edge (ret)->flags |= EDGE_DFS_BACK;
   }
   n_debug("mtcscfgcontext mtcs_cfg_context_split_edge 22 state:%s current_loops:%p\n",mtcsCfgState->name,current_loops);
  // testprint();
   if (dom_info_available_p (CDI_DOMINATORS))
      set_immediate_dominator (CDI_DOMINATORS, ret, single_pred (ret));

   if (dom_info_state (CDI_DOMINATORS) >= DOM_NO_FAST_QUERY){
      /* There are two cases:
      If the immediate dominator of e->dest is not e->src, it
      remains unchanged.
      If immediate dominator of e->dest is e->src, it may become
      ret, provided that all other predecessors of e->dest are
      dominated by e->dest.  */
      if (get_immediate_dominator (CDI_DOMINATORS, single_succ (ret)) == single_pred (ret)){
         edge_iterator ei;
         FOR_EACH_EDGE (f, ei, single_succ (ret)->preds){
            if (f == single_succ_edge (ret))
               continue;

            if (!dominated_by_p (CDI_DOMINATORS, f->src,single_succ (ret)))
               break;
         }

         if (!f)
            set_immediate_dominator (CDI_DOMINATORS, single_succ (ret), ret);
      }
   }
   n_debug("mtcscfgcontext mtcs_cfg_context_split_edge 33 state:%s current_loops:%p\n",mtcsCfgState->name,current_loops);
   //testprint();
   if (current_loops != NULL){
      loop = find_common_loop (src->loop_father, dest->loop_father);
      add_bb_to_loop (ret, loop);

      /* If we split the latch edge of loop adjust the latch block.  */
      if (loop->latch == src && loop->header == dest)
         loop->latch = ret;
   }
   return ret;
}

//原型 create_basic_block cfghooks.h cfghooks.cc
basic_block mtcs_cfg_context_create_basic_block (MtcsCfgContext *self,gimple_seq seq, basic_block after)
{
  return create_basic_block_1(self,seq, NULL, after);
}

//原型 create_basic_block cfghooks.h cfghooks.cc
basic_block mtcs_cfg_context_create_basic_block (MtcsCfgContext *self,rtx head, rtx end, basic_block after)
{
  return create_basic_block_1(self,head, end, after);
}

/* Checks whether we may merge blocks BB1 and BB2.  */
//原型 can_merge_blocks_p cfghooks.h cfghooks.cc
bool mtcs_cfg_context_can_merge_blocks_p (MtcsCfgContext *self,basic_block bb1, basic_block bb2)
{
   MtcsCfgState *mtcsCfgState=self->current;

   bool ret;
   if (!mtcsCfgState->can_merge_blocks_p)
      internal_error ("%s does not support can_merge_blocks_p", mtcsCfgState->name);
   ret = mtcsCfgState->can_merge_blocks_p(mtcsCfgState,bb1, bb2);
   return ret;
}

//原型 predict_edge cfghooks.h cfghooks.cc
void mtcs_cfg_context_predict_edge (MtcsCfgContext *self,edge e, enum br_predictor predictor, int probability)
{
   MtcsCfgState *mtcsCfgState=self->current;
   if (!mtcsCfgState->predict_edge)
      internal_error ("%s does not support predict_edge", mtcsCfgState->name);
   mtcsCfgState->predict_edge (mtcsCfgState,e, predictor, probability);
}

//原型 predicted_by_p cfghooks.h cfghooks.cc
bool mtcs_cfg_context_predicted_by_p (MtcsCfgContext *self,const_basic_block bb, enum br_predictor predictor)
{
   MtcsCfgState *mtcsCfgState=self->current;
   if (!mtcsCfgState->predict_edge)
      internal_error ("%s does not support predicted_by_p", mtcsCfgState->name);
   return mtcsCfgState->predicted_by_p(mtcsCfgState,bb, predictor);
}

/* Merges basic block B into basic block A.  */
//原型 merge_blocks cfghooks.h cfghooks.cc
void mtcs_cfg_context_merge_blocks (MtcsCfgContext *self,basic_block a, basic_block b)
{
   MtcsCfgState *mtcsCfgState=self->current;

   edge e;
   edge_iterator ei;

   if (!mtcsCfgState->merge_blocks)
      internal_error ("%s does not support merge_blocks", mtcsCfgState->name);
   n_debug("mtcscfgcontext.c mtcs_cfg_context_merge_blocks 00 state:%s\n",mtcsCfgState->name);
   mtcsCfgState->merge_blocks(mtcsCfgState,a, b);

   if (current_loops != NULL){
      /* If the block we merge into is a loop header do nothing unless ... */
      if (a->loop_father->header == a){
         /* ... we merge two loop headers, in which case we kill
         the inner loop.  */
         if (b->loop_father->header == b)
            mark_loop_for_removal (b->loop_father);
      }
      /* If we merge a loop header into its predecessor, update the loop
      structure.  */
      else if (b->loop_father->header == b){
         remove_bb_from_loops (a);
         add_bb_to_loop  (a, b->loop_father);
         a->loop_father->header = a;
      }
      /* If we merge a loop latch into its predecessor, update the loop
      structure.  */
      if (b->loop_father->latch && b->loop_father->latch == b)
         b->loop_father->latch = a;
      remove_bb_from_loops (b);
   }

   /* Normally there should only be one successor of A and that is B, but
   partway though the merge of blocks for conditional_execution we'll
   be merging a TEST block with THEN and ELSE successors.  Free the
   whole lot of them and hope the caller knows what they're doing.  */

   while (EDGE_COUNT (a->succs) != 0)
      mtcs_cfg_context_remove_edge/*!remove_edge*/(self,EDGE_SUCC (a, 0));

   /* Adjust the edges out of B for the new owner.  */
   FOR_EACH_EDGE (e, ei, b->succs){
      e->src = a;
      if (current_loops != NULL){
         /* If b was a latch, a now is.  */
         if (e->dest->loop_father->latch == b)
            e->dest->loop_father->latch = a;
         rescan_loop_exit (e, true, false);
      }
   }
   a->succs = b->succs;
   a->flags |= b->flags;

   /* B hasn't quite yet ceased to exist.  Attempt to prevent mishap.  */
   b->preds = b->succs = NULL;

   if (dom_info_available_p (CDI_DOMINATORS))
      redirect_immediate_dominators (CDI_DOMINATORS, b, a);

   if (dom_info_available_p (CDI_DOMINATORS))
      delete_from_dominance_info (CDI_DOMINATORS, b);
   if (dom_info_available_p (CDI_POST_DOMINATORS))
      delete_from_dominance_info (CDI_POST_DOMINATORS, b);

   expunge_block (b);
}

/* Split BB into entry part and the rest (the rest is the newly created block).
   Redirect those edges for that REDIRECT_EDGE_P returns true to the entry
   part.  Returns the edge connecting the entry part to the rest.  */
//原型 make_forwarder_block cfghooks.h cfghooks.cc
edge mtcs_cfg_context_make_forwarder_block (MtcsCfgContext *self,basic_block bb, bool (*redirect_edge_p) (edge),
            void (*new_bb_cbk) (basic_block))
{
   MtcsCfgState *mtcsCfgState=self->current;

   edge e, fallthru;
   edge_iterator ei;
   basic_block dummy, jump;
   class loop *loop, *ploop, *cloop;

   if (!mtcsCfgState->make_forwarder_block)
      internal_error ("%s does not support make_forwarder_block",mtcsCfgState->name);

   fallthru = mtcs_cfg_context_split_block_after_labels/*!split_block_after_labels*/(self,bb);
   dummy = fallthru->src;
   dummy->count = profile_count::zero ();
   bb = fallthru->dest;

   /* Redirect back edges we want to keep.  */
   for (ei = ei_start (dummy->preds); (e = ei_safe_edge (ei)); ){
      basic_block e_src;

      if (redirect_edge_p (e)){
         dummy->count += e->count ();
         ei_next (&ei);
         continue;
      }

      e_src = e->src;
      jump = mtcs_cfg_context_redirect_edge_and_branch_force/*!redirect_edge_and_branch_force*/(self,e, bb);
      if (jump != NULL){
         /* If we redirected the loop latch edge, the JUMP block now acts like
         the new latch of the loop.  */
         if (current_loops != NULL
         && dummy->loop_father != NULL
         && dummy->loop_father->header == dummy
         && dummy->loop_father->latch == e_src)
            dummy->loop_father->latch = jump;

         if (new_bb_cbk != NULL)
            new_bb_cbk (jump);
      }
   }

   if (dom_info_available_p (CDI_DOMINATORS)){
      vec<basic_block> doms_to_fix;
      doms_to_fix.create (2);
      doms_to_fix.quick_push (dummy);
      doms_to_fix.quick_push (bb);
      iterate_fix_dominators (CDI_DOMINATORS, doms_to_fix, false);
      doms_to_fix.release ();
   }

   if (current_loops != NULL){
      /* If we do not split a loop header, then both blocks belong to the
      same loop.  In case we split loop header and do not redirect the
      latch edge to DUMMY, then DUMMY belongs to the outer loop, and
      BB becomes the new header.  If latch is not recorded for the loop,
      we leave this updating on the caller (this may only happen during
      loop analysis).  */
      loop = dummy->loop_father;
      if (loop->header == dummy   && loop->latch != NULL && find_edge (loop->latch, dummy) == NULL){
         remove_bb_from_loops (dummy);
         loop->header = bb;

         cloop = loop;
         FOR_EACH_EDGE (e, ei, dummy->preds){
            cloop = find_common_loop (cloop, e->src->loop_father);
         }
         add_bb_to_loop (dummy, cloop);
      }

      /* In case we split loop latch, update it.  */
      for (ploop = loop; ploop; ploop = loop_outer (ploop))
         if (ploop->latch == dummy)
            ploop->latch = bb;
   }

   mtcsCfgState->make_forwarder_block(mtcsCfgState,fallthru);

   return fallthru;
}

/* Try to make the edge fallthru.  */
//原型 tidy_fallthru_edge cfghooks.h cfghooks.cc
void mtcs_cfg_context_tidy_fallthru_edge (MtcsCfgContext *self,edge e)
{
   MtcsCfgState *mtcsCfgState=self->current;
   if (mtcsCfgState->tidy_fallthru_edge)
      mtcsCfgState->tidy_fallthru_edge(mtcsCfgState,e);
}

/* Fix up edges that now fall through, or rather should now fall through
   but previously required a jump around now deleted blocks.  Simplify
   the search by only examining blocks numerically adjacent, since this
   is how they were created.

   ??? This routine is currently RTL specific.  */
//原型 tidy_fallthru_edges cfghooks.h cfghooks.cc
void mtcs_cfg_context_tidy_fallthru_edges (MtcsCfgContext *self)
{
   MtcsCfgState *mtcsCfgState=self->current;

   basic_block b, c;
   if (!mtcsCfgState->tidy_fallthru_edge)
      return;
   if (ENTRY_BLOCK_PTR_FOR_FN (cfun)->next_bb == EXIT_BLOCK_PTR_FOR_FN (cfun))
      return;

   FOR_BB_BETWEEN (b, ENTRY_BLOCK_PTR_FOR_FN (cfun)->next_bb,
   EXIT_BLOCK_PTR_FOR_FN (cfun)->prev_bb, next_bb){
      edge s;
      c = b->next_bb;
      /* We care about simple conditional or unconditional jumps with
      a single successor.

      If we had a conditional branch to the next instruction when
      CFG was built, then there will only be one out edge for the
      block which ended with the conditional branch (since we do
      not create duplicate edges).

      Furthermore, the edge will be marked as a fallthru because we
      merge the flags for the duplicate edges.  So we do not want to
      check that the edge is not a FALLTHRU edge.  */
      if (single_succ_p (b)){
         s = single_succ_edge (b);
         if (! (s->flags & EDGE_COMPLEX) && s->dest == c
         && !(JUMP_P (BB_END (b)) && CROSSING_JUMP_P (BB_END (b))))
            mtcs_cfg_context_tidy_fallthru_edge/*!tidy_fallthru_edge*/(self,s);
      }
   }
}

/* Edge E is assumed to be fallthru edge.  Emit needed jump instruction
   (and possibly create new basic block) to make edge non-fallthru.
   Return newly created BB or NULL if none.  */
//原型 force_nonfallthru cfghooks.h cfghooks.cc
basic_block mtcs_cfg_context_force_nonfallthru (MtcsCfgContext *self,edge e)
{
   MtcsCfgState *mtcsCfgState=self->current;

   basic_block ret, src = e->src;
   if (!mtcsCfgState->force_nonfallthru)
      internal_error ("%s does not support force_nonfallthru",mtcsCfgState->name);
   ret = mtcsCfgState->force_nonfallthru(mtcsCfgState,e);
   if (ret != NULL) {
      if (dom_info_available_p (CDI_DOMINATORS))
         set_immediate_dominator (CDI_DOMINATORS, ret, src);

      if (current_loops != NULL){
         basic_block pred = single_pred (ret);
         basic_block succ = single_succ (ret);
         class loop *loop = find_common_loop (pred->loop_father, succ->loop_father);
         rescan_loop_exit (e, false, true);
         add_bb_to_loop (ret, loop);
         /* If we split the latch edge of loop adjust the latch block.  */
         if (loop->latch == pred && loop->header == succ)
            loop->latch = ret;
      }
   }

   return ret;
}

/* Returns true if we can duplicate basic block BB.  */
//原型 can_duplicate_block_p cfghooks.h cfghooks.cc
bool mtcs_cfg_context_can_duplicate_block_p (MtcsCfgContext *self,const_basic_block bb)
{
   MtcsCfgState *mtcsCfgState=self->current;
   n_debug("mtcscfgcontext.c mtcs_cfg_context_can_duplicate_block_p mtcsCfgState:%p\n",mtcsCfgState);
   if (!mtcsCfgState->can_duplicate_block_p)
      internal_error ("%s does not support can_duplicate_block_p",mtcsCfgState->name);
   if (bb == EXIT_BLOCK_PTR_FOR_FN (cfun) || bb == ENTRY_BLOCK_PTR_FOR_FN (cfun))
      return false;
   return mtcsCfgState->can_duplicate_block_p(mtcsCfgState,bb);
}

/* Duplicate basic block BB, place it after AFTER (if non-null) and redirect
   edge E to it (if non-null).  Return the new basic block.

   If BB contains a returns_twice call, the caller is responsible for recreating
   incoming abnormal edges corresponding to the "second return" for the copy.
   gimple_can_duplicate_bb_p rejects such blocks, while RTL likes to live
   dangerously.

   If BB has incoming abnormal edges for some other reason, their destinations
   should be tied to label(s) of the original BB and not the copy.  */
//原型 duplicate_block cfghooks.h cfghooks.cc
basic_block mtcs_cfg_context_duplicate_block (MtcsCfgContext *self,basic_block bb, edge e, basic_block after, copy_bb_data *id)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfg  *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
   MtcsCfgState *mtcsCfgState=self->current;

   edge s, n;
   basic_block new_bb;
   profile_count new_count = e ? e->count (): profile_count::uninitialized ();
   edge_iterator ei;

   if (!mtcsCfgState->duplicate_block)
      internal_error ("%s does not support duplicate_block",mtcsCfgState->name);

   if (bb->count < new_count)
      new_count = bb->count;

   gcc_checking_assert (mtcs_cfg_context_can_duplicate_block_p/*!can_duplicate_block_p*/(self,bb));

   new_bb = mtcsCfgState->duplicate_block(mtcsCfgState,bb, id);
   if (after)
      mtcs_cfg_context_move_block_after/*!move_block_after*/(self,new_bb, after);

     class loop *loop = (id && current_loops) ? bb->loop_father : NULL;
   n_debug("mtcscfgcontext.c duplicate_block 00 %p loop:%p header:%p\n",new_bb,loop,loop->header);
   new_bb->flags = (bb->flags & ~BB_DUPLICATED);
   FOR_EACH_EDGE (s, ei, bb->succs){
      /* Since we are creating edges from a new block to successors
      of another block (which therefore are known to be disjoint), there
      is no need to actually check for duplicated edges.  */
      n = mtcs_cfg_unchecked_make_edge/*!unchecked_make_edge*/(mtcsCfg,new_bb, s->dest, s->flags);
      n->probability = s->probability;
      n->aux = s->aux;
   }
   n_debug("mtcscfgcontext.c duplicate_block 11 %p loop:%p header:%p\n",new_bb,loop,loop->header);

   if (e){
      new_bb->count = new_count;
      bb->count -= new_count;
      mtcs_cfg_context_redirect_edge_and_branch_force/*!redirect_edge_and_branch_force*/(self,e, new_bb);
   }else
      new_bb->count = bb->count;

   mtcs_cfg_set_bb_original/*!set_bb_original*/(mtcsCfg,new_bb, bb);
   mtcs_cfg_set_bb_copy/*!set_bb_copy*/(mtcsCfg,bb, new_bb);
   n_debug("mtcscfgcontext.c duplicate_block 22 %p loop:%p header:%p\n",new_bb,loop,loop->header);

   /* Add the new block to the copy of the loop of BB, or directly to the loop
   of BB if the loop is not being copied.  */
   if (current_loops != NULL){
      class loop *cloop = bb->loop_father;
      class loop *copy = mtcs_cfg_get_loop_copy/*!get_loop_copy*/(mtcsCfg,cloop);
      n_debug("mtcscfgcontext.c duplicate_block 22aa %p loop:%p header:%p copy:%p\n",new_bb,cloop,cloop->header,copy);

      /* If we copied the loop header block but not the loop
      we have created a loop with multiple entries.  Ditch the loop,
      add the new block to the outer loop and arrange for a fixup.  */
      if (!copy  && cloop->header == bb){
         add_bb_to_loop (new_bb, loop_outer (cloop));
         mark_loop_for_removal (cloop);
         n_debug("mtcscfgcontext.c duplicate_block 22bb %p loop:%p header:%p copy:%p\n",new_bb,cloop,cloop->header,copy);

      }else{
         add_bb_to_loop (new_bb, copy ? copy : cloop);
         n_debug("mtcscfgcontext.c duplicate_block 22cc %p loop:%p header:%p copy:%p\n",new_bb,cloop,cloop->header,copy);

         /* If we copied the loop latch block but not the loop, adjust
         loop state.  */
         if (!copy && cloop->latch == bb){
            cloop->latch = NULL;
            loops_state_set (LOOPS_MAY_HAVE_MULTIPLE_LATCHES);
         }
         n_debug("mtcscfgcontext.c duplicate_block 22dd %p loop:%p header:%p copy:%p\n",new_bb,cloop,cloop->header,copy);

      }
   }
   n_debug("mtcscfgcontext.c duplicate_block 33 %p loop:%p header:%p\n",new_bb,loop,loop->header);

   return new_bb;
}

/* Return 1 if BB ends with a call, possibly followed by some
   instructions that must stay with the call, 0 otherwise.  */
//原型 block_ends_with_call_p cfghooks.h cfghooks.cc
bool  mtcs_cfg_context_block_ends_with_call_p (MtcsCfgContext *self,basic_block bb)
{
   MtcsCfgState *mtcsCfgState=self->current;
   if (!mtcsCfgState->block_ends_with_call_p)
      internal_error ("%s does not support block_ends_with_call_p", mtcsCfgState->name);
   return (mtcsCfgState->block_ends_with_call_p)(mtcsCfgState,bb);
}

/* Return 1 if BB ends with a conditional branch, 0 otherwise.  */
//原型 block_ends_with_condjump_p cfghooks.h cfghooks.cc
bool  mtcs_cfg_context_block_ends_with_condjump_p (MtcsCfgContext *self,const_basic_block bb)
{
   MtcsCfgState *mtcsCfgState=self->current;
   if (!mtcsCfgState->block_ends_with_condjump_p)
      internal_error ("%s does not support block_ends_with_condjump_p",mtcsCfgState->name);
   return (mtcsCfgState->block_ends_with_condjump_p)(mtcsCfgState,bb);
}


/* Add fake edges to the function exit for any non constant and non noreturn
   calls, volatile inline assembly in the bitmap of blocks specified by
   BLOCKS or to the whole CFG if BLOCKS is zero.  Return the number of blocks
   that were split.

   The goal is to expose cases in which entering a basic block does not imply
   that all subsequent instructions must be executed.  */
//原型 flow_call_edges_add cfghooks.h cfghooks.cc
int mtcs_cfg_context_block_flow_call_edges_add (MtcsCfgContext *self,sbitmap blocks)
{
   MtcsCfgState *mtcsCfgState=self->current;
   if (!mtcsCfgState->flow_call_edges_add)
      internal_error ("%s does not support flow_call_edges_add",mtcsCfgState->name);
   return (mtcsCfgState->flow_call_edges_add)(mtcsCfgState,blocks);
}

/* This function is called immediately after edge E is added to the
   edge vector E->dest->preds.  */
//原型 execute_on_growing_pred cfghooks.h cfghooks.cc
void mtcs_cfg_context_execute_on_growing_pred (MtcsCfgContext *self,edge e)
{
   MtcsCfgState *mtcsCfgState=self->current;
   if (! (e->dest->flags & BB_DUPLICATED) && mtcsCfgState->execute_on_growing_pred)
      mtcsCfgState->execute_on_growing_pred(mtcsCfgState,e);
}

/* This function is called immediately before edge E is removed from
   the edge vector E->dest->preds.  */
//原型 execute_on_shrinking_pred cfghooks.h cfghooks.cc
void mtcs_cfg_context_execute_on_shrinking_pred (MtcsCfgContext *self,edge e)
{
   MtcsCfgState *mtcsCfgState=self->current;
   if (! (e->dest->flags & BB_DUPLICATED) && mtcsCfgState->execute_on_shrinking_pred)
      mtcsCfgState->execute_on_shrinking_pred(mtcsCfgState,e);
}

/* This is used inside loop versioning when we want to insert
   stmts/insns on the edges, which have a different behavior
   in tree's and in RTL, so we made a CFG hook.  */
//原型 lv_flush_pending_stmts cfghooks.h cfghooks.cc
void mtcs_cfg_context_lv_flush_pending_stmts (MtcsCfgContext *self,edge e)
{
   MtcsCfgState *mtcsCfgState=self->current;
   if (mtcsCfgState->flush_pending_stmts)
      mtcsCfgState->flush_pending_stmts(mtcsCfgState,e);
}

/* Loop versioning uses the duplicate_loop_body_to_header_edge to create
   a new version of the loop basic-blocks, the parameters here are
   exactly the same as in duplicate_loop_body_to_header_edge or
   tree_duplicate_loop_body_to_header_edge; while in tree-ssa there is
   additional work to maintain ssa information that's why there is
   a need to call the tree_duplicate_loop_body_to_header_edge rather
   than duplicate_loop_body_to_header_edge when we are in tree mode.  */
//原型 cfg_hook_duplicate_loop_body_to_header_edge cfghooks.h cfghooks.cc
bool mtcs_cfg_context_cfg_hook_duplicate_loop_body_to_header_edge (MtcsCfgContext *self,class loop *loop, edge e,
                    unsigned int ndupl,sbitmap wont_exit, edge orig,vec<edge> *to_remove, int flags)
{
   MtcsCfgState *mtcsCfgState=self->current;
   gcc_assert (mtcsCfgState->cfg_hook_duplicate_loop_body_to_header_edge);
   return mtcsCfgState->cfg_hook_duplicate_loop_body_to_header_edge(mtcsCfgState,
               loop, e, ndupl, wont_exit, orig, to_remove, flags);
}

/* Conditional jumps are represented differently in trees and RTL,
   this hook takes a basic block that is known to have a cond jump
   at its end and extracts the taken and not taken edges out of it
   and store it in E1 and E2 respectively.  */
//原型 extract_cond_bb_edges cfghooks.h cfghooks.cc
void mtcs_cfg_context_extract_cond_bb_edges (MtcsCfgContext *self,basic_block b, edge *e1, edge *e2)
{
   MtcsCfgState *mtcsCfgState=self->current;
   gcc_assert (mtcsCfgState->extract_cond_bb_edges);
   mtcsCfgState->extract_cond_bb_edges(mtcsCfgState,b, e1, e2);
}

/* Responsible for updating the ssa info (PHI nodes) on the
   new condition basic block that guards the versioned loop.  */
//原型 lv_adjust_loop_header_phi cfghooks.h cfghooks.cc
void mtcs_cfg_context_lv_adjust_loop_header_phi (MtcsCfgContext *self,basic_block first, basic_block second,
            basic_block new_block, edge e)
{
   MtcsCfgState *mtcsCfgState=self->current;
   if (mtcsCfgState->lv_adjust_loop_header_phi)
      mtcsCfgState->lv_adjust_loop_header_phi(mtcsCfgState,first, second, new_block, e);
}

/* Conditions in trees and RTL are different so we need
   a different handling when we add the condition to the
   versioning code.  */
//原型 lv_add_condition_to_bb cfghooks.h cfghooks.cc
void mtcs_cfg_context_lv_add_condition_to_bb (MtcsCfgContext *self,basic_block first, basic_block second,
         basic_block new_block, void *cond)
{
   MtcsCfgState *mtcsCfgState=self->current;
   gcc_assert (mtcsCfgState->lv_add_condition_to_bb);
   mtcsCfgState->lv_add_condition_to_bb(mtcsCfgState,first, second, new_block, cond);
}

/* Checks whether all N blocks in BBS array can be copied.  */
//原型 can_copy_bbs_p cfghooks.h cfghooks.cc
bool mtcs_cfg_context_can_copy_bbs_p (MtcsCfgContext *self,basic_block *bbs, unsigned n)
{
   unsigned i;
   edge e;
   int ret = true;

   for (i = 0; i < n; i++)
      bbs[i]->flags |= BB_DUPLICATED;

   for (i = 0; i < n; i++){
      /* In case we should redirect abnormal edge during duplication, fail.  */
      edge_iterator ei;
      FOR_EACH_EDGE (e, ei, bbs[i]->succs)
         if ((e->flags & EDGE_ABNORMAL) && (e->dest->flags & BB_DUPLICATED)){
            ret = false;
            goto end;
         }

      if (!mtcs_cfg_context_can_duplicate_block_p/*!can_duplicate_block_p*/(self,bbs[i])){
         ret = false;
         break;
      }
   }

end:
   for (i = 0; i < n; i++)
      bbs[i]->flags &= ~BB_DUPLICATED;

   return ret;
}

/* Duplicates N basic blocks stored in array BBS.  Newly created basic blocks
   are placed into array NEW_BBS in the same order.  Edges from basic blocks
   in BBS are also duplicated and copies of those that lead into BBS are
   redirected to appropriate newly created block.  The function assigns bbs
   into loops (copy of basic block bb is assigned to bb->loop_father->copy
   loop, so this must be set up correctly in advance)

   If UPDATE_DOMINANCE is true then this function updates dominators locally
   (LOOPS structure that contains the information about dominators is passed
   to enable this), otherwise it does not update the dominator information
   and it assumed that the caller will do this, perhaps by destroying and
   recreating it instead of trying to do an incremental update like this
   function does when update_dominance is true.

   BASE is the superloop to that basic block belongs; if its header or latch
   is copied, we do not set the new blocks as header or latch.

   Created copies of N_EDGES edges in array EDGES are stored in array NEW_EDGES,
   also in the same order.

   Newly created basic blocks are put after the basic block AFTER in the
   instruction stream, and the order of the blocks in BBS array is preserved.  */
//原型 copy_bbs cfghooks.h cfghooks.cc
void mtcs_cfg_context_copy_bbs (MtcsCfgContext *self,basic_block *bbs, unsigned n, basic_block *new_bbs,
     edge *edges, unsigned num_edges, edge *new_edges,
     class loop *base, basic_block after, bool update_dominance)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfg  *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);

   unsigned i, j;
   basic_block bb, new_bb, dom_bb;
   edge e;
   copy_bb_data id;

   /* Mark the blocks to be copied.  This is used by edge creation hooks
   to decide whether to reallocate PHI nodes capacity to avoid reallocating
   PHIs in the set of source BBs.  */
   for (i = 0; i < n; i++)
      bbs[i]->flags |= BB_DUPLICATED;
   n_debug("mtcscfgcontext.c copy_bbs 00 loop:%p header:%p n:%d after:%p\n",base,base->header,n,after);
   /* Duplicate bbs, update dominators, assign bbs to loops.  */
   for (i = 0; i < n; i++){
      /* Duplicate.  */
      bb = bbs[i];
      n_debug("mtcscfgcontext.c copy_bbs 11 i:%d bb:%p loop:%p header:%p n:%d\n",i,bb,base,base->header,n);

      new_bb = new_bbs[i] =mtcs_cfg_context_duplicate_block/*!duplicate_block*/(self,bb, NULL, after, &id);
      after = new_bb;
      n_debug("mtcscfgcontext.c copy_bbs 22 loop:%p header:%p n:%d new_bb:%p bb->loop_father:%p\n",
            base,base->header,n,new_bb,bb->loop_father);

      if (bb->loop_father){
         /* Possibly set loop header.  */
         if (bb->loop_father->header == bb && bb->loop_father != base){
            n_debug("mtcscfgcontext.c copy_bbs 22aa loop:%p new_bb->loop_father:%p new_bb:%p\n",
                 new_bb->loop_father,base,new_bb);
            new_bb->loop_father->header = new_bb;
         }
         /* Or latch.  */
         if (bb->loop_father->latch == bb && bb->loop_father != base)
            new_bb->loop_father->latch = new_bb;
      }
   }
   n_debug("mtcscfgcontext.c copy_bbs 33 loop:%p header:%p n:%d\n",base,base->header,n);

   /* Set dominators.  */
   if (update_dominance){
      for (i = 0; i < n; i++){
         bb = bbs[i];
         new_bb = new_bbs[i];

         dom_bb = get_immediate_dominator (CDI_DOMINATORS, bb);
         if (dom_bb->flags & BB_DUPLICATED){
            dom_bb = mtcs_cfg_get_bb_copy/*!get_bb_copy*/(mtcsCfg,dom_bb);
            set_immediate_dominator (CDI_DOMINATORS, new_bb, dom_bb);
         }
      }
   }
   n_debug("mtcscfgcontext.c copy_bbs 44 loop:%p header:%p n:%d\n",base,base->header,n);

   /* Redirect edges.  */
   for (i = 0; i < n; i++){
      edge_iterator ei;
      new_bb = new_bbs[i];
      bb = bbs[i];

      FOR_EACH_EDGE (e, ei, new_bb->succs){
         if (!(e->dest->flags & BB_DUPLICATED))
            continue;
         mtcs_cfg_context_redirect_edge_and_branch_force/*!redirect_edge_and_branch_force*/(self,
               e, mtcs_cfg_get_bb_copy/*!get_bb_copy*/(mtcsCfg,e->dest));
      }
   }
   n_debug("mtcscfgcontext.c copy_bbs 55 loop:%p header:%p n:%d\n",base,base->header,n);

   for (j = 0; j < num_edges; j++){
      if (!edges[j])
         new_edges[j] = NULL;
      else{
         basic_block src = edges[j]->src;
         basic_block dest = edges[j]->dest;
         if (src->flags & BB_DUPLICATED)
            src = mtcs_cfg_get_bb_copy/*!get_bb_copy*/(mtcsCfg,src);
         if (dest->flags & BB_DUPLICATED)
            dest = mtcs_cfg_get_bb_copy/*!get_bb_copy*/(mtcsCfg,dest);
         new_edges[j] = find_edge (src, dest);
      }
   }
   n_debug("mtcscfgcontext.c copy_bbs 66 loop:%p header:%p n:%d\n",base,base->header,n);

   /* Clear information about duplicates.  */
   for (i = 0; i < n; i++)
      bbs[i]->flags &= ~BB_DUPLICATED;
}


/* Return true if BB contains only labels or non-executable
   instructions */
//原型 empty_block_p cfghooks.h cfghooks.cc
bool mtcs_cfg_context_empty_block_p (MtcsCfgContext *self,basic_block bb)
{
   MtcsCfgState *mtcsCfgState=self->current;
   gcc_assert (mtcsCfgState->empty_block_p);
   return mtcsCfgState->empty_block_p(mtcsCfgState,bb);
}

/* Split a basic block if it ends with a conditional branch and if
   the other part of the block is not empty.  */
//原型 split_block_before_cond_jump cfghooks.h cfghooks.cc
basic_block mtcs_cfg_context_split_block_before_cond_jump (MtcsCfgContext *self,basic_block bb)
{
   MtcsCfgState *mtcsCfgState=self->current;
   gcc_assert (mtcsCfgState->split_block_before_cond_jump);
   return mtcsCfgState->split_block_before_cond_jump(mtcsCfgState,bb);
}
/* Work-horse for passes.cc:check_profile_consistency.
   Do book-keeping of the CFG for the profile consistency checker.
   Store the counting in RECORD.  */
//原型 profile_record_check_consistency cfghooks.h cfghooks.cc
void mtcs_cfg_context_profile_record_check_consistency (MtcsCfgContext *self,profile_record *record)
{
   basic_block bb;
   edge_iterator ei;
   edge e;

   FOR_ALL_BB_FN (bb, cfun){
      if (bb != EXIT_BLOCK_PTR_FOR_FN (cfun)
      && profile_status_for_fn (cfun) != PROFILE_ABSENT
      && EDGE_COUNT (bb->succs)){
         sreal sum = 0;
         bool found = false;
         FOR_EACH_EDGE (e, ei, bb->succs){
            if (!(e->flags & (EDGE_EH | EDGE_FAKE)))
               found = true;
            if (e->probability.initialized_p ())
               sum += e->probability.to_sreal ();
         }
         double dsum = sum.to_double ();
         if (found && (dsum < 0.9 || dsum > 1.1)
         && !(bb->count == profile_count::zero ())){
            record->num_mismatched_prob_out++;
            dsum = dsum > 1 ? dsum - 1 : 1 - dsum;
            if (profile_info){
               if (ENTRY_BLOCK_PTR_FOR_FN(cfun)->count.ipa ().initialized_p ()
               && ENTRY_BLOCK_PTR_FOR_FN(cfun)->count.ipa ().nonzero_p ()
               && bb->count.ipa ().initialized_p ())
                  record->dyn_mismatched_prob_out += dsum * bb->count.ipa ().to_gcov_type ();
            }else if (bb->count.initialized_p ())
               record->dyn_mismatched_prob_out += dsum * bb->count.to_sreal_scale(ENTRY_BLOCK_PTR_FOR_FN (cfun)->count).to_double ();
         }
      }
      if (bb != ENTRY_BLOCK_PTR_FOR_FN (cfun) && profile_status_for_fn (cfun) != PROFILE_ABSENT){
         profile_count lsum = profile_count::zero ();
         FOR_EACH_EDGE (e, ei, bb->preds)
            lsum += e->count ();
         if (lsum.differs_from_p (bb->count)){
            record->num_mismatched_count_in++;
            profile_count max;
            if (lsum < bb->count)
               max = bb->count;
            else
               max = lsum;
            if (profile_info){
               if (ENTRY_BLOCK_PTR_FOR_FN(cfun)->count.ipa ().initialized_p ()
               && ENTRY_BLOCK_PTR_FOR_FN (cfun)->count.ipa ().nonzero_p ()
               && max.ipa ().initialized_p ())
                  record->dyn_mismatched_count_in += max.ipa ().to_gcov_type ();
            }else if (bb->count.initialized_p ())
               record->dyn_mismatched_prob_out += max.to_sreal_scale(ENTRY_BLOCK_PTR_FOR_FN (cfun)->count).to_double ();
         }
      }
      if (bb == ENTRY_BLOCK_PTR_FOR_FN (cfun) || bb == EXIT_BLOCK_PTR_FOR_FN (cfun))
         continue;
   }
}

/* Work-horse for passes.cc:acount_profile.
   Do book-keeping of the CFG for the profile accounting.
   Store the counting in RECORD.  */
//原型 profile_record_account_profile cfghooks.h cfghooks.cc
void mtcs_cfg_context_profile_record_account_profile (MtcsCfgContext *self,profile_record *record)
{
   MtcsCfgState *mtcsCfgState=self->current;
   basic_block bb;
   FOR_ALL_BB_FN (bb, cfun){
      gcc_assert (mtcsCfgState->account_profile_record);
      mtcsCfgState->account_profile_record(mtcsCfgState,bb, record);
   }
}

void mtcs_cfg_context_change_gimple_state(MtcsCfgContext *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    if( self->mtcsCfgGimpleState==NULL)
       self->mtcsCfgGimpleState = (MtcsCfgState *)mtcs_cfg_gimple_state_new(mtcsMode);
    self->current=self->mtcsCfgGimpleState;
}

void mtcs_cfg_context_change_rtl_state(MtcsCfgContext *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   if( self->mtcsCfgRtlState==NULL)
      self->mtcsCfgRtlState = (MtcsCfgState *)mtcs_cfg_rtl_state_new(mtcsMode);
   self->current=self->mtcsCfgRtlState;
}

void mtcs_cfg_context_change_layout_state(MtcsCfgContext *self)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    if( self->mtcsCfgLayoutState==NULL)
       self->mtcsCfgLayoutState = (MtcsCfgState *)mtcs_cfg_layout_state_new(mtcsMode);
    self->current=self->mtcsCfgLayoutState;
}

int  mtcs_cfg_context_get_state(MtcsCfgContext *self)
{
   if(self->current==NULL){
      n_error("MtcsCfgContext的状态是空的\n");
   }
   return (int)self->current->stateType;
}

/* Check control flow invariants, if internal consistency checks are
   enabled.  */
//原型 inline void checking_verify_flow_info (void) cfghooks.h
void mtcs_cfg_context_checking_verify_flow_info (MtcsCfgContext *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
  /* TODO: Add a separate option for -fchecking=cfg.  */
  if (mtcsOptionsItem->x_flag_checking)
     mtcs_cfg_context_verify_flow_info/*!verify_flow_info*/(self);
}

static void mtcsCfgContextInit(MtcsCfgContext *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   self->mtcsCfgRtlState = NULL;
   self->mtcsCfgLayoutState =NULL;
   mtcs_cfg_context_change_gimple_state(self);
}

MtcsCfgContext *mtcs_cfg_context_new(MtcsMode *mtcsMode)
{
      MtcsCfgContext *self = n_slice_alloc0 (sizeof(MtcsCfgContext));
      mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
      mtcsCfgContextInit(self);
      return self;
}

#if __GNUC__ >= 10
#  pragma GCC diagnostic pop
#endif
