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
 * base on range.cc
 */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "predict.h"
#include "memmodel.h"
#include "tm_p.h"
#include "ssa.h"
#include "optabs.h"
#include "expmed.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "alias.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "attribs.h"
#include "varasm.h"
#include "except.h"
#include "insn-attr.h"
#include "dojump.h"
#include "explow.h"
#include "calls.h"
#include "stmt.h"
/* Include expr.h after insn-config.h so we get HAVE_conditional_move.  */
#include "expr.h"
#include "optabs-tree.h"
#include "libfuncs.h"
#include "reload.h"
#include "langhooks.h"
#include "common/common-target.h"
#include "tree-dfa.h"
#include "tree-ssa-live.h"
#include "tree-outof-ssa.h"
#include "tree-ssa-address.h"
#include "builtins.h"
#include "ccmp.h"
#include "gimple-iterator.h"
#include "gimple-fold.h"
#include "rtx-vector-builder.h"
#include "tree-pretty-print.h"
#include "flags.h"

#include "mtcsrange.h"
#include "mtcstarget.h"
#include "mtcscompile.h"


/**
 * MtcsFrange实现
 */
MtcsFrange::MtcsFrange ()
  : vrange (MTCS_VR_FRANGE)
{
  set_undefined ();
}

MtcsFrange::MtcsFrange (const MtcsFrange &src)
  : vrange (MTCS_VR_FRANGE)
{
  *this = src;
}

MtcsFrange::MtcsFrange (tree type,MtcsMode *mtcsMode)
  : vrange (MTCS_VR_FRANGE)
{
  this->mtcsMode = mtcsMode;
  set_varying (type);
}

// frange constructor from REAL_VALUE_TYPE endpoints.
MtcsFrange::MtcsFrange (MtcsMode *mtcsMode,tree type,
      const REAL_VALUE_TYPE &min, const REAL_VALUE_TYPE &max,
      value_range_kind kind)
  : vrange (MTCS_VR_FRANGE)
{
  this->mtcsMode = mtcsMode;
  set (type, min, max, kind);
}

// frange constructor from trees.
MtcsFrange::MtcsFrange (tree min, tree max, value_range_kind kind)
  : vrange (MTCS_VR_FRANGE)
{
  set (min, max, kind);
}

tree MtcsFrange::type () const
{
  gcc_checking_assert (!undefined_p ());
  return m_type;
}

void MtcsFrange::set_varying (tree type)
{
   fprintf(stderr,"set_varying mode:%p this:%p\n",mtcsMode,this);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReal *mtcsReal = mtcs_target_get_real(mtcsTarget);

   m_kind = VR_VARYING;
   m_type = type;
   m_min = mtcs_real_frange_val_min/*!frange_val_min*/(mtcsReal,type);
   m_max = mtcs_real_frange_val_max/*!frange_val_max*/(mtcsReal,type);
   if (mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,m_type)){
      m_pos_nan = true;
      m_neg_nan = true;
   }else{
      m_pos_nan = false;
      m_neg_nan = false;
   }
}

void MtcsFrange::set_undefined ()
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   m_kind = VR_UNDEFINED;
   m_type = NULL;
   m_pos_nan = false;
   m_neg_nan = false;
   // m_min and m_min are uninitialized as they are REAL_VALUE_TYPE ??.
   if (mtcsOptionsItem->x_flag_checking)
      verify_range ();
}

// Set the NAN bits to NAN and adjust the range.
void MtcsFrange::update_nan (const nan_state &nan)
{

   gcc_checking_assert (!undefined_p ());
   if (mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,m_type)){
      m_pos_nan = nan.pos_p ();
      m_neg_nan = nan.neg_p ();
      normalize_kind ();
      if (flag_checking)
         verify_range ();
   }
}

// Set the NAN bit to +-NAN.
void MtcsFrange::update_nan ()
{
   gcc_checking_assert (!undefined_p ());
   nan_state nan (true);
   update_nan (nan);
}

