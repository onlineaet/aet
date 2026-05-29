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

#ifndef __GCC_MTCS_CFG_BUILD__
#define __GCC_MTCS_CFG_BUILD__

#include "../nlib.h"
#include "mtcscomponent.h"

typedef struct _MtcsCfgBuild MtcsCfgBuild;
struct _MtcsCfgBuild
{
   MtcsComponent parent;
};

MtcsCfgBuild *mtcs_cfg_build_new(MtcsMode *mtcsMode);
//原型 find_many_sub_basic_blocks cfgbuild.h  cfgbuild.cc
void mtcs_cfg_build_find_many_sub_basic_blocks (MtcsCfgBuild *self,sbitmap blocks);
//原型 rtl_make_eh_edge cfgbuild.h cfgbuild.cc
void mtcs_cfg_build_rtl_make_eh_edge (MtcsCfgBuild *self,sbitmap edge_cache, basic_block src, rtx insn);
//原型 find_sub_basic_blocks cfgbuild.h cfgbuild.cc
void mtcs_cfg_build_find_sub_basic_blocks (MtcsCfgBuild *self,basic_block bb);
//原型 control_flow_insn_p cfgbuild.h cfgbuild.cc
bool mtcs_cfg_build_control_flow_insn_p (MtcsCfgBuild *self,const rtx_insn *insn);

#endif

