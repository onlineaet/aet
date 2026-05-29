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
 * base on recog.cc
 */


/* This file handles generation of all the assembler code
   *except* the instructions of a recogtion.
   This includes declarations of variables and their initial values.

   We also output the assembler code for constants stored in memory
   and are responsible for combining constants with the same value.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "tree.h"
#include "rtl.h"
#include "predict.h"
#include "memmodel.h"
#include "tm_p.h"
#include "stringpool.h"
#include "regs.h"
#include "emit-rtl.h"
#include "cgraph.h"
#include "diagnostic-core.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "varasm.h"
#include "version.h"
#include "flags.h"
#include "stmt.h"
#include "expr.h"
#include "expmed.h"
#include "optabs.h"
#include "output.h"
#include "langhooks.h"
#include "debug.h"
#include "common/common-target.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "rtl-iter.h"
#include "file-prefix-map.h" /* remap_debug_filename()  */
#include "alloc-pool.h"
#include "toplev.h"
#include "opts.h"
#include "asan.h"
#include "recog.h"
#include "dwarf2out.h"
#include "dwarf2.h"
#include "dwarf2asm.h"
#include "except.h"
#include "reload.h"
#include "df.h"

#include "mtcsrecog.h"
#include "mtcstarget.h"
#include "mtcsprintrtl.h"

struct change_t
{
  rtx object;
  int old_code;
  int old_len;
  bool unshare;
  rtx *loc;
  rtx old;
};

void mtcs_recog_init(MtcsRecog *self)
{
   self->num_changes = 0;
   self->temporarily_undone_changes=0;
   self->changes=NULL;
}

static void df_bb_verify (basic_block bb)
{
   rtx_insn *insn;

   n_debug("mtcsrecog.c 打印每个bb中的insn bb:%p bb->index:%d\n",bb,bb->index);
   /* Scan the block, one insn at a time, from beginning to end.  */
   int count=0;
   FOR_BB_INSNS_REVERSE (bb, insn){
      n_debug("mtcsrecog.c 打印每个bb中的count:%d insn:%p \n",count,insn);
      if (!INSN_P (insn)){
         continue;
      }
      count++;
      mtcs_print_rtl_single(stderr,insn);
     // print_rtl_single(stderr,insn);

   }
}

static void testprint()
{
   return;
   if(!cfun )
      return;
   basic_block bb;
   FOR_ALL_BB_FN (bb, cfun)
      df_bb_verify(bb);
}

/* Try to process the address of memory expression MEM.  Return true on
   success; leave the caller to clean up on failure.  */

bool mtcs_insn_propagation::apply_to_mem_1 (rtx mem)
{
   auto old_num_changes = mtcs_recog_num_validated_changes/*!num_validated_changes*/(mtcsRecog);
   mem_depth += 1;
   n_debug("mtcsrecog.c apply_to_mem_1 mem:%p XEXP (mem, 0):%p\n",mem,XEXP (mem, 0));
   bool res = apply_to_rvalue_1 (&XEXP (mem, 0));
   mem_depth -= 1;
   if (!res)
      return false;

   if (old_num_changes != mtcs_recog_num_validated_changes/*!num_validated_changes*/(mtcsRecog)
   && should_check_mems  && !check_mem (old_num_changes, mem))
      return false;

   return true;
}

/* Try to process the rvalue expression at *LOC.  Return true on success;
   leave the caller to clean up on failure.  */
bool mtcs_insn_propagation::apply_to_rvalue_1 (rtx *loc)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsRecog);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   rtx x = *loc;
   enum rtx_code code = GET_CODE (x);
   machine_mode mode = GET_MODE (x);

   auto old_num_changes = mtcs_recog_num_validated_changes/*!num_validated_changes*/(mtcsRecog);
   if (from    && GET_CODE (x) == GET_CODE (from)
   && (REG_P (x)  ? REGNO (x) == REGNO (from) : rtx_equal_p (x, from))){
      /* Don't replace register asms in asm statements; we mustn't
      change the user's register allocation.  */
      if (REG_P (x)  && mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,x)
            && register_asm_p (x)  && asm_noperands (PATTERN (insn)) > 0)
         return false;

      rtx newval = to;
      if (GET_MODE (x) != GET_MODE (from)){
         gcc_assert (REG_P (x) && mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,x));
         if (REG_NREGS (x) != REG_NREGS (from)
               || !mtcs_reg_can_change_mode/*!REG_CAN_CHANGE_MODE_P*/(mtcsReg,REGNO (x), GET_MODE (from),GET_MODE (x)))
            return false;

         /* If the reference is paradoxical and the replacement
         value contains registers, we would need to check that the
         simplification below does not increase REG_NREGS for those
         registers either.  It seems simpler to punt on nonconstant
         values instead.  */
         if (mtcs_mode_paradoxical_subreg_p/*!paradoxical_subreg_p*/(mtcsMode,
               GET_MODE (x), GET_MODE (from))   && !CONSTANT_P (to))
            return false;
         n_debug("mtcsrecog.c apply_to_rvalue_1 创建新的 newval--- to:%p\n",to);
         newval = mtcs_simplify_rtx_subreg/*!simplify_subreg*/(mtcsSimplifyRtx,
               GET_MODE (x), to, GET_MODE (from),mtcs_mode_subreg_lowpart_offset/*!subreg_lowpart_offset*/(mtcsMode,
                     GET_MODE (x), GET_MODE (from)));
         if (!newval)
            return false;

         /* Check that the simplification didn't just push an explicit
         subreg down into subexpressions.  In particular, for a register
         R that has a fixed mode, such as the stack pointer, a subreg of:

         (plus:M (reg:M R) (const_int C))

         would be:

         (plus:N (subreg:N (reg:M R) ...) (const_int C'))

         But targets can legitimately assume that subregs of hard registers
         will not be created after RA (except in special circumstances,
         such as strict_low_part).  */
         subrtx_iterator::array_type array;
         FOR_EACH_SUBRTX (iter, array, newval, NONCONST)
            if (GET_CODE (*iter) == SUBREG)
               return false;
      }
      n_debug("mtcsrecog.c apply_to_rvalue_1 00 should_unshare:%d x:%p insn:%p\n",should_unshare,x,insn);
      mtcs_print_rtl_single(stderr,x);
      testprint();
      if (should_unshare)
         mtcs_recog_validate_unshare_change/*!validate_unshare_change*/(mtcsRecog,insn, loc, newval, 1);
      else
         mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, loc, newval, 1);
      n_debug("mtcsrecog.c apply_to_rvalue_1 00xx should_unshare:%d\n",should_unshare);
       testprint();
      if (mem_depth && !REG_P (newval) && !CONSTANT_P (newval)){
         n_debug("mtcsrecog.c mtcs_insn_propagation xxx should_unshare:%d mem_depth:%d\n",should_unshare,mem_depth);

         /* We're substituting into an address, but TO will have the
         form expected outside an address.  Canonicalize it if
         necessary.  */
         mtcs_insn_propagation subprop (insn);
         subprop.mtcsRecog = mtcsRecog;
         subprop.mem_depth += 1;
         if (!subprop.apply_to_rvalue (loc))
            gcc_unreachable ();
         if (should_unshare
         && mtcs_recog_num_validated_changes/*!num_validated_changes*/(mtcsRecog) != old_num_changes + 1){
            n_debug("mtcsrecog.c mtcs_insn_propagation yyy should_unshare:%d mem_depth:%d\n",should_unshare,mem_depth);

            /* TO is owned by someone else, so create a copy and
            return TO to its original form.  */
            newval = copy_rtx (*loc);
            mtcs_recog_cancel_changes/*!cancel_changes*/(mtcsRecog,old_num_changes);
            mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, loc, newval, 1);
         }
      }
      num_replacements += 1;
      should_unshare = true;
      result_flags |= UNSIMPLIFIED;
      return true;
   }
   n_debug("mtcsrecog.c apply_to_rvalue_1 11 *loc:%p x:%p from:%p to:%p\n",*loc,x,from,to);
   mtcs_print_rtl_single(stderr,*loc);
   /* Recursively apply the substitution and see if we can simplify
   the result.  This specifically shouldn't use simplify_gen_* for
   speculative simplifications, since we want to avoid generating new
   expressions where possible.  */
   auto old_result_flags = result_flags;
   rtx newx = NULL_RTX;
   bool recurse_p = false;
   switch (GET_RTX_CLASS (code)){
      case RTX_UNARY:
      {
         machine_mode op0_mode = GET_MODE (XEXP (x, 0));
         if (!apply_to_rvalue_1 (&XEXP (x, 0)))
            return false;
         if (from && old_num_changes == mtcs_recog_num_validated_changes/*!num_validated_changes*/(mtcsRecog))
            return true;

         newx = mtcs_simplify_rtx_unary_operation/*!simplify_unary_operation*/(mtcsSimplifyRtx,code, mode, XEXP (x, 0), op0_mode);
         break;
      }

      case RTX_BIN_ARITH:
      case RTX_COMM_ARITH:
      {
         if (!apply_to_rvalue_1 (&XEXP (x, 0)) || !apply_to_rvalue_1 (&XEXP (x, 1)))
            return false;
         if (from && old_num_changes == mtcs_recog_num_validated_changes/*!num_validated_changes*/(mtcsRecog))
            return true;

         if (GET_RTX_CLASS (code) == RTX_COMM_ARITH
         && mtcs_rtlanal_swap_commutative_operands_p/*!swap_commutative_operands_p*/(mtcsRtlanal,XEXP (x, 0), XEXP (x, 1)))
            newx = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,code, mode, XEXP (x, 1), XEXP (x, 0));
         else
            newx = mtcs_simplify_rtx_binary_operation/*!simplify_binary_operation*/(mtcsSimplifyRtx,code, mode,
         XEXP (x, 0), XEXP (x, 1));
         break;
      }

      case RTX_COMPARE:
      case RTX_COMM_COMPARE:
      {
         machine_mode op_mode = (GET_MODE (XEXP (x, 0)) != VOIDmode
            ? GET_MODE (XEXP (x, 0))
            : GET_MODE (XEXP (x, 1)));
         if (!apply_to_rvalue_1 (&XEXP (x, 0))
         || !apply_to_rvalue_1 (&XEXP (x, 1)))
            return false;
         if (from && old_num_changes == mtcs_recog_num_validated_changes/*!num_validated_changes*/(mtcsRecog))
            return true;

         newx =mtcs_simplify_rtx_relational_operation/*!simplify_relational_operation*/(mtcsSimplifyRtx,
               code, mode, op_mode,XEXP (x, 0), XEXP (x, 1));
         break;
      }

      case RTX_TERNARY:
      case RTX_BITFIELD_OPS:
      {
         machine_mode op0_mode = GET_MODE (XEXP (x, 0));
         if (!apply_to_rvalue_1 (&XEXP (x, 0))
            || !apply_to_rvalue_1 (&XEXP (x, 1))
            || !apply_to_rvalue_1 (&XEXP (x, 2)))
            return false;
         if (from && old_num_changes == mtcs_recog_num_validated_changes/*!num_validated_changes*/(mtcsRecog))
            return true;

         newx = mtcs_simplify_rtx_ternary_operation/*!simplify_ternary_operation*/(mtcsSimplifyRtx,
               code, mode, op0_mode,XEXP (x, 0), XEXP (x, 1),XEXP (x, 2));
         break;
      }

      case RTX_EXTRA:
         if (code == SUBREG){
            machine_mode inner_mode = GET_MODE (SUBREG_REG (x));
            if (!apply_to_rvalue_1 (&SUBREG_REG (x)))
               return false;
            if (from && old_num_changes == mtcs_recog_num_validated_changes/*!num_validated_changes*/(mtcsRecog))
               return true;

            rtx inner = SUBREG_REG (x);
            newx = mtcs_simplify_rtx_subreg/*!simplify_subreg*/(mtcsSimplifyRtx,mode, inner, inner_mode, SUBREG_BYTE (x));
            /* Reject the same cases that simplify_gen_subreg would.  */
            if (!newx
            && (GET_CODE (inner) == SUBREG
            || GET_CODE (inner) == CONCAT
            || GET_MODE (inner) == VOIDmode
            || !mtcs_rtl_validate_subreg/*!validate_subreg*/(mtcsRTL,mode, inner_mode,inner, SUBREG_BYTE (x)))){
               failure_reason = "would create an invalid subreg";
               return false;
            }
            break;
         }else
            recurse_p = true;
         break;

      case RTX_OBJ:
         if (code == LO_SUM){
            if (!apply_to_rvalue_1 (&XEXP (x, 0)) || !apply_to_rvalue_1 (&XEXP (x, 1)))
               return false;
            if (from && old_num_changes == mtcs_recog_num_validated_changes/*!num_validated_changes*/(mtcsRecog))
               return true;

            /* (lo_sum (high x) y) -> y where x and y have the same base.  */
            rtx op0 = XEXP (x, 0);
            rtx op1 = XEXP (x, 1);
            if (GET_CODE (op0) == HIGH){
               rtx base0, base1, offset0, offset1;
               split_const (XEXP (op0, 0), &base0, &offset0);
               split_const (op1, &base1, &offset1);
               if (rtx_equal_p (base0, base1))
                  newx = op1;
            }
         }else if (code == REG){
            if (from && REG_P (from) && reg_overlap_mentioned_p (x, from)){
               failure_reason = "inexact register overlap";
               return false;
            }
         }else if (code == MEM)
            return apply_to_mem_1 (x);
         else
            recurse_p = true;
         break;

      case RTX_CONST_OBJ:
         break;

      case RTX_AUTOINC:
         if (from && reg_overlap_mentioned_p (XEXP (x, 0), from)){
            failure_reason = "is subject to autoinc";
            return false;
         }
         recurse_p = true;
         break;

      case RTX_MATCH:
      case RTX_INSN:
         gcc_unreachable ();
   }

   if (recurse_p){
      const char *fmt = GET_RTX_FORMAT (code);
      for (int i = 0; fmt[i]; i++)
         switch (fmt[i]){
            case 'E':
               for (int j = 0; j < XVECLEN (x, i); j++)
                  if (!apply_to_rvalue_1 (&XVECEXP (x, i, j)))
                     return false;
               break;

            case 'e':
               if (XEXP (x, i) && !apply_to_rvalue_1 (&XEXP (x, i)))
                  return false;
               break;
         }
   }else if (newx && !rtx_equal_p (x, newx)){
      /* All substitutions made by OLD_NUM_CHANGES onwards have been
      simplified.  */
      result_flags = ((result_flags & ~UNSIMPLIFIED)  | (old_result_flags & UNSIMPLIFIED));

      if (should_note_simplifications)
         note_simplification (old_num_changes, old_result_flags, x, newx);

      /* There's no longer any point unsharing the substitutions made
      for subexpressions, since we'll just copy this one instead.  */
      bool unshare = false;
      change_t *changes_t=(change_t *)mtcsRecog->changes;
      for (int i = old_num_changes; i < mtcsRecog->num_changes; ++i){
         unshare |= changes_t[i].unshare;
         changes_t[i].unshare = false;
      }
      n_debug("mtcsrecog.c apply_to_rvalue_1 22 x:%p\n",x);
       testprint();
      if (unshare)
         mtcs_recog_validate_unshare_change/*!validate_unshare_change*/(mtcsRecog,insn, loc, newx, 1);
      else
         mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, loc, newx, 1);
   }
   n_debug("mtcsrecog.c apply_to_rvalue_1 33 x:%p\n",x);
    testprint();
   return true;
}

