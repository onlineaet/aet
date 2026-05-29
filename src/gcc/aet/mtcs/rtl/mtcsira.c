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
 * base on ira.cc
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

#include "../mtcstarget.h"
#include "../mtcscompile.h"
#include "../mtcsdfcore.h"
#include "../mtcsdfproblems.h"
#include "mtcsira.h"
#include "mtcsiraint.h"
#include "mtcsiraemit.h"
#include "mtcsirabuild.h"
#include "mtcsiracolor.h"
#include "mtcsiracosts.h"

static bool equiv_init_varies_p (MtcsIra *self,rtx x);
static bool memref_referenced_p (MtcsIra *self,rtx memref, rtx x, bool read_p);

enum valid_equiv { valid_none, valid_combine, valid_reload };

/* Return number of hard registers in hard register SET.  */
//原型 hard_reg_set_size ira-int.h
int mtcs_ira_hard_reg_set_size (MtcsIra *self, HardRegSet *set)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int i, size;
   for (size = i = 0; i < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i++)
      if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(set, i))
         size++;
   return size;
}

/* The function returns TRUE if hard registers starting with
   HARD_REGNO and containing value of MODE are fully in set
   HARD_REGSET.  */
//原型 ira_hard_reg_in_set_p ira-int.h
bool mtcs_ira_hard_reg_in_set_p (MtcsIra *self,int hard_regno, machine_mode mode,HardRegSet *hard_regset)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int i;
   ira_assert (hard_regno >= 0);
   for (i = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,hard_regno, mode) - 1; i >= 0; i--)
      if (!mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(hard_regset, hard_regno + i))
         return false;
   return true;
}

//原型 ira_init_register_move_cost_if_necessary ira-int.h
void mtcs_ira_init_register_move_cost_if_necessary (MtcsIra *self,machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

  if (mtcsIraInt->x_ira_register_move_cost[mode] == NULL)
     mtcs_ira_init_register_move_cost/*!ira_init_register_move_cost*/(self,mode);
}


/* Return true if equivalence of pseudo REGNO is not a lvalue.  */
//原型 ira_equiv_no_lvalue_p ira-int.h
bool mtcs_ira_equiv_no_lvalue_p (MtcsIra *self,int regno)
{
   if (regno >= self->ira_reg_equiv_len)
      return false;
   return (self->ira_reg_equiv[regno].constant != NULL_RTX
      || self->ira_reg_equiv[regno].invariant != NULL_RTX
      || (self->ira_reg_equiv[regno].memory != NULL_RTX
      && MEM_READONLY_P (self->ira_reg_equiv[regno].memory)));
}

#define last_mode_for_init_move_cost (mtcsIraInt/*!this_target_ira_int*/->x_last_mode_for_init_move_cost)
#define no_unit_alloc_regs (mtcsIraInt/*!this_target_ira_int*/->x_no_unit_alloc_regs)
#define alloc_reg_class_subclasses  (mtcsIraInt->x_alloc_reg_class_subclasses)
#define ira_prohibited_mode_move_regs_initialized_p \
  (mtcsIraInt/*!this_target_ira_int*/->x_ira_prohibited_mode_move_regs_initialized_p)

/* The function sets up the map IRA_REG_MODE_HARD_REGSET.  */
static void setup_reg_mode_hard_regset (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int i, m, hard_regno;

   for (m = 0; m < mtcs_mode_get_number/*!NUM_MACHINE_MODES*/(mtcsMode); m++)
      for (hard_regno = 0; hard_regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); hard_regno++){
         mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&mtcsIraInt->x_ira_reg_mode_hard_regset[hard_regno][m]);
         for (i = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,hard_regno, (machine_mode) m) - 1;i >= 0; i--)
            if (hard_regno + i < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg))
               mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,
                     &mtcsIraInt->x_ira_reg_mode_hard_regset/*!ira_reg_mode_hard_regset*/[hard_regno][m],hard_regno + i);
      }
}

/* The function sets up the three arrays declared above.  */
static void setup_class_hard_regs (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);
   int cl, i, hard_regno, n;
   HardRegSet /*!HARD_REG_SET*/ processed_hard_reg_set = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};

   ira_assert (SHRT_MAX >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/);
   for (cl = (int) mtcs_reg_get_n_reg_classes/*!N_REG_CLASSES*/(mtcsReg) - 1; cl >= 0; cl--){
      self->temp_hard_regset = mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl] & ~no_unit_alloc_regs;
      mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&processed_hard_reg_set);
      for (i = 0; i < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; i++){
         mtcsIraInt->x_ira_non_ordered_class_hard_regs/*!ira_non_ordered_class_hard_regs*/[cl][i] = -1;
         mtcsIraInt->x_ira_class_hard_reg_index/*!ira_class_hard_reg_index*/[cl][i] = -1;
      }
      for (n = 0, i = 0; i < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; i++){
         if(mtcs_config_ifdef(mtcsConfig,MTCS_REG_ALLOC_ORDER)){
         //#ifdef REG_ALLOC_ORDER
            hard_regno = mtcsReg->hardRegs.x_reg_alloc_order/*!reg_alloc_order*/[i];
         //#else
         }else
            hard_regno = i;
         //#endif
         if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&processed_hard_reg_set, hard_regno))
            continue;
         mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&processed_hard_reg_set, hard_regno);
         if (! mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&self->temp_hard_regset, hard_regno))
            mtcsIraInt->x_ira_class_hard_reg_index/*!ira_class_hard_reg_index*/[cl][hard_regno] = -1;
         else{
            mtcsIraInt->x_ira_class_hard_reg_index/*!ira_class_hard_reg_index*/[cl][hard_regno] = n;
            self->x_ira_class_hard_regs/*!ira_class_hard_regs*/[cl][n++] = hard_regno;
         }
      }

      self->x_ira_class_hard_regs_num/*!ira_class_hard_regs_num*/[cl] = n;
      for (n = 0, i = 0; i < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; i++)
         if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&self->temp_hard_regset, i)){
            mtcsIraInt->x_ira_non_ordered_class_hard_regs/*!ira_non_ordered_class_hard_regs*/[cl][n++] = i;
         }
      ira_assert (self->x_ira_class_hard_regs_num/*!ira_class_hard_regs_num*/[cl] == n);
   }
}

/* Set up global variables defining info about hard registers for the
   allocation.  These depend on USE_HARD_FRAME_P whose TRUE value means
   that we can use the hard frame pointer for the allocation.  */
static void setup_alloc_regs (MtcsIra *self,bool use_hard_frame_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   //#ifdef ADJUST_REG_ALLOC_ORDER host=1 nvptx=0
   //  ADJUST_REG_ALLOC_ORDER;
   //#endif
   no_unit_alloc_regs =mtcsReg->hardRegs.x_fixed_nonglobal_reg_set/*!fixed_nonglobal_reg_set*/;
   if (! use_hard_frame_p)
      mtcs_reg_add_to_hard_reg_set/*!add_to_hard_reg_set*/(mtcsReg,&no_unit_alloc_regs,
               mtcs_mode_get_Pmode(mtcsMode),mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg));
   setup_class_hard_regs(self);
}

/* Initialize the table of subclasses of each reg class.  */
static void setup_reg_subclasses (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int i, j;
   HardRegSet /*!HARD_REG_SET*/ temp_hard_regset2 = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};
   int nRegClasses=mtcs_reg_get_n_reg_classes/*!N_REG_CLASSES*/(mtcsReg);
   for (i = 0; i < nRegClasses/*!N_REG_CLASSES*/; i++)
      for (j = 0; j < nRegClasses/*!N_REG_CLASSES*/; j++)
         alloc_reg_class_subclasses[i][j] = mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg);

   for (i = 0; i < nRegClasses/*!N_REG_CLASSES*/; i++){
      if (i == (int) NO_REGS)
         continue;

      self->temp_hard_regset = mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[i] & ~no_unit_alloc_regs;
      if (mtcs_reg_hard_reg_set_empty_p/*!hard_reg_set_empty_p*/(&self->temp_hard_regset))
         continue;
      for (j = 0; j < nRegClasses/*!N_REG_CLASSES*/; j++)
         if (i != j){
            enum reg_class *p;

            temp_hard_regset2 = mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[j] & ~no_unit_alloc_regs;
            if (! mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&self->temp_hard_regset,&temp_hard_regset2))
               continue;
            p = &alloc_reg_class_subclasses[j][0];
            while (*p != mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg)) p++;
            *p = (enum reg_class) i;
         }
   }
}

/* Set up IRA_MEMORY_MOVE_COST and IRA_MAX_MEMORY_MOVE_COST.  */
static void setup_class_subset_and_memory_move_costs (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsReload *mtcsReload=mtcs_target_get_reload(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int cl, cl2, mode, cost;
   HardRegSet /*!HARD_REG_SET*/ temp_hard_regset2 = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};
   int maxMachineMode= mtcs_mode_get_max_number/*!MAX_MACHINE_MODE*/(mtcsMode);
   int nRegClasses=mtcs_reg_get_n_reg_classes/*!N_REG_CLASSES*/(mtcsReg);

   for (mode = 0; mode < maxMachineMode/*!MAX_MACHINE_MODE*/; mode++)
      self->x_ira_memory_move_cost[mode][NO_REGS][0] = self->x_ira_memory_move_cost[mode][NO_REGS][1] = SHRT_MAX;

   for (cl = (int) nRegClasses/*!N_REG_CLASSES*/ - 1; cl >= 0; cl--) {
      if (cl != (int) NO_REGS)
         for (mode = 0; mode <  maxMachineMode/*!MAX_MACHINE_MODE*/; mode++){
            mtcsIraInt->x_ira_max_memory_move_cost/*!ira_max_memory_move_cost*/[mode][cl][0]
            = self->x_ira_memory_move_cost/*!self->x_ira_memory_move_cost*/[mode][cl][0]
            = mtcs_reload_memory_move_cost/*!memory_move_cost*/(mtcsReload,(machine_mode) mode, (reg_class_t) cl, false);
            mtcsIraInt->x_ira_max_memory_move_cost[mode][cl][1]
            = self->x_ira_memory_move_cost[mode][cl][1]
            = mtcs_reload_memory_move_cost/*!memory_move_cost*/(mtcsReload,(machine_mode) mode, (reg_class_t) cl, true);
            /* Costs for NO_REGS are used in cost calculation on the
            1st pass when the preferred register classes are not
            known yet.  In this case we take the best scenario.  */
            if (!mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,
                  self->x_ira_class_hard_regs/*!ira_class_hard_regs*/[cl][0],(machine_mode) mode))
               continue;

            if (self->x_ira_memory_move_cost[mode][NO_REGS][0] > self->x_ira_memory_move_cost[mode][cl][0])
               mtcsIraInt->x_ira_max_memory_move_cost[mode][NO_REGS][0]
               = self->x_ira_memory_move_cost[mode][NO_REGS][0]
               = self->x_ira_memory_move_cost[mode][cl][0];
            if (self->x_ira_memory_move_cost[mode][NO_REGS][1] > self->x_ira_memory_move_cost[mode][cl][1])
               mtcsIraInt->x_ira_max_memory_move_cost[mode][NO_REGS][1]
               = self->x_ira_memory_move_cost[mode][NO_REGS][1]
               = self->x_ira_memory_move_cost[mode][cl][1];
         }
   }

   for (cl = (int) nRegClasses/*!N_REG_CLASSES*/ - 1; cl >= 0; cl--)
      for (cl2 = (int) nRegClasses/*!N_REG_CLASSES*/ - 1; cl2 >= 0; cl2--){
         self->temp_hard_regset = mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl] & ~no_unit_alloc_regs;
         temp_hard_regset2 = mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl2] & ~no_unit_alloc_regs;
         self->x_ira_class_subset_p[cl][cl2] =
               mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&self->temp_hard_regset, &temp_hard_regset2);
         if (! mtcs_reg_hard_reg_set_empty_p/*!hard_reg_set_empty_p*/(&temp_hard_regset2)
         && mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl2],
         &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl]))
            for (mode = 0; mode <  maxMachineMode/*!MAX_MACHINE_MODE*/; mode++){
               cost = self->x_ira_memory_move_cost[mode][cl2][0];
               if (cost > mtcsIraInt->x_ira_max_memory_move_cost[mode][cl][0])
                  mtcsIraInt->x_ira_max_memory_move_cost[mode][cl][0] = cost;
               cost = self->x_ira_memory_move_cost[mode][cl2][1];
               if (cost > mtcsIraInt->x_ira_max_memory_move_cost[mode][cl][1])
                  mtcsIraInt->x_ira_max_memory_move_cost[mode][cl][1] = cost;
            }
      }

   for (cl = (int) nRegClasses/*!N_REG_CLASSES*/ - 1; cl >= 0; cl--)
      for (mode = 0; mode <  maxMachineMode/*!MAX_MACHINE_MODE*/; mode++){
         self->x_ira_memory_move_cost[mode][cl][0] = mtcsIraInt->x_ira_max_memory_move_cost[mode][cl][0];
         self->x_ira_memory_move_cost[mode][cl][1] = mtcsIraInt->x_ira_max_memory_move_cost[mode][cl][1];
      }
   setup_reg_subclasses(self);
}

/* Output information about allocation of all allocnos (except for
   caps) into file F.  */
//原型 ira_print_disposition ira-int.h ira.cc
void mtcs_ira_print_disposition (MtcsIra *self,FILE *f)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   int i, n, max_regno;
   MtcsIraAllocno * a;
   basic_block bb;

   fprintf (f, "Disposition:");
   max_regno = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);
   for (n = 0, i = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i < max_regno; i++)
      for (a =mtcsIraBuild->ira_regno_allocno_map[i]; a != NULL; a = a->next_regno_allocno){
         if (n % 4 == 0)
            fprintf (f, "\n");
         n++;
         fprintf (f, " %4d:r%-4d", a->num, a->regno);
         if ((bb = a->loop_tree_node->bb) != NULL)
            fprintf (f, "b%-3d", bb->index);
         else
            fprintf (f, "l%-3d", a->loop_tree_node->loop_num);
         if (a->hard_regno >= 0)
            fprintf (f, " %3d", a->hard_regno);
         else
            fprintf (f, " mem");
      }
   fprintf (f, "\n");
}

/* Outputs information about allocation of all allocnos into
   stderr.  */
//原型 ira_debug_disposition ira-int.h ira.cc
void mtcs_ira_debug_disposition (MtcsIra *self)
{
   mtcs_ira_print_disposition/*!ira_print_disposition*/(self,stderr);
}

/* Set up ira_stack_reg_pressure_class which is the biggest pressure
   register class containing stack registers or NO_REGS if there are
   no stack registers.  To find this class, we iterate through all
   register pressure classes and choose the first register pressure
   class containing all the stack registers and having the biggest
   size.  */
static void setup_stack_reg_pressure_class (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   self->x_ira_stack_reg_pressure_class = NO_REGS;
   if(mtcs_config_ifdef(mtcsConfig,MTCS_STACK_REGS)){
//#ifdef STACK_REGS
//{
      int i, best, size;
      enum reg_class cl;
      HardRegSet /*!HARD_REG_SET*/ temp_hard_regset2 = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};

      mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&self->temp_hard_regset);
      for (i = mtcs_reg_get_first_stack_reg/*!FIRST_STACK_REG*/(mtcsReg);
      i <= mtcs_reg_get_last_stack_reg/*!LAST_STACK_REG*/(mtcsReg); i++)
         mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&self->temp_hard_regset, i);
      best = 0;
      for (i = 0; i < self->x_ira_pressure_classes_num; i++){
         cl = self->x_ira_pressure_classes[i];
         temp_hard_regset2 = self->temp_hard_regset & mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl];
         size =mtcs_ira_hard_reg_set_size/*!hard_reg_set_size*/(self,&temp_hard_regset2);
         if (best < size){
            best = size;
            self->x_ira_stack_reg_pressure_class = cl;
         }
      }
   }
//#endif
}

/* Find pressure classes which are register classes for which we
   calculate register pressure in IRA, register pressure sensitive
   insn scheduling, and register pressure sensitive loop invariant
   motion.

   To make register pressure calculation easy, we always use
   non-intersected register pressure classes.  A move of hard
   registers from one register pressure class is not more expensive
   than load and store of the hard registers.  Most likely an allocno
   class will be a subset of a register pressure class and in many
   cases a register pressure class.  That makes usage of register
   pressure classes a good approximation to find a high register
   pressure.  */
static void setup_pressure_classes (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int nRegClasses=mtcs_reg_get_n_reg_classes/*!N_REG_CLASSES*/(mtcsReg);
   int numMachineModes= mtcs_mode_get_number/*!NUM_MACHINE_MODES*/(mtcsMode);
   int cost, i, n, curr;
   int cl, cl2;
   enum reg_class pressure_classes[nRegClasses/*!N_REG_CLASSES*/];
   int m;
   HardRegSet /*!HARD_REG_SET*/ temp_hard_regset2 = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};
   bool insert_p;

   if (mtcsTarget/*!targetm.compute_pressure_classes*/->compute_pressure_classes)
      n = mtcsTarget/*!targetm.compute_pressure_classes*/->compute_pressure_classes(mtcsTarget,pressure_classes);
   else{
      n = 0;
      for (cl = 0; cl < nRegClasses/*!N_REG_CLASSES*/; cl++){
         if (self->x_ira_class_hard_regs_num[cl] == 0)
            continue;
         if (self->x_ira_class_hard_regs_num[cl] != 1
         /* A register class without subclasses may contain a few
         hard registers and movement between them is costly
         (e.g. SPARC FPCC registers).  We still should consider it
         as a candidate for a pressure class.  */
         && alloc_reg_class_subclasses[cl][0] < cl){
            /* Check that the moves between any hard registers of the
            current class are not more expensive for a legal mode
            than load/store of the hard registers of the current
            class.  Such class is a potential candidate to be a
            register pressure class.  */
            for (m = 0; m < numMachineModes/*!NUM_MACHINE_MODES*/; m++){
               self->temp_hard_regset = (mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl]
                                                 & ~(no_unit_alloc_regs| self->x_ira_prohibited_class_mode_regs[cl][m]));
               if (mtcs_reg_hard_reg_set_empty_p/*!hard_reg_set_empty_p*/(&self->temp_hard_regset))
                  continue;
               mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(self,(machine_mode) m);
               cost = mtcsIraInt->x_ira_register_move_cost[m][cl][cl];
               if (cost <= mtcsIraInt->x_ira_max_memory_move_cost[m][cl][1]
               || cost <= mtcsIraInt->x_ira_max_memory_move_cost[m][cl][0])
                  break;
            }
            if (m >= numMachineModes/*!NUM_MACHINE_MODES*/)
               continue;
         }
         curr = 0;
         insert_p = true;
         self->temp_hard_regset = mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl] & ~no_unit_alloc_regs;
         /* Remove so far added pressure classes which are subset of the
         current candidate class.  Prefer GENERAL_REGS as a pressure
         register class to another class containing the same
         allocatable hard registers.  We do this because machine
         dependent cost hooks might give wrong costs for the latter
         class but always give the right cost for the former class
         (GENERAL_REGS).  */
         for (i = 0; i < n; i++){
            cl2 = pressure_classes[i];
            temp_hard_regset2 = (mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl2] & ~no_unit_alloc_regs);
            if (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&self->temp_hard_regset, &temp_hard_regset2)
            && (self->temp_hard_regset != temp_hard_regset2 || cl2 == (int) mtcs_reg_get_general_regs/*!GENERAL_REGS*/(mtcsReg))){
               pressure_classes[curr++] = (enum reg_class) cl2;
               insert_p = false;
               continue;
            }
            if (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&temp_hard_regset2, &self->temp_hard_regset)
            && (temp_hard_regset2 != self->temp_hard_regset
            || cl == (int)  mtcs_reg_get_general_regs/*!GENERAL_REGS*/(mtcsReg)))
               continue;
            if (temp_hard_regset2 == self->temp_hard_regset)
               insert_p = false;
            pressure_classes[curr++] = (enum reg_class) cl2;
         }
         /* If the current candidate is a subset of a so far added
         pressure class, don't add it to the list of the pressure
         classes.  */
         if (insert_p)
            pressure_classes[curr++] = (enum reg_class) cl;
         n = curr;
      }
   }
#ifdef ENABLE_IRA_CHECKING
   {
      HardRegSet /*!HARD_REG_SET*/ ignore_hard_regs = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};

      /* Check pressure classes correctness: here we check that hard
      registers from all register pressure classes contains all hard
      registers available for the allocation.  */
      mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&self->temp_hard_regset);
      mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&temp_hard_regset2);
      ignore_hard_regs = no_unit_alloc_regs;
      for (cl = 0; cl < mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg); cl++){
         /* For some targets (like MIPS with MD_REGS), there are some
         classes with hard registers available for allocation but
         not able to hold value of any mode.  */
         for (m = 0; m < numMachineModes/*!NUM_MACHINE_MODES*/; m++)
            if (mtcsReg->hardRegs.x_contains_reg_of_mode[cl][m])
               break;
         if (m >= numMachineModes/*!NUM_MACHINE_MODES*/){
            ignore_hard_regs |= mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl];
            continue;
         }
         for (i = 0; i < n; i++)
            if ((int) pressure_classes[i] == cl)
               break;
         temp_hard_regset2 |= mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl];
         if (i < n)
            self->temp_hard_regset |= mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl];
      }
      for (i = 0; i < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i++)
         /* Some targets (like SPARC with ICC reg) have allocatable regs
         for which no reg class is defined.  */
         if (mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,i) == NO_REGS)
            mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&ignore_hard_regs, i);
      self->temp_hard_regset &= ~ignore_hard_regs;
      temp_hard_regset2 &= ~ignore_hard_regs;
      ira_assert (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&temp_hard_regset2, &self->temp_hard_regset));
   }
#endif
   self->x_ira_pressure_classes_num = 0;
   for (i = 0; i < n; i++){
      cl = (int) pressure_classes[i];
      mtcsIraInt->x_ira_reg_pressure_class_p[cl] = true;
      self->x_ira_pressure_classes[self->x_ira_pressure_classes_num++] = (enum reg_class) cl;
   }
   setup_stack_reg_pressure_class(self);
}

/* Set up IRA_UNIFORM_CLASS_P.  Uniform class is a register class
   whose register move cost between any registers of the class is the
   same as for all its subclasses.  We use the data to speed up the
   2nd pass of calculations of allocno costs.  */
static void setup_uniform_class_p (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int nRegClasses=mtcs_reg_get_n_reg_classes/*!N_REG_CLASSES*/(mtcsReg);
   int numMachineModes= mtcs_mode_get_number/*!NUM_MACHINE_MODES*/(mtcsMode);
   int i, cl, cl2, m;

   for (cl = 0; cl < nRegClasses/*!N_REG_CLASSES*/; cl++){
      mtcsIraInt->x_ira_uniform_class_p[cl] = false;
      if (self->x_ira_class_hard_regs_num[cl] == 0)
         continue;
      /* We cannot use alloc_reg_class_subclasses here because move
      cost hooks does not take into account that some registers are
      unavailable for the subtarget.  E.g. for i686, INT_SSE_REGS
      is element of alloc_reg_class_subclasses for GENERAL_REGS
      because SSE regs are unavailable.  */
      for (i = 0; (cl2 = mtcsReg->hardRegs.x_reg_class_subclasses/*reg_class_subclasses*/[cl][i]) != mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg); i++){
         if (self->x_ira_class_hard_regs_num[cl2] == 0)
            continue;
         for (m = 0; m < numMachineModes/*!NUM_MACHINE_MODES*/; m++)
            if (contains_reg_of_mode[cl][m] && contains_reg_of_mode[cl2][m]){
               mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(self,(machine_mode) m);
               if (mtcsIraInt->x_ira_register_move_cost[m][cl][cl] != mtcsIraInt->x_ira_register_move_cost[m][cl2][cl2])
                  break;
            }
         if (m < numMachineModes/*!NUM_MACHINE_MODES*/)
            break;
      }
      if (cl2 == mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg))
         mtcsIraInt->x_ira_uniform_class_p[cl] = true;
   }
}

/* Set up IRA_ALLOCNO_CLASSES, IRA_ALLOCNO_CLASSES_NUM,
   IRA_IMPORTANT_CLASSES, and IRA_IMPORTANT_CLASSES_NUM.

   Target may have many subtargets and not all target hard registers can
   be used for allocation, e.g. x86 port in 32-bit mode cannot use
   hard registers introduced in x86-64 like r8-r15).  Some classes
   might have the same allocatable hard registers, e.g.  INDEX_REGS
   and GENERAL_REGS in x86 port in 32-bit mode.  To decrease different
   calculations efforts we introduce allocno classes which contain
   unique non-empty sets of allocatable hard-registers.

   Pseudo class cost calculation in ira-costs.cc is very expensive.
   Therefore we are trying to decrease number of classes involved in
   such calculation.  Register classes used in the cost calculation
   are called important classes.  They are allocno classes and other
   non-empty classes whose allocatable hard register sets are inside
   of an allocno class hard register set.  From the first sight, it
   looks like that they are just allocno classes.  It is not true.  In
   example of x86-port in 32-bit mode, allocno classes will contain
   GENERAL_REGS but not LEGACY_REGS (because allocatable hard
   registers are the same for the both classes).  The important
   classes will contain GENERAL_REGS and LEGACY_REGS.  It is done
   because a machine description insn constraint may refers for
   LEGACY_REGS and code in ira-costs.cc is mostly base on investigation
   of the insn constraints.  */
static void setup_allocno_and_important_classes (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int nRegClasses=mtcs_reg_get_n_reg_classes/*!N_REG_CLASSES*/(mtcsReg);

   int i, j, n, cl;
   bool set_p;
   HardRegSet /*!HARD_REG_SET*/ temp_hard_regset2 = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};

   enum reg_class classes[mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg) + 1];

   n = 0;
   /* Collect classes which contain unique sets of allocatable hard
   registers.  Prefer GENERAL_REGS to other classes containing the
   same set of hard registers.  */
   for (i = 0; i < mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg); i++){
      self->temp_hard_regset = mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[i] & ~no_unit_alloc_regs;
      for (j = 0; j < n; j++){
         cl = classes[j];
         temp_hard_regset2 = mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl] & ~no_unit_alloc_regs;
         if (self->temp_hard_regset == temp_hard_regset2)
            break;
      }
      if (j >= n || targetm.additional_allocno_class_p (i))
         classes[n++] = (enum reg_class) i;
      else if (i == mtcs_reg_get_general_regs/*!GENERAL_REGS*/(mtcsReg))
      /* Prefer general regs.  For i386 example, it means that
      we prefer GENERAL_REGS over INDEX_REGS or LEGACY_REGS
      (all of them consists of the same available hard
      registers).  */
         classes[j] = (enum reg_class) i;
   }
   classes[n] = mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg);

   /* Set up classes which can be used for allocnos as classes
   containing non-empty unique sets of allocatable hard
   registers.  */
   self->x_ira_allocno_classes_num = 0;
   for (i = 0; (cl = classes[i]) != mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg); i++)
      if (self->x_ira_class_hard_regs_num[cl] > 0)
         self->x_ira_allocno_classes[self->x_ira_allocno_classes_num++] = (enum reg_class) cl;
   mtcsIraInt->x_ira_important_classes_num = 0;
   /* Add non-allocno classes containing to non-empty set of
   allocatable hard regs.  */
   for (cl = 0; cl < nRegClasses/*!N_REG_CLASSES*/; cl++)
      if (self->x_ira_class_hard_regs_num[cl] > 0){
         self->temp_hard_regset = mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl] & ~no_unit_alloc_regs;
         set_p = false;
         for (j = 0; j < self->x_ira_allocno_classes_num; j++){
            temp_hard_regset2 = (mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[self->x_ira_allocno_classes[j]]
                                                                       & ~no_unit_alloc_regs);
            if ((enum reg_class) cl == self->x_ira_allocno_classes[j])
               break;
            else if (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&self->temp_hard_regset,&temp_hard_regset2))
               set_p = true;
         }
         if (set_p && j >= self->x_ira_allocno_classes_num)
            mtcsIraInt->x_ira_important_classes[mtcsIraInt->x_ira_important_classes_num++]= (enum reg_class) cl;
      }
   /* Now add allocno classes to the important classes.  */
   for (j = 0; j < self->x_ira_allocno_classes_num; j++)
      mtcsIraInt->x_ira_important_classes[mtcsIraInt->x_ira_important_classes_num++] = self->x_ira_allocno_classes[j];
   for (cl = 0; cl < nRegClasses/*!N_REG_CLASSES*/; cl++){
      mtcsIraInt->x_ira_reg_allocno_class_p[cl] = false;
      mtcsIraInt->x_ira_reg_pressure_class_p[cl] = false;
   }
   for (j = 0; j < self->x_ira_allocno_classes_num; j++)
      mtcsIraInt->x_ira_reg_allocno_class_p[self->x_ira_allocno_classes[j]] = true;
   setup_pressure_classes(self);
   setup_uniform_class_p(self);
}

