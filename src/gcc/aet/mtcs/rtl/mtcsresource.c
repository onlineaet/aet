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
 * base on resource.cc
 */

#include "config.h"
#define INCLUDE_ALGORITHM /* reverse */
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "tree-pass.h"
#include "cfghooks.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "tree-pretty-print.h" /* for dump_function_header */
#include "varasm.h"
#include "insn-attr.h"
#include "conditions.h"
#include "flags.h"
#include "output.h"
#include "except.h"
#include "rtl-error.h"
#include "toplev.h" /* exact_log2, floor_log2 */
#include "reload.h"
#include "intl.h"
#include "cfgrtl.h"
#include "debug.h"
#include "tree-pass.h"
#include "tree-ssa.h"
#include "cfgloop.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "rtl-iter.h"
#include "print-rtl.h"
#include "function-abi.h"
#include "common/common-target.h"
#include "diagnostic.h"
#include "context.h"
#include "options.h"
#include "expmed.h"
#include "expr.h"
#include "fold-const.h"
#include "dwarf2out.h"
#include "common/common-targhooks.h"

#include "cfganal.h"
#include "cfgcleanup.h"
#include "alias.h"
#include "rtlhooks-def.h"
#include "tree-pass.h"
#include "dbgcnt.h"
#include "rtlanal.h"
#include "resource.h"

#include "aet/aetprinttree.h"
#include "mtcsresource.h"
#include "../mtcstarget.h"

#define TARGET_HASH_PRIME 257



static int       find_basic_block (rtx_insn *, int);
static rtx_insn *next_insn_no_annul (rtx_insn *);
static rtx_insn *find_dead_or_set_registers (MtcsResource *self,rtx_insn *target, ReorgResources *res,
      rtx *jump_target, int jump_count, ReorgResources set, ReorgResources needed);

static void mtcsResourceInit(MtcsResource *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg= mtcs_target_get_reg(mtcsTarget);
   int regElementCount = mtcs_reg_get_hard_reg_element_count(mtcsReg);
   self->current_live_regs.count=regElementCount;
   self->pending_dead_regs.count=regElementCount;
   self->target_hash_table=NULL;
   self->start_of_epilogue_needs.regs.count=regElementCount;
   self->end_of_function_needs.regs.count=regElementCount;

 /* Indicates what resources are required at function end.  */
}

/* Utility function called from mark_target_live_regs via note_stores.
   It deadens any CLOBBERed registers and livens any SET registers.  */
//原型 update_live_status resource.cc
static void updateLiveStatus_cb (rtx dest, const_rtx x, void *userData ATTRIBUTE_UNUSED)
{
   MtcsResource *self=(MtcsResource *)userData;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   int first_regno, last_regno;
   int i;

   if (!REG_P (dest)  && (GET_CODE (dest) != SUBREG || !REG_P (SUBREG_REG (dest))))
      return;

   if (GET_CODE (dest) == SUBREG) {
      first_regno = mtcs_rtlanal_subreg_regno/*!subreg_regno*/(mtcsRtlanal,dest);
      last_regno = first_regno + mtcs_rtlanal_subreg_nregs/*!subreg_nregs*/(mtcsRtlanal,dest);

   }else{
      first_regno = REGNO (dest);
      last_regno = END_REGNO (dest);
   }

   if (GET_CODE (x) == CLOBBER)
      for (i = first_regno; i < last_regno; i++)
         mtcs_reg_clear_hard_reg_bit/*!CLEAR_HARD_REG_BIT*/(mtcsReg,&self->current_live_regs, i);
   else
      for (i = first_regno; i < last_regno; i++){
         mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&self->current_live_regs, i);
         mtcs_reg_clear_hard_reg_bit/*!CLEAR_HARD_REG_BIT*/(mtcsReg,&self->pending_dead_regs, i);
      }
}

/* Find the number of the basic block with correct live register
   information that starts closest to INSN.  Return -1 if we couldn't
   find such a basic block or the beginning is more than
   SEARCH_LIMIT instructions before INSN.  Use SEARCH_LIMIT = -1 for
   an unlimited search.

   The delay slot filling code destroys the control-flow graph so,
   instead of finding the basic block containing INSN, we search
   backwards toward a BARRIER where the live register information is
   correct.  */

static int find_basic_block (rtx_insn *insn, int search_limit)
{
   /* Scan backwards to the previous BARRIER.  Then see if we can find a
   label that starts a basic block.  Return the basic block number.  */
   for (insn = prev_nonnote_insn (insn); insn && !BARRIER_P (insn)
   && search_limit != 0; insn = prev_nonnote_insn (insn), --search_limit)
      ;

   /* The closest BARRIER is too far away.  */
   if (search_limit == 0)
      return -1;

   /* The start of the function.  */
   else if (insn == 0)
      return ENTRY_BLOCK_PTR_FOR_FN (cfun)->next_bb->index;

   /* See if any of the upcoming CODE_LABELs start a basic block.  If we reach
   anything other than a CODE_LABEL or note, we can't find this code.  */
   for (insn = next_nonnote_insn (insn);insn && LABEL_P (insn); insn = next_nonnote_insn (insn))
      if (BLOCK_FOR_INSN (insn))
         return BLOCK_FOR_INSN (insn)->index;

   return -1;
}

/* Similar to next_insn, but ignores insns in the delay slots of
   an annulled branch.  */

static rtx_insn * next_insn_no_annul (rtx_insn *insn)
{
   if (insn){
      /* If INSN is an annulled branch, skip any insns from the target
      of the branch.  */
      if (JUMP_P (insn) && INSN_ANNULLED_BRANCH_P (insn) && NEXT_INSN (PREV_INSN (insn)) != insn){
         rtx_insn *next = NEXT_INSN (insn);

         while ((NONJUMP_INSN_P (next) || JUMP_P (next) || CALL_P (next))  && INSN_FROM_TARGET_P (next)){
            insn = next;
            next = NEXT_INSN (insn);
         }
      }

      insn = NEXT_INSN (insn);
      if (insn && NONJUMP_INSN_P (insn)  && GET_CODE (PATTERN (insn)) == SEQUENCE)
         insn = as_a <rtx_sequence *> (PATTERN (insn))->insn (0);
   }

   return insn;
}

