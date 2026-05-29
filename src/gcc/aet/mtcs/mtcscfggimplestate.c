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
 * base on tree-cfg.cc
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
#include "mtcscfggimplestate.h"
#include "mtcstarget.h"
#include "mtcscompile.h"
//
//struct cfg_hooks gimple_cfg_hooks = {
//  "gimple",
//  gimple_verify_flow_info,
//  gimple_dump_bb,    /* dump_bb  */
//  gimple_dump_bb_for_graph,   /* dump_bb_for_graph  */
//  create_bb,         /* create_basic_block  */
//  gimple_redirect_edge_and_branch, /* redirect_edge_and_branch  */
//  gimple_redirect_edge_and_branch_force, /* redirect_edge_and_branch_force  */
//  gimple_can_remove_branch_p, /* can_remove_branch_p  */
//  remove_bb,         /* delete_basic_block  */
//  gimple_split_block,      /* split_block  */
//  gimple_move_block_after, /* move_block_after  */
//  gimple_can_merge_blocks_p,  /* can_merge_blocks_p  */
//  gimple_merge_blocks,     /* merge_blocks  */
//  gimple_predict_edge,     /* predict_edge  */
//  gimple_predicted_by_p,   /* predicted_by_p  */
//  gimple_can_duplicate_bb_p,  /* can_duplicate_block_p  */
//  gimple_duplicate_bb,     /* duplicate_block  */
//  gimple_split_edge,    /* split_edge  */
//  gimple_make_forwarder_block,   /* make_forward_block  */
//  NULL,           /* tidy_fallthru_edge  */
//  NULL,           /* force_nonfallthru */
//  gimple_block_ends_with_call_p,/* block_ends_with_call_p */
//  gimple_block_ends_with_condjump_p, /* block_ends_with_condjump_p */
//  gimple_flow_call_edges_add,   /* flow_call_edges_add */
//  gimple_execute_on_growing_pred,   /* execute_on_growing_pred */
//  gimple_execute_on_shrinking_pred, /* execute_on_shrinking_pred */
//  gimple_duplicate_loop_body_to_header_edge, /* duplicate loop for trees */
//  gimple_lv_add_condition_to_bb, /* lv_add_condition_to_bb */
//  gimple_lv_adjust_loop_header_phi, /* lv_adjust_loop_header_phi*/
//  extract_true_false_edges_from_block, /* extract_cond_bb_edges */
//  flush_pending_stmts,     /* flush_pending_stmts */
//  gimple_empty_block_p,           /* block_empty_p */
//  gimple_split_block_before_cond_jump, /* split_block_before_cond_jump */
//  gimple_account_profile_record,
//};


static bool verify_flow_info_cb(MtcsCfgState *self);
static void dump_bb_cb(MtcsCfgState *self,FILE * file, basic_block bb, int n, dump_flags_t flags);
static void dump_bb_for_graph_cb(MtcsCfgState *self,pretty_printer *print, basic_block bb);
static basic_block create_basic_block_cb(MtcsCfgState *self,void *head, void *end, basic_block after);
static edge redirect_edge_and_branch_cb(MtcsCfgState *self,edge e, basic_block b);
static basic_block redirect_edge_and_branch_force_cb (MtcsCfgState *self,edge, basic_block);
static bool can_remove_branch_p_cb (MtcsCfgState *self,const_edge);
static void delete_basic_block_cb (MtcsCfgState *self,basic_block);
static basic_block split_block_cb(MtcsCfgState *self,basic_block b, void * i);
static bool move_block_after_cb (MtcsCfgState *self,basic_block b, basic_block a);
static bool can_merge_blocks_p_cb (MtcsCfgState *self,basic_block a, basic_block b);
static void merge_blocks_cb (MtcsCfgState *self,basic_block a, basic_block b);
static void predict_edge_cb (MtcsCfgState *self,edge e, enum br_predictor predictor, int probability);
static bool predicted_by_p_cb (MtcsCfgState *self,const_basic_block bb, enum br_predictor predictor);
static bool can_duplicate_block_p_cb (MtcsCfgState *self,const_basic_block a);
static basic_block duplicate_block_cb (MtcsCfgState *self,basic_block a, copy_bb_data *data);
static basic_block split_edge_cb (MtcsCfgState *self,edge e);
static void make_forwarder_block_cb (MtcsCfgState *self,edge e);
static bool block_ends_with_call_p_cb (MtcsCfgState *self,basic_block bb);
static bool block_ends_with_condjump_p_cb (MtcsCfgState *self,const_basic_block bb);
static int flow_call_edges_add_cb (MtcsCfgState *self,sbitmap map);
static void execute_on_growing_pred_cb (MtcsCfgState *self,edge e);
static void execute_on_shrinking_pred_cb (MtcsCfgState *self,edge e);
static bool cfg_hook_duplicate_loop_body_to_header_edge_cb(MtcsCfgState *self,class loop *loop, edge e,
      unsigned int ndupl, sbitmap wont_exit, edge orig, vec<edge> *to_remove, int flags);