/* Setup translation in CLASS_TRANSLATE of all classes into a class
   given by array CLASSES of length CLASSES_NUM.  The function is used
   make translation any reg class to an allocno class or to an
   pressure class.  This translation is necessary for some
   calculations when we can use only allocno or pressure classes and
   such translation represents an approximate representation of all
   classes.

   The translation in case when allocatable hard register set of a
   given class is subset of allocatable hard register set of a class
   in CLASSES is pretty simple.  We use smallest classes from CLASSES
   containing a given class.  If allocatable hard register set of a
   given class is not a subset of any corresponding set of a class
   from CLASSES, we use the cheapest (with load/store point of view)
   class from CLASSES whose set intersects with given class set.  */
static void setup_class_translate_array (MtcsIra *self,enum reg_class *class_translate,
              int classes_num, enum reg_class *classes)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int nRegClasses=mtcs_reg_get_n_reg_classes/*!N_REG_CLASSES*/(mtcsReg);

   int cl, mode;
   enum reg_class aclass, best_class, *cl_ptr;
   int i, cost, min_cost, best_cost;

   for (cl = 0; cl < nRegClasses/*!N_REG_CLASSES*/; cl++)
      class_translate[cl] = NO_REGS;

   for (i = 0; i < classes_num; i++){
      aclass = classes[i];
      for (cl_ptr = &alloc_reg_class_subclasses[aclass][0];(cl = *cl_ptr) != mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg);
                        cl_ptr++)
         if (class_translate[cl] == NO_REGS)
            class_translate[cl] = aclass;
      class_translate[aclass] = aclass;
   }
   /* For classes which are not fully covered by one of given classes
   (in other words covered by more one given class), use the
   cheapest class.  */
   for (cl = 0; cl < nRegClasses/*!N_REG_CLASSES*/; cl++){
      if (cl == NO_REGS || class_translate[cl] != NO_REGS)
         continue;
      best_class = NO_REGS;
      best_cost = INT_MAX;
      for (i = 0; i < classes_num; i++){
         aclass = classes[i];
         self->temp_hard_regset = (mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[aclass]
                            & mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl] & ~no_unit_alloc_regs);
         if (! mtcs_reg_hard_reg_set_empty_p/*!hard_reg_set_empty_p*/(&self->temp_hard_regset)){
            min_cost = INT_MAX;
            for (mode = 0; mode < mtcs_mode_get_max_number/*!MAX_MACHINE_MODE*/(mtcsMode); mode++){
               cost = (self->x_ira_memory_move_cost[mode][aclass][0] + self->x_ira_memory_move_cost[mode][aclass][1]);
               if (min_cost > cost)
                  min_cost = cost;
            }
            if (best_class == NO_REGS || best_cost > min_cost){
               best_class = aclass;
               best_cost = min_cost;
            }
         }
      }
      class_translate[cl] = best_class;
   }
}

/* Set up array IRA_ALLOCNO_CLASS_TRANSLATE and
   IRA_PRESSURE_CLASS_TRANSLATE.  */
static void setup_class_translate (MtcsIra *self)
{
  setup_class_translate_array(self,self->x_ira_allocno_class_translate,
                self->x_ira_allocno_classes_num, self->x_ira_allocno_classes);
  setup_class_translate_array(self,self->x_ira_pressure_class_translate,
                self->x_ira_pressure_classes_num, self->x_ira_pressure_classes);
}


/* The function used to sort the important classes.  */
static int compRegClassesFunc_cb/*!comp_reg_classes_func*/(const void *v1p, const void *v2p)
{
   MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIraInt = mtcs_ira_mgr_get_ira(mtcsIraMgr);

   enum reg_class cl1 = *(const enum reg_class *) v1p;
   enum reg_class cl2 = *(const enum reg_class *) v2p;
   enum reg_class tcl1, tcl2;
   int diff;

   tcl1 = mtcsIraInt->x_ira_allocno_class_translate[cl1];
   tcl2 = mtcsIraInt->x_ira_allocno_class_translate[cl2];
   if (tcl1 != NO_REGS && tcl2 != NO_REGS
   && (diff = mtcsIraInt->allocno_class_order[tcl1] - mtcsIraInt->allocno_class_order[tcl2]) != 0)
      return diff;
   return (int) cl1 - (int) cl2;
}

/* For correct work of function setup_reg_class_relation we need to
   reorder important classes according to the order of their allocno
   classes.  It places important classes containing the same
   allocatable hard register set adjacent to each other and allocno
   class with the allocatable hard register set right after the other
   important classes with the same set.

   In example from comments of function
   setup_allocno_and_important_classes, it places LEGACY_REGS and
   GENERAL_REGS close to each other and GENERAL_REGS is after
   LEGACY_REGS.  */
static void reorder_important_classes (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int nRegClasses=mtcs_reg_get_n_reg_classes/*!N_REG_CLASSES*/(mtcsReg);

   int i;

   for (i = 0; i < nRegClasses/*!N_REG_CLASSES*/; i++)
      self->allocno_class_order[i] = -1;
   for (i = 0; i < self->x_ira_allocno_classes_num; i++)
      self->allocno_class_order[self->x_ira_allocno_classes[i]] = i;
   qsort (mtcsIraInt->x_ira_important_classes, mtcsIraInt->x_ira_important_classes_num,
               sizeof (enum reg_class), compRegClassesFunc_cb/*!comp_reg_classes_func*/);
   for (i = 0; i < mtcsIraInt->x_ira_important_classes_num; i++)
      mtcsIraInt->x_ira_important_class_nums[mtcsIraInt->x_ira_important_classes[i]] = i;
}

/* Set up IRA_REG_CLASS_SUBUNION, IRA_REG_CLASS_SUPERUNION,
IRA_REG_CLASS_SUPER_CLASSES, IRA_REG_CLASSES_INTERSECT, and
IRA_REG_CLASSES_INTERSECT_P.  For the meaning of the relations,
please see corresponding comments in ira-int.h.  */
static void setup_reg_class_relations (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int nRegClasses=mtcs_reg_get_n_reg_classes/*!N_REG_CLASSES*/(mtcsReg);
   int i, cl1, cl2, cl3;
   HardRegSet /*!HARD_REG_SET*/ intersection_set = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};
   HardRegSet /*!HARD_REG_SET*/ union_set = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};
   HardRegSet /*!HARD_REG_SET*/ temp_set2 = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};

   bool important_class_p[nRegClasses/*!N_REG_CLASSES*/];

   memset (important_class_p, 0, sizeof (important_class_p));
   for (i = 0; i < mtcsIraInt->x_ira_important_classes_num; i++)
      important_class_p[mtcsIraInt->x_ira_important_classes[i]] = true;
   for (cl1 = 0; cl1 < nRegClasses/*!N_REG_CLASSES*/; cl1++){
      mtcsIraInt->x_ira_reg_class_super_classes[cl1][0] = mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg);
      for (cl2 = 0; cl2 < nRegClasses/*!N_REG_CLASSES*/; cl2++){
         self->x_ira_reg_classes_intersect_p[cl1][cl2] = false;
         mtcsIraInt->x_ira_reg_class_intersect[cl1][cl2] = NO_REGS;
         self->x_ira_reg_class_subset[cl1][cl2] = NO_REGS;
         self->temp_hard_regset = mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl1] & ~no_unit_alloc_regs;
         temp_set2 = mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl2] & ~no_unit_alloc_regs;
         if (mtcs_reg_hard_reg_set_empty_p/*!hard_reg_set_empty_p*/(&self->temp_hard_regset)
         && mtcs_reg_hard_reg_set_empty_p/*!hard_reg_set_empty_p*/(&temp_set2)){
            /* The both classes have no allocatable hard registers
            -- take all class hard registers into account and use
            reg_class_subunion and reg_class_superunion.  */
            for (i = 0;; i++){
               cl3 = mtcsReg->hardRegs.x_reg_class_subclasses/*reg_class_subclasses*/[cl1][i];
               if (cl3 == mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg))
                  break;
               if (mtcs_reg_reg_class_subset_p/*!reg_class_subset_p*/(mtcsReg,
                                 mtcsIraInt->x_ira_reg_class_intersect[cl1][cl2],(enum reg_class) cl3))
                  mtcsIraInt->x_ira_reg_class_intersect[cl1][cl2] = (enum reg_class) cl3;
            }
            mtcsIraInt->x_ira_reg_class_subunion[cl1][cl2] = mtcsReg->hardRegs.x_reg_class_subunion/*!reg_class_subunion*/[cl1][cl2];
            mtcsIraInt->x_ira_reg_class_superunion[cl1][cl2] = mtcsReg->hardRegs.x_reg_class_subunion/*!reg_class_subunion*/[cl1][cl2];
            continue;
         }
         self->x_ira_reg_classes_intersect_p[cl1][cl2]
                             = mtcs_reg_hard_reg_set_intersect_p/*!hard_reg_set_intersect_p*/(&self->temp_hard_regset, &temp_set2);
         if (important_class_p[cl1] && important_class_p[cl2]
         && mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&self->temp_hard_regset, &temp_set2)){
            /* CL1 and CL2 are important classes and CL1 allocatable
            hard register set is inside of CL2 allocatable hard
            registers -- make CL1 a superset of CL2.  */
            enum reg_class *p;

            p = &mtcsIraInt->x_ira_reg_class_super_classes[cl1][0];
            while (*p != mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg))
               p++;
            *p++ = (enum reg_class) cl2;
            *p = mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg);
         }
         mtcsIraInt->x_ira_reg_class_subunion[cl1][cl2] = NO_REGS;
         mtcsIraInt->x_ira_reg_class_superunion[cl1][cl2] = NO_REGS;
         intersection_set = (mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl1]
                                      & mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl2] & ~no_unit_alloc_regs);
         union_set = ((mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl1]
                                      | mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl2]) & ~no_unit_alloc_regs);
         for (cl3 = 0; cl3 < nRegClasses/*!N_REG_CLASSES*/; cl3++){
            self->temp_hard_regset = mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl3] & ~no_unit_alloc_regs;
            if (mtcs_reg_hard_reg_set_empty_p/*!hard_reg_set_empty_p*/(&self->temp_hard_regset))
               continue;

            if (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&self->temp_hard_regset, &intersection_set)){
               /* CL3 allocatable hard register set is inside of
               intersection of allocatable hard register sets
               of CL1 and CL2.  */
               if (important_class_p[cl3]){
                  temp_set2 = (mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[mtcsIraInt->x_ira_reg_class_intersect[cl1][cl2]]);
                  temp_set2 &= ~no_unit_alloc_regs;
                  if (! mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&self->temp_hard_regset, &temp_set2)
                  /* If the allocatable hard register sets are
                  the same, prefer GENERAL_REGS or the
                  smallest class for debugging
                  purposes.  */
                  || (self->temp_hard_regset == temp_set2
                  && (cl3 == mtcs_reg_get_general_regs/*!GENERAL_REGS*/(mtcsReg)
                  || ((mtcsIraInt->x_ira_reg_class_intersect[cl1][cl2]
                  != mtcs_reg_get_general_regs/*!GENERAL_REGS*/(mtcsReg))
                  && mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(
                  &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl3],
                  &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/
                  [(int)mtcsIraInt->x_ira_reg_class_intersect[cl1][cl2]])))))
                     mtcsIraInt->x_ira_reg_class_intersect[cl1][cl2] = (enum reg_class) cl3;
               }
               temp_set2 = (mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[self->x_ira_reg_class_subset[cl1][cl2]]
                                                                                          & ~no_unit_alloc_regs);
               if (! mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&self->temp_hard_regset, &temp_set2)
               /* Ignore unavailable hard registers and prefer
               smallest class for debugging purposes.  */
               || (self->temp_hard_regset == temp_set2
               && mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/
               (&mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl3],
               &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/
               [(int) self->x_ira_reg_class_subset[cl1][cl2]])))
                  self->x_ira_reg_class_subset[cl1][cl2] = (enum reg_class) cl3;
            }
            if (important_class_p[cl3]  && mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&self->temp_hard_regset, &union_set)){
               /* CL3 allocatable hard register set is inside of
               union of allocatable hard register sets of CL1
               and CL2.  */
               temp_set2 = (mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[mtcsIraInt->x_ira_reg_class_subunion[cl1][cl2]]
               & ~no_unit_alloc_regs);
               if (mtcsIraInt->x_ira_reg_class_subunion[cl1][cl2] == NO_REGS
               || (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&temp_set2, &self->temp_hard_regset)
               && (temp_set2 != self->temp_hard_regset
               || cl3 == mtcs_reg_get_general_regs/*!GENERAL_REGS*/(mtcsReg)
               /* If the allocatable hard register sets are the
               same, prefer GENERAL_REGS or the smallest
               class for debugging purposes.  */
               || (mtcsIraInt->x_ira_reg_class_subunion[cl1][cl2] != mtcs_reg_get_general_regs/*!GENERAL_REGS*/(mtcsReg)
               && mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/
               (&mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl3],
               &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/
               [(int) mtcsIraInt->x_ira_reg_class_subunion[cl1][cl2]])))))
                  mtcsIraInt->x_ira_reg_class_subunion[cl1][cl2] = (enum reg_class) cl3;
            }
            if (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&union_set, &self->temp_hard_regset)){
               /* CL3 allocatable hard register set contains union
               of allocatable hard register sets of CL1 and
               CL2.  */
               temp_set2 = (mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[mtcsIraInt->x_ira_reg_class_superunion[cl1][cl2]]
               & ~no_unit_alloc_regs);
               if (mtcsIraInt->x_ira_reg_class_superunion[cl1][cl2] == NO_REGS
               || (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&self->temp_hard_regset, &temp_set2)

               && (temp_set2 != self->temp_hard_regset
               || cl3 == mtcs_reg_get_general_regs/*!GENERAL_REGS*/(mtcsReg)
               /* If the allocatable hard register sets are the
               same, prefer GENERAL_REGS or the smallest
               class for debugging purposes.  */
               || (mtcsIraInt->x_ira_reg_class_superunion[cl1][cl2] != mtcs_reg_get_general_regs/*!GENERAL_REGS*/(mtcsReg)
               && mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/
               (&mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl3],
               &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/
               [(int) mtcsIraInt->x_ira_reg_class_superunion[cl1][cl2]])))))
                  mtcsIraInt->x_ira_reg_class_superunion[cl1][cl2] = (enum reg_class) cl3;
            }
         }
      }
   }
}

/* Output all uniform and important classes into file F.  */
static void print_uniform_and_important_classes (MtcsIra *self,FILE *f)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int nRegClasses=mtcs_reg_get_n_reg_classes/*!N_REG_CLASSES*/(mtcsReg);
   MtcsRegClass *mtcsRegClass =mtcs_reg_get_reg_class(mtcsReg);
   int i, cl;
   fprintf (f, "Uniform classes:\n");
   for (cl = 0; cl < nRegClasses/*!N_REG_CLASSES*/; cl++)
      if (mtcsIraInt->x_ira_uniform_class_p[cl])
         fprintf (f, " %s", mtcsRegClass/*!reg_class_names*/[cl].name);
   fprintf (f, "\nImportant classes:\n");
   for (i = 0; i < mtcsIraInt->x_ira_important_classes_num; i++)
      fprintf (f, " %s", mtcsRegClass/*!reg_class_names*/[mtcsIraInt->x_ira_important_classes[i]].name);
   fprintf (f, "\n");
}

/* Output all possible allocno or pressure classes and their
   translation map into file F.  */
static void print_translated_classes (MtcsIra *self,FILE *f, bool pressure_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int nRegClasses=mtcs_reg_get_n_reg_classes/*!N_REG_CLASSES*/(mtcsReg);
   MtcsRegClass *mtcsRegClass =mtcs_reg_get_reg_class(mtcsReg);
   int classes_num = (pressure_p ? self->x_ira_pressure_classes_num : self->x_ira_allocno_classes_num);
   enum reg_class *classes = (pressure_p ? self->x_ira_pressure_classes : self->x_ira_allocno_classes);
   enum reg_class *class_translate = (pressure_p ? self->x_ira_pressure_class_translate : self->x_ira_allocno_class_translate);
   int i;

   fprintf (f, "%s classes:\n", pressure_p ? "Pressure" : "Allocno");
   for (i = 0; i < classes_num; i++)
      fprintf (f, " %s", mtcsRegClass/*!reg_class_names*/[classes[i]].name);
   fprintf (f, "\nClass translation:\n");
   for (i = 0; i < nRegClasses/*!N_REG_CLASSES*/; i++)
      fprintf (f, " %s -> %s\n", mtcsRegClass/*!reg_class_names*/[i].name,mtcsRegClass/*!reg_class_names*/[class_translate[i]].name);
}

/* Output all possible allocno and translation classes and the
   translation maps into stderr.  */
//原型 ira_debug_allocno_classes ira-int.h ira.cc
void mtcs_ira_debug_allocno_classes (MtcsIra *self)
{
  print_uniform_and_important_classes(self,stderr);
  print_translated_classes(self,stderr, false);
  print_translated_classes(self,stderr, true);
}

/* Set up different arrays concerning class subsets, allocno and
   important classes.  */
static void find_reg_classes (MtcsIra *self)
{
  setup_allocno_and_important_classes(self);
  setup_class_translate(self);
  reorder_important_classes(self);
  setup_reg_class_relations(self);
}

/* Set up the array above.  */
static void setup_hard_regno_aclass (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int nRegClasses=mtcs_reg_get_n_reg_classes/*!N_REG_CLASSES*/(mtcsReg);
   int i;
   for (i = 0; i < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i++){
#if 1
      self->x_ira_hard_regno_allocno_class[i] = (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&no_unit_alloc_regs, i)
      ? NO_REGS : self->x_ira_allocno_class_translate[mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,i)]);
#else
      int j;
      enum reg_class cl;
      self->x_ira_hard_regno_allocno_class[i] = NO_REGS;
      for (j = 0; j < self->x_ira_allocno_classes_num; j++){
         cl = self->x_ira_allocno_classes[j];
         if (mtcsIraInt->x_ira_class_hard_reg_index[cl][i] >= 0){
            self->x_ira_hard_regno_allocno_class[i] = cl;
            break;
         }
      }
#endif
   }
}

/* Form IRA_REG_CLASS_MAX_NREGS and IRA_REG_CLASS_MIN_NREGS maps.  */
static void setup_reg_class_nregs (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int nRegClasses=mtcs_reg_get_n_reg_classes/*!N_REG_CLASSES*/(mtcsReg);
   int i, cl, cl2, m;
   for (m = 0; m < mtcs_mode_get_max_number/*!MAX_MACHINE_MODE*/(mtcsMode); m++){
      for (cl = 0; cl < nRegClasses/*!N_REG_CLASSES*/; cl++)
         self->x_ira_reg_class_max_nregs[cl][m]
         = self->x_ira_reg_class_min_nregs[cl][m]
         = mtcsTarget/*!targetm.class_max_nregs*/->class_max_nregs(mtcsTarget,(reg_class_t) cl, (machine_mode) m);
      for (cl = 0; cl < nRegClasses/*!N_REG_CLASSES*/; cl++)
         for (i = 0;(cl2 = alloc_reg_class_subclasses[cl][i]) != mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg);i++)
            if (self->x_ira_reg_class_min_nregs[cl2][m] < self->x_ira_reg_class_min_nregs[cl][m])
               self->x_ira_reg_class_min_nregs[cl][m] = self->x_ira_reg_class_min_nregs[cl2][m];
   }
}

/* Set up IRA_PROHIBITED_CLASS_MODE_REGS, IRA_EXCLUDE_CLASS_MODE_REGS, and
   IRA_CLASS_SINGLETON.  This function is called once IRA_CLASS_HARD_REGS has
   been initialized.  */
static void setup_prohibited_and_exclude_class_mode_regs (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int nRegClasses=mtcs_reg_get_n_reg_classes/*!N_REG_CLASSES*/(mtcsReg);
   int numMachineModes=mtcs_mode_get_number/*!NUM_MACHINE_MODES*/(mtcsMode);
   int j, k, hard_regno, cl, last_hard_regno, count;

   for (cl = (int) nRegClasses/*!N_REG_CLASSES*/ - 1; cl >= 0; cl--){
      self->temp_hard_regset = mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl] & ~no_unit_alloc_regs;
      for (j = 0; j < numMachineModes/*!NUM_MACHINE_MODES*/; j++){
         count = 0;
         last_hard_regno = -1;
         mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&self->x_ira_prohibited_class_mode_regs[cl][j]);
         mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&self->x_ira_exclude_class_mode_regs[cl][j]);
         for (k = self->x_ira_class_hard_regs_num[cl] - 1; k >= 0; k--){
            hard_regno = self->x_ira_class_hard_regs[cl][k];
            if (!mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,hard_regno, (machine_mode) j))
               mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&self->x_ira_prohibited_class_mode_regs[cl][j],hard_regno);
            else if (mtcs_reg_in_hard_reg_set_p/*!in_hard_reg_set_p*/(mtcsReg,&self->temp_hard_regset,(machine_mode) j, hard_regno)){
               last_hard_regno = hard_regno;
               count++;
            }else{
               mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&self->x_ira_exclude_class_mode_regs[cl][j], hard_regno);
            }
         }
         self->x_ira_class_singleton[cl][j] = (count == 1 ? last_hard_regno : -1);
      }
   }
}

/* Clarify IRA_PROHIBITED_CLASS_MODE_REGS by excluding hard registers
   spanning from one register pressure class to another one.  It is
   called after defining the pressure classes.  */
static void clarify_prohibited_class_mode_regs (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int nRegClasses=mtcs_reg_get_n_reg_classes/*!N_REG_CLASSES*/(mtcsReg);
   int numMachineModes=mtcs_mode_get_number/*!NUM_MACHINE_MODES*/(mtcsMode);

   int j, k, hard_regno, cl, pclass, nregs;

   for (cl = (int) nRegClasses/*!N_REG_CLASSES*/ - 1; cl >= 0; cl--)
      for (j = 0; j < numMachineModes/*!NUM_MACHINE_MODES*/; j++){
         mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&mtcsIraInt->x_ira_useful_class_mode_regs[cl][j]);
         for (k = self->x_ira_class_hard_regs_num[cl] - 1; k >= 0; k--){
            hard_regno = self->x_ira_class_hard_regs[cl][k];
            if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&self->x_ira_prohibited_class_mode_regs[cl][j], hard_regno))
               continue;
            nregs = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,hard_regno, (machine_mode) j);
            if (hard_regno + nregs > mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)){
               mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&self->x_ira_prohibited_class_mode_regs[cl][j],hard_regno);
               continue;
            }
            pclass = self->x_ira_pressure_class_translate[mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,hard_regno)];
            for (nregs-- ;nregs >= 0; nregs--)
               if (((enum reg_class) pclass
                     != self->x_ira_pressure_class_translate[mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg, (hard_regno + nregs))])){
                  mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&self->x_ira_prohibited_class_mode_regs[cl][j],hard_regno);
                  break;
               }
            if (!mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&self->x_ira_prohibited_class_mode_regs[cl][j],hard_regno))
               mtcs_reg_add_to_hard_reg_set/*!add_to_hard_reg_set*/(mtcsReg,
                     &mtcsIraInt->x_ira_useful_class_mode_regs[cl][j],(machine_mode) j, hard_regno);
         }
      }
}

/* Allocate and initialize IRA_REGISTER_MOVE_COST, IRA_MAY_MOVE_IN_COST
   and IRA_MAY_MOVE_OUT_COST for MODE.  */
//原型 ira_init_register_move_cost ira-int.h ira.cc
void mtcs_ira_init_register_move_cost (MtcsIra *self,machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsReload *mtcsReload=mtcs_target_get_reload(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int nRegClasses=mtcs_reg_get_n_reg_classes/*!N_REG_CLASSES*/(mtcsReg);
   int numMachineModes=mtcs_mode_get_number/*!NUM_MACHINE_MODES*/(mtcsMode);
   //原来是 static
   unsigned short last_move_cost[nRegClasses/*!N_REG_CLASSES*/][nRegClasses/*!N_REG_CLASSES*/];
   //static unsigned short last_move_cost[100/*!N_REG_CLASSES*/][100/*!N_REG_CLASSES*/];

   bool all_match = true;
   unsigned int i, cl1, cl2;
   HardRegSet /*!HARD_REG_SET*/ ok_regs = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};

   ira_assert (mtcsIraInt->x_ira_register_move_cost[mode] == NULL
               && mtcsIraInt->x_ira_may_move_in_cost[mode] == NULL
               && mtcsIraInt->x_ira_may_move_out_cost[mode] == NULL);
   mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&ok_regs);
   for (i = 0; i < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i++)
      if (mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,i, mode))
         mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&ok_regs, i);

   /* Note that we might be asked about the move costs of modes that
   cannot be stored in any hard register, for example if an inline
   asm tries to create a register operand with an impossible mode.
   We therefore can't assert have_regs_of_mode[mode] here.  */
   for (cl1 = 0; cl1 < nRegClasses/*!N_REG_CLASSES*/; cl1++)
      for (cl2 = 0; cl2 < nRegClasses/*!N_REG_CLASSES*/; cl2++){
         int cost;
         if (!mtcs_reg_hard_reg_set_intersect_p/*!hard_reg_set_intersect_p*/(&ok_regs,
         &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl1])
         || !mtcs_reg_hard_reg_set_intersect_p/*!hard_reg_set_intersect_p*/(&ok_regs,
         &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl2])){
            if ((self->x_ira_reg_class_max_nregs[cl1][mode] > self->x_ira_class_hard_regs_num[cl1])
            || (self->x_ira_reg_class_max_nregs[cl2][mode]  > self->x_ira_class_hard_regs_num[cl2]))
               cost = 65535;
            else
               cost = (self->x_ira_memory_move_cost[mode][cl1][0] + self->x_ira_memory_move_cost[mode][cl2][1]) * 2;
         }else{
            cost =mtcs_reload_register_move_cost/*!register_move_cost*/(mtcsReload,mode, (enum reg_class) cl1, (enum reg_class) cl2);
            ira_assert (cost < 65535);
         }
         all_match &= (last_move_cost[cl1][cl2] == cost);
         last_move_cost[cl1][cl2] = cost;
      }

   if (all_match && last_mode_for_init_move_cost != -1){
      mtcsIraInt->x_ira_register_move_cost[mode] = mtcsIraInt->x_ira_register_move_cost[last_mode_for_init_move_cost];
      mtcsIraInt->x_ira_may_move_in_cost[mode] = mtcsIraInt->x_ira_may_move_in_cost[last_mode_for_init_move_cost];
      mtcsIraInt->x_ira_may_move_out_cost[mode] = mtcsIraInt->x_ira_may_move_out_cost[last_mode_for_init_move_cost];
      return;
   }
   last_mode_for_init_move_cost = mode;
   mtcsIraInt->x_ira_register_move_cost[mode] = XNEWVEC (ira_move_table, nRegClasses/*!N_REG_CLASSES*/);
   mtcsIraInt->x_ira_may_move_in_cost[mode] = XNEWVEC (ira_move_table, nRegClasses/*!N_REG_CLASSES*/);
   mtcsIraInt->x_ira_may_move_out_cost[mode] = XNEWVEC (ira_move_table, nRegClasses/*!N_REG_CLASSES*/);
   for (cl1 = 0; cl1 < nRegClasses/*!N_REG_CLASSES*/; cl1++)
      for (cl2 = 0; cl2 < nRegClasses/*!N_REG_CLASSES*/; cl2++){
         int cost;
         enum reg_class *p1, *p2;

         if (last_move_cost[cl1][cl2] == 65535){
            mtcsIraInt->x_ira_register_move_cost[mode][cl1][cl2] = 65535;
            mtcsIraInt->x_ira_may_move_in_cost[mode][cl1][cl2] = 65535;
            mtcsIraInt->x_ira_may_move_out_cost[mode][cl1][cl2] = 65535;
         }else{
            cost = last_move_cost[cl1][cl2];
            enum reg_class temp2=(enum reg_class)mtcsReg->hardRegs.x_reg_class_subclasses/*reg_class_subclasses*/[cl2][0];
            for (p2 = &temp2; *p2 != mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg); p2++)
               if (self->x_ira_class_hard_regs_num[*p2] > 0 && (self->x_ira_reg_class_max_nregs[*p2][mode]
                                     <= self->x_ira_class_hard_regs_num[*p2]))
                  cost = MAX (cost, mtcsIraInt->x_ira_register_move_cost[mode][cl1][*p2]);
            enum reg_class temp1=(enum reg_class)mtcsReg->hardRegs.x_reg_class_subclasses/*reg_class_subclasses*/[cl1][0];
            for (p1 = &temp1; *p1 != mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg); p1++)
               if (self->x_ira_class_hard_regs_num[*p1] > 0 && (self->x_ira_reg_class_max_nregs[*p1][mode]
                                     <= self->x_ira_class_hard_regs_num[*p1]))
                  cost = MAX (cost, mtcsIraInt->x_ira_register_move_cost[mode][*p1][cl2]);

            ira_assert (cost <= 65535);
            mtcsIraInt->x_ira_register_move_cost[mode][cl1][cl2] = cost;

            if (self->x_ira_class_subset_p[cl1][cl2])
               mtcsIraInt->x_ira_may_move_in_cost[mode][cl1][cl2] = 0;
            else
               mtcsIraInt->x_ira_may_move_in_cost[mode][cl1][cl2] = cost;

            if (self->x_ira_class_subset_p[cl2][cl1])
               mtcsIraInt->x_ira_may_move_out_cost[mode][cl1][cl2] = 0;
            else
               mtcsIraInt->x_ira_may_move_out_cost[mode][cl1][cl2] = cost;
         }
      }
}

