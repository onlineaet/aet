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

#ifndef __GCC_MTCS_CFG__
#define __GCC_MTCS_CFG__

#include "../nlib.h"
#include "mtcscomponent.h"

typedef struct _MtcsCfg MtcsCfg;
struct _MtcsCfg
{
    MtcsComponent parent;
    //typedef hash_map<int_hash<int, -1, -2>, int> copy_map_t;
    hash_map<int_hash<int, -1, -2>, int>  *bb_original;
    hash_map<int_hash<int, -1, -2>, int>  *bb_copy;

    /* And between loops and copies.  */
    hash_map<int_hash<int, -1, -2>, int>  *loop_copy;
 };

MtcsCfg *mtcs_cfg_new(MtcsMode *mtcsMode);
//原型 redirect_edge_succ cfg.h cfg.cc
void mtcs_cfg_redirect_edge_succ (MtcsCfg *self,edge e, basic_block new_succ);
//原型 make_edge cfg.h cfg.cc
edge mtcs_cfg_make_edge (MtcsCfg *self,basic_block src, basic_block dest, int flags);
//原型 make_single_succ_edge cfg.h cfg.cc
edge mtcs_cfg_make_single_succ_edge (MtcsCfg *self,basic_block src, basic_block dest, int flags);
//原型 unchecked_make_edge cfg.h cfg.cc
edge mtcs_cfg_unchecked_make_edge (MtcsCfg *self,basic_block src, basic_block dst, int flags);
//原型 remove_edge_raw cfg.h cfg.cc
void mtcs_cfg_remove_edge_raw (MtcsCfg *self,edge e);
//原型 compact_blocks cfg.h cfg.cc
void mtcs_cfg_compact_blocks (MtcsCfg *self);
//原型 cached_make_edge cfg.h cfg.cc
edge mtcs_cfg_cached_make_edge (MtcsCfg *self,sbitmap edge_cache, basic_block src, basic_block dst, int flags);
//原型 initialize_original_copy_tables cfg.h cfg.cc
void mtcs_cfg_initialize_original_copy_tables (MtcsCfg *self);
//原型 reset_original_copy_tables cfg.h cfg.cc
void mtcs_cfg_reset_original_copy_tables (MtcsCfg *self);
//原型 free_original_copy_tables cfg.h cfg.cc
void mtcs_cfg_free_original_copy_tables (MtcsCfg *self);
//原型 original_copy_tables_initialized_p cfg.h cfg.cc
bool mtcs_cfg_original_copy_tables_initialized_p (MtcsCfg *self);
//原型 set_bb_original cfg.h cfg.cc
void mtcs_cfg_set_bb_original (MtcsCfg *self,basic_block bb, basic_block original);
//原型 get_bb_original cfg.h cfg.cc
basic_block mtcs_cfg_get_bb_original (MtcsCfg *self,basic_block bb);
//原型 set_bb_copy cfg.h cfg.cc
void mtcs_cfg_set_bb_copy (MtcsCfg *self,basic_block bb, basic_block copy);
//原型 get_bb_copy cfg.h cfg.cc
basic_block mtcs_cfg_get_bb_copy (MtcsCfg *self,basic_block bb);
//原型 set_loop_copy cfg.h cfg.cc
void mtcs_cfg_set_loop_copy (MtcsCfg *self,class loop *loop, class loop *copy);
//原型 get_loop_copy cfg.h cfg.cc
class loop *mtcs_cfg_get_loop_copy (MtcsCfg *self,class loop *loop);
//原型 scale_strictly_dominated_blocks cfg.h cfg.cc
void mtcs_cfg_scale_strictly_dominated_blocks (MtcsCfg *self,basic_block bb,
             profile_count num, profile_count den);

#endif

