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


#ifndef __GCC_MTCS_COMBINE__
#define __GCC_MTCS_COMBINE__

#include "../../nlib.h"
#include "../mtcspass.h"
#include "../mtcsmicro.h"

//原型 reg_stat_type combine.cc

struct mtcs_reg_stat_type/*!mtcs_reg_stat_type*/ {
  /* Record last point of death of (hard or pseudo) register n.  */
  rtx_insn        *last_death;

  /* Record last point of modification of (hard or pseudo) register n.  */
  rtx_insn        *last_set;

  /* The next group of fields allows the recording of the last value assigned
     to (hard or pseudo) register n.  We use this information to see if an
     operation being processed is redundant given a prior operation performed
     on the register.  For example, an `and' with a constant is redundant if
     all the zero bits are already known to be turned off.

     We use an approach similar to that used by cse, but change it in the
     following ways:

     (1) We do not want to reinitialize at each label.
     (2) It is useful, but not critical, to know the actual value assigned
    to a register.  Often just its form is helpful.

     Therefore, we maintain the following fields:

     last_set_value     the last value assigned
     last_set_label     records the value of self->label_tick when the
            register was assigned
     last_set_table_tick   records the value of self->label_tick when a
            value using the register is assigned
     last_set_invalid      set to true when it is not valid
            to use the value of this register in some
            register's value

     To understand the usage of these tables, it is important to understand
     the distinction between the value in last_set_value being valid and
     the register being validly contained in some other expression in the
     table.

     (The next two parameters are out of date).

     self->reg_stat[i].last_set_value is valid if it is nonzero, and either
     reg_n_sets[i] is 1 or self->reg_stat[i].last_set_label == self->label_tick.

     Register I may validly appear in any expression returned for the value
     of another register if reg_n_sets[i] is 1.  It may also appear in the
     value for register J if self->reg_stat[j].last_set_invalid is zero, or
     self->reg_stat[i].last_set_label < self->reg_stat[j].last_set_label.

     If an expression is found in the table containing a register which may
     not validly appear in an expression, the register is replaced by
     something that won't match, (clobber (const_int 0)).  */

  /* Record last value assigned to (hard or pseudo) register n.  */

  rtx          last_set_value;

  /* Record the value of self->label_tick when an expression involving register n
     is placed in last_set_value.  */

  int          last_set_table_tick;

  /* Record the value of self->label_tick when the value for register n is placed in
     last_set_value.  */

  int          last_set_label;

  /* These fields are maintained in parallel with last_set_value and are
     used to store the mode in which the register was last set, the bits
     that were known to be zero when it was last set, and the number of
     sign bits copies it was known to have when it was last set.  */

  unsigned HOST_WIDE_INT   last_set_nonzero_bits;
  char            last_set_sign_bit_copies;
  ENUM_BITFIELD(machine_mode) last_set_mode : MACHINE_MODE_BITSIZE;

  /* Set to true if references to register n in expressions should not be
     used.  last_set_invalid is set nonzero when this register is being
     assigned to and last_set_table_tick == self->label_tick.  */

  bool            last_set_invalid;

  /* Some registers that are set more than once and used in more than one
     basic block are nevertheless always set in similar ways.  For example,
     a QImode register may be loaded from memory in two places on a machine
     where byte loads zero extend.

     We record in the following fields if a register has some leading bits
     that are always equal to the sign bit, and what we know about the
     nonzero bits of a register, specifically which bits are known to be
     zero.

     If an entry is zero, it means that we don't know anything special.  */

  unsigned char         sign_bit_copies;

  unsigned HOST_WIDE_INT   nonzero_bits;

  /* Record the value of the self->label_tick when the last truncation
     happened.  The field truncated_to_mode is only valid if
     truncation_label == self->label_tick.  */

  int          truncation_label;

  /* Record the last truncation seen for this register.  If truncation
     is not a nop to this mode we might be able to save an explicit
     truncation if we know that value already contains a truncated
     value.  */

  ENUM_BITFIELD(machine_mode) truncated_to_mode : MACHINE_MODE_BITSIZE;
};


struct insn_link;
struct undobuf;
struct undo;

struct undobuf
{
  struct undo *undos;
  struct undo *frees;
  rtx_insn *other_insn;
};


//原型 NEXT_PASS (pass_web, 1); RTL_PASS web.cc  web y 有条件执行代码  optimize > 0 && flag_web

typedef struct _MtcsCombine MtcsCombine;

struct _MtcsCombine
{
     MtcsComponent parent;

    /* Number of attempts to combine instructions in this function.  */

     int combine_attempts;

    /* Number of attempts that got as far as substitution in this function.  */

     int combine_merges;

    /* Number of instructions combined with added SETs in this function.  */

     int combine_extras;

    /* Number of instructions combined in this function.  */

     int combine_successes;

