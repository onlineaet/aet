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
 * base on ira-costs.cc
 */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "predict.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "regset.h"
#include "ira.h"
#include "ira-int.h"
#include "addresses.h"
#include "reload.h"
#include "print-rtl.h"
#include "alloc-pool.h"

#include "mtcsiracosts.h"
#include "mtcsira.h"
#include "mtcsiraint.h"
#include "../mtcstarget.h"
#include "mtcsirabuild.h"

/* Return pointer to structure containing costs of allocno or pseudo
   with given NUM in array ARR.  */
#define COSTS(arr, num) ((IraCosts *) ((char *) (arr) + (num) * self->struct_costs_size))

/* Return index in COSTS when processing reg with REGNO.  */
#define COST_INDEX(regno) (self->allocno_p ? mtcsIraBuild->ira_curr_regno_allocno_map[regno]->num : (int) regno)

/* Helper for cost_classes hashing.  */

struct cost_classes_hasher : pointer_hash <CostClasses>
{
  static inline hashval_t hash (const CostClasses *);
  static inline bool equal (const CostClasses *, const CostClasses *);
  static inline void remove (CostClasses *);
};

/* Returns hash value for cost classes info HV.  */
inline hashval_t cost_classes_hasher::hash (const CostClasses *hv)
{
  return iterative_hash (&hv->classes, sizeof (enum reg_class) * hv->num, 0);
}

/* Compares cost classes info HV1 and HV2.  */
inline bool cost_classes_hasher::equal (const CostClasses *hv1, const CostClasses *hv2)
{
  return (hv1->num == hv2->num
     && memcmp (hv1->classes, hv2->classes,
           sizeof (enum reg_class) * hv1->num) == 0);
}

/* Delete cost classes info V from the hash table.  */
inline void cost_classes_hasher::remove (CostClasses *v)
{
  ira_free (v);
}


/* Use the array of classes in CLASSES_PTR to fill out the rest of
   the structure.  */
static void complete_cost_classes (MtcsIraCosts *self,CostClasses * classes_ptr)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);
   for (int i = 0; i < mtcs_reg_get_n_reg_classes/*!N_REG_CLASSES*/(mtcsReg); i++)
      classes_ptr->index[i] = -1;
   for (int i = 0; i < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; i++)
      classes_ptr->hard_regno_index[i] = -1;
   for (int i = 0; i < classes_ptr->num; i++){
      enum reg_class cl = classes_ptr->classes[i];
      classes_ptr->index[cl] = i;
      for (int j = ira_class_hard_regs_num[cl] - 1; j >= 0; j--){
         unsigned int hard_regno = mtcsIra->x_ira_class_hard_regs[cl][j];
         if (classes_ptr->hard_regno_index[hard_regno] < 0)
            classes_ptr->hard_regno_index[hard_regno] = i;
      }
   }
}

/* Initialize info about the cost classes for each pseudo.  */
static void initiate_regno_cost_classes (MtcsIraCosts *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int size = sizeof (CostClasses *) * mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);

   self->regno_cost_classes = (CostClasses * *) ira_allocate (size);
   memset (self->regno_cost_classes, 0, size);
   memset (self->cost_classes_aclass_cache, 0,
   sizeof (CostClasses *) * mtcs_reg_get_n_reg_classes/*!N_REG_CLASSES*/(mtcsReg));
   memset (self->cost_classes_mode_cache, 0,
   sizeof (CostClasses *) * MAX_MACHINE_MODE);
   self->cost_classes_htab = new hash_table<cost_classes_hasher> (200);
   self->all_cost_classes.num = mtcsIraInt->x_ira_important_classes_num;
   for (int i = 0; i < mtcsIraInt->x_ira_important_classes_num; i++)
      self->all_cost_classes.classes[i] = mtcsIraInt->x_ira_important_classes[i];
   complete_cost_classes(self,&self->all_cost_classes);
}

/* Create new cost classes from cost classes FROM and set up members
   index and hard_regno_index.  Return the new classes.  The function
   implements some common code of two functions
   setup_regno_cost_classes_by_aclass and
   setup_regno_cost_classes_by_mode.  */
static CostClasses * setup_cost_classes (MtcsIraCosts *self,CostClasses * from)
{
   CostClasses * classes_ptr;

   classes_ptr = (CostClasses *) ira_allocate (sizeof (CostClasses));
   classes_ptr->num = from->num;
   for (int i = 0; i < from->num; i++)
      classes_ptr->classes[i] = from->classes[i];
   complete_cost_classes(self,classes_ptr);
   return classes_ptr;
}

/* Return a version of FULL that only considers registers in REGS that are
   valid for mode MODE.  Both FULL and the returned class are globally
   allocated.  */
static CostClasses * restrict_cost_classes (MtcsIraCosts *self,CostClasses * full, machine_mode mode,
             HardRegSet *regs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   static CostClasses narrow;
   int map[mtcs_reg_get_n_reg_classes/*!N_REG_CLASSES*/(mtcsReg)];
   narrow.num = 0;
   for (int i = 0; i < full->num; i++){
      /* Assume that we'll drop the class.  */
      map[i] = -1;

      /* Ignore classes that are too small for the mode.  */
      enum reg_class cl = full->classes[i];
      if (!  mtcsReg->hardRegs.x_contains_reg_of_mode/*!contains_reg_of_mode*/[cl][mode])
         continue;

      /* Calculate the set of registers in CL that belong to REGS and
      are valid for MODE.  */
      HardRegSet valid_for_cl = mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl] & *regs;
      valid_for_cl &= ~(mtcsIra->x_ira_prohibited_class_mode_regs[cl][mode] | mtcsIra->x_ira_no_alloc_regs);
      if (mtcs_reg_hard_reg_set_empty_p/*!hard_reg_set_empty_p*/(&valid_for_cl))
         continue;

      /* Don't use this class if the set of valid registers is a subset
      of an existing class.  For example, suppose we have two classes
      GR_REGS and FR_REGS and a union class GR_AND_FR_REGS.  Suppose
      that the mode changes allowed by FR_REGS are not as general as
      the mode changes allowed by GR_REGS.

      In this situation, the mode changes for GR_AND_FR_REGS could
      either be seen as the union or the intersection of the mode
      changes allowed by the two subclasses.  The justification for
      the union-based definition would be that, if you want a mode
      change that's only allowed by GR_REGS, you can pick a register
      from the GR_REGS subclass.  The justification for the
      intersection-based definition would be that every register
      from the class would allow the mode change.

      However, if we have a register that needs to be in GR_REGS,
      using GR_AND_FR_REGS with the intersection-based definition
      would be too pessimistic, since it would bring in restrictions
      that only apply to FR_REGS.  Conversely, if we have a register
      that needs to be in FR_REGS, using GR_AND_FR_REGS with the
      union-based definition would lose the extra restrictions
      placed on FR_REGS.  GR_AND_FR_REGS is therefore only useful
      for cases where GR_REGS and FP_REGS are both valid.  */
      int pos;
      for (pos = 0; pos < narrow.num; ++pos){
         enum reg_class cl2 = narrow.classes[pos];
         if (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&valid_for_cl,
               &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl2]))
            break;
      }
      map[i] = pos;
      if (pos == narrow.num){
         /* If several classes are equivalent, prefer to use the one
         that was chosen as the allocno class.  */
         enum reg_class cl2 = mtcsIra->x_ira_allocno_class_translate[cl];
         if (mtcsIra->x_ira_class_hard_regs_num[cl] == mtcsIra->x_ira_class_hard_regs_num[cl2])
            cl = cl2;
         narrow.classes[narrow.num++] = cl;
      }
   }
   if (narrow.num == full->num)
      return full;

   CostClasses **slot = self->cost_classes_htab->find_slot (&narrow, INSERT);
   if (*slot == NULL){
      CostClasses * classes = setup_cost_classes(self,&narrow);
      /* Map equivalent classes to the representative that we chose above.  */
      for (int i = 0; i < mtcsIraInt->x_ira_important_classes_num; i++){
         enum reg_class cl = mtcsIraInt->x_ira_important_classes[i];
         int index = full->index[cl];
         if (index >= 0)
            classes->index[cl] = map[index];
      }
      *slot = classes;
   }
   return *slot;
}

/* Setup cost classes for pseudo REGNO whose allocno class is ACLASS.
   This function is used when we know an initial approximation of
   allocno class of the pseudo already, e.g. on the second iteration
   of class cost calculation or after class cost calculation in
   register-pressure sensitive insn scheduling or register-pressure
   sensitive loop-invariant motion.  */
static void setup_regno_cost_classes_by_aclass (MtcsIraCosts *self,int regno, enum reg_class aclass)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   static CostClasses classes;
   CostClasses * classes_ptr;
   enum reg_class cl;
   int i;
   CostClasses **slot;
   HardRegSet /*!HARD_REG_SET*/ temp = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};
   HardRegSet /*!HARD_REG_SET*/ temp2 = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};

   bool exclude_p;

   if ((classes_ptr = self->cost_classes_aclass_cache[aclass]) == NULL){
      temp =  mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[aclass] & ~mtcsIra->x_ira_no_alloc_regs;
      /* We exclude classes from consideration which are subsets of
      ACLASS only if ACLASS is an uniform class.  */
      exclude_p = mtcsIraInt->x_ira_uniform_class_p[aclass];
      classes.num = 0;
      for (i = 0; i < mtcsIraInt->x_ira_important_classes_num; i++){
         cl = mtcsIraInt->x_ira_important_classes[i];
         if (exclude_p) {
            /* Exclude non-uniform classes which are subsets of
            ACLASS.  */
            temp2 =  mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl] & ~mtcsIra->x_ira_no_alloc_regs;
            if (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&temp2, &temp) && cl != aclass)
               continue;
         }
         classes.classes[classes.num++] = cl;
      }
      slot = self->cost_classes_htab->find_slot (&classes, INSERT);
      if (*slot == NULL){
         classes_ptr = setup_cost_classes(self,&classes);
         *slot = classes_ptr;
      }
      classes_ptr = self->cost_classes_aclass_cache[aclass] = (CostClasses *) *slot;
   }
   if (regno_reg_rtx[regno] != NULL_RTX){
      /* Restrict the classes to those that are valid for REGNO's mode
      (which might for example exclude singleton classes if the mode
      requires two registers).  Also restrict the classes to those that
      are valid for subregs of REGNO.  */
      const HardRegSet *valid_regs = mtcs_reg_valid_mode_changes_for_regno/*!valid_mode_changes_for_regno*/(mtcsReg,regno);
      if (!valid_regs)
         valid_regs = & mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[mtcs_reg_get_all_regs/*!ALL_REGS*/(mtcsReg)];
      classes_ptr = restrict_cost_classes(self,classes_ptr,
            mtcs_rtl_data_get_pseudo_regno_mode/*!PSEUDO_REGNO_MODE*/(mtcsRtlData,regno), valid_regs);
   }
   self->regno_cost_classes[regno] = classes_ptr;
}

/* Setup cost classes for pseudo REGNO with MODE.  Usage of MODE can
   decrease number of cost classes for the pseudo, if hard registers
   of some important classes cannot hold a value of MODE.  So the
   pseudo cannot get hard register of some important classes and cost
   calculation for such important classes is only wasting CPU
   time.  */
static void setup_regno_cost_classes_by_mode (MtcsIraCosts *self,int regno, machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   if (const HardRegSet *valid_regs = mtcs_reg_valid_mode_changes_for_regno/*!valid_mode_changes_for_regno*/(mtcsReg,regno))
      self->regno_cost_classes[regno] = restrict_cost_classes(self,&self->all_cost_classes, mode, valid_regs);
   else{
      if (self->cost_classes_mode_cache[mode] == NULL)
         self->cost_classes_mode_cache[mode] = restrict_cost_classes(self,&self->all_cost_classes, mode,
               &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[mtcs_reg_get_all_regs/*!ALL_REGS*/(mtcsReg)]);
      self->regno_cost_classes[regno] = self->cost_classes_mode_cache[mode];
   }
}

/* Finalize info about the cost classes for each pseudo.  */
static void finish_regno_cost_classes (MtcsIraCosts *self)
{
   ira_free (self->regno_cost_classes);
   delete self->cost_classes_htab;
   self->cost_classes_htab = NULL;
}



/* Compute the cost of loading X into (if TO_P is TRUE) or from (if
   TO_P is FALSE) a register of class RCLASS in mode MODE.  X must not
   be a pseudo register.  */
static int copy_cost (MtcsIraCosts *self,rtx x, machine_mode mode, reg_class_t rclass, bool to_p,
      secondary_reload_info *prev_sri)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   secondary_reload_info sri;
   reg_class_t secondary_class = NO_REGS;

   /* If X is a SCRATCH, there is actually nothing to move since we are
   assuming optimal allocation.  */
   if (GET_CODE (x) == SCRATCH)
      return 0;

   /* Get the class we will actually use for a reload.  */
   rclass = mtcsTarget/*!targetm.preferred_reload_class*/->preferred_reload_class(mtcsTarget,x, rclass);

   /* If we need a secondary reload for an intermediate, the cost is
   that to load the input into the intermediate register, then to
   copy it.  */
   sri.prev_sri = prev_sri;
   sri.extra_cost = 0;
   /* PR 68770: Secondary reload might examine the t_icode field.  */
   sri.t_icode = CODE_FOR_nothing;

   secondary_class = mtcsTarget/*!targetm.secondary_reload*/->secondary_reload(mtcsTarget,to_p, x, rclass, mode, &sri);

   if (secondary_class != NO_REGS) {
      mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,mode);
      return (mtcsIraInt->x_ira_register_move_cost[mode][(int) secondary_class][(int) rclass]
                                            + sri.extra_cost  + copy_cost(self,x, mode, secondary_class, to_p, &sri));
   }

   /* For memory, use the memory move cost, for (hard) registers, use
   the cost to move between the register classes, and use 2 for
   everything else (constants).  */
   if (MEM_P (x) || rclass == NO_REGS)
      return sri.extra_cost + mtcsIra->x_ira_memory_move_cost[mode][(int) rclass][to_p != 0];
   else if (REG_P (x)){
      reg_class_t x_class = mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,REGNO (x));

      mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,mode);
      return (sri.extra_cost + mtcsIraInt->x_ira_register_move_cost[mode][(int) x_class][(int) rclass]);
   }else
      /* If this is a constant, we may eventually want to call rtx_cost
      here.  */
      return sri.extra_cost + COSTS_N_INSNS (1);
}



