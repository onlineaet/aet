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
 * base on fwprop.cc
 */

#define INCLUDE_ALGORITHM
#define INCLUDE_FUNCTIONAL
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "rtl.h"
#include "rtlanal.h"
#include "df.h"
#include "ssa/mtcsrtlssa.h"

#include "predict.h"
#include "cfgrtl.h"
#include "cfgcleanup.h"
#include "cfgloop.h"
#include "tree-pass.h"
#include "rtl-iter.h"
#include "target.h"

#include "aet/aetprinttree.h"
#include "mtcsfwprop.h"
#include "../mtcstarget.h"
#include "../mtcsdfcore.h"
#include "../mtcsdfproblems.h"
#include "../mtcsprintrtl.h"

using namespace mtcs_rtl_ssa;

/*此过程在以下情况下进行简单的正向传播和简化
insn的操作数只能来自一个def
RTL SSA，所以它是全球性的。然而，我们仅对以下内容进行了有限的分析
可用表达式。
*/

static void mtcsFwpropInit(MtcsFwprop *self)
{
  self->num_changes=0;
}

static void df_bb_verify (basic_block bb)
{
   rtx_insn *insn;
   n_debug("mtcsfwprop.c 打印每个bb中的insn bb:%p bb->index:%d\n",bb,bb->index);
   /* Scan the block, one insn at a time, from beginning to end.  */
   int count=0;
   FOR_BB_INSNS_REVERSE (bb, insn){
      n_debug("mtcsfwprop.c 打印每个bb中的count:%d insn:%p \n",count,insn);
      if (!INSN_P (insn)){
         continue;
      }
      count++;
      mtcs_print_rtl_single(stderr,insn);
   }
}

static void testprint()
{
   if(!n_log_is_debug())
      return;
   if(!cfun )
      return;
   basic_block bb;
   FOR_ALL_BB_FN (bb, cfun)
      df_bb_verify(bb);
}

/* Do not try to replace constant addresses or addresses of local and
   argument slots.  These MEM expressions are made only once and inserted
   in many instructions, as well as being used to control symbol table
   output.  It is not safe to clobber them.

   There are some uncommon cases where the address is already in a register
   for some reason, but we cannot take advantage of that because we have
   no easy way to unshare the MEM.  In addition, looking up all stack
   addresses is costly.  */

static bool can_simplify_addr (MtcsFwprop *self,rtx addr)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   rtx reg;

   if (mtcs_rtl_constant_address_p/*!CONSTANT_ADDRESS_P*/(mtcsRTL,addr))
      return false;

   if (GET_CODE (addr) == PLUS)
      reg = XEXP (addr, 0);
   else
      reg = addr;

   return (!REG_P (reg)
      || (REGNO (reg) != mtcs_reg_get_frame_pointer_regnum/*!FRAME_POINTER_REGNUM*/(mtcsReg)
      && REGNO (reg) != mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg)
      && REGNO (reg) != mtcs_reg_get_arg_pointer_regnum/*!ARG_POINTER_REGNUM*/(mtcsReg)));
}

/* MEM is the result of an address simplification, and temporarily
   undoing changes OLD_NUM_CHANGES onwards restores the original address.
   Return whether it is good to use the new address instead of the
   old one.  INSN is the containing instruction.  */

static bool should_replace_address (MtcsFwprop *self,int old_num_changes, rtx mem, rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   int gain;

   /* Prefer the new address if it is less expensive.  */
   bool speed = optimize_bb_for_speed_p (BLOCK_FOR_INSN (insn));
   mtcs_recog_temporarily_undo_changes/*!temporarily_undo_changes*/(mtcsRecog,old_num_changes);
   gain = mtcs_rtlanal_address_cost/*!address_cost*/(mtcsRtlanal,XEXP (mem, 0), GET_MODE (mem),
         mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,mem), speed);
   mtcs_recog_redo_changes/*!redo_changes*/(mtcsRecog,old_num_changes);
   gain -= mtcs_rtlanal_address_cost/*!address_cost*/(mtcsRtlanal,XEXP (mem, 0), GET_MODE (mem),
         mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,mem), speed);

   /* If the addresses have equivalent cost, prefer the new address
   if it has the highest `set_src_cost'.  That has the potential of
   eliminating the most insns without additional costs, and it
   is the same that cse.cc used to do.  */
   if (gain == 0){
      gain = mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,XEXP (mem, 0), VOIDmode, speed);
      mtcs_recog_temporarily_undo_changes/*!temporarily_undo_changes*/(mtcsRecog,old_num_changes);
      gain -= mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,XEXP (mem, 0), VOIDmode, speed);
      mtcs_recog_redo_changes/*!redo_changes*/(mtcsRecog,old_num_changes);
   }

   return (gain > 0);
}



class fwprop_propagation : public mtcs_insn_propagation
{
public:
   static const uint16_t CHANGED_MEM = FIRST_SPARE_RESULT;
   static const uint16_t CONSTANT = FIRST_SPARE_RESULT << 1;
   static const uint16_t PROFITABLE = FIRST_SPARE_RESULT << 2;

   fwprop_propagation (insn_info *, set_info *, rtx, rtx,MtcsFwprop *);

   bool changed_mem_p () const { return result_flags & CHANGED_MEM; }
   bool folded_to_constants_p () const;
   bool likely_profitable_p () const;

