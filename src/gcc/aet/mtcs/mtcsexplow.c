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
 * base on explow.cc
 */


/* This file handles generation of all the assembler code
   *except* the instructions of a lowtion.
   This includes declarations of variables and their initial values.

   We also output the assembler code for constants stored in memory
   and are responsible for combining constants with the same value.  */

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

#include "mtcsexplow.h"
#include "mtcstarget.h"
#include "mtcsasm.h"

//原型 adjust_stack_1 explow.cc
static void adjust_stack_1 (MtcsExplow *self,rtx adjust, bool anti_p);

static void mtcsExplowInit(MtcsExplow *self)
{

}


/* Round the size of a block to be pushed up to the boundary required
   by this machine.  SIZE is the desired size, which need not be constant.  */
//原型 round_push explow.cc
static rtx round_push (MtcsExplow *self,rtx size)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);

  mtcs_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
  rtx align_rtx, alignm1_rtx;

  if (!mtcs_func_is_support_stack_alignment/*!SUPPORTS_STACK_ALIGNMENT*/(mtcsFunc)
      || mtcsRtlData/*!crtl*/->preferred_stack_boundary ==
              mtcs_func_get_max_support_stack_alignment/*!MAX_SUPPORTED_STACK_ALIGNMENT*/(mtcsFunc)){
      int align = mtcsRtlData/*!crtl*/->preferred_stack_boundary / BITS_PER_UNIT;
      if (align == 1)
          return size;
      if (CONST_INT_P (size)){
          HOST_WIDE_INT new_size = (INTVAL (size) + align - 1) / align * align;

          if (INTVAL (size) != new_size)
            size = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,new_size);
          return size;
      }
      align_rtx =mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,align);
      alignm1_rtx =mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,align - 1);
  }else{
      /* If crtl->preferred_stack_boundary might still grow, use
     virtual_preferred_stack_boundary_rtx instead.  This will be
     substituted by the right value in vregs pass and optimized
     during combine.  */
      align_rtx = mtcs_rtl_get_virtual_preferred_stack_boundary_rtx/*!virtual_preferred_stack_boundary_rtx*/(mtcsRTL);
      alignm1_rtx = mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,
              mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, align_rtx, -1),NULL_RTX);
  }

  /* CEIL_DIV_EXPR needs to worry about the addition overflowing,
     but we know it can't.  So add ourselves and then do
     TRUNC_DIV_EXPR.  */
  size = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,pMode, add_optab, size, alignm1_rtx,
               NULL_RTX, 1, OPTAB_LIB_WIDEN);
  size = mtcs_expmed_expand_divmod/*!expand_divmod*/(mtcsExpmed,0, TRUNC_DIV_EXPR, pMode, size, align_rtx,
            NULL_RTX, 1);
  size = mtcs_expmed_expand_mult/*!expand_mult*/(mtcsExpmed,pMode, size, align_rtx, NULL_RTX, 1);

  return size;
}

/* A helper for adjust_stack and anti_adjust_stack.  */
//原型 adjust_stack_1 explow.cc
static void adjust_stack_1 (MtcsExplow *self,rtx adjust, bool anti_p)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

  rtx temp;
  rtx_insn *insn;
  rtx  stackPointerRtx= mtcs_rtl_get_stack_pointer_rtx(mtcsRTL);

  /* Hereafter anti_p means subtract_p.  */
  if (!mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc))
    anti_p = !anti_p;

  temp = mtcs_optabs_expand_binop(mtcsOptabs,mtcs_mode_get_Pmode(mtcsMode),
               anti_p ? sub_optab : add_optab,
                       stackPointerRtx/*!stack_pointer_rtx*/, adjust, stackPointerRtx/*!stack_pointer_rtx*/, 0,
               OPTAB_LIB_WIDEN);

  if (temp != stackPointerRtx/*!stack_pointer_rtx*/)
    insn = mtcs_expr_emit_move_insn(mtcsExpr,stackPointerRtx/*!stack_pointer_rtx*/, temp);
  else{
      insn = mtcs_rtl_data_get_last_insn (mtcsRtlData);
      temp = single_set (insn);
      gcc_assert (temp != NULL && SET_DEST (temp) == stackPointerRtx/*!stack_pointer_rtx*/);
  }

  if (!self->suppress_reg_args_size)
    mtcs_rtlanal_add_args_size_note (mtcsRtlanal,insn, mtcsRtlData->expr.x_stack_pointer_delta/*!stack_pointer_delta*/);
}


/* Return a copy of X in which all memory references
   and all constants that involve symbol refs
   have been replaced with new temporary registers.
   Also emit code to load the memory locations and constants
   into those registers.

   If X contains no such constants or memory references,
   X itself (not a copy) is returned.

   If a constant is found in the address that is not a legitimate constant
   in an insn, it is left alone in the hope that it might be valid in the
   address.

   X may contain no arithmetic except addition, subtraction and multiplication.
   Values returned by expand_expr with 1 for sum_ok fit this constraint.  */
//原型 explow
static rtx break_out_memory_refs (MtcsExplow *self,rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  if (MEM_P (x) || (CONSTANT_P (x) && mtcs_rtl_constant_address_p/*!CONSTANT_ADDRESS_P*/(mtcsRTL,x) && GET_MODE (x) != VOIDmode))
    x = mtcs_explow_force_reg/*!force_reg*/ (self,GET_MODE (x), x);
  else if (GET_CODE (x) == PLUS || GET_CODE (x) == MINUS || GET_CODE (x) == MULT){
      rtx op0 = break_out_memory_refs (self,XEXP (x, 0));
      rtx op1 = break_out_memory_refs (self,XEXP (x, 1));

      if (op0 != XEXP (x, 0) || op1 != XEXP (x, 1))
          x = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,GET_CODE (x), GET_MODE (x), op0, op1);
  }
  return x;
}

/* Load X into a register if it is not already one.
   Use mode MODE for the register.
   X should be valid for mode MODE, but it may be a constant which
   is valid for all integer modes; that's why caller must specify MODE.

   The caller must not alter the value in the register we return,
   since we mark it as a "constant" register.  */
//原型 force_reg explow.h explow.cc
rtx mtcs_explow_force_reg (MtcsExplow *self,machine_mode mode, rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  rtx temp, set;
  rtx_insn *insn;

  if (REG_P (x))
    return x;

  if (mtcs_preds_general_operand/*!general_operand*/ (mtcsPreds,x, mode)){
      temp =mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/ (mtcsEmit,mode);
      insn = mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,temp, x);
  }else{
      temp = mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,x, NULL_RTX);
      if (REG_P (temp))
          insn = mtcs_rtl_data_get_last_insn/*!get_last_insn*/ (mtcsRtlData);
      else{
          rtx temp2 = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
          insn = mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,temp2, temp);
          temp = temp2;
      }
  }

  /* Let optimizers know that TEMP's value never changes
     and that X can be substituted for it.  Don't get confused
     if INSN set something else (such as a SUBREG of TEMP).  */
  if (CONSTANT_P (x) && (set = single_set (insn)) != 0 && SET_DEST (set) == temp && ! rtx_equal_p (x, SET_SRC (set)))
     mtcs_rtl_set_unique_reg_note/*!set_unique_reg_note*/(mtcsRTL,insn, REG_EQUAL, x);

  /* Let optimizers know that TEMP is a pointer, and if so, the
     known alignment of that pointer.  */
  {
    unsigned align = 0;
    if (GET_CODE (x) == SYMBOL_REF){
        align = BITS_PER_UNIT;
        if (SYMBOL_REF_DECL (x) && DECL_P (SYMBOL_REF_DECL (x)))
          align = DECL_ALIGN (SYMBOL_REF_DECL (x));
    }else if (GET_CODE (x) == LABEL_REF)
      align = BITS_PER_UNIT;
    else if (GET_CODE (x) == CONST && GET_CODE (XEXP (x, 0)) == PLUS
         && GET_CODE (XEXP (XEXP (x, 0), 0)) == SYMBOL_REF && CONST_INT_P (XEXP (XEXP (x, 0), 1))){
        rtx s = XEXP (XEXP (x, 0), 0);
        rtx c = XEXP (XEXP (x, 0), 1);
        unsigned sa, ca;

        sa = BITS_PER_UNIT;
        if (SYMBOL_REF_DECL (s) && DECL_P (SYMBOL_REF_DECL (s)))
          sa = DECL_ALIGN (SYMBOL_REF_DECL (s));

        if (INTVAL (c) == 0)
          align = sa;
        else {
            ca = ctz_hwi (INTVAL (c)) * BITS_PER_UNIT;
            align = MIN (sa, ca);
        }
    }

    if (align || (MEM_P (x) && MEM_POINTER (x)))
        mtcs_rtl_mark_reg_pointer/*!mark_reg_pointer*/(mtcsRTL,temp, align);
  }

  return temp;
}

/* Copy the value or contents of X to a new temp reg and return that reg.  */
//原型 copy_to_reg explow.h explow.cc
rtx mtcs_explow_copy_to_reg (MtcsExplow *self,rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  rtx temp = mtcs_emit_gen_reg_rtx (mtcsEmit,GET_MODE (x));

  /* If not an operand, must be an address with PLUS and MULT so
     do the computation.  */
  if (! mtcs_preds_general_operand (mtcsPreds,x, VOIDmode))
    x = mtcs_expr_force_operand (mtcsExpr,x, temp);

  if (x != temp)
    mtcs_expr_emit_move_insn (mtcsExpr,temp, x);

  return temp;
}

