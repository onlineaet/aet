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
 * base on ira-lives.cc
 */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "predict.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "ira.h"
#include "ira-int.h"
#include "sparseset.h"
#include "function-abi.h"
#include "except.h"
#include "sparseset.h"


#include "../mtcstarget.h"
#include "mtcsiralives.h"
#include "mtcsira.h"
#include "mtcsiraint.h"
#include "mtcsiralooptreenode.h"
#include "mtcsirabuild.h"



/* Record hard register REGNO as now being live.  */
static void make_hard_regno_live (MtcsIraLives *self,int regno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&self->hard_regs_live, regno);
}

/* Process the definition of hard register REGNO.  This updates
   hard_regs_live and hard reg conflict information for living allocnos.  */
static void make_hard_regno_dead (MtcsIraLives *self,int regno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   unsigned int i;
   EXECUTE_IF_SET_IN_SPARSESET (self->objects_live, i){
      MtcsIraObject * obj = mtcsIraBuild->ira_object_id_map[i];

      if (self->ignore_reg_for_conflicts != NULL_RTX
      && REGNO (self->ignore_reg_for_conflicts) == (unsigned int) ((MtcsIraAllocno *)obj->allocno)->regno)
         continue;

      mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&obj->conflict_hard_regs, regno);
      mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&obj->total_conflict_hard_regs, regno);
   }
   mtcs_reg_clear_hard_reg_bit/*!CLEAR_HARD_REG_BIT*/(mtcsReg,&self->hard_regs_live, regno);
}

/* Record object OBJ as now being live.  Set a bit for it in objects_live,
   and start a new live range for it if necessary.  */
static void make_object_live (MtcsIraLives *self,MtcsIraObject * obj)
{
   sparseset_set_bit (self->objects_live, obj->id);

   MtcsLiveRange * lr = obj->live_ranges;
   if (lr == NULL || (lr->finish !=self->curr_point && lr->finish + 1 != self->curr_point))
      mtcs_ira_object_add_live_range_to_object/*!ira_add_live_range_to_object*/(obj, self->curr_point, -1);
}

/* Update ALLOCNO_EXCESS_PRESSURE_POINTS_NUM for the allocno
   associated with object OBJ.  */
static void update_allocno_pressure_excess_length (MtcsIraLives *self,MtcsIraObject * obj)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   MtcsIraAllocno * a = obj->allocno;
   int start, i;
   enum reg_class aclass, pclass, cl;
   MtcsLiveRange * p;

   aclass = a->aclass;
   pclass = mtcsIra->x_ira_pressure_class_translate[aclass];
   int limRegClasses = mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg);
   for (i = 0; (cl = mtcsIraInt->x_ira_reg_class_super_classes[pclass][i]) != limRegClasses/*!LIM_REG_CLASSES*/;i++){
      if (! mtcsIraInt->x_ira_reg_pressure_class_p[cl])
         continue;
      if (self->high_pressure_start_point[cl] < 0)
         continue;
      p = obj->live_ranges;
      ira_assert (p != NULL);
      start = (self->high_pressure_start_point[cl] > p->start ? self->high_pressure_start_point[cl] : p->start);
      a->excess_pressure_points_num += self->curr_point - start + 1;
   }
}

/* Process the definition of object OBJ, which is associated with allocno A.
   This finishes the current live range for it.  */
static void make_object_dead (MtcsIraLives *self,MtcsIraObject * obj)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   MtcsLiveRange * lr;
   int regno;
   int ignore_regno = -1;
   int ignore_total_regno = -1;
   int end_regno = -1;

   sparseset_clear_bit (self->objects_live, obj->id);
   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   /* Check whether any part of IGNORE_REG_FOR_CONFLICTS already conflicts
   with OBJ.  */
   if (self->ignore_reg_for_conflicts != NULL_RTX
   && REGNO (self->ignore_reg_for_conflicts) < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/){
      end_regno = END_REGNO (self->ignore_reg_for_conflicts);
      ignore_regno = ignore_total_regno = REGNO (self->ignore_reg_for_conflicts);

      for (regno = ignore_regno; regno < end_regno; regno++){
         if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&obj->conflict_hard_regs, regno))
            ignore_regno = end_regno;
         if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&obj->total_conflict_hard_regs, regno))
            ignore_total_regno = end_regno;
      }
   }

   obj->conflict_hard_regs |= self->hard_regs_live;
   obj->total_conflict_hard_regs |= self->hard_regs_live;

   /* If IGNORE_REG_FOR_CONFLICTS did not already conflict with OBJ, make
   sure it still doesn't.  */
   for (regno = ignore_regno; regno < end_regno; regno++)
      mtcs_reg_clear_hard_reg_bit/*!CLEAR_HARD_REG_BIT*/(mtcsReg,&obj->conflict_hard_regs, regno);
   for (regno = ignore_total_regno; regno < end_regno; regno++)
      mtcs_reg_clear_hard_reg_bit/*!CLEAR_HARD_REG_BIT*/(mtcsReg,&obj->total_conflict_hard_regs, regno);

   lr = obj->live_ranges;
   ira_assert (lr != NULL);
   lr->finish = self->curr_point;
   update_allocno_pressure_excess_length(self,obj);
}

/* Record that register pressure for PCLASS increased by N registers.
   Update the current register pressure, maximal register pressure for
   the current BB and the start point of the register pressure
   excess.  */
static void inc_register_pressure (MtcsIraLives *self,enum reg_class pclass, int n)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int i;
   enum reg_class cl;
   int limRegClasses = mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg);

   for (i = 0; (cl = mtcsIraInt->x_ira_reg_class_super_classes[pclass][i]) != limRegClasses/*!LIM_REG_CLASSES*/; i++) {
      if (! mtcsIraInt->x_ira_reg_pressure_class_p[cl])
         continue;
      self->curr_reg_pressure[cl] += n;
      if (self->high_pressure_start_point[cl] < 0
      && (self->curr_reg_pressure[cl] > mtcsIra->x_ira_class_hard_regs_num[cl]))
         self->high_pressure_start_point[cl] = self->curr_point;
      if (self->curr_bb_node->reg_pressure[cl] < self->curr_reg_pressure[cl])
         self->curr_bb_node->reg_pressure[cl] = self->curr_reg_pressure[cl];
   }
}

/* Record that register pressure for PCLASS has decreased by NREGS
   registers; update current register pressure, start point of the
   register pressure excess, and register pressure excess length for
   living allocnos.  */

static void dec_register_pressure (MtcsIraLives *self,enum reg_class pclass, int nregs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   int limRegClasses = mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg);
   int i;
   unsigned int j;
   enum reg_class cl;
   bool set_p = false;

   for (i = 0; (cl = mtcsIraInt->x_ira_reg_class_super_classes[pclass][i]) !=limRegClasses/*!LIM_REG_CLASSES*/; i++){
      if (! mtcsIraInt->x_ira_reg_pressure_class_p[cl])
         continue;
      self->curr_reg_pressure[cl] -= nregs;
      ira_assert (self->curr_reg_pressure[cl] >= 0);
      if (self->high_pressure_start_point[cl] >= 0
      && self->curr_reg_pressure[cl] <= mtcsIra->x_ira_class_hard_regs_num[cl])
         set_p = true;
   }
   if (set_p){
      EXECUTE_IF_SET_IN_SPARSESET (self->objects_live, j)
            update_allocno_pressure_excess_length(self,mtcsIraBuild->ira_object_id_map[j]);
      for (i = 0; (cl = mtcsIraInt->x_ira_reg_class_super_classes[pclass][i]) != limRegClasses/*!LIM_REG_CLASSES*/;i++){
         if (! mtcsIraInt->x_ira_reg_pressure_class_p[cl])
            continue;
         if (self->high_pressure_start_point[cl] >= 0
         && self->curr_reg_pressure[cl] <= mtcsIra->x_ira_class_hard_regs_num[cl])
            self->high_pressure_start_point[cl] = -1;
      }
   }
}

/* Determine from the objects_live bitmap whether REGNO is currently live,
   and occupies only one object.  Return false if we have no information.  */
static bool pseudo_regno_single_word_and_live_p (MtcsIraLives *self,int regno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   MtcsIraAllocno * a = mtcsIraBuild->ira_curr_regno_allocno_map[regno];
   MtcsIraObject * obj;
   if (a == NULL)
      return false;
   if (a->num_objects > 1)
      return false;

   obj = a->objects[0];
   return sparseset_bit_p (self->objects_live, obj->id);
}

/* Mark the pseudo register REGNO as live.  Update all information about
   live ranges and register pressure.  */
