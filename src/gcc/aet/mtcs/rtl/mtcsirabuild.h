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

#ifndef __GCC_MTCS_IRA_BUILD__
#define __GCC_MTCS_IRA_BUILD__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "../mtcsmicro.h"
#include "mtcsiraallocno.h"
#include "mtcsiralooptreenode.h"
#include "mtcsiraobject.h"

typedef struct _MtcsIraBuild  MtcsIraBuild;

struct _MtcsIraBuild
{
   MtcsComponent parent;
   //原型 ira_regno_allocno_map ira-int.h
   MtcsIraAllocno **ira_regno_allocno_map;
   //原型 allocno_vec ira-build.cc
   vec<MtcsIraAllocno *> allocno_vec;
   //原型 ira_allocnos ira-int.h
   MtcsIraAllocno **ira_allocnos;//build color emit 引用
   /* The size of the previous array.  */
   //原型 ira_allocnos_num ira-int.h
   int ira_allocnos_num;//build color emit.. 引用

   //iraobject比iraallocno少一个 ira_allocnos
   /* Map a conflict id to its corresponding ira_object structure.  */
   //原型 ira_object_id_map ira-int.h
   MtcsIraObject **ira_object_id_map;
   /* Vec containing references to all created ira_objects.  It is a
      container of ira_object_id_map.  */
   //原型 ira_object_id_map_vec ira-build.cc
   vec<MtcsIraObject *> ira_object_id_map_vec;
      //原型 ira_objects_num ira-int.h
   int ira_objects_num;

   /* All nodes representing basic blocks are referred through the
      following array.  We cannot use basic block member `aux' for this
      because it is used for insertion of insns on edges.  */
   //原型 ira_bb_nodes ira-int.h 原来是堆栈，改为堆分配，可以调用 mtcs_ira_loop_tree_node_new方法
   MtcsIraLoopTreeNode **ira_bb_nodes; //build costs引用

   /* LAST_BASIC_BLOCK before generating additional insns because of live
      range splitting.  Emitting insns on a critical edge creates a new
      basic block.  */
   //原型 last_basic_block_before_change ira-build.cc
   int last_basic_block_before_change;

   /* And size of the ira_loop_nodes array.  */
   //原型 ira_loop_nodes_count ira-build.cc 只有 ira-build.cc 引用 应声明为static
   unsigned int ira_loop_nodes_count;


   /* All nodes representing loops are referred through the following
      array.  */
   //原型 ira_loop_nodes ira-int.h 原来是堆栈，改为堆分配，可以调用 mtcs_ira_loop_tree_node_new方法
   MtcsIraLoopTreeNode **ira_loop_nodes; //build emit 引用

   /* The root of the loop tree corresponding to the all function.  */
   //原型 ira_loop_tree_root ira-int.h
   MtcsIraLoopTreeNode *ira_loop_tree_root; //ira build color conflicts costs emit lives 引用


   /* The array used to find duplications in conflict vectors of
      allocnos.  */
   //原型 conflict_check ira-build.cc
   int *conflict_check;

   /* The value used to mark allocation presence in conflict vector of
      the current allocno.  */
   //原型 curr_conflict_check_tick ira-build.cc
   int curr_conflict_check_tick;

  /* The current loop tree node and its regno allocno map.  */
   //原型 ira_curr_loop_tree_node ira-int.h
   MtcsIraLoopTreeNode *ira_curr_loop_tree_node;
   //原型 ira_curr_regno_allocno_map ira-int.h
   MtcsIraAllocno **ira_curr_regno_allocno_map;

   /* Array of references to all copies.  The order number of the copy
      corresponds to the index in the array.  Removed copies have NULL
      element value.  */
   //原型 ira_copies ira-int.h
   MtcsIraAllocnoCopy **ira_copies; //build 引用
   /* Size of the previous array.  */
   //原型 ira_copies_num ira-int.h
   int ira_copies_num;  //build color 引用

   /* Vec containing references to all created copies.  It is a
      container of array ira_copies.  */
   //原型 copy_vec ira-build.cc
   vec<MtcsIraAllocnoCopy *> copy_vec;

   /* The basic block currently being processed.  */
   //原型 curr_bb ira-build.cc
   basic_block curr_bb;

   /* Array of references to all allocno preferences.  The order number
      of the preference corresponds to the index in the array.  */
   //原型 ira_prefs ira-int.h ira-build.cc
   MtcsIraAllocnoPref **ira_prefs;

