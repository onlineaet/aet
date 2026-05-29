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
 * base on postreload.cc
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
#include "optabs.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"

#include "cfgrtl.h"
#include "cfgbuild.h"
#include "cfgcleanup.h"
#include "reload.h"
#include "cselib.h"
#include "tree-pass.h"
#include "dbgcnt.h"
#include "function-abi.h"
#include "rtl-iter.h"
#include "regs.h"

#include "mtcspostreload.h"
#include "../mtcstarget.h"
#include "../mtcscompile.h"



static bool reload_cse_simplify (MtcsPostReload *self,rtx_insn *, rtx);
static void reload_cse_regs_1 (MtcsPostReload *self);
static int reload_cse_simplify_set (MtcsPostReload *self,rtx, rtx_insn *);
static int reload_cse_simplify_operands (MtcsPostReload *self,rtx_insn *, rtx);

static void reload_combine (MtcsPostReload *self);
static void reload_combine_note_use (MtcsPostReload *self,rtx *, rtx_insn *, int, rtx);
static void reload_combine_note_store (rtx, const_rtx, void *);

static bool reload_cse_move2add (MtcsPostReload *self,rtx_insn *);
static void move2add_note_store (rtx, const_rtx, void *);

/* Call cse / combine like post-reload optimization phases.
   FIRST is the first instruction.  */
static void reload_cse_regs (MtcsPostReload *self,rtx_insn *first ATTRIBUTE_UNUSED)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   bool moves_converted;
   reload_cse_regs_1(self);
   reload_combine(self);
   moves_converted = reload_cse_move2add(self,first);
   if (mtcsOptionsItem->x_flag_expensive_optimizations){
      if (moves_converted)
         reload_combine(self);
      reload_cse_regs_1(self);
   }
}

/* Try to simplify INSN.  Return true if the CFG may have changed.  */
static bool reload_cse_simplify (MtcsPostReload *self,rtx_insn *insn, rtx testreg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsCseLib *mtcsCseLib = mtcs_target_get_cse_lib(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsRtlPassMgr  *mtcsRtlPassMgr = mtcs_target_get_rtl_pass_mgr(mtcsTarget);
   MtcsDse *mtcsDse = mtcs_rtl_pass_mgr_get_dse(mtcsRtlPassMgr);

   rtx stackPointerRtx = mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL);

   rtx body = PATTERN (insn);
   basic_block insn_bb = BLOCK_FOR_INSN (insn);
   unsigned insn_bb_succs = EDGE_COUNT (insn_bb->succs);

   /* If NO_FUNCTION_CSE has been set by the target, then we should not try
   to cse function calls.  */
   if (NO_FUNCTION_CSE && CALL_P (insn))
      return false;

   /* Remember if this insn has been sp += const_int.  */
   rtx sp_set = set_for_reg_notes (insn);
   rtx sp_addend = NULL_RTX;
   if (sp_set
   && SET_DEST (sp_set) == stackPointerRtx/*!stack_pointer_rtx*/
   && GET_CODE (SET_SRC (sp_set)) == PLUS
   && XEXP (SET_SRC (sp_set), 0) == stackPointerRtx/*!stack_pointer_rtx*/
   && CONST_INT_P (XEXP (SET_SRC (sp_set), 1)))
      sp_addend = XEXP (SET_SRC (sp_set), 1);

   if (GET_CODE (body) == SET){
      int count = 0;

      /* Simplify even if we may think it is a no-op.
      We may think a memory load of a value smaller than WORD_SIZE
      is redundant because we haven't taken into account possible
      implicit extension.  reload_cse_simplify_set() will bring
      this out, so it's safer to simplify before we delete.  */
      count += reload_cse_simplify_set(self,body, insn);

      if (!count && mtcs_cse_lib_cselib_redundant_set_p/*!cselib_redundant_set_p*/(mtcsCseLib,body)){
         if (check_for_inc_dec (insn))
            mtcs_cfg_rtl_delete_insn_and_edges/*!delete_insn_and_edges*/(mtcsCfgRtl,insn);
         /* We're done with this insn.  */
         goto done;
      }

      if (count > 0)
         apply_change_group ();
      else
         reload_cse_simplify_operands(self,insn, testreg);
   }else if (GET_CODE (body) == PARALLEL){
      int i;
      int count = 0;
      rtx value = NULL_RTX;

      /* Registers mentioned in the clobber list for an asm cannot be reused
      within the body of the asm.  Invalidate those registers now so that
      we don't try to substitute values for them.  */
      if (asm_noperands (body) >= 0){
         for (i = XVECLEN (body, 0) - 1; i >= 0; --i){
            rtx part = XVECEXP (body, 0, i);
            if (GET_CODE (part) == CLOBBER && REG_P (XEXP (part, 0)))
               mtcs_cse_lib_cselib_invalidate_rtx/*!cselib_invalidate_rtx*/(mtcsCseLib,XEXP (part, 0));
         }
      }

      /* If every action in a PARALLEL is a noop, we can delete
      the entire PARALLEL.  */
      for (i = XVECLEN (body, 0) - 1; i >= 0; --i){
         rtx part = XVECEXP (body, 0, i);
         if (GET_CODE (part) == SET){
            if (! mtcs_cse_lib_cselib_redundant_set_p/*!cselib_redundant_set_p*/(mtcsCseLib,part))
               break;
            if (REG_P (SET_DEST (part)) && REG_FUNCTION_VALUE_P (SET_DEST (part))){
               if (value)
                  break;
               value = SET_DEST (part);
            }
         }else if (GET_CODE (part) != CLOBBER && GET_CODE (part) != USE)
            break;
      }

      if (i < 0){
         if (mtcs_dse_check_for_inc_dec/*!check_for_inc_dec*/(mtcsDse,insn))
            mtcs_cfg_rtl_delete_insn_and_edges/*!delete_insn_and_edges*/(mtcsCfgRtl,insn);
         /* We're done with this insn.  */
         goto done;
      }

      /* It's not a no-op, but we can try to simplify it.  */
      for (i = XVECLEN (body, 0) - 1; i >= 0; --i)
         if (GET_CODE (XVECEXP (body, 0, i)) == SET)
            count += reload_cse_simplify_set(self,XVECEXP (body, 0, i), insn);

      if (count > 0)
         apply_change_group ();
      else
         reload_cse_simplify_operands(self,insn, testreg);
   }

   /* If sp += const_int insn is changed into sp = reg;, add REG_EQUAL
   note so that the stack_adjustments pass can undo it if beneficial.  */
   if (sp_addend   && SET_DEST (sp_set) == stackPointerRtx/*!stack_pointer_rtx*/   && REG_P (SET_SRC (sp_set)))
      mtcs_rtl_set_dst_reg_note/*!set_dst_reg_note*/(mtcsRTL,insn, REG_EQUAL, gen_rtx_PLUS (mtcs_mode_get_Pmode(mtcsMode),
            stackPointerRtx/*!stack_pointer_rtx*/, sp_addend), stackPointerRtx/*!stack_pointer_rtx*/);

done:
   return (EDGE_COUNT (insn_bb->succs) != insn_bb_succs);
}

/* Do a very simple CSE pass over the hard registers.

   This function detects no-op moves where we happened to assign two
   different pseudo-registers to the same hard register, and then
   copied one to the other.  Reload will generate a useless
   instruction copying a register to itself.

   This function also detects cases where we load a value from memory
   into two different registers, and (if memory is more expensive than
   registers) changes it to simply copy the first register into the
   second register.

   Another optimization is performed that scans the operands of each
   instruction to see whether the value is already available in a
   hard register.  It then replaces the operand with the hard register
   if possible, much like an optional reload would.  */
static void reload_cse_regs_1 (MtcsPostReload *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsCseLib *mtcsCseLib = mtcs_target_get_cse_lib(mtcsTarget);
   MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);
   MtcsCfgCleanup *mtcsCfgCleanup=mtcs_target_get_cfg_cleanup(mtcsTarget);

   bool cfg_changed = false;
   basic_block bb;
   rtx_insn *insn;
   rtx testreg = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mtcsMode->word_mode,
   mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg) + 1);

   mtcs_cse_lib_cselib_init/*!cselib_init*/(mtcsCseLib,CSELIB_RECORD_MEMORY);
   mtcs_alias_init_alias_analysis/*!init_alias_analysis*/(mtcsAlias);

   FOR_EACH_BB_FN (bb, cfun)
      FOR_BB_INSNS (bb, insn){
         if (INSN_P (insn))
            cfg_changed |= reload_cse_simplify(self,insn, testreg);

         mtcs_cse_lib_cselib_process_insn/*!cselib_process_insn*/(mtcsCseLib,insn);
      }

   /* Clean up.  */
   mtcs_alias_end_alias_analysis/*!end_alias_analysis*/(mtcsAlias);
   mtcs_cse_lib_cselib_finish/*!cselib_finish*/(mtcsCseLib);
   if (cfg_changed)
      mtcs_cfg_cleanup_cleanup_cfg/*!cleanup_cfg*/(mtcsCfgCleanup,0);
}

/* Try to simplify a single SET instruction.  SET is the set pattern.
   INSN is the instruction it came from.
   This function only handles one case: if we set a register to a value
   which is not a register, we try to find that value in some other register
   and change the set into a register copy.  */
