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
 * base on ira-conflicts.cc
 */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "predict.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "ira.h"
#include "ira-int.h"
#include "sparseset.h"
#include "addresses.h"
#include "alloc-pool.h"

#include "mtcsiraconflicts.h"
#include "mtcsiraallocno.h"
#include "mtcsirabuild.h"
#include "mtcsira.h"
#include "mtcsiraint.h"
#include "mtcsiralives.h"
#include "../mtcsmicro.h"
#include "../mtcstarget.h"


/* Macro to test a conflict of C1 and C2 in `conflicts'.  */
//原型 OBJECTS_CONFLICT_P(C1, C2) ira-conflicts.cc
static bool objects_conflict_p(MtcsIraConflicts *self,MtcsIraObject *c1,MtcsIraObject *c2)
{
   return c1->min <= c2->id &&  c2->id <= c1->max && TEST_MINMAX_SET_BIT(self->conflicts[c1->id],c2->id,c1->min,c1->max);
}

/* Record a conflict between objects OBJ1 and OBJ2.  If necessary,
   canonicalize the conflict by recording it for lower-order subobjects
   of the corresponding allocnos.  */
static void record_object_conflict (MtcsIraConflicts *self,MtcsIraObject *obj1, MtcsIraObject *obj2)
{
   MtcsIraAllocno *a1 =obj1->allocno;
   MtcsIraAllocno *a2 = obj2->allocno;
   int w1 = obj1->subword;
   int w2 = obj2->subword;
   int id1, id2;

   /* Canonicalize the conflict.  If two identically-numbered words
   conflict, always record this as a conflict between words 0.  That
   is the only information we need, and it is easier to test for if
   it is collected in each allocno's lowest-order object.  */
   if (w1 == w2 && w1 > 0){
      obj1 =a1->objects[0];
      obj2 =a2->objects[0];
   }
   id1 = obj1->id;
   id2 = obj2->id;

   SET_MINMAX_SET_BIT (self->conflicts[id1], id2, obj1->min,obj1->max);
   SET_MINMAX_SET_BIT (self->conflicts[id2], id1, obj2->min,obj2->max);
}

/* Build allocno conflict table by processing allocno live ranges.
   Return true if the table was built.  The table is not built if it
   is too big.  */
static bool build_conflict_bit_table (MtcsIraConflicts *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraLives *mtcsIraLives=mtcs_ira_mgr_get_lives(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);


   int i;
   unsigned int j;
   enum reg_class aclass;
   int object_set_words, allocated_words_num, conflict_bit_vec_words_num;
   MtcsLiveRange *r;
   MtcsIraAllocno *allocno;
   MtcsIraAllocnoIterator ai;
   sparseset objects_live;
   MtcsIraObject *obj;
   MtcsIraAllocnoObjectIterator aoi;

   allocated_words_num = 0;
   MTCS_FOR_EACH_ALLOCNO (mtcsIraBuild,allocno, ai)
      MTCS_FOR_EACH_ALLOCNO_OBJECT (allocno, obj, aoi){
         if (obj->max < obj->min)
            continue;
         conflict_bit_vec_words_num = (obj->max - obj->min + IRA_INT_BITS)/ IRA_INT_BITS;
         allocated_words_num += conflict_bit_vec_words_num;
         if ((uint64_t) allocated_words_num * sizeof (IRA_INT_TYPE)
         > (uint64_t) mtcsOptionsItem->x_param_ira_max_conflict_table_size * 1024 * 1024){
            if (mtcsIraGlobal->internal_flag_ira_verbose > 0 && mtcsIraGlobal->ira_dump_file != NULL)
               fprintf (mtcsIraGlobal->ira_dump_file, "+++Conflict table will be too big(>%dMB) "
               "-- don't use it\n", mtcsOptionsItem->x_param_ira_max_conflict_table_size);
            return false;
         }
      }

   self->conflicts = (IRA_INT_TYPE **) ira_allocate (sizeof (IRA_INT_TYPE *) * mtcsIraBuild->ira_objects_num);
   allocated_words_num = 0;
   MTCS_FOR_EACH_ALLOCNO (mtcsIraBuild,allocno, ai)
      MTCS_FOR_EACH_ALLOCNO_OBJECT (allocno, obj, aoi){
         int id = obj->id;
         if (obj->max  < obj->min){
            self->conflicts[id] = NULL;
            continue;
         }
         conflict_bit_vec_words_num = ((obj->max - obj->min + IRA_INT_BITS) / IRA_INT_BITS);
         allocated_words_num += conflict_bit_vec_words_num;
         self->conflicts[id] = (IRA_INT_TYPE *) ira_allocate (sizeof (IRA_INT_TYPE) * conflict_bit_vec_words_num);
         memset (self->conflicts[id], 0, sizeof (IRA_INT_TYPE) * conflict_bit_vec_words_num);
      }

   object_set_words = (mtcsIraBuild->ira_objects_num + IRA_INT_BITS - 1) / IRA_INT_BITS;
   if (mtcsIraGlobal->internal_flag_ira_verbose > 0 && mtcsIraGlobal->ira_dump_file != NULL)
      fprintf (mtcsIraGlobal->ira_dump_file, "+++Allocating " HOST_SIZE_T_PRINT_UNSIGNED
      " bytes for conflict table (uncompressed size "
      HOST_SIZE_T_PRINT_UNSIGNED ")\n",(fmt_size_t) (sizeof (IRA_INT_TYPE) * allocated_words_num),
      (fmt_size_t) (sizeof (IRA_INT_TYPE) * object_set_words * mtcsIraBuild->ira_objects_num));

   objects_live = sparseset_alloc (mtcsIraBuild->ira_objects_num);
   for (i = 0; i < mtcsIraLives->ira_max_point; i++){
      for (r = mtcsIraLives->ira_start_point_ranges[i]; r != NULL; r = r->start_next){
         MtcsIraObject *obj = r->object;
         MtcsIraAllocno *allocno = obj->allocno;
         int id = obj->id;

         gcc_assert (id < mtcsIraBuild->ira_objects_num);

         aclass = allocno->aclass;
         EXECUTE_IF_SET_IN_SPARSESET (objects_live, j){
            MtcsIraObject * live_obj = mtcsIraBuild->ira_object_id_map[j];
            MtcsIraAllocno *live_a = live_obj->allocno;
            enum reg_class live_aclass = live_a->aclass;

            if (mtcsIra->x_ira_reg_classes_intersect_p[aclass][live_aclass]
            /* Don't set up conflict for the allocno with itself.  */
            && live_a != allocno){
               record_object_conflict(self,obj, live_obj);
            }
         }
         sparseset_set_bit (objects_live, id);
      }

      for (r = mtcsIraLives->ira_finish_point_ranges[i]; r != NULL; r = r->finish_next)
         sparseset_clear_bit (objects_live, r->object->id);
   }
   sparseset_free (objects_live);
   return true;
}