/* Try to process the lvalue expression at *LOC.  Return true on success;
   leave the caller to clean up on failure.  */

bool mtcs_insn_propagation::apply_to_lvalue_1 (rtx dest)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsRecog);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   rtx old_dest = dest;
   while (GET_CODE (dest) == SUBREG || GET_CODE (dest) == ZERO_EXTRACT || GET_CODE (dest) == STRICT_LOW_PART){
      if (GET_CODE (dest) == ZERO_EXTRACT && (!apply_to_rvalue_1 (&XEXP (dest, 1))  || !apply_to_rvalue_1 (&XEXP (dest, 2))))
         return false;
      dest = XEXP (dest, 0);
   }
   n_debug("mtcsrecog.c apply_to_lvalue_1 00 dest:%p MEM_P (dest):%p\n",dest,MEM_P (dest));
   testprint();
   if (MEM_P (dest))
      return apply_to_mem_1 (dest);

   /* Check whether the substitution is safe in the presence of this lvalue.  */
   if (!from
   || dest == old_dest
   || !REG_P (dest)
   || !mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,dest, from)){
      n_debug("mtcsrecog.c apply_to_lvalue_1 11 dest:%p\n",dest);
      testprint();
      return true;
   }
   n_debug("mtcsrecog.c apply_to_lvalue_1 22 dest:%p\n",dest);
    testprint();
   if (SUBREG_P (old_dest)
   && SUBREG_REG (old_dest) == dest
   && !mtcs_rtlanal_read_modify_subreg_p/*!read_modify_subreg_p*/(mtcsRtlanal,old_dest))
      return true;
   n_debug("mtcsrecog.c apply_to_lvalue_1 33 dest:%p\n",dest);
    testprint();
   failure_reason = "is part of a read-write destination";
   return false;
}

/* Try to process the instruction pattern at *LOC.  Return true on success;
   leave the caller to clean up on failure.  */

bool mtcs_insn_propagation::apply_to_pattern_1 (rtx *loc)
{
   rtx body = *loc;
   n_debug("mtcsrecog.c insn_propagation::apply_to_pattern_1 *loc:%p code:%d %s\n",*loc,GET_CODE(body),GET_RTX_NAME(GET_CODE(body)));
   mtcs_print_rtl_single(stderr,body);
   switch (GET_CODE (body)){
      case COND_EXEC:
         return (apply_to_rvalue_1 (&COND_EXEC_TEST (body))  && apply_to_pattern_1 (&COND_EXEC_CODE (body)));

      case PARALLEL:
         for (int i = 0; i < XVECLEN (body, 0); ++i){
            rtx *subloc = &XVECEXP (body, 0, i);
            if (GET_CODE (*subloc) == SET){
               if (!apply_to_lvalue_1 (SET_DEST (*subloc)))
                  return false;
               /* ASM_OPERANDS are shared between SETs in the same PARALLEL.
               Only process them on the first iteration.  */
               if ((i == 0 || GET_CODE (SET_SRC (*subloc)) != ASM_OPERANDS)  && !apply_to_rvalue_1 (&SET_SRC (*subloc)))
                  return false;
            }else{
               if (!apply_to_pattern_1 (subloc))
                  return false;
            }
         }
         return true;

      case ASM_OPERANDS:
         for (int i = 0, len = ASM_OPERANDS_INPUT_LENGTH (body); i < len; ++i)
            if (!apply_to_rvalue_1 (&ASM_OPERANDS_INPUT (body, i)))
               return false;
         return true;

      case CLOBBER:
         return apply_to_lvalue_1 (XEXP (body, 0));

      case SET:
         return (apply_to_lvalue_1 (SET_DEST (body))  && apply_to_rvalue_1 (&SET_SRC (body)));

      default:
         /* All the other possibilities never store and can use a normal
         rtx walk.  This includes:

         - USE
         - TRAP_IF
         - PREFETCH
         - UNSPEC
         - UNSPEC_VOLATILE.  */
         return apply_to_rvalue_1 (loc);
   }
}

/* Apply this insn_propagation object's simplification or substitution
   to the instruction pattern at LOC.  */

bool mtcs_insn_propagation::apply_to_pattern (rtx *loc)
{
   unsigned int num_changes = mtcs_recog_num_validated_changes/*!num_validated_changes*/(mtcsRecog);
   n_debug("mtcsrecog.c nsn_propagation::apply_to_pattern 00 *loc:%p num_changes:%d\n",*loc,num_changes);
   mtcs_print_rtl_single(stderr,*loc);
   bool res = apply_to_pattern_1 (loc);
   n_debug("mtcsrecog.c nsn_propagation::apply_to_pattern 11 *loc:%p num_changes:%d res:%d\n",*loc,num_changes,res);
   mtcs_print_rtl_single(stderr,*loc);
   if (!res)
      mtcs_recog_cancel_changes/*!cancel_changes*/(mtcsRecog,num_changes);
   return res;
}

/* Apply this insn_propagation object's simplification or substitution
   to the rvalue expression at LOC.  */

bool mtcs_insn_propagation::apply_to_rvalue (rtx *loc)
{
   unsigned int num_changes = mtcs_recog_num_validated_changes/*!num_validated_changes*/(mtcsRecog);
   bool res = apply_to_rvalue_1 (loc);
   if (!res)
      mtcs_recog_cancel_changes/*!cancel_changes*/(mtcsRecog,num_changes);
   return res;
}

/* Like apply_to_rvalue, but specifically for the case where *LOC is in
   a note.  This never changes the INSN_CODE.  */

bool mtcs_insn_propagation::apply_to_note (rtx *loc)
{
   auto old_code = INSN_CODE (insn);
   bool res = apply_to_rvalue (loc);
   if (INSN_CODE (insn) != old_code)
      INSN_CODE (insn) = old_code;
   return res;
}


/* Return true if labels in asm operands BODY are LABEL_REFs.  */
//原型 asm_labels_ok recog.cc
static bool asm_labels_ok (rtx body)
{
   rtx asmop;
   int i;

   asmop = extract_asm_operands (body);
   if (asmop == NULL_RTX)
      return true;

   for (i = 0; i < ASM_OPERANDS_LABEL_LENGTH (asmop); i++)
      if (GET_CODE (ASM_OPERANDS_LABEL (asmop, i)) != LABEL_REF)
         return false;

   return true;
}

//原型 memory_address_addr_space_p recog.h 实现recog.cc
bool mtcs_recog_memory_address_addr_space_p (MtcsRecog *self,mtcs_mode mode ATTRIBUTE_UNUSED, rtx addr,
                         addr_space_t as, code_helper ch ATTRIBUTE_UNUSED)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

    //GO_IF_LEGITIMATE_ADDRESS host=0 nvptx=0
#ifdef GO_IF_LEGITIMATE_ADDRESS
  gcc_assert (ADDR_SPACE_GENERIC_P (as));
  GO_IF_LEGITIMATE_ADDRESS (mode, addr, win);
  return false;

 win:
  return true;
#else
  //return targetm.addr_space.legitimate_address_p (mode, addr, 0, as, ch);
  return target_addr_space_legitimate_address_p(mtcsMachine->addrSpace,mode,addr,0,as,ch);
#endif
}

//原型 #define memory_address_p(mode,addr)   memory_address_addr_space_p ((mode), (addr), ADDR_SPACE_GENERIC)
bool mtcs_recog_memory_address_p (MtcsRecog *self,mtcs_mode mode ATTRIBUTE_UNUSED, rtx addr)
{
   return mtcs_recog_memory_address_addr_space_p(self,mode,addr,ADDR_SPACE_GENERIC);
}

//原型 strict_memory_address_addr_space_p recog.h reload.cc
bool mtcs_recog_strict_memory_address_addr_space_p (MtcsRecog *self,machine_mode mode ATTRIBUTE_UNUSED,
                    rtx addr, addr_space_t as, code_helper)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

#ifdef GO_IF_LEGITIMATE_ADDRESS
  gcc_assert (ADDR_SPACE_GENERIC_P (as));
  GO_IF_LEGITIMATE_ADDRESS (mode, addr, win);
  return false;

 win:
  return true;
#else
  //return targetm.addr_space.legitimate_address_p (mode, addr, 1, as,ERROR_MARK);
  return target_addr_space_legitimate_address_p(mtcsMachine->addrSpace,mode, addr, 1, as,ERROR_MARK);
#endif
}

//原型 #define strict_memory_address_p(mode,addr)
//mtcs_recog_strict_memory_address_addr_space_p ((mode), (addr), ADDR_SPACE_GENERIC) recog.h
bool mtcs_recog_strict_memory_address_p (MtcsRecog *self,machine_mode mode ATTRIBUTE_UNUSED,rtx addr)
{
   return mtcs_recog_strict_memory_address_addr_space_p(self,mode,addr,ADDR_SPACE_GENERIC);
}

//原型 LEGITIMATE_PIC_OPERAND_P default.h 每个平台都有
nboolean mtcs_recog_is_legitimate_pic_operand_p(MtcsRecog *self,rtx op)
{
    return self->is_legitimate_pic_operand_p(self,op);
}

/* Return true if Y is a memory address which contains no side effects
   and would remain valid for address space AS after the addition of
   a positive integer less than the size of that mode.

   We assume that the original address is valid and do not check it.
   We do check that it is valid for narrower modes.

   If STRICTP is nonzero, we require a strictly valid address,
   for the sake of use in reload.cc.  */
//原型 offsettable_address_addr_space_p recog.h recog.cc
bool mtcs_recog_offsettable_address_addr_space_p (MtcsRecog *self,int strictp, machine_mode mode, rtx y,addr_space_t as)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
  MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  enum rtx_code ycode = GET_CODE (y);
  rtx z;
  rtx y1 = y;
  rtx *y2;
  bool (*addressp) (MtcsRecog *,machine_mode, rtx, addr_space_t, code_helper) =
    (strictp ? mtcs_recog_strict_memory_address_addr_space_p: mtcs_recog_memory_address_addr_space_p);
  poly_int64 mode_sz = mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,mode);

  if (mtcs_rtl_constant_address_p/*!CONSTANT_ADDRESS_P*/(mtcsRTL,y))
    return true;

  /* Adjusting an offsettable address involves changing to a narrower mode.
     Make sure that's OK.  */

  if (mtcs_recog_mode_dependent_address_p/*!mode_dependent_address_p*/(self,y, as))
    return false;

  machine_mode address_mode = GET_MODE (y);
  if (address_mode == VOIDmode)
    address_mode = target_addr_space_address_mode/*!targetm.addr_space.address_mode*/(mtcsMachine->addrSpace,as);

  machine_mode pointer_mode =0;
  if(mtcs_config_ifdef(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED)){
      pointer_mode = target_addr_space_address_mode/*!targetm.addr_space.address_mode*/(mtcsMachine->addrSpace,as);
  }
/*!#ifdef POINTERS_EXTEND_UNSIGNED host=1 nvptx=0
  machine_mode pointer_mode = targetm.addr_space.pointer_mode (as);
#endif
*/

  /* ??? How much offset does an offsettable BLKmode reference need?
     Clearly that depends on the situation in which it's being used.
     However, the current situation in which we test 0xffffffff is
     less than ideal.  Caveat user.  */
  if (known_eq (mode_sz, 0))
    mode_sz = mtcs_align_get_biggest_alignment/*!BIGGEST_ALIGNMENT*/(mtcsAlign)/BITS_PER_UNIT;

  /* If the expression contains a constant term,
     see if it remains valid when max possible offset is added.  */

  if ((ycode == PLUS) && (y2 = find_constant_term_loc (&y1))){
      bool good;

      y1 = *y2;
      *y2 = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,address_mode, *y2, mode_sz - 1);
      /* Use QImode because an odd displacement may be automatically invalid
     for any wider mode.  But it should be valid for a single byte.  */
      good = (*addressp) (self,mtcsMode->modes.M_QImode, y, as, ERROR_MARK);

      /* In any case, restore old contents of memory.  */
      *y2 = y1;
      return good;
  }

  if (GET_RTX_CLASS (ycode) == RTX_AUTOINC)
    return false;

  /* The offset added here is chosen as the maximum offset that
     any instruction could need to add when operating on something
     of the specified mode.  We assume that if Y and Y+c are
     valid addresses then so is Y+d for all 0<d<c.  adjust_address will
     go inside a LO_SUM here, so we do so as well.  */
  if (GET_CODE (y) == LO_SUM  && mode != mtcsMode->modes.M_BLKmode
           && known_le (mode_sz,mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode) / BITS_PER_UNIT))
    z = gen_rtx_LO_SUM (address_mode, XEXP (y, 0),mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,address_mode, XEXP (y, 1),mode_sz - 1));

//#ifdef POINTERS_EXTEND_UNSIGNED host=1 nvptx=0
  /* Likewise for a ZERO_EXTEND from pointer_mode.  */
  else if (mtcs_config_ifdef(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED)/*!#ifdef POINTERS_EXTEND_UNSIGNED host=1 nvptx=0*/
          && mtcs_config_get_value(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED)/*!POINTERS_EXTEND_UNSIGNED*/ > 0
          && GET_CODE (y) == ZERO_EXTEND
          && GET_MODE (XEXP (y, 0)) == pointer_mode)
    z = gen_rtx_ZERO_EXTEND (address_mode,
            mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pointer_mode, XEXP (y, 0),mode_sz - 1));
/*!#endif*/
  else
    z = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,address_mode, y, mode_sz - 1);

  /* Use QImode because an odd displacement may be automatically invalid
     for any wider mode.  But it should be valid for a single byte.  */
  return (*addressp) (self,mtcsMode->modes.M_QImode, z, as, ERROR_MARK);
}

/* Return true if ADDR is an address-expression whose effect depends
   on the mode of the memory reference it is used in.

   ADDRSPACE is the address space associated with the address.

   Autoincrement addressing is a typical example of mode-dependence
   because the amount of the increment depends on the mode.  */
//原型 mode_dependent_address_p recog.h recog.cc
bool mtcs_recog_mode_dependent_address_p (MtcsRecog *self,rtx addr, addr_space_t addrspace)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  /* Auto-increment addressing with anything other than post_modify
     or pre_modify always introduces a mode dependency.  Catch such
     cases now instead of deferring to the target.  */
  if (GET_CODE (addr) == PRE_INC
      || GET_CODE (addr) == POST_INC
      || GET_CODE (addr) == PRE_DEC
      || GET_CODE (addr) == POST_DEC)
    return true;

  return mtcsTarget->/*!targetm.*/mode_dependent_address_p(mtcsTarget,addr, addrspace);
}

/* Similar, but don't require a strictly valid mem ref:
   consider pseudo-regs valid as index or base regs.  */
//原型 offsettable_nonstrict_memref_p recog.h recog.cc
bool mtcs_recog_offsettable_nonstrict_memref_p (MtcsRecog *self,rtx op)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  return ((MEM_P (op))
      && mtcs_recog_offsettable_address_addr_space_p (self,0, GET_MODE (op), XEXP (op, 0),
              mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,op)));
}

//原型 extern void add_clobbers (rtx, int); recog.h
void mtcs_recog_add_clobbers (MtcsRecog *self,rtx pattern ATTRIBUTE_UNUSED, int insn_code_number)
{
     self->add_clobbers(self,pattern,insn_code_number);
}
//原型 extern bool added_clobbers_hard_reg_p (int);recog.h
bool mtcs_recog_added_clobbers_hard_reg_p (MtcsRecog *self,int insn_code_number)
{
    return self->added_clobbers_hard_reg_p(self,insn_code_number);
}

