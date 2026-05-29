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
 * base on dojump.cc
 */

/* This file handles generation of all the assembler code
   *except* the instructions of a function.
   This includes declarations of variables and their initial values.

   We also output the assembler code for constants stored in dojumpory
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
#include "optabs.h"
#include "emit-rtl.h"
#include "fold-const.h"
#include "stor-layout.h"
/* Include expr.h after insn-config.h so we get HAVE_conditional_move.  */
#include "dojump.h"
#include "explow.h"
#include "expr.h"
#include "langhooks.h"
#include "regs.h"

#include "mtcsdojump.h"
#include "mtcsreg.h"
#include "mtcstarget.h"
#include "mtcsprintrtl.h"
#include "aet/aetprinttree.h"

//原型 REVERSE_CONDITION 缺省实现
static enum rtx_code reverseCondition_cb(MtcsDojump *self,enum rtx_code code, machine_mode mode);
//原型 do_jump_by_parts_equality
static void do_jump_by_parts_equality(MtcsDojump *self,scalar_int_mode mode, tree treeop0, tree treeop1,
               rtx_code_label *if_false_label, rtx_code_label *if_true_label,profile_probability prob);
static void do_jump (MtcsDojump *self,tree exp, rtx_code_label *if_false_label,
           rtx_code_label *if_true_label, profile_probability prob);
static void mark_jump_label_1 (MtcsDojump *self,rtx x, rtx_insn *insn, bool in_mem, bool is_target);

static void mtcsDojumpInit(MtcsDojump *self)
{
    //原型 REVERSE_CONDITION
    self->reverse_condition=reverseCondition_cb;
}

//原型 REVERSE_CONDITION 缺省实现
//reverse_condition rtl.h jump.cc
static enum rtx_code reverseCondition_cb(MtcsDojump *self,enum rtx_code code, machine_mode mode)
{
    return reverse_condition(code);
}

/* Jump according to whether OP0 is 0.  We assume that OP0 has an integer
   mode, MODE, that is too wide for the available compare insns.  Either
   Either (but not both) of IF_TRUE_LABEL and IF_FALSE_LABEL may be NULL
   to indicate drop through.  */

static void do_jump_by_parts_zero_rtx (MtcsDojump *self,scalar_int_mode mode, rtx op0,
               rtx_code_label *if_false_label,rtx_code_label *if_true_label,profile_probability prob)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

  int nwords = mtcs_mode_get_size (mtcsMode,mode) / UNITS_PER_WORD;
  rtx part;
  int i;
  rtx_code_label *drop_through_label = NULL;
  /* The fastest way of doing this comparison on almost any machine is to
     "or" all the words and compare the result.  If all have to be loaded
     from memory and this is a very wide item, it's possible this may
     be slower, but that's highly unlikely.  */
  part = mtcs_emit_gen_reg_rtx (mtcsEmit,word_mode);
  mtcs_expr_emit_move_insn (mtcsExpr,part, mtcs_rtl_operand_subword_force (mtcsRTL,op0, 0, mode));
  for (i = 1; i < nwords && part != 0; i++)
    part = mtcs_optabs_expand_binop (mtcsOptabs,word_mode, ior_optab, part,
            mtcs_rtl_operand_subword_force (mtcsRTL,op0, i, mode),
                         part, 1, OPTAB_WIDEN);

  if (part != 0){
      mtcs_dojump_do_compare_rtx_and_jump (self,part, const0_rtx, EQ, 1, NULL, word_mode,
                   NULL_RTX, if_false_label, if_true_label, prob);
      return;
  }

  /* If we couldn't do the "or" simply, do this with a series of compares.  */
  if (! if_false_label)
    if_false_label = drop_through_label =mtcs_rtl_gen_label_rtx(mtcsRTL);

  for (i = 0; i < nwords; i++)
      mtcs_dojump_do_compare_rtx_and_jump (self,mtcs_rtl_operand_subword_force (mtcsRTL,op0, i, mode),
                 const0_rtx, EQ, 1, NULL, word_mode, NULL_RTX,if_false_label, NULL, prob);

  if (if_true_label)
    mtcs_emit_emit_jump (mtcsEmit,if_true_label);

  if (drop_through_label)
    mtcs_emit_emit_label (mtcsEmit,drop_through_label);
}

/* Compare the relative costs of "(X & (1 << BITNUM))" and "(X >> BITNUM) & 1",
   where X is an arbitrary register of mode MODE.  Return true if the former
   is preferred.  */
//原型 prefer_and_bit_test dojump.cc
static bool prefer_and_bit_test (MtcsDojump *self,scalar_int_mode mode, int bitnum)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

  bool speed_p;
  wide_int mask = wi::set_bit_in_zero (bitnum, mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode));

  if (self->and_test == 0){
      /* Set up rtxes for the two variations.  Use NULL as a placeholder
     for the BITNUM-based constants.  */
      self->and_reg = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mode,
            mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg) + 1);
      self->and_test = gen_rtx_AND (mode, self->and_reg, NULL);
      self->shift_test = gen_rtx_AND (mode, gen_rtx_ASHIFTRT (mode, self->and_reg, NULL),const1_rtx);
  }else{
      /* Change the mode of the previously-created rtxes.  */
      mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,self->and_reg, mode);
      mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,self->and_test, mode);
      mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,self->shift_test, mode);
      mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,XEXP (self->shift_test, 0), mode);
  }

  /* Fill in the integers.  */
  XEXP (self->and_test, 1) = mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,mask, mode);
  XEXP (XEXP (self->shift_test, 0), 1) =mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,bitnum);

  speed_p = optimize_insn_for_speed_p ();
  return (mtcs_rtlanal_rtx_cost/*!rtx_cost*/(mtcsRtlanal,self->and_test, mode, IF_THEN_ELSE, 0, speed_p)
      <= mtcs_rtlanal_rtx_cost/*!rtx_cost*/(mtcsRtlanal,self->shift_test, mode, IF_THEN_ELSE, 0, speed_p));
}

/* Generate code for a comparison expression EXP (including code to compute
   the values to be compared) and a conditional jump to IF_FALSE_LABEL and/or
   IF_TRUE_LABEL.  One of the labels can be NULL_RTX, in which case the
   generated code will drop through.
   SIGNED_CODE should be the rtx operation for this comparison for
   signed data; UNSIGNED_CODE, likewise for use if data is unsigned.

   We force a stack adjustment unless there are currently
   things pushed on the stack that aren't yet used.  */

static void do_compare_and_jump (MtcsDojump *self,tree treeop0, tree treeop1, enum rtx_code signed_code,
             enum rtx_code unsigned_code,
             rtx_code_label *if_false_label,
             rtx_code_label *if_true_label, profile_probability prob)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  rtx op0, op1;
  tree type;
  machine_mode mode;
  int unsignedp;
  enum rtx_code code;

  /* Don't crash if the comparison was erroneous.  */
  op0 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,treeop0);
  if (TREE_CODE (treeop0) == ERROR_MARK)
    return;

  op1 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,treeop1);
  if (TREE_CODE (treeop1) == ERROR_MARK)
    return;

  type = TREE_TYPE (treeop0);
  if (TREE_CODE (treeop0) == INTEGER_CST
      && (TREE_CODE (treeop1) != INTEGER_CST
      || (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,type))
          > mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,
                mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,TREE_TYPE (treeop1))))))
    /* op0 might have been replaced by promoted constant, in which
       case the type of second argument should be used.  */
    type = TREE_TYPE (treeop1);
  mode = TYPE_MODE (type);
  unsignedp = TYPE_UNSIGNED (type);
  code = unsignedp ? unsigned_code : signed_code;

  /* If function pointers need to be "canonicalized" before they can
     be reliably compared, then canonicalize them.  Canonicalize the
     expression when one of the operands is a function pointer.  This
     handles the case where the other operand is a void pointer.  See
     PR middle-end/17564.  */
  if (target_rtx_have_canonicalize_funcptr_for_compare/*!targetm.have_canonicalize_funcptr_for_compare*/(mtcsMachine->tmrtx)
      && ((POINTER_TYPE_P (TREE_TYPE (treeop0))
       && FUNC_OR_METHOD_TYPE_P (TREE_TYPE (TREE_TYPE (treeop0))))
      || (POINTER_TYPE_P (TREE_TYPE (treeop1))
          && FUNC_OR_METHOD_TYPE_P (TREE_TYPE (TREE_TYPE (treeop1)))))){
      rtx new_op0 = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
      rtx new_op1 = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,
              target_rtx_gen_canonicalize_funcptr_for_compare/*!targetm.gen_canonicalize_funcptr_for_compare*/(mtcsMachine->tmrtx,
                    new_op0, op0));
      op0 = new_op0;

      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,
            target_rtx_gen_canonicalize_funcptr_for_compare/*!targetm.gen_canonicalize_funcptr_for_compare*/(mtcsMachine->tmrtx,
                  new_op1, op1));
      op1 = new_op1;
  }

  mtcs_dojump_do_compare_rtx_and_jump(self,op0, op1, code, unsignedp, treeop0, mode,
               ((mode == mtcsMode->modes.M_BLKmode)? mtcs_expr_expr_size/*!expr_size*/(mtcsExpr,treeop0)
                     : NULL_RTX),if_false_label, if_true_label, prob);
}


/* Test for the equality of two RTX expressions OP0 and OP1 in mode MODE,
   where MODE is an integer mode too wide to be compared with one insn.
   Either (but not both) of IF_TRUE_LABEL and IF_FALSE_LABEL may be NULL_RTX
   to indicate drop through.  */

static void do_jump_by_parts_equality_rtx (MtcsDojump *self,scalar_int_mode mode, rtx op0, rtx op1,
                   rtx_code_label *if_false_label,rtx_code_label *if_true_label,profile_probability prob)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  int nwords = (mtcs_mode_get_size (mtcsMode,mode) / UNITS_PER_WORD);
  rtx_code_label *drop_through_label = NULL;
  int i;

  if (op1 == const0_rtx){
      do_jump_by_parts_zero_rtx (self,mode, op0, if_false_label, if_true_label,prob);
      return;
  }else if (op0 == const0_rtx){
      do_jump_by_parts_zero_rtx (self,mode, op1, if_false_label, if_true_label,prob);
      return;
  }

  if (! if_false_label)
    drop_through_label = if_false_label =mtcs_rtl_gen_label_rtx(mtcsRTL);

  for (i = 0; i < nwords; i++)
    mtcs_dojump_do_compare_rtx_and_jump (self,mtcs_rtl_operand_subword_force(mtcsRTL,op0, i, mode),
            mtcs_rtl_operand_subword_force(mtcsRTL,op1, i, mode),EQ, 0, NULL, word_mode, NULL_RTX,if_false_label, NULL, prob);

  if (if_true_label)
    mtcs_emit_emit_jump (mtcsEmit,if_true_label);
  if (drop_through_label)
    mtcs_emit_emit_label (mtcsEmit,drop_through_label);
}

/* Compare OP0 with OP1, word at a time, in mode MODE.
   UNSIGNEDP says to do unsigned comparison.
   Jump to IF_TRUE_LABEL if OP0 is greater, IF_FALSE_LABEL otherwise.  */
