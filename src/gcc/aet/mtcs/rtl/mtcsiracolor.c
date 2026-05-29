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
 * base on ira-color.cc
 */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "predict.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "ira.h"
#include "ira-int.h"
#include "reload.h"
#include "cfgloop.h"


#include "mtcsiracolor.h"
#include "../mtcstarget.h"
#include "../mtcscompile.h"

#include "mtcsiraint.h"
#include "mtcsiraemit.h"
#include "mtcsirabuild.h"
#include "mtcsira.h"
#include "mtcsiralives.h"



/* Info about changing hard reg costs of an allocno.  */
struct update_cost_record
{
  /* Hard regno for which we changed the cost.  */
  int hard_regno;
  /* Divisor used when we changed the cost of HARD_REGNO.  */
  int divisor;
  /* Next record for given allocno.  */
  struct update_cost_record *next;
};


/* The structure contains information about hard registers can be
   assigned to allocnos.  Usually it is allocno profitable hard
   registers but in some cases this set can be a bit different.  Major
   reason of the difference is a requirement to use hard register sets
   that form a tree or a forest (set of trees), i.e. hard register set
   of a node should contain hard register sets of its subnodes.  */
//原型 allocno_hard_regs
struct _AllocnoHardRegs
{
  /* Hard registers can be assigned to an allocno.  */
  HardRegSet  set;
  /* Overall (spilling) cost of all allocnos with given register
     set.  */
  int64_t cost;
};

/* A node representing allocno hard registers.  Such nodes form a
   forest (set of trees).  Each subnode of given node in the forest
   refers for hard register set (usually allocno profitable hard
   register set) which is a subset of one referred from given
   node.  */
//原型 allocno_hard_regs_node
struct _AllocnoHardRegsNode
{
  /* Set up number of the node in preorder traversing of the forest.  */
  int preorder_num;
  /* Used for different calculation like finding conflict size of an
     allocno.  */
  int check;
  /* Used for calculation of conflict size of an allocno.  The
     conflict size of the allocno is maximal number of given allocno
     hard registers needed for allocation of the conflicting allocnos.
     Given allocno is trivially colored if this number plus the number
     of hard registers needed for given allocno is not greater than
     the number of given allocno hard register set.  */
  int conflict_size;
  /* The number of hard registers given by member hard_regs.  */
  int hard_regs_num;
  /* The following member is used to form the final forest.  */
  bool used_p;
  /* Pointer to the corresponding profitable hard registers.  */
  AllocnoHardRegs * hard_regs;
  /* Parent, first subnode, previous and next node with the same
     parent in the forest.  */
  AllocnoHardRegsNode * parent, *first, *prev, *next;
};


/* To decrease footprint of ira_allocno structure we store all data
   needed only for coloring in the following structure.  */
//原型 struct allocno_color_data
struct _AllocnoColorData
{
  /* TRUE value means that the allocno was not removed yet from the
     conflicting graph during coloring.  */
  unsigned int in_graph_p : 1;
  /* TRUE if it is put on the stack to make other allocnos
     colorable.  */
  unsigned int may_be_spilled_p : 1;
  /* TRUE if the allocno is trivially colorable.  */
  unsigned int colorable_p : 1;
  /* Number of hard registers of the allocno class really
     available for the allocno allocation.  It is number of the
     profitable hard regs.  */
  int available_regs_num;
  /* Sum of frequencies of hard register preferences of all
     conflicting allocnos which are not the coloring stack yet.  */
  int conflict_allocno_hard_prefs;
  /* Allocnos in a bucket (used in coloring) chained by the following
     two members.  */
  MtcsIraAllocno * next_bucket_allocno;
  MtcsIraAllocno * prev_bucket_allocno;
  /* Used for temporary purposes.  */
  int temp;
  /* Used to exclude repeated processing.  */
  int last_process;
  /* Profitable hard regs available for this pseudo allocation.  It
     means that the set excludes unavailable hard regs and hard regs
     conflicting with given pseudo.  They should be of the allocno
     class.  */
  HardRegSet profitable_hard_regs;
  /* The allocno hard registers node.  */
  AllocnoHardRegsNode * hard_regs_node;
  /* Array of structures allocno_hard_regs_subnode representing
     given allocno hard registers node (the 1st element in the array)
     and all its subnodes in the tree (forest) of allocno hard
     register nodes (see comments above).  */
  int hard_regs_subnodes_start;
  /* The length of the previous array.  */
  int hard_regs_subnodes_num;
  /* Records about updating allocno hard reg costs from copies.  If
     the allocno did not get expected hard register, these records are
     used to restore original hard reg costs of allocnos connected to
     this allocno by copies.  */
  struct update_cost_record *update_cost_records;
  /* Threads.  We collect allocnos connected by copies into threads
     and try to assign hard regs to allocnos by threads.  */
  /* Allocno representing all thread.  */
  MtcsIraAllocno * first_thread_allocno;
  /* Allocnos in thread forms a cycle list through the following
     member.  */
  MtcsIraAllocno * next_thread_allocno;
  /* All thread frequency.  Defined only for first thread allocno.  */
  int thread_freq;
  /* Sum of frequencies of hard register preferences of the allocno.  */
  int hard_reg_prefs;
};


/* Describes one element in a queue of allocnos whose costs need to be
   updated.  Each allocno in the queue is known to have an allocno
   class.  */
struct _UpdateCostQueueElem
{
  /* This element is in the queue iff CHECK == update_cost_check.  */
  int check;

  /* COST_HOP_DIVISOR**N, where N is the length of the shortest path
     connecting this allocno to the one being allocated.  */
  int divisor;

  /* Allocno from which we started chaining costs of connected
     allocnos. */
  MtcsIraAllocno * start;

  /* Allocno from which we are chaining costs of connected allocnos.
     It is used not go back in graph of allocnos connected by
     copies.  */
  MtcsIraAllocno * from;

  /* The next allocno in the queue, or null if this is the last element.  */
  MtcsIraAllocno * next;
};

/* The structure is used to describes all subnodes (not only immediate
   ones) in the mentioned above tree for given allocno hard register
   node.  The usage of such data accelerates calculation of
   colorability of given allocno.  */
struct _AllocnoHardRegsSubnode
{
  /* The conflict size of conflicting allocnos whose hard register
     sets are equal sets (plus supersets if given node is given
     allocno hard registers node) of one in the given node.  */
  int left_conflict_size;
  /* The summary conflict size of conflicting allocnos whose hard
     register sets are strict subsets of one in the given node.
     Overall conflict size is
     left_conflict_subnodes_size
       + MIN (max_node_impact - left_conflict_subnodes_size,
              left_conflict_size)
  */
  short left_conflict_subnodes_size;
  short max_node_impact;
};



/* Macro to access the data concerning coloring.  */
#define ALLOCNO_COLOR_DATA(a) ((AllocnoColorData *) a->add_data)

/* Helper for qsort comparison callbacks - return a positive integer if
   X > Y, or a negative value otherwise.  Use a conditional expression
   instead of a difference computation to insulate from possible overflow
   issues, e.g. X - Y < 0 for some X > 0 and Y < 0.  */
#define SORTGT(x,y) (((x) > (y)) ? 1 : -1)


struct allocno_hard_regs_hasher : nofree_ptr_hash <AllocnoHardRegs>
{
  static inline hashval_t hash (const AllocnoHardRegs *);
  static inline bool equal (const AllocnoHardRegs *,const AllocnoHardRegs *);
};

/* Returns hash value for allocno hard registers V.  */
inline hashval_t allocno_hard_regs_hasher::hash (const AllocnoHardRegs *hv)
{
  return iterative_hash (&hv->set, sizeof (HardRegSet/*!HARD_REG_SET*/), 0);
}

/* Compares allocno hard registers V1 and V2.  */
inline bool allocno_hard_regs_hasher::equal (const AllocnoHardRegs *hv1,
             const AllocnoHardRegs *hv2)
{
  return hv1->set == hv2->set;
}

/* Return allocno hard registers in the hash table equal to HV.  */
static AllocnoHardRegs *find_hard_regs (MtcsIraColor *self,AllocnoHardRegs * hv)
{
  return self->allocno_hard_regs_htab->find (hv);
}

/* Insert allocno hard registers HV in the hash table (if it is not
   there yet) and return the value which in the table.  */
static AllocnoHardRegs *insert_hard_regs (MtcsIraColor *self,AllocnoHardRegs * hv)
{
   AllocnoHardRegs **slot = self->allocno_hard_regs_htab->find_slot (hv, INSERT);
   if (*slot == NULL)
      *slot = hv;
   return *slot;
}

/* Initialize data concerning allocno hard registers.  */
static void init_allocno_hard_regs (MtcsIraColor *self)
{
  self->allocno_hard_regs_vec.create (200);
  self->allocno_hard_regs_htab = new hash_table<allocno_hard_regs_hasher> (200);
}

/* Add (or update info about) allocno hard registers with SET and
   COST.  */
static AllocnoHardRegs * add_allocno_hard_regs (MtcsIraColor *self,HardRegSet *set, int64_t cost)
{
   AllocnoHardRegs temp;
   AllocnoHardRegs * hv;

   gcc_assert (! mtcs_reg_hard_reg_set_empty_p/*!hard_reg_set_empty_p*/(set));
   temp.set = *set;
   if ((hv = find_hard_regs(self,&temp)) != NULL)
      hv->cost += cost;
   else{
      hv = ((AllocnoHardRegs *)ira_allocate (sizeof (AllocnoHardRegs)));
      hv->set = *set;
      hv->cost = cost;
      self->allocno_hard_regs_vec.safe_push (hv);
      insert_hard_regs(self,hv);
   }
   return hv;
}

/* Finalize data concerning allocno hard registers.  */
static void finish_allocno_hard_regs (MtcsIraColor *self)
{
   int i;
   AllocnoHardRegs * hv;

   for (i = 0; self->allocno_hard_regs_vec.iterate (i, &hv); i++)
      ira_free (hv);
   delete self->allocno_hard_regs_htab;
   self->allocno_hard_regs_htab = NULL;
   self->allocno_hard_regs_vec.release ();
}

/* Sort hard regs according to their frequency of usage. */
static int allocno_hard_regs_compare_cb (const void *v1p, const void *v2p)
{
  AllocnoHardRegs * hv1 = *(const AllocnoHardRegs * *) v1p;
  AllocnoHardRegs * hv2 = *(const AllocnoHardRegs * *) v2p;

  if (hv2->cost > hv1->cost)
    return 1;
  else if (hv2->cost < hv1->cost)
    return -1;
  return SORTGT (allocno_hard_regs_hasher::hash(hv2), allocno_hard_regs_hasher::hash(hv1));
}

/* Create and return allocno hard registers node containing allocno
   hard registers HV.  */
static AllocnoHardRegsNode *create_new_allocno_hard_regs_node (MtcsIraColor *self,AllocnoHardRegs * hv)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   AllocnoHardRegsNode * new_node;

   new_node = ((AllocnoHardRegsNode *) ira_allocate (sizeof (AllocnoHardRegsNode)));
   new_node->check = 0;
   new_node->hard_regs = hv;
   new_node->hard_regs_num = mtcs_ira_hard_reg_set_size/*!hard_reg_set_size*/(mtcsIra, &hv->set);
   new_node->first = NULL;
   new_node->used_p = false;
   return new_node;
}

/* Add allocno hard registers node NEW_NODE to the forest on its level
   given by ROOTS.  */
static void add_new_allocno_hard_regs_node_to_forest (MtcsIraColor *self,AllocnoHardRegsNode **roots,
                 AllocnoHardRegsNode * new_node)
{
   new_node->next = *roots;
   if (new_node->next != NULL)
      new_node->next->prev = new_node;
   new_node->prev = NULL;
   *roots = new_node;
}

/* Add allocno hard registers HV (or its best approximation if it is
   not possible) to the forest on its level given by ROOTS.  */
static void add_allocno_hard_regs_to_forest (MtcsIraColor *self,AllocnoHardRegsNode **roots,AllocnoHardRegs *hv)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   unsigned int i, start;
   AllocnoHardRegsNode * node, *prev, *new_node;
   HardRegSet /*!HARD_REG_SET*/ temp_set = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};

   AllocnoHardRegs * hv2;

   start = self->hard_regs_node_vec.length ();
   for (node = *roots; node != NULL; node = node->next){
      if (hv->set == node->hard_regs->set)
         return;
      if (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&hv->set, &node->hard_regs->set)) {
         add_allocno_hard_regs_to_forest(self,&node->first, hv);
         return;
      }
      if (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&node->hard_regs->set, &hv->set))
         self->hard_regs_node_vec.safe_push (node);
      else if (mtcs_reg_hard_reg_set_intersect_p/*!hard_reg_set_intersect_p*/(&hv->set, &node->hard_regs->set)){
         temp_set = hv->set & node->hard_regs->set;
         hv2 = add_allocno_hard_regs(self,&temp_set, hv->cost);
         add_allocno_hard_regs_to_forest(self,&node->first, hv2);
      }
   }
   if (self->hard_regs_node_vec.length () > start + 1){
      /* Create a new node which contains nodes in hard_regs_node_vec.  */
      mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&temp_set);
      for (i = start;  i < self->hard_regs_node_vec.length ();  i++){
         node = self->hard_regs_node_vec[i];
         temp_set |= node->hard_regs->set;
      }
      hv = add_allocno_hard_regs(self,&temp_set, hv->cost);
      new_node = create_new_allocno_hard_regs_node(self,hv);
      prev = NULL;
      for (i = start; i < self->hard_regs_node_vec.length (); i++){
         node = self->hard_regs_node_vec[i];
         if (node->prev == NULL)
            *roots = node->next;
         else
            node->prev->next = node->next;
         if (node->next != NULL)
            node->next->prev = node->prev;
         if (prev == NULL)
            new_node->first = node;
         else
            prev->next = node;
         node->prev = prev;
         node->next = NULL;
         prev = node;
      }
      add_new_allocno_hard_regs_node_to_forest(self,roots, new_node);
   }
   self->hard_regs_node_vec.truncate (start);
}

/* Add allocno hard registers nodes starting with the forest level
   given by FIRST which contains biggest set inside SET.  */
static void collect_allocno_hard_regs_cover (MtcsIraColor *self,AllocnoHardRegsNode *first, HardRegSet *set)
{
   AllocnoHardRegsNode * node;

   ira_assert (first != NULL);
   for (node = first; node != NULL; node = node->next)
      if (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&node->hard_regs->set, set))
         self->hard_regs_node_vec.safe_push (node);
      else if (mtcs_reg_hard_reg_set_intersect_p/*!hard_reg_set_intersect_p*/(set, &node->hard_regs->set))
         collect_allocno_hard_regs_cover(self,node->first, set);
}

/* Set up field parent as PARENT in all allocno hard registers nodes
   in forest given by FIRST.  */
static void setup_allocno_hard_regs_nodes_parent (MtcsIraColor *self,AllocnoHardRegsNode * first, AllocnoHardRegsNode * parent)
{
   AllocnoHardRegsNode * node;

   for (node = first; node != NULL; node = node->next){
      node->parent = parent;
      setup_allocno_hard_regs_nodes_parent(self,node->first, node);
   }
}

/* Return allocno hard registers node which is a first common ancestor
   node of FIRST and SECOND in the forest.  */
static AllocnoHardRegsNode *first_common_ancestor_node (MtcsIraColor *self,AllocnoHardRegsNode *first, AllocnoHardRegsNode *second)
{
   AllocnoHardRegsNode * node;

   self->node_check_tick++;
   for (node = first; node != NULL; node = node->parent)
      node->check = self->node_check_tick;
   for (node = second; node != NULL; node = node->parent)
      if (node->check == self->node_check_tick)
         return node;
   return first_common_ancestor_node(self,second, first);
}

/* Print hard reg set SET to F.  */
static void print_hard_reg_set (MtcsIraColor *self,FILE *f, HardRegSet *set, bool new_line_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);
   int i, start, end;

   for (start = end = -1, i = 0; i < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; i++){
      bool reg_included = mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(set, i);

      if (reg_included){
         if (start == -1)
            start = i;
         end = i;
      }
      if (start >= 0 && (!reg_included || i == firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/ - 1)){
         if (start == end)
            fprintf (f, " %d", start);
         else if (start == end + 1)
            fprintf (f, " %d %d", start, end);
         else
            fprintf (f, " %d-%d", start, end);
         start = -1;
      }
   }
   if (new_line_p)
   fprintf (f, "\n");
}

/* Dump a hard reg set SET to stderr.  */
//原型 debug_hard_reg_set sel-sched-dump.h ira-color.cc
DEBUG_FUNCTION void mtcs_ira_color_debug_hard_reg_set (MtcsIraColor *self,HardRegSet *set)
{
  print_hard_reg_set(self,stderr, set, true);
}

/* Print allocno hard register subforest given by ROOTS and its LEVEL
   to F.  */
static void print_hard_regs_subforest (MtcsIraColor *self,FILE *f, AllocnoHardRegsNode * roots,int level)
{
   int i;
   AllocnoHardRegsNode * node;

   for (node = roots; node != NULL; node = node->next){
      fprintf (f, "    ");
      for (i = 0; i < level * 2; i++)
         fprintf (f, " ");
      fprintf (f, "%d:(", node->preorder_num);
      print_hard_reg_set(self,f, &node->hard_regs->set, false);
      fprintf (f, ")@%" PRId64"\n", node->hard_regs->cost);
      print_hard_regs_subforest(self,f, node->first, level + 1);
   }
}

/* Print the allocno hard register forest to F.  */
static void print_hard_regs_forest (MtcsIraColor *self,FILE *f)
{
   fprintf (f, "    Hard reg set forest:\n");
   print_hard_regs_subforest(self,f, self->hard_regs_roots, 1);
}

/* Print the allocno hard register forest to stderr.  */
//原型 ira_debug_hard_regs_forest ira-int.h ira-color.cc
void mtcs_ira_color_debug_hard_regs_forest (MtcsIraColor *self)
{
   print_hard_regs_forest(self,stderr);
}

/* Remove unused allocno hard registers nodes from forest given by its
   *ROOTS.  */
static void remove_unused_allocno_hard_regs_nodes (MtcsIraColor *self,AllocnoHardRegsNode * *roots)
{
   AllocnoHardRegsNode * node, *prev, *next, *last;

   for (prev = NULL, node = *roots; node != NULL; node = next){
      next = node->next;
      if (node->used_p){
         remove_unused_allocno_hard_regs_nodes(self,&node->first);
         prev = node;
      }else{
         for (last = node->first;last != NULL && last->next != NULL;last = last->next)
            ;
         if (last != NULL){
            if (prev == NULL)
               *roots = node->first;
            else
               prev->next = node->first;
            if (next != NULL)
               next->prev = last;
            last->next = next;
            next = node->first;
         }else{
            if (prev == NULL)
               *roots = next;
            else
               prev->next = next;
            if (next != NULL)
               next->prev = prev;
         }
         ira_free (node);
      }
   }
}

/* Set up fields preorder_num starting with START_NUM in all allocno
   hard registers nodes in forest given by FIRST.  Return biggest set
   PREORDER_NUM increased by 1.  */
static int enumerate_allocno_hard_regs_nodes (MtcsIraColor *self,AllocnoHardRegsNode *first,
               AllocnoHardRegsNode *parent, int start_num)
{
   AllocnoHardRegsNode *node;
   for (node = first; node != NULL; node = node->next){
      node->preorder_num = start_num++;
      node->parent = parent;
      start_num = enumerate_allocno_hard_regs_nodes(self,node->first, node,start_num);
   }
   return start_num;
}

/* Setup arrays ALLOCNO_HARD_REGS_NODES and
   ALLOCNO_HARD_REGS_SUBNODE_INDEX.  */
static void setup_allocno_hard_regs_subnode_index (MtcsIraColor *self,AllocnoHardRegsNode * first)
{
   AllocnoHardRegsNode * node, *parent;
   int index;

   for (node = first; node != NULL; node = node->next){
      self->allocno_hard_regs_nodes[node->preorder_num] = node;
      for (parent = node; parent != NULL; parent = parent->parent){
         index = parent->preorder_num * self->allocno_hard_regs_nodes_num;
         self->allocno_hard_regs_subnode_index[index + node->preorder_num] = node->preorder_num - parent->preorder_num;
      }
      setup_allocno_hard_regs_subnode_index(self,node->first);
   }
}

/* Count all allocno hard registers nodes in tree ROOT.  */
static int get_allocno_hard_regs_subnodes_num (MtcsIraColor *self,AllocnoHardRegsNode * root)
{
   int len = 1;
   for (root = root->first; root != NULL; root = root->next)
      len += get_allocno_hard_regs_subnodes_num(self,root);
   return len;
}

/* Build the forest of allocno hard registers nodes and assign each
   allocno a node from the forest.  */
static void form_allocno_hard_regs_nodes_forest (MtcsIraColor *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);

   unsigned int i, j, size, len;
   int start;
   MtcsIraAllocno * a;
   AllocnoHardRegs * hv;
   bitmap_iterator bi;
   HardRegSet /*!HARD_REG_SET*/ temp = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};

   AllocnoHardRegsNode *node, *allocno_hard_regs_node;
   AllocnoColorData * allocno_data;

   self->node_check_tick = 0;
   init_allocno_hard_regs(self);
   self->hard_regs_roots = NULL;
   self->hard_regs_node_vec.create (100);

   for (i = 0; i < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i++)
      if (! mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&mtcsIra->x_ira_no_alloc_regs, i)){
         mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&temp);
         mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&temp, i);
         hv = add_allocno_hard_regs(self,&temp, 0);
         node = create_new_allocno_hard_regs_node(self,hv);
         add_new_allocno_hard_regs_node_to_forest(self,&self->hard_regs_roots, node);
      }
   start = self->allocno_hard_regs_vec.length ();
   EXECUTE_IF_SET_IN_BITMAP (self->coloring_allocno_bitmap, 0, i, bi){
      a = mtcsIraBuild->ira_allocnos[i];
      allocno_data = ALLOCNO_COLOR_DATA (a);

      if (mtcs_reg_hard_reg_set_empty_p/*!hard_reg_set_empty_p*/(&allocno_data->profitable_hard_regs))
         continue;
      hv = (add_allocno_hard_regs(self,&allocno_data->profitable_hard_regs,a->memory_cost - a->class_cost));
   }
   temp = ~mtcsIra->x_ira_no_alloc_regs;
   add_allocno_hard_regs(self,&temp, 0);
   qsort (self->allocno_hard_regs_vec.address () + start,
         self->allocno_hard_regs_vec.length () - start, sizeof (AllocnoHardRegs *), allocno_hard_regs_compare_cb);
   for (i = start; self->allocno_hard_regs_vec.iterate (i, &hv); i++){
      add_allocno_hard_regs_to_forest(self,&self->hard_regs_roots, hv);
      ira_assert (self->hard_regs_node_vec.length () == 0);
   }
   /* We need to set up parent fields for right work of
   first_common_ancestor_node. */
   setup_allocno_hard_regs_nodes_parent(self,self->hard_regs_roots, NULL);

   EXECUTE_IF_SET_IN_BITMAP (self->coloring_allocno_bitmap, 0, i, bi){
      a = mtcsIraBuild->ira_allocnos[i];
      allocno_data = ALLOCNO_COLOR_DATA (a);
      if (mtcs_reg_hard_reg_set_empty_p/*!hard_reg_set_empty_p*/(&allocno_data->profitable_hard_regs))
         continue;
      self->hard_regs_node_vec.truncate (0);
      collect_allocno_hard_regs_cover(self,self->hard_regs_roots,&allocno_data->profitable_hard_regs);
      allocno_hard_regs_node = NULL;
      for (j = 0; self->hard_regs_node_vec.iterate (j, &node); j++)
         allocno_hard_regs_node = (j == 0 ? node: first_common_ancestor_node(self,node, allocno_hard_regs_node));
      /* That is a temporary storage.  */
      allocno_hard_regs_node->used_p = true;
      allocno_data->hard_regs_node = allocno_hard_regs_node;
   }

   ira_assert (self->hard_regs_roots->next == NULL);
   self->hard_regs_roots->used_p = true;
   remove_unused_allocno_hard_regs_nodes(self,&self->hard_regs_roots);
   self->allocno_hard_regs_nodes_num = enumerate_allocno_hard_regs_nodes(self,self->hard_regs_roots, NULL, 0);
   self->allocno_hard_regs_nodes =
         ((AllocnoHardRegsNode **)ira_allocate (self->allocno_hard_regs_nodes_num * sizeof (AllocnoHardRegsNode *)));
   size = self->allocno_hard_regs_nodes_num * self->allocno_hard_regs_nodes_num;
   self->allocno_hard_regs_subnode_index = (int *) ira_allocate (size * sizeof (int));
   for (i = 0; i < size; i++)
      self->allocno_hard_regs_subnode_index[i] = -1;
   setup_allocno_hard_regs_subnode_index(self,self->hard_regs_roots);
   start = 0;

   EXECUTE_IF_SET_IN_BITMAP (self->coloring_allocno_bitmap, 0, i, bi){
      a = mtcsIraBuild->ira_allocnos[i];
      allocno_data = ALLOCNO_COLOR_DATA (a);
      if (mtcs_reg_hard_reg_set_empty_p/*!hard_reg_set_empty_p*/(&allocno_data->profitable_hard_regs))
         continue;
      len = get_allocno_hard_regs_subnodes_num(self,allocno_data->hard_regs_node);
      allocno_data->hard_regs_subnodes_start = start;
      allocno_data->hard_regs_subnodes_num = len;
      start += len;
   }
   self->allocno_hard_regs_subnodes = ((AllocnoHardRegsSubnode *) ira_allocate (sizeof (AllocnoHardRegsSubnode) * start));
   self->hard_regs_node_vec.release ();
}

