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

#ifndef __GCC_MTCS_IRA_COSTS__
#define __GCC_MTCS_IRA_COSTS__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "../mtcsmicro.h"
#include "mtcsiraobject.h"

struct cost_classes_hasher;

/* The `costs' struct records the cost of using hard registers of each
   class considered for the calculation and of using memory for each
   allocno or pseudo.  */
//原型 struct costs ira-costs.cc
typedef struct _IraCosts
{
  int mem_cost;
  /* Costs for register classes start here.  We process only some
     allocno classes.  */
  int cost[1];
}IraCosts;

/* Info about reg classes whose costs are calculated for a pseudo.  */
//原型 struct cost_classes ira-costs.cc
typedef struct _CostClasses
{
  /* Number of the cost classes in the subsequent array.  */
  int num;
  /* Container of the cost classes.  */
  enum reg_class classes[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];
  /* Map reg class -> index of the reg class in the previous array.
     -1 if it is not a cost class.  */
  int index[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];
  /* Map hard regno index of first class in array CLASSES containing
     the hard regno, -1 otherwise.  */
  int hard_regno_index[MAX_FIRST_PSEUDO_REGISTER/*!FIRST_PSEUDO_REGISTER*/];
}CostClasses;

typedef struct _MtcsIraCosts  MtcsIraCosts;
struct _MtcsIraCosts
{
  MtcsComponent parent;
  /* The flags is set up every time when we calculate pseudo register
     classes through function ira_set_pseudo_classes.  */
  //原型 pseudo_classes_defined_p ira-costs.cc
  bool pseudo_classes_defined_p;;

  /* TRUE if we work with allocnos.  Otherwise we work with pseudos.  */
  //原型 allocno_p ira-costs.cc
  bool allocno_p;
  /* Number of elements in array `costs'.  */
  //原型 cost_elements_num ira-costs.cc
  int cost_elements_num;
  /* Costs of each class for each allocno or pseudo.  */
  //原型 costs ira-costs.cc
   IraCosts *costs;
  /* Accumulated costs of each class for each allocno.  */
  //原型 total_allocno_costs ira-costs.cc
   IraCosts *total_allocno_costs;
  /* It is the current size of struct costs.  */
  //原型 struct_costs_size ira-costs.cc
   size_t struct_costs_size;
   /* Record register class preferences of each allocno or pseudo.  Null
      value means no preferences.  It happens on the 1st iteration of the
      cost calculation.  */
   //原型 pref ira-costs.cc
   enum reg_class *pref;
   /* Allocated buffers for pref.  */
   //原型 pref_buffer ira-costs.cc
   enum reg_class *pref_buffer;
   /* Record allocno class of each allocno with the same regno.  */
   //原型 regno_aclass ira-costs.cc
   enum reg_class *regno_aclass;
   /* Record cost gains for not allocating a register with an invariant
      equivalence.  */
   //原型 regno_equiv_gains ira-costs.cc
   int *regno_equiv_gains;
   /* Execution frequency of the current insn.  */
   //原型 frequency ira-costs.cc
   int frequency;
   /* Info about cost classes for each pseudo.  */
   //原型 regno_cost_classes ira-costs.cc
   CostClasses * *regno_cost_classes;
   /* Hash table of unique cost classes.  */
   //原型 cost_classes_htab ira-costs.cc
   hash_table<cost_classes_hasher> *cost_classes_htab;
   /* Map allocno class -> cost classes for pseudo of given allocno
      class.  */
   //原型 cost_classes_aclass_cache ira-costs.cc
   CostClasses * cost_classes_aclass_cache[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];
   /* Map mode -> cost classes for pseudo of give mode.  */
   //原型 cost_classes_mode_cache ira-costs.cc
   CostClasses * cost_classes_mode_cache[MAX_N_REG_CLASSES/*!MAX_MACHINE_MODE*/];
   /* Cost classes that include all classes in ira_important_classes.  */
   //原型 all_cost_classes ira-costs.cc
   CostClasses all_cost_classes;
};

MtcsIraCosts *mtcs_ira_costs_new(MtcsMode *mtcsMode);
//原型 ira_init_costs_once ira-int.h ira-costs.cc
void mtcs_ira_costs_init_costs_once (MtcsIraCosts *self);
//原型 ira_init_costs ira-int.h ira-costs.cc
void mtcs_ira_costs_init_costs (MtcsIraCosts *self);
//原型 ira_costs_cc_finalize ira.h ira-costs.cc
void mtcs_ira_costs_cc_finalize (MtcsIraCosts *self);
//原型 ira_adjust_equiv_reg_cost ira.h ira-costs.cc
void mtcs_ira_costs_adjust_equiv_reg_cost (MtcsIraCosts *self,unsigned regno, int cost);
//原型 ira_costs ira-int.h ira-costs.cc
void mtcs_ira_costs_costs (MtcsIraCosts *self);
//原型 ira_set_pseudo_classes ira.h ira-costs.cc
void mtcs_ira_costs_set_pseudo_classes (MtcsIraCosts *self,bool define_pseudo_classes, FILE *dump_file);
//原型 ira_tune_allocno_costs ira-int.h ira-costs.cc
void mtcs_ira_costs_tune_allocno_costs (MtcsIraCosts *self);

#endif
