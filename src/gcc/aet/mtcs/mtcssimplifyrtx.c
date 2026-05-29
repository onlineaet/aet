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
 * base on simplify-rtx.cc
 */


/* This file handles generation of all the assembler code
   *except* the instructions of a rtxtion.
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

#include "mtcssimplifyrtx.h"
#include "mtcstarget.h"
#include "mtcsmicro.h"
#include "mtcsvectorbuilder.h"
#include "mtcsasm.h"
#include "mtcsrtlanal.h"
#include "mtcsprintrtl.h"

static bool exact_int_to_float_conversion_p (MtcsSimplifyRtx *self,const_rtx op);
static int comparison_to_mask (enum rtx_code comparison);
static rtx comparison_result (enum rtx_code code, int known_results);

static void mtcsSimplifyRtxInit(MtcsSimplifyRtx *self)
{
     self->mem_depth = 0;
     self->assoc_count = 0;
     self->max_assoc_count = 64;
}

enum
{
  CMP_EQ = 1,
  CMP_LT = 2,
  CMP_GT = 4,
  CMP_LTU = 8,
  CMP_GTU = 16
};

/* Convert the known results for EQ, LT, GT, LTU, GTU contained in
   KNOWN_RESULT to a CONST_INT, based on the requested comparison CODE
   For KNOWN_RESULT to make sense it should be either CMP_EQ, or the
   logical OR of one of (CMP_LT, CMP_GT) and one of (CMP_LTU, CMP_GTU).
   For floating-point comparisons, assume that the operands were ordered.  */

static rtx comparison_result (enum rtx_code code, int known_results)
{
  switch (code)
    {
    case EQ:
    case UNEQ:
      return (known_results & CMP_EQ) ? const_true_rtx : const0_rtx;
    case NE:
    case LTGT:
      return (known_results & CMP_EQ) ? const0_rtx : const_true_rtx;

    case LT:
    case UNLT:
      return (known_results & CMP_LT) ? const_true_rtx : const0_rtx;
    case GE:
    case UNGE:
      return (known_results & CMP_LT) ? const0_rtx : const_true_rtx;

    case GT:
    case UNGT:
      return (known_results & CMP_GT) ? const_true_rtx : const0_rtx;
    case LE:
    case UNLE:
      return (known_results & CMP_GT) ? const0_rtx : const_true_rtx;

    case LTU:
      return (known_results & CMP_LTU) ? const_true_rtx : const0_rtx;
    case GEU:
      return (known_results & CMP_LTU) ? const0_rtx : const_true_rtx;

    case GTU:
      return (known_results & CMP_GTU) ? const_true_rtx : const0_rtx;
    case LEU:
      return (known_results & CMP_GTU) ? const0_rtx : const_true_rtx;

    case ORDERED:
      return const_true_rtx;
    case UNORDERED:
      return const0_rtx;
    default:
      gcc_unreachable ();
    }
}

/* Return a mask describing the COMPARISON.  */
static int comparison_to_mask (enum rtx_code comparison)
{
  switch (comparison)
    {
    case LT:
      return 8;
    case GT:
      return 4;
    case EQ:
      return 2;
    case UNORDERED:
      return 1;

    case LTGT:
      return 12;
    case LE:
      return 10;
    case GE:
      return 6;
    case UNLT:
      return 9;
    case UNGT:
      return 5;
    case UNEQ:
      return 3;

    case ORDERED:
      return 14;
    case NE:
      return 13;
    case UNLE:
      return 11;
    case UNGE:
      return 7;

    default:
      gcc_unreachable ();
    }
}

/* Return true if FLOAT or UNSIGNED_FLOAT operation OP is known
   to be exact.  */
static bool exact_int_to_float_conversion_p (MtcsSimplifyRtx *self,const_rtx op)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  machine_mode op0_mode = GET_MODE (XEXP (op, 0));
  /* Constants can reach here with -frounding-math, if they do then
     the conversion isn't exact.  */
  if (op0_mode == VOIDmode)
    return false;
  //不能直接调用significand_size(mode) real.h 中 format_helper 调用 REAL_MODE_FORMAT
  const struct real_format *rf=GET_MODE (op)==VOIDmode?0:mtcs_mode_get_real_format (mtcsMode,
        mtcs_mode_get_inner/*!GET_MODE_INNER*/ (mtcsMode,(mtcs_mode)GET_MODE (op)));
  format_helper help(rf);
  int out_bits = significand_size (help);
  int in_prec = mtcs_mode_get_unit_precision/*!GET_MODE_UNIT_PRECISION*/(mtcsMode,op0_mode);
  int in_bits = in_prec;
  if (mtcs_mode_is_hwi_computable_p/*!HWI_COMPUTABLE_MODE_P*/ (mtcsMode,op0_mode)){
      unsigned HOST_WIDE_INT nonzero = mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(mtcsRtlanal,XEXP (op, 0), op0_mode);
      if (GET_CODE (op) == FLOAT)
          in_bits -= mtcs_rtlanal_num_sign_bit_copies/*!num_sign_bit_copies*/(mtcsRtlanal,XEXP (op, 0), op0_mode);
      else if (GET_CODE (op) == UNSIGNED_FLOAT)
          in_bits = wi::min_precision (wi::uhwi (nonzero, in_prec), UNSIGNED);
      else
          gcc_unreachable ();
      in_bits -= wi::ctz (wi::uhwi (nonzero, in_prec));
  }
  return in_bits <= out_bits;
}

/* Canonicalize RES, a scalar const0_rtx/const_true_rtx to the right
   false/true value of comparison with MODE where comparison operands
   have CMP_MODE.  */

static rtx relational_result (MtcsSimplifyRtx *self,machine_mode mode, machine_mode cmp_mode, rtx res)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);

  if (mtcs_mode_is_scalar_float_p (mtcsMode,mode)){
      if (res == const0_rtx)
        return CONST0_RTX (mode);
      /*
#ifdef FLOAT_STORE_FLAG_VALUE
      REAL_VALUE_TYPE val = FLOAT_STORE_FLAG_VALUE (mode);
      return mtcs_rtl_const_double_from_real_value (mtcsRTL,val, mode);
#else
      return NULL_RTX;
#endif*/
      if(mtcs_config_ifdef(mtcsConfig,MTCS_FLOAT_STORE_FLAG_VALUE)){
          REAL_VALUE_TYPE val = mtcs_real_float_store_flag_value (mtcsReal,mode);
          return mtcs_rtl_const_double_from_real_value/*!const_double_from_real_value*/(mtcsRTL,val, mode);
      }else{
          return NULL_RTX;
      }
  }
  if (mtcs_mode_is_vector_p (mtcsMode,mode)){
      if (res == const0_rtx)
    return CONST0_RTX (mode);
#ifdef VECTOR_STORE_FLAG_VALUE
      rtx val = VECTOR_STORE_FLAG_VALUE (mode);
      if (val == NULL_RTX)
    return NULL_RTX;
      if (val == const1_rtx)
    return CONST1_RTX (mode);

      return gen_const_vec_duplicate (mode, val);
#else
      return NULL_RTX;
#endif
  }
  /* For vector comparison with scalar int result, it is unknown
     if the target means here a comparison into an integral bitmask,
     or comparison where all comparisons true mean const_true_rtx
     whole result, or where any comparisons true mean const_true_rtx
     whole result.  For const0_rtx all the cases are the same.  */
  if (mtcs_mode_is_vector_p (mtcsMode,cmp_mode)
      && mtcs_mode_is_scalar_int_p (mtcsMode,mode)
      && res == const_true_rtx)
    return NULL_RTX;

  return res;
}


/* Return a comparison corresponding to the MASK.  */
static enum rtx_code mask_to_comparison (int mask)
{
  switch (mask)
    {
    case 8:
      return LT;
    case 4:
      return GT;
    case 2:
      return EQ;
    case 1:
      return UNORDERED;

    case 12:
      return LTGT;
    case 10:
      return LE;
    case 6:
      return GE;
    case 9:
      return UNLT;
    case 5:
      return UNGT;
    case 3:
      return UNEQ;

    case 14:
      return ORDERED;
    case 13:
      return NE;
    case 11:
      return UNLE;
    case 7:
      return UNGE;

    default:
      gcc_unreachable ();
    }
}


/* Return true if CODE is valid for comparisons of mode MODE, false
   otherwise.

   It is always safe to return false, even if the code was valid for the
   given mode as that will merely suppress optimizations.  */

static bool comparison_code_valid_for_mode (MtcsSimplifyRtx *self,enum rtx_code code, enum machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  switch (code){
      /* These are valid for integral, floating and vector modes.  */
      case NE:
      case EQ:
      case GE:
      case GT:
      case LE:
      case LT:
          return (mtcs_mode_is_integral_p (mtcsMode,mode) || mtcs_mode_is_float_p (mtcsMode,mode) || mtcs_mode_is_vector_p (mtcsMode,mode));

      /* These are valid for floating point modes.  */
      case LTGT:
      case UNORDERED:
      case ORDERED:
      case UNEQ:
      case UNGE:
      case UNGT:
      case UNLE:
      case UNLT:
          return mtcs_mode_is_float_p (mtcsMode,mode);

      /* These are filtered out in simplify_logical_operation, but
     we check for them too as a matter of safety.   They are valid
     for integral and vector modes.  */
      case GEU:
      case GTU:
      case LEU:
      case LTU:
          return mtcs_mode_is_integral_p (mtcsMode,mode) || mtcs_mode_is_vector_p (mtcsMode,mode);
      default:
          gcc_unreachable ();
  }
}

/* Check whether an operand is suitable for calling simplify_plus_minus.  */
static bool plus_minus_operand_p (const_rtx x)
{
  return GET_CODE (x) == PLUS
         || GET_CODE (x) == MINUS
     || (GET_CODE (x) == CONST
         && GET_CODE (XEXP (x, 0)) == PLUS
         && CONSTANT_P (XEXP (XEXP (x, 0), 0))
         && CONSTANT_P (XEXP (XEXP (x, 0), 1)));
}


static rtx neg_poly_int_rtx (MtcsSimplifyRtx *self,machine_mode mode, const_rtx i)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  return mtcs_rtl_immed_wide_int_const (mtcsRTL,-mtcs_rtl_to_poly_wide/*!wi::to_poly_wide*/(i, mode), (mtcs_mode)mode);

}

/* Return a positive integer if X should sort after Y.  The value
   returned is 1 if and only if X and Y are both regs.  */

static int simplify_plus_minus_op_data_cmp (rtx x, rtx y)
{
  int result;

  result = (commutative_operand_precedence (y)
        - commutative_operand_precedence (x));
  if (result)
    return result + result;

  /* Group together equal REGs to do more simplification.  */
  if (REG_P (x) && REG_P (y))
    return REGNO (x) > REGNO (y);
  return 0;
}


//原型 rtx simplify_context::simplify_gen_unary  rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_gen_unary (MtcsSimplifyRtx *self,rtx_code code, machine_mode mode, rtx op,machine_mode op_mode)
{
  rtx tem;
  /* If this simplifies, use it.  */
  if ((tem = mtcs_simplify_rtx_unary_operation (self,code, mode, op, op_mode)) != 0)
    return tem;

  return gen_rtx_fmt_e (code, mode, op);
}

/* Try to simplify a unary operation CODE whose output mode is to be
   MODE with input operand OP whose mode was originally OP_MODE.
   Return zero if no simplification can be made.  */
//原型 rtx simplify_context::simplify_unary_operation  rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_unary_operation (MtcsSimplifyRtx *self,rtx_code code, machine_mode mode,rtx op, machine_mode op_mode)
{
  rtx trueop, tem;
  trueop = mtcs_simplify_rtx_avoid_constant_pool_reference (self,op);
  tem = mtcs_simplify_rtx_const_unary_operation (self,code, mode, trueop, op_mode);
  if (tem)
    return tem;
  return mtcs_simplify_rtx_unary_operation_1 (self,code, mode, op);
}

/* Perform some simplifications we can do even if the operands
   aren't constant.  */
//原型 simplify_context::simplify_unary_operation_1 rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_unary_operation_1 (MtcsSimplifyRtx *self,rtx_code code, machine_mode mode,rtx op)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);


  enum rtx_code reversed;
  rtx temp, elt, base, step;
  scalar_int_mode inner, int_mode, op_mode, op0_mode;

  switch (code){
    case NOT:
      /* (not (not X)) == X.  */
      if (GET_CODE (op) == NOT)
          return XEXP (op, 0);

      /* (not (eq X Y)) == (ne X Y), etc. if BImode or the result of the
     comparison is all ones.   */
      if (COMPARISON_P (op)  && (mode == mtcsMode->modes.M_BImode || mtcs_real_get_store_flag_value(mtcsReal) == -1)
              && ((reversed = mtcs_dojump_reversed_comparison_code/*!reversed_comparison_code*/(mtcsDojump,op, NULL)) != UNKNOWN))
          return mtcs_simplify_rtx_gen_relational (self,reversed, mode, VOIDmode, XEXP (op, 0), XEXP (op, 1));

      /* (not (plus X -1)) can become (neg X).  */
      if (GET_CODE (op) == PLUS  && XEXP (op, 1) == constm1_rtx)
          return mtcs_simplify_rtx_gen_unary (self,NEG, mode, XEXP (op, 0), mode);

      /* Similarly, (not (neg X)) is (plus X -1).  Only do this for
     modes that have CONSTM1_RTX, i.e. MODE_INT, MODE_PARTIAL_INT
     and MODE_VECTOR_INT.  */
      if (GET_CODE (op) == NEG && CONSTM1_RTX (mode))
          return mtcs_simplify_rtx_gen_binary (self,PLUS, mode, XEXP (op, 0),CONSTM1_RTX (mode));

      /* (not (xor X C)) for C constant is (xor X D) with D = ~C.  */
      if (GET_CODE (op) == XOR && CONST_INT_P (XEXP (op, 1))
      && (temp = mtcs_simplify_rtx_unary_operation (self,NOT, mode,XEXP (op, 1), mode)) != 0)
          return mtcs_simplify_rtx_gen_binary (self,XOR, mode, XEXP (op, 0), temp);

      /* (not (plus X C)) for signbit C is (xor X D) with D = ~C.  */
      if (GET_CODE (op) == PLUS && CONST_INT_P (XEXP (op, 1))  && mtcs_simplify_rtx_mode_signbit_p(self,mode, XEXP (op, 1))
              && (temp = mtcs_simplify_rtx_unary_operation (self,NOT, mode, XEXP (op, 1), mode)) != 0)
          return mtcs_simplify_rtx_gen_binary (self,XOR, mode, XEXP (op, 0), temp);


      /* (not (ashift 1 X)) is (rotate ~1 X).  We used to do this for
     operands other than 1, but that is not valid.  We could do a
     similar simplification for (not (lshiftrt C X)) where C is
     just the sign bit, but this doesn't seem common enough to
     bother with.  */
      if (GET_CODE (op) == ASHIFT  && XEXP (op, 0) == const1_rtx){
          temp = mtcs_simplify_rtx_gen_unary (self,NOT, mode, const1_rtx, mode);
          return mtcs_simplify_rtx_gen_binary (self,ROTATE, mode, temp, XEXP (op, 1));
      }

      /* (not (ashiftrt foo C)) where C is the number of bits in FOO
     minus 1 is (ge foo (const_int 0)) if STORE_FLAG_VALUE is -1,
     so we can perform the above simplification.  */
      if (mtcs_real_get_store_flag_value(mtcsReal) == -1 && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)
              && GET_CODE (op) == ASHIFTRT
              && CONST_INT_P (XEXP (op, 1)) && INTVAL (XEXP (op, 1)) ==mtcs_mode_get_precision(mtcsMode,int_mode) - 1)
          return mtcs_simplify_rtx_gen_relational (self,GE, int_mode, VOIDmode,XEXP (op, 0), const0_rtx);


      if (mtcs_rtl_partial_subreg_p/*!partial_subreg_p*/(mtcsRTL,op)
              && mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,op)
              && GET_CODE (SUBREG_REG (op)) == ASHIFT
              && XEXP (SUBREG_REG (op), 0) == const1_rtx){
          machine_mode inner_mode = GET_MODE (SUBREG_REG (op));
          rtx x;
          x = gen_rtx_ROTATE (inner_mode,mtcs_simplify_rtx_gen_unary (self,
                NOT, inner_mode, const1_rtx,inner_mode),XEXP (SUBREG_REG (op), 1));
          temp = rtl_hooks.gen_lowpart_no_emit (mode, x);
          if (temp)
            return temp;
      }

      /* Apply De Morgan's laws to reduce number of patterns for machines
     with negating logical insns (and-not, nand, etc.).  If result has
     only one NOT, put it first, since that is how the patterns are
     coded.  */
      if (GET_CODE (op) == IOR || GET_CODE (op) == AND){
          rtx in1 = XEXP (op, 0), in2 = XEXP (op, 1);
          machine_mode op_mode;

          op_mode = GET_MODE (in1);
          in1 = mtcs_simplify_rtx_gen_unary (self,NOT, op_mode, in1, op_mode);

          op_mode = GET_MODE (in2);
          if (op_mode == VOIDmode)
            op_mode = mode;
          in2 = mtcs_simplify_rtx_gen_unary (self,NOT, op_mode, in2, op_mode);

          if (GET_CODE (in2) == NOT && GET_CODE (in1) != NOT)
            std::swap (in1, in2);

          return gen_rtx_fmt_ee (GET_CODE (op) == IOR ? AND : IOR,mode, in1, in2);
      }

      /* (not (bswap x)) -> (bswap (not x)).  */
      if (GET_CODE (op) == BSWAP || GET_CODE (op) == BITREVERSE){
          rtx x = mtcs_simplify_rtx_gen_unary (self,NOT, mode, XEXP (op, 0), mode);
          return mtcs_simplify_rtx_gen_unary (self,GET_CODE (op), mode, x, mode);
      }
      break;

    case NEG:
      /* (neg (neg X)) == X.  */
      if (GET_CODE (op) == NEG)
          return XEXP (op, 0);

      /* (neg (x ? (neg y) : y)) == !x ? (neg y) : y.
     If comparison is not reversible use
     x ? y : (neg y).  */
      if (GET_CODE (op) == IF_THEN_ELSE){
          rtx cond = XEXP (op, 0);
          rtx true_rtx = XEXP (op, 1);
          rtx false_rtx = XEXP (op, 2);

          if ((GET_CODE (true_rtx) == NEG && rtx_equal_p (XEXP (true_rtx, 0), false_rtx))
               || (GET_CODE (false_rtx) == NEG && rtx_equal_p (XEXP (false_rtx, 0), true_rtx))){
              if (mtcs_dojump_reversed_comparison_code/*!reversed_comparison_code*/(mtcsDojump,cond, NULL) != UNKNOWN)
                  temp = reversed_comparison (cond, mode);
              else{
                  temp = cond;
                  std::swap (true_rtx, false_rtx);
              }
              return mtcs_simplify_rtx_gen_ternary (self,IF_THEN_ELSE, mode,mode, temp, true_rtx, false_rtx);
          }
      }

      /* (neg (plus X 1)) can become (not X).  */
      if (GET_CODE (op) == PLUS  && XEXP (op, 1) == const1_rtx)
          return mtcs_simplify_rtx_gen_unary(self,NOT, mode, XEXP (op, 0), mode);

      /* Similarly, (neg (not X)) is (plus X 1).  */
      if (GET_CODE (op) == NOT)
          return mtcs_simplify_rtx_gen_binary (self,PLUS, mode, XEXP (op, 0),CONST1_RTX (mode));

      /* (neg (minus X Y)) can become (minus Y X).  This transformation
     isn't safe for modes with signed zeros, since if X and Y are
     both +0, (minus Y X) is the same as (minus X Y).  If the
     rounding mode is towards +infinity (or -infinity) then the two
     expressions will be rounded differently.  */
      if (GET_CODE (op) == MINUS && !mtcs_mode_honor_signed_zeros(mtcsMode,mode)
              && !mtcs_mode_honor_sign_dependent_rounding (mtcsMode,mode))
          return mtcs_simplify_rtx_gen_binary (self,MINUS, mode, XEXP (op, 1), XEXP (op, 0));

      if (GET_CODE (op) == PLUS  && !mtcs_mode_honor_signed_zeros(mtcsMode,mode)
              && !mtcs_mode_honor_sign_dependent_rounding (mtcsMode,mode)){
      /* (neg (plus A C)) is simplified to (minus -C A).  */
         if (CONST_SCALAR_INT_P (XEXP (op, 1)) || CONST_DOUBLE_AS_FLOAT_P (XEXP (op, 1))){
            temp = mtcs_simplify_rtx_unary_operation (self,NEG, mode, XEXP (op, 1), mode);
            if (temp)
              return mtcs_simplify_rtx_gen_binary (self,MINUS, mode, temp, XEXP (op, 0));
         }

         /* (neg (plus A B)) is canonicalized to (minus (neg A) B).  */
         temp = mtcs_simplify_rtx_gen_unary(self,NEG, mode, XEXP (op, 0), mode);
         return mtcs_simplify_rtx_gen_binary (self,MINUS, mode, temp, XEXP (op, 1));
      }

      /* (neg (mult A B)) becomes (mult A (neg B)).
     This works even for floating-point values.  */
      if (GET_CODE (op) == MULT  && !mtcs_mode_honor_sign_dependent_rounding (mtcsMode,mode)){
          temp = mtcs_simplify_rtx_gen_unary(self,NEG, mode, XEXP (op, 1), mode);
          return mtcs_simplify_rtx_gen_binary (self,MULT, mode, XEXP (op, 0), temp);
      }

      /* NEG commutes with ASHIFT since it is multiplication.  Only do
     this if we can then eliminate the NEG (e.g., if the operand
     is a constant).  */
      if (GET_CODE (op) == ASHIFT){
          temp = mtcs_simplify_rtx_unary_operation (self,NEG, mode, XEXP (op, 0), mode);
          if (temp)
            return mtcs_simplify_rtx_gen_binary (self,ASHIFT, mode, temp, XEXP (op, 1));
      }

      /* (neg (ashiftrt X C)) can be replaced by (lshiftrt X C) when
     C is equal to the width of MODE minus 1.  */
      if (GET_CODE (op) == ASHIFTRT  && CONST_INT_P (XEXP (op, 1))
         && INTVAL (XEXP (op, 1)) == mtcs_mode_get_unit_precision(mtcsMode,mode) - 1)
          return mtcs_simplify_rtx_gen_binary (self,LSHIFTRT, mode,XEXP (op, 0), XEXP (op, 1));

      /* (neg (lshiftrt X C)) can be replaced by (ashiftrt X C) when
     C is equal to the width of MODE minus 1.  */
      if (GET_CODE (op) == LSHIFTRT  && CONST_INT_P (XEXP (op, 1))
          && INTVAL (XEXP (op, 1)) == mtcs_mode_get_unit_precision(mtcsMode,mode) - 1)
          return mtcs_simplify_rtx_gen_binary (self,ASHIFTRT, mode,XEXP (op, 0), XEXP (op, 1));

      /* (neg (xor A 1)) is (plus A -1) if A is known to be either 0 or 1.  */
      if (GET_CODE (op) == XOR  && XEXP (op, 1) == const1_rtx
            && mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(mtcsRtlanal,XEXP (op, 0), mode) == 1)
          return mtcs_rtl_plus_constant(mtcsRTL,mode, XEXP (op, 0), -1);

      /* (neg (lt x 0)) is (ashiftrt X C) if STORE_FLAG_VALUE is 1.  */
      /* (neg (lt x 0)) is (lshiftrt X C) if STORE_FLAG_VALUE is -1.  */
      if (GET_CODE (op) == LT  && XEXP (op, 1) == const0_rtx &&
              mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (XEXP (op, 0)), &inner)){
          int_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,mode);
          int isize = mtcs_mode_get_precision(mtcsMode,inner);
          if (mtcs_real_get_store_flag_value(mtcsReal) == 1) {
              temp = mtcs_simplify_rtx_gen_binary (self,ASHIFTRT, inner, XEXP (op, 0),
                          mtcs_rtl_gen_int_shift_amount(mtcsRTL,inner,isize - 1));
              if (int_mode == inner)
                  return temp;
              if (mtcs_mode_get_precision(mtcsMode,int_mode) > isize)
                  return mtcs_simplify_rtx_gen_unary(self,SIGN_EXTEND, int_mode, temp, inner);
              return mtcs_simplify_rtx_gen_unary(self,TRUNCATE, int_mode, temp, inner);
          }else if (mtcs_real_get_store_flag_value(mtcsReal) == -1){
              temp = mtcs_simplify_rtx_gen_binary (self,LSHIFTRT, inner, XEXP (op, 0),
                          mtcs_rtl_gen_int_shift_amount(mtcsRTL,inner,isize - 1));
              if (int_mode == inner)
                  return temp;
              if (mtcs_mode_get_precision(mtcsMode,int_mode) > isize)
                  return mtcs_simplify_rtx_gen_unary(self,ZERO_EXTEND, int_mode, temp, inner);
              return mtcs_simplify_rtx_gen_unary(self,TRUNCATE, int_mode, temp, inner);
          }
      }

      if (vec_series_p (op, &base, &step)){
          /* Only create a new series if we can simplify both parts.  In other
             cases this isn't really a simplification, and it's not necessarily
             a win to replace a vector operation with a scalar operation.  */
          scalar_mode inner_mode = mtcs_mode_get_inner (mtcsMode,mode);
          base = mtcs_simplify_rtx_unary_operation (self,NEG, inner_mode, base, inner_mode);
          if (base){
              step = mtcs_simplify_rtx_unary_operation (self,NEG, inner_mode,step, inner_mode);
              if (step)
                  return gen_vec_series (mode, base, step);
          }
      }
      break;

    case TRUNCATE:
      /* Don't optimize (lshiftrt (mult ...)) as it would interfere
     with the umulXi3_highpart patterns.  */
      if (GET_CODE (op) == LSHIFTRT && GET_CODE (XEXP (op, 0)) == MULT)
          break;

      if (mtcs_mode_get_class(mtcsMode,mode) == MODE_PARTIAL_INT){
          if (mtcs_mode_truly_noop_truncation_p/*!TRULY_NOOP_TRUNCATION_MODES_P*/ (mtcsMode,mode, GET_MODE (op))){
              temp = rtl_hooks.gen_lowpart_no_emit (mode, op);
              if (temp)
                  return temp;
          }
          /* We can't handle truncation to a partial integer mode here
             because we don't know the real bitsize of the partial
             integer mode.  */
          break;
      }

      if (GET_MODE (op) != VOIDmode){
          temp = mtcs_simplify_rtx_truncation (self,mode, op, GET_MODE (op));
          if (temp)
            return temp;
      }

      /* If we know that the value is already truncated, we can
     replace the TRUNCATE with a SUBREG.  */
      if (known_eq (mtcs_mode_get_nunits(mtcsMode,mode), 1)
          && (mtcs_mode_truly_noop_truncation_p/*!TRULY_NOOP_TRUNCATION_MODES_P*/ (mtcsMode,mode, GET_MODE (op))
              || truncated_to_mode (mode, op))){
          temp = rtl_hooks.gen_lowpart_no_emit (mode, op);
          if (temp)
            return temp;
      }

      /* A truncate of a comparison can be replaced with a subreg if
         STORE_FLAG_VALUE permits.  This is like the previous test,
         but it works even if the comparison is done in a mode larger
         than HOST_BITS_PER_WIDE_INT.  */
      if (mtcs_mode_is_hwi_computable_p(mtcsMode,mode)  && COMPARISON_P (op)
          && (mtcs_real_get_store_flag_value(mtcsReal) & ~mtcs_mode_get_mask(mtcsMode,mode)) == 0
          && mtcs_mode_truly_noop_truncation_p/*!TRULY_NOOP_TRUNCATION_MODES_P*/ (mtcsMode,mode, GET_MODE (op))){
          temp = rtl_hooks.gen_lowpart_no_emit (mode, op);//在mtcscompile调用mtcsinterface中的接口set方法改变了域指针位置
          if (temp)
            return temp;
      }

      /* A truncate of a memory is just loading the low part of the memory
     if we are not changing the meaning of the address. */
      if (GET_CODE (op) == MEM  && !mtcs_mode_is_vector_p(mtcsMode,mode)
              && !MEM_VOLATILE_P (op)
              && !mtcs_recog_mode_dependent_address_p/*!mode_dependent_address_p*/(mtcsRecog,XEXP (op, 0), MEM_ADDR_SPACE (op))){
          temp = rtl_hooks.gen_lowpart_no_emit (mode, op);
          if (temp)
            return temp;
      }

      /* Check for useless truncation.  */
      if (GET_MODE (op) == mode)
          return op;
      break;

    case FLOAT_TRUNCATE:
      /* Check for useless truncation.  */
      if (GET_MODE (op) == mode)
          return op;

      if (mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,mode))
          break;

      /* (float_truncate:SF (float_extend:DF foo:SF)) = foo:SF.  */
      if (GET_CODE (op) == FLOAT_EXTEND && GET_MODE (XEXP (op, 0)) == mode)
          return XEXP (op, 0);

      /* (float_truncate:SF (float_truncate:DF foo:XF))
         = (float_truncate:SF foo:XF).
     This may eliminate double rounding, so it is unsafe.

         (float_truncate:SF (float_extend:XF foo:DF))
         = (float_truncate:SF foo:DF).

         (float_truncate:DF (float_extend:XF foo:SF))
         = (float_extend:DF foo:SF).  */
      if ((GET_CODE (op) == FLOAT_TRUNCATE  && flag_unsafe_math_optimizations) || GET_CODE (op) == FLOAT_EXTEND)
          return mtcs_simplify_rtx_gen_unary(self,mtcs_mode_get_unit_size (mtcsMode,GET_MODE (XEXP (op, 0)))
                   > mtcs_mode_get_unit_size (mtcsMode,mode) ? FLOAT_TRUNCATE : FLOAT_EXTEND,
                   mode, XEXP (op, 0), GET_MODE (XEXP (op, 0)));

      /*  (float_truncate (float x)) is (float x)  */
      if ((GET_CODE (op) == FLOAT || GET_CODE (op) == UNSIGNED_FLOAT)
              && (flag_unsafe_math_optimizations   || exact_int_to_float_conversion_p (self,op)))
          return mtcs_simplify_rtx_gen_unary(self,GET_CODE (op), mode,XEXP (op, 0), GET_MODE (XEXP (op, 0)));

      /* (float_truncate:SF (OP:DF (float_extend:DF foo:sf))) is
     (OP:SF foo:SF) if OP is NEG or ABS.  */
      if ((GET_CODE (op) == ABS || GET_CODE (op) == NEG)
         && GET_CODE (XEXP (op, 0)) == FLOAT_EXTEND && GET_MODE (XEXP (XEXP (op, 0), 0)) == mode)
          return mtcs_simplify_rtx_gen_unary(self,GET_CODE (op), mode, XEXP (XEXP (op, 0), 0), mode);

      /* (float_truncate:SF (subreg:DF (float_truncate:SF X) 0))
     is (float_truncate:SF x).  */
      if (GET_CODE (op) == SUBREG && mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,op)
           && GET_CODE (SUBREG_REG (op)) == FLOAT_TRUNCATE)
          return SUBREG_REG (op);
      break;

    case FLOAT_EXTEND:
      /* Check for useless extension.  */
      if (GET_MODE (op) == mode)
          return op;

      if (mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,mode))
          break;

      /*  (float_extend (float_extend x)) is (float_extend x)

      (float_extend (float x)) is (float x) assuming that double
      rounding can't happen.
          */
      if (GET_CODE (op) == FLOAT_EXTEND || ((GET_CODE (op) == FLOAT || GET_CODE (op) == UNSIGNED_FLOAT)
          && exact_int_to_float_conversion_p (self,op)))
          return mtcs_simplify_rtx_gen_unary(self,GET_CODE (op), mode,XEXP (op, 0),GET_MODE (XEXP (op, 0)));

      break;

    case ABS:
      /* (abs (neg <foo>)) -> (abs <foo>) */
      if (GET_CODE (op) == NEG)
          return mtcs_simplify_rtx_gen_unary(self,ABS, mode, XEXP (op, 0),GET_MODE (XEXP (op, 0)));

      /* If the mode of the operand is VOIDmode (i.e. if it is ASM_OPERANDS),
         do nothing.  */
      if (GET_MODE (op) == VOIDmode)
          break;

      /* If operand is something known to be positive, ignore the ABS.  */
      if (val_signbit_known_clear_p (GET_MODE (op), mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(mtcsRtlanal,op, GET_MODE (op))))
          return op;

      /* Using nonzero_bits doesn't (currently) work for modes wider than
     HOST_WIDE_INT, so the following transformations help simplify
     ABS for TImode and wider.  */
      switch (GET_CODE (op)){
        case ABS:
        case CLRSB:
        case FFS:
        case PARITY:
        case POPCOUNT:
        case SS_ABS:
          return op;

        case LSHIFTRT:
          if (CONST_INT_P (XEXP (op, 1)) && INTVAL (XEXP (op, 1)) > 0
              && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode) && INTVAL (XEXP (op, 1)) <
              mtcs_mode_get_precision(mtcsMode,int_mode))
            return op;
          break;

        default:
          break;
      }

      /* If operand is known to be only -1 or 0, convert ABS to NEG.  */
      if (mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)
              && (mtcs_rtlanal_num_sign_bit_copies/*!num_sign_bit_copies*/(mtcsRtlanal,op, int_mode)
                    == mtcs_mode_get_precision(mtcsMode,int_mode)))
          return gen_rtx_NEG (int_mode, op);

      break;

    case FFS:
      /* (ffs (*_extend <X>)) = (*_extend (ffs <X>)).  */
      if (GET_CODE (op) == SIGN_EXTEND || GET_CODE (op) == ZERO_EXTEND){
          temp = mtcs_simplify_rtx_gen_unary(self,FFS, GET_MODE (XEXP (op, 0)),XEXP (op, 0), GET_MODE (XEXP (op, 0)));
          return mtcs_simplify_rtx_gen_unary(self,GET_CODE (op), mode, temp,GET_MODE (temp));
      }
      break;

    case POPCOUNT:
      switch (GET_CODE (op)){
        case BSWAP:
        case BITREVERSE:
          /* (popcount (bswap <X>)) = (popcount <X>).  */
          return mtcs_simplify_rtx_gen_unary(self,POPCOUNT, mode, XEXP (op, 0),GET_MODE (XEXP (op, 0)));

        case ZERO_EXTEND:
          /* (popcount (zero_extend <X>)) = (zero_extend (popcount <X>)).  */
          temp = mtcs_simplify_rtx_gen_unary(self,POPCOUNT, GET_MODE (XEXP (op, 0)),XEXP (op, 0), GET_MODE (XEXP (op, 0)));
          return mtcs_simplify_rtx_gen_unary(self,ZERO_EXTEND, mode, temp,GET_MODE (temp));

        case ROTATE:
        case ROTATERT:
          /* Rotations don't affect popcount.  */
          if (!side_effects_p (XEXP (op, 1)))
            return mtcs_simplify_rtx_gen_unary(self,POPCOUNT, mode, XEXP (op, 0),GET_MODE (XEXP (op, 0)));
          break;

        default:
          break;
      }
      break;

    case PARITY:
      switch (GET_CODE (op)){
        case NOT:
        case BSWAP:
        case BITREVERSE:
          return mtcs_simplify_rtx_gen_unary(self,PARITY, mode, XEXP (op, 0),
                         GET_MODE (XEXP (op, 0)));

        case ZERO_EXTEND:
        case SIGN_EXTEND:
          temp = mtcs_simplify_rtx_gen_unary(self,PARITY, GET_MODE (XEXP (op, 0)),XEXP (op, 0), GET_MODE (XEXP (op, 0)));
          return mtcs_simplify_rtx_gen_unary(self,GET_CODE (op), mode, temp,GET_MODE (temp));

        case ROTATE:
        case ROTATERT:
          /* Rotations don't affect parity.  */
          if (!side_effects_p (XEXP (op, 1)))
            return mtcs_simplify_rtx_gen_unary(self,PARITY, mode, XEXP (op, 0),GET_MODE (XEXP (op, 0)));
          break;

        case PARITY:
          /* (parity (parity x)) -> parity (x).  */
          return op;

        default:
          break;
      }
      break;

    case BSWAP:
      /* (bswap (bswap x)) -> x.  */
      if (GET_CODE (op) == BSWAP)
          return XEXP (op, 0);
      break;

    case BITREVERSE:
      /* (bitreverse (bitreverse x)) -> x.  */
      if (GET_CODE (op) == BITREVERSE)
          return XEXP (op, 0);
      break;

    case FLOAT:
      /* (float (sign_extend <X>)) = (float <X>).  */
      if (GET_CODE (op) == SIGN_EXTEND)
          return mtcs_simplify_rtx_gen_unary(self,FLOAT, mode, XEXP (op, 0),GET_MODE (XEXP (op, 0)));
      break;

    case SIGN_EXTEND:
      /* Check for useless extension.  */
      if (GET_MODE (op) == mode)
          return op;

      /* (sign_extend (truncate (minus (label_ref L1) (label_ref L2))))
     becomes just the MINUS if its mode is MODE.  This allows
     folding switch statements on machines using casesi (such as
     the VAX).  */
      if (GET_CODE (op) == TRUNCATE  && GET_MODE (XEXP (op, 0)) == mode
          && GET_CODE (XEXP (op, 0)) == MINUS  && GET_CODE (XEXP (XEXP (op, 0), 0)) == LABEL_REF
          && GET_CODE (XEXP (XEXP (op, 0), 1)) == LABEL_REF)
          return XEXP (op, 0);

      /* Extending a widening multiplication should be canonicalized to
     a wider widening multiplication.  */
      if (GET_CODE (op) == MULT){
          rtx lhs = XEXP (op, 0);
          rtx rhs = XEXP (op, 1);
          enum rtx_code lcode = GET_CODE (lhs);
          enum rtx_code rcode = GET_CODE (rhs);

          /* Widening multiplies usually extend both operands, but sometimes
             they use a shift to extract a portion of a register.  */
          if ((lcode == SIGN_EXTEND
               || (lcode == ASHIFTRT && CONST_INT_P (XEXP (lhs, 1))))
              && (rcode == SIGN_EXTEND
              || (rcode == ASHIFTRT && CONST_INT_P (XEXP (rhs, 1))))){
              machine_mode lmode = GET_MODE (lhs);
              machine_mode rmode = GET_MODE (rhs);
              int bits;

              if (lcode == ASHIFTRT)
                /* Number of bits not shifted off the end.  */
                bits = (mtcs_mode_get_unit_precision(mtcsMode,lmode)- INTVAL (XEXP (lhs, 1)));
              else /* lcode == SIGN_EXTEND */
                /* Size of inner mode.  */
                bits = mtcs_mode_get_unit_precision(mtcsMode,GET_MODE (XEXP (lhs, 0)));

              if (rcode == ASHIFTRT)
                  bits += (mtcs_mode_get_unit_precision(mtcsMode,rmode)- INTVAL (XEXP (rhs, 1)));
              else /* rcode == SIGN_EXTEND */
                  bits += mtcs_mode_get_unit_precision(mtcsMode,GET_MODE (XEXP (rhs, 0)));

              /* We can only widen multiplies if the result is mathematiclly
             equivalent.  I.e. if overflow was impossible.  */
              if (bits <= mtcs_mode_get_unit_precision(mtcsMode,GET_MODE (op)))
                return mtcs_simplify_rtx_gen_binary(self,MULT, mode,
                      mtcs_simplify_rtx_gen_unary(self,SIGN_EXTEND, mode, lhs, lmode),
                      mtcs_simplify_rtx_gen_unary(self,SIGN_EXTEND, mode, rhs, rmode));
            }
      }

      /* Check for a sign extension of a subreg of a promoted
     variable, where the promotion is sign-extended, and the
     target mode is the same as the variable's promotion.  */
      if (GET_CODE (op) == SUBREG   && SUBREG_PROMOTED_VAR_P (op)  && SUBREG_PROMOTED_SIGNED_P (op)){
          rtx subreg = SUBREG_REG (op);
          machine_mode subreg_mode = GET_MODE (subreg);
          if (!mtcs_mode_paradoxical_subreg_p (mtcsMode,mode, subreg_mode)){
              temp = rtl_hooks.gen_lowpart_no_emit (mode, subreg);
              if (temp){
                  /* Preserve SUBREG_PROMOTED_VAR_P.  */
                  if (mtcs_rtl_partial_subreg_p/*!partial_subreg_p*/(mtcsRTL,temp)){
                      SUBREG_PROMOTED_VAR_P (temp) = 1;
                      SUBREG_PROMOTED_SET (temp, SRP_SIGNED);
                  }
                  return temp;
              }
          }else
            /* Sign-extending a sign-extended subreg.  */
            return mtcs_simplify_rtx_gen_unary(self,SIGN_EXTEND, mode,subreg, subreg_mode);
      }

      /* (sign_extend:M (sign_extend:N <X>)) is (sign_extend:M <X>).
     (sign_extend:M (zero_extend:N <X>)) is (zero_extend:M <X>).  */
      if (GET_CODE (op) == SIGN_EXTEND || GET_CODE (op) == ZERO_EXTEND){
          gcc_assert (mtcs_mode_get_unit_precision(mtcsMode,mode) > mtcs_mode_get_unit_precision(mtcsMode,GET_MODE (op)));
          return mtcs_simplify_rtx_gen_unary(self,GET_CODE (op), mode, XEXP (op, 0),GET_MODE (XEXP (op, 0)));
      }

      /* (sign_extend:M (ashiftrt:N (ashift <X> (const_int I)) (const_int I)))
     is (sign_extend:M (subreg:O <X>)) if there is mode with
     GET_MODE_BITSIZE (N) - I bits.
     (sign_extend:M (lshiftrt:N (ashift <X> (const_int I)) (const_int I)))
     is similarly (zero_extend:M (subreg:O <X>)).  */
      if ((GET_CODE (op) == ASHIFTRT || GET_CODE (op) == LSHIFTRT)
          && GET_CODE (XEXP (op, 0)) == ASHIFT
          && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)
          && CONST_INT_P (XEXP (op, 1))
          && XEXP (XEXP (op, 0), 1) == XEXP (op, 1)
          && (op_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,GET_MODE (op)),
              mtcs_mode_get_precision(mtcsMode,op_mode) > INTVAL (XEXP (op, 1)))){
          scalar_int_mode tmode;
          gcc_assert (mtcs_mode_get_precision(mtcsMode,int_mode)
                  > mtcs_mode_get_precision(mtcsMode,op_mode));
          if (mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,mtcs_mode_get_precision(mtcsMode,op_mode)- INTVAL (XEXP (op, 1)), 1).exists (&tmode)){
              rtx inner = rtl_hooks.gen_lowpart_no_emit (tmode, XEXP (XEXP (op, 0), 0));
              if (inner)
                  return mtcs_simplify_rtx_gen_unary(self,GET_CODE (op) == ASHIFTRT ? SIGN_EXTEND : ZERO_EXTEND,int_mode, inner, tmode);
          }
      }

      /* (sign_extend:M (lshiftrt:N <X> (const_int I))) is better as
         (zero_extend:M (lshiftrt:N <X> (const_int I))) if I is not 0.  */
      if (GET_CODE (op) == LSHIFTRT  && CONST_INT_P (XEXP (op, 1))  && XEXP (op, 1) != const0_rtx)
          return mtcs_simplify_rtx_gen_unary(self,ZERO_EXTEND, mode, op, GET_MODE (op));

      /* (sign_extend:M (truncate:N (lshiftrt:O <X> (const_int I)))) where
     I is GET_MODE_PRECISION(O) - GET_MODE_PRECISION(N), simplifies to
     (ashiftrt:M <X> (const_int I)) if modes M and O are the same, and
     (truncate:M (ashiftrt:O <X> (const_int I))) if M is narrower than
     O, and (sign_extend:M (ashiftrt:O <X> (const_int I))) if M is
     wider than O.  */
      if (GET_CODE (op) == TRUNCATE && GET_CODE (XEXP (op, 0)) == LSHIFTRT && CONST_INT_P (XEXP (XEXP (op, 0), 1))){
          scalar_int_mode m_mode, n_mode, o_mode;
          rtx old_shift = XEXP (op, 0);
          if (mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &m_mode)
              && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (op), &n_mode)
              && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (old_shift), &o_mode)
              && mtcs_mode_get_precision(mtcsMode,o_mode) - mtcs_mode_get_precision(mtcsMode,n_mode)
             == INTVAL (XEXP (old_shift, 1))){
              rtx new_shift = mtcs_simplify_rtx_gen_binary (self,ASHIFTRT,
                               GET_MODE (old_shift),XEXP (old_shift, 0),XEXP (old_shift, 1));
              if (mtcs_mode_get_precision(mtcsMode,m_mode) > mtcs_mode_get_precision(mtcsMode,o_mode))
                  return mtcs_simplify_rtx_gen_unary(self,SIGN_EXTEND, mode, new_shift, GET_MODE (new_shift));
              if (mode != GET_MODE (new_shift))
                  return mtcs_simplify_rtx_gen_unary(self,TRUNCATE, mode, new_shift,GET_MODE (new_shift));
              return new_shift;
          }
      }

      /* We can canonicalize SIGN_EXTEND (op) as ZERO_EXTEND (op) when
         we know the sign bit of OP must be clear.  */
      if (val_signbit_known_clear_p (GET_MODE (op), mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(mtcsRtlanal,op, GET_MODE (op))))
          return mtcs_simplify_rtx_gen_unary(self,ZERO_EXTEND, mode, op, GET_MODE (op));

      /* (sign_extend:DI (subreg:SI (ctz:DI ...))) is (ctz:DI ...).  */
      if (GET_CODE (op) == SUBREG
          && mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,op)
          && GET_MODE (SUBREG_REG (op)) == mode
          && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)
          && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (op), &op_mode)
          && mtcs_mode_get_precision(mtcsMode,int_mode) <= HOST_BITS_PER_WIDE_INT
          && mtcs_mode_get_precision(mtcsMode,op_mode) < mtcs_mode_get_precision(mtcsMode,int_mode)
          && (mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(mtcsRtlanal,SUBREG_REG (op), mode)
              & ~(mtcs_mode_get_mask(mtcsMode,op_mode) >> 1)) == 0)
          return SUBREG_REG (op);

//#if defined(POINTERS_EXTEND_UNSIGNED)  //host=1 nvptx=0
//      /* As we do not know which address space the pointer is referring to,
//     we can do this only if the target does not support different pointer
//     or address modes depending on the address space.  */
//      if (target_default_pointer_address_modes_p ()
//      && ! POINTERS_EXTEND_UNSIGNED
//      && mode == Pmode && GET_MODE (op) == ptr_mode
//      && (CONSTANT_P (op)
//          || (GET_CODE (op) == SUBREG
//          && REG_P (SUBREG_REG (op))
//          && REG_POINTER (SUBREG_REG (op))
//          && GET_MODE (SUBREG_REG (op)) == Pmode))
//      && !targetm.have_ptr_extend ())
//    {
//      temp
//        = convert_memory_address_addr_space_1 (Pmode, op,
//                           ADDR_SPACE_GENERIC, false,
//                           true);
//      if (temp)
//        return temp;
//    }
//#endif
      break;

    case ZERO_EXTEND:
      /* Check for useless extension.  */
      if (GET_MODE (op) == mode)
          return op;

      /* Check for a zero extension of a subreg of a promoted
     variable, where the promotion is zero-extended, and the
     target mode is the same as the variable's promotion.  */
      if (GET_CODE (op) == SUBREG  && SUBREG_PROMOTED_VAR_P (op) && SUBREG_PROMOTED_UNSIGNED_P (op)){
          rtx subreg = SUBREG_REG (op);
          machine_mode subreg_mode = GET_MODE (subreg);
          if (!mtcs_mode_paradoxical_subreg_p/*!paradoxical_subreg_p*/(mtcsMode,mode, subreg_mode)){
              temp = rtl_hooks.gen_lowpart_no_emit (mode, subreg);
              if (temp){
                  /* Preserve SUBREG_PROMOTED_VAR_P.  */
                  if (mtcs_rtl_partial_subreg_p/*!partial_subreg_p*/(mtcsRTL,temp)){
                      SUBREG_PROMOTED_VAR_P (temp) = 1;
                      SUBREG_PROMOTED_SET (temp, SRP_UNSIGNED);
                  }
                  return temp;
              }
          }else
            /* Zero-extending a zero-extended subreg.  */
            return mtcs_simplify_rtx_gen_unary(self,ZERO_EXTEND, mode,subreg, subreg_mode);
      }

      /* Extending a widening multiplication should be canonicalized to
     a wider widening multiplication.  */
      if (GET_CODE (op) == MULT){
          rtx lhs = XEXP (op, 0);
          rtx rhs = XEXP (op, 1);
          enum rtx_code lcode = GET_CODE (lhs);
          enum rtx_code rcode = GET_CODE (rhs);

          /* Widening multiplies usually extend both operands, but sometimes
             they use a shift to extract a portion of a register.  */
          if ((lcode == ZERO_EXTEND || (lcode == LSHIFTRT && CONST_INT_P (XEXP (lhs, 1))))
              && (rcode == ZERO_EXTEND   || (rcode == LSHIFTRT && CONST_INT_P (XEXP (rhs, 1))))){
              machine_mode lmode = GET_MODE (lhs);
              machine_mode rmode = GET_MODE (rhs);
              int bits;

              if (lcode == LSHIFTRT)
                /* Number of bits not shifted off the end.  */
                bits = (mtcs_mode_get_unit_precision(mtcsMode,lmode)- INTVAL (XEXP (lhs, 1)));
              else /* lcode == ZERO_EXTEND */
                /* Size of inner mode.  */
                bits = mtcs_mode_get_unit_precision(mtcsMode,GET_MODE (XEXP (lhs, 0)));

              if (rcode == LSHIFTRT)
                  bits += (mtcs_mode_get_unit_precision(mtcsMode,rmode) - INTVAL (XEXP (rhs, 1)));
              else /* rcode == ZERO_EXTEND */
                  bits += mtcs_mode_get_unit_precision(mtcsMode,GET_MODE (XEXP (rhs, 0)));

              /* We can only widen multiplies if the result is mathematiclly
             equivalent.  I.e. if overflow was impossible.  */
              if (bits <= mtcs_mode_get_unit_precision(mtcsMode,GET_MODE (op)))
                return mtcs_simplify_rtx_gen_binary(self,MULT, mode,
                      mtcs_simplify_rtx_gen_unary(self,ZERO_EXTEND, mode, lhs, lmode),
                      mtcs_simplify_rtx_gen_unary(self,ZERO_EXTEND, mode, rhs, rmode));
            }
      }

      /* (zero_extend:M (zero_extend:N <X>)) is (zero_extend:M <X>).  */
      if (GET_CODE (op) == ZERO_EXTEND)
          return mtcs_simplify_rtx_gen_unary(self,ZERO_EXTEND, mode, XEXP (op, 0),GET_MODE (XEXP (op, 0)));

      /* (zero_extend:M (lshiftrt:N (ashift <X> (const_int I)) (const_int I)))
     is (zero_extend:M (subreg:O <X>)) if there is mode with
     GET_MODE_PRECISION (N) - I bits.  */
      if (GET_CODE (op) == LSHIFTRT
          && GET_CODE (XEXP (op, 0)) == ASHIFT
          && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)
          && CONST_INT_P (XEXP (op, 1))
          && XEXP (XEXP (op, 0), 1) == XEXP (op, 1)
          && (op_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,GET_MODE (op)),
              mtcs_mode_get_precision(mtcsMode,op_mode) > INTVAL (XEXP (op, 1)))){
           scalar_int_mode tmode;
           if (mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,
                   mtcs_mode_get_precision(mtcsMode,op_mode)- INTVAL (XEXP (op, 1)), 1).exists (&tmode)){
              rtx inner = rtl_hooks.gen_lowpart_no_emit (tmode, XEXP (XEXP (op, 0), 0));
              if (inner)
                  return mtcs_simplify_rtx_gen_unary(self,ZERO_EXTEND, int_mode,inner, tmode);
           }
       }

      /* (zero_extend:M (subreg:N <X:O>)) is <X:O> (for M == O) or
     (zero_extend:M <X:O>), if X doesn't have any non-zero bits outside
     of mode N.  E.g.
     (zero_extend:SI (subreg:QI (and:SI (reg:SI) (const_int 63)) 0)) is
     (and:SI (reg:SI) (const_int 63)).  */
      if (mtcs_rtl_partial_subreg_p/*!partial_subreg_p*/(mtcsRTL,op)
      && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)
      && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (SUBREG_REG (op)), &op0_mode)
      && mtcs_mode_get_precision(mtcsMode,op0_mode) <= HOST_BITS_PER_WIDE_INT
      && mtcs_mode_get_precision(mtcsMode,int_mode) >= mtcs_mode_get_precision(mtcsMode,op0_mode)
      && mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,op)
      && (mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(mtcsRtlanal,SUBREG_REG (op), op0_mode)
          & ~mtcs_mode_get_mask(mtcsMode,GET_MODE (op))) == 0){
          if (mtcs_mode_get_precision(mtcsMode,int_mode) == mtcs_mode_get_precision(mtcsMode,op0_mode))
            return SUBREG_REG (op);
          return mtcs_simplify_rtx_gen_unary(self,ZERO_EXTEND, int_mode, SUBREG_REG (op),op0_mode);
      }

      /* (zero_extend:DI (subreg:SI (ctz:DI ...))) is (ctz:DI ...).  */
      if (GET_CODE (op) == SUBREG
      && mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,op)
      && GET_MODE (SUBREG_REG (op)) == mode
      && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)
      && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (op), &op_mode)
      && mtcs_mode_get_precision(mtcsMode,int_mode) <= HOST_BITS_PER_WIDE_INT
      && mtcs_mode_get_precision(mtcsMode,op_mode) < mtcs_mode_get_precision(mtcsMode,int_mode)
      && (mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(mtcsRtlanal,SUBREG_REG (op), mode)
          & ~mtcs_mode_get_mask(mtcsMode,op_mode)) == 0)
          return SUBREG_REG (op);
      if(mtcs_config_ifdefine(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED)){

         //#if defined(POINTERS_EXTEND_UNSIGNED)
         //      /* As we do not know which address space the pointer is referring to,
         //     we can do this only if the target does not support different pointer
         //     or address modes depending on the address space.  */
         //      if (target_default_pointer_address_modes_p ()
         //      && POINTERS_EXTEND_UNSIGNED > 0
         //      && mode == Pmode && GET_MODE (op) == ptr_mode
         //      && (CONSTANT_P (op)
         //          || (GET_CODE (op) == SUBREG
         //          && REG_P (SUBREG_REG (op))
         //          && REG_POINTER (SUBREG_REG (op))
         //          && GET_MODE (SUBREG_REG (op)) == Pmode))
         //      && !target_rtx_have_ptr_extend/*!targetm.have_ptr_extend*/(mtcsMachine->tmrtx))
         //    {
         //      temp
         //        = convert_memory_address_addr_space_1 (Pmode, op,
         //                           ADDR_SPACE_GENERIC, false,
         //                           true);
         //      if (temp)
         //        return temp;
         //    }
         //#endif
         if (mtcs_target_target_default_pointer_address_modes_p/*!target_default_pointer_address_modes_p*/(mtcsTarget)
         && mtcs_config_get_value/*!POINTERS_EXTEND_UNSIGNED*/(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED)> 0
         && mode == mtcs_mode_get_Pmode(mtcsMode) && GET_MODE (op) == ptr_mode
         && (CONSTANT_P (op)
         || (GET_CODE (op) == SUBREG
         && REG_P (SUBREG_REG (op))
         && REG_POINTER (SUBREG_REG (op))
         && GET_MODE (SUBREG_REG (op)) == mtcs_mode_get_Pmode(mtcsMode)))
         && !target_rtx_have_ptr_extend/*!targetm.have_ptr_extend*/(mtcsMachine->tmrtx)){
            temp = mtcs_explow_convert_memory_address_addr_space_1/*!convert_memory_address_addr_space_1*/(mtcsExplow,
            mtcs_mode_get_Pmode(mtcsMode), op,ADDR_SPACE_GENERIC, false,true);
            if (temp)
               return temp;
         }
      }
      break;

    default:
      break;
    }

  if (mtcs_mode_is_vector_p(mtcsMode,mode) && vec_duplicate_p (op, &elt) && code != VEC_DUPLICATE){
      if (code == SIGN_EXTEND || code == ZERO_EXTEND)
    /* Enforce a canonical order of VEC_DUPLICATE wrt other unary
       operations by promoting VEC_DUPLICATE to the root of the expression
       (as far as possible).  */
          temp = mtcs_simplify_rtx_gen_unary(self,code, mtcs_mode_get_inner (mtcsMode,mode),elt, mtcs_mode_get_inner (mtcsMode,GET_MODE (op)));
      else
    /* Try applying the operator to ELT and see if that simplifies.
       We can duplicate the result if so.

       The reason we traditionally haven't used simplify_gen_unary
       for these codes is that it didn't necessarily seem to be a
       win to convert things like:

         (neg:V (vec_duplicate:V (reg:S R)))

       to:

         (vec_duplicate:V (neg:S (reg:S R)))

       The first might be done entirely in vector registers while the
       second might need a move between register files.

       However, there also cases where promoting the vec_duplicate is
       more efficient, and there is definite value in having a canonical
       form when matching instruction patterns.  We should consider
       extending the simplify_gen_unary code above to more cases.  */
          temp = mtcs_simplify_rtx_unary_operation (self,code, mtcs_mode_get_inner (mtcsMode,mode),elt, mtcs_mode_get_inner (mtcsMode,GET_MODE (op)));
      if (temp)
          return gen_vec_duplicate (mode, temp);
  }

  return 0;
}


/* Try to compute the value of a unary operation CODE whose output mode is to
   be MODE with input operand OP whose mode was originally OP_MODE.
   Return zero if the value cannot be computed.  */
//原型 rtx simplify_const_unary_operation  rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_const_unary_operation (MtcsSimplifyRtx *self,enum rtx_code code, machine_mode mode, rtx op, machine_mode op_mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);

  scalar_int_mode result_mode;

  if (code == VEC_DUPLICATE){
      gcc_assert (mtcs_mode_is_vector_p(mtcsMode,mode));
      if (GET_MODE (op) != VOIDmode){
        if (!mtcs_mode_is_vector_p (mtcsMode,GET_MODE (op)))
          gcc_assert (mtcs_mode_get_inner (mtcsMode,mode) == GET_MODE (op));
        else
          gcc_assert (mtcs_mode_get_inner (mtcsMode,mode) == mtcs_mode_get_inner(mtcsMode,GET_MODE (op)));
      }
      if (CONST_SCALAR_INT_P (op) || CONST_DOUBLE_AS_FLOAT_P (op))
          return mtcs_rtl_gen_const_vec_duplicate (mtcsRTL,mode, op);
      if (GET_CODE (op) == CONST_VECTOR  && (CONST_VECTOR_DUPLICATE_P (op) || CONST_VECTOR_NUNITS (op).is_constant ())){
          unsigned int npatterns = (CONST_VECTOR_DUPLICATE_P (op)
                        ? CONST_VECTOR_NPATTERNS (op)
                        : CONST_VECTOR_NUNITS (op).to_constant ());
          gcc_assert (multiple_p (mtcs_mode_get_nunits(mtcsMode,mode), npatterns));
          MtcsVectorBuilder builder (mtcsMode,mode, npatterns, 1);
          for (unsigned i = 0; i < npatterns; i++)
            builder.quick_push (mtcs_rtl_const_vector_elt (mtcsRTL,op, i));
          return builder.build ();
      }
  }

  if (mtcs_mode_is_vector_p (mtcsMode,mode) && GET_CODE (op) == CONST_VECTOR
          && known_eq (mtcs_mode_get_nunits(mtcsMode,mode), CONST_VECTOR_NUNITS (op))){
      gcc_assert (GET_MODE (op) == op_mode);

      MtcsVectorBuilder builder;
      if (!builder.new_unary_operation (mode, op, false))
          return 0;

      unsigned int count = builder.encoded_nelts ();
      for (unsigned int i = 0; i < count; i++){
          rtx x = mtcs_simplify_rtx_unary_operation (self,code, mtcs_mode_get_inner (mtcsMode,mode),
                            mtcs_rtl_const_vector_elt (mtcsRTL,op, i),mtcs_mode_get_inner (mtcsMode,op_mode));
          if (!x || !valid_for_const_vector_p (mode, x))
            return 0;
          builder.quick_push (x);
      }
      return builder.build ();
  }

  /* The order of these tests is critical so that, for example, we don't
     check the wrong mode (input vs. output) for a conversion operation,
     such as FIX.  At some point, this should be simplified.  */

  if (code == FLOAT && CONST_SCALAR_INT_P (op)){
      REAL_VALUE_TYPE d;

      if (op_mode == VOIDmode){
          /* CONST_INT have VOIDmode as the mode.  We assume that all
             the bits of the constant are significant, though, this is
             a dangerous assumption as many times CONST_INTs are
             created and used with garbage in the bits outside of the
             precision of the implied mode of the const_int.  */
          op_mode = mtcsMode->modesMinMax.max_INT/*!MAX_MODE_INT*/;
      }

      mtcs_real_real_from_integer/*!real_from_integer*/(mtcsReal,&d, mode, mtcs_rtx_mode_t/*!rtx_mode_t*/(op, op_mode), SIGNED);
      /* Avoid the folding if flag_signaling_nans is on and
         operand is a signaling NaN.  */
      if (mtcs_mode_honor_snans/*!HONOR_SNANS*/ (mtcsMode,mode) && REAL_VALUE_ISSIGNALING_NAN (d))
        return 0;

      d = mtcs_real_real_value_truncate (mtcsReal,mode, d);

      /* Avoid the folding if flag_rounding_math is on and the
     conversion is not exact.  */
      if (mtcs_mode_honor_sign_dependent_rounding/*!HONOR_SIGN_DEPENDENT_ROUNDING*/ (mtcsMode,mode)){
          bool fail = false;
          wide_int w = real_to_integer (&d, &fail,mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,
                  mtcs_mode_as_a <scalar_int_mode> (mtcsMode,op_mode)));
          if (fail || wi::ne_p (w, wide_int (mtcs_rtx_mode_t/*!rtx_mode_t*/(op, op_mode))))
            return 0;
      }
      return mtcs_rtl_const_double_from_real_value/*!const_double_from_real_value*/(mtcsRTL,d, mode);
  }else if (code == UNSIGNED_FLOAT && CONST_SCALAR_INT_P (op)){
      REAL_VALUE_TYPE d;
      if (op_mode == VOIDmode){
          /* CONST_INT have VOIDmode as the mode.  We assume that all
             the bits of the constant are significant, though, this is
             a dangerous assumption as many times CONST_INTs are
             created and used with garbage in the bits outside of the
             precision of the implied mode of the const_int.  */
          op_mode = mtcsMode->modesMinMax.max_INT/*!MAX_MODE_INT*/;
      }

      mtcs_real_real_from_integer (mtcsReal,&d, mode, mtcs_rtx_mode_t/*!rtx_mode_t*/(op, op_mode), UNSIGNED);

      /* Avoid the folding if flag_signaling_nans is on and
         operand is a signaling NaN.  */
      if (mtcs_mode_honor_snans/*!HONOR_SNANS*/ (mtcsMode,mode) && REAL_VALUE_ISSIGNALING_NAN (d))
        return 0;

      d = mtcs_real_real_value_truncate (mtcsReal,mode, d);

      /* Avoid the folding if flag_rounding_math is on and the
     conversion is not exact.  */
      if (mtcs_mode_honor_sign_dependent_rounding/*!HONOR_SIGN_DEPENDENT_ROUNDING*/ (mtcsMode,mode)){
          bool fail = false;
          wide_int w = real_to_integer (&d, &fail, mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,
                  mtcs_mode_as_a <scalar_int_mode> (mtcsMode,op_mode)));
          if (fail || wi::ne_p (w, wide_int (mtcs_rtx_mode_t/*!rtx_mode_t*/(op, op_mode))))
            return 0;
      }

      return mtcs_rtl_const_double_from_real_value/*!const_double_from_real_value*/(mtcsRTL,d, mode);
  }

  if (CONST_SCALAR_INT_P (op) && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &result_mode)){
      unsigned int width = mtcs_mode_get_precision(mtcsMode,result_mode);
      if (width > MAX_BITSIZE_MODE_ANY_INT)
          return 0;

      wide_int result;
      scalar_int_mode imode = (op_mode == VOIDmode? result_mode: mtcs_mode_as_a <scalar_int_mode> (mtcsMode,op_mode));
      mtcs_rtx_mode_t op0 = mtcs_rtx_mode_t/*!rtx_mode_t*/(op, imode);
      int int_value;
      if(mtcs_config_get_value(mtcsConfig,MTCS_TARGET_SUPPORTS_WIDE_INT)==0)
//#if TARGET_SUPPORTS_WIDE_INT == 0
      /* This assert keeps the simplification from producing a result
     that cannot be represented in a CONST_DOUBLE but a lot of
     upstream callers expect that this function never fails to
     simplify something and so you if you added this to the test
     above the code would die later anyway.  If this assert
     happens, you just need to make the port support wide int.  */
         gcc_assert (width <= HOST_BITS_PER_DOUBLE_INT);
//#endif

      switch (code){
        case NOT:
          result = wi::bit_not (op0);
          break;

        case NEG:
          result = wi::neg (op0);
          break;

        case ABS:
          result = wi::abs (op0);
          break;

        case FFS:
          result = wi::shwi (wi::ffs (op0), result_mode);
          break;

        case CLZ:
          if (wi::ne_p (op0, 0))
            int_value = wi::clz (op0);
          else if (!mtcs_mode_clz_defined_value_at_zero/*!CLZ_DEFINED_VALUE_AT_ZERO (imode, int_value)*/(mtcsMode,imode, &int_value))
            return NULL_RTX;
          result = wi::shwi (int_value, result_mode);
          break;

        case CLRSB:
          result = wi::shwi (wi::clrsb (op0), result_mode);
          break;

        case CTZ:
          if (wi::ne_p (op0, 0))
            int_value = wi::ctz (op0);
          else if (! mtcs_mode_ctz_defined_value_at_zero/*!CTZ_DEFINED_VALUE_AT_ZERO (imode, int_value)*/(mtcsMode,imode,&int_value))
            return NULL_RTX;
          result = wi::shwi (int_value, result_mode);
          break;

        case POPCOUNT:
          result = wi::shwi (wi::popcount (op0), result_mode);
          break;

        case PARITY:
          result = wi::shwi (wi::parity (op0), result_mode);
          break;

        case BSWAP:
          result = wi::bswap (op0);
          break;

        case BITREVERSE:
          result = wi::bitreverse (op0);
          break;

        case TRUNCATE:
        case ZERO_EXTEND:
          result = wide_int::from (op0, width, UNSIGNED);
          break;

        case US_TRUNCATE:
        case SS_TRUNCATE:
          {
            signop sgn = code == US_TRUNCATE ? UNSIGNED : SIGNED;
            wide_int nmax
              = wide_int::from (wi::max_value (width, sgn),mtcs_mode_get_precision (mtcsMode,imode), sgn);
            wide_int nmin
              = wide_int::from (wi::min_value (width, sgn),mtcs_mode_get_precision (mtcsMode,imode), sgn);
            result = wi::min (wi::max (op0, nmin, sgn), nmax, sgn);
            result = wide_int::from (result, width, sgn);
            break;
          }
        case SIGN_EXTEND:
          result = wide_int::from (op0, width, SIGNED);
          break;

        case SS_NEG:
          if (wi::only_sign_bit_p (op0))
            result = wi::max_value (mtcs_mode_get_precision (mtcsMode,imode), SIGNED);
          else
            result = wi::neg (op0);
          break;

        case SS_ABS:
          if (wi::only_sign_bit_p (op0))
            result = wi::max_value (mtcs_mode_get_precision (mtcsMode,imode), SIGNED);
          else
            result = wi::abs (op0);
          break;

        case SQRT:
        default:
          return 0;
      }

      return mtcs_rtl_immed_wide_int_const (mtcsRTL,result, result_mode);
  }else if (CONST_DOUBLE_AS_FLOAT_P (op) && mtcs_mode_is_scalar_float_p (mtcsMode,mode)
        && mtcs_mode_is_scalar_float_p (mtcsMode,GET_MODE (op))){
      REAL_VALUE_TYPE d = *CONST_DOUBLE_REAL_VALUE (op);
      switch (code){
        case SQRT:
          return 0;
        case ABS:
          d = real_value_abs (&d);
          break;
        case NEG:
          d = real_value_negate (&d);
          break;
        case FLOAT_TRUNCATE:
          /* Don't perform the operation if flag_signaling_nans is on
             and the operand is a signaling NaN.  */
          if (mtcs_mode_honor_snans(mtcsMode,mode) && REAL_VALUE_ISSIGNALING_NAN (d))
            return NULL_RTX;
          /* Or if flag_rounding_math is on and the truncation is not
             exact.  */
          if (mtcs_mode_honor_sign_dependent_rounding/*!HONOR_SIGN_DEPENDENT_ROUNDING*/ (mtcsMode,mode)
              && !mtcs_real_exact_real_truncate/*!exact_real_truncate*/(mtcsReal,mode, &d))
            return NULL_RTX;
          d = mtcs_real_real_value_truncate (mtcsReal,mode, d);
          break;
        case FLOAT_EXTEND:
          /* Don't perform the operation if flag_signaling_nans is on
             and the operand is a signaling NaN.  */
          if (mtcs_mode_honor_snans (mtcsMode,mode) && REAL_VALUE_ISSIGNALING_NAN (d))
            return NULL_RTX;
          /* All this does is change the mode, unless changing
             mode class.  */
          if (mtcs_mode_get_class(mtcsMode,mode) != mtcs_mode_get_class (mtcsMode,GET_MODE (op)))
            mtcs_real_real_convert/*!real_convert*/(mtcsReal,&d, mode, &d);
          break;
        case FIX:
          /* Don't perform the operation if flag_signaling_nans is on
             and the operand is a signaling NaN.  */
          if (mtcs_mode_honor_snans (mtcsMode,mode) && REAL_VALUE_ISSIGNALING_NAN (d))
            return NULL_RTX;
          real_arithmetic (&d, FIX_TRUNC_EXPR, &d, NULL);
          break;
        case NOT:
          {
            long tmp[4];
            int i;

            mtcs_real_real_to_target/*!real_to_target*/(mtcsReal,tmp, &d, GET_MODE (op));
            for (i = 0; i < 4; i++)
              tmp[i] = ~tmp[i];
            mtcs_real_real_from_target (mtcsReal,&d, tmp, mode);
            break;
          }
        default:
          gcc_unreachable ();
        }
      return mtcs_rtl_const_double_from_real_value/*!const_double_from_real_value*/(mtcsRTL,d, mode);
  }else if (CONST_DOUBLE_AS_FLOAT_P (op)  && mtcs_mode_is_scalar_float_p (mtcsMode,GET_MODE (op))
       && mtcs_mode_is_int_mode(mtcsMode,mode, &result_mode)){
      unsigned int width = mtcs_mode_get_precision (mtcsMode,result_mode);
      if (width > mtcs_mode_get_max_bitsize_mode_any_int/*MAX_BITSIZE_MODE_ANY_INT*/(mtcsMode))
          return 0;

      /* Although the overflow semantics of RTL's FIX and UNSIGNED_FIX
     operators are intentionally left unspecified (to ease implementation
     by target backends), for consistency, this routine implements the
     same semantics for constant folding as used by the middle-end.  */

      /* This was formerly used only for non-IEEE float.
     eggert@twinsun.com says it is safe for IEEE also.  */
      REAL_VALUE_TYPE t;
      const REAL_VALUE_TYPE *x = CONST_DOUBLE_REAL_VALUE (op);
      wide_int wmax, wmin;
      /* This is part of the abi to real_to_integer, but we check
     things before making this call.  */
      bool fail;

      switch (code){
        case FIX:
          if (REAL_VALUE_ISNAN (*x))
            return const0_rtx;

          /* Test against the signed upper bound.  */
          wmax = wi::max_value (width, SIGNED);
          real_from_integer (&t, VOIDmode, wmax, SIGNED);
          if (real_less (&t, x))
            return mtcs_rtl_immed_wide_int_const (mtcsRTL,wmax, mode);

          /* Test against the signed lower bound.  */
          wmin = wi::min_value (width, SIGNED);
          mtcs_real_real_from_integer (mtcsReal,&t, VOIDmode, wmin, SIGNED);
          if (real_less (x, &t))
            return mtcs_rtl_immed_wide_int_const (mtcsRTL,wmin, mode);

          return mtcs_rtl_immed_wide_int_const (mtcsRTL,real_to_integer (x, &fail, width),mode);

        case UNSIGNED_FIX:
          if (REAL_VALUE_ISNAN (*x) || REAL_VALUE_NEGATIVE (*x))
            return const0_rtx;

          /* Test against the unsigned upper bound.  */
          wmax = wi::max_value (width, UNSIGNED);
          mtcs_real_real_from_integer (mtcsReal,&t, VOIDmode, wmax, UNSIGNED);
          if (real_less (&t, x))
            return mtcs_rtl_immed_wide_int_const (mtcsRTL,wmax, mode);

          return mtcs_rtl_immed_wide_int_const (mtcsRTL,real_to_integer (x, &fail, width),mode);

        default:
          gcc_unreachable ();
        }
  }
  /* Handle polynomial integers.  */
  else if (CONST_POLY_INT_P (op)){
      poly_wide_int result;
      switch (code){
        case NEG:
          result = -const_poly_int_value (op);
          break;

        case NOT:
          result = ~const_poly_int_value (op);
          break;

        default:
          return NULL_RTX;
        }
      return mtcs_rtl_immed_wide_int_const (mtcsRTL,result, mode);
  }

  return NULL_RTX;
}

/* If X is a MEM referencing the constant pool, return the real value.
   Otherwise return X.  */
//原型 avoid_constant_pool_reference rtl.h  simplify-rtx.cc
rtx mtcs_simplify_rtx_avoid_constant_pool_reference (MtcsSimplifyRtx *self,rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  rtx c, tmp, addr;
  machine_mode cmode;
  poly_int64 offset = 0;

  switch (GET_CODE (x)){
    case MEM:
      break;

    case FLOAT_EXTEND:
      /* Handle float extensions of constant pool references.  */
      tmp = XEXP (x, 0);
      c = mtcs_simplify_rtx_avoid_constant_pool_reference (self,tmp);
      if (c != tmp && CONST_DOUBLE_AS_FLOAT_P (c))
          return mtcs_rtl_const_double_from_real_value/*!const_double_from_real_value*/ (mtcsRTL,*CONST_DOUBLE_REAL_VALUE (c),GET_MODE (x));
      return x;

    default:
      return x;
  }

  if (GET_MODE (x) == mtcsMode->modes.M_BLKmode)
    return x;

  addr = XEXP (x, 0);

  /* Call target hook to avoid the effects of -fpic etc....  */
  addr = mtcsTarget->delegitimize_address/*!targetm.delegitimize_address*/(mtcsTarget,addr);

  /* Split the address into a base and integer offset.  */
  addr = strip_offset (addr, &offset);

  if (GET_CODE (addr) == LO_SUM)
    addr = XEXP (addr, 1);

  /* If this is a constant pool reference, we can turn it into its
     constant and hope that simplifications happen.  */
  if (GET_CODE (addr) == SYMBOL_REF && CONSTANT_POOL_ADDRESS_P (addr)){
      c = get_pool_constant (addr);
      cmode = get_pool_mode (addr);

      /* If we're accessing the constant in a different mode than it was
         originally stored, attempt to fix that up via subreg simplifications.
         If that fails we have no choice but to return the original memory.  */
      if (known_eq (offset, 0) && cmode == GET_MODE (x))
          return c;
      else if (known_in_range_p (offset, 0, mtcs_mode_get_size(mtcsMode,cmode))){
          rtx tem = mtcs_simplify_rtx_subreg/*!simplify_subreg*/(self,GET_MODE (x), c, cmode, offset);
          if (tem && CONSTANT_P (tem))
            return tem;
      }
  }

  return x;
}

/* Return true if binary operation OP distributes over addition in operand
   OPNO, with the other operand being held constant.  OPNO counts from 1.  */
//原型 simplify-rtx.cc
static bool distributes_over_addition_p (rtx_code op, int opno)
{
  switch (op)
    {
    case PLUS:
    case MINUS:
    case MULT:
      return true;

    case ASHIFT:
      return opno == 1;

    default:
      return false;
    }
}


//原型 simplify_const_binary_operation rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_const_binary_operation (MtcsSimplifyRtx *self,enum rtx_code code, machine_mode mode,rtx op0, rtx op1)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
    MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
    MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);

    nboolean  isVector=mtcs_mode_is_vector_p(mtcsMode,mode);

  if (isVector/*!VECTOR_MODE_P(mode)*/  && code != VEC_CONCAT
      && GET_CODE (op0) == CONST_VECTOR && GET_CODE (op1) == CONST_VECTOR){
      bool step_ok_p;
      if (CONST_VECTOR_STEPPED_P (op0)  && CONST_VECTOR_STEPPED_P (op1))
         /* We can operate directly on the encoding if:

              a3 - a2 == a2 - a1 && b3 - b2 == b2 - b1
            implies
              (a3 op b3) - (a2 op b2) == (a2 op b2) - (a1 op b1)

           Addition and subtraction are the supported operators
           for which this is true.  */
        step_ok_p = (code == PLUS || code == MINUS);
      else if (CONST_VECTOR_STEPPED_P (op0))
        /* We can operate directly on stepped encodings if:

             a3 - a2 == a2 - a1
           implies:
             (a3 op c) - (a2 op c) == (a2 op c) - (a1 op c)

           which is true if (x -> x op c) distributes over addition.  */
        step_ok_p = distributes_over_addition_p (code, 1);
      else
        /* Similarly in reverse.  */
        step_ok_p = distributes_over_addition_p (code, 2);
      MtcsVectorBuilder/*!rtx_vector_builder*/ builder;
      if (!builder.new_binary_operation (mode, op0, op1, step_ok_p))
          return 0;

      unsigned int count = builder.encoded_nelts ();
      for (unsigned int i = 0; i < count; i++){
          rtx x = mtcs_simplify_rtx_binary_operation/*!simplify_binary_operation*/(self,code, mtcs_mode_get_inner(mtcsMode,mode),
                  mtcs_rtl_const_vector_elt/*!CONST_VECTOR_ELT*/ (mtcsRTL,op0, i),mtcs_rtl_const_vector_elt/*!CONST_VECTOR_ELT*/ (mtcsRTL,op1, i));
          if (!x || !valid_for_const_vector_p (mode, x))
            return 0;
          builder.quick_push (x);
      }
      return builder.build ();
  }

  if (isVector/*!VECTOR_MODE_P(mode)*/
      && code == VEC_CONCAT  && (CONST_SCALAR_INT_P (op0) || CONST_FIXED_P (op0) || CONST_DOUBLE_AS_FLOAT_P (op0))
         && (CONST_SCALAR_INT_P (op1) || CONST_DOUBLE_AS_FLOAT_P (op1) || CONST_FIXED_P (op1))){
      /* Both inputs have a constant number of elements, so the result
         must too.  */
      poly_uint16 temp=mtcs_mode_get_nunits(mtcsMode,mode);
      unsigned n_elts =temp.to_constant();//mtcs_mode_get_nunits(mtcsMode,mode).coeffs[0].to_constant();/*!GET_MODE_NUNITS (mode).to_constant ();*/
      rtvec v = rtvec_alloc (n_elts);
      gcc_assert (n_elts >= 2);
      if (n_elts == 2){
          gcc_assert (GET_CODE (op0) != CONST_VECTOR);
          gcc_assert (GET_CODE (op1) != CONST_VECTOR);

          RTVEC_ELT (v, 0) = op0;
          RTVEC_ELT (v, 1) = op1;
      }else{
          unsigned op0_n_elts = mtcs_mode_get_nunits (mtcsMode,GET_MODE (op0)).to_constant ();
          unsigned op1_n_elts = mtcs_mode_get_nunits (mtcsMode,GET_MODE (op1)).to_constant ();
          unsigned i;

          gcc_assert (GET_CODE (op0) == CONST_VECTOR);
          gcc_assert (GET_CODE (op1) == CONST_VECTOR);
          gcc_assert (op0_n_elts + op1_n_elts == n_elts);

          for (i = 0; i < op0_n_elts; ++i)
            RTVEC_ELT (v, i) = mtcs_rtl_const_vector_elt/*!CONST_VECTOR_ELT*/ (mtcsRTL,op0, i);
          for (i = 0; i < op1_n_elts; ++i)
            RTVEC_ELT (v, op0_n_elts+i) = mtcs_rtl_const_vector_elt/*!CONST_VECTOR_ELT*/ (mtcsRTL,op1, i);
      }

      return mtcs_rtl_gen_rtx_CONST_VECTOR/*!gen_rtx_CONST_VECTOR*/ (mtcsRTL,mode, v);
  }
  nboolean isScalarFloatModeP=mtcs_mode_is_scalar_float_p(mtcsMode,mode);
  if (isScalarFloatModeP/*!SCALAR_FLOAT_MODE_P (mode)*/  && CONST_DOUBLE_AS_FLOAT_P (op0) && CONST_DOUBLE_AS_FLOAT_P (op1)
      && mode == GET_MODE (op0) && mode == GET_MODE (op1)) {
      if (code == AND || code == IOR || code == XOR){
          long tmp0[4];
          long tmp1[4];
          REAL_VALUE_TYPE r;
          int i;

          mtcs_real_real_to_target/*!real_to_target*/(mtcsReal,tmp0, CONST_DOUBLE_REAL_VALUE (op0),GET_MODE (op0));
          mtcs_real_real_to_target/*!real_to_target*/(mtcsReal,tmp1, CONST_DOUBLE_REAL_VALUE (op1),GET_MODE (op1));
          for (i = 0; i < 4; i++){
              switch (code){
                  case AND:
                    tmp0[i] &= tmp1[i];
                    break;
                  case IOR:
                    tmp0[i] |= tmp1[i];
                    break;
                  case XOR:
                    tmp0[i] ^= tmp1[i];
                    break;
                  default:
                      gcc_unreachable ();
              }
           }
           real_from_target (&r, tmp0, mode);
           return mtcs_rtl_const_double_from_real_value/*!const_double_from_real_value*/(mtcsRTL,r, mode);
      }else if (code == COPYSIGN){
          REAL_VALUE_TYPE f0, f1;
          mtcs_real_real_convert/*!real_convert*/(mtcsReal,&f0, mode, CONST_DOUBLE_REAL_VALUE (op0));
          mtcs_real_real_convert/*!real_convert*/(mtcsReal,&f1, mode, CONST_DOUBLE_REAL_VALUE (op1));
          real_copysign (&f0, &f1);
          return mtcs_rtl_const_double_from_real_value/*!const_double_from_real_value*/(mtcsRTL,f0, mode);
      }else{
          REAL_VALUE_TYPE f0, f1, value, result;
          const REAL_VALUE_TYPE *opr0, *opr1;
          bool inexact;

          opr0 = CONST_DOUBLE_REAL_VALUE (op0);
          opr1 = CONST_DOUBLE_REAL_VALUE (op1);

          if (mtcs_mode_honor_snans(mtcsMode,mode) && (REAL_VALUE_ISSIGNALING_NAN (*opr0) || REAL_VALUE_ISSIGNALING_NAN (*opr1)))
            return 0;

          mtcs_real_real_convert/*!real_convert*/(mtcsReal,&f0, mode, opr0);
          mtcs_real_real_convert/*!real_convert*/(mtcsReal,&f1, mode, opr1);

          if (code == DIV && real_equal (&f1, &dconst0)
          && (flag_trapping_math || ! mtcs_mode_has_infinities/*!MODE_HAS_INFINITIES*/(mtcsMode,mode)))
            return 0;

          if (mtcs_mode_has_infinities/*!MODE_HAS_INFINITIES*/(mtcsMode,mode)
                && mtcs_mode_honor_nans(mtcsMode,mode) && flag_trapping_math
                   && REAL_VALUE_ISINF (f0) && REAL_VALUE_ISINF (f1)){
              int s0 = REAL_VALUE_NEGATIVE (f0);
              int s1 = REAL_VALUE_NEGATIVE (f1);

              switch (code){
                case PLUS:
                  /* Inf + -Inf = NaN plus exception.  */
                  if (s0 != s1)
                    return 0;
                  break;
                case MINUS:
                  /* Inf - Inf = NaN plus exception.  */
                  if (s0 == s1)
                    return 0;
                  break;
                case DIV:
                  /* Inf / Inf = NaN plus exception.  */
                  return 0;
                default:
                  break;
              }
          }
          if (code == MULT && mtcs_mode_has_infinities/*!MODE_HAS_INFINITIES*/ (mtcsMode,mode)
              && mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,mode)
              && flag_trapping_math  && ((REAL_VALUE_ISINF (f0) && real_equal (&f1, &dconst0))
              || (REAL_VALUE_ISINF (f1)  && real_equal (&f0, &dconst0))))
                /* Inf * 0 = NaN plus exception.  */
                return 0;
          inexact = real_arithmetic (&value, rtx_to_tree_code (code),&f0, &f1);
          mtcs_real_real_convert/*!real_convert*/(mtcsReal,&result, mode, &value);

          /* Don't constant fold this floating point operation if
             the result has overflowed and flag_trapping_math.  */

          if (flag_trapping_math && mtcs_mode_has_infinities/*!MODE_HAS_INFINITIES*/ (mtcsMode,mode)  && REAL_VALUE_ISINF (result)
              && !REAL_VALUE_ISINF (f0) && !REAL_VALUE_ISINF (f1))
            /* Overflow plus exception.  */
            return 0;

          /* Don't constant fold this floating point operation if the
             result may dependent upon the run-time rounding mode and
             flag_rounding_math is set, or if GCC's software emulation
             is unable to accurately represent the result.  */

          if ((flag_rounding_math || (mtcs_mode_is_composite_p/*!MODE_COMPOSITE_P*/ (mtcsMode,mode) && !flag_unsafe_math_optimizations))
              && (inexact || !real_identical (&result, &value)))
            return NULL_RTX;

          return mtcs_rtl_const_double_from_real_value/*!const_double_from_real_value*/(mtcsRTL,result, mode);
      }
  }

  /* We can fold some multi-word operations.  */
  scalar_int_mode int_mode;
  if (mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode) && CONST_SCALAR_INT_P (op0) && CONST_SCALAR_INT_P (op1)
          && mtcs_mode_get_precision(mtcsMode,int_mode) <= mtcs_mode_get_max_bitsize_mode_any_int/*!MAX_BITSIZE_MODE_ANY_INT*/(mtcsMode)){
      wide_int result;
      wi::overflow_type overflow;
      mtcs_rtx_mode_t pop0 = mtcs_rtx_mode_t/*!rtx_mode_t*/(op0, int_mode);
      mtcs_rtx_mode_t pop1 = mtcs_rtx_mode_t/*!rtx_mode_t*/(op1, int_mode);
      if(mtcs_config_get_value(mtcsConfig,MTCS_TARGET_SUPPORTS_WIDE_INT)==0)
//#if TARGET_SUPPORTS_WIDE_INT == 0 //host=1 nvptx=1
      /* This assert keeps the simplification from producing a result
     that cannot be represented in a CONST_DOUBLE but a lot of
     upstream callers expect that this function never fails to
     simplify something and so you if you added this to the test
     above the code would die later anyway.  If this assert
     happens, you just need to make the port support wide int.  */
         gcc_assert (mtcs_mode_get_precision(mtcsMode,int_mode) <= HOST_BITS_PER_DOUBLE_INT);
//#endif
      switch (code){
        case MINUS:
          result = wi::sub (pop0, pop1);
          break;

        case PLUS:
          result = wi::add (pop0, pop1);
          break;

        case MULT:
          result = wi::mul (pop0, pop1);
          break;

        case DIV:
          result = wi::div_trunc (pop0, pop1, SIGNED, &overflow);
          if (overflow)
            return NULL_RTX;
          break;

        case MOD:
          result = wi::mod_trunc (pop0, pop1, SIGNED, &overflow);
          if (overflow)
            return NULL_RTX;
          break;

        case UDIV:
          result = wi::div_trunc (pop0, pop1, UNSIGNED, &overflow);
          if (overflow)
            return NULL_RTX;
          break;

        case UMOD:
          result = wi::mod_trunc (pop0, pop1, UNSIGNED, &overflow);
          if (overflow)
            return NULL_RTX;
          break;

        case AND:
          result = wi::bit_and (pop0, pop1);
          break;

        case IOR:
          result = wi::bit_or (pop0, pop1);
          break;

        case XOR:
          result = wi::bit_xor (pop0, pop1);
          break;

        case SMIN:
          result = wi::smin (pop0, pop1);
          break;

        case SMAX:
          result = wi::smax (pop0, pop1);
          break;

        case UMIN:
          result = wi::umin (pop0, pop1);
          break;

        case UMAX:
          result = wi::umax (pop0, pop1);
          break;

        case LSHIFTRT:
        case ASHIFTRT:
        case ASHIFT:
        case SS_ASHIFT:
        case US_ASHIFT:
          {
            /* The shift count might be in SImode while int_mode might
               be narrower.  On IA-64 it is even DImode.  If the shift
               count is too large and doesn't fit into int_mode, we'd
               ICE.  So, if int_mode is narrower than word, use
               word_mode for the shift count.  */
            if (GET_MODE (op1) == VOIDmode && mtcs_mode_get_precision (mtcsMode,int_mode) < BITS_PER_WORD)
              pop1 = mtcs_rtx_mode_t/*!rtx_mode_t*/(op1, word_mode);

            wide_int wop1 = pop1;
            if (SHIFT_COUNT_TRUNCATED)
              wop1 = wi::umod_trunc (wop1, mtcs_mode_get_precision (mtcsMode,int_mode));
            else if (wi::geu_p (wop1, mtcs_mode_get_precision (mtcsMode,int_mode)))
              return NULL_RTX;
            unsigned int precision = mtcs_mode_get_precision (mtcsMode,int_mode);
            switch (code){
              case LSHIFTRT:
                result = wi::lrshift (pop0, wop1);
                break;

              case ASHIFTRT:
                result = wi::arshift (pop0, wop1);
                break;

              case ASHIFT:
                result = wi::lshift (pop0, wop1);
                break;

              case SS_ASHIFT:
                if (wi::leu_p (wop1, wi::clrsb (pop0)))
                  result = wi::lshift (pop0, wop1);
                else if (wi::neg_p (pop0))
                  result = wi::min_value (int_mode, SIGNED);
                else
                  result =wi::max_value (precision, SIGNED)/*! wi::max_value (int_mode, SIGNED)*/;
                break;

              case US_ASHIFT:
                if (wi::eq_p (pop0, 0))
                  result = pop0;
                else if (wi::leu_p (wop1, wi::clz (pop0)))
                  result = wi::lshift (pop0, wop1);
                else
                  result =wi::max_value (precision, UNSIGNED)/*!wi::max_value (int_mode, UNSIGNED)*/;
                break;

              default:
                  gcc_unreachable ();
            }
            break;
          }
        case ROTATE:
        case ROTATERT:
          {
            /* The rotate count might be in SImode while int_mode might
               be narrower.  On IA-64 it is even DImode.  If the shift
               count is too large and doesn't fit into int_mode, we'd
               ICE.  So, if int_mode is narrower than word, use
               word_mode for the shift count.  */
            if (GET_MODE (op1) == VOIDmode && mtcs_mode_get_precision (mtcsMode,int_mode) < BITS_PER_WORD)
              pop1 = mtcs_rtx_mode_t/*!rtx_mode_t*/(op1, word_mode);

            if (wi::neg_p (pop1))
              return NULL_RTX;

            switch (code){
              case ROTATE:
                result = wi::lrotate (pop0, pop1);
                break;

              case ROTATERT:
                result = wi::rrotate (pop0, pop1);
                break;

              default:
                  gcc_unreachable ();
            }
            break;
          }

        case SS_PLUS:
          result = wi::add (pop0, pop1, SIGNED, &overflow);
     clamp_signed_saturation:
          if (overflow == wi::OVF_OVERFLOW)
            result = wi::max_value (mtcs_mode_get_precision (mtcsMode,int_mode), SIGNED);
          else if (overflow == wi::OVF_UNDERFLOW)
            result = wi::min_value (mtcs_mode_get_precision (mtcsMode,int_mode), SIGNED);
          else if (overflow != wi::OVF_NONE)
            return NULL_RTX;
          break;

        case US_PLUS:
          result = wi::add (pop0, pop1, UNSIGNED, &overflow);
     clamp_unsigned_saturation:
          if (overflow != wi::OVF_NONE)
            result = wi::max_value (mtcs_mode_get_precision (mtcsMode,int_mode), UNSIGNED);
          break;

        case SS_MINUS:
          result = wi::sub (pop0, pop1, SIGNED, &overflow);
          goto clamp_signed_saturation;

        case US_MINUS:
          result = wi::sub (pop0, pop1, UNSIGNED, &overflow);
          if (overflow != wi::OVF_NONE)
            result = wi::min_value (mtcs_mode_get_precision (mtcsMode,int_mode), UNSIGNED);
          break;

        case SS_MULT:
          result = wi::mul (pop0, pop1, SIGNED, &overflow);
          goto clamp_signed_saturation;

        case US_MULT:
          result = wi::mul (pop0, pop1, UNSIGNED, &overflow);
          goto clamp_unsigned_saturation;

        case SMUL_HIGHPART:
          result = wi::mul_high (pop0, pop1, SIGNED);
          break;

        case UMUL_HIGHPART:
          result = wi::mul_high (pop0, pop1, UNSIGNED);
          break;

        default:
          return NULL_RTX;
        }
      return mtcs_rtl_immed_wide_int_const (mtcsRTL,result, int_mode);
  }

  /* Handle polynomial integers.  */
  if (NUM_POLY_INT_COEFFS > 1  && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)
      && poly_int_rtx_p (op0) && poly_int_rtx_p (op1)) {
      poly_wide_int result;
      switch (code){
        case PLUS:
          result = mtcs_rtl_to_poly_wide/*!wi::to_poly_wide*/(op0, mode) + mtcs_rtl_to_poly_wide/*!wi::to_poly_wide*/(op1, mode);
          break;

        case MINUS:
          result = mtcs_rtl_to_poly_wide/*!wi::to_poly_wide*/(op0, mode) - mtcs_rtl_to_poly_wide/*!wi::to_poly_wide*/(op1, mode);
          break;

        case MULT:
          if (CONST_SCALAR_INT_P (op1))
            result = mtcs_rtl_to_poly_wide/*!wi::to_poly_wide*/(op0, mode) * mtcs_rtx_mode_t/*!rtx_mode_t*/(op1, mode);
          else
            return NULL_RTX;
          break;

        case ASHIFT:
          if (CONST_SCALAR_INT_P (op1)){
              wide_int shift = mtcs_rtx_mode_t/*!rtx_mode_t*/(op1,GET_MODE (op1) == VOIDmode
                    && mtcs_mode_get_precision (mtcsMode,int_mode) < BITS_PER_WORD ? word_mode : mode);
              if (SHIFT_COUNT_TRUNCATED)
                  shift = wi::umod_trunc (shift, mtcs_mode_get_precision (mtcsMode,int_mode));
              else if (wi::geu_p (shift, mtcs_mode_get_precision (mtcsMode,int_mode)))
                  return NULL_RTX;
              result = mtcs_rtl_to_poly_wide/*!wi::to_poly_wide*/(op0, mode) << shift;
          }else
            return NULL_RTX;
          break;

        case IOR:
          if (!CONST_SCALAR_INT_P (op1) || !can_ior_p (mtcs_rtl_to_poly_wide/*!wi::to_poly_wide*/(op0, mode),
                mtcs_rtx_mode_t/*!rtx_mode_t*/(op1, mode), &result))
            return NULL_RTX;
          break;

        default:
          return NULL_RTX;
      }
      return mtcs_rtl_immed_wide_int_const (mtcsRTL,result, int_mode);
  }

  return NULL_RTX;
}


/* Simplify a binary operation CODE with result mode MODE, operating on OP0
   and OP1.  Return 0 if no simplification is possible.

   Don't use this for relational operations such as EQ or LT.
   Use simplify_relational_operation instead.  */
//原型 class simplify_context simplify_binary_operation rtl.h  simplify-rtx.cc
rtx mtcs_simplify_rtx_binary_operation (MtcsSimplifyRtx *self,rtx_code code, machine_mode mode,rtx op0, rtx op1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   rtx trueop0, trueop1;
   rtx tem;

   /* Relational operations don't work here.  We must know the mode
   of the operands in order to do the comparison correctly.
   Assuming a full word can give incorrect results.
   Consider comparing 128 with -128 in QImode.  */
   gcc_assert (GET_RTX_CLASS (code) != RTX_COMPARE);
   gcc_assert (GET_RTX_CLASS (code) != RTX_COMM_COMPARE);

   /* Make sure the constant is second.  */
   if (GET_RTX_CLASS (code) == RTX_COMM_ARITH
   && mtcs_rtlanal_swap_commutative_operands_p/*!swap_commutative_operands_p*/(mtcsRtlanal, op0, op1))
      std::swap (op0, op1);
   n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_binary_operation 00 code:%d mode:%d\n",code,mode);
   mtcs_print_rtl(stderr,op0);
   n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_binary_operation 11\n");
   mtcs_print_rtl(stderr,op1);

   trueop0 = mtcs_simplify_rtx_avoid_constant_pool_reference/*!avoid_constant_pool_reference*/(self,op0);
   trueop1 = mtcs_simplify_rtx_avoid_constant_pool_reference/*!avoid_constant_pool_reference*/(self,op1);
   n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_binary_operation 22\n");
   mtcs_print_rtl(stderr,trueop0);
   n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_binary_operation 33\n");
   mtcs_print_rtl(stderr,trueop1);
   tem = mtcs_simplify_rtx_const_binary_operation/*!simplify_const_binary_operation*/(self,code, mode, trueop0, trueop1);
   if (tem){
      n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_binary_operation 44\n");
      mtcs_print_rtl(stderr,tem);
      return tem;
   }
   tem = mtcs_simplify_rtx_binary_operation_1 (self,code, mode, op0, op1, trueop0, trueop1);

   if (tem){
      n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_binary_operation 55\n");
       mtcs_print_rtl(stderr,tem);
      return tem;
   }

   /* If the above steps did not result in a simplification and op0 or op1
   were constant pool references, use the referenced constants directly.  */
   if (trueop0 != op0 || trueop1 != op1){
      tem= mtcs_simplify_rtx_gen_binary (self,code, mode, trueop0, trueop1);

      n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_binary_operation 55 tem:%p\n",tem);
       mtcs_print_rtl(stderr,tem);
      return tem;
   }

   return NULL_RTX;
}

/* Simplify a MEM based on its attributes.  This is the default
   delegitimize_address target hook, and it's recommended that every
   overrider call it.  */
//原型 delegitimize_mem_from_attrs rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_delegitimize_mem_from_attrs (MtcsSimplifyRtx *self,rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);//DECL_RTL 变成DECL_RTL_AET 需要mtcsAsm
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

  /* MEMs without MEM_OFFSETs may have been offset, so we can't just
     use their base addresses as equivalent.  */
  if (MEM_P (x) && mtcs_rtl_get_mem_expr/*!MEM_EXPR*/(mtcsRTL,x) &&
        mtcs_rtl_is_mem_offset_known_p/*!MEM_OFFSET_KNOWN_P*/(mtcsRTL,x)){
      tree decl =mtcs_rtl_get_mem_expr/*!MEM_EXPR*/(mtcsRTL,x);
      machine_mode mode = GET_MODE (x);
      poly_int64 offset = 0;
      switch (TREE_CODE (decl)){
        default:
          decl = NULL;
          break;

        case VAR_DECL:
          break;

        case ARRAY_REF:
        case ARRAY_RANGE_REF:
        case COMPONENT_REF:
        case BIT_FIELD_REF:
        case REALPART_EXPR:
        case IMAGPART_EXPR:
        case VIEW_CONVERT_EXPR:
          {
            poly_int64 bitsize, bitpos, bytepos, toffset_val = 0;
            tree toffset;
            int unsignedp, reversep, volatilep = 0;

            decl = mtcs_expr_get_inner_reference/*!get_inner_reference*/(mtcsExpr,
                  decl, &bitsize, &bitpos, &toffset, &mode,&unsignedp, &reversep, &volatilep);
            if (maybe_ne (bitsize, mtcs_mode_get_bitsize(mtcsMode,mode)) || !multiple_p (bitpos, BITS_PER_UNIT, &bytepos)
                    || (toffset && !poly_int_tree_p (toffset, &toffset_val)))
              decl = NULL;
            else
              offset += bytepos + toffset_val;
            break;
          }
      }

      if (decl && mode == GET_MODE (x)  && VAR_P (decl) && (TREE_STATIC (decl) || DECL_THREAD_LOCAL_P (decl))
              && DECL_RTL_SET_P (decl)  && MEM_P (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl))){
          rtx newx;
          offset += mtcs_rtl_get_mem_offset/*!MEM_OFFSET*/(mtcsRTL,x);
          newx = mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl);
          if (MEM_P (newx)){
              rtx n = XEXP (newx, 0), o = XEXP (x, 0);
              poly_int64 n_offset, o_offset;

              /* Avoid creating a new MEM needlessly if we already had
             the same address.  We do if there's no OFFSET and the
             old address X is identical to NEWX, or if X is of the
             form (plus NEWX OFFSET), or the NEWX is of the form
             (plus Y (const_int Z)) and X is that with the offset
             added: (plus Y (const_int Z+OFFSET)).  */
              n = strip_offset (n, &n_offset);
              o = strip_offset (o, &o_offset);
              if (!(known_eq (o_offset, n_offset + offset) && rtx_equal_p (o, n)))
                  x = mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/ (mtcsRTL,newx, mode, offset);
          }else if (GET_MODE (x) == GET_MODE (newx) && known_eq (offset, 0))
            x = newx;
      }
  }

  return x;
}

/* Simplify a byte offset BYTE into CONST_VECTOR X.  The main purpose
   is to convert a runtime BYTE value into a constant one.  */
static poly_uint64 simplify_const_vector_byte_offset (MtcsSimplifyRtx *self,rtx x, poly_uint64 byte)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsAsm *mtcsAsm=(MtcsAsm *)mtcs_target_get_asm(mtcsTarget);//DECL_RTL 变成DECL_RTL_AET 需要mtcsAsm
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  /* Cope with MODE_VECTOR_BOOL by operating on bits rather than bytes.  */
  machine_mode mode = GET_MODE (x);
  unsigned int elt_bits = vector_element_size (mtcs_mode_get_precision_poly/*GET_MODE_PRECISION*/ (mtcsMode,mode),
          mtcs_mode_get_nunits/*GET_MODE_NUNITS*/ (mtcsMode,mode));
  /* The number of bits needed to encode one element from each pattern.  */
  unsigned int sequence_bits = CONST_VECTOR_NPATTERNS (x) * elt_bits;

  /* Identify the start point in terms of a sequence number and a byte offset
     within that sequence.  */
  poly_uint64 first_sequence;
  unsigned HOST_WIDE_INT subbit;
  if (can_div_trunc_p (byte * BITS_PER_UNIT, sequence_bits, &first_sequence, &subbit)){
      unsigned int nelts_per_pattern = CONST_VECTOR_NELTS_PER_PATTERN (x);
      if (nelts_per_pattern == 1)
        /* This is a duplicated vector, so the value of FIRST_SEQUENCE
           doesn't matter.  */
        byte = subbit / BITS_PER_UNIT;
      else if (nelts_per_pattern == 2 && known_gt (first_sequence, 0U)){
          /* The subreg drops the first element from each pattern and
             only uses the second element.  Find the first sequence
             that starts on a byte boundary.  */
          subbit += least_common_multiple (sequence_bits, BITS_PER_UNIT);
          byte = subbit / BITS_PER_UNIT;
      }
  }
  return byte;
}

/* Subroutine of simplify_subreg in which:

   - X is known to be a CONST_VECTOR
   - OUTERMODE is known to be a vector mode

   Try to handle the subreg by operating on the CONST_VECTOR encoding
   rather than on each individual element of the CONST_VECTOR.

   Return the simplified subreg on success, otherwise return NULL_RTX.  */

static rtx simplify_const_vector_subreg (MtcsSimplifyRtx *self,machine_mode outermode, rtx x, machine_mode innermode, unsigned int first_byte)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsAsm *mtcsAsm=(MtcsAsm *)mtcs_target_get_asm(mtcsTarget);//DECL_RTL 变成DECL_RTL_AET 需要mtcsAsm
    MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  /* Paradoxical subregs of vectors have dubious semantics.  */
  if (mtcs_mode_paradoxical_subreg_p/*!paradoxical_subreg_p*/ (mtcsMode,outermode, innermode))
    return NULL_RTX;

  /* We can only preserve the semantics of a stepped pattern if the new
     vector element is the same as the original one.  */
  if (CONST_VECTOR_STEPPED_P (x)
      && mtcs_mode_get_inner/*!GET_MODE_INNER*/ (mtcsMode,outermode) != mtcs_mode_get_inner/*!GET_MODE_INNER*/ (mtcsMode,innermode))
    return NULL_RTX;

  /* Cope with MODE_VECTOR_BOOL by operating on bits rather than bytes.  */
  unsigned int x_elt_bits= vector_element_size (mtcs_mode_get_precision_poly(mtcsMode,innermode),
          mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/ (mtcsMode,innermode));
  unsigned int out_elt_bits= vector_element_size (mtcs_mode_get_precision_poly(mtcsMode,outermode),
          mtcs_mode_get_nunits/*!GET_MODE_NUNITS */(mtcsMode,outermode));

  /* The number of bits needed to encode one element from every pattern
     of the original vector.  */
  unsigned int x_sequence_bits = CONST_VECTOR_NPATTERNS (x) * x_elt_bits;

  /* The number of bits needed to encode one element from every pattern
     of the result.  */
  unsigned int out_sequence_bits
    = least_common_multiple (x_sequence_bits, out_elt_bits);

  /* Work out the number of interleaved patterns in the output vector
     and the number of encoded elements per pattern.  */
  unsigned int out_npatterns = out_sequence_bits / out_elt_bits;
  unsigned int nelts_per_pattern = CONST_VECTOR_NELTS_PER_PATTERN (x);

  /* The encoding scheme requires the number of elements to be a multiple
     of the number of patterns, so that each pattern appears at least once
     and so that the same number of elements appear from each pattern.  */
  bool ok_p = multiple_p (mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/ (mtcsMode,outermode), out_npatterns);
  unsigned int const_nunits;
  if (mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/ (mtcsMode,outermode).is_constant (&const_nunits)
          && (!ok_p || out_npatterns * nelts_per_pattern > const_nunits)){
      /* Either the encoding is invalid, or applying it would give us
     more elements than we need.  Just encode each element directly.  */
      out_npatterns = const_nunits;
      nelts_per_pattern = 1;
  }else if (!ok_p)
    return NULL_RTX;

  /* Get enough bytes of X to form the new encoding.  */
  unsigned int buffer_bits = out_npatterns * nelts_per_pattern * out_elt_bits;
  unsigned int buffer_bytes = CEIL (buffer_bits, BITS_PER_UNIT);
  auto_vec<target_unit, 128> buffer (buffer_bytes);
  if (!mtcs_simplify_rtx_native_encode_rtx (self,innermode, x, buffer, first_byte, buffer_bytes))
    return NULL_RTX;

  /* Reencode the bytes as OUTERMODE.  */
  return mtcs_simplify_rtx_native_decode_vector_rtx (self,outermode, buffer, 0, out_npatterns,nelts_per_pattern);
}



/* Try to calculate NUM_BYTES bytes of the target memory image of X,
   starting at byte FIRST_BYTE.  Return true on success and add the
   bytes to BYTES, such that each byte has BITS_PER_UNIT bits and such
   that the bytes follow target memory order.  Leave BYTES unmodified
   on failure.

   MODE is the mode of X.  The caller must reserve NUM_BYTES bytes in
   BYTES before calling this function.  */
//原型 native_encode_rtx rtl.h simplify-rtx.cc
bool mtcs_simplify_rtx_native_encode_rtx (MtcsSimplifyRtx *self,machine_mode mode, rtx x, vec<target_unit> &bytes,
           unsigned int first_byte, unsigned int num_bytes)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=(MtcsAsm *)mtcs_target_get_asm(mtcsTarget);//DECL_RTL 变成DECL_RTL_AET 需要mtcsAsm
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);

   /* Check the mode is sensible.  */
   gcc_assert (GET_MODE (x) == VOIDmode? mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode): mode == GET_MODE (x));

   if (GET_CODE (x) == CONST_VECTOR){
      /* CONST_VECTOR_ELT follows target memory order, so no shuffling
      is necessary.  The only complication is that MODE_VECTOR_BOOL
      vectors can have several elements per byte.  */
      unsigned int elt_bits = vector_element_size (mtcs_mode_get_precision_poly/*!GET_MODE_PRECISION*/(mtcsMode,mode),
            mtcs_mode_get_nunits/*GET_MODE_NUNITS*/ (mtcsMode,mode));
      unsigned int elt = first_byte * BITS_PER_UNIT / elt_bits;
      if (elt_bits < BITS_PER_UNIT){
         /* This is the only case in which elements can be smaller than
         a byte.  */
         gcc_assert (mtcs_mode_get_class/*!GET_MODE_CLASS*/ (mtcsMode,mode) == MODE_VECTOR_BOOL);
         auto mask = mtcs_mode_get_mask/*!GET_MODE_MASK*/ (mtcsMode,mtcs_mode_get_inner/*!GET_MODE_INNER*/ (mtcsMode,mode));
         for (unsigned int i = 0; i < num_bytes; ++i){
            target_unit value = 0;
            for (unsigned int j = 0; j < BITS_PER_UNIT; j += elt_bits){
               value |= (INTVAL (mtcs_rtl_const_vector_elt (mtcsRTL,x, elt)) & mask) << j;
               elt += 1;
            }
            bytes.quick_push (value);
         }
         return true;
      }

      unsigned int start = bytes.length ();
      unsigned int elt_bytes = mtcs_mode_get_unit_size/*!GET_MODE_UNIT_SIZE*/(mtcsMode,mode);
      /* Make FIRST_BYTE relative to ELT.  */
      first_byte %= elt_bytes;
      while (num_bytes > 0){
         /* Work out how many bytes we want from element ELT.  */
         unsigned int chunk_bytes = MIN (num_bytes, elt_bytes - first_byte);
         if (!mtcs_simplify_rtx_native_encode_rtx (self,mtcs_mode_get_inner(mtcsMode,mode),
          mtcs_rtl_const_vector_elt (mtcsRTL,x, elt), bytes,first_byte, chunk_bytes)){
            bytes.truncate (start);
            return false;
         }
         elt += 1;
         first_byte = 0;
         num_bytes -= chunk_bytes;
      }
      return true;
   }

   /* All subsequent cases are limited to scalars.  */
   scalar_mode smode;
   if (!mtcs_mode_is_a <scalar_mode> (mtcsMode,mode, &smode))
      return false;

   /* Make sure that the region is in range.  */
   unsigned int end_byte = first_byte + num_bytes;
   unsigned int mode_bytes = mtcs_mode_get_size(mtcsMode,smode);
   n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_native_encode_rtx 00 first_byte:%d num_bytes:%d mode_bytes:%d smode:%d\n",
         first_byte,num_bytes,mode_bytes,smode);
   gcc_assert (end_byte <= mode_bytes);

   if (CONST_SCALAR_INT_P (x)){
      /* The target memory layout is affected by both BYTES_BIG_ENDIAN
      and WORDS_BIG_ENDIAN.  Use the subreg machinery to get the lsb
      position of each byte.  */
      mtcs_rtx_mode_t/*!rtx_mode_t*/ value (x, smode);
      wide_int_ref value_wi (value);
      for (unsigned int byte = first_byte; byte < end_byte; ++byte){
         /* Always constant because the inputs are.  */
         unsigned int lsb = subreg_size_lsb (1, mode_bytes, byte).to_constant ();
         /* Operate directly on the encoding rather than using
         wi::extract_uhwi, so that we preserve the sign or zero
         extension for modes that are not a whole number of bits in
         size.  (Zero extension is only used for the combination of
         innermode == BImode && STORE_FLAG_VALUE == 1).  */
         unsigned int elt = lsb / HOST_BITS_PER_WIDE_INT;
         unsigned int shift = lsb % HOST_BITS_PER_WIDE_INT;
         unsigned HOST_WIDE_INT uhwi = value_wi.elt (elt);
         bytes.quick_push (uhwi >> shift);
      }
      return true;
   }

   if (CONST_DOUBLE_P (x)){
      /* real_to_target produces an array of integers in target memory order.
      All integers before the last one have 32 bits; the last one may
      have 32 bits or fewer, depending on whether the mode bitsize
      is divisible by 32.  Each of these integers is then laid out
      in target memory as any other integer would be.  */
      long el32[MAX_BITSIZE_MODE_ANY_MODE / 32];
      mtcs_real_real_to_target/*!real_to_target*/(mtcsReal,el32, CONST_DOUBLE_REAL_VALUE (x), smode);

      /* The (maximum) number of target bytes per element of el32.  */
      unsigned int bytes_per_el32 = 32 / BITS_PER_UNIT;
      gcc_assert (bytes_per_el32 != 0);

      /* Build up the integers in a similar way to the CONST_SCALAR_INT_P
      handling above.  */
      for (unsigned int byte = first_byte; byte < end_byte; ++byte){
         unsigned int index = byte / bytes_per_el32;
         unsigned int subbyte = byte % bytes_per_el32;
         unsigned int int_bytes = MIN (bytes_per_el32, mode_bytes - index * bytes_per_el32);
         /* Always constant because the inputs are.  */
         unsigned int lsb = subreg_size_lsb (1, int_bytes, subbyte).to_constant ();
         bytes.quick_push ((unsigned long) el32[index] >> lsb);
      }
      return true;
   }

   if (GET_CODE (x) == CONST_FIXED){
      for (unsigned int byte = first_byte; byte < end_byte; ++byte){
         /* Always constant because the inputs are.  */
         unsigned int lsb= subreg_size_lsb (1, mode_bytes, byte).to_constant ();
         unsigned HOST_WIDE_INT piece = CONST_FIXED_VALUE_LOW (x);
         if (lsb >= HOST_BITS_PER_WIDE_INT){
            lsb -= HOST_BITS_PER_WIDE_INT;
            piece = CONST_FIXED_VALUE_HIGH (x);
         }
         bytes.quick_push (piece >> lsb);
      }
      return true;
   }

   return false;
}

/* Try to simplify a subreg of a constant by encoding the subreg region
   as a sequence of target bytes and reading them back in the new mode.
   Return the new value on success, otherwise return null.

   The subreg has outer mode OUTERMODE, inner mode INNERMODE, inner value X
   and byte offset FIRST_BYTE.  */

static rtx simplify_immed_subreg (MtcsSimplifyRtx *self,fixed_size_mode outermode, rtx x,machine_mode innermode, unsigned int first_byte)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=(MtcsAsm *)mtcs_target_get_asm(mtcsTarget);//DECL_RTL 变成DECL_RTL_AET 需要mtcsAsm
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   unsigned int buffer_bytes = mtcs_mode_get_size(mtcsMode,outermode);
   auto_vec<target_unit, 128> buffer (buffer_bytes);
   n_debug("mtcssimplifyrtx.c simplify_immed_subreg outermode:%d innermode:%d buffer_bytes:%d first_byte:%d\n",
         outermode,innermode,buffer_bytes,first_byte);

   /* Some ports misuse CCmode.  */
   if (mtcs_mode_get_class (mtcsMode,outermode) == MODE_CC && CONST_INT_P (x))
      return x;

   /* Paradoxical subregs read undefined values for bytes outside of the
   inner value.  However, we have traditionally always sign-extended
   integer constants and zero-extended others.  */
   unsigned int inner_bytes = buffer_bytes;
   if (mtcs_mode_paradoxical_subreg_p (mtcsMode,outermode, innermode)){
      if (!mtcs_mode_get_size_poly(mtcsMode,innermode).is_constant (&inner_bytes))
         return NULL_RTX;

      target_unit filler = 0;
      if (CONST_SCALAR_INT_P (x) && wi::neg_p (mtcs_rtx_mode_t/*!rtx_mode_t*/(x, innermode)))
         filler = -1;

      /* Add any leading bytes due to big-endian layout.  The number of
      bytes must be constant because both modes have constant size.  */
      unsigned int leading_bytes= -mtcs_mode_byte_lowpart_offset (mtcsMode,outermode, innermode).to_constant ();
      for (unsigned int i = 0; i < leading_bytes; ++i)
         buffer.quick_push (filler);

      if (!mtcs_simplify_rtx_native_encode_rtx (self,innermode, x, buffer, first_byte, inner_bytes))
         return NULL_RTX;

      /* Add any trailing bytes due to little-endian layout.  */
      while (buffer.length () < buffer_bytes)
         buffer.quick_push (filler);
   }else if(mtcs_mode_get_size(mtcsMode,outermode)>mtcs_mode_get_size(mtcsMode,innermode)){
      n_debug("mtcssimplifyrtx.c simplify_immed_subreg 解决BUG,outermode:%d innermode:%d buffer_bytes:%d first_byte:%d\n",
            outermode,innermode,buffer_bytes,first_byte);
      //解决BUG 033
      return NULL_RTX;
   }else if (!mtcs_simplify_rtx_native_encode_rtx (self,innermode, x, buffer, first_byte, inner_bytes))
      return NULL_RTX;
   rtx ret = mtcs_simplify_rtx_native_decode_rtx (self,outermode, buffer, 0);
   if (ret && mtcs_mode_is_float_p(mtcsMode,outermode)){
      auto_vec<target_unit, 128> buffer2 (buffer_bytes);
      if (!mtcs_simplify_rtx_native_encode_rtx (self,outermode, ret, buffer2, 0, buffer_bytes))
         return NULL_RTX;
      for (unsigned int i = 0; i < buffer_bytes; ++i)
         if (buffer[i] != buffer2[i])
            return NULL_RTX;
   }
   return ret;
}


/* Simplify SUBREG:OUTERMODE(OP:INNERMODE, BYTE)
   Return 0 if no simplifications are possible.  */
//原型 simplify_context::simplify_subreg rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_subreg (MtcsSimplifyRtx *self,machine_mode outermode, rtx op,machine_mode innermode, poly_uint64 byte)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsAsm *mtcsAsm=(MtcsAsm *)mtcs_target_get_asm(mtcsTarget);//DECL_RTL 变成DECL_RTL_AET 需要mtcsAsm
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  /* Little bit of sanity checking.  */
  gcc_assert (innermode != VOIDmode);
  gcc_assert (outermode != VOIDmode);
  gcc_assert (innermode != mtcsMode->modes.M_BLKmode);
  gcc_assert (outermode != mtcsMode->modes.M_BLKmode);

  gcc_assert (GET_MODE (op) == innermode || GET_MODE (op) == VOIDmode);

  poly_uint64 outersize = mtcs_mode_get_size(mtcsMode,outermode);
  if (!multiple_p (byte, outersize))
    return NULL_RTX;

  poly_uint64 innersize = mtcs_mode_get_size(mtcsMode,innermode);
  if (maybe_ge (byte, innersize))
    return NULL_RTX;

  if (outermode == innermode && known_eq (byte, 0U))
    return op;

  if (GET_CODE (op) == CONST_VECTOR)
    byte = simplify_const_vector_byte_offset (self,op, byte);

  if (multiple_p (byte, mtcs_mode_get_unit_size(mtcsMode,innermode))){
      rtx elt;
      if (mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/ (mtcsMode,outermode)
          && mtcs_mode_get_inner(mtcsMode,outermode) == mtcs_mode_get_inner(mtcsMode,innermode)
          && vec_duplicate_p (op, &elt))
          return gen_vec_duplicate (outermode, elt);

      if (outermode == mtcs_mode_get_inner(mtcsMode,innermode)  && vec_duplicate_p (op, &elt))
          return elt;
  }

  if (CONST_SCALAR_INT_P (op) || CONST_DOUBLE_AS_FLOAT_P (op) || CONST_FIXED_P (op) || GET_CODE (op) == CONST_VECTOR){
      unsigned HOST_WIDE_INT cbyte;
      if (byte.is_constant (&cbyte)){
          if (GET_CODE (op) == CONST_VECTOR && mtcs_mode_is_vector_p(mtcsMode,outermode)){
              rtx tmp = simplify_const_vector_subreg (self,outermode, op,innermode, cbyte);
              if (tmp)
                  return tmp;
          }
          fixed_size_mode fs_outermode;
          n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_subreg 00 outermode:%d innermode:%d\n",outermode,innermode);
          if (mtcs_mode_is_a <fixed_size_mode> (mtcsMode,outermode, &fs_outermode))
            return simplify_immed_subreg (self,fs_outermode, op, innermode, cbyte);
      }
  }

  /* Changing mode twice with SUBREG => just change it once,
     or not at all if changing back op starting mode.  */
  if (GET_CODE (op) == SUBREG){
      machine_mode innermostmode = GET_MODE (SUBREG_REG (op));
      poly_uint64 innermostsize = mtcs_mode_get_size (mtcsMode,innermostmode);
      rtx newx;

      if (outermode == innermostmode   && known_eq (byte, 0U) && known_eq (SUBREG_BYTE (op), 0))
          return SUBREG_REG (op);

      /* Work out the memory offset of the final OUTERMODE value relative
     to the inner value of OP.  */
      poly_int64 mem_offset = mtcs_rtl_subreg_memory_offset (mtcsRTL,outermode,innermode, byte);
      poly_int64 op_mem_offset = mtcs_rtl_subreg_memory_offset_with_rtx/*!subreg_memory_offset*/(mtcsRTL,op);
      poly_int64 final_offset = mem_offset + op_mem_offset;

      /* See whether resulting subreg will be paradoxical.  */
      if (!mtcs_mode_paradoxical_subreg_p (mtcsMode,outermode, innermostmode)){
          /* Bail out in case resulting subreg would be incorrect.  */
          if (maybe_lt (final_offset, 0) || maybe_ge (poly_uint64 (final_offset), innermostsize)|| !multiple_p (final_offset, outersize))
            return NULL_RTX;
      }else{
          poly_int64 required_offset = mtcs_rtl_subreg_memory_offset (mtcsRTL,outermode,innermostmode, 0);
          if (maybe_ne (final_offset, required_offset))
            return NULL_RTX;
          /* Paradoxical subregs always have byte offset 0.  */
          final_offset = 0;
      }

      /* Recurse for further possible simplifications.  */
      newx = mtcs_simplify_rtx_subreg/*simplify_subreg*/(self,outermode, SUBREG_REG (op), innermostmode,final_offset);
      if (newx)
          return newx;
      if (mtcs_rtl_validate_subreg (mtcsRTL,outermode, innermostmode,SUBREG_REG (op), final_offset)){
          newx = mtcs_rtl_gen_rtx_SUBREG (mtcsRTL,outermode, SUBREG_REG (op), final_offset);
          if (SUBREG_PROMOTED_VAR_P (op) && SUBREG_PROMOTED_SIGN (op) >= 0 && mtcs_mode_get_class(mtcsMode,outermode) == MODE_INT
                  && known_ge (outersize, innersize) && known_le (outersize, innermostsize)
                  && mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,newx)){
              SUBREG_PROMOTED_VAR_P (newx) = 1;
              SUBREG_PROMOTED_SET (newx, SUBREG_PROMOTED_GET (op));
          }
          return newx;
      }
      return NULL_RTX;
  }

  /* SUBREG of a hard register => just change the register number
     and/or mode.  If the hard register is not valid in that mode,
     suppress this simplification.  If the hard register is the stack,
     frame, or argument pointer, leave this as a SUBREG.  */

  if (REG_P (op) && mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,op)){
      unsigned int regno, final_regno;

      regno = REGNO (op);
      final_regno = mtcs_rtl_simplify_subreg_regno (mtcsRTL,regno, innermode, byte, outermode);
      if (mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,final_regno)){
         rtx x = gen_rtx_REG_offset (op, outermode, final_regno,mtcs_rtl_subreg_memory_offset (mtcsRTL,outermode,innermode, byte));

      /* Propagate original regno.  We don't have any way to specify
         the offset inside original regno, so do so only for lowpart.
         The information is used only by alias analysis that cannot
         grog partial register anyway.  */

          if (known_eq (mtcs_mode_subreg_lowpart_offset (mtcsMode,outermode, innermode), byte))
            ORIGINAL_REGNO (x) = ORIGINAL_REGNO (op);
          return x;
      }
  }

  /* If we have a SUBREG of a register that we are replacing and we are
     replacing it with a MEM, make a new MEM and try replacing the
     SUBREG with it.  Don't do this if the MEM has a mode-dependent address
     or if we would be widening it.  */

  if (MEM_P (op) && ! mtcs_recog_mode_dependent_address_p/*!mode_dependent_address_p*/(mtcsRecog,XEXP (op, 0),
          mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/ (mtcsRTL,op))
      /* Allow splitting of volatile memory references in case we don't
         have instruction to move the whole thing.  */
      && (! MEM_VOLATILE_P (op) || ! mtcs_optabs_have_insn_for/*!have_insn_for*/(mtcsOptabs,SET, innermode))
      && !(mtcs_align_get_strict_alignment/*!STRICT_ALIGNMENT*/(mtcsAlign)
      && mtcs_rtl_get_mem_align/*!MEM_ALIGN*/ (mtcsRTL,op) < mtcs_mode_get_alignment(mtcsMode,outermode))
      && known_le (outersize, innersize))
    return mtcs_rtl_adjust_address_nv (mtcsRTL,op, outermode, byte);

  /* Handle complex or vector values represented as CONCAT or VEC_CONCAT
     of two parts.  */
  if (GET_CODE (op) == CONCAT  || GET_CODE (op) == VEC_CONCAT){
      poly_uint64 final_offset;
      rtx part, res;

      machine_mode part_mode = GET_MODE (XEXP (op, 0));
      if (part_mode == VOIDmode)
          part_mode = mtcs_mode_get_inner (mtcsMode,GET_MODE (op));
      poly_uint64 part_size = mtcs_mode_get_size (mtcsMode,part_mode);
      if (known_lt (byte, part_size)){
          part = XEXP (op, 0);
          final_offset = byte;
      }else if (known_ge (byte, part_size)){
          part = XEXP (op, 1);
          final_offset = byte - part_size;
      }else
          return NULL_RTX;

      if (maybe_gt (final_offset + outersize, part_size))
          return NULL_RTX;

      part_mode = GET_MODE (part);
      if (part_mode == VOIDmode)
          part_mode = mtcs_mode_get_inner (mtcsMode,GET_MODE (op));
      res = mtcs_simplify_rtx_subreg(self,outermode, part, part_mode, final_offset);
      if (res)
          return res;
      if (mtcs_rtl_validate_subreg (mtcsRTL,outermode, part_mode, part, final_offset))
          return mtcs_rtl_gen_rtx_SUBREG (mtcsRTL,outermode, part, final_offset);
      return NULL_RTX;
  }

  /* Simplify
    (subreg (vec_merge (X)
               (vector)
               (const_int ((1 << N) | M)))
        (N * sizeof (outermode)))
     to
    (subreg (X) (N * sizeof (outermode)))
   */
  unsigned int idx;
  if (constant_multiple_p (byte, mtcs_mode_get_size (mtcsMode,outermode), &idx)
      && idx < HOST_BITS_PER_WIDE_INT
      && GET_CODE (op) == VEC_MERGE
      && mtcs_mode_get_inner (mtcsMode,innermode) == outermode
      && CONST_INT_P (XEXP (op, 2))
      && (UINTVAL (XEXP (op, 2)) & (HOST_WIDE_INT_1U << idx)) != 0)
    return mtcs_simplify_rtx_gen_subreg (self,outermode, XEXP (op, 0), innermode, byte);

  /* A SUBREG resulting from a zero extension may fold to zero if
     it extracts higher bits that the ZERO_EXTEND's source bits.  */
  if (GET_CODE (op) == ZERO_EXTEND && mtcs_mode_is_scalar_int_p (mtcsMode,innermode)){
      poly_uint64 bitpos = mtcs_rtlanal_subreg_lsb_1/*!subreg_lsb_1*/(mtcsRtlanal,outermode, innermode, byte);
      if (known_ge (bitpos, mtcs_mode_get_precision(mtcsMode,GET_MODE (XEXP (op, 0)))))
          return CONST0_RTX (outermode);
  }

  /* Optimize SUBREGS of scalar integral ASHIFT by a valid constant.  */
  if (GET_CODE (op) == ASHIFT
      && mtcs_mode_is_scalar_int_p (mtcsMode,innermode)
      && CONST_INT_P (XEXP (op, 1))
      && INTVAL (XEXP (op, 1)) > 0
      && known_gt (mtcs_mode_get_bitsize (mtcsMode,innermode), INTVAL (XEXP (op, 1)))){
      HOST_WIDE_INT val = INTVAL (XEXP (op, 1));
      /* A lowpart SUBREG of a ASHIFT by a constant may fold to zero.  */
      if (known_eq (mtcs_mode_subreg_lowpart_offset (mtcsMode,outermode, innermode), byte)
              && known_le (mtcs_mode_get_bitsize (mtcsMode,outermode), val))
        return CONST0_RTX (outermode);
      /* Optimize the highpart SUBREG of a suitable ASHIFT (ZERO_EXTEND).  */
      if (GET_CODE (XEXP (op, 0)) == ZERO_EXTEND
          && GET_MODE (XEXP (XEXP (op, 0), 0)) == outermode
          && known_eq (mtcs_mode_get_bitsize (mtcsMode,outermode), val)
          && known_eq (mtcs_mode_get_bitsize (mtcsMode,innermode), 2 * val)
          && known_eq (mtcs_mode_subreg_highpart_offset (mtcsMode,outermode, innermode), byte))
          return XEXP (XEXP (op, 0), 0);
  }

  /* Attempt to simplify WORD_MODE SUBREGs of bitwise expressions.  */
  if (outermode == word_mode && (GET_CODE (op) == IOR || GET_CODE (op) == XOR || GET_CODE (op) == AND)
      && mtcs_mode_is_scalar_int_p (mtcsMode,innermode)){
      n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_subreg 11 outermode:%d innermode:%d byte:%d\n",outermode,innermode,byte);
      rtx op0 = mtcs_simplify_rtx_subreg (self,outermode, XEXP (op, 0), innermode, byte);
      rtx op1 = mtcs_simplify_rtx_subreg (self,outermode, XEXP (op, 1), innermode, byte);
      if (op0 && op1)
          return mtcs_simplify_rtx_gen_binary (self,GET_CODE (op), outermode, op0, op1);
  }

  scalar_int_mode int_outermode, int_innermode;
  if (mtcs_mode_is_a <scalar_int_mode> (mtcsMode,outermode, &int_outermode)
          && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,innermode, &int_innermode)
      && known_eq (byte, mtcs_mode_subreg_lowpart_offset (mtcsMode,int_outermode, int_innermode))){
      /* Handle polynomial integers.  The upper bits of a paradoxical
     subreg are undefined, so this is safe regardless of whether
     we're truncating or extending.  */
      if (CONST_POLY_INT_P (op)){
          poly_wide_int val = poly_wide_int::from (const_poly_int_value (op),
                  mtcs_mode_get_precision (mtcsMode,int_outermode),SIGNED);
          return mtcs_rtl_immed_wide_int_const (mtcsRTL,val, int_outermode);
      }

      if (mtcs_mode_get_precision (mtcsMode,int_outermode) < mtcs_mode_get_precision (mtcsMode,int_innermode)){
          rtx tem = mtcs_simplify_rtx_truncation (self,int_outermode, op, int_innermode);
          if (tem)
            return tem;
      }
  }

  /* If the outer mode is not integral, try taking a subreg with the equivalent
     integer outer mode and then bitcasting the result.
     Other simplifications rely on integer to integer subregs and we'd
     potentially miss out on optimizations otherwise.  */
  if (known_gt (mtcs_mode_get_size (mtcsMode,innermode),
          mtcs_mode_get_size (mtcsMode,outermode))
      && mtcs_mode_is_scalar_int_p (mtcsMode,innermode)
      && !mtcs_mode_is_scalar_int_p (mtcsMode,outermode)
      && mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,mtcs_mode_get_bitsize (mtcsMode,outermode),0).exists (&int_outermode)){
      rtx tem = mtcs_simplify_rtx_subreg (self,int_outermode, op, innermode, byte);
      if (tem)
          return mtcs_simplify_rtx_lowpart_subreg (self,outermode, tem, int_outermode);
  }

  /* If OP is a vector comparison and the subreg is not changing the
     number of elements or the size of the elements, change the result
     of the comparison to the new mode.  */
  if (COMPARISON_P (op)
      && mtcs_mode_is_vector_p (mtcsMode,outermode)
      && mtcs_mode_is_vector_p (mtcsMode,innermode)
      && known_eq (mtcs_mode_get_nunits (mtcsMode,outermode), mtcs_mode_get_nunits (mtcsMode,innermode))
      && known_eq (mtcs_mode_get_unit_size (mtcsMode,outermode),
              mtcs_mode_get_unit_size (mtcsMode,innermode)))
    return mtcs_simplify_rtx_gen_relational (self,GET_CODE (op), outermode, innermode,XEXP (op, 0), XEXP (op, 1));
  return NULL_RTX;
}

/* Make a SUBREG operation or equivalent if it folds.  */
//原型 simplify_context::simplify_gen_subreg rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_gen_subreg (MtcsSimplifyRtx *self,machine_mode outermode, rtx op,machine_mode innermode,poly_uint64 byte)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsAsm *mtcsAsm=(MtcsAsm *)mtcs_target_get_asm(mtcsTarget);//DECL_RTL 变成DECL_RTL_AET 需要mtcsAsm
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_gen_subreg 00 outermode:%d innermode:%d byte:%d\n",outermode,innermode,byte);

  rtx newx;
  newx = mtcs_simplify_rtx_subreg(self,outermode, op, innermode, byte);
  if (newx)
    return newx;

  if (GET_CODE (op) == SUBREG || GET_CODE (op) == CONCAT || GET_MODE (op) == VOIDmode)
    return NULL_RTX;

  if (mtcs_mode_is_composite_p (mtcsMode,outermode)
      && (CONST_SCALAR_INT_P (op)
      || CONST_DOUBLE_AS_FLOAT_P (op)
      || CONST_FIXED_P (op)
      || GET_CODE (op) == CONST_VECTOR))
    return NULL_RTX;

  if (mtcs_rtl_validate_subreg (mtcsRTL,outermode, innermode, op, byte))
    return mtcs_rtl_gen_rtx_SUBREG (mtcsRTL,outermode, op, byte);

  return NULL_RTX;
}

/* Make a binary operation by properly ordering the operands and
   seeing if the expression folds.  */
//原型  simplify_context::simplify_gen_binary rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_gen_binary (MtcsSimplifyRtx *self,rtx_code code, machine_mode mode,rtx op0, rtx op1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  rtx tem;
  /* If this simplifies, do it.  */
  tem = mtcs_simplify_rtx_binary_operation (self,code, mode, op0, op1);
  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_gen_binary--%d %d temp:%p\n",code,mode,tem);
  if (tem)
    return tem;
  /* Put complex operands first and constants second if commutative.  */
  if (GET_RTX_CLASS (code) == RTX_COMM_ARITH
        && mtcs_rtlanal_swap_commutative_operands_p/*!swap_commutative_operands_p*/(mtcsRtlanal, op0, op1))
    std::swap (op0, op1);
  return gen_rtx_fmt_ee (code, mode, op0, op1);
}

//原型  rtx simplify_binary_operation_1 (rtx_code, machine_mode, rtx, rtx, rtx, rtx); rtl.h simplify-rtx.cc
//2000多行
rtx mtcs_simplify_rtx_binary_operation_1 (MtcsSimplifyRtx *self,rtx_code code,machine_mode mode,rtx op0, rtx op1,rtx trueop0, rtx trueop1)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsAsm *mtcsAsm=(MtcsAsm *)mtcs_target_get_asm(mtcsTarget);//DECL_RTL 变成DECL_RTL_AET 需要mtcsAsm
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsReg *mtcsReg = mtcs_target_get_reg(mtcsTarget);

  rtx tem, reversed, opleft, opright, elt0, elt1;
  HOST_WIDE_INT val;
  scalar_int_mode int_mode, inner_mode;
  poly_int64 offset;
  /* Even if we can't compute a constant result,
     there are some cases worth simplifying.  */
  switch (code){
    case PLUS:
      /* Maybe simplify x + 0 to x.  The two expressions are equivalent
     when x is NaN, infinite, or finite and nonzero.  They aren't
     when x is -0 and the rounding mode is not towards -infinity,
     since (-0) + 0 is then 0.  */
      if (!mtcs_mode_honor_signed_zeros/*!HONOR_SIGNED_ZEROS*/ (mtcsMode,mode)
              && !mtcs_mode_honor_snans/*!HONOR_SNANS*/ (mtcsMode,mode) && trueop1 == CONST0_RTX (mode))
          return op0;

      /* ((-a) + b) -> (b - a) and similarly for (a + (-b)).  These
     transformations are safe even for IEEE.  */
      if (GET_CODE (op0) == NEG)
          return mtcs_simplify_rtx_gen_binary (self,MINUS, mode, op1, XEXP (op0, 0));
      else if (GET_CODE (op1) == NEG)
          return mtcs_simplify_rtx_gen_binary (self,MINUS, mode, op0, XEXP (op1, 0));

      /* (~a) + 1 -> -a */
      if (mtcs_mode_is_integral_p (mtcsMode,mode) && GET_CODE (op0) == NOT  && trueop1 == const1_rtx)
          return mtcs_simplify_rtx_gen_unary(self,NEG, mode, XEXP (op0, 0), mode);

      /* Handle both-operands-constant cases.  We can only add
     CONST_INTs to constants since the sum of relocatable symbols
     can't be handled by most assemblers.  Don't add CONST_INT
     to CONST_INT since overflow won't be computed properly if wider
     than HOST_BITS_PER_WIDE_INT.  */

      if ((GET_CODE (op0) == CONST || GET_CODE (op0) == SYMBOL_REF || GET_CODE (op0) == LABEL_REF) && poly_int_rtx_p (op1, &offset))
          return mtcs_rtl_plus_constant (mtcsRTL,mode, op0, offset);
      else if ((GET_CODE (op1) == CONST || GET_CODE (op1) == SYMBOL_REF || GET_CODE (op1) == LABEL_REF) && poly_int_rtx_p (op0, &offset))
          return mtcs_rtl_plus_constant (mtcsRTL,mode, op1, offset);

      /* See if this is something like X * C - X or vice versa or
     if the multiplication is written as a shift.  If so, we can
     distribute and make a new multiply, shift, or maybe just
     have X (if C is 2 in the example above).  But don't make
     something more expensive than we had before.  */

      if (mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)){
          rtx lhs = op0, rhs = op1;

          wide_int coeff0 = wi::one (mtcs_mode_get_precision(mtcsMode,int_mode));
          wide_int coeff1 = wi::one (mtcs_mode_get_precision (mtcsMode,int_mode));

          if (GET_CODE (lhs) == NEG){
              coeff0 = wi::minus_one (mtcs_mode_get_precision (mtcsMode,int_mode));
              lhs = XEXP (lhs, 0);
          }else if (GET_CODE (lhs) == MULT && CONST_SCALAR_INT_P (XEXP (lhs, 1))){
              coeff0 = mtcs_rtx_mode_t/*!rtx_mode_t*/(XEXP (lhs, 1), int_mode);
              lhs = XEXP (lhs, 0);
          }else if (GET_CODE (lhs) == ASHIFT && CONST_INT_P (XEXP (lhs, 1)) && INTVAL (XEXP (lhs, 1)) >= 0
               && INTVAL (XEXP (lhs, 1)) < mtcs_mode_get_precision (mtcsMode,int_mode)){
              coeff0 = wi::set_bit_in_zero (INTVAL (XEXP (lhs, 1)), mtcs_mode_get_precision (mtcsMode,int_mode));
              lhs = XEXP (lhs, 0);
          }

          if (GET_CODE (rhs) == NEG){
              coeff1 = wi::minus_one (mtcs_mode_get_precision (mtcsMode,int_mode));
              rhs = XEXP (rhs, 0);
          }else if (GET_CODE (rhs) == MULT && CONST_INT_P (XEXP (rhs, 1))){
              coeff1 = mtcs_rtx_mode_t/*!rtx_mode_t*/(XEXP (rhs, 1), int_mode);
              rhs = XEXP (rhs, 0);
          }else if (GET_CODE (rhs) == ASHIFT  && CONST_INT_P (XEXP (rhs, 1)) && INTVAL (XEXP (rhs, 1)) >= 0
               && INTVAL (XEXP (rhs, 1)) < mtcs_mode_get_precision (mtcsMode,int_mode)){
              coeff1 = wi::set_bit_in_zero (INTVAL (XEXP (rhs, 1)),mtcs_mode_get_precision (mtcsMode,int_mode));
              rhs = XEXP (rhs, 0);
          }

          if (rtx_equal_p (lhs, rhs)){
              rtx orig = gen_rtx_PLUS (int_mode, op0, op1);
              rtx coeff;
              bool speed = optimize_function_for_speed_p (cfun);
              coeff = mtcs_rtl_immed_wide_int_const (mtcsRTL,coeff0 + coeff1, int_mode);
              tem = mtcs_simplify_rtx_gen_binary (self,MULT, int_mode, lhs, coeff);
              return (mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,tem, int_mode, speed) <=
                      mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,orig, int_mode, speed) ? tem : 0);
          }

          /* Optimize (X - 1) * Y + Y to X * Y.  */
          lhs = op0;
          rhs = op1;
          if (GET_CODE (op0) == MULT){
              if (((GET_CODE (XEXP (op0, 0)) == PLUS && XEXP (XEXP (op0, 0), 1) == constm1_rtx)
               || (GET_CODE (XEXP (op0, 0)) == MINUS && XEXP (XEXP (op0, 0), 1) == const1_rtx))
               && rtx_equal_p (XEXP (op0, 1), op1))
                  lhs = XEXP (XEXP (op0, 0), 0);
              else if (((GET_CODE (XEXP (op0, 1)) == PLUS  && XEXP (XEXP (op0, 1), 1) == constm1_rtx)
                || (GET_CODE (XEXP (op0, 1)) == MINUS && XEXP (XEXP (op0, 1), 1) == const1_rtx))
                   && rtx_equal_p (XEXP (op0, 0), op1))
                  lhs = XEXP (XEXP (op0, 1), 0);
          }else if (GET_CODE (op1) == MULT){
              if (((GET_CODE (XEXP (op1, 0)) == PLUS
                && XEXP (XEXP (op1, 0), 1) == constm1_rtx)
               || (GET_CODE (XEXP (op1, 0)) == MINUS
                   && XEXP (XEXP (op1, 0), 1) == const1_rtx))
              && rtx_equal_p (XEXP (op1, 1), op0))
                  rhs = XEXP (XEXP (op1, 0), 0);
              else if (((GET_CODE (XEXP (op1, 1)) == PLUS
                 && XEXP (XEXP (op1, 1), 1) == constm1_rtx)
                || (GET_CODE (XEXP (op1, 1)) == MINUS
                    && XEXP (XEXP (op1, 1), 1) == const1_rtx))
                   && rtx_equal_p (XEXP (op1, 0), op0))
                  rhs = XEXP (XEXP (op1, 1), 0);
          }
          if (lhs != op0 || rhs != op1)
            return mtcs_simplify_rtx_gen_binary (self,MULT, int_mode, lhs, rhs);
      }

      /* (plus (xor X C1) C2) is (xor X (C1^C2)) if C2 is signbit.  */
      if (CONST_SCALAR_INT_P (op1)  && GET_CODE (op0) == XOR
              && CONST_SCALAR_INT_P (XEXP (op0, 1))  && mtcs_simplify_rtx_mode_signbit_p(self,mode, op1))
          return mtcs_simplify_rtx_gen_binary (self,XOR, mode, XEXP (op0, 0),
                  mtcs_simplify_rtx_gen_binary (self,XOR, mode, op1, XEXP (op0, 1)));

      /* Canonicalize (plus (mult (neg B) C) A) to (minus A (mult B C)).  */
      if (!mtcs_mode_honor_sign_dependent_rounding/*!HONOR_SIGN_DEPENDENT_ROUNDING*/ (mtcsMode,mode)
              && GET_CODE (op0) == MULT  && GET_CODE (XEXP (op0, 0)) == NEG){
          rtx in1, in2;
          in1 = XEXP (XEXP (op0, 0), 0);
          in2 = XEXP (op0, 1);
          return mtcs_simplify_rtx_gen_binary (self,MINUS, mode, op1,
                  mtcs_simplify_rtx_gen_binary (self,MULT, mode,in1, in2));
      }

      /* (plus (comparison A B) C) can become (neg (rev-comp A B)) if
     C is 1 and STORE_FLAG_VALUE is -1 or if C is -1 and STORE_FLAG_VALUE
     is 1.  */
      if (COMPARISON_P (op0)  && ((mtcs_real_get_store_flag_value/*!STORE_FLAG_VALUE*/(mtcsReal) == -1 && trueop1 == const1_rtx)
          || (mtcs_real_get_store_flag_value/*!STORE_FLAG_VALUE*/(mtcsReal) == 1
                  && trueop1 == constm1_rtx))  && (reversed = reversed_comparison (op0, mode)))
          return
      mtcs_simplify_rtx_gen_unary (self,NEG, mode, reversed, mode);

      /* If one of the operands is a PLUS or a MINUS, see if we can
     simplify this by the associative law.
     Don't use the associative law for floating point.
     The inaccuracy makes it nonassociative,
     and subtle programs can break if operations are associated.  */

      if (mtcs_mode_is_integral_p (mtcsMode,mode) && (plus_minus_operand_p (op0)
          || plus_minus_operand_p (op1))  && (tem = mtcs_simplify_rtx_plus_minus (self,code, mode, op0, op1)) != 0)
          return tem;

      /* Reassociate floating point addition only when the user
     specifies associative math operations.  */
      if (mtcs_mode_is_float_p (mtcsMode,mode) && flag_associative_math){
          tem = mtcs_simplify_rtx_associative_operation (self,code, mode, op0, op1);
          if (tem)
            return tem;
      }

      /* Handle vector series.  */
      if (mtcs_mode_get_class/*!GET_MODE_CLASS*/ (mtcsMode,mode) == MODE_VECTOR_INT){
          tem = mtcs_simplify_rtx_binary_operation_series (self,code, mode, op0, op1);
          if (tem)
            return tem;
      }
      break;

    case COMPARE:
      /* Convert (compare (gt (flags) 0) (lt (flags) 0)) to (flags).  */
      if (((GET_CODE (op0) == GT && GET_CODE (op1) == LT) || (GET_CODE (op0) == GTU && GET_CODE (op1) == LTU))
              && XEXP (op0, 1) == const0_rtx && XEXP (op1, 1) == const0_rtx){
          rtx xop00 = XEXP (op0, 0);
          rtx xop10 = XEXP (op1, 0);

          if (REG_P (xop00) && REG_P (xop10)
              && REGNO (xop00) == REGNO (xop10)
              && GET_MODE (xop00) == mode
              && GET_MODE (xop10) == mode
              && mtcs_mode_get_class (mtcsMode,mode) == MODE_CC)
            return xop00;
      }
      break;

    case MINUS:
      /* We can't assume x-x is 0 even with non-IEEE floating point,
     but since it is zero except in very strange circumstances, we
     will treat it as zero with -ffinite-math-only.  */
      if (rtx_equal_p (trueop0, trueop1)
      && ! side_effects_p (op0)
      && (!mtcs_mode_is_float_p (mtcsMode,mode) || !mtcs_mode_has_nans (mtcsMode,mode)))
          return CONST0_RTX (mode);

      /* Change subtraction from zero into negation.  (0 - x) is the
     same as -x when x is NaN, infinite, or finite and nonzero.
     But if the mode has signed zeros, and does not round towards
     -infinity, then 0 - 0 is 0, not -0.  */
      if (!mtcs_mode_honor_signed_zeros (mtcsMode,mode) && trueop0 == CONST0_RTX (mode))
          return mtcs_simplify_rtx_gen_unary (self,NEG, mode, op1, mode);

      /* (-1 - a) is ~a, unless the expression contains symbolic
     constants, in which case not retaining additions and
     subtractions could cause invalid assembly to be produced.  */
      if (trueop0 == CONSTM1_RTX (mode) && !contains_symbolic_reference_p (op1))
          return mtcs_simplify_rtx_gen_unary(self,NOT, mode, op1, mode);

      /* Subtracting 0 has no effect unless the mode has signalling NaNs,
     or has signed zeros and supports rounding towards -infinity.
     In such a case, 0 - 0 is -0.  */
      if (!(mtcs_mode_honor_signed_zeros (mtcsMode,mode) && mtcs_mode_honor_sign_dependent_rounding (mtcsMode,mode))
              && !mtcs_mode_honor_snans (mtcsMode,mode) && trueop1 == CONST0_RTX (mode))
          return op0;

      /* See if this is something like X * C - X or vice versa or
     if the multiplication is written as a shift.  If so, we can
     distribute and make a new multiply, shift, or maybe just
     have X (if C is 2 in the example above).  But don't make
     something more expensive than we had before.  */

      if (mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)){
          rtx lhs = op0, rhs = op1;

          wide_int coeff0 = wi::one (mtcs_mode_get_precision(mtcsMode,int_mode));
          wide_int negcoeff1 = wi::minus_one (mtcs_mode_get_precision (mtcsMode,int_mode));

          if (GET_CODE (lhs) == NEG){
              coeff0 = wi::minus_one (mtcs_mode_get_precision (mtcsMode,int_mode));
              lhs = XEXP (lhs, 0);
          }else if (GET_CODE (lhs) == MULT && CONST_SCALAR_INT_P (XEXP (lhs, 1))){
              coeff0 = mtcs_rtx_mode_t/*!rtx_mode_t*/(XEXP (lhs, 1), int_mode);
              lhs = XEXP (lhs, 0);
          }else if (GET_CODE (lhs) == ASHIFT  && CONST_INT_P (XEXP (lhs, 1))
               && INTVAL (XEXP (lhs, 1)) >= 0  && INTVAL (XEXP (lhs, 1)) < mtcs_mode_get_precision (mtcsMode,int_mode)){
              coeff0 = wi::set_bit_in_zero (INTVAL (XEXP (lhs, 1)),mtcs_mode_get_precision (mtcsMode,int_mode));
              lhs = XEXP (lhs, 0);
          }

          if (GET_CODE (rhs) == NEG) {
              negcoeff1 = wi::one (mtcs_mode_get_precision (mtcsMode,int_mode));
              rhs = XEXP (rhs, 0);
          }else if (GET_CODE (rhs) == MULT && CONST_INT_P (XEXP (rhs, 1))){
              negcoeff1 = wi::neg (mtcs_rtx_mode_t/*!rtx_mode_t*/(XEXP (rhs, 1), int_mode));
              rhs = XEXP (rhs, 0);
          }else if (GET_CODE (rhs) == ASHIFT  && CONST_INT_P (XEXP (rhs, 1))
               && INTVAL (XEXP (rhs, 1)) >= 0 && INTVAL (XEXP (rhs, 1)) < mtcs_mode_get_precision (mtcsMode,int_mode)){
              negcoeff1 = wi::set_bit_in_zero (INTVAL (XEXP (rhs, 1)), mtcs_mode_get_precision (mtcsMode,int_mode));
              negcoeff1 = -negcoeff1;
              rhs = XEXP (rhs, 0);
          }

          if (rtx_equal_p (lhs, rhs)){
              rtx orig = gen_rtx_MINUS (int_mode, op0, op1);
              rtx coeff;
              bool speed = optimize_function_for_speed_p (cfun);
              coeff = mtcs_rtl_immed_wide_int_const (mtcsRTL,coeff0 + negcoeff1, int_mode);
              tem = mtcs_simplify_rtx_gen_binary (self,MULT, int_mode, lhs, coeff);
              return (mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,tem, int_mode, speed) <=
                      mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,orig, int_mode, speed) ? tem : 0);
          }

          /* Optimize (X + 1) * Y - Y to X * Y.  */
          lhs = op0;
          if (GET_CODE (op0) == MULT){
              if (((GET_CODE (XEXP (op0, 0)) == PLUS
                && XEXP (XEXP (op0, 0), 1) == const1_rtx)
               || (GET_CODE (XEXP (op0, 0)) == MINUS
                   && XEXP (XEXP (op0, 0), 1) == constm1_rtx))
              && rtx_equal_p (XEXP (op0, 1), op1))
                  lhs = XEXP (XEXP (op0, 0), 0);
              else if (((GET_CODE (XEXP (op0, 1)) == PLUS
                 && XEXP (XEXP (op0, 1), 1) == const1_rtx)
                || (GET_CODE (XEXP (op0, 1)) == MINUS
                    && XEXP (XEXP (op0, 1), 1) == constm1_rtx))
                   && rtx_equal_p (XEXP (op0, 0), op1))
                  lhs = XEXP (XEXP (op0, 1), 0);
          }
          if (lhs != op0)
            return mtcs_simplify_rtx_gen_binary (self,MULT, int_mode, lhs, op1);
      }

      /* (a - (-b)) -> (a + b).  True even for IEEE.  */
      if (GET_CODE (op1) == NEG)
          return mtcs_simplify_rtx_gen_binary (self,PLUS, mode, op0, XEXP (op1, 0));

      /* (-x - c) may be simplified as (-c - x).  */
      if (GET_CODE (op0) == NEG && (CONST_SCALAR_INT_P (op1) || CONST_DOUBLE_AS_FLOAT_P (op1))){
          tem = mtcs_simplify_rtx_unary_operation (self,NEG, mode, op1, mode);
          if (tem)
            return mtcs_simplify_rtx_gen_binary (self,MINUS, mode, tem, XEXP (op0, 0));
      }

      if ((GET_CODE (op0) == CONST  || GET_CODE (op0) == SYMBOL_REF || GET_CODE (op0) == LABEL_REF)
              && poly_int_rtx_p (op1, &offset))
          return mtcs_rtl_plus_constant (mtcsRTL,mode, op0, mtcs_mode_trunc_int_for_mode_with_poly_int64 (mtcsMode,-offset, mode));

      /* Don't let a relocatable value get a negative coeff.  */
      if (poly_int_rtx_p (op1) && GET_MODE (op0) != VOIDmode)
          return mtcs_simplify_rtx_gen_binary (self,PLUS, mode,op0,neg_poly_int_rtx (self,mode, op1));

      /* (x - (x & y)) -> (x & ~y) */
      if (mtcs_mode_is_integral_p (mtcsMode,mode) && GET_CODE (op1) == AND){
          if (rtx_equal_p (op0, XEXP (op1, 0))){
              tem = mtcs_simplify_rtx_gen_unary (self,NOT, mode, XEXP (op1, 1),GET_MODE (XEXP (op1, 1)));
              return mtcs_simplify_rtx_gen_binary (self,AND, mode, op0, tem);
          }
          if (rtx_equal_p (op0, XEXP (op1, 1))){
              tem = mtcs_simplify_rtx_gen_unary (self,NOT, mode, XEXP (op1, 0),GET_MODE (XEXP (op1, 0)));
              return mtcs_simplify_rtx_gen_binary (self,AND, mode, op0, tem);
          }
      }

      /* If STORE_FLAG_VALUE is 1, (minus 1 (comparison foo bar)) can be done
     by reversing the comparison code if valid.  */
      if (mtcs_real_get_store_flag_value/*!STORE_FLAG_VALUE*/(mtcsReal) == 1
              && trueop0 == const1_rtx  && COMPARISON_P (op1) && (reversed = reversed_comparison (op1, mode)))
          return reversed;

      /* Canonicalize (minus A (mult (neg B) C)) to (plus (mult B C) A).  */
      if (!mtcs_mode_honor_sign_dependent_rounding (mtcsMode,mode)
              && GET_CODE (op1) == MULT  && GET_CODE (XEXP (op1, 0)) == NEG){
          rtx in1, in2;

          in1 = XEXP (XEXP (op1, 0), 0);
          in2 = XEXP (op1, 1);
          return mtcs_simplify_rtx_gen_binary (self,PLUS, mode,mtcs_simplify_rtx_gen_binary (self,MULT, mode,in1, in2),op0);
      }

      /* Canonicalize (minus (neg A) (mult B C)) to
     (minus (mult (neg B) C) A).  */
      if (!mtcs_mode_honor_sign_dependent_rounding (mtcsMode,mode)
              && GET_CODE (op1) == MULT && GET_CODE (op0) == NEG){
          rtx in1, in2;

          in1 = mtcs_simplify_rtx_gen_unary (self,NEG, mode, XEXP (op1, 0), mode);
          in2 = XEXP (op1, 1);
          return mtcs_simplify_rtx_gen_binary (self,MINUS, mode,mtcs_simplify_rtx_gen_binary (self,MULT, mode,in1, in2),XEXP (op0, 0));
      }

      /* If one of the operands is a PLUS or a MINUS, see if we can
     simplify this by the associative law.  This will, for example,
         canonicalize (minus A (plus B C)) to (minus (minus A B) C).
     Don't use the associative law for floating point.
     The inaccuracy makes it nonassociative,
     and subtle programs can break if operations are associated.  */

      if (mtcs_mode_is_integral_p (mtcsMode,mode) && (plus_minus_operand_p (op0)
          || plus_minus_operand_p (op1)) && (tem = mtcs_simplify_rtx_plus_minus (self,code, mode, op0, op1)) != 0)
          return tem;

      /* Handle vector series.  */
      if (mtcs_mode_get_class (mtcsMode,mode) == MODE_VECTOR_INT){
          tem = mtcs_simplify_rtx_binary_operation_series (self,code, mode, op0, op1);
          if (tem)
            return tem;
      }
      break;

    case MULT:
      if (trueop1 == constm1_rtx)
          return mtcs_simplify_rtx_gen_unary (self,NEG, mode, op0, mode);

      if (GET_CODE (op0) == NEG){
          rtx temp = mtcs_simplify_rtx_unary_operation (self,NEG, mode, op1, mode);
          /* If op1 is a MULT as well and simplify_unary_operation
             just moved the NEG to the second operand, simplify_gen_binary
             below could through simplify_associative_operation move
             the NEG around again and recurse endlessly.  */
          if (temp
              && GET_CODE (op1) == MULT
              && GET_CODE (temp) == MULT
              && XEXP (op1, 0) == XEXP (temp, 0)
              && GET_CODE (XEXP (temp, 1)) == NEG
              && XEXP (op1, 1) == XEXP (XEXP (temp, 1), 0))
            temp = NULL_RTX;
          if (temp)
            return mtcs_simplify_rtx_gen_binary (self,MULT, mode, XEXP (op0, 0), temp);
      }
      if (GET_CODE (op1) == NEG){
          rtx temp = mtcs_simplify_rtx_unary_operation (self,NEG, mode, op0, mode);
          /* If op0 is a MULT as well and simplify_unary_operation
             just moved the NEG to the second operand, simplify_gen_binary
             below could through simplify_associative_operation move
             the NEG around again and recurse endlessly.  */
          if (temp
              && GET_CODE (op0) == MULT
              && GET_CODE (temp) == MULT
              && XEXP (op0, 0) == XEXP (temp, 0)
              && GET_CODE (XEXP (temp, 1)) == NEG
              && XEXP (op0, 1) == XEXP (XEXP (temp, 1), 0))
            temp = NULL_RTX;
          if (temp)
            return mtcs_simplify_rtx_gen_binary (self,MULT, mode, temp, XEXP (op1, 0));
      }

      /* Maybe simplify x * 0 to 0.  The reduction is not valid if
     x is NaN, since x * 0 is then also NaN.  Nor is it valid
     when the mode has signed zeros, since multiplying a negative
     number by 0 will give -0, not 0.  */
      if (!mtcs_mode_honor_nans (mtcsMode,mode)  && !mtcs_mode_honor_signed_zeros (mtcsMode,mode)
              && trueop1 == CONST0_RTX (mode) && ! side_effects_p (op0))
          return op1;

      /* In IEEE floating point, x*1 is not equivalent to x for
     signalling NaNs.  */
      if (!mtcs_mode_honor_snans (mtcsMode,mode)  && trueop1 == CONST1_RTX (mode))
          return op0;

      /* Convert multiply by constant power of two into shift.  */
      if (self->mem_depth == 0 && CONST_SCALAR_INT_P (trueop1)){
          val = wi::exact_log2 (mtcs_rtx_mode_t/*!rtx_mode_t*/(trueop1, mode));
          if (val >= 0)
            return mtcs_simplify_rtx_gen_binary (self,ASHIFT, mode, op0,mtcs_rtl_gen_int_shift_amount(mtcsRTL,mode, val));
      }

      /* x*2 is x+x and x*(-1) is -x */
      if (CONST_DOUBLE_AS_FLOAT_P (trueop1)   && mtcs_mode_is_scalar_float_p (mtcsMode,GET_MODE (trueop1))
              && !mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/ (mtcsMode,GET_MODE (trueop1)) && GET_MODE (op0) == mode){
          const REAL_VALUE_TYPE *d1 = CONST_DOUBLE_REAL_VALUE (trueop1);

          if (real_equal (d1, &dconst2))
            return mtcs_simplify_rtx_gen_binary (self,PLUS, mode, op0, copy_rtx (op0));

          if (!mtcs_mode_honor_snans (mtcsMode,mode)
              && real_equal (d1, &dconstm1))
            return mtcs_simplify_rtx_gen_unary (self,NEG, mode, op0, mode);
      }

      /* Optimize -x * -x as x * x.  */
      if (mtcs_mode_is_float_p(mtcsMode,mode)  && GET_CODE (op0) == NEG && GET_CODE (op1) == NEG
          && rtx_equal_p (XEXP (op0, 0), XEXP (op1, 0)) && !side_effects_p (XEXP (op0, 0)))
          return mtcs_simplify_rtx_gen_binary (self,MULT, mode, XEXP (op0, 0), XEXP (op1, 0));

      /* Likewise, optimize abs(x) * abs(x) as x * x.  */
      if (mtcs_mode_is_scalar_float_p(mtcsMode,mode) && GET_CODE (op0) == ABS  && GET_CODE (op1) == ABS
              && rtx_equal_p (XEXP (op0, 0), XEXP (op1, 0)) && !side_effects_p (XEXP (op0, 0)))
          return mtcs_simplify_rtx_gen_binary (self,MULT, mode, XEXP (op0, 0), XEXP (op1, 0));

      /* Reassociate multiplication, but for floating point MULTs
     only when the user specifies unsafe math optimizations.  */
      if (! mtcs_mode_is_float_p(mtcsMode,mode) || flag_unsafe_math_optimizations){
          tem = mtcs_simplify_rtx_associative_operation (self,code, mode, op0, op1);
          if (tem)
            return tem;
      }
      break;

    case IOR:
      if (trueop1 == CONST0_RTX (mode))
          return op0;
      if (mtcs_mode_is_integral_p(mtcsMode,mode)  && trueop1 == CONSTM1_RTX (mode) && !side_effects_p (op0))
          return op1;
      if (rtx_equal_p (trueop0, trueop1) && ! side_effects_p (op0))
          return op0;
      /* A | (~A) -> -1 */
      if (((GET_CODE (op0) == NOT && rtx_equal_p (XEXP (op0, 0), op1))
          || (GET_CODE (op1) == NOT && rtx_equal_p (XEXP (op1, 0), op0)))
          && ! side_effects_p (op0)
          && mtcs_mode_get_class(mtcsMode,mode) != MODE_CC)
          return CONSTM1_RTX (mode);

      /* (ior A C) is C if all bits of A that might be nonzero are on in C.  */
      if (CONST_INT_P (op1) && mtcs_mode_is_hwi_computable_p(mtcsMode,mode)
          && (mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(mtcsRtlanal,op0, mode) & ~UINTVAL (op1)) == 0 && !side_effects_p (op0))
          return op1;

      /* Canonicalize (X & C1) | C2.  */
      if (GET_CODE (op0) == AND  && CONST_INT_P (trueop1) && CONST_INT_P (XEXP (op0, 1))){
          HOST_WIDE_INT mask = mtcs_mode_get_mask(mtcsMode,mode);
          HOST_WIDE_INT c1 = INTVAL (XEXP (op0, 1));
          HOST_WIDE_INT c2 = INTVAL (trueop1);

          /* If (C1&C2) == C1, then (X&C1)|C2 becomes C2.  */
          if ((c1 & c2) == c1 && !side_effects_p (XEXP (op0, 0)))
            return trueop1;

          /* If (C1|C2) == ~0 then (X&C1)|C2 becomes X|C2.  */
          if (((c1|c2) & mask) == mask)
            return mtcs_simplify_rtx_gen_binary (self,IOR, mode, XEXP (op0, 0), op1);
      }

      /* Convert (A & B) | A to A.  */
      if (GET_CODE (op0) == AND   && (rtx_equal_p (XEXP (op0, 0), op1) || rtx_equal_p (XEXP (op0, 1), op1))
              && ! side_effects_p (XEXP (op0, 0))  && ! side_effects_p (XEXP (op0, 1)))
          return op1;

      /* Convert (ior (ashift A CX) (lshiftrt A CY)) where CX+CY equals the
         mode size to (rotate A CX).  */

      if (GET_CODE (op1) == ASHIFT || GET_CODE (op1) == SUBREG){
          opleft = op1;
          opright = op0;
      }else{
          opright = op1;
          opleft = op0;
      }

      if (GET_CODE (opleft) == ASHIFT && GET_CODE (opright) == LSHIFTRT
          && rtx_equal_p (XEXP (opleft, 0), XEXP (opright, 0))
          && CONST_INT_P (XEXP (opleft, 1))
          && CONST_INT_P (XEXP (opright, 1))
          && (INTVAL (XEXP (opleft, 1)) + INTVAL (XEXP (opright, 1))
          == mtcs_mode_get_unit_precision/*GET_MODE_UNIT_PRECISION*/(mtcsMode,mode)))
        return gen_rtx_ROTATE (mode, XEXP (opright, 0), XEXP (opleft, 1));

      /* Same, but for ashift that has been "simplified" to a wider mode
        by simplify_shift_const.  */

      if (GET_CODE (opleft) == SUBREG
              && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)
              && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (SUBREG_REG (opleft)),&inner_mode)
              && GET_CODE (SUBREG_REG (opleft)) == ASHIFT
              && GET_CODE (opright) == LSHIFTRT
              && GET_CODE (XEXP (opright, 0)) == SUBREG
              && known_eq (SUBREG_BYTE (opleft), SUBREG_BYTE (XEXP (opright, 0)))
              && mtcs_mode_get_size(mtcsMode,int_mode) < mtcs_mode_get_size(mtcsMode,inner_mode)
              && rtx_equal_p (XEXP (SUBREG_REG (opleft), 0),SUBREG_REG (XEXP (opright, 0)))
              && CONST_INT_P (XEXP (SUBREG_REG (opleft), 1))
              && CONST_INT_P (XEXP (opright, 1))
              && (INTVAL (XEXP (SUBREG_REG (opleft), 1)) + INTVAL (XEXP (opright, 1)) == mtcs_mode_get_precision(mtcsMode,int_mode)))
          return gen_rtx_ROTATE (int_mode, XEXP (opright, 0),XEXP (SUBREG_REG (opleft), 1));

      /* If OP0 is (ashiftrt (plus ...) C), it might actually be
         a (sign_extend (plus ...)).  Then check if OP1 is a CONST_INT and
     the PLUS does not affect any of the bits in OP1: then we can do
     the IOR as a PLUS and we can associate.  This is valid if OP1
         can be safely shifted left C bits.  */
      if (CONST_INT_P (trueop1) && GET_CODE (op0) == ASHIFTRT
          && GET_CODE (XEXP (op0, 0)) == PLUS
          && CONST_INT_P (XEXP (XEXP (op0, 0), 1))
          && CONST_INT_P (XEXP (op0, 1))
          && INTVAL (XEXP (op0, 1)) < HOST_BITS_PER_WIDE_INT){
          int count = INTVAL (XEXP (op0, 1));
          HOST_WIDE_INT mask = UINTVAL (trueop1) << count;
          if (mask >> count == INTVAL (trueop1)
                  && mtcs_mode_trunc_int_for_mode (mtcsMode,mask, mode) == mask && (mask & mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(mtcsRtlanal,XEXP (op0, 0), mode)) == 0)
              return mtcs_simplify_rtx_gen_binary (self,ASHIFTRT, mode,mtcs_rtl_plus_constant(mtcsRTL,mode, XEXP (op0, 0),mask),XEXP (op0, 1));
      }

      /* The following happens with bitfield merging.
         (X & C) | ((X | Y) & ~C) -> X | (Y & ~C) */
      if (GET_CODE (op0) == AND
          && GET_CODE (op1) == AND
          && CONST_INT_P (XEXP (op0, 1))
          && CONST_INT_P (XEXP (op1, 1))
          && (INTVAL (XEXP (op0, 1)) == ~INTVAL (XEXP (op1, 1)))){
          /* The IOR may be on both sides.  */
          rtx top0 = NULL_RTX, top1 = NULL_RTX;
          if (GET_CODE (XEXP (op1, 0)) == IOR)
            top0 = op0, top1 = op1;
          else if (GET_CODE (XEXP (op0, 0)) == IOR)
            top0 = op1, top1 = op0;
          if (top0 && top1){
              /* X may be on either side of the inner IOR.  */
              rtx tem = NULL_RTX;
              if (rtx_equal_p (XEXP (top0, 0),XEXP (XEXP (top1, 0), 0)))
                  tem = XEXP (XEXP (top1, 0), 1);
              else if (rtx_equal_p (XEXP (top0, 0),XEXP (XEXP (top1, 0), 1)))
                  tem = XEXP (XEXP (top1, 0), 0);
              if (tem)
                  return mtcs_simplify_rtx_gen_binary (self,IOR, mode, XEXP (top0, 0),
                    mtcs_simplify_rtx_gen_binary(self,AND, mode, tem, XEXP (top1, 1)));
          }
      }

      /* Convert (ior (and A C) (and B C)) into (and (ior A B) C).  */
      if (GET_CODE (op0) == GET_CODE (op1)
          && (GET_CODE (op0) == AND
          || GET_CODE (op0) == IOR
          || GET_CODE (op0) == LSHIFTRT
          || GET_CODE (op0) == ASHIFTRT
          || GET_CODE (op0) == ASHIFT
          || GET_CODE (op0) == ROTATE
          || GET_CODE (op0) == ROTATERT)){
          tem = mtcs_simplify_rtx_distributive_operation(self,code, mode, op0, op1);
          if (tem)
            return tem;
      }
      tem = mtcs_simplify_rtx_byte_swapping_operation(self,code, mode, op0, op1);
      if (tem)
          return tem;

      tem = mtcs_simplify_rtx_associative_operation (self,code, mode, op0, op1);
      if (tem)
          return tem;

      tem = mtcs_simplify_rtx_logical_relational_operation (self,code, mode, op0, op1);
      if (tem)
          return tem;
      break;

    case XOR:
      if (trueop1 == CONST0_RTX (mode))
          return op0;
      if (mtcs_mode_is_integral_p(mtcsMode,mode) && trueop1 == CONSTM1_RTX (mode))
          return mtcs_simplify_rtx_gen_unary(self,NOT, mode, op0, mode);
      if (rtx_equal_p (trueop0, trueop1)  && ! side_effects_p (op0)  && mtcs_mode_get_class(mtcsMode,mode) != MODE_CC)
          return CONST0_RTX (mode);

      /* Canonicalize XOR of the most significant bit to PLUS.  */
      if (CONST_SCALAR_INT_P (op1)  && mtcs_simplify_rtx_mode_signbit_p(self,mode, op1))
          return mtcs_simplify_rtx_gen_binary (self,PLUS, mode, op0, op1);
      /* (xor (plus X C1) C2) is (xor X (C1^C2)) if C1 is signbit.  */
      if (CONST_SCALAR_INT_P (op1) && GET_CODE (op0) == PLUS && CONST_SCALAR_INT_P (XEXP (op0, 1))
          && mtcs_simplify_rtx_mode_signbit_p(self,mode, XEXP (op0, 1)))
          return mtcs_simplify_rtx_gen_binary (self,XOR, mode, XEXP (op0, 0),
                    mtcs_simplify_rtx_gen_binary (self,XOR, mode, op1,XEXP (op0, 1)));

      /* If we are XORing two things that have no bits in common,
     convert them into an IOR.  This helps to detect rotation encoded
     using those methods and possibly other simplifications.  */

      if (mtcs_mode_is_hwi_computable_p(mtcsMode,mode) && (mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(mtcsRtlanal,op0, mode) & mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(mtcsRtlanal,op1, mode)) == 0)
          return (mtcs_simplify_rtx_gen_binary (self,IOR, mode, op0, op1));

      /* Convert (XOR (NOT x) (NOT y)) to (XOR x y).
     Also convert (XOR (NOT x) y) to (NOT (XOR x y)), similarly for
     (NOT y).  */
      {
        int num_negated = 0;

        if (GET_CODE (op0) == NOT)
          num_negated++, op0 = XEXP (op0, 0);
        if (GET_CODE (op1) == NOT)
          num_negated++, op1 = XEXP (op1, 0);

        if (num_negated == 2)
          return mtcs_simplify_rtx_gen_binary (self,XOR, mode, op0, op1);
        else if (num_negated == 1)
          return mtcs_simplify_rtx_gen_unary(self,NOT, mode,
                         mtcs_simplify_rtx_gen_binary (self,XOR, mode, op0, op1), mode);
      }

      /* Convert (xor (and A B) B) to (and (not A) B).  The latter may
     correspond to a machine insn or result in further simplifications
     if B is a constant.  */

      if (GET_CODE (op0) == AND && rtx_equal_p (XEXP (op0, 1), op1)  && ! side_effects_p (op1))
        return mtcs_simplify_rtx_gen_binary (self,AND, mode,
                        mtcs_simplify_rtx_gen_unary(self,NOT, mode,XEXP (op0, 0), mode), op1);

      else if (GET_CODE (op0) == AND && rtx_equal_p (XEXP (op0, 0), op1) && ! side_effects_p (op1))
        return mtcs_simplify_rtx_gen_binary (self,AND, mode,
                        mtcs_simplify_rtx_gen_unary(self,NOT, mode,XEXP (op0, 1), mode), op1);

      /* Given (xor (ior (xor A B) C) D), where B, C and D are
     constants, simplify to (xor (ior A C) (B&~C)^D), canceling
     out bits inverted twice and not set by C.  Similarly, given
     (xor (and (xor A B) C) D), simplify without inverting C in
     the xor operand: (xor (and A C) (B&C)^D).
      */
      else if ((GET_CODE (op0) == IOR || GET_CODE (op0) == AND) && GET_CODE (XEXP (op0, 0)) == XOR && CONST_INT_P (op1)
           && CONST_INT_P (XEXP (op0, 1)) && CONST_INT_P (XEXP (XEXP (op0, 0), 1))){
          enum rtx_code op = GET_CODE (op0);
          rtx a = XEXP (XEXP (op0, 0), 0);
          rtx b = XEXP (XEXP (op0, 0), 1);
          rtx c = XEXP (op0, 1);
          rtx d = op1;
          HOST_WIDE_INT bval = INTVAL (b);
          HOST_WIDE_INT cval = INTVAL (c);
          HOST_WIDE_INT dval = INTVAL (d);
          HOST_WIDE_INT xcval;

          if (op == IOR)
            xcval = ~cval;
          else
            xcval = cval;

          return mtcs_simplify_rtx_gen_binary (self,XOR, mode,
                          mtcs_simplify_rtx_gen_binary (self,op, mode, a, c),
                          mtcs_rtl_gen_int_mode(mtcsRTL,(bval & xcval) ^ dval, mode));
      }

      /* Given (xor (and A B) C), using P^Q == (~P&Q) | (~Q&P),
     we can transform like this:
            (A&B)^C == ~(A&B)&C | ~C&(A&B)
                    == (~A|~B)&C | ~C&(A&B)    * DeMorgan's Law
                    == ~A&C | ~B&C | A&(~C&B)  * Distribute and re-order
     Attempt a few simplifications when B and C are both constants.  */
      if (GET_CODE (op0) == AND  && CONST_INT_P (op1)  && CONST_INT_P (XEXP (op0, 1))){
          rtx a = XEXP (op0, 0);
          rtx b = XEXP (op0, 1);
          rtx c = op1;
          HOST_WIDE_INT bval = INTVAL (b);
          HOST_WIDE_INT cval = INTVAL (c);

          /* Instead of computing ~A&C, we compute its negated value,
             ~(A|~C).  If it yields -1, ~A&C is zero, so we can
             optimize for sure.  If it does not simplify, we still try
             to compute ~A&C below, but since that always allocates
             RTL, we don't try that before committing to returning a
             simplified expression.  */
          rtx n_na_c = simplify_binary_operation (IOR, mode, a, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,~cval));

          if ((~cval & bval) == 0){
              rtx na_c = NULL_RTX;
              if (n_na_c)
                  na_c = mtcs_simplify_rtx_gen_unary(self,NOT, mode, n_na_c, mode);
              else{
                  /* If ~A does not simplify, don't bother: we don't
                     want to simplify 2 operations into 3, and if na_c
                     were to simplify with na, n_na_c would have
                     simplified as well.  */
                  rtx na = mtcs_simplify_rtx_unary_operation (self,NOT, mode, a, mode);
                  if (na)
                    na_c = mtcs_simplify_rtx_gen_binary (self,AND, mode, na, c);
              }

              /* Try to simplify ~A&C | ~B&C.  */
              if (na_c != NULL_RTX)
                  return mtcs_simplify_rtx_gen_binary (self,IOR, mode, na_c, mtcs_rtl_gen_int_mode(mtcsRTL,~bval & cval, mode));
          }else{
              /* If ~A&C is zero, simplify A&(~C&B) | ~B&C.  */
              if (n_na_c == CONSTM1_RTX (mode)){
                  rtx a_nc_b = mtcs_simplify_rtx_gen_binary (self,AND, mode, a,mtcs_rtl_gen_int_mode(mtcsRTL,~cval & bval,mode));
                  return mtcs_simplify_rtx_gen_binary (self,IOR, mode, a_nc_b, mtcs_rtl_gen_int_mode(mtcsRTL,~bval & cval,mode));
              }
          }
      }

      /* If we have (xor (and (xor A B) C) A) with C a constant we can instead
     do (ior (and A ~C) (and B C)) which is a machine instruction on some
     machines, and also has shorter instruction path length.  */
      if (GET_CODE (op0) == AND  && GET_CODE (XEXP (op0, 0)) == XOR
              && CONST_INT_P (XEXP (op0, 1)) && rtx_equal_p (XEXP (XEXP (op0, 0), 0), trueop1)){
          rtx a = trueop1;
          rtx b = XEXP (XEXP (op0, 0), 1);
          rtx c = XEXP (op0, 1);
          rtx nc = mtcs_simplify_rtx_gen_unary(self,NOT, mode, c, mode);
          rtx a_nc = mtcs_simplify_rtx_gen_binary (self,AND, mode, a, nc);
          rtx bc = mtcs_simplify_rtx_gen_binary (self,AND, mode, b, c);
          return mtcs_simplify_rtx_gen_binary (self,IOR, mode, a_nc, bc);
      }
      /* Similarly, (xor (and (xor A B) C) B) as (ior (and A C) (and B ~C))  */
      else if (GET_CODE (op0) == AND  && GET_CODE (XEXP (op0, 0)) == XOR
              && CONST_INT_P (XEXP (op0, 1)) && rtx_equal_p (XEXP (XEXP (op0, 0), 1), trueop1)){
          rtx a = XEXP (XEXP (op0, 0), 0);
          rtx b = trueop1;
          rtx c = XEXP (op0, 1);
          rtx nc = mtcs_simplify_rtx_gen_unary(self,NOT, mode, c, mode);
          rtx b_nc = mtcs_simplify_rtx_gen_binary (self,AND, mode, b, nc);
          rtx ac = mtcs_simplify_rtx_gen_binary (self,AND, mode, a, c);
          return mtcs_simplify_rtx_gen_binary (self,IOR, mode, ac, b_nc);
      }

      /* (xor (comparison foo bar) (const_int 1)) can become the reversed
     comparison if STORE_FLAG_VALUE is 1.  */
      if (mtcs_real_get_store_flag_value(mtcsReal) == 1  && trueop1 == const1_rtx  && COMPARISON_P (op0)
          && (reversed = reversed_comparison (op0, mode)))
          return reversed;

      /* (lshiftrt foo C) where C is the number of bits in FOO minus 1
     is (lt foo (const_int 0)), so we can perform the above
     simplification if STORE_FLAG_VALUE is 1.  */

      if (mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)
      && mtcs_real_get_store_flag_value(mtcsReal) == 1
      && trueop1 == const1_rtx
      && GET_CODE (op0) == LSHIFTRT
      && CONST_INT_P (XEXP (op0, 1))
      && INTVAL (XEXP (op0, 1)) == mtcs_mode_get_precision(mtcsMode,int_mode) - 1)
          return gen_rtx_GE (int_mode, XEXP (op0, 0), const0_rtx);

      /* (xor (comparison foo bar) (const_int sign-bit))
     when STORE_FLAG_VALUE is the sign bit.  */
      if (mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)
      && mtcs_simplify_rtx_val_signbit_p (self,int_mode, mtcs_real_get_store_flag_value(mtcsReal))
      && trueop1 == const_true_rtx
      && COMPARISON_P (op0)
      && (reversed = reversed_comparison (op0, int_mode)))
          return reversed;

      /* Convert (xor (and A C) (and B C)) into (and (xor A B) C).  */
      if (GET_CODE (op0) == GET_CODE (op1)
          && (GET_CODE (op0) == AND
          || GET_CODE (op0) == LSHIFTRT
          || GET_CODE (op0) == ASHIFTRT
          || GET_CODE (op0) == ASHIFT
          || GET_CODE (op0) == ROTATE
          || GET_CODE (op0) == ROTATERT)){
          tem = mtcs_simplify_rtx_distributive_operation(self,code, mode, op0, op1);
          if (tem)
            return tem;
      }

      tem = mtcs_simplify_rtx_byte_swapping_operation(self,code, mode, op0, op1);
      if (tem)
          return tem;

      tem = mtcs_simplify_rtx_associative_operation (self,code, mode, op0, op1);
      if (tem)
          return tem;
      break;

    case AND:
       n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_binary_operation_1 AND 00\n");
      if (trueop1 == CONST0_RTX (mode) && ! side_effects_p (op0))
          return trueop1;
      if (mtcs_mode_is_integral_p(mtcsMode,mode) && trueop1 == CONSTM1_RTX (mode))
          return op0;
      if (mtcs_mode_is_hwi_computable_p(mtcsMode,mode)){
          /* When WORD_REGISTER_OPERATIONS is true, we need to know the
             nonzero bits in WORD_MODE rather than MODE.  */
          scalar_int_mode tmode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,mode);
          if (mtcs_reg_get_word_register_operations/*!WORD_REGISTER_OPERATIONS*/(mtcsReg)
                && mtcs_mode_get_bitsize(mtcsMode,tmode) < BITS_PER_WORD)
            tmode = word_mode;
          n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_binary_operation_1 AND 00AA %d %d\n",tmode,word_mode);

          HOST_WIDE_INT nzop0 = mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(mtcsRtlanal,trueop0, tmode);
          HOST_WIDE_INT nzop1;
          if (CONST_INT_P (trueop1)){
              HOST_WIDE_INT val1 = INTVAL (trueop1);
              /* If we are turning off bits already known off in OP0, we need
             not do an AND.  */
              if ((nzop0 & ~val1) == 0)
                  return op0;
          }
          nzop1 = mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(mtcsRtlanal,trueop1, mode);
          /* If we are clearing all the nonzero bits, the result is zero.  */
          if ((nzop1 & nzop0) == 0 && !side_effects_p (op0) && !side_effects_p (op1))
            return CONST0_RTX (mode);
      }
      n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_binary_operation_1 AND 00aa\n");

      if (rtx_equal_p (trueop0, trueop1) && ! side_effects_p (op0)  && mtcs_mode_get_class(mtcsMode,mode) != MODE_CC)
          return op0;
      /* A & (~A) -> 0 */
      if (((GET_CODE (op0) == NOT && rtx_equal_p (XEXP (op0, 0), op1))
         || (GET_CODE (op1) == NOT && rtx_equal_p (XEXP (op1, 0), op0)))
         && ! side_effects_p (op0) && mtcs_mode_get_class(mtcsMode,mode) != MODE_CC)
          return CONST0_RTX (mode);
      n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_binary_operation_1 AND 00bb\n");

      /* Transform (and (extend X) C) into (zero_extend (and X C)) if
     there are no nonzero bits of C outside of X's mode.  */
      if ((GET_CODE (op0) == SIGN_EXTEND
        || GET_CODE (op0) == ZERO_EXTEND)
        && CONST_SCALAR_INT_P (trueop1)
        && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)
        && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (XEXP (op0, 0)), &inner_mode)
        && (wi::mask (mtcs_mode_get_precision(mtcsMode,inner_mode),
              true,mtcs_mode_get_precision(mtcsMode,int_mode)) & mtcs_rtx_mode_t/*!rtx_mode_t*/(trueop1, mode)) == 0){

          machine_mode imode = GET_MODE (XEXP (op0, 0));
          n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_binary_operation_1 AND 11 mode:%d imode:%d\n",mode,imode);
          tem = mtcs_rtl_immed_wide_int_const (mtcsRTL,mtcs_rtx_mode_t/*!rtx_mode_t*/(trueop1, mode), imode);
          mtcs_print_rtl(stderr,tem);
          tem = mtcs_simplify_rtx_gen_binary (self,AND, imode, XEXP (op0, 0), tem);
          mtcs_print_rtl(stderr,tem);
          tem= mtcs_simplify_rtx_gen_unary(self,ZERO_EXTEND, mode, tem, imode);
          n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_binary_operation_1 AND 11aa mode:%d imode:%d\n",mode,imode);

          mtcs_print_rtl(stderr,tem);

          return tem;
     }
      n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_binary_operation_1 AND 22\n");

      /* Transform (and (truncate X) C) into (truncate (and X C)).  This way
     we might be able to further simplify the AND with X and potentially
     remove the truncation altogether.  */
      if (GET_CODE (op0) == TRUNCATE && CONST_INT_P (trueop1)){
          rtx x = XEXP (op0, 0);
          machine_mode xmode = GET_MODE (x);
          tem = mtcs_simplify_rtx_gen_binary (self,AND, xmode, x,
                         mtcs_rtl_gen_int_mode(mtcsRTL,INTVAL (trueop1), xmode));
          return mtcs_simplify_rtx_gen_unary(self,TRUNCATE, mode, tem, xmode);
      }

      /* Canonicalize (A | C1) & C2 as (A & C2) | (C1 & C2).  */
      if (GET_CODE (op0) == IOR   && CONST_INT_P (trueop1)  && CONST_INT_P (XEXP (op0, 1))){
          HOST_WIDE_INT tmp = INTVAL (trueop1) & INTVAL (XEXP (op0, 1));
          return mtcs_simplify_rtx_gen_binary (self,IOR, mode,
                          mtcs_simplify_rtx_gen_binary (self,AND, mode,XEXP (op0, 0), op1),
                          mtcs_rtl_gen_int_mode(mtcsRTL,tmp, mode));
      }
      n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_binary_operation_1 AND 33\n");

      /* Convert (A ^ B) & A to A & (~B) since the latter is often a single
     insn (and may simplify more).  */
      if (GET_CODE (op0) == XOR  && rtx_equal_p (XEXP (op0, 0), op1)  && ! side_effects_p (op1))
        return mtcs_simplify_rtx_gen_binary (self,AND, mode,
                        mtcs_simplify_rtx_gen_unary(self,NOT, mode,XEXP (op0, 1), mode),op1);

      if (GET_CODE (op0) == XOR  && rtx_equal_p (XEXP (op0, 1), op1)  && ! side_effects_p (op1))
        return mtcs_simplify_rtx_gen_binary (self,AND, mode,
                        mtcs_simplify_rtx_gen_unary(self,NOT, mode,XEXP (op0, 0), mode),op1);

      /* Similarly for (~(A ^ B)) & A.  */
      if (GET_CODE (op0) == NOT   && GET_CODE (XEXP (op0, 0)) == XOR
        && rtx_equal_p (XEXP (XEXP (op0, 0), 0), op1) && ! side_effects_p (op1))
          return mtcs_simplify_rtx_gen_binary (self,AND, mode, XEXP (XEXP (op0, 0), 1), op1);

      if (GET_CODE (op0) == NOT   && GET_CODE (XEXP (op0, 0)) == XOR
      && rtx_equal_p (XEXP (XEXP (op0, 0), 1), op1) && ! side_effects_p (op1))
          return mtcs_simplify_rtx_gen_binary (self,AND, mode, XEXP (XEXP (op0, 0), 0), op1);

      /* Convert (A | B) & A to A.  */
      if (GET_CODE (op0) == IOR && (rtx_equal_p (XEXP (op0, 0), op1) || rtx_equal_p (XEXP (op0, 1), op1))
              && ! side_effects_p (XEXP (op0, 0)) && ! side_effects_p (XEXP (op0, 1)))
          return op1;

      /* For constants M and N, if M == (1LL << cst) - 1 && (N & M) == M,
     ((A & N) + B) & M -> (A + B) & M
     Similarly if (N & M) == 0,
     ((A | N) + B) & M -> (A + B) & M
     and for - instead of + and/or ^ instead of |.
         Also, if (N & M) == 0, then
     (A +- N) & M -> A & M.  */
      if (CONST_INT_P (trueop1)  && mtcs_mode_is_hwi_computable_p(mtcsMode,mode) && ~UINTVAL (trueop1)
         && (UINTVAL (trueop1) & (UINTVAL (trueop1) + 1)) == 0 && (GET_CODE (op0) == PLUS || GET_CODE (op0) == MINUS)){
          rtx pmop[2];
          int which;

          pmop[0] = XEXP (op0, 0);
          pmop[1] = XEXP (op0, 1);

          if (CONST_INT_P (pmop[1]) && (UINTVAL (pmop[1]) & UINTVAL (trueop1)) == 0)
            return mtcs_simplify_rtx_gen_binary (self,AND, mode, pmop[0], op1);

          for (which = 0; which < 2; which++){
              tem = pmop[which];
              switch (GET_CODE (tem)){
                case AND:
                  if (CONST_INT_P (XEXP (tem, 1)) && (UINTVAL (XEXP (tem, 1)) & UINTVAL (trueop1)) == UINTVAL (trueop1))
                    pmop[which] = XEXP (tem, 0);
                  break;
                case IOR:
                case XOR:
                  if (CONST_INT_P (XEXP (tem, 1)) && (UINTVAL (XEXP (tem, 1)) & UINTVAL (trueop1)) == 0)
                    pmop[which] = XEXP (tem, 0);
                  break;
                default:
                  break;
              }
          }

          if (pmop[0] != XEXP (op0, 0) || pmop[1] != XEXP (op0, 1)) {
              tem = mtcs_simplify_rtx_gen_binary (self,GET_CODE (op0), mode,pmop[0], pmop[1]);
              return mtcs_simplify_rtx_gen_binary (self,code, mode, tem, op1);
          }
      }
      n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_binary_operation_1 AND 44\n");

      /* (and X (ior (not X) Y) -> (and X Y) */
      if (GET_CODE (op1) == IOR && GET_CODE (XEXP (op1, 0)) == NOT && rtx_equal_p (op0, XEXP (XEXP (op1, 0), 0)))
          return mtcs_simplify_rtx_gen_binary (self,AND, mode, op0, XEXP (op1, 1));

      /* (and (ior (not X) Y) X) -> (and X Y) */
      if (GET_CODE (op0) == IOR && GET_CODE (XEXP (op0, 0)) == NOT  && rtx_equal_p (op1, XEXP (XEXP (op0, 0), 0)))
          return mtcs_simplify_rtx_gen_binary (self,AND, mode, op1, XEXP (op0, 1));

      /* (and X (ior Y (not X)) -> (and X Y) */
      if (GET_CODE (op1) == IOR  && GET_CODE (XEXP (op1, 1)) == NOT && rtx_equal_p (op0, XEXP (XEXP (op1, 1), 0)))
          return mtcs_simplify_rtx_gen_binary (self,AND, mode, op0, XEXP (op1, 0));

      /* (and (ior Y (not X)) X) -> (and X Y) */
      if (GET_CODE (op0) == IOR && GET_CODE (XEXP (op0, 1)) == NOT && rtx_equal_p (op1, XEXP (XEXP (op0, 1), 0)))
          return mtcs_simplify_rtx_gen_binary (self,AND, mode, op1, XEXP (op0, 0));

      /* Convert (and (ior A C) (ior B C)) into (ior (and A B) C).  */
      if (GET_CODE (op0) == GET_CODE (op1)
          && (GET_CODE (op0) == AND
          || GET_CODE (op0) == IOR
          || GET_CODE (op0) == LSHIFTRT
          || GET_CODE (op0) == ASHIFTRT
          || GET_CODE (op0) == ASHIFT
          || GET_CODE (op0) == ROTATE
          || GET_CODE (op0) == ROTATERT)){
          tem = mtcs_simplify_rtx_distributive_operation(self,code, mode, op0, op1);
          if (tem)
            return tem;
      }
      n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_binary_operation_1 AND 55\n");

      tem = mtcs_simplify_rtx_byte_swapping_operation(self,code, mode, op0, op1);
      if (tem)
          return tem;
      n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_binary_operation_1 AND 66\n");

      tem = mtcs_simplify_rtx_associative_operation (self,code, mode, op0, op1);
      if (tem)
          return tem;
      break;

    case UDIV:
      /* 0/x is 0 (or x&0 if x has side-effects).  */
      if (trueop0 == CONST0_RTX (mode) && !cfun->can_throw_non_call_exceptions){
          if (side_effects_p (op1))
            return mtcs_simplify_rtx_gen_binary (self,AND, mode, op1, trueop0);
          return trueop0;
      }
      /* x/1 is x.  */
      if (trueop1 == CONST1_RTX (mode)){
          tem = rtl_hooks.gen_lowpart_no_emit (mode, op0);
          if (tem)
            return tem;
      }
      /* Convert divide by power of two into shift.  */
      if (CONST_INT_P (trueop1)  && (val = exact_log2 (UINTVAL (trueop1))) > 0)
          return mtcs_simplify_rtx_gen_binary (self,LSHIFTRT, mode, op0,mtcs_rtl_gen_int_shift_amount (mtcsRTL,mode, val));
      break;

    case DIV:
      /* Handle floating point and integers separately.  */
      if (mtcs_mode_is_scalar_float_p(mtcsMode,mode)){
          /* Maybe change 0.0 / x to 0.0.  This transformation isn't
             safe for modes with NaNs, since 0.0 / 0.0 will then be
             NaN rather than 0.0.  Nor is it safe for modes with signed
             zeros, since dividing 0 by a negative number gives -0.0  */
          if (trueop0 == CONST0_RTX (mode)  && !mtcs_mode_honor_nans(mtcsMode,mode)
              && !mtcs_mode_honor_signed_zeros(mtcsMode,mode)  && ! side_effects_p (op1))
            return op0;
          /* x/1.0 is x.  */
          if (trueop1 == CONST1_RTX (mode)
              && !mtcs_mode_honor_snans(mtcsMode,mode))
            return op0;

          if (CONST_DOUBLE_AS_FLOAT_P (trueop1) && trueop1 != CONST0_RTX (mode)){
              const REAL_VALUE_TYPE *d1 = CONST_DOUBLE_REAL_VALUE (trueop1);

              /* x/-1.0 is -x.  */
              if (real_equal (d1, &dconstm1)  && !mtcs_mode_honor_snans(mtcsMode,mode))
                  return mtcs_simplify_rtx_gen_unary(self,NEG, mode, op0, mode);

              /* Change FP division by a constant into multiplication.
             Only do this with -freciprocal-math.  */
              if (flag_reciprocal_math && !real_equal (d1, &dconst0)){
                  REAL_VALUE_TYPE d;
                  real_arithmetic (&d, RDIV_EXPR, &dconst1, d1);
                  tem = mtcs_rtl_const_double_from_real_value/*!const_double_from_real_value*/(mtcsRTL,d, mode);
                  return mtcs_simplify_rtx_gen_binary (self,MULT, mode, op0, tem);
              }
          }
      }else if (mtcs_mode_is_scalar_int_p(mtcsMode,mode) || mtcs_mode_get_class(mtcsMode,mode) == MODE_VECTOR_INT){
          /* 0/x is 0 (or x&0 if x has side-effects).  */
          if (trueop0 == CONST0_RTX (mode) && !cfun->can_throw_non_call_exceptions){
              if (side_effects_p (op1))
                  return mtcs_simplify_rtx_gen_binary (self,AND, mode, op1, trueop0);
              return trueop0;
          }
          /* x/1 is x.  */
          if (trueop1 == CONST1_RTX (mode)){
              tem = rtl_hooks.gen_lowpart_no_emit (mode, op0);
              if (tem)
                  return tem;
          }
          /* x/-1 is -x.  */
          if (trueop1 == CONSTM1_RTX (mode)){
              rtx x = rtl_hooks.gen_lowpart_no_emit (mode, op0);
              if (x)
                  return mtcs_simplify_rtx_gen_unary(self,NEG, mode, x, mode);
          }
      }
      break;

    case UMOD:
      /* 0%x is 0 (or x&0 if x has side-effects).  */
      if (trueop0 == CONST0_RTX (mode)){
          if (side_effects_p (op1))
            return mtcs_simplify_rtx_gen_binary (self,AND, mode, op1, trueop0);
          return trueop0;
      }
      /* x%1 is 0 (of x&0 if x has side-effects).  */
      if (trueop1 == CONST1_RTX (mode)){
          if (side_effects_p (op0))
            return mtcs_simplify_rtx_gen_binary (self,AND, mode, op0, CONST0_RTX (mode));
          return CONST0_RTX (mode);
      }
      /* Implement modulus by power of two as AND.  */
      if (CONST_INT_P (trueop1)  && exact_log2 (UINTVAL (trueop1)) > 0)
        return mtcs_simplify_rtx_gen_binary (self,AND, mode, op0,
                        mtcs_rtl_gen_int_mode(mtcsRTL,UINTVAL (trueop1) - 1,mode));
      break;

    case MOD:
      /* 0%x is 0 (or x&0 if x has side-effects).  */
      if (trueop0 == CONST0_RTX (mode)){
          if (side_effects_p (op1))
            return mtcs_simplify_rtx_gen_binary (self,AND, mode, op1, trueop0);
          return trueop0;
      }
      /* x%1 and x%-1 is 0 (or x&0 if x has side-effects).  */
      if (trueop1 == CONST1_RTX (mode) || trueop1 == constm1_rtx){
          if (side_effects_p (op0))
            return mtcs_simplify_rtx_gen_binary (self,AND, mode, op0, CONST0_RTX (mode));
          return CONST0_RTX (mode);
      }
      break;

    case ROTATERT:
    case ROTATE:
      if (trueop1 == CONST0_RTX (mode))
          return op0;
      /* Canonicalize rotates by constant amount.  If the condition of
     reversing direction is met, then reverse the direction. */
#if defined(HAVE_rotate) && defined(HAVE_rotatert)
      if (reverse_rotate_by_imm_p (mode, (code == ROTATE), trueop1))
    {
      int new_amount = mtcs_mode_get_unit_precision(mtcsMode,mode) - INTVAL (trueop1);
      rtx new_amount_rtx = mtcs_rtl_gen_int_shift_amount(mtcsRTL,mode, new_amount);
      return mtcs_simplify_rtx_gen_binary (self,code == ROTATE ? ROTATERT : ROTATE,
                      mode, op0, new_amount_rtx);
    }
#endif
      /* FALLTHRU */
    case ASHIFTRT:
      if (trueop1 == CONST0_RTX (mode))
          return op0;
      if (trueop0 == CONST0_RTX (mode) && ! side_effects_p (op1))
          return op0;
      /* Rotating ~0 always results in ~0.  */
      if (CONST_INT_P (trueop0)  && mtcs_mode_is_hwi_computable_p(mtcsMode,mode)
         && UINTVAL (trueop0) == mtcs_mode_get_mask(mtcsMode,mode)  && ! side_effects_p (op1))
          return op0;

    canonicalize_shift:
      /* Given:
     scalar modes M1, M2
     scalar constants c1, c2
     size (M2) > size (M1)
     c1 == size (M2) - size (M1)
     optimize:
     ([a|l]shiftrt:M1 (subreg:M1 (lshiftrt:M2 (reg:M2) (const_int <c1>))
                 <low_part>)
              (const_int <c2>))
     to:
     (subreg:M1 ([a|l]shiftrt:M2 (reg:M2) (const_int <c1 + c2>))
            <low_part>).  */
      if ((code == ASHIFTRT || code == LSHIFTRT)  && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)
          && SUBREG_P (op0)  && CONST_INT_P (op1) && GET_CODE (SUBREG_REG (op0)) == LSHIFTRT
          && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (SUBREG_REG (op0)), &inner_mode)
          && CONST_INT_P (XEXP (SUBREG_REG (op0), 1))
          && mtcs_mode_get_bitsize(mtcsMode,inner_mode) > mtcs_mode_get_bitsize(mtcsMode,int_mode)
          && (INTVAL (XEXP (SUBREG_REG (op0), 1)) == mtcs_mode_get_bitsize(mtcsMode,inner_mode) - mtcs_mode_get_bitsize(mtcsMode,int_mode))
          && mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,op0)){
          rtx tmp = mtcs_rtl_gen_int_shift_amount(mtcsRTL,inner_mode, INTVAL (XEXP (SUBREG_REG (op0), 1)) + INTVAL (op1));

         /* Combine would usually zero out the value when combining two
            local shifts and the range becomes larger or equal to the mode.
            However since we fold away one of the shifts here combine won't
            see it so we should immediately zero the result if it's out of
            range.  */
         if (code == LSHIFTRT && INTVAL (tmp) >= mtcs_mode_get_bitsize(mtcsMode,inner_mode))
            tmp = const0_rtx;
         else
             tmp = mtcs_simplify_rtx_gen_binary (self,code,inner_mode, XEXP (SUBREG_REG (op0), 0),tmp);

          return mtcs_simplify_rtx_lowpart_subreg(self,int_mode, tmp, inner_mode);
      }

      if (SHIFT_COUNT_TRUNCATED && CONST_INT_P (op1)){
          val = INTVAL (op1) & (mtcs_mode_get_unit_precision(mtcsMode,mode) - 1);
          if (val != INTVAL (op1))
            return mtcs_simplify_rtx_gen_binary (self,code, mode, op0,mtcs_rtl_gen_int_shift_amount(mtcsRTL,mode, val));
      }
      break;

    case SS_ASHIFT:
      if (CONST_INT_P (trueop0)  && mtcs_mode_is_hwi_computable_p(mtcsMode,mode)
        && (UINTVAL (trueop0) == (mtcs_mode_get_mask(mtcsMode,mode) >> 1) || mtcs_simplify_rtx_mode_signbit_p(self,mode, trueop0))
        && ! side_effects_p (op1))
          return op0;
      goto simplify_ashift;

    case US_ASHIFT:
      if (CONST_INT_P (trueop0)  && mtcs_mode_is_hwi_computable_p(mtcsMode,mode)
        && UINTVAL (trueop0) == mtcs_mode_get_mask(mtcsMode,mode) && ! side_effects_p (op1))
          return op0;
      /* FALLTHRU */

    case ASHIFT:
simplify_ashift:
      if (trueop1 == CONST0_RTX (mode))
          return op0;
      if (trueop0 == CONST0_RTX (mode) && ! side_effects_p (op1))
          return op0;
      if (self->mem_depth  && code == ASHIFT  && CONST_INT_P (trueop1)
        && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)
        && IN_RANGE (UINTVAL (trueop1), 1, mtcs_mode_get_precision(mtcsMode,int_mode) - 1)){
          auto c = (wi::one (mtcs_mode_get_precision(mtcsMode,int_mode)) << UINTVAL (trueop1));
          rtx new_op1 = mtcs_rtl_immed_wide_int_const (mtcsRTL,c, int_mode);
          return mtcs_simplify_rtx_gen_binary (self,MULT, int_mode, op0, new_op1);
      }
      goto canonicalize_shift;

    case LSHIFTRT:
      if (trueop1 == CONST0_RTX (mode))
          return op0;
      if (trueop0 == CONST0_RTX (mode) && ! side_effects_p (op1))
          return op0;
      /* Optimize (lshiftrt (clz X) C) as (eq X 0).  */
      if (GET_CODE (op0) == CLZ  && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (XEXP (op0, 0)), &inner_mode)
         && CONST_INT_P (trueop1) && mtcs_real_get_store_flag_value(mtcsReal) == 1
         && INTVAL (trueop1) < mtcs_mode_get_unit_precision(mtcsMode,mode)){
          unsigned HOST_WIDE_INT zero_val = 0;
          if (mtcs_mode_clz_defined_value_at_zero/*!CLZ_DEFINED_VALUE_AT_ZERO*/ (mtcsMode,inner_mode,(int *)&zero_val)
                  && zero_val == mtcs_mode_get_precision(mtcsMode,inner_mode)
              && INTVAL (trueop1) == exact_log2 (zero_val))
            return mtcs_simplify_rtx_gen_relational (self,EQ, mode, inner_mode,XEXP (op0, 0), const0_rtx);
       }
      goto canonicalize_shift;

    case SMIN:
      if (mtcs_mode_is_hwi_computable_p(mtcsMode,mode) && mtcs_simplify_rtx_mode_signbit_p(self,mode, trueop1)
        && ! side_effects_p (op0))
          return op1;
      if (rtx_equal_p (trueop0, trueop1) && ! side_effects_p (op0))
          return op0;
      tem = mtcs_simplify_rtx_associative_operation (self,code, mode, op0, op1);
      if (tem)
          return tem;
      break;

    case SMAX:
      if (mtcs_mode_is_hwi_computable_p(mtcsMode,mode) && CONST_INT_P (trueop1)
        && (UINTVAL (trueop1) == mtcs_mode_get_mask(mtcsMode,mode) >> 1)  && ! side_effects_p (op0))
          return op1;
      if (rtx_equal_p (trueop0, trueop1) && ! side_effects_p (op0))
          return op0;
      tem = mtcs_simplify_rtx_associative_operation (self,code, mode, op0, op1);
      if (tem)
          return tem;
      break;

    case UMIN:
      if (trueop1 == CONST0_RTX (mode) && ! side_effects_p (op0))
          return op1;
      if (rtx_equal_p (trueop0, trueop1) && ! side_effects_p (op0))
          return op0;
      tem = mtcs_simplify_rtx_associative_operation (self,code, mode, op0, op1);
      if (tem)
          return tem;
      break;

    case UMAX:
      if (trueop1 == constm1_rtx && ! side_effects_p (op0))
          return op1;
      if (rtx_equal_p (trueop0, trueop1) && ! side_effects_p (op0))
          return op0;
      tem = mtcs_simplify_rtx_associative_operation (self,code, mode, op0, op1);
      if (tem)
          return tem;
      break;

    case SS_PLUS:
    case US_PLUS:
    case SS_MINUS:
    case US_MINUS:
      /* Simplify x +/- 0 to x, if possible.  */
      if (trueop1 == CONST0_RTX (mode))
          return op0;
      return 0;

    case SS_MULT:
    case US_MULT:
      /* Simplify x * 0 to 0, if possible.  */
      if (trueop1 == CONST0_RTX (mode) && !side_effects_p (op0))
          return op1;
      /* Simplify x * 1 to x, if possible.  */
      if (trueop1 == CONST1_RTX (mode))
          return op0;
      return 0;
    case SMUL_HIGHPART:
    case UMUL_HIGHPART:
      /* Simplify x * 0 to 0, if possible.  */
      if (trueop1 == CONST0_RTX (mode) && !side_effects_p (op0))
          return op1;
      return 0;
    case SS_DIV:
    case US_DIV:
      /* Simplify x / 1 to x, if possible.  */
      if (trueop1 == CONST1_RTX (mode))
          return op0;
      return 0;
    case COPYSIGN:
      if (rtx_equal_p (trueop0, trueop1) && ! side_effects_p (op0))
          return op0;
      if (CONST_DOUBLE_AS_FLOAT_P (trueop1)){
          REAL_VALUE_TYPE f1;
          mtcs_real_real_convert/*!real_convert*/(mtcsReal,&f1, mode, CONST_DOUBLE_REAL_VALUE (trueop1));
          rtx tmp = mtcs_simplify_rtx_gen_unary(self,ABS, mode, op0, mode);
          if (REAL_VALUE_NEGATIVE (f1))
            tmp = mtcs_simplify_rtx_unary_operation (self,NEG, mode, tmp, mode);
          return tmp;
      }
      if (GET_CODE (op0) == NEG || GET_CODE (op0) == ABS)
          return mtcs_simplify_rtx_gen_binary (self,COPYSIGN, mode, XEXP (op0, 0), op1);
      if (GET_CODE (op1) == ABS && ! side_effects_p (op1))
          return mtcs_simplify_rtx_gen_unary(self,ABS, mode, op0, mode);
      if (GET_CODE (op0) == COPYSIGN  && ! side_effects_p (XEXP (op0, 1)))
          return mtcs_simplify_rtx_gen_binary (self,COPYSIGN, mode, XEXP (op0, 0), op1);
      if (GET_CODE (op1) == COPYSIGN  && ! side_effects_p (XEXP (op1, 0)))
          return mtcs_simplify_rtx_gen_binary (self,COPYSIGN, mode, op0, XEXP (op1, 1));
      return 0;

    case VEC_SERIES:
      if (op1 == CONST0_RTX (mtcs_mode_get_inner (mtcsMode,mode)))
          return gen_vec_duplicate (mode, op0);
      if (valid_for_const_vector_p (mode, op0) && valid_for_const_vector_p (mode, op1))
          return gen_const_vec_series (mode, op0, op1);
      return 0;

    case VEC_SELECT:
      if (!mtcs_mode_is_vector_p(mtcsMode,mode)){
          gcc_assert (mtcs_mode_is_vector_p(mtcsMode,GET_MODE (trueop0)));
          gcc_assert (mode == mtcs_mode_get_inner (mtcsMode,GET_MODE (trueop0)));
          gcc_assert (GET_CODE (trueop1) == PARALLEL);
          gcc_assert (XVECLEN (trueop1, 0) == 1);

          /* We can't reason about selections made at runtime.  */
          if (!CONST_INT_P (XVECEXP (trueop1, 0, 0)))
            return 0;

          if (vec_duplicate_p (trueop0, &elt0))
            return elt0;

          if (GET_CODE (trueop0) == CONST_VECTOR)
            return mtcs_rtl_const_vector_elt/*!CONST_VECTOR_ELT*/ (mtcsRTL,trueop0, INTVAL (XVECEXP(trueop1, 0, 0)));

          /* Extract a scalar element from a nested VEC_SELECT expression
             (with optional nested VEC_CONCAT expression).  Some targets
             (i386) extract scalar element from a vector using chain of
             nested VEC_SELECT expressions.  When input operand is a memory
             operand, this operation can be simplified to a simple scalar
             load from an offseted memory address.  */
          int n_elts;
          if (GET_CODE (trueop0) == VEC_SELECT
                  && (mtcs_mode_get_nunits(mtcsMode,GET_MODE (XEXP (trueop0, 0))).is_constant (&n_elts))){
              rtx op0 = XEXP (trueop0, 0);
              rtx op1 = XEXP (trueop0, 1);

              int i = INTVAL (XVECEXP (trueop1, 0, 0));
              int elem;

              rtvec vec;
              rtx tmp_op, tmp;

              gcc_assert (GET_CODE (op1) == PARALLEL);
              gcc_assert (i < n_elts);

              /* Select element, pointed by nested selector.  */
              elem = INTVAL (XVECEXP (op1, 0, i));

              /* Handle the case when nested VEC_SELECT wraps VEC_CONCAT.  */
              if (GET_CODE (op0) == VEC_CONCAT){
                  rtx op00 = XEXP (op0, 0);
                  rtx op01 = XEXP (op0, 1);
                  machine_mode mode00, mode01;
                  int n_elts00, n_elts01;
                  mode00 = GET_MODE (op00);
                  mode01 = GET_MODE (op01);
                  /* Find out the number of elements of each operand.
                     Since the concatenated result has a constant number
                     of elements, the operands must too.  */
                  n_elts00 = mtcs_mode_get_nunits(mtcsMode,mode00).to_constant ();
                  n_elts01 = mtcs_mode_get_nunits(mtcsMode,mode01).to_constant ();
                  gcc_assert (n_elts == n_elts00 + n_elts01);
                  /* Select correct operand of VEC_CONCAT
                     and adjust selector. */
                  if (elem < n_elts01)
                    tmp_op = op00;
                  else{
                      tmp_op = op01;
                      elem -= n_elts00;
                  }
              }else
                  tmp_op = op0;

              vec = rtvec_alloc (1);
              RTVEC_ELT (vec, 0) = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,elem);
              tmp = gen_rtx_fmt_ee (code, mode,tmp_op, gen_rtx_PARALLEL (VOIDmode, vec));
              return tmp;
          }
      }else{
          gcc_assert (mtcs_mode_is_vector_p(mtcsMode,GET_MODE (trueop0)));
          gcc_assert (mtcs_mode_get_inner (mtcsMode,mode)== mtcs_mode_get_inner (mtcsMode,GET_MODE (trueop0)));
          gcc_assert (GET_CODE (trueop1) == PARALLEL);

          if (vec_duplicate_p (trueop0, &elt0))
            /* It doesn't matter which elements are selected by trueop1,
               because they are all the same.  */
            return gen_vec_duplicate (mode, elt0);

          if (GET_CODE (trueop0) == CONST_VECTOR){
              unsigned n_elts = XVECLEN (trueop1, 0);
              rtvec v = rtvec_alloc (n_elts);
              unsigned int i;
              gcc_assert (known_eq (n_elts, mtcs_mode_get_nunits(mtcsMode,mode)));
              for (i = 0; i < n_elts; i++){
                  rtx x = XVECEXP (trueop1, 0, i);
                  if (!CONST_INT_P (x))
                    return 0;
                  RTVEC_ELT (v, i) = mtcs_rtl_const_vector_elt (mtcsRTL,trueop0,INTVAL (x));
              }
              return gen_rtx_CONST_VECTOR (mode, v);
          }
          /* Recognize the identity.  */
          if (GET_MODE (trueop0) == mode){
              bool maybe_ident = true;
              for (int i = 0; i < XVECLEN (trueop1, 0); i++){
                  rtx j = XVECEXP (trueop1, 0, i);
                  if (!CONST_INT_P (j) || INTVAL (j) != i){
                      maybe_ident = false;
                      break;
                  }
              }
              if (maybe_ident)
                  return trueop0;
          }

          /* If we select a low-part subreg, return that.  */
          if (mtcs_rtlanal_vec_series_lowpart_p (mtcsRtlanal,mode, GET_MODE (trueop0), trueop1)){
              rtx new_rtx = mtcs_simplify_rtx_lowpart_subreg(self,mode, trueop0,GET_MODE (trueop0));
              if (new_rtx != NULL_RTX)
                  return new_rtx;
          }

          /* If we build {a,b} then permute it, build the result directly.  */
          if (XVECLEN (trueop1, 0) == 2
              && CONST_INT_P (XVECEXP (trueop1, 0, 0))
              && CONST_INT_P (XVECEXP (trueop1, 0, 1))
              && GET_CODE (trueop0) == VEC_CONCAT
              && GET_CODE (XEXP (trueop0, 0)) == VEC_CONCAT
              && GET_MODE (XEXP (trueop0, 0)) == mode
              && GET_CODE (XEXP (trueop0, 1)) == VEC_CONCAT
              && GET_MODE (XEXP (trueop0, 1)) == mode){
                  unsigned int i0 = INTVAL (XVECEXP (trueop1, 0, 0));
                  unsigned int i1 = INTVAL (XVECEXP (trueop1, 0, 1));
                  rtx subop0, subop1;

                  gcc_assert (i0 < 4 && i1 < 4);
                  subop0 = XEXP (XEXP (trueop0, i0 / 2), i0 % 2);
                  subop1 = XEXP (XEXP (trueop0, i1 / 2), i1 % 2);

                  return mtcs_simplify_rtx_gen_binary (self,VEC_CONCAT, mode, subop0, subop1);
          }

          if (XVECLEN (trueop1, 0) == 2
              && CONST_INT_P (XVECEXP (trueop1, 0, 0))
              && CONST_INT_P (XVECEXP (trueop1, 0, 1))
              && GET_CODE (trueop0) == VEC_CONCAT
              && GET_MODE (trueop0) == mode){
              unsigned int i0 = INTVAL (XVECEXP (trueop1, 0, 0));
              unsigned int i1 = INTVAL (XVECEXP (trueop1, 0, 1));
              rtx subop0, subop1;

              gcc_assert (i0 < 2 && i1 < 2);
              subop0 = XEXP (trueop0, i0);
              subop1 = XEXP (trueop0, i1);

              return mtcs_simplify_rtx_gen_binary (self,VEC_CONCAT, mode, subop0, subop1);
          }

          /* If we select one half of a vec_concat, return that.  */
          int l0, l1;
          if (GET_CODE (trueop0) == VEC_CONCAT
              && (mtcs_mode_get_nunits(mtcsMode,GET_MODE (XEXP (trueop0, 0))).is_constant (&l0))
              && (mtcs_mode_get_nunits(mtcsMode,GET_MODE (XEXP (trueop0, 1))).is_constant (&l1))
              && CONST_INT_P (XVECEXP (trueop1, 0, 0))){
              rtx subop0 = XEXP (trueop0, 0);
              rtx subop1 = XEXP (trueop0, 1);
              machine_mode mode0 = GET_MODE (subop0);
              machine_mode mode1 = GET_MODE (subop1);
              int i0 = INTVAL (XVECEXP (trueop1, 0, 0));
              if (i0 == 0 && !side_effects_p (op1) && mode == mode0){
                  bool success = true;
                  for (int i = 1; i < l0; ++i){
                      rtx j = XVECEXP (trueop1, 0, i);
                      if (!CONST_INT_P (j) || INTVAL (j) != i){
                          success = false;
                          break;
                      }
                  }
                  if (success)
                    return subop0;
              }
              if (i0 == l0 && !side_effects_p (op0) && mode == mode1){
                  bool success = true;
                  for (int i = 1; i < l1; ++i){
                      rtx j = XVECEXP (trueop1, 0, i);
                      if (!CONST_INT_P (j) || INTVAL (j) != i0 + i){
                          success = false;
                          break;
                      }
                  }
                  if (success)
                    return subop1;
              }
          }

          /* Simplify vec_select of a subreg of X to just a vec_select of X
             when X has same component mode as vec_select.  */
          unsigned HOST_WIDE_INT subreg_offset = 0;
          if (GET_CODE (trueop0) == SUBREG
              && mtcs_mode_get_inner (mtcsMode,mode) == mtcs_mode_get_inner (mtcsMode,GET_MODE (SUBREG_REG (trueop0)))
              && mtcs_mode_get_nunits(mtcsMode,mode).is_constant (&l1)
              && constant_multiple_p (subreg_memory_offset (trueop0), mtcs_mode_get_unit_bitsize(mtcsMode,mode),&subreg_offset)){
              poly_uint64 nunits = mtcs_mode_get_nunits(mtcsMode,GET_MODE (SUBREG_REG (trueop0)));
              bool success = true;
              for (int i = 0; i != l1; i++){
                  rtx idx = XVECEXP (trueop1, 0, i);
                  if (!CONST_INT_P (idx) || maybe_ge (UINTVAL (idx) + subreg_offset, nunits)){
                      success = false;
                      break;
                  }
              }

              if (success){
                  rtx par = trueop1;
                  if (subreg_offset) {
                      rtvec vec = rtvec_alloc (l1);
                      for (int i = 0; i < l1; i++)
                          RTVEC_ELT (vec, i) = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,INTVAL (XVECEXP (trueop1, 0, i)) + subreg_offset);
                      par = gen_rtx_PARALLEL (VOIDmode, vec);
                  }
                  return gen_rtx_VEC_SELECT (mode, SUBREG_REG (trueop0), par);
              }
          }
      }//end else

      if (XVECLEN (trueop1, 0) == 1  && CONST_INT_P (XVECEXP (trueop1, 0, 0))  && GET_CODE (trueop0) == VEC_CONCAT){
          rtx vec = trueop0;
          offset = INTVAL (XVECEXP (trueop1, 0, 0)) * mtcs_mode_get_size(mtcsMode,mode);

          /* Try to find the element in the VEC_CONCAT.  */
          while (GET_MODE (vec) != mode && GET_CODE (vec) == VEC_CONCAT) {
              poly_int64 vec_size;
              if (CONST_INT_P (XEXP (vec, 0))) {
                  /* vec_concat of two const_ints doesn't make sense with
                     respect to modes.  */
                  if (CONST_INT_P (XEXP (vec, 1)))
                    return 0;
                  vec_size = mtcs_mode_get_size(mtcsMode,GET_MODE (trueop0))- mtcs_mode_get_size(mtcsMode,GET_MODE (XEXP (vec, 1)));
              }else
                vec_size = mtcs_mode_get_size(mtcsMode,GET_MODE (XEXP (vec, 0)));

              if (known_lt (offset, vec_size))
                  vec = XEXP (vec, 0);
              else if (known_ge (offset, vec_size)){
                  offset -= vec_size;
                  vec = XEXP (vec, 1);
              }else
                  break;
              vec = mtcs_simplify_rtx_avoid_constant_pool_reference/*!avoid_constant_pool_reference*/(self,vec);
          }
          if (GET_MODE (vec) == mode)
            return vec;
      }

      /* If we select elements in a vec_merge that all come from the same
     operand, select from that operand directly.  */
      if (GET_CODE (op0) == VEC_MERGE){
          rtx trueop02 = mtcs_simplify_rtx_avoid_constant_pool_reference (self,XEXP (op0, 2));
          if (CONST_INT_P (trueop02)){
              unsigned HOST_WIDE_INT sel = UINTVAL (trueop02);
              bool all_operand0 = true;
              bool all_operand1 = true;
              for (int i = 0; i < XVECLEN (trueop1, 0); i++){
                  rtx j = XVECEXP (trueop1, 0, i);
                  if (sel & (HOST_WIDE_INT_1U << UINTVAL (j)))
                    all_operand1 = false;
                  else
                    all_operand0 = false;
              }
              if (all_operand0 && !side_effects_p (XEXP (op0, 1)))
                  return mtcs_simplify_rtx_gen_binary (self,VEC_SELECT, mode, XEXP (op0, 0), op1);
              if (all_operand1 && !side_effects_p (XEXP (op0, 0)))
                  return mtcs_simplify_rtx_gen_binary (self,VEC_SELECT, mode, XEXP (op0, 1), op1);
          }
      }

      /* If we have two nested selects that are inverses of each
     other, replace them with the source operand.  */
      if (GET_CODE (trueop0) == VEC_SELECT  && GET_MODE (XEXP (trueop0, 0)) == mode){
          rtx op0_subop1 = XEXP (trueop0, 1);
          gcc_assert (GET_CODE (op0_subop1) == PARALLEL);
          gcc_assert (known_eq (XVECLEN (trueop1, 0), mtcs_mode_get_nunits(mtcsMode,mode)));

          /* Apply the outer ordering vector to the inner one.  (The inner
             ordering vector is expressly permitted to be of a different
             length than the outer one.)  If the result is { 0, 1, ..., n-1 }
             then the two VEC_SELECTs cancel.  */
          for (int i = 0; i < XVECLEN (trueop1, 0); ++i){
              rtx x = XVECEXP (trueop1, 0, i);
              if (!CONST_INT_P (x))
                  return 0;
              rtx y = XVECEXP (op0_subop1, 0, INTVAL (x));
              if (!CONST_INT_P (y) || i != INTVAL (y))
                  return 0;
          }
          return XEXP (trueop0, 0);
      }

      return 0;
    case VEC_CONCAT:
      {
        machine_mode op0_mode = (GET_MODE (trueop0) != VOIDmode
                          ? GET_MODE (trueop0) : mtcs_mode_get_inner (mtcsMode,mode));
        machine_mode op1_mode = (GET_MODE (trueop1) != VOIDmode
                          ? GET_MODE (trueop1) : mtcs_mode_get_inner (mtcsMode,mode));

        gcc_assert (mtcs_mode_is_vector_p(mtcsMode,mode));
        gcc_assert (known_eq (mtcs_mode_get_size(mtcsMode,op0_mode)
                      + mtcs_mode_get_size(mtcsMode,op1_mode), mtcs_mode_get_size(mtcsMode,mode)));

        if (mtcs_mode_is_vector_p(mtcsMode,op0_mode))
          gcc_assert (mtcs_mode_get_inner (mtcsMode,mode) == mtcs_mode_get_inner (mtcsMode,op0_mode));
        else
          gcc_assert (mtcs_mode_get_inner (mtcsMode,mode) == op0_mode);

        if (mtcs_mode_is_vector_p(mtcsMode,op1_mode))
          gcc_assert (mtcs_mode_get_inner (mtcsMode,mode) == mtcs_mode_get_inner (mtcsMode,op1_mode));
        else
          gcc_assert (mtcs_mode_get_inner (mtcsMode,mode) == op1_mode);

        unsigned int n_elts, in_n_elts;
        if ((GET_CODE (trueop0) == CONST_VECTOR
             || CONST_SCALAR_INT_P (trueop0)
             || CONST_DOUBLE_AS_FLOAT_P (trueop0))
            && (GET_CODE (trueop1) == CONST_VECTOR
            || CONST_SCALAR_INT_P (trueop1)
            || CONST_DOUBLE_AS_FLOAT_P (trueop1))
            && mtcs_mode_get_nunits(mtcsMode,mode).is_constant (&n_elts)
            && mtcs_mode_get_nunits(mtcsMode,op0_mode).is_constant (&in_n_elts)){
            rtvec v = rtvec_alloc (n_elts);
            unsigned int i;
            for (i = 0; i < n_elts; i++){
                if (i < in_n_elts){
                    if (!mtcs_mode_is_vector_p(mtcsMode,op0_mode))
                      RTVEC_ELT (v, i) = trueop0;
                    else
                      RTVEC_ELT (v, i) = mtcs_rtl_const_vector_elt (mtcsRTL,trueop0, i);
                }else{
                    if (!mtcs_mode_is_vector_p(mtcsMode,op1_mode))
                      RTVEC_ELT (v, i) = trueop1;
                    else
                      RTVEC_ELT (v, i) = mtcs_rtl_const_vector_elt (mtcsRTL,trueop1,i - in_n_elts);
                }
            }
            return gen_rtx_CONST_VECTOR (mode, v);
          }

        /* Try to merge two VEC_SELECTs from the same vector into a single one.
           Restrict the transformation to avoid generating a VEC_SELECT with a
           mode unrelated to its operand.  */
        if (GET_CODE (trueop0) == VEC_SELECT
            && GET_CODE (trueop1) == VEC_SELECT
            && rtx_equal_p (XEXP (trueop0, 0), XEXP (trueop1, 0))
            && mtcs_mode_get_inner (mtcsMode,GET_MODE (XEXP (trueop0, 0))) == mtcs_mode_get_inner(mtcsMode,mode)){
            rtx par0 = XEXP (trueop0, 1);
            rtx par1 = XEXP (trueop1, 1);
            int len0 = XVECLEN (par0, 0);
            int len1 = XVECLEN (par1, 0);
            rtvec vec = rtvec_alloc (len0 + len1);
            for (int i = 0; i < len0; i++)
              RTVEC_ELT (vec, i) = XVECEXP (par0, 0, i);
            for (int i = 0; i < len1; i++)
              RTVEC_ELT (vec, len0 + i) = XVECEXP (par1, 0, i);
            return mtcs_simplify_rtx_gen_binary (self,VEC_SELECT, mode, XEXP (trueop0, 0),gen_rtx_PARALLEL (VOIDmode, vec));
        }
        /* (vec_concat:
             (subreg_lowpart:N OP)
             (vec_select:N OP P))  -->  OP when P selects the high half
            of the OP.  */
        if (GET_CODE (trueop0) == SUBREG
            && mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,trueop0)
            && GET_CODE (trueop1) == VEC_SELECT
            && SUBREG_REG (trueop0) == XEXP (trueop1, 0)
            && !side_effects_p (XEXP (trueop1, 0))
            && mtcs_rtlanal_vec_series_highpart_p (mtcsRtlanal,op1_mode, mode, XEXP (trueop1, 1)))
            return XEXP (trueop1, 0);
      }
      return 0;

    default:
      gcc_unreachable ();
  }

  if (mode == GET_MODE (op0) && mode == GET_MODE (op1)
          && vec_duplicate_p (op0, &elt0)  && vec_duplicate_p (op1, &elt1)) {
      /* Try applying the operator to ELT and see if that simplifies.
     We can duplicate the result if so.

     The reason we don't use simplify_gen_binary is that it isn't
     necessarily a win to convert things like:

       (plus:V (vec_duplicate:V (reg:S R1))
           (vec_duplicate:V (reg:S R2)))

     to:

       (vec_duplicate:V (plus:S (reg:S R1) (reg:S R2)))

     The first might be done entirely in vector registers while the
     second might need a move between register files.  */
      tem = mtcs_simplify_rtx_binary_operation (self,code, mtcs_mode_get_inner/*!GET_MODE_INNER*/ (mtcsMode,mode), elt0, elt1);
      if (tem)
          return gen_vec_duplicate (mode, tem);
  }

  return 0;
}

/* Generates a subreg to get the least significant part of EXPR (in mode
   INNER_MODE) to OUTER_MODE.  */
//原型 simplify_context::lowpart_subreg rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_lowpart_subreg (MtcsSimplifyRtx *self,machine_mode outer_mode, rtx expr,machine_mode inner_mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  return mtcs_simplify_rtx_gen_subreg (self,outer_mode, expr, inner_mode,
                  mtcs_mode_subreg_lowpart_offset (mtcsMode,outer_mode, inner_mode));
}

/* Likewise, for relational operations.
   CMP_MODE specifies mode comparison is done in.  */
//原型 rtx simplify_context::simplify_gen_relational rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_gen_relational (MtcsSimplifyRtx *self,rtx_code code, machine_mode mode, machine_mode cmp_mode,rtx op0, rtx op1)
{
  rtx tem;
  if ((tem = mtcs_simplify_rtx_relational_operation (self,code, mode, cmp_mode,op0, op1)) != 0)
    return tem;
  return gen_rtx_fmt_ee (code, mode, op0, op1);
}

/* Likewise for ternary operations.  */
//原型 rtx simplify_context::simplify_gen_ternary rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_gen_ternary (MtcsSimplifyRtx *self,rtx_code code, machine_mode mode,
                    machine_mode op0_mode, rtx op0, rtx op1, rtx op2)
{
  rtx tem;
  /* If this simplifies, use it.  */
  if ((tem = mtcs_simplify_rtx_ternary_operation (self,code, mode, op0_mode, op0, op1, op2)) != 0)
    return tem;
  return gen_rtx_fmt_eee (code, mode, op0, op1, op2);
}

/* Simplify CODE, an operation with result mode MODE and three operands,
   OP0, OP1, and OP2.  OP0_MODE was the mode of OP0 before it became
   a constant.  Return 0 if no simplifications is possible.  */
//原型 rtx simplify_context::simplify_ternary_operation rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_ternary_operation (MtcsSimplifyRtx *self,rtx_code code, machine_mode mode,
                          machine_mode op0_mode,rtx op0, rtx op1, rtx op2)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);

  bool any_change = false;
  rtx tem, trueop2;
  scalar_int_mode int_mode, int_op0_mode;
  unsigned int n_elts;

  switch (code){
    case FMA:
      /* Simplify negations around the multiplication.  */
      /* -a * -b + c  =>  a * b + c.  */
      if (GET_CODE (op0) == NEG){
          tem = mtcs_simplify_rtx_unary_operation (self,NEG, mode, op1, mode);
          if (tem)
            op1 = tem, op0 = XEXP (op0, 0), any_change = true;
      }else if (GET_CODE (op1) == NEG){
          tem = mtcs_simplify_rtx_unary_operation (self,NEG, mode, op0, mode);
          if (tem)
            op0 = tem, op1 = XEXP (op1, 0), any_change = true;
      }

      /* Canonicalize the two multiplication operands.  */
      /* a * -b + c  =>  -b * a + c.  */
      if (mtcs_rtlanal_swap_commutative_operands_p/*!swap_commutative_operands_p*/(mtcsRtlanal, op0, op1))
          std::swap (op0, op1), any_change = true;

      if (any_change)
          return gen_rtx_FMA (mode, op0, op1, op2);
      return NULL_RTX;

    case SIGN_EXTRACT:
    case ZERO_EXTRACT:
      if (CONST_INT_P (op0)
      && CONST_INT_P (op1)
      && CONST_INT_P (op2)
      && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)
      && INTVAL (op1) + INTVAL (op2) <= mtcs_mode_get_precision (mtcsMode,int_mode)
      && mtcs_mode_is_hwi_computable_p/*!HWI_COMPUTABLE_MODE_P*/(mtcsMode,int_mode)){
          /* Extracting a bit-field from a constant */
          unsigned HOST_WIDE_INT val = UINTVAL (op0);
          HOST_WIDE_INT op1val = INTVAL (op1);
          HOST_WIDE_INT op2val = INTVAL (op2);
          if (!BITS_BIG_ENDIAN)
            val >>= op2val;
          else if (mtcs_mode_is_a <scalar_int_mode> (mtcsMode,op0_mode, &int_op0_mode))
            val >>= mtcs_mode_get_precision (mtcsMode,int_op0_mode) - op2val - op1val;
          else
            /* Not enough information to calculate the bit position.  */
            break;

          if (HOST_BITS_PER_WIDE_INT != op1val){
              /* First zero-extend.  */
              val &= (HOST_WIDE_INT_1U << op1val) - 1;
              /* If desired, propagate sign bit.  */
              if (code == SIGN_EXTRACT && (val & (HOST_WIDE_INT_1U << (op1val - 1)))!= 0)
                  val |= ~ ((HOST_WIDE_INT_1U << op1val) - 1);
          }
          return mtcs_rtl_gen_int_mode(mtcsRTL,val, int_mode);
      }
      break;

    case IF_THEN_ELSE:
      if (CONST_INT_P (op0))
          return op0 != const0_rtx ? op1 : op2;

      /* Convert c ? a : a into "a".  */
      if (rtx_equal_p (op1, op2) && ! side_effects_p (op0))
          return op1;

      /* Convert a != b ? a : b into "a".  */
      if (GET_CODE (op0) == NE
      && ! side_effects_p (op0)
      && ! mtcs_mode_honor_nans(mtcsMode,mode)
      && ! mtcs_mode_honor_signed_zeros/*!HONOR_SIGNED_ZEROS*/ (mtcsMode,mode)
      && ((rtx_equal_p (XEXP (op0, 0), op1)
           && rtx_equal_p (XEXP (op0, 1), op2))
          || (rtx_equal_p (XEXP (op0, 0), op2)
          && rtx_equal_p (XEXP (op0, 1), op1))))
          return op1;

      /* Convert a == b ? a : b into "b".  */
      if (GET_CODE (op0) == EQ  && ! side_effects_p (op0) && ! mtcs_mode_honor_nans (mtcsMode,mode)
          && ! mtcs_mode_honor_signed_zeros/*HONOR_SIGNED_ZEROS*/ (mtcsMode,mode)
          && ((rtx_equal_p (XEXP (op0, 0), op1)  && rtx_equal_p (XEXP (op0, 1), op2))
              || (rtx_equal_p (XEXP (op0, 0), op2) && rtx_equal_p (XEXP (op0, 1), op1))))
          return op2;

      /* Convert (!c) != {0,...,0} ? a : b into
         c != {0,...,0} ? b : a for vector modes.  */
      if (mtcs_mode_is_vector_p(mtcsMode,GET_MODE (op1))  && GET_CODE (op0) == NE
          && GET_CODE (XEXP (op0, 0)) == NOT && GET_CODE (XEXP (op0, 1)) == CONST_VECTOR){
          rtx cv = XEXP (op0, 1);
          int nunits;
          bool ok = true;
          if (!CONST_VECTOR_NUNITS (cv).is_constant (&nunits))
            ok = false;
          else
            for (int i = 0; i < nunits; ++i)
              if (mtcs_rtl_const_vector_elt (mtcsRTL,cv, i) != const0_rtx){
                  ok = false;
                  break;
              }
          if (ok){
              rtx new_op0 = gen_rtx_NE (GET_MODE (op0),XEXP (XEXP (op0, 0), 0),XEXP (op0, 1));
              rtx retval = gen_rtx_IF_THEN_ELSE (mode, new_op0, op2, op1);
              return retval;
          }
      }

      /* Convert x == 0 ? N : clz (x) into clz (x) when
     CLZ_DEFINED_VALUE_AT_ZERO is defined to N for the mode of x.
     Similarly for ctz (x).  */
      if (COMPARISON_P (op0) && !side_effects_p (op0) && XEXP (op0, 1) == const0_rtx){
          rtx simplified= mtcs_simplify_rtx_cond_clz_ctz (self,XEXP (op0, 0), GET_CODE (op0), op1, op2);
          if (simplified)
            return simplified;
      }

      if (COMPARISON_P (op0) && ! side_effects_p (op0)){
          machine_mode cmp_mode = (GET_MODE (XEXP (op0, 0)) == VOIDmode
                    ? GET_MODE (XEXP (op0, 1)): GET_MODE (XEXP (op0, 0)));
          rtx temp;
          /* Look for happy constants in op1 and op2.  */
          if (CONST_INT_P (op1) && CONST_INT_P (op2)){
              HOST_WIDE_INT t = INTVAL (op1);
              HOST_WIDE_INT f = INTVAL (op2);

              if (t == mtcs_real_get_store_flag_value(mtcsReal) && f == 0)
                code = GET_CODE (op0);
              else if (t == 0 && f == mtcs_real_get_store_flag_value(mtcsReal)){
                  enum rtx_code tmp;
                  tmp = mtcs_dojump_reversed_comparison_code/*!reversed_comparison_code*/(mtcsDojump,op0, NULL);
                  if (tmp == UNKNOWN)
                    break;
                  code = tmp;
              }else
                  break;
              return mtcs_simplify_rtx_gen_relational (self,code, mode, cmp_mode,XEXP (op0, 0), XEXP (op0, 1));
          }
          temp = mtcs_simplify_rtx_relational_operation (self,GET_CODE (op0), op0_mode,cmp_mode, XEXP (op0, 0),XEXP (op0, 1));

          /* See if any simplifications were possible.  */
          if (temp){
              if (CONST_INT_P (temp))
                  return temp == const0_rtx ? op2 : op1;
              else if (temp)
                  return gen_rtx_IF_THEN_ELSE (mode, temp, op1, op2);
          }
      }
      break;

    case VEC_MERGE:
      gcc_assert (GET_MODE (op0) == mode);
      gcc_assert (GET_MODE (op1) == mode);
      gcc_assert (mtcs_mode_is_vector_p(mtcsMode,mode));
      trueop2 = mtcs_simplify_rtx_avoid_constant_pool_reference (self,op2);
      if (CONST_INT_P (trueop2) && mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/ (mtcsMode,mode).is_constant (&n_elts)){
          unsigned HOST_WIDE_INT sel = UINTVAL (trueop2);
          unsigned HOST_WIDE_INT mask;
          if (n_elts == HOST_BITS_PER_WIDE_INT)
            mask = -1;
          else
            mask = (HOST_WIDE_INT_1U << n_elts) - 1;

          if (!(sel & mask) && !side_effects_p (op0))
            return op1;
          if ((sel & mask) == mask && !side_effects_p (op1))
            return op0;

          rtx trueop0 = mtcs_simplify_rtx_avoid_constant_pool_reference (self,op0);
          rtx trueop1 = mtcs_simplify_rtx_avoid_constant_pool_reference (self,op1);
          if (GET_CODE (trueop0) == CONST_VECTOR  && GET_CODE (trueop1) == CONST_VECTOR){
              rtvec v = rtvec_alloc (n_elts);
              unsigned int i;

              for (i = 0; i < n_elts; i++)
                  RTVEC_ELT (v, i) = ((sel & (HOST_WIDE_INT_1U << i))
                        ? mtcs_rtl_const_vector_elt/*!CONST_VECTOR_ELT*/ (mtcsRTL,trueop0, i)
                        :  mtcs_rtl_const_vector_elt/*!CONST_VECTOR_ELT*/ (mtcsRTL,trueop1, i));
              return mtcs_rtl_gen_rtx_CONST_VECTOR (mtcsRTL,mode, v);
          }

          /* Replace (vec_merge (vec_merge a b m) c n) with (vec_merge b c n)
             if no element from a appears in the result.  */
          if (GET_CODE (op0) == VEC_MERGE){
              tem = mtcs_simplify_rtx_avoid_constant_pool_reference (self,XEXP (op0, 2));
              if (CONST_INT_P (tem)){
                  unsigned HOST_WIDE_INT sel0 = UINTVAL (tem);
                  if (!(sel & sel0 & mask) && !side_effects_p (XEXP (op0, 0)))
                    return mtcs_simplify_rtx_gen_ternary (self,code, mode, mode,XEXP (op0, 1), op1, op2);
                  if (!(sel & ~sel0 & mask) && !side_effects_p (XEXP (op0, 1)))
                    return mtcs_simplify_rtx_gen_ternary (self,code, mode, mode,XEXP (op0, 0), op1, op2);
              }
          }
          if (GET_CODE (op1) == VEC_MERGE){
              tem = mtcs_simplify_rtx_avoid_constant_pool_reference (self,XEXP (op1, 2));
              if (CONST_INT_P (tem)){
                  unsigned HOST_WIDE_INT sel1 = UINTVAL (tem);
                  if (!(~sel & sel1 & mask) && !side_effects_p (XEXP (op1, 0)))
                    return mtcs_simplify_rtx_gen_ternary (self,code, mode, mode,op0, XEXP (op1, 1), op2);
                  if (!(~sel & ~sel1 & mask) && !side_effects_p (XEXP (op1, 1)))
                    return mtcs_simplify_rtx_gen_ternary (self,code, mode, mode,op0, XEXP (op1, 0), op2);
              }
          }

          /* Replace (vec_merge (vec_duplicate (vec_select a parallel (i))) a 1 << i)
             with a.  */
          if (GET_CODE (op0) == VEC_DUPLICATE
              && GET_CODE (XEXP (op0, 0)) == VEC_SELECT
              && GET_CODE (XEXP (XEXP (op0, 0), 1)) == PARALLEL
              && known_eq (mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/ (mtcsMode,GET_MODE (XEXP (op0, 0))), 1)){
              tem = XVECEXP ((XEXP (XEXP (op0, 0), 1)), 0, 0);
              if (CONST_INT_P (tem) && CONST_INT_P (op2)){
                  if (XEXP (XEXP (op0, 0), 0) == op1 && UINTVAL (op2) == HOST_WIDE_INT_1U << UINTVAL (tem))
                    return op1;
              }
          }
          /* Replace (vec_merge (vec_duplicate (X)) (const_vector [A, B])
             (const_int N))
             with (vec_concat (X) (B)) if N == 1 or
             (vec_concat (A) (X)) if N == 2.  */
          if (GET_CODE (op0) == VEC_DUPLICATE
              && GET_CODE (op1) == CONST_VECTOR
              && known_eq (CONST_VECTOR_NUNITS (op1), 2)
              && known_eq (mtcs_mode_get_nunits (mtcsMode,GET_MODE (op0)), 2)
              && IN_RANGE (sel, 1, 2)){
              rtx newop0 = XEXP (op0, 0);
              rtx newop1 = mtcs_rtl_const_vector_elt (mtcsRTL,op1, 2 - sel);
              if (sel == 2)
                  std::swap (newop0, newop1);
              return mtcs_simplify_rtx_gen_binary (self,VEC_CONCAT, mode, newop0, newop1);
           }
          /* Replace (vec_merge (vec_duplicate x) (vec_concat (y) (z)) (const_int N))
             with (vec_concat x z) if N == 1, or (vec_concat y x) if N == 2.
             Only applies for vectors of two elements.  */
          if (GET_CODE (op0) == VEC_DUPLICATE
              && GET_CODE (op1) == VEC_CONCAT
              && known_eq (mtcs_mode_get_nunits (mtcsMode,GET_MODE (op0)), 2)
              && known_eq (mtcs_mode_get_nunits (mtcsMode,GET_MODE (op1)), 2)
              && IN_RANGE (sel, 1, 2)){
              rtx newop0 = XEXP (op0, 0);
              rtx newop1 = XEXP (op1, 2 - sel);
              rtx otherop = XEXP (op1, sel - 1);
              if (sel == 2)
                  std::swap (newop0, newop1);
              /* Don't want to throw away the other part of the vec_concat if
             it has side-effects.  */
              if (!side_effects_p (otherop))
                  return mtcs_simplify_rtx_gen_binary (self,VEC_CONCAT, mode, newop0, newop1);
           }

          /* Replace:

              (vec_merge:outer (vec_duplicate:outer x:inner)
                       (subreg:outer y:inner 0)
                       (const_int N))

             with (vec_concat:outer x:inner y:inner) if N == 1,
             or (vec_concat:outer y:inner x:inner) if N == 2.

             Implicitly, this means we have a paradoxical subreg, but such
             a check is cheap, so make it anyway.

             Only applies for vectors of two elements.  */
          if (GET_CODE (op0) == VEC_DUPLICATE
              && GET_CODE (op1) == SUBREG
              && GET_MODE (op1) == GET_MODE (op0)
              && GET_MODE (SUBREG_REG (op1)) == GET_MODE (XEXP (op0, 0))
              && mtcs_rtl_paradoxical_subreg_p (mtcsRTL,op1)
              && mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,op1)
              && known_eq (mtcs_mode_get_nunits (mtcsMode,GET_MODE (op0)), 2)
              && known_eq (mtcs_mode_get_nunits (mtcsMode,GET_MODE (op1)), 2)
              && IN_RANGE (sel, 1, 2)){
              rtx newop0 = XEXP (op0, 0);
              rtx newop1 = SUBREG_REG (op1);
              if (sel == 2)
                  std::swap (newop0, newop1);
              return mtcs_simplify_rtx_gen_binary (self,VEC_CONCAT, mode, newop0, newop1);
          }

          /* Same as above but with switched operands:
            Replace (vec_merge:outer (subreg:outer x:inner 0)
                         (vec_duplicate:outer y:inner)
                       (const_int N))

             with (vec_concat:outer x:inner y:inner) if N == 1,
             or (vec_concat:outer y:inner x:inner) if N == 2.  */
          if (GET_CODE (op1) == VEC_DUPLICATE
              && GET_CODE (op0) == SUBREG
              && GET_MODE (op0) == GET_MODE (op1)
              && GET_MODE (SUBREG_REG (op0)) == GET_MODE (XEXP (op1, 0))
              && mtcs_rtl_paradoxical_subreg_p (mtcsRTL,op0)
              && mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,op0)
              && known_eq (mtcs_mode_get_nunits (mtcsMode,GET_MODE (op1)), 2)
              && known_eq (mtcs_mode_get_nunits (mtcsMode,GET_MODE (op0)), 2)
              && IN_RANGE (sel, 1, 2)){
              rtx newop0 = SUBREG_REG (op0);
              rtx newop1 = XEXP (op1, 0);
              if (sel == 2)
                  std::swap (newop0, newop1);
              return mtcs_simplify_rtx_gen_binary (self,VEC_CONCAT, mode, newop0, newop1);
          }

          /* Replace (vec_merge (vec_duplicate x) (vec_duplicate y)
                     (const_int n))
             with (vec_concat x y) or (vec_concat y x) depending on value
             of N.  */
          if (GET_CODE (op0) == VEC_DUPLICATE
              && GET_CODE (op1) == VEC_DUPLICATE
              && known_eq (mtcs_mode_get_nunits (mtcsMode,GET_MODE (op0)), 2)
              && known_eq (mtcs_mode_get_nunits (mtcsMode,GET_MODE (op1)), 2)
              && IN_RANGE (sel, 1, 2)){
              rtx newop0 = XEXP (op0, 0);
              rtx newop1 = XEXP (op1, 0);
              if (sel == 2)
                  std::swap (newop0, newop1);

              return mtcs_simplify_rtx_gen_binary (self,VEC_CONCAT, mode, newop0, newop1);
          }
      }

      if (rtx_equal_p (op0, op1) && !side_effects_p (op2) && !side_effects_p (op1))
          return op0;

      if (!side_effects_p (op2)){
          rtx top0 = mtcs_rtlanal_may_trap_p/*!may_trap_p*/(mtcsRtlanal,op0)
                ? NULL_RTX : mtcs_simplify_rtx_merge_mask (self,op0, op2, 0);
          rtx top1 = mtcs_rtlanal_may_trap_p/*!may_trap_p*/(mtcsRtlanal,op1)
                ? NULL_RTX : mtcs_simplify_rtx_merge_mask (self,op1, op2, 1);
          if (top0 || top1)
            return mtcs_simplify_rtx_gen_ternary (self,code, mode, mode,
                         top0 ? top0 : op0, top1 ? top1 : op1, op2);
      }

      break;

    default:
      gcc_unreachable ();
    }

  return 0;
}

/* Like simplify_binary_operation except used for relational operators.
   MODE is the mode of the result. If MODE is VOIDmode, both operands must
   not also be VOIDmode.

   CMP_MODE specifies in which mode the comparison is done in, so it is
   the mode of the operands.  If CMP_MODE is VOIDmode, it is taken from
   the operands or, if both are VOIDmode, the operands are compared in
   "infinite precision".  */
//原型 rtx simplify_context::simplify_relational_operation rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_relational_operation (MtcsSimplifyRtx *self,rtx_code code,machine_mode mode, machine_mode cmp_mode,rtx op0, rtx op1)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  rtx tem, trueop0, trueop1;
  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_relational_operation 00 code:%d mode:%d cmp_mode:%d\n",code,mode,cmp_mode);
  if (cmp_mode == VOIDmode)
    cmp_mode = GET_MODE (op0);
  if (cmp_mode == VOIDmode)
    cmp_mode = GET_MODE (op1);

  tem = mtcs_simplify_rtx_const_relational_operation (self,code, cmp_mode, op0, op1);
  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_relational_operation 00aa mode:%d cmp_mode:%d tem:%p\n",mode,cmp_mode,tem);

  if (tem)
    return relational_result (self,mode, cmp_mode, tem);
  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_relational_operation 11 mode:%d cmp_mode:%d\n",mode,cmp_mode);

  /* For the following tests, ensure const0_rtx is op1.  */
  if (mtcs_rtlanal_swap_commutative_operands_p/*!swap_commutative_operands_p*/(mtcsRtlanal, op0, op1)
      || (op0 == const0_rtx && op1 != const0_rtx))
    std::swap (op0, op1), code = swap_condition (code);

  /* If op0 is a compare, extract the comparison arguments from it.  */
  if (GET_CODE (op0) == COMPARE && op1 == const0_rtx)
    return mtcs_simplify_rtx_gen_relational (self,code, mode, VOIDmode,XEXP (op0, 0), XEXP (op0, 1));
  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_relational_operation 22 mode:%d cmp_mode:%d\n",mode,cmp_mode);

  if (mtcs_mode_get_class (mtcsMode,cmp_mode) == MODE_CC)
    return NULL_RTX;
  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_relational_operation 33 mode:%d cmp_mode:%d\n",mode,cmp_mode);

  trueop0 = mtcs_simplify_rtx_avoid_constant_pool_reference (self,op0);
  trueop1 = mtcs_simplify_rtx_avoid_constant_pool_reference (self,op1);
  return mtcs_simplify_rtx_relational_operation_1 (self,code, mode, cmp_mode,trueop0, trueop1);
}

/* Check if the given comparison (done in the given MODE) is actually
   a tautology or a contradiction.  If the mode is VOIDmode, the
   comparison is done in "infinite precision".  If no simplification
   is possible, this function returns zero.  Otherwise, it returns
   either const_true_rtx or const0_rtx.  */
//原型 simplify_const_relational_operation rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_const_relational_operation (MtcsSimplifyRtx *self,enum rtx_code code,machine_mode mode,rtx op0, rtx op1)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  rtx tem;
  rtx trueop0;
  rtx trueop1;

  gcc_assert (mode != VOIDmode || (GET_MODE (op0) == VOIDmode && GET_MODE (op1) == VOIDmode));
  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 00code:%d mode:%d %p %p GET_CODE (op0):%d COMPARE = %d\n",
        code,mode,op1,const0_rtx,GET_CODE (op0),COMPARE);
  /* We only handle MODE_CC comparisons that are COMPARE against zero.  */
  if (mtcs_mode_get_class (mtcsMode,mode) == MODE_CC  && (op1 != const0_rtx || GET_CODE (op0) != COMPARE))
    return NULL_RTX;
  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 11 code:%d mode:%d\n",code,mode);

  /* If op0 is a compare, extract the comparison arguments from it.  */
  if (GET_CODE (op0) == COMPARE && op1 == const0_rtx){
      op1 = XEXP (op0, 1);
      op0 = XEXP (op0, 0);
      if (GET_MODE (op0) != VOIDmode)
          mode = GET_MODE (op0);
      else if (GET_MODE (op1) != VOIDmode)
          mode = GET_MODE (op1);
      else{
         n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 22 code:%d mode:%d\n",code,mode);

          return 0;
      }
  }
  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 33 code:%d mode:%d\n",code,mode);

  /* We can't simplify MODE_CC values since we don't know what the
     actual comparison is.  */
  if (mtcs_mode_get_class (mtcsMode,GET_MODE (op0)) == MODE_CC)
    return 0;
  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 44 code:%d mode:%d\n",code,mode);

  /* Make sure the constant is second.  */
  if (mtcs_rtlanal_swap_commutative_operands_p/*!swap_commutative_operands_p*/(mtcsRtlanal, op0, op1)){
      std::swap (op0, op1);
      code = swap_condition (code);
  }
  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 55 code:%d mode:%d\n",code,mode);
  mtcs_print_rtl(stderr,op0);
  mtcs_print_rtl(stderr,op1);

  trueop0 = mtcs_simplify_rtx_avoid_constant_pool_reference (self,op0);
  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 55aa code:%d mode:%d\n",code,mode);
  mtcs_print_rtl(stderr,trueop0);
  trueop1 = mtcs_simplify_rtx_avoid_constant_pool_reference (self,op1);
  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 55bb code:%d mode:%d\n",code,mode);
  mtcs_print_rtl(stderr,trueop1);

  /* For integer comparisons of A and B maybe we can simplify A - B and can
     then simplify a comparison of that with zero.  If A and B are both either
     a register or a CONST_INT, this can't help; testing for these cases will
     prevent infinite recursion here and speed things up.

     We can only do this for EQ and NE comparisons as otherwise we may
     lose or introduce overflow which we cannot disregard as undefined as
     we do not know the signedness of the operation on either the left or
     the right hand side of the comparison.  */

  if (mtcs_mode_is_integral_p/*!INTEGRAL_MODE_P*/ (mtcsMode,mode) && trueop1 != const0_rtx
      && (code == EQ || code == NE)
      && ! ((REG_P (op0) || CONST_INT_P (trueop0))
        && (REG_P (op1) || CONST_INT_P (trueop1)))
      && (tem = mtcs_simplify_rtx_binary_operation (self,MINUS, mode, op0, op1)) != 0
      /* We cannot do this if tem is a nonzero address.  */
      && ! mtcs_rtlanal_nonzero_address_p/*!nonzero_address_p*/(mtcsRtlanal,tem))
    return mtcs_simplify_rtx_const_relational_operation (self,signed_condition (code),mode, tem, const0_rtx);

  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 66 code:%d mode:%d\n",code,mode);

  if (!mtcs_mode_honor_nans/*!HONOR_NANS*/ (mtcsMode,mode) && code == ORDERED)
    return const_true_rtx;

  if (! mtcs_mode_honor_nans (mtcsMode,mode) && code == UNORDERED)
    return const0_rtx;
  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 77 code:%d mode:%d\n",code,mode);

  /* For modes without NaNs, if the two operands are equal, we know the
     result except if they have side-effects.  Even with NaNs we know
     the result of unordered comparisons and, if signaling NaNs are
     irrelevant, also the result of LT/GT/LTGT.  */
  if ((! mtcs_mode_honor_nans/*!HONOR_NANS (trueop0)*/(mtcsMode,GET_MODE (trueop0))
       || code == UNEQ || code == UNLE || code == UNGE
       || ((code == LT || code == GT || code == LTGT)
       && !mtcs_mode_honor_snans/*! HONOR_SNANS (trueop0)*/(mtcsMode,GET_MODE (trueop0)) ))
      && rtx_equal_p (trueop0, trueop1)
      && ! side_effects_p (trueop0))
    return comparison_result (code, CMP_EQ);
  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 88 code:%d mode:%d\n",code,mode);

  /* If the operands are floating-point constants, see if we can fold
     the result.  */
  if (CONST_DOUBLE_AS_FLOAT_P (trueop0)
      && CONST_DOUBLE_AS_FLOAT_P (trueop1)
      && mtcs_mode_is_scalar_float_p/*!SCALAR_FLOAT_MODE_P*/ (mtcsMode,GET_MODE (trueop0))){
     n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 88aa code:%d mode:%d\n",code,mode);

      const REAL_VALUE_TYPE *d0 = CONST_DOUBLE_REAL_VALUE (trueop0);
      const REAL_VALUE_TYPE *d1 = CONST_DOUBLE_REAL_VALUE (trueop1);

      /* Comparisons are unordered iff at least one of the values is NaN.  */
      if (REAL_VALUE_ISNAN (*d0) || REAL_VALUE_ISNAN (*d1))
        switch (code){
          case UNEQ:
          case UNLT:
          case UNGT:
          case UNLE:
          case UNGE:
          case NE:
          case UNORDERED:
            return const_true_rtx;
          case EQ:
          case LT:
          case GT:
          case LE:
          case GE:
          case LTGT:
          case ORDERED:
            return const0_rtx;
          default:
            return 0;
        }

      return comparison_result (code,(real_equal (d0, d1) ? CMP_EQ : real_less (d0, d1) ? CMP_LT : CMP_GT));
  }
  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 99 code:%d mode:%d\n",code,mode);

  /* Otherwise, see if the operands are both integers.  */
  if ((mtcs_mode_get_class (mtcsMode,mode) == MODE_INT || mode == VOIDmode)
      && CONST_SCALAR_INT_P (trueop0) && CONST_SCALAR_INT_P (trueop1)){
      /* It would be nice if we really had a mode here.  However, the
     largest int representable on the target is as good as
     infinite.  */
     n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 99aa code:%d mode:%d\n",code,mode);

      machine_mode cmode = (mode == VOIDmode) ? mtcsMode->modesMinMax.max_INT/*!MAX_MODE_INT*/ : mode;
      mtcs_rtx_mode_t ptrueop0 = mtcs_rtx_mode_t/*!rtx_mode_t*/(trueop0, cmode);
      mtcs_rtx_mode_t ptrueop1 = mtcs_rtx_mode_t/*!rtx_mode_t*/(trueop1, cmode);

      if (wi::eq_p (ptrueop0, ptrueop1))
          return comparison_result (code, CMP_EQ);
      else{
          int cr = wi::lts_p (ptrueop0, ptrueop1) ? CMP_LT : CMP_GT;
          cr |= wi::ltu_p (ptrueop0, ptrueop1) ? CMP_LTU : CMP_GTU;
          return comparison_result (code, cr);
      }
  }
  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 100 code:%d mode:%d\n",code,mode);

  /* Optimize comparisons with upper and lower bounds.  */
  scalar_int_mode int_mode;
  if (CONST_INT_P (trueop1)
      && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)
      && mtcs_mode_is_hwi_computable_p/*!HWI_COMPUTABLE_MODE_P*/ (mtcsMode,int_mode)
      && !side_effects_p (trueop0)){
     n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 100aa code:%d mode:%d %d\n",code,mode,int_mode);

      int sign;
      unsigned HOST_WIDE_INT nonzero = mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(mtcsRtlanal,trueop0, int_mode);
      HOST_WIDE_INT val = INTVAL (trueop1);
      HOST_WIDE_INT mmin, mmax;
      n_debug("mtcssimplifyrtx.c xx---xx NE:%d EQ:%d GE:%d GT:%d LE:%d LT:%d LTGT:%d GEU:%d GTU:%d LEU:%d LTU:%d\n",
            NE,EQ,GE,GT,LE,LT,LTGT,GEU,GTU,LEU,LTU);


      if (code == GEU || code == LEU || code == GTU || code == LTU)
          sign = 0;
      else
          sign = 1;
      n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 100bb "
            "code:%d mode:%d nonzero:"HOST_WIDE_INT_PRINT_UNSIGNED" val:"HOST_WIDE_INT_PRINT_UNSIGNED" sign:%d\n",
                  code,mode,nonzero,val,sign);
      /* Get a reduced range if the sign bit is zero.  */
      if (nonzero <= (mtcs_mode_get_mask(mtcsMode,int_mode) >> 1)){

          mmin = 0;
          mmax = nonzero;
          n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 100cc code:%d"
                "mode:%d "HOST_WIDE_INT_PRINT_DEC HOST_WIDE_INT_PRINT_DEC"\n",
                         code,mode,mmin,mmax);

      }else{
          rtx mmin_rtx, mmax_rtx;
          mtc_rtl_get_mode_bounds/*get_mode_bounds*/ (mtcsRTL,int_mode, sign, int_mode, &mmin_rtx, &mmax_rtx);
          mmin = INTVAL (mmin_rtx);
          mmax = INTVAL (mmax_rtx);
          n_debug("mtcssimplifyrtx.c  mtcs_simplify_rtx_const_relational_operation 100dd code:%d mode:%d min:"
                HOST_WIDE_INT_PRINT_DEC "max:"HOST_WIDE_INT_PRINT_DEC" nonzero:"HOST_WIDE_INT_PRINT_UNSIGNED"\n",
                code,mode,mmin,mmax,nonzero);
          if (sign){
              unsigned int sign_copies = mtcs_rtlanal_num_sign_bit_copies/*!num_sign_bit_copies*/(mtcsRtlanal,trueop0, int_mode);
              mmin >>= (sign_copies - 1);
              mmax >>= (sign_copies - 1);
              n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 100ee code:%d mode:%d"
                                 "min:"HOST_WIDE_INT_PRINT_DEC" max:"HOST_WIDE_INT_PRINT_DEC" nonzero:"HOST_WIDE_INT_PRINT_UNSIGNED"\n",
                                  code,mode,mmin,mmax,nonzero);
          }
          n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 100ff code:%d mode:%d"
                        "min:"HOST_WIDE_INT_PRINT_DEC" max:"HOST_WIDE_INT_PRINT_DEC" nonzero:"HOST_WIDE_INT_PRINT_UNSIGNED"\n",
                         code,mode,mmin,mmax,nonzero);

      }

      switch (code){
        /* x >= y is always true for y <= mmin, always false for y > mmax.  */
        case GEU:
          if ((unsigned HOST_WIDE_INT) val <= (unsigned HOST_WIDE_INT) mmin)
            return const_true_rtx;
          if ((unsigned HOST_WIDE_INT) val > (unsigned HOST_WIDE_INT) mmax)
            return const0_rtx;
          break;
        case GE:
          if (val <= mmin)
            return const_true_rtx;
          if (val > mmax)
            return const0_rtx;
          break;

        /* x <= y is always true for y >= mmax, always false for y < mmin.  */
        case LEU:
          if ((unsigned HOST_WIDE_INT) val >= (unsigned HOST_WIDE_INT) mmax)
            return const_true_rtx;
          if ((unsigned HOST_WIDE_INT) val < (unsigned HOST_WIDE_INT) mmin)
            return const0_rtx;
          break;
        case LE:
          if (val >= mmax)
            return const_true_rtx;
          if (val < mmin)
            return const0_rtx;
          break;

        case EQ:
          /* x == y is always false for y out of range.  */
          if (val < mmin || val > mmax)
            return const0_rtx;
          break;

        /* x > y is always false for y >= mmax, always true for y < mmin.  */
        case GTU:
          if ((unsigned HOST_WIDE_INT) val >= (unsigned HOST_WIDE_INT) mmax)
            return const0_rtx;
          if ((unsigned HOST_WIDE_INT) val < (unsigned HOST_WIDE_INT) mmin)
            return const_true_rtx;
          break;
        case GT:
          if (val >= mmax)
            return const0_rtx;
          if (val < mmin)
            return const_true_rtx;
          break;

        /* x < y is always false for y <= mmin, always true for y > mmax.  */
        case LTU:
          if ((unsigned HOST_WIDE_INT) val <= (unsigned HOST_WIDE_INT) mmin)
            return const0_rtx;
          if ((unsigned HOST_WIDE_INT) val > (unsigned HOST_WIDE_INT) mmax)
            return const_true_rtx;
          break;
        case LT:
          if (val <= mmin)
            return const0_rtx;
          if (val > mmax)
            return const_true_rtx;
          break;

        case NE:
          /* x != y is always true for y out of range.  */
          if (val < mmin || val > mmax)
            return const_true_rtx;
          break;

        default:
          break;
      }
  }
  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 101 code:%d mode:%d\n",code,mode);

  /* Optimize integer comparisons with zero.  */
  if (mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode) && trueop1 == const0_rtx && !side_effects_p (trueop0)){
      /* Some addresses are known to be nonzero.  We don't know
     their sign, but equality comparisons are known.  */
     n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 102 code:%d %s mode:%d %d\n",
           code,GET_RTX_NAME(code),mode,mtcs_rtlanal_nonzero_address_p/*!nonzero_address_p*/(mtcsRtlanal,trueop0));
     mtcs_print_rtl_single(stderr,trueop0);

      if (mtcs_rtlanal_nonzero_address_p/*!nonzero_address_p*/(mtcsRtlanal,trueop0)){
          if (code == EQ || code == LEU)
            return const0_rtx;
          if (code == NE || code == GTU)
            return const_true_rtx;
      }
      n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 103 code:%d mode:%d\n",code,mode);

      /* See if the first operand is an IOR with a constant.  If so, we
     may be able to determine the result of this comparison.  */
      if (GET_CODE (op0) == IOR){
          rtx inner_const = mtcs_simplify_rtx_avoid_constant_pool_reference (self,XEXP (op0, 1));
          if (CONST_INT_P (inner_const) && inner_const != const0_rtx){
              int sign_bitnum = mtcs_mode_get_precision(mtcsMode,int_mode) - 1;
              int has_sign = (HOST_BITS_PER_WIDE_INT >= sign_bitnum && (UINTVAL (inner_const) & (HOST_WIDE_INT_1U << sign_bitnum)));
              switch (code){
                case EQ:
                case LEU:
                  return const0_rtx;
                case NE:
                case GTU:
                  return const_true_rtx;
                case LT:
                case LE:
                  if (has_sign)
                    return const_true_rtx;
                  break;
                case GT:
                case GE:
                  if (has_sign)
                    return const0_rtx;
                  break;
                default:
                  break;
              }//end switch
          }
      }
  }
  n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 104 code:%d mode:%d\n",code,mode);

  /* Optimize comparison of ABS with zero.  */
  if (trueop1 == CONST0_RTX (mode) && !side_effects_p (trueop0)
      && (GET_CODE (trueop0) == ABS  || (GET_CODE (trueop0) == FLOAT_EXTEND  && GET_CODE (XEXP (trueop0, 0)) == ABS))){
     n_debug("mtcssimplifyrtx.c mtcs_simplify_rtx_const_relational_operation 105 code:%d mode:%d\n",code,mode);

      switch (code){
        case LT:
          /* Optimize abs(x) < 0.0.  */
          if (!mtcs_mode_is_integral_p/*!INTEGRAL_MODE_P*/ (mtcsMode,mode) && !mtcs_mode_honor_snans/*!HONOR_SNANS*/(mtcsMode,mode))
            return const0_rtx;
          break;

        case GE:
          /* Optimize abs(x) >= 0.0.  */
          if (!mtcs_mode_is_integral_p (mtcsMode,mode) && !mtcs_mode_honor_nans(mtcsMode,mode))
            return const_true_rtx;
          break;

        case UNGE:
          /* Optimize ! (abs(x) < 0.0).  */
          return const_true_rtx;

        default:
          break;
        }
  }
  return 0;
}


/* Simplify and canonicalize a PLUS or MINUS, at least one of whose
   operands may be another PLUS or MINUS.

   Rather than test for specific case, we do this by a brute-force method
   and do all possible simplifications until no more changes occur.  Then
   we rebuild the operation.

   May return NULL_RTX when no changes were made.  */
//原型 simplify_context::simplify_plus_minus  rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_plus_minus (MtcsSimplifyRtx *self,rtx_code code, machine_mode mode, rtx op0, rtx op1)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  struct simplify_plus_minus_op_data{
    rtx op;
    short neg;
  } ops[16];
  rtx result, tem;
  int n_ops = 2;
  int changed, n_constants, canonicalized = 0;
  int i, j;
  memset (ops, 0, sizeof ops);
  /* Set up the two operands and then expand them until nothing has been
     changed.  If we run out of room in our array, give up; this should
     almost never happen.  */
  ops[0].op = op0;
  ops[0].neg = 0;
  ops[1].op = op1;
  ops[1].neg = (code == MINUS);

  do{
      changed = 0;
      n_constants = 0;
      for (i = 0; i < n_ops; i++){
          rtx this_op = ops[i].op;
          int this_neg = ops[i].neg;
          enum rtx_code this_code = GET_CODE (this_op);

          switch (this_code){
            case PLUS:
            case MINUS:
              if (n_ops == ARRAY_SIZE (ops))
                  return NULL_RTX;

              ops[n_ops].op = XEXP (this_op, 1);
              ops[n_ops].neg = (this_code == MINUS) ^ this_neg;
              n_ops++;
              ops[i].op = XEXP (this_op, 0);
              changed = 1;
              /* If this operand was negated then we will potentially
             canonicalize the expression.  Similarly if we don't
             place the operands adjacent we're re-ordering the
             expression and thus might be performing a
             canonicalization.  Ignore register re-ordering.
             ??? It might be better to shuffle the ops array here,
             but then (plus (plus (A, B), plus (C, D))) wouldn't
             be seen as non-canonical.  */
              if (this_neg || (i != n_ops - 2  && !(REG_P (ops[i].op) && REG_P (ops[n_ops - 1].op))))
                  canonicalized = 1;
              break;

            case NEG:
              ops[i].op = XEXP (this_op, 0);
              ops[i].neg = ! this_neg;
              changed = 1;
              canonicalized = 1;
              break;

            case CONST:
              if (n_ops != ARRAY_SIZE (ops) && GET_CODE (XEXP (this_op, 0)) == PLUS
                      && CONSTANT_P (XEXP (XEXP (this_op, 0), 0))  && CONSTANT_P (XEXP (XEXP (this_op, 0), 1))){
                  ops[i].op = XEXP (XEXP (this_op, 0), 0);
                  ops[n_ops].op = XEXP (XEXP (this_op, 0), 1);
                  ops[n_ops].neg = this_neg;
                  n_ops++;
                  changed = 1;
                  canonicalized = 1;
              }
              break;

            case NOT:
              /* ~a -> (-a - 1) */
              if (n_ops != ARRAY_SIZE (ops)){
                  ops[n_ops].op = CONSTM1_RTX (mode);
                  ops[n_ops++].neg = this_neg;
                  ops[i].op = XEXP (this_op, 0);
                  ops[i].neg = !this_neg;
                  changed = 1;
                  canonicalized = 1;
              }
              break;

            CASE_CONST_SCALAR_INT:
            case CONST_POLY_INT:
              n_constants++;
              if (this_neg){
                  ops[i].op = neg_poly_int_rtx (self,mode, this_op);
                  ops[i].neg = 0;
                  changed = 1;
                  canonicalized = 1;
              }
              break;
            default:
              break;
          }
      }//end for
  }while (changed);

  if (n_constants > 1)
    canonicalized = 1;

  gcc_assert (n_ops >= 2);

  /* If we only have two operands, we can avoid the loops.  */
  if (n_ops == 2){
      enum rtx_code code = ops[0].neg || ops[1].neg ? MINUS : PLUS;
      rtx lhs, rhs;
      /* Get the two operands.  Be careful with the order, especially for
     the cases where code == MINUS.  */
      if (ops[0].neg && ops[1].neg){
          lhs = gen_rtx_NEG (mode, ops[0].op);
          rhs = ops[1].op;
      }else if (ops[0].neg){
          lhs = ops[1].op;
          rhs = ops[0].op;
      }else{
          lhs = ops[0].op;
          rhs = ops[1].op;
      }
      return mtcs_simplify_rtx_const_binary_operation (self,code, mode, lhs, rhs);
  }

  /* Now simplify each pair of operands until nothing changes.  */
  while (1){
      /* Insertion sort is good enough for a small array.  */
      for (i = 1; i < n_ops; i++){
          struct simplify_plus_minus_op_data save;
          int cmp;
          j = i - 1;
          cmp = simplify_plus_minus_op_data_cmp (ops[j].op, ops[i].op);
          if (cmp <= 0)
            continue;
          /* Just swapping registers doesn't count as canonicalization.  */
          if (cmp != 1)
            canonicalized = 1;
          save = ops[i];
          do
            ops[j + 1] = ops[j];
          while (j--    && simplify_plus_minus_op_data_cmp (ops[j].op, save.op) > 0);
          ops[j + 1] = save;
      }

      changed = 0;
      for (i = n_ops - 1; i > 0; i--)
        for (j = i - 1; j >= 0; j--){
            rtx lhs = ops[j].op, rhs = ops[i].op;
            int lneg = ops[j].neg, rneg = ops[i].neg;
            if (lhs != 0 && rhs != 0){
                enum rtx_code ncode = PLUS;
                if (lneg != rneg){
                    ncode = MINUS;
                    if (lneg)
                      std::swap (lhs, rhs);
                }else if (mtcs_rtlanal_swap_commutative_operands_p/*!swap_commutative_operands_p*/(mtcsRtlanal, lhs, rhs))
                  std::swap (lhs, rhs);

                if ((GET_CODE (lhs) == CONST || CONST_INT_P (lhs)) && (GET_CODE (rhs) == CONST || CONST_INT_P (rhs))){
                    rtx tem_lhs, tem_rhs;
                    tem_lhs = GET_CODE (lhs) == CONST ? XEXP (lhs, 0) : lhs;
                    tem_rhs = GET_CODE (rhs) == CONST ? XEXP (rhs, 0) : rhs;
                    tem = mtcs_simplify_rtx_binary_operation (self,ncode, mode, tem_lhs,tem_rhs);
                    if (tem && !CONSTANT_P (tem))
                      tem = gen_rtx_CONST (GET_MODE (tem), tem);
                }else
                  tem = mtcs_simplify_rtx_binary_operation (self,ncode, mode, lhs, rhs);

                if (tem){
                    /* Reject "simplifications" that just wrap the two
                       arguments in a CONST.  Failure to do so can result
                       in infinite recursion with simplify_binary_operation
                       when it calls us to simplify CONST operations.
                       Also, if we find such a simplification, don't try
                       any more combinations with this rhs:  We must have
                       something like symbol+offset, ie. one of the
                       trivial CONST expressions we handle later.  */
                    if (GET_CODE (tem) == CONST
                    && GET_CODE (XEXP (tem, 0)) == ncode
                    && XEXP (XEXP (tem, 0), 0) == lhs
                    && XEXP (XEXP (tem, 0), 1) == rhs)
                      break;
                    lneg &= rneg;
                    if (GET_CODE (tem) == NEG)
                      tem = XEXP (tem, 0), lneg = !lneg;
                    if (poly_int_rtx_p (tem) && lneg)
                      tem = neg_poly_int_rtx (self,mode, tem), lneg = 0;

                    ops[i].op = tem;
                    ops[i].neg = lneg;
                    ops[j].op = NULL_RTX;
                    changed = 1;
                    canonicalized = 1;
                 }
            }
        }//end for (j = i - 1; j >= 0; j--){

      if (!changed)
          break;

      /* Pack all the operands to the lower-numbered entries.  */
      for (i = 0, j = 0; j < n_ops; j++)
        if (ops[j].op){
            ops[i] = ops[j];
            i++;
        }
      n_ops = i;
  }//end while(1)

  /* If nothing changed, check that rematerialization of rtl instructions
     is still required.  */
  if (!canonicalized){
      /* Perform rematerialization if only all operands are registers and
     all operations are PLUS.  */
      /* ??? Also disallow (non-global, non-frame) fixed registers to work
     around rs6000 and how it uses the CA register.  See PR67145.  */
      for (i = 0; i < n_ops; i++)
        if (ops[i].neg
            || !REG_P (ops[i].op)
            || (REGNO (ops[i].op) < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
            && mtcsReg->hardRegs.x_fixed_regs/*!fixed_regs*/[REGNO (ops[i].op)]
            && !mtcsReg->global_regs/*!global_regs*/[REGNO (ops[i].op)]
            && ops[i].op != mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL)
            && ops[i].op != arg_pointer_rtx
            && ops[i].op != mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)))
          return NULL_RTX;
      goto gen_result;
  }

  /* Create (minus -C X) instead of (neg (const (plus X C))).  */
  if (n_ops == 2
      && CONST_INT_P (ops[1].op)
      && CONSTANT_P (ops[0].op)
      && ops[0].neg)
    return gen_rtx_fmt_ee (MINUS, mode, ops[1].op, ops[0].op);

  /* We suppressed creation of trivial CONST expressions in the
     combination loop to avoid recursion.  Create one manually now.
     The combination loop should have ensured that there is exactly
     one CONST_INT, and the sort will have ensured that it is last
     in the array and that any other constant will be next-to-last.  */

  if (n_ops > 1  && poly_int_rtx_p (ops[n_ops - 1].op) && CONSTANT_P (ops[n_ops - 2].op)){
      rtx value = ops[n_ops - 1].op;
      if (ops[n_ops - 1].neg ^ ops[n_ops - 2].neg)
          value = neg_poly_int_rtx (self,mode, value);
      if (CONST_INT_P (value)){
          ops[n_ops - 2].op = mtcs_rtl_plus_constant (mtcsRTL,mode, ops[n_ops - 2].op,INTVAL (value));
          n_ops--;
      }
  }

  /* Put a non-negated operand first, if possible.  */

  for (i = 0; i < n_ops && ops[i].neg; i++)
    continue;
  if (i == n_ops)
    ops[0].op = gen_rtx_NEG (mode, ops[0].op);
  else if (i != 0){
      tem = ops[0].op;
      ops[0] = ops[i];
      ops[i].op = tem;
      ops[i].neg = 1;
  }

  /* Now make the result by performing the requested operations.  */
 gen_result:
  result = ops[0].op;
  for (i = 1; i < n_ops; i++)
    result = gen_rtx_fmt_ee (ops[i].neg ? MINUS : PLUS, mode, result, ops[i].op);

  return result;
}

/* Subroutine of simplify_binary_operation to simplify a commutative,
   associative binary operation CODE with result mode MODE, operating
   on OP0 and OP1.  CODE is currently one of PLUS, MULT, AND, IOR, XOR,
   SMIN, SMAX, UMIN or UMAX.  Return zero if no simplification or
   canonicalization is possible.  */
//原型 rtx simplify_context::simplify_associative_operation rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_associative_operation ( MtcsSimplifyRtx *self,rtx_code code,machine_mode mode,rtx op0, rtx op1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  rtx tem;

  /* Normally expressions simplified by simplify-rtx.cc are combined
     at most from a few machine instructions and therefore the
     expressions should be fairly small.  During var-tracking
     we can see arbitrarily large expressions though and reassociating
     those can be quadratic, so punt after encountering max_assoc_count
     simplify_associative_operation calls during outermost simplify_*
     call.  */
  if (++self->assoc_count >= self->max_assoc_count)
    return NULL_RTX;

  /* Linearize the operator to the left.  */
  if (GET_CODE (op1) == code){
      /* "(a op b) op (c op d)" becomes "((a op b) op c) op d)".  */
      if (GET_CODE (op0) == code){
          tem = mtcs_simplify_rtx_gen_binary (self,code, mode, op0, XEXP (op1, 0));
          return mtcs_simplify_rtx_gen_binary (self,code, mode, tem, XEXP (op1, 1));
      }

      /* "a op (b op c)" becomes "(b op c) op a".  */
      if (! mtcs_rtlanal_swap_commutative_operands_p/*!swap_commutative_operands_p*/(mtcsRtlanal, op1, op0))
          return mtcs_simplify_rtx_gen_binary (self,code, mode, op1, op0);

      std::swap (op0, op1);
  }

  if (GET_CODE (op0) == code){
      /* Canonicalize "(x op c) op y" as "(x op y) op c".  */
      if (mtcs_rtlanal_swap_commutative_operands_p/*!swap_commutative_operands_p*/(mtcsRtlanal, XEXP (op0, 1), op1)){
          tem = mtcs_simplify_rtx_gen_binary (self,code, mode, XEXP (op0, 0), op1);
          return mtcs_simplify_rtx_gen_binary (self,code, mode, tem, XEXP (op0, 1));
      }

      /* Attempt to simplify "(a op b) op c" as "a op (b op c)".  */
      tem = mtcs_simplify_rtx_binary_operation (self,code, mode, XEXP (op0, 1), op1);
      if (tem != 0)
        return mtcs_simplify_rtx_gen_binary (self,code, mode, XEXP (op0, 0), tem);

      /* Attempt to simplify "(a op b) op c" as "(a op c) op b".  */
      tem = mtcs_simplify_rtx_binary_operation (self,code, mode, XEXP (op0, 0), op1);
      if (tem != 0)
        return mtcs_simplify_rtx_gen_binary (self,code, mode, tem, XEXP (op0, 1));
  }

  return 0;
}

/* Subroutine of simplify_binary_operation_1 that looks for cases in
   which OP0 and OP1 are both vector series or vector duplicates
   (which are really just series with a step of 0).  If so, try to
   form a new series by applying CODE to the bases and to the steps.
   Return null if no simplification is possible.

   MODE is the mode of the operation and is known to be a vector
   integer mode.  */
//原型rtx simplify_context::simplify_binary_operation_series rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_binary_operation_series ( MtcsSimplifyRtx *self,rtx_code code,
                            machine_mode mode, rtx op0, rtx op1)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  rtx base0, step0;
  if (vec_duplicate_p (op0, &base0))
    step0 = const0_rtx;
  else if (!vec_series_p (op0, &base0, &step0))
    return NULL_RTX;

  rtx base1, step1;
  if (vec_duplicate_p (op1, &base1))
    step1 = const0_rtx;
  else if (!vec_series_p (op1, &base1, &step1))
    return NULL_RTX;

  /* Only create a new series if we can simplify both parts.  In other
     cases this isn't really a simplification, and it's not necessarily
     a win to replace a vector operation with a scalar operation.  */
  scalar_mode inner_mode = mtcs_mode_get_inner/*!GET_MODE_INNER*/ (mtcsMode,mode);
  rtx new_base = mtcs_simplify_rtx_binary_operation (self,code, inner_mode, base0, base1);
  if (!new_base)
    return NULL_RTX;

  rtx new_step = mtcs_simplify_rtx_binary_operation (self,code, inner_mode, step0, step1);
  if (!new_step)
    return NULL_RTX;

  return gen_vec_series (mode, new_base, new_step);
}

/* Subroutine of simplify_binary_operation_1.  Un-distribute a binary
   operation CODE with result mode MODE, operating on OP0 and OP1.
   e.g. simplify (xor (and A C) (and (B C)) to (and (xor (A B) C).
   Returns NULL_RTX if no simplification is possible.  */
//原型 rtx simplify_context::simplify_distributive_operation rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_distributive_operation ( MtcsSimplifyRtx *self,rtx_code code,machine_mode mode,rtx op0, rtx op1)
{
  enum rtx_code op = GET_CODE (op0);
  gcc_assert (GET_CODE (op1) == op);
  if (rtx_equal_p (XEXP (op0, 1), XEXP (op1, 1)) && ! side_effects_p (XEXP (op0, 1)))
    return mtcs_simplify_rtx_gen_binary (self,op, mode,
            mtcs_simplify_rtx_gen_binary (self,code, mode,XEXP (op0, 0),XEXP (op1, 0)),XEXP (op0, 1));
  if (GET_RTX_CLASS (op) == RTX_COMM_ARITH){
      if (rtx_equal_p (XEXP (op0, 0), XEXP (op1, 0))  && ! side_effects_p (XEXP (op0, 0)))
          return mtcs_simplify_rtx_gen_binary (self,op, mode,
            mtcs_simplify_rtx_gen_binary (self,code, mode,XEXP (op0, 1),XEXP (op1, 1)),XEXP (op0, 0));
      if (rtx_equal_p (XEXP (op0, 0), XEXP (op1, 1)) && ! side_effects_p (XEXP (op0, 0)))
          return mtcs_simplify_rtx_gen_binary (self,op, mode,
                  mtcs_simplify_rtx_gen_binary (self,code, mode,XEXP (op0, 1),XEXP (op1, 0)), XEXP (op0, 0));
      if (rtx_equal_p (XEXP (op0, 1), XEXP (op1, 0)) && ! side_effects_p (XEXP (op0, 1)))
          return mtcs_simplify_rtx_gen_binary (self,op, mode,
                  mtcs_simplify_rtx_gen_binary (self,code, mode,XEXP (op0, 0),XEXP (op1, 1)),XEXP (op0, 1));
  }
  return NULL_RTX;
}

/* Subroutine of simplify_binary_operation to simplify a binary operation
   CODE that can commute with byte swapping, with result mode MODE and
   operating on OP0 and OP1.  CODE is currently one of AND, IOR or XOR.
   Return zero if no simplification or canonicalization is possible.  */
//原型 rtx simplify_context::simplify_byte_swapping_operation rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_byte_swapping_operation (MtcsSimplifyRtx *self,rtx_code code,machine_mode mode,rtx op0, rtx op1)
{
  rtx tem;
  /* (op (bswap x) C1)) -> (bswap (op x C2)) with C2 swapped.  */
  if (GET_CODE (op0) == BSWAP && CONST_SCALAR_INT_P (op1)){
      tem = mtcs_simplify_rtx_gen_binary (self,code, mode, XEXP (op0, 0),
              mtcs_simplify_rtx_gen_unary (self,BSWAP, mode, op1, mode));
      return mtcs_simplify_rtx_gen_unary (self,BSWAP, mode, tem, mode);
  }
  /* (op (bswap x) (bswap y)) -> (bswap (op x y)).  */
  if (GET_CODE (op0) == BSWAP && GET_CODE (op1) == BSWAP){
      tem = mtcs_simplify_rtx_gen_binary (self,code, mode, XEXP (op0, 0), XEXP (op1, 0));
      return mtcs_simplify_rtx_gen_unary (self,BSWAP, mode, tem, mode);
  }

  return NULL_RTX;
}

/* Simplify a logical operation CODE with result mode MODE, operating on OP0
   and OP1, which should be both relational operations.  Return 0 if no such
   simplification is possible.  */
//原型 rtx simplify_context::simplify_logical_relational_operation rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_logical_relational_operation (MtcsSimplifyRtx *self,rtx_code code,machine_mode mode,rtx op0, rtx op1)
{
  /* We only handle IOR of two relational operations.  */
  if (code != IOR)
    return 0;
  if (!(COMPARISON_P (op0) && COMPARISON_P (op1)))
    return 0;
  if (!(rtx_equal_p (XEXP (op0, 0), XEXP (op1, 0)) && rtx_equal_p (XEXP (op0, 1), XEXP (op1, 1))))
    return 0;

  enum rtx_code code0 = GET_CODE (op0);
  enum rtx_code code1 = GET_CODE (op1);

  /* We don't handle unsigned comparisons currently.  */
  if (code0 == LTU || code0 == GTU || code0 == LEU || code0 == GEU)
    return 0;
  if (code1 == LTU || code1 == GTU || code1 == LEU || code1 == GEU)
    return 0;

  int mask0 = comparison_to_mask (code0);
  int mask1 = comparison_to_mask (code1);

  int mask = mask0 | mask1;

  if (mask == 15)
    return relational_result (self,mode, GET_MODE (op0), const_true_rtx);

  code = mask_to_comparison (mask);

  /* Many comparison codes are only valid for certain mode classes.  */
  if (!comparison_code_valid_for_mode (self,code, mode))
    return 0;

  op0 = XEXP (op1, 0);
  op1 = XEXP (op1, 1);

  return mtcs_simplify_rtx_gen_relational (self,code, mode, VOIDmode, op0, op1);
}

/* Test whether expression, X, is an immediate constant that represents
   the most significant bit of machine mode MODE.  */
//原型 bool mode_signbit_p rtl.h simplify-rtx.cc
bool mtcs_simplify_rtx_mode_signbit_p (MtcsSimplifyRtx *self,machine_mode mode, const_rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);

  unsigned HOST_WIDE_INT val;
  unsigned int width;
  scalar_int_mode int_mode;

  if (!mtcs_mode_is_int_mode(mtcsMode,mode, &int_mode))
    return false;

  width = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/ (mtcsMode,int_mode);
  if (width == 0)
    return false;

  if (width <= HOST_BITS_PER_WIDE_INT  && CONST_INT_P (x))
    val = INTVAL (x);
//#if TARGET_SUPPORTS_WIDE_INT
  else if (mtcs_config_get_value(mtcsConfig,MTCS_TARGET_SUPPORTS_WIDE_INT)!=0 && CONST_WIDE_INT_P (x)){
      unsigned int i;
      unsigned int elts = CONST_WIDE_INT_NUNITS (x);
      if (elts != (width + HOST_BITS_PER_WIDE_INT - 1) / HOST_BITS_PER_WIDE_INT)
          return false;
      for (i = 0; i < elts - 1; i++)
        if (CONST_WIDE_INT_ELT (x, i) != 0)
          return false;
      val = CONST_WIDE_INT_ELT (x, elts - 1);
      width %= HOST_BITS_PER_WIDE_INT;
      if (width == 0)
          width = HOST_BITS_PER_WIDE_INT;
  }
//#else
  else if (mtcs_config_get_value(mtcsConfig,MTCS_TARGET_SUPPORTS_WIDE_INT)==0 &&
        width <= HOST_BITS_PER_DOUBLE_INT  && CONST_DOUBLE_AS_INT_P (x) && CONST_DOUBLE_LOW (x) == 0){
      val = CONST_DOUBLE_HIGH (x);
      width -= HOST_BITS_PER_WIDE_INT;
  }
//#endif
  else
    /* X is not an integer constant.  */
    return false;

  if (width < HOST_BITS_PER_WIDE_INT)
    val &= (HOST_WIDE_INT_1U << width) - 1;
  return val == (HOST_WIDE_INT_1U << (width - 1));
}

/* Try to simplify a MODE truncation of OP, which has OP_MODE.
   Only handle cases where the truncated value is inherently an rvalue.

   RTL provides two ways of truncating a value:

   1. a lowpart subreg.  This form is only a truncation when both
      the outer and inner modes (here MODE and OP_MODE respectively)
      are scalar integers, and only then when the subreg is used as
      an rvalue.

      It is only valid to form such truncating subregs if the
      truncation requires no action by the target.  The onus for
      proving this is on the creator of the subreg -- e.g. the
      caller to simplify_subreg or simplify_gen_subreg -- and typically
      involves either TRULY_NOOP_TRUNCATION_MODES_P or truncated_to_mode.

   2. a TRUNCATE.  This form handles both scalar and compound integers.

   The first form is preferred where valid.  However, the TRUNCATE
   handling in simplify_unary_operation turns the second form into the
   first form when TRULY_NOOP_TRUNCATION_MODES_P or truncated_to_mode allow,
   so it is generally safe to form rvalue truncations using:

      simplify_gen_unary (TRUNCATE, ...)

   and leave simplify_unary_operation to work out which representation
   should be used.

   Because of the proof requirements on (1), simplify_truncation must
   also use simplify_gen_unary (TRUNCATE, ...) to truncate parts of OP,
   regardless of whether the outer truncation came from a SUBREG or a
   TRUNCATE.  For example, if the caller has proven that an SImode
   truncation of:

      (and:DI X Y)

   is a no-op and can be represented as a subreg, it does not follow
   that SImode truncations of X and Y are also no-ops.  On a target
   like 64-bit MIPS that requires SImode values to be stored in
   sign-extended form, an SImode truncation of:

      (and:DI (reg:DI X) (const_int 63))

   is trivially a no-op because only the lower 6 bits can be set.
   However, X is still an arbitrary 64-bit number and so we cannot
   assume that truncating it too is a no-op.  */
//原型 simplify_truncation rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_truncation (MtcsSimplifyRtx *self,machine_mode mode, rtx op, machine_mode op_mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  MtcsReg *mtcsReg = mtcs_target_get_reg(mtcsTarget);

  unsigned int precision = mtcs_mode_get_unit_precision/*!GET_MODE_UNIT_PRECISION*/ (mtcsMode,mode);
  unsigned int op_precision = mtcs_mode_get_unit_precision (mtcsMode,op_mode);
  scalar_int_mode int_mode, int_op_mode, subreg_mode;

  gcc_assert (precision <= op_precision);

  /* Optimize truncations of zero and sign extended values.  */
  if (GET_CODE (op) == ZERO_EXTEND  || GET_CODE (op) == SIGN_EXTEND){
      /* There are three possibilities.  If MODE is the same as the
     origmode, we can omit both the extension and the subreg.
     If MODE is not larger than the origmode, we can apply the
     truncation without the extension.  Finally, if the outermode
     is larger than the origmode, we can just extend to the appropriate
     mode.  */
      machine_mode origmode = GET_MODE (XEXP (op, 0));
      if (mode == origmode)
          return XEXP (op, 0);
      else if (precision <= mtcs_mode_get_unit_precision (mtcsMode,origmode))
          return mtcs_simplify_rtx_gen_unary (self,TRUNCATE, mode, XEXP (op, 0), origmode);
      else
          return mtcs_simplify_rtx_gen_unary (self,GET_CODE (op), mode,XEXP (op, 0), origmode);
    }

  /* If the machine can perform operations in the truncated mode, distribute
     the truncation, i.e. simplify (truncate:QI (op:SI (x:SI) (y:SI))) into
     (op:QI (truncate:QI (x:SI)) (truncate:QI (y:SI))).  */
  if (1 && (!mtcs_reg_get_word_register_operations/*!WORD_REGISTER_OPERATIONS*/(mtcsReg) || precision >= BITS_PER_WORD)
      && (GET_CODE (op) == PLUS || GET_CODE (op) == MINUS || GET_CODE (op) == MULT)){
      rtx op0 = mtcs_simplify_rtx_gen_unary (self,TRUNCATE, mode, XEXP (op, 0), op_mode);
      if (op0){
          rtx op1 = mtcs_simplify_rtx_gen_unary (self,TRUNCATE, mode, XEXP (op, 1), op_mode);
          if (op1)
            return mtcs_simplify_rtx_gen_binary (self,GET_CODE (op), mode, op0, op1);
      }
  }

  /* Simplify (truncate:QI (lshiftrt:SI (sign_extend:SI (x:QI)) C)) into
     to (ashiftrt:QI (x:QI) C), where C is a suitable small constant and
     the outer subreg is effectively a truncation to the original mode.  */
  if ((GET_CODE (op) == LSHIFTRT  || GET_CODE (op) == ASHIFTRT)
      /* Ensure that OP_MODE is at least twice as wide as MODE
     to avoid the possibility that an outer LSHIFTRT shifts by more
     than the sign extension's sign_bit_copies and introduces zeros
     into the high bits of the result.  */
      && 2 * precision <= op_precision   && CONST_INT_P (XEXP (op, 1))  && GET_CODE (XEXP (op, 0)) == SIGN_EXTEND
      && GET_MODE (XEXP (XEXP (op, 0), 0)) == mode   && UINTVAL (XEXP (op, 1)) < precision)
    return mtcs_simplify_rtx_gen_binary (self,ASHIFTRT, mode,XEXP (XEXP (op, 0), 0), XEXP (op, 1));

  /* Likewise (truncate:QI (lshiftrt:SI (zero_extend:SI (x:QI)) C)) into
     to (lshiftrt:QI (x:QI) C), where C is a suitable small constant and
     the outer subreg is effectively a truncation to the original mode.  */
  if ((GET_CODE (op) == LSHIFTRT || GET_CODE (op) == ASHIFTRT)  && CONST_INT_P (XEXP (op, 1))
      && GET_CODE (XEXP (op, 0)) == ZERO_EXTEND  && GET_MODE (XEXP (XEXP (op, 0), 0)) == mode  && UINTVAL (XEXP (op, 1)) < precision)
    return mtcs_simplify_rtx_gen_binary (self,LSHIFTRT, mode,XEXP (XEXP (op, 0), 0), XEXP (op, 1));

  /* Likewise (truncate:QI (ashift:SI (zero_extend:SI (x:QI)) C)) into
     to (ashift:QI (x:QI) C), where C is a suitable small constant and
     the outer subreg is effectively a truncation to the original mode.  */
  if (GET_CODE (op) == ASHIFT  && CONST_INT_P (XEXP (op, 1))  && (GET_CODE (XEXP (op, 0)) == ZERO_EXTEND
      || GET_CODE (XEXP (op, 0)) == SIGN_EXTEND) && GET_MODE (XEXP (XEXP (op, 0), 0)) == mode  && UINTVAL (XEXP (op, 1)) < precision)
    return mtcs_simplify_rtx_gen_binary (self,ASHIFT, mode, XEXP (XEXP (op, 0), 0), XEXP (op, 1));

  /* Likewise (truncate:QI (and:SI (lshiftrt:SI (x:SI) C) C2)) into
     (and:QI (lshiftrt:QI (truncate:QI (x:SI)) C) C2) for suitable C
     and C2.  */
  if (GET_CODE (op) == AND  && (GET_CODE (XEXP (op, 0)) == LSHIFTRT  || GET_CODE (XEXP (op, 0)) == ASHIFTRT)
      && CONST_INT_P (XEXP (XEXP (op, 0), 1))  && CONST_INT_P (XEXP (op, 1))){
      rtx op0 = (XEXP (XEXP (op, 0), 0));
      rtx shift_op = XEXP (XEXP (op, 0), 1);
      rtx mask_op = XEXP (op, 1);
      unsigned HOST_WIDE_INT shift = UINTVAL (shift_op);
      unsigned HOST_WIDE_INT mask = UINTVAL (mask_op);

      if (shift < precision
          /* If doing this transform works for an X with all bits set,
             it works for any X.  */
          && ((mtcs_mode_get_mask/*!GET_MODE_MASK*/ (mtcsMode,mode) >> shift) & mask)
             == ((mtcs_mode_get_mask (mtcsMode,op_mode) >> shift) & mask)
          && (op0 = mtcs_simplify_rtx_gen_unary (self,TRUNCATE, mode, op0, op_mode))
          && (op0 = mtcs_simplify_rtx_gen_binary (self,LSHIFTRT, mode, op0, shift_op))){
              mask_op = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,mtcs_mode_trunc_int_for_mode/*!trunc_int_for_mode*/(mtcsMode,mask, mode));
              return mtcs_simplify_rtx_gen_binary (self,AND, mode, op0, mask_op);
       }
  }

  /* Turn (truncate:M1 (*_extract:M2 (reg:M2) (len) (pos))) into
     (*_extract:M1 (truncate:M1 (reg:M2)) (len) (pos')) if possible without
     changing len.  */
  if ((GET_CODE (op) == ZERO_EXTRACT || GET_CODE (op) == SIGN_EXTRACT) && REG_P (XEXP (op, 0))
      && GET_MODE (XEXP (op, 0)) == GET_MODE (op) && CONST_INT_P (XEXP (op, 1))  && CONST_INT_P (XEXP (op, 2))){
      rtx op0 = XEXP (op, 0);
      unsigned HOST_WIDE_INT len = UINTVAL (XEXP (op, 1));
      unsigned HOST_WIDE_INT pos = UINTVAL (XEXP (op, 2));
      if (BITS_BIG_ENDIAN && pos >= op_precision - precision){
          op0 = mtcs_simplify_rtx_gen_unary (self,TRUNCATE, mode, op0, GET_MODE (op0));
          if (op0){
              pos -= op_precision - precision;
              return mtcs_simplify_rtx_gen_ternary (self,GET_CODE (op), mode, mode,
                      op0,XEXP (op, 1), mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,pos));
          }
      }else if (!BITS_BIG_ENDIAN && precision >= len + pos){
          op0 = mtcs_simplify_rtx_gen_unary (self,TRUNCATE, mode, op0, GET_MODE (op0));
          if (op0)
            return mtcs_simplify_rtx_gen_ternary (self,GET_CODE (op), mode, mode, op0,XEXP (op, 1), XEXP (op, 2));
      }
  }

  /* Recognize a word extraction from a multi-word subreg.  */
  if ((GET_CODE (op) == LSHIFTRT || GET_CODE (op) == ASHIFTRT)
      && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/ (mtcsMode,mode)
      && mtcs_mode_is_scalar_int_p (mtcsMode,op_mode) && precision >= BITS_PER_WORD && 2 * precision <= op_precision
      && CONST_INT_P (XEXP (op, 1))  && (INTVAL (XEXP (op, 1)) & (precision - 1)) == 0  && UINTVAL (XEXP (op, 1)) < op_precision){
      poly_int64 byte = mtcs_mode_subreg_lowpart_offset/*!subreg_lowpart_offset*/(mtcsMode,mode, op_mode);
      int shifted_bytes = INTVAL (XEXP (op, 1)) / BITS_PER_UNIT;
      return mtcs_simplify_rtx_gen_subreg (self,mode, XEXP (op, 0), op_mode,
                  (WORDS_BIG_ENDIAN? byte - shifted_bytes: byte + shifted_bytes));
  }

  /* If we have a TRUNCATE of a right shift of MEM, make a new MEM
     and try replacing the TRUNCATE and shift with it.  Don't do this
     if the MEM has a mode-dependent address.  */
  if ((GET_CODE (op) == LSHIFTRT  || GET_CODE (op) == ASHIFTRT)
      && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)
      && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,op_mode, &int_op_mode)
      && MEM_P (XEXP (op, 0))  && CONST_INT_P (XEXP (op, 1))
      && INTVAL (XEXP (op, 1)) % mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/ (mtcsMode,int_mode) == 0
      && INTVAL (XEXP (op, 1)) > 0 && INTVAL (XEXP (op, 1)) < mtcs_mode_get_bitsize (mtcsMode,int_op_mode)
      && ! mtcs_recog_mode_dependent_address_p/*!mode_dependent_address_p*/(mtcsRecog,XEXP (XEXP (op, 0), 0),
              mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,XEXP (op, 0)))
      && ! MEM_VOLATILE_P (XEXP (op, 0))  && (mtcs_mode_get_size (mtcsMode,int_mode) >= UNITS_PER_WORD   || WORDS_BIG_ENDIAN == BYTES_BIG_ENDIAN)){
      poly_int64 byte = mtcs_mode_subreg_lowpart_offset/*!subreg_lowpart_offset*/ (mtcsMode,int_mode, int_op_mode);
      int shifted_bytes = INTVAL (XEXP (op, 1)) / BITS_PER_UNIT;
      return mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/ (mtcsRTL,XEXP (op, 0), int_mode,
              (WORDS_BIG_ENDIAN ? byte - shifted_bytes : byte + shifted_bytes));
  }

  /* (truncate:SI (OP:DI ({sign,zero}_extend:DI foo:SI))) is
     (OP:SI foo:SI) if OP is NEG or ABS.  */
  if ((GET_CODE (op) == ABS || GET_CODE (op) == NEG)
      && (GET_CODE (XEXP (op, 0)) == SIGN_EXTEND
      || GET_CODE (XEXP (op, 0)) == ZERO_EXTEND)
      && GET_MODE (XEXP (XEXP (op, 0), 0)) == mode)
    return mtcs_simplify_rtx_gen_unary (self,GET_CODE (op), mode,XEXP (XEXP (op, 0), 0), mode);

  /* Simplifications of (truncate:A (subreg:B X 0)).  */
  if (GET_CODE (op) == SUBREG && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)
      && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,op_mode)
      && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (SUBREG_REG (op)), &subreg_mode)
      && mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,op)){
      /* (truncate:A (subreg:B (truncate:C X) 0)) is (truncate:A X).  */
      if (GET_CODE (SUBREG_REG (op)) == TRUNCATE){
          rtx inner = XEXP (SUBREG_REG (op), 0);
          if (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,int_mode) <= mtcs_mode_get_precision (mtcsMode,subreg_mode))
            return mtcs_simplify_rtx_gen_unary (self,TRUNCATE, int_mode, inner, GET_MODE (inner));
          else
            /* If subreg above is paradoxical and C is narrower
               than A, return (subreg:A (truncate:C X) 0).  */
            return mtcs_simplify_rtx_gen_subreg (self,int_mode, SUBREG_REG (op),subreg_mode, 0);
      }

      /* Simplifications of (truncate:A (subreg:B X:C 0)) with
     paradoxical subregs (B is wider than C).  */
      if (mtcs_mode_is_a <scalar_int_mode> (mtcsMode,op_mode, &int_op_mode)){
          unsigned int int_op_prec = mtcs_mode_get_precision (mtcsMode,int_op_mode);
          unsigned int subreg_prec = mtcs_mode_get_precision (mtcsMode,subreg_mode);
          if (int_op_prec > subreg_prec){
              if (int_mode == subreg_mode)
                  return SUBREG_REG (op);
              if (mtcs_mode_get_precision (mtcsMode,int_mode) < subreg_prec)
                  return mtcs_simplify_rtx_gen_unary (self,TRUNCATE, int_mode,SUBREG_REG (op), subreg_mode);
          }
          /* Simplification of (truncate:A (subreg:B X:C 0)) where
             A is narrower than B and B is narrower than C.  */
          else if (int_op_prec < subreg_prec  && mtcs_mode_get_precision (mtcsMode,int_mode) < int_op_prec)
            return mtcs_simplify_rtx_gen_unary (self,TRUNCATE, int_mode,SUBREG_REG (op), subreg_mode);
      }
  }

  /* (truncate:A (truncate:B X)) is (truncate:A X).  */
  if (GET_CODE (op) == TRUNCATE)
    return mtcs_simplify_rtx_gen_unary (self,TRUNCATE, mode, XEXP (op, 0),GET_MODE (XEXP (op, 0)));

  /* (truncate:A (ior X C)) is (const_int -1) if C is equal to that already,
     in mode A.  */
  if (GET_CODE (op) == IOR
      && mtcs_mode_is_scalar_int_p (mtcsMode,mode)
      && mtcs_mode_is_scalar_int_p (mtcsMode,op_mode)
      && CONST_INT_P (XEXP (op, 1))
      && mtcs_mode_trunc_int_for_mode/*!trunc_int_for_mode*/(mtcsMode,INTVAL (XEXP (op, 1)), mode) == -1)
    return constm1_rtx;

  return NULL_RTX;
}

/* Read a vector of mode MODE from the target memory image given by BYTES,
   starting at byte FIRST_BYTE.  The vector is known to be encodable using
   NPATTERNS interleaved patterns with NELTS_PER_PATTERN elements each,
   and BYTES is known to have enough bytes to supply NPATTERNS *
   NELTS_PER_PATTERN vector elements.  Each element of BYTES contains
   BITS_PER_UNIT bits and the bytes are in target memory order.

   Return the vector on success, otherwise return NULL_RTX.  */
//原型 native_decode_vector_rtx rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_native_decode_vector_rtx (MtcsSimplifyRtx *self,machine_mode mode, const vec<target_unit> &bytes,
              unsigned int first_byte, unsigned int npatterns,unsigned int nelts_per_pattern)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  /*!rtx_vector_builder builder (mode, npatterns, nelts_per_pattern);*/
  MtcsVectorBuilder  builder (mtcsMode,mode, npatterns, nelts_per_pattern);

  unsigned int elt_bits = vector_element_size (mtcs_mode_get_precision_poly/*!GET_MODE_PRECISION*/ (mtcsMode,mode),
          mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,mode));
  if (elt_bits < BITS_PER_UNIT){
      /* This is the only case in which elements can be smaller than a byte.
     Element 0 is always in the lsb of the containing byte.  */
      gcc_assert (mtcs_mode_get_class/*!GET_MODE_CLASS*/ (mtcsMode,mode) == MODE_VECTOR_BOOL);
      for (unsigned int i = 0; i < builder.encoded_nelts (); ++i){
          unsigned int bit_index = first_byte * BITS_PER_UNIT + i * elt_bits;
          unsigned int byte_index = bit_index / BITS_PER_UNIT;
          unsigned int lsb = bit_index % BITS_PER_UNIT;
          unsigned int value = bytes[byte_index] >> lsb;
          builder.quick_push (mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,value,
                  mtcs_mode_get_inner/*!GET_MODE_INNER*/ (mtcsMode,mode)));
      }
  }else{
      for (unsigned int i = 0; i < builder.encoded_nelts (); ++i){
          rtx x = mtcs_simplify_rtx_native_decode_rtx (self,mtcs_mode_get_inner/*!GET_MODE_INNER*/ (mtcsMode,mode), bytes, first_byte);
          if (!x)
            return NULL_RTX;
          builder.quick_push (x);
          first_byte += elt_bits / BITS_PER_UNIT;
      }
  }
  return builder.build ();
}

/* Read an rtx of mode MODE from the target memory image given by BYTES,
   starting at byte FIRST_BYTE.  Each element of BYTES contains BITS_PER_UNIT
   bits and the bytes are in target memory order.  The image has enough
   values to specify all bytes of MODE.

   Return the rtx on success, otherwise return NULL_RTX.  */
//原型 native_decode_rtx rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_native_decode_rtx (MtcsSimplifyRtx *self,machine_mode mode, const vec<target_unit> &bytes,unsigned int first_byte)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  if (mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/ (mtcsMode,mode)){
      /* If we know at compile time how many elements there are,
     pull each element directly from BYTES.  */
      unsigned int nelts;
      if (mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/ (mtcsMode,mode).is_constant (&nelts))
          return mtcs_simplify_rtx_native_decode_vector_rtx (self,mode, bytes, first_byte, nelts, 1);
      return NULL_RTX;
  }

  scalar_int_mode imode;
  if (mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &imode)
          && mtcs_mode_get_precision/*!GET_MODE_PRECISION*/ (mtcsMode,imode)
          <= mtcs_mode_get_max_bitsize_mode_any_int/*!MAX_BITSIZE_MODE_ANY_INT*/(mtcsMode)){
      /* Pull the bytes msb first, so that we can use simple
     shift-and-insert wide_int operations.  */
      unsigned int size = mtcs_mode_get_size/*!GET_MODE_SIZE*/ (mtcsMode,imode);
      wide_int result (wi::zero (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,imode)));
      for (unsigned int i = 0; i < size; ++i){
          unsigned int lsb = (size - i - 1) * BITS_PER_UNIT;
          /* Always constant because the inputs are.  */
          unsigned int subbyte = subreg_size_offset_from_lsb (1, size, lsb).to_constant ();
          result <<= BITS_PER_UNIT;
          result |= bytes[first_byte + subbyte];
      }
      return mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/ (mtcsRTL,result, imode);
  }

  scalar_float_mode fmode;
  if (mtcs_mode_is_a <scalar_float_mode> (mtcsMode,mode, &fmode)){
      /* We need to build an array of integers in target memory order.
     All integers before the last one have 32 bits; the last one may
     have 32 bits or fewer, depending on whether the mode bitsize
     is divisible by 32.  */
      long el32[MAX_BITSIZE_MODE_ANY_MODE / 32];
      unsigned int num_el32 = CEIL (GET_MODE_BITSIZE (fmode), 32);
      memset (el32, 0, num_el32 * sizeof (long));

      /* The (maximum) number of target bytes per element of el32.  */
      unsigned int bytes_per_el32 = 32 / BITS_PER_UNIT;
      gcc_assert (bytes_per_el32 != 0);

      unsigned int mode_bytes = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,fmode);
      for (unsigned int byte = 0; byte < mode_bytes; ++byte){
          unsigned int index = byte / bytes_per_el32;
          unsigned int subbyte = byte % bytes_per_el32;
          unsigned int int_bytes = MIN (bytes_per_el32,
                        mode_bytes - index * bytes_per_el32);
          /* Always constant because the inputs are.  */
          unsigned int lsb= subreg_size_lsb (1, int_bytes, subbyte).to_constant ();
          el32[index] |= (unsigned long) bytes[first_byte + byte] << lsb;
      }
      REAL_VALUE_TYPE r;
      mtcs_real_real_from_target/*!real_from_target*/ (mtcsReal,&r, el32, fmode);
      return mtcs_rtl_const_double_from_real_value/*!const_double_from_real_value*/(mtcsRTL,r, fmode);
  }

  if (mtcs_mode_is_all_scalar_fixed_point_p/*!ALL_SCALAR_FIXED_POINT_MODE_P*/ (mtcsMode,mode)){
      scalar_mode smode = mtcs_mode_as_a <scalar_mode> (mtcsMode,mode);
      FIXED_VALUE_TYPE f;
      f.data.low = 0;
      f.data.high = 0;
      f.mode = smode;

      unsigned int mode_bytes = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,smode);
      for (unsigned int byte = 0; byte < mode_bytes; ++byte){
          /* Always constant because the inputs are.  */
          unsigned int lsb= subreg_size_lsb (1, mode_bytes, byte).to_constant ();
          unsigned HOST_WIDE_INT unit = bytes[first_byte + byte];
          if (lsb >= HOST_BITS_PER_WIDE_INT)
            f.data.high |= unit << (lsb - HOST_BITS_PER_WIDE_INT);
          else
            f.data.low |= unit << lsb;
      }
      return mtcs_rtl_const_fixed_from_fixed_value/*!CONST_FIXED_FROM_FIXED_VALUE*/(mtcsRTL,f, mode);
  }

  return NULL_RTX;
}


/* This part of simplify_relational_operation is only used when CMP_MODE
   is not in class MODE_CC (i.e. it is a real comparison).

   MODE is the mode of the result, while CMP_MODE specifies in which
   mode the comparison is done in, so it is the mode of the operands.  */
//原型 simplify_context::simplify_relational_operation_1 rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_relational_operation_1 (MtcsSimplifyRtx *self,rtx_code code,machine_mode mode,machine_mode cmp_mode,rtx op0, rtx op1)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  enum rtx_code op0code = GET_CODE (op0);

  if (op1 == const0_rtx && COMPARISON_P (op0)){
      /* If op0 is a comparison, extract the comparison arguments
         from it.  */
      if (code == NE){
          if (GET_MODE (op0) == mode)
            return mtcs_simplify_rtx_simplify_rtx (self,op0);
          else
            return mtcs_simplify_rtx_gen_relational (self,GET_CODE (op0), mode, VOIDmode,XEXP (op0, 0), XEXP (op0, 1));
      }else if (code == EQ){
          enum rtx_code new_code = mtcs_dojump_reversed_comparison_code/*!reversed_comparison_code*/(mtcsDojump,op0, NULL);
          if (new_code != UNKNOWN)
            return mtcs_simplify_rtx_gen_relational (self,new_code, mode, VOIDmode,XEXP (op0, 0), XEXP (op0, 1));
      }
  }

  /* (LTU/GEU (PLUS a C) C), where C is constant, can be simplified to
     (GEU/LTU a -C).  Likewise for (LTU/GEU (PLUS a C) a).  */
  if ((code == LTU || code == GEU) && GET_CODE (op0) == PLUS  && CONST_INT_P (XEXP (op0, 1))
      && (rtx_equal_p (op1, XEXP (op0, 0)) || rtx_equal_p (op1, XEXP (op0, 1)))
      /* (LTU/GEU (PLUS a 0) 0) is not the same as (GEU/LTU a 0). */
      && XEXP (op0, 1) != const0_rtx) {
      rtx new_cmp = mtcs_simplify_rtx_gen_unary (self,NEG, cmp_mode, XEXP (op0, 1), cmp_mode);
      return mtcs_simplify_rtx_gen_relational (self,(code == LTU ? GEU : LTU), mode,cmp_mode, XEXP (op0, 0), new_cmp);
  }

  /* (GTU (PLUS a C) (C - 1)) where C is a non-zero constant can be
     transformed into (LTU a -C).  */
  if (code == GTU && GET_CODE (op0) == PLUS && CONST_INT_P (op1) && CONST_INT_P (XEXP (op0, 1))
          && (UINTVAL (op1) == UINTVAL (XEXP (op0, 1)) - 1) && XEXP (op0, 1) != const0_rtx){
      rtx new_cmp = mtcs_simplify_rtx_gen_unary (self,NEG, cmp_mode, XEXP (op0, 1), cmp_mode);
      return mtcs_simplify_rtx_gen_relational (self,LTU, mode, cmp_mode,XEXP (op0, 0), new_cmp);
  }

  /* Canonicalize (LTU/GEU (PLUS a b) b) as (LTU/GEU (PLUS a b) a).  */
  if ((code == LTU || code == GEU)  && GET_CODE (op0) == PLUS  && rtx_equal_p (op1, XEXP (op0, 1))
      /* Don't recurse "infinitely" for (LTU/GEU (PLUS b b) b).  */
      && !rtx_equal_p (op1, XEXP (op0, 0)))
    return mtcs_simplify_rtx_gen_relational (self,code, mode, cmp_mode, op0,copy_rtx (XEXP (op0, 0)));

  if (op1 == const0_rtx){
      /* Canonicalize (GTU x 0) as (NE x 0).  */
      if (code == GTU)
        return mtcs_simplify_rtx_gen_relational (self,NE, mode, cmp_mode, op0, op1);
      /* Canonicalize (LEU x 0) as (EQ x 0).  */
      if (code == LEU)
        return mtcs_simplify_rtx_gen_relational (self,EQ, mode, cmp_mode, op0, op1);
  }else if (op1 == const1_rtx){
      switch (code){
        case GE:
          /* Canonicalize (GE x 1) as (GT x 0).  */
          return mtcs_simplify_rtx_gen_relational (self,GT, mode, cmp_mode,op0, const0_rtx);
        case GEU:
          /* Canonicalize (GEU x 1) as (NE x 0).  */
          return mtcs_simplify_rtx_gen_relational (self,NE, mode, cmp_mode,op0, const0_rtx);
        case LT:
          /* Canonicalize (LT x 1) as (LE x 0).  */
          return mtcs_simplify_rtx_gen_relational (self,LE, mode, cmp_mode,op0, const0_rtx);
        case LTU:
          /* Canonicalize (LTU x 1) as (EQ x 0).  */
          return mtcs_simplify_rtx_gen_relational (self,EQ, mode, cmp_mode,op0, const0_rtx);
        default:
          break;
      }
  }else if (op1 == constm1_rtx){
      /* Canonicalize (LE x -1) as (LT x 0).  */
      if (code == LE)
        return mtcs_simplify_rtx_gen_relational (self,LT, mode, cmp_mode, op0, const0_rtx);
      /* Canonicalize (GT x -1) as (GE x 0).  */
      if (code == GT)
        return mtcs_simplify_rtx_gen_relational (self,GE, mode, cmp_mode, op0, const0_rtx);
  }

  /* (eq/ne (plus x cst1) cst2) simplifies to (eq/ne x (cst2 - cst1))  */
  if ((code == EQ || code == NE) && (op0code == PLUS || op0code == MINUS)
      && CONSTANT_P (op1) && CONSTANT_P (XEXP (op0, 1))  && (mtcs_mode_is_integral_p/*!INTEGRAL_MODE_P*/ (mtcsMode,cmp_mode)
      || flag_unsafe_math_optimizations)){
      rtx x = XEXP (op0, 0);
      rtx c = XEXP (op0, 1);
      enum rtx_code invcode = op0code == PLUS ? MINUS : PLUS;
      rtx tem = mtcs_simplify_rtx_gen_binary (self,invcode, cmp_mode, op1, c);

      /* Detect an infinite recursive condition, where we oscillate at this
     simplification case between:
        A + B == C  <--->  C - B == A,
     where A, B, and C are all constants with non-simplifiable expressions,
     usually SYMBOL_REFs.  */
      if (GET_CODE (tem) == invcode && CONSTANT_P (x) && rtx_equal_p (c, XEXP (tem, 1)))
          return NULL_RTX;

      return mtcs_simplify_rtx_gen_relational (self,code, mode, cmp_mode, x, tem);
  }

  /* (ne:SI (zero_extract:SI FOO (const_int 1) BAR) (const_int 0))) is
     the same as (zero_extract:SI FOO (const_int 1) BAR).  */
  scalar_int_mode int_mode, int_cmp_mode;
  if (code == NE
      && op1 == const0_rtx
      && mtcs_mode_is_int_mode(mtcsMode,mode, &int_mode)
      && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,cmp_mode, &int_cmp_mode)
      /* ??? Work-around BImode bugs in the ia64 backend.  */
      && int_mode != mtcsMode->modes.M_BImode
      && int_cmp_mode != mtcsMode->modes.M_BImode
      && mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(mtcsRtlanal,op0, int_cmp_mode) == 1
      && STORE_FLAG_VALUE == 1)
    return mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,int_mode) > mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,int_cmp_mode)
       ? mtcs_simplify_rtx_gen_unary (self,ZERO_EXTEND, int_mode, op0, int_cmp_mode)
       : mtcs_simplify_rtx_lowpart_subreg(self,int_mode, op0, int_cmp_mode);

  /* (eq/ne (xor x y) 0) simplifies to (eq/ne x y).  */
  if ((code == EQ || code == NE) && op1 == const0_rtx  && op0code == XOR)
    return mtcs_simplify_rtx_gen_relational (self,code, mode, cmp_mode,XEXP (op0, 0), XEXP (op0, 1));

  /* (eq/ne (xor x y) x) simplifies to (eq/ne y 0).  */
  if ((code == EQ || code == NE) && op0code == XOR  && rtx_equal_p (XEXP (op0, 0), op1) && !side_effects_p (XEXP (op0, 0)))
    return mtcs_simplify_rtx_gen_relational (self,code, mode, cmp_mode, XEXP (op0, 1),CONST0_RTX (mode));

  /* Likewise (eq/ne (xor x y) y) simplifies to (eq/ne x 0).  */
  if ((code == EQ || code == NE) && op0code == XOR  && rtx_equal_p (XEXP (op0, 1), op1) && !side_effects_p (XEXP (op0, 1)))
    return mtcs_simplify_rtx_gen_relational (self,code, mode, cmp_mode, XEXP (op0, 0), CONST0_RTX (mode));

  /* (eq/ne (xor x C1) C2) simplifies to (eq/ne x (C1^C2)).  */
  if ((code == EQ || code == NE) && op0code == XOR
      && CONST_SCALAR_INT_P (op1)  && CONST_SCALAR_INT_P (XEXP (op0, 1)))
    return mtcs_simplify_rtx_gen_relational (self,code, mode, cmp_mode, XEXP (op0, 0),
            mtcs_simplify_rtx_gen_binary (self,XOR, cmp_mode,XEXP (op0, 1), op1));

  /* Simplify eq/ne (and/ior x y) x/y) for targets with a BICS instruction or
     constant folding if x/y is a constant.  */
  if ((code == EQ || code == NE)  && (op0code == AND || op0code == IOR)
      && !side_effects_p (op1)  && op1 != CONST0_RTX (cmp_mode)){
      /* Both (eq/ne (and x y) x) and (eq/ne (ior x y) y) simplify to
     (eq/ne (and (not y) x) 0).  */
      if ((op0code == AND && rtx_equal_p (XEXP (op0, 0), op1))
        || (op0code == IOR && rtx_equal_p (XEXP (op0, 1), op1))){
          rtx not_y = mtcs_simplify_rtx_gen_unary (self,NOT, cmp_mode, XEXP (op0, 1),cmp_mode);
          rtx lhs = mtcs_simplify_rtx_gen_binary (self,AND, cmp_mode, not_y, XEXP (op0, 0));
              return mtcs_simplify_rtx_gen_relational (self,code, mode, cmp_mode, lhs,CONST0_RTX (cmp_mode));
      }

      /* Both (eq/ne (and x y) y) and (eq/ne (ior x y) x) simplify to
     (eq/ne (and (not x) y) 0).  */
      if ((op0code == AND && rtx_equal_p (XEXP (op0, 1), op1))
        || (op0code == IOR && rtx_equal_p (XEXP (op0, 0), op1))){
          rtx not_x = mtcs_simplify_rtx_gen_unary (self,NOT, cmp_mode, XEXP (op0, 0),cmp_mode);
          rtx lhs = mtcs_simplify_rtx_gen_binary (self,AND, cmp_mode, not_x, XEXP (op0, 1));
          return mtcs_simplify_rtx_gen_relational (self,code, mode, cmp_mode, lhs,CONST0_RTX (cmp_mode));
      }
  }

  /* (eq/ne (bswap x) C1) simplifies to (eq/ne x C2) with C2 swapped.  */
  if ((code == EQ || code == NE)  && GET_CODE (op0) == BSWAP  && CONST_SCALAR_INT_P (op1))
    return mtcs_simplify_rtx_gen_relational (self,code, mode, cmp_mode, XEXP (op0, 0),
                    mtcs_simplify_rtx_gen_unary (self,BSWAP, cmp_mode,op1, cmp_mode));

  /* (eq/ne (bswap x) (bswap y)) simplifies to (eq/ne x y).  */
  if ((code == EQ || code == NE) && GET_CODE (op0) == BSWAP && GET_CODE (op1) == BSWAP)
    return mtcs_simplify_rtx_gen_relational (self,code, mode, cmp_mode, XEXP (op0, 0), XEXP (op1, 0));

  if (op0code == POPCOUNT && op1 == const0_rtx)
    switch (code){
      case EQ:
      case LE:
      case LEU:
        /* (eq (popcount x) (const_int 0)) -> (eq x (const_int 0)).  */
        return mtcs_simplify_rtx_gen_relational (self,EQ, mode, GET_MODE (XEXP (op0, 0)), XEXP (op0, 0), const0_rtx);

      case NE:
      case GT:
      case GTU:
        /* (ne (popcount x) (const_int 0)) -> (ne x (const_int 0)).  */
        return mtcs_simplify_rtx_gen_relational (self,NE, mode, GET_MODE (XEXP (op0, 0)),XEXP (op0, 0), const0_rtx);

      default:
          break;
    }

  /* (ne:SI (subreg:QI (ashift:SI x 7) 0) 0) -> (and:SI x 1).  */
  if (code == NE  && op1 == const0_rtx
          && (op0code == TRUNCATE || (mtcs_rtl_partial_subreg_p/*!partial_subreg_p*/(mtcsRTL,op0)
          && mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,op0)))
          && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,mode)
          && STORE_FLAG_VALUE == 1){
      rtx tmp = XEXP (op0, 0);
      if (GET_CODE (tmp) == ASHIFT  && GET_MODE (tmp) == mode  && CONST_INT_P (XEXP (tmp, 1))
         && mtcs_mode_is_int_mode(mtcsMode,GET_MODE (op0), &int_mode) &&
         INTVAL (XEXP (tmp, 1)) == mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,int_mode) - 1)
          return mtcs_simplify_rtx_gen_binary (self,AND, mode, XEXP (tmp, 0), const1_rtx);
  }
  return NULL_RTX;
}

/* Simplify X, an rtx expression.

   Return the simplified expression or NULL if no simplifications
   were possible.

   This is the preferred entry point into the simplification routines;
   however, we still allow passes to call the more specific routines.

   Right now GCC has three (yes, three) major bodies of RTL simplification
   code that need to be unified.

    1. fold_rtx in cse.cc.  This code uses various CSE specific
       information to aid in RTL simplification.

    2. simplify_rtx in combine.cc.  Similar to fold_rtx, except that
       it uses combine specific information to aid in RTL
       simplification.

    3. The routines in this file.


   Long term we want to only have one body of simplification code; to
   get to that state I recommend the following steps:

    1. Pour over fold_rtx & simplify_rtx and move any simplifications
       which are not pass dependent state into these routines.

    2. As code is moved by #1, change fold_rtx & simplify_rtx to
       use this routine whenever possible.

    3. Allow for pass dependent state to be provided to these
       routines and add simplifications based on the pass dependent
       state.  Remove code from cse.cc & combine.cc that becomes
       redundant/dead.

    It will take time, but ultimately the compiler will be easier to
    maintain and improve.  It's totally silly that when we add a
    simplification that it needs to be added to 4 places (3 for RTL
    simplification and 1 for tree simplification.  */
//原型 ssimplify_rtx rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_simplify_rtx (MtcsSimplifyRtx *self,const_rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  const enum rtx_code code = GET_CODE (x);
  const machine_mode mode = GET_MODE (x);

  switch (GET_RTX_CLASS (code)){
    case RTX_UNARY:
      return mtcs_simplify_rtx_unary_operation (self,code, mode,XEXP (x, 0), GET_MODE (XEXP (x, 0)));
    case RTX_COMM_ARITH:
      if (mtcs_rtlanal_swap_commutative_operands_p/*!swap_commutative_operands_p*/(mtcsRtlanal, XEXP (x, 0), XEXP (x, 1)))
          return mtcs_simplify_rtx_gen_binary (self,code, mode, XEXP (x, 1), XEXP (x, 0));

      /* Fall through.  */

    case RTX_BIN_ARITH:
      return mtcs_simplify_rtx_binary_operation (self,code, mode, XEXP (x, 0), XEXP (x, 1));

    case RTX_TERNARY:
    case RTX_BITFIELD_OPS:
      return mtcs_simplify_rtx_ternary_operation (self,code, mode, GET_MODE (XEXP (x, 0)),
                     XEXP (x, 0), XEXP (x, 1),XEXP (x, 2));

    case RTX_COMPARE:
    case RTX_COMM_COMPARE:
      return mtcs_simplify_rtx_relational_operation (self,code, mode,((GET_MODE (XEXP (x, 0))!= VOIDmode)
                                            ? GET_MODE (XEXP (x, 0)) : GET_MODE (XEXP (x, 1))),XEXP (x, 0),XEXP (x, 1));

    case RTX_EXTRA:
      if (code == SUBREG)
          return mtcs_simplify_rtx_subreg (self,mode, SUBREG_REG (x),GET_MODE (SUBREG_REG (x)),SUBREG_BYTE (x));
      break;

    case RTX_OBJ:
      if (code == LO_SUM){
          /* Convert (lo_sum (high FOO) FOO) to FOO.  */
          if (GET_CODE (XEXP (x, 0)) == HIGH  && rtx_equal_p (XEXP (XEXP (x, 0), 0), XEXP (x, 1)))
              return XEXP (x, 1);
      }
      break;

    default:
      break;
  }
  return NULL;
}

/* Try to simplify X given that it appears within operand OP of a
   VEC_MERGE operation whose mask is MASK.  X need not use the same
   vector mode as the VEC_MERGE, but it must have the same number of
   elements.

   Return the simplified X on success, otherwise return NULL_RTX.  */
//原型 simplify_context::simplify_merge_mask rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_merge_mask (MtcsSimplifyRtx *self,rtx x, rtx mask, int op)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  gcc_assert (mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,GET_MODE (x)));
  poly_uint64 nunits = mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/ (mtcsMode,GET_MODE (x));
  if (GET_CODE (x) == VEC_MERGE && rtx_equal_p (XEXP (x, 2), mask)){
      if (side_effects_p (XEXP (x, 1 - op)))
          return NULL_RTX;
      return XEXP (x, op);
  }
  if (UNARY_P (x)  && mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,GET_MODE (XEXP (x, 0)))
      && known_eq (mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/ (mtcsMode,GET_MODE (XEXP (x, 0))), nunits)){
      rtx top0 = mtcs_simplify_rtx_merge_mask (self,XEXP (x, 0), mask, op);
      if (top0)
          return simplify_gen_unary (GET_CODE (x), GET_MODE (x), top0, GET_MODE (XEXP (x, 0)));
  }
  if (BINARY_P (x)
      && mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,GET_MODE (XEXP (x, 0)))
      && known_eq (mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,GET_MODE (XEXP (x, 0))), nunits)
      && mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,GET_MODE (XEXP (x, 1)))
      && known_eq (mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,GET_MODE (XEXP (x, 1))), nunits)){
      rtx top0 = mtcs_simplify_rtx_merge_mask (self,XEXP (x, 0), mask, op);
      rtx top1 = mtcs_simplify_rtx_merge_mask (self,XEXP (x, 1), mask, op);
      if (top0 || top1) {
          if (COMPARISON_P (x))
            return mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(self,GET_CODE (x), GET_MODE (x),
                            GET_MODE (XEXP (x, 0)) != VOIDmode
                            ? GET_MODE (XEXP (x, 0))
                            : GET_MODE (XEXP (x, 1)),
                            top0 ? top0 : XEXP (x, 0),
                            top1 ? top1 : XEXP (x, 1));
          else
            return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(self,GET_CODE (x), GET_MODE (x),
                        top0 ? top0 : XEXP (x, 0),
                        top1 ? top1 : XEXP (x, 1));
      }
  }
  if (GET_RTX_CLASS (GET_CODE (x)) == RTX_TERNARY
      && mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,GET_MODE (XEXP (x, 0)))
      && known_eq (mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,GET_MODE (XEXP (x, 0))), nunits)
      && mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,GET_MODE (XEXP (x, 1)))
      && known_eq (mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,GET_MODE (XEXP (x, 1))), nunits)
      && mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,GET_MODE (XEXP (x, 2)))
      && known_eq (mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,GET_MODE (XEXP (x, 2))), nunits))
  {
      rtx top0 = mtcs_simplify_rtx_merge_mask (self,XEXP (x, 0), mask, op);
      rtx top1 = mtcs_simplify_rtx_merge_mask (self,XEXP (x, 1), mask, op);
      rtx top2 = mtcs_simplify_rtx_merge_mask (self,XEXP (x, 2), mask, op);
      if (top0 || top1 || top2)
        return mtcs_simplify_rtx_gen_ternary (self,GET_CODE (x), GET_MODE (x),
                         GET_MODE (XEXP (x, 0)),
                         top0 ? top0 : XEXP (x, 0),
                         top1 ? top1 : XEXP (x, 1),
                         top2 ? top2 : XEXP (x, 2));
  }
  return NULL_RTX;
}


/* Recognize expressions of the form (X CMP 0) ? VAL : OP (X)
   where OP is CLZ or CTZ and VAL is the value from CLZ_DEFINED_VALUE_AT_ZERO
   or CTZ_DEFINED_VALUE_AT_ZERO respectively and return OP (X) if the expression
   can be simplified to that or NULL_RTX if not.
   Assume X is compared against zero with CMP_CODE and the true
   arm is TRUE_VAL and the false arm is FALSE_VAL.  */
//原型 simplify_context::simplify_cond_clz_ctz rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_cond_clz_ctz (MtcsSimplifyRtx *self,rtx x, rtx_code cmp_code,rtx true_val, rtx false_val)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  if (cmp_code != EQ && cmp_code != NE)
    return NULL_RTX;

  /* Result on X == 0 and X !=0 respectively.  */
  rtx on_zero, on_nonzero;
  if (cmp_code == EQ){
      on_zero = true_val;
      on_nonzero = false_val;
  }else{
      on_zero = false_val;
      on_nonzero = true_val;
  }

  rtx_code op_code = GET_CODE (on_nonzero);
  if ((op_code != CLZ && op_code != CTZ)  || !rtx_equal_p (XEXP (on_nonzero, 0), x) || !CONST_INT_P (on_zero))
    return NULL_RTX;

  HOST_WIDE_INT op_val;
  scalar_int_mode mode ATTRIBUTE_UNUSED = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,GET_MODE (XEXP (on_nonzero, 0)));
  if (((op_code == CLZ &&  mtcs_mode_clz_defined_value_at_zero/*!CLZ_DEFINED_VALUE_AT_ZERO (mode, op_val)*/(mtcsMode,mode, (int*)&op_val))
       || (op_code == CTZ &&mtcs_mode_ctz_defined_value_at_zero/*!CTZ_DEFINED_VALUE_AT_ZERO (mode, op_val)*/(mtcsMode,mode,(int*)&op_val)))
      && op_val == INTVAL (on_zero))
    return on_nonzero;

  return NULL_RTX;
}

/* Test whether VAL is equal to the most significant bit of mode MODE
   (after masking with the mode mask of MODE).  Returns false if the
   precision of MODE is too large to handle.  */
//原型 val_signbit_p rtl.h simplify-rtx.cc
bool mtcs_simplify_rtx_val_signbit_p (MtcsSimplifyRtx *self,machine_mode mode, unsigned HOST_WIDE_INT val)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  unsigned int width;
  scalar_int_mode int_mode;

  if (!mtcs_mode_is_int_mode (mtcsMode,mode, &int_mode))
    return false;

  width = mtcs_mode_get_precision(mtcsMode,int_mode);
  if (width == 0 || width > HOST_BITS_PER_WIDE_INT)
    return false;

  val &= mtcs_mode_get_mask/*!GET_MODE_MASK*/ (mtcsMode,int_mode);
  return val == (HOST_WIDE_INT_1U << (width - 1));
}

/* Return TRUE if a rotate in mode MODE with a constant count in OP1
   should be reversed.

   If the rotate should not be reversed, return FALSE.

   LEFT indicates if this is a rotate left or a rotate right.  */
//原型 reverse_rotate_by_imm_p rtl.h simplify-rtx.cc
bool mtcs_simplify_rtx_reverse_rotate_by_imm_p (MtcsSimplifyRtx *self,machine_mode mode, unsigned int left, rtx op1)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

  if (!CONST_INT_P (op1))
    return false;
  /* Some targets may only be able to rotate by a constant
     in one direction.  So we need to query the optab interface
     to see what is possible.  */
  optab binoptab = left ? rotl_optab : rotr_optab;
  optab re_binoptab = left ? rotr_optab : rotl_optab;
  enum insn_code icode = mtcs_opinit_optab_handler (mtcsOpinit,binoptab, mode);
  enum insn_code re_icode = mtcs_opinit_optab_handler (mtcsOpinit,re_binoptab, mode);
  /* If the target can not support the reversed optab, then there
     is nothing to do.  */
  if (re_icode == CODE_FOR_nothing)
    return false;
  /* If the target does not support the requested rotate-by-immediate,
     then we want to try reversing the rotate.  We also want to try
     reversing to minimize the count.  */
  if ((icode == CODE_FOR_nothing)
      || (!mtcs_optabs_insn_operand_matches (mtcsOptabs,icode, 2, op1))
      || (IN_RANGE (INTVAL (op1),
            mtcs_mode_get_unit_precision (mtcsMode,mode) / 2 + left,
            mtcs_mode_get_unit_precision (mtcsMode,mode) - 1)))
    return (mtcs_optabs_insn_operand_matches (mtcsOptabs,re_icode, 2, op1));
  return false;
}

/* If FN is NULL, replace all occurrences of OLD_RTX in X with copy_rtx (DATA)
   and simplify the result.  If FN is non-NULL, call this callback on each
   X, if it returns non-NULL, replace X with its return value and simplify the
   result.  */
//原型 simplify_replace_fn_rtx rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_simplify_replace_fn_rtx (MtcsSimplifyRtx *self,rtx x, const_rtx old_rtx,
          rtx (*fn) (rtx, const_rtx, void *), void *data)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   enum rtx_code code = GET_CODE (x);
   machine_mode mode = GET_MODE (x);
   machine_mode op_mode;
   const char *fmt;
   rtx op0, op1, op2, newx, op;
   rtvec vec, newvec;
   int i, j;

   if (UNLIKELY (fn != NULL)){
      newx = fn (x, old_rtx, data);
      if (newx)
         return newx;
   }else if (rtx_equal_p (x, old_rtx))
      return copy_rtx ((rtx) data);

   switch (GET_RTX_CLASS (code)){
      case RTX_UNARY:
         op0 = XEXP (x, 0);
         op_mode = GET_MODE (op0);
         op0 = mtcs_simplify_rtx_simplify_replace_fn_rtx/*!simplify_replace_fn_rtx*/(self,op0, old_rtx, fn, data);
         if (op0 == XEXP (x, 0))
            return x;
         return mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(self,code, mode, op0, op_mode);

      case RTX_BIN_ARITH:
      case RTX_COMM_ARITH:
         op0 = mtcs_simplify_rtx_simplify_replace_fn_rtx/*!simplify_replace_fn_rtx*/(self,XEXP (x, 0), old_rtx, fn, data);
         op1 = mtcs_simplify_rtx_simplify_replace_fn_rtx/*!simplify_replace_fn_rtx*/(self,XEXP (x, 1), old_rtx, fn, data);
         if (op0 == XEXP (x, 0) && op1 == XEXP (x, 1))
            return x;
         return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(self,code, mode, op0, op1);

      case RTX_COMPARE:
      case RTX_COMM_COMPARE:
         op0 = XEXP (x, 0);
         op1 = XEXP (x, 1);
         op_mode = GET_MODE (op0) != VOIDmode ? GET_MODE (op0) : GET_MODE (op1);
         op0 = mtcs_simplify_rtx_simplify_replace_fn_rtx/*!simplify_replace_fn_rtx*/(self,op0, old_rtx, fn, data);
         op1 = mtcs_simplify_rtx_simplify_replace_fn_rtx/*!simplify_replace_fn_rtx*/(self,op1, old_rtx, fn, data);
         if (op0 == XEXP (x, 0) && op1 == XEXP (x, 1))
            return x;
         return mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(self,code, mode, op_mode, op0, op1);

      case RTX_TERNARY:
      case RTX_BITFIELD_OPS:
         op0 = XEXP (x, 0);
         op_mode = GET_MODE (op0);
         op0 = mtcs_simplify_rtx_simplify_replace_fn_rtx/*!simplify_replace_fn_rtx*/(self,op0, old_rtx, fn, data);
         op1 = mtcs_simplify_rtx_simplify_replace_fn_rtx/*!simplify_replace_fn_rtx*/(self,XEXP (x, 1), old_rtx, fn, data);
         op2 = mtcs_simplify_rtx_simplify_replace_fn_rtx/*!simplify_replace_fn_rtx*/(self,XEXP (x, 2), old_rtx, fn, data);
         if (op0 == XEXP (x, 0) && op1 == XEXP (x, 1) && op2 == XEXP (x, 2))
            return x;
         if (op_mode == VOIDmode)
            op_mode = GET_MODE (op0);
         return mtcs_simplify_rtx_gen_ternary/*!simplify_gen_ternary*/(self,code, mode, op_mode, op0, op1, op2);

      case RTX_EXTRA:
         if (code == SUBREG){
            op0 = mtcs_simplify_rtx_simplify_replace_fn_rtx/*!simplify_replace_fn_rtx*/(self,SUBREG_REG (x), old_rtx, fn, data);
            if (op0 == SUBREG_REG (x))
               return x;
            op0 = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(self,GET_MODE (x), op0,
                                                GET_MODE (SUBREG_REG (x)), SUBREG_BYTE (x));
            return op0 ? op0 : x;
         }
         break;

      case RTX_OBJ:
         if (code == MEM){
            op0 = mtcs_simplify_rtx_simplify_replace_fn_rtx/*!simplify_replace_fn_rtx*/(self,XEXP (x, 0), old_rtx, fn, data);
            if (op0 == XEXP (x, 0))
               return x;
            return mtcs_rtl_replace_equiv_address_nv/*!replace_equiv_address_nv*/(mtcsRTL,x, op0);
         }else if (code == LO_SUM){
            op0 = mtcs_simplify_rtx_simplify_replace_fn_rtx/*!simplify_replace_fn_rtx*/(self,XEXP (x, 0), old_rtx, fn, data);
            op1 = mtcs_simplify_rtx_simplify_replace_fn_rtx/*!simplify_replace_fn_rtx*/(self,XEXP (x, 1), old_rtx, fn, data);

            /* (lo_sum (high x) y) -> y where x and y have the same base.  */
            if (GET_CODE (op0) == HIGH){
               rtx base0, base1, offset0, offset1;
               split_const (XEXP (op0, 0), &base0, &offset0);
               split_const (op1, &base1, &offset1);
               if (rtx_equal_p (base0, base1))
                  return op1;
            }

            if (op0 == XEXP (x, 0) && op1 == XEXP (x, 1))
               return x;
            return gen_rtx_LO_SUM (mode, op0, op1);
         }
         break;

      default:
         break;
   }

   newx = x;
   fmt = GET_RTX_FORMAT (code);
   for (i = 0; fmt[i]; i++)
      switch (fmt[i]){
         case 'E':
            vec = XVEC (x, i);
            newvec = XVEC (newx, i);
            for (j = 0; j < GET_NUM_ELEM (vec); j++){
               op = mtcs_simplify_rtx_simplify_replace_fn_rtx/*!simplify_replace_fn_rtx*/(self,RTVEC_ELT (vec, j),
                              old_rtx, fn, data);
               if (op != RTVEC_ELT (vec, j)){
                  if (newvec == vec){
                     newvec = shallow_copy_rtvec (vec);
                     if (x == newx)
                        newx = shallow_copy_rtx (x);
                      XVEC (newx, i) = newvec;
                   }
                   RTVEC_ELT (newvec, j) = op;
               }
            }
            break;

         case 'e':
            if (XEXP (x, i)){
               op = mtcs_simplify_rtx_simplify_replace_fn_rtx/*!simplify_replace_fn_rtx*/(self,XEXP (x, i), old_rtx, fn, data);
               if (op != XEXP (x, i)) {
                  if (x == newx)
                  newx = shallow_copy_rtx (x);
                  XEXP (newx, i) = op;
               }
            }
            break;
      }
   return newx;
}

/* Replace all occurrences of OLD_RTX in X with NEW_RTX and try to simplify the
   resulting RTX.  Return a new RTX which is as simplified as possible.  */
//原型 simplify_replace_rtx rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_simplify_replace_rtx (MtcsSimplifyRtx *self,rtx x, const_rtx old_rtx, rtx new_rtx)
{
  return mtcs_simplify_rtx_simplify_replace_fn_rtx/*!simplify_replace_fn_rtx*/(self,x, old_rtx, 0, new_rtx);
}

/* Generate RTX to select element at INDEX out of vector OP.  */
//原型 simplify_context::simplify_gen_vec_select rtl.h simplify-rtx.cc
rtx mtcs_simplify_rtx_simplify_gen_vec_select (MtcsSimplifyRtx *self,rtx op, unsigned int index)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   gcc_assert (mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,GET_MODE (op)));

   scalar_mode imode = mtcs_mode_get_inner/*!GET_MODE_INNER*/ (mtcsMode,GET_MODE (op));

   if (known_eq (index * mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,imode),
   mtcs_mode_subreg_lowpart_offset/*!subreg_lowpart_offset*/(mtcsMode,imode, GET_MODE (op)))){
      rtx res = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(self,imode, op, GET_MODE (op));
      if (res)
         return res;
   }

   rtx tmp = gen_rtx_PARALLEL (VOIDmode, gen_rtvec (1, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,index)));
   return gen_rtx_VEC_SELECT (imode, op, tmp);
}

/* Test whether the most significant bit of mode MODE is set in VAL.
   Returns false if the precision of MODE is too large to handle.  */
//原型 val_signbit_known_set_p rtl.h simplify-rtx.cc
bool mtcs_simplify_rtx_val_signbit_known_set_p (MtcsSimplifyRtx *self,machine_mode mode, unsigned HOST_WIDE_INT val)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   unsigned int width;

   scalar_int_mode int_mode;
   if (!mtcs_mode_is_int_mode/*!is_int_mode*/(mtcsMode,mode, &int_mode))
      return false;

   width = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,int_mode);
   if (width == 0 || width > HOST_BITS_PER_WIDE_INT)
      return false;

   val &= HOST_WIDE_INT_1U << (width - 1);
   return val != 0;
}

MtcsSimplifyRtx *mtcs_simplify_rtx_new(MtcsMode *mtcsMode)
{
    MtcsSimplifyRtx *self = n_slice_alloc0 (sizeof(MtcsSimplifyRtx));
    mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
    mtcsSimplifyRtxInit(self);
    return self;
}