/* Return true iff allocnos A1 and A2 cannot be allocated to the same
   register due to conflicts.  */

static bool allocnos_conflict_for_copy_p (MtcsIraConflicts *self,MtcsIraAllocno * a1, MtcsIraAllocno * a2)
{
  /* Due to the fact that we canonicalize conflicts (see
     record_object_conflict), we only need to test for conflicts of
     the lowest order words.  */
  MtcsIraObject * obj1 = a1->objects[0];
  MtcsIraObject * obj2 = a2->objects[0];

  return objects_conflict_p(self,obj1, obj2);
}

/* Check that X is REG or SUBREG of REG.  */
#define REG_SUBREG_P(x)                   \
   (REG_P (x) || (GET_CODE (x) == SUBREG && REG_P (SUBREG_REG (x))))

/* Return X if X is a REG, otherwise it should be SUBREG of REG and
   the function returns the reg in this case.  *OFFSET will be set to
   0 in the first case or the regno offset in the first case.  */
static rtx go_through_subreg (MtcsIraConflicts *self,rtx x, int *offset)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   rtx reg;

   *offset = 0;
   if (REG_P (x))
      return x;
   ira_assert (GET_CODE (x) == SUBREG);
   reg = SUBREG_REG (x);
   ira_assert (REG_P (reg));
   if (REGNO (reg) <mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg))
      *offset = mtcs_rtlanal_subreg_regno_offset/*!subreg_regno_offset*/(mtcsRtlanal,REGNO (reg), GET_MODE (reg),
            SUBREG_BYTE (x), GET_MODE (x));
   else if (!can_div_trunc_p (SUBREG_BYTE (x),
         mtcs_mode_get_regmode_natural_size/*!REGMODE_NATURAL_SIZE*/(mtcsMode,GET_MODE (x)), offset))
      /* Checked by validate_subreg.  We must know at compile time which
      inner hard registers are being accessed.  */
      gcc_unreachable ();
   return reg;
}

/* Return the recomputed frequency for this shuffle copy or its similar
   case, since it's not for a real move insn, make it smaller.  */
static int get_freq_for_shuffle_copy (int freq)
{
  return freq < 8 ? 1 : freq / 8;
}

