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


#ifndef __GCC_MTCS_CSE_LIB__
#define __GCC_MTCS_CSE_LIB__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "alloc-pool.h"
#include "cselib.h"


typedef struct _MtcsCseLib MtcsCseLib;

struct mtcs_cselib_hasher;
//struct elt_list;

struct _MtcsCseLib
{
   MtcsComponent parent;
   bool cselib_record_memory;
   bool cselib_preserve_constants;
   bool cselib_any_perm_equivs;

   /* This is a global so we don't have to pass this through every function.
      It is used in new_elt_loc_list to set SETTING_INSN.  */
   rtx_insn *cselib_current_insn;

   /* A table that enables us to look up elts by their value.  */
   hash_table<mtcs_cselib_hasher> *cselib_hash_table;

   /* A table to hold preserved values.  */
   hash_table<mtcs_cselib_hasher> *cselib_preserved_hash_table;

   /* The unique id that the next create value will take.  */
   unsigned int next_uid;

   /* The number of registers we had when the varrays were last resized.  */
   unsigned int cselib_nregs;



   /* Count values without known locations, or with only locations that
      wouldn't have been known except for debug insns.  Whenever this
      grows too big, we remove these useless values from the table.

      Counting values with only debug values is a bit tricky.  We don't
      want to increment n_useless_values when we create a value for a
      debug insn, for this would get n_useless_values out of sync, but we
      want increment it if all locs in the list that were ever referenced
      in nondebug insns are removed from the list.

      In the general case, once we do that, we'd have to stop accepting
      nondebug expressions in the loc list, to avoid having two values
      equivalent that, without debug insns, would have been made into
      separate values.  However, because debug insns never introduce
      equivalences themselves (no assignments), the only means for
      growing loc lists is through nondebug assignments.  If the locs
      also happen to be referenced in debug insns, it will work just fine.

      A consequence of this is that there's at most one debug-only loc in
      each loc list.  If we keep it in the first entry, testing whether
      we have a debug-only loc list takes O(1).

      Furthermore, since any additional entry in a loc list containing a
      debug loc would have to come from an assignment (nondebug) that
      references both the initial debug loc and the newly-equivalent loc,
      the initial debug loc would be promoted to a nondebug loc, and the
      loc list would not contain debug locs any more.

      So the only case we have to be careful with in order to keep
      n_useless_values in sync between debug and nondebug compilations is
      to avoid incrementing n_useless_values when removing the single loc
      from a value that turns out to not appear outside debug values.  We
      increment n_useless_debug_values instead, and leave such values
      alone until, for other reasons, we garbage-collect useless
      values.  */
   int n_useless_values;
   int n_useless_debug_values;

   /* Count values whose locs have been taken exclusively from debug
      insns for the entire life of the value.  */
   int n_debug_values;

   /* This table maps from register number to values.  It does not
      contain pointers to cselib_val structures, but rather elt_lists.
      The purpose is to be able to refer to the same register in
      different modes.  The first element of the list defines the mode in
      which the register was set; if the mode is unknown or the value is
      no longer valid in that mode, ELT will be NULL for the first
      element.  */
   struct elt_list **reg_values;
   unsigned int reg_values_size;

   /* The largest number of hard regs used by any entry added to the
      REG_VALUES table.  Cleared on each cselib_clear_table() invocation.  */
   unsigned int max_value_regs;

   /* Here the set of indices I with REG_VALUES(I) != 0 is saved.  This is used
      in cselib_clear_table() for fast emptying.  */
   unsigned int *used_regs;
   unsigned int n_used_regs;

   /* We pass this to cselib_invalidate_mem to invalidate all of
      memory for a non-const call instruction.  */
   GTY(()) rtx callmem;

   /* Set by discard_useless_locs if it deleted the last location of any
      value.  */
   int values_became_useless;

   /* Used as stop element of the containing_mem list so we can check
      presence in the list by checking the next pointer.  */
   cselib_val dummy_val;

   /* If non-NULL, value of the eliminated arg_pointer_rtx or frame_pointer_rtx
      that is constant through the whole function and should never be
      eliminated.  */
   cselib_val *cfa_base_preserved_val;
   unsigned int cfa_base_preserved_regno ;//= INVALID_REGNUM;

   /* Used to list all values that contain memory reference.
      May or may not contain the useless values - the list is compacted
      each time memory is invalidated.  */
   cselib_val *first_containing_mem;// = &dummy_val;


   object_allocator<elt_list> elt_list_pool ;//("elt_list");
   object_allocator<elt_loc_list> elt_loc_list_pool ;// ("elt_loc_list");
   object_allocator<cselib_val> cselib_val_pool ;//("cselib_val_list");

   pool_allocator value_pool;// ("value", RTX_CODE_SIZE (VALUE));

   void (*cselib_discard_hook)(cselib_val *);//函数指针
   void (*cselib_record_sets_hook) (rtx_insn *insn, struct cselib_set *sets,int n_sets);

};