/* Given X, some rtl, and RES, a pointer to a `struct resource', mark
   which resources are referenced by the insn.  If INCLUDE_DELAYED_EFFECTS
   is TRUE, resources used by the called routine will be included for
   CALL_INSNs.  */
//原型 mark_referenced_resources resource.h resource.cc
void mtcs_resource_mark_referenced_resources (MtcsResource *self,rtx x, ReorgResources/*!struct resources*/ *res,
            bool include_delayed_effects)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);


   enum rtx_code code = GET_CODE (x);
   int i, j;
   unsigned int r;
   const char *format_ptr;

   /* Handle leaf items for which we set resource flags.  Also, special-case
   CALL, SET and CLOBBER operators.  */
   switch (code){
      case CONST:
      CASE_CONST_ANY:
      case PC:
      case SYMBOL_REF:
      case LABEL_REF:
      case DEBUG_INSN:
         return;

      case SUBREG:
         if (!REG_P (SUBREG_REG (x)))
            mtcs_resource_mark_referenced_resources/*!mark_referenced_resources*/(self,SUBREG_REG (x), res, false);
         else{
            unsigned int regno = mtcs_rtlanal_subreg_regno/*!subreg_regno*/(mtcsRtlanal,x);
            unsigned int last_regno = regno + mtcs_rtlanal_subreg_nregs/*!subreg_nregs*/(mtcsRtlanal,x);

            gcc_assert (last_regno <= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg));
            for (r = regno; r < last_regno; r++)
               mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&res->regs, r);
         }
         return;

      case REG:
         gcc_assert (mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,x));
         mtcs_reg_add_to_hard_reg_set/*!add_to_hard_reg_set*/(mtcsReg,&res->regs, GET_MODE (x), REGNO (x));
         return;

      case MEM:
         /* If this memory shouldn't change, it really isn't referencing
         memory.  */
         if (! MEM_READONLY_P (x))
            res->memory = 1;
         res->volatil |= MEM_VOLATILE_P (x);

         /* Mark registers used to access memory.  */
         mtcs_resource_mark_referenced_resources/*!mark_referenced_resources*/(self,XEXP (x, 0), res, false);
         return;

      case UNSPEC_VOLATILE:
      case TRAP_IF:
      case ASM_INPUT:
         /* Traditional asm's are always volatile.  */
         res->volatil = 1;
         break;

      case ASM_OPERANDS:
         res->volatil |= MEM_VOLATILE_P (x);

         /* For all ASM_OPERANDS, we must traverse the vector of input operands.
         We cannot just fall through here since then we would be confused
         by the ASM_INPUT rtx inside ASM_OPERANDS, which do not indicate
         traditional asms unlike their normal usage.  */

         for (i = 0; i < ASM_OPERANDS_INPUT_LENGTH (x); i++)
            mtcs_resource_mark_referenced_resources/*!mark_referenced_resources*/(self,ASM_OPERANDS_INPUT (x, i), res, false);
         return;

      case CALL:
         /* The first operand will be a (MEM (xxx)) but doesn't really reference
         memory.  The second operand may be referenced, though.  */
         mtcs_resource_mark_referenced_resources/*!mark_referenced_resources*/(self,XEXP (XEXP (x, 0), 0), res, false);
         mtcs_resource_mark_referenced_resources/*!mark_referenced_resources*/(self,XEXP (x, 1), res, false);
         return;

      case SET:
         /* Usually, the first operand of SET is set, not referenced.  But
         registers used to access memory are referenced.  SET_DEST is
         also referenced if it is a ZERO_EXTRACT.  */

         mtcs_resource_mark_referenced_resources/*!mark_referenced_resources*/(self,SET_SRC (x), res, false);

         x = SET_DEST (x);
         if (GET_CODE (x) == ZERO_EXTRACT || GET_CODE (x) == STRICT_LOW_PART)
            mtcs_resource_mark_referenced_resources/*!mark_referenced_resources*/(self,x, res, false);
         else if (GET_CODE (x) == SUBREG)
            x = SUBREG_REG (x);
         if (MEM_P (x))
            mtcs_resource_mark_referenced_resources/*!mark_referenced_resources*/(self,XEXP (x, 0), res, false);
         return;

      case CLOBBER:
         return;

      case CALL_INSN:
         if (include_delayed_effects){
            /* A CALL references memory, the frame pointer if it exists, the
            stack pointer, any global registers and any registers given in
            USE insns immediately in front of the CALL.

            However, we may have moved some of the parameter loading insns
            into the delay slot of this CALL.  If so, the USE's for them
            don't count and should be skipped.  */
            rtx_insn *insn = PREV_INSN (as_a <rtx_insn *> (x));
            rtx_sequence *sequence = 0;
            int seq_size = 0;
            int i;

            /* If we are part of a delay slot sequence, point at the SEQUENCE.  */
            if (NEXT_INSN (insn) != x){
               sequence = as_a <rtx_sequence *> (PATTERN (NEXT_INSN (insn)));
               seq_size = sequence->len ();
               gcc_assert (GET_CODE (sequence) == SEQUENCE);
            }

            res->memory = 1;
            mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&res->regs,
                  mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg));
            if (mtcsRtlData->x_frame_pointer_needed/*!frame_pointer_needed*/){
               mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&res->regs,
                     mtcs_reg_get_frame_pointer_regnum/*!FRAME_POINTER_REGNUM*/(mtcsReg));
               if (!mtcs_reg_hard_frame_pointer_is_frame_pointer/*!HARD_FRAME_POINTER_IS_FRAME_POINTER*/(mtcsReg))
                  mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&res->regs,
                        mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg));
            }

            for (i = 0; i < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i++)
               if (mtcsReg->global_regs/*!global_regs*/[i])
                  mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&res->regs, i);

            /* Check for a REG_SETJMP.  If it exists, then we must
            assume that this call can need any register.

            This is done to be more conservative about how we handle setjmp.
            We assume that they both use and set all registers.  Using all
            registers ensures that a register will not be considered dead
            just because it crosses a setjmp call.  A register should be
            considered dead only if the setjmp call returns nonzero.  */
            if (find_reg_note (x, REG_SETJMP, NULL))
               mtcs_reg_set_hard_reg_set/*!SET_HARD_REG_SET*/(&res->regs);

            {
               rtx link;

               for (link = CALL_INSN_FUNCTION_USAGE (x);  link;  link = XEXP (link, 1))
                  if (GET_CODE (XEXP (link, 0)) == USE){
                     for (i = 1; i < seq_size; i++){
                        rtx slot_pat = PATTERN (sequence->element (i));
                        if (GET_CODE (slot_pat) == SET  && rtx_equal_p (SET_DEST (slot_pat), XEXP (XEXP (link, 0), 0)))
                           break;
                     }
                     if (i >= seq_size)
                        mtcs_resource_mark_referenced_resources/*!mark_referenced_resources*/(self,
                              XEXP (XEXP (link, 0), 0),res, false);
                  }
            }
         }

         /* ... fall through to other INSN processing ...  */
         gcc_fallthrough ();

      case INSN:
      case JUMP_INSN:

         if (GET_CODE (PATTERN (x)) == COND_EXEC)
            /* In addition to the usual references, also consider all outputs
            as referenced, to compensate for mark_set_resources treating
            them as killed.  This is similar to ZERO_EXTRACT / STRICT_LOW_PART
            handling, execpt that we got a partial incidence instead of a partial
            width.  */
            mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,
                  x, res, 0,include_delayed_effects ? MARK_SRC_DEST_CALL : MARK_SRC_DEST);

         if (! include_delayed_effects  && INSN_REFERENCES_ARE_DELAYED (as_a <rtx_insn *> (x)))
            return;

         /* No special processing, just speed up.  */
         mtcs_resource_mark_referenced_resources/*!mark_referenced_resources*/(self,PATTERN (x), res, include_delayed_effects);
         return;

      default:
         break;
   }

   /* Process each sub-expression and flag what it needs.  */
   format_ptr = GET_RTX_FORMAT (code);
   for (i = 0; i < GET_RTX_LENGTH (code); i++)
      switch (*format_ptr++){
         case 'e':
            mtcs_resource_mark_referenced_resources/*!mark_referenced_resources*/(self,
                  XEXP (x, i), res, include_delayed_effects);
            break;

         case 'E':
            for (j = 0; j < XVECLEN (x, i); j++)
               mtcs_resource_mark_referenced_resources/*!mark_referenced_resources*/(self,
                     XVECEXP (x, i, j), res,include_delayed_effects);
            break;
      }
}

