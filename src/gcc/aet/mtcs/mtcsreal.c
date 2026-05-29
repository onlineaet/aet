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
 * base on real.cc
 */

/* This file handles generation of all the assembler code
   *real* the instructions of a function.
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
#include "real.h"
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
#include "dfp.h"
#include "real.h"


#include "decimal128.h"
#include "decimal64.h"
#include "decimal32.h"
#ifndef WORDS_BIGENDIAN
#define WORDS_BIGENDIAN 0
#endif
#include "mtcsreg.h"
#include "mtcsreal.h"
#include "mtcstarget.h"
#include "mtcstool.h"

#define M_LOG10_2   0.30102999566398119521

/* Used to classify two numbers simultaneously.  */
#define CLASS2(A, B)  ((A) << 2 | (B))

#if HOST_BITS_PER_LONG != 64 && HOST_BITS_PER_LONG != 32
 #error "Some constant folding done by hand to avoid shift count warnings"
#endif


static const REAL_VALUE_TYPE *real_digit (MtcsReal *self,int n);
static bool do_multiply (REAL_VALUE_TYPE *r, const REAL_VALUE_TYPE *a, const REAL_VALUE_TYPE *b);
static void normalize (MtcsReal *self,REAL_VALUE_TYPE *r);
static bool sticky_rshift_significand (MtcsReal *self,REAL_VALUE_TYPE *r, const REAL_VALUE_TYPE *a,unsigned int n);
static const REAL_VALUE_TYPE *ten_to_ptwo (MtcsReal *self,int n);
static bool do_divide (MtcsReal *self,REAL_VALUE_TYPE *r, const REAL_VALUE_TYPE *a,const REAL_VALUE_TYPE *b);
static unsigned long rtd_divmod (MtcsReal *self,REAL_VALUE_TYPE *num, REAL_VALUE_TYPE *den);
static void round_for_format (MtcsReal *self,const struct real_format *fmt, REAL_VALUE_TYPE *r);
static void clear_significand_below (REAL_VALUE_TYPE *r, unsigned int n);

void mtcs_real_init(MtcsReal *self)
{
    self->float_store_flag_value=NULL;
    self->store_flag_value=1;//原型 #define STORE_FLAG_VALUE 1

}

/*开始 real_format------*/
static bool decimal_p (struct real_format *fmt)
{
    return fmt && fmt->b == 10;
}

static int realFormatSignificand_size (struct real_format *fmt)
{
  if (fmt == NULL)
    return 0;

  if (fmt->b == 10){
      /* Return the size in bits of the largest binary value that can be
     held by the decimal coefficient for this format.  This is one more
     than the number of bits required to hold the largest coefficient
     of this format.  */
      double log2_10 = 3.3219281;
      return fmt->p * log2_10;
  }
  return fmt->p;
}

/* Clear bit N of the significand of R.  */
static inline void clear_significand_bit (REAL_VALUE_TYPE *r, unsigned int n)
{
  r->sig[n / HOST_BITS_PER_LONG] &= ~((unsigned long)1 << (n % HOST_BITS_PER_LONG));
}

/* True if all values of integral type can be represented
   by this floating-point type exactly.  */

static bool can_represent_integral_type_p (struct real_format *fmt,tree type)
{
  gcc_assert (! decimal_p (fmt) && INTEGRAL_TYPE_P (type));

  /* INT?_MIN is power-of-two so it takes
     only one mantissa bit.  */
  bool signed_p = TYPE_SIGN (type) == SIGNED;
  return TYPE_PRECISION (type) - signed_p <= realFormatSignificand_size (fmt);
}



//结束 real_format功能-------------------

/* Test bit N of the significand of R.  */

static inline bool test_significand_bit (REAL_VALUE_TYPE *r, unsigned int n)
{
  /* ??? Compiler bug here if we return this expression directly.
     The conversion to bool strips the "&1" and we wind up testing
     e.g. 2 != 0 -> true.  Seen in gcc version 3.2 20020520.  */
  int t = (r->sig[n / HOST_BITS_PER_LONG] >> (n % HOST_BITS_PER_LONG)) & 1;
  return t;
}

/* Initialize R with the canonical quiet NaN.  */

static inline void get_canonical_qnan (REAL_VALUE_TYPE *r, int sign)
{
  memset (r, 0, sizeof (*r));
  r->cl = rvc_nan;
  r->sign = sign;
  r->canonical = 1;
}

static inline void get_canonical_snan (REAL_VALUE_TYPE *r, int sign)
{
  memset (r, 0, sizeof (*r));
  r->cl = rvc_nan;
  r->sign = sign;
  r->signalling = 1;
  r->canonical = 1;
}

static inline void get_inf (REAL_VALUE_TYPE *r, int sign)
{
  memset (r, 0, sizeof (*r));
  r->cl = rvc_inf;
  r->sign = sign;
}

/* Initialize R with a positive zero.  */
//原型 get_zero real.cc
static inline void get_zero (REAL_VALUE_TYPE *r, int sign)
{
  memset (r, 0, sizeof (*r));
  r->sign = sign;
}

/* Compare significands.  Return tri-state vs zero.  */

static inline int cmp_significands (const REAL_VALUE_TYPE *a, const REAL_VALUE_TYPE *b)
{
  int i;

  for (i = SIGSZ - 1; i >= 0; --i)
    {
      unsigned long ai = a->sig[i];
      unsigned long bi = b->sig[i];

      if (ai > bi)
    return 1;
      if (ai < bi)
    return -1;
    }

  return 0;
}
/* Likewise, but N is specialized to 1.  */

static inline void lshift_significand_1 (REAL_VALUE_TYPE *r, const REAL_VALUE_TYPE *a)
{
  unsigned int i;

  for (i = SIGSZ - 1; i > 0; --i)
    r->sig[i] = (a->sig[i] << 1) | (a->sig[i-1] >> (HOST_BITS_PER_LONG - 1));
  r->sig[0] = a->sig[0] << 1;
}

/* Negate the significand A, placing the result in R.  */