static int reload_cse_simplify_set (MtcsPostReload *self,rtx set, rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsReload   *mtcsReload = mtcs_target_get_reload(mtcsTarget);
   MtcsRecog   *mtcsRecog = mtcs_target_get_recog(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsCseLib *mtcsCseLib = mtcs_target_get_cse_lib(mtcsTarget);

   int did_change = 0;
   int dreg;
   rtx src;
   reg_class_t dclass;
   int old_cost;
   cselib_val *val;
   struct elt_loc_list *l;
   enum rtx_code extend_op = UNKNOWN;
   bool speed = optimize_bb_for_speed_p (BLOCK_FOR_INSN (insn));

   dreg = mtcs_dojump_true_regnum/*!true_regnum*/(mtcsDojump,SET_DEST (set));
   if (dreg < 0)
      return 0;

   src = SET_SRC (set);
   if (side_effects_p (src) || mtcs_dojump_true_regnum/*!true_regnum*/(mtcsDojump,src) >= 0)
      return 0;

   dclass = mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,dreg);

   /* When replacing a memory with a register, we need to honor assumptions
   that combine made wrt the contents of sign bits.  We'll do this by
   generating an extend instruction instead of a reg->reg copy.  Thus
   the destination must be a register that we can widen.  */
   if (MEM_P (src)
   && (extend_op = mtcs_rtl_load_extend_op/*!load_extend_op*/(mtcsRTL,GET_MODE (src))) != UNKNOWN
   && !REG_P (SET_DEST (set)))
      return 0;

   val = mtcs_cse_lib_cselib_lookup/*!cselib_lookup*/(mtcsCseLib,src, GET_MODE (SET_DEST (set)), 0, VOIDmode);
   if (! val)
      return 0;

   /* If memory loads are cheaper than register copies, don't change them.  */
   if (MEM_P (src))
      old_cost = mtcs_reload_memory_move_cost/*!memory_move_cost*/(mtcsReload,GET_MODE (src), dclass, true);
   else if (REG_P (src))
      old_cost =mtcs_reload_register_move_cost/*!register_move_cost*/(mtcsReload,
            GET_MODE (src), mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,REGNO (src)), dclass);
   else
      old_cost = mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,src, GET_MODE (SET_DEST (set)), speed);

   for (l = val->locs; l; l = l->next){
      rtx this_rtx = l->loc;
      int this_cost;

      if (CONSTANT_P (this_rtx) && ! mtcs_cse_lib_references_value_p/*!references_value_p*/(mtcsCseLib,this_rtx, 0)){
         if (extend_op != UNKNOWN){
            wide_int result;

            if (!CONST_SCALAR_INT_P (this_rtx))
               continue;

            switch (extend_op){
               case ZERO_EXTEND:
                  result = wide_int::from (mtcs_rtx_mode_t/*!rtx_mode_t*/(this_rtx,GET_MODE (src)),BITS_PER_WORD, UNSIGNED);
                  break;
               case SIGN_EXTEND:
                  result = wide_int::from (mtcs_rtx_mode_t/*!rtx_mode_t*/(this_rtx, GET_MODE (src)),BITS_PER_WORD, SIGNED);
                  break;
               default:
                  gcc_unreachable ();
            }
            this_rtx = mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,result, word_mode);
         }

         this_cost = mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,this_rtx, GET_MODE (SET_DEST (set)), speed);
      }else if (REG_P (this_rtx)){
         if (extend_op != UNKNOWN){
            this_rtx = gen_rtx_fmt_e (extend_op, word_mode, this_rtx);
            this_cost = mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,this_rtx, word_mode, speed);
         }else
            this_cost = mtcs_reload_register_move_cost/*!register_move_cost*/(mtcsReload,
                  GET_MODE (this_rtx),mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,REGNO (this_rtx)),dclass);
      }else
         continue;

      /* If equal costs, prefer registers over anything else.  That
      tends to lead to smaller instructions on some machines.  */
      if (this_cost < old_cost || (this_cost == old_cost && REG_P (this_rtx) && !REG_P (SET_SRC (set)))){
         if (extend_op != UNKNOWN
               && mtcs_reg_can_change_mode/*!REG_CAN_CHANGE_MODE_P*/(mtcsReg,
                     REGNO (SET_DEST (set)),GET_MODE (SET_DEST (set)), word_mode)){
            rtx wide_dest = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,word_mode, REGNO (SET_DEST (set)));
            ORIGINAL_REGNO (wide_dest) = ORIGINAL_REGNO (SET_DEST (set));
            mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, &SET_DEST (set), wide_dest, 1);
         }

         mtcs_recog_validate_unshare_change/*!validate_unshare_change*/(mtcsRecog,insn, &SET_SRC (set), this_rtx, 1);
         old_cost = this_cost, did_change = 1;
      }
   }

   return did_change;
}

/* Try to replace operands in INSN with equivalent values that are already
   in registers.  This can be viewed as optional reloading.

   For each non-register operand in the insn, see if any hard regs are
   known to be equivalent to that operand.  Record the alternatives which
   can accept these hard registers.  Among all alternatives, select the
   ones which are better or equal to the one currently matching, where
   "better" is in terms of '?' and '!' constraints.  Among the remaining
   alternatives, select the one which replaces most operands with
   hard registers.  */
static int reload_cse_simplify_operands (MtcsPostReload *self,rtx_insn *insn, rtx testreg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsReload   *mtcsReload = mtcs_target_get_reload(mtcsTarget);
   MtcsRecog   *mtcsRecog = mtcs_target_get_recog(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsCseLib *mtcsCseLib = mtcs_target_get_cse_lib(mtcsTarget);
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);

   int i, j;
   int maxRecogOperands = mtcs_recog_get_max_recog_operands(mtcsRecog);
   int firstPseudoRegister= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);


   /* For each operand, all registers that are equivalent to it.  */
   HardRegSet /*!HARD_REG_SET*/ equiv_regs[maxRecogOperands/*!MAX_RECOG_OPERANDS*/];
   for(i=0;i<maxRecogOperands;i++)
      equiv_regs[i].count=mtcs_reg_get_hard_reg_element_count(mtcsReg);


   const char *constraints[maxRecogOperands/*!MAX_RECOG_OPERANDS*/];

   /* Vector recording how bad an alternative is.  */
   int *alternative_reject;
   /* Vector recording how many registers can be introduced by choosing
   this alternative.  */
   int *alternative_nregs;
   /* Array of vectors recording, for each operand and each alternative,
   which hard register to substitute, or -1 if the operand should be
   left as it is.  */
   int *op_alt_regno[maxRecogOperands/*!MAX_RECOG_OPERANDS*/];
   /* Array of alternatives, sorted in order of decreasing desirability.  */
   int *alternative_order;

   extract_constrain_insn (insn);

   if (mtcsRecog->recog_data.n_alternatives == 0 || mtcsRecog->recog_data.n_operands == 0)
      return 0;

   alternative_reject = XALLOCAVEC (int, mtcsRecog->recog_data.n_alternatives);
   alternative_nregs = XALLOCAVEC (int, mtcsRecog->recog_data.n_alternatives);
   alternative_order = XALLOCAVEC (int, mtcsRecog->recog_data.n_alternatives);
   memset (alternative_reject, 0, mtcsRecog->recog_data.n_alternatives * sizeof (int));
   memset (alternative_nregs, 0, mtcsRecog->recog_data.n_alternatives * sizeof (int));

   /* For each operand, find out which regs are equivalent.  */
   for (i = 0; i < mtcsRecog->recog_data.n_operands; i++){
      cselib_val *v;
      struct elt_loc_list *l;
      rtx op;

      mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&equiv_regs[i]);

      /* cselib blows up on CODE_LABELs.  Trying to fix that doesn't seem
      right, so avoid the problem here.  Similarly NOTE_INSN_DELETED_LABEL.
      Likewise if we have a constant and the insn pattern doesn't tell us
      the mode we need.  */
      if (LABEL_P (mtcsRecog->recog_data.operand[i])
      || (NOTE_P (mtcsRecog->recog_data.operand[i]) && NOTE_KIND (mtcsRecog->recog_data.operand[i]) == NOTE_INSN_DELETED_LABEL)
      || (CONSTANT_P (mtcsRecog->recog_data.operand[i]) && mtcsRecog->recog_data.operand_mode[i] == VOIDmode))
         continue;

      op = mtcsRecog->recog_data.operand[i];
      if (MEM_P (op) && mtcs_rtl_load_extend_op/*!load_extend_op*/(mtcsRTL,GET_MODE (op)) != UNKNOWN){
         rtx set = single_set (insn);

         /* We might have multiple sets, some of which do implicit
         extension.  Punt on this for now.  */
         if (! set)
            continue;
         /* If the destination is also a MEM or a STRICT_LOW_PART, no
         extension applies.
         Also, if there is an explicit extension, we don't have to
         worry about an implicit one.  */
         else if (MEM_P (SET_DEST (set))
         || GET_CODE (SET_DEST (set)) == STRICT_LOW_PART
         || GET_CODE (SET_SRC (set)) == ZERO_EXTEND
         || GET_CODE (SET_SRC (set)) == SIGN_EXTEND)
            ; /* Continue ordinary processing.  */
         /* If the register cannot change mode to word_mode, it follows that
         it cannot have been used in word_mode.  */
         else if (REG_P (SET_DEST (set)) && !mtcs_reg_can_change_mode/*!REG_CAN_CHANGE_MODE_P*/(mtcsReg,
               REGNO (SET_DEST (set)),GET_MODE (SET_DEST (set)), word_mode))
            ; /* Continue ordinary processing.  */
         /* If this is a straight load, make the extension explicit.  */
         else if (REG_P (SET_DEST (set))
         && mtcsRecog->recog_data.n_operands == 2
         && SET_SRC (set) == op
         && SET_DEST (set) == mtcsRecog->recog_data.operand[1-i]){
            mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,
                  insn, mtcsRecog->recog_data.operand_loc[i],gen_rtx_fmt_e (mtcs_rtl_load_extend_op/*!load_extend_op*/(mtcsRTL,
                        GET_MODE (op)), word_mode, op),1);
            mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,
                  insn, mtcsRecog->recog_data.operand_loc[1-i], mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,
                        word_mode, REGNO (SET_DEST (set))),1);
            if (! mtcs_recog_apply_change_group/*!apply_change_group*/(mtcsRecog))
               return 0;
            return reload_cse_simplify_operands(self,insn, testreg);
         }else
            /* ??? There might be arithmetic operations with memory that are
            safe to optimize, but is it worth the trouble?  */
            continue;
      }

      if (side_effects_p (op))
         continue;
      v = mtcs_cse_lib_cselib_lookup/*!cselib_lookup*/(mtcsCseLib,op, mtcsRecog->recog_data.operand_mode[i], 0, VOIDmode);
      if (! v)
         continue;

      for (l = v->locs; l; l = l->next)
         if (REG_P (l->loc))
            mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&equiv_regs[i], REGNO (l->loc));
   }

   mtcs_alternative_mask preferred =mtcs_recog_get_preferred_alternatives/*!get_preferred_alternatives*/(mtcsRecog,insn);
   for (i = 0; i < mtcsRecog->recog_data.n_operands; i++){
      machine_mode mode;
      int regno;
      const char *p;

      op_alt_regno[i] = XALLOCAVEC (int, mtcsRecog->recog_data.n_alternatives);
      for (j = 0; j < mtcsRecog->recog_data.n_alternatives; j++)
         op_alt_regno[i][j] = -1;

      p = constraints[i] = mtcsRecog->recog_data.constraints[i];
      mode = mtcsRecog->recog_data.operand_mode[i];

      /* Add the reject values for each alternative given by the constraints
      for this operand.  */
      j = 0;
      while (*p != '\0'){
         char c = *p++;
         if (c == ',')
            j++;
         else if (c == '?')
            alternative_reject[j] += 3;
         else if (c == '!')
            alternative_reject[j] += 300;
      }

      /* We won't change operands which are already registers.  We
      also don't want to modify output operands.  */
      regno = mtcs_dojump_true_regnum/*!true_regnum*/(mtcsDojump,mtcsRecog->recog_data.operand[i]);
      if (regno >= 0 || constraints[i][0] == '='  || constraints[i][0] == '+')
         continue;

      for (regno = 0; regno < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; regno++){
         enum reg_class rclass = NO_REGS;

         if (! mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&equiv_regs[i], regno))
            continue;

         mtcs_rtl_set_mode_and_regno/*!set_mode_and_regno*/(mtcsRTL,testreg, mode, regno);

         /* We found a register equal to this operand.  Now look for all
         alternatives that can accept this register and have not been
         assigned a register they can use yet.  */
         j = 0;
         p = constraints[i];
         for (;;){
            char c = *p;

            switch (c){
               case 'g':
                  rclass = mtcsReg->hardRegs.x_reg_class_subunion/*!reg_class_subunion*/[rclass]
                  [mtcs_reg_get_general_regs/*!GENERAL_REGS*/(mtcsReg)];
                  break;

               default:
                  rclass = (mtcsReg->hardRegs.x_reg_class_subunion/*!reg_class_subunion*/[rclass]
                                                                  [mtcs_preds_reg_class_for_constraint/*!reg_class_for_constraint*/(mtcsPreds,
                                                                        mtcs_preds_lookup_constraint/*!lookup_constraint*/(mtcsPreds,p))]);
                  break;

               case ',': case '\0':
                  /* See if REGNO fits this alternative, and set it up as the
                  replacement register if we don't have one for this
                  alternative yet and the operand being replaced is not
                  a cheap CONST_INT.  */
                  if (op_alt_regno[i][j] == -1
                  && TEST_BIT (preferred, j)
                  && mtcs_recog_reg_fits_class_p/*!reg_fits_class_p*/(mtcsRecog,testreg, rclass, 0, mode)
                  && (!CONST_INT_P (mtcsRecog->recog_data.operand[i])
                  || (mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,mtcsRecog->recog_data.operand[i], mode,
                  optimize_bb_for_speed_p (BLOCK_FOR_INSN (insn)))
                  > mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,testreg, mode,optimize_bb_for_speed_p (BLOCK_FOR_INSN (insn)))))){
                     alternative_nregs[j]++;
                     op_alt_regno[i][j] = regno;
                  }
                  j++;
                  rclass = NO_REGS;
                  break;
            }
            p += CONSTRAINT_LEN (c, p);

            if (c == '\0')
               break;
         }
      }
   }

   /* The loop below sets alternative_order[0] but -Wmaybe-uninitialized
   can't know that.  Clear it here to avoid the warning.  */
   alternative_order[0] = 0;
   gcc_assert (!mtcsRecog->recog_data.n_alternatives || (which_alternative >= 0
         && which_alternative < mtcsRecog->recog_data.n_alternatives));

   /* Record all alternatives which are better or equal to the currently
   matching one in the alternative_order array.  */
   for (i = j = 0; i < mtcsRecog->recog_data.n_alternatives; i++)
      if (alternative_reject[i] <= alternative_reject[which_alternative])
         alternative_order[j++] = i;

   mtcsRecog->recog_data.n_alternatives = j;

   /* Sort it.  Given a small number of alternatives, a dumb algorithm
   won't hurt too much.  */
   for (i = 0; i < mtcsRecog->recog_data.n_alternatives - 1; i++){
      int best = i;
      int best_reject = alternative_reject[alternative_order[i]];
      int best_nregs = alternative_nregs[alternative_order[i]];

      for (j = i + 1; j < mtcsRecog->recog_data.n_alternatives; j++){
         int this_reject = alternative_reject[alternative_order[j]];
         int this_nregs = alternative_nregs[alternative_order[j]];

         if (this_reject < best_reject || (this_reject == best_reject && this_nregs > best_nregs)){
            best = j;
            best_reject = this_reject;
            best_nregs = this_nregs;
         }
      }

      std::swap (alternative_order[best], alternative_order[i]);
   }

   /* Substitute the operands as determined by op_alt_regno for the best
   alternative.  */
   j = alternative_order[0];

   for (i = 0; i < mtcsRecog->recog_data.n_operands; i++){
      machine_mode mode = mtcsRecog->recog_data.operand_mode[i];
      if (op_alt_regno[i][j] == -1)
         continue;

      mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, mtcsRecog->recog_data.operand_loc[i],
            mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mode, op_alt_regno[i][j]), 1);
   }

   for (i = mtcsRecog->recog_data.n_dups - 1; i >= 0; i--){
      int op = mtcsRecog->recog_data.dup_num[i];
      machine_mode mode = mtcsRecog->recog_data.operand_mode[op];

      if (op_alt_regno[op][j] == -1)
         continue;

      mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, mtcsRecog->recog_data.dup_loc[i],
            mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mode, op_alt_regno[op][j]), 1);
   }

   return mtcs_recog_apply_change_group/*!apply_change_group*/(mtcsRecog);
}