//原型 #define memory_address(MODE,RTX)    memory_address_addr_space ((MODE), (RTX), ADDR_SPACE_GENERIC) explow.h
rtx mtcs_explow_memory_address(MtcsExplow *self,machine_mode mode, rtx x)
{
    return mtcs_explow_memory_address_addr_space(self,mode,x,ADDR_SPACE_GENERIC);
}

/* Return something equivalent to X but valid as a memory address for something
   of mode MODE in the named address space AS.  When X is not itself valid,
   this works by copying X or subexpressions of it into registers.  */
/*
//原型 memory_address_addr_space explow.h explow.cc
* eliminate_constant_term(explow.cc) 调 simplify_binary_operation(simplify-rtx.cc) 调 avoid_constant_pool_reference 调
* const_double_from_real_value(emit-rtl.cc) 调 lookup_const_double 和 targetm.delegitimize_address (addr);
*/
rtx mtcs_explow_memory_address_addr_space (MtcsExplow *self,machine_mode mode, rtx x, addr_space_t as)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  rtx oldx = x;
  scalar_int_mode address_mode =target_addr_space_address_mode/*!targetm.addr_space.address_mode*/(mtcsMachine->addrSpace,as);

  x = mtcs_explow_convert_memory_address_addr_space/*!convert_memory_address_addr_space*/ (self,address_mode, x, as);

  /* By passing constant addresses through registers
     we get a chance to cse them.  */
  if (! cse_not_expected && CONSTANT_P (x) && mtcs_rtl_constant_address_p/*!CONSTANT_ADDRESS_P*/(mtcsRTL,x))
    x = mtcs_explow_force_reg/*!force_reg*/ (self,address_mode, x);

  /* We get better cse by rejecting indirect addressing at this stage.
     Let the combiner create indirect addresses where appropriate.
     For now, generate the code so that the subexpressions useful to share
     are visible.  But not if cse won't be done!  */
  else{
      if (! cse_not_expected && !REG_P (x))
          x = break_out_memory_refs (self,x);

      /* At this point, any valid address is accepted.  */
      if (mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/ (mtcsRecog,mode, x, as,ERROR_MARK))
          goto done;

      /* If it was valid before but breaking out memory refs invalidated it,
     use it the old way.  */
      if (mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/ (mtcsRecog,mode, oldx, as,ERROR_MARK)){
          x = oldx;
          goto done;
      }

      /* Perform machine-dependent transformations on X
     in certain cases.  This is not necessary since the code
     below can handle all possible cases, but machine-dependent
     transformations can make better code.  */
      {
        rtx orig_x = x;
        x = target_addr_space_legitimize_address/*!targetm.addr_space.legitimize_address*/(mtcsMachine->addrSpace,x, oldx, mode, as);
        if (orig_x != x && mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/ (mtcsRecog,mode, x, as,ERROR_MARK))
          goto done;
      }

      /* PLUS and MULT can appear in special ways
     as the result of attempts to make an address usable for indexing.
     Usually they are dealt with by calling force_operand, below.
     But a sum containing constant terms is special
     if removing them makes the sum a valid address:
     then we generate that address in a register
     and index off of it.  We do this because it often makes
     shorter code, and because the addresses thus generated
     in registers often become common subexpressions.  */
      if (GET_CODE (x) == PLUS){
          rtx constant_term = const0_rtx;
          rtx y = mtcs_explow_eliminate_constant_term/*eliminate_constant_term*/ (self,x, &constant_term);
          if (constant_term == const0_rtx
                  || !mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/ (mtcsRecog,mode, y, as,ERROR_MARK))
            x = mtcs_expr_force_operand/*!force_operand*/ (mtcsExpr,x, NULL_RTX);
          else{
              y = gen_rtx_PLUS (GET_MODE (x), mtcs_explow_copy_to_reg (self,y), constant_term);
              if (! mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/(mtcsRecog,mode, y, as,ERROR_MARK))
                  x = mtcs_expr_force_operand/*!force_operand*/ (mtcsExpr,x, NULL_RTX);
              else
                  x = y;
          }
      }else if (GET_CODE (x) == MULT || GET_CODE (x) == MINUS)
          x = mtcs_expr_force_operand/*!force_operand*/ (mtcsExpr,x, NULL_RTX);

      /* If we have a register that's an invalid address,
     it must be a hard reg of the wrong class.  Copy it to a pseudo.  */
      else if (REG_P (x))
          x = mtcs_explow_copy_to_reg (self,x);
      /* Last resort: copy the value to a register, since
     the register is a valid address.  */
      else
          x = mtcs_explow_force_reg/*!force_reg*/(self,address_mode, x);
  }

 done:

  gcc_assert (mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/ (mtcsRecog,mode, x, as,ERROR_MARK));
  /* If we didn't change the address, we are done.  Otherwise, mark
     a reg as a pointer if we have REG or REG + CONST_INT.  */
  if (oldx == x)
    return x;
  else if (REG_P (x))
      mtcs_rtl_mark_reg_pointer/*!mark_reg_pointer*/(mtcsRTL,x, BITS_PER_UNIT);
  else if (GET_CODE (x) == PLUS && REG_P (XEXP (x, 0))  && CONST_INT_P (XEXP (x, 1)))
      mtcs_rtl_mark_reg_pointer/*!mark_reg_pointer*/(mtcsRTL,XEXP (x, 0), BITS_PER_UNIT);

  /* OLDX may have been the address on a temporary.  Update the address
     to indicate that X is now used.  */
  mtcs_func_update_temp_slot_address/*!update_temp_slot_address*/ (mtcsFunc,oldx, x);

  return x;
}


/* Given X, a memory address in address space AS' pointer mode, convert it to
   an address in the address space's address mode, or vice versa (TO_MODE says
   which way).  We take advantage of the fact that pointers are not allowed to
   overflow by commuting arithmetic operations over conversions so that address
   arithmetic insns can be used. IN_CONST is true if this conversion is inside
   a CONST. NO_EMIT is true if no insns should be emitted, and instead
   it should return NULL if it can't be simplified without emitting insns.  */
//原型 convert_memory_address_addr_space_1 rtl.h explow.cc
//POINTERS_EXTEND_UNSIGNED host=1 nvptx=0 gen=? spirv=? 实现gen或spirv时重做
rtx mtcs_explow_convert_memory_address_addr_space_1 (MtcsExplow *self,scalar_int_mode to_mode ATTRIBUTE_UNUSED,
                     rtx x, addr_space_t as ATTRIBUTE_UNUSED, bool in_const ATTRIBUTE_UNUSED,bool no_emit ATTRIBUTE_UNUSED)
{
      n_debug("mtcsexplow.c mtcs_explow_convert_memory_address_addr_space_1 00 %d %d\n",GET_MODE (x),to_mode);
      gcc_assert (GET_MODE (x) == to_mode || GET_MODE (x) == VOIDmode);
      return x;
//#ifndef POINTERS_EXTEND_UNSIGNED
//  gcc_assert (GET_MODE (x) == to_mode || GET_MODE (x) == VOIDmode);
//  return x;
//#else /* defined(POINTERS_EXTEND_UNSIGNED) */
//  scalar_int_mode pointer_mode, address_mode, from_mode;
//  rtx temp;
//  enum rtx_code code;
//
//  /* If X already has the right mode, just return it.  */
//  if (GET_MODE (x) == to_mode)
//    return x;
//
//  pointer_mode = targetm.addr_space.pointer_mode (as);
//  address_mode = targetm.addr_space.address_mode (as);
//  from_mode = to_mode == pointer_mode ? address_mode : pointer_mode;
//
//  /* Here we handle some special cases.  If none of them apply, fall through
//     to the default case.  */
//  switch (GET_CODE (x))
//    {
//    CASE_CONST_SCALAR_INT:
//      if (GET_MODE_SIZE (to_mode) < GET_MODE_SIZE (from_mode))
//    code = TRUNCATE;
//      else if (POINTERS_EXTEND_UNSIGNED < 0)
//    break;
//      else if (POINTERS_EXTEND_UNSIGNED > 0)
//    code = ZERO_EXTEND;
//      else
//    code = SIGN_EXTEND;
//      temp = simplify_unary_operation (code, to_mode, x, from_mode);
//      if (temp)
//    return temp;
//      break;
//
//    case SUBREG:
//      if ((SUBREG_PROMOTED_VAR_P (x) || REG_POINTER (SUBREG_REG (x)))
//      && GET_MODE (SUBREG_REG (x)) == to_mode)
//    return SUBREG_REG (x);
//      break;
//
//    case LABEL_REF:
//      temp = gen_rtx_LABEL_REF (to_mode, label_ref_label (x));
//      LABEL_REF_NONLOCAL_P (temp) = LABEL_REF_NONLOCAL_P (x);
//      return temp;
//
//    case SYMBOL_REF:
//      temp = shallow_copy_rtx (x);
//      PUT_MODE (temp, to_mode);
//      return temp;
//
//    case CONST:
//      {
//    auto *last = no_emit ? nullptr : get_last_insn ();
//    temp = convert_memory_address_addr_space_1 (to_mode, XEXP (x, 0), as,
//                            true, no_emit);
//    if (temp && (no_emit || last == get_last_insn ()))
//      return gen_rtx_CONST (to_mode, temp);
//    return temp;
//      }
//
//    case PLUS:
//    case MULT:
//      /* For addition we can safely permute the conversion and addition
//     operation if one operand is a constant and converting the constant
//     does not change it or if one operand is a constant and we are
//     using a ptr_extend instruction  (POINTERS_EXTEND_UNSIGNED < 0).
//     We can always safely permute them if we are making the address
//     narrower. Inside a CONST RTL, this is safe for both pointers
//     zero or sign extended as pointers cannot wrap. */
//      if (GET_MODE_SIZE (to_mode) < GET_MODE_SIZE (from_mode)
//      || (GET_CODE (x) == PLUS
//          && CONST_INT_P (XEXP (x, 1))
//          && ((in_const && POINTERS_EXTEND_UNSIGNED != 0)
//          || XEXP (x, 1) == convert_memory_address_addr_space_1
//                     (to_mode, XEXP (x, 1), as, in_const,
//                      no_emit)
//                  || POINTERS_EXTEND_UNSIGNED < 0)))
//    {
//      temp = convert_memory_address_addr_space_1 (to_mode, XEXP (x, 0),
//                              as, in_const, no_emit);
//      return (temp ? gen_rtx_fmt_ee (GET_CODE (x), to_mode,
//                     temp, XEXP (x, 1))
//               : temp);
//    }
//      break;
//
//    case UNSPEC:
//      /* Assume that all UNSPECs in a constant address can be converted
//     operand-by-operand.  We could add a target hook if some targets
//     require different behavior.  */
//      if (in_const && GET_MODE (x) == from_mode)
//    {
//      unsigned int n = XVECLEN (x, 0);
//      rtvec v = gen_rtvec (n);
//      for (unsigned int i = 0; i < n; ++i)
//        {
//          rtx op = XVECEXP (x, 0, i);
//          if (GET_MODE (op) == from_mode)
//        op = convert_memory_address_addr_space_1 (to_mode, op, as,
//                              in_const, no_emit);
//          RTVEC_ELT (v, i) = op;
//        }
//      return gen_rtx_UNSPEC (to_mode, v, XINT (x, 1));
//    }
//      break;
//
//    default:
//      break;
//    }
//
//  if (no_emit)
//    return NULL_RTX;
//
//  return convert_modes (to_mode, from_mode,
//            x, POINTERS_EXTEND_UNSIGNED);
//#endif /* defined(POINTERS_EXTEND_UNSIGNED) */
}