static void lv_add_condition_to_bb_cb (MtcsCfgState *self,basic_block first_head ,
                basic_block second_head ATTRIBUTE_UNUSED, basic_block cond_bb, void *comp_rtx);
static void lv_adjust_loop_header_phi_cb (MtcsCfgState *self,basic_block, basic_block,basic_block, edge);
static void extract_cond_bb_edges_cb (MtcsCfgState *self,basic_block b, edge *branch_edge,edge *fallthru_edge);
static void flush_pending_stmts_cb (MtcsCfgState *self,edge e);
static bool empty_block_p_cb (MtcsCfgState *self,basic_block bb);
static basic_block split_block_before_cond_jump_cb (MtcsCfgState *self,basic_block bb);
static void  account_profile_record_cb (MtcsCfgState *self,basic_block bb, struct profile_record *pr);


static void mtcsCfgGimpleStateInit(MtcsCfgGimpleState *self)
{
   MtcsCfgState *mtcsCfgState=(MtcsCfgState *)self;
   mtcsCfgState->name=n_strdup("gimple");
   mtcsCfgState->stateType=IR_GIMPLE;
   mtcsCfgState->verify_flow_info = verify_flow_info_cb;
   mtcsCfgState->dump_bb=dump_bb_cb;
   mtcsCfgState->dump_bb_for_graph=dump_bb_for_graph_cb;
   mtcsCfgState->create_basic_block = create_basic_block_cb;
   mtcsCfgState->redirect_edge_and_branch = redirect_edge_and_branch_cb;
   mtcsCfgState->redirect_edge_and_branch_force = redirect_edge_and_branch_force_cb;
   mtcsCfgState->can_remove_branch_p=can_remove_branch_p_cb;
   mtcsCfgState->delete_basic_block = delete_basic_block_cb;
   mtcsCfgState->split_block = split_block_cb;
   mtcsCfgState->move_block_after=move_block_after_cb;
   mtcsCfgState->can_merge_blocks_p = can_merge_blocks_p_cb;
   mtcsCfgState->merge_blocks = merge_blocks_cb;
   mtcsCfgState->predict_edge = predict_edge_cb;
   mtcsCfgState->predicted_by_p = predicted_by_p_cb;
   mtcsCfgState->can_duplicate_block_p = can_duplicate_block_p_cb;
   mtcsCfgState->duplicate_block = duplicate_block_cb;
   mtcsCfgState->split_edge = split_edge_cb;
   mtcsCfgState->make_forwarder_block=make_forwarder_block_cb;
   mtcsCfgState->tidy_fallthru_edge = NULL;
   mtcsCfgState->force_nonfallthru=NULL;
   mtcsCfgState->block_ends_with_call_p=block_ends_with_call_p_cb;
   mtcsCfgState->block_ends_with_condjump_p=block_ends_with_condjump_p_cb;
   mtcsCfgState->flow_call_edges_add=flow_call_edges_add_cb;
   mtcsCfgState->execute_on_growing_pred=execute_on_growing_pred_cb; /* execute_on_growing_pred */
   mtcsCfgState->execute_on_shrinking_pred=execute_on_shrinking_pred_cb; /* execute_on_shrinking_pred */
   mtcsCfgState->cfg_hook_duplicate_loop_body_to_header_edge=cfg_hook_duplicate_loop_body_to_header_edge_cb;
   mtcsCfgState->lv_add_condition_to_bb=lv_add_condition_to_bb_cb;
   mtcsCfgState->lv_adjust_loop_header_phi=lv_adjust_loop_header_phi_cb; /* lv_adjust_loop_header_phi*/
   mtcsCfgState->extract_cond_bb_edges=extract_cond_bb_edges_cb;
   mtcsCfgState->flush_pending_stmts=flush_pending_stmts_cb; /* flush_pending_stmts */
   mtcsCfgState->empty_block_p=empty_block_p_cb;
   mtcsCfgState->split_block_before_cond_jump=split_block_before_cond_jump_cb;
   mtcsCfgState->account_profile_record=account_profile_record_cb;

}

static bool verify_flow_info_cb(MtcsCfgState *self)
{
   return gimple_cfg_hooks.verify_flow_info();
}

static void dump_bb_cb(MtcsCfgState *self,FILE * file, basic_block bb, int n, dump_flags_t flags)
{
   gimple_cfg_hooks.dump_bb(file,bb,n,flags);

}

static void dump_bb_for_graph_cb(MtcsCfgState *self,pretty_printer *print, basic_block bb)
{
   gimple_cfg_hooks.dump_bb_for_graph(print,bb);

}

static basic_block create_basic_block_cb(MtcsCfgState *self,void *head, void *end, basic_block after)
{
   return gimple_cfg_hooks.create_basic_block(head,end,after);
}


static edge redirect_edge_and_branch_cb(MtcsCfgState *self,edge e, basic_block b)
{
   return gimple_cfg_hooks.redirect_edge_and_branch(e,b);
}


static basic_block redirect_edge_and_branch_force_cb (MtcsCfgState *self,edge e, basic_block bb)
{
   return gimple_cfg_hooks.redirect_edge_and_branch_force(e,bb);
}

