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
 * base on cselib.cc
 */


#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "regs.h"
#include "emit-rtl.h"
#include "dumpfile.h"
#include "cselib.h"
#include "function-abi.h"
#include "alias.h"

#include "aet/aetprinttree.h"
#include "mtcscselib.h"
#include "mtcstarget.h"
#include "mtcsdfcore.h"
#include "mtcsdfproblems.h"
#include "mtcscompile.h"
#include "mtcsprintrtl.h"

/* A list of cselib_val structures.  */
struct elt_list
{
  struct elt_list *next;
  cselib_val *elt;
};


static inline void promote_debug_loc (MtcsCseLib *self,struct elt_loc_list *l);
static struct elt_list *new_elt_list (MtcsCseLib *self,struct elt_list *, cselib_val *);
static void new_elt_loc_list (MtcsCseLib *self,cselib_val *, rtx);
static void unchain_one_value (MtcsCseLib *self,cselib_val *);
static void unchain_one_elt_list (MtcsCseLib *self,struct elt_list **);
static void unchain_one_elt_loc_list (MtcsCseLib *self,struct elt_loc_list **);
static void remove_useless_values (MtcsCseLib *self);
static unsigned int cselib_hash_rtx (MtcsCseLib *self,rtx, int, machine_mode);
static cselib_val *new_cselib_val (MtcsCseLib *self,unsigned int, machine_mode, rtx);
static void add_mem_for_addr (MtcsCseLib *self,cselib_val *, cselib_val *, rtx);
static cselib_val *cselib_lookup_mem (MtcsCseLib *self,rtx, int);
static void cselib_invalidate_regno (MtcsCseLib *self,unsigned int, machine_mode);
static void cselib_invalidate_mem (MtcsCseLib *self,rtx);
static void cselib_record_set (MtcsCseLib *self,rtx, cselib_val *, cselib_val *);
static void cselib_record_sets (MtcsCseLib *self,rtx_insn *);
static rtx autoinc_split (MtcsCseLib *self,rtx, rtx *, machine_mode);

#define PRESERVED_VALUE_P(RTX) \
  (RTL_FLAG_CHECK1 ("PRESERVED_VALUE_P", (RTX), VALUE)->unchanging)

#define SP_BASED_VALUE_P(RTX) \
  (RTL_FLAG_CHECK1 ("SP_BASED_VALUE_P", (RTX), VALUE)->jump)

#define SP_DERIVED_VALUE_P(RTX) \
  (RTL_FLAG_CHECK1 ("SP_DERIVED_VALUE_P", (RTX), VALUE)->call)

struct expand_value_data
{
  bitmap regs_active;
  cselib_expand_callback callback;
  void *callback_arg;
  bool dummy;
};

static rtx cselib_expand_value_rtx_1 (MtcsCseLib *self,rtx, struct expand_value_data *, int);



/* There are three ways in which cselib can look up an rtx:
   - for a REG, the self->reg_values table (which is indexed by regno) is used
   - for a MEM, we recursively look up its address and then follow the
     addr_list of that value
   - for everything else, we compute a hash value and go through the hash
     table.  Since different rtx's can still have the same hash value,
     this involves walking the table entries for a given value and comparing
     the locations of the entries with the rtx we are looking up.  */

struct mtcs_cselib_hasher : nofree_ptr_hash <cselib_val>
{
  struct key {
    /* The rtx value and its mode (needed separately for constant
       integers).  */
    machine_mode mode;
    rtx x;
    /* The mode of the contaning MEM, if any, otherwise VOIDmode.  */
    machine_mode memmode;
  };
  typedef key *compare_type;
  static inline hashval_t hash (const cselib_val *);
  static inline bool equal (const cselib_val *, const key *);
};

/* The hash function for our hash table.  The value is always computed with
   cselib_hash_rtx when adding an element; this function just extracts the
   hash value from a cselib_val structure.  */

inline hashval_t mtcs_cselib_hasher::hash (const cselib_val *v)
{
  return v->hash;
}

/* The equality test for our hash table.  The first argument V is a table
   element (i.e. a cselib_val), while the second arg X is an rtx.  We know
   that all callers of htab_find_slot_with_hash will wrap CONST_INTs into a
   CONST of an appropriate mode.  */

inline bool mtcs_cselib_hasher::equal (const cselib_val *v, const key *x_arg)
{
   MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
   MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
   MtcsCseLib *self =mtcs_target_get_cse_lib(mtcsTarget);

   struct elt_loc_list *l;
   rtx x = x_arg->x;
   machine_mode mode = x_arg->mode;
   machine_mode memmode = x_arg->memmode;

   if (mode != GET_MODE (v->val_rtx))
      return false;

   if (GET_CODE (x) == VALUE)
      return x == v->val_rtx;

   if (SP_DERIVED_VALUE_P (v->val_rtx) && GET_MODE (x) ==mtcs_mode_get_Pmode(mtcsMode)){
      rtx xoff = NULL;
      if (autoinc_split(self,x, &xoff, memmode) == v->val_rtx && xoff == NULL_RTX)
         return true;
   }

   /* We don't guarantee that distinct rtx's have different hash values,
   so we need to do a comparison.  */
   for (l = v->locs; l; l = l->next)
      if (l->setting_insn && DEBUG_INSN_P (l->setting_insn)
      && (!self->cselib_current_insn || !DEBUG_INSN_P (self->cselib_current_insn))){
         rtx_insn *save_cselib_current_insn = self->cselib_current_insn;
         /* If l is so far a debug only loc, without debug stmts it
         would never be compared to x at all, so temporarily pretend
         current instruction is that DEBUG_INSN so that we don't
         promote other debug locs even for unsuccessful comparison.  */
         self->cselib_current_insn = l->setting_insn;
         bool match = mtcs_cse_lib_rtx_equal_for_cselib_1/*!rtx_equal_for_cselib_1*/(self,l->loc, x, memmode, 0);
         self->cselib_current_insn = save_cselib_current_insn;
         if (match){
            promote_debug_loc(self,l);
            return true;
         }
      }else if (mtcs_cse_lib_rtx_equal_for_cselib_1/*!rtx_equal_for_cselib_1*/(self,l->loc, x, memmode, 0))
         return true;

   return false;
}


/* Number of useless values before we remove them from the hash table.  */
#define MAX_USELESS_VALUES 32
#define REG_VALUES(i) self->reg_values[i]


/* Allocate a struct elt_list and fill in its two elements with the
   arguments.  */

static inline struct elt_list *new_elt_list (MtcsCseLib *self,struct elt_list *next, cselib_val *elt)
{
  elt_list *el = self->elt_list_pool.allocate ();
  el->next = next;
  el->elt = elt;
  return el;
}

/* Allocate a struct elt_loc_list with LOC and prepend it to VAL's loc
   list.  */

static inline void new_elt_loc_list (MtcsCseLib *self,cselib_val *val, rtx loc)
{
   struct elt_loc_list *el, *next = val->locs;

   gcc_checking_assert (!next || !next->setting_insn
         || !DEBUG_INSN_P (next->setting_insn)
         || self->cselib_current_insn == next->setting_insn);

   /* If we're creating the first loc in a debug insn context, we've
   just created a debug value.  Count it.  */
   if (!next && self->cselib_current_insn && DEBUG_INSN_P (self->cselib_current_insn))
   self->n_debug_values++;

   val = canonical_cselib_val (val);
   next = val->locs;

   if (GET_CODE (loc) == VALUE){
      loc = canonical_cselib_val (CSELIB_VAL_PTR (loc))->val_rtx;

      gcc_checking_assert (PRESERVED_VALUE_P (loc) == PRESERVED_VALUE_P (val->val_rtx));

      if (val->val_rtx == loc)
         return;
      else if (val->uid > CSELIB_VAL_PTR (loc)->uid){
         /* Reverse the insertion.  */
         new_elt_loc_list(self,CSELIB_VAL_PTR (loc), val->val_rtx);
         return;
      }

      gcc_checking_assert (val->uid < CSELIB_VAL_PTR (loc)->uid);

      if (CSELIB_VAL_PTR (loc)->locs){
         /* Bring all locs from LOC to VAL.  */
         for (el = CSELIB_VAL_PTR (loc)->locs; el->next; el = el->next){
            /* Adjust values that have LOC as canonical so that VAL
            becomes their canonical.  */
            if (el->loc && GET_CODE (el->loc) == VALUE){
               gcc_checking_assert (CSELIB_VAL_PTR (el->loc)->locs->loc == loc);
               CSELIB_VAL_PTR (el->loc)->locs->loc = val->val_rtx;
            }
         }
         el->next = val->locs;
         next = val->locs = CSELIB_VAL_PTR (loc)->locs;
      }

      if (CSELIB_VAL_PTR (loc)->addr_list){
         /* Bring in addr_list into canonical node.  */
         struct elt_list *last = CSELIB_VAL_PTR (loc)->addr_list;
         while (last->next)
            last = last->next;
         last->next = val->addr_list;
         val->addr_list = CSELIB_VAL_PTR (loc)->addr_list;
         CSELIB_VAL_PTR (loc)->addr_list = NULL;
      }

      if (CSELIB_VAL_PTR (loc)->next_containing_mem != NULL  && val->next_containing_mem == NULL){
         /* Add VAL to the containing_mem list after LOC.  LOC will
         be removed when we notice it doesn't contain any
         MEMs.  */
         val->next_containing_mem = CSELIB_VAL_PTR (loc)->next_containing_mem;
         CSELIB_VAL_PTR (loc)->next_containing_mem = val;
      }

      /* Chain LOC back to VAL.  */
      el = self->elt_loc_list_pool.allocate ();
      el->loc = val->val_rtx;
      el->setting_insn = self->cselib_current_insn;
      el->next = NULL;
      CSELIB_VAL_PTR (loc)->locs = el;
   }

   el = self->elt_loc_list_pool.allocate ();
   el->loc = loc;
   el->setting_insn = self->cselib_current_insn;
   el->next = next;
   val->locs = el;
}

/* Promote loc L to a nondebug self->cselib_current_insn if L is marked as
   originating from a debug insn, maintaining the debug values
   count.  */
static inline void promote_debug_loc (MtcsCseLib *self,struct elt_loc_list *l)
{
   if (l && l->setting_insn && DEBUG_INSN_P (l->setting_insn)
   && (!self->cselib_current_insn || !DEBUG_INSN_P (self->cselib_current_insn))){
      self->n_debug_values--;
      l->setting_insn = self->cselib_current_insn;
      if (self->cselib_preserve_constants && l->next){
         gcc_assert (l->next->setting_insn && DEBUG_INSN_P (l->next->setting_insn) && !l->next->next);
         l->next->setting_insn = self->cselib_current_insn;
      }else
         gcc_assert (!l->next);
   }
}

/* The elt_list at *PL is no longer needed.  Unchain it and free its
   storage.  */
static inline void unchain_one_elt_list (MtcsCseLib *self,struct elt_list **pl)
{
   struct elt_list *l = *pl;

   *pl = l->next;
   self->elt_list_pool.remove (l);
}

/* Likewise for elt_loc_lists.  */
static void unchain_one_elt_loc_list (MtcsCseLib *self,struct elt_loc_list **pl)
{
  struct elt_loc_list *l = *pl;

  *pl = l->next;
  self->elt_loc_list_pool.remove (l);
}

/* Likewise for cselib_vals.  This also frees the addr_list associated with
   V.  */
static void unchain_one_value (MtcsCseLib *self,cselib_val *v)
{
   while (v->addr_list)
      unchain_one_elt_list(self,&v->addr_list);

   self->cselib_val_pool.remove (v);
}

/* Remove all entries from the hash table.  Also used during
   initialization.  */
//原型 cselib_clear_table cselib.h cselib.cc
void mtcs_cse_lib_cselib_clear_table (MtcsCseLib *self)
{
   mtcs_cse_lib_cselib_reset_table/*!cselib_reset_table*/(self,1);
}

/* Return TRUE if V is a constant, a function invariant or a VALUE
   equivalence; FALSE otherwise.  */
static bool invariant_or_equiv_p (MtcsCseLib *self,cselib_val *v)
{
   struct elt_loc_list *l;

   if (v == self->cfa_base_preserved_val)
      return true;

   /* Keep VALUE equivalences around.  */
   for (l = v->locs; l; l = l->next)
      if (GET_CODE (l->loc) == VALUE)
         return true;

   if (v->locs != NULL  && v->locs->next == NULL){
      if (CONSTANT_P (v->locs->loc) && (GET_CODE (v->locs->loc) != CONST  || !references_value_p (v->locs->loc, 0)))
         return true;
      /* Although a debug expr may be bound to different expressions,
      we can preserve it as if it was constant, to get unification
      and proper merging within var-tracking.  */
      if (GET_CODE (v->locs->loc) == DEBUG_EXPR
      || GET_CODE (v->locs->loc) == DEBUG_IMPLICIT_PTR
      || GET_CODE (v->locs->loc) == ENTRY_VALUE
      || GET_CODE (v->locs->loc) == DEBUG_PARAMETER_REF)
         return true;

      /* (plus (value V) (const_int C)) is invariant iff V is invariant.  */
      if (GET_CODE (v->locs->loc) == PLUS
      && CONST_INT_P (XEXP (v->locs->loc, 1))
      && GET_CODE (XEXP (v->locs->loc, 0)) == VALUE
      && invariant_or_equiv_p(self,CSELIB_VAL_PTR (XEXP (v->locs->loc, 0))))
         return true;
   }

   return false;
}

/* Remove from hash table all VALUEs except constants, function
   invariants and VALUE equivalences.  */
//原型 int preserve_constants_and_equivs
static int preserveConstantsAndEquivs_cb (cselib_val **x, void *info ATTRIBUTE_UNUSED)
{
   MtcsCseLib *self = (MtcsCseLib *)info;
   cselib_val *v = *x;

   if (invariant_or_equiv_p(self,v)){
      mtcs_cselib_hasher::key lookup = {
            GET_MODE (v->val_rtx), v->val_rtx, VOIDmode
      };
      cselib_val **slot = self->cselib_preserved_hash_table->find_slot_with_hash (&lookup, v->hash, INSERT);
      gcc_assert (!*slot);
      *slot = v;
   }

   self->cselib_hash_table->clear_slot (x);
   return 1;
}

/* Remove all entries from the hash table, arranging for the next
   value to be numbered NUM.  */