static inline void neg_significand (REAL_VALUE_TYPE *r, const REAL_VALUE_TYPE *a)
{
  bool carry = true;
  int i;
  for (i = 0; i < SIGSZ; ++i){
      unsigned long ri, ai = a->sig[i];

      if (carry){
          if (ai){
              ri = -ai;
              carry = false;
          }else
            ri = ai;
      }else
          ri = ~ai;

      r->sig[i] = ri;
  }
}

/* Subtract the significands of A and B, placing the result in R.  CARRY is
   true if there's a borrow incoming to the least significant word.
   Return true if there was borrow out of the most significant word.  */

static inline bool sub_significands (REAL_VALUE_TYPE *r, const REAL_VALUE_TYPE *a,const REAL_VALUE_TYPE *b, int carry)
{
  int i;
  for (i = 0; i < SIGSZ; ++i){
      unsigned long ai = a->sig[i];
      unsigned long ri = ai - b->sig[i];

      if (carry){
          carry = ri > ai;
          carry |= ~--ri == 0;
      }else
          carry = ri > ai;

      r->sig[i] = ri;
  }
  return carry;
}

/* Add the significands of A and B, placing the result in R.  Return
   true if there was carry out of the most significant word.  */

static inline bool add_significands (REAL_VALUE_TYPE *r, const REAL_VALUE_TYPE *a,const REAL_VALUE_TYPE *b)
{
  bool carry = false;
  int i;
  for (i = 0; i < SIGSZ; ++i){
      unsigned long ai = a->sig[i];
      unsigned long ri = ai + b->sig[i];

      if (carry){
          carry = ri < ai;
          carry |= ++ri == 0;
      }else
          carry = ri < ai;

      r->sig[i] = ri;
  }
  return carry;
}

/* Set bit N of the significand of R.  */

static inline void set_significand_bit (REAL_VALUE_TYPE *r, unsigned int n)
{
  r->sig[n / HOST_BITS_PER_LONG]
    |= (unsigned long)1 << (n % HOST_BITS_PER_LONG);
}

/* Divide the significands of A and B, placing the result in R.  Return
   true if the division was inexact.  */

static inline bool div_significands (REAL_VALUE_TYPE *r, const REAL_VALUE_TYPE *a, const REAL_VALUE_TYPE *b)
{
  REAL_VALUE_TYPE u;
  int i, bit = SIGNIFICAND_BITS - 1;
  unsigned long msb, inexact;

  u = *a;
  memset (r->sig, 0, sizeof (r->sig));

  msb = 0;
  goto start;
  do
    {
      msb = u.sig[SIGSZ-1] & SIG_MSB;
      lshift_significand_1 (&u, &u);
    start:
      if (msb || cmp_significands (&u, b) >= 0)
    {
      sub_significands (&u, &u, b, 0);
      set_significand_bit (r, bit);
    }
    }
  while (--bit >= 0);

  for (i = 0, inexact = 0; i < SIGSZ; i++)
    inexact |= u.sig[i];

  return inexact != 0;
}



/* Calculate R = A + (SUBTRACT_P ? -B : B).  Return true if the
   result may be inexact due to a loss of precision.  */

static bool do_add (MtcsReal *self,REAL_VALUE_TYPE *r, const REAL_VALUE_TYPE *a,const REAL_VALUE_TYPE *b, int subtract_p)
{
  int dexp, sign, exp;
  REAL_VALUE_TYPE t;
  bool inexact = false;

  /* Determine if we need to add or subtract.  */
  sign = a->sign;
  subtract_p = (sign ^ b->sign) ^ subtract_p;

  switch (CLASS2 (a->cl, b->cl))
    {
    case CLASS2 (rvc_zero, rvc_zero):
      /* -0 + -0 = -0, -0 - +0 = -0; all other cases yield +0.  */
      get_zero (r, sign & !subtract_p);
      return false;

    case CLASS2 (rvc_zero, rvc_normal):
    case CLASS2 (rvc_zero, rvc_inf):
    case CLASS2 (rvc_zero, rvc_nan):
      /* 0 + ANY = ANY.  */
    case CLASS2 (rvc_normal, rvc_nan):
    case CLASS2 (rvc_inf, rvc_nan):
    case CLASS2 (rvc_nan, rvc_nan):
      /* ANY + NaN = NaN.  */
    case CLASS2 (rvc_normal, rvc_inf):
      /* R + Inf = Inf.  */
      *r = *b;
      /* Make resulting NaN value to be qNaN. The caller has the
         responsibility to avoid the operation if flag_signaling_nans
         is on.  */
      r->signalling = 0;
      r->sign = sign ^ subtract_p;
      return false;

    case CLASS2 (rvc_normal, rvc_zero):
    case CLASS2 (rvc_inf, rvc_zero):
    case CLASS2 (rvc_nan, rvc_zero):
      /* ANY + 0 = ANY.  */
    case CLASS2 (rvc_nan, rvc_normal):
    case CLASS2 (rvc_nan, rvc_inf):
      /* NaN + ANY = NaN.  */
    case CLASS2 (rvc_inf, rvc_normal):
      /* Inf + R = Inf.  */
      *r = *a;
      /* Make resulting NaN value to be qNaN. The caller has the
         responsibility to avoid the operation if flag_signaling_nans
         is on.  */
      r->signalling = 0;
      return false;

    case CLASS2 (rvc_inf, rvc_inf):
      if (subtract_p)
        /* Inf - Inf = NaN.  */
        get_canonical_qnan (r, 0);
      else
        /* Inf + Inf = Inf.  */
        *r = *a;
      return false;

    case CLASS2 (rvc_normal, rvc_normal):
      break;

    default:
      gcc_unreachable ();
  }

  /* Swap the arguments such that A has the larger exponent.  */
  dexp = REAL_EXP (a) - REAL_EXP (b);
  if (dexp < 0){
      const REAL_VALUE_TYPE *t;
      t = a, a = b, b = t;
      dexp = -dexp;
      sign ^= subtract_p;
  }
  exp = REAL_EXP (a);

  /* If the exponents are not identical, we need to shift the
     significand of B down.  */
  if (dexp > 0){
      /* If the exponents are too far apart, the significands
     do not overlap, which makes the subtraction a noop.  */
      if (dexp >= SIGNIFICAND_BITS){
          *r = *a;
          r->sign = sign;
          return true;
      }

      inexact |= sticky_rshift_significand (self,&t, b, dexp);
      b = &t;
  }

  if (subtract_p){
      if (sub_significands (r, a, b, inexact)){
          /* We got a borrow out of the subtraction.  That means that
             A and B had the same exponent, and B had the larger
             significand.  We need to swap the sign and negate the
             significand.  */
          sign ^= 1;
          neg_significand (r, r);
      }
  }else{
      if (add_significands (r, a, b)){
          /* We got carry out of the addition.  This means we need to
             shift the significand back down one bit and increase the
             exponent.  */
          inexact |= sticky_rshift_significand (self,r, r, 1);
          r->sig[SIGSZ-1] |= SIG_MSB;
          if (++exp > MAX_EXP){
              get_inf (r, sign);
              return true;
          }
      }
  }

  r->cl = rvc_normal;
  r->sign = sign;
  SET_REAL_EXP (r, exp);
  /* Zero out the remaining fields.  */
  r->signalling = 0;
  r->canonical = 0;
  r->decimal = 0;

  /* Re-normalize the result.  */
  normalize (self,r);

  /* Special case: if the subtraction results in zero, the result
     is positive.  */
  if (r->cl == rvc_zero)
    r->sign = 0;
  else
    r->sig[0] |= inexact;

  return inexact;
}