//原型 do_jump_by_parts_greater_rtx dojump.cc
static void do_jump_by_parts_greater_rtx (MtcsDojump *self,scalar_int_mode mode, int unsignedp, rtx op0,
                  rtx op1, rtx_code_label *if_false_label,rtx_code_label *if_true_label,profile_probability prob)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  int nwords = (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode) / UNITS_PER_WORD);
  rtx_code_label *drop_through_label = 0;
  bool drop_through_if_true = false, drop_through_if_false = false;
  enum rtx_code code = GT;
  int i;
  n_debug("mtcsdojump.c do_jump_by_parts_greater_rtx 00 if_false_label:%p if_true_label:%p nwords:%d\n",
        if_false_label,if_true_label,nwords);
  mtcs_print_rtl_single(stderr,op0);
  mtcs_print_rtl_single(stderr,op1);
  mtcs_print_rtl_single(stderr,if_false_label);

  if (! if_true_label || ! if_false_label)
    drop_through_label =mtcs_rtl_gen_label_rtx(mtcsRTL);
  if (! if_true_label){
      if_true_label = drop_through_label;
      drop_through_if_true = true;
  }
  if (! if_false_label){
      if_false_label = drop_through_label;
      drop_through_if_false = true;
  }

  n_debug("mtcsdojump.c do_jump_by_parts_greater_rtx 11 drop_through_if_false:%d drop_through_if_true:%d\n",
        drop_through_if_false,drop_through_if_true);

  /* Deal with the special case 0 > x: only one comparison is necessary and
     we reverse it to avoid jumping to the drop-through label.  */
  if (op0 == const0_rtx && drop_through_if_true && !drop_through_if_false){
     n_debug("mtcsdojump.c do_jump_by_parts_greater_rtx 22 drop_through_if_false:%d drop_through_if_true:%d\n",
           drop_through_if_false,drop_through_if_true);
      code = LE;
      if_true_label = if_false_label;
      if_false_label = drop_through_label;
      prob = prob.invert ();
  }
  n_debug("mtcsdojump.c do_jump_by_parts_greater_rtx 33 drop_through_if_false:%d drop_through_if_true:%d\n",
        drop_through_if_false,drop_through_if_true);
  mtcs_print_rtl_single(stderr,drop_through_label);
  mtcs_print_rtl_single(stderr,if_false_label);
  mtcs_print_rtl_single(stderr,if_true_label);

  /* Compare a word at a time, high order first.  */
  for (i = 0; i < nwords; i++){
      rtx op0_word, op1_word;
      n_debug("mtcsdojump.c do_jump_by_parts_greater_rtx 33 i:%d\n",i);
      if (WORDS_BIG_ENDIAN){
          op0_word = mtcs_rtl_operand_subword_force/*!operand_subword_force*/(mtcsRTL,op0, i, mode);
          op1_word = mtcs_rtl_operand_subword_force(mtcsRTL,op1, i, mode);
      }else{
          op0_word = mtcs_rtl_operand_subword_force(mtcsRTL,op0, nwords - 1 - i, mode);
          op1_word = mtcs_rtl_operand_subword_force(mtcsRTL,op1, nwords - 1 - i, mode);
      }

      /* All but high-order word must be compared as unsigned.  */
      mtcs_dojump_do_compare_rtx_and_jump (self,op0_word, op1_word, code, (unsignedp || i > 0),
                   NULL, word_mode, NULL_RTX, NULL, if_true_label,prob);

      /* Emit only one comparison for 0.  Do not emit the last cond jump.  */
      if (op0 == const0_rtx || i == nwords - 1)
          break;

      /* Consider lower words only if these are equal.  */
      mtcs_dojump_do_compare_rtx_and_jump (self,op0_word, op1_word, NE, unsignedp, NULL,
                   word_mode, NULL_RTX, NULL, if_false_label, prob.invert ());
  }

  if (!drop_through_if_false)
    mtcs_emit_emit_jump (mtcsEmit,if_false_label);
  if (drop_through_label)
    mtcs_emit_emit_label (mtcsEmit,drop_through_label);
}


/* Given an EQ_EXPR expression EXP for values too wide to be compared
   with one insn, test the comparison and jump to the appropriate label.
   MODE is the mode of the two operands.  */
//原型 do_jump_by_parts_equality
static void do_jump_by_parts_equality(MtcsDojump *self,scalar_int_mode mode, tree treeop0, tree treeop1,
               rtx_code_label *if_false_label,
               rtx_code_label *if_true_label,
               profile_probability prob)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  rtx op0 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,treeop0);
  rtx op1 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,treeop1);
  do_jump_by_parts_equality_rtx(self,mode, op0, op1, if_false_label,if_true_label, prob);
}

/* Given a comparison expression EXP for values too wide to be compared
   with one insn, test the comparison and jump to the appropriate label.
   The code of EXP is ignored; we always test GT if SWAP is 0,
   and LT if SWAP is 1.  MODE is the mode of the two operands.  */
//原型 do_jump_by_parts_greater dojump.cc
static void do_jump_by_parts_greater(MtcsDojump *self,scalar_int_mode mode, tree treeop0, tree treeop1,
              int swap, rtx_code_label *if_false_label, rtx_code_label *if_true_label, profile_probability prob)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

  rtx op0 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,swap ? treeop1 : treeop0);
  rtx op1 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,swap ? treeop0 : treeop1);
  int unsignedp = TYPE_UNSIGNED (TREE_TYPE (treeop0));

  do_jump_by_parts_greater_rtx(self,mode, unsignedp, op0, op1, if_false_label,if_true_label, prob);
}

/* Subroutine of do_jump, dealing with exploded comparisons of the type
   OP0 CODE OP1 .  IF_FALSE_LABEL and IF_TRUE_LABEL like in do_jump.
   PROB is probability of jump to if_true_label.  */
//原型 do_jump_1 dojump.cc
static void do_jump_1 (MtcsDojump *self,enum tree_code code, tree op0, tree op1,
       rtx_code_label *if_false_label, rtx_code_label *if_true_label,
       profile_probability prob)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  machine_mode mode;
  rtx_code_label *drop_through_label = 0;
  scalar_int_mode int_mode;
  switch (code){
    case EQ_EXPR:
      {
        tree inner_type = TREE_TYPE (op0);

        gcc_assert (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,TYPE_MODE (inner_type))!= MODE_COMPLEX_FLOAT);
        gcc_assert (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,TYPE_MODE (inner_type))!= MODE_COMPLEX_INT);

        if (integer_zerop (op1))
            do_jump(self,op0, if_true_label, if_false_label,prob.invert ());
        else if (mtcs_mode_is_int_mode/*!is_int_mode*/(mtcsMode,TYPE_MODE (inner_type), &int_mode)
                && !mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,EQ, int_mode, ccp_jump))
            do_jump_by_parts_equality(self,int_mode, op0, op1, if_false_label,if_true_label, prob);
        else
            do_compare_and_jump(self,op0, op1, EQ, EQ, if_false_label, if_true_label,prob);
        break;
      }

    case NE_EXPR:
      {
        tree inner_type = TREE_TYPE (op0);

        gcc_assert (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,TYPE_MODE (inner_type)) != MODE_COMPLEX_FLOAT);
        gcc_assert (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,TYPE_MODE (inner_type))!= MODE_COMPLEX_INT);

        if (integer_zerop (op1)){
          n_debug("mtcsdojump.c do_jump_1 NE_EXPR 00\n");
            do_jump(self,op0, if_false_label, if_true_label, prob);
        }else if (mtcs_mode_is_int_mode/*!is_int_mode*/(mtcsMode,TYPE_MODE (inner_type), &int_mode)
                && !mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,NE, int_mode, ccp_jump)){
          n_debug("mtcsdojump.c do_jump_1 NE_EXPR 11\n");

            do_jump_by_parts_equality(self,int_mode, op0, op1, if_true_label,if_false_label, prob.invert ());
        }else{
          n_debug("mtcsdojump.c do_jump_1 NE_EXPR 22\n");
            do_compare_and_jump(self,op0, op1, NE, NE, if_false_label, if_true_label,prob);
        }
        break;
      }

    case LT_EXPR:
      mode = TYPE_MODE (TREE_TYPE (op0));
      if (mtcs_mode_is_int_mode/*!is_int_mode*/(mtcsMode,mode, &int_mode)
        && ! mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,LT, int_mode, ccp_jump)){
         n_debug("mtcsdojump.c do_jump_1 LT_EXPR 00 mode:%d\n",mode);

          do_jump_by_parts_greater(self,int_mode, op0, op1, 1, if_false_label,if_true_label, prob);
      }else{
         n_debug("mtcsdojump.c do_jump_1 LT_EXPR 11 mode:%d\n",mode);

          do_compare_and_jump(self,op0, op1, LT, LTU, if_false_label, if_true_label,prob);
      }
      break;

    case LE_EXPR:
      mode = TYPE_MODE (TREE_TYPE (op0));
      n_debug("mtcsdojump.c do_jump_1 LE_EXPR 00 mode:%d\n",mode);
      if (mtcs_mode_is_int_mode/*!is_int_mode*/(mtcsMode,mode, &int_mode)
      && ! mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,LE, int_mode, ccp_jump)){
         n_debug("mtcsdojump.c do_jump_1 LE_EXPR 11 mode:%d int_mode:%d\n",mode,int_mode);

        do_jump_by_parts_greater (self,int_mode, op0, op1, 0, if_true_label,
                      if_false_label, prob.invert ());
      }else{
         n_debug("mtcsdojump.c do_jump_1 LE_EXPR 22 mode:%d\n",mode);

          do_compare_and_jump(self,op0, op1, LE, LEU, if_false_label, if_true_label, prob);
      }
      break;

    case GT_EXPR:
      mode = TYPE_MODE (TREE_TYPE (op0));
     n_debug("mtcsdojump.c do_jump_1 GT_EXPR aa op0的mode:%d if_false_label:%p\n",mode,if_false_label);
      if (mtcs_mode_is_int_mode/*!is_int_mode*/(mtcsMode,mode, &int_mode)
        && ! mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,GT, int_mode, ccp_jump))
          do_jump_by_parts_greater(self,int_mode, op0, op1, 0, if_false_label,if_true_label, prob);
      else
          do_compare_and_jump (self,op0, op1, GT, GTU, if_false_label, if_true_label,prob);
     n_debug("mtcsdojump.c do_jump_1 GT_EXPR bb op0的mode:%d if_false_label:%p\n",mode,if_false_label);

      break;

    case GE_EXPR:
      mode = TYPE_MODE (TREE_TYPE (op0));
      if (mtcs_mode_is_int_mode/*!is_int_mode*/(mtcsMode,mode, &int_mode)
        && ! mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,GE, int_mode, ccp_jump))
          do_jump_by_parts_greater(self,int_mode, op0, op1, 1, if_true_label,if_false_label, prob.invert ());
      else
          do_compare_and_jump(self,op0, op1, GE, GEU, if_false_label, if_true_label,prob);
      break;

    case ORDERED_EXPR:
      do_compare_and_jump(self,op0, op1, ORDERED, ORDERED,if_false_label, if_true_label, prob);
      break;

    case UNORDERED_EXPR:
      do_compare_and_jump(self,op0, op1, UNORDERED, UNORDERED,if_false_label, if_true_label, prob);
      break;

    case UNLT_EXPR:
      do_compare_and_jump(self,op0, op1, UNLT, UNLT, if_false_label, if_true_label,prob);
      break;

    case UNLE_EXPR:
      do_compare_and_jump(self,op0, op1, UNLE, UNLE, if_false_label, if_true_label,prob);
      break;

    case UNGT_EXPR:
      do_compare_and_jump(self,op0, op1, UNGT, UNGT, if_false_label, if_true_label,prob);
      break;

    case UNGE_EXPR:
      do_compare_and_jump(self,op0, op1, UNGE, UNGE, if_false_label, if_true_label, prob);
      break;

    case UNEQ_EXPR:
      do_compare_and_jump(self,op0, op1, UNEQ, UNEQ, if_false_label, if_true_label,prob);
      break;

    case LTGT_EXPR:
      do_compare_and_jump(self,op0, op1, LTGT, LTGT, if_false_label, if_true_label,prob);
      break;

    case TRUTH_ANDIF_EXPR:
      {
        /* Spread the probability that the expression is false evenly between
           the two conditions. So the first condition is false half the total
           probability of being false. The second condition is false the other
           half of the total probability of being false, so its jump has a false
           probability of half the total, relative to the probability we
           reached it (i.e. the first condition was true).  */
        profile_probability op0_prob = profile_probability::uninitialized ();
        profile_probability op1_prob = profile_probability::uninitialized ();
        if (prob.initialized_p ()){
            op1_prob = prob.invert ();
            op0_prob = op1_prob.split (profile_probability::even ());
                /* Get the probability that each jump below is true.  */
            op0_prob = op0_prob.invert ();
            op1_prob = op1_prob.invert ();
        }
        if (if_false_label == NULL){
            drop_through_label =mtcs_rtl_gen_label_rtx(mtcsRTL);
            do_jump(self,op0, drop_through_label, NULL, op0_prob);
            do_jump(self,op1, NULL, if_true_label, op1_prob);
        }else{
            do_jump(self,op0, if_false_label, NULL, op0_prob);
            do_jump(self,op1, if_false_label, if_true_label, op1_prob);
        }
        break;
      }

    case TRUTH_ORIF_EXPR:
      {
        /* Spread the probability evenly between the two conditions. So
           the first condition has half the total probability of being true.
           The second condition has the other half of the total probability,
           so its jump has a probability of half the total, relative to
           the probability we reached it (i.e. the first condition was false).  */
        profile_probability op0_prob = profile_probability::uninitialized ();
        profile_probability op1_prob = profile_probability::uninitialized ();
        if (prob.initialized_p ()){
            op1_prob = prob;
            op0_prob = op1_prob.split (profile_probability::even ());
        }
        if (if_true_label == NULL){
            drop_through_label =mtcs_rtl_gen_label_rtx(mtcsRTL);
            do_jump(self,op0, NULL, drop_through_label, op0_prob);
            do_jump(self,op1, if_false_label, NULL, op1_prob);
        }else{
            do_jump(self,op0, NULL, if_true_label, op0_prob);
            do_jump(self,op1, if_false_label, if_true_label, op1_prob);
        }
        break;
      }

    default:
      gcc_unreachable ();
  }

  if (drop_through_label){
      mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(self);
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,drop_through_label);
  }
}