/* This is called once during compiler work.  It sets up
   different arrays whose values don't depend on the compiled
   function.  */
//原型 ira_init_once ira.h ira.cc
void mtcs_ira_init_once (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraCosts *mtcsIraCosts = mtcs_ira_mgr_get_costs(mtcsIraMgr);

   mtcs_ira_costs_init_costs_once/*!ira_init_costs_once*/(mtcsIraCosts);
   //lra_init_once ();//还未实现 lra
   self->ira_use_lra_p = mtcsTarget/*!targetm.lra_p*/->lra_p(mtcsTarget);
}

/* 原型 target_ira_int::free_register_move_costs ira-int.h ira.cc  被mtcs_ira_int_free_register_move_costs替换*/

/* 原型 target_ira_int::~target_ira_int   ira-int.h ira.cc  被 mtcs_ira_int_free 替换*/

/* This is called every time when register related information is
   changed.  */
//原型 ira_init ira.h ira.cc
void mtcs_ira_init (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsReload *mtcsReload=mtcs_target_get_reload(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   mtcs_ira_int_free_register_move_costs/*!this_target_ira_int->free_register_move_costs*/(mtcsIraInt);
   setup_reg_mode_hard_regset(self);
   setup_alloc_regs(self,mtcsOptionsItem->x_flag_omit_frame_pointer != 0);
   setup_class_subset_and_memory_move_costs(self);
   setup_reg_class_nregs(self);
   setup_prohibited_and_exclude_class_mode_regs(self);
   find_reg_classes(self);
   clarify_prohibited_class_mode_regs(self);
   setup_hard_regno_aclass(self);
   ira_init_costs ();
}

/* Set up IRA_PROHIBITED_MODE_MOVE_REGS.  */
static void setup_prohibited_mode_move_regs (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int i, j;
   rtx test_reg1, test_reg2, move_pat;
   rtx_insn *move_insn;

   int lastVirtualRegister = mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg);
   int numMachineModes= mtcs_mode_get_number/*!NUM_MACHINE_MODES*/(mtcsMode);

   if (ira_prohibited_mode_move_regs_initialized_p)
      return;
   ira_prohibited_mode_move_regs_initialized_p = true;
   test_reg1 = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mtcsMode->word_mode, lastVirtualRegister/*!LAST_VIRTUAL_REGISTER*/ + 1);
   test_reg2 = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mtcsMode->word_mode, lastVirtualRegister/*!LAST_VIRTUAL_REGISTER*/ + 2);
   move_pat = gen_rtx_SET (test_reg1, test_reg2);
   move_insn = gen_rtx_INSN (VOIDmode, 0, 0, 0, move_pat, 0, -1, 0);
   for (i = 0; i < numMachineModes/*!NUM_MACHINE_MODES*/; i++){
      mtcs_reg_set_hard_reg_set/*!SET_HARD_REG_SET*/(&mtcsIraInt->x_ira_prohibited_mode_move_regs[i]);
      for (j = 0; j < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); j++){
         if (!mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,j, (machine_mode) i))
            continue;
         mtcs_rtl_set_mode_and_regno/*!set_mode_and_regno*/(mtcsRTL,test_reg1, (machine_mode) i, j);
         mtcs_rtl_set_mode_and_regno/*!set_mode_and_regno*/(mtcsRTL,test_reg2, (machine_mode) i, j);
         INSN_CODE (move_insn) = -1;
         mtcs_recog_recog_memoized/*!recog_memoized*/(mtcsRecog,move_insn);
         if (INSN_CODE (move_insn) < 0)
            continue;
         mtcs_recog_extract_insn/*!extract_insn*/(mtcsRecog,move_insn);
         /* We don't know whether the move will be in code that is optimized
         for size or speed, so consider all enabled alternatives.  */
         if (! mtcs_recog_constrain_operands/*!constrain_operands*/(mtcsRecog,1,
               mtcs_recog_get_enabled_alternatives/*!get_enabled_alternatives*/(mtcsRecog,move_insn)))
            continue;
         mtcs_reg_clear_hard_reg_bit/*!CLEAR_HARD_REG_BIT*/(mtcsReg, &mtcsIraInt->x_ira_prohibited_mode_move_regs[i], j);
      }
   }
}



/* Extract INSN and return the set of alternatives that we should consider.
   This excludes any alternatives whose constraints are obviously impossible
   to meet (e.g. because the constraint requires a constant and the operand
   is nonconstant).  It also excludes alternatives that are bound to need
   a spill or reload, as long as we have other alternatives that match
   exactly.  */
//原型 ira_setup_alts ira-int.h ira.cc
alternative_mask mtcs_ira_setup_alts (MtcsIra *self ,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int nop, nalt;
   bool curr_swapped;
   const char *p;
   int commutative = -1;

   mtcs_recog_extract_insn/*!extract_insn*/(mtcsRecog,insn);
   mtcs_recog_preprocess_constraints/*!preprocess_constraints*/(mtcsRecog,insn);
   alternative_mask preferred = mtcs_recog_get_preferred_alternatives/*!get_preferred_alternatives*/(mtcsRecog,insn);
   alternative_mask alts = 0;
   alternative_mask exact_alts = 0;
   /* Check that the hard reg set is enough for holding all
   alternatives.  It is hard to imagine the situation when the
   assertion is wrong.  */
   ira_assert (mtcsRecog->recog_data.n_alternatives <= (int) MAX (sizeof (HARD_REG_ELT_TYPE) * CHAR_BIT,
         mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)));
   for (nop = 0; nop < mtcsRecog->recog_data.n_operands; nop++)
      if (mtcsRecog->recog_data.constraints[nop][0] == '%'){
         commutative = nop;
         break;
      }

   for (curr_swapped = false;; curr_swapped = true){
      for (nalt = 0; nalt < mtcsRecog->recog_data.n_alternatives; nalt++){
         if (!TEST_BIT (preferred, nalt) || TEST_BIT (exact_alts, nalt))
            continue;

         const struct mtcs_operand_alternative *op_alt = &mtcsRecog->recog_op_alt[nalt * mtcsRecog->recog_data.n_operands];
         int this_reject = 0;
         for (nop = 0; nop < mtcsRecog->recog_data.n_operands; nop++){
            int c, len;

            this_reject += op_alt[nop].reject;

            rtx op = mtcsRecog->recog_data.operand[nop];
            p = op_alt[nop].constraint;
            if (*p == 0 || *p == ',')
               continue;

            bool win_p = false;
            do
               switch (c = *p, len = mtcs_preds_insn_constraint_len/*!CONSTRAINT_LEN*/(mtcsPreds,c, p), c){
               case '#':
               case ',':
                  c = '\0';
               /* FALLTHRU */
               case '\0':
                  len = 0;
                  break;

               case '%':
                  /* The commutative modifier is handled above.  */
                  break;

               case '0':  case '1':  case '2':  case '3':  case '4':
               case '5':  case '6':  case '7':  case '8':  case '9':
               {
                  char *end;
                  unsigned long dup = strtoul (p, &end, 10);
                  rtx other = mtcsRecog->recog_data.operand[dup];
                  len = end - p;
                  if (MEM_P (other)? rtx_equal_p (other, op): REG_P (op) || SUBREG_P (op))
                     goto op_success;
                  win_p = true;
               }
                  break;

               case 'g':
                  goto op_success;
                  break;

               default:
               {
                  enum constraint_num cn = mtcs_preds_lookup_constraint/*!lookup_constraint*/(mtcsPreds,p);
                  rtx mem = NULL;
                  switch (mtcs_preds_get_constraint_type/*!get_constraint_type*/(mtcsPreds,cn)){
                     case CT_REGISTER:
                        if (mtcs_preds_reg_class_for_constraint/*!reg_class_for_constraint*/(mtcsPreds,cn) != NO_REGS){
                           if (REG_P (op) || SUBREG_P (op))
                              goto op_success;
                           win_p = true;
                        }
                        break;

                     case CT_CONST_INT:
                        if (CONST_INT_P (op)  && (mtcs_preds_insn_const_int_ok_for_constraint
                              /*!insn_const_int_ok_for_constraint*/(mtcsPreds,INTVAL (op), cn)))
                           goto op_success;
                        break;

                     case CT_ADDRESS:
                        goto op_success;

                     case CT_MEMORY:
                     case CT_RELAXED_MEMORY:
                        mem = op;
                     /* Fall through.  */
                     case CT_SPECIAL_MEMORY:
                        if (!mem)
                           mem = extract_mem_from_operand (op);
                        if (MEM_P (mem))
                           goto op_success;
                        win_p = true;
                        break;

                     case CT_FIXED_FORM:
                        if (mtcs_preds_constraint_satisfied_p/*!constraint_satisfied_p*/(mtcsPreds,op, cn))
                           goto op_success;
                        break;
                  }
                  break;
               }
            }while (p += len, c);

            if (!win_p)
               break;
            /* We can make the alternative match by spilling a register
            to memory or loading something into a register.  Count a
            cost of one reload (the equivalent of the '?' constraint).  */
            this_reject += 6;
op_success:
            ;
         }

         if (nop >= mtcsRecog->recog_data.n_operands){
            alts |= ALTERNATIVE_BIT (nalt);
            if (this_reject == 0)
               exact_alts |= ALTERNATIVE_BIT (nalt);
         }
      }
      if (commutative < 0)
         break;
      /* Swap forth and back to avoid changing recog_data.  */
      std::swap ( mtcsRecog->recog_data.operand[commutative],mtcsRecog->recog_data.operand[commutative + 1]);
      if (curr_swapped)
         break;
   }
   return exact_alts ? exact_alts : alts;
}

/* Return the number of the output non-early clobber operand which
   should be the same in any case as operand with number OP_NUM (or
   negative value if there is no such operand).  ALTS is the mask
   of alternatives that we should consider.  SINGLE_INPUT_OP_HAS_CSTR_P
   should be set in this function, it indicates whether there is only
   a single input operand which has the matching constraint on the
   output operand at the position specified in return value.  If the
   pattern allows any one of several input operands holds the matching
   constraint, it's set as false, one typical case is destructive FMA
   instruction on target rs6000.  Note that for a non-NO_REG preferred
   register class with no free register move copy, if the parameter
   PARAM_IRA_CONSIDER_DUP_IN_ALL_ALTS is set to one, this function
   will check all available alternatives for matching constraints,
   even if it has found or will find one alternative with non-NO_REG
   regclass, it can respect more cases with matching constraints.  If
   PARAM_IRA_CONSIDER_DUP_IN_ALL_ALTS is set to zero,
   SINGLE_INPUT_OP_HAS_CSTR_P is always true, it will stop to find
   matching constraint relationship once it hits some alternative with
   some non-NO_REG regclass.  */
//原型 ira_get_dup_out_num ira-int.h ira.cc
int mtcs_ira_get_dup_out_num (MtcsIra *self,int op_num, alternative_mask alts,
           bool &single_input_op_has_cstr_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int curr_alt, c, original;
   bool ignore_p, use_commut_op_p;
   const char *str;

   if (op_num < 0 || mtcsRecog->recog_data.n_alternatives == 0)
      return -1;
   /* We should find duplications only for input operands.  */
   if (mtcsRecog->recog_data.operand_type[op_num] != OP_IN)
      return -1;
   str = mtcsRecog->recog_data.constraints[op_num];
   use_commut_op_p = false;
   single_input_op_has_cstr_p = true;

   rtx op = mtcsRecog->recog_data.operand[op_num];
   int op_regno = reg_or_subregno (op);
   enum reg_class op_pref_cl = mtcs_reg_reg_preferred_class/*!reg_preferred_class*/(mtcsReg,op_regno);
   machine_mode op_mode = GET_MODE (op);

   mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(self,op_mode);
   /* If the preferred regclass isn't NO_REG, continue to find the matching
   constraint in all available alternatives with preferred regclass, even
   if we have found or will find one alternative whose constraint stands
   for a REG (non-NO_REG) regclass.  Note that it would be fine not to
   respect matching constraint if the register copy is free, so exclude
   it.  */
   bool respect_dup_despite_reg_cstr = mtcsOptionsItem->x_param_ira_consider_dup_in_all_alts
                        && op_pref_cl != NO_REGS
                        && mtcsIraInt->x_ira_register_move_cost[op_mode][op_pref_cl][op_pref_cl] > 0;

   /* Record the alternative whose constraint uses the same regclass as the
   preferred regclass, later if we find one matching constraint for this
   operand with preferred reclass, we will visit these recorded
   alternatives to check whether if there is one alternative in which no
   any INPUT operands have one matching constraint same as our candidate.
   If yes, it means there is one alternative which is perfectly fine
   without satisfying this matching constraint.  If no, it means in any
   alternatives there is one other INPUT operand holding this matching
   constraint, it's fine to respect this matching constraint and further
   create this constraint copy since it would become harmless once some
   other takes preference and it's interfered.  */
   alternative_mask pref_cl_alts;

   for (;;){
      pref_cl_alts = 0;

      for (curr_alt = 0, ignore_p = !TEST_BIT (alts, curr_alt), original = -1;;){
         c = *str;
         if (c == '\0')
            break;
         if (c == '#')
            ignore_p = true;
         else if (c == ','){
            curr_alt++;
            ignore_p = !TEST_BIT (alts, curr_alt);
         }else if (! ignore_p)
            switch (c){
               case 'g':
                  goto fail;
               default:
               {
                  enum constraint_num cn = mtcs_preds_lookup_constraint/*!lookup_constraint*/(mtcsPreds,str);
                  enum reg_class cl = mtcs_preds_reg_class_for_constraint/*!reg_class_for_constraint*/(mtcsPreds,cn);
                  if (cl != NO_REGS && !mtcsTarget/*!targetm.class_likely_spilled_p*/->class_likely_spilled_p(mtcsTarget,cl)){
                     if (respect_dup_despite_reg_cstr){
                        /* If it's free to move from one preferred class to
                        the one without matching constraint, it doesn't
                        have to respect this constraint with costs.  */
                        if (cl != op_pref_cl
                        && (mtcsIraInt->x_ira_reg_class_intersect[cl][op_pref_cl] != NO_REGS)
                        && (mtcsIraInt->x_ira_may_move_in_cost[op_mode][op_pref_cl][cl] == 0))
                           goto fail;
                        else if (cl == op_pref_cl)
                           pref_cl_alts |= ALTERNATIVE_BIT (curr_alt);
                     }else
                        goto fail;
                  }
                  if (mtcs_preds_constraint_satisfied_p/*!constraint_satisfied_p*/(mtcsPreds,op, cn))
                     goto fail;
                  break;
               }

               case '0': case '1': case '2': case '3': case '4':
               case '5': case '6': case '7': case '8': case '9':
               {
                  char *end;
                  int n = (int) strtoul (str, &end, 10);
                  str = end;
                  if (original != -1 && original != n)
                     goto fail;
                  gcc_assert (n < mtcsRecog->recog_data.n_operands);
                  if (respect_dup_despite_reg_cstr){
                     const mtcs_operand_alternative *op_alt = &mtcsRecog->recog_op_alt[curr_alt * mtcsRecog->recog_data.n_operands];
                     /* Only respect the one with preferred rclass, without
                     respect_dup_despite_reg_cstr it's possible to get
                     one whose regclass isn't preferred first before,
                     but it would fail since there should be other
                     alternatives with preferred regclass.  */
                     if (op_alt[n].cl == op_pref_cl)
                        original = n;
                  }else
                     original = n;
                  continue;
               }
            }
         str += mtcs_preds_insn_constraint_len/*!CONSTRAINT_LEN*/(mtcsPreds,c, str);
      }
      if (original == -1)
         goto fail;
      if (mtcsRecog->recog_data.operand_type[original] == OP_OUT){
         if (pref_cl_alts == 0)
            return original;
         /* Visit these recorded alternatives to check whether
         there is one alternative in which no any INPUT operands
         have one matching constraint same as our candidate.
         Give up this candidate if so.  */
         int nop, nalt;
         for (nalt = 0; nalt < mtcsRecog->recog_data.n_alternatives; nalt++){
            if (!TEST_BIT (pref_cl_alts, nalt))
               continue;
            const struct mtcs_operand_alternative *op_alt = &mtcsRecog->recog_op_alt[nalt * mtcsRecog->recog_data.n_operands];
            bool dup_in_other = false;
            for (nop = 0; nop < mtcsRecog->recog_data.n_operands; nop++){
               if (mtcsRecog->recog_data.operand_type[nop] != OP_IN)
                  continue;
               if (nop == op_num)
                  continue;
               if (op_alt[nop].matches == original){
                  dup_in_other = true;
                  break;
               }
            }
            if (!dup_in_other)
               return -1;
         }
         single_input_op_has_cstr_p = false;
         return original;
      }
fail:
      if (use_commut_op_p)
         break;
      use_commut_op_p = true;
      if (mtcsRecog->recog_data.constraints[op_num][0] == '%')
         str = mtcsRecog->recog_data.constraints[op_num + 1];
      else if (op_num > 0 && mtcsRecog->recog_data.constraints[op_num - 1][0] == '%')
         str = mtcsRecog->recog_data.constraints[op_num - 1];
      else
         break;
   }//end    for (;;){
   return -1;
}



/* Search forward to see if the source register of a copy insn dies
   before either it or the destination register is modified, but don't
   scan past the end of the basic block.  If so, we can replace the
   source with the destination and let the source die in the copy
   insn.

   This will reduce the number of registers live in that range and may
   enable the destination and the source coalescing, thus often saving
   one register in addition to a register-register copy.  */
static void decrease_live_ranges_number (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   basic_block bb;
   rtx_insn *insn;
   rtx set, src, dest, dest_death, note;
   rtx_insn *p, *q;
   int sregno, dregno;

   int stackPointerRegnum = mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg);
   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   if (! mtcsOptionsItem->x_flag_expensive_optimizations)
      return;
   FILE *dumpFile= mtcsIraGlobal->ira_dump_file;
   if (dumpFile)
      fprintf (dumpFile, "Starting decreasing number of live ranges...\n");

   FOR_EACH_BB_FN (bb, cfun)
      FOR_BB_INSNS (bb, insn){
         set = single_set (insn);
         if (! set)
            continue;
         src = SET_SRC (set);
         dest = SET_DEST (set);
         if (! REG_P (src) || ! REG_P (dest) || find_reg_note (insn, REG_DEAD, src))
            continue;
         sregno = REGNO (src);
         dregno = REGNO (dest);

         /* We don't want to mess with hard regs if register classes
         are small.  */
         if (sregno == dregno
         || (mtcsTarget/*!targetm.small_register_classes_for_mode_p*/->small_register_classes_for_mode_p(mtcsTarget,GET_MODE (src))
         && (sregno < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/
         || dregno < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/))
         /* We don't see all updates to SP if they are in an
         auto-inc memory reference, so we must disallow this
         optimization on them.  */
         || sregno == stackPointerRegnum/*!STACK_POINTER_REGNUM*/
         || dregno == stackPointerRegnum/*!STACK_POINTER_REGNUM*/)
            continue;

         dest_death = NULL_RTX;

         for (p = NEXT_INSN (insn); p; p = NEXT_INSN (p)){
            if (! INSN_P (p))
               continue;
            if (BLOCK_FOR_INSN (p) != bb)
               break;

            if (mtcs_rtlanal_reg_set_p/*!reg_set_p*/(mtcsRtlanal,src, p) || mtcs_rtlanal_reg_set_p/*!reg_set_p*/(mtcsRtlanal,dest, p)
            /* If SRC is an asm-declared register, it must not be
            replaced in any asm.  Unfortunately, the REG_EXPR
            tree for the asm variable may be absent in the SRC
            rtx, so we can't check the actual register
            declaration easily (the asm operand will have it,
            though).  To avoid complicating the test for a rare
            case, we just don't perform register replacement
            for a hard reg mentioned in an asm.  */
            || (sregno < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/
            && asm_noperands (PATTERN (p)) >= 0
            && mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,src, PATTERN (p)))
            /* Don't change hard registers used by a call.  */
            || (CALL_P (p) && sregno < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/
            && mtcs_rtlanal_find_reg_fusage/*!find_reg_fusage*/(mtcsRtlanal,p, USE, src))
            /* Don't change a USE of a register.  */
            || (GET_CODE (PATTERN (p)) == USE
            && mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,src, XEXP (PATTERN (p), 0))))
               break;

            /* See if all of SRC dies in P.  This test is slightly
            more conservative than it needs to be.  */
            if ((note = find_regno_note (p, REG_DEAD, sregno)) && GET_MODE (XEXP (note, 0)) == GET_MODE (src)){
               int failed = 0;

               /* We can do the optimization.  Scan forward from INSN
               again, replacing regs as we go.  Set FAILED if a
               replacement can't be done.  In that case, we can't
               move the death note for SRC.  This should be
               rare.  */

               /* Set to stop at next insn.  */
               for (q = next_real_insn (insn); q != next_real_insn (p); q = next_real_insn (q)){
                  if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,src, PATTERN (q))){
                     /* If SRC is a hard register, we might miss
                     some overlapping registers with
                     validate_replace_rtx, so we would have to
                     undo it.  We can't if DEST is present in
                     the insn, so fail in that combination of
                     cases.  */
                     if (sregno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
                     && reg_mentioned_p (dest, PATTERN (q)))
                        failed = 1;
                     /* Attempt to replace all uses.  */
                     else if (!mtcs_recog_validate_replace_rtx/*!validate_replace_rtx*/(mtcsRecog,src, dest, q))
                        failed = 1;
                     /* If this succeeded, but some part of the
                     register is still present, undo the
                     replacement.  */
                     else if (sregno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
                     && mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,src, PATTERN (q))){
                        mtcs_recog_validate_replace_rtx/*!validate_replace_rtx*/(mtcsRecog,dest, src, q);
                        failed = 1;
                     }
                  }

                  /* If DEST dies here, remove the death note and
                  save it for later.  Make sure ALL of DEST dies
                  here; again, this is overly conservative.  */
                  if (! dest_death  && (dest_death = find_regno_note (q, REG_DEAD, dregno))){
                     if (GET_MODE (XEXP (dest_death, 0)) == GET_MODE (dest))
                        mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,q, dest_death);
                     else{
                        failed = 1;
                        dest_death = 0;
                     }
                  }
               }//end  for (q = next_real_insn (insn); q !...

               if (! failed){
                  /* Move death note of SRC from P to INSN.  */
                  mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,p, note);
                  XEXP (note, 1) = REG_NOTES (insn);
                  REG_NOTES (insn) = note;
               }

               /* DEST is also dead if INSN has a REG_UNUSED note for
               DEST.  */
               if (! dest_death  && (dest_death = find_regno_note (insn, REG_UNUSED, dregno))){
                  PUT_REG_NOTE_KIND (dest_death, REG_DEAD);
                  mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,insn, dest_death);
               }

               /* Put death note of DEST on P if we saw it die.  */
               if (dest_death) {
                  XEXP (dest_death, 1) = REG_NOTES (p);
                  REG_NOTES (p) = dest_death;
               }
               break;
            }//进入 if ((note = find_regno_note (p, R... 的else if
            /* If SRC is a hard register which is set or killed in
            some other way, we can't do this optimization.  */
            else if (sregno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
                  && mtcs_rtlanal_dead_or_set_p/*!dead_or_set_p*/(mtcsRtlanal,p, src))
               break;
         }// end for (p = NEXT_INSN (insn); p; p = NEXT_INSN (p)){
      }//end       FOR_BB_INSNS (bb, insn){
}

/* Return nonzero if REGNO is a particularly bad choice for reloading X.  */
static bool ira_bad_reload_regno_1 (MtcsIra *self,int regno, rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);

   int x_regno, n, i;
   MtcsIraAllocno * a;
   enum reg_class pref;

   /* We only deal with pseudo regs.  */
   if (! x || GET_CODE (x) != REG)
      return false;

   x_regno = REGNO (x);
   if (x_regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg))
      return false;

   /* If the pseudo prefers REGNO explicitly, then do not consider
   REGNO a bad spill choice.  */
   pref = mtcs_reg_reg_preferred_class/*!reg_preferred_class*/(mtcsReg,x_regno);
   if (reg_class_size[pref] == 1)
      return !mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[pref], regno);

   /* If the pseudo conflicts with REGNO, then we consider REGNO a
   poor choice for a reload regno.  */
   a = mtcsIraBuild->ira_regno_allocno_map[x_regno];
   n = a->num_objects;
   for (i = 0; i < n; i++){
      MtcsIraObject * obj = a->objects[i];
      if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&obj->total_conflict_hard_regs, regno))
         return true;
   }
   return false;
}

/* Return nonzero if REGNO is a particularly bad choice for reloading
   IN or OUT.  */
//原型 ira_bad_reload_regno ira.h ira.cc
bool mtcs_ira_bad_reload_regno (MtcsIra *self,int regno, rtx in, rtx out)
{
  return (ira_bad_reload_regno_1(self,regno, in)
     || ira_bad_reload_regno_1(self,regno, out));
}

/* Add register clobbers from asm statements.  */
static void compute_regs_asm_clobbered (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   basic_block bb;

   FOR_EACH_BB_FN (bb, cfun){
      rtx_insn *insn;
      FOR_BB_INSNS_REVERSE (bb, insn){
         df_ref def;
         if (NONDEBUG_INSN_P (insn) && asm_noperands (PATTERN (insn)) >= 0)
            FOR_EACH_INSN_DEF (def, insn){
               unsigned int dregno = DF_REF_REGNO (def);
               if (mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,dregno))
                  mtcs_reg_add_to_hard_reg_set/*!add_to_hard_reg_set*/(mtcsReg,
                        &mtcsRtlData/*!crtl*/->asm_clobbers,GET_MODE (DF_REF_REAL_REG (def)), dregno);
            }
      }
   }
}

/* Set up ELIMINABLE_REGSET, IRA_NO_ALLOC_REGS, and
   REGS_EVER_LIVE.  */
