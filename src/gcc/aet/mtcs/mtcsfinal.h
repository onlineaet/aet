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


#ifndef __GCC_MTCS_FINAL__
#define __GCC_MTCS_FINAL__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcspass.h"

typedef struct _MtcsFinal MtcsFinal;
struct _MtcsFinal
{
   MtcsComponent parent;
   rtx_sequence *final_sequence; //原型  final_sequence output.h final.cc
   int dialect_number;//依赖宏ASSEMBLER_DIALECT
   bool app_on; //原型 app_on final.cc
   /* Whether we saw any functions with no_split_stack.  */
   bool saw_no_split_stack;
   bool need_profile_function;
   bool force_source_line;
   int block_depth;

   /* Filename of last NOTE.  */
   const char *last_filename;

   /* Override filename, line and column number.  */
    const char *override_filename;
    int override_linenum;
    int override_columnnum;
    int override_discriminator;

    /* Line number of last NOTE.  */
    int last_linenum;

    /* Column number of last NOTE.  */
    int last_columnnum;

    /* Discriminator written to assembly.  */
    int last_discriminator;

    /* Highest line number in current block.  */
    int high_block_linenum;

    /* Likewise for function.  */
    int high_function_linenum;
    rtx last_ignored_compare ;
    const char *some_local_dynamic_name;
    int insn_last_address;
    int min_labelno, max_labelno;

    rtx *uid_align;
    int *uid_shuid;
    vec<align_flags> label_align;

    /* Max uid for which the above arrays are valid.  */
    int insn_lengths_max_uid;
    int *insn_lengths;

    /* known invariant alignment of insn being processed.  */
    int insn_current_align;
};

MtcsFinal *mtcs_final_new(MtcsMode *mtcsMode);
rtx_insn  *mtcs_final_final_scan_insn (MtcsFinal *self,rtx_insn *insn, int optimize_p,int nopeepholes, int *seen);
void       mtcs_final_final_end_function (MtcsFinal *self);
//原型 init_final output.h final.cc
void       mtcs_final_init_final (MtcsFinal *self,const char *mainName);
//原型 get_attr_min_length output.h final.cc
int mtcs_final_get_attr_min_length (MtcsFinal *self,rtx_insn *insn);
//原型 get_attr_length output.h final.cc
int mtcs_final_get_attr_length (MtcsFinal *self,rtx_insn *insn);
//原型 compute_alignments rtl.h final.cc
void mtcs_final_compute_alignments (MtcsFinal *self);
//原型 shorten_branches output.h(insn-attr.h host也声明) final.cc
void mtcs_final_shorten_branches (MtcsFinal *self,rtx_insn *first);
//原型 app_enable output.h final.cc
void mtcs_final_app_enable(MtcsFinal *self);
//原型 app_disable output.h final.cc
void mtcs_final_app_disable(MtcsFinal *self);
//原型 init_insn_lengths output.h final.cc
void mtcs_final_init_insn_lengths (MtcsFinal *self);

//原型 NEXT_PASS (pass_compute_alignments, 1);  RTL_PASS  final.cc  alignments   y 无条件执行 compute_alignments
typedef struct _MtcsPassComputeAlignments MtcsPassComputeAlignments;
struct _MtcsPassComputeAlignments
{
   MtcsPass parent;
};
MtcsPassComputeAlignments *mtcs_pass_compute_alignments_new (MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_shorten_branches, 1);  RTL_PASS  final.cc  shorten   y 无条件执行 rest_of_handle_shorten_branches
typedef struct _MtcsPassShortenBranches MtcsPassShortenBranches;
struct _MtcsPassShortenBranches
{
   MtcsPass parent;
};
MtcsPassShortenBranches *mtcs_pass_shorten_branches_new (MtcsMode *mtcsMode);


//原型 NEXT_PASS (pass_final, 1);  RTL_PASS  final.cc  final   y 无条件执行 rest_of_handle_final
typedef struct _MtcsPassFinal MtcsPassFinal;
struct _MtcsPassFinal
{
   MtcsPass parent;
};
MtcsPassFinal *mtcs_pass_final_new (MtcsMode *mtcsMode);


//原型 NEXT_PASS (pass_clean_state, 1);  RTL_PASS  final.cc *clean_state   y 无条件执行 rest_of_clean_state
typedef struct _MtcsPassCleanState MtcsPassCleanState;
struct _MtcsPassCleanState
{
   MtcsPass parent;
};
MtcsPassCleanState *mtcs_pass_clean_state_new (MtcsMode *mtcsMode);

#endif
