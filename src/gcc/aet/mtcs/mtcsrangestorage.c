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
#include "system.h"
#include "coretypes.h"
#include "backend.h"


#include "rtl.h"
#include "tree.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "regs.h"
#include "ira.h"
#include "rtl-error.h"
#include "addresses.h"
#include "function-abi.h"


#include "insn-codes.h"
#include "tree.h"
#include "gimple.h"
#include "ssa.h"
#include "gimple-pretty-print.h"
#include "gimple-range.h"
#include "value-range-storage.h"
#include "tree-cfg.h"
#include "target.h"
#include "attribs.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "cfganal.h"

#include "mtcsrangestorage.h"
#include "mtcstarget.h"


#include "aet/aetprintgimple.h"
#include "aet/aetprinttree.h"

///////////////////////////------- vrange_allocator--------------------

// Generic memory allocator to share one interface between GC and
// obstack allocators.

class mtcs_vrange_internal_alloc
{
public:
   mtcs_vrange_internal_alloc () { }
  virtual ~mtcs_vrange_internal_alloc () { }
  virtual void *alloc (size_t size) = 0;
  virtual void free (void *) = 0;
private:
  DISABLE_COPY_AND_ASSIGN (mtcs_vrange_internal_alloc);
};

class mtcs_vrange_obstack_alloc : public mtcs_vrange_internal_alloc
{
public:
   mtcs_vrange_obstack_alloc ()
  {
    obstack_init (&m_obstack);
  }
  virtual ~mtcs_vrange_obstack_alloc () final override
  {
    obstack_free (&m_obstack, NULL);
  }
  virtual void *alloc (size_t size) final override
  {
    return obstack_alloc (&m_obstack, size);
  }
  virtual void free (void *) final override { }
private:
  obstack m_obstack;
};

class mtcs_vrange_ggc_alloc : public mtcs_vrange_internal_alloc
{
public:
   mtcs_vrange_ggc_alloc () { }
  virtual ~mtcs_vrange_ggc_alloc () final override { }
  virtual void *alloc (size_t size) final override
  {
    return ggc_internal_alloc (size);
  }
  virtual void free (void *p) final override
  {
    return ggc_free (p);
  }
};

mtcs_vrange_allocator::mtcs_vrange_allocator (bool gc)
{
  if (gc)
    m_alloc = new mtcs_vrange_ggc_alloc;
  else
    m_alloc = new mtcs_vrange_obstack_alloc;
}

mtcs_vrange_allocator::~mtcs_vrange_allocator ()
{
  delete m_alloc;
}

void *mtcs_vrange_allocator::alloc (size_t size)
{

  return m_alloc->alloc (size);
}

void mtcs_vrange_allocator::free (void *p)
{
  m_alloc->free (p);
}

// Allocate a new vrange_storage object initialized to R and return
// it.
vrange_storage *mtcs_vrange_allocator::clone (const vrange &r)
{
   fprintf(stderr,"mtcsrangestorage.c mtcs_vrange_allocator::clone 关键 分配 range\n");
  return mtcs_vrange_storage::alloc (*m_alloc, r);
}


// Allocate a new irange_storage object initialized to R.
//原型  irange_storage *irange_storage::alloc  value-range-storage.h  value-range-storage.cc
static irange_storage *irange_storage_alloc (mtcs_vrange_internal_alloc &allocator, const irange &r)
{
   size_t size = irange_storage::size(r);
   irange_storage *p = static_cast <irange_storage *> (allocator.alloc (size));
   new (p) irange_storage (r);//见 value-range-storage.c irange_storage::irange_storage (const irange &r) 298行
   return p;
}

static const unsigned int NINTS = 4;

//============================================================================
// prange_storage implementation
//============================================================================
//原型  prange_storage *prange_storage::alloc value-range-storage.h  value-range-storage.cc
static prange_storage * prange_storage_alloc (mtcs_vrange_internal_alloc &allocator, const prange &r)
{
   size_t size = sizeof (prange_storage);
   if (!r.undefined_p ()){
      unsigned prec = TYPE_PRECISION (r.type ());
      size += trailing_wide_ints<NINTS>::extra_size (prec);
   }
   prange_storage *p = static_cast <prange_storage *> (allocator.alloc (size));
   new (p) prange_storage (r);
   return p;
}