//原型 ira_setup_eliminable_regset ira.h ira.cc
void  mtcs_ira_setup_eliminable_regset (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int i;
   /*!
   static const struct {const int from, to; } eliminables[] = ELIMINABLE_REGS;
   */
   static  struct elim_table_t
   {
      int from;
      int to;
   } eliminables[20];//20足够大了
   //给table赋值
   for(i=0;i<mtcsReg->elimiableRegsCount;i++){
      eliminables[i].from=mtcsReg->eliminableRegs[i].from;
      eliminables[i].to=mtcsReg->eliminableRegs[i].to;
   }

   int hardFramePointerRegnum = mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg);
   int fp_reg_count = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,
         hardFramePointerRegnum/*!HARD_FRAME_POINTER_REGNUM*/, mtcs_mode_get_Pmode(mtcsMode));

   /* Setup is_leaf as frame_pointer_required may use it.  This function
   is called by sched_init before ira if scheduling is enabled.  */
   mtcsRtlData/*!crtl*/->is_leaf = mtcs_output_leaf_function_p/*!leaf_function_p*/(mtcsOutput);

   /* FIXME: If EXIT_IGNORE_STACK is set, we will not save and restore
   sp for alloca.  So we can't eliminate the frame pointer in that
   case.  At some point, we should improve this by emitting the
   sp-adjusting insns for this case.  */
   mtcsRtlData->x_frame_pointer_needed/*!frame_pointer_needed*/ = (! mtcsOptionsItem->x_flag_omit_frame_pointer
   || (cfun->calls_alloca && mtcs_func_get_exit_ignore_stack/*!EXIT_IGNORE_STACK*/(mtcsFunc))
   /* We need the frame pointer to catch stack overflow exceptions if
   the stack pointer is moving (as for the alloca case just above).  */
   || (mtcs_func_get_stack_check_moving_sp/*!STACK_CHECK_MOVING_SP*/(mtcsFunc)
   && mtcsOptionsItem->x_flag_stack_check
   && mtcsOptionsItem->x_flag_exceptions
   && cfun->can_throw_non_call_exceptions)
   || mtcsRtlData/*!crtl*/->accesses_prior_frames
   || (mtcs_func_is_support_stack_alignment/*!SUPPORTS_STACK_ALIGNMENT*/(mtcsFunc) &&  mtcsRtlData/*!crtl*/->stack_realign_needed)
   || mtcsTarget/*!targetm.frame_pointer_required*/->frame_pointer_required(mtcsTarget));

   /* The chance that FRAME_POINTER_NEEDED is changed from inspecting
   RTL is very small.  So if we use frame pointer for RA and RTL
   actually prevents this, we will spill pseudos assigned to the
   frame pointer in LRA.  */

   if ( mtcsRtlData->x_frame_pointer_needed/*!frame_pointer_needed*/)
      for (i = 0; i < fp_reg_count; i++)
         mtcs_dfscan_df_set_regs_ever_live/*!df_set_regs_ever_live*/(mtcsDfscan,
               hardFramePointerRegnum/*!HARD_FRAME_POINTER_REGNUM*/ + i, true);

   self->x_ira_no_alloc_regs = no_unit_alloc_regs;
   mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&self->eliminable_regset);

   compute_regs_asm_clobbered(self);

   /* Build the regset of all eliminable registers and show we can't
   use those that we already know won't be eliminated.  */
   for (i = 0; i < mtcsReg->elimiableRegsCount/*!(int) ARRAY_SIZE (eliminables)*/; i++){
      bool cannot_elim = (! mtcsTarget/*!targetm.can_eliminate*/->can_eliminate(mtcsTarget,eliminables[i].from, eliminables[i].to)
      || (eliminables[i].to == mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg)
            && mtcsRtlData->x_frame_pointer_needed/*!frame_pointer_needed*/));

      if (!mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&mtcsRtlData/*!crtl*/->asm_clobbers, eliminables[i].from)){
         mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&self->eliminable_regset, eliminables[i].from);

         if (cannot_elim)
            mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&self->x_ira_no_alloc_regs, eliminables[i].from);
      }else if (cannot_elim)
         error ("%s cannot be used in %<asm%> here",mtcsReg->hardRegs.x_reg_names/*!reg_names*/[eliminables[i].from]);
      else
         mtcs_dfscan_df_set_regs_ever_live/*!df_set_regs_ever_live*/(mtcsDfscan,eliminables[i].from, true);
   }
   if (!mtcs_reg_hard_frame_pointer_is_frame_pointer/*!HARD_FRAME_POINTER_IS_FRAME_POINTER*/(mtcsReg)){
      for (i = 0; i < fp_reg_count; i++)
         if (mtcsReg->global_regs/*!global_regs*/[hardFramePointerRegnum/*!HARD_FRAME_POINTER_REGNUM*/ + i])
            /* Nothing to do: the register is already treated as live
            where appropriate, and cannot be eliminated.  */
            ;
         else if (!mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&mtcsRtlData/*!crtl*/->asm_clobbers,
               hardFramePointerRegnum/*!HARD_FRAME_POINTER_REGNUM*/ + i)){
            mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&self->eliminable_regset,
            hardFramePointerRegnum/*!HARD_FRAME_POINTER_REGNUM*/ + i);
            if (mtcsRtlData->x_frame_pointer_needed/*!frame_pointer_needed*/)
               mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&self->x_ira_no_alloc_regs,
                     hardFramePointerRegnum/*!HARD_FRAME_POINTER_REGNUM*/ + i);
         }else if (mtcsRtlData->x_frame_pointer_needed/*!frame_pointer_needed*/)
            error ("%s cannot be used in %<asm%> here",
                  mtcsReg->hardRegs.x_reg_names/*!reg_names*/[hardFramePointerRegnum/*!HARD_FRAME_POINTER_REGNUM*/ + i]);
         else
            mtcs_dfscan_df_set_regs_ever_live/*!df_set_regs_ever_live*/(mtcsDfscan,
                  hardFramePointerRegnum/*!HARD_FRAME_POINTER_REGNUM*/ + i, true);
   }
}



/* Vector of substitutions of register numbers,
   used to map pseudo regs into hardware regs.
   This is set up as a result of register allocation.
   Element N is the hard reg assigned to pseudo reg N,
   or is -1 if no hard reg was assigned.
   If N is a hard reg number, element N is N.  */
//short *reg_renumber; ira-int.h ira.cc

/* Set up REG_RENUMBER and CALLER_SAVE_NEEDED (used by reload) from
   the allocation found by IRA.  */
static void setup_reg_renumber (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   int regno, hard_regno;
   MtcsIraAllocno *a;
   MtcsIraAllocnoIterator ai;

   caller_save_needed = 0;//声明在 regs.h
   MTCS_FOR_EACH_ALLOCNO (mtcsIraBuild,a, ai){
      if (self->ira_use_lra_p && a->cap_member != NULL)
         continue;
      /* There are no caps at this point.  */
      ira_assert (a->cap_member == NULL);
      if (! a->assigned_p)
         /* It can happen if A is not referenced but partially anticipated
         somewhere in a region.  */
         a->assigned_p = true;
      mtcs_ira_build_free_allocno_updated_costs/*!ira_free_allocno_updated_costs*/(mtcsIraBuild,a);
      hard_regno = a->hard_regno;
      regno = a->regno;
      reg_renumber[regno] = (hard_regno < 0 ? -1 : hard_regno);
      if (hard_regno >= 0){
         int i, nwords;
         enum reg_class pclass;
         MtcsIraObject *obj;

         pclass = self->x_ira_pressure_class_translate[mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,hard_regno)];
         nwords = a->num_objects;
         for (i = 0; i < nwords; i++){
            obj = a->objects[i];
            obj->total_conflict_hard_regs |= ~mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[pclass];
         }
         if (mtcs_ira_allocno_need_caller_save_p/*!ira_need_caller_save_p*/(a, hard_regno)){
            ira_assert (!mtcsOptionsItem->x_optimize || mtcsOptionsItem->x_flag_caller_saves
            || (a->calls_crossed_num == a->cheap_calls_crossed_num)
            || regno >= self->ira_reg_equiv_len
            || mtcs_ira_equiv_no_lvalue_p/*!ira_equiv_no_lvalue_p*/(self,regno));
            caller_save_needed = 1;
         }
      }
   }
}

/* Set up allocno assignment flags for further allocation
   improvements.  */
static void setup_allocno_assignment_flags (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   int hard_regno;
   MtcsIraAllocno *a;
   MtcsIraAllocnoIterator ai;

   MTCS_FOR_EACH_ALLOCNO (mtcsIraBuild,a, ai){
      if (! a->assigned_p)
         /* It can happen if A is not referenced but partially anticipated
         somewhere in a region.  */
         mtcs_ira_build_free_allocno_updated_costs/*!ira_free_allocno_updated_costs*/(mtcsIraBuild,a);
      hard_regno = a->hard_regno;
      /* Don't assign hard registers to allocnos which are destination
      of removed store at the end of loop.  It has no sense to keep
      the same value in different hard registers.  It is also
      impossible to assign hard registers correctly to such
      allocnos because the cost info and info about intersected
      calls are incorrect for them.  */
      a->assigned_p = (hard_regno >= 0  || MTCS_ALLOCNO_EMIT_DATA (a)->mem_optimized_dest_p
            || (a->memory_cost  - a->class_cost) < 0);
      ira_assert(hard_regno < 0 || mtcs_ira_hard_reg_in_set_p/*!ira_hard_reg_in_set_p*/(self,hard_regno, a->mode,
            &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[a->aclass]));
   }
}

/* Evaluate overall allocation cost and the costs for using hard
   registers and memory for allocnos.  */
static void calculate_allocation_cost (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int hard_regno, cost;
   MtcsIraAllocno *a;
   MtcsIraAllocnoIterator ai;


   self->ira_overall_cost = self->ira_reg_cost = self->ira_mem_cost = 0;
   MTCS_FOR_EACH_ALLOCNO (mtcsIraBuild,a, ai){
      hard_regno = a->hard_regno;
      ira_assert (hard_regno < 0 || (mtcs_ira_hard_reg_in_set_p/*!ira_hard_reg_in_set_p*/(self,hard_regno, a->mode,
                           &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[a->aclass])));
      if (hard_regno < 0){
         cost = a->memory_cost;
         self->ira_mem_cost += cost;
      }else if (a->hard_reg_costs != NULL){
         cost = (a->hard_reg_costs[mtcsIraInt->x_ira_class_hard_reg_index[a->aclass][hard_regno]]);
         ira_reg_cost += cost;
      }else{
         cost = a->class_cost;
         self->ira_reg_cost += cost;
      }
      self->ira_overall_cost += cost;
   }

   if (mtcsIraGlobal->internal_flag_ira_verbose > 0 && mtcsIraGlobal->ira_dump_file != NULL){
      fprintf (mtcsIraGlobal->ira_dump_file,
      "+++Costs: overall %" PRId64
      ", reg %" PRId64
      ", mem %" PRId64
      ", ld %" PRId64
      ", st %" PRId64
      ", move %" PRId64,
      self->ira_overall_cost, self->ira_reg_cost, self->ira_mem_cost,
      self->ira_load_cost, self->ira_store_cost, self->ira_shuffle_cost);
      fprintf (mtcsIraGlobal->ira_dump_file, "\n+++       move loops %d, new jumps %d\n",
      self->ira_move_loops_num, self->ira_additional_jumps_num);
   }

}

#ifdef ENABLE_IRA_CHECKING
   /* Check the correctness of the allocation.  We do need this because
   of complicated code to transform more one region internal
   representation into one region representation.  */
static void check_allocation (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraColor *mtcsIraColor = mtcs_ira_mgr_get_color(mtcsIraMgr);

   MtcsIraAllocno *a;
   int hard_regno, nregs, conflict_nregs;
   MtcsIraAllocnoIterator ai;

   MTCS_FOR_EACH_ALLOCNO (mtcsIraBuild,a, ai){
      int n = a->num_objects;
      int i;

      if (a->cap_member != NULL  || (hard_regno = a->hard_regno) < 0)
         continue;
      nregs = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,hard_regno, a->mode);
      if (nregs == 1)
         /* We allocated a single hard register.  */
         n = 1;
      else if (n > 1)
         /* We allocated multiple hard registers, and we will test
         conflicts in a granularity of single hard regs.  */
         nregs = 1;

      for (i = 0; i < n; i++){
         MtcsIraObject *obj = a->objects[i];
         MtcsIraObject *conflict_obj;
         MtcsIraObjectConflictIterator oci;
         int this_regno = hard_regno;
         if (n > 1){
            if (REG_WORDS_BIG_ENDIAN)
               this_regno += n - i - 1;
            else
               this_regno += i;
         }
         MTCS_FOR_EACH_OBJECT_CONFLICT (mtcsIraBuild,obj, conflict_obj, oci){
            MtcsIraAllocno *conflict_a = conflict_obj->allocno;
            int conflict_hard_regno = conflict_a->hard_regno;
            if (conflict_hard_regno < 0)
               continue;
            if (mtcs_ira_color_soft_conflict/*!ira_soft_conflict*/(mtcsIraColor,a, conflict_a)) //mtcsira...color
               continue;

            conflict_nregs = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,conflict_hard_regno,conflict_a->mode);

            if (conflict_a->num_objects > 1  && conflict_nregs == conflict_a->num_objects){
               if (REG_WORDS_BIG_ENDIAN)
                  conflict_hard_regno += (conflict_a->num_objects - conflict_obj->subword - 1);
               else
                  conflict_hard_regno += conflict_obj->subword;
               conflict_nregs = 1;
            }

            if ((conflict_hard_regno <= this_regno
            && this_regno < conflict_hard_regno + conflict_nregs)
            || (this_regno <= conflict_hard_regno
            && conflict_hard_regno < this_regno + nregs)){
               fprintf (stderr, "bad allocation for %d and %d\n", a->regno, conflict_a->regno);
               gcc_unreachable ();
            }
         }
      }
   }
}
#endif

/* Allocate REG_EQUIV_INIT.  Set up it from IRA_REG_EQUIV which should
   be already calculated.  */
static void setup_reg_equiv_init (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   int i;
   int max_regno = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);
   //reg_equiv_init 声明在 reload.h
   for (i = 0; i < max_regno; i++)
      reg_equiv_init (i) = self->ira_reg_equiv[i].init_insns;
}

/* Update equiv regno from movement of FROM_REGNO to TO_REGNO.  INSNS
   are insns which were generated for such movement.  It is assumed
   that FROM_REGNO and TO_REGNO always have the same value at the
   point of any move containing such registers. This function is used
   to update equiv info for register shuffles on the region borders
   and for caller save/restore insns.  */
//原型 ira_update_equiv_info_by_shuffle_insn ira.h ira.cc
void mtcs_ira_update_equiv_info_by_shuffle_insn (MtcsIra *self,int to_regno, int from_regno, rtx_insn *insns)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   rtx_insn *insn;
   rtx x, note;

   if (! self->ira_reg_equiv[from_regno].defined_p
   && (! self->ira_reg_equiv[to_regno].defined_p
   || ((x = self->ira_reg_equiv[to_regno].memory) != NULL_RTX
   && ! MEM_READONLY_P (x))))
      return;
   insn = insns;
   if (NEXT_INSN (insn) != NULL_RTX) {
      if (! self->ira_reg_equiv[to_regno].defined_p){
         ira_assert (self->ira_reg_equiv[to_regno].init_insns == NULL_RTX);
         return;
      }
      self->ira_reg_equiv[to_regno].defined_p = false;
      self->ira_reg_equiv[to_regno].caller_save_p = false;
      self->ira_reg_equiv[to_regno].memory = self->ira_reg_equiv[to_regno].constant
            = self->ira_reg_equiv[to_regno].invariant  = self->ira_reg_equiv[to_regno].init_insns = NULL;
      if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
         fprintf (ira_dump_file, "      Invalidating equiv info for reg %d\n", to_regno);
      return;
   }
   /* It is possible that FROM_REGNO still has no equivalence because
   in shuffles to_regno<-from_regno and from_regno<-to_regno the 2nd
   insn was not processed yet.  */
   if (self->ira_reg_equiv[from_regno].defined_p){
      self->ira_reg_equiv[to_regno].defined_p = true;
      if ((x = self->ira_reg_equiv[from_regno].memory) != NULL_RTX){
         ira_assert (self->ira_reg_equiv[from_regno].invariant == NULL_RTX && self->ira_reg_equiv[from_regno].constant == NULL_RTX);
         ira_assert (self->ira_reg_equiv[to_regno].memory == NULL_RTX || rtx_equal_p (self->ira_reg_equiv[to_regno].memory, x));
         self->ira_reg_equiv[to_regno].memory = x;
         if (! MEM_READONLY_P (x))
            /* We don't add the insn to insn init list because memory
            equivalence is just to say what memory is better to use
            when the pseudo is spilled.  */
            return;
      }else if ((x = self->ira_reg_equiv[from_regno].constant) != NULL_RTX){
         ira_assert (self->ira_reg_equiv[from_regno].invariant == NULL_RTX);
         ira_assert (self->ira_reg_equiv[to_regno].constant == NULL_RTX || rtx_equal_p (self->ira_reg_equiv[to_regno].constant, x));
         self->ira_reg_equiv[to_regno].constant = x;
      }else{
         x = self->ira_reg_equiv[from_regno].invariant;
         ira_assert (x != NULL_RTX);
         ira_assert (self->ira_reg_equiv[to_regno].invariant == NULL_RTX || rtx_equal_p (self->ira_reg_equiv[to_regno].invariant, x));
         self->ira_reg_equiv[to_regno].invariant = x;
      }
      if (find_reg_note (insn, REG_EQUIV, x) == NULL_RTX){
         note = mtcs_rtl_set_unique_reg_note/*!set_unique_reg_note*/(mtcsRTL,insn, REG_EQUIV, copy_rtx (x));
         gcc_assert (note != NULL_RTX);
         if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && ira_dump_file != NULL) {
            fprintf (ira_dump_file, "      Adding equiv note to insn %u for reg %d ",INSN_UID (insn), to_regno);
            dump_value_slim (mtcsIraGlobal->ira_dump_file, x, 1);
            fprintf (ira_dump_file, "\n");
         }
      }
   }
   self->ira_reg_equiv[to_regno].init_insns= gen_rtx_INSN_LIST (VOIDmode, insn, self->ira_reg_equiv[to_regno].init_insns);
   if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
   fprintf (mtcsIraGlobal->ira_dump_file,"      Adding equiv init move insn %u to reg %d\n",INSN_UID (insn), to_regno);
}

/* Fix values of array REG_EQUIV_INIT after live range splitting done
   by IRA.  */
static void fix_reg_equiv_init (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsReload1 *mtcsReload1=mtcs_target_get_reload1(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int max_regno = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);
   int i, new_regno, max;
   rtx set;
   rtx_insn_list *x, *next, *prev;
   rtx_insn *insn;

   if (self->max_regno_before_ira < max_regno){
      max = vec_safe_length (reg_equivs);
      mtcs_reload1_grow_reg_equivs/*!grow_reg_equivs*/(mtcsReload1);
      for (i = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i < max; i++)
         for (prev = NULL, x = reg_equiv_init (i); x != NULL_RTX;  x = next) {
            next = x->next ();
            insn = x->insn ();
            set = single_set (insn);
            ira_assert (set != NULL_RTX  && (REG_P (SET_DEST (set)) || REG_P (SET_SRC (set))));
            if (REG_P (SET_DEST (set)) && ((int) REGNO (SET_DEST (set)) == i  || (int) ORIGINAL_REGNO (SET_DEST (set)) == i))
               new_regno = REGNO (SET_DEST (set));
            else if (REG_P (SET_SRC (set))  && ((int) REGNO (SET_SRC (set)) == i || (int) ORIGINAL_REGNO (SET_SRC (set)) == i))
               new_regno = REGNO (SET_SRC (set));
            else
               gcc_unreachable ();
            if (new_regno == i)
               prev = x;
            else{
               /* Remove the wrong list element.  */
               if (prev == NULL_RTX)
                  reg_equiv_init (i) = next;
               else
                  XEXP (prev, 1) = next;
               XEXP (x, 1) = reg_equiv_init (new_regno);
               reg_equiv_init (new_regno) = x;
            }
         }
   }
}

#ifdef ENABLE_IRA_CHECKING
/* Print redundant memory-memory copies.  */
static void print_redundant_copies (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int hard_regno;
   MtcsIraAllocno *a;
   MtcsIraAllocnoCopy *cp, *next_cp;
   MtcsIraAllocnoIterator ai;

   MTCS_FOR_EACH_ALLOCNO (mtcsIraBuild,a, ai){
      if (a->cap_member != NULL)
         /* It is a cap.  */
         continue;
      hard_regno = a->hard_regno;
      if (hard_regno >= 0)
         continue;
      for (cp = a->allocno_copies; cp != NULL; cp = next_cp)
         if (cp->first == a)
            next_cp = cp->next_first_allocno_copy;
         else{
            next_cp = cp->next_second_allocno_copy;
            if (mtcsIraGlobal->internal_flag_ira_verbose > 4 && mtcsIraGlobal->ira_dump_file != NULL
            && cp->insn != NULL_RTX
            && cp->first->hard_regno == hard_regno)
               fprintf (mtcsIraGlobal->ira_dump_file,"        Redundant move from %d(freq %d):%d\n",
                     INSN_UID (cp->insn), cp->freq, hard_regno);
         }
   }
}
#endif

/* Setup preferred and alternative classes for new pseudo-registers
   created by IRA starting with START.  */
static void setup_preferred_alternate_classes_for_new_pseudos (MtcsIra *self,int start)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int i, old_regno;
   int max_regno = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);
   MtcsRegClass *mtcsRegClass =mtcs_reg_get_reg_class(mtcsReg);

   for (i = start; i < max_regno; i++){
      old_regno = ORIGINAL_REGNO (mtcsRtlData->regno_reg_rtx/*! regno_reg_rtx*/[i]);
      ira_assert (i != old_regno);
      mtcs_reg_setup_reg_classes/*!setup_reg_classes*/(mtcsReg,i,
            mtcs_reg_reg_preferred_class/*!reg_preferred_class*/(mtcsReg,old_regno),
            mtcs_reg_reg_alternate_class/*!reg_alternate_class*/(mtcsReg,old_regno),
            mtcs_reg_reg_allocno_class/*!reg_allocno_class*/(mtcsReg,old_regno));
      if (mtcsIraGlobal->internal_flag_ira_verbose > 2 && mtcsIraGlobal->ira_dump_file != NULL)
         fprintf (mtcsIraGlobal->ira_dump_file,
            "    New r%d: setting preferred %s, alternative %s\n",
            i, mtcsRegClass/*!reg_class_names*/[mtcs_reg_reg_preferred_class/*!reg_preferred_class*/(mtcsReg,old_regno)].name,
            mtcsRegClass/*!reg_class_names*/[mtcs_reg_reg_alternate_class/*!reg_alternate_class*/(mtcsReg,old_regno)].name);
   }
}



/* Regional allocation can create new pseudo-registers.  This function
   expands some arrays for pseudo-registers.  */
static void expand_reg_info (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   int i;
   int size = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);

   mtcs_reg_resize_reg_info/*!resize_reg_info*/(mtcsReg);
   for (i = self->allocated_reg_info_size; i < size; i++)
      mtcs_reg_setup_reg_classes/*!setup_reg_classes*/(mtcsReg,i,
               mtcs_reg_get_general_regs/*!GENERAL_REGS*/(mtcsReg),
               mtcs_reg_get_all_regs/*!ALL_REGS*/(mtcsReg),
               mtcs_reg_get_general_regs/*!GENERAL_REGS*/(mtcsReg));
   setup_preferred_alternate_classes_for_new_pseudos(self,self->allocated_reg_info_size);
   self->allocated_reg_info_size = size;
}

/* Return TRUE if there is too high register pressure in the function.
   It is used to decide when stack slot sharing is worth to do.  */
static bool too_high_register_pressure_p (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   int i;
   enum reg_class pclass;

   for (i = 0; i < self->x_ira_pressure_classes_num; i++){
      pclass = self->x_ira_pressure_classes[i];
      if (mtcsIraBuild->ira_loop_tree_root->reg_pressure[pclass] > 10000)
         return true;
   }
   return false;
}



/* Indicate that hard register number FROM was eliminated and replaced with
   an offset from hard register number TO.  The status of hard registers live
   at the start of a basic block is updated by replacing a use of FROM with
   a use of TO.  */
//rtl.h中声明 只有 reload1调用一次
//void mark_elimination (int from, int to)
//{
//  basic_block bb;
//  bitmap r;
//
//  FOR_EACH_BB_FN (bb, cfun)
//    {
//      r = DF_LR_IN (bb);
//      if (bitmap_bit_p (r, from))
//   {
//     bitmap_clear_bit (r, from);
//     bitmap_set_bit (r, to);
//   }
//      if (! df_live)
//        continue;
//      r = DF_LIVE_IN (bb);
//      if (bitmap_bit_p (r, from))
//   {
//     bitmap_clear_bit (r, from);
//     bitmap_set_bit (r, to);
//   }
//    }
//}


/* Expand self->ira_reg_equiv if necessary.  */
//原型 ira_expand_reg_equiv ira.h ira.cc
void mtcs_ira_expand_reg_equiv (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   int old = self->ira_reg_equiv_len;
   if (self->ira_reg_equiv_len > mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc))
      return;
   self->ira_reg_equiv_len = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc) * 3 / 2 + 1;
   self->ira_reg_equiv = (struct ira_reg_equiv_s *) xrealloc (ira_reg_equiv,
         self->ira_reg_equiv_len * sizeof (struct ira_reg_equiv_s));
   gcc_assert (old < self->ira_reg_equiv_len);
   memset (self->ira_reg_equiv + old, 0, sizeof (struct ira_reg_equiv_s) * (self->ira_reg_equiv_len - old));
}

static void init_reg_equiv (MtcsIra *self)
{
  self->ira_reg_equiv_len = 0;
  self->ira_reg_equiv = NULL;
  mtcs_ira_expand_reg_equiv/*!ira_expand_reg_equiv*/(self);
}

static void finish_reg_equiv (MtcsIra *self)
{
  free (self->ira_reg_equiv);
}

struct equivalence
{
  /* Set when a REG_EQUIV note is found or created.  Use to
     keep track of what memory accesses might be created later,
     e.g. by reload.  */
  rtx replacement;
  rtx *src_p;

  /* The list of each instruction which initializes this register.

     NULL indicates we know nothing about this register's equivalence
     properties.

     An INSN_LIST with a NULL insn indicates this pseudo is already
     known to not have a valid equivalence.  */
  rtx_insn_list *init_insns;
  /* Loop depth is used to recognize equivalences which appear
     to be present within the same loop (or in an inner loop).  */
  short loop_depth;
  /* Nonzero if this had a preexisting REG_EQUIV note.  */
  unsigned char is_arg_equivalence : 1;
  /* Set when an attempt should be made to replace a register
     with the associated src_p entry.  */
  unsigned char replace : 1;
  /* Set if this register has no known equivalence.  */
  unsigned char no_equiv : 1;
  /* Set if this register is mentioned in a paradoxical subreg.  */
  unsigned char pdx_subregs : 1;
};

/* Used for communication between the following two functions.  */
struct equiv_mem_data
{
  /* A MEM that we wish to ensure remains unchanged.  */
  rtx equiv_mem;

  /* Set true if EQUIV_MEM is modified.  */
  bool equiv_mem_modified;
  MtcsIra *mtcsIra;
};

/* If EQUIV_MEM is modified by modifying DEST, indicate that it is modified.
   Called via note_stores.  */
static void validateEquivMemFromStore_cb(rtx dest, const_rtx set ATTRIBUTE_UNUSED,
                void *userData)
{
   struct equiv_mem_data *info = (struct equiv_mem_data *) userData;
   MtcsIra *self=info->mtcsIra;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);

   if ((REG_P (dest)
   && mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,dest, info->equiv_mem))
   || (MEM_P (dest)
   && mtcs_alias_anti_dependence/*!anti_dependence*/(mtcsAlias,info->equiv_mem, dest)))
      info->equiv_mem_modified = true;
}



/* Verify that no store between START and the death of REG invalidates
   MEMREF.  MEMREF is invalidated by modifying a register used in MEMREF,
   by storing into an overlapping memory location, or with a non-const
   CALL_INSN.

   Return VALID_RELOAD if MEMREF remains valid for both reload and
   combine_and_move insns, VALID_COMBINE if only valid for
   combine_and_move_insns, and VALID_NONE otherwise.  */
static enum valid_equiv validate_equiv_mem (MtcsIra *self,rtx_insn *start, rtx reg, rtx memref)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   rtx_insn *insn;
   rtx note;
   struct equiv_mem_data info = { memref, false,self};
   enum valid_equiv ret = valid_reload;

   /* If the memory reference has side effects or is volatile, it isn't a
   valid equivalence.  */
   if (side_effects_p (memref))
      return valid_none;

   for (insn = start; insn; insn = NEXT_INSN (insn)){
      if (!INSN_P (insn))
         continue;

      if (find_reg_note (insn, REG_DEAD, reg))
         return ret;

      if (CALL_P (insn)){
         /* We can combine a reg def from one insn into a reg use in
         another over a call if the memory is readonly or the call
         const/pure.  However, we can't set reg_equiv notes up for
         reload over any call.  The problem is the equivalent form
         may reference a pseudo which gets assigned a call
         clobbered hard reg.  When we later replace REG with its
         equivalent form, the value in the call-clobbered reg has
         been changed and all hell breaks loose.  */
         ret = valid_combine;
         if (!MEM_READONLY_P (memref) && (!RTL_CONST_OR_PURE_CALL_P (insn)
         || equiv_init_varies_p(self,XEXP (memref, 0))))
            return valid_none;
      }

      mtcs_rtlanal_note_stores/*!note_stores*/(mtcsRtlanal,insn, validateEquivMemFromStore_cb, &info);
      if (info.equiv_mem_modified)
         return valid_none;

      /* If a register mentioned in MEMREF is modified via an
      auto-increment, we lose the equivalence.  Do the same if one
      dies; although we could extend the life, it doesn't seem worth
      the trouble.  */

      for (note = REG_NOTES (insn); note; note = XEXP (note, 1))
         if ((REG_NOTE_KIND (note) == REG_INC
         || REG_NOTE_KIND (note) == REG_DEAD)
         && REG_P (XEXP (note, 0))
         && mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,XEXP (note, 0), memref))
            return valid_none;
   }

   return valid_none;
}

/* Returns false if X is known to be invariant.  */
static bool equiv_init_varies_p (MtcsIra *self,rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   RTX_CODE code = GET_CODE (x);
   int i;
   const char *fmt;
   switch (code){
      case MEM:
         return !MEM_READONLY_P (x) || equiv_init_varies_p(self,XEXP (x, 0));
      case CONST:
      CASE_CONST_ANY:
      case SYMBOL_REF:
      case LABEL_REF:
         return false;
      case REG:
         return self->reg_equiv[REGNO (x)].replace == 0 && mtcs_rtlanal_rtx_varies_p/*!rtx_varies_p*/(mtcsRtlanal,x, 0);
      case ASM_OPERANDS:
         if (MEM_VOLATILE_P (x))
            return true;
      /* Fall through.  */
      default:
         break;
   }

   fmt = GET_RTX_FORMAT (code);
   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
   if (fmt[i] == 'e'){
      if (equiv_init_varies_p(self,XEXP (x, i)))
         return true;
   }else if (fmt[i] == 'E'){
      int j;
      for (j = 0; j < XVECLEN (x, i); j++)
         if (equiv_init_varies_p(self,XVECEXP (x, i, j)))
            return true;
   }
   return false;
}