/* A subroutine of mark_target_live_regs.  Search forward from TARGET
   looking for registers that are set before they are used.  These are dead.
   Stop after passing a few conditional jumps, and/or a small
   number of unconditional branches.  */

static rtx_insn *find_dead_or_set_registers (MtcsResource *self,rtx_insn *target, ReorgResources *res,
             rtx *jump_target, int jump_count, ReorgResources set, ReorgResources needed)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   HardRegSet/*!HARD_REG_SET*/ scratch={mtcs_reg_get_hard_reg_element_count(mtcsReg)};
   rtx_insn *insn;
   rtx_insn *next_insn;
   rtx_insn *jump_insn = 0;
   int i;

   for (insn = target; insn; insn = next_insn){
      rtx_insn *this_insn = insn;

      next_insn = NEXT_INSN (insn);

      /* If this instruction can throw an exception, then we don't
      know where we might end up next.  That means that we have to
      assume that whatever we have already marked as live really is
      live.  */
      if (can_throw_internal (insn))
         break;

      switch (GET_CODE (insn)){
         case CODE_LABEL:
            /* After a label, any pending dead registers that weren't yet
            used can be made dead.  */
            self->pending_dead_regs &= ~needed.regs;
            res->regs &= ~self->pending_dead_regs;
            mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&self->pending_dead_regs);
            continue;

         case BARRIER:
         case NOTE:
         case DEBUG_INSN:
            continue;

         case INSN:
            if (GET_CODE (PATTERN (insn)) == USE){
               /* If INSN is a USE made by update_block, we care about the
               underlying insn.  Any registers set by the underlying insn
               are live since the insn is being done somewhere else.  */
               if (INSN_P (XEXP (PATTERN (insn), 0)))
                  mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,XEXP (PATTERN (insn), 0), res, 0,MARK_SRC_DEST_CALL);

               /* All other USE insns are to be ignored.  */
               continue;
            }else if (GET_CODE (PATTERN (insn)) == CLOBBER)
               continue;
            else if (rtx_sequence *seq = dyn_cast <rtx_sequence *> (PATTERN (insn))){
               /* An unconditional jump can be used to fill the delay slot
               of a call, so search for a JUMP_INSN in any position.  */
               for (i = 0; i < seq->len (); i++){
                  this_insn = seq->insn (i);
                  if (JUMP_P (this_insn))
                     break;
               }
            }

         default:
            break;
      }

      if (rtx_jump_insn *this_jump_insn = dyn_cast <rtx_jump_insn *> (this_insn)){
         if (jump_count++ < 10){
            if (any_uncondjump_p (this_jump_insn) || ANY_RETURN_P (PATTERN (this_jump_insn))){
               rtx lab_or_return = this_jump_insn->jump_label ();
               if (ANY_RETURN_P (lab_or_return))
                  next_insn = NULL;
               else
                  next_insn = as_a <rtx_insn *> (lab_or_return);
               if (jump_insn == 0){
                  jump_insn = insn;
                  if (jump_target)
                     *jump_target = JUMP_LABEL (this_jump_insn);
               }
            }else if (any_condjump_p (this_jump_insn)){
               ReorgResources/*!struct resources*/ target_set, target_res;
               ReorgResources/*!struct resources*/ fallthrough_res;

               /* We can handle conditional branches here by following
               both paths, and then IOR the results of the two paths
               together, which will give us registers that are dead
               on both paths.  Since this is expensive, we give it
               a much higher cost than unconditional branches.  The
               cost was chosen so that we will follow at most 1
               conditional branch.  */

               jump_count += 4;
               if (jump_count >= 10)
                  break;

               mtcs_resource_mark_referenced_resources/*!mark_referenced_resources*/(self,insn, &needed, true);

               /* For an annulled branch, mark_set_resources ignores slots
               filled by instructions from the target.  This is correct
               if the branch is not taken.  Since we are following both
               paths from the branch, we must also compute correct info
               if the branch is taken.  We do this by inverting all of
               the INSN_FROM_TARGET_P bits, calling mark_set_resources,
               and then inverting the INSN_FROM_TARGET_P bits again.  */

               if (GET_CODE (PATTERN (insn)) == SEQUENCE  && INSN_ANNULLED_BRANCH_P (this_jump_insn)){
                  rtx_sequence *seq = as_a <rtx_sequence *> (PATTERN (insn));
                  for (i = 1; i < seq->len (); i++)
                     INSN_FROM_TARGET_P (seq->element (i)) = ! INSN_FROM_TARGET_P (seq->element (i));

                  target_set = set;
                  mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,insn, &target_set, 0,MARK_SRC_DEST_CALL);

                  for (i = 1; i < seq->len (); i++)
                     INSN_FROM_TARGET_P (seq->element (i)) = ! INSN_FROM_TARGET_P (seq->element (i));

                  mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,insn, &set, 0, MARK_SRC_DEST_CALL);
               }else{
                  mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,insn, &set, 0, MARK_SRC_DEST_CALL);
                  target_set = set;
               }

               target_res = *res;
               scratch = target_set.regs & ~needed.regs;
               target_res.regs &= ~scratch;

               fallthrough_res = *res;
               scratch = set.regs & ~needed.regs;
               fallthrough_res.regs &= ~scratch;

               if (!ANY_RETURN_P (this_jump_insn->jump_label ()))
                  find_dead_or_set_registers(self,this_jump_insn->jump_target (),&target_res, 0, jump_count, target_set, needed);
               find_dead_or_set_registers(self,next_insn,&fallthrough_res, 0, jump_count,set, needed);
               fallthrough_res.regs |= target_res.regs;
               res->regs &= fallthrough_res.regs;
               break;
            }else
               break;
         }else{
            /* Don't try this optimization if we expired our jump count
            above, since that would mean there may be an infinite loop
            in the function being compiled.  */
            jump_insn = 0;
            break;
         }
      }

      mtcs_resource_mark_referenced_resources/*!mark_referenced_resources*/(self,insn, &needed, true);
      mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,insn, &set, 0, MARK_SRC_DEST_CALL);

      scratch = set.regs & ~needed.regs;
      res->regs &= ~scratch;
   }//end for (insn = target; insn; insn = next_insn)

   return jump_insn;
}

