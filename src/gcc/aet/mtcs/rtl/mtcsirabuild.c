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
 * base on ira-build.cc
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
#include "sparseset.h"

#include "../mtcstarget.h"
#include "../mtcsdfcore.h"
#include "../mtcsdfproblems.h"
#include "mtcsirabuild.h"
#include "mtcsira.h"
#include "mtcsiraint.h"
#include "mtcsiralives.h"


/* Initialize some members in loop tree node NODE.  Use LOOP_NUM for
   the member loop_num.  */
static void init_loop_tree_node (MtcsIraBuild *self,MtcsIraLoopTreeNode *node, int loop_num)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   int max_regno = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);

   node->regno_allocno_map   = (MtcsIraAllocno **) ira_allocate (sizeof (MtcsIraAllocno *) * max_regno);
   memset (node->regno_allocno_map, 0, sizeof (MtcsIraAllocno*) * max_regno);
   memset (node->reg_pressure, 0, sizeof (node->reg_pressure));
   node->all_allocnos = ira_allocate_bitmap ();
   node->modified_regnos = ira_allocate_bitmap ();
   node->border_allocnos = ira_allocate_bitmap ();
   node->local_copies = ira_allocate_bitmap ();
   node->loop_num = loop_num;
   node->children = NULL;
   node->subloops = NULL;
}


/* The following function allocates the loop tree nodes.  If
   CURRENT_LOOPS is NULL, the nodes corresponding to the loops (except
   the root which corresponds the all function) will be not allocated
   but nodes will still be allocated for basic blocks.  */
static void create_loop_tree_nodes (MtcsIraBuild *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   unsigned int i, j;
   bool skip_p;
   edge_iterator ei;
   edge e;
   loop_p loop;

   self->ira_bb_nodes  = ((MtcsIraLoopTreeNode **) ira_allocate (sizeof (MtcsIraLoopTreeNode*) * last_basic_block_for_fn (cfun)));
   self->last_basic_block_before_change = last_basic_block_for_fn (cfun);
   for (i = 0; i < (unsigned int) last_basic_block_for_fn (cfun); i++)
      self->ira_bb_nodes[i] =mtcs_ira_loop_tree_node_new(mtcsMode);

   if (current_loops == NULL){
      self->ira_loop_nodes_count = 1;
      self->ira_loop_nodes  = ((MtcsIraLoopTreeNode **) ira_allocate (sizeof (MtcsIraLoopTreeNode*) * 1));
      self->ira_loop_nodes[0] =mtcs_ira_loop_tree_node_new(mtcsMode);
      init_loop_tree_node (self,self->ira_loop_nodes[0], 0);
      return;
   }
   self->ira_loop_nodes_count = number_of_loops (cfun);
   self->ira_loop_nodes  = ((MtcsIraLoopTreeNode **) ira_allocate (sizeof (MtcsIraLoopTreeNode*) * self->ira_loop_nodes_count));
   for(i=0;i<self->ira_loop_nodes_count;i++)
      self->ira_loop_nodes[i] =mtcs_ira_loop_tree_node_new(mtcsMode);


   FOR_EACH_VEC_SAFE_ELT (get_loops (cfun), i, loop){
      if (loop_outer (loop) != NULL){
         self->ira_loop_nodes[i]->regno_allocno_map = NULL;
         skip_p = false;
         FOR_EACH_EDGE (e, ei, loop->header->preds)
            if (e->src != loop->latch  && (e->flags & EDGE_ABNORMAL) && EDGE_CRITICAL_P (e)){
               skip_p = true;
               break;
            }
         if (skip_p)
            continue;
         auto_vec<edge> edges = get_loop_exit_edges (loop);
         FOR_EACH_VEC_ELT (edges, j, e)
         if ((e->flags & EDGE_ABNORMAL) && EDGE_CRITICAL_P (e)){
            skip_p = true;
            break;
         }
         if (skip_p)
            continue;
      }
      init_loop_tree_node (self,self->ira_loop_nodes[i], loop->num);
   }
}

/* The function returns TRUE if there are more one allocation
   region.  */
static bool more_one_region_p (MtcsIraBuild *self)
{
   unsigned int i;
   loop_p loop;

   if (current_loops != NULL)
      FOR_EACH_VEC_SAFE_ELT (get_loops (cfun), i, loop)
         if (self->ira_loop_nodes[i]->regno_allocno_map != NULL  && self->ira_loop_tree_root != self->ira_loop_nodes[i])
            return true;
   return false;
}


/* Free the loop tree node of a loop.  */
//被 mtcs_ira_loop_tree_node_free替换
//static void finish_loop_tree_node (MtcsIraBuild *self,MtcsIraLoopTreeNode *loop)

/* Free the loop tree nodes.  */
static void finish_loop_tree_nodes (MtcsIraBuild *self)
{
   unsigned int i;
   for (i = 0; i < self->ira_loop_nodes_count; i++){
      mtcs_ira_loop_tree_node_free(self->ira_loop_nodes[i]);
      self->ira_loop_nodes[i] =NULL;
   }
   ira_free (self->ira_loop_nodes);
   for (i = 0; i < (unsigned int) self->last_basic_block_before_change; i++){
      mtcs_ira_loop_tree_node_free(self->ira_bb_nodes[i]);
      self->ira_bb_nodes[i] =NULL;
   }
   ira_free (self->ira_bb_nodes);
}

/* The following recursive function adds LOOP to the loop tree
   hierarchy.  LOOP is added only once.  If LOOP is NULL we adding
   loop designating the whole function when CFG loops are not
   built.  */
static void add_loop_to_tree (MtcsIraBuild *self,class loop *loop)
{
   int loop_num;
   class loop *parent;
   MtcsIraLoopTreeNode *loop_node, *parent_node;

   /* We cannot use loop node access macros here because of potential
   checking and because the nodes are not initialized enough
   yet.  */
   if (loop != NULL && loop_outer (loop) != NULL)
      add_loop_to_tree (self,loop_outer (loop));
   loop_num = loop != NULL ? loop->num : 0;
   if (self->ira_loop_nodes[loop_num]->regno_allocno_map != NULL && self->ira_loop_nodes[loop_num]->children == NULL){
      /* We have not added loop node to the tree yet.  */
      loop_node = self->ira_loop_nodes[loop_num];
      loop_node->loop = loop;
      loop_node->bb = NULL;
      if (loop == NULL)
         parent = NULL;
      else{
         for (parent = loop_outer (loop); parent != NULL; parent = loop_outer (parent))
            if (self->ira_loop_nodes[parent->num]->regno_allocno_map != NULL)
               break;
      }
      if (parent == NULL){
         loop_node->next = NULL;
         loop_node->subloop_next = NULL;
         loop_node->parent = NULL;
      }else{
         parent_node = self->ira_loop_nodes[parent->num];
         loop_node->next = parent_node->children;
         parent_node->children = loop_node;
         loop_node->subloop_next = parent_node->subloops;
         parent_node->subloops = loop_node;
         loop_node->parent = parent_node;
      }
   }
}


/* The following recursive function sets up levels of nodes of the
   tree given its root LOOP_NODE.  The enumeration starts with LEVEL.
   The function returns maximal value of level in the tree + 1.  */
static int setup_loop_tree_level (MtcsIraBuild *self,MtcsIraLoopTreeNode *loop_node, int level)
{
   int height, max_height;
   MtcsIraLoopTreeNode *subloop_node;

   ira_assert (loop_node->bb == NULL);
   loop_node->level = level;
   max_height = level + 1;
   for (subloop_node = loop_node->subloops; subloop_node != NULL; subloop_node = subloop_node->subloop_next){
      ira_assert (subloop_node->bb == NULL);
      height = setup_loop_tree_level(self,subloop_node, level + 1);
      if (height > max_height)
         max_height = height;
   }
   return max_height;
}

/* Two access macros to the nodes representing loops.  */
#define MTCS_IRA_LOOP_NODE_BY_INDEX(index) __extension__         \
(({ MtcsIraLoopTreeNode * const _node = (self->ira_loop_nodes[index]);  \
     if (_node->children == NULL || _node->bb != NULL       \
         || (_node->loop == NULL && current_loops != NULL))    \
       {                      \
         fprintf (stderr,                 \
                  "\n%s: %d: error in %s: it is not a loop node\n",  \
                  __FILE__, __LINE__, __FUNCTION__);        \
         gcc_unreachable ();                 \
       }                      \
     _node; }))


/* Create the loop tree.  The algorithm is designed to provide correct
   order of loops (they are ordered by their last loop BB) and basic
   blocks in the chain formed by member next.  */
static void form_loop_tree (MtcsIraBuild *self)
{
   basic_block bb;
   class loop *parent;
   MtcsIraLoopTreeNode *bb_node, *loop_node;

   /* We cannot use loop/bb node access macros because of potential
   checking and because the nodes are not initialized enough
   yet.  */
   FOR_EACH_BB_FN (bb, cfun){
      bb_node = self->ira_bb_nodes[bb->index];
      bb_node->bb = bb;
      bb_node->loop = NULL;
      bb_node->subloops = NULL;
      bb_node->children = NULL;
      bb_node->subloop_next = NULL;
      bb_node->next = NULL;
      if (current_loops == NULL)
         parent = NULL;
      else{
         for (parent = bb->loop_father; parent != NULL; parent = loop_outer (parent))
            if (self->ira_loop_nodes[parent->num]->regno_allocno_map != NULL)
               break;
      }
      add_loop_to_tree(self,parent);
      loop_node = self->ira_loop_nodes[parent == NULL ? 0 : parent->num];
      bb_node->next = loop_node->children;
      bb_node->parent = loop_node;
      loop_node->children = bb_node;
   }
   self->ira_loop_tree_root = MTCS_IRA_LOOP_NODE_BY_INDEX (0);
   ira_loop_tree_height = setup_loop_tree_level(self,self->ira_loop_tree_root, 0);
   ira_assert (self->ira_loop_tree_root->regno_allocno_map != NULL);
}

/* Rebuild IRA_REGNO_ALLOCNO_MAP and REGNO_ALLOCNO_MAPs of the loop
   tree nodes.  */
static void rebuild_regno_allocno_maps (MtcsIraBuild *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);


   unsigned int l;
   int max_regno, regno;
   MtcsIraAllocno *a;
   MtcsIraLoopTreeNode *loop_tree_node;
   loop_p loop;
   MtcsIraAllocnoIterator ai;

   ira_assert (current_loops != NULL);
   max_regno = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);
   FOR_EACH_VEC_SAFE_ELT (get_loops (cfun), l, loop)
      if (self->ira_loop_nodes[l]->regno_allocno_map != NULL){
         ira_free (self->ira_loop_nodes[l]->regno_allocno_map);
         self->ira_loop_nodes[l]->regno_allocno_map  = (MtcsIraAllocno **) ira_allocate (sizeof (MtcsIraAllocno*)* max_regno);
         memset (self->ira_loop_nodes[l]->regno_allocno_map, 0, sizeof (MtcsIraAllocno*) * max_regno);
      }
   ira_free (self->ira_regno_allocno_map);
   self->ira_regno_allocno_map    = (MtcsIraAllocno **) ira_allocate (max_regno * sizeof (MtcsIraAllocno*));
   memset (self->ira_regno_allocno_map, 0, max_regno * sizeof (MtcsIraAllocno*));
   MTCS_FOR_EACH_ALLOCNO(self,a, ai){
      if (a->cap_member != NULL)
         /* Caps are not in the regno allocno maps.  */
         continue;
      regno = a->regno;
      loop_tree_node = a->loop_tree_node;
      a->next_regno_allocno =self->ira_regno_allocno_map[regno];
      self->ira_regno_allocno_map[regno] = a;
      if (loop_tree_node->regno_allocno_map[regno] == NULL)
         /* Remember that we can create temporary allocnos to break
         cycles in register shuffle.  */
         loop_tree_node->regno_allocno_map[regno] = a;
   }
}

/* Initialize data concerning allocnos.  */
//原型 initiate_allocnos ira-build.cc
static void initiate_allocnos (MtcsIraBuild *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   int maxRegNum = mtcs_func_max_reg_num(mtcsFunc);

  self->allocno_vec.create (maxRegNum/*!max_reg_num ()*/ * 2);
  self->ira_allocnos = NULL;
  self->ira_allocnos_num = 0;
  self->ira_objects_num = 0;
  self->ira_object_id_map_vec.create (maxRegNum/*!max_reg_num ()*/ * 2);
  self->ira_object_id_map = NULL;
  self->ira_regno_allocno_map = (MtcsIraAllocno **) ira_allocate (maxRegNum/*!max_reg_num ()*/
                  * sizeof (MtcsIraAllocno *));
  memset (self->ira_regno_allocno_map, 0, maxRegNum/*!max_reg_num ()*/ * sizeof (MtcsIraAllocno *));
}

/* Create and return an object corresponding to a new allocno A.  */
//原型 ira_create_object ira-build
static MtcsIraObject *ira_create_object (MtcsIraBuild *self,MtcsIraAllocno *a, int subword)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   enum reg_class aclass = a->aclass;
   MtcsIraObject *obj =mtcs_ira_object_new(mtcsMode,subword);
   obj->allocno = a; //如何解藕 MtcsIraAllocno MtcsIraObject
   obj->id  = self->ira_objects_num;
   obj->conflict_hard_regs =mtcsIra->x_ira_no_alloc_regs/*!ira_no_alloc_regs*/;
   obj->total_conflict_hard_regs = mtcsIra->x_ira_no_alloc_regs/*!ira_no_alloc_regs*/;
   obj->conflict_hard_regs |= ~mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[aclass];
   obj->total_conflict_hard_regs |= ~mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[aclass];

   self->ira_object_id_map_vec.safe_push (obj);
   self->ira_object_id_map = self->ira_object_id_map_vec.address ();
   self->ira_objects_num =self->ira_object_id_map_vec.length ();

   return obj;
}

/* Create and return the allocno corresponding to REGNO in
   LOOP_TREE_NODE.  Add the allocno to the list of allocnos with the
   same regno if CAP_P is FALSE.  */
