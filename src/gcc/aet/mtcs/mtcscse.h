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


#ifndef __GCC_MTCS_CSE__
#define __GCC_MTCS_CSE__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcsmicro.h"
#include "mtcspass.h"

struct qty_table_elem
{
   rtx const_rtx;
   rtx_insn *const_insn;
   rtx comparison_const;
   int comparison_qty;
   unsigned int first_reg, last_reg;
   ENUM_BITFIELD(machine_mode) mode : MACHINE_MODE_BITSIZE;
   ENUM_BITFIELD(rtx_code) comparison_code : RTX_CODE_BITSIZE;
};


/* Per-register equivalence chain.  */
struct reg_eqv_elem
{
   int next, prev;
};

/* Index by register number, gives the number of the next (or
   previous) register in the chain of registers sharing the same
   value.

   Or -1 if this register is at the end of the chain.

   If REG_QTY (N) == -N - 1, reg_eqv_table[N].next is undefined.  */

struct cse_reg_info
{
  /* The timestamp at which this register is initialized.  */
  unsigned int timestamp;

  /* The quantity number of the register's current contents.  */
  int reg_qty;

  /* The number of times the register has been altered in the current
     basic block.  */
  int reg_tick;

  /* The REG_TICK value at which rtx's containing this register are
     valid in the hash table.  If this does not equal the current
     reg_tick value, such expressions existing in the hash table are
     invalid.  */
  int reg_in_table;

  /* The SUBREG that was set when REG_TICK was last incremented.  Set
     to -1 if the last store was to the whole register, not a subreg.  */
  unsigned int subreg_ticked;
};


struct table_elt
{
  rtx exp;
  rtx canon_exp;
  struct table_elt *next_same_hash;
  struct table_elt *prev_same_hash;
  struct table_elt *next_same_value;
  struct table_elt *prev_same_value;
  struct table_elt *first_same_value;
  struct table_elt *related_value;
  int cost;
  int regcost;
  ENUM_BITFIELD(machine_mode) mode : MACHINE_MODE_BITSIZE;
  char in_memory;
  char is_const;
  char flag;
};

/*
 * CSE ( Common Subexpression Elimination ,通用子表达式消除) 一种优化技术,它可以识别重复的表达式并重用它的值,而不是再次执行相应的计算
 */
typedef struct _MtcsCse MtcsCse;
struct _MtcsCse
{
    MtcsComponent parent;
    int max_qty;
    /* Next quantity number to be allocated.
       This is 1 + the largest number needed so far.  */
    int next_qty;


    /* The table of all qtys, indexed by qty number.  */
    struct qty_table_elem *qty_table;

    /* Insn being scanned.  */
    rtx_insn *this_insn;
    bool optimize_this_for_speed_p;

    /* The table of all register equivalence chains.  */
    struct reg_eqv_elem *reg_eqv_table;



    /* A table of cse_reg_info indexed by register numbers.  */
    struct cse_reg_info *cse_reg_info_table;



    /* The size of the above table.  */
     unsigned int cse_reg_info_table_size;

    /* The index of the first entry that has not been initialized.  */
     unsigned int cse_reg_info_table_first_uninitialized;

    /* The timestamp at the beginning of the current run of
       cse_extended_basic_block.  We increment this variable at the beginning of
       the current run of cse_extended_basic_block.  The timestamp field of a
       cse_reg_info entry matches the value of this variable if and only
       if the entry has been initialized during the current run of
       cse_extended_basic_block.  */
     unsigned int cse_reg_info_timestamp;

    /* A HARD_REG_SET containing all the hard registers for which there is
       currently a REG expression in the hash table.  Note the difference
       from the above variables, which indicate if the REG is mentioned in some
       expression in the table.  */

     HardRegSet /*!HARD_REG_SET*/ hard_regs_in_table;

    /* True if CSE has altered the CFG.  */
     bool cse_cfg_altered;

    /* True if CSE has altered conditional jump insns in such a way
       that jump optimization should be redone.  */
     bool cse_jumps_altered;

    /* True if we put a LABEL_REF into the hash table for an INSN
       without a REG_LABEL_OPERAND, we have to rerun jump after CSE
       to put in the note.  */
     bool recorded_label_ref;

    /* canon_hash stores 1 in do_not_record if it notices a reference to PC or
       some other volatile subexpression.  */

     int do_not_record;

    /* canon_hash stores 1 in hash_arg_in_memory
       if it notices a reference to memory within the expression being hashed.  */

     int hash_arg_in_memory;

     struct table_elt *table[(1 << 5)];

     /* Chain of `struct table_elt's made so far for this function
        but currently removed from the table.  */
      struct table_elt *free_element_chain;
     /* Pointers to the live in/live out bitmaps for the boundaries of the
        current EBB.  */
      bitmap cse_ebb_live_in, cse_ebb_live_out;

     /* A simple bitmap to track which basic blocks have been visited
        already as part of an already processed extended basic block.  */
      sbitmap cse_visited_basic_blocks;


};

MtcsCse *mtcs_cse_new(MtcsMode *mtcsMode);
//原型 delete_trivially_dead_insns rtl.h cse.cc
int mtcs_cse_delete_trivially_dead_insns (MtcsCse *self,rtx_insn *insns, int nreg);
//原型 exp_equiv_p rtl.h cse.cc
bool mtcs_cse_exp_equiv_p (MtcsCse *self,const_rtx x, const_rtx y, int validate, bool for_gcse);
//原型 hash_rtx rtl.h cse.cc
unsigned mtcs_cse_hash_rtx (MtcsCse *self,const_rtx x, machine_mode mode,
     int *do_not_record_p, int *hash_arg_in_memory_p, bool have_reg_qty, hash_rtx_callback_function cb=NULL);

//原型 NEXT_PASS (pass_cse, 1); RTL_PASS cse.cc cse1 n 有条件执行 optimize > 0 rest_of_handle_cse
typedef struct _MtcsPassCse1  MtcsPassCse1;
struct _MtcsPassCse1
{
   MtcsPass parent;
};
MtcsPassCse1 *mtcs_pass_cse1_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_cse2, 1); RTL_PASS cse.cc cse2 n 有条件执行 optimize > 0 && flag_rerun_cse_after_loop; rest_of_handle_cse2
typedef struct _MtcsPassCse2  MtcsPassCse2;
struct _MtcsPassCse2
{
   MtcsPass parent;
};
MtcsPassCse2 *mtcs_pass_cse2_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_cse_after_global_opts, 1); RTL_PASS cse.cc cse_local n 有条件执行 optimize > 0 ... rest_of_handle_cse_after_global_opts
typedef struct _MtcsPassCseLocal  MtcsPassCseLocal;
struct _MtcsPassCseLocal
{
   MtcsPass parent;
};
MtcsPassCseLocal *mtcs_pass_cse_local_new(MtcsMode *mtcsMode);

#endif

