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


#ifndef __GCC_MTCS_REORG__
#define __GCC_MTCS_REORG__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "mtcsresource.h"
#include "../mtcspass.h"

/* Counters for delay-slot filling.  */

#define MTCS_NUM_REORG_FUNCTIONS 2
#define MTCS_MAX_DELAY_HISTOGRAM 3
#define MTCS_MAX_REORG_PASSES 2

typedef struct _MtcsReorg MtcsReorg;
struct _MtcsReorg
{
    MtcsComponent parent;
    //原型 unfilled_slots_obstack reorg.cc
    struct obstack unfilled_slots_obstack;
    //原型 unfilled_firstobj reorg.cc
    rtx *unfilled_firstobj;
    /* Points to the label before the end of the function, or before a
       return insn.  */
    //原型 function_return_label reorg.cc

     rtx_code_label *function_return_label;
    /* Likewise for a simple_return.  */
     //原型 function_simple_return_label reorg.cc

     rtx_code_label *function_simple_return_label;
     /* Mapping between INSN_UID's and position in the code since INSN_UID's do
        not always monotonically increase.  */
     //原型 uid_to_ruid reorg.cc
      int *uid_to_ruid;
     /* Highest valid index in `uid_to_ruid'.  */
     //原型 max_uid reorg.cc
      int max_uid;

      int num_insns_needing_delays[MTCS_NUM_REORG_FUNCTIONS][MTCS_MAX_REORG_PASSES];

      int num_filled_delays[MTCS_NUM_REORG_FUNCTIONS][MTCS_MAX_DELAY_HISTOGRAM+1][MTCS_MAX_REORG_PASSES];

      int reorg_pass_number;
      //原型 sibling_labels reorg.cc
      vec <rtx> sibling_labels;


      MtcsResource *mtcsResource;
};

MtcsReorg *mtcs_reorg_new(MtcsMode *mtcsMode);



//原型 NEXT_PASS (pass_machine_reorg, 1);  RTL_PASS reorg.cc  mach   y  有条件执行 targetm.machine_dependent_reorg != 0; targetm.machine_dependent_reorg ();
typedef struct _MtcsPassMach MtcsPassMach;
struct _MtcsPassMach
{
   MtcsPass parent;
};
MtcsPassMach *mtcs_pass_mach_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_delay_slots, 1);  RTL_PASS reorg.cc  dbr   y  有条件执行 DELAY_SLOTS   rest_of_handle_delay_slots
typedef struct _MtcsPassDelaySlots MtcsPassDelaySlots;
struct _MtcsPassDelaySlots
{
   MtcsPass parent;
};
MtcsPassDelaySlots *mtcs_pass_delay_slots_new(MtcsMode *mtcsMode);

#endif