static void mark_pseudo_regno_live (MtcsIraLives *self,int regno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt =mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   MtcsIraAllocno * a = mtcsIraBuild->ira_curr_regno_allocno_map[regno];
   enum reg_class pclass;
   int i, n, nregs;
   if (a == NULL)
      return;
   /* Invalidate because it is referenced.  */
   self->allocno_saved_at_call[a->num] = 0;
   n = a->num_objects;
   pclass = mtcsIra->x_ira_pressure_class_translate[a->aclass];
   nregs = mtcsIra->x_ira_reg_class_max_nregs[a->aclass][ALLOCNO_MODE (a)];
   if (n > 1){
      /* We track every subobject separately.  */
      gcc_assert (nregs == n);
      nregs = 1;
   }
   for (i = 0; i < n; i++){
      MtcsIraObject * obj = a->objects[i];
      if (sparseset_bit_p (self->objects_live, obj->id))
         continue;
      inc_register_pressure(self,pclass, nregs);
      make_object_live(self,obj);
   }
}

/* Like mark_pseudo_regno_live, but try to only mark one subword of
   the pseudo as live.  SUBWORD indicates which; a value of 0
   indicates the low part.  */
static void mark_pseudo_regno_subword_live (MtcsIraLives *self,int regno, int subword)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt =mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   MtcsIraAllocno * a = mtcsIraBuild->ira_curr_regno_allocno_map[regno];
   int n;
   enum reg_class pclass;
   MtcsIraObject * obj;
   if (a == NULL)
      return;
   /* Invalidate because it is referenced.  */
   self->allocno_saved_at_call[a->num] = 0;
   n = a->num_objects;
   if (n == 1){
      mark_pseudo_regno_live(self,regno);
      return;
   }
   pclass = mtcsIra->x_ira_pressure_class_translate[a->aclass];
   gcc_assert (n == mtcsIra->x_ira_reg_class_max_nregs[a->aclass][a->mode]);
   obj = a->objects[subword];
   if (sparseset_bit_p (self->objects_live, obj->id))
      return;
   inc_register_pressure(self,pclass, 1);
   make_object_live(self,obj);
}

/* Mark the register REG as live.  Store a 1 in hard_regs_live for
   this register, record how many consecutive hardware registers it
   actually needs.  */
static void mark_hard_reg_live (MtcsIraLives *self,rtx reg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt =mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int regno = REGNO (reg);
   if (! mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&mtcsIra->x_ira_no_alloc_regs, regno)){
      int last = END_REGNO (reg);
      enum reg_class aclass, pclass;

      while (regno < last){
         if (! mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&self->hard_regs_live, regno)
         && ! mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&mtcsIra->eliminable_regset, regno)){
            aclass = ira_hard_regno_allocno_class[regno];
            pclass = mtcsIra->x_ira_pressure_class_translate[aclass];
            inc_register_pressure(self,pclass, 1);
            make_hard_regno_live(self,regno);
         }
         regno++;
      }
   }
}

/* Mark a pseudo, or one of its subwords, as live.  REGNO is the pseudo's
   register number; ORIG_REG is the access in the insn, which may be a
   subreg.  */
static void mark_pseudo_reg_live (MtcsIraLives *self,rtx orig_reg, unsigned regno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   if (mtcs_rtlanal_read_modify_subreg_p/*!read_modify_subreg_p*/(mtcsRtlanal,orig_reg)){
      mark_pseudo_regno_subword_live(self,regno,
      mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,orig_reg) ? 0 : 1);
   }else
      mark_pseudo_regno_live(self,regno);
}

/* Mark the register referenced by use or def REF as live.  */
static void mark_ref_live (MtcsIraLives *self,df_ref ref)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   rtx reg = DF_REF_REG (ref);
   rtx orig_reg = reg;
   if (GET_CODE (reg) == SUBREG)
      reg = SUBREG_REG (reg);

   if (REGNO (reg) >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg))
      mark_pseudo_reg_live(self,orig_reg, REGNO (reg));
   else
      mark_hard_reg_live(self,reg);
}

/* Mark the pseudo register REGNO as dead.  Update all information about
   live ranges and register pressure.  */
static void mark_pseudo_regno_dead (MtcsIraLives *self,int regno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt =mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   MtcsIraAllocno * a = mtcsIraBuild->ira_curr_regno_allocno_map[regno];
   int n, i, nregs;
   enum reg_class cl;

   if (a == NULL)
      return;

   /* Invalidate because it is referenced.  */
   self->allocno_saved_at_call[a->num] = 0;

   n = a->num_objects;
   cl = mtcsIra->x_ira_pressure_class_translate[a->aclass];
   nregs = mtcsIra->x_ira_reg_class_max_nregs[a->aclass][a->mode];
   if (n > 1){
      /* We track every subobject separately.  */
      gcc_assert (nregs == n);
      nregs = 1;
   }
   for (i = 0; i < n; i++){
      MtcsIraObject * obj = a->objects[i];
      if (!sparseset_bit_p (self->objects_live, obj->id))
         continue;

      dec_register_pressure(self,cl, nregs);
      make_object_dead(self,obj);
   }
}

/* Like mark_pseudo_regno_dead, but called when we know that only part of the
   register dies.  SUBWORD indicates which; a value of 0 indicates the low part.  */
static void mark_pseudo_regno_subword_dead (MtcsIraLives *self,int regno, int subword)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt =mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   MtcsIraAllocno * a = mtcsIraBuild->ira_curr_regno_allocno_map[regno];
   int n;
   enum reg_class cl;
   MtcsIraObject * obj;
   if (a == NULL)
      return;
   /* Invalidate because it is referenced.  */
   self->allocno_saved_at_call[a->num] = 0;
   n = a->num_objects;
   if (n == 1)
      /* The allocno as a whole doesn't die in this case.  */
      return;
   cl = mtcsIra->x_ira_pressure_class_translate[a->aclass];
   gcc_assert(n == mtcsIra->x_ira_reg_class_max_nregs[a->aclass][a->mode]);
   obj = a->objects[subword];
   if (!sparseset_bit_p (self->objects_live, obj->id))
      return;
   dec_register_pressure(self,cl, 1);
   make_object_dead(self,obj);
}

/* Process the definition of hard register REG.  This updates hard_regs_live
   and hard reg conflict information for living allocnos.  */
static void mark_hard_reg_dead (MtcsIraLives *self,rtx reg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt =mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int regno = REGNO (reg);

   if (! mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&mtcsIra->x_ira_no_alloc_regs, regno)){
      int last = END_REGNO (reg);
      enum reg_class aclass, pclass;

      while (regno < last){
         if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&self->hard_regs_live, regno)){
            aclass = mtcsIra->x_ira_hard_regno_allocno_class[regno];
            pclass = mtcsIra->x_ira_pressure_class_translate[aclass];
            dec_register_pressure(self,pclass, 1);
            make_hard_regno_dead(self,regno);
         }
         regno++;
      }
   }
}

/* Mark a pseudo, or one of its subwords, as dead.  REGNO is the pseudo's
   register number; ORIG_REG is the access in the insn, which may be a
   subreg.  */
static void mark_pseudo_reg_dead (MtcsIraLives *self,rtx orig_reg, unsigned regno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   if (mtcs_rtlanal_read_modify_subreg_p/*!read_modify_subreg_p*/(mtcsRtlanal,orig_reg)){
      mark_pseudo_regno_subword_dead(self,regno,mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,orig_reg) ? 0 : 1);
   }else
      mark_pseudo_regno_dead(self,regno);
}

/* Mark the register referenced by definition DEF as dead, if the
   definition is a total one.  */
static void mark_ref_dead (MtcsIraLives *self,df_ref def)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);
   rtx reg = DF_REF_REG (def);
   rtx orig_reg = reg;
   if (DF_REF_FLAGS_IS_SET (def, DF_REF_CONDITIONAL))
      return;

   if (GET_CODE (reg) == SUBREG)
      reg = SUBREG_REG (reg);

   if (DF_REF_FLAGS_IS_SET (def, DF_REF_PARTIAL)
   && (GET_CODE (orig_reg) != SUBREG
   || REGNO (reg) < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/
   || !mtcs_rtlanal_read_modify_subreg_p/*!read_modify_subreg_p*/(mtcsRtlanal,orig_reg)))
      return;

   if (REGNO (reg) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
      mark_pseudo_reg_dead(self,orig_reg, REGNO (reg));
   else
      mark_hard_reg_dead(self,reg);
}

/* If REG is a pseudo or a subreg of it, and the class of its allocno
   intersects CL, make a conflict with pseudo DREG.  ORIG_DREG is the
   rtx actually accessed, it may be identical to DREG or a subreg of it.
   Advance the current program point before making the conflict if
   ADVANCE_P.  Return TRUE if we will need to advance the current
   program point.  */