/* Generate code to evaluate EXP and jump to IF_FALSE_LABEL if
   the result is zero, or IF_TRUE_LABEL if the result is one.
   Either of IF_FALSE_LABEL and IF_TRUE_LABEL may be zero,
   meaning fall through in that case.

   do_jump always does any pending stack adjust except when it does not
   actually perform a jump.  An example where there is no jump
   is when EXP is `(foo (), 0)' and IF_FALSE_LABEL is null.

   PROB is probability of jump to if_true_label.  */
//原型 do_jump dojump.cc
static void do_jump (MtcsDojump *self,tree exp, rtx_code_label *if_false_label,rtx_code_label *if_true_label, profile_probability prob)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

  enum tree_code code = TREE_CODE (exp);
  rtx temp;
  int i;
  tree type;
  scalar_int_mode mode;
  rtx_code_label *drop_through_label = NULL;

  switch (code){
    case ERROR_MARK:
      break;

    case INTEGER_CST:
      {
        rtx_code_label *lab = integer_zerop (exp) ? if_false_label: if_true_label;
        if (lab)
            mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,lab);
        break;
      }

#if 0
      /* This is not true with #pragma weak  */
    case ADDR_EXPR:
      /* The address of something can never be zero.  */
      if (if_true_label)
         mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,if_true_label);
      break;
#endif

    CASE_CONVERT:
      if (TREE_CODE (TREE_OPERAND (exp, 0)) == COMPONENT_REF
          || TREE_CODE (TREE_OPERAND (exp, 0)) == BIT_FIELD_REF
          || TREE_CODE (TREE_OPERAND (exp, 0)) == ARRAY_REF
          || TREE_CODE (TREE_OPERAND (exp, 0)) == ARRAY_RANGE_REF){
        n_debug("mtcsdojump.c do_jump 00 跳进  normal code:%d %s\n",code,get_tree_code_name(code));

        goto normal;
      }
      /* If we are narrowing the operand, we have to do the compare in the
         narrower mode.  */
      if ((TYPE_PRECISION (TREE_TYPE (exp)) < TYPE_PRECISION (TREE_TYPE (TREE_OPERAND (exp, 0))))){
        n_debug("mtcsdojump.c do_jump 11 跳进  normal code:%d %s\n",code,get_tree_code_name(code));
        goto normal;
      }
      /* FALLTHRU */
    case NON_LVALUE_EXPR:
    case ABS_EXPR:
    case ABSU_EXPR:
    case NEGATE_EXPR:
    case LROTATE_EXPR:
    case RROTATE_EXPR:
      /* These cannot change zero->nonzero or vice versa.  */
      do_jump(self,TREE_OPERAND (exp, 0), if_false_label, if_true_label, prob);
      break;

    case TRUTH_NOT_EXPR:
      do_jump(self,TREE_OPERAND (exp, 0), if_true_label, if_false_label,prob.invert ());
      break;

    case COND_EXPR:
      {
        rtx_code_label *label1 =mtcs_rtl_gen_label_rtx(mtcsRTL);
        if (!if_true_label || !if_false_label){
            drop_through_label =mtcs_rtl_gen_label_rtx(mtcsRTL);
            if (!if_true_label)
              if_true_label = drop_through_label;
            if (!if_false_label)
              if_false_label = drop_through_label;
        }

        do_pending_stack_adjust ();
        do_jump(self,TREE_OPERAND (exp, 0), label1, NULL,profile_probability::uninitialized ());
        do_jump(self,TREE_OPERAND (exp, 1), if_false_label, if_true_label, prob);
        mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label1);
        do_jump (self,TREE_OPERAND (exp, 2), if_false_label, if_true_label, prob);
        break;
      }

    case COMPOUND_EXPR:
      /* Lowered by gimplify.cc.  */
      gcc_unreachable ();

    case MINUS_EXPR:
      /* Nonzero iff operands of minus differ.  */
      code = NE_EXPR;

      /* FALLTHRU */
    case EQ_EXPR:
    case NE_EXPR:
    case LT_EXPR:
    case LE_EXPR:
    case GT_EXPR:
    case GE_EXPR:
    case ORDERED_EXPR:
    case UNORDERED_EXPR:
    case UNLT_EXPR:
    case UNLE_EXPR:
    case UNGT_EXPR:
    case UNGE_EXPR:
    case UNEQ_EXPR:
    case LTGT_EXPR:
    case TRUTH_ANDIF_EXPR:
    case TRUTH_ORIF_EXPR:
    other_code:
      do_jump_1(self,code, TREE_OPERAND (exp, 0), TREE_OPERAND (exp, 1),
         if_false_label, if_true_label, prob);
      break;

    case BIT_AND_EXPR:
      /* fold_single_bit_test() converts (X & (1 << C)) into (X >> C) & 1.
     See if the former is preferred for jump tests and restore it
     if so.  */
      if (integer_onep (TREE_OPERAND (exp, 1))){
          tree exp0 = TREE_OPERAND (exp, 0);
          rtx_code_label *set_label, *clr_label;
          profile_probability setclr_prob = prob;

          /* Strip narrowing integral type conversions.  */
          while (CONVERT_EXPR_P (exp0)
             && TREE_OPERAND (exp0, 0) != error_mark_node
             && TYPE_PRECISION (TREE_TYPE (exp0))
                <= TYPE_PRECISION (TREE_TYPE (TREE_OPERAND (exp0, 0))))
            exp0 = TREE_OPERAND (exp0, 0);

          /* "exp0 ^ 1" inverts the sense of the single bit test.  */
          if (TREE_CODE (exp0) == BIT_XOR_EXPR  && integer_onep (TREE_OPERAND (exp0, 1))){
              exp0 = TREE_OPERAND (exp0, 0);
              clr_label = if_true_label;
              set_label = if_false_label;
              setclr_prob = prob.invert ();
          }else{
              clr_label = if_false_label;
              set_label = if_true_label;
          }

          if (TREE_CODE (exp0) == RSHIFT_EXPR){
              tree arg = TREE_OPERAND (exp0, 0);
              tree shift = TREE_OPERAND (exp0, 1);
              tree argtype = TREE_TYPE (arg);

              if (TREE_CODE (shift) == INTEGER_CST
                && compare_tree_int (shift, 0) >= 0
                && compare_tree_int (shift, HOST_BITS_PER_WIDE_INT) < 0
                && prefer_and_bit_test(self,mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,argtype),
                          TREE_INT_CST_LOW (shift))){
                  unsigned HOST_WIDE_INT mask = HOST_WIDE_INT_1U << TREE_INT_CST_LOW (shift);
                  do_jump(self,build2 (BIT_AND_EXPR, argtype, arg, build_int_cstu (argtype, mask)),clr_label, set_label, setclr_prob);
                  break;
             }
          }
      }

      /* If we are AND'ing with a small constant, do this comparison in the
         smallest type that fits.  If the machine doesn't have comparisons
         that small, it will be converted back to the wider comparison.
         This helps if we are testing the sign bit of a narrower object.
         combine can't do this for us because it can't know whether a
         ZERO_EXTRACT or a compare in a smaller mode exists, but we do.  */

      if (! SLOW_BYTE_ACCESS
          && TREE_CODE (TREE_OPERAND (exp, 1)) == INTEGER_CST
          && TYPE_PRECISION (TREE_TYPE (exp)) <= HOST_BITS_PER_WIDE_INT
          && (i = tree_floor_log2 (TREE_OPERAND (exp, 1))) >= 0
          && mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,i + 1, 0).exists (&mode)
          && (type = lang_hooks.types.type_for_mode (mode, 1)) != 0
          && TYPE_PRECISION (type) < TYPE_PRECISION (TREE_TYPE (exp))
          && mtcs_optabs_have_insn_for/*!have_insn_for*/(mtcsOptabs,COMPARE, TYPE_MODE (type))){
          do_jump(self,mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,type, exp), if_false_label, if_true_label,prob);
          break;
      }

      if (TYPE_PRECISION (TREE_TYPE (exp)) > 1 || TREE_CODE (TREE_OPERAND (exp, 1)) == INTEGER_CST){
        n_debug("mtcsdojump.c do_jump 22 跳进  normal  code:%d %s\n",code,get_tree_code_name(code));

          goto normal;
      }

      /* Boolean comparisons can be compiled as TRUTH_AND_EXPR.  */
      /* FALLTHRU */

    case TRUTH_AND_EXPR:
      /* High branch cost, expand as the bitwise AND of the conditions.
     Do the same if the RHS has side effects, because we're effectively
     turning a TRUTH_AND_EXPR into a TRUTH_ANDIF_EXPR.  */
      if (BRANCH_COST (optimize_insn_for_speed_p (),false) >= 4 || TREE_SIDE_EFFECTS (TREE_OPERAND (exp, 1))){
        n_debug("mtcsdojump.c do_jump 33 跳进  normal  code:%d %s\n",code,get_tree_code_name(code));

          goto normal;
      }
      code = TRUTH_ANDIF_EXPR;
      goto other_code;

    case BIT_IOR_EXPR:
    case TRUTH_OR_EXPR:
      /* High branch cost, expand as the bitwise OR of the conditions.
     Do the same if the RHS has side effects, because we're effectively
     turning a TRUTH_OR_EXPR into a TRUTH_ORIF_EXPR.  */
      if (BRANCH_COST (optimize_insn_for_speed_p (), false) >= 4 || TREE_SIDE_EFFECTS (TREE_OPERAND (exp, 1))){
        n_debug("mtcsdojump.c do_jump 44 跳进  normal  code:%d %s\n",code,get_tree_code_name(code));

          goto normal;
      }
      code = TRUTH_ORIF_EXPR;
      goto other_code;

      /* Fall through and generate the normal code.  */
    default:
    normal:
     n_debug("mtcsdojump.c do_jump 55 进入 normal code:%d %s\n",code,get_tree_code_name(code));
      temp = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,exp);
      mtcs_print_rtl_single(stderr,temp);
      mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(self);
      /* The RTL optimizers prefer comparisons against pseudos.  */
      if (GET_CODE (temp) == SUBREG){
        n_debug("mtcsdojump.c do_jump 66 进入 normal code:%d %s\n",code,get_tree_code_name(code));

          /* Compare promoted variables in their promoted mode.  */
          if (SUBREG_PROMOTED_VAR_P (temp)   && REG_P (XEXP (temp, 0)))
            temp = XEXP (temp, 0);
          else
            temp = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,temp);
      }
     n_debug("mtcsdojump.c do_jump 77 进入 normal code:%d %s\n",code,get_tree_code_name(code));

      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(self,temp, CONST0_RTX (GET_MODE (temp)),
                   NE, TYPE_UNSIGNED (TREE_TYPE (exp)),exp, GET_MODE (temp), NULL_RTX, if_false_label, if_true_label, prob);
  }

  if (drop_through_label){
      mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(self);
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,drop_through_label);
  }
}