/* Process registers REG1 and REG2 in move INSN with execution
   frequency FREQ.  The function also processes the registers in a
   potential move insn (INSN == NULL in this case) with frequency
   FREQ.  The function can modify hard register costs of the
   corresponding allocnos or create a copy involving the corresponding
   allocnos.  The function does nothing if the both registers are hard
   registers.  When nothing is changed, the function returns FALSE.
   SINGLE_INPUT_OP_HAS_CSTR_P is only meaningful when constraint_p
   is true, see function ira_get_dup_out_num for its meaning.  */
static bool process_regs_for_copy (MtcsIraConflicts *self,rtx reg1, rtx reg2, bool constraint_p, rtx_insn *insn,
             int freq, bool single_input_op_has_cstr_p = true)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt = mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraLives *mtcsIraLives=mtcs_ira_mgr_get_lives(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int allocno_preferenced_hard_regno, index, offset1, offset2;
   int cost, conflict_cost, move_cost;
   bool only_regs_p;
   MtcsIraAllocno * a;
   reg_class_t rclass, aclass;
   machine_mode mode;
   MtcsIraAllocnoCopy *cp;

   gcc_assert (REG_SUBREG_P (reg1) && REG_SUBREG_P (reg2));
   only_regs_p = REG_P (reg1) && REG_P (reg2);
   reg1 = go_through_subreg(self,reg1, &offset1);
   reg2 = go_through_subreg(self,reg2, &offset2);
   /* Set up hard regno preferenced by allocno.  If allocno gets the
   hard regno the copy (or potential move) insn will be removed.  */
   if (mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,reg1)){
      if (mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,reg2))
         return false;
      allocno_preferenced_hard_regno = REGNO (reg1) + offset1 - offset2;
      a = mtcsIraBuild->ira_curr_regno_allocno_map[REGNO (reg2)];
   }else if (mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,reg2)){
      allocno_preferenced_hard_regno = REGNO (reg2) + offset2 - offset1;
      a = mtcsIraBuild->ira_curr_regno_allocno_map[REGNO (reg1)];
   }else{
      MtcsIraAllocno * a1 = mtcsIraBuild->ira_curr_regno_allocno_map[REGNO (reg1)];
      MtcsIraAllocno * a2 = mtcsIraBuild->ira_curr_regno_allocno_map[REGNO (reg2)];

      if (!allocnos_conflict_for_copy_p(self,a1, a2)
      && offset1 == offset2
      && ordered_p (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,a1->mode),
      mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,a2->mode))){
         cp = mtcs_ira_build_add_allocno_copy/*!ira_add_allocno_copy*/(mtcsIraBuild,a1,
               a2, freq, constraint_p, insn,mtcsIraBuild->ira_curr_loop_tree_node);
         bitmap_set_bit (mtcsIraBuild->ira_curr_loop_tree_node->local_copies, cp->num);
         return true;
      }else
         return false;
   }

   if (! IN_RANGE (allocno_preferenced_hard_regno,
         0, mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg) - 1))
      /* Cannot be tied.  */
      return false;
   rclass = mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,allocno_preferenced_hard_regno);
   mode = a->mode;
   aclass = a->aclass;
   if (only_regs_p && insn != NULL_RTX
   && mtcsReg->hardRegs.x_reg_class_size/*!reg_class_size*/[rclass] <=mtcsIra->x_ira_reg_class_max_nregs [rclass][mode])
   /* It is already taken into account in ira-costs.cc.  */
      return false;
   index =mtcsIraInt->x_ira_class_hard_reg_index[aclass][allocno_preferenced_hard_regno];
   if (index < 0)
      /* Cannot be tied.  It is not in the allocno class.  */
      return false;
   mtcs_ira_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIra,mode);
   if (mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,reg1))
      move_cost = mtcsIraInt->x_ira_register_move_cost[mode][aclass][rclass];
   else
      move_cost = mtcsIraInt->x_ira_register_move_cost[mode][rclass][aclass];

   if (!single_input_op_has_cstr_p){
      /* When this is a constraint copy and the matching constraint
      doesn't only exist for this given operand but also for some
      other operand(s), it means saving the possible move cost does
      NOT need to require reg1 and reg2 to use the same hardware
      register, so this hardware preference isn't required to be
      fixed.  To avoid it to over prefer this hardware register,
      and over disparage this hardware register on conflicted
      objects, we need some cost tweaking here, similar to what
      we do for shuffle copy.  */
      gcc_assert (constraint_p);
      int reduced_freq = get_freq_for_shuffle_copy (freq);
      if (mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,reg1))
         /* For reg2 = opcode(reg1, reg3 ...), assume that reg3 is a
         pseudo register which has matching constraint on reg2,
         even if reg2 isn't assigned by reg1, it's still possible
         not to have register moves if reg2 and reg3 use the same
         hardware register.  So to avoid the allocation to over
         prefer reg1, we can just take it as a shuffle copy.  */
         cost = conflict_cost = move_cost * reduced_freq;
      else{
         /* For reg1 = opcode(reg2, reg3 ...), assume that reg3 is a
         pseudo register which has matching constraint on reg2,
         to save the register move, it's better to assign reg1
         to either of reg2 and reg3 (or one of other pseudos like
         reg3), it's reasonable to use freq for the cost.  But
         for conflict_cost, since reg2 and reg3 conflicts with
         each other, both of them has the chance to be assigned
         by reg1, assume reg3 has one copy which also conflicts
         with reg2, we shouldn't make it less preferred on reg1
         since reg3 has the same chance to be assigned by reg1.
         So it adjusts the conflic_cost to make it same as what
         we use for shuffle copy.  */
         cost = move_cost * freq;
         conflict_cost = move_cost * reduced_freq;
      }
   }else
      cost = conflict_cost = move_cost * freq;

   do{
      mtcs_ira_build_allocate_and_set_costs/*!ira_allocate_and_set_costs*/(mtcsIraBuild,&a->hard_reg_costs, aclass,
      a->class_cost);
      mtcs_ira_build_allocate_and_set_costs/*!ira_allocate_and_set_costs*/(mtcsIraBuild,&a->conflict_hard_reg_costs, aclass, 0);
      a->hard_reg_costs[index] -= cost;
      a->conflict_hard_reg_costs[index] -= conflict_cost;
      if (a->hard_reg_costs[index] < a->class_cost)
         a->class_cost = a->hard_reg_costs[index];
      mcs_ira_build_add_allocno_pref/*!ira_add_allocno_pref*/(mtcsIraBuild,a, allocno_preferenced_hard_regno, freq);
      a = mtcs_ira_allocno_parent_or_cap_allocno/*!ira_parent_or_cap_allocno*/(a);
   }while (a != NULL);
   return true;
}