//原型 rtl.h explow.cc
rtx  mtcs_explow_convert_memory_address_addr_space (MtcsExplow *self,scalar_int_mode to_mode, rtx x,addr_space_t as)
{
  return mtcs_explow_convert_memory_address_addr_space_1 (self,to_mode, x, as, false, false);
}


/* If X is a sum, return a new sum like X but lacking any constant terms.
   Add all the removed constant terms into *CONSTPTR.
   X itself is not altered.  The result != X if and only if
   it is not isomorphic to X.  */
//原型explow.h explow.cc
rtx mtcs_explow_eliminate_constant_term (MtcsExplow *self,rtx x, rtx *constptr)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

    rtx x0, x1;
    rtx tem;
    if (GET_CODE (x) != PLUS)
      return x;
    /* First handle constants appearing at this level explicitly.  */
    if (CONST_INT_P (XEXP (x, 1))
        && (tem = mtcs_simplify_rtx_binary_operation/*!simplify_binary_operation*/(mtcsSimplifyRtx,PLUS, GET_MODE (x), *constptr,XEXP (x, 1))) != 0
        && CONST_INT_P (tem)){
        *constptr = tem;
        return mtcs_explow_eliminate_constant_term (self,XEXP (x, 0), constptr);
    }

    tem = const0_rtx;
    x0 = mtcs_explow_eliminate_constant_term (self,XEXP (x, 0), &tem);
    x1 = mtcs_explow_eliminate_constant_term (self,XEXP (x, 1), &tem);
    if ((x1 != XEXP (x, 1) || x0 != XEXP (x, 0))
        && (tem = mtcs_simplify_rtx_binary_operation/*simplify_binary_operation*/ (mtcsSimplifyRtx,PLUS, GET_MODE (x),*constptr, tem)) != 0
        && CONST_INT_P (tem)){
        *constptr = tem;
        return gen_rtx_PLUS (GET_MODE (x), x0, x1);
    }

    return x;
}

/* Like copy_to_reg but always give the new register mode MODE
   in case X is a constant.  */
//原型 copy_to_mode_reg explow.h explow.cc
rtx mtcs_explow_copy_to_mode_reg ( MtcsExplow *self,machine_mode mode, rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);

  rtx temp = mtcs_emit_gen_reg_rtx (mtcsEmit,mode);
  /* If not an operand, must be an address with PLUS and MULT so
     do the computation.  */
  if (! mtcs_preds_general_operand/*!general_operand*/(mtcsPreds,x, VOIDmode))
    x =mtcs_expr_force_operand/*!force_operand*/ (mtcsExpr,x, temp);

  gcc_assert (GET_MODE (x) == mode || GET_MODE (x) == VOIDmode);
  if (x != temp)
      mtcs_expr_emit_move_insn (mtcsExpr,temp, x);
  return temp;
}

/* Return an rtx representing the register or memory location
   in which a scalar value of mode MODE was returned by a library call.  */
//原型 hard_libcall_value explow.h explow.cc
rtx mtcs_explow_hard_libcall_value (MtcsExplow *self,machine_mode mode, rtx fun)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  return target_calls_libcall_value/*!targetm.calls.libcall_value*/(mtcsMachine->calls,mode, fun);
}

/* Convert a mem ref into one with a valid memory address.
   Pass through anything else unchanged.  */
//原型 validize_mem explow.h explow.cc
rtx mtcs_explow_validize_mem (MtcsExplow *self,rtx ref)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  if (!MEM_P (ref))
    return ref;
  ref = mtcs_explow_use_anchored_address (self,ref);
  if (mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/(mtcsRecog,
          GET_MODE (ref), XEXP (ref, 0),mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,ref),ERROR_MARK))
    return ref;

  /* Don't alter REF itself, since that is probably a stack slot.  */
  return mtcs_rtl_replace_equiv_address/*!replace_equiv_address*/(mtcsRTL,ref, XEXP (ref, 0));
}

/* If X is a memory reference to a member of an object block, try rewriting
   it to use an anchor instead.  Return the new memory reference on success
   and the old one on failure.  */
//原型 rtx use_anchored_address (rtx x) explow.h explow.cc
rtx mtcs_explow_use_anchored_address (MtcsExplow *self,rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  rtx base;
  HOST_WIDE_INT offset;
  machine_mode mode;
  n_debug("mtcsexplow.c mtcs_explow_use_anchored_address 00 flag_section_anchors:%d\n",mtcsOptionsItem->x_flag_section_anchors);
  if (!mtcsOptionsItem->x_flag_section_anchors)
    return x;

  n_debug("mtcsexplow.c mtcs_explow_use_anchored_address 11 MEM_P (x):%d\n",MEM_P (x));

  if (!MEM_P (x))
    return x;

  /* Split the address into a base and offset.  */
  base = XEXP (x, 0);
  offset = 0;
  if (GET_CODE (base) == CONST && GET_CODE (XEXP (base, 0)) == PLUS
      && CONST_INT_P (XEXP (XEXP (base, 0), 1))){
      offset += INTVAL (XEXP (XEXP (base, 0), 1));
      base = XEXP (XEXP (base, 0), 0);
  }
  n_debug("mtcsexplow.c mtcs_explow_use_anchored_address 22 base mode:%d\n",GET_MODE(base));

  /* Check whether BASE is suitable for anchors.  */
  if (GET_CODE (base) != SYMBOL_REF
      || !SYMBOL_REF_HAS_BLOCK_INFO_P (base)
      || SYMBOL_REF_ANCHOR_P (base)
      || SYMBOL_REF_BLOCK (base) == NULL
      || !mtcsTarget->use_anchors_for_symbol_p/*!targetm.use_anchors_for_symbol_p*/ (mtcsTarget,base))
    return x;

  n_debug("mtcsexplow.c mtcs_explow_use_anchored_address 33 base mode:%d\n",GET_MODE(base));

  /* Decide where BASE is going to be.  */
  mtcs_asm_place_block_symbol/*!place_block_symbol*/(mtcsAsm,base);
  n_debug("mtcsexplow.c mtcs_explow_use_anchored_address 44 base mode:%d\n",GET_MODE(base));

  /* Get the anchor we need to use.  */
  offset += SYMBOL_REF_BLOCK_OFFSET (base);
  base = mtcs_asm_get_section_anchor/*!get_section_anchor*/(mtcsAsm,SYMBOL_REF_BLOCK (base), offset,
                 SYMBOL_REF_TLS_MODEL (base));

  /* Work out the offset from the anchor.  */
  offset -= SYMBOL_REF_BLOCK_OFFSET (base);

  /* If we're going to run a CSE pass, force the anchor into a register.
     We will then be able to reuse registers for several accesses, if the
     target costs say that that's worthwhile.  */
  mode = GET_MODE (base);
  n_debug("mtcsexplow.c mtcs_explow_use_anchored_address 55 base mode:%d\n",GET_MODE(base));

  if (!cse_not_expected)
    base = mtcs_explow_force_reg (self,mode, base);
  n_debug("mtcsexplow.c mtcs_explow_use_anchored_address 66 base mode:%d\n",GET_MODE(base));

  return mtcs_rtl_replace_equiv_address/*!replace_equiv_address*/(mtcsRTL,x,
          mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,mode, base, offset));
}