//原型 cselib_reset_table cselib.h cselib.cc
void mtcs_cse_lib_cselib_reset_table (MtcsCseLib *self,unsigned int num)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg =mtcs_target_get_reg(mtcsTarget);

   unsigned int i;

   self->max_value_regs = 0;

   if (self->cfa_base_preserved_val){
      unsigned int regno = self->cfa_base_preserved_regno;
      unsigned int new_used_regs = 0;
      for (i = 0; i < self->n_used_regs; i++)
         if (self->used_regs[i] == regno){
            new_used_regs = 1;
            continue;
         }else
            REG_VALUES (self->used_regs[i]) = 0;
      gcc_assert (new_used_regs == 1);
      self->n_used_regs = new_used_regs;
      self->used_regs[0] = regno;
      self->max_value_regs= mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,
            regno, GET_MODE (self->cfa_base_preserved_val->locs->loc));

      /* If cfa_base is sp + const_int, need to preserve also the
      SP_DERIVED_VALUE_P value.  */
      for (struct elt_loc_list *l = self->cfa_base_preserved_val->locs; l; l = l->next)
         if (GET_CODE (l->loc) == PLUS
         && GET_CODE (XEXP (l->loc, 0)) == VALUE
         && SP_DERIVED_VALUE_P (XEXP (l->loc, 0))
         && CONST_INT_P (XEXP (l->loc, 1))){
            if (! invariant_or_equiv_p(self,CSELIB_VAL_PTR (XEXP (l->loc, 0)))){
               rtx val = self->cfa_base_preserved_val->val_rtx;
               rtx_insn *save_cselib_current_insn = self->cselib_current_insn;
               self->cselib_current_insn = l->setting_insn;
               new_elt_loc_list(self,CSELIB_VAL_PTR (XEXP (l->loc, 0)),
               mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,mtcs_mode_get_Pmode(mtcsMode), val, -UINTVAL (XEXP (l->loc, 1))));
               self->cselib_current_insn = save_cselib_current_insn;
            }
            break;
         }
   }else{
      for (i = 0; i < self->n_used_regs; i++)
         REG_VALUES (self->used_regs[i]) = 0;
      self->n_used_regs = 0;
   }

   if (self->cselib_preserve_constants)
      self->cselib_hash_table->traverse <void *, preserveConstantsAndEquivs_cb> (self/*!NULL*/);
   else{
      self->cselib_hash_table->empty ();
      gcc_checking_assert (!self->cselib_any_perm_equivs);
   }

   self->n_useless_values = 0;
   self->n_useless_debug_values = 0;
   self->n_debug_values = 0;

   self->next_uid = num;

   self->first_containing_mem = &self->dummy_val;
}

/* Return the number of the next value that will be generated.  */
//原型 cselib_get_next_uid cselib.h cselib.cc
unsigned int mtcs_cse_lib_cselib_get_next_uid (MtcsCseLib *self)
{
  return self->next_uid;
}

/* Search for X, whose hashcode is HASH, in CSELIB_HASH_TABLE,
   INSERTing if requested.  When X is part of the address of a MEM,
   MEMMODE should specify the mode of the MEM.  */
static cselib_val ** cselib_find_slot (MtcsCseLib *self,machine_mode mode, rtx x, hashval_t hash,
        enum insert_option insert, machine_mode memmode)
{
   cselib_val **slot = NULL;
   mtcs_cselib_hasher::key lookup = { mode, x, memmode };
   if (self->cselib_preserve_constants)
      slot = self->cselib_preserved_hash_table->find_slot_with_hash (&lookup, hash, NO_INSERT);
   if (!slot)
      slot = self->cselib_hash_table->find_slot_with_hash (&lookup, hash, insert);
   return slot;
}

/* Return true if X contains a VALUE rtx.  If ONLY_USELESS is set, we
   only return true for values which point to a cselib_val whose value
   element has been set to zero, which implies the cselib_val will be
   removed.  */
//原型 references_value_p cselib.h cselib.cc
bool mtcs_cse_lib_references_value_p (MtcsCseLib *self,const_rtx x, int only_useless)
{
   const enum rtx_code code = GET_CODE (x);
   const char *fmt = GET_RTX_FORMAT (code);
   int i, j;

   if (GET_CODE (x) == VALUE && (! only_useless || (CSELIB_VAL_PTR (x)->locs == 0 && !PRESERVED_VALUE_P (x))))
      return true;

   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--){
      if (fmt[i] == 'e' && mtcs_cse_lib_references_value_p/*!references_value_p*/(self,XEXP (x, i), only_useless))
         return true;
      else if (fmt[i] == 'E')
         for (j = 0; j < XVECLEN (x, i); j++)
            if (mtcs_cse_lib_references_value_p/*!references_value_p*/(self,XVECEXP (x, i, j), only_useless))
      return true;
   }

   return false;
}

/* Return true if V is a useless VALUE and can be discarded as such.  */
static bool cselib_useless_value_p (cselib_val *v)
{
   return (v->locs == 0  && !PRESERVED_VALUE_P (v->val_rtx)  && !SP_DERIVED_VALUE_P (v->val_rtx));
}

/* For all locations found in X, delete locations that reference useless
   values (i.e. values without any location).  Called through
   htab_traverse.  */
//原型 int discard_useless_locs 回调函数
static int discardUselessLocs_cb  (cselib_val **x, void *info ATTRIBUTE_UNUSED)
{
   MtcsCseLib *self=(MtcsCseLib *)info;
   cselib_val *v = *x;
   struct elt_loc_list **p = &v->locs;
   bool had_locs = v->locs != NULL;
   rtx_insn *setting_insn = v->locs ? v->locs->setting_insn : NULL;

   while (*p){
      if (mtcs_cse_lib_references_value_p/*!references_value_p*/(self,(*p)->loc, 1))
         unchain_one_elt_loc_list(self,p);
      else
         p = &(*p)->next;
   }

   if (had_locs && cselib_useless_value_p (v)){
      if (setting_insn && DEBUG_INSN_P (setting_insn))
         self->n_useless_debug_values++;
      else
         self->n_useless_values++;
      self->values_became_useless = 1;
   }
   return 1;
}

/* If X is a value with no locations, remove it from the hashtable.  */
//原型 int discard_useless_values 回调函数
static int discardUselessValues_cb (cselib_val **x, void *info ATTRIBUTE_UNUSED)
{
   MtcsCseLib *self=(MtcsCseLib *)info;
   cselib_val *v = *x;
   if (v->locs == 0 && cselib_useless_value_p (v)){
      if (self->cselib_discard_hook/*!cselib_discard_hook*/)
         self->cselib_discard_hook(v)/*!cselib_discard_hook (v)*/;

      CSELIB_VAL_PTR (v->val_rtx) = NULL;
      self->cselib_hash_table->clear_slot (x);
      unchain_one_value(self,v);
      self->n_useless_values--;
   }
   return 1;
}

/* Clean out useless values (i.e. those which no longer have locations
   associated with them) from the hash table.  */
static void remove_useless_values (MtcsCseLib *self)
{
   cselib_val **p, *v;
   /* First pass: eliminate locations that reference the value.  That in
   turn can make more values useless.  */
   do{
      self->values_became_useless = 0;
      self->cselib_hash_table->traverse <void *, discardUselessLocs_cb> (self);
   }while (self->values_became_useless);

   /* Second pass: actually remove the values.  */
   p = &self->first_containing_mem;
   for (v = *p; v != &self->dummy_val; v = v->next_containing_mem)
      if (v->locs && v == canonical_cselib_val (v)){
         *p = v;
         p = &(*p)->next_containing_mem;
      }
   *p = &self->dummy_val;

   self->n_useless_values += self->n_useless_debug_values;
   self->n_debug_values -= self->n_useless_debug_values;
   self->n_useless_debug_values = 0;

   self->cselib_hash_table->traverse <void *, discardUselessValues_cb> (self);
   gcc_assert (!self->n_useless_values);
}

/* Arrange for a value to not be removed from the hash table even if
   it becomes useless.  */
//原型 cselib_preserve_value cselib.h cselib.cc
void mtcs_cse_lib_cselib_preserve_value (MtcsCseLib *self,cselib_val *v)
{
  PRESERVED_VALUE_P (v->val_rtx) = 1;
}

/* Test whether a value is preserved.  */
//原型 cselib_preserved_value_p cselib.h cselib.cc
bool mtcs_cse_lib_cselib_preserved_value_p (MtcsCseLib *self,cselib_val *v)
{
  return PRESERVED_VALUE_P (v->val_rtx);
}

/* Arrange for a REG value to be assumed constant through the whole function,
   never invalidated and preserved across cselib_reset_table calls.  */
//原型 cselib_preserve_cfa_base_value cselib.h cselib.cc
void mtcs_cse_lib_cselib_preserve_cfa_base_value (MtcsCseLib *self,cselib_val *v, unsigned int regno)
{
   if (self->cselib_preserve_constants  && v->locs  && REG_P (v->locs->loc)){
      self->cfa_base_preserved_val = v;
      self->cfa_base_preserved_regno = regno;
   }
}

/* Clean all non-constant expressions in the hash table, but retain
   their values.  */
//原型 cselib_preserve_only_values cselib.h cselib.cc
void mtcs_cse_lib_cselib_preserve_only_values (MtcsCseLib *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg =mtcs_target_get_reg(mtcsTarget);

   int i;
   for (i = 0; i < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i++)
      cselib_invalidate_regno(self,i, mtcsReg->hardRegs.x_reg_raw_mode/*!reg_raw_mode*/[i]);

   cselib_invalidate_mem(self,self->callmem);
   remove_useless_values(self);
   gcc_assert (self->first_containing_mem == &self->dummy_val);
}

/* Arrange for a value to be marked as based on stack pointer
   for find_base_term purposes.  */
//原型 cselib_set_value_sp_based cselib.h cselib.cc
void mtcs_cse_lib_cselib_set_value_sp_based (MtcsCseLib *self,cselib_val *v)
{
   SP_BASED_VALUE_P (v->val_rtx) = 1;
}

/* Test whether a value is based on stack pointer for
   find_base_term purposes.  */
//原型 cselib_sp_based_value_p cselib.h cselib.cc
bool mtcs_cse_lib_cselib_sp_based_value_p (MtcsCseLib *self,cselib_val *v)
{
  return SP_BASED_VALUE_P (v->val_rtx);
}

/* Return the mode in which a register was last set.  If X is not a
   register, return its mode.  If the mode in which the register was
   set is not known, or the value was already clobbered, return
   VOIDmode.  */
//原型 cselib_reg_set_mode cselib.h cselib.cc
machine_mode mtcs_cse_lib_cselib_reg_set_mode (MtcsCseLib *self,const_rtx x)
{
   if (!REG_P (x))
      return GET_MODE (x);

   if (REG_VALUES (REGNO (x)) == NULL || REG_VALUES (REGNO (x))->elt == NULL)
      return VOIDmode;

   return GET_MODE (REG_VALUES (REGNO (x))->elt->val_rtx);
}

/* If x is a PLUS or an autoinc operation, expand the operation,
   storing the offset, if any, in *OFF.  */
static rtx autoinc_split (MtcsCseLib *self,rtx x, rtx *off, machine_mode memmode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg =mtcs_target_get_reg(mtcsTarget);

   switch (GET_CODE (x)){
      case PLUS:
         *off = XEXP (x, 1);
         x = XEXP (x, 0);
         break;

      case PRE_DEC:
         if (memmode == VOIDmode)
            return x;

         *off = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,-mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,memmode), GET_MODE (x));
         x = XEXP (x, 0);
         break;

      case PRE_INC:
         if (memmode == VOIDmode)
            return x;

         *off = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,memmode), GET_MODE (x));
         x = XEXP (x, 0);
         break;

      case PRE_MODIFY:
         x = XEXP (x, 1);
         break;

      case POST_DEC:
      case POST_INC:
      case POST_MODIFY:
         x = XEXP (x, 0);
         break;

      default:
         break;
   }

   if (GET_MODE (x) == Pmode
   && (REG_P (x) || MEM_P (x) || GET_CODE (x) == VALUE)
   && (*off == NULL_RTX || CONST_INT_P (*off))){
      cselib_val *e;
      if (GET_CODE (x) == VALUE)
         e = CSELIB_VAL_PTR (x);
      else
         e = mtcs_cse_lib_cselib_lookup/*!cselib_lookup*/(self,x, GET_MODE (x), 0, memmode);
      if (e){
         if (SP_DERIVED_VALUE_P (e->val_rtx) && (*off == NULL_RTX || *off == const0_rtx)){
            *off = NULL_RTX;
            return e->val_rtx;
         }
         for (struct elt_loc_list *l = e->locs; l; l = l->next)
            if (GET_CODE (l->loc) == PLUS
            && GET_CODE (XEXP (l->loc, 0)) == VALUE
            && SP_DERIVED_VALUE_P (XEXP (l->loc, 0))
            && CONST_INT_P (XEXP (l->loc, 1))){
               if (*off == NULL_RTX)
                  *off = XEXP (l->loc, 1);
               else
                  *off = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,mtcs_mode_get_Pmode(mtcsMode), *off,INTVAL (XEXP (l->loc, 1)));
               if (*off == const0_rtx)
                  *off = NULL_RTX;
               return XEXP (l->loc, 0);
            }
      }
   }
   return x;
}

/* Return true if we can prove that X and Y contain the same value,
   taking our gathered information into account.  MEMMODE holds the
   mode of the enclosing MEM, if any, as required to deal with autoinc
   addressing modes.  If X and Y are not (known to be) part of
   addresses, MEMMODE should be VOIDmode.  */