   bool check_mem (int, rtx) final override;
   void note_simplification (int, uint16_t, rtx, rtx) final override;
   uint16_t classify_result (rtx, rtx);

private:
   const bool single_use_p;
   const bool single_ebb_p;
   MtcsFwprop *mtcsFwprop;
};

/* Prepare to replace FROM with TO in USE_INSN.  */

fwprop_propagation::fwprop_propagation (insn_info *use_insn,
               set_info *def, rtx from, rtx to,MtcsFwprop *mf)
  : mtcs_insn_propagation (use_insn->rtl (), from, to),
    single_use_p (def->single_nondebug_use ()),
    single_ebb_p (use_insn->ebb () == def->ebb ()),mtcsFwprop(mf)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsFwprop);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   should_check_mems = true;
   should_note_simplifications = true;
}

/* MEM is the result of an address simplification, and temporarily
   undoing changes OLD_NUM_CHANGES onwards restores the original address.
   Return true if the propagation should continue, false if it has failed.  */

bool fwprop_propagation::check_mem (int old_num_changes, rtx mem)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsFwprop);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   if (!mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/(mtcsRecog,
         GET_MODE (mem), XEXP (mem, 0),mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,mem))){
      failure_reason = "would create an invalid MEM";
      return false;
   }

   mtcs_recog_temporarily_undo_changes/*!temporarily_undo_changes*/(mtcsRecog,old_num_changes);
   bool can_simplify = can_simplify_addr(mtcsFwprop,XEXP (mem, 0));
   mtcs_recog_redo_changes/*!redo_changes*/(mtcsRecog,old_num_changes);
   if (!can_simplify){
      failure_reason = "would replace a frame address";
      return false;
   }

   /* Copy propagations are always ok.  Otherwise check the costs.  */
   if (!(REG_P (from) && REG_P (to)) && !should_replace_address(mtcsFwprop,old_num_changes, mem, insn)){
      failure_reason = "would increase the cost of a MEM";
      return false;
   }

   result_flags |= CHANGED_MEM;
   return true;
}

/* OLDX has been simplified to NEWX.  Describe the change in terms of
   result_flags.  */

uint16_t fwprop_propagation::classify_result (rtx old_rtx, rtx new_rtx)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsFwprop);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   if (CONSTANT_P (new_rtx)){
      /* If OLD_RTX is a LO_SUM, then it presumably exists for a reason,
      and NEW_RTX is likely not a legitimate address.  We want it to
      disappear if it is invalid.

      ??? Using the mode of the LO_SUM as the mode of the address
      seems odd, but it was what the pre-SSA code did.  */
      if (GET_CODE (old_rtx) == LO_SUM
      && !mtcs_recog_memory_address_p/*!memory_address_p*/(mtcsRecog,GET_MODE (old_rtx), new_rtx))
         return CONSTANT;
      return CONSTANT | PROFITABLE;
   }

   /* Allow replacements that simplify operations on a vector or complex
   value to a component.  The most prominent case is
   (subreg ([vec_]concat ...)).   */
   if (REG_P (new_rtx)
   && !mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,new_rtx)
   && (mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,GET_MODE (from))
   || mtcs_mode_is_complex_p/*!COMPLEX_MODE_P*/(mtcsMode,GET_MODE (from)))
   && GET_MODE (new_rtx) == mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,GET_MODE (from)))
      return PROFITABLE;

   /* Allow (subreg (mem)) -> (mem) simplifications with the following
   exceptions:
   1) Propagating (mem)s into multiple uses is not profitable.
   2) Propagating (mem)s across EBBs may not be profitable if the source EBB
   runs less frequently.
   3) Propagating (mem)s into paradoxical (subreg)s is not profitable.
   4) Creating new (mem/v)s is not correct, since DCE will not remove the old
   ones.  */
   if (single_use_p
   && single_ebb_p
   && SUBREG_P (old_rtx)
   && !mtcs_rtl_paradoxical_subreg_p/*!paradoxical_subreg_p*/(mtcsRTL,old_rtx)
   && MEM_P (new_rtx)
   && !MEM_VOLATILE_P (new_rtx))
      return PROFITABLE;

   return 0;
}

/* Record that OLD_RTX has been simplified to NEW_RTX.  OLD_NUM_CHANGES
   is the number of unrelated changes that had been made before processing
   OLD_RTX and its subrtxes.  OLD_RESULT_FLAGS is the value that result_flags
   had at that point.  */

void fwprop_propagation::note_simplification (int old_num_changes,
                uint16_t old_result_flags,
                rtx old_rtx, rtx new_rtx)
{
   result_flags &= ~(CONSTANT | PROFITABLE);
   uint16_t new_flags = classify_result (old_rtx, new_rtx);
   if (old_num_changes)
      new_flags &= old_result_flags;
   result_flags |= new_flags;
}

/* Return true if all substitutions eventually folded to constants.  */

bool fwprop_propagation::folded_to_constants_p () const
{
   /* If we're propagating a HIGH, require it to be folded with a
   partnering LO_SUM.  For example, a REG_EQUAL note with a register
   replaced by an unfolded HIGH is not useful.  */
   if (CONSTANT_P (to) && GET_CODE (to) != HIGH)
      return true;
   return !(result_flags & UNSIMPLIFIED) && (result_flags & CONSTANT);
}


/* Return true if it is worth keeping the result of the propagation,
   false if it would increase the complexity of the pattern too much.  */