// Like above, but set the sign of the NAN.
void MtcsFrange::update_nan (bool sign)
{
   gcc_checking_assert (!undefined_p ());
   nan_state nan (/*pos=*/!sign, /*neg=*/sign);
   update_nan (nan);
}

bool MtcsFrange::contains_p (tree cst) const
{
   return contains_p (*TREE_REAL_CST_PTR (cst));
}

// Clear the NAN bit and adjust the range.
void MtcsFrange::clear_nan ()
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   gcc_checking_assert (!undefined_p ());
   m_pos_nan = false;
   m_neg_nan = false;
   normalize_kind ();
   if (mtcsOptionsItem->x_flag_checking)
      verify_range ();
}


// Build a NAN with a state of NAN.
void MtcsFrange::set_nan (tree type, const nan_state &nan)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   gcc_checking_assert (nan.pos_p () || nan.neg_p ());
   if (mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,type)){
      m_kind = VR_NAN;
      m_type = type;
      m_neg_nan = nan.neg_p ();
      m_pos_nan = nan.pos_p ();
      if (mtcsOptionsItem->x_flag_checking)
         verify_range ();
   }else
      set_undefined ();
}

// Build a signless NAN of type TYPE.
void MtcsFrange::set_nan (tree type)
{
   nan_state nan (true);
   set_nan (type, nan);
}

// Build a NAN of type TYPE with SIGN.
void MtcsFrange::set_nan (tree type, bool sign)
{
   nan_state nan (/*pos=*/!sign, /*neg=*/sign);
   set_nan (type, nan);
}

// Return TRUE if range is known to be finite.

bool MtcsFrange::known_isfinite () const
{
   if (undefined_p () || varying_p () || m_kind == VR_ANTI_RANGE)
      return false;
   return (!maybe_isnan () && !real_isinf (&m_min) && !real_isinf (&m_max));
}

// Return TRUE if range is known to be normal.
bool MtcsFrange::known_isnormal () const
{
   if (!known_isfinite ())
      return false;

   machine_mode mode = TYPE_MODE (type ());
   return (!real_isdenormal (&m_min, mode) && !real_isdenormal (&m_max, mode)
      && !real_iszero (&m_min) && !real_iszero (&m_max)
      && (!real_isneg (&m_min) || real_isneg (&m_max)));
}

// Return TRUE if range is known to be denormal.
bool MtcsFrange::known_isdenormal_or_zero () const
{
   if (!known_isfinite ())
      return false;

   machine_mode mode = TYPE_MODE (type ());
   return ((real_isdenormal (&m_min, mode) || real_iszero (&m_min))
         && (real_isdenormal (&m_max, mode) || real_iszero (&m_max)));
}

// Return TRUE if range may be infinite.
bool MtcsFrange::maybe_isinf () const
{
   if (undefined_p () || m_kind == VR_ANTI_RANGE || m_kind == VR_NAN)
      return false;
   if (varying_p ())
      return true;
   return real_isinf (&m_min) || real_isinf (&m_max);
}

// Return TRUE if range is known to be the [-INF,-INF] or [+INF,+INF].
bool MtcsFrange::known_isinf () const
{
   return (m_kind == VR_RANGE
      && !maybe_isnan ()
      && real_identical (&m_min, &m_max)
      && real_isinf (&m_min));
}

// Return TRUE if range is possibly a NAN.
bool MtcsFrange::maybe_isnan () const
{
   if (undefined_p ())
      return false;
   return m_pos_nan || m_neg_nan;
}

// Return TRUE if range is possibly a NAN with SIGN.

bool MtcsFrange::maybe_isnan (bool sign) const
{
   if (undefined_p ())
      return false;
   if (sign)
      return m_neg_nan;
   return m_pos_nan;
}

// Return TRUE if range is a +NAN or -NAN.
bool MtcsFrange::known_isnan () const
{
   return m_kind == VR_NAN;
}

// If the signbit for the range is known, set it in SIGNBIT and return
// TRUE.

