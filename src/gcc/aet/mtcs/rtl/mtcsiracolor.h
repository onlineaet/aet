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
#ifndef __GCC_MTCS_IRA_COLOR__
#define __GCC_MTCS_IRA_COLOR__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "../mtcsmicro.h"
#include "mtcsiraallocno.h"

//原型 allocno_hard_regs
typedef struct _AllocnoHardRegs  AllocnoHardRegs;
//原型 allocno_hard_regs_node
typedef struct _AllocnoHardRegsNode AllocnoHardRegsNode;
//原型 struct allocno_color_data
typedef struct _AllocnoColorData  AllocnoColorData;

struct allocno_hard_regs_hasher;
//原型 struct allocno_hard_regs_subnode
typedef struct _AllocnoHardRegsSubnode AllocnoHardRegsSubnode;
//原型 struct update_cost_queue_elem
typedef struct _UpdateCostQueueElem  UpdateCostQueueElem;
//原型 coalesce_data
typedef struct _CoalesceData  CoalesceData ;


typedef struct _MtcsIraColor  MtcsIraColor;
struct _MtcsIraColor
{
  MtcsComponent parent;
  /* The number of elements in the following array.  */
  //原型 ira_spilled_reg_stack_slots_num ira-int.h ira.cc
  int ira_spilled_reg_stack_slots_num;
  /* The following array contains info about spilled pseudo-registers
     stack slots used in current function so far.  */
  //原型 ira_spilled_reg_stack_slot ira-int.h ira.cc
  class ira_spilled_reg_stack_slot *ira_spilled_reg_stack_slots;
  /* Container for storing allocno data concerning coloring.  */
  //原型 allocno_color_data_t allocno_color_data ira-color.cc
  AllocnoColorData * allocno_color_data;
  /* Used for finding allocno colorability to exclude repeated allocno
     processing and for updating preferencing to exclude repeated
     allocno processing during assignment.  */
  //原型 curr_allocno_process ira-color.cc
  int curr_allocno_process;

  /* Bitmap of allocnos which should be colored.  */
  //原型 coloring_allocno_bitmap ira-color.cc
  bitmap coloring_allocno_bitmap;

  /* Bitmap of allocnos which should be taken into account during
     coloring.  In general case it contains allocnos from
     coloring_allocno_bitmap plus other already colored conflicting
     allocnos.  */
  //原型 consideration_allocno_bitmap ira-color.cc
  bitmap consideration_allocno_bitmap;

  /* All allocnos sorted according their priorities.  */
  //原型 sorted_allocnos ira-color.cc
   MtcsIraAllocno **sorted_allocnos;

  /* Vec representing the stack of allocnos used during coloring.  */
  //原型 allocno_stack_vec ira-color.cc
   vec<MtcsIraAllocno *> allocno_stack_vec;

   /* Vector of unique allocno hard registers.  */
   //原型 allocno_hard_regs_vec ira-color.cc
   vec<AllocnoHardRegs *> allocno_hard_regs_vec;

   /* Hash table of unique allocno hard registers.  */
   //原型 allocno_hard_regs_htab ira-color.cc
   hash_table<allocno_hard_regs_hasher> *allocno_hard_regs_htab;

   /* Used for finding a common ancestor of two allocno hard registers
      nodes in the forest.  We use the current value of
      'node_check_tick' to mark all nodes from one node to the top and
      then walking up from another node until we find a marked node.

      It is also used to figure out allocno colorability as a mark that
      we already reset value of member 'conflict_size' for the forest
      node corresponding to the processed allocno.  */
   //原型 node_check_tick ira-color.cc
    int node_check_tick;

   /* Roots of the forest containing hard register sets can be assigned
      to allocnos.  */
   //原型 hard_regs_roots ira-color.cc
   AllocnoHardRegsNode *hard_regs_roots;

   /* Definition of vector of allocno hard register nodes.  */
   /* Vector used to create the forest.  */
   //原型 hard_regs_node_vec ira-color.cc
   vec<AllocnoHardRegsNode *> hard_regs_node_vec;

