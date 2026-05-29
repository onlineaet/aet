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

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "ira.h"
#include "ira-int.h"
#include "diagnostic-core.h"
#include "cfgrtl.h"
#include "cfgbuild.h"
#include "cfgcleanup.h"
#include "expr.h"
#include "tree-pass.h"
#include "output.h"
#include "reload.h"
#include "cfgloop.h"
#include "lra.h"
#include "dce.h"
#include "dbgcnt.h"
#include "rtl-iter.h"
#include "shrink-wrap.h"
#include "print-rtl.h"

#include "mtcsiraobject.h"
#include "mtcsiraallocno.h"
#include "../mtcstarget.h"

/**
 * 创建 Object时target已经完成了各个组件的创建，所以在初始化的地方可以引用组件。
 */
static void mtcsIraObjectInit(MtcsIraObject *self)
{
   self->conflict_vec_p = false;
   self->conflicts_array = NULL;
   self->num_accumulated_conflicts = 0;
   self->min = INT_MAX;
   self->max = -1;
   self->live_ranges = NULL;
}

/* Return TRUE if a conflict vector with NUM elements is more
   profitable than a conflict bit vector for OBJ.  */
//原型 ira_conflict_vector_profitable_p ira-int.h ira-build.cc
bool mtcs_ira_object_ira_conflict_vector_profitable_p (MtcsIraObject *self, int num)
{
  int nbytes;
  int max = self->max;
  int min = self->min;

  if (max < min)
    /* We prefer a bit vector in such case because it does not result
       in allocation.  */
    return false;

  nbytes = (max - min) / 8 + 1;
  STATIC_ASSERT (sizeof (MtcsIraObject *) <= 8);
  /* Don't use sizeof (ira_object_t), use constant 8.  Size of ira_object_t (a
     pointer) is different on 32-bit and 64-bit targets.  Usage sizeof
     (ira_object_t) can result in different code generation by GCC built as 32-
     and 64-bit program.  In any case the profitability is just an estimation
     and border cases are rare.  */
  return (2 * 8 /* sizeof (ira_object_t) */ * (num + 1) < 3 * nbytes);
}

/* Allocates and initialize the conflict vector of OBJ for NUM
   conflicting objects.  */
//原型 ira_allocate_conflict_vec ira-int.h ira-build.cc
void mtcs_ira_object_ira_allocate_conflict_vec (MtcsIraObject *self, int num)
{
  int size;
  MtcsIraObject **vec;

  ira_assert (self->conflicts_array == NULL);
  num++; /* for NULL end marker  */
  size = sizeof (MtcsIraObject *) * num;
  self->conflicts_array = ira_allocate (size);
  vec = (MtcsIraObject **) self->conflicts_array;
  vec[0] = NULL;
  self->num_accumulated_conflicts  = 0;
  self->conflicts_array_size = size;
  self->conflict_vec_p = true;
}

/* Allocate and initialize the conflict bit vector of OBJ.  */
//原型 static void allocate_conflict_bit_vec ira-build.cc
static void allocate_conflict_bit_vec (MtcsIraObject *self)
{
  unsigned int size;

  ira_assert (self->conflicts_array == NULL);
  size = ((self->max - self->min + IRA_INT_BITS) / IRA_INT_BITS * sizeof (IRA_INT_TYPE));
  self->conflicts_array = ira_allocate (size);
  memset (self->conflicts_array, 0, size);
  self->conflicts_array_size = size;
  self->conflict_vec_p = false;
}

/* Allocate and initialize the conflict vector or conflict bit vector
   of OBJ for NUM conflicting allocnos whatever is more profitable.  */
//原型 ira_allocate_object_conflicts ira-int.h ira-build.cc
void mtcs_ira_object_allocate_object_conflicts (MtcsIraObject *self, int num)
{
  if (mtcs_ira_object_ira_conflict_vector_profitable_p/*!ira_conflict_vector_profitable_p*/(self, num))
     mtcs_ira_object_ira_allocate_conflict_vec/*!ira_allocate_conflict_vec*/(self, num);
  else
    allocate_conflict_bit_vec (self);
}

