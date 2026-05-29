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


#ifndef __GCC_MTCS_LOOP_INVARIANT__
#define __GCC_MTCS_LOOP_INVARIANT__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "../mtcspass.h"
#include "../mtcsmicro.h"

struct invariant ;

typedef struct _MtcsLoopInvariant MtcsLoopInvariant;
struct _MtcsLoopInvariant
{
    MtcsPass parent;
    /* Currently processed loop.  */
    class loop *curr_loop;
    /* Table of invariants indexed by the df_ref uid field.  */
    unsigned int invariant_table_size = 0;
    struct invariant ** invariant_table;
    /* The actual stamp for marking already visited invariants during determining
       costs of movements.  */
    unsigned actual_stamp;
    /* The invariants.  */
    vec<struct invariant *> invariants;

    /* Registers currently living.  */
    bitmap_head curr_regs_live;

    /* Current reg pressure for each pressure class.  */
    int curr_reg_pressure[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];
    /* Number of regs stored in the previous array.  */
    int n_regs_set;

    /* Record all regs that are set in any one insn.  Communication from
       mark_reg_{store,clobber} and global_conflicts.  Asm can refer to
       all hard-registers.  */
    rtx regs_set[MAX_MAX_RECOG_OPERANDS*2/*!(FIRST_PSEUDO_REGISTER > MAX_RECOG_OPERANDS ? FIRST_PSEUDO_REGISTER : MAX_RECOG_OPERANDS) * 2*/];


};

//原型 NEXT_PASS (pass_rtl_move_loop_invariants, 1); RTL_PASS loop-init.cc loop2_invariant  y 有条件执行  flag_move_loop_invariants move_loop_invariants
MtcsLoopInvariant *mtcs_loop_invariant_new(MtcsMode *mtcsMode);


#endif