/* Return true if output operand OUTPUT and input operand INPUT of
   INSN can use the same register class for at least one alternative.
   INSN is already described in recog_data and recog_op_alt.  */
static bool can_use_same_reg_p (MtcsIraConflicts *self,rtx_insn *insn, int output, int input)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);

   mtcs_alternative_mask preferred = mtcs_recog_get_preferred_alternatives/*!get_preferred_alternatives*/(mtcsRecog,insn);
   for (int nalt = 0; nalt < mtcsRecog->recog_data.n_alternatives; nalt++){
      if (!TEST_BIT (preferred, nalt))
         continue;

      const struct mtcs_operand_alternative *op_alt = &mtcsRecog->recog_op_alt[nalt * mtcsRecog->recog_data.n_operands];
      if (op_alt[input].matches == output)
         return true;

      if (op_alt[output].earlyclobber)
         continue;

      if (mtcsIraInt->x_ira_reg_class_intersect[op_alt[input].cl][op_alt[output].cl] != NO_REGS)
         return true;
   }
   return false;
}

/* Process all of the output registers of the current insn (INSN) which
   are not bound (BOUND_P) and the input register REG (its operand number
   OP_NUM) which dies in the insn as if there were a move insn between
   them with frequency FREQ.  */
static void process_reg_shuffles (MtcsIraConflicts *self,rtx_insn *insn, rtx reg, int op_num, int freq,
            bool *bound_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   int i;
   rtx another_reg;

   gcc_assert (REG_SUBREG_P (reg));
   for (i = 0; i < mtcsRecog->recog_data.n_operands; i++){
      another_reg = mtcsRecog->recog_data.operand[i];

      if (!REG_SUBREG_P (another_reg) || op_num == i
      || mtcsRecog->recog_data.operand_type[i] != OP_OUT
      || bound_p[i]
      || (!can_use_same_reg_p(self,insn, i, op_num)
      && (mtcsRecog->recog_data.constraints[op_num][0] != '%'
      || !can_use_same_reg_p(self,insn, i, op_num + 1))
      && (op_num == 0
      || mtcsRecog->recog_data.constraints[op_num - 1][0] != '%'
      || !can_use_same_reg_p(self,insn, i, op_num - 1))))
         continue;

      process_regs_for_copy(self,reg, another_reg, false, NULL, freq);
   }
}

/* Process INSN and create allocno copies if necessary.  For example,
   it might be because INSN is a pseudo-register move or INSN is two
   operand insn.  */