/* Record the cost of using memory or hard registers of various
   classes for the operands in INSN.

   N_ALTS is the number of alternatives.
   N_OPS is the number of operands.
   OPS is an array of the operands.
   MODES are the modes of the operands, in case any are VOIDmode.
   CONSTRAINTS are the constraints to use for the operands.  This array
   is modified by this procedure.

   This procedure works alternative by alternative.  For each
   alternative we assume that we will be able to allocate all allocnos
   to their ideal register class and calculate the cost of using that
   alternative.  Then we compute, for each operand that is a
   pseudo-register, the cost of having the allocno allocated to each
   register class and using it in that alternative.  To this cost is
   added the cost of the alternative.

   The cost of each class for this insn is its lowest cost among all
   the alternatives.  */
static void record_reg_classes (MtcsIraCosts *self,int n_alts, int n_ops, rtx *ops,
          machine_mode *modes, const char **constraints,
          rtx_insn *insn, enum reg_class *pref)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   int alt;
   int i, j, k;
   int insn_allows_mem[mtcs_recog_get_max_recog_operands/*!MAX_RECOG_OPERANDS*/(mtcsRecog)];
   ira_move_table *move_in_cost, *move_out_cost;
   short (*mem_cost)[2];
   const char *p;

   if (mtcsIraGlobal->ira_dump_file != NULL && mtcsIraGlobal->internal_flag_ira_verbose > 5){
      fprintf (mtcsIraGlobal->ira_dump_file, "    Processing insn %u", INSN_UID (insn));
      if (INSN_CODE (insn) >= 0 && (p = mtcs_output_get_insn_name/*!get_insn_name*/(mtcsOutput,INSN_CODE (insn))) != NULL)
         fprintf (mtcsIraGlobal->ira_dump_file, " {%s}", p);
      fprintf (mtcsIraGlobal->ira_dump_file, " (freq=%d)\n",REG_FREQ_FROM_BB (BLOCK_FOR_INSN (insn)));
      dump_insn_slim (mtcsIraGlobal->ira_dump_file, insn);
   }

   for (i = 0; i < n_ops; i++)
      insn_allows_mem[i] = 0;

   /* Process each alternative, each time minimizing an operand's cost
   with the cost for each operand in that alternative.  */
   alternative_mask preferred = mtcs_recog_get_preferred_alternatives/*!get_preferred_alternatives*/(mtcsRecog,insn);
   for (alt = 0; alt < n_alts; alt++){
   enum reg_class classes[mtcs_recog_get_max_recog_operands/*!MAX_RECOG_OPERANDS*/(mtcsRecog)];
   int allows_mem[mtcs_recog_get_max_recog_operands/*!MAX_RECOG_OPERANDS*/(mtcsRecog)];
   enum reg_class rclass;
   int alt_fail = 0;
   int alt_cost = 0, op_cost_add;

   if (!TEST_BIT (preferred, alt)){
      for (i = 0; i < mtcsRecog->recog_data.n_operands; i++)
         constraints[i] = skip_alternative (constraints[i]);

      continue;
   }

   if (mtcsIraGlobal->ira_dump_file != NULL && mtcsIraGlobal->internal_flag_ira_verbose > 5){
      fprintf (mtcsIraGlobal->ira_dump_file, "      Alt %d:", alt);
      for (i = 0; i < n_ops; i++){
         p = constraints[i];
         if (*p == '\0')
            continue;
         fprintf (mtcsIraGlobal->ira_dump_file, "  (%d) ", i);
         for (; *p != '\0' && *p != ',' && *p != '#'; p++)
            fputc (*p, mtcsIraGlobal->ira_dump_file);
      }
      fprintf (mtcsIraGlobal->ira_dump_file, "\n");
   }

   for (i = 0; i < n_ops; i++){
      unsigned char c;
      const char *p = constraints[i];
      rtx op = ops[i];
      machine_mode mode = modes[i];
      int allows_addr = 0;
      int win = 0;

      /* Initially show we know nothing about the register class.  */
      classes[i] = NO_REGS;
      allows_mem[i] = 0;

      /* If this operand has no constraints at all, we can
      conclude nothing about it since anything is valid.  */
      if (*p == 0){
         if (REG_P (op) && REGNO (op) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
            memset (mtcsIraInt->x_this_op_costs[i], 0, self->struct_costs_size);
         continue;
      }

      /* If this alternative is only relevant when this operand
      matches a previous operand, we do different things
      depending on whether this operand is a allocno-reg or not.
      We must process any modifiers for the operand before we
      can make this test.  */
      while (*p == '%' || *p == '=' || *p == '+' || *p == '&')
         p++;

      if (p[0] >= '0' && p[0] <= '0' + i){
      /* Copy class and whether memory is allowed from the
      matching alternative.  Then perform any needed cost
      computations and/or adjustments.  */
      j = p[0] - '0';
      classes[i] = classes[j];
      allows_mem[i] = allows_mem[j];
      if (allows_mem[i])
         insn_allows_mem[i] = 1;

      if (! REG_P (op) || REGNO (op) < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/){
         /* If this matches the other operand, we have no
         added cost and we win.  */
         if (rtx_equal_p (ops[j], op))
            win = 1;
         /* If we can put the other operand into a register,
         add to the cost of this alternative the cost to
         copy this operand to the register used for the
         other operand.  */
         else if (classes[j] != NO_REGS){
            alt_cost += copy_cost(self,op, mode, classes[j], 1, NULL);
            win = 1;
         }
      }else if (! REG_P (ops[j])    || REGNO (ops[j]) < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/){
         /* This op is an allocno but the one it matches is
         not.  */

         /* If we can't put the other operand into a
         register, this alternative can't be used.  */

         if (classes[j] == NO_REGS){
            alt_fail = 1;
         }else
         /* Otherwise, add to the cost of this alternative the cost
         to copy the other operand to the hard register used for
         this operand.  */
         {
            alt_cost += copy_cost(self,ops[j], mode, classes[j], 1, NULL);
         }
      }else{
         /* The costs of this operand are not the same as the
         other operand since move costs are not symmetric.
         Moreover, if we cannot tie them, this alternative
         needs to do a copy, which is one insn.  */
         IraCosts *pp = mtcsIraInt->x_this_op_costs[i];
         int *pp_costs = pp->cost;
         CostClasses * cost_classes_ptr = self->regno_cost_classes[REGNO (op)];
         enum reg_class *cost_classes = cost_classes_ptr->classes;
         bool in_p = mtcsRecog->recog_data.operand_type[i] != OP_OUT;
         bool out_p = mtcsRecog->recog_data.operand_type[i] != OP_IN;
         enum reg_class op_class = classes[i];

         mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,mode);
         if (! in_p){
            ira_assert (out_p);
            if (op_class == NO_REGS){
               mem_cost = mtcsIra->x_ira_memory_move_cost[mode];
               for (k = cost_classes_ptr->num - 1; k >= 0; k--){
                  rclass = cost_classes[k];
                  pp_costs[k] = mem_cost[rclass][0] * self->frequency;
               }
            }else{
               move_out_cost =mtcsIraInt->x_ira_may_move_out_cost[mode];
               for (k = cost_classes_ptr->num - 1; k >= 0; k--){
                  rclass = cost_classes[k];
                  pp_costs[k] = move_out_cost[op_class][rclass] * self->frequency;
               }
            }
         }else if (! out_p){
            ira_assert (in_p);
            if (op_class == NO_REGS){
               mem_cost = mtcsIra->x_ira_memory_move_cost[mode];
               for (k = cost_classes_ptr->num - 1; k >= 0; k--){
                  rclass = cost_classes[k];
                  pp_costs[k] = mem_cost[rclass][1] * self->frequency;
               }
            }else{
               move_in_cost = mtcsIraInt->x_ira_may_move_in_cost[mode];
               for (k = cost_classes_ptr->num - 1; k >= 0; k--){
                  rclass = cost_classes[k];
                  pp_costs[k] = move_in_cost[rclass][op_class] * self->frequency;
               }
            }
         }else{
            if (op_class == NO_REGS){
               mem_cost = mtcsIra->x_ira_memory_move_cost[mode];
               for (k = cost_classes_ptr->num - 1; k >= 0; k--){
                  rclass = cost_classes[k];
                  pp_costs[k] = ((mem_cost[rclass][0] + mem_cost[rclass][1])* self->frequency);
               }
            }else{
               move_in_cost = mtcsIraInt->x_ira_may_move_in_cost[mode];
               move_out_cost = mtcsIraInt->x_ira_may_move_out_cost[mode];
               for (k = cost_classes_ptr->num - 1; k >= 0; k--){
                  rclass = cost_classes[k];
                  pp_costs[k] = ((move_in_cost[rclass][op_class] + move_out_cost[op_class][rclass])* self->frequency);
               }
            }
         }

         /* If the alternative actually allows memory, make
         things a bit cheaper since we won't need an extra
         insn to load it.  */
         pp->mem_cost = ((out_p ? mtcsIra->x_ira_memory_move_cost[mode][op_class][0] : 0)
            + (in_p ? mtcsIra->x_ira_memory_move_cost[mode][op_class][1] : 0)
            - allows_mem[i]) * self->frequency;

         /* If we have assigned a class to this allocno in
         our first pass, add a cost to this alternative
         corresponding to what we would add if this
         allocno were not in the appropriate class.  */
         if (pref){
            enum reg_class pref_class = pref[COST_INDEX (REGNO (op))];

            if (pref_class == NO_REGS)
               alt_cost += ((out_p ? mtcsIra->x_ira_memory_move_cost[mode][op_class][0] : 0)
                        + (in_p  ? mtcsIra->x_ira_memory_move_cost[mode][op_class][1] : 0));
            else if (mtcsIraInt->x_ira_reg_class_intersect[pref_class][op_class] == NO_REGS)
               alt_cost  += mtcsIraInt->x_ira_register_move_cost[mode][pref_class][op_class];
         }
         if (REGNO (ops[i]) != REGNO (ops[j])  && ! find_reg_note (insn, REG_DEAD, op))
            alt_cost += 2;

         p++;
      }
   }

   /* Scan all the constraint letters.  See if the operand
   matches any of the constraints.  Collect the valid
   register classes and see if this operand accepts
   memory.  */
   while ((c = *p)){
      switch (c){
         case '*':
            /* Ignore the next letter for this pass.  */
            c = *++p;
            break;

         case '^':
            alt_cost += 2;
            break;

         case '?':
            alt_cost += 2;
            break;

         case 'g':
            if (MEM_P (op)  || (CONSTANT_P (op)  && (! mtcsOptionsItem->x_flag_pic
            || mtcs_recog_is_legitimate_pic_operand_p/*!LEGITIMATE_PIC_OPERAND_P*/(mtcsRecog,op))))
               win = 1;
            insn_allows_mem[i] = allows_mem[i] = 1;
            classes[i] = mtcsIraInt->x_ira_reg_class_subunion[classes[i]][mtcs_reg_get_general_regs/*!GENERAL_REGS*/(mtcsReg)];
            break;

         default:
            enum constraint_num cn = mtcs_preds_lookup_constraint/*!lookup_constraint*/(mtcsPreds,p);
            enum reg_class cl;
            switch (mtcs_preds_get_constraint_type/*!get_constraint_type*/(mtcsPreds,cn)){
               case CT_REGISTER:
                  cl = mtcs_preds_reg_class_for_constraint/*!reg_class_for_constraint*/(mtcsPreds,cn);
                  if (cl != NO_REGS)
                     classes[i] = mtcsIraInt->x_ira_reg_class_subunion[classes[i]][cl];
                  break;

               case CT_CONST_INT:
                  if (CONST_INT_P (op)
                  && mtcs_preds_insn_const_int_ok_for_constraint/*!insn_const_int_ok_for_constraint*/(mtcsPreds,INTVAL (op), cn))
                     win = 1;
                  break;

               case CT_MEMORY:
               case CT_RELAXED_MEMORY:
                  /* Every MEM can be reloaded to fit.  */
                  insn_allows_mem[i] = allows_mem[i] = 1;
                  if (MEM_P (op))
                     win = 1;
                  break;

               case CT_SPECIAL_MEMORY:
                  insn_allows_mem[i] = allows_mem[i] = 1;
                  if (MEM_P (extract_mem_from_operand (op))  && mtcs_preds_constraint_satisfied_p/*!constraint_satisfied_p*/(mtcsPreds,op, cn))
                     win = 1;
                  break;

               case CT_ADDRESS:
                  /* Every address can be reloaded to fit.  */
                  allows_addr = 1;
                  if (address_operand (op, GET_MODE (op))
                  || mtcs_preds_constraint_satisfied_p/*!constraint_satisfied_p*/(mtcsPreds,op, cn))
                     win = 1;
                  /* We know this operand is an address, so we
                  want it to be allocated to a hard register
                  that can be the base of an address,
                  i.e. BASE_REG_CLASS.  */
                  classes[i] = mtcsIraInt->x_ira_reg_class_subunion[classes[i]][base_reg_class (VOIDmode, ADDR_SPACE_GENERIC, ADDRESS, SCRATCH)];
                  break;

               case CT_FIXED_FORM:
                  if (mtcs_preds_constraint_satisfied_p/*!constraint_satisfied_p*/(mtcsPreds,op, cn))
                     win = 1;
                  break;
            }
            break;
      }
      p += mtcs_preds_insn_constraint_len/*!CONSTRAINT_LEN*/(mtcsPreds,c, p);
      if (c == ',')
         break;
   }

   constraints[i] = p;

   if (alt_fail)
      break;

   /* How we account for this operand now depends on whether it
   is a pseudo register or not.  If it is, we first check if
   any register classes are valid.  If not, we ignore this
   alternative, since we want to assume that all allocnos get
   allocated for register preferencing.  If some register
   class is valid, compute the costs of moving the allocno
   into that class.  */
   if (REG_P (op) && REGNO (op) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/){
      if (classes[i] == NO_REGS && ! allows_mem[i]){
         /* We must always fail if the operand is a REG, but
         we did not find a suitable class and memory is
         not allowed.

         Otherwise we may perform an uninitialized read
         from mtcsIraInt->x_this_op_costs after the `continue' statement
         below.  */
         alt_fail = 1;
      }else{
         unsigned int regno = REGNO (op);
         IraCosts *pp = mtcsIraInt->x_this_op_costs[i];
         int *pp_costs = pp->cost;
         CostClasses * cost_classes_ptr = self->regno_cost_classes[regno];
         enum reg_class *cost_classes = cost_classes_ptr->classes;
         bool in_p = mtcsRecog->recog_data.operand_type[i] != OP_OUT;
         bool out_p = mtcsRecog->recog_data.operand_type[i] != OP_IN;
         enum reg_class op_class = classes[i];

         mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,mode);
         if (! in_p){
            ira_assert (out_p);
            if (op_class == NO_REGS){
               mem_cost = mtcsIra->x_ira_memory_move_cost[mode];
               for (k = cost_classes_ptr->num - 1; k >= 0; k--){
                  rclass = cost_classes[k];
                  pp_costs[k] = mem_cost[rclass][0] * self->frequency;
               }
            }else{
               move_out_cost = mtcsIraInt->x_ira_may_move_out_cost[mode];
               for (k = cost_classes_ptr->num - 1; k >= 0; k--){
                  rclass = cost_classes[k];
                  pp_costs[k] = move_out_cost[op_class][rclass] * self->frequency;
               }
            }
         }else if (! out_p){
            ira_assert (in_p);
            if (op_class == NO_REGS){
               mem_cost = mtcsIra->x_ira_memory_move_cost[mode];
               for (k = cost_classes_ptr->num - 1; k >= 0; k--){
                  rclass = cost_classes[k];
                  pp_costs[k] = mem_cost[rclass][1] * self->frequency;
               }
            }else{
               move_in_cost = mtcsIraInt->x_ira_may_move_in_cost[mode];
               for (k = cost_classes_ptr->num - 1; k >= 0; k--){
                  rclass = cost_classes[k];
                  pp_costs[k] = move_in_cost[rclass][op_class] * self->frequency;
               }
            }
         }else{
            if (op_class == NO_REGS){
               mem_cost = mtcsIra->x_ira_memory_move_cost[mode];
               for (k = cost_classes_ptr->num - 1; k >= 0; k--){
                  rclass = cost_classes[k];
                  pp_costs[k] = ((mem_cost[rclass][0] + mem_cost[rclass][1]) * self->frequency);
               }
            }else{
               move_in_cost = mtcsIraInt->x_ira_may_move_in_cost[mode];
               move_out_cost = mtcsIraInt->x_ira_may_move_out_cost[mode];
               for (k = cost_classes_ptr->num - 1; k >= 0; k--){
                  rclass = cost_classes[k];
                  pp_costs[k] = ((move_in_cost[rclass][op_class] + move_out_cost[op_class][rclass]) * self->frequency);
               }
            }
         }

         if (op_class == NO_REGS)
            /* Although we don't need insn to reload from
            memory, still accessing memory is usually more
            expensive than a register.  */
            pp->mem_cost = self->frequency;
         else
            /* If the alternative actually allows memory, make
            things a bit cheaper since we won't need an
            extra insn to load it.  */
            pp->mem_cost  = ((out_p ? mtcsIra->x_ira_memory_move_cost[mode][op_class][0] : 0)
            + (in_p ? mtcsIra->x_ira_memory_move_cost[mode][op_class][1] : 0) - allows_mem[i]) * self->frequency;
         /* If we have assigned a class to this allocno in
         our first pass, add a cost to this alternative
         corresponding to what we would add if this
         allocno were not in the appropriate class.  */
         if (pref){
            enum reg_class pref_class = pref[COST_INDEX (REGNO (op))];

            if (pref_class == NO_REGS){
               if (op_class != NO_REGS)
                  alt_cost  += ((out_p ? mtcsIra->x_ira_memory_move_cost[mode][op_class][0] : 0)
                  + (in_p ? mtcsIra->x_ira_memory_move_cost[mode][op_class][1]  : 0));
            }else if (op_class == NO_REGS)
               alt_cost  += ((out_p ? mtcsIra->x_ira_memory_move_cost[mode][pref_class][1] : 0)
                        + (in_p ? mtcsIra->x_ira_memory_move_cost[mode][pref_class][0] : 0));
            else if (mtcsIraInt->x_ira_reg_class_intersect[pref_class][op_class] == NO_REGS)
               alt_cost += (mtcsIraInt->x_ira_register_move_cost[mode][pref_class][op_class]);
         }
      }
   }

   /* Otherwise, if this alternative wins, either because we
   have already determined that or if we have a hard
   register of the proper class, there is no cost for this
   alternative.  */
   else if (win || (REG_P (op)   && reg_fits_class_p (op, classes[i], 0, GET_MODE (op))))
      ;

   /* If registers are valid, the cost of this alternative
   includes copying the object to and/or from a
   register.  */
   else if (classes[i] != NO_REGS){
      if (mtcsRecog->recog_data.operand_type[i] != OP_OUT)
         alt_cost += copy_cost(self,op, mode, classes[i], 1, NULL);

      if (mtcsRecog->recog_data.operand_type[i] != OP_IN)
         alt_cost += copy_cost(self,op, mode, classes[i], 0, NULL);
   }
   /* The only other way this alternative can be used is if
   this is a constant that could be placed into memory.  */
   else if (CONSTANT_P (op) && (allows_addr || allows_mem[i]))
      alt_cost += mtcsIra->x_ira_memory_move_cost[mode][classes[i]][1];
   else
      alt_fail = 1;

   if (alt_fail)
      break;
   }

   if (alt_fail){
      /* The loop above might have exited early once the failure
      was seen.  Skip over the constraints for the remaining
      operands.  */
      i += 1;
      for (; i < n_ops; ++i)
         constraints[i] = skip_alternative (constraints[i]);
      continue;
   }

   op_cost_add = alt_cost * self->frequency;
   MtcsRegClass *mtcsRegClass =mtcs_reg_get_reg_class(mtcsReg);
   /* Finally, update the costs with the information we've
   calculated about this alternative.  */
   for (i = 0; i < n_ops; i++)
      if (REG_P (ops[i]) && REGNO (ops[i]) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/){
         int old_cost;
         bool cost_change_p = false;
         IraCosts *pp = mtcsIraInt->x_op_costs[i], *qq = mtcsIraInt->x_this_op_costs[i];
         int *pp_costs = pp->cost, *qq_costs = qq->cost;
         int scale = 1 + (mtcsRecog->recog_data.operand_type[i] == OP_INOUT);
         CostClasses * cost_classes_ptr = self->regno_cost_classes[REGNO (ops[i])];

         old_cost = pp->mem_cost;
         pp->mem_cost = MIN (old_cost, (qq->mem_cost + op_cost_add) * scale);

         if (mtcsIraGlobal->ira_dump_file != NULL && mtcsIraGlobal->internal_flag_ira_verbose > 5  && pp->mem_cost < old_cost){
            cost_change_p = true;
            fprintf (mtcsIraGlobal->ira_dump_file, "        op %d(r=%u) new costs MEM:%d", i, REGNO(ops[i]), pp->mem_cost);
         }
         for (k = cost_classes_ptr->num - 1; k >= 0; k--){
            old_cost = pp_costs[k];
            pp_costs[k]  = MIN (old_cost, (qq_costs[k] + op_cost_add) * scale);
            if (mtcsIraGlobal->ira_dump_file != NULL && mtcsIraGlobal->internal_flag_ira_verbose > 5  && pp_costs[k] < old_cost){
               if (!cost_change_p)
                  fprintf (mtcsIraGlobal->ira_dump_file, "        op %d(r=%u) new costs",i, REGNO(ops[i]));
               cost_change_p = true;
               fprintf (mtcsIraGlobal->ira_dump_file, " %s:%d",
                     mtcsRegClass/*!reg_class_names*/[cost_classes_ptr->classes[k]].name,pp_costs[k]);
            }
         }
         if (mtcsIraGlobal->ira_dump_file != NULL && mtcsIraGlobal->internal_flag_ira_verbose > 5  && cost_change_p)
            fprintf (mtcsIraGlobal->ira_dump_file, "\n");
      }
   }

   if (self->allocno_p)
      for (i = 0; i < n_ops; i++){
         MtcsIraAllocno * a;
         rtx op = ops[i];

         if (! REG_P (op) || REGNO (op) < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
            continue;
         a = mtcsIraBuild->ira_curr_regno_allocno_map [REGNO (op)];
         if (! a->bad_spill_p && insn_allows_mem[i] == 0)
            a->bad_spill_p = true;
      }

}