/* Like do_compare_and_jump but expects the values to compare as two rtx's.
   The decision as to signed or unsigned comparison must be made by the caller.

   If MODE is BLKmode, SIZE is an RTX giving the size of the objects being
   compared.  */
//原型 do_compare_rtx_and_jump dojump.h dojump.cc
void mtcs_dojump_do_compare_rtx_and_jump (MtcsDojump *self,rtx op0, rtx op1, enum rtx_code code, int unsignedp,
             machine_mode mode, rtx size,
             rtx_code_label *if_false_label,
             rtx_code_label *if_true_label,
             profile_probability prob)
{
    mtcs_dojump_do_compare_rtx_and_jump (self,op0, op1, code, unsignedp, NULL, mode, size,
              if_false_label, if_true_label, prob);
}

/* Like do_compare_and_jump but expects the values to compare as two rtx's.
   The decision as to signed or unsigned comparison must be made by the caller.

   If MODE is BLKmode, SIZE is an RTX giving the size of the objects being
   compared.  */
//原型 do_compare_rtx_and_jump dojump.h dojump.cc 重载函数
void mtcs_dojump_do_compare_rtx_and_jump (MtcsDojump *self,rtx op0, rtx op1, enum rtx_code code, int unsignedp,
             tree val, machine_mode mode, rtx size,
             rtx_code_label *if_false_label,
             rtx_code_label *if_true_label,
             profile_probability prob)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

  rtx tem;
  rtx_code_label *dummy_label = NULL;
 n_debug("mtcsdojump.c mtcs_dojump_do_compare_rtx_and_jump 00 code:%d mode:%d unsignedp:%d\n",code,mode,unsignedp);
  aet_print_tree(val);
  mtcs_print_rtl_single(stderr,op0);
  mtcs_print_rtl_single(stderr,op1);
  mtcs_print_rtl_single(stderr,size);
  mtcs_print_rtl_single(stderr,if_true_label);

  /* Reverse the comparison if that is safe and we want to jump if it is
     false.  Also convert to the reverse comparison if the target can
     implement it.  */
  if ((! if_true_label || ! mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,code, mode, ccp_jump))
      && (! mtcs_mode_is_float_p(mtcsMode,mode)
      || code == ORDERED || code == UNORDERED
      || (! mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,mode) && (code == LTGT || code == UNEQ))
      || (! mtcs_mode_honor_snans/*!HONOR_SNANS*/(mtcsMode,mode) && (code == EQ || code == NE)))){
      enum rtx_code rcode;
     n_debug("mtcsdojump.c mtcs_dojump_do_compare_rtx_and_jump 11 float:%d\n",mtcs_mode_is_float_p(mtcsMode,mode));
      if (mtcs_mode_is_float_p(mtcsMode,mode))
        rcode = reverse_condition_maybe_unordered (code);
      else
        rcode = reverse_condition (code);

      /* Canonicalize to UNORDERED for the libcall.  */
      if (mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,rcode, mode, ccp_jump)
          || (code == ORDERED && ! mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,ORDERED, mode, ccp_jump))){
        n_debug("mtcsdojump.c mtcs_dojump_do_compare_rtx_and_jump 22 float:%d\n",mtcs_mode_is_float_p(mtcsMode,mode));
          std::swap (if_true_label, if_false_label);
          code = rcode;
          prob = prob.invert ();
      }
  }

  /* If one operand is constant, make it the second one.  Only do this
     if the other operand is not constant as well.  */

  if (mtcs_rtlanal_swap_commutative_operands_p/*!swap_commutative_operands_p*/(mtcsRtlanal,op0, op1)){
      std::swap (op0, op1);
      code = swap_condition (code);
  }
 n_debug("mtcsdojump.c mtcs_dojump_do_compare_rtx_and_jump 33 code:%d mode:%d unsignedp:%d\n",code,mode,unsignedp);
  mtcs_dojump_do_pending_stack_adjust (self);
  code = unsignedp ? unsigned_condition (code) : code;
  if ((tem = mtcs_simplify_rtx_relational_operation(mtcsSimplifyRtx,code, mode, VOIDmode,op0, op1)) != 0){
    n_debug("mtcsdojump.c mtcs_dojump_do_compare_rtx_and_jump 44 code:%d mode:%d tem:%p const0_rtx:%p %p\n",
          code,mode,tem,const0_rtx,CONST0_RTX (mode));
      if (CONSTANT_P (tem)){
          rtx_code_label *label = (tem == const0_rtx || tem == CONST0_RTX (mode)) ? if_false_label : if_true_label;
          if (label)
             mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,label);
          return;
      }

      code = GET_CODE (tem);
      mode = GET_MODE (tem);
      op0 = XEXP (tem, 0);
      op1 = XEXP (tem, 1);
      unsignedp = (code == GTU || code == LTU || code == GEU || code == LEU);
  }

  if (! if_true_label)
    dummy_label = if_true_label =mtcs_rtl_gen_label_rtx(mtcsRTL);

  scalar_int_mode int_mode;
  if (mtcs_mode_is_int_mode/*!is_int_mode*/(mtcsMode,mode, &int_mode)
      && ! mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,code, int_mode, ccp_jump)){
    n_debug("mtcsdojump.c mtcs_dojump_do_compare_rtx_and_jump 55 code:%d\n",code);

      switch (code){
        case LTU:
          do_jump_by_parts_greater_rtx (self,int_mode, 1, op1, op0,if_false_label, if_true_label, prob);
          break;

        case LEU:
          do_jump_by_parts_greater_rtx (self,int_mode, 1, op0, op1,if_true_label, if_false_label,prob.invert ());
          break;

        case GTU:
          do_jump_by_parts_greater_rtx (self,int_mode, 1, op0, op1,if_false_label, if_true_label, prob);
          break;

        case GEU:
          do_jump_by_parts_greater_rtx (self,int_mode, 1, op1, op0,if_true_label, if_false_label,prob.invert ());
          break;

        case LT:
          do_jump_by_parts_greater_rtx (self,int_mode, 0, op1, op0,if_false_label, if_true_label, prob);
          break;

        case LE:
          do_jump_by_parts_greater_rtx (self,int_mode, 0, op0, op1,if_true_label, if_false_label,prob.invert ());
          break;

        case GT:
          do_jump_by_parts_greater_rtx (self,int_mode, 0, op0, op1, if_false_label, if_true_label, prob);
          break;

        case GE:
          do_jump_by_parts_greater_rtx (self,int_mode, 0, op1, op0,if_true_label, if_false_label, prob.invert ());
          break;

        case EQ:
          do_jump_by_parts_equality_rtx (self,int_mode, op0, op1, if_false_label,if_true_label, prob);
          break;

        case NE:
          do_jump_by_parts_equality_rtx (self,int_mode, op0, op1, if_true_label,if_false_label,prob.invert ());
          break;

        default:
          gcc_unreachable ();
      }
  }else{
      if (mtcs_mode_is_scalar_float_p(mtcsMode,mode)
        && ! mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,code, mode, ccp_jump)
        && mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,swap_condition (code), mode, ccp_jump)){
        n_debug("mtcsdojump.c mtcs_dojump_do_compare_rtx_and_jump 66 code:%d\n",code);
          code = swap_condition (code);
          std::swap (op0, op1);
      }else if (mtcs_mode_is_scalar_float_p(mtcsMode,mode)
           && ! mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,code, mode, ccp_jump)
           /* Never split ORDERED and UNORDERED.
          These must be implemented.  */
           && (code != ORDERED && code != UNORDERED)
               /* Split a floating-point comparison if
          we can jump on other conditions...  */
           && (mtcs_optabs_have_insn_for/*!have_insn_for*/(mtcsOptabs,COMPARE, mode)
               /* ... or if there is no libcall for it.  */
               || code_to_optab (code) == unknown_optab)){
        n_debug("mtcsdojump.c mtcs_dojump_do_compare_rtx_and_jump 77 code:%d\n",code);

          enum rtx_code first_code, orig_code = code;
          bool and_them = split_comparison (code, mode, &first_code, &code);

          /* If there are no NaNs, the first comparison should always fall
             through.  */
          if (!mtcs_mode_honor_nans/*!HONOR_NANS*/ (mtcsMode,mode)){
            n_debug("mtcsdojump.c mtcs_dojump_do_compare_rtx_and_jump 88 code:%d\n",code);

            gcc_assert (first_code == (and_them ? ORDERED : UNORDERED));
          }else if ((orig_code == EQ || orig_code == NE) && rtx_equal_p (op0, op1)){
            n_debug("mtcsdojump.c mtcs_dojump_do_compare_rtx_and_jump 99 code:%d\n",code);

            /* Self-comparisons x == x or x != x can be optimized into
               just x ord x or x nord x.  */
            code = orig_code == EQ ? ORDERED : UNORDERED;
          }else{
            n_debug("mtcsdojump.c mtcs_dojump_do_compare_rtx_and_jump 100 code:%d\n",code);

              profile_probability cprob = profile_probability::guessed_always ();
              if (first_code == UNORDERED)
                  cprob /= 100;
              else if (first_code == ORDERED)
                  cprob = cprob.apply_scale (99, 100);
              else
                  cprob = profile_probability::even ();
              /* For and_them we want to split:
             if (x) goto t; // prob;
             goto f;
             into
             if (a) ; else goto f; // first_prob for ;
                           // 1 - first_prob for goto f;
             if (b) goto t; // adjusted prob;
             goto f;
             such that the overall probability of jumping to t
             remains the same.  The and_them case should be
             probability-wise equivalent to the !and_them case with
             f and t swapped and also the conditions inverted, i.e.
             if (!a) goto f;
             if (!b) goto f;
             goto t;
             where the overall probability of jumping to f is
             1 - prob (thus the first prob.invert () below).
             cprob.invert () is because the a condition is inverted,
             so if it was originally ORDERED, !a is UNORDERED and
             thus should be relative 1% rather than 99%.
             The invert () on assignment to first_prob is because
             first_prob represents the probability of fallthru,
             rather than goto f.  And the last prob.invert () is
             because the adjusted prob represents the probability of
             jumping to t rather than to f.  */
              if (and_them){
                  rtx_code_label *dest_label;
                  prob = prob.invert ();
                  profile_probability first_prob
                    = prob.split (cprob.invert ()).invert ();
                  prob = prob.invert ();
                  /* If we only jump if true, just bypass the second jump.  */
                  if (! if_false_label){
                      if (! dummy_label)
                        dummy_label =mtcs_rtl_gen_label_rtx(mtcsRTL);
                      dest_label = dummy_label;
                  }else
                    dest_label = if_false_label;

                  mtcs_dojump_do_compare_rtx_and_jump (self,op0, op1, first_code, unsignedp,
                               val, mode, size, dest_label, NULL,first_prob);
              }
              /* For !and_them we want to split:
             if (x) goto t; // prob;
             goto f;
             into
             if (a) goto t; // first_prob;
             if (b) goto t; // adjusted prob;
             goto f;
             such that the overall probability of jumping to t
             remains the same and first_prob is prob * cprob.  */
               else{
                  profile_probability first_prob = prob.split (cprob);
                  mtcs_dojump_do_compare_rtx_and_jump (self,op0, op1, first_code, unsignedp,
                               val, mode, size, NULL,if_true_label, first_prob);
                  if (orig_code == NE && mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,UNEQ, mode, ccp_jump)){
                      /* x != y can be split into x unord y || x ltgt y
                     or x unord y || !(x uneq y).  The latter has the
                     advantage that both comparisons are non-signalling and
                     so there is a higher chance that the RTL optimizations
                     merge the two comparisons into just one.  */
                      code = UNEQ;
                      prob = prob.invert ();
                      if (! if_false_label){
                          if (! dummy_label)
                            dummy_label =mtcs_rtl_gen_label_rtx(mtcsRTL);
                          if_false_label = dummy_label;
                      }
                      std::swap (if_false_label, if_true_label);
                  }
              }
          }
      }

      /* For boolean vectors with less than mode precision
     make sure to fill padding with consistent values.  */
      if (val && VECTOR_BOOLEAN_TYPE_P (TREE_TYPE (val))  && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,mode)){
        n_debug("mtcsdojump.c mtcs_dojump_do_compare_rtx_and_jump 101 code:%d\n",code);

          auto nunits = TYPE_VECTOR_SUBPARTS (TREE_TYPE (val)).to_constant ();
          if (maybe_ne (mtcs_mode_get_precision(mtcsMode,mode), nunits)){
              op0 = mtcs_optabs_expand_binop (mtcsOptabs,mode, and_optab, op0,
                      mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,(HOST_WIDE_INT_1U << nunits) - 1),NULL_RTX, true, OPTAB_WIDEN);
              op1 = mtcs_optabs_expand_binop (mtcsOptabs,mode, and_optab, op1,
                      mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,(HOST_WIDE_INT_1U << nunits) - 1),NULL_RTX, true, OPTAB_WIDEN);
          }
      }
     n_debug("mtcsdojump.c mtcs_dojump_do_compare_rtx_and_jump 102 code:%d\n",code);

      mtcs_optabs_emit_cmp_and_jump_insns (mtcsOptabs,op0, op1, code, size, mode, unsignedp, val,if_true_label, prob);
  }
 n_debug("mtcsdojump.c mtcs_dojump_do_compare_rtx_and_jump 103 code:%d if_false_label:%p\n",code,if_false_label);

   if (if_false_label)
      mtcs_emit_emit_jump(mtcsEmit,if_false_label);
  n_debug("mtcsdojump.c mtcs_dojump_do_compare_rtx_and_jump 104 code:%d if_false_label:%p\n",code,dummy_label);
   if (dummy_label)
      mtcs_emit_emit_label(mtcsEmit,dummy_label);
}