/* Returns true if X (used to initialize register REGNO) is movable.
   X is only movable if the registers it uses have equivalent initializations
   which appear to be within the same loop (or in an inner loop) and movable
   or if they are not candidates for local_alloc and don't vary.  */
static bool equiv_init_movable_p (MtcsIra *self,rtx x, int regno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   int i, j;
   const char *fmt;
   enum rtx_code code = GET_CODE (x);
   switch (code){
      case SET:
         return equiv_init_movable_p(self,SET_SRC (x), regno);
      case CLOBBER:
         return false;
      case PRE_INC:
      case PRE_DEC:
      case POST_INC:
      case POST_DEC:
      case PRE_MODIFY:
      case POST_MODIFY:
         return false;
      case REG:
         return ((self->reg_equiv[REGNO (x)].loop_depth >= self->reg_equiv[regno].loop_depth
               && self->reg_equiv[REGNO (x)].replace)
               || (REG_BASIC_BLOCK (REGNO (x)) < NUM_FIXED_BLOCKS
               && ! mtcs_rtlanal_rtx_varies_p/*!rtx_varies_p*/(mtcsRtlanal,x, 0)));

      case UNSPEC_VOLATILE:
         return false;
      case ASM_OPERANDS:
         if (MEM_VOLATILE_P (x))
            return false;
      /* Fall through.  */
      default:
         break;
   }

   fmt = GET_RTX_FORMAT (code);
   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
      switch (fmt[i]){
         case 'e':
            if (! equiv_init_movable_p(self,XEXP (x, i), regno))
               return false;
            break;
         case 'E':
            for (j = XVECLEN (x, i) - 1; j >= 0; j--)
               if (! equiv_init_movable_p(self,XVECEXP (x, i, j), regno))
                  return false;
            break;
      }

   return true;
}


/* Auxiliary function for memref_referenced_p.  Process setting X for
   MEMREF store.  */
static bool process_set_for_memref_referenced_p (MtcsIra *self,rtx memref, rtx x)
{
   /* If we are setting a MEM, it doesn't count (its address does), but any
   other SET_DEST that has a MEM in it is referencing the MEM.  */
   if (MEM_P (x)){
      if (memref_referenced_p(self,memref, XEXP (x, 0), true))
         return true;
   }else if (memref_referenced_p(self,memref, x, false))
      return true;

   return false;
}

/* TRUE if X references a memory location (as a read if READ_P) that
   would be affected by a store to MEMREF.  */
static bool memref_referenced_p (MtcsIra *self,rtx memref, rtx x, bool read_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);

   int i, j;
   const char *fmt;
   enum rtx_code code = GET_CODE (x);

   switch (code){
      case CONST:
      case LABEL_REF:
      case SYMBOL_REF:
      CASE_CONST_ANY:
      case PC:
      case HIGH:
      case LO_SUM:
         return false;

      case REG:
         return (self->reg_equiv[REGNO (x)].replacement
               && memref_referenced_p(self,memref,self->reg_equiv[REGNO (x)].replacement, read_p));

      case MEM:
         /* Memory X might have another effective type than MEMREF.  */
         if (read_p || mtcs_alias_true_dependence/*!true_dependence*/(mtcsAlias,memref, VOIDmode, x))
            return true;
         break;

      case SET:
         if (process_set_for_memref_referenced_p(self,memref, SET_DEST (x)))
            return true;
         return memref_referenced_p(self,memref, SET_SRC (x), true);

      case CLOBBER:
         if (process_set_for_memref_referenced_p(self,memref, XEXP (x, 0)))
            return true;
         return false;

      case PRE_DEC:
      case POST_DEC:
      case PRE_INC:
      case POST_INC:
         if (process_set_for_memref_referenced_p(self,memref, XEXP (x, 0)))
            return true;
         return memref_referenced_p(self,memref, XEXP (x, 0), true);

      case POST_MODIFY:
      case PRE_MODIFY:
         /* op0 = op0 + op1 */
         if (process_set_for_memref_referenced_p(self,memref, XEXP (x, 0)))
            return true;
         if (memref_referenced_p(self,memref, XEXP (x, 0), true))
            return true;
         return memref_referenced_p(self,memref, XEXP (x, 1), true);

      default:
         break;
   }

   fmt = GET_RTX_FORMAT (code);
   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
      switch (fmt[i]){
         case 'e':
            if (memref_referenced_p(self,memref, XEXP (x, i), read_p))
               return true;
            break;
         case 'E':
            for (j = XVECLEN (x, i) - 1; j >= 0; j--)
               if (memref_referenced_p(self,memref, XVECEXP (x, i, j), read_p))
                  return true;
            break;
      }

   return false;
}

/* TRUE if some insn in the range (START, END] references a memory location
   that would be affected by a store to MEMREF.

   Callers should not call this routine if START is after END in the
   RTL chain.  */
static bool memref_used_between_p (MtcsIra *self,rtx memref, rtx_insn *start, rtx_insn *end)
{
   rtx_insn *insn;
   for (insn = NEXT_INSN (start);insn && insn != NEXT_INSN (end);insn = NEXT_INSN (insn)){
      if (!NONDEBUG_INSN_P (insn))
         continue;

      if (memref_referenced_p(self,memref, PATTERN (insn), false))
         return true;

      /* Nonconst functions may access memory.  */
      if (CALL_P (insn) && (! RTL_CONST_CALL_P (insn)))
         return true;
   }
   gcc_assert (insn == NEXT_INSN (end));
   return false;
}

/* Mark REG as having no known equivalence.
   Some instructions might have been processed before and furnished
   with REG_EQUIV notes for this register; these notes will have to be
   removed.
   STORE is the piece of RTL that does the non-constant / conflicting
   assignment - a SET, CLOBBER or REG_INC note.  It is currently not used,
   but needs to be there because this function is called from note_stores.  */
static void no_equiv (rtx reg, const_rtx store ATTRIBUTE_UNUSED,void *userData ATTRIBUTE_UNUSED)
{
   MtcsIra *self=(MtcsIra *)userData;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   int regno;
   rtx_insn_list *list;

   if (!REG_P (reg))
      return;
   regno = REGNO (reg);
   self->reg_equiv[regno].no_equiv = 1;
   list = self->reg_equiv[regno].init_insns;
   if (list && list->insn () == NULL)
      return;
   self->reg_equiv[regno].init_insns = gen_rtx_INSN_LIST (VOIDmode, NULL_RTX, NULL);
   self->reg_equiv[regno].replacement = NULL_RTX;
   /* This doesn't matter for equivalences made for argument registers, we
   should keep their initialization insns.  */
   if (self->reg_equiv[regno].is_arg_equivalence)
      return;
   self->ira_reg_equiv[regno].defined_p = false;
   self->ira_reg_equiv[regno].caller_save_p = false;
   self->ira_reg_equiv[regno].init_insns = NULL;
   for (; list; list = list->next ()){
      rtx_insn *insn = list->insn ();
      mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,insn, find_reg_note (insn, REG_EQUIV, NULL_RTX));
   }
}

/* Check whether the SUBREG is a paradoxical subreg and set the result
   in PDX_SUBREGS.  */
static void set_paradoxical_subreg (MtcsIra *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   subrtx_iterator::array_type array;
   FOR_EACH_SUBRTX (iter, array, PATTERN (insn), NONCONST){
      const_rtx subreg = *iter;
      if (GET_CODE (subreg) == SUBREG){
         const_rtx reg = SUBREG_REG (subreg);
         if (REG_P (reg) && mtcs_rtl_paradoxical_subreg_p/*!paradoxical_subreg_p*/(mtcsRTL,subreg))
            self->reg_equiv[REGNO (reg)].pdx_subregs = true;
      }
   }
}

typedef struct _AdjustClearedRegsCallBackData{
   bitmap cleared_regs;
   MtcsIra *mtcsIra;
}AdjustClearedRegsCallBackData;

/* In DEBUG_INSN location adjust REGs from CLEARED_REGS bitmap to the
   equivalent replacement.  */
static rtx adjust_cleared_regs (rtx loc, const_rtx old_rtx ATTRIBUTE_UNUSED, void *userData)
{
   AdjustClearedRegsCallBackData *info=(AdjustClearedRegsCallBackData *)userData;
   MtcsIra *self=info->mtcsIra;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   if (REG_P (loc)){
      bitmap cleared_regs = (bitmap)info->cleared_regs;
      if (bitmap_bit_p (cleared_regs, REGNO (loc)))
         return mtcs_simplify_rtx_simplify_replace_fn_rtx/*!simplify_replace_fn_rtx*/(mtcsSimplifyRtx,
                     copy_rtx (*self->reg_equiv[REGNO (loc)].src_p),NULL_RTX, adjust_cleared_regs, userData);
   }
   return NULL_RTX;
}

/* Given register REGNO is set only once, return true if the defining
   insn dominates all uses.  */
static bool def_dominates_uses (MtcsIra *self,int regno)
{
   df_ref def = DF_REG_DEF_CHAIN (regno);
   struct df_insn_info *def_info = DF_REF_INSN_INFO (def);
   /* If this is an artificial def (eh handler regs, hard frame pointer
   for non-local goto, regs defined on function entry) then def_info
   is NULL and the reg is always live before any use.  We might
   reasonably return true in that case, but since the only call
   of this function is currently here in ira.cc when we are looking
   at a defining insn we can't have an artificial def as that would
   bump DF_REG_DEF_COUNT.  */
   gcc_assert (DF_REG_DEF_COUNT (regno) == 1 && def_info != NULL);

   rtx_insn *def_insn = DF_REF_INSN (def);
   basic_block def_bb = BLOCK_FOR_INSN (def_insn);

   for (df_ref use = DF_REG_USE_CHAIN (regno); use; use = DF_REF_NEXT_REG (use)){
      struct df_insn_info *use_info = DF_REF_INSN_INFO (use);
      /* Only check real uses, not artificial ones.  */
      if (use_info){
         rtx_insn *use_insn = DF_REF_INSN (use);
         if (!DEBUG_INSN_P (use_insn)){
            basic_block use_bb = BLOCK_FOR_INSN (use_insn);
            if (use_bb != def_bb ? !dominated_by_p (CDI_DOMINATORS, use_bb, def_bb)
                              : DF_INSN_INFO_LUID (use_info) < DF_INSN_INFO_LUID (def_info))
               return false;
         }
      }
   }
   return true;
}

/* Scan the instructions before update_equiv_regs.  Record which registers
   are referenced as paradoxical subregs.  Also check for cases in which
   the current function needs to save a register that one of its call
   instructions clobbers.

   These things are logically unrelated, but it's more efficient to do
   them together.  */
static void update_equiv_regs_prescan (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
   MtcsFuncAbi *mtcsFuncAbi=mtcs_target_get_func_abi(mtcsTarget);

   basic_block bb;
   rtx_insn *insn;
   mtcs_function_abi_aggregator callee_abis(mtcsFuncAbi);

   FOR_EACH_BB_FN (bb, cfun)
      FOR_BB_INSNS (bb, insn)
         if (NONDEBUG_INSN_P (insn)){
            set_paradoxical_subreg(self,insn);
            if (CALL_P (insn))
               callee_abis.note_callee_abi (mtcs_func_abi_insn_callee_abi/*!insn_callee_abi*/(mtcsFuncAbi,insn));
         }
   mtcs_function_abi ret(*mtcsRtlData/*!crtl*/->abi,mtcsFuncAbi);
   HardRegSet extra_caller_saves = callee_abis.caller_save_regs (ret/*!crtl->abi*/);
   if (!mtcs_reg_hard_reg_set_empty_p/*!hard_reg_set_empty_p*/(&extra_caller_saves))
      for (unsigned int regno = 0; regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); ++regno)
         if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&extra_caller_saves, regno))
            mtcs_dfscan_df_set_regs_ever_live/*!df_set_regs_ever_live*/(mtcsDfscan,regno, true);
}

/* Find registers that are equivalent to a single value throughout the
   compilation (either because they can be referenced in memory or are
   set once from a single constant).  Lower their priority for a
   register.

   If such a register is only referenced once, try substituting its
   value into the using insn.  If it succeeds, we can eliminate the
   register completely.

   Initialize init_insns in ira_reg_equiv array.  */
static void update_equiv_regs (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   rtx_insn *insn;
   basic_block bb;

   /* Scan the insns and find which registers have equivalences.  Do this
   in a separate scan of the insns because (due to -fcse-follow-jumps)
   a register can be set below its use.  */
   bitmap setjmp_crosses = regstat_get_setjmp_crosses ();
   FOR_EACH_BB_FN (bb, cfun){
      int loop_depth = bb_loop_depth (bb);

      for (insn = BB_HEAD (bb); insn != NEXT_INSN (BB_END (bb));insn = NEXT_INSN (insn)){
         rtx note;
         rtx set;
         rtx dest, src;
         int regno;

         if (! INSN_P (insn))
            continue;

         for (note = REG_NOTES (insn); note; note = XEXP (note, 1))
            if (REG_NOTE_KIND (note) == REG_INC)
               no_equiv(XEXP (note, 0), note, (void*)self/*!NULL*/);

         set = single_set (insn);

         /* If this insn contains more (or less) than a single SET,
         only mark all destinations as having no known equivalence.  */
         if (set == NULL_RTX || side_effects_p (SET_SRC (set))){
            mtcs_rtlanal_note_pattern_stores/*!note_pattern_stores*/(mtcsRtlanal,PATTERN (insn), no_equiv, (void*)self/*!NULL*/);
            continue;
         }else if (GET_CODE (PATTERN (insn)) == PARALLEL){
            int i;

            for (i = XVECLEN (PATTERN (insn), 0) - 1; i >= 0; i--){
               rtx part = XVECEXP (PATTERN (insn), 0, i);
               if (part != set)
                  mtcs_rtlanal_note_pattern_stores/*!note_pattern_stores*/(mtcsRtlanal,part, no_equiv, (void*)self/*!NULL*/);
            }
         }

         dest = SET_DEST (set);
         src = SET_SRC (set);

         /* See if this is setting up the equivalence between an argument
         register and its stack slot.  */
         note = find_reg_note (insn, REG_EQUIV, NULL_RTX);
         if (note){
            gcc_assert (REG_P (dest));
            regno = REGNO (dest);

            /* Note that we don't want to clear init_insns in
            ira_reg_equiv even if there are multiple sets of this
            register.  */
            self->reg_equiv[regno].is_arg_equivalence = 1;

            /* The insn result can have equivalence memory although
            the equivalence is not set up by the insn.  We add
            this insn to init insns as it is a flag for now that
            regno has an equivalence.  We will remove the insn
            from init insn list later.  */
            if (rtx_equal_p (src, XEXP (note, 0)) || MEM_P (XEXP (note, 0)))
               self->ira_reg_equiv[regno].init_insns = gen_rtx_INSN_LIST (VOIDmode, insn, self->ira_reg_equiv[regno].init_insns);
            /* Continue normally in case this is a candidate for
            replacements.  */
         }

         if (!mtcsOptionsItem->x_optimize)
            continue;

         /* We only handle the case of a pseudo register being set
         once, or always to the same value.  */
         /* ??? The mn10200 port breaks if we add equivalences for
         values that need an ADDRESS_REGS register and set them equivalent
         to a MEM of a pseudo.  The actual problem is in the over-conservative
         handling of INPADDR_ADDRESS / INPUT_ADDRESS / INPUT triples in
         calculate_needs, but we traditionally work around this problem
         here by rejecting equivalences when the destination is in a register
         that's likely spilled.  This is fragile, of course, since the
         preferred class of a pseudo depends on all instructions that set
         or use it.  */

         if (!REG_P (dest)
         || (regno = REGNO (dest)) < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
         || (self->reg_equiv[regno].init_insns
         && self->reg_equiv[regno].init_insns->insn () == NULL)
         || (mtcsTarget/*!targetm.class_likely_spilled_p*/->class_likely_spilled_p(mtcsTarget,
               mtcs_reg_reg_preferred_class/*!reg_preferred_class*/(mtcsReg,regno))
         && MEM_P (src) && ! self->reg_equiv[regno].is_arg_equivalence)){
            /* This might be setting a SUBREG of a pseudo, a pseudo that is
            also set somewhere else to a constant.  */
            mtcs_rtlanal_note_pattern_stores/*!note_pattern_stores*/(mtcsRtlanal,set, no_equiv, NULL);
            continue;
         }

         /* Don't set reg mentioned in a paradoxical subreg
         equivalent to a mem.  */
         if (MEM_P (src) && self->reg_equiv[regno].pdx_subregs){
            mtcs_rtlanal_note_pattern_stores/*!note_pattern_stores*/(mtcsRtlanal,set, no_equiv, NULL);
            continue;
         }

         note = find_reg_note (insn, REG_EQUAL, NULL_RTX);

         /* cse sometimes generates function invariants, but doesn't put a
         REG_EQUAL note on the insn.  Since this note would be redundant,
         there's no point creating it earlier than here.  */
         if (! note && ! mtcs_rtlanal_rtx_varies_p/*!rtx_varies_p*/(mtcsRtlanal,src, 0))
            note = mtcs_rtl_set_unique_reg_note/*!set_unique_reg_note*/(mtcsRTL,insn, REG_EQUAL, copy_rtx (src));

         /* Don't bother considering a REG_EQUAL note containing an EXPR_LIST
         since it represents a function call.  */
         if (note && GET_CODE (XEXP (note, 0)) == EXPR_LIST)
            note = NULL_RTX;

         if (DF_REG_DEF_COUNT (regno) != 1){
            bool equal_p = true;
            rtx_insn_list *list;

            /* If we have already processed this pseudo and determined it
            cannot have an equivalence, then honor that decision.  */
            if (self->reg_equiv[regno].no_equiv)
               continue;

            if (! note
            || mtcs_rtlanal_rtx_varies_p/*!rtx_varies_p*/(mtcsRtlanal,XEXP (note, 0), 0)
            || (self->reg_equiv[regno].replacement
            && ! rtx_equal_p (XEXP (note, 0), self->reg_equiv[regno].replacement))){
               no_equiv (dest, set, (void *)self/*!NULL*/);
               continue;
            }

            list = self->reg_equiv[regno].init_insns;
            for (; list; list = list->next ()){
               rtx note_tmp;
               rtx_insn *insn_tmp;

               insn_tmp = list->insn ();
               note_tmp = find_reg_note (insn_tmp, REG_EQUAL, NULL_RTX);
               gcc_assert (note_tmp);
               if (! rtx_equal_p (XEXP (note, 0), XEXP (note_tmp, 0))){
                  equal_p = false;
                  break;
               }
            }

            if (! equal_p){
               no_equiv (dest, set, (void *)self/*!NULL*/);
               continue;
            }
         }

         /* Record this insn as initializing this register.  */
         self->reg_equiv[regno].init_insns = gen_rtx_INSN_LIST (VOIDmode, insn, self->reg_equiv[regno].init_insns);

         /* If this register is known to be equal to a constant, record that
         it is always equivalent to the constant.
         Note that it is possible to have a register use before
         the def in loops (see gcc.c-torture/execute/pr79286.c)
         where the reg is undefined on first use.  If the def insn
         won't trap we can use it as an equivalence, effectively
         choosing the "undefined" value for the reg to be the
         same as the value set by the def.  */
         if (DF_REG_DEF_COUNT (regno) == 1
         && note
         && !mtcs_rtlanal_rtx_varies_p/*!rtx_varies_p*/(mtcsRtlanal,XEXP (note, 0), 0)
         && (!mtcs_rtlanal_may_trap_or_fault_p/*!may_trap_or_fault_p*/(mtcsRtlanal,XEXP (note, 0))
         || def_dominates_uses(self,regno))){
            rtx note_value = XEXP (note, 0);
            mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,insn, note);
            mtcs_rtl_set_unique_reg_note/*!set_unique_reg_note*/(mtcsRTL,insn, REG_EQUIV, note_value);
         }

         /* If this insn introduces a "constant" register, decrease the priority
         of that register.  Record this insn if the register is only used once
         more and the equivalence value is the same as our source.

         The latter condition is checked for two reasons:  First, it is an
         indication that it may be more efficient to actually emit the insn
         as written (if no registers are available, reload will substitute
         the equivalence).  Secondly, it avoids problems with any registers
         dying in this insn whose death notes would be missed.

         If we don't have a REG_EQUIV note, see if this insn is loading
         a register used only in one basic block from a MEM.  If so, and the
         MEM remains unchanged for the life of the register, add a REG_EQUIV
         note.  */
         note = find_reg_note (insn, REG_EQUIV, NULL_RTX);

         rtx replacement = NULL_RTX;
         if (note)
            replacement = XEXP (note, 0);
         else if (REG_BASIC_BLOCK (regno) >= NUM_FIXED_BLOCKS  && MEM_P (SET_SRC (set))){
            enum valid_equiv validity;
            validity = validate_equiv_mem(self,insn, dest, SET_SRC (set));
            if (validity != valid_none){
               replacement = copy_rtx (SET_SRC (set));
               if (validity == valid_reload){
                  note = mtcs_rtl_set_unique_reg_note/*!set_unique_reg_note*/(mtcsRTL,insn, REG_EQUIV, replacement);
               }else if (self->ira_use_lra_p){
                  /* We still can use this equivalence for caller save
                  optimization in LRA.  Mark this.  */
                  self->ira_reg_equiv[regno].caller_save_p = true;
                  self->ira_reg_equiv[regno].init_insns = gen_rtx_INSN_LIST (VOIDmode, insn, self->ira_reg_equiv[regno].init_insns);
               }
            }
         }

         /* If we haven't done so, record for reload that this is an
         equivalencing insn.  */
         if (note && !self->reg_equiv[regno].is_arg_equivalence)
            self->ira_reg_equiv[regno].init_insns = gen_rtx_INSN_LIST (VOIDmode, insn,self->ira_reg_equiv[regno].init_insns);

         if (replacement){
            self->reg_equiv[regno].replacement = replacement;
            self->reg_equiv[regno].src_p = &SET_SRC (set);
            self->reg_equiv[regno].loop_depth = (short) loop_depth;

            /* Don't mess with things live during setjmp.  */
            if (mtcsOptionsItem->x_optimize && !bitmap_bit_p (setjmp_crosses, regno)){
               /* If the register is referenced exactly twice, meaning it is
               set once and used once, indicate that the reference may be
               replaced by the equivalence we computed above.  Do this
               even if the register is only used in one block so that
               dependencies can be handled where the last register is
               used in a different block (i.e. HIGH / LO_SUM sequences)
               and to reduce the number of registers alive across
               calls.  */

               if (REG_N_REFS (regno) == 2  && (rtx_equal_p (replacement, src)
               || ! equiv_init_varies_p(self,src)) && NONJUMP_INSN_P (insn)
               && equiv_init_movable_p(self,PATTERN (insn), regno))
                  self->reg_equiv[regno].replace = 1;
            }
         }
      }// end  for (insn = BB_HEAD (bb); ...
   }//end    FOR_EACH_BB_FN (bb, cfun){
}

/* For insns that set a MEM to the contents of a REG that is only used
   in a single basic block, see if the register is always equivalent
   to that memory location and if moving the store from INSN to the
   insn that sets REG is safe.  If so, put a REG_EQUIV note on the
   initializing insn.  */
static void add_store_equivs (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   auto_bitmap seen_insns;

   for (rtx_insn *insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = NEXT_INSN (insn)){
      rtx set, src, dest;
      unsigned regno;
      rtx_insn *init_insn;

      bitmap_set_bit (seen_insns, INSN_UID (insn));

      if (! INSN_P (insn))
         continue;

      set = single_set (insn);
      if (! set)
         continue;

      dest = SET_DEST (set);
      src = SET_SRC (set);

      /* Don't add a REG_EQUIV note if the insn already has one.  The existing
      REG_EQUIV is likely more useful than the one we are adding.  */
      if (MEM_P (dest) && REG_P (src)
      && (regno = REGNO (src)) >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
      && REG_BASIC_BLOCK (regno) >= NUM_FIXED_BLOCKS
      && DF_REG_DEF_COUNT (regno) == 1
      && ! self->reg_equiv[regno].pdx_subregs
      && self->reg_equiv[regno].init_insns != NULL
      && (init_insn = self->reg_equiv[regno].init_insns->insn ()) != 0
      && bitmap_bit_p (seen_insns, INSN_UID (init_insn))
      && ! find_reg_note (init_insn, REG_EQUIV, NULL_RTX)
      && validate_equiv_mem(self,init_insn, src, dest) == valid_reload
      && ! memref_used_between_p(self,dest, init_insn, insn)
      /* Attaching a REG_EQUIV note will fail if INIT_INSN has
      multiple sets.  */
      && mtcs_rtl_set_unique_reg_note/*!set_unique_reg_note*/(mtcsRTL,init_insn, REG_EQUIV, copy_rtx (dest))){
         /* This insn makes the equivalence, not the one initializing
         the register.  */
         self->ira_reg_equiv[regno].init_insns = gen_rtx_INSN_LIST (VOIDmode, insn, NULL_RTX);
         mtcs_dfscan_df_notes_rescan/*!df_notes_rescan*/(mtcsDfscan,init_insn);
         if (dump_file)
            fprintf (dump_file,"Adding REG_EQUIV to insn %d for source of insn %d\n",INSN_UID (init_insn),INSN_UID (insn));
      }
   }
}

/* Scan all regs killed in an insn to see if any of them are registers
   only used that once.  If so, see if we can replace the reference
   with the equivalent form.  If we can, delete the initializing
   reference and this register will go away.  If we can't replace the
   reference, and the initializing reference is within the same loop
   (or in an inner loop), then move the register initialization just
   before the use, so that they are in the same basic block.  */
