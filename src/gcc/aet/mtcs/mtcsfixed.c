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

#include "aet/aetprinttree.h"
#include "mtcsfixed.h"
#include "mtcstarget.h"


/* Define the enum code for the range of the fixed-point value.  */
enum fixed_value_range_code {
  FIXED_OK,    /* The value is within the range.  */
  FIXED_UNDERFLOW,   /* The value is less than the minimum.  */
  FIXED_GT_MAX_EPS,  /* The value is greater than the maximum, but not equal
            to the maximum plus the epsilon.  */
  FIXED_MAX_EPS      /* The value equals the maximum plus the epsilon.  */
};


static void mtcsFixedInit(MtcsFixed *self)
{

}

/* Check REAL_VALUE against the range of the fixed-point mode.
   Return FIXED_OK, if it is within the range.
          FIXED_UNDERFLOW, if it is less than the minimum.
          FIXED_GT_MAX_EPS, if it is greater than the maximum, but not equal to
       the maximum plus the epsilon.
          FIXED_MAX_EPS, if it is equal to the maximum plus the epsilon.  */
//原型 check_real_for_fixed_mode  fixed-value.cc

static enum fixed_value_range_code check_real_for_fixed_mode (MtcsFixed *self,
      REAL_VALUE_TYPE *real_value, machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   REAL_VALUE_TYPE max_value, min_value, epsilon_value;

   real_2expN (&max_value, mtcs_mode_get_ibit/*!GET_MODE_IBIT*/(mtcsMode,mode), VOIDmode);
   real_2expN (&epsilon_value, -mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,mode), VOIDmode);

   if (mtcs_mode_is_signed_fixed_point_p/*!SIGNED_FIXED_POINT_MODE_P*/(mtcsMode,mode))
      min_value = real_value_negate (&max_value);
   else
      real_from_string (&min_value, "0.0");

   if (real_compare (LT_EXPR, real_value, &min_value))
      return FIXED_UNDERFLOW;
   if (real_compare (EQ_EXPR, real_value, &max_value))
      return FIXED_MAX_EPS;
   real_arithmetic (&max_value, MINUS_EXPR, &max_value, &epsilon_value);
   if (real_compare (GT_EXPR, real_value, &max_value))
      return FIXED_GT_MAX_EPS;
   return FIXED_OK;
}

/* Convert to a new fixed-point mode from a real.
   If SAT_P, saturate the result to the max or the min.
   Return true, if !SAT_P and overflow.  */
//原型 fixed_convert_from_real fixed-value.h fixed-value.cc
bool mtcs_fixed_fixed_convert_from_real (MtcsFixed *self,FIXED_VALUE_TYPE *f, scalar_mode mode,
          const REAL_VALUE_TYPE *a, bool sat_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   bool overflow_p = false;
   REAL_VALUE_TYPE real_value, fixed_value, base_value;
   bool unsigned_p = mtcs_mode_is_unsigned_fixed_point_p/*!UNSIGNED_FIXED_POINT_MODE_P*/(mtcsMode,mode);
   int i_f_bits = mtcs_mode_get_ibit/*!GET_MODE_IBIT*/(mtcsMode,mode) + mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,mode);
   unsigned int fbit = mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,mode);
   enum fixed_value_range_code temp;
   bool fail;

   real_value = *a;
   f->mode = mode;
   real_2expN (&base_value, fbit, VOIDmode);
   real_arithmetic (&fixed_value, MULT_EXPR, &real_value, &base_value);

   wide_int w = real_to_integer (&fixed_value, &fail,
   mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode));
   f->data.low = w.ulow ();
   f->data.high = w.elt (1);
   temp = check_real_for_fixed_mode(self,&real_value, mode);
   if (temp == FIXED_UNDERFLOW) /* Minimum.  */{
      if (sat_p){
         if (unsigned_p){
            f->data.low = 0;
            f->data.high = 0;
         }else{
            f->data.low = 1;
            f->data.high = 0;
            f->data = f->data.alshift (i_f_bits, HOST_BITS_PER_DOUBLE_INT);
            f->data = f->data.sext (1 + i_f_bits);
         }
      }else
         overflow_p = true;
   }else if (temp == FIXED_GT_MAX_EPS || temp == FIXED_MAX_EPS) /* Maximum.  */{
      if (sat_p){
         f->data.low = -1;
         f->data.high = -1;
         f->data = f->data.zext (i_f_bits);
      }else
         overflow_p = true;
   }
   f->data = f->data.ext ((!unsigned_p) + i_f_bits, unsigned_p);
   return overflow_p;
}