/* Pop any previously-pushed arguments that have not been popped yet.  */
//原型 do_pending_stack_adjust dojump.h dojump.cc
void mtcs_dojump_do_pending_stack_adjust (MtcsDojump *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  if (mtcsRtlData->expr.x_inhibit_defer_pop/*!inhibit_defer_pop*/ == 0){
     n_debug("mtcsdojump.c mtcs_dojump_do_pending_stack_adjust 00\n");
      if (maybe_ne (mtcsRtlData->expr.x_pending_stack_adjust/*!pending_stack_adjust function.h中定义*/, 0))
          mtcs_explow_adjust_stack/*!adjust_stack*/ (mtcsExplow,mtcs_rtl_gen_int_mode (mtcsRTL,
                  mtcsRtlData->expr.x_pending_stack_adjust/*!pending_stack_adjust function.h中定义*/,mtcs_mode_get_Pmode(mtcsMode)));
      mtcsRtlData->expr.x_pending_stack_adjust/*!pending_stack_adjust function.h中定义*/ = 0;
  }
}

/* Split a comparison into two others, the second of which has the other
   "orderedness".  The first is always ORDERED or UNORDERED if MODE
   does not honor NaNs (which means that it can be skipped in that case;
   see do_compare_rtx_and_jump).

   The two conditions are written in *CODE1 and *CODE2.  Return true if
   the conditions must be ANDed, false if they must be ORed.  */
//原型 split_comparison dojump.h dojump.cc
bool mtcs_dojump_split_comparison (MtcsDojump *self,enum rtx_code code, machine_mode mode,enum rtx_code *code1, enum rtx_code *code2)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  switch (code){
    case LT:
      *code1 = ORDERED;
      *code2 = UNLT;
      return true;
    case LE:
      *code1 = ORDERED;
      *code2 = UNLE;
      return true;
    case GT:
      *code1 = ORDERED;
      *code2 = UNGT;
      return true;
    case GE:
      *code1 = ORDERED;
      *code2 = UNGE;
      return true;
    case EQ:
      *code1 = ORDERED;
      *code2 = UNEQ;
      return true;
    case NE:
      *code1 = UNORDERED;
      *code2 = LTGT;
      return false;
    case UNLT:
      *code1 = UNORDERED;
      *code2 = LT;
      return false;
    case UNLE:
      *code1 = UNORDERED;
      *code2 = LE;
      return false;
    case UNGT:
      *code1 = UNORDERED;
      *code2 = GT;
      return false;
    case UNGE:
      *code1 = UNORDERED;
      *code2 = GE;
      return false;
    case UNEQ:
      *code1 = UNORDERED;
      *code2 = EQ;
      return false;
    case LTGT:
      /* Do not turn a trapping comparison into a non-trapping one.  */
      if (mtcs_mode_honor_nans/*!HONOR_NANS*/ (mtcsMode,mode)){
          *code1 = LT;
          *code2 = GT;
          return false;
      }else{
          *code1 = ORDERED;
          *code2 = NE;
          return true;
      }
    default:
      gcc_unreachable ();
  }
}

/* Given a comparison (CODE ARG0 ARG1), inside an insn, INSN, return a code
   of reversed comparison if it is possible to do so.  Otherwise return UNKNOWN.
   UNKNOWN may be returned in case we are having CC_MODE compare and we don't
   know whether it's source is floating point or integer comparison.  Machine
   description should define REVERSIBLE_CC_MODE and REVERSE_CONDITION macros
   to help this function avoid overhead in these cases.  */

//原型 reversed_comparison_code_parts rtl.h jump.cc
enum rtx_code mtcs_dojump_reversed_comparison_code_parts (MtcsDojump *self,enum rtx_code code, const_rtx arg0,
                const_rtx arg1, const rtx_insn *insn)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  machine_mode mode;

  /* If this is not actually a comparison, we can't reverse it.  */
  if (GET_RTX_CLASS (code) != RTX_COMPARE && GET_RTX_CLASS (code) != RTX_COMM_COMPARE)
    return UNKNOWN;

  mode = GET_MODE (arg0);
  if (mode == VOIDmode)
    mode = GET_MODE (arg1);

  /* First see if machine description supplies us way to reverse the
     comparison.  Give it priority over everything else to allow
     machine description to do tricks.  */
  if (mtcs_mode_get_class(mtcsMode,mode) == MODE_CC  && mtcs_mode_get_reversible_cc_mode/*!REVERSIBLE_CC_MODE*/(mtcsMode,mode))
    return mtcs_dojump_reverse_condition/*!REVERSE_CONDITION*/(self,code, mode);//每个平台的宏REVERSE_CONDITION指向的函数都不一样
  /* Try a few special cases based on the comparison code.  */
  switch (code){
    case GEU:
    case GTU:
    case LEU:
    case LTU:
    case NE:
    case EQ:
      /* It is always safe to reverse EQ and NE, even for the floating
     point.  Similarly the unsigned comparisons are never used for
     floating point so we can reverse them in the default way.  */
      return reverse_condition (code);
    case ORDERED:
    case UNORDERED:
    case LTGT:
    case UNEQ:
      /* In case we already see unordered comparison, we can be sure to
     be dealing with floating point so we don't need any more tests.  */
      return reverse_condition_maybe_unordered (code);
    case UNLT:
    case UNLE:
    case UNGT:
    case UNGE:
      /* We don't have safe way to reverse these yet.  */
      return UNKNOWN;
    default:
      break;
  }

  if (mtcs_mode_get_class(mtcsMode,mode) == MODE_CC){
      /* Try to search for the comparison to determine the real mode.
         This code is expensive, but with sane machine description it
         will be never used, since REVERSIBLE_CC_MODE will return true
         in all cases.  */
      if (! insn)
          return UNKNOWN;

      /* These CONST_CAST's are okay because prev_nonnote_insn just
     returns its argument and we assign it to a const_rtx
     variable.  */
      for (rtx_insn *prev = prev_nonnote_insn (const_cast<rtx_insn *> (insn));
       prev != 0 && !LABEL_P (prev);prev = prev_nonnote_insn (prev)){
          const_rtx set = mtcs_rtlanal_set_of/*!set_of*/(mtcsRtlanal,arg0, prev);
          if (set && GET_CODE (set) == SET && rtx_equal_p (SET_DEST (set), arg0)){
              rtx src = SET_SRC (set);
              if (GET_CODE (src) == COMPARE) {
                  rtx comparison = src;
                  arg0 = XEXP (src, 0);
                  mode = GET_MODE (arg0);
                  if (mode == VOIDmode)
                    mode = GET_MODE (XEXP (comparison, 1));
                  break;
              }
              /* We can get past reg-reg moves.  This may be useful for model
                 of i387 comparisons that first move flag registers around.  */
              if (REG_P (src)){
                  arg0 = src;
                  continue;
              }
          }
          /* If register is clobbered in some ununderstandable way,
             give up.  */
          if (set)
            return UNKNOWN;
      }
  }

  /* Test for an integer condition, or a floating-point comparison
     in which NaNs can be ignored.  */
  if (CONST_INT_P (arg0)|| (GET_MODE (arg0) != VOIDmode
      && mtcs_mode_get_class(mtcsMode,mode) != MODE_CC && !mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,mode)))
    return reverse_condition (code);

  return UNKNOWN;
}

enum rtx_code mtcs_dojump_reverse_condition(MtcsDojump *self,enum rtx_code code, machine_mode mode)
{
    return self->reverse_condition(self,code,mode);
}

/* Discard any pending stack adjustment.  This avoid relying on the
   RTL optimizers to remove useless adjustments when we know the
   stack pointer value is dead.  */
//原型 discard_pending_stack_adjust dojump.h dojump.cc
void mtcs_dojump_discard_pending_stack_adjust (MtcsDojump *self)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
    MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

    mtcsRtlData->expr.x_stack_pointer_delta/*!stack_pointer_delta*/ -=  mtcsRtlData->expr.x_pending_stack_adjust/*!pending_stack_adjust*/;
    mtcsRtlData->expr.x_pending_stack_adjust/*!pending_stack_adjust*/ = 0;
}

/* Restore the saved pending_stack_adjust/stack_pointer_delta.  */
//原型 restore_pending_stack_adjust dojump.j dojump.cc
void mtcs_dojump_restore_pending_stack_adjust (MtcsDojump *self,saved_pending_stack_adjust *save)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  if (mtcsRtlData->expr.x_inhibit_defer_pop/*!inhibit_defer_pop*/ == 0){
      mtcsRtlData->expr.x_pending_stack_adjust/*!pending_stack_adjust*/   = save->x_pending_stack_adjust;
      mtcsRtlData->expr.x_stack_pointer_delta/*!stack_pointer_delta*/ = save->x_stack_pointer_delta;
  }
}

/* Generate code to evaluate EXP and jump to LABEL if the value is zero.
   PROB is probability of jump to LABEL.  */
//原型 jumpifnot dojump.h dojump.cc
void mtcs_dojump_jumpifnot (MtcsDojump *self,tree exp, rtx_code_label *label, profile_probability prob)
{
  do_jump(self,exp, label, NULL, prob.invert ());
}

/* Generate code to evaluate EXP and jump to LABEL if the value is nonzero.
   PROB is probability of jump to LABEL.  */
//原型 jumpif dojump.h dojump.cc
void mtcs_dojump_jumpif (MtcsDojump *self,tree exp, rtx_code_label *label, profile_probability prob)
{
  do_jump (self,exp, NULL, label, prob);
}