//原型 ira_create_allocno ira-int.h ira-build.cc
MtcsIraAllocno *mtcs_ira_build_create_allocno (MtcsIraBuild *self,int regno, bool cap_p,MtcsIraLoopTreeNode *loop_tree_node)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   MtcsIraAllocno *a = mtcs_ira_allocno_new(mtcsMode,regno);
   a->loop_tree_node = loop_tree_node;
   if (! cap_p){
      a->next_regno_allocno = self->ira_regno_allocno_map[regno];
      self->ira_regno_allocno_map[regno] = a;
      if (loop_tree_node->regno_allocno_map[regno] == NULL)
         /* Remember that we can create temporary allocnos to break
         cycles in register shuffle on region borders (see
         ira-emit.cc).  */
         loop_tree_node->regno_allocno_map[regno] = a;
   }

   a->num = self->ira_allocnos_num;
   bitmap_set_bit (loop_tree_node->all_allocnos, a->num);
   self->allocno_vec.safe_push (a);
   self->ira_allocnos = self->allocno_vec.address ();
   self->ira_allocnos_num = self->allocno_vec.length ();

   return a;
}

/* Set up register class for A and update its conflict hard
   registers.  */
//原型 ira_set_allocno_class ira-int.h ira-build.cc
void mtcs_ira_build_set_allocno_class (MtcsIraBuild *self,MtcsIraAllocno *a, enum reg_class aclass)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   MtcsIraAllocnoObjectIterator oi;
   MtcsIraObject *obj;

   a->aclass = aclass;
   MTCS_FOR_EACH_ALLOCNO_OBJECT (a, obj, oi){
      obj->conflict_hard_regs |= ~mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[aclass];
      obj->total_conflict_hard_regs |= ~mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[aclass];
   }
}

/* Determine the number of objects we should associate with allocno A
   and allocate them.  */
//原型 ira_create_allocno_objects
void mtcs_ira_build_create_allocno_objects (MtcsIraBuild *self,MtcsIraAllocno *a)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   machine_mode mode =a->mode;
   enum reg_class aclass = a->aclass;
   int n = mtcsIra->x_ira_reg_class_max_nregs/*!ira_reg_class_max_nregs*/[aclass][mode];
   int i;

   if (n != 2 || maybe_ne (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode), n * UNITS_PER_WORD))
      n = 1;

   a->num_objects = n;
   for (i = 0; i < n; i++)
      a->objects[i] = ira_create_object(self,a, i);
}

/* For each allocno, set ALLOCNO_NUM_OBJECTS and create the
   ALLOCNO_OBJECT structures.  This must be called after the allocno
   classes are known.  */
static void create_allocno_objects (MtcsIraBuild *self)
{
   MtcsIraAllocno *a;
   MtcsIraAllocnoIterator ai;
   //为 ira_allocnos 创建对象 ira_allocnos是全局MtcIraAlloco变量
   MTCS_FOR_EACH_ALLOCNO(self,a, ai)
      mtcs_ira_build_create_allocno_objects/*!ira_create_allocno_objects*/(self,a);
}

/* Merge hard register conflict information for all objects associated with
   allocno TO into the corresponding objects associated with FROM.
   If TOTAL_ONLY is true, we only merge OBJECT_TOTAL_CONFLICT_HARD_REGS.  */
static void merge_hard_reg_conflicts (MtcsIraBuild *self,MtcsIraAllocno *from , MtcsIraAllocno *to, bool total_only)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);

   int i;
   gcc_assert (to->num_objects == from->num_objects);
   for (i = 0; i < to->num_objects; i++){
      MtcsIraObject *from_obj =from->objects[i] ;
      MtcsIraObject * to_obj = to->objects[i] ;

      if (!total_only)
         to_obj->conflict_hard_regs     |=from_obj->conflict_hard_regs;
      to_obj->total_conflict_hard_regs |=   from_obj->total_conflict_hard_regs;
   }
   if(mtcs_config_ifdef(mtcsConfig,MTCS_STACK_REGS)){
      if (!total_only && from->no_stack_reg_p )
         to->no_stack_reg_p = true;
      if (from->total_no_stack_reg_p)
         to->total_no_stack_reg_p = true;
   }
   /*
   #ifdef STACK_REGS
   if (!total_only && ALLOCNO_NO_STACK_REG_P (from))
   ALLOCNO_NO_STACK_REG_P (to) = true;
   if (ALLOCNO_TOTAL_NO_STACK_REG_P (from))
   ALLOCNO_TOTAL_NO_STACK_REG_P (to) = true;
   #endif
   */
}

/* 原型 ior_hard_reg_conflicts ira-int.h ira-build.cc 被mtcs_ira_allocno_ior_hard_reg_conflicts替换*/

/* 原型 ira_conflict_vector_profitable_p ira-int.h ira-build.cc 被mtcs_ira_object_ira_conflict_vector_profitable_p 替换*/

/* 原型 ira_allocate_conflict_vec ira-int.h ira-build.cc 被mtcs_ira_object_ira_allocate_conflict_vec 替换*/

/* 原型 static void allocate_conflict_bit_vec ira-build.cc 被 mtcsiraobject.c static void allocate_conflict_bit_vec 替换*/

/* 原型 ira_allocate_object_conflicts ira-int.h ira-build.cc 被 mtcs_ira_object_allocate_object_conflicts 替换*/

/* 原型 static void add_to_conflicts ira-build.cc 被 mtcsiraobject.c static void add_to_conflicts 替换*/

/* 原型 static void ira_add_conflict ira-build.cc 改为公共方法 mtcs_ira_object_add_conflict 供mtcsirabuild 的方法 ira_flattening调用 */

/* 原型 static void clear_conflicts ira-build.cc 改为公共方法 供mtcsirabuild 的方法 ira_flattening调用 */

/* Remove duplications in conflict vector of OBJ.  */
static void compress_conflict_vec (MtcsIraBuild *self,MtcsIraObject *obj)
{
   MtcsIraObject **vec, *conflict_obj;
   int i, j;

   ira_assert (obj->conflict_vec_p);
   vec = (MtcsIraObject **)obj->conflicts_array ;
   self->curr_conflict_check_tick++;
   for (i = j = 0; (conflict_obj = vec[i]) != NULL; i++){
      int id = conflict_obj->id ;
      if (self->conflict_check[id] != self->curr_conflict_check_tick){
         self->conflict_check[id] = self->curr_conflict_check_tick;
         vec[j++] = conflict_obj;
      }
   }
   obj->num_accumulated_conflicts = j;
   vec[j] = NULL;
}

/* Remove duplications in conflict vectors of all allocnos.  */
static void compress_conflict_vecs (MtcsIraBuild *self)
{
   MtcsIraObject *obj;
   MtcsIraObjectIterator oi;

   self->conflict_check = (int *) ira_allocate (sizeof (int) * self->ira_objects_num);
   memset (self->conflict_check, 0, sizeof (int) * self->ira_objects_num);
   self->curr_conflict_check_tick = 0;
   MTCS_FOR_EACH_OBJECT(self,obj, oi){
      if (obj->conflict_vec_p)
         compress_conflict_vec (self,obj);
   }
   ira_free (self->conflict_check);
}

/* 原型 ira_print_expanded_allocno ira-int.h ira-build.cc 被 mtcs_ira_allocno_print_expanded_allocno 替换 */

/* Create and return the cap representing allocno A in the
   parent loop.  */
//原型 static ira_allocno_t create_cap_allocno ira-build.cc
static MtcsIraAllocno *create_cap_allocno (MtcsIraBuild *self,MtcsIraAllocno *a)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   MtcsIraAllocno *cap;
   MtcsIraLoopTreeNode *parent;
   enum reg_class aclass;

   parent = a->loop_tree_node->parent;
   cap = mtcs_ira_build_create_allocno/*!ira_create_allocno*/(self,a->regno, true, parent);
   cap->mode = a->mode;
   cap->wmode = a->wmode;
   aclass = a->aclass;
   mtcs_ira_build_set_allocno_class/*!ira_set_allocno_class*/(self,cap, aclass);
   mtcs_ira_build_create_allocno_objects/*!ira_create_allocno_objects*/(self,cap);
   cap->cap_member = a;
   a->cap = cap;
   cap->class_cost = a->class_cost;
   cap->memory_cost = a->memory_cost;
   mtcs_ira_build_allocate_and_copy_costs/*!ira_allocate_and_copy_costs*/(self,
     &cap->hard_reg_costs, aclass, a->hard_reg_costs);
   mtcs_ira_build_allocate_and_copy_costs/*!ira_allocate_and_copy_costs*/(self,
     &cap->conflict_hard_reg_costs, aclass,a->conflict_hard_reg_costs);
   cap->bad_spill_p = a->bad_spill_p;
   cap->nrefs = a->nrefs;
   cap->freq = a->freq;
   cap->call_freq = a->call_freq;
   mtcs_ira_allocno_set_register_filter/*!ALLOCNO_SET_REGISTER_FILTERS*/(cap, a->register_filters);

   merge_hard_reg_conflicts(self,a, cap, false);

   cap->calls_crossed_num = a->calls_crossed_num;
   cap->cheap_calls_crossed_num = a->cheap_calls_crossed_num;
   cap->crossed_calls_abis = a->crossed_calls_abis;
   cap->crossed_calls_clobbered_regs = a->crossed_calls_clobbered_regs;
   if (mtcsIraGlobal->internal_flag_ira_verbose > 2 && mtcsIraGlobal->ira_dump_file != NULL){
      fprintf (mtcsIraGlobal->ira_dump_file, "    Creating cap ");
      mtcs_ira_allocno_print_expanded_allocno/*!ira_print_expanded_allocno*/(cap);
      fprintf (mtcsIraGlobal->ira_dump_file, "\n");
   }
   return cap;
}

/* 原型 ira_create_live_range ira-int.h ira-build.cc 被 mtcs_ira_object_create_live_range 替换 */

/* 原型 ira_add_live_range_to_object ira-int.h ira-build.cc 被 mtcs_ira_object_add_live_range_to_object 替换 */

/* 原型 static live_range_t copy_live_range (live_range_t r) ira-build.cc 被 mtcs_live_range_copy_live_range 替换 */

/* 原型 ira_copy_live_range_list ira-int.h ira-build.cc 被 mtcs_live_range_copy_live_range_list 替换 */

/*原型 ira_merge_live_ranges ira-int.h ira-build.cc 被 mtcs_ira_object_merge_live_ranges 替换 */

/*原型 ira_live_ranges_intersect_p ira-int.h ira-build.cc 被 mtcsira_object_live_ranges_intersect_p 替换 */

/*原型 ira_finish_live_range ira-int.h ira-build.cc 被  mtcs_ira_object_finish_live_range 替换 */

/*原型 ira_finish_live_range_list ira-int.h ira-build.cc 被 mtcs_ira_object_finish_live_range_list 替换 */

/* Free updated register costs of allocno A.  */
//原型 ira_free_allocno_updated_costs ira-int.c ira-build.cc
void mtcs_ira_build_free_allocno_updated_costs (MtcsIraBuild *self,MtcsIraAllocno *a)
{
   enum reg_class aclass;

   aclass = a->aclass;
   if (a->updated_hard_reg_costs != NULL)
      mtcs_ira_build_free_cost_vector/*!ira_free_cost_vector*/(self,a->updated_hard_reg_costs, aclass);
   a->updated_hard_reg_costs = NULL;
   if (a->updated_conflict_hard_reg_costs != NULL)
      mtcs_ira_build_free_cost_vector/*!ira_free_cost_vector*/(self,a->updated_conflict_hard_reg_costs,aclass);
   a->updated_conflict_hard_reg_costs = NULL;
}

/* Free and nullify all cost vectors allocated earlier for allocno
   A.  */
static void ira_free_allocno_costs (MtcsIraBuild *self,MtcsIraAllocno *a)
{
   enum reg_class aclass = a->aclass;
   MtcsIraObject * obj;
   MtcsIraAllocnoObjectIterator oi;

   MTCS_FOR_EACH_ALLOCNO_OBJECT (a, obj, oi){
      mtcs_ira_object_finish_live_range_list/*!ira_finish_live_range_list*/(obj->live_ranges);
      self->ira_object_id_map[obj->id] = NULL;
      if (obj->conflicts_array != NULL)
         ira_free (obj->conflicts_array);
      mtcs_ira_object_free/*!object_pool.remove*/(obj);
   }

   self->ira_allocnos[a->num] = NULL;
   if (a->hard_reg_costs != NULL)
      mtcs_ira_build_free_cost_vector/*!ira_free_cost_vector*/(self,a->hard_reg_costs, aclass);
   if (a->conflict_hard_reg_costs != NULL)
      mtcs_ira_build_free_cost_vector/*!ira_free_cost_vector*/(self,a->conflict_hard_reg_costs, aclass);
   if (a->updated_hard_reg_costs != NULL)
      mtcs_ira_build_free_cost_vector/*!ira_free_cost_vector*/(self,a->updated_hard_reg_costs, aclass);
   if (a->updated_conflict_hard_reg_costs != NULL)
      mtcs_ira_build_free_cost_vector/*!ira_free_cost_vector*/(self,a->updated_conflict_hard_reg_costs,aclass);
   a->hard_reg_costs = NULL;
   a->conflict_hard_reg_costs = NULL;
   a->updated_hard_reg_costs = NULL;
   a->updated_conflict_hard_reg_costs = NULL;
}

/* Free the memory allocated for allocno A.  */
static void finish_allocno (MtcsIraBuild *self,MtcsIraAllocno *a)
{
  ira_free_allocno_costs(self,a);
}

/* Free the memory allocated for all allocnos.  */
static void finish_allocnos (MtcsIraBuild *self)
{
   MtcsIraAllocno *a;
   MtcsIraAllocnoIterator ai;

   MTCS_FOR_EACH_ALLOCNO(self,a, ai)
      finish_allocno(self,a);
   ira_free (self->ira_regno_allocno_map);
   self->ira_object_id_map_vec.release ();
   self->allocno_vec.release ();
}

/* The function initializes data concerning allocno prefs.  */
static void initiate_prefs (MtcsIraBuild *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   self->pref_vec.create (mtcs_rtl_data_get_max_uid/*!get_max_uid*/(mtcsRtlData));
   self->ira_prefs = NULL;
   self->ira_prefs_num = 0;
}

/*原型 static ira_pref_t find_allocno_pref (ira_allocno_t a, int hard_regno) ira-build.cc 被 mtcs_ira_allocno_find_allocno_pref 替换*/


/* Create and return pref with given attributes A, HARD_REGNO, and FREQ.  */
//原型 ira_create_pref ira-int.h ira-build.cc
MtcsIraAllocnoPref *mtcs_ira_build_create_pref (MtcsIraBuild *self,MtcsIraAllocno *a, int hard_regno, int freq)
{
   MtcsIraAllocnoPref *pref;
   pref= mtcs_ira_allocno_pref_new();/*!pref_pool.allocate ();*/
   pref->num = self->ira_prefs_num;
   pref->allocno = a;
   pref->hard_regno = hard_regno;
   pref->freq = freq;
   self->pref_vec.safe_push (pref);
   self->ira_prefs = self->pref_vec.address ();
   self->ira_prefs_num = self->pref_vec.length ();
   return pref;
}