//原型extern rtx_insn *peephole2_insns (rtx, rtx_insn *, int *); recog.h insn-recog.cc
rtx_insn *mtcs_recog_peephole2_insns (MtcsRecog *self, rtx x1 ATTRIBUTE_UNUSED,
    rtx_insn *insn ATTRIBUTE_UNUSED, int *pmatch_len_ ATTRIBUTE_UNUSED)
{
    return self->peephole2_insns(self,x1,insn,pmatch_len_);
}

//原型rtx_insn *split_insns (rtx x1 ATTRIBUTE_UNUSED, rtx_insn *insn ATTRIBUTE_UNUSED) recog.h insn-recog.cc
rtx_insn *mtcs_recog_split_insns (MtcsRecog *self,rtx x1 ATTRIBUTE_UNUSED, rtx_insn *insn ATTRIBUTE_UNUSED)
{
    return self->split_insns(self,x1,insn);
}

//原型int recog (rtx x1 ATTRIBUTE_UNUSED,rtx_insn *insn ATTRIBUTE_UNUSED,int *pnum_clobbers ATTRIBUTE_UNUSED) recog.h insn-recog.cc
int mtcs_recog_recog (MtcsRecog *self,rtx x1 ATTRIBUTE_UNUSED,rtx_insn *insn ATTRIBUTE_UNUSED,int *pnum_clobbers ATTRIBUTE_UNUSED)
{
    return self->recog(self,x1,insn,pnum_clobbers);
}

//原型 recog_init recog.h recog.cc
void mtcs_recog_recog_init (MtcsRecog *self)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
     MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
     MtcsCodes  *mtcsCodes=mtcs_target_get_codes(mtcsTarget);
  /* The information is zero-initialized, so we don't need to do anything
     first time round.  */
  if (!self->target_recog.x_initialized){
      self->target_recog.x_initialized = true;
      return;
  }
  nuint numInsnCodes= mtcs_codes_get_number(mtcsCodes);
  memset (self->target_recog.x_bool_attr_masks, 0,sizeof (self->target_recog.x_bool_attr_masks));
  for (unsigned int i = 0; i < numInsnCodes; ++i)
    if (self->target_recog.x_op_alt[i]){
        free (self->target_recog.x_op_alt[i]);
        self->target_recog.x_op_alt[i] = 0;
    }
}

/* Initialize data used by the function `recog'.
   This must be called once in the compilation of a function
   before any insn recognition may be done in the function.  */
//原型 init_recog_no_volatile recog.h recog.cc
void mtcs_recog_init_recog_no_volatile (MtcsRecog *self)
{
  self->volatile_ok = 0;
}

//原型 init_recog recog.h recog.cc
void mtcs_recog_init_recog (MtcsRecog *self)
{
  self->volatile_ok = 1;
}


/* Check if an asm_operand matches its constraints.
   Return > 0 if ok, = 0 if bad, < 0 if inconclusive.  */
//原型 asm_operand_ok recog.h recog.cc
int mtcs_recog_asm_operand_ok (MtcsRecog *self,rtx op, const char *constraint, const char **constraints)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);

  int result = 0;
  bool incdec_ok = false;
  /* Use constrain_operands after reload.  */
  gcc_assert (!reload_completed);
  /* Empty constraint string is the same as "X,...,X", i.e. X for as
     many alternatives as required to match the other operands.  */
  if (*constraint == '\0')
    result = 1;

  while (*constraint){
      enum constraint_num cn;
      char c = *constraint;
      int len;
      switch (c){
        case ',':
          constraint++;
          continue;

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
          /* If caller provided constraints pointer, look up
             the matching constraint.  Otherwise, our caller should have
             given us the proper matching constraint, but we can't
             actually fail the check if they didn't.  Indicate that
             results are inconclusive.  */
          if (constraints){
              char *end;
              unsigned long match;
              match = strtoul (constraint, &end, 10);
              if (!result)
                  result = mtcs_recog_asm_operand_ok/*!asm_operand_ok*/(self,op, constraints[match], NULL);
              constraint = (const char *) end;
          }else{
              do
                  constraint++;
              while (ISDIGIT (*constraint));
              if (! result)
                  result = -1;
          }
          continue;

          /* The rest of the compiler assumes that reloading the address
             of a MEM into a register will make it fit an 'o' constraint.
             That is, if it sees a MEM operand for an 'o' constraint,
             it assumes that (mem (base-reg)) will fit.

             That assumption fails on targets that don't have offsettable
             addresses at all.  We therefore need to treat 'o' asm
             constraints as a special case and only accept operands that
             are already offsettable, thus proving that at least one
             offsettable address exists.  */
        case 'o': /* offsettable */
          if (mtcs_recog_offsettable_nonstrict_memref_p/*!offsettable_nonstrict_memref_p*/(self,op))
            result = 1;
          break;

        case 'g':
          if (mtcs_preds_general_operand/*!general_operand*/(mtcsPreds,op, VOIDmode))
            result = 1;
          break;

        case '<':
        case '>':
          /* ??? Before auto-inc-dec, auto inc/dec insns are not supposed
             to exist, excepting those that expand_call created.  Further,
             on some machines which do not have generalized auto inc/dec,
             an inc/dec is not a memory_operand.

             Match any memory and hope things are resolved after reload.  */
          incdec_ok = true;
          /* FALLTHRU */
        default:
          cn =mtcs_preds_lookup_constraint/*!lookup_constraint*/(mtcsPreds,constraint);
          rtx mem = NULL;
          switch (get_constraint_type (cn)){
            case CT_REGISTER:
              if (!result
              && mtcs_preds_reg_class_for_constraint/*!reg_class_for_constraint*/(mtcsPreds,cn) != NO_REGS
              && GET_MODE (op) != mtcsMode->modes.M_BLKmode
              && mtcs_preds_register_operand/*!register_operand*/(mtcsPreds,op, VOIDmode))
                  result = 1;
              break;

            case CT_CONST_INT:
              if (!result  && CONST_INT_P (op)
              && mtcs_preds_insn_const_int_ok_for_constraint/*!insn_const_int_ok_for_constraint*/(mtcsPreds,INTVAL (op), cn))
            result = 1;
              break;

            case CT_MEMORY:
            case CT_RELAXED_MEMORY:
              mem = op;
              /* Fall through.  */
            case CT_SPECIAL_MEMORY:
              /* Every memory operand can be reloaded to fit.  */
              if (!mem)
            mem = extract_mem_from_operand (op);
              result = result || mtcs_preds_memory_operand/*!memory_operand*/(mtcsPreds,mem, VOIDmode);
              break;

            case CT_ADDRESS:
              /* Every address operand can be reloaded to fit.  */
              result = result || mtcs_preds_address_operand/*!address_operand*/(mtcsPreds,op, VOIDmode);
              break;

            case CT_FIXED_FORM:
              result = result || mtcs_preds_constraint_satisfied_p/*!constraint_satisfied_p*/(mtcsPreds,op, cn);
              break;
            }
            break;
      }
      len = CONSTRAINT_LEN (c, constraint);
      do
          constraint++;
      while (--len && *constraint && *constraint != ',');
      if (len)
          return 0;
  }
  /* For operands without < or > constraints reject side-effects.  */
  if (AUTO_INC_DEC && !incdec_ok && result && MEM_P (op))
      switch (GET_CODE (XEXP (op, 0))){
          case PRE_INC:
          case POST_INC:
          case PRE_DEC:
          case POST_DEC:
          case PRE_MODIFY:
          case POST_MODIFY:
              return 0;
          default:
              break;
     }

  return result;
}

/* Analyze INSN and fill in recog_data.  */
//原型 extract_insn recog.h recog.cc
 //MAX_RECOG_OPERANDS是平台相关的，其它引用函数 asm_noperands、decode_asm_operands 不需要改变
void mtcs_recog_extract_insn (MtcsRecog *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);

   int i;
   int icode;
   int noperands;
   rtx body = PATTERN (insn);

   self->recog_data.n_operands = 0;
   self->recog_data.n_alternatives = 0;
   self->recog_data.n_dups = 0;
   self->recog_data.is_asm = false;

   switch (GET_CODE (body)){
      case USE:
      case CLOBBER:
      case ASM_INPUT:
      case ADDR_VEC:
      case ADDR_DIFF_VEC:
      case VAR_LOCATION:
      case DEBUG_MARKER:
         return;

      case SET:
         if (GET_CODE (SET_SRC (body)) == ASM_OPERANDS){
            n_debug("mtcsrecog.c mtcs_recog_extract_insn GET_CODE (SET_SRC (body)) == ASM_OPERANDS isns:%p body:%p\n",insn,body);
            goto asm_insn;
         }else{
            n_debug("mtcsrecog.c mtcs_recog_extract_insn goto normal_insn isns:%p body:%p INSN_CODE (insn):%d\n",
                  insn,body,INSN_CODE (insn));
            goto normal_insn;
         }
      case PARALLEL:
         if ((GET_CODE (XVECEXP (body, 0, 0)) == SET
         && GET_CODE (SET_SRC (XVECEXP (body, 0, 0))) == ASM_OPERANDS)
         || GET_CODE (XVECEXP (body, 0, 0)) == ASM_OPERANDS
         || GET_CODE (XVECEXP (body, 0, 0)) == ASM_INPUT)
            goto asm_insn;
         else
            goto normal_insn;
      case ASM_OPERANDS:
asm_insn:
         self->recog_data.n_operands = noperands = asm_noperands (body);
         if (noperands >= 0){
            /* This insn is an `asm' with operands.  */

            /* expand_asm_operands makes sure there aren't too many operands.  */
            gcc_assert (noperands <= mtcs_recog_get_max_recog_operands/*!MAX_RECOG_OPERANDS*/(self));

            /* Now get the operand values and constraints out of the insn.  */
            decode_asm_operands (body, self->recog_data.operand,
                  self->recog_data.operand_loc,
                  self->recog_data.constraints,
                  self->recog_data.operand_mode, NULL);
            memset (self->recog_data.is_operator, 0, sizeof self->recog_data.is_operator);
            if (noperands > 0){
               const char *p =  self->recog_data.constraints[0];
               self->recog_data.n_alternatives = 1;
               while (*p)
                  self->recog_data.n_alternatives += (*p++ == ',');
            }
            self->recog_data.is_asm = true;
            break;
         }
         n_debug("mtcsrecog.c mtcs_recog_extract_insn 00 \n");
         fatal_insn_not_found (insn);
         n_debug("mtcsrecog.c mtcs_recog_extract_insn 11 \n");

      default:
normal_insn:
         /* Ordinary insn: recognize it, get the operands via insn_extract
         and get the constraints.  */
         rtx temp=PATTERN (insn);
         rtx x2 = XEXP (temp, 1);

         n_debug("mtcsrecog.c mtcs_recog_extract_insn xx -- %p  %d %s %s\n",
            temp,GET_CODE(temp),GET_RTX_NAME(GET_CODE(temp)),GET_RTX_FORMAT (GET_CODE (temp)));
         n_debug("mtcsrecog.c mtcs_recog_extract_insn yy -- %p  %d %s %s\n",
            insn,GET_CODE(insn),GET_RTX_NAME(GET_CODE(insn)),GET_RTX_FORMAT (GET_CODE (insn)));
      if(GET_CODE(temp)==SET && x2)
         n_debug("mtcsrecog.c mtcs_recog_extract_insn zz -- %p %d %s\n",x2,GET_CODE(x2),GET_RTX_NAME(GET_CODE(x2)));
         icode = mtcs_recog_recog_memoized/*!recog_memoized*/(self,insn);
      n_debug("mtcsrecog.c mtcs_recog_extract_insn 22 icode:%d code:%d\n",icode,INSN_CODE (insn));

         if (icode < 0)
            fatal_insn_not_found (insn);

         self->recog_data.n_operands = noperands = mtcsOutput->insn_data[icode].n_operands;
         self->recog_data.n_alternatives = mtcsOutput->insn_data[icode].n_alternatives;
         self->recog_data.n_dups = mtcsOutput->insn_data[icode].n_dups;

         self->insn_extract/*!insn_extract 子类声明 mtcsgenextract.c生成源文件*/(self,insn);

         for (i = 0; i < noperands; i++){
            self->recog_data.constraints[i] = mtcsOutput->insn_data[icode].operand[i].constraint;
            self->recog_data.is_operator[i] = mtcsOutput->insn_data[icode].operand[i].is_operator;
            self->recog_data.operand_mode[i] = mtcsOutput->insn_data[icode].operand[i].mode;
            /* VOIDmode match_operands gets mode from their real operand.  */
            if (self->recog_data.operand_mode[i] == VOIDmode)
               self->recog_data.operand_mode[i] = GET_MODE (self->recog_data.operand[i]);
         }
   }

   for (i = 0; i < noperands; i++)
      self->recog_data.operand_type[i] = (self->recog_data.constraints[i][0] == '=' ? MTCS_OP_OUT
            : self->recog_data.constraints[i][0] == '+' ? MTCS_OP_INOUT  : MTCS_OP_IN);

   gcc_assert (self->recog_data.n_alternatives <= MAX_RECOG_ALTERNATIVES);

   self->recog_data.insn = NULL;
   which_alternative = -1;
}

/* Return true iff OPERAND (assumed to be a REG rtx)
   is a hard reg in class CLASS when its regno is offset by OFFSET
   and changed to mode MODE.
   If REG occupies multiple hard regs, all of them must be in CLASS.  */
//原型 reg_fits_class_p recog.h recog.cc
bool mtcs_recog_reg_fits_class_p (MtcsRecog *self,const_rtx operand, reg_class_t cl, int offset,
        machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   unsigned int regno = REGNO (operand);

   if (cl == NO_REGS)
      return false;

   /* Regno must not be a pseudo register.  Offset may be negative.  */
   return (mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,regno)
   && mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,regno + offset)
   && mtcs_reg_in_hard_reg_set_p/*!in_hard_reg_set_p*/(mtcsReg,
         &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[(int) cl], mode,regno + offset));
}

/* Check the operands of an insn against the insn's operand constraints
   and return 1 if they match any of the alternatives in ALTERNATIVES.

   The information about the insn's operands, constraints, operand modes
   etc. is obtained from the global variables set up by extract_insn.

   WHICH_ALTERNATIVE is set to a number which indicates which
   alternative of constraints was matched: 0 for the first alternative,
   1 for the next, etc.

   In addition, when two operands are required to match
   and it happens that the output operand is (reg) while the
   input operand is --(reg) or ++(reg) (a pre-inc or pre-dec),
   make the output operand look like the input.
   This is because the output operand is the one the template will print.

   This is used in final, just before printing the assembler code and by
   the routines that determine an insn's attribute.

   If STRICT is a positive nonzero value, it means that we have been
   called after reload has been completed.  In that case, we must
   do all checks strictly.  If it is zero, it means that we have been called
   before reload has completed.  In that case, we first try to see if we can
   find an alternative that matches strictly.  If not, we try again, this
   time assuming that reload will fix up the insn.  This provides a "best
   guess" for the alternative and is used to compute attributes of insns prior
   to reload.  A negative value of STRICT is used for this internal call.  */