//原型 rtx_equal_for_cselib_1 cselib.h cselib.cc
bool mtcs_cse_lib_rtx_equal_for_cselib_1 (MtcsCseLib *self,rtx x, rtx y, machine_mode memmode, int depth)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
   enum rtx_code code;
   const char *fmt;
   int i;

   if (REG_P (x) || MEM_P (x)){
      cselib_val *e = mtcs_cse_lib_cselib_lookup/*!cselib_lookup*/(self,x, GET_MODE (x), 0, memmode);

      if (e)
         x = e->val_rtx;
   }

   if (REG_P (y) || MEM_P (y)){
      cselib_val *e = mtcs_cse_lib_cselib_lookup/*!cselib_lookup*/(self,y, GET_MODE (y), 0, memmode);

      if (e)
         y = e->val_rtx;
   }

   if (x == y)
      return true;

   if (GET_CODE (x) == VALUE){
      cselib_val *e = canonical_cselib_val (CSELIB_VAL_PTR (x));
      struct elt_loc_list *l;

      if (GET_CODE (y) == VALUE)
         return e == canonical_cselib_val (CSELIB_VAL_PTR (y));

      if ((SP_DERIVED_VALUE_P (x) || SP_DERIVED_VALUE_P (e->val_rtx)) && GET_MODE (y) == pMode){
         rtx yoff = NULL;
         rtx yr = autoinc_split(self,y, &yoff, memmode);
         if ((yr == x || yr == e->val_rtx) && yoff == NULL_RTX)
            return true;
      }

      if (depth == 128)
         return false;

      for (l = e->locs; l; l = l->next){
         rtx t = l->loc;

         /* Avoid infinite recursion.  We know we have the canonical
         value, so we can just skip any values in the equivalence
         list.  */
         if (REG_P (t) || MEM_P (t) || GET_CODE (t) == VALUE)
            continue;
         else if (mtcs_cse_lib_rtx_equal_for_cselib_1/*!rtx_equal_for_cselib_1*/(self,t, y, memmode, depth + 1))
            return true;
      }

      return false;
   }else if (GET_CODE (y) == VALUE){
      cselib_val *e = canonical_cselib_val (CSELIB_VAL_PTR (y));
      struct elt_loc_list *l;

      if ((SP_DERIVED_VALUE_P (y) || SP_DERIVED_VALUE_P (e->val_rtx))  && GET_MODE (x) == pMode){
         rtx xoff = NULL;
         rtx xr = autoinc_split(self,x, &xoff, memmode);
         if ((xr == y || xr == e->val_rtx) && xoff == NULL_RTX)
            return true;
      }

      if (depth == 128)
         return false;

      for (l = e->locs; l; l = l->next){
         rtx t = l->loc;

         if (REG_P (t) || MEM_P (t) || GET_CODE (t) == VALUE)
            continue;
         else if (mtcs_cse_lib_rtx_equal_for_cselib_1/*!rtx_equal_for_cselib_1*/(self,x, t, memmode, depth + 1))
            return true;
      }

      return false;
   }

   if (GET_MODE (x) != GET_MODE (y))
      return false;

   if (GET_CODE (x) != GET_CODE (y)  || (GET_CODE (x) == PLUS
   && GET_MODE (x) == Pmode  && CONST_INT_P (XEXP (x, 1))  && CONST_INT_P (XEXP (y, 1)))) {
      rtx xorig = x, yorig = y;
      rtx xoff = NULL, yoff = NULL;

      x = autoinc_split(self,x, &xoff, memmode);
      y = autoinc_split(self,y, &yoff, memmode);
      /* Don't recurse if nothing changed.  */
      if (x != xorig || y != yorig){
         if (!xoff != !yoff)
            return false;

         if (xoff && !mtcs_cse_lib_rtx_equal_for_cselib_1/*!rtx_equal_for_cselib_1*/(self,xoff, yoff, memmode, depth))
            return false;

         return mtcs_cse_lib_rtx_equal_for_cselib_1/*!rtx_equal_for_cselib_1*/(self,x, y, memmode, depth);
      }
      if (GET_CODE (xorig) != GET_CODE (yorig))
         return false;
   }

   /* These won't be handled correctly by the code below.  */
   switch (GET_CODE (x)){
      CASE_CONST_UNIQUE:
      case DEBUG_EXPR:
         return false;

      case CONST_VECTOR:
         if (!same_vector_encodings_p (x, y))
            return false;
         break;

      case DEBUG_IMPLICIT_PTR:
         return DEBUG_IMPLICIT_PTR_DECL (x) == DEBUG_IMPLICIT_PTR_DECL (y);

      case DEBUG_PARAMETER_REF:
         return DEBUG_PARAMETER_REF_DECL (x) == DEBUG_PARAMETER_REF_DECL (y);

      case ENTRY_VALUE:
         /* ENTRY_VALUEs are function invariant, it is thus undesirable to
         use rtx_equal_for_cselib_1 to compare the operands.  */
         return rtx_equal_p (ENTRY_VALUE_EXP (x), ENTRY_VALUE_EXP (y));

      case LABEL_REF:
         return label_ref_label (x) == label_ref_label (y);

      case REG:
         return REGNO (x) == REGNO (y);

      case MEM:
         /* We have to compare any autoinc operations in the addresses
         using this MEM's mode.  */
         return mtcs_cse_lib_rtx_equal_for_cselib_1/*!rtx_equal_for_cselib_1*/(self,
               XEXP (x, 0), XEXP (y, 0), GET_MODE (x),depth);
      default:
         break;
   }

   code = GET_CODE (x);
   fmt = GET_RTX_FORMAT (code);

   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--){
      int j;

      switch (fmt[i]){
         case 'w':
            if (XWINT (x, i) != XWINT (y, i))
               return false;
            break;

         case 'n':
         case 'i':
            if (XINT (x, i) != XINT (y, i))
               return false;
            break;

         case 'p':
            if (maybe_ne (SUBREG_BYTE (x), SUBREG_BYTE (y)))
               return false;
            break;

         case 'V':
         case 'E':
            /* Two vectors must have the same length.  */
            if (XVECLEN (x, i) != XVECLEN (y, i))
               return false;

            /* And the corresponding elements must match.  */
            for (j = 0; j < XVECLEN (x, i); j++)
               if (! mtcs_cse_lib_rtx_equal_for_cselib_1/*!rtx_equal_for_cselib_1*/(self,
                     XVECEXP (x, i, j), XVECEXP (y, i, j), memmode, depth))
                  return false;
            break;

         case 'e':
            if (i == 1
            && targetm.commutative_p (x, UNKNOWN)
            && mtcs_cse_lib_rtx_equal_for_cselib_1/*!rtx_equal_for_cselib_1*/(self,XEXP (x, 1), XEXP (y, 0), memmode,depth)
            && mtcs_cse_lib_rtx_equal_for_cselib_1/*!rtx_equal_for_cselib_1*/(self,XEXP (x, 0), XEXP (y, 1), memmode,depth))
               return true;
            if (! mtcs_cse_lib_rtx_equal_for_cselib_1/*!rtx_equal_for_cselib_1*/(self,XEXP (x, i), XEXP (y, i), memmode, depth))
               return false;
            break;

         case 'S':
         case 's':
            if (strcmp (XSTR (x, i), XSTR (y, i)))
               return false;
            break;

         case 'u':
            /* These are just backpointers, so they don't matter.  */
            break;

         case '0':
         case 't':
            break;
         /* It is believed that rtx's at this level will never
         contain anything but integers and other rtx's,
         except for within LABEL_REFs and SYMBOL_REFs.  */
         default:
            gcc_unreachable ();
      }
   }
   return true;
}

/* Wrapper for rtx_equal_for_cselib_p to determine whether a SET is
   truly redundant, taking into account aliasing information.  */
//原型 cselib_redundant_set_p cselib.h cselib.cc
bool mtcs_cse_lib_cselib_redundant_set_p (MtcsCseLib *self,rtx set)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   gcc_assert (GET_CODE (set) == SET);
   rtx dest = SET_DEST (set);
   if (mtcs_cse_lib_cselib_reg_set_mode/*!cselib_reg_set_mode*/(self,dest) != GET_MODE (dest))
      return false;

   if (!mtcs_cse_lib_rtx_equal_for_cselib_p/*!rtx_equal_for_cselib_p*/(self,dest, SET_SRC (set)))
   return false;

   while (GET_CODE (dest) == SUBREG || GET_CODE (dest) == ZERO_EXTRACT || GET_CODE (dest) == STRICT_LOW_PART)
      dest = XEXP (dest, 0);

   if (!mtcsOptionsItem->x_flag_strict_aliasing || !MEM_P (dest))
      return true;

   /* For a store we need to check that suppressing it will not change
   the effective alias set.  */
   rtx dest_addr = XEXP (dest, 0);

   /* Lookup the equivalents to the original dest (rather than just the
   MEM).  */
   cselib_val *src_val = mtcs_cse_lib_cselib_lookup/*!cselib_lookup*/(self,SET_DEST (set),GET_MODE (SET_DEST (set)),0, VOIDmode);

   if (src_val){
      /* Walk the list of source equivalents to find the MEM accessing
      the same location.  */
      for (elt_loc_list *l = src_val->locs; l; l = l->next){
         rtx src_equiv = l->loc;
         while (GET_CODE (src_equiv) == SUBREG || GET_CODE (src_equiv) == ZERO_EXTRACT || GET_CODE (src_equiv) == STRICT_LOW_PART)
            src_equiv = XEXP (src_equiv, 0);

         if (MEM_P (src_equiv)){
            /* Match the MEMs by comparing the addresses.  We can
            only remove the later store if the earlier aliases at
            least all the accesses of the later one.  */
            if (mtcs_cse_lib_rtx_equal_for_cselib_1/*!rtx_equal_for_cselib_1*/(self,dest_addr, XEXP (src_equiv, 0), GET_MODE (dest), 0))
               return mtcs_alias_mems_same_for_tbaa_p/*!mems_same_for_tbaa_p*/(mtcsAlias,src_equiv, dest);
         }
      }
   }

   /* We failed to find a recorded value in the cselib history, so try
   the source of this set; this catches cases such as *p = *q when p
   and q have the same value.  */
   rtx src = SET_SRC (set);
   while (GET_CODE (src) == SUBREG)
      src = XEXP (src, 0);

   if (MEM_P (src)
   && mtcs_cse_lib_rtx_equal_for_cselib_1/*!rtx_equal_for_cselib_1*/(self,dest_addr, XEXP (src, 0), GET_MODE (dest), 0))
      return mtcs_alias_mems_same_for_tbaa_p/*!mems_same_for_tbaa_p*/(mtcsAlias,src, dest);

   return false;
}

/* Helper function for cselib_hash_rtx.  Arguments like for cselib_hash_rtx,
   except that it hashes (plus:P x c).  */
static unsigned int cselib_hash_plus_const_int (MtcsCseLib *self,rtx x, HOST_WIDE_INT c, int create,
             machine_mode memmode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   cselib_val *e = mtcs_cse_lib_cselib_lookup/*!cselib_lookup*/(self,x, GET_MODE (x), create, memmode);
   if (! e)
      return 0;

   if (! SP_DERIVED_VALUE_P (e->val_rtx))
      for (struct elt_loc_list *l = e->locs; l; l = l->next)
         if (GET_CODE (l->loc) == PLUS  && GET_CODE (XEXP (l->loc, 0)) == VALUE
         && SP_DERIVED_VALUE_P (XEXP (l->loc, 0))  && CONST_INT_P (XEXP (l->loc, 1))){
            e = CSELIB_VAL_PTR (XEXP (l->loc, 0));
            c = mtcs_mode_trunc_int_for_mode/*!trunc_int_for_mode*/(mtcsMode,
                  c + UINTVAL (XEXP (l->loc, 1)),mtcs_mode_get_Pmode(mtcsMode));
            break;
         }
   if (c == 0)
      return e->hash;

   unsigned hash = (unsigned) PLUS + (unsigned) GET_MODE (x);
   hash += e->hash;
   unsigned int tem_hash = (unsigned) CONST_INT + (unsigned) VOIDmode;
   tem_hash += ((unsigned) CONST_INT << 7) + (unsigned HOST_WIDE_INT) c;
   if (tem_hash == 0)
   tem_hash = (unsigned int) CONST_INT;
   hash += tem_hash;
   return hash ? hash : 1 + (unsigned int) PLUS;
}

/* Hash an rtx.  Return 0 if we couldn't hash the rtx.
   For registers and memory locations, we look up their cselib_val structure
   and return its VALUE element.
   Possible reasons for return 0 are: the object is volatile, or we couldn't
   find a register or memory location in the table and CREATE is zero.  If
   CREATE is nonzero, table elts are created for regs and mem.
   N.B. this hash function returns the same hash value for RTXes that
   differ only in the order of operands, thus it is suitable for comparisons
   that take commutativity into account.
   If we wanted to also support associative rules, we'd have to use a different
   strategy to avoid returning spurious 0, e.g. return ~(~0U >> 1) .
   MEMMODE indicates the mode of an enclosing MEM, and it's only
   used to compute autoinc values.
   We used to have a MODE argument for hashing for CONST_INTs, but that
   didn't make sense, since it caused spurious hash differences between
    (set (reg:SI 1) (const_int))
    (plus:SI (reg:SI 2) (reg:SI 1))
   and
    (plus:SI (reg:SI 2) (const_int))
   If the mode is important in any context, it must be checked specifically
   in a comparison anyway, since relying on hash differences is unsafe.  */
