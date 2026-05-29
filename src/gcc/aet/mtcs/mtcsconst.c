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
 * base on fold-const.cc
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
#include "tree-eh.h"

#include "cfganal.h"
#include "cfgcleanup.h"
#include "alias.h"
#include "rtlhooks-def.h"
#include "tree-pass.h"
#include "dbgcnt.h"
#include "rtlanal.h"
#include "stor-layout.h"
#include "generic-match.h"
#include "tree-vector-builder.h"
#include "langhooks.h"

#include "aet/aetprinttree.h"
#include "mtcsconst.h"
#include "mtcstarget.h"

#ifndef LOGICAL_OP_NON_SHORT_CIRCUIT
#define LOGICAL_OP_NON_SHORT_CIRCUIT \
  (BRANCH_COST (optimize_function_for_speed_p (cfun), \
      false) >= 2)
#endif
//原型 fold_convert_const fold-const.cc
static tree fold_convert_const (MtcsConst *self,enum tree_code code, tree type, tree arg1);
//原型 fold_negate_expr fold-const.cc
static tree fold_negate_expr (MtcsConst *self,location_t loc, tree t);
//原型 extract_muldiv fold-const.cc
static tree extract_muldiv (MtcsConst *self,tree t, tree c, enum tree_code code, tree wide_type,
      bool *strict_overflow_p);

static void mtcsConstInit(MtcsConst *self)
{

}

/* Return true if binary operation OP distributes over addition in operand
   OPNO, with the other operand being held constant.  OPNO counts from 1.  */
//原型 distributes_over_addition_p fold-const.cc

static bool distributes_over_addition_p (tree_code op, int opno)
{
   switch (op){
      case PLUS_EXPR:
      case MINUS_EXPR:
      case MULT_EXPR:
         return true;

      case LSHIFT_EXPR:
         return opno == 1;

      default:
         return false;
   }
}

/* Return false if expr can be assumed not to be an lvalue, true
   otherwise.  */
//原型 maybe_lvalue_p fold-const.cc
static bool maybe_lvalue_p (const_tree x)
{
   /* We only need to wrap lvalue tree codes.  */
   switch (TREE_CODE (x)){
      case VAR_DECL:
      case PARM_DECL:
      case RESULT_DECL:
      case LABEL_DECL:
      case FUNCTION_DECL:
      case SSA_NAME:
      case COMPOUND_LITERAL_EXPR:

      case COMPONENT_REF:
      case MEM_REF:
      case INDIRECT_REF:
      case ARRAY_REF:
      case ARRAY_RANGE_REF:
      case BIT_FIELD_REF:
      case OBJ_TYPE_REF:

      case REALPART_EXPR:
      case IMAGPART_EXPR:
      case PREINCREMENT_EXPR:
      case PREDECREMENT_EXPR:
      case SAVE_EXPR:
      case TRY_CATCH_EXPR:
      case WITH_CLEANUP_EXPR:
      case COMPOUND_EXPR:
      case MODIFY_EXPR:
      case TARGET_EXPR:
      case COND_EXPR:
      case BIND_EXPR:
      case VIEW_CONVERT_EXPR:
         break;

      default:
         /* Assume the worst for front-end tree codes.  */
         if ((int)TREE_CODE (x) >= NUM_TREE_CODES)
            break;
         return false;
   }

   return true;
}


/* Determine whether an expression T can be cheaply negated using
   the function negate_expr without introducing undefined overflow.  */
//原型 negate_expr_p fold-const.cc
static bool negate_expr_p (MtcsConst *self,tree t)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFixed *mtcsFixed=mtcs_target_get_fixed(mtcsTarget);

   tree type;
   if (t == 0)
      return false;

   type = TREE_TYPE (t);

   STRIP_SIGN_NOPS (t);
   switch (TREE_CODE (t)){
      case INTEGER_CST:
         if (INTEGRAL_TYPE_P (type) && TYPE_UNSIGNED (type))
            return true;

         /* Check that -CST will not overflow type.  */
         return may_negate_without_overflow_p (t);
      case BIT_NOT_EXPR:
         return (INTEGRAL_TYPE_P (type) && TYPE_OVERFLOW_WRAPS (type));

      case FIXED_CST:
         return true;

      case NEGATE_EXPR:
         return !TYPE_OVERFLOW_SANITIZED (type);

      case REAL_CST:
         /* We want to canonicalize to positive real constants.  Pretend
         that only negative ones can be easily negated.  */
         return REAL_VALUE_NEGATIVE (TREE_REAL_CST (t));

      case COMPLEX_CST:
         return negate_expr_p(self,TREE_REALPART (t)) && negate_expr_p(self,TREE_IMAGPART (t));

      case VECTOR_CST:
      {
         if (FLOAT_TYPE_P (TREE_TYPE (type)) || TYPE_OVERFLOW_WRAPS (type))
            return true;

         /* Steps don't prevent negation.  */
         unsigned int count = vector_cst_encoded_nelts (t);
         for (unsigned int i = 0; i < count; ++i)
            if (!negate_expr_p(self,VECTOR_CST_ENCODED_ELT (t, i)))
               return false;

         return true;
      }

      case COMPLEX_EXPR:
         return negate_expr_p(self,TREE_OPERAND (t, 0)) && negate_expr_p(self,TREE_OPERAND (t, 1));

      case CONJ_EXPR:
         return negate_expr_p(self,TREE_OPERAND (t, 0));

      case PLUS_EXPR:
         if (mtcs_mode_honor_sign_dependent_rounding/*!HONOR_SIGN_DEPENDENT_ROUNDING*/(mtcsMode,type)
               || HONOR_SIGNED_ZEROS (type) || (ANY_INTEGRAL_TYPE_P (type)  && ! TYPE_OVERFLOW_WRAPS (type)))
            return false;
         /* -(A + B) -> (-B) - A.  */
         if (negate_expr_p(self,TREE_OPERAND (t, 1)))
            return true;
         /* -(A + B) -> (-A) - B.  */
         return negate_expr_p(self,TREE_OPERAND (t, 0));

      case MINUS_EXPR:
         /* We can't turn -(A-B) into B-A when we honor signed zeros.  */
         return !mtcs_mode_honor_sign_dependent_rounding/*!HONOR_SIGN_DEPENDENT_ROUNDING*/(mtcsMode,type)
               && !HONOR_SIGNED_ZEROS (type) && (! ANY_INTEGRAL_TYPE_P (type) || TYPE_OVERFLOW_WRAPS (type));

      case MULT_EXPR:
         if (TYPE_UNSIGNED (type))
            break;
         /* INT_MIN/n * n doesn't overflow while negating one operand it does
         if n is a (negative) power of two.  */
         if (INTEGRAL_TYPE_P (TREE_TYPE (t))
         && ! TYPE_OVERFLOW_WRAPS (TREE_TYPE (t))
         && ! ((TREE_CODE (TREE_OPERAND (t, 0)) == INTEGER_CST
         && (wi::popcount
         (wi::abs (wi::to_wide (TREE_OPERAND (t, 0))))) != 1)
         || (TREE_CODE (TREE_OPERAND (t, 1)) == INTEGER_CST
         && (wi::popcount(wi::abs (wi::to_wide (TREE_OPERAND (t, 1))))) != 1)))
            break;

      /* Fall through.  */

      case RDIV_EXPR:
         if (! mtcs_mode_honor_sign_dependent_rounding/*!HONOR_SIGN_DEPENDENT_ROUNDING*/(mtcsMode,t))
            return negate_expr_p(self,TREE_OPERAND (t, 1)) || negate_expr_p(self,TREE_OPERAND (t, 0));
         break;

      case TRUNC_DIV_EXPR:
      case ROUND_DIV_EXPR:
      case EXACT_DIV_EXPR:
         if (TYPE_UNSIGNED (type))
            break;
         /* In general we can't negate A in A / B, because if A is INT_MIN and
         B is not 1 we change the sign of the result.  */
         if (TREE_CODE (TREE_OPERAND (t, 0)) == INTEGER_CST
         && negate_expr_p(self,TREE_OPERAND (t, 0)))
            return true;
         /* In general we can't negate B in A / B, because if A is INT_MIN and
         B is 1, we may turn this into INT_MIN / -1 which is undefined
         and actually traps on some architectures.  */
         if (! ANY_INTEGRAL_TYPE_P (TREE_TYPE (t))
         || TYPE_OVERFLOW_WRAPS (TREE_TYPE (t))
         || (TREE_CODE (TREE_OPERAND (t, 1)) == INTEGER_CST
         && ! integer_onep (TREE_OPERAND (t, 1))))
            return negate_expr_p(self,TREE_OPERAND (t, 1));
         break;

      case NOP_EXPR:
         /* Negate -((double)float) as (double)(-float).  */
         if (SCALAR_FLOAT_TYPE_P (type)){
            tree tem = strip_float_extensions (t);
            if (tem != t)
               return negate_expr_p(self,tem);
         }
         break;

      case CALL_EXPR:
         /* Negate -f(x) as f(-x).  */
         if (negate_mathfn_p (get_call_combined_fn (t)))
            return negate_expr_p(self,CALL_EXPR_ARG (t, 0));
         break;

      case RSHIFT_EXPR:
         /* Optimize -((int)x >> 31) into (unsigned)x >> 31 for int.  */
         if (TREE_CODE (TREE_OPERAND (t, 1)) == INTEGER_CST){
            tree op1 = TREE_OPERAND (t, 1);
            if (wi::to_wide (op1) == element_precision (type) - 1)
               return true;
         }
         break;

      default:
         break;
   }
   return false;
}

/* Like fold_negate_expr, but return a NEGATE_EXPR tree, if T cannot be
   negated in a simpler way.  Also allow for T to be NULL_TREE, in which case
   return NULL_TREE. */
//原型 negate_expr fold-const.cc
static tree negate_expr (MtcsConst *self,tree t)
{
   tree type, tem;
   location_t loc;

   if (t == NULL_TREE)
      return NULL_TREE;

   loc = EXPR_LOCATION (t);
   type = TREE_TYPE (t);
   STRIP_SIGN_NOPS (t);

   tem = fold_negate_expr(self,loc, t);
   if (!tem)
      tem = build1_loc (loc, NEGATE_EXPR, TREE_TYPE (t), t);
   return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, tem);
}

/* A subroutine of fold_convert_const handling conversions a REAL_CST
   to a fixed-point type.  */
//原型 fold_convert_const_fixed_from_real fold-const.cc
static tree fold_convert_const_fixed_from_real (MtcsConst *self,tree type, const_tree arg1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFixed *mtcsFixed=mtcs_target_get_fixed(mtcsTarget);

   FIXED_VALUE_TYPE value;
   tree t;
   bool overflow_p;

   overflow_p = mtcs_fixed_fixed_convert_from_real/*!fixed_convert_from_real*/(mtcsFixed,
         &value, mtcs_mode_scalar_type_mode/*!SCALAR_TYPE_MODE*/(mtcsMode,type),&TREE_REAL_CST (arg1),TYPE_SATURATING (type));
   t = build_fixed (type, value);

   /* Propagate overflow flags.  */
   if (overflow_p | TREE_OVERFLOW (arg1))
      TREE_OVERFLOW (t) = 1;
   return t;
}

/* A subroutine of fold_convert_const handling conversions an INTEGER_CST
   to a fixed-point type.  */
//原型 fold_convert_const_fixed_from_int fold-const.cc
static tree fold_convert_const_fixed_from_int (MtcsConst *self,tree type, const_tree arg1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFixed *mtcsFixed=mtcs_target_get_fixed(mtcsTarget);

   FIXED_VALUE_TYPE value;
   tree t;
   bool overflow_p;
   double_int di;

   gcc_assert (TREE_INT_CST_NUNITS (arg1) <= 2);

   di.low = TREE_INT_CST_ELT (arg1, 0);
   if (TREE_INT_CST_NUNITS (arg1) == 1)
      di.high = (HOST_WIDE_INT) di.low < 0 ? HOST_WIDE_INT_M1 : 0;
   else
      di.high = TREE_INT_CST_ELT (arg1, 1);

   overflow_p = mtcs_fixed_fixed_convert_from_int/*!fixed_convert_from_int*/(mtcsFixed,&value,
         mtcs_mode_scalar_type_mode/*!SCALAR_TYPE_MODE*/(mtcsMode,type), di,
       TYPE_UNSIGNED (TREE_TYPE (arg1)),
       TYPE_SATURATING (type));
   t = build_fixed (type, value);

   /* Propagate overflow flags.  */
   if (overflow_p | TREE_OVERFLOW (arg1))
      TREE_OVERFLOW (t) = 1;
   return t;
}

/* A subroutine of fold_convert_const handling conversions a REAL_CST
   to an integer type.  */
//原型 fold_convert_const_int_from_real fold-const.cc

static tree fold_convert_const_int_from_real (MtcsConst *self,enum tree_code code, tree type, const_tree arg1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   bool overflow = false;
   tree t;

   /* The following code implements the floating point to integer
   conversion rules required by the Java Language Specification,
   that IEEE NaNs are mapped to zero and values that overflow
   the target precision saturate, i.e. values greater than
   INT_MAX are mapped to INT_MAX, and values less than INT_MIN
   are mapped to INT_MIN.  These semantics are allowed by the
   C and C++ standards that simply state that the behavior of
   FP-to-integer conversion is unspecified upon overflow.  */

   wide_int val;
   REAL_VALUE_TYPE r;
   REAL_VALUE_TYPE x = TREE_REAL_CST (arg1);

   switch (code){
      case FIX_TRUNC_EXPR:
         mtcs_real_real_trunc/*!real_trunc*/(mtcsReal,&r, VOIDmode, &x);
         break;

      default:
         gcc_unreachable ();
   }

   /* If R is NaN, return zero and show we have an overflow.  */
   if (REAL_VALUE_ISNAN (r)){
      overflow = true;
      val = wi::zero (TYPE_PRECISION (type));
   }

   /* See if R is less than the lower bound or greater than the
   upper bound.  */

   if (! overflow){
      tree lt = TYPE_MIN_VALUE (type);
      REAL_VALUE_TYPE l = mtcs_real_real_value_from_int_cst/*!real_value_from_int_cst*/(mtcsReal,NULL_TREE, lt);
      if (real_less (&r, &l)){
         overflow = true;
         val = wi::to_wide (lt);
      }
   }

   if (! overflow) {
      tree ut = TYPE_MAX_VALUE (type);
      if (ut){
         REAL_VALUE_TYPE u = mtcs_real_real_value_from_int_cst/*!real_value_from_int_cst*/(mtcsReal,NULL_TREE, ut);
         if (real_less (&u, &r)){
            overflow = true;
            val = wi::to_wide (ut);
         }
      }
   }

   if (! overflow)
      val = real_to_integer (&r, &overflow, TYPE_PRECISION (type));

   t = mtcs_tree_force_fit_type/*!force_fit_type*/(mtcsTree,type, val, -1, overflow | TREE_OVERFLOW (arg1));
   return t;
}


/* A subroutine of fold_convert_const handling conversions of a
   FIXED_CST to an integer type.  */
//原型 fold_convert_const_int_from_fixed fold-const.cc
static tree fold_convert_const_int_from_fixed (MtcsConst *self,tree type, const_tree arg1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree t;
   double_int temp, temp_trunc;
   scalar_mode mode;

   /* Right shift FIXED_CST to temp by fbit.  */
   temp = TREE_FIXED_CST (arg1).data;
   mode = TREE_FIXED_CST (arg1).mode;
   if (mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,mode) < HOST_BITS_PER_DOUBLE_INT){
      temp = temp.rshift (mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,mode),HOST_BITS_PER_DOUBLE_INT,
            mtcs_mode_is_signed_fixed_point_p/*!SIGNED_FIXED_POINT_MODE_P*/(mtcsMode,mode));

      /* Left shift temp to temp_trunc by fbit.  */
      temp_trunc = temp.lshift (mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,mode),HOST_BITS_PER_DOUBLE_INT,
            mtcs_mode_is_signed_fixed_point_p/*!SIGNED_FIXED_POINT_MODE_P*/(mtcsMode,mode));
   }else{
      temp = double_int_zero;
      temp_trunc = double_int_zero;
   }

   /* If FIXED_CST is negative, we need to round the value toward 0.
   By checking if the fractional bits are not zero to add 1 to temp.  */
   if (mtcs_mode_is_signed_fixed_point_p/*!SIGNED_FIXED_POINT_MODE_P*/(mtcsMode,mode)
   && temp_trunc.is_negative () && TREE_FIXED_CST (arg1).data != temp_trunc)
      temp += double_int_one;

   /* Given a fixed-point constant, make new constant with new type,
   appropriately sign-extended or truncated.  */
   t = mtcs_tree_force_fit_type/*!force_fit_type*/(mtcsTree,type, temp, -1,
         (temp.is_negative () && (TYPE_UNSIGNED (type) < TYPE_UNSIGNED (TREE_TYPE (arg1)))) | TREE_OVERFLOW (arg1));

   return t;
}

/* A subroutine of fold_convert_const handling conversions a REAL_CST
   to another floating point type.  */
//原型 fold_convert_const_real_from_real fold-const.cc
static tree fold_convert_const_real_from_real (MtcsConst *self,tree type, const_tree arg1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   REAL_VALUE_TYPE value;
   tree t;

   /* If the underlying modes are the same, simply treat it as
   copy and rebuild with TREE_REAL_CST information and the
   given type.  */
   if (TYPE_MODE (type) == TYPE_MODE (TREE_TYPE (arg1))){
      t = mtcs_tree_build_real/*!build_real*/(mtcsTree,type, TREE_REAL_CST (arg1));
      return t;
   }

   /* Don't perform the operation if flag_signaling_nans is on
   and the operand is a signaling NaN.  */
   if (mtcs_mode_honor_snans/*!HONOR_SNANS*/(mtcsMode,arg1) && REAL_VALUE_ISSIGNALING_NAN (TREE_REAL_CST (arg1)))
      return NULL_TREE;

   /* With flag_rounding_math we should respect the current rounding mode
   unless the conversion is exact.  */
   if (mtcs_mode_honor_sign_dependent_rounding/*!HONOR_SIGN_DEPENDENT_ROUNDING*/(mtcsMode,arg1)
         && !mtcs_real_exact_real_truncate/*!exact_real_truncate*/(mtcsReal,TYPE_MODE (type), &TREE_REAL_CST (arg1)))
      return NULL_TREE;

   mtcs_real_real_convert/*!real_convert*/(mtcsReal,&value, TYPE_MODE (type), &TREE_REAL_CST (arg1));
   t =mtcs_tree_build_real/*!build_real*/(mtcsTree,type, value);

   /* If converting an infinity or NAN to a representation that doesn't
   have one, set the overflow bit so that we can produce some kind of
   error message at the appropriate point if necessary.  It's not the
   most user-friendly message, but it's better than nothing.  */
   if (REAL_VALUE_ISINF (TREE_REAL_CST (arg1))
         && !mtcs_mode_has_infinities/*!MODE_HAS_INFINITIES*/(mtcsMode,TYPE_MODE (type)))
      TREE_OVERFLOW (t) = 1;
   else if (REAL_VALUE_ISNAN (TREE_REAL_CST (arg1)) && !mtcs_mode_has_nans/*!MODE_HAS_NANS*/(mtcsMode,TYPE_MODE (type)))
      TREE_OVERFLOW (t) = 1;
   /* Regular overflow, conversion produced an infinity in a mode that
   can't represent them.  */
   else if (!mtcs_mode_has_infinities/*!MODE_HAS_INFINITIES*/(mtcsMode,TYPE_MODE (type))
         && REAL_VALUE_ISINF (value) && !REAL_VALUE_ISINF (TREE_REAL_CST (arg1)))
      TREE_OVERFLOW (t) = 1;
   else
      TREE_OVERFLOW (t) = TREE_OVERFLOW (arg1);
   return t;
}

/* A subroutine of fold_convert_const handling conversions a FIXED_CST
   to a floating point type.  */
//原型 fold_convert_const_real_from_fixed fold-const.cc
static tree fold_convert_const_real_from_fixed (MtcsConst *self,tree type, const_tree arg1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFixed *mtcsFixed=mtcs_target_get_fixed(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   REAL_VALUE_TYPE value;
   tree t;

   mtcs_fixed_real_convert_from_fixed/*!real_convert_from_fixed*/(mtcsFixed,&value,
         mtcs_mode_scalar_float_type_mode/*!SCALAR_FLOAT_TYPE_MODE*/(mtcsMode,type),&TREE_FIXED_CST (arg1));
   t = mtcs_tree_build_real/*!build_real*/(mtcsTree,type, value);

   TREE_OVERFLOW (t) = TREE_OVERFLOW (arg1);
   return t;
}

/* A subroutine of fold_convert_const handling conversions a FIXED_CST
   to another fixed-point type.  */
//原型 fold_convert_const_fixed_from_fixed fold-const.cc
static tree fold_convert_const_fixed_from_fixed (MtcsConst *self,tree type, const_tree arg1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFixed *mtcsFixed=mtcs_target_get_fixed(mtcsTarget);

   FIXED_VALUE_TYPE value;
   tree t;
   bool overflow_p;

   overflow_p = mtcs_fixed_fixed_convert/*!fixed_convert*/(mtcsFixed,&value,
         mtcs_mode_scalar_type_mode/*!SCALAR_TYPE_MODE*/(mtcsMode,type),
         &TREE_FIXED_CST (arg1), TYPE_SATURATING (type));
   t = build_fixed (type, value);

   /* Propagate overflow flags.  */
   if (overflow_p | TREE_OVERFLOW (arg1))
      TREE_OVERFLOW (t) = 1;
   return t;
}


/* Like native_encode_vector, but only encode the first COUNT elements.
   The other arguments are as for native_encode_vector.  */
//原型 fold_view_convert_vector_encoding fold-const.cc
static int native_encode_vector_part (MtcsConst *self,const_tree expr, unsigned char *ptr, int len,
            int off, unsigned HOST_WIDE_INT count)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   tree itype = TREE_TYPE (TREE_TYPE (expr));
   if (VECTOR_BOOLEAN_TYPE_P (TREE_TYPE (expr)) && TYPE_PRECISION (itype) <= BITS_PER_UNIT){
      /* This is the only case in which elements can be smaller than a byte.
      Element 0 is always in the lsb of the containing byte.  */
      unsigned int elt_bits = TYPE_PRECISION (itype);
      int total_bytes = CEIL (elt_bits * count, BITS_PER_UNIT);
      if ((off == -1 && total_bytes > len) || off >= total_bytes)
         return 0;

      if (off == -1)
         off = 0;

      /* Zero the buffer and then set bits later where necessary.  */
      int extract_bytes = MIN (len, total_bytes - off);
      if (ptr)
         memset (ptr, 0, extract_bytes);

      unsigned int elts_per_byte = BITS_PER_UNIT / elt_bits;
      unsigned int first_elt = off * elts_per_byte;
      unsigned int extract_elts = extract_bytes * elts_per_byte;
      for (unsigned int i = 0; i < extract_elts; ++i){
         tree elt = VECTOR_CST_ELT (expr, first_elt + i);
         if (TREE_CODE (elt) != INTEGER_CST)
            return 0;

         if (ptr && wi::extract_uhwi (wi::to_wide (elt), 0, 1)){
            unsigned int bit = i * elt_bits;
            ptr[bit / BITS_PER_UNIT] |= 1 << (bit % BITS_PER_UNIT);
         }
      }
      return extract_bytes;
   }

   int offset = 0;
   int size = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mtcs_mode_scalar_type_mode/*!SCALAR_TYPE_MODE*/(mtcsMode,itype));
   for (unsigned HOST_WIDE_INT i = 0; i < count; i++){
      if (off >= size){
         off -= size;
         continue;
      }
      tree elem = VECTOR_CST_ELT (expr, i);
      int res = mtcs_const_native_encode_expr/*!native_encode_expr*/(self,elem, ptr ? ptr + offset : NULL,
      len - offset, off);
      if ((off == -1 && res != size) || res == 0)
         return 0;
      offset += res;
      if (offset >= len)
         return (off == -1 && i < count - 1) ? 0 : offset;
      if (off != -1)
         off = 0;
   }
   return offset;
}

/* Read a vector of type TYPE from the target memory image given by BYTES,
   which contains LEN bytes.  The vector is known to be encodable using
   NPATTERNS interleaved patterns with NELTS_PER_PATTERN elements each.

   Return the vector on success, otherwise return null.  */
//原型 native_interpret_vector_part fold-const.cc
static tree native_interpret_vector_part (MtcsConst *self,tree type, const unsigned char *bytes,
               unsigned int len, unsigned int npatterns,
               unsigned int nelts_per_pattern)
{
   tree elt_type = TREE_TYPE (type);
   if (VECTOR_BOOLEAN_TYPE_P (type)  && TYPE_PRECISION (elt_type) <= BITS_PER_UNIT){
      /* This is the only case in which elements can be smaller than a byte.
      Element 0 is always in the lsb of the containing byte.  */
      unsigned int elt_bits = TYPE_PRECISION (elt_type);
      if (elt_bits * npatterns * nelts_per_pattern > len * BITS_PER_UNIT)
         return NULL_TREE;

      tree_vector_builder builder (type, npatterns, nelts_per_pattern);
      for (unsigned int i = 0; i < builder.encoded_nelts (); ++i){
         unsigned int bit_index = i * elt_bits;
         unsigned int byte_index = bit_index / BITS_PER_UNIT;
         unsigned int lsb = bit_index % BITS_PER_UNIT;
         builder.quick_push (bytes[byte_index] & (1 << lsb) ? build_all_ones_cst (elt_type): build_zero_cst (elt_type));
      }
      return builder.build ();
   }

   unsigned int elt_bytes = tree_to_uhwi (TYPE_SIZE_UNIT (elt_type));
   if (elt_bytes * npatterns * nelts_per_pattern > len)
      return NULL_TREE;

   tree_vector_builder builder (type, npatterns, nelts_per_pattern);
   for (unsigned int i = 0; i < builder.encoded_nelts (); ++i){
      tree elt = native_interpret_expr (elt_type, bytes, elt_bytes);
      if (!elt)
         return NULL_TREE;
      builder.quick_push (elt);
      bytes += elt_bytes;
   }
   return builder.build ();
}


/* Try to view-convert VECTOR_CST EXPR to VECTOR_TYPE TYPE by operating
   directly on the VECTOR_CST encoding, in a way that works for variable-
   length vectors.  Return the resulting VECTOR_CST on success or null
   on failure.  */
//原型 fold_view_convert_vector_encoding fold-const.cc
static tree fold_view_convert_vector_encoding (MtcsConst *self,tree type, tree expr)
{
  tree expr_type = TREE_TYPE (expr);
  poly_uint64 type_bits, expr_bits;
  if (!poly_int_tree_p (TYPE_SIZE (type), &type_bits) || !poly_int_tree_p (TYPE_SIZE (expr_type), &expr_bits))
    return NULL_TREE;

  poly_uint64 type_units = TYPE_VECTOR_SUBPARTS (type);
  poly_uint64 expr_units = TYPE_VECTOR_SUBPARTS (expr_type);
  unsigned int type_elt_bits = vector_element_size (type_bits, type_units);
  unsigned int expr_elt_bits = vector_element_size (expr_bits, expr_units);

  /* We can only preserve the semantics of a stepped pattern if the new
     vector element is an integer of the same size.  */
  if (VECTOR_CST_STEPPED_P (expr) && (!INTEGRAL_TYPE_P (type) || type_elt_bits != expr_elt_bits))
    return NULL_TREE;

  /* The number of bits needed to encode one element from every pattern
     of the original vector.  */
  unsigned int expr_sequence_bits = VECTOR_CST_NPATTERNS (expr) * expr_elt_bits;

  /* The number of bits needed to encode one element from every pattern
     of the result.  */
  unsigned int type_sequence_bits = least_common_multiple (expr_sequence_bits, type_elt_bits);

  /* Don't try to read more bytes than are available, which can happen
     for constant-sized vectors if TYPE has larger elements than EXPR_TYPE.
     The general VIEW_CONVERT handling can cope with that case, so there's
     no point complicating things here.  */
  unsigned int nelts_per_pattern = VECTOR_CST_NELTS_PER_PATTERN (expr);
  unsigned int buffer_bytes = CEIL (nelts_per_pattern * type_sequence_bits,BITS_PER_UNIT);
  unsigned int buffer_bits = buffer_bytes * BITS_PER_UNIT;
  if (known_gt (buffer_bits, expr_bits))
    return NULL_TREE;

  /* Get enough bytes of EXPR to form the new encoding.  */
  auto_vec<unsigned char, 128> buffer (buffer_bytes);
  buffer.quick_grow (buffer_bytes);
  if (native_encode_vector_part(self,expr, buffer.address (), buffer_bytes, 0,
             buffer_bits / expr_elt_bits) != (int) buffer_bytes)
    return NULL_TREE;

  /* Reencode the bytes as TYPE.  */
  unsigned int type_npatterns = type_sequence_bits / type_elt_bits;
  return native_interpret_vector_part(self,type, &buffer[0], buffer.length (),type_npatterns, nelts_per_pattern);
}


/* Fold a VIEW_CONVERT_EXPR of a constant expression EXPR to type
   TYPE at compile-time.  If we're unable to perform the conversion
   return NULL_TREE.  */
//原型 fold_view_convert_expr fold-const.cc
static tree fold_view_convert_expr (MtcsConst *self,tree type, tree expr)
{
   unsigned char buffer[128];
   unsigned char *buf;
   int len;
   HOST_WIDE_INT l;

   /* Check that the host and target are sane.  */
   if (CHAR_BIT != 8 || BITS_PER_UNIT != 8)
      return NULL_TREE;

   if (VECTOR_TYPE_P (type) && TREE_CODE (expr) == VECTOR_CST)
      if (tree res = fold_view_convert_vector_encoding(self,type, expr))
         return res;

   l = int_size_in_bytes (type);
   if (l > (int) sizeof (buffer)  && l <= WIDE_INT_MAX_PRECISION / BITS_PER_UNIT){
      buf = XALLOCAVEC (unsigned char, l);
      len = l;
   }else{
      buf = buffer;
      len = sizeof (buffer);
   }
   len = mtcs_const_native_encode_expr(self,expr, buf, len);
   if (len == 0)
      return NULL_TREE;

   return native_interpret_expr (type, buf, len);
}

/* OP is the INDEXth operand to CODE (counting from zero) and OTHER_OP
   is the other operand.  Try to use the value of OP to simplify the
   operation in one step, without having to process individual elements.  */
//原型 simplify_const_binop fold-const.cc
static tree simplify_const_binop (tree_code code, tree op, tree other_op,
            int index ATTRIBUTE_UNUSED)
{
   /* AND, IOR as well as XOR with a zerop can be simplified directly.  */
   if (TREE_CODE (op) == VECTOR_CST && TREE_CODE (other_op) == VECTOR_CST){
      if (integer_zerop (other_op)){
         if (code == BIT_IOR_EXPR || code == BIT_XOR_EXPR)
            return op;
         else if (code == BIT_AND_EXPR)
            return other_op;
      }
   }
   return NULL_TREE;
}


/* Subroutine of native_encode_expr.  Encode the INTEGER_CST
   specified by EXPR into the buffer PTR of length LEN bytes.
   Return the number of bytes placed in the buffer, or zero
   upon failure.  */
//原型 native_encode_int fold-const.cc
static int native_encode_int (MtcsConst *self,const_tree expr, unsigned char *ptr, int len, int off)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   tree type = TREE_TYPE (expr);
   int total_bytes;
   if (TREE_CODE (type) == BITINT_TYPE){
      struct bitint_info info;
      bool ok =target_c_bitint_type_info/*!targetm.c.bitint_type_info*/(mtcsMachine->c,TYPE_PRECISION (type), &info);
      gcc_assert (ok);
      scalar_int_mode limb_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,info.limb_mode);
      if (TYPE_PRECISION (type) > mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,limb_mode)){
         total_bytes = tree_to_uhwi (TYPE_SIZE_UNIT (type));
         /* More work is needed when adding _BitInt support to PDP endian
         if limb is smaller than word, or if _BitInt limb ordering doesn't
         match target endianity here.  */
         gcc_checking_assert (info.big_endian == WORDS_BIG_ENDIAN   && (BYTES_BIG_ENDIAN == WORDS_BIG_ENDIAN
               || (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,limb_mode) >= UNITS_PER_WORD)));
      }else
         total_bytes = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,type));
   }else
      total_bytes = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,type));

   int byte, offset, word, words;
   unsigned char value;

   if ((off == -1 && total_bytes > len) || off >= total_bytes)
      return 0;
   if (off == -1)
      off = 0;

   if (ptr == NULL)
      /* Dry run.  */
      return MIN (len, total_bytes - off);

   words = total_bytes / UNITS_PER_WORD;

   for (byte = 0; byte < total_bytes; byte++){
      int bitpos = byte * BITS_PER_UNIT;
      /* Extend EXPR according to TYPE_SIGN if the precision isn't a whole
      number of bytes.  */
      value = wi::extract_uhwi (wi::to_widest (expr), bitpos, BITS_PER_UNIT);

      if (total_bytes > UNITS_PER_WORD){
         word = byte / UNITS_PER_WORD;
         if (WORDS_BIG_ENDIAN)
            word = (words - 1) - word;
         offset = word * UNITS_PER_WORD;
         if (BYTES_BIG_ENDIAN)
            offset += (UNITS_PER_WORD - 1) - (byte % UNITS_PER_WORD);
         else
            offset += byte % UNITS_PER_WORD;
      }else
         offset = BYTES_BIG_ENDIAN ? (total_bytes - 1) - byte : byte;
      if (offset >= off && offset - off < len)
         ptr[offset - off] = value;
   }
   return MIN (len, total_bytes - off);
}


/* Subroutine of native_encode_expr.  Encode the FIXED_CST
   specified by EXPR into the buffer PTR of length LEN bytes.
   Return the number of bytes placed in the buffer, or zero
   upon failure.  */
//原型 native_encode_fixed fold-const.cc
static int native_encode_fixed (MtcsConst *self,const_tree expr, unsigned char *ptr, int len, int off)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsLang *mtcsLang=mtcs_target_get_lang(mtcsTarget);

   tree type = TREE_TYPE (expr);
   scalar_mode mode = mtcs_mode_scalar_type_mode/*!SCALAR_TYPE_MODE*/(mtcsMode,type);
   int total_bytes = mtcs_mode_get_size/*!GET_MODE_BITSIZE*/(mtcsMode,mode);
   FIXED_VALUE_TYPE value;
   tree i_value, i_type;

   if (total_bytes * BITS_PER_UNIT > HOST_BITS_PER_DOUBLE_INT)
      return 0;

   i_type = mtcsLang->types.type_for_size/*!lang_hooks.types.type_for_size*/(mtcsLang,
         mtcs_mode_get_size/*!GET_MODE_BITSIZE*/(mtcsMode,mode), 1);

   if (NULL_TREE == i_type || TYPE_PRECISION (i_type) != total_bytes)
      return 0;

   value = TREE_FIXED_CST (expr);
   i_value = mtcs_tree_double_int_to_tree/*!double_int_to_tree*/(mtcsTree,i_type, value.data);

   return native_encode_int(self,i_value, ptr, len, off);
}


/* Subroutine of native_encode_expr.  Encode the REAL_CST
   specified by EXPR into the buffer PTR of length LEN bytes.
   Return the number of bytes placed in the buffer, or zero
   upon failure.  */
//原型 native_encode_real fold-const.cc
static int native_encode_real (MtcsConst *self,const_tree expr, unsigned char *ptr, int len, int off)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);

   tree type = TREE_TYPE (expr);
   int total_bytes = mtcs_mode_get_size/*!GET_MODE_BITSIZE*/(mtcsMode,
         mtcs_mode_scalar_float_type_mode/*!SCALAR_FLOAT_TYPE_MODE*/(mtcsMode,type));
   int byte, offset, word, words, bitpos;
   unsigned char value;

   /* There are always 32 bits in each long, no matter the size of
   the hosts long.  We handle floating point representations with
   up to 192 bits.  */
   long tmp[6];

   if ((off == -1 && total_bytes > len) || off >= total_bytes)
      return 0;
   if (off == -1)
      off = 0;

   if (ptr == NULL)
      /* Dry run.  */
      return MIN (len, total_bytes - off);

   words = (32 / BITS_PER_UNIT) / UNITS_PER_WORD;

   mtcs_real_real_to_target/*!real_to_target*/(mtcsReal,tmp, TREE_REAL_CST_PTR (expr), TYPE_MODE (type));

   for (bitpos = 0; bitpos < total_bytes * BITS_PER_UNIT; bitpos += BITS_PER_UNIT){
      byte = (bitpos / BITS_PER_UNIT) & 3;
      value = (unsigned char) (tmp[bitpos / 32] >> (bitpos & 31));

      if (UNITS_PER_WORD < 4){
         word = byte / UNITS_PER_WORD;
         if (WORDS_BIG_ENDIAN)
            word = (words - 1) - word;
         offset = word * UNITS_PER_WORD;
         if (BYTES_BIG_ENDIAN)
            offset += (UNITS_PER_WORD - 1) - (byte % UNITS_PER_WORD);
         else
            offset += byte % UNITS_PER_WORD;
      }else{
         offset = byte;
         if (BYTES_BIG_ENDIAN){
            /* Reverse bytes within each long, or within the entire float
            if it's smaller than a long (for HFmode).  */
            offset = MIN (3, total_bytes - 1) - offset;
            gcc_assert (offset >= 0);
         }
      }
      offset = offset + ((bitpos / BITS_PER_UNIT) & ~3);
      if (offset >= off && offset - off < len)
         ptr[offset - off] = value;
   }
   return MIN (len, total_bytes - off);
}

/* Subroutine of native_encode_expr.  Encode the COMPLEX_CST
   specified by EXPR into the buffer PTR of length LEN bytes.
   Return the number of bytes placed in the buffer, or zero
   upon failure.  */
//原型 native_encode_complex fold-const.cc
static int native_encode_complex (MtcsConst *self,const_tree expr, unsigned char *ptr, int len, int off)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsLang *mtcsLang=mtcs_target_get_lang(mtcsTarget);

   int rsize, isize;
   tree part;

   part = TREE_REALPART (expr);
   rsize = mtcs_const_native_encode_expr(self,part, ptr, len, off);
   if (off == -1 && rsize == 0)
      return 0;
   part = TREE_IMAGPART (expr);
   if (off != -1)
      off = MAX (0, off - mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,
            mtcs_mode_scalar_type_mode/*!SCALAR_TYPE_MODE*/(mtcsMode,TREE_TYPE (part))));
   isize = mtcs_const_native_encode_expr(self,part, ptr ? ptr + rsize : NULL,
   len - rsize, off);
   if (off == -1 && isize != rsize)
      return 0;
   return rsize + isize;
}

/* Subroutine of native_encode_expr.  Encode the VECTOR_CST
   specified by EXPR into the buffer PTR of length LEN bytes.
   Return the number of bytes placed in the buffer, or zero
   upon failure.  */
//原型 native_encode_vector fold-const.cc
static int native_encode_vector (MtcsConst *self,const_tree expr, unsigned char *ptr, int len, int off)
{
   unsigned HOST_WIDE_INT count;
   if (!VECTOR_CST_NELTS (expr).is_constant (&count))
      return 0;
   return native_encode_vector_part(self,expr, ptr, len, off, count);
}


/* Subroutine of native_encode_expr.  Encode the STRING_CST
   specified by EXPR into the buffer PTR of length LEN bytes.
   Return the number of bytes placed in the buffer, or zero
   upon failure.  */
//原型 native_encode_string fold-const.cc

static int native_encode_string (MtcsConst *self,const_tree expr, unsigned char *ptr, int len, int off)
{
   tree type = TREE_TYPE (expr);

   /* Wide-char strings are encoded in target byte-order so native
   encoding them is trivial.  */
   if (BITS_PER_UNIT != CHAR_BIT
   || TREE_CODE (type) != ARRAY_TYPE
   || TREE_CODE (TREE_TYPE (type)) != INTEGER_TYPE
   || !tree_fits_shwi_p (TYPE_SIZE_UNIT (type)))
      return 0;

   HOST_WIDE_INT total_bytes = tree_to_shwi (TYPE_SIZE_UNIT (TREE_TYPE (expr)));
   if ((off == -1 && total_bytes > len) || off >= total_bytes)
      return 0;
   if (off == -1)
      off = 0;
   len = MIN (total_bytes - off, len);
   if (ptr == NULL)
   /* Dry run.  */;
   else{
      int written = 0;
      if (off < TREE_STRING_LENGTH (expr)){
         written = MIN (len, TREE_STRING_LENGTH (expr) - off);
         memcpy (ptr, TREE_STRING_POINTER (expr) + off, written);
      }
      memset (ptr + written, 0, len - written);
   }
   return len;
}

/* Return the tree for neg (ARG0) when ARG0 is known to be either
   an integer constant, real, or fixed-point constant.

   TYPE is the type of the result.  */
//原型 fold_negate_const fold-const.cc
static tree fold_negate_const (MtcsConst *self,tree arg0, tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree t = NULL_TREE;
   switch (TREE_CODE (arg0)){
      case REAL_CST:
         t = mtcs_tree_build_real/*!build_real*/(mtcsTree,type, real_value_negate (&TREE_REAL_CST (arg0)));
         break;
      case FIXED_CST:
      {
         FIXED_VALUE_TYPE f;
         bool overflow_p = fixed_arithmetic (&f, NEGATE_EXPR,&(TREE_FIXED_CST (arg0)), NULL,TYPE_SATURATING (type));
         t = build_fixed (type, f);
         /* Propagate overflow flags.  */
         if (overflow_p | TREE_OVERFLOW (arg0))
            TREE_OVERFLOW (t) = 1;
         break;
      }
      default:
         if (poly_int_tree_p (arg0)){
            wi::overflow_type overflow;
            poly_wide_int res = wi::neg (wi::to_poly_wide (arg0), &overflow);
            t = mtcs_tree_force_fit_type/*!force_fit_type*/(mtcsTree,
                  type, res, 1,(overflow && ! TYPE_UNSIGNED (type)) || TREE_OVERFLOW (arg0));
            break;
         }
         gcc_unreachable ();
   }
   return t;
}

/* Return the tree for not (ARG0) when ARG0 is known to be an integer
   constant.  TYPE is the type of the result.  */
//原型 fold_not_const fold-const.cc
static tree fold_not_const (MtcsConst *self,const_tree arg0, tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   gcc_assert (TREE_CODE (arg0) == INTEGER_CST);
   return mtcs_tree_force_fit_type/*!force_fit_type*/(mtcsTree,type, ~wi::to_wide (arg0), 0, TREE_OVERFLOW (arg0));
}


/* Given CODE, a relational operator, the target type, TYPE and two
   constant operands OP0 and OP1, return the result of the
   relational operation.  If the result is not a compile time
   constant, then return NULL_TREE.  */
//原型 fold_relational_const fold-const.cc
static tree fold_relational_const (MtcsConst *self,enum tree_code code, tree type, tree op0, tree op1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

   int result, invert;
   /* From here on, the only cases we handle are when the result is
   known to be a constant.  */
   if (TREE_CODE (op0) == REAL_CST && TREE_CODE (op1) == REAL_CST){
      const REAL_VALUE_TYPE *c0 = TREE_REAL_CST_PTR (op0);
      const REAL_VALUE_TYPE *c1 = TREE_REAL_CST_PTR (op1);

      /* Handle the cases where either operand is a NaN.  */
      if (real_isnan (c0) || real_isnan (c1)){
         switch (code){
            case EQ_EXPR:
            case ORDERED_EXPR:
               result = 0;
               break;

            case NE_EXPR:
            case UNORDERED_EXPR:
            case UNLT_EXPR:
            case UNLE_EXPR:
            case UNGT_EXPR:
            case UNGE_EXPR:
            case UNEQ_EXPR:
               result = 1;
               break;

            case LT_EXPR:
            case LE_EXPR:
            case GT_EXPR:
            case GE_EXPR:
            case LTGT_EXPR:
               if (mtcsOptionsItem->x_flag_trapping_math)
                  return NULL_TREE;
               result = 0;
               break;

            default:
               gcc_unreachable ();
         }

         return mtcs_const_constant_boolean_node/*!constant_boolean_node*/(self,result, type);
      }

      return mtcs_const_constant_boolean_node/*!constant_boolean_node*/(self,real_compare (code, c0, c1), type);
   }

   if (TREE_CODE (op0) == FIXED_CST && TREE_CODE (op1) == FIXED_CST){
      const FIXED_VALUE_TYPE *c0 = TREE_FIXED_CST_PTR (op0);
      const FIXED_VALUE_TYPE *c1 = TREE_FIXED_CST_PTR (op1);
      return mtcs_const_constant_boolean_node/*!constant_boolean_node*/(self,fixed_compare (code, c0, c1), type);
   }

   /* Handle equality/inequality of complex constants.  */
   if (TREE_CODE (op0) == COMPLEX_CST && TREE_CODE (op1) == COMPLEX_CST){
      tree rcond = fold_relational_const(self,code, type, TREE_REALPART (op0),TREE_REALPART (op1));
      tree icond = fold_relational_const(self,code, type, TREE_IMAGPART (op0),TREE_IMAGPART (op1));
      if (code == EQ_EXPR)
         return mtcs_const_fold_build2/*!fold_build2*/(self,TRUTH_ANDIF_EXPR, type, rcond, icond);
      else if (code == NE_EXPR)
         return mtcs_const_fold_build2/*!fold_build2*/(self,TRUTH_ORIF_EXPR, type, rcond, icond);
      else
         return NULL_TREE;
   }

   if (TREE_CODE (op0) == VECTOR_CST && TREE_CODE (op1) == VECTOR_CST){
      if (!VECTOR_TYPE_P (type)){
         /* Have vector comparison with scalar boolean result.  */
         gcc_assert ((code == EQ_EXPR || code == NE_EXPR) && known_eq (VECTOR_CST_NELTS (op0),VECTOR_CST_NELTS (op1)));
         unsigned HOST_WIDE_INT nunits;
         if (!VECTOR_CST_NELTS (op0).is_constant (&nunits))
            return NULL_TREE;
         for (unsigned i = 0; i < nunits; i++){
            tree elem0 = VECTOR_CST_ELT (op0, i);
            tree elem1 = VECTOR_CST_ELT (op1, i);
            tree tmp = fold_relational_const(self,EQ_EXPR, type, elem0, elem1);
            if (tmp == NULL_TREE)
               return NULL_TREE;
            if (integer_zerop (tmp))
               return mtcs_const_constant_boolean_node/*!constant_boolean_node*/(self,code == NE_EXPR, type);
         }
         return mtcs_const_constant_boolean_node/*!constant_boolean_node*/(self,code == EQ_EXPR, type);
      }
      tree_vector_builder elts;
      if (!elts.new_binary_operation (type, op0, op1, false))
         return NULL_TREE;
      unsigned int count = elts.encoded_nelts ();
      for (unsigned i = 0; i < count; i++){
         tree elem_type = TREE_TYPE (type);
         tree elem0 = VECTOR_CST_ELT (op0, i);
         tree elem1 = VECTOR_CST_ELT (op1, i);

         tree tem = fold_relational_const(self,code, elem_type,elem0, elem1);

         if (tem == NULL_TREE)
            return NULL_TREE;

         elts.quick_push (mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,elem_type,integer_zerop (tem) ? 0 : -1));
      }

      return elts.build ();
   }

   /* From here on we only handle LT, LE, GT, GE, EQ and NE.

   To compute GT, swap the arguments and do LT.
   To compute GE, do LT and invert the result.
   To compute LE, swap the arguments, do LT and invert the result.
   To compute NE, do EQ and invert the result.

   Therefore, the code below must handle only EQ and LT.  */

   if (code == LE_EXPR || code == GT_EXPR){
      std::swap (op0, op1);
      code = swap_tree_comparison (code);
   }

   /* Note that it is safe to invert for real values here because we
   have already handled the one case that it matters.  */

   invert = 0;
   if (code == NE_EXPR || code == GE_EXPR){
      invert = 1;
      code = invert_tree_comparison (code, false);
   }

   /* Compute a result for LT or EQ if args permit;
   Otherwise return T.  */
   if (TREE_CODE (op0) == INTEGER_CST && TREE_CODE (op1) == INTEGER_CST){
      if (code == EQ_EXPR)
         result = tree_int_cst_equal (op0, op1);
      else
         result = tree_int_cst_lt (op0, op1);
   }else
      return NULL_TREE;

   if (invert)
      result ^= 1;
   return mtcs_const_constant_boolean_node/*!constant_boolean_node*/(self,result, type);
}

/* Return EXPR_LOCATION of T if it is not UNKNOWN_LOCATION.
   Otherwise, return LOC.  */
//原型 expr_location_or fold-const.cc
static location_t expr_location_or (tree t, location_t loc)
{
  location_t tloc = EXPR_LOCATION (t);
  return tloc == UNKNOWN_LOCATION ? loc : tloc;
}

/* Return a simplified tree node for the truth-negation of ARG.  This
   never alters ARG itself.  We assume that ARG is an operation that
   returns a truth value (0 or 1).

   FIXME: one would think we would fold the result, but it causes
   problems with the dominator optimizer.  */
//原型 fold_truth_not_expr fold-const.cc
static tree fold_truth_not_expr (MtcsConst *self,location_t loc, tree arg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

   tree type = TREE_TYPE (arg);
   enum tree_code code = TREE_CODE (arg);
   location_t loc1, loc2;

   /* If this is a comparison, we can simply invert it, except for
   floating-point non-equality comparisons, in which case we just
   enclose a TRUTH_NOT_EXPR around what we have.  */

   if (TREE_CODE_CLASS (code) == tcc_comparison){
      tree op_type = TREE_TYPE (TREE_OPERAND (arg, 0));
      if (FLOAT_TYPE_P (op_type)
      && mtcsOptionsItem->x_flag_trapping_math
      && code != ORDERED_EXPR && code != UNORDERED_EXPR
      && code != NE_EXPR && code != EQ_EXPR)
         return NULL_TREE;

      code = invert_tree_comparison (code, mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,op_type));
      if (code == ERROR_MARK)
         return NULL_TREE;

      tree ret = build2_loc (loc, code, type, TREE_OPERAND (arg, 0),TREE_OPERAND (arg, 1));
      copy_warning (ret, arg);
      return ret;
   }

   switch (code){
      case INTEGER_CST:
         return mtcs_const_constant_boolean_node/*!constant_boolean_node*/(self,integer_zerop (arg), type);

      case TRUTH_AND_EXPR:
         loc1 = expr_location_or (TREE_OPERAND (arg, 0), loc);
         loc2 = expr_location_or (TREE_OPERAND (arg, 1), loc);
         return build2_loc (loc, TRUTH_OR_EXPR, type,
               invert_truthvalue_loc (loc1, TREE_OPERAND (arg, 0)),invert_truthvalue_loc (loc2, TREE_OPERAND (arg, 1)));

      case TRUTH_OR_EXPR:
         loc1 = expr_location_or (TREE_OPERAND (arg, 0), loc);
         loc2 = expr_location_or (TREE_OPERAND (arg, 1), loc);
         return build2_loc (loc, TRUTH_AND_EXPR, type,invert_truthvalue_loc (loc1, TREE_OPERAND (arg, 0)),
               invert_truthvalue_loc (loc2, TREE_OPERAND (arg, 1)));

      case TRUTH_XOR_EXPR:
         /* Here we can invert either operand.  We invert the first operand
         unless the second operand is a TRUTH_NOT_EXPR in which case our
         result is the XOR of the first operand with the inside of the
         negation of the second operand.  */

         if (TREE_CODE (TREE_OPERAND (arg, 1)) == TRUTH_NOT_EXPR)
            return build2_loc (loc, TRUTH_XOR_EXPR, type, TREE_OPERAND (arg, 0),TREE_OPERAND (TREE_OPERAND (arg, 1), 0));
         else
            return build2_loc (loc, TRUTH_XOR_EXPR, type,
                  invert_truthvalue_loc (loc, TREE_OPERAND (arg, 0)),TREE_OPERAND (arg, 1));

      case TRUTH_ANDIF_EXPR:
         loc1 = expr_location_or (TREE_OPERAND (arg, 0), loc);
         loc2 = expr_location_or (TREE_OPERAND (arg, 1), loc);
         return build2_loc (loc, TRUTH_ORIF_EXPR, type,invert_truthvalue_loc (loc1, TREE_OPERAND (arg, 0)),
               invert_truthvalue_loc (loc2, TREE_OPERAND (arg, 1)));

      case TRUTH_ORIF_EXPR:
         loc1 = expr_location_or (TREE_OPERAND (arg, 0), loc);
         loc2 = expr_location_or (TREE_OPERAND (arg, 1), loc);
         return build2_loc (loc, TRUTH_ANDIF_EXPR, type,invert_truthvalue_loc (loc1, TREE_OPERAND (arg, 0)),
               invert_truthvalue_loc (loc2, TREE_OPERAND (arg, 1)));

      case TRUTH_NOT_EXPR:
         return TREE_OPERAND (arg, 0);

      case COND_EXPR:
      {
         tree arg1 = TREE_OPERAND (arg, 1);
         tree arg2 = TREE_OPERAND (arg, 2);

         loc1 = expr_location_or (TREE_OPERAND (arg, 1), loc);
         loc2 = expr_location_or (TREE_OPERAND (arg, 2), loc);

         /* A COND_EXPR may have a throw as one operand, which
         then has void type.  Just leave void operands
         as they are.  */
         return build3_loc (loc, COND_EXPR, type, TREE_OPERAND (arg, 0),
            VOID_TYPE_P (TREE_TYPE (arg1))? arg1 : invert_truthvalue_loc (loc1, arg1),
            VOID_TYPE_P (TREE_TYPE (arg2)) ? arg2 : invert_truthvalue_loc (loc2, arg2));
      }

      case COMPOUND_EXPR:
         loc1 = expr_location_or (TREE_OPERAND (arg, 1), loc);
         return build2_loc (loc, COMPOUND_EXPR, type,TREE_OPERAND (arg, 0),invert_truthvalue_loc (loc1, TREE_OPERAND (arg, 1)));

      case NON_LVALUE_EXPR:
         loc1 = expr_location_or (TREE_OPERAND (arg, 0), loc);
         return invert_truthvalue_loc (loc1, TREE_OPERAND (arg, 0));

      CASE_CONVERT:
         if (TREE_CODE (TREE_TYPE (arg)) == BOOLEAN_TYPE)
            return build1_loc (loc, TRUTH_NOT_EXPR, type, arg);

      /* fall through */

      case FLOAT_EXPR:
         loc1 = expr_location_or (TREE_OPERAND (arg, 0), loc);
         return build1_loc (loc, TREE_CODE (arg), type,invert_truthvalue_loc (loc1, TREE_OPERAND (arg, 0)));

      case BIT_AND_EXPR:
         if (!integer_onep (TREE_OPERAND (arg, 1)))
            return NULL_TREE;
         return build2_loc (loc, EQ_EXPR, type, arg, mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,type, 0));

      case SAVE_EXPR:
         return build1_loc (loc, TRUTH_NOT_EXPR, type, arg);

      case CLEANUP_POINT_EXPR:
         loc1 = expr_location_or (TREE_OPERAND (arg, 0), loc);
         return build1_loc (loc, CLEANUP_POINT_EXPR, type,invert_truthvalue_loc (loc1, TREE_OPERAND (arg, 0)));

      default:
         return NULL_TREE;
   }
}

/* Given T, an expression, return a folded tree for -T or NULL_TREE, if no
   simplification is possible.
   If negate_expr_p would return true for T, NULL_TREE will never be
   returned.  */
//原型 fold_negate_expr_1 fold-const.cc
static tree fold_negate_expr_1 (MtcsConst *self,location_t loc, tree t)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

   tree type = TREE_TYPE (t);
   tree tem;

   switch (TREE_CODE (t)){
      /* Convert - (~A) to A + 1.  */
      case BIT_NOT_EXPR:
         if (INTEGRAL_TYPE_P (type))
            return mtcs_const_fold_build2_loc(self,loc, PLUS_EXPR, type, TREE_OPERAND (t, 0),build_one_cst (type));
         break;

      case INTEGER_CST:
         tem = fold_negate_const(self,t, type);
         if (TREE_OVERFLOW (tem) == TREE_OVERFLOW (t)
         || (ANY_INTEGRAL_TYPE_P (type)
         && !TYPE_OVERFLOW_TRAPS (type)
         && TYPE_OVERFLOW_WRAPS (type))
         || (mtcsOptionsItem->x_flag_sanitize & SANITIZE_SI_OVERFLOW) == 0)
            return tem;
         break;

      case POLY_INT_CST:
      case REAL_CST:
      case FIXED_CST:
         tem = fold_negate_const(self,t, type);
         return tem;

      case COMPLEX_CST:
      {
         tree rpart = fold_negate_expr(self,loc, TREE_REALPART (t));
         tree ipart = fold_negate_expr(self,loc, TREE_IMAGPART (t));
         if (rpart && ipart)
            return build_complex (type, rpart, ipart);
      }
         break;

      case VECTOR_CST:
      {
         tree_vector_builder elts;
         elts.new_unary_operation (type, t, true);
         unsigned int count = elts.encoded_nelts ();
         for (unsigned int i = 0; i < count; ++i){
            tree elt = fold_negate_expr(self,loc, VECTOR_CST_ELT (t, i));
            if (elt == NULL_TREE)
               return NULL_TREE;
            elts.quick_push (elt);
         }

         return elts.build ();
      }

      case COMPLEX_EXPR:
         if (negate_expr_p(self,t))
            return mtcs_const_fold_build2_loc(self,loc, COMPLEX_EXPR, type,
         fold_negate_expr(self,loc, TREE_OPERAND (t, 0)),
         fold_negate_expr(self,loc, TREE_OPERAND (t, 1)));
         break;

      case CONJ_EXPR:
         if (negate_expr_p(self,t))
            return mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, CONJ_EXPR, type,
         fold_negate_expr(self,loc, TREE_OPERAND (t, 0)));
         break;

      case NEGATE_EXPR:
         if (!TYPE_OVERFLOW_SANITIZED (type))
            return TREE_OPERAND (t, 0);
         break;

      case PLUS_EXPR:
         if (!HONOR_SIGN_DEPENDENT_ROUNDING (type) && !HONOR_SIGNED_ZEROS (type)){
            /* -(A + B) -> (-B) - A.  */
            if (negate_expr_p(self,TREE_OPERAND (t, 1))){
               tem = negate_expr(self,TREE_OPERAND (t, 1));
               return mtcs_const_fold_build2_loc(self,loc, MINUS_EXPR, type,tem, TREE_OPERAND (t, 0));
            }

            /* -(A + B) -> (-A) - B.  */
            if (negate_expr_p(self,TREE_OPERAND (t, 0))){
               tem = negate_expr(self,TREE_OPERAND (t, 0));
               return mtcs_const_fold_build2_loc(self,loc, MINUS_EXPR, type,tem, TREE_OPERAND (t, 1));
            }
         }
         break;

      case MINUS_EXPR:
         /* - (A - B) -> B - A  */
         if (!HONOR_SIGN_DEPENDENT_ROUNDING (type) && !HONOR_SIGNED_ZEROS (type))
            return mtcs_const_fold_build2_loc(self,loc, MINUS_EXPR, type,TREE_OPERAND (t, 1), TREE_OPERAND (t, 0));
         break;

      case MULT_EXPR:
         if (TYPE_UNSIGNED (type))
            break;

      /* Fall through.  */

      case RDIV_EXPR:
         if (! HONOR_SIGN_DEPENDENT_ROUNDING (type)){
            tem = TREE_OPERAND (t, 1);
            if (negate_expr_p(self,tem))
               return mtcs_const_fold_build2_loc(self,loc, TREE_CODE (t), type,TREE_OPERAND (t, 0), negate_expr(self,tem));
            tem = TREE_OPERAND (t, 0);
            if (negate_expr_p(self,tem))
               return mtcs_const_fold_build2_loc(self,loc, TREE_CODE (t), type,
            negate_expr(self,tem), TREE_OPERAND (t, 1));
         }
         break;

      case TRUNC_DIV_EXPR:
      case ROUND_DIV_EXPR:
      case EXACT_DIV_EXPR:
         if (TYPE_UNSIGNED (type))
            break;
         /* In general we can't negate A in A / B, because if A is INT_MIN and
         B is not 1 we change the sign of the result.  */
         if (TREE_CODE (TREE_OPERAND (t, 0)) == INTEGER_CST  && negate_expr_p(self,TREE_OPERAND (t, 0)))
            return mtcs_const_fold_build2_loc(self,loc, TREE_CODE (t), type,
         negate_expr(self,TREE_OPERAND (t, 0)),TREE_OPERAND (t, 1));
         /* In general we can't negate B in A / B, because if A is INT_MIN and
         B is 1, we may turn this into INT_MIN / -1 which is undefined
         and actually traps on some architectures.  */
         if ((! ANY_INTEGRAL_TYPE_P (TREE_TYPE (t))
         || TYPE_OVERFLOW_WRAPS (TREE_TYPE (t))
         || (TREE_CODE (TREE_OPERAND (t, 1)) == INTEGER_CST
         && ! integer_onep (TREE_OPERAND (t, 1))))
         && negate_expr_p(self,TREE_OPERAND (t, 1)))
            return mtcs_const_fold_build2_loc(self,loc, TREE_CODE (t), type,TREE_OPERAND (t, 0),negate_expr(self,TREE_OPERAND (t, 1)));
            break;

      case NOP_EXPR:
         /* Convert -((double)float) into (double)(-float).  */
         if (SCALAR_FLOAT_TYPE_P (type)){
            tem = strip_float_extensions (t);
            if (tem != t && negate_expr_p(self,tem))
               return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, negate_expr(self,tem));
         }
         break;

      case CALL_EXPR:
         /* Negate -f(x) as f(-x).  */
         if (negate_mathfn_p (get_call_combined_fn (t)) && negate_expr_p(self,CALL_EXPR_ARG (t, 0))){
            tree fndecl, arg;
            fndecl = get_callee_fndecl (t);
            arg = negate_expr(self,CALL_EXPR_ARG (t, 0));
            return mtcs_tree_build_call_expr/*!build_call_expr_loc*/(mtcsTree,loc, fndecl, 1, arg);
         }
         break;

      case RSHIFT_EXPR:
         /* Optimize -((int)x >> 31) into (unsigned)x >> 31 for int.  */
         if (TREE_CODE (TREE_OPERAND (t, 1)) == INTEGER_CST){
            tree op1 = TREE_OPERAND (t, 1);
            if (wi::to_wide (op1) == element_precision (type) - 1){
               tree ntype = TYPE_UNSIGNED (type) ? signed_type_for (type) : unsigned_type_for (type);
               tree temp = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, ntype, TREE_OPERAND (t, 0));
               temp = mtcs_const_fold_build2_loc(self,loc, RSHIFT_EXPR, ntype, temp, op1);
               return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, temp);
            }
         }
         break;

      default:
         break;
   }

   return NULL_TREE;
}

/* A wrapper for fold_negate_expr_1.  */
//原型 fold_negate_expr fold-const.cc
static tree fold_negate_expr (MtcsConst *self,location_t loc, tree t)
{
  tree type = TREE_TYPE (t);
  STRIP_SIGN_NOPS (t);
  tree tem = fold_negate_expr_1(self,loc, t);
  if (tem == NULL_TREE)
    return NULL_TREE;
  return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, tem);
}


/* Construct a vector of zero elements of vector type TYPE.  */
//原型 build_zero_vector fold-const.cc
static tree build_zero_vector (MtcsConst *self,tree type)
{
   tree t;
   t = fold_convert_const(self,NOP_EXPR, TREE_TYPE (type), integer_zero_node);
   return build_vector_from_val (type, t);
}


/* Returns true if we know who is smaller or equal, ARG1 or ARG2, and set the
   min value to RES.  */
//原型 can_min_p fold-const.cc
static bool can_min_p (const_tree arg1, const_tree arg2, poly_wide_int &res)
{
   if (known_le (wi::to_poly_widest (arg1), wi::to_poly_widest (arg2))){
      res = wi::to_poly_wide (arg1);
      return true;
   }else if (known_le (wi::to_poly_widest (arg2), wi::to_poly_widest (arg1))){
      res = wi::to_poly_wide (arg2);
      return true;
   }

   return false;
}

/* Check whether TYPE1 and TYPE2 are equivalent integer types, suitable
   for use in int_const_binop, size_binop and size_diffop.  */
//原型 int_binop_types_match_p fold-const.cc
static bool int_binop_types_match_p (enum tree_code code, const_tree type1, const_tree type2)
{
   if (!INTEGRAL_TYPE_P (type1) && !POINTER_TYPE_P (type1))
      return false;
   if (!INTEGRAL_TYPE_P (type2) && !POINTER_TYPE_P (type2))
      return false;

   switch (code){
      case LSHIFT_EXPR:
      case RSHIFT_EXPR:
      case LROTATE_EXPR:
      case RROTATE_EXPR:
         return true;

      default:
         break;
   }

   if(TYPE_MODE (type1) != TYPE_MODE (type2)){
      aet_print_tree(type1);
      aet_print_tree(type2);
      error("mtcsconst.c int_binop_types_match_p 出错了，两个类型的mode不相同。 %d %d\n",TYPE_MODE (type1),TYPE_MODE (type2));
   }
   return TYPE_UNSIGNED (type1) == TYPE_UNSIGNED (type2)
   && TYPE_PRECISION (type1) == TYPE_PRECISION (type2)
   && TYPE_MODE (type1) == TYPE_MODE (type2);
}

/* Combine two poly int's ARG1 and ARG2 under operation CODE to
   produce a new constant in RES.  Return FALSE if we don't know how
   to evaluate CODE at compile-time.  */
//原型 poly_int_binop fold-const.cc gcc15把 poly_int_binop 变成公共方法
//static bool poly_int_binop (poly_wide_int &res, enum tree_code code,
//      const_tree arg1, const_tree arg2, signop sign, wi::overflow_type *overflow)
//{
//   gcc_assert (NUM_POLY_INT_COEFFS != 1);
//   gcc_assert (poly_int_tree_p (arg1) && poly_int_tree_p (arg2));
//   switch (code){
//      case PLUS_EXPR:
//         res = wi::add (wi::to_poly_wide (arg1), wi::to_poly_wide (arg2), sign, overflow);
//         break;
//
//      case MINUS_EXPR:
//         res = wi::sub (wi::to_poly_wide (arg1),wi::to_poly_wide (arg2), sign, overflow);
//         break;
//
//      case MULT_EXPR:
//         if (TREE_CODE (arg2) == INTEGER_CST)
//            res = wi::mul (wi::to_poly_wide (arg1), wi::to_wide (arg2), sign, overflow);
//         else if (TREE_CODE (arg1) == INTEGER_CST)
//            res = wi::mul (wi::to_poly_wide (arg2), wi::to_wide (arg1), sign, overflow);
//         else
//            return NULL_TREE;
//         break;
//
//      case LSHIFT_EXPR:
//         if (TREE_CODE (arg2) == INTEGER_CST)
//            res = wi::to_poly_wide (arg1) << wi::to_wide (arg2);
//         else
//            return false;
//         break;
//
//      case BIT_IOR_EXPR:
//         if (TREE_CODE (arg2) != INTEGER_CST || !can_ior_p (wi::to_poly_wide (arg1), wi::to_wide (arg2),&res))
//            return false;
//         break;
//
//      case MIN_EXPR:
//         if (!can_min_p (arg1, arg2, res))
//            return false;
//         break;
//
//      default:
//         return false;
//   }
//   return true;
//}

/* A subroutine of fold_convert_const handling conversions of an
   INTEGER_CST to another integer type.  */
//原型 fold_convert_const_int_from_int fold-const.cc
static tree fold_convert_const_int_from_int (MtcsConst *self,tree type, const_tree arg1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   /* Given an integer constant, make new constant with new type,
   appropriately sign-extended or truncated.  Use widest_int
   so that any extension is done according ARG1's type.  */
   tree arg1_type = TREE_TYPE (arg1);
   unsigned prec = MAX (TYPE_PRECISION (arg1_type), TYPE_PRECISION (type));
   return mtcs_tree_force_fit_type/*!force_fit_type*/(mtcsTree,type, wide_int::from (wi::to_wide (arg1), prec,
         TYPE_SIGN (arg1_type)),!POINTER_TYPE_P (TREE_TYPE (arg1)),TREE_OVERFLOW (arg1));
}


/* Attempt to fold type conversion operation CODE of expression ARG1 to
   type TYPE.  If no simplification can be done return NULL_TREE.  */
//原型 fold_convert_const fold-const.cc
static tree fold_convert_const (MtcsConst *self,enum tree_code code, tree type, tree arg1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree arg_type = TREE_TYPE (arg1);
   if (arg_type == type)
      return arg1;

   /* We can't widen types, since the runtime value could overflow the
   original type before being extended to the new type.  */
   if (POLY_INT_CST_P (arg1)
   && (POINTER_TYPE_P (type) || INTEGRAL_TYPE_P (type))
   && TYPE_PRECISION (type) <= TYPE_PRECISION (arg_type))
   return mtcs_tree_build_poly_int_cst/*!build_poly_int_cst*/(mtcsTree,type,
         poly_wide_int::from (poly_int_cst_value (arg1),TYPE_PRECISION (type),TYPE_SIGN (arg_type)));

   if (POINTER_TYPE_P (type) || INTEGRAL_TYPE_P (type)
   || TREE_CODE (type) == OFFSET_TYPE){
      if (TREE_CODE (arg1) == INTEGER_CST)
         return fold_convert_const_int_from_int(self,type, arg1);
      else if (TREE_CODE (arg1) == REAL_CST)
         return fold_convert_const_int_from_real(self,code, type, arg1);
      else if (TREE_CODE (arg1) == FIXED_CST)
         return fold_convert_const_int_from_fixed(self,type, arg1);
   }else if (SCALAR_FLOAT_TYPE_P (type)){
      if (TREE_CODE (arg1) == INTEGER_CST){
         tree res = mtcs_tree_build_real_from_int_cst/*!build_real_from_int_cst*/(mtcsTree,type, arg1);
         /* Avoid the folding if flag_rounding_math is on and the
         conversion is not exact.  */
         if (mtcs_mode_honor_sign_dependent_rounding/*!HONOR_SIGN_DEPENDENT_ROUNDING*/(mtcsMode,type)){
            bool fail = false;
            wide_int w = real_to_integer (&TREE_REAL_CST (res), &fail,TYPE_PRECISION (TREE_TYPE (arg1)));
            if (fail || wi::ne_p (w, wi::to_wide (arg1)))
               return NULL_TREE;
         }
         return res;
      }else if (TREE_CODE (arg1) == REAL_CST)
         return fold_convert_const_real_from_real(self,type, arg1);
      else if (TREE_CODE (arg1) == FIXED_CST)
         return fold_convert_const_real_from_fixed(self,type, arg1);
   }else if (FIXED_POINT_TYPE_P (type)){
      if (TREE_CODE (arg1) == FIXED_CST)
         return fold_convert_const_fixed_from_fixed(self,type, arg1);
      else if (TREE_CODE (arg1) == INTEGER_CST)
         return fold_convert_const_fixed_from_int(self,type, arg1);
      else if (TREE_CODE (arg1) == REAL_CST)
         return fold_convert_const_fixed_from_real(self,type, arg1);
   }else if (VECTOR_TYPE_P (type)){
      if (TREE_CODE (arg1) == VECTOR_CST  && known_eq (TYPE_VECTOR_SUBPARTS (type), VECTOR_CST_NELTS (arg1))){
         tree elttype = TREE_TYPE (type);
         tree arg1_elttype = TREE_TYPE (TREE_TYPE (arg1));
         /* We can't handle steps directly when extending, since the
         values need to wrap at the original precision first.  */
         bool step_ok_p = (INTEGRAL_TYPE_P (elttype) && INTEGRAL_TYPE_P (arg1_elttype)
               && TYPE_PRECISION (elttype) <= TYPE_PRECISION (arg1_elttype));
         tree_vector_builder v;
         if (!v.new_unary_operation (type, arg1, step_ok_p))
            return NULL_TREE;
         unsigned int len = v.encoded_nelts ();
         for (unsigned int i = 0; i < len; ++i){
            tree elt = VECTOR_CST_ELT (arg1, i);
            tree cvt = fold_convert_const(self,code, elttype, elt);
            if (cvt == NULL_TREE)
               return NULL_TREE;
            v.quick_push (cvt);
         }
         return v.build ();
      }
   }
   return NULL_TREE;
}


/* Combine two constants ARG1 and ARG2 under operation CODE to produce a new
   constant.  We assume ARG1 and ARG2 have the same data type, or at least
   are the same kind of constant and the same machine mode.  Return zero if
   combining the constants is not allowed in the current operating mode.  */
//原型 const_binop fold-const.cc fold-const.h中声明同名的 重载函数
static tree const_binop (MtcsConst *self,enum tree_code code, tree arg1, tree arg2)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
   MtcsBuiltins *mtcsBuiltins=mtcs_target_get_builtins(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

   /* Sanity check for the recursive cases.  */
   if (!arg1 || !arg2)
      return NULL_TREE;

   STRIP_NOPS (arg1);
   STRIP_NOPS (arg2);

   if (poly_int_tree_p (arg1) && poly_int_tree_p (arg2)){
      if (code == POINTER_PLUS_EXPR)
         return mtcs_const_int_const_binop/*!int_const_binop*/(self,PLUS_EXPR, arg1,
               mtcs_const_fold_convert/*!fold_convert*/(self,TREE_TYPE (arg1), arg2));

      return mtcs_const_int_const_binop/*!int_const_binop*/(self,code, arg1, arg2);
   }

   if (TREE_CODE (arg1) == REAL_CST && TREE_CODE (arg2) == REAL_CST){
      machine_mode mode;
      REAL_VALUE_TYPE d1;
      REAL_VALUE_TYPE d2;
      REAL_VALUE_TYPE value;
      REAL_VALUE_TYPE result;
      bool inexact;
      tree t, type;

      /* The following codes are handled by real_arithmetic.  */
      switch (code){
         case PLUS_EXPR:
         case MINUS_EXPR:
         case MULT_EXPR:
         case RDIV_EXPR:
         case MIN_EXPR:
         case MAX_EXPR:
            break;

         default:
            return NULL_TREE;
      }

      d1 = TREE_REAL_CST (arg1);
      d2 = TREE_REAL_CST (arg2);

      type = TREE_TYPE (arg1);
      mode = TYPE_MODE (type);

      /* Don't perform operation if we honor signaling NaNs and
      either operand is a signaling NaN.  */
      if (mtcs_mode_honor_snans/*!HONOR_SNANS*/(mtcsMode,mode)
            && (REAL_VALUE_ISSIGNALING_NAN (d1) || REAL_VALUE_ISSIGNALING_NAN (d2)))
         return NULL_TREE;

      /* Don't perform operation if it would raise a division
      by zero exception.  */
      if (code == RDIV_EXPR  && real_equal (&d2, &dconst0)
      && (mtcsOptionsItem->x_flag_trapping_math || ! mtcs_mode_has_infinities/*!MODE_HAS_INFINITIES*/(mtcsMode,mode)))
         return NULL_TREE;

      /* If either operand is a NaN, just return it.  Otherwise, set up
      for floating-point trap; we return an overflow.  */
      if (REAL_VALUE_ISNAN (d1)){
         /* Make resulting NaN value to be qNaN when flag_signaling_nans
         is off.  */
         d1.signalling = 0;
         t = mtcs_tree_build_real/*!build_real*/(mtcsTree,type, d1);
         return t;
      }else if (REAL_VALUE_ISNAN (d2)){
         /* Make resulting NaN value to be qNaN when flag_signaling_nans
         is off.  */
         d2.signalling = 0;
         t = mtcs_tree_build_real/*!build_real*/(mtcsTree,type, d2);
         return t;
      }

      inexact = real_arithmetic (&value, code, &d1, &d2);
      mtcs_real_real_convert/*!real_convert*/(mtcsReal,&result, mode, &value);

      /* Don't constant fold this floating point operation if
      both operands are not NaN but the result is NaN, and
      flag_trapping_math.  Such operations should raise an
      invalid operation exception.  */
      if (mtcsOptionsItem->x_flag_trapping_math
      && mtcs_mode_has_nans/*!MODE_HAS_NANS*/(mtcsMode,mode)
      && REAL_VALUE_ISNAN (result)
      && !REAL_VALUE_ISNAN (d1)
      && !REAL_VALUE_ISNAN (d2))
         return NULL_TREE;

      /* Don't constant fold this floating point operation if
      the result has overflowed and flag_trapping_math.  */
      if (mtcsOptionsItem->x_flag_trapping_math
      && mtcs_mode_has_infinities/*!MODE_HAS_INFINITIES*/(mtcsMode,mode)
      && REAL_VALUE_ISINF (result)
      && !REAL_VALUE_ISINF (d1)
      && !REAL_VALUE_ISINF (d2))
         return NULL_TREE;

      /* Don't constant fold this floating point operation if the
      result may dependent upon the run-time rounding mode and
      flag_rounding_math is set, or if GCC's software emulation
      is unable to accurately represent the result.  */
      if ((mtcsOptionsItem->x_flag_rounding_math  || (mtcs_mode_is_composite_p/*!MODE_COMPOSITE_P*/(mtcsMode,mode)
            && !mtcsOptionsItem->x_flag_unsafe_math_optimizations))
      && (inexact || !real_identical (&result, &value)))
         return NULL_TREE;

      t = mtcs_tree_build_real/*!build_real*/(mtcsTree,type, result);

      TREE_OVERFLOW (t) = TREE_OVERFLOW (arg1) | TREE_OVERFLOW (arg2);
      return t;
   }

   if (TREE_CODE (arg1) == FIXED_CST){
      FIXED_VALUE_TYPE f1;
      FIXED_VALUE_TYPE f2;
      FIXED_VALUE_TYPE result;
      tree t, type;
      bool sat_p;
      bool overflow_p;

      /* The following codes are handled by fixed_arithmetic.  */
      switch (code){
         case PLUS_EXPR:
         case MINUS_EXPR:
         case MULT_EXPR:
         case TRUNC_DIV_EXPR:
            if (TREE_CODE (arg2) != FIXED_CST)
               return NULL_TREE;
            f2 = TREE_FIXED_CST (arg2);
            break;

         case LSHIFT_EXPR:
         case RSHIFT_EXPR:
         {
            if (TREE_CODE (arg2) != INTEGER_CST)
               return NULL_TREE;
            wi::tree_to_wide_ref w2 = wi::to_wide (arg2);
            f2.data.high = w2.elt (1);
            f2.data.low = w2.ulow ();
            f2.mode = SImode;
         }
            break;

         default:
            return NULL_TREE;
      }

      f1 = TREE_FIXED_CST (arg1);
      type = TREE_TYPE (arg1);
      sat_p = TYPE_SATURATING (type);
      overflow_p = fixed_arithmetic (&result, code, &f1, &f2, sat_p);
      t = build_fixed (type, result);
      /* Propagate overflow flags.  */
      if (overflow_p | TREE_OVERFLOW (arg1) | TREE_OVERFLOW (arg2))
         TREE_OVERFLOW (t) = 1;
      return t;
   }

   if (TREE_CODE (arg1) == COMPLEX_CST && TREE_CODE (arg2) == COMPLEX_CST){
      tree type = TREE_TYPE (arg1);
      tree r1 = TREE_REALPART (arg1);
      tree i1 = TREE_IMAGPART (arg1);
      tree r2 = TREE_REALPART (arg2);
      tree i2 = TREE_IMAGPART (arg2);
      tree real, imag;

      switch (code){
         case PLUS_EXPR:
         case MINUS_EXPR:
         real = const_binop(self,code, r1, r2);
         imag = const_binop(self,code, i1, i2);
         break;

         case MULT_EXPR:
            if (COMPLEX_FLOAT_TYPE_P (type))
               return mtcs_builtins_do_mpc_arg2/*!do_mpc_arg2*/(mtcsBuiltins,
                     arg1, arg2, type,/* do_nonfinite= */ folding_initializer,mpc_mul);

            real = const_binop(self,MINUS_EXPR, const_binop(self,MULT_EXPR, r1, r2), const_binop(self,MULT_EXPR, i1, i2));
            imag = const_binop(self,PLUS_EXPR,  const_binop(self,MULT_EXPR, r1, i2),  const_binop(self,MULT_EXPR, i1, r2));
            break;

         case RDIV_EXPR:
            if (COMPLEX_FLOAT_TYPE_P (type))
               return mtcs_builtins_do_mpc_arg2/*!do_mpc_arg2*/(mtcsBuiltins,
                     arg1, arg2, type, /* do_nonfinite= */ folding_initializer, mpc_div);
         /* Fallthru. */
         case TRUNC_DIV_EXPR:
         case CEIL_DIV_EXPR:
         case FLOOR_DIV_EXPR:
         case ROUND_DIV_EXPR:
            if (mtcsOptionsItem->x_flag_complex_method == 0) {
               /* Keep this algorithm in sync with
               tree-complex.cc:expand_complex_div_straight().

               Expand complex division to scalars, straightforward algorithm.
               a / b = ((ar*br + ai*bi)/t) + i((ai*br - ar*bi)/t)
               t = br*br + bi*bi
               */
               tree magsquared = const_binop(self,PLUS_EXPR, const_binop(self,MULT_EXPR, r2, r2), const_binop(self,MULT_EXPR, i2, i2));
               tree t1 = const_binop(self,PLUS_EXPR, const_binop(self,MULT_EXPR, r1, r2), const_binop(self,MULT_EXPR, i1, i2));
               tree t2 = const_binop(self,MINUS_EXPR, const_binop(self,MULT_EXPR, i1, r2),const_binop(self,MULT_EXPR, r1, i2));

               real = const_binop(self,code, t1, magsquared);
               imag = const_binop(self,code, t2, magsquared);
            }else{
               /* Keep this algorithm in sync with
               tree-complex.cc:expand_complex_div_wide().

               Expand complex division to scalars, modified algorithm to minimize
               overflow with wide input ranges.  */
               tree compare = mtcs_const_fold_build2/*!fold_build2*/(self,LT_EXPR, boolean_type_node,
               fold_abs_const (r2, TREE_TYPE (type)),
               fold_abs_const (i2, TREE_TYPE (type)));

               if (integer_nonzerop (compare)){
                  /* In the TRUE branch, we compute
                  ratio = br/bi;
                  div = (br * ratio) + bi;
                  tr = (ar * ratio) + ai;
                  ti = (ai * ratio) - ar;
                  tr = tr / div;
                  ti = ti / div;  */
                  tree ratio = const_binop(self,code, r2, i2);
                  tree div = const_binop(self,PLUS_EXPR, i2, const_binop(self,MULT_EXPR, r2, ratio));
                  real = const_binop(self,MULT_EXPR, r1, ratio);
                  real = const_binop(self,PLUS_EXPR, real, i1);
                  real = const_binop(self,code, real, div);

                  imag = const_binop(self,MULT_EXPR, i1, ratio);
                  imag = const_binop(self,MINUS_EXPR, imag, r1);
                  imag = const_binop(self,code, imag, div);
               }else{
                  /* In the FALSE branch, we compute
                  ratio = d/c;
                  divisor = (d * ratio) + c;
                  tr = (b * ratio) + a;
                  ti = b - (a * ratio);
                  tr = tr / div;
                  ti = ti / div;  */
                  tree ratio = const_binop(self,code, i2, r2);
                  tree div = const_binop(self,PLUS_EXPR, r2, const_binop(self,MULT_EXPR, i2, ratio));

                  real = const_binop(self,MULT_EXPR, i1, ratio);
                  real = const_binop(self,PLUS_EXPR, real, r1);
                  real = const_binop(self,code, real, div);

                  imag = const_binop(self,MULT_EXPR, r1, ratio);
                  imag = const_binop(self,MINUS_EXPR, i1, imag);
                  imag = const_binop(self,code, imag, div);
               }
            }
            break;

         default:
            return NULL_TREE;
      }

      if (real && imag)
         return mtcs_tree_build_complex/*!build_complex*/(mtcsTree,type, real, imag);
   }

   tree simplified;
   if ((simplified = simplify_const_binop (code, arg1, arg2, 0)))
      return simplified;

   if (commutative_tree_code (code) && (simplified = simplify_const_binop (code, arg2, arg1, 1)))
      return simplified;

   if (TREE_CODE (arg1) == VECTOR_CST
   && TREE_CODE (arg2) == VECTOR_CST
   && known_eq (TYPE_VECTOR_SUBPARTS (TREE_TYPE (arg1)),
   TYPE_VECTOR_SUBPARTS (TREE_TYPE (arg2)))){
      tree type = TREE_TYPE (arg1);
      bool step_ok_p;
      if (VECTOR_CST_STEPPED_P (arg1) && VECTOR_CST_STEPPED_P (arg2))
         /* We can operate directly on the encoding if:

         a3 - a2 == a2 - a1 && b3 - b2 == b2 - b1
         implies
         (a3 op b3) - (a2 op b2) == (a2 op b2) - (a1 op b1)

         Addition and subtraction are the supported operators
         for which this is true.  */
         step_ok_p = (code == PLUS_EXPR || code == MINUS_EXPR);
      else if (VECTOR_CST_STEPPED_P (arg1))
         /* We can operate directly on stepped encodings if:

         a3 - a2 == a2 - a1
         implies:
         (a3 op c) - (a2 op c) == (a2 op c) - (a1 op c)

         which is true if (x -> x op c) distributes over addition.  */
         step_ok_p = distributes_over_addition_p (code, 1);
      else
         /* Similarly in reverse.  */
         step_ok_p = distributes_over_addition_p (code, 2);
      tree_vector_builder elts;
      if (!elts.new_binary_operation (type, arg1, arg2, step_ok_p))
         return NULL_TREE;
      unsigned int count = elts.encoded_nelts ();
      for (unsigned int i = 0; i < count; ++i){
         tree elem1 = VECTOR_CST_ELT (arg1, i);
         tree elem2 = VECTOR_CST_ELT (arg2, i);

         tree elt = const_binop(self,code, elem1, elem2);

         /* It is possible that const_binop cannot handle the given
         code and return NULL_TREE */
         if (elt == NULL_TREE)
            return NULL_TREE;
         elts.quick_push (elt);
      }

      return elts.build ();
   }

   /* Shifts allow a scalar offset for a vector.  */
   if (TREE_CODE (arg1) == VECTOR_CST  && TREE_CODE (arg2) == INTEGER_CST){
      tree type = TREE_TYPE (arg1);
      bool step_ok_p = distributes_over_addition_p (code, 1);
      tree_vector_builder elts;
      if (!elts.new_unary_operation (type, arg1, step_ok_p))
         return NULL_TREE;
      unsigned int count = elts.encoded_nelts ();
      for (unsigned int i = 0; i < count; ++i){
         tree elem1 = VECTOR_CST_ELT (arg1, i);

         tree elt = const_binop(self,code, elem1, arg2);

         /* It is possible that const_binop cannot handle the given
         code and return NULL_TREE.  */
         if (elt == NULL_TREE)
            return NULL_TREE;
         elts.quick_push (elt);
      }

      return elts.build ();
   }
   return NULL_TREE;
}



/* Overload that adds a TYPE parameter to be able to dispatch
   to fold_relational_const.  */
//原型 const_binop fold-const.h fold-const.cc
tree mtcs_const_const_binop (MtcsConst *self,enum tree_code code, tree type, tree arg1, tree arg2)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   if (TREE_CODE_CLASS (code) == tcc_comparison)
      return fold_relational_const(self,code, type, arg1, arg2);

   /* ???  Until we make the const_binop worker take the type of the
   result as argument put those cases that need it here.  */
   switch (code){
      case VEC_SERIES_EXPR:
         if (CONSTANT_CLASS_P (arg1)  && CONSTANT_CLASS_P (arg2))
            return build_vec_series (type, arg1, arg2);
         return NULL_TREE;

      case COMPLEX_EXPR:
         if ((TREE_CODE (arg1) == REAL_CST   && TREE_CODE (arg2) == REAL_CST)
         || (TREE_CODE (arg1) == INTEGER_CST  && TREE_CODE (arg2) == INTEGER_CST))
            return build_complex (type, arg1, arg2);
         return NULL_TREE;

      case POINTER_DIFF_EXPR:
         if (poly_int_tree_p (arg1) && poly_int_tree_p (arg2)){
            poly_offset_int res = (wi::to_poly_offset (arg1) - wi::to_poly_offset (arg2));
            return force_fit_type (type, res, 1, TREE_OVERFLOW (arg1) | TREE_OVERFLOW (arg2));
         }
         return NULL_TREE;

      case VEC_PACK_TRUNC_EXPR:
      case VEC_PACK_FIX_TRUNC_EXPR:
      case VEC_PACK_FLOAT_EXPR:
      {
         unsigned int HOST_WIDE_INT out_nelts, in_nelts, i;

         if (TREE_CODE (arg1) != VECTOR_CST  || TREE_CODE (arg2) != VECTOR_CST)
            return NULL_TREE;

         if (!VECTOR_CST_NELTS (arg1).is_constant (&in_nelts))
            return NULL_TREE;

         out_nelts = in_nelts * 2;
         gcc_assert (known_eq (in_nelts, VECTOR_CST_NELTS (arg2))  && known_eq (out_nelts, TYPE_VECTOR_SUBPARTS (type)));

         tree_vector_builder elts (type, out_nelts, 1);
         for (i = 0; i < out_nelts; i++){
            tree elt = (i < in_nelts ? VECTOR_CST_ELT (arg1, i) : VECTOR_CST_ELT (arg2, i - in_nelts));
            elt = fold_convert_const(self,code == VEC_PACK_TRUNC_EXPR ? NOP_EXPR : code == VEC_PACK_FLOAT_EXPR
            ? FLOAT_EXPR : FIX_TRUNC_EXPR, TREE_TYPE (type), elt);
            if (elt == NULL_TREE || !CONSTANT_CLASS_P (elt))
               return NULL_TREE;
            elts.quick_push (elt);
         }

         return elts.build ();
      }

      case VEC_WIDEN_MULT_LO_EXPR:
      case VEC_WIDEN_MULT_HI_EXPR:
      case VEC_WIDEN_MULT_EVEN_EXPR:
      case VEC_WIDEN_MULT_ODD_EXPR:
      {
         unsigned HOST_WIDE_INT out_nelts, in_nelts, out, ofs, scale;

         if (TREE_CODE (arg1) != VECTOR_CST || TREE_CODE (arg2) != VECTOR_CST)
         return NULL_TREE;

         if (!VECTOR_CST_NELTS (arg1).is_constant (&in_nelts))
            return NULL_TREE;
         out_nelts = in_nelts / 2;
         gcc_assert (known_eq (in_nelts, VECTOR_CST_NELTS (arg2))
         && known_eq (out_nelts, TYPE_VECTOR_SUBPARTS (type)));

         if (code == VEC_WIDEN_MULT_LO_EXPR)
            scale = 0, ofs = BYTES_BIG_ENDIAN ? out_nelts : 0;
         else if (code == VEC_WIDEN_MULT_HI_EXPR)
            scale = 0, ofs = BYTES_BIG_ENDIAN ? 0 : out_nelts;
         else if (code == VEC_WIDEN_MULT_EVEN_EXPR)
            scale = 1, ofs = 0;
         else /* if (code == VEC_WIDEN_MULT_ODD_EXPR) */
            scale = 1, ofs = 1;

         tree_vector_builder elts (type, out_nelts, 1);
         for (out = 0; out < out_nelts; out++){
            unsigned int in = (out << scale) + ofs;
            tree t1 = fold_convert_const(self,NOP_EXPR, TREE_TYPE (type),
            VECTOR_CST_ELT (arg1, in));
            tree t2 = fold_convert_const(self,NOP_EXPR, TREE_TYPE (type),
            VECTOR_CST_ELT (arg2, in));

            if (t1 == NULL_TREE || t2 == NULL_TREE)
               return NULL_TREE;
            tree elt = const_binop(self,MULT_EXPR, t1, t2);
            if (elt == NULL_TREE || !CONSTANT_CLASS_P (elt))
               return NULL_TREE;
            elts.quick_push (elt);
         }

         return elts.build ();
      }

      default:;
   }

   if (TREE_CODE_CLASS (code) != tcc_binary)
      return NULL_TREE;

   /* Make sure type and arg0 have the same saturating flag.  */
   gcc_checking_assert (TYPE_SATURATING (type)  == TYPE_SATURATING (TREE_TYPE (arg1)));
   return const_binop(self,code, arg1, arg2);
}

/* Compute CODE ARG1 with resulting type TYPE with ARG1 being constant.
   Return zero if computing the constants is not possible.  */
//原型 const_unop fold-const.h fold-const.cc
tree mtcs_const_const_unop (MtcsConst *self,enum tree_code code, tree type, tree arg0)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   /* Don't perform the operation, other than NEGATE and ABS, if
   flag_signaling_nans is on and the operand is a signaling NaN.  */
   if (TREE_CODE (arg0) == REAL_CST
   && mtcs_mode_honor_snans/*!HONOR_SNANS*/(mtcsMode,arg0)
   && REAL_VALUE_ISSIGNALING_NAN (TREE_REAL_CST (arg0))
   && code != NEGATE_EXPR
   && code != ABS_EXPR
   && code != ABSU_EXPR)
      return NULL_TREE;

   switch (code){
      CASE_CONVERT:
      case FLOAT_EXPR:
      case FIX_TRUNC_EXPR:
      case FIXED_CONVERT_EXPR:
         return fold_convert_const(self,code, type, arg0);

      case ADDR_SPACE_CONVERT_EXPR:
         /* If the source address is 0, and the source address space
         cannot have a valid object at 0, fold to dest type null.  */
         if (integer_zerop (arg0)
         && !(target_addr_space_zero_address_valid/*!targetm.addr_space.zero_address_valid*/(mtcsMachine->addrSpace,
               TYPE_ADDR_SPACE (TREE_TYPE (TREE_TYPE (arg0))))))
            return fold_convert_const(self,code, type, arg0);
         break;

      case VIEW_CONVERT_EXPR:
         return fold_view_convert_expr(self,type, arg0);

      case NEGATE_EXPR:
      {
         /* Can't call fold_negate_const directly here as that doesn't
         handle all cases and we might not be able to negate some
         constants.  */
         tree tem = fold_negate_expr(self,UNKNOWN_LOCATION, arg0);
         if (tem && CONSTANT_CLASS_P (tem))
            return tem;
         break;
      }

      case ABS_EXPR:
      case ABSU_EXPR:
         if (TREE_CODE (arg0) == INTEGER_CST || TREE_CODE (arg0) == REAL_CST)
            return fold_abs_const (arg0, type);
         break;

      case CONJ_EXPR:
         if (TREE_CODE (arg0) == COMPLEX_CST){
            tree ipart = fold_negate_const(self,TREE_IMAGPART (arg0),
            TREE_TYPE (type));
            return mtcs_tree_build_complex/*!build_complex*/(mtcsTree,type, TREE_REALPART (arg0), ipart);
         }
         break;

      case BIT_NOT_EXPR:
         if (TREE_CODE (arg0) == INTEGER_CST)
            return fold_not_const(self,arg0, type);
         else if (POLY_INT_CST_P (arg0))
            return mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,type, -poly_int_cst_value (arg0));
         /* Perform BIT_NOT_EXPR on each element individually.  */
         else if (TREE_CODE (arg0) == VECTOR_CST){
            tree elem;

            /* This can cope with stepped encodings because ~x == -1 - x.  */
            tree_vector_builder elements;
            elements.new_unary_operation (type, arg0, true);
            unsigned int i, count = elements.encoded_nelts ();
            for (i = 0; i < count; ++i){
               elem = VECTOR_CST_ELT (arg0, i);
               elem = const_unop (BIT_NOT_EXPR, TREE_TYPE (type), elem);
               if (elem == NULL_TREE)
                  break;
               elements.quick_push (elem);
            }
            if (i == count)
               return elements.build ();
         }
         break;

      case TRUTH_NOT_EXPR:
         if (TREE_CODE (arg0) == INTEGER_CST)
            return mtcs_const_constant_boolean_node/*!constant_boolean_node*/(self,integer_zerop (arg0), type);
         break;

      case REALPART_EXPR:
         if (TREE_CODE (arg0) == COMPLEX_CST)
            return mtcs_const_fold_convert/*!fold_convert*/(self,type, TREE_REALPART (arg0));
         break;

      case IMAGPART_EXPR:
         if (TREE_CODE (arg0) == COMPLEX_CST)
            return mtcs_const_fold_convert/*!fold_convert*/(self,type, TREE_IMAGPART (arg0));
         break;

      case VEC_UNPACK_LO_EXPR:
      case VEC_UNPACK_HI_EXPR:
      case VEC_UNPACK_FLOAT_LO_EXPR:
      case VEC_UNPACK_FLOAT_HI_EXPR:
      case VEC_UNPACK_FIX_TRUNC_LO_EXPR:
      case VEC_UNPACK_FIX_TRUNC_HI_EXPR:
      {
         unsigned HOST_WIDE_INT out_nelts, in_nelts, i;
         enum tree_code subcode;

         if (TREE_CODE (arg0) != VECTOR_CST)
            return NULL_TREE;

         if (!VECTOR_CST_NELTS (arg0).is_constant (&in_nelts))
            return NULL_TREE;
         out_nelts = in_nelts / 2;
         gcc_assert (known_eq (out_nelts, TYPE_VECTOR_SUBPARTS (type)));

         unsigned int offset = 0;
         if ((!BYTES_BIG_ENDIAN) ^ (code == VEC_UNPACK_LO_EXPR
         || code == VEC_UNPACK_FLOAT_LO_EXPR
         || code == VEC_UNPACK_FIX_TRUNC_LO_EXPR))
         offset = out_nelts;

         if (code == VEC_UNPACK_LO_EXPR || code == VEC_UNPACK_HI_EXPR)
            subcode = NOP_EXPR;
         else if (code == VEC_UNPACK_FLOAT_LO_EXPR  || code == VEC_UNPACK_FLOAT_HI_EXPR)
            subcode = FLOAT_EXPR;
         else
            subcode = FIX_TRUNC_EXPR;

         tree_vector_builder elts (type, out_nelts, 1);
         for (i = 0; i < out_nelts; i++){
            tree elt = fold_convert_const(self,subcode, TREE_TYPE (type),VECTOR_CST_ELT (arg0, i + offset));
            if (elt == NULL_TREE || !CONSTANT_CLASS_P (elt))
               return NULL_TREE;
            elts.quick_push (elt);
         }

         return elts.build ();
      }

      case VEC_DUPLICATE_EXPR:
         if (CONSTANT_CLASS_P (arg0))
            return build_vector_from_val (type, arg0);
         return NULL_TREE;

      default:
         break;
   }

   return NULL_TREE;
}


/* Fold a unary expression of code CODE and type TYPE with operand
   OP0.  Return the folded expression if folding is successful.
   Otherwise, return NULL_TREE.  */
//原型 fold_unary_loc fold-const.h fold-const.cc
tree mtcs_const_fold_unary_loc (MtcsConst *self,location_t loc, enum tree_code code, tree type, tree op0)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   tree tem;
   tree arg0;
   enum tree_code_class kind = TREE_CODE_CLASS (code);

   gcc_assert (IS_EXPR_CODE_CLASS (kind)  && TREE_CODE_LENGTH (code) == 1);

   arg0 = op0;
   if (arg0){
      if (CONVERT_EXPR_CODE_P (code) || code == FLOAT_EXPR || code == ABS_EXPR || code == NEGATE_EXPR){
         /* Don't use STRIP_NOPS, because signedness of argument type
         matters.  */
         STRIP_SIGN_NOPS (arg0);
      }else{
         /* Strip any conversions that don't change the mode.  This
         is safe for every expression, except for a comparison
         expression because its signedness is derived from its
         operands.

         Note that this is done as an internal manipulation within
         the constant folder, in order to find the simplest
         representation of the arguments so that their form can be
         studied.  In any cases, the appropriate type conversions
         should be put back in the tree that will get out of the
         constant folder.  */
         STRIP_NOPS (arg0);
      }

      if (CONSTANT_CLASS_P (arg0)){
         tree tem = const_unop (code, type, arg0);
         if (tem){
            if (TREE_TYPE (tem) != type)
               tem = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, tem);
            return tem;
         }
      }
   }

   tem = generic_simplify (loc, code, type, op0);
   if (tem)
      return tem;

   if (TREE_CODE_CLASS (code) == tcc_unary){
      if (TREE_CODE (arg0) == COMPOUND_EXPR)
         return build2 (COMPOUND_EXPR, type, TREE_OPERAND (arg0, 0),
               mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc,
                     code, type,mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, TREE_TYPE (op0),TREE_OPERAND (arg0, 1))));
      else if (TREE_CODE (arg0) == COND_EXPR){
         tree arg01 = TREE_OPERAND (arg0, 1);
         tree arg02 = TREE_OPERAND (arg0, 2);
         if (! VOID_TYPE_P (TREE_TYPE (arg01)))
            arg01 = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, code,
                  type,mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc,TREE_TYPE (op0), arg01));
         if (! VOID_TYPE_P (TREE_TYPE (arg02)))
            arg02 = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, code, type,mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc,TREE_TYPE (op0), arg02));
         tem = fold_build3_loc (loc, COND_EXPR, type, TREE_OPERAND (arg0, 0),arg01, arg02);

         /* If this was a conversion, and all we did was to move into
         inside the COND_EXPR, bring it back out.  But leave it if
         it is a conversion from integer to integer and the
         result precision is no wider than a word since such a
         conversion is cheap and may be optimized away by combine,
         while it couldn't if it were outside the COND_EXPR.  Then return
         so we don't get into an infinite recursion loop taking the
         conversion out and then back in.  */

         if ((CONVERT_EXPR_CODE_P (code)
         || code == NON_LVALUE_EXPR)
         && TREE_CODE (tem) == COND_EXPR
         && TREE_CODE (TREE_OPERAND (tem, 1)) == code
         && TREE_CODE (TREE_OPERAND (tem, 2)) == code
         && ! VOID_TYPE_P (TREE_TYPE (TREE_OPERAND (tem, 1)))
         && ! VOID_TYPE_P (TREE_TYPE (TREE_OPERAND (tem, 2)))
         && (TREE_TYPE (TREE_OPERAND (TREE_OPERAND (tem, 1), 0))
         == TREE_TYPE (TREE_OPERAND (TREE_OPERAND (tem, 2), 0)))
         && (! (INTEGRAL_TYPE_P (TREE_TYPE (tem))
         && (INTEGRAL_TYPE_P
         (TREE_TYPE (TREE_OPERAND (TREE_OPERAND (tem, 1), 0))))
         && TYPE_PRECISION (TREE_TYPE (tem)) <= BITS_PER_WORD)
         || flag_syntax_only))
         tem = build1_loc (loc, code, type,build3 (COND_EXPR,
            TREE_TYPE (TREE_OPERAND(TREE_OPERAND (tem, 1), 0)), TREE_OPERAND (tem, 0),
            TREE_OPERAND (TREE_OPERAND (tem, 1), 0),TREE_OPERAND (TREE_OPERAND (tem, 2),0)));
         return tem;
      }
   }

   switch (code){
      case NON_LVALUE_EXPR:
         if (!maybe_lvalue_p (op0))
            return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, op0);
         return NULL_TREE;

      CASE_CONVERT:
      case FLOAT_EXPR:
      case FIX_TRUNC_EXPR:
         if (COMPARISON_CLASS_P (op0)){
            /* If we have (type) (a CMP b) and type is an integral type, return
            new expression involving the new type.  Canonicalize
            (type) (a CMP b) to (a CMP b) ? (type) true : (type) false for
            non-integral type.
            Do not fold the result as that would not simplify further, also
            folding again results in recursions.  */
            if (TREE_CODE (type) == BOOLEAN_TYPE)
               return build2_loc (loc, TREE_CODE (op0), type,TREE_OPERAND (op0, 0), TREE_OPERAND (op0, 1));
            else if (!INTEGRAL_TYPE_P (type) && !VOID_TYPE_P (type) && TREE_CODE (type) != VECTOR_TYPE)
               return build3_loc (loc, COND_EXPR, type, op0, mtcs_const_constant_boolean_node/*!constant_boolean_node*/(self,true, type),mtcs_const_constant_boolean_node/*!constant_boolean_node*/(self,false, type));
         }

         /* Handle (T *)&A.B.C for A being of type T and B and C
         living at offset zero.  This occurs frequently in
         C++ upcasting and then accessing the base.  */
         if (TREE_CODE (op0) == ADDR_EXPR  && POINTER_TYPE_P (type) && handled_component_p (TREE_OPERAND (op0, 0))){
            poly_int64 bitsize, bitpos;
            tree offset;
            machine_mode mode;
            int unsignedp, reversep, volatilep;
            tree base  = mtcs_expr_get_inner_reference/*!get_inner_reference*/(mtcsExpr,
                  TREE_OPERAND (op0, 0), &bitsize, &bitpos,&offset, &mode, &unsignedp, &reversep,&volatilep);
            /* If the reference was to a (constant) zero offset, we can use
            the address of the base if it has the same base type
            as the result type and the pointer type is unqualified.  */
            if (!offset
            && known_eq (bitpos, 0)
            && (TYPE_MAIN_VARIANT (TREE_TYPE (type))
            == TYPE_MAIN_VARIANT (TREE_TYPE (base)))
            && TYPE_QUALS (type) == TYPE_UNQUALIFIED)
               return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type,
                     mtcs_const_build_fold_addr_expr_loc/*!build_fold_addr_expr_loc*/(self,loc, base));
         }

         if (TREE_CODE (op0) == MODIFY_EXPR
         && TREE_CONSTANT (TREE_OPERAND (op0, 1))
         /* Detect assigning a bitfield.  */
         && !(TREE_CODE (TREE_OPERAND (op0, 0)) == COMPONENT_REF
         && DECL_BIT_FIELD(TREE_OPERAND (TREE_OPERAND (op0, 0), 1)))){
            /* Don't leave an assignment inside a conversion
            unless assigning a bitfield.  */
            tem = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, code, type, TREE_OPERAND (op0, 1));
            /* First do the assignment, then return converted constant.  */
            tem = build2_loc (loc, COMPOUND_EXPR, TREE_TYPE (tem), op0, tem);
            suppress_warning (tem /* What warning? */);
            TREE_USED (tem) = 1;
            return tem;
         }

         /* Convert (T)(x & c) into (T)x & (T)c, if c is an integer
         constants (if x has signed type, the sign bit cannot be set
         in c).  This folds extension into the BIT_AND_EXPR.
         ??? We don't do it for BOOLEAN_TYPE or ENUMERAL_TYPE because they
         very likely don't have maximal range for their precision and this
         transformation effectively doesn't preserve non-maximal ranges.  */
         if (TREE_CODE (type) == INTEGER_TYPE
         && TREE_CODE (op0) == BIT_AND_EXPR
         && TREE_CODE (TREE_OPERAND (op0, 1)) == INTEGER_CST){
            tree and_expr = op0;
            tree and0 = TREE_OPERAND (and_expr, 0);
            tree and1 = TREE_OPERAND (and_expr, 1);
            int change = 0;

            if (TYPE_UNSIGNED (TREE_TYPE (and_expr)) || (TYPE_PRECISION (type) <= TYPE_PRECISION (TREE_TYPE (and_expr))))
               change = 1;
            else if (TYPE_PRECISION (TREE_TYPE (and1)) <= HOST_BITS_PER_WIDE_INT && tree_fits_uhwi_p (and1)){
               unsigned HOST_WIDE_INT cst;

               cst = tree_to_uhwi (and1);
               cst &= HOST_WIDE_INT_M1U << (TYPE_PRECISION (TREE_TYPE (and1)) - 1);
               change = (cst == 0);
               if (change && !flag_syntax_only && (mtcs_rtl_load_extend_op/*!load_extend_op*/(mtcsRTL,
                     TYPE_MODE (TREE_TYPE (and0))) == ZERO_EXTEND)){
                  tree uns = unsigned_type_for (TREE_TYPE (and0));
                  and0 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, uns, and0);
                  and1 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, uns, and1);
               }
            }
            if (change){
               tree and1_type = TREE_TYPE (and1);
               unsigned prec = MAX (TYPE_PRECISION (and1_type),TYPE_PRECISION (type));
               tem = force_fit_type (type, wide_int::from (wi::to_wide (and1), prec,TYPE_SIGN (and1_type)),
               0, TREE_OVERFLOW (and1));
               return mtcs_const_fold_build2_loc(self,loc, BIT_AND_EXPR, type,
                     mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, and0), tem);
            }
         }

         /* Convert (T1)(X p+ Y) into ((T1)X p+ Y), for pointer type, when the new
         cast (T1)X will fold away.  We assume that this happens when X itself
         is a cast.  */
         if (POINTER_TYPE_P (type)
         && TREE_CODE (arg0) == POINTER_PLUS_EXPR
         && CONVERT_EXPR_P (TREE_OPERAND (arg0, 0))){
            tree arg00 = TREE_OPERAND (arg0, 0);
            tree arg01 = TREE_OPERAND (arg0, 1);

            /* If -fsanitize=alignment, avoid this optimization in GENERIC
            when the pointed type needs higher alignment than
            the p+ first operand's pointed type.  */
            if (!in_gimple_form
            && sanitize_flags_p (SANITIZE_ALIGNMENT)
            && (min_align_of_type (TREE_TYPE (type)) > min_align_of_type (TREE_TYPE (TREE_TYPE (arg00)))))
               return NULL_TREE;

            /* Similarly, avoid this optimization in GENERIC for -fsanitize=null
            when type is a reference type and arg00's type is not,
            because arg00 could be validly nullptr and if arg01 doesn't return,
            we don't want false positive binding of reference to nullptr.  */
            if (TREE_CODE (type) == REFERENCE_TYPE
            && !in_gimple_form
            && sanitize_flags_p (SANITIZE_NULL)
            && TREE_CODE (TREE_TYPE (arg00)) != REFERENCE_TYPE)
               return NULL_TREE;

            arg00 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, arg00);
            return fold_build_pointer_plus_loc (loc, arg00, arg01);
         }

         /* Convert (T1)(~(T2)X) into ~(T1)X if T1 and T2 are integral types
         of the same precision, and X is an integer type not narrower than
         types T1 or T2, i.e. the cast (T2)X isn't an extension.  */
         if (INTEGRAL_TYPE_P (type)
         && TREE_CODE (op0) == BIT_NOT_EXPR
         && INTEGRAL_TYPE_P (TREE_TYPE (op0))
         && CONVERT_EXPR_P (TREE_OPERAND (op0, 0))
         && TYPE_PRECISION (type) == TYPE_PRECISION (TREE_TYPE (op0))){
            tem = TREE_OPERAND (TREE_OPERAND (op0, 0), 0);
            if (INTEGRAL_TYPE_P (TREE_TYPE (tem))  && TYPE_PRECISION (type) <= TYPE_PRECISION (TREE_TYPE (tem)))
               return mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,
                     loc, BIT_NOT_EXPR, type, mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, tem));
         }

         /* Convert (T1)(X * Y) into (T1)X * (T1)Y if T1 is narrower than the
         type of X and Y (integer types only).  */
         if (INTEGRAL_TYPE_P (type)
         && TREE_CODE (op0) == MULT_EXPR
         && INTEGRAL_TYPE_P (TREE_TYPE (op0))
         && TYPE_PRECISION (type) < TYPE_PRECISION (TREE_TYPE (op0))
         && (TYPE_OVERFLOW_WRAPS (TREE_TYPE (op0)) || !sanitize_flags_p (SANITIZE_SI_OVERFLOW))){
            /* Be careful not to introduce new overflows.  */
            tree mult_type;
            if (TYPE_OVERFLOW_WRAPS (type))
               mult_type = type;
            else
               mult_type = unsigned_type_for (type);

            if (TYPE_PRECISION (mult_type) < TYPE_PRECISION (TREE_TYPE (op0))){
               tem = mtcs_const_fold_build2_loc(self,loc, MULT_EXPR, mult_type,
               mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, mult_type,TREE_OPERAND (op0, 0)),
               mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, mult_type,TREE_OPERAND (op0, 1)));
               return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, tem);
            }
         }

         return NULL_TREE;

      case VIEW_CONVERT_EXPR:
         if (TREE_CODE (op0) == MEM_REF){
            if (TYPE_ALIGN (TREE_TYPE (op0)) != TYPE_ALIGN (type))
               type = build_aligned_type (type, TYPE_ALIGN (TREE_TYPE (op0)));
            tem = mtcs_const_fold_build2_loc(self,loc, MEM_REF, type,TREE_OPERAND (op0, 0), TREE_OPERAND (op0, 1));
            REF_REVERSE_STORAGE_ORDER (tem) = REF_REVERSE_STORAGE_ORDER (op0);
            return tem;
         }

         return NULL_TREE;

      case NEGATE_EXPR:
         tem = fold_negate_expr(self,loc, arg0);
         if (tem)
            return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, tem);
         return NULL_TREE;

      case ABS_EXPR:
         /* Convert fabs((double)float) into (double)fabsf(float).  */
         if (TREE_CODE (arg0) == NOP_EXPR && TREE_CODE (type) == REAL_TYPE){
            tree targ0 = strip_float_extensions (arg0);
            if (targ0 != arg0)
               return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc,
                     type,mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, ABS_EXPR,TREE_TYPE (targ0),targ0));
         }
         return NULL_TREE;

      case BIT_NOT_EXPR:
         /* Convert ~(X ^ Y) to ~X ^ Y or X ^ ~Y if ~X or ~Y simplify.  */
         if (TREE_CODE (arg0) == BIT_XOR_EXPR
         && (tem = mtcs_const_fold_unary_loc/*!fold_unary_loc*/(self,loc, BIT_NOT_EXPR, type,
               mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, TREE_OPERAND (arg0, 0)))))
            return mtcs_const_fold_build2_loc(self,loc, BIT_XOR_EXPR, type, tem,
                  mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, TREE_OPERAND (arg0, 1)));
         else if (TREE_CODE (arg0) == BIT_XOR_EXPR
         && (tem =  mtcs_const_fold_unary_loc/*!fold_unary_loc*/(self,loc, BIT_NOT_EXPR, type,
               mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type,TREE_OPERAND (arg0, 1)))))
            return mtcs_const_fold_build2_loc(self,loc, BIT_XOR_EXPR, type,
                  mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type,TREE_OPERAND (arg0, 0)), tem);

         return NULL_TREE;

      case TRUTH_NOT_EXPR:
         /* Note that the operand of this must be an int
         and its values must be 0 or 1.
         ("true" is a fixed value perhaps depending on the language,
         but we don't handle values other than 1 correctly yet.)  */
         tem = fold_truth_not_expr(self,loc, arg0);
         if (!tem)
            return NULL_TREE;
         return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, tem);

      case INDIRECT_REF:
         /* Fold *&X to X if X is an lvalue.  */
         if (TREE_CODE (op0) == ADDR_EXPR){
            tree op00 = TREE_OPERAND (op0, 0);
            if ((VAR_P (op00) || TREE_CODE (op00) == PARM_DECL  || TREE_CODE (op00) == RESULT_DECL) && !TREE_READONLY (op00))
               return op00;
         }
         return NULL_TREE;

      default:
         return NULL_TREE;
   } /* switch (code) */
}


/* Fold a unary tree expression with code CODE of type TYPE with an
   operand OP0.  LOC is the location of the resulting expression.
   Return a folded expression if successful.  Otherwise, return a tree
   expression with code CODE of type TYPE with an operand OP0.  */
//原型 fold_build1_loc fold-const.h fold-const.cc
tree mtcs_const_fold_build1_loc (MtcsConst *self,location_t loc,  enum tree_code code, tree type, tree op0 MEM_STAT_DECL)
{
  tree tem;
#ifdef ENABLE_FOLD_CHECKING
  unsigned char checksum_before[16], checksum_after[16];
  struct md5_ctx ctx;
  hash_table<nofree_ptr_hash<const tree_node> > ht (32);

  md5_init_ctx (&ctx);
  fold_checksum_tree (op0, &ctx, &ht);
  md5_finish_ctx (&ctx, checksum_before);
  ht.empty ();
#endif

  tem =  mtcs_const_fold_unary_loc/*!fold_unary_loc*/(self,loc, code, type, op0);
  if (!tem)
    tem = build1_loc (loc, code, type, op0 PASS_MEM_STAT);

#ifdef ENABLE_FOLD_CHECKING
  md5_init_ctx (&ctx);
  fold_checksum_tree (op0, &ctx, &ht);
  md5_finish_ctx (&ctx, checksum_after);

  if (memcmp (checksum_before, checksum_after, 16))
    fold_check_failed (op0, tem);
#endif
  return tem;
}

/* Convert expression ARG to type TYPE.  Used by the middle-end for
   simple conversions in preference to calling the front-end's convert.  */
//原型 fold_convert_loc fold-const.h fold-const.cc
tree mtcs_const_fold_convert_loc (MtcsConst *self,location_t loc, tree type, tree arg)
{
   tree orig = TREE_TYPE (arg);
   tree tem;

   if (type == orig)
      return arg;

   if (TREE_CODE (arg) == ERROR_MARK || TREE_CODE (type) == ERROR_MARK || TREE_CODE (orig) == ERROR_MARK)
      return error_mark_node;

   switch (TREE_CODE (type)){
      case POINTER_TYPE:
      case REFERENCE_TYPE:
         /* Handle conversions between pointers to different address spaces.  */
         if (POINTER_TYPE_P (orig)  && (TYPE_ADDR_SPACE (TREE_TYPE (type)) != TYPE_ADDR_SPACE (TREE_TYPE (orig))))
            return mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, ADDR_SPACE_CONVERT_EXPR, type, arg);
         /* fall through */

      case INTEGER_TYPE:
      case ENUMERAL_TYPE:
      case BOOLEAN_TYPE:
      case OFFSET_TYPE:
      case BITINT_TYPE:
         if (TREE_CODE (arg) == INTEGER_CST){
            tem = fold_convert_const(self,NOP_EXPR, type, arg);
            if (tem != NULL_TREE)
               return tem;
         }
         if (INTEGRAL_TYPE_P (orig) || POINTER_TYPE_P (orig)  || TREE_CODE (orig) == OFFSET_TYPE)
            return mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, NOP_EXPR, type, arg);
         if (TREE_CODE (orig) == COMPLEX_TYPE)
            return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type,
                  mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, REALPART_EXPR, TREE_TYPE (orig), arg));
         gcc_assert (VECTOR_TYPE_P (orig) && tree_int_cst_equal (TYPE_SIZE (type), TYPE_SIZE (orig)));
         return mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, VIEW_CONVERT_EXPR, type, arg);

      case REAL_TYPE:
         if (TREE_CODE (arg) == INTEGER_CST){
            tem = fold_convert_const(self,FLOAT_EXPR, type, arg);
            if (tem != NULL_TREE)
               return tem;
         }else if (TREE_CODE (arg) == REAL_CST){
            tem = fold_convert_const(self,NOP_EXPR, type, arg);
            if (tem != NULL_TREE)
               return tem;
         }else if (TREE_CODE (arg) == FIXED_CST){
            tem = fold_convert_const(self,FIXED_CONVERT_EXPR, type, arg);
            if (tem != NULL_TREE)
               return tem;
         }

         switch (TREE_CODE (orig)){
            case INTEGER_TYPE:
            case BITINT_TYPE:
            case BOOLEAN_TYPE:
            case ENUMERAL_TYPE:
            case POINTER_TYPE:
            case REFERENCE_TYPE:
               return mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, FLOAT_EXPR, type, arg);

            case REAL_TYPE:
               return mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, NOP_EXPR, type, arg);

            case FIXED_POINT_TYPE:
               return mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, FIXED_CONVERT_EXPR, type, arg);

            case COMPLEX_TYPE:
               tem = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, REALPART_EXPR, TREE_TYPE (orig), arg);
               return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, tem);

            default:
               gcc_unreachable ();
         }

      case FIXED_POINT_TYPE:
         if (TREE_CODE (arg) == FIXED_CST || TREE_CODE (arg) == INTEGER_CST
         || TREE_CODE (arg) == REAL_CST){
            tem = fold_convert_const(self,FIXED_CONVERT_EXPR, type, arg);
            if (tem != NULL_TREE)
               goto fold_convert_exit;
         }

         switch (TREE_CODE (orig)){
            case FIXED_POINT_TYPE:
            case INTEGER_TYPE:
            case ENUMERAL_TYPE:
            case BOOLEAN_TYPE:
            case REAL_TYPE:
            case BITINT_TYPE:
               return mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, FIXED_CONVERT_EXPR, type, arg);

            case COMPLEX_TYPE:
               tem = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, REALPART_EXPR, TREE_TYPE (orig), arg);
               return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, tem);

            default:
            gcc_unreachable ();
         }

      case COMPLEX_TYPE:
         switch (TREE_CODE (orig)){
            case INTEGER_TYPE:
            case BITINT_TYPE:
            case BOOLEAN_TYPE:
            case ENUMERAL_TYPE:
            case POINTER_TYPE:
            case REFERENCE_TYPE:
            case REAL_TYPE:
            case FIXED_POINT_TYPE:
               return mtcs_const_fold_build2_loc(self,loc, COMPLEX_EXPR, type,
                     mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, TREE_TYPE (type), arg),
                     mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, TREE_TYPE (type),integer_zero_node));
            case COMPLEX_TYPE:
            {
               tree rpart, ipart;

               if (TREE_CODE (arg) == COMPLEX_EXPR){
                  rpart = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, TREE_TYPE (type),TREE_OPERAND (arg, 0));
                  ipart = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, TREE_TYPE (type),TREE_OPERAND (arg, 1));
                  return mtcs_const_fold_build2_loc(self,loc, COMPLEX_EXPR, type, rpart, ipart);
               }

               arg = save_expr (arg);
               rpart = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, REALPART_EXPR, TREE_TYPE (orig), arg);
               ipart = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, IMAGPART_EXPR, TREE_TYPE (orig), arg);
               rpart = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, TREE_TYPE (type), rpart);
               ipart = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, TREE_TYPE (type), ipart);
               return mtcs_const_fold_build2_loc(self,loc, COMPLEX_EXPR, type, rpart, ipart);
            }

            default:
               gcc_unreachable ();
         }

      case VECTOR_TYPE:
         if (integer_zerop (arg))
            return build_zero_vector(self,type);
         gcc_assert (tree_int_cst_equal (TYPE_SIZE (type), TYPE_SIZE (orig)));
         gcc_assert (INTEGRAL_TYPE_P (orig) || POINTER_TYPE_P (orig) || VECTOR_TYPE_P (orig));
         return mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, VIEW_CONVERT_EXPR, type, arg);

      case VOID_TYPE:
         tem = fold_ignored_result (arg);
         return mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, NOP_EXPR, type, tem);

      default:
         if (TYPE_MAIN_VARIANT (type) == TYPE_MAIN_VARIANT (orig))
            return mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, NOP_EXPR, type, arg);
         gcc_unreachable ();
   }

fold_convert_exit:
   tem = protected_set_expr_location_unshare (tem, loc);
   return tem;
}

//原型  #define fold_convert(T1,T2)   fold_convert_loc (UNKNOWN_LOCATION, T1, T2) fold-const.h
tree mtcs_const_fold_convert(MtcsConst *self,tree type, tree arg)
{
   return mtcs_const_fold_convert_loc(self,UNKNOWN_LOCATION,type,arg);
}

//原型  build_fold_addr_expr_with_type_loc fold-const.h fold-const.cc
tree mtcs_const_build_fold_addr_expr_with_type_loc (MtcsConst *self,location_t loc, tree t, tree ptrtype)
{
   /* The size of the object is not relevant when talking about its address.  */
   if (TREE_CODE (t) == WITH_SIZE_EXPR)
      t = TREE_OPERAND (t, 0);

   if (INDIRECT_REF_P (t)) {
      t = TREE_OPERAND (t, 0);
      if (TREE_TYPE (t) != ptrtype)
         t = build1_loc (loc, NOP_EXPR, ptrtype, t);
   }else if (TREE_CODE (t) == MEM_REF  && integer_zerop (TREE_OPERAND (t, 1))){
      t = TREE_OPERAND (t, 0);
      if (TREE_TYPE (t) != ptrtype)
      t = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, ptrtype, t);
   }else if (TREE_CODE (t) == MEM_REF  && TREE_CODE (TREE_OPERAND (t, 0)) == INTEGER_CST)
      return mtcs_const_fold_binary/*!fold_binary*/(self,POINTER_PLUS_EXPR,
            ptrtype,TREE_OPERAND (t, 0),convert_to_ptrofftype (TREE_OPERAND (t, 1)));
   else if (TREE_CODE (t) == VIEW_CONVERT_EXPR){
      t = mtcs_const_build_fold_addr_expr_loc/*!build_fold_addr_expr_loc*/(self,loc, TREE_OPERAND (t, 0));
      if (TREE_TYPE (t) != ptrtype)
         t = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, ptrtype, t);
   }else
      t = build1_loc (loc, ADDR_EXPR, ptrtype, t);

   return t;
}


//原型 #define build_fold_addr_expr(T)  build_fold_addr_expr_loc (UNKNOWN_LOCATION, (T)) fold-const.h
tree mtcs_const_build_fold_addr_expr (MtcsConst *self,tree t)
{
   return mtcs_const_build_fold_addr_expr_loc(self,UNKNOWN_LOCATION,t);
}

//原型  build_fold_addr_expr_loc fold-const.h fold-const.cc
tree mtcs_const_build_fold_addr_expr_loc (MtcsConst *self,location_t loc, tree t)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree ptrtype = mtcs_tree_build_pointer_type/*!build_pointer_type*/(mtcsTree,TREE_TYPE (t));
   return mtcs_const_build_fold_addr_expr_with_type_loc/*!build_fold_addr_expr_with_type_loc*/(self,loc, t, ptrtype);
}

/* Combine operands OP1 and OP2 with arithmetic operation CODE.  CODE
   is a tree code.  The type of the result is taken from the operands.
   Both must be equivalent integer types, ala int_binop_types_match_p.
   If the operands are constant, so is the result.  */
//原型 size_binop_loc fold-const.h fold-const.cc
tree mtcs_const_size_binop_loc (MtcsConst *self,location_t loc, enum tree_code code, tree arg0, tree arg1)
{
   tree type = TREE_TYPE (arg0);

   if (arg0 == error_mark_node || arg1 == error_mark_node)
      return error_mark_node;
   gcc_assert (int_binop_types_match_p (code, TREE_TYPE (arg0),TREE_TYPE (arg1)));

   /* Handle the special case of two poly_int constants faster.  */
   if (poly_int_tree_p (arg0) && poly_int_tree_p (arg1)){
      /* And some specific cases even faster than that.  */
      if (code == PLUS_EXPR){
         if (integer_zerop (arg0)  && !TREE_OVERFLOW (tree_strip_any_location_wrapper (arg0)))
            return arg1;
         if (integer_zerop (arg1)   && !TREE_OVERFLOW (tree_strip_any_location_wrapper (arg1)))
            return arg0;
      }else if (code == MINUS_EXPR){
         if (integer_zerop (arg1)   && !TREE_OVERFLOW (tree_strip_any_location_wrapper (arg1)))
            return arg0;
      }else if (code == MULT_EXPR){
         if (integer_onep (arg0)  && !TREE_OVERFLOW (tree_strip_any_location_wrapper (arg0)))
            return arg1;
      }
      /* Handle general case of two integer constants.  For sizetype
      constant calculations we always want to know about overflow,
      even in the unsigned case.  */
      tree res = mtcs_const_int_const_binop/*!int_const_binop*/(self,code, arg0, arg1, -1);
      if (res != NULL_TREE)
         return res;
   }
   return mtcs_const_fold_build2_loc(self,loc, code, type, arg0, arg1);
}


/* Combine two integer constants ARG1 and ARG2 under operation CODE to
   produce a new constant.  Return NULL_TREE if we don't know how to
   evaluate CODE at compile-time.  */
//原型 int_const_binop fold-const.h fold-const.cc
tree mtcs_const_int_const_binop (MtcsConst *self,enum tree_code code, const_tree arg1, const_tree arg2,int overflowable)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree   *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   poly_wide_int poly_res;
   tree type = TREE_TYPE (arg1);
   signop sign = TYPE_SIGN (type);
   wi::overflow_type overflow = wi::OVF_NONE;
   if (TREE_CODE (arg1) == INTEGER_CST && TREE_CODE (arg2) == INTEGER_CST){
      wide_int warg1 = wi::to_wide (arg1), res;
      wide_int warg2 = wi::to_wide (arg2, TYPE_PRECISION (type));
      if (!wide_int_binop (res, code, warg1, warg2, sign, &overflow))
         return NULL_TREE;
      poly_res = res;
   }else if (!poly_int_tree_p (arg1) || !poly_int_tree_p (arg2) || !poly_int_binop (poly_res, code, arg1, arg2, sign, &overflow))
      return NULL_TREE;
   return mtcs_tree_force_fit_type/*!force_fit_type*/(mtcsTree,type, poly_res, overflowable,
         (((sign == SIGNED || overflowable == -1) && overflow) | TREE_OVERFLOW (arg1) | TREE_OVERFLOW (arg2)));
}

/* Return a node which has the indicated constant VALUE (either 0 or
   1 for scalars or {-1,-1,..} or {0,0,...} for vectors),
   and is of the indicated TYPE.  */
//原型 constant_boolean_node fold-const.h fold-const.cc
tree mtcs_const_constant_boolean_node (MtcsConst *self,bool value, tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   if (type == integer_type_node)
      return value ? integer_one_node : integer_zero_node;
   else if (type == boolean_type_node)
      return value ? boolean_true_node : boolean_false_node;
   else if (VECTOR_TYPE_P (type))
      return build_vector_from_val (type, mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (type),value ? -1 : 0));
   else
      return mtcs_const_fold_convert/*!fold_convert*/(self,type, value ? integer_one_node : integer_zero_node);
}


/* Subroutine of fold_view_convert_expr.  Encode the INTEGER_CST, REAL_CST,
   FIXED_CST, COMPLEX_CST, STRING_CST, or VECTOR_CST specified by EXPR into
   the buffer PTR of size LEN bytes.  If PTR is NULL, don't actually store
   anything, just do a dry run.  Fail either if OFF is -1 and LEN isn't
   sufficient to encode the entire EXPR, or if OFF is out of bounds.
   Otherwise, start at byte offset OFF and encode at most LEN bytes.
   Return the number of bytes placed in the buffer, or zero upon failure.  */
//原型 native_encode_expr fold-const.h fold-const.cc
int mtcs_const_native_encode_expr (MtcsConst *self,const_tree expr, unsigned char *ptr, int len, int off)
{
   /* We don't support starting at negative offset and -1 is special.  */
   if (off < -1)
      return 0;

   switch (TREE_CODE (expr)){
      case INTEGER_CST:
         return native_encode_int(self,expr, ptr, len, off);

      case REAL_CST:
         return native_encode_real(self,expr, ptr, len, off);

      case FIXED_CST:
         return native_encode_fixed(self,expr, ptr, len, off);

      case COMPLEX_CST:
         return native_encode_complex(self,expr, ptr, len, off);

      case VECTOR_CST:
         return native_encode_vector(self,expr, ptr, len, off);

      case STRING_CST:
         return native_encode_string(self,expr, ptr, len, off);

      default:
         return 0;
   }
}


/* Subroutine of native_interpret_expr.  Interpret the contents of
   the buffer PTR of length LEN as an INTEGER_CST of type TYPE.
   If the buffer cannot be interpreted, return NULL_TREE.  */
//原型 native_interpret_int fold-const.cc
static tree native_interpret_int (MtcsConst *self,tree type, const unsigned char *ptr, int len)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   int total_bytes;
   if (TREE_CODE (type) == BITINT_TYPE){
      struct bitint_info info;
      bool ok =target_c_bitint_type_info/*!targetm.c.bitint_type_info*/(mtcsMachine->c,TYPE_PRECISION (type), &info);
      gcc_assert (ok);
      scalar_int_mode limb_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,info.limb_mode);
      if (TYPE_PRECISION (type) > mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,limb_mode)){
         total_bytes = tree_to_uhwi (TYPE_SIZE_UNIT (type));
         /* More work is needed when adding _BitInt support to PDP endian
         if limb is smaller than word, or if _BitInt limb ordering doesn't
         match target endianity here.  */
         gcc_checking_assert (info.big_endian == WORDS_BIG_ENDIAN && (BYTES_BIG_ENDIAN == WORDS_BIG_ENDIAN
               || (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,limb_mode) >= UNITS_PER_WORD)));
      }else
         total_bytes = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,type));
   }else
      total_bytes =mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,type));

   if (total_bytes > len)
      return NULL_TREE;

   wide_int result = wi::from_buffer (ptr, total_bytes);

   return mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,type, result);
}


/* Subroutine of native_interpret_expr.  Interpret the contents of
   the buffer PTR of length LEN as a FIXED_CST of type TYPE.
   If the buffer cannot be interpreted, return NULL_TREE.  */
//原型 native_interpret_fixed  fold-const.cc
static tree native_interpret_fixed (MtcsConst *self,tree type, const unsigned char *ptr, int len)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFixed *mtcsFixed=mtcs_target_get_fixed(mtcsTarget);

   scalar_mode mode = mtcs_mode_scalar_type_mode/*!SCALAR_TYPE_MODE*/(mtcsMode,type);
   int total_bytes = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);
   double_int result;
   FIXED_VALUE_TYPE fixed_value;

   if (total_bytes > len || total_bytes * BITS_PER_UNIT > HOST_BITS_PER_DOUBLE_INT)
      return NULL_TREE;

   result = double_int::from_buffer (ptr, total_bytes);
   fixed_value = mtcs_fixed_fixed_from_double_int/*!fixed_from_double_int*/(mtcsFixed,result, mode);

   return build_fixed (type, fixed_value);
}

/* Subroutine of native_interpret_expr.  Interpret the contents of
   the buffer PTR of length LEN as a COMPLEX_CST of type TYPE.
   If the buffer cannot be interpreted, return NULL_TREE.  */
//原型 native_interpret_complex  fold-const.cc
static tree native_interpret_complex (MtcsConst *self,tree type, const unsigned char *ptr, int len)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree etype, rpart, ipart;
   int size;

   etype = TREE_TYPE (type);
   size = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mtcs_mode_scalar_type_mode/*!SCALAR_TYPE_MODE*/(mtcsMode,etype));
   if (size * 2 > len)
      return NULL_TREE;
   rpart = mtcs_const_native_interpret_expr/*!native_interpret_expr*/(self,etype, ptr, size);
   if (!rpart)
      return NULL_TREE;
   ipart = mtcs_const_native_interpret_expr/*!native_interpret_expr*/(self,etype, ptr+size, size);
   if (!ipart)
      return NULL_TREE;
   return mtcs_tree_build_complex/*!build_complex*/(mtcsTree,type, rpart, ipart);
}

/* Subroutine of native_interpret_expr.  Interpret the contents of
   the buffer PTR of length LEN as a VECTOR_CST of type TYPE.
   If the buffer cannot be interpreted, return NULL_TREE.  */
//原型 native_interpret_vector  fold-const.cc

static tree native_interpret_vector (MtcsConst *self,tree type, const unsigned char *ptr, unsigned int len)
{
   unsigned HOST_WIDE_INT size;

   if (!tree_to_poly_uint64 (TYPE_SIZE_UNIT (type)).is_constant (&size) || size > len)
      return NULL_TREE;

   unsigned HOST_WIDE_INT count = TYPE_VECTOR_SUBPARTS (type).to_constant ();
   return native_interpret_vector_part(self,type, ptr, len, count, 1);
}


/* Subroutine of fold_view_convert_expr.  Interpret the contents of
   the buffer PTR of length LEN as a constant of type TYPE.  For
   INTEGRAL_TYPE_P we return an INTEGER_CST, for SCALAR_FLOAT_TYPE_P
   we return a REAL_CST, etc...  If the buffer cannot be interpreted,
   return NULL_TREE.  */
//原型 native_interpret_expr fold-const.h fold-const.cc
tree mtcs_const_native_interpret_expr (MtcsConst *self,tree type, const unsigned char *ptr, int len)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   switch (TREE_CODE (type)){
      case INTEGER_TYPE:
      case ENUMERAL_TYPE:
      case BOOLEAN_TYPE:
      case POINTER_TYPE:
      case REFERENCE_TYPE:
      case OFFSET_TYPE:
      case BITINT_TYPE:
         return native_interpret_int(self,type, ptr, len);

      case REAL_TYPE:
         if (tree ret = mtcs_const_native_interpret_real/*!native_interpret_real*/(self,type, ptr, len)){
            /* For floating point values in composite modes, punt if this
            folding doesn't preserve bit representation.  As the mode doesn't
            have fixed precision while GCC pretends it does, there could be
            valid values that GCC can't really represent accurately.
            See PR95450.  Even for other modes, e.g. x86 XFmode can have some
            bit combinationations which GCC doesn't preserve.  */
            unsigned char buf[24 * 2];
            scalar_float_mode mode = mtcs_mode_scalar_float_type_mode/*!SCALAR_FLOAT_TYPE_MODE*/(mtcsMode,type);
            int total_bytes =  mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);
            memcpy (buf + 24, ptr, total_bytes);
            clear_type_padding_in_mask (type, buf + 24);
            if (mtcs_const_native_encode_expr/*!native_encode_expr*/(self,
                  ret, buf, total_bytes, 0) != total_bytes || memcmp (buf + 24, buf, total_bytes) != 0)
               return NULL_TREE;
            return ret;
         }
         return NULL_TREE;

      case FIXED_POINT_TYPE:
         return native_interpret_fixed(self,type, ptr, len);

      case COMPLEX_TYPE:
         return native_interpret_complex(self,type, ptr, len);

      case VECTOR_TYPE:
         return native_interpret_vector(self,type, ptr, len);

      default:
         return NULL_TREE;
   }
}


/* Subroutine of native_interpret_expr.  Interpret the contents of
   the buffer PTR of length LEN as a REAL_CST of type TYPE.
   If the buffer cannot be interpreted, return NULL_TREE.  */
//原型 native_interpret_real fold-const.h fold-const.cc
tree mtcs_const_native_interpret_real (MtcsConst *self,tree type, const unsigned char *ptr, int len)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);

   scalar_float_mode mode = mtcs_mode_scalar_float_type_mode/*!SCALAR_FLOAT_TYPE_MODE*/(mtcsMode,type);
   int total_bytes =  mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);
   unsigned char value;
   /* There are always 32 bits in each long, no matter the size of
   the hosts long.  We handle floating point representations with
   up to 192 bits.  */
   REAL_VALUE_TYPE r;
   long tmp[6];

   if (total_bytes > len || total_bytes > 24)
      return NULL_TREE;
   int words = (32 / BITS_PER_UNIT) / UNITS_PER_WORD;

   memset (tmp, 0, sizeof (tmp));
   for (int bitpos = 0; bitpos < total_bytes * BITS_PER_UNIT; bitpos += BITS_PER_UNIT){
      /* Both OFFSET and BYTE index within a long;
      bitpos indexes the whole float.  */
      int offset, byte = (bitpos / BITS_PER_UNIT) & 3;
      if (UNITS_PER_WORD < 4){
         int word = byte / UNITS_PER_WORD;
         if (WORDS_BIG_ENDIAN)
            word = (words - 1) - word;
         offset = word * UNITS_PER_WORD;
         if (BYTES_BIG_ENDIAN)
            offset += (UNITS_PER_WORD - 1) - (byte % UNITS_PER_WORD);
         else
            offset += byte % UNITS_PER_WORD;
      }else{
         offset = byte;
         if (BYTES_BIG_ENDIAN){
            /* Reverse bytes within each long, or within the entire float
            if it's smaller than a long (for HFmode).  */
            offset = MIN (3, total_bytes - 1) - offset;
            gcc_assert (offset >= 0);
         }
      }
      value = ptr[offset + ((bitpos / BITS_PER_UNIT) & ~3)];

      tmp[bitpos / 32] |= (unsigned long)value << (bitpos & 31);
   }

   mtcs_real_real_from_target/*!real_from_target*/(mtcsReal,&r, tmp, mode);
   return mtcs_tree_build_real/*!build_real*/(mtcsTree,type, r);
}


/* Fold a binary tree expression with code CODE of type TYPE with
   operands OP0 and OP1.  LOC is the location of the resulting
   expression.  Return a folded expression if successful.  Otherwise,
   return a tree expression with code CODE of type TYPE with operands
   OP0 and OP1.  */
//原型 fold_build2_loc fold-const.h fold-const.cc
tree mtcs_const_fold_build2_loc(MtcsConst *self,location_t loc,
            enum tree_code code, tree type, tree op0, tree op1  MEM_STAT_DECL)
{
  tree tem;
#ifdef ENABLE_FOLD_CHECKING
  unsigned char checksum_before_op0[16],
                checksum_before_op1[16],
      checksum_after_op0[16],
      checksum_after_op1[16];
  struct md5_ctx ctx;
  hash_table<nofree_ptr_hash<const tree_node> > ht (32);

  md5_init_ctx (&ctx);
  fold_checksum_tree (op0, &ctx, &ht);
  md5_finish_ctx (&ctx, checksum_before_op0);
  ht.empty ();

  md5_init_ctx (&ctx);
  fold_checksum_tree (op1, &ctx, &ht);
  md5_finish_ctx (&ctx, checksum_before_op1);
  ht.empty ();
#endif

  tem = mtcs_const_fold_binary_loc/*!fold_binary_loc*/(self,loc, code, type, op0, op1);
  if (!tem)
    tem = build2_loc (loc, code, type, op0, op1 PASS_MEM_STAT);

#ifdef ENABLE_FOLD_CHECKING
  md5_init_ctx (&ctx);
  fold_checksum_tree (op0, &ctx, &ht);
  md5_finish_ctx (&ctx, checksum_after_op0);
  ht.empty ();

  if (memcmp (checksum_before_op0, checksum_after_op0, 16))
    fold_check_failed (op0, tem);

  md5_init_ctx (&ctx);
  fold_checksum_tree (op1, &ctx, &ht);
  md5_finish_ctx (&ctx, checksum_after_op1);

  if (memcmp (checksum_before_op1, checksum_after_op1, 16))
    fold_check_failed (op1, tem);
#endif
  return tem;
}

/* Transform `a + (b ? x : y)' into `b ? (a + x) : (a + y)'.
   Transform, `a + (x < y)' into `(x < y) ? (a + 1) : (a + 0)'.  Here
   CODE corresponds to the `+', COND to the `(b ? x : y)' or `(x < y)'
   expression, and ARG to `a'.  If COND_FIRST_P is nonzero, then the
   COND is the first argument to CODE; otherwise (as in the example
   given here), it is the second argument.  TYPE is the type of the
   original expression.  Return NULL_TREE if no simplification is
   possible.  */
//原型 fold_binary_op_with_conditional_arg fold-const.cc
static tree fold_binary_op_with_conditional_arg (MtcsConst *self,location_t loc,
                 enum tree_code code,
                 tree type, tree op0, tree op1,
                 tree cond, tree arg, int cond_first_p)
{
   tree cond_type = cond_first_p ? TREE_TYPE (op0) : TREE_TYPE (op1);
   tree arg_type = cond_first_p ? TREE_TYPE (op1) : TREE_TYPE (op0);
   tree test, true_value, false_value;
   tree lhs = NULL_TREE;
   tree rhs = NULL_TREE;
   enum tree_code cond_code = COND_EXPR;

   /* Do not move possibly trapping operations into the conditional as this
   pessimizes code and causes gimplification issues when applied late.  */
   if (operation_could_trap_p (code, FLOAT_TYPE_P (type), ANY_INTEGRAL_TYPE_P (type)  && TYPE_OVERFLOW_TRAPS (type), op1))
      return NULL_TREE;

   if (TREE_CODE (cond) == COND_EXPR || TREE_CODE (cond) == VEC_COND_EXPR){
      test = TREE_OPERAND (cond, 0);
      true_value = TREE_OPERAND (cond, 1);
      false_value = TREE_OPERAND (cond, 2);
      /* If this operand throws an expression, then it does not make
      sense to try to perform a logical or arithmetic operation
      involving it.  */
      if (VOID_TYPE_P (TREE_TYPE (true_value)))
         lhs = true_value;
      if (VOID_TYPE_P (TREE_TYPE (false_value)))
         rhs = false_value;
   }else if (!(TREE_CODE (type) != VECTOR_TYPE && VECTOR_TYPE_P (TREE_TYPE (cond)))){
      tree testtype = TREE_TYPE (cond);
      test = cond;
      true_value = constant_boolean_node (true, testtype);
      false_value = constant_boolean_node (false, testtype);
   }else
      /* Detect the case of mixing vector and scalar types - bail out.  */
      return NULL_TREE;

   if (VECTOR_TYPE_P (TREE_TYPE (test)))
      cond_code = VEC_COND_EXPR;

   /* This transformation is only worthwhile if we don't have to wrap ARG
   in a SAVE_EXPR and the operation can be simplified without recursing
   on at least one of the branches once its pushed inside the COND_EXPR.  */
   if (!TREE_CONSTANT (arg) && (TREE_SIDE_EFFECTS (arg)
   || TREE_CODE (arg) == COND_EXPR || TREE_CODE (arg) == VEC_COND_EXPR
   || TREE_CONSTANT (true_value) || TREE_CONSTANT (false_value)))
      return NULL_TREE;

   arg = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, arg_type, arg);
   if (lhs == 0){
      true_value = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, cond_type, true_value);
      if (cond_first_p)
         lhs = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, type, true_value, arg);
      else
         lhs = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, type, arg, true_value);
   }
   if (rhs == 0){
      false_value = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, cond_type, false_value);
      if (cond_first_p)
         rhs = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, type, false_value, arg);
      else
         rhs = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, type, arg, false_value);
   }

   /* Check that we have simplified at least one of the branches.  */
   if (!TREE_CONSTANT (arg) && !TREE_CONSTANT (lhs) && !TREE_CONSTANT (rhs))
      return NULL_TREE;

   return fold_build3_loc (loc, cond_code, type, test, lhs, rhs);
}

/* Fold a sum or difference of at least one multiplication.
   Returns the folded tree or NULL if no simplification could be made.  */
//原型 fold_plusminus_mult_expr fold-const.cc
static tree fold_plusminus_mult_expr (MtcsConst *self,location_t loc, enum tree_code code, tree type,
           tree arg0, tree arg1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree  *mtcsTree =mtcs_target_get_tree(mtcsTarget);

   tree arg00, arg01, arg10, arg11;
   tree alt0 = NULL_TREE, alt1 = NULL_TREE, same;

   /* (A * C) +- (B * C) -> (A+-B) * C.
   (A * C) +- A -> A * (C+-1).
   We are most concerned about the case where C is a constant,
   but other combinations show up during loop reduction.  Since
   it is not difficult, try all four possibilities.  */

   if (TREE_CODE (arg0) == MULT_EXPR){
      arg00 = TREE_OPERAND (arg0, 0);
      arg01 = TREE_OPERAND (arg0, 1);
   }else if (TREE_CODE (arg0) == INTEGER_CST){
      arg00 = build_one_cst (type);
      arg01 = arg0;
   }else{
      /* We cannot generate constant 1 for fract.  */
      if (ALL_FRACT_MODE_P (TYPE_MODE (type)))
         return NULL_TREE;
      arg00 = arg0;
      arg01 = build_one_cst (type);
   }
   if (TREE_CODE (arg1) == MULT_EXPR){
      arg10 = TREE_OPERAND (arg1, 0);
      arg11 = TREE_OPERAND (arg1, 1);
   }else if (TREE_CODE (arg1) == INTEGER_CST){
      arg10 = build_one_cst (type);
      /* As we canonicalize A - 2 to A + -2 get rid of that sign for
      the purpose of this canonicalization.  */
      if (wi::neg_p (wi::to_wide (arg1), TYPE_SIGN (TREE_TYPE (arg1)))
      && negate_expr_p(self,arg1)  && code == PLUS_EXPR){
         arg11 = negate_expr(self,arg1);
         code = MINUS_EXPR;
      }else
        arg11 = arg1;
   }else{
      /* We cannot generate constant 1 for fract.  */
      if (ALL_FRACT_MODE_P (TYPE_MODE (type)))
         return NULL_TREE;
      arg10 = arg1;
      arg11 = build_one_cst (type);
   }
   same = NULL_TREE;

   /* Prefer factoring a common non-constant.  */
   if (operand_equal_p (arg00, arg10, 0))
      same = arg00, alt0 = arg01, alt1 = arg11;
   else if (operand_equal_p (arg01, arg11, 0))
      same = arg01, alt0 = arg00, alt1 = arg10;
   else if (operand_equal_p (arg00, arg11, 0))
      same = arg00, alt0 = arg01, alt1 = arg10;
   else if (operand_equal_p (arg01, arg10, 0))
      same = arg01, alt0 = arg00, alt1 = arg11;

   /* No identical multiplicands; see if we can find a common
   power-of-two factor in non-power-of-two multiplies.  This
   can help in multi-dimensional array access.  */
   else if (tree_fits_shwi_p (arg01) && tree_fits_shwi_p (arg11)){
      HOST_WIDE_INT int01 = tree_to_shwi (arg01);
      HOST_WIDE_INT int11 = tree_to_shwi (arg11);
      HOST_WIDE_INT tmp;
      bool swap = false;
      tree maybe_same;

      /* Move min of absolute values to int11.  */
      if (absu_hwi (int01) < absu_hwi (int11)){
         tmp = int01, int01 = int11, int11 = tmp;
         alt0 = arg00, arg00 = arg10, arg10 = alt0;
         maybe_same = arg01;
         swap = true;
      }else
         maybe_same = arg11;

      const unsigned HOST_WIDE_INT factor = absu_hwi (int11);
      if (factor > 1
      && pow2p_hwi (factor)
      && (int01 & (factor - 1)) == 0
      /* The remainder should not be a constant, otherwise we
      end up folding i * 4 + 2 to (i * 2 + 1) * 2 which has
      increased the number of multiplications necessary.  */
      && TREE_CODE (arg10) != INTEGER_CST){
         alt0 = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, MULT_EXPR, TREE_TYPE (arg00), arg00,
                     mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (arg00),int01 / int11));
         alt1 = arg10;
         same = maybe_same;
         if (swap)
            maybe_same = alt0, alt0 = alt1, alt1 = maybe_same;
      }
   }

   if (!same)
      return NULL_TREE;

   if (! ANY_INTEGRAL_TYPE_P (type) || TYPE_OVERFLOW_WRAPS (type)
   /* We are neither factoring zero nor minus one.  */
   || TREE_CODE (same) == INTEGER_CST)
      return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, MULT_EXPR, type,
   mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, type,
         mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, alt0),
         mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, alt1)),
         mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, same));

   /* Same may be zero and thus the operation 'code' may overflow.  Likewise
   same may be minus one and thus the multiplication may overflow.  Perform
   the sum operation in an unsigned type.  */
   tree utype = unsigned_type_for (type);
   tree tem = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, utype,
   mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, utype, alt0),
   mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, utype, alt1));
   /* If the sum evaluated to a constant that is not -INF the multiplication
   cannot overflow.  */
   if (TREE_CODE (tem) == INTEGER_CST
   && (wi::to_wide (tem)
   != wi::min_value (TYPE_PRECISION (utype), SIGNED)))
      return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, MULT_EXPR, type,mtcs_const_fold_convert/*!fold_convert*/(self,type, tem), same);

   /* Do not resort to unsigned multiplication because
   we lose the no-overflow property of the expression.  */
   return NULL_TREE;
}

/* Split a tree IN into a constant, literal and variable parts that could be
   combined with CODE to make IN.  "constant" means an expression with
   TREE_CONSTANT but that isn't an actual constant.  CODE must be a
   commutative arithmetic operation.  Store the constant part into *CONP,
   the literal in *LITP and return the variable part.  If a part isn't
   present, set it to null.  If the tree does not decompose in this way,
   return the entire tree as the variable part and the other parts as null.

   If CODE is PLUS_EXPR we also split trees that use MINUS_EXPR.  In that
   case, we negate an operand that was subtracted.  Except if it is a
   literal for which we use *MINUS_LITP instead.

   If NEGATE_P is true, we are negating all of IN, again except a literal
   for which we use *MINUS_LITP instead.  If a variable part is of pointer
   type, it is negated after converting to TYPE.  This prevents us from
   generating illegal MINUS pointer expression.  LOC is the location of
   the converted variable part.

   If IN is itself a literal or constant, return it as appropriate.

   Note that we do not guarantee that any of the three values will be the
   same type as IN, but they will have the same signedness and mode.  */
//原型 split_tree fold-const.cc
static tree split_tree (tree in, tree type, enum tree_code code,
       tree *minus_varp, tree *conp, tree *minus_conp,
       tree *litp, tree *minus_litp, int negate_p)
{
   tree var = 0;
   *minus_varp = 0;
   *conp = 0;
   *minus_conp = 0;
   *litp = 0;
   *minus_litp = 0;

   /* Strip any conversions that don't change the machine mode or signedness.  */
   STRIP_SIGN_NOPS (in);

   if (TREE_CODE (in) == INTEGER_CST || TREE_CODE (in) == REAL_CST
   || TREE_CODE (in) == FIXED_CST)
      *litp = in;
   else if (TREE_CODE (in) == code
   || ((! FLOAT_TYPE_P (TREE_TYPE (in)) || flag_associative_math)
   && ! SAT_FIXED_POINT_TYPE_P (TREE_TYPE (in))
   /* We can associate addition and subtraction together (even
   though the C standard doesn't say so) for integers because
   the value is not affected.  For reals, the value might be
   affected, so we can't.  */
   && ((code == PLUS_EXPR && TREE_CODE (in) == POINTER_PLUS_EXPR)
   || (code == PLUS_EXPR && TREE_CODE (in) == MINUS_EXPR)
   || (code == MINUS_EXPR && (TREE_CODE (in) == PLUS_EXPR
   || TREE_CODE (in) == POINTER_PLUS_EXPR))))){
      tree op0 = TREE_OPERAND (in, 0);
      tree op1 = TREE_OPERAND (in, 1);
      bool neg1_p = TREE_CODE (in) == MINUS_EXPR;
      bool neg_litp_p = false, neg_conp_p = false, neg_var_p = false;

      /* First see if either of the operands is a literal, then a constant.  */
      if (TREE_CODE (op0) == INTEGER_CST || TREE_CODE (op0) == REAL_CST || TREE_CODE (op0) == FIXED_CST)
         *litp = op0, op0 = 0;
      else if (TREE_CODE (op1) == INTEGER_CST || TREE_CODE (op1) == REAL_CST || TREE_CODE (op1) == FIXED_CST)
         *litp = op1, neg_litp_p = neg1_p, op1 = 0;

      if (op0 != 0 && TREE_CONSTANT (op0))
         *conp = op0, op0 = 0;
      else if (op1 != 0 && TREE_CONSTANT (op1))
         *conp = op1, neg_conp_p = neg1_p, op1 = 0;

      /* If we haven't dealt with either operand, this is not a case we can
      decompose.  Otherwise, VAR is either of the ones remaining, if any.  */
      if (op0 != 0 && op1 != 0)
         var = in;
      else if (op0 != 0)
         var = op0;
      else
         var = op1, neg_var_p = neg1_p;

      /* Now do any needed negations.  */
      if (neg_litp_p)
         *minus_litp = *litp, *litp = 0;
      if (neg_conp_p && *conp)
         *minus_conp = *conp, *conp = 0;
      if (neg_var_p && var)
         *minus_varp = var, var = 0;
   }else if (TREE_CONSTANT (in))
      *conp = in;
   else if (TREE_CODE (in) == BIT_NOT_EXPR  && code == PLUS_EXPR){
      /* -1 - X is folded to ~X, undo that here.  Do _not_ do this
      when IN is constant.  */
      *litp = build_minus_one_cst (type);
      *minus_varp = TREE_OPERAND (in, 0);
   }else
      var = in;

   if (negate_p){
      if (*litp)
         *minus_litp = *litp, *litp = 0;
      else if (*minus_litp)
         *litp = *minus_litp, *minus_litp = 0;
      if (*conp)
         *minus_conp = *conp, *conp = 0;
      else if (*minus_conp)
         *conp = *minus_conp, *minus_conp = 0;
      if (var)
         *minus_varp = var, var = 0;
      else if (*minus_varp)
         var = *minus_varp, *minus_varp = 0;
   }

   if (*litp && TREE_OVERFLOW_P (*litp))
      *litp = drop_tree_overflow (*litp);
   if (*minus_litp && TREE_OVERFLOW_P (*minus_litp))
      *minus_litp = drop_tree_overflow (*minus_litp);

   return var;
}

/* Re-associate trees split by the above function.  T1 and T2 are
   either expressions to associate or null.  Return the new
   expression, if any.  LOC is the location of the new expression.  If
   we build an operation, do it in TYPE and with CODE.  */
//原型 associate_trees fold-const.cc
static tree associate_trees (MtcsConst *self,location_t loc, tree t1, tree t2, enum tree_code code, tree type)
{
   if (t1 == 0){
      gcc_assert (t2 == 0 || code != MINUS_EXPR);
      return t2;
   }else if (t2 == 0)
      return t1;

   /* If either input is CODE, a PLUS_EXPR, or a MINUS_EXPR, don't
   try to fold this since we will have infinite recursion.  But do
   deal with any NEGATE_EXPRs.  */
   if (TREE_CODE (t1) == code || TREE_CODE (t2) == code
   || TREE_CODE (t1) == PLUS_EXPR || TREE_CODE (t2) == PLUS_EXPR
   || TREE_CODE (t1) == MINUS_EXPR || TREE_CODE (t2) == MINUS_EXPR){
      if (code == PLUS_EXPR){
         if (TREE_CODE (t1) == NEGATE_EXPR)
            return build2_loc (loc, MINUS_EXPR, type,
                  mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, t2),
                  mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type,
                  TREE_OPERAND (t1, 0)));
         else if (TREE_CODE (t2) == NEGATE_EXPR)
            return build2_loc (loc, MINUS_EXPR, type,
                     mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, t1),
                     mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type,
                     TREE_OPERAND (t2, 0)));
         else if (integer_zerop (t2))
            return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, t1);
      }else if (code == MINUS_EXPR){
         if (integer_zerop (t2))
            return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, t1);
      }

      return build2_loc (loc, code, type, mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, t1),
               mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, t2));
   }

   return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, type,
         mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, t1),
               mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, t2));
}

/* Try to fold a pointer difference of type TYPE two address expressions of
   array references AREF0 and AREF1 using location LOC.  Return a
   simplified expression for the difference or NULL_TREE.  */
//原型 fold_addr_of_array_ref_difference fold-const.cc
static tree fold_addr_of_array_ref_difference (MtcsConst *self,location_t loc, tree type,
               tree aref0, tree aref1,
               bool use_pointer_diff)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree base0 = TREE_OPERAND (aref0, 0);
   tree base1 = TREE_OPERAND (aref1, 0);
   tree base_offset = mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,type, 0);

   /* If the bases are array references as well, recurse.  If the bases
   are pointer indirections compute the difference of the pointers.
   If the bases are equal, we are set.  */
   if ((TREE_CODE (base0) == ARRAY_REF
   && TREE_CODE (base1) == ARRAY_REF
   && (base_offset = fold_addr_of_array_ref_difference(self,loc, type, base0, base1, use_pointer_diff)))
   || (INDIRECT_REF_P (base0)  && INDIRECT_REF_P (base1)  && (base_offset = use_pointer_diff
   ? mtcs_const_fold_binary_loc/*!fold_binary_loc*/(self,loc, POINTER_DIFF_EXPR, type,
   TREE_OPERAND (base0, 0),
   TREE_OPERAND (base1, 0))
   : mtcs_const_fold_binary_loc/*!fold_binary_loc*/(self,loc, MINUS_EXPR, type,
   mtcs_const_fold_convert/*!fold_convert*/(self,type,
   TREE_OPERAND (base0, 0)),
   mtcs_const_fold_convert/*!fold_convert*/(self,type,
   TREE_OPERAND (base1, 0)))))
   || operand_equal_p (base0, base1, OEP_ADDRESS_OF)){
      tree op0 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, TREE_OPERAND (aref0, 1));
      tree op1 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, TREE_OPERAND (aref1, 1));
      tree esz = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, array_ref_element_size (aref0));
      tree diff = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, MINUS_EXPR, type, op0, op1);
      return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, PLUS_EXPR, type,base_offset,
            mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, MULT_EXPR, type,diff, esz));
   }
   return NULL_TREE;
}

/* For an expression that has the form
     (A && B) || ~B
   or
     (A || B) && ~B,
   we can drop one of the inner expressions and simplify to
     A || ~B
   or
     A && ~B
   LOC is the location of the resulting expression.  OP is the inner
   logical operation; the left-hand side in the examples above, while CMPOP
   is the right-hand side.  RHS_ONLY is used to prevent us from accidentally
   removing a condition that guards another, as in
     (A != NULL && A->...) || A == NULL
   which we must not transform.  If RHS_ONLY is true, only eliminate the
   right-most operand of the inner logical operation.  */
static tree merge_truthop_with_opposite_arm (MtcsConst *self,location_t loc, tree op, tree cmpop,
             bool rhs_only)
{
   enum tree_code code = TREE_CODE (cmpop);
   enum tree_code truthop_code = TREE_CODE (op);
   tree lhs = TREE_OPERAND (op, 0);
   tree rhs = TREE_OPERAND (op, 1);
   tree orig_lhs = lhs, orig_rhs = rhs;
   enum tree_code rhs_code = TREE_CODE (rhs);
   enum tree_code lhs_code = TREE_CODE (lhs);
   enum tree_code inv_code;

   if (TREE_SIDE_EFFECTS (op) || TREE_SIDE_EFFECTS (cmpop))
      return NULL_TREE;

   if (TREE_CODE_CLASS (code) != tcc_comparison)
      return NULL_TREE;

   tree type = TREE_TYPE (TREE_OPERAND (cmpop, 0));

   if (rhs_code == truthop_code){
      tree newrhs = merge_truthop_with_opposite_arm(self,loc, rhs, cmpop, rhs_only);
      if (newrhs != NULL_TREE){
         rhs = newrhs;
         rhs_code = TREE_CODE (rhs);
      }
   }
   if (lhs_code == truthop_code && !rhs_only){
      tree newlhs = merge_truthop_with_opposite_arm(self,loc, lhs, cmpop, false);
      if (newlhs != NULL_TREE){
         lhs = newlhs;
         lhs_code = TREE_CODE (lhs);
      }
   }

   inv_code = invert_tree_comparison (code, HONOR_NANS (type));
   if (inv_code == rhs_code
   && operand_equal_p (TREE_OPERAND (rhs, 0), TREE_OPERAND (cmpop, 0), 0)
   && operand_equal_p (TREE_OPERAND (rhs, 1), TREE_OPERAND (cmpop, 1), 0))
      return lhs;
   if (!rhs_only && inv_code == lhs_code
   && operand_equal_p (TREE_OPERAND (lhs, 0), TREE_OPERAND (cmpop, 0), 0)
   && operand_equal_p (TREE_OPERAND (lhs, 1), TREE_OPERAND (cmpop, 1), 0))
      return rhs;
   if (rhs != orig_rhs || lhs != orig_lhs)
      return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, truthop_code, TREE_TYPE (cmpop),lhs, rhs);
   return NULL_TREE;
}

/* Subroutine for fold_truth_andor_1 and simple_condition_p: determine if an
   operand is simple enough to be evaluated unconditionally.  */
//原型 simple_operand_p fold-const.cc
static bool simple_operand_p (const_tree exp)
{
   /* Strip any conversions that don't change the machine mode.  */
   STRIP_NOPS (exp);

   return (CONSTANT_CLASS_P (exp)
      || TREE_CODE (exp) == SSA_NAME
      || (DECL_P (exp)
      && ! TREE_ADDRESSABLE (exp)
      && ! TREE_THIS_VOLATILE (exp)
      && ! DECL_NONLOCAL (exp)
      /* Don't regard global variables as simple.  They may be
      allocated in ways unknown to the compiler (shared memory,
      #pragma weak, etc).  */
      && ! TREE_PUBLIC (exp)
      && ! DECL_EXTERNAL (exp)
      /* Weakrefs are not safe to be read, since they can be NULL.
      They are !TREE_PUBLIC && !DECL_EXTERNAL but still
      have DECL_WEAK flag set.  */
      && (! VAR_OR_FUNCTION_DECL_P (exp) || ! DECL_WEAK (exp))
      /* Loading a static variable is unduly expensive, but global
      registers aren't expensive.  */
      && (! TREE_STATIC (exp) || DECL_REGISTER (exp))));
}

/* Find ways of folding logical expressions of LHS and RHS:
   Try to merge two comparisons to the same innermost item.
   Look for range tests like "ch >= '0' && ch <= '9'".
   Look for combinations of simple terms on machines with expensive branches
   and evaluate the RHS unconditionally.

   We check for both normal comparisons and the BIT_AND_EXPRs made this by
   function and the one above.

   CODE is the logical operation being done.  It can be TRUTH_ANDIF_EXPR,
   TRUTH_AND_EXPR, TRUTH_ORIF_EXPR, or TRUTH_OR_EXPR.

   TRUTH_TYPE is the type of the logical operand and LHS and RHS are its
   two operands.

   We return the simplified tree or 0 if no optimization is possible.  */
//原型 fold_truth_andor_1 fold-const.cc
static tree fold_truth_andor_1 (MtcsConst *self,location_t loc, enum tree_code code, tree truth_type,
          tree lhs, tree rhs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree  *mtcsTree =mtcs_target_get_tree(mtcsTarget);

   /* If this is the "or" of two comparisons, we can do something if
   the comparisons are NE_EXPR.  If this is the "and", we can do something
   if the comparisons are EQ_EXPR.  I.e.,
   (a->b == 2 && a->c == 4) can become (a->new == NEW).

   WANTED_CODE is this operation code.  For single bit fields, we can
   convert EQ_EXPR to NE_EXPR so we need not reject the "wrong"
   comparison for one-bit fields.  */

   enum tree_code lcode, rcode;
   tree ll_arg, lr_arg, rl_arg, rr_arg;
   tree result;

   /* Start by getting the comparison codes.  Fail if anything is volatile.
   If one operand is a BIT_AND_EXPR with the constant one, treat it as if
   it were surrounded with a NE_EXPR.  */

   if (TREE_SIDE_EFFECTS (lhs) || TREE_SIDE_EFFECTS (rhs))
      return 0;

   lcode = TREE_CODE (lhs);
   rcode = TREE_CODE (rhs);

   if (lcode == BIT_AND_EXPR && integer_onep (TREE_OPERAND (lhs, 1))){
      lhs = build2 (NE_EXPR, truth_type, lhs,mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (lhs), 0));
      lcode = NE_EXPR;
   }

   if (rcode == BIT_AND_EXPR && integer_onep (TREE_OPERAND (rhs, 1))){
      rhs = build2 (NE_EXPR, truth_type, rhs,mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (rhs), 0));
      rcode = NE_EXPR;
   }

   if (TREE_CODE_CLASS (lcode) != tcc_comparison || TREE_CODE_CLASS (rcode) != tcc_comparison)
      return 0;

   ll_arg = TREE_OPERAND (lhs, 0);
   lr_arg = TREE_OPERAND (lhs, 1);
   rl_arg = TREE_OPERAND (rhs, 0);
   rr_arg = TREE_OPERAND (rhs, 1);

   /* Simplify (x<y) && (x==y) into (x<=y) and related optimizations.  */
   if (simple_operand_p (ll_arg) && simple_operand_p (lr_arg)){
      if (operand_equal_p (ll_arg, rl_arg, 0) && operand_equal_p (lr_arg, rr_arg, 0)){
         result = combine_comparisons (loc, code, lcode, rcode, truth_type, ll_arg, lr_arg);
         if (result)
            return result;
      }else if (operand_equal_p (ll_arg, rr_arg, 0)  && operand_equal_p (lr_arg, rl_arg, 0)){
         result = combine_comparisons (loc, code, lcode,
         swap_tree_comparison (rcode), truth_type, ll_arg, lr_arg);
         if (result)
            return result;
      }
   }

   code = ((code == TRUTH_AND_EXPR || code == TRUTH_ANDIF_EXPR) ? TRUTH_AND_EXPR : TRUTH_OR_EXPR);

   /* If the RHS can be evaluated unconditionally and its operands are
   simple, it wins to evaluate the RHS unconditionally on machines
   with expensive branches.  In this case, this isn't a comparison
   that can be merged.  */

   if (BRANCH_COST (optimize_function_for_speed_p (cfun),false) >= 2
   && ! FLOAT_TYPE_P (TREE_TYPE (rl_arg))
   && simple_operand_p (rl_arg)
   && simple_operand_p (rr_arg)){
      /* Convert (a != 0) || (b != 0) into (a | b) != 0.  */
      if (code == TRUTH_OR_EXPR
      && lcode == NE_EXPR && integer_zerop (lr_arg)
      && rcode == NE_EXPR && integer_zerop (rr_arg)
      && TREE_TYPE (ll_arg) == TREE_TYPE (rl_arg)
      && INTEGRAL_TYPE_P (TREE_TYPE (ll_arg)))
         return build2_loc (loc, NE_EXPR, truth_type,
               build2 (BIT_IOR_EXPR, TREE_TYPE (ll_arg),ll_arg, rl_arg),
               mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (ll_arg), 0));

      /* Convert (a == 0) && (b == 0) into (a | b) == 0.  */
      if (code == TRUTH_AND_EXPR
      && lcode == EQ_EXPR && integer_zerop (lr_arg)
      && rcode == EQ_EXPR && integer_zerop (rr_arg)
      && TREE_TYPE (ll_arg) == TREE_TYPE (rl_arg)
      && INTEGRAL_TYPE_P (TREE_TYPE (ll_arg)))
         return build2_loc (loc, EQ_EXPR, truth_type,
               build2 (BIT_IOR_EXPR, TREE_TYPE (ll_arg),ll_arg, rl_arg),
               mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (ll_arg), 0));
   }

   return 0;
}

/* EXP is some logical combination of boolean tests.  See if we can
   merge it into some range test.  Return the new tree if so.  */
//原型 fold_range_test fold-const.cc
static tree fold_range_test (location_t loc, enum tree_code code, tree type,
       tree op0, tree op1)
{
   int or_op = (code == TRUTH_ORIF_EXPR || code == TRUTH_OR_EXPR);
   int in0_p, in1_p, in_p;
   tree low0, low1, low, high0, high1, high;
   bool strict_overflow_p = false;
   tree tem, lhs, rhs;
   const char * const warnmsg = G_("assuming signed overflow does not occur "
   "when simplifying range test");

   if (!INTEGRAL_TYPE_P (type))
      return 0;

   lhs = make_range (op0, &in0_p, &low0, &high0, &strict_overflow_p);
   /* If op0 is known true or false and this is a short-circuiting
   operation we must not merge with op1 since that makes side-effects
   unconditional.  So special-case this.  */
   if (!lhs && ((code == TRUTH_ORIF_EXPR && in0_p)|| (code == TRUTH_ANDIF_EXPR && !in0_p)))
      return op0;
   rhs = make_range (op1, &in1_p, &low1, &high1, &strict_overflow_p);

   /* If this is an OR operation, invert both sides; we will invert
   again at the end.  */
   if (or_op)
      in0_p = ! in0_p, in1_p = ! in1_p;

   /* If both expressions are the same, if we can merge the ranges, and we
   can build the range test, return it or it inverted.  If one of the
   ranges is always true or always false, consider it to be the same
   expression as the other.  */
   if ((lhs == 0 || rhs == 0 || operand_equal_p (lhs, rhs, 0))
   && merge_ranges (&in_p, &low, &high, in0_p, low0, high0,in1_p, low1, high1)
   && (tem = (build_range_check (loc, type,
   lhs != 0 ? lhs
   : rhs != 0 ? rhs : integer_zero_node,
   in_p, low, high))) != 0){
      if (strict_overflow_p)
         fold_overflow_warning (warnmsg, WARN_STRICT_OVERFLOW_COMPARISON);
      return or_op ? invert_truthvalue_loc (loc, tem) : tem;
   }

   /* On machines where the branch cost is expensive, if this is a
   short-circuited branch and the underlying object on both sides
   is the same, make a non-short-circuit operation.  */
   bool logical_op_non_short_circuit = LOGICAL_OP_NON_SHORT_CIRCUIT;
   if (param_logical_op_non_short_circuit != -1)
   logical_op_non_short_circuit = param_logical_op_non_short_circuit;
   if (logical_op_non_short_circuit
   && !sanitize_coverage_p ()
   && lhs != 0 && rhs != 0
   && (code == TRUTH_ANDIF_EXPR || code == TRUTH_ORIF_EXPR)
   && operand_equal_p (lhs, rhs, 0)){
      /* If simple enough, just rewrite.  Otherwise, make a SAVE_EXPR
      unless we are at top level or LHS contains a PLACEHOLDER_EXPR, in
      which cases we can't do this.  */
      if (simple_operand_p (lhs))
         return build2_loc (loc, code == TRUTH_ANDIF_EXPR ? TRUTH_AND_EXPR : TRUTH_OR_EXPR, type, op0, op1);

      else if (!lang_hooks.decls.global_bindings_p () && !CONTAINS_PLACEHOLDER_P (lhs)){
         tree common = save_expr (lhs);

         if ((lhs = build_range_check (loc, type, common, or_op ? ! in0_p : in0_p,low0, high0)) != 0
         && (rhs = build_range_check (loc, type, common, or_op ? ! in1_p : in1_p,low1, high1)) != 0){
            if (strict_overflow_p)
               fold_overflow_warning (warnmsg, WARN_STRICT_OVERFLOW_COMPARISON);
            return build2_loc (loc, code == TRUTH_ANDIF_EXPR ? TRUTH_AND_EXPR : TRUTH_OR_EXPR, type, lhs, rhs);
         }
      }
   }

   return 0;
}

/* Fold a binary bitwise/truth expression of code CODE and type TYPE with
   operands OP0 and OP1.  LOC is the location of the resulting expression.
   ARG0 and ARG1 are the NOP_STRIPed results of OP0 and OP1.
   Return the folded expression if folding is successful.  Otherwise,
   return NULL_TREE.  */
//原型 fold_truth_andor fold-const.cc
static tree fold_truth_andor (MtcsConst *self,location_t loc, enum tree_code code, tree type,
        tree arg0, tree arg1, tree op0, tree op1)
{
   tree tem;

   /* We only do these simplifications if we are optimizing.  */
   if (!optimize)
      return NULL_TREE;

   /* Check for things like (A || B) && (A || C).  We can convert this
   to A || (B && C).  Note that either operator can be any of the four
   truth and/or operations and the transformation will still be
   valid.   Also note that we only care about order for the
   ANDIF and ORIF operators.  If B contains side effects, this
   might change the truth-value of A.  */
   if (TREE_CODE (arg0) == TREE_CODE (arg1)
   && (TREE_CODE (arg0) == TRUTH_ANDIF_EXPR
   || TREE_CODE (arg0) == TRUTH_ORIF_EXPR
   || TREE_CODE (arg0) == TRUTH_AND_EXPR
   || TREE_CODE (arg0) == TRUTH_OR_EXPR)
   && ! TREE_SIDE_EFFECTS (TREE_OPERAND (arg0, 1))){
      tree a00 = TREE_OPERAND (arg0, 0);
      tree a01 = TREE_OPERAND (arg0, 1);
      tree a10 = TREE_OPERAND (arg1, 0);
      tree a11 = TREE_OPERAND (arg1, 1);
      bool commutative = ((TREE_CODE (arg0) == TRUTH_OR_EXPR
      || TREE_CODE (arg0) == TRUTH_AND_EXPR)  && (code == TRUTH_AND_EXPR || code == TRUTH_OR_EXPR));

      if (operand_equal_p (a00, a10, 0))
         return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, TREE_CODE (arg0), type, a00,
               mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, type, a01, a11));
      else if (commutative && operand_equal_p (a00, a11, 0))
         return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, TREE_CODE (arg0), type, a00,
               mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, type, a01, a10));
      else if (commutative && operand_equal_p (a01, a10, 0))
         return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, TREE_CODE (arg0), type, a01,
               mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, type, a00, a11));

      /* This case if tricky because we must either have commutative
      operators or else A10 must not have side-effects.  */

      else if ((commutative || ! TREE_SIDE_EFFECTS (a10))  && operand_equal_p (a01, a11, 0))
         return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, TREE_CODE (arg0), type,
               mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, type, a00, a10),a01);
   }

   /* See if we can build a range comparison.  */
   if ((tem = fold_range_test (loc, code, type, op0, op1)) != 0)
      return tem;

   if ((code == TRUTH_ANDIF_EXPR && TREE_CODE (arg0) == TRUTH_ORIF_EXPR)
   || (code == TRUTH_ORIF_EXPR && TREE_CODE (arg0) == TRUTH_ANDIF_EXPR)){
      tem = merge_truthop_with_opposite_arm(self,loc, arg0, arg1, true);
      if (tem)
         return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, type, tem, arg1);
   }

   if ((code == TRUTH_ANDIF_EXPR && TREE_CODE (arg1) == TRUTH_ORIF_EXPR)
   || (code == TRUTH_ORIF_EXPR && TREE_CODE (arg1) == TRUTH_ANDIF_EXPR)){
      tem = merge_truthop_with_opposite_arm(self,loc, arg1, arg0, false);
      if (tem)
         return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, type, arg0, tem);
   }

   /* Check for the possibility of merging component references.  If our
   lhs is another similar operation, try to merge its rhs with our
   rhs.  Then try to merge our lhs and rhs.  */
   if (TREE_CODE (arg0) == code && (tem = fold_truth_andor_1(self,loc, code, type,TREE_OPERAND (arg0, 1), arg1)) != 0)
      return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, type, TREE_OPERAND (arg0, 0), tem);

   if ((tem = fold_truth_andor_1(self,loc, code, type, arg0, arg1)) != 0)
      return tem;

   bool logical_op_non_short_circuit = LOGICAL_OP_NON_SHORT_CIRCUIT;
   if (param_logical_op_non_short_circuit != -1)
      logical_op_non_short_circuit = param_logical_op_non_short_circuit;
   if (logical_op_non_short_circuit
   && !sanitize_coverage_p ()
   && (code == TRUTH_AND_EXPR
   || code == TRUTH_ANDIF_EXPR
   || code == TRUTH_OR_EXPR
   || code == TRUTH_ORIF_EXPR)){
      enum tree_code ncode, icode;

      ncode = (code == TRUTH_ANDIF_EXPR || code == TRUTH_AND_EXPR) ? TRUTH_AND_EXPR : TRUTH_OR_EXPR;
      icode = ncode == TRUTH_AND_EXPR ? TRUTH_ANDIF_EXPR : TRUTH_ORIF_EXPR;

      /* Transform ((A AND-IF B) AND[-IF] C) into (A AND-IF (B AND C)),
      or ((A OR-IF B) OR[-IF] C) into (A OR-IF (B OR C))
      We don't want to pack more than two leafs to a non-IF AND/OR
      expression.
      If tree-code of left-hand operand isn't an AND/OR-IF code and not
      equal to IF-CODE, then we don't want to add right-hand operand.
      If the inner right-hand side of left-hand operand has
      side-effects, or isn't simple, then we can't add to it,
      as otherwise we might destroy if-sequence.  */
      if (TREE_CODE (arg0) == icode
      && simple_condition_p (arg1)
      /* Needed for sequence points to handle trappings, and
      side-effects.  */
      && simple_condition_p (TREE_OPERAND (arg0, 1))){
         tem = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, ncode, type, TREE_OPERAND (arg0, 1),arg1);
         return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, icode, type, TREE_OPERAND (arg0, 0),tem);
      }
      /* Same as above but for (A AND[-IF] (B AND-IF C)) -> ((A AND B) AND-IF C),
      or (A OR[-IF] (B OR-IF C) -> ((A OR B) OR-IF C).  */
      else if (TREE_CODE (arg1) == icode
      && simple_condition_p (arg0)
      /* Needed for sequence points to handle trappings, and
      side-effects.  */
      && simple_condition_p (TREE_OPERAND (arg1, 0))){
         tem = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, ncode, type,arg0, TREE_OPERAND (arg1, 0));
         return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, icode, type, tem, TREE_OPERAND (arg1, 1));
      }
      /* Transform (A AND-IF B) into (A AND B), or (A OR-IF B)
      into (A OR B).
      For sequence point consistancy, we need to check for trapping,
      and side-effects.  */
      else if (code == icode && simple_condition_p (arg0)  && simple_condition_p (arg1))
         return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, ncode, type, arg0, arg1);
   }

   return NULL_TREE;
}


/* Subroutine of fold_binary.  Optimize complex multiplications of the
   form z * conj(z), as pow(realpart(z),2) + pow(imagpart(z),2).  The
   argument EXPR represents the expression "z" of type TYPE.  */
//原型 fold_mult_zconjz fold-const.cc
static tree fold_mult_zconjz (MtcsConst *self,location_t loc, tree type, tree expr)
{
   tree itype = TREE_TYPE (type);
   tree rpart, ipart, tem;

   if (TREE_CODE (expr) == COMPLEX_EXPR){
      rpart = TREE_OPERAND (expr, 0);
      ipart = TREE_OPERAND (expr, 1);
   }else if (TREE_CODE (expr) == COMPLEX_CST){
      rpart = TREE_REALPART (expr);
      ipart = TREE_IMAGPART (expr);
   }else{
      expr = save_expr (expr);
      rpart = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, REALPART_EXPR, itype, expr);
      ipart = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, IMAGPART_EXPR, itype, expr);
   }

   rpart = save_expr (rpart);
   ipart = save_expr (ipart);
   tem = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, PLUS_EXPR, itype,
   mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, MULT_EXPR, itype, rpart, rpart),
   mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, MULT_EXPR, itype, ipart, ipart));
   return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, COMPLEX_EXPR, type, tem,build_zero_cst (itype));
}

/*  Mask out the tz least significant bits of X of type TYPE where
    tz is the number of trailing zeroes in Y.  */
//原型 mask_with_tz fold-const.cc
static wide_int mask_with_tz (tree type, const wide_int &x, const wide_int &y)
{
  int tz = wi::ctz (y);
  if (tz > 0)
    return wi::mask (tz, true, TYPE_PRECISION (type)) & x;
  return x;
}


/* Return whether BASE + OFFSET + BITPOS may wrap around the address
   space.  This is used to avoid issuing overflow warnings for
   expressions like &p->x which cannot wrap.  */
//原型 pointer_may_wrap_p fold-const.cc
static bool pointer_may_wrap_p (tree base, tree offset, poly_int64 bitpos)
{
   if (!POINTER_TYPE_P (TREE_TYPE (base)))
      return true;

   if (maybe_lt (bitpos, 0))
      return true;

   poly_wide_int wi_offset;
   int precision = TYPE_PRECISION (TREE_TYPE (base));
   if (offset == NULL_TREE)
      wi_offset = wi::zero (precision);
   else if (!poly_int_tree_p (offset) || TREE_OVERFLOW (offset))
      return true;
   else
      wi_offset = wi::to_poly_wide (offset);

   wi::overflow_type overflow;
   poly_wide_int units = wi::shwi (bits_to_bytes_round_down (bitpos),precision);
   poly_wide_int total = wi::add (wi_offset, units, UNSIGNED, &overflow);
   if (overflow)
      return true;

   poly_uint64 total_hwi, size;
   if (!total.to_uhwi (&total_hwi)
         || !poly_int_tree_p (TYPE_SIZE_UNIT (TREE_TYPE (TREE_TYPE (base))),&size) || known_eq (size, 0U))
      return true;

   if (known_le (total_hwi, size))
      return false;

   /* We can do slightly better for SIZE if we have an ADDR_EXPR of an
   array.  */
   if (TREE_CODE (base) == ADDR_EXPR && poly_int_tree_p (TYPE_SIZE_UNIT (TREE_TYPE (TREE_OPERAND (base, 0))),&size)
   && maybe_ne (size, 0U) && known_le (total_hwi, size))
      return false;

   return true;
}

/* Return a positive integer when the symbol DECL is known to have
   a nonzero address, zero when it's known not to (e.g., it's a weak
   symbol), and a negative integer when the symbol is not yet in the
   symbol table and so whether or not its address is zero is unknown.
   For function local objects always return positive integer.  */
//原型 maybe_nonzero_address fold-const.cc
static int maybe_nonzero_address (tree decl)
{
   /* Normally, don't do anything for variables and functions before symtab is
   built; it is quite possible that DECL will be declared weak later.
   But if folding_initializer, we need a constant answer now, so create
   the symtab entry and prevent later weak declaration.  */
   if (DECL_P (decl) && decl_in_symtab_p (decl))
      if (struct symtab_node *symbol = (folding_initializer
      ? symtab_node::get_create (decl) : symtab_node::get (decl)))
         return symbol->nonzero_address ();

   /* Function local objects are never NULL.  */
   if (DECL_P (decl)
   && (DECL_CONTEXT (decl)
   && TREE_CODE (DECL_CONTEXT (decl)) == FUNCTION_DECL
   && auto_var_in_fn_p (decl, DECL_CONTEXT (decl))))
      return 1;

   return -1;
}

/* See if ARG is an expression that is either a comparison or is performing
   arithmetic on comparisons.  The comparisons must only be comparing
   two different values, which will be stored in *CVAL1 and *CVAL2; if
   they are nonzero it means that some operands have already been found.
   No variables may be used anywhere else in the expression except in the
   comparisons.

   If this is true, return 1.  Otherwise, return zero.  */
//原型 twoval_comparison_p fold-const.cc
static bool twoval_comparison_p (tree arg, tree *cval1, tree *cval2)
{
   enum tree_code code = TREE_CODE (arg);
   enum tree_code_class tclass = TREE_CODE_CLASS (code);

   /* We can handle some of the tcc_expression cases here.  */
   if (tclass == tcc_expression && code == TRUTH_NOT_EXPR)
      tclass = tcc_unary;
   else if (tclass == tcc_expression && (code == TRUTH_ANDIF_EXPR || code == TRUTH_ORIF_EXPR || code == COMPOUND_EXPR))
      tclass = tcc_binary;

   switch (tclass){
      case tcc_unary:
         return twoval_comparison_p (TREE_OPERAND (arg, 0), cval1, cval2);

      case tcc_binary:
         return (twoval_comparison_p (TREE_OPERAND (arg, 0), cval1, cval2)
               && twoval_comparison_p (TREE_OPERAND (arg, 1), cval1, cval2));

      case tcc_constant:
         return true;

      case tcc_expression:
         if (code == COND_EXPR)
            return (twoval_comparison_p (TREE_OPERAND (arg, 0), cval1, cval2)
               && twoval_comparison_p (TREE_OPERAND (arg, 1), cval1, cval2)
               && twoval_comparison_p (TREE_OPERAND (arg, 2), cval1, cval2));
         return false;

      case tcc_comparison:
         /* First see if we can handle the first operand, then the second.  For
         the second operand, we know *CVAL1 can't be zero.  It must be that
         one side of the comparison is each of the values; test for the
         case where this isn't true by failing if the two operands
         are the same.  */

         if (operand_equal_p (TREE_OPERAND (arg, 0),TREE_OPERAND (arg, 1), 0))
            return false;

         if (*cval1 == 0)
            *cval1 = TREE_OPERAND (arg, 0);
         else if (operand_equal_p (*cval1, TREE_OPERAND (arg, 0), 0))
            ;
         else if (*cval2 == 0)
            *cval2 = TREE_OPERAND (arg, 0);
         else if (operand_equal_p (*cval2, TREE_OPERAND (arg, 0), 0))
            ;
         else
            return false;

         if (operand_equal_p (*cval1, TREE_OPERAND (arg, 1), 0))
            ;
         else if (*cval2 == 0)
            *cval2 = TREE_OPERAND (arg, 1);
         else if (operand_equal_p (*cval2, TREE_OPERAND (arg, 1), 0))
            ;
         else
            return false;

         return true;

      default:
         return false;
   }
}

/* ARG is a tree that is known to contain just arithmetic operations and
   comparisons.  Evaluate the operations in the tree substituting NEW0 for
   any occurrence of OLD0 as an operand of a comparison and likewise for
   NEW1 and OLD1.  */
//原型 eval_subst fold-const.cc
static tree eval_subst (MtcsConst *self,location_t loc, tree arg, tree old0, tree new0,
       tree old1, tree new1)
{
   tree type = TREE_TYPE (arg);
   enum tree_code code = TREE_CODE (arg);
   enum tree_code_class tclass = TREE_CODE_CLASS (code);

   /* We can handle some of the tcc_expression cases here.  */
   if (tclass == tcc_expression && code == TRUTH_NOT_EXPR)
      tclass = tcc_unary;
   else if (tclass == tcc_expression  && (code == TRUTH_ANDIF_EXPR || code == TRUTH_ORIF_EXPR))
      tclass = tcc_binary;

   switch (tclass){
      case tcc_unary:
         return mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,
               loc, code, type,eval_subst(self,loc, TREE_OPERAND (arg, 0),old0, new0, old1, new1));

      case tcc_binary:
         return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, type,
            eval_subst(self,loc, TREE_OPERAND (arg, 0),old0, new0, old1, new1),
            eval_subst(self,loc, TREE_OPERAND (arg, 1),old0, new0, old1, new1));

      case tcc_expression:
         switch (code){
            case SAVE_EXPR:
               return eval_subst(self,loc, TREE_OPERAND (arg, 0), old0, new0,old1, new1);

            case COMPOUND_EXPR:
               return eval_subst(self,loc, TREE_OPERAND (arg, 1), old0, new0,old1, new1);

            case COND_EXPR:
               return fold_build3_loc (loc, code, type,
               eval_subst(self,loc, TREE_OPERAND (arg, 0),old0, new0, old1, new1),
               eval_subst(self,loc, TREE_OPERAND (arg, 1),old0, new0, old1, new1),
               eval_subst(self,loc, TREE_OPERAND (arg, 2),old0, new0, old1, new1));
            default:
               break;
         }
         /* Fall through - ???  */

      case tcc_comparison:
      {
         tree arg0 = TREE_OPERAND (arg, 0);
         tree arg1 = TREE_OPERAND (arg, 1);

         /* We need to check both for exact equality and tree equality.  The
         former will be true if the operand has a side-effect.  In that
         case, we know the operand occurred exactly once.  */

         if (arg0 == old0 || operand_equal_p (arg0, old0, 0))
            arg0 = new0;
         else if (arg0 == old1 || operand_equal_p (arg0, old1, 0))
            arg0 = new1;

         if (arg1 == old0 || operand_equal_p (arg1, old0, 0))
            arg1 = new0;
         else if (arg1 == old1 || operand_equal_p (arg1, old1, 0))
            arg1 = new1;

         return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, type, arg0, arg1);
      }

      default:
         return arg;
   }
}

/* Optimize a bit-field compare.

   There are two cases:  First is a compare against a constant and the
   second is a comparison of two items where the fields are at the same
   bit position relative to the start of a chunk (byte, halfword, word)
   large enough to contain it.  In these cases we can avoid the shift
   implicit in bitfield extractions.

   For constants, we emit a compare of the shifted constant with the
   BIT_AND_EXPR of a mask and a byte, halfword, or word of the operand being
   compared.  For two fields at the same position, we do the ANDs with the
   similar mask and compare the result of the ANDs.

   CODE is the comparison code, known to be either NE_EXPR or EQ_EXPR.
   COMPARE_TYPE is the type of the comparison, and LHS and RHS
   are the left and right operands of the comparison, respectively.

   If the optimization described above can be done, we return the resulting
   tree.  Otherwise we return zero.  */
//原型 optimize_bit_field_compare fold-const.cc
static tree optimize_bit_field_compare (MtcsConst *self,location_t loc, enum tree_code code,
             tree compare_type, tree lhs, tree rhs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   poly_int64 plbitpos, plbitsize, rbitpos, rbitsize;
   HOST_WIDE_INT lbitpos, lbitsize, nbitpos, nbitsize;
   tree type = TREE_TYPE (lhs);
   tree unsigned_type;
   int const_p = TREE_CODE (rhs) == INTEGER_CST;
   machine_mode lmode, rmode;
   scalar_int_mode nmode;
   int lunsignedp, runsignedp;
   int lreversep, rreversep;
   int lvolatilep = 0, rvolatilep = 0;
   tree linner, rinner = NULL_TREE;
   tree mask;
   tree offset;

   /* Get all the information about the extractions being done.  If the bit size
   is the same as the size of the underlying object, we aren't doing an
   extraction at all and so can do nothing.  We also don't want to
   do anything if the inner expression is a PLACEHOLDER_EXPR since we
   then will no longer be able to replace it.  */
   linner = mtcs_expr_get_inner_reference/*!get_inner_reference*/(mtcsExpr,
         lhs, &plbitsize, &plbitpos, &offset, &lmode,&lunsignedp, &lreversep, &lvolatilep);
   if (linner == lhs
   || !known_size_p (plbitsize)
   || !plbitsize.is_constant (&lbitsize)
   || !plbitpos.is_constant (&lbitpos)
   || known_eq (lbitsize, mtcs_mode_get_size/*!GET_MODE_BITSIZE*/(mtcsMode,lmode))
   || offset != 0
   || TREE_CODE (linner) == PLACEHOLDER_EXPR
   || lvolatilep)
      return 0;

   if (const_p)
      rreversep = lreversep;
   else{
      /* If this is not a constant, we can only do something if bit positions,
      sizes, signedness and storage order are the same.  */
      rinner = mtcs_expr_get_inner_reference/*!get_inner_reference*/(mtcsExpr,
            rhs, &rbitsize, &rbitpos, &offset, &rmode, &runsignedp, &rreversep, &rvolatilep);

      if (rinner == rhs
      || maybe_ne (lbitpos, rbitpos)
      || maybe_ne (lbitsize, rbitsize)
      || lunsignedp != runsignedp
      || lreversep != rreversep
      || offset != 0
      || TREE_CODE (rinner) == PLACEHOLDER_EXPR
      || rvolatilep)
         return 0;
   }

   /* Honor the C++ memory model and mimic what RTL expansion does.  */
   poly_uint64 bitstart = 0;
   poly_uint64 bitend = 0;
   if (TREE_CODE (lhs) == COMPONENT_REF){
      get_bit_range (&bitstart, &bitend, lhs, &plbitpos, &offset);
      if (!plbitpos.is_constant (&lbitpos) || offset != NULL_TREE)
         return 0;
   }

   /* See if we can find a mode to refer to this field.  We should be able to,
   but fail if we can't.  */
   if (!mtcs_mode_get_best_mode/*!get_best_mode*/(mtcsMode,lbitsize, lbitpos, bitstart, bitend,
   const_p ? TYPE_ALIGN (TREE_TYPE (linner)): MIN (TYPE_ALIGN (TREE_TYPE (linner)),
   TYPE_ALIGN (TREE_TYPE (rinner))), BITS_PER_WORD, false, &nmode))
      return 0;

   /* Set signed and unsigned types of the precision of this mode for the
   shifts below.  */
   unsigned_type = lang_hooks.types.type_for_mode (nmode, 1);

   /* Compute the bit position and size for the new reference and our offset
   within it. If the new reference is the same size as the original, we
   won't optimize anything, so return zero.  */
   nbitsize = mtcs_mode_get_size/*!GET_MODE_BITSIZE*/(mtcsMode,nmode);
   nbitpos = lbitpos & ~ (nbitsize - 1);
   lbitpos -= nbitpos;
   if (nbitsize == lbitsize)
      return 0;

   if (lreversep ? !BYTES_BIG_ENDIAN : BYTES_BIG_ENDIAN)
   lbitpos = nbitsize - lbitsize - lbitpos;

   /* Make the mask to be used against the extracted field.  */
   mask = build_int_cst_type (unsigned_type, -1);
   mask = const_binop(self,LSHIFT_EXPR, mask, size_int (nbitsize - lbitsize));
   mask = const_binop(self,RSHIFT_EXPR, mask, size_int (nbitsize - lbitsize - lbitpos));

   if (! const_p){
      if (nbitpos < 0)
         return 0;

      /* If not comparing with constant, just rework the comparison
      and return.  */
      tree t1 = make_bit_field_ref (loc, linner, lhs, unsigned_type, nbitsize, nbitpos, 1, lreversep);
      t1 = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, BIT_AND_EXPR, unsigned_type, t1, mask);
      tree t2 = make_bit_field_ref (loc, rinner, rhs, unsigned_type, nbitsize, nbitpos, 1, rreversep);
      t2 = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, BIT_AND_EXPR, unsigned_type, t2, mask);
      return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, compare_type, t1, t2);
   }

   /* Otherwise, we are handling the constant case.  See if the constant is too
   big for the field.  Warn and return a tree for 0 (false) if so.  We do
   this not only for its own sake, but to avoid having to test for this
   error case below.  If we didn't, we might generate wrong code.

   For unsigned fields, the constant shifted right by the field length should
   be all zero.  For signed fields, the high-order bits should agree with
   the sign bit.  */

   if (lunsignedp){
      if (wi::lrshift (wi::to_wide (rhs), lbitsize) != 0){
         warning (0, "comparison is always %d due to width of bit-field", code == NE_EXPR);
         return constant_boolean_node (code == NE_EXPR, compare_type);
      }
   }else{
      wide_int tem = wi::arshift (wi::to_wide (rhs), lbitsize - 1);
      if (tem != 0 && tem != -1){
         warning (0, "comparison is always %d due to width of bit-field",code == NE_EXPR);
         return constant_boolean_node (code == NE_EXPR, compare_type);
      }
   }

   if (nbitpos < 0)
      return 0;

   /* Single-bit compares should always be against zero.  */
   if (lbitsize == 1 && ! integer_zerop (rhs)){
      code = code == EQ_EXPR ? NE_EXPR : EQ_EXPR;
      rhs = mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,type, 0);
   }

   /* Make a new bitfield reference, shift the constant over the
   appropriate number of bits and mask it with the computed mask
   (in case this was a signed field).  If we changed it, make a new one.  */
   lhs = make_bit_field_ref (loc, linner, lhs, unsigned_type,nbitsize, nbitpos, 1, lreversep);

   rhs = const_binop(self,BIT_AND_EXPR, const_binop(self,LSHIFT_EXPR,
         mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, unsigned_type, rhs),size_int (lbitpos)), mask);

   lhs = build2_loc (loc, code, compare_type,build2 (BIT_AND_EXPR, unsigned_type, lhs, mask), rhs);
   return lhs;
}

//原型 extract_muldiv_1 fold-const.cc
static tree extract_muldiv_1 (MtcsConst *self,tree t, tree c, enum tree_code code, tree wide_type,
        bool *strict_overflow_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree type = TREE_TYPE (t);
   enum tree_code tcode = TREE_CODE (t);
   tree ctype = type;
   if (wide_type){
      if (TREE_CODE (type) == BITINT_TYPE || TREE_CODE (wide_type) == BITINT_TYPE){
         if (TYPE_PRECISION (wide_type) > TYPE_PRECISION (type))
            ctype = wide_type;
      }else if (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,
            mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,wide_type))
            > mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,type)))
         ctype = wide_type;
   }
   tree t1, t2;
   bool same_p = tcode == code;
   tree op0 = NULL_TREE, op1 = NULL_TREE;
   bool sub_strict_overflow_p;

   /* Don't deal with constants of zero here; they confuse the code below.  */
   if (integer_zerop (c))
      return NULL_TREE;

   if (TREE_CODE_CLASS (tcode) == tcc_unary)
      op0 = TREE_OPERAND (t, 0);

   if (TREE_CODE_CLASS (tcode) == tcc_binary)
      op0 = TREE_OPERAND (t, 0), op1 = TREE_OPERAND (t, 1);

   /* Note that we need not handle conditional operations here since fold
   already handles those cases.  So just do arithmetic here.  */
   switch (tcode){
      case INTEGER_CST:
         /* For a constant, we can always simplify if we are a multiply
         or (for divide and modulus) if it is a multiple of our constant.  */
         if (code == MULT_EXPR || wi::multiple_of_p (wi::to_wide (t), wi::to_wide (c), TYPE_SIGN (type))){
            tree tem = const_binop(self,code, mtcs_const_fold_convert/*!fold_convert*/(self,ctype, t),
                  mtcs_const_fold_convert/*!fold_convert*/(self,ctype, c));
            /* If the multiplication overflowed, we lost information on it.
            See PR68142 and PR69845.  */
            if (TREE_OVERFLOW (tem))
               return NULL_TREE;
            return tem;
         }
         break;

      CASE_CONVERT: case NON_LVALUE_EXPR:
         if (!INTEGRAL_TYPE_P (TREE_TYPE (op0)))
            break;
         /* If op0 is an expression ...  */
         if ((COMPARISON_CLASS_P (op0)
         || UNARY_CLASS_P (op0)
         || BINARY_CLASS_P (op0)
         || VL_EXP_CLASS_P (op0)
         || EXPRESSION_CLASS_P (op0))
         /* ... and has wrapping overflow, and its type is smaller
         than ctype, then we cannot pass through as widening.  */
         && ((TYPE_OVERFLOW_WRAPS (TREE_TYPE (op0))
         && (TYPE_PRECISION (ctype)  > TYPE_PRECISION (TREE_TYPE (op0))))
         /* ... or this is a truncation (t is narrower than op0),
         then we cannot pass through this narrowing.  */
         || (TYPE_PRECISION (type)  < TYPE_PRECISION (TREE_TYPE (op0)))
         /* ... or signedness changes for division or modulus,
         then we cannot pass through this conversion.  */
         || (code != MULT_EXPR  && (TYPE_UNSIGNED (ctype) != TYPE_UNSIGNED (TREE_TYPE (op0))))
         /* ... or has undefined overflow while the converted to
         type has not, we cannot do the operation in the inner type
         as that would introduce undefined overflow.  */
         || (TYPE_OVERFLOW_UNDEFINED (TREE_TYPE (op0))  && !TYPE_OVERFLOW_UNDEFINED (type))))
            break;

         /* Pass the constant down and see if we can make a simplification.  If
         we can, replace this expression with the inner simplification for
         possible later conversion to our or some other type.  */
         if ((t2 = mtcs_const_fold_convert/*!fold_convert*/(self,TREE_TYPE (op0), c)) != 0
         && TREE_CODE (t2) == INTEGER_CST
         && !TREE_OVERFLOW (t2)
         && (t1 = extract_muldiv(self,op0, t2, code, code == MULT_EXPR ? ctype : NULL_TREE, strict_overflow_p)) != 0)
            return t1;
         break;

      case ABS_EXPR:
         /* If widening the type changes it from signed to unsigned, then we
         must avoid building ABS_EXPR itself as unsigned.  */
         if (TYPE_UNSIGNED (ctype) && !TYPE_UNSIGNED (type)){
            tree cstype = (*signed_type_for) (ctype);
            if ((t1 = extract_muldiv(self,op0, c, code, cstype, strict_overflow_p))!= 0){
               t1 = fold_build1 (tcode, cstype, mtcs_const_fold_convert/*!fold_convert*/(self,cstype, t1));
               return mtcs_const_fold_convert/*!fold_convert*/(self,ctype, t1);
            }
            break;
         }
         /* If the constant is negative, we cannot simplify this.  */
         if (tree_int_cst_sgn (c) == -1)
            break;
      /* FALLTHROUGH */
      case NEGATE_EXPR:
         /* For division and modulus, type can't be unsigned, as e.g.
         (-(x / 2U)) / 2U isn't equal to -((x / 2U) / 2U) for x >= 2.
         For signed types, even with wrapping overflow, this is fine.  */
         if (code != MULT_EXPR && TYPE_UNSIGNED (type))
            break;
         if ((t1 = extract_muldiv(self,op0, c, code, wide_type, strict_overflow_p)) != 0)
            return fold_build1 (tcode, ctype, mtcs_const_fold_convert/*!fold_convert*/(self,ctype, t1));
         break;

      case MIN_EXPR:  case MAX_EXPR:
         /* If widening the type changes the signedness, then we can't perform
         this optimization as that changes the result.  */
         if (TYPE_UNSIGNED (ctype) != TYPE_UNSIGNED (type))
            break;

         /* Punt for multiplication altogether.
         MAX (1U + INT_MAX, 1U) * 2U is not equivalent to
         MAX ((1U + INT_MAX) * 2U, 1U * 2U), the former is
         0U, the latter is 2U.
         MAX (INT_MIN / 2, 0) * -2 is not equivalent to
         MIN (INT_MIN / 2 * -2, 0 * -2), the former is
         well defined 0, the latter invokes UB.
         MAX (INT_MIN / 2, 5) * 5 is not equivalent to
         MAX (INT_MIN / 2 * 5, 5 * 5), the former is
         well defined 25, the latter invokes UB.  */
         if (code == MULT_EXPR)
            break;
         /* For division/modulo, punt on c being -1 for MAX, as
         MAX (INT_MIN, 0) / -1 is not equivalent to
         MIN (INT_MIN / -1, 0 / -1), the former is well defined
         0, the latter invokes UB (or for -fwrapv is INT_MIN).
         MIN (INT_MIN, 0) / -1 already invokes UB, so the
         transformation won't make it worse.  */
         else if (tcode == MAX_EXPR && integer_minus_onep (c))
            break;

         /* MIN (a, b) / 5 -> MIN (a / 5, b / 5)  */
         sub_strict_overflow_p = false;
         if ((t1 = extract_muldiv(self,op0, c, code, wide_type, &sub_strict_overflow_p)) != 0
         && (t2 = extract_muldiv(self,op1, c, code, wide_type,  &sub_strict_overflow_p)) != 0){
            if (tree_int_cst_sgn (c) < 0)
               tcode = (tcode == MIN_EXPR ? MAX_EXPR : MIN_EXPR);
            if (sub_strict_overflow_p)
               *strict_overflow_p = true;
            return mtcs_const_fold_build2/*!fold_build2*/(self,tcode, ctype, mtcs_const_fold_convert/*!fold_convert*/(self,ctype, t1),
                  mtcs_const_fold_convert/*!fold_convert*/(self,ctype, t2));
         }
         break;

      case LSHIFT_EXPR:  case RSHIFT_EXPR:
         /* If the second operand is constant, this is a multiplication
         or floor division, by a power of two, so we can treat it that
         way unless the multiplier or divisor overflows.  Signed
         left-shift overflow is implementation-defined rather than
         undefined in C90, so do not convert signed left shift into
         multiplication.  */
         if (TREE_CODE (op1) == INTEGER_CST
         && (tcode == RSHIFT_EXPR || TYPE_UNSIGNED (TREE_TYPE (op0)))
         /* const_binop may not detect overflow correctly,
         so check for it explicitly here.  */
         && wi::gtu_p (TYPE_PRECISION (TREE_TYPE (size_one_node)), wi::to_wide (op1))
         && (t1 = mtcs_const_fold_convert/*!fold_convert*/(self,ctype,const_binop(self,LSHIFT_EXPR, size_one_node, op1))) != 0
         && !TREE_OVERFLOW (t1))
            return extract_muldiv(self,build2 (tcode == LSHIFT_EXPR
                  ? MULT_EXPR : FLOOR_DIV_EXPR, ctype,mtcs_const_fold_convert/*!fold_convert*/(self,ctype, op0),t1),
                  c, code, wide_type, strict_overflow_p);
         break;

      case PLUS_EXPR:  case MINUS_EXPR:
         /* See if we can eliminate the operation on both sides.  If we can, we
         can return a new PLUS or MINUS.  If we can't, the only remaining
         cases where we can do anything are if the second operand is a
         constant.  */
         sub_strict_overflow_p = false;
         t1 = extract_muldiv(self,op0, c, code, wide_type, &sub_strict_overflow_p);
         t2 = extract_muldiv(self,op1, c, code, wide_type, &sub_strict_overflow_p);
         if (t1 != 0 && t2 != 0  && TYPE_OVERFLOW_WRAPS (ctype) && (code == MULT_EXPR
         /* If not multiplication, we can only do this if both operands
         are divisible by c.  */
         || (multiple_of_p (ctype, op0, c) && multiple_of_p (ctype, op1, c)))){
            if (sub_strict_overflow_p)
               *strict_overflow_p = true;
            return mtcs_const_fold_build2/*!fold_build2*/(self,tcode, ctype, mtcs_const_fold_convert/*!fold_convert*/(self,ctype, t1),
                  mtcs_const_fold_convert/*!fold_convert*/(self,ctype, t2));
         }

         /* If this was a subtraction, negate OP1 and set it to be an addition.
         This simplifies the logic below.  */
         if (tcode == MINUS_EXPR){
            tcode = PLUS_EXPR, op1 = negate_expr(self,op1);
            /* If OP1 was not easily negatable, the constant may be OP0.  */
            if (TREE_CODE (op0) == INTEGER_CST){
               std::swap (op0, op1);
               std::swap (t1, t2);
            }
         }

         if (TREE_CODE (op1) != INTEGER_CST)
            break;

         /* If either OP1 or C are negative, this optimization is not safe for
         some of the division and remainder types while for others we need
         to change the code.  */
         if (tree_int_cst_sgn (op1) < 0 || tree_int_cst_sgn (c) < 0){
            if (code == CEIL_DIV_EXPR)
               code = FLOOR_DIV_EXPR;
            else if (code == FLOOR_DIV_EXPR)
               code = CEIL_DIV_EXPR;
            else if (code != MULT_EXPR && code != CEIL_MOD_EXPR && code != FLOOR_MOD_EXPR)
               break;
         }

         /* If it's a multiply or a division/modulus operation of a multiple
         of our constant, do the operation and verify it doesn't overflow.  */
         if (code == MULT_EXPR|| wi::multiple_of_p (wi::to_wide (op1), wi::to_wide (c),TYPE_SIGN (type))){
            op1 = const_binop(self,code, mtcs_const_fold_convert/*!fold_convert*/(self,ctype, op1),
                  mtcs_const_fold_convert/*!fold_convert*/(self,ctype, c));
            /* We allow the constant to overflow with wrapping semantics.  */
            if (op1 == 0 || (TREE_OVERFLOW (op1) && !TYPE_OVERFLOW_WRAPS (ctype)))
               break;
         }else
            break;

         /* If we have an unsigned type, we cannot widen the operation since it
         will change the result if the original computation overflowed.  */
         if (TYPE_UNSIGNED (ctype) && ctype != type)
            break;

         /* The last case is if we are a multiply.  In that case, we can
         apply the distributive law to commute the multiply and addition
         if the multiplication of the constants doesn't overflow
         and overflow is defined.  With undefined overflow
         op0 * c might overflow, while (op0 + orig_op1) * c doesn't.
         But fold_plusminus_mult_expr would factor back any power-of-two
         value so do not distribute in the first place in this case.  */
         if (code == MULT_EXPR && TYPE_OVERFLOW_WRAPS (ctype)
         && !(tree_fits_shwi_p (c) && pow2p_hwi (absu_hwi (tree_to_shwi (c)))))
            return mtcs_const_fold_build2/*!fold_build2*/(self,tcode, ctype,
               mtcs_const_fold_build2/*!fold_build2*/(self,code, ctype, mtcs_const_fold_convert/*!fold_convert*/(self,ctype, op0),
               mtcs_const_fold_convert/*!fold_convert*/(self,ctype, c)), op1);

         break;

      case MULT_EXPR:
         /* We have a special case here if we are doing something like
         (C * 8) % 4 since we know that's zero.  */
         if ((code == TRUNC_MOD_EXPR || code == CEIL_MOD_EXPR
         || code == FLOOR_MOD_EXPR || code == ROUND_MOD_EXPR)
         /* If the multiplication can overflow we cannot optimize this.  */
         && TYPE_OVERFLOW_UNDEFINED (TREE_TYPE (t))
         && TREE_CODE (TREE_OPERAND (t, 1)) == INTEGER_CST
         && wi::multiple_of_p (wi::to_wide (op1), wi::to_wide (c),
         TYPE_SIGN (type))){
            *strict_overflow_p = true;
            return omit_one_operand (type, integer_zero_node, op0);
         }

         /* ... fall through ...  */

      case TRUNC_DIV_EXPR:  case CEIL_DIV_EXPR:  case FLOOR_DIV_EXPR:
      case ROUND_DIV_EXPR:  case EXACT_DIV_EXPR:
         /* If we can extract our operation from the LHS, do so and return a
         new operation.  Likewise for the RHS from a MULT_EXPR.  Otherwise,
         do something only if the second operand is a constant.  */
         if (same_p  && TYPE_OVERFLOW_WRAPS (ctype)
         && (t1 = extract_muldiv(self,op0, c, code, wide_type, strict_overflow_p)) != 0)
            return mtcs_const_fold_build2/*!fold_build2*/(self,tcode, ctype, mtcs_const_fold_convert/*!fold_convert*/(self,ctype, t1),
                  mtcs_const_fold_convert/*!fold_convert*/(self,ctype, op1));
         else if (tcode == MULT_EXPR && code == MULT_EXPR  && TYPE_OVERFLOW_WRAPS (ctype)
         && (t1 = extract_muldiv(self,op1, c, code, wide_type, strict_overflow_p)) != 0)
            return mtcs_const_fold_build2/*!fold_build2*/(self,tcode, ctype, mtcs_const_fold_convert/*!fold_convert*/(self,ctype, op0),
                  mtcs_const_fold_convert/*!fold_convert*/(self,ctype, t1));
         else if (TREE_CODE (op1) != INTEGER_CST)
            return 0;

         /* If these are the same operation types, we can associate them
         assuming no overflow.  */
         if (tcode == code){
            bool overflow_p = false;
            wi::overflow_type overflow_mul;
            signop sign = TYPE_SIGN (ctype);
            unsigned prec = TYPE_PRECISION (ctype);
            wide_int mul = wi::mul (wi::to_wide (op1, prec),
            wi::to_wide (c, prec), sign, &overflow_mul);
            overflow_p = TREE_OVERFLOW (c) | TREE_OVERFLOW (op1);
            if (overflow_mul  && ((sign == UNSIGNED && tcode != MULT_EXPR) || sign == SIGNED))
               overflow_p = true;
            if (!overflow_p)
               return mtcs_const_fold_build2/*!fold_build2*/(self,tcode, ctype, mtcs_const_fold_convert/*!fold_convert*/(self,ctype, op0),
                     mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,ctype, mul));
         }

         /* If these operations "cancel" each other, we have the main
         optimizations of this pass, which occur when either constant is a
         multiple of the other, in which case we replace this with either an
         operation or CODE or TCODE.

         If we have an unsigned type, we cannot do this since it will change
         the result if the original computation overflowed.  */
         if (TYPE_OVERFLOW_UNDEFINED (ctype)
         && !TYPE_OVERFLOW_SANITIZED (ctype)
         && ((code == MULT_EXPR && tcode == EXACT_DIV_EXPR)
         || (tcode == MULT_EXPR
         && code != TRUNC_MOD_EXPR && code != CEIL_MOD_EXPR
         && code != FLOOR_MOD_EXPR && code != ROUND_MOD_EXPR
         && code != MULT_EXPR))){
            if (wi::multiple_of_p (wi::to_wide (op1), wi::to_wide (c), TYPE_SIGN (type))){
               *strict_overflow_p = true;
               return mtcs_const_fold_build2/*!fold_build2*/(self,tcode, ctype, mtcs_const_fold_convert/*!fold_convert*/(self,ctype, op0),
                        mtcs_const_fold_convert/*!fold_convert*/(self,ctype, const_binop(self,TRUNC_DIV_EXPR, op1, c)));
            }else if (wi::multiple_of_p (wi::to_wide (c), wi::to_wide (op1), TYPE_SIGN (type))){
               *strict_overflow_p = true;
               return mtcs_const_fold_build2/*!fold_build2*/(self,code, ctype, mtcs_const_fold_convert/*!fold_convert*/(self,ctype, op0),
                     mtcs_const_fold_convert/*!fold_convert*/(self,ctype,  const_binop(self,TRUNC_DIV_EXPR, c, op1)));
            }
         }
         break;

      default:
         break;
   }

   return 0;
}

/* T is an integer expression that is being multiplied, divided, or taken a
   modulus (CODE says which and what kind of divide or modulus) by a
   constant C.  See if we can eliminate that operation by folding it with
   other operations already in T.  WIDE_TYPE, if non-null, is a type that
   should be used for the computation if wider than our type.

   For example, if we are dividing (X * 8) + (Y * 16) by 4, we can return
   (X * 2) + (Y * 4).  We must, however, be assured that either the original
   expression would not overflow or that overflow is undefined for the type
   in the language in question.

   If we return a non-null expression, it is an equivalent form of the
   original computation, but need not be in the original type.

   We set *STRICT_OVERFLOW_P to true if the return values depends on
   signed overflow being undefined.  Otherwise we do not change
   *STRICT_OVERFLOW_P.  */
//原型 extract_muldiv fold-const.cc
static tree extract_muldiv (MtcsConst *self,tree t, tree c, enum tree_code code, tree wide_type,
      bool *strict_overflow_p)
{
   /* To avoid exponential search depth, refuse to allow recursion past
   three levels.  Beyond that (1) it's highly unlikely that we'll find
   something interesting and (2) we've probably processed it before
   when we built the inner expression.  */

   static int depth;
   tree ret;

   if (depth > 3)
      return NULL;

   depth++;
   ret = extract_muldiv_1(self,t, c, code, wide_type, strict_overflow_p);
   depth--;

   return ret;
}

/* Fold A < X && A + 1 > Y to A < X && A >= Y.  Normally A + 1 > Y
   means A >= Y && A != MAX, but in this case we know that
   A < X <= MAX.  INEQ is A + 1 > Y, BOUND is A < X.  */
//原型 fold_to_nonsharp_ineq_using_bound fold-const.cc
static tree fold_to_nonsharp_ineq_using_bound (MtcsConst *self,location_t loc, tree ineq, tree bound)
{
   tree a, typea, type = TREE_TYPE (bound), a1, diff, y;

   if (TREE_CODE (bound) == LT_EXPR)
      a = TREE_OPERAND (bound, 0);
   else if (TREE_CODE (bound) == GT_EXPR)
      a = TREE_OPERAND (bound, 1);
   else
      return NULL_TREE;

   typea = TREE_TYPE (a);
   if (!INTEGRAL_TYPE_P (typea)  && !POINTER_TYPE_P (typea))
      return NULL_TREE;

   if (TREE_CODE (ineq) == LT_EXPR){
      a1 = TREE_OPERAND (ineq, 1);
      y = TREE_OPERAND (ineq, 0);
   }else if (TREE_CODE (ineq) == GT_EXPR){
      a1 = TREE_OPERAND (ineq, 0);
      y = TREE_OPERAND (ineq, 1);
   }else
      return NULL_TREE;

   if (TREE_TYPE (a1) != typea)
      return NULL_TREE;

   if (POINTER_TYPE_P (typea)){
      /* Convert the pointer types into integer before taking the difference.  */
      tree ta = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, ssizetype, a);
      tree ta1 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, ssizetype, a1);
      diff = mtcs_const_fold_binary_loc/*!fold_binary_loc*/(self,loc, MINUS_EXPR, ssizetype, ta1, ta);
   }else
      diff = mtcs_const_fold_binary_loc/*!fold_binary_loc*/(self,loc, MINUS_EXPR, typea, a1, a);

   if (!diff || !integer_onep (diff))
      return NULL_TREE;

   return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, GE_EXPR, type, a, y);
}

/* Helper that tries to canonicalize the comparison ARG0 CODE ARG1
   by changing CODE to reduce the magnitude of constants involved in
   ARG0 of the comparison.
   Returns a canonicalized comparison tree if a simplification was
   possible, otherwise returns NULL_TREE.
   Set *STRICT_OVERFLOW_P to true if the canonicalization is only
   valid if signed overflow is undefined.  */
//原型 maybe_canonicalize_comparison_1 fold-const.cc
static tree maybe_canonicalize_comparison_1 (MtcsConst *self,location_t loc, enum tree_code code, tree type,
             tree arg0, tree arg1,
             bool *strict_overflow_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree  *mtcsTree =mtcs_target_get_tree(mtcsTarget);

   enum tree_code code0 = TREE_CODE (arg0);
   tree t, cst0 = NULL_TREE;
   int sgn0;

   /* Match A +- CST code arg1.  We can change this only if overflow
   is undefined.  */
   if (!((ANY_INTEGRAL_TYPE_P (TREE_TYPE (arg0))
   && TYPE_OVERFLOW_UNDEFINED (TREE_TYPE (arg0)))
   /* In principle pointers also have undefined overflow behavior,
   but that causes problems elsewhere.  */
   && !POINTER_TYPE_P (TREE_TYPE (arg0))
   && (code0 == MINUS_EXPR || code0 == PLUS_EXPR)
   && TREE_CODE (TREE_OPERAND (arg0, 1)) == INTEGER_CST))
      return NULL_TREE;

   /* Identify the constant in arg0 and its sign.  */
   cst0 = TREE_OPERAND (arg0, 1);
   sgn0 = tree_int_cst_sgn (cst0);

   /* Overflowed constants and zero will cause problems.  */
   if (integer_zerop (cst0) || TREE_OVERFLOW (cst0))
      return NULL_TREE;

   /* See if we can reduce the magnitude of the constant in
   arg0 by changing the comparison code.  */
   /* A - CST < arg1  ->  A - CST-1 <= arg1.  */
   if (code == LT_EXPR && code0 == ((sgn0 == -1) ? PLUS_EXPR : MINUS_EXPR))
      code = LE_EXPR;
   /* A + CST > arg1  ->  A + CST-1 >= arg1.  */
   else if (code == GT_EXPR  && code0 == ((sgn0 == -1) ? MINUS_EXPR : PLUS_EXPR))
      code = GE_EXPR;
   /* A + CST <= arg1  ->  A + CST-1 < arg1.  */
   else if (code == LE_EXPR  && code0 == ((sgn0 == -1) ? MINUS_EXPR : PLUS_EXPR))
      code = LT_EXPR;
   /* A - CST >= arg1  ->  A - CST-1 > arg1.  */
   else if (code == GE_EXPR  && code0 == ((sgn0 == -1) ? PLUS_EXPR : MINUS_EXPR))
      code = GT_EXPR;
   else
      return NULL_TREE;
   *strict_overflow_p = true;

   /* Now build the constant reduced in magnitude.  But not if that
   would produce one outside of its types range.  */
   if (INTEGRAL_TYPE_P (TREE_TYPE (cst0))
   && ((sgn0 == 1 && TYPE_MIN_VALUE (TREE_TYPE (cst0))
   && tree_int_cst_equal (cst0, TYPE_MIN_VALUE (TREE_TYPE (cst0))))
   || (sgn0 == -1 && TYPE_MAX_VALUE (TREE_TYPE (cst0))
   && tree_int_cst_equal (cst0, TYPE_MAX_VALUE (TREE_TYPE (cst0))))))
      return NULL_TREE;

   t = mtcs_const_int_const_binop/*!int_const_binop*/(self,sgn0 == -1 ? PLUS_EXPR : MINUS_EXPR,cst0, mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (cst0), 1));
   t = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code0, TREE_TYPE (arg0), TREE_OPERAND (arg0, 0), t);
   t = mtcs_const_fold_convert/*!fold_convert*/(self,TREE_TYPE (arg1), t);
   return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, type, t, arg1);
}

/* Canonicalize the comparison ARG0 CODE ARG1 with type TYPE with undefined
   overflow further.  Try to decrease the magnitude of constants involved
   by changing LE_EXPR and GE_EXPR to LT_EXPR and GT_EXPR or vice versa
   and put sole constants at the second argument position.
   Returns the canonicalized tree if changed, otherwise NULL_TREE.  */
//原型 maybe_canonicalize_comparison fold-const.cc
static tree maybe_canonicalize_comparison (MtcsConst *self,location_t loc, enum tree_code code, tree type,
                tree arg0, tree arg1)
{
   tree t;
   bool strict_overflow_p;
   const char * const warnmsg = G_("assuming signed overflow does not occur "
         "when reducing constant in comparison");

   /* Try canonicalization by simplifying arg0.  */
   strict_overflow_p = false;
   t = maybe_canonicalize_comparison_1(self,loc, code, type, arg0, arg1, &strict_overflow_p);
   if (t){
      if (strict_overflow_p)
         fold_overflow_warning (warnmsg, WARN_STRICT_OVERFLOW_MAGNITUDE);
      return t;
   }

   /* Try canonicalization by simplifying arg1 using the swapped
   comparison.  */
   code = swap_tree_comparison (code);
   strict_overflow_p = false;
   t = maybe_canonicalize_comparison_1(self,loc, code, type, arg1, arg0, &strict_overflow_p);
   if (t && strict_overflow_p)
      fold_overflow_warning (warnmsg, WARN_STRICT_OVERFLOW_MAGNITUDE);
   return t;
}

/* Subroutine of fold_binary.  This routine performs all of the
   transformations that are common to the equality/inequality
   operators (EQ_EXPR and NE_EXPR) and the ordering operators
   (LT_EXPR, LE_EXPR, GE_EXPR and GT_EXPR).  Callers other than
   fold_binary should call fold_binary.  Fold a comparison with
   tree code CODE and type TYPE with operands OP0 and OP1.  Return
   the folded comparison or NULL_TREE.  */
//原型 fold_comparison fold-const.cc
static tree fold_comparison (MtcsConst *self,location_t loc, enum tree_code code, tree type,
       tree op0, tree op1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsTree  *mtcsTree =mtcs_target_get_tree(mtcsTarget);

   const bool equality_code = (code == EQ_EXPR || code == NE_EXPR);
   tree arg0, arg1, tem;

   arg0 = op0;
   arg1 = op1;

   STRIP_SIGN_NOPS (arg0);
   STRIP_SIGN_NOPS (arg1);

   /* For comparisons of pointers we can decompose it to a compile time
   comparison of the base objects and the offsets into the object.
   This requires at least one operand being an ADDR_EXPR or a
   POINTER_PLUS_EXPR to do more than the operand_equal_p test below.  */
   if (POINTER_TYPE_P (TREE_TYPE (arg0))
   && (TREE_CODE (arg0) == ADDR_EXPR
   || TREE_CODE (arg1) == ADDR_EXPR
   || TREE_CODE (arg0) == POINTER_PLUS_EXPR
   || TREE_CODE (arg1) == POINTER_PLUS_EXPR)){
      tree base0, base1, offset0 = NULL_TREE, offset1 = NULL_TREE;
      poly_int64 bitsize, bitpos0 = 0, bitpos1 = 0;
      machine_mode mode;
      int volatilep, reversep, unsignedp;
      bool indirect_base0 = false, indirect_base1 = false;

      /* Get base and offset for the access.  Strip ADDR_EXPR for
      get_inner_reference, but put it back by stripping INDIRECT_REF
      off the base object if possible.  indirect_baseN will be true
      if baseN is not an address but refers to the object itself.  */
      base0 = arg0;
      if (TREE_CODE (arg0) == ADDR_EXPR){
         base0 = mtcs_expr_get_inner_reference/*!get_inner_reference*/(mtcsExpr,TREE_OPERAND (arg0, 0),
               &bitsize, &bitpos0, &offset0, &mode, &unsignedp, &reversep, &volatilep);
         if (INDIRECT_REF_P (base0))
            base0 = TREE_OPERAND (base0, 0);
         else
            indirect_base0 = true;
      }else if (TREE_CODE (arg0) == POINTER_PLUS_EXPR){
         base0 = TREE_OPERAND (arg0, 0);
         STRIP_SIGN_NOPS (base0);
         if (TREE_CODE (base0) == ADDR_EXPR){
            base0 = mtcs_expr_get_inner_reference/*!get_inner_reference*/(mtcsExpr,TREE_OPERAND (base0, 0),
                  &bitsize, &bitpos0, &offset0, &mode, &unsignedp, &reversep, &volatilep);
            if (INDIRECT_REF_P (base0))
               base0 = TREE_OPERAND (base0, 0);
            else
               indirect_base0 = true;
         }
         if (offset0 == NULL_TREE || integer_zerop (offset0))
            offset0 = TREE_OPERAND (arg0, 1);
         else
            offset0 = size_binop (PLUS_EXPR, offset0,
         TREE_OPERAND (arg0, 1));
         if (poly_int_tree_p (offset0)){
            poly_offset_int tem = wi::sext (wi::to_poly_offset (offset0),TYPE_PRECISION (sizetype));
            tem <<= LOG2_BITS_PER_UNIT;
            tem += bitpos0;
            if (tem.to_shwi (&bitpos0))
               offset0 = NULL_TREE;
         }
      }

      base1 = arg1;
      if (TREE_CODE (arg1) == ADDR_EXPR){
         base1 = mtcs_expr_get_inner_reference/*!get_inner_reference*/(mtcsExpr,TREE_OPERAND (arg1, 0),
         &bitsize, &bitpos1, &offset1, &mode, &unsignedp, &reversep, &volatilep);
         if (INDIRECT_REF_P (base1))
            base1 = TREE_OPERAND (base1, 0);
         else
            indirect_base1 = true;
      }else if (TREE_CODE (arg1) == POINTER_PLUS_EXPR){
         base1 = TREE_OPERAND (arg1, 0);
         STRIP_SIGN_NOPS (base1);
         if (TREE_CODE (base1) == ADDR_EXPR){
            base1= mtcs_expr_get_inner_reference/*!get_inner_reference*/(mtcsExpr,TREE_OPERAND (base1, 0),
                  &bitsize, &bitpos1, &offset1, &mode,&unsignedp, &reversep, &volatilep);
            if (INDIRECT_REF_P (base1))
               base1 = TREE_OPERAND (base1, 0);
            else
               indirect_base1 = true;
         }
         if (offset1 == NULL_TREE || integer_zerop (offset1))
            offset1 = TREE_OPERAND (arg1, 1);
         else
            offset1 = size_binop (PLUS_EXPR, offset1,TREE_OPERAND (arg1, 1));
         if (poly_int_tree_p (offset1)){
            poly_offset_int tem = wi::sext (wi::to_poly_offset (offset1),TYPE_PRECISION (sizetype));
            tem <<= LOG2_BITS_PER_UNIT;
            tem += bitpos1;
            if (tem.to_shwi (&bitpos1))
               offset1 = NULL_TREE;
         }
      }

      /* If we have equivalent bases we might be able to simplify.  */
      if (indirect_base0 == indirect_base1
      && operand_equal_p (base0, base1,indirect_base0 ? OEP_ADDRESS_OF : 0)){
         /* We can fold this expression to a constant if the non-constant
         offset parts are equal.  */
         if ((offset0 == offset1 || (offset0 && offset1
         && operand_equal_p (offset0, offset1, 0)))
         && (equality_code || (indirect_base0 && (DECL_P (base0) || CONSTANT_CLASS_P (base0)))
         || TYPE_OVERFLOW_UNDEFINED (TREE_TYPE (arg0)))){
            if (!equality_code
            && maybe_ne (bitpos0, bitpos1)
            && (pointer_may_wrap_p (base0, offset0, bitpos0)
            || pointer_may_wrap_p (base1, offset1, bitpos1)))
               fold_overflow_warning (("assuming pointer wraparound does not "
                     "occur when comparing P +- C1 with P +- C2"),WARN_STRICT_OVERFLOW_CONDITIONAL);

            switch (code){
               case EQ_EXPR:
                  if (known_eq (bitpos0, bitpos1))
                     return constant_boolean_node (true, type);
                  if (known_ne (bitpos0, bitpos1))
                     return constant_boolean_node (false, type);
                  break;
               case NE_EXPR:
                  if (known_ne (bitpos0, bitpos1))
                     return constant_boolean_node (true, type);
                  if (known_eq (bitpos0, bitpos1))
                     return constant_boolean_node (false, type);
                  break;
               case LT_EXPR:
                  if (known_lt (bitpos0, bitpos1))
                     return constant_boolean_node (true, type);
                  if (known_ge (bitpos0, bitpos1))
                     return constant_boolean_node (false, type);
                  break;
               case LE_EXPR:
                  if (known_le (bitpos0, bitpos1))
                     return constant_boolean_node (true, type);
                  if (known_gt (bitpos0, bitpos1))
                     return constant_boolean_node (false, type);
                  break;
               case GE_EXPR:
                  if (known_ge (bitpos0, bitpos1))
                     return constant_boolean_node (true, type);
                  if (known_lt (bitpos0, bitpos1))
                     return constant_boolean_node (false, type);
                  break;
               case GT_EXPR:
                  if (known_gt (bitpos0, bitpos1))
                     return constant_boolean_node (true, type);
                  if (known_le (bitpos0, bitpos1))
                     return constant_boolean_node (false, type);
                  break;
               default:;
            }
         }
         /* We can simplify the comparison to a comparison of the variable
         offset parts if the constant offset parts are equal.
         Be careful to use signed sizetype here because otherwise we
         mess with array offsets in the wrong way.  This is possible
         because pointer arithmetic is restricted to retain within an
         object and overflow on pointer differences is undefined as of
         6.5.6/8 and /9 with respect to the signed ptrdiff_t.  */
         else if (known_eq (bitpos0, bitpos1) && (equality_code || (indirect_base0
         && (DECL_P (base0) || CONSTANT_CLASS_P (base0))) || TYPE_OVERFLOW_UNDEFINED (TREE_TYPE (arg0)))){
            /* By converting to signed sizetype we cover middle-end pointer
            arithmetic which operates on unsigned pointer types of size
            type size and ARRAY_REF offsets which are properly sign or
            zero extended from their type in case it is narrower than
            sizetype.  */
            if (offset0 == NULL_TREE)
               offset0 = mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,ssizetype, 0);
            else
               offset0 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, ssizetype, offset0);
            if (offset1 == NULL_TREE)
               offset1 = mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,ssizetype, 0);
            else
               offset1 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, ssizetype, offset1);

            if (!equality_code  && (pointer_may_wrap_p (base0, offset0, bitpos0)
            || pointer_may_wrap_p (base1, offset1, bitpos1)))
               fold_overflow_warning (("assuming pointer wraparound does not "
                     "occur when comparing P +- C1 with P +- C2"), WARN_STRICT_OVERFLOW_COMPARISON);

            return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, type, offset0, offset1);
         }
      }
      /* For equal offsets we can simplify to a comparison of the
      base addresses.  */
      else if (known_eq (bitpos0, bitpos1)
      && (indirect_base0
      ? base0 != TREE_OPERAND (arg0, 0) : base0 != arg0)
      && (indirect_base1
      ? base1 != TREE_OPERAND (arg1, 0) : base1 != arg1)
      && ((offset0 == offset1)
      || (offset0 && offset1
      && operand_equal_p (offset0, offset1, 0)))){
         if (indirect_base0)
            base0 = build_fold_addr_expr_loc (loc, base0);
         if (indirect_base1)
            base1 = build_fold_addr_expr_loc (loc, base1);
         return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, type, base0, base1);
      }
      /* Comparison between an ordinary (non-weak) symbol and a null
      pointer can be eliminated since such symbols must have a non
      null address.  In C, relational expressions between pointers
      to objects and null pointers are undefined.  The results
      below follow the C++ rules with the additional property that
      every object pointer compares greater than a null pointer.
      */
      else if (((DECL_P (base0)
      && maybe_nonzero_address (base0) > 0
      /* Avoid folding references to struct members at offset 0 to
      prevent tests like '&ptr->firstmember == 0' from getting
      eliminated.  When ptr is null, although the -> expression
      is strictly speaking invalid, GCC retains it as a matter
      of QoI.  See PR c/44555. */
      && (offset0 == NULL_TREE && known_ne (bitpos0, 0)))
      || CONSTANT_CLASS_P (base0))
      && indirect_base0
      /* The caller guarantees that when one of the arguments is
      constant (i.e., null in this case) it is second.  */
      && integer_zerop (arg1)){
         switch (code){
            case EQ_EXPR:
            case LE_EXPR:
            case LT_EXPR:
               return constant_boolean_node (false, type);
            case GE_EXPR:
            case GT_EXPR:
            case NE_EXPR:
               return constant_boolean_node (true, type);
            default:
               gcc_unreachable ();
         }
      }
   }

   /* Transform comparisons of the form X +- C1 CMP Y +- C2 to
   X CMP Y +- C2 +- C1 for signed X, Y.  This is valid if
   the resulting offset is smaller in absolute value than the
   original one and has the same sign.  */
   if (ANY_INTEGRAL_TYPE_P (TREE_TYPE (arg0))
   && TYPE_OVERFLOW_UNDEFINED (TREE_TYPE (arg0))
   && (TREE_CODE (arg0) == PLUS_EXPR || TREE_CODE (arg0) == MINUS_EXPR)
   && (TREE_CODE (TREE_OPERAND (arg0, 1)) == INTEGER_CST
   && !TREE_OVERFLOW (TREE_OPERAND (arg0, 1)))
   && (TREE_CODE (arg1) == PLUS_EXPR || TREE_CODE (arg1) == MINUS_EXPR)
   && (TREE_CODE (TREE_OPERAND (arg1, 1)) == INTEGER_CST
   && !TREE_OVERFLOW (TREE_OPERAND (arg1, 1)))){
      tree const1 = TREE_OPERAND (arg0, 1);
      tree const2 = TREE_OPERAND (arg1, 1);
      tree variable1 = TREE_OPERAND (arg0, 0);
      tree variable2 = TREE_OPERAND (arg1, 0);
      tree cst;
      const char * const warnmsg = G_("assuming signed overflow does not "
               "occur when combining constants around a comparison");

      /* Put the constant on the side where it doesn't overflow and is
      of lower absolute value and of same sign than before.  */
      cst = mtcs_const_int_const_binop/*!int_const_binop*/(self,
            TREE_CODE (arg0) == TREE_CODE (arg1) ? MINUS_EXPR : PLUS_EXPR, const2, const1);
      if (!TREE_OVERFLOW (cst)
      && tree_int_cst_compare (const2, cst) == tree_int_cst_sgn (const2)
      && tree_int_cst_sgn (cst) == tree_int_cst_sgn (const2)){
         fold_overflow_warning (warnmsg, WARN_STRICT_OVERFLOW_COMPARISON);
         return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,
               loc, code, type, variable1,
               mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, TREE_CODE (arg1),TREE_TYPE (arg1),variable2, cst));
      }

      cst = mtcs_const_int_const_binop/*!int_const_binop*/(self,TREE_CODE (arg0) == TREE_CODE (arg1) ? MINUS_EXPR : PLUS_EXPR,const1, const2);
      if (!TREE_OVERFLOW (cst)
      && tree_int_cst_compare (const1, cst) == tree_int_cst_sgn (const1)
      && tree_int_cst_sgn (cst) == tree_int_cst_sgn (const1)){
         fold_overflow_warning (warnmsg, WARN_STRICT_OVERFLOW_COMPARISON);
         return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,
               loc, code, type,mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, TREE_CODE (arg0),
                     TREE_TYPE (arg0), variable1, cst),variable2);
      }
   }

   tem = maybe_canonicalize_comparison(self,loc, code, type, arg0, arg1);
   if (tem)
      return tem;

   /* If we are comparing an expression that just has comparisons
   of two integer values, arithmetic expressions of those comparisons,
   and constants, we can simplify it.  There are only three cases
   to check: the two values can either be equal, the first can be
   greater, or the second can be greater.  Fold the expression for
   those three values.  Since each value must be 0 or 1, we have
   eight possibilities, each of which corresponds to the constant 0
   or 1 or one of the six possible comparisons.

   This handles common cases like (a > b) == 0 but also handles
   expressions like  ((x > y) - (y > x)) > 0, which supposedly
   occur in macroized code.  */

   if (TREE_CODE (arg1) == INTEGER_CST && TREE_CODE (arg0) != INTEGER_CST){
      tree cval1 = 0, cval2 = 0;

      if (twoval_comparison_p (arg0, &cval1, &cval2)
      /* Don't handle degenerate cases here; they should already
      have been handled anyway.  */
      && cval1 != 0 && cval2 != 0
      && ! (TREE_CONSTANT (cval1) && TREE_CONSTANT (cval2))
      && TREE_TYPE (cval1) == TREE_TYPE (cval2)
      && INTEGRAL_TYPE_P (TREE_TYPE (cval1))
      && TYPE_MAX_VALUE (TREE_TYPE (cval1))
      && TYPE_MAX_VALUE (TREE_TYPE (cval2))
      && ! operand_equal_p (TYPE_MIN_VALUE (TREE_TYPE (cval1)),
      TYPE_MAX_VALUE (TREE_TYPE (cval2)), 0)){
         tree maxval = TYPE_MAX_VALUE (TREE_TYPE (cval1));
         tree minval = TYPE_MIN_VALUE (TREE_TYPE (cval1));

         /* We can't just pass T to eval_subst in case cval1 or cval2
         was the same as ARG1.  */

         tree high_result = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,
               loc, code, type, eval_subst(self,loc, arg0, cval1, maxval, cval2, minval),arg1);
         tree equal_result = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,
               loc, code, type, eval_subst(self,loc, arg0, cval1, maxval,cval2, maxval),arg1);
         tree low_result = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,
               loc, code, type,eval_subst(self,loc, arg0, cval1, minval, cval2, maxval),arg1);

         /* All three of these results should be 0 or 1.  Confirm they are.
         Then use those values to select the proper code to use.  */

         if (TREE_CODE (high_result) == INTEGER_CST
         && TREE_CODE (equal_result) == INTEGER_CST
         && TREE_CODE (low_result) == INTEGER_CST){
            /* Make a 3-bit mask with the high-order bit being the
            value for `>', the next for '=', and the low for '<'.  */
            switch ((integer_onep (high_result) * 4)
            + (integer_onep (equal_result) * 2)
            + integer_onep (low_result)){
               case 0:
                  /* Always false.  */
                  return omit_one_operand_loc (loc, type, integer_zero_node, arg0);
               case 1:
                  code = LT_EXPR;
                  break;
               case 2:
                  code = EQ_EXPR;
                  break;
               case 3:
                  code = LE_EXPR;
                  break;
               case 4:
                  code = GT_EXPR;
                  break;
               case 5:
                  code = NE_EXPR;
                  break;
               case 6:
                  code = GE_EXPR;
                  break;
               case 7:
                  /* Always true.  */
                  return omit_one_operand_loc (loc, type, integer_one_node, arg0);
            }

            return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, code, type, cval1, cval2);
         }
      }
   }

   return NULL_TREE;
}

/* Fold a binary expression of code CODE and type TYPE with operands
   OP0 and OP1.  LOC is the location of the resulting expression.
   Return the folded expression if folding is successful.  Otherwise,
   return NULL_TREE.  */
//原型 fold_binary_loc fold-const.h fold-const.cc
tree mtcs_const_fold_binary_loc (MtcsConst *self,location_t loc, enum tree_code code, tree type,
       tree op0, tree op1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfa   *mtcsDfa=mtcs_target_get_dfa(mtcsTarget);
   MtcsTree  *mtcsTree =mtcs_target_get_tree(mtcsTarget);

   enum tree_code_class kind = TREE_CODE_CLASS (code);
   tree arg0, arg1, tem;
   tree t1 = NULL_TREE;
   bool strict_overflow_p;
   unsigned int prec;

   gcc_assert (IS_EXPR_CODE_CLASS (kind)
   && TREE_CODE_LENGTH (code) == 2
   && op0 != NULL_TREE
   && op1 != NULL_TREE);

   arg0 = op0;
   arg1 = op1;

   /* Strip any conversions that don't change the mode.  This is
   safe for every expression, except for a comparison expression
   because its signedness is derived from its operands.  So, in
   the latter case, only strip conversions that don't change the
   signedness.  MIN_EXPR/MAX_EXPR also need signedness of arguments
   preserved.

   Note that this is done as an internal manipulation within the
   constant folder, in order to find the simplest representation
   of the arguments so that their form can be studied.  In any
   cases, the appropriate type conversions should be put back in
   the tree that will get out of the constant folder.  */

   if (kind == tcc_comparison || code == MIN_EXPR || code == MAX_EXPR){
      STRIP_SIGN_NOPS (arg0);
      STRIP_SIGN_NOPS (arg1);
   }else{
      STRIP_NOPS (arg0);
      STRIP_NOPS (arg1);
   }

   /* Note that TREE_CONSTANT isn't enough: static var addresses are
   constant but we can't do arithmetic on them.  */
   if (CONSTANT_CLASS_P (arg0) && CONSTANT_CLASS_P (arg1)){
      tem =mtcs_const_const_binop/*!const_binop*/(self,code, type, arg0, arg1);
      if (tem != NULL_TREE){
         if (TREE_TYPE (tem) != type)
            tem = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, tem);
         return tem;
      }
   }

   /* If this is a commutative operation, and ARG0 is a constant, move it
   to ARG1 to reduce the number of tests below.  */
   if (commutative_tree_code (code) && tree_swap_operands_p (arg0, arg1))
      return mtcs_const_fold_build2_loc(self,loc, code, type, op1, op0);

   /* Likewise if this is a comparison, and ARG0 is a constant, move it
   to ARG1 to reduce the number of tests below.  */
   if (kind == tcc_comparison && tree_swap_operands_p (arg0, arg1))
      return mtcs_const_fold_build2_loc(self,loc, swap_tree_comparison (code), type, op1, op0);

   tem = generic_simplify (loc, code, type, op0, op1);
   if (tem)
      return tem;

   /* ARG0 is the first operand of EXPR, and ARG1 is the second operand.

   First check for cases where an arithmetic operation is applied to a
   compound, conditional, or comparison operation.  Push the arithmetic
   operation inside the compound or conditional to see if any folding
   can then be done.  Convert comparison to conditional for this purpose.
   The also optimizes non-constant cases that used to be done in
   expand_expr.

   Before we do that, see if this is a BIT_AND_EXPR or a BIT_IOR_EXPR,
   one of the operands is a comparison and the other is a comparison, a
   BIT_AND_EXPR with the constant 1, or a truth value.  In that case, the
   code below would make the expression more complex.  Change it to a
   TRUTH_{AND,OR}_EXPR.  Likewise, convert a similar NE_EXPR to
   TRUTH_XOR_EXPR and an EQ_EXPR to the inversion of a TRUTH_XOR_EXPR.  */

   if ((code == BIT_AND_EXPR || code == BIT_IOR_EXPR
   || code == EQ_EXPR || code == NE_EXPR)
   && !VECTOR_TYPE_P (TREE_TYPE (arg0))
   && ((truth_value_p (TREE_CODE (arg0))
   && (truth_value_p (TREE_CODE (arg1))
   || (TREE_CODE (arg1) == BIT_AND_EXPR
   && integer_onep (TREE_OPERAND (arg1, 1)))))
   || (truth_value_p (TREE_CODE (arg1))
   && (truth_value_p (TREE_CODE (arg0))
   || (TREE_CODE (arg0) == BIT_AND_EXPR
   && integer_onep (TREE_OPERAND (arg0, 1))))))){
      tem = mtcs_const_fold_build2_loc(self,loc, code == BIT_AND_EXPR ? TRUTH_AND_EXPR
               : code == BIT_IOR_EXPR ? TRUTH_OR_EXPR : TRUTH_XOR_EXPR,
               boolean_type_node,
               mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, boolean_type_node, arg0),
               mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, boolean_type_node, arg1));

      if (code == EQ_EXPR)
         tem = invert_truthvalue_loc (loc, tem);

      return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, tem);
   }

   if (TREE_CODE_CLASS (code) == tcc_binary  || TREE_CODE_CLASS (code) == tcc_comparison){
      if (TREE_CODE (arg0) == COMPOUND_EXPR){
         tem = mtcs_const_fold_build2_loc(self,loc, code, type,
               mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, TREE_TYPE (op0),TREE_OPERAND (arg0, 1)), op1);
         return build2_loc (loc, COMPOUND_EXPR, type, TREE_OPERAND (arg0, 0),tem);
      }
      if (TREE_CODE (arg1) == COMPOUND_EXPR){
         tem = mtcs_const_fold_build2_loc(self,loc, code, type, op0,
               mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, TREE_TYPE (op1),TREE_OPERAND (arg1, 1)));
         return build2_loc (loc, COMPOUND_EXPR, type, TREE_OPERAND (arg1, 0),tem);
      }

      if (TREE_CODE (arg0) == COND_EXPR  || TREE_CODE (arg0) == VEC_COND_EXPR || COMPARISON_CLASS_P (arg0)){
         tem = fold_binary_op_with_conditional_arg(self,loc, code, type, op0, op1,arg0, arg1,/*cond_first_p=*/1);
         if (tem != NULL_TREE)
            return tem;
      }

      if (TREE_CODE (arg1) == COND_EXPR || TREE_CODE (arg1) == VEC_COND_EXPR || COMPARISON_CLASS_P (arg1)){
         tem = fold_binary_op_with_conditional_arg(self,loc, code, type, op0, op1,arg1, arg0,/*cond_first_p=*/0);
         if (tem != NULL_TREE)
            return tem;
      }
   }

   switch (code){
      case MEM_REF:
         /* MEM[&MEM[p, CST1], CST2] -> MEM[p, CST1 + CST2].  */
         if (TREE_CODE (arg0) == ADDR_EXPR && TREE_CODE (TREE_OPERAND (arg0, 0)) == MEM_REF){
            tree iref = TREE_OPERAND (arg0, 0);
            return mtcs_const_fold_build2/*!fold_build2*/(self,MEM_REF, type, TREE_OPERAND (iref, 0),
                  mtcs_const_int_const_binop/*!int_const_binop*/(self,PLUS_EXPR, arg1,TREE_OPERAND (iref, 1)));
         }

         /* MEM[&a.b, CST2] -> MEM[&a, offsetof (a, b) + CST2].  */
         if (TREE_CODE (arg0) == ADDR_EXPR && handled_component_p (TREE_OPERAND (arg0, 0))){
            tree base;
            poly_int64 coffset;
            base = mtcs_dfa_get_addr_base_and_unit_offset/*!get_addr_base_and_unit_offset*/(mtcsDfa,TREE_OPERAND (arg0, 0),&coffset);
            if (!base)
               return NULL_TREE;
            return mtcs_const_fold_build2/*!fold_build2*/(self,MEM_REF, type,build1 (ADDR_EXPR, TREE_TYPE (arg0), base),
                  mtcs_const_int_const_binop/*!int_const_binop*/(self,PLUS_EXPR, arg1,size_int (coffset)));
         }

         return NULL_TREE;

      case POINTER_PLUS_EXPR:
         /* INT +p INT -> (PTR)(INT + INT).  Stripping types allows for this. */
         if (INTEGRAL_TYPE_P (TREE_TYPE (arg1)) && INTEGRAL_TYPE_P (TREE_TYPE (arg0)))
            return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type,
         mtcs_const_fold_build2_loc(self,loc, PLUS_EXPR, sizetype,
         mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, sizetype, arg1),
         mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, sizetype, arg0)));

         return NULL_TREE;

      case PLUS_EXPR:
         if (INTEGRAL_TYPE_P (type) || VECTOR_INTEGER_TYPE_P (type)){
            /* X + (X / CST) * -CST is X % CST.  */
            if (TREE_CODE (arg1) == MULT_EXPR
            && TREE_CODE (TREE_OPERAND (arg1, 0)) == TRUNC_DIV_EXPR
            && operand_equal_p (arg0, TREE_OPERAND (TREE_OPERAND (arg1, 0), 0), 0)){
               tree cst0 = TREE_OPERAND (TREE_OPERAND (arg1, 0), 1);
               tree cst1 = TREE_OPERAND (arg1, 1);
               tree sum = mtcs_const_fold_binary_loc/*!fold_binary_loc*/(self,loc, PLUS_EXPR, TREE_TYPE (cst1),cst1, cst0);
               if (sum && integer_zerop (sum))
                  return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type,
               mtcs_const_fold_build2_loc(self,loc, TRUNC_MOD_EXPR,TREE_TYPE (arg0), arg0,cst0));
            }
         }

         /* Handle (A1 * C1) + (A2 * C2) with A1, A2 or C1, C2 being the same or
         one.  Make sure the type is not saturating and has the signedness of
         the stripped operands, as fold_plusminus_mult_expr will re-associate.
         ??? The latter condition should use TYPE_OVERFLOW_* flags instead.  */
         if ((TREE_CODE (arg0) == MULT_EXPR
         || TREE_CODE (arg1) == MULT_EXPR)
         && !TYPE_SATURATING (type)
         && TYPE_UNSIGNED (type) == TYPE_UNSIGNED (TREE_TYPE (arg0))
         && TYPE_UNSIGNED (type) == TYPE_UNSIGNED (TREE_TYPE (arg1))
         && (!FLOAT_TYPE_P (type) || flag_associative_math)){
            tree tem = fold_plusminus_mult_expr(self,loc, code, type, arg0, arg1);
            if (tem)
               return tem;
         }

         if (! FLOAT_TYPE_P (type)){
            /* Reassociate (plus (plus (mult) (foo)) (mult)) as
            (plus (plus (mult) (mult)) (foo)) so that we can
            take advantage of the factoring cases below.  */
            if (ANY_INTEGRAL_TYPE_P (type)
            && TYPE_OVERFLOW_WRAPS (type)
            && (((TREE_CODE (arg0) == PLUS_EXPR
            || TREE_CODE (arg0) == MINUS_EXPR)
            && TREE_CODE (arg1) == MULT_EXPR)
            || ((TREE_CODE (arg1) == PLUS_EXPR
            || TREE_CODE (arg1) == MINUS_EXPR)
            && TREE_CODE (arg0) == MULT_EXPR))){
               tree parg0, parg1, parg, marg;
               enum tree_code pcode;

               if (TREE_CODE (arg1) == MULT_EXPR)
                  parg = arg0, marg = arg1;
               else
                  parg = arg1, marg = arg0;
               pcode = TREE_CODE (parg);
               parg0 = TREE_OPERAND (parg, 0);
               parg1 = TREE_OPERAND (parg, 1);
               STRIP_NOPS (parg0);
               STRIP_NOPS (parg1);

               if (TREE_CODE (parg0) == MULT_EXPR  && TREE_CODE (parg1) != MULT_EXPR)
                  return mtcs_const_fold_build2_loc(self,loc, pcode, type,
               mtcs_const_fold_build2_loc(self,loc, PLUS_EXPR, type,
                        mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type,parg0),
                        mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, marg)),
                        mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, parg1));
               if (TREE_CODE (parg0) != MULT_EXPR  && TREE_CODE (parg1) == MULT_EXPR)
                  return
               mtcs_const_fold_build2_loc(self,loc, PLUS_EXPR, type,
                     mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, parg0),
                     mtcs_const_fold_build2_loc(self,loc, pcode, type,
                     mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, marg),
                     mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, parg1)));
            }
         }else{
            /* Fold __complex__ ( x, 0 ) + __complex__ ( 0, y )
            to __complex__ ( x, y ).  This is not the same for SNaNs or
            if signed zeros are involved.  */
            if (!mtcs_mode_honor_snans/*!HONOR_SNANS*/(mtcsMode,arg0)  && !HONOR_SIGNED_ZEROS (arg0)  && COMPLEX_FLOAT_TYPE_P (TREE_TYPE (arg0))){
               tree rtype = TREE_TYPE (TREE_TYPE (arg0));
               tree arg0r = fold_unary_loc (loc, REALPART_EXPR, rtype, arg0);
               tree arg0i = fold_unary_loc (loc, IMAGPART_EXPR, rtype, arg0);
               bool arg0rz = false, arg0iz = false;
               if ((arg0r && (arg0rz = real_zerop (arg0r))) || (arg0i && (arg0iz = real_zerop (arg0i)))){
                  tree arg1r = fold_unary_loc (loc, REALPART_EXPR, rtype, arg1);
                  tree arg1i = fold_unary_loc (loc, IMAGPART_EXPR, rtype, arg1);
                  if (arg0rz && arg1i && real_zerop (arg1i)){
                     tree rp = arg1r ? arg1r : build1 (REALPART_EXPR, rtype, arg1);
                     tree ip = arg0i ? arg0i : build1 (IMAGPART_EXPR, rtype, arg0);
                     return mtcs_const_fold_build2_loc(self,loc, COMPLEX_EXPR, type, rp, ip);
                  }else if (arg0iz && arg1r && real_zerop (arg1r)){
                     tree rp = arg0r ? arg0r : build1 (REALPART_EXPR, rtype, arg0);
                     tree ip = arg1i ? arg1i : build1 (IMAGPART_EXPR, rtype, arg1);
                     return mtcs_const_fold_build2_loc(self,loc, COMPLEX_EXPR, type, rp, ip);
                  }
               }
            }

            /* Convert a + (b*c + d*e) into (a + b*c) + d*e.
            We associate floats only if the user has specified
            -fassociative-math.  */
            if (flag_associative_math  && TREE_CODE (arg1) == PLUS_EXPR  && TREE_CODE (arg0) != MULT_EXPR){
               tree tree10 = TREE_OPERAND (arg1, 0);
               tree tree11 = TREE_OPERAND (arg1, 1);
               if (TREE_CODE (tree11) == MULT_EXPR  && TREE_CODE (tree10) == MULT_EXPR){
                  tree tree0;
                  tree0 = mtcs_const_fold_build2_loc(self,loc, PLUS_EXPR, type, arg0, tree10);
                  return mtcs_const_fold_build2_loc(self,loc, PLUS_EXPR, type, tree0, tree11);
               }
            }
            /* Convert (b*c + d*e) + a into b*c + (d*e +a).
            We associate floats only if the user has specified
            -fassociative-math.  */
            if (flag_associative_math  && TREE_CODE (arg0) == PLUS_EXPR  && TREE_CODE (arg1) != MULT_EXPR){
               tree tree00 = TREE_OPERAND (arg0, 0);
               tree tree01 = TREE_OPERAND (arg0, 1);
               if (TREE_CODE (tree01) == MULT_EXPR && TREE_CODE (tree00) == MULT_EXPR){
                  tree tree0;
                  tree0 = mtcs_const_fold_build2_loc(self,loc, PLUS_EXPR, type, tree01, arg1);
                  return mtcs_const_fold_build2_loc(self,loc, PLUS_EXPR, type, tree00, tree0);
               }
            }
         }

   bit_rotate:
         /* (A << C1) + (A >> C2) if A is unsigned and C1+C2 is the size of A
         is a rotate of A by C1 bits.  */
         /* (A << B) + (A >> (Z - B)) if A is unsigned and Z is the size of A
         is a rotate of A by B bits.
         Similarly for (A << B) | (A >> (-B & C3)) where C3 is Z-1,
         though in this case CODE must be | and not + or ^, otherwise
         it doesn't return A when B is 0.  */
         {
            enum tree_code code0, code1;
            tree rtype;
            code0 = TREE_CODE (arg0);
            code1 = TREE_CODE (arg1);
            if (((code0 == RSHIFT_EXPR && code1 == LSHIFT_EXPR)
            || (code1 == RSHIFT_EXPR && code0 == LSHIFT_EXPR))
            && operand_equal_p (TREE_OPERAND (arg0, 0),TREE_OPERAND (arg1, 0), 0)
            && (rtype = TREE_TYPE (TREE_OPERAND (arg0, 0)),TYPE_UNSIGNED (rtype))
            /* Only create rotates in complete modes.  Other cases are not
            expanded properly.  */
            && (element_precision (rtype)  == GET_MODE_UNIT_PRECISION (TYPE_MODE (rtype)))){
               tree tree01, tree11;
               tree orig_tree01, orig_tree11;
               enum tree_code code01, code11;

               tree01 = orig_tree01 = TREE_OPERAND (arg0, 1);
               tree11 = orig_tree11 = TREE_OPERAND (arg1, 1);
               STRIP_NOPS (tree01);
               STRIP_NOPS (tree11);
               code01 = TREE_CODE (tree01);
               code11 = TREE_CODE (tree11);
               if (code11 != MINUS_EXPR  && (code01 == MINUS_EXPR || code01 == BIT_AND_EXPR)){
                  std::swap (code0, code1);
                  std::swap (code01, code11);
                  std::swap (tree01, tree11);
                  std::swap (orig_tree01, orig_tree11);
               }
               if (code01 == INTEGER_CST  && code11 == INTEGER_CST
                     && (wi::to_widest (tree01) + wi::to_widest (tree11) == element_precision (rtype))){
                  tem = build2_loc (loc, LROTATE_EXPR,rtype, TREE_OPERAND (arg0, 0),
                        code0 == LSHIFT_EXPR ? orig_tree01 : orig_tree11);
                  return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, tem);
               }else if (code11 == MINUS_EXPR){
                  tree tree110, tree111;
                  tree110 = TREE_OPERAND (tree11, 0);
                  tree111 = TREE_OPERAND (tree11, 1);
                  STRIP_NOPS (tree110);
                  STRIP_NOPS (tree111);
                  if (TREE_CODE (tree110) == INTEGER_CST  && compare_tree_int (tree110,element_precision (rtype)) == 0
                  && operand_equal_p (tree01, tree111, 0)){
                     tem = build2_loc (loc, (code0 == LSHIFT_EXPR ? LROTATE_EXPR : RROTATE_EXPR),
                     rtype, TREE_OPERAND (arg0, 0),orig_tree01);
                     return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, tem);
                  }
               }else if (code == BIT_IOR_EXPR  && code11 == BIT_AND_EXPR && pow2p_hwi (element_precision (rtype))){
                  tree tree110, tree111;
                  tree110 = TREE_OPERAND (tree11, 0);
                  tree111 = TREE_OPERAND (tree11, 1);
                  STRIP_NOPS (tree110);
                  STRIP_NOPS (tree111);
                  if (TREE_CODE (tree110) == NEGATE_EXPR
                  && TREE_CODE (tree111) == INTEGER_CST
                  && compare_tree_int (tree111, element_precision (rtype) - 1) == 0
                  && operand_equal_p (tree01, TREE_OPERAND (tree110, 0), 0)){
                     tem = build2_loc (loc, (code0 == LSHIFT_EXPR
                           ? LROTATE_EXPR : RROTATE_EXPR),rtype, TREE_OPERAND (arg0, 0),orig_tree01);
                     return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, tem);
                  }
               }
            }
         }

   associate:
         /* In most languages, can't associate operations on floats through
         parentheses.  Rather than remember where the parentheses were, we
         don't associate floats at all, unless the user has specified
         -fassociative-math.
         And, we need to make sure type is not saturating.  */
         if ((! FLOAT_TYPE_P (type) || flag_associative_math)
         && !TYPE_SATURATING (type) && !TYPE_OVERFLOW_SANITIZED (type)){
            tree var0, minus_var0, con0, minus_con0, lit0, minus_lit0;
            tree var1, minus_var1, con1, minus_con1, lit1, minus_lit1;
            tree atype = type;
            bool ok = true;

            /* Split both trees into variables, constants, and literals.  Then
            associate each group together, the constants with literals,
            then the result with variables.  This increases the chances of
            literals being recombined later and of generating relocatable
            expressions for the sum of a constant and literal.  */
            var0 = split_tree (arg0, type, code,&minus_var0, &con0, &minus_con0,&lit0, &minus_lit0, 0);
            var1 = split_tree (arg1, type, code,&minus_var1, &con1, &minus_con1,&lit1, &minus_lit1, code == MINUS_EXPR);

            /* Recombine MINUS_EXPR operands by using PLUS_EXPR.  */
            if (code == MINUS_EXPR)
               code = PLUS_EXPR;

            /* With undefined overflow prefer doing association in a type
            which wraps on overflow, if that is one of the operand types.  */
            if ((POINTER_TYPE_P (type) || INTEGRAL_TYPE_P (type))  && !TYPE_OVERFLOW_WRAPS (type)){
               if (INTEGRAL_TYPE_P (TREE_TYPE (arg0)) && TYPE_OVERFLOW_WRAPS (TREE_TYPE (arg0)))
                  atype = TREE_TYPE (arg0);
               else if (INTEGRAL_TYPE_P (TREE_TYPE (arg1)) && TYPE_OVERFLOW_WRAPS (TREE_TYPE (arg1)))
                  atype = TREE_TYPE (arg1);
               gcc_assert (TYPE_PRECISION (atype) == TYPE_PRECISION (type));
            }

            /* With undefined overflow we can only associate constants with one
            variable, and constants whose association doesn't overflow.  */
            if ((POINTER_TYPE_P (atype) || INTEGRAL_TYPE_P (atype)) && !TYPE_OVERFLOW_WRAPS (atype)){
               if ((var0 && var1) || (minus_var0 && minus_var1)){
                  /* ???  If split_tree would handle NEGATE_EXPR we could
                  simply reject these cases and the allowed cases would
                  be the var0/minus_var1 ones.  */
                  tree tmp0 = var0 ? var0 : minus_var0;
                  tree tmp1 = var1 ? var1 : minus_var1;
                  bool one_neg = false;

                  if (TREE_CODE (tmp0) == NEGATE_EXPR){
                     tmp0 = TREE_OPERAND (tmp0, 0);
                     one_neg = !one_neg;
                  }
                  if (CONVERT_EXPR_P (tmp0) && INTEGRAL_TYPE_P (TREE_TYPE (TREE_OPERAND (tmp0, 0)))
                  && (TYPE_PRECISION (TREE_TYPE (TREE_OPERAND (tmp0, 0))) <= TYPE_PRECISION (atype)))
                     tmp0 = TREE_OPERAND (tmp0, 0);
                  if (TREE_CODE (tmp1) == NEGATE_EXPR) {
                     tmp1 = TREE_OPERAND (tmp1, 0);
                     one_neg = !one_neg;
                  }
                  if (CONVERT_EXPR_P (tmp1) && INTEGRAL_TYPE_P (TREE_TYPE (TREE_OPERAND (tmp1, 0)))
                  && (TYPE_PRECISION (TREE_TYPE (TREE_OPERAND (tmp1, 0))) <= TYPE_PRECISION (atype)))
                     tmp1 = TREE_OPERAND (tmp1, 0);
                  /* The only case we can still associate with two variables
                  is if they cancel out.  */
                  if (!one_neg || !operand_equal_p (tmp0, tmp1, 0))
                     ok = false;
               }else if ((var0 && minus_var1 && ! operand_equal_p (var0, minus_var1, 0))
               || (minus_var0 && var1 && ! operand_equal_p (minus_var0, var1, 0)))
                  ok = false;
            }

            /* Only do something if we found more than two objects.  Otherwise,
            nothing has changed and we risk infinite recursion.  */
            if (ok
            && ((var0 != 0) + (var1 != 0)
            + (minus_var0 != 0) + (minus_var1 != 0)
            + (con0 != 0) + (con1 != 0)
            + (minus_con0 != 0) + (minus_con1 != 0)
            + (lit0 != 0) + (lit1 != 0)
            + (minus_lit0 != 0) + (minus_lit1 != 0)) > 2){
               int var0_origin = (var0 != 0) + 2 * (var1 != 0);
               int minus_var0_origin = (minus_var0 != 0) + 2 * (minus_var1 != 0);
               int con0_origin = (con0 != 0) + 2 * (con1 != 0);
               int minus_con0_origin = (minus_con0 != 0) + 2 * (minus_con1 != 0);
               int lit0_origin = (lit0 != 0) + 2 * (lit1 != 0);
               int minus_lit0_origin = (minus_lit0 != 0) + 2 * (minus_lit1 != 0);
               var0 = associate_trees(self,loc, var0, var1, code, atype);
               minus_var0 = associate_trees(self,loc, minus_var0, minus_var1,code, atype);
               con0 = associate_trees(self,loc, con0, con1, code, atype);
               minus_con0 = associate_trees(self,loc, minus_con0, minus_con1,code, atype);
               lit0 = associate_trees(self,loc, lit0, lit1, code, atype);
               minus_lit0 = associate_trees(self,loc, minus_lit0, minus_lit1,code, atype);

               if (minus_var0 && var0){
                  var0_origin |= minus_var0_origin;
                  var0 = associate_trees(self,loc, var0, minus_var0,MINUS_EXPR, atype);
                  minus_var0 = 0;
                  minus_var0_origin = 0;
               }
               if (minus_con0 && con0){
                  con0_origin |= minus_con0_origin;
                  con0 = associate_trees(self,loc, con0, minus_con0,MINUS_EXPR, atype);
                  minus_con0 = 0;
                  minus_con0_origin = 0;
               }

               /* Preserve the MINUS_EXPR if the negative part of the literal is
               greater than the positive part.  Otherwise, the multiplicative
               folding code (i.e extract_muldiv) may be fooled in case
               unsigned constants are subtracted, like in the following
               example: ((X*2 + 4) - 8U)/2.  */
               if (minus_lit0 && lit0){
                  if (TREE_CODE (lit0) == INTEGER_CST
                  && TREE_CODE (minus_lit0) == INTEGER_CST
                  && tree_int_cst_lt (lit0, minus_lit0)
                  /* But avoid ending up with only negated parts.  */
                  && (var0 || con0)){
                     minus_lit0_origin |= lit0_origin;
                     minus_lit0 = associate_trees(self,loc, minus_lit0, lit0, MINUS_EXPR, atype);
                     lit0 = 0;
                     lit0_origin = 0;
                  }else{
                     lit0_origin |= minus_lit0_origin;
                     lit0 = associate_trees(self,loc, lit0, minus_lit0,MINUS_EXPR, atype);
                     minus_lit0 = 0;
                     minus_lit0_origin = 0;
                  }
               }

               /* Don't introduce overflows through reassociation.  */
               if ((lit0 && TREE_OVERFLOW_P (lit0)) || (minus_lit0 && TREE_OVERFLOW_P (minus_lit0)))
                  return NULL_TREE;

               /* Eliminate lit0 and minus_lit0 to con0 and minus_con0. */
               con0_origin |= lit0_origin;
               con0 = associate_trees(self,loc, con0, lit0, code, atype);
               minus_con0_origin |= minus_lit0_origin;
               minus_con0 = associate_trees(self,loc, minus_con0, minus_lit0,code, atype);

               /* Eliminate minus_con0.  */
               if (minus_con0){
                  if (con0){
                     con0_origin |= minus_con0_origin;
                     con0 = associate_trees(self,loc, con0, minus_con0,MINUS_EXPR, atype);
                  }else if (var0){
                     var0_origin |= minus_con0_origin;
                     var0 = associate_trees(self,loc, var0, minus_con0,MINUS_EXPR, atype);
                  }else
                     gcc_unreachable ();
               }

               /* Eliminate minus_var0.  */
               if (minus_var0){
                  if (con0){
                     con0_origin |= minus_var0_origin;
                     con0 = associate_trees(self,loc, con0, minus_var0,MINUS_EXPR, atype);
                  }else
                     gcc_unreachable ();
               }

               /* Reassociate only if there has been any actual association
               between subtrees from op0 and subtrees from op1 in at
               least one of the operands, otherwise we risk infinite
               recursion.  See PR114084.  */
               if (var0_origin != 3 && con0_origin != 3)
                  return NULL_TREE;

               return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, associate_trees(self,loc, var0, con0,code, atype));
            }
         }

         return NULL_TREE;

      case POINTER_DIFF_EXPR:
      case MINUS_EXPR:
         /* Fold &a[i] - &a[j] to i-j.  */
         if (TREE_CODE (arg0) == ADDR_EXPR
         && TREE_CODE (TREE_OPERAND (arg0, 0)) == ARRAY_REF
         && TREE_CODE (arg1) == ADDR_EXPR
         && TREE_CODE (TREE_OPERAND (arg1, 0)) == ARRAY_REF){
            tree tem = fold_addr_of_array_ref_difference(self,loc, type,
            TREE_OPERAND (arg0, 0),
            TREE_OPERAND (arg1, 0),
            code == POINTER_DIFF_EXPR);
            if (tem)
               return tem;
         }

         /* Further transformations are not for pointers.  */
         if (code == POINTER_DIFF_EXPR)
            return NULL_TREE;

         /* (-A) - B -> (-B) - A  where B is easily negated and we can swap.  */
         if (TREE_CODE (arg0) == NEGATE_EXPR
         && negate_expr_p(self,op1)
         /* If arg0 is e.g. unsigned int and type is int, then this could
         introduce UB, because if A is INT_MIN at runtime, the original
         expression can be well defined while the latter is not.
         See PR83269.  */
         && !(ANY_INTEGRAL_TYPE_P (type)
         && TYPE_OVERFLOW_UNDEFINED (type)
         && ANY_INTEGRAL_TYPE_P (TREE_TYPE (arg0))
         && !TYPE_OVERFLOW_UNDEFINED (TREE_TYPE (arg0))))
            return mtcs_const_fold_build2_loc(self,loc, MINUS_EXPR, type, negate_expr(self,op1),
               mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, TREE_OPERAND (arg0, 0)));

         /* Fold __complex__ ( x, 0 ) - __complex__ ( 0, y ) to
         __complex__ ( x, -y ).  This is not the same for SNaNs or if
         signed zeros are involved.  */
         if (!mtcs_mode_honor_snans/*!HONOR_SNANS*/(mtcsMode,arg0)
         && !HONOR_SIGNED_ZEROS (arg0)
         && COMPLEX_FLOAT_TYPE_P (TREE_TYPE (arg0))){
            tree rtype = TREE_TYPE (TREE_TYPE (arg0));
            tree arg0r = fold_unary_loc (loc, REALPART_EXPR, rtype, arg0);
            tree arg0i = fold_unary_loc (loc, IMAGPART_EXPR, rtype, arg0);
            bool arg0rz = false, arg0iz = false;
            if ((arg0r && (arg0rz = real_zerop (arg0r))) || (arg0i && (arg0iz = real_zerop (arg0i)))){
               tree arg1r = fold_unary_loc (loc, REALPART_EXPR, rtype, arg1);
               tree arg1i = fold_unary_loc (loc, IMAGPART_EXPR, rtype, arg1);
               if (arg0rz && arg1i && real_zerop (arg1i)){
                  tree rp = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, NEGATE_EXPR, rtype,
                  arg1r ? arg1r : build1 (REALPART_EXPR, rtype, arg1));
                  tree ip = arg0i ? arg0i: build1 (IMAGPART_EXPR, rtype, arg0);
                  return mtcs_const_fold_build2_loc(self,loc, COMPLEX_EXPR, type, rp, ip);
               }else if (arg0iz && arg1r && real_zerop (arg1r)){
                  tree rp = arg0r ? arg0r : build1 (REALPART_EXPR, rtype, arg0);
                  tree ip = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, NEGATE_EXPR, rtype,
                  arg1i ? arg1i : build1 (IMAGPART_EXPR, rtype, arg1));
                  return mtcs_const_fold_build2_loc(self,loc, COMPLEX_EXPR, type, rp, ip);
               }
            }
         }

         /* A - B -> A + (-B) if B is easily negatable.  */
         if (negate_expr_p(self,op1) && ! TYPE_OVERFLOW_SANITIZED (type) && ((FLOAT_TYPE_P (type)
         /* Avoid this transformation if B is a positive REAL_CST.  */
         && (TREE_CODE (op1) != REAL_CST
         || REAL_VALUE_NEGATIVE (TREE_REAL_CST (op1))))
         || INTEGRAL_TYPE_P (type)))
            return mtcs_const_fold_build2_loc(self,loc, PLUS_EXPR, type,
                  mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, arg0),negate_expr(self,op1));

         /* Handle (A1 * C1) - (A2 * C2) with A1, A2 or C1, C2 being the same or
         one.  Make sure the type is not saturating and has the signedness of
         the stripped operands, as fold_plusminus_mult_expr will re-associate.
         ??? The latter condition should use TYPE_OVERFLOW_* flags instead.  */
         if ((TREE_CODE (arg0) == MULT_EXPR
         || TREE_CODE (arg1) == MULT_EXPR)
         && !TYPE_SATURATING (type)
         && TYPE_UNSIGNED (type) == TYPE_UNSIGNED (TREE_TYPE (arg0))
         && TYPE_UNSIGNED (type) == TYPE_UNSIGNED (TREE_TYPE (arg1))
         && (!FLOAT_TYPE_P (type) || flag_associative_math)){
            tree tem = fold_plusminus_mult_expr(self,loc, code, type, arg0, arg1);
            if (tem)
               return tem;
         }

         goto associate;

      case MULT_EXPR:
         if (! FLOAT_TYPE_P (type)){
            /* Transform x * -C into -x * C if x is easily negatable.  */
            if (TREE_CODE (op1) == INTEGER_CST
            && tree_int_cst_sgn (op1) == -1
            && negate_expr_p(self,op0)
            && negate_expr_p(self,op1)
            && (tem = negate_expr(self,op1)) != op1
            && ! TREE_OVERFLOW (tem))
               return mtcs_const_fold_build2_loc(self,loc, MULT_EXPR, type,
                        mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, negate_expr(self,op0)), tem);

            strict_overflow_p = false;
            if (TREE_CODE (arg1) == INTEGER_CST
                  && (tem = extract_muldiv(self,op0, arg1, code, NULL_TREE,  &strict_overflow_p)) != 0){
               if (strict_overflow_p)
                  fold_overflow_warning (("assuming signed overflow does not occur when simplifying multiplication"),
                        WARN_STRICT_OVERFLOW_MISC);
               return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, tem);
            }

            /* Optimize z * conj(z) for integer complex numbers.  */
            if (TREE_CODE (arg0) == CONJ_EXPR && operand_equal_p (TREE_OPERAND (arg0, 0), arg1, 0))
               return fold_mult_zconjz(self,loc, type, arg1);
            if (TREE_CODE (arg1) == CONJ_EXPR  && operand_equal_p (arg0, TREE_OPERAND (arg1, 0), 0))
               return fold_mult_zconjz(self,loc, type, arg0);
         }else{
            /* Fold z * +-I to __complex__ (-+__imag z, +-__real z).
            This is not the same for NaNs or if signed zeros are
            involved.  */
            if (!HONOR_NANS (arg0)
            && !HONOR_SIGNED_ZEROS (arg0)
            && COMPLEX_FLOAT_TYPE_P (TREE_TYPE (arg0))
            && TREE_CODE (arg1) == COMPLEX_CST
            && real_zerop (TREE_REALPART (arg1))){
               tree rtype = TREE_TYPE (TREE_TYPE (arg0));
               if (real_onep (TREE_IMAGPART (arg1))){
                  if (TREE_CODE (arg0) != COMPLEX_EXPR)
                     arg0 = save_expr (arg0);
                  tree iarg0 = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, IMAGPART_EXPR, rtype, arg0);
                  tree rarg0 = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, REALPART_EXPR, rtype, arg0);
                  return mtcs_const_fold_build2_loc(self,loc, COMPLEX_EXPR, type,negate_expr(self,iarg0),rarg0);
               }else if (real_minus_onep (TREE_IMAGPART (arg1))){
                  if (TREE_CODE (arg0) != COMPLEX_EXPR)
                     arg0 = save_expr (arg0);
                  tree iarg0 = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, IMAGPART_EXPR,rtype, arg0);
                  tree rarg0 = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, REALPART_EXPR,rtype, arg0);
                  return mtcs_const_fold_build2_loc(self,loc, COMPLEX_EXPR, type,iarg0,negate_expr(self,rarg0));
               }
            }

            /* Optimize z * conj(z) for floating point complex numbers.
            Guarded by flag_unsafe_math_optimizations as non-finite
            imaginary components don't produce scalar results.  */
            if (flag_unsafe_math_optimizations  && TREE_CODE (arg0) == CONJ_EXPR
            && operand_equal_p (TREE_OPERAND (arg0, 0), arg1, 0))
               return fold_mult_zconjz(self,loc, type, arg1);
            if (flag_unsafe_math_optimizations  && TREE_CODE (arg1) == CONJ_EXPR
            && operand_equal_p (arg0, TREE_OPERAND (arg1, 0), 0))
               return fold_mult_zconjz(self,loc, type, arg0);
         }
         goto associate;

      case BIT_IOR_EXPR:
         /* Canonicalize (X & C1) | C2.  */
         if (TREE_CODE (arg0) == BIT_AND_EXPR  && TREE_CODE (arg1) == INTEGER_CST
         && TREE_CODE (TREE_OPERAND (arg0, 1)) == INTEGER_CST){
            int width = TYPE_PRECISION (type), w;
            wide_int c1 = wi::to_wide (TREE_OPERAND (arg0, 1));
            wide_int c2 = wi::to_wide (arg1);

            /* If (C1&C2) == C1, then (X&C1)|C2 becomes (X,C2).  */
            if ((c1 & c2) == c1)
               return omit_one_operand_loc (loc, type, arg1,TREE_OPERAND (arg0, 0));

            wide_int msk = wi::mask (width, false,TYPE_PRECISION (TREE_TYPE (arg1)));

            /* If (C1|C2) == ~0 then (X&C1)|C2 becomes X|C2.  */
            if (wi::bit_and_not (msk, c1 | c2) == 0){
               tem = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, TREE_OPERAND (arg0, 0));
               return mtcs_const_fold_build2_loc(self,loc, BIT_IOR_EXPR, type, tem, arg1);
            }

            /* Minimize the number of bits set in C1, i.e. C1 := C1 & ~C2,
            unless (C1 & ~C2) | (C2 & C3) for some C3 is a mask of some
            mode which allows further optimizations.  */
            c1 &= msk;
            c2 &= msk;
            wide_int c3 = wi::bit_and_not (c1, c2);
            for (w = BITS_PER_UNIT; w <= width; w <<= 1){
               wide_int mask = wi::mask (w, false,TYPE_PRECISION (type));
               if (((c1 | c2) & mask) == mask && wi::bit_and_not (c1, mask) == 0){
                  c3 = mask;
                  break;
               }
            }

            if (c3 != c1){
               tem = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, TREE_OPERAND (arg0, 0));
               tem = mtcs_const_fold_build2_loc(self,
                     loc, BIT_AND_EXPR, type, tem,mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,type, c3));
               return mtcs_const_fold_build2_loc(self,loc, BIT_IOR_EXPR, type, tem, arg1);
            }
         }

         /* See if this can be simplified into a rotate first.  If that
         is unsuccessful continue in the association code.  */
         goto bit_rotate;

      case BIT_XOR_EXPR:
         /* Fold (X & 1) ^ 1 as (X & 1) == 0.  */
         if (TREE_CODE (arg0) == BIT_AND_EXPR
         && INTEGRAL_TYPE_P (type)
         && integer_onep (TREE_OPERAND (arg0, 1))
         && integer_onep (arg1))
         return mtcs_const_fold_build2_loc(self,loc, EQ_EXPR, type, arg0, build_zero_cst (TREE_TYPE (arg0)));

         /* See if this can be simplified into a rotate first.  If that
         is unsuccessful continue in the association code.  */
         goto bit_rotate;

      case BIT_AND_EXPR:
         /* Fold !X & 1 as X == 0.  */
         if (TREE_CODE (arg0) == TRUTH_NOT_EXPR  && integer_onep (arg1)){
            tem = TREE_OPERAND (arg0, 0);
            return mtcs_const_fold_build2_loc(self,loc, EQ_EXPR, type, tem,build_zero_cst (TREE_TYPE (tem)));
         }

         /* Fold (X * Y) & -(1 << CST) to X * Y if Y is a constant
         multiple of 1 << CST.  */
         if (TREE_CODE (arg1) == INTEGER_CST){
            wi::tree_to_wide_ref cst1 = wi::to_wide (arg1);
            wide_int ncst1 = -cst1;
            if ((cst1 & ncst1) == ncst1 && multiple_of_p (type, arg0,
                  mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,TREE_TYPE (arg1), ncst1)))
               return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, arg0);
         }

         /* Fold (X * CST1) & CST2 to zero if we can, or drop known zero
         bits from CST2.  */
         if (TREE_CODE (arg1) == INTEGER_CST  && TREE_CODE (arg0) == MULT_EXPR
         && TREE_CODE (TREE_OPERAND (arg0, 1)) == INTEGER_CST){
            wi::tree_to_wide_ref warg1 = wi::to_wide (arg1);
            wide_int masked = mask_with_tz (type, warg1, wi::to_wide (TREE_OPERAND (arg0, 1)));

            if (masked == 0)
               return omit_two_operands_loc (loc, type, build_zero_cst (type),arg0, arg1);
            else if (masked != warg1){
               /* Avoid the transform if arg1 is a mask of some
               mode which allows further optimizations.  */
               int pop = wi::popcount (warg1);
               if (!(pop >= BITS_PER_UNIT  && pow2p_hwi (pop) && wi::mask (pop, false, warg1.get_precision ()) == warg1))
                  return mtcs_const_fold_build2_loc(self,loc, code, type, op0,
                        mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,type, masked));
            }
         }

         /* Simplify ((int)c & 0377) into (int)c, if c is unsigned char.  */
         if (TREE_CODE (arg1) == INTEGER_CST && TREE_CODE (arg0) == NOP_EXPR
         && TYPE_UNSIGNED (TREE_TYPE (TREE_OPERAND (arg0, 0)))){
            prec = element_precision (TREE_TYPE (TREE_OPERAND (arg0, 0)));

            wide_int mask = wide_int::from (wi::to_wide (arg1), prec, UNSIGNED);
            if (mask == -1)
               return  mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, TREE_OPERAND (arg0, 0));
         }

         goto associate;

      case RDIV_EXPR:
         /* Don't touch a floating-point divide by zero unless the mode
         of the constant can represent infinity.  */
         if (TREE_CODE (arg1) == REAL_CST && !MODE_HAS_INFINITIES (TYPE_MODE (TREE_TYPE (arg1))) && real_zerop (arg1))
            return NULL_TREE;

         /* (-A) / (-B) -> A / B  */
         if (TREE_CODE (arg0) == NEGATE_EXPR && negate_expr_p(self,arg1))
            return mtcs_const_fold_build2_loc(self,loc, RDIV_EXPR, type,TREE_OPERAND (arg0, 0),negate_expr(self,arg1));
         if (TREE_CODE (arg1) == NEGATE_EXPR && negate_expr_p(self,arg0))
            return mtcs_const_fold_build2_loc(self,loc, RDIV_EXPR, type,negate_expr(self,arg0),TREE_OPERAND (arg1, 0));
         return NULL_TREE;

      case TRUNC_DIV_EXPR:
      /* Fall through */

      case FLOOR_DIV_EXPR:
         /* Simplify A / (B << N) where A and B are positive and B is
         a power of 2, to A >> (N + log2(B)).  */
         strict_overflow_p = false;
         if (TREE_CODE (arg1) == LSHIFT_EXPR && (TYPE_UNSIGNED (type)
               || tree_expr_nonnegative_warnv_p (op0, &strict_overflow_p))){
            tree sval = TREE_OPERAND (arg1, 0);
            if (integer_pow2p (sval) && tree_int_cst_sgn (sval) > 0){
               tree sh_cnt = TREE_OPERAND (arg1, 1);
               tree pow2 = mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (sh_cnt),wi::exact_log2 (wi::to_wide (sval)));

               if (strict_overflow_p)
                  fold_overflow_warning (("assuming signed overflow does not "
                  "occur when simplifying A / (B << N)"),   WARN_STRICT_OVERFLOW_MISC);

               sh_cnt = mtcs_const_fold_build2_loc(self,loc, PLUS_EXPR, TREE_TYPE (sh_cnt),sh_cnt, pow2);
               return mtcs_const_fold_build2_loc(self,loc, RSHIFT_EXPR, type,
                     mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, arg0), sh_cnt);
            }
         }

      /* Fall through */

      case ROUND_DIV_EXPR:
      case CEIL_DIV_EXPR:
      case EXACT_DIV_EXPR:
         if (integer_zerop (arg1))
            return NULL_TREE;

         /* Convert -A / -B to A / B when the type is signed and overflow is
         undefined.  */
         if ((!ANY_INTEGRAL_TYPE_P (type) || TYPE_OVERFLOW_UNDEFINED (type))
         && TREE_CODE (op0) == NEGATE_EXPR  && negate_expr_p(self,op1)){
            if (ANY_INTEGRAL_TYPE_P (type))
               fold_overflow_warning (("assuming signed overflow does not occur "
                        "when distributing negation across division"), WARN_STRICT_OVERFLOW_MISC);
            return mtcs_const_fold_build2_loc(self,loc, code, type,
                  mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type,
                  TREE_OPERAND (arg0, 0)), negate_expr(self,op1));
         }
         if ((!ANY_INTEGRAL_TYPE_P (type) || TYPE_OVERFLOW_UNDEFINED (type))
         && TREE_CODE (arg1) == NEGATE_EXPR  && negate_expr_p(self,op0)){
            if (ANY_INTEGRAL_TYPE_P (type))
               fold_overflow_warning (("assuming signed overflow does not occur "
                        "when distributing negation across division"),WARN_STRICT_OVERFLOW_MISC);
            return mtcs_const_fold_build2_loc(self,loc, code, type,negate_expr(self,op0),
                        mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type,TREE_OPERAND (arg1, 0)));
         }

         /* If arg0 is a multiple of arg1, then rewrite to the fastest div
         operation, EXACT_DIV_EXPR.

         Note that only CEIL_DIV_EXPR and FLOOR_DIV_EXPR are rewritten now.
         At one time others generated faster code, it's not clear if they do
         after the last round to changes to the DIV code in expmed.cc.  */
         if ((code == CEIL_DIV_EXPR || code == FLOOR_DIV_EXPR) && multiple_of_p (type, arg0, arg1))
            return mtcs_const_fold_build2_loc(self,loc, EXACT_DIV_EXPR, type,
                  mtcs_const_fold_convert/*!fold_convert*/(self,type, arg0),
                  mtcs_const_fold_convert/*!fold_convert*/(self,type, arg1));

         strict_overflow_p = false;
         if (TREE_CODE (arg1) == INTEGER_CST  && (tem = extract_muldiv(self,op0, arg1, code, NULL_TREE, &strict_overflow_p)) != 0){
            if (strict_overflow_p)
               fold_overflow_warning (("assuming signed overflow does not occur when simplifying division"),WARN_STRICT_OVERFLOW_MISC);
            return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, tem);
         }

         return NULL_TREE;

      case CEIL_MOD_EXPR:
      case FLOOR_MOD_EXPR:
      case ROUND_MOD_EXPR:
      case TRUNC_MOD_EXPR:
         strict_overflow_p = false;
         if (TREE_CODE (arg1) == INTEGER_CST
         && (tem = extract_muldiv(self,op0, arg1, code, NULL_TREE, &strict_overflow_p)) != 0){
            if (strict_overflow_p)
               fold_overflow_warning (("assuming signed overflow does not occur when simplifying modulus"),
                        WARN_STRICT_OVERFLOW_MISC);
            return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, tem);
         }

         return NULL_TREE;

      case LROTATE_EXPR:
      case RROTATE_EXPR:
      case RSHIFT_EXPR:
      case LSHIFT_EXPR:
         /* Since negative shift count is not well-defined,
         don't try to compute it in the compiler.  */
         if (TREE_CODE (arg1) == INTEGER_CST && tree_int_cst_sgn (arg1) < 0)
            return NULL_TREE;

         prec = element_precision (type);

         /* If we have a rotate of a bit operation with the rotate count and
         the second operand of the bit operation both constant,
         permute the two operations.  */
         if (code == RROTATE_EXPR && TREE_CODE (arg1) == INTEGER_CST
         && (TREE_CODE (arg0) == BIT_AND_EXPR
         || TREE_CODE (arg0) == BIT_IOR_EXPR
         || TREE_CODE (arg0) == BIT_XOR_EXPR)
         && TREE_CODE (TREE_OPERAND (arg0, 1)) == INTEGER_CST){
            tree arg00 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, TREE_OPERAND (arg0, 0));
            tree arg01 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, TREE_OPERAND (arg0, 1));
            return mtcs_const_fold_build2_loc(self,loc, TREE_CODE (arg0), type,
            mtcs_const_fold_build2_loc(self,loc, code, type,arg00, arg1),
            mtcs_const_fold_build2_loc(self,loc, code, type, arg01, arg1));
         }

         return NULL_TREE;

      case MIN_EXPR:
      case MAX_EXPR:
         goto associate;

      case TRUTH_ANDIF_EXPR:
         /* Note that the operands of this must be ints
         and their values must be 0 or 1.
         ("true" is a fixed value perhaps depending on the language.)  */
         /* If first arg is constant zero, return it.  */
         if (integer_zerop (arg0))
            return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, arg0);
      /* FALLTHRU */
      case TRUTH_AND_EXPR:
         /* If either arg is constant true, drop it.  */
         if (TREE_CODE (arg0) == INTEGER_CST && ! integer_zerop (arg0))
            return non_lvalue_loc (loc, mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, arg1));
         if (TREE_CODE (arg1) == INTEGER_CST && ! integer_zerop (arg1)
         /* Preserve sequence points.  */
         && (code != TRUTH_ANDIF_EXPR || ! TREE_SIDE_EFFECTS (arg0)))
            return non_lvalue_loc (loc, mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, arg0));
         /* If second arg is constant zero, result is zero, but first arg
         must be evaluated.  */
         if (integer_zerop (arg1))
            return omit_one_operand_loc (loc, type, arg1, arg0);
         /* Likewise for first arg, but note that only the TRUTH_AND_EXPR
         case will be handled here.  */
         if (integer_zerop (arg0))
            return omit_one_operand_loc (loc, type, arg0, arg1);

         /* !X && X is always false.  */
         if (TREE_CODE (arg0) == TRUTH_NOT_EXPR  && operand_equal_p (TREE_OPERAND (arg0, 0), arg1, 0))
            return omit_one_operand_loc (loc, type, integer_zero_node, arg1);
         /* X && !X is always false.  */
         if (TREE_CODE (arg1) == TRUTH_NOT_EXPR && operand_equal_p (arg0, TREE_OPERAND (arg1, 0), 0))
            return omit_one_operand_loc (loc, type, integer_zero_node, arg0);

         /* A < X && A + 1 > Y ==> A < X && A >= Y.  Normally A + 1 > Y
         means A >= Y && A != MAX, but in this case we know that
         A < X <= MAX.  */

         if (!TREE_SIDE_EFFECTS (arg0) && !TREE_SIDE_EFFECTS (arg1)){
            tem = fold_to_nonsharp_ineq_using_bound(self,loc, arg0, arg1);
            if (tem && !operand_equal_p (tem, arg0, 0))
               return mtcs_const_fold_convert/*!fold_convert*/(self,type,
                     mtcs_const_fold_build2_loc(self,loc, code, TREE_TYPE (arg1),tem, arg1));

            tem = fold_to_nonsharp_ineq_using_bound(self,loc, arg1, arg0);
            if (tem && !operand_equal_p (tem, arg1, 0))
               return mtcs_const_fold_convert/*!fold_convert*/(self,type,
                     mtcs_const_fold_build2_loc(self,loc, code, TREE_TYPE (arg0),arg0, tem));
         }

         if ((tem = fold_truth_andor(self,loc, code, type, arg0, arg1, op0, op1)) != NULL_TREE)
            return tem;

         return NULL_TREE;

      case TRUTH_ORIF_EXPR:
         /* Note that the operands of this must be ints
         and their values must be 0 or true.
         ("true" is a fixed value perhaps depending on the language.)  */
         /* If first arg is constant true, return it.  */
         if (TREE_CODE (arg0) == INTEGER_CST && ! integer_zerop (arg0))
            return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, arg0);
      /* FALLTHRU */
      case TRUTH_OR_EXPR:
         /* If either arg is constant zero, drop it.  */
         if (TREE_CODE (arg0) == INTEGER_CST && integer_zerop (arg0))
            return non_lvalue_loc (loc, mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, arg1));
         if (TREE_CODE (arg1) == INTEGER_CST && integer_zerop (arg1)
         /* Preserve sequence points.  */
         && (code != TRUTH_ORIF_EXPR || ! TREE_SIDE_EFFECTS (arg0)))
            return non_lvalue_loc (loc, mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, arg0));
         /* If second arg is constant true, result is true, but we must
         evaluate first arg.  */
         if (TREE_CODE (arg1) == INTEGER_CST && ! integer_zerop (arg1))
            return omit_one_operand_loc (loc, type, arg1, arg0);
         /* Likewise for first arg, but note this only occurs here for
         TRUTH_OR_EXPR.  */
         if (TREE_CODE (arg0) == INTEGER_CST && ! integer_zerop (arg0))
            return omit_one_operand_loc (loc, type, arg0, arg1);

         /* !X || X is always true.  */
         if (TREE_CODE (arg0) == TRUTH_NOT_EXPR  && operand_equal_p (TREE_OPERAND (arg0, 0), arg1, 0))
            return omit_one_operand_loc (loc, type, integer_one_node, arg1);
         /* X || !X is always true.  */
         if (TREE_CODE (arg1) == TRUTH_NOT_EXPR  && operand_equal_p (arg0, TREE_OPERAND (arg1, 0), 0))
            return omit_one_operand_loc (loc, type, integer_one_node, arg0);

         /* (X && !Y) || (!X && Y) is X ^ Y */
         if (TREE_CODE (arg0) == TRUTH_AND_EXPR && TREE_CODE (arg1) == TRUTH_AND_EXPR){
            tree a0, a1, l0, l1, n0, n1;

            a0 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, TREE_OPERAND (arg1, 0));
            a1 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, TREE_OPERAND (arg1, 1));

            l0 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, TREE_OPERAND (arg0, 0));
            l1 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, TREE_OPERAND (arg0, 1));

            n0 = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, TRUTH_NOT_EXPR, type, l0);
            n1 = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, TRUTH_NOT_EXPR, type, l1);

            if ((operand_equal_p (n0, a0, 0)
            && operand_equal_p (n1, a1, 0))
            || (operand_equal_p (n0, a1, 0)
            && operand_equal_p (n1, a0, 0)))
               return mtcs_const_fold_build2_loc(self,loc, TRUTH_XOR_EXPR, type, l0, n1);
         }

         if ((tem = fold_truth_andor(self,loc, code, type, arg0, arg1, op0, op1)) != NULL_TREE)
            return tem;

         return NULL_TREE;

      case TRUTH_XOR_EXPR:
         /* If the second arg is constant zero, drop it.  */
         if (integer_zerop (arg1))
            return non_lvalue_loc (loc, mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, arg0));
         /* If the second arg is constant true, this is a logical inversion.  */
         if (integer_onep (arg1)){
            tem = invert_truthvalue_loc (loc, arg0);
            return non_lvalue_loc (loc, mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, tem));
         }
         /* Identical arguments cancel to zero.  */
         if (operand_equal_p (arg0, arg1, 0))
            return omit_one_operand_loc (loc, type, integer_zero_node, arg0);

         /* !X ^ X is always true.  */
         if (TREE_CODE (arg0) == TRUTH_NOT_EXPR
         && operand_equal_p (TREE_OPERAND (arg0, 0), arg1, 0))
            return omit_one_operand_loc (loc, type, integer_one_node, arg1);

         /* X ^ !X is always true.  */
         if (TREE_CODE (arg1) == TRUTH_NOT_EXPR  && operand_equal_p (arg0, TREE_OPERAND (arg1, 0), 0))
            return omit_one_operand_loc (loc, type, integer_one_node, arg0);

         return NULL_TREE;

      case EQ_EXPR:
      case NE_EXPR:
         STRIP_NOPS (arg0);
         STRIP_NOPS (arg1);

         tem = fold_comparison(self,loc, code, type, op0, op1);
         if (tem != NULL_TREE)
            return tem;

         /* bool_var != 1 becomes !bool_var. */
         if (TREE_CODE (TREE_TYPE (arg0)) == BOOLEAN_TYPE && integer_onep (arg1) && code == NE_EXPR)
            return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type,
                     mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, TRUTH_NOT_EXPR,TREE_TYPE (arg0), arg0));

         /* bool_var == 0 becomes !bool_var. */
         if (TREE_CODE (TREE_TYPE (arg0)) == BOOLEAN_TYPE && integer_zerop (arg1)  && code == EQ_EXPR)
            return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type,
                     mtcs_const_fold_build1_loc/*!fold_build1_loc*/(self,loc, TRUTH_NOT_EXPR,TREE_TYPE (arg0), arg0));

         /* !exp != 0 becomes !exp */
         if (TREE_CODE (arg0) == TRUTH_NOT_EXPR && integer_zerop (arg1) && code == NE_EXPR)
            return non_lvalue_loc (loc, mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, arg0));

         /* If this is an EQ or NE comparison with zero and ARG0 is
         (1 << foo) & bar, convert it to (bar >> foo) & 1.  Both require
         two operations, but the latter can be done in one less insn
         on machines that have only two-operand insns or on which a
         constant cannot be the first operand.  */
         if (TREE_CODE (arg0) == BIT_AND_EXPR && integer_zerop (arg1)){
            tree arg00 = TREE_OPERAND (arg0, 0);
            tree arg01 = TREE_OPERAND (arg0, 1);
            if (TREE_CODE (arg00) == LSHIFT_EXPR && integer_onep (TREE_OPERAND (arg00, 0))){
               tree tem = mtcs_const_fold_build2_loc(self,loc, RSHIFT_EXPR, TREE_TYPE (arg00),
               arg01, TREE_OPERAND (arg00, 1));
               tem = mtcs_const_fold_build2_loc(self,loc, BIT_AND_EXPR, TREE_TYPE (arg0), tem,
                        build_one_cst (TREE_TYPE (arg0)));
               return mtcs_const_fold_build2_loc(self,loc, code, type,
                        mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, TREE_TYPE (arg1),tem), arg1);
            }else if (TREE_CODE (arg01) == LSHIFT_EXPR  && integer_onep (TREE_OPERAND (arg01, 0))){
               tree tem = mtcs_const_fold_build2_loc(self,loc, RSHIFT_EXPR, TREE_TYPE (arg01),arg00, TREE_OPERAND (arg01, 1));
               tem = mtcs_const_fold_build2_loc(self,loc, BIT_AND_EXPR, TREE_TYPE (arg0), tem,build_one_cst (TREE_TYPE (arg0)));
               return mtcs_const_fold_build2_loc(self,loc, code, type,
                        mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, TREE_TYPE (arg1),tem), arg1);
            }
         }

         /* If this is a comparison of a field, we may be able to simplify it.  */
         if ((TREE_CODE (arg0) == COMPONENT_REF
         || TREE_CODE (arg0) == BIT_FIELD_REF)
         /* Handle the constant case even without -O
         to make sure the warnings are given.  */
         && (optimize || TREE_CODE (arg1) == INTEGER_CST)){
            t1 = optimize_bit_field_compare(self,loc, code, type, arg0, arg1);
            if (t1)
               return t1;
         }

         /* Optimize comparisons of strlen vs zero to a compare of the
         first character of the string vs zero.  To wit,
         strlen(ptr) == 0   =>  *ptr == 0
         strlen(ptr) != 0   =>  *ptr != 0
         Other cases should reduce to one of these two (or a constant)
         due to the return value of strlen being unsigned.  */
         if (TREE_CODE (arg0) == CALL_EXPR && integer_zerop (arg1)){
            tree fndecl = get_callee_fndecl (arg0);

            if (fndecl && fndecl_built_in_p (fndecl, BUILT_IN_STRLEN)
            && call_expr_nargs (arg0) == 1 && (TREE_CODE (TREE_TYPE (CALL_EXPR_ARG (arg0, 0))) == POINTER_TYPE)){
               tree ptrtype =mtcs_tree_build_pointer_type/*!build_pointer_type*/(mtcsTree,
                     mtcs_tree_build_qualified_type/*!build_qualified_type*/(mtcsTree,char_type_node,TYPE_QUAL_CONST));
               tree ptr = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, ptrtype,CALL_EXPR_ARG (arg0, 0));
               tree iref = build_fold_indirect_ref_loc (loc, ptr);
               return mtcs_const_fold_build2_loc(self,loc, code, type, iref,
                     mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (iref), 0));
            }
         }

         /* Fold (X >> C) != 0 into X < 0 if C is one less than the width
         of X.  Similarly fold (X >> C) == 0 into X >= 0.  */
         if (TREE_CODE (arg0) == RSHIFT_EXPR && integer_zerop (arg1)
         && TREE_CODE (TREE_OPERAND (arg0, 1)) == INTEGER_CST){
            tree arg00 = TREE_OPERAND (arg0, 0);
            tree arg01 = TREE_OPERAND (arg0, 1);
            tree itype = TREE_TYPE (arg00);
            if (wi::to_wide (arg01) == element_precision (itype) - 1){
               if (TYPE_UNSIGNED (itype)){
                  itype = signed_type_for (itype);
                  arg00 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, itype, arg00);
               }
               return mtcs_const_fold_build2_loc(self,loc, code == EQ_EXPR ? GE_EXPR : LT_EXPR,type, arg00, build_zero_cst (itype));
            }
         }

         /* Fold (~X & C) == 0 into (X & C) != 0 and (~X & C) != 0 into
         (X & C) == 0 when C is a single bit.  */
         if (TREE_CODE (arg0) == BIT_AND_EXPR
         && TREE_CODE (TREE_OPERAND (arg0, 0)) == BIT_NOT_EXPR
         && integer_zerop (arg1)
         && integer_pow2p (TREE_OPERAND (arg0, 1))){
            tem = mtcs_const_fold_build2_loc(self,loc, BIT_AND_EXPR, TREE_TYPE (arg0),
                  TREE_OPERAND (TREE_OPERAND (arg0, 0), 0),TREE_OPERAND (arg0, 1));
            return mtcs_const_fold_build2_loc(self,loc, code == EQ_EXPR ? NE_EXPR : EQ_EXPR,
                     type, tem, mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, TREE_TYPE (arg0),arg1));
         }

         /* Fold ((X & C) ^ C) eq/ne 0 into (X & C) ne/eq 0, when the
         constant C is a power of two, i.e. a single bit.  */
         if (TREE_CODE (arg0) == BIT_XOR_EXPR
         && TREE_CODE (TREE_OPERAND (arg0, 0)) == BIT_AND_EXPR
         && integer_zerop (arg1)
         && integer_pow2p (TREE_OPERAND (arg0, 1))
         && operand_equal_p (TREE_OPERAND (TREE_OPERAND (arg0, 0), 1),
         TREE_OPERAND (arg0, 1), OEP_ONLY_CONST)){
            tree arg00 = TREE_OPERAND (arg0, 0);
            return mtcs_const_fold_build2_loc(self,loc, code == EQ_EXPR ? NE_EXPR : EQ_EXPR, type,
                     arg00, mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (arg00), 0));
         }

         /* Likewise, fold ((X ^ C) & C) eq/ne 0 into (X & C) ne/eq 0,
         when is C is a power of two, i.e. a single bit.  */
         if (TREE_CODE (arg0) == BIT_AND_EXPR
         && TREE_CODE (TREE_OPERAND (arg0, 0)) == BIT_XOR_EXPR
         && integer_zerop (arg1)
         && integer_pow2p (TREE_OPERAND (arg0, 1))
         && operand_equal_p (TREE_OPERAND (TREE_OPERAND (arg0, 0), 1),
         TREE_OPERAND (arg0, 1), OEP_ONLY_CONST)){
            tree arg000 = TREE_OPERAND (TREE_OPERAND (arg0, 0), 0);
            tem = mtcs_const_fold_build2_loc(self,loc, BIT_AND_EXPR, TREE_TYPE (arg000),arg000, TREE_OPERAND (arg0, 1));
            return mtcs_const_fold_build2_loc(self,loc, code == EQ_EXPR ? NE_EXPR : EQ_EXPR, type,
                  tem, mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (tem), 0));
         }

         if (TREE_CODE (arg0) == BIT_XOR_EXPR  && TREE_CODE (arg1) == BIT_XOR_EXPR){
            tree arg00 = TREE_OPERAND (arg0, 0);
            tree arg01 = TREE_OPERAND (arg0, 1);
            tree arg10 = TREE_OPERAND (arg1, 0);
            tree arg11 = TREE_OPERAND (arg1, 1);
            tree itype = TREE_TYPE (arg0);

            /* Optimize (X ^ Z) op (Y ^ Z) as X op Y, and symmetries.
            operand_equal_p guarantees no side-effects so we don't need
            to use omit_one_operand on Z.  */
            if (operand_equal_p (arg01, arg11, 0))
               return mtcs_const_fold_build2_loc(self,loc, code, type, arg00,
                     mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, TREE_TYPE (arg00),arg10));
            if (operand_equal_p (arg01, arg10, 0))
               return mtcs_const_fold_build2_loc(self,loc, code, type, arg00,
                     mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, TREE_TYPE (arg00),arg11));
            if (operand_equal_p (arg00, arg11, 0))
               return mtcs_const_fold_build2_loc(self,loc, code, type, arg01,
                     mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, TREE_TYPE (arg01),arg10));
            if (operand_equal_p (arg00, arg10, 0))
               return mtcs_const_fold_build2_loc(self,loc, code, type, arg01,
                  mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, TREE_TYPE (arg01),arg11));

            /* Optimize (X ^ C1) op (Y ^ C2) as (X ^ (C1 ^ C2)) op Y.  */
            if (TREE_CODE (arg01) == INTEGER_CST  && TREE_CODE (arg11) == INTEGER_CST){
               tem = mtcs_const_fold_build2_loc(self,loc, BIT_XOR_EXPR, itype, arg01,
                     mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, itype, arg11));
               tem = mtcs_const_fold_build2_loc(self,loc, BIT_XOR_EXPR, itype, arg00, tem);
               return mtcs_const_fold_build2_loc(self,loc, code, type, tem,
                     mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, itype, arg10));
            }
         }

         /* Attempt to simplify equality/inequality comparisons of complex
         values.  Only lower the comparison if the result is known or
         can be simplified to a single scalar comparison.  */
         if ((TREE_CODE (arg0) == COMPLEX_EXPR || TREE_CODE (arg0) == COMPLEX_CST)
         && (TREE_CODE (arg1) == COMPLEX_EXPR  || TREE_CODE (arg1) == COMPLEX_CST)){
            tree real0, imag0, real1, imag1;
            tree rcond, icond;

            if (TREE_CODE (arg0) == COMPLEX_EXPR){
               real0 = TREE_OPERAND (arg0, 0);
               imag0 = TREE_OPERAND (arg0, 1);
            }else{
               real0 = TREE_REALPART (arg0);
               imag0 = TREE_IMAGPART (arg0);
            }

            if (TREE_CODE (arg1) == COMPLEX_EXPR){
               real1 = TREE_OPERAND (arg1, 0);
               imag1 = TREE_OPERAND (arg1, 1);
            }else{
               real1 = TREE_REALPART (arg1);
               imag1 = TREE_IMAGPART (arg1);
            }

            rcond = mtcs_const_fold_binary_loc/*!fold_binary_loc*/(self,loc, code, type, real0, real1);
            if (rcond && TREE_CODE (rcond) == INTEGER_CST){
               if (integer_zerop (rcond)){
                  if (code == EQ_EXPR)
                     return omit_two_operands_loc (loc, type, boolean_false_node,imag0, imag1);
                  return mtcs_const_fold_build2_loc(self,loc, NE_EXPR, type, imag0, imag1);
               }else{
                  if (code == NE_EXPR)
                     return omit_two_operands_loc (loc, type, boolean_true_node,imag0, imag1);
                  return mtcs_const_fold_build2_loc(self,loc, EQ_EXPR, type, imag0, imag1);
               }
            }

            icond = mtcs_const_fold_binary_loc/*!fold_binary_loc*/(self,loc, code, type, imag0, imag1);
            if (icond && TREE_CODE (icond) == INTEGER_CST){
               if (integer_zerop (icond)){
                  if (code == EQ_EXPR)
                     return omit_two_operands_loc (loc, type, boolean_false_node,real0, real1);
                  return mtcs_const_fold_build2_loc(self,loc, NE_EXPR, type, real0, real1);
               }else{
                  if (code == NE_EXPR)
                     return omit_two_operands_loc (loc, type, boolean_true_node,real0, real1);
                  return mtcs_const_fold_build2_loc(self,loc, EQ_EXPR, type, real0, real1);
               }
            }
         }

         return NULL_TREE;

      case LT_EXPR:
      case GT_EXPR:
      case LE_EXPR:
      case GE_EXPR:
         tem = fold_comparison(self,loc, code, type, op0, op1);
         if (tem != NULL_TREE)
            return tem;

         /* Transform comparisons of the form X +- C CMP X.  */
         if ((TREE_CODE (arg0) == PLUS_EXPR || TREE_CODE (arg0) == MINUS_EXPR)
         && operand_equal_p (TREE_OPERAND (arg0, 0), arg1, 0)
         && TREE_CODE (TREE_OPERAND (arg0, 1)) == REAL_CST
         && !mtcs_mode_honor_snans/*!HONOR_SNANS*/(mtcsMode,arg0)){
            tree arg01 = TREE_OPERAND (arg0, 1);
            enum tree_code code0 = TREE_CODE (arg0);
            int is_positive = REAL_VALUE_NEGATIVE (TREE_REAL_CST (arg01)) ? -1 : 1;

            /* (X - c) > X becomes false.  */
            if (code == GT_EXPR
            && ((code0 == MINUS_EXPR && is_positive >= 0)
            || (code0 == PLUS_EXPR && is_positive <= 0)))
               return constant_boolean_node (0, type);

            /* Likewise (X + c) < X becomes false.  */
            if (code == LT_EXPR
            && ((code0 == PLUS_EXPR && is_positive >= 0)
            || (code0 == MINUS_EXPR && is_positive <= 0)))
               return constant_boolean_node (0, type);

            /* Convert (X - c) <= X to true.  */
            if (!HONOR_NANS (arg1)
            && code == LE_EXPR
            && ((code0 == MINUS_EXPR && is_positive >= 0)
            || (code0 == PLUS_EXPR && is_positive <= 0)))
               return constant_boolean_node (1, type);

            /* Convert (X + c) >= X to true.  */
            if (!HONOR_NANS (arg1)
            && code == GE_EXPR
            && ((code0 == PLUS_EXPR && is_positive >= 0)
            || (code0 == MINUS_EXPR && is_positive <= 0)))
               return constant_boolean_node (1, type);
         }

         /* If we are comparing an ABS_EXPR with a constant, we can
         convert all the cases into explicit comparisons, but they may
         well not be faster than doing the ABS and one comparison.
         But ABS (X) <= C is a range comparison, which becomes a subtraction
         and a comparison, and is probably faster.  */
         if (code == LE_EXPR
         && TREE_CODE (arg1) == INTEGER_CST
         && TREE_CODE (arg0) == ABS_EXPR
         && ! TREE_SIDE_EFFECTS (arg0)
         && (tem = negate_expr(self,arg1)) != 0
         && TREE_CODE (tem) == INTEGER_CST
         && !TREE_OVERFLOW (tem))
            return mtcs_const_fold_build2_loc(self,loc, TRUTH_ANDIF_EXPR, type,
         build2 (GE_EXPR, type,TREE_OPERAND (arg0, 0), tem),
         build2 (LE_EXPR, type,TREE_OPERAND (arg0, 0), arg1));

         /* Convert ABS_EXPR<x> >= 0 to true.  */
         strict_overflow_p = false;
         if (code == GE_EXPR && (integer_zerop (arg1) || (! HONOR_NANS (arg0)
         && real_zerop (arg1)))  && tree_expr_nonnegative_warnv_p (arg0, &strict_overflow_p)){
            if (strict_overflow_p)
               fold_overflow_warning (("assuming signed overflow does not occur "
                     "when simplifying comparison of absolute value and zero"),WARN_STRICT_OVERFLOW_CONDITIONAL);
            return omit_one_operand_loc (loc, type, constant_boolean_node (true, type),arg0);
         }

         /* Convert ABS_EXPR<x> < 0 to false.  */
         strict_overflow_p = false;
         if (code == LT_EXPR
         && (integer_zerop (arg1) || real_zerop (arg1))
         && tree_expr_nonnegative_warnv_p (arg0, &strict_overflow_p)){
            if (strict_overflow_p)
               fold_overflow_warning (("assuming signed overflow does not occur "
                  "when simplifying comparison of absolute value and zero"),WARN_STRICT_OVERFLOW_CONDITIONAL);
            return omit_one_operand_loc (loc, type, constant_boolean_node (false, type),arg0);
         }

         /* If X is unsigned, convert X < (1 << Y) into X >> Y == 0
         and similarly for >= into !=.  */
         if ((code == LT_EXPR || code == GE_EXPR)
         && TYPE_UNSIGNED (TREE_TYPE (arg0)) && TREE_CODE (arg1) == LSHIFT_EXPR
         && integer_onep (TREE_OPERAND (arg1, 0)))
            return build2_loc (loc, code == LT_EXPR ? EQ_EXPR : NE_EXPR, type,
               build2 (RSHIFT_EXPR, TREE_TYPE (arg0), arg0,
               TREE_OPERAND (arg1, 1)), build_zero_cst (TREE_TYPE (arg0)));

         /* Similarly for X < (cast) (1 << Y).  But cast can't be narrowing,
         otherwise Y might be >= # of bits in X's type and thus e.g.
         (unsigned char) (1 << Y) for Y 15 might be 0.
         If the cast is widening, then 1 << Y should have unsigned type,
         otherwise if Y is number of bits in the signed shift type minus 1,
         we can't optimize this.  E.g. (unsigned long long) (1 << Y) for Y
         31 might be 0xffffffff80000000.  */
         if ((code == LT_EXPR || code == GE_EXPR)
         && (INTEGRAL_TYPE_P (TREE_TYPE (arg0))
         || VECTOR_INTEGER_TYPE_P (TREE_TYPE (arg0)))
         && TYPE_UNSIGNED (TREE_TYPE (arg0))
         && CONVERT_EXPR_P (arg1)
         && TREE_CODE (TREE_OPERAND (arg1, 0)) == LSHIFT_EXPR
         && (element_precision (TREE_TYPE (arg1))
         >= element_precision (TREE_TYPE (TREE_OPERAND (arg1, 0))))
         && (TYPE_UNSIGNED (TREE_TYPE (TREE_OPERAND (arg1, 0)))
         || (element_precision (TREE_TYPE (arg1))
         == element_precision (TREE_TYPE (TREE_OPERAND (arg1, 0)))))
         && integer_onep (TREE_OPERAND (TREE_OPERAND (arg1, 0), 0))){
            tem = build2 (RSHIFT_EXPR, TREE_TYPE (arg0), arg0,
            TREE_OPERAND (TREE_OPERAND (arg1, 0), 1));
            return build2_loc (loc, code == LT_EXPR ? EQ_EXPR : NE_EXPR, type,
               mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, TREE_TYPE (arg0), tem),
               build_zero_cst (TREE_TYPE (arg0)));
         }

         return NULL_TREE;

      case UNORDERED_EXPR:
      case ORDERED_EXPR:
      case UNLT_EXPR:
      case UNLE_EXPR:
      case UNGT_EXPR:
      case UNGE_EXPR:
      case UNEQ_EXPR:
      case LTGT_EXPR:
         /* Fold (double)float1 CMP (double)float2 into float1 CMP float2.  */
         {
            tree targ0 = strip_float_extensions (arg0);
            tree targ1 = strip_float_extensions (arg1);
            tree newtype = TREE_TYPE (targ0);

            if (element_precision (TREE_TYPE (targ1)) > element_precision (newtype))
               newtype = TREE_TYPE (targ1);

            if (element_precision (newtype) < element_precision (TREE_TYPE (arg0))
            && (!VECTOR_TYPE_P (type) || is_truth_type_for (newtype, type)))
               return mtcs_const_fold_build2_loc(self,loc, code, type,
                  mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, newtype, targ0),
                  mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, newtype, targ1));
         }

         return NULL_TREE;

      case COMPOUND_EXPR:
         /* When pedantic, a compound expression can be neither an lvalue
         nor an integer constant expression.  */
         if (TREE_SIDE_EFFECTS (arg0) || TREE_CONSTANT (arg1))
            return NULL_TREE;
         /* Don't let (0, 0) be null pointer constant.  */
         tem = integer_zerop (arg1) ? build1_loc (loc, NOP_EXPR, type, arg1)
               : mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, type, arg1);
         return tem;

      default:
         return NULL_TREE;
   } /* switch (code) */
}


/* Return OFF converted to a pointer offset type suitable as offset for
   POINTER_PLUS_EXPR.  Use location LOC for this conversion.  */
//原型 convert_to_ptrofftype_loc fold-const.h fold-const.cc
tree mtcs_const_convert_to_ptrofftype_loc (MtcsConst *self,location_t loc, tree off)
{
  if (ptrofftype_p (TREE_TYPE (off)))
    return off;
  return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(self,loc, sizetype, off);
}


/* Build and fold a POINTER_PLUS_EXPR at LOC offsetting PTR by OFF.  */
//原型 fold_build_pointer_plus_loc fold-const.h fold-const.cc
tree mtcs_const_build_pointer_plus_loc (MtcsConst *self,location_t loc, tree ptr, tree off)
{
  return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(self,loc, POINTER_PLUS_EXPR, TREE_TYPE (ptr),
           ptr, mtcs_const_convert_to_ptrofftype_loc/*!convert_to_ptrofftype_loc*/(self,loc, off));
}


/* Given the components of a binary expression CODE, TYPE, OP0 and OP1,
   attempt to fold the expression to a constant without modifying TYPE,
   OP0 or OP1.

   If the expression could be simplified to a constant, then return
   the constant.  If the expression would not be simplified to a
   constant, then return NULL_TREE.  */
//原型 fold_binary_to_constant fold-const.h fold-const.cc
tree mtcs_const_fold_binary_to_constant (MtcsConst *self,enum tree_code code, tree type, tree op0, tree op1)
{
  tree tem = mtcs_const_fold_binary/*!fold_binary*/(self,code, type, op0, op1);
  return (tem && TREE_CONSTANT (tem)) ? tem : NULL_TREE;
}


/* Fold a CALL_EXPR expression of type TYPE with operands FN and NARGS
   arguments in ARGARRAY, and a null static chain.
   Return a folded expression if successful.  Otherwise, return a CALL_EXPR
   of type TYPE from the given operands as constructed by build_call_array.  */
//原型 fold_build_call_array_loc fold-const.h fold-const.cc
tree mtcs_const_fold_build_call_array_loc (MtcsConst *self,location_t loc, tree type, tree fn,
            int nargs, tree *argarray)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsBuiltins *mtcsBuiltins=mtcs_target_get_builtins(mtcsTarget);

   tree tem;
#ifdef ENABLE_FOLD_CHECKING
   unsigned char checksum_before_fn[16],
   checksum_before_arglist[16],
   checksum_after_fn[16],
   checksum_after_arglist[16];
   struct md5_ctx ctx;
   hash_table<nofree_ptr_hash<const tree_node> > ht (32);
   int i;

   md5_init_ctx (&ctx);
   fold_checksum_tree (fn, &ctx, &ht);
   md5_finish_ctx (&ctx, checksum_before_fn);
   ht.empty ();

   md5_init_ctx (&ctx);
   for (i = 0; i < nargs; i++)
   fold_checksum_tree (argarray[i], &ctx, &ht);
   md5_finish_ctx (&ctx, checksum_before_arglist);
   ht.empty ();
#endif

   tem = mtcs_builtins_fold_builtin_call_array/*!fold_builtin_call_array*/(mtcsBuiltins,loc, type, fn, nargs, argarray);
   if (!tem)
      tem = build_call_array_loc (loc, type, fn, nargs, argarray);

#ifdef ENABLE_FOLD_CHECKING
   md5_init_ctx (&ctx);
   fold_checksum_tree (fn, &ctx, &ht);
   md5_finish_ctx (&ctx, checksum_after_fn);
   ht.empty ();

   if (memcmp (checksum_before_fn, checksum_after_fn, 16))
      fold_check_failed (fn, tem);

   md5_init_ctx (&ctx);
   for (i = 0; i < nargs; i++)
      fold_checksum_tree (argarray[i], &ctx, &ht);
   md5_finish_ctx (&ctx, checksum_after_arglist);

   if (memcmp (checksum_before_arglist, checksum_after_arglist, 16))
      fold_check_failed (NULL_TREE, tem);
#endif
   return tem;
}


MtcsConst *mtcs_const_new(MtcsMode *mtcsMode)
{
   MtcsConst *self = n_slice_alloc0 (sizeof(MtcsConst));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsConstInit(self);
   return self;
}