    /* combine_instructions may try to replace the right hand side of the
       second instruction with the value of an associated REG_EQUAL note
       before throwing it at try_combine.  That is problematic when there
       is a REG_DEAD note for a register used in the old right hand side
       and can cause distribute_notes to do wrong things.  This is the
       second instruction if it has been so modified, null otherwise.  */

     rtx_insn *i2mod;

    /* When I2MOD is nonnull, this is a copy of the old right hand side.  */

     rtx i2mod_old_rhs;

    /* When I2MOD is nonnull, this is a copy of the new right hand side.  */

     rtx i2mod_new_rhs;


      vec<mtcs_reg_stat_type/*!reg_stat_type*/> reg_stat;

     /* One plus the highest pseudo for which we track REG_N_SETS.
        regstat_init_n_sets_and_refs allocates the array for REG_N_SETS just once,
        but during combine_split_insns new pseudos can be created.  As we don't have
        updated DF information in that case, it is hard to initialize the array
        after growing.  The combiner only cares about REG_N_SETS (regno) == 1,
        so instead of growing the arrays, just assume all newly created pseudos
        during combine might be set multiple times.  */

      unsigned int reg_n_sets_max;

     /* Record the luid of the last insn that invalidated memory
        (anything that writes memory, and subroutine calls, but not pushes).  */

      int mem_last_set;

     /* Record the luid of the last CALL_INSN
        so we can tell whether a potential combination crosses any calls.  */

      int last_call_luid;

     /* When `subst' is called, this is the insn that is being modified
        (by combining in a previous insn).  The PATTERN of this insn
        is still the old pattern partially modified and it should not be
        looked at, but this may be used to examine the successors of the insn
        to judge whether a simplification is valid.  */

      rtx_insn *subst_insn;

     /* This is the lowest LUID that `subst' is currently dealing with.
        get_last_value will not return a value if the register was set at or
        after this LUID.  If not for this mechanism, we could get confused if
        I2 or I1 in try_combine were an insn that used the old value of a register
        to obtain a new value.  In that case, we might erroneously get the
        new value of the register when we wanted the old one.  */

      int subst_low_luid;

     /* This contains any hard registers that are used in newpat; reg_dead_at_p
        must consider all these registers to be always live.  */

      HardRegSet /*!HARD_REG_SET*/ newpat_used_regs;

     /* This is an insn to which a LOG_LINKS entry has been added.  If this
        insn is the earlier than I2 or I3, combine should rescan starting at
        that location.  */

      rtx_insn *added_links_insn;

     /* And similarly, for notes.  */

      rtx_insn *added_notes_insn;

     /* Basic block in which we are performing combines.  */
      basic_block this_basic_block;
      bool optimize_this_for_speed_p;


     /* Length of the currently allocated uid_insn_cost array.  */

     int max_uid_known;

     /* The following array records the insn_cost for every insn
        in the instruction stream.  */

     int *uid_insn_cost;

     struct insn_link **uid_log_links;

     /* Links for LOG_LINKS are allocated from this obstack.  */

     struct obstack insn_link_obstack;


     /* Incremented for each basic block.  */

      int label_tick;

     /* Reset to label_tick for each extended basic block in scanning order.  */

      int label_tick_ebb_start;

     /* Mode used to compute significance in reg_stat[].nonzero_bits.  It is the
        largest integer mode that can fit in HOST_BITS_PER_WIDE_INT.  */

      scalar_int_mode nonzero_bits_mode;

     /* Nonzero when reg_stat[].nonzero_bits and reg_stat[].sign_bit_copies can
        be safely used.  It is zero while computing them and after combine has
        completed.  This former test prevents propagating values based on
        previously set values, which can be incorrect if a variable is modified
        in a loop.  */

      int nonzero_sign_valid;

      struct undobuf undobuf;

      /* Number of times the pseudo being substituted for
         was found and replaced.  */

      int n_occurrences;

      /* Define three variables used for communication between the following
         routines.  */

      unsigned int reg_dead_regno, reg_dead_endregno;
      int reg_dead_flag;
      rtx reg_dead_reg;
};

MtcsCombine *mtcs_combine_new(MtcsMode *mtcsMode);
//原型 make_compound_operation rtl.h combine.cc valtrack.cc引用
rtx mtcs_combine_make_compound_operation (MtcsCombine *self,rtx x, enum rtx_code in_code);
//原型 extended_count rtl.h combine.cc
unsigned int mtcs_combine_extended_count (MtcsCombine *self,const_rtx x, machine_mode mode, bool unsignedp);

//原型 NEXT_PASS (pass_combine, 1); RTL_PASS combine.cc combine y 有条件执行 optimize > 0 rest_of_handle_combine
typedef struct _MtcsPassCombine MtcsPassCombine;
struct _MtcsPassCombine
{
   MtcsPass parent;
};
MtcsPassCombine *mtcs_pass_combine_new(MtcsMode *mtcsMode);

#endif