/*原型 static void add_allocno_pref_to_list (ira_pref_t pref) ira-build.cc  被 mtcs_ira_allocno_add_allocno_pref_to_list 替换*/

/* Create (or update frequency if the pref already exists) the pref of
   allocnos A preferring HARD_REGNO with frequency FREQ.  */
//原型 ira_add_allocno_pref ira-int.h ira-build.cc
void mcs_ira_build_add_allocno_pref (MtcsIraBuild *self,MtcsIraAllocno *a, int hard_regno, int freq)
{
   MtcsIraAllocnoPref *pref;

   if (freq <= 0)
      return;
   if ((pref = mtcs_ira_allocno_find_allocno_pref/*!find_allocno_pref*/(a, hard_regno)) != NULL){
      pref->freq += freq;
      return;
   }
   pref = mtcs_ira_build_create_pref/*!ira_create_pref*/(self,a, hard_regno, freq);
   ira_assert (a != NULL);
   mtcs_ira_allocno_add_allocno_pref_to_list/*!add_allocno_pref_to_list*/(pref);
}

/* 原型 static void print_pref (FILE *f, ira_pref_t pref) ira-build.cc 被 mtcs_ira_allocno_print_pref 替换*/

/* 原型 ira_debug_pref ira-int.h ira-build.cc   被 mtcs_ira_allocno_debug_pref 替换*/

/* Print info about all prefs into file F.  */
static void print_prefs (MtcsIraBuild *self,FILE *f)
{
   MtcsIraAllocnoPref *pref;
   MtcsIraAllocnoPrefIterator pi;

   MTCS_FOR_EACH_PREF (self,pref, pi)
      mtcs_ira_allocno_print_pref/*!print_pref*/(pref,f);
}

/* Print info about all prefs into stderr.  */
//原型 ira_debug_prefs ira-int.h ira-build.cc
void mtcs_ira_build_debug_prefs (MtcsIraBuild *self)
{
  print_prefs(self,stderr);
}

/* 原型 static void print_allocno_prefs (FILE *f, ira_allocno_t a) ira-build.cc 被 print_allocno_prefs mtcsiraallocno.c 替换*/

/* 原型 ira_debug_allocno_prefs ira-int.h ira-build.cc  被 mtcs_ira_allocno_debug_allocno_prefs 替换*/

/* The function frees memory allocated for PREF.  */
static void finish_pref (MtcsIraBuild *self,MtcsIraAllocnoPref *pref)
{
  self->ira_prefs[pref->num] = NULL;
  mtcs_ira_allocno_pref_free/*!pref_pool.remove (pref)*/(pref);
}

/* Remove PREF from the list of allocno prefs and free memory for
   it.  */
//原型 ira_remove_pref ira-int.h ira-build.cc
void mtcs_ira_build_remove_pref (MtcsIraBuild *self,MtcsIraAllocnoPref *pref)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   MtcsIraAllocnoPref *cpref, *prev;

   if (mtcsIraGlobal->internal_flag_ira_verbose > 1 && mtcsIraGlobal->ira_dump_file != NULL)
      fprintf (mtcsIraGlobal->ira_dump_file, " Removing pref%d:hr%d@%d\n",pref->num, pref->hard_regno, pref->freq);
   for (prev = NULL, cpref = pref->allocno->allocno_prefs;cpref != NULL; prev = cpref, cpref = cpref->next_pref)
      if (cpref == pref)
         break;
   ira_assert (cpref != NULL);
   if (prev == NULL)
      pref->allocno->allocno_prefs = pref->next_pref;
   else
      prev->next_pref = pref->next_pref;
   finish_pref (self,pref);
}

/* Remove all prefs of allocno A.  */
//原型 ira_remove_allocno_prefs ira-int.h ira-build.cc
void mtcs_ira_build_remove_allocno_prefs (MtcsIraBuild *self,MtcsIraAllocno *a)
{
   MtcsIraAllocnoPref *pref, *next_pref;

   for (pref = a->allocno_prefs; pref != NULL; pref = next_pref){
      next_pref = pref->next_pref;
      finish_pref (self,pref);
   }
   a->allocno_prefs = NULL;
}

/* Free memory allocated for all prefs.  */
static void finish_prefs (MtcsIraBuild *self)
{
   MtcsIraAllocnoPref *pref;
   MtcsIraAllocnoPrefIterator pi;

   MTCS_FOR_EACH_PREF (self,pref, pi)
      finish_pref (self,pref);
   self->pref_vec.release ();
}

/* The function initializes data concerning allocno copies.  */
static void initiate_copies (MtcsIraBuild *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   self->copy_vec.create (mtcs_rtl_data_get_max_uid/*!get_max_uid*/(mtcsRtlData));
   self->ira_copies = NULL;
   self->ira_copies_num = 0;
}

/* 原型 static ira_copy_t find_allocno_copy (ira_allocno_t a1, i ... ira-build.cc 被  mtcs_ira_allocno_find_allocno_copy 替换*/

/* Create and return copy with given attributes LOOP_TREE_NODE, FIRST,
   SECOND, FREQ, CONSTRAINT_P, and INSN.  */
//原型 ira_create_copy ira-int.h ira-build.cc
MtcsIraAllocnoCopy *mtcs_ira_build_create_copy (MtcsIraBuild *self,MtcsIraAllocno *first, MtcsIraAllocno *second, int freq,
       bool constraint_p, rtx_insn *insn,MtcsIraLoopTreeNode *loop_tree_node)
{
   MtcsIraAllocnoCopy *cp;
   cp = mtcs_ira_allocno_copy_new();/*!copy_pool.allocate ();*/
   cp->num = ira_copies_num;
   cp->first = first;
   cp->second = second;
   cp->freq = freq;
   cp->constraint_p = constraint_p;
   cp->insn = insn;
   cp->loop_tree_node = loop_tree_node;
   self->copy_vec.safe_push (cp);
   self->ira_copies = self->copy_vec.address ();
   self->ira_copies_num = self->copy_vec.length ();
   return cp;
}

/* 原型 static void add_allocno_copy_to_list (MtcsIraAllocnoCopy *cp) ira-build.cc 被 mtcs_ira_allocno_add_allocno_copy_to_list 替换*/

/* 原型 static void add_allocno_copy_to_list (MtcsIraAllocnoCopy *cp) ira-build.cc 被 mtcs_ira_allocno_add_allocno_copy_to_list 替换*/

/* Create (or update frequency if the copy already exists) and return
   the copy of allocnos FIRST and SECOND with frequency FREQ
   corresponding to move insn INSN (if any) and originated from
   LOOP_TREE_NODE.  */
//原型 ira_add_allocno_copy ira-int.h ira-build.cc
MtcsIraAllocnoCopy *mtcs_ira_build_add_allocno_copy (MtcsIraBuild *self,MtcsIraAllocno *first, MtcsIraAllocno *second, int freq,
            bool constraint_p, rtx_insn *insn, MtcsIraLoopTreeNode *loop_tree_node)
{
   MtcsIraAllocnoCopy *cp;

   if ((cp = mtcs_ira_allocno_find_allocno_copy/*!find_allocno_copy*/(first, second, insn, loop_tree_node)) != NULL){
      cp->freq += freq;
      return cp;
   }
   cp = mtcs_ira_build_create_copy/*!ira_create_copy*/(self,first, second, freq, constraint_p, insn,
   loop_tree_node);
   ira_assert (first != NULL && second != NULL);
   mtcs_ira_allocno_add_allocno_copy_to_list/*!add_allocno_copy_to_list*/(cp);
   mtcs_ira_allocno_swap_allocno_copy_ends_if_necessary/*!swap_allocno_copy_ends_if_necessary*/(cp);
   return cp;
}

//原型 static void print_copy (MtcsIraAllocnoCopy *mtcsIraAllocnoCopy,FILE *f) ira-build.cc 被 mtcs_ira_allocno_print_copy 替换*/

//原型 DEBUG_FUNCTION void debug (ira_allocno_copy &ref) ira-int.h ira-build.cc 被  mtcs_ira_alloc_copy_debug 替换*/

//原型 DEBUG_FUNCTION void debug (ira_allocno_copy *ptr) ira-int.h ira-build.cc 被 mtcs_ira_alloc_copy_debug_1 替换*/

//原型 ira_debug_copy ira-int.h ira-build.cc  被 mtcs_ira_allocno_debug_copy 替换*/

/* Print info about all copies into file F.  */
static void print_copies (MtcsIraBuild *self,FILE *f)
{
   MtcsIraAllocnoCopy *cp;
   MtcsIraAllocnoCopyIterator ci;

   MTCS_FOR_EACH_COPY (self,cp, ci)
      mtcs_ira_allocno_print_copy/*!print_copy*/(cp,f);
}

/* Print info about all copies into stderr.  */
//原型 ira_debug_copies ira-int.h ira-build.cc
void mtcs_ira_build_debug_copies (MtcsIraBuild *self)
{
  print_copies (self,stderr);
}

/* 原型 static void print_allocno_copies (FILE *f, ira_allocno_t a) ira-build.cc 被 mtcs_ira_allocno_print_allocno_copies 替换*/

/* 原型 debug ira-int.h ira-build.cc  被 mtcs_ira_allocno_debug 替换*/

/* 原型 ira_debug_allocno_copies ira-int.h ira-build.cc 被 mtcs_ira_allocno_debug_allocno_copies 替换*/

/* The function frees memory allocated for copy CP.  */
static void finish_copy (MtcsIraAllocnoCopy *cp)
{
   mtcs_ira_allocno_copy_free/*!copy_pool.remove (cp)*/(cp);
}

/* Free memory allocated for all copies.  */
static void finish_copies (MtcsIraBuild *self)
{
   MtcsIraAllocnoCopy *cp;
   MtcsIraAllocnoCopyIterator ci;

   MTCS_FOR_EACH_COPY (self,cp, ci)
      finish_copy (cp);
   self->copy_vec.release ();
}



/* The function initiates work with hard register cost vectors.  It
   creates allocation pool for each allocno class.  */
static void initiate_cost_vectors (MtcsIraBuild *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int i;
   enum reg_class aclass;

   for (i = 0; i < mtcsIra->x_ira_allocno_classes_num; i++){
      aclass = mtcsIra->x_ira_allocno_classes[i];
      self->cost_vector_pool[aclass] = new pool_allocator("cost vectors",
            sizeof (int) * (mtcsIra->x_ira_class_hard_regs_num[aclass]));
   }
}

/* Allocate cost vector *VEC for hard registers of ACLASS and copy
   values of vector SRC into the vector if it is necessary */
//原型 ira_allocate_and_copy_costs ira-int.h
void mtcs_ira_build_allocate_and_copy_costs (MtcsIraBuild *self,int **vec, enum reg_class aclass, int *src)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int len;
   if (*vec != NULL || src == NULL)
      return;
   *vec = mtcs_ira_build_allocate_cost_vector/*!ira_allocate_cost_vector*/(self,aclass);
   len = mtcsIra->x_ira_class_hard_regs_num[aclass];
   memcpy (*vec, src, sizeof (int) * len);
}

/* Allocate and return a cost vector VEC for ACLASS.  */
//原型 ira_allocate_cost_vector ira-int.h ira-build.cc
int *mtcs_ira_build_allocate_cost_vector (MtcsIraBuild *self,reg_class_t aclass)
{
  return (int*) self->cost_vector_pool[(int) aclass]->allocate ();
}


/* Free a cost vector VEC for ACLASS.  */
//原型 ira_free_cost_vector ira-int.h ira-build.cc
void mtcs_ira_build_free_cost_vector (MtcsIraBuild *self,int *vec, reg_class_t aclass)
{
  ira_assert (vec != NULL);
  self->cost_vector_pool[(int) aclass]->remove (vec);
}

/* Finish work with hard register cost vectors.  Release allocation
   pool for each allocno class.  */
static void finish_cost_vectors (MtcsIraBuild *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int i;
   enum reg_class aclass;

   for (i = 0; i < mtcsIra->x_ira_allocno_classes_num; i++){
      aclass = mtcsIra->x_ira_allocno_classes[i];
      delete self->cost_vector_pool[aclass];
   }
}

/* Compute a post-ordering of the reverse control flow of the loop body
   designated by the children nodes of LOOP_NODE, whose body nodes in
   pre-order are input as LOOP_PREORDER.  Return a VEC with a post-order
   of the reverse loop body.

   For the post-order of the reverse CFG, we visit the basic blocks in
   LOOP_PREORDER array in the reverse order of where they appear.
   This is important: We do not just want to compute a post-order of
   the reverse CFG, we want to make a best-guess for a visiting order that
   minimizes the number of chain elements per allocno live range.  If the
   blocks would be visited in a different order, we would still compute a
   correct post-ordering but it would be less likely that two nodes
   connected by an edge in the CFG are neighbors in the topsort.  */

static vec<MtcsIraLoopTreeNode *> ira_loop_tree_body_rev_postorder (MtcsIraBuild *self,
      MtcsIraLoopTreeNode * loop_node ATTRIBUTE_UNUSED, const vec<MtcsIraLoopTreeNode *> &loop_preorder)
{
   vec<MtcsIraLoopTreeNode *> topsort_nodes = vNULL;
   unsigned int n_loop_preorder;

   n_loop_preorder = loop_preorder.length ();
   if (n_loop_preorder != 0){
      MtcsIraLoopTreeNode * subloop_node;
      unsigned int i;
      auto_vec<MtcsIraLoopTreeNode *> dfs_stack;

         /* This is a bit of strange abuse of the BB_VISITED flag:  We use
         the flag to mark blocks we still have to visit to add them to
         our post-order.  Define an alias to avoid confusion.  */
#define BB_TO_VISIT BB_VISITED

      FOR_EACH_VEC_ELT (loop_preorder, i, subloop_node){
         gcc_checking_assert (! (subloop_node->bb->flags & BB_TO_VISIT));
         subloop_node->bb->flags |= BB_TO_VISIT;
      }

      topsort_nodes.create (n_loop_preorder);
      dfs_stack.create (n_loop_preorder);

      FOR_EACH_VEC_ELT_REVERSE (loop_preorder, i, subloop_node){
         if (! (subloop_node->bb->flags & BB_TO_VISIT))
            continue;

         subloop_node->bb->flags &= ~BB_TO_VISIT;
         dfs_stack.quick_push (subloop_node);
         while (! dfs_stack.is_empty ()){
            edge e;
            edge_iterator ei;

            MtcsIraLoopTreeNode * n = dfs_stack.last ();
            FOR_EACH_EDGE (e, ei, n->bb->preds){
               MtcsIraLoopTreeNode * pred_node;
               basic_block pred_bb = e->src;

               if (e->src == ENTRY_BLOCK_PTR_FOR_FN (cfun))
                  continue;
               MtcsIraBuild *mtcsIraBuild=self;
               pred_node = MTCS_IRA_BB_NODE_BY_INDEX (pred_bb->index);
               if (pred_node != n  && (pred_node->bb->flags & BB_TO_VISIT)){
                  pred_node->bb->flags &= ~BB_TO_VISIT;
                  dfs_stack.quick_push (pred_node);
               }
            }
            if (n == dfs_stack.last ()){
               dfs_stack.pop ();
               topsort_nodes.quick_push (n);
            }
         }
      }

#undef BB_TO_VISIT
   }

   gcc_assert (topsort_nodes.length () == n_loop_preorder);
   return topsort_nodes;
}


