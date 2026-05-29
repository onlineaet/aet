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


#ifndef __GCC_MTCS_CFG_RTL__
#define __GCC_MTCS_CFG_RTL__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcspass.h"

typedef struct _MtcsCfgRtl MtcsCfgRtl;
struct _MtcsCfgRtl
{
     MtcsComponent parent;
    /* Holds the interesting leading and trailing notes for the function.
       Only applicable if the CFG is in cfglayout mode.  */
     GTY(()) rtx_insn *cfg_layout_function_footer;
     GTY(()) rtx_insn *cfg_layout_function_header;
};

MtcsCfgRtl *mtcs_cfg_rtl_new(MtcsMode *mtcsMode);
//原型 delete_insn  cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_delete_insn (MtcsCfgRtl *self,rtx_insn *insn);
//原型 delete_insn_and_edges cfgrtl.h cfgrtl.cc
bool mtcs_cfg_rtl_delete_insn_and_edges (MtcsCfgRtl *self,rtx_insn *insn);
//原型 delete_insn_chain cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_delete_insn_chain (MtcsCfgRtl *self,rtx start, rtx_insn *finish, bool clear_bb);
//原型 create_basic_block_structure cfgrtl.h cfgrtl.cc
basic_block mtcs_cfg_rtl_create_basic_block_structure (MtcsCfgRtl *self,rtx_insn *head, rtx_insn *end,
        rtx_note *bb_note,basic_block after);
//原型 compute_bb_for_insn cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_compute_bb_for_insn (MtcsCfgRtl *self);
//原型 free_bb_for_insn cfgrl.h cfgrtl.cc
void mtcs_cfg_rtl_free_bb_for_insn (MtcsCfgRtl *self);
//原型 entry_of_function cfgrtl.h cfgrtl.cc
rtx_insn *mtcs_cfg_rtl_entry_of_function (MtcsCfgRtl *self);
//原型 update_bb_for_insn cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_update_bb_for_insn (MtcsCfgRtl *self,basic_block bb);
//原型 contains_no_active_insn_p cfgrtl.h cfgrtl.cc
bool mtcs_cfg_rtl_contains_no_active_insn_p (MtcsCfgRtl *self, const_basic_block bb);
//原型 forwarder_block_p cfgrtl.h cfgrtl.cc
bool mtcs_cfg_rtl_forwarder_block_p (MtcsCfgRtl *self,const_basic_block bb);
//原型 can_fallthru cfgrtl.h cfgrtl.cc
bool mtcs_cfg_rgl_can_fallthru (MtcsCfgRtl *self,basic_block src, basic_block target);
//原型 bb_note cfgrtl.h cfgrtl.cc
rtx_note * mtcs_cfg_rtl_bb_note (MtcsCfgRtl *self,basic_block bb);
//原型 try_redirect_by_replacing_jump cfgrtl.h cfgrtl.cc
edge mtcs_cfg_rtl_try_redirect_by_replacing_jump (MtcsCfgRtl *self,edge e, basic_block target, bool in_cfglayout);
//原型 emit_barrier_after_bb cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_emit_barrier_after_bb (MtcsCfgRtl *self,basic_block bb);
//原型 force_nonfallthru_and_redirect cfgrtl.h cfgrtl.cc
basic_block mcs_cfg_rtl_force_nonfallthru_and_redirect (MtcsCfgRtl *self,edge e, basic_block target, rtx jump_label);
//原型 purge_dead_edges cfgrtl.h cfgrtl.cc
bool mtcs_cfg_rtl_purge_dead_edges (MtcsCfgRtl *self,basic_block bb);
//原型 insert_insn_on_edge cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_insert_insn_on_edge (MtcsCfgRtl *self,rtx pattern, edge e);
//原型 prepend_insn_to_edge cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_prepend_insn_to_edge (MtcsCfgRtl *self,rtx pattern, edge e);
//原型 commit_edge_insertions cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_commit_edge_insertions (MtcsCfgRtl *self);
//原型 print_rtl_with_bb cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_print_rtl_with_bb (MtcsCfgRtl *self,FILE *outf, const rtx_insn *rtx_first, dump_flags_t flags);
//原型 update_br_prob_note cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_update_br_prob_note (MtcsCfgRtl *self,basic_block bb);
//原型 update_br_prob_note cfgrtl.h cfgrtl.cc
rtx_insn * mtcs_cfg_rtl_get_last_bb_insn (MtcsCfgRtl *self,basic_block bb);
//原型 find_bbs_reachable_by_hot_paths cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_find_bbs_reachable_by_hot_paths (MtcsCfgRtl *self,hash_set<basic_block> *set);
//原型 fixup_partitions cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_fixup_partitions (MtcsCfgRtl *self);
//原型 purge_all_dead_edges cfgrtl.h cfgrtl.cc
bool mtcs_cfg_rtl_purge_all_dead_edges (MtcsCfgRtl *self);
//原型 fixup_abnormal_edges cfgrtl.h cfgrtl.cc
bool mtcs_cfg_rtl_fixup_abnormal_edges (MtcsCfgRtl *self);
//原型 update_cfg_for_uncondjump cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_update_cfg_for_uncondjump (MtcsCfgRtl *self,rtx_insn *insn);
//原型 unlink_insn_chain cfgrtl.h cfgrtl.cc
rtx_insn * mtcs_cfg_rtl_unlink_insn_chain (MtcsCfgRtl *self,rtx_insn *first, rtx_insn *last);
//原型 relink_block_chain cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_relink_block_chain (MtcsCfgRtl *self,bool stay_in_cfglayout_mode);
//原型 duplicate_insn_chain cfgrtl.h cfgrtl.cc
rtx_insn * mtcs_cfg_rtl_duplicate_insn_chain (MtcsCfgRtl *self,rtx_insn *from, rtx_insn *to,
              class loop *loop, class copy_bb_data *id);
