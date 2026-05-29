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


#ifndef __GCC_MTCS_IRA__
#define __GCC_MTCS_IRA__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "../mtcsmicro.h"
#include "mtcsiraallocno.h"


typedef struct _MtcsIra MtcsIra;


struct equivalence;//在mtcsira.c中定义
struct mtcs_ira_sloc;

struct _MtcsIra
{
    MtcsComponent parent;
    /* Map: hard register number -> allocno class it belongs to.  If the
       corresponding class is NO_REGS, the hard register is not available
       for allocation.  */
    enum reg_class x_ira_hard_regno_allocno_class[MAX_FIRST_PSEUDO_REGISTER/*!FIRST_PSEUDO_REGISTER*/];

    /* Number of allocno classes.  Allocno classes are register classes
       which can be used for allocations of allocnos.  */
    int x_ira_allocno_classes_num;

    /* The array containing allocno classes.  Only first
       IRA_ALLOCNO_CLASSES_NUM elements are used for this.  */
    enum reg_class x_ira_allocno_classes[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

    /* Map of all register classes to corresponding allocno classes
       containing the given class.  If given class is not a subset of an
       allocno class, we translate it into the cheapest allocno class.  */
    enum reg_class x_ira_allocno_class_translate[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

    /* Number of pressure classes.  Pressure classes are register
       classes for which we calculate register pressure.  */
    int x_ira_pressure_classes_num;

    /* The array containing pressure classes.  Only first
       IRA_PRESSURE_CLASSES_NUM elements are used for this.  */
    enum reg_class x_ira_pressure_classes[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

    /* Map of all register classes to corresponding pressure classes
       containing the given class.  If given class is not a subset of an
       pressure class, we translate it into the cheapest pressure
       class.  */
    enum reg_class x_ira_pressure_class_translate[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

    /* Biggest pressure register class containing stack registers.
       NO_REGS if there are no stack registers.  */
    enum reg_class x_ira_stack_reg_pressure_class;

    /* Maps: register class x machine mode -> maximal/minimal number of
       hard registers of given class needed to store value of given
       mode.  */
    unsigned char x_ira_reg_class_max_nregs[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/][MAX_MAX_MACHINE_MODE/*!MAX_MACHINE_MODE*/];
    unsigned char x_ira_reg_class_min_nregs[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/][MAX_MAX_MACHINE_MODE/*!MAX_MACHINE_MODE*/];

    /* Array analogous to target hook TARGET_MEMORY_MOVE_COST.  */
    short x_ira_memory_move_cost[MAX_MAX_MACHINE_MODE/*!MAX_MACHINE_MODE*/][MAX_N_REG_CLASSES/*!N_REG_CLASSES*/][2];

    /* Array of number of hard registers of given class which are
       available for the allocation.  The order is defined by the
       allocation order.  */
    short x_ira_class_hard_regs[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/][MAX_FIRST_PSEUDO_REGISTER/*!FIRST_PSEUDO_REGISTER*/];

    /* The number of elements of the above array for given register
       class.  */
    int x_ira_class_hard_regs_num[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

    /* Register class subset relation: TRUE if the first class is a subset
       of the second one considering only hard registers available for the
       allocation.  */
    int x_ira_class_subset_p[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/][MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

    /* The biggest class inside of intersection of the two classes (that
       is calculated taking only hard registers available for allocation
       into account.  If the both classes contain no hard registers
       available for allocation, the value is calculated with taking all
       hard-registers including fixed ones into account.  */
    enum reg_class x_ira_reg_class_subset[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/][MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

    /* True if the two classes (that is calculated taking only hard
       registers available for allocation into account; are
       intersected.  */
    bool x_ira_reg_classes_intersect_p[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/][MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

    /* If class CL has a single allocatable register of mode M,
       index [CL][M] gives the number of that register, otherwise it is -1.  */
    short x_ira_class_singleton[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/][MAX_MAX_MACHINE_MODE/*!MAX_MACHINE_MODE*/];

    /* Function specific hard registers cannot be used for the register
       allocation.  */
    HardRegSet/*!HARD_REG_SET*/ x_ira_no_alloc_regs;

    /* Array whose values are hard regset of hard registers available for
       the allocation of given register class whose targetm.hard_regno_mode_ok
       values for given mode are false.  */
    HardRegSet/*!HARD_REG_SET*/ x_ira_prohibited_class_mode_regs[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/][MAX_NUM_MACHINE_MODES/*!NUM_MACHINE_MODES*/];

    /* When an allocatable hard register in given mode can not be placed in given
       register class, it is in the set of the following array element.  It can
       happen only when given mode requires more one hard register.  */
    HardRegSet/*!HARD_REG_SET*/ x_ira_exclude_class_mode_regs[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/][MAX_NUM_MACHINE_MODES/*!NUM_MACHINE_MODES*/];
   //--------------------------以下是新加的成员-----------------------/
    /* Temporary hard reg set used for a different calculation.  */
    //原型 temp_hard_regset ira.cc
    HardRegSet/*!HARD_REG_SET*/ temp_hard_regset;

    /* Obstack used for storing all bitmaps of the IRA.  */
    //原型 ira_bitmap_obstack ira.cc
    struct bitmap_obstack ira_bitmap_obstack;

    /* Correspondingly overall cost of the allocation, overall cost before
       reload, cost of the allocnos assigned to hard-registers, cost of
       the allocnos assigned to memory, cost of loads, stores and register
       move insns generated for pseudo-register live range splitting (see
       ira-emit.cc).  */
    //ira emit color中引用 主要在ira emit中
    //原型 ira-int.h ira.cc
    int64_t ira_overall_cost, overall_cost_before;
    int64_t ira_reg_cost, ira_mem_cost;
    int64_t ira_load_cost, ira_store_cost, ira_shuffle_cost;
    int ira_move_loops_num, ira_additional_jumps_num;

    /* Value of max_reg_num () before IRA work start.  This value helps
       us to recognize a situation when new pseudos were created during
       IRA work.  */
    //原型 max_regno_before_ira ira-int.h ira.cc
    int max_regno_before_ira;
    //原型 eliminable_regset rtl.h ira.cc
    /* All registers that can be eliminated.  */
    HardRegSet/*!HARD_REG_SET*/ eliminable_regset;
    /* Order numbers of allocno classes in original target allocno class
       array, -1 for non-allocno classes.  */
    //原型 allocno_class_order ira.cc
    int allocno_class_order[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

    /* True when we use LRA instead of reload pass for the current
       function.  */
    //原型 ira_use_lra_p ira.h ira.cc
    bool ira_use_lra_p;
    /* True if we have allocno conflicts.  It is false for non-optimized
       mode or when the conflict table is too big.  */
    //原型 ira_conflicts_p ira.h ira.cc
    bool ira_conflicts_p;//ira build color conflicts reload1引用

    /* The length of the following array.  */
    //原型 ira_reg_equiv_len ira.h ira.cc
    int ira_reg_equiv_len;
    /* Info about equiv. info for each register.  */
    //原型 ira_reg_equiv ira.h
    struct ira_reg_equiv_s *ira_reg_equiv;

    /* The number of entries allocated in reg_info.  */
    //原型 allocated_reg_info_size ira.cc
    int allocated_reg_info_size;

    /* reg_equiv[N] (where N is a pseudo reg number) is the equivalence
       structure for that register.  */
    //原型 reg_equiv ira.cc
    struct equivalence *reg_equiv;

    /* Record the range of register numbers added by find_moveable_pseudos.  */
    //原型 int first_moveable_pseudo, last_moveable_pseudo; ira-int.h ira.cc
    int first_moveable_pseudo, last_moveable_pseudo;

    /* These two vectors hold data for every register added by
       find_movable_pseudos, with index 0 holding data for the
       first_moveable_pseudo.  */
    /* The original home register.  */
    //原型 pseudo_replaced_reg ira.cc
    vec<rtx> pseudo_replaced_reg;

    /* Code dealing with scratches (changing them onto
       pseudos and restoring them from the pseudos).

       We change scratches into pseudos at the beginning of IRA to
       simplify dealing with them (conflicts, hard register assignments).

       If the pseudo denoting scratch was spilled it means that we do not
       need a hard register for it.  Such pseudos are transformed back to
       scratches at the end of LRA.  */
    /* Locations of the former scratches.  */
    //原型 sloc_t ira.cc
    vec<struct mtcs_ira_sloc *> scratches;

    /* Bitmap of scratch regnos.  */
    //原型 scratch_bitmap ira.cc
    bitmap_head scratch_bitmap;

    /* Bitmap of scratch operands.   */
    //原型 scratch_operand_bitmap ira.cc
    bitmap_head scratch_operand_bitmap;

    /* Saved between IRA and reload.  */
    //原型 saved_flag_ira_share_spill_slots ira.cc
    int saved_flag_ira_share_spill_slots;
    /* Set to true while in IRA.  */
    //原型 ira_in_progress rtl.h ira.cc 只在文件ira.cc中出现
    bool ira_in_progress ;
};


MtcsIra *mtcs_ira_new(MtcsMode *mtcsMode);
//原型 ira_debug_disposition ira-int.h ira.cc
void mtcs_ira_debug_disposition (MtcsIra *self);
//原型 ira_print_disposition ira-int.h ira.cc
void mtcs_ira_print_disposition (MtcsIra *self,FILE *f);
//原型 ira_debug_allocno_classes ira-int.h ira.cc
void mtcs_ira_debug_allocno_classes (MtcsIra *self);
//原型 ira_init_register_move_cost ira-int.h ira.cc
void mtcs_ira_init_register_move_cost (MtcsIra *self,machine_mode mode);
//原型 ira_init_once ira.h ira.cc
void mtcs_ira_init_once (MtcsIra *self);
//原型 ira_init ira.h ira.cc
void mtcs_ira_init (MtcsIra *self);
//原型 ira_setup_alts ira-int.h ira.cc
alternative_mask mtcs_ira_setup_alts (MtcsIra *self ,rtx_insn *insn);
//原型 ira_get_dup_out_num ira-int.h ira.cc
int mtcs_ira_get_dup_out_num (MtcsIra *self,int op_num, alternative_mask alts,
           bool &single_input_op_has_cstr_p);
//原型 ira_bad_reload_regno ira.h ira.cc
bool mtcs_ira_bad_reload_regno (MtcsIra *self,int regno, rtx in, rtx out);
//原型 ira_setup_eliminable_regset ira.h ira.cc
void  mtcs_ira_setup_eliminable_regset (MtcsIra *self);
//原型 ira_update_equiv_info_by_shuffle_insn ira.h ira.cc
void mtcs_ira_update_equiv_info_by_shuffle_insn (MtcsIra *self,int to_regno, int from_regno, rtx_insn *insns);
//原型 ira_expand_reg_equiv ira.h ira.cc
void mtcs_ira_expand_reg_equiv (MtcsIra *self);
//原型 ira_former_scratch_p ira.h ira.cc
bool mtcs_ira_former_scratch_p (MtcsIra *self,int regno);
//原型 ira_former_scratch_operand_p ira.h ira.cc
bool mtcs_ira_former_scratch_operand_p (MtcsIra *self,rtx_insn *insn, int nop);
//原型 ira_register_new_scratch_op ira.h ira.cc
void mtcs_ira_register_new_scratch_op (MtcsIra *self,rtx_insn *insn, int nop, int icode);
//原型 ira_remove_insn_scratches ira.h ira.cc 原型没有 void *userData
bool mtcs_ira_remove_insn_scratches (MtcsIra *self,rtx_insn *insn, bool all_p, FILE *dump_file,
            rtx (*get_reg) (rtx original,void *userData),void *userData);
//原型 ira_restore_scratches ira.h ira.cc
void mtcs_ira_restore_scratches (MtcsIra *self,FILE *dump_file);
//原型 ira_nullify_asm_goto ira.h ira.cc
void mtcs_ira_nullify_asm_goto (MtcsIra *self,rtx_insn *insn);
//原型 non_spilled_static_chain_regno_p ira.h
bool mtcs_ira_non_spilled_static_chain_regno_p (MtcsIra *self,int regno);

//原型 ira_single_region_allocno_p ira-int.h
bool mtcs_ira_single_region_allocno_p (MtcsIra *self,MtcsIraAllocno *a, MtcsIraAllocno *subloop_a);
//原型 hard_reg_set_size ira-int.h
int mtcs_ira_hard_reg_set_size (MtcsIra *self, HardRegSet *set);
//原型 ira_hard_reg_in_set_p ira-int.h
bool mtcs_ira_hard_reg_in_set_p (MtcsIra *self,int hard_regno, machine_mode mode,HardRegSet *hard_regset);
//原型 ira_hard_reg_set_intersection_p ira-int.h
bool mtcs_ira_hard_reg_set_intersection_p (MtcsIra *self,int hard_regno, machine_mode mode,
             HardRegSet *hard_regset);
/* Initialize register costs for MODE if necessary.  */
//原型 ira_init_register_move_cost_if_necessary ira-int.h
void mtcs_ira_init_register_move_cost_if_necessary (MtcsIra *self,machine_mode mode);
//原型 ira_equiv_no_lvalue_p ira-int.h
bool mtcs_ira_equiv_no_lvalue_p (MtcsIra *self,int regno);


/* Return true if subloops that contain allocnos for A's register can
   use a different assignment from A.  ALLOCATED_P is true for the case
   in which allocation succeeded for A.  EXCLUDE_OLD_RELOAD is true if
   we should always return false for non-LRA targets.  (This is a hack
   and should be removed along with old reload.)  */
//原型 ira_subloop_allocnos_can_differ_p ira-int.h
bool mtcs_ira_subloop_allocnos_can_differ_p (MtcsIra *self,MtcsIraAllocno *a, bool allocated_p = true,
               bool exclude_old_reload = true);

/* Return the cost of saving a caller-saved register before each call
   in A's live range and restoring the same register after each call.  */
//原型 ira_caller_save_cost ira-int.h
inline int mtcs_ira_caller_save_cost (MtcsIra *self,MtcsIraAllocno *a)
{
  auto mode = a->mode;
  auto rclass = a->aclass;
  return (a->freq
     * (self->x_ira_memory_move_cost[mode][rclass][0]
        + self->x_ira_memory_move_cost[mode][rclass][1]));
}


/* A and SUBLOOP_A are allocnos for the same pseudo register, with A's
   loop immediately enclosing SUBLOOP_A's loop.  If we allocate to A a
   hard register R that is clobbered by a call in SUBLOOP_A, decide
   which of the following approaches should be used for handling the
   conflict:

   (1) Spill R on entry to SUBLOOP_A's loop, assign memory to SUBLOOP_A,
       and restore R on exit from SUBLOOP_A's loop.

   (2) Spill R before each necessary call in SUBLOOP_A's live range and
       restore R after each such call.

   Return true if (1) is better than (2).  SPILL_COST is the cost of
   doing (1).  */
//原型 ira_caller_save_loop_spill_p ira-int.h
inline bool mtcs_ira_caller_save_loop_spill_p (MtcsIra *self,MtcsIraAllocno *a, MtcsIraAllocno *subloop_a,int spill_cost)
{
  if (!mtcs_ira_subloop_allocnos_can_differ_p/*!ira_subloop_allocnos_can_differ_p*/(self,a))
    return false;

  /* Calculate the cost of saving a call-clobbered register
     before each call and restoring it afterwards.  */
  int call_cost = mtcs_ira_caller_save_cost/*!ira_caller_save_cost*/(self,subloop_a);
  return call_cost && call_cost >= spill_cost;
}


/* Represents the boundary between an allocno in one loop and its parent
   allocno in the enclosing loop.  It is usually possible to change a
   register's allocation on this boundary; the class provides routines
   for calculating the cost of such changes.  */
//原型 ira_loop_border_costs ira-int.h
class mtcs_ira_loop_border_costs
{
public:
   mtcs_ira_loop_border_costs (MtcsIra *mtcsIra,MtcsIraAllocno *a);

  int move_between_loops_cost () const;
  int spill_outside_loop_cost () const;
  int spill_inside_loop_cost () const;

private:
  /* The mode and class of the child allocno.  */
  machine_mode m_mode;
  reg_class m_class;
  MtcsIra *mtcsIra;

  /* Sums the frequencies of the entry edges and the exit edges.  */
  int m_entry_freq, m_exit_freq;
};

/* Return the cost of storing the register on entry to the loop and
   loading it back on exit from the loop.  This is the cost to use if
   the register is spilled within the loop but is successfully allocated
   in the parent loop.  */
inline int mtcs_ira_loop_border_costs::spill_inside_loop_cost () const
{
  return (m_entry_freq * mtcsIra->x_ira_memory_move_cost[m_mode][m_class][0]
     + m_exit_freq * mtcsIra->x_ira_memory_move_cost[m_mode][m_class][1]);
}

/* Return the cost of loading the register on entry to the loop and
   storing it back on exit from the loop.  This is the cost to use if
   the register is successfully allocated within the loop but is spilled
   in the parent loop.  */
inline int mtcs_ira_loop_border_costs::spill_outside_loop_cost () const
{
  return (m_entry_freq * mtcsIra->x_ira_memory_move_cost[m_mode][m_class][1]
     + m_exit_freq * mtcsIra->x_ira_memory_move_cost[m_mode][m_class][0]);
}




#endif