   /* Number of allocno hard registers nodes in the forest.  */
   //原型 allocno_hard_regs_nodes_num ira-color.cc
    int allocno_hard_regs_nodes_num;

   /* Table preorder number of allocno hard registers node in the forest
      -> the allocno hard registers node.  */
   //原型 allocno_hard_regs_nodes ira-color.cc
    AllocnoHardRegsNode **allocno_hard_regs_nodes;

    /* Container for hard regs subnodes of all allocnos.  */
    //原型 allocno_hard_regs_subnodes ira-color.cc
    AllocnoHardRegsSubnode *allocno_hard_regs_subnodes;

    /* Table (preorder number of allocno hard registers node in the
       forest, preorder number of allocno hard registers subnode) -> index
       of the subnode relative to the node.  -1 if it is not a
       subnode.  */
    //原型 allocno_hard_regs_subnode_index ira-color.cc
    int *allocno_hard_regs_subnode_index;

    /* Array whose element value is TRUE if the corresponding hard
       register was already allocated for an allocno.  */
    //原型 allocated_hardreg_p ira-color.cc
    bool allocated_hardreg_p[MAX_FIRST_PSEUDO_REGISTER/*!FIRST_PSEUDO_REGISTER 足够大*/];

    /* The first element in a queue of allocnos whose copy costs need to be
       updated.  Null if the queue is empty.  */
    //原型 update_cost_queue ira-color.cc
     MtcsIraAllocno * update_cost_queue;

    /* The last element in the queue described by update_cost_queue.
       Not valid if update_cost_queue is null.  */
    //原型 update_cost_queue_tail ira-color.cc
     UpdateCostQueueElem *update_cost_queue_tail;

    /* A pool of elements in the queue described by update_cost_queue.
       Elements are indexed by ALLOCNO_NUM.  */
    //原型 update_cost_queue_elems ira-color.cc
     UpdateCostQueueElem *update_cost_queue_elems;

    /* The current value of update_costs_from_copies call count.  */
    //原型 update_cost_check ira-color.cc
     int update_cost_check;

     /* An array used to sort copies.  */
     //原型 sorted_copies ira-color.cc
     MtcsIraAllocnoCopy **sorted_copies;


     /* This page contains the allocator based on the Chaitin-Briggs algorithm.  */

     /* Bucket of allocnos that can colored currently without spilling.  */
     //原型 colorable_allocno_bucket ira-color.cc
      MtcsIraAllocno * colorable_allocno_bucket;

     /* Bucket of allocnos that might be not colored currently without
        spilling.  */
      //原型 uncolorable_allocno_bucket ira-color.cc
      MtcsIraAllocno * uncolorable_allocno_bucket;

     /* The current number of allocnos in the uncolorable_bucket.  */
     //原型 uncolorable_allocnos_num ira-color.cc
      int uncolorable_allocnos_num;

      /* Map: allocno number -> allocno priority.  */
      //原型 allocno_priorities ira-color.cc
      int *allocno_priorities;

      /* TRUE if we coalesced some allocnos.  In other words, if we got
         loops formed by members first_coalesced_allocno and
         next_coalesced_allocno containing more one allocno.  */
      //原型 allocno_coalesced_p ira-color.cc
       bool allocno_coalesced_p;

      /* Bitmap used to prevent a repeated allocno processing because of
         coalescing.  */
      //原型 processed_coalesced_allocno_bitmap ira-color.cc
      bitmap processed_coalesced_allocno_bitmap;

      /* Container for storing allocno data concerning coalescing.  */
      //原型 allocno_coalesce_data ira-color.cc
       CoalesceData * allocno_coalesce_data;
       /* Usage cost and order number of coalesced allocno set to which
          given pseudo register belongs to.  */
      //原型 regno_coalesced_allocno_cost regno_coalesced_allocno_num ira-color.cc
        int *regno_coalesced_allocno_cost;
        int *regno_coalesced_allocno_num;