static unsigned int cselib_hash_rtx (MtcsCseLib *self,rtx x, int create, machine_mode memmode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg =mtcs_target_get_reg(mtcsTarget);

   cselib_val *e;
   poly_int64 offset;
   int i, j;
   enum rtx_code code;
   const char *fmt;
   unsigned int hash = 0;

   code = GET_CODE (x);
   hash += (unsigned) code + (unsigned) GET_MODE (x);

   switch (code){
      case VALUE:
         e = CSELIB_VAL_PTR (x);
         return e->hash;

      case MEM:
      case REG:
         e = mtcs_cse_lib_cselib_lookup/*!cselib_lookup*/(self,x, GET_MODE (x), create, memmode);
         if (! e)
            return 0;

         return e->hash;

      case DEBUG_EXPR:
         hash += ((unsigned) DEBUG_EXPR << 7) + DEBUG_TEMP_UID (DEBUG_EXPR_TREE_DECL (x));
         return hash ? hash : (unsigned int) DEBUG_EXPR;

      case DEBUG_IMPLICIT_PTR:
         hash += ((unsigned) DEBUG_IMPLICIT_PTR << 7) + DECL_UID (DEBUG_IMPLICIT_PTR_DECL (x));
         return hash ? hash : (unsigned int) DEBUG_IMPLICIT_PTR;

      case DEBUG_PARAMETER_REF:
         hash += ((unsigned) DEBUG_PARAMETER_REF << 7) + DECL_UID (DEBUG_PARAMETER_REF_DECL (x));
         return hash ? hash : (unsigned int) DEBUG_PARAMETER_REF;

      case ENTRY_VALUE:
         /* ENTRY_VALUEs are function invariant, thus try to avoid
         recursing on argument if ENTRY_VALUE is one of the
         forms emitted by expand_debug_expr, otherwise
         ENTRY_VALUE hash would depend on the current value
         in some register or memory.  */
         if (REG_P (ENTRY_VALUE_EXP (x)))
            hash += (unsigned int) REG + (unsigned int) GET_MODE (ENTRY_VALUE_EXP (x))
                          + (unsigned int) REGNO (ENTRY_VALUE_EXP (x));
         else if (MEM_P (ENTRY_VALUE_EXP (x))  && REG_P (XEXP (ENTRY_VALUE_EXP (x), 0)))
            hash += (unsigned int) MEM  + (unsigned int) GET_MODE (XEXP (ENTRY_VALUE_EXP (x), 0))
                     + (unsigned int) REGNO (XEXP (ENTRY_VALUE_EXP (x), 0));
         else
            hash += cselib_hash_rtx(self,ENTRY_VALUE_EXP (x), create, memmode);
         return hash ? hash : (unsigned int) ENTRY_VALUE;

      case CONST_INT:
         hash += ((unsigned) CONST_INT << 7) + UINTVAL (x);
         return hash ? hash : (unsigned int) CONST_INT;

      case CONST_WIDE_INT:
         for (i = 0; i < CONST_WIDE_INT_NUNITS (x); i++)
            hash += CONST_WIDE_INT_ELT (x, i);
         return hash;

      case CONST_POLY_INT:
      {
         inchash::hash h;
         h.add_int (hash);
         for (unsigned int i = 0; i < NUM_POLY_INT_COEFFS; ++i)
            h.add_wide_int (CONST_POLY_INT_COEFFS (x)[i]);
         return h.end ();
      }

      case CONST_DOUBLE:
         /* This is like the general case, except that it only counts
         the integers representing the constant.  */
         hash += (unsigned) code + (unsigned) GET_MODE (x);
         if (TARGET_SUPPORTS_WIDE_INT == 0 && GET_MODE (x) == VOIDmode)
            hash += ((unsigned) CONST_DOUBLE_LOW (x)  + (unsigned) CONST_DOUBLE_HIGH (x));
         else
            hash += real_hash (CONST_DOUBLE_REAL_VALUE (x));
         return hash ? hash : (unsigned int) CONST_DOUBLE;

      case CONST_FIXED:
         hash += (unsigned int) code + (unsigned int) GET_MODE (x);
         hash += fixed_hash (CONST_FIXED_VALUE (x));
         return hash ? hash : (unsigned int) CONST_FIXED;

      case CONST_VECTOR:
      {
         int units;
         rtx elt;
         units = const_vector_encoded_nelts (x);
         for (i = 0; i < units; ++i){
            elt = CONST_VECTOR_ENCODED_ELT (x, i);
            hash += cselib_hash_rtx(self,elt, 0, memmode);
         }
         return hash;
      }
      /* Assume there is only one rtx object for any given label.  */
      case LABEL_REF:
         /* We don't hash on the address of the CODE_LABEL to avoid bootstrap
         differences and differences between each stage's debugging dumps.  */
         hash += (((unsigned int) LABEL_REF << 7) + CODE_LABEL_NUMBER (label_ref_label (x)));
         return hash ? hash : (unsigned int) LABEL_REF;

      case SYMBOL_REF:
      {
         /* Don't hash on the symbol's address to avoid bootstrap differences.
         Different hash values may cause expressions to be recorded in
         different orders and thus different registers to be used in the
         final assembler.  This also avoids differences in the dump files
         between various stages.  */
         unsigned int h = 0;
         const unsigned char *p = (const unsigned char *) XSTR (x, 0);
         while (*p)
            h += (h << 7) + *p++; /* ??? revisit */
         hash += ((unsigned int) SYMBOL_REF << 7) + h;
         return hash ? hash : (unsigned int) SYMBOL_REF;
      }

      case PRE_DEC:
      case PRE_INC:
         /* We can't compute these without knowing the MEM mode.  */
         gcc_assert (memmode != VOIDmode);
         offset = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,memmode);
         if (code == PRE_DEC)
            offset = -offset;
         /* Adjust the hash so that (mem:MEMMODE (pre_* (reg))) hashes
         like (mem:MEMMODE (plus (reg) (const_int I))).  */
         if (GET_MODE (x) == Pmode  && (REG_P (XEXP (x, 0))
         || MEM_P (XEXP (x, 0))  || GET_CODE (XEXP (x, 0)) == VALUE)){
            HOST_WIDE_INT c;
            if (offset.is_constant (&c))
               return cselib_hash_plus_const_int(self,XEXP (x, 0),
            mtcs_mode_trunc_int_for_mode/*!trunc_int_for_mode*/(mtcsMode,c, mtcs_mode_get_Pmode(mtcsMode)),create, memmode);
         }
         hash = ((unsigned) PLUS + (unsigned) GET_MODE (x)
            + cselib_hash_rtx(self,XEXP (x, 0), create, memmode)
            + cselib_hash_rtx(self,mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,offset, GET_MODE (x)),create, memmode));
         return hash ? hash : 1 + (unsigned) PLUS;

      case PRE_MODIFY:
         gcc_assert (memmode != VOIDmode);
         return cselib_hash_rtx(self,XEXP (x, 1), create, memmode);
      case POST_DEC:
      case POST_INC:
      case POST_MODIFY:
         gcc_assert (memmode != VOIDmode);
         return cselib_hash_rtx(self,XEXP (x, 0), create, memmode);
      case PC:
      case CALL:
      case UNSPEC_VOLATILE:
         return 0;
      case ASM_OPERANDS:
         if (MEM_VOLATILE_P (x))
            return 0;
         break;
      case PLUS:
         if (GET_MODE (x) == Pmode
         && (REG_P (XEXP (x, 0))
         || MEM_P (XEXP (x, 0))
         || GET_CODE (XEXP (x, 0)) == VALUE)
         && CONST_INT_P (XEXP (x, 1)))
            return cselib_hash_plus_const_int(self,XEXP (x, 0), INTVAL (XEXP (x, 1)),create, memmode);
         break;

      default:
         break;
   }

   i = GET_RTX_LENGTH (code) - 1;
   fmt = GET_RTX_FORMAT (code);
   for (; i >= 0; i--){
      switch (fmt[i]){
         case 'e':
         {
            rtx tem = XEXP (x, i);
            unsigned int tem_hash = cselib_hash_rtx(self,tem, create, memmode);

            if (tem_hash == 0)
               return 0;

            hash += tem_hash;
         }
            break;
         case 'E':
            for (j = 0; j < XVECLEN (x, i); j++){
               unsigned int tem_hash = cselib_hash_rtx(self,XVECEXP (x, i, j), create, memmode);

               if (tem_hash == 0)
                  return 0;

               hash += tem_hash;
            }
            break;
         case 's':
         {
            const unsigned char *p = (const unsigned char *) XSTR (x, i);
            if (p)
               while (*p)
                  hash += *p++;
            break;
         }
         case 'i':
            hash += XINT (x, i);
            break;
         case 'p':
            hash += constant_lower_bound (SUBREG_BYTE (x));
            break;
         case '0':
         case 't':
            /* unused */
            break;
         default:
            gcc_unreachable ();
      }
   }
   return hash ? hash : 1 + (unsigned int) GET_CODE (x);
}

/* Create a new value structure for VALUE and initialize it.  The mode of the
   value is MODE.  */
static inline cselib_val *new_cselib_val (MtcsCseLib *self,unsigned int hash, machine_mode mode, rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   cselib_val *e = self->cselib_val_pool.allocate ();

   gcc_assert (hash);
   gcc_assert (self->next_uid);

   e->hash = hash;
   e->uid = self->next_uid++;
   /* We use an alloc pool to allocate this RTL construct because it
   accounts for about 8% of the overall memory usage.  We know
   precisely when we can have VALUE RTXen (when cselib is active)
   so we don't need to put them in garbage collected memory.
   ??? Why should a VALUE be an RTX in the first place?  */
   e->val_rtx = (rtx_def*) self->value_pool.allocate ();
   memset (e->val_rtx, 0, RTX_HDR_SIZE);
   PUT_CODE (e->val_rtx, VALUE);
   mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,e->val_rtx, mode);
   CSELIB_VAL_PTR (e->val_rtx) = e;
   e->addr_list = 0;
   e->locs = 0;
   e->next_containing_mem = 0;

   scalar_int_mode int_mode;
   if (REG_P (x) && mtcs_mode_is_int_mode/*!is_int_mode*/(mtcsMode,mode, &int_mode)
   && mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,int_mode) > 1
   && REG_VALUES (REGNO (x)) != NULL
   && (!self->cselib_current_insn || !DEBUG_INSN_P (self->cselib_current_insn))){
      rtx copy = shallow_copy_rtx (x);
      scalar_int_mode narrow_mode_iter;
      MTCS_FOR_EACH_MODE_UNTIL (mtcsMode,narrow_mode_iter, int_mode){
         PUT_MODE_RAW (copy, narrow_mode_iter);
         cselib_val *v = mtcs_cse_lib_cselib_lookup/*!cselib_lookup*/(self,copy, narrow_mode_iter, 0, VOIDmode);
         if (v){
            rtx sub = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,narrow_mode_iter, e->val_rtx, int_mode);
            if (sub)
               new_elt_loc_list(self,v, sub);
         }
      }
   }

   if (dump_file && (dump_flags & TDF_CSELIB)){
      fprintf (dump_file, "cselib value %u:%u ", e->uid, hash);
      if (mtcsOptionsItem->x_flag_dump_noaddr || mtcsOptionsItem->x_flag_dump_unnumbered)
         fputs ("# ", dump_file);
      else
         fprintf (dump_file, "%p ", (void*)e);
      mtcs_print_rtl_single/*!print_rtl_single*/(dump_file, x);
      fputc ('\n', dump_file);
   }

   return e;
}

/* ADDR_ELT is a value that is used as address.  MEM_ELT is the value that
   contains the data at this address.  X is a MEM that represents the
   value.  Update the two value structures to represent this situation.  */
static void add_mem_for_addr (MtcsCseLib *self,cselib_val *addr_elt, cselib_val *mem_elt, rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   addr_elt = canonical_cselib_val (addr_elt);
   mem_elt = canonical_cselib_val (mem_elt);
   /* Avoid duplicates.  */
   addr_space_t as = mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,x);
   for (elt_loc_list *l = mem_elt->locs; l; l = l->next)
      if (MEM_P (l->loc) && CSELIB_VAL_PTR (XEXP (l->loc, 0)) == addr_elt
      && mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,l->loc) == as){
         promote_debug_loc(self,l);
         return;
      }

   addr_elt->addr_list = new_elt_list(self,addr_elt->addr_list, mem_elt);
   new_elt_loc_list(self,mem_elt, mtcs_rtl_replace_equiv_address_nv/*!replace_equiv_address_nv*/(mtcsRTL,x, addr_elt->val_rtx));
   if (mem_elt->next_containing_mem == NULL){
      mem_elt->next_containing_mem = self->first_containing_mem;
      self->first_containing_mem = mem_elt;
   }
}

/* Subroutine of cselib_lookup.  Return a value for X, which is a MEM rtx.
   If CREATE, make a new one if we haven't seen it before.  */
static cselib_val *cselib_lookup_mem (MtcsCseLib *self,rtx x, int create)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   machine_mode mode = GET_MODE (x);
   machine_mode addr_mode;
   cselib_val **slot;
   cselib_val *addr;
   cselib_val *mem_elt;

   if (MEM_VOLATILE_P (x) || mode == mtcsMode->modes.M_BLKmode
   || !self->cselib_record_memory
   || (mtcs_mode_is_float_p/*!FLOAT_MODE_P*/(mtcsMode,mode) && mtcsOptionsItem->x_flag_float_store))
      return 0;

   addr_mode = GET_MODE (XEXP (x, 0));
   if (addr_mode == VOIDmode)
      addr_mode = mtcs_mode_get_Pmode(mtcsMode);
   /* Look up the value for the address.  */
   addr = mtcs_cse_lib_cselib_lookup/*!cselib_lookup*/(self,XEXP (x, 0), addr_mode, create, mode);
   if (! addr)
      return 0;
   addr = canonical_cselib_val (addr);
   /* Find a value that describes a value of our mode at that address.  */
   addr_space_t as = mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,x);
   for (elt_list *l = addr->addr_list; l; l = l->next)
      if (GET_MODE (l->elt->val_rtx) == mode){
         for (elt_loc_list *l2 = l->elt->locs; l2; l2 = l2->next)
            if (MEM_P (l2->loc) && mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,l2->loc) == as){
               promote_debug_loc(self,l->elt->locs);
               return l->elt;
            }
      }

   if (! create)
      return 0;

   mem_elt = new_cselib_val(self,self->next_uid, mode, x);
   add_mem_for_addr(self,addr, mem_elt, x);
   slot = cselib_find_slot(self,mode, x, mem_elt->hash, INSERT, VOIDmode);
   *slot = mem_elt;
   return mem_elt;
}

/* Search through the possible substitutions in P.  We prefer a non reg
   substitution because this allows us to expand the tree further.  If
   we find, just a reg, take the lowest regno.  There may be several
   non-reg results, we just take the first one because they will all
   expand to the same place.  */
static rtx expand_loc (MtcsCseLib *self,struct elt_loc_list *p, struct expand_value_data *evd,
       int max_depth)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg =mtcs_target_get_reg(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   rtx reg_result = NULL;
   unsigned int regno = UINT_MAX;
   struct elt_loc_list *p_in = p;

   for (; p; p = p->next){
      /* Return these right away to avoid returning stack pointer based
      expressions for frame pointer and vice versa, which is something
      that would confuse DSE.  See the comment in cselib_expand_value_rtx_1
      for more details.  */
      if (REG_P (p->loc)
      && (REGNO (p->loc) == mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg)
      || REGNO (p->loc) == mtcs_reg_get_frame_pointer_regnum/*!FRAME_POINTER_REGNUM*/(mtcsReg)
      || REGNO (p->loc) ==mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg)
      || REGNO (p->loc) == self->cfa_base_preserved_regno))
         return p->loc;
      /* Avoid infinite recursion trying to expand a reg into a
      the same reg.  */
      if ((REG_P (p->loc)) && (REGNO (p->loc) < regno) && !bitmap_bit_p (evd->regs_active, REGNO (p->loc))){
         reg_result = p->loc;
         regno = REGNO (p->loc);
      }
      /* Avoid infinite recursion and do not try to expand the
      value.  */
      else if (GET_CODE (p->loc) == VALUE   && CSELIB_VAL_PTR (p->loc)->locs == p_in)
         continue;
      else if (!REG_P (p->loc)){
         rtx result, note;
         if (dump_file && (dump_flags & TDF_CSELIB)){
            mtcs_print_inline_rtx/*!print_inline_rtx*/(dump_file, p->loc, 0);
            fprintf (dump_file, "\n");
         }
         if (GET_CODE (p->loc) == LO_SUM
         && GET_CODE (XEXP (p->loc, 1)) == SYMBOL_REF
         && p->setting_insn
         && (note = find_reg_note (p->setting_insn, REG_EQUAL, NULL_RTX))
         && XEXP (note, 0) == XEXP (p->loc, 1))
            return XEXP (p->loc, 1);
         result = cselib_expand_value_rtx_1(self,p->loc, evd, max_depth - 1);
         if (result)
            return result;
      }
   }

   if (regno != UINT_MAX){
      rtx result;
      if (dump_file && (dump_flags & TDF_CSELIB))
         fprintf (dump_file, "r%d\n", regno);

      result = cselib_expand_value_rtx_1(self,reg_result, evd, max_depth - 1);
      if (result)
         return result;
   }

   if (dump_file && (dump_flags & TDF_CSELIB)){
      if (reg_result){
         mtcs_print_inline_rtx/*!print_inline_rtx*/(dump_file, reg_result, 0);
         fprintf (dump_file, "\n");
      }else
         fprintf (dump_file, "NULL\n");
   }
   return reg_result;
}


