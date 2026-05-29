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

#ifndef __GCC_MTCS_IRA_ALLOCNO__
#define __GCC_MTCS_IRA_ALLOCNO__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "../mtcsmicro.h"
#include "mtcsiraobject.h"
#include "mtcsiralooptreenode.h"

//原型 struct ira_allocno_copy ira-int.h
typedef struct _MtcsIraAllocnoCopy  MtcsIraAllocnoCopy;
//原型 ira_allocno_pref ira-int.h ira_pref_t
typedef struct _MtcsIraAllocnoPref  MtcsIraAllocnoPref;
//原型 ira_emit_data ira-int.h
typedef struct _MtcsIraEmitData MtcsIraEmitData;

/* A structure representing an allocno (allocation entity).  Allocno
   represents a pseudo-register in an allocation region.  If
   pseudo-register does not live in a region but it lives in the
   nested regions, it is represented in the region by special allocno
   called *cap*.  There may be more one cap representing the same
   pseudo-register in region.  It means that the corresponding
   pseudo-register lives in more one non-intersected subregion.  */
typedef struct _MtcsIraAllocno  MtcsIraAllocno;
struct _MtcsIraAllocno
{
   MtcsComponent parent;

  /* The allocno order number starting with 0.  Each allocno has an
     unique number and the number is never changed for the
     allocno.  */
  int num;
  /* Regno for allocno or cap.  */
  int regno;
  /* Mode of the allocno which is the mode of the corresponding
     pseudo-register.  */
  ENUM_BITFIELD (machine_mode) mode : MACHINE_MODE_BITSIZE;
  /* Widest mode of the allocno which in at least one case could be
     for paradoxical subregs where wmode > mode.  */
  ENUM_BITFIELD (machine_mode) wmode : MACHINE_MODE_BITSIZE;
  /* Register class which should be used for allocation for given
     allocno.  NO_REGS means that we should use memory.  */
  ENUM_BITFIELD (reg_class) aclass : 16;
  /* Hard register assigned to given allocno.  Negative value means
     that memory was allocated to the allocno.  During the reload,
     spilled allocno has value equal to the corresponding stack slot
     number (0, ...) - 2.  Value -1 is used for allocnos spilled by the
     reload (at this point pseudo-register has only one allocno) which
     did not get stack slot yet.  */
  signed int hard_regno : 16;
  /* A bitmask of the ABIs used by calls that occur while the allocno
     is live.  */
  unsigned int crossed_calls_abis : MTCS_NUM_ABI_IDS;
  /* During the reload, value TRUE means that we should not reassign a
     hard register to the allocno got memory earlier.  It is set up
     when we removed memory-memory move insn before each iteration of
     the reload.  */
  unsigned int dont_reassign_p : 1;
//#ifdef STACK_REGS
  /* Set to TRUE if allocno can't be assigned to the stack hard
     register correspondingly in this region and area including the
     region and all its subregions recursively.  */
  unsigned int no_stack_reg_p : 1, total_no_stack_reg_p : 1;
//#endif
  /* TRUE value means that there is no sense to spill the allocno
     during coloring because the spill will result in additional
     reloads in reload pass.  */
  unsigned int bad_spill_p : 1;
  /* TRUE if a hard register or memory has been assigned to the
     allocno.  */
  unsigned int assigned_p : 1;
  /* TRUE if conflicts for given allocno are represented by vector of
     pointers to the conflicting allocnos.  Otherwise, we use a bit
     vector where a bit with given index represents allocno with the
     same number.  */
  unsigned int conflict_vec_p : 1;
  /* True if the parent loop has an allocno for the same register and
     if the parent allocno's assignment might not be valid in this loop.
     This means that we cannot merge this allocno and the parent allocno
     together.

     This is only ever true for non-cap allocnos.  */
  unsigned int might_conflict_with_parent_p : 1;
//#ifndef NUM_REGISTER_FILTERS
//#error "insn-config.h not included"
//#elif NUM_REGISTER_FILTERS
  /* The set of register filters applied to the allocno by operand
     alternatives that accept class ACLASS.  */
  unsigned int register_filters  ;
//#endif
  /* Accumulated usage references of the allocno.  Here and below,
     word 'accumulated' means info for given region and all nested
     subregions.  In this case, 'accumulated' means sum of references
     of the corresponding pseudo-register in this region and in all
     nested subregions recursively. */
  int nrefs;
  /* Accumulated frequency of usage of the allocno.  */
  int freq;
  /* Minimal accumulated and updated costs of usage register of the
     allocno class.  */
  int class_cost, updated_class_cost;
  /* Minimal accumulated, and updated costs of memory for the allocno.
     At the allocation start, the original and updated costs are
     equal.  The updated cost may be changed after finishing
     allocation in a region and starting allocation in a subregion.
     The change reflects the cost of spill/restore code on the
     subregion border if we assign memory to the pseudo in the
     subregion.  */
  int memory_cost, updated_memory_cost;
  /* Accumulated number of points where the allocno lives and there is
     excess pressure for its class.  Excess pressure for a register
     class at some point means that there are more allocnos of given
     register class living at the point than number of hard-registers
     of the class available for the allocation.  */
  int excess_pressure_points_num;
  /* The number of objects tracked in the following array.  */
  int num_objects;
  /* Accumulated frequency of calls which given allocno
     intersects.  */
  int call_freq;
  /* Accumulated number of the intersected calls.  */
  int calls_crossed_num;
  /* The number of calls across which it is live, but which should not
     affect register preferences.  */
  int cheap_calls_crossed_num;
  /* Allocnos with the same regno are linked by the following member.
     Allocnos corresponding to inner loops are first in the list (it
     corresponds to depth-first traverse of the loops).  */
  MtcsIraAllocno *next_regno_allocno;
  /* There may be different allocnos with the same regno in different
     regions.  Allocnos are bound to the corresponding loop tree node.
     Pseudo-register may have only one regular allocno with given loop
     tree node but more than one cap (see comments above).  */
  MtcsIraLoopTreeNode *loop_tree_node;
  /* Allocno hard reg preferences.  */
  MtcsIraAllocnoPref *allocno_prefs;
  /* Copies to other non-conflicting allocnos.  The copies can
     represent move insn or potential move insn usually because of two
     operand insn constraints.  */
  MtcsIraAllocnoCopy *allocno_copies;
  /* It is a allocno (cap) representing given allocno on upper loop tree
     level.  */
  MtcsIraAllocno *cap;
  /* It is a link to allocno (cap) on lower loop level represented by
     given cap.  Null if given allocno is not a cap.  */
  MtcsIraAllocno *cap_member;
  /* An array of structures describing conflict information and live
     ranges for each object associated with the allocno.  There may be
     more than one such object in cases where the allocno represents a
     multi-word register.  */
  MtcsIraObject *objects[2];
  /* Registers clobbered by intersected calls.  */
   HardRegSet /*!HARD_REG_SET*/ crossed_calls_clobbered_regs;
  /* Array of usage costs (accumulated and the one updated during
     coloring) for each hard register of the allocno class.  The
     member value can be NULL if all costs are the same and equal to
     CLASS_COST.  For example, the costs of two different hard
     registers can be different if one hard register is callee-saved
     and another one is callee-used and the allocno lives through
     calls.  Another example can be case when for some insn the
     corresponding pseudo-register value should be put in specific
     register class (e.g. AREG for x86) which is a strict subset of
     the allocno class (GENERAL_REGS for x86).  We have updated costs
     to reflect the situation when the usage cost of a hard register
     is decreased because the allocno is connected to another allocno
     by a copy and the another allocno has been assigned to the hard
     register.  */
  int *hard_reg_costs, *updated_hard_reg_costs;
  /* Array of decreasing costs (accumulated and the one updated during
     coloring) for allocnos conflicting with given allocno for hard
     regno of the allocno class.  The member value can be NULL if all
     costs are the same.  These costs are used to reflect preferences
     of other allocnos not assigned yet during assigning to given
     allocno.  */
  int *conflict_hard_reg_costs, *updated_conflict_hard_reg_costs;
  /* Different additional data.  It is used to decrease size of
     allocno data footprint.  */
  void *add_data;
};

