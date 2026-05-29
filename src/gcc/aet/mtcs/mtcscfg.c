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
 * base on cfg.cc
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

#include "cfganal.h"
#include "cfgcleanup.h"
#include "alias.h"
#include "rtlhooks-def.h"
#include "tree-pass.h"
#include "dbgcnt.h"
#include "rtlanal.h"

#include "aet/aetprinttree.h"
#include "mtcscfg.h"
#include "mtcstarget.h"
#include "mtcsdfcore.h"
#include "mtcsdfproblems.h"

static void mtcsCfgInit(MtcsCfg *self)
{

}

static void test_rtl_verify_edges (basic_block bb)
{
   if(!n_log_is_debug())
      return;
   int  n_branch = 0;
   int n_eh = 0, n_abnormal = 0;
   edge e, fallthru = NULL;
   edge_iterator ei;
   if( bb->index==14 || bb->index==15 || bb->index==16 || bb->index==17) {
      FOR_EACH_EDGE (e, ei, bb->succs){
          n_debug("mtcscfg.c rtl_verify_edges xxx e:%p flags:%d\n",e,e->flags);
          if ((e->flags & ~(EDGE_DFS_BACK
                    | EDGE_CAN_FALLTHRU
                    | EDGE_IRREDUCIBLE_LOOP
                    | EDGE_LOOP_EXIT
                    | EDGE_CROSSING
                    | EDGE_PRESERVE)) == 0)
            n_branch++;
      }
      n_debug("mtcscfg.c rtl_verify_edges 00 bb:%p index:%d n_branch:%d BB_END (bb):%p \n",
            bb,bb->index,n_branch,BB_END (bb));
   }
}

/* Disconnect edge E from E->src.  */
//原型 disconnect_src cfg.cc

static inline void disconnect_src (MtcsCfg *self,edge e)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);

   basic_block src = e->src;
   edge_iterator ei;
   edge tmp;

   for (ei = ei_start (src->succs); (tmp = ei_safe_edge (ei)); ){
      if (tmp == e){
         src->succs->unordered_remove (ei.index);
         mtcs_dfcore_df_mark_solutions_dirty/*!df_mark_solutions_dirty*/(mtcsDfcore);
         return;
      } else
         ei_next (&ei);
   }

   gcc_unreachable ();
}

/* Disconnect edge E from E->dest.  */
//原型 disconnect_dest cfg.cc
static inline void disconnect_dest (MtcsCfg *self,edge e)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);

   basic_block dest = e->dest;
   unsigned int dest_idx = e->dest_idx;

   dest->preds->unordered_remove (dest_idx);

   /* If we removed an edge in the middle of the edge vector, we need
   to update dest_idx of the edge that moved into the "hole".  */
   if (dest_idx < EDGE_COUNT (dest->preds))
      EDGE_PRED (dest, dest_idx)->dest_idx = dest_idx;
   mtcs_dfcore_df_mark_solutions_dirty/*!df_mark_solutions_dirty*/(mtcsDfcore);
}

/* Connect E to E->src.  */
//原型 connect_src cfg.cc
static inline void connect_src (MtcsCfg *self,edge e)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);

   vec_safe_push (e->src->succs, e);
   mtcs_dfcore_df_mark_solutions_dirty/*!df_mark_solutions_dirty*/(mtcsDfcore);
}

/* Connect E to E->dest.  */
//原型 connect_dest cfg.cc
static inline void connect_dest (MtcsCfg *self,edge e)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);

   basic_block dest = e->dest;
   vec_safe_push (dest->preds, e);
   e->dest_idx = EDGE_COUNT (dest->preds) - 1;
   mtcs_dfcore_df_mark_solutions_dirty/*!df_mark_solutions_dirty*/(mtcsDfcore);
}

/* Helper function for remove_edge and free_cffg.  Frees edge structure
   without actually removing it from the pred/succ arrays.  */
//原型 free_edge cfg.cc
static void free_edge (function *fn, edge e)
{
  n_edges_for_fn (fn)--;
  ggc_free (e);
}