/* Calculate R = A * B.  Return true if the result may be inexact.  */

static bool do_multiply (MtcsReal *self,REAL_VALUE_TYPE *r, const REAL_VALUE_TYPE *a, const REAL_VALUE_TYPE *b)
{
  REAL_VALUE_TYPE u, t, *rr;
  unsigned int i, j, k;
  int sign = a->sign ^ b->sign;
  bool inexact = false;

  switch (CLASS2 (a->cl, b->cl)){
    case CLASS2 (rvc_zero, rvc_zero):
    case CLASS2 (rvc_zero, rvc_normal):
    case CLASS2 (rvc_normal, rvc_zero):
      /* +-0 * ANY = 0 with appropriate sign.  */
      get_zero (r, sign);
      return false;

    case CLASS2 (rvc_zero, rvc_nan):
    case CLASS2 (rvc_normal, rvc_nan):
    case CLASS2 (rvc_inf, rvc_nan):
    case CLASS2 (rvc_nan, rvc_nan):
      /* ANY * NaN = NaN.  */
      *r = *b;
      /* Make resulting NaN value to be qNaN. The caller has the
         responsibility to avoid the operation if flag_signaling_nans
         is on.  */
      r->signalling = 0;
      r->sign = sign;
      return false;

    case CLASS2 (rvc_nan, rvc_zero):
    case CLASS2 (rvc_nan, rvc_normal):
    case CLASS2 (rvc_nan, rvc_inf):
      /* NaN * ANY = NaN.  */
      *r = *a;
      /* Make resulting NaN value to be qNaN. The caller has the
         responsibility to avoid the operation if flag_signaling_nans
         is on.  */
      r->signalling = 0;
      r->sign = sign;
      return false;

    case CLASS2 (rvc_zero, rvc_inf):
    case CLASS2 (rvc_inf, rvc_zero):
      /* 0 * Inf = NaN */
      get_canonical_qnan (r, sign);
      return false;

    case CLASS2 (rvc_inf, rvc_inf):
    case CLASS2 (rvc_normal, rvc_inf):
    case CLASS2 (rvc_inf, rvc_normal):
      /* Inf * Inf = Inf, R * Inf = Inf */
      get_inf (r, sign);
      return false;

    case CLASS2 (rvc_normal, rvc_normal):
      break;

    default:
      gcc_unreachable ();
    }

  if (r == a || r == b)
    rr = &t;
  else
    rr = r;
  get_zero (rr, 0);

  /* Collect all the partial products.  Since we don't have sure access
     to a widening multiply, we split each long into two half-words.

     Consider the long-hand form of a four half-word multiplication:

         A  B  C  D
          *  E  F  G  H
         --------------
            DE DF DG DH
         CE CF CG CH
      BE BF BG BH
       AE AF AG AH

     We construct partial products of the widened half-word products
     that are known to not overlap, e.g. DF+DH.  Each such partial
     product is given its proper exponent, which allows us to sum them
     and obtain the finished product.  */

  for (i = 0; i < SIGSZ * 2; ++i){
      unsigned long ai = a->sig[i / 2];
      if (i & 1)
          ai >>= HOST_BITS_PER_LONG / 2;
      else
          ai &= ((unsigned long)1 << (HOST_BITS_PER_LONG / 2)) - 1;

      if (ai == 0)
          continue;

      for (j = 0; j < 2; ++j){
          int exp = (REAL_EXP (a) - (2*SIGSZ-1-i)*(HOST_BITS_PER_LONG/2)
                 + (REAL_EXP (b) - (1-j)*(HOST_BITS_PER_LONG/2)));

          if (exp > MAX_EXP){
              get_inf (r, sign);
              return true;
          }
          if (exp < -MAX_EXP){
              /* Would underflow to zero, which we shouldn't bother adding.  */
              inexact = true;
              continue;
          }

          memset (&u, 0, sizeof (u));
          u.cl = rvc_normal;
          SET_REAL_EXP (&u, exp);

          for (k = j; k < SIGSZ * 2; k += 2){
              unsigned long bi = b->sig[k / 2];
              if (k & 1)
                  bi >>= HOST_BITS_PER_LONG / 2;
              else
                  bi &= ((unsigned long)1 << (HOST_BITS_PER_LONG / 2)) - 1;
              u.sig[k / 2] = ai * bi;
          }

          normalize (self,&u);
          inexact |= do_add (self,rr, rr, &u, 0);
      }
  }

  rr->sign = sign;
  if (rr != r)
    *r = t;

  return inexact;
}


static void times_pten (MtcsReal *self,REAL_VALUE_TYPE *r, int exp)
{
  REAL_VALUE_TYPE pten, *rr;
  bool negative = (exp < 0);
  int i;

  if (negative){
      exp = -exp;
      pten = *real_digit (self,1);
      rr = &pten;
  }else
    rr = r;

  for (i = 0; exp > 0; ++i, exp >>= 1)
    if (exp & 1)
      do_multiply (self,rr, rr, ten_to_ptwo (self,i));

  if (negative)
    do_divide (self,r, r, &pten);
}