bool MtcsFrange::signbit_p (bool &signbit) const
{
   if (undefined_p ())
      return false;

   // NAN with unknown sign.
   if (m_pos_nan && m_neg_nan)
      return false;
   // No NAN.
   if (!m_pos_nan && !m_neg_nan){
      if (m_min.sign == m_max.sign){
         signbit = m_min.sign;
         return true;
      }
      return false;
   }
   // NAN with known sign.
   bool nan_sign = m_neg_nan;
   if (known_isnan () || (nan_sign == m_min.sign && nan_sign == m_max.sign)){
      signbit = nan_sign;
      return true;
   }
   return false;
}

// If range has a NAN with a known sign, set it in SIGNBIT and return
// TRUE.
bool MtcsFrange::nan_signbit_p (bool &signbit) const
{
   if (undefined_p ())
      return false;

   if (m_pos_nan == m_neg_nan)
      return false;

   signbit = m_neg_nan;
   return true;
}


// Frange implementation.
void MtcsFrange::accept (const vrange_visitor &v) const
{
  //v.visit (*this);
}

bool MtcsFrange::fits_p (const vrange &) const
{
   return true;
}

// Flush denormal endpoints to the appropriate 0.0.
void MtcsFrange::flush_denormals_to_zero ()
{
   if (undefined_p () || known_isnan ())
   return;

   machine_mode mode = TYPE_MODE (type ());
   // Flush [x, -DENORMAL] to [x, -0.0].
   if (real_isdenormal (&m_max, mode) && real_isneg (&m_max)){
      if (mtcs_mode_honor_signed_zeros/*!HONOR_SIGNED_ZEROS*/(mtcsMode,m_type))
         m_max = dconstm0;
      else
         m_max = dconst0;
   }
   // Flush [+DENORMAL, x] to [+0.0, x].
   if (real_isdenormal (&m_min, mode) && !real_isneg (&m_min))
      m_min = dconst0;
}

// Setter for franges.
void MtcsFrange::set (tree type, const REAL_VALUE_TYPE &min, const REAL_VALUE_TYPE &max,
        const nan_state &nan, value_range_kind kind)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReal *mtcsReal = mtcs_target_get_real(mtcsTarget);

   switch (kind){
      case VR_UNDEFINED:
         set_undefined ();
         return;
      case VR_VARYING:
      case VR_ANTI_RANGE:
         set_varying (type);
         return;
      case VR_RANGE:
         break;
      default:
         gcc_unreachable ();
   }

   gcc_checking_assert (!real_isnan (&min) && !real_isnan (&max));

   m_kind = kind;
   m_type = type;
   m_min = min;
   m_max = max;
   if (mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,m_type)){
      m_pos_nan = nan.pos_p ();
      m_neg_nan = nan.neg_p ();
   }else{
      m_pos_nan = false;
      m_neg_nan = false;
   }

   if (!mtcs_mode_has_signed_zeros/*!MODE_HAS_SIGNED_ZEROS*/(mtcsMode,TYPE_MODE(m_type))){
      if (real_iszero (&m_min, 1))
         m_min.sign = 0;
      if (real_iszero (&m_max, 1))
         m_max.sign = 0;
   }else if (!mtcs_mode_honor_signed_zeros/*!HONOR_SIGNED_ZEROS*/(mtcsMode,m_type)){
      if (real_iszero (&m_max, 1))
         m_max.sign = 0;
      if (real_iszero (&m_min, 0))
         m_min.sign = 1;
   }

   // For -ffinite-math-only we can drop ranges outside the
   // representable numbers to min/max for the type.
   if (!mtcs_mode_honor_infinities/*!HONOR_INFINITIES*/(mtcsMode,m_type)){
      REAL_VALUE_TYPE min_repr = mtcs_real_frange_val_min/*!frange_val_min*/(mtcsReal,m_type);
      REAL_VALUE_TYPE max_repr = mtcs_real_frange_val_max/*!frange_val_max*/(mtcsReal,m_type);
      if (real_less (&m_min, &min_repr))
         m_min = min_repr;
      else if (real_less (&max_repr, &m_min))
         m_min = max_repr;
      if (real_less (&max_repr, &m_max))
         m_max = max_repr;
      else if (real_less (&m_max, &min_repr))
         m_max = min_repr;
   }

   // Check for swapped ranges.
   gcc_checking_assert (real_compare (LE_EXPR, &min, &max));

   normalize_kind ();
}