struct funny_match
{
  int this_op, other;
};
//原型 constrain_operands recog.h recog.cc
bool mtcs_recog_constrain_operands (MtcsRecog *self,int strict, mtcs_alternative_mask/*!alternative_mask*/ alternatives)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsReload *mtcsReload=mtcs_target_get_reload(mtcsTarget);
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);

   int firstPseudoRegister= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);
   int maxRecogOperands=self->maxRecogOperands;
   const char *constraints[maxRecogOperands/*!MAX_RECOG_OPERANDS*/];
   int matching_operands[maxRecogOperands/*!MAX_RECOG_OPERANDS*/];
   int earlyclobber[maxRecogOperands/*!MAX_RECOG_OPERANDS*/];
   int c;

   struct funny_match funny_match[maxRecogOperands/*!MAX_RECOG_OPERANDS*/];
   int funny_match_index;

   which_alternative = 0;
   if (self->recog_data.n_operands == 0 || self->recog_data.n_alternatives == 0)
      return true;

   for (c = 0; c < self->recog_data.n_operands; c++)
      constraints[c] = self->recog_data.constraints[c];

   do{
      int seen_earlyclobber_at = -1;
      int opno;
      bool lose = false;
      funny_match_index = 0;

      if (!TEST_BIT (alternatives, which_alternative)){
         int i;
         for (i = 0; i < self->recog_data.n_operands; i++)
            constraints[i] = skip_alternative (constraints[i]);
         which_alternative++;
         continue;
      }

      for (opno = 0; opno < self->recog_data.n_operands; opno++)
         matching_operands[opno] = -1;

      for (opno = 0; opno < self->recog_data.n_operands; opno++){
         rtx op = self->recog_data.operand[opno];
         machine_mode mode = GET_MODE (op);
         const char *p = constraints[opno];
         int offset = 0;
         bool win = false;
         int val;
         int len;

         earlyclobber[opno] = 0;

         if (GET_CODE (op) == SUBREG){
            if (REG_P (SUBREG_REG (op)) && REGNO (SUBREG_REG (op)) < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
               offset = mtcs_rtlanal_subreg_regno_offset/*!subreg_regno_offset*/(mtcsRtlanal,
                     REGNO (SUBREG_REG (op)),GET_MODE (SUBREG_REG (op)),SUBREG_BYTE (op),GET_MODE (op));
            op = SUBREG_REG (op);
         }

         /* An empty constraint or empty alternative
         allows anything which matched the pattern.  */
         if (*p == 0 || *p == ',')
            win = true;

         do
            switch (c = *p, len = CONSTRAINT_LEN (c, p), c){
               case '\0':
                  len = 0;
                  break;
               case ',':
                  c = '\0';
                  break;

               case '#':
                  /* Ignore rest of this alternative as far as
                  constraint checking is concerned.  */
                  do
                     p++;
                  while (*p && *p != ',');
                  len = 0;
                  break;

               case '&':
                  earlyclobber[opno] = 1;
                  if (seen_earlyclobber_at < 0)
                     seen_earlyclobber_at = opno;
                  break;

               case '0':  case '1':  case '2':  case '3':  case '4':
               case '5':  case '6':  case '7':  case '8':  case '9':
               {
                  /* This operand must be the same as a previous one.
                  This kind of constraint is used for instructions such
                  as add when they take only two operands.

                  Note that the lower-numbered operand is passed first.

                  If we are not testing strictly, assume that this
                  constraint will be satisfied.  */

                  char *end;
                  int match;

                  match = strtoul (p, &end, 10);
                  p = end;

                  if (strict < 0)
                     val = 1;
                  else{
                     rtx op1 = self->recog_data.operand[match];
                     rtx op2 = self->recog_data.operand[opno];
                     val = mtcs_reload_operands_match_p/*!operands_match_p*/(mtcsReload,op1, op2);
                  }

                  matching_operands[opno] = match;
                  matching_operands[match] = opno;

                  if (val != 0)
                     win = true;

                  /* If output is *x and input is *--x, arrange later
                  to change the output to *--x as well, since the
                  output op is the one that will be printed.  */
                  if (val == 2 && strict > 0){
                     funny_match[funny_match_index].this_op = opno;
                     funny_match[funny_match_index++].other = match;
                  }
               }
                  len = 0;
                  break;

               case 'p':
               /* p is used for address_operands.  When we are called by
               gen_reload, no one will have checked that the address is
               strictly valid, i.e., that all pseudos requiring hard regs
               have gotten them.  We also want to make sure we have a
               valid mode.  */
               {
                  auto mem_mode = (self->recog_data.is_asm ? VOIDmode : self->recog_data.operand_mode[opno]);
                  if ((GET_MODE (op) == VOIDmode|| mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,GET_MODE (op)))
                  && (strict <= 0 || mtcs_recog_strict_memory_address_p/*!strict_memory_address_p*/(self,mem_mode, op)))
                     win = true;
                  break;
               }

               /* No need to check general_operand again;
               it was done in insn-recog.cc.  Well, except that reload
               doesn't check the validity of its replacements, but
               that should only matter when there's a bug.  */
               case 'g':
                  /* Anything goes unless it is a REG and really has a hard reg
                  but the hard reg is not in the class GENERAL_REGS.  */
                  if (REG_P (op)){
                     if (strict < 0
                     || mtcs_reg_get_general_regs/*!GENERAL_REGS*/(mtcsReg) == mtcs_reg_get_all_regs/*!ALL_REGS*/(mtcsReg)
                     || (reload_in_progress
                     && REGNO (op) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
                     || mtcs_recog_reg_fits_class_p/*!reg_fits_class_p*/(self,op,
                           mtcs_reg_get_general_regs/*!GENERAL_REGS*/(mtcsReg), offset, mode))
                        win = true;
                  }else if (strict < 0 || mtcs_preds_general_operand/*!general_operand*/(mtcsPreds,op, mode))
                     win = true;
                  break;

               default:
               {
                  enum constraint_num cn = mtcs_preds_lookup_constraint/*!lookup_constraint*/(mtcsPreds,p);
                  enum reg_class cl = mtcs_preds_reg_class_for_constraint/*!reg_class_for_constraint*/(mtcsPreds,cn);
                  if (cl != NO_REGS){
                     HardRegSet *filter =mtcs_preds_get_register_filter/*!get_register_filter*/(mtcsPreds,cn);
                     if (strict < 0 || (strict == 0 && REG_P (op) && REGNO (op) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
                           || (strict == 0 && GET_CODE (op) == SCRATCH)|| (REG_P (op)
                           && mtcs_recog_reg_fits_class_p/*!reg_fits_class_p*/(self,op, cl, offset, mode)
                           && (!filter || mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(
                                 filter/*!*filter*/,REGNO (op) + offset))))
                        win = true;
                  }else if (mtcs_preds_constraint_satisfied_p/*!constraint_satisfied_p*/(mtcsPreds,op, (int)cn))
                     win = true;
                  else if ((mtcs_preds_insn_extra_memory_constraint/*!insn_extra_memory_constraint*/(mtcsPreds,cn)
                  || mtcs_preds_insn_extra_relaxed_memory_constraint/*!insn_extra_relaxed_memory_constraint*/(mtcsPreds,cn))
                  /* Every memory operand can be reloaded to fit.  */
                  && ((strict < 0 && MEM_P (op))
                  /* Before reload, accept what reload can turn
                  into a mem.  */
                  || (strict < 0 && CONSTANT_P (op))
                  /* Before reload, accept a pseudo or hard register,
                  since LRA can turn it into a mem.  */
                  || (strict < 0 && mtcsTarget/*!targetm.lra_p*/->lra_p(mtcsTarget) && REG_P (op))
                  /* During reload, accept a pseudo  */
                  || (reload_in_progress && REG_P (op)
                  && REGNO (op) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)))
                     win = true;
                  else if (mtcs_preds_insn_extra_address_constraint/*!insn_extra_address_constraint*/(mtcsPreds,cn)
                        /* Every address operand can be reloaded to fit.  */ && strict < 0)
                     win = true;
                  /* Cater to architectures like IA-64 that define extra memory
                  constraints without using define_memory_constraint.  */
                  else if (reload_in_progress
                  && REG_P (op)
                  && REGNO (op) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/
                  && reg_renumber[REGNO (op)] < 0
                  && reg_equiv_mem (REGNO (op)) != 0
                  && mtcs_preds_constraint_satisfied_p/*!constraint_satisfied_p*/(mtcsPreds,reg_equiv_mem (REGNO (op)), cn))
                     win = true;
                  break;
               }
            }
         while (p += len, c);

         constraints[opno] = p;
         /* If this operand did not win somehow,
         this alternative loses.  */
         if (! win)
            lose = true;
      }
      /* This alternative won; the operands are ok.
      Change whichever operands this alternative says to change.  */
      if (! lose){
         int opno, eopno;

         /* See if any earlyclobber operand conflicts with some other
         operand.  */

         if (strict > 0  && seen_earlyclobber_at >= 0)
            for (eopno = seen_earlyclobber_at; eopno < self->recog_data.n_operands; eopno++)
               /* Ignore earlyclobber operands now in memory,
               because we would often report failure when we have
               two memory operands, one of which was formerly a REG.  */
               if (earlyclobber[eopno]  && REG_P (self->recog_data.operand[eopno]))
                  for (opno = 0; opno < recog_data.n_operands; opno++)
                     if ((MEM_P (self->recog_data.operand[opno])
                     || self->recog_data.operand_type[opno] != OP_OUT)
                     && opno != eopno
                     /* Ignore things like match_operator operands.  */
                     && *self->recog_data.constraints[opno] != 0
                     && ! (matching_operands[opno] == eopno
                     && mtcs_reload_operands_match_p/*!operands_match_p*/(mtcsReload,self->recog_data.operand[opno],
                     self->recog_data.operand[eopno]))
                     && ! mtcs_reload_safe_from_earlyclobber/*!safe_from_earlyclobber*/(mtcsReload,
                           self->recog_data.operand[opno],self->recog_data.operand[eopno]))
                        lose = true;

         if (! lose){
            while (--funny_match_index >= 0){
               self->recog_data.operand[funny_match[funny_match_index].other]
                                        = self->recog_data.operand[funny_match[funny_match_index].this_op];
            }

            /* For operands without < or > constraints reject side-effects.  */
            if (AUTO_INC_DEC && self->recog_data.is_asm) {
               for (opno = 0; opno < self->recog_data.n_operands; opno++)
                  if (MEM_P (self->recog_data.operand[opno]))
                     switch (GET_CODE (XEXP (self->recog_data.operand[opno], 0))){
                        case PRE_INC:
                        case POST_INC:
                        case PRE_DEC:
                        case POST_DEC:
                        case PRE_MODIFY:
                        case POST_MODIFY:
                           if (strchr (self->recog_data.constraints[opno], '<') == NULL
                           && strchr (self->recog_data.constraints[opno], '>') == NULL)
                              return false;
                           break;
                        default:
                           break;
                     }
            }

            return true;
         }
      }

      which_alternative++;
   }while (which_alternative < self->recog_data.n_alternatives);

   which_alternative = -1;
   /* If we are about to reject this, but we are not to test strictly,
   try a very loose test.  Only return failure if it fails also.  */
   if (strict == 0)
      return mtcs_recog_constrain_operands/*!constrain_operands*/(self,-1, alternatives);
   else
      return false;
}

/* Do cached constrain_operands on INSN and complain about failures.  */
//原型 constrain_operands_cached recog.h recog.cc
bool mtcs_recog_constrain_operands_cached (MtcsRecog *self,rtx_insn *insn, int strict)
{
  if (which_alternative == -1)
    return mtcs_recog_constrain_operands/*!constrain_operands*/(self,
          strict, mtcs_recog_get_enabled_alternatives/*!get_enabled_alternatives*/(self,insn));
  else
    return true;
}

/* Check that X is an insn-body for an `asm' with operands
   and that the operands mentioned in it are legitimate.  */
//原型 check_asm_operands recog.h recog.cc
bool mtcs_recog_check_asm_operands (MtcsRecog *self,rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   int noperands;
   rtx *operands;
   const char **constraints;
   int i;

   if (!asm_labels_ok (x))
      return false;

   /* Post-reload, be more strict with things.  */
   if (reload_completed){
      /* ??? Doh!  We've not got the wrapping insn.  Cook one up.  */
      rtx_insn *insn = mtcs_emit_make_insn_raw/*!make_insn_raw*/(mtcsEmit,x);
      mtcs_recog_extract_insn/*!extract_insn*/(self,insn);
      mtcs_recog_constrain_operands/*!constrain_operands*/(self,1,
            mtcs_recog_get_enabled_alternatives/*!get_enabled_alternatives*/(self,insn));
      return which_alternative >= 0;
   }

   noperands = asm_noperands (x);
   if (noperands < 0)
      return false;
   if (noperands == 0)
      return true;

   operands = XALLOCAVEC (rtx, noperands);
   constraints = XALLOCAVEC (const char *, noperands);

   decode_asm_operands (x, operands, NULL, constraints, NULL, NULL);

   for (i = 0; i < noperands; i++){
      const char *c = constraints[i];
      if (c[0] == '%')
         c++;
      if (! mtcs_recog_asm_operand_ok/*!asm_operand_ok*/(self,operands[i], c, constraints))
         return false;
   }
   return true;
}

/* Return the value of ATTR for instruction INSN.  */
//原型 get_bool_attr recog.cc

static bool get_bool_attr (MtcsRecog *self,rtx_insn *insn, bool_attr attr)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsInsnAttr *mtcsInsnAttr=mtcs_target_get_insn_attr(mtcsTarget);
  switch (attr)
    {
    case BA_ENABLED:
      return mtcs_insn_attr_get_enabled/*!get_attr_enabled*/(mtcsInsnAttr,insn);
    case BA_PREFERRED_FOR_SIZE:
      return mtcs_insn_attr_get_enabled/*!get_attr_enabled*/(mtcsInsnAttr,insn)
            && mtcs_insn_attr_get_preferred_for_size/*!get_attr_preferred_for_size*/(mtcsInsnAttr,insn);
    case BA_PREFERRED_FOR_SPEED:
      return mtcs_insn_attr_get_enabled/*!get_attr_enabled*/(mtcsInsnAttr,insn)
            && mtcs_insn_attr_get_preferred_for_speed/*!get_attr_preferred_for_speed*/(mtcsInsnAttr,insn);
    }
  gcc_unreachable ();
}

/* Like get_bool_attr_mask, but don't use the cache.  */
//原型 get_bool_attr_mask_uncached recog.cc
static alternative_mask get_bool_attr_mask_uncached (MtcsRecog *self,rtx_insn *insn, bool_attr attr)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);

   /* Temporarily install enough information for get_attr_<foo> to assume
   that the insn operands are already cached.  As above, the attribute
   mustn't depend on the values of operands, so we don't provide their
   real values here.  */
   rtx_insn *old_insn = self->recog_data.insn;
   int old_alternative = which_alternative;

   self->recog_data.insn = insn;
   alternative_mask mask = ALL_ALTERNATIVES;
   int n_alternatives = mtcsOutput->insn_data[INSN_CODE (insn)].n_alternatives;
   for (int i = 0; i < n_alternatives; i++){
      which_alternative = i;
      if (!get_bool_attr(self,insn, attr))
         mask &= ~ALTERNATIVE_BIT (i);
   }

   self->recog_data.insn = old_insn;
   which_alternative = old_alternative;
   return mask;
}