/* This recursive function traverses loop tree with root LOOP_NODE
   calling non-null functions PREORDER_FUNC and POSTORDER_FUNC
   correspondingly in preorder and postorder.  The function sets up
   IRA_CURR_LOOP_TREE_NODE and IRA_CURR_REGNO_ALLOCNO_MAP.  If BB_P,
   basic block nodes of LOOP_NODE is also processed (before its
   subloop nodes).

   If BB_P is set and POSTORDER_FUNC is given, the basic blocks in
   the loop are passed in the *reverse* post-order of the *reverse*
   CFG.  This is only used by ira_create_allocno_live_ranges, which
   wants to visit basic blocks in this order to minimize the number
   of elements per live range chain.
   Note that the loop tree nodes are still visited in the normal,
   forward post-order of  the loop tree.  */
//原型 ira_traverse_loop_tree ira-int.h ira-build.cc
void mtcs_ira_build_traverse_loop_tree (MtcsIraBuild *self,bool bb_p, MtcsIraLoopTreeNode * loop_node,
         void (*preorder_func) (MtcsIraLoopTreeNode *,void *), void (*postorder_func) (MtcsIraLoopTreeNode *,void *),void *userData)
{
   MtcsIraLoopTreeNode * subloop_node;

   ira_assert (loop_node->bb == NULL);
   self->ira_curr_loop_tree_node = loop_node;
   self->ira_curr_regno_allocno_map = self->ira_curr_loop_tree_node->regno_allocno_map;

   if (preorder_func != NULL)
      (*preorder_func) (loop_node,userData);

   if (bb_p){
      auto_vec<MtcsIraLoopTreeNode *> loop_preorder;
      unsigned int i;

      /* Add all nodes to the set of nodes to visit.  The IRA loop tree
      is set up such that nodes in the loop body appear in a pre-order
      of their place in the CFG.  */
      for (subloop_node = loop_node->children; subloop_node != NULL; subloop_node = subloop_node->next)
         if (subloop_node->bb != NULL)
            loop_preorder.safe_push (subloop_node);

      if (preorder_func != NULL)
         FOR_EACH_VEC_ELT (loop_preorder, i, subloop_node)
            (*preorder_func) (subloop_node,userData);

      if (postorder_func != NULL){
         vec<MtcsIraLoopTreeNode *> loop_rev_postorder =  ira_loop_tree_body_rev_postorder(self,loop_node, loop_preorder);
         FOR_EACH_VEC_ELT_REVERSE (loop_rev_postorder, i, subloop_node)
            (*postorder_func) (subloop_node,userData);
         loop_rev_postorder.release ();
      }
   }

   for (subloop_node = loop_node->subloops;subloop_node != NULL;subloop_node = subloop_node->subloop_next){
      ira_assert (subloop_node->bb == NULL);
      mtcs_ira_build_traverse_loop_tree/*!ira_traverse_loop_tree*/(self,bb_p, subloop_node, preorder_func, postorder_func,userData);
   }

   self->ira_curr_loop_tree_node = loop_node;
   self->ira_curr_regno_allocno_map = self->ira_curr_loop_tree_node->regno_allocno_map;

   if (postorder_func != NULL)
      (*postorder_func) (loop_node,userData);
}

/* This recursive function creates allocnos corresponding to
   pseudo-registers containing in X.  True OUTPUT_P means that X is
   an lvalue.  OUTER corresponds to the parent expression of X.  */
static void create_insn_allocnos (MtcsIraBuild *self,rtx x, rtx outer, bool output_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int i, j;
   const char *fmt;
   enum rtx_code code = GET_CODE (x);

   if (code == REG){
      int regno;
      if ((regno = REGNO (x)) >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)){
         MtcsIraAllocno * a;

         if ((a = self->ira_curr_regno_allocno_map[regno]) == NULL){
            a = mtcs_ira_build_create_allocno/*!ira_create_allocno*/(self,regno, false, self->ira_curr_loop_tree_node);
            if (outer != NULL && GET_CODE (outer) == SUBREG){
               machine_mode wmode = GET_MODE (outer);
               if (mtcs_mode_partial_subreg_p/*!partial_subreg_p*/(mtcsMode,a->wmode, wmode))
                  a->wmode = wmode;
            }
         }
         a->nrefs++;
         a->freq += REG_FREQ_FROM_BB (self->curr_bb);
         if (output_p)
            bitmap_set_bit (self->ira_curr_loop_tree_node->modified_regnos, regno);
      }
      return;
   }else if (code == SET){
      create_insn_allocnos(self,SET_DEST (x), NULL, true);
      create_insn_allocnos(self,SET_SRC (x), NULL, false);
      return;
   }else if (code == CLOBBER){
      create_insn_allocnos(self,XEXP (x, 0), NULL, true);
      return;
   }else if (code == MEM){
      create_insn_allocnos(self,XEXP (x, 0), NULL, false);
      return;
   }else if (code == PRE_DEC || code == POST_DEC || code == PRE_INC ||
      code == POST_INC || code == POST_MODIFY || code == PRE_MODIFY){
      create_insn_allocnos(self,XEXP (x, 0), NULL, true);
      create_insn_allocnos(self,XEXP (x, 0), NULL, false);
      return;
   }

   fmt = GET_RTX_FORMAT (code);
   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--){
      if (fmt[i] == 'e')
         create_insn_allocnos(self,XEXP (x, i), x, output_p);
      else if (fmt[i] == 'E')
         for (j = 0; j < XVECLEN (x, i); j++)
            create_insn_allocnos(self,XVECEXP (x, i, j), x, output_p);
   }
}

/* Create allocnos corresponding to pseudo-registers living in the
   basic block represented by the corresponding loop tree node
   BB_NODE.  */
static void create_bb_allocnos (MtcsIraBuild *self,MtcsIraLoopTreeNode *bb_node)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   basic_block bb;
   rtx_insn *insn;
   unsigned int i;
   bitmap_iterator bi;

   self->curr_bb = bb = bb_node->bb;
   ira_assert (bb != NULL);
   FOR_BB_INSNS_REVERSE (bb, insn)
   if (NONDEBUG_INSN_P (insn))
      create_insn_allocnos(self,PATTERN (insn), NULL, false);
   /* It might be a allocno living through from one subloop to
   another.  */
   EXECUTE_IF_SET_IN_REG_SET (df_get_live_in (bb), mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg), i, bi)
      if (self->ira_curr_regno_allocno_map[i] == NULL)
         mtcs_ira_build_create_allocno/*!ira_create_allocno*/(self,i, false, self->ira_curr_loop_tree_node);
}

/* Create allocnos corresponding to pseudo-registers living on edge E
   (a loop entry or exit).  Also mark the allocnos as living on the
   loop border. */
static void create_loop_allocnos (MtcsIraBuild *self,edge e)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   unsigned int i;
   bitmap live_in_regs, border_allocnos;
   bitmap_iterator bi;
   MtcsIraLoopTreeNode * parent;

   live_in_regs = df_get_live_in (e->dest);
   border_allocnos = self->ira_curr_loop_tree_node->border_allocnos;
   EXECUTE_IF_SET_IN_REG_SET (df_get_live_out (e->src), mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg), i, bi)
      if (bitmap_bit_p (live_in_regs, i)){
         if (self->ira_curr_regno_allocno_map[i] == NULL){
            /* The order of creations is important for right
            ira_regno_allocno_map.  */
            if ((parent = self->ira_curr_loop_tree_node->parent) != NULL && parent->regno_allocno_map[i] == NULL)
               mtcs_ira_build_create_allocno/*!ira_create_allocno*/(self,i, false, parent);
            mtcs_ira_build_create_allocno/*!ira_create_allocno*/(self,i, false, self->ira_curr_loop_tree_node);
         }
         bitmap_set_bit (border_allocnos,self->ira_curr_regno_allocno_map[i]->num );
      }
}

/* Create allocnos corresponding to pseudo-registers living in loop
   represented by the corresponding loop tree node LOOP_NODE.  This
   function is called by ira_traverse_loop_tree.  */
static void createLoopTreeNodeAllocnos_cb (MtcsIraLoopTreeNode * loop_node,void *userData)
{
   MtcsIraBuild *self = (MtcsIraBuild *)userData;
   if (loop_node->bb != NULL)
      create_bb_allocnos(self,loop_node);
   else if (loop_node != self->ira_loop_tree_root){
      int i;
      edge_iterator ei;
      edge e;

      ira_assert (current_loops != NULL);
      FOR_EACH_EDGE (e, ei, loop_node->loop->header->preds)
         if (e->src != loop_node->loop->latch)
            create_loop_allocnos(self,e);

      auto_vec<edge> edges = get_loop_exit_edges (loop_node->loop);
      FOR_EACH_VEC_ELT (edges, i, e)
         create_loop_allocnos(self,e);
   }
}

/* Propagate information about allocnos modified inside the loop given
   by its LOOP_TREE_NODE to its parent.  */
static void  propagateModifiedRegnos_cb  (MtcsIraLoopTreeNode * loop_tree_node,void *userData)
{
   MtcsIraBuild *self = (MtcsIraBuild *)userData;

   if (loop_tree_node == self->ira_loop_tree_root)
      return;
   ira_assert (loop_tree_node->bb == NULL);
   bitmap_ior_into (loop_tree_node->parent->modified_regnos,loop_tree_node->modified_regnos);
}

/* Propagate ALLOCNO_HARD_REG_COSTS from A to PARENT_A.  Use SPILL_COST
   as the cost of spilling a register throughout A (which we have to do
   for PARENT_A allocations that conflict with A).  */
static void ira_propagate_hard_reg_costs (MtcsIraBuild *self,MtcsIraAllocno * parent_a, MtcsIraAllocno * a,int spill_cost)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   HardRegSet conflicts = mtcs_ira_allocno_total_conflict_hard_regs/*!ira_total_conflict_hard_regs*/(a);
   if (mtcs_ira_caller_save_loop_spill_p/*!ira_caller_save_loop_spill_p*/(mtcsIra,parent_a, a, spill_cost))
      conflicts |= mtcs_ira_allocno_need_caller_save_regs/*!ira_need_caller_save_regs*/(a);
   conflicts &= ~mtcs_ira_allocno_total_conflict_hard_regs/*!ira_total_conflict_hard_regs*/(parent_a);

   auto costs = a->hard_reg_costs;
   if (!mtcs_reg_hard_reg_set_empty_p/*!hard_reg_set_empty_p*/(&conflicts))
      a->might_conflict_with_parent_p = true;
   else if (!costs)
      return;

   auto aclass =a->aclass;
   mtcs_ira_build_allocate_and_set_costs/*!ira_allocate_and_set_costs*/(self,&parent_a->hard_reg_costs,
   aclass, parent_a->class_cost);
   auto parent_costs = parent_a->hard_reg_costs;
   for (int i = 0; i < mtcsIra->x_ira_class_hard_regs_num[aclass]; ++i)
      if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&conflicts, mtcsIra->x_ira_class_hard_regs[aclass][i]))
         parent_costs[i] += spill_cost;
      else if (costs)
      /* The cost to A of allocating this register to PARENT_A can't
      be more than the cost of spilling the register throughout A.  */
      parent_costs[i] += MIN (costs[i], spill_cost);
}

/* Propagate new info about allocno A (see comments about accumulated
   info in allocno definition) to the corresponding allocno on upper
   loop tree level.  So allocnos on upper levels accumulate
   information about the corresponding allocnos in nested regions.
   The new info means allocno info finally calculated in this
   file.  */
static void propagate_allocno_info (MtcsIraBuild *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int i;
   MtcsIraAllocno * a, *parent_a;
   MtcsIraLoopTreeNode * parent;
   enum reg_class aclass;

   if (mtcsOptionsItem->x_flag_ira_region != IRA_REGION_ALL
   && mtcsOptionsItem->x_flag_ira_region != IRA_REGION_MIXED)
      return;
   for (i = max_reg_num () - 1; i >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i--)
      for (a = self->ira_regno_allocno_map[i]; a != NULL; a = a->next_regno_allocno)
         if ((parent = a->loop_tree_node->parent) != NULL
            && (parent_a = parent->regno_allocno_map[i]) != NULL
            /* There are no caps yet at this point.  So use
            border_allocnos to find allocnos for the propagation.  */
            && bitmap_bit_p (a->loop_tree_node->border_allocnos,a->num)){
            /* Calculate the cost of storing to memory on entry to A's loop,
            referencing as memory within A's loop, and restoring from
            memory on exit from A's loop.  */
            mtcs_ira_loop_border_costs border_costs (mtcsIra,a);
            int spill_cost = INT_MAX;
            if (mtcs_ira_subloop_allocnos_can_differ_p/*!ira_subloop_allocnos_can_differ_p*/(mtcsIra,parent_a))
               spill_cost = (border_costs.spill_inside_loop_cost () + a->memory_cost);

            if (! a->bad_spill_p)
               parent_a->bad_spill_p = false;
            parent_a->nrefs += a->nrefs;
            parent_a->freq += a->freq;
            ALLOCNO_SET_REGISTER_FILTERS (parent_a,parent_a->register_filters | a->register_filters);

            /* If A's allocation can differ from PARENT_A's, we can if necessary
            spill PARENT_A on entry to A's loop and restore it afterwards.
            Doing that has cost SPILL_COST.  */
            if (!mtcs_ira_subloop_allocnos_can_differ_p/*!ira_subloop_allocnos_can_differ_p*/(mtcsIra,parent_a))
               merge_hard_reg_conflicts(self,a, parent_a, true);

            if (!mtcs_ira_caller_save_loop_spill_p/*!ira_caller_save_loop_spill_p*/(mtcsIra,parent_a, a, spill_cost)){
               parent_a->call_freq += a->call_freq;
               parent_a->calls_crossed_num += a->calls_crossed_num;
               parent_a->cheap_calls_crossed_num  += a->cheap_calls_crossed_num;
               parent_a->crossed_calls_abis |= a->crossed_calls_abis;
               parent_a->crossed_calls_clobbered_regs |= a->crossed_calls_clobbered_regs;
            }
            parent_a->excess_pressure_points_num += a->excess_pressure_points_num;
            aclass = a->aclass;
            ira_assert (aclass == parent_a->aclass);
            ira_propagate_hard_reg_costs(self,parent_a, a, spill_cost);
            mtcs_ira_build_allocate_and_accumulate_costs/*!ira_allocate_and_accumulate_costs*/(self,
                  &parent_a->conflict_hard_reg_costs, aclass, a->conflict_hard_reg_costs);
            /* The cost to A of allocating a register to PARENT_A can't be
            more than the cost of spilling the register throughout A.  */
            parent_a->class_cost  += MIN (a->class_cost, spill_cost);
            parent_a->memory_cost += a->memory_cost;
         }
}

