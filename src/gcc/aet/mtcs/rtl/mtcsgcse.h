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


#ifndef __GCC_MTCS_GCSE__
#define __GCC_MTCS_GCSE__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "../mtcspass.h"
#include "../mtcsmicro.h"
#include "gcse-common.h"

//原型 NEXT_PASS (pass_rtl_pre, 1); RTL_PASS gcse.cc rtl pre n 有条件执行 optimize > 0 && flag_gcse... execute_rtl_pre;
/* Hash table of expressions.  */
struct gcse_expr
{
  /* The expression.  */
  rtx expr;
  /* Index in the available expression bitmaps.  */
  int bitmap_index;
  /* Next entry with the same hash.  */
  struct gcse_expr *next_same_hash;
  /* List of anticipatable occurrences in basic blocks in the function.
     An "anticipatable occurrence" is one that is the first occurrence in the
     basic block, the operands are not modified in the basic block prior
     to the occurrence and the output is not used between the start of
     the block and the occurrence.  */
  struct gcse_occr *antic_occr;
  /* List of available occurrence in basic blocks in the function.
     An "available occurrence" is one that is the last occurrence in the
     basic block and the operands are not modified by following statements in
     the basic block [including this insn].  */
  struct gcse_occr *avail_occr;
  /* Non-null if the computation is PRE redundant.
     The value is the newly created pseudo-reg to record a copy of the
     expression in all the places that reach the redundant copy.  */
  rtx reaching_reg;
  /* Maximum distance in instructions this expression can travel.
     We avoid moving simple expressions for more than a few instructions
     to keep register pressure under control.
     A value of "0" removes restrictions on how far the expression can
     travel.  */
  HOST_WIDE_INT max_distance;
};

/* Occurrence of an expression.
   There is one per basic block.  If a pattern appears more than once the
   last appearance is used [or first for anticipatable expressions].  */

struct gcse_occr
{
  /* Next occurrence of this expression.  */
  struct gcse_occr *next;
  /* The insn that computes the expression.  */
  rtx_insn *insn;
  /* Nonzero if this [anticipatable] occurrence has been deleted.  */
  char deleted_p;
  /* Nonzero if this [available] occurrence has been copied to
     reaching_reg.  */
  /* ??? This is mutually exclusive with deleted_p, so they could share
     the same byte.  */
  char copied_p;
};


typedef struct gcse_occr *occr_t;

/* Expression hash tables.
   Each hash table is an array of buckets.
   ??? It is known that if it were an array of entries, structure elements
   `next_same_hash' and `bitmap_index' wouldn't be necessary.  However, it is
   not clear whether in the final analysis a sufficient amount of memory would
   be saved as the size of the available expression bitmaps would be larger
   [one could build a mapping table without holes afterwards though].
   Someday I'll perform the computation and figure it out.  */

struct gcse_hash_table_d
{
  /* The table itself.
     This is an array of `expr_hash_table_size' elements.  */
  struct gcse_expr **table;

  /* Size of the hash table, in elements.  */
  unsigned int size;

  /* Number of hash table elements.  */
  unsigned int n_elems;
};

/* This is a list of expressions which are MEMs and will be used by load
   or store motion.
   Load motion tracks MEMs which aren't killed by anything except itself,
   i.e. loads and stores to a single location.
   We can then allow movement of these MEM refs with a little special
   allowance. (all stores copy the same value to the reaching reg used
   for the loads).  This means all values used to store into memory must have
   no side effects so we can re-issue the setter value.  */

struct ls_expr
{
  struct gcse_expr * expr; /* Gcse expression reference for LM.  */
  rtx pattern;       /* Pattern of this mem.  */
  rtx pattern_regs;     /* List of registers mentioned by the mem.  */
  vec<rtx_insn *> stores;  /* INSN list of stores seen.  */
  struct ls_expr * next;   /* Next in the list.  */
  int invalid;       /* Invalid for some reason.  */
  int index;         /* If it maps to a bitmap index.  */
  unsigned int hash_index; /* Index when in a hash table.  */
  rtx reaching_reg;     /* Register to use when re-writing.  */
};

/* Hash table support.  */
struct reg_avail_info
{
  basic_block last_bb;
  int first_set;
  int last_set;
};

class pre_ldst_expr_hasher;

typedef struct _MtcsGcse MtcsGcse;
struct _MtcsGcse
{
    MtcsComponent parent;
    /* An obstack for our working variables.  */
    struct obstack gcse_obstack;
    /* Expression hash table.  */
    struct gcse_hash_table_d expr_hash_table;
    /* Head of the list of load/store memory refs.  */
    struct ls_expr * pre_ldst_mems ;
    /* Hashtable for the load/store memory refs.  */
    hash_table<pre_ldst_expr_hasher> *pre_ldst_table;


    /* Bitmap containing one bit for each register in the program.
       Used when performing GCSE to track which registers have been set since
       the start of the basic block.  */
    regset reg_set_bitmap;