bool fwprop_propagation::likely_profitable_p () const
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsFwprop);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);


   if (changed_mem_p ())
      return true;

   if (!(result_flags & UNSIMPLIFIED)
   && (result_flags & PROFITABLE))
      return true;

   if (REG_P (to))
      return true;

   if (GET_CODE (to) == SUBREG
   && REG_P (SUBREG_REG (to))
   && !mtcs_rtl_paradoxical_subreg_p/*!paradoxical_subreg_p*/(mtcsRTL,to))
      return true;

   if (CONSTANT_P (to))
      return true;

   return false;
}

/* Check that X has a single def.  */

static bool reg_single_def_p (MtcsFwprop *self,rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   return REG_P (x) && mtcsRtlData/*!crtl*/->ssa->single_dominating_def (REGNO (x));
}

/* Try to substitute (set DEST SRC), which defines DEF, into note NOTE of
   USE_INSN.  Return the number of substitutions on success, otherwise return
   -1 and leave USE_INSN unchanged.

   If REQUIRE_CONSTANT is true, require all substituted occurrences of SRC
   to fold to a constant, so that the note does not use any more registers
   than it did previously.  If REQUIRE_CONSTANT is false, also allow the
   substitution if it's something we'd normally allow for the main
   instruction pattern.  */

static int try_fwprop_subst_note (MtcsFwprop *self,insn_info *use_insn, set_info *def,
             rtx note, rtx dest, rtx src, bool require_constant)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   rtx_insn *use_rtl = use_insn->rtl ();
   insn_info *def_insn = def->insn ();

   mtcs_insn_change_watermark watermark(mtcsRecog);
   fwprop_propagation prop (use_insn, def, dest, src,self);
   if (!prop.apply_to_rvalue (&XEXP (note, 0))){
      if (dump_file && (dump_flags & TDF_DETAILS))
         fprintf (dump_file, "cannot propagate from insn %d into"
               " notes of insn %d: %s\n", def_insn->uid (), use_insn->uid (), prop.failure_reason);
      return -1;
   }

   if (prop.num_replacements == 0)
      return 0;

   if (require_constant){
      if (!prop.folded_to_constants_p ()){
         if (dump_file && (dump_flags & TDF_DETAILS))
            fprintf (dump_file, "cannot propagate from insn %d into"
                  " notes of insn %d: %s\n", def_insn->uid (), use_insn->uid (), "wouldn't fold to constants");
         return -1;
      }
   }else{
      if (!prop.folded_to_constants_p () && !prop.likely_profitable_p ()){
         if (dump_file && (dump_flags & TDF_DETAILS))
            fprintf (dump_file, "cannot propagate from insn %d into"
                  " notes of insn %d: %s\n", def_insn->uid (), use_insn->uid (), "would increase complexity of node");
         return -1;
      }
   }

   if (dump_file && (dump_flags & TDF_DETAILS)){
      fprintf (dump_file, "\nin notes of insn %d, replacing:\n  ",INSN_UID (use_rtl));
      mtcs_recog_temporarily_undo_changes/*!temporarily_undo_changes*/(mtcsRecog,0);
      mtcs_print_inline_rtx/*!print_inline_rtx*/(dump_file, note, 2);
      mtcs_recog_redo_changes/*!redo_changes*/(mtcsRecog,0);
      fprintf (dump_file, "\n with:\n  ");
      mtcs_print_inline_rtx/*!print_inline_rtx*/(dump_file, note, 2);
      fprintf (dump_file, "\n");
   }
   watermark.keep ();
   return prop.num_replacements;
}

/* Try to substitute (set DEST SRC), which defines DEF, into location LOC of
   USE_INSN's pattern.  Return true on success, otherwise leave USE_INSN
   unchanged.  */