#define LABEL_LIVE(LABEL) \
  (label_live[CODE_LABEL_NUMBER (LABEL) - min_labelno])

/* Subroutine of reload_combine_split_ruids, called to fix up a single
   ruid pointed to by *PRUID if it is higher than SPLIT_RUID.  */
static inline void reload_combine_split_one_ruid (int *pruid, int split_ruid)
{
  if (*pruid > split_ruid)
    (*pruid)++;
}

/* Called when we insert a new insn in a position we've already passed in
   the scan.  Examine all our state, increasing all ruids that are higher
   than SPLIT_RUID by one in order to make room for a new insn.  */
static void reload_combine_split_ruids (MtcsPostReload *self,int split_ruid)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int firstPseudoRegister= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   unsigned i;

   reload_combine_split_one_ruid (&self->reload_combine_ruid, split_ruid);
   reload_combine_split_one_ruid (&self->last_label_ruid, split_ruid);
   reload_combine_split_one_ruid (&self->last_jump_ruid, split_ruid);

   for (i = 0; i < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; i++){
      int j, idx = self->reg_state[i].use_index;
      reload_combine_split_one_ruid (&self->reg_state[i].use_ruid, split_ruid);
      reload_combine_split_one_ruid (&self->reg_state[i].store_ruid, split_ruid);
      reload_combine_split_one_ruid (&self->reg_state[i].real_store_ruid, split_ruid);
      if (idx < 0)
         continue;
      for (j = idx; j < MTCS_RELOAD_COMBINE_MAX_USES; j++){
         reload_combine_split_one_ruid (&self->reg_state[i].reg_use[j].ruid,split_ruid);
      }
   }
}

/* Called when we are about to rescan a previously encountered insn with
   reload_combine_note_use after modifying some part of it.  This clears all
   information about uses in that particular insn.  */
static void reload_combine_purge_insn_uses (MtcsPostReload *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int firstPseudoRegister= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   unsigned i;

   for (i = 0; i < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; i++){
      int j, k, idx = self->reg_state[i].use_index;
      if (idx < 0)
         continue;
      j = k = MTCS_RELOAD_COMBINE_MAX_USES;
      while (j-- > idx){
         if (self->reg_state[i].reg_use[j].insn != insn){
            k--;
            if (k != j)
               self->reg_state[i].reg_use[k] = self->reg_state[i].reg_use[j];
         }
      }
      self->reg_state[i].use_index = k;
   }
}

/* Called when we need to forget about all uses of REGNO after an insn
   which is identified by RUID.  */
static void reload_combine_purge_reg_uses_after_ruid (MtcsPostReload *self,unsigned regno, int ruid)
{
   int j, k, idx = self->reg_state[regno].use_index;
   if (idx < 0)
      return;
   j = k = MTCS_RELOAD_COMBINE_MAX_USES;
   while (j-- > idx){
      if (self->reg_state[regno].reg_use[j].ruid >= ruid){
         k--;
         if (k != j)
            self->reg_state[regno].reg_use[k] = self->reg_state[regno].reg_use[j];
      }
   }
   self->reg_state[regno].use_index = k;
}

/* Find the use of REGNO with the ruid that is highest among those
   lower than RUID_LIMIT, and return it if it is the only use of this
   reg in the insn.  Return NULL otherwise.  */
static struct reg_use *reload_combine_closest_single_use (MtcsPostReload *self,unsigned regno, int ruid_limit)
{
   int i, best_ruid = 0;
   int use_idx = self->reg_state[regno].use_index;
   struct reg_use *retval;

   if (use_idx < 0)
      return NULL;
   retval = NULL;
   for (i = use_idx; i < MTCS_RELOAD_COMBINE_MAX_USES; i++){
      struct reg_use *use = self->reg_state[regno].reg_use + i;
      int this_ruid = use->ruid;
      if (this_ruid >= ruid_limit)
         continue;
      if (this_ruid > best_ruid){
         best_ruid = this_ruid;
         retval = use;
      }else if (this_ruid == best_ruid)
         retval = NULL;
   }
   if (self->last_label_ruid >= best_ruid)
      return NULL;
   return retval;
}

/* After we've moved an add insn, fix up any debug insns that occur
   between the old location of the add and the new location.  REG is
   the destination register of the add insn; REPLACEMENT is the
   SET_SRC of the add.  FROM and TO specify the range in which we
   should make this change on debug insns.  */
static void fixup_debug_insns (MtcsPostReload *self,rtx reg, rtx replacement, rtx_insn *from, rtx_insn *to)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsRecog   *mtcsRecog = mtcs_target_get_recog(mtcsTarget);

   rtx_insn *insn;
   for (insn = from; insn != to; insn = NEXT_INSN (insn)){
      rtx t;

      if (!DEBUG_BIND_INSN_P (insn))
         continue;

      t = INSN_VAR_LOCATION_LOC (insn);
      t = mtcs_simplify_rtx_simplify_replace_rtx/*!simplify_replace_rtx*/(mtcsSimplifyRtx,t, reg, replacement);
      mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, &INSN_VAR_LOCATION_LOC (insn), t, 0);
   }
}

/* Subroutine of reload_combine_recognize_const_pattern.  Try to replace REG
   with SRC in the insn described by USE, taking costs into account.  Return
   true if we made the replacement.  */
static bool try_replace_in_use (MtcsPostReload *self,struct reg_use *use, rtx reg, rtx src)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsRecog   *mtcsRecog = mtcs_target_get_recog(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   rtx_insn *use_insn = use->insn;
   rtx mem = use->containing_mem;
   bool speed = optimize_bb_for_speed_p (BLOCK_FOR_INSN (use_insn));

   if (mem != NULL_RTX){
      addr_space_t as = mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,mem);
      rtx oldaddr = XEXP (mem, 0);
      rtx newaddr = NULL_RTX;
      int old_cost = mtcs_rtlanal_address_cost/*!address_cost*/(mtcsRtlanal,oldaddr, GET_MODE (mem), as, speed);
      int new_cost;

      newaddr = mtcs_simplify_rtx_simplify_replace_rtx/*!simplify_replace_rtx*/(mtcsSimplifyRtx,oldaddr, reg, src);
      if (mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/(mtcsRecog,GET_MODE (mem), newaddr, as)){
         XEXP (mem, 0) = newaddr;
         new_cost = mtcs_rtlanal_address_cost/*!address_cost*/(mtcsRtlanal,newaddr, GET_MODE (mem), as, speed);
         XEXP (mem, 0) = oldaddr;
         if (new_cost <= old_cost && mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,use_insn, &XEXP (mem, 0), newaddr, 0))
            return true;
      }
   }else{
      rtx new_set = single_set (use_insn);
      if (new_set
      && REG_P (SET_DEST (new_set))
      && GET_CODE (SET_SRC (new_set)) == PLUS
      && REG_P (XEXP (SET_SRC (new_set), 0))
      && CONSTANT_P (XEXP (SET_SRC (new_set), 1))){
         rtx new_src;
         machine_mode mode = GET_MODE (SET_DEST (new_set));
         int old_cost = mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,SET_SRC (new_set), mode, speed);

         gcc_assert (rtx_equal_p (XEXP (SET_SRC (new_set), 0), reg));
         new_src = mtcs_simplify_rtx_simplify_replace_rtx/*!simplify_replace_rtx*/(mtcsSimplifyRtx,SET_SRC (new_set), reg, src);

         if (mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,new_src, mode, speed) <= old_cost
         && mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,use_insn, &SET_SRC (new_set),new_src, 0))
            return true;
      }
   }
   return false;
}