/* When exiting from function, if safe, clear out any pending stack adjust
   so the adjustment won't get done.

   Note, if the current function calls alloca, then it must have a
   frame pointer regardless of the value of flag_omit_frame_pointer.  */
//原型 clear_pending_stack_adjust dojump.h dojump.cc
void mtcs_dojump_clear_pending_stack_adjust (MtcsDojump *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *opts=mtcsOptions->global_options;

  if (opts->x_optimize > 0
      && (! opts->x_flag_omit_frame_pointer || cfun->calls_alloca)
      &&  mtcs_func_get_exit_ignore_stack/*!EXIT_IGNORE_STACK*/(mtcsFunc))
      mtcs_dojump_discard_pending_stack_adjust/*!discard_pending_stack_adjust*/(self);
}

/* Similar to jumpifnot but dealing with exploded comparisons of the type
   OP0 CODE OP1 .  LABEL and PROB are like in jumpifnot.  */
//原型 jumpifnot_1 dojump.h dojump.cc
void mtcs_dojump_jumpifnot_1 (MtcsDojump *self,enum tree_code code, tree op0, tree op1, rtx_code_label *label,
         profile_probability prob)
{
  do_jump_1(self,code, op0, op1, label, NULL, prob.invert ());
}


/* Similar to jumpif but dealing with exploded comparisons of the type
   OP0 CODE OP1 .  LABEL and PROB are like in jumpif.  */
//原型 jumpif_1 dojump.h dojump.cc
void mtcs_dojump_jumpif_1 (MtcsDojump *self,enum tree_code code, tree op0, tree op1, rtx_code_label *label,
      profile_probability prob)
{
   do_jump_1 (self,code, op0, op1, NULL, label, prob);
}

/* Delete insn INSN from the chain of insns and update label ref counts
   and delete insns now unreachable.

   Returns the first insn after INSN that was not deleted.

   Usage of this instruction is deprecated.  Use delete_insn instead and
   subsequent cfg_cleanup pass to delete unreachable code if needed.  */
//原型 delete_related_insns rtl.h jump.cc
rtx_insn * mtcs_dojump_delete_related_insns (MtcsDojump *self,rtx uncast_insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);

   rtx_insn *insn = as_a <rtx_insn *> (uncast_insn);
   bool was_code_label = LABEL_P (insn);
   rtx note;
   rtx_insn *next = NEXT_INSN (insn), *prev = PREV_INSN (insn);

   while (next && next->deleted ())
      next = NEXT_INSN (next);
   /* This insn is already deleted => return first following nondeleted.  */
   if (insn->deleted ())
      return next;

   mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,insn);
   /* If instruction is followed by a barrier,
   delete the barrier too.  */
   if (next != 0 && BARRIER_P (next))
      mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,next);
   /* If deleting a jump, decrement the count of the label,
   and delete the label if it is now unused.  */
   if (jump_to_label_p (insn)){
      rtx lab = JUMP_LABEL (insn);
      rtx_jump_table_data *lab_next;
      if (LABEL_NUSES (lab) == 0)
         /* This can delete NEXT or PREV,
         either directly if NEXT is JUMP_LABEL (INSN),
         or indirectly through more levels of jumps.  */
         mtcs_dojump_delete_related_insns/*!delete_related_insns*/(self,lab);
      else if (tablejump_p (insn, NULL, &lab_next)){
         /* If we're deleting the tablejump, delete the dispatch table.
         We may not be able to kill the label immediately preceding
         just yet, as it might be referenced in code leading up to
         the tablejump.  */
         mtcs_dojump_delete_related_insns/*!delete_related_insns*/(self,lab_next);
      }
   }
   /* Likewise if we're deleting a dispatch table.  */
   if (rtx_jump_table_data *table = dyn_cast <rtx_jump_table_data *> (insn)){
      rtvec labels = table->get_labels ();
      int i;
      int len = GET_NUM_ELEM (labels);

      for (i = 0; i < len; i++)
         if (LABEL_NUSES (XEXP (RTVEC_ELT (labels, i), 0)) == 0)
            mtcs_dojump_delete_related_insns/*!delete_related_insns*/(self,XEXP (RTVEC_ELT (labels, i), 0));
      while (next && next->deleted ())
         next = NEXT_INSN (next);
      return next;
   }

   /* Likewise for any JUMP_P / INSN / CALL_INSN with a
   REG_LABEL_OPERAND or REG_LABEL_TARGET note.  */
   if (INSN_P (insn))
      for (note = REG_NOTES (insn); note; note = XEXP (note, 1))
         if ((REG_NOTE_KIND (note) == REG_LABEL_OPERAND
         || REG_NOTE_KIND (note) == REG_LABEL_TARGET)
         /* This could also be a NOTE_INSN_DELETED_LABEL note.  */
         && LABEL_P (XEXP (note, 0)))
            if (LABEL_NUSES (XEXP (note, 0)) == 0)
               mtcs_dojump_delete_related_insns/*!delete_related_insns*/(self,XEXP (note, 0));

   while (prev && (prev->deleted () || NOTE_P (prev)))
      prev = PREV_INSN (prev);
   /* If INSN was a label and a dispatch table follows it,
   delete the dispatch table.  The tablejump must have gone already.
   It isn't useful to fall through into a table.  */
   if (was_code_label && NEXT_INSN (insn) != 0 && JUMP_TABLE_DATA_P (NEXT_INSN (insn)))
      next = mtcs_dojump_delete_related_insns/*!delete_related_insns*/(self,NEXT_INSN (insn));
   /* If INSN was a label, delete insns following it if now unreachable.  */
   if (was_code_label && prev && BARRIER_P (prev)){
      enum rtx_code code;
      while (next){
         code = GET_CODE (next);
         if (code == NOTE)
            next = NEXT_INSN (next);
         /* Keep going past other deleted labels to delete what follows.  */
         else if (code == CODE_LABEL && next->deleted ())
            next = NEXT_INSN (next);
            /* Keep the (use (insn))s created by dbr_schedule, which needs
            them in order to track liveness relative to a previous
            barrier.  */
         else if (INSN_P (next)  && GET_CODE (PATTERN (next)) == USE && INSN_P (XEXP (PATTERN (next), 0)))
            next = NEXT_INSN (next);
         else if (code == BARRIER || INSN_P (next))
            /* Note: if this deletes a jump, it can cause more
            deletion of unreachable code, after a different label.
            As long as the value from this recursive call is correct,
            this invocation functions correctly.  */
            next = mtcs_dojump_delete_related_insns/*!delete_related_insns*/(self,next);
         else
            break;
      }
   }
   /* I feel a little doubtful about this loop,
   but I see no clean and sure alternative way
   to find the first insn after INSN that is not now deleted.
   I hope this works.  */
   while (next && next->deleted ())
      next = NEXT_INSN (next);
   return next;
}

/* A helper function for redirect_exp_1; examines its input X and returns
   either a LABEL_REF around a label, or a RETURN if X was NULL.  */
//原型 redirect_target jump.cc
static rtx redirect_target (MtcsDojump *self,rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   if (x == NULL_RTX)
      return ret_rtx;
   if (!ANY_RETURN_P (x))
      return gen_rtx_LABEL_REF (mtcs_mode_get_Pmode(mtcsMode), x);
   return x;
}

/* Throughout LOC, redirect OLABEL to NLABEL.  Treat null OLABEL or
   NLABEL as a return.  Accrue modifications into the change group.  */
//原型 redirect_exp_1 jump.cc
static void redirect_exp_1 (MtcsDojump *self,rtx *loc, rtx olabel, rtx nlabel, rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   rtx x = *loc;
   RTX_CODE code = GET_CODE (x);
   int i;
   const char *fmt;

   if ((code == LABEL_REF && label_ref_label (x) == olabel)  || x == olabel){
      x = redirect_target(self,nlabel);
      if (GET_CODE (x) == LABEL_REF && loc == &PATTERN (insn))
         x = gen_rtx_SET (pc_rtx, x);
      mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, loc, x, 1);
      return;
   }

   if (code == SET && SET_DEST (x) == pc_rtx
   && ANY_RETURN_P (nlabel)
   && GET_CODE (SET_SRC (x)) == LABEL_REF
   && label_ref_label (SET_SRC (x)) == olabel){
      mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, loc, nlabel, 1);
      return;
   }

   if (code == IF_THEN_ELSE){
      /* Skip the condition of an IF_THEN_ELSE.  We only want to
      change jump destinations, not eventual label comparisons.  */
      redirect_exp_1(self,&XEXP (x, 1), olabel, nlabel, insn);
      redirect_exp_1(self,&XEXP (x, 2), olabel, nlabel, insn);
      return;
   }

   fmt = GET_RTX_FORMAT (code);
   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--){
      if (fmt[i] == 'e')
         redirect_exp_1(self,&XEXP (x, i), olabel, nlabel, insn);
      else if (fmt[i] == 'E'){
         int j;
         for (j = 0; j < XVECLEN (x, i); j++)
            redirect_exp_1(self,&XVECEXP (x, i, j), olabel, nlabel, insn);
      }
   }
}

/* Make JUMP go to NLABEL instead of where it jumps now.  Accrue
   the modifications into the change group.  Return false if we did
   not see how to do that.  */
//原型 redirect_jump_1 rtl.h jump.cc
bool mtcs_dojump_redirect_jump_1 (MtcsDojump *self,rtx_insn *jump, rtx nlabel)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   int ochanges = mtcs_recog_num_validated_changes/*!num_validated_changes*/(mtcsRecog);
   rtx *loc, asmop;

   gcc_assert (nlabel != NULL_RTX);
   asmop = extract_asm_operands (PATTERN (jump));
   if (asmop){
      if (nlabel == NULL)
         return false;
      gcc_assert (ASM_OPERANDS_LABEL_LENGTH (asmop) == 1);
      loc = &ASM_OPERANDS_LABEL (asmop, 0);
   }else if (GET_CODE (PATTERN (jump)) == PARALLEL)
      loc = &XVECEXP (PATTERN (jump), 0, 0);
   else
      loc = &PATTERN (jump);

   redirect_exp_1(self,loc, JUMP_LABEL (jump), nlabel, jump);
   return mtcs_recog_num_validated_changes/*!num_validated_changes*/(mtcsRecog) > ochanges;
}

/* Make JUMP go to NLABEL instead of where it jumps now.  If the old
   jump target label is unused as a result, it and the code following
   it may be deleted.

   Normally, NLABEL will be a label, but it may also be a RETURN rtx;
   in that case we are to turn the jump into a (possibly conditional)
   return insn.

   The return value will be true if the change was made, false if it wasn't
   (this can only occur when trying to produce return insns).  */
//原型 redirect_jump rtl.h jump.cc
bool mtcs_dojump_redirect_jump (MtcsDojump *self,rtx_jump_insn *jump, rtx nlabel, int delete_unused)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   rtx olabel = jump->jump_label ();

   if (!nlabel){
      /* If there is no label, we are asked to redirect to the EXIT block.
      When before the epilogue is emitted, return/simple_return cannot be
      created so we return false immediately.  After the epilogue
      is emitted, we always expect a label, either a non-null label, or a
      return/simple_return RTX.  */

      if (!epilogue_completed)
         return false;
      gcc_unreachable ();
   }

   if (nlabel == olabel)
      return true;

   if (! mtcs_dojump_redirect_jump_1/*!redirect_jump_1*/(self,jump, nlabel)
         || ! mtcs_recog_apply_change_group/*!apply_change_group*/(mtcsRecog))
      return false;

   mtcs_dojump_redirect_jump_2/*!redirect_jump_2*/(self,jump, olabel, nlabel, delete_unused, 0);
   return true;
}

/* A wrapper around the previous function to take COMPARISON as rtx
   expression.  This simplifies many callers.  */