/* Given X, a part of an insn, and a pointer to a `struct resource',
   RES, indicate which resources are modified by the insn. If
   MARK_TYPE is MARK_SRC_DEST_CALL, also mark resources potentially
   set by the called routine.

   If IN_DEST is nonzero, it means we are inside a SET.  Otherwise,
   objects are being referenced instead of set.  */
//原型 mark_set_resources resource.h resource.cc
void mtcs_resource_mark_set_resources (MtcsResource *self,rtx x, ReorgResources/*!struct resources*/ *res, int in_dest,
          enum mark_resource_type mark_type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsFuncAbi *mtcsFuncAbi=mtcs_target_get_func_abi(mtcsTarget);

   enum rtx_code code;
   int i, j;
   unsigned int r;
   const char *format_ptr;

restart:

   code = GET_CODE (x);

   switch (code){
      case NOTE:
      case BARRIER:
      case CODE_LABEL:
      case USE:
      CASE_CONST_ANY:
      case LABEL_REF:
      case SYMBOL_REF:
      case CONST:
      case PC:
      case DEBUG_INSN:
         /* These don't set any resources.  */
         return;

      case CALL_INSN:
         /* Called routine modifies the condition code, memory, any registers
         that aren't saved across calls, global registers and anything
         explicitly CLOBBERed immediately after the CALL_INSN.  */

         if (mark_type == MARK_SRC_DEST_CALL){
            rtx_call_insn *call_insn = as_a <rtx_call_insn *> (x);
            rtx link;

            res->cc = res->memory = 1;

            res->regs |= mtcs_func_abi_insn_callee_abi/*!insn_callee_abi*/(mtcsFuncAbi,call_insn).full_reg_clobbers ();

            for (link = CALL_INSN_FUNCTION_USAGE (call_insn); link; link = XEXP (link, 1))
               if (GET_CODE (XEXP (link, 0)) == CLOBBER)
                  mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,
                        SET_DEST (XEXP (link, 0)), res, 1,MARK_SRC_DEST);

            /* Check for a REG_SETJMP.  If it exists, then we must
            assume that this call can clobber any register.  */
            if (find_reg_note (call_insn, REG_SETJMP, NULL))
               mtcs_reg_set_hard_reg_set/*!SET_HARD_REG_SET*/(&res->regs);
         }

         /* ... and also what its RTL says it modifies, if anything.  */
         gcc_fallthrough ();

      case JUMP_INSN:
      case INSN:
         /* An insn consisting of just a CLOBBER (or USE) is just for flow
         and doesn't actually do anything, so we ignore it.  */
         if (mark_type != MARK_SRC_DEST_CALL && INSN_SETS_ARE_DELAYED (as_a <rtx_insn *> (x)))
            return;

         x = PATTERN (x);
         if (GET_CODE (x) != USE && GET_CODE (x) != CLOBBER)
            goto restart;
         return;

      case SET:
         /* If the source of a SET is a CALL, this is actually done by
         the called routine.  So only include it if we are to include the
         effects of the calling routine.  */

         mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,SET_DEST (x), res,
               (mark_type == MARK_SRC_DEST_CALL || GET_CODE (SET_SRC (x)) != CALL),mark_type);

         mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,SET_SRC (x), res, 0, MARK_SRC_DEST);
         return;

      case CLOBBER:
         mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,XEXP (x, 0), res, 1, MARK_SRC_DEST);
         return;

      case SEQUENCE:
      {
         rtx_sequence *seq = as_a <rtx_sequence *> (x);
         rtx control = seq->element (0);
         bool annul_p = JUMP_P (control) && INSN_ANNULLED_BRANCH_P (control);

         mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,control, res, 0, mark_type);
         for (i = seq->len () - 1; i >= 0; --i){
            rtx elt = seq->element (i);
            if (!annul_p && INSN_FROM_TARGET_P (elt))
               mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,elt, res, 0, mark_type);
         }
      }
         return;

      case POST_INC:
      case PRE_INC:
      case POST_DEC:
      case PRE_DEC:
         mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,XEXP (x, 0), res, 1, MARK_SRC_DEST);
         return;

      case PRE_MODIFY:
      case POST_MODIFY:
         mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,XEXP (x, 0), res, 1, MARK_SRC_DEST);
         mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,XEXP (XEXP (x, 1), 0), res, 0, MARK_SRC_DEST);
         mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,XEXP (XEXP (x, 1), 1), res, 0, MARK_SRC_DEST);
         return;

      case SIGN_EXTRACT:
      case ZERO_EXTRACT:
         mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,XEXP (x, 0), res, in_dest, MARK_SRC_DEST);
         mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,XEXP (x, 1), res, 0, MARK_SRC_DEST);
         mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,XEXP (x, 2), res, 0, MARK_SRC_DEST);
         return;

      case MEM:
         if (in_dest){
            res->memory = 1;
            res->volatil |= MEM_VOLATILE_P (x);
         }

         mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,XEXP (x, 0), res, 0, MARK_SRC_DEST);
         return;

      case SUBREG:
         if (in_dest){
            if (!REG_P (SUBREG_REG (x)))
               mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,SUBREG_REG (x), res, in_dest, mark_type);
            else{
               unsigned int regno = mtcs_rtlanal_subreg_regno/*!subreg_regno*/(mtcsRtlanal,x);
               unsigned int last_regno = regno + mtcs_rtlanal_subreg_nregs/*!subreg_nregs*/(mtcsRtlanal,x);

               gcc_assert (last_regno <= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg));
               for (r = regno; r < last_regno; r++)
                  mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&res->regs, r);
            }
         }
         return;

      case REG:
         if (in_dest){
            gcc_assert (mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,x));
            mtcs_reg_add_to_hard_reg_set/*!add_to_hard_reg_set*/(mtcsReg,&res->regs, GET_MODE (x), REGNO (x));
         }
         return;

      case UNSPEC_VOLATILE:
      case ASM_INPUT:
         /* Traditional asm's are always volatile.  */
         res->volatil = 1;
         return;

      case TRAP_IF:
         res->volatil = 1;
         break;

      case ASM_OPERANDS:
         res->volatil |= MEM_VOLATILE_P (x);

         /* For all ASM_OPERANDS, we must traverse the vector of input operands.
         We cannot just fall through here since then we would be confused
         by the ASM_INPUT rtx inside ASM_OPERANDS, which do not indicate
         traditional asms unlike their normal usage.  */

         for (i = 0; i < ASM_OPERANDS_INPUT_LENGTH (x); i++)
            mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,
                  ASM_OPERANDS_INPUT (x, i), res, in_dest,MARK_SRC_DEST);
         return;

      default:
         break;
   }

   /* Process each sub-expression and flag what it needs.  */
   format_ptr = GET_RTX_FORMAT (code);
   for (i = 0; i < GET_RTX_LENGTH (code); i++)
      switch (*format_ptr++){
         case 'e':
            mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,XEXP (x, i), res, in_dest, mark_type);
            break;

         case 'E':
            for (j = 0; j < XVECLEN (x, i); j++)
               mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,XVECEXP (x, i, j), res, in_dest, mark_type);
            break;
      }
}