/* Calculate R = A / B.  Return true if the result may be inexact.  */

static bool do_divide (MtcsReal *self,REAL_VALUE_TYPE *r, const REAL_VALUE_TYPE *a,const REAL_VALUE_TYPE *b)
{
  int exp, sign = a->sign ^ b->sign;
  REAL_VALUE_TYPE t, *rr;
  bool inexact;

  switch (CLASS2 (a->cl, b->cl))
    {
    case CLASS2 (rvc_zero, rvc_zero):
      /* 0 / 0 = NaN.  */
    case CLASS2 (rvc_inf, rvc_inf):
      /* Inf / Inf = NaN.  */
      get_canonical_qnan (r, sign);
      return false;

    case CLASS2 (rvc_zero, rvc_normal):
    case CLASS2 (rvc_zero, rvc_inf):
      /* 0 / ANY = 0.  */
    case CLASS2 (rvc_normal, rvc_inf):
      /* R / Inf = 0.  */
      get_zero (r, sign);
      return false;

    case CLASS2 (rvc_normal, rvc_zero):
      /* R / 0 = Inf.  */
    case CLASS2 (rvc_inf, rvc_zero):
      /* Inf / 0 = Inf.  */
      get_inf (r, sign);
      return false;

    case CLASS2 (rvc_zero, rvc_nan):
    case CLASS2 (rvc_normal, rvc_nan):
    case CLASS2 (rvc_inf, rvc_nan):
    case CLASS2 (rvc_nan, rvc_nan):
      /* ANY / NaN = NaN.  */
      *r = *b;
      /* Make resulting NaN value to be qNaN. The caller has the
         responsibility to avoid the operation if flag_signaling_nans
         is on.  */
      r->signalling = 0;
      r->sign = sign;
      return false;

    case CLASS2 (rvc_nan, rvc_zero):
    case CLASS2 (rvc_nan, rvc_normal):
    case CLASS2 (rvc_nan, rvc_inf):
      /* NaN / ANY = NaN.  */
      *r = *a;
      /* Make resulting NaN value to be qNaN. The caller has the
         responsibility to avoid the operation if flag_signaling_nans
         is on.  */
      r->signalling = 0;
      r->sign = sign;
      return false;

    case CLASS2 (rvc_inf, rvc_normal):
      /* Inf / R = Inf.  */
      get_inf (r, sign);
      return false;

    case CLASS2 (rvc_normal, rvc_normal):
      break;

    default:
      gcc_unreachable ();
    }

  if (r == a || r == b)
    rr = &t;
  else
    rr = r;

  /* Make sure all fields in the result are initialized.  */
  get_zero (rr, 0);
  rr->cl = rvc_normal;
  rr->sign = sign;

  exp = REAL_EXP (a) - REAL_EXP (b) + 1;
  if (exp > MAX_EXP)
    {
      get_inf (r, sign);
      return true;
    }
  if (exp < -MAX_EXP)
    {
      get_zero (r, sign);
      return true;
    }
  SET_REAL_EXP (rr, exp);

  inexact = div_significands (rr, a, b);

  /* Re-normalize the result.  */
  normalize (self,rr);
  rr->sig[0] |= inexact;

  if (rr != r)
    *r = t;

  return inexact;
}

/* Returns 10**2**N.  */

static const REAL_VALUE_TYPE *ten_to_ptwo (MtcsReal *self,int n)
{
  static REAL_VALUE_TYPE tens[EXP_BITS];

  gcc_assert (n >= 0);
  gcc_assert (n < EXP_BITS);
  if (tens[n].cl == rvc_zero){
      if (n < (HOST_BITS_PER_WIDE_INT == 64 ? 5 : 4)){
          HOST_WIDE_INT t = 10;
          int i;
          for (i = 0; i < n; ++i)
            t *= t;
          mtcs_real_real_from_integer (self,&tens[n], VOIDmode, t, UNSIGNED);
      }else{
          const REAL_VALUE_TYPE *t = ten_to_ptwo (self,n - 1);
          do_multiply (self,&tens[n], t, t);
      }
  }

  return &tens[n];
}

/* Returns N.  */

static const REAL_VALUE_TYPE *real_digit (MtcsReal *self,int n)
{
  static REAL_VALUE_TYPE num[10];
  gcc_assert (n >= 0);
  gcc_assert (n <= 9);

  if (n > 0 && num[n].cl == rvc_zero)
      mtcs_real_real_from_integer (self,&num[n], VOIDmode, n, UNSIGNED);

  return &num[n];
}


/* Left-shift the significand of A by N bits; put the result in the
   significand of R.  */

static void lshift_significand (MtcsReal *self,REAL_VALUE_TYPE *r, const REAL_VALUE_TYPE *a,unsigned int n)
{
  unsigned int i, ofs = n / HOST_BITS_PER_LONG;

  n &= HOST_BITS_PER_LONG - 1;
  if (n == 0){
      for (i = 0; ofs + i < SIGSZ; ++i)
          r->sig[SIGSZ-1-i] = a->sig[SIGSZ-1-i-ofs];
      for (; i < SIGSZ; ++i)
          r->sig[SIGSZ-1-i] = 0;
  }else
    for (i = 0; i < SIGSZ; ++i){
        r->sig[SIGSZ-1-i] = (((ofs + i >= SIGSZ ? 0 : a->sig[SIGSZ-1-i-ofs]) << n)
             | ((ofs + i + 1 >= SIGSZ ? 0 : a->sig[SIGSZ-1-i-ofs-1]) >> (HOST_BITS_PER_LONG - n)));
  }
}

/* Adjust the exponent and significand of R such that the most
   significant bit is set.  We underflow to zero and overflow to
   infinity here, without denormals.  (The intermediate representation
   exponent is large enough to handle target denormals normalized.)  */