// Allocate a new frange_storage object initialized to R.
//原型  frange_storage *frange_storage::alloc value-range-storage.h  value-range-storage.cc
MtcsFrangeStorage *mtcsfrange_storage_alloc (mtcs_vrange_internal_alloc &allocator, const MtcsFrange &r)
{
  size_t size = sizeof (MtcsFrangeStorage);
  MtcsFrangeStorage *p = static_cast <MtcsFrangeStorage *> (allocator.alloc (size));
  new (p) MtcsFrangeStorage (r);
  fprintf(stderr,"mtcsfrange_storage_alloc --创建 MtcsFrangeStorage  %p\n",p);
  return p;
}

vrange_storage *mtcs_vrange_allocator::clone_varying (tree type)
{
  if (irange::supports_p (type))
    return irange_storage_alloc/*!irange_storage::alloc*/(*m_alloc, int_range <1> (type));
  if (prange::supports_p (type))
    return prange_storage_alloc/*!prange_storage::alloc*/(*m_alloc, prange (type));
  if (MtcsFrange::supports_p (type))
    return mtcsfrange_storage_alloc/*!frange_storage::alloc*/(*m_alloc, MtcsFrange (type,mtcsMode));
  return NULL;
}

vrange_storage * mtcs_vrange_allocator::clone_undefined (tree type)
{
   if (irange::supports_p (type))
      return irange_storage_alloc/*!irange_storage::alloc*/(*m_alloc, int_range <1> ());
   if (prange::supports_p (type))
      return prange_storage_alloc/*!prange_storage::alloc*/(*m_alloc, prange ());
   if (MtcsFrange::supports_p (type))
      return mtcsfrange_storage_alloc/*!frange_storage::alloc*/(*m_alloc, MtcsFrange ());
   return NULL;
}

///////////////////////mtcs_varange_storage  实现-----------------



// Allocate a new vrange_storage object initialized to R and return
// it.  Return NULL if R is unsupported.
vrange_storage *mtcs_vrange_storage::alloc (mtcs_vrange_internal_alloc &allocator, const vrange &r)
{
  if (is_a <irange> (r))
    return irange_storage_alloc/*!irange_storage::alloc*/(allocator, as_a <irange> (r));
  if (is_a <prange> (r))
     return prange_storage_alloc/*!prange_storage::alloc*/(allocator, as_a <prange> (r));
  if (is_a <MtcsFrange> (r))
     return mtcsfrange_storage_alloc/*!frange_storage::alloc*/(allocator, as_a <MtcsFrange> (r));
  return NULL;
}

// Set storage to R.

void mtcs_vrange_storage::set_vrange (const vrange &r)
{
   if (is_a <irange> (r)){
      irange_storage *s = static_cast <irange_storage *> ((irange_storage *)this);
      gcc_checking_assert (s->fits_p (as_a <irange> (r)));
      s->set_irange (as_a <irange> (r));
   }else if (is_a <prange> (r)){
      prange_storage *s = static_cast <prange_storage *> ((prange_storage*)this);
      gcc_checking_assert (s->fits_p (as_a <prange> (r)));
      s->set_prange (as_a <prange> (r));
   }else if (is_a <MtcsFrange> (r)){
      MtcsFrangeStorage *s = static_cast <MtcsFrangeStorage *> ((MtcsFrangeStorage*)this);
      gcc_checking_assert (s->fits_p (as_a <MtcsFrange> (r)));
      s->set_frange (as_a <MtcsFrange> (r));
   }else
      gcc_unreachable ();

   // Verify that reading back from the cache didn't drop bits.
   if (flag_checking
   // FIXME: Avoid checking frange, as it currently pessimizes some ranges:
   //
   // gfortran.dg/pr49472.f90 pessimizes [0.0, 1.0] into [-0.0, 1.0].
   && !is_a <frange> (r)
   && !r.undefined_p ()){
      value_range tmp (r);
      get_vrange (tmp, r.type ());
      gcc_checking_assert (tmp == r);
   }
}

// Restore R from storage.
void mtcs_vrange_storage::get_vrange (vrange &r, tree type) const
{
   fprintf(stderr,"mtcs_vrange_storage::get_vrange :this:%p r:%p\n",this,&r,get_tree_code_name(TREE_CODE(type)));
   if (is_a <irange> (r)){
      const irange_storage *s = static_cast <const irange_storage *> ((irange_storage*)this);
      s->get_irange (as_a <irange> (r), type);
   }else if (is_a <prange> (r)){
      const prange_storage *s = static_cast <const prange_storage *> ((prange_storage*)this);
      s->get_prange (as_a <prange> (r), type);
   }else if (is_a <MtcsFrange> (r)){
      const MtcsFrangeStorage *s = static_cast <const MtcsFrangeStorage *> ((MtcsFrangeStorage *)this);
      s->get_frange (as_a <MtcsFrange> (r), type);
   }else
      gcc_unreachable ();
}

// Return TRUE if storage can fit R.