// Setter for an frange defaulting the NAN possibility to +-NAN when
// HONOR_NANS.
void MtcsFrange::set (tree type,
        const REAL_VALUE_TYPE &min, const REAL_VALUE_TYPE &max,
        value_range_kind kind)
{
   set (type, min, max, nan_state (true), kind);
}

void MtcsFrange::set (tree min, tree max, value_range_kind kind)
{
   set (TREE_TYPE (min),  *TREE_REAL_CST_PTR (min), *TREE_REAL_CST_PTR (max), kind);
}

// Normalize range to VARYING or UNDEFINED, or vice versa.  Return
// TRUE if anything changed.
//
// A range with no known properties can be dropped to VARYING.
// Similarly, a VARYING with any properties should be dropped to a
// VR_RANGE.  Normalizing ranges upon changing them ensures there is
// only one representation for a given range.
bool MtcsFrange::normalize_kind ()
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReal *mtcsReal = mtcs_target_get_real(mtcsTarget);

   if (m_kind == VR_RANGE
   && mtcs_real_frange_val_is_min/*!frange_val_is_min*/(mtcsReal,m_min, m_type)
   && mtcs_real_frange_val_is_max/*!frange_val_is_max*/(mtcsReal,m_max, m_type)){
      if (!mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,m_type) || (m_pos_nan && m_neg_nan)){
         set_varying (m_type);
         return true;
      }
   }else if (m_kind == VR_VARYING){
      if (mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,m_type) && (!m_pos_nan || !m_neg_nan)){
         m_kind = VR_RANGE;
         m_min = mtcs_real_frange_val_min/*!frange_val_min*/(mtcsReal,m_type);
         m_max = mtcs_real_frange_val_max/*!frange_val_max*/(mtcsReal,m_type);
         if (flag_checking)
            verify_range ();
         return true;
      }
   }else if (m_kind == VR_NAN && !m_pos_nan && !m_neg_nan)
      set_undefined ();
   return false;
}

// Union or intersect the zero endpoints of two ranges.  For example:
//   [-0,  x] U [+0,  x] => [-0,  x]
//   [ x, -0] U [ x, +0] => [ x, +0]
//   [-0,  x] ^ [+0,  x] => [+0,  x]
//   [ x, -0] ^ [ x, +0] => [ x, -0]
//
// UNION_P is true when performing a union, or false when intersecting.
bool MtcsFrange::combine_zeros (const MtcsFrange &r, bool union_p)
{
   gcc_checking_assert (!undefined_p () && !known_isnan ());

   bool changed = false;
   if (real_iszero (&m_min) && real_iszero (&r.m_min)
   && real_isneg (&m_min) != real_isneg (&r.m_min)){
      m_min.sign = union_p;
      changed = true;
   }
   if (real_iszero (&m_max) && real_iszero (&r.m_max)
   && real_isneg (&m_max) != real_isneg (&r.m_max)){
      m_max.sign = !union_p;
      changed = true;
   }
   // If the signs are swapped, the resulting range is empty.
   if (m_min.sign == 0 && m_max.sign == 1){
      if (maybe_isnan ())
         m_kind = VR_NAN;
      else
         set_undefined ();
      changed = true;
   }
   return changed;
}

// Union two ranges when one is known to be a NAN.