/* Wrapper around REGNO_OK_FOR_INDEX_P, to allow pseudo registers.  */
static inline bool ok_for_index_p_nonstrict (MtcsIraCosts *self,rtx reg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   unsigned regno = REGNO (reg);

   return regno >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/
         || mtcs_reg_regno_ok_for_index_p/*!REGNO_OK_FOR_INDEX_P*/(mtcsReg,regno);
}

/* A version of regno_ok_for_base_p for use here, when all
   pseudo-registers should count as OK.  Arguments as for
   regno_ok_for_base_p.  */
static inline bool ok_for_base_p_nonstrict (MtcsIraCosts *self,rtx reg, machine_mode mode, addr_space_t as,
          enum rtx_code outer_code, enum rtx_code index_code)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   unsigned regno = REGNO (reg);

   if (regno >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
      return true;
   return mtcs_reg_ok_for_base_p_1/*!ok_for_base_p_1*/(mtcsReg,regno, mode, as, outer_code, index_code,NULL);
}

/* Record the pseudo registers we must reload into hard registers in a
   subexpression of a memory address, X.

   If CONTEXT is 0, we are looking at the base part of an address,
   otherwise we are looking at the index part.

   MODE and AS are the mode and address space of the memory reference;
   OUTER_CODE and INDEX_CODE give the context that the rtx appears in.
   These four arguments are passed down to base_reg_class.

   SCALE is twice the amount to multiply the cost by (it is twice so
   we can represent half-cost adjustments).  */