bool mtcs_vrange_storage::fits_p (const vrange &r) const
{
   if (is_a <irange> (r)){
      const irange_storage *s = static_cast <const irange_storage *> ((irange_storage*)this);
      return s->fits_p (as_a <irange> (r));
   }
   if (is_a <prange> (r)){
      const prange_storage *s = static_cast <const prange_storage *> ((prange_storage*)this);
      return s->fits_p (as_a <prange> (r));
   }
   if (is_a <MtcsFrange> (r)){
      const MtcsFrangeStorage *s = static_cast <const MtcsFrangeStorage *> ((MtcsFrangeStorage*)this);
      return s->fits_p (as_a <MtcsFrange> (r));
   }
   gcc_unreachable ();
}

// Return TRUE if the range in storage is equal to R.  It is the
// caller's responsibility to verify that the type of the range in
// storage matches that of R.

bool mtcs_vrange_storage::equal_p (const vrange &r) const
{
   if (is_a <irange> (r)){
      const irange_storage *s = static_cast <const irange_storage *> ((irange_storage*)this);
      return s->equal_p (as_a <irange> (r));
   }
   if (is_a <prange> (r)){
      const prange_storage *s = static_cast <const prange_storage *> ((prange_storage*)this);
      return s->equal_p (as_a <prange> (r));
   }
   if (is_a <MtcsFrange> (r)){
      const MtcsFrangeStorage *s = static_cast <const MtcsFrangeStorage *> ((MtcsFrangeStorage*)this);
      return s->equal_p (as_a <MtcsFrange> (r));
   }
   gcc_unreachable ();
}

//============================================================================
// frange_storage implementation
//============================================================================

// Allocate a new frange_storage object initialized to R.

MtcsFrangeStorage *MtcsFrangeStorage::alloc (mtcs_vrange_internal_alloc &allocator, const MtcsFrange &r)
{
  fprintf(stderr,"MtcsFrangeStorage::alloc 用 MtcsFrange 创建MtcsFrangeStorage\n");
  size_t size = sizeof (MtcsFrangeStorage);
  MtcsFrangeStorage *p = static_cast <MtcsFrangeStorage *> (allocator.alloc (size));
  new (p) MtcsFrangeStorage (r);
  return p;
}

void MtcsFrangeStorage::set_frange (const MtcsFrange &r)
{
   fprintf(stderr,"MtcsFrangeStorage::set_frange 用 MtcsFrange 设置 MtcsFrangeStorage\n");

   gcc_checking_assert (fits_p (r));
   m_kind = r.m_kind;
   m_min = r.m_min;
   m_max = r.m_max;
   m_pos_nan = r.m_pos_nan;
   m_neg_nan = r.m_neg_nan;
}

void MtcsFrangeStorage::get_frange (MtcsFrange &r, tree type) const
{
   gcc_checking_assert (r.supports_type_p (type));
   fprintf(stderr,"MtcsFrangeStorage::get_frange ---- this:%p r:%p\n",this,&r);
   MtcsTarget *mtcsTarget = r.mtcsMode->target;
   MtcsMode *mtcsMode = r.mtcsMode;
   // Handle explicit NANs.
   if (m_kind == VR_NAN){
      if (mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,type)){
         if (m_pos_nan && m_neg_nan)
            r.set_nan (type);
         else
            r.set_nan (type, m_neg_nan);
      }else
         r.set_undefined ();
      return;
   }
   if (m_kind == VR_UNDEFINED){
      r.set_undefined ();
      return;
   }

   // We use the constructor to create the new range instead of writing
   // out the bits into the MtcsFrange directly, because the global range
   // being read may be being inlined into a function with different
   // restrictions as when it was originally written.  We want to make
   // sure the resulting range is canonicalized correctly for the new
   // consumer.
   r = MtcsFrange (mtcsMode,type, m_min, m_max, m_kind);

   // The constructor will set the NAN bits for HONOR_NANS, but we must
   // make sure to set the NAN sign if known.
   if (mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,type) && (m_pos_nan ^ m_neg_nan) == 1)
      r.update_nan (m_neg_nan);
   else if (!m_pos_nan && !m_neg_nan)
      r.clear_nan ();
}

bool MtcsFrangeStorage::equal_p (const MtcsFrange &r) const
{
  if (r.undefined_p ())
    return m_kind == VR_UNDEFINED;

  MtcsFrange tmp;
  tmp.mtcsMode = r.mtcsMode;
  get_frange (tmp, r.type ());
  return tmp == r;
}

bool MtcsFrangeStorage::fits_p (const MtcsFrange &) const
{
  return true;
}

