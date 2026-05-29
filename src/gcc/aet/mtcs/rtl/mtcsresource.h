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


#ifndef __GCC_MTCS_RESOURCE__
#define __GCC_MTCS_RESOURCE__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "../mtcsmicro.h"
#include "resource.h"

/* The resources used by a given insn.  */
//原型 struct resource resource.h
typedef struct _ReorgResources
{
  char memory;    /* Insn sets or needs a memory location.  */
  char volatil;      /* Insn sets or needs a volatile memory loc.  */
  char cc;     /* Insn sets or needs the condition codes.  */
  HardRegSet regs; /* Which registers are set or needed.  */
}ReorgResources;

/* This structure is used to record liveness information at the targets or
   fallthrough insns of branches.  We will most likely need the information
   at targets again, so save them in a hash table rather than recomputing them
   each time.  */
//原型 struct target_info resource.cc
typedef struct _ReorgTargetInfo ReorgTargetInfo;
struct _ReorgTargetInfo
{
  int uid;        /* INSN_UID of target.  */
  ReorgTargetInfo *next;   /* Next info for same hash bucket.  */
  HardRegSet live_regs;  /* Registers live at target.  */
  int block;         /* Basic block number containing target.  */
  int bb_tick;       /* Generation count of basic block info.  */
};

typedef struct _MtcsResource MtcsResource;
struct _MtcsResource
{
     MtcsComponent parent;

     /* Indicates what resources are required at the beginning of the epilogue.  */
    //原型 start_of_epilogue_needs  resource.cc
      ReorgResources start_of_epilogue_needs;

    /* Indicates what resources are required at function end.  */
      ReorgResources end_of_function_needs;

    /* Define the hash table itself.  */
      ReorgTargetInfo **target_hash_table ;

    /* For each basic block, we maintain a generation number of its basic
       block info, which is updated each time we move an insn from the
       target of a jump.  This is the generation number indexed by block
       number.  */

     int *bb_ticks;

    /* Marks registers possibly live at the current place being scanned by
       mark_target_live_regs.  Also used by update_live_status.  */

     HardRegSet current_live_regs;

    /* Marks registers for which we have seen a REG_DEAD note but no assignment.
       Also only used by the next two functions.  */

     HardRegSet pending_dead_regs;

};

MtcsResource *mtcs_resource_new(MtcsMode *mtcsMode);
//原型 mark_end_of_function_resources resource.h resource.cc
void mtcs_resource_mark_end_of_function_resources (MtcsResource *self,rtx trial, bool include_delayed_effects);
//原型 incr_ticks_for_insn resource.h resource.cc
void mtcs_resource_incr_ticks_for_insn (MtcsResource *self,rtx_insn *insn);
//原型 clear_hashed_info_until_next_barrier resource.h resource.cc
void  mtcs_resource_clear_hashed_info_until_next_barrier (MtcsResource *self,rtx_insn *insn);
//原型 mark_end_of_function_resources resource.h resource.cc
void mtcs_resource_clear_hashed_info_for_insn (MtcsResource *self,rtx_insn *insn);
//原型 free_resource_info resource.h resource.cc
void mtcs_resource_free_resource_info (MtcsResource *self);
//原型 mark_referenced_resources resource.h resource.cc
void mtcs_resource_mark_referenced_resources (MtcsResource *self,rtx x, ReorgResources/*!struct resources*/ *res,
            bool include_delayed_effects);
//原型 mark_set_resources resource.h resource.cc
void mtcs_resource_mark_set_resources (MtcsResource *self,rtx x, ReorgResources/*!struct resources*/ *res, int in_dest,
          enum mark_resource_type mark_type);
//原型 mark_target_live_regs resource.h resource.cc
void mtcs_resource_mark_target_live_regs (MtcsResource *self,rtx_insn *insns,
      rtx target_maybe_return,ReorgResources/*!struct resources*/ *res);
//原型 init_resource_info resource.h resource.cc
void mtcs_resource_init_resource_info (MtcsResource *self,rtx_insn *epilogue_insn);

//原型 #define CLEAR_RESOURCE(RES)   \
// do { (RES)->memory = (RES)->volatil = (RES)->cc = 0; \
//      CLEAR_HARD_REG_SET ((RES)->regs); } while (0)
void mtcs_resources_clean(ReorgResources *res);



#endif