/* If SAT_P, saturate A to the maximum or the minimum, and save to *F based on
   the machine mode MODE.
   Do not modify *F otherwise.
   This function assumes the width of double_int is greater than the width
   of the fixed-point value (the sum of a possible sign bit, possible ibits,
   and fbits).
   Return true, if !SAT_P and overflow.  */
//原型 fixed_saturate1  fixed-value.cc
static bool fixed_saturate1 (MtcsFixed *self,machine_mode mode, double_int a, double_int *f,
       bool sat_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   bool overflow_p = false;
   bool unsigned_p = mtcs_mode_is_unsigned_fixed_point_p/*!UNSIGNED_FIXED_POINT_MODE_P*/(mtcsMode,mode);
   int i_f_bits = mtcs_mode_get_ibit/*!GET_MODE_IBIT*/(mtcsMode,mode) + mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,mode);

   if (unsigned_p) /* Unsigned type.  */{
      double_int max;
      max.low = -1;
      max.high = -1;
      max = max.zext (i_f_bits);
      if (a.ugt (max)){
         if (sat_p)
            *f = max;
         else
            overflow_p = true;
      }
   }else /* Signed type.  */{
      double_int max, min;
      max.high = -1;
      max.low = -1;
      max = max.zext (i_f_bits);
      min.high = 0;
      min.low = 1;
      min = min.alshift (i_f_bits, HOST_BITS_PER_DOUBLE_INT);
      min = min.sext (1 + i_f_bits);
      if (a.sgt (max)){
         if (sat_p)
            *f = max;
         else
            overflow_p = true;
      }else if (a.slt (min)){
         if (sat_p)
            *f = min;
         else
            overflow_p = true;
      }
   }
   return overflow_p;
}


/* If SAT_P, saturate {A_HIGH, A_LOW} to the maximum or the minimum, and
   save to *F based on the machine mode MODE.
   Do not modify *F otherwise.
   This function assumes the width of two double_int is greater than the width
   of the fixed-point value (the sum of a possible sign bit, possible ibits,
   and fbits).
   Return true, if !SAT_P and overflow.  */
//原型 fixed_saturate2  fixed-value.cc
static bool fixed_saturate2 (MtcsFixed *self,machine_mode mode, double_int a_high, double_int a_low,
       double_int *f, bool sat_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   bool overflow_p = false;
   bool unsigned_p = mtcs_mode_is_unsigned_fixed_point_p/*!UNSIGNED_FIXED_POINT_MODE_P*/(mtcsMode,mode);
   int i_f_bits =mtcs_mode_get_ibit/*!GET_MODE_IBIT*/(mtcsMode,mode) + mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,mode);

   if (unsigned_p) /* Unsigned type.  */{
      double_int max_r, max_s;
      max_r.high = 0;
      max_r.low = 0;
      max_s.high = -1;
      max_s.low = -1;
      max_s = max_s.zext (i_f_bits);
      if (a_high.ugt (max_r) || (a_high == max_r && a_low.ugt (max_s))){
         if (sat_p)
            *f = max_s;
         else
            overflow_p = true;
      }
   }else /* Signed type.  */{
      double_int max_r, max_s, min_r, min_s;
      max_r.high = 0;
      max_r.low = 0;
      max_s.high = -1;
      max_s.low = -1;
      max_s = max_s.zext (i_f_bits);
      min_r.high = -1;
      min_r.low = -1;
      min_s.high = 0;
      min_s.low = 1;
      min_s = min_s.alshift (i_f_bits, HOST_BITS_PER_DOUBLE_INT);
      min_s = min_s.sext (1 + i_f_bits);
      if (a_high.sgt (max_r) || (a_high == max_r && a_low.ugt (max_s))){
         if (sat_p)
            *f = max_s;
         else
            overflow_p = true;
      }else if (a_high.slt (min_r) || (a_high == min_r && a_low.ult (min_s))){
         if (sat_p)
            *f = min_s;
         else
            overflow_p = true;
      }
   }
   return overflow_p;
}

