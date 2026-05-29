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
 * base on reload1.cc
 */


/* This file handles generation of all the assembler code
   *except* the instructions of a function.
   This includes declarations of variables and their initial values.

   We also output the assembler code for constants stored in reloadory
   and are responsible for combining constants with the same value.  */


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
#include "optabs.h"
#include "regs.h"
#include "ira.h"
#include "recog.h"
#include "rtl-error.h"
#include "reload.h"
#include "addresses.h"
#include "function-abi.h"
#include "reload.h"

#include "mtcsreload1.h"
#include "mtcsreg.h"
#include "mtcstarget.h"


static void mtcsReload1Init(MtcsReload1 *self)
{

}

/* Grow (or allocate) the REG_EQUIVS array from its current size (which may be
   zero elements) to MAX_REG_NUM elements.

   Initialize all new fields to NULL and update REG_EQUIVS_SIZE.  */
//原型 grow_reg_equivs reload.h reload1.cc
void mtcs_reload1_grow_reg_equivs (MtcsReload1 *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   int old_size = vec_safe_length (reg_equivs);
   int max_regno = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);
   int i;
   reg_equivs_t ze;

   memset (&ze, 0, sizeof (reg_equivs_t));
   vec_safe_reserve (reg_equivs, max_regno);
   for (i = old_size; i < max_regno; i++)
      reg_equivs->quick_insert (i, ze);
}

/* This function is called from the register allocator to set up estimates
   for the cost of eliminating pseudos which have REG_EQUIV equivalences to
   an invariant.  The structure is similar to calculate_needs_all_insns.  */
//原型 calculate_elim_costs_all_insns reload.h reload1.cc
//void mtcs_reload1_calculate_elim_costs_all_insns (MtcsReload1 *self)
//{
//  int *reg_equiv_init_cost;
//  basic_block bb;
//  int i;
//
//  reg_equiv_init_cost = XCNEWVEC (int, max_regno);
//  init_elim_table ();
//  init_eliminable_invariants (get_insns (), false);
//
//  set_initial_elim_offsets ();
//  set_initial_label_offsets ();
//
//  FOR_EACH_BB_FN (bb, cfun)
//    {
//      rtx_insn *insn;
//      elim_bb = bb;
//
//      FOR_BB_INSNS (bb, insn)
//   {
//     /* If this is a label, a JUMP_INSN, or has REG_NOTES (which might
//        include REG_LABEL_OPERAND and REG_LABEL_TARGET), we need to see
//        what effects this has on the known offsets at labels.  */
//
//     if (LABEL_P (insn) || JUMP_P (insn) || JUMP_TABLE_DATA_P (insn)
//         || (INSN_P (insn) && REG_NOTES (insn) != 0))
//       set_label_offsets (insn, insn, 0);
//
//     if (INSN_P (insn))
//       {
//         rtx set = single_set (insn);
//
//         /* Skip insns that only set an equivalence.  */
//         if (set && REG_P (SET_DEST (set))
//        && reg_renumber[REGNO (SET_DEST (set))] < 0
//        && (reg_equiv_constant (REGNO (SET_DEST (set)))
//            || reg_equiv_invariant (REGNO (SET_DEST (set)))))
//      {
//        unsigned regno = REGNO (SET_DEST (set));
//        rtx_insn_list *init = reg_equiv_init (regno);
//        if (init)
//          {
//            rtx t = eliminate_regs_1 (SET_SRC (set), VOIDmode, insn,
//                  false, true);
//            machine_mode mode = GET_MODE (SET_DEST (set));
//            int cost = set_src_cost (t, mode,
//                      optimize_bb_for_speed_p (bb));
//            int freq = REG_FREQ_FROM_BB (bb);
//
//            reg_equiv_init_cost[regno] = cost * freq;
//            continue;
//          }
//      }
//         /* If needed, eliminate any eliminable registers.  */
//         if (num_eliminable || num_eliminable_invariants)
//      elimination_costs_in_insn (insn);
//
//         if (num_eliminable)
//      update_eliminable_offsets ();
//       }
//   }
//    }
//  for (i = FIRST_PSEUDO_REGISTER; i < max_regno; i++)
//    {
//      if (reg_equiv_invariant (i))
//   {
//     if (reg_equiv_init (i))
//       {
//         int cost = reg_equiv_init_cost[i];
//         if (dump_file)
//      fprintf (dump_file,
//          "Reg %d has equivalence, initial gains %d\n", i, cost);
//         if (cost != 0)
//      ira_adjust_equiv_reg_cost (i, cost);
//       }
//     else
//       {
//         if (dump_file)
//      fprintf (dump_file,
//          "Reg %d had equivalence, but can't be eliminated\n",
//          i);
//         ira_adjust_equiv_reg_cost (i, 0);
//       }
//   }
//    }
//
//  free (reg_equiv_init_cost);
//  free (offsets_known_at);
//  free (offsets_at);
//  offsets_at = NULL;
//  offsets_known_at = NULL;
//}


/* Small utility function to set all regs in hard reg set TO which are
   allocated to pseudos in regset FROM.  */
//原型 compute_use_by_pseudos reload.h reload1.cc
void mtcs_reload1_compute_use_by_pseudos (MtcsReload1 *self,HardRegSet *to, regset from)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int i, j;
   int firstPseudoRegister= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   unsigned int regno;
   reg_set_iterator rsi;

   EXECUTE_IF_SET_IN_REG_SET (from, firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/, regno, rsi){
      int r = reg_renumber[regno];

      if (r < 0){
         /* reload_combine uses the information from DF_LIVE_IN,
         which might still contain registers that have not
         actually been allocated since they have an
         equivalence.  */
         gcc_assert (ira_conflicts_p || reload_completed);
      }else
         mtcs_reg_add_to_hard_reg_set/*!add_to_hard_reg_set*/(mtcsReg,
               to, mtcs_reg_get_pseudo_regno_bytes/*!PSEUDO_REGNO_MODE*/(mtcsReg,regno), r);
   }
}

/* Return true if the rtx X is invariant over the current function.  */
/* ??? Actually, the places where we use this expect exactly what is
tested here, and not everything that is function invariant.  In
particular, the frame pointer and arg pointer are special cased;
pic_offset_table_rtx is not, and we must not spill these things to
memory.  */
//原型 function_invariant_p rtl.h reload1.cc
bool  mtcs_reload1_function_invariant_p (MtcsReload1 *self,const_rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   if (CONSTANT_P (x))
      return 1;
   if (x == mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL)
   || x ==  mtcs_rtl_get_arg_pointer_rtx/*!arg_pointer_rtx*/(mtcsRTL))
      return 1;
   if (GET_CODE (x) == PLUS
   && (XEXP (x, 0) == mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL)
   || XEXP (x, 0) ==  mtcs_rtl_get_arg_pointer_rtx/*!arg_pointer_rtx*/(mtcsRTL))
   && GET_CODE (XEXP (x, 1)) == CONST_INT)
      return 1;
   return 0;
}


MtcsReload1 *mtcs_reload1_new(MtcsMode *mtcsMode)
{
     MtcsReload1 *self = n_slice_alloc0 (sizeof(MtcsReload1));
     mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
     mtcsReload1Init(self);
     return self;
}