bool MtcsFrange::union_nans (const MtcsFrange &r)
{
   gcc_checking_assert (known_isnan () || r.known_isnan ());
   bool changed = false;
   if (known_isnan () && m_kind != r.m_kind){
      m_kind = r.m_kind;
      m_min = r.m_min;
      m_max = r.m_max;
      changed = true;
   }
   if (m_pos_nan != r.m_pos_nan || m_neg_nan != r.m_neg_nan){
      m_pos_nan |= r.m_pos_nan;
      m_neg_nan |= r.m_neg_nan;
      changed = true;
   }
   if (changed){
      normalize_kind ();
      return true;
   }
   return false;
}

bool MtcsFrange::union_ (const vrange &v)
{
  // const MtcsFrange &r = as_a <MtcsFrange> (v);
   const MtcsFrange &r =  static_cast <const MtcsFrange &> (v);
   if (r.undefined_p () || varying_p ())
      return false;
   if (undefined_p () || r.varying_p ()){
      *this = r;
      return true;
   }

   // Combine NAN info.
   if (known_isnan () || r.known_isnan ())
      return union_nans (r);
   bool changed = false;
   if (m_pos_nan != r.m_pos_nan || m_neg_nan != r.m_neg_nan){
      m_pos_nan |= r.m_pos_nan;
      m_neg_nan |= r.m_neg_nan;
      changed = true;
   }

   // Combine endpoints.
   if (real_less (&r.m_min, &m_min)){
      m_min = r.m_min;
      changed = true;
   }
   if (real_less (&m_max, &r.m_max)){
      m_max = r.m_max;
      changed = true;
   }

   if (mtcs_mode_honor_signed_zeros/*!HONOR_SIGNED_ZEROS*/(mtcsMode,m_type))
      changed |= combine_zeros (r, true);

   changed |= normalize_kind ();
   return changed;
}

// Intersect two ranges when one is known to be a NAN.
bool MtcsFrange::intersect_nans (const MtcsFrange &r)
{
  gcc_checking_assert (known_isnan () || r.known_isnan ());

  m_pos_nan &= r.m_pos_nan;
  m_neg_nan &= r.m_neg_nan;
  if (maybe_isnan ())
    m_kind = VR_NAN;
  else
    set_undefined ();
  if (flag_checking)
    verify_range ();
  return true;
}

bool MtcsFrange::intersect (const vrange &v)
{
  // const MtcsFrange &r = as_a <MtcsFrange> (v);
   const MtcsFrange &r =  static_cast <const MtcsFrange &> (v);

   if (undefined_p () || r.varying_p ())
      return false;
   if (r.undefined_p ()){
      set_undefined ();
      return true;
   }
   if (varying_p ()){
      *this = r;
      return true;
   }

   // Combine NAN info.
   if (known_isnan () || r.known_isnan ())
      return intersect_nans (r);
   bool changed = false;
   if (m_pos_nan != r.m_pos_nan || m_neg_nan != r.m_neg_nan){
      m_pos_nan &= r.m_pos_nan;
      m_neg_nan &= r.m_neg_nan;
      changed = true;
   }

   // Combine endpoints.
   if (real_less (&m_min, &r.m_min)){
      m_min = r.m_min;
      changed = true;
   }
   if (real_less (&r.m_max, &m_max)){
      m_max = r.m_max;
      changed = true;
   }
   // If the endpoints are swapped, the resulting range is empty.
   if (real_less (&m_max, &m_min)){
      if (maybe_isnan ())
         m_kind = VR_NAN;
      else
         set_undefined ();
      if (flag_checking)
         verify_range ();
      return true;
   }

   if (mtcs_mode_honor_signed_zeros/*!HONOR_SIGNED_ZEROS*/(mtcsMode,m_type))
      changed |= combine_zeros (r, false);

   changed |= normalize_kind ();
   return changed;
}

MtcsFrange & MtcsFrange::operator= (const MtcsFrange &src)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   m_kind = src.m_kind;
   m_type = src.m_type;
   m_min = src.m_min;
   m_max = src.m_max;
   m_pos_nan = src.m_pos_nan;
   m_neg_nan = src.m_neg_nan;

   if (mtcsOptionsItem->x_flag_checking)
      verify_range ();
   return *this;
}