/* Create allocnos corresponding to pseudo-registers in the current
   function.  Traverse the loop tree for this.  */
static void create_allocnos (MtcsIraBuild *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  /* We need to process BB first to correctly link allocnos by member
     next_regno_allocno.  */
   mtcs_ira_build_traverse_loop_tree/*!ira_traverse_loop_tree*/(self,true, self->ira_loop_tree_root,
         createLoopTreeNodeAllocnos_cb, NULL,(void*)self);
   if (mtcsOptionsItem->x_optimize)
     mtcs_ira_build_traverse_loop_tree/*!ira_traverse_loop_tree*/(self,false, self->ira_loop_tree_root, NULL,
           propagateModifiedRegnos_cb,(void*)self);
}

/* The page contains function to remove some regions from a separate
   register allocation.  We remove regions whose separate allocation
   will hardly improve the result.  As a result we speed up regional
   register allocation.  */

/* The function changes the object in range list given by R to OBJ.  */
static void change_object_in_range_list (MtcsLiveRange * r, MtcsIraObject *obj)
{
  for (; r != NULL; r = r->next)
    r->object = obj;
}

/* Move all live ranges associated with allocno FROM to allocno TO.  */
static void move_allocno_live_ranges (MtcsIraBuild *self,MtcsIraAllocno * from, MtcsIraAllocno * to)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraLives *mtcsIraLives = mtcs_ira_mgr_get_lives(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int i;
   int n = from->num_objects;
   gcc_assert (n == to->num_objects);
   for (i = 0; i < n; i++){
      MtcsIraObject *from_obj =from->objects[i];
      MtcsIraObject *to_obj =to->objects[i];
      MtcsLiveRange *lr = from_obj->live_ranges;

      if (mtcsIraGlobal->internal_flag_ira_verbose > 4 && mtcsIraGlobal->ira_dump_file != NULL){
         fprintf (mtcsIraGlobal->ira_dump_file,"      Moving ranges of a%dr%d to a%dr%d: ",from->num, from->regno,to->num,to->regno);
         mtcs_ira_object_print_live_range_list/*!ira_print_live_range_list*/(lr,mtcsIraGlobal->ira_dump_file);
      }
      change_object_in_range_list (lr, to_obj);
      to_obj->live_ranges = mtcs_ira_object_merge_live_ranges/*!ira_merge_live_ranges*/(lr, to_obj->live_ranges);
      from_obj->live_ranges = NULL;
   }
}

static void copy_allocno_live_ranges (MtcsIraBuild *self,MtcsIraAllocno * from, MtcsIraAllocno * to)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraLives *mtcsIraLives = mtcs_ira_mgr_get_lives(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int i;
   int n = from->num_objects;

   gcc_assert (n == to->num_objects);

   for (i = 0; i < n; i++){
      MtcsIraObject * from_obj = from->objects[i];
      MtcsIraObject * to_obj = to->objects[i];
      MtcsLiveRange * lr = from_obj->live_ranges;

      if (mtcsIraGlobal->internal_flag_ira_verbose > 4 && mtcsIraGlobal->ira_dump_file != NULL){
         fprintf (ira_dump_file, "      Copying ranges of a%dr%d to a%dr%d: ", from->num, from->regno,to->num, to->regno);
         mtcs_ira_object_print_live_range_list/*!ira_print_live_range_list*/(lr,mtcsIraGlobal->ira_dump_file);
      }
      lr = mtcs_live_range_copy_live_range_list/*!ira_copy_live_range_list*/(lr);
      change_object_in_range_list (lr, to_obj);
      to_obj->live_ranges = mtcs_ira_object_merge_live_ranges/*!ira_merge_live_ranges*/(lr, to_obj->live_ranges);
   }
}

/* Return TRUE if NODE represents a loop with low register
   pressure.  */
static bool low_pressure_loop_node_p (MtcsIraBuild *self,MtcsIraLoopTreeNode * node)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int i;
   enum reg_class pclass;
   if (node->bb != NULL)
      return false;
   for (i = 0; i <mtcsIra->x_ira_pressure_classes_num; i++){
      pclass = mtcsIra->x_ira_pressure_classes[i];
      if (node->reg_pressure[pclass] > mtcsIra->x_ira_class_hard_regs_num[pclass]
      && mtcsIra->x_ira_class_hard_regs_num[pclass] > 1)
         return false;
   }
   return true;
}

//#ifdef STACK_REGS
/* Return TRUE if LOOP has a complex enter or exit edge.  We don't
   form a region from such loop if the target use stack register
   because reg-stack.cc cannot deal with such edges.  */
static bool loop_with_complex_edge_p (class loop *loop)
{
   int i;
   edge_iterator ei;
   edge e;
   bool res;

   FOR_EACH_EDGE (e, ei, loop->header->preds)
      if (e->flags & EDGE_EH)
         return true;
   auto_vec<edge> edges = get_loop_exit_edges (loop);
   res = false;
   FOR_EACH_VEC_ELT (edges, i, e)
      if (e->flags & EDGE_COMPLEX){
         res = true;
         break;
      }
   return res;
}
//#endif

/* Sort loops for marking them for removal.  We put already marked
   loops first, then less frequent loops next, and then outer loops
   next.  */
static int loop_compare_func (const void *v1p, const void *v2p)
{
   int diff;
   MtcsIraLoopTreeNode * l1 = *(const MtcsIraLoopTreeNode * *) v1p;
   MtcsIraLoopTreeNode * l2 = *(const MtcsIraLoopTreeNode * *) v2p;

   ira_assert (l1->parent != NULL && l2->parent != NULL);
   if (l1->to_remove_p && ! l2->to_remove_p)
      return -1;
   if (! l1->to_remove_p && l2->to_remove_p)
      return 1;
   if ((diff = l1->loop->header->count.to_frequency (cfun) - l2->loop->header->count.to_frequency (cfun)) != 0)
      return diff;
   if ((diff = (int) loop_depth (l1->loop) - (int) loop_depth (l2->loop)) != 0)
      return diff;
   /* Make sorting stable.  */
   return l1->loop_num - l2->loop_num;
}

/* Mark loops which should be removed from regional allocation.  We
   remove a loop with low register pressure inside another loop with
   register pressure.  In this case a separate allocation of the loop
   hardly helps (for irregular register file architecture it could
   help by choosing a better hard register in the loop but we prefer
   faster allocation even in this case).  We also remove cheap loops
   if there are more than param_ira_max_loops_num of them.  Loop with EH
   exit or enter edges are removed too because the allocation might
   require put pseudo moves on the EH edges (we could still do this
   for pseudos with caller saved hard registers in some cases but it
   is impossible to say here or during top-down allocation pass what
   hard register the pseudos get finally).  */
static void mark_loops_for_removal (MtcsIraBuild *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int i, n;
   MtcsIraLoopTreeNode **sorted_loops;
   loop_p loop;

   ira_assert (current_loops != NULL);
   sorted_loops = (MtcsIraLoopTreeNode **) ira_allocate (sizeof (MtcsIraLoopTreeNode *) * number_of_loops (cfun));
   for (n = i = 0; vec_safe_iterate (get_loops (cfun), i, &loop); i++)
      if (self->ira_loop_nodes[i]->regno_allocno_map != NULL){
         if (self->ira_loop_nodes[i]->parent == NULL){
            /* Don't remove the root.  */
            self->ira_loop_nodes[i]->to_remove_p = false;
            continue;
         }
         sorted_loops[n++] = self->ira_loop_nodes[i];
         //#ifdef STACK_REGS 被 mtcs_config_ifdef(mtcsConfig,MTCS_STACK_REGS)替换

         if(mtcs_config_ifdef(mtcsConfig,MTCS_STACK_REGS))
            self->ira_loop_nodes[i]->to_remove_p  = ((low_pressure_loop_node_p(self,self->ira_loop_nodes[i]->parent)
                 && low_pressure_loop_node_p(self,self->ira_loop_nodes[i]))
                 || loop_with_complex_edge_p (self->ira_loop_nodes[i]->loop));
         else
            self->ira_loop_nodes[i]->to_remove_p  = ( (low_pressure_loop_node_p(self,self->ira_loop_nodes[i]->parent)
                  && low_pressure_loop_node_p(self,self->ira_loop_nodes[i])));
      }

   qsort (sorted_loops, n, sizeof (MtcsIraLoopTreeNode *), loop_compare_func);
   for (i = 0; i < n - mtcsOptionsItem->x_param_ira_max_loops_num; i++){
      sorted_loops[i]->to_remove_p = true;
      if (mtcsIraGlobal->internal_flag_ira_verbose > 1 && mtcsIraGlobal->ira_dump_file != NULL)
         fprintf(mtcsIraGlobal->ira_dump_file,"  Mark loop %d (header %d, freq %d, depth %d) for removal (%s)\n",
               sorted_loops[i]->loop_num, sorted_loops[i]->loop->header->index,
               sorted_loops[i]->loop->header->count.to_frequency (cfun),
               loop_depth (sorted_loops[i]->loop),
               low_pressure_loop_node_p(self,sorted_loops[i]->parent)
               && low_pressure_loop_node_p(self,sorted_loops[i]) ? "low pressure" : "cheap loop");
   }
   ira_free (sorted_loops);
}

/* Mark all loops but root for removing.  */
static void mark_all_loops_for_removal (MtcsIraBuild *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int i;
   loop_p loop;

   ira_assert (current_loops != NULL);
   FOR_EACH_VEC_SAFE_ELT (get_loops (cfun), i, loop)
      if (self->ira_loop_nodes[i]->regno_allocno_map != NULL){
         if (self->ira_loop_nodes[i]->parent == NULL){
            /* Don't remove the root.  */
            self->ira_loop_nodes[i]->to_remove_p = false;
            continue;
         }
         self->ira_loop_nodes[i]->to_remove_p = true;
         if (mtcsIraGlobal->internal_flag_ira_verbose > 1 && mtcsIraGlobal->ira_dump_file != NULL)
            fprintf(mtcsIraGlobal->ira_dump_file,"  Mark loop %d (header %d, freq %d, depth %d) for removal\n",
                  self->ira_loop_nodes[i]->loop_num,
                  self->ira_loop_nodes[i]->loop->header->index,
                  self->ira_loop_nodes[i]->loop->header->count.to_frequency (cfun),
                  loop_depth (self->ira_loop_nodes[i]->loop));
      }
}


/* Remove subregions of NODE if their separate allocation will not
   improve the result.  */
static void remove_uneccesary_loop_nodes_from_loop_tree (MtcsIraBuild *self,MtcsIraLoopTreeNode * node)
{
   unsigned int start;
   bool remove_p;
   MtcsIraLoopTreeNode * subnode;

   remove_p = node->to_remove_p;
   if (! remove_p)
      self->children_vec.safe_push (node);
   start = self->children_vec.length ();
   for (subnode = node->children; subnode != NULL; subnode = subnode->next)
      if (subnode->bb == NULL)
         remove_uneccesary_loop_nodes_from_loop_tree(self,subnode);
      else
         self->children_vec.safe_push (subnode);
   node->children = node->subloops = NULL;
   if (remove_p){
      self->removed_loop_vec.safe_push (node);
      return;
   }
   while (self->children_vec.length () > start){
      subnode = self->children_vec.pop ();
      subnode->parent = node;
      subnode->next = node->children;
      node->children = subnode;
      if (subnode->bb == NULL){
         subnode->subloop_next = node->subloops;
         node->subloops = subnode;
      }
   }
}

/* 原型 static bool loop_is_inside_p (MtcsIraLoopTreeNode * node, MtcsIraLoopTreeNode * parent) ira-build.cc 被
bool mtcs_ira_loop_tree_node_loop_is_inside_p (MtcsIraLoopTreeNode * self, MtcsIraLoopTreeNode * parent);替换*/

/* Sort allocnos according to their order in regno allocno list.  */
static int regno_allocno_order_compare_func (const void *v1p, const void *v2p)
{
   MtcsIraAllocno * a1 = *(const MtcsIraAllocno * *) v1p;
   MtcsIraAllocno * a2 = *(const MtcsIraAllocno * *) v2p;
   MtcsIraLoopTreeNode * n1 = a1->loop_tree_node;
   MtcsIraLoopTreeNode * n2 = a2->loop_tree_node;

   if (mtcs_ira_loop_tree_node_loop_is_inside_p/*!loop_is_inside_p*/(n1, n2))
      return -1;
   else if (mtcs_ira_loop_tree_node_loop_is_inside_p/*!loop_is_inside_p*/(n2, n1))
      return 1;
   /* If allocnos are equally good, sort by allocno numbers, so that
   the results of qsort leave nothing to chance.  We put allocnos
   with higher number first in the list because it is the original
   order for allocnos from loops on the same levels.  */
   return a2->num - a1->num;
}