/* Return TRUE if INSN is a return, possibly with a filled delay slot.  */
static bool return_insn_p (const_rtx insn)
{
   if (JUMP_P (insn) && ANY_RETURN_P (PATTERN (insn)))
      return true;

   if (NONJUMP_INSN_P (insn) && GET_CODE (PATTERN (insn)) == SEQUENCE)
      return return_insn_p (XVECEXP (PATTERN (insn), 0, 0));

   return false;
}

/* Set the resources that are live at TARGET.

   If TARGET is zero, we refer to the end of the current function and can
   return our precomputed value.

   Otherwise, we try to find out what is live by consulting the basic block
   information.  This is tricky, because we must consider the actions of
   reload and jump optimization, which occur after the basic block information
   has been computed.

   Accordingly, we proceed as follows::

   We find the previous BARRIER and look at all immediately following labels
   (with no intervening active insns) to see if any of them start a basic
   block.  If we hit the start of the function first, we use block 0.

   Once we have found a basic block and a corresponding first insn, we can
   accurately compute the live status (by starting at a label following a
   BARRIER, we are immune to actions taken by reload and jump.)  Then we
   scan all insns between that point and our target.  For each CLOBBER (or
   for call-clobbered regs when we pass a CALL_INSN), mark the appropriate
   registers are dead.  For a SET, mark them as live.

   We have to be careful when using REG_DEAD notes because they are not
   updated by such things as find_equiv_reg.  So keep track of registers
   marked as dead that haven't been assigned to, and mark them dead at the
   next CODE_LABEL since reload and jump won't propagate values across labels.

   If we cannot find the start of a basic block (should be a very rare
   case, if it can happen at all), mark everything as potentially live.

   Next, scan forward from TARGET looking for things set or clobbered
   before they are used.  These are not live.

   Because we can be called many times on the same target, save our results
   in a hash table indexed by INSN_UID.  This is only done if the function
   init_resource_info () was invoked before we are called.  */
