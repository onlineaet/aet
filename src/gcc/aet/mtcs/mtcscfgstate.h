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

#ifndef __GCC_MTCS_CFG_STATE__
#define __GCC_MTCS_CFG_STATE__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "predict.h"

typedef struct _MtcsCfgState MtcsCfgState;
struct _MtcsCfgState
{
     MtcsComponent parent;
     /* Name of the corresponding ir.  */
     const char *name;

     /* Debugging.  */
     bool (*verify_flow_info) (MtcsCfgState *self);
     void (*dump_bb) (MtcsCfgState *self,FILE *, basic_block, int, dump_flags_t);
     void (*dump_bb_for_graph) (MtcsCfgState *self,pretty_printer *, basic_block);

     /* Basic CFG manipulation.  */

     /* Return new basic block.  */
     basic_block (*create_basic_block) (MtcsCfgState *self,void *head, void *end, basic_block after);

     /* Redirect edge E to the given basic block B and update underlying program
        representation.  Returns edge representing redirected branch (that may not
        be equivalent to E in the case of duplicate edges being removed) or NULL
        if edge is not easily redirectable for whatever reason.  */
     edge (*redirect_edge_and_branch) (MtcsCfgState *self,edge e, basic_block b);

     /* Same as the above but allows redirecting of fallthru edges.  In that case
        newly created forwarder basic block is returned.  The edge must
        not be abnormal.  */
     basic_block (*redirect_edge_and_branch_force) (MtcsCfgState *self,edge, basic_block);

     /* Returns true if it is possible to remove the edge by redirecting it
        to the destination of the other edge going from its source.  */
     bool (*can_remove_branch_p) (MtcsCfgState *self,const_edge);

     /* Remove statements corresponding to a given basic block.  */
     void (*delete_basic_block) (MtcsCfgState *self,basic_block);

     /* Creates a new basic block just after basic block B by splitting
        everything after specified instruction I.  */
     basic_block (*split_block) (MtcsCfgState *self,basic_block b, void * i);

     /* Move block B immediately after block A.  */
     bool (*move_block_after) (MtcsCfgState *self,basic_block b, basic_block a);

     /* Return true when blocks A and B can be merged into single basic block.  */
     bool (*can_merge_blocks_p) (MtcsCfgState *self,basic_block a, basic_block b);

     /* Merge blocks A and B.  */
     void (*merge_blocks) (MtcsCfgState *self,basic_block a, basic_block b);

     /* Predict edge E using PREDICTOR to given PROBABILITY.  */
     void (*predict_edge) (MtcsCfgState *self,edge e, enum br_predictor predictor, int probability);

     /* Return true if the one of outgoing edges is already predicted by
        PREDICTOR.  */
     bool (*predicted_by_p) (MtcsCfgState *self,const_basic_block bb, enum br_predictor predictor);

     /* Return true when block A can be duplicated.  */
     bool (*can_duplicate_block_p) (MtcsCfgState *self,const_basic_block a);

     /* Duplicate block A.  */
     basic_block (*duplicate_block) (MtcsCfgState *self,basic_block a, copy_bb_data *);

     /* Higher level functions representable by primitive operations above if
        we didn't have some oddities in RTL and Tree representations.  */
     basic_block (*split_edge) (MtcsCfgState *self,edge);
     void (*make_forwarder_block) (MtcsCfgState *self,edge);

     /* Try to make the edge fallthru.  */
     void (*tidy_fallthru_edge) (MtcsCfgState *self,edge);

     /* Make the edge non-fallthru.  */
     basic_block (*force_nonfallthru) (MtcsCfgState *self,edge);

     /* Say whether a block ends with a call, possibly followed by some
        other code that must stay with the call.  */
     bool (*block_ends_with_call_p) (MtcsCfgState *self,basic_block);

     /* Say whether a block ends with a conditional branch.  Switches
        and unconditional branches do not qualify.  */
     bool (*block_ends_with_condjump_p) (MtcsCfgState *self,const_basic_block);