/* Convert to a new fixed-point mode from an integer.
   If UNSIGNED_P, this integer is unsigned.
   If SAT_P, saturate the result to the max or the min.
   Return true, if !SAT_P and overflow.  */
//原型 fixed_convert_from_int fixed-value.h fixed-value.cc
bool mtcs_fixed_fixed_convert_from_int (MtcsFixed *self,FIXED_VALUE_TYPE *f, scalar_mode mode,
         double_int a, bool unsigned_p, bool sat_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   bool overflow_p = false;
   /* Left shift a to temp_high, temp_low.  */
   double_int temp_high, temp_low;
   int amount = mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,mode);
   if (amount == HOST_BITS_PER_DOUBLE_INT){
      temp_high = a;
      temp_low.low = 0;
      temp_low.high = 0;
   }else{
      temp_low = a.llshift (amount, HOST_BITS_PER_DOUBLE_INT);

      /* Logical shift right to temp_high.  */
      temp_high = a.llshift (amount - HOST_BITS_PER_DOUBLE_INT,HOST_BITS_PER_DOUBLE_INT);
   }
   if (!unsigned_p && a.high < 0) /* Signed-extend temp_high.  */
      temp_high = temp_high.sext (amount);

   f->mode = mode;
   f->data = temp_low;

   if (unsigned_p ==  mtcs_mode_is_unsigned_fixed_point_p/*!UNSIGNED_FIXED_POINT_MODE_P*/(mtcsMode,f->mode))
      overflow_p = fixed_saturate2(self,f->mode, temp_high, temp_low, &f->data,sat_p);
   else{
      /* Take care of the cases when converting between signed and unsigned.  */
      if (!unsigned_p){
         /* Signed -> Unsigned.  */
         if (a.high < 0){
            if (sat_p){
               f->data.low = 0;  /* Set to zero.  */
               f->data.high = 0;  /* Set to zero.  */
            }else
               overflow_p = true;
         }else
            overflow_p = fixed_saturate2(self,f->mode, temp_high, temp_low,&f->data, sat_p);
      }else{
         /* Unsigned -> Signed.  */
         if (temp_high.high < 0){
            if (sat_p){
               /* Set to maximum.  */
               f->data.low = -1;  /* Set to all ones.  */
               f->data.high = -1;  /* Set to all ones.  */
               f->data = f->data.zext (mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,f->mode)
                     + mtcs_mode_get_ibit/*!GET_MODE_IBIT*/(mtcsMode,f->mode));
               /* Clear the sign.  */
            }else
               overflow_p = true;
         }else
            overflow_p = fixed_saturate2(self,f->mode, temp_high, temp_low,&f->data, sat_p);
      }
   }
   f->data = f->data.ext (mtcs_mode_is_signed_fixed_point_p/*!SIGNED_FIXED_POINT_MODE_P*/(mtcsMode,f->mode)
      + mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,f->mode)
      + mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,f->mode),
      mtcs_mode_is_unsigned_fixed_point_p/*!UNSIGNED_FIXED_POINT_MODE_P*/(mtcsMode,f->mode));
   return overflow_p;
}