static bool try_fwprop_subst_pattern (MtcsFwprop *self,obstack_watermark &attempt, insn_change &use_change,
           set_info *def, rtx *loc, rtx dest, rtx src)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   insn_info *use_insn = use_change.insn ();
   rtx_insn *use_rtl = use_insn->rtl ();
   insn_info *def_insn = def->insn ();
   n_debug("mtcsfwprop.c try_fwprop_subst_pattern 00 *loc:%p dest:%p src:%p\n",*loc,dest,src);
   testprint();
   mtcs_insn_change_watermark watermark(mtcsRecog);
   fwprop_propagation prop (use_insn, def, dest, src,self);
   if (!prop.apply_to_pattern (loc)){
      if (dump_file && (dump_flags & TDF_DETAILS))
         fprintf (dump_file, "cannot propagate from insn %d into insn %d: %s\n",
               def_insn->uid (), use_insn->uid (),prop.failure_reason);
      return false;
   }
   n_debug("mtcsfwprop.c try_fwprop_subst_pattern 11 *loc:%p dest:%p src:%p\n",*loc,dest,src);
   testprint();
   if (prop.num_replacements == 0)
      return false;

   if (!prop.likely_profitable_p ()
   && (prop.changed_mem_p ()
   || contains_mem_rtx_p (src)
   || use_insn->is_asm ()
   || !single_set (use_rtl))){
      if (dump_file && (dump_flags & TDF_DETAILS))
         fprintf (dump_file, "cannot propagate from insn %d into insn %d: %s\n",
               def_insn->uid (), use_insn->uid (), "would increase complexity of pattern");
      return false;
   }
   n_debug("mtcsfwprop.c try_fwprop_subst_pattern 22\n");
   if (dump_file && (dump_flags & TDF_DETAILS)){
      fprintf (dump_file, "\npropagating insn %d into insn %d, replacing:\n",def_insn->uid (), use_insn->uid ());
      mtcs_recog_temporarily_undo_changes/*!temporarily_undo_changes*/(mtcsRecog,0);
      print_rtl_single (dump_file, PATTERN (use_rtl));
      mtcs_recog_redo_changes/*!redo_changes*/(mtcsRecog,0);
   }
   n_debug("mtcsfwprop.c try_fwprop_subst_pattern 33 执行 recog\n");
   /* ??? In theory, it should be better to use insn costs rather than
   set_src_costs here.  That would involve replacing this code with
   change_is_worthwhile.  */
   bool ok = recog (attempt, use_change);//recog 定义在 rtl-ssa/change-utils.h
   if (ok && !prop.changed_mem_p () && !use_insn->is_asm ())
      if (rtx use_set = single_set (use_rtl)){
         bool speed = optimize_bb_for_speed_p (BLOCK_FOR_INSN (use_rtl));
         mtcs_recog_temporarily_undo_changes/*!temporarily_undo_changes*/(mtcsRecog,0);
         auto old_cost = mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,
               SET_SRC (use_set),GET_MODE (SET_DEST (use_set)), speed);
         mtcs_recog_redo_changes/*!redo_changes*/(mtcsRecog,0);
         auto new_cost = mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,
               SET_SRC (use_set),GET_MODE (SET_DEST (use_set)), speed);
         if (new_cost > old_cost || (new_cost == old_cost && !prop.likely_profitable_p ())){
            if (dump_file)
               fprintf (dump_file, "change not profitable (cost %d -> cost %d)\n", old_cost, new_cost);
            ok = false;
         }
      }
   n_debug("mtcsfwprop.c try_fwprop_subst_pattern 44 ok:%d\n",ok);
   if (!ok){
      /* The pattern didn't match, but if all uses of SRC folded to
      constants, we can add a REG_EQUAL note for the result, if there
      isn't one already.  */
      if (!prop.folded_to_constants_p ())
         return false;
      n_debug("mtcsfwprop.c try_fwprop_subst_pattern 44aa ok:%d\n",ok);

      /* Test this first to avoid creating an unnecessary copy of SRC.  */
      if (find_reg_note (use_rtl, REG_EQUAL, NULL_RTX))
         return false;
      n_debug("mtcsfwprop.c try_fwprop_subst_pattern 44bb ok:%d\n",ok);

      rtx set = set_for_reg_notes (use_rtl);
      if (!set || !REG_P (SET_DEST (set)))
         return false;
      n_debug("mtcsfwprop.c try_fwprop_subst_pattern 44cc ok:%d\n",ok);

      rtx value = copy_rtx (SET_SRC (set));
      mtcs_recog_cancel_changes/*!cancel_changes*/(mtcsRecog,0);

      /* If there are any paradoxical SUBREGs, drop the REG_EQUAL note,
      because the bits in there can be anything and so might not
      match the REG_EQUAL note content.  See PR70574.  */
      if (mtcs_rtlanal_contains_paradoxical_subreg_p/*!contains_paradoxical_subreg_p*/(mtcsRtlanal,SET_SRC (set)))
         return false;
      n_debug("mtcsfwprop.c try_fwprop_subst_pattern 44dd ok:%d\n",ok);

      if (dump_file && (dump_flags & TDF_DETAILS))
         fprintf (dump_file, " Setting REG_EQUAL note\n");

      return mtcs_rtl_set_unique_reg_note/*!set_unique_reg_note*/(mtcsRTL,use_rtl, REG_EQUAL, value);
   }
   n_debug("mtcsfwprop.c try_fwprop_subst_pattern 55\n");
   rtx *note_ptr = &REG_NOTES (use_rtl);
   while (rtx note = *note_ptr){
      if ((REG_NOTE_KIND (note) == REG_EQUAL
      || REG_NOTE_KIND (note) == REG_EQUIV)
      && try_fwprop_subst_note(self,use_insn, def, note, dest, src, false) < 0){
         *note_ptr = XEXP (note, 1);
         free_EXPR_LIST_node (note);
      } else
         note_ptr = &XEXP (note, 1);
   }
   n_debug("mtcsfwprop.c try_fwprop_subst_pattern 66\n");
   mtcs_recog_confirm_change_group/*!confirm_change_group*/(mtcsRecog);
   mtcsRtlData/*!crtl*/->ssa->change_insn (use_change);
   self->num_changes++;
   return true;
}

/* Try to substitute (set DEST SRC), which defines DEF, into USE_INSN's notes,
   given that it was not possible to do this for USE_INSN's main pattern.
   Return true on success, otherwise leave USE_INSN unchanged.  */

static bool try_fwprop_subst_notes (MtcsFwprop *self,insn_info *use_insn, set_info *def,
         rtx dest, rtx src)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   rtx_insn *use_rtl = use_insn->rtl ();
   for (rtx note = REG_NOTES (use_rtl); note; note = XEXP (note, 1))
      if ((REG_NOTE_KIND (note) == REG_EQUAL
      || REG_NOTE_KIND (note) == REG_EQUIV)
      && try_fwprop_subst_note(self,use_insn, def, note, dest, src, true) > 0){
         mtcs_recog_confirm_change_group/*!confirm_change_group*/(mtcsRecog);
         return true;
      }

   return false;
}