static void combine_and_move_insns (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   auto_bitmap cleared_regs;
   int max = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);

   for (int regno = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); regno < max; regno++){
      if (!self->reg_equiv[regno].replace)
         continue;

      rtx_insn *use_insn = 0;
      for (df_ref use = DF_REG_USE_CHAIN (regno);use; use = DF_REF_NEXT_REG (use))
         if (DF_REF_INSN_INFO (use)){
            if (DEBUG_INSN_P (DF_REF_INSN (use)))
               continue;
            gcc_assert (!use_insn);
            use_insn = DF_REF_INSN (use);
         }
      gcc_assert (use_insn);
      /* Don't substitute into jumps.  indirect_jump_optimize does
      this for anything we are prepared to handle.  */
      if (JUMP_P (use_insn))
         continue;
      /* Also don't substitute into a conditional trap insn -- it can become
      an unconditional trap, and that is a flow control insn.  */
      if (GET_CODE (PATTERN (use_insn)) == TRAP_IF)
         continue;

      df_ref def = DF_REG_DEF_CHAIN (regno);
      gcc_assert (DF_REG_DEF_COUNT (regno) == 1 && DF_REF_INSN_INFO (def));
      rtx_insn *def_insn = DF_REF_INSN (def);
      /* We may not move instructions that can throw, since that
      changes basic block boundaries and we are not prepared to
      adjust the CFG to match.  */
      if (can_throw_internal (def_insn))
         continue;

      /* Instructions with multiple sets can only be moved if DF analysis is
      performed for all of the registers set.  See PR91052.  */
      if (multiple_sets (def_insn))
         continue;

      basic_block use_bb = BLOCK_FOR_INSN (use_insn);
      basic_block def_bb = BLOCK_FOR_INSN (def_insn);
      if (bb_loop_depth (use_bb) > bb_loop_depth (def_bb))
         continue;

      if (asm_noperands (PATTERN (def_insn)) < 0
      && mtcs_recog_validate_replace_rtx/*!validate_replace_rtx*/(mtcsRecog,mtcsRtlData->regno_reg_rtx/*! regno_reg_rtx*/[regno],
      *self->reg_equiv[regno].src_p, use_insn)){
         rtx link;
         /* Append the REG_DEAD notes from def_insn.  */
         for (rtx *p = &REG_NOTES (def_insn); (link = *p) != 0; ){
            if (REG_NOTE_KIND (XEXP (link, 0)) == REG_DEAD){
               *p = XEXP (link, 1);
               XEXP (link, 1) = REG_NOTES (use_insn);
               REG_NOTES (use_insn) = link;
            }else
               p = &XEXP (link, 1);
         }

         remove_death (regno, use_insn);
         SET_REG_N_REFS (regno, 0);
         REG_FREQ (regno) = 0;
         df_ref use;
         FOR_EACH_INSN_USE (use, def_insn){
            unsigned int use_regno = DF_REF_REGNO (use);
            if (!mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,use_regno))
               self->reg_equiv[use_regno].replace = 0;
         }

         mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,def_insn);

         self->reg_equiv[regno].init_insns = NULL;
         self->ira_reg_equiv[regno].init_insns = NULL;
         bitmap_set_bit (cleared_regs, regno);
      }
      /* Move the initialization of the register to just before
      USE_INSN.  Update the flow information.  */
      else if (prev_nondebug_insn (use_insn) != def_insn){
         rtx_insn *new_insn;

         new_insn = mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,PATTERN (def_insn), use_insn);
         REG_NOTES (new_insn) = REG_NOTES (def_insn);
         REG_NOTES (def_insn) = 0;
         /* Rescan it to process the notes.  */
         mtcs_dfscan_df_insn_rescan/*!df_insn_rescan*/(mtcsDfscan,new_insn);

         /* Make sure this insn is recognized before reload begins,
         otherwise eliminate_regs_in_insn will die.  */
         INSN_CODE (new_insn) = INSN_CODE (def_insn);

         mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,def_insn);

         XEXP (self->reg_equiv[regno].init_insns, 0) = new_insn;

         REG_BASIC_BLOCK (regno) = use_bb->index;
         REG_N_CALLS_CROSSED (regno) = 0;

         if (use_insn == BB_HEAD (use_bb))
            BB_HEAD (use_bb) = new_insn;

         /* We know regno dies in use_insn, but inside a loop
         REG_DEAD notes might be missing when def_insn was in
         another basic block.  However, when we move def_insn into
         this bb we'll definitely get a REG_DEAD note and reload
         will see the death.  It's possible that update_equiv_regs
         set up an equivalence referencing regno for a reg set by
         use_insn, when regno was seen as non-local.  Now that
         regno is local to this block, and dies, such an
         equivalence is invalid.  */
         if (find_reg_note (use_insn, REG_EQUIV, mtcsRtlData->regno_reg_rtx/*! regno_reg_rtx*/[regno])){
            rtx set = single_set (use_insn);
            if (set && REG_P (SET_DEST (set)))
               no_equiv (SET_DEST (set), set, (void *)self/*!NULL*/);
         }

         self->ira_reg_equiv[regno].init_insns = gen_rtx_INSN_LIST (VOIDmode, new_insn, NULL_RTX);
         bitmap_set_bit (cleared_regs, regno);
      }
   }

   if (!bitmap_empty_p (cleared_regs)){
      basic_block bb;

      FOR_EACH_BB_FN (bb, cfun){
         bitmap_and_compl_into (DF_LR_IN (bb), cleared_regs);
         bitmap_and_compl_into (DF_LR_OUT (bb), cleared_regs);
         if (!df_live)
            continue;
         bitmap_and_compl_into (DF_LIVE_IN (bb), cleared_regs);
         bitmap_and_compl_into (DF_LIVE_OUT (bb), cleared_regs);
      }

      /* Last pass - adjust debug insns referencing cleared regs.  */
      if (MAY_HAVE_DEBUG_BIND_INSNS)
         for (rtx_insn *insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = NEXT_INSN (insn))
            if (DEBUG_BIND_INSN_P (insn)){
               AdjustClearedRegsCallBackData userData={cleared_regs,self};
               rtx old_loc = INSN_VAR_LOCATION_LOC (insn);
               INSN_VAR_LOCATION_LOC (insn) = simplify_replace_fn_rtx (old_loc, NULL_RTX,
                     adjust_cleared_regs, (void *) &userData/*!cleared_regs*/);
               if (old_loc != INSN_VAR_LOCATION_LOC (insn))
                  mtcs_dfscan_df_insn_rescan/*!df_insn_rescan*/(mtcsDfscan,insn);
            }
   }
}

/* A pass over indirect jumps, converting simple cases to direct jumps.
   Combine does this optimization too, but only within a basic block.  */
static void indirect_jump_optimize (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsCfgCleanup *mtcsCfgCleanup=mtcs_target_get_cfg_cleanup(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);

   basic_block bb;
   bool rebuild_p = false;

   FOR_EACH_BB_REVERSE_FN (bb, cfun){
      rtx_insn *insn = BB_END (bb);
      if (!JUMP_P (insn) || find_reg_note (insn, REG_NON_LOCAL_GOTO, NULL_RTX))
         continue;

      rtx x = pc_set (insn);
      if (!x || !REG_P (SET_SRC (x)))
         continue;

      int regno = REGNO (SET_SRC (x));
      if (DF_REG_DEF_COUNT (regno) == 1) {
         df_ref def = DF_REG_DEF_CHAIN (regno);
         if (!DF_REF_IS_ARTIFICIAL (def)){
            rtx_insn *def_insn = DF_REF_INSN (def);
            rtx lab = NULL_RTX;
            rtx set = single_set (def_insn);
            if (set && GET_CODE (SET_SRC (set)) == LABEL_REF)
               lab = SET_SRC (set);
            else{
               rtx eqnote = find_reg_note (def_insn, REG_EQUAL, NULL_RTX);
               if (eqnote && GET_CODE (XEXP (eqnote, 0)) == LABEL_REF)
                  lab = XEXP (eqnote, 0);
            }
            if (lab && mtcs_recog_validate_replace_rtx/*!validate_replace_rtx*/(mtcsRecog,SET_SRC (x), lab, insn))
               rebuild_p = true;
         }
      }
   }

   if (rebuild_p){
      rebuild_jump_labels (mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData));
      if (mtcs_cfg_rtl_purge_all_dead_edges/*!purge_all_dead_edges*/(mtcsCfgRtl))
         mtcs_cfg_cleanup_delete_unreachable_blocks/*!delete_unreachable_blocks*/(mtcsCfgCleanup);
   }
}

/* Set up fields memory, constant, and invariant from init_insns in
   the structures of array ira_reg_equiv.  */
static void setup_reg_equiv (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsReload1 *mtcsReload1=mtcs_target_get_reload1(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   int i;
   rtx_insn_list *elem, *prev_elem, *next_elem;
   rtx_insn *insn;
   rtx set, x;

   for (i = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i < self->ira_reg_equiv_len; i++)
      for (prev_elem = NULL, elem = self->ira_reg_equiv[i].init_insns;elem; prev_elem = elem, elem = next_elem){
         next_elem = elem->next ();
         insn = elem->insn ();
         set = single_set (insn);

         /* Init insns can set up equivalence when the reg is a destination or
         a source (in this case the destination is memory).  */
         if (set != 0 && (REG_P (SET_DEST (set)) || REG_P (SET_SRC (set)))){
            if ((x = find_reg_note (insn, REG_EQUIV, NULL_RTX)) != NULL){
               x = XEXP (x, 0);
               if (REG_P (SET_DEST (set)) && REGNO (SET_DEST (set)) == (unsigned int) i
                     && ! rtx_equal_p (SET_SRC (set), x) && MEM_P (x)){
                  /* This insn reporting the equivalence but
                  actually not setting it.  Remove it from the
                  list.  */
                  if (prev_elem == NULL)
                     self->ira_reg_equiv[i].init_insns = next_elem;
                  else
                     XEXP (prev_elem, 1) = next_elem;
                  elem = prev_elem;
               }
            }else if (REG_P (SET_DEST (set))  && REGNO (SET_DEST (set)) == (unsigned int) i)
               x = SET_SRC (set);
            else{
               gcc_assert (REG_P (SET_SRC (set))  && REGNO (SET_SRC (set)) == (unsigned int) i);
               x = SET_DEST (set);
            }
            if (! mtcs_reload1_function_invariant_p/*!function_invariant_p*/(mtcsReload1,x) || ! mtcsOptionsItem->x_flag_pic
            /* A function invariant is often CONSTANT_P but may
            include a register.  We promise to only pass
            CONSTANT_P objects to LEGITIMATE_PIC_OPERAND_P.  */
            || (CONSTANT_P (x) && mtcs_recog_is_legitimate_pic_operand_p/*!LEGITIMATE_PIC_OPERAND_P*/(mtcsRecog,x))){
               /* It can happen that a REG_EQUIV note contains a MEM
               that is not a legitimate memory operand.  As later
               stages of reload assume that all addresses found in
               the lra_regno_equiv_* arrays were originally
               legitimate, we ignore such REG_EQUIV notes.  */
               if (mtcs_preds_memory_operand/*!memory_operand*/(mtcsPreds,x, VOIDmode)){
                  self->ira_reg_equiv[i].defined_p = !self->ira_reg_equiv[i].caller_save_p;
                  self->ira_reg_equiv[i].memory = x;
                  continue;
               }else if (mtcs_reload1_function_invariant_p/*!function_invariant_p*/(mtcsReload1,x)){
                  machine_mode mode;

                  mode = GET_MODE (SET_DEST (set));
                  if (GET_CODE (x) == PLUS || x == mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL)
                        || x == mtcs_rtl_get_arg_pointer_rtx/*!arg_pointer_rtx*/(mtcsRTL))
                     /* This is PLUS of frame pointer and a constant,
                     or fp, or argp.  */
                     self->ira_reg_equiv[i].invariant = x;
                  else if (targetm.legitimate_constant_p (mode, x))
                     self->ira_reg_equiv[i].constant = x;
                  else{
                     self->ira_reg_equiv[i].memory = mtcs_asm_force_const_mem/*!force_const_mem*/(mtcsAsm,mode, x);
                     if (self->ira_reg_equiv[i].memory == NULL_RTX){
                        self->ira_reg_equiv[i].defined_p = false;
                        self->ira_reg_equiv[i].caller_save_p = false;
                        self->ira_reg_equiv[i].init_insns = NULL;
                        break;
                     }
                  }
                  self->ira_reg_equiv[i].defined_p = true;
                  continue;
               }
            }
         }
         self->ira_reg_equiv[i].defined_p = false;
         self->ira_reg_equiv[i].caller_save_p = false;
         self->ira_reg_equiv[i].init_insns = NULL;
         break;
      }
}

/* Print chain C to FILE.  */
static void print_insn_chain (FILE *file, class insn_chain *c)
{
   fprintf (file, "insn=%d, ", INSN_UID (c->insn));
   bitmap_print (file, &c->live_throughout, "live_throughout: ", ", ");
   bitmap_print (file, &c->dead_or_set, "dead_or_set: ", "\n");
}


/* Print all reload_insn_chains to FILE.  */
static void print_insn_chains (FILE *file)
{
   class insn_chain *c;
   for (c = reload_insn_chain; c ; c = c->next)
      print_insn_chain (file, c);
}

/* Return true if pseudo REGNO should be added to set live_throughout
   or dead_or_set of the insn chains for reload consideration.  */
static bool pseudo_for_reload_consideration_p (MtcsIra *self,int regno)
{
  /* Consider spilled pseudos too for IRA because they still have a
     chance to get hard-registers in the reload when IRA is used.  */
   return (reg_renumber[regno] >= 0 || self->ira_conflicts_p);
}

/* Return true if we can track the individual bytes of subreg X.
   When returning true, set *OUTER_SIZE to the number of bytes in
   X itself, *INNER_SIZE to the number of bytes in the inner register
   and *START to the offset of the first byte.  */
static bool get_subreg_tracking_sizes (MtcsIra *self,rtx x, HOST_WIDE_INT *outer_size,
            HOST_WIDE_INT *inner_size, HOST_WIDE_INT *start)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   rtx reg = mtcsRtlData->regno_reg_rtx/*! regno_reg_rtx*/[REGNO (SUBREG_REG (x))];
   return (mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (x)).is_constant (outer_size)
      && mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (reg)).is_constant (inner_size)
      && SUBREG_BYTE (x).is_constant (start));
}

/* Init LIVE_SUBREGS[ALLOCNUM] and LIVE_SUBREGS_USED[ALLOCNUM] for
   a register with SIZE bytes, making the register live if INIT_VALUE.  */
static void init_live_subregs (bool init_value, sbitmap *live_subregs,
         bitmap live_subregs_used, int allocnum, int size)
{
   gcc_assert (size > 0);
   /* Been there, done that.  */
   if (bitmap_bit_p (live_subregs_used, allocnum))
      return;
   /* Create a new one.  */
   if (live_subregs[allocnum] == NULL)
      live_subregs[allocnum] = sbitmap_alloc (size);
   /* If the entire reg was live before blasting into subregs, we need
   to init all of the subregs to ones else init to 0.  */
   if (init_value)
      bitmap_ones (live_subregs[allocnum]);
   else
      bitmap_clear (live_subregs[allocnum]);
   bitmap_set_bit (live_subregs_used, allocnum);
}

/* Walk the insns of the current function and build reload_insn_chain,
   and record register life information.  */
static void build_insn_chain (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   unsigned int i;
   class insn_chain **p = &reload_insn_chain;
   basic_block bb;
   class insn_chain *c = NULL;
   class insn_chain *next = NULL;
   auto_bitmap live_relevant_regs;
   auto_bitmap elim_regset;
   /* live_subregs is a vector used to keep accurate information about
   which hardregs are live in multiword pseudos.  live_subregs and
   live_subregs_used are indexed by pseudo number.  The live_subreg
   entry for a particular pseudo is only used if the corresponding
   element is non zero in live_subregs_used.  The sbitmap size of
   live_subreg[allocno] is number of bytes that the pseudo can
   occupy.  */
   sbitmap *live_subregs = XCNEWVEC (sbitmap, max_regno);
   auto_bitmap live_subregs_used;

   for (i = 0; i < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i++)
      if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&self->eliminable_regset, i))
         bitmap_set_bit (elim_regset, i);

   FOR_EACH_BB_REVERSE_FN (bb, cfun){
      bitmap_iterator bi;
      rtx_insn *insn;

      CLEAR_REG_SET (live_relevant_regs);
      bitmap_clear (live_subregs_used);

      EXECUTE_IF_SET_IN_BITMAP (df_get_live_out (bb), 0, i, bi){
         if (i >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg))
            break;
         bitmap_set_bit (live_relevant_regs, i);
      }

      EXECUTE_IF_SET_IN_BITMAP (df_get_live_out (bb),
            mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg), i, bi){
         if (pseudo_for_reload_consideration_p(self,i))
            bitmap_set_bit (live_relevant_regs, i);
      }

      FOR_BB_INSNS_REVERSE (bb, insn){
         if (!NOTE_P (insn) && !BARRIER_P (insn)){
            struct df_insn_info *insn_info = DF_INSN_INFO_GET (insn);
            df_ref def, use;

            c = new_insn_chain ();
            c->next = next;
            next = c;
            *p = c;
            p = &c->prev;

            c->insn = insn;
            c->block = bb->index;

            if (NONDEBUG_INSN_P (insn))
               FOR_EACH_INSN_INFO_DEF (def, insn_info){
                  unsigned int regno = DF_REF_REGNO (def);

                  /* Ignore may clobbers because these are generated
                  from calls. However, every other kind of def is
                  added to dead_or_set.  */
                  if (!DF_REF_FLAGS_IS_SET (def, DF_REF_MAY_CLOBBER)){
                     if (regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)){
                        if (!mtcsReg->hardRegs.x_fixed_regs/*!fixed_regs*/[regno])
                           bitmap_set_bit (&c->dead_or_set, regno);
                     }else if (pseudo_for_reload_consideration_p(self,regno))
                        bitmap_set_bit (&c->dead_or_set, regno);
                  }

                  if ((regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
                  || reg_renumber[regno] >= 0
                  || self->ira_conflicts_p)
                  && (!DF_REF_FLAGS_IS_SET (def, DF_REF_CONDITIONAL))){
                     rtx reg = DF_REF_REG (def);
                     HOST_WIDE_INT outer_size, inner_size, start;

                     /* We can usually track the liveness of individual
                     bytes within a subreg.  The only exceptions are
                     subregs wrapped in ZERO_EXTRACTs and subregs whose
                     size is not known; in those cases we need to be
                     conservative and treat the definition as a partial
                     definition of the full register rather than a full
                     definition of a specific part of the register.  */
                     if (GET_CODE (reg) == SUBREG
                     && !DF_REF_FLAGS_IS_SET (def, DF_REF_ZERO_EXTRACT)
                     && get_subreg_tracking_sizes(self,reg, &outer_size,&inner_size, &start)){
                        HOST_WIDE_INT last = start + outer_size;

                        init_live_subregs(bitmap_bit_p (live_relevant_regs, regno),
                        live_subregs, live_subregs_used, regno,inner_size);

                        if (!DF_REF_FLAGS_IS_SET (def, DF_REF_STRICT_LOW_PART)){
                           /* Expand the range to cover entire words.
                           Bytes added here are "don't care".  */
                           start = start / UNITS_PER_WORD * UNITS_PER_WORD;
                           last = ((last + UNITS_PER_WORD - 1) / UNITS_PER_WORD * UNITS_PER_WORD);
                        }

                        /* Ignore the paradoxical bits.  */
                        if (last > SBITMAP_SIZE (live_subregs[regno]))
                           last = SBITMAP_SIZE (live_subregs[regno]);

                        while (start < last){
                           bitmap_clear_bit (live_subregs[regno], start);
                           start++;
                        }

                        if (bitmap_empty_p (live_subregs[regno])){
                           bitmap_clear_bit (live_subregs_used, regno);
                           bitmap_clear_bit (live_relevant_regs, regno);
                        }else
                           /* Set live_relevant_regs here because
                           that bit has to be true to get us to
                           look at the live_subregs fields.  */
                           bitmap_set_bit (live_relevant_regs, regno);
                     }else{
                        /* DF_REF_PARTIAL is generated for
                        subregs, STRICT_LOW_PART, and
                        ZERO_EXTRACT.  We handle the subreg
                        case above so here we have to keep from
                        modeling the def as a killing def.  */
                        if (!DF_REF_FLAGS_IS_SET (def, DF_REF_PARTIAL)){
                           bitmap_clear_bit (live_subregs_used, regno);
                           bitmap_clear_bit (live_relevant_regs, regno);
                        }
                     }
                  }//end if ((regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_
               }//end  FOR_EACH_INSN_INFO_DEF (def, insn_info)

            bitmap_and_compl_into (live_relevant_regs, elim_regset);
            bitmap_copy (&c->live_throughout, live_relevant_regs);

            if (NONDEBUG_INSN_P (insn))
               FOR_EACH_INSN_INFO_USE (use, insn_info){
                  unsigned int regno = DF_REF_REGNO (use);
                  rtx reg = DF_REF_REG (use);

                  /* DF_REF_READ_WRITE on a use means that this use
                  is fabricated from a def that is a partial set
                  to a multiword reg.  Here, we only model the
                  subreg case that is not wrapped in ZERO_EXTRACT
                  precisely so we do not need to look at the
                  fabricated use.  */
                  if (DF_REF_FLAGS_IS_SET (use, DF_REF_READ_WRITE)
                  && !DF_REF_FLAGS_IS_SET (use, DF_REF_ZERO_EXTRACT)
                  && DF_REF_FLAGS_IS_SET (use, DF_REF_SUBREG))
                     continue;

                  /* Add the last use of each var to dead_or_set.  */
                  if (!bitmap_bit_p (live_relevant_regs, regno)){
                     if (regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)){
                        if (!mtcsReg->hardRegs.x_fixed_regs/*!fixed_regs*/[regno])
                           bitmap_set_bit (&c->dead_or_set, regno);
                     }else if (pseudo_for_reload_consideration_p(self,regno))
                           bitmap_set_bit (&c->dead_or_set, regno);
                  }

                  if (regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
                  || pseudo_for_reload_consideration_p(self,regno)){
                     HOST_WIDE_INT outer_size, inner_size, start;
                     if (GET_CODE (reg) == SUBREG
                     && !DF_REF_FLAGS_IS_SET (use, DF_REF_SIGN_EXTRACT | DF_REF_ZERO_EXTRACT)
                     && get_subreg_tracking_sizes(self,reg, &outer_size, &inner_size, &start)){
                        HOST_WIDE_INT last = start + outer_size;

                        init_live_subregs(bitmap_bit_p (live_relevant_regs, regno),
                        live_subregs, live_subregs_used, regno,inner_size);

                        /* Ignore the paradoxical bits.  */
                        if (last > SBITMAP_SIZE (live_subregs[regno]))
                           last = SBITMAP_SIZE (live_subregs[regno]);

                        while (start < last){
                           bitmap_set_bit (live_subregs[regno], start);
                           start++;
                        }
                     }else
                        /* Resetting the live_subregs_used is
                        effectively saying do not use the subregs
                        because we are reading the whole
                        pseudo.  */
                        bitmap_clear_bit (live_subregs_used, regno);
                     bitmap_set_bit (live_relevant_regs, regno);
                  }
               }
         }
      }

      /* FIXME!! The following code is a disaster.  Reload needs to see the
      labels and jump tables that are just hanging out in between
      the basic blocks.  See pr33676.  */
      insn = BB_HEAD (bb);

      /* Skip over the barriers and cruft.  */
      while (insn && (BARRIER_P (insn) || NOTE_P (insn) || BLOCK_FOR_INSN (insn) == bb))
         insn = PREV_INSN (insn);

      /* While we add anything except barriers and notes, the focus is
      to get the labels and jump tables into the
      reload_insn_chain.  */
      while (insn){
         if (!NOTE_P (insn) && !BARRIER_P (insn)){
            if (BLOCK_FOR_INSN (insn))
               break;

            c = new_insn_chain ();
            c->next = next;
            next = c;
            *p = c;
            p = &c->prev;

            /* The block makes no sense here, but it is what the old
            code did.  */
            c->block = bb->index;
            c->insn = insn;
            bitmap_copy (&c->live_throughout, live_relevant_regs);
         }
         insn = PREV_INSN (insn);
      }
   }

   reload_insn_chain = c;
   *p = NULL;

   for (i = 0; i < (unsigned int) max_regno; i++)
      if (live_subregs[i] != NULL)
         sbitmap_free (live_subregs[i]);
   free (live_subregs);

   if (dump_file)
      print_insn_chains (dump_file);
}

/* Examine the rtx found in *LOC, which is read or written to as determined
   by TYPE.  Return false if we find a reason why an insn containing this
   rtx should not be moved (such as accesses to non-constant memory), true
   otherwise.  */
static bool rtx_moveable_p (MtcsIra *self,rtx *loc, enum op_type type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   const char *fmt;
   rtx x = *loc;
   int i, j;

   enum rtx_code code = GET_CODE (x);
   switch (code){
      case CONST:
      CASE_CONST_ANY:
      case SYMBOL_REF:
      case LABEL_REF:
         return true;

      case PC:
         return type == OP_IN;

      case REG:
         if (x == mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL))
            return true;
         if (mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,x))
            return false;

         return true;

      case MEM:
         if (type == OP_IN && MEM_READONLY_P (x))
            return rtx_moveable_p(self,&XEXP (x, 0), OP_IN);
         return false;

      case SET:
         return (rtx_moveable_p(self,&SET_SRC (x), OP_IN) && rtx_moveable_p(self,&SET_DEST (x), OP_OUT));

      case STRICT_LOW_PART:
         return rtx_moveable_p(self,&XEXP (x, 0), OP_OUT);

      case ZERO_EXTRACT:
      case SIGN_EXTRACT:
         return (rtx_moveable_p(self,&XEXP (x, 0), type)
         && rtx_moveable_p(self,&XEXP (x, 1), OP_IN)
         && rtx_moveable_p(self,&XEXP (x, 2), OP_IN));

      case CLOBBER:
         return rtx_moveable_p(self,&SET_DEST (x), OP_OUT);

      case UNSPEC_VOLATILE:
         /* It is a bad idea to consider insns with such rtl
         as moveable ones.  The insn scheduler also considers them as barrier
         for a reason.  */
         return false;

      case ASM_OPERANDS:
      /* The same is true for volatile asm: it has unknown side effects, it
      cannot be moved at will.  */
      if (MEM_VOLATILE_P (x))
         return false;

      default:
         break;
   }

   fmt = GET_RTX_FORMAT (code);
   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--){
   if (fmt[i] == 'e'){
      if (!rtx_moveable_p(self,&XEXP (x, i), type))
      return false;
   }else if (fmt[i] == 'E')
      for (j = XVECLEN (x, i) - 1; j >= 0; j--){
         if (!rtx_moveable_p(self,&XVECEXP (x, i, j), type))
            return false;
      }
   }
   return true;
}

/* A wrapper around dominated_by_p, which uses the information in UID_LUID
   to give dominance relationships between two insns I1 and I2.  */
static bool insn_dominated_by_p (rtx i1, rtx i2, int *uid_luid)
{
  basic_block bb1 = BLOCK_FOR_INSN (i1);
  basic_block bb2 = BLOCK_FOR_INSN (i2);

  if (bb1 == bb2)
    return uid_luid[INSN_UID (i2)] < uid_luid[INSN_UID (i1)];
  return dominated_by_p (CDI_DOMINATORS, bb1, bb2);
}

/* Look for instances where we have an instruction that is known to increase
   register pressure, and whose result is not used immediately.  If it is
   possible to move the instruction downwards to just before its first use,
   split its lifetime into two ranges.  We create a new pseudo to compute the
   value, and emit a move instruction just before the first use.  If, after
   register allocation, the new pseudo remains unallocated, the function
   move_unallocated_pseudos then deletes the move instruction and places
   the computation just before the first use.

   Such a move is safe and profitable if all the input registers remain live
   and unchanged between the original computation and its first use.  In such
   a situation, the computation is known to increase register pressure, and
   moving it is known to at least not worsen it.

   We restrict moves to only those cases where a register remains unallocated,
   in order to avoid interfering too much with the instruction schedule.  As
   an exception, we may move insns which only modify their input register
   (typically induction variables), as this increases the freedom for our
   intended transformation, and does not limit the second instruction
   scheduler pass.  */