/* The following structure represents a copy of two allocnos.  The
   copies represent move insns or potential move insns usually because
   of two operand insn constraints.  To remove register shuffle, we
   also create copies between allocno which is output of an insn and
   allocno becoming dead in the insn.  */
//原型 struct ira_allocno_copy ira-int.h
struct _MtcsIraAllocnoCopy
{
  /* The unique order number of the copy node starting with 0.  */
  int num;
  /* Allocnos connected by the copy.  The first allocno should have
     smaller order number than the second one.  */
  MtcsIraAllocno *first, *second;
  /* Execution frequency of the copy.  */
  int freq;
  bool constraint_p;
  /* It is a move insn which is an origin of the copy.  The member
     value for the copy representing two operand insn constraints or
     for the copy created to remove register shuffle is NULL.  In last
     case the copy frequency is smaller than the corresponding insn
     execution frequency.  */
  rtx_insn *insn;
  /* All copies with the same allocno as FIRST are linked by the two
     following members.  */
  MtcsIraAllocnoCopy *prev_first_allocno_copy, *next_first_allocno_copy;
  /* All copies with the same allocno as SECOND are linked by the two
     following members.  */
  MtcsIraAllocnoCopy *prev_second_allocno_copy, *next_second_allocno_copy;
  /* Region from which given copy is originated.  */
  MtcsIraLoopTreeNode *loop_tree_node;
};