MtcsCseLib *mtcs_cse_lib_new(MtcsMode *mtcsMode);
//原型 cselib_clear_table cselib.h cselib.cc
void mtcs_cse_lib_cselib_clear_table (MtcsCseLib *self);
//原型 cselib_reset_table cselib.h cselib.cc
void mtcs_cse_lib_cselib_reset_table (MtcsCseLib *self,unsigned int num);
//原型 cselib_get_next_uid cselib.h cselib.cc
unsigned int mtcs_cse_lib_cselib_get_next_uid (MtcsCseLib *self);
//原型 references_value_p cselib.h cselib.cc
bool mtcs_cse_lib_references_value_p (MtcsCseLib *self,const_rtx x, int only_useless);
//原型 cselib_preserve_value cselib.h cselib.cc
void mtcs_cse_lib_cselib_preserve_value (MtcsCseLib *self,cselib_val *v);
//原型 cselib_preserved_value_p cselib.h cselib.cc
bool mtcs_cse_lib_cselib_preserved_value_p (MtcsCseLib *self,cselib_val *v);
//原型 cselib_preserve_cfa_base_value cselib.h cselib.cc
void mtcs_cse_lib_cselib_preserve_cfa_base_value (MtcsCseLib *self,cselib_val *v, unsigned int regno);
//原型 cselib_preserve_only_values cselib.h cselib.cc
void mtcs_cse_lib_cselib_preserve_only_values (MtcsCseLib *self);
//原型 cselib_set_value_sp_based cselib.h cselib.cc
void mtcs_cse_lib_cselib_set_value_sp_based (MtcsCseLib *self,cselib_val *v);
//原型 cselib_sp_based_value_p cselib.h cselib.cc
bool mtcs_cse_lib_cselib_sp_based_value_p (MtcsCseLib *self,cselib_val *v);
//原型 cselib_reg_set_mode cselib.h cselib.cc
machine_mode mtcs_cse_lib_cselib_reg_set_mode (MtcsCseLib *self,const_rtx x);
//原型 rtx_equal_for_cselib_1 cselib.h cselib.cc
bool mtcs_cse_lib_rtx_equal_for_cselib_1 (MtcsCseLib *self,rtx x, rtx y, machine_mode memmode, int depth);
//原型 cselib_redundant_set_p cselib.h cselib.cc
bool mtcs_cse_lib_cselib_redundant_set_p (MtcsCseLib *self,rtx set);
//原型 rtx_equal_for_cselib_p cselib.h
inline bool mtcs_cse_lib_rtx_equal_for_cselib_p (MtcsCseLib *self,rtx x, rtx y)
{
  if (x == y)
    return true;

  return mtcs_cse_lib_rtx_equal_for_cselib_1/*!rtx_equal_for_cselib_1*/(self,x, y, VOIDmode, 0);
}

//原型 cselib_expand_value_rtx cselib.h cselib.cc
rtx mtcs_cse_lib_cselib_expand_value_rtx (MtcsCseLib *self,rtx orig, bitmap regs_active, int max_depth);
//原型 cselib_expand_value_rtx_cb cselib.h cselib.cc
rtx mtcs_cse_lib_cselib_expand_value_rtx_cb (MtcsCseLib *self,rtx orig, bitmap regs_active, int max_depth,
             cselib_expand_callback cb, void *data);
//原型 cselib_dummy_expand_value_rtx_cb cselib.h cselib.cc
bool mtcs_cse_lib_cselib_dummy_expand_value_rtx_cb (MtcsCseLib *self,rtx orig, bitmap regs_active, int max_depth,
              cselib_expand_callback cb, void *data);
//原型 cselib_subst_to_values cselib.h cselib.cc
rtx mtcs_cse_lib_cselib_subst_to_values (MtcsCseLib *self,rtx x, machine_mode memmode);
//原型 cselib_subst_to_values_from_insn cselib.h cselib.cc
rtx mtcs_cse_lib_cselib_subst_to_values_from_insn (MtcsCseLib *self,rtx x, machine_mode memmode, rtx_insn *insn);
//原型 cselib_lookup_from_insn cselib.h cselib.cc
cselib_val *mtcs_cse_lib_cselib_lookup_from_insn (MtcsCseLib *self,rtx x, machine_mode mode,
          int create, machine_mode memmode, rtx_insn *insn);
//原型 cselib_lookup cselib.h cselib.cc
cselib_val *mtcs_cse_lib_cselib_lookup (MtcsCseLib *self,rtx x, machine_mode mode, int create, machine_mode memmode);
//原型 cselib_invalidate_rtx cselib.h cselib.cc
void mtcs_cse_lib_cselib_invalidate_rtx (MtcsCseLib *self,rtx dest);
//原型 cselib_add_permanent_equiv cselib.h cselib.cc
void mtcs_cselib_cselib_add_permanent_equiv (cselib_val *elt, rtx x, rtx_insn *insn);
//原型 cselib_have_permanent_equivalences cselib.h cselib.cc
bool mtcs_cse_lib_cselib_have_permanent_equivalences (MtcsCseLib *self);
//原型 cselib_record_sp_cfa_base_equiv cselib.h cselib.cc
void mtcs_cse_lib_cselib_record_sp_cfa_base_equiv (MtcsCseLib *self,HOST_WIDE_INT offset, rtx_insn *insn);
//原型 cselib_sp_derived_value_p cselib.h cselib.cc
bool mtcs_cse_lib_cselib_sp_derived_value_p (MtcsCseLib *self,cselib_val *v);
//原型 fp_setter_insn cselib.h cselib.cc
bool mtcs_cse_lib_fp_setter_insn (MtcsCseLib *self,rtx_insn *insn);
//原型 cselib_process_insn cselib.h cselib.cc
void mtcs_cse_lib_cselib_process_insn (MtcsCseLib *self,rtx_insn *insn);
//原型 cselib_init cselib.h cselib.cc
void mtcs_cse_lib_cselib_init (MtcsCseLib *self,int record_what);
//原型 cselib_finish cselib.h cselib.cc
void mtcs_cse_lib_cselib_finish (MtcsCseLib *self);
//原型 dump_cselib_table cselib.h cselib.cc
void mtcs_cse_lib_dump_cselib_table (MtcsCseLib *self,FILE *out);

#endif