bool MtcsFrange::operator== (const MtcsFrange &src) const
{
   if (m_kind == src.m_kind){
      if (undefined_p ())
         return true;

      if (varying_p ())
         return types_compatible_p (m_type, src.m_type);

      bool nan1 = known_isnan ();
      bool nan2 = src.known_isnan ();
      if (nan1 || nan2){
         if (nan1 && nan2)
            return (m_pos_nan == src.m_pos_nan   && m_neg_nan == src.m_neg_nan);
         return false;
      }

      return (real_identical (&m_min, &src.m_min)
                              && real_identical (&m_max, &src.m_max)
                              && m_pos_nan == src.m_pos_nan
                              && m_neg_nan == src.m_neg_nan
                              && types_compatible_p (m_type, src.m_type));
   }
   return false;
}

// Return TRUE if range contains R.
bool MtcsFrange::contains_p (const REAL_VALUE_TYPE &r) const
{
   gcc_checking_assert (m_kind != VR_ANTI_RANGE);

   if (undefined_p ())
      return false;

   if (varying_p ())
      return true;

   if (real_isnan (&r)){
      // No NAN in range.
      if (!m_pos_nan && !m_neg_nan)
         return false;
      // Both +NAN and -NAN are present.
      if (m_pos_nan && m_neg_nan)
         return true;
      return m_neg_nan == r.sign;
   }
   if (known_isnan ())
      return false;

   if (real_compare (GE_EXPR, &r, &m_min) && real_compare (LE_EXPR, &r, &m_max)){
      // Make sure the signs are equal for signed zeros.
      if (mtcs_mode_honor_signed_zeros/*!HONOR_SIGNED_ZEROS*/(mtcsMode,m_type) && real_iszero (&r))
         return r.sign == m_min.sign || r.sign == m_max.sign;
      return true;
   }
   return false;
}

// If range is a singleton, place it in RESULT and return TRUE.  If
// RESULT is NULL, just return TRUE.
//
// A NAN can never be a singleton.

bool MtcsFrange::internal_singleton_p (REAL_VALUE_TYPE *result) const
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReal *mtcsReal = mtcs_target_get_real(mtcsTarget);

   if (m_kind == VR_RANGE && real_identical (&m_min, &m_max)){
      // Return false for any singleton that may be a NAN.
      if (mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,m_type) && maybe_isnan ())
         return false;

      if (mtcs_mode_is_composite_p/*!MODE_COMPOSITE_P*/(mtcsMode,TYPE_MODE (m_type))){
         // For IBM long doubles, if the value is +-Inf or is exactly
         // representable in double, the other double could be +0.0
         // or -0.0.  Since this means there is more than one way to
         // represent a value, return false to avoid propagating it.
         // See libgcc/config/rs6000/ibm-ldouble-format for details.
         if (real_isinf (&m_min))
            return false;
         REAL_VALUE_TYPE r;
         mtcs_real_real_convert/*!real_convert*/(mtcsReal,&r, mtcsMode->modes.M_DFmode, &m_min);
         if (real_identical (&r, &m_min))
            return false;
      }

      if (result)
         *result = m_min;
      return true;
   }
   return false;
}

bool MtcsFrange::singleton_p (tree *result) const
{
   if (internal_singleton_p ()){
      if (result)
         *result = build_real (m_type, m_min);
      return true;
   }
   return false;
}

bool MtcsFrange::singleton_p (REAL_VALUE_TYPE &r) const
{
   return internal_singleton_p (&r);
}

bool MtcsFrange::supports_type_p (const_tree type) const
{
   return supports_p (type);
}