static bool make_pseudo_conflict (MtcsIraLives *self,rtx reg, enum reg_class cl, rtx dreg, rtx orig_dreg,
            bool advance_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   rtx orig_reg = reg;
   MtcsIraAllocno * a;

   if (GET_CODE (reg) == SUBREG)
      reg = SUBREG_REG (reg);

   if (! REG_P (reg) || REGNO (reg) < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
      return advance_p;

   a = mtcsIraBuild->ira_curr_regno_allocno_map[REGNO (reg)];
   if (! reg_classes_intersect_p (cl, a->aclass))
      return advance_p;

   if (advance_p)
      self->curr_point++;

   mark_pseudo_reg_live(self,orig_reg, REGNO (reg));
   mark_pseudo_reg_live(self,orig_dreg, REGNO (dreg));
   mark_pseudo_reg_dead(self,orig_reg, REGNO (reg));
   mark_pseudo_reg_dead(self,orig_dreg, REGNO (dreg));

   return false;
}

/* Check and make if necessary conflicts for pseudo DREG of class
   DEF_CL of the current insn with input operand USE of class USE_CL.
   ORIG_DREG is the rtx actually accessed, it may be identical to
   DREG or a subreg of it.  Advance the current program point before
   making the conflict if ADVANCE_P.  Return TRUE if we will need to
   advance the current program point.  */
static bool check_and_make_def_use_conflict (MtcsIraLives *self,rtx dreg, rtx orig_dreg,
             enum reg_class def_cl, int use,
             enum reg_class use_cl, bool advance_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog  *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   if (! reg_classes_intersect_p (def_cl, use_cl))
      return advance_p;

   advance_p = make_pseudo_conflict(self,mtcsRecog->recog_data.operand[use],use_cl, dreg, orig_dreg, advance_p);

   /* Reload may end up swapping commutative operands, so you
   have to take both orderings into account.  The
   constraints for the two operands can be completely
   different.  (Indeed, if the constraints for the two
   operands are the same for all alternatives, there's no
   point marking them as commutative.)  */
   if (use < mtcsRecog->recog_data.n_operands - 1
   && mtcsRecog->recog_data.constraints[use][0] == '%')
      advance_p  = make_pseudo_conflict(self,mtcsRecog->recog_data.operand[use + 1],use_cl, dreg, orig_dreg, advance_p);
   if (use >= 1  && mtcsRecog->recog_data.constraints[use - 1][0] == '%')
      advance_p  = make_pseudo_conflict(self,mtcsRecog->recog_data.operand[use - 1],use_cl, dreg, orig_dreg, advance_p);
   return advance_p;
}

/* Check and make if necessary conflicts for definition DEF of class
   DEF_CL of the current insn with input operands.  Process only
   constraints of alternative ALT.

   One of three things is true when this function is called:

   (1) DEF is an earlyclobber for alternative ALT.  Input operands then
       conflict with DEF in ALT unless they explicitly match DEF via 0-9
       constraints.

   (2) DEF matches (via 0-9 constraints) an operand that is an
       earlyclobber for alternative ALT.  Other input operands then
       conflict with DEF in ALT.

   (3) [FOR_TIE_P] Some input operand X matches DEF for alternative ALT.
       Input operands with a different value from X then conflict with
       DEF in ALT.

   However, there's still a judgement call to make when deciding
   whether a conflict in ALT is important enough to be reflected
   in the pan-alternative allocno conflict set.  */
static void check_and_make_def_conflict (MtcsIraLives *self,int alt, int def, enum reg_class def_cl,
              bool for_tie_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog  *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   int use, use_match;
   MtcsIraAllocno * a;
   enum reg_class use_cl, acl;
   bool advance_p;
   rtx dreg = mtcsRecog->recog_data.operand[def];
   rtx orig_dreg = dreg;

   if (def_cl == NO_REGS)
      return;

   if (GET_CODE (dreg) == SUBREG)
      dreg = SUBREG_REG (dreg);

   if (! REG_P (dreg) || REGNO (dreg) < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
      return;

   a = mtcsIraBuild->ira_curr_regno_allocno_map[REGNO (dreg)];
   acl = a->aclass;
   if (! reg_classes_intersect_p (acl, def_cl))
      return;

   advance_p = true;

   int n_operands = mtcsRecog->recog_data.n_operands;
   const mtcs_operand_alternative *op_alt = &mtcsRecog->recog_op_alt[alt * n_operands];
   for (use = 0; use < n_operands; use++){
      int alt1;
      if (use == def || mtcsRecog->recog_data.operand_type[use] == OP_OUT)
         continue;
      /* An earlyclobber on DEF doesn't apply to an input operand X if X
      explicitly matches DEF, but it applies to other input operands
      even if they happen to be the same value as X.

      In contrast, if an input operand X is tied to a non-earlyclobber
      DEF, there's no conflict with other input operands that have the
      same value as X.  */
      if (op_alt[use].matches == def
      || (for_tie_p  && rtx_equal_p (mtcsRecog->recog_data.operand[use],
      mtcsRecog->recog_data.operand[op_alt[def].matched])))
         continue;

      if (op_alt[use].anything_ok)
         use_cl = mtcs_reg_get_all_regs/*!ALL_REGS*/(mtcsReg);
      else
         use_cl = op_alt[use].cl;
      if (use_cl == NO_REGS)
         continue;

      /* If DEF is simply a tied operand, ignore cases in which this
      alternative requires USE to have a likely-spilled class.
      Adding a conflict would just constrain USE further if DEF
      happens to be allocated first.  */
      if (for_tie_p && targetm.class_likely_spilled_p (use_cl))
         continue;

      /* If there's any alternative that allows USE to match DEF, do not
      record a conflict.  If that causes us to create an invalid
      instruction due to the earlyclobber, reload must fix it up.

      Likewise, if we're treating a tied DEF like a partial earlyclobber,
      do not record a conflict if there's another alternative in which
      DEF is neither tied nor earlyclobber.  */
      for (alt1 = 0; alt1 < mtcsRecog->recog_data.n_alternatives; alt1++){
         if (!TEST_BIT (self->preferred_alternatives, alt1))
            continue;
         const mtcs_operand_alternative *op_alt1  = &mtcsRecog->recog_op_alt[alt1 * n_operands];
         if (op_alt1[use].matches == def
         || (use < n_operands - 1
         && mtcsRecog->recog_data.constraints[use][0] == '%'
         && op_alt1[use + 1].matches == def)
         || (use >= 1
         && mtcsRecog->recog_data.constraints[use - 1][0] == '%'
         && op_alt1[use - 1].matches == def))
            break;
         if (for_tie_p
         && !op_alt1[def].earlyclobber
         && op_alt1[def].matched < 0
         && mtcs_recog_alternative_class/*!alternative_class*/(op_alt1, def) != NO_REGS
         && mtcs_recog_alternative_class/*!alternative_class*/(op_alt1, use) != NO_REGS)
            break;
      }

      if (alt1 < mtcsRecog->recog_data.n_alternatives)
         continue;

      advance_p = check_and_make_def_use_conflict(self,dreg, orig_dreg, def_cl,use, use_cl, advance_p);

      if ((use_match = op_alt[use].matches) >= 0){
         gcc_checking_assert (use_match != def);

         if (op_alt[use_match].anything_ok)
            use_cl = mtcs_reg_get_all_regs/*!ALL_REGS*/(mtcsReg);
         else
            use_cl = op_alt[use_match].cl;
         advance_p = check_and_make_def_use_conflict(self,dreg, orig_dreg, def_cl, use, use_cl, advance_p);
      }
   }
}

/* Make conflicts of early clobber pseudo registers of the current
   insn with its inputs.  Avoid introducing unnecessary conflicts by
   checking classes of the constraints and pseudos because otherwise
   significant code degradation is possible for some targets.

   For these purposes, tying an input to an output makes that output act
   like an earlyclobber for inputs with a different value, since the output
   register then has a predetermined purpose on input to the instruction.  */
static void make_early_clobber_and_input_conflicts (MtcsIraLives *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog  *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   int alt;
   int def, def_match;
   enum reg_class def_cl;

   int n_alternatives = mtcsRecog->recog_data.n_alternatives;
   int n_operands = mtcsRecog->recog_data.n_operands;
   const mtcs_operand_alternative *op_alt = mtcsRecog->recog_op_alt;
   for (alt = 0; alt < n_alternatives; alt++, op_alt += n_operands)
      if (TEST_BIT (self->preferred_alternatives, alt))
         for (def = 0; def < n_operands; def++){
            if (op_alt[def].anything_ok)
               def_cl = mtcs_reg_get_all_regs/*!ALL_REGS*/(mtcsReg);
            else
               def_cl = op_alt[def].cl;
            if (def_cl != NO_REGS){
               if (op_alt[def].earlyclobber)
                  check_and_make_def_conflict(self,alt, def, def_cl, false);
               else if (op_alt[def].matched >= 0  && !targetm.class_likely_spilled_p (def_cl))
                  check_and_make_def_conflict(self,alt, def, def_cl, true);
            }

            if ((def_match = op_alt[def].matches) >= 0 && (op_alt[def_match].earlyclobber || op_alt[def].earlyclobber)){
               if (op_alt[def_match].anything_ok)
                  def_cl = mtcs_reg_get_all_regs/*!ALL_REGS*/(mtcsReg);
               else
                  def_cl = op_alt[def_match].cl;
               check_and_make_def_conflict(self,alt, def, def_cl, false);
            }
         }
}

/* Mark early clobber hard registers of the current INSN as live (if
   LIVE_P) or dead.  Return true if there are such registers.  */
static bool mark_hard_reg_early_clobbers (MtcsIraLives *self,rtx_insn *insn, bool live_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);
   df_ref def;
   bool set_p = false;

   FOR_EACH_INSN_DEF (def, insn)
      if (DF_REF_FLAGS_IS_SET (def, DF_REF_MUST_CLOBBER)){
         rtx dreg = DF_REF_REG (def);

         if (GET_CODE (dreg) == SUBREG)
            dreg = SUBREG_REG (dreg);
         if (! REG_P (dreg) || REGNO (dreg) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
            continue;

         /* Hard register clobbers are believed to be early clobber
         because there is no way to say that non-operand hard
         register clobbers are not early ones.  */
         if (live_p)
            mark_ref_live(self,def);
         else
            mark_ref_dead(self,def);
         set_p = true;
      }

   return set_p;
}

/* Checks that CONSTRAINTS permits to use only one hard register.  If
   it is so, the function returns the class of the hard register.
   Otherwise it returns NO_REGS.  */
static enum reg_class single_reg_class (MtcsIraLives *self,const char *constraints, rtx op, rtx equiv_const)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog  *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsPreds   *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int c;
   enum reg_class cl, next_cl;
   enum constraint_num cn;

   cl = NO_REGS;
   alternative_mask preferred = self->preferred_alternatives;
   while ((c = *constraints)){
      if (c == '#')
         preferred &= ~ALTERNATIVE_BIT (0);
      else if (c == ',')
         preferred >>= 1;
      else if (preferred & 1)
         switch (c){
            case 'g':
               return NO_REGS;

            default:
               /* ??? Is this the best way to handle memory constraints?  */
               cn = mtcs_preds_lookup_constraint/*!lookup_constraint*/(mtcsPreds,constraints);
               if (mtcs_preds_insn_extra_memory_constraint/*!insn_extra_memory_constraint*/(mtcsPreds,cn)
               || mtcs_preds_insn_extra_special_memory_constraint/*!insn_extra_special_memory_constraint*/(mtcsPreds,cn)
               || mtcs_preds_insn_extra_relaxed_memory_constraint/*!insn_extra_relaxed_memory_constraint*/(mtcsPreds,cn)
               || mtcs_preds_insn_extra_address_constraint/*!insn_extra_address_constraint*/(mtcsPreds,cn))
                  return NO_REGS;
               if (mtcs_preds_constraint_satisfied_p/*!constraint_satisfied_p*/(mtcsPreds,op, cn)
               || (equiv_const != NULL_RTX
               && CONSTANT_P (equiv_const)
               && mtcs_preds_constraint_satisfied_p/*!constraint_satisfied_p*/(mtcsPreds,equiv_const, cn)))
                  return NO_REGS;
               next_cl = mtcs_preds_reg_class_for_constraint/*!reg_class_for_constraint*/(mtcsPreds,cn);
               if (next_cl == NO_REGS)
                  break;
               if (cl == NO_REGS
               ? mtcsIra->x_ira_class_singleton[next_cl][GET_MODE (op)] < 0
               : (mtcsIra->x_ira_class_singleton[cl][GET_MODE (op)]
               != mtcsIra->x_ira_class_singleton[next_cl][GET_MODE (op)]))
                  return NO_REGS;
               cl = next_cl;
               break;

            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
            {
               char *end;
               unsigned long dup = strtoul (constraints, &end, 10);
               constraints = end;
               next_cl = single_reg_class(self,mtcsRecog->recog_data.constraints[dup],
               mtcsRecog->recog_data.operand[dup], NULL_RTX);
               if (cl == NO_REGS
               ? mtcsIra->x_ira_class_singleton[next_cl][GET_MODE (op)] < 0
               : (mtcsIra->x_ira_class_singleton[cl][GET_MODE (op)]
               != mtcsIra->x_ira_class_singleton[next_cl][GET_MODE (op)]))
                  return NO_REGS;
               cl = next_cl;
               continue;
            }
         }
      constraints += mtcs_preds_insn_constraint_len/*!CONSTRAINT_LEN*/(mtcsPreds,c, constraints);
   }
   return cl;
}

/* The function checks that operand OP_NUM of the current insn can use
   only one hard register.  If it is so, the function returns the
   class of the hard register.  Otherwise it returns NO_REGS.  */
static enum reg_class single_reg_operand_class (MtcsIraLives *self,int op_num)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog  *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   if (op_num < 0 || mtcsRecog->recog_data.n_alternatives == 0)
      return NO_REGS;
   return single_reg_class(self,mtcsRecog->recog_data.constraints[op_num],
         mtcsRecog->recog_data.operand[op_num], NULL_RTX);
}