     /* Add fake edges to the function exit for any non constant and non noreturn
        calls, volatile inline assembly in the bitmap of blocks specified by
        BLOCKS or to the whole CFG if BLOCKS is zero.  Return the number of blocks
        that were split.

        The goal is to expose cases in which entering a basic block does not imply
        that all subsequent instructions must be executed.  */
     int (*flow_call_edges_add) (MtcsCfgState *self,sbitmap);

     /* This function is called immediately after edge E is added to the
        edge vector E->dest->preds.  */
     void (*execute_on_growing_pred) (MtcsCfgState *self,edge);

     /* This function is called immediately before edge E is removed from
        the edge vector E->dest->preds.  */
     void (*execute_on_shrinking_pred) (MtcsCfgState *self,edge);

     /* A hook for duplicating loop in CFG, currently this is used
        in loop versioning.  */
     bool (*cfg_hook_duplicate_loop_body_to_header_edge) (MtcsCfgState *self,class loop *, edge,
                            unsigned, sbitmap, edge,
                            vec<edge> *, int);

     /* Add condition to new basic block and update CFG used in loop
        versioning.  */
     void (*lv_add_condition_to_bb) (MtcsCfgState *self,basic_block, basic_block, basic_block,void *);
     /* Update the PHI nodes in case of loop versioning.  */
     void (*lv_adjust_loop_header_phi) (MtcsCfgState *self,basic_block, basic_block,basic_block, edge);

     /* Given a condition BB extract the true/false taken/not taken edges
        (depending if we are on tree's or RTL). */
     void (*extract_cond_bb_edges) (MtcsCfgState *self,basic_block, edge *, edge *);


     /* Add PHI arguments queued in PENDINT_STMT list on edge E to edge
        E->dest (only in tree-ssa loop versioning.  */
     void (*flush_pending_stmts) (MtcsCfgState *self,edge);

     /* True if a block contains no executable instructions.  */
     bool (*empty_block_p) (MtcsCfgState *self,basic_block);

     /* Split a basic block if it ends with a conditional branch and if
        the other part of the block is not empty.  */
     basic_block (*split_block_before_cond_jump) (MtcsCfgState *self,basic_block);

     /* Do book-keeping of a basic block for the profile consistency checker.  */
     void (*account_profile_record) (MtcsCfgState *self,basic_block, struct profile_record *);

     int stateType ;//状态是gimple rtl 或layoutrtl三种之一，具体值 enum ir_type
};

void mtcs_cfg_state_init(MtcsCfgState *self);
enum ir_type mtcs_cfg_state_get_state_type(MtcsCfgState *self);
//原型  basic_block (*create_basic_block) (MtcsCfgState *self,void *head, void *end, basic_block after);
basic_block mtcs_cfg_state_create_basic_block (MtcsCfgState *self,void *headp, void *endp, basic_block after);
//原型 cfg_layout_rtl_cfg_hooks member rtl_verify_flow_info_1 cfgrtl.cc
bool mtcs_cfg_state_verify_flow_info_1 (MtcsCfgState *self);

/***以下是公共方法****/
//原型 fixup_partition_crossing cfgrtl.cc
void mtcs_cfg_state_fixup_partition_crossing (MtcsCfgState *self,edge e);
//原型 redirect_branch_edge cfgrtl.cc
edge mtcs_cfg_state_redirect_branch_edge (MtcsCfgState *self,edge e, basic_block target);
//原型 rtl_delete_block cfgrtl.cc
void mtcs_cfg_state_rtl_delete_block (MtcsCfgState *self,basic_block b);
//原型 rtl_split_block cfgrtl.cc
basic_block mtcs_cfg_state_split_block (MtcsCfgState *self, basic_block bb, void *insnp);
//原型 cfg_layout_duplicate_bb cfgrtl.cc
basic_block mtcs_cfg_state_duplicate_bb (MtcsCfgState *self,basic_block bb, copy_bb_data *id);
//原型 patch_jump_insn cfgrtl.cc
bool mtcs_cfg_state_patch_jump_insn (MtcsCfgState *self,rtx_insn *insn, rtx_insn *old_label, basic_block new_bb);

#endif