void MtcsFrange::verify_range ()
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReal *mtcsReal = mtcs_target_get_real(mtcsTarget);

   if (!undefined_p ())
      gcc_checking_assert (mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,m_type) || !maybe_isnan ());
   switch (m_kind){
      case VR_UNDEFINED:
         gcc_checking_assert (!m_type);
         return;
      case VR_VARYING:
         gcc_checking_assert (m_type);
         gcc_checking_assert (mtcs_real_frange_val_is_min/*!frange_val_is_min*/(mtcsReal,m_min, m_type));
         gcc_checking_assert (mtcs_real_frange_val_is_max/*!frange_val_is_max*/(mtcsReal,m_max, m_type));
         if (mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,m_type))
            gcc_checking_assert (m_pos_nan && m_neg_nan);
         else
            gcc_checking_assert (!m_pos_nan && !m_neg_nan);
         return;
      case VR_RANGE:
         gcc_checking_assert (m_type);
         break;
      case VR_NAN:
         gcc_checking_assert (m_type);
         gcc_checking_assert (m_pos_nan || m_neg_nan);
         return;
      default:
         gcc_unreachable ();
   }

   // NANs cannot appear in the endpoints of a range.
   gcc_checking_assert (!real_isnan (&m_min) && !real_isnan (&m_max));

   // Make sure we don't have swapped ranges.
   gcc_checking_assert (!real_less (&m_max, &m_min));

   // [ +0.0, -0.0 ] is nonsensical.
   gcc_checking_assert (!(real_iszero (&m_min, 0) && real_iszero (&m_max, 1)));

   // If all the properties are clear, we better not span the entire
   // domain, because that would make us varying.
   if (m_pos_nan && m_neg_nan)
      gcc_checking_assert (!mtcs_real_frange_val_is_min/*!frange_val_is_min*/(mtcsReal,m_min, m_type)
            || !mtcs_real_frange_val_is_max/*!frange_val_is_max*/(mtcsReal,m_max, m_type));
}

// We can't do much with nonzeros yet.
void MtcsFrange::set_nonzero (tree type)
{
   set_varying (type);
}

// We can't do much with nonzeros yet.
bool MtcsFrange::nonzero_p () const
{
   return false;
}

// Set range to [+0.0, +0.0] if honoring signed zeros, or [0.0, 0.0]
// otherwise.
void MtcsFrange::set_zero (tree type)
{
   if (mtcs_mode_honor_signed_zeros/*!HONOR_SIGNED_ZEROS*/(mtcsMode,type)){
      set (type, dconstm0, dconst0);
      clear_nan ();
   }else
      set (type, dconst0, dconst0);
}

// Return TRUE for any zero regardless of sign.
bool MtcsFrange::zero_p () const
{
   return (m_kind == VR_RANGE   && real_iszero (&m_min)  && real_iszero (&m_max));
}

// Set the range to non-negative numbers, that is [+0.0, +INF].
//
// The NAN in the resulting range (if HONOR_NANS) has a varying sign
// as there are no guarantees in IEEE 754 wrt to the sign of a NAN,
// except for copy, abs, and copysign.  It is the responsibility of
// the caller to set the NAN's sign if desired.
void MtcsFrange::set_nonnegative (tree type)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReal *mtcsReal = mtcs_target_get_real(mtcsTarget);

   set (type, dconst0, mtcs_real_frange_val_max/*!frange_val_max*/(mtcsReal,type));
}

tree MtcsFrange::lbound () const
{
   return build_real (type (), lower_bound ());
}

tree MtcsFrange::ubound () const
{
   return build_real (type (), upper_bound ());
}

/**
 * value-range.h中的类 vrange中声明
 */
vrange *value_range::mtcs_create_vrange (const_tree type) //zclei
{
   vrange *range=NULL;
   if(!mtcs_compile_is_compiling(mtcs_compile_get()))
      return NULL;
   MtcsTarget   *mtcsTarget = mtcs_compile_get_current_target(mtcs_compile_get());
   if(MtcsFrange::supports_p(type)){
      fprintf(stderr,"vrange *value_range::mtcs_create_vrange ---\n");
      range = new MtcsFrange(type,mtcsTarget->mtcsMode);
   }
   return range;
}