    /* Array, indexed by basic block number for a list of insns which modify
       memory within that block.  */
     vec<rtx_insn *> *modify_mem_list;
     bitmap modify_mem_list_set;

    /* This array parallels modify_mem_list, except that it stores MEMs
       being set and their canonicalized memory addresses.  */
     vec<modify_pair> *canon_modify_mem_list;

    /* Bitmap indexed by block numbers to record which blocks contain
       function calls.  */
     bitmap blocks_with_calls;

    /* Various variables for statistics gathering.  */

    /* Memory used in a pass.
       This isn't intended to be absolutely precise.  Its intent is only
       to keep an eye on memory usage.  */
     int bytes_used;

    /* GCSE substitutions made.  */
     int gcse_subst_count;
    /* Number of copy instructions created.  */
     int gcse_create_count;

    /* Doing code hoisting.  */
     bool doing_code_hoisting_p;

    /* For available exprs */
     sbitmap *ae_kill;


     basic_block curr_bb;

     /* Current register pressure for each pressure class.  */
     int curr_reg_pressure[MAX_N_REG_CLASSES/*!N_REG_CLASSES足够大*/];


      struct reg_avail_info *reg_avail_info;
      basic_block current_bb;

      /* Used internally by can_assign_to_reg_without_clobbers_p.  */
       GTY(()) rtx_insn *test_insn;
       /* Compute PRE+LCM working variables.  */

       /* Local properties of expressions.  */

       /* Nonzero for expressions that are transparent in the block.  */
        sbitmap *transp;

       /* Nonzero for expressions that are computed (available) in the block.  */
        sbitmap *comp;

       /* Nonzero for expressions that are locally anticipatable in the block.  */
        sbitmap *antloc;

       /* Nonzero for expressions where this block is an optimal computation
          point.  */
        sbitmap *pre_optimal;

       /* Nonzero for expressions which are redundant in a particular block.  */
        sbitmap *pre_redundant;

       /* Nonzero for expressions which should be inserted on a specific edge.  */
        sbitmap *pre_insert_map;

       /* Nonzero for expressions which should be deleted in a specific block.  */
        sbitmap *pre_delete_map;

        /* Code Hoisting variables and subroutines.  */

        /* Very busy expressions.  */
        sbitmap *hoist_vbein;
        sbitmap *hoist_vbeout;

     /* Nonzero for each mode that supports (set (reg) (reg)).
        This is trivially true for integer and floating point values.
        It may or may not be true for condition codes.  */
     //原型 x_can_copy struct target_gcse  gcse.h
     char x_can_copy[MAX_NUM_MACHINE_MODES/*!(int) NUM_MACHINE_MODES足够大*/];
     //原型 x_can_copy_init_p struct target_gcse  gcse.h
     /* True if the previous field has been initialized.  */
     bool x_can_copy_init_p;
     /* Doing hardreg_pre.  */
     bool doing_hardreg_pre_p ;
     unsigned int current_hardreg_regno;

};

MtcsGcse *mtcs_gcse_new(MtcsMode *mtcsMode);
//原型 can_copy_p rtl.h gcse.cc
bool mtcs_gcse_can_copy_p (MtcsGcse *self,machine_mode mode);
//原型 can_assign_to_reg_without_clobbers_p rtl.h gcse.cc
bool mtcs_gcse_can_assign_to_reg_without_clobbers_p (MtcsGcse *self,rtx x, machine_mode mode);
//原型 record_last_mem_set_info_common gcse-common.h gcse-common.cc
void mtcs_gcse_record_last_mem_set_info_common (MtcsGcse *self,rtx_insn *insn,
             vec<rtx_insn *> *modify_mem_list,
             vec<modify_pair> *canon_modify_mem_list,
             bitmap modify_mem_list_set,
             bitmap blocks_with_calls);
//原型 prepare_copy_insn rtl.h gcse.cc
rtx_insn * mtcs_gcse_prepare_copy_insn (MtcsGcse *self,rtx reg, rtx exp);
//原型 gcse_or_cprop_is_too_expensive gcse.h gcse.cc
bool mtcs_gcse_gcse_or_cprop_is_too_expensive (MtcsGcse *self,const char *pass);
//原型 gcse_cc_finalize gcse.h gcse.cc
void mtcs_gcse_cc_finalize (MtcsGcse *self);

//原型 NEXT_PASS (pass_rtl_pre, 1); RTL_PASS gcse.cc rtl pre  y 有条件执行 optimize > 0 && flag_gcse...
typedef struct _MtcsPassRtlPre MtcsPassRtlPre;
struct _MtcsPassRtlPre
{
   MtcsPass parent;
};
MtcsPassRtlPre *mtcs_pass_rtl_pre_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_rtl_hoist, 1); RTL_PASS gcse.cc hoist  y 有条件执行  optimize > 0 && flag_gcse...
typedef struct _MtcsPassHoist MtcsPassHoist;
struct _MtcsPassHoist
{
   MtcsPass parent;
};
MtcsPassHoist *mtcs_pass_hoist_new(MtcsMode *mtcsMode);


#endif