static void add_insn_allocno_copies (MtcsIraConflicts *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   rtx set, operand, dup;
   bool bound_p[MAX_RECOG_OPERANDS];
   int i, n, freq;
   alternative_mask alts;

   freq = REG_FREQ_FROM_BB (BLOCK_FOR_INSN (insn));
   if (freq == 0)
      freq = 1;
   if ((set = single_set (insn)) != NULL_RTX
   && REG_SUBREG_P (SET_DEST (set)) && REG_SUBREG_P (SET_SRC (set))
   && ! side_effects_p (set)
   && find_reg_note (insn, REG_DEAD,  REG_P (SET_SRC (set))
   ? SET_SRC (set) : SUBREG_REG (SET_SRC (set))) != NULL_RTX) {
      process_regs_for_copy(self,SET_SRC (set), SET_DEST (set),false, insn, freq);
      return;
   }
   /* Fast check of possibility of constraint or shuffle copies.  If
   there are no dead registers, there will be no such copies.  */
   if (! find_reg_note (insn, REG_DEAD, NULL_RTX))
      return;
   alts = ira_setup_alts (insn);
   for (i = 0; i < mtcsRecog->recog_data.n_operands; i++)
      bound_p[i] = false;
   for (i = 0; i < mtcsRecog->recog_data.n_operands; i++){
      operand = mtcsRecog->recog_data.operand[i];
      if (! REG_SUBREG_P (operand))
         continue;
      bool single_input_op_has_cstr_p;
      if ((n = ira_get_dup_out_num (i, alts, single_input_op_has_cstr_p)) >= 0){
         bound_p[n] = true;
         dup = mtcsRecog->recog_data.operand[n];
         if (REG_SUBREG_P (dup)
         && find_reg_note (insn, REG_DEAD, REG_P (operand) ? operand : SUBREG_REG (operand)) != NULL_RTX)
            process_regs_for_copy(self,operand, dup, true, NULL, freq,single_input_op_has_cstr_p);
      }
   }
   for (i = 0; i < mtcsRecog->recog_data.n_operands; i++){
      operand = mtcsRecog->recog_data.operand[i];
      if (REG_SUBREG_P (operand)
      && find_reg_note (insn, REG_DEAD,  REG_P (operand) ? operand : SUBREG_REG (operand)) != NULL_RTX){
         /* If an operand dies, prefer its hard register for the output
         operands by decreasing the hard register cost or creating
         the corresponding allocno copies.  The cost will not
         correspond to a real move insn cost, so make the frequency
         smaller.  */
         int new_freq = get_freq_for_shuffle_copy (freq);
         process_reg_shuffles(self,insn, operand, i, new_freq, bound_p);
      }
   }
}

/* Add copies originated from BB given by LOOP_TREE_NODE.  */
static void addCopies_cb (MtcsIraLoopTreeNode * loop_tree_node,void *userData)
{
   MtcsIraConflicts *self=(MtcsIraConflicts *)userData;
   basic_block bb;
   rtx_insn *insn;

   bb = loop_tree_node->bb;
   if (bb == NULL)
      return;
   FOR_BB_INSNS (bb, insn)
      if (NONDEBUG_INSN_P (insn))
         add_insn_allocno_copies(self,insn);
}

/* Propagate copies the corresponding allocnos on upper loop tree
   level.  */
static void propagate_copies (MtcsIraConflicts *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   MtcsIraAllocnoCopy *cp;
   MtcsIraAllocnoCopyIterator ci;
   MtcsIraAllocno * a1, *a2, *parent_a1, *parent_a2;

   MTCS_FOR_EACH_COPY (mtcsIraBuild,cp,ci){
      a1 = cp->first;
      a2 = cp->second;
      if (a1->loop_tree_node == mtcsIraBuild->ira_loop_tree_root)
         continue;
      ira_assert ((a2->loop_tree_node != mtcsIraBuild->ira_loop_tree_root));
      parent_a1 = mtcs_ira_allocno_parent_or_cap_allocno/*!ira_parent_or_cap_allocno*/(a1);
      parent_a2 =  mtcs_ira_allocno_parent_or_cap_allocno/*!ira_parent_or_cap_allocno*/(a2);
      ira_assert (parent_a1 != NULL && parent_a2 != NULL);
      if (! allocnos_conflict_for_copy_p(self,parent_a1, parent_a2))
         mtcs_ira_build_add_allocno_copy/*!ira_add_allocno_copy*/(mtcsIraBuild,parent_a1, parent_a2, cp->freq,
               cp->constraint_p, cp->insn, cp->loop_tree_node);
   }
}

/* Build conflict vectors or bit conflict vectors (whatever is more
   profitable) for object OBJ from the conflict table.  */