static void record_address_regs (MtcsIraCosts *self,machine_mode mode, addr_space_t as, rtx x,
           int context, enum rtx_code outer_code,
           enum rtx_code index_code, int scale)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   enum rtx_code code = GET_CODE (x);
   enum reg_class rclass;

   if (context == 1)
      rclass = INDEX_REG_CLASS;
   else
      rclass = mtcs_reg_base_reg_class/*!base_reg_class*/(mtcsReg,mode, as, outer_code, index_code);

   switch (code){
      case CONST_INT:
      case CONST:
      case PC:
      case SYMBOL_REF:
      case LABEL_REF:
         return;

      case PLUS:
      /* When we have an address that is a sum, we must determine
      whether registers are "base" or "index" regs.  If there is a
      sum of two registers, we must choose one to be the "base".
      Luckily, we can use the REG_POINTER to make a good choice
      most of the time.  We only need to do this on machines that
      can have two registers in an address and where the base and
      index register classes are different.

      ??? This code used to set REGNO_POINTER_FLAG in some cases,
      but that seems bogus since it should only be set when we are
      sure the register is being used as a pointer.  */
      {
         rtx arg0 = XEXP (x, 0);
         rtx arg1 = XEXP (x, 1);
         enum rtx_code code0 = GET_CODE (arg0);
         enum rtx_code code1 = GET_CODE (arg1);

         /* Look inside subregs.  */
         if (code0 == SUBREG)
            arg0 = SUBREG_REG (arg0), code0 = GET_CODE (arg0);
         if (code1 == SUBREG)
            arg1 = SUBREG_REG (arg1), code1 = GET_CODE (arg1);

         /* If index registers do not appear, or coincide with base registers,
         just record registers in any non-constant operands.  We
         assume here, as well as in the tests below, that all
         addresses are in canonical form.  */
         if (MAX_REGS_PER_ADDRESS == 1
         || INDEX_REG_CLASS == mtcs_reg_base_reg_class/*!base_reg_class*/(mtcsReg,VOIDmode, as, PLUS, SCRATCH)){
            record_address_regs(self,mode, as, arg0, context, PLUS, code1, scale);
         if (! CONSTANT_P (arg1))
            record_address_regs(self,mode, as, arg1, context, PLUS, code0, scale);
         }
         /* If the second operand is a constant integer, it doesn't
         change what class the first operand must be.  */
         else if (CONST_SCALAR_INT_P (arg1))
            record_address_regs(self,mode, as, arg0, context, PLUS, code1, scale);
         /* If the second operand is a symbolic constant, the first
         operand must be an index register.  */
         else if (code1 == SYMBOL_REF || code1 == CONST || code1 == LABEL_REF)
            record_address_regs(self,mode, as, arg0, 1, PLUS, code1, scale);
         /* If both operands are registers but one is already a hard
         register of index or reg-base class, give the other the
         class that the hard register is not.  */
         else if (code0 == REG && code1 == REG
         && REGNO (arg0) < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/
         && (ok_for_base_p_nonstrict(self,arg0, mode, as, PLUS, REG)
         || ok_for_index_p_nonstrict(self,arg0)))
            record_address_regs(self,mode, as, arg1,ok_for_base_p_nonstrict(self,
                  arg0, mode, as, PLUS, REG) ? 1 : 0,  PLUS, REG, scale);
         else if (code0 == REG && code1 == REG
         && REGNO (arg1) < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/
         && (ok_for_base_p_nonstrict(self,arg1, mode, as, PLUS, REG)  || ok_for_index_p_nonstrict(self,arg1)))
            record_address_regs(self,mode, as, arg0, ok_for_base_p_nonstrict(self,
                  arg1, mode, as,PLUS, REG) ? 1 : 0, PLUS, REG, scale);
         /* If one operand is known to be a pointer, it must be the
         base with the other operand the index.  Likewise if the
         other operand is a MULT.  */
         else if ((code0 == REG && REG_POINTER (arg0)) || code1 == MULT){
            record_address_regs(self,mode, as, arg0, 0, PLUS, code1, scale);
            record_address_regs(self,mode, as, arg1, 1, PLUS, code0, scale);
         }else if ((code1 == REG && REG_POINTER (arg1)) || code0 == MULT){
            record_address_regs(self,mode, as, arg0, 1, PLUS, code1, scale);
            record_address_regs(self,mode, as, arg1, 0, PLUS, code0, scale);
         }
         /* Otherwise, count equal chances that each might be a base or
         index register.  This case should be rare.  */
         else{
            record_address_regs(self,mode, as, arg0, 0, PLUS, code1, scale / 2);
            record_address_regs(self,mode, as, arg0, 1, PLUS, code1, scale / 2);
            record_address_regs(self,mode, as, arg1, 0, PLUS, code0, scale / 2);
            record_address_regs(self,mode, as, arg1, 1, PLUS, code0, scale / 2);
         }
      }
         break;

      /* Double the importance of an allocno that is incremented or
      decremented, since it would take two extra insns if it ends
      up in the wrong place.  */
      case POST_MODIFY:
      case PRE_MODIFY:
         record_address_regs(self,mode, as, XEXP (x, 0), 0, code, GET_CODE (XEXP (XEXP (x, 1), 1)), 2 * scale);
         if (REG_P (XEXP (XEXP (x, 1), 1)))
            record_address_regs(self,mode, as, XEXP (XEXP (x, 1), 1), 1, code, REG,2 * scale);
         break;

      case POST_INC:
      case PRE_INC:
      case POST_DEC:
      case PRE_DEC:
         /* Double the importance of an allocno that is incremented or
         decremented, since it would take two extra insns if it ends
         up in the wrong place.  */
         record_address_regs(self,mode, as, XEXP (x, 0), 0, code, SCRATCH, 2 * scale);
            break;

      case REG:
      {
         IraCosts *pp;
         int *pp_costs;
         enum reg_class i;
         int k, regno, add_cost;
         CostClasses * cost_classes_ptr;
         enum reg_class *cost_classes;
         ira_move_table *move_in_cost;

         if (REGNO (x) < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
            break;

         regno = REGNO (x);
         if (self->allocno_p)
            mtcsIraBuild->ira_curr_regno_allocno_map[regno]->bad_spill_p = true;
         pp = COSTS (self->costs, COST_INDEX (regno));
         add_cost = (mtcsIra->x_ira_memory_move_cost[mtcs_mode_get_Pmode(mtcsMode)][rclass][1] * scale) / 2;
         if (INT_MAX - add_cost < pp->mem_cost)
            pp->mem_cost = INT_MAX;
         else
            pp->mem_cost += add_cost;
         cost_classes_ptr = self->regno_cost_classes[regno];
         cost_classes = cost_classes_ptr->classes;
         pp_costs = pp->cost;
         mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,mtcs_mode_get_Pmode(mtcsMode));
         move_in_cost = mtcsIraInt->x_ira_may_move_in_cost[mtcs_mode_get_Pmode(mtcsMode)];
         for (k = cost_classes_ptr->num - 1; k >= 0; k--){
            i = cost_classes[k];
            add_cost = (move_in_cost[i][rclass] * scale) / 2;
            if (INT_MAX - add_cost < pp_costs[k])
               pp_costs[k] = INT_MAX;
            else
               pp_costs[k] += add_cost;
         }
      }
         break;

      default:
      {
         const char *fmt = GET_RTX_FORMAT (code);
         int i;
         for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
            if (fmt[i] == 'e')
               record_address_regs(self,mode, as, XEXP (x, i), context, code, SCRATCH,scale);
      }
   }
}


/* Calculate the costs of insn operands.  */
static void record_operand_costs (MtcsIraCosts *self,rtx_insn *insn, enum reg_class *pref)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);


   const char *constraints[mtcs_recog_get_max_recog_operands/*!MAX_RECOG_OPERANDS*/(mtcsRecog)];
   machine_mode modes[mtcs_recog_get_max_recog_operands/*!MAX_RECOG_OPERANDS*/(mtcsRecog)];
   rtx set;
   int i;

   if ((set = single_set (insn)) != NULL_RTX
   /* In rare cases the single set insn might have less 2 operands
   as the source can be a fixed special reg.  */
   && mtcsRecog->recog_data.n_operands > 1
   && mtcsRecog->recog_data.operand[0] == SET_DEST (set)
   && mtcsRecog->recog_data.operand[1] == SET_SRC (set)){
      int regno, other_regno;
      rtx dest = SET_DEST (set);
      rtx src = SET_SRC (set);

      if (GET_CODE (dest) == SUBREG  && known_eq (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (dest)),
            mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (SUBREG_REG (dest)))))
         dest = SUBREG_REG (dest);
      if (GET_CODE (src) == SUBREG
      && known_eq (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (src)),
      mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (SUBREG_REG (src)))))
         src = SUBREG_REG (src);
      if (REG_P (src) && REG_P (dest)
      && (((regno = REGNO (src)) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/
      && (other_regno = REGNO (dest)) < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
      || ((regno = REGNO (dest)) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/
      && (other_regno = REGNO (src)) < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/))){

         machine_mode mode = GET_MODE (SET_SRC (set)), cost_mode = mode;
         machine_mode hard_reg_mode = GET_MODE(mtcsRtlData->regno_reg_rtx/*! regno_reg_rtx*/[other_regno]);
         poly_int64 pmode_size = mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,mode);
         poly_int64 phard_reg_mode_size = mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,hard_reg_mode);
         HOST_WIDE_INT mode_size, hard_reg_mode_size;
         CostClasses * cost_classes_ptr = self->regno_cost_classes[regno];
         enum reg_class *cost_classes = cost_classes_ptr->classes;
         reg_class_t rclass, hard_reg_class, bigger_hard_reg_class;
         int cost_factor = 1, cost, k;
         ira_move_table *move_costs;
         bool dead_p = find_regno_note (insn, REG_DEAD, REGNO (src));

         hard_reg_class = mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,other_regno);
         bigger_hard_reg_class = mtcsIra->x_ira_pressure_class_translate/*!ira_pressure_class_translate*/[hard_reg_class];
         /* Target code may return any cost for mode which does not fit the
         hard reg class (e.g. DImode for AREG on i386).  Check this and use
         a bigger class to get the right cost.  */
         if (bigger_hard_reg_class != NO_REGS
         && ! mtcs_ira_hard_reg_in_set_p/*!ira_hard_reg_in_set_p*/(mtcsIra,other_regno, mode,
                      &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[hard_reg_class]))
         hard_reg_class = bigger_hard_reg_class;
         mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,mode);
         mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,hard_reg_mode);
         /* Use smaller movement cost for natural hard reg mode or its mode as
         operand.  */
         if (pmode_size.is_constant (&mode_size)
         && phard_reg_mode_size.is_constant (&hard_reg_mode_size)){
            /* Assume we are moving in the natural modes: */
            cost_factor = mode_size / hard_reg_mode_size;
            if (mode_size % hard_reg_mode_size != 0)
               cost_factor++;
            if (cost_factor * (mtcsIraInt->x_ira_register_move_cost [hard_reg_mode][hard_reg_class][hard_reg_class])
                  < (mtcsIraInt->x_ira_register_move_cost[mode][hard_reg_class][hard_reg_class]))
               cost_mode = hard_reg_mode;
            else
               cost_factor = 1;
         }
         move_costs = mtcsIraInt->x_ira_register_move_cost[cost_mode];
         i = regno == (int) REGNO (src) ? 1 : 0;
         for (k = cost_classes_ptr->num - 1; k >= 0; k--){
            rclass = cost_classes[k];
            cost = (i == 0 ? move_costs[hard_reg_class][rclass]: move_costs[rclass][hard_reg_class]);
            cost *= cost_factor;
            mtcsIraInt->x_op_costs[i]->cost[k] = cost * self->frequency;
            /* If this insn is a single set copying operand 1 to
            operand 0 and one operand is an allocno with the
            other a hard reg or an allocno that prefers a hard
            register that is in its own register class then we
            may want to adjust the cost of that register class to
            -1.

            Avoid the adjustment if the source does not die to
            avoid stressing of register allocator by preferencing
            two colliding registers into single class.  */
            if (dead_p
            && mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(
            &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[rclass], other_regno)
            && (reg_class_size[(int) rclass] == (mtcsIra->x_ira_reg_class_max_nregs[(int) rclass][(int) GET_MODE(src)]))){
               if (reg_class_size[rclass] == 1)
                  mtcsIraInt->x_op_costs[i]->cost[k] = -self->frequency;
               else if (mtcs_reg_in_hard_reg_set_p/*!in_hard_reg_set_p*/(mtcsReg,
                     &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[rclass],GET_MODE(src), other_regno))
                  mtcsIraInt->x_op_costs[i]->cost[k] = -self->frequency;
            }
         }
         mtcsIraInt->x_op_costs[i]->mem_cost = mtcsIra->x_ira_memory_move_cost[mode][hard_reg_class][i] * self->frequency;
         return;
      }
   }

   for (i = 0; i < mtcsRecog->recog_data.n_operands; i++){
      constraints[i] = mtcsRecog->recog_data.constraints[i];
      modes[i] = mtcsRecog->recog_data.operand_mode[i];
   }

   /* If we get here, we are set up to record the costs of all the
   operands for this insn.  Start by initializing the costs.  Then
   handle any address registers.  Finally record the desired classes
   for any allocnos, doing it twice if some pair of operands are
   commutative.  */
   for (i = 0; i < mtcsRecog->recog_data.n_operands; i++){
      rtx op_mem = extract_mem_from_operand (mtcsRecog->recog_data.operand[i]);
      memcpy (mtcsIraInt->x_op_costs[i], mtcsIraInt->x_init_cost, self->struct_costs_size);

      if (GET_CODE (mtcsRecog->recog_data.operand[i]) == SUBREG)
         mtcsRecog->recog_data.operand[i] = SUBREG_REG (mtcsRecog->recog_data.operand[i]);

      if (MEM_P (op_mem))
         record_address_regs(self,GET_MODE (op_mem), MEM_ADDR_SPACE (op_mem), XEXP (op_mem, 0),0, MEM, SCRATCH, self->frequency * 2);
      else if (constraints[i][0] == 'p'  || (mtcs_preds_insn_extra_address_constraint/*!insn_extra_address_constraint*/(mtcsPreds,
            mtcs_preds_lookup_constraint/*!lookup_constraint*/(mtcsPreds,constraints[i]))))
         record_address_regs(self,VOIDmode, ADDR_SPACE_GENERIC,
               mtcsRecog->recog_data.operand[i], 0, ADDRESS, SCRATCH, self->frequency * 2);
   }

   /* Check for commutative in a separate loop so everything will have
   been initialized.  We must do this even if one operand is a
   constant--see addsi3 in m68k.md.  */
   for (i = 0; i < (int) mtcsRecog->recog_data.n_operands - 1; i++)
      if (constraints[i][0] == '%'){
         const char *xconstraints[mtcs_recog_get_max_recog_operands/*!MAX_RECOG_OPERANDS*/(mtcsRecog)];
         int j;

         /* Handle commutative operands by swapping the
         constraints.  We assume the modes are the same.  */
         for (j = 0; j < mtcsRecog->recog_data.n_operands; j++)
            xconstraints[j] = constraints[j];

         xconstraints[i] = constraints[i+1];
         xconstraints[i+1] = constraints[i];
         record_reg_classes(self,mtcsRecog->recog_data.n_alternatives, mtcsRecog->recog_data.n_operands,
         mtcsRecog->recog_data.operand, modes, xconstraints, insn, pref);
      }
   record_reg_classes(self,mtcsRecog->recog_data.n_alternatives, mtcsRecog->recog_data.n_operands,
   mtcsRecog->recog_data.operand, modes, constraints, insn, pref);
}