/* Free tree of allocno hard registers nodes given by its ROOT.  */
static void finish_allocno_hard_regs_nodes_tree (MtcsIraColor *self,AllocnoHardRegsNode * root)
{
   AllocnoHardRegsNode * child, *next;
   for (child = root->first; child != NULL; child = next){
      next = child->next;
      finish_allocno_hard_regs_nodes_tree(self,child);
   }
   ira_free (root);
}

/* Finish work with the forest of allocno hard registers nodes.  */
static void finish_allocno_hard_regs_nodes_forest (MtcsIraColor *self)
{
   AllocnoHardRegsNode *node, *next;
   ira_free (self->allocno_hard_regs_subnodes);
   for (node = self->hard_regs_roots; node != NULL; node = next){
      next = node->next;
      finish_allocno_hard_regs_nodes_tree(self,node);
   }
   ira_free (self->allocno_hard_regs_nodes);
   ira_free (self->allocno_hard_regs_subnode_index);
   finish_allocno_hard_regs(self);
}

/* Set up left conflict sizes and left conflict subnodes sizes of hard
   registers subnodes of allocno A.  Return TRUE if allocno A is
   trivially colorable.  */
static bool setup_left_conflict_sizes_p (MtcsIraColor *self,MtcsIraAllocno * a)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);

   int i, k, nobj, start;
   int conflict_size, left_conflict_subnodes_size, node_preorder_num;
   AllocnoColorData * data;
   HardRegSet /*!HARD_REG_SET*/ profitable_hard_regs = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};
   AllocnoHardRegsSubnode * subnodes;
   AllocnoHardRegsNode * node;
   HardRegSet /*!HARD_REG_SET*/ node_set = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};


   nobj = a->num_objects;
   data = ALLOCNO_COLOR_DATA (a);
   subnodes = self->allocno_hard_regs_subnodes + data->hard_regs_subnodes_start;
   profitable_hard_regs = data->profitable_hard_regs;
   node = data->hard_regs_node;
   node_preorder_num = node->preorder_num;
   node_set = node->hard_regs->set;
   self->node_check_tick++;

   for (k = 0; k < nobj; k++){
      MtcsIraObject * obj =a->objects[k];
      MtcsIraObject * conflict_obj;
      MtcsIraObjectConflictIterator oci;

      MTCS_FOR_EACH_OBJECT_CONFLICT(mtcsIraBuild,obj, conflict_obj, oci){
         int size;
         MtcsIraAllocno * conflict_a = conflict_obj->allocno;
         AllocnoHardRegsNode * conflict_node, *temp_node;
         HardRegSet /*!HARD_REG_SET*/ conflict_node_set = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};
         AllocnoColorData * conflict_data;

         conflict_data = ALLOCNO_COLOR_DATA (conflict_a);
         if (! ALLOCNO_COLOR_DATA (conflict_a)->in_graph_p
         || ! mtcs_reg_hard_reg_set_intersect_p/*!hard_reg_set_intersect_p*/(&profitable_hard_regs,&conflict_data->profitable_hard_regs))
            continue;
         conflict_node = conflict_data->hard_regs_node;
         conflict_node_set = conflict_node->hard_regs->set;
         if (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&node_set, &conflict_node_set))
            temp_node = node;
         else{
            ira_assert (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&conflict_node_set, &node_set));
            temp_node = conflict_node;
         }
         if (temp_node->check != self->node_check_tick){
            temp_node->check = self->node_check_tick;
            temp_node->conflict_size = 0;
         }
         size = (mtcsIra->x_ira_reg_class_max_nregs[conflict_a->aclass][conflict_a->mode]);
         if (conflict_a->num_objects > 1)
            /* We will deal with the subwords individually.  */
            size = 1;
         temp_node->conflict_size += size;
      }
   }
   for (i = 0; i < data->hard_regs_subnodes_num; i++){
      AllocnoHardRegsNode * temp_node;

      temp_node = self->allocno_hard_regs_nodes[i + node_preorder_num];
      ira_assert (temp_node->preorder_num == i + node_preorder_num);
      subnodes[i].left_conflict_size = (temp_node->check != self->node_check_tick
      ? 0 : temp_node->conflict_size);
      if (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&temp_node->hard_regs->set, &profitable_hard_regs))
         subnodes[i].max_node_impact = temp_node->hard_regs_num;
      else{
         HardRegSet /*!HARD_REG_SET*/ temp_set = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};

         int j, n, hard_regno;
         enum reg_class aclass;

         temp_set = temp_node->hard_regs->set & profitable_hard_regs;
         aclass = a->aclass;
         for (n = 0, j = mtcsIra->x_ira_class_hard_regs_num[aclass] - 1; j >= 0; j--){
            hard_regno = mtcsIra->x_ira_class_hard_regs[aclass][j];
            if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&temp_set, hard_regno))
               n++;
         }
         subnodes[i].max_node_impact = n;
      }
      subnodes[i].left_conflict_subnodes_size = 0;
   }
   start = node_preorder_num * self->allocno_hard_regs_nodes_num;
   for (i = data->hard_regs_subnodes_num - 1; i > 0; i--){
      int size, parent_i;
      AllocnoHardRegsNode * parent;

      size = (subnodes[i].left_conflict_subnodes_size + MIN (subnodes[i].max_node_impact -
            subnodes[i].left_conflict_subnodes_size, subnodes[i].left_conflict_size));
      parent = self->allocno_hard_regs_nodes[i + node_preorder_num]->parent;
      gcc_checking_assert(parent);
      parent_i = self->allocno_hard_regs_subnode_index[start + parent->preorder_num];
      gcc_checking_assert(parent_i >= 0);
      subnodes[parent_i].left_conflict_subnodes_size += size;
   }
   left_conflict_subnodes_size = subnodes[0].left_conflict_subnodes_size;
   conflict_size = (left_conflict_subnodes_size  + MIN (subnodes[0].max_node_impact -
         left_conflict_subnodes_size, subnodes[0].left_conflict_size));
   conflict_size += mtcsIra->x_ira_reg_class_max_nregs[a->aclass][a->mode];
   data->colorable_p = conflict_size <= data->available_regs_num;
   return data->colorable_p;
}

/* Update left conflict sizes of hard registers subnodes of allocno A
   after removing allocno REMOVED_A with SIZE from the conflict graph.
   Return TRUE if A is trivially colorable.  */
static bool update_left_conflict_sizes_p (MtcsIraColor *self,MtcsIraAllocno *a,
               MtcsIraAllocno *removed_a, int size)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int i, conflict_size, before_conflict_size, diff, start;
   int node_preorder_num, parent_i;
   AllocnoHardRegsNode * node, *removed_node, *parent;
   AllocnoHardRegsSubnode * subnodes;
   AllocnoColorData * data = ALLOCNO_COLOR_DATA (a);

   ira_assert (! data->colorable_p);
   node = data->hard_regs_node;
   node_preorder_num = node->preorder_num;
   removed_node = ALLOCNO_COLOR_DATA (removed_a)->hard_regs_node;
   ira_assert (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&removed_node->hard_regs->set,&node->hard_regs->set)
   || mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&node->hard_regs->set,&removed_node->hard_regs->set));
   start = node_preorder_num * self->allocno_hard_regs_nodes_num;
   i = self->allocno_hard_regs_subnode_index[start + removed_node->preorder_num];
   if (i < 0)
      i = 0;
   subnodes = self->allocno_hard_regs_subnodes + data->hard_regs_subnodes_start;
   before_conflict_size = (subnodes[i].left_conflict_subnodes_size + MIN (subnodes[i].max_node_impact
   - subnodes[i].left_conflict_subnodes_size, subnodes[i].left_conflict_size));
   subnodes[i].left_conflict_size -= size;
   for (;;){
      conflict_size = (subnodes[i].left_conflict_subnodes_size + MIN (subnodes[i].max_node_impact
            - subnodes[i].left_conflict_subnodes_size, subnodes[i].left_conflict_size));
      if ((diff = before_conflict_size - conflict_size) == 0)
         break;
      ira_assert (conflict_size < before_conflict_size);
      parent = self->allocno_hard_regs_nodes[i + node_preorder_num]->parent;
      if (parent == NULL)
         break;
      parent_i = self->allocno_hard_regs_subnode_index[start + parent->preorder_num];
      if (parent_i < 0)
         break;
      i = parent_i;
      before_conflict_size = (subnodes[i].left_conflict_subnodes_size + MIN (subnodes[i].max_node_impact
            - subnodes[i].left_conflict_subnodes_size, subnodes[i].left_conflict_size));
      subnodes[i].left_conflict_subnodes_size -= diff;
   }
   if (i != 0  || (conflict_size + mtcsIra->x_ira_reg_class_max_nregs[a->aclass][a->mode] > data->available_regs_num))
      return false;
   data->colorable_p = true;
   return true;
}

/* Return true if allocno A has empty profitable hard regs.  */
static bool empty_profitable_hard_regs (MtcsIraColor *self,MtcsIraAllocno * a)
{
   AllocnoColorData * data = ALLOCNO_COLOR_DATA (a);
   return mtcs_reg_hard_reg_set_empty_p/*!hard_reg_set_empty_p*/(&data->profitable_hard_regs);
}

/* Set up profitable hard registers for each allocno being
   colored.  */
static void setup_profitable_hard_regs (MtcsIraColor *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);

   unsigned int i;
   int j, k, nobj, hard_regno, nregs, class_size;
   MtcsIraAllocno * a;
   bitmap_iterator bi;
   enum reg_class aclass;
   machine_mode mode;
   AllocnoColorData * data;

   /* Initial set up from allocno classes and explicitly conflicting
   hard regs.  */
   EXECUTE_IF_SET_IN_BITMAP (self->coloring_allocno_bitmap, 0, i, bi){
      a = mtcsIraBuild->ira_allocnos[i];
      if ((aclass = a->aclass) == NO_REGS)
         continue;
      data = ALLOCNO_COLOR_DATA (a);
      if (a->updated_hard_reg_costs == NULL  && a->class_cost > a->memory_cost
      /* Do not empty profitable regs for static chain pointer
      pseudo when non-local goto is used.  */
      && ! mtcs_ira_non_spilled_static_chain_regno_p/*!non_spilled_static_chain_regno_p*/(mtcsIra,a->regno))
         mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&data->profitable_hard_regs);
      else{
         mode =a->mode;
         data->profitable_hard_regs = mtcsIraInt->x_ira_useful_class_mode_regs[aclass][mode];
         nobj = a->num_objects;
         for (k = 0; k < nobj; k++){
            MtcsIraObject * obj = a->objects[k];
            data->profitable_hard_regs &= ~obj->total_conflict_hard_regs;
         }
      }
   }
   /* Exclude hard regs already assigned for conflicting objects.  */
   EXECUTE_IF_SET_IN_BITMAP (self->consideration_allocno_bitmap, 0, i, bi){
      a = mtcsIraBuild->ira_allocnos[i];
      if ((aclass = a->aclass) == NO_REGS || ! a->assigned_p || (hard_regno = a->hard_regno) < 0)
         continue;
      mode = a->mode;
      nregs = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,hard_regno, mode);
      nobj = a->num_objects;
      for (k = 0; k < nobj; k++){
         MtcsIraObject * obj = a->objects[k];
         MtcsIraObject * conflict_obj;
         MtcsIraObjectConflictIterator oci;

         MTCS_FOR_EACH_OBJECT_CONFLICT(mtcsIraBuild,obj, conflict_obj, oci){
            MtcsIraAllocno * conflict_a = conflict_obj->allocno;

            /* We can process the conflict allocno repeatedly with
            the same result.  */
            if (nregs == nobj && nregs > 1){
               int num = conflict_obj->subword;

               if (REG_WORDS_BIG_ENDIAN)
                  mtcs_reg_clear_hard_reg_bit/*!CLEAR_HARD_REG_BIT*/(mtcsReg,
                        &ALLOCNO_COLOR_DATA (conflict_a)->profitable_hard_regs,hard_regno + nobj - num - 1);
               else
                  mtcs_reg_clear_hard_reg_bit/*!CLEAR_HARD_REG_BIT*/(mtcsReg,
                        &ALLOCNO_COLOR_DATA (conflict_a)->profitable_hard_regs,hard_regno + num);
            }else
               ALLOCNO_COLOR_DATA (conflict_a)->profitable_hard_regs  &= ~mtcsIraInt->x_ira_reg_mode_hard_regset[hard_regno][mode];
         }
      }
   }
   /* Exclude too costly hard regs.  */
   EXECUTE_IF_SET_IN_BITMAP (self->coloring_allocno_bitmap, 0, i, bi){
      int min_cost = INT_MAX;
      int *costs;

      a = mtcsIraBuild->ira_allocnos[i];
      if ((aclass = a->aclass) == NO_REGS || empty_profitable_hard_regs(self,a))
         continue;
      data = ALLOCNO_COLOR_DATA (a);
      if ((costs = a->updated_hard_reg_costs) != NULL || (costs = a->hard_reg_costs) != NULL){
         class_size = mtcsIra->x_ira_class_hard_regs_num[aclass];
         for (j = 0; j < class_size; j++){
            hard_regno = mtcsIra->x_ira_class_hard_regs[aclass][j];
            if (! mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&data->profitable_hard_regs,hard_regno))
               continue;
            if (a->updated_memory_cost < costs[j]
            /* Do not remove HARD_REGNO for static chain pointer
            pseudo when non-local goto is used.  */
            && !  mtcs_ira_non_spilled_static_chain_regno_p/*!non_spilled_static_chain_regno_p*/(mtcsIra,a->regno))
               mtcs_reg_clear_hard_reg_bit/*!CLEAR_HARD_REG_BIT*/(mtcsReg, &data->profitable_hard_regs,hard_regno);
            else if (min_cost > costs[j])
               min_cost = costs[j];
         }
      }else if (a->updated_memory_cost < a->updated_class_cost
      /* Do not empty profitable regs for static chain
      pointer pseudo when non-local goto is used.  */
      && !  mtcs_ira_non_spilled_static_chain_regno_p/*!non_spilled_static_chain_regno_p*/(mtcsIra,a->regno))
         mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&data->profitable_hard_regs);

      if (a->updated_class_cost > min_cost)
         a->updated_class_cost = min_cost;
   }
}



/* This page contains functions used to choose hard registers for
   allocnos.  */

/* Pool for update cost records.  */
static object_allocator<update_cost_record> update_cost_record_pool("update cost records");

/* Return new update cost record with given params.  */
static struct update_cost_record *get_update_cost_record (MtcsIraColor *self,int hard_regno, int divisor,
         struct update_cost_record *next)
{
   struct update_cost_record *record;
   record = update_cost_record_pool.allocate ();
   record->hard_regno = hard_regno;
   record->divisor = divisor;
   record->next = next;
   return record;
}

/* Free memory for all records in LIST.  */
static void free_update_cost_record_list (MtcsIraColor *self,struct update_cost_record *list)
{
   struct update_cost_record *next;
   while (list != NULL){
      next = list->next;
      update_cost_record_pool.remove (list);
      list = next;
   }
}

/* Free memory allocated for all update cost records.  */
static void finish_update_cost_records (MtcsIraColor *self)
{
   update_cost_record_pool.release ();
}



/* Allocate and initialize data necessary for function
   update_costs_from_copies.  */
static void initiate_cost_update (MtcsIraColor *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);

   size_t size;

   size = mtcsIraBuild->ira_allocnos_num * sizeof (UpdateCostQueueElem);
   self->update_cost_queue_elems = (UpdateCostQueueElem *) ira_allocate (size);
   memset (self->update_cost_queue_elems, 0, size);
   self->update_cost_check = 0;
}

/* Deallocate data used by function update_costs_from_copies.  */
static void finish_cost_update (MtcsIraColor *self)
{
   ira_free (self->update_cost_queue_elems);
   finish_update_cost_records(self);
}

/* When we traverse allocnos to update hard register costs, the cost
   divisor will be multiplied by the following macro value for each
   hop from given allocno to directly connected allocnos.  */
#define COST_HOP_DIVISOR 4

/* Start a new cost-updating pass.  */
static void start_update_cost (MtcsIraColor *self)
{
   self->update_cost_check++;
   self->update_cost_queue = NULL;
}

/* Add (ALLOCNO, START, FROM, DIVISOR) to the end of self->update_cost_queue, unless
   ALLOCNO is already in the queue, or has NO_REGS class.  */
static inline void queue_update_cost (MtcsIraColor *self,MtcsIraAllocno * allocno, MtcsIraAllocno * start,
         MtcsIraAllocno * from, int divisor)
{
   UpdateCostQueueElem *elem;

   elem = &self->update_cost_queue_elems[allocno->num];
   if (elem->check != self->update_cost_check  && allocno->aclass != NO_REGS){
      elem->check = self->update_cost_check;
      elem->start = start;
      elem->from = from;
      elem->divisor = divisor;
      elem->next = NULL;
      if (self->update_cost_queue == NULL)
         self->update_cost_queue = allocno;
      else
         self->update_cost_queue_tail->next = allocno;
      self->update_cost_queue_tail = elem;
   }
}

/* Try to remove the first element from self->update_cost_queue.  Return
   false if the queue was empty, otherwise make (*ALLOCNO, *START,
   *FROM, *DIVISOR) describe the removed element.  */
static inline bool get_next_update_cost (MtcsIraColor *self,MtcsIraAllocno **allocno, MtcsIraAllocno * *start,
            MtcsIraAllocno * *from, int *divisor)
{
   UpdateCostQueueElem *elem;

   if (self->update_cost_queue == NULL)
      return false;

   *allocno = self->update_cost_queue;
   elem = &self->update_cost_queue_elems[(*allocno)->num];
   *start = elem->start;
   *from = elem->from;
   *divisor = elem->divisor;
   self->update_cost_queue = elem->next;
   return true;
}

/* Increase costs of HARD_REGNO by UPDATE_COST and conflict cost by
   UPDATE_CONFLICT_COST for ALLOCNO.  Return true if we really
   modified the cost.  */
static bool update_allocno_cost (MtcsIraColor *self,MtcsIraAllocno * allocno, int hard_regno,
           int update_cost, int update_conflict_cost)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);

   int i;
   enum reg_class aclass = allocno->aclass;

   i = mtcsIraInt->x_ira_class_hard_reg_index[aclass][hard_regno];
   if (i < 0)
      return false;
   mtcs_ira_build_allocate_and_set_or_copy_costs/*!ira_allocate_and_set_or_copy_costs*/(mtcsIraBuild,
            &allocno->updated_hard_reg_costs, aclass,allocno->updated_class_cost,allocno->hard_reg_costs);
   mtcs_ira_build_allocate_and_set_or_copy_costs/*!ira_allocate_and_set_or_copy_costs*/(mtcsIraBuild,
            &allocno->updated_conflict_hard_reg_costs,aclass, 0, allocno->conflict_hard_reg_costs);
   allocno->updated_hard_reg_costs[i] += update_cost;
   allocno->updated_conflict_hard_reg_costs[i] += update_conflict_cost;
   return true;
}

/* Return TRUE if the object OBJ conflicts with the allocno A.  */
static bool object_conflicts_with_allocno_p (MtcsIraColor *self,MtcsIraObject * obj, MtcsIraAllocno * a)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);

   if  (!obj->conflict_vec_p)
      for (int word = 0; word < a->num_objects; word++){
         MtcsIraObject * another_obj = a->objects[word];
         if (another_obj->id >= obj->min  && another_obj->id <= obj->max
               && TEST_MINMAX_SET_BIT (OBJECT_CONFLICT_BITVEC (obj), another_obj->id,obj->min, obj->max))
            return true;
      }
   else{
      /* If this linear walk ever becomes a bottleneck we could add a
      conflict_vec_sorted_p flag and if not set, sort the conflicts after
      their ID so we can use a binary search.  That would also require
      tracking the actual number of conflicts in the vector to not rely
      on the NULL termination.  */
      MtcsIraObjectConflictIterator oci;
      MtcsIraObject * conflict_obj;
      MTCS_FOR_EACH_OBJECT_CONFLICT(mtcsIraBuild,obj, conflict_obj, oci)
         if (conflict_obj->allocno == a)
            return true;
   }
   return false;
}

/* Return TRUE if allocnos A1 and A2 conflicts. Here we are
   interested only in conflicts of allocnos with intersecting allocno
   classes.  */
static bool allocnos_conflict_p (MtcsIraColor *self,MtcsIraAllocno * a1, MtcsIraAllocno * a2)
{
   /* Compute the upper bound for the linear iteration when the object
   conflicts are represented as a sparse vector.  In particular this
   will make sure we prefer O(1) bitvector testing.  */
   int num_conflicts_in_vec1 = 0, num_conflicts_in_vec2 = 0;
   for (int word = 0; word < a1->num_objects; ++word)
      if (a1->objects[word]->conflict_vec_p)
         num_conflicts_in_vec1 += a1->objects[word]->num_accumulated_conflicts;
   for (int word = 0; word < a2->num_objects; ++word)
      if (a2->objects[word]->conflict_vec_p)
         num_conflicts_in_vec2 += a2->objects[word]->num_accumulated_conflicts;
   if (num_conflicts_in_vec2 < num_conflicts_in_vec1)
      std::swap (a1, a2);

   for (int word = 0; word < a1->num_objects; word++){
      MtcsIraObject * obj = a1->objects[word];
      /* Take preferences of conflicting allocnos into account.  */
      if (object_conflicts_with_allocno_p(self,obj, a2))
         return true;
   }
   return false;
}

/* Update (decrease if DECR_P) HARD_REGNO cost of allocnos connected
   by copies to ALLOCNO to increase chances to remove some copies as
   the result of subsequent assignment.  Update conflict costs.
   Record cost updates if RECORD_P is true.  */
static void update_costs_from_allocno (MtcsIraColor *self,MtcsIraAllocno * allocno, int hard_regno,
            int divisor, bool decr_p, bool record_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int cost, update_cost, update_conflict_cost;
   machine_mode mode;
   enum reg_class rclass, aclass;
   MtcsIraAllocno * another_allocno;
   MtcsIraAllocno *start = allocno;
   MtcsIraAllocno *from = NULL;

   MtcsIraAllocnoCopy *cp, *next_cp;

   rclass = mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,hard_regno);
   do {
      mode = allocno->mode;
      mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,mode);
      for (cp = allocno->allocno_copies; cp != NULL; cp = next_cp){
         if (cp->first == allocno){
            next_cp = cp->next_first_allocno_copy;
            another_allocno = cp->second;
         }else if (cp->second == allocno){
            next_cp = cp->next_second_allocno_copy;
            another_allocno = cp->first;
         }else
            gcc_unreachable ();

         if (another_allocno == from
         || (ALLOCNO_COLOR_DATA (another_allocno) != NULL
         && (ALLOCNO_COLOR_DATA (allocno)->first_thread_allocno
         != ALLOCNO_COLOR_DATA (another_allocno)->first_thread_allocno)))
            continue;

         aclass = another_allocno->aclass;
         if (! mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(
               &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[aclass],hard_regno)
               || another_allocno->assigned_p)
            continue;

         /* If we have different modes use the smallest one.  It is
         a sub-register move.  It is hard to predict what LRA
         will reload (the pseudo or its sub-register) but LRA
         will try to minimize the data movement.  Also for some
         register classes bigger modes might be invalid,
         e.g. DImode for AREG on x86.  For such cases the
         register move cost will be maximal.  */
         mode = narrower_subreg_mode (cp->first->mode,cp->second->mode);

         mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,mode);

         cost = (cp->second == allocno ? mtcsIraInt->x_ira_register_move_cost[mode][rclass][aclass]
                                       : mtcsIraInt->x_ira_register_move_cost[mode][aclass][rclass]);
         if (decr_p)
            cost = -cost;

         update_cost = cp->freq * cost / divisor;
         update_conflict_cost = update_cost;

         if (mtcsIraGlobal->internal_flag_ira_verbose > 5 && mtcsIraGlobal->ira_dump_file != NULL)
            fprintf (mtcsIraGlobal->ira_dump_file,"          a%dr%d (hr%d): update cost by %d, conflict cost by %d\n",
                  another_allocno->num, another_allocno->regno, hard_regno, update_cost, update_conflict_cost);
         if (update_cost == 0)
            continue;

         if (! update_allocno_cost(self,another_allocno, hard_regno, update_cost, update_conflict_cost))
            continue;
         queue_update_cost(self,another_allocno, start, allocno, divisor * COST_HOP_DIVISOR);
         if (record_p && ALLOCNO_COLOR_DATA (another_allocno) != NULL)
            ALLOCNO_COLOR_DATA (another_allocno)->update_cost_records
               = get_update_cost_record(self,hard_regno, divisor,ALLOCNO_COLOR_DATA (another_allocno)->update_cost_records);
      }
   }while (get_next_update_cost(self,&allocno, &start, &from, &divisor));
}

/* Decrease preferred ALLOCNO hard register costs and costs of
   allocnos connected to ALLOCNO through copy.  */
static void update_costs_from_prefs (MtcsIraColor *self,MtcsIraAllocno * allocno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   MtcsIraAllocnoPref *pref;

   start_update_cost(self);
   for (pref = allocno->allocno_prefs; pref != NULL; pref = pref->next_pref){
      if (mtcsIraGlobal->internal_flag_ira_verbose > 5 && mtcsIraGlobal->ira_dump_file != NULL)
         fprintf (mtcsIraGlobal->ira_dump_file, "        Start updating from pref of hr%d for a%dr%d:\n",
               pref->hard_regno, allocno->num, allocno->regno);
      update_costs_from_allocno(self,allocno, pref->hard_regno,COST_HOP_DIVISOR, true, true);
   }
}

/* Update (decrease if DECR_P) the cost of allocnos connected to
   ALLOCNO through copies to increase chances to remove some copies as
   the result of subsequent assignment.  ALLOCNO was just assigned to
   a hard register.  Record cost updates if RECORD_P is true.  */