//原型 mark_target_live_regs resource.h resource.cc
void mtcs_resource_mark_target_live_regs (MtcsResource *self,rtx_insn *insns,
      rtx target_maybe_return,ReorgResources/*!struct resources*/ *res)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsFuncAbi *mtcsFuncAbi=mtcs_target_get_func_abi(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   int b = -1;
   unsigned int i;
   ReorgTargetInfo/*!struct target_info*/ *tinfo = NULL;
   rtx_insn *insn;
   rtx jump_target;
   HardRegSet/*!HARD_REG_SET*/ scratch={mtcs_reg_get_hard_reg_element_count(mtcsReg)};
   ReorgResources/*!struct resources*/ set, needed;

   /* Handle end of function.  */
   if (target_maybe_return == 0 || ANY_RETURN_P (target_maybe_return)){
      *res = self->end_of_function_needs;
      return;
   }

   /* We've handled the case of RETURN/SIMPLE_RETURN; we should now have an
   instruction.  */
   rtx_insn *target = as_a <rtx_insn *> (target_maybe_return);

   /* Handle return insn.  */
   if (return_insn_p (target)){
      *res = self->end_of_function_needs;
      mtcs_resource_mark_referenced_resources/*!mark_referenced_resources*/(self,target, res, false);
      return;
   }

   /* We have to assume memory is needed, but the CC isn't.  */
   res->memory = 1;
   res->volatil = 0;
   res->cc = 0;

   /* See if we have computed this value already.  */
   if (self->target_hash_table != NULL){
      for (tinfo = self->target_hash_table[INSN_UID (target) % TARGET_HASH_PRIME]; tinfo; tinfo = tinfo->next)
         if (tinfo->uid == INSN_UID (target))
            break;

      /* Start by getting the basic block number.  If we have saved
      information, we can get it from there unless the insn at the
      start of the basic block has been deleted.  */
      if (tinfo && tinfo->block != -1  && ! BB_HEAD (BASIC_BLOCK_FOR_FN (cfun, tinfo->block))->deleted ())
         b = tinfo->block;
   }

   if (b == -1)
      b = find_basic_block (target, mtcsOptionsItem->x_param_max_delay_slot_live_search);

   if (self->target_hash_table != NULL){
      if (tinfo){
         /* If the information is up-to-date, use it.  Otherwise, we will
         update it below.  */
         if (b == tinfo->block && b != -1 && tinfo->bb_tick == self->bb_ticks[b]){
            res->regs = tinfo->live_regs;
            return;
         }
      }else{
         /* Allocate a place to put our results and chain it into the
         hash table.  */
         tinfo = XNEW (ReorgTargetInfo/*!struct target_info*/);
         tinfo->uid = INSN_UID (target);
         tinfo->block = b;
         tinfo->live_regs.count= mtcs_reg_get_hard_reg_element_count(mtcsReg);
         tinfo->next = self->target_hash_table[INSN_UID (target) % TARGET_HASH_PRIME];
         self->target_hash_table[INSN_UID (target) % TARGET_HASH_PRIME] = tinfo;
      }
   }

   mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&self->pending_dead_regs);

   /* If we found a basic block, get the live registers from it and update
   them with anything set or killed between its start and the insn before
   TARGET; this custom life analysis is really about registers so we need
   to use the LR problem.  Otherwise, we must assume everything is live.  */
   if (b != -1){
      regset regs_live = DF_LR_IN (BASIC_BLOCK_FOR_FN (cfun, b));
      rtx_insn *start_insn, *stop_insn;
      df_ref def;

      /* Compute hard regs live at start of block.  */
      mtcs_reg_reg_set_to_hard_reg_set/*!REG_SET_TO_HARD_REG_SET*/(mtcsReg,&self->current_live_regs, regs_live);
      FOR_EACH_ARTIFICIAL_DEF (def, b)
         if (DF_REF_FLAGS (def) & DF_REF_AT_TOP)
            mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&self->current_live_regs, DF_REF_REGNO (def));

      /* Get starting and ending insn, handling the case where each might
      be a SEQUENCE.  */
      start_insn = (b == ENTRY_BLOCK_PTR_FOR_FN (cfun)->next_bb->index ? insns : BB_HEAD (BASIC_BLOCK_FOR_FN (cfun, b)));
      stop_insn = target;

      if (NONJUMP_INSN_P (start_insn) && GET_CODE (PATTERN (start_insn)) == SEQUENCE)
         start_insn = as_a <rtx_sequence *> (PATTERN (start_insn))->insn (0);

      if (NONJUMP_INSN_P (stop_insn) && GET_CODE (PATTERN (stop_insn)) == SEQUENCE)
         stop_insn = next_insn (PREV_INSN (stop_insn));

      for (insn = start_insn; insn != stop_insn; insn = next_insn_no_annul (insn)){
         rtx link;
         rtx_insn *real_insn = insn;
         enum rtx_code code = GET_CODE (insn);

         if (DEBUG_INSN_P (insn))
            continue;

         /* If this insn is from the target of a branch, it isn't going to
         be used in the sequel.  If it is used in both cases, this
         test will not be true.  */
         if ((code == INSN || code == JUMP_INSN || code == CALL_INSN) && INSN_FROM_TARGET_P (insn))
            continue;

         /* If this insn is a USE made by update_block, we care about the
         underlying insn.  */
         if (code == INSN  && GET_CODE (PATTERN (insn)) == USE  && INSN_P (XEXP (PATTERN (insn), 0)))
            real_insn = as_a <rtx_insn *> (XEXP (PATTERN (insn), 0));

         if (CALL_P (real_insn)){
            /* Values in call-clobbered registers survive a COND_EXEC CALL
            if that is not executed; this matters for resoure use because
            they may be used by a complementarily (or more strictly)
            predicated instruction, or if the CALL is NORETURN.  */
            if (GET_CODE (PATTERN (real_insn)) != COND_EXEC){
               HardRegSet/*!HARD_REG_SET*/ regs_invalidated_by_this_call  =
                     mtcs_func_abi_insn_callee_abi/*!insn_callee_abi*/(mtcsFuncAbi,real_insn).full_reg_clobbers ();
               /* CALL clobbers all call-used regs that aren't fixed except
               sp, ap, and fp.  Do this before setting the result of the
               call live.  */
               self->current_live_regs &= ~regs_invalidated_by_this_call;
            }

            /* A CALL_INSN sets any global register live, since it may
            have been modified by the call.  */
            for (i = 0; i < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i++)
               if (mtcsReg->global_regs/*!global_regs*/[i])
                  mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&self->current_live_regs, i);
         }

         /* Mark anything killed in an insn to be deadened at the next
         label.  Ignore USE insns; the only REG_DEAD notes will be for
         parameters.  But they might be early.  A CALL_INSN will usually
         clobber registers used for parameters.  It isn't worth bothering
         with the unlikely case when it won't.  */
         if ((NONJUMP_INSN_P (real_insn)  && GET_CODE (PATTERN (real_insn)) != USE
         && GET_CODE (PATTERN (real_insn)) != CLOBBER) || JUMP_P (real_insn) || CALL_P (real_insn)){
            for (link = REG_NOTES (real_insn); link; link = XEXP (link, 1))
               if (REG_NOTE_KIND (link) == REG_DEAD  && REG_P (XEXP (link, 0))
               && REGNO (XEXP (link, 0)) < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg))
                  mtcs_reg_add_to_hard_reg_set/*!add_to_hard_reg_set*/(mtcsReg,&self->pending_dead_regs,
                        GET_MODE (XEXP (link, 0)),REGNO (XEXP (link, 0)));

            mtcs_rtlanal_note_stores/*!note_stores*/(mtcsRtlanal,real_insn, updateLiveStatus_cb, (void*)self/*!NULL*/);

            /* If any registers were unused after this insn, kill them.
            These notes will always be accurate.  */
            for (link = REG_NOTES (real_insn); link; link = XEXP (link, 1))
               if (REG_NOTE_KIND (link) == REG_UNUSED && REG_P (XEXP (link, 0))
               && REGNO (XEXP (link, 0)) < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg))
                  mtcs_reg_remove_from_hard_reg_set/*!remove_from_hard_reg_set*/(mtcsReg,&self->current_live_regs,
                        GET_MODE (XEXP (link, 0)), REGNO (XEXP (link, 0)));
         }else if (LABEL_P (real_insn)){
            basic_block bb;

            /* A label clobbers the pending dead registers since neither
            reload nor jump will propagate a value across a label.  */
            self->current_live_regs &= ~self->pending_dead_regs;
            mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&self->pending_dead_regs);

            /* We must conservatively assume that all registers that used
            to be live here still are.  The fallthrough edge may have
            left a live register uninitialized.  */
            bb = BLOCK_FOR_INSN (real_insn);
            if (bb){
               HardRegSet/*!HARD_REG_SET*/ extra_live={mtcs_reg_get_hard_reg_element_count(mtcsReg)};
               mtcs_reg_reg_set_to_hard_reg_set/*!REG_SET_TO_HARD_REG_SET*/(mtcsReg,&extra_live, DF_LR_IN (bb));
               self->current_live_regs |= extra_live;
            }
         }
         /* The beginning of the epilogue corresponds to the end of the
         RTL chain when there are no epilogue insns.  Certain resources
         are implicitly required at that point.  */
         else if (NOTE_P (real_insn)&& NOTE_KIND (real_insn) == NOTE_INSN_EPILOGUE_BEG)
            self->current_live_regs |= self->start_of_epilogue_needs.regs;
      }//end  for (insn = start_insn; insn != stop_insn; insn = next_insn_no_annul (insn))

      res->regs = self->current_live_regs;
      if (tinfo != NULL){
         tinfo->block = b;
         tinfo->bb_tick = self->bb_ticks[b];
      }
   }else
      /* We didn't find the start of a basic block.  Assume everything
      in use.  This should happen only extremely rarely.  */
      mtcs_reg_set_hard_reg_set/*!SET_HARD_REG_SET*/(&res->regs);

   mtcs_resources_clean(&set);
   mtcs_resources_clean(&needed);

   rtx_insn *jump_insn = find_dead_or_set_registers(self,target, res, &jump_target,0, set, needed);

   /* If we hit an unconditional branch, we have another way of finding out
   what is live: we can see what is live at the branch target and include
   anything used but not set before the branch.  We add the live
   resources found using the test below to those found until now.  */

   if (jump_insn){
      ReorgResources/*!struct resources*/ new_resources;
      rtx_insn *stop_insn = next_active_insn (jump_insn);

      if (!ANY_RETURN_P (jump_target))
         jump_target = next_active_insn (as_a<rtx_insn *> (jump_target));
      mtcs_resource_mark_target_live_regs/*!mark_target_live_regs*/(self,insns, jump_target, &new_resources);
      mtcs_resources_clean(&set);
      mtcs_resources_clean(&needed);

      /* Include JUMP_INSN in the needed registers.  */
      for (insn = target; insn != stop_insn; insn = next_active_insn (insn)){
         mtcs_resource_mark_referenced_resources/*!mark_referenced_resources*/(self,insn, &needed, true);
         scratch = needed.regs & ~set.regs;
         new_resources.regs |= scratch;
         mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,insn, &set, 0, MARK_SRC_DEST_CALL);
      }
      res->regs |= new_resources.regs;
   }

   if (tinfo != NULL)
      tinfo->live_regs = res->regs;
}

