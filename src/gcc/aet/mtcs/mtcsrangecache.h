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

#ifndef __GCC_MTCS_RANGE_CACHE_H__
#define __GCC_MTCS_RANGE_CACHE_H__

#include "gimple-range-gori.h"
#include "gimple-range-infer.h"
#include "gimple-range-phi.h"
#include "mtcscomponent.h"

//原型
// This class manages a vector of pointers to ssa_block ranges.  It
// provides the basis for the "range on entry" cache for all
// SSA names.

class mtcs_block_range_cache
{
public:
   mtcs_block_range_cache ();
  ~mtcs_block_range_cache ();

  bool set_bb_range (tree name, const_basic_block bb, const vrange &v);
  bool get_bb_range (vrange &v, tree name, const_basic_block bb);
  bool bb_range_p (tree name, const_basic_block bb);

  void dump (FILE *f);
  void dump (FILE *f, basic_block bb, bool print_varying = true);
  void setMtcsMode(MtcsMode *mtcsMode);
  MtcsMode *getMtcsMode();

private:
  vec<class mtcs_ssa_block_ranges *> m_ssa_ranges;
  mtcs_ssa_block_ranges &get_block_ranges (tree name);
  mtcs_ssa_block_ranges *query_block_ranges (tree name);
  class vrange_allocator *m_range_allocator;
  bitmap_obstack m_bitmaps;
  MtcsMode *mtcsMode;
};

// This global cache is used with the range engine as markers for what
// has been visited during this incarnation.  Once the ranger evaluates
// a name, it is typically not re-evaluated again.

class mtcs_ssa_cache : public range_query
{
public:
   mtcs_ssa_cache (MtcsMode *mtcsMode);
  ~mtcs_ssa_cache ();
  virtual bool has_range (tree name) const;
  virtual bool get_range (vrange &r, tree name) const;
  virtual bool set_range (tree name, const vrange &r);
  virtual bool merge_range (tree name, const vrange &r);
  virtual void clear_range (tree name);
  virtual void clear ();
  void dump (FILE *f = stderr);
  virtual bool range_of_expr (vrange &r, tree expr, gimple *stmt = NULL);
protected:
  vec<vrange_storage *> m_tab;
  vrange_allocator *m_range_allocator;
};

// This is the same as global cache, except it maintains an active bitmap
// rather than depending on a zero'd out vector of pointers.  This is better
// for sparsely/lightly used caches.

class mtcs_ssa_lazy_cache : public mtcs_ssa_cache
{
public:
   mtcs_ssa_lazy_cache (MtcsMode *mtcsMode,bitmap_obstack *ob = NULL);
  ~mtcs_ssa_lazy_cache ();
  inline bool empty_p () const { return bitmap_empty_p (active_p); }
  virtual bool has_range (tree name) const;
  virtual bool set_range (tree name, const vrange &r);
  virtual bool merge_range (tree name, const vrange &r);
  virtual bool get_range (vrange &r, tree name) const;
  virtual void clear_range (tree name);
  virtual void clear ();
  void merge (const mtcs_ssa_lazy_cache &);
protected:
  bitmap_obstack m_bitmaps;
  bitmap_obstack *m_ob;
  bitmap active_p;
};

// This class provides all the caches a global ranger may need, and makes
// them available for gori-computes to query so outgoing edges can be
// properly calculated.

class mtcs_ranger_cache : public range_query
{
public:
   mtcs_ranger_cache (MtcsMode *mtcsMode,int not_executable_flag, bool use_imm_uses);
  ~mtcs_ranger_cache ();

  bool range_of_expr (vrange &r, tree name, gimple *stmt) final override;
  bool range_on_edge (vrange &r, edge e, tree expr) final override;
  bool block_range (vrange &r, basic_block bb, tree name, bool calc = true);

  bool get_global_range (vrange &r, tree name) const;
  bool get_global_range (vrange &r, tree name, bool &current_p);
  void set_global_range (tree name, const vrange &r, bool changed = true);
  range_query &const_query () { return m_globals; }

  void propagate_updated_value (tree name, basic_block bb);

  void register_inferred_value (const vrange &r, tree name, basic_block bb);
  void apply_inferred_ranges (gimple *s);

  void dump_bb (FILE *f, basic_block bb);
  virtual void dump (FILE *f) override;
private:
  mtcs_ssa_cache m_globals;
  mtcs_block_range_cache m_on_entry;
  class mtcs_temporal_cache *m_temporal;
  void fill_block_cache (tree name, basic_block bb, basic_block def_bb);
  void propagate_cache (tree name);

  enum rfd_mode
    {
      RFD_NONE,		// Only look at current block cache.
      RFD_READ_ONLY,	// Scan DOM tree, do not write to cache.
      RFD_FILL		// Scan DOM tree, updating important nodes.
    };
  bool range_from_dom (vrange &r, tree name, basic_block bb, enum rfd_mode);
  void resolve_dom (vrange &r, tree name, basic_block bb);
  void range_of_def (vrange &r, tree name, basic_block bb = NULL);
  void entry_range (vrange &r, tree expr, basic_block bb, enum rfd_mode);
  void exit_range (vrange &r, tree expr, basic_block bb, enum rfd_mode);
  bool edge_range (vrange &r, edge e, tree name, enum rfd_mode);

  vec<basic_block> m_workback;
  class mtcs_update_list *m_update;
  MtcsMode *mtcsMode;

};



#endif // __GCC_MTCS_GIMPLE_RANGE_CACHE_H__