static void update_costs_from_copies (MtcsIraColor *self,MtcsIraAllocno * allocno, bool decr_p, bool record_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int hard_regno;

   hard_regno = allocno->hard_regno;
   ira_assert (hard_regno >= 0 && allocno->aclass != NO_REGS);
   start_update_cost(self);
   if (mtcsIraGlobal->internal_flag_ira_verbose > 5 && mtcsIraGlobal->ira_dump_file != NULL)
      fprintf (mtcsIraGlobal->ira_dump_file, "        Start updating from a%dr%d by copies:\n",allocno->num, allocno->regno);
   update_costs_from_allocno(self,allocno, hard_regno, 1, decr_p, record_p);
}

/* Update conflict_allocno_hard_prefs of allocnos conflicting with
   ALLOCNO.  */
static void update_conflict_allocno_hard_prefs (MtcsIraColor *self,MtcsIraAllocno * allocno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);

   int l, nr = allocno->num_objects;
   for (l = 0; l < nr; l++){
      MtcsIraObject * conflict_obj;
      MtcsIraObject *obj =allocno->objects[l];
      MtcsIraObjectConflictIterator oci;

      MTCS_FOR_EACH_OBJECT_CONFLICT(mtcsIraBuild,obj, conflict_obj, oci){
         MtcsIraAllocno * conflict_a = conflict_obj->allocno;
         AllocnoColorData * conflict_data = ALLOCNO_COLOR_DATA (conflict_a);
         MtcsIraAllocnoPref *pref;

         if (!(mtcs_reg_hard_reg_set_intersect_p/*!hard_reg_set_intersect_p*/(&ALLOCNO_COLOR_DATA (allocno)->profitable_hard_regs,
         &conflict_data->profitable_hard_regs)))
            continue;
         for (pref = allocno->allocno_prefs; pref != NULL; pref = pref->next_pref)
            conflict_data->conflict_allocno_hard_prefs += pref->freq;
      }
   }
}

/* Restore costs of allocnos connected to ALLOCNO by copies as it was
   before updating costs of these allocnos from given allocno.  This
   is a wise thing to do as if given allocno did not get an expected
   hard reg, using smaller cost of the hard reg for allocnos connected
   by copies to given allocno becomes actually misleading.  Free all
   update cost records for ALLOCNO as we don't need them anymore.  */
static void restore_costs_from_copies (MtcsIraColor *self,MtcsIraAllocno * allocno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   struct update_cost_record *records, *curr;

   if (ALLOCNO_COLOR_DATA (allocno) == NULL)
      return;
   records = ALLOCNO_COLOR_DATA (allocno)->update_cost_records;
   start_update_cost(self);
   if (mtcsIraGlobal->internal_flag_ira_verbose > 5 && mtcsIraGlobal->ira_dump_file != NULL)
      fprintf (mtcsIraGlobal->ira_dump_file, "        Start restoring from a%dr%d:\n",allocno->num, allocno->regno);
   for (curr = records; curr != NULL; curr = curr->next)
      update_costs_from_allocno(self,allocno, curr->hard_regno,curr->divisor, true, false);
   free_update_cost_record_list(self,records);
   ALLOCNO_COLOR_DATA (allocno)->update_cost_records = NULL;
}

/* This function updates COSTS (decrease if DECR_P) for hard_registers
   of ACLASS by conflict costs of the unassigned allocnos
   connected by copies with allocnos in self->update_cost_queue.  This
   update increases chances to remove some copies.  */
static void update_conflict_hard_regno_costs (MtcsIraColor *self,int *costs, enum reg_class aclass,bool decr_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);

   int i, cost, class_size, freq, mult, div, divisor;
   int index, hard_regno;
   int *conflict_costs;
   bool cont_p;
   enum reg_class another_aclass;
   MtcsIraAllocno * allocno, *another_allocno, *start, *from;
   MtcsIraAllocnoCopy * cp, *next_cp;

   while (get_next_update_cost(self,&allocno, &start, &from, &divisor))
      for (cp = allocno->allocno_copies; cp != NULL; cp = next_cp){
         if (cp->first == allocno){
            next_cp = cp->next_first_allocno_copy;
            another_allocno = cp->second;
         }else if (cp->second == allocno){
            next_cp = cp->next_second_allocno_copy;
            another_allocno = cp->first;
         }else
            gcc_unreachable ();

         another_aclass = another_allocno->aclass;
         if (another_allocno == from
         || another_allocno->assigned_p
         || ALLOCNO_COLOR_DATA (another_allocno)->may_be_spilled_p
         || ! mtcsIra->x_ira_reg_classes_intersect_p[aclass][another_aclass])
            continue;
         if (allocnos_conflict_p(self,another_allocno, start))
            continue;

         class_size = mtcsIra->x_ira_class_hard_regs_num[another_aclass];
         mtcs_ira_build_allocate_and_copy_costs/*!ira_allocate_and_copy_costs*/(mtcsIraBuild,
               &another_allocno->updated_conflict_hard_reg_costs,another_aclass, another_allocno->conflict_hard_reg_costs);
         conflict_costs = another_allocno->updated_conflict_hard_reg_costs;
         if (conflict_costs == NULL)
            cont_p = true;
         else{
            mult = cp->freq;
            freq = another_allocno->freq;
            if (freq == 0)
               freq = 1;
            div = freq * divisor;
            cont_p = false;
            for (i = class_size - 1; i >= 0; i--){
               hard_regno = mtcsIra->x_ira_class_hard_regs[another_aclass][i];
               ira_assert (hard_regno >= 0);
               index = mtcsIraInt->x_ira_class_hard_reg_index[aclass][hard_regno];
               if (index < 0)
                  continue;
               cost = (int) (((int64_t) conflict_costs [i] * mult) / div);
               if (cost == 0)
                  continue;
               cont_p = true;
               if (decr_p)
                  cost = -cost;
               costs[index] += cost;
            }
         }
         /* Probably 5 hops will be enough.  */
         if (cont_p   && divisor <= (COST_HOP_DIVISOR * COST_HOP_DIVISOR * COST_HOP_DIVISOR * COST_HOP_DIVISOR))
            queue_update_cost(self,another_allocno, start, from, divisor * COST_HOP_DIVISOR);
      }
}

/* Set up conflicting (through CONFLICT_REGS) for each object of
   allocno A and the start allocno profitable regs (through
   START_PROFITABLE_REGS).  Remember that the start profitable regs
   exclude hard regs which cannot hold value of mode of allocno A.
   This covers mostly cases when multi-register value should be
   aligned.  */
static inline void get_conflict_and_start_profitable_regs (MtcsIraColor *self,MtcsIraAllocno * a, bool retry_p,
               HardRegSet *conflict_regs,HardRegSet *start_profitable_regs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int i, nwords;
   MtcsIraObject * obj;

   nwords = a->num_objects;
   for (i = 0; i < nwords; i++){
      obj = a->objects[i];
      conflict_regs[i] = obj->total_conflict_hard_regs;
   }
   if (retry_p)
      *start_profitable_regs = (mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[a->aclass]
      &~ (mtcsIra->x_ira_prohibited_class_mode_regs [a->aclass][a->mode]));
   else
      *start_profitable_regs = ALLOCNO_COLOR_DATA (a)->profitable_hard_regs;
}

/* Return true if HARD_REGNO is ok for assigning to allocno A with
   PROFITABLE_REGS and whose objects have CONFLICT_REGS.  */
static inline bool check_hard_reg_p (MtcsIraColor *self,MtcsIraAllocno * a, int hard_regno,
      HardRegSet *conflict_regs, HardRegSet *profitable_regs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int j, nwords, nregs;
   enum reg_class aclass;
   machine_mode mode;

   aclass = a->aclass;
   mode = a->mode;
   if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(
         &mtcsIra->x_ira_prohibited_class_mode_regs[aclass][mode],hard_regno))
      return false;
   /* Checking only profitable hard regs.  */
   if (! mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(profitable_regs, hard_regno))
      return false;
   nregs = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,hard_regno, mode);
   nwords = a->num_objects;
   for (j = 0; j < nregs; j++){
      int k;
      int set_to_test_start = 0, set_to_test_end = nwords;

      if (nregs == nwords){
         if (REG_WORDS_BIG_ENDIAN)
            set_to_test_start = nwords - j - 1;
         else
            set_to_test_start = j;
         set_to_test_end = set_to_test_start + 1;
      }
      for (k = set_to_test_start; k < set_to_test_end; k++)
         if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&conflict_regs[k], hard_regno + j))
            break;
      if (k != set_to_test_end)
         break;
   }
   return j == nregs;
}

/* Return number of registers needed to be saved and restored at
   function prologue/epilogue if we allocate HARD_REGNO to hold value
   of MODE.  */
static int calculate_saved_nregs (MtcsIraColor *self,int hard_regno, machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   int i;
   int nregs = 0;

   ira_assert (hard_regno >= 0);
   for (i = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,hard_regno, mode) - 1; i >= 0; i--)
      if (!self->allocated_hardreg_p[hard_regno + i]
      && !mtcsRtlData/*!crtl*/->abi->clobbers_full_reg_p (hard_regno + i)
      && !LOCAL_REGNO (hard_regno + i))
         nregs++;
   return nregs;
}

/* Allocnos A1 and A2 are known to conflict.  Check whether, in some loop L
   that is either the current loop or a nested subloop, the conflict is of
   the following form:

   - One allocno (X) is a cap allocno for some non-cap allocno X2.

   - X2 belongs to some loop L2.

   - The other allocno (Y) is a non-cap allocno.

   - Y is an ancestor of some allocno Y2 in L2.  (Note that such a Y2
     must exist, given that X and Y conflict.)

   - Y2 is not referenced in L2 (that is, ALLOCNO_NREFS (Y2) == 0).

   - Y can use a different allocation from Y2.

   In this case, Y's register is live across L2 but is not used within it,
   whereas X's register is used only within L2.  The conflict is therefore
   only "soft", in that it can easily be avoided by spilling Y2 inside L2
   without affecting any insn references.

   If the conflict does have this form, return the Y2 that would need to be
   spilled in order to allow X and Y (and thus A1 and A2) to use the same
   register.  Return null otherwise.  Returning null is conservatively correct;
   any nonnnull return value is an optimization.  */
//原型 ira_soft_conflict ira-int.h ira-color.cc
MtcsIraAllocno *mtcs_ira_color_soft_conflict (MtcsIraColor *self,MtcsIraAllocno * a1, MtcsIraAllocno * a2)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   /* Search for the loop L and its associated allocnos X and Y.  */
   int search_depth = 0;
   while (a1->cap_member && a2->cap_member){
      a1 = a1->cap_member;
      a2 = a2->cap_member;
      if (search_depth++ > self->max_soft_conflict_loop_depth)
         return NULL;
   }
   /* This must be true if A1 and A2 conflict.  */
   ira_assert (a1->loop_tree_node == a2->loop_tree_node);

   /* Make A1 the cap allocno (X in the comment above) and A2 the
   non-cap allocno (Y in the comment above).  */
   if (a2->cap_member)
      std::swap (a1, a2);
   if (!a1->cap_member)
      return NULL;

   /* Search for the real allocno that A1 caps (X2 in the comment above).  */
   do{
      a1 = a1->cap_member;
      if (search_depth++ > self->max_soft_conflict_loop_depth)
         return NULL;
   }while (a1->cap_member);

   /* Find the associated allocno for A2 (Y2 in the comment above).  */
   auto node = a1->loop_tree_node;
   auto local_a2 = (MtcsIraAllocno *)node->regno_allocno_map[a2->regno];

   /* Find the parent of LOCAL_A2/Y2.  LOCAL_A2 must be a descendant of A2
   for the conflict query to make sense, so this parent lookup must succeed.

   If the parent allocno has no references, it is usually cheaper to
   spill at that loop level instead.  Keep searching until we find
   a parent allocno that does have references (but don't look past
   the starting allocno).  */
   MtcsIraAllocno * local_parent_a2;
   for (;;){
      local_parent_a2 = mtcs_ira_allocno_parent_allocno/*!ira_parent_allocno*/(local_a2);
      if (local_parent_a2 == a2 || local_parent_a2->nrefs != 0)
         break;
      local_a2 = local_parent_a2;
   }
   if (CHECKING_P){
      /* Sanity check to make sure that the conflict we've been given
      makes sense.  */
      auto test_a2 = local_parent_a2;
      while (test_a2 != a2){
         test_a2 = mtcs_ira_allocno_parent_allocno/*!ira_parent_allocno*/(test_a2);
         ira_assert (test_a2);
      }
   }
   if (local_a2  && local_a2->nrefs == 0
   && mtcs_ira_subloop_allocnos_can_differ_p/*!ira_subloop_allocnos_can_differ_p*/(mtcsIra,local_parent_a2))
      return local_a2;
   return NULL;
}

/* The caller has decided to allocate HREGNO to A and has proved that
   this is safe.  However, the allocation might require the kind of
   spilling described in the comment above ira_soft_conflict.
   The caller has recorded that:

   - The allocnos in ALLOCNOS_TO_SPILL are the ones that would need
     to be spilled to satisfy soft conflicts for at least one allocation
     (not necessarily HREGNO).

   - The soft conflicts apply only to A allocations that overlap
     SOFT_CONFLICT_REGS.

   If allocating HREGNO is subject to any soft conflicts, record the
   subloop allocnos that need to be spilled.  */
static void spill_soft_conflicts (MtcsIraColor *self,MtcsIraAllocno * a, bitmap allocnos_to_spill,
            HardRegSet *soft_conflict_regs, int hregno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);

   auto nregs = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,hregno, a->mode);
   bitmap_iterator bi;
   unsigned int i;
   EXECUTE_IF_SET_IN_BITMAP (allocnos_to_spill, 0, i, bi){
      /* SPILL_A needs to be spilled for at least one allocation
      (not necessarily this one).  */
      auto spill_a = mtcsIraBuild->ira_allocnos[i];

      /* Find the corresponding allocno for this loop.  */
      auto conflict_a = spill_a;
      do{
         conflict_a = mtcs_ira_allocno_parent_or_cap_allocno/*!ira_parent_or_cap_allocno*/(conflict_a);
         ira_assert (conflict_a);
      }while (conflict_a->loop_tree_node->level > a->loop_tree_node->level);

      ira_assert (conflict_a->loop_tree_node  == a->loop_tree_node);

      if (conflict_a == a){
         /* SPILL_A is a descendant of A.  We don't know (and don't need
         to know) which cap allocnos have a soft conflict with A.
         All we need to do is test whether the soft conflict applies
         to the chosen allocation.  */
         if (mtcs_ira_hard_reg_set_intersection_p/*!ira_hard_reg_set_intersection_p*/(mtcsIra,hregno, a->mode, soft_conflict_regs))
            spill_a->might_conflict_with_parent_p = true;
      }else{
         /* SPILL_A is a descendant of CONFLICT_A, which has a soft conflict
         with A.  Test whether the soft conflict applies to the current
         allocation.  */
         ira_assert (mtcs_ira_color_soft_conflict/*!ira_soft_conflict*/(self,a, conflict_a) == spill_a);
         auto conflict_hregno = conflict_a->hard_regno;
         ira_assert (conflict_hregno >= 0);
         auto conflict_nregs = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,conflict_hregno,
         conflict_a->mode);
         if (hregno + nregs > conflict_hregno  && conflict_hregno + conflict_nregs > hregno)
            spill_a->might_conflict_with_parent_p = true;
      }
   }
}

/* Choose a hard register for allocno A.  If RETRY_P is TRUE, it means
   that the function called from function
   `ira_reassign_conflict_allocnos' and `allocno_reload_assign'.  In
   this case some allocno data are not defined or updated and we
   should not touch these data.  The function returns true if we
   managed to assign a hard register to the allocno.

   To assign a hard register, first of all we calculate all conflict
   hard registers which can come from conflicting allocnos with
   already assigned hard registers.  After that we find first free
   hard register with the minimal cost.  During hard register cost
   calculation we take conflict hard register costs into account to
   give a chance for conflicting allocnos to get a better hard
   register in the future.

   If the best hard register cost is bigger than cost of memory usage
   for the allocno, we don't assign a hard register to given allocno
   at all.

   If we assign a hard register to the allocno, we update costs of the
   hard register for allocnos connected by copies to improve a chance
   to coalesce insns represented by the copies when we assign hard
   registers to the allocnos connected by the copies.  */
static bool assign_hard_reg (MtcsIraColor *self,MtcsIraAllocno * a, bool retry_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   HardRegSet conflicting_regs[2];
   conflicting_regs[0].count=mtcs_reg_get_hard_reg_element_count(mtcsReg);
   conflicting_regs[1].count=mtcs_reg_get_hard_reg_element_count(mtcsReg);
   HardRegSet /*!HARD_REG_SET*/ profitable_hard_regs = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};

   int i, j, hard_regno, best_hard_regno, class_size;
   int cost, mem_cost, min_cost, full_cost, min_full_cost, nwords, word;
   int *a_costs;
   enum reg_class aclass;
   machine_mode mode;
   int costs[firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/], full_costs[firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/];
   int saved_nregs;
   enum reg_class rclass;
   int add_cost;
   //#ifdef STACK_REGS
   bool no_stack_reg_p;
   //#endif
   auto_bitmap allocnos_to_spill;
   HardRegSet /*!HARD_REG_SET*/ soft_conflict_regs = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};

   ira_assert (! a->assigned_p);
   get_conflict_and_start_profitable_regs(self,a, retry_p,
   conflicting_regs,
   &profitable_hard_regs);
   aclass = ALLOCNO_CLASS (a);
   class_size = mtcsIra->x_ira_class_hard_regs_num[aclass];
   best_hard_regno = -1;
   mem_cost = 0;
   memset (costs, 0, sizeof (int) * class_size);
   memset (full_costs, 0, sizeof (int) * class_size);
   if(mtcs_config_ifdef(mtcsConfig,MTCS_STACK_REGS)){
      no_stack_reg_p = false;
      /*!
      #ifdef STACK_REGS
      no_stack_reg_p = false;
      #endif
      */
   }
   if (! retry_p)
      start_update_cost(self);
   mem_cost += a->updated_memory_cost;

   mtcs_ira_build_allocate_and_copy_costs/*!ira_allocate_and_copy_costs*/(mtcsIraBuild,&a->updated_hard_reg_costs,
         aclass, a->hard_reg_costs);
   a_costs = a->updated_hard_reg_costs;
   if(mtcs_config_ifdef(mtcsConfig,MTCS_STACK_REGS)){
      //#ifdef STACK_REGS
      no_stack_reg_p = no_stack_reg_p || a->total_no_stack_reg_p;
      //#endif
   }
   cost = a->updated_class_cost;
   for (i = 0; i < class_size; i++)
      if (a_costs != NULL){
         costs[i] += a_costs[i];
         full_costs[i] += a_costs[i];
      }else{
         costs[i] += cost;
         full_costs[i] += cost;
      }
   nwords = a->num_objects;
   self->curr_allocno_process++;

   for (word = 0; word < nwords; word++){
      MtcsIraObject * conflict_obj;
      MtcsIraObject * obj = a->objects[word];
      MtcsIraObjectConflictIterator oci;

      /* Take preferences of conflicting allocnos into account.  */
      MTCS_FOR_EACH_OBJECT_CONFLICT(mtcsIraBuild, obj, conflict_obj, oci){
         MtcsIraAllocno * conflict_a = conflict_obj->allocno;
         enum reg_class conflict_aclass;
         AllocnoColorData * data = ALLOCNO_COLOR_DATA (conflict_a);

         /* Reload can give another class so we need to check all
         allocnos.  */
         if (!retry_p
         && ((!conflict_a->assigned_p
         || conflict_a->hard_regno < 0)
         && !(mtcs_reg_hard_reg_set_intersect_p/*!hard_reg_set_intersect_p*/(&profitable_hard_regs,
         &ALLOCNO_COLOR_DATA(conflict_a)->profitable_hard_regs)))){
            /* All conflict allocnos are in consideration bitmap
            when retry_p is false.  It might change in future and
            if it happens the assert will be broken.  It means
            the code should be modified for the new
            assumptions.  */
            ira_assert (bitmap_bit_p (self->consideration_allocno_bitmap, conflict_a->num));
            continue;
         }
         conflict_aclass = conflict_a->aclass;
         ira_assert (mtcsIra->x_ira_reg_classes_intersect_p[aclass][conflict_aclass]);
         if (conflict_a->assigned_p){
            hard_regno = conflict_a->hard_regno;
            if (hard_regno >= 0   && (mtcs_ira_hard_reg_set_intersection_p/*!ira_hard_reg_set_intersection_p*/(mtcsIra,
                  hard_regno, conflict_a->mode, &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[aclass]))){
               int n_objects = conflict_a->num_objects;
               int conflict_nregs;

               mode = conflict_a->mode;
               conflict_nregs = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,hard_regno, mode);
               auto spill_a = (retry_p ? nullptr : mtcs_ira_color_soft_conflict/*!ira_soft_conflict*/(self,a, conflict_a));
               if (spill_a){
                  if (bitmap_set_bit (allocnos_to_spill, spill_a->num)){
                     mtcs_ira_loop_border_costs border_costs (mtcsIra,spill_a);
                     auto cost = border_costs.spill_inside_loop_cost ();
                     auto note_conflict = [&](int r)
                     {
                        mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&soft_conflict_regs, r);
                        auto hri = mtcsIraInt->x_ira_class_hard_reg_index[aclass][r];
                        if (hri >= 0){
                           costs[hri] += cost;
                           full_costs[hri] += cost;
                        }
                     };
                     for (int r = hard_regno; r >= 0
                        && (int) mtcs_reg_end_hard_regno/*!end_hard_regno*/(mtcsReg,mode, r) > hard_regno; r--)
                        note_conflict (r);
                     for (int r = hard_regno + 1; r < hard_regno + conflict_nregs; r++)
                        note_conflict (r);
                  }
               }else{
                  if (conflict_nregs == n_objects && conflict_nregs > 1){
                     int num = conflict_obj->subword;

                     if (REG_WORDS_BIG_ENDIAN)
                        mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&conflicting_regs[word],hard_regno + n_objects - num - 1);
                     else
                        mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&conflicting_regs[word],hard_regno + num);
                  }else
                     conflicting_regs[word] |= mtcsIraInt->x_ira_reg_mode_hard_regset[hard_regno][mode];
                  if (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&profitable_hard_regs, &conflicting_regs[word]))
                     goto fail;
               }
            }
         }else if (! retry_p
         && ! ALLOCNO_COLOR_DATA (conflict_a)->may_be_spilled_p
         /* Don't process the conflict allocno twice.  */
         && (ALLOCNO_COLOR_DATA (conflict_a)->last_process != self->curr_allocno_process)){
            int k, *conflict_costs;

            ALLOCNO_COLOR_DATA (conflict_a)->last_process  = self->curr_allocno_process;
            mtcs_ira_build_allocate_and_copy_costs/*!ira_allocate_and_copy_costs*/(mtcsIraBuild,
            &conflict_a->updated_conflict_hard_reg_costs, conflict_aclass,conflict_a->conflict_hard_reg_costs);
            conflict_costs = conflict_a->updated_conflict_hard_reg_costs;
            if (conflict_costs != NULL)
               for (j = class_size - 1; j >= 0; j--){
                  hard_regno = mtcsIra->x_ira_class_hard_regs[aclass][j];
                  ira_assert (hard_regno >= 0);
                  k = mtcsIraInt->x_ira_class_hard_reg_index[conflict_aclass][hard_regno];
                  if (k < 0
                  /* If HARD_REGNO is not available for CONFLICT_A,
                  the conflict would be ignored, since HARD_REGNO
                  will never be assigned to CONFLICT_A.  */
                  || !mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&data->profitable_hard_regs,hard_regno))
                     continue;
                  full_costs[j] -= conflict_costs[k];
               }
            queue_update_cost(self,conflict_a, conflict_a, NULL, COST_HOP_DIVISOR);
         }
      }
   }

   if (! retry_p)
      /* Take into account preferences of allocnos connected by copies to
      the conflict allocnos.  */
      update_conflict_hard_regno_costs(self,full_costs, aclass, true);

   /* Take preferences of allocnos connected by copies into
   account.  */
   if (! retry_p){
      start_update_cost(self);
      queue_update_cost(self,a, a, NULL, COST_HOP_DIVISOR);
      update_conflict_hard_regno_costs(self,full_costs, aclass, false);
   }
   min_cost = min_full_cost = INT_MAX;
   /* We don't care about giving callee saved registers to allocnos no
   living through calls because call clobbered registers are
   allocated first (it is usual practice to put them first in
   REG_ALLOC_ORDER).  */
   mode = a->mode;
   for (i = 0; i < class_size; i++){
      hard_regno = mtcsIra->x_ira_class_hard_regs[aclass][i];
      if(mtcs_config_ifdef(mtcsConfig,MTCS_STACK_REGS)){
         if (no_stack_reg_p && mtcs_reg_get_first_stack_reg/*!FIRST_STACK_REG*/(mtcsReg)
         <= hard_regno && hard_regno <= mtcs_reg_get_last_stack_reg/*!LAST_STACK_REG*/(mtcsReg))
             continue;
         /*!
              #ifdef STACK_REGS
              if (no_stack_reg_p
              && FIRST_STACK_REG <= hard_regno && hard_regno <= LAST_STACK_REG)
              continue;
              #endif
              */
      }

      if (! check_hard_reg_p(self,a, hard_regno, conflicting_regs, &profitable_hard_regs))
         continue;
      if (NUM_REGISTER_FILTERS  && !test_register_filters (a->register_filters, hard_regno))
         continue;
      cost = costs[i];
      full_cost = full_costs[i];
      if (!HONOR_REG_ALLOC_ORDER){
         if ((saved_nregs = calculate_saved_nregs(self,hard_regno, mode)) != 0){
         /* We need to save/restore the hard register in
         epilogue/prologue.  Therefore we increase the cost.  */
            rclass = mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,hard_regno);
            add_cost = ((mtcsIra->x_ira_memory_move_cost[mode][rclass][0]+ mtcsIra->x_ira_memory_move_cost[mode][rclass][1])
            * saved_nregs / mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,hard_regno, mode) - 1);
            cost += add_cost;
            full_cost += add_cost;
         }
      }
      if (min_cost > cost)
         min_cost = cost;
      if (min_full_cost > full_cost){
         min_full_cost = full_cost;
         best_hard_regno = hard_regno;
         ira_assert (hard_regno >= 0);
      }
      if (mtcsIraGlobal->internal_flag_ira_verbose > 5 && mtcsIraGlobal->ira_dump_file != NULL)
         fprintf (mtcsIraGlobal->ira_dump_file, "(%d=%d,%d) ", hard_regno, cost, full_cost);
   }
   if (mtcsIraGlobal->internal_flag_ira_verbose > 5 && mtcsIraGlobal->ira_dump_file != NULL)
      fprintf (mtcsIraGlobal->ira_dump_file, "\n");
   if (min_full_cost > mem_cost
   /* Do not spill static chain pointer pseudo when non-local goto
   is used.  */
   && !  mtcs_ira_non_spilled_static_chain_regno_p/*!non_spilled_static_chain_regno_p*/(mtcsIra,a->regno)){
      if (! retry_p && mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
         fprintf (mtcsIraGlobal->ira_dump_file, "(memory is more profitable %d vs %d) ",mem_cost, min_full_cost);
      best_hard_regno = -1;
   }
fail:
   if (best_hard_regno >= 0) {
      for (i = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,best_hard_regno, mode) - 1; i >= 0; i--)
         self->allocated_hardreg_p[best_hard_regno + i] = true;
      spill_soft_conflicts(self,a, allocnos_to_spill, &soft_conflict_regs, best_hard_regno);
   }
   if (! retry_p)
      restore_costs_from_copies(self,a);
   a->hard_regno = best_hard_regno;
   a->assigned_p = true;
   if (best_hard_regno >= 0 && !retry_p)
      update_costs_from_copies(self,a, true, true);
   ira_assert (a->aclass == aclass);
   /* We don't need updated costs anymore.  */
   mtcs_ira_build_free_allocno_updated_costs/*!ira_free_allocno_updated_costs*/(mtcsIraBuild,a);
   return best_hard_regno >= 0;
}