/* Convert to a new real mode from a fixed-point.  */
//原型 real_convert_from_fixed fixed-value.h fixed-value.cc
void mtcs_fixed_real_convert_from_fixed (MtcsFixed *self,REAL_VALUE_TYPE *r, scalar_mode mode,
          const FIXED_VALUE_TYPE *f)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);

   REAL_VALUE_TYPE base_value, fixed_value, real_value;

   signop sgn = mtcs_mode_is_unsigned_fixed_point_p/*!UNSIGNED_FIXED_POINT_MODE_P*/(mtcsMode,f->mode) ? UNSIGNED : SIGNED;
   real_2expN (&base_value, mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,f->mode), VOIDmode);
   mtcs_real_real_from_integer/*!real_from_integer*/(mtcsReal,&fixed_value, VOIDmode,
         wide_int::from (f->data, mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,f->mode),sgn), sgn);
   real_arithmetic (&real_value, RDIV_EXPR, &fixed_value, &base_value);
   mtcs_real_real_convert/*!real_convert*/(mtcsReal,r, mode, &real_value);
}

/* Extend or truncate to a new mode.
   If SAT_P, saturate the result to the max or the min.
   Return true, if !SAT_P and overflow.  */
//原型 fixed_convert fixed-value.h fixed-value.cc
bool mtcs_fixed_fixed_convert (MtcsFixed *self,FIXED_VALUE_TYPE *f, scalar_mode mode,
               const FIXED_VALUE_TYPE *a, bool sat_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   bool overflow_p = false;
   if (mode == a->mode){
      *f = *a;
      return overflow_p;
   }

   if (mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,mode) > mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,a->mode)){
      /* Left shift a to temp_high, temp_low based on a->mode.  */
      double_int temp_high, temp_low;
      int amount = mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,mode) - mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,a->mode);
      temp_low = a->data.lshift (amount,HOST_BITS_PER_DOUBLE_INT,
            mtcs_mode_is_signed_fixed_point_p/*!SIGNED_FIXED_POINT_MODE_P*/(mtcsMode,a->mode));
      /* Logical shift right to temp_high.  */
      temp_high = a->data.llshift (amount - HOST_BITS_PER_DOUBLE_INT, HOST_BITS_PER_DOUBLE_INT);
      if (mtcs_mode_is_signed_fixed_point_p/*!SIGNED_FIXED_POINT_MODE_P*/(mtcsMode,a->mode)
            && a->data.high < 0) /* Signed-extend temp_high.  */
         temp_high = temp_high.sext (amount);
      f->mode = mode;
      f->data = temp_low;
      if (mtcs_mode_is_signed_fixed_point_p/*!SIGNED_FIXED_POINT_MODE_P*/(mtcsMode,a->mode) ==
            mtcs_mode_is_signed_fixed_point_p/*!SIGNED_FIXED_POINT_MODE_P*/(mtcsMode,f->mode))
         overflow_p = fixed_saturate2(self,f->mode, temp_high, temp_low, &f->data, sat_p);
      else{
         /* Take care of the cases when converting between signed and
         unsigned.  */
         if (mtcs_mode_is_signed_fixed_point_p/*!SIGNED_FIXED_POINT_MODE_P*/(mtcsMode,a->mode)){
            /* Signed -> Unsigned.  */
            if (a->data.high < 0){
               if (sat_p){
                  f->data.low = 0;  /* Set to zero.  */
                  f->data.high = 0;  /* Set to zero.  */
               }else
                  overflow_p = true;
            }else
               overflow_p = fixed_saturate2(self,f->mode, temp_high, temp_low,&f->data, sat_p);
         }else{
            /* Unsigned -> Signed.  */
            if (temp_high.high < 0){
               if (sat_p){
                  /* Set to maximum.  */
                  f->data.low = -1;  /* Set to all ones.  */
                  f->data.high = -1;  /* Set to all ones.  */
                  f->data = f->data.zext (mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,f->mode)
                        +mtcs_mode_get_ibit/*!GET_MODE_IBIT*/(mtcsMode,f->mode));
                  /* Clear the sign.  */
               }else
                  overflow_p = true;
            }else
               overflow_p = fixed_saturate2(self,f->mode, temp_high, temp_low,&f->data, sat_p);
         }
      }
   }else{
      /* Right shift a to temp based on a->mode.  */
      double_int temp;
      temp = a->data.lshift (mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,mode) - mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,a->mode),
      HOST_BITS_PER_DOUBLE_INT,
      mtcs_mode_is_signed_fixed_point_p/*!SIGNED_FIXED_POINT_MODE_P*/(mtcsMode,a->mode));
      f->mode = mode;
      f->data = temp;
      if (mtcs_mode_is_signed_fixed_point_p/*!SIGNED_FIXED_POINT_MODE_P*/(mtcsMode,a->mode) ==
            mtcs_mode_is_signed_fixed_point_p/*!SIGNED_FIXED_POINT_MODE_P*/(mtcsMode,f->mode))
         overflow_p = fixed_saturate1(self,f->mode, f->data, &f->data, sat_p);
      else{
         /* Take care of the cases when converting between signed and
         unsigned.  */
         if (mtcs_mode_is_signed_fixed_point_p/*!SIGNED_FIXED_POINT_MODE_P*/(mtcsMode,a->mode)){
            /* Signed -> Unsigned.  */
            if (a->data.high < 0){
               if (sat_p){
                  f->data.low = 0;  /* Set to zero.  */
                  f->data.high = 0;  /* Set to zero.  */
               }else
                  overflow_p = true;
            }else
            overflow_p = fixed_saturate1(self,f->mode, f->data, &f->data,sat_p);
         }else{
            /* Unsigned -> Signed.  */
            if (temp.high < 0){
               if (sat_p){
                  /* Set to maximum.  */
                  f->data.low = -1;  /* Set to all ones.  */
                  f->data.high = -1;  /* Set to all ones.  */
                  f->data = f->data.zext (mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,f->mode)
                        +mtcs_mode_get_ibit/*!GET_MODE_IBIT*/(mtcsMode,f->mode));
                  /* Clear the sign.  */
               }else
                  overflow_p = true;
            }else
               overflow_p = fixed_saturate1(self,f->mode, f->data, &f->data,sat_p);
         }
      }
   }

   f->data = f->data.ext (mtcs_mode_is_signed_fixed_point_p/*!SIGNED_FIXED_POINT_MODE_P*/(mtcsMode,f->mode)
         + mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,f->mode)
         + mtcs_mode_get_ibit/*!GET_MODE_IBIT*/(mtcsMode,f->mode),
         mtcs_mode_is_unsigned_fixed_point_p/*!UNSIGNED_FIXED_POINT_MODE_P*/(mtcsMode,f->mode));
   return overflow_p;
}