/* Return true if boolean attribute ATTR is supported.  */
//原型 have_bool_attr recog.cc
static bool have_bool_attr (MtcsRecog *self,bool_attr attr)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsInsnAttr *mtcsInsnAttr=mtcs_target_get_insn_attr(mtcsTarget);
   /*!
   switch (attr){
      case BA_ENABLED:
         return HAVE_ATTR_enabled;
      case BA_PREFERRED_FOR_SIZE:
         return HAVE_ATTR_enabled || HAVE_ATTR_preferred_for_size;
      case BA_PREFERRED_FOR_SPEED:
         return HAVE_ATTR_enabled || HAVE_ATTR_preferred_for_speed;
   }
   gcc_unreachable ();
   */
   return mtcs_insn_attr_have_bool_attr(mtcsInsnAttr,(int)attr);
}

/* Return the mask of operand alternatives that are allowed for INSN
   by boolean attribute ATTR.  This mask depends only on INSN and on
   the current target; it does not depend on things like the values of
   operands.  */
//原型 get_bool_attr_mask recog.cc
static alternative_mask get_bool_attr_mask (MtcsRecog *self,rtx_insn *insn, bool_attr attr)
{
   /* Quick exit for asms and for targets that don't use these attributes.  */
   int code = INSN_CODE (insn);
   if (code < 0 || !have_bool_attr(self,attr))
      return ALL_ALTERNATIVES;

   /* Calling get_attr_<foo> can be expensive, so cache the mask
   for speed.  */
   if (!self->/*!this_target_recog->*/target_recog.x_bool_attr_masks[code][attr])
      self->/*!this_target_recog->*/target_recog.x_bool_attr_masks[code][attr] = get_bool_attr_mask_uncached(self,insn, attr);
   return self->/*!this_target_recog->*/target_recog.x_bool_attr_masks[code][attr];
}


/* Return the set of alternatives of INSN that are allowed by the current
   target.  */
//原型 get_enabled_alternatives recog.h recog.cc
mtcs_alternative_mask mtcs_recog_get_enabled_alternatives (MtcsRecog *self,rtx_insn *insn)
{
  return get_bool_attr_mask(self,insn, BA_ENABLED);
}

//原型 MAX_RECOG_OPERANDS insn-config.h 平台相关
void mtcs_recog_set_max_recog_operands(MtcsRecog *self,int maxValue)
{
   self->maxRecogOperands=maxValue;
}

int  mtcs_recog_get_max_recog_operands(MtcsRecog *self)
{
   return self->maxRecogOperands;
}

/* Return the number of changes so far in the current group.  */
//原型 num_validated_changes recog.h recog.cc
int mtcs_recog_num_validated_changes (MtcsRecog *self)
{
  return self->num_changes;
}

/* Return number of changes made and not validated yet.  */
//原型 num_changes_pending recog.h recog.cc
int mtcs_recog_num_changes_pending (MtcsRecog *self)
{
   return self->num_changes;
}

/* Validate a proposed change to OBJECT.  LOC is the location in the rtl
   at which NEW_RTX will be placed.  If NEW_LEN is >= 0, XVECLEN (NEW_RTX, 0)
   will also be changed to NEW_LEN, which is no greater than the current
   XVECLEN.  If OBJECT is zero, no validation is done, the change is
   simply made.

   Two types of objects are supported:  If OBJECT is a MEM, memory_address_p
   will be called with the address and mode as parameters.  If OBJECT is
   an INSN, CALL_INSN, or JUMP_INSN, the insn will be re-recognized with
   the change in place.

   IN_GROUP is nonzero if this is part of a group of changes that must be
   performed as a group.  In that case, the changes will be stored.  The
   function `apply_change_group' will validate and apply the changes.

   If IN_GROUP is zero, this is a single change.  Try to recognize the insn
   or validate the memory reference with the change applied.  If the result
   is not valid for the machine, suppress the change and return false.
   Otherwise, perform the change and return true.  */

static bool validate_change_1 (MtcsRecog *self,rtx object, rtx *loc, rtx new_rtx, bool in_group,
         bool unshare, int new_len = -1)
{
   gcc_assert (self->temporarily_undone_changes == 0);
   rtx old = *loc;
   n_debug("mtcsrecog.c validate_change_1 00 in_group:%d unshare:%d new_len:%d\n",
         in_group,unshare,new_len);
   /* Single-element parallels aren't valid and won't match anything.
   Replace them with the single element.  */
   if (new_len == 1 && GET_CODE (new_rtx) == PARALLEL){
      new_rtx = XVECEXP (new_rtx, 0, 0);
      new_len = -1;
   }

   if ((old == new_rtx || rtx_equal_p (old, new_rtx)) && (new_len < 0 || XVECLEN (new_rtx, 0) == new_len))
      return true;
   n_debug("mtcsrecog.c validate_change_1 11 in_group:%d unshare:%d new_len:%d num_changes:%d changes_allocated:%d\n",
         in_group,unshare,new_len,self->num_changes,self->changes_allocated);
   gcc_assert ((in_group != 0 || self->num_changes == 0)  && (new_len < 0 || new_rtx == *loc));
   testprint();
   n_debug("mtcsrecog.c ---------------------xx-----------------object:%p *loc %p new_rtx:%p\n",object,*loc,new_rtx);
   mtcs_print_rtl_single(stderr,object);

   mtcs_print_rtl_single(stderr,*loc);

   mtcs_print_rtl_single(stderr,new_rtx);

   *loc = new_rtx;
   n_debug("mtcsrecog.c validate_change_1 11tt in_group:%d unshare:%d new_len:%d num_changes:%d changes_allocated:%d\n",
         in_group,unshare,new_len,self->num_changes,self->changes_allocated);
   testprint();
   /* Save the information describing this change.  */
   if (self->num_changes >= self->changes_allocated){
      if (self->changes_allocated == 0)
         /* This value allows for repeated substitutions inside complex
         indexed addresses, or changes in up to 5 insns.  */
         self->changes_allocated = mtcs_recog_get_max_recog_operands/*!MAX_RECOG_OPERANDS*/(self) * 5;
      else
         self->changes_allocated *= 2;
      self->changes = (void*)XRESIZEVEC (change_t, (change_t *)self->changes, self->changes_allocated);
   }
   change_t *changes=(change_t *)self->changes;
   changes[self->num_changes].object = object;
   changes[self->num_changes].loc = loc;
   changes[self->num_changes].old = old;
   changes[self->num_changes].old_len = (new_len >= 0 ? XVECLEN (new_rtx, 0) : -1);
   changes[self->num_changes].unshare = unshare;
   n_debug("mtcsrecog.c validate_change_1 11xx in_group:%d unshare:%d new_len:%d num_changes:%d changes_allocated:%d\n",
           in_group,unshare,new_len,self->num_changes,self->changes_allocated);
     testprint();
   if (new_len >= 0)
      XVECLEN (new_rtx, 0) = new_len;
   n_debug("mtcsrecog.c validate_change_1 11xxyy in_group:%d unshare:%d new_len:%d num_changes:%d changes_allocated:%d\n",
           in_group,unshare,new_len,self->num_changes,self->changes_allocated);
     testprint();
   if (object && !MEM_P (object)){
      /* Set INSN_CODE to force rerecognition of insn.  Save old code in
      case invalid.  */
      changes[self->num_changes].old_code = INSN_CODE (object);
      n_debug("mtcsrecog.c validate_change_1 11xxwww in_group:%d unshare:%d new_len:%d num_changes:%d changes_allocated:%d\n",
              in_group,unshare,new_len,self->num_changes,self->changes_allocated);
        testprint();
      INSN_CODE (object) = -1;
   }
   self->num_changes++;
   n_debug("mtcsrecog.cc validate_change_1 22 in_group:%d unshare:%d new_len:%d num_changes:%d changes_allocated:%d\n",
         in_group,unshare,new_len,self->num_changes,self->changes_allocated);
   testprint();
   /* If we are making a group of changes, return 1.  Otherwise, validate the
   change group we made.  */
   if (in_group)
      return true;
   else
      return mtcs_recog_apply_change_group/*!apply_change_group*/(self);
}

/* Retract the changes numbered NUM and up.  */
//原型 cancel_changes recog.h recog.cc
void mtcs_recog_cancel_changes (MtcsRecog *self,int num)
{
   change_t *changes=(change_t *)self->changes;
   gcc_assert (self->temporarily_undone_changes == 0);
   int i;
   /* Back out all the changes.  Do this in the opposite order in which
   they were made.  */
   for (i = self->num_changes - 1; i >= num; i--){
      if (changes[i].old_len >= 0)
         XVECLEN (*changes[i].loc, 0) = changes[i].old_len;
      else
         *changes[i].loc = changes[i].old;
      if (changes[i].object && !MEM_P (changes[i].object))
         INSN_CODE (changes[i].object) = changes[i].old_code;
   }
   n_debug("mtcsrecog.c cancel_changes num_changes:%d num:%d\n",self->num_changes,num);

   self->num_changes = num;
}

/* Apply a group of changes previously issued with `validate_change'.
   If all changes are valid, call confirm_change_group and return true,
   otherwise, call cancel_changes and return false.  */
//原型 apply_change_group recog.h recog.cc
bool mtcs_recog_apply_change_group (MtcsRecog *self)
{
   n_debug("mtcsrecog.c apply_change_group 00\n");
   if (mtcs_recog_verify_changes/*!verify_changes*/(self,0)){
      n_debug("mtcsrecog.c apply_change_group 11\n");

      mtcs_recog_confirm_change_group/*!confirm_change_group*/(self);
      return true;
   }else{
      n_debug("mtcsrecog.c apply_change_group 22\n");

      mtcs_recog_cancel_changes/*!cancel_changes*/(self,0);
      return false;
   }
}

/* Tentatively apply the changes numbered NUM and up.
   Return true if all changes are valid, false otherwise.  */
//原型 verify_changes recog.h recog.cc
bool mtcs_recog_verify_changes (MtcsRecog *self,int num)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   int i;
   rtx last_validated = NULL_RTX;
   change_t *changes=(change_t *)self->changes;

   /* The changes have been applied and all INSN_CODEs have been reset to force
   rerecognition.

   The changes are valid if we aren't given an object, or if we are
   given a MEM and it still is a valid address, or if this is in insn
   and it is recognized.  In the latter case, if reload has completed,
   we also require that the operands meet the constraints for
   the insn.  */

   for (i = num; i < self->num_changes; i++){
      rtx object = changes[i].object;

      /* If there is no object to test or if it is the same as the one we
      already tested, ignore it.  */
      if (object == 0 || object == last_validated)
         continue;

      if (MEM_P (object)){
         if (! mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/(self,GET_MODE (object),
               XEXP (object, 0), mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,object)))
            break;
      }else if (/* changes[i].old might be zero, e.g. when putting a
      REG_FRAME_RELATED_EXPR into a previously empty list.  */
      changes[i].old
      && REG_P (changes[i].old)
      && asm_noperands (PATTERN (object)) > 0
      && register_asm_p (changes[i].old)){
      /* Don't allow changes of hard register operands to inline
      assemblies if they have been defined as register asm ("x").  */
         break;
      }else if (DEBUG_INSN_P (object))
         continue;
      else if (mtcs_recog_insn_invalid_p/*!insn_invalid_p*/(self,as_a <rtx_insn *> (object), true)){
         rtx pat = PATTERN (object);

         /* Perhaps we couldn't recognize the insn because there were
         extra CLOBBERs at the end.  If so, try to re-recognize
         without the last CLOBBER (later iterations will cause each of
         them to be eliminated, in turn).  But don't do this if we
         have an ASM_OPERAND.  */
         if (GET_CODE (pat) == PARALLEL
         && GET_CODE (XVECEXP (pat, 0, XVECLEN (pat, 0) - 1)) == CLOBBER
         && asm_noperands (PATTERN (object)) < 0){
            rtx newpat;

            if (XVECLEN (pat, 0) == 2)
               newpat = XVECEXP (pat, 0, 0);
            else{
               int j;

               newpat = gen_rtx_PARALLEL (VOIDmode,
               rtvec_alloc (XVECLEN (pat, 0) - 1));
               for (j = 0; j < XVECLEN (newpat, 0); j++)
                  XVECEXP (newpat, 0, j) = XVECEXP (pat, 0, j);
            }

            /* Add a new change to this group to replace the pattern
            with this new pattern.  Then consider this change
            as having succeeded.  The change we added will
            cause the entire call to fail if things remain invalid.

            Note that this can lose if a later change than the one
            we are processing specified &XVECEXP (PATTERN (object), 0, X)
            but this shouldn't occur.  */

            mtcs_recog_validate_change/*!validate_change*/(self,object, &PATTERN (object), newpat, 1);
            continue;
         }else if (GET_CODE (pat) == USE || GET_CODE (pat) == CLOBBER  || GET_CODE (pat) == VAR_LOCATION)
            /* If this insn is a CLOBBER or USE, it is always valid, but is
            never recognized.  */
            continue;
         else
            break;
      }
      last_validated = object;
   }

   return (i == self->num_changes);
}

/* Check if REG_INC argument in *data overlaps a stored REG.  */
typedef struct _CheckInvalidData{
   MtcsRecog *mtcsRecog;
   rtx *x;
}CheckInvalidData;

static void check_invalid_inc_dec (rtx reg, const_rtx, void *userData)
{
   CheckInvalidData *data=(CheckInvalidData *)userData;
   MtcsRecog *self=data->mtcsRecog;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   rtx *pinc = (rtx *) data->x;
   if (*pinc == NULL_RTX || MEM_P (reg))
      return;
   if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,reg, *pinc))
      *pinc = NULL_RTX;
}

/* This subroutine of apply_change_group verifies whether the changes to INSN
   were valid; i.e. whether INSN can still be recognized.

   If IN_GROUP is true clobbers which have to be added in order to
   match the instructions will be added to the current change group.
   Otherwise the changes will take effect immediately.  */
//原型 insn_invalid_p recog.h recog.cc
bool mtcs_recog_insn_invalid_p (MtcsRecog *self,rtx_insn *insn, bool in_group)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   rtx pat = PATTERN (insn);
   int num_clobbers = 0;
   /* If we are before reload and the pattern is a SET, see if we can add
   clobbers.  */
   int icode = mtcs_recog_recog/*!recog*/(self,pat, insn,(GET_CODE (pat) == SET
         && ! reload_completed  && ! reload_in_progress) ? &num_clobbers : 0);
   bool is_asm = icode < 0 && asm_noperands (PATTERN (insn)) >= 0;


   /* If this is an asm and the operand aren't legal, then fail.  Likewise if
   this is not an asm and the insn wasn't recognized.  */
   if ((is_asm && ! mtcs_recog_check_asm_operands/*!check_asm_operands*/(self,PATTERN (insn)))
   || (!is_asm && icode < 0))
      return true;

   /* If we have to add CLOBBERs, fail if we have to add ones that reference
   hard registers since our callers can't know if they are live or not.
   Otherwise, add them.  */
   if (num_clobbers > 0){
      rtx newpat;

      if (mtcs_recog_added_clobbers_hard_reg_p/*!added_clobbers_hard_reg_p*/(self,icode))
         return true;

      newpat = gen_rtx_PARALLEL (VOIDmode, rtvec_alloc (num_clobbers + 1));
      XVECEXP (newpat, 0, 0) = pat;
      mtcs_recog_add_clobbers/*!add_clobbers*/(self,newpat, icode);
      if (in_group)
         mtcs_recog_validate_change/*!validate_change*/(self,insn, &PATTERN (insn), newpat, 1);
      else
         PATTERN (insn) = pat = newpat;
   }

   /* After reload, verify that all constraints are satisfied.  */
   if (reload_completed){
      mtcs_recog_extract_insn/*!extract_insn*/(self,insn);

      if (! mtcs_recog_constrain_operands/*!constrain_operands*/(self,1,
            mtcs_recog_get_preferred_alternatives/*!get_preferred_alternatives*/(self,insn)))
         return true;
   }

   /* Punt if REG_INC argument overlaps some stored REG.  */
   for (rtx link = FIND_REG_INC_NOTE (insn, NULL_RTX); link; link = XEXP (link, 1))
      if (REG_NOTE_KIND (link) == REG_INC){
         rtx reg = XEXP (link, 0);
         CheckInvalidData userData={self,&reg};
         mtcs_rtlanal_note_stores/*!note_stores*/(mtcsRtlanal,insn, check_invalid_inc_dec,&userData/*!&reg*/);
         if (reg == NULL_RTX)
            return true;
      }

   INSN_CODE (insn) = icode;
   return false;
}