static void build_object_conflicts (MtcsIraConflicts *self,MtcsIraObject * obj)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int i, px, parent_num;
   MtcsIraAllocno * parent_a, *another_parent_a;
   MtcsIraObject * parent_obj;
   MtcsIraAllocno * a = obj->allocno;
   IRA_INT_TYPE *object_conflicts;
   minmax_set_iterator asi;
   int parent_min, parent_max ATTRIBUTE_UNUSED;

   object_conflicts = self->conflicts[obj->id];
   px = 0;
   FOR_EACH_BIT_IN_MINMAX_SET (object_conflicts, obj->min, obj->max, i, asi){
      MtcsIraObject * another_obj = mtcsIraBuild->ira_object_id_map[i];
      MtcsIraAllocno * another_a = obj->allocno;
      ira_assert (mtcsIra->x_ira_reg_classes_intersect_p[a->aclass][another_a->aclass]);
      self->collected_conflict_objects[px++] = another_obj;
   }
   if (mtcs_ira_object_ira_conflict_vector_profitable_p/*!ira_conflict_vector_profitable_p*/(obj, px)){
      MtcsIraObject * *vec;
      mtcs_ira_object_ira_allocate_conflict_vec/*!ira_allocate_conflict_vec*/(obj, px);
      vec = obj->conflicts_array;
      memcpy (vec, self->collected_conflict_objects, sizeof (MtcsIraObject *) * px);
      vec[px] = NULL;
      obj->num_accumulated_conflicts = px;
   }else{
      int conflict_bit_vec_words_num;

      obj->conflicts_array = object_conflicts;
      if (obj->max < obj->min)
         conflict_bit_vec_words_num = 0;
      else
         conflict_bit_vec_words_num = ((obj->max - obj->min + IRA_INT_BITS)/ IRA_INT_BITS);
      obj->conflicts_array_size = conflict_bit_vec_words_num * sizeof (IRA_INT_TYPE);
   }

   parent_a = mtcs_ira_allocno_parent_or_cap_allocno/*!ira_parent_or_cap_allocno*/(a);
   if (parent_a == NULL)
      return;
   ira_assert (a->aclass == parent_a->aclass);
   ira_assert (a->num_objects == parent_a->num_objects);
   parent_obj = parent_a->objects[obj->subword];
   parent_num = parent_obj->id;
   parent_min = parent_obj->min;
   parent_max = parent_obj->max;
   FOR_EACH_BIT_IN_MINMAX_SET (object_conflicts,obj->min, obj->max, i, asi){
      MtcsIraObject * another_obj =mtcsIraBuild->ira_object_id_map[i];
      MtcsIraAllocno * another_a = another_obj->allocno;
      int another_word = another_obj->subword;

      ira_assert (mtcsIra->x_ira_reg_classes_intersect_p[a->aclass][another_a->aclass]);

      another_parent_a = mtcs_ira_allocno_parent_or_cap_allocno/*!ira_parent_or_cap_allocno*/(another_a);
      if (another_parent_a == NULL)
         continue;
      ira_assert (another_parent_a->num >= 0);
      ira_assert (another_a->aclass == another_parent_a->aclass);
      ira_assert (another_a->num_objects == another_parent_a->num_objects);
      SET_MINMAX_SET_BIT (self->conflicts[parent_num],another_parent_a->objects[another_word]->id, parent_min, parent_max);
   }
}

/* Build conflict vectors or bit conflict vectors (whatever is more
   profitable) of all allocnos from the conflict table.  */
static void build_conflicts (MtcsIraConflicts *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int i;
   MtcsIraAllocno *a, *cap;

   self->collected_conflict_objects  = (MtcsIraObject **) ira_allocate (sizeof (MtcsIraObject *)
     * mtcsIraBuild->ira_objects_num);
   for (i = max_reg_num () - 1; i >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i--)
      for (a = mtcsIraBuild->ira_regno_allocno_map[i]; a != NULL; a = a->next_regno_allocno){
         int j, nregs = a->num_objects;
         for (j = 0; j < nregs; j++){
            MtcsIraObject * obj = a->objects[j];
            build_object_conflicts(self,obj);
            for (cap = a->cap; cap != NULL; cap = cap->cap){
               MtcsIraObject * cap_obj =cap->objects[j];
               gcc_assert (cap->num_objects == a->num_objects);
               build_object_conflicts(self,cap_obj);
            }
         }
      }
   ira_free (self->collected_conflict_objects);
}