/* Add OBJ2 to the conflicts of self.  */
//原型 add_to_conflicts ira-build.cc
static void add_to_conflicts (MtcsIraObject *self, MtcsIraObject *obj2)
{
   int num;
   unsigned int size;

   if (self->conflict_vec_p) {
      MtcsIraObject **vec =self->conflicts_array;
      int curr_num =self->num_accumulated_conflicts;
      num = curr_num + 2;
      if (self->conflicts_array_size < num * sizeof (MtcsIraObject *)){
         MtcsIraObject **newvec;
         size = (3 * num / 2 + 1) * sizeof (MtcsIraAllocno *);
         newvec = (MtcsIraObject **) ira_allocate (size);
         memcpy (newvec, vec, curr_num * sizeof (MtcsIraObject *));
         ira_free (vec);
         vec = newvec;
         self->conflicts_array = vec;
         self->conflicts_array_size = size;
      }
      vec[num - 2] = obj2;
      vec[num - 1] = NULL;
      self->num_accumulated_conflicts++;
   }else{
      int nw, added_head_nw, id;
      IRA_INT_TYPE *vec = (IRA_INT_TYPE *)self->conflicts_array ;

      id = obj2->id;
      if (self->min > id){
         /* Expand head of the bit vector.  */
         added_head_nw = (self->min - id - 1) / IRA_INT_BITS + 1;
         nw = (self->max - self->min) / IRA_INT_BITS + 1;
         size = (nw + added_head_nw) * sizeof (IRA_INT_TYPE);
         if (self->conflicts_array_size  >= size){
            memmove ((char *) vec + added_head_nw * sizeof (IRA_INT_TYPE),vec, nw * sizeof (IRA_INT_TYPE));
            memset (vec, 0, added_head_nw * sizeof (IRA_INT_TYPE));
         }else{
            size = (3 * (nw + added_head_nw) / 2 + 1) * sizeof (IRA_INT_TYPE);
            vec = (IRA_INT_TYPE *) ira_allocate (size);
            memcpy ((char *) vec + added_head_nw * sizeof (IRA_INT_TYPE),
            self->conflicts_array , nw * sizeof (IRA_INT_TYPE));
            memset (vec, 0, added_head_nw * sizeof (IRA_INT_TYPE));
            memset ((char *) vec + (nw + added_head_nw) * sizeof (IRA_INT_TYPE),
                  0, size - (nw + added_head_nw) * sizeof (IRA_INT_TYPE));
            ira_free (self->conflicts_array);
            self->conflicts_array = vec;
            self->conflicts_array_size  = size;
         }
         self->min-= added_head_nw * IRA_INT_BITS;
      }else if (self->max < id){
         nw = (id - self->min) / IRA_INT_BITS + 1;
         size = nw * sizeof (IRA_INT_TYPE);
         if (self->conflicts_array_size  < size){
            /* Expand tail of the bit vector.  */
            size = (3 * nw / 2 + 1) * sizeof (IRA_INT_TYPE);
            vec = (IRA_INT_TYPE *) ira_allocate (size);
            memcpy (vec, self->conflicts_array, self->conflicts_array_size);
            memset ((char *) vec + self->conflicts_array_size,
            0, size -self->conflicts_array_size);
            ira_free (self->conflicts_array);
            self->conflicts_array = vec;
            self->conflicts_array_size= size;
         }
         self->max = id;
      }
      SET_MINMAX_SET_BIT (vec, id, self->min, self->max);
   }
}

/* Add OBJ1 to the conflicts of OBJ2 and vice versa.  */
//原型 static void ira_add_conflict ira-build.cc 改为公共方法 供mtcsirabuild 的方法 ira_flattening调用
void mtcs_ira_object_add_conflict (MtcsIraObject *self, MtcsIraObject *obj2)
{
  add_to_conflicts (self, obj2);
  add_to_conflicts (obj2, self);
}