/* Redirect an edge's successor from one block to another.  */
//原型 redirect_edge_succ cfg.h cfg.cc
void mtcs_cfg_redirect_edge_succ (MtcsCfg *self,edge e, basic_block new_succ)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);

   mtcs_cfg_context_execute_on_shrinking_pred/*!execute_on_shrinking_pred*/(mtcsCfgContext,e);
   disconnect_dest(self,e);
   e->dest = new_succ;
   /* Reconnect the edge to the new successor block.  */
   connect_dest(self,e);
   mtcs_cfg_context_execute_on_growing_pred/*!execute_on_growing_pred*/(mtcsCfgContext,e);
}

/* Create an edge connecting SRC and DEST with flags FLAGS.  Return newly
   created edge.  Use this only if you are sure that this edge can't
   possibly already exist.  */
//原型 unchecked_make_edge cfg.h cfg.cc
edge mtcs_cfg_unchecked_make_edge (MtcsCfg *self,basic_block src, basic_block dst, int flags)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);

   edge e;
   e = ggc_cleared_alloc<edge_def> ();
   n_edges_for_fn (cfun)++;
   e->probability = profile_probability::uninitialized ();
   e->src = src;
   e->dest = dst;
   e->flags = flags;
   n_debug("mtcscfg.c  unchecked_make_edge 00 src:%p %d dest:%p %d flags:%d\n",src,src->index,dst,dst->index,flags);
   test_rtl_verify_edges(src);
   connect_src(self,e);
   n_debug("mtcscfg.c unchecked_make_edge 11 src:%p %d dest:%p %d flags:%d\n",src,src->index,dst,dst->index,flags);
   test_rtl_verify_edges(src);
   connect_dest(self,e);
   mtcs_cfg_context_execute_on_growing_pred/*!execute_on_growing_pred*/(mtcsCfgContext,e);
   return e;
}


/* Create an edge connecting SRC and DST with FLAGS optionally using
   edge cache CACHE.  Return the new edge, NULL if already exist.  */
//原型 cached_make_edge cfg.h cfg.cc
edge mtcs_cfg_cached_make_edge (MtcsCfg *self,sbitmap edge_cache, basic_block src, basic_block dst, int flags)
{
   if (edge_cache == NULL
   || src == ENTRY_BLOCK_PTR_FOR_FN (cfun)
   || dst == EXIT_BLOCK_PTR_FOR_FN (cfun)){
      n_debug("mtcscfg.c cached_make_edge 00 src:%p %d dst:%p %d flags:%d\n",src,src->index,dst,dst->index,flags);

      return mtcs_cfg_make_edge/*!make_edge*/(self,src, dst, flags);
   }
   /* Does the requested edge already exist?  */
   if (! bitmap_bit_p (edge_cache, dst->index)){
      /* The edge does not exist.  Create one and update the
      cache.  */
      bitmap_set_bit (edge_cache, dst->index);
      return mtcs_cfg_unchecked_make_edge/*!unchecked_make_edge*/(self,src, dst, flags);
   }
   /* At this point, we know that the requested edge exists.  Adjust
   flags if necessary.  */
   if (flags){
      edge e = find_edge (src, dst);
      e->flags |= flags;
   }
   return NULL;
}


/* Create an edge connecting SRC and DEST with flags FLAGS.  Return newly
   created edge or NULL if already exist.  */
//原型 make_edge cfg.h cfg.cc
edge mtcs_cfg_make_edge (MtcsCfg *self,basic_block src, basic_block dest, int flags)
{
   edge e = find_edge (src, dest);
   /* Make sure we don't add duplicate edges.  */
   if (e){
      n_debug("mtcscfg.c mtcs_cfg_make_edge src:%p %d dest:%p %d e->flags:%d flags:%d\n",
            src,src->index,dest,dest->index, e->flags,flags);
      e->flags |= flags;
      return NULL;
   }
   return mtcs_cfg_unchecked_make_edge/*!unchecked_make_edge*/(self,src, dest, flags);
}

/* Create an edge connecting SRC to DEST and set probability by knowing
   that it is the single edge leaving SRC.  */
//原型 make_single_succ_edge cfg.h cfg.cc
edge mtcs_cfg_make_single_succ_edge (MtcsCfg *self,basic_block src, basic_block dest, int flags)
{
  edge e = mtcs_cfg_make_edge/*!make_edge*/(self,src, dest, flags);
  e->probability = profile_probability::always ();
  return e;
}