/* Forward substitute and expand an expression out to its roots.
   This is the opposite of common subexpression.  Because local value
   numbering is such a weak optimization, the expanded expression is
   pretty much unique (not from a pointer equals point of view but
   from a tree shape point of view.

   This function returns NULL if the expansion fails.  The expansion
   will fail if there is no value number for one of the operands or if
   one of the operands has been overwritten between the current insn
   and the beginning of the basic block.  For instance x has no
   expansion in:

   r1 <- r1 + 3
   x <- r1 + 8

   REGS_ACTIVE is a scratch bitmap that should be clear when passing in.
   It is clear on return.  */
//原型 cselib_expand_value_rtx cselib.h cselib.cc
rtx mtcs_cse_lib_cselib_expand_value_rtx (MtcsCseLib *self,rtx orig, bitmap regs_active, int max_depth)
{
   struct expand_value_data evd;

   evd.regs_active = regs_active;
   evd.callback = NULL;
   evd.callback_arg = NULL;
   evd.dummy = false;

   return cselib_expand_value_rtx_1(self,orig, &evd, max_depth);
}

/* Same as cselib_expand_value_rtx, but using a callback to try to
   resolve some expressions.  The CB function should return ORIG if it
   can't or does not want to deal with a certain RTX.  Any other
   return value, including NULL, will be used as the expansion for
   VALUE, without any further changes.  */
//原型 cselib_expand_value_rtx_cb cselib.h cselib.cc
rtx mtcs_cse_lib_cselib_expand_value_rtx_cb (MtcsCseLib *self,rtx orig, bitmap regs_active, int max_depth,
             cselib_expand_callback cb, void *data)
{
   struct expand_value_data evd;
   evd.regs_active = regs_active;
   evd.callback = cb;
   evd.callback_arg = data;
   evd.dummy = false;
   return cselib_expand_value_rtx_1(self,orig, &evd, max_depth);
}

/* Similar to cselib_expand_value_rtx_cb, but no rtxs are actually copied
   or simplified.  Useful to find out whether cselib_expand_value_rtx_cb
   would return NULL or non-NULL, without allocating new rtx.  */
//原型 cselib_dummy_expand_value_rtx_cb cselib.h cselib.cc
bool mtcs_cse_lib_cselib_dummy_expand_value_rtx_cb (MtcsCseLib *self,rtx orig, bitmap regs_active, int max_depth,
              cselib_expand_callback cb, void *data)
{
   struct expand_value_data evd;
   evd.regs_active = regs_active;
   evd.callback = cb;
   evd.callback_arg = data;
   evd.dummy = true;
   return cselib_expand_value_rtx_1(self,orig, &evd, max_depth) != NULL;
}

/* Internal implementation of cselib_expand_value_rtx and
   cselib_expand_value_rtx_cb.  */
static rtx cselib_expand_value_rtx_1 (MtcsCseLib *self,rtx orig, struct expand_value_data *evd,int max_depth)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsReg *mtcsReg =mtcs_target_get_reg(mtcsTarget);

   rtx copy, scopy;
   int i, j;
   RTX_CODE code;
   const char *format_ptr;
   machine_mode mode;

   code = GET_CODE (orig);

   /* For the context of dse, if we end up expand into a huge tree, we
   will not have a useful address, so we might as well just give up
   quickly.  */
   if (max_depth <= 0)
      return NULL;

   switch (code){
      case REG:
      {
         struct elt_list *l = REG_VALUES (REGNO (orig));

         if (l && l->elt == NULL)
            l = l->next;
         for (; l; l = l->next)
            if (GET_MODE (l->elt->val_rtx) == GET_MODE (orig)){
               rtx result;
               unsigned regno = REGNO (orig);

               /* The only thing that we are not willing to do (this
               is requirement of dse and if others potential uses
               need this function we should add a parm to control
               it) is that we will not substitute the
               STACK_POINTER_REGNUM, FRAME_POINTER or the
               HARD_FRAME_POINTER.

               These expansions confuses the code that notices that
               stores into the frame go dead at the end of the
               function and that the frame is not effected by calls
               to subroutines.  If you allow the
               STACK_POINTER_REGNUM substitution, then dse will
               think that parameter pushing also goes dead which is
               wrong.  If you allow the FRAME_POINTER or the
               HARD_FRAME_POINTER then you lose the opportunity to
               make the frame assumptions.  */
               if (regno == mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg)
               || regno == mtcs_reg_get_frame_pointer_regnum/*!FRAME_POINTER_REGNUM*/(mtcsReg)
               || regno == mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg)
               || regno == self->cfa_base_preserved_regno)
                  return orig;

               bitmap_set_bit (evd->regs_active, regno);

               if (dump_file && (dump_flags & TDF_CSELIB))
                  fprintf (dump_file, "expanding: r%d into: ", regno);

               result = expand_loc(self,l->elt->locs, evd, max_depth);
               bitmap_clear_bit (evd->regs_active, regno);

               if (result)
                  return result;
               else
                  return orig;
            }
         return orig;
      }

      CASE_CONST_ANY:
      case SYMBOL_REF:
      case CODE_LABEL:
      case PC:
      case SCRATCH:
         /* SCRATCH must be shared because they represent distinct values.  */
         return orig;
      case CLOBBER:
         if (REG_P (XEXP (orig, 0)) && mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,REGNO (XEXP (orig, 0))))
            return orig;
         break;

      case CONST:
         if (shared_const_p (orig))
            return orig;
         break;

      case SUBREG:
      {
         rtx subreg;

         if (evd->callback){
            subreg = evd->callback (orig, evd->regs_active, max_depth,evd->callback_arg);
            if (subreg != orig)
               return subreg;
         }

         subreg = cselib_expand_value_rtx_1(self,SUBREG_REG (orig), evd, max_depth - 1);
         if (!subreg)
            return NULL;
         scopy = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,
               GET_MODE (orig), subreg,GET_MODE (SUBREG_REG (orig)),SUBREG_BYTE (orig));
         if (scopy == NULL || (GET_CODE (scopy) == SUBREG  && !REG_P (SUBREG_REG (scopy)) && !MEM_P (SUBREG_REG (scopy))))
            return NULL;

         return scopy;
      }

      case VALUE:
      {
         rtx result;

         if (dump_file && (dump_flags & TDF_CSELIB)){
            fputs ("\nexpanding ", dump_file);
            mtcs_print_rtl_single/*!print_rtl_single*/(dump_file, orig);
            fputs (" into...", dump_file);
         }

         if (evd->callback){
            result = evd->callback (orig, evd->regs_active, max_depth, evd->callback_arg);

            if (result != orig)
               return result;
         }

         result = expand_loc(self,CSELIB_VAL_PTR (orig)->locs, evd, max_depth);
         return result;
      }

      case DEBUG_EXPR:
         if (evd->callback)
            return evd->callback (orig, evd->regs_active, max_depth, evd->callback_arg);
         return orig;

      default:
         break;
   }

   /* Copy the various flags, fields, and other information.  We assume
   that all fields need copying, and then clear the fields that should
   not be copied.  That is the sensible default behavior, and forces
   us to explicitly document why we are *not* copying a flag.  */
   if (evd->dummy)
      copy = NULL;
   else
      copy = shallow_copy_rtx (orig);

   format_ptr = GET_RTX_FORMAT (code);

   for (i = 0; i < GET_RTX_LENGTH (code); i++)
      switch (*format_ptr++){
         case 'e':
            if (XEXP (orig, i) != NULL){
               rtx result = cselib_expand_value_rtx_1(self,XEXP (orig, i), evd,
               max_depth - 1);
               if (!result)
                  return NULL;
               if (copy)
                  XEXP (copy, i) = result;
            }
            break;

         case 'E':
         case 'V':
            if (XVEC (orig, i) != NULL){
               if (copy)
                  XVEC (copy, i) = rtvec_alloc (XVECLEN (orig, i));
               for (j = 0; j < XVECLEN (orig, i); j++){
                  rtx result = cselib_expand_value_rtx_1(self,XVECEXP (orig, i, j), evd, max_depth - 1);
                  if (!result)
                     return NULL;
                  if (copy)
                     XVECEXP (copy, i, j) = result;
               }
            }
            break;

         case 't':
         case 'w':
         case 'i':
         case 's':
         case 'S':
         case 'T':
         case 'u':
         case 'B':
         case '0':
            /* These are left unchanged.  */
            break;

         default:
            gcc_unreachable ();
      }

   if (evd->dummy)
      return orig;

   mode = GET_MODE (copy);
   /* If an operand has been simplified into CONST_INT, which doesn't
   have a mode and the mode isn't derivable from whole rtx's mode,
   try simplify_*_operation first with mode from original's operand
   and as a fallback wrap CONST_INT into gen_rtx_CONST.  */
   scopy = copy;
   switch (GET_RTX_CLASS (code)){
      case RTX_UNARY:
         if (CONST_INT_P (XEXP (copy, 0)) && GET_MODE (XEXP (orig, 0)) != VOIDmode){
            scopy = mtcs_simplify_rtx_unary_operation/*!simplify_unary_operation*/(mtcsSimplifyRtx,
                  code, mode, XEXP (copy, 0),GET_MODE (XEXP (orig, 0)));
            if (scopy)
               return scopy;
         }
         break;
      case RTX_COMM_ARITH:
      case RTX_BIN_ARITH:
         /* These expressions can derive operand modes from the whole rtx's mode.  */
         break;
      case RTX_TERNARY:
      case RTX_BITFIELD_OPS:
         if (CONST_INT_P (XEXP (copy, 0))  && GET_MODE (XEXP (orig, 0)) != VOIDmode){
            scopy = mtcs_simplify_rtx_ternary_operation/*!simplify_ternary_operation*/(mtcsSimplifyRtx,code, mode,
            GET_MODE (XEXP (orig, 0)),
            XEXP (copy, 0), XEXP (copy, 1),
            XEXP (copy, 2));
            if (scopy)
               return scopy;
         }
         break;
      case RTX_COMPARE:
      case RTX_COMM_COMPARE:
         if (CONST_INT_P (XEXP (copy, 0))
         && GET_MODE (XEXP (copy, 1)) == VOIDmode
         && (GET_MODE (XEXP (orig, 0)) != VOIDmode
         || GET_MODE (XEXP (orig, 1)) != VOIDmode)){
            scopy = mtcs_simplify_rtx_relational_operation/*!simplify_relational_operation*/(mtcsSimplifyRtx,code, mode,
            (GET_MODE (XEXP (orig, 0)) != VOIDmode)
            ? GET_MODE (XEXP (orig, 0)) : GET_MODE (XEXP (orig, 1)), XEXP (copy, 0), XEXP (copy, 1));
            if (scopy)
               return scopy;
         }
         break;
      default:
         break;
   }
   scopy = mtcs_simplify_rtx_simplify_rtx/*!simplify_rtx*/(mtcsSimplifyRtx,copy);
   if (scopy)
      return scopy;
   return copy;
}

/* Walk rtx X and replace all occurrences of REG and MEM subexpressions
   with VALUE expressions.  This way, it becomes independent of changes
   to registers and memory.
   X isn't actually modified; if modifications are needed, new rtl is
   allocated.  However, the return value can share rtl with X.
   If X is within a MEM, MEMMODE must be the mode of the MEM.  */
