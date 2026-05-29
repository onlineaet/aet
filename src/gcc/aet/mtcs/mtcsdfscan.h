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

#ifndef __GCC_MTCS_DFSCAN__
#define __GCC_MTCS_DFSCAN__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcsmicro.h"

typedef struct _MtcsDfscan MtcsDfscan;
struct _MtcsDfscan
{
    MtcsComponent parent;
    //原型 initialized df-scan.cc
    bool initialized;
    //原型 elim_reg_set df-scan.cc
    HardRegSet elim_reg_set;
    //原型 regs_ever_live df-scan.cc
    bool regs_ever_live[MAX_FIRST_PSEUDO_REGISTER/*!FIRST_PSEUDO_REGISTER 足够大*/];
};

MtcsDfscan *mtcs_dfscan_new(MtcsMode *mtcsMode);
//原型 df_epilogue_uses_p df.h df-scan.cc
bool mtcs_dfscan_df_epilogue_uses_p ( MtcsDfscan *self,unsigned int regno);
//原型 df_scan_alloc df.h df-scan.cc
void mtcs_dfscan_df_scan_alloc (MtcsDfscan *self,bitmap all_blocks ATTRIBUTE_UNUSED);
//原型 df_grow_reg_info df.h df-scan.cc
void mtcs_dfsan_df_grow_reg_info (MtcsDfscan *self);
//原型 df_grow_insn_info df.h df-scan.cc
void mtcs_dfscan_df_grow_insn_info (MtcsDfscan *self);
//原型 df_hard_reg_init df.h df-scan.cc
void mtcs_dfscan_df_hard_reg_init (MtcsDfscan *self);
//原型 df_compute_regs_ever_live df.h df-scan.cc
void mtcs_dfscan_df_compute_regs_ever_live (MtcsDfscan *self,bool reset);
//原型 df_update_entry_exit_and_calls df.h df-scan.cc
void mtcs_dfscan_df_update_entry_exit_and_calls (MtcsDfscan *self);
//原型 df_regs_ever_live_p df.h df-scan.cc
bool mtcs_dfscan_df_regs_ever_live_p (MtcsDfscan *self,unsigned int regno);
//原型 df_scan_blocks df.h df-scan.cc
void mtcs_dfscan_df_scan_blocks (MtcsDfscan *self);
//原型 df_set_regs_ever_live df.h df-scan.cc
void mtcs_dfscan_df_set_regs_ever_live (MtcsDfscan *self,unsigned int regno, bool value);
//原型 df_scan_verify df.h df-scan.cc
void mtcs_dfscan_df_scan_verify (MtcsDfscan *self);
//原型 df_insn_rescan df.h df-scan.cc
bool mtcs_dfscan_df_insn_rescan (MtcsDfscan *self,rtx_insn *insn);
//原型 df_uses_create df.h df-scan.cc
void mtcs_dfscan_df_uses_create (MtcsDfscan *self,rtx *loc, rtx_insn *insn, int ref_flags);
//原型 df_insn_delete df.h df-scan.cc
void mtcs_dfscan_df_insn_delete (MtcsDfscan *self,rtx_insn *insn);
//原型 df_insn_rescan_debug_internal df.h df-scan.cc
bool mtcs_dfscan_df_insn_rescan_debug_internal (MtcsDfscan *self,rtx_insn *insn);
//原型 df_insn_rescan_all df.h df-scan.cc
void mtcs_dfscan_df_insn_rescan_all (MtcsDfscan *self);
//原型 df_process_deferred_rescans df.h df-scan.cc
void mtcs_dfscan_df_process_deferred_rescans (MtcsDfscan *self);
//原型 df_maybe_reorganize_use_refs df.h df-scan.cc
void mtcs_dfscan_df_maybe_reorganize_use_refs (MtcsDfscan *self,int dforder/*!enum df_ref_order order*/);
//原型 df_maybe_reorganize_def_refs df.h df-scan.cc
void mtcs_dfscan_df_maybe_reorganize_def_refs (MtcsDfscan *self,int dforder/*!enum df_ref_order order*/);
//原型 df_insn_change_bb df.h df-scan.cc
void mtcs_dfscan_df_insn_change_bb (MtcsDfscan *self,rtx_insn *insn, basic_block new_bb);
//原型 df_ref_change_reg_with_loc df.h df-scan.cc
void mtcs_dfscan_df_ref_change_reg_with_loc (MtcsDfscan *self,rtx loc, unsigned int new_regno);
//原型 df_notes_rescan df.h df-scan.cc
void mtcs_dfscan_df_notes_rescan (MtcsDfscan *self,rtx_insn *insn);
//原型 df_recompute_luids df.h df-scan.cc
void mtcs_dfscan_df_recompute_luids (MtcsDfscan *self,basic_block bb);
//原型 df_bb_refs_record df.h df-scan.cc
void mtcs_dfscan_df_bb_refs_record (MtcsDfscan *self,int bb_index, bool scan_insns);
//原型 df_update_entry_block_defs df.h df-scan.cc
void mtcs_dfscan_df_update_entry_block_defs (MtcsDfscan *self);
//原型 df_update_exit_block_uses df.h df-scan.cc
void mtcs_dfscan_df_update_exit_block_uses (MtcsDfscan *self);
//原型 df_scan_add_problem df.h df-scan.cc
void mtcs_dfscan_df_scan_add_problem (MtcsDfscan *self);
//原型 df_get_exit_block_use_set df.h df-scan.cc
void mtcs_dfscan_df_get_exit_block_use_set (MtcsDfscan *self,bitmap exit_block_uses);

#endif