/* This function will remove an edge from the flow graph.  */
//原型 remove_edge_raw cfg.h cfg.cc
void mtcs_cfg_remove_edge_raw (MtcsCfg *self,edge e)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);

   remove_predictions_associated_with_edge (e);
   mtcs_cfg_context_execute_on_shrinking_pred/*!execute_on_shrinking_pred*/(mtcsCfgContext,e);

   disconnect_src(self,e);
   disconnect_dest(self,e);

   free_edge (cfun, e);
}

/* Sequentially order blocks and compact the arrays.  */
//原型 compact_blocks cfg.h cfg.cc
void mtcs_cfg_compact_blocks (MtcsCfg *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);

   int i;

   SET_BASIC_BLOCK_FOR_FN (cfun, ENTRY_BLOCK, ENTRY_BLOCK_PTR_FOR_FN (cfun));
   SET_BASIC_BLOCK_FOR_FN (cfun, EXIT_BLOCK, EXIT_BLOCK_PTR_FOR_FN (cfun));

   if (df)
      mtcs_dfcore_df_compact_blocks/*!df_compact_blocks*/(mtcsDfcore);
   else{
      basic_block bb;

      i = NUM_FIXED_BLOCKS;
      FOR_EACH_BB_FN (bb, cfun){
         SET_BASIC_BLOCK_FOR_FN (cfun, i, bb);
         bb->index = i;
         i++;
      }
      gcc_assert (i == n_basic_blocks_for_fn (cfun));

      for (; i < last_basic_block_for_fn (cfun); i++)
         SET_BASIC_BLOCK_FOR_FN (cfun, i, NULL);
   }
   last_basic_block_for_fn (cfun) = n_basic_blocks_for_fn (cfun);
}

/* Initialize the data structures to maintain mapping between blocks
   and its copies.  */
//原型 initialize_original_copy_tables cfg.h cfg.cc
void mtcs_cfg_initialize_original_copy_tables (MtcsCfg *self)
{
  self->bb_original = new  hash_map<int_hash<int, -1, -2>, int>  (10);
  self->bb_copy = new  hash_map<int_hash<int, -1, -2>, int>  (10);
  self->loop_copy = new  hash_map<int_hash<int, -1, -2>, int>  (10);
}

/* Reset the data structures to maintain mapping between blocks and
   its copies.  */
//原型 reset_original_copy_tables cfg.h cfg.cc
void mtcs_cfg_reset_original_copy_tables (MtcsCfg *self)
{
  self->bb_original->empty ();
  self->bb_copy->empty ();
  self->loop_copy->empty ();
}

/* Free the data structures to maintain mapping between blocks and
   its copies.  */
//原型 free_original_copy_tables cfg.h cfg.cc
void mtcs_cfg_free_original_copy_tables (MtcsCfg *self)
{
  delete self->bb_copy;
  self->bb_copy = NULL;
  delete self->bb_original;
  self->bb_original = NULL;
  delete self->loop_copy;
  self->loop_copy = NULL;
}

/* Return true iff we have had a call to initialize_original_copy_tables
   without a corresponding call to free_original_copy_tables.  */
//原型 original_copy_tables_initialized_p cfg.h cfg.cc
bool mtcs_cfg_original_copy_tables_initialized_p (MtcsCfg *self)
{
  return self->bb_copy != NULL;
}

/* Removes the value associated with OBJ from table TAB.  */
static void copy_original_table_clear (MtcsCfg *self,hash_map<int_hash<int, -1, -2>, int>  *tab, unsigned obj)
{
  if (!mtcs_cfg_original_copy_tables_initialized_p/*!original_copy_tables_initialized_p*/(self))
    return;

  tab->remove (obj);
}

/* Sets the value associated with OBJ in table TAB to VAL.
   Do nothing when data structures are not initialized.  */