/* Check whether we could validly substitute (set DEST SRC), which defines DEF,
   into USE.  If so, first try performing the substitution in location LOC
   of USE->insn ()'s pattern.  If that fails, try instead to substitute
   into the notes.

   Return true on success, otherwise leave USE_INSN unchanged.  */
static bool try_fwprop_subst (MtcsFwprop *self,use_info *use, set_info *def,
        rtx *loc, rtx dest, rtx src)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   insn_info *use_insn = use->insn ();
   insn_info *def_insn = def->insn ();

   auto attempt = mtcsRtlData/*!crtl*/->ssa->new_change_attempt ();
   use_array src_uses = remove_note_accesses (attempt, def_insn->uses ());
   n_debug("mtcsfwprop.c try_fwprop_subst 00 *loc:%p dest:%p src:%p\n",*loc,dest,src);

   /* ??? Not really a meaningful test: it means we can propagate arithmetic
   involving hard registers but not bare references to them.  A better
   test would be to iterate over src_uses looking for hard registers
   that are not fixed.  */
   if (REG_P (src) && mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,src))
      return false;

   /* ??? It would be better to make this EBB-based instead.  That would
   involve checking for equal EBBs rather than equal BBs and trying
   to make the uses available at use_insn->ebb ()->first_bb ().  */
   if (def_insn->bb () != use_insn->bb ()){
      src_uses =  mtcsRtlData/*!crtl*/->ssa->make_uses_available (attempt,
            src_uses,use_insn->bb (), use_insn->is_debug_insn ());
      if (!src_uses.is_valid ())
         return false;
   }
   insn_change use_change (use_insn);
   use_change.new_uses = merge_access_arrays (attempt, use_change.new_uses,  src_uses);
   if (!use_change.new_uses.is_valid ())
      return false;

   /* ??? We could allow movement within the EBB by adding:
   use_change.move_range = use_insn->ebb ()->insn_range ();  */
   if (!restrict_movement (use_change))
      return false;
   n_debug("mtcsfwprop.c try_fwprop_subst 11 准备执行 try_fwprop_subst_pattern 和 try_fwprop_subst_notes *loc:%p dest:%p src:%p\n",
         *loc,dest,src);

   return (try_fwprop_subst_pattern(self,attempt, use_change, def, loc, dest, src)
         || try_fwprop_subst_notes(self,use_insn, def, dest, src));
}

/* For the given single_set INSN, containing SRC known to be a
   ZERO_EXTEND or SIGN_EXTEND of a register, return true if INSN
   is redundant due to the register being set by a LOAD_EXTEND_OP
   load from memory.  */

static bool free_load_extend (MtcsFwprop *self,rtx src, insn_info *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   rtx reg = XEXP (src, 0);
   if (mtcs_mode_load_extend_op/*!load_extend_op*/(mtcsMode,GET_MODE (reg)) != GET_CODE (src))
      return false;

   def_info *def = nullptr;
   for (use_info *use : insn->uses ())
      if (use->regno () == REGNO (reg)){
         def = use->def ();
         break;
      }

   if (!def)
      return false;

   insn_info *def_insn = def->insn ();
   if (def_insn->is_artificial ())
      return false;

   rtx_insn *def_rtl = def_insn->rtl ();
   if (NONJUMP_INSN_P (def_rtl)){
      rtx patt = PATTERN (def_rtl);

      if (GET_CODE (patt) == SET && GET_CODE (SET_SRC (patt)) == MEM && rtx_equal_p (SET_DEST (patt), reg))
         return true;
   }
   return false;
}

/* Subroutine of forward_propagate_subreg that handles a use of DEST
   in REF.  The other parameters are the same.  */

static bool forward_propagate_subreg (MtcsFwprop *self,use_info *use, set_info *def,
           rtx dest, rtx src, df_ref ref)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   scalar_int_mode int_use_mode, src_mode;

   /* Only consider subregs... */
   rtx use_reg = DF_REF_REG (ref);
   machine_mode use_mode = GET_MODE (use_reg);
   if (GET_CODE (use_reg) != SUBREG || GET_MODE (SUBREG_REG (use_reg)) != GET_MODE (dest))
      return false;

   /* ??? Replacing throughout the pattern would help for match_dups.  */
   rtx *loc = DF_REF_LOC (ref);
   if (mtcs_rtl_paradoxical_subreg_p/*!paradoxical_subreg_p*/(mtcsRTL,use_reg)){
      /* If this is a paradoxical SUBREG, we have no idea what value the
      extra bits would have.  However, if the operand is equivalent to
      a SUBREG whose operand is the same as our mode, and all the modes
      are within a word, we can just use the inner operand because
      these SUBREGs just say how to treat the register.  */
      if (GET_CODE (src) == SUBREG
      && REG_P (SUBREG_REG (src))
      && REGNO (SUBREG_REG (src)) >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
      && GET_MODE (SUBREG_REG (src)) == use_mode
      && mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,src))
         return try_fwprop_subst(self,use, def, loc, use_reg, SUBREG_REG (src));
   }

   /* If this is a SUBREG of a ZERO_EXTEND or SIGN_EXTEND, and the SUBREG
   is the low part of the reg being extended then just use the inner
   operand.  Don't do this if the ZERO_EXTEND or SIGN_EXTEND insn will
   be removed due to it matching a LOAD_EXTEND_OP load from memory,
   or due to the operation being a no-op when applied to registers.
   For example, if we have:

   A: (set (reg:DI X) (sign_extend:DI (reg:SI Y)))
   B: (... (subreg:SI (reg:DI X)) ...)

   and mode_rep_extended says that Y is already sign-extended,
   the backend will typically allow A to be combined with the
   definition of Y or, failing that, allow A to be deleted after
   reload through register tying.  Introducing more uses of Y
   prevents both optimisations.  */
   else if (mtcs_mode_is_a <scalar_int_mode>(mtcsMode,use_mode, &int_use_mode)
      && mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,use_reg)){
      if ((GET_CODE (src) == ZERO_EXTEND
      || GET_CODE (src) == SIGN_EXTEND)
      && mtcs_mode_is_a<scalar_int_mode>(mtcsMode,GET_MODE (src), &src_mode)
      && REG_P (XEXP (src, 0))
      && REGNO (XEXP (src, 0)) >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
      && GET_MODE (XEXP (src, 0)) == use_mode
      && !free_load_extend(self,src, def->insn ())
      && (mtcsTarget/*!targetm.mode_rep_extended*/->mode_rep_extended(mtcsTarget,int_use_mode, src_mode)
      != (int) GET_CODE (src)))
         return try_fwprop_subst(self,use, def, loc, use_reg, XEXP (src, 0));
   }

   return false;
}

