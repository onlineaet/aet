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

#ifndef __GCC_MTCS_CFG_CONTEXT__
#define __GCC_MTCS_CFG_CONTEXT__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcscfgstate.h"

typedef struct _MtcsCfgContext MtcsCfgContext;
struct _MtcsCfgContext
{
     MtcsComponent parent;
     MtcsCfgState *current;
     MtcsCfgState *mtcsCfgGimpleState;
     MtcsCfgState *mtcsCfgRtlState;
     MtcsCfgState *mtcsCfgLayoutState;
};

MtcsCfgContext *mtcs_cfg_context_new(MtcsMode *mtcsMode);
//原型 verify_flow_info cfghooks.h cfghooks.cc
void mtcs_cfg_context_verify_flow_info (MtcsCfgContext *self);
//原型 dump_bb cfghooks.h cfghooks.cc
void mtcs_cfg_context_dump_bb (MtcsCfgContext *self,FILE *outf, basic_block bb, int indent, dump_flags_t flags);
//原型 debug cfg.h cfghooks.cc
void mtcs_cfg_context_debug (MtcsCfgContext *self,basic_block_def &ref);
//原型 debug cfg.h cfghooks.cc
void mtcs_cfg_context_debug (MtcsCfgContext *self,basic_block_def *ptr);
//原型 dump_bb_for_graph cfghooks.h cfghooks.cc
void mtcs_cfg_context_dump_bb_for_graph (MtcsCfgContext *self,pretty_printer *pp, basic_block bb);
//原型 dump_flow_info cfghooks.h cfghooks.cc
void mtcs_cfg_context_dump_flow_info (MtcsCfgContext *self,FILE *file, dump_flags_t flags);
//原型 debug_flow_info  cfghooks.cc
void mtcs_cfg_context_debug_flow_info (MtcsCfgContext *self);
//原型 redirect_edge_and_branch cfghooks.h cfghooks.cc
edge mtcs_cfg_context_redirect_edge_and_branch (MtcsCfgContext *self,edge e, basic_block dest);
//原型 can_remove_branch_p cfghooks.h cfghooks.cc
bool mtcs_cfg_context_can_remove_branch_p (MtcsCfgContext *self,const_edge e);
//原型 can_remove_branch_p cfghooks.h cfghooks.cc
void mtcs_cfg_context_remove_branch (MtcsCfgContext *self,edge e);
//原型 remove_edge cfghooks.h cfghooks.cc
void mtcs_cfg_context_remove_edge (MtcsCfgContext *self,edge e);
//原型 redirect_edge_succ_nodup cfghooks.h cfghooks.cc
edge mtcs_cfg_context_redirect_edge_succ_nodup (MtcsCfgContext *self,edge e, basic_block new_succ);
//原型 redirect_edge_and_branch_force cfghooks.h cfghooks.cc
basic_block mtcs_cfg_context_redirect_edge_and_branch_force (MtcsCfgContext *self,edge e, basic_block dest);
//原型 split_block cfghooks.h cfghooks.cc
edge mtcs_cfg_context_split_block (MtcsCfgContext *self,basic_block bb, gimple *i);
//原型 split_block cfghooks.h cfghooks.cc
edge mtcs_cfg_context_split_block (MtcsCfgContext *self,basic_block bb, rtx i);
//原型 split_block_after_labels cfghooks.h cfghooks.cc
edge mtcs_cfg_context_split_block_after_labels (MtcsCfgContext *self,basic_block bb);
//原型 move_block_after cfghooks.h cfghooks.cc
bool mtcs_cfg_context_move_block_after (MtcsCfgContext *self,basic_block bb, basic_block after);
//原型 delete_basic_block cfghooks.h cfghooks.cc
void  mtcs_cfg_context_delete_basic_block (MtcsCfgContext *self,basic_block bb);
//原型 split_edge cfghooks.h cfghooks.cc
basic_block mtcs_cfg_context_split_edge (MtcsCfgContext *self,edge e);
//原型 create_basic_block cfghooks.h cfghooks.cc
basic_block mtcs_cfg_context_create_basic_block (MtcsCfgContext *self,gimple_seq seq, basic_block after);
//原型 create_basic_block cfghooks.h cfghooks.cc
basic_block mtcs_cfg_context_create_basic_block (MtcsCfgContext *self,rtx head, rtx end, basic_block after);
//原型 can_merge_blocks_p cfghooks.h cfghooks.cc
bool mtcs_cfg_context_can_merge_blocks_p (MtcsCfgContext *self,basic_block bb1, basic_block bb2);
//原型 merge_blocks cfghooks.h cfghooks.cc
void mtcs_cfg_context_merge_blocks (MtcsCfgContext *self,basic_block a, basic_block b);
//原型 make_forwarder_block cfghooks.h cfghooks.cc
edge mtcs_cfg_context_make_forwarder_block (MtcsCfgContext *self,basic_block bb, bool (*redirect_edge_p) (edge),
            void (*new_bb_cbk) (basic_block));