        /* Widest width in which each pseudo reg is referred to (via subreg).
           It is used for sorting pseudo registers.  */
        //原型 regno_max_ref_mode  ira-color.cc
        machine_mode *regno_max_ref_mode;

        /* Array of live ranges of size IRA_ALLOCNOS_NUM.  Live range for
           given slot contains live ranges of coalesced allocnos assigned to
           given slot.  */
        //原型 slot_coalesced_allocnos_live_ranges ira-color.cc
        MtcsLiveRange **slot_coalesced_allocnos_live_ranges;

        /* To prevent soft conflict detection becoming quadratic in the
           loop depth.  Only for very pathological cases, so it hardly
           seems worth a --param.  */
        //原型 max_soft_conflict_loop_depth ira-color.cc
        int max_soft_conflict_loop_depth;// = 64;
};


MtcsIraColor *mtcs_ira_color_new(MtcsMode *mtcsMode);
//原型 debug_hard_reg_set sel-sched-dump.h ira-color.cc
void mtcs_ira_color_debug_hard_reg_set (MtcsIraColor *self,HardRegSet *set);
//原型 ira_debug_hard_regs_forest ira-int.h ira-color.cc
void mtcs_ira_color_debug_hard_regs_forest (MtcsIraColor *self);
//原型 ira_soft_conflict ira-int.h ira-color.cc
MtcsIraAllocno *mtcs_ira_color_soft_conflict (MtcsIraColor *self,MtcsIraAllocno * a1, MtcsIraAllocno * a2);
//原型 ira_loop_edge_freq ira-int.h ira-color.cc
int mtcs_ira_color_loop_edge_freq (MtcsIraColor *self,MtcsIraLoopTreeNode *loop_node, int regno, bool exit_p);
//原型 ira_reassign_conflict_allocnos ira-int.h ira-color.cc
void mtcs_ira_color_reassign_conflict_allocnos (MtcsIraColor *self,int start_regno);
//原型 ira_sort_regnos_for_alter_reg ira.h ira-color.cc
void mtcs_ira_color_sort_regnos_for_alter_reg (MtcsIraColor *self,int *pseudo_regnos, int n,
                machine_mode *reg_max_ref_mode);
//原型 ira_mark_allocation_change ira.h ira-color.cc
void mtcs_ira_color_mark_allocation_change (MtcsIraColor *self,int regno);
//原型 ira_mark_memory_move_deletion ira.h ira-color.cc
void mtcs_ira_color_mark_memory_move_deletion (MtcsIraColor *self,int dst_regno, int src_regno);
//原型 ira_reassign_pseudos ira.h ira-color.cc
bool mtcs_ira_color_reassign_pseudos (MtcsIraColor *self,int *spilled_pseudo_regs, int num,
            HardRegSet bad_spill_regs,
            HardRegSet *pseudo_forbidden_regs,
            HardRegSet *pseudo_previous_regs,
            bitmap spilled);
//原型 ira_reuse_stack_slot ira-h ira-color.cc
rtx mtcs_ira_color_reuse_stack_slot (MtcsIraColor *self,int regno, poly_uint64 inherent_size,
            poly_uint64 total_size);
//原型 ira_mark_new_stack_slot ira.h ira-color.cc
void mtcs_ira_color_mark_new_stack_slot (MtcsIraColor *self,rtx x, int regno, poly_uint64 total_size);
//原型 ira_better_spill_reload_regno_p ira.h ira-color.cc
bool mtcs_ira_color_better_spill_reload_regno_p (MtcsIraColor *self,int *regnos, int *other_regnos,
             rtx in, rtx out, rtx_insn *insn);
//原型 ira_finish_assign ira-int.h ira-color.cc
void mtcs_ira_color_finish_assign (MtcsIraColor *self);
//原型 ira_initiate_assign ira-int.h ira-color.cc
void mtcs_ira_color_initiate_assign (MtcsIraColor *self);
//原型 ira_color ira-int.h ira-color.cc
void mtc_ira_color_color (MtcsIraColor *self);

#endif