/* Try to substitute (set DEST SRC), which defines DEF, into USE and simplify
   the result, handling cases where DEST is used in a subreg and where
   applying that subreg to SRC results in a useful simplification.  */

static bool forward_propagate_subreg (MtcsFwprop *self,use_info *use, set_info *def, rtx dest, rtx src)
{
   if (!use->includes_subregs () || !REG_P (dest))
      return false;

   if (GET_CODE (src) != SUBREG && GET_CODE (src) != ZERO_EXTEND && GET_CODE (src) != SIGN_EXTEND)
      return false;

   rtx_insn *use_rtl = use->insn ()->rtl ();
   df_ref ref;

   FOR_EACH_INSN_USE (ref, use_rtl)
   if (DF_REF_REGNO (ref) == use->regno () && forward_propagate_subreg(self,use, def, dest, src, ref))
      return true;

   FOR_EACH_INSN_EQ_USE (ref, use_rtl)
   if (DF_REF_REGNO (ref) == use->regno () && forward_propagate_subreg(self,use, def, dest, src, ref))
      return true;

   return false;
}

/* Try to substitute (set DEST SRC), which defines DEF, into USE and
   simplify the result.  */

static bool forward_propagate_and_simplify (MtcsFwprop *self,use_info *use, set_info *def,
            rtx dest, rtx src)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   insn_info *use_insn = use->insn ();
   rtx_insn *use_rtl = use_insn->rtl ();
   insn_info *def_insn = def->insn ();

   /* ??? This check seems unnecessary.  We should be able to propagate
   into any kind of instruction, regardless of whether it's a single set.
   It seems odd to be more permissive with asms than normal instructions.  */
   bool need_single_set = (!use_insn->is_asm () && !use_insn->is_debug_insn ());
   rtx use_set = single_set (use_rtl);
   if (need_single_set && !use_set)
      return false;

   /* Do not propagate into PC etc.

   ??? This too seems unnecessary.  The current code should work correctly
   without it, including cases where jumps become unconditional.  */
   if (use_set && GET_MODE (SET_DEST (use_set)) == VOIDmode)
      return false;

   /* In __asm don't replace if src might need more registers than
   reg, as that could increase register pressure on the __asm.  */
   if (use_insn->is_asm () && def_insn->uses ().size () > 1)
      return false;

   /* Check if the def is loading something from the constant pool; in this
   case we would undo optimization such as compress_float_constant.
   Still, we can set a REG_EQUAL note.  */
   if (MEM_P (src) && MEM_READONLY_P (src)){
      rtx x = mtcs_simplify_rtx_avoid_constant_pool_reference/*!avoid_constant_pool_reference*/(mtcsSimplifyRtx,src);
      rtx note_set;
      if (x != src
      && (note_set = set_for_reg_notes (use_rtl))
      && REG_P (SET_DEST (note_set))
      && !mtcs_rtlanal_contains_paradoxical_subreg_p/*!contains_paradoxical_subreg_p*/(mtcsRtlanal,SET_SRC (note_set))){
         rtx note = find_reg_note (use_rtl, REG_EQUAL, NULL_RTX);
         rtx old_rtx = note ? XEXP (note, 0) : SET_SRC (note_set);
         rtx new_rtx = mtcs_simplify_rtx_simplify_replace_rtx/*!simplify_replace_rtx*/(mtcsSimplifyRtx,old_rtx, src, x);
         if (old_rtx != new_rtx)
            mtcs_rtl_set_unique_reg_note/*!set_unique_reg_note*/(mtcsRTL,use_rtl, REG_EQUAL, copy_rtx (new_rtx));
      }
      return false;
   }

   /* ??? Unconditionally propagating into PATTERN would work better
   for instructions that have match_dups.  */
   rtx *loc = need_single_set ? &use_set : &PATTERN (use_rtl);
   n_debug("mtcsfwprop.c forward_propagate_and_simplify 00 加入loc :%p dest:%p src:%p\n",*loc,dest,src);
   mtcs_print_rtl_single(stderr,*loc);
   bool ret = try_fwprop_subst(self,use, def, loc, dest, src);
   return ret;
}