//原型 tidy_fallthru_edge cfghooks.h cfghooks.cc
void mtcs_cfg_context_tidy_fallthru_edge (MtcsCfgContext *self,edge e);
//原型 tidy_fallthru_edges cfghooks.h cfghooks.cc
void mtcs_cfg_context_tidy_fallthru_edges (MtcsCfgContext *self);
//原型 force_nonfallthru cfghooks.h cfghooks.cc
basic_block mtcs_cfg_context_force_nonfallthru (MtcsCfgContext *self,edge e);
//原型 can_duplicate_block_p cfghooks.h cfghooks.cc
bool mtcs_cfg_context_can_duplicate_block_p (MtcsCfgContext *self,const_basic_block bb);
//原型 duplicate_block cfghooks.h cfghooks.cc
basic_block mtcs_cfg_context_duplicate_block (MtcsCfgContext *self,basic_block bb, edge e, basic_block after, copy_bb_data *id=NULL);
//原型 block_ends_with_call_p cfghooks.h cfghooks.cc
bool  mtcs_cfg_context_block_ends_with_call_p (MtcsCfgContext *self,basic_block bb);
//原型 block_ends_with_condjump_p cfghooks.h cfghooks.cc
bool  mtcs_cfg_context_block_ends_with_condjump_p (MtcsCfgContext *self,const_basic_block bb);
//原型 flow_call_edges_add cfghooks.h cfghooks.cc
int mtcs_cfg_context_block_flow_call_edges_add (MtcsCfgContext *self,sbitmap blocks);
//原型 execute_on_growing_pred cfghooks.h cfghooks.cc
void mtcs_cfg_context_execute_on_growing_pred (MtcsCfgContext *self,edge e);
//原型 execute_on_shrinking_pred cfghooks.h cfghooks.cc
void mtcs_cfg_context_execute_on_shrinking_pred (MtcsCfgContext *self,edge e);
//原型 lv_flush_pending_stmts cfghooks.h cfghooks.cc
void mtcs_cfg_context_lv_flush_pending_stmts (MtcsCfgContext *self,edge e);
//原型 cfg_hook_duplicate_loop_body_to_header_edge cfghooks.h cfghooks.cc
bool mtcs_cfg_context_cfg_hook_duplicate_loop_body_to_header_edge (MtcsCfgContext *self,class loop *loop, edge e,
                    unsigned int ndupl,sbitmap wont_exit, edge orig,vec<edge> *to_remove, int flags);
//原型 extract_cond_bb_edges cfghooks.h cfghooks.cc
void mtcs_cfg_context_extract_cond_bb_edges (MtcsCfgContext *self,basic_block b, edge *e1, edge *e2);
//原型 lv_adjust_loop_header_phi cfghooks.h cfghooks.cc
void mtcs_cfg_context_lv_adjust_loop_header_phi (MtcsCfgContext *self,basic_block first, basic_block second,
            basic_block new_block, edge e);
//原型 lv_add_condition_to_bb cfghooks.h cfghooks.cc
void mtcs_cfg_context_lv_add_condition_to_bb (MtcsCfgContext *self,basic_block first, basic_block second,
         basic_block new_block, void *cond);
//原型 can_copy_bbs_p cfghooks.h cfghooks.cc
bool mtcs_cfg_context_can_copy_bbs_p (MtcsCfgContext *self,basic_block *bbs, unsigned n);
//原型 copy_bbs cfghooks.h cfghooks.cc
void mtcs_cfg_context_copy_bbs (MtcsCfgContext *self,basic_block *bbs, unsigned n, basic_block *new_bbs,
     edge *edges, unsigned num_edges, edge *new_edges,class loop *base, basic_block after, bool update_dominance);
//原型 empty_block_p cfghooks.h cfghooks.cc
bool mtcs_cfg_context_empty_block_p (MtcsCfgContext *self,basic_block bb);
//原型 split_block_before_cond_jump cfghooks.h cfghooks.cc
basic_block mtcs_cfg_context_split_block_before_cond_jump (MtcsCfgContext *self,basic_block bb);
//原型 profile_record_check_consistency cfghooks.h cfghooks.cc
void mtcs_cfg_context_profile_record_check_consistency (MtcsCfgContext *self,profile_record *record);
//原型 profile_record_account_profile cfghooks.h cfghooks.cc
void mtcs_cfg_context_profile_record_account_profile (MtcsCfgContext *self,profile_record *record);
//原型 inline void checking_verify_flow_info (void) cfghooks.h
void mtcs_cfg_context_checking_verify_flow_info (MtcsCfgContext *self);

void mtcs_cfg_context_change_gimple_state(MtcsCfgContext *self);
void mtcs_cfg_context_change_rtl_state(MtcsCfgContext *self);
void mtcs_cfg_context_change_layout_state(MtcsCfgContext *self);
int  mtcs_cfg_context_get_state(MtcsCfgContext *self);

#endif