/* Adjust the stack pointer by ADJUST (an rtx for a number of bytes).
   This pops when ADJUST is positive.  ADJUST need not be constant.  */
//原型 adjust_stack explow.h explow.cc
void mtcs_explow_adjust_stack (MtcsExplow *self,rtx adjust)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  if (adjust == const0_rtx)
    return;
  /* We expect all variable sized adjustments to be multiple of
     PREFERRED_STACK_BOUNDARY.  */
  poly_int64 const_adjust;
  if (poly_int_rtx_p (adjust, &const_adjust))
      mtcsRtlData->expr.x_stack_pointer_delta/*!stack_pointer_delta*/-=const_adjust;
  adjust_stack_1 (self,adjust, false);
}

/* Like copy_to_reg but always give the new register mode Pmode
   in case X is a constant.  */
//原型 copy_addr_to_reg explow.h explow.cc
rtx mtcs_explow_copy_addr_to_reg (MtcsExplow *self,rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  return mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(self,mtcs_mode_get_Pmode(mtcsMode), x);
}

//原型 anti_adjust_stack exprlow.h exprlow.cc
void mtcs_explow_anti_adjust_stack (MtcsExplow *self,rtx adjust)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  if (adjust == const0_rtx)
    return;

  /* We expect all variable sized adjustments to be multiple of
     PREFERRED_STACK_BOUNDARY.  */
  poly_int64 const_adjust;
  if (poly_int_rtx_p (adjust, &const_adjust))
      mtcsRtlData->expr.x_stack_pointer_delta/*!stack_pointer_delta*/ += const_adjust;
  adjust_stack_1 (self,adjust, true);
}

/* If X is a memory ref, copy its contents to a new temp reg and return
   that reg.  Otherwise, return X.  */
//原型 force_not_mem explow.h explow.cc
rtx mtcs_explow_force_not_mem (MtcsExplow *self,rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  rtx temp;

  if (!MEM_P (x) || GET_MODE (x) ==mtcsMode->modes.M_BLKmode)
    return x;

  temp = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (x));

  if (MEM_POINTER (x))
    REG_POINTER (temp) = 1;

  mtcs_expr_emit_move_insn (mtcsExpr,temp, x);
  return temp;
}

//原型 explow.cc emit_stack_save 函数中的gen_move_insn
static void emit_stack_save_callback(MtcsTarget *mtcsTarget ,rtx r0,rtx r1)
{
    MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
    mtcs_expr_gen_move_insn/*!gen_move_insn*/(mtcsExpr,r0,r1);
}

/* Save the stack pointer for the purpose in SAVE_LEVEL.  PSAVE is a pointer
   to a previously-created save area.  If no save area has been allocated,
   this function will allocate one.  If a save area is specified, it
   must be of the proper mode.  */
//原型 emit_stack_save explow.h explow.cc
void mtcs_explow_emit_stack_save (MtcsExplow *self,enum save_level save_level, rtx *psave)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  rtx sa = *psave;
  /* The default is that we use a move insn and save in a Pmode object.  */
  rtx_insn *(*fcn) (MtcsTarget *,rtx, rtx) = emit_stack_save_callback/*!gen_move_insn*/;
  rtx_insn *(*fcnSave) (TargetRtx *,rtx, rtx) =NULL;// mtcsMachine->tmrtx->gen_save_stack_block/*!gen_save_stack_block*/;

  machine_mode mode =mtcs_mode_get_stack_savearea_mode/*!STACK_SAVEAREA_MODE*/(mtcsMode,save_level);
  n_debug("mtcsexplow.c mtcs_explow_emit_stack_save 00 mode:%d save_level:%d\n",mode,save_level);

  /* See if this machine has anything special to do for this kind of save.  */
  switch (save_level){
    case SAVE_BLOCK:
      if (target_rtx_have_save_stack_block/*!targetm.have_save_stack_block*/(mtcsMachine->tmrtx))
         fcnSave = mtcsMachine->tmrtx->gen_save_stack_block/*!targetm.gen_save_stack_block*/;
      break;
    case SAVE_FUNCTION:
        if (target_rtx_have_save_stack_function/*!targetm.have_save_stack_function*/(mtcsMachine->tmrtx))
           fcnSave = mtcsMachine->tmrtx->gen_save_stack_function/*!targetm.gen_save_stack_function*/;
      break;
    case SAVE_NONLOCAL:
        if (target_rtx_have_save_stack_nonlocal/*!targetm.have_save_stack_nonlocal*/(mtcsMachine->tmrtx))
           fcnSave = mtcsMachine->tmrtx->gen_save_stack_nonlocal/*!targetm.gen_save_stack_nonlocal*/;
      break;
    default:
      break;
  }

  /* If there is no save area and we have to allocate one, do so.  Otherwise
     verify the save area is the proper mode.  */

  if (sa == 0){
      if (mode != VOIDmode){
        n_debug("mtcsexplow.c mtcs_explow_emit_stack_save 11 mode:%d SAVE_NONLOCAL:%d\n",mode,SAVE_NONLOCAL);
          if (save_level == SAVE_NONLOCAL)
            *psave = sa = mtcs_func_assign_stack_local/*!assign_stack_local*/(mtcsFunc,mode, mtcs_mode_get_size(mtcsMode,mode), 0);
          else
            *psave = sa = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
      }
  }
  n_debug("mtcsexplow.c mtcs_explow_emit_stack_save 22 mode:%d sa:%p\n",mode,sa);

  mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
  if (sa != 0)
    sa = mtcs_explow_validize_mem/*!validize_mem*/(self,sa);
  if(fcnSave)
     mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,fcnSave(mtcsMachine->tmrtx,
           sa, mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)));
  else
     mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,fcn(mtcsTarget,
                sa, mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)));
}


/* Return an rtx representing the address of an area of memory dynamically
   pushed on the stack.

   Any required stack pointer alignment is preserved.

   SIZE is an rtx representing the size of the area.

   SIZE_ALIGN is the alignment (in bits) that we know SIZE has.  This
   parameter may be zero.  If so, a proper value will be extracted
   from SIZE if it is constant, otherwise BITS_PER_UNIT will be assumed.

   REQUIRED_ALIGN is the alignment (in bits) required for the region
   of memory.

   MAX_SIZE is an upper bound for SIZE, if SIZE is not constant, or -1 if
   no such upper bound is known.

   If CANNOT_ACCUMULATE is set to TRUE, the caller guarantees that the
   stack space allocated by the generated code cannot be added with itself
   in the course of the execution of the function.  It is always safe to
   pass FALSE here and the following criterion is sufficient in order to
   pass TRUE: every path in the CFG that starts at the allocation point and
   loops to it executes the associated deallocation code.  */