/* Given a use USE of an insn, if it has a single reaching
   definition, try to forward propagate it into that insn.
   Return true if something changed.

   REG_PROP_ONLY is true if we should only propagate register copies.  */
static bool forward_propagate_into (MtcsFwprop *self,use_info *use, bool reg_prop_only = false)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   n_debug("mtcsfwprop.c forward_propagate_into 00\n");
   if (use->includes_read_writes ())
      return false;

   /* Disregard uninitialized uses.  */
   set_info *def = use->def ();
   if (!def)
      return false;

   /* Only consider single-register definitions.  This could be relaxed,
   but it should rarely be needed before RA.  */
   def = look_through_degenerate_phi (def);
   if (def->includes_multiregs ())
      return false;

   /* Only consider uses whose definition comes from a real instruction.  */
   insn_info *def_insn = def->insn ();
   if (def_insn->is_artificial ())
      return false;

   rtx_insn *def_rtl = def_insn->rtl ();
   if (!NONJUMP_INSN_P (def_rtl))
      return false;
   /* ??? This seems an unnecessary restriction.  We can easily tell
   which set the definition comes from.  */
   if (multiple_sets (def_rtl))
      return false;

   rtx def_set = mtcs_rtlanal_simple_regno_set/*!simple_regno_set*/(mtcsRtlanal,PATTERN (def_rtl), def->regno ());
   if (!def_set)
      return false;
   rtx dest = SET_DEST (def_set);
   rtx src = SET_SRC (def_set);
   if (volatile_refs_p (src))
      return false;
   n_debug("mtcsfwprop.c forward_propagate_into 11 dest:%p src:%p\n",dest,src);
   mtcs_print_rtl_single(stderr,dest);
   mtcs_print_rtl_single(stderr,src);
   /* Allow propagations into a loop only for reg-to-reg copies, since
   replacing one register by another shouldn't increase the cost.
   Propagations from inner loop to outer loop should also be ok.  */
   struct loop *def_loop = def_insn->bb ()->cfg_bb ()->loop_father;
   struct loop *use_loop = use->bb ()->cfg_bb ()->loop_father;
   if ((reg_prop_only
   || (def_loop != use_loop
   && !flow_loop_nested_p (use_loop, def_loop)))
   && (!reg_single_def_p(self,dest) || !reg_single_def_p(self,src)))
      return false;

   /* Don't substitute into a non-local goto, this confuses CFG.  */
   insn_info *use_insn = use->insn ();
   rtx_insn *use_rtl = use_insn->rtl ();
   if (JUMP_P (use_rtl)
   && find_reg_note (use_rtl, REG_NON_LOCAL_GOTO, NULL_RTX))
      return false;
   if (forward_propagate_and_simplify(self,use, def, dest, src)
   || forward_propagate_subreg(self,use, def, dest, src))
      return true;

   return false;
}

static void fwprop_init (MtcsFwprop *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsLoopinit *mtcsLoopinit =mtcs_target_get_loopinit(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   self->num_changes = 0;
   calculate_dominance_info (CDI_DOMINATORS);
   /* We do not always want to propagate into loops, so we have to find
   loops and be careful about them.  Avoid CFG modifications so that
   we don't have to update dominance information afterwards for
   build_single_def_use_links.  */
   mtcs_loopinit_loop_optimizer_init/*!loop_optimizer_init*/(mtcsLoopinit,AVOID_CFG_MODIFICATIONS);
   mtcs_dfcore_df_analyze/*!df_analyze*/(mtcsDfcore);
   mtcsRtlData/*!crtl*/->ssa = new mtcs_rtl_ssa::function_info (cfun);
}



static void fwprop_done (MtcsFwprop *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCse *mtcsCse=mtcs_target_get_cse(mtcsTarget);
   MtcsCfgCleanup *mtcsCfgCleanup=mtcs_target_get_cfg_cleanup(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   loop_optimizer_finalize ();

   mtcsRtlData/*!crtl*/->ssa->perform_pending_updates ();
   free_dominance_info (CDI_DOMINATORS);
   n_debug("mtcsfwprop.c fwprop_done 00\n");
   testprint();
   mtcs_cfg_cleanup_cleanup_cfg/*!cleanup_cfg*/(mtcsCfgCleanup,0);
   n_debug("mtcsfwprop.c fwprop_done 11\n");
   delete mtcsRtlData/*!crtl*/->ssa;
   n_debug("mtcsfwprop.c fwprop_done 22\n");
   testprint();
   mtcsRtlData/*!crtl*/->ssa = nullptr;
   n_debug("mtcsfwprop.c fwprop_done 33\n");

   mtcs_cse_delete_trivially_dead_insns/*!delete_trivially_dead_insns*/(mtcsCse,
   mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData), mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc));
   testprint();
   n_debug("mtcsfwprop.c fwprop_done 44\n");


   if (dump_file)
      fprintf (dump_file,"\nNumber of successful forward propagations: %d\n\n", self->num_changes);
}

/* Try to optimize INSN, returning true if something changes.
   FWPROP_ADDR_P is true if we are running fwprop_addr rather than
   the full fwprop.  */