   /* Size of the previous array.  */
   //原型 ira_prefs_num ira-int.h ira-build.cc
   int ira_prefs_num;

   /* Vec containing references to all created preferences.  It is a
      container of array ira_prefs.  */
   //原型 pref_vec  ira-build.cc
   vec<MtcsIraAllocnoPref *> pref_vec;

   /* Pools for cost vectors.  It is defined only for allocno classes.  */
   //原型 cost_vector_pool  ira-build.cc
   pool_allocator *cost_vector_pool[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

   /* Definition of vector of loop tree nodes.  */
   /* Vec containing references to all removed loop tree nodes.  */
   //原型 static vec<ira_loop_tree_node_t> removed_loop_vec;  ira-build.cc
   vec<MtcsIraLoopTreeNode *> removed_loop_vec;

   /* Vec containing references to all children of loop tree nodes.  */
   //原型 static vec<ira_loop_tree_node_t> children_vec; ira-build.cc
   vec<MtcsIraLoopTreeNode *> children_vec;

   /* This array is used to sort allocnos to restore allocno order in
      the regno allocno list.  */
   //原型 static ira_allocno_t *regno_allocnos; ira-build.cc
    MtcsIraAllocno **regno_allocnos;

    /* Map: regno -> allocnos which will finally represent the regno for
       IR with one region.  */
    //原型 static MtcsIraAllocno * *regno_top_level_allocno_map; ira-build.cc

    MtcsIraAllocno **regno_top_level_allocno_map;


};

//原型 ira_set_allocno_class ira-int.h ira-build.cc
void mtcs_ira_build_set_allocno_class (MtcsIraBuild *self,MtcsIraAllocno *a, enum reg_class aclass);
//原型 ira_create_allocno ira-int.h ira-build.cc
MtcsIraAllocno *mtcs_ira_build_create_allocno (MtcsIraBuild *self,int regno, bool cap_p,MtcsIraLoopTreeNode *loop_tree_node);
//原型 ira_create_allocno_objects
void mtcs_ira_build_create_allocno_objects (MtcsIraBuild *self,MtcsIraAllocno *a);
//原型 ira_allocate_and_copy_costs ira-int.h
void mtcs_ira_build_allocate_and_copy_costs (MtcsIraBuild *self,int **vec, enum reg_class aclass, int *src);
//原型 ira_allocate_cost_vector ira-build.cc
int *mtcs_ira_build_allocate_cost_vector (MtcsIraBuild *self,reg_class_t aclass);
//原型 ira_allocate_and_set_or_copy_costs ira-int.h
void mtcs_ira_build_allocate_and_set_or_copy_costs (MtcsIraBuild *self,int **vec, enum reg_class aclass,
                int val, int *src);
//原型 ira_allocate_and_set_costs ira-int.h
void mtcs_ira_build_allocate_and_set_costs (MtcsIraBuild *self,int **vec, reg_class_t aclass, int val);
//原型 ira_allocate_and_accumulate_costs ira-int.h
void mtcs_ira_build_allocate_and_accumulate_costs (MtcsIraBuild *self,int **vec, enum reg_class aclass, int *src);
//原型 ira_free_cost_vector ira-int.h ira-build.cc
void mtcs_ira_build_free_cost_vector (MtcsIraBuild *self,int *vec, reg_class_t aclass);
//原型 ira_traverse_loop_tree ira-int.h ira-build.cc
void mtcs_ira_build_traverse_loop_tree (MtcsIraBuild *self,bool bb_p, MtcsIraLoopTreeNode * loop_node,
         void (*preorder_func) (MtcsIraLoopTreeNode *,void *), void (*postorder_func) (MtcsIraLoopTreeNode *,void *),void *userData);
//原型 ira_free_allocno_updated_costs ira-int.c ira-build.cc
void mtcs_ira_build_free_allocno_updated_costs (MtcsIraBuild *self,MtcsIraAllocno *a);
//原型 ira_create_pref ira-int.h ira-build.cc
MtcsIraAllocnoPref *mtcs_ira_build_create_pref (MtcsIraBuild *self,MtcsIraAllocno *a, int hard_regno, int freq);
//原型 ira_add_allocno_pref ira-int.h ira-build.cc
void mcs_ira_build_add_allocno_pref (MtcsIraBuild *self,MtcsIraAllocno *a, int hard_regno, int freq);
//原型 ira_debug_prefs ira-int.h ira-build.cc
void mtcs_ira_build_debug_prefs (MtcsIraBuild *self);
//原型 ira_remove_pref ira-int.h ira-build.cc
void mtcs_ira_build_remove_pref (MtcsIraBuild *self,MtcsIraAllocnoPref *pref);
//原型 ira_remove_allocno_prefs ira-int.h ira-build.cc
void mtcs_ira_build_remove_allocno_prefs (MtcsIraBuild *self,MtcsIraAllocno *a);
//原型 ira_create_copy ira-int.h ira-build.cc
MtcsIraAllocnoCopy *mtcs_ira_build_create_copy (MtcsIraBuild *self,MtcsIraAllocno *first, MtcsIraAllocno *second, int freq,
       bool constraint_p, rtx_insn *insn,MtcsIraLoopTreeNode *loop_tree_node);
//原型 ira_add_allocno_copy ira-int.h ira-build.cc
MtcsIraAllocnoCopy *mtcs_ira_build_add_allocno_copy (MtcsIraBuild *self,MtcsIraAllocno *first, MtcsIraAllocno *second, int freq,
            bool constraint_p, rtx_insn *insn, MtcsIraLoopTreeNode *loop_tree_node);
//原型 ira_debug_copies ira-int.h ira-build.cc
void mtcs_ira_build_debug_copies (MtcsIraBuild *self);
//原型 ira_allocate_cost_vector ira-int.h ira-build.cc
int * mtcs_ira_build_allocate_cost_vector (MtcsIraBuild *self,reg_class_t aclass);
//原型 ira_flattening ira-int.h ira-build.cc
void mtcs_ira_build_flattening (MtcsIraBuild *self,int max_regno_before_emit, int ira_max_point_before_emit);
//原型 ira_destroy ira-int.h ira-build.cc
void mtcs_ira_build_destroy (MtcsIraBuild *self);
//原型 ira_build ira-int.h ira-build.cc
bool mtcs_ira_build_build (MtcsIraBuild *self);

/* Two access macros to the nodes representing basic blocks.  */
#define MTCS_IRA_BB_NODE_BY_INDEX(index) __extension__        \
(({ MtcsIraLoopTreeNode * _node = (mtcsIraBuild->ira_bb_nodes[index]);    \
     if (_node->children != NULL || _node->loop != NULL || _node->bb == NULL)\
       {                      \
         fprintf (stderr,                 \
                  "\n%s: %d: error in %s: it is not a block node\n", \
                  __FILE__, __LINE__, __FUNCTION__);        \
         gcc_unreachable ();                 \
       }                      \
     _node; }))

#define MTCS_IRA_BB_NODE(bb) MTCS_IRA_BB_NODE_BY_INDEX ((bb)->index)

/* The iterator for all allocnos.  */
//原型 ira_allocno_iterator ira-int.h
typedef struct  _MtcsIraAllocnoIterator {
  /* The number of the current element in IRA_ALLOCNOS.  */
  int n;
}MtcsIraAllocnoIterator;


/* Initialize the iterator I.  */
//原型 ira_allocno_iter_init ira-int.h
inline void mtcs_ira_allocno_iter_init (MtcsIraAllocnoIterator *i)
{
  i->n = 0;
}

/* Return TRUE if we have more allocnos to visit, in which case *A is
   set to the allocno to be visited.  Otherwise, return FALSE.  */
//原型 ira_allocno_iter_cond ira-int.h
inline bool mtcs_ira_allocno_iter_cond (MtcsIraBuild *self,MtcsIraAllocnoIterator *i, MtcsIraAllocno **a)
{
   int n;

   for (n = i->n; n < self->ira_allocnos_num; n++)
      if (self->ira_allocnos[n] != NULL){
         *a = self->ira_allocnos[n];
         i->n = n + 1;
         return true;
      }
   return false;
}

/* Loop over all allocnos.  In each iteration, A is set to the next
   allocno.  ITER is an instance of ira_allocno_iterator used to iterate
   the allocnos.  */
//原型 FOR_EACH_ALLOCNO ira-int.h
#define MTCS_FOR_EACH_ALLOCNO(BUILD,A, ITER)         \
  for (mtcs_ira_allocno_iter_init (&(ITER));         \
  mtcs_ira_allocno_iter_cond (BUILD,&(ITER), &(A));)


/* The iterator for all objects.  */
//原型 ira_object_iterator ira-int.h
typedef struct _MtcsIraObjectIterator{
  /* The number of the current element in ira_object_id_map.  */
  int n;
}MtcsIraObjectIterator;

/* Initialize the iterator I.  */
//原型 ira_object_iter_init ira-int.h
inline void mtcs_ira_object_iter_init (MtcsIraObjectIterator *i)
{
  i->n = 0;
}

/* Return TRUE if we have more objects to visit, in which case *OBJ is
   set to the object to be visited.  Otherwise, return FALSE.  */
//原型 ira_object_iter_cond ira-int.h
inline bool mtcs_ira_object_iter_cond (MtcsIraBuild *self,MtcsIraObjectIterator *i, MtcsIraObject **obj)
{
   int n;

   for (n = i->n; n < self->ira_objects_num; n++)
      if (self->ira_object_id_map[n] != NULL){
         *obj = self->ira_object_id_map[n];
         i->n = n + 1;
         return true;
      }
   return false;
}

/* Loop over all objects.  In each iteration, OBJ is set to the next
   object.  ITER is an instance of ira_object_iterator used to iterate
   the objects.  */
//原型 FOR_EACH_OBJECT ira-int.h
#define MTCS_FOR_EACH_OBJECT(BUILD,OBJ, ITER)        \
  for (mtcs_ira_object_iter_init (&(ITER));       \
       mtcs_ira_object_iter_cond (BUILD,&(ITER), &(OBJ));)

/* The iterator for object conflicts.  */
//原型 ira_object_conflict_iterator ira-int.h
typedef struct _MtcsIraObjectConflictIterator {

  /* TRUE if the conflicts are represented by vector of allocnos.  */
  bool conflict_vec_p;

  /* The conflict vector or conflict bit vector.  */
  void *vec;

  /* The number of the current element in the vector (of type
     ira_object_t or IRA_INT_TYPE).  */
  unsigned int word_num;

  /* The bit vector size.  It is defined only if
     OBJECT_CONFLICT_VEC_P is FALSE.  */
  unsigned int size;

  /* The current bit index of bit vector.  It is defined only if
     OBJECT_CONFLICT_VEC_P is FALSE.  */
  unsigned int bit_num;

  /* The object id corresponding to the 1st bit of the bit vector.  It
     is defined only if OBJECT_CONFLICT_VEC_P is FALSE.  */
  int base_conflict_id;

  /* The word of bit vector currently visited.  It is defined only if
     OBJECT_CONFLICT_VEC_P is FALSE.  */
  unsigned IRA_INT_TYPE word;

}MtcsIraObjectConflictIterator;

/* Initialize the iterator I with ALLOCNO conflicts.  */
//原型 ira_object_conflict_iter_init ira-int.h
inline void mtcs_ira_object_conflict_iter_init (MtcsIraObjectConflictIterator *i,
                MtcsIraObject *obj)
{
   i->conflict_vec_p = obj->conflict_vec_p;
   i->vec = obj->conflicts_array;
   i->word_num = 0;
   if (i->conflict_vec_p)
      i->size = i->bit_num = i->base_conflict_id = i->word = 0;
   else{
      if (obj->min > obj->max)
         i->size = 0;
      else
         i->size = ((obj->max - obj->min + IRA_INT_BITS) / IRA_INT_BITS) * sizeof (IRA_INT_TYPE);
      i->bit_num = 0;
      i->base_conflict_id = obj->min;
      i->word = (i->size == 0 ? 0 : ((IRA_INT_TYPE *) i->vec)[0]);
   }
}

/* Return TRUE if we have more conflicting allocnos to visit, in which
   case *A is set to the allocno to be visited.  Otherwise, return
   FALSE.  */
//原型 ira_object_conflict_iter_cond ira-int.h
inline bool mtcs_ira_object_conflict_iter_cond (MtcsIraBuild *self,MtcsIraObjectConflictIterator *i,
      MtcsIraObject **pobj)
{
   MtcsIraObject *obj;

   if (i->conflict_vec_p){
      obj = ((MtcsIraObject **) i->vec)[i->word_num++];
      if (obj == NULL)
         return false;
   }else{
      unsigned IRA_INT_TYPE word = i->word;
      unsigned int bit_num = i->bit_num;

      /* Skip words that are zeros.  */
      for (; word == 0; word = ((IRA_INT_TYPE *) i->vec)[i->word_num]){
         i->word_num++;

         /* If we have reached the end, break.  */
         if (i->word_num * sizeof (IRA_INT_TYPE) >= i->size)
            return false;

         bit_num = i->word_num * IRA_INT_BITS;
      }

      /* Skip bits that are zero.  */
      int off = ctz_hwi (word);
      bit_num += off;
      word >>= off;

      obj = self->ira_object_id_map[bit_num + i->base_conflict_id];
      i->bit_num = bit_num + 1;
      i->word = word >> 1;
   }

   *pobj = obj;
   return true;
}

/* Loop over all objects conflicting with OBJ.  In each iteration,
   CONF is set to the next conflicting object.  ITER is an instance
   of ira_object_conflict_iterator used to iterate the conflicts.  */
//原型 FOR_EACH_OBJECT_CONFLICT ira-int.h
#define MTCS_FOR_EACH_OBJECT_CONFLICT(BUILD,OBJ, CONF, ITER)        \
  for (mtcs_ira_object_conflict_iter_init (&(ITER), (OBJ));         \
       mtcs_ira_object_conflict_iter_cond (BUILD,&(ITER), &(CONF));)


/* The iterator for prefs.  */
//原型 struct ira_pref_iterator ira-int.h
typedef struct _MtcsIraAllocnoPrefIterator {
  /* The number of the current element in IRA_PREFS.  */
  int n;
}MtcsIraAllocnoPrefIterator;

/* Initialize the iterator I.  */
//原型 ira_pref_iter_init ira-int.h
inline void mtcs_ira_allocno_pref_iter_init (MtcsIraAllocnoPrefIterator *i)
{
  i->n = 0;
}

/* Return TRUE if we have more prefs to visit, in which case *PREF is
   set to the pref to be visited.  Otherwise, return FALSE.  */
//原型 ira_pref_iter_cond ira-int.h
inline bool mtcs_ira_allocno_pref_iter_cond (MtcsIraBuild *self,MtcsIraAllocnoPrefIterator *i, MtcsIraAllocnoPref **pref)
{
   int n;

   for (n = i->n; n < self->ira_prefs_num; n++)
      if (self->ira_prefs[n] != NULL){
         *pref = self->ira_prefs[n];
         i->n = n + 1;
         return true;
      }
   return false;
}

/* Loop over all prefs.  In each iteration, P is set to the next
   pref.  ITER is an instance of ira_pref_iterator used to iterate
   the prefs.  */
//原型 FOR_EACH_PREF ira-int.h
#define MTCS_FOR_EACH_PREF(BUILD,P, ITER)            \
  for (mtcs_ira_allocno_pref_iter_init (&(ITER));         \
  mtcs_ira_allocno_pref_iter_cond (BUILD,&(ITER), &(P));)

/* The iterator for copies.  */
//原型 struct ira_copy_iterator ira-int.h
typedef struct _MtcsIraAllocnoCopyIterator {
  /* The number of the current element in IRA_COPIES.  */
  int n;
}MtcsIraAllocnoCopyIterator;

/* Initialize the iterator I.  */
//原型  ira_copy_iter_init ira-int.h
inline void mtcs_ira_allocno_copy_iter_init (MtcsIraAllocnoCopyIterator *i)
{
  i->n = 0;
}

/* Return TRUE if we have more copies to visit, in which case *CP is
   set to the copy to be visited.  Otherwise, return FALSE.  */
//原型  ira_copy_iter_cond ira-int.h
inline bool mtcs_ira_allocno_copy_iter_cond (MtcsIraBuild *self,MtcsIraAllocnoCopyIterator *i, MtcsIraAllocnoCopy **cp)
{
   int n;
   for (n = i->n; n < self->ira_copies_num; n++)
      if (self->ira_copies[n] != NULL){
         *cp = self->ira_copies[n];
         i->n = n + 1;
         return true;
      }
   return false;
}

/* Loop over all copies.  In each iteration, C is set to the next
   copy.  ITER is an instance of ira_copy_iterator used to iterate
   the copies.  */
//原型  FOR_EACH_COPY ira-int.h
#define MTCS_FOR_EACH_COPY(BUILD,P, ITER)            \
  for (mtcs_ira_allocno_copy_iter_init (&(ITER));         \
  mtcs_ira_allocno_copy_iter_cond (BUILD,&(ITER), &(P));)


#endif
