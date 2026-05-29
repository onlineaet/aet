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

#ifndef __GCC_MTCS_IRA_LIVES__
#define __GCC_MTCS_IRA_LIVES__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "mtcsiraobject.h"
#include "mtcsiralooptreenode.h"
#include "sparseset.h"

typedef struct _MtcsIraLives  MtcsIraLives;


struct _MtcsIraLives
{
  MtcsComponent parent;
  /* Program points are enumerated by numbers from range
     0..IRA_MAX_POINT-1.  There are approximately two times more program
     points than insns.  Program points are places in the program where
     liveness info can be changed.  In most general case (there are more
     complicated cases too) some program points correspond to places
     where input operand dies and other ones correspond to places where
     output operands are born.  */
  //原型 ira_max_point ira-int.h
   int ira_max_point; //ira build color conflicts emit lives 引用 emit lives改值

   /* Arrays of size IRA_MAX_POINT mapping a program point to the allocno
      live ranges with given start/finish point.  */
   //原型 ira_start_point_ranges  ira_finish_point_ranges ira-int.h
    MtcsLiveRange  **ira_start_point_ranges, **ira_finish_point_ranges;//build conflicts lives引用

    /* Number of the current program point.  */
    //原型 curr_point ira-lives.cc
     int curr_point;

     /* Point where register pressure excess started or -1 if there is no
        register pressure excess.  Excess pressure for a register class at
        some point means that there are more allocnos of given register
        class living at the point than number of hard-registers of the
        class available for the allocation.  It is defined only for
        pressure classes.  */
     //原型 high_pressure_start_point ira-lives.cc
     int high_pressure_start_point[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];


     /* Objects live at current point in the scan.  */
     //原型 objects_live ira-lives.cc
      sparseset objects_live;

     /* A temporary bitmap used in functions that wish to avoid visiting an allocno
        multiple times.  */
     //原型 allocnos_processed ira-lives.cc
     sparseset allocnos_processed;

     /* Set of hard regs (except eliminable ones) currently live.  */
     //原型 hard_regs_live ira-lives.cc
     HardRegSet hard_regs_live;

     /* The loop tree node corresponding to the current basic block.  */
     //原型 curr_bb_node ira-lives.cc
     MtcsIraLoopTreeNode *curr_bb_node;

     /* The number of the last processed call.  */
     //原型 last_call_num ira-lives.cc
     int last_call_num;

     /* The number of last call at which given allocno was saved.  */
     //原型 allocno_saved_at_call ira-lives.cc
     int *allocno_saved_at_call;

     /* The value returned by ira_setup_alts for the current instruction;
        i.e. the set of alternatives that we should consider to be likely
        candidates during reloading.  */
     //原型 preferred_alternatives ira-lives.cc
     alternative_mask preferred_alternatives;

     /* If non-NULL, the source operand of a register to register copy for which
        we should not add a conflict with the copy's destination operand.  */
     //原型 ignore_reg_for_conflicts ira-lives.cc
     rtx ignore_reg_for_conflicts;

     /* The current register pressures for each pressure class for the current
        basic block.  */
     //原型 curr_reg_pressure ira-lives.cc
      int curr_reg_pressure[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

};


MtcsIraLives *mtcs_ira_lives_new(MtcsMode *mtcsMode);
//原型 ira_implicitly_set_insn_hard_regs ira-int.h ira-lives.cc
void mtcs_ira_lives_implicitly_set_insn_hard_regs (MtcsIraLives *self, HardRegSet *set,
               alternative_mask preferred);
//原型 non_conflicting_reg_copy_p ira.h ira-lives.cc
rtx mtcs_ira_lives_non_conflicting_reg_copy_p (MtcsIraLives *self,rtx_insn *insn);
//原型 ira_rebuild_start_finish_chains ira-int.h ira-lives.cc
void mtcs_ira_lives_rebuild_start_finish_chains (MtcsIraLives *self);
//原型 ira_debug_live_ranges ira-int.h ira-lives.cc
void mtcs_ira_lives_debug_live_ranges (MtcsIraLives *self);
//原型 ira_compress_allocno_live_ranges ira-int.h ira-lives.cc
void mtcs_ira_lives_compress_allocno_live_ranges (MtcsIraLives *self);
//原型 ira_finish_allocno_live_ranges ira-int.h ira-lives.cc
void mtcs_ira_lives_finish_allocno_live_ranges (MtcsIraLives *self);

#endif