/* The following structure represents a hard register preference of
   allocno.  The preference represent move insns or potential move
   insns usually because of two operand insn constraints.  One move
   operand is a hard register.  */
//原型 ira_allocno_pref ira-int.h
struct _MtcsIraAllocnoPref
{
  /* The unique order number of the preference node starting with 0.  */
  int num;
  /* Preferred hard register.  */
  int hard_regno;
  /* Accumulated execution frequency of insns from which the
     preference created.  */
  int freq;
  /* Given allocno.  */
  MtcsIraAllocno *allocno;
  /* All preferences with the same allocno are linked by the following
     member.  */
  MtcsIraAllocnoPref *next_pref;
};



/* Allocno bound data used for emit pseudo live range split insns and
   to flattening IR.  */
struct _MtcsIraEmitData
{
  /* TRUE if the allocno assigned to memory was a destination of
     removed move (see ira-emit.cc) at loop exit because the value of
     the corresponding pseudo-register is not changed inside the
     loop.  */
  unsigned int mem_optimized_dest_p : 1;
  /* TRUE if the corresponding pseudo-register has disjoint live
     ranges and the other allocnos of the pseudo-register except this
     one changed REG.  */
  unsigned int somewhere_renamed_p : 1;
  /* TRUE if allocno with the same REGNO in a subregion has been
     renamed, in other words, got a new pseudo-register.  */
  unsigned int child_renamed_p : 1;
  /* Final rtx representation of the allocno.  */
  rtx reg;
  /* Non NULL if we remove restoring value from given allocno to
     MEM_OPTIMIZED_DEST at loop exit (see ira-emit.cc) because the
     allocno value is not changed inside the loop.  */
  MtcsIraAllocno * mem_optimized_dest;
};

MtcsIraAllocno *mtcs_ira_allocno_new(MtcsMode *mtcsMode,int regn);
//原型 ALLOCNO_SET_REGISTER_FILTERS ira-int.h
void mtcs_ira_allocno_set_register_filter(MtcsIraAllocno *self,int filter);
//原型 ior_hard_reg_conflicts ira-int.h ira-build.cc
void mtcs_ira_allocno_ior_hard_reg_conflicts (MtcsIraAllocno *self, HardRegSet set);
//原型 ira_print_expanded_allocno ira-int.h ira-build.cc
void mtcs_ira_allocno_print_expanded_allocno (MtcsIraAllocno *self);
//原型 ira_need_caller_save_regs ira-int.h
HardRegSet mtcs_ira_allocno_need_caller_save_regs (MtcsIraAllocno *self);
//原型 ira_need_caller_save_p ira-int.h
bool mtcs_ira_allocno_need_caller_save_p (MtcsIraAllocno *self, unsigned int regno);
//原型 static ira_pref_t find_allocno_pref (ira_allocno_t a, int hard_regno) ira-build.cc
MtcsIraAllocnoPref *mtcs_ira_allocno_find_allocno_pref (MtcsIraAllocno *self, int hard_regno);
//原型 ira_debug_allocno_prefs ira-int.h ira-build.cc
void mtcs_ira_allocno_debug_allocno_prefs (MtcsIraAllocno *self);
//原型 static ira_copy_t find_allocno_copy (ira_allocno_t a1, i ... ira-build.cc
MtcsIraAllocnoCopy *mtcs_ira_allocno_find_allocno_copy (MtcsIraAllocno * self, MtcsIraAllocno *a2, rtx_insn *insn,
      MtcsIraLoopTreeNode *loop_tree_node);
//原型 static void print_allocno_copies (FILE *f, ira_allocno_t a) ira-build.cc
void mtcs_ira_allocno_print_allocno_copies (MtcsIraAllocno *self,FILE *f);
//原型 debug ira-int.h ira-build.cc
void mtcs_ira_allocno_debug (MtcsIraAllocno *self);
//原型 ira_debug_allocno_copies ira-int.h ira-build.cc
void mtcs_ira_allocno_debug_allocno_copies (MtcsIraAllocno *self);
//原型 extern ira_allocno_t ira_parent_allocno (ira_allocno_t); ira-int.h ira-build.cc
MtcsIraAllocno *mtcs_ira_allocno_parent_allocno (MtcsIraAllocno * self);
//原型 ira_allocno_t ira_parent_or_cap_allocno (ira_allocno_t);
MtcsIraAllocno * mtcs_ira_allocno_parent_or_cap_allocno (MtcsIraAllocno * self);
//原型 allocno_emit_reg ira-int.h
rtx mtcs_ira_allocno_emit_reg (MtcsIraAllocno *self);
MtcsIraAllocnoPref *mtcs_ira_allocno_pref_new();


/* The iterator for objects associated with an allocno.  */
//原型 ira_allocno_object_iterator ira-int.h
typedef struct _MtcsIraAllocnoObjectIterator {
  /* The number of the element the allocno's object array.  */
  int n;
}MtcsIraAllocnoObjectIterator;