/* Clear all conflicts of OBJ.  */
//原型 static void clear_conflicts ira-build.cc 改为公共方法 供mtcsirabuild 的方法 ira_flattening调用
void mtcs_ira_object_clear_conflicts (MtcsIraObject *self)
{
   if (self->conflict_vec_p){
      self->num_accumulated_conflicts = 0;
      ((MtcsIraObject **)self->conflicts_array)[0] = NULL;
   }else if (self->conflicts_array_size != 0){
      int nw;

      nw = (self->max - self->min) / IRA_INT_BITS + 1;
      memset ( (IRA_INT_TYPE *)self->conflicts_array , 0, nw * sizeof (IRA_INT_TYPE));
   }
}

/* Create and return a live range for OBJECT with given attributes.  */
//原型 ira_create_live_range ira-int.h ira-build.cc
MtcsLiveRange * mtcs_ira_object_create_live_range (MtcsIraObject *self, int start, int finish,MtcsLiveRange *next)
{
   MtcsLiveRange *p;

   p = n_slice_alloc0 (sizeof(MtcsLiveRange));/*!live_range_pool.allocate ();*/
   p->object = self;
   p->start = start;
   p->finish = finish;
   p->next = next;
   return p;
}

/* Create a new live range for OBJECT and queue it at the head of its
   live range list.  */
//原型 ira_create_live_range ira-int.h ira-build.cc
void mtcs_ira_object_add_live_range_to_object (MtcsIraObject *self, int start, int finish)
{
   MtcsLiveRange *p;
   p = mtcs_ira_object_create_live_range/*!ira_create_live_range*/(self, start, finish, self->live_ranges);
   self->live_ranges = p;
}

/* Copy allocno live range R and return the result.  */
//原型 static copy_live_range ira-build.cc
MtcsLiveRange * mtcs_live_range_copy_live_range (MtcsLiveRange *mtcsLiveRange)
{
   MtcsLiveRange *p;
   p = n_slice_alloc0 (sizeof(MtcsLiveRange));/*!live_range_pool.allocate ();*/

   *p = *mtcsLiveRange;
   return p;
}

/* Copy allocno live range list given by its head R and return the
   result.  */
//原型 ira_copy_live_range_list ira-int.h ira-build.cc
MtcsLiveRange * mtcs_live_range_copy_live_range_list (MtcsLiveRange *mtcsLiveRange)
{
   MtcsLiveRange *p, *first, *last;

   if (mtcsLiveRange == NULL)
      return NULL;
   for (first = last = NULL; mtcsLiveRange != NULL; mtcsLiveRange = mtcsLiveRange->next){
      p = mtcs_live_range_copy_live_range/*!copy_live_range*/(mtcsLiveRange);
      if (first == NULL)
         first = p;
      else
         last->next = p;
      last = p;
   }
   return first;
}

/* Merge ranges R1 and R2 and returns the result.  The function
   maintains the order of ranges and tries to minimize number of the
   result ranges.  */