//原型 normalize real.cc
static void normalize (MtcsReal *self,REAL_VALUE_TYPE *r)
{
  int shift = 0, exp;
  int i, j;

  if (r->decimal)
    return;

  /* Find the first word that is nonzero.  */
  for (i = SIGSZ - 1; i >= 0; i--)
    if (r->sig[i] == 0)
      shift += HOST_BITS_PER_LONG;
    else
      break;

  /* Zero significand flushes to zero.  */
  if (i < 0){
      r->cl = rvc_zero;
      SET_REAL_EXP (r, 0);
      return;
  }

  /* Find the first bit that is nonzero.  */
  for (j = 0; ; j++)
    if (r->sig[i] & ((unsigned long)1 << (HOST_BITS_PER_LONG - 1 - j)))
      break;
  shift += j;

  if (shift > 0){
      exp = REAL_EXP (r) - shift;
      if (exp > MAX_EXP)
          get_inf (r, r->sign);
      else if (exp < -MAX_EXP)
          get_zero (r, r->sign);
      else{
          SET_REAL_EXP (r, exp);
          lshift_significand (self,r, r, shift);
      }
  }
}

/* Render R, an integral value, as a floating point constant with no
   specified exponent.  */

static void decimal_integer_string (MtcsReal *self,char *str, const REAL_VALUE_TYPE *r_orig,size_t buf_size)
{
  int dec_exp, digit, digits;
  REAL_VALUE_TYPE r, pten;
  char *p;
  bool sign;

  r = *r_orig;

  if (r.cl == rvc_zero){
      strcpy (str, "0.");
      return;
  }

  sign = r.sign;
  r.sign = 0;

  dec_exp = REAL_EXP (&r) * M_LOG10_2;
  digits = dec_exp + 1;
  gcc_assert ((digits + 2) < (int)buf_size);

  pten = *real_digit (self,1);
  times_pten (self,&pten, dec_exp);

  p = str;
  if (sign)
    *p++ = '-';

  digit = rtd_divmod (self,&r, &pten);
  gcc_assert (digit >= 0 && digit <= 9);
  *p++ = digit + '0';
  while (--digits > 0){
      times_pten (self,&r, 1);
      digit = rtd_divmod (self,&r, &pten);
      *p++ = digit + '0';
  }
  *p++ = '.';
  *p++ = '\0';
}

/* Convert a real with an integral value to decimal float.  */

static void decimal_from_integer (MtcsReal *self,REAL_VALUE_TYPE *r)
{
  char str[256];
  decimal_integer_string (self,str, r, sizeof (str) - 1);
  decimal_real_from_string (r, str);
}

/* Right-shift the significand of A by N bits; put the result in the
   significand of R.  If any one bits are shifted out, return true.  */

static bool sticky_rshift_significand (MtcsReal *self,REAL_VALUE_TYPE *r, const REAL_VALUE_TYPE *a,unsigned int n)
{
  unsigned long sticky = 0;
  unsigned int i, ofs = 0;

  if (n >= HOST_BITS_PER_LONG){
      for (i = 0, ofs = n / HOST_BITS_PER_LONG; i < ofs; ++i)
          sticky |= a->sig[i];
      n &= HOST_BITS_PER_LONG - 1;
  }

  if (n != 0){
      sticky |= a->sig[ofs] & (((unsigned long)1 << n) - 1);
      for (i = 0; i < SIGSZ; ++i){
          r->sig[i] = (((ofs + i >= SIGSZ ? 0 : a->sig[ofs + i]) >> n) | ((ofs + i + 1 >= SIGSZ ? 0 : a->sig[ofs + i + 1])
              << (HOST_BITS_PER_LONG - n)));
      }
  }else{
      for (i = 0; ofs + i < SIGSZ; ++i)
          r->sig[i] = a->sig[ofs + i];
      for (; i < SIGSZ; ++i)
          r->sig[i] = 0;
  }

  return sticky != 0;
}

/* A subroutine of real_to_decimal.  Compute the quotient and remainder
   of NUM / DEN.  Return the quotient and place the remainder in NUM.
   It is expected that NUM / DEN are close enough that the quotient is
   small.  */

static unsigned long rtd_divmod (MtcsReal *self,REAL_VALUE_TYPE *num, REAL_VALUE_TYPE *den)
{
  unsigned long q, msb;
  int expn = REAL_EXP (num), expd = REAL_EXP (den);

  if (expn < expd)
    return 0;

  q = msb = 0;
  goto start;
  do{
      msb = num->sig[SIGSZ-1] & SIG_MSB;
      q <<= 1;
      lshift_significand_1 (num, num);
    start:
      if (msb || cmp_significands (num, den) >= 0){
          sub_significands (num, num, den, 0);
          q |= 1;
      }
  }while (--expn >= expd);

  SET_REAL_EXP (num, expd);
  normalize (self,num);

  return q;
}


/* Clear bits 0..N-1 of the significand of R.  */

static void clear_significand_below (REAL_VALUE_TYPE *r, unsigned int n)
{
  int i, w = n / HOST_BITS_PER_LONG;

  for (i = 0; i < w; ++i)
    r->sig[i] = 0;

  /* We are actually passing N == SIGNIFICAND_BITS which would result
     in an out-of-bound access below.  */
  if (n % HOST_BITS_PER_LONG != 0)
    r->sig[w] &= ~(((unsigned long)1 << (n % HOST_BITS_PER_LONG)) - 1);
}