//原型 allocate_dynamic_stack_space explow.h explow.cc
rtx mtcs_explow_allocate_dynamic_stack_space(MtcsExplow *self,rtx size, unsigned size_align,
                  unsigned required_align,HOST_WIDE_INT max_size,bool cannot_accumulate)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsCodes *mtcsCodes=mtcs_target_get_codes(mtcsTarget);
  MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
  MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
  rtx  stackPointerRtx= mtcs_rtl_get_stack_pointer_rtx(mtcsRTL);

  HOST_WIDE_INT stack_usage_size = -1;
  rtx_code_label *final_label;
  rtx final_target, target;
  rtx addr = (mtcsFunc->virtuals_instantiated/*!virtuals_instantiated*/
          ? mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, stackPointerRtx/*!stack_pointer_rtx*/,
                  mtcs_func_get_stack_dynamic_offset/*!get_stack_dynamic_offset ()*/(mtcsFunc,current_function_decl))
          : mtcs_rtl_get_virtual_stack_dynamic_rtx/*!virtual_stack_dynamic_rtx*/(mtcsRTL));

  /* If we're asking for zero bytes, it doesn't matter what we point
     to since we can't dereference it.  But return a reasonable
     address anyway.  */
  if (size == const0_rtx)
    return addr;
  n_debug("mtcsexplow.c allocate_dynamic_stack_space 00 %d %d %d %d\n",size_align,required_align,max_size,cannot_accumulate);
  /* Otherwise, show we're calling alloca or equivalent.  */
  //mtcsFunc/*!cfun*/->calls_alloca = 1;
  cfun->calls_alloca = 1;//改变cfun

  /* If stack usage info is requested, look into the size we are passed.
     We need to do so this early to avoid the obfuscation that may be
     introduced later by the various alignment operations.  */
  if (mtcsOptionsItem/*!flag_stack_usage_info*/->x_flag_stack_usage_info){
      if (CONST_INT_P (size))
          stack_usage_size = INTVAL (size);
      else if (REG_P (size)){
          /* Look into the last emitted insn and see if we can deduce
             something for the register.  */
          rtx_insn *insn;
          rtx set, note;
          insn = mtcs_rtl_data_get_last_insn/*!get_last_insn*/ (mtcsRtlData);
          if ((set = single_set (insn)) && rtx_equal_p (SET_DEST (set), size)){
              if (CONST_INT_P (SET_SRC (set)))
                  stack_usage_size = INTVAL (SET_SRC (set));
              else if ((note = find_reg_equal_equiv_note (insn)) && CONST_INT_P (XEXP (note, 0)))
                  stack_usage_size = INTVAL (XEXP (note, 0));
          }
      }

      /* If the size is not constant, try the maximum size.  */
      if (stack_usage_size < 0)
          stack_usage_size = max_size;

      /* If the size is still not constant, we can't say anything.  */
      if (stack_usage_size < 0){
          //mtcsFunc->su->has_unbounded_dynamic_stack_size/*!current_function_has_unbounded_dynamic_stack_size*/ = 1;
          current_function_has_unbounded_dynamic_stack_size = 1;//改变cfun
          stack_usage_size = 0;
      }
  }
  n_debug("mtcsexplow.c allocate_dynamic_stack_space 11 stack_usage_size:%d \n",  stack_usage_size);

  mtcs_explow_get_dynamic_stack_size/*!get_dynamic_stack_size*/(self,&size, size_align, required_align, &stack_usage_size);

  n_debug("mtcsexplow.c allocate_dynamic_stack_space 11aa stack_usage_size:%d \n",stack_usage_size);

  target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,pMode);

  /* The size is supposed to be fully adjusted at this point so record it
     if stack usage info is requested.  */
  if (mtcsOptionsItem/*!flag_stack_usage_info*/->x_flag_stack_usage_info){
      //mtcsFunc->su->dynamic_stack_size/*!current_function_dynamic_stack_size*/ += stack_usage_size;
      current_function_dynamic_stack_size += stack_usage_size;//改变cfun

      /* ??? This is gross but the only safe stance in the absence
     of stack usage oriented flow analysis.  */
      if (!cannot_accumulate)
         // mtcsFunc->su->has_unbounded_dynamic_stack_size/*!current_function_has_unbounded_dynamic_stack_size*/ = 1;
          current_function_has_unbounded_dynamic_stack_size = 1;//改变cfun
  }

  n_debug("mtcsexplow.c allocate_dynamic_stack_space 22 stack_usage_size:%d \n",stack_usage_size);
  mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);

  final_label = NULL;
  final_target = NULL_RTX;

  /* If we are splitting the stack, we need to ask the backend whether
     there is enough room on the current stack.  If there isn't, or if
     the backend doesn't know how to tell is, then we need to call a
     function to allocate memory in some other way.  This memory will
     be released when we release the current stack segment.  The
     effect is that stack allocation becomes less efficient, but at
     least it doesn't cause a stack overflow.  */
  if (mtcsOptionsItem/*!flag_split_stack*/->x_flag_split_stack){
     n_debug("mtcsexplow.c allocate_dynamic_stack_space 33\n");
      rtx_code_label *available_label;
      rtx ask, space, func;
      available_label = NULL;
      if (target_rtx_have_split_stack_space_check/*!targetm.have_split_stack_space_check*/(mtcsMachine->tmrtx)){
          available_label =mtcs_rtl_gen_label_rtx(mtcsRTL);
          /* This instruction will branch to AVAILABLE_LABEL if there
             are SIZE bytes available on the stack.  */
          mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,target_rtx_gen_split_stack_space_check/*!targetm.gen_split_stack_space_check*/
                  (mtcsMachine->tmrtx,size, available_label));
      }
      /* The __morestack_allocate_stack_space function will allocate
     memory using malloc.  If the alignment of the memory returned
     by malloc does not meet REQUIRED_ALIGN, we increase SIZE to
     make sure we allocate enough space.  */
      if (MALLOC_ABI_ALIGNMENT >= required_align)
          ask = size;
      else
        ask =mtcs_optabs_expand_binop(mtcsOptabs,pMode, add_optab, size,
                mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,required_align / BITS_PER_UNIT - 1,
                          pMode),NULL_RTX, 1, OPTAB_LIB_WIDEN);

      func = mtcs_libfuncs_init_one_libfunc/*!init_one_libfunc*/(mtcsLibfuncs,"__morestack_allocate_stack_space");
      space =mtcs_calls_emit_library_call_value/*!emit_library_call_value*/(mtcsCalls,func, target, LCT_NORMAL, pMode,ask, pMode);
      if (available_label == NULL_RTX)
          return space;
      final_target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,pMode);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/ (mtcsExpr,final_target, space);
      final_label =mtcs_rtl_gen_label_rtx(mtcsRTL);
      mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,final_label);
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,available_label);
  }

 /* We ought to be called always on the toplevel and stack ought to be aligned
    properly.  */
  gcc_assert (multiple_p (mtcsRtlData->expr.x_stack_pointer_delta/*!stack_pointer_delta*/,
        mtcs_func_get_preferred_stack_boundary/*!PREFERRED_STACK_BOUNDARY*/(mtcsFunc)/ BITS_PER_UNIT));

  /* If needed, check that we have the required amount of stack.  Take into
     account what has already been checked.  */
  if (mtcs_func_get_stack_check_moving_sp/*!STACK_CHECK_MOVING_SP*/(mtcsFunc)){
    //;
     n_debug("mtcsexplow.c allocate_dynamic_stack_space 44\n");

  }else if (mtcsOptionsItem/*!flag_stack_check*/->x_flag_stack_check == GENERIC_STACK_CHECK){
     n_debug("mtcsexplow.c allocate_dynamic_stack_space 55\n");

    mtcs_explow_probe_stack_range/*!probe_stack_range*/(self,
            mtcs_func_get_stack_old_check_protect/*!STACK_OLD_CHECK_PROTECT*/(mtcsFunc) +
            mtcs_func_get_stack_check_max_frame_size/*!STACK_CHECK_MAX_FRAME_SIZE*/(mtcsFunc),size);
  }else if (mtcsOptionsItem/*!flag_stack_check*/->x_flag_stack_check == STATIC_BUILTIN_STACK_CHECK){
     n_debug("mtcsexplow.c allocate_dynamic_stack_space 66\n");

      mtcs_explow_probe_stack_range/*!probe_stack_range*/(self,
            mtcs_explow_get_stack_check_protect/*!get_stack_check_protect*/(self), size);
  }

  /* Don't let anti_adjust_stack emit notes.  */
  self->suppress_reg_args_size = true;

  /* Perform the required allocation from the stack.  Some systems do
     this differently than simply incrementing/decrementing from the
     stack pointer, such as acquiring the space by calling malloc().  */
  if (target_rtx_have_allocate_stack/*!targetm.have_allocate_stack*/(mtcsMachine->tmrtx)){
     n_debug("mtcsexplow.c allocate_dynamic_stack_space 77\n");

      class expand_operand ops[2];
      /* We don't have to check against the predicate for operand 0 since
     TARGET is known to be a pseudo of the proper mode, which must
     be valid for the operand.  */
      create_fixed_operand (&ops[0], target);
      create_convert_operand_to (&ops[1], size, mtcs_mode_get_stack_size_mode/*!STACK_SIZE_MODE*/(mtcsMode), true);
      mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,
              mtcs_codes_get_code_for_allocate_stack/*!targetm.code_for_allocate_stack*/(mtcsCodes), 2, ops);
  }else{
     n_debug("mtcsexplow.c allocate_dynamic_stack_space 88\n");

      poly_int64 saved_stack_pointer_delta;
      if (!mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc))
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,addr, target));
      rtx stackLimitRtx=mtcs_rtl_get_stack_limit_rtx(mtcsRTL);
      /* Check stack bounds if necessary.  */
      if (mtcsRtlData/*!crtl*/->limit_stack){
          rtx available;
          rtx_code_label *space_available =mtcs_rtl_gen_label_rtx(mtcsRTL);
          if (mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc))
            available = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,pMode, sub_optab,
                    stackPointerRtx/*!stack_pointer_rtx*/, stackLimitRtx/*!stack_limit_rtx*/,
                          NULL_RTX, 1, OPTAB_WIDEN);
          else
            available = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,pMode, sub_optab,
                          stackLimitRtx/*!stack_limit_rtx*/, stackPointerRtx/*!stack_pointer_rtx*/,
                          NULL_RTX, 1, OPTAB_WIDEN);

          mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,available, size, GEU, NULL_RTX, pMode, 1,
                       space_available);
          if (target_rtx_have_trap/*!targetm.have_trap*/(mtcsMachine->tmrtx))
              mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,target_rtx_gen_trap/*!targetm.gen_trap*/(mtcsMachine->tmrtx));
          else
            error ("stack limits not supported on this target");
          mtcs_emit_emit_barrier/*!emit_barrier*/(mtcsEmit);
          mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,space_available);
      }

      saved_stack_pointer_delta = stack_pointer_delta;

      /* If stack checking or stack clash protection is requested,
     then probe the stack while allocating space from it.  */
      if (mtcsOptionsItem/*!flag_stack_check*/->x_flag_stack_check
            && mtcs_func_get_stack_check_moving_sp/*!STACK_CHECK_MOVING_SP*/(mtcsFunc))
          anti_adjust_stack_and_probe (size, false);
      else if (mtcsOptionsItem->x_flag_stack_clash_protection)
          anti_adjust_stack_and_probe_stack_clash (size);
      else
          mtcs_explow_anti_adjust_stack/*!anti_adjust_stack*/(self,size);

      /* Even if size is constant, don't modify stack_pointer_delta.
     The constant size alloca should preserve
     crtl->preferred_stack_boundary alignment.  */
      mtcsRtlData->expr.x_stack_pointer_delta/*!stack_pointer_delta*/ = saved_stack_pointer_delta;

      if (mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc))
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, force_operand (addr, target));
  }

  self->suppress_reg_args_size = false;
  n_debug("mtcsexplow.c allocate_dynamic_stack_space 99\n");

  /* Finish up the split stack handling.  */
  if (final_label != NULL_RTX){
      gcc_assert (mtcsOptionsItem->x_flag_split_stack);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,final_target, target);
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,final_label);
      target = final_target;
  }

  target = mtcs_explow_align_dynamic_address/*!align_dynamic_address*/(self,target, required_align);

  /* Now that we've committed to a return value, mark its alignment.  */
  mtcs_rtl_mark_reg_pointer/*!mark_reg_pointer*/(mtcsRTL,target, required_align);

  /* Record the new stack level.  */
  mtcs_explow_record_new_stack_level/*!record_new_stack_level*/(self);

  return target;
}