//原型 reversed_comparison_code rtl.h jump.cc
enum rtx_code mtcs_dojump_reversed_comparison_code (MtcsDojump *self,const_rtx comparison, const rtx_insn *insn)
{
   if (!COMPARISON_P (comparison))
      return UNKNOWN;
   return mtcs_dojump_reversed_comparison_code_parts/*!reversed_comparison_code_parts*/(self,
               GET_CODE (comparison),XEXP (comparison, 0), XEXP (comparison, 1), insn);
}

/* Invert the jump condition X contained in jump insn INSN.  Accrue the
   modifications into the change group.  Return true for success.  */
//原型 invert_exp_1 jump.cc
static bool invert_exp_1 (MtcsDojump *self,rtx x, rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   RTX_CODE code = GET_CODE (x);

   if (code == IF_THEN_ELSE){
      rtx comp = XEXP (x, 0);
      rtx tem;
      enum rtx_code reversed_code;

      /* We can do this in two ways:  The preferable way, which can only
      be done if this is not an integer comparison, is to reverse
      the comparison code.  Otherwise, swap the THEN-part and ELSE-part
      of the IF_THEN_ELSE.  If we can't do either, fail.  */

      reversed_code = mtcs_dojump_reversed_comparison_code/*!reversed_comparison_code*/(self,comp, insn);

      if (reversed_code != UNKNOWN){
         mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, &XEXP (x, 0),
               gen_rtx_fmt_ee (reversed_code, GET_MODE (comp), XEXP (comp, 0), XEXP (comp, 1)),1);
         return true;
      }

      tem = XEXP (x, 1);
      mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, &XEXP (x, 1), XEXP (x, 2), 1);
      mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, &XEXP (x, 2), tem, 1);
      return true;
   }else
      return false;
}


/* Invert the condition of the jump JUMP, and make it jump to label
   NLABEL instead of where it jumps now.  Accrue changes into the
   change group.  Return false if we didn't see how to perform the
   inversion and redirection.  */
//原型 invert_jump_1 rtl.h jump.cc
bool mtcs_dojump_invert_jump_1 (MtcsDojump *self,rtx_jump_insn *jump, rtx nlabel)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   rtx x = pc_set (jump);
   int ochanges;
   bool ok;

   ochanges = mtcs_recog_num_validated_changes/*!num_validated_changes*/(mtcsRecog);
   if (x == NULL)
      return false;
   ok = invert_exp_1(self,SET_SRC (x), jump);
   gcc_assert (ok);

   if (mtcs_recog_num_validated_changes/*!num_validated_changes*/(mtcsRecog) == ochanges)
      return false;

   /* redirect_jump_1 will fail of nlabel == olabel, and the current use is
   in Pmode, so checking this is not merely an optimization.  */
   return nlabel == JUMP_LABEL (jump) || mtcs_dojump_redirect_jump_1/*!redirect_jump_1*/(self,jump, nlabel);
}

/* Invert the condition of the jump JUMP, and make it jump to label
   NLABEL instead of where it jumps now.  Return true if successful.  */
//原型 invert_jump rtl.h jump.cc
bool mtcs_dojump_invert_jump (MtcsDojump *self,rtx_jump_insn *jump, rtx nlabel, int delete_unused)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   rtx olabel = JUMP_LABEL (jump);

   if (mtcs_dojump_invert_jump_1/*!invert_jump_1*/(self,jump, nlabel)
         && mtcs_recog_apply_change_group/*!apply_change_group*/(mtcsRecog)){
      mtcs_dojump_redirect_jump_2/*!redirect_jump_2*/(self,jump, olabel, nlabel, delete_unused, 1);
      return true;
   }
   mtcs_recog_cancel_changes/*!cancel_changes*/(mtcsRecog,0);
   return false;
}

/* Fix up JUMP_LABEL and label ref counts after OLABEL has been replaced with
   NLABEL in JUMP.
   If DELETE_UNUSED is positive, delete related insn to OLABEL if its ref
   count has dropped to zero.  */
//原型 redirect_jump_2 rtl.h jump.cc
void mtcs_dojump_redirect_jump_2 (MtcsDojump *self,rtx_jump_insn *jump, rtx olabel,
      rtx nlabel, int delete_unused,int invert)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   rtx note;

   gcc_assert (JUMP_LABEL (jump) == olabel);

   /* Negative DELETE_UNUSED used to be used to signalize behavior on
   moving FUNCTION_END note.  Just sanity check that no user still worry
   about this.  */
   gcc_assert (delete_unused >= 0);
   JUMP_LABEL (jump) = nlabel;
   if (!ANY_RETURN_P (nlabel))
      ++LABEL_NUSES (nlabel);

   /* Update labels in any REG_EQUAL note.  */
   if ((note = find_reg_note (jump, REG_EQUAL, NULL_RTX)) != NULL_RTX){
      if (ANY_RETURN_P (nlabel)
      || (invert && !invert_exp_1(self,XEXP (note, 0), jump)))
         mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,jump, note);
      else{
         redirect_exp_1(self,&XEXP (note, 0), olabel, nlabel, jump);
         mtcs_recog_confirm_change_group/*!confirm_change_group*/(mtcsRecog);
      }
   }

   /* Handle the case where we had a conditional crossing jump to a return
   label and are now changing it into a direct conditional return.
   The jump is no longer crossing in that case.  */
   if (ANY_RETURN_P (nlabel))
      CROSSING_JUMP_P (jump) = 0;

   if (!ANY_RETURN_P (olabel) && --LABEL_NUSES (olabel) == 0 && delete_unused > 0
   /* Undefined labels will remain outside the insn stream.  */
   && INSN_UID (olabel))
      mtcs_dojump_delete_related_insns/*!delete_related_insns*/(self,olabel);
   if (invert)
      invert_br_probabilities (jump);
}

/* Initialize LABEL_NUSES and JUMP_LABEL fields, add REG_LABEL_TARGET
   for remaining targets for JUMP_P.  Delete any REG_LABEL_OPERAND
   notes whose labels don't occur in the insn any more.  */
//原型 init_label_info jump.cc
static void init_label_info (MtcsDojump *self,rtx_insn *f)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   rtx_insn *insn;

   for (insn = f; insn; insn = NEXT_INSN (insn)){
      if (LABEL_P (insn))
         LABEL_NUSES (insn) = (LABEL_PRESERVE_P (insn) != 0);

      /* REG_LABEL_TARGET notes (including the JUMP_LABEL field) are
      sticky and not reset here; that way we won't lose association
      with a label when e.g. the source for a target register
      disappears out of reach for targets that may use jump-target
      registers.  Jump transformations are supposed to transform
      any REG_LABEL_TARGET notes.  The target label reference in a
      branch may disappear from the branch (and from the
      instruction before it) for other reasons, like register
      allocation.  */

      if (INSN_P (insn)){
         rtx note, next;

         for (note = REG_NOTES (insn); note; note = next){
            next = XEXP (note, 1);
            if (REG_NOTE_KIND (note) == REG_LABEL_OPERAND  && ! reg_mentioned_p (XEXP (note, 0), PATTERN (insn)))
               mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,insn, note);
         }
      }
   }
}

/* A subroutine of mark_all_labels.  Trivially propagate a simple label
   load into a jump_insn that uses it.  */
static void maybe_propagate_label_ref (MtcsDojump *self,rtx_insn *jump_insn, rtx_insn *prev_nonjump_insn)
{
   rtx label_note, pc, pc_src;

   pc = pc_set (jump_insn);
   pc_src = pc != NULL ? SET_SRC (pc) : NULL;
   label_note = find_reg_note (prev_nonjump_insn, REG_LABEL_OPERAND, NULL);

   /* If the previous non-jump insn sets something to a label,
   something that this jump insn uses, make that label the primary
   target of this insn if we don't yet have any.  That previous
   insn must be a single_set and not refer to more than one label.
   The jump insn must not refer to other labels as jump targets
   and must be a plain (set (pc) ...), maybe in a parallel, and
   may refer to the item being set only directly or as one of the
   arms in an IF_THEN_ELSE.  */

   if (label_note != NULL && pc_src != NULL){
      rtx label_set = single_set (prev_nonjump_insn);
      rtx label_dest = label_set != NULL ? SET_DEST (label_set) : NULL;

      if (label_set != NULL
      /* The source must be the direct LABEL_REF, not a
      PLUS, UNSPEC, IF_THEN_ELSE etc.  */
      && GET_CODE (SET_SRC (label_set)) == LABEL_REF
      && (rtx_equal_p (label_dest, pc_src)
      || (GET_CODE (pc_src) == IF_THEN_ELSE
      && (rtx_equal_p (label_dest, XEXP (pc_src, 1))
      || rtx_equal_p (label_dest, XEXP (pc_src, 2)))))){
         /* The CODE_LABEL referred to in the note must be the
         CODE_LABEL in the LABEL_REF of the "set".  We can
         conveniently use it for the marker function, which
         requires a LABEL_REF wrapping.  */
         gcc_assert (XEXP (label_note, 0) == label_ref_label (SET_SRC (label_set)));

         mark_jump_label_1(self,label_set, jump_insn, false, true);

         gcc_assert (JUMP_LABEL (jump_insn) == XEXP (label_note, 0));
      }
   }
}


/* Mark the label each jump jumps to.
   Combine consecutive labels, and count uses of labels.  */
//原型 mark_all_labels jump.cc
static void mark_all_labels (MtcsDojump *self,rtx_insn *f)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);

   rtx_insn *insn;

   if (mtcs_cfg_context_get_state/*!current_ir_type ()*/(mtcsCfgContext)== IR_RTL_CFGLAYOUT){
      basic_block bb;
      FOR_EACH_BB_FN (bb, cfun){
         /* In cfglayout mode, we don't bother with trivial next-insn
         propagation of LABEL_REFs into JUMP_LABEL.  This will be
         handled by other optimizers using better algorithms.  */
         FOR_BB_INSNS (bb, insn){
            gcc_assert (! insn->deleted ());
            if (NONDEBUG_INSN_P (insn))
               mtcs_dojump_mark_jump_label/*!mark_jump_label*/(self,PATTERN (insn), insn, 0);
         }

         /* In cfglayout mode, there may be non-insns between the
         basic blocks.  If those non-insns represent tablejump data,
         they contain label references that we must record.  */
         for (insn = BB_HEADER (bb); insn; insn = NEXT_INSN (insn))
            if (JUMP_TABLE_DATA_P (insn))
               mtcs_dojump_mark_jump_label/*!mark_jump_label*/(self,PATTERN (insn), insn, 0);
         for (insn = BB_FOOTER (bb); insn; insn = NEXT_INSN (insn))
            if (JUMP_TABLE_DATA_P (insn))
               mtcs_dojump_mark_jump_label/*!mark_jump_label*/(self,PATTERN (insn), insn, 0);
      }
   }else{
      rtx_insn *prev_nonjump_insn = NULL;
      for (insn = f; insn; insn = NEXT_INSN (insn)){
         if (insn->deleted ())
            ;
         else if (LABEL_P (insn))
            prev_nonjump_insn = NULL;
         else if (JUMP_TABLE_DATA_P (insn))
            mtcs_dojump_mark_jump_label/*!mark_jump_label*/(self,PATTERN (insn), insn, 0);
         else if (NONDEBUG_INSN_P (insn)){
            mtcs_dojump_mark_jump_label/*!mark_jump_label*/(self,PATTERN (insn), insn, 0);
            if (JUMP_P (insn)){
               if (JUMP_LABEL (insn) == NULL && prev_nonjump_insn != NULL)
                  maybe_propagate_label_ref(self,insn, prev_nonjump_insn);
            }else
               prev_nonjump_insn = insn;
         }
      }
   }
}