static void round_for_format (MtcsReal *self,const struct real_format *fmt, REAL_VALUE_TYPE *r)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  int p2, np2, i, w;
  int emin2m1, emax2;
  bool round_up = false;

  if (r->decimal){
      if (fmt->b == 10){
          decimal_round_for_format (fmt, r);
          return;
      }
      /* FIXME. We can come here via fp_easy_constant
     (e.g. -O0 on '_Decimal32 x = 1.0 + 2.0dd'), but have not
     investigated whether this convert needs to be here, or
     something else is missing. */
      decimal_real_convert (r, mtcs_mode_get_real_format/*!REAL_MODE_FORMAT (DFmode)*/(mtcsMode,mtcsMode->modes.M_DFmode), r);
  }

  p2 = fmt->p;
  emin2m1 = fmt->emin - 1;
  emax2 = fmt->emax;

  np2 = SIGNIFICAND_BITS - p2;
  switch (r->cl){
    underflow:
      get_zero (r, r->sign);
      /* FALLTHRU */
    case rvc_zero:
      if (!fmt->has_signed_zero)
          r->sign = 0;
      return;

    overflow:
      get_inf (r, r->sign);
    case rvc_inf:
      return;

    case rvc_nan:
      clear_significand_below (r, np2);
      return;

    case rvc_normal:
      break;

    default:
      gcc_unreachable ();
  }

  /* Check the range of the exponent.  If we're out of range,
     either underflow or overflow.  */
  if (REAL_EXP (r) > emax2)
    goto overflow;
  else if (REAL_EXP (r) <= emin2m1){
      int diff;

      if (!fmt->has_denorm){
          /* Don't underflow completely until we've had a chance to round.  */
          if (REAL_EXP (r) < emin2m1)
            goto underflow;
      }else{
          diff = emin2m1 - REAL_EXP (r) + 1;
          if (diff > p2)
            goto underflow;

          /* De-normalize the significand.  */
          r->sig[0] |= sticky_rshift_significand (self,r, r, diff);
          SET_REAL_EXP (r, REAL_EXP (r) + diff);
      }
  }

  if (!fmt->round_towards_zero){
      /* There are P2 true significand bits, followed by one guard bit,
         followed by one sticky bit, followed by stuff.  Fold nonzero
         stuff into the sticky bit.  */
      unsigned long sticky;
      bool guard, lsb;

      sticky = 0;
      for (i = 0, w = (np2 - 1) / HOST_BITS_PER_LONG; i < w; ++i)
          sticky |= r->sig[i];
      sticky |= r->sig[w] & (((unsigned long)1 << ((np2 - 1) % HOST_BITS_PER_LONG)) - 1);

      guard = test_significand_bit (r, np2 - 1);
      lsb = test_significand_bit (r, np2);

      /* Round to even.  */
      round_up = guard && (sticky || lsb);
  }

  if (round_up){
      REAL_VALUE_TYPE u;
      get_zero (&u, 0);
      set_significand_bit (&u, np2);

      if (add_significands (r, r, &u)){
          /* Overflow.  Means the significand had been all ones, and
             is now all zeros.  Need to increase the exponent, and
             possibly re-normalize it.  */
          SET_REAL_EXP (r, REAL_EXP (r) + 1);
          if (REAL_EXP (r) > emax2)
            goto overflow;
          r->sig[SIGSZ-1] = SIG_MSB;
      }
  }

  /* Catch underflow that we deferred until after rounding.  */
  if (REAL_EXP (r) <= emin2m1)
    goto underflow;

  /* Clear out trailing garbage.  */
  clear_significand_below (r, np2);
}




/* Initialize R from the wide_int VAL_IN.  Round it to format FMT if
   FMT is nonnull.  */
//原型 real_from_integer real.h real.cc
void mtcs_real_real_from_integer (MtcsReal *self,REAL_VALUE_TYPE *r, mtcs_mode mode, const wide_int_ref val_in, signop sgn)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  struct real_format *fmt=mtcs_mode_get_real_format(mtcsMode,mode);

  int maxBitSizeModeAnyInt=mtcs_mode_get_max_bitsize_mode_any_int/*!MAX_BITSIZE_MODE_ANY_INT*/(mtcsMode);
  if (val_in == 0)
    get_zero (r, 0);
  else{
      unsigned int len = val_in.get_precision ();
      int i, j, e = 0;
      int maxbitlen = maxBitSizeModeAnyInt/*!MAX_BITSIZE_MODE_ANY_INT*/ + HOST_BITS_PER_WIDE_INT;
      const unsigned int realmax = (SIGNIFICAND_BITS / HOST_BITS_PER_WIDE_INT* HOST_BITS_PER_WIDE_INT);

      memset (r, 0, sizeof (*r));
      r->cl = rvc_normal;
      r->sign = wi::neg_p (val_in, sgn);

      /* We have to ensure we can negate the largest negative number.  */
      wide_int val = wide_int::from (val_in, maxbitlen, sgn);

      if (r->sign)
          val = -val;

      /* Ensure a multiple of HOST_BITS_PER_WIDE_INT, ceiling, as elt
     won't work with precisions that are not a multiple of
     HOST_BITS_PER_WIDE_INT.  */
      len += HOST_BITS_PER_WIDE_INT - 1;

      /* Ensure we can represent the largest negative number.  */
      len += 1;

      len = len/HOST_BITS_PER_WIDE_INT * HOST_BITS_PER_WIDE_INT;

      /* Cap the size to the size allowed by real.h.  */
      if (len > realmax){
          HOST_WIDE_INT cnt_l_z;
          cnt_l_z = wi::clz (val);

          if (maxbitlen - cnt_l_z > realmax){
              e = maxbitlen - cnt_l_z - realmax;
              /* This value is too large, we must shift it right to
             preserve all the bits we can, and then bump the
             exponent up by that amount.  */
              val = wi::lrshift (val, e);
          }
          len = realmax;
      }

      /* Clear out top bits so elt will work with precisions that aren't
     a multiple of HOST_BITS_PER_WIDE_INT.  */
      val = wide_int::from (val, len, sgn);
      len = len / HOST_BITS_PER_WIDE_INT;

      SET_REAL_EXP (r, len * HOST_BITS_PER_WIDE_INT + e);

      j = SIGSZ - 1;
      if (HOST_BITS_PER_LONG == HOST_BITS_PER_WIDE_INT)
        for (i = len - 1; i >= 0; i--){
            r->sig[j--] = val.elt (i);
            if (j < 0)
              break;
        }
      else{
          gcc_assert (HOST_BITS_PER_LONG*2 == HOST_BITS_PER_WIDE_INT);
          for (i = len - 1; i >= 0; i--){
              HOST_WIDE_INT e = val.elt (i);
              r->sig[j--] = e >> (HOST_BITS_PER_LONG - 1) >> 1;
              if (j < 0)
                  break;
              r->sig[j--] = e;
              if (j < 0)
                  break;
         }
      }

      normalize (self,r);
  }

  if (decimal_p (fmt))
    decimal_from_integer (self,r);
  if (fmt)
    mtcs_real_real_convert/*!real_convert*/(self,r, mode, r);
}


