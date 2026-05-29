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

#ifndef __GCC_MTCS_DFPROBLEMS__
#define __GCC_MTCS_DFPROBLEMS__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcsmicro.h"
#include "df.h"

typedef struct _MtcsDfproblems MtcsDfproblems;
struct _MtcsDfproblems
{
    MtcsComponent parent;

    bitmap_head seen_in_block;
    bitmap_head seen_in_insn;
    /* Scratch var used by transfer functions.  This is used to implement
       an optimization to reduce the amount of space used to compute the
       combined lr and live analysis.  */
    bitmap_head df_live_scratch;
    /* Scratch var used by transfer functions.  This is used to do md analysis
       only for live registers.  */
    bitmap_head df_md_scratch;
};

MtcsDfproblems *mtcs_dfproblems_new(MtcsMode *mtcsMode);
//原型 df_chain_dump df.h df-problems.cc
void mtcs_dfproblems_df_chain_dump (MtcsDfproblems *self,struct df_link *link, FILE *file);
//原型 df_print_bb_index df.h df-problems.cc
void mtcs_dfproblems_df_print_bb_index (MtcsDfproblems *self,basic_block bb, FILE *file);
//原型 df_rd_simulate_one_insn df.h df-problems.cc
void mtcs_dfproblems_df_rd_simulate_one_insn (MtcsDfproblems *self,basic_block bb ATTRIBUTE_UNUSED, rtx_insn *insn,
          bitmap local_rd);
//原型 df_rd_add_problem df.h df-problems.cc
void mtcs_dfproblems_df_rd_add_problem (MtcsDfproblems *self);
//原型 df_lr_add_problem df.h df-problems.cc
void mtcs_dfproblems_df_lr_add_problem (MtcsDfproblems *self);
//原型 df_lr_verify_transfer_functions df.h df-problems.cc
void mtcs_dfproblems_df_lr_verify_transfer_functions (MtcsDfproblems *self);
//原型 df_live_add_problem df.h df-problems.cc
void mtcs_dfproblems_df_live_add_problem (MtcsDfproblems *self);
//原型 df_live_set_all_dirty df.h df-problems.cc
void mtcs_dfproblems_df_live_set_all_dirty (MtcsDfproblems *self);
//原型 df_live_verify_transfer_functions df.h df-problems.cc
void mtcs_dfproblems_df_live_verify_transfer_functions (MtcsDfproblems *self);
//原型 df_mir_add_problem df.h df-problems.cc
void mtcs_dfproblems_df_mir_add_problem (MtcsDfproblems *self);
//原型 df_mir_simulate_one_insn df.h df-problems.cc
void mtcs_dfproblems_df_mir_simulate_one_insn (MtcsDfproblems *self,basic_block bb ATTRIBUTE_UNUSED, rtx_insn *insn,
           bitmap kill, bitmap gen);
//原型 df_chain_create df.h df-problems.cc
struct df_link * mtcs_dfproblems_df_chain_create (MtcsDfproblems *self,df_ref src, df_ref dst);
//原型 df_chain_unlink df.h df-problems.cc
void mtcs_dfproblems_df_chain_unlink (MtcsDfproblems *self,df_ref ref);
//原型 df_chain_copy df.h df-problems.cc
void  mtcs_dfproblems_df_chain_copy (MtcsDfproblems *self,df_ref to_ref,struct df_link *from_ref);
//原型 df_chain_add_problem df.h df-problems.cc
void mtcs_dfproblems_df_chain_add_problem (MtcsDfproblems *self,unsigned int chain_flags);
//原型 df_word_lr_mark_ref df.h df-problems.cc
bool mtcs_dfproblems_df_word_lr_mark_ref (MtcsDfproblems *self,df_ref ref, bool is_set, regset live);
//原型 df_word_lr_add_problem df.h df-problems.cc
void mtcs_dfproblems_df_word_lr_add_problem (MtcsDfproblems *self);
//原型 df_word_lr_simulate_defs df.h df-problems.cc
bool mtcs_dfproblems_df_word_lr_simulate_defs (MtcsDfproblems *self,rtx_insn *insn, bitmap live);
//原型 df_word_lr_simulate_uses df.h df-problems.cc
void mtcs_dfproblems_df_word_lr_simulate_uses (MtcsDfproblems *self,rtx_insn *insn, bitmap live);
//原型 df_note_add_problem df.h df-problems.cc
void mtcs_dfproblems_df_note_add_problem (MtcsDfproblems *self);
//原型 df_simulate_find_defs df.h df-problems.cc
void mtcs_dfproblems_df_simulate_find_defs (MtcsDfproblems *self,rtx_insn *insn, bitmap defs);
//原型 df_simulate_defs df.h df-problems.cc
void mtcs_dfproblems_df_simulate_defs (MtcsDfproblems *self,rtx_insn *insn, bitmap live);
//原型 df_simulate_uses df.h df-problems.cc
void mtcs_dfproblems_df_simulate_uses (MtcsDfproblems *self,rtx_insn *insn, bitmap live);
//原型 df_simulate_initialize_backwards df.h df-problems.cc
void mtcs_dfproblems_df_simulate_initialize_backwards (MtcsDfproblems *self,basic_block bb, bitmap live);
//原型 df_simulate_one_insn_backwards df.h df-problems.cc
void mtcs_dfproblems_df_simulate_one_insn_backwards (MtcsDfproblems *self,basic_block bb, rtx_insn *insn, bitmap live);
//原型 df_simulate_finalize_backwards df.h df-problems.cc
void mtcs_dfproblems_df_simulate_finalize_backwards (MtcsDfproblems *self,basic_block bb, bitmap live);
//原型 df_simulate_initialize_forwards df.h df-problems.cc
void mtcs_dfproblems_df_simulate_initialize_forwards (MtcsDfproblems *self,basic_block bb, bitmap live);
//原型 df_simulate_one_insn_forwards df.h df-problems.cc
void mtcs_dfproblems_df_simulate_one_insn_forwards (MtcsDfproblems *self,basic_block bb, rtx_insn *insn, bitmap live);
//原型 simulate_backwards_to_point df.h df-problems.cc
void mtcs_dfproblems_simulate_backwards_to_point (MtcsDfproblems *self,basic_block bb, regset live, rtx point);
//原型 can_move_insns_across df.h df-problems.cc
bool mtcs_dfproblems_can_move_insns_across (MtcsDfproblems *self,rtx_insn *from, rtx_insn *to,
             rtx_insn *across_from, rtx_insn *across_to,
             basic_block merge_bb, regset merge_live,
             regset other_branch_live, rtx_insn **pmove_upto);
//原型 df_md_simulate_one_insn df.h df-problems.cc
void mtcs_dfproblems_df_md_simulate_one_insn (MtcsDfproblems *self,basic_block bb ATTRIBUTE_UNUSED,
      rtx_insn *insn,bitmap local_md);
//原型 df_md_simulate_artificial_defs_at_top df.h df-problems.cc
void df_md_simulate_artificial_defs_at_top (MtcsDfproblems *self,basic_block bb, bitmap local_md);
//原型 df_md_add_problem df.h df-problems.cc
void mtcs_dfproblems_df_md_add_problem (MtcsDfproblems *self);

#endif