//原型 ira_merge_live_ranges ira-int.h ira-build.cc
MtcsLiveRange * mtcs_ira_object_merge_live_ranges (MtcsLiveRange * r1, MtcsLiveRange * r2)
{
   MtcsLiveRange * first, *last;

   if (r1 == NULL)
      return r2;
   if (r2 == NULL)
      return r1;
   for (first = last = NULL; r1 != NULL && r2 != NULL;){
      if (r1->start < r2->start)
         std::swap (r1, r2);
      if (r1->start <= r2->finish + 1){
         /* Intersected ranges: merge r1 and r2 into r1.  */
         r1->start = r2->start;
         if (r1->finish < r2->finish)
            r1->finish = r2->finish;
         MtcsLiveRange * temp = r2;
         r2 = r2->next;
         mtcs_ira_object_finish_live_range/*!ira_finish_live_range*/(temp);
         if (r2 == NULL){
            /* To try to merge with subsequent ranges in r1.  */
            r2 = r1->next;
            r1->next = NULL;
         }
      }else{
         /* Add r1 to the result.  */
         if (first == NULL)
            first = last = r1;
         else{
            last->next = r1;
            last = r1;
         }
         r1 = r1->next;
         if (r1 == NULL){
            /* To try to merge with subsequent ranges in r2.  */
            r1 = r2->next;
            r2->next = NULL;
         }
      }
   }
   if (r1 != NULL){
      if (first == NULL)
         first = r1;
      else
         last->next = r1;
      ira_assert (r1->next == NULL);
   }else if (r2 != NULL){
      if (first == NULL)
         first = r2;
      else
         last->next = r2;
      ira_assert (r2->next == NULL);
   }else{
      ira_assert (last->next == NULL);
   }
   return first;
}

/* Return TRUE if live ranges R1 and R2 intersect.  */
//原型 ira_live_ranges_intersect_p ira-int.h ira-build.cc
bool mtcsira_object_live_ranges_intersect_p (MtcsLiveRange * r1, MtcsLiveRange * r2)
{
   /* Remember the live ranges are always kept ordered.  */
   while (r1 != NULL && r2 != NULL){
      if (r1->start > r2->finish)
         r1 = r1->next;
      else if (r2->start > r1->finish)
         r2 = r2->next;
      else
      return true;
   }
   return false;
}

/* Free allocno live range R.  */
//原型 ira_finish_live_range ira-int.h ira-build.cc
void mtcs_ira_object_finish_live_range (MtcsLiveRange *r)
{
  n_slice_free(MtcsLiveRange,r);/*!live_range_pool.remove (r);*/
}

/* Free list of allocno live ranges starting with R.  */
//原型 ira_finish_live_range_list ira-int.h ira-build.cc
void mtcs_ira_object_finish_live_range_list (MtcsLiveRange *r)
{
   MtcsLiveRange *next_r;

   for (; r != NULL; r = next_r){
      next_r = r->next;
      mtcs_ira_object_finish_live_range/*!ira_finish_live_range*/(r);
   }
}

/* Print live ranges R to file F.  */
//原型 ira_print_live_range_list ira-int.h ira-lives.cc
void mtcs_ira_object_print_live_range_list (MtcsLiveRange *mtcsLiveRange,FILE *f)
{
  for (; mtcsLiveRange != NULL; mtcsLiveRange = mtcsLiveRange->next)
    fprintf (f, " [%d..%d]", mtcsLiveRange->start, mtcsLiveRange->finish);
  fprintf (f, "\n");
}

//原型 void debug (live_range *ptr) ira-int.h ira-lives.cc
DEBUG_FUNCTION void mtcs_ira_object_live_range_debug (MtcsLiveRange *mtcsLiveRange)
{
  if (mtcsLiveRange)
     mtcs_ira_object_print_live_range_list (mtcsLiveRange,stderr);
  else
    fprintf (stderr, "<nil>\n");
}

/* Print live ranges R to stderr.  */
//原型 extern void ira_debug_live_range_list (live_range_t); ira-int.h ira-lives.cc
void mtcs_ira_object_debug_live_range_list (MtcsLiveRange * mtcsLiveRange)
{
   mtcs_ira_object_print_live_range_list/*!ira_print_live_range_list*/ (mtcsLiveRange,stderr);
}

//原型 object_pool.remove (obj);
void mtcs_ira_object_free(MtcsIraObject *self)
{
   n_slice_free(MtcsIraObject,self);
}


MtcsIraObject *mtcs_ira_object_new(MtcsMode *mtcsMode,int subword)
{
   MtcsIraObject *self = n_slice_alloc0 (sizeof(MtcsIraObject));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   self->subword=subword;
   mtcsIraObjectInit(self);
   return self;
}