/* Initialize the resources required by mark_target_live_regs ().
   This should be invoked before the first call to mark_target_live_regs.  */
//原型 init_resource_info resource.h resource.cc
void mtcs_resource_init_resource_info (MtcsResource *self,rtx_insn *epilogue_insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   int i;
   basic_block bb;

   /* Indicate what resources are required to be valid at the end of the current
   function.  The condition code never is and memory always is.
   The stack pointer is needed unless EXIT_IGNORE_STACK is true
   and there is an epilogue that restores the original stack pointer
   from the frame pointer.  Registers used to return the function value
   are needed.  Registers holding global variables are needed.  */

   self->end_of_function_needs.cc = 0;
   self->end_of_function_needs.memory = 1;
   mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&(self->end_of_function_needs.regs));

   if (mtcsRtlData->/*!frame_pointer_needed*/x_frame_pointer_needed){
      mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&self->end_of_function_needs.regs,
      mtcs_reg_get_frame_pointer_regnum/*!FRAME_POINTER_REGNUM*/(mtcsReg));
      if (!mtcs_reg_hard_frame_pointer_is_frame_pointer/*!HARD_FRAME_POINTER_IS_FRAME_POINTER*/(mtcsReg))
         mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&self->end_of_function_needs.regs,
               mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg));
   }
   if (!(mtcsRtlData->x_frame_pointer_needed/*!frame_pointer_needed*/
   && mtcs_func_get_exit_ignore_stack/*!EXIT_IGNORE_STACK*/(mtcsFunc)
   && epilogue_insn && !mtcsRtlData/*!crtl*/->sp_is_unchanging))
      mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&self->end_of_function_needs.regs,
            mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg));

   if (mtcsRtlData/*!crtl*/->return_rtx != 0)
      mtcs_resource_mark_referenced_resources/*!mark_referenced_resources*/(self,
            mtcsRtlData/*!crtl*/->return_rtx,&self->end_of_function_needs, true);

   for (i = 0; i < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i++)
      if (mtcsReg->global_regs/*!global_regs*/[i] || mtcs_dfscan_df_epilogue_uses_p/*!df_epilogue_uses_p*/(mtcsDfscan,i))
         mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&self->end_of_function_needs.regs, i);

   /* The registers required to be live at the end of the function are
   represented in the flow information as being dead just prior to
   reaching the end of the function.  For example, the return of a value
   might be represented by a USE of the return register immediately
   followed by an unconditional jump to the return label where the
   return label is the end of the RTL chain.  The end of the RTL chain
   is then taken to mean that the return register is live.

   This sequence is no longer maintained when epilogue instructions are
   added to the RTL chain.  To reconstruct the original meaning, the
   start of the epilogue (NOTE_INSN_EPILOGUE_BEG) is regarded as the
   point where these registers become live (start_of_epilogue_needs).
   If epilogue instructions are present, the registers set by those
   instructions won't have been processed by flow.  Thus, those
   registers are additionally required at the end of the RTL chain
   (end_of_function_needs).  */

   self->start_of_epilogue_needs = self->end_of_function_needs;

   while ((epilogue_insn = next_nonnote_insn (epilogue_insn))){
      mtcs_resource_mark_set_resources/*!mark_set_resources*/(self,
            epilogue_insn, &self->end_of_function_needs, 0,MARK_SRC_DEST_CALL);
      if (return_insn_p (epilogue_insn))
         break;
   }

   /* Filter-out the flags register from those additionally required
   registers. */
   if (mtcsTarget/*!targetm.flags_regnum*/->flags_regnum != INVALID_REGNUM)
      mtcs_reg_clear_hard_reg_bit/*!CLEAR_HARD_REG_BIT*/(mtcsReg,&self->end_of_function_needs.regs,
            mtcsTarget/*!targetm.flags_regnum*/->flags_regnum);

   /* Allocate and initialize the tables used by mark_target_live_regs.  */
   self->target_hash_table = XCNEWVEC (ReorgTargetInfo/*!struct target_info*/ *, TARGET_HASH_PRIME);
   self->bb_ticks = XCNEWVEC (int, last_basic_block_for_fn (cfun));

   /* Set the BLOCK_FOR_INSN of each label that starts a basic block.  */
   FOR_EACH_BB_FN (bb, cfun)
      if (LABEL_P (BB_HEAD (bb)))
         BLOCK_FOR_INSN (BB_HEAD (bb)) = bb;
}