/* Restore allocno order for REGNO in the regno allocno list.  */
static void ira_rebuild_regno_allocno_list (MtcsIraBuild *self,int regno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int i, n;
   MtcsIraAllocno * a;

   for (n = 0, a = self->ira_regno_allocno_map[regno]; a != NULL; a = a->next_regno_allocno)
      self->regno_allocnos[n++] = a;
   ira_assert (n > 0);
   qsort (self->regno_allocnos, n, sizeof (MtcsIraAllocno *),regno_allocno_order_compare_func);
   for (i = 1; i < n; i++)
      self->regno_allocnos[i - 1]->next_regno_allocno = self->regno_allocnos[i];
   self->regno_allocnos[n - 1]->next_regno_allocno = NULL;
   self->ira_regno_allocno_map[regno] = self->regno_allocnos[0];
   if (mtcsIraGlobal->internal_flag_ira_verbose > 1 && mtcsIraGlobal->ira_dump_file != NULL)
      fprintf (mtcsIraGlobal->ira_dump_file, " Rebuilding regno allocno list for %d\n", regno);
}

/* Propagate info from allocno FROM_A to allocno A.  */
static void propagate_some_info_from_allocno (MtcsIraBuild *self,MtcsIraAllocno * a, MtcsIraAllocno * from_a)
{
   enum reg_class aclass;

   merge_hard_reg_conflicts(self,from_a, a, false);
   a->nrefs += from_a->nrefs;
   a->freq += from_a->freq;
   a->call_freq += from_a->call_freq;
   a->calls_crossed_num += from_a->calls_crossed_num;
   a->cheap_calls_crossed_num += from_a->cheap_calls_crossed_num;
   a->crossed_calls_abis |= from_a->crossed_calls_abis;
   a->crossed_calls_clobbered_regs|= from_a->crossed_calls_clobbered_regs;
   a->register_filters= from_a->register_filters | a->register_filters;

   a->excess_pressure_points_num += from_a->excess_pressure_points_num;
   if (! from_a->bad_spill_p)
      a->bad_spill_p = false;
   aclass =from_a->aclass;
   ira_assert (aclass == a->aclass);
   mtcs_ira_build_allocate_and_accumulate_costs/*!ira_allocate_and_accumulate_costs*/(self,
         &a->hard_reg_costs, aclass,from_a->hard_reg_costs);
   mtcs_ira_build_allocate_and_accumulate_costs/*!ira_allocate_and_accumulate_costs*/(self,
         &a->conflict_hard_reg_costs,aclass, from_a->conflict_hard_reg_costs);
   a->class_cost += from_a->class_cost;
   a->memory_cost += from_a->memory_cost;
}

/* Remove allocnos from loops removed from the allocation
   consideration.  */
static void remove_unnecessary_allocnos (MtcsIraBuild *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int regno;
   bool merged_p, rebuild_p;
   MtcsIraAllocno * a, *prev_a, *next_a, *parent_a;
   MtcsIraLoopTreeNode * a_node, *parent;

   merged_p = false;
   self->regno_allocnos = NULL;
   for (regno = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc) - 1;
         regno >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); regno--){
      rebuild_p = false;
      for (prev_a = NULL, a = self->ira_regno_allocno_map[regno]; a != NULL; a = next_a){
         next_a = a->next_regno_allocno;
         a_node = a->loop_tree_node;
         if (! a_node->to_remove_p)
            prev_a = a;
         else {
            for (parent = a_node->parent; (parent_a = parent->regno_allocno_map[regno]) == NULL
                  && parent->to_remove_p; parent = parent->parent)
               ;
            if (parent_a == NULL){
               /* There are no allocnos with the same regno in
               upper region -- just move the allocno to the
               upper region.  */
               prev_a = a;
               a->loop_tree_node = parent;
               parent->regno_allocno_map[regno] = a;
               bitmap_set_bit (parent->all_allocnos, a->num);
               rebuild_p = true;
            }else{
               /* Remove the allocno and update info of allocno in
               the upper region.  */
               if (prev_a == NULL)
                  self->ira_regno_allocno_map[regno] = next_a;
               else
                  prev_a->next_regno_allocno = next_a;
               move_allocno_live_ranges(self,a, parent_a);
               merged_p = true;
               propagate_some_info_from_allocno(self,parent_a, a);
               /* Remove it from the corresponding regno allocno
               map to avoid info propagation of subsequent
               allocno into this already removed allocno.  */
               a_node->regno_allocno_map[regno] = NULL;
               mtcs_ira_build_remove_allocno_prefs/*!ira_remove_allocno_prefs*/(self,a);
               finish_allocno(self,a);
            }
         }
      }
      if (rebuild_p)/* We need to restore the order in regno allocno list.  */{
         if (self->regno_allocnos == NULL)
            self->regno_allocnos   = (MtcsIraAllocno **) ira_allocate (sizeof (MtcsIraAllocno *) *self->ira_allocnos_num);
         ira_rebuild_regno_allocno_list(self,regno);
      }
   }
   if (merged_p)
      ira_rebuild_start_finish_chains ();
   if (self->regno_allocnos != NULL)
      ira_free (self->regno_allocnos);
}

/* Remove allocnos from all loops but the root.  */
static void remove_low_level_allocnos (MtcsIraBuild *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);

   int regno;
   bool merged_p, propagate_p;
   MtcsIraAllocno *a, *top_a;
   MtcsIraLoopTreeNode *a_node, *parent;
   MtcsIraAllocnoIterator ai;

   merged_p = false;
   MTCS_FOR_EACH_ALLOCNO(self,a, ai){
      a_node = a->loop_tree_node;
      if (a_node == self->ira_loop_tree_root || a->cap_member != NULL)
         continue;
      regno = a->regno;
      if ((top_a = self->ira_loop_tree_root->regno_allocno_map[regno]) == NULL){
         a->loop_tree_node = self->ira_loop_tree_root;
         self->ira_loop_tree_root->regno_allocno_map[regno] = a;
         continue;
      }
      propagate_p = a_node->parent->regno_allocno_map[regno] == NULL;
      /* Remove the allocno and update info of allocno in the upper
      region.  */
      move_allocno_live_ranges(self,a, top_a);
      merged_p = true;
      if (propagate_p)
         propagate_some_info_from_allocno(self,top_a, a);
   }

   MTCS_FOR_EACH_ALLOCNO(self,a, ai){
      a_node = a->loop_tree_node;
      if (a_node == self->ira_loop_tree_root)
         continue;
      parent = a_node->parent;
      regno = a->regno;
      if (a->cap_member != NULL)
         ira_assert (a->cap != NULL);
      else if (a->cap == NULL)
         ira_assert (parent->regno_allocno_map[regno] != NULL);
   }
   MTCS_FOR_EACH_ALLOCNO(self,a, ai){
      regno = a->regno;
      if (self->ira_loop_tree_root->regno_allocno_map[regno] == a){
         MtcsIraObject * obj;
         MtcsIraAllocnoObjectIterator oi;

         self->ira_regno_allocno_map[regno] = a;
         a->next_regno_allocno = NULL;
         a->cap_member = NULL;
         MTCS_FOR_EACH_ALLOCNO_OBJECT (a, obj, oi)
            obj->conflict_hard_regs  = obj->total_conflict_hard_regs;

         if(mtcs_config_ifdef(mtcsConfig,MTCS_STACK_REGS)){
            if (a->total_no_stack_reg_p)
            a->no_stack_reg_p = true;
         }
         /*!
         #ifdef STACK_REGS
         if (ALLOCNO_TOTAL_NO_STACK_REG_P (a))
         ALLOCNO_NO_STACK_REG_P (a) = true;
         #endif
         */
      }else{
         mtcs_ira_build_remove_allocno_prefs/*!ira_remove_allocno_prefs*/(self,a);
         finish_allocno(self,a);
      }
   }
   if (merged_p)
      ira_rebuild_start_finish_chains ();
}

/* Remove loops from consideration.  We remove all loops except for
   root if ALL_P or loops for which a separate allocation will not
   improve the result.  We have to do this after allocno creation and
   their costs and allocno class evaluation because only after that
   the register pressure can be known and is calculated.  */
static void remove_unnecessary_regions (MtcsIraBuild *self,bool all_p)
{
   if (current_loops == NULL)
      return;
   if (all_p)
      mark_all_loops_for_removal(self);
   else
      mark_loops_for_removal(self);
   self->children_vec.create (last_basic_block_for_fn (cfun) + number_of_loops (cfun));
   self->removed_loop_vec.create (last_basic_block_for_fn (cfun) + number_of_loops (cfun));
   remove_uneccesary_loop_nodes_from_loop_tree(self,self->ira_loop_tree_root);
   self->children_vec.release ();
   if (all_p)
      remove_low_level_allocnos(self);
   else
      remove_unnecessary_allocnos(self);
   while (self->removed_loop_vec.length () > 0)
      mtcs_ira_loop_tree_node_free/*!finish_loop_tree_node*/(self->removed_loop_vec.pop ());
   self->removed_loop_vec.release ();
}

/* At this point true value of allocno attribute bad_spill_p means
   that there is an insn where allocno occurs and where the allocno
   cannot be used as memory.  The function updates the attribute, now
   it can be true only for allocnos which cannot be used as memory in
   an insn and in whose live ranges there is other allocno deaths.
   Spilling allocnos with true value will not improve the code because
   it will not make other allocnos colorable and additional reloads
   for the corresponding pseudo will be generated in reload pass for
   each insn it occurs.

   This is a trick mentioned in one classic article of Chaitin etc
   which is frequently omitted in other implementations of RA based on
   graph coloring.  */
static void update_bad_spill_attribute (MtcsIraBuild *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int i;
   MtcsIraAllocno * a;
   MtcsIraAllocnoIterator ai;
   MtcsIraAllocnoObjectIterator aoi;
   MtcsIraObject * obj;
   MtcsLiveRange *r;
   enum reg_class aclass;
   bitmap_head dead_points[mtcs_reg_get_n_reg_classes/*!N_REG_CLASSES*/(mtcsReg)];

   for (i = 0; i < mtcsIra->x_ira_allocno_classes_num; i++){
      aclass = mtcsIra->x_ira_allocno_classes[i];
      bitmap_initialize (&dead_points[aclass], &reg_obstack);
   }
   MTCS_FOR_EACH_ALLOCNO(self,a, ai){
      aclass = a->aclass;
      if (aclass == NO_REGS)
         continue;
      MTCS_FOR_EACH_ALLOCNO_OBJECT (a, obj, aoi)
         for (r = obj->live_ranges; r != NULL; r = r->next)
            bitmap_set_bit (&dead_points[aclass], r->finish);
   }
   MTCS_FOR_EACH_ALLOCNO(self,a, ai){
      aclass = a->aclass;
      if (aclass == NO_REGS)
         continue;
      if (! a->bad_spill_p)
         continue;
      MTCS_FOR_EACH_ALLOCNO_OBJECT (a, obj, aoi){
         for (r = obj->live_ranges; r != NULL; r = r->next){
            for (i = r->start + 1; i < r->finish; i++)
               if (bitmap_bit_p (&dead_points[aclass], i))
                  break;
            if (i < r->finish)
               break;
         }
         if (r != NULL){
            a->bad_spill_p = false;
            break;
         }
      }
   }
   for (i = 0; i < mtcsIra->x_ira_allocno_classes_num; i++){
      aclass = mtcsIra->x_ira_allocno_classes[i];
      bitmap_clear (&dead_points[aclass]);
   }
}

/* Set up minimal and maximal live range points for allocnos.  */
static void setup_min_max_allocno_live_range_point (MtcsIraBuild *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraLives *mtcsLives=mtcs_ira_mgr_get_lives(mtcsIraMgr);

   int i;
   MtcsIraAllocno * a, *parent_a, *cap;
   MtcsIraAllocnoIterator ai;
#ifdef ENABLE_IRA_CHECKING
   MtcsIraObjectIterator oi;
   MtcsIraObject * obj;
#endif
   MtcsLiveRange * r;
   MtcsIraLoopTreeNode * parent;

   MTCS_FOR_EACH_ALLOCNO(self,a, ai){
      int n = a->num_objects ;
      for (i = 0; i < n; i++){
         MtcsIraObject * obj = a->objects[i];
         r = obj->live_ranges;
         if (r == NULL)
            continue;
         obj->max = r->finish;
         for (; r->next != NULL; r = r->next)
            ;
         obj->min = r->start;
      }
   }
   for (i = mtcs_func_max_reg_num/*@max_reg_num*/(mtcsFunc) - 1;
         i >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i--)
      for (a = self->ira_regno_allocno_map[i];a != NULL; a = a->next_regno_allocno){
         int j;
         int n = a->num_objects;

         for (j = 0; j < n; j++) {
            MtcsIraObject * obj = ALLOCNO_OBJECT (a, j);
            MtcsIraObject * parent_obj;

            if (obj->max < 0){
               /* The object is not used and hence does not live.  */
               ira_assert (obj->live_ranges == NULL);
               obj->max = 0;
               obj->min = 1;
               continue;
            }
            ira_assert (a->cap_member == NULL);
            /* Accumulation of range info.  */
            if (a->cap != NULL){
               for (cap = a->cap; cap != NULL; cap = cap->cap){
                  MtcsIraObject * cap_obj =cap->objects[j];
                  if (cap_obj->max < obj->max)
                     cap_obj->max = obj->max;
                  if (cap_obj->min > obj->min)
                     cap_obj->min = obj->min;
               }
               continue;
            }
            if ((parent = a->loop_tree_node->parent) == NULL)
               continue;
            parent_a = parent->regno_allocno_map[i];
            parent_obj = parent_a->objects[j];
            if (parent_obj->max < obj->max)
               parent_obj->max = obj->max;
            if (parent_obj->min > obj->min)
               parent_obj->min = obj->min;
         }
      }
#ifdef ENABLE_IRA_CHECKING
   MTCS_FOR_EACH_OBJECT(self,obj, oi){
      if ((obj->min >= 0 && obj->min <= mtcsLives->ira_max_point)
      && (obj->max >= 0 && obj->max <= mtcsLives->ira_max_point))
         continue;
      gcc_unreachable ();
   }
#endif
}

/* Sort allocnos according to their live ranges.  Allocnos with
   smaller allocno class are put first unless we use priority
   coloring.  Allocnos with the same class are ordered according
   their start (min).  Allocnos with the same start are ordered
   according their finish (max).  */
static int object_range_compare_func (const void *v1p, const void *v2p)
{
  int diff;
  MtcsIraObject * obj1 = *(const MtcsIraObject * *) v1p;
  MtcsIraObject * obj2 = *(const MtcsIraObject * *) v2p;
  MtcsIraAllocno * a1 = obj1->allocno;
  MtcsIraAllocno * a2 = obj2->allocno;

  if ((diff = obj1->min - obj2->min) != 0)
    return diff;
  if ((diff = obj1->max - obj2->max) != 0)
     return diff;
  return a1->num - a2->num;
}