/* Print hard reg set SET with TITLE to FILE.  */
static void print_hard_reg_set (MtcsIraConflicts *self,FILE *file, const char *title, HardRegSet *set)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);
   int i, start, end;

   fputs (title, file);
   for (start = end = -1, i = 0; i < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; i++){
      bool reg_included = mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(set, i);

      if (reg_included){
         if (start == -1)
            start = i;
         end = i;
      }
      if (start >= 0 && (!reg_included || i == firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/ - 1)){
         if (start == end)
            fprintf (file, " %d", start);
         else if (start == end + 1)
            fprintf (file, " %d %d", start, end);
         else
            fprintf (file, " %d-%d", start, end);
         start = -1;
      }
   }
   putc ('\n', file);
}

static void print_allocno_conflicts (MtcsIraConflicts *self,FILE * file, bool reg_p, MtcsIraAllocno * a)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);

   HardRegSet /*!HARD_REG_SET*/ conflicting_hard_regs = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};

   basic_block bb;
   int n, i;

   if (reg_p)
      fprintf (file, ";; r%d", a->regno);
   else{
      fprintf (file, ";; a%d(r%d,", a->num, a->regno);
      if ((bb = a->loop_tree_node->bb) != NULL)
         fprintf (file, "b%d", bb->index);
      else
         fprintf (file, "l%d", a->loop_tree_node->loop_num);
      putc (')', file);
   }

   fputs (" conflicts:", file);
   n = a->num_objects;
   for (i = 0; i < n; i++){
      MtcsIraObject * obj = a->objects[i];
      MtcsIraObject * conflict_obj;
      MtcsIraObjectConflictIterator oci;

      if (obj->conflicts_array == NULL){
         fprintf (file, "\n;;     total conflict hard regs:\n");
         fprintf (file, ";;     conflict hard regs:\n\n");
         continue;
      }

      if (n > 1)
         fprintf (file, "\n;;   subobject %d:", i);
      MTCS_FOR_EACH_OBJECT_CONFLICT (mtcsIraBuild,obj, conflict_obj, oci){
      MtcsIraAllocno * conflict_a = conflict_obj->allocno;
      if (reg_p)
         fprintf (file, " r%d,", conflict_a->regno);
      else{
         fprintf (file, " a%d(r%d", conflict_a->num, conflict_a->regno);
         if (conflict_a->num_objects > 1)
            fprintf (file, ",w%d", conflict_obj->subword);
         if ((bb = conflict_a->loop_tree_node->bb) != NULL)
            fprintf (file, ",b%d", bb->index);
         else
            fprintf (file, ",l%d", conflict_a->loop_tree_node->loop_num);
         putc (')', file);
      }
      }
      conflicting_hard_regs = (obj->total_conflict_hard_regs & ~mtcsIra->x_ira_no_alloc_regs
            & mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[a->aclass]);
      print_hard_reg_set(self,file, "\n;;     total conflict hard regs:", &conflicting_hard_regs);

      conflicting_hard_regs = (obj->conflict_hard_regs  & ~mtcsIra->x_ira_no_alloc_regs
            & mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[a->aclass]);
      print_hard_reg_set(self,file, ";;     conflict hard regs:", &conflicting_hard_regs);
      putc ('\n', file);
   }

}

/* Print information about allocno or only regno (if REG_P) conflicts
   to FILE.  */
static void print_conflicts (MtcsIraConflicts *self,FILE *file, bool reg_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);

   MtcsIraAllocno * a;
   MtcsIraAllocnoIterator ai;

   MTCS_FOR_EACH_ALLOCNO (mtcsIraBuild,a, ai)
      print_allocno_conflicts(self,file, reg_p, a);
   putc ('\n', file);
}

/* Print information about allocno or only regno (if REG_P) conflicts
   to stderr.  */
//原型 ira_debug_conflicts ira-int.h ira-conflicts.cc
void mtcs_ira_conflicts_debug_conflicts (MtcsIraConflicts *self,bool reg_p)
{
  print_conflicts(self,stderr, reg_p);
}



/* Entry function which builds allocno conflicts and allocno copies
   and accumulate some allocno info on upper level regions.  */
