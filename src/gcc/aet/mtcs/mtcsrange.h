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

#ifndef __GCC_MTCS_RANGE__
#define __GCC_MTCS_RANGE__

#include "../nlib.h"
#include "mtcscomponent.h"


#define MTCS_VR_FRANGE 10



// A floating point range.
//
// The representation is a type with a couple of endpoints, unioned
// with the set of { -NAN, +Nan }.

class MtcsFrange : public vrange
{
  friend class MtcsFrangeStorage;
  friend class vrange_printer;
public:
  MtcsFrange ();
  MtcsFrange (const MtcsFrange &);
  MtcsFrange (tree, tree, value_range_kind = VR_RANGE);
  MtcsFrange (tree type,MtcsMode *mtcsMode);
  MtcsFrange (MtcsMode *mtcsMode,tree type, const REAL_VALUE_TYPE &min, const REAL_VALUE_TYPE &max,
     value_range_kind = VR_RANGE);
  static bool supports_p (const_tree type)
  {
    // ?? Decimal floats can have multiple representations for the
    // same number.  Supporting them may be as simple as just
    // disabling them in singleton_p.  No clue.
    return SCALAR_FLOAT_TYPE_P (type) && !DECIMAL_FLOAT_TYPE_P (type);
  }
  virtual tree type () const override;
  void set (tree type, const REAL_VALUE_TYPE &, const REAL_VALUE_TYPE &,
       value_range_kind = VR_RANGE);
  void set (tree type, const REAL_VALUE_TYPE &, const REAL_VALUE_TYPE &,
       const nan_state &, value_range_kind = VR_RANGE);
  void set_nan (tree type);
  void set_nan (tree type, bool sign);
  void set_nan (tree type, const nan_state &);
  virtual void set_varying (tree type) override;
  virtual void set_undefined () override;
  virtual bool union_ (const vrange &) override;
  virtual bool intersect (const vrange &) override;
  bool contains_p (const REAL_VALUE_TYPE &) const;
  virtual bool singleton_p (tree *result = NULL) const override;
  bool singleton_p (REAL_VALUE_TYPE &r) const;
  virtual bool supports_type_p (const_tree type) const override;
  virtual void accept (const vrange_visitor &v) const override;
  virtual bool zero_p () const override;
  virtual bool nonzero_p () const override;
  virtual void set_nonzero (tree type) override;
  virtual void set_zero (tree type) override;
  virtual void set_nonnegative (tree type) override;
  virtual bool fits_p (const vrange &) const override;
  MtcsFrange& operator= (const MtcsFrange &);
  bool operator== (const MtcsFrange &) const;
  bool operator!= (const MtcsFrange &r) const { return !(*this == r); }
  const REAL_VALUE_TYPE &lower_bound () const;
  const REAL_VALUE_TYPE &upper_bound () const;
  virtual tree lbound () const override;
  virtual tree ubound () const override;
  nan_state get_nan_state () const;
  void update_nan ();
  void update_nan (bool sign);
  void update_nan (tree) = delete; // Disallow silent conversion to bool.
  void update_nan (const nan_state &);
  void clear_nan ();
  void flush_denormals_to_zero ();

  // fpclassify like API
  bool known_isfinite () const;
  bool known_isnan () const;
  bool known_isinf () const;
  bool maybe_isnan () const;
  bool maybe_isnan (bool sign) const;
  bool maybe_isinf () const;
  bool signbit_p (bool &signbit) const;
  bool nan_signbit_p (bool &signbit) const;
  bool known_isnormal () const;
  bool known_isdenormal_or_zero () const;
  //引用mtcsMode,可以获取MtcsRange
  MtcsMode *mtcsMode;
protected:
  virtual bool contains_p (tree cst) const override;
  virtual void set (tree, tree, value_range_kind = VR_RANGE) override;

private:
  bool internal_singleton_p (REAL_VALUE_TYPE * = NULL) const;
  void verify_range ();
  bool normalize_kind ();
  bool union_nans (const MtcsFrange &);
  bool intersect_nans (const MtcsFrange &);
  bool combine_zeros (const MtcsFrange &, bool union_p);

  tree m_type;
  REAL_VALUE_TYPE m_min;
  REAL_VALUE_TYPE m_max;
  bool m_pos_nan;
  bool m_neg_nan;
};


class mtcs_vrange_visitor:vrange_visitor
{
public:
  virtual void visit (const MtcsFrange &) const { }
};

inline const REAL_VALUE_TYPE & MtcsFrange::lower_bound () const
{
  gcc_checking_assert (!undefined_p () && !known_isnan ());
  return m_min;
}

inline const REAL_VALUE_TYPE & MtcsFrange::upper_bound () const
{
  gcc_checking_assert (!undefined_p () && !known_isnan ());
  return m_max;
}

// Return the NAN state.

inline nan_state MtcsFrange::get_nan_state () const
{
  return nan_state (m_pos_nan, m_neg_nan);
}


template <>
inline bool is_a <MtcsFrange> (vrange &v)
{
  return v.m_discriminator == MTCS_VR_FRANGE;
}

template <>
inline MtcsFrange & as_a<MtcsFrange> (vrange &v)
{
  gcc_checking_assert (is_a <MtcsFrange> (v));
  return static_cast <MtcsFrange &> (v);
}


#endif