/* The function sets up hard register set *SET to hard registers which
   might be used by insn reloads because the constraints are too
   strict.  */
//原型 ira_implicitly_set_insn_hard_regs ira-int.h ira-lives.cc
void mtcs_ira_lives_implicitly_set_insn_hard_regs (MtcsIraLives *self, HardRegSet *set,
               alternative_mask preferred)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog  *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsPreds   *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);
   int i, c, regno = 0;
   enum reg_class cl;
   rtx op;
   machine_mode mode;

   mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(set);
   for (i = 0; i < mtcsRecog->recog_data.n_operands; i++){
      op = mtcsRecog->recog_data.operand[i];

      if (GET_CODE (op) == SUBREG)
         op = SUBREG_REG (op);

      if (GET_CODE (op) == SCRATCH
      || (REG_P (op) && (regno = REGNO (op)) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)){
         const char *p = mtcsRecog->recog_data.constraints[i];

         mode = (GET_CODE (op) == SCRATCH ? GET_MODE (op) : mtcs_rtl_data_get_pseudo_regno_mode/*!PSEUDO_REGNO_MODE*/(mtcsRtlData,regno));
         cl = NO_REGS;
         for (; (c = *p); p += mtcs_preds_insn_constraint_len/*!CONSTRAINT_LEN*/(mtcsPreds,c, p))
            if (c == '#')
               preferred &= ~ALTERNATIVE_BIT (0);
            else if (c == ',')
               preferred >>= 1;
            else if (preferred & 1){
               cl = mtcs_preds_reg_class_for_constraint/*!reg_class_for_constraint*/(mtcsPreds,
               mtcs_preds_lookup_constraint/*!lookup_constraint*/(mtcsPreds,p));
               if (cl != NO_REGS){
                  /* There is no register pressure problem if all of the
                  regs in this class are fixed.  */
                  int regno = mtcsIra->x_ira_class_singleton[cl][mode];
                  if (regno >= 0)
                     mtcs_reg_add_to_hard_reg_set/*!add_to_hard_reg_set*/(mtcsReg,set, mode, regno);
               }
            }
      }
   }
}
/* Processes input operands, if IN_P, or output operands otherwise of
   the current insn with FREQ to find allocno which can use only one
   hard register and makes other currently living allocnos conflicting
   with the hard register.  */