/* If allocno A is a cap, return non-cap allocno from which A is
created.  Otherwise, return A.  */
static MtcsIraAllocno *get_cap_member (MtcsIraColor *self,MtcsIraAllocno * a)
{
   MtcsIraAllocno * member;
   while ((member = a->cap_member) != NULL)
      a = member;
   return a;
}

/* Return TRUE if live ranges of allocnos A1 and A2 intersect.  It is
used to find a conflict for new allocnos or allocnos with the
different allocno classes.  */
static bool allocnos_conflict_by_live_ranges_p (MtcsIraColor *self,MtcsIraAllocno * a1, MtcsIraAllocno * a2)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   rtx reg1, reg2;
   int i, j;
   int n1 = a1->num_objects;
   int n2 = a2->num_objects;

   if (a1 == a2)
      return false;
   reg1 = mtcsRtlData->regno_reg_rtx/*! regno_reg_rtx*/[a1->regno];
   reg2 = mtcsRtlData->regno_reg_rtx/*! regno_reg_rtx*/[a2->regno];
   if (reg1 != NULL && reg2 != NULL && ORIGINAL_REGNO (reg1) == ORIGINAL_REGNO (reg2))
      return false;

   /* We don't keep live ranges for caps because they can be quite big.
   Use ranges of non-cap allocno from which caps are created.  */
   a1 = get_cap_member(self,a1);
   a2 = get_cap_member(self,a2);
   for (i = 0; i < n1; i++){
      MtcsIraObject * c1 = a1->objects[i];

      for (j = 0; j < n2; j++){
         MtcsIraObject * c2 = a2->objects[j];
         if (mtcsira_object_live_ranges_intersect_p/*!ira_live_ranges_intersect_p*/(c1->live_ranges,c2->live_ranges))
            return true;
      }
   }
   return false;
}

/* The function is used to sort copies according to their execution
   frequencies.  */
static int copy_freq_compare_func (const void *v1p, const void *v2p)
{
   MtcsIraAllocnoCopy *cp1 = *(const MtcsIraAllocnoCopy **) v1p;
   MtcsIraAllocnoCopy *cp2 = *(const MtcsIraAllocnoCopy **) v2p;
   int pri1, pri2;

   pri1 = cp1->freq;
   pri2 = cp2->freq;
   if (pri2 - pri1)
      return pri2 - pri1;

   /* If frequencies are equal, sort by copies, so that the results of
   qsort leave nothing to chance.  */
   return cp1->num - cp2->num;
}



/* Return true if any allocno from thread of A1 conflicts with any
   allocno from thread A2.  */
static bool allocno_thread_conflict_p (MtcsIraColor *self,MtcsIraAllocno * a1, MtcsIraAllocno * a2)
{
   MtcsIraAllocno * a, *conflict_a;
   for (a = ALLOCNO_COLOR_DATA (a2)->next_thread_allocno;; a = ALLOCNO_COLOR_DATA (a)->next_thread_allocno){
      for (conflict_a = ALLOCNO_COLOR_DATA (a1)->next_thread_allocno;;
            conflict_a = ALLOCNO_COLOR_DATA (conflict_a)->next_thread_allocno){
         if (allocnos_conflict_by_live_ranges_p(self,a, conflict_a))
            return true;
         if (conflict_a == a1)
            break;
      }
      if (a == a2)
         break;
   }
   return false;
}

/* Merge two threads given correspondingly by their first allocnos T1
   and T2 (more accurately merging T2 into T1).  */
static void merge_threads (MtcsIraColor *self,MtcsIraAllocno * t1, MtcsIraAllocno * t2)
{
   MtcsIraAllocno * a, *next, *last;

   gcc_assert (t1 != t2  && ALLOCNO_COLOR_DATA (t1)->first_thread_allocno == t1
            && ALLOCNO_COLOR_DATA (t2)->first_thread_allocno == t2);
   for (last = t2, a = ALLOCNO_COLOR_DATA (t2)->next_thread_allocno;;
               a = ALLOCNO_COLOR_DATA (a)->next_thread_allocno){
      ALLOCNO_COLOR_DATA (a)->first_thread_allocno = t1;
      if (a == t2)
         break;
      last = a;
   }
   next = ALLOCNO_COLOR_DATA (t1)->next_thread_allocno;
   ALLOCNO_COLOR_DATA (t1)->next_thread_allocno = t2;
   ALLOCNO_COLOR_DATA (last)->next_thread_allocno = next;
   ALLOCNO_COLOR_DATA (t1)->thread_freq += ALLOCNO_COLOR_DATA (t2)->thread_freq;
}

/* Create threads by processing CP_NUM copies from sorted copies.  We
   process the most expensive copies first.  */
static void form_threads_from_copies (MtcsIraColor *self,int cp_num)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   MtcsIraAllocno * a, *thread1, *thread2;
   MtcsIraAllocnoCopy * cp;

   qsort (self->sorted_copies, cp_num, sizeof (MtcsIraAllocnoCopy *), copy_freq_compare_func);
   /* Form threads processing copies, most frequently executed
   first.  */
   for (int i = 0; i < cp_num; i++){
      cp = self->sorted_copies[i];
      thread1 = ALLOCNO_COLOR_DATA (cp->first)->first_thread_allocno;
      thread2 = ALLOCNO_COLOR_DATA (cp->second)->first_thread_allocno;
      if (thread1 == thread2)
         continue;
      if (! allocno_thread_conflict_p(self,thread1, thread2)){
         if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
            fprintf(mtcsIraGlobal->ira_dump_file, "        Forming thread by copy %d:a%dr%d-a%dr%d (freq=%d):\n",
                        cp->num, cp->first->num, cp->first->regno,cp->second->num, cp->second->regno,cp->freq);
         merge_threads(self,thread1, thread2);
         if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL){
            thread1 = ALLOCNO_COLOR_DATA (thread1)->first_thread_allocno;
            fprintf (mtcsIraGlobal->ira_dump_file, "          Result (freq=%d): a%dr%d(%d)",
                     ALLOCNO_COLOR_DATA (thread1)->thread_freq,thread1->num, thread1->regno,thread1->freq);
            for (a = ALLOCNO_COLOR_DATA (thread1)->next_thread_allocno; a != thread1; a = ALLOCNO_COLOR_DATA (a)->next_thread_allocno)
               fprintf (mtcsIraGlobal->ira_dump_file, " a%dr%d(%d)",a->num, a->regno,a->freq);
            fprintf (mtcsIraGlobal->ira_dump_file, "\n");
         }
      }
   }
}

/* Create threads by processing copies of all alocnos from BUCKET.  We
   process the most expensive copies first.  */
static void form_threads_from_bucket (MtcsIraColor *self,MtcsIraAllocno * bucket)
{
   MtcsIraAllocno * a;
   MtcsIraAllocnoCopy * cp, *next_cp;
   int cp_num = 0;

   for (a = bucket; a != NULL; a = ALLOCNO_COLOR_DATA (a)->next_bucket_allocno){
      for (cp = ALLOCNO_COPIES (a); cp != NULL; cp = next_cp){
         if (cp->first == a){
            next_cp = cp->next_first_allocno_copy;
            self->sorted_copies[cp_num++] = cp;
         }else if (cp->second == a)
            next_cp = cp->next_second_allocno_copy;
         else
            gcc_unreachable ();
      }
   }
   form_threads_from_copies(self,cp_num);
}

/* Create threads by processing copies of colorable allocno A.  We
   process most expensive copies first.  */
static void form_threads_from_colorable_allocno (MtcsIraColor *self,MtcsIraAllocno * a)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   MtcsIraAllocno * another_a;
   MtcsIraAllocnoCopy * cp, *next_cp;
   int cp_num = 0;

   if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
      fprintf (mtcsIraGlobal->ira_dump_file, "      Forming thread from allocno a%dr%d:\n", a->num, a->regno);
   for (cp = ALLOCNO_COPIES (a); cp != NULL; cp = next_cp){
      if (cp->first == a){
         next_cp = cp->next_first_allocno_copy;
         another_a = cp->second;
      }else if (cp->second == a){
         next_cp = cp->next_second_allocno_copy;
         another_a = cp->first;
      }else
         gcc_unreachable ();
      if ((! ALLOCNO_COLOR_DATA (another_a)->in_graph_p
      && !ALLOCNO_COLOR_DATA (another_a)->may_be_spilled_p)
      || ALLOCNO_COLOR_DATA (another_a)->colorable_p)
         self->sorted_copies[cp_num++] = cp;
   }
   form_threads_from_copies(self,cp_num);
}

/* Form initial threads which contain only one allocno.  */
static void init_allocno_threads (MtcsIraColor *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);

   MtcsIraAllocno * a;
   unsigned int j;
   bitmap_iterator bi;
   MtcsIraAllocnoPref * pref;

   EXECUTE_IF_SET_IN_BITMAP (self->consideration_allocno_bitmap, 0, j, bi){
      a = mtcsIraBuild->ira_allocnos[j];
      /* Set up initial thread data: */
      ALLOCNO_COLOR_DATA (a)->first_thread_allocno = ALLOCNO_COLOR_DATA (a)->next_thread_allocno = a;
      ALLOCNO_COLOR_DATA (a)->thread_freq = a->freq;
      ALLOCNO_COLOR_DATA (a)->hard_reg_prefs = 0;
      for (pref = a->allocno_prefs; pref != NULL; pref = pref->next_pref)
         ALLOCNO_COLOR_DATA (a)->hard_reg_prefs += pref->freq;
   }
}

/* Return the current spill priority of allocno A.  The less the
   number, the more preferable the allocno for spilling.  */
static inline int allocno_spill_priority (MtcsIraColor *self,MtcsIraAllocno * a)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   AllocnoColorData * data = ALLOCNO_COLOR_DATA (a);
   return (data->temp  / (a->excess_pressure_points_num   * mtcsIra->x_ira_reg_class_max_nregs[a->aclass][a->mode]  + 1));
}

/* Add allocno A to bucket *BUCKET_PTR.  A should be not in a bucket
   before the call.  */
static void add_allocno_to_bucket (MtcsIraColor *self,MtcsIraAllocno * a, MtcsIraAllocno * *bucket_ptr)
{
   MtcsIraAllocno * first_a;
   AllocnoColorData * data;

   if (bucket_ptr == &self->uncolorable_allocno_bucket   && a->aclass != NO_REGS){
      self->uncolorable_allocnos_num++;
      ira_assert (self->uncolorable_allocnos_num > 0);
   }
   first_a = *bucket_ptr;
   data = ALLOCNO_COLOR_DATA (a);
   data->next_bucket_allocno = first_a;
   data->prev_bucket_allocno = NULL;
   if (first_a != NULL)
      ALLOCNO_COLOR_DATA (first_a)->prev_bucket_allocno = a;
   *bucket_ptr = a;
}

/* Compare two allocnos to define which allocno should be pushed first
   into the coloring stack.  If the return is a negative number, the
   allocno given by the first parameter will be pushed first.  In this
   case such allocno has less priority than the second one and the
   hard register will be assigned to it after assignment to the second
   one.  As the result of such assignment order, the second allocno
   has a better chance to get the best hard register.  */
static int bucket_allocno_compare_func (const void *v1p, const void *v2p)
{
  MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
  MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
  MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

  MtcsIraAllocno * a1 = *(const MtcsIraAllocno * *) v1p;
  MtcsIraAllocno * a2 = *(const MtcsIraAllocno * *) v2p;
  int diff, freq1, freq2, a1_num, a2_num, pref1, pref2;
  MtcsIraAllocno * t1 = ALLOCNO_COLOR_DATA (a1)->first_thread_allocno;
  MtcsIraAllocno * t2 = ALLOCNO_COLOR_DATA (a2)->first_thread_allocno;
  int cl1 = a1->aclass, cl2 = a2->aclass;

  freq1 = ALLOCNO_COLOR_DATA (t1)->thread_freq;
  freq2 = ALLOCNO_COLOR_DATA (t2)->thread_freq;
  if ((diff = freq1 - freq2) != 0)
    return diff;

  if ((diff = t2->num - t1->num) != 0)
    return diff;

  /* Push pseudos requiring less hard registers first.  It means that
     we will assign pseudos requiring more hard registers first
     avoiding creation small holes in free hard register file into
     which the pseudos requiring more hard registers cannot fit.  */
  if ((diff = (mtcsIra->x_ira_reg_class_max_nregs[cl1][a1->mode]
          - mtcsIra->x_ira_reg_class_max_nregs[cl2][a2->mode])) != 0)
    return diff;

  freq1 = a1->freq;
  freq2 = a2->freq;
  if ((diff = freq1 - freq2) != 0)
    return diff;

  a1_num = ALLOCNO_COLOR_DATA (a1)->available_regs_num;
  a2_num = ALLOCNO_COLOR_DATA (a2)->available_regs_num;
  if ((diff = a2_num - a1_num) != 0)
    return diff;
  /* Push allocnos with minimal conflict_allocno_hard_prefs first.  */
  pref1 = ALLOCNO_COLOR_DATA (a1)->conflict_allocno_hard_prefs;
  pref2 = ALLOCNO_COLOR_DATA (a2)->conflict_allocno_hard_prefs;
  if ((diff = pref1 - pref2) != 0)
    return diff;
  return a2->num - a1->num;
}

/* Sort bucket *BUCKET_PTR and return the result through
   BUCKET_PTR.  */
static void sort_bucket (MtcsIraColor *self,MtcsIraAllocno * *bucket_ptr,
        int (*compare_func) (const void *, const void *))
{
   MtcsIraAllocno * a, *head;
   int n;

   for (n = 0, a = *bucket_ptr;  a != NULL;  a = ALLOCNO_COLOR_DATA (a)->next_bucket_allocno)
      self->sorted_allocnos[n++] = a;
   if (n <= 1)
      return;
   qsort (self->sorted_allocnos, n, sizeof (MtcsIraAllocno *), compare_func);
   head = NULL;
   for (n--; n >= 0; n--) {
      a = self->sorted_allocnos[n];
      ALLOCNO_COLOR_DATA (a)->next_bucket_allocno = head;
      ALLOCNO_COLOR_DATA (a)->prev_bucket_allocno = NULL;
      if (head != NULL)
         ALLOCNO_COLOR_DATA (head)->prev_bucket_allocno = a;
      head = a;
   }
   *bucket_ptr = head;
}

/* Add ALLOCNO to colorable bucket maintaining the order according
   their priority.  ALLOCNO should be not in a bucket before the
   call.  */
static void add_allocno_to_ordered_colorable_bucket (MtcsIraColor *self,MtcsIraAllocno * allocno)
{
   MtcsIraAllocno * before, *after;

   form_threads_from_colorable_allocno(self,allocno);
   for (before = self->colorable_allocno_bucket, after = NULL;
           before != NULL; after = before, before = ALLOCNO_COLOR_DATA (before)->next_bucket_allocno)
      if (bucket_allocno_compare_func (&allocno, &before) < 0)
         break;
   ALLOCNO_COLOR_DATA (allocno)->next_bucket_allocno = before;
   ALLOCNO_COLOR_DATA (allocno)->prev_bucket_allocno = after;
   if (after == NULL)
      self->colorable_allocno_bucket = allocno;
   else
      ALLOCNO_COLOR_DATA (after)->next_bucket_allocno = allocno;
   if (before != NULL)
      ALLOCNO_COLOR_DATA (before)->prev_bucket_allocno = allocno;
}

/* Delete ALLOCNO from bucket *BUCKET_PTR.  It should be there before
   the call.  */
static void delete_allocno_from_bucket (MtcsIraColor *self,MtcsIraAllocno * allocno, MtcsIraAllocno * *bucket_ptr)
{
   MtcsIraAllocno * prev_allocno, *next_allocno;

   if (bucket_ptr == &self->uncolorable_allocno_bucket && allocno->aclass != NO_REGS){
      self->uncolorable_allocnos_num--;
      ira_assert (self->uncolorable_allocnos_num >= 0);
   }
   prev_allocno = ALLOCNO_COLOR_DATA (allocno)->prev_bucket_allocno;
   next_allocno = ALLOCNO_COLOR_DATA (allocno)->next_bucket_allocno;
   if (prev_allocno != NULL)
      ALLOCNO_COLOR_DATA (prev_allocno)->next_bucket_allocno = next_allocno;
   else{
      ira_assert (*bucket_ptr == allocno);
      *bucket_ptr = next_allocno;
   }
   if (next_allocno != NULL)
      ALLOCNO_COLOR_DATA (next_allocno)->prev_bucket_allocno = prev_allocno;
}

/* Put allocno A onto the coloring stack without removing it from its
   bucket.  Pushing allocno to the coloring stack can result in moving
   conflicting allocnos from the uncolorable bucket to the colorable
   one.  Update conflict_allocno_hard_prefs of the conflicting
   allocnos which are not on stack yet.  */
static void push_allocno_to_stack (MtcsIraColor *self,MtcsIraAllocno * a)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   enum reg_class aclass;
   AllocnoColorData * data, *conflict_data;
   int size, i, n = a->num_objects;

   data = ALLOCNO_COLOR_DATA (a);
   data->in_graph_p = false;
   self->allocno_stack_vec.safe_push (a);
   aclass = a->aclass;
   if (aclass == NO_REGS)
      return;
   size = mtcsIra->x_ira_reg_class_max_nregs[aclass][a->mode];
   if (n > 1){
      /* We will deal with the subwords individually.  */
      gcc_assert (size == a->num_objects);
      size = 1;
   }

   for (i = 0; i < n; i++){
      MtcsIraObject * obj = a->objects[i];
      MtcsIraObject * conflict_obj;
      MtcsIraObjectConflictIterator oci;

      MTCS_FOR_EACH_OBJECT_CONFLICT(mtcsIraBuild, obj, conflict_obj, oci){
         MtcsIraAllocno * conflict_a = OBJECT_ALLOCNO (conflict_obj);
         MtcsIraAllocnoPref * pref;

         conflict_data = ALLOCNO_COLOR_DATA (conflict_a);
         if (! conflict_data->in_graph_p
         || conflict_a->assigned_p
         || !(mtcs_reg_hard_reg_set_intersect_p/*!hard_reg_set_intersect_p*/(&ALLOCNO_COLOR_DATA (a)->profitable_hard_regs,
         &conflict_data->profitable_hard_regs)))
            continue;
         for (pref = a->allocno_prefs; pref != NULL; pref = pref->next_pref)
            conflict_data->conflict_allocno_hard_prefs -= pref->freq;
         if (conflict_data->colorable_p)
            continue;
         ira_assert (bitmap_bit_p (self->coloring_allocno_bitmap,conflict_a->num));
         if (update_left_conflict_sizes_p(self,conflict_a, a, size)){
            delete_allocno_from_bucket(self,conflict_a, &self->uncolorable_allocno_bucket);
            add_allocno_to_ordered_colorable_bucket(self,conflict_a);
            if (mtcsIraGlobal->internal_flag_ira_verbose > 4 && mtcsIraGlobal->ira_dump_file != NULL){
               fprintf (mtcsIraGlobal->ira_dump_file, "        Making");
               mtcs_ira_allocno_print_expanded_allocno/*!ira_print_expanded_allocno*/(conflict_a);
               fprintf (mtcsIraGlobal->ira_dump_file, " colorable\n");
            }
         }
      }
   }
}

/* Put ALLOCNO onto the coloring stack and remove it from its bucket.
   The allocno is in the colorable bucket if COLORABLE_P is TRUE.  */
static void remove_allocno_from_bucket_and_push (MtcsIraColor *self,MtcsIraAllocno * allocno, bool colorable_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   if (colorable_p)
      delete_allocno_from_bucket(self,allocno, &self->colorable_allocno_bucket);
   else
      delete_allocno_from_bucket(self,allocno, &self->uncolorable_allocno_bucket);
   if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL){
      fprintf (mtcsIraGlobal->ira_dump_file, "      Pushing");
      mtcs_ira_allocno_print_expanded_allocno/*!ira_print_expanded_allocno*/(allocno);
      if (colorable_p)
         fprintf (mtcsIraGlobal->ira_dump_file, "(cost %d)\n",ALLOCNO_COLOR_DATA (allocno)->temp);
      else
         fprintf (mtcsIraGlobal->ira_dump_file, "(potential spill: %spri=%d, cost=%d)\n",
               allocno->bad_spill_p ? "bad spill, " : "",allocno_spill_priority(self,allocno),ALLOCNO_COLOR_DATA (allocno)->temp);
   }
   if (! colorable_p)
      ALLOCNO_COLOR_DATA (allocno)->may_be_spilled_p = true;
   push_allocno_to_stack(self,allocno);
}

/* Put all allocnos from colorable bucket onto the coloring stack.  */
static void push_only_colorable (MtcsIraColor *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
      fprintf (mtcsIraGlobal->ira_dump_file, "      Forming thread from colorable bucket:\n");

   form_threads_from_bucket(self,self->colorable_allocno_bucket);
   for (MtcsIraAllocno * a = self->colorable_allocno_bucket;
         a != NULL; a = ALLOCNO_COLOR_DATA (a)->next_bucket_allocno)
      update_costs_from_prefs(self,a);
   sort_bucket(self,&self->colorable_allocno_bucket, bucket_allocno_compare_func);
   for (;self->colorable_allocno_bucket != NULL;)
      remove_allocno_from_bucket_and_push(self,self->colorable_allocno_bucket, true);
}

/* Return the frequency of exit edges (if EXIT_P) or entry from/to the
   loop given by its LOOP_NODE.  */
//原型 ira_loop_edge_freq ira-int.h ira-color.cc
int mtcs_ira_color_loop_edge_freq (MtcsIraColor *self,MtcsIraLoopTreeNode *loop_node, int regno, bool exit_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   int freq, i;
   edge_iterator ei;
   edge e;

   ira_assert (current_loops != NULL && loop_node->loop != NULL
   && (regno < 0 || regno >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/));
   freq = 0;
   if (! exit_p){
      FOR_EACH_EDGE (e, ei, loop_node->loop->header->preds)
         if (e->src != loop_node->loop->latch   && (regno < 0 || (bitmap_bit_p (df_get_live_out (e->src), regno)
         && bitmap_bit_p (df_get_live_in (e->dest), regno))))
            freq += EDGE_FREQUENCY (e);
   }else{
      auto_vec<edge> edges = get_loop_exit_edges (loop_node->loop);
      FOR_EACH_VEC_ELT (edges, i, e)
      if (regno < 0 || (bitmap_bit_p (df_get_live_out (e->src), regno) && bitmap_bit_p (df_get_live_in (e->dest), regno)))
         freq += EDGE_FREQUENCY (e);
   }

   return REG_FREQ_FROM_EDGE_FREQ (freq);
}

/* Construct an object that describes the boundary between A and its
   parent allocno.  */
mtcs_ira_loop_border_costs::mtcs_ira_loop_border_costs (MtcsIra *mtcsIra,MtcsIraAllocno *a)
 {
   m_mode=a->mode;
   m_class=a->aclass;
   this->mtcsIra=mtcsIra;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsIra);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraColor *mtcsIraColor=mtcs_ira_mgr_get_color(mtcsIraMgr);
   m_entry_freq = mtcs_ira_color_loop_edge_freq/*!ira_loop_edge_freq*/(mtcsIraColor,a->loop_tree_node, a->regno, false);
   m_exit_freq = mtcs_ira_color_loop_edge_freq/*!ira_loop_edge_freq*/(mtcsIraColor,a->loop_tree_node, a->regno, true);

}