/* Sort ira_object_id_map and set up conflict id of allocnos.  */
static void sort_conflict_id_map (MtcsIraBuild *self)
{
   int i, num;
   MtcsIraAllocno * a;
   MtcsIraAllocnoIterator ai;

   num = 0;
   MTCS_FOR_EACH_ALLOCNO(self,a, ai){
      MtcsIraAllocnoObjectIterator oi;
      MtcsIraObject * obj;

      MTCS_FOR_EACH_ALLOCNO_OBJECT (a, obj, oi)
         self->ira_object_id_map[num++] = obj;
   }
   if (num > 1)
      qsort (self->ira_object_id_map, num, sizeof (MtcsIraObject *), object_range_compare_func);
   for (i = 0; i < num; i++){
      MtcsIraObject * obj = self->ira_object_id_map[i];
      gcc_assert (obj != NULL);
      obj->id = i;
   }
   for (i = num; i < self->ira_objects_num; i++)
      self->ira_object_id_map[i] = NULL;
}

/* Set up minimal and maximal conflict ids of allocnos with which
   given allocno can conflict.  */
static void setup_min_max_conflict_allocno_ids (MtcsIraBuild *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraLives *mtcsLives=mtcs_ira_mgr_get_lives(mtcsIraMgr);

   int aclass;
   int i, j, min, max, start, finish, first_not_finished, filled_area_start;
   int *live_range_min, *last_lived;
   int word0_min, word0_max;
   MtcsIraAllocno * a;
   MtcsIraAllocnoIterator ai;

   live_range_min = (int *) ira_allocate (sizeof (int) * self->ira_objects_num);
   aclass = -1;
   first_not_finished = -1;
   for (i = 0; i < self->ira_objects_num; i++){
      MtcsIraObject * obj = self->ira_object_id_map[i];
      if (obj == NULL)
         continue;
      a = obj->allocno;
      if (aclass < 0){
         aclass = a->aclass;
         min = i;
         first_not_finished = i;
      }else{
         start = obj->min;
         /* If we skip an allocno, the allocno with smaller ids will
         be also skipped because of the secondary sorting the
         range finishes (see function
         object_range_compare_func).  */
         while (first_not_finished < i   && start > self->ira_object_id_map[first_not_finished]->max)
            first_not_finished++;
         min = first_not_finished;
      }
      if (min == i)
         /* We could increase min further in this case but it is good
         enough.  */
         min++;
      live_range_min[i] = obj->min;
      obj->min = min;
   }
   last_lived = (int *) ira_allocate (sizeof (int) * mtcsLives->ira_max_point);
   aclass = -1;
   filled_area_start = -1;
   for (i = self->ira_objects_num - 1; i >= 0; i--){
      MtcsIraObject * obj = self->ira_object_id_map[i];

      if (obj == NULL)
         continue;

      a = obj->allocno;
      if (aclass < 0){
         aclass = a->aclass;
         for (j = 0; j < mtcsLives->ira_max_point; j++)
            last_lived[j] = -1;
         filled_area_start = mtcsLives->ira_max_point;
      }
      min = live_range_min[i];
      finish = obj->max;
      max = last_lived[finish];
      if (max < 0)
         /* We could decrease max further in this case but it is good
         enough.  */
         max = obj->id - 1;
      obj->max = max;
      /* In filling, we can go further A range finish to recognize
      intersection quickly because if the finish of subsequently
      processed allocno (it has smaller conflict id) range is
      further A range finish than they are definitely intersected
      (the reason for this is the allocnos with bigger conflict id
      have their range starts not smaller than allocnos with
      smaller ids.  */
      for (j = min; j < filled_area_start; j++)
         last_lived[j] = i;
      filled_area_start = min;
   }
   ira_free (last_lived);
   ira_free (live_range_min);

   /* For allocnos with more than one object, we may later record extra conflicts in
   subobject 0 that we cannot really know about here.
   For now, simply widen the min/max range of these subobjects.  */

   word0_min = INT_MAX;
   word0_max = INT_MIN;

   MTCS_FOR_EACH_ALLOCNO(self,a, ai){
      int n = a->num_objects;
      MtcsIraObject * obj0;

      if (n < 2)
         continue;
      obj0 = a->objects[0];
      if (obj0->id < word0_min)
         word0_min = obj0->id;
      if (obj0->id > word0_max)
         word0_max = obj0->id;
   }
   MTCS_FOR_EACH_ALLOCNO(self,a, ai){
      int n = a->num_objects;
      MtcsIraObject * obj0;

      if (n < 2)
         continue;
      obj0 = a->objects[0];
      if (obj0->min > word0_min)
         obj0->min = word0_min;
      if (obj0->max < word0_max)
         obj0->max = word0_max;
   }
}

static void create_caps (MtcsIraBuild *self)
{
   MtcsIraAllocno * a;
   MtcsIraAllocnoIterator ai;
   MtcsIraLoopTreeNode * loop_tree_node;
   MTCS_FOR_EACH_ALLOCNO(self,a, ai){
      if (a->loop_tree_node == self->ira_loop_tree_root)
         continue;
      if (a->cap_member != NULL)
         create_cap_allocno(self,a);
      else if (a->cap == NULL){
         loop_tree_node = a->loop_tree_node;
         if (!bitmap_bit_p (loop_tree_node->border_allocnos, a->num))
            create_cap_allocno(self,a);
      }
   }
}

/* The page contains code transforming more one region internal
   representation (IR) to one region IR which is necessary for reload.
   This transformation is called IR flattening.  We might just rebuild
   the IR for one region but we don't do it because it takes a lot of
   time.  */

/* 原型 extern ira_allocno_t ira_parent_allocno (ira_allocno_t); ira-int.h ira-build.cc 被 mtcs_ira_allocno_parent_allocno 替换*/

/* 原型 ira_allocno_t ira_parent_or_cap_allocno (ira_allocno_t); mtcs_ira_allocno_parent_or_cap_allocno 替换*/

/* Process all allocnos originated from pseudo REGNO and copy live
   ranges, hard reg conflicts, and allocno stack reg attributes from
   low level allocnos to final allocnos which are destinations of
   removed stores at a loop exit.  Return true if we copied live
   ranges.  */
static bool copy_info_to_removed_store_destinations (MtcsIraBuild *self,int regno)
{
   MtcsIraAllocno * a;
   MtcsIraAllocno * parent_a = NULL;
   MtcsIraLoopTreeNode * parent;
   bool merged_p;

   merged_p = false;
   for (a = self->ira_regno_allocno_map[regno]; a != NULL; a = a->next_regno_allocno){
      if (a != self->regno_top_level_allocno_map[REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(a))])
         /* This allocno will be removed.  */
         continue;

      /* Caps will be removed.  */
      ira_assert (a->cap_member == NULL);
      for (parent = a->loop_tree_node->parent; parent != NULL; parent = parent->parent)
         if ((parent_a = parent->regno_allocno_map[regno]) == NULL
         || (parent_a == self->regno_top_level_allocno_map[REGNO(mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(parent_a))]
         && MTCS_ALLOCNO_EMIT_DATA(parent_a)->mem_optimized_dest_p))
            break;
      if (parent == NULL || parent_a == NULL)
         continue;

      copy_allocno_live_ranges(self,a, parent_a);
      merge_hard_reg_conflicts(self,a, parent_a, true);

      parent_a->call_freq += a->call_freq;
      parent_a->calls_crossed_num += a->calls_crossed_num;
      parent_a->cheap_calls_crossed_num += a->cheap_calls_crossed_num;
      parent_a->crossed_calls_abis |= a->crossed_calls_abis;
      parent_a->crossed_calls_clobbered_regs |= a->crossed_calls_clobbered_regs;
      parent_a->excess_pressure_points_num += a->excess_pressure_points_num;
      merged_p = true;
   }
   return merged_p;
}

/* Flatten the IR.  In other words, this function transforms IR as if
   it were built with one region (without loops).  We could make it
   much simpler by rebuilding IR with one region, but unfortunately it
   takes a lot of time.  MAX_REGNO_BEFORE_EMIT and
   IRA_MAX_POINT_BEFORE_EMIT are correspondingly MAX_REG_NUM () and
   IRA_MAX_POINT before emitting insns on the loop borders.  */
//原型 ira_flattening ira-int.h ira-build.cc
void mtcs_ira_build_flattening (MtcsIraBuild *self,int max_regno_before_emit, int ira_max_point_before_emit)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraLives *mtcsIraLives=mtcs_ira_mgr_get_lives(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int max_regno = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);
   int i, j;
   bool keep_p;
   int hard_regs_num;
   bool new_pseudos_p, merged_p, mem_dest_p;
   unsigned int n;
   enum reg_class aclass;
   MtcsIraAllocno *a, *parent_a, *first, *second, *node_first, *node_second;
   MtcsIraAllocnoCopy *cp;
   MtcsIraLoopTreeNode * node;
   MtcsLiveRange * r;
   MtcsIraAllocnoIterator ai;
   MtcsIraAllocnoCopyIterator ci;

   self->regno_top_level_allocno_map = (MtcsIraAllocno **) ira_allocate (max_regno/*!max_reg_num ()*/ * sizeof (MtcsIraAllocno *));
   memset (self->regno_top_level_allocno_map, 0,max_regno/*!max_reg_num ()*/ * sizeof (MtcsIraAllocno *));
   new_pseudos_p = merged_p = false;
   MTCS_FOR_EACH_ALLOCNO(self,a, ai){
      MtcsIraAllocnoObjectIterator oi;
      MtcsIraObject *obj;

      if (a->cap_member != NULL)
         /* Caps are not in the regno allocno maps and they are never
         will be transformed into allocnos existing after IR
         flattening.  */
         continue;
      MTCS_FOR_EACH_ALLOCNO_OBJECT (a, obj, oi)
         obj->total_conflict_hard_regs = obj->conflict_hard_regs;
      if(mtcs_config_ifdef(mtcsConfig,MTCS_STACK_REGS)){
         a->total_no_stack_reg_p = a->no_stack_reg_p;
         /*!
         #ifdef STACK_REGS
         ALLOCNO_TOTAL_NO_STACK_REG_P (a) = ALLOCNO_NO_STACK_REG_P (a);
         #endif
         */
      }
   }
   /* Fix final allocno attributes.  */
   for (i = max_regno_before_emit - 1; i >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i--){
      mem_dest_p = false;
      for (a = self->ira_regno_allocno_map[i]; a != NULL; a = a->next_regno_allocno){
         MtcsIraEmitData * parent_data, *data = MTCS_ALLOCNO_EMIT_DATA (a);

         ira_assert (a->cap_member == NULL);
         if (data->somewhere_renamed_p)
            new_pseudos_p = true;
         parent_a = mtcs_ira_allocno_parent_allocno/*!ira_parent_allocno*/(a);
         if (parent_a == NULL){
            a->allocno_copies = NULL;
            self->regno_top_level_allocno_map[REGNO (data->reg)] = a;
            continue;
         }
         ira_assert (parent_a->cap_member == NULL);

         if (data->mem_optimized_dest != NULL)
            mem_dest_p = true;
         parent_data = MTCS_ALLOCNO_EMIT_DATA (parent_a);
         if (REGNO (data->reg) == REGNO (parent_data->reg)){
            merge_hard_reg_conflicts(self,a, parent_a, true);
            move_allocno_live_ranges(self,a, parent_a);
            merged_p = true;
            parent_data->mem_optimized_dest_p = (parent_data->mem_optimized_dest_p || data->mem_optimized_dest_p);
            continue;
         }
         new_pseudos_p = true;
         for (;;){
            parent_a->nrefs -= a->nrefs;
            parent_a->freq -= a->freq;
            parent_a->call_freq -= a->call_freq;
            parent_a->calls_crossed_num -= a->calls_crossed_num;
            parent_a->cheap_calls_crossed_num -= a->cheap_calls_crossed_num;
            /* Assume that ALLOCNO_CROSSED_CALLS_ABIS and
            ALLOCNO_CROSSED_CALLS_CLOBBERED_REGS stay the same.
            We'd need to rebuild the IR to do better.  */
            parent_a->excess_pressure_points_num -= a->excess_pressure_points_num;
            ira_assert (parent_a->calls_crossed_num >= 0 && parent_a->nrefs >= 0  && parent_a->freq >= 0);
            aclass = parent_a->aclass;
            hard_regs_num = mtcsIra->x_ira_class_hard_regs_num[aclass];
            if (a->hard_reg_costs != NULL && parent_a->hard_reg_costs != NULL)
               for (j = 0; j < hard_regs_num; j++)
                  parent_a->hard_reg_costs[j]  -= a->hard_reg_costs[j];
            if (a->conflict_hard_reg_costs != NULL && parent_a->conflict_hard_reg_costs != NULL)
               for (j = 0; j < hard_regs_num; j++)
                  parent_a->conflict_hard_reg_costs[j] -= a->conflict_hard_reg_costs[j];
            parent_a->class_cost  -= a->class_cost;
            parent_a->memory_cost -= a->memory_cost;
            parent_a = mtcs_ira_allocno_parent_allocno/*!ira_parent_allocno*/(parent_a);
            if (parent_a == NULL)
               break;
         }
         a->allocno_copies = NULL;
         self->regno_top_level_allocno_map[REGNO (data->reg)] = a;
      }
      if (mem_dest_p && copy_info_to_removed_store_destinations(self,i))
         merged_p = true;
   }
   ira_assert (new_pseudos_p || ira_max_point_before_emit == ira_max_point);
   if (merged_p || ira_max_point_before_emit != mtcsIraLives->ira_max_point)
      ira_rebuild_start_finish_chains ();
   if (new_pseudos_p){
      sparseset objects_live;

      /* Rebuild conflicts.  */
      MTCS_FOR_EACH_ALLOCNO(self,a, ai){
         MtcsIraAllocnoObjectIterator oi;
         MtcsIraObject * obj;

         if (a != self->regno_top_level_allocno_map[REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(a))]
                                                    || a->cap_member != NULL)
            continue;
         MTCS_FOR_EACH_ALLOCNO_OBJECT(a, obj, oi){
            for (r = obj->live_ranges; r != NULL; r = r->next)
               ira_assert (r->object == obj);
            mtcs_ira_object_clear_conflicts/*!clear_conflicts*/(obj);
         }
      }

      objects_live = sparseset_alloc (self->ira_objects_num);
      for (i = 0; i <  mtcsIraLives->ira_max_point; i++){
         for (r = mtcsIraLives->ira_start_point_ranges[i]; r != NULL; r = r->start_next){
            MtcsIraObject * obj = r->object;

            a = obj->allocno;
            if (a != self->regno_top_level_allocno_map[REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(a))]
                                                       || a->cap_member != NULL)
               continue;

            aclass = a->aclass;
            EXECUTE_IF_SET_IN_SPARSESET (objects_live, n){
               MtcsIraObject * live_obj = self->ira_object_id_map[n];
               MtcsIraAllocno * live_a = live_obj->allocno;
               enum reg_class live_aclass = live_a->aclass;

               if (mtcsIra->x_ira_reg_classes_intersect_p[aclass][live_aclass]
               /* Don't set up conflict for the allocno with itself.  */
               && live_a != a)
                  mtcs_ira_object_add_conflict/*!ira_add_conflict*/(obj, live_obj);
            }
            sparseset_set_bit (objects_live, obj->id);
         }

         for (r = mtcsIraLives->ira_finish_point_ranges[i]; r != NULL; r = r->finish_next)
            sparseset_clear_bit (objects_live, r->object->id);
      }
      sparseset_free (objects_live);
      compress_conflict_vecs(self);
   }
   /* Mark some copies for removing and change allocnos in the rest
   copies.  */
   MTCS_FOR_EACH_COPY(self,cp, ci){
      if (cp->first->cap_member != NULL || cp->second->cap_member != NULL){
         if (mtcsIraGlobal->internal_flag_ira_verbose > 4 && mtcsIraGlobal->ira_dump_file != NULL)
            fprintf (mtcsIraGlobal->ira_dump_file, "      Remove cp%d:%c%dr%d-%c%dr%d\n",
                     cp->num, cp->first->cap_member != NULL ? 'c' : 'a', cp->first->num,
                     REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(cp->first)),
                     cp->second->cap_member != NULL ? 'c' : 'a',
                     cp->second->num, REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(cp->second)));
         cp->loop_tree_node = NULL;
         continue;
      }
      first = self->regno_top_level_allocno_map[REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(cp->first))];
      second = self->regno_top_level_allocno_map[REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(cp->second))];
      node = cp->loop_tree_node;
      if (node == NULL)
         keep_p = true; /* It copy generated in ira-emit.cc.  */
      else{
         /* Check that the copy was not propagated from level on
         which we will have different pseudos.  */
         node_first = node->regno_allocno_map[cp->first->regno];
         node_second = node->regno_allocno_map[cp->second->regno];
         keep_p = ((REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(first))
               == REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(node_first)))
               && (REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(second))
                     == REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(node_second))));
      }
      if (keep_p){
         cp->loop_tree_node = self->ira_loop_tree_root;
         cp->first = first;
         cp->second = second;
      }else{
         cp->loop_tree_node = NULL;
         if (mtcsIraGlobal->internal_flag_ira_verbose > 4 && mtcsIraGlobal->ira_dump_file != NULL)
            fprintf (mtcsIraGlobal->ira_dump_file, "      Remove cp%d:a%dr%d-a%dr%d\n",
                     cp->num, cp->first->num,REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(cp->first)),
                     cp->second->num,  REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(cp->second)));
      }
   }
   /* Remove unnecessary allocnos on lower levels of the loop tree.  */
   MTCS_FOR_EACH_ALLOCNO(self,a, ai){
      if (a != self->regno_top_level_allocno_map[REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(a))]
      || a->cap_member != NULL){
         if (mtcsIraGlobal->internal_flag_ira_verbose > 4 && mtcsIraGlobal->ira_dump_file != NULL)
            fprintf (mtcsIraGlobal->ira_dump_file, "      Remove a%dr%d\n",
                              a->num, REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(a)));
                              mtcs_ira_build_remove_allocno_prefs/*!ira_remove_allocno_prefs*/(self,a);
                              finish_allocno(self,a);
         continue;
      }
      a->loop_tree_node = self->ira_loop_tree_root;
      a->regno = REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(a));
      a->cap = NULL;
      /* Restore updated costs for assignments from reload.  */
      a->updated_memory_cost = a->memory_cost;
      a->updated_class_cost = a->class_cost;
      if (! a->assigned_p)
         mtcs_ira_build_free_allocno_updated_costs/*!ira_free_allocno_updated_costs*/(self,a);
      ira_assert (a->updated_hard_reg_costs == NULL);
      ira_assert (a->updated_conflict_hard_reg_costs == NULL);
   }
   /* Remove unnecessary copies.  */
   MTCS_FOR_EACH_COPY(self,cp, ci){
      if (cp->loop_tree_node == NULL){
         self->ira_copies[cp->num] = NULL;
         finish_copy (cp);
         continue;
      }
      ira_assert (cp->first->loop_tree_node == self->ira_loop_tree_root
            && cp->second->loop_tree_node == self->ira_loop_tree_root);
            mtcs_ira_allocno_add_allocno_copy_to_list/*!add_allocno_copy_to_list*/(cp);
            mtcs_ira_allocno_swap_allocno_copy_ends_if_necessary/*!swap_allocno_copy_ends_if_necessary*/(cp);
   }
   rebuild_regno_allocno_maps(self);
   if (mtcsIraLives->ira_max_point != ira_max_point_before_emit)
      ira_compress_allocno_live_ranges ();
   ira_free (self->regno_top_level_allocno_map);
}