static void process_single_reg_class_operands (MtcsIraLives *self,bool in_p, int freq)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog  *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);
   int i, regno;
   unsigned int px;
   enum reg_class cl;
   rtx operand;
   MtcsIraAllocno * operand_a, *a;

   for (i = 0; i < mtcsRecog->recog_data.n_operands; i++){
      operand = mtcsRecog->recog_data.operand[i];
      if (in_p && mtcsRecog->recog_data.operand_type[i] != OP_IN
      && mtcsRecog->recog_data.operand_type[i] != OP_INOUT)
         continue;
      if (! in_p && mtcsRecog->recog_data.operand_type[i] != OP_OUT
      && mtcsRecog->recog_data.operand_type[i] != OP_INOUT)
         continue;
      cl = single_reg_operand_class(self,i);
      if (cl == NO_REGS)
         continue;

      operand_a = NULL;

      if (GET_CODE (operand) == SUBREG)
         operand = SUBREG_REG (operand);

      if (REG_P (operand)  && (regno = REGNO (operand)) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/){
         enum reg_class aclass;

         operand_a = mtcsIraBuild->ira_curr_regno_allocno_map[regno];
         aclass = operand_a->aclass;
         if (mtcsIra->x_ira_class_subset_p[cl][aclass]){
            /* View the desired allocation of OPERAND as:

            (REG:YMODE YREGNO),

            a simplification of:

            (subreg:YMODE (reg:XMODE XREGNO) OFFSET).  */
            machine_mode ymode, xmode;
            int xregno, yregno;
            poly_int64 offset;

            xmode = mtcsRecog->recog_data.operand_mode[i];
            xregno = mtcsIra->x_ira_class_singleton[cl][xmode];
            gcc_assert (xregno >= 0);
            ymode = operand_a->mode;
            offset = mtcs_mode_subreg_lowpart_offset/*!subreg_lowpart_offset*/(mtcsMode,ymode, xmode);
            yregno = mtcs_rtl_simplify_subreg_regno/*!simplify_subreg_regno*/(mtcsRTL,xregno, xmode, offset, ymode);
            if (yregno >= 0 && mtcsIraInt->x_ira_class_hard_reg_index[aclass][yregno] >= 0){
               int cost;

               mtcs_ira_build_allocate_and_set_costs/*!ira_allocate_and_set_costs*/(mtcsIraBuild,
                     &operand_a->conflict_hard_reg_costs,aclass, 0);
               mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,xmode);
               cost = freq * (in_p
               ? mtcsIraInt->x_ira_register_move_cost[xmode][aclass][cl]
               : mtcsIraInt->x_ira_register_move_cost[xmode][cl][aclass]);
               operand_a->conflict_hard_reg_costs[mtcsIraInt->x_ira_class_hard_reg_index[aclass][yregno]] -= cost;
            }
         }
      }

      EXECUTE_IF_SET_IN_SPARSESET (self->objects_live, px){
         MtcsIraObject * obj = mtcsIraBuild->ira_object_id_map[px];
         a = obj->allocno;
         if (a != operand_a){
            /* We could increase costs of A instead of making it
            conflicting with the hard register.  But it works worse
            because it will be spilled in reload in anyway.  */
            obj->conflict_hard_regs |= mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl];
            obj->total_conflict_hard_regs |= mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[cl];
         }
      }
   }
}

/* Go through the operands of the extracted insn looking for operand
   alternatives that apply a register filter.  Record any such filters
   in the operand's allocno.  */
static void process_register_constraint_filters (MtcsIraLives *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog  *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   for (int opno = 0; opno < mtcsRecog->recog_data.n_operands; ++opno){
      rtx op = mtcsRecog->recog_data.operand[opno];
      if (SUBREG_P (op))
         op = SUBREG_REG (op);
      if (REG_P (op) && !mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,op)){
         MtcsIraAllocno * a = mtcsIraBuild->ira_curr_regno_allocno_map[REGNO (op)];
         for (int alt = 0; alt < mtcsRecog->recog_data.n_alternatives; alt++){
            if (!TEST_BIT (self->preferred_alternatives, alt))
               continue;

            auto *op_alt = &mtcsRecog->recog_op_alt[alt * mtcsRecog->recog_data.n_operands];
            auto cl = mtcs_recog_alternative_class/*!alternative_class*/(op_alt, opno);
            /* The two extremes are easy:

            - We should record the filter if CL matches the
            allocno class.

            - We should ignore the filter if CL and the allocno class
            are disjoint.  We'll either pick a different alternative
            or reload the operand.

            Things are trickier if the classes overlap.  However:

            - If the allocno class includes registers that are not
            in CL, some choices of hard register will need a reload
            anyway.  It isn't obvious that reloads due to filters
            are worse than reloads due to regnos being outside CL.

            - Conversely, if the allocno class is a subset of CL,
            any allocation will satisfy the class requirement.
            We should try to make sure it satisfies the filter
            requirement too.  This is useful if, for example,
            an allocno needs to be in "low" registers to satisfy
            some uses, and its allocno class is therefore those
            low registers, but the allocno is elsewhere allowed
            to be in any even-numbered register.  Picking an
            even-numbered low register satisfies both types of use.  */
            if (!mtcsIra->x_ira_class_subset_p[a->aclass][cl])
               continue;

            auto filters = mtcs_recog_alternative_register_filters/*!alternative_register_filters*/(op_alt, opno);
            if (!filters)
               continue;

            filters |= a->register_filters;
            a->register_filters = filters;/*!ALLOCNO_SET_REGISTER_FILTERS (a, filters);*/
         }
      }
   }
}

/* Look through the CALL_INSN_FUNCTION_USAGE of a call insn INSN, and see if
   we find a SET rtx that we can use to deduce that a register can be cheaply
   caller-saved.  Return such a register, or NULL_RTX if none is found.  */
static rtx find_call_crossed_cheap_reg (MtcsIraLives *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   rtx cheap_reg = NULL_RTX;
   rtx exp = CALL_INSN_FUNCTION_USAGE (insn);

   while (exp != NULL){
      rtx x = XEXP (exp, 0);
      if (GET_CODE (x) == SET){
         exp = x;
         break;
      }
      exp = XEXP (exp, 1);
   }
   if (exp != NULL){
      basic_block bb = BLOCK_FOR_INSN (insn);
      rtx reg = SET_SRC (exp);
      rtx_insn *prev = PREV_INSN (insn);
      while (prev && !(INSN_P (prev)  && BLOCK_FOR_INSN (prev) != bb)){
         if (NONDEBUG_INSN_P (prev)){
            rtx set = single_set (prev);

            if (set && rtx_equal_p (SET_DEST (set), reg)){
               rtx src = SET_SRC (set);
               if (!REG_P (src) || mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,src)
                     || !pseudo_regno_single_word_and_live_p(self,REGNO (src)))
                  break;
               if (!mtcs_rtlanal_modified_between_p/*!modified_between_p*/(mtcsRtlanal,src, prev, insn))
                  cheap_reg = src;
               break;
            }
            if (set && rtx_equal_p (SET_SRC (set), reg)){
               rtx dest = SET_DEST (set);
               if (!REG_P (dest) || mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,dest)
               || !pseudo_regno_single_word_and_live_p(self,REGNO (dest)))
                  break;
               if (!mtcs_rtlanal_modified_between_p/*!modified_between_p*/(mtcsRtlanal,dest, prev, insn))
                  cheap_reg = dest;
               break;
            }

            if (mtcs_rtlanal_reg_set_p/*!reg_set_p*/(mtcsRtlanal,reg, prev))
               break;
         }
         prev = PREV_INSN (prev);
      }
   }
   return cheap_reg;
}

/* Determine whether INSN is a register to register copy of the type where
   we do not need to make the source and destiniation registers conflict.
   If this is a copy instruction, then return the source reg.  Otherwise,
   return NULL_RTX.  */
//原型 non_conflicting_reg_copy_p ira.h ira-lives.cc
rtx mtcs_ira_lives_non_conflicting_reg_copy_p (MtcsIraLives *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   /* Reload has issues with overlapping pseudos being assigned to the
   same hard register, so don't allow it.  See PR87600 for details.  */
   if (!mtcsTarget/*!targetm.lra_p*/->lra_p(mtcsTarget))
      return NULL_RTX;

   rtx set = single_set (insn);

   /* Disallow anything other than a simple register to register copy
   that has no side effects.  */
   if (set == NULL_RTX  || !REG_P (SET_DEST (set)) || !REG_P (SET_SRC (set)) || side_effects_p (set))
      return NULL_RTX;

   int dst_regno = REGNO (SET_DEST (set));
   int src_regno = REGNO (SET_SRC (set));
   machine_mode mode = GET_MODE (SET_DEST (set));

   /* By definition, a register does not conflict with itself, therefore we
   do not have to handle it specially.  Returning NULL_RTX now, helps
   simplify the callers of this function.  */
   if (dst_regno == src_regno)
      return NULL_RTX;

   /* Computing conflicts for register pairs is difficult to get right, so
   for now, disallow it.  */
   if ((mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,dst_regno)
   && mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,dst_regno, mode) != 1)
   || (mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,src_regno)
   && mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,src_regno, mode) != 1))
      return NULL_RTX;

   return SET_SRC (set);
}