/* Calculate and return the cost of putting allocno A into memory.  */
static int calculate_allocno_spill_cost (MtcsIraColor *self,MtcsIraAllocno * a)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int regno, cost;
   MtcsIraAllocno * parent_allocno;
   MtcsIraLoopTreeNode * parent_node, *loop_node;

   regno = a->regno;
   cost = a->updated_memory_cost - a->updated_class_cost;
   if (a->cap != NULL)
      return cost;
   loop_node = a->loop_tree_node;
   if ((parent_node = loop_node->parent) == NULL)
      return cost;
   if ((parent_allocno = parent_node->regno_allocno_map[regno]) == NULL)
      return cost;
   mtcs_ira_loop_border_costs border_costs (mtcsIra,a);
   if (parent_allocno->hard_regno < 0)
      cost -= border_costs.spill_outside_loop_cost ();
   else
      cost += (border_costs.spill_inside_loop_cost () - border_costs.move_between_loops_cost ());
   return cost;
}

/* Used for sorting allocnos for spilling.  */
static inline int allocno_spill_priority_compare (MtcsIraColor *self,MtcsIraAllocno * a1, MtcsIraAllocno * a2)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int pri1, pri2, diff;

   /* Avoid spilling static chain pointer pseudo when non-local goto is
   used.  */
   if ( mtcs_ira_non_spilled_static_chain_regno_p/*!non_spilled_static_chain_regno_p*/(mtcsIra,a1->regno))
      return 1;
   else if ( mtcs_ira_non_spilled_static_chain_regno_p/*!non_spilled_static_chain_regno_p*/(mtcsIra,a2->regno))
      return -1;
   if (a1->bad_spill_p && ! a2->bad_spill_p)
      return 1;
   if (a2->bad_spill_p && ! a1->bad_spill_p)
      return -1;
   pri1 = allocno_spill_priority(self,a1);
   pri2 = allocno_spill_priority(self,a2);
   if ((diff = pri1 - pri2) != 0)
      return diff;
   if ((diff = ALLOCNO_COLOR_DATA (a1)->temp - ALLOCNO_COLOR_DATA (a2)->temp) != 0)
   return diff;
   return a1->num - a2->num;
}

/* Used for sorting allocnos for spilling.  */
static int allocno_spill_sort_compare (const void *v1p, const void *v2p)
{
   MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraColor *mtcsIraColor=mtcs_ira_mgr_get_color(mtcsIraMgr);

   MtcsIraAllocno * p1 = *(const MtcsIraAllocno * *) v1p;
   MtcsIraAllocno * p2 = *(const MtcsIraAllocno * *) v2p;

   return allocno_spill_priority_compare (mtcsIraColor,p1, p2);
}

/* Push allocnos to the coloring stack.  The order of allocnos in the
   stack defines the order for the subsequent coloring.  */
static void push_allocnos_to_stack (MtcsIraColor *self)
{
   MtcsIraAllocno * a;
   int cost;
   /* Calculate uncolorable allocno spill costs.  */
   for (a = self->uncolorable_allocno_bucket; a != NULL; a = ALLOCNO_COLOR_DATA (a)->next_bucket_allocno)
      if (a->aclass != NO_REGS){
         cost = calculate_allocno_spill_cost(self,a);
         /* ??? Remove cost of copies between the coalesced
         allocnos.  */
         ALLOCNO_COLOR_DATA (a)->temp = cost;
      }
   sort_bucket(self,&self->uncolorable_allocno_bucket, allocno_spill_sort_compare);
   for (;;){
      push_only_colorable(self);
      a = self->uncolorable_allocno_bucket;
      if (a == NULL)
         break;
      remove_allocno_from_bucket_and_push(self,a, false);
   }
   ira_assert (self->colorable_allocno_bucket == NULL && self->uncolorable_allocno_bucket == NULL);
   ira_assert (self->uncolorable_allocnos_num == 0);
}

/* Pop the coloring stack and assign hard registers to the popped
   allocnos.  */
static void pop_allocnos_from_stack (MtcsIraColor *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   MtcsIraAllocno * allocno;
   enum reg_class aclass;

   for (;self->allocno_stack_vec.length () != 0;){
      allocno = self->allocno_stack_vec.pop ();
      aclass = allocno->aclass;
      if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL){
         fprintf (mtcsIraGlobal->ira_dump_file, "      Popping");
         mtcs_ira_allocno_print_expanded_allocno/*!ira_print_expanded_allocno*/(allocno);
         fprintf (mtcsIraGlobal->ira_dump_file, "  -- ");
      }
      if (aclass == NO_REGS){
         allocno->hard_regno = -1;
         allocno->assigned_p = true;
         ira_assert (allocno->updated_hard_reg_costs == NULL);
         ira_assert (allocno->updated_conflict_hard_reg_costs == NULL);
         if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
            fprintf (mtcsIraGlobal->ira_dump_file, "assign memory\n");
      }else if (assign_hard_reg(self,allocno, false)){
         if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
            fprintf (mtcsIraGlobal->ira_dump_file, "        assign reg %d\n", allocno->hard_regno);
      }else if (allocno->assigned_p){
         if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
            fprintf (mtcsIraGlobal->ira_dump_file, "spill%s\n", ALLOCNO_COLOR_DATA (allocno)->may_be_spilled_p ? "" : "!");
      }
      ALLOCNO_COLOR_DATA (allocno)->in_graph_p = true;
   }
}

/* Set up number of available hard registers for allocno A.  */
static void setup_allocno_available_regs_num (MtcsIraColor *self,MtcsIraAllocno * a)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   MtcsRegClass *mtcsRegClass =mtcs_reg_get_reg_class(mtcsReg);
   int i, n, hard_regno, hard_regs_num, nwords;
   enum reg_class aclass;
   AllocnoColorData * data;

   aclass = a->aclass;
   data = ALLOCNO_COLOR_DATA (a);
   data->available_regs_num = 0;
   if (aclass == NO_REGS)
      return;
   hard_regs_num = mtcsIra->x_ira_class_hard_regs_num[aclass];
   nwords = a->num_objects;
   for (n = 0, i = hard_regs_num - 1; i >= 0; i--){
      hard_regno = mtcsIra->x_ira_class_hard_regs[aclass][i];
      /* Checking only profitable hard regs.  */
      if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&data->profitable_hard_regs, hard_regno))
         n++;
   }
   data->available_regs_num = n;
   if (mtcsIraGlobal->internal_flag_ira_verbose <= 2 || mtcsIraGlobal->ira_dump_file == NULL)
      return;
   fprintf(mtcsIraGlobal->ira_dump_file,"      Allocno a%dr%d of %s(%d) has %d avail. regs ",
         a->num, a->regno,mtcsRegClass/*!reg_class_names*/[aclass].name, mtcsIra->x_ira_class_hard_regs_num[aclass], n);
   print_hard_reg_set(self,mtcsIraGlobal->ira_dump_file, &data->profitable_hard_regs, false);
   fprintf (mtcsIraGlobal->ira_dump_file, ", %snode: ",
         data->profitable_hard_regs == data->hard_regs_node->hard_regs->set ? "" : "^");
   print_hard_reg_set(self,mtcsIraGlobal->ira_dump_file,&data->hard_regs_node->hard_regs->set, false);
   for (i = 0; i < nwords; i++){
      MtcsIraObject * obj = a->objects[i];

      if (nwords != 1){
         if (i != 0)
            fprintf (mtcsIraGlobal->ira_dump_file, ", ");
         fprintf (mtcsIraGlobal->ira_dump_file, " obj %d", i);
      }
      fprintf (mtcsIraGlobal->ira_dump_file, " (confl regs = ");
      print_hard_reg_set(self,mtcsIraGlobal->ira_dump_file, &obj->total_conflict_hard_regs,false);
      fprintf (mtcsIraGlobal->ira_dump_file, ")");
   }
   fprintf (mtcsIraGlobal->ira_dump_file, "\n");
}

/* Put ALLOCNO in a bucket corresponding to its number and size of its
   conflicting allocnos and hard registers.  */
static void put_allocno_into_bucket (MtcsIraColor *self,MtcsIraAllocno * allocno)
{
   ALLOCNO_COLOR_DATA (allocno)->in_graph_p = true;
   setup_allocno_available_regs_num(self,allocno);
   if (setup_left_conflict_sizes_p(self,allocno))
      add_allocno_to_bucket(self,allocno, &self->colorable_allocno_bucket);
   else
      add_allocno_to_bucket(self,allocno, &self->uncolorable_allocno_bucket);
}

/* Set up priorities for N allocnos in array
   CONSIDERATION_ALLOCNOS.  */
static void setup_allocno_priorities (MtcsIraColor *self,MtcsIraAllocno * *consideration_allocnos, int n)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int i, length, nrefs, priority, max_priority, mult, diff;
   MtcsIraAllocno * a;

   max_priority = 0;
   for (i = 0; i < n; i++){
      a = consideration_allocnos[i];
      nrefs = a->nrefs;
      ira_assert (nrefs >= 0);
      mult = floor_log2 (a->nrefs) + 1;
      ira_assert (mult >= 0);
      mult *= mtcsIra->x_ira_reg_class_max_nregs[a->aclass][a->mode];
      diff = a->memory_cost - a->class_cost;
#ifdef __has_builtin
#if __has_builtin(__builtin_smul_overflow)
#define HAS_SMUL_OVERFLOW
#endif
#endif
      /* Multiplication can overflow for very large functions.
      Check the overflow and constrain the result if necessary: */
#ifdef HAS_SMUL_OVERFLOW
      if (__builtin_smul_overflow (mult, diff, &priority) || priority < -INT_MAX)
         priority = diff >= 0 ? INT_MAX : -INT_MAX;
#else
      static_assert(sizeof (long long) >= 2 * sizeof (int),
      "overflow code does not work for such int and long long sizes");
      long long priorityll = (long long) mult * diff;
      if (priorityll < -INT_MAX || priorityll > INT_MAX)
         priority = diff >= 0 ? INT_MAX : -INT_MAX;
      else
         priority = priorityll;
#endif
      self->allocno_priorities[a->num] = priority;
      if (priority < 0)
         priority = -priority;
      if (max_priority < priority)
         max_priority = priority;
   }
   mult = max_priority == 0 ? 1 : INT_MAX / max_priority;
   for (i = 0; i < n; i++){
      a = consideration_allocnos[i];
      length = a->excess_pressure_points_num;
      if (a->num_objects > 1)
         length /= a->num_objects;
      if (length <= 0)
         length = 1;
      self->allocno_priorities[a->num]= self->allocno_priorities[a->num] * mult / length;
   }
}

/* Sort allocnos according to the profit of usage of a hard register
   instead of memory for them. */
static int allocno_cost_compare_func (const void *v1p, const void *v2p)
{
   MtcsIraAllocno * p1 = *(const MtcsIraAllocno * *) v1p;
   MtcsIraAllocno * p2 = *(const MtcsIraAllocno * *) v2p;
   int c1, c2;

   c1 = p1->updated_memory_cost - p1->updated_class_cost;
   c2 = p2->updated_memory_cost - p2->updated_class_cost;
   if (c1 - c2)
      return c1 - c2;

   /* If regs are equally good, sort by allocno numbers, so that the
   results of qsort leave nothing to chance.  */
   return p1->num - p2->num;
}

/* Return savings on removed copies when ALLOCNO is assigned to
   HARD_REGNO.  */
static int allocno_copy_cost_saving (MtcsIraColor *self,MtcsIraAllocno * allocno, int hard_regno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);

   int cost = 0;
   machine_mode allocno_mode = allocno->mode;
   enum reg_class rclass;
   MtcsIraAllocnoCopy * cp, *next_cp;

   rclass =mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,hard_regno);
   if (mtcsIra->x_ira_reg_class_max_nregs[rclass][allocno_mode] > mtcsIra->x_ira_class_hard_regs_num[rclass])
      /* For the above condition the cost can be wrong.  Use the allocno
      class in this case.  */
      rclass = allocno->aclass;
   for (cp = allocno->allocno_copies; cp != NULL; cp = next_cp){
      if (cp->first == allocno){
         next_cp = cp->next_first_allocno_copy;
         if (cp->second->hard_regno != hard_regno)
            continue;
      }else if (cp->second == allocno){
         next_cp = cp->next_second_allocno_copy;
         if (cp->first->hard_regno != hard_regno)
            continue;
      }else
         gcc_unreachable ();
      mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,allocno_mode);
      cost += cp->freq * mtcsIraInt->x_ira_register_move_cost[allocno_mode][rclass][rclass];
   }
   return cost;
}

/* We used Chaitin-Briggs coloring to assign as many pseudos as
   possible to hard registers.  Let us try to improve allocation with
   cost point of view.  This function improves the allocation by
   spilling some allocnos and assigning the freed hard registers to
   other allocnos if it decreases the overall allocation cost.  */
static void improve_allocation (MtcsIraColor *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   unsigned int i;
   int j, k, n, hregno, conflict_hregno, base_cost, class_size, word, nwords;
   int check, spill_cost, min_cost, nregs, conflict_nregs, r, best;
   bool try_p;
   enum reg_class aclass, rclass;
   machine_mode mode;
   int *allocno_costs;
   int costs[firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/];

   HardRegSet conflicting_regs[2];
   conflicting_regs[0].count=mtcs_reg_get_hard_reg_element_count(mtcsReg);
   conflicting_regs[1].count=mtcs_reg_get_hard_reg_element_count(mtcsReg);
   HardRegSet /*!HARD_REG_SET*/ profitable_hard_regs = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};


   MtcsIraAllocno * a;
   bitmap_iterator bi;
   int saved_nregs;
   int add_cost;

   /* Don't bother to optimize the code with static chain pointer and
   non-local goto in order not to spill the chain pointer
   pseudo.  */
   if (cfun->static_chain_decl && mtcsRtlData/*!crtl*/->has_nonlocal_goto)
      return;
   /* Clear counts used to process conflicting allocnos only once for
   each allocno.  */
   EXECUTE_IF_SET_IN_BITMAP (self->coloring_allocno_bitmap, 0, i, bi)
      ALLOCNO_COLOR_DATA (mtcsIraBuild->ira_allocnos[i])->temp = 0;
   check = n = 0;
   /* Process each allocno and try to assign a hard register to it by
   spilling some its conflicting allocnos.  */
   EXECUTE_IF_SET_IN_BITMAP (self->coloring_allocno_bitmap, 0, i, bi){
      a = mtcsIraBuild->ira_allocnos[i];
      ALLOCNO_COLOR_DATA (a)->temp = 0;
      if (empty_profitable_hard_regs(self,a))
         continue;
      check++;
      aclass = a->aclass;
      allocno_costs = a->hard_reg_costs;
      if ((hregno = a->hard_regno) < 0)
         base_cost = a->updated_memory_cost;
      else if (allocno_costs == NULL)
         /* It means that assigning a hard register is not profitable
         (we don't waste memory for hard register costs in this
         case).  */
         continue;
      else
         base_cost = (allocno_costs[mtcsIraInt->x_ira_class_hard_reg_index[aclass][hregno]]- allocno_copy_cost_saving(self,a, hregno));
      try_p = false;
      get_conflict_and_start_profitable_regs(self,a, false,
      conflicting_regs, &profitable_hard_regs);
      class_size = mtcsIra->x_ira_class_hard_regs_num[aclass];
      mode = a->mode;
      /* Set up cost improvement for usage of each profitable hard
      register for allocno A.  */
      for (j = 0; j < class_size; j++){
         hregno = mtcsIra->x_ira_class_hard_regs[aclass][j];
         if (! check_hard_reg_p(self,a, hregno, conflicting_regs, &profitable_hard_regs))
            continue;
         if (NUM_REGISTER_FILTERS  && !test_register_filters (a->register_filters, hregno))
            continue;
         ira_assert (mtcsIraInt->x_ira_class_hard_reg_index[aclass][hregno] == j);
         k = allocno_costs == NULL ? 0 : j;
         costs[hregno] = (allocno_costs == NULL ? a->updated_class_cost : allocno_costs[k]);
         costs[hregno] -= allocno_copy_cost_saving(self,a, hregno);

         if ((saved_nregs = calculate_saved_nregs(self,hregno, mode)) != 0){
            /* We need to save/restore the hard register in
            epilogue/prologue.  Therefore we increase the cost.
            Since the prolog is placed in the entry BB, the frequency
            of the entry BB is considered while computing the cost.  */
            rclass = mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,hregno);
            add_cost = ((mtcsIra->x_ira_memory_move_cost[mode][rclass][0]   + mtcsIra->x_ira_memory_move_cost[mode][rclass][1])
                  * saved_nregs / mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,hregno,mode) - 1)
                  * REG_FREQ_FROM_BB (ENTRY_BLOCK_PTR_FOR_FN (cfun));
            costs[hregno] += add_cost;
         }

         costs[hregno] -= base_cost;
         if (costs[hregno] < 0)
            try_p = true;
      }
      if (! try_p)
         /* There is no chance to improve the allocation cost by
         assigning hard register to allocno A even without spilling
         conflicting allocnos.  */
         continue;
      auto_bitmap allocnos_to_spill;
      HardRegSet /*!HARD_REG_SET*/ soft_conflict_regs = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};

      mode = a->mode;
      nwords = a->num_objects;
      /* Process each allocno conflicting with A and update the cost
      improvement for profitable hard registers of A.  To use a
      hard register for A we need to spill some conflicting
      allocnos and that creates penalty for the cost
      improvement.  */
      for (word = 0; word < nwords; word++){
         MtcsIraObject * conflict_obj;
         MtcsIraObject * obj = a->objects[word];
         MtcsIraObjectConflictIterator oci;

         MTCS_FOR_EACH_OBJECT_CONFLICT(mtcsIraBuild, obj, conflict_obj, oci){
            MtcsIraAllocno * conflict_a = conflict_obj->allocno;

            if (ALLOCNO_COLOR_DATA (conflict_a)->temp == check)
               /* We already processed this conflicting allocno
               because we processed earlier another object of the
               conflicting allocno.  */
               continue;
            ALLOCNO_COLOR_DATA (conflict_a)->temp = check;
            if ((conflict_hregno = conflict_a->hard_regno) < 0)
               continue;
            auto spill_a = mtcs_ira_color_soft_conflict/*!ira_soft_conflict*/(self,a, conflict_a);
            if (spill_a) {
               if (!bitmap_set_bit (allocnos_to_spill,spill_a->num))
                  continue;
               mtcs_ira_loop_border_costs border_costs (mtcsIra,spill_a);
               spill_cost = border_costs.spill_inside_loop_cost ();
            }else{
               spill_cost = conflict_a->updated_memory_cost;
               k = (mtcsIraInt->x_ira_class_hard_reg_index[conflict_a->aclass][conflict_hregno]);
               ira_assert (k >= 0);
               if ((allocno_costs = conflict_a->hard_reg_costs) != NULL)
                  spill_cost -= allocno_costs[k];
               else
                  spill_cost -= conflict_a->updated_class_cost;
               spill_cost += allocno_copy_cost_saving(self,conflict_a, conflict_hregno);
            }
            conflict_nregs = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,conflict_hregno, conflict_a->mode);
            auto note_conflict = [&](int r)
            {
               if (check_hard_reg_p(self,a, r,conflicting_regs, &profitable_hard_regs)){
                  if (spill_a)
                     mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&soft_conflict_regs, r);
                  costs[r] += spill_cost;
               }
            };
            for (r = conflict_hregno; r >= 0 && (int)mtcs_reg_end_hard_regno/*!end_hard_regno*/(mtcsReg,
                  mode, r) > conflict_hregno; r--)
               note_conflict (r);
            for (r = conflict_hregno + 1; r < conflict_hregno + conflict_nregs; r++)
               note_conflict (r);
         }
      }
      min_cost = INT_MAX;
      best = -1;
      /* Now we choose hard register for A which results in highest
      allocation cost improvement.  */
      for (j = 0; j < class_size; j++){
         hregno = mtcsIra->x_ira_class_hard_regs[aclass][j];
         if (check_hard_reg_p(self,a, hregno, conflicting_regs, &profitable_hard_regs) && min_cost > costs[hregno]){
            best = hregno;
            min_cost = costs[hregno];
         }
      }
      if (min_cost >= 0)
         /* We are in a situation when assigning any hard register to A
         by spilling some conflicting allocnos does not improve the
         allocation cost.  */
         continue;
      spill_soft_conflicts(self,a, allocnos_to_spill, &soft_conflict_regs, best);
      nregs = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,best, mode);
      /* Now spill conflicting allocnos which contain a hard register
      of A when we assign the best chosen hard register to it.  */
      for (word = 0; word < nwords; word++){
         MtcsIraObject * conflict_obj;
         MtcsIraObject * obj = a->objects[word];
         MtcsIraObjectConflictIterator oci;

         MTCS_FOR_EACH_OBJECT_CONFLICT(mtcsIraBuild, obj, conflict_obj, oci){
            MtcsIraAllocno * conflict_a = conflict_obj->allocno;

            if ((conflict_hregno = conflict_a->hard_regno) < 0)
               continue;
            conflict_nregs = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,conflict_hregno,conflict_a->mode);
            if (best + nregs <= conflict_hregno || conflict_hregno + conflict_nregs <= best)
               /* No intersection.  */
               continue;
            conflict_a->hard_regno = -1;
            self->sorted_allocnos[n++] = conflict_a;
            if (mtcsIraGlobal->internal_flag_ira_verbose > 2 && mtcsIraGlobal->ira_dump_file != NULL)
               fprintf (mtcsIraGlobal->ira_dump_file, "Spilling a%dr%d for a%dr%d\n",conflict_a->num, conflict_a->regno,a->num, a->regno);
         }
      }
      /* Assign the best chosen hard register to A.  */
      a->hard_regno = best;

      for (j = nregs - 1; j >= 0; j--)
         self->allocated_hardreg_p[best + j] = true;

      if (mtcsIraGlobal->internal_flag_ira_verbose > 2 && mtcsIraGlobal->ira_dump_file != NULL)
         fprintf (mtcsIraGlobal->ira_dump_file, "Assigning %d to a%dr%d\n",best, a->num, a->regno);
   }

   if (n == 0)
      return;
   /* We spilled some allocnos to assign their hard registers to other
   allocnos.  The spilled allocnos are now in array
   'sorted_allocnos'.  There is still a possibility that some of the
   spilled allocnos can get hard registers.  So let us try assign
   them hard registers again (just a reminder -- function
   'assign_hard_reg' assigns hard registers only if it is possible
   and profitable).  We process the spilled allocnos with biggest
   benefit to get hard register first -- see function
   'allocno_cost_compare_func'.  */
   qsort (self->sorted_allocnos, n, sizeof (MtcsIraAllocno *), allocno_cost_compare_func);
   for (j = 0; j < n; j++){
      a = self->sorted_allocnos[j];
      a->assigned_p = false;
      if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL){
         fprintf (mtcsIraGlobal->ira_dump_file, "      ");
         mtcs_ira_allocno_print_expanded_allocno/*!ira_print_expanded_allocno*/(a);
         fprintf (mtcsIraGlobal->ira_dump_file, "  -- ");
      }
      if (assign_hard_reg(self,a, false)){
         if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
            fprintf (mtcsIraGlobal->ira_dump_file, "assign hard reg %d\n",a->hard_regno);
      }else{
         if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
            fprintf (mtcsIraGlobal->ira_dump_file, "assign memory\n");
      }
   }
}

