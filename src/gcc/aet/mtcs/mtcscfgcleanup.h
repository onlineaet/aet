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

#ifndef __GCC_MTCS_CFG_CLEANUP__
#define __GCC_MTCS_CFG_CLEANUP__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcspass.h"
#include "cfgcleanup.h"

typedef struct _MtcsCfgCleanup MtcsCfgCleanup;
struct _MtcsCfgCleanup
{
     MtcsComponent parent;

     /* Set to true when we are running first pass of try_optimize_cfg loop.  */
      bool first_pass;

     /* Set to true if crossjumps occurred in the latest run of try_optimize_cfg.  */
      bool crossjumps_occurred;

     /* Set to true if we couldn't run an optimization due to stale liveness
        information; we should run df_analyze to enable more opportunities.  */
      bool block_was_dirty;
};

MtcsCfgCleanup *mtcs_cfg_cleanup_new(MtcsMode *mtcsMode);
//原型 cleanup_cfg cfgcleanup.h cfgcleanup.cc
bool mtcs_cfg_cleanup_cleanup_cfg (MtcsCfgCleanup *self,int mode);
//原型 delete_unreachable_blocks cfgcleanup.h cfgcleanup.cc
bool mtcs_cfg_cleanup_delete_unreachable_blocks (MtcsCfgCleanup *self);
//原型 bb_is_just_return cfgcleanup.h cfgcleanup.cc
bool mtcs_cfg_cleanup_bb_is_just_return (MtcsCfgCleanup *self,basic_block bb, rtx_insn **ret, rtx_insn **use);
//原型 delete_dead_jumptables cfgcleanup.h cfgcleanup.cc
void mtcs_cfg_cleanup_delete_dead_jumptables (MtcsCfgCleanup *self);
//原型 flow_find_head_matching_sequence cfgcleanup.h cfgcleanup.cc
int mtcs_cfg_cleanup_flow_find_head_matching_sequence (MtcsCfgCleanup *self,basic_block bb1,
      basic_block bb2, rtx_insn **f1,rtx_insn **f2, int stop_after);
//原型 flow_find_cross_jump cfgcleanup.h cfgcleanup.cc
int mtcs_cfg_cleanup_flow_find_cross_jump (MtcsCfgCleanup *self,basic_block bb1, basic_block bb2, rtx_insn **f1,
            rtx_insn **f2, enum replace_direction *dir_p);


//原型 NEXT_PASS (pass_jump, 1);    RTL_PASS   cfgcleanup.cc   jump   y  无条件执行 ...cleanup_cfg....
typedef struct _MtcsPassJump MtcsPassJump;
struct _MtcsPassJump
{
   MtcsPass parent;
};
MtcsPassJump *mtcs_pass_jump_new(MtcsMode *mtcsMode);


//原型 NEXT_PASS (pass_jump_after_combine, 1); RTL_PASS cfgcleanup.cc jump_after_combine y 有条件执行
//flag_thread_jumps && flag_expensive_optimizations; cleanup_cfg
typedef struct _MtcsPassJumpAfterCombine MtcsPassJumpAfterCombine;
struct _MtcsPassJumpAfterCombine
{
   MtcsPass parent;
};
MtcsPassJumpAfterCombine *mtcs_pass_jump_after_combine(MtcsMode *mtcsMode);


#endif