//原型 ira_build_conflicts ira-int.h ira-conflicts.cc
void mtcs_ira_conflicts_build_conflicts (MtcsIraConflicts *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);


   enum reg_class base;
   MtcsIraAllocno * a;
   MtcsIraAllocnoIterator ai;
   HardRegSet /*!HARD_REG_SET*/ temp_hard_reg_set = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};

   if (mtcsIra->ira_conflicts_p){
      mtcsIra->ira_conflicts_p = build_conflict_bit_table(self);
      if (mtcsIra->ira_conflicts_p){
         MtcsIraObject * obj;
         MtcsIraObjectIterator oi;

         build_conflicts(self);
         mtcs_ira_build_traverse_loop_tree/*!ira_traverse_loop_tree*/(mtcsIraBuild,true,
               mtcsIraBuild->ira_loop_tree_root, addCopies_cb, NULL,(void *)self);
         /* We need finished conflict table for the subsequent call.  */
         if (mtcsOptionsItem->x_flag_ira_region == IRA_REGION_ALL || mtcsOptionsItem->x_flag_ira_region  == IRA_REGION_MIXED)
            propagate_copies(self);

         /* Now we can free memory for the conflict table (see function
         build_object_conflicts for details).  */
         MTCS_FOR_EACH_OBJECT(mtcsIraBuild,obj, oi){
            if (obj->conflicts_array != self->conflicts[obj->id])
               ira_free (self->conflicts[obj->id]);
         }
         ira_free (self->conflicts);
      }
   }

   base = mtcs_reg_base_reg_class/*!base_reg_class*/(mtcsReg,VOIDmode, ADDR_SPACE_GENERIC, ADDRESS, SCRATCH);
   if (! mtcsTarget/*!targetm.class_likely_spilled_p*/->class_likely_spilled_p(mtcsTarget,base))
      mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&temp_hard_reg_set);
   else
      temp_hard_reg_set =  mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[base] & ~mtcsIra->x_ira_no_alloc_regs;

   MTCS_FOR_EACH_ALLOCNO(mtcsIraBuild,a, ai){
      int i, n = a->num_objects;

      for (i = 0; i < n; i++){
         MtcsIraObject * obj = a->objects[i];
         rtx allocno_reg = mtcsRtlData->regno_reg_rtx/*! regno_reg_rtx*/[a->regno];

         /* For debugging purposes don't put user defined variables in
         callee-clobbered registers.  However, do allow parameters
         in callee-clobbered registers to improve debugging.  This
         is a bit of a fragile hack.  */
         if (mtcsOptionsItem->x_optimize == 0 && REG_USERVAR_P (allocno_reg)  && ! reg_is_parm_p (allocno_reg)){
            HardRegSet new_conflict_regs = mtcsRtlData/*!crtl*/->abi->full_reg_clobbers ();
            obj->total_conflict_hard_regs |= new_conflict_regs;
            obj->conflict_hard_regs |= new_conflict_regs;
         }

         if (a->calls_crossed_num != 0){
            HardRegSet new_conflict_regs = mtcs_ira_allocno_need_caller_save_regs/*!ira_need_caller_save_regs*/(a);
            if (mtcsOptionsItem->x_flag_caller_saves)
               new_conflict_regs &= (~mtcsReg->hardRegs.x_savable_regs/*!savable_regs*/ | temp_hard_reg_set);
            obj->total_conflict_hard_regs |= new_conflict_regs;
            obj->conflict_hard_regs |= new_conflict_regs;
         }

         /* Now we deal with paradoxical subreg cases where certain registers
         cannot be accessed in the widest mode.  */
         machine_mode outer_mode = a->wmode;
         machine_mode inner_mode = a->mode;
         if (mtcs_mode_paradoxical_subreg_p/*!paradoxical_subreg_p*/(mtcsMode,outer_mode, inner_mode)){
            enum reg_class aclass = a->aclass;
            for (int j =mtcsIra->x_ira_class_hard_regs_num[aclass] - 1; j >= 0; --j){
               int inner_regno = mtcsIra->x_ira_class_hard_regs[aclass][j];
               int outer_regno = mtcs_rtl_simplify_subreg_regno/*!simplify_subreg_regno*/(mtcsRTL,inner_regno,
                      inner_mode, 0,outer_mode);
               if (outer_regno < 0
               || !mtcs_reg_in_hard_reg_set_p/*!in_hard_reg_set_p*/(mtcsReg,
                &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[aclass],outer_mode, outer_regno)){
                  mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&obj->total_conflict_hard_regs,inner_regno);
                  mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&obj->conflict_hard_regs,inner_regno);
               }
            }
         }
      }
   }

   if (mtcsOptionsItem->x_optimize && mtcsIra->ira_conflicts_p
   && mtcsIraGlobal->internal_flag_ira_verbose > 2 && mtcsIraGlobal->ira_dump_file != NULL)
      print_conflicts(self,mtcsIraGlobal->ira_dump_file, false);
}

static void mtcsIraConflictsInit(MtcsIraConflicts *self)
{

}


MtcsIraConflicts *mtcs_ira_conflictst_new(MtcsMode *mtcsMode)
{
   MtcsIraConflicts *self = n_slice_alloc0 (sizeof(MtcsIraConflicts));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsIraConflictsInit(self);
   return self;
}