/* Worker for rebuild_jump_labels and rebuild_jump_labels_chain.  */
//原型 rebuild_jump_labels_1 jump.cc
static void rebuild_jump_labels_1 (MtcsDojump *self,rtx_insn *f, bool count_forced)
{
   init_label_info(self,f);
   mark_all_labels(self,f);
   /* Keep track of labels used from static data; we don't track them
   closely enough to delete them here, so make sure their reference
   count doesn't drop to zero.  */
   if (count_forced){
      rtx_insn *insn;
      unsigned int i;
      FOR_EACH_VEC_SAFE_ELT (forced_labels, i, insn)
         if (LABEL_P (insn))
            LABEL_NUSES (insn)++;
   }
}

/* This function rebuilds the JUMP_LABEL field and REG_LABEL_TARGET
   notes in jumping insns and REG_LABEL_OPERAND notes in non-jumping
   instructions and jumping insns that have labels as operands
   (e.g. cbranchsi4).  */
//原型 rebuild_jump_labels rtl.h jump.cc
void mtcs_dojump_rebuild_jump_labels (MtcsDojump *self,rtx_insn *f)
{
  rebuild_jump_labels_1(self,f, true);
}

/* Worker function for mark_jump_label.  IN_MEM is TRUE when X occurs
   within a (MEM ...).  IS_TARGET is TRUE when X is to be treated as a
   jump-target; when the JUMP_LABEL field of INSN should be set or a
   REG_LABEL_TARGET note should be added, not a REG_LABEL_OPERAND
   note.  */
static void mark_jump_label_1 (MtcsDojump *self,rtx x, rtx_insn *insn, bool in_mem, bool is_target)
{
   RTX_CODE code = GET_CODE (x);
   int i;
   const char *fmt;

   switch (code){
      case PC:
      case REG:
      case CLOBBER:
      case CALL:
         return;

      case RETURN:
      case SIMPLE_RETURN:
         if (is_target){
            gcc_assert (JUMP_LABEL (insn) == NULL || JUMP_LABEL (insn) == x);
            JUMP_LABEL (insn) = x;
         }
         return;

      case MEM:
         in_mem = true;
         break;

      case SEQUENCE:
      {
         rtx_sequence *seq = as_a <rtx_sequence *> (x);
         for (i = 0; i < seq->len (); i++)
            mtcs_dojump_mark_jump_label/*!mark_jump_label*/(self,PATTERN (seq->insn (i)), seq->insn (i), 0);
      }
         return;

      case SYMBOL_REF:
         if (!in_mem)
            return;

         /* If this is a constant-pool reference, see if it is a label.  */
         if (CONSTANT_POOL_ADDRESS_P (x))
            mark_jump_label_1(self,get_pool_constant (x), insn, in_mem, is_target);
         break;

      /* Handle operands in the condition of an if-then-else as for a
      non-jump insn.  */
      case IF_THEN_ELSE:
         if (!is_target)
            break;
         mark_jump_label_1(self,XEXP (x, 0), insn, in_mem, false);
         mark_jump_label_1(self,XEXP (x, 1), insn, in_mem, true);
         mark_jump_label_1(self,XEXP (x, 2), insn, in_mem, true);
         return;

      case LABEL_REF:
      {
         rtx_insn *label = label_ref_label (x);

         /* Ignore remaining references to unreachable labels that
         have been deleted.  */
         if (NOTE_P (label)  && NOTE_KIND (label) == NOTE_INSN_DELETED_LABEL)
            break;

         gcc_assert (LABEL_P (label));

         /* Ignore references to labels of containing functions.  */
         if (LABEL_REF_NONLOCAL_P (x))
            break;

         set_label_ref_label (x, label);
         if (! insn || ! insn->deleted ())
            ++LABEL_NUSES (label);

         if (insn){
            if (is_target
            /* Do not change a previous setting of JUMP_LABEL.  If the
            JUMP_LABEL slot is occupied by a different label,
            create a note for this label.  */
            && (JUMP_LABEL (insn) == NULL || JUMP_LABEL (insn) == label))
               JUMP_LABEL (insn) = label;
            else{
               enum reg_note kind   = is_target ? REG_LABEL_TARGET : REG_LABEL_OPERAND;

               /* Add a REG_LABEL_OPERAND or REG_LABEL_TARGET note
               for LABEL unless there already is one.  All uses of
               a label, except for the primary target of a jump,
               must have such a note.  */
               if (! find_reg_note (insn, kind, label))
                  add_reg_note (insn, kind, label);
            }
         }
         return;
      }

      /* Do walk the labels in a vector, but not the first operand of an
      ADDR_DIFF_VEC.  Don't set the JUMP_LABEL of a vector.  */
      case ADDR_VEC:
      case ADDR_DIFF_VEC:
         if (! insn->deleted ()){
            int eltnum = code == ADDR_DIFF_VEC ? 1 : 0;
            for (i = 0; i < XVECLEN (x, eltnum); i++)
               mark_jump_label_1(self,XVECEXP (x, eltnum, i), NULL, in_mem,is_target);
         }
         return;

      default:
         break;
   }

   fmt = GET_RTX_FORMAT (code);
   /* The primary target of a tablejump is the label of the ADDR_VEC,
   which is canonically mentioned *last* in the insn.  To get it
   marked as JUMP_LABEL, we iterate over items in reverse order.  */
   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--){
      if (fmt[i] == 'e')
         mark_jump_label_1(self,XEXP (x, i), insn, in_mem, is_target);
      else if (fmt[i] == 'E'){
         int j;

         for (j = XVECLEN (x, i) - 1; j >= 0; j--)
            mark_jump_label_1(self,XVECEXP (x, i, j), insn, in_mem,is_target);
      }
   }
}

/* Worker function for mark_jump_label.  Handle asm insns specially.
   In particular, output operands need not be considered so we can
   avoid re-scanning the replicated asm_operand.  Also, the asm_labels
   need to be considered targets.  */
static void mark_jump_label_asm (MtcsDojump *self,rtx asmop, rtx_insn *insn)
{
   int i;
   for (i = ASM_OPERANDS_INPUT_LENGTH (asmop) - 1; i >= 0; --i)
      mark_jump_label_1(self,ASM_OPERANDS_INPUT (asmop, i), insn, false, false);
   for (i = ASM_OPERANDS_LABEL_LENGTH (asmop) - 1; i >= 0; --i)
      mark_jump_label_1(self,ASM_OPERANDS_LABEL (asmop, i), insn, false, true);
}


/* Find all CODE_LABELs referred to in X, and increment their use
   counts.  If INSN is a JUMP_INSN and there is at least one
   CODE_LABEL referenced in INSN as a jump target, then store the last
   one in JUMP_LABEL (INSN).  For a tablejump, this must be the label
   for the ADDR_VEC.  Store any other jump targets as REG_LABEL_TARGET
   notes.  If INSN is an INSN or a CALL_INSN or non-target operands of
   a JUMP_INSN, and there is at least one CODE_LABEL referenced in
   INSN, add a REG_LABEL_OPERAND note containing that label to INSN.
   For returnjumps, the JUMP_LABEL will also be set as appropriate.

   Note that two labels separated by a loop-beginning note
   must be kept distinct if we have not yet done loop-optimization,
   because the gap between them is where loop-optimize
   will want to move invariant code to.  CROSS_JUMP tells us
   that loop-optimization is done with.  */
//原型 mark_jump_label rtl.h jump.cc
void mtcs_dojump_mark_jump_label (MtcsDojump *self,rtx x, rtx_insn *insn, int in_mem)
{
   rtx asmop = extract_asm_operands (x);
   if (asmop)
      mark_jump_label_asm(self,asmop, insn);
   else
      mark_jump_label_1(self,x, insn, in_mem != 0, (insn != NULL && x == PATTERN (insn) && JUMP_P (insn)));
}

/* If X is a hard register or equivalent to one or a subregister of one,
   return the hard register number.  If X is a pseudo register that was not
   assigned a hard register, return the pseudo register number.  Otherwise,
   return -1.  Any rtx is valid for X.  */
//原型 true_regnum rtl.h jump.cc
int mtcs_dojump_true_regnum (MtcsDojump *self,const_rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);
   //#if defined (FIND_BASE_TERM) host=1 nvptx=0

   if (REG_P (x)){
      if (REGNO (x) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/  && (lra_in_progress || reg_renumber[REGNO (x)] >= 0))
         return reg_renumber[REGNO (x)];
      return REGNO (x);
   }
   if (GET_CODE (x) == SUBREG){
      int base = mtcs_dojump_true_regnum/*!true_regnum*/(self,SUBREG_REG (x));
      if (base >= 0 && base < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/){
         struct subreg_info info;

         mtcs_rtl_subreg_get_info/*!subreg_get_info*/(mtcsRTL,lra_in_progress
               ? (unsigned) base : REGNO (SUBREG_REG (x)), GET_MODE (SUBREG_REG (x)),SUBREG_BYTE (x), GET_MODE (x), &info);

         if (info.representable_p)
            return base + info.offset;
      }
   }
   return -1;
}

/* Some old code expects exactly one BARRIER as the NEXT_INSN of a
   non-fallthru insn.  This is not generally true, as multiple barriers
   may have crept in, or the BARRIER may be separated from the last
   real insn by one or more NOTEs.

   This simple pass moves barriers and removes duplicates so that the
   old code is happy.
 */
//原型 cleanup_barriers jump.cc
static unsigned int cleanup_barriers (MtcsDojump *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);

   rtx_insn *insn;
   for (insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = NEXT_INSN (insn)){
      if (BARRIER_P (insn)){
         rtx_insn *prev = prev_nonnote_nondebug_insn (insn);
         if (!prev)
            continue;

         if (BARRIER_P (prev))
            mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,insn);
         else if (prev != PREV_INSN (insn)){
            basic_block bb = BLOCK_FOR_INSN (prev);
            rtx_insn *end = PREV_INSN (insn);
            mtcs_rtl_reorder_insns_nobb/*!reorder_insns_nobb*/(mtcsRTL,insn, insn, prev);
            if (bb){
               /* If the backend called in machine reorg compute_bb_for_insn
               and didn't free_bb_for_insn again, preserve basic block
               boundaries.  Move the end of basic block to PREV since
               it is followed by a barrier now, and clear BLOCK_FOR_INSN
               on the following notes.
               ???  Maybe the proper solution for the targets that have
               cfg around after machine reorg is not to run cleanup_barriers
               pass at all.  */
               BB_END (bb) = prev;
               do{
                  prev = NEXT_INSN (prev);
                  if (prev != insn && BLOCK_FOR_INSN (prev) == bb)
                     BLOCK_FOR_INSN (prev) = NULL;
               }while (prev != end);
            }
         }
      }
   }
   return 0;
}

/* This function is like rebuild_jump_labels, but doesn't run over
   forced_labels.  It can be used on insn chains that aren't the
   main function chain.  */
//原型 rebuild_jump_labels_chain rtl.h jump.cc
void mtcs_dojump_rebuild_jump_labels_chain (MtcsDojump *self,rtx_insn *chain)
{
  rebuild_jump_labels_1(self,chain, false);
}

MtcsDojump *mtcs_dojump_new(MtcsMode *mtcsMode)
{
     MtcsDojump *self = n_slice_alloc0 (sizeof(MtcsDojump));
     mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
     mtcsDojumpInit(self);
     return self;
}

//原型 NEXT_PASS (pass_cleanup_barriers, 1);  RTL_PASS  jump.cc  barriers   y 无条件执行 cleanup_barriers
static nuint cleanup_barriers_execute_cb(MtcsPass *mtcsPass,function *func)
{
   MtcsPassCleanupBarriers *self=(MtcsPassCleanupBarriers *)mtcsPass;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);

   return cleanup_barriers(mtcsDojump);
}

static void mtcsPassCleanupBarriersInit(MtcsPassCleanupBarriers *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =cleanup_barriers_execute_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassCleanupBarriers *mtcs_pass_cleanup_barriers_new (MtcsMode *mtcsMode)
{
   MtcsPassCleanupBarriers *self = n_slice_alloc0 (sizeof(MtcsPassCleanupBarriers));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"barriers");
   mtcsPassCleanupBarriersInit(self);
   return self;
}