/* Return an rtx through *PSIZE, representing the size of an area of memory to
   be dynamically pushed on the stack.

   *PSIZE is an rtx representing the size of the area.

   SIZE_ALIGN is the alignment (in bits) that we know SIZE has.  This
   parameter may be zero.  If so, a proper value will be extracted
   from SIZE if it is constant, otherwise BITS_PER_UNIT will be assumed.

   REQUIRED_ALIGN is the alignment (in bits) required for the region
   of memory.

   If PSTACK_USAGE_SIZE is not NULL it points to a value that is increased for
   the additional size returned.  */
//原型 get_dynamic_stack_size explow.h explow.cc
void mtcs_explow_get_dynamic_stack_size (MtcsExplow *self,rtx *psize, unsigned size_align,
            unsigned required_align,HOST_WIDE_INT *pstack_usage_size)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem = mtcsOptions->global_options;

  machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);

  rtx size = *psize;

  /* Ensure the size is in the proper mode.  */
  if (GET_MODE (size) != VOIDmode && GET_MODE (size) != pMode)
    size = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,pMode, size, 1);

  if (CONST_INT_P (size)){
      unsigned HOST_WIDE_INT lsb;
      lsb = INTVAL (size);
      lsb &= -lsb;

      /* Watch out for overflow truncating to "unsigned".  */
      if (lsb > UINT_MAX / BITS_PER_UNIT)
          size_align = 1u << (HOST_BITS_PER_INT - 1);
      else
          size_align = (unsigned)lsb * BITS_PER_UNIT;
  }else if (size_align < BITS_PER_UNIT)
    size_align = BITS_PER_UNIT;

  /* We can't attempt to minimize alignment necessary, because we don't
     know the final value of preferred_stack_boundary yet while executing
     this code.  */
  if (mtcsRtlData/*!crtl*/->preferred_stack_boundary <mtcs_func_get_preferred_stack_boundary/*!PREFERRED_STACK_BOUNDARY*/(mtcsFunc))
      mtcsRtlData/*!crtl*/->preferred_stack_boundary = mtcs_func_get_preferred_stack_boundary/*!PREFERRED_STACK_BOUNDARY*/(mtcsFunc);

  /* We will need to ensure that the address we return is aligned to
     REQUIRED_ALIGN.  At this point in the compilation, we don't always
     know the final value of the STACK_DYNAMIC_OFFSET used in function.cc
     (it might depend on the size of the outgoing parameter lists, for
     example), so we must preventively align the value.  We leave space
     in SIZE for the hole that might result from the alignment operation.  */

  unsigned known_align =mtcs_rtl_data_get_regno_pointer_align/*!REGNO_POINTER_ALIGN*/(mtcsRtlData,
          mtcs_reg_get_virtual_stack_dynamic_regnum/*!VIRTUAL_STACK_DYNAMIC_REGNUM*/(mtcsReg));
  if (known_align == 0)
    known_align = BITS_PER_UNIT;
  if (required_align > known_align){
      unsigned extra = (required_align - known_align) / BITS_PER_UNIT;
      size = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, size, extra);
      size = mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,size, NULL_RTX);
      if (size_align > known_align)
          size_align = known_align;

      if (flag_stack_usage_info && pstack_usage_size)
          *pstack_usage_size += extra;
  }

  /* Round the size to a multiple of the required stack alignment.
     Since the stack is presumed to be rounded before this allocation,
     this will maintain the required alignment.

     If the stack grows downward, we could save an insn by subtracting
     SIZE from the stack pointer and then aligning the stack pointer.
     The problem with this is that the stack pointer may be unaligned
     between the execution of the subtraction and alignment insns and
     some machines do not allow this.  Even on those that do, some
     signal handlers malfunction if a signal should occur between those
     insns.  Since this is an extremely rare event, we have no reliable
     way of knowing which systems have this problem.  So we avoid even
     momentarily mis-aligning the stack.  */
  if (size_align % mtcs_func_get_max_support_stack_alignment/*!MAX_SUPPORTED_STACK_ALIGNMENT*/(mtcsFunc) != 0){
      size = round_push(self,size);
      if (mtcsOptionsItem->x_flag_stack_usage_info/*!flag_stack_usage_info*/ && pstack_usage_size){
          int align = mtcsRtlData/*!crtl*/->preferred_stack_boundary / BITS_PER_UNIT;
          *pstack_usage_size =(*pstack_usage_size + align - 1) / align * align;
      }
  }

  *psize = size;
}

///mtcs_func_get_stack_grows_downward STACK_GROWS_DOWNWARD
#define STACK_GROW_OP(MTCSFUNC) \
   mtcs_func_get_stack_grows_downward(MTCSFUNC)?MINUS:PLUS

#define STACK_GROW_OFF(MTCSFUNC,off) \
   mtcs_func_get_stack_grows_downward(MTCSFUNC)?-(off):(off)

#define STACK_GROW_OPTAB(MTCSFUNC) \
   mtcs_func_get_stack_grows_downward(MTCSFUNC)?sub_optab:add_optab

#define PROBE_INTERVAL (1 << mtcs_func_get_stack_check_probe_interval_exp/*!STACK_CHECK_PROBE_INTERVAL_EXP*/(mtcsFunc))

//#if STACK_GROWS_DOWNWARD
//#define STACK_GROW_OP MINUS
//#define STACK_GROW_OPTAB sub_optab
//#define STACK_GROW_OFF(off) -(off)
//#else
//#define STACK_GROW_OP PLUS
//#define STACK_GROW_OPTAB add_optab
//#define STACK_GROW_OFF(off) (off)
//#endif
//原型 probe_stack_range explow.h explow.cc
void mtcs_explow_probe_stack_range (MtcsExplow *self,HOST_WIDE_INT first, rtx size)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);

  /* First ensure SIZE is Pmode.  */
  if (GET_MODE (size) != VOIDmode && GET_MODE (size) != pMode)
    size = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,pMode, size, 1);

  /* Next see if we have a function to check the stack.  */
  if (self->stack_check_libfunc){
      rtx addr = mtcs_explow_memory_address/*!memory_address*/(self,pMode,
                 gen_rtx_fmt_ee (STACK_GROW_OP(mtcsFunc), pMode,
                             mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL),
                             mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode,size, first)));
      mtcs_calls_emit_library_call/*!emit_library_call*/(mtcsCalls,self->stack_check_libfunc, LCT_THROW, VOIDmode,addr, pMode);
  }
  /* Next see if we have an insn to check the stack.  */
  else if (target_rtx_have_check_stack/*!targetm.have_check_stack*/(mtcsMachine->tmrtx)){
      class expand_operand ops[1];
      rtx addr = mtcs_explow_memory_address/*!memory_address*/(self,pMode,
                 gen_rtx_fmt_ee (STACK_GROW_OP(mtcsFunc), pMode,
                         mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL),
                             mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode,size, first)));
      bool success;
      create_input_operand (&ops[0], addr, pMode);
      success = mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(mtcsOptabs,
            mtcsMachine->tmrtx->code_for_check_stack/*!targetm.code_for_check_stack*/, 1, ops);
      gcc_assert (success);
  }

  /* Otherwise we have to generate explicit probes.  If we have a constant
     small number of them to generate, that's the easy case.  */
  else if (CONST_INT_P (size) && INTVAL (size) < 7 * PROBE_INTERVAL){
      HOST_WIDE_INT isize = INTVAL (size), i;
      rtx addr;

      /* Probe at FIRST + N * PROBE_INTERVAL for values of N from 1 until
     it exceeds SIZE.  If only one probe is needed, this will not
     generate any code.  Then probe at FIRST + SIZE.  */
      for (i = PROBE_INTERVAL; i < isize; i += PROBE_INTERVAL){
          addr = mtcs_explow_memory_address/*!memory_address*/(self,pMode,
                     mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode,  mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL),
                            STACK_GROW_OFF(mtcsFunc,first + i)));
          mtcs_explow_emit_stack_probe/*!emit_stack_probe*/(self,addr);
      }

      addr = mtcs_explow_memory_address/*!memory_address*/(self,pMode,
                 mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode,  mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL),
                        STACK_GROW_OFF(mtcsFunc,first + isize)));
      mtcs_explow_emit_stack_probe/*!emit_stack_probe*/(self,addr);
  }

  /* In the variable case, do the same as above, but in a loop.  Note that we
     must be extra careful with variables wrapping around because we might be
     at the very top (or the very bottom) of the address space and we have to
     be able to handle this case properly; in particular, we use an equality
     test for the loop condition.  */
  else{
      rtx rounded_size, rounded_size_op, test_addr, last_addr, temp;
      rtx_code_label *loop_lab =mtcs_rtl_gen_label_rtx(mtcsRTL);
      rtx_code_label *end_lab =mtcs_rtl_gen_label_rtx(mtcsRTL);

      /* Step 1: round SIZE to the previous multiple of the interval.  */

      /* ROUNDED_SIZE = SIZE & -PROBE_INTERVAL  */
      rounded_size = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,AND, pMode, size,
              mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,-PROBE_INTERVAL, pMode));
      rounded_size_op = mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,rounded_size, NULL_RTX);


      /* Step 2: compute initial and final value of the loop counter.  */

      /* TEST_ADDR = SP + FIRST.  */
      test_addr = mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,gen_rtx_fmt_ee (STACK_GROW_OP(mtcsFunc), pMode,
              mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL),
              mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,first, pMode)),NULL_RTX);

      /* LAST_ADDR = SP + FIRST + ROUNDED_SIZE.  */
      last_addr = mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,gen_rtx_fmt_ee (STACK_GROW_OP(mtcsFunc), pMode,
                         test_addr,rounded_size_op), NULL_RTX);


      /* Step 3: the loop

     while (TEST_ADDR != LAST_ADDR)
       {
         TEST_ADDR = TEST_ADDR + PROBE_INTERVAL
         probe at TEST_ADDR
       }

     probes at FIRST + N * PROBE_INTERVAL for values of N from 1
     until it is equal to ROUNDED_SIZE.  */

      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,loop_lab);

      /* Jump to END_LAB if TEST_ADDR == LAST_ADDR.  */
      mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,test_addr, last_addr, EQ, NULL_RTX, pMode, 1,
                   end_lab);

      /* TEST_ADDR = TEST_ADDR + PROBE_INTERVAL.  */
      temp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,pMode, STACK_GROW_OPTAB(mtcsFunc), test_addr,
              mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,PROBE_INTERVAL, pMode), test_addr,
               1, OPTAB_WIDEN);

      /* There is no guarantee that expand_binop constructs its result
     in TEST_ADDR.  So copy into TEST_ADDR if necessary.  */
      if (temp != test_addr)
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,test_addr, temp);

      /* Probe at TEST_ADDR.  */
      mtcs_explow_emit_stack_probe/*!emit_stack_probe*/(self,test_addr);

      mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,loop_lab);

      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,end_lab);


      /* Step 4: probe at FIRST + SIZE if we cannot assert at compile-time
     that SIZE is equal to ROUNDED_SIZE.  */

      /* TEMP = SIZE - ROUNDED_SIZE.  */
      temp = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MINUS, pMode, size, rounded_size);
      if (temp != const0_rtx){
          rtx addr;

          if (CONST_INT_P (temp)){
              /* Use [base + disp} addressing mode if supported.  */
              HOST_WIDE_INT offset = INTVAL (temp);
              addr = mtcs_explow_memory_address/*!memory_address*/(self,pMode,
                         mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, last_addr,
                                STACK_GROW_OFF(mtcsFunc,offset)));
          }else{
              /* Manual CSE if the difference is not known at compile-time.  */
              temp = gen_rtx_MINUS (pMode, size, rounded_size_op);
              addr = mtcs_explow_memory_address/*!memory_address*/(self,pMode,
                         gen_rtx_fmt_ee (STACK_GROW_OP(mtcsFunc), pMode,
                                 last_addr, temp));
          }

          mtcs_explow_emit_stack_probe/*!emit_stack_probe*/(self,addr);
      }
  }

  /* Make sure nothing is scheduled before we are done.  */
  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_blockage ());
}