/* Free up the resources allocated to mark_target_live_regs ().  This
   should be invoked after the last call to mark_target_live_regs ().  */
//原型 free_resource_info resource.h resource.cc
void mtcs_resource_free_resource_info (MtcsResource *self)
{
   basic_block bb;

   if (self->target_hash_table != NULL){
      int i;
      for (i = 0; i < TARGET_HASH_PRIME; ++i){
         ReorgTargetInfo/*!struct target_info*/ *ti = self->target_hash_table[i];
         while (ti){
            ReorgTargetInfo/*!struct target_info*/  *next = ti->next;
            free (ti);
            ti = next;
         }
      }
      free (self->target_hash_table);
      self->target_hash_table = NULL;
   }

   if (self->bb_ticks != NULL){
      free (self->bb_ticks);
      self->bb_ticks = NULL;
   }
   FOR_EACH_BB_FN (bb, cfun)
      if (LABEL_P (BB_HEAD (bb)))
         BLOCK_FOR_INSN (BB_HEAD (bb)) = NULL;
}

/* Clear any hashed information that we have stored for INSN.  */
//原型 mark_end_of_function_resources resource.h resource.cc
void mtcs_resource_clear_hashed_info_for_insn (MtcsResource *self,rtx_insn *insn)
{
   ReorgTargetInfo/*!struct target_info*/ *tinfo;
   if (self->target_hash_table != NULL){
      for (tinfo = self->target_hash_table[INSN_UID (insn) % TARGET_HASH_PRIME]; tinfo; tinfo = tinfo->next)
         if (tinfo->uid == INSN_UID (insn))
            break;
      if (tinfo)
         tinfo->block = -1;
   }
}

/* Clear any hashed information that we have stored for instructions
   between INSN and the next BARRIER that follow a JUMP or a LABEL.  */
//原型 clear_hashed_info_until_next_barrier resource.h resource.cc
void  mtcs_resource_clear_hashed_info_until_next_barrier (MtcsResource *self,rtx_insn *insn)
{
   while (insn && !BARRIER_P (insn)){
      if (JUMP_P (insn) || LABEL_P (insn)){
         rtx_insn *next = next_active_insn (insn);
         if (next)
            mtcs_resource_clear_hashed_info_for_insn/*!clear_hashed_info_for_insn*/(self,next);
      }
      insn = next_nonnote_insn (insn);
   }
}

/* Increment the tick count for the basic block that contains INSN.  */
//原型 incr_ticks_for_insn resource.h resource.cc
void mtcs_resource_incr_ticks_for_insn (MtcsResource *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   int b = find_basic_block (insn, mtcsOptionsItem->x_param_max_delay_slot_live_search);
   if (b != -1)
      self->bb_ticks[b]++;
}

/* Add TRIAL to the set of resources used at the end of the current
   function.  */
//原型 mark_end_of_function_resources resource.h resource.cc
void mtcs_resource_mark_end_of_function_resources (MtcsResource *self,rtx trial, bool include_delayed_effects)
{
  mtcs_resource_mark_referenced_resources/*!mark_referenced_resources*/(self,
        trial, &self->end_of_function_needs,include_delayed_effects);
}

/* Macro to clear all resources.  */
//原型 #define CLEAR_RESOURCE(RES)   \
// do { (RES)->memory = (RES)->volatil = (RES)->cc = 0; \
//      CLEAR_HARD_REG_SET ((RES)->regs); } while (0)
void mtcs_resources_clean(ReorgResources *res)
{
   res->memory = res->volatil = res->cc = 0;
   mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&res->regs);
}

MtcsResource *mtcs_resource_new(MtcsMode *mtcsMode)
{
     MtcsResource *self = n_slice_alloc0 (sizeof(MtcsResource));
     mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
     mtcsResourceInit(self);
     return self;
}


