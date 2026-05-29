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


#ifndef __GCC_MTCS_CFG_LOOP_MANIP__
#define __GCC_MTCS_CFG_LOOP_MANIP__

#include "../nlib.h"
#include "mtcscomponent.h"



typedef struct _MtcsCfgLoopManip MtcsCfgLoopManip;
struct _MtcsCfgLoopManip
{
   MtcsComponent parent;
};



MtcsCfgLoopManip *mtcs_cfg_loop_manip_new(MtcsMode *mtcsMode);
//原型 remove_path cfgloopmanip.h cfgloopmanip.cc
bool mtcs_cfg_loop_manip_remove_path (MtcsCfgLoopManip *self,edge e, bool *irred_invalidated=NULL,
      bitmap loop_closed_ssa_invalidated=NULL);
//原型 place_new_loop cfgloopmanip.h cfgloopmanip.cc
void mtcs_cfg_loop_manip_place_new_loop (MtcsCfgLoopManip *self,struct function *fn, class loop *loop);
//原型 add_loop cfgloopmanip.h cfgloopmanip.cc
void mtcs_cfg_loop_manip_add_loop (MtcsCfgLoopManip *self,class loop *loop, class loop *outer);
//原型 scale_loop_frequencies cfgloopmanip.h cfgloopmanip.cc
void mtcs_cfg_loop_manip_scale_loop_frequencies (MtcsCfgLoopManip *self,class loop *loop, profile_probability p);
//原型 scale_dominated_blocks_in_loop cfgloopmanip.h cfgloopmanip.cc
void mtcs_cfg_loop_manip_scale_dominated_blocks_in_loop (MtcsCfgLoopManip *self,class loop *loop, basic_block bb,
            profile_count num, profile_count den);
//原型 update_loop_exit_probability_scale_dom_bbs cfgloopmanip.h cfgloopmanip.cc
edge mtcs_cfg_loop_manip_update_loop_exit_probability_scale_dom_bbs (MtcsCfgLoopManip *self,class loop *loop,
                   edge exit_edge, profile_count desired_count);
//原型 scale_loop_profile cfgloopmanip.h cfgloopmanip.cc
void mtcs_cfg_loop_manip_scale_loop_profile (MtcsCfgLoopManip *self,class loop *loop, profile_probability p,
          gcov_type iteration_bound);
//原型 create_empty_if_region_on_edge cfgloopmanip.h cfgloopmanip.cc
edge mtcs_cfg_loop_manip_create_empty_if_region_on_edge (MtcsCfgLoopManip *self,edge entry_edge, tree condition);
//原型 create_empty_loop_on_edge cfgloopmanip.h cfgloopmanip.cc
class loop *mtcs_cfg_loop_manip_create_empty_loop_on_edge (MtcsCfgLoopManip *self,
            edge entry_edge,
            tree initial_value,
            tree stride, tree upper_bound,
            tree iv,
            tree *iv_before,
            tree *iv_after,
            class loop *outer);
//原型 unloop cfgloopmanip.h cfgloopmanip.cc
void mtcs_cfg_loop_manip_unloop (MtcsCfgLoopManip *self,class loop *loop, bool *irred_invalidated,
   bitmap loop_closed_ssa_invalidated);
//原型 copy_loop_info cfgloopmanip.h cfgloopmanip.cc
void mtcs_cfg_loop_manip_copy_loop_info (MtcsCfgLoopManip *self,class loop *loop, class loop *target);
//原型 duplicate_loop cfgloopmanip.h cfgloopmanip.cc
class loop * mtcs_cfg_loop_manip_duplicate_loop (MtcsCfgLoopManip *self,class loop *loop, class loop *target, class loop *after);
//原型 duplicate_subloops cfgloopmanip.h cfgloopmanip.cc
void mtcs_cfg_loop_manip_duplicate_subloops (MtcsCfgLoopManip *self,class loop *loop, class loop *target);
//原型 can_duplicate_loop_p cfgloopmanip.h cfgloopmanip.cc
bool mtcs_cfg_loop_manip_can_duplicate_loop_p (MtcsCfgLoopManip *self,const class loop *loop);
//原型 duplicate_loop_body_to_header_edge cfgloopmanip.h cfgloopmanip.cc
bool mtcs_cfg_loop_manip_duplicate_loop_body_to_header_edge (MtcsCfgLoopManip *self,
               class loop *loop, edge e,
                unsigned int ndupl, sbitmap wont_exit,
                edge orig, vec<edge> *to_remove, int flags);
//原型 create_preheader cfgloopmanip.h cfgloopmanip.cc
basic_block mtcs_cfg_loop_mainip_create_preheader (MtcsCfgLoopManip *self,class loop *loop, int flags);
//原型 create_preheader cfgloopmanip.h cfgloopmanip.cc
void mtcs_cfg_loop_mainip_create_preheader (MtcsCfgLoopManip *self,int flags);
//原型 force_single_succ_latches cfgloopmanip.h cfgloopmanip.cc
void mtcs_cfg_loop_mainip_force_single_succ_latches (MtcsCfgLoopManip *self);
//原型 loop_version cfgloopmanip.h cfgloopmanip.cc
class loop *mtcs_cfg_loop_mainip_loop_version (MtcsCfgLoopManip *self,class loop *loop,
         void *cond_expr, basic_block *condition_bb,
         profile_probability then_prob, profile_probability else_prob,
         profile_probability then_scale, profile_probability else_scale,
         bool place_after);


#endif
