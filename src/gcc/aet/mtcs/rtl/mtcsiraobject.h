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

#ifndef __GCC_MTCS_IRA_OBJECT__
#define __GCC_MTCS_IRA_OBJECT__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "../mtcsmicro.h"

//原型 live_range ira-int.h
typedef struct _MtcsLiveRange  MtcsLiveRange;
//原型 ira_object ira-int.h
typedef struct _MtcsIraObject  MtcsIraObject;

/* A structure representing conflict information for an allocno
   (or one of its subwords).  */
struct _MtcsIraObject
{
  MtcsComponent parent;
  /* The allocno associated with this record.  */
  void /*!MtcsIraAllocno*/ *allocno; //改为 void * 避免与MtcsIraAllocno 相互引用 通过 MTCS_IRA_OBJECT_ALLOCNO 访问
  /* Vector of accumulated conflicting conflict_redords with NULL end
     marker (if OBJECT_CONFLICT_VEC_P is true) or conflict bit vector
     otherwise.  */
  void *conflicts_array;
  /* Pointer to structures describing at what program point the
     object lives.  We always maintain the list in such way that *the
     ranges in the list are not intersected and ordered by decreasing
     their program points*.  */
  MtcsLiveRange * live_ranges;
  /* The subword within ALLOCNO which is represented by this object.
     Zero means the lowest-order subword (or the entire allocno in case
     it is not being tracked in subwords).  */
  int subword;
  /* Allocated size of the conflicts array.  */
  unsigned int conflicts_array_size;
  /* A unique number for every instance of this structure, which is used
     to represent it in conflict bit vectors.  */
  int id;
  /* Before building conflicts, MIN and MAX are initialized to
     correspondingly minimal and maximal points of the accumulated
     live ranges.  Afterwards, they hold the minimal and maximal ids
     of other ira_objects that this one can conflict with.  */
  int min, max;
  /* Initial and accumulated hard registers conflicting with this
     object and as a consequences cannot be assigned to the allocno.
     All non-allocatable hard regs and hard regs of register classes
     different from given allocno one are included in the sets.  */
  HardRegSet /*!HARD_REG_SET*/ conflict_hard_regs, total_conflict_hard_regs;
  /* Number of accumulated conflicts in the vector of conflicting
     objects.  */
  int num_accumulated_conflicts;
  /* TRUE if conflicts are represented by a vector of pointers to
     ira_object structures.  Otherwise, we use a bit vector indexed
     by conflict ID numbers.  */
  unsigned int conflict_vec_p : 1;
};

/* The structure describes program points where a given allocno lives.
   If the live ranges of two allocnos are intersected, the allocnos
   are in conflict.  */
//原型 live_range ira-int.h 是否变成类iralive的成员
struct _MtcsLiveRange
{
  /* Object whose live range is described by given structure.  */
  MtcsIraObject *object;
  /* Program point range.  */
  int start, finish;
  /* Next structure describing program points where the allocno
     lives.  */
  MtcsLiveRange * next;
  /* Pointer to structures with the same start/finish.  */
  MtcsLiveRange * start_next, *finish_next;
};


MtcsIraObject *mtcs_ira_object_new(MtcsMode *mtcsMode,int subword);
//原型 object_pool.remove (obj);
void mtcs_ira_object_free(MtcsIraObject *self);
//原型 ira_conflict_vector_profitable_p ira-int.h ira-build.cc
bool mtcs_ira_object_ira_conflict_vector_profitable_p (MtcsIraObject *self, int num);
//原型 ira_allocate_conflict_vec ira-int.h ira-build.cc
void mtcs_ira_object_ira_allocate_conflict_vec (MtcsIraObject *self, int num);
//原型 ira_allocate_object_conflicts ira-int.h ira-build.cc
void mtcs_ira_object_allocate_object_conflicts (MtcsIraObject *self, int num);
//原型 static void ira_add_conflict ira-build.cc 改为公共方法 供mtcsirabuild 的方法 ira_flattening调用
void mtcs_ira_object_add_conflict (MtcsIraObject *self, MtcsIraObject *obj2);
//原型 static void clear_conflicts ira-build.cc 改为公共方法 供mtcsirabuild 的方法 ira_flattening调用
void mtcs_ira_object_clear_conflicts (MtcsIraObject *self);

//原型 ira_create_live_range ira-int.h ira-build.cc
MtcsLiveRange * mtcs_ira_object_create_live_range (MtcsIraObject *self, int start, int finish,MtcsLiveRange *next);
//原型 ira_create_live_range ira-int.h ira-build.cc
void mtcs_ira_object_add_live_range_to_object (MtcsIraObject *self, int start, int finish);

//////////////以下是 MtcsLiveRange方法 --------------------------
//原型 static copy_live_range ira-build.cc
MtcsLiveRange * mtcs_live_range_copy_live_range (MtcsLiveRange *mtcsLiveRange);
//原型 ira_copy_live_range_list ira-int.h ira-build.cc
MtcsLiveRange * mtcs_live_range_copy_live_range_list (MtcsLiveRange *mtcsLiveRange);
//原型 ira_merge_live_ranges ira-int.h ira-build.cc
MtcsLiveRange * mtcs_ira_object_merge_live_ranges (MtcsLiveRange * r1, MtcsLiveRange * r2);
//原型 ira_live_ranges_intersect_p ira-int.h ira-build.cc
bool mtcsira_object_live_ranges_intersect_p (MtcsLiveRange * r1, MtcsLiveRange * r2);
//原型 ira_finish_live_range ira-int.h ira-build.cc
void mtcs_ira_object_finish_live_range (MtcsLiveRange *r);
//原型 ira_finish_live_range_list ira-int.h ira-build.cc
void mtcs_ira_object_finish_live_range_list (MtcsLiveRange *r);
//原型 ira_print_live_range_list ira-int.h ira-lives.cc
void mtcs_ira_object_print_live_range_list (MtcsLiveRange *mtcsLiveRange,FILE *f);
//原型 void debug (live_range *ptr) ira-int.h ira-lives.cc
DEBUG_FUNCTION void mtcs_ira_object_live_range_debug (MtcsLiveRange *mtcsLiveRange);
//原型 extern void ira_debug_live_range_list (live_range_t); ira-int.h ira-lives.cc
void mtcs_ira_object_debug_live_range_list (MtcsLiveRange * mtcsLiveRange);

#define MTCS_IRA_OBJECT_ALLOCNO(O) ((MtcsIraAllocno *)(O)->allocno)



#endif