/* Return the set of alternatives of INSN that are allowed by the current
   target and are preferred for the current size/speed optimization
   choice.  */
//原型 get_preferred_alternatives recog.h recog.cc
mtcs_alternative_mask mtcs_recog_get_preferred_alternatives (MtcsRecog *self,rtx_insn *insn)
{
  if (optimize_bb_for_speed_p (BLOCK_FOR_INSN (insn)))
    return get_bool_attr_mask(self,insn, BA_PREFERRED_FOR_SPEED);
  else
    return get_bool_attr_mask(self,insn, BA_PREFERRED_FOR_SIZE);
}

/* Return the set of alternatives of INSN that are allowed by the current
   target and are preferred for the size/speed optimization choice
   associated with BB.  Passing a separate BB is useful if INSN has not
   been emitted yet or if we are considering moving it to a different
   block.  */
//原型 get_preferred_alternatives recog.h recog.cc
mtcs_alternative_mask mtcs_recog_get_preferred_alternatives (MtcsRecog *self,rtx_insn *insn, basic_block bb)
{
  if (optimize_bb_for_speed_p (bb))
    return get_bool_attr_mask(self,insn, BA_PREFERRED_FOR_SPEED);
  else
    return get_bool_attr_mask(self,insn, BA_PREFERRED_FOR_SIZE);
}


/* Wrapper for validate_change_1 without the UNSHARE argument defaulting
   UNSHARE to false.  */
//原型 validate_change recog.h recog.cc
bool mtcs_recog_validate_change (MtcsRecog *self,rtx object, rtx *loc, rtx new_rtx, bool in_group)
{
  return validate_change_1(self,object, loc, new_rtx, in_group, false);
}

/* A group of changes has previously been issued with validate_change
   and verified with verify_changes.  Call df_insn_rescan for each of
   the insn changed and clear num_changes.  */
//原型 confirm_change_group recog.h recog.cc
void mtcs_recog_confirm_change_group (MtcsRecog *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan   *mtcsDfscan =mtcs_target_get_dfscan(mtcsTarget);

   int i;
   rtx last_object = NULL;
   change_t *changes=(change_t *)self->changes;

   gcc_assert (self->temporarily_undone_changes == 0);
   for (i = 0; i < self->num_changes; i++){
      rtx object = changes[i].object;

      if (changes[i].unshare)
         *changes[i].loc = copy_rtx (*changes[i].loc);

      /* Avoid unnecessary rescanning when multiple changes to same instruction
      are made.  */
      if (object){
         if (object != last_object && last_object && INSN_P (last_object))
            mtcs_dfscan_df_insn_rescan/*!df_insn_rescan*/(mtcsDfscan,as_a <rtx_insn *> (last_object));
         last_object = object;
      }
   }

   if (last_object && INSN_P (last_object))
      mtcs_dfscan_df_insn_rescan/*!df_insn_rescan*/(mtcsDfscan,as_a <rtx_insn *> (last_object));
   n_debug("mtcsrecog.c mtcs_recog_confirm_change_group 设为零 num_changes:%d\n",self->num_changes);

   self->num_changes = 0;
}

/* Copies frame related info of an insn (OLD_INSN) to the single
   insn (NEW_INSN) that was obtained by splitting OLD_INSN.  */
//原型 copy_frame_info_to_split_insn recog.h recog.cc
void mtcs_recog_copy_frame_info_to_split_insn (MtcsRecog *self,rtx_insn *old_insn, rtx_insn *new_insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   bool any_note = false;
   rtx note;

   if (!RTX_FRAME_RELATED_P (old_insn))
      return;

   RTX_FRAME_RELATED_P (new_insn) = 1;

   /* Allow the backend to fill in a note during the split.  */
   for (note = REG_NOTES (new_insn); note ; note = XEXP (note, 1))
      switch (REG_NOTE_KIND (note)){
         case REG_FRAME_RELATED_EXPR:
         case REG_CFA_DEF_CFA:
         case REG_CFA_ADJUST_CFA:
         case REG_CFA_OFFSET:
         case REG_CFA_REGISTER:
         case REG_CFA_EXPRESSION:
         case REG_CFA_RESTORE:
         case REG_CFA_SET_VDRAP:
            any_note = true;
            break;
         default:
            break;
      }

   /* If the backend didn't supply a note, copy one over.  */
   if (!any_note)
      for (note = REG_NOTES (old_insn); note ; note = XEXP (note, 1))
         switch (REG_NOTE_KIND (note)){
            case REG_FRAME_RELATED_EXPR:
            case REG_CFA_DEF_CFA:
            case REG_CFA_ADJUST_CFA:
            case REG_CFA_OFFSET:
            case REG_CFA_REGISTER:
            case REG_CFA_EXPRESSION:
            case REG_CFA_RESTORE:
            case REG_CFA_SET_VDRAP:
               add_reg_note (new_insn, REG_NOTE_KIND (note), XEXP (note, 0));
               any_note = true;
               break;
            default:
               break;
         }

   /* If there still isn't a note, make sure the unwind info sees the
   same expression as before the split.  */
   if (!any_note){
      rtx old_set, new_set;

      /* The old insn had better have been simple, or annotated.  */
      old_set = single_set (old_insn);
      gcc_assert (old_set != NULL);

      new_set = single_set (new_insn);
      if (!new_set || !rtx_equal_p (new_set, old_set))
         add_reg_note (new_insn, REG_FRAME_RELATED_EXPR, old_set);
   }

   /* Copy prologue/epilogue status.  This is required in order to keep
   proper placement of EPILOGUE_BEG and the DW_CFA_remember_state.  */
   mtcs_func_maybe_copy_prologue_epilogue_insn/*!maybe_copy_prologue_epilogue_insn*/(mtcsFunc,old_insn, new_insn);
}


/* Reduce conditional compilation elsewhere.  */
/* A subroutine of validate_replace_rtx_1 that tries to simplify the resulting
   rtx.  */
//原型 simplify_while_replacing recog.cc
static void simplify_while_replacing (MtcsRecog *self,rtx *loc, rtx to, rtx_insn *object,machine_mode op0_mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   rtx x = *loc;
   enum rtx_code code = GET_CODE (x);
   rtx new_rtx = NULL_RTX;
   scalar_int_mode is_mode;

   if (SWAPPABLE_OPERANDS_P (x)
   && mtcs_rtlanal_swap_commutative_operands_p/*!swap_commutative_operands_p*/(mtcsRtlanal,
         XEXP (x, 0), XEXP (x, 1))){
      mtcs_recog_validate_unshare_change/*!validate_unshare_change*/(self,object, loc,
            gen_rtx_fmt_ee (COMMUTATIVE_ARITH_P (x) ? code : swap_condition (code),GET_MODE (x), XEXP (x, 1),XEXP (x, 0)), 1);
      x = *loc;
      code = GET_CODE (x);
   }

   /* Canonicalize arithmetics with all constant operands.  */
   switch (GET_RTX_CLASS (code)){
      case RTX_UNARY:
         if (CONSTANT_P (XEXP (x, 0)))
         new_rtx = mtcs_simplify_rtx_unary_operation/*!simplify_unary_operation*/(mtcsSimplifyRtx,
               code, GET_MODE (x), XEXP (x, 0),op0_mode);
         break;
      case RTX_COMM_ARITH:
      case RTX_BIN_ARITH:
         if (CONSTANT_P (XEXP (x, 0)) && CONSTANT_P (XEXP (x, 1)))
         new_rtx = mtcs_simplify_rtx_binary_operation/*!simplify_binary_operation*/(mtcsSimplifyRtx,
               code, GET_MODE (x), XEXP (x, 0), XEXP (x, 1));
         break;
      case RTX_COMPARE:
      case RTX_COMM_COMPARE:
         if (CONSTANT_P (XEXP (x, 0)) && CONSTANT_P (XEXP (x, 1)))
         new_rtx = mtcs_simplify_rtx_relational_operation/*!simplify_relational_operation*/(mtcsSimplifyRtx,
               code, GET_MODE (x), op0_mode,
         XEXP (x, 0), XEXP (x, 1));
         break;
      default:
         break;
   }

   if (new_rtx){
      mtcs_recog_validate_change/*!validate_change*/(self,object, loc, new_rtx, 1);
      return;
   }

   switch (code){
      case PLUS:
         /* If we have a PLUS whose second operand is now a CONST_INT, use
         simplify_gen_binary to try to simplify it.
         ??? We may want later to remove this, once simplification is
         separated from this function.  */
         if (CONST_INT_P (XEXP (x, 1)) && XEXP (x, 1) == to)
            mtcs_recog_validate_change/*!validate_change*/(self,object, loc,
                  mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,
                        PLUS, GET_MODE (x), XEXP (x, 0), XEXP (x, 1)), 1);
         break;
      case MINUS:
         if (CONST_SCALAR_INT_P (XEXP (x, 1)))
            mtcs_recog_validate_change/*!validate_change*/(self,object, loc,
                     mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,
                           PLUS, GET_MODE (x), XEXP (x, 0),
                     mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,
                           NEG,GET_MODE (x), XEXP (x, 1),GET_MODE (x))), 1);
         break;
      case ZERO_EXTEND:
      case SIGN_EXTEND:
         if (GET_MODE (XEXP (x, 0)) == VOIDmode){
            new_rtx = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,
                  code, GET_MODE (x), XEXP (x, 0),op0_mode);
            /* If any of the above failed, substitute in something that
            we know won't be recognized.  */
            if (!new_rtx)
               new_rtx = gen_rtx_CLOBBER (GET_MODE (x), const0_rtx);
            mtcs_recog_validate_change/*!validate_change*/(self,object, loc, new_rtx, 1);
         }
         break;
      case SUBREG:
         /* All subregs possible to simplify should be simplified.  */
         new_rtx = mtcs_simplify_rtx_subreg/*!simplify_subreg*/(mtcsSimplifyRtx,
               GET_MODE (x), SUBREG_REG (x), op0_mode,SUBREG_BYTE (x));

         /* Subregs of VOIDmode operands are incorrect.  */
         if (!new_rtx && GET_MODE (SUBREG_REG (x)) == VOIDmode)
            new_rtx = gen_rtx_CLOBBER (GET_MODE (x), const0_rtx);
         if (new_rtx)
            mtcs_recog_validate_change/*!validate_change*/(self,object, loc, new_rtx, 1);
         break;
      case ZERO_EXTRACT:
      case SIGN_EXTRACT:
         /* If we are replacing a register with memory, try to change the memory
         to be the mode required for memory in extract operations (this isn't
         likely to be an insertion operation; if it was, nothing bad will
         happen, we might just fail in some cases).  */

         if (MEM_P (XEXP (x, 0))
         && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (XEXP (x, 0)), &is_mode)
         && CONST_INT_P (XEXP (x, 1))
         && CONST_INT_P (XEXP (x, 2))
         && !mtcs_recog_mode_dependent_address_p/*!mode_dependent_address_p*/(self,XEXP (XEXP (x, 0), 0),
               mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,XEXP (x, 0)))
         && !MEM_VOLATILE_P (XEXP (x, 0))){
            int pos = INTVAL (XEXP (x, 2));
            machine_mode new_mode = is_mode;
            if (GET_CODE (x) == ZERO_EXTRACT && target_rtx_have_extzv/*!targetm.have_extzv*/(mtcsMachine->tmrtx))
               new_mode = mtcsOutput->insn_data[mtcsMachine->tmrtx->code_for_extzv/*!targetm.code_for_extzv*/].operand[1].mode;
            else if (GET_CODE (x) == SIGN_EXTRACT && target_rtx_have_extv/*!targetm.have_extv*/(mtcsMachine->tmrtx))
               new_mode = mtcsOutput->insn_data[ mtcsMachine->tmrtx->code_for_extv/*!targetm.code_for_extv*/].operand[1].mode;
            scalar_int_mode wanted_mode = (new_mode == VOIDmode
                  ? mtcsMode->word_mode : mtcs_mode_as_a <scalar_int_mode> (mtcsMode,new_mode));

            /* If we have a narrower mode, we can do something.  */
            if (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,wanted_mode) < mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,is_mode)){
               int offset = pos / BITS_PER_UNIT;
               rtx newmem;

               /* If the bytes and bits are counted differently, we
               must adjust the offset.  */
               if (BYTES_BIG_ENDIAN != BITS_BIG_ENDIAN)
                  offset =(mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,is_mode)
                        - mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,wanted_mode) - offset);

               gcc_assert (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,wanted_mode)
                     == mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,wanted_mode));
               pos %=mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,wanted_mode);

               newmem = mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/(mtcsRTL,XEXP (x, 0), wanted_mode, offset);

               mtcs_recog_validate_change/*!validate_change*/(self,object, &XEXP (x, 2), GEN_INT (pos), 1);
               mtcs_recog_validate_change/*!validate_change*/(self,object, &XEXP (x, 0), newmem, 1);
            }
         }

         break;

      default:
         break;
   }
}

/* Replace every occurrence of FROM in X with TO.  Mark each change with
   validate_change passing OBJECT.  */