//原型 cselib_subst_to_values cselib.h cselib.cc
rtx mtcs_cse_lib_cselib_subst_to_values (MtcsCseLib *self,rtx x, machine_mode memmode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg =mtcs_target_get_reg(mtcsTarget);

   enum rtx_code code = GET_CODE (x);
   const char *fmt = GET_RTX_FORMAT (code);
   cselib_val *e;
   struct elt_list *l;
   rtx copy = x;
   int i;
   poly_int64 offset;

   switch (code){
      case REG:
         l = REG_VALUES (REGNO (x));
         if (l && l->elt == NULL)
            l = l->next;
         for (; l; l = l->next)
            if (GET_MODE (l->elt->val_rtx) == GET_MODE (x))
               return l->elt->val_rtx;

         gcc_unreachable ();

      case MEM:
         e = cselib_lookup_mem(self,x, 0);
         /* This used to happen for autoincrements, but we deal with them
         properly now.  Remove the if stmt for the next release.  */
         if (! e){
            /* Assign a value that doesn't match any other.  */
            e = new_cselib_val(self,self->next_uid, GET_MODE (x), x);
         }
         return e->val_rtx;

      case ENTRY_VALUE:
         e = mtcs_cse_lib_cselib_lookup/*!cselib_lookup*/(self,x, GET_MODE (x), 0, memmode);
         if (! e)
            break;
         return e->val_rtx;

      CASE_CONST_ANY:
         return x;

      case PRE_DEC:
      case PRE_INC:
         gcc_assert (memmode != VOIDmode);
         offset = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,memmode);
         if (code == PRE_DEC)
            offset = -offset;
         return mtcs_cse_lib_cselib_subst_to_values/*!cselib_subst_to_values*/(self,
               mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,GET_MODE (x),XEXP (x, 0), offset), memmode);

      case PRE_MODIFY:
         gcc_assert (memmode != VOIDmode);
         return mtcs_cse_lib_cselib_subst_to_values/*!cselib_subst_to_values*/(self,XEXP (x, 1), memmode);

      case POST_DEC:
      case POST_INC:
      case POST_MODIFY:
         gcc_assert (memmode != VOIDmode);
         return mtcs_cse_lib_cselib_subst_to_values/*!cselib_subst_to_values*/(self,XEXP (x, 0), memmode);

      case PLUS:
         if (GET_MODE (x) == Pmode && CONST_INT_P (XEXP (x, 1))){
            rtx t = mtcs_cse_lib_cselib_subst_to_values/*!cselib_subst_to_values*/(self,XEXP (x, 0), memmode);
            if (GET_CODE (t) == VALUE){
               if (SP_DERIVED_VALUE_P (t) && XEXP (x, 1) == const0_rtx)
                  return t;
               for (struct elt_loc_list *l = CSELIB_VAL_PTR (t)->locs; l; l = l->next)
                  if (GET_CODE (l->loc) == PLUS
                  && GET_CODE (XEXP (l->loc, 0)) == VALUE
                  && SP_DERIVED_VALUE_P (XEXP (l->loc, 0))
                  && CONST_INT_P (XEXP (l->loc, 1)))
                     return mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,Pmode, l->loc, INTVAL (XEXP (x, 1)));
            }
            if (t != XEXP (x, 0)){
               copy = shallow_copy_rtx (x);
               XEXP (copy, 0) = t;
            }
            return copy;
         }

      default:
         break;
   }

   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--){
      if (fmt[i] == 'e'){
         rtx t = mtcs_cse_lib_cselib_subst_to_values/*!cselib_subst_to_values*/(self,XEXP (x, i), memmode);

         if (t != XEXP (x, i)){
            if (x == copy)
               copy = shallow_copy_rtx (x);
            XEXP (copy, i) = t;
         }
      }else if (fmt[i] == 'E'){
         int j;

         for (j = 0; j < XVECLEN (x, i); j++){
            rtx t = mtcs_cse_lib_cselib_subst_to_values/*!cselib_subst_to_values*/(self,XVECEXP (x, i, j), memmode);

            if (t != XVECEXP (x, i, j)){
               if (XVEC (x, i) == XVEC (copy, i)){
                  if (x == copy)
                     copy = shallow_copy_rtx (x);
                  XVEC (copy, i) = shallow_copy_rtvec (XVEC (x, i));
               }
               XVECEXP (copy, i, j) = t;
            }
         }
      }
   }

   return copy;
}

/* Wrapper for cselib_subst_to_values, that indicates X is in INSN.  */
//原型 cselib_subst_to_values_from_insn cselib.h cselib.cc
rtx mtcs_cse_lib_cselib_subst_to_values_from_insn (MtcsCseLib *self,rtx x, machine_mode memmode, rtx_insn *insn)
{
   rtx ret;
   gcc_assert (!self->cselib_current_insn);
   self->cselib_current_insn = insn;
   ret = mtcs_cse_lib_cselib_subst_to_values/*!cselib_subst_to_values*/(self,x, memmode);
   self->cselib_current_insn = NULL;
   return ret;
}

/* Look up the rtl expression X in our tables and return the value it
   has.  If CREATE is zero, we return NULL if we don't know the value.
   Otherwise, we create a new one if possible, using mode MODE if X
   doesn't have a mode (i.e. because it's a constant).  When X is part
   of an address, MEMMODE should be the mode of the enclosing MEM if
   we're tracking autoinc expressions.  */
static cselib_val *cselib_lookup_1 (MtcsCseLib *self,rtx x, machine_mode mode,  int create, machine_mode memmode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg =mtcs_target_get_reg(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

   cselib_val **slot;
   cselib_val *e;
   unsigned int hashval;

   if (GET_MODE (x) != VOIDmode)
      mode = GET_MODE (x);

   if (GET_CODE (x) == VALUE)
      return CSELIB_VAL_PTR (x);

   if (REG_P (x)){
      struct elt_list *l;
      unsigned int i = REGNO (x);

      l = REG_VALUES (i);
      if (l && l->elt == NULL)
         l = l->next;
      for (; l; l = l->next)
         if (mode == GET_MODE (l->elt->val_rtx)){
            promote_debug_loc(self,l->elt->locs);
            return l->elt;
         }

      if (! create)
         return 0;

      if (i < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)){
         unsigned int n = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,i, mode);

         if (n > self->max_value_regs)
            self->max_value_regs = n;
      }

      e = new_cselib_val(self,self->next_uid, GET_MODE (x), x);
      if (GET_MODE (x) == mtcs_mode_get_Pmode(mtcsMode) && x == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL))
         SP_DERIVED_VALUE_P (e->val_rtx) = 1;
      new_elt_loc_list(self,e, x);

      scalar_int_mode int_mode;
      if (REG_VALUES (i) == 0){
         /* Maintain the invariant that the first entry of
         REG_VALUES, if present, must be the value used to set the
         register, or NULL.  */
         self->used_regs[self->n_used_regs++] = i;
         REG_VALUES (i) = new_elt_list(self,REG_VALUES (i), NULL);
      }else if (self->cselib_preserve_constants  && mtcs_mode_is_int_mode/*!is_int_mode*/(mtcsMode,mode, &int_mode)){
         /* During var-tracking, try harder to find equivalences
         for SUBREGs.  If a setter sets say a DImode register
         and user uses that register only in SImode, add a lowpart
         subreg location.  */
         struct elt_list *lwider = NULL;
         scalar_int_mode lmode;
         l = REG_VALUES (i);
         if (l && l->elt == NULL)
            l = l->next;
         for (; l; l = l->next)
            if (mtcs_mode_is_int_mode/*!is_int_mode*/(mtcsMode,GET_MODE (l->elt->val_rtx), &lmode)
            && mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,lmode) > mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,int_mode)
            && (lwider == NULL
            || mtcs_mode_partial_subreg_p/*!partial_subreg_p*/(mtcsMode,lmode,GET_MODE (lwider->elt->val_rtx)))){
               struct elt_loc_list *el;
               if (i < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)  && hard_regno_nregs (i, lmode) != 1)
                  continue;
               for (el = l->elt->locs; el; el = el->next)
                  if (!REG_P (el->loc))
                     break;
               if (el)
                  lwider = l;
            }
         if (lwider){
            rtx sub = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,
                  int_mode, lwider->elt->val_rtx, GET_MODE (lwider->elt->val_rtx));
            if (sub)
               new_elt_loc_list(self,e, sub);
         }
      }
      REG_VALUES (i)->next = new_elt_list(self,REG_VALUES (i)->next, e);
      slot = cselib_find_slot(self,mode, x, e->hash, INSERT, memmode);
      *slot = e;
      return e;
   }

   if (MEM_P (x))
      return cselib_lookup_mem(self,x, create);

   hashval = cselib_hash_rtx(self,x, create, memmode);
   /* Can't even create if hashing is not possible.  */
   if (! hashval)
      return 0;

   slot = cselib_find_slot(self,mode, x, hashval,
   create ? INSERT : NO_INSERT, memmode);
   if (slot == 0)
      return 0;

   e = (cselib_val *) *slot;
   if (e)
      return e;

   e = new_cselib_val(self,hashval, mode, x);

   /* We have to fill the slot before calling cselib_subst_to_values:
   the hash table is inconsistent until we do so, and
   cselib_subst_to_values will need to do lookups.  */
   *slot = e;
   rtx v = mtcs_cse_lib_cselib_subst_to_values/*!cselib_subst_to_values*/(self,x, memmode);

   /* If self->cselib_preserve_constants, we might get a SP_DERIVED_VALUE_P
   VALUE that isn't in the hash tables anymore.  */
   if (GET_CODE (v) == VALUE && SP_DERIVED_VALUE_P (v) && PRESERVED_VALUE_P (v))
      PRESERVED_VALUE_P (e->val_rtx) = 1;

   new_elt_loc_list(self,e, v);
   return e;
}

/* Wrapper for cselib_lookup, that indicates X is in INSN.  */
//原型 cselib_lookup_from_insn cselib.h cselib.cc
cselib_val *mtcs_cse_lib_cselib_lookup_from_insn (MtcsCseLib *self,rtx x, machine_mode mode,
          int create, machine_mode memmode, rtx_insn *insn)
{
   cselib_val *ret;
   gcc_assert (!self->cselib_current_insn);
   self->cselib_current_insn = insn;
   ret = mtcs_cse_lib_cselib_lookup/*!cselib_lookup*/(self,x, mode, create, memmode);
   self->cselib_current_insn = NULL;
   return ret;
}

/* Wrapper for cselib_lookup_1, that logs the lookup result and
   maintains invariants related with debug insns.  */
//原型 cselib_lookup cselib.h cselib.cc
cselib_val *mtcs_cse_lib_cselib_lookup (MtcsCseLib *self,rtx x, machine_mode mode, int create, machine_mode memmode)
{
   cselib_val *ret = cselib_lookup_1(self,x, mode, create, memmode);
   /* ??? Should we return NULL if we're not to create an entry, the
   found loc is a debug loc and self->cselib_current_insn is not DEBUG?
   If so, we should also avoid converting val to non-DEBUG; probably
   easiest setting self->cselib_current_insn to NULL before the call
   above.  */
   if (dump_file && (dump_flags & TDF_CSELIB)){
      fputs ("cselib lookup ", dump_file);
      mtcs_print_inline_rtx/*!print_inline_rtx*/(dump_file, x, 2);
      fprintf (dump_file, " => %u:%u\n", ret ? ret->uid : 0, ret ? ret->hash : 0);
   }

   return ret;
}

/* Invalidate the value at *L, which is part of REG_VALUES (REGNO).  */
static void cselib_invalidate_regno_val (MtcsCseLib *self,unsigned int regno, struct elt_list **l)
{
   cselib_val *v = (*l)->elt;
   if (*l == REG_VALUES (regno)){
      /* Maintain the invariant that the first entry of
      REG_VALUES, if present, must be the value used to set
      the register, or NULL.  This is also nice because
      then we won't push the same regno onto user_regs
      multiple times.  */
      (*l)->elt = NULL;
      l = &(*l)->next;
   }else
      unchain_one_elt_list(self,l);

   v = canonical_cselib_val (v);

   bool had_locs = v->locs != NULL;
   rtx_insn *setting_insn = v->locs ? v->locs->setting_insn : NULL;

   /* Now, we clear the mapping from value to reg.  It must exist, so
   this code will crash intentionally if it doesn't.  */
   for (elt_loc_list **p = &v->locs; ; p = &(*p)->next){
      rtx x = (*p)->loc;

      if (REG_P (x) && REGNO (x) == regno){
         unchain_one_elt_loc_list(self,p);
         break;
      }
   }

   if (had_locs && cselib_useless_value_p (v)){
      if (setting_insn && DEBUG_INSN_P (setting_insn))
         self->n_useless_debug_values++;
      else
         self->n_useless_values++;
   }
}

/* Invalidate any entries in self->reg_values that overlap REGNO.  This is called
   if REGNO is changing.  MODE is the mode of the assignment to REGNO, which
   is used to determine how many hard registers are being changed.  If MODE
   is VOIDmode, then only REGNO is being changed; this is used when
   invalidating call clobbered registers across a call.  */
static void cselib_invalidate_regno (MtcsCseLib *self,unsigned int regno, machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg =mtcs_target_get_reg(mtcsTarget);

   unsigned int endregno;
   unsigned int i;

   /* If we see pseudos after reload, something is _wrong_.  */
   gcc_assert (!reload_completed
         || regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
         || reg_renumber[regno] < 0);

   /* Determine the range of registers that must be invalidated.  For
   pseudos, only REGNO is affected.  For hard regs, we must take MODE
   into account, and we must also invalidate lower register numbers
   if they contain values that overlap REGNO.  */
   if (regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)){
      gcc_assert (mode != VOIDmode);
      if (regno < self->max_value_regs)
         i = 0;
      else
         i = regno - self->max_value_regs;

      endregno = mtcs_reg_end_hard_regno/*!end_hard_regno*/(mtcsReg,mode, regno);
   }else{
      i = regno;
      endregno = regno + 1;
   }

   for (; i < endregno; i++){
      struct elt_list **l = &REG_VALUES (i);

      /* Go through all known values for this reg; if it overlaps the range
      we're invalidating, remove the value.  */
      while (*l){
         cselib_val *v = (*l)->elt;
         unsigned int this_last = i;

         if (i < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg) && v != NULL)
            this_last = mtcs_reg_end_hard_regno/*!end_hard_regno*/(mtcsReg,GET_MODE (v->val_rtx), i) - 1;

         if (this_last < regno || v == NULL  || (v == self->cfa_base_preserved_val
         && i == self->cfa_base_preserved_regno)){
            l = &(*l)->next;
            continue;
         }

         /* We have an overlap.  */
         cselib_invalidate_regno_val(self,i, l);
      }
   }
}

/* Invalidate any locations in the table which are changed because of a
   store to MEM_RTX.  If this is called because of a non-const call
   instruction, MEM_RTX is (mem:BLK const0_rtx).  */
static void cselib_invalidate_mem (MtcsCseLib *self,rtx mem_rtx)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   cselib_val **vp, *v, *next;
   int num_mems = 0;
   rtx mem_addr;

   mem_addr = mtcs_alias_canon_rtx/*!canon_rtx*/(mtcsAlias,get_addr (XEXP (mem_rtx, 0)));
   mem_rtx = mtcs_alias_canon_rtx/*!canon_rtx*/(mtcsAlias,mem_rtx);

   vp = &self->first_containing_mem;
   for (v = *vp; v != &self->dummy_val; v = next){
      bool has_mem = false;
      struct elt_loc_list **p = &v->locs;
      bool had_locs = v->locs != NULL;
      rtx_insn *setting_insn = v->locs ? v->locs->setting_insn : NULL;

      while (*p){
         rtx x = (*p)->loc;
         cselib_val *addr;
         struct elt_list **mem_chain;

         /* MEMs may occur in locations only at the top level; below
         that every MEM or REG is substituted by its VALUE.  */
         if (!MEM_P (x)){
            p = &(*p)->next;
            continue;
         }
         if (num_mems <mtcsOptionsItem->x_param_max_cselib_memory_locations
         && ! mtcs_alias_canon_anti_dependence/*!canon_anti_dependence*/(mtcsAlias,
               x, false, mem_rtx,GET_MODE (mem_rtx), mem_addr)){
            has_mem = true;
            num_mems++;
            p = &(*p)->next;
            continue;
         }

         /* This one overlaps.  */
         /* We must have a mapping from this MEM's address to the
         value (E).  Remove that, too.  */
         addr = mtcs_cse_lib_cselib_lookup/*!cselib_lookup*/(self,XEXP (x, 0), VOIDmode, 0, GET_MODE (x));
         addr = canonical_cselib_val (addr);
         gcc_checking_assert (v == canonical_cselib_val (v));
         mem_chain = &addr->addr_list;
         for (;;){
            cselib_val *canon = canonical_cselib_val ((*mem_chain)->elt);

            if (canon == v){
               unchain_one_elt_list(self,mem_chain);
               break;
            }

            /* Record canonicalized elt.  */
            (*mem_chain)->elt = canon;

            mem_chain = &(*mem_chain)->next;
         }

         unchain_one_elt_loc_list(self,p);
      }

      if (had_locs && cselib_useless_value_p (v)){
         if (setting_insn && DEBUG_INSN_P (setting_insn))
            self->n_useless_debug_values++;
         else
            self->n_useless_values++;
      }

      next = v->next_containing_mem;
      if (has_mem){
         *vp = v;
         vp = &(*vp)->next_containing_mem;
      }else
         v->next_containing_mem = NULL;
   }
   *vp = &self->dummy_val;
}

