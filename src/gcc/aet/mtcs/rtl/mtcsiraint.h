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

#ifndef __GCC_MTCS_IRA_INT__
#define __GCC_MTCS_IRA_INT__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "../mtcsmicro.h"
#include "mtcsiracosts.h"


typedef unsigned short ira_move_table[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

typedef struct _MtcsIraInt MtcsIraInt;

struct _MtcsIraInt
{
   MtcsComponent parent;
  //void free_ira_costs (); mtcs_ira_int_free_ira_costs替换
 // void free_register_move_costs ();  mtcs_ira_int_free_register_move_costs替换

  /* Initialized once.  It is a maximal possible size of the allocated
     struct costs.  */
  size_t x_max_struct_costs_size;

  /* Allocated and initialized once, and used to initialize cost values
     for each insn.  */
  IraCosts *x_init_cost;

  /* Allocated once, and used for temporary purposes.  */
  IraCosts*x_temp_costs;

  /* Allocated once, and used for the cost calculation.  */
  IraCosts *x_op_costs[MAX_MAX_RECOG_OPERANDS/*!MAX_RECOG_OPERANDS*/];
  IraCosts *x_this_op_costs[MAX_MAX_RECOG_OPERANDS/*!MAX_RECOG_OPERANDS*/];

  /* Hard registers that cannot be used for the register allocator for
     all functions of the current compilation unit.  */
  HardRegSet x_no_unit_alloc_regs;

  /* Map: hard regs X modes -> set of hard registers for storing value
     of given mode starting with given hard register.  */
  HardRegSet (x_ira_reg_mode_hard_regset
      [MAX_FIRST_PSEUDO_REGISTER/*!FIRST_PSEUDO_REGISTER*/][MAX_NUM_MACHINE_MODES/*!NUM_MACHINE_MODES*/]);

  /* Maximum cost of moving from a register in one class to a register
     in another class.  Based on TARGET_REGISTER_MOVE_COST.  */
  ira_move_table *x_ira_register_move_cost[MAX_MAX_MACHINE_MODE/*!MAX_MACHINE_MODE*/];

  /* Similar, but here we don't have to move if the first index is a
     subset of the second so in that case the cost is zero.  */
  ira_move_table *x_ira_may_move_in_cost[MAX_MAX_MACHINE_MODE/*!MAX_MACHINE_MODE*/];

  /* Similar, but here we don't have to move if the first index is a
     superset of the second so in that case the cost is zero.  */
  ira_move_table *x_ira_may_move_out_cost[MAX_MAX_MACHINE_MODE/*!MAX_MACHINE_MODE*/];

  /* Keep track of the last mode we initialized move costs for.  */
  int x_last_mode_for_init_move_cost;

  /* Array analog of the macro MEMORY_MOVE_COST but they contain maximal
     cost not minimal.  */
  short int x_ira_max_memory_move_cost[MAX_MAX_MACHINE_MODE/*!MAX_MACHINE_MODE*/][MAX_N_REG_CLASSES/*!N_REG_CLASSES*/][2];

  /* Map class->true if class is a possible allocno class, false
     otherwise. */
  bool x_ira_reg_allocno_class_p[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

  /* Map class->true if class is a pressure class, false otherwise. */
  bool x_ira_reg_pressure_class_p[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

  /* Array of the number of hard registers of given class which are
     available for allocation.  The order is defined by the hard
     register numbers.  */
  short x_ira_non_ordered_class_hard_regs[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/][MAX_FIRST_PSEUDO_REGISTER/*!FIRST_PSEUDO_REGISTER*/];

  /* Index (in ira_class_hard_regs; for given register class and hard
     register (in general case a hard register can belong to several
     register classes;.  The index is negative for hard registers
     unavailable for the allocation.  */
  short x_ira_class_hard_reg_index[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/][MAX_FIRST_PSEUDO_REGISTER/*!FIRST_PSEUDO_REGISTER*/];

  /* Index [CL][M] contains R if R appears somewhere in a register of the form:

         (reg:M R'), R' not in x_ira_prohibited_class_mode_regs[CL][M]

     For example, if:

     - (reg:M 2) is valid and occupies two registers;
     - register 2 belongs to CL; and
     - register 3 belongs to the same pressure class as CL

     then (reg:M 2) contributes to [CL][M] and registers 2 and 3 will be
     in the set.  */
  HardRegSet x_ira_useful_class_mode_regs[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/][MAX_NUM_MACHINE_MODES/*!NUM_MACHINE_MODES*/];

  /* The value is number of elements in the subsequent array.  */
  int x_ira_important_classes_num;

  /* The array containing all non-empty classes.  Such classes is
     important for calculation of the hard register usage costs.  */
  enum reg_class x_ira_important_classes[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

  /* The array containing indexes of important classes in the previous
     array.  The array elements are defined only for important
     classes.  */
  int x_ira_important_class_nums[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

  /* Map class->true if class is an uniform class, false otherwise.  */
  bool x_ira_uniform_class_p[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

  /* The biggest important class inside of intersection of the two
     classes (that is calculated taking only hard registers available
     for allocation into account;.  If the both classes contain no hard
     registers available for allocation, the value is calculated with
     taking all hard-registers including fixed ones into account.  */
  enum reg_class x_ira_reg_class_intersect[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/][MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

  /* Classes with end marker LIM_REG_CLASSES which are intersected with
     given class (the first index).  That includes given class itself.
     This is calculated taking only hard registers available for
     allocation into account.  */
  enum reg_class x_ira_reg_class_super_classes[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/][MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

  /* The biggest (smallest) important class inside of (covering) union
     of the two classes (that is calculated taking only hard registers
     available for allocation into account).  If the both classes
     contain no hard registers available for allocation, the value is
     calculated with taking all hard-registers including fixed ones
     into account.  In other words, the value is the corresponding
     reg_class_subunion (reg_class_superunion) value.  */
  enum reg_class x_ira_reg_class_subunion[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/][MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];
  enum reg_class x_ira_reg_class_superunion[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/][MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

  /* For each reg class, table listing all the classes contained in it
     (excluding the class itself.  Non-allocatable registers are
     excluded from the consideration).  */
  enum reg_class x_alloc_reg_class_subclasses[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/][MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

  /* Array whose values are hard regset of hard registers for which
     move of the hard register in given mode into itself is
     prohibited.  */
  HardRegSet x_ira_prohibited_mode_move_regs[MAX_NUM_MACHINE_MODES/*!NUM_MACHINE_MODES*/];

  /* Flag of that the above array has been initialized.  */
  bool x_ira_prohibited_mode_move_regs_initialized_p;
};

MtcsIraInt *mtcs_ira_int_new(MtcsMode *mtcsMode);
//原型 free_register_move_costs ira-int.h ira.cc
void mtcs_ira_int_free_register_move_costs (MtcsIraInt *self);
// 原型 target_ira_int::~target_ira_int   ira-int.h ira.cc
void mtcs_ira_int_free (MtcsIraInt *self);
//原型 ira_init_register_move_cost_if_necessary ira-int.h
void mtcs_ira_int_init_register_move_cost_if_necessary (MtcsIraInt *self,machine_mode mode);
//原型 void target_ira_int::free_ira_costs () ira-int.h ira-costs.cc
void mtcs_ira_int_free_ira_costs (MtcsIraInt *self);

#endif