//原型 validate_replace_rtx_1 recog.cc
static void validate_replace_rtx_1 (MtcsRecog *self,rtx *loc, rtx from, rtx to, rtx_insn *object,bool simplify)
{
   int i, j;
   const char *fmt;
   rtx x = *loc;
   enum rtx_code code;
   machine_mode op0_mode = VOIDmode;
   int prev_changes = self->num_changes;

   if (!x)
      return;

   code = GET_CODE (x);
   fmt = GET_RTX_FORMAT (code);
   if (fmt[0] == 'e')
      op0_mode = GET_MODE (XEXP (x, 0));

   /* X matches FROM if it is the same rtx or they are both referring to the
   same register in the same mode.  Avoid calling rtx_equal_p unless the
   operands look similar.  */

   if (x == from
   || (REG_P (x) && REG_P (from)
   && GET_MODE (x) == GET_MODE (from)
   && REGNO (x) == REGNO (from))
   || (GET_CODE (x) == GET_CODE (from) && GET_MODE (x) == GET_MODE (from)
   && rtx_equal_p (x, from))){
      mtcs_recog_validate_unshare_change/*!validate_unshare_change*/(self,object, loc, to, 1);
      return;
   }

   /* Call ourself recursively to perform the replacements.
   We must not replace inside already replaced expression, otherwise we
   get infinite recursion for replacements like (reg X)->(subreg (reg X))
   so we must special case shared ASM_OPERANDS.  */

   if (GET_CODE (x) == PARALLEL){
      for (j = XVECLEN (x, 0) - 1; j >= 0; j--){
         if (j && GET_CODE (XVECEXP (x, 0, j)) == SET
         && GET_CODE (SET_SRC (XVECEXP (x, 0, j))) == ASM_OPERANDS){
            /* Verify that operands are really shared.  */
            gcc_assert (ASM_OPERANDS_INPUT_VEC (SET_SRC (XVECEXP (x, 0, 0)))  == ASM_OPERANDS_INPUT_VEC (SET_SRC (XVECEXP(x, 0, j))));
            validate_replace_rtx_1(self,&SET_DEST (XVECEXP (x, 0, j)),from, to, object, simplify);
         }else
            validate_replace_rtx_1(self,&XVECEXP (x, 0, j), from, to, object,simplify);
      }
   }else
      for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--){
         if (fmt[i] == 'e')
            validate_replace_rtx_1(self,&XEXP (x, i), from, to, object, simplify);
         else if (fmt[i] == 'E')
            for (j = XVECLEN (x, i) - 1; j >= 0; j--)
               validate_replace_rtx_1(self,&XVECEXP (x, i, j), from, to, object,simplify);
      }

   /* If we didn't substitute, there is nothing more to do.  */
   if (self->num_changes == prev_changes)
      return;

   /* ??? The regmove is no more, so is this aberration still necessary?  */
   /* Allow substituted expression to have different mode.  This is used by
   regmove to change mode of pseudo register.  */
   if (fmt[0] == 'e' && GET_MODE (XEXP (x, 0)) != VOIDmode)
      op0_mode = GET_MODE (XEXP (x, 0));

   /* Do changes needed to keep rtx consistent.  Don't do any other
   simplifications, as it is not our job.  */
   if (simplify)
      simplify_while_replacing(self,loc, to, object, op0_mode);
}

/* Wrapper for validate_change_1 without the UNSHARE argument defaulting
   UNSHARE to true.  */
//原型 validate_unshare_change recog.h recog.cc
bool mtcs_recog_validate_unshare_change (MtcsRecog *self,rtx object, rtx *loc, rtx new_rtx, bool in_group)
{
  return validate_change_1(self,object, loc, new_rtx, in_group, true);
}
/* Try replacing every occurrence of FROM in INSN with TO.  After all
   changes have been made, validate by seeing if INSN is still valid.  */
//原型 validate_replace_rtx recog.h recog.cc
bool mtcs_recog_validate_replace_rtx (MtcsRecog *self,rtx from, rtx to, rtx_insn *insn)
{
  validate_replace_rtx_1(self,&PATTERN (insn), from, to, insn, true);
  return mtcs_recog_apply_change_group/*!apply_change_group*/(self);
}

/* Change XVECLEN (*LOC, 0) to NEW_LEN.  OBJECT, IN_GROUP and the return
   value are as for validate_change_1.  */
//原型 validate_change_xveclen recog.h recog.cc
bool mtcs_recog_validate_change_xveclen (MtcsRecog *self,rtx object, rtx *loc, int new_len, bool in_group)
{
  return validate_change_1(self,object, loc, *loc, in_group, false, new_len);
}


/* Swap the status of change NUM from being applied to not being applied,
   or vice versa.  */

static void swap_change (MtcsRecog *self,int num)
{
   change_t *changes=(change_t *)self->changes;

   if (changes[num].old_len >= 0)
      std::swap (XVECLEN (*changes[num].loc, 0), changes[num].old_len);
   else
      std::swap (*changes[num].loc, changes[num].old);
   if (changes[num].object && !MEM_P (changes[num].object))
      std::swap (INSN_CODE (changes[num].object), changes[num].old_code);
}

/* Temporarily undo all the changes numbered NUM and up, with a view
   to reapplying them later.  The next call to the changes machinery
   must be:

      redo_changes (NUM)

   otherwise things will end up in an invalid state.  */
//原型 temporarily_undo_changes recog.h recog.cc
void mtcs_recog_temporarily_undo_changes (MtcsRecog *self,int num)
{
  gcc_assert (self->temporarily_undone_changes == 0 && num <= self->num_changes);
  for (int i = self->num_changes - 1; i >= num; i--)
    swap_change (self,i);
  self->temporarily_undone_changes = self->num_changes - num;
}

/* Redo the changes that were temporarily undone by:

      temporarily_undo_changes (NUM).  */
//原型 redo_changes recog.h recog.cc
void mtcs_recog_redo_changes (MtcsRecog *self,int num)
{
   gcc_assert (self->temporarily_undone_changes == self->num_changes - num);
   for (int i = num; i < self->num_changes; ++i)
      swap_change (self,i);
   self->temporarily_undone_changes = 0;
}

/* Fill in OP_ALT_BASE for an instruction that has N_OPERANDS
   operands, N_ALTERNATIVES alternatives and constraint strings
   CONSTRAINTS.  OP_ALT_BASE has N_ALTERNATIVES * N_OPERANDS entries
   and CONSTRAINTS has N_OPERANDS entries.  OPLOC should be passed in
   if the insn is an asm statement and preprocessing should take the
   asm operands into account, e.g. to determine whether they could be
   addresses in constraints that require addresses; it should then
   point to an array of pointers to each operand.  */
//原型 preprocess_constraints recog.h recog.cc
void mtcs_recog_preprocess_constraints (MtcsRecog *self,int n_operands, int n_alternatives,
         const char **constraints,struct mtcs_operand_alternative *op_alt_base, rtx **oploc)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);

   for (int i = 0; i < n_operands; i++){
      int j;
      struct mtcs_operand_alternative *op_alt;
      const char *p = constraints[i];

      op_alt = op_alt_base;

      for (j = 0; j < n_alternatives; j++, op_alt += n_operands){
         op_alt[i].cl = NO_REGS;
         op_alt[i].register_filters = 0;
         op_alt[i].constraint = p;
         op_alt[i].matches = -1;
         op_alt[i].matched = -1;

         if (*p == '\0' || *p == ','){
            op_alt[i].anything_ok = 1;
            continue;
         }

         for (;;){
            char c = *p;
            if (c == '#')
               do
                  c = *++p;
               while (c != ',' && c != '\0');
            if (c == ',' || c == '\0'){
               p++;
               break;
            }

            switch (c){
               case '?':
                  op_alt[i].reject += 6;
                  break;
               case '!':
                  op_alt[i].reject += 600;
                  break;
               case '&':
                  op_alt[i].earlyclobber = 1;
                  break;

               case '0': case '1': case '2': case '3': case '4':
               case '5': case '6': case '7': case '8': case '9':
               {
                  char *end;
                  op_alt[i].matches = strtoul (p, &end, 10);
                  op_alt[op_alt[i].matches].matched = i;
                  p = end;
               }
                  continue;

               case 'X':
                  op_alt[i].anything_ok = 1;
                  break;

               case 'g':
                  op_alt[i].cl = mtcsReg->hardRegs.x_reg_class_subunion/*!reg_class_subunion*/
                  [(int) op_alt[i].cl][(int) mtcs_reg_get_general_regs/*!GENERAL_REGS*/(mtcsReg)];
                  break;

               default:
               enum constraint_num cn =mtcs_preds_lookup_constraint/*!lookup_constraint*/(mtcsPreds,p);
               enum reg_class cl;
               switch (mtcs_preds_get_constraint_type/*!get_constraint_type*/(mtcsPreds,cn)){
                  case CT_REGISTER:
                     cl = mtcs_preds_reg_class_for_constraint/*!reg_class_for_constraint*/(mtcsPreds,cn);
                     if (cl != NO_REGS){
                        op_alt[i].cl = mtcsReg->hardRegs.x_reg_class_subunion/*!reg_class_subunion*/[op_alt[i].cl][cl];
                        auto filter_id = mtcs_preds_get_register_filter_id/*!get_register_filter_id*/(mtcsPreds,cn);
                        if (filter_id >= 0)
                           op_alt[i].register_filters |= 1U << filter_id;
                     }
                     break;

                  case CT_CONST_INT:
                     break;

                  case CT_MEMORY:
                  case CT_SPECIAL_MEMORY:
                  case CT_RELAXED_MEMORY:
                     op_alt[i].memory_ok = 1;
                     break;

                  case CT_ADDRESS:
                     if (oploc && !mtcs_preds_address_operand/*!address_operand*/(mtcsPreds,*oploc[i], VOIDmode))
                        break;

                     op_alt[i].is_address = 1;
                     op_alt[i].cl = (mtcsReg->hardRegs.x_reg_class_subunion/*!reg_class_subunion*/
                     [(int) op_alt[i].cl]
                     [(int) mtcs_reg_base_reg_class/*!base_reg_class*/(mtcsReg,VOIDmode, ADDR_SPACE_GENERIC,ADDRESS, SCRATCH)]);
                     break;

                  case CT_FIXED_FORM:
                     break;
               }//end switch (mtcs_preds_get_constraint_type/*!get_constraint_type*/(mtcsPreds,cn)){
               break;
            }//end switch (c){
            p += mtcs_preds_insn_constraint_len/*!CONSTRAINT_LEN*/(mtcsPreds,c, p);
         }//end   for (;;){
      }
   }
}


/* After calling extract_insn, you can use this function to extract some
   information from the constraint strings into a more usable form.
   The collected data is stored in recog_op_alt.  */
//原型 preprocess_constraints recog.h recog.cc 重载函数
void mtcs_recog_preprocess_constraints (MtcsRecog *self,rtx_insn *insn)
{
   int icode = INSN_CODE (insn);
   if (icode >= 0)
      self->recog_op_alt = mtcs_recog_preprocess_insn_constraints/*!preprocess_insn_constraints*/(self,icode);
   else{
      int n_operands = self->recog_data.n_operands;
      int n_alternatives = self->recog_data.n_alternatives;
      int n_entries = n_operands * n_alternatives;
      memset (self->asm_op_alt, 0, n_entries * sizeof (struct mtcs_operand_alternative));
      mtcs_recog_preprocess_constraints/*!preprocess_constraints*/(self,n_operands, n_alternatives,
      self->recog_data.constraints, self->asm_op_alt,NULL);
      self->recog_op_alt = self->asm_op_alt;
   }
}

/* Return an array of operand_alternative instructions for
   instruction ICODE.  */
//原型 preprocess_insn_constraints recog.h recog.cc
const struct mtcs_operand_alternative * mtcs_recog_preprocess_insn_constraints (MtcsRecog *self,unsigned int icode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCodes *mtcsCodes=mtcs_target_get_codes(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);

   gcc_checking_assert (IN_RANGE (icode, 0, mtcs_codes_get_number/*!NUM_INSN_CODES*/(mtcsCodes) - 1));
   if (self->target_recog.x_op_alt/*!this_target_recog->x_op_alt*/[icode])
      return self->target_recog.x_op_alt/*!this_target_recog->x_op_alt*/[icode];

   int n_operands = mtcsOutput->insn_data[icode].n_operands;
   if (n_operands == 0)
      return 0;
   /* Always provide at least one alternative so that which_op_alt ()
   works correctly.  If the instruction has 0 alternatives (i.e. all
   constraint strings are empty) then each operand in this alternative
   will have anything_ok set.  */
   int n_alternatives = MAX (mtcsOutput->insn_data[icode].n_alternatives, 1);
   int n_entries = n_operands * n_alternatives;

   struct mtcs_operand_alternative *op_alt = XCNEWVEC (struct mtcs_operand_alternative, n_entries);
   const char **constraints = XALLOCAVEC (const char *, n_operands);

   for (int i = 0; i < n_operands; ++i)
      constraints[i] = mtcsOutput->insn_data[icode].operand[i].constraint;
   mtcs_recog_preprocess_constraints/*!preprocess_constraints*/(self,n_operands,
         n_alternatives, constraints, op_alt,NULL);

   self->target_recog.x_op_alt/*!this_target_recog->x_op_alt*/[icode] = op_alt;
   return op_alt;
}

/* Return the class for operand I of alternative ALT, taking matching
   constraints into account.  */
//原型 alternative_class recog.h
enum reg_class mtcs_recog_alternative_class (const struct mtcs_operand_alternative *alt, int i)
{
  return alt[i].matches >= 0 ? alt[alt[i].matches].cl : alt[i].cl;
}

/* Return the mask of register filters that should be applied to operand I
   of alternative ALT, taking matching constraints into account.  */
//原型 alternative_class recog.h
unsigned int mtcs_recog_alternative_register_filters (const struct mtcs_operand_alternative *alt, int i)
{
  return (alt[i].matches >= 0
     ? alt[alt[i].matches].register_filters
     : alt[i].register_filters);
}

/* Function called by note_uses to replace used subexpressions.  */
struct validate_replace_src_data
{
  rtx from;       /* Old RTX */
  rtx to;         /* New RTX */
  rtx_insn *insn;    /* Insn in which substitution is occurring.  */
  MtcsRecog *mtcsRecog;
};

//原型 validate_replace_src_1 recog.cc
static void validate_replace_src_1 (rtx *x, void *data)
{
  struct validate_replace_src_data *d = (struct validate_replace_src_data *) data;
  validate_replace_rtx_1 (d->mtcsRecog,x, d->from, d->to, d->insn, true);
}

/* Try replacing every occurrence of FROM in INSN with TO, avoiding
   SET_DESTs.  */
//原型 validate_replace_src_group recog.h recog.cc
void mtcs_recog_validate_replace_src_group (MtcsRecog *self,rtx from, rtx to, rtx_insn *insn)
{
  struct validate_replace_src_data d;

  d.from = from;
  d.to = to;
  d.insn = insn;
  d.mtcsRecog = self;
  note_uses (&PATTERN (insn), validate_replace_src_1, &d);
}

/* Check whether INSN matches a specific alternative of an .md pattern.  */
//原型 valid_insn_p recog.h recog.cc
bool mtcs_recog_valid_insn_p (MtcsRecog *self,rtx_insn *insn)
{
   mtcs_recog_recog_memoized/*!recog_memoized*/(self,insn);
   if (INSN_CODE (insn) < 0)
      return false;
   mtcs_recog_extract_insn/*!extract_insn*/(self,insn);
   /* We don't know whether the insn will be in code that is optimized
   for size or speed, so consider all enabled alternatives.  */
   if (!mtcs_recog_constrain_operands/*!constrain_operands*/(self,
         1, mtcs_recog_get_enabled_alternatives/*!get_enabled_alternatives*/(self,insn)))
      return false;
   return true;
}

/* Split single instruction.  Helper function for split_all_insns and
   split_all_insns_noflow.  Return last insn in the sequence if successful,
   or NULL if unsuccessful.  */