/* Called by reload_combine when scanning INSN.  This function tries to detect
   patterns where a constant is added to a register, and the result is used
   in an address.
   Return true if no further processing is needed on INSN; false if it wasn't
   recognized and should be handled normally.  */
static bool reload_combine_recognize_const_pattern (MtcsPostReload *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsCseLib *mtcsCseLib = mtcs_target_get_cse_lib(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);

   int from_ruid = self->reload_combine_ruid;
   rtx set, pat, reg, src, addreg;
   unsigned int regno;
   struct reg_use *use;
   bool must_move_add;
   rtx_insn *add_moved_after_insn = NULL;
   int add_moved_after_ruid = 0;
   int clobbered_regno = -1;

   set = single_set (insn);
   if (set == NULL_RTX)
      return false;

   reg = SET_DEST (set);
   src = SET_SRC (set);
   if (!REG_P (reg)
   || REG_NREGS (reg) != 1
   || GET_MODE (reg) != mtcs_mode_get_Pmode(mtcsMode)
   || reg == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL))
      return false;

   regno = REGNO (reg);

   /* We look for a REG1 = REG2 + CONSTANT insn, followed by either
   uses of REG1 inside an address, or inside another add insn.  If
   possible and profitable, merge the addition into subsequent
   uses.  */
   if (GET_CODE (src) != PLUS  || !REG_P (XEXP (src, 0))  || !CONSTANT_P (XEXP (src, 1)))
      return false;

   addreg = XEXP (src, 0);
   must_move_add = rtx_equal_p (reg, addreg);

   pat = PATTERN (insn);
   if (must_move_add && set != pat){
      /* We have to be careful when moving the add; apart from the
      single_set there may also be clobbers.  Recognize one special
      case, that of one clobber alongside the set (likely a clobber
      of the CC register).  */
      gcc_assert (GET_CODE (PATTERN (insn)) == PARALLEL);
      if (XVECLEN (pat, 0) != 2 || XVECEXP (pat, 0, 0) != set
      || GET_CODE (XVECEXP (pat, 0, 1)) != CLOBBER
      || !REG_P (XEXP (XVECEXP (pat, 0, 1), 0)))
         return false;
      clobbered_regno = REGNO (XEXP (XVECEXP (pat, 0, 1), 0));
   }

   do{
      use = reload_combine_closest_single_use(self,regno, from_ruid);

      if (use)
         /* Start the search for the next use from here.  */
         from_ruid = use->ruid;

      if (use && GET_MODE (*use->usep) == mtcs_mode_get_Pmode(mtcsMode)){
         bool delete_add = false;
         rtx_insn *use_insn = use->insn;
         int use_ruid = use->ruid;

         /* Avoid moving the add insn past a jump.  */
         if (must_move_add && use_ruid <= self->last_jump_ruid)
            break;

         /* If the add clobbers another hard reg in parallel, don't move
         it past a real set of this hard reg.  */
         if (must_move_add && clobbered_regno >= 0 && self->reg_state[clobbered_regno].real_store_ruid >= use_ruid)
            break;

         gcc_assert (self->reg_state[regno].store_ruid <= use_ruid);
         /* Avoid moving a use of ADDREG past a point where it is stored.  */
         if (self->reg_state[REGNO (addreg)].store_ruid > use_ruid)
            break;

         /* We also must not move the addition past an insn that sets
         the same register, unless we can combine two add insns.  */
         if (must_move_add && self->reg_state[regno].store_ruid == use_ruid){
            if (use->containing_mem == NULL_RTX)
               delete_add = true;
            else
               break;
         }

         if (try_replace_in_use(self,use, reg, src)){
            reload_combine_purge_insn_uses(self,use_insn);
            reload_combine_note_use(self,&PATTERN (use_insn), use_insn, use_ruid, NULL_RTX);

            if (delete_add){
               fixup_debug_insns(self,reg, src, insn, use_insn);
               mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,insn);
               return true;
            }
            if (must_move_add){
               add_moved_after_insn = use_insn;
               add_moved_after_ruid = use_ruid;
            }
            continue;
         }
      }
      /* If we get here, we couldn't handle this use.  */
      if (must_move_add)
         break;
   }while (use);

   if (!must_move_add || add_moved_after_insn == NULL_RTX)
      /* Process the add normally.  */
      return false;

   fixup_debug_insns(self,reg, src, insn, add_moved_after_insn);

   mtcs_rtl_reorder_insns/*!reorder_insns*/(mtcsRTL,insn, insn, add_moved_after_insn);
   reload_combine_purge_reg_uses_after_ruid(self,regno, add_moved_after_ruid);
   reload_combine_split_ruids(self,add_moved_after_ruid - 1);
   reload_combine_note_use(self,&PATTERN (insn), insn,  add_moved_after_ruid, NULL_RTX);
   self->reg_state[regno].store_ruid = add_moved_after_ruid;

   return true;
}

/* Called by reload_combine when scanning INSN.  Try to detect a pattern we
   can handle and improve.  Return true if no further processing is needed on
   INSN; false if it wasn't recognized and should be handled normally.  */
static bool reload_combine_recognize_pattern (MtcsPostReload *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsCseLib *mtcsCseLib = mtcs_target_get_cse_lib(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRecog   *mtcsRecog = mtcs_target_get_recog(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);

   int indexRegClass = mtcs_reg_get_index_reg_class/*!INDEX_REG_CLASS*/(mtcsReg);
   rtx set, reg, src;

   set = single_set (insn);
   if (set == NULL_RTX)
      return false;

   reg = SET_DEST (set);
   src = SET_SRC (set);
   if (!REG_P (reg) || REG_NREGS (reg) != 1)
      return false;

   unsigned int regno = REGNO (reg);
   machine_mode mode = GET_MODE (reg);

   if (self->reg_state[regno].use_index < 0 || self->reg_state[regno].use_index >= MTCS_RELOAD_COMBINE_MAX_USES)
      return false;

   for (int i = self->reg_state[regno].use_index;   i < MTCS_RELOAD_COMBINE_MAX_USES; i++){
      struct reg_use *use = self->reg_state[regno].reg_use + i;
      if (GET_MODE (*use->usep) != mode)
         return false;
      /* Don't try to adjust (use (REGX)).  */
      if (GET_CODE (PATTERN (use->insn)) == USE  && &XEXP (PATTERN (use->insn), 0) == use->usep)
         return false;
   }

   /* Look for (set (REGX) (CONST_INT))
   (set (REGX) (PLUS (REGX) (REGY)))
   ...
   ... (MEM (REGX)) ...
   and convert it to
   (set (REGZ) (CONST_INT))
   ...
   ... (MEM (PLUS (REGZ) (REGY)))... .

   First, check that we have (set (REGX) (PLUS (REGX) (REGY)))
   and that we know all uses of REGX before it dies.
   Also, explicitly check that REGX != REGY; our life information
   does not yet show whether REGY changes in this insn.  */

   if (GET_CODE (src) == PLUS
   && self->reg_state[regno].all_offsets_match
   && self->last_index_reg != -1
   && REG_P (XEXP (src, 1))
   && rtx_equal_p (XEXP (src, 0), reg)
   && !rtx_equal_p (XEXP (src, 1), reg)
   && self->last_label_ruid < self->reg_state[regno].use_ruid){
      rtx base = XEXP (src, 1);
      rtx_insn *prev = prev_nonnote_nondebug_insn (insn);
      rtx prev_set = prev ? single_set (prev) : NULL_RTX;
      rtx index_reg = NULL_RTX;
      rtx reg_sum = NULL_RTX;
      int i;

      /* Now we need to set INDEX_REG to an index register (denoted as
      REGZ in the illustration above) and REG_SUM to the expression
      register+register that we want to use to substitute uses of REG
      (typically in MEMs) with.  First check REG and BASE for being
      index registers; we can use them even if they are not dead.  */
      if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(
      &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[indexRegClass/*!INDEX_REG_CLASS*/], regno)
      || mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(
      &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[indexRegClass/*!INDEX_REG_CLASS*/],REGNO (base)))
      {
         index_reg = reg;
         reg_sum = src;
      }else{
         /* Otherwise, look for a free index register.  Since we have
         checked above that neither REG nor BASE are index registers,
         if we find anything at all, it will be different from these
         two registers.  */
         for (i = self->first_index_reg; i <= self->last_index_reg; i++){
            if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(
                  &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[indexRegClass/*!INDEX_REG_CLASS*/], i)
            && self->reg_state[i].use_index == MTCS_RELOAD_COMBINE_MAX_USES
            && self->reg_state[i].store_ruid <= self->reg_state[regno].use_ruid
            && (mtcsRtlData/*!crtl*/->abi->clobbers_full_reg_p (i)
            || mtcs_dfscan_df_regs_ever_live_p/*!df_regs_ever_live_p*/(mtcsDfscan,i))
            && (!mtcsRtlData->x_frame_pointer_needed/*!frame_pointer_needed*/
                  || i != mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg))
            && !mtcsReg->hardRegs.x_fixed_regs/*!fixed_regs*/[i] && !mtcsReg->global_regs/*!global_regs*/[i]
            && mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,i, GET_MODE (reg)) == 1
            && mtcsTarget/*!targetm.hard_regno_scratch_ok*/->hard_regno_scratch_ok(mtcsTarget,i))
            {
               index_reg = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,GET_MODE (reg), i);
               reg_sum = gen_rtx_PLUS (GET_MODE (reg), index_reg, base);
               break;
            }
         }
      }

      /* Check that PREV_SET is indeed (set (REGX) (CONST_INT)) and that
      (REGY), i.e. BASE, is not clobbered before the last use we'll
      create.  */
      if (reg_sum
      && prev_set
      && CONST_INT_P (SET_SRC (prev_set))
      && rtx_equal_p (SET_DEST (prev_set), reg)
      && (self->reg_state[REGNO (base)].store_ruid <= self->reg_state[regno].use_ruid)){
         /* Change destination register and, if necessary, the constant
         value in PREV, the constant loading instruction.  */
         mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,prev, &SET_DEST (prev_set), index_reg, 1);
         if (self->reg_state[regno].offset != const0_rtx){
            HOST_WIDE_INT c  = mtcs_mode_trunc_int_for_mode/*!trunc_int_for_mode*/(mtcsMode,UINTVAL (SET_SRC (prev_set))
                                    + UINTVAL (self->reg_state[regno].offset), GET_MODE (index_reg));
            mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,prev, &SET_SRC (prev_set), GEN_INT (c), 1);
         }

         /* Now for every use of REG that we have recorded, replace REG
         with REG_SUM.  */
         for (i = self->reg_state[regno].use_index; i < MTCS_RELOAD_COMBINE_MAX_USES; i++)
            mtcs_recog_validate_unshare_change/*!validate_unshare_change*/(mtcsRecog,
                  self->reg_state[regno].reg_use[i].insn,self->reg_state[regno].reg_use[i].usep,
                  /* Each change must have its own replacement.  */reg_sum, 1);

         if (mtcs_recog_apply_change_group/*!apply_change_group*/(mtcsRecog)){
            struct reg_use *lowest_ruid = NULL;

            /* For every new use of REG_SUM, we have to record the use
            of BASE therein, i.e. operand 1.  */
            for (i = self->reg_state[regno].use_index; i < MTCS_RELOAD_COMBINE_MAX_USES; i++){
               struct reg_use *use = self->reg_state[regno].reg_use + i;
               reload_combine_note_use(self,&XEXP (*use->usep, 1), use->insn, use->ruid, use->containing_mem);
               if (lowest_ruid == NULL || use->ruid < lowest_ruid->ruid)
                  lowest_ruid = use;
            }

            fixup_debug_insns(self,reg, reg_sum, insn, lowest_ruid->insn);

            /* Delete the reg-reg addition.  */
            mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,insn);

            if (self->reg_state[regno].offset != const0_rtx)
               /* Previous REG_EQUIV / REG_EQUAL notes for PREV
               are now invalid.  */
               mtcs_rtlanal_remove_reg_equal_equiv_notes/*!remove_reg_equal_equiv_notes*/(mtcsRtlanal,prev);

            self->reg_state[regno].use_index = MTCS_RELOAD_COMBINE_MAX_USES;
            return true;
         }
      }
   }
   return false;
}