/* Process one insn INSN.  Scan it and record each time it would save
   code to put a certain allocnos in a certain class.  Return the last
   insn processed, so that the scan can be continued from there.  */
static rtx_insn *scan_one_insn (MtcsIraCosts *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   enum rtx_code pat_code;
   rtx set, note;
   int i, k;
   bool counted_mem;

   if (!NONDEBUG_INSN_P (insn))
      return insn;

   pat_code = GET_CODE (PATTERN (insn));
   if (pat_code == ASM_INPUT)
      return insn;

   /* If INSN is a USE/CLOBBER of a pseudo in a mode M then go ahead
   and initialize the register move costs of mode M.

   The pseudo may be related to another pseudo via a copy (implicit or
   explicit) and if there are no mode M uses/sets of the original
   pseudo, then we may leave the register move costs uninitialized for
   mode M. */
   if (pat_code == USE || pat_code == CLOBBER){
      rtx x = XEXP (PATTERN (insn), 0);
      if (GET_CODE (x) == REG
      && REGNO (x) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/
      && mtcsReg->hardRegs.x_have_regs_of_mode/*!have_regs_of_mode*/[GET_MODE (x)])
         mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,GET_MODE (x));
      return insn;
   }

   counted_mem = false;
   set = single_set (insn);
   mtcs_recog_extract_insn/*!extract_insn*/(mtcsRecog,insn);

   /* If this insn loads a parameter from its stack slot, then it
   represents a savings, rather than a cost, if the parameter is
   stored in memory.  Record this fact.

   Similarly if we're loading other constants from memory (constant
   pool, TOC references, small data areas, etc) and this is the only
   assignment to the destination pseudo.

   Don't do this if SET_SRC (set) isn't a general operand, if it is
   a memory requiring special instructions to load it, decreasing
   mem_cost might result in it being loaded using the specialized
   instruction into a register, then stored into stack and loaded
   again from the stack.  See PR52208.

   Don't do this if SET_SRC (set) has side effect.  See PR56124.  */
   if (set != 0 && REG_P (SET_DEST (set)) && MEM_P (SET_SRC (set))
   && (note = find_reg_note (insn, REG_EQUIV, NULL_RTX)) != NULL_RTX
   && ((MEM_P (XEXP (note, 0))
   && !side_effects_p (SET_SRC (set)))
   || (CONSTANT_P (XEXP (note, 0))
   && mtcsTarget/*!targetm.legitimate_constant_p*/->legitimate_constant_p(mtcsTarget,GET_MODE (SET_DEST (set)),
   XEXP (note, 0))
   && REG_N_SETS (REGNO (SET_DEST (set))) == 1))
   && mtcs_preds_general_operand/*!general_operand*/(mtcsPreds,SET_SRC (set), GET_MODE (SET_SRC (set)))
   /* LRA does not use equiv with a symbol for PIC code.  */
   && (! mtcsIra->ira_use_lra_p || ! mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(mtcsRTL)
   || ! contains_symbol_ref_p (XEXP (note, 0)))){
      enum reg_class cl = mtcs_reg_get_general_regs/*!GENERAL_REGS*/(mtcsReg);
      rtx reg = SET_DEST (set);
      int num = COST_INDEX (REGNO (reg));
      /* Costs for NO_REGS are used in cost calculation on the
      1st pass when the preferred register classes are not
      known yet.  In this case we take the best scenario when
      mode can't be put into GENERAL_REGS.  */
      if (!mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,
            mtcsIra->x_ira_class_hard_regs[cl][0],GET_MODE (reg)))
         cl = NO_REGS;

      COSTS (self->costs, num)->mem_cost -= mtcsIra->x_ira_memory_move_cost[GET_MODE (reg)][cl][1] * self->frequency;
      record_address_regs(self,GET_MODE (SET_SRC (set)), MEM_ADDR_SPACE (SET_SRC (set)),
            XEXP (SET_SRC (set), 0), 0, MEM, SCRATCH,  self->frequency * 2);
      counted_mem = true;
   }

   record_operand_costs(self,insn, self->pref);

   if (mtcsIraGlobal->ira_dump_file != NULL && mtcsIraGlobal->internal_flag_ira_verbose > 5){
      const char *p;
      fprintf (mtcsIraGlobal->ira_dump_file, "    Final costs after insn %u", INSN_UID (insn));
      if (INSN_CODE (insn) >= 0
      && (p = mtcs_output_get_insn_name/*!get_insn_name*/(mtcsOutput,INSN_CODE (insn))) != NULL)
         fprintf (mtcsIraGlobal->ira_dump_file, " {%s}", p);
      fprintf (mtcsIraGlobal->ira_dump_file, " (freq=%d)\n",REG_FREQ_FROM_BB (BLOCK_FOR_INSN (insn)));
      dump_insn_slim (mtcsIraGlobal->ira_dump_file, insn);
   }

   MtcsRegClass *mtcsRegClass =mtcs_reg_get_reg_class(mtcsReg);

   /* Now add the cost for each operand to the total costs for its
   allocno.  */
   for (i = 0; i < mtcsRecog->recog_data.n_operands; i++){
      rtx op = mtcsRecog->recog_data.operand[i];

      if (GET_CODE (op) == SUBREG)
         op = SUBREG_REG (op);
      if (REG_P (op) && REGNO (op) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/){
         int regno = REGNO (op);
         IraCosts *p = COSTS (self->costs, COST_INDEX (regno));
         IraCosts *q = mtcsIraInt->x_op_costs[i];
         int *p_costs = p->cost, *q_costs = q->cost;
         CostClasses * cost_classes_ptr = self->regno_cost_classes[regno];
         int add_cost = 0;

         /* If the already accounted for the memory "cost" above, don't
         do so again.  */
         if (!counted_mem){
            add_cost = q->mem_cost;
            if (add_cost > 0 && INT_MAX - add_cost < p->mem_cost)
               p->mem_cost = INT_MAX;
            else
               p->mem_cost += add_cost;
         }
         if (mtcsIraGlobal->ira_dump_file != NULL && mtcsIraGlobal->internal_flag_ira_verbose > 5){
            fprintf (mtcsIraGlobal->ira_dump_file, "        op %d(r=%u) MEM:%d(+%d)",  i, REGNO(op), p->mem_cost, add_cost);
         }
         for (k = cost_classes_ptr->num - 1; k >= 0; k--){
            add_cost = q_costs[k];
            if (add_cost > 0 && INT_MAX - add_cost < p_costs[k])
               p_costs[k] = INT_MAX;
            else
               p_costs[k] += add_cost;
            if (mtcsIraGlobal->ira_dump_file != NULL && mtcsIraGlobal->internal_flag_ira_verbose > 5){
               fprintf (mtcsIraGlobal->ira_dump_file, " %s:%d(+%d)",
               mtcsRegClass/*!reg_class_names*/[cost_classes_ptr->classes[k]].name,p_costs[k], add_cost);
            }
         }
         if (mtcsIraGlobal->ira_dump_file != NULL && mtcsIraGlobal->internal_flag_ira_verbose > 5)
            fprintf (mtcsIraGlobal->ira_dump_file, "\n");
      }
   }
   return insn;
}



/* Print allocnos costs to the dump file.  */
static void print_allocno_costs (MtcsIraCosts *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int k;
   MtcsIraAllocno * a;
   MtcsIraAllocnoIterator ai;

   MtcsRegClass *mtcsRegClass =mtcs_reg_get_reg_class(mtcsReg);

   ira_assert (self->allocno_p);
   fprintf (mtcsIraGlobal->ira_dump_file, "\n");
   MTCS_FOR_EACH_ALLOCNO (mtcsIraBuild,a, ai){
      int i, rclass;
      basic_block bb;
      int regno = a->regno;
      CostClasses * cost_classes_ptr = self->regno_cost_classes[regno];
      enum reg_class *cost_classes = cost_classes_ptr->classes;

      i = a->num;
      fprintf (mtcsIraGlobal->ira_dump_file, "  a%d(r%d,", i, regno);
      if ((bb = a->loop_tree_node->bb) != NULL)
         fprintf (mtcsIraGlobal->ira_dump_file, "b%d", bb->index);
      else
         fprintf (mtcsIraGlobal->ira_dump_file, "l%d", a->loop_tree_node->loop_num);
      fprintf (mtcsIraGlobal->ira_dump_file, ") costs:");
      for (k = 0; k < cost_classes_ptr->num; k++){
         rclass = cost_classes[k];
         fprintf (mtcsIraGlobal->ira_dump_file, " %s:%d", mtcsRegClass/*!reg_class_names*/[rclass].name,
               COSTS (self->costs, i)->cost[k]);
         if (mtcsOptionsItem->x_flag_ira_region == IRA_REGION_ALL || mtcsOptionsItem->x_flag_ira_region == IRA_REGION_MIXED)
            fprintf (mtcsIraGlobal->ira_dump_file, ",%d",COSTS (self->total_allocno_costs, i)->cost[k]);
      }
      fprintf (mtcsIraGlobal->ira_dump_file, " MEM:%i", COSTS (self->costs, i)->mem_cost);
      if (mtcsOptionsItem->x_flag_ira_region == IRA_REGION_ALL || mtcsOptionsItem->x_flag_ira_region == IRA_REGION_MIXED)
         fprintf (mtcsIraGlobal->ira_dump_file, ",%d",COSTS (self->total_allocno_costs, i)->mem_cost);
      fprintf (mtcsIraGlobal->ira_dump_file, "\n");
   }
}

/* Print pseudo costs to the dump file.  */
static void print_pseudo_costs (MtcsIraCosts *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   int regno, k;
   int rclass;
   CostClasses * cost_classes_ptr;
   enum reg_class *cost_classes;
   MtcsRegClass *mtcsRegClass =mtcs_reg_get_reg_class(mtcsReg);

   ira_assert (! self->allocno_p);
   fprintf (mtcsIraGlobal->ira_dump_file, "\n");
   for (regno = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc) - 1;
         regno >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; regno--){
      if (REG_N_REFS (regno) <= 0)
         continue;
      cost_classes_ptr = self->regno_cost_classes[regno];
      cost_classes = cost_classes_ptr->classes;
      fprintf (mtcsIraGlobal->ira_dump_file, "  r%d costs:", regno);
      for (k = 0; k < cost_classes_ptr->num; k++){
         rclass = cost_classes[k];
         fprintf (mtcsIraGlobal->ira_dump_file, " %s:%d", mtcsRegClass/*!reg_class_names*/[rclass].name,
               COSTS (self->costs, regno)->cost[k]);
      }
      fprintf (mtcsIraGlobal->ira_dump_file, " MEM:%i\n", COSTS (self->costs, regno)->mem_cost);
   }
}

/* Traverse the BB represented by LOOP_TREE_NODE to update the allocno
   costs.  */
static void process_bb_for_costs (MtcsIraCosts *self,basic_block bb)
{
   rtx_insn *insn;

   self->frequency = REG_FREQ_FROM_BB (bb);
   if (self->frequency == 0)
      self->frequency = 1;
   FOR_BB_INSNS (bb, insn)
      insn = scan_one_insn(self,insn);
}

/* Traverse the BB represented by LOOP_TREE_NODE to update the allocno
   costs.  */
static void processBBNodeForCosts_cb(MtcsIraLoopTreeNode * loop_tree_node,void *userData)
{
   MtcsIraCosts *self =(MtcsIraCosts *)userData;
   basic_block bb;
   bb = loop_tree_node->bb;
   if (bb != NULL)
      process_bb_for_costs(self,bb);
}

/* Return true if all autoinc rtx in X change only a register and memory is
   valid.  */
static bool validate_autoinc_and_mem_addr_p (MtcsIraCosts *self,rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   enum rtx_code code = GET_CODE (x);
   if (GET_RTX_CLASS (code) == RTX_AUTOINC)
      return REG_P (XEXP (x, 0));
   const char *fmt = GET_RTX_FORMAT (code);
   for (int i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
      if (fmt[i] == 'e'){
         if (!validate_autoinc_and_mem_addr_p(self,XEXP (x, i)))
            return false;
      }else if (fmt[i] == 'E'){
         for (int j = 0; j < XVECLEN (x, i); j++)
            if (!validate_autoinc_and_mem_addr_p(self,XVECEXP (x, i, j)))
               return false;
      }
   /* Check memory after checking autoinc to guarantee that autoinc is already
   valid for machine-dependent code checking memory address.  */
   return (!MEM_P (x)|| mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/(mtcsRecog,
         GET_MODE (x), XEXP (x, 0), mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,x)));
}

/* Check that reg REGNO can be changed by TO in INSN.  Return true in case the
   result insn would be valid one.  */