/* Initialize the iterator I.  */
//原型 ira_allocno_object_iter_init ira-int.h
inline void mtcs_ira_allocno_object_iter_init (MtcsIraAllocnoObjectIterator *i)
{
  i->n = 0;
}

/* Return TRUE if we have more objects to visit in allocno A, in which
   case *O is set to the object to be visited.  Otherwise, return
   FALSE.  */
//原型 ira_allocno_object_iter_cond ira-int.h
inline bool mtcs_ira_allocno_object_iter_cond (MtcsIraAllocnoObjectIterator *i, MtcsIraAllocno *a,MtcsIraObject **o)
{
   int n = i->n++;
   if (n < a->num_objects){
      *o =a->objects[n];
      return true;
   }
   return false;
}

/* Loop over all objects associated with allocno A.  In each
   iteration, O is set to the next object.  ITER is an instance of
   ira_allocno_object_iterator used to iterate the conflicts.  */
//原型 FOR_EACH_ALLOCNO_OBJECT ira-int.h
#define MTCS_FOR_EACH_ALLOCNO_OBJECT(A, O, ITER)        \
  for (mtcs_ira_allocno_object_iter_init (&(ITER));        \
       mtcs_ira_allocno_object_iter_cond (&(ITER), (A), &(O));)

//原型 #define ALLOCNO_EMIT_DATA(a) ((ira_emit_data_t) ALLOCNO_ADD_DATA (a))
#define MTCS_ALLOCNO_EMIT_DATA(a) ((MtcsIraEmitData *) a->add_data)

/* Return the set of all hard registers that conflict with A.  */
//原型 ira_total_conflict_hard_regs ira-int.h
inline HardRegSet mtcs_ira_allocno_total_conflict_hard_regs (MtcsIraAllocno *self)
{
  auto obj_0 = self->objects[0];
  HardRegSet conflicts = obj_0->total_conflict_hard_regs;
  for (int i = 1; i < self->num_objects; i++)
    conflicts |= self->objects[i]->total_conflict_hard_regs;
  return conflicts;
}

MtcsIraAllocnoPref *mtcs_ira_allocno_pref_new();
//原型 static void add_allocno_pref_to_list (ira_pref_t pref) ira-build.cc
void mtcs_ira_allocno_add_allocno_pref_to_list (MtcsIraAllocnoPref *pref);
//原型 static void print_pref (FILE *f, ira_pref_t pref) ira-build.cc
void mtcs_ira_allocno_print_pref (MtcsIraAllocnoPref *pref,FILE *f);
/* Print info about PREF into stderr.  */
//原型 ira_debug_pref ira-int.h ira-build.cc
void mtcs_ira_allocno_debug_pref (MtcsIraAllocnoPref *pref);
//原型   pref_pool.remove (pref); ira-build.cc
void mtcs_ira_allocno_pref_free (MtcsIraAllocnoPref *pref);

////--------------以下是 MtcsIraAllocnoCopy-----------------

//原型 static void swap_allocno_copy_ends_if_necessary (ira_copy_t cp) ira-build.cc
void mtcs_ira_allocno_swap_allocno_copy_ends_if_necessary (MtcsIraAllocnoCopy *mtcsIraAllocnoCopy);
//原型 static void add_allocno_copy_to_list (MtcsIraAllocnoCopy *cp) ira-build.cc
void mtcs_ira_allocno_add_allocno_copy_to_list (MtcsIraAllocnoCopy *mtcsIraAllocnoCopy);
//原型 static void print_copy (MtcsIraAllocnoCopy *mtcsIraAllocnoCopy,FILE *f) ira-build.cc
void mtcs_ira_allocno_print_copy (MtcsIraAllocnoCopy *mtcsIraAllocnoCopy,FILE *f);
//原型 DEBUG_FUNCTION void debug (ira_allocno_copy &ref) ira-int.h ira-build.cc
void mtcs_ira_alloc_copy_debug (MtcsIraAllocnoCopy *mtcsIraAllocnoCopy);
//原型 DEBUG_FUNCTION void debug (ira_allocno_copy *ptr) ira-int.h ira-build.cc
void mtcs_ira_alloc_copy_debug_1(MtcsIraAllocnoCopy *mtcsIraAllocnoCopy);
//原型 ira_debug_copy ira-int.h ira-build.cc
void mtcs_ira_allocno_debug_copy (MtcsIraAllocnoCopy *mtcsIraAllocnoCopy);
//原型   copy_pool.remove (cp);; ira-build.cc
void mtcs_ira_allocno_copy_free (MtcsIraAllocnoCopy *mtcsIraAllocnoCopy);
MtcsIraAllocnoCopy *mtcs_ira_allocno_copy_new();

#endif