static void reload_combine (MtcsPostReload *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRecog   *mtcsRecog = mtcs_target_get_recog(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
   MtcsReload1 *mtcsReload1 = mtcs_target_get_reload1(mtcsTarget);
   MtcsFuncAbi  *mtcsFuncAbi = mtcs_target_get_func_abi(mtcsTarget);
   MtcsCfgBuild *mtcsCfgBuild=mtcs_target_get_cfg_build(mtcsTarget);

   int indexRegClass = mtcs_reg_get_index_reg_class/*!INDEX_REG_CLASS*/(mtcsReg);
   int firstPseudoRegister= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);


   rtx_insn *insn, *prev;
   basic_block bb;
   unsigned int r;
   int min_labelno, n_labels;
   // HARD_REG_SET ever_live_at_start, *label_live;
   HardRegSet ever_live_at_start ={mtcs_reg_get_hard_reg_element_count(mtcsReg)};
   HardRegSet *label_live;

   /* To avoid wasting too much time later searching for an index register,
   determine the minimum and maximum index register numbers.  */
   if (INDEX_REG_CLASS == NO_REGS)
      self->last_index_reg = -1;
   else if (self->first_index_reg == -1 && self->last_index_reg == 0){
      for (r = 0; r < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; r++)
         if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(
         &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[indexRegClass/*!INDEX_REG_CLASS*/], r))
         {
            if (self->first_index_reg == -1)
               self->first_index_reg = r;

            self->last_index_reg = r;
         }

      /* If no index register is available, we can quit now.  Set LAST_INDEX_REG
      to -1 so we'll know to quit early the next time we get here.  */
      if (self->first_index_reg == -1){
         self->last_index_reg = -1;
         return;
      }
   }

   /* Set up LABEL_LIVE and EVER_LIVE_AT_START.  The register lifetime
   information is a bit fuzzy immediately after reload, but it's
   still good enough to determine which registers are live at a jump
   destination.  */
   min_labelno = mtcs_rtl_data_get_first_label_num/*!get_first_label_num*/(mtcsRtlData);
   n_labels = mtcs_rtl_max_label_num/*!max_label_num*/(mtcsRTL) - min_labelno;
   label_live = XNEWVEC (HardRegSet, n_labels);
   mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&ever_live_at_start);

   FOR_EACH_BB_REVERSE_FN (bb, cfun){
      insn = BB_HEAD (bb);
      if (LABEL_P (insn)){
         HardRegSet live ={mtcs_reg_get_hard_reg_element_count(mtcsReg)};

         bitmap live_in = df_get_live_in (bb);

         mtcs_reg_reg_set_to_hard_reg_set/*!REG_SET_TO_HARD_REG_SET*/(mtcsReg,&live, live_in);
         mtcs_reload1_compute_use_by_pseudos/*!compute_use_by_pseudos*/(mtcsReload1,&live, live_in);
         LABEL_LIVE (insn) = live;
         ever_live_at_start |= live;
      }
   }

   /* Initialize self->last_label_ruid, self->reload_combine_ruid and self->reg_state.  */
   self->last_label_ruid = self->last_jump_ruid = self->reload_combine_ruid = 0;
   for (r = 0; r < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; r++){
      self->reg_state[r].store_ruid = 0;
      self->reg_state[r].real_store_ruid = 0;
      if (fixed_regs[r])
         self->reg_state[r].use_index = -1;
      else
         self->reg_state[r].use_index = MTCS_RELOAD_COMBINE_MAX_USES;
   }

   for (insn = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData); insn; insn = prev){
      bool control_flow_insn;
      rtx note;

      prev = PREV_INSN (insn);

      /* We cannot do our optimization across labels.  Invalidating all the use
      information we have would be costly, so we just note where the label
      is and then later disable any optimization that would cross it.  */
      if (LABEL_P (insn))
         self->last_label_ruid = self->reload_combine_ruid;
      else if (BARRIER_P (insn)){
         /* Crossing a barrier resets all the use information.  */
         for (r = 0; r < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; r++)
            if (! fixed_regs[r])
               self->reg_state[r].use_index = MTCS_RELOAD_COMBINE_MAX_USES;
      }else if (INSN_P (insn) && volatile_insn_p (PATTERN (insn)))
         /* Optimizations across insns being marked as volatile must be
         prevented.  All the usage information is invalidated
         here.  */
         for (r = 0; r < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; r++)
            if (! fixed_regs[r]  && self->reg_state[r].use_index != MTCS_RELOAD_COMBINE_MAX_USES)
               self->reg_state[r].use_index = -1;

      if (! NONDEBUG_INSN_P (insn))
         continue;

      self->reload_combine_ruid++;

      control_flow_insn = mtcs_cfg_build_control_flow_insn_p/*!control_flow_insn_p*/(mtcsCfgBuild,insn);
      if (control_flow_insn)
         self->last_jump_ruid = self->reload_combine_ruid;

      if (reload_combine_recognize_const_pattern(self,insn) || reload_combine_recognize_pattern(self,insn))
         continue;

      mtcs_rtlanal_note_stores/*!note_stores*/(mtcsRtlanal,insn, reload_combine_note_store, (void*)self/*!NULL*/);

      if (CALL_P (insn)){
         rtx link;
         HardRegSet used_regs = mtcs_func_abi_insn_callee_abi/*!insn_callee_abi*/(mtcsFuncAbi,insn).full_reg_clobbers ();

         for (r = 0; r < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; r++)
            if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&used_regs, r)){
               self->reg_state[r].use_index = MTCS_RELOAD_COMBINE_MAX_USES;
               self->reg_state[r].store_ruid = self->reload_combine_ruid;
            }

         for (link = CALL_INSN_FUNCTION_USAGE (insn); link; link = XEXP (link, 1)){
            rtx setuse = XEXP (link, 0);
            rtx usage_rtx = XEXP (setuse, 0);

            if (GET_CODE (setuse) == USE && REG_P (usage_rtx)){
               unsigned int end_regno = END_REGNO (usage_rtx);
               for (unsigned int i = REGNO (usage_rtx); i < end_regno; ++i)
                  self->reg_state[i].use_index = -1;
            }
         }
      }

      if (control_flow_insn && !ANY_RETURN_P (PATTERN (insn))){
         /* Non-spill registers might be used at the call destination in
         some unknown fashion, so we have to mark the unknown use.  */
         HardRegSet *live;

         if ((condjump_p (insn) || condjump_in_parallel_p (insn))  && JUMP_LABEL (insn)){
            if (ANY_RETURN_P (JUMP_LABEL (insn)))
               live = NULL;
            else
               live = &LABEL_LIVE (JUMP_LABEL (insn));
         } else
            live = &ever_live_at_start;

         if (live)
            for (r = 0; r < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; r++)
               if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(live, r))
                  self->reg_state[r].use_index = -1;
      }

      reload_combine_note_use(self,&PATTERN (insn), insn, self->reload_combine_ruid, NULL_RTX);

      for (note = REG_NOTES (insn); note; note = XEXP (note, 1)){
         if (REG_NOTE_KIND (note) == REG_INC && REG_P (XEXP (note, 0))){
            int regno = REGNO (XEXP (note, 0));
            self->reg_state[regno].store_ruid = self->reload_combine_ruid;
            self->reg_state[regno].real_store_ruid = self->reload_combine_ruid;
            self->reg_state[regno].use_index = -1;
         }
      }
   }

   free (label_live);
}

/* Check if DST is a register or a subreg of a register; if it is,
   update store_ruid, real_store_ruid and use_index in the self->reg_state
   structure accordingly.  Called via note_stores from reload_combine.  */
static void reload_combine_note_store (rtx dst, const_rtx set, void *data ATTRIBUTE_UNUSED)
{
   MtcsPostReload *self=(MtcsPostReload *)data;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsReg *mtcsReg =mtcs_target_get_reg(mtcsTarget);

   int regno = 0;
   int i;
   machine_mode mode = GET_MODE (dst);

   if (GET_CODE (dst) == SUBREG){
      regno = mtcs_rtlanal_subreg_regno_offset/*!subreg_regno_offset*/(mtcsRtlanal,REGNO (SUBREG_REG (dst)),
      GET_MODE (SUBREG_REG (dst)),  SUBREG_BYTE (dst),  GET_MODE (dst));
      dst = SUBREG_REG (dst);
   }

   /* Some targets do argument pushes without adding REG_INC notes.  */

   if (MEM_P (dst)){
      dst = XEXP (dst, 0);
      if (GET_CODE (dst) == PRE_INC || GET_CODE (dst) == POST_INC
      || GET_CODE (dst) == PRE_DEC || GET_CODE (dst) == POST_DEC
      || GET_CODE (dst) == PRE_MODIFY || GET_CODE (dst) == POST_MODIFY){
         unsigned int end_regno = END_REGNO (XEXP (dst, 0));
         for (unsigned int i = REGNO (XEXP (dst, 0)); i < end_regno; ++i){
            /* We could probably do better, but for now mark the register
            as used in an unknown fashion and set/clobbered at this
            insn.  */
            self->reg_state[i].use_index = -1;
            self->reg_state[i].store_ruid = self->reload_combine_ruid;
            self->reg_state[i].real_store_ruid = self->reload_combine_ruid;
         }
      }else
         return;
   }

   if (!REG_P (dst))
      return;
   regno += REGNO (dst);

   /* note_stores might have stripped a STRICT_LOW_PART, so we have to be
   careful with registers / register parts that are not full words.
   Similarly for ZERO_EXTRACT.  */
   if (GET_CODE (SET_DEST (set)) == ZERO_EXTRACT || GET_CODE (SET_DEST (set)) == STRICT_LOW_PART){
      for (i = mtcs_reg_end_hard_regno/*!end_hard_regno*/(mtcsReg,mode, regno) - 1; i >= regno; i--){
         self->reg_state[i].use_index = -1;
         self->reg_state[i].store_ruid = self->reload_combine_ruid;
         self->reg_state[i].real_store_ruid = self->reload_combine_ruid;
      }
   }else{
      for (i = mtcs_reg_end_hard_regno/*!end_hard_regno*/(mtcsReg,mode, regno) - 1; i >= regno; i--){
         self->reg_state[i].store_ruid = self->reload_combine_ruid;
         if (GET_CODE (set) == SET)
            self->reg_state[i].real_store_ruid = self->reload_combine_ruid;
         self->reg_state[i].use_index = MTCS_RELOAD_COMBINE_MAX_USES;
      }
   }
}