static bool equiv_can_be_consumed_p (MtcsIraCosts *self,int regno, rtx to, rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   mtcs_recog_validate_replace_src_group/*!validate_replace_src_group*/(mtcsRecog,
         mtcsRtlData->regno_reg_rtx/*! regno_reg_rtx*/[regno], to, insn);
   /* We can change register to equivalent memory in autoinc rtl.  Some code
   including verify_changes assumes that autoinc contains only a register.
   So check this first.  */
   bool res = validate_autoinc_and_mem_addr_p(self,PATTERN (insn));
   if (res)
      res = mtcs_recog_verify_changes/*!verify_changes*/(mtcsRecog,0);
   mtcs_recog_cancel_changes/*!cancel_changes*/(mtcsRecog,0);
   return res;
}

/* Return true if X contains a pseudo with equivalence.  In this case also
   return the pseudo through parameter REG.  If the pseudo is a part of subreg,
   return the subreg through parameter SUBREG.  */

static bool get_equiv_regno (MtcsIraCosts *self,rtx x, int &regno, rtx &subreg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);

   subreg = NULL_RTX;
   if (GET_CODE (x) == SUBREG){
      subreg = x;
      x = SUBREG_REG (x);
   }
   if (REG_P (x)
   && (mtcsIra->ira_reg_equiv[REGNO (x)].memory != NULL
   || mtcsIra->ira_reg_equiv[REGNO (x)].invariant != NULL
   || mtcsIra->ira_reg_equiv[REGNO (x)].constant != NULL)){
      regno = REGNO (x);
      return true;
   }
   RTX_CODE code = GET_CODE (x);
   const char *fmt = GET_RTX_FORMAT (code);

   for (int i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
      if (fmt[i] == 'e'){
         if (get_equiv_regno(self,XEXP (x, i), regno, subreg))
         return true;
      }else if (fmt[i] == 'E'){
         for (int j = 0; j < XVECLEN (x, i); j++)
            if (get_equiv_regno(self,XVECEXP (x, i, j), regno, subreg))
               return true;
      }
   return false;
}

/* A pass through the current function insns.  Calculate costs of using
   equivalences for pseudos and store them in regno_equiv_gains.  */

static void calculate_equiv_gains (MtcsIraCosts *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   basic_block bb;
   int regno, freq, cost;
   rtx subreg;
   rtx_insn *insn;
   machine_mode mode;
   enum reg_class rclass;
   bitmap_head equiv_pseudos;

   ira_assert (self->allocno_p);
   bitmap_initialize (&equiv_pseudos, &reg_obstack);
   for (regno = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc) - 1; regno >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; regno--)
      if (mtcsIra->ira_reg_equiv[regno].init_insns != NULL
      && (mtcsIra->ira_reg_equiv[regno].memory != NULL
      || mtcsIra->ira_reg_equiv[regno].invariant != NULL
      || (mtcsIra->ira_reg_equiv[regno].constant != NULL
      /* Ignore complicated constants which probably will be placed
      in memory:  */
      && GET_CODE (mtcsIra->ira_reg_equiv[regno].constant) != CONST_DOUBLE
      && GET_CODE (mtcsIra->ira_reg_equiv[regno].constant) != CONST_VECTOR
      && GET_CODE (mtcsIra->ira_reg_equiv[regno].constant) != LABEL_REF))){
         rtx_insn_list *x;
         for (x = mtcsIra->ira_reg_equiv[regno].init_insns; x != NULL; x = x->next ()){
            insn = x->insn ();
            rtx set = single_set (insn);

            if (set == NULL_RTX || SET_DEST (set) != mtcsRtlData->regno_reg_rtx/*! regno_reg_rtx*/[regno])
               break;
            bb = BLOCK_FOR_INSN (insn);
            mtcsIraBuild->ira_curr_regno_allocno_map  = mtcsIraBuild->ira_bb_nodes[bb->index]->parent->regno_allocno_map;
            mode = mtcs_rtl_data_get_pseudo_regno_mode/*!PSEUDO_REGNO_MODE*/(mtcsRtlData,regno);
            rclass = self->pref[COST_INDEX (regno)];
            mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,mode);
            if (mtcsIra->ira_reg_equiv[regno].memory != NULL)
               cost = mtcsIra->x_ira_memory_move_cost[mode][rclass][1];
            else
               cost = mtcsIraInt->x_ira_register_move_cost[mode][rclass][rclass];
            freq = REG_FREQ_FROM_BB (bb);
            self->regno_equiv_gains[regno] += cost * freq;
         }
         if (x != NULL)
            /* We found complicated equiv or reverse equiv mem=reg.  Ignore
            them.  */
            self->regno_equiv_gains[regno] = 0;
         else
            bitmap_set_bit (&equiv_pseudos, regno);
      }

   FOR_EACH_BB_FN (bb, cfun){
      freq = REG_FREQ_FROM_BB (bb);
      mtcsIraBuild->ira_curr_regno_allocno_map = mtcsIraBuild->ira_bb_nodes[bb->index]->parent->regno_allocno_map;
      FOR_BB_INSNS (bb, insn){
         if (!NONDEBUG_INSN_P (insn) || !get_equiv_regno(self,PATTERN (insn), regno, subreg) || !bitmap_bit_p (&equiv_pseudos, regno))
            continue;
         rtx subst = mtcsIra->ira_reg_equiv[regno].memory;

         if (subst == NULL)
            subst = mtcsIra->ira_reg_equiv[regno].constant;
         if (subst == NULL)
            subst = mtcsIra->ira_reg_equiv[regno].invariant;
         ira_assert (subst != NULL);
         mode = mtcs_rtl_data_get_pseudo_regno_mode/*!PSEUDO_REGNO_MODE*/(mtcsRtlData,regno);
         mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,mode);
         bool consumed_p = equiv_can_be_consumed_p(self,regno, subst, insn);

         rclass = self->pref[COST_INDEX (regno)];
         if (MEM_P (subst)
            /* If it is a change of constant into double for example, the
            result constant probably will be placed in memory.  */
            || (subreg != NULL_RTX && !INTEGRAL_MODE_P (GET_MODE (subreg))))
            cost = mtcsIra->x_ira_memory_move_cost[mode][rclass][1] + (consumed_p ? 0 : 1);
         else if (consumed_p)
            continue;
         else
            cost = mtcsIraInt->x_ira_register_move_cost[mode][rclass][rclass];
         self->regno_equiv_gains[regno] -= cost * freq;
      }
   }
   bitmap_clear (&equiv_pseudos);
}

/* Find costs of register classes and memory for allocnos or pseudos
   and their best costs.  Set up preferred, alternative and allocno
   classes for pseudos.  */