/* Extend or truncate to a new format.  */
//原型 real_convert real.h real.cc
void mtcs_real_real_convert (MtcsReal *self,REAL_VALUE_TYPE *r, mtcs_mode mode, const REAL_VALUE_TYPE *a)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   struct real_format *fmt=mtcs_mode_get_real_format(mtcsMode,mode);
  *r = *a;

  if (a->decimal || fmt->b == 10)
    decimal_real_convert (r, fmt, a);

  round_for_format (self,fmt, r);

  /* Make resulting NaN value to be qNaN. The caller has the
     responsibility to avoid the operation if flag_signaling_nans
     is on.  */
  if (r->cl == rvc_nan)
    r->signalling = 0;

  /* round_for_format de-normalizes denormals.  Undo just that part.  */
  if (r->cl == rvc_normal)
    normalize (self,r);
}

/* Legacy.  Likewise, except return the struct directly.  */
//原型 real_value_truncate real.h real.cc
REAL_VALUE_TYPE mtcs_real_real_value_truncate (MtcsReal *self,mtcs_mode mode, REAL_VALUE_TYPE a)
{
  REAL_VALUE_TYPE r;
  mtcs_real_real_convert/*!real_convert*/(self,&r, mode, &a);
  return r;
}

/* Return true if truncating to FMT is exact.  */
//原型 exact_real_truncate real.h real.cc
bool mtcs_real_exact_real_truncate (MtcsReal *self,mtcs_mode mode, const REAL_VALUE_TYPE *a)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  struct real_format *fmt=mtcs_mode_get_real_format(mtcsMode,mode);
  REAL_VALUE_TYPE t;
  int emin2m1;

  /* Don't allow conversion to denormals.  */
  emin2m1 = fmt->emin - 1;
  if (REAL_EXP (a) <= emin2m1)
    return false;

  /* After conversion to the new format, the value must be identical.  */
  mtcs_real_real_convert/*!real_convert*/(self,&t, mode, a);
  return real_identical (&t, a);
}

//原型 real_to_target real.h real.cc
long mtcs_real_real_to_target (MtcsReal *self,long *buf, const REAL_VALUE_TYPE *r_orig,mtcs_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  struct real_format *fmt=mtcs_mode_get_real_format(mtcsMode,mode);
  REAL_VALUE_TYPE r;
  long buf1;

  r = *r_orig;
  round_for_format (self,fmt, &r);

  if (!buf)
    buf = &buf1;
  (*fmt->encode) (fmt, buf, &r);
  return *buf;
}

/* Read R from the given target format.  Read the words of the result
   in target word order in BUF.  There are always 32 bits in each
   long, no matter the size of the host long.  */
//原型 real_from_target real.h real.cc
void mtcs_real_real_from_target (MtcsReal *self,REAL_VALUE_TYPE *r, const long *buf, mtcs_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   struct real_format *fmt=mtcs_mode_get_real_format(mtcsMode,mode);
  (*fmt->decode) (fmt, r, buf);
}

/* Legacy.  Similar, but return the result directly.  */
//原型 real_from_string2 real.h real.cc
REAL_VALUE_TYPE mtcs_real_real_from_string2 (MtcsReal *self,const char *s, mtcs_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  struct real_format *fmt=mtcs_mode_get_real_format(mtcsMode,mode);
  REAL_VALUE_TYPE r;
  real_from_string (&r, s);
  if (fmt)
    mtcs_real_real_convert/*!real_convert*/(self,&r, mode, &r);
  return r;
}

//原型 STORE_FLAG_VALUE 每个平台不一样 default.h STORE_FLAG_VALUE=1 gcn STORE_FLAG_VALUE=-1;
int mtcs_real_get_store_flag_value(MtcsReal *self)
{
    return self->store_flag_value;
}

void mtcs_real_set_store_flag_value(MtcsReal *self,int value)
{
    self->store_flag_value=1;
}

//原型 FLOAT_STORE_FLAG_VALUE (mode);
REAL_VALUE_TYPE mtcs_real_float_store_flag_value(MtcsReal *self,mtcs_mode mode)
{
    if(self->float_store_flag_value)
        return self->float_store_flag_value(self,mode);
    REAL_VALUE_TYPE r;
    return r;
}

/* Return a new REAL_CST node whose type is TYPE
   and whose value is the integer value of the INTEGER_CST node I.  */
//原型 real_value_from_int_cst real.h tree.cc
REAL_VALUE_TYPE mtcs_real_real_value_from_int_cst (MtcsReal *self,const_tree type, const_tree i)
{
   REAL_VALUE_TYPE d;

   /* Clear all bits of the real value type so that we can later do
   bitwise comparisons to see if two values are the same.  */
   memset (&d, 0, sizeof d);

   mtcs_real_real_from_integer/*!real_from_integer*/(self,&d, type ? TYPE_MODE (type) : VOIDmode, wi::to_wide (i),
         TYPE_SIGN (TREE_TYPE (i)));
   return d;
}

/* Return A truncated to an integral value toward zero.  */
//原型 do_fix_trunc real.cc
static void do_fix_trunc (REAL_VALUE_TYPE *r, const REAL_VALUE_TYPE *a)
{
   *r = *a;

   switch (r->cl){
      case rvc_zero:
      case rvc_inf:
      case rvc_nan:
         /* Make resulting NaN value to be qNaN. The caller has the
         responsibility to avoid the operation if flag_signaling_nans
         is on.  */
         r->signalling = 0;
         break;

      case rvc_normal:
         if (r->decimal){
            decimal_do_fix_trunc (r, a);
            return;
         }
         if (REAL_EXP (r) <= 0)
            get_zero (r, r->sign);
         else if (REAL_EXP (r) < SIGNIFICAND_BITS)
            clear_significand_below (r, SIGNIFICAND_BITS - REAL_EXP (r));
         break;

      default:
         gcc_unreachable ();
   }
}

/* Round X to the nearest integer not larger in absolute value, i.e.
   towards zero, placing the result in R in format FMT.  */
//原型 real_trunc real.h real.cc
void mtcs_real_real_trunc (MtcsReal *self,REAL_VALUE_TYPE *r, format_helper fmt,const REAL_VALUE_TYPE *x)
{
  do_fix_trunc (r, x);
  if (fmt)
     mtcs_real_real_convert/*!real_convert*/(self,r, fmt, r);
}