#ifdef EH_RETURN_DATA_REGNO

/* Add EH return hard registers as conflict hard registers to allocnos
   living at end of BB.  For most allocnos it is already done in
   process_bb_node_lives when we processing input edges but it does
   not work when and EH edge is edge out of the current region.  This
   function covers such out of region edges. */
static void process_out_of_region_eh_regs (MtcsIraLives *self,basic_block bb)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);
   edge e;
   edge_iterator ei;
   unsigned int i;
   bitmap_iterator bi;
   bool eh_p = false;

   FOR_EACH_EDGE (e, ei, bb->succs)
      if ((e->flags & EDGE_EH) && MTCS_IRA_BB_NODE (e->dest)->parent != MTCS_IRA_BB_NODE (bb)->parent)
         eh_p = true;

   if (! eh_p)
      return;

   EXECUTE_IF_SET_IN_BITMAP (df_get_live_out (bb), firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/, i, bi){
      MtcsIraAllocno * a = mtcsIraBuild->ira_curr_regno_allocno_map[i];
      for (int n = a->num_objects - 1; n >= 0; n--){
         MtcsIraObject * obj = a->objects[n];
        // gcc15改为
        // OBJECT_CONFLICT_HARD_REGS (obj) |= eh_return_data_regs;
        // OBJECT_TOTAL_CONFLICT_HARD_REGS (obj) |= eh_return_data_regs;
         obj->conflict_hard_regs |=mtcsReg->hardRegs.x_eh_return_data_regs;
         obj->total_conflict_hard_regs |=mtcsReg->hardRegs.x_eh_return_data_regs;

//         for (int k = 0; ; k++){
//            unsigned int regno = EH_RETURN_DATA_REGNO (k);
//            if (regno == INVALID_REGNUM)
//               break;
//            mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&obj->conflict_hard_regs, regno);
//            mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&obj->total_conflict_hard_regs, regno);
//         }
      }
   }
}

#endif

/* Add conflicts for object OBJ from REGION landing pads using CALLEE_ABI.  */
static void add_conflict_from_region_landing_pads (MtcsIraLives *self,eh_region region, MtcsIraObject * obj,
      mtcs_function_abi/*!function_abi*/ callee_abi)
{
   MtcsIraAllocno * a = obj->allocno;
   rtx_code_label *landing_label;
   basic_block landing_bb;

   for (eh_landing_pad lp = region->landing_pads; lp ; lp = lp->next_lp){
      if ((landing_label = lp->landing_pad) != NULL  && (landing_bb = BLOCK_FOR_INSN (landing_label)) != NULL
      && (region->type != ERT_CLEANUP || bitmap_bit_p (df_get_live_in (landing_bb),a->regno))){
         HardRegSet new_conflict_regs  = callee_abi.mode_clobbers (a->mode);
         obj->conflict_hard_regs |= new_conflict_regs;
         obj->total_conflict_hard_regs |= new_conflict_regs;
         return;
      }
   }
}

/* Process insns of the basic block given by its LOOP_TREE_NODE to
   update allocno live ranges, allocno hard register conflicts,
   intersected calls, and register pressure info for allocnos for the
   basic block for and regions containing the basic block.  */
static void processBBNodeLives_cb (MtcsIraLoopTreeNode *loop_tree_node,void *userData)
{
   MtcsIraLives *self=(MtcsIraLives *)userData;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExcept  *mtcsExcept =mtcs_target_get_except(mtcsTarget);
   MtcsFuncAbi *mtcsFuncAbi=mtcs_target_get_func_abi(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   int i, freq;
   unsigned int j;
   basic_block bb;
   rtx_insn *insn;
   bitmap_iterator bi;
   bitmap reg_live_out;
   unsigned int px;
   bool set_p;

   bb = loop_tree_node->bb;
   if (bb != NULL){
      for (i = 0; i < mtcsIra->x_ira_pressure_classes_num; i++){
         self->curr_reg_pressure[mtcsIra->x_ira_pressure_classes[i]] = 0;
         self->high_pressure_start_point[mtcsIra->x_ira_pressure_classes[i]] = -1;
      }
      self->curr_bb_node = loop_tree_node;
      reg_live_out = df_get_live_out (bb);
      sparseset_clear (self->objects_live);
      mtcs_reg_reg_set_to_hard_reg_set/*!REG_SET_TO_HARD_REG_SET*/(mtcsReg,&self->hard_regs_live, reg_live_out);
      self->hard_regs_live &= ~(mtcsIra->eliminable_regset | mtcsIra->x_ira_no_alloc_regs);
      for (i = 0; i < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; i++)
         if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&self->hard_regs_live, i)){
            enum reg_class aclass, pclass, cl;

            aclass = mtcsIra->x_ira_allocno_class_translate[mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,i)];
            pclass = mtcsIra->x_ira_pressure_class_translate[aclass];
            int limRegClasses = mtcs_reg_get_lim_reg_classes/*!LIM_REG_CLASSES*/(mtcsReg);
            for (j = 0;(cl = mtcsIraInt->x_ira_reg_class_super_classes[pclass][j]) != limRegClasses/*!LIM_REG_CLASSES*/;j++){
               if (! mtcsIraInt->x_ira_reg_pressure_class_p[cl])
                  continue;
               self->curr_reg_pressure[cl]++;
               if (self->curr_bb_node->reg_pressure[cl] < self->curr_reg_pressure[cl])
                  self->curr_bb_node->reg_pressure[cl] = self->curr_reg_pressure[cl];
               ira_assert (self->curr_reg_pressure[cl] <= mtcsIra->x_ira_class_hard_regs_num[cl]);
            }
         }

      EXECUTE_IF_SET_IN_BITMAP (reg_live_out, firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/, j, bi)
         mark_pseudo_regno_live(self,j);

#ifdef EH_RETURN_DATA_REGNO
      process_out_of_region_eh_regs(self,bb);