#ifdef ENABLE_IRA_CHECKING
/* Check creation of all allocnos.  Allocnos on lower levels should
   have allocnos or caps on all upper levels.  */
static void check_allocno_creation (MtcsIraBuild *self)
{
   MtcsIraAllocno * a;
   MtcsIraAllocnoIterator ai;
   MtcsIraLoopTreeNode * loop_tree_node;

   MTCS_FOR_EACH_ALLOCNO(self,a, ai){
      loop_tree_node = a->loop_tree_node;
      ira_assert (bitmap_bit_p (loop_tree_node->all_allocnos, a->num));
      if (loop_tree_node == self->ira_loop_tree_root)
         continue;
      if (a->cap_member != NULL)
         ira_assert (a->cap != NULL);
      else if (a->cap == NULL)
         ira_assert (loop_tree_node->parent->regno_allocno_map[a->regno] != NULL
               && bitmap_bit_p (loop_tree_node->border_allocnos, a->num));
   }
}
#endif

/* Identify allocnos which prefer a register class with a single hard register.
   Adjust ALLOCNO_CONFLICT_HARD_REG_COSTS so that conflicting allocnos are
   less likely to use the preferred singleton register.  */
static void update_conflict_hard_reg_costs (MtcsIraBuild *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   MtcsIraAllocno * a;
   MtcsIraAllocnoIterator ai;
   int i, index, min;

   MTCS_FOR_EACH_ALLOCNO(self,a, ai){
      reg_class_t aclass = a->aclass;
      reg_class_t pref = mtcs_reg_reg_preferred_class/*!reg_preferred_class*/(mtcsReg,a->regno);
      int singleton = mtcsIra->x_ira_class_singleton[pref][a->mode];
      if (singleton < 0)
         continue;
      index = mtcsIraInt->x_ira_class_hard_reg_index[(int) aclass][singleton];
      if (index < 0)
         continue;
      if (a->conflict_hard_reg_costs == NULL || a->hard_reg_costs == NULL)
         continue;
      min = INT_MAX;
      for (i = mtcsIra->x_ira_class_hard_regs_num[(int) aclass] - 1; i >= 0; i--)
         if (a->hard_reg_costs[i] > a->class_cost  && min > a->hard_reg_costs[i])
            min = a->hard_reg_costs[i];
      if (min == INT_MAX)
         continue;
      mtcs_ira_build_allocate_and_set_costs/*!ira_allocate_and_set_costs*/(self,&a->conflict_hard_reg_costs,aclass, 0);
      a->conflict_hard_reg_costs[index] -= min - a->class_cost;
   }
}

/* Create a internal representation (IR) for IRA (allocnos, copies,
   loop tree nodes).  The function returns TRUE if we generate loop
   structure (besides nodes representing all function and the basic
   blocks) for regional allocation.  A true return means that we
   really need to flatten IR before the reload.  */
//原型 ira_build ira-int.h ira-build.cc
bool mtcs_ira_build_build (MtcsIraBuild *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraLives *mtcsIraLives = mtcs_ira_mgr_get_lives(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   bool loops_p;

   mtcs_dfcore_df_analyze/*!df_analyze*/(mtcsDfcore);
   initiate_cost_vectors (self);
   initiate_allocnos (self);
   initiate_prefs(self);
   initiate_copies(self);
   create_loop_tree_nodes(self);
   form_loop_tree (self);
   create_allocnos(self);
   ira_costs ();
   create_allocno_objects(self);
   ira_create_allocno_live_ranges ();
   remove_unnecessary_regions(self,false);
   ira_compress_allocno_live_ranges ();
   update_bad_spill_attribute(self);
   loops_p = more_one_region_p(self);
   if (loops_p){
      propagate_allocno_info(self);
      create_caps(self);
   }
   ira_tune_allocno_costs ();
#ifdef ENABLE_IRA_CHECKING
   check_allocno_creation(self);
#endif
   setup_min_max_allocno_live_range_point(self);
   sort_conflict_id_map(self);
   setup_min_max_conflict_allocno_ids(self);
   ira_build_conflicts ();
   update_conflict_hard_reg_costs(self);
   if (! mtcsIra->ira_conflicts_p){
      MtcsIraAllocno * a;
      MtcsIraAllocnoIterator ai;

      /* Remove all regions but root one.  */
      if (loops_p){
         remove_unnecessary_regions(self,true);
         loops_p = false;
      }
      /* We don't save hard registers around calls for fast allocation
      -- add caller clobbered registers as conflicting ones to
      allocno crossing calls.  */
      MTCS_FOR_EACH_ALLOCNO(self,a, ai)
         if (a->calls_crossed_num != 0)
            mtcs_ira_allocno_ior_hard_reg_conflicts/*!ior_hard_reg_conflicts*/(a,
                  mtcs_ira_allocno_need_caller_save_regs/*!ira_need_caller_save_regs*/(a));
   }

   if (mtcsIraGlobal->internal_flag_ira_verbose > 2 && mtcsIraGlobal->ira_dump_file != NULL)
      print_copies(self,mtcsIraGlobal->ira_dump_file);
   if (mtcsIraGlobal->internal_flag_ira_verbose > 2 && mtcsIraGlobal->ira_dump_file != NULL)
      print_prefs(self,mtcsIraGlobal->ira_dump_file);

   if (mtcsIraGlobal->internal_flag_ira_verbose > 0 && mtcsIraGlobal->ira_dump_file != NULL){
      int n, nr, nr_big;
      MtcsIraAllocno * a;
      MtcsLiveRange * r;
      MtcsIraAllocnoIterator ai;

      n = 0;
      nr = 0;
      nr_big = 0;
      MTCS_FOR_EACH_ALLOCNO(self,a, ai){
         int j, nobj = a->num_objects;

         if (nobj > 1)
            nr_big++;
         for (j = 0; j < nobj; j++){
            MtcsIraObject * obj = a->objects[j];
            n += obj->num_accumulated_conflicts;
            for (r = obj->live_ranges; r != NULL; r = r->next)
               nr++;
         }
      }
      fprintf (mtcsIraGlobal->ira_dump_file, "  regions=%d, blocks=%d, points=%d\n",
                  current_loops == NULL ? 1 : number_of_loops (cfun),
                  n_basic_blocks_for_fn (cfun), mtcsIraLives->ira_max_point);
      fprintf (mtcsIraGlobal->ira_dump_file, "    allocnos=%d (big %d), copies=%d, conflicts=%d, ranges=%d\n",
                  self->ira_allocnos_num, nr_big, self->ira_copies_num, n, nr);
   }
   return loops_p;
}

/* Release the data created by function ira_build.  */
//原型 ira_destroy ira-int.h ira-build.cc
void mtcs_ira_build_destroy (MtcsIraBuild *self)
{
  finish_loop_tree_nodes (self);
  finish_prefs(self);
  finish_copies(self);
  finish_allocnos(self);
  finish_cost_vectors(self);
  ira_finish_allocno_live_ranges ();
}



//////////////////////////////以下不是ira-build中的代码----------------------------
/* Allocate cost vector *VEC for hard registers of ACLASS and copy
   values of vector SRC into the vector or initialize it by VAL (if
   SRC is null).  */
//原型 ira_allocate_and_set_or_copy_costs ira-int.h
void mtcs_ira_build_allocate_and_set_or_copy_costs (MtcsIraBuild *self,int **vec, enum reg_class aclass,
                int val, int *src)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int i, *reg_costs;
   int len;
   if (*vec != NULL)
      return;
   *vec = reg_costs = mtcs_ira_build_allocate_cost_vector/*!ira_allocate_cost_vector*/(self,aclass);
   len = mtcsIra->x_ira_class_hard_regs_num[aclass];
   if (src != NULL)
      memcpy (reg_costs, src, sizeof (int) * len);
   else{
      for (i = 0; i < len; i++)
         reg_costs[i] = val;
   }
}

/* To save memory we use a lazy approach for allocation and
   initialization of the cost vectors.  We do this only when it is
   really necessary.  */

/* Allocate cost vector *VEC for hard registers of ACLASS and
   initialize the elements by VAL if it is necessary */
//原型 ira_allocate_and_set_costs ira-int.h
void mtcs_ira_build_allocate_and_set_costs (MtcsIraBuild *self,int **vec, reg_class_t aclass, int val)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int i, *reg_costs;
   int len;
   if (*vec != NULL)
      return;
   *vec = reg_costs = mtcs_ira_build_allocate_cost_vector/*!ira_allocate_cost_vector*/(self,aclass);
   len = mtcsIra->x_ira_class_hard_regs_num[(int) aclass];
   for (i = 0; i < len; i++)
      reg_costs[i] = val;
}


/* Allocate cost vector *VEC for hard registers of ACLASS and add
   values of vector SRC into the vector if it is necessary */
//原型 ira_allocate_and_accumulate_costs ira-int.h
void mtcs_ira_build_allocate_and_accumulate_costs (MtcsIraBuild *self,int **vec, enum reg_class aclass, int *src)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int i, len;

   if (src == NULL)
      return;
   len = mtcsIra->x_ira_class_hard_regs_num[aclass];
   if (*vec == NULL){
      *vec = mtcs_ira_build_allocate_cost_vector/*!ira_allocate_cost_vector*/(self,aclass);
      memset (*vec, 0, sizeof (int) * len);
   }
   for (i = 0; i < len; i++)
   (*vec)[i] += src[i];
}