//原型 init_emit_once rtl.h emit-rtl.cc 的real部份
void mtcs_real_init_once(MtcsReal *self)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

    scalar_float_mode double_mode;
    //double_mode = mtcs_mode_float_mode_for_size/*!float_mode_for_size*/(mtcsMode,TI_DOUBLE_TYPE);
    machine_mode mode = mtcs_mode_for_floating_type/*!targetm.c.mode_for_floating_type*/(mtcsMode,TI_DOUBLE_TYPE);
    double_mode = mtcs_mode_as_a<scalar_float_mode> (mtcsMode,mode);

    mtcs_real_real_from_integer/*!real_from_integer*/(self,&self->dconst0, double_mode, 0, SIGNED);
    mtcs_real_real_from_integer/*!real_from_integer*/(self,&self->dconst1, double_mode, 1, SIGNED);
    mtcs_real_real_from_integer/*!real_from_integer*/(self,&self->dconst2, double_mode, 2, SIGNED);
    self->dconstm0 = self->dconst0;
    self->dconstm0.sign = 1;

    self->dconstm1 = self->dconst1;
    self->dconstm1.sign = 1;

    self->dconsthalf = self->dconst1;
    SET_REAL_EXP (&self->dconsthalf, REAL_EXP (&self->dconsthalf) - 1);

    real_inf (&self->dconstinf);
    real_inf (&self->dconstninf, true);
}


//原型 real_max_representable value-range.h
REAL_VALUE_TYPE mtcs_real_max_representable (MtcsReal *self,const_tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   REAL_VALUE_TYPE r;
   char buf[128];
   get_max_float ( mtcs_mode_get_real_format/*!REAL_MODE_FORMAT (DFmode)*/(mtcsMode,TYPE_MODE (type)),
   buf, sizeof (buf), false);
   int res = real_from_string (&r, buf);
   gcc_checking_assert (!res);
   return r;
}


// Return the minimum representable value for TYPE.
//原型 real_min_representable value-range.h
REAL_VALUE_TYPE mtcs_real_min_representable (MtcsReal *self,const_tree type)
{
  REAL_VALUE_TYPE r = mtcs_real_max_representable/*!real_max_representable*/(self,type);
  r = real_value_negate (&r);
  return r;
}

// Return the minimum value for TYPE.
//原型 frange_val_min value-range.h
REAL_VALUE_TYPE mtcs_real_frange_val_min (MtcsReal *self,const_tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   if (mtcs_mode_honor_infinities/*!HONOR_INFINITIES*/(mtcsMode,type))
      return dconstinf;
   else
      return mtcs_real_min_representable/*!real_max_representable*/(self,type);
}

// Return the maximum value for TYPE.
//原型 frange_val_max value-range.h
REAL_VALUE_TYPE mtcs_real_frange_val_max (MtcsReal *self,const_tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   if (mtcs_mode_honor_infinities/*!HONOR_INFINITIES*/(mtcsMode,type))
      return dconstinf;
   else
      return mtcs_real_max_representable/*!real_max_representable*/(self,type);
}

// Return TRUE if R is the minimum value for TYPE.
//原型 frange_val_is_min  value-range.h
bool mtcs_real_frange_val_is_min (MtcsReal *self,const REAL_VALUE_TYPE &r, const_tree type)
{
   REAL_VALUE_TYPE min = mtcs_real_frange_val_min/*!frange_val_min*/(self,type);
   return real_identical (&min, &r);
}

// Return TRUE if R is the max value for TYPE.
//原型 frange_val_is_max  value-range.h
bool mtcs_real_frange_val_is_max (MtcsReal *self,const REAL_VALUE_TYPE &r, const_tree type)
{
  REAL_VALUE_TYPE max = mtcs_real_frange_val_max/*!frange_val_max*/(self,type);
  return real_identical (&max, &r);
}

/* Fills R with the largest finite value representable in mode MODE.
   If SIGN is nonzero, R is set to the most negative finite value.  */
//原型 real_maxval real.h real.cc
void mtcs_real_real_maxval (MtcsReal *self,REAL_VALUE_TYPE *r, int sign, machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   const struct real_format *fmt;
   int np2;

   fmt = mtcs_mode_get_real_format/*!REAL_MODE_FORMAT (DFmode)*/(mtcsMode,mode);
   gcc_assert (fmt);
   memset (r, 0, sizeof (*r));

   if (fmt->b == 10)
      mtcs_real_decimal_real_maxval/*!decimal_real_maxval*/(self,r, sign, mode);
   else{
      r->cl = rvc_normal;
      r->sign = sign;
      SET_REAL_EXP (r, fmt->emax);

      np2 = SIGNIFICAND_BITS - fmt->p;
      memset (r->sig, -1, SIGSZ * sizeof (unsigned long));
      clear_significand_below (r, np2);

      if (fmt->pnan < fmt->p)
         /* This is an IBM extended double format made up of two IEEE
         doubles.  The value of the long double is the sum of the
         values of the two parts.  The most significant part is
         required to be the value of the long double rounded to the
         nearest double.  Rounding means we need a slightly smaller
         value for LDBL_MAX.  */
         clear_significand_bit (r, SIGNIFICAND_BITS - fmt->pnan - 1);
   }
}

//////////////////////////dfp.cc 移到这里-------------------------
/* Fills R with the largest finite value representable in mode MODE.
   If SIGN is nonzero, R is set to the most negative finite value.  */
//原型 decimal_real_maxval dfp.h dfp.cc
void mtcs_real_decimal_real_maxval (MtcsReal *self,REAL_VALUE_TYPE *r, int sign, machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   const char *max;

   if(mode==mtcsMode->modes.M_SDmode)
      max = "9.999999E96";
   else if(mode==mtcsMode->modes.M_DDmode)
      max = "9.999999999999999E384";
   else if(mode==mtcsMode->modes.M_TDmode)
      max = "9.999999999999999999999999999999999E6144";
   else
      gcc_unreachable ();

   decimal_real_from_string (r, max);
   if (sign)
      decimal128SetSign ((decimal128 *) r->sig, 1);
   r->sign = sign;
}




