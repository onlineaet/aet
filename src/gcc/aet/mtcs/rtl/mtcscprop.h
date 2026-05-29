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


#ifndef __GCC_MTCS_CPROP__
#define __GCC_MTCS_CPROP__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "../mtcspass.h"
#include "regset.h"

//原型 NEXT_PASS (pass_rtl_cprop, 1); RTL_PASS cprop.cc cprop n 有条件执行 optimize > 0 && flag_gcse... execute_rtl_cprop;
/* Occurrence of an expression.
   There is one per basic block.  If a pattern appears more than once the
   last appearance is used.  */

struct cprop_occr
{
  /* Next occurrence of this expression.  */
  struct cprop_occr *next;
  /* The insn that computes the expression.  */
  rtx_insn *insn;
};

/* Hash table entry for assignment expressions.  */

struct cprop_expr
{
  /* The expression (DEST := SRC).  */
  rtx dest;
  rtx src;

  /* Index in the available expression bitmaps.  */
  int bitmap_index;
  /* Next entry with the same hash.  */
  struct cprop_expr *next_same_hash;
  /* List of available occurrence in basic blocks in the function.
     An "available occurrence" is one that is the last occurrence in the
     basic block and whose operands are not modified by following statements
     in the basic block [including this insn].  */
  struct cprop_occr *avail_occr;
};

/* Hash table for copy propagation expressions.
   Each hash table is an array of buckets.
   ??? It is known that if it were an array of entries, structure elements
   `next_same_hash' and `bitmap_index' wouldn't be necessary.  However, it is
   not clear whether in the final analysis a sufficient amount of memory would
   be saved as the size of the available expression bitmaps would be larger
   [one could build a mapping table without holes afterwards though].
   Someday I'll perform the computation and figure it out.  */

struct hash_table_d
{
  /* The table itself.
     This is an array of `set_hash_table_size' elements.  */
  struct cprop_expr **table;

  /* Size of the hash table, in elements.  */
  unsigned int size;

  /* Number of hash table elements.  */
  unsigned int n_elems;
};

typedef struct _MtcsCprop MtcsCprop;
struct _MtcsCprop
{
    MtcsComponent parent;
    /* An obstack for our working variables.  */
    struct obstack cprop_obstack;

    /* Copy propagation hash table.  */
     struct hash_table_d set_hash_table;

    /* Array of implicit set patterns indexed by basic block index.  */
     rtx *implicit_sets;

    /* Array of indexes of expressions for implicit set patterns indexed by basic
       block index.  In other words, implicit_set_indexes[i] is the bitmap_index
       of the expression whose RTX is implicit_sets[i].  */
     int *implicit_set_indexes;

    /* Bitmap containing one bit for each register in the program.
       Used when performing GCSE to track which registers have been set since
       the start or end of the basic block while traversing that block.  */
     regset reg_set_bitmap;

    /* Various variables for statistics gathering.  */

    /* Memory used in a pass.
       This isn't intended to be absolutely precise.  Its intent is only
       to keep an eye on memory usage.  */
     int bytes_used;

    /* Number of local constants propagated.  */
     int local_const_prop_count;
    /* Number of local copies propagated.  */
     int local_copy_prop_count;
    /* Number of global constants propagated.  */
     int global_const_prop_count;
    /* Number of global copies propagated.  */
     int global_copy_prop_count;

     /* Compute copy/constant propagation working variables.  */

     /* Local properties of assignments.  */
     sbitmap *cprop_avloc;
     sbitmap *cprop_kill;

     /* Global properties of assignments (computed from the local properties).  */
     sbitmap *cprop_avin;
     sbitmap *cprop_avout;

     /* Table of uses (registers, both hard and pseudo) found in an insn.
        Allocated statically to avoid alloc/free complexity and overhead.  */
     rtx reg_use_table[10/*!MAX_USES*/];

     /* Index into `reg_use_table' while building it.  */
     unsigned reg_use_count;

     /* The value of last_basic_block at the beginning of the jump_bypass
        pass.  The use of redirect_edge_and_branch_force may introduce new
        basic blocks, but the data flow analysis is only valid for basic
        block indices less than bypass_last_basic_block.  */

     int bypass_last_basic_block;
 };

MtcsCprop *mtcs_cprop_new(MtcsMode *mtcsMode);
//原型 fis_get_condition rtl.h cprop.cc
rtx mtcs_cprop_fis_get_condition (MtcsCprop *self,rtx_insn *jump);


//原型 NEXT_PASS (pass_rtl_cprop, 1); RTL_PASS cprop.cc cprop y 有条件执行 optimize > 0 && flag_forward_propagate;   fwprop (false);
typedef struct _MtcsPassCprop MtcsPassCprop;
struct _MtcsPassCprop
{
   MtcsPass parent;
};
MtcsPassCprop *mtcs_pass_cprop_new(MtcsMode *mtcsMode);

#endif