static void find_moveable_pseudos (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsCfgBuild *mtcsCfgBuild=mtcs_target_get_cfg_build(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   unsigned i;
   int max_regs = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);
   int max_uid = get_max_uid ();
   basic_block bb;
   int *uid_luid = XNEWVEC (int, max_uid);
   rtx_insn **closest_uses = XNEWVEC (rtx_insn *, max_regs);
   /* A set of registers which are live but not modified throughout a block.  */
   bitmap_head *bb_transp_live = XNEWVEC (bitmap_head,last_basic_block_for_fn (cfun));
   /* A set of registers which only exist in a given basic block.  */
   bitmap_head *bb_local = XNEWVEC (bitmap_head, last_basic_block_for_fn (cfun));
   /* A set of registers which are set once, in an instruction that can be
   moved freely downwards, but are otherwise transparent to a block.  */
   bitmap_head *bb_moveable_reg_sets = XNEWVEC (bitmap_head,last_basic_block_for_fn (cfun));
   auto_bitmap live, used, set, interesting, unusable_as_input;
   bitmap_iterator bi;

   self->first_moveable_pseudo = max_regs;
   self->pseudo_replaced_reg.release ();
   self->pseudo_replaced_reg.safe_grow_cleared (max_regs, true);

   mtcs_dfcore_df_analyze/*!df_analyze*/(mtcsDfcore);
   calculate_dominance_info (CDI_DOMINATORS);

   i = 0;
   FOR_EACH_BB_FN (bb, cfun){
      rtx_insn *insn;
      bitmap transp = bb_transp_live + bb->index;
      bitmap moveable = bb_moveable_reg_sets + bb->index;
      bitmap local = bb_local + bb->index;

      bitmap_initialize (local, 0);
      bitmap_initialize (transp, 0);
      bitmap_initialize (moveable, 0);
      bitmap_copy (live, df_get_live_out (bb));
      bitmap_and_into (live, df_get_live_in (bb));
      bitmap_copy (transp, live);
      bitmap_clear (moveable);
      bitmap_clear (live);
      bitmap_clear (used);
      bitmap_clear (set);
      FOR_BB_INSNS (bb, insn)
         if (NONDEBUG_INSN_P (insn)){
            df_insn_info *insn_info = DF_INSN_INFO_GET (insn);
            df_ref def, use;

            uid_luid[INSN_UID (insn)] = i++;

            def = df_single_def (insn_info);
            use = df_single_use (insn_info);
            if (use
            && def
            && DF_REF_REGNO (use) == DF_REF_REGNO (def)
            && !bitmap_bit_p (set, DF_REF_REGNO (use))
            && rtx_moveable_p(self,&PATTERN (insn), OP_IN)){
               unsigned regno = DF_REF_REGNO (use);
               bitmap_set_bit (moveable, regno);
               bitmap_set_bit (set, regno);
               bitmap_set_bit (used, regno);
               bitmap_clear_bit (transp, regno);
               continue;
            }
            FOR_EACH_INSN_INFO_USE (use, insn_info){
               unsigned regno = DF_REF_REGNO (use);
               bitmap_set_bit (used, regno);
               if (bitmap_clear_bit (moveable, regno))
                  bitmap_clear_bit (transp, regno);
            }

            FOR_EACH_INSN_INFO_DEF (def, insn_info){
               unsigned regno = DF_REF_REGNO (def);
               bitmap_set_bit (set, regno);
               bitmap_clear_bit (transp, regno);
               bitmap_clear_bit (moveable, regno);
            }
         }
   }//end    FOR_EACH_BB_FN (bb, cfun)

   FOR_EACH_BB_FN (bb, cfun){
      bitmap local = bb_local + bb->index;
      rtx_insn *insn;

      FOR_BB_INSNS (bb, insn)
         if (NONDEBUG_INSN_P (insn)){
            df_insn_info *insn_info = DF_INSN_INFO_GET (insn);
            rtx_insn *def_insn;
            rtx closest_use, note;
            df_ref def, use;
            unsigned regno;
            bool all_dominated, all_local;
            machine_mode mode;

            def = df_single_def (insn_info);
            /* There must be exactly one def in this insn.  */
            if (!def || !single_set (insn))
               continue;
            /* This must be the only definition of the reg.  We also limit
            which modes we deal with so that we can assume we can generate
            move instructions.  */
            regno = DF_REF_REGNO (def);
            mode = GET_MODE (DF_REF_REG (def));
            if (DF_REG_DEF_COUNT (regno) != 1
            || !DF_REF_INSN_INFO (def)
            || mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,regno)
            || DF_REG_EQ_USE_COUNT (regno) > 0
            || (!mtcs_mode_is_integral_p/*!INTEGRAL_MODE_P*/(mtcsMode,mode)
            && !mtcs_mode_is_float_p/*!FLOAT_MODE_P*/(mtcsMode,mode)
            && !mtcs_mode_opaque_mode_p/*!OPAQUE_MODE_P*/(mtcsMode,mode)))
               continue;
            def_insn = DF_REF_INSN (def);

            for (note = REG_NOTES (def_insn); note; note = XEXP (note, 1))
               if (REG_NOTE_KIND (note) == REG_EQUIV && MEM_P (XEXP (note, 0)))
                  break;

            if (note){
               if (dump_file)
                  fprintf (dump_file, "Ignoring reg %d, has equiv memory\n",regno);
               bitmap_set_bit (unusable_as_input, regno);
               continue;
            }

            use = DF_REG_USE_CHAIN (regno);
            all_dominated = true;
            all_local = true;
            closest_use = NULL_RTX;
            for (; use; use = DF_REF_NEXT_REG (use)){
               rtx_insn *insn;
               if (!DF_REF_INSN_INFO (use)){
                  all_dominated = false;
                  all_local = false;
                  break;
               }
               insn = DF_REF_INSN (use);
               if (DEBUG_INSN_P (insn))
                  continue;
               if (BLOCK_FOR_INSN (insn) != BLOCK_FOR_INSN (def_insn))
                  all_local = false;
               if (!insn_dominated_by_p (insn, def_insn, uid_luid))
                  all_dominated = false;
               if (closest_use != insn && closest_use != const0_rtx){
                  if (closest_use == NULL_RTX)
                     closest_use = insn;
                  else if (insn_dominated_by_p (closest_use, insn, uid_luid))
                     closest_use = insn;
                  else if (!insn_dominated_by_p (insn, closest_use, uid_luid))
                     closest_use = const0_rtx;
               }
            }
            if (!all_dominated){
               if (dump_file)
                  fprintf (dump_file, "Reg %d not all uses dominated by set\n",regno);
               continue;
            }
            if (all_local)
               bitmap_set_bit (local, regno);
            if (closest_use == const0_rtx || closest_use == NULL
            || next_nonnote_nondebug_insn (def_insn) == closest_use){
               if (dump_file)
                  fprintf (dump_file, "Reg %d uninteresting%s\n", regno,
                        closest_use == const0_rtx || closest_use == NULL ? " (no unique first use)" : "");
               continue;
            }

            bitmap_set_bit (interesting, regno);
            /* If we get here, we know closest_use is a non-NULL insn
            (as opposed to const_0_rtx).  */
            closest_uses[regno] = as_a <rtx_insn *> (closest_use);

            if (dump_file && (all_local || all_dominated)){
               fprintf (dump_file, "Reg %u:", regno);
               if (all_local)
                  fprintf (dump_file, " local to bb %d", bb->index);
               if (all_dominated)
                  fprintf (dump_file, " def dominates all uses");
               if (closest_use != const0_rtx)
                  fprintf (dump_file, " has unique first use");
               fputs ("\n", dump_file);
            }
         }
   }//end    FOR_EACH_BB_FN (bb, cfun)

   EXECUTE_IF_SET_IN_BITMAP (interesting, 0, i, bi){
      df_ref def = DF_REG_DEF_CHAIN (i);
      rtx_insn *def_insn = DF_REF_INSN (def);
      basic_block def_block = BLOCK_FOR_INSN (def_insn);
      bitmap def_bb_local = bb_local + def_block->index;
      bitmap def_bb_moveable = bb_moveable_reg_sets + def_block->index;
      bitmap def_bb_transp = bb_transp_live + def_block->index;
      bool local_to_bb_p = bitmap_bit_p (def_bb_local, i);
      rtx_insn *use_insn = closest_uses[i];
      df_ref use;
      bool all_ok = true;
      bool all_transp = true;

      if (!REG_P (DF_REF_REG (def)))
         continue;

      if (!local_to_bb_p){
         if (dump_file)
            fprintf (dump_file, "Reg %u not local to one basic block\n",i);
         continue;
      }
      if (reg_equiv_init (i) != NULL_RTX){
         if (dump_file)
            fprintf (dump_file, "Ignoring reg %u with equiv init insn\n",i);
         continue;
      }
      if (!rtx_moveable_p(self,&PATTERN (def_insn), OP_IN)){
         if (dump_file)
            fprintf (dump_file, "Found def insn %d for %d to be not moveable\n",INSN_UID (def_insn), i);
         continue;
      }
      if (dump_file)
         fprintf (dump_file, "Examining insn %d, def for %d\n",INSN_UID (def_insn), i);
      FOR_EACH_INSN_USE (use, def_insn){
         unsigned regno = DF_REF_REGNO (use);
         if (bitmap_bit_p (unusable_as_input, regno)){
            all_ok = false;
            if (dump_file)
               fprintf (dump_file, "  found unusable input reg %u.\n", regno);
            break;
         }
         if (!bitmap_bit_p (def_bb_transp, regno)){
            if (bitmap_bit_p (def_bb_moveable, regno)
            && !mtcs_cfg_build_control_flow_insn_p/*!control_flow_insn_p*/(mtcsCfgBuild,use_insn)){
               if (mtcs_rtlanal_modified_between_p/*!modified_between_p*/(mtcsRtlanal,DF_REF_REG (use), def_insn, use_insn)){
                  rtx_insn *x = NEXT_INSN (def_insn);
                  while (!modified_in_p (DF_REF_REG (use), x)){
                     gcc_assert (x != use_insn);
                     x = NEXT_INSN (x);
                  }
                  if (dump_file)
                     fprintf (dump_file, "  input reg %u modified but insn %d moveable\n",regno, INSN_UID (x));
                  mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,PATTERN (x), use_insn);
                  mtcs_rtl_set_insn_deleted/*!set_insn_deleted*/(mtcsRTL,x);
               }else{
                  if (dump_file)
                     fprintf (dump_file, "  input reg %u modified between def and use\n",regno);
                  all_transp = false;
               }
            }else
               all_transp = false;
         }
      }//end    FOR_EACH_INSN_USE (use, def_insn)
      if (!all_ok)
         continue;
      if (!dbg_cnt (ira_move))
         break;
      if (dump_file)
         fprintf (dump_file, "  all ok%s\n", all_transp ? " and transp" : "");

      if (all_transp){
         rtx def_reg = DF_REF_REG (def);
         rtx newreg = ira_create_new_reg (def_reg);//ira-emit.cc实现
         if (mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,def_insn, DF_REF_REAL_LOC (def), newreg, 0)){
            unsigned nregno = REGNO (newreg);
            mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,
                  mtcs_expr_gen_move_insn/*!gen_move_insn*/(mtcsExpr,def_reg, newreg), use_insn);
            nregno -= max_regs;
            self->pseudo_replaced_reg[nregno] = def_reg;
         }
      }
   }//end    EXECUTE_IF_SET_IN_BITMAP (interesting, 0, i, bi)

   FOR_EACH_BB_FN (bb, cfun){
      bitmap_clear (bb_local + bb->index);
      bitmap_clear (bb_transp_live + bb->index);
      bitmap_clear (bb_moveable_reg_sets + bb->index);
   }
   free (uid_luid);
   free (closest_uses);
   free (bb_local);
   free (bb_transp_live);
   free (bb_moveable_reg_sets);

   self->last_moveable_pseudo = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);

   fix_reg_equiv_init(self);
   expand_reg_info(self);
   regstat_free_n_sets_and_refs ();
   regstat_free_ri ();
   mtcs_reg_regstat_init_n_sets_and_refs/*!regstat_init_n_sets_and_refs*/(mtcsReg);
   regstat_compute_ri ();
   free_dominance_info (CDI_DOMINATORS);
}

/* If SET pattern SET is an assignment from a hard register to a pseudo which
   is live at CALL_DOM (if non-NULL, otherwise this check is omitted), return
   the destination.  Otherwise return NULL.  */

static rtx interesting_dest_for_shprep_1 (MtcsIra *self,rtx set, basic_block call_dom)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   rtx src = SET_SRC (set);
   rtx dest = SET_DEST (set);
   if (!REG_P (src) || !mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,src)
   || !REG_P (dest) || mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,dest)
   || (call_dom && !bitmap_bit_p (df_get_live_in (call_dom), REGNO (dest))))
      return NULL;
   return dest;
}

/* If insn is interesting for parameter range-splitting shrink-wrapping
   preparation, i.e. it is a single set from a hard register to a pseudo, which
   is live at CALL_DOM (if non-NULL, otherwise this check is omitted), or a
   parallel statement with only one such statement, return the destination.
   Otherwise return NULL.  */
static rtx interesting_dest_for_shprep (MtcsIra *self,rtx_insn *insn, basic_block call_dom)
{
   if (!INSN_P (insn))
   return NULL;
   rtx pat = PATTERN (insn);
   if (GET_CODE (pat) == SET)
      return interesting_dest_for_shprep_1(self,pat, call_dom);

   if (GET_CODE (pat) != PARALLEL)
      return NULL;
   rtx ret = NULL;
   for (int i = 0; i < XVECLEN (pat, 0); i++){
      rtx sub = XVECEXP (pat, 0, i);
      if (GET_CODE (sub) == USE || GET_CODE (sub) == CLOBBER)
         continue;
      if (GET_CODE (sub) != SET || side_effects_p (sub))
         return NULL;
      rtx dest = interesting_dest_for_shprep_1(self,sub, call_dom);
      if (dest && ret)
         return NULL;
      if (dest)
         ret = dest;
   }
   return ret;
}

/* Split live ranges of pseudos that are loaded from hard registers in the
   first BB in a BB that dominates all non-sibling call if such a BB can be
   found and is not in a loop.  Return true if the function has made any
   changes.  */
static bool split_live_ranges_for_shrink_wrap (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsLoopinit *mtcsLoopinit =mtcs_target_get_loopinit(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   basic_block bb, call_dom = NULL;
   basic_block first = single_succ (ENTRY_BLOCK_PTR_FOR_FN (cfun));
   rtx_insn *insn, *last_interesting_insn = NULL;
   auto_bitmap need_new, reachable;
   vec<basic_block> queue;
   bool shrink= (mtcsOptionsItem->x_flag_shrink_wrap
         && target_rtx_have_simple_return/*!targetm.have_simple_return*/(mtcsMachine->tmrtx));
   if (!shrink/*!SHRINK_WRAPPING_ENABLED*/)
      return false;

   queue.create (n_basic_blocks_for_fn (cfun));

   FOR_EACH_BB_FN (bb, cfun)
      FOR_BB_INSNS (bb, insn)
         if (CALL_P (insn) && !SIBLING_CALL_P (insn)){
            if (bb == first){
               queue.release ();
               return false;
            }

            bitmap_set_bit (need_new, bb->index);
            bitmap_set_bit (reachable, bb->index);
            queue.quick_push (bb);
            break;
         }

   if (queue.is_empty ()){
      queue.release ();
      return false;
   }

   while (!queue.is_empty ()){
      edge e;
      edge_iterator ei;

      bb = queue.pop ();
      FOR_EACH_EDGE (e, ei, bb->succs)
         if (e->dest != EXIT_BLOCK_PTR_FOR_FN (cfun) && bitmap_set_bit (reachable, e->dest->index))
            queue.quick_push (e->dest);
   }
   queue.release ();

   FOR_BB_INSNS (first, insn){
      rtx dest = interesting_dest_for_shprep(self,insn, NULL);
      if (!dest)
         continue;

      if (DF_REG_DEF_COUNT (REGNO (dest)) > 1)
         return false;

      for (df_ref use = DF_REG_USE_CHAIN (REGNO(dest)); use; use = DF_REF_NEXT_REG (use)){
         int ubbi = DF_REF_BB (use)->index;
         if (bitmap_bit_p (reachable, ubbi))
            bitmap_set_bit (need_new, ubbi);
      }
      last_interesting_insn = insn;
   }

   if (!last_interesting_insn)
      return false;

   call_dom = nearest_common_dominator_for_set (CDI_DOMINATORS, need_new);
   if (call_dom == first)
      return false;

   mtcs_loopinit_loop_optimizer_init/*!loop_optimizer_init*/(mtcsLoopinit,AVOID_CFG_MODIFICATIONS);
   while (bb_loop_depth (call_dom) > 0)
      call_dom = get_immediate_dominator (CDI_DOMINATORS, call_dom);
   loop_optimizer_finalize ();

   if (call_dom == first)
      return false;

   calculate_dominance_info (CDI_POST_DOMINATORS);
   if (dominated_by_p (CDI_POST_DOMINATORS, first, call_dom)){
      free_dominance_info (CDI_POST_DOMINATORS);
      return false;
   }
   free_dominance_info (CDI_POST_DOMINATORS);

   if (dump_file)
      fprintf (dump_file, "Will split live ranges of parameters at BB %i\n",call_dom->index);

   bool ret = false;
   FOR_BB_INSNS (first, insn){
      rtx dest = interesting_dest_for_shprep(self,insn, call_dom);
      if (!dest || dest == mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(mtcsRTL))
         continue;

      bool need_newreg = false;
      df_ref use, next;
      for (use = DF_REG_USE_CHAIN (REGNO (dest)); use; use = next){
         rtx_insn *uin = DF_REF_INSN (use);
         next = DF_REF_NEXT_REG (use);

         if (DEBUG_INSN_P (uin))
            continue;

         basic_block ubb = BLOCK_FOR_INSN (uin);
         if (ubb == call_dom || dominated_by_p (CDI_DOMINATORS, ubb, call_dom)){
            need_newreg = true;
            break;
         }
      }

      if (need_newreg){
         rtx newreg = ira_create_new_reg (dest);//ira-emit.cc实现

         for (use = DF_REG_USE_CHAIN (REGNO (dest)); use; use = next){
            rtx_insn *uin = DF_REF_INSN (use);
            next = DF_REF_NEXT_REG (use);

            basic_block ubb = BLOCK_FOR_INSN (uin);
            if (ubb == call_dom || dominated_by_p (CDI_DOMINATORS, ubb, call_dom))
               mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,uin, DF_REF_REAL_LOC (use), newreg, true);
         }

         rtx_insn *new_move = mtcs_expr_gen_move_insn/*!gen_move_insn*/(mtcsExpr,newreg, dest);
         mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,new_move, bb_note (call_dom));
         if (dump_file){
            fprintf (dump_file, "Split live-range of register ");
            print_rtl_single (dump_file, dest);
         }
         ret = true;
      }

      if (insn == last_interesting_insn)
         break;
   }
   mtcs_recog_apply_change_group/*!apply_change_group*/(mtcsRecog);
   return ret;
}

/* Perform the second half of the transformation started in
   find_moveable_pseudos.  We look for instances where the newly introduced
   pseudo remains unallocated, and remove it by moving the definition to
   just before its use, replacing the move instruction generated by
   find_moveable_pseudos.  */
static void move_unallocated_pseudos (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   int i;
   for (i = self->first_moveable_pseudo; i < self->last_moveable_pseudo; i++)
      if (reg_renumber[i] < 0){
         int idx = i - self->first_moveable_pseudo;
         rtx other_reg = self->pseudo_replaced_reg[idx];
         /* The iterating range [self->first_moveable_pseudo, last_moveable_pseudo)
         covers every new pseudo created in find_moveable_pseudos,
         regardless of the validation with it is successful or not.
         So we need to skip the pseudos which were used in those failed
         validations to avoid unexpected DF info and consequent ICE.
         We only set pseudo_replaced_reg[] when the validation is successful
         in find_moveable_pseudos, it's enough to check it here.  */
         if (!other_reg)
            continue;
         rtx_insn *def_insn = DF_REF_INSN (DF_REG_DEF_CHAIN (i));
         /* The use must follow all definitions of OTHER_REG, so we can
         insert the new definition immediately after any of them.  */
         df_ref other_def = DF_REG_DEF_CHAIN (REGNO (other_reg));
         rtx_insn *move_insn = DF_REF_INSN (other_def);
         rtx_insn *newinsn = mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,PATTERN (def_insn), move_insn);
         rtx set;
         int success;

         if (dump_file)
            fprintf (dump_file, "moving def of %d (insn %d now) ",REGNO (other_reg), INSN_UID (def_insn));

         mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,move_insn);
         while ((other_def = DF_REG_DEF_CHAIN (REGNO (other_reg))))
            mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,DF_REF_INSN (other_def));
         mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,def_insn);

         set = single_set (newinsn);
         success = mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,newinsn, &SET_DEST (set), other_reg, 0);
         gcc_assert (success);
         if (dump_file)
            fprintf (dump_file, " %d) rather than keep unallocated replacement %d\n",INSN_UID (newinsn), i);
         SET_REG_N_REFS (i, 0);
      }

   self->first_moveable_pseudo = self->last_moveable_pseudo = 0;
}



/* Code dealing with scratches (changing them onto
   pseudos and restoring them from the pseudos).

   We change scratches into pseudos at the beginning of IRA to
   simplify dealing with them (conflicts, hard register assignments).

   If the pseudo denoting scratch was spilled it means that we do not
   need a hard register for it.  Such pseudos are transformed back to
   scratches at the end of LRA.  */

/* Description of location of a former scratch operand.   */
//在mtcsira.h中声明
struct mtcs_ira_sloc/*!sloc*/
{
  rtx_insn *insn; /* Insn where the scratch was.  */
  int nop;  /* Number of the operand which was a scratch.  */
  unsigned regno; /* regno gnerated instead of scratch */
  int icode;  /* Original icode from which scratch was removed.  */
};


/* Return true if pseudo REGNO is made of SCRATCH.  */
//原型 ira_former_scratch_p ira.h ira.cc
bool mtcs_ira_former_scratch_p (MtcsIra *self,int regno)
{
  return bitmap_bit_p (&self->scratch_bitmap, regno);
}

/* Return true if the operand NOP of INSN is a former scratch. */
//原型 ira_former_scratch_operand_p ira.h ira.cc
bool mtcs_ira_former_scratch_operand_p (MtcsIra *self,rtx_insn *insn, int nop)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   return bitmap_bit_p (&self->scratch_operand_bitmap,  INSN_UID (insn) *
         mtcs_recog_get_max_recog_operands/*!MAX_RECOG_OPERANDS*/(mtcsRecog) + nop) != 0;
}

/* Register operand NOP in INSN as a former scratch.  It will be
   changed to scratch back, if it is necessary, at the LRA end.  */
//原型 ira_register_new_scratch_op ira.h ira.cc
void mtcs_ira_register_new_scratch_op (MtcsIra *self,rtx_insn *insn, int nop, int icode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   rtx op = *mtcsRecog->recog_data.operand_loc[nop];
   struct mtcs_ira_sloc *loc = XNEW (struct mtcs_ira_sloc);
   ira_assert (REG_P (op));
   loc->insn = insn;
   loc->nop = nop;
   loc->regno = REGNO (op);
   loc->icode = icode;
   self->scratches.safe_push (loc);
   bitmap_set_bit (&self->scratch_bitmap, REGNO (op));
   bitmap_set_bit (&self->scratch_operand_bitmap,
         INSN_UID (insn) * mtcs_recog_get_max_recog_operands/*!MAX_RECOG_OPERANDS*/(mtcsRecog) + nop);
   add_reg_note (insn, REG_UNUSED, op);
}

/* Return true if string STR contains constraint 'X'.  */
static bool contains_X_constraint_p (MtcsIra *self,const char *str)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);

   int c;
   while ((c = *str)){
      str += mtcs_preds_insn_constraint_len/*!CONSTRAINT_LEN*/(mtcsPreds,c, str);
      if (c == 'X') return true;
   }
   return false;
}

/* Change INSN's scratches into pseudos and save their location.
   Return true if we changed any scratch.  */
//原型 ira_remove_insn_scratches ira.h ira.cc
bool mtcs_ira_remove_insn_scratches (MtcsIra *self,rtx_insn *insn, bool all_p, FILE *dump_file,
            rtx (*get_reg) (rtx original,void *userData),void *userData)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int i;
   bool insn_changed_p;
   rtx reg, *loc;

   mtcs_recog_extract_insn/*!extract_insn*/(mtcsRecog,insn);
   insn_changed_p = false;
   for (i = 0; i < mtcsRecog->recog_data.n_operands; i++){
      loc = mtcsRecog->recog_data.operand_loc[i];
      if (GET_CODE (*loc) == SCRATCH && GET_MODE (*loc) != VOIDmode){
         if (! all_p && contains_X_constraint_p(self,mtcsRecog->recog_data.constraints[i]))
            continue;
         insn_changed_p = true;
         *loc = reg = get_reg (*loc,userData);
         mtcs_ira_register_new_scratch_op/*!ira_register_new_scratch_op*/(self,insn, i, INSN_CODE (insn));
         if (mtcsIraGlobal->ira_dump_file != NULL)
            fprintf (dump_file, "Removing SCRATCH to p%u in insn #%u (nop %d)\n",REGNO (reg), INSN_UID (insn), i);
      }
   }
   return insn_changed_p;
}

/* Return new register of the same mode as ORIGINAL.  Used in
   remove_scratches.  */
static rtx getScratchReg_cb (rtx original,void *userData)
{
  MtcsIra *self=(MtcsIra *)userData;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  return mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (original));
}

/* Change scratches into pseudos and save their location.  Return true
   if we changed any scratch.  */
static bool remove_scratches (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   bool change_p = false;
   basic_block bb;
   rtx_insn *insn;

   self->scratches.create (mtcs_rtl_data_get_max_uid/*!get_max_uid*/(mtcsRtlData));
   bitmap_initialize (&self->scratch_bitmap, &reg_obstack);
   bitmap_initialize (&self->scratch_operand_bitmap, &reg_obstack);
   FOR_EACH_BB_FN (bb, cfun)
      FOR_BB_INSNS (bb, insn)
         if (INSN_P (insn) && mtcs_ira_remove_insn_scratches/*!ira_remove_insn_scratches*/(self,
         insn, false, ira_dump_file, getScratchReg_cb,(void *)self)){
            /* Because we might use DF, we need to keep DF info up to date.  */
            mtcs_dfscan_df_insn_rescan/*!df_insn_rescan*/(mtcsDfscan,insn);
            change_p = true;
         }
   return change_p;
}

/* Changes pseudos created by function remove_scratches onto scratches.  */
//原型 ira_restore_scratches ira.h ira.cc
void mtcs_ira_restore_scratches (MtcsIra *self,FILE *dump_file)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   int regno, n;
   unsigned i;
   rtx *op_loc;
   struct mtcs_ira_sloc *loc;

   for (i = 0; self->scratches.iterate (i, &loc); i++){
      /* Ignore already deleted insns.  */
      if (NOTE_P (loc->insn)  && NOTE_KIND (loc->insn) == NOTE_INSN_DELETED)
         continue;
      mtcs_recog_extract_insn/*!extract_insn*/(mtcsRecog,loc->insn);
      if (loc->icode != INSN_CODE (loc->insn)){
         /* The icode doesn't match, which means the insn has been modified
         (e.g. register elimination).  The scratch cannot be restored.  */
         continue;
      }
      op_loc = mtcsRecog->recog_data.operand_loc[loc->nop];
      if (REG_P (*op_loc)
      && ((regno = REGNO (*op_loc)) >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg))
      && reg_renumber[regno] < 0){
         /* It should be only case when scratch register with chosen
         constraint 'X' did not get memory or hard register.  */
         ira_assert (mtcs_ira_former_scratch_p/*!ira_former_scratch_p*/(self,regno));
         *op_loc = gen_rtx_SCRATCH (GET_MODE (*op_loc));
         for (n = 0; n < mtcsRecog->recog_data.n_dups; n++)
            *mtcsRecog->recog_data.dup_loc[n] = *mtcsRecog->recog_data.operand_loc[(int) mtcsRecog->recog_data.dup_num[n]];
         if (dump_file != NULL)
            fprintf (dump_file, "Restoring SCRATCH in insn #%u(nop %d)\n",INSN_UID (loc->insn), loc->nop);
      }
   }
   for (i = 0; self->scratches.iterate (i, &loc); i++)
      free (loc);
   self->scratches.release ();
   bitmap_clear (&self->scratch_bitmap);
   bitmap_clear (&self->scratch_operand_bitmap);
}



/* If the backend knows where to allocate pseudos for hard
   register initial values, register these allocations now.  */
static void allocate_initial_values (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);

   if (mtcsTarget/*!targetm.allocate_initial_value*/->allocate_initial_value){
      rtx hreg, preg, x;
      int i, regno;

      for (i = 0; mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,i); i++){
         if (! mtcs_func_initial_value_entry/*!initial_value_entry*/(mtcsFunc,i, &hreg, &preg))
            break;

         x = mtcsTarget/*!targetm.allocate_initial_value*/->allocate_initial_value(mtcsTarget,hreg);
         regno = REGNO (preg);
         if (x && REG_N_SETS (regno) <= 1){
            if (MEM_P (x))
               reg_equiv_memory_loc (regno) = x;
            else{
               basic_block bb;
               int new_regno;

               gcc_assert (REG_P (x));
               new_regno = REGNO (x);
               reg_renumber[regno] = new_regno;
               /* Poke the regno right into regno_reg_rtx so that even
               fixed regs are accepted.  */
               mtcs_dfscan_df_ref_change_reg_with_loc/*!SET_REGNO*/(mtcsDfscan,preg, new_regno);
               /* Update global register liveness information.  */
               FOR_EACH_BB_FN (bb, cfun){
                  if (REGNO_REG_SET_P (df_get_live_in (bb), regno))
                     SET_REGNO_REG_SET (df_get_live_in (bb), new_regno);
                  if (REGNO_REG_SET_P (df_get_live_out (bb), regno))
                     SET_REGNO_REG_SET (df_get_live_out (bb), new_regno);
               }
            }
         }
      }

      gcc_checking_assert (! mtcs_func_initial_value_entry/*!initial_value_entry*/(mtcsFunc,
            mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg),&hreg, &preg));
   }
}