/* XP points to a piece of rtl that has to be checked for any uses of
   registers.
   *XP is the pattern of INSN, or a part of it.
   Called from reload_combine, and recursively by itself.  */
static void reload_combine_note_use (MtcsPostReload *self,rtx *xp, rtx_insn *insn, int ruid, rtx containing_mem)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int firstPseudoRegister= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   rtx x = *xp;
   enum rtx_code code = x->code;
   const char *fmt;
   int i, j;
   rtx offset = const0_rtx; /* For the REG case below.  */

   switch (code){
      case SET:
         if (REG_P (SET_DEST (x))){
            reload_combine_note_use(self,&SET_SRC (x), insn, ruid, NULL_RTX);
            return;
         }
         break;

      case USE:
         /* If this is the USE of a return value, we can't change it.  */
         if (REG_P (XEXP (x, 0)) && REG_FUNCTION_VALUE_P (XEXP (x, 0))){
            /* Mark the return register as used in an unknown fashion.  */
            rtx reg = XEXP (x, 0);
            unsigned int end_regno = END_REGNO (reg);
            for (unsigned int regno = REGNO (reg); regno < end_regno; ++regno)
               self->reg_state[regno].use_index = -1;
            return;
         }
         break;

      case CLOBBER:
         if (REG_P (SET_DEST (x))){
            /* No spurious CLOBBERs of pseudo registers may remain.  */
            gcc_assert (REGNO (SET_DEST (x)) < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/);
            return;
         }
         break;

      case PLUS:
         /* We are interested in (plus (reg) (const_int)) .  */
         if (!REG_P (XEXP (x, 0)) || !CONST_INT_P (XEXP (x, 1)))
            break;
         offset = XEXP (x, 1);
         x = XEXP (x, 0);
      /* Fall through.  */
      case REG:
      {
         int regno = REGNO (x);
         int use_index;
         int nregs;

         /* No spurious USEs of pseudo registers may remain.  */
         gcc_assert (regno < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/);

         nregs = REG_NREGS (x);

         /* We can't substitute into multi-hard-reg uses.  */
         if (nregs > 1){
            while (--nregs >= 0)
               self->reg_state[regno + nregs].use_index = -1;
            return;
         }

         /* We may be called to update uses in previously seen insns.
         Don't add uses beyond the last store we saw.  */
         if (ruid < self->reg_state[regno].store_ruid)
            return;

         /* If this register is already used in some unknown fashion, we
         can't do anything.
         If we decrement the index from zero to -1, we can't store more
         uses, so this register becomes used in an unknown fashion.  */
         use_index = --self->reg_state[regno].use_index;
         if (use_index < 0)
            return;

         if (use_index == MTCS_RELOAD_COMBINE_MAX_USES - 1){
            /* This is the first use of this register we have seen since we
            marked it as dead.  */
            self->reg_state[regno].offset = offset;
            self->reg_state[regno].all_offsets_match = true;
            self->reg_state[regno].use_ruid = ruid;
         }else{
            if (self->reg_state[regno].use_ruid > ruid)
               self->reg_state[regno].use_ruid = ruid;

            if (! rtx_equal_p (offset, self->reg_state[regno].offset))
               self->reg_state[regno].all_offsets_match = false;
         }

         self->reg_state[regno].reg_use[use_index].insn = insn;
         self->reg_state[regno].reg_use[use_index].ruid = ruid;
         self->reg_state[regno].reg_use[use_index].containing_mem = containing_mem;
         self->reg_state[regno].reg_use[use_index].usep = xp;
         return;
      }

      case MEM:
         containing_mem = x;
         break;

      default:
         break;
   }

   /* Recursively process the components of X.  */
   fmt = GET_RTX_FORMAT (code);
   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--){
      if (fmt[i] == 'e')
         reload_combine_note_use(self,&XEXP (x, i), insn, ruid, containing_mem);
      else if (fmt[i] == 'E'){
         for (j = XVECLEN (x, i) - 1; j >= 0; j--)
            reload_combine_note_use(self,&XVECEXP (x, i, j), insn, ruid, containing_mem);
      }
   }
}


/* ??? We don't know how zero / sign extension is handled, hence we
   can't go from a narrower to a wider mode.  */
#define MODES_OK_FOR_MOVE2ADD(OUTMODE, INMODE) \
  (mtcs_mode_get_size(mtcsMode,OUTMODE) == mtcs_mode_get_size(mtcsMode,INMODE) \
   || (mtcs_mode_get_size(mtcsMode,OUTMODE) <= mtcs_mode_get_size(mtcsMode,INMODE) \
       && mtcs_mode_truly_noop_truncation_p(mtcsMode,OUTMODE, INMODE)))

/* Record that REG is being set to a value with the mode of REG.  */

static void move2add_record_mode (MtcsPostReload *self,rtx reg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   int regno, nregs;
   machine_mode mode = GET_MODE (reg);

   if (GET_CODE (reg) == SUBREG){
      regno = mtcs_rtlanal_subreg_regno/*!subreg_regno*/(mtcsRtlanal,reg);
      nregs = mtcs_rtlanal_subreg_nregs/*!subreg_nregs*/(mtcsRtlanal,reg);
   }else if (REG_P (reg)){
      regno = REGNO (reg);
      nregs = REG_NREGS (reg);
   }else
      gcc_unreachable ();
   for (int i = nregs - 1; i > 0; i--)
      self->reg_mode[regno + i] = mtcsMode->modes.M_BLKmode;
   self->reg_mode[regno] = mode;
}

/* Record that REG is being set to the sum of SYM and OFF.  */
static void move2add_record_sym_value (MtcsPostReload *self,rtx reg, rtx sym, rtx off)
{
   int regno = REGNO (reg);

   move2add_record_mode(self,reg);
   self->reg_set_luid[regno] = self->move2add_luid;
   self->reg_base_reg[regno] = -1;
   self->reg_symbol_ref[regno] = sym;
   self->reg_offset[regno] = INTVAL (off);
}

/* Check if REGNO contains a valid value in MODE.  */
static bool move2add_valid_value_p (MtcsPostReload *self,int regno, scalar_int_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   if (self->reg_set_luid[regno] <= self->move2add_last_label_luid)
      return false;

   if (mode != self->reg_mode[regno]){
      scalar_int_mode old_mode;
      if (!mtcs_mode_is_a <scalar_int_mode>(mtcsMode,self->reg_mode[regno], &old_mode)
      || !MODES_OK_FOR_MOVE2ADD (mode, old_mode)
      || !mtcs_reg_can_change_mode/*!REG_CAN_CHANGE_MODE_P*/(mtcsReg,regno, old_mode, mode))
         return false;
      /* The value loaded into regno in self->reg_mode[regno] is also valid in
      mode after truncation only if (REG:mode regno) is the lowpart of
      (REG:self->reg_mode[regno] regno).  Now, for big endian, the starting
      regno of the lowpart might be different.  */
      poly_int64 s_off = mtcs_mode_subreg_lowpart_offset/*!subreg_lowpart_offset*/(mtcsMode,mode, old_mode);
      s_off = mtcs_rtlanal_subreg_regno_offset/*!subreg_regno_offset*/(mtcsRtlanal,regno, old_mode, s_off, mode);
      if (maybe_ne (s_off, 0))
         /* We could in principle adjust regno, check self->reg_mode[regno] to be
         BLKmode, and return s_off to the caller (vs. -1 for failure),
         but we currently have no callers that could make use of this
         information.  */
         return false;
   }

   for (int i = mtcs_reg_end_hard_regno/*!end_hard_regno*/(mtcsReg,mode, regno) - 1; i > regno; i--)
      if (self->reg_mode[i] != mtcsMode->modes.M_BLKmode)
         return false;
   return true;
}

/* This function is called with INSN that sets REG (of mode MODE)
   to (SYM + OFF), while REG is known to already have value (SYM + offset).
   This function tries to change INSN into an add instruction
   (set (REG) (plus (REG) (OFF - offset))) using the known value.
   It also updates the information about REG's known value.
   Return true if we made a change.  */
static bool move2add_use_add2_insn (MtcsPostReload *self,scalar_int_mode mode, rtx reg, rtx sym, rtx off,
         rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRecog   *mtcsRecog = mtcs_target_get_recog(mtcsTarget);

   rtx set = single_set (insn);
   rtx src = SET_SRC (set);
   int regno = REGNO (reg);
   rtx new_src = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,UINTVAL (off) - self->reg_offset[regno], mode);
   bool speed = optimize_bb_for_speed_p (BLOCK_FOR_INSN (insn));
   bool changed = false;

   /* (set (reg) (plus (reg) (const_int 0))) is not canonical;
   use (set (reg) (reg)) instead.
   We don't delete this insn, nor do we convert it into a
   note, to avoid losing register notes or the return
   value flag.  jump2 already knows how to get rid of
   no-op moves.  */
   if (new_src == const0_rtx){
      /* If the constants are different, this is a
      truncation, that, if turned into (set (reg)
      (reg)), would be discarded.  Maybe we should
      try a truncMN pattern?  */
      if (INTVAL (off) == self->reg_offset [regno])
         changed = mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, &SET_SRC (set), reg, 0);
      }else{
         struct full_rtx_costs oldcst, newcst;
         rtx tem = gen_rtx_PLUS (mode, reg, new_src);

         mtcs_rtlanal_get_full_set_rtx_cost/*!get_full_set_rtx_cost*/(mtcsRtlanal,set, &oldcst);
         SET_SRC (set) = tem;
         mtcs_rtlanal_get_full_set_rtx_cost/*!get_full_set_rtx_cost*/(mtcsRtlanal,set, &newcst);
         SET_SRC (set) = src;

         if (costs_lt_p (&newcst, &oldcst, speed)  && have_add2_insn (reg, new_src))
            changed = mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, &SET_SRC (set), tem, 0);
         else if (sym == NULL_RTX && mode != mtcsMode->modes.M_BImode){
            scalar_int_mode narrow_mode;
            FOR_EACH_MODE_UNTIL (narrow_mode, mode){
            if (have_insn_for (STRICT_LOW_PART, narrow_mode)
            && ((self->reg_offset[regno] & ~mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,narrow_mode))
            == (INTVAL (off) & ~mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,narrow_mode)))){
               rtx narrow_reg = mtcs_rtl_gen_lowpart_common/*!gen_lowpart_common*/(mtcsRTL,narrow_mode, reg);
               rtx narrow_src = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,INTVAL (off),narrow_mode);
               rtx new_set = gen_rtx_SET (gen_rtx_STRICT_LOW_PART (VOIDmode,narrow_reg),narrow_src);
               mtcs_rtlanal_get_full_set_rtx_cost/*!get_full_set_rtx_cost*/(mtcsRtlanal,new_set, &newcst);

               /* We perform this replacement only if NEXT is either a
               naked SET, or else its single_set is the first element
               in a PARALLEL.  */
               rtx *setloc = GET_CODE (PATTERN (insn)) == PARALLEL ? &XVECEXP (PATTERN (insn), 0, 0) : &PATTERN (insn);
               if (*setloc == set && costs_lt_p (&newcst, &oldcst, speed)){
                  changed = mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, setloc, new_set, 0);
                  if (changed)
                     break;
               }
            }
         }
      }
   }
   move2add_record_sym_value(self,reg, sym, off);
   return changed;
}