/* Sort allocnos according to their priorities.  */
static int allocno_priority_compare_func (const void *v1p, const void *v2p)
{
   MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraColor *self=mtcs_ira_mgr_get_color(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   MtcsIraAllocno * a1 = *(const MtcsIraAllocno * *) v1p;
   MtcsIraAllocno * a2 = *(const MtcsIraAllocno * *) v2p;
   int pri1, pri2, diff;

   /* Assign hard reg to static chain pointer pseudo first when
   non-local goto is used.  */
   if ((diff = ( mtcs_ira_non_spilled_static_chain_regno_p/*!non_spilled_static_chain_regno_p*/(mtcsIra,a2->regno)
         -  mtcs_ira_non_spilled_static_chain_regno_p/*!non_spilled_static_chain_regno_p*/(mtcsIra,a1->regno))) != 0)
      return diff;
   pri1 = self->allocno_priorities[a1->num];
   pri2 = self->allocno_priorities[a2->num];
   if (pri2 != pri1)
      return SORTGT (pri2, pri1);

   /* If regs are equally good, sort by allocnos, so that the results of
   qsort leave nothing to chance.  */
   return a1->num - a2->num;
}

/* Chaitin-Briggs coloring for allocnos in COLORING_ALLOCNO_BITMAP
   taking into account allocnos in CONSIDERATION_ALLOCNO_BITMAP.  */
static void color_allocnos (MtcsIraColor *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   unsigned int i, n;
   bitmap_iterator bi;
   MtcsIraAllocno * a;

   setup_profitable_hard_regs(self);
   EXECUTE_IF_SET_IN_BITMAP (self->coloring_allocno_bitmap, 0, i, bi){
      AllocnoColorData * data;
      MtcsIraAllocnoPref * pref, *next_pref;

      a = mtcsIraBuild->ira_allocnos[i];
      data = ALLOCNO_COLOR_DATA (a);
      data->conflict_allocno_hard_prefs = 0;
      for (pref = a->allocno_prefs; pref != NULL; pref = next_pref){
         next_pref = pref->next_pref;
         if (! mtcs_ira_hard_reg_in_set_p/*!ira_hard_reg_in_set_p*/(mtcsIra,pref->hard_regno,a->mode,&data->profitable_hard_regs))
            mtcs_ira_build_remove_pref/*!ira_remove_pref*/(mtcsIraBuild,pref);
      }
   }

   if (mtcsOptionsItem->x_flag_ira_algorithm == IRA_ALGORITHM_PRIORITY){
      n = 0;
      EXECUTE_IF_SET_IN_BITMAP (self->coloring_allocno_bitmap, 0, i, bi){
         a = mtcsIraBuild->ira_allocnos[i];
         if (a->aclass == NO_REGS){
            a->hard_regno = -1;
            a->assigned_p = true;
            ira_assert (a->updated_hard_reg_costs == NULL);
            ira_assert (a->updated_conflict_hard_reg_costs == NULL);
            if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL){
               fprintf (mtcsIraGlobal->ira_dump_file, "      Spill");
               mtcs_ira_allocno_print_expanded_allocno/*!ira_print_expanded_allocno*/(a);
               fprintf (mtcsIraGlobal->ira_dump_file, "\n");
            }
            continue;
         }
         self->sorted_allocnos[n++] = a;
      }
      if (n != 0){
         setup_allocno_priorities(self,self->sorted_allocnos, n);
         qsort (self->sorted_allocnos, n, sizeof (MtcsIraAllocno *),allocno_priority_compare_func);
         for (i = 0; i < n; i++){
            a = self->sorted_allocnos[i];
            if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL){
               fprintf (mtcsIraGlobal->ira_dump_file, "      ");
               mtcs_ira_allocno_print_expanded_allocno/*!ira_print_expanded_allocno*/(a);
               fprintf (mtcsIraGlobal->ira_dump_file, "  -- ");
            }
            if (assign_hard_reg(self,a, false)){
               if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
                  fprintf (mtcsIraGlobal->ira_dump_file, "assign hard reg %d\n",a->hard_regno);
            }else{
               if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
                  fprintf (mtcsIraGlobal->ira_dump_file, "assign memory\n");
            }
         }
      }
   }else{
      form_allocno_hard_regs_nodes_forest(self);
      if (mtcsIraGlobal->internal_flag_ira_verbose > 2 && mtcsIraGlobal->ira_dump_file != NULL)
         print_hard_regs_forest(self,mtcsIraGlobal->ira_dump_file);
      EXECUTE_IF_SET_IN_BITMAP (self->coloring_allocno_bitmap, 0, i, bi){
         a = mtcsIraBuild->ira_allocnos[i];
         if (a->aclass != NO_REGS && ! empty_profitable_hard_regs(self,a)){
            ALLOCNO_COLOR_DATA (a)->in_graph_p = true;
            update_conflict_allocno_hard_prefs(self,a);
         }else{
            a->hard_regno = -1;
            a->assigned_p = true;
            /* We don't need updated costs anymore.  */
            mtcs_ira_build_free_allocno_updated_costs/*!ira_free_allocno_updated_costs*/(mtcsIraBuild,a);
            if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL){
               fprintf (mtcsIraGlobal->ira_dump_file, "      Spill");
               mtcs_ira_allocno_print_expanded_allocno/*!ira_print_expanded_allocno*/(a);
               fprintf (mtcsIraGlobal->ira_dump_file, "\n");
            }
         }
      }
      /* Put the allocnos into the corresponding buckets.  */
      self->colorable_allocno_bucket = NULL;
      self->uncolorable_allocno_bucket = NULL;
      EXECUTE_IF_SET_IN_BITMAP (self->coloring_allocno_bitmap, 0, i, bi){
         a = mtcsIraBuild->ira_allocnos[i];
         if (ALLOCNO_COLOR_DATA (a)->in_graph_p)
            put_allocno_into_bucket(self,a);
      }
      push_allocnos_to_stack(self);
      pop_allocnos_from_stack(self);
      finish_allocno_hard_regs_nodes_forest(self);
   }
   improve_allocation(self);
}

/* Output information about the loop given by its LOOP_TREE_NODE.  */
static void print_loop_title (MtcsIraColor *self,MtcsIraLoopTreeNode * loop_tree_node)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   MtcsRegClass *mtcsRegClass =mtcs_reg_get_reg_class(mtcsReg);

   unsigned int j;
   bitmap_iterator bi;
   MtcsIraLoopTreeNode * subloop_node, *dest_loop_node;
   edge e;
   edge_iterator ei;

   if (loop_tree_node->parent == NULL)
      fprintf (mtcsIraGlobal->ira_dump_file, "\n  Loop 0 (parent -1, header bb%d, depth 0)\n    bbs:",NUM_FIXED_BLOCKS);
   else{
      ira_assert (current_loops != NULL && loop_tree_node->loop != NULL);
      fprintf (mtcsIraGlobal->ira_dump_file,"\n  Loop %d (parent %d, header bb%d, depth %d)\n    bbs:",
            loop_tree_node->loop_num, loop_tree_node->parent->loop_num,
            loop_tree_node->loop->header->index,loop_depth (loop_tree_node->loop));
   }
   for (subloop_node = loop_tree_node->children; subloop_node != NULL; subloop_node = subloop_node->next)
      if (subloop_node->bb != NULL){
         fprintf (mtcsIraGlobal->ira_dump_file, " %d", subloop_node->bb->index);
         FOR_EACH_EDGE (e, ei, subloop_node->bb->succs)
            if (e->dest != EXIT_BLOCK_PTR_FOR_FN (cfun) && ((dest_loop_node = MTCS_IRA_BB_NODE (e->dest)->parent)!= loop_tree_node))
               fprintf (mtcsIraGlobal->ira_dump_file, "(->%d:l%d)", e->dest->index, dest_loop_node->loop_num);
      }
   fprintf (mtcsIraGlobal->ira_dump_file, "\n    all:");
   EXECUTE_IF_SET_IN_BITMAP (loop_tree_node->all_allocnos, 0, j, bi)
      fprintf (mtcsIraGlobal->ira_dump_file, " %dr%d", j, mtcsIraBuild->ira_allocnos[j]->regno);
   fprintf (mtcsIraGlobal->ira_dump_file, "\n    modified regnos:");
   EXECUTE_IF_SET_IN_BITMAP (loop_tree_node->modified_regnos, 0, j, bi)
      fprintf (mtcsIraGlobal->ira_dump_file, " %d", j);
   fprintf (mtcsIraGlobal->ira_dump_file, "\n    border:");
   EXECUTE_IF_SET_IN_BITMAP (loop_tree_node->border_allocnos, 0, j, bi)
      fprintf (mtcsIraGlobal->ira_dump_file, " %dr%d", j, mtcsIraBuild->ira_allocnos[j]->regno);
   fprintf (mtcsIraGlobal->ira_dump_file, "\n    Pressure:");
   for (j = 0; (int) j < mtcsIra->x_ira_pressure_classes_num; j++){
      enum reg_class pclass;

      pclass = mtcsIra->x_ira_pressure_classes[j];
      if (loop_tree_node->reg_pressure[pclass] == 0)
         continue;
      fprintf (mtcsIraGlobal->ira_dump_file, " %s=%d", mtcsRegClass/*!reg_class_names*/[pclass].name,
            loop_tree_node->reg_pressure[pclass]);
   }
   fprintf (mtcsIraGlobal->ira_dump_file, "\n");
}

/* Color the allocnos inside loop (in the extreme case it can be all
   of the function) given the corresponding LOOP_TREE_NODE.  The
   function is called for each loop during top-down traverse of the
   loop tree.  */
static void colorPass_cb (MtcsIraLoopTreeNode * loop_tree_node,void *userData)
{
   MtcsIraColor *self=(MtcsIraColor *)userData;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int regno, hard_regno, index = -1, n;
   int cost;
   unsigned int j;
   bitmap_iterator bi;
   machine_mode mode;
   enum reg_class rclass, aclass;
   MtcsIraAllocno * a, *subloop_allocno;
   MtcsIraLoopTreeNode * subloop_node;

   ira_assert (loop_tree_node->bb == NULL);
   if (mtcsIraGlobal->internal_flag_ira_verbose > 1 && mtcsIraGlobal->ira_dump_file != NULL)
      print_loop_title(self,loop_tree_node);

   bitmap_copy (self->coloring_allocno_bitmap, loop_tree_node->all_allocnos);
   bitmap_copy (self->consideration_allocno_bitmap, self->coloring_allocno_bitmap);
   n = 0;
   EXECUTE_IF_SET_IN_BITMAP (self->consideration_allocno_bitmap, 0, j, bi){
      a = mtcsIraBuild->ira_allocnos[j];
      n++;
      if (! a->assigned_p)
         continue;
      bitmap_clear_bit (self->coloring_allocno_bitmap, a->num);
   }
   self->allocno_color_data = (AllocnoColorData *) ira_allocate (sizeof (AllocnoColorData)* n);
   memset (self->allocno_color_data, 0, sizeof (AllocnoColorData) * n);
   self->curr_allocno_process = 0;
   n = 0;
   EXECUTE_IF_SET_IN_BITMAP (self->consideration_allocno_bitmap, 0, j, bi){
      a = mtcsIraBuild->ira_allocnos[j];
      ALLOCNO_ADD_DATA (a) = self->allocno_color_data + n;
      n++;
   }
   init_allocno_threads(self);
   /* Color all mentioned allocnos including transparent ones.  */
   color_allocnos(self);
   /* Process caps.  They are processed just once.  */
   if (mtcsOptionsItem->x_flag_ira_region == IRA_REGION_MIXED || mtcsOptionsItem->x_flag_ira_region == IRA_REGION_ALL)
      EXECUTE_IF_SET_IN_BITMAP (loop_tree_node->all_allocnos, 0, j, bi){
         a = mtcsIraBuild->ira_allocnos[j];
         if (a->cap_member == NULL)
            continue;
         /* Remove from processing in the next loop.  */
         bitmap_clear_bit (self->consideration_allocno_bitmap, j);
         rclass = a->aclass;
         subloop_allocno = a->cap_member;
         subloop_node = subloop_allocno->loop_tree_node;
         if (mtcs_ira_single_region_allocno_p/*!ira_single_region_allocno_p*/(mtcsIra,a, subloop_allocno)){
            mode = a->mode;
            hard_regno = a->hard_regno;
            if (hard_regno >= 0){
               index = mtcsIraInt->x_ira_class_hard_reg_index[rclass][hard_regno];
               ira_assert (index >= 0);
            }
            regno = a->regno;
            ira_assert (!subloop_allocno->assigned_p);
            subloop_allocno->hard_regno = hard_regno;
            subloop_allocno->assigned_p = true;
            if (hard_regno >= 0)
               update_costs_from_copies(self,subloop_allocno, true, true);
            /* We don't need updated costs anymore.  */
            mtcs_ira_build_free_allocno_updated_costs/*!ira_free_allocno_updated_costs*/(mtcsIraBuild,subloop_allocno);
         }
      }
   /* Update costs of the corresponding allocnos (not caps) in the
   subloops.  */
   for (subloop_node = loop_tree_node->subloops; subloop_node != NULL; subloop_node = subloop_node->subloop_next){
      ira_assert (subloop_node->bb == NULL);
      EXECUTE_IF_SET_IN_BITMAP (self->consideration_allocno_bitmap, 0, j, bi){
         a = mtcsIraBuild->ira_allocnos[j];
         ira_assert (a->cap_member == NULL);
         mode = a->mode;
         rclass = a->aclass;
         hard_regno = a->hard_regno;
         /* Use hard register class here.  ??? */
         if (hard_regno >= 0){
            index = mtcsIraInt->x_ira_class_hard_reg_index[rclass][hard_regno];
            ira_assert (index >= 0);
         }
         regno = a->regno;
         /* ??? conflict costs */
         subloop_allocno = subloop_node->regno_allocno_map[regno];
         if (subloop_allocno == NULL|| subloop_allocno->cap != NULL)
            continue;
         ira_assert (subloop_allocno->aclass == rclass);
         ira_assert (bitmap_bit_p (subloop_node->all_allocnos,subloop_allocno->num));
         if (mtcs_ira_single_region_allocno_p/*!ira_single_region_allocno_p*/(mtcsIra,a, subloop_allocno)
         || !mtcs_ira_subloop_allocnos_can_differ_p/*!ira_subloop_allocnos_can_differ_p*/(mtcsIra,a, hard_regno >= 0,false)){
            gcc_assert (!subloop_allocno->might_conflict_with_parent_p);
            if (! subloop_allocno->assigned_p){
               subloop_allocno->hard_regno = hard_regno;
               subloop_allocno->assigned_p = true;
               if (hard_regno >= 0)
                  update_costs_from_copies(self,subloop_allocno, true, true);
               /* We don't need updated costs anymore.  */
               mtcs_ira_build_free_allocno_updated_costs/*!ira_free_allocno_updated_costs*/(mtcsIraBuild,subloop_allocno);
            }
         }else if (hard_regno < 0){
            /* If we allocate a register to SUBLOOP_ALLOCNO, we'll need
            to load the register on entry to the subloop and store
            the register back on exit from the subloop.  This incurs
            a fixed cost for all registers.  Since UPDATED_MEMORY_COST
            is (and should only be) used relative to the register costs
            for the same allocno, we can subtract this shared register
            cost from the memory cost.  */
            mtcs_ira_loop_border_costs border_costs (mtcsIra,subloop_allocno);
            subloop_allocno->updated_memory_cost -= border_costs.spill_outside_loop_cost ();
         }else{
            mtcs_ira_loop_border_costs border_costs (mtcsIra,subloop_allocno);
            aclass = subloop_allocno->aclass;
            mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,mode);
            cost = border_costs.move_between_loops_cost ();
            mtcs_ira_build_allocate_and_set_or_copy_costs/*!ira_allocate_and_set_or_copy_costs*/(mtcsIraBuild,
                  &subloop_allocno->updated_hard_reg_costs, aclass, subloop_allocno->updated_class_cost,  subloop_allocno->hard_reg_costs);
            mtcs_ira_build_allocate_and_set_or_copy_costs/*!ira_allocate_and_set_or_copy_costs*/(mtcsIraBuild,
                  &subloop_allocno->updated_conflict_hard_reg_costs, aclass, 0, subloop_allocno->conflict_hard_reg_costs);
            subloop_allocno->updated_hard_reg_costs[index] -= cost;
            subloop_allocno->updated_conflict_hard_reg_costs[index] -= cost;
            if (subloop_allocno->updated_class_cost  > subloop_allocno->updated_hard_reg_costs[index])
               subloop_allocno->updated_class_cost = subloop_allocno->updated_hard_reg_costs[index];
            /* If we spill SUBLOOP_ALLOCNO, we'll need to store HARD_REGNO
            on entry to the subloop and restore HARD_REGNO on exit from
            the subloop.  */
            subloop_allocno->updated_memory_cost += border_costs.spill_inside_loop_cost ();
         }
      }
   }

   ira_free (self->allocno_color_data);
   EXECUTE_IF_SET_IN_BITMAP (self->consideration_allocno_bitmap, 0, j, bi){
      a = mtcsIraBuild->ira_allocnos[j];
      ALLOCNO_ADD_DATA (a) = NULL;
   }
}

/* Initialize the common data for coloring and calls functions to do
   Chaitin-Briggs and regional coloring.  */
static void do_coloring (MtcsIraColor *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   self->coloring_allocno_bitmap = ira_allocate_bitmap ();
   if (mtcsIraGlobal->internal_flag_ira_verbose > 0 && mtcsIraGlobal->ira_dump_file != NULL)
      fprintf (mtcsIraGlobal->ira_dump_file, "\n**** Allocnos coloring:\n\n");

   mtcs_ira_build_traverse_loop_tree/*!ira_traverse_loop_tree*/(mtcsIraBuild,false,
         mtcsIraBuild->ira_loop_tree_root, colorPass_cb, NULL,(void *)self);

   if (mtcsIraGlobal->internal_flag_ira_verbose > 1 && mtcsIraGlobal->ira_dump_file != NULL)
      mtcs_ira_print_disposition/*!ira_print_disposition*/(mtcsIra,mtcsIraGlobal->ira_dump_file);

   ira_free_bitmap (self->coloring_allocno_bitmap);
}



/* Move spill/restore code, which are to be generated in ira-emit.cc,
   to less frequent points (if it is profitable) by reassigning some
   allocnos (in loop with subloops containing in another loop) to
   memory which results in longer live-range where the corresponding
   pseudo-registers will be in memory.  */
static void move_spill_restore (MtcsIraColor *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int cost, regno, hard_regno, hard_regno2, index;
   bool changed_p;
   machine_mode mode;
   enum reg_class rclass;
   MtcsIraAllocno * a, *parent_allocno, *subloop_allocno;
   MtcsIraLoopTreeNode * parent, *loop_node, *subloop_node;
   MtcsIraAllocnoIterator ai;

   for (;;){
      changed_p = false;
      if (mtcsIraGlobal->internal_flag_ira_verbose > 0 && mtcsIraGlobal->ira_dump_file != NULL)
         fprintf (mtcsIraGlobal->ira_dump_file, "New iteration of spill/restore move\n");
      MTCS_FOR_EACH_ALLOCNO(mtcsIraBuild,a, ai){
         regno = a->regno;
         loop_node = a->loop_tree_node;
         if (a->cap_member != NULL
         || a->cap != NULL
         || (hard_regno = a->hard_regno) < 0
         || loop_node->children == NULL
         /* don't do the optimization because it can create
         copies and the reload pass can spill the allocno set
         by copy although the allocno will not get memory
         slot.  */
         || ira_equiv_no_lvalue_p (regno)
         || !bitmap_bit_p (loop_node->border_allocnos, a->num)
         /* Do not spill static chain pointer pseudo when
         non-local goto is used.  */
         ||  mtcs_ira_non_spilled_static_chain_regno_p/*!non_spilled_static_chain_regno_p*/(mtcsIra,regno))
            continue;
         mode = a->mode;
         rclass = a->aclass;
         index = mtcsIraInt->x_ira_class_hard_reg_index[rclass][hard_regno];
         ira_assert (index >= 0);
         cost = (a->memory_cost- (a->hard_reg_costs == NULL ? a->class_cost : a->hard_reg_costs[index]));
         mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,mode);
         for (subloop_node = loop_node->subloops;subloop_node != NULL;subloop_node = subloop_node->subloop_next){
            ira_assert (subloop_node->bb == NULL);
            subloop_allocno = subloop_node->regno_allocno_map[regno];
            if (subloop_allocno == NULL)
               continue;
            ira_assert (rclass == subloop_allocno->aclass);
            mtcs_ira_loop_border_costs border_costs (mtcsIra,subloop_allocno);

            /* We have accumulated cost.  To get the real cost of
            allocno usage in the loop we should subtract the costs
            added by propagate_allocno_info for the subloop allocnos.  */
            int reg_cost = (subloop_allocno->hard_reg_costs == NULL ? subloop_allocno->class_cost : subloop_allocno->hard_reg_costs[index]);

            int spill_cost  = (border_costs.spill_inside_loop_cost () + subloop_allocno->memory_cost);

            /* If HARD_REGNO conflicts with SUBLOOP_A then
            propagate_allocno_info will have propagated
            the cost of spilling HARD_REGNO in SUBLOOP_NODE.
            (ira_subloop_allocnos_can_differ_p must be true
            in that case.)  If HARD_REGNO is a caller-saved
            register, we might have modelled it in the same way.

            Otherwise, SPILL_COST acted as a cap on the propagated
            register cost, in cases where the allocations can differ.  */
            auto conflicts = mtcs_ira_allocno_total_conflict_hard_regs/*!ira_total_conflict_hard_regs*/(subloop_allocno);
            if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&conflicts, hard_regno)
            || (mtcs_ira_allocno_need_caller_save_p/*!ira_need_caller_save_p*/(subloop_allocno, hard_regno)
            && mtcs_ira_caller_save_loop_spill_p/*!ira_caller_save_loop_spill_p*/(mtcsIra,a, subloop_allocno,spill_cost)))
               reg_cost = spill_cost;
            else if (mtcs_ira_subloop_allocnos_can_differ_p/*!ira_subloop_allocnos_can_differ_p*/(mtcsIra,a))
               reg_cost = MIN (reg_cost, spill_cost);

            cost -= subloop_allocno->memory_cost - reg_cost;

            if ((hard_regno2 = subloop_allocno->hard_regno) < 0)
               /* The register was spilled in the subloop.  If we spill
               it in the outer loop too then we'll no longer need to
               save the register on entry to the subloop and restore
               the register on exit from the subloop.  */
               cost -= border_costs.spill_inside_loop_cost ();
            else{
               /* The register was also allocated in the subloop.  If we
               spill it in the outer loop then we'll need to load the
               register on entry to the subloop and store the register
               back on exit from the subloop.  */
               cost += border_costs.spill_outside_loop_cost ();
               if (hard_regno2 != hard_regno)
                  cost -= border_costs.move_between_loops_cost ();
            }
         }

         if ((parent = loop_node->parent) != NULL && (parent_allocno = parent->regno_allocno_map[regno]) != NULL){
            ira_assert (rclass == parent_allocno->aclass);
            mtcs_ira_loop_border_costs border_costs (mtcsIra,a);
            if ((hard_regno2 = ALLOCNO_HARD_REGNO (parent_allocno)) < 0)
               /* The register was spilled in the parent loop.  If we spill
               it in this loop too then we'll no longer need to load the
               register on entry to this loop and save the register back
               on exit from this loop.  */
               cost -= border_costs.spill_outside_loop_cost ();
            else{
               /* The register was also allocated in the parent loop.
               If we spill it in this loop then we'll need to save
               the register on entry to this loop and restore the
               register on exit from this loop.  */
               cost += border_costs.spill_inside_loop_cost ();
               if (hard_regno2 != hard_regno)
                  cost -= border_costs.move_between_loops_cost ();
            }
         }
         if (cost < 0){
            a->hard_regno = -1;
            if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL){
               fprintf(mtcsIraGlobal->ira_dump_file, "      Moving spill/restore for a%dr%d up from loop %d",
                     a->num, regno, loop_node->loop_num);
               fprintf (mtcsIraGlobal->ira_dump_file, " - profit %d\n", -cost);
            }
            changed_p = true;
         }
      }
      if (! changed_p)
         break;
   }
}



/* Update current hard reg costs and current conflict hard reg costs
   for allocno A.  It is done by processing its copies containing
   other allocnos already assigned.  */
static void update_curr_costs (MtcsIraColor *self,MtcsIraAllocno * a)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int i, hard_regno, cost;
   machine_mode mode;
   enum reg_class aclass, rclass;
   MtcsIraAllocno * another_a;
   MtcsIraAllocnoCopy * cp, *next_cp;

   mtcs_ira_build_free_allocno_updated_costs/*!ira_free_allocno_updated_costs*/(mtcsIraBuild,a);
   ira_assert (! a->assigned_p);
   aclass = a->aclass;
   if (aclass == NO_REGS)
      return;
   mode = a->mode;
   mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,mode);
   for (cp = a->allocno_copies; cp != NULL; cp = next_cp){
      if (cp->first == a){
         next_cp = cp->next_first_allocno_copy;
         another_a = cp->second;
      }else if (cp->second == a){
         next_cp = cp->next_second_allocno_copy;
         another_a = cp->first;
      }else
         gcc_unreachable ();
      if (! mtcsIra->x_ira_reg_classes_intersect_p[aclass][another_a->aclass]
      || ! another_a->assigned_p || (hard_regno = another_a->hard_regno) < 0)
         continue;
      rclass = mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,hard_regno);
      i = mtcsIraInt->x_ira_class_hard_reg_index[aclass][hard_regno];
      if (i < 0)
         continue;
      cost = (cp->first == a ? mtcsIraInt->x_ira_register_move_cost[mode][rclass][aclass]
                             : mtcsIraInt->x_ira_register_move_cost[mode][aclass][rclass]);
      mtcs_ira_build_allocate_and_set_or_copy_costs/*!ira_allocate_and_set_or_copy_costs*/(mtcsIraBuild,
            &a->updated_hard_reg_costs, aclass, a->class_cost, a->hard_reg_costs);
      mtcs_ira_build_allocate_and_set_or_copy_costs/*!ira_allocate_and_set_or_copy_costs*/(mtcsIraBuild,
            &a->updated_conflict_hard_reg_costs,aclass, 0, a->conflict_hard_reg_costs);
      a->updated_hard_reg_costs[i] -= cp->freq * cost;
      a->updated_conflict_hard_reg_costs[i] -= cp->freq * cost;
   }
}

/* Try to assign hard registers to the unassigned allocnos and
   allocnos conflicting with them or conflicting with allocnos whose
   regno >= START_REGNO.  The function is called after ira_flattening,
   so more allocnos (including ones created in ira-emit.cc) will have a
   chance to get a hard register.  We use simple assignment algorithm
   based on priorities.  */
//原型 ira_reassign_conflict_allocnos ira-int.h ira-color.cc
void mtcs_ira_color_reassign_conflict_allocnos (MtcsIraColor *self,int start_regno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int i, allocnos_to_color_num;
   MtcsIraAllocno * a;
   enum reg_class aclass;
   bitmap allocnos_to_color;
   MtcsIraAllocnoIterator ai;

   allocnos_to_color = ira_allocate_bitmap ();
   allocnos_to_color_num = 0;
   MTCS_FOR_EACH_ALLOCNO(mtcsIraBuild,a, ai){
      int n = a->num_objects;

      if (! a->assigned_p && ! bitmap_bit_p (allocnos_to_color, a->num)){
         if (a->aclass != NO_REGS)
            self->sorted_allocnos[allocnos_to_color_num++] = a;
         else{
            a->assigned_p = true;
            a->hard_regno = -1;
            ira_assert (a->updated_hard_reg_costs == NULL);
            ira_assert (a->updated_conflict_hard_reg_costs == NULL);
         }
         bitmap_set_bit (allocnos_to_color, a->num);
      }
      if (a->regno < start_regno || (aclass = a->aclass) == NO_REGS)
         continue;
      for (i = 0; i < n; i++){
         MtcsIraObject * obj = a->objects[i];
         MtcsIraObject * conflict_obj;
         MtcsIraObjectConflictIterator oci;

         MTCS_FOR_EACH_OBJECT_CONFLICT(mtcsIraBuild, obj, conflict_obj, oci){
            MtcsIraAllocno * conflict_a = conflict_obj->allocno;

            ira_assert (mtcsIra->x_ira_reg_classes_intersect_p[aclass][conflict_a->aclass]);
            if (!bitmap_set_bit (allocnos_to_color, conflict_a->num))
               continue;
            self->sorted_allocnos[allocnos_to_color_num++] = conflict_a;
         }
      }
   }
   ira_free_bitmap (allocnos_to_color);
   if (allocnos_to_color_num > 1){
      setup_allocno_priorities(self,self->sorted_allocnos, allocnos_to_color_num);
      qsort (self->sorted_allocnos, allocnos_to_color_num, sizeof (MtcsIraAllocno *), allocno_priority_compare_func);
   }
   for (i = 0; i < allocnos_to_color_num; i++){
      a = self->sorted_allocnos[i];
      a->assigned_p = false;
      update_curr_costs(self,a);
   }
   for (i = 0; i < allocnos_to_color_num; i++){
      a = self->sorted_allocnos[i];
      if (assign_hard_reg(self,a, true)){
         if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
            fprintf(mtcsIraGlobal->ira_dump_file,"      Secondary allocation: assign hard reg %d to reg %d\n",a->hard_regno, a->regno);
      }
   }
}