static bool fwprop_insn (MtcsFwprop *self,insn_info *insn, bool fwprop_addr_p)
{
   for (use_info *use : insn->uses ()){
      if (use->is_mem ())
         continue;
      /* ??? The choices here follow those in the pre-SSA code.  */
      if (!use->includes_address_uses ()){
         n_debug("mtcsfwprop.c fwprop_insn 00 %p\n",insn->rtl());
                   mtcs_print_rtl_single(stderr,insn->rtl());
         if (forward_propagate_into(self,use, fwprop_addr_p))
            return true;
      }else{
            struct loop *loop = insn->bb ()->cfg_bb ()->loop_father;
            /* The outermost loop is not really a loop.  */
            if (loop == NULL || loop_outer (loop) == NULL){
               n_debug("mtcsfwprop.c fwprop_insn 11 %p\n",insn->rtl());
                         mtcs_print_rtl_single(stderr,insn->rtl());
               if (forward_propagate_into(self,use, fwprop_addr_p))
                  return true;
            }else if (fwprop_addr_p){
               n_debug("mtcsfwprop.c fwprop_insn 22 %p\n",insn->rtl());
                         mtcs_print_rtl_single(stderr,insn->rtl());
               if (forward_propagate_into(self,use, false))
                  return true;
            }
      }
   }
   return false;
}

static unsigned int fwprop (MtcsFwprop *self,bool fwprop_addr_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   n_debug("mtcsfwprop.c fwprop 00\n");
   fwprop_init(self);
   n_debug("mtcsfwprop.c fwprop 11 完成 fwprop_init\n");

   /* Go through all the instructions (including debug instructions) looking
   for uses that we could propagate into.

   Do not forward propagate addresses into loops until after unrolling.
   CSE did so because it was able to fix its own mess, but we are not.  */
   insn_info *next;
   /* ??? This code uses a worklist in order to preserve the behavior
   of the pre-SSA implementation.  It would be better to instead
   iterate on each instruction until no more propagations are
   possible, then move on to the next.  */
   auto_vec<insn_info *> worklist;
   for (insn_info *insn = mtcsRtlData/*!crtl*/->ssa->first_insn (); insn; insn = next){
      next = insn->next_any_insn ();
      if (insn->can_be_optimized () || insn->is_debug_insn ())
         if (fwprop_insn(self,insn, fwprop_addr_p)){
            n_debug("mtcsfwprop.c fwprop 00 %p\n",insn->rtl());
            mtcs_print_rtl_single(stderr,insn->rtl());
            worklist.safe_push (insn);
         }
   }
   for (unsigned int i = 0; i < worklist.length (); ++i){
      insn_info *insn = worklist[i];
      if (fwprop_insn(self,insn, fwprop_addr_p)){
         n_debug("mtcsfwprop.c fwprop 11 %p\n",insn->rtl());
         mtcs_print_rtl_single(stderr,insn->rtl());
         worklist.safe_push (insn);
      }
   }

   fwprop_done(self);
   return 0;
}

MtcsFwprop *mtcs_fwprop_new(MtcsMode *mtcsMode)
{
    MtcsFwprop *self = n_slice_alloc0 (sizeof(MtcsFwprop));
     mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
     mtcsFwpropInit(self);
     return self;
}

/********************以下是pass的共用功能*******************************/
static nboolean fwprop_gate_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   return mtcsOptionsItem->x_optimize>0 && mtcsOptionsItem->x_flag_forward_propagate;
}

static nuint fwprop_execute_cb(MtcsPass *mtcsPass,function *func)
{
   bool fwprop_addr_p=false;
   if(strcmp(mtcsPass->name,"fwprop2")==0)
         fwprop_addr_p=true;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlPassMgr *mtcsRtlPassMgr=mtcs_target_get_rtl_pass_mgr(mtcsTarget);
   MtcsFwprop *mtcsFwprop=mtcs_rtl_pass_mgr_get_fwprop(mtcsRtlPassMgr);
   return fwprop(mtcsFwprop,fwprop_addr_p);
}

static void commonInit(MtcsPass *mtcsPass)
{
     mtcsPass->execute =fwprop_execute_cb;
     mtcsPass->gate =fwprop_gate_cb;
     mtcs_pass_set_properties(mtcsPass,
        0,/* properties_required */
        0, /* properties_provided */
        0 /* properties_destroyed */);
     mtcs_pass_set_todo_flags(mtcsPass,
        0, /* todo_flags_start */
        TODO_df_finish /*todo_flags_finish */);
}

//原型 NEXT_PASS (pass_rtl_fwprop, 1); RTL_PASS fwprop.cc fwprop1 n 有条件执行 optimize > 0 && flag_forward_propagate;   fwprop (false);
static void mtcsPassFwprop1Init(MtcsPassFwprop1 *self)
{
   commonInit((MtcsPass *)self);
}

MtcsPassFwprop1 *mtcs_pass_fwprop1_new(MtcsMode *mtcsMode)
{
   MtcsPassFwprop1 *self = n_slice_alloc0 (sizeof(MtcsPassFwprop1));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"fwprop1");
   mtcsPassFwprop1Init(self);
   return self;
}


//原型 NEXT_PASS (pass_rtl_fwprop_addr, 1); RTL_PASS fwprop.cc fwprop2 n 有条件执行 optimize > 0 && flag_forward_propagate;   fwprop (true);
static void mtcsPassFwprop2Init(MtcsPassFwprop2 *self)
{
   commonInit((MtcsPass *)self);
}

MtcsPassFwprop2 *mtcs_pass_fwprop2_new(MtcsMode *mtcsMode)
{
   MtcsPassFwprop2 *self = n_slice_alloc0 (sizeof(MtcsPassFwprop2));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"fwprop2");
   mtcsPassFwprop2Init(self);
   return self;
}