/* This function is called with INSN that sets REG (of mode MODE) to
   (SYM + OFF), but REG doesn't have known value (SYM + offset).  This
   function tries to find another register which is known to already have
   value (SYM + offset) and change INSN into an add instruction
   (set (REG) (plus (the found register) (OFF - offset))) if such
   a register is found.  It also updates the information about
   REG's known value.
   Return true iff we made a change.  */
static bool move2add_use_add3_insn (MtcsPostReload *self,scalar_int_mode mode, rtx reg, rtx sym, rtx off,
         rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRecog   *mtcsRecog = mtcs_target_get_recog(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   int firstPseudoRegister= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   rtx set = single_set (insn);
   rtx src = SET_SRC (set);
   int regno = REGNO (reg);
   int min_regno = 0;
   bool speed = optimize_bb_for_speed_p (BLOCK_FOR_INSN (insn));
   int i;
   bool changed = false;
   struct full_rtx_costs oldcst, newcst, mincst;
   rtx plus_expr;

   init_costs_to_max (&mincst);
   mtcs_rtlanal_get_full_set_rtx_cost/*!get_full_set_rtx_cost*/(mtcsRtlanal,set, &oldcst);

   plus_expr = gen_rtx_PLUS (GET_MODE (reg), reg, const0_rtx);
   SET_SRC (set) = plus_expr;

   for (i = 0; i < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; i++)
      if (move2add_valid_value_p(self,i, mode)
      && self->reg_base_reg[i] < 0
      && self->reg_symbol_ref[i] != NULL_RTX
      && rtx_equal_p (sym, self->reg_symbol_ref[i])){
         rtx new_src = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,UINTVAL (off) - self->reg_offset[i], GET_MODE (reg));
         /* (set (reg) (plus (reg) (const_int 0))) is not canonical;
         use (set (reg) (reg)) instead.
         We don't delete this insn, nor do we convert it into a
         note, to avoid losing register notes or the return
         value flag.  jump2 already knows how to get rid of
         no-op moves.  */
         if (new_src == const0_rtx){
            init_costs_to_zero (&mincst);
            min_regno = i;
            break;
         }else{
            XEXP (plus_expr, 1) = new_src;
            mtcs_rtlanal_get_full_set_rtx_cost/*!get_full_set_rtx_cost*/(mtcsRtlanal,set, &newcst);

            if (costs_lt_p (&newcst, &mincst, speed)){
               mincst = newcst;
               min_regno = i;
            }
         }
      }

   SET_SRC (set) = src;

   if (costs_lt_p (&mincst, &oldcst, speed)){
      rtx tem;

      tem = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,GET_MODE (reg), min_regno);
      if (i != min_regno){
         rtx new_src = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,UINTVAL (off) - self->reg_offset[min_regno],GET_MODE (reg));
         tem = gen_rtx_PLUS (GET_MODE (reg), tem, new_src);
      }
      if (mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, &SET_SRC (set), tem, 0))
         changed = true;
   }
   self->reg_set_luid[regno] = self->move2add_luid;
   move2add_record_sym_value(self,reg, sym, off);
   return changed;
}

typedef struct _Move2AddData
{
   rtx_insn *insn;
   MtcsPostReload *mtcsPostReload;
}Move2AddData;

/* Perform any invalidations necessary for INSN.  */
static void reload_cse_move2add_invalidate (MtcsPostReload *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRecog   *mtcsRecog = mtcs_target_get_recog(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsRtlPassMgr *mtcsRtlPassMgr=mtcs_target_get_rtl_pass_mgr(mtcsTarget);
   MtcsCprop *mtcsCprop =mtcs_rtl_pass_mgr_get_cprop(mtcsRtlPassMgr);
   MtcsFuncAbi  *mtcsFuncAbi = mtcs_target_get_func_abi(mtcsTarget);

   int firstPseudoRegister= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   for (rtx note = REG_NOTES (insn); note; note = XEXP (note, 1)){
      if (REG_NOTE_KIND (note) == REG_INC && REG_P (XEXP (note, 0))){
         /* Reset the information about this register.  */
         int regno = REGNO (XEXP (note, 0));
         if (regno < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/){
            move2add_record_mode(self,XEXP (note, 0));
            self->reg_mode[regno] = VOIDmode;
         }
      }
   }

   /* There are no REG_INC notes for SP autoinc.  */
   subrtx_var_iterator::array_type array;
   FOR_EACH_SUBRTX_VAR (iter, array, PATTERN (insn), NONCONST){
      rtx mem = *iter;
      if (mem  && MEM_P (mem)  && GET_RTX_CLASS (GET_CODE (XEXP (mem, 0))) == RTX_AUTOINC){
         if (XEXP (XEXP (mem, 0), 0) == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL))
         self->reg_mode[mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg)] = VOIDmode;
      }
   }

   Move2AddData userData={insn,self};
   mtcs_rtlanal_note_stores/*!note_stores*/(mtcsRtlanal,insn, move2add_note_store, (void*)&userData/*!insn*/);

   /* If INSN is a conditional branch, we try to extract an
   implicit set out of it.  */
   if (any_condjump_p (insn)){
      rtx cnd = mtcs_cprop_fis_get_condition/*!fis_get_condition*/(mtcsCprop,insn);

      if (cnd != NULL_RTX
      && GET_CODE (cnd) == NE
      && REG_P (XEXP (cnd, 0))
      && !mtcs_rtlanal_reg_set_p/*!reg_set_p*/(mtcsRtlanal,XEXP (cnd, 0), insn)
      /* The following two checks, which are also in
      move2add_note_store, are intended to reduce the
      number of calls to gen_rtx_SET to avoid memory
      allocation if possible.  */
      && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,GET_MODE (XEXP (cnd, 0)))
      && REG_NREGS (XEXP (cnd, 0)) == 1
      && CONST_INT_P (XEXP (cnd, 1))){
         rtx implicit_set = gen_rtx_SET (XEXP (cnd, 0), XEXP (cnd, 1));
         move2add_note_store (SET_DEST (implicit_set), implicit_set, (void*)&userData/*!insn*/);
      }
   }

   /* If this is a CALL_INSN, all call used registers are stored with
   unknown values.  */
   if (CALL_P (insn)){
      mtcs_function_abi callee_abi =  mtcs_func_abi_insn_callee_abi/*!insn_callee_abi*/(mtcsFuncAbi,insn);
      for (int i = firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/ - 1; i >= 0; i--)
         if (self->reg_mode[i] != VOIDmode
               && self->reg_mode[i] != mtcsMode->modes.M_BLKmode
               && callee_abi.clobbers_reg_p (self->reg_mode[i], i))
            /* Reset the information about this register.  */
            self->reg_mode[i] = VOIDmode;
   }
}

/* Convert move insns with constant inputs to additions if they are cheaper.
   Return true if any changes were made.  */