static void find_costs_and_classes (MtcsIraCosts *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   int maxRegNum = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);

   int i, k, start, max_cost_classes_num;
   int pass;
   basic_block bb;
   enum reg_class *regno_best_class, new_class;

   init_recog ();
   regno_best_class = (enum reg_class *) ira_allocate (maxRegNum/*!max_reg_num*/ * sizeof (enum reg_class));
   for (i = maxRegNum/*!max_reg_num*/  - 1; i >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; i--)
      regno_best_class[i] = NO_REGS;
   if (!resize_reg_info () && self->allocno_p && self->pseudo_classes_defined_p && mtcsOptionsItem->x_flag_expensive_optimizations){
      MtcsIraAllocno * a;
      MtcsIraAllocnoIterator ai;

      self->pref = self->pref_buffer;
      max_cost_classes_num = 1;
      MTCS_FOR_EACH_ALLOCNO (mtcsIraBuild,a, ai){
         self->pref[a->num] = reg_preferred_class (a->regno);
         setup_regno_cost_classes_by_aclass(self,a->regno, self->pref[a->num]);
         max_cost_classes_num = MAX (max_cost_classes_num, self->regno_cost_classes[a->regno]->num);
      }
      start = 1;
   }else{
      self->pref = NULL;
      max_cost_classes_num = mtcsIraInt->x_ira_important_classes_num;
      for (i = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc) - 1; i >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; i--)
         if (mtcsRtlData->regno_reg_rtx/*! regno_reg_rtx*/[i] != NULL_RTX)
            setup_regno_cost_classes_by_mode(self,i, mtcs_rtl_data_get_pseudo_regno_mode/*!PSEUDO_REGNO_MODE*/(mtcsRtlData,i));
         else
            setup_regno_cost_classes_by_aclass(self,i, mtcs_reg_get_all_regs/*!ALL_REGS*/(mtcsReg));
      start = 0;
   }

   if (self->allocno_p)
      /* Clear the flag for the next compiled function.  */
      self->pseudo_classes_defined_p = false;
   /* Normally we scan the insns once and determine the best class to
   use for each allocno.  However, if -fexpensive-optimizations are
   on, we do so twice, the second time using the tentative best
   classes to guide the selection.  */
   for (pass = start; pass <= mtcsOptionsItem->x_flag_expensive_optimizations; pass++){
      if ((!self->allocno_p || mtcsIraGlobal->internal_flag_ira_verbose > 0) && mtcsIraGlobal->ira_dump_file)
         fprintf (mtcsIraGlobal->ira_dump_file, "\nPass %i for finding pseudo/allocno costs\n\n", pass);

      if (pass != start){
         max_cost_classes_num = 1;
         for (i = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc) - 1; i >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; i--){
            setup_regno_cost_classes_by_aclass(self,i, regno_best_class[i]);
            max_cost_classes_num = MAX (max_cost_classes_num, self->regno_cost_classes[i]->num);
         }
      }

      self->struct_costs_size = sizeof (IraCosts) + sizeof (int) * (max_cost_classes_num - 1);
      /* Zero out our accumulation of the cost of each class for each
      allocno.  */
      memset (self->costs, 0, self->cost_elements_num * self->struct_costs_size);

      if (self->allocno_p){
         /* Scan the instructions and record each time it would save code
         to put a certain allocno in a certain class.  */
         mtcs_ira_build_traverse_loop_tree/*!ira_traverse_loop_tree*/(mtcsIraBuild,
               true,  mtcsIraBuild->ira_loop_tree_root,processBBNodeForCosts_cb, NULL,(void *)self);


         memcpy (self->total_allocno_costs, self->costs,
         mtcsIraInt->x_max_struct_costs_size * ira_allocnos_num);
      }else{
         basic_block bb;

         FOR_EACH_BB_FN (bb, cfun)
            process_bb_for_costs(self,bb);
      }

      if (pass == 0)
         self->pref = self->pref_buffer;

      if (mtcsIra->ira_use_lra_p && self->allocno_p && pass == 1)
         /* It is a pass through all insns.  So do it once and only for RA (not
         for insn scheduler) when we already found preferable pseudo register
         classes on the previous pass.  */
         calculate_equiv_gains(self);

      /* Now for each allocno look at how desirable each class is and
      find which class is preferred.  */
      for (i = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc) - 1; i >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; i--){
         MtcsIraAllocno * a, *parent_a;
         int rclass, a_num, parent_a_num, add_cost;
         MtcsIraLoopTreeNode * parent;
         int best_cost, allocno_cost;
         enum reg_class best, alt_class;
         CostClasses * cost_classes_ptr = self->regno_cost_classes[i];
         enum reg_class *cost_classes;
         int *i_costs = mtcsIraInt->x_temp_costs->cost;
         int i_mem_cost;
         int equiv_savings = self->regno_equiv_gains[i];

         if (! self->allocno_p){
            if (mtcsRtlData->regno_reg_rtx/*! regno_reg_rtx*/[i] == NULL_RTX)
               continue;
            memcpy (mtcsIraInt->x_temp_costs, COSTS (self->costs, i), self->struct_costs_size);
            i_mem_cost = mtcsIraInt->x_temp_costs->mem_cost;
            cost_classes = cost_classes_ptr->classes;
         }else{
            if (mtcsIraBuild->ira_regno_allocno_map[i] == NULL)
               continue;
            memset (mtcsIraInt->x_temp_costs, 0, self->struct_costs_size);
            i_mem_cost = 0;
            cost_classes = cost_classes_ptr->classes;
            /* Find cost of all allocnos with the same regno.  */
            for (a = mtcsIraBuild->ira_regno_allocno_map[i]; a != NULL; a = a->next_regno_allocno){
               int *a_costs, *p_costs;

               a_num = a->num;
               if ((mtcsOptionsItem->x_flag_ira_region == IRA_REGION_ALL
               || mtcsOptionsItem->x_flag_ira_region == IRA_REGION_MIXED)
               && (parent = a->loop_tree_node->parent) != NULL
               && (parent_a = parent->regno_allocno_map[i]) != NULL
               /* There are no caps yet.  */
               && bitmap_bit_p (ALLOCNO_LOOP_TREE_NODE (a)->border_allocnos, a->num)){
                  /* Propagate costs to upper levels in the region
                  tree.  */
                  parent_a_num = ALLOCNO_NUM (parent_a);
                  a_costs = COSTS (self->total_allocno_costs, a_num)->cost;
                  p_costs = COSTS (self->total_allocno_costs, parent_a_num)->cost;
                  for (k = cost_classes_ptr->num - 1; k >= 0; k--){
                     add_cost = a_costs[k];
                     if (add_cost > 0 && INT_MAX - add_cost < p_costs[k])
                        p_costs[k] = INT_MAX;
                     else
                        p_costs[k] += add_cost;
                  }
                  add_cost = COSTS (self->total_allocno_costs, a_num)->mem_cost;
                  if (add_cost > 0
                  && (INT_MAX - add_cost < COSTS (self->total_allocno_costs, parent_a_num)->mem_cost))
                     COSTS (self->total_allocno_costs, parent_a_num)->mem_cost = INT_MAX;
                  else
                     COSTS (self->total_allocno_costs, parent_a_num)->mem_cost += add_cost;

                  if (i >= first_moveable_pseudo && i < last_moveable_pseudo)
                     COSTS (self->total_allocno_costs, parent_a_num)->mem_cost = 0;
               }
               a_costs = COSTS (self->costs, a_num)->cost;
               for (k = cost_classes_ptr->num - 1; k >= 0; k--){
                  add_cost = a_costs[k];
                  if (add_cost > 0 && INT_MAX - add_cost < i_costs[k])
                     i_costs[k] = INT_MAX;
                  else
                     i_costs[k] += add_cost;
               }
               add_cost = COSTS (self->costs, a_num)->mem_cost;
               if (add_cost > 0 && INT_MAX - add_cost < i_mem_cost)
                  i_mem_cost = INT_MAX;
               else
                  i_mem_cost += add_cost;
            }
         }
         if (i >= first_moveable_pseudo && i < last_moveable_pseudo)
            i_mem_cost = 0;
         else if (mtcsIra->ira_use_lra_p){
            if (equiv_savings > 0){
               i_mem_cost = 0;
               if (mtcsIraGlobal->ira_dump_file != NULL && mtcsIraGlobal->internal_flag_ira_verbose > 5)
                  fprintf (mtcsIraGlobal->ira_dump_file, "   Use MEM for r%d as the equiv savings is %d\n",i, equiv_savings);
            }
         }else if (equiv_savings < 0)
            i_mem_cost = -equiv_savings;
         else if (equiv_savings > 0){
            i_mem_cost = 0;
            for (k = cost_classes_ptr->num - 1; k >= 0; k--)
               i_costs[k] += equiv_savings;
         }

         best_cost = (1 << (HOST_BITS_PER_INT - 2)) - 1;
         best = mtcs_reg_get_all_regs/*!ALL_REGS*/(mtcsReg);
         alt_class = NO_REGS;
         /* Find best common class for all allocnos with the same
         regno.  */
         for (k = 0; k < cost_classes_ptr->num; k++){
            rclass = cost_classes[k];
            if (i_costs[k] < best_cost){
               best_cost = i_costs[k];
               best = (enum reg_class) rclass;
            }else if (i_costs[k] == best_cost)
               best = mtcsIraInt->x_ira_reg_class_subunion[best][rclass];
            if (pass == mtcsOptionsItem->x_flag_expensive_optimizations
            /* We still prefer registers to memory even at this
            stage if their costs are the same.  We will make
            a final decision during assigning hard registers
            when we have all info including more accurate
            costs which might be affected by assigning hard
            registers to other pseudos because the pseudos
            involved in moves can be coalesced.  */
            && i_costs[k] <= i_mem_cost
            && (mtcsReg->hardRegs.x_reg_class_size/*!reg_class_size*/
                  [mtcsReg->hardRegs.x_reg_class_subunion/*!reg_class_subunion*/[alt_class][rclass]] >
            mtcsReg->hardRegs.x_reg_class_size/*!reg_class_size*/[alt_class]))
               alt_class = mtcsReg->hardRegs.x_reg_class_subunion/*!reg_class_subunion*/[alt_class][rclass];
         }
         alt_class = mtcsIra->x_ira_allocno_class_translate[alt_class];
         if (best_cost > i_mem_cost
         && ! mtcs_ira_non_spilled_static_chain_regno_p/*!non_spilled_static_chain_regno_p*/(mtcsIra,i))
            self->regno_aclass[i] = NO_REGS;
         else if (!mtcsOptionsItem->x_optimize && !targetm.class_likely_spilled_p (best))
            /* Registers in the alternative class are likely to need
            longer or slower sequences than registers in the best class.
            When optimizing we make some effort to use the best class
            over the alternative class where possible, but at -O0 we
            effectively give the alternative class equal weight.
            We then run the risk of using slower alternative registers
            when plenty of registers from the best class are still free.
            This is especially true because live ranges tend to be very
            short in -O0 code and so register pressure tends to be low.

            Avoid that by ignoring the alternative class if the best
            class has plenty of registers.

            The union class arrays give important classes and only
            part of it are allocno classes.  So translate them into
            allocno classes.  */
            self->regno_aclass[i] = mtcsIra->x_ira_allocno_class_translate[best];
         else{
            /* Make the common class the biggest class of best and
            alt_class.  Translate the common class into an
            allocno class too.  */
            self->regno_aclass[i] = (mtcsIra->x_ira_allocno_class_translate[mtcsIraInt->x_ira_reg_class_superunion[best][alt_class]]);
            ira_assert (self->regno_aclass[i] != NO_REGS  && mtcsIraInt->x_ira_reg_allocno_class_p[self->regno_aclass[i]]);
         }
         if (mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(mtcsRTL) != NULL
         && i == (int) REGNO (mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(mtcsRTL))){
            /* For some targets, integer pseudos can be assigned to fp
            regs.  As we don't want reload pic offset table pseudo, we
            should avoid using non-integer regs.  */
            self->regno_aclass[i] = mtcsIraInt->x_ira_reg_class_intersect[self->regno_aclass[i]]
                                                                          [mtcs_reg_get_general_regs/*!GENERAL_REGS*/(mtcsReg)];
            alt_class = mtcsIraInt->x_ira_reg_class_intersect[alt_class][mtcs_reg_get_general_regs/*!GENERAL_REGS*/(mtcsReg)];
         }
         if ((new_class= (reg_class) (mtcsTarget/*!targetm.ira_change_pseudo_allocno_class*/->ira_change_pseudo_allocno_class(mtcsTarget,
               i, self->regno_aclass[i], best))) != self->regno_aclass[i]){
            self->regno_aclass[i] = new_class;
            if (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[new_class],
            &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[best]))
               best = new_class;
            if (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[new_class],
            &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[alt_class]))
               alt_class = new_class;
         }
         MtcsRegClass *mtcsRegClass =mtcs_reg_get_reg_class(mtcsReg);

         if (pass == mtcsOptionsItem->x_flag_expensive_optimizations){
            if (best_cost > i_mem_cost
            /* Do not assign NO_REGS to static chain pointer
            pseudo when non-local goto is used.  */
            && ! mtcs_ira_non_spilled_static_chain_regno_p/*!non_spilled_static_chain_regno_p*/(mtcsIra,i))
               best = alt_class = NO_REGS;
            else if (best == alt_class)
               alt_class = NO_REGS;
            mtcs_reg_setup_reg_classes/*!setup_reg_classes*/(mtcsReg,i, best, alt_class, self->regno_aclass[i]);
            if ((!self->allocno_p || mtcsIraGlobal->internal_flag_ira_verbose > 2)  && mtcsIraGlobal->ira_dump_file != NULL)
               fprintf (mtcsIraGlobal->ira_dump_file,"    r%d: preferred %s, alternative %s, allocno %s\n",
                     i, mtcsRegClass/*!reg_class_names*/[best].name, mtcsRegClass/*!reg_class_names*/[alt_class].name,
                     mtcsRegClass/*!reg_class_names*/[self->regno_aclass[i]].name);
         }
         regno_best_class[i] = best;
         if (! self->allocno_p){
            self->pref[i] = (best_cost > i_mem_cost
                  && ! mtcs_ira_non_spilled_static_chain_regno_p/*!non_spilled_static_chain_regno_p*/(mtcsIra,i) ? NO_REGS : best);
            continue;
         }
         for (a = mtcsIraBuild->ira_regno_allocno_map[i];  a != NULL; a = a->next_regno_allocno){
            enum reg_class aclass = self->regno_aclass[i];
            int a_num = a->num;
            int *total_a_costs = COSTS (self->total_allocno_costs, a_num)->cost;
            int *a_costs = COSTS (self->costs, a_num)->cost;

            if (aclass == NO_REGS)
               best = NO_REGS;
            else{
               /* Finding best class which is subset of the common
               class.  */
               best_cost = (1 << (HOST_BITS_PER_INT - 2)) - 1;
               allocno_cost = best_cost;
               best = mtcs_reg_get_all_regs/*!ALL_REGS*/(mtcsReg);
               for (k = 0; k < cost_classes_ptr->num; k++){
                  rclass = cost_classes[k];
                  if (! ira_class_subset_p[rclass][aclass])
                     continue;
                  if (total_a_costs[k] < best_cost){
                     best_cost = total_a_costs[k];
                     allocno_cost = a_costs[k];
                     best = (enum reg_class) rclass;
                  }else if (total_a_costs[k] == best_cost){
                     best = mtcsIraInt->x_ira_reg_class_subunion[best][rclass];
                     allocno_cost = MAX (allocno_cost, a_costs[k]);
                  }
               }
               a->class_cost = allocno_cost;
            }
            if (mtcsIraGlobal->internal_flag_ira_verbose > 2 && mtcsIraGlobal->ira_dump_file != NULL
            && (pass == 0 || self->pref[a_num] != best)){
               MtcsRegClass *mtcsRegClass =mtcs_reg_get_reg_class(mtcsReg);

               fprintf (mtcsIraGlobal->ira_dump_file, "    a%d (r%d,", a_num, i);
               if ((bb = a->loop_tree_node->bb) != NULL)
                  fprintf (mtcsIraGlobal->ira_dump_file, "b%d", bb->index);
               else
                  fprintf (mtcsIraGlobal->ira_dump_file, "l%d", a->loop_tree_node->loop_num);
               fprintf (mtcsIraGlobal->ira_dump_file, ") best %s, allocno %s\n",
                     mtcsRegClass/*!reg_class_names*/[best].name, mtcsRegClass/*!reg_class_names*/[aclass].name);
            }
            self->pref[a_num] = best;
            if (pass == mtcsOptionsItem->x_flag_expensive_optimizations && best != aclass
            && mtcsIra->x_ira_class_hard_regs_num[best] > 0
            && (mtcsIra->x_ira_reg_class_max_nregs[best][a->mode] >= mtcsIra->x_ira_class_hard_regs_num[best])){
               int ind = cost_classes_ptr->index[aclass];

               ira_assert (ind >= 0);
               mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,a->mode);
               mcs_ira_build_add_allocno_pref/*!ira_add_allocno_pref*/(mtcsIraBuild,
                     a, mtcsIra->x_ira_class_hard_regs[best][0],(a_costs[ind] - a->class_cost) /
                     (mtcsIraInt->x_ira_register_move_cost[a->mode][best][aclass]));
               for (k = 0; k < cost_classes_ptr->num; k++)
                  if (ira_class_subset_p[cost_classes[k]][best])
                     a_costs[k] = a_costs[ind];
            }
         }
      }

      if (mtcsIraGlobal->internal_flag_ira_verbose > 4 && mtcsIraGlobal->ira_dump_file){
         if (self->allocno_p)
            print_allocno_costs(self);
         else
            print_pseudo_costs(self);
         fprintf (mtcsIraGlobal->ira_dump_file,"\n");
      }
   }
   ira_free (regno_best_class);
}



/* Process moves involving hard regs to modify allocno hard register
   costs.  We can do this only after determining allocno class.  If a
   hard register forms a register class, then moves with the hard
   register are already taken into account in class costs for the
   allocno.  */
static void processBBNodeForHardRegMoves_cb(MtcsIraLoopTreeNode * loop_tree_node,void *userData)
{
   MtcsIraCosts *self=(MtcsIraCosts *)userData;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   int i, freq, src_regno, dst_regno, hard_regno, a_regno;
   bool to_p;
   MtcsIraAllocno * a, *curr_a;
   MtcsIraLoopTreeNode * curr_loop_tree_node;
   enum reg_class rclass;
   basic_block bb;
   rtx_insn *insn;
   rtx set, src, dst;

   bb = loop_tree_node->bb;
   if (bb == NULL)
      return;
   freq = REG_FREQ_FROM_BB (bb);
   if (freq == 0)
      freq = 1;

   FOR_BB_INSNS (bb, insn){
      if (!NONDEBUG_INSN_P (insn))
         continue;
      set = single_set (insn);
      if (set == NULL_RTX)
         continue;
      dst = SET_DEST (set);
      src = SET_SRC (set);
      if (! REG_P (dst) || ! REG_P (src))
         continue;
      dst_regno = REGNO (dst);
      src_regno = REGNO (src);
      if (dst_regno >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/
      && src_regno < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/){
         hard_regno = src_regno;
         a = mtcsIraBuild->ira_curr_regno_allocno_map[dst_regno];
         to_p = true;
      }else if (src_regno >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/
      && dst_regno < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/){
         hard_regno = dst_regno;
         a = mtcsIraBuild->ira_curr_regno_allocno_map[src_regno];
         to_p = false;
      }else
         continue;

      if (mtcsReg->hardRegs.x_reg_class_size/*!reg_class_size*/[(int) mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,hard_regno)]
      == (mtcsIra->x_ira_reg_class_max_nregs[mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,hard_regno)][(int) a->mode]))
         /* If the class can provide only one hard reg to the allocno,
         we processed the insn record_operand_costs already and we
         actually updated the hard reg cost there.  */
         continue;
      rclass = a->aclass;
      if (! mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(
            &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[rclass], hard_regno))
         continue;
      i = mtcsIraInt->x_ira_class_hard_reg_index[rclass][hard_regno];
      if (i < 0)
         continue;
      a_regno = a->regno;
      for (curr_loop_tree_node = a->loop_tree_node; curr_loop_tree_node != NULL;curr_loop_tree_node = curr_loop_tree_node->parent)
         if ((curr_a = curr_loop_tree_node->regno_allocno_map[a_regno]) != NULL)
            mcs_ira_build_add_allocno_pref/*!ira_add_allocno_pref*/(mtcsIraBuild,curr_a, hard_regno, freq);

      {
         int cost;
         enum reg_class hard_reg_class;
         machine_mode mode;

         mode = a->mode;
         hard_reg_class = mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,hard_regno);
         mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,mode);
         cost = (to_p ? mtcsIraInt->x_ira_register_move_cost[mode][hard_reg_class][rclass]
                             : mtcsIraInt->x_ira_register_move_cost[mode][rclass][hard_reg_class]) * freq;
         mtcs_ira_build_allocate_and_set_costs/*!ira_allocate_and_set_costs*/(mtcsIraBuild,&a->hard_reg_costs, rclass,a->class_cost);
         mtcs_ira_build_allocate_and_set_costs/*!ira_allocate_and_set_costs*/(mtcsIraBuild,&a->conflict_hard_reg_costs,rclass, 0);
         a->hard_reg_costs[i] -= cost;
         a->conflict_hard_reg_costs[i] -= cost;
         a->class_cost = MIN (a->class_cost,
         a->hard_reg_costs[i]);
      }
   }
}