/* This page contains functions used to find conflicts using allocno
   live ranges.  */

#ifdef ENABLE_IRA_CHECKING

/* Return TRUE if live ranges of pseudo-registers REGNO1 and REGNO2
   intersect.  This should be used when there is only one region.
   Currently this is used during reload.  */
static bool conflict_by_live_ranges_p (MtcsIraColor *self,int regno1, int regno2)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);
   MtcsIraAllocno * a1, *a2;

   ira_assert (regno1 >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/ && regno2 >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/);
   /* Reg info calculated by dataflow infrastructure can be different
   from one calculated by regclass.  */
   if ((a1 = mtcsIraBuild->ira_loop_tree_root->regno_allocno_map[regno1]) == NULL
         || (a2 = mtcsIraBuild->ira_loop_tree_root->regno_allocno_map[regno2]) == NULL)
      return false;
   return allocnos_conflict_by_live_ranges_p(self,a1, a2);
}

#endif



/* This page contains code to coalesce memory stack slots used by
   spilled allocnos.  This results in smaller stack frame, better data
   locality, and in smaller code for some architectures like
   x86/x86_64 where insn size depends on address displacement value.
   On the other hand, it can worsen insn scheduling after the RA but
   in practice it is less important than smaller stack frames.  */

/* To decrease footprint of ira_allocno structure we store all data
   needed only for coalescing in the following structure.  */
struct _CoalesceData
{
  /* Coalesced allocnos form a cyclic list.  One allocno given by
     FIRST represents all coalesced allocnos.  The
     list is chained by NEXT.  */
  MtcsIraAllocno * first;
  MtcsIraAllocno * next;
  int temp;
};



/* Macro to access the data concerning coalescing.  */
#define ALLOCNO_COALESCE_DATA(a) ((CoalesceData *)a->add_data)

/* Merge two sets of coalesced allocnos given correspondingly by
   allocnos A1 and A2 (more accurately merging A2 set into A1
   set).  */
static void merge_allocnos (MtcsIraColor *self,MtcsIraAllocno * a1, MtcsIraAllocno * a2)
{
   MtcsIraAllocno * a, *first, *last, *next;

   first = ALLOCNO_COALESCE_DATA (a1)->first;
   a = ALLOCNO_COALESCE_DATA (a2)->first;
   if (first == a)
      return;
   for (last = a2, a = ALLOCNO_COALESCE_DATA (a2)->next;;a = ALLOCNO_COALESCE_DATA (a)->next) {
      ALLOCNO_COALESCE_DATA (a)->first = first;
      if (a == a2)
         break;
      last = a;
   }
   next = self->allocno_coalesce_data[first->num].next;
   self->allocno_coalesce_data[first->num].next = a2;
   self->allocno_coalesce_data[last->num].next = next;
}

/* Return TRUE if there are conflicting allocnos from two sets of
   coalesced allocnos given correspondingly by allocnos A1 and A2.  We
   use live ranges to find conflicts because conflicts are represented
   only for allocnos of the same allocno class and during the reload
   pass we coalesce allocnos for sharing stack memory slots.  */
static bool coalesced_allocno_conflict_p (MtcsIraColor *self,MtcsIraAllocno * a1, MtcsIraAllocno * a2)
{
   MtcsIraAllocno * a, *conflict_a;

   if (self->allocno_coalesced_p){
      bitmap_clear (self->processed_coalesced_allocno_bitmap);
      for (a = ALLOCNO_COALESCE_DATA (a1)->next;; a = ALLOCNO_COALESCE_DATA (a)->next){
         bitmap_set_bit (self->processed_coalesced_allocno_bitmap, a->num);
         if (a == a1)
            break;
      }
   }
   for (a = ALLOCNO_COALESCE_DATA (a2)->next;; a = ALLOCNO_COALESCE_DATA (a)->next){
      for (conflict_a = ALLOCNO_COALESCE_DATA (a1)->next;; conflict_a = ALLOCNO_COALESCE_DATA (conflict_a)->next){
         if (allocnos_conflict_by_live_ranges_p(self,a, conflict_a))
            return true;
         if (conflict_a == a1)
            break;
      }
      if (a == a2)
         break;
   }
   return false;
}

/* The major function for aggressive allocno coalescing.  We coalesce
   only spilled allocnos.  If some allocnos have been coalesced, we
   set up flag allocno_coalesced_p.  */
static void coalesce_allocnos (MtcsIraColor *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   MtcsIraAllocno * a;
   MtcsIraAllocnoCopy * cp, *next_cp;
   unsigned int j;
   int i, n, cp_num, regno;
   bitmap_iterator bi;

   cp_num = 0;
   /* Collect copies.  */
   EXECUTE_IF_SET_IN_BITMAP (self->coloring_allocno_bitmap, 0, j, bi){
      a = mtcsIraBuild->ira_allocnos[j];
      regno = a->regno;
      if (! a->assigned_p || a->hard_regno >= 0 || ira_equiv_no_lvalue_p (regno))
         continue;
      for (cp = a->allocno_copies; cp != NULL; cp = next_cp){
         if (cp->first == a){
            next_cp = cp->next_first_allocno_copy;
            regno = cp->second->regno;
            /* For priority coloring we coalesce allocnos only with
            the same allocno class not with intersected allocno
            classes as it were possible.  It is done for
            simplicity.  */
            if ((cp->insn != NULL || cp->constraint_p)
            && cp->second->assigned_p
            && cp->second->hard_regno < 0
            && ! mtcs_ira_equiv_no_lvalue_p/*!ira_equiv_no_lvalue_p*/(mtcsIra,regno))
               self->sorted_copies[cp_num++] = cp;
         }else if (cp->second == a)
            next_cp = cp->next_second_allocno_copy;
         else
            gcc_unreachable ();
      }
   }
   qsort (self->sorted_copies, cp_num, sizeof (MtcsIraAllocnoCopy *), copy_freq_compare_func);
   /* Coalesced copies, most frequently executed first.  */
   for (; cp_num != 0;){
      for (i = 0; i < cp_num; i++){
         cp = self->sorted_copies[i];
         if (! coalesced_allocno_conflict_p(self,cp->first, cp->second)){
            self->allocno_coalesced_p = true;
            if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
               fprintf(mtcsIraGlobal->ira_dump_file,"      Coalescing copy %d:a%dr%d-a%dr%d (freq=%d)\n",
                     cp->num, cp->first->num, cp->first->regno,cp->second->num, cp->second->regno,cp->freq);
            merge_allocnos(self,cp->first, cp->second);
            i++;
            break;
         }
      }
      /* Collect the rest of copies.  */
      for (n = 0; i < cp_num; i++){
         cp = self->sorted_copies[i];
         if (self->allocno_coalesce_data[cp->first->num].first != self->allocno_coalesce_data[cp->second->num].first)
            self->sorted_copies[n++] = cp;
      }
      cp_num = n;
   }
}


/* Sort pseudos according frequencies of coalesced allocno sets they
   belong to (putting most frequently ones first), and according to
   coalesced allocno set order numbers.  */
static int coalesced_pseudo_reg_freq_compare (const void *v1p, const void *v2p)
{
   MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraColor *self=mtcs_ira_mgr_get_color(mtcsIraMgr);

   const int regno1 = *(const int *) v1p;
   const int regno2 = *(const int *) v2p;
   int diff;

   if ((diff = (self->regno_coalesced_allocno_cost[regno2] - self->regno_coalesced_allocno_cost[regno1])) != 0)
      return diff;
   if ((diff = (self->regno_coalesced_allocno_num[regno1] - self->regno_coalesced_allocno_num[regno2])) != 0)
      return diff;
   return regno1 - regno2;
}



/* Sort pseudos according their slot numbers (putting ones with
  smaller numbers first, or last when the frame pointer is not
  needed).  */
static int coalesced_pseudo_reg_slot_compare (const void *v1p, const void *v2p)
{
   MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraColor *self=mtcs_ira_mgr_get_color(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   const int regno1 = *(const int *) v1p;
   const int regno2 = *(const int *) v2p;
   MtcsIraAllocno * a1 = mtcsIraBuild->ira_regno_allocno_map[regno1];
   MtcsIraAllocno * a2 = mtcsIraBuild->ira_regno_allocno_map[regno2];
   int diff, slot_num1, slot_num2;
   machine_mode mode1, mode2;

   if (a1 == NULL || a1->hard_regno >= 0){
      if (a2 == NULL || a2->hard_regno >= 0)
         return regno1 - regno2;
      return 1;
   }else if (a2 == NULL || a2->hard_regno >= 0)
      return -1;
   slot_num1 = -a1->hard_regno;
   slot_num2 = -a2->hard_regno;
   if ((diff = slot_num1 - slot_num2) != 0)
      return (mtcsRtlData->x_frame_pointer_needed/*!frame_pointer_needed*/
            || (!mtcs_func_get_frame_grows_downward/*!FRAME_GROWS_DOWNWARD*/(mtcsFunc))
            == mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc) ? diff : -diff);
   mode1 = mtcs_mode_wider_subreg_mode/*!wider_subreg_mode*/(mtcsMode,
         mtcs_rtl_data_get_pseudo_regno_mode/*!PSEUDO_REGNO_MODE*/(mtcsRtlData,regno1),
   self->regno_max_ref_mode[regno1]);
   mode2 =  mtcs_mode_wider_subreg_mode/*!wider_subreg_mode*/(mtcsMode,
         mtcs_rtl_data_get_pseudo_regno_mode/*!PSEUDO_REGNO_MODE*/(mtcsRtlData,regno2),
   self->regno_max_ref_mode[regno2]);
   if ((diff = compare_sizes_for_sort (mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,mode2),
         mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,mode1))) != 0)
         return diff;
   return regno1 - regno2;
}

/* Setup REGNO_COALESCED_ALLOCNO_COST and REGNO_COALESCED_ALLOCNO_NUM
   for coalesced allocno sets containing allocnos with their regnos
   given in array PSEUDO_REGNOS of length N.  */
static void setup_coalesced_allocno_costs_and_nums (MtcsIraColor *self,int *pseudo_regnos, int n)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);

   int i, num, regno, cost;
   MtcsIraAllocno * allocno, *a;

   for (num = i = 0; i < n; i++){
      regno = pseudo_regnos[i];
      allocno = mtcsIraBuild->ira_regno_allocno_map[regno];
      if (allocno == NULL){
         self->regno_coalesced_allocno_cost[regno] = 0;
         self->regno_coalesced_allocno_num[regno] = ++num;
         continue;
      }
      if (ALLOCNO_COALESCE_DATA (allocno)->first != allocno)
         continue;
      num++;
      for (cost = 0, a = ALLOCNO_COALESCE_DATA (allocno)->next;; a = ALLOCNO_COALESCE_DATA (a)->next){
         cost += a->freq;
         if (a == allocno)
            break;
      }
      for (a = ALLOCNO_COALESCE_DATA (allocno)->next;; a = ALLOCNO_COALESCE_DATA (a)->next){
         self->regno_coalesced_allocno_num[a->regno] = num;
         self->regno_coalesced_allocno_cost[a->regno] = cost;
         if (a == allocno)
            break;
      }
   }
}

/* Collect spilled allocnos representing coalesced allocno sets (the
   first coalesced allocno).  The collected allocnos are returned
   through array SPILLED_COALESCED_ALLOCNOS.  The function returns the
   number of the collected allocnos.  The allocnos are given by their
   regnos in array PSEUDO_REGNOS of length N.  */
static int collect_spilled_coalesced_allocnos (MtcsIraColor *self,int *pseudo_regnos, int n,
                MtcsIraAllocno * *spilled_coalesced_allocnos)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);

   int i, num, regno;
   MtcsIraAllocno * allocno;

   for (num = i = 0; i < n; i++){
      regno = pseudo_regnos[i];
      allocno = mtcsIraBuild->ira_regno_allocno_map[regno];
      if (allocno == NULL || allocno->hard_regno >= 0 || ALLOCNO_COALESCE_DATA (allocno)->first != allocno)
         continue;
      spilled_coalesced_allocnos[num++] = allocno;
   }
   return num;
}

/* Return TRUE if coalesced allocnos represented by ALLOCNO has live
   ranges intersected with live ranges of coalesced allocnos assigned
   to slot with number N.  */
static bool slot_coalesced_allocno_live_ranges_intersect_p (MtcsIraColor *self,MtcsIraAllocno * allocno, int n)
{
   MtcsIraAllocno * a;
   for (a = ALLOCNO_COALESCE_DATA (allocno)->next;; a = ALLOCNO_COALESCE_DATA (a)->next){
      int i;
      int nr = a->num_objects;
      gcc_assert (a->cap_member == NULL);
      for (i = 0; i < nr; i++){
         MtcsIraObject * obj = a->objects[i];
         if (mtcsira_object_live_ranges_intersect_p/*!ira_live_ranges_intersect_p*/(
               self->slot_coalesced_allocnos_live_ranges[n],obj->live_ranges))
            return true;
      }
      if (a == allocno)
         break;
   }
   return false;
}

/* Update live ranges of slot to which coalesced allocnos represented
   by ALLOCNO were assigned.  */
static void setup_slot_coalesced_allocno_live_ranges (MtcsIraColor *self,MtcsIraAllocno * allocno)
{
   int i, n;
   MtcsIraAllocno * a;
   MtcsLiveRange * r;
   n = ALLOCNO_COALESCE_DATA (allocno)->temp;
   for (a = ALLOCNO_COALESCE_DATA (allocno)->next;; a = ALLOCNO_COALESCE_DATA (a)->next) {
      int nr = a->num_objects;
      gcc_assert (a->cap_member == NULL);
      for (i = 0; i < nr; i++){
         MtcsIraObject * obj = a->objects[i];
         r = mtcs_live_range_copy_live_range_list/*!ira_copy_live_range_list*/(obj->live_ranges);
         self->slot_coalesced_allocnos_live_ranges[n]  =
               mtcs_ira_object_merge_live_ranges/*!ira_merge_live_ranges*/(self->slot_coalesced_allocnos_live_ranges[n], r);
      }
      if (a == allocno)
         break;
   }
}

/* We have coalesced allocnos involving in copies.  Coalesce allocnos
   further in order to share the same memory stack slot.  Allocnos
   representing sets of allocnos coalesced before the call are given
   in array SPILLED_COALESCED_ALLOCNOS of length NUM.  Return TRUE if
   some allocnos were coalesced in the function.  */
static bool coalesce_spill_slots (MtcsIraColor *self,MtcsIraAllocno * *spilled_coalesced_allocnos, int num)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int i, j, n, last_coalesced_allocno_num;
   MtcsIraAllocno * allocno, *a;
   bool merged_p = false;
   bitmap set_jump_crosses = regstat_get_setjmp_crosses ();

   self->slot_coalesced_allocnos_live_ranges = (MtcsLiveRange * *) ira_allocate (sizeof (MtcsLiveRange *)
         * mtcsIraBuild->ira_allocnos_num);
   memset (self->slot_coalesced_allocnos_live_ranges, 0, sizeof (MtcsLiveRange *) * mtcsIraBuild->ira_allocnos_num);
   last_coalesced_allocno_num = 0;
   /* Coalesce non-conflicting spilled allocnos preferring most
   frequently used.  */
   for (i = 0; i < num; i++){
      allocno = spilled_coalesced_allocnos[i];
      if (ALLOCNO_COALESCE_DATA (allocno)->first != allocno
      || bitmap_bit_p (set_jump_crosses, allocno->regno)
      || mtcs_ira_equiv_no_lvalue_p/*!ira_equiv_no_lvalue_p*/(mtcsIra,allocno->regno))
         continue;
      for (j = 0; j < i; j++){
         a = spilled_coalesced_allocnos[j];
         n = ALLOCNO_COALESCE_DATA (a)->temp;
         if (ALLOCNO_COALESCE_DATA (a)->first == a
         && ! bitmap_bit_p (set_jump_crosses, a->regno)
         && ! mtcs_ira_equiv_no_lvalue_p/*!ira_equiv_no_lvalue_p*/(mtcsIra,a->regno)
         && ! slot_coalesced_allocno_live_ranges_intersect_p(self,allocno, n))
            break;
      }
      if (j >= i){
         /* No coalescing: set up number for coalesced allocnos
         represented by ALLOCNO.  */
         ALLOCNO_COALESCE_DATA (allocno)->temp = last_coalesced_allocno_num++;
         setup_slot_coalesced_allocno_live_ranges(self,allocno);
      }else{
         self->allocno_coalesced_p = true;
         merged_p = true;
         if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
            fprintf (mtcsIraGlobal->ira_dump_file,"      Coalescing spilled allocnos a%dr%d->a%dr%d\n",
               allocno->num, allocno->regno,a->num, a->regno);
         ALLOCNO_COALESCE_DATA (allocno)->temp = ALLOCNO_COALESCE_DATA (a)->temp;
         setup_slot_coalesced_allocno_live_ranges(self,allocno);
         merge_allocnos(self,a, allocno);
         ira_assert (ALLOCNO_COALESCE_DATA (a)->first == a);
      }
   }
   for (i = 0; i < mtcsIraBuild->ira_allocnos_num; i++)
      mtcs_ira_object_finish_live_range_list/*!ira_finish_live_range_list*/(self->slot_coalesced_allocnos_live_ranges[i]);
   ira_free (self->slot_coalesced_allocnos_live_ranges);
   return merged_p;
}

/* Sort pseudo-register numbers in array PSEUDO_REGNOS of length N for
   subsequent assigning stack slots to them in the reload pass.  To do
   this we coalesce spilled allocnos first to decrease the number of
   memory-memory move insns.  This function is called by the
   reload.  */
//原型 ira_sort_regnos_for_alter_reg ira.h ira-color.cc
void mtcs_ira_color_sort_regnos_for_alter_reg (MtcsIraColor *self,int *pseudo_regnos, int n,
                machine_mode *reg_max_ref_mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int max_regno = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);
   int i, regno, num, slot_num;
   MtcsIraAllocno * allocno, *a;
   MtcsIraAllocnoIterator ai;
   MtcsIraAllocno * *spilled_coalesced_allocnos;

   ira_assert (! mtcsIra->ira_use_lra_p);

   /* Set up allocnos can be coalesced.  */
   self->coloring_allocno_bitmap = ira_allocate_bitmap ();
   for (i = 0; i < n; i++){
      regno = pseudo_regnos[i];
      allocno = mtcsIraBuild->ira_regno_allocno_map[regno];
      if (allocno != NULL)
         bitmap_set_bit (self->coloring_allocno_bitmap, allocno->num);
   }
   self->allocno_coalesced_p = false;
   self->processed_coalesced_allocno_bitmap = ira_allocate_bitmap ();
   self->allocno_coalesce_data = (CoalesceData *) ira_allocate (sizeof (CoalesceData) * mtcsIraBuild->ira_allocnos_num);
   /* Initialize coalesce data for allocnos.  */
   MTCS_FOR_EACH_ALLOCNO(mtcsIraBuild,a, ai){
      a->add_data = self->allocno_coalesce_data + a->num;
      ALLOCNO_COALESCE_DATA (a)->first = a;
      ALLOCNO_COALESCE_DATA (a)->next = a;
   }
   coalesce_allocnos(self);
   ira_free_bitmap (self->coloring_allocno_bitmap);
   self->regno_coalesced_allocno_cost = (int *) ira_allocate (max_regno * sizeof (int));
   self->regno_coalesced_allocno_num  = (int *) ira_allocate (max_regno * sizeof (int));
   memset (self->regno_coalesced_allocno_num, 0, max_regno * sizeof (int));
   setup_coalesced_allocno_costs_and_nums(self,pseudo_regnos, n);
   /* Sort regnos according frequencies of the corresponding coalesced
   allocno sets.  */
   qsort (pseudo_regnos, n, sizeof (int), coalesced_pseudo_reg_freq_compare);
   spilled_coalesced_allocnos = (MtcsIraAllocno * *) ira_allocate (mtcsIraBuild->ira_allocnos_num * sizeof (MtcsIraAllocno *));
   /* Collect allocnos representing the spilled coalesced allocno
   sets.  */
   num = collect_spilled_coalesced_allocnos(self,pseudo_regnos, n,  spilled_coalesced_allocnos);
   if (mtcsOptionsItem->x_flag_ira_share_spill_slots
   && coalesce_spill_slots(self,spilled_coalesced_allocnos, num)){
      setup_coalesced_allocno_costs_and_nums(self,pseudo_regnos, n);
      qsort (pseudo_regnos, n, sizeof (int),coalesced_pseudo_reg_freq_compare);
      num = collect_spilled_coalesced_allocnos(self,pseudo_regnos, n,spilled_coalesced_allocnos);
   }
   ira_free_bitmap (self->processed_coalesced_allocno_bitmap);
   self->allocno_coalesced_p = false;
   /* Assign stack slot numbers to spilled allocno sets, use smaller
   numbers for most frequently used coalesced allocnos.  -1 is
   reserved for dynamic search of stack slots for pseudos spilled by
   the reload.  */
   slot_num = 1;
   for (i = 0; i < num; i++){
      allocno = spilled_coalesced_allocnos[i];
      if (ALLOCNO_COALESCE_DATA (allocno)->first != allocno
      || allocno->hard_regno >= 0
      || mtcs_ira_equiv_no_lvalue_p/*!ira_equiv_no_lvalue_p*/(mtcsIra,allocno->regno))
         continue;
      if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
         fprintf (mtcsIraGlobal->ira_dump_file, "      Slot %d (freq,size):", slot_num);
      slot_num++;
      for (a = ALLOCNO_COALESCE_DATA (allocno)->next;; a = ALLOCNO_COALESCE_DATA (a)->next){
         ira_assert (a->hard_regno < 0);
         a->hard_regno = -slot_num;
         if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL){
            machine_mode mode = mtcs_mode_wider_subreg_mode/*!wider_subreg_mode*/(mtcsMode,
                  mtcs_rtl_data_get_pseudo_regno_mode/*!PSEUDO_REGNO_MODE*/(mtcsRtlData,a->regno),reg_max_ref_mode[a->regno]);
            fprintf (mtcsIraGlobal->ira_dump_file, " a%dr%d(%d,",
            a->num, a->regno, a->freq);
            print_dec (GET_MODE_SIZE (mode), mtcsIraGlobal->ira_dump_file, SIGNED);
            fprintf (mtcsIraGlobal->ira_dump_file, ")\n");
         }

         if (a == allocno)
            break;
      }
      if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
         fprintf (mtcsIraGlobal->ira_dump_file, "\n");
   }
   self->ira_spilled_reg_stack_slots_num = slot_num - 1;
   ira_free (spilled_coalesced_allocnos);
   /* Sort regnos according the slot numbers.  */
   self->regno_max_ref_mode = reg_max_ref_mode;
   qsort (pseudo_regnos, n, sizeof (int), coalesced_pseudo_reg_slot_compare);
   MTCS_FOR_EACH_ALLOCNO(mtcsIraBuild,a, ai)
      a->add_data = NULL;
   ira_free (self->allocno_coalesce_data);
   ira_free (self->regno_coalesced_allocno_num);
   ira_free (self->regno_coalesced_allocno_cost);
}



/* This page contains code used by the reload pass to improve the
   final code.  */

/* The function is called from reload to mark changes in the
   allocation of REGNO made by the reload.  Remember that reg_renumber
   reflects the change result.  */
//原型 ira_mark_allocation_change ira.h ira-color.cc
void mtcs_ira_color_mark_allocation_change (MtcsIraColor *self,int regno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);

   MtcsIraAllocno * a = mtcsIraBuild->ira_regno_allocno_map[regno];
   int old_hard_regno, hard_regno, cost;
   enum reg_class aclass = a->aclass;

   ira_assert (a != NULL);
   hard_regno = reg_renumber[regno];
   if ((old_hard_regno = a->hard_regno) == hard_regno)
      return;
   if (old_hard_regno < 0)
      cost = -a->memory_cost;
   else{
      ira_assert (mtcsIraInt->x_ira_class_hard_reg_index[aclass][old_hard_regno] >= 0);
      cost = -(a->hard_reg_costs == NULL
            ? a->class_cost: a->hard_reg_costs[mtcsIraInt->x_ira_class_hard_reg_index[aclass][old_hard_regno]]);
      update_costs_from_copies(self,a, false, false);
   }
   mtcsIra->ira_overall_cost -= cost;
   a->hard_regno = hard_regno;
   if (hard_regno < 0){
      a->hard_regno = -1;
      cost += a->memory_cost;
   }else if (mtcsIraInt->x_ira_class_hard_reg_index[aclass][hard_regno] >= 0){
      cost += (a->hard_reg_costs == NULL
            ? a->class_cost : a->hard_reg_costs[mtcsIraInt->x_ira_class_hard_reg_index[aclass][hard_regno]]);
      update_costs_from_copies(self,a, true, false);
   }else
      /* Reload changed class of the allocno.  */
      cost = 0;
   mtcsIra->ira_overall_cost += cost;
}