static bool reload_cse_move2add (MtcsPostReload *self,rtx_insn *first)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRecog   *mtcsRecog = mtcs_target_get_recog(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);

   int firstPseudoRegister= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   int i;
   rtx_insn *insn;
   bool changed = false;

   for (i =firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/ - 1; i >= 0; i--){
      self->reg_set_luid[i] = 0;
      self->reg_offset[i] = 0;
      self->reg_base_reg[i] = 0;
      self->reg_symbol_ref[i] = NULL_RTX;
      self->reg_mode[i] = VOIDmode;
   }

   self->move2add_last_label_luid = 0;
   self->move2add_luid = 2;
   for (insn = first; insn; insn = NEXT_INSN (insn), self->move2add_luid++){
      rtx set;

      if (LABEL_P (insn)){
         self->move2add_last_label_luid = self->move2add_luid;
         /* We're going to increment self->move2add_luid twice after a
         label, so that we can use self->move2add_last_label_luid + 1 as
         the luid for constants.  */
         self->move2add_luid++;
         continue;
      }
      if (! INSN_P (insn))
         continue;
      set = single_set (insn);
      /* For simplicity, we only perform this optimization on
      single-sets.  */
      scalar_int_mode mode;
      if (set
      && REG_P (SET_DEST (set))
      && mtcs_mode_is_a <scalar_int_mode>(mtcsMode,GET_MODE (SET_DEST (set)), &mode)){
         rtx reg = SET_DEST (set);
         int regno = REGNO (reg);
         rtx src = SET_SRC (set);

         /* Check if we have valid information on the contents of this
         register in the mode of REG.  */
         if (move2add_valid_value_p(self,regno, mode) && dbg_cnt (cse2_move2add)){
            /* Try to transform (set (REGX) (CONST_INT A))
            ...
            (set (REGX) (CONST_INT B))
            to
            (set (REGX) (CONST_INT A))
            ...
            (set (REGX) (plus (REGX) (CONST_INT B-A)))
            or
            (set (REGX) (CONST_INT A))
            ...
            (set (STRICT_LOW_PART (REGX)) (CONST_INT B))
            */

            if (CONST_INT_P (src) && self->reg_base_reg[regno] < 0  && self->reg_symbol_ref[regno] == NULL_RTX){
               changed |= move2add_use_add2_insn(self,mode, reg, NULL_RTX, src, insn);
               continue;
            }
            /* Try to transform (set (REGX) (REGY))
            (set (REGX) (PLUS (REGX) (CONST_INT A)))
            ...
            (set (REGX) (REGY))
            (set (REGX) (PLUS (REGX) (CONST_INT B)))
            to
            (set (REGX) (REGY))
            (set (REGX) (PLUS (REGX) (CONST_INT A)))
            ...
            (set (REGX) (plus (REGX) (CONST_INT B-A)))  */
            else if (REG_P (src)
            && self->reg_set_luid[regno] == self->reg_set_luid[REGNO (src)]
            && self->reg_base_reg[regno] == self->reg_base_reg[REGNO (src)]
            && move2add_valid_value_p(self,REGNO (src), mode)){
               rtx_insn *next = next_nonnote_nondebug_insn (insn);
               rtx set = NULL_RTX;
               if (next)
                  set = single_set (next);
               if (set
               && SET_DEST (set) == reg
               && GET_CODE (SET_SRC (set)) == PLUS
               && XEXP (SET_SRC (set), 0) == reg
               && CONST_INT_P (XEXP (SET_SRC (set), 1))){
                  rtx src3 = XEXP (SET_SRC (set), 1);
                  unsigned HOST_WIDE_INT added_offset = UINTVAL (src3);
                  HOST_WIDE_INT base_offset = self->reg_offset[REGNO (src)];
                  HOST_WIDE_INT regno_offset = self->reg_offset[regno];
                  rtx new_src =  mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,added_offset + base_offset  - regno_offset, mode);
                  bool success = false;
                  bool speed = optimize_bb_for_speed_p (BLOCK_FOR_INSN (insn));

                  if (new_src == const0_rtx)
                     /* See above why we create (set (reg) (reg)) here.  */
                     success = mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,next, &SET_SRC (set), reg, 0);
                  else{
                     rtx old_src = SET_SRC (set);
                     struct full_rtx_costs oldcst, newcst;
                     rtx tem = gen_rtx_PLUS (mode, reg, new_src);

                     mtcs_rtlanal_get_full_set_rtx_cost/*!get_full_set_rtx_cost*/(mtcsRtlanal,set, &oldcst);
                     SET_SRC (set) = tem;
                     mtcs_rtlanal_get_full_set_src_cost/*!get_full_set_src_cost*/(mtcsRtlanal,tem, mode, &newcst);
                     SET_SRC (set) = old_src;
                     costs_add_n_insns (&oldcst, 1);

                     rtx *setloc = GET_CODE (PATTERN (next)) == PARALLEL ? &XVECEXP (PATTERN (next), 0, 0) : &PATTERN (next);
                     if (*setloc == set && costs_lt_p (&newcst, &oldcst, speed) && have_add2_insn (reg, new_src)){
                        rtx newpat = gen_rtx_SET (reg, tem);
                        success  = mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,next, setloc, newpat, 0);
                     }
                  }
                  if (success)
                     mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,insn);
                  changed |= success;
                  insn = next;
                  /* Make sure to perform any invalidations related to
                  NEXT/INSN since we're going to bypass the normal
                  flow with the continue below.

                  Do this before recording the new mode/offset.  */
                  reload_cse_move2add_invalidate(self,insn);
                  move2add_record_mode(self,reg);
                  self->reg_offset[regno] = mtcs_mode_trunc_int_for_mode/*!trunc_int_for_mode*/(mtcsMode,
                        added_offset + base_offset,mode);
                  continue;
               }
            }
         }

         /* Try to transform
         (set (REGX) (CONST (PLUS (SYMBOL_REF) (CONST_INT A))))
         ...
         (set (REGY) (CONST (PLUS (SYMBOL_REF) (CONST_INT B))))
         to
         (set (REGX) (CONST (PLUS (SYMBOL_REF) (CONST_INT A))))
         ...
         (set (REGY) (CONST (PLUS (REGX) (CONST_INT B-A))))  */
         if ((GET_CODE (src) == SYMBOL_REF
         || (GET_CODE (src) == CONST
         && GET_CODE (XEXP (src, 0)) == PLUS
         && GET_CODE (XEXP (XEXP (src, 0), 0)) == SYMBOL_REF
         && CONST_INT_P (XEXP (XEXP (src, 0), 1))))
         && dbg_cnt (cse2_move2add)){
            rtx sym, off;

            if (GET_CODE (src) == SYMBOL_REF){
               sym = src;
               off = const0_rtx;
            }else{
               sym = XEXP (XEXP (src, 0), 0);
               off = XEXP (XEXP (src, 0), 1);
            }

            /* If the reg already contains the value which is sum of
            sym and some constant value, we can use an add2 insn.  */
            if (move2add_valid_value_p(self,regno, mode)
            && self->reg_base_reg[regno] < 0
            && self->reg_symbol_ref[regno] != NULL_RTX
            && rtx_equal_p (sym, self->reg_symbol_ref[regno]))
               changed |= move2add_use_add2_insn(self,mode, reg, sym, off, insn);

               /* Otherwise, we have to find a register whose value is sum
               of sym and some constant value.  */
            else
               changed |= move2add_use_add3_insn(self,mode, reg, sym, off, insn);

            continue;
         }
      }
      reload_cse_move2add_invalidate(self,insn);
   }
   return changed;
}

/* SET is a SET or CLOBBER that sets DST.  DATA is the insn which
   contains SET.
   Update self->reg_set_luid, self->reg_offset and self->reg_base_reg accordingly.
   Called from reload_cse_move2add via note_stores.  */
static void move2add_note_store (rtx dst, const_rtx set, void *userData)
{
   Move2AddData *info=(Move2AddData *)userData;
   MtcsPostReload *self = info->mtcsPostReload;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRecog   *mtcsRecog = mtcs_target_get_recog(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   rtx_insn *insn = (rtx_insn *) info->insn;
   unsigned int regno = 0;
   scalar_int_mode mode;

   if (GET_CODE (dst) == SUBREG)
      regno = mtcs_rtlanal_subreg_regno/*!subreg_regno*/(mtcsRtlanal,dst);
   else if (REG_P (dst))
      regno = REGNO (dst);
   else
      return;

   if (!mtcs_mode_is_a <scalar_int_mode>(mtcsMode,GET_MODE (dst), &mode))
      goto invalidate;

   if (GET_CODE (set) == SET){
      rtx note, sym = NULL_RTX;
      rtx off;

      note = find_reg_equal_equiv_note (insn);
      if (note && GET_CODE (XEXP (note, 0)) == SYMBOL_REF){
         sym = XEXP (note, 0);
         off = const0_rtx;
      }else if (note && GET_CODE (XEXP (note, 0)) == CONST
      && GET_CODE (XEXP (XEXP (note, 0), 0)) == PLUS
      && GET_CODE (XEXP (XEXP (XEXP (note, 0), 0), 0)) == SYMBOL_REF
      && CONST_INT_P (XEXP (XEXP (XEXP (note, 0), 0), 1))){
         sym = XEXP (XEXP (XEXP (note, 0), 0), 0);
         off = XEXP (XEXP (XEXP (note, 0), 0), 1);
      }

      if (sym != NULL_RTX){
         move2add_record_sym_value(self,dst, sym, off);
         return;
      }
   }

   if (GET_CODE (set) == SET
   && GET_CODE (SET_DEST (set)) != ZERO_EXTRACT
   && GET_CODE (SET_DEST (set)) != STRICT_LOW_PART){
      rtx src = SET_SRC (set);
      rtx base_reg;
      unsigned HOST_WIDE_INT offset;
      int base_regno;

      switch (GET_CODE (src)){
         case PLUS:
            if (REG_P (XEXP (src, 0))){
               base_reg = XEXP (src, 0);

               if (CONST_INT_P (XEXP (src, 1)))
                  offset = UINTVAL (XEXP (src, 1));
               else if (REG_P (XEXP (src, 1))  && move2add_valid_value_p(self,REGNO (XEXP (src, 1)), mode)){
                  if (self->reg_base_reg[REGNO (XEXP (src, 1))] < 0  && self->reg_symbol_ref[REGNO (XEXP (src, 1))] == NULL_RTX)
                     offset = self->reg_offset[REGNO (XEXP (src, 1))];
                  /* Maybe the first register is known to be a
                  constant.  */
                  else if (move2add_valid_value_p(self,REGNO (base_reg), mode)
                  && self->reg_base_reg[REGNO (base_reg)] < 0
                  && self->reg_symbol_ref[REGNO (base_reg)] == NULL_RTX){
                     offset = self->reg_offset[REGNO (base_reg)];
                     base_reg = XEXP (src, 1);
                  }else
                     goto invalidate;
               }else
                  goto invalidate;

               break;
            }

            goto invalidate;

         case REG:
            base_reg = src;
            offset = 0;
            break;

         case CONST_INT:
            /* Start tracking the register as a constant.  */
            self->reg_base_reg[regno] = -1;
            self->reg_symbol_ref[regno] = NULL_RTX;
            self->reg_offset[regno] = INTVAL (SET_SRC (set));
            /* We assign the same luid to all registers set to constants.  */
            self->reg_set_luid[regno] = self->move2add_last_label_luid + 1;
            move2add_record_mode(self,dst);
            return;

         default:
            goto invalidate;
      }

      base_regno = REGNO (base_reg);
      /* If information about the base register is not valid, set it
      up as a new base register, pretending its value is known
      starting from the current insn.  */
      if (!move2add_valid_value_p(self,base_regno, mode)){
         self->reg_base_reg[base_regno] = base_regno;
         self->reg_symbol_ref[base_regno] = NULL_RTX;
         self->reg_offset[base_regno] = 0;
         self->reg_set_luid[base_regno] = self->move2add_luid;
         gcc_assert (GET_MODE (base_reg) == mode);
         move2add_record_mode(self,base_reg);
      }

      /* Copy base information from our base register.  */
      self->reg_set_luid[regno] = self->reg_set_luid[base_regno];
      self->reg_base_reg[regno] = self->reg_base_reg[base_regno];
      self->reg_symbol_ref[regno] = self->reg_symbol_ref[base_regno];

      /* Compute the sum of the offsets or constants.  */
      self->reg_offset[regno] = trunc_int_for_mode (offset + self->reg_offset[base_regno], mode);

      move2add_record_mode(self,dst);
   }else{
invalidate:
      /* Invalidate the contents of the register.  */
      move2add_record_mode(self,dst);
      self->reg_mode[regno] = VOIDmode;
   }
}

//原型 NEXT_PASS (pass_postreload_cse, 1); RTL_PASS postreload.cc postreload n 有条件执行 (optimize > 0 && reload_completed);reload_cse_regs
static nboolean post_reload_gate_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   return mtcsOptionsItem->x_optimize>0 &&  reload_completed;
}

static nuint post_reload_execute_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsPostReload *self=(MtcsPostReload *)mtcsPass;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsCfgCleanup *mtcsCfgCleanup=mtcs_target_get_cfg_cleanup(mtcsTarget);

   if (!dbg_cnt (postreload_cse))
      return 0;

   /* Do a very simple CSE pass over just the hard registers.  */
   reload_cse_regs(self,mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData));
   /* Reload_cse_regs can eliminate potentially-trapping MEMs.
   Remove any EH edges associated with them.  */
   if (fun->can_throw_non_call_exceptions  && mtcs_cfg_rtl_purge_all_dead_edges/*!purge_all_dead_edges*/(mtcsCfgRtl))
      mtcs_cfg_cleanup_cleanup_cfg/*!cleanup_cfg*/(mtcsCfgCleanup,0);

   return 0;
}

static void mtcsPostReloadInit(MtcsPostReload *self)
{
   MtcsPass *mtcsPass= (MtcsPass *)self;
   mtcsPass->execute =post_reload_execute_cb;
   mtcsPass->gate =post_reload_gate_cb;
   mtcs_pass_set_properties(mtcsPass,
      0,/* properties_required */
      0, /* properties_provided */
      0 /* properties_destroyed */);
   mtcs_pass_set_todo_flags(mtcsPass,
      0, /* todo_flags_start */
      TODO_df_finish /*todo_flags_finish */);
}

MtcsPostReload *mtcs_post_reload_new(MtcsMode *mtcsMode)
{
   MtcsPostReload *self = n_slice_alloc0 (sizeof(MtcsPostReload));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"postreload");
   mtcsPostReloadInit(self);
   return self;
}