/* Invalidate DEST.  */
//原型 cselib_invalidate_rtx cselib.h cselib.cc
void mtcs_cse_lib_cselib_invalidate_rtx (MtcsCseLib *self,rtx dest)
{
   while (GET_CODE (dest) == SUBREG
   || GET_CODE (dest) == ZERO_EXTRACT
   || GET_CODE (dest) == STRICT_LOW_PART)
      dest = XEXP (dest, 0);

   if (REG_P (dest))
      cselib_invalidate_regno(self,REGNO (dest), GET_MODE (dest));
   else if (MEM_P (dest))
      cselib_invalidate_mem(self,dest);
}

/* A wrapper for cselib_invalidate_rtx to be called via note_stores.  */
static void invalidateRtxNoteStores_cb (rtx dest, const_rtx,
               void *data ATTRIBUTE_UNUSED)
{
    MtcsCseLib *self=(MtcsCseLib *)data;
    mtcs_cse_lib_cselib_invalidate_rtx/*!cselib_invalidate_rtx*/(self,dest);
}

/* Record the result of a SET instruction.  DEST is being set; the source
   contains the value described by SRC_ELT.  If DEST is a MEM, DEST_ADDR_ELT
   describes its address.  */
static void cselib_record_set (MtcsCseLib *self,rtx dest, cselib_val *src_elt, cselib_val *dest_addr_elt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg =mtcs_target_get_reg(mtcsTarget);

   if (src_elt == 0 || side_effects_p (dest))
      return;

   if (REG_P (dest)){
      unsigned int dreg = REGNO (dest);
      if (dreg < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)){
         unsigned int n = REG_NREGS (dest);

         if (n > self->max_value_regs)
            self->max_value_regs = n;
      }

      if (REG_VALUES (dreg) == 0){
         self->used_regs[self->n_used_regs++] = dreg;
         REG_VALUES (dreg) = new_elt_list(self,REG_VALUES (dreg), src_elt);
      }else{
         /* The register should have been invalidated.  */
         gcc_assert (REG_VALUES (dreg)->elt == 0);
         REG_VALUES (dreg)->elt = src_elt;
      }

      if (cselib_useless_value_p (src_elt))
         self->n_useless_values--;
      new_elt_loc_list(self,src_elt, dest);
   }else if (MEM_P (dest) && dest_addr_elt != 0  && self->cselib_record_memory){
      if (cselib_useless_value_p (src_elt))
         self->n_useless_values--;
      add_mem_for_addr(self,dest_addr_elt, src_elt, dest);
   }
}

/* Make ELT and X's VALUE equivalent to each other at INSN.  */
//原型 cselib_add_permanent_equiv cselib.h cselib.cc
void mtcs_cselib_cselib_add_permanent_equiv (MtcsCseLib *self,cselib_val *elt, rtx x, rtx_insn *insn)
{
   cselib_val *nelt;
   rtx_insn *save_cselib_current_insn = self->cselib_current_insn;
   gcc_checking_assert (elt);
   gcc_checking_assert (PRESERVED_VALUE_P (elt->val_rtx));
   gcc_checking_assert (!side_effects_p (x));
   self->cselib_current_insn = insn;
   nelt = mtcs_cse_lib_cselib_lookup/*!cselib_lookup*/(self,x, GET_MODE (elt->val_rtx), 1, VOIDmode);
   if (nelt != elt){
      self->cselib_any_perm_equivs = true;
      if (!PRESERVED_VALUE_P (nelt->val_rtx))
         mtcs_cse_lib_cselib_preserve_value/*!cselib_preserve_value*/(self,nelt);
      new_elt_loc_list(self,nelt, elt->val_rtx);
   }

   self->cselib_current_insn = save_cselib_current_insn;
}

/* Return TRUE if any permanent equivalences have been recorded since
   the table was last initialized.  */
//原型 cselib_have_permanent_equivalences cselib.h cselib.cc
bool mtcs_cse_lib_cselib_have_permanent_equivalences (MtcsCseLib *self)
{
  return self->cselib_any_perm_equivs;
}

/* Record stack_pointer_rtx to be equal to
   (plus:P self->cfa_base_preserved_val offset).  Used by var-tracking
   at the start of basic blocks for !frame_pointer_needed functions.  */
//原型 cselib_record_sp_cfa_base_equiv cselib.h cselib.cc
void mtcs_cse_lib_cselib_record_sp_cfa_base_equiv (MtcsCseLib *self,HOST_WIDE_INT offset, rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
   rtx sp_derived_value = NULL_RTX;
   for (struct elt_loc_list *l = self->cfa_base_preserved_val->locs; l; l = l->next)
      if (GET_CODE (l->loc) == VALUE && SP_DERIVED_VALUE_P (l->loc)){
         sp_derived_value = l->loc;
         break;
      }else if (GET_CODE (l->loc) == PLUS
      && GET_CODE (XEXP (l->loc, 0)) == VALUE
      && SP_DERIVED_VALUE_P (XEXP (l->loc, 0))
      && CONST_INT_P (XEXP (l->loc, 1))){
         sp_derived_value = XEXP (l->loc, 0);
         offset = offset + UINTVAL (XEXP (l->loc, 1));
         break;
      }
   if (sp_derived_value == NULL_RTX)
      return;
   cselib_val *val = mtcs_cse_lib_cselib_lookup_from_insn/*!cselib_lookup_from_insn*/(self,
         mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, sp_derived_value, offset),pMode, 1, VOIDmode, insn);
   if (val != NULL){
      PRESERVED_VALUE_P (val->val_rtx) = 1;
      cselib_record_set(self,mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL), val, NULL);
   }
}

/* Return true if V is SP_DERIVED_VALUE_P (or SP_DERIVED_VALUE_P + CONST_INT)
   that can be expressed using self->cfa_base_preserved_val + CONST_INT.  */
//原型 cselib_sp_derived_value_p cselib.h cselib.cc
bool mtcs_cse_lib_cselib_sp_derived_value_p (MtcsCseLib *self,cselib_val *v)
{
   if (!SP_DERIVED_VALUE_P (v->val_rtx))
      for (struct elt_loc_list *l = v->locs; l; l = l->next)
         if (GET_CODE (l->loc) == PLUS
         && GET_CODE (XEXP (l->loc, 0)) == VALUE
         && SP_DERIVED_VALUE_P (XEXP (l->loc, 0))
         && CONST_INT_P (XEXP (l->loc, 1)))
            v = CSELIB_VAL_PTR (XEXP (l->loc, 0));
   if (!SP_DERIVED_VALUE_P (v->val_rtx))
      return false;
   for (struct elt_loc_list *l = v->locs; l; l = l->next)
      if (l->loc == self->cfa_base_preserved_val->val_rtx)
         return true;
      else if (GET_CODE (l->loc) == PLUS
      && XEXP (l->loc, 0) == self->cfa_base_preserved_val->val_rtx
      && CONST_INT_P (XEXP (l->loc, 1)))
         return true;
   return false;
}

/* There is no good way to determine how many elements there can be
   in a PARALLEL.  Since it's fairly cheap, use a really large number.  */
#define MAX_SETS (FIRST_PSEUDO_REGISTER * 2)

struct cselib_record_autoinc_data
{
  struct cselib_set *sets;
  int n_sets;
};

/* Callback for for_each_inc_dec.  Records in ARG the SETs implied by
   autoinc RTXs: SRC plus SRCOFF if non-NULL is stored in DEST.  */

static int cselib_record_autoinc_cb (rtx mem ATTRIBUTE_UNUSED, rtx op ATTRIBUTE_UNUSED,
           rtx dest, rtx src, rtx srcoff, void *arg)
{
  struct cselib_record_autoinc_data *data;
  data = (struct cselib_record_autoinc_data *)arg;

  data->sets[data->n_sets].dest = dest;

  if (srcoff)
    data->sets[data->n_sets].src = gen_rtx_PLUS (GET_MODE (src), src, srcoff);
  else
    data->sets[data->n_sets].src = src;

  data->n_sets++;

  return 0;
}

/* Record the effects of any sets and autoincs in INSN.  */
static void cselib_record_sets (MtcsCseLib *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg =mtcs_target_get_reg(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal =mtcs_target_get_rtlanal(mtcsTarget);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   int n_sets = 0;
   int i;
   struct cselib_set sets[firstPseudoRegister*2/*!MAX_SETS*/];
   rtx cond = 0;
   int n_sets_before_autoinc;
   int n_strict_low_parts = 0;
   struct cselib_record_autoinc_data data;

   rtx body = PATTERN (insn);
   if (GET_CODE (body) == COND_EXEC){
      cond = COND_EXEC_TEST (body);
      body = COND_EXEC_CODE (body);
   }

   /* Find all sets.  */
   if (GET_CODE (body) == SET){
      sets[0].src = SET_SRC (body);
      sets[0].dest = SET_DEST (body);
      n_sets = 1;
   }else if (GET_CODE (body) == PARALLEL){
      /* Look through the PARALLEL and record the values being
      set, if possible.  Also handle any CLOBBERs.  */
      for (i = XVECLEN (body, 0) - 1; i >= 0; --i){
         rtx x = XVECEXP (body, 0, i);

         if (GET_CODE (x) == SET){
            sets[n_sets].src = SET_SRC (x);
            sets[n_sets].dest = SET_DEST (x);
            n_sets++;
         }
      }
   }

   if (n_sets == 1  && MEM_P (sets[0].src)  && !self->cselib_record_memory  && MEM_READONLY_P (sets[0].src)){
      rtx note = find_reg_equal_equiv_note (insn);

      if (note && CONSTANT_P (XEXP (note, 0)))
         sets[0].src = XEXP (note, 0);
   }

   data.sets = sets;
   data.n_sets = n_sets_before_autoinc = n_sets;
   mtcs_rtlanal_for_each_inc_dec/*!for_each_inc_dec*/(mtcsRtlanal,PATTERN (insn), cselib_record_autoinc_cb, &data);
   n_sets = data.n_sets;

   /* Look up the values that are read.  Do this before invalidating the
   locations that are written.  */
   for (i = 0; i < n_sets; i++){
      rtx dest = sets[i].dest;
      rtx orig = dest;

      /* A STRICT_LOW_PART can be ignored; we'll record the equivalence for
      the low part after invalidating any knowledge about larger modes.  */
      if (GET_CODE (sets[i].dest) == STRICT_LOW_PART)
         sets[i].dest = dest = XEXP (dest, 0);

      /* We don't know how to record anything but REG or MEM.  */
      if (REG_P (dest) || (MEM_P (dest) && self->cselib_record_memory)){
         rtx src = sets[i].src;
         if (cond)
            src = gen_rtx_IF_THEN_ELSE (GET_MODE (dest), cond, src, dest);
         sets[i].src_elt = mtcs_cse_lib_cselib_lookup/*!cselib_lookup*/(self,src, GET_MODE (dest), 1, VOIDmode);
         if (MEM_P (dest)){
            machine_mode address_mode = mtcs_rtl_get_address_mode/*!get_address_mode*/(mtcsRTL,dest);

            sets[i].dest_addr_elt = mtcs_cse_lib_cselib_lookup/*!cselib_lookup*/(self,XEXP (dest, 0),
            address_mode, 1, GET_MODE (dest));
         }else
            sets[i].dest_addr_elt = 0;
      }

      /* Improve handling of STRICT_LOW_PART if the current value is known
      to be const0_rtx, then the low bits will be set to dest and higher
      bits will remain zero.  Used in code like:

      {di:SI=0;clobber flags:CC;}
      flags:CCNO=cmp(bx:SI,0)
      strict_low_part(di:QI)=flags:CCNO<=0

      where we can note both that di:QI=flags:CCNO<=0 and
      also that because di:SI is known to be 0 and strict_low_part(di:QI)
      preserves the upper bits that di:SI=zero_extend(flags:CCNO<=0).  */
      scalar_int_mode mode;
      if (dest != orig
      && self->cselib_record_sets_hook
      && REG_P (dest)
      && mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,dest)
      && sets[i].src_elt
      && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (dest), &mode)
      && n_sets + n_strict_low_parts < firstPseudoRegister*2/*!MAX_SETS*/){
         opt_scalar_int_mode wider_mode_iter;
         MTCS_FOR_EACH_WIDER_MODE (mtcsMode,wider_mode_iter, mode){
            scalar_int_mode wider_mode = wider_mode_iter.require ();
            if (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,wider_mode) > BITS_PER_WORD)
               break;

            rtx reg = gen_lowpart (wider_mode, dest);
            if (!REG_P (reg))
               break;

            cselib_val *v = mtcs_cse_lib_cselib_lookup/*!cselib_lookup*/(self,reg, wider_mode, 0, VOIDmode);
            if (!v)
               continue;

            struct elt_loc_list *l;
            for (l = v->locs; l; l = l->next)
               if (l->loc == const0_rtx)
                  break;

            if (!l)
               continue;

            sets[n_sets + n_strict_low_parts].dest = reg;
            sets[n_sets + n_strict_low_parts].src = dest;
            sets[n_sets + n_strict_low_parts++].src_elt = sets[i].src_elt;
            break;
         }
      }
   }

   if (self->cselib_record_sets_hook)
      self->cselib_record_sets_hook (insn, sets, n_sets);

   /* Invalidate all locations written by this insn.  Note that the elts we
   looked up in the previous loop aren't affected, just some of their
   locations may go away.  */
   mtcs_rtlanal_note_pattern_stores/*!note_pattern_stores*/(mtcsRtlanal,
         body, invalidateRtxNoteStores_cb, (void*)self/*!NULL*/);

   for (i = n_sets_before_autoinc; i < n_sets; i++)
      mtcs_cse_lib_cselib_invalidate_rtx/*!cselib_invalidate_rtx*/(self,sets[i].dest);

   /* If this is an asm, look for duplicate sets.  This can happen when the
   user uses the same value as an output multiple times.  This is valid
   if the outputs are not actually used thereafter.  Treat this case as
   if the value isn't actually set.  We do this by smashing the destination
   to pc_rtx, so that we won't record the value later.  */
   if (n_sets >= 2 && asm_noperands (body) >= 0){
      for (i = 0; i < n_sets; i++){
         rtx dest = sets[i].dest;
         if (REG_P (dest) || MEM_P (dest)){
            int j;
            for (j = i + 1; j < n_sets; j++)
               if (rtx_equal_p (dest, sets[j].dest)){
                  sets[i].dest = pc_rtx;
                  sets[j].dest = pc_rtx;
               }
         }
      }
   }

   /* Now enter the equivalences in our tables.  */
   for (i = 0; i < n_sets; i++){
      rtx dest = sets[i].dest;
      if (REG_P (dest) || (MEM_P (dest) && self->cselib_record_memory))
         cselib_record_set(self,dest, sets[i].src_elt, sets[i].dest_addr_elt);
   }

   /* And deal with STRICT_LOW_PART.  */
   for (i = 0; i < n_strict_low_parts; i++){
      if (! PRESERVED_VALUE_P (sets[n_sets + i].src_elt->val_rtx))
         continue;
      machine_mode dest_mode = GET_MODE (sets[n_sets + i].dest);
      cselib_val *v = mtcs_cse_lib_cselib_lookup/*!cselib_lookup*/(self,sets[n_sets + i].dest, dest_mode, 1, VOIDmode);
      mtcs_cse_lib_cselib_preserve_value/*!cselib_preserve_value*/(self,v);
      rtx r = gen_rtx_ZERO_EXTEND (dest_mode,  sets[n_sets + i].src_elt->val_rtx);
      mtcs_cselib_cselib_add_permanent_equiv/*!cselib_add_permanent_equiv*/(self,v, r, insn);
   }
}