#endif

      freq = REG_FREQ_FROM_BB (bb);
      if (freq == 0)
         freq = 1;

      /* Invalidate all allocno_saved_at_call entries.  */
      self->last_call_num++;

      /* Scan the code of this basic block, noting which allocnos and
      hard regs are born or die.

      Note that this loop treats uninitialized values as live until
      the beginning of the block.  For example, if an instruction
      uses (reg:DI foo), and only (subreg:SI (reg:DI foo) 0) is ever
      set, FOO will remain live until the beginning of the block.
      Likewise if FOO is not set at all.  This is unnecessarily
      pessimistic, but it probably doesn't matter much in practice.  */
      FOR_BB_INSNS_REVERSE (bb, insn){
         MtcsIraAllocno * a;
         df_ref def, use;
         bool call_p;

         if (!NONDEBUG_INSN_P (insn))
            continue;

         if (mtcsIraGlobal->internal_flag_ira_verbose > 2 && mtcsIraGlobal->ira_dump_file != NULL)
            fprintf (mtcsIraGlobal->ira_dump_file, "   Insn %u(l%d): point = %d\n",INSN_UID (insn),
                  loop_tree_node->parent->loop_num,self->curr_point);

         call_p = CALL_P (insn);
         self->ignore_reg_for_conflicts = mtcs_ira_lives_non_conflicting_reg_copy_p/*!non_conflicting_reg_copy_p*/(self,insn);

         /* Mark each defined value as live.  We need to do this for
         unused values because they still conflict with quantities
         that are live at the time of the definition.

         Ignore DF_REF_MAY_CLOBBERs on a call instruction.  Such
         references represent the effect of the called function
         on a call-clobbered register.  Marking the register as
         live would stop us from allocating it to a call-crossing
         allocno.  */
         FOR_EACH_INSN_DEF (def, insn)
            if (!call_p || !DF_REF_FLAGS_IS_SET (def, DF_REF_MAY_CLOBBER))
               mark_ref_live(self,def);

         /* If INSN has multiple outputs, then any value used in one
         of the outputs conflicts with the other outputs.  Model this
         by making the used value live during the output phase.

         It is unsafe to use !single_set here since it will ignore
         an unused output.  Just because an output is unused does
         not mean the compiler can assume the side effect will not
         occur.  Consider if ALLOCNO appears in the address of an
         output and we reload the output.  If we allocate ALLOCNO
         to the same hard register as an unused output we could
         set the hard register before the output reload insn.  */
         if (GET_CODE (PATTERN (insn)) == PARALLEL && multiple_sets (insn))
            FOR_EACH_INSN_USE (use, insn){
               int i;
               rtx reg;

               reg = DF_REF_REG (use);
               for (i = XVECLEN (PATTERN (insn), 0) - 1; i >= 0; i--){
                  rtx set;

                  set = XVECEXP (PATTERN (insn), 0, i);
                  if (GET_CODE (set) == SET  && reg_overlap_mentioned_p (reg, SET_DEST (set))){
                     /* After the previous loop, this is a no-op if
                     REG is contained within SET_DEST (SET).  */
                     mark_ref_live(self,use);
                     break;
                  }
               }
            }

         self->preferred_alternatives = mtcs_ira_setup_alts/*!ira_setup_alts*/(mtcsIra,insn);
         process_register_constraint_filters(self);
         process_single_reg_class_operands(self,false, freq);

         if (call_p){
            /* Try to find a SET in the CALL_INSN_FUNCTION_USAGE, and from
            there, try to find a pseudo that is live across the call but
            can be cheaply reconstructed from the return value.  */
            rtx cheap_reg = find_call_crossed_cheap_reg(self,insn);
            if (cheap_reg != NULL_RTX)
               add_reg_note (insn, REG_RETURNED, cheap_reg);

            self->last_call_num++;
            sparseset_clear (self->allocnos_processed);
            /* The current set of live allocnos are live across the call.  */
            EXECUTE_IF_SET_IN_SPARSESET (self->objects_live, i){
               MtcsIraObject * obj = mtcsIraBuild->ira_object_id_map[i];
               a = obj->allocno;
               int num = a->num;
               mtcs_function_abi callee_abi = mtcs_func_abi_insn_callee_abi/*!insn_callee_abi*/(mtcsFuncAbi,insn);

               /* Don't allocate allocnos that cross setjmps or any
               call, if this function receives a nonlocal
               goto.  */
               if (cfun->has_nonlocal_label
               || (!mtcsTarget/*!targetm.setjmp_preserves_nonvolatile_regs_p*/->setjmp_preserves_nonvolatile_regs_p(mtcsTarget)
               && (find_reg_note (insn, REG_SETJMP, NULL_RTX) != NULL_RTX))){
                  mtcs_reg_set_hard_reg_set/*!SET_HARD_REG_SET*/(&obj->conflict_hard_regs);
                  mtcs_reg_set_hard_reg_set/*!SET_HARD_REG_SET*/(&obj->total_conflict_hard_regs);
               }
               eh_region r;
               if (can_throw_internal (insn)
               && (r = mtcs_except_get_eh_region_from_rtx/*!get_eh_region_from_rtx*/(mtcsExcept,insn)) != NULL)
                  add_conflict_from_region_landing_pads(self,r, obj, callee_abi);
               if (sparseset_bit_p (self->allocnos_processed, num))
                  continue;
               sparseset_set_bit (self->allocnos_processed, num);

               if (self->allocno_saved_at_call[num] != self->last_call_num)
                  /* Here we are mimicking caller-save.cc behavior
                  which does not save hard register at a call if
                  it was saved on previous call in the same basic
                  block and the hard register was not mentioned
                  between the two calls.  */
                  a->freq += freq;
               /* Mark it as saved at the next call.  */
               self->allocno_saved_at_call[num] = self->last_call_num + 1;
               a->calls_crossed_num++;
               a->crossed_calls_abis |= 1 << callee_abi.id ();
               a->crossed_calls_clobbered_regs  |= callee_abi.full_and_partial_reg_clobbers ();
               if (cheap_reg != NULL_RTX && a->regno == (int) REGNO (cheap_reg))
                  a->cheap_calls_crossed_num++;
            }//end EXECUTE_IF_SET_IN_SPARSESET (self->objects_live, i)
         }//end if (call_p)

         /* See which defined values die here.  Note that we include
         the call insn in the lifetimes of these values, so we don't
         mistakenly consider, for e.g. an addressing mode with a
         side-effect like a post-increment fetching the address,
         that the use happens before the call, and the def to happen
         after the call: we believe both to happen before the actual
         call.  (We don't handle return-values here.)  */
         FOR_EACH_INSN_DEF (def, insn)
            if (!call_p || !DF_REF_FLAGS_IS_SET (def, DF_REF_MAY_CLOBBER))
               mark_ref_dead(self,def);

         make_early_clobber_and_input_conflicts(self);

         self->curr_point++;

         /* Mark each used value as live.  */
         FOR_EACH_INSN_USE (use, insn)
            mark_ref_live(self,use);

         process_single_reg_class_operands(self,true, freq);

         set_p = mark_hard_reg_early_clobbers(self,insn, true);

         if (set_p){
            mark_hard_reg_early_clobbers(self,insn, false);

            /* Mark each hard reg as live again.  For example, a
            hard register can be in clobber and in an insn
            input.  */
            FOR_EACH_INSN_USE (use, insn){
               rtx ureg = DF_REF_REG (use);

               if (GET_CODE (ureg) == SUBREG)
                  ureg = SUBREG_REG (ureg);
               if (! REG_P (ureg) || REGNO (ureg) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
                  continue;

               mark_ref_live(self,use);
            }
         }
         self->curr_point++;
      }//end        FOR_BB_INSNS_REVERSE (bb, insn)

      self->ignore_reg_for_conflicts = NULL_RTX;

      if (bb_has_eh_pred (bb))
         for (j = 0; ; ++j){
            unsigned int regno = EH_RETURN_DATA_REGNO (j);
            if (regno == INVALID_REGNUM)
               break;
            make_hard_regno_live(self,regno);
         }

      /* Allocnos can't go in stack regs at the start of a basic block
      that is reached by an abnormal edge. Likewise for registers
      that are at least partly call clobbered, because caller-save,
      fixup_abnormal_edges and possibly the table driven EH machinery
      are not quite ready to handle such allocnos live across such
      edges.  */
      if (bb_has_abnormal_pred (bb)){
         if(mtcs_config_ifdef(mtcsConfig,MTCS_STACK_REGS)){ /*!#ifdef STACK_REGS*/
            EXECUTE_IF_SET_IN_SPARSESET (self->objects_live, px){
               MtcsIraAllocno * a = mtcsIraBuild->ira_object_id_map[px]->allocno;

               a->no_stack_reg_p = true;
               a->total_no_stack_reg_p = true;
            }
            for (px = mtcs_reg_get_first_stack_reg/*!FIRST_STACK_REG*/(mtcsReg);
                  px <= mtcs_reg_get_last_stack_reg/*!LAST_STACK_REG*/(mtcsReg); px++)
            make_hard_regno_live(self,px);
         }/*!#endif*/
         /* No need to record conflicts for call clobbered regs if we
         have nonlocal labels around, as we don't ever try to
         allocate such regs in this case.  */
         if (!cfun->has_nonlocal_label && has_abnormal_call_or_eh_pred_edge_p (bb))
            for (px = 0; px < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; px++)
               if (eh_edge_abi.clobbers_at_least_part_of_reg_p (px)
               #ifdef REAL_PIC_OFFSET_TABLE_REGNUM
               /* We should create a conflict of PIC pseudo with
               PIC hard reg as PIC hard reg can have a wrong
               value after jump described by the abnormal edge.
               In this case we cannot allocate PIC hard reg to
               PIC pseudo as PIC pseudo will also have a wrong
               value.  This code is not critical as LRA can fix
               it but it is better to have the right allocation
               earlier.  */
               || (px == REAL_PIC_OFFSET_TABLE_REGNUM
               && mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(mtcsRTL) != NULL_RTX
               && REGNO (mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(mtcsRTL))
                     >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
               #endif
               )
                  make_hard_regno_live(self,px);
      }

      EXECUTE_IF_SET_IN_SPARSESET (self->objects_live, i)
         make_object_dead(self,mtcsIraBuild->ira_object_id_map[i]);

      self->curr_point++;

   }//end    if (bb != NULL)

   /* Propagate register pressure to upper loop tree nodes.  */
   if (loop_tree_node != mtcsIraBuild->ira_loop_tree_root)
      for (i = 0; i < mtcsIra->x_ira_pressure_classes_num; i++){
         enum reg_class pclass;
         pclass = mtcsIra->x_ira_pressure_classes[i];
         if (loop_tree_node->reg_pressure[pclass] > loop_tree_node->parent->reg_pressure[pclass])
            loop_tree_node->parent->reg_pressure[pclass] = loop_tree_node->reg_pressure[pclass];
      }
}

/* Create and set up IRA_START_POINT_RANGES and
   IRA_FINISH_POINT_RANGES.  */