/* After we find hard register and memory costs for allocnos, define
   its class and modify hard register cost because insns moving
   allocno to/from hard registers.  */
static void setup_allocno_class_and_costs (MtcsIraCosts *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);

   int i, j, n, regno, hard_regno, num;
   int *reg_costs;
   enum reg_class aclass, rclass;
   MtcsIraAllocno * a;
   MtcsIraAllocnoIterator ai;
   CostClasses * cost_classes_ptr;

   ira_assert (self->allocno_p);
   MTCS_FOR_EACH_ALLOCNO(mtcsIraBuild,a, ai){
      i = a->num;
      regno = a->regno;
      aclass = self->regno_aclass[regno];
      cost_classes_ptr = self->regno_cost_classes[regno];
      ira_assert (self->pref[i] == NO_REGS || aclass != NO_REGS);
      a->memory_cost = COSTS (self->costs, i)->mem_cost;
      mtcs_ira_build_set_allocno_class/*!ira_set_allocno_class*/(mtcsIraBuild,a, aclass);
      if (aclass == NO_REGS)
         continue;
      if (mtcsOptionsItem->x_optimize && a->aclass != self->pref[i]){
         n = mtcsIra->x_ira_class_hard_regs_num[aclass];
         a->hard_reg_costs = reg_costs = ira_allocate_cost_vector (aclass);
         for (j = n - 1; j >= 0; j--){
            hard_regno = mtcsIra->x_ira_class_hard_regs[aclass][j];
            if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(
            &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[self->pref[i]], hard_regno))
               reg_costs[j] = a->class_cost;
            else{
               rclass = mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,hard_regno);
               num = cost_classes_ptr->index[rclass];
               if (num < 0){
                  num = cost_classes_ptr->hard_regno_index[hard_regno];
                  ira_assert (num >= 0);
               }
               reg_costs[j] = COSTS (self->costs, i)->cost[num];
            }
         }
      }
   }

   if (mtcsOptionsItem->x_optimize)
      mtcs_ira_build_traverse_loop_tree/*!ira_traverse_loop_tree*/(mtcsIraBuild,true, mtcsIraBuild->ira_loop_tree_root,
                  processBBNodeForHardRegMoves_cb, NULL,(void*)self);
}



/* Function called once during compiler work.  */
//原型 ira_init_costs_once ira-int.h ira-costs.cc
void mtcs_ira_costs_init_costs_once (MtcsIraCosts *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int i;
   //MAX_RECOG_OPERANDS host nvptx都是30
   mtcsIraInt->x_init_cost = NULL;
   for (i = 0; i < mtcs_recog_get_max_recog_operands/*!MAX_RECOG_OPERANDS*/(mtcsRecog); i++){
      mtcsIraInt->x_op_costs[i] = NULL;
      mtcsIraInt->x_this_op_costs[i] = NULL;
   }
   mtcsIraInt->x_temp_costs = NULL;
}


/* This is called each time register related information is
   changed.  */
//原型 ira_init_costs ira-int.h ira-costs.cc
void mtcs_ira_costs_init_costs (MtcsIraCosts *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   int i;

   mtcs_ira_int_free_ira_costs/*!this_target_ira_int->free_ira_costs*/(mtcsIraInt);
   mtcsIraInt->x_max_struct_costs_size
   = sizeof (IraCosts) + sizeof (int) * (mtcsIraInt->x_ira_important_classes_num - 1);
   /* Don't use ira_allocate because vectors live through several IRA
   calls.  */
   mtcsIraInt->x_init_cost = (IraCosts *) xmalloc (mtcsIraInt->x_max_struct_costs_size);
   mtcsIraInt->x_init_cost->mem_cost = 1000000;
   for (i = 0; i < mtcsIraInt->x_ira_important_classes_num; i++)
      mtcsIraInt->x_init_cost->cost[i] = 1000000;
   for (i = 0; i < mtcs_recog_get_max_recog_operands/*!MAX_RECOG_OPERANDS*/(mtcsRecog); i++){
      mtcsIraInt->x_op_costs[i] = (IraCosts *) xmalloc (mtcsIraInt->x_max_struct_costs_size);
      mtcsIraInt->x_this_op_costs[i] = (IraCosts *) xmalloc (mtcsIraInt->x_max_struct_costs_size);
   }
   mtcsIraInt->x_temp_costs = (IraCosts *) xmalloc (mtcsIraInt->x_max_struct_costs_size);
}



/* Common initialization function for ira_costs and
   ira_set_pseudo_classes.  */
static void init_costs (MtcsIraCosts *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   mtcs_reg_init_subregs_of_mode/*!init_subregs_of_mode*/(mtcsReg);
   self->costs = (IraCosts *) ira_allocate (mtcsIraInt->x_max_struct_costs_size * self->cost_elements_num);
   self->pref_buffer = (enum reg_class *) ira_allocate (sizeof (enum reg_class) * self->cost_elements_num);
   self->regno_aclass = (enum reg_class *) ira_allocate (sizeof (enum reg_class)* mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc));
   self->regno_equiv_gains = (int *) ira_allocate (sizeof (int) * mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc));
   memset (self->regno_equiv_gains, 0, sizeof (int) * mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc));
}

/* Common finalization function for ira_costs and
   ira_set_pseudo_classes.  */
static void finish_costs (MtcsIraCosts *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   mtcs_reg_finish_subregs_of_mode/*!finish_subregs_of_mode*/(mtcsReg);
   ira_free (self->regno_equiv_gains);
   ira_free (self->regno_aclass);
   ira_free (self->pref_buffer);
   ira_free (self->costs);
}

/* Entry function which defines register class, memory and hard
   register costs for each allocno.  */
//原型 ira_costs ira-int.h ira-costs.cc
void mtcs_ira_costs_costs (MtcsIraCosts *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);

   self->allocno_p = true;
   self->cost_elements_num = ira_allocnos_num;
   init_costs(self);
   self->total_allocno_costs = (IraCosts *) ira_allocate (mtcsIraInt->x_max_struct_costs_size * mtcsIraBuild->ira_allocnos_num);
   initiate_regno_cost_classes(self);
   if (!mtcsIra->ira_use_lra_p)
      /* Process equivs in reload to update costs through hook
      ira_adjust_equiv_reg_cost.  */
      calculate_elim_costs_all_insns ();
   find_costs_and_classes(self);
   setup_allocno_class_and_costs(self);
   finish_regno_cost_classes(self);
   finish_costs(self);
   ira_free (self->total_allocno_costs);
}

/* Entry function which defines classes for pseudos.
   Set self->pseudo_classes_defined_p only if DEFINE_PSEUDO_CLASSES is true.  */
//原型 ira_set_pseudo_classes ira.h ira-costs.cc
void mtcs_ira_costs_set_pseudo_classes (MtcsIraCosts *self,bool define_pseudo_classes, FILE *dump_file)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   FILE *saved_file = mtcsIraGlobal->ira_dump_file;
   self->allocno_p = false;
   mtcsIraGlobal->internal_flag_ira_verbose = mtcsOptionsItem->x_flag_ira_verbose;
   mtcsIraGlobal->ira_dump_file = dump_file;
   self->cost_elements_num = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);
   init_costs(self);
   initiate_regno_cost_classes(self);
   find_costs_and_classes(self);
   finish_regno_cost_classes(self);
   if (define_pseudo_classes)
      self->pseudo_classes_defined_p = true;

   finish_costs(self);
   mtcsIraGlobal->ira_dump_file = saved_file;
}



/* Change hard register costs for allocnos which lives through
   function calls.  This is called only when we found all intersected
   calls during building allocno live ranges.  */
//原型 ira_tune_allocno_costs ira-int.h ira-costs.cc
void mtcs_ira_costs_tune_allocno_costs (MtcsIraCosts *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);

   int j, n, regno;
   int cost, min_cost, *reg_costs;
   enum reg_class aclass;
   machine_mode mode;
   MtcsIraAllocno * a;
   MtcsIraAllocnoIterator ai;
   MtcsIraAllocnoObjectIterator oi;
   MtcsIraObject * obj;
   bool skip_p;

   MTCS_FOR_EACH_ALLOCNO (mtcsIraBuild,a, ai){
      aclass = a->aclass;
      if (aclass == NO_REGS)
         continue;
      mode = a->mode;
      n = mtcsIra->x_ira_class_hard_regs_num[aclass];
      min_cost = INT_MAX;
      if (a->calls_crossed_num != a->cheap_calls_crossed_num){
         mtcs_ira_build_allocate_and_set_costs/*!ira_allocate_and_set_costs*/(mtcsIraBuild,&a->hard_reg_costs, aclass,a->class_cost);
         reg_costs = a->hard_reg_costs;
         for (j = n - 1; j >= 0; j--){
            regno = mtcsIra->x_ira_class_hard_regs[aclass][j];
            skip_p = false;
            MTCS_FOR_EACH_ALLOCNO_OBJECT(a, obj, oi){
               if (mtcs_ira_hard_reg_set_intersection_p/*!ira_hard_reg_set_intersection_p*/(mtcsIra,
                     regno, mode,&obj->conflict_hard_regs)){
                  skip_p = true;
                  break;
               }
            }
            if (skip_p)
               continue;
            cost = 0;
            if (mtcs_ira_allocno_need_caller_save_p/*!ira_need_caller_save_p*/(a, regno))
               cost += mtcs_ira_caller_save_cost/*!ira_caller_save_cost*/(mtcsIra,a);
         #ifdef IRA_HARD_REGNO_ADD_COST_MULTIPLIER host=0 nvptx=0
            {
               auto rclass = mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,regno);
               cost += ((mtcsIra->x_ira_memory_move_cost[mode][rclass][0]
               + mtcsIra->x_ira_memory_move_cost[mode][rclass][1])
               * a->freq
               * IRA_HARD_REGNO_ADD_COST_MULTIPLIER (regno) / 2);
            }
         #endif
            if (INT_MAX - cost < reg_costs[j])
               reg_costs[j] = INT_MAX;
            else
               reg_costs[j] += cost;
            if (min_cost > reg_costs[j])
               min_cost = reg_costs[j];
         }
      }

      if (min_cost != INT_MAX)
         a->class_cost = min_cost;

      /* Some targets allow pseudos to be allocated to unaligned sequences
      of hard registers.  However, selecting an unaligned sequence can
      unnecessarily restrict later allocations.  So increase the cost of
      unaligned hard regs to encourage the use of aligned hard regs.  */
      {
         const int nregs = mtcsIra->x_ira_reg_class_max_nregs[aclass][a->mode];

         if (nregs > 1){
            mtcs_ira_build_allocate_and_set_costs/*!ira_allocate_and_set_costs*/(mtcsIraBuild,&a->hard_reg_costs, aclass, a->class_cost);
            reg_costs = a->hard_reg_costs;
            for (j = n - 1; j >= 0; j--){
               regno =mtcsIraInt->x_ira_non_ordered_class_hard_regs[aclass][j];
               if ((regno % nregs) != 0){
                  int index = mtcsIraInt->x_ira_class_hard_reg_index[aclass][regno];
                  ira_assert (index != -1);
                  reg_costs[index] += a->freq;
               }
            }
         }
      }
   }
}

/* A hook from the reload pass.  Add COST to the estimated gain for eliminating
   REGNO with its equivalence.  If COST is zero, record that no such
   elimination is possible.  */
//原型 ira_adjust_equiv_reg_cost ira.h ira-costs.cc
void mtcs_ira_costs_adjust_equiv_reg_cost (MtcsIraCosts *self,unsigned regno, int cost)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);

   ira_assert (!mtcsIra->ira_use_lra_p);
   if (cost == 0)
      self->regno_equiv_gains[regno] = 0;
   else
      self->regno_equiv_gains[regno] += cost;
}

//原型 ira_costs_cc_finalize ira.h ira-costs.cc
void mtcs_ira_costs_cc_finalize (MtcsIraCosts *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);

   mtcs_ira_int_free_ira_costs/*!this_target_ira_int->free_ira_costs*/(mtcsIraInt);
}

static void mtcsIraCostsInit(MtcsIraCosts *self)
{
   self->pseudo_classes_defined_p = false;
}

MtcsIraCosts *mtcs_ira_costs_new(MtcsMode *mtcsMode)
{
   MtcsIraCosts *self = n_slice_alloc0 (sizeof(MtcsIraCosts));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsIraCostsInit(self);
   return self;
}
