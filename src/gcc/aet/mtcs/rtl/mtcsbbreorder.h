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


#ifndef __GCC_MTCS_BB_REORDER__
#define __GCC_MTCS_BB_REORDER__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "../mtcspass.h"
#include "regset.h"

struct bbro_basic_block_data;

typedef struct _MtcsBBReorder MtcsBBReorder;
struct _MtcsBBReorder
{
    MtcsComponent parent;
    int x_uncond_jump_length;

    /* The current size of the following dynamic array.  */
    int array_size;

    /* The array which holds needed information for basic blocks.  */
    bbro_basic_block_data *bbd;
    /* Maximum count of one of the entry blocks.  */
    profile_count max_entry_count;
 };

MtcsBBReorder *mtcs_bb_reorder_new(MtcsMode *mtcsMode);
//原型 get_uncond_jump_length bb-reorder.h bb-reorder.cc shrink-wrap.cc引用
int mtcs_bb_reorder_get_uncond_jump_length (MtcsBBReorder *self);



//原型 NEXT_PASS (pass_reorder_blocks, 1); RTL_PASS bb-reorder.cc bbro y 有条件执行 targetm.cannot_modify_jumps_p () cfg_layout_initialize...
typedef struct _MtcsPassBBReorder  MtcsPassBBReorder;
struct _MtcsPassBBReorder
{
   MtcsPass parent;
};
MtcsPassBBReorder *mtcs_pass_bb_reorder_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_duplicate_computed_gotos, 1); RTL_PASS bb-reorder.cc compgotos y 有条件执行 targetm.cannot_modify_jumps_p duplicate_computed_gotos
typedef struct _MtcsPassCompGotos  MtcsPassCompGotos;
struct _MtcsPassCompGotos
{
   MtcsPass parent;
};
MtcsPassCompGotos *mtcs_pass_comp_gotos_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_partition_blocks, 1); RTL_PASS bb-reorder.cc bbpart y 有条件执行 flag_reorder_blocks_and_partition
typedef struct _MtcsPassBBPart  MtcsPassBBPart;
struct _MtcsPassBBPart
{
   MtcsPass parent;
};
MtcsPassBBPart *mtcs_pass_bb_part_new(MtcsMode *mtcsMode);

#endif