/* This function is called when reload deletes memory-memory move.  In
   this case we marks that the allocation of the corresponding
   allocnos should be not changed in future.  Otherwise we risk to get
   a wrong code.  */
//原型 ira_mark_memory_move_deletion ira.h ira-color.cc
void mtcs_ira_color_mark_memory_move_deletion (MtcsIraColor *self,int dst_regno, int src_regno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);

   MtcsIraAllocno * dst = mtcsIraBuild->ira_regno_allocno_map[dst_regno];
   MtcsIraAllocno * src = mtcsIraBuild->ira_regno_allocno_map[src_regno];

   ira_assert (dst != NULL && src != NULL && dst->hard_regno < 0 && src->hard_regno < 0);
   dst->dont_reassign_p = true;
   src->dont_reassign_p = true;
}

/* Try to assign a hard register (except for FORBIDDEN_REGS) to
   allocno A and return TRUE in the case of success.  */
static bool allocno_reload_assign (MtcsIraColor *self,MtcsIraAllocno * a, HardRegSet *forbidden_regs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int hard_regno;
   enum reg_class aclass;
   int regno = a->regno;
   HardRegSet saved[2];
   saved[0].count=mtcs_reg_get_hard_reg_element_count(mtcsReg);
   saved[1].count=mtcs_reg_get_hard_reg_element_count(mtcsReg);
   int i, n;

   n = a->num_objects;
   for (i = 0; i < n; i++){
      MtcsIraObject * obj = a->objects[i];
      saved[i] = obj->total_conflict_hard_regs;
      obj->total_conflict_hard_regs |= *forbidden_regs;
      if (! mtcsOptionsItem->x_flag_caller_saves && a->calls_crossed_num != 0)
         obj->total_conflict_hard_regs |= mtcs_ira_allocno_need_caller_save_regs/*!ira_need_caller_save_regs*/(a);
   }
   a->assigned_p = false;
   aclass = a->aclass;
   update_curr_costs(self,a);
   assign_hard_reg(self,a, true);
   hard_regno = a->hard_regno;
   reg_renumber[regno] = hard_regno;
   if (hard_regno < 0)
      a->hard_regno = -1;
   else{
      ira_assert (mtcsIraInt->x_ira_class_hard_reg_index[aclass][hard_regno] >= 0);
      mtcsIra->ira_overall_cost -= (a->memory_cost - (a->hard_reg_costs == NULL
            ? a->class_cost : a->hard_reg_costs[mtcsIraInt->x_ira_class_hard_reg_index[aclass][hard_regno]]));
      if (mtcs_ira_allocno_need_caller_save_p/*!ira_need_caller_save_p*/(a, hard_regno)){
         ira_assert (mtcsOptionsItem->x_flag_caller_saves);
         caller_save_needed = 1;
      }
   }

   /* If we found a hard register, modify the RTL for the pseudo
   register to show the hard register, and mark the pseudo register
   live.  */
   if (reg_renumber[regno] >= 0){
      if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
         fprintf (mtcsIraGlobal->ira_dump_file, ": reassign to %d\n", reg_renumber[regno]);
      mtcs_dfscan_df_ref_change_reg_with_loc/*!SET_REGNO*/(mtcsDfscan,mtcsRtlData->regno_reg_rtx/*! regno_reg_rtx*/[regno],
            reg_renumber[regno]);
      mark_home_live (regno);
   }else if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
      fprintf (mtcsIraGlobal->ira_dump_file, "\n");

   for (i = 0; i < n; i++){
      MtcsIraObject * obj = a->objects[i];
      obj->total_conflict_hard_regs = saved[i];
   }
   return reg_renumber[regno] >= 0;
}

/* Sort pseudos according their usage frequencies (putting most
   frequently ones first).  */
static int pseudo_reg_compare (const void *v1p, const void *v2p)
{
   int regno1 = *(const int *) v1p;
   int regno2 = *(const int *) v2p;
   int diff;

   if ((diff = REG_FREQ (regno2) - REG_FREQ (regno1)) != 0)
      return diff;
   return regno1 - regno2;
}

/* Try to allocate hard registers to SPILLED_PSEUDO_REGS (there are
   NUM of them) or spilled pseudos conflicting with pseudos in
   SPILLED_PSEUDO_REGS.  Return TRUE and update SPILLED, if the
   allocation has been changed.  The function doesn't use
   BAD_SPILL_REGS and hard registers in PSEUDO_FORBIDDEN_REGS and
   PSEUDO_PREVIOUS_REGS for the corresponding pseudos.  The function
   is called by the reload pass at the end of each reload
   iteration.  */
//原型 ira_reassign_pseudos ira.h ira-color.cc
bool mtcs_ira_color_reassign_pseudos (MtcsIraColor *self,int *spilled_pseudo_regs, int num,
            HardRegSet bad_spill_regs,
            HardRegSet *pseudo_forbidden_regs,
            HardRegSet *pseudo_previous_regs,
            bitmap spilled)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int i, n, regno;
   bool changed_p;
   MtcsIraAllocno * a;
   HardRegSet forbidden_regs = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};

   bitmap temp = BITMAP_ALLOC (NULL);

   /* Add pseudos which conflict with pseudos already in
   SPILLED_PSEUDO_REGS to SPILLED_PSEUDO_REGS.  This is preferable
   to allocating in two steps as some of the conflicts might have
   a higher priority than the pseudos passed in SPILLED_PSEUDO_REGS.  */
   for (i = 0; i < num; i++)
      bitmap_set_bit (temp, spilled_pseudo_regs[i]);

   for (i = 0, n = num; i < n; i++){
      int nr, j;
      int regno = spilled_pseudo_regs[i];
      bitmap_set_bit (temp, regno);

      a = mtcsIraBuild->ira_regno_allocno_map[regno];
      nr = a->num_objects;
      for (j = 0; j < nr; j++){
         MtcsIraObject * conflict_obj;
         MtcsIraObject * obj =a->objects[j];
         MtcsIraObjectConflictIterator oci;

         MTCS_FOR_EACH_OBJECT_CONFLICT(mtcsIraBuild, obj, conflict_obj, oci){
            MtcsIraAllocno * conflict_a = conflict_obj->allocno;
            if (conflict_a->hard_regno < 0   && ! conflict_a->dont_reassign_p && bitmap_set_bit (temp, conflict_a->regno)){
               spilled_pseudo_regs[num++] = conflict_a->regno;
               /* ?!? This seems wrong.  */
               bitmap_set_bit (self->consideration_allocno_bitmap,conflict_a->num);
            }
         }
      }
   }

   if (num > 1)
      qsort (spilled_pseudo_regs, num, sizeof (int), pseudo_reg_compare);
   changed_p = false;
   /* Try to assign hard registers to pseudos from
   SPILLED_PSEUDO_REGS.  */
   for (i = 0; i < num; i++){
      regno = spilled_pseudo_regs[i];
      forbidden_regs = (bad_spill_regs | pseudo_forbidden_regs[regno] | pseudo_previous_regs[regno]);
      gcc_assert (reg_renumber[regno] < 0);
      a = mtcsIraBuild->ira_regno_allocno_map[regno];
      mtcs_ira_color_mark_allocation_change/*!ira_mark_allocation_change*/(self,regno);
      ira_assert (reg_renumber[regno] < 0);
      if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
         fprintf (mtcsIraGlobal->ira_dump_file, "      Try Assign %d(a%d), cost=%d", regno, a->num, a->memory_cost  - a->class_cost);
      allocno_reload_assign(self,a, &forbidden_regs);
      if (reg_renumber[regno] >= 0){
         CLEAR_REGNO_REG_SET (spilled, regno);
         changed_p = true;
      }
   }
   BITMAP_FREE (temp);
   return changed_p;
}

/* The function is called by reload and returns already allocated
   stack slot (if any) for REGNO with given INHERENT_SIZE and
   TOTAL_SIZE.  In the case of failure to find a slot which can be
   used for REGNO, the function returns NULL.  */
//原型 ira_reuse_stack_slot ira-h ira-color.cc
rtx mtcs_ira_color_reuse_stack_slot (MtcsIraColor *self,int regno, poly_uint64 inherent_size,
            poly_uint64 total_size)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   unsigned int i;
   int slot_num, best_slot_num;
   int cost, best_cost;
   MtcsIraAllocnoCopy * cp, *next_cp;
   MtcsIraAllocno * another_allocno, *allocno = mtcsIraBuild->ira_regno_allocno_map[regno];
   rtx x;
   bitmap_iterator bi;
   class ira_spilled_reg_stack_slot *slot = NULL;

   ira_assert (! mtcsIra->ira_use_lra_p);

   ira_assert (known_eq (inherent_size, mtcs_reg_get_pseudo_regno_bytes/*!PSEUDO_REGNO_BYTES*/(mtcsReg,regno))
         && known_le (inherent_size, total_size)  && allocno->hard_regno < 0);
   if (! mtcsOptionsItem->x_flag_ira_share_spill_slots)
      return NULL_RTX;
   slot_num = -allocno->hard_regno - 2;
   if (slot_num != -1){
      slot = &self->ira_spilled_reg_stack_slots[slot_num];
      x = slot->mem;
   }else{
      best_cost = best_slot_num = -1;
      x = NULL_RTX;
      /* It means that the pseudo was spilled in the reload pass, try
      to reuse a slot.  */
      for (slot_num = 0; slot_num < self->ira_spilled_reg_stack_slots_num;slot_num++){
         slot = &self->ira_spilled_reg_stack_slots[slot_num];
         if (slot->mem == NULL_RTX)
            continue;
         if (maybe_lt (slot->width, total_size)
         || maybe_lt (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (slot->mem)), inherent_size))
            continue;

         EXECUTE_IF_SET_IN_BITMAP (&slot->spilled_regs, firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/, i, bi){
            another_allocno = mtcsIraBuild->ira_regno_allocno_map[i];
            if (allocnos_conflict_by_live_ranges_p(self,allocno, another_allocno))
               goto cont;
         }
         for (cost = 0, cp = allocno->allocno_copies; cp != NULL; cp = next_cp){
            if (cp->first == allocno){
               next_cp = cp->next_first_allocno_copy;
               another_allocno = cp->second;
            }else if (cp->second == allocno){
               next_cp = cp->next_second_allocno_copy;
               another_allocno = cp->first;
            }else
               gcc_unreachable ();
            if (cp->insn == NULL_RTX)
               continue;
            if (bitmap_bit_p (&slot->spilled_regs, another_allocno->regno))
               cost += cp->freq;
         }
         if (cost > best_cost){
            best_cost = cost;
            best_slot_num = slot_num;
         }
cont:
         ;
      }
      if (best_cost >= 0){
         slot_num = best_slot_num;
         slot = &self->ira_spilled_reg_stack_slots[slot_num];
         SET_REGNO_REG_SET (&slot->spilled_regs, regno);
         x = slot->mem;
         allocno->hard_regno = -slot_num - 2;
      }
   }

   if (x != NULL_RTX){
      ira_assert (known_ge (slot->width, total_size));
#ifdef ENABLE_IRA_CHECKING
      EXECUTE_IF_SET_IN_BITMAP (&slot->spilled_regs,firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/, i, bi){
         ira_assert (! conflict_by_live_ranges_p(self,regno, i));
      }
#endif
      SET_REGNO_REG_SET (&slot->spilled_regs, regno);
      if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file){
         fprintf (mtcsIraGlobal->ira_dump_file, "      Assigning %d(freq=%d) slot %d of",regno, REG_FREQ (regno), slot_num);
         EXECUTE_IF_SET_IN_BITMAP (&slot->spilled_regs, firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/, i, bi){
            if ((unsigned) regno != i)
               fprintf (mtcsIraGlobal->ira_dump_file, " %d", i);
         }
         fprintf (mtcsIraGlobal->ira_dump_file, "\n");
      }
   }
   return x;
}

/* This is called by reload every time a new stack slot X with
   TOTAL_SIZE was allocated for REGNO.  We store this info for
   subsequent ira_reuse_stack_slot calls.  */
//原型 ira_mark_new_stack_slot ira.h ira-color.cc
void mtcs_ira_color_mark_new_stack_slot (MtcsIraColor *self,rtx x, int regno, poly_uint64 total_size)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   class ira_spilled_reg_stack_slot *slot;
   int slot_num;
   MtcsIraAllocno * allocno;

   ira_assert (! mtcsIra->ira_use_lra_p);

   ira_assert (known_le (mtcs_reg_get_pseudo_regno_bytes/*!PSEUDO_REGNO_BYTES*/(mtcsReg,regno), total_size));
   allocno = mtcsIraBuild->ira_regno_allocno_map[regno];
   slot_num = -allocno->hard_regno - 2;
   if (slot_num == -1){
      slot_num = self->ira_spilled_reg_stack_slots_num++;
      allocno->hard_regno = -slot_num - 2;
   }
   slot = &self->ira_spilled_reg_stack_slots[slot_num];
   INIT_REG_SET (&slot->spilled_regs);
   SET_REGNO_REG_SET (&slot->spilled_regs, regno);
   slot->mem = x;
   slot->width = total_size;
   if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file)
      fprintf (mtcsIraGlobal->ira_dump_file, "      Assigning %d(freq=%d) a new slot %d\n",regno, REG_FREQ (regno), slot_num);
}


/* Return spill cost for pseudo-registers whose numbers are in array
   REGNOS (with a negative number as an end marker) for reload with
   given IN and OUT for INSN.  Return also number points (through
   EXCESS_PRESSURE_LIVE_LENGTH) where the pseudo-register lives and
   the register pressure is high, number of references of the
   pseudo-registers (through NREFS), the number of psuedo registers
   whose allocated register wouldn't need saving in the prologue
   (through CALL_USED_COUNT), and the first hard regno occupied by the
   pseudo-registers (through FIRST_HARD_REGNO).  */
static int calculate_spill_cost (MtcsIraColor *self,int *regnos, rtx in, rtx out, rtx_insn *insn,
            int *excess_pressure_live_length,
            int *nrefs, int *call_used_count, int *first_hard_regno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int i, cost, regno, hard_regno, count, saved_cost;
   bool in_p, out_p;
   int length;
   MtcsIraAllocno * a;

   *nrefs = 0;
   for (length = count = cost = i = 0;; i++){
      regno = regnos[i];
      if (regno < 0)
         break;
      *nrefs += REG_N_REFS (regno);
      hard_regno = reg_renumber[regno];
      ira_assert (hard_regno >= 0);
      a = mtcsIraBuild->ira_regno_allocno_map[regno];
      length += a->excess_pressure_points_num / a->num_objects;
      cost += a->memory_cost - a->class_cost;
      if (mtcs_reg_in_hard_reg_set_p/*!in_hard_reg_set_p*/(mtcsReg,&mtcsRtlData/*!crtl*/->abi->full_reg_clobbers (),a->mode, hard_regno))
         count++;
      in_p = in && REG_P (in) && (int) REGNO (in) == hard_regno;
      out_p = out && REG_P (out) && (int) REGNO (out) == hard_regno;
      if ((in_p || out_p) && find_regno_note (insn, REG_DEAD, hard_regno) != NULL_RTX){
         saved_cost = 0;
         if (in_p)
            saved_cost += mtcsIra->x_ira_memory_move_cost[a->mode][a->aclass][1];
         if (out_p)
            saved_cost += mtcsIra->x_ira_memory_move_cost[a->mode][a->aclass][0];
         cost -= REG_FREQ_FROM_BB (BLOCK_FOR_INSN (insn)) * saved_cost;
      }
   }
   *excess_pressure_live_length = length;
   *call_used_count = count;
   hard_regno = -1;
   if (regnos[0] >= 0){
      hard_regno = reg_renumber[regnos[0]];
   }
   *first_hard_regno = hard_regno;
   return cost;
}

/* Return TRUE if spilling pseudo-registers whose numbers are in array
   REGNOS is better than spilling pseudo-registers with numbers in
   OTHER_REGNOS for reload with given IN and OUT for INSN.  The
   function used by the reload pass to make better register spilling
   decisions.  */
//原型 ira_better_spill_reload_regno_p ira.h ira-color.cc
bool mtcs_ira_color_better_spill_reload_regno_p (MtcsIraColor *self,int *regnos, int *other_regnos,
             rtx in, rtx out, rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);

   int cost, other_cost;
   int length, other_length;
   int nrefs, other_nrefs;
   int call_used_count, other_call_used_count;
   int hard_regno, other_hard_regno;

   cost = calculate_spill_cost(self,regnos, in, out, insn, &length, &nrefs, &call_used_count, &hard_regno);
   other_cost = calculate_spill_cost(self,other_regnos, in, out, insn,&other_length, &other_nrefs,
         &other_call_used_count,&other_hard_regno);
   if (nrefs == 0 && other_nrefs != 0)
      return true;
   if (nrefs != 0 && other_nrefs == 0)
      return false;
   if (cost != other_cost)
      return cost < other_cost;
   if (length != other_length)
      return length > other_length;
   if(mtcs_config_ifdef(mtcsConfig,MTCS_STACK_REGS)){
// #ifdef REG_ALLOC_ORDER
      if (hard_regno >= 0 && other_hard_regno >= 0)
         return (mtcsReg->hardRegs.x_inv_reg_alloc_order/*!inv_reg_alloc_order*/[hard_regno]
                                       < mtcsReg->hardRegs.x_inv_reg_alloc_order/*!inv_reg_alloc_order*/[other_hard_regno]);
//#else
   }else{
      if (call_used_count != other_call_used_count)
         return call_used_count > other_call_used_count;
//#endif
   }
   return false;
}

/* Allocate and initialize data necessary for assign_hard_reg.  */
//原型 ira_initiate_assign ira-int.h ira-color.cc
void mtcs_ira_color_initiate_assign (MtcsIraColor *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);

   self->sorted_allocnos = (MtcsIraAllocno * *) ira_allocate (sizeof (MtcsIraAllocno *) * mtcsIraBuild->ira_allocnos_num);
   self->consideration_allocno_bitmap = ira_allocate_bitmap ();
   initiate_cost_update(self);
   self->allocno_priorities = (int *) ira_allocate (sizeof (int) * mtcsIraBuild->ira_allocnos_num);
   self->sorted_copies = (MtcsIraAllocnoCopy * *) ira_allocate (mtcsIraBuild->ira_copies_num * sizeof (MtcsIraAllocnoCopy *));
}

/* Deallocate data used by assign_hard_reg.  */
//原型 ira_finish_assign ira-int.h ira-color.cc
void mtcs_ira_color_finish_assign (MtcsIraColor *self)
{
  ira_free (self->sorted_allocnos);
  ira_free_bitmap (self->consideration_allocno_bitmap);
  finish_cost_update(self);
  ira_free (self->allocno_priorities);
  ira_free (self->sorted_copies);
}

/* Entry function doing color-based register allocation.  */
static void color (MtcsIraColor *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);

   self->allocno_stack_vec.create (mtcsIraBuild->ira_allocnos_num);
   memset (self->allocated_hardreg_p, 0, sizeof (self->allocated_hardreg_p));
   mtcs_ira_color_initiate_assign/*!ira_initiate_assign*/(self);
   do_coloring(self);
   mtcs_ira_color_finish_assign/*!ira_finish_assign*/(self);
   self->allocno_stack_vec.release ();
   move_spill_restore(self);
}

/* This page contains a simple register allocator without usage of
   allocno conflicts.  This is used for fast allocation for -O0.  */

/* Do register allocation by not using allocno conflicts.  It uses
   only allocno live ranges.  The algorithm is close to Chow's
   priority coloring.  */
static void fast_allocation (MtcsIraColor *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraLives *mtcsIraLives =mtcs_ira_mgr_get_lives(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int i, j, k, num, class_size, hard_regno, best_hard_regno, cost, min_cost;
   int *costs;
   //#ifdef STACK_REGS
   bool no_stack_reg_p;
   //#endif
   enum reg_class aclass;
   machine_mode mode;
   MtcsIraAllocno * a;
   MtcsIraAllocnoIterator ai;
   MtcsLiveRange * r;
   HardRegSet /*!HARD_REG_SET*/ conflict_hard_regs = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};
   HardRegSet /*!HARD_REG_SET*/ *used_hard_regs;

   self->sorted_allocnos = (MtcsIraAllocno * *) ira_allocate (sizeof (MtcsIraAllocno *) * mtcsIraBuild->ira_allocnos_num);
   num = 0;
   MTCS_FOR_EACH_ALLOCNO(mtcsIraBuild,a, ai)
      self->sorted_allocnos[num++] = a;
   self->allocno_priorities = (int *) ira_allocate (sizeof (int) * mtcsIraBuild->ira_allocnos_num);
   setup_allocno_priorities(self,self->sorted_allocnos, num);
   used_hard_regs = (HardRegSet *) ira_allocate (sizeof (HardRegSet) * mtcsIraLives->ira_max_point);
   for (i = 0; i < mtcsIraLives->ira_max_point; i++){
      used_hard_regs[i].count=mtcs_reg_get_hard_reg_element_count(mtcsReg);
      mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&used_hard_regs[i]);
   }
   qsort (self->sorted_allocnos, num, sizeof (MtcsIraAllocno *),allocno_priority_compare_func);
   for (i = 0; i < num; i++){
      int nr, l;

      a = self->sorted_allocnos[i];
      nr = a->num_objects;
      mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&conflict_hard_regs);
      for (l = 0; l < nr; l++){
         MtcsIraObject * obj = a->objects[l];
         conflict_hard_regs |= obj->conflict_hard_regs;
         for (r = obj->live_ranges; r != NULL; r = r->next)
            for (j = r->start; j <= r->finish; j++)
               conflict_hard_regs |= used_hard_regs[j];
      }
      aclass = a->aclass;
      a->assigned_p = true;
      a->hard_regno = -1;
      if (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[aclass],
            &conflict_hard_regs))
         continue;
      mode = a->mode;
      if(mtcs_config_ifdef(mtcsConfig,MTCS_STACK_REGS))
         no_stack_reg_p = a->no_stack_reg_p;
      /*!
      #ifdef STACK_REGS
      no_stack_reg_p = ALLOCNO_NO_STACK_REG_P (a);
      #endif
      */
      class_size = mtcsIra->x_ira_class_hard_regs_num[aclass];
      costs = a->hard_reg_costs;
      min_cost = INT_MAX;
      best_hard_regno = -1;
      for (j = 0; j < class_size; j++){
         hard_regno = mtcsIra->x_ira_class_hard_regs[aclass][j];
         if(mtcs_config_ifdef(mtcsConfig,MTCS_STACK_REGS)){
            if (no_stack_reg_p && mtcs_reg_get_first_stack_reg/*!FIRST_STACK_REG*/(mtcsReg)
            <= hard_regno && hard_regno <= mtcs_reg_get_last_stack_reg/*!LAST_STACK_REG*/(mtcsReg))
               continue;
         }
         /*
         #ifdef STACK_REGS
         if (no_stack_reg_p && FIRST_STACK_REG <= hard_regno
         && hard_regno <= LAST_STACK_REG)
         continue;
         #endif
         */
         if (mtcs_ira_hard_reg_set_intersection_p/*!ira_hard_reg_set_intersection_p*/(mtcsIra,
               hard_regno, mode, &conflict_hard_regs)
         || (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(
         &mtcsIra->x_ira_prohibited_class_mode_regs[aclass][mode], hard_regno)))
            continue;
         if (NUM_REGISTER_FILTERS && !test_register_filters (a->register_filters,hard_regno))
            continue;
         if (costs == NULL){
            best_hard_regno = hard_regno;
            break;
         }
         cost = costs[j];
         if (min_cost > cost){
            min_cost = cost;
            best_hard_regno = hard_regno;
         }
      }
      if (best_hard_regno < 0)
         continue;
      a->hard_regno = hard_regno = best_hard_regno;
      for (l = 0; l < nr; l++){
         MtcsIraObject * obj = a->objects[l];
         for (r = obj->live_ranges; r != NULL; r = r->next)
            for (k = r->start; k <= r->finish; k++)
               used_hard_regs[k] |= mtcsIraInt->x_ira_reg_mode_hard_regset[hard_regno][mode];
      }
   }
   ira_free (self->sorted_allocnos);
   ira_free (used_hard_regs);
   ira_free (self->allocno_priorities);
   if (mtcsIraGlobal->internal_flag_ira_verbose > 1 && mtcsIraGlobal->ira_dump_file != NULL)
      mtcs_ira_print_disposition/*!ira_print_disposition*/(mtcsIra,mtcsIraGlobal->ira_dump_file);
}

/* Entry function doing coloring.  */
//原型 ira_color ira-int.h ira-color.cc
void mtc_ira_color_color (MtcsIraColor *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild =mtcs_ira_mgr_get_build(mtcsIraMgr);

   MtcsIraAllocno * a;
   MtcsIraAllocnoIterator ai;
   /* Setup updated costs.  */
   MTCS_FOR_EACH_ALLOCNO(mtcsIraBuild,a, ai){
      a->updated_memory_cost = a->memory_cost;
      a->updated_class_cost = a->class_cost;
   }
   if (mtcsIra->ira_conflicts_p)
      color(self);
   else
      fast_allocation(self);
}

static void mtcsIraColorInit(MtcsIraColor *self)
{
   self->max_soft_conflict_loop_depth = 64;
}


MtcsIraColor *mtcs_ira_color_new(MtcsMode *mtcsMode)
{
   MtcsIraColor *self = n_slice_alloc0 (sizeof(MtcsIraColor));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsIraColorInit(self);
   return self;
}