/* Return true if INSN in the prologue initializes hard_frame_pointer_rtx.  */
//原型 fp_setter_insn cselib.h cselib.cc
bool mtcs_cse_lib_fp_setter_insn (MtcsCseLib *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal =mtcs_target_get_rtlanal(mtcsTarget);

   rtx expr, pat = NULL_RTX;

   if (!RTX_FRAME_RELATED_P (insn))
      return false;

   expr = find_reg_note (insn, REG_FRAME_RELATED_EXPR, NULL_RTX);
   if (expr)
      pat = XEXP (expr, 0);
   if (!mtcs_rtlanal_modified_in_p/*!modified_in_p*/(mtcsRtlanal,
   mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL), pat ? pat : insn))
      return false;

   /* Don't return true for frame pointer restores in the epilogue.  */
   if (find_reg_note (insn, REG_CFA_RESTORE, mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL)))
      return false;
   return true;
}

/* V is one of the values in REG_VALUES (REGNO).  Return true if it
   would be invalidated by CALLEE_ABI.  */
static bool cselib_invalidated_by_call_p (MtcsCseLib *self,const mtcs_function_abi &callee_abi,
               unsigned int regno, cselib_val *v)
{
   machine_mode mode = GET_MODE (v->val_rtx);
   if (mode == VOIDmode){
      v = REG_VALUES (regno)->elt;
      if (!v)
         /* If we don't know what the mode of the constant value is, and we
         don't know what mode the register was set in, conservatively
         assume that the register is clobbered.  The value's going to be
         essentially useless in this case anyway.  */
         return true;
      mode = GET_MODE (v->val_rtx);
   }
   return callee_abi.clobbers_reg_p (mode, regno);
}

/* Record the effects of INSN.  */
//原型 cselib_process_insn cselib.h cselib.cc
void mtcs_cse_lib_cselib_process_insn (MtcsCseLib *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFuncAbi *mtcsFuncAbi =mtcs_target_get_func_abi(mtcsTarget);

   int i;
   rtx x;

   self->cselib_current_insn = insn;

   /* Forget everything at a CODE_LABEL or a setjmp.  */
   if ((LABEL_P (insn) || (CALL_P (insn) && find_reg_note (insn, REG_SETJMP, NULL)))
   && !self->cselib_preserve_constants){
      mtcs_cse_lib_cselib_reset_table/*!cselib_reset_table*/(self,self->next_uid);
      self->cselib_current_insn = NULL;
      return;
   }

   if (! INSN_P (insn)){
      self->cselib_current_insn = NULL;
      return;
   }

   /* If this is a call instruction, forget anything stored in a
   call clobbered register, or, if this is not a const call, in
   memory.  */
   if (CALL_P (insn)){
      mtcs_function_abi callee_abi = mtcs_func_abi_insn_callee_abi/*!insn_callee_abi*/(mtcsFuncAbi,insn);
      for (i = 0; i < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i++){
         elt_list **l = &REG_VALUES (i);
         while (*l){
            cselib_val *v = (*l)->elt;
            if (v && cselib_invalidated_by_call_p(self,callee_abi, i, v))
               cselib_invalidate_regno_val(self,i, l);
            else
               l = &(*l)->next;
         }
      }

      /* Since it is not clear how cselib is going to be used, be
      conservative here and treat looping pure or const functions
      as if they were regular functions.  */
      if (RTL_LOOPING_CONST_OR_PURE_CALL_P (insn) || !(RTL_CONST_OR_PURE_CALL_P (insn)))
         cselib_invalidate_mem(self,self->callmem);
      else
         /* For const/pure calls, invalidate any argument slots because
         they are owned by the callee.  */
         for (x = CALL_INSN_FUNCTION_USAGE (insn); x; x = XEXP (x, 1))
            if (GET_CODE (XEXP (x, 0)) == USE  && MEM_P (XEXP (XEXP (x, 0), 0)))
               cselib_invalidate_mem(self,XEXP (XEXP (x, 0), 0));
   }

   cselib_record_sets(self,insn);

   /* Look for any CLOBBERs in CALL_INSN_FUNCTION_USAGE, but only
   after we have processed the insn.  */
   if (CALL_P (insn)){
      for (x = CALL_INSN_FUNCTION_USAGE (insn); x; x = XEXP (x, 1))
         if (GET_CODE (XEXP (x, 0)) == CLOBBER)
            mtcs_cse_lib_cselib_invalidate_rtx/*!cselib_invalidate_rtx*/(self,XEXP (XEXP (x, 0), 0));

      /* Flush everything on setjmp.  */
      if (self->cselib_preserve_constants  && find_reg_note (insn, REG_SETJMP, NULL)){
         mtcs_cse_lib_cselib_preserve_only_values/*!cselib_preserve_only_values*/(self);
         mtcs_cse_lib_cselib_reset_table/*!cselib_reset_table*/(self,self->next_uid);
      }
   }

   /* On setter of the hard frame pointer if frame_pointer_needed,
   invalidate stack_pointer_rtx, so that sp and {,h}fp based
   VALUEs are distinct.  */
   if (reload_completed
   && mtcsRtlData->x_frame_pointer_needed/*!frame_pointer_needed*/
   && mtcs_cse_lib_fp_setter_insn/*!fp_setter_insn*/(self,insn))
      mtcs_cse_lib_cselib_invalidate_rtx/*!cselib_invalidate_rtx*/(self,mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL));

   self->cselib_current_insn = NULL;

   if (self->n_useless_values > MAX_USELESS_VALUES
   /* remove_useless_values is linear in the hash table size.  Avoid
   quadratic behavior for very large hashtables with very few
   useless elements.  */
   && ((unsigned int)self->n_useless_values > (self->cselib_hash_table->elements () - self->n_debug_values) / 4))
      remove_useless_values(self);
}

/* Initialize cselib for one pass.  The caller must also call
   init_alias_analysis.  */
//原型 cselib_init cselib.h cselib.cc
void mtcs_cse_lib_cselib_init (MtcsCseLib *self,int record_what)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   self->cselib_record_memory = record_what & CSELIB_RECORD_MEMORY;
   self->cselib_preserve_constants = record_what & CSELIB_PRESERVE_CONSTANTS;
   self->cselib_any_perm_equivs = false;

   /* (mem:BLK (scratch)) is a special mechanism to conflict with everything,
   see canon_true_dependence.  This is only created once.  */
   if (! self->callmem)
      self->callmem = gen_rtx_MEM (mtcsMode->modes.M_BLKmode, gen_rtx_SCRATCH (VOIDmode));

   self->cselib_nregs = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);

   /* We preserve self->reg_values to allow expensive clearing of the whole thing.
   Reallocate it however if it happens to be too large.  */
   if (!self->reg_values || self->reg_values_size < self->cselib_nregs
   || (self->reg_values_size > 10 && self->reg_values_size > self->cselib_nregs * 4)){
      free (self->reg_values);
      /* Some space for newly emit instructions so we don't end up
      reallocating in between passes.  */
      self->reg_values_size = self->cselib_nregs + (63 + self->cselib_nregs) / 16;
      self->reg_values = XCNEWVEC (struct elt_list *, self->reg_values_size);
   }
   self->used_regs = XNEWVEC (unsigned int, self->cselib_nregs);
   self->n_used_regs = 0;
   /* FIXME: enable sanitization (PR87845) */
   self->cselib_hash_table = new hash_table<mtcs_cselib_hasher> (31, /* ggc */ false,/* sanitize_eq_and_hash */ false);
   if (self->cselib_preserve_constants)
      self->cselib_preserved_hash_table  = new hash_table<mtcs_cselib_hasher> (31, /* ggc */ false, /* sanitize_eq_and_hash */ false);
   self->next_uid = 1;
}

/* Called when the current user is done with cselib.  */
//原型 cselib_finish cselib.h cselib.cc
void mtcs_cse_lib_cselib_finish (MtcsCseLib *self)
{
   bool preserved = self->cselib_preserve_constants;
   self->cselib_discard_hook/*!cselib_discard_hook*/ = NULL;
   self->cselib_preserve_constants = false;
   self->cselib_any_perm_equivs = false;
   self->cfa_base_preserved_val = NULL;
   self->cfa_base_preserved_regno = INVALID_REGNUM;
   self->elt_list_pool.release ();
   self->elt_loc_list_pool.release ();
   self->cselib_val_pool.release ();
   self->value_pool.release ();
   mtcs_cse_lib_cselib_clear_table(self);
   delete self->cselib_hash_table;
   self->cselib_hash_table = NULL;
   if (preserved)
      delete self->cselib_preserved_hash_table;
   self->cselib_preserved_hash_table = NULL;
   free (self->used_regs);
   self->used_regs = 0;
   self->n_useless_values = 0;
   self->n_useless_debug_values = 0;
   self->n_debug_values = 0;
   self->next_uid = 0;
}

/* Dump the cselib_val *X to FILE *OUT.  */
//原型 int dump_cselib_val (cselib_val **x, FILE *out) cselib.cc
static int dump_cselib_val (cselib_val **x, FILE *out)
{
   MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
   MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
   MtcsCseLib *self =mtcs_target_get_cse_lib(mtcsTarget);

   cselib_val *v = *x;
   bool need_lf = true;

   mtcs_print_inline_rtx/*!print_inline_rtx*/(out, v->val_rtx, 0);

   if (v->locs){
      struct elt_loc_list *l = v->locs;
      if (need_lf){
         fputc ('\n', out);
         need_lf = false;
      }
      fputs (" locs:", out);
      do{
         if (l->setting_insn)
            fprintf (out, "\n  from insn %i ",INSN_UID (l->setting_insn));
         else
            fprintf (out, "\n   ");
         mtcs_print_inline_rtx/*!print_inline_rtx*/(out, l->loc, 4);
      }while ((l = l->next));
      fputc ('\n', out);
   }else{
      fputs (" no locs", out);
      need_lf = true;
   }

   if (v->addr_list){
      struct elt_list *e = v->addr_list;
      if (need_lf){
         fputc ('\n', out);
         need_lf = false;
      }
      fputs (" addr list:", out);
      do{
      fputs ("\n  ", out);
      mtcs_print_inline_rtx/*!print_inline_rtx*/(out, e->elt->val_rtx, 2);
      }while ((e = e->next));
      fputc ('\n', out);
   }else{
      fputs (" no addrs", out);
      need_lf = true;
   }

   if (v->next_containing_mem == &self->dummy_val)
      fputs (" last mem\n", out);
   else if (v->next_containing_mem){
      fputs (" next mem ", out);
      mtcs_print_inline_rtx/*!print_inline_rtx*/(out, v->next_containing_mem->val_rtx, 2);
      fputc ('\n', out);
   }else if (need_lf)
      fputc ('\n', out);

   return 1;
}

/* Dump to OUT everything in the CSELIB table.  */
//原型 dump_cselib_table cselib.h cselib.cc
void mtcs_cse_lib_dump_cselib_table (MtcsCseLib *self,FILE *out)
{
   fprintf (out, "cselib hash table:\n");
   self->cselib_hash_table->traverse <FILE *, dump_cselib_val> (out);
   fprintf (out, "cselib preserved hash table:\n");
   self->cselib_preserved_hash_table->traverse <FILE *, dump_cselib_val> (out);
   if (self->first_containing_mem != &self->dummy_val){
      fputs ("first mem ", out);
      mtcs_print_inline_rtx/*!print_inline_rtx*/(out, self->first_containing_mem->val_rtx, 2);
      fputc ('\n', out);
   }
   fprintf (out, "next uid %i\n", self->next_uid);
}

static void mtcsCseLibInit(MtcsCseLib *self)
{
   self->elt_list_pool = object_allocator<elt_list>("mtcs_elt_list");
   self->elt_loc_list_pool =object_allocator<elt_loc_list>("mtcs_elt_loc_list");
   self->cselib_val_pool=object_allocator<cselib_val>("mtcs_cselib_val_list");
   self->value_pool= pool_allocator ("mtcs_value", RTX_CODE_SIZE (VALUE));
   self->first_containing_mem = &self->dummy_val;
}

MtcsCseLib *mtcs_cse_lib_new(MtcsMode *mtcsMode)
{
   MtcsCseLib *self = n_slice_alloc0 (sizeof(MtcsCseLib));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsCseLibInit(self);
   return self;
}
