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
 * base on stmt.cc
 */

#ifndef __GCC_MTCS_VECTOR_BUILDER__
#define __GCC_MTCS_VECTOR_BUILDER__

#include "../nlib.h"
#include "mtcsmode.h"


//typedef struct _MtcsVectorBuilder MtcsVectorBuilder;
//
//
//struct _MtcsVectorBuilder
//{
//   MtcsMode *mtcsMode;
//};


//MtcsVectorBuilder * mtcs_vector_builder_get();

/* A class for building vector rtx constants.
   Copyright (C) 2017-2024 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

//#ifndef GCC_RTX_VECTOR_BUILDER_H
//#define GCC_RTX_VECTOR_BUILDER_H

#include "vector-builder.h"

/* This class is used to build VECTOR_CSTs from a sequence of elements.
   See vector_builder for more details.  */
class MtcsVectorBuilder : public vector_builder<rtx, machine_mode,MtcsVectorBuilder>
{
  typedef vector_builder<rtx, machine_mode, MtcsVectorBuilder> parent;
  friend class vector_builder<rtx, machine_mode, MtcsVectorBuilder>;

public:
  MtcsVectorBuilder () : m_mode (VOIDmode) {}
  MtcsVectorBuilder (MtcsMode *mtcsMode,machine_mode, unsigned int, unsigned int);
  MtcsVectorBuilder (MtcsMode *mtcsMode);

  rtx build (rtvec);
  rtx build ();

  machine_mode mode () const { return m_mode; }

  void new_vector (machine_mode, unsigned int, unsigned int);
  MtcsMode *mtcsMode;

private:
  bool equal_p (rtx, rtx) const;
  bool allow_steps_p () const;
  bool integral_p (rtx) const;
  poly_wide_int step (rtx, rtx) const;
  rtx apply_step (rtx, unsigned int, const poly_wide_int &) const;
  bool can_elide_p (rtx) const { return true; }
  void note_representative (rtx *, rtx) {}

  static poly_uint64 shape_nelts (machine_mode mode)
    { return GET_MODE_NUNITS (mode); }
  static poly_uint64 nelts_of (const_rtx x)
    { return CONST_VECTOR_NUNITS (x); }
  static unsigned int npatterns_of (const_rtx x)
    { return CONST_VECTOR_NPATTERNS (x); }
  static unsigned int nelts_per_pattern_of (const_rtx x)
    { return CONST_VECTOR_NELTS_PER_PATTERN (x); }

  rtx find_cached_value ();

  machine_mode m_mode;
};

/* Create a new builder for a vector of mode MODE.  Initially encode the
   value as NPATTERNS interleaved patterns with NELTS_PER_PATTERN elements
   each.  */

inline MtcsVectorBuilder::MtcsVectorBuilder (MtcsMode *mmode,machine_mode mode,unsigned int npatterns,unsigned int nelts_per_pattern)
{
   mtcsMode=mmode;
   new_vector (mode, npatterns, nelts_per_pattern);
}

inline MtcsVectorBuilder::MtcsVectorBuilder (MtcsMode *mmode)
{
    mtcsMode=mmode;
}

/* Start building a new vector of mode MODE.  Initially encode the value
   as NPATTERNS interleaved patterns with NELTS_PER_PATTERN elements each.  */

inline void MtcsVectorBuilder::new_vector (machine_mode mode, unsigned int npatterns,unsigned int nelts_per_pattern)
{
  m_mode = mode;
  parent::new_vector (mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,mode), npatterns, nelts_per_pattern);
}

/* Return true if elements ELT1 and ELT2 are equal.  */

inline bool MtcsVectorBuilder::equal_p (rtx elt1, rtx elt2) const
{
  return rtx_equal_p (elt1, elt2);
}

/* Return true if a stepped representation is OK.  We don't allow
   linear series for anything other than integers, to avoid problems
   with rounding.  */
//is_a 调用 scalar_int_mode的 scalar_int_mode::includes_p (machine_mode m) 调用的是 SCALAR_INT_MODE_P (m);

inline bool MtcsVectorBuilder::allow_steps_p () const
{
    //原型   return is_a <scalar_int_mode> (GET_MODE_INNER (m_mode));
   machine_mode mode=mtcs_mode_get_inner(mtcsMode,m_mode);
   return mtcs_mode_is_scalar_int_p(mtcsMode,mode);
}

/* Return true if element ELT can be interpreted as an integer.  */

inline bool MtcsVectorBuilder::integral_p (rtx elt) const
{
  return CONST_SCALAR_INT_P (elt);
}

/* Return the value of element ELT2 minus the value of element ELT1.
   Both elements are known to be CONST_SCALAR_INT_Ps.  */

inline poly_wide_int MtcsVectorBuilder::step (rtx elt1, rtx elt2) const
{
  return (wi::to_poly_wide (elt2, mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,m_mode))
      - wi::to_poly_wide (elt1, mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,m_mode)));
}

#endif