static void create_start_finish_chains (MtcsIraLives *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   MtcsIraObject * obj;
   MtcsIraObjectIterator oi;
   MtcsLiveRange * r;

   self->ira_start_point_ranges = (MtcsLiveRange * *) ira_allocate (self->ira_max_point * sizeof (MtcsLiveRange *));
   memset (self->ira_start_point_ranges, 0, self->ira_max_point * sizeof (MtcsLiveRange *));
   self->ira_finish_point_ranges = (MtcsLiveRange * *) ira_allocate (self->ira_max_point * sizeof (MtcsLiveRange *));
   memset (self->ira_finish_point_ranges, 0, self->ira_max_point * sizeof (MtcsLiveRange *));
   MTCS_FOR_EACH_OBJECT (mtcsIraBuild,obj, oi)
      for (r = obj->live_ranges; r != NULL; r = r->next){
         r->start_next = self->ira_start_point_ranges[r->start];
         self->ira_start_point_ranges[r->start] = r;
         r->finish_next = self->ira_finish_point_ranges[r->finish];
         self->ira_finish_point_ranges[r->finish] = r;
      }
}

/* Rebuild IRA_START_POINT_RANGES and IRA_FINISH_POINT_RANGES after
   new live ranges and program points were added as a result if new
   insn generation.  */
//原型 ira_rebuild_start_finish_chains ira-int.h ira-lives.cc
void mtcs_ira_lives_rebuild_start_finish_chains (MtcsIraLives *self)
{
   ira_free (self->ira_finish_point_ranges);
   ira_free (self->ira_start_point_ranges);
   create_start_finish_chains(self);
}

/* Compress allocno live ranges by removing program points where
   nothing happens.  */
static void remove_some_program_points_and_update_live_ranges (MtcsIraLives *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   unsigned i;
   int n;
   int *map;
   MtcsIraObject * obj;
   MtcsIraObjectIterator oi;
   MtcsLiveRange * r, *prev_r, *next_r;
   sbitmap_iterator sbi;
   bool born_p, dead_p, prev_born_p, prev_dead_p;

   auto_sbitmap born (self->ira_max_point);
   auto_sbitmap dead (self->ira_max_point);
   bitmap_clear (born);
   bitmap_clear (dead);
   MTCS_FOR_EACH_OBJECT (mtcsIraBuild,obj, oi)
      for (r = obj->live_ranges; r != NULL; r = r->next){
         ira_assert (r->start <= r->finish);
         bitmap_set_bit (born, r->start);
         bitmap_set_bit (dead, r->finish);
      }

   auto_sbitmap born_or_dead (self->ira_max_point);
   bitmap_ior (born_or_dead, born, dead);
   map = (int *) ira_allocate (sizeof (int) * self->ira_max_point);
   n = -1;
   prev_born_p = prev_dead_p = false;
   EXECUTE_IF_SET_IN_BITMAP (born_or_dead, 0, i, sbi){
      born_p = bitmap_bit_p (born, i);
      dead_p = bitmap_bit_p (dead, i);
      if ((prev_born_p && ! prev_dead_p && born_p && ! dead_p) || (prev_dead_p && ! prev_born_p && dead_p && ! born_p))
         map[i] = n;
      else
         map[i] = ++n;
      prev_born_p = born_p;
      prev_dead_p = dead_p;
   }

   n++;
   if (mtcsIraGlobal->internal_flag_ira_verbose > 1 && mtcsIraGlobal->ira_dump_file != NULL)
      fprintf (mtcsIraGlobal->ira_dump_file, "Compressing live ranges: from %d to %d - %d%%\n",
            self->ira_max_point, n, 100 * n / self->ira_max_point);
   self->ira_max_point = n;

   MTCS_FOR_EACH_OBJECT (mtcsIraBuild,obj, oi)
      for (r = obj->live_ranges, prev_r = NULL; r != NULL; r = next_r){
         next_r = r->next;
         r->start = map[r->start];
         r->finish = map[r->finish];
         if (prev_r == NULL || prev_r->start > r->finish + 1){
            prev_r = r;
            continue;
         }
         prev_r->start = r->start;
         prev_r->next = next_r;
         mtcs_ira_object_finish_live_range/*!ira_finish_live_range*/(r);
      }

   ira_free (map);
}

/* 原型 ira_print_live_range_list ira-int.h ira-lives.cc 被 void mtcs_ira_object_print_live_range_list 替换*/

/* 原型 void debug (live_range *ptr) ira-int.h ira-lives.cc 被 mtcs_ira_object_live_range_debug 替换*/

/* 原型 extern void ira_debug_live_range_list (live_range_t); ira-int.h ira-lives.cc 被  mtcs_ira_object_debug_live_range_list 替换*/


/* Print live ranges of object OBJ to file F.  */
static void print_object_live_ranges (FILE *f, MtcsIraObject * obj)
{
   mtcs_ira_object_print_live_range_list/*!ira_print_live_range_list*/(obj->live_ranges,f);
}

/* Print live ranges of allocno A to file F.  */
static void print_allocno_live_ranges (FILE *f, MtcsIraAllocno * a)
{
   int n = a->num_objects;
   int i;

   for (i = 0; i < n; i++){
      fprintf (f, " a%d(r%d", a->num, a->regno);
      if (n > 1)
         fprintf (f, " [%d]", i);
      fprintf (f, "):");
      print_object_live_ranges (f, a->objects[i]);
   }
}

/* Print live ranges of allocno A to stderr.  */
void ira_debug_allocno_live_ranges (MtcsIraLives *self,MtcsIraAllocno * a)
{
  print_allocno_live_ranges (stderr, a);
}

/* Print live ranges of all allocnos to file F.  */
static void print_live_ranges (MtcsIraLives *self,FILE *f)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   MtcsIraAllocno * a;
   MtcsIraAllocnoIterator ai;

   MTCS_FOR_EACH_ALLOCNO (mtcsIraBuild,a, ai)
      print_allocno_live_ranges (f, a);
}

/* Print live ranges of all allocnos to stderr.  */
//原型 ira_debug_live_ranges ira-int.h ira-lives.cc
void mtcs_ira_lives_debug_live_ranges (MtcsIraLives *self)
{
  print_live_ranges (self,stderr);
}

/* The main entry function creates live ranges, set up
   CONFLICT_HARD_REGS and TOTAL_CONFLICT_HARD_REGS for objects, and
   calculate register pressure info.  */
void ira_create_allocno_live_ranges (MtcsIraLives *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   self->objects_live = sparseset_alloc (mtcsIraBuild->ira_objects_num);
   self->allocnos_processed = sparseset_alloc (mtcsIraBuild->ira_allocnos_num);
   self->curr_point = 0;
   self->last_call_num = 0;
   self->allocno_saved_at_call = (int *) ira_allocate (mtcsIraBuild->ira_allocnos_num * sizeof (int));
   memset (self->allocno_saved_at_call, 0, mtcsIraBuild->ira_allocnos_num * sizeof (int));
   mtcs_ira_build_traverse_loop_tree/*!ira_traverse_loop_tree*/(mtcsIraBuild,
         true, mtcsIraBuild->ira_loop_tree_root, NULL, processBBNodeLives_cb,(void*)self);
   self->ira_max_point = self->curr_point;
   create_start_finish_chains (self);
   if (mtcsIraGlobal->internal_flag_ira_verbose > 2 && mtcsIraGlobal->ira_dump_file != NULL)
      print_live_ranges (self,mtcsIraGlobal->ira_dump_file);
   /* Clean up.  */
   ira_free (self->allocno_saved_at_call);
   sparseset_free (self->objects_live);
   sparseset_free (self->allocnos_processed);
}

/* Compress allocno live ranges.  */
//原型 ira_compress_allocno_live_ranges ira-int.h ira-lives.cc
void mtcs_ira_lives_compress_allocno_live_ranges (MtcsIraLives *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   remove_some_program_points_and_update_live_ranges(self);
   ira_rebuild_start_finish_chains ();
   if (mtcsIraGlobal->internal_flag_ira_verbose > 2 && mtcsIraGlobal->ira_dump_file != NULL){
      fprintf (ira_dump_file, "Ranges after the compression:\n");
      print_live_ranges (self,mtcsIraGlobal->ira_dump_file);
   }
}

/* Free arrays IRA_START_POINT_RANGES and IRA_FINISH_POINT_RANGES.  */
//原型 ira_finish_allocno_live_ranges ira-int.h ira-lives.cc
void mtcs_ira_lives_finish_allocno_live_ranges (MtcsIraLives *self)
{
  ira_free (self->ira_finish_point_ranges);
  ira_free (self->ira_start_point_ranges);
}



static void mtcsIraLivesInit(MtcsIraLives *self)
{

}


MtcsIraLives *mtcs_ira_lives_new(MtcsMode *mtcsMode)
{
   MtcsIraLives *self = n_slice_alloc0 (sizeof(MtcsIraLives));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsIraLivesInit(self);
   return self;
}