/* Emit one stack probe at ADDRESS, an address within the stack.  */
//原型 emit_stack_probe explow.h explow.cc
void mtcs_explow_emit_stack_probe (MtcsExplow *self,rtx address)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  if (target_rtx_have_probe_stack_address/*!targetm.have_probe_stack_address*/(mtcsMachine->tmrtx)){
      class expand_operand ops[1];
      insn_code icode = mtcsMachine->tmrtx->code_for_probe_stack_address/*!targetm.code_for_probe_stack_address*/;
      create_address_operand (ops, address);
      mtcs_optabs_maybe_legitimize_operands/*!maybe_legitimize_operands*/(mtcsOptabs,icode, 0, 1, ops);
      mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,icode, 1, ops);
  }else{
      rtx memref = gen_rtx_MEM (word_mode, address);

      MEM_VOLATILE_P (memref) = 1;
      memref = mtcs_explow_validize_mem/*!validize_mem*/(self,memref);

      /* See if we have an insn to probe the stack.  */
      if (target_rtx_have_probe_stack/*!targetm.have_probe_stack*/(mtcsMachine->tmrtx))
          mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,target_rtx_gen_probe_stack/*!targetm.gen_probe_stack*/(mtcsMachine->tmrtx,memref));
      else
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,memref, const0_rtx);
  }
}

/* Return the number of bytes to "protect" on the stack for -fstack-check.

   "protect" in the context of -fstack-check means how many bytes we need
   to always ensure are available on the stack; as a consequence, this is
   also how many bytes are first skipped when probing the stack.

   On some targets we want to reuse the -fstack-check prologue support
   to give a degree of protection against stack clashing style attacks.

   In that scenario we do not want to skip bytes before probing as that
   would render the stack clash protections useless.

   So we never use STACK_CHECK_PROTECT directly.  Instead we indirectly
   use it through this helper, which allows to provide different values
   for -fstack-check and -fstack-clash-protection.  */
//原型 get_stack_check_protect rtl.h explow.cc
HOST_WIDE_INT mtcs_explow_get_stack_check_protect (MtcsExplow *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  if (flag_stack_clash_protection)
    return 0;
  return  mtcs_func_get_stack_check_protect/*!STACK_CHECK_PROTECT*/(mtcsFunc);
}


/* Return an rtx doing runtime alignment to REQUIRED_ALIGN on TARGET.  */
//原型 align_dynamic_address explow.h explow.cc
rtx mtcs_explow_align_dynamic_address (MtcsExplow *self,rtx target, unsigned required_align)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);

  machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
  if (required_align == BITS_PER_UNIT)
    return target;

  /* CEIL_DIV_EXPR needs to worry about the addition overflowing,
     but we know it can't.  So add ourselves and then do
     TRUNC_DIV_EXPR.  */
  target = mtcs_optabs_expand_binop(mtcsOptabs,pMode, add_optab, target,
          mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,required_align / BITS_PER_UNIT - 1,
                     pMode),
             NULL_RTX, 1, OPTAB_LIB_WIDEN);
  target = mtcs_expmed_expand_divmod /*!expand_divmod*/(mtcsExpmed,0, TRUNC_DIV_EXPR, pMode, target,
          mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,required_align / BITS_PER_UNIT,
                      pMode),
              NULL_RTX, 1);
  target = mtcs_expmed_expand_mult/*!expand_mult*/(mtcsExpmed,pMode, target,
          mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,required_align / BITS_PER_UNIT,
                    pMode),
            NULL_RTX, 1);

  return target;
}

/* Invoke emit_stack_save on the nonlocal_goto_save_area for the current
   function.  This should be called whenever we allocate or deallocate
   dynamic stack space.  */
//原型 update_nonlocal_goto_save_area explow.h explow.cc

void mtcs_explow_update_nonlocal_goto_save_area (MtcsExplow *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

  tree t_save;
  rtx r_save;

  /* The nonlocal_goto_save_area object is an array of N pointers.  The
     first one is used for the frame pointer save; the rest are sized by
     STACK_SAVEAREA_MODE.  Create a reference to array index 1, the first
     of the stack save area slots.  */
  t_save = build4 (ARRAY_REF,
           TREE_TYPE (TREE_TYPE (cfun->nonlocal_goto_save_area)),
           cfun->nonlocal_goto_save_area,
           integer_one_node, NULL_TREE, NULL_TREE);
  r_save = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,t_save, NULL_RTX, VOIDmode, EXPAND_WRITE);

  mtcs_explow_emit_stack_save/*!emit_stack_save*/(self,SAVE_NONLOCAL, &r_save);
}

/* Record a new stack level for the current function.  This should be called
   whenever we allocate or deallocate dynamic stack space.  */
