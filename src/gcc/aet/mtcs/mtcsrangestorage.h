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

#ifndef __GCC_MTCS_RANGE_STORAGE_H__
#define __GCC_MTCS_RANGE_STORAGE_H__

#include "mtcsrange.h"

class mtcs_vrange_allocator :public vrange_allocator
{
public:
  // Use GC memory when GC is true, otherwise use obstacks.
   mtcs_vrange_allocator (bool gc = false);
  ~mtcs_vrange_allocator ();
  class vrange_storage *clone (const vrange &r);
  vrange_storage *clone_varying (tree type);
  vrange_storage *clone_undefined (tree type);
  void *alloc (size_t size);
  void free (void *);
  MtcsMode *mtcsMode;
private:
  DISABLE_COPY_AND_ASSIGN (mtcs_vrange_allocator);
  class mtcs_vrange_internal_alloc *m_alloc;
};

// Efficient memory storage for a vrange.
//
// The GTY marker here does nothing but get gengtype to generate the
// ggc_test_and_set_mark calls.  We ignore the derived classes, since
// they don't contain any pointers.

class GTY(()) mtcs_vrange_storage :public vrange_storage
{
public:
  static vrange_storage *alloc (mtcs_vrange_internal_alloc &, const vrange &);
  void get_vrange (vrange &r, tree type) const;
  void set_vrange (const vrange &r);
  bool fits_p (const vrange &r) const;
  bool equal_p (const vrange &r) const;
protected:
  // Stack initialization disallowed.
  mtcs_vrange_storage () { }
};

// Efficient memory storage for an frange.

class MtcsFrangeStorage : public vrange_storage
{
 public:
  static MtcsFrangeStorage *alloc (mtcs_vrange_internal_alloc &, const MtcsFrange &r);
  void set_frange (const MtcsFrange &r);
  void get_frange (MtcsFrange &r, tree type) const;
  bool equal_p (const MtcsFrange &r) const;
  bool fits_p (const MtcsFrange &) const;
  MtcsFrangeStorage (const MtcsFrange &r) { set_frange (r); }
 private:
  DISABLE_COPY_AND_ASSIGN (MtcsFrangeStorage);

  enum value_range_kind m_kind;
  REAL_VALUE_TYPE m_min;
  REAL_VALUE_TYPE m_max;
  bool m_pos_nan;
  bool m_neg_nan;
};


#endif // __GCC_MTCS_RANGE_STORAGE_H__