/* This is the main entry of IRA.  */
static void ira (MtcsIra *self,FILE *f)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsLoopinit *mtcsLoopinit =mtcs_target_get_loopinit(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsCse *mtcsCse=mtcs_target_get_cse(mtcsTarget);
   MtcsReload1 *mtcsReload1=mtcs_target_get_reload1(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsDfproblems *mtcsDfproblems=mtcs_target_get_dfproblems(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraColor *mtcsIraColor=mtcs_ira_mgr_get_color(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   bool loops_p;
   int ira_max_point_before_emit;
   bool saved_flag_caller_saves = mtcsOptionsItem->x_flag_caller_saves;
   enum ira_region saved_flag_ira_region = mtcsOptionsItem->x_flag_ira_region;
   basic_block bb;
   edge_iterator ei;
   edge e;
   bool output_jump_reload_p = false;

   if (self->ira_use_lra_p){
      /* First put potential jump output reloads on the output edges
      as USE which will be removed at the end of LRA.  The major
      goal is actually to create BBs for critical edges for LRA and
      populate them later by live info.  In LRA it will be
      difficult to do this. */
      FOR_EACH_BB_FN (bb, cfun){
         rtx_insn *end = BB_END (bb);
         if (!JUMP_P (end))
            continue;
         mtcs_recog_extract_insn/*!extract_insn*/(mtcsRecog,end);
         for (int i = 0; i < mtcsRecog->recog_data.n_operands; i++)
            if (mtcsRecog->recog_data.operand_type[i] != OP_IN){
               bool skip_p = false;
               FOR_EACH_EDGE (e, ei, bb->succs)
                  if (EDGE_CRITICAL_P (e) && e->dest != EXIT_BLOCK_PTR_FOR_FN (cfun) && (e->flags & EDGE_ABNORMAL)){
                     skip_p = true;
                     break;
                  }
               if (skip_p)
                  break;
               output_jump_reload_p = true;
               FOR_EACH_EDGE (e, ei, bb->succs)
                  if (EDGE_CRITICAL_P (e)  && e->dest != EXIT_BLOCK_PTR_FOR_FN (cfun)){
                     mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
                     /* We need to put some no-op insn here.  We can
                     not put a note as commit_edges insertion will
                     fail.  */
                     mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_rtx_USE (VOIDmode, const1_rtx));
                     rtx_insn *insns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
                     mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
                     mtcs_cfg_rtl_insert_insn_on_edge/*!insert_insn_on_edge*/(mtcsCfgRtl,insns, e);
                  }
               break;
            }
      }
      if (output_jump_reload_p)
         mtcs_cfg_rtl_commit_edge_insertions/*!commit_edge_insertions*/(mtcsCfgRtl);
   }//end    if (self->ira_use_lra_p)

   if (mtcsOptionsItem->x_flag_ira_verbose < 10){
      mtcsIraGlobal->internal_flag_ira_verbose = mtcsOptionsItem->x_flag_ira_verbose;
      mtcsIraGlobal->ira_dump_file = f;
   }else{
      mtcsIraGlobal->internal_flag_ira_verbose = mtcsOptionsItem->x_flag_ira_verbose - 10;
      mtcsIraGlobal->ira_dump_file = stderr;
   }

   clear_bb_flags ();

   /* Determine if the current function is a leaf before running IRA
   since this can impact optimizations done by the prologue and
   epilogue thus changing register elimination offsets.
   Other target callbacks may use crtl->is_leaf too, including
   SHRINK_WRAPPING_ENABLED, so initialize as early as possible.  */
   mtcsRtlData/*!crtl*/->is_leaf = mtcs_output_leaf_function_p/*!leaf_function_p*/(mtcsOutput);

   /* Perform target specific PIC register initialization.  */
   mtcsTarget/*!targetm.init_pic_reg*/->init_pic_reg(mtcsTarget);

   self->ira_conflicts_p = mtcsOptionsItem->x_optimize > 0;

   /* Determine the number of pseudos actually requiring coloring.  */
   unsigned int num_used_regs = 0;
   for (unsigned int i = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i < DF_REG_SIZE (df); i++)
      if (DF_REG_DEF_COUNT (i) || DF_REG_USE_COUNT (i))
         num_used_regs++;

   /* If there are too many pseudos and/or basic blocks (e.g. 10K pseudos and
   10K blocks or 100K pseudos and 1K blocks) or we have too many function
   insns, we will use simplified and faster algorithms in LRA.  */
   lra_simple_p = (self->ira_use_lra_p && (num_used_regs >= (1U << 26) / last_basic_block_for_fn (cfun)
            /* max uid is a good evaluation of the number of insns as most
            optimizations are done on tree-SSA level.  */
            || ((uint64_t) mtcs_rtl_data_get_max_uid/*!get_max_uid*/(mtcsRtlData)
            > (uint64_t) mtcsOptionsItem->x_param_ira_simple_lra_insn_threshold * 1000)));

   if (lra_simple_p){
      /* It permits to skip live range splitting in LRA.  */
      mtcsOptionsItem->x_flag_caller_saves = false;
      /* There is no sense to do regional allocation when we use
      simplified LRA.  */
      mtcsOptionsItem->x_flag_ira_region = IRA_REGION_ONE;
      self->ira_conflicts_p = false;
   }

   //#ifndef IRA_NO_OBSTACK
   gcc_obstack_init (&mtcsIraGlobal->ira_obstack);
   //#endif
   bitmap_obstack_initialize (&self->ira_bitmap_obstack);

   /* LRA uses its own infrastructure to handle caller save registers.  */
   if (mtcsOptionsItem->x_flag_caller_saves && !self->ira_use_lra_p)
      init_caller_save ();

   setup_prohibited_mode_move_regs(self);
   decrease_live_ranges_number(self);
   mtcs_dfproblems_df_note_add_problem/*!df_note_add_problem*/(mtcsDfproblems);

   /* DF_LIVE can't be used in the register allocator, too many other
   parts of the compiler depend on using the "classic" liveness
   interpretation of the DF_LR problem.  See PR38711.
   Remove the problem, so that we don't spend time updating it in
   any of the df_analyze() calls during IRA/LRA.  */
   if (mtcsOptionsItem->x_optimize > 1)
      mtcs_dfcore_df_remove_problem/*!df_remove_problem*/(mtcsDfcore,df_live);
   gcc_checking_assert (df_live == NULL);

   if (mtcsOptionsItem->x_flag_checking)
      df->changeable_flags |= DF_VERIFY_SCHEDULED;

   mtcs_dfcore_df_analyze/*!df_analyze*/(mtcsDfcore);

   init_reg_equiv(self);
   if (self->ira_conflicts_p){
      calculate_dominance_info (CDI_DOMINATORS);

      if (split_live_ranges_for_shrink_wrap(self))
         mtcs_dfcore_df_analyze/*!df_analyze*/(mtcsDfcore);

      free_dominance_info (CDI_DOMINATORS);
   }

   mtcs_dfcore_df_clear_flags/*!df_clear_flags*/(mtcsDfcore,DF_NO_INSN_RESCAN);

   indirect_jump_optimize(self);
   if (mtcs_cse_delete_trivially_dead_insns/*!delete_trivially_dead_insns*/(mtcsCse,
   mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData), mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc)))
      mtcs_dfcore_df_analyze/*!df_analyze*/(mtcsDfcore);

   mtcs_reg_regstat_init_n_sets_and_refs/*!regstat_init_n_sets_and_refs*/(mtcsReg);
   regstat_compute_ri ();

   /* If we are not optimizing, then this is the only place before
   register allocation where dataflow is done.  And that is needed
   to generate these warnings.  */
   if (mtcsOptionsItem->x_warn_clobbered)
      generate_setjmp_warnings ();

   /* update_equiv_regs can use reg classes of pseudos and they are set up in
   register pressure sensitive scheduling and loop invariant motion and in
   live range shrinking.  This info can become obsolete if we add new pseudos
   since the last set up.  Recalculate it again if the new pseudos were
   added.  */
   if (mtcs_reg_resize_reg_info/*!resize_reg_info*/(mtcsReg)
   && (mtcsOptionsItem->x_flag_sched_pressure || mtcsOptionsItem->x_flag_live_range_shrinkage
   || mtcsOptionsItem->x_flag_ira_loop_pressure))
      ira_set_pseudo_classes (true, ira_dump_file);//ira-costs.cc实现

   mtcs_alias_init_alias_analysis/*!init_alias_analysis*/(mtcsAlias);
   mtcs_loopinit_loop_optimizer_init/*!loop_optimizer_init*/(mtcsLoopinit,AVOID_CFG_MODIFICATIONS);
   self->reg_equiv = XCNEWVEC (struct equivalence, mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc));
   update_equiv_regs_prescan(self);
   update_equiv_regs(self);

   /* Don't move insns if live range shrinkage or register
   pressure-sensitive scheduling were done because it will not
   improve allocation but likely worsen insn scheduling.  */
   if (mtcsOptionsItem->x_optimize
   && !mtcsOptionsItem->x_flag_live_range_shrinkage
   && !(mtcsOptionsItem->x_flag_sched_pressure && mtcsOptionsItem->x_flag_schedule_insns))
      combine_and_move_insns(self);

   /* Gather additional equivalences with memory.  */
   if (mtcsOptionsItem->x_optimize)
      add_store_equivs(self);

   loop_optimizer_finalize ();
   free_dominance_info (CDI_DOMINATORS);
   mtcs_alias_end_alias_analysis/*!end_alias_analysis*/(mtcsAlias);
   free (self->reg_equiv);

   /* Once max_regno changes, we need to free and re-init/re-compute
   some data structures like regstat_n_sets_and_refs and reg_info_p.  */
   auto regstat_recompute_for_max_regno = []() {
      regstat_free_n_sets_and_refs ();
      regstat_free_ri ();
      //有问题 mtcs_reg_regstat_init_n_sets_and_refs/*!regstat_init_n_sets_and_refs*/(mtcsReg);
      regstat_init_n_sets_and_refs ();
      regstat_compute_ri ();
      //mtcs_reg_resize_reg_info/*!resize_reg_info*/(mtcsReg);
      resize_reg_info();
   };

   int max_regno_before_rm = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);
   if (self->ira_use_lra_p && remove_scratches(self)){
      mtcs_ira_expand_reg_equiv/*!ira_expand_reg_equiv*/(self);
      /* For now remove_scatches is supposed to create pseudos when it
      succeeds, assert this happens all the time.  Once it doesn't
      hold, we should guard the regstat recompute for the case
      max_regno changes.  */
      gcc_assert (max_regno_before_rm != mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc));
      regstat_recompute_for_max_regno ();
   }

   setup_reg_equiv(self);
   grow_reg_equivs ();
   setup_reg_equiv_init(self);

   self->allocated_reg_info_size = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);

   /* It is not worth to do such improvement when we use a simple
   allocation because of -O0 usage or because the function is too
   big.  */
   if (self->ira_conflicts_p)
      find_moveable_pseudos(self);

   self->max_regno_before_ira = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);
   mtcs_ira_setup_eliminable_regset/*!ira_setup_eliminable_regset*/(self);

   self->ira_overall_cost = self->ira_reg_cost = self->ira_mem_cost = 0;
   self->ira_load_cost = self->ira_store_cost = self->ira_shuffle_cost = 0;
   self->ira_move_loops_num = self->ira_additional_jumps_num = 0;

   ira_assert (current_loops == NULL);
   if (mtcsOptionsItem->x_flag_ira_region == IRA_REGION_ALL || mtcsOptionsItem->x_flag_ira_region == IRA_REGION_MIXED)
      mtcs_loopinit_loop_optimizer_init/*!loop_optimizer_init*/(mtcsLoopinit,AVOID_CFG_MODIFICATIONS | LOOPS_HAVE_RECORDED_EXITS);

   if (mtcsIraGlobal->internal_flag_ira_verbose > 0 && mtcsIraGlobal->ira_dump_file != NULL)
      fprintf (mtcsIraGlobal->ira_dump_file, "Building IRA IR\n");

   loops_p = ira_build ();

   ira_assert (self->ira_conflicts_p || !loops_p);

   self->saved_flag_ira_share_spill_slots = mtcsOptionsItem->x_flag_ira_share_spill_slots;
   if (too_high_register_pressure_p(self) || cfun->calls_setjmp)
      /* It is just wasting compiler's time to pack spilled pseudos into
      stack slots in this case -- prohibit it.  We also do this if
      there is setjmp call because a variable not modified between
      setjmp and longjmp the compiler is required to preserve its
      value and sharing slots does not guarantee it.  */
      mtcsOptionsItem->x_flag_ira_share_spill_slots = false;

   ira_color ();

   ira_max_point_before_emit = ira_max_point;

   ira_initiate_emit_data ();

   ira_emit (loops_p);

   max_regno = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);
   if (self->ira_conflicts_p){
      if (! loops_p){
         if (! self->ira_use_lra_p)
            mtcs_ira_color_initiate_assign/*!ira_initiate_assign*/(mtcsIraColor);
      }else{
         expand_reg_info(self);

         if (self->ira_use_lra_p){
            MtcsIraAllocno *a;
            MtcsIraAllocnoIterator ai;

            MTCS_FOR_EACH_ALLOCNO (mtcsIraBuild,a, ai){
               int old_regno = a->regno;
               int new_regno = REGNO (MTCS_ALLOCNO_EMIT_DATA (a)->reg);

               a->regno = new_regno;

               if (old_regno != new_regno)
                  mtcs_reg_setup_reg_classes/*!setup_reg_classes*/(mtcsReg,new_regno,
                        mtcs_reg_reg_preferred_class/*!reg_preferred_class*/(mtcsReg,old_regno),
                        mtcs_reg_reg_alternate_class/*!reg_alternate_class*/(mtcsReg,old_regno),
                        mtcs_reg_reg_allocno_class/*!reg_allocno_class*/(mtcsReg,old_regno));
            }
         }else{
            if (mtcsIraGlobal->internal_flag_ira_verbose > 0 && mtcsIraGlobal->ira_dump_file != NULL)
               fprintf (mtcsIraGlobal->ira_dump_file, "Flattening IR\n");
            ira_flattening (self->max_regno_before_ira, ira_max_point_before_emit);
         }
         /* New insns were generated: add notes and recalculate live
         info.  */
         mtcs_dfcore_df_analyze/*!df_analyze*/(mtcsDfcore);

         /* ??? Rebuild the loop tree, but why?  Does the loop tree
         change if new insns were generated?  Can that be handled
         by updating the loop tree incrementally?  */
         loop_optimizer_finalize ();
         free_dominance_info (CDI_DOMINATORS);
         mtcs_loopinit_loop_optimizer_init/*!loop_optimizer_init*/(mtcsLoopinit,AVOID_CFG_MODIFICATIONS | LOOPS_HAVE_RECORDED_EXITS);

         if (! self->ira_use_lra_p){
            setup_allocno_assignment_flags(self);
            mtcs_ira_color_initiate_assign/*!ira_initiate_assign*/(mtcsIraColor);
            mtcs_ira_color_reassign_conflict_allocnos/*!ira_reassign_conflict_allocnos*/(mtcsIraColor,max_regno);
         }
      }
   }

   ira_finish_emit_data ();

   setup_reg_renumber(self);

   calculate_allocation_cost(self);

#ifdef ENABLE_IRA_CHECKING
   if (self->ira_conflicts_p && ! self->ira_use_lra_p)
      /* Opposite to reload pass, LRA does not use any conflict info
      from IRA.  We don't rebuild conflict info for LRA (through
      ira_flattening call) and cannot use the check here.  We could
      rebuild this info for LRA in the check mode but there is a risk
      that code generated with the check and without it will be a bit
      different.  Calling ira_flattening in any mode would be a
      wasting CPU time.  So do not check the allocation for LRA.  */
      check_allocation(self);
#endif

   if (max_regno != self->max_regno_before_ira)
      regstat_recompute_for_max_regno ();

   self->overall_cost_before = self->ira_overall_cost;
   if (! self->ira_conflicts_p)
      mtcs_reload1_grow_reg_equivs/*!grow_reg_equivs*/(mtcsReload1);
   else{
      fix_reg_equiv_init(self);

#ifdef ENABLE_IRA_CHECKING
      print_redundant_copies(self);
#endif
      if (! self->ira_use_lra_p){
         ira_spilled_reg_stack_slots_num = 0;
         ira_spilled_reg_stack_slots = ((class ira_spilled_reg_stack_slot *)
               ira_allocate (max_regno * sizeof (class ira_spilled_reg_stack_slot)));
         memset ((void *)ira_spilled_reg_stack_slots, 0, max_regno * sizeof (class ira_spilled_reg_stack_slot));
      }
   }
   allocate_initial_values(self);

   /* See comment for find_moveable_pseudos call.  */
   if (self->ira_conflicts_p)
      move_unallocated_pseudos(self);

   /* Restore original values.  */
   if (lra_simple_p){
      mtcsOptionsItem->x_flag_caller_saves = saved_flag_caller_saves;
      mtcsOptionsItem->x_flag_ira_region = saved_flag_ira_region;
   }
}

/* Modify asm goto to avoid further trouble with this insn.  We can
   not replace the insn by USE as in other asm insns as we still
   need to keep CFG consistency.  */
//原型 ira_nullify_asm_goto ira.h ira.cc
void mtcs_ira_nullify_asm_goto (MtcsIra *self,rtx_insn *insn)
{
  ira_assert (JUMP_P (insn) && INSN_CODE (insn) < 0);
  rtx tmp = extract_asm_operands (PATTERN (insn));
  PATTERN (insn) = gen_rtx_ASM_OPERANDS (VOIDmode, ggc_strdup (""), "", 0,
                rtvec_alloc (0),
                rtvec_alloc (0),
                ASM_OPERANDS_LABEL_VEC (tmp),
                ASM_OPERANDS_SOURCE_LOCATION(tmp));
}

static void do_reload (MtcsIra *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsCse *mtcsCse=mtcs_target_get_cse(mtcsTarget);
   MtcsReload1 *mtcsReload1=mtcs_target_get_reload1(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsCfgCleanup *mtcsCfgCleanup=mtcs_target_get_cfg_cleanup(mtcsTarget);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsDfproblems *mtcsDfproblems=mtcs_target_get_dfproblems(mtcsTarget);
   MtcsDce *mtcsDce=mtcs_target_get_dce(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraColor *mtcsIraColor=mtcs_ira_mgr_get_color(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int hardFramePointerRegnum = mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg);

   basic_block bb;
   bool need_dce;
   unsigned pic_offset_table_regno = INVALID_REGNUM;

   if (mtcsOptionsItem->x_flag_ira_verbose < 10)
      mtcsIraGlobal->ira_dump_file = dump_file;

   rtx picOffsetTableRtx = mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(mtcsRTL);
   /* If pic_offset_table_rtx is a pseudo register, then keep it so
   after reload to avoid possible wrong usages of hard reg assigned
   to it.  */
   if (picOffsetTableRtx/*!pic_offset_table_rtx*/
   && REGNO (picOffsetTableRtx/*!pic_offset_table_rtx*/) >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg))
      pic_offset_table_regno = REGNO (picOffsetTableRtx/*!pic_offset_table_rtx*/);

   if (self->ira_use_lra_p){
      if (current_loops != NULL){
         loop_optimizer_finalize ();
         free_dominance_info (CDI_DOMINATORS);
      }
      FOR_ALL_BB_FN (bb, cfun)
         bb->loop_father = NULL;
      current_loops = NULL;

      ira_destroy ();

      lra (mtcsIraGlobal->ira_dump_file,  mtcsIraGlobal->internal_flag_ira_verbose);
      /* ???!!! Move it before lra () when we use ira_reg_equiv in
      LRA.  */
      vec_free (reg_equivs);
      reg_equivs = NULL;
      need_dce = false;
   }else{
      mtcs_dfcore_df_set_flags/*!df_set_flags*/(mtcsDfcore,DF_NO_INSN_RESCAN);
      build_insn_chain(self);
      need_dce = reload (mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData), self->ira_conflicts_p);
   }

   if (self->ira_conflicts_p && ! self->ira_use_lra_p){
      ira_free (mtcsIraColor->ira_spilled_reg_stack_slots);
      mtcs_ira_color_finish_assign/*!ira_finish_assign*/(mtcsIraColor);
   }

   if (mtcsIraGlobal->internal_flag_ira_verbose > 0 && mtcsIraGlobal->ira_dump_file != NULL
   && self->overall_cost_before != self->ira_overall_cost)
      fprintf (mtcsIraGlobal->ira_dump_file, "+++Overall after reload %" PRId64 "\n",self->ira_overall_cost);

   mtcsOptionsItem->x_flag_ira_share_spill_slots = self->saved_flag_ira_share_spill_slots;

   if (! self->ira_use_lra_p){
      ira_destroy ();
      if (current_loops != NULL){
         loop_optimizer_finalize ();
         free_dominance_info (CDI_DOMINATORS);
      }
      FOR_ALL_BB_FN (bb, cfun)
         bb->loop_father = NULL;
      current_loops = NULL;

      regstat_free_ri ();
      regstat_free_n_sets_and_refs ();
   }

   if (mtcsOptionsItem->x_optimize)
      mtcs_cfg_cleanup_cleanup_cfg/*!cleanup_cfg*/(mtcsCfgCleanup,CLEANUP_EXPENSIVE);

   finish_reg_equiv(self);

   bitmap_obstack_release (&self->ira_bitmap_obstack);
   //#ifndef IRA_NO_OBSTACK
   obstack_free (&mtcsIraGlobal->ira_obstack, NULL);
   //#endif

   /* The code after the reload has changed so much that at this point
   we might as well just rescan everything.  Note that
   df_rescan_all_insns is not going to help here because it does not
   touch the artificial uses and defs.  */
   mtcs_dfcore_df_finish_pass/*!df_finish_pass*/(mtcsDfcore,true);
   mtcs_dfscan_df_scan_alloc/*!df_scan_alloc*/(mtcsDfscan,NULL);
   mtcs_dfscan_df_scan_blocks/*!df_scan_blocks*/(mtcsDfscan);

   if (mtcsOptionsItem->x_optimize > 1){
      mtcs_dfproblems_df_live_add_problem/*!df_live_add_problem*/(mtcsDfproblems);
      mtcs_dfproblems_df_live_set_all_dirty/*!df_live_set_all_dirty*/(mtcsDfproblems);
   }

   if (mtcsOptionsItem->x_optimize)
      mtcs_dfcore_df_analyze/*!df_analyze*/(mtcsDfcore);

   if (need_dce && mtcsOptionsItem->x_optimize)
      mtcs_dce_run_fast_dce/*!run_fast_dce*/(mtcsDce);

   /* Diagnose uses of the hard frame pointer when it is used as a global
   register.  Often we can get away with letting the user appropriate
   the frame pointer, but we should let them know when code generation
   makes that impossible.  */
   if (mtcsReg->global_regs/*!global_regs*/[hardFramePointerRegnum/*!HARD_FRAME_POINTER_REGNUM*/]
                   && mtcsRtlData->x_frame_pointer_needed/*!frame_pointer_needed*/){
      tree decl =mtcsReg->global_regs_decl/*!global_regs_decl*/[hardFramePointerRegnum/*!HARD_FRAME_POINTER_REGNUM*/];
      error_at (DECL_SOURCE_LOCATION (current_function_decl),"frame pointer required, but reserved");
      inform (DECL_SOURCE_LOCATION (decl), "for %qD", decl);
   }

   /* If we are doing generic stack checking, give a warning if this
   function's frame size is larger than we expect.  */
   if (mtcsOptionsItem->x_flag_stack_check == GENERIC_STACK_CHECK){
      poly_int64 size = mtcs_func_get_frame_size/*!get_frame_size*/(mtcsFunc) + STACK_CHECK_FIXED_FRAME_SIZE;

      for (int i = 0; i < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i++)
         if (mtcs_dfscan_df_regs_ever_live_p/*!df_regs_ever_live_p*/(mtcsDfscan,i)
         && !mtcsReg->hardRegs.x_fixed_regs/*!fixed_regs*/[i]  && !mtcsRtlData/*!crtl*/->abi->clobbers_full_reg_p (i))
            size += UNITS_PER_WORD;

      if (constant_lower_bound (size) > mtcs_func_get_stack_check_max_frame_size/*!STACK_CHECK_MAX_FRAME_SIZE*/(mtcsFunc))
         warning (0, "frame size too large for reliable stack checking");
   }

   if (pic_offset_table_regno != INVALID_REGNUM)
      picOffsetTableRtx/*!pic_offset_table_rtx*/ = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,
            mtcs_mode_get_Pmode(mtcsMode), pic_offset_table_regno);
}

/* Spilling static chain pseudo may result in generation of wrong
   non-local goto code using frame-pointer to address saved stack
   pointer value after restoring old frame pointer value.  The
   function returns TRUE if REGNO is such a static chain pseudo.  */
//原型 non_spilled_static_chain_regno_p ira.h
bool mtcs_ira_non_spilled_static_chain_regno_p (MtcsIra *self,int regno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   return (cfun->static_chain_decl && mtcsRtlData/*!crtl*/->has_nonlocal_goto
         && REG_EXPR (mtcsRtlData->regno_reg_rtx/*! regno_reg_rtx*/[regno]) == cfun->static_chain_decl);
}

/* Return true if subloops that contain allocnos for A's register can
   use a different assignment from A.  ALLOCATED_P is true for the case
   in which allocation succeeded for A.  EXCLUDE_OLD_RELOAD is true if
   we should always return false for non-LRA targets.  (This is a hack
   and should be removed along with old reload.)  */
//原型 ira_subloop_allocnos_can_differ_p ira-int.h
bool mtcs_ira_subloop_allocnos_can_differ_p (MtcsIra *self,MtcsIraAllocno *a, bool allocated_p = true,
               bool exclude_old_reload = true)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   if (exclude_old_reload && !self->ira_use_lra_p)
      return false;

   auto regno = a->regno;

   if (mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(mtcsRTL) != NULL
   && regno == (int) REGNO (mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(mtcsRTL)))
      return false;

   ira_assert (regno < self->ira_reg_equiv_len);
   if (mtcs_ira_equiv_no_lvalue_p/*!ira_equiv_no_lvalue_p*/(self,regno))
      return false;

   /* Avoid overlapping multi-registers.  Moves between them might result
   in wrong code generation.  */
   if (allocated_p){
      auto pclass = self->x_ira_pressure_class_translate[a->aclass];
      if (self->x_ira_reg_class_max_nregs[pclass][a->mode] > 1)
         return false;
   }

   return true;
}


/* The function returns TRUE if at least one hard register from ones
   starting with HARD_REGNO and containing value of MODE are in set
   HARD_REGSET.  */
//原型 ira_hard_reg_set_intersection_p ira-int.h
bool mtcs_ira_hard_reg_set_intersection_p (MtcsIra *self,int hard_regno, machine_mode mode,
             HardRegSet *hard_regset)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg =mtcs_target_get_reg(mtcsTarget);

   int i;

   gcc_assert (hard_regno >= 0);
   for (i = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,hard_regno, mode) - 1; i >= 0; i--)
      if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(hard_regset, hard_regno + i))
         return true;
   return false;
}

/* Return true if we should treat A and SUBLOOP_A as belonging to a
   single region.  */
//原型 ira_single_region_allocno_p ira-int.h
bool mtcs_ira_single_region_allocno_p (MtcsIra *self,MtcsIraAllocno *a, MtcsIraAllocno *subloop_a)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   if (mtcsOptionsItem->x_flag_ira_region != IRA_REGION_MIXED)
      return false;

   if (subloop_a->might_conflict_with_parent_p)
      return false;

   auto rclass = a->aclass;
   auto pclass = self->x_ira_pressure_class_translate[rclass];
   auto loop_used_regs = a->loop_tree_node->reg_pressure[pclass];
   return loop_used_regs <= self->x_ira_class_hard_regs_num[pclass];
}

/* Return the cost of moving the pseudo register between different hard
   registers on entry and exit from the loop.  This is the cost to use
   if the register is successfully allocated within both this loop and
   the parent loop, but the allocations for the loops differ.  */
int mtcs_ira_loop_border_costs::move_between_loops_cost () const
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsIra);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,m_mode);
   auto move_cost =mtcsIraInt->x_ira_register_move_cost[m_mode][m_class][m_class];
   return move_cost * (m_entry_freq + m_exit_freq);
}

static void mtcsIraInit(MtcsIra *self)
{
   self->ira_in_progress = false;
}

MtcsIra *mtcs_ira_new(MtcsMode *mtcsMode)
{
    MtcsIra *self = n_slice_alloc0 (sizeof(MtcsIra));
    mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
    mtcsIraInit(self);
    return self;
}

/* Run the integrated register allocator.  */

//namespace {
//
//const pass_data pass_data_ira =
//{
//  RTL_PASS, /* type */
//  "ira", /* name */
//  OPTGROUP_NONE, /* optinfo_flags */
//  TV_IRA, /* tv_id */
//  0, /* properties_required */
//  0, /* properties_provided */
//  0, /* properties_destroyed */
//  0, /* todo_flags_start */
//  TODO_do_not_ggc_collect, /* todo_flags_finish */
//};
//
//class pass_ira : public rtl_opt_pass
//{
//public:
//  pass_ira (gcc::context *ctxt)
//    : rtl_opt_pass (pass_data_ira, ctxt)
//  {}
//
//  /* opt_pass methods: */
//  bool gate (function *) final override
//    {
//      return !targetm.no_register_allocation;
//    }
//  unsigned int execute (function *) final override
//    {
//      self->ira_in_progress = true;
//      ira (dump_file);
//      self->ira_in_progress = false;
//      return 0;
//    }
//
//}; // class pass_ira
//
//} // anon namespace
//
//rtl_opt_pass *
//make_pass_ira (gcc::context *ctxt)
//{
//  return new pass_ira (ctxt);
//}
//
//namespace {
//
//const pass_data pass_data_reload =
//{
//  RTL_PASS, /* type */
//  "reload", /* name */
//  OPTGROUP_NONE, /* optinfo_flags */
//  TV_RELOAD, /* tv_id */
//  0, /* properties_required */
//  0, /* properties_provided */
//  0, /* properties_destroyed */
//  0, /* todo_flags_start */
//  0, /* todo_flags_finish */
//};
//
//class pass_reload : public rtl_opt_pass
//{
//public:
//  pass_reload (gcc::context *ctxt)
//    : rtl_opt_pass (pass_data_reload, ctxt)
//  {}
//
//  /* opt_pass methods: */
//  bool gate (function *) final override
//    {
//      return !targetm.no_register_allocation;
//    }
//  unsigned int execute (function *) final override
//    {
//      do_reload ();
//      return 0;
//    }
//
//}; // class pass_reload
//
//} // anon namespace
//
//rtl_opt_pass *
//make_pass_reload (gcc::context *ctxt)
//{
//  return new pass_reload (ctxt);
//}