//原型 record_new_stack_level explow.h explow.cc
void mtcs_explow_record_new_stack_level (MtcsExplow *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsExcept   *mtcsExcept=mtcs_target_get_except(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  /* Record the new stack level for nonlocal gotos.  */
  //if (mtcsFunc/*!cfun*/->nonlocal_goto_save_area)
  if (cfun->nonlocal_goto_save_area)
      mtcs_explow_update_nonlocal_goto_save_area/*!update_nonlocal_goto_save_area*/(self);

  /* Record the new stack level for SJLJ exceptions.  */
  if (target_common_except_unwind_info/*!targetm_common.except_unwind_info*/(mtcsMachine->common,mtcsOptions->global_options) == UI_SJLJ)
      mtcs_except_update_sjlj_context/*!update_sjlj_context*/(mtcsExcept);
}

/* Restore the stack pointer for the purpose in SAVE_LEVEL.  SA is the save
   area made by emit_stack_save.  If it is zero, we have nothing to do.  */
//原型 emit_stack_restore explow.h explow.cc
void mtcs_explow_emit_stack_restore (MtcsExplow *self,enum save_level save_level, rtx sa)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  /* The default is that we use a move insn.  */
  /*!rtx_insn *(*fcn) (rtx, rtx) = gen_move_insn;*/
  void *voidSelf=(void*)mtcsExpr;
  rtx_insn *(*fcn) (MtcsExpr *,rtx, rtx) = mtcs_expr_gen_move_insn;
  rtx_insn *(*fcnRestore) (TargetRtx*,rtx, rtx) = NULL;

  /* If stack_realign_drap, the x86 backend emits a prologue that aligns both
     STACK_POINTER and HARD_FRAME_POINTER.
     If stack_realign_fp, the x86 backend emits a prologue that aligns only
     STACK_POINTER. This renders the HARD_FRAME_POINTER unusable for accessing
     aligned variables, which is reflected in ix86_can_eliminate.
     We normally still have the realigned STACK_POINTER that we can use.
     But if there is a stack restore still present at reload, it can trigger
     mark_not_eliminable for the STACK_POINTER, leaving no way to eliminate
     FRAME_POINTER into a hard reg.
     To prevent this situation, we force need_drap if we emit a stack
     restore.  */
  if (mtcs_func_is_support_stack_alignment/*!SUPPORTS_STACK_ALIGNMENT*/(mtcsFunc))
      mtcsRtlData/*!crtl*/->need_drap = true;

  n_debug("mtcsexplow.c mtcs_explow_emit_stack_restore 00 %d %d sa:%p level:%d\n",
        mtcs_func_is_support_stack_alignment/*!SUPPORTS_STACK_ALIGNMENT*/(mtcsFunc),
        mtcsRtlData/*!crtl*/->need_drap,sa,save_level );

  /* See if this machine has anything special to do for this kind of save.  */
  switch (save_level){
    case SAVE_BLOCK:
      if (target_rtx_have_restore_stack_block/*!targetm.have_restore_stack_block*/(mtcsMachine->tmrtx)){
         fcnRestore =mtcsMachine->tmrtx->gen_restore_stack_block/*!targetm.gen_restore_stack_block*/;
      }
      break;
    case SAVE_FUNCTION:
      if (target_rtx_have_restore_stack_function/*!targetm.have_restore_stack_function*/(mtcsMachine->tmrtx)){
         fcnRestore = mtcsMachine->tmrtx->gen_restore_stack_function/*!targetm.gen_restore_stack_function*/ ;
      }
      break;
    case SAVE_NONLOCAL:
      if (target_rtx_have_restore_stack_nonlocal/*!targetm.have_restore_stack_nonlocal*/(mtcsMachine->tmrtx)){
         fcnRestore = mtcsMachine->tmrtx->gen_restore_stack_nonlocal/*!targetm.gen_restore_stack_nonlocal*/ ;
      }
      break;
    default:
      break;
  }

  if (sa != 0){
      sa = mtcs_explow_validize_mem/*!validize_mem*/(self,sa);
      n_debug("mtcsexplow.c mtcs_explow_emit_stack_restore 11 %p\n",sa);

      /* These clobbers prevent the scheduler from moving
     references to variable arrays below the code
     that deletes (pops) the arrays.  */
      mtcs_emit_emit_clobber/*!emit_clobber*/(mtcsEmit,gen_rtx_MEM (mtcsMode->modes.M_BLKmode, gen_rtx_SCRATCH (VOIDmode)));
      mtcs_emit_emit_clobber/*!emit_clobber*/(mtcsEmit,gen_rtx_MEM (mtcsMode->modes.M_BLKmode,
              mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)));
  }
  n_debug("mtcsexplow.c mtcs_explow_emit_stack_restore 22 %p %p\n",sa,mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL));

  mtcs_dojump_discard_pending_stack_adjust(mtcsDojump);
  if(fcnRestore)
     mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,fcnRestore (mtcsMachine->tmrtx,
           mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL), sa));
  else
     mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,fcn (mtcsExpr,
           mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL), sa));

}


/* Adjust the stack pointer by minus SIZE (an rtx for a number of bytes)
   while probing it.  This pushes when SIZE is positive.  SIZE need not
   be constant.  If ADJUST_BACK is true, adjust back the stack pointer
   by plus SIZE at the end.  */
//原型 anti_adjust_stack_and_probe explow.h explow.cc
void mtcs_explow_anti_adjust_stack_and_probe (MtcsExplow *self,rtx size, bool adjust_back)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

  /* We skip the probe for the first interval + a small dope of 4 words and
     probe that many bytes past the specified size to maintain a protection
     area at the botton of the stack.  */
  const int dope = 4 * UNITS_PER_WORD;
  machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
  rtx  stackPointerRtx= mtcs_rtl_get_stack_pointer_rtx(mtcsRTL);

  /* First ensure SIZE is Pmode.  */
  if (GET_MODE (size) != VOIDmode && GET_MODE (size) != pMode)
    size = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,pMode, size, 1);
  /* If we have a constant small number of probes to generate, that's the
     easy case.  */
  if (CONST_INT_P (size) && INTVAL (size) < 7 * PROBE_INTERVAL){
      HOST_WIDE_INT isize = INTVAL (size), i;
      bool first_probe = true;
      /* Adjust SP and probe at PROBE_INTERVAL + N * PROBE_INTERVAL for
     values of N from 1 until it exceeds SIZE.  If only one probe is
     needed, this will not generate any code.  Then adjust and probe
     to PROBE_INTERVAL + SIZE.  */
      for (i = PROBE_INTERVAL; i < isize; i += PROBE_INTERVAL){
          if (first_probe){
              mtcs_explow_anti_adjust_stack/*!anti_adjust_stack*/(self,GEN_INT (2 * PROBE_INTERVAL + dope));
              first_probe = false;
          }else
              mtcs_explow_anti_adjust_stack/*!anti_adjust_stack*/(self,GEN_INT (PROBE_INTERVAL));
          mtcs_explow_emit_stack_probe/*!emit_stack_probe*/(self,
                  mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL));
      }

      if (first_probe)
          mtcs_explow_anti_adjust_stack/*!anti_adjust_stack*/(self,
                  mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, size, PROBE_INTERVAL + dope));
      else
          mtcs_explow_anti_adjust_stack/*!anti_adjust_stack*/(self,
                  mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, size, PROBE_INTERVAL - i));
      mtcs_explow_emit_stack_probe/*!emit_stack_probe*/(self,
                      mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL));
  }

  /* In the variable case, do the same as above, but in a loop.  Note that we
     must be extra careful with variables wrapping around because we might be
     at the very top (or the very bottom) of the address space and we have to
     be able to handle this case properly; in particular, we use an equality
     test for the loop condition.  */
  else{
      rtx rounded_size, rounded_size_op, last_addr, temp;
      rtx_code_label *loop_lab =mtcs_rtl_gen_label_rtx(mtcsRTL);
      rtx_code_label *end_lab =mtcs_rtl_gen_label_rtx(mtcsRTL);
      /* Step 1: round SIZE to the previous multiple of the interval.  */
      /* ROUNDED_SIZE = SIZE & -PROBE_INTERVAL  */
      rounded_size = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,AND, pMode, size,
            mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,-PROBE_INTERVAL, pMode));
      rounded_size_op = mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,rounded_size, NULL_RTX);
      /* Step 2: compute initial and final value of the loop counter.  */
      /* SP = SP_0 + PROBE_INTERVAL.  */
      mtcs_explow_anti_adjust_stack/*!anti_adjust_stack*/(self,GEN_INT (PROBE_INTERVAL + dope));
      /* LAST_ADDR = SP_0 + PROBE_INTERVAL + ROUNDED_SIZE.  */
      last_addr = mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,gen_rtx_fmt_ee (STACK_GROW_OP(mtcsFunc), pMode,
              stackPointerRtx/*!stack_pointer_rtx*/, rounded_size_op), NULL_RTX);

      /* Step 3: the loop
     while (SP != LAST_ADDR)
       {
         SP = SP + PROBE_INTERVAL
         probe at SP
       }

     adjusts SP and probes at PROBE_INTERVAL + N * PROBE_INTERVAL for
     values of N from 1 until it is equal to ROUNDED_SIZE.  */

      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,loop_lab);

      /* Jump to END_LAB if SP == LAST_ADDR.  */
      mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,
              stackPointerRtx/*!stack_pointer_rtx*/, last_addr, EQ, NULL_RTX,pMode, 1, end_lab);

      /* SP = SP + PROBE_INTERVAL and probe at SP.  */
      mtcs_explow_anti_adjust_stack/*!anti_adjust_stack*/(self,GEN_INT (PROBE_INTERVAL));
      mtcs_explow_emit_stack_probe/*!emit_stack_probe*/(self,stackPointerRtx/*!stack_pointer_rtx*/);
      mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,loop_lab);
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,end_lab);
      /* Step 4: adjust SP and probe at PROBE_INTERVAL + SIZE if we cannot
     assert at compile-time that SIZE is equal to ROUNDED_SIZE.  */
      /* TEMP = SIZE - ROUNDED_SIZE.  */
      temp =mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MINUS, pMode, size, rounded_size);
      if (temp != const0_rtx){
          /* Manual CSE if the difference is not known at compile-time.  */
          if (GET_CODE (temp) != CONST_INT)
            temp = gen_rtx_MINUS (Pmode, size, rounded_size_op);
          mtcs_explow_anti_adjust_stack/*!anti_adjust_stack*/(self,temp);
          mtcs_explow_emit_stack_probe/*!emit_stack_probe*/(self,stackPointerRtx/*!stack_pointer_rtx*/);
      }
  }

  /* Adjust back and account for the additional first interval.  */
  if (adjust_back)
      mtcs_explow_adjust_stack/*!adjust_stack*/(self,
              mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, size, PROBE_INTERVAL + dope));
  else
      mtcs_explow_adjust_stack/*!adjust_stack*/(self,GEN_INT (PROBE_INTERVAL + dope));
}

MtcsExplow *mtcs_explow_new(MtcsMode *mtcsMode)
{
    MtcsExplow *self = n_slice_alloc0 (sizeof(MtcsExplow));
    mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
    mtcsExplowInit(self);
    return self;
}