//原型 cfg_layout_initialize cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_cfg_layout_initialize (MtcsCfgRtl *self,int flags);
//原型 break_superblocks cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_break_superblocks (MtcsCfgRtl *self);
//原型 cfg_layout_finalize cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_cfg_layout_finalize (MtcsCfgRtl *self);
//原型 init_rtl_bb_info cfgrtl.h cfgrtl.cc
void mtcs_cfg_rtl_init_rtl_bb_info (MtcsCfgRtl *self,basic_block bb);
//原型 emit_insn_at_entry rtl.h cfgrtl.cc
void mtcs_cfg_rtl_emit_insn_at_entry (MtcsCfgRtl *self,rtx insn);
//原型 block_label cfgrtl.h cfgrtl.cc
rtx_code_label *mtcs_cfg_rtl_block_label (MtcsCfgRtl *self,basic_block block);
//原型 rtl_merge_blocks cfgrtl.cc
void mtcs_cfg_rtl_merge_blocks (MtcsCfgRtl *self,basic_block a, basic_block b);
//原型 remove_barriers_from_footer cfgrtl.cc
void mtcs_cfg_rtl_remove_barriers_from_footer (MtcsCfgRtl *self,basic_block bb);
//实现 cfglayout的接口 void (*merge_blocks) (MtcsCfgState *self,basic_block a, basic_block b);
void mtcs_cfg_rtl_layout_merge_blocks (MtcsCfgRtl *self, basic_block a, basic_block b);
//原型 insert_section_boundary_note bb-reorder.h bb-reorder.cc
void mtcs_cfg_rtl_insert_section_boundary_note (MtcsCfgRtl *self);

//原型 NEXT_PASS (pass_into_cfg_layout_mode, 1);    RTL_PASS   cfgrtl.cc   into_cfglayout   y  无条件执行 cfg_layout_initialize (0)
typedef struct _MtcsPassIntoCfgLayout MtcsPassIntoCfgLayout;
struct _MtcsPassIntoCfgLayout
{
   MtcsPass parent;
};
MtcsPassIntoCfgLayout *mtcs_pass_into_cfg_layout_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_outof_cfg_layout_mode, 1);  RTL_PASS cfgrtl.cc outof_cfglayout   y  无条件执行 cfg_layout_finalize
typedef struct _MtcsPassOutofCfgLayout MtcsPassOutofCfgLayout;
struct _MtcsPassOutofCfgLayout
{
   MtcsPass parent;
};
MtcsPassOutofCfgLayout *mtcs_pass_outof_cfg_layout_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_free_cfg, 1);  RTL_PASS cfgrtl.cc *free_cfg   y  无条件执行 df_note_add_problem..df_analyze
typedef struct _MtcsPassFreeCfg MtcsPassFreeCfg;
struct _MtcsPassFreeCfg
{
   MtcsPass parent;
};
MtcsPassFreeCfg *mtcs_pass_free_cfg_new(MtcsMode *mtcsMode);

#endif

