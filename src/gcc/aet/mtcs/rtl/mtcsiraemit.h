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

#ifndef __GCC_MTCS_IRA_EMIT__
#define __GCC_MTCS_IRA_EMIT__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "mtcsiraobject.h"
#include "mtcsiraallocno.h"


typedef struct _EmitMove  EmitMove;

typedef struct _MtcsIraEmit  MtcsIraEmit;
struct _MtcsIraEmit
{
  MtcsComponent parent;
  //原型 ira_allocno_emit_data ira-int.h ira-emit.cc
  MtcsIraEmitData *ira_allocno_emit_data;
  /* Pointers to data allocated for allocnos being created during
     emitting.  Usually there are quite few such allocnos because they
     are created only for resolving loop in register shuffling.  */
  //原型 new_allocno_emit_data_vec ira-emit.cc
  vec<void *> new_allocno_emit_data_vec;

  /* Array of moves (indexed by BB index) which should be put at the
     start/end of the corresponding basic blocks.  */
  //原型   static move_t *at_bb_start, *at_bb_end ira-emit.cc
  EmitMove **at_bb_start, **at_bb_end;

  /* Max regno before renaming some pseudo-registers.  For example, the
     same pseudo-register can be renamed in a loop if its allocation is
     different outside the loop.  */
  //原型   max_regno_before_changing ira-emit.cc
   int max_regno_before_changing;


   /* Bitmap of allocnos local for the current loop.  */
   //原型   local_allocno_bitmap ira-emit.cc
   bitmap local_allocno_bitmap;

   /* This bitmap is used to find that we need to generate and to use a
      new pseudo-register when processing allocnos with the same original
      regno.  */
   //原型   used_regno_bitmap ira-emit.cc
   bitmap used_regno_bitmap;

   /* This bitmap contains regnos of allocnos which were renamed locally
      because the allocnos correspond to disjoint live ranges in loops
      with a common parent.  */
   //原型   renamed_regno_bitmap ira-emit.cc
   bitmap renamed_regno_bitmap;

   /* Last move (in move sequence being processed) setting up the
      corresponding hard register.  */
   //原型   hard_regno_last_set ira-emit.cc
    EmitMove * hard_regno_last_set[MAX_FIRST_PSEUDO_REGISTER/*!FIRST_PSEUDO_REGISTER*/];

   /* If the element value is equal to CURR_TICK then the corresponding
      element in `hard_regno_last_set' is defined and correct.  */
   //原型   hard_regno_last_set_check ira-emit.cc
    int hard_regno_last_set_check[MAX_FIRST_PSEUDO_REGISTER/*!FIRST_PSEUDO_REGISTER*/];

   /* Last move (in move sequence being processed) setting up the
      corresponding allocno.  */
   //原型   allocno_last_set ira-emit.cc
    EmitMove **allocno_last_set;

   /* If the element value is equal to CURR_TICK then the corresponding
      element in . `allocno_last_set' is defined and correct.  */
   //原型   allocno_last_set_check ira-emit.cc
    int *allocno_last_set_check;

   /* Definition of vector of moves.  */

   /* This vec contains moves sorted topologically (depth-first) on their
      dependency graph.  */
   //原型   move_vec ira-emit.cc
    vec<EmitMove *> move_vec;

   /* The variable value is used to check correctness of values of
      elements of arrays `hard_regno_last_set' and
      `allocno_last_set_check'.  */
   //原型   curr_tick ira-emit.cc
    int curr_tick;

};


MtcsIraEmit *mtcs_ira_emit_new(MtcsMode *mtcsMode);
//原型 ira_initiate_emit_data ira-int.h ira-emit.cc
void mtcs_ira_emit_initiate_emit_data (MtcsIraEmit *self);
//原型 ira_finish_emit_data ira-int.h ira-emit.cc
void mtcs_ira_emit_finish_emit_data (MtcsIraEmit *self);
//原型 ira_create_new_reg ira-int.h ira-emit.cc
rtx mtcs_ira_emit_create_new_reg (MtcsIraEmit *self,rtx original_reg);
//原型 ira_emit ira-int.h ira-emit.cc
void mtcs_ira_emit_emit (MtcsIraEmit *self,bool loops_p);

#endif