/* Construct a CONST_FIXED from a bit payload and machine mode MODE.
   The bits in PAYLOAD are sign-extended/zero-extended according to MODE.  */
//原型 fixed_convert fixed-value.h fixed-value.cc
FIXED_VALUE_TYPE mtcs_fixed_fixed_from_double_int (MtcsFixed *self,double_int payload, scalar_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   FIXED_VALUE_TYPE value;
   gcc_assert (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) <= HOST_BITS_PER_DOUBLE_INT);
   if (mtcs_mode_is_signed_scalar_fixed_point_p/*!SIGNED_SCALAR_FIXED_POINT_MODE_P*/(mtcsMode,mode))
      value.data = payload.sext (1 + mtcs_mode_get_ibit/*!GET_MODE_IBIT*/(mtcsMode,mode)
            + mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,mode));
   else if (mtcs_mode_is_unsigned_scalar_fixed_point_p/*!UNSIGNED_SCALAR_FIXED_POINT_MODE_P*/(mtcsMode,mode))
      value.data = payload.zext (mtcs_mode_get_ibit/*!GET_MODE_IBIT*/(mtcsMode,mode)
            + mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,mode));
   else
      gcc_unreachable ();

   value.mode = mode;

   return value;
}



MtcsFixed *mtcs_fixed_new(MtcsMode *mtcsMode)
{
   MtcsFixed *self = n_slice_alloc0 (sizeof(MtcsFixed));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsFixedInit(self);
   return self;
}