static void copy_original_table_set (MtcsCfg *self,hash_map<int_hash<int, -1, -2>, int>  *tab, unsigned obj, unsigned val)
{
  if (!mtcs_cfg_original_copy_tables_initialized_p/*!original_copy_tables_initialized_p*/(self))
    return;
  n_debug("mtcscfg.c copy_original_table_set:%p %p %d %d\n",self->bb_copy,tab,obj,val);
  tab->put (obj, val);
}

/* Set original for basic block.  Do nothing when data structures are not
   initialized so passes not needing this don't need to care.  */
//原型 set_bb_original cfg.h cfg.cc
void mtcs_cfg_set_bb_original (MtcsCfg *self,basic_block bb, basic_block original)
{
  copy_original_table_set (self,self->bb_original, bb->index, original->index);
}

/* Get the original basic block.  */
//原型 get_bb_original cfg.h cfg.cc
basic_block mtcs_cfg_get_bb_original (MtcsCfg *self,basic_block bb)
{
  gcc_assert (mtcs_cfg_original_copy_tables_initialized_p/*!original_copy_tables_initialized_p*/(self));

  int *entry = self->bb_original->get (bb->index);
  if (entry)
    return BASIC_BLOCK_FOR_FN (cfun, *entry);
  else
    return NULL;
}

/* Set copy for basic block.  Do nothing when data structures are not
   initialized so passes not needing this don't need to care.  */
//原型 set_bb_copy cfg.h cfg.cc
void mtcs_cfg_set_bb_copy (MtcsCfg *self,basic_block bb, basic_block copy)
{
  copy_original_table_set(self,self->bb_copy, bb->index, copy->index);
}

/* Get the copy of basic block.  */
//原型 get_bb_copy cfg.h cfg.cc
basic_block mtcs_cfg_get_bb_copy (MtcsCfg *self,basic_block bb)
{
  gcc_assert (mtcs_cfg_original_copy_tables_initialized_p/*!original_copy_tables_initialized_p*/(self));

  int *entry = self->bb_copy->get (bb->index);
  if (entry)
    return BASIC_BLOCK_FOR_FN (cfun, *entry);
  else
    return NULL;
}

/* Set copy for LOOP to COPY.  Do nothing when data structures are not
   initialized so passes not needing this don't need to care.  */
//原型 set_loop_copy cfg.h cfg.cc
void mtcs_cfg_set_loop_copy (MtcsCfg *self,class loop *loop, class loop *copy)
{
  if (!copy)
    copy_original_table_clear (self,self->loop_copy, loop->num);
  else
    copy_original_table_set (self,self->loop_copy, loop->num, copy->num);
}

/* Get the copy of LOOP.  */
//原型 get_loop_copy cfg.h cfg.cc
class loop *mtcs_cfg_get_loop_copy (MtcsCfg *self,class loop *loop)
{
   gcc_assert (mtcs_cfg_original_copy_tables_initialized_p/*!original_copy_tables_initialized_p*/(self));

   int *entry = self->loop_copy->get (loop->num);
   n_debug("mtcscfg.c mtcs_cfg_get_loop_copy 00 loop:%p loop->num:%d %d entry:%p %d\n",loop,loop->num,entry,*entry);
   if (entry)
      return get_loop (cfun, *entry);
   else
      return NULL;
}

/* Scales the frequencies of all basic blocks that are strictly
   dominated by BB by NUM/DEN.  */
//原型 scale_strictly_dominated_blocks cfg.h cfg.cc
void mtcs_cfg_scale_strictly_dominated_blocks (MtcsCfg *self,basic_block bb,
             profile_count num, profile_count den)
{
   basic_block son;

   if (!den.nonzero_p () && !(num == profile_count::zero ()))
      return;
   auto_vec <basic_block, 8> worklist;
   worklist.safe_push (bb);

   while (!worklist.is_empty ())
      for (son = first_dom_son (CDI_DOMINATORS, worklist.pop ());  son;  son = next_dom_son (CDI_DOMINATORS, son)){
         son->count = son->count.apply_scale (num, den);
         worklist.safe_push (son);
      }
}


MtcsCfg *mtcs_cfg_new(MtcsMode *mtcsMode)
{
     MtcsCfg *self = n_slice_alloc0 (sizeof(MtcsCfg));
     mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
     mtcsCfgInit(self);
     return self;
}


