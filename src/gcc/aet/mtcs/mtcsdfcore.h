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
 * base on dfa.cc
 */

#ifndef __GCC_MTCS_DFCORE__
#define __GCC_MTCS_DFCORE__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcspass.h"
#include "df.h"

typedef struct _MtcsDfcore MtcsDfcore;
struct _MtcsDfcore
{
    MtcsComponent parent;
    /* An obstack for bitmap not related to specific dataflow problems.
       This obstack should e.g. be used for bitmaps with a short life time
       such as temporary bitmaps.  */
    //原型 df_bitmap_obstack df.h df-core.cc
    bitmap_obstack df_bitmap_obstack;
    //原型 static struct df_problem user_problem; df-core.cc
    struct df_problem user_problem;
    //原型 static dataflow user_dflow; df-core.cc
    struct dataflow user_dflow;

};

MtcsDfcore *mtcs_dfcore_new(MtcsMode *mtcsMode);
//原型 df_add_problem df.h df-core.cc
void mtcs_dfcore_df_add_problem (MtcsDfcore *self,const struct df_problem *problem);
//原型 df_set_flags df.h df-core.cc
int mtcs_dfcore_df_set_flags (MtcsDfcore *self,int changeable_flags);
//原型 df_clear_flags df.h df-core.cc
int mtcs_dfcore_df_clear_flags (MtcsDfcore *self,int changeable_flags);
//原型 df_set_blocks df.h df-core.cc
void mtcs_dfcore_df_set_blocks (MtcsDfcore *self,bitmap blocks);
//原型 df_remove_problem df.h df-core.cc
void mtcs_dfcore_df_remove_problem (MtcsDfcore *self,struct dataflow *dflow);
//原型 df_finish_pass df.h df-core.cc
void mtcs_dfcore_df_finish_pass (MtcsDfcore *self,bool verify ATTRIBUTE_UNUSED);
//原型 df_worklist_dataflow df.h df-core.cc
void mtcs_dfcore_df_worklist_dataflow (MtcsDfcore *self,struct dataflow *dataflow,
                      bitmap blocks_to_consider,
                      int *blocks_in_postorder,
                      int n_blocks);
//原型 df_analyze_problem df.h df-core.cc
void mtcs_dfcore_df_analyze_problem (MtcsDfcore *self,struct dataflow *dflow,
          bitmap blocks_to_consider,int *postorder, int n_blocks);
//原型 df_analyze df.h df-core.cc
void mtcs_dfcore_df_analyze (MtcsDfcore *self);
//原型 df_analyze_loop df.h df-core.cc
void mtcs_dfcore_df_analyze_loop (MtcsDfcore *self,class loop *loop);
//原型 df_get_n_blocks df.h df-core.cc
int mtcs_dfcore_df_get_n_blocks (MtcsDfcore *self,enum df_flow_dir dir);
//原型 df_get_postorder df.h df-core.cc
int *mtcs_dfcore_df_get_postorder (MtcsDfcore *self,enum df_flow_dir dir);
//原型 df_simple_dataflow df.h df-core.cc
void mtcs_dfcore_df_simple_dataflow (MtcsDfcore *self,enum df_flow_dir dir,
          df_init_function init_fun,
          df_confluence_function_0 con_fun_0,
          df_confluence_function_n con_fun_n,
          df_transfer_function trans_fun,
          bitmap blocks, int * postorder, int n_blocks);
