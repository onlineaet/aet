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


#ifndef __GCC_MTCS_POST_RELOAD__
#define __GCC_MTCS_POST_RELOAD__

#include "../../nlib.h"
#include "../mtcspass.h"
#include "../mtcsmicro.h"

//原型 NEXT_PASS (pass_postreload_cse, 1); RTL_PASS postreload.cc postreload n 有条件执行 (optimize > 0 && reload_completed);reload_cse_regs

/* If reload couldn't use reg+reg+offset addressing, try to use reg+reg
   addressing now.
   This code might also be useful when reload gave up on reg+reg addressing
   because of clashes between the return register and INDEX_REG_CLASS.  */

/* The maximum number of uses of a register we can keep track of to
   replace them with reg+reg addressing.  */
#define MTCS_RELOAD_COMBINE_MAX_USES 16


/* Describes a recorded use of a register.  */
struct reg_use
{
  /* The insn where a register has been used.  */
  rtx_insn *insn;
  /* Points to the memory reference enclosing the use, if any, NULL_RTX
     otherwise.  */
  rtx containing_mem;
  /* Location of the register within INSN.  */
  rtx *usep;
  /* The reverse uid of the insn.  */
  int ruid;
};


typedef struct _MtcsPostReload  MtcsPostReload;
struct _MtcsPostReload
{
   MtcsPass parent;
   /* Reverse linear uid.  This is increased in reload_combine while scanning
      the instructions from last to first.  It is used to set last_label_ruid
      and the store_ruid / use_ruid fields in reg_state.  */
    int reload_combine_ruid;

   /* The RUID of the last label we encountered in reload_combine.  */
    int last_label_ruid;

   /* The RUID of the last jump we encountered in reload_combine.  */
    int last_jump_ruid;

   /* The register numbers of the first and last index register.  A value of
      -1 in LAST_INDEX_REG indicates that we've previously computed these
      values and found no suitable index registers.  */
    int first_index_reg ;//= -1;
    int last_index_reg;
    /* If the register is used in some unknown fashion, USE_INDEX is negative.
       If it is dead, USE_INDEX is RELOAD_COMBINE_MAX_USES, and STORE_RUID
       indicates where it is first set or clobbered.
       Otherwise, USE_INDEX is the index of the last encountered use of the
       register (which is first among these we have seen since we scan backwards).
       USE_RUID indicates the first encountered, i.e. last, of these uses.
       If ALL_OFFSETS_MATCH is true, all encountered uses were inside a PLUS
       with a constant offset; OFFSET contains this constant in that case.
       STORE_RUID is always meaningful if we only want to use a value in a
       register in a different place: it denotes the next insn in the insn
       stream (i.e. the last encountered) that sets or clobbers the register.
       REAL_STORE_RUID is similar, but clobbers are ignored when updating it.
       EXPR is the expression used when storing the register.  */
    struct
      {
        struct reg_use reg_use[MTCS_RELOAD_COMBINE_MAX_USES/*!RELOAD_COMBINE_MAX_USES*/];
        rtx offset;
        int use_index;
        int store_ruid;
        int real_store_ruid;
        int use_ruid;
        bool all_offsets_match;
        rtx expr;
      } reg_state[MAX_FIRST_PSEUDO_REGISTER];


      /* See if we can reduce the cost of a constant by replacing a move
         with an add.  We track situations in which a register is set to a
         constant or to a register plus a constant.  */
      /* We cannot do our optimization across labels.  Invalidating all the
         information about register contents we have would be costly, so we
         use move2add_last_label_luid to note where the label is and then
         later disable any optimization that would cross it.
         reg_offset[n] / reg_base_reg[n] / reg_symbol_ref[n] / reg_mode[n]
         are only valid if reg_set_luid[n] is greater than
         move2add_last_label_luid.
         For a set that established a new (potential) base register with
         non-constant value, we use move2add_luid from the place where the
         setting insn is encountered; registers based off that base then
         get the same reg_set_luid.  Constants all get
         move2add_last_label_luid + 1 as their reg_set_luid.  */
      int reg_set_luid[MAX_FIRST_PSEUDO_REGISTER];

      /* If reg_base_reg[n] is negative, register n has been set to
         reg_offset[n] or reg_symbol_ref[n] + reg_offset[n] in mode reg_mode[n].
         If reg_base_reg[n] is non-negative, register n has been set to the
         sum of reg_offset[n] and the value of register reg_base_reg[n]
         before reg_set_luid[n], calculated in mode reg_mode[n] .
         For multi-hard-register registers, all but the first one are
         recorded as BLKmode in reg_mode.  Setting reg_mode to VOIDmode
         marks it as invalid.  */
       HOST_WIDE_INT reg_offset[MAX_FIRST_PSEUDO_REGISTER];
       int reg_base_reg[MAX_FIRST_PSEUDO_REGISTER];
       rtx reg_symbol_ref[MAX_FIRST_PSEUDO_REGISTER];
       machine_mode reg_mode[MAX_FIRST_PSEUDO_REGISTER];

      /* move2add_luid is linearly increased while scanning the instructions
         from first to last.  It is used to set reg_set_luid in
         reload_cse_move2add and move2add_note_store.  */
       int move2add_luid;

      /* move2add_last_label_luid is set whenever a label is found.  Labels
         invalidate all previously collected reg_offset data.  */
       int move2add_last_label_luid;

};


MtcsPostReload *mtcs_post_reload_new(MtcsMode *mtcsMode);


#endif