static bool can_remove_branch_p_cb (MtcsCfgState *self,const_edge edge)
{
   return gimple_cfg_hooks.can_remove_branch_p(edge);
}

static void delete_basic_block_cb (MtcsCfgState *self,basic_block bb)
{
   gimple_cfg_hooks.delete_basic_block(bb);
}

static basic_block split_block_cb(MtcsCfgState *self,basic_block b, void * i)
{
   return gimple_cfg_hooks.split_block(b,i);
}

static bool move_block_after_cb (MtcsCfgState *self,basic_block b, basic_block a)
{
   return gimple_cfg_hooks.move_block_after(b,a);
}

static bool can_merge_blocks_p_cb (MtcsCfgState *self,basic_block a, basic_block b)
{
   return gimple_cfg_hooks.can_merge_blocks_p(a,b);

}

static void merge_blocks_cb (MtcsCfgState *self,basic_block a, basic_block b)
{
   gimple_cfg_hooks.merge_blocks(a,b);
}

static void predict_edge_cb (MtcsCfgState *self,edge e, enum br_predictor predictor, int probability)
{
   gimple_cfg_hooks.predict_edge(e,predictor,probability);
}

static bool predicted_by_p_cb (MtcsCfgState *self,const_basic_block bb, enum br_predictor predictor)
{
   return gimple_cfg_hooks.predicted_by_p(bb,predictor);
}

static bool can_duplicate_block_p_cb (MtcsCfgState *self,const_basic_block a)
{
   return gimple_cfg_hooks.can_duplicate_block_p(a);
}
static basic_block duplicate_block_cb (MtcsCfgState *self,basic_block a, copy_bb_data *data)
{
   return gimple_cfg_hooks.duplicate_block(a,data);
}

static basic_block split_edge_cb (MtcsCfgState *self,edge e)
{
   return gimple_cfg_hooks.split_edge(e);
}

static void make_forwarder_block_cb (MtcsCfgState *self,edge e)
{
   gimple_cfg_hooks.make_forwarder_block(e);
}

static bool block_ends_with_call_p_cb (MtcsCfgState *self,basic_block bb)
{
   return gimple_cfg_hooks.block_ends_with_call_p(bb);
}

static bool block_ends_with_condjump_p_cb(MtcsCfgState *self,const_basic_block bb)
{
   return gimple_cfg_hooks.block_ends_with_condjump_p(bb);
}

static int flow_call_edges_add_cb (MtcsCfgState *self,sbitmap map)
{
   return gimple_cfg_hooks.flow_call_edges_add(map);
}

static void execute_on_growing_pred_cb (MtcsCfgState *self,edge e)
{
   gimple_cfg_hooks.execute_on_growing_pred(e);
}

static void execute_on_shrinking_pred_cb (MtcsCfgState *self,edge e)
{
   gimple_cfg_hooks.execute_on_shrinking_pred(e);
}

static bool cfg_hook_duplicate_loop_body_to_header_edge_cb(MtcsCfgState *self,class loop *loop, edge e,
      unsigned int ndupl, sbitmap wont_exit, edge orig, vec<edge> *to_remove, int flags)
{
   return gimple_cfg_hooks.cfg_hook_duplicate_loop_body_to_header_edge(loop,e,ndupl,wont_exit,orig,to_remove,flags);
}

static void lv_add_condition_to_bb_cb (MtcsCfgState *self,basic_block first_head ,
                basic_block second_head ATTRIBUTE_UNUSED, basic_block cond_bb, void *comp_rtx)
{
   gimple_cfg_hooks.lv_add_condition_to_bb(first_head,second_head,cond_bb,comp_rtx);
}

static void lv_adjust_loop_header_phi_cb (MtcsCfgState *self,basic_block first, basic_block second,basic_block new_head, edge e)
{
   gimple_cfg_hooks.lv_adjust_loop_header_phi(first,second,new_head,e);
}

static void extract_cond_bb_edges_cb (MtcsCfgState *self,basic_block b, edge *branch_edge,edge *fallthru_edge)
{
   gimple_cfg_hooks.extract_cond_bb_edges(b,branch_edge,fallthru_edge);
}

static void flush_pending_stmts_cb (MtcsCfgState *self,edge e)
{
   gimple_cfg_hooks.flush_pending_stmts(e);
}

static bool empty_block_p_cb (MtcsCfgState *self,basic_block bb)
{
   return gimple_cfg_hooks.empty_block_p(bb);
}

static basic_block split_block_before_cond_jump_cb (MtcsCfgState *self,basic_block bb)
{
   return gimple_cfg_hooks.split_block_before_cond_jump(bb);
}

static void  account_profile_record_cb (MtcsCfgState *self,basic_block bb, struct profile_record *pr)
{
   gimple_cfg_hooks.account_profile_record(bb,pr);
}

MtcsCfgGimpleState *mtcs_cfg_gimple_state_new(MtcsMode *mtcsMode)
{
   MtcsCfgGimpleState *self = n_slice_alloc0 (sizeof(MtcsCfgGimpleState));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_cfg_state_init((MtcsCfgState *)self);
   mtcsCfgGimpleStateInit(self);
   return self;
}