//原型 df_mark_solutions_dirty df.h df-core.cc
void mtcs_dfcore_df_mark_solutions_dirty (MtcsDfcore *self);
//原型 df_get_bb_dirty df.h df-core.cc
bool mtcs_dfcore_df_get_bb_dirty (MtcsDfcore *self,basic_block bb);
//原型 df_set_bb_dirty df.h df-core.cc
void mtcs_dfcore_df_set_bb_dirty (MtcsDfcore *self,basic_block bb);
//原型 df_grow_bb_info df.h df-core.cc
void mtcs_dfcore_df_grow_bb_info (MtcsDfcore *self,struct dataflow *dflow);
//原型 df_compact_blocks df.h df-core.cc
void mtcs_dfcore_df_compact_blocks (MtcsDfcore *self);
//原型 df_bb_replace df.h df-core.cc
void mtcs_dfcore_df_bb_replace (MtcsDfcore *self,int old_index, basic_block new_block);
//原型 df_bb_delete df.h df-core.cc
void mtcs_dfcore_df_bb_delete (MtcsDfcore *self,int bb_index);
//原型 df_verify df.h df-core.cc
void mtcs_dfcore_df_verify (MtcsDfcore *self);
//原型 df_check_cfg_clean df.h df-core.cc 依赖 #ifdef DF_DEBUG_CFG
void mtcs_dfcore_df_check_cfg_clean (MtcsDfcore *self);
//原型 df_bb_regno_first_def_find df.h df-core.cc
df_ref mtcs_dfcore_df_bb_regno_first_def_find (MtcsDfcore *self,basic_block bb, unsigned int regno);
//原型 df_bb_regno_last_def_find df.h df-core.cc
df_ref mtcs_dfcore_df_bb_regno_last_def_find (MtcsDfcore *self,basic_block bb, unsigned int regno);
//原型 df_find_def df.h df-core.cc
df_ref mtcs_dfcore_df_find_def (MtcsDfcore *self,rtx_insn *insn, rtx reg);
//原型 df_reg_defined df.h df-core.cc
bool mtcs_dfcore_df_reg_defined (MtcsDfcore *self,rtx_insn *insn, rtx reg);
//原型 df_find_use df.h df-core.cc
df_ref mtcs_dfcore_df_find_use (MtcsDfcore *self,rtx_insn *insn, rtx reg);
//原型 df_reg_used df.h df-core.cc
bool mtcs_dfcore_df_reg_used (MtcsDfcore *self,rtx_insn *insn, rtx reg);
//原型 df_find_single_def_src df.h df-core.cc
rtx mtcs_dfcore_df_find_single_def_src (MtcsDfcore *self,rtx reg);
//原型 dump_regset df.h df-core.cc
void mtcs_dfcore_dump_regset (MtcsDfcore *self,regset r, FILE *outf);
//原型 debug_regset df.h df-core.cc
void mtcs_dfcore_debug_regset (MtcsDfcore *self,regset r);
//原型 df_print_regset df.h df-core.cc
void mtcs_dfcore_df_print_regset (MtcsDfcore *self,FILE *file, const_bitmap r);
//原型 df_print_word_regset df.h df-core.cc
void mtcs_dfcore_df_print_word_regset (MtcsDfcore *self,FILE *file, const_bitmap r);
//原型 df_dump df.h df-core.cc
void mtcs_dfcore_df_dump (MtcsDfcore *self,FILE *file);
//原型 df_dump_region df.h df-core.cc
void mtcs_dfcore_df_dump_region (MtcsDfcore *self,FILE *file);
//原型 df_dump_start df.h df-core.cc
void mtcs_dfcore_df_dump_start (MtcsDfcore *self,FILE *file);
//原型 df_dump_top df.h df-core.cc
void mtcs_dfcore_df_dump_top (MtcsDfcore *self,basic_block bb, FILE *file);
//原型 df_dump_bottom df.h df-core.cc
void mtcs_dfcore_df_dump_bottom (MtcsDfcore *self,basic_block bb, FILE *file);
//原型 df_dump_insn_top df.h df-core.cc
void mtcs_dfcore_df_dump_insn_top (MtcsDfcore *self,const rtx_insn *insn, FILE *file);
//原型 df_dump_insn_bottom df.h df-core.cc
void mtcs_dfcore_df_dump_insn_bottom (MtcsDfcore *self,const rtx_insn *insn, FILE *file);
//原型 df_refs_chain_dump df.h df-core.cc
void mtcs_dfcore_df_refs_chain_dump (MtcsDfcore *self,df_ref ref, bool follow_chain, FILE *file);
//原型 df_regs_chain_dump df.h df-core.cc
void mtcs_dfcore_df_regs_chain_dump (MtcsDfcore *self,df_ref ref,  FILE *file);
//原型 df_insn_debug df.h df-core.cc
void mtcs_dfcore_df_insn_debug (MtcsDfcore *self,rtx_insn *insn, bool follow_chain, FILE *file);
//原型 df_insn_debug_regno df.h df-core.cc
void mtcs_dfcore_df_insn_debug_regno (MtcsDfcore *self,rtx_insn *insn, FILE *file);
//原型 df_regno_debug df.h df-core.cc
void mtcs_dfcore_df_regno_debug (MtcsDfcore *self,unsigned int regno, FILE *file);
//原型 df_ref_debug  df.h df-core.cc
void mtcs_dfcore_df_ref_debug (MtcsDfcore *self,df_ref ref, FILE *file);
//原型 debug_df_insn  df.h df-core.cc
void mtcs_dfcore_debug_df_insn (MtcsDfcore *self,rtx_insn *insn);
//原型 debug_df_reg  df.h df-core.cc
void mtcs_dfcore_debug_df_reg (MtcsDfcore *self,rtx reg);
//原型 debug_df_regno  df.h df-core.cc
void mtcs_dfcore_debug_df_regno (MtcsDfcore *self,unsigned int regno);
//原型 debug_df_ref   df.h df-core.cc
void mtcs_dfcore_debug_df_ref (MtcsDfcore *self,df_ref ref);
//原型 debug_df_defno   df.h df-core.cc
void mtcs_dfcore_debug_df_defno (MtcsDfcore *self,unsigned int defno);
//原型 debug_df_useno   df.h df-core.cc
void mtcs_dfcore_debug_df_useno (MtcsDfcore *self,unsigned int defno);
//原型 debug_df_chain   df.h df-core.cc
void mtcs_dfcore_debug_df_chain (MtcsDfcore *self,struct df_link *link);

//原型 NEXT_PASS (pass_df_initialize_opt, 1); RTL_PASS df-core.cc dfinit n 有条件执行 optimize > 0 rest_of_handle_df_initialize
typedef struct _MtcsPassDfInitializeOpt MtcsPassDfInitializeOpt;
struct _MtcsPassDfInitializeOpt
{
   MtcsPass parent;
};
MtcsPassDfInitializeOpt *mtcs_pass_df_initialize_opt_new(MtcsMode *mtcsMode,int num);

//原型 NEXT_PASS (pass_df_initialize_no_opt, 1); RTL_PASS df-core.cc no-opt dfinit n 有条件执行 optimize == 0 rest_of_handle_df_initialize
typedef struct _MtcsPassDfInitializeNoOpt MtcsPassDfInitializeNoOpt;
struct _MtcsPassDfInitializeNoOpt
{
   MtcsPass parent;
};
MtcsPassDfInitializeNoOpt *mtcs_pass_df_initialize_no_opt_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_df_finish, 1); RTL_PASS df-core.cc dfinish n 无条件执行 rest_of_handle_df_finish
typedef struct _MtcsPassDfFinish MtcsPassDfFinish;
struct _MtcsPassDfFinish
{
   MtcsPass parent;
};
MtcsPassDfFinish *mtcs_pass_df_finish_new(MtcsMode *mtcsMode);


#endif