static rtx_insn * split_insn (MtcsRecog *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   /* Split insns here to get max fine-grain parallelism.  */
   rtx_insn *first = PREV_INSN (insn);
   rtx_insn *last = mtcs_emit_try_split/*!try_split*/(mtcsEmit,PATTERN (insn), insn, 1);
   rtx insn_set, last_set, note;

   if (last == insn)
      return NULL;

   /* If the original instruction was a single set that was known to be
   equivalent to a constant, see if we can say the same about the last
   instruction in the split sequence.  The two instructions must set
   the same destination.  */
   insn_set = single_set (insn);
   if (insn_set){
      last_set = single_set (last);
      if (last_set && rtx_equal_p (SET_DEST (last_set), SET_DEST (insn_set))){
         note = find_reg_equal_equiv_note (insn);
         if (note && CONSTANT_P (XEXP (note, 0)))
            mtcs_rtl_set_unique_reg_note/*!set_unique_reg_note*/(mtcsRTL,last, REG_EQUAL, XEXP (note, 0));
         else if (CONSTANT_P (SET_SRC (insn_set)))
            mtcs_rtl_set_unique_reg_note/*!set_unique_reg_note*/(mtcsRTL,last, REG_EQUAL,copy_rtx (SET_SRC (insn_set)));
      }
   }

   /* try_split returns the NOTE that INSN became.  */
   mtcs_rtl_set_insn_deleted/*!SET_INSN_DELETED*/(mtcsRTL,insn);

   /* ??? Coddle to md files that generate subregs in post-reload
   splitters instead of computing the proper hard register.  */
   if (reload_completed && first != last){
      first = NEXT_INSN (first);
      for (;;){
         if (INSN_P (first))
            mtcs_output_cleanup_subreg_operands/*!cleanup_subreg_operands*/(mtcsOutput,first);
         if (first == last)
            break;
         first = NEXT_INSN (first);
      }
   }

   return last;
}

/* Split all insns in the function.  If UPD_LIFE, update life info after.  */
//原型 split_all_insns rtl.h recog.cc
void mtcs_recog_split_all_insns (MtcsRecog *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsCfgCleanup *mtcsCfgCleanup=mtcs_target_get_cfg_cleanup(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsCfgBuild *mtcsCfgBuild=mtcs_target_get_cfg_build(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   bool changed;
   bool need_cfg_cleanup = false;
   basic_block bb;

   auto_sbitmap blocks (last_basic_block_for_fn (cfun));
   bitmap_clear (blocks);
   changed = false;

   FOR_EACH_BB_REVERSE_FN (bb, cfun){
      rtx_insn *insn, *next;
      bool finish = false;

      mtcs_func_rtl_profile_for_bb/*!rtl_profile_for_bb*/(mtcsFunc,bb);
      for (insn = BB_HEAD (bb); !finish ; insn = next){
         /* Can't use `next_real_insn' because that might go across
         CODE_LABELS and short-out basic blocks.  */
         next = NEXT_INSN (insn);
         finish = (insn == BB_END (bb));

         /* If INSN has a REG_EH_REGION note and we split INSN, the
         resulting split may not have/need REG_EH_REGION notes.

         If that happens and INSN was the last reference to the
         given EH region, then the EH region will become unreachable.
         We cannot leave the unreachable blocks in the CFG as that
         will trigger a checking failure.

         So track if INSN has a REG_EH_REGION note.  If so and we
         split INSN, then trigger a CFG cleanup.  */
         rtx note = find_reg_note (insn, REG_EH_REGION, NULL_RTX);
         if (INSN_P (insn)){
            rtx set = single_set (insn);

            /* Don't split no-op move insns.  These should silently
            disappear later in final.  Splitting such insns would
            break the code that handles LIBCALL blocks.  */
            if (set && mtcs_rtlanal_set_noop_p/*!set_noop_p*/(mtcsRtlanal,set)){
               /* Nops get in the way while scheduling, so delete them
               now if register allocation has already been done.  It
               is too risky to try to do this before register
               allocation, and there are unlikely to be very many
               nops then anyways.  */
               if (reload_completed)
                  mtcs_cfg_rtl_delete_insn_and_edges/*!delete_insn_and_edges*/(mtcsCfgRtl,insn);
               if (note)
                  need_cfg_cleanup = true;
            }else{
                  if (split_insn(self,insn)){
                     bitmap_set_bit (blocks, bb->index);
                     changed = true;
                     if (note)
                        need_cfg_cleanup = true;
                  }
            }
         }
      }
   }

   mtcs_func_default_rtl_profile/*!default_rtl_profile*/(mtcsFunc);
   if (changed){
      mtcs_cfg_build_find_many_sub_basic_blocks/*!find_many_sub_basic_blocks*/(mtcsCfgBuild,blocks);

      /* Splitting could drop an REG_EH_REGION if it potentially
      trapped in its original form, but does not in its split
      form.  Consider a FLOAT_TRUNCATE which splits into a memory
      store/load pair and -fnon-call-exceptions.  */
      if (need_cfg_cleanup)
         mtcs_cfg_cleanup_cleanup_cfg/*!cleanup_cfg*/(mtcsCfgCleanup,0);
   }

   mtcs_cfg_context_checking_verify_flow_info/*!checking_verify_flow_info*/(mtcsCfgContext);
}

/* Same as split_all_insns, but do not expect CFG to be available.
   Used by machine dependent reorg passes.  */
//原型 split_all_insns_noflow rtl.h recog.cc
void mtcs_recog_split_all_insns_noflow (MtcsRecog *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   rtx_insn *next, *insn;

   for (insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = next){
      next = NEXT_INSN (insn);
      if (INSN_P (insn)){
         /* Don't split no-op move insns.  These should silently
         disappear later in final.  Splitting such insns would
         break the code that handles LIBCALL blocks.  */
         rtx set = single_set (insn);
         if (set && mtcs_rtlanal_set_noop_p/*!set_noop_p*/(mtcsRtlanal,set)){
            /* Nops get in the way while scheduling, so delete them
            now if register allocation has already been done.  It
            is too risky to try to do this before register
            allocation, and there are unlikely to be very many
            nops then anyways.

            ??? Should we use delete_insn when the CFG isn't valid?  */
            if (reload_completed)
               mtcs_cfg_rtl_delete_insn_and_edges/*!delete_insn_and_edges*/(mtcsCfgRtl,insn);
         }else
            split_insn(self,insn);
      }
   }
}

/* Like extract_insn, but save insn extracted and don't extract again, when
   called again for the same insn expecting that recog_data still contain the
   valid information.  This is used primary by gen_attr infrastructure that
   often does extract insn again and again.  */
//原型 extract_insn_cached recog.h recog.cc
void mtcs_recog_extract_insn_cached (MtcsRecog *self,rtx_insn *insn)
{
   if (self->recog_data.insn == insn && INSN_CODE (insn) >= 0)
      return;
   mtcs_recog_extract_insn/*!extract_insn*/(self,insn);
   self->recog_data.insn = insn;
}

static bool enable_split_before_sched2 (MtcsRecog *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   if(mtcs_config_ifdef(mtcsConfig,MTCS_INSN_SCHEDULING)){ /*!#ifdef INSN_SCHEDULING host INSN_SCHEDULING=1 nvptx=0*/
//#ifdef INSN_SCHEDULING
      return mtcsOptionsItem->x_optimize > 0 && mtcsOptionsItem->x_flag_schedule_insns_after_reload;
//#else
   }else{
      return false;
//#endif
   }
}


/* Try replacing every occurrence of FROM in INSN with TO.  This also
   will replace in REG_EQUAL and REG_EQUIV notes.  */
//原型 validate_replace_rtx_group recog.h recog.cc
void mtcs_recog_validate_replace_rtx_group (MtcsRecog *self,rtx from, rtx to, rtx_insn *insn)
{
   rtx note;
   validate_replace_rtx_1(self,&PATTERN (insn), from, to, insn, true);
   for (note = REG_NOTES (insn); note; note = XEXP (note, 1))
      if (REG_NOTE_KIND (note) == REG_EQUAL|| REG_NOTE_KIND (note) == REG_EQUIV)
         validate_replace_rtx_1(self,&XEXP (note, 0), from, to, insn, true);
}


/****************************以下是基于MtcsRecog的 rtl pass **************************************/
//原型 NEXT_PASS (pass_split_all_insns, 1); RTL_PASS recog.cc split1 n 无条件执行  split_all_insns ();
static nuint split_all_insns_execute_cb(MtcsPass *mtcsPass,function *func)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   mtcs_recog_split_all_insns/*!split_all_insns*/(mtcsRecog);
   return 0;
}

static void mtcsPassSplitAllInsnsInit(MtcsPassSplitAllInsns *self)
{
   MtcsPass *mtcsPass= (MtcsPass *)self;
   mtcsPass->execute =split_all_insns_execute_cb;
   mtcs_pass_set_properties(mtcsPass,
      0,/* properties_required */
      PROP_rtl_split_insns, /* properties_provided */
      0 /* properties_destroyed */);
   mtcs_pass_set_todo_flags(mtcsPass,
      0, /* todo_flags_start */
      0 /*todo_flags_finish */);
}

MtcsPassSplitAllInsns *mtcs_pass_split_all_insns_new(MtcsMode *mtcsMode)
{
   MtcsPassSplitAllInsns *self = n_slice_alloc0 (sizeof(MtcsPassSplitAllInsns));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"split1");
   mtcsPassSplitAllInsnsInit(self);
   return self;
}

//原型 NEXT_PASS (pass_split_after_reload, 1); RTL_PASS recog.cc split2 y 有条件执行  optimize > 0; split_all_insns
static nboolean split_after_reload_gate_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   return mtcsOptionsItem->x_optimize>0 ;
}

static nuint split_after_reload_execute_cb(MtcsPass *mtcsPass,function *func)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   mtcs_recog_split_all_insns/*!split_all_insns*/(mtcsRecog);
   return 0;
}

static void mtcsPassSplitAfterReloadInit(MtcsPassSplitAfterReload *self)
{
   MtcsPass *mtcsPass= (MtcsPass *)self;
   mtcsPass->execute =split_after_reload_execute_cb;
   mtcsPass->gate =split_after_reload_gate_cb;

   mtcs_pass_set_properties(mtcsPass,
      0,/* properties_required */
      0, /* properties_provided */
      0 /* properties_destroyed */);
   mtcs_pass_set_todo_flags(mtcsPass,
      0, /* todo_flags_start */
      0 /*todo_flags_finish */);
}

MtcsPassSplitAfterReload *mtcs_pass_split_after_reload_new(MtcsMode *mtcsMode)
{
   MtcsPassSplitAfterReload *self = n_slice_alloc0 (sizeof(MtcsPassSplitAfterReload));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"split2");
   mtcsPassSplitAfterReloadInit(self);
   return self;
}

//原型 NEXT_PASS (pass_split_before_sched2, 1); RTL_PASS recog.cc split3 y 有条件执行  enable_split_before_sched2; split_all_insns
static nboolean split_before_sched2_gate_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   return enable_split_before_sched2(mtcsRecog);
}

static nuint split_before_sched2_execute_cb(MtcsPass *mtcsPass,function *func)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   mtcs_recog_split_all_insns/*!split_all_insns*/(mtcsRecog);
   return 0;
}

static void mtcsPassSplitBeforeSched2Init(MtcsPassSplitBeforeSched2 *self)
{
   MtcsPass *mtcsPass= (MtcsPass *)self;
   mtcsPass->execute =split_before_sched2_execute_cb;
   mtcsPass->gate =split_before_sched2_gate_cb;

   mtcs_pass_set_properties(mtcsPass,
      0,/* properties_required */
      0, /* properties_provided */
      0 /* properties_destroyed */);
   mtcs_pass_set_todo_flags(mtcsPass,
      0, /* todo_flags_start */
      0 /*todo_flags_finish */);
}

MtcsPassSplitBeforeSched2 *mtcs_pass_split_before_sched2_new(MtcsMode *mtcsMode)
{
   MtcsPassSplitBeforeSched2 *self = n_slice_alloc0 (sizeof(MtcsPassSplitBeforeSched2));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"split3");
   mtcsPassSplitBeforeSched2Init(self);
   return self;
}

//原型 NEXT_PASS (pass_split_before_regstack, 1); RTL_PASS recog.cc split4 y 有条件执行  enable_split_before_sched2; split_all_insns
static nboolean split_before_regstack_gate_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsInsnAttr *mtcsInsnAttr=mtcs_target_get_insn_attr(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   if(mtcs_insn_attr_get_have_attr_length(mtcsInsnAttr) && mtcs_config_ifdefine(mtcsConfig,MTCS_STACK_REGS)){
      if(mtcs_config_ifdef(mtcsConfig,MTCS_INSN_SCHEDULING)){
         return !enable_split_before_sched2(mtcsRecog) || mtcsOptionsItem->x_flag_selective_scheduling2;
      }else{
         return !enable_split_before_sched2(mtcsRecog);
      }
   }else{
      return false;
   }
  /*!
#if HAVE_ATTR_length && defined (STACK_REGS)
//   If flow2 creates new instructions which need splitting
//     and scheduling after reload is not done, they might not be
//     split until final which doesn't allow splitting
//     if HAVE_ATTR_length.  Selective scheduling can result in
//     further instructions that need splitting.
#ifdef INSN_SCHEDULING
  return !enable_split_before_sched2 () || flag_selective_scheduling2;
#else
  return !enable_split_before_sched2 ();
#endif
#else
  return false;
#endif
  */
}

static nuint split_before_regstack_execute_cb(MtcsPass *mtcsPass,function *func)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   mtcs_recog_split_all_insns/*!split_all_insns*/(mtcsRecog);
   return 0;
}

static void mtcsPassSplitBeforeRegstackInit(MtcsPassSplitBeforeRegstack *self)
{
   MtcsPass *mtcsPass= (MtcsPass *)self;
   mtcsPass->execute =split_before_regstack_execute_cb;
   mtcsPass->gate =split_before_regstack_gate_cb;
   mtcs_pass_set_properties(mtcsPass,
      0,/* properties_required */
      0, /* properties_provided */
      0 /* properties_destroyed */);
   mtcs_pass_set_todo_flags(mtcsPass,
      0, /* todo_flags_start */
      0 /*todo_flags_finish */);
}

MtcsPassSplitBeforeRegstack *mtcs_pass_split_before_regstack_new(MtcsMode *mtcsMode)
{
   MtcsPassSplitBeforeRegstack *self = n_slice_alloc0 (sizeof(MtcsPassSplitBeforeRegstack));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"split4");
   mtcsPassSplitBeforeRegstackInit(self);
   return self;
}

//原型 NEXT_PASS (pass_split_for_shorten_branches, 1); RTL_PASS recog.cc split5 y 有条件执行  enable_split_before_sched2; split_all_insns
static nboolean split_for_shorten_branches_gate_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsInsnAttr *mtcsInsnAttr=mtcs_target_get_insn_attr(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);

   if(mtcs_insn_attr_get_have_attr_length(mtcsInsnAttr) && !mtcs_config_ifdefine(mtcsConfig,MTCS_STACK_REGS)){
      return true;
   }else{
      return false;
   }
}

static nuint split_for_shorten_branches_execute_cb(MtcsPass *mtcsPass,function *func)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   mtcs_recog_split_all_insns_noflow/*!split_all_insns_noflow*/(mtcsRecog);
   return 0;
}

static void mtcsPassSplitForShortenBranchesInit(MtcsPassSplitForShortenBranches *self)
{
   MtcsPass *mtcsPass= (MtcsPass *)self;
   mtcsPass->execute =split_for_shorten_branches_execute_cb;
   mtcsPass->gate =split_for_shorten_branches_gate_cb;
   mtcs_pass_set_properties(mtcsPass,
      0,/* properties_required */
      0, /* properties_provided */
      0 /* properties_destroyed */);
   mtcs_pass_set_todo_flags(mtcsPass,
      0, /* todo_flags_start */
      0 /*todo_flags_finish */);
}

MtcsPassSplitForShortenBranches *mtcs_pass_split_for_shorten_branches_new(MtcsMode *mtcsMode)
{
   MtcsPassSplitForShortenBranches *self = n_slice_alloc0 (sizeof(MtcsPassSplitForShortenBranches));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"split5");
   mtcsPassSplitForShortenBranchesInit(self);
   return self;
}
