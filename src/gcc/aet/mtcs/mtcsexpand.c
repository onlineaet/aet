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
 * base on cfgexpand.cc
 */


#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "cfghooks.h"
#include "tree-pass.h"
#include "memmodel.h"
#include "tm_p.h"
#include "ssa.h"
#include "optabs.h"
#include "regs.h" /* For reg_renumber.  */
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "fold-const.h"
#include "varasm.h"
#include "stor-layout.h"
#include "stmt.h"
#include "print-tree.h"
#include "cfgrtl.h"
#include "cfganal.h"
#include "cfgbuild.h"
#include "cfgcleanup.h"
#include "dojump.h"
#include "explow.h"
#include "calls.h"
#include "expr.h"
#include "internal-fn.h"
#include "tree-eh.h"
#include "gimple-iterator.h"
#include "gimple-expr.h"
#include "gimple-walk.h"
#include "tree-cfg.h"
#include "tree-dfa.h"
#include "tree-ssa.h"
#include "except.h"
#include "gimple-pretty-print.h"
#include "toplev.h"
#include "debug.h"
#include "tree-inline.h"
#include "value-prof.h"
#include "tree-ssa-live.h"
#include "tree-outof-ssa.h"
#include "cfgloop.h"
#include "insn-attr.h" /* For INSN_SCHEDULING.  */
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "tree-ssa-address.h"
#include "output.h"
#include "builtins.h"
#include "cfgexpand.h"
#include "rtl-iter.h"

#include "opts.h"

#include "aet/aetprinttree.h"
#include "mtcsexpand.h"
#include "mtcstool.h"
#include "mtcsmicro.h"
#include "mtcstarget.h"
#include "mtcscompile.h"
#include "mtcsprintrtl.h"

#include "aet/aetprintgimple.h"

static void testprint(struct function *fn);

static bool rtl_verify_bb_insn_chain (MtcsExpand *self);


#define SSAVAR(x) (TREE_CODE (x) == SSA_NAME ? SSA_NAME_VAR (x) : x)
#define EOC  ((size_t)-1)

#ifndef STACK_ALIGNMENT_NEEDED
#define STACK_ALIGNMENT_NEEDED 1
#endif

static void record_alignment_for_reg_var (MtcsExpand *self,unsigned int align);
static void set_rtl (MtcsExpand *self,tree t, rtx x);
static bool defer_stack_allocation (MtcsExpand *self,tree var, bool toplevel);
static void add_stack_var (MtcsExpand *self,tree decl, bool really_expand);
static void expand_one_stack_var_at (MtcsExpand *self,tree decl, rtx base, unsigned base_align,poly_int64 offset);
static unsigned int align_local_variable (MtcsExpand *self,tree decl, bool really_expand);
static poly_int64 align_frame_offset (MtcsExpand *self,unsigned HOST_WIDE_INT align);
static poly_int64 alloc_stack_frame_space (MtcsExpand *self,poly_int64 size, unsigned HOST_WIDE_INT align);
static void expand_one_stack_var_1 (MtcsExpand *self,tree var);
static void expand_one_stack_var (MtcsExpand *self,tree var);
static void partition_stack_vars (MtcsExpand *self);
static void add_stack_var_conflict (MtcsExpand *self,size_t x, size_t y);
static bool stack_var_conflict_p (MtcsExpand *self,size_t x, size_t y);
static void dump_stack_var_partition (MtcsExpand *self);
static void expand_stack_vars (MtcsExpand *self,bool (*pred) (MtcsExpand *,size_t), class stack_vars_data *data);
static int stack_protect_decl_phase (MtcsExpand *self,tree decl);
static void fini_vars_expansion (MtcsExpand *self);
static basic_block expand_gimple_cond (MtcsExpand *self,basic_block bb, gcond *stmt);
//原型 expand_null_return_1 cfgexpand.cc
static void expand_null_return_1 (MtcsExpand *self);
//原型 expand_debug_expr cfgexpand.cc
static rtx expand_debug_expr (MtcsExpand *self,tree exp);


class stack_vars_data
{
public:
  /* Vector of offset pairs, always end of some padding followed
     by start of the padding that needs Address Sanitizer protection.
     The vector is in reversed, highest offset pairs come first.  */
  auto_vec<HOST_WIDE_INT> asan_vec;

  /* Vector of partition representative decls in between the paddings.  */
  auto_vec<tree> asan_decl_vec;

  /* Base pseudo register for Address Sanitizer protected automatic vars.  */
  rtx asan_base;

  /* Alignment needed for the Address Sanitizer protected automatic vars.  */
  unsigned int asan_alignb;
};

/* Align given offset BASE with ALIGN.  Truncate up if ALIGN_UP is true,
   down otherwise.  Return truncated BASE value.  */

static inline unsigned HOST_WIDE_INT align_base (HOST_WIDE_INT base, unsigned HOST_WIDE_INT align, bool align_up)
{
  return align_up ? (base + align - 1) & -align : base & -align;
}

/* Two helper routines that check for phase 1 and phase 2.  These are used
   as callbacks for expand_stack_vars.  */

static bool stack_protect_decl_phase_1 (MtcsExpand *self,size_t i)
{
  return stack_protect_decl_phase (self,self->stack_vars[i].decl) == 1;
}

static bool stack_protect_decl_phase_2 (MtcsExpand *self,size_t i)
{
  return stack_protect_decl_phase(self,self->stack_vars[i].decl) == 2;
}

/* If we need to produce a detailed dump, print the tree representation
   for STMT to the dump file.  SINCE is the last RTX after which the RTL
   generated for STMT should have been appended.  */
//原型 maybe_dump_rtl_for_gimple_stmt cfgexpand.cc
static void maybe_dump_rtl_for_gimple_stmt (gimple *stmt, rtx_insn *since)
{
  if (dump_file && (dump_flags & TDF_DETAILS)){
      fprintf (dump_file, "\n;; ");
      print_gimple_stmt (dump_file, stmt, 0, TDF_SLIM | (dump_flags & TDF_LINENO));
      fprintf (dump_file, "\n");
      print_rtl (dump_file, since ? NEXT_INSN (since) : since);
  }
}

/* And helper function that checks for asan phase (with stack protector
   it is phase 3).  This is used as callback for expand_stack_vars.
   Returns true if any of the vars in the partition need to be protected.  */

static bool asan_decl_phase_3 (MtcsExpand *self,size_t i)
{
  while (i != EOC){
      if (asan_protect_stack_decl (self->stack_vars[i].decl))
          return true;
      i = self->stack_vars[i].next;
  }
  return false;
}

/* Free up stack variable graph data.  */
static void fini_vars_expansion (MtcsExpand *self)
{
  bitmap_obstack_release (&self->stack_var_bitmap_obstack);
  if (self->stack_vars)
    XDELETEVEC (self->stack_vars);
  if (self->stack_vars_sorted)
    XDELETEVEC (self->stack_vars_sorted);
  self->stack_vars = NULL;
  self->stack_vars_sorted = NULL;
  self->stack_vars_alloc = self->stack_vars_num = 0;
  delete self->decl_to_stack_part;
  self->decl_to_stack_part = NULL;
}

/* A subroutine of expand_used_vars.  Give each partition representative
   a unique location within the stack frame.  Update each partition member
   with that location.  */
static void expand_stack_vars (MtcsExpand *self,bool (*pred) (MtcsExpand *,size_t), class stack_vars_data *data)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

  size_t si, i, j, n = self->stack_vars_num;
  poly_uint64 large_size = 0, large_alloc = 0;
  rtx large_base = NULL;
  rtx large_untagged_base = NULL;
  unsigned large_align = 0;
  bool large_allocation_done = false;
  tree decl;

  /* Determine if there are any variables requiring "large" alignment.
     Since these are dynamically allocated, we only process these if
     no predicate involved.  */
  int maxSupportStackAlignment=mtcs_func_get_max_support_stack_alignment/*!MAX_SUPPORTED_STACK_ALIGNMENT*/(mtcsFunc);
  large_align = self->stack_vars[self->stack_vars_sorted[0]].alignb * BITS_PER_UNIT;
  n_debug("mtcsexpand.c expand_stack_vars 00");
  if (pred == NULL && large_align > maxSupportStackAlignment){
     n_debug("mtcsexpand.c expand_stack_vars 11");

      /* Find the total size of these variables.  */
      for (si = 0; si < n; ++si){
         n_debug("mtcsexpand.c expand_stack_vars 22");

          unsigned alignb;

          i = self->stack_vars_sorted[si];
          alignb = self->stack_vars[i].alignb;

          /* All "large" alignment decls come before all "small" alignment
             decls, but "large" alignment decls are not sorted based on
             their alignment.  Increase large_align to track the largest
             required alignment.  */
          if ((alignb * BITS_PER_UNIT) > large_align)
            large_align = alignb * BITS_PER_UNIT;

          /* Stop when we get to the first decl with "small" alignment.  */
          if (alignb * BITS_PER_UNIT <= maxSupportStackAlignment)
            break;

          /* Skip variables that aren't partition representatives.  */
          if (self->stack_vars[i].representative != i)
            continue;

          /* Skip variables that have already had rtl assigned.  See also
             add_stack_var where we perpetrate this pc_rtx hack.  */
          decl = self->stack_vars[i].decl;
          if (TREE_CODE (decl) == SSA_NAME
                  ? SA.partition_to_pseudo[var_to_partition (SA.map, decl)] != NULL_RTX :
                        mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl) != pc_rtx)
            continue;

          large_size = aligned_upper_bound (large_size, alignb);
          large_size += self->stack_vars[i].size;
       }
  }

  for (si = 0; si < n; ++si){
     n_debug("mtcsexpand.c expand_stack_vars 33");

      rtx base;
      unsigned base_align, alignb;
      poly_int64 offset = 0;

      i = self->stack_vars_sorted[si];

      /* Skip variables that aren't partition representatives, for now.  */
      if (self->stack_vars[i].representative != i)
          continue;

      /* Skip variables that have already had rtl assigned.  See also
     add_stack_var where we perpetrate this pc_rtx hack.  */
      decl = self->stack_vars[i].decl;
      if (TREE_CODE (decl) == SSA_NAME
          ? SA.partition_to_pseudo[var_to_partition (SA.map, decl)] != NULL_RTX
          : mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl) != pc_rtx)
          continue;

      /* Check the predicate to see whether this variable should be
     allocated in this pass.  */
      if (pred && !pred (self,i))
          continue;

      base = (hwasan_sanitize_stack_p () ? hwasan_frame_base (): mtcs_rtl_get_virtual_stack_args_rtx/*!virtual_stack_vars_rtx*/(mtcsRTL));
      alignb = self->stack_vars[i].alignb;
      if (alignb * BITS_PER_UNIT <= maxSupportStackAlignment){
         n_debug("mtcsexpand.c expand_stack_vars 44");

          poly_int64 hwasan_orig_offset;
          if (hwasan_sanitize_stack_p ()){
             n_debug("mtcsexpand.c expand_stack_vars 55");

              /* There must be no tag granule "shared" between different
             objects.  This means that no HWASAN_TAG_GRANULE_SIZE byte
             chunk can have more than one object in it.

             We ensure this by forcing the end of the last bit of data to
             be aligned to HWASAN_TAG_GRANULE_SIZE bytes here, and setting
             the start of each variable to be aligned to
             HWASAN_TAG_GRANULE_SIZE bytes in `align_local_variable`.

             We can't align just one of the start or end, since there are
             untagged things stored on the stack which we do not align to
             HWASAN_TAG_GRANULE_SIZE bytes.  If we only aligned the start
             or the end of tagged objects then untagged objects could end
             up sharing the first granule of a tagged object or sharing the
             last granule of a tagged object respectively.  */
              hwasan_orig_offset = align_frame_offset (self,HWASAN_TAG_GRANULE_SIZE);
              gcc_assert (self->stack_vars[i].alignb >= HWASAN_TAG_GRANULE_SIZE);
          }
          /* ASAN description strings don't yet have a syntax for expressing
             polynomial offsets.  */
          HOST_WIDE_INT prev_offset;
          n_debug("mtcsexpand.c expand_stack_vars 55aa");

          if (asan_sanitize_stack_p () && pred
                && mtcsRtlData->x_frame_offset.is_constant (&prev_offset) && self->stack_vars[i].size.is_constant ()){
             n_debug("mtcsexpand.c expand_stack_vars 66");

              if (data->asan_vec.is_empty ()){
                  align_frame_offset (self,ASAN_RED_ZONE_SIZE);
                  prev_offset = mtcsRtlData->x_frame_offset.to_constant ();
              }
              prev_offset = align_base (prev_offset,ASAN_MIN_RED_ZONE_SIZE,!mtcs_func_get_frame_grows_downward/*!FRAME_GROWS_DOWNWARD*/(mtcsFunc));
              tree repr_decl = NULL_TREE;
              unsigned HOST_WIDE_INT size = asan_var_and_redzone_size (self->stack_vars[i].size.to_constant ());
              if (data->asan_vec.is_empty ())
                  size = MAX (size, ASAN_RED_ZONE_SIZE);

              unsigned HOST_WIDE_INT alignment = MAX (alignb,ASAN_MIN_RED_ZONE_SIZE);
              offset = alloc_stack_frame_space (self,size, alignment);
              data->asan_vec.safe_push (prev_offset);
              /* Allocating a constant amount of space from a constant
             starting offset must give a constant result.  */
              data->asan_vec.safe_push ((offset + self->stack_vars[i].size).to_constant ());
              /* Find best representative of the partition.
             Prefer those with DECL_NAME, even better
             satisfying asan_protect_stack_decl predicate.  */
              for (j = i; j != EOC; j = self->stack_vars[j].next)
                if (asan_protect_stack_decl (self->stack_vars[j].decl)  && DECL_NAME (self->stack_vars[j].decl)){
                    repr_decl = self->stack_vars[j].decl;
                    break;
                }else if (repr_decl == NULL_TREE && DECL_P (self->stack_vars[j].decl) && DECL_NAME (self->stack_vars[j].decl))
                  repr_decl = self->stack_vars[j].decl;

              if (repr_decl == NULL_TREE)
                  repr_decl = self->stack_vars[i].decl;
              data->asan_decl_vec.safe_push (repr_decl);

              /* Make sure a representative is unpoison if another
             variable in the partition is handled by
             use-after-scope sanitization.  */
              if (asan_handled_variables != NULL && !asan_handled_variables->contains (repr_decl)){
                  for (j = i; j != EOC; j = self->stack_vars[j].next)
                    if (asan_handled_variables->contains (self->stack_vars[j].decl))
                      break;
                  if (j != EOC)
                    asan_handled_variables->add (repr_decl);
              }
              n_debug("mtcsexpand.c expand_stack_vars 77");

              data->asan_alignb = MAX (data->asan_alignb, alignb);
              if (data->asan_base == NULL)
                  data->asan_base = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mtcs_mode_get_Pmode(mtcsMode));
              base = data->asan_base;

              if (!STRICT_ALIGNMENT)
                  base_align = mtcsRtlData/*!crtl*/->max_used_stack_slot_alignment;
              else
                  base_align = MAX (mtcsRtlData/*!crtl*/->max_used_stack_slot_alignment,
                      mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mtcsMode->modes.M_SImode)<< ASAN_SHADOW_SHIFT);
            }else{
               n_debug("mtcsexpand.c expand_stack_vars 88");

               offset = alloc_stack_frame_space (self,self->stack_vars[i].size, alignb);
               n_debug("mtcsexpand.c expand_stack_vars 88aa");

               base_align = mtcsRtlData/*!crtl*/->max_used_stack_slot_alignment;
               if (hwasan_sanitize_stack_p ()){
                  /* Align again since the point of this alignment is to handle
                     the "end" of the object (i.e. smallest address after the
                     stack object).  For FRAME_GROWS_DOWNWARD that requires
                     aligning the stack before allocating, but for a frame that
                     grows upwards that requires aligning the stack after
                     allocation.

                     Use `frame_offset` to record the offset value rather than
                     `offset` since the `frame_offset` describes the extent
                     allocated for this particular variable while `offset`
                     describes the address that this variable starts at.  */
                  n_debug("mtcsexpand.c expand_stack_vars 88bb");

                  align_frame_offset (self,HWASAN_TAG_GRANULE_SIZE);
                  hwasan_record_stack_var (mtcs_rtl_get_virtaul_stack_var_rtx/*!virtual_stack_vars_rtx*/(mtcsRTL),
                          base,hwasan_orig_offset, mtcsRtlData->x_frame_offset);
               }
               n_debug("mtcsexpand.c expand_stack_vars 88cc");

            }
      }else{
          /* Large alignment is only processed in the last pass.  */
          if (pred)
            continue;
          n_debug("mtcsexpand.c expand_stack_vars 99");

          /* If there were any variables requiring "large" alignment, allocate
             space.  */
          if (maybe_ne (large_size, 0U) && ! large_allocation_done){
              poly_int64 loffset;
              rtx large_allocsize;

              large_allocsize =mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,large_size, Pmode);
              mtcs_explow_get_dynamic_stack_size/*!get_dynamic_stack_size*/(mtcsExplow,&large_allocsize, 0, large_align, NULL);
              loffset = alloc_stack_frame_space(self,rtx_to_poly_int64 (large_allocsize),
                      mtcs_func_get_preferred_stack_boundary/*!PREFERRED_STACK_BOUNDARY*/(mtcsFunc) / BITS_PER_UNIT);
              large_base = get_dynamic_stack_base (loffset, large_align, base);
              large_allocation_done = true;
          }

          gcc_assert (large_base != NULL);
          large_alloc = aligned_upper_bound (large_alloc, alignb);
          offset = large_alloc;
          large_alloc += self->stack_vars[i].size;
          if (hwasan_sanitize_stack_p ()){
              /* An object with a large alignment requirement means that the
             alignment requirement is greater than the required alignment
             for tags.  */
              if (!large_untagged_base)
                  large_untagged_base = targetm.memtag.untagged_pointer (large_base, NULL_RTX);
              /* Ensure the end of the variable is also aligned correctly.  */
              poly_int64 align_again= aligned_upper_bound (large_alloc, HWASAN_TAG_GRANULE_SIZE);
              /* For large allocations we always allocate a chunk of space
             (which is addressed by large_untagged_base/large_base) and
             then use positive offsets from that.  Hence the farthest
             offset is `align_again` and the nearest offset from the base
             is `offset`.  */
              hwasan_record_stack_var (large_untagged_base, large_base,offset, align_again);
          }

          base = large_base;
          base_align = large_align;
      }
      n_debug("mtcsexpand.c expand_stack_vars 99");

      /* Create rtl for each variable based on their location within the
     partition.  */
      for (j = i; j != EOC; j = self->stack_vars[j].next){
          expand_one_stack_var_at (self,self->stack_vars[j].decl,base, base_align, offset);
      }
      if (hwasan_sanitize_stack_p ())
          hwasan_increment_frame_tag ();
  }//end for
  gcc_assert (known_eq (large_alloc, large_size));
}


/* Create a decl for the guard at the top of the stack frame.  */
static void create_stack_guard (MtcsExpand *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsTree *mtcsTree = mtcs_target_get_tree(mtcsTarget);

  tree guard = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,DECL_SOURCE_LOCATION (current_function_decl),VAR_DECL, NULL, ptr_type_node);
  TREE_THIS_VOLATILE (guard) = 1;
  TREE_USED (guard) = 1;
  expand_one_stack_var (self,guard);
  mtcsRtlData/*!crtl*/->stack_protect_guard = guard;
  mtcsRtlData->stack_protect_guard=guard;
}

//原型 avoid_deep_ter_for_debug cfgexpand.cc
static void avoid_deep_ter_for_debug (MtcsExpand *self,gimple *stmt, int depth)
{
  use_operand_p use_p;
  ssa_op_iter iter;
  FOR_EACH_SSA_USE_OPERAND (use_p, stmt, iter, SSA_OP_USE){
      tree use = USE_FROM_PTR (use_p);
      if (TREE_CODE (use) != SSA_NAME || SSA_NAME_IS_DEFAULT_DEF (use))
          continue;
      gimple *g = get_gimple_for_ssa_name (use);
      if (g == NULL)
          continue;
      if (depth > 6 && !stmt_ends_bb_p (g)){
          if (self->deep_ter_debug_map == NULL)
            self->deep_ter_debug_map = new hash_map<tree, tree>;

          tree &vexpr = self->deep_ter_debug_map->get_or_insert (use);
          if (vexpr != NULL)
            continue;
          vexpr = build_debug_expr_decl (TREE_TYPE (use));
          gimple *def_temp = gimple_build_debug_bind (vexpr, use, g);
          gimple_stmt_iterator gsi = gsi_for_stmt (g);
          gsi_insert_after (&gsi, def_temp, GSI_NEW_STMT);
          avoid_deep_ter_for_debug(self,def_temp, 0);
      }else
          avoid_deep_ter_for_debug(self,g, depth + 1);
  }
}

/* A subroutine of partition_stack_vars.  A comparison function for qsort,
   sorting an array of indices by the properties of the object.  */

static int stack_var_cmp (const void *a, const void *b)
{
  MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
  MtcsExpand *self=mtcs_target_get_expand(mtcsTarget);
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  size_t ia = *(const size_t *)a;
  size_t ib = *(const size_t *)b;
  unsigned int aligna = self->stack_vars[ia].alignb;
  unsigned int alignb = self->stack_vars[ib].alignb;
  poly_int64 sizea = self->stack_vars[ia].size;
  poly_int64 sizeb = self->stack_vars[ib].size;
  tree decla = self->stack_vars[ia].decl;
  tree declb = self->stack_vars[ib].decl;
  bool largea, largeb;
  unsigned int uida, uidb;

  /* Primary compare on "large" alignment.  Large comes first.  */
  largea = (aligna * BITS_PER_UNIT > mtcs_func_get_max_support_stack_alignment/*!MAX_SUPPORTED_STACK_ALIGNMENT*/(mtcsFunc));
  largeb = (alignb * BITS_PER_UNIT > mtcs_func_get_max_support_stack_alignment/*!MAX_SUPPORTED_STACK_ALIGNMENT*/(mtcsFunc));
  if (largea != largeb)
    return (int)largeb - (int)largea;

  /* Secondary compare on size, decreasing  */
  int diff = compare_sizes_for_sort (sizeb, sizea);
  if (diff != 0)
    return diff;

  /* Tertiary compare on true alignment, decreasing.  */
  if (aligna < alignb)
    return -1;
  if (aligna > alignb)
    return 1;

  /* Final compare on ID for sort stability, increasing.
     Two SSA names are compared by their version, SSA names come before
     non-SSA names, and two normal decls are compared by their DECL_UID.  */
  if (TREE_CODE (decla) == SSA_NAME){
      if (TREE_CODE (declb) == SSA_NAME)
          uida = SSA_NAME_VERSION (decla), uidb = SSA_NAME_VERSION (declb);
      else
          return -1;
  }else if (TREE_CODE (declb) == SSA_NAME)
    return 1;
  else
    uida = DECL_UID (decla), uidb = DECL_UID (declb);
  if (uida < uidb)
    return 1;
  if (uida > uidb)
    return -1;
  return 0;
}

struct part_traits : unbounded_int_hashmap_traits <size_t, bitmap> {};
typedef hash_map<size_t, bitmap, part_traits> part_hashmap;

/* If the points-to solution *PI points to variables that are in a partition
   together with other variables add all partition members to the pointed-to
   variables bitmap.  */
static void add_partitioned_vars_to_ptset (MtcsExpand *self,struct pt_solution *pt,
                   part_hashmap *decls_to_partitions, hash_set<bitmap> *visited, bitmap temp)
{
  bitmap_iterator bi;
  unsigned i;
  bitmap *part;

  if (pt->anything || pt->vars == NULL
      /* The pointed-to vars bitmap is shared, it is enough to
     visit it once.  */
      || visited->add (pt->vars))
    return;

  bitmap_clear (temp);

  /* By using a temporary bitmap to store all members of the partitions
     we have to add we make sure to visit each of the partitions only
     once.  */
  EXECUTE_IF_SET_IN_BITMAP (pt->vars, 0, i, bi)
    if ((!temp || !bitmap_bit_p (temp, i)) && (part = decls_to_partitions->get (i)))
      bitmap_ior_into (temp, *part);
  if (!bitmap_empty_p (temp))
    bitmap_ior_into (pt->vars, temp);
}

/* Update points-to sets based on partition info, so we can use them on RTL.
   The bitmaps representing stack partitions will be saved until expand,
   where partitioned decls used as bases in memory expressions will be
   rewritten.

   It is not necessary to update TBAA info on accesses to the coalesced
   storage since our memory model doesn't allow TBAA to be used for
   WAW or WAR dependences.  For RAW when the write is to an old object
   the new object would not have been initialized at the point of the
   read, invoking undefined behavior.  */

static void update_alias_info_with_stack_vars (MtcsExpand *self)
{
  part_hashmap *decls_to_partitions = NULL;
  size_t i, j;
  tree var = NULL_TREE;

  for (i = 0; i < self->stack_vars_num; i++){
      bitmap part = NULL;
      tree name;
      struct ptr_info_def *pi;
      /* Not interested in partitions with single variable.  */
      if (self->stack_vars[i].representative != i  || self->stack_vars[i].next == EOC)
        continue;

      if (!decls_to_partitions){
          decls_to_partitions = new part_hashmap;
          cfun->gimple_df->decls_to_pointers = new hash_map<tree, tree>;
      }

      /* Create an SSA_NAME that points to the partition for use
         as base during alias-oracle queries on RTL for bases that
     have been partitioned.  */
      if (var == NULL_TREE)
          var = create_tmp_var (ptr_type_node);
      name = make_ssa_name (var);

      /* Create bitmaps representing partitions.  They will be used for
         points-to sets later, so use GGC alloc.  */
      part = BITMAP_GGC_ALLOC ();
      for (j = i; j != EOC; j = self->stack_vars[j].next){
          tree decl = self->stack_vars[j].decl;
          unsigned int uid = DECL_PT_UID (decl);
          bitmap_set_bit (part, uid);
          decls_to_partitions->put (uid, part);
          cfun->gimple_df->decls_to_pointers->put (decl, name);
          if (TREE_ADDRESSABLE (decl))
            TREE_ADDRESSABLE (name) = 1;
      }

      /* Make the SSA name point to all partition members.  */
      pi = get_ptr_info (name);
      pt_solution_set (&pi->pt, part, false);
  }

  /* Make all points-to sets that contain one member of a partition
     contain all members of the partition.  */
  if (decls_to_partitions){
      unsigned i;
      tree name;
      hash_set<bitmap> visited;
      bitmap temp = BITMAP_ALLOC (&self->stack_var_bitmap_obstack);

      FOR_EACH_SSA_NAME (i, name, cfun){
          struct ptr_info_def *pi;

          if (POINTER_TYPE_P (TREE_TYPE (name))  && ((pi = SSA_NAME_PTR_INFO (name)) != NULL))
            add_partitioned_vars_to_ptset (self,&pi->pt, decls_to_partitions, &visited, temp);
       }

      add_partitioned_vars_to_ptset (self,&cfun->gimple_df->escaped, decls_to_partitions, &visited, temp);
      add_partitioned_vars_to_ptset (self,&cfun->gimple_df->escaped_return, decls_to_partitions, &visited, temp);
      delete decls_to_partitions;
      BITMAP_FREE (temp);
  }
}



/* A subroutine of partition_stack_vars.  The UNION portion of a UNION/FIND
   partitioning algorithm.  Partitions A and B are known to be non-conflicting.
   Merge them into a single partition A.  */

static void union_stack_vars (MtcsExpand *self,size_t a, size_t b)
{
  StackVar  *vb = &self->stack_vars[b];
  bitmap_iterator bi;
  unsigned u;

  gcc_assert (self->stack_vars[b].next == EOC);
   /* Add B to A's partition.  */
  self->stack_vars[b].next = self->stack_vars[a].next;
  self->stack_vars[b].representative = a;
  self->stack_vars[a].next = b;

  /* Make sure A is big enough to hold B.  */
  self->stack_vars[a].size = upper_bound (self->stack_vars[a].size, self->stack_vars[b].size);

  /* Update the required alignment of partition A to account for B.  */
  if (self->stack_vars[a].alignb < self->stack_vars[b].alignb)
      self->stack_vars[a].alignb = self->stack_vars[b].alignb;

  /* Update the interference graph and merge the conflicts.  */
  if (vb->conflicts) {
      EXECUTE_IF_SET_IN_BITMAP (vb->conflicts, 0, u, bi)
            add_stack_var_conflict (self,a, self->stack_vars[u].representative);
      BITMAP_FREE (vb->conflicts);
  }
}

static void partition_stack_vars (MtcsExpand *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  size_t si, sj, n = self->stack_vars_num;

  self->stack_vars_sorted = XNEWVEC (size_t, self->stack_vars_num);
  for (si = 0; si < n; ++si)
    self->stack_vars_sorted[si] = si;

  if (n == 1)
    return;

  qsort (self->stack_vars_sorted, n, sizeof (size_t), stack_var_cmp);
  int maxSupportStackAlignment=mtcs_func_get_max_support_stack_alignment/*!MAX_SUPPORTED_STACK_ALIGNMENT*/(mtcsFunc);
  for (si = 0; si < n; ++si){
      size_t i = self->stack_vars_sorted[si];
      unsigned int ialign = self->stack_vars[i].alignb;
      poly_int64 isize = self->stack_vars[i].size;

      /* Ignore objects that aren't partition representatives. If we
         see a var that is not a partition representative, it must
         have been merged earlier.  */
      if (self->stack_vars[i].representative != i)
        continue;

      for (sj = si + 1; sj < n; ++sj){
          size_t j = self->stack_vars_sorted[sj];
          unsigned int jalign = self->stack_vars[j].alignb;
          poly_int64 jsize = self->stack_vars[j].size;

          /* Ignore objects that aren't partition representatives.  */
          if (self->stack_vars[j].representative != j)
            continue;

          /* Do not mix objects of "small" (supported) alignment
             and "large" (unsupported) alignment.  */
          if ((ialign * BITS_PER_UNIT <= maxSupportStackAlignment/*!MAX_SUPPORTED_STACK_ALIGNMENT*/)
              != (jalign * BITS_PER_UNIT <= maxSupportStackAlignment/*!MAX_SUPPORTED_STACK_ALIGNMENT*/))
            break;

          /* For Address Sanitizer do not mix objects with different
             sizes, as the shorter vars wouldn't be adequately protected.
             Don't do that for "large" (unsupported) alignment objects,
             those aren't protected anyway.  */
          if (asan_sanitize_stack_p () && maybe_ne (isize, jsize)
              && ialign * BITS_PER_UNIT <= maxSupportStackAlignment/*!MAX_SUPPORTED_STACK_ALIGNMENT*/)
            break;

          /* Ignore conflicting objects.  */
          if (stack_var_conflict_p (self,i, j))
            continue;

          /* UNION the objects, placing J at OFFSET.  */
          union_stack_vars (self,i, j);
      }
  }

  update_alias_info_with_stack_vars (self);
}

/* Make the decls associated with luid's X and Y conflict.  */

static void add_stack_var_conflict (MtcsExpand *self,size_t x, size_t y)
{
  StackVar *a = &self->stack_vars[x];
  StackVar *b = &self->stack_vars[y];
  if (x == y)
    return;
  if (!a->conflicts)
    a->conflicts = BITMAP_ALLOC (&self->stack_var_bitmap_obstack);
  if (!b->conflicts)
    b->conflicts = BITMAP_ALLOC (&self->stack_var_bitmap_obstack);
  bitmap_set_bit (a->conflicts, y);
  bitmap_set_bit (b->conflicts, x);
}

/* Check whether the decls associated with luid's X and Y conflict.  */

static bool stack_var_conflict_p (MtcsExpand *self,size_t x, size_t y)
{
  StackVar *a = &self->stack_vars[x];
  StackVar *b = &self->stack_vars[y];
  if (x == y)
    return false;
  /* Partitions containing an SSA name result from gimple registers
     with things like unsupported modes.  They are top-level and
     hence conflict with everything else.  */
  if (TREE_CODE (a->decl) == SSA_NAME || TREE_CODE (b->decl) == SSA_NAME)
    return true;

  if (!a->conflicts || !b->conflicts)
    return false;
  return bitmap_bit_p (a->conflicts, y);
}

/* Callback for walk_stmt_ops.  If OP is a decl touched by add_stack_var
   enter its partition number into bitmap DATA.  */
typedef struct _VisitOpData
{
   MtcsExpand *mtcsExpand;
   bitmap work;
}VisitOpData;

static bool visit_op_cb (gimple *, tree op, tree, void *data)
{
  VisitOpData *userData=(VisitOpData *)data;
  MtcsExpand *self=userData->mtcsExpand;
  bitmap active = (bitmap)userData->work;
  op = get_base_address (op);
  if (op  && DECL_P (op) && DECL_RTL_IF_SET (op) == pc_rtx){
      size_t *v = self->decl_to_stack_part->get (op);
      if (v)
          bitmap_set_bit (active, *v);
  }
  return false;
}

/* Callback for walk_stmt_ops.  If OP is a decl touched by add_stack_var
   record conflicts between it and all currently active other partitions
   from bitmap DATA.  */

static bool visit_conflict_cb (gimple *, tree op, tree, void *data)
{
  VisitOpData *userData=(VisitOpData *)data;
  MtcsExpand *self=userData->mtcsExpand;
  bitmap active = (bitmap)userData->work;
  op = get_base_address (op);
  if (op  && DECL_P (op) && DECL_RTL_IF_SET (op) == pc_rtx){
      size_t *v = self->decl_to_stack_part->get (op);
      if (v && bitmap_set_bit (active, *v)){
          size_t num = *v;
          bitmap_iterator bi;
          unsigned i;
          gcc_assert (num < self->stack_vars_num);
          EXECUTE_IF_SET_IN_BITMAP (active, 0, i, bi)
            add_stack_var_conflict (self,num, i);
      }
  }
  return false;
}

/* Helper function for add_scope_conflicts_1.  For USE on
   a stmt, if it is a SSA_NAME and in its SSA_NAME_DEF_STMT is known to be
   based on some ADDR_EXPR, invoke VISIT on that ADDR_EXPR.  */

static inline void add_scope_conflicts_2 (MtcsExpand *self, tree use, bitmap work,
               walk_stmt_load_store_addr_fn visit)
{
  if (TREE_CODE (use) == SSA_NAME   && (POINTER_TYPE_P (TREE_TYPE (use))
      || INTEGRAL_TYPE_P (TREE_TYPE (use)))){
      gimple *g = SSA_NAME_DEF_STMT (use);
      VisitOpData userData={self,work};

      if (is_gimple_assign (g))
        if (tree op = gimple_assign_rhs1 (g))
          if (TREE_CODE (op) == ADDR_EXPR)
            visit (g, TREE_OPERAND (op, 0), op, (void *)&userData/*!work*/);
  }
}

/* Helper routine for add_scope_conflicts, calculating the active partitions
   at the end of BB, leaving the result in WORK.  We're called to generate
   conflicts when FOR_CONFLICT is true, otherwise we're just tracking
   liveness.  */

static void add_scope_conflicts_1 (MtcsExpand *self,basic_block bb, bitmap work, bool for_conflict)
{
  edge e;
  edge_iterator ei;
  gimple_stmt_iterator gsi;
  walk_stmt_load_store_addr_fn visit;
  use_operand_p use_p;
  ssa_op_iter iter;

  bitmap_clear (work);
  FOR_EACH_EDGE (e, ei, bb->preds)
    bitmap_ior_into (work, (bitmap)e->src->aux);

  visit = visit_op_cb;
  VisitOpData userData={self,work};

  for (gsi = gsi_start_phis (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
      gimple *stmt = gsi_stmt (gsi);
      gphi *phi = as_a <gphi *> (stmt);
      walk_stmt_load_store_addr_ops (stmt, &userData/*!work*/, NULL, NULL, visit);
      FOR_EACH_PHI_ARG (use_p, phi, iter, SSA_OP_USE)
         add_scope_conflicts_2 (self,USE_FROM_PTR (use_p), work, visit);
  }
  for (gsi = gsi_after_labels (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
      gimple *stmt = gsi_stmt (gsi);

      if (gimple_clobber_p (stmt)){
          tree lhs = gimple_assign_lhs (stmt);
          size_t *v;
          /* Nested function lowering might introduce LHSs
             that are COMPONENT_REFs.  */
          if (!VAR_P (lhs))
            continue;
          if (DECL_RTL_IF_SET (lhs) == pc_rtx
              && (v = self->decl_to_stack_part->get (lhs)))
            bitmap_clear_bit (work, *v);
      }else if (!is_gimple_debug (stmt)){
          if (for_conflict && visit == visit_op_cb){
              /* If this is the first real instruction in this BB we need
                 to add conflicts for everything live at this point now.
             Unlike classical liveness for named objects we can't
             rely on seeing a def/use of the names we're interested in.
             There might merely be indirect loads/stores.  We'd not add any
             conflicts for such partitions.  */
              bitmap_iterator bi;
              unsigned i;
              EXECUTE_IF_SET_IN_BITMAP (work, 0, i, bi){
                  StackVar *a = &self->stack_vars[i];
                  if (!a->conflicts)
                      a->conflicts = BITMAP_ALLOC (&self->stack_var_bitmap_obstack);
                  bitmap_ior_into (a->conflicts, work);
              }
              visit = visit_conflict_cb;
          }
          walk_stmt_load_store_addr_ops (stmt, &userData/*!work*/, visit, visit, visit);
          FOR_EACH_SSA_USE_OPERAND (use_p, stmt, iter, SSA_OP_USE)
            add_scope_conflicts_2 (self,USE_FROM_PTR (use_p), work, visit);
      }
  }
}


/* Generate stack partition conflicts between all partitions that are
   simultaneously live.  */

static void add_scope_conflicts (MtcsExpand *self)
{
  basic_block bb;
  bool changed;
  bitmap work = BITMAP_ALLOC (NULL);
  int *rpo;
  int n_bbs;

  /* We approximate the live range of a stack variable by taking the first
     mention of its name as starting point(s), and by the end-of-scope
     death clobber added by gimplify as ending point(s) of the range.
     This overapproximates in the case we for instance moved an address-taken
     operation upward, without also moving a dereference to it upwards.
     But it's conservatively correct as a variable never can hold values
     before its name is mentioned at least once.

     We then do a mostly classical bitmap liveness algorithm.  */

  FOR_ALL_BB_FN (bb, cfun)
    bb->aux = BITMAP_ALLOC (&self->stack_var_bitmap_obstack);

  rpo = XNEWVEC (int, last_basic_block_for_fn (cfun));
  n_bbs = pre_and_rev_post_order_compute (NULL, rpo, false);

  changed = true;
  while (changed){
      int i;
      changed = false;
      for (i = 0; i < n_bbs; i++){
          bitmap active;
          bb = BASIC_BLOCK_FOR_FN (cfun, rpo[i]);
          active = (bitmap)bb->aux;
          add_scope_conflicts_1 (self,bb, work, false);
          if (bitmap_ior_into (active, work))
            changed = true;
      }
  }

  FOR_EACH_BB_FN (bb, cfun)
    add_scope_conflicts_1 (self,bb, work, true);

  free (rpo);
  BITMAP_FREE (work);
  FOR_ALL_BB_FN (bb, cfun)
    BITMAP_FREE (bb->aux);
}


/* Examine TYPE and determine a bit mask of the following features.  */

#define SPCT_HAS_LARGE_CHAR_ARRAY   1
#define SPCT_HAS_SMALL_CHAR_ARRAY   2
#define SPCT_HAS_ARRAY          4
#define SPCT_HAS_AGGREGATE      8

static unsigned int stack_protect_classify_type (tree type)
{
  unsigned int ret = 0;
  tree t;

  switch (TREE_CODE (type))
    {
    case ARRAY_TYPE:
      t = TYPE_MAIN_VARIANT (TREE_TYPE (type));
      if (t == char_type_node
      || t == signed_char_type_node
      || t == unsigned_char_type_node)
    {
      unsigned HOST_WIDE_INT max = param_ssp_buffer_size;
      unsigned HOST_WIDE_INT len;

      if (!TYPE_SIZE_UNIT (type)
          || !tree_fits_uhwi_p (TYPE_SIZE_UNIT (type)))
        len = max;
      else
        len = tree_to_uhwi (TYPE_SIZE_UNIT (type));

      if (len < max)
        ret = SPCT_HAS_SMALL_CHAR_ARRAY | SPCT_HAS_ARRAY;
      else
        ret = SPCT_HAS_LARGE_CHAR_ARRAY | SPCT_HAS_ARRAY;
    }
      else
    ret = SPCT_HAS_ARRAY;
      break;

    case UNION_TYPE:
    case QUAL_UNION_TYPE:
    case RECORD_TYPE:
      ret = SPCT_HAS_AGGREGATE;
      for (t = TYPE_FIELDS (type); t ; t = TREE_CHAIN (t))
    if (TREE_CODE (t) == FIELD_DECL)
      ret |= stack_protect_classify_type (TREE_TYPE (t));
      break;

    default:
      break;
    }

  return ret;
}


/* Return nonzero if DECL should be segregated into the "vulnerable" upper
   part of the local stack frame.  Remember if we ever return nonzero for
   any variable in this function.  The return value is the phase number in
   which the variable should be allocated.  */

static int stack_protect_decl_phase (MtcsExpand *self,tree decl)
{
  unsigned int bits = stack_protect_classify_type (TREE_TYPE (decl));
  int ret = 0;

  if (bits & SPCT_HAS_SMALL_CHAR_ARRAY)
    self->has_short_buffer = true;

  tree attribs = DECL_ATTRIBUTES (current_function_decl);
  if (!lookup_attribute ("no_stack_protector", attribs)
      && (flag_stack_protect == SPCT_FLAG_ALL
      || flag_stack_protect == SPCT_FLAG_STRONG
      || (flag_stack_protect == SPCT_FLAG_EXPLICIT
          && lookup_attribute ("stack_protect", attribs)))){
      if ((bits & (SPCT_HAS_SMALL_CHAR_ARRAY | SPCT_HAS_LARGE_CHAR_ARRAY))
      && !(bits & SPCT_HAS_AGGREGATE))
          ret = 1;
      else if (bits & SPCT_HAS_ARRAY)
          ret = 2;
  }else
    ret = (bits & SPCT_HAS_LARGE_CHAR_ARRAY) != 0;

  if (ret)
    self->has_protected_decls = true;

  return ret;
}


/* Ensure that variables in different stack protection phases conflict
   so that they are not merged and share the same stack slot.
   Return true if there are any address taken variables.  */

static bool add_stack_protection_conflicts (MtcsExpand *self)
{
  size_t i, j, n = self->stack_vars_num;
  unsigned char *phase;
  bool ret = false;

  phase = XNEWVEC (unsigned char, n);
  for (i = 0; i < n; ++i){
      phase[i] = stack_protect_decl_phase (self,self->stack_vars[i].decl);
      if (TREE_ADDRESSABLE (self->stack_vars[i].decl))
          ret = true;
  }

  for (i = 0; i < n; ++i){
      unsigned char ph_i = phase[i];
      for (j = i + 1; j < n; ++j)
        if (ph_i != phase[j])
          add_stack_var_conflict (self,i, j);
  }
  XDELETEVEC (phase);
  return ret;
}


/* A subroutine of expand_used_vars.  Walk down through the BLOCK tree
   and clear TREE_USED on all local variables.  */
static void clear_tree_used (tree block)
{
   tree t;
   for (t = BLOCK_VARS (block); t ; t = DECL_CHAIN (t)){
   /* if (!TREE_STATIC (t) && !DECL_EXTERNAL (t)) */
//      n_debug("mtcsexpand.c clear_tree_used 00 t:%p need:%d %d %d %s\n",
//             t,TREE_USED (t),(!VAR_P (t) && TREE_CODE (t) != RESULT_DECL),DECL_NONSHAREABLE (t),
//             VAR_P (t)?IDENTIFIER_POINTER(DECL_NAME(t)):"null");
      if ((!VAR_P (t) && TREE_CODE (t) != RESULT_DECL) || !DECL_NONSHAREABLE (t)){
//         n_debug("mtcsexpand.c clear_tree_used 11 t:%p need:%d %d %d %s\n",
//                t,TREE_USED (t),(!VAR_P (t) && TREE_CODE (t) != RESULT_DECL),DECL_NONSHAREABLE (t),
//                VAR_P (t)?IDENTIFIER_POINTER(DECL_NAME(t)):"null");
         TREE_USED (t) = 0;
      }
   }

   for (t = BLOCK_SUBBLOCKS (block); t ; t = BLOCK_CHAIN (t))
      clear_tree_used (t);
}

/* Check if the current function has calls that use a return slot.  */
static bool stack_protect_return_slot_p ()
{
  basic_block bb;
  FOR_ALL_BB_FN (bb, cfun)
    for (gimple_stmt_iterator gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
        gimple *stmt = gsi_stmt (gsi);
        /* This assumes that calls to internal-only functions never
           use a return slot.  */
        if (is_gimple_call (stmt) && !gimple_call_internal_p (stmt)
            && aggregate_value_p (TREE_TYPE (gimple_call_fntype (stmt)), gimple_call_fndecl (stmt)))
          return true;
    }
  return false;
}



/* If there's a chance to get a pseudo for t then if it would be of float mode
   and the actual access is via an integer mode (lowered memcpy or similar
   access) then avoid the register expansion if the mode likely is not storage
   suitable for raw bits processing (like XFmode on i?86).  */

static void avoid_type_punning_on_regs (MtcsExpand *self,tree t, bitmap forced_stack_vars)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   machine_mode access_mode = TYPE_MODE (TREE_TYPE (t));
   if (access_mode !=mtcsMode->modes.M_BLKmode && !mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,access_mode))
      return;
   tree base = get_base_address (t);
   if (DECL_P (base)
   && !TREE_ADDRESSABLE (base)
   && mtcs_mode_is_float_p/*!FLOAT_MODE_P*/(mtcsMode,DECL_MODE (base))
   && maybe_lt ( mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,DECL_MODE (base)),
   mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,DECL_MODE (base))))
   /* Double check in the expensive way we really would get a pseudo.  */
   && mtcs_func_use_register_for_decl/*!use_register_for_decl*/(mtcsFunc,base))
      bitmap_set_bit (forced_stack_vars, DECL_UID (base));
}

/* A debugging aid for expand_used_vars.  Dump the generated partitions.  */

static void dump_stack_var_partition (MtcsExpand *self)
{
  size_t si, i, j, n = self->stack_vars_num;
  for (si = 0; si < n; ++si){
      i = self->stack_vars_sorted[si];

      /* Skip variables that aren't partition representatives, for now.  */
      if (self->stack_vars[i].representative != i)
          continue;

      fprintf (dump_file, "Partition " HOST_SIZE_T_PRINT_UNSIGNED ": size ",(fmt_size_t) i);
      print_dec (self->stack_vars[i].size, dump_file);
      fprintf (dump_file, " align %u\n", self->stack_vars[i].alignb);
      for (j = i; j != EOC; j = self->stack_vars[j].next){
          fputc ('\t', dump_file);
          print_generic_expr (dump_file, self->stack_vars[j].decl, dump_flags);
      }
      fputc ('\n', dump_file);
  }
}

typedef struct _DiscoverData
{
    MtcsExpand *mtcsExpand;
    bitmap forced_stack_vars;
}DiscoverData;
/* Helper function for discover_nonconstant_array_refs.
   Look for ARRAY_REF nodes with non-constant indexes and mark them
   addressable.  */
//回调函数
static tree discover_nonconstant_array_refs_r_cb (tree * tp, int *walk_subtrees, void *data)
{
  DiscoverData *discoverData=(DiscoverData *)((walk_stmt_info *)data)->info;
  MtcsExpand *self=discoverData->mtcsExpand;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

  bitmap forced_stack_vars = discoverData->forced_stack_vars;
  tree t = *tp;
  //bitmap forced_stack_vars = (bitmap)((walk_stmt_info *)data)->info;
  if (IS_TYPE_OR_DECL_P (t))
    *walk_subtrees = 0;
  else if (REFERENCE_CLASS_P (t) && TREE_THIS_VOLATILE (t)){
      t = get_base_address (t);
      if (t && DECL_P (t) && DECL_MODE (t) != mtcsMode->modes.M_BLKmode && !TREE_ADDRESSABLE (t))
          bitmap_set_bit (forced_stack_vars, DECL_UID (t));
      *walk_subtrees = 0;
  }else if (TREE_CODE (t) == ARRAY_REF || TREE_CODE (t) == ARRAY_RANGE_REF){
      while (((TREE_CODE (t) == ARRAY_REF || TREE_CODE (t) == ARRAY_RANGE_REF)
          && is_gimple_min_invariant (TREE_OPERAND (t, 1))
          && (!TREE_OPERAND (t, 2)
          || is_gimple_min_invariant (TREE_OPERAND (t, 2))))
         || (TREE_CODE (t) == COMPONENT_REF
         && (!TREE_OPERAND (t,2)
             || is_gimple_min_invariant (TREE_OPERAND (t, 2))))
         || TREE_CODE (t) == BIT_FIELD_REF
         || TREE_CODE (t) == REALPART_EXPR
         || TREE_CODE (t) == IMAGPART_EXPR
         || TREE_CODE (t) == VIEW_CONVERT_EXPR
         || CONVERT_EXPR_P (t))
          t = TREE_OPERAND (t, 0);

          if (TREE_CODE (t) == ARRAY_REF || TREE_CODE (t) == ARRAY_RANGE_REF){
              t = get_base_address (t);
          if (t && DECL_P (t) && DECL_MODE (t) != mtcsMode->modes.M_BLKmode && !TREE_ADDRESSABLE (t))
            bitmap_set_bit (forced_stack_vars, DECL_UID (t));
      }
      *walk_subtrees = 0;
  }
  /* References of size POLY_INT_CST to a fixed-size object must go
     through memory.  It's more efficient to force that here than
     to create temporary slots on the fly.
     RTL expansion expectes TARGET_MEM_REF to always address actual memory.
     Also, force to stack non-BLKmode vars accessed through VIEW_CONVERT_EXPR
     to BLKmode type.  */
  else if (TREE_CODE (t) == TARGET_MEM_REF
       || (TREE_CODE (t) == MEM_REF && TYPE_SIZE (TREE_TYPE (t)) && POLY_INT_CST_P (TYPE_SIZE (TREE_TYPE (t))))
       || (TREE_CODE (t) == VIEW_CONVERT_EXPR && TYPE_MODE (TREE_TYPE (t)) == mtcsMode->modes.M_BLKmode)){
      tree base = get_base_address (t);
      if (base && DECL_P (base) && !TREE_ADDRESSABLE (base) && DECL_MODE (base) != mtcsMode->modes.M_BLKmode
            && mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,DECL_MODE (base)).is_constant ())
          bitmap_set_bit (forced_stack_vars, DECL_UID (base));
      *walk_subtrees = 0;
  }

  return NULL_TREE;
}

/* RTL expansion is not able to compile array references with variable
   offsets for arrays stored in single register.  Discover such
   expressions and mark variables as addressable to avoid this
   scenario.  */

static void discover_nonconstant_array_refs (MtcsExpand *self,bitmap forced_stack_vars)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   basic_block bb;
   gimple_stmt_iterator gsi;
   DiscoverData dis={self,forced_stack_vars};
   walk_stmt_info wi = {};
   wi.info =(void *) &dis/*!forced_stack_vars*/;
   FOR_EACH_BB_FN (bb, cfun)
      for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
         gimple *stmt = gsi_stmt (gsi);
         if (!is_gimple_debug (stmt)){
            walk_gimple_op (stmt, discover_nonconstant_array_refs_r_cb, &wi);
            gcall *call = dyn_cast <gcall *> (stmt);
            if (call && gimple_call_internal_p (call)){
               tree cand = NULL_TREE;
               switch (gimple_call_internal_fn (call)){
                  case IFN_LOAD_LANES:
                     /* The source must be a MEM.  */
                     cand = gimple_call_arg (call, 0);
                     break;
                  case IFN_STORE_LANES:
                     /* The destination must be a MEM.  */
                     cand = gimple_call_lhs (call);
                     break;
                  default:
                     break;
               }
               if (cand)
                  cand = get_base_address (cand);
               if (cand && DECL_P (cand) && mtcs_func_use_register_for_decl/*!use_register_for_decl*/(mtcsFunc,cand))
                  bitmap_set_bit (forced_stack_vars, DECL_UID (cand));
            }
            if (gimple_vdef (stmt)){
               tree t = gimple_get_lhs (stmt);
               if (t && REFERENCE_CLASS_P (t))
                  avoid_type_punning_on_regs(self,t, forced_stack_vars);
            }
         }
      }
}

/* A subroutine of expand_one_var.  Called to assign rtl to a VAR_DECL that
   has some associated error, e.g. its type is error-mark.  We just need
   to pick something that won't crash the rest of the compiler.  */

static void expand_one_error_var (MtcsExpand *self,tree var)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  machine_mode mode = DECL_MODE (var);
  rtx x;
  if (mode == mtcsMode->modes.M_BLKmode){
    x = gen_rtx_MEM (mtcsMode->modes.M_BLKmode/*!BLKmode*/, const0_rtx);
  }else if (mode == VOIDmode)
    x = const0_rtx;
  else
    x = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

  mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,var, x);
}

/* A subroutine of expand_one_var.  Called to assign rtl to a VAR_DECL
   that will reside in a hard register.  */

static void expand_one_hard_reg_var (tree var)
{
  rest_of_decl_compilation (var, 0, 0);
}

/* A subroutine of expand_one_var.  Called to assign rtl to a VAR_DECL
   that will reside in a pseudo register.  */

static void expand_one_register_var (MtcsExpand *self,tree var)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  if (TREE_CODE (var) == SSA_NAME){
      int part = var_to_partition (SA.map, var);
      if (part != NO_PARTITION){
          rtx x = SA.partition_to_pseudo[part];
          gcc_assert (x);
          gcc_assert (REG_P (x));
          return;
      }
      gcc_unreachable ();
  }

  tree decl = var;
  tree type = TREE_TYPE (decl);
  machine_mode reg_mode = promote_decl_mode (decl, NULL);
  rtx x = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,reg_mode);

  set_rtl (self,var, x);

  /* Note if the object is a user variable.  */
  if (!DECL_ARTIFICIAL (decl))
    mark_user_reg (x);

  if (POINTER_TYPE_P (type))
      mtcs_rtl_mark_reg_pointer/*!mark_reg_pointer*/(mtcsRTL,x, get_pointer_alignment (var));
}



/* Wrapper for expand_one_stack_var_1 that checks SSA_NAMEs are
   already assigned some MEM.  */
static void expand_one_stack_var (MtcsExpand *self,tree var)
{
  if (TREE_CODE (var) == SSA_NAME){
      int part = var_to_partition (SA.map, var);
      if (part != NO_PARTITION){
          rtx x = SA.partition_to_pseudo[part];
          gcc_assert (x);
          gcc_assert (MEM_P (x));
          return;
      }
  }
  return expand_one_stack_var_1 (self,var);
}

/* A subroutine of expand_used_vars.  Expand one variable according to
   its flavor.  Variables to be placed on the stack are not actually
   expanded yet, merely recorded.
   When REALLY_EXPAND is false, only add stack values to be allocated.
   Return stack usage this variable is supposed to take.
*/
//原型 expand_one_var ecfgexpand.cc
static poly_uint64 expand_one_var (MtcsExpand *self,tree var, bool toplevel, bool really_expand, bitmap forced_stack_var = NULL)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

  unsigned int align = BITS_PER_UNIT;
  tree origvar = var;
  n_debug("mtcsexpand.c expand_one_var 00 var:%p toplevel:%d really_expand:%d\n",var,toplevel,really_expand);

  var = SSAVAR (var);

  if (TREE_TYPE (var) != error_mark_node && VAR_P (var)){
      if (is_global_var (var))
          return 0;
      n_debug("mtcsexpand.c expand_one_var 11 var:%p toplevel:%d really_expand:%d\n",var,toplevel,really_expand);
      /* Because we don't know if VAR will be in register or on stack,
     we conservatively assume it will be on stack even if VAR is
     eventually put into register after RA pass.  For non-automatic
     variables, which won't be on stack, we collect alignment of
     type and ignore user specified alignment.  Similarly for
     SSA_NAMEs for which use_register_for_decl returns true.  */
      if (TREE_STATIC (var)   || DECL_EXTERNAL (var)
            || (TREE_CODE (origvar) == SSA_NAME && mtcs_func_use_register_for_decl/*!use_register_for_decl*/(mtcsFunc,var)))
          align = mtcs_mode_get_mininum_alignment/*!MINIMUM_ALIGNMENT*/(mtcsMode,
                  TREE_TYPE (var), TYPE_MODE (TREE_TYPE (var)),TYPE_ALIGN (TREE_TYPE (var)));
      else if (DECL_HAS_VALUE_EXPR_P (var)|| (DECL_RTL_SET_P (var) && MEM_P (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,var))))
        /* Don't consider debug only variables with DECL_HAS_VALUE_EXPR_P set
           or variables which were assigned a stack slot already by
           expand_one_stack_var_at - in the latter case DECL_ALIGN has been
           changed from the offset chosen to it.  */
        align = mtcsRtlData/*!crtl*/->stack_alignment_estimated;
      else
          align = mtcs_mode_get_mininum_alignment/*!MINIMUM_ALIGNMENT*/(mtcsMode,var, DECL_MODE (var), DECL_ALIGN (var));

      /* If the variable alignment is very large we'll dynamicaly allocate
     it, which means that in-frame portion is just a pointer.  */
      if (align > mtcs_func_get_max_support_stack_alignment/*!MAX_SUPPORTED_STACK_ALIGNMENT*/(mtcsFunc))
          align =mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mtcs_mode_get_Pmode/*!Pmode*/(mtcsMode));
  }
  n_debug("mtcsexpand.c expand_one_var 22 var:%p toplevel:%d really_expand:%d\n",var,toplevel,really_expand);

  record_alignment_for_reg_var (self,align);

  poly_uint64 size;
  if (TREE_CODE (origvar) == SSA_NAME){
      gcc_assert (!VAR_P (var)
          || (!DECL_EXTERNAL (var)
              && !DECL_HAS_VALUE_EXPR_P (var)
              && !TREE_STATIC (var)
              && TREE_TYPE (var) != error_mark_node
              && !DECL_HARD_REGISTER (var)
              && really_expand));
  }
  n_debug("mtcsexpand.c expand_one_var 33 var:%p toplevel:%d really_expand:%d\n",var,toplevel,really_expand);

  if (!VAR_P (var) && TREE_CODE (origvar) != SSA_NAME)
    ;
  else if (DECL_EXTERNAL (var))
    ;
  else if (DECL_HAS_VALUE_EXPR_P (var))
    ;
  else if (TREE_STATIC (var))
    ;
  else if (TREE_CODE (origvar) != SSA_NAME && DECL_RTL_SET_P (var))
    ;
  else if (TREE_TYPE (var) == error_mark_node){
     n_debug("mtcsexpand.c expand_one_var 44 var:%p toplevel:%d really_expand:%d\n",var,toplevel,really_expand);

      if (really_expand)
        expand_one_error_var (self,var);
  }else if (VAR_P (var) && DECL_HARD_REGISTER (var)){
     n_debug("mtcsexpand.c expand_one_var 55 var:%p toplevel:%d really_expand:%d\n",var,toplevel,really_expand);

      if (really_expand){
          expand_one_hard_reg_var (var);
          if (!DECL_HARD_REGISTER (var))
            /* Invalid register specification.  */
            expand_one_error_var (self,var);
      }
  }else if (mtcs_func_use_register_for_decl/*!use_register_for_decl*/ (mtcsFunc,var)
          && (!forced_stack_var || !bitmap_bit_p (forced_stack_var, DECL_UID (var)))){
     n_debug("mtcsexpand.c expand_one_var 66 var:%p toplevel:%d really_expand:%d\n",var,toplevel,really_expand);

      if (really_expand)
        expand_one_register_var (self,origvar);
  }else if (!poly_int_tree_p (DECL_SIZE_UNIT (var), &size) || !valid_constant_size_p (DECL_SIZE_UNIT (var))){
     n_debug("mtcsexpand.c expand_one_var 77 var:%p toplevel:%d really_expand:%d\n",var,toplevel,really_expand);

      /* Reject variables which cover more than half of the address-space.  */
      if (really_expand){
          if (DECL_NONLOCAL_FRAME (var))
            error_at (DECL_SOURCE_LOCATION (current_function_decl),"total size of local objects is too large");
          else
            error_at (DECL_SOURCE_LOCATION (var),"size of variable %q+D is too large", var);
          expand_one_error_var (self,var);
      }
  }else if (defer_stack_allocation (self,var, toplevel)){
     n_debug("mtcsexpand.c expand_one_var 88 var:%p toplevel:%d really_expand:%d\n",var,toplevel,really_expand);

    add_stack_var (self,origvar, really_expand);
  }else{
     n_debug("mtcsexpand.c expand_one_var 99 var:%p toplevel:%d really_expand:%d\n",var,toplevel,really_expand);

      if (really_expand){
          if (lookup_attribute ("naked",DECL_ATTRIBUTES (current_function_decl)))
              error ("cannot allocate stack for variable %q+D, naked function",var);

          expand_one_stack_var (self,origvar);
      }
      return size;
  }
  return 0;
}


/* A subroutine of expand_used_vars.  Walk down through the BLOCK tree
   expanding variables.  Those variables that can be put into registers
   are allocated pseudos; those that can't are put on the stack.

   TOPLEVEL is true if this is the outermost BLOCK.  */
static void expand_used_vars_for_block (MtcsExpand *self,tree block, bool toplevel, bitmap forced_stack_vars)
{
   tree t;
   /* Expand all variables at this level.  */
   for (t = BLOCK_VARS (block); t ; t = DECL_CHAIN (t)){
      //n_debug("mtcsexpand.c expand_used_vars_for_block 00 toplevel:%d t:%p need:%d %d %d %s\n",
      //toplevel,t,TREE_USED (t),(!VAR_P (t) && TREE_CODE (t) != RESULT_DECL),DECL_NONSHAREABLE (t),
      //VAR_P (t)?IDENTIFIER_POINTER(DECL_NAME(t)):"null");

      if (TREE_USED (t) && ((!VAR_P (t) && TREE_CODE (t) != RESULT_DECL) || !DECL_NONSHAREABLE (t))){
         //n_debug("mtcsexpand.c expand_used_vars_for_block 11 toplevel:%d t:%p %s\n",toplevel,t,
        // VAR_P (t)?IDENTIFIER_POINTER(DECL_NAME(t)):"null");
         expand_one_var (self,t, toplevel, true, forced_stack_vars);
      }
   }

   /* Expand all variables at containing levels.  */
   for (t = BLOCK_SUBBLOCKS (block); t ; t = BLOCK_CHAIN (t)){
      //n_debug("mtcsexpand.c expand_used_vars_for_block 22 toplevel:%d t:%p\n",toplevel,t);
      expand_used_vars_for_block (self,t, false, forced_stack_vars);
   }
}

/* Prepare for expanding variables.  */
static void init_vars_expansion (MtcsExpand *self)
{
  /* Conflict bitmaps, and a few related temporary bitmaps, go here.  */
  bitmap_obstack_initialize (&self->stack_var_bitmap_obstack);

  /* A map from decl to stack partition.  */
  self->decl_to_stack_part = new hash_map<tree, size_t>;

  /* Initialize local stack smashing state.  */
  self->has_protected_decls = false;
  self->has_short_buffer = false;
  if (hwasan_sanitize_stack_p ())
    hwasan_record_frame_init ();
}

/* Record the alignment requirements of some variable assigned to a
   pseudo.  */

static void record_alignment_for_reg_var (MtcsExpand *self,unsigned int align)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

  if (mtcs_func_is_support_stack_alignment(mtcsFunc)/*SUPPORTS_STACK_ALIGNMENT*/
      && mtcsRtlData->stack_alignment_estimated < align){
      /* stack_alignment_estimated shouldn't change after stack
         realign decision made */
      gcc_assert (!mtcsRtlData->stack_realign_processed);
      mtcsRtlData->stack_alignment_estimated = align;
  }

  /* stack_alignment_needed > PREFERRED_STACK_BOUNDARY is permitted.
     So here we only make sure stack_alignment_needed >= align.  */
  if (mtcsRtlData->stack_alignment_needed < align)
      mtcsRtlData->stack_alignment_needed = align;
  if (mtcsRtlData->max_used_stack_slot_alignment < align)
      mtcsRtlData->max_used_stack_slot_alignment = align;
}

/* A subroutine of expand_one_var.  VAR is a variable that will be
   allocated to the local stack frame.  Return true if we wish to
   add VAR to STACK_VARS so that it will be coalesced with other
   variables.  Return false to allocate VAR immediately.

   This function is used to reduce the number of variables considered
   for coalescing, which reduces the size of the quadratic problem.  */

static bool defer_stack_allocation (MtcsExpand *self,tree var, bool toplevel)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

  tree size_unit = TREE_CODE (var) == SSA_NAME ? TYPE_SIZE_UNIT (TREE_TYPE (var)) : DECL_SIZE_UNIT (var);
  poly_uint64 size;

  /* Whether the variable is small enough for immediate allocation not to be
     a problem with regard to the frame size.  */
  bool smallish = (poly_int_tree_p (size_unit, &size) && (estimated_poly_value (size)< param_min_size_for_stack_sharing));

  /* If stack protection is enabled, *all* stack variables must be deferred,
     so that we can re-order the strings to the top of the frame.
     Similarly for Address Sanitizer.  */
  if (flag_stack_protect || asan_sanitize_stack_p ())
    return true;

  unsigned int align = TREE_CODE (var) == SSA_NAME ? TYPE_ALIGN (TREE_TYPE (var)) : DECL_ALIGN (var);

  /* We handle "large" alignment via dynamic allocation.  We want to handle
     this extra complication in only one place, so defer them.  */
  if (align > mtcs_func_get_max_support_stack_alignment/*!MAX_SUPPORTED_STACK_ALIGNMENT*/(mtcsFunc))
    return true;

  bool ignored = TREE_CODE (var) == SSA_NAME ? !SSAVAR (var) || DECL_IGNORED_P (SSA_NAME_VAR (var)) : DECL_IGNORED_P (var);

  /* When optimization is enabled, DECL_IGNORED_P variables originally scoped
     might be detached from their block and appear at toplevel when we reach
     here.  We want to coalesce them with variables from other blocks when
     the immediate contribution to the frame size would be noticeable.  */
  if (toplevel && optimize > 0 && ignored && !smallish)
    return true;

  /* Variables declared in the outermost scope automatically conflict
     with every other variable.  The only reason to want to defer them
     at all is that, after sorting, we can more efficiently pack
     small variables in the stack frame.  Continue to defer at -O2.  */
  if (toplevel && optimize < 2)
    return false;

  /* Without optimization, *most* variables are allocated from the
     stack, which makes the quadratic problem large exactly when we
     want compilation to proceed as quickly as possible.  On the
     other hand, we don't want the function's stack frame size to
     get completely out of hand.  So we avoid adding scalars and
     "small" aggregates to the list at all.  */
  if (optimize == 0 && smallish)
    return false;

  return true;
}

/* Compute the byte alignment to use for DECL.  Ignore alignment
   we can't do with expected alignment of the stack boundary.  */

static unsigned int align_local_variable (MtcsExpand *self,tree decl, bool really_expand)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
  unsigned int align;

  if (TREE_CODE (decl) == SSA_NAME){
      tree type = TREE_TYPE (decl);
      machine_mode mode = TYPE_MODE (type);
      //mtcs_mode deviceMode=mtcs_mode_host2device_by_tree(mtcsMode,type,mode);
      align = TYPE_ALIGN (type);
      if (mode != mtcsMode->modes.M_BLKmode  && align < mtcs_mode_get_alignment/*GET_MODE_ALIGNMENT (mode)*/(mtcsMode,mode))
          align = mtcs_mode_get_alignment/*GET_MODE_ALIGNMENT (mode)*/(mtcsMode,mode);
  }else
    align =mtcs_align_get_local_decl_alignment(mtcsAlign,decl);/* LOCAL_DECL_ALIGNMENT (decl);*/

  if (hwasan_sanitize_stack_p ())
    align = MAX (align, (unsigned) HWASAN_TAG_GRANULE_SIZE * BITS_PER_UNIT);

  if (TREE_CODE (decl) != SSA_NAME && really_expand)
    /* Don't change DECL_ALIGN when called from estimated_stack_frame_size.
       That is done before IPA and could bump alignment based on host
       backend even for offloaded code which wants different
       LOCAL_DECL_ALIGNMENT.  */
    SET_DECL_ALIGN (decl, align);

  return align / BITS_PER_UNIT;
}

static tree leader_merge (tree cur, tree next)
{
  if (cur == NULL || cur == next)
    return next;
  if (DECL_P (cur) && DECL_IGNORED_P (cur))
    return cur;
  if (DECL_P (next) && DECL_IGNORED_P (next))
    return next;
  return cur;
}


/* Associate declaration T with storage space X.  If T is no
   SSA name this is exactly SET_DECL_RTL, otherwise make the
   partition of T associated with X.  */
static void set_rtl (MtcsExpand *self,tree t, rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsGimpleExpr *mtcsGimpleExpr=mtcs_target_get_gimple_expr(mtcsTarget);
  n_debug("mtcsexpand.c set_rtl 00 x:%p\n",x);
  n_debug("mtcsexpand.c set_rtl 11 :%d\n",(TREE_CODE (t) == SSA_NAME || mtcs_gimple_expr_is_gimple_reg/*!is_gimple_reg*/(mtcsGimpleExpr,t)));
  n_debug("mtcsexpand.c set_rtl 22 x:%p\n",mtcs_func_use_register_for_decl/*!use_register_for_decl*/(mtcsFunc,t));


  gcc_checking_assert (!x
               || !(TREE_CODE (t) == SSA_NAME || mtcs_gimple_expr_is_gimple_reg/*!is_gimple_reg*/(mtcsGimpleExpr,t))
               || (mtcs_func_use_register_for_decl/*!use_register_for_decl*/(mtcsFunc,t)
               ? (REG_P (x) || (GET_CODE (x) == CONCAT
                  && (REG_P (XEXP (x, 0)) || SUBREG_P (XEXP (x, 0)))
                  && (REG_P (XEXP (x, 1)) || SUBREG_P (XEXP (x, 1))))
                  /* We need to accept PARALLELs for RESUT_DECLs
                 because of vector types with BLKmode returned
                 in multiple registers, but they are supposed
                 to be uncoalesced.  */
                  || (GET_CODE (x) == PARALLEL  && SSAVAR (t)
                  && TREE_CODE (SSAVAR (t)) == RESULT_DECL
                  && (GET_MODE (x) == mtcsMode->modes.M_BLKmode  || !flag_tree_coalesce_vars)))
               : (MEM_P (x) || x == pc_rtx
                  || (GET_CODE (x) == CONCAT
                  && MEM_P (XEXP (x, 0))
                  && MEM_P (XEXP (x, 1))))));
  /* Check that the RTL for SSA_NAMEs and gimple-reg PARM_DECLs and
     RESULT_DECLs has the expected mode.  For memory, we accept
     unpromoted modes, since that's what we're likely to get.  For
     PARM_DECLs and RESULT_DECLs, we'll have been called by
     set_parm_rtl, which will give us the default def, so we don't
     have to compute it ourselves.  For RESULT_DECLs, we accept mode
     mismatches too, as long as we have BLKmode or are not coalescing
     across variables, so that we don't reject BLKmode PARALLELs or
     unpromoted REGs.  */
  n_debug("mtcsexpand.c set_rtl 00 t:%p x:%p pc_rtx:%p %p %d\n",t,x,mtcsRTL->pc_rtx,pc_rtx,TREE_CODE (t) != SSA_NAME);
  //aet_print_tree(t);
  //aet_print_tree(SSAVAR (t));
  if(TREE_CODE (t) == SSA_NAME){
     bool rs=  mtcs_func_use_register_for_decl/*!use_register_for_decl*/(mtcsFunc,t);
     machine_mode mx= mtcs_mode_promote_ssa_mode/*!promote_ssa_mode*/(mtcsMode,t, NULL);
     n_debug("mtcsexpand.c set_rtl 00xx blkmode:%d GET_MODE(x):%d mode:%d registerfordecl:%d\n",
           mtcsMode->modes.M_BLKmode,GET_MODE (x),mx,rs);
  }

  gcc_checking_assert (!x || x == pc_rtx || TREE_CODE (t) != SSA_NAME
               || (SSAVAR (t)  && TREE_CODE (SSAVAR (t)) == RESULT_DECL
               && (mtcs_mode_promote_ssa_mode/*!promote_ssa_mode*/(mtcsMode,t, NULL) == mtcsMode->modes.M_BLKmode || !flag_tree_coalesce_vars))
               || !mtcs_func_use_register_for_decl/*!use_register_for_decl*/(mtcsFunc,t)
               || GET_MODE (x) == mtcs_mode_promote_ssa_mode/*!promote_ssa_mode*/(mtcsMode,t, NULL));

  n_debug("mtcsexpand.c set_rtl 11 t:%p x:%p pc_rtx:%p %p %d\n",t,x,mtcsRTL->pc_rtx,pc_rtx,TREE_CODE (t) != SSA_NAME);


  if (x){
      bool skip = false;
      tree cur = NULL_TREE;
      rtx xm = x;
    retry:
      if (MEM_P (xm)){
         n_debug("mtcsexpand.c set_rtl 22 MEM_P (xm) t:%p \n",t);

          cur = mtcs_rtl_get_mem_expr/*!MEM_EXPR*/(mtcsRTL,xm);
      }else if (REG_P (xm)){
         n_debug("mtcsexpand.c set_rtl 33 REG_P (xm) t:%p \n",t);

          cur = REG_EXPR (xm);
      }else if (SUBREG_P (xm)){
         n_debug("mtcsexpand.c set_rtl 44 SUBREG_P (xm) t:%p \n",t);
          gcc_assert (mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,xm));
          xm = SUBREG_REG (xm);
          goto retry;
      }else if (GET_CODE (xm) == CONCAT){
         n_debug("mtcsexpand.c set_rtl 55 GET_CODE (xm)==CONCAT t:%p \n",t);

          xm = XEXP (xm, 0);
          goto retry;
      }else if (GET_CODE (xm) == PARALLEL){
         n_debug("mtcsexpand.c set_rtl 66 GET_CODE (xm)==PARALLEL t:%p \n",t);


          xm = XVECEXP (xm, 0, 0);
          gcc_assert (GET_CODE (xm) == EXPR_LIST);
          xm = XEXP (xm, 0);
          goto retry;
      }else if (xm == pc_rtx){
         n_debug("mtcsexpand.c set_rtl 77 xm == pc_rtx t:%p \n",t);

          skip = true;
      }else
          gcc_unreachable ();

      tree next = skip ? cur : leader_merge (cur, SSAVAR (t) ? SSAVAR (t) : t);
      if (cur != next){
         n_debug("mtcsexpand.c set_rtl 88 t:%p MEM_P (x):%d \n",t,MEM_P (x));

          if (MEM_P (x))
              mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,x,
                      next && TREE_CODE (next) == SSA_NAME ? TREE_TYPE (next): next, true);
          else
            mtcs_rtl_set_reg_attrs_for_decl_rtl(mtcsRTL,next,x);/*set_reg_attrs_for_decl_rtl (next, x);*/
      }
  }
   n_debug("mtcsexpand.c set_rtl 99 t:%p x:%p pc_rtx:%p %d\n",t,x,pc_rtx,TREE_CODE (t) != SSA_NAME);
  if (TREE_CODE (t) == SSA_NAME){
      int part = var_to_partition (SA.map, t);
      n_debug("mtcsexpand.c set_rtl 100 t:%p x:%p pc_rtx:%p %d\n",t,x,pc_rtx,TREE_CODE (t) != SSA_NAME);
      if (part != NO_PARTITION){
         n_debug("mtcsexpand.c set_rtl 101 t:%p %d %d\n",t,SA.partition_to_pseudo[part],(x != pc_rtx));

          if (SA.partition_to_pseudo[part]){
            gcc_assert (SA.partition_to_pseudo[part] == x);
          }else if (x != pc_rtx){
            SA.partition_to_pseudo[part] = x;
          }
      }
      /* For the benefit of debug information at -O0 (where
         vartracking doesn't run) record the place also in the base
         DECL.  For PARMs and RESULTs, do so only when setting the
         default def.  */
      if (x && x != pc_rtx && SSA_NAME_VAR (t)  && (VAR_P (SSA_NAME_VAR (t)) || SSA_NAME_IS_DEFAULT_DEF (t))){

          tree var = SSA_NAME_VAR (t);
          n_debug("mtcsexpand.c set_rtl 102 t:%p DECL_RTL_SET_P:%d\n",t,DECL_RTL_SET_P (var));
          mtcs_print_rtl_single(stderr,x);

          /* If we don't yet have something recorded, just record it now.  */
          if (!DECL_RTL_SET_P (var))
             mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,var, x);
          /* If we have it set already to "multiple places" don't
             change this.  */
          else if (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,var) == pc_rtx)
            ;
          /* If we have something recorded and it's not the same place
             as we want to record now, we have multiple partitions for the
             same base variable, with different places.  We can't just
             randomly chose one, hence we have to say that we don't know.
             This only happens with optimization, and there var-tracking
             will figure out the right thing.  */
          else if (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,var) != x)
             mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,var, pc_rtx);
      }
  }else{
     n_debug("mtcsexpand.c set_rtl 103 t:%p \n",t);
     mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,t, x);
     n_debug("mtcsexpand.c set_rtl 104 --- t:%p \n",t);

  }
}

/* Accumulate DECL into STACK_VARS.  */
static void add_stack_var (MtcsExpand *self,tree decl, bool really_expand)
{
  StackVar  *v;

  if (self->stack_vars_num >= self->stack_vars_alloc){
      if (self->stack_vars_alloc)
          self->stack_vars_alloc = self->stack_vars_alloc * 3 / 2;
      else
          self->stack_vars_alloc = 32;
      self->stack_vars= XRESIZEVEC (StackVar , self->stack_vars, self->stack_vars_alloc);
  }
  if (!self->decl_to_stack_part)
    self->decl_to_stack_part = new hash_map<tree, size_t>;

  v = &self->stack_vars[self->stack_vars_num];
  self->decl_to_stack_part->put (decl, self->stack_vars_num);

  v->decl = decl;
  tree size = TREE_CODE (decl) == SSA_NAME ? TYPE_SIZE_UNIT (TREE_TYPE (decl)): DECL_SIZE_UNIT (decl);
  v->size = tree_to_poly_uint64 (size);
  /* Ensure that all variables have size, so that &a != &b for any two
     variables that are simultaneously live.  */
  if (known_eq (v->size, 0U))
    v->size = 1;
  v->alignb = align_local_variable (self,decl, really_expand);
  /* An alignment of zero can mightily confuse us later.  */
  gcc_assert (v->alignb != 0);

  /* All variables are initially in their own partition.  */
  v->representative = self->stack_vars_num;
  v->next = EOC;

  /* All variables initially conflict with no other.  */
  v->conflicts = NULL;

  /* Ensure that this decl doesn't get put onto the list twice.  */
  set_rtl (self,decl, pc_rtx);

  self->stack_vars_num++;
}

/* Allocate SIZE bytes at byte alignment ALIGN from the stack frame.
   Return the frame offset.  */

static poly_int64 alloc_stack_frame_space (MtcsExpand *self,poly_int64 size, unsigned HOST_WIDE_INT align)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  poly_int64 offset, new_frame_offset;
  n_debug("mtcsexpand.c alloc_stack_frame_space 00 %ld %d\n",size,align);
  if (mtcs_func_get_frame_grows_downward/*!FRAME_GROWS_DOWNWARD*/(mtcsFunc)){
      new_frame_offset = aligned_lower_bound (mtcsRtlData->x_frame_offset - self->frame_phase - size,align) + self->frame_phase;
      offset = new_frame_offset;
  }else{
      new_frame_offset = aligned_upper_bound (mtcsRtlData->x_frame_offset - self->frame_phase,align) + self->frame_phase;
      offset = new_frame_offset;
      new_frame_offset += size;
  }
  mtcsRtlData->x_frame_offset = new_frame_offset;
  n_debug("mtcsexpand.c alloc_stack_frame_space 11 %ld %d\n",size,align);

  if (mtcs_func_frame_offset_overflow/*!frame_offset_overflow*/(mtcsFunc,mtcsRtlData->x_frame_offset, cfun->decl))
      mtcsRtlData->x_frame_offset = offset = 0;

  return offset;
}

/* Ensure that the stack is aligned to ALIGN bytes.
   Return the new frame offset.  */
static poly_int64 align_frame_offset (MtcsExpand *self,unsigned HOST_WIDE_INT align)
{
  return alloc_stack_frame_space (self,0, align);
}


/* Assign rtl to DECL at BASE + OFFSET.  */

static void expand_one_stack_var_at (MtcsExpand *self,tree decl, rtx base, unsigned base_align,poly_int64 offset)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   unsigned align;
   rtx x;
   mtcs_mode pmode=mtcs_mode_get_Pmode(mtcsMode);
   /* If this fails, we've overflowed the stack frame.  Error nicely?  */
   gcc_assert (known_eq (offset, mtcs_mode_trunc_int_for_mode(mtcsMode,offset,pmode)/*trunc_int_for_mode (offset, Pmode)*/));
   if (hwasan_sanitize_stack_p ())
      //x = mtcsTarget->memtag.add_tag/*targetm.memtag.add_tag*/(mtcsTarget,base, offset,hwasan_current_frame_tag ());
      x = target_mem_tag_add_tag/*targetm.memtag.add_tag*/(mtcsMachine->memTag,base,offset,hwasan_current_frame_tag ());
   else
      x = mtcs_rtl_plus_constant/*plus_constant*/ (mtcsRTL,pmode, base, offset,false);

   x = gen_rtx_MEM (TREE_CODE (decl) == SSA_NAME? TYPE_MODE (TREE_TYPE (decl)) : DECL_MODE (decl), x);

   /* Set alignment we actually gave this decl if it isn't an SSA name.
   If it is we generate stack slots only accidentally so it isn't as
   important, we'll simply set the alignment directly on the MEM.  */

   if (stack_vars_base_reg_p (base))
      offset -= self->frame_phase;
   align = known_alignment (offset);
   align *= BITS_PER_UNIT;
   if (align == 0 || align > base_align)
      align = base_align;

   if (TREE_CODE (decl) != SSA_NAME){
      /* One would think that we could assert that we're not decreasing
      alignment here, but (at least) the i386 port does exactly this
      via the MINIMUM_ALIGNMENT hook.  */

      SET_DECL_ALIGN (decl, align);
      DECL_USER_ALIGN (decl) = 0;
   }
   set_rtl (self,decl, x);
   mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,x, align);
}

/* A subroutine of expand_one_var.  Called to immediately assign rtl
   to a variable to be allocated in the stack frame.  */

static void expand_one_stack_var_1 (MtcsExpand *self,tree var)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  poly_uint64 size;
  poly_int64 offset;
  unsigned byte_align;

  if (TREE_CODE (var) == SSA_NAME){
      tree type = TREE_TYPE (var);
      size = tree_to_poly_uint64 (TYPE_SIZE_UNIT (type));
  }else
    size = tree_to_poly_uint64 (DECL_SIZE_UNIT (var));

  byte_align = align_local_variable (self,var, true);

  /* We handle highly aligned variables in expand_stack_vars.  */
  gcc_assert (byte_align * BITS_PER_UNIT <= mtcs_func_get_max_support_stack_alignment(mtcsFunc)/*MAX_SUPPORTED_STACK_ALIGNMENT*/);

  rtx base;
  if (hwasan_sanitize_stack_p ()){
      /* Allocate zero bytes to align the stack.  */
      poly_int64 hwasan_orig_offset = align_frame_offset(self,HWASAN_TAG_GRANULE_SIZE);
      offset = alloc_stack_frame_space (self,size, byte_align);
      align_frame_offset (self,HWASAN_TAG_GRANULE_SIZE);
      base = hwasan_frame_base ();
      /* Use `frame_offset` to automatically account for machines where the
     frame grows upwards.

     `offset` will always point to the "start" of the stack object, which
     will be the smallest address, for ! FRAME_GROWS_DOWNWARD this is *not*
     the "furthest" offset from the base delimiting the current stack
     object.  `frame_offset` will always delimit the extent that the frame.
     */
      hwasan_record_stack_var (mtcs_rtl_get_virtual_stack_args_rtx/*!virtual_stack_vars_rtx*/(mtcsRTL),
            base,hwasan_orig_offset, mtcsRtlData->x_frame_offset);
  }else{
      offset = alloc_stack_frame_space (self,size, byte_align);
      base = mtcs_rtl_get_virtual_stack_args_rtx/*!virtual_stack_vars_rtx*/(mtcsRTL);
  }

  expand_one_stack_var_at (self,var, base,mtcsRtlData/*!crtl*/->max_used_stack_slot_alignment, offset);

  if (hwasan_sanitize_stack_p ())
    hwasan_increment_frame_tag ();
}


/* Create RTL for an SSA partition.  */
static void expand_one_ssa_partition (MtcsExpand *self,tree var)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

  int part = var_to_partition (SA.map, var);
  gcc_assert (part != NO_PARTITION);
  if (SA.partition_to_pseudo[part])
    return;
  //machine_mode hostMode=TYPE_MODE (TREE_TYPE (var));
  mtcs_mode mode=TYPE_MODE (TREE_TYPE (var));//mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (var),hostMode);
  /*unsigned int align = MINIMUM_ALIGNMENT (TREE_TYPE (var),TYPE_MODE (TREE_TYPE (var)),TYPE_ALIGN (TREE_TYPE (var)));*/
  unsigned int align = mtcs_mode_get_mininum_alignment/*MINIMUM_ALIGNMENT*/ (mtcsMode,
          TREE_TYPE (var), mode, TYPE_ALIGN (TREE_TYPE (var)));


  /* If the variable alignment is very large we'll dynamicaly allocate
     it, which means that in-frame portion is just a pointer.  */
  if (align > mtcs_func_get_max_support_stack_alignment(mtcsFunc)/*MAX_SUPPORTED_STACK_ALIGNMENT*/){
    align =mtcs_mode_get_alignment/* GET_MODE_ALIGNMENT*/ (mtcsMode,mtcs_mode_get_Pmode(mtcsMode));
  }

  record_alignment_for_reg_var (self,align);

  if (!mtcs_func_use_register_for_decl/*!use_register_for_decl*/ (mtcsFunc,var)){
      if (defer_stack_allocation (self,var, true))
          add_stack_var (self,var, true);
      else
          expand_one_stack_var_1 (self,var);
      return;
  }
  n_debug("mtcsexpand.c expand_one_ssa_partition 00 mode:%d\n",mode);
  mtcs_mode reg_mode = mtcs_mode_promote_ssa_mode/*promote_ssa_mode*/ (mtcsMode,var, NULL);
  n_debug("mtcsexpand.c expand_one_ssa_partition 11 mode:%d\n",reg_mode);
  rtx x = mtcs_emit_gen_reg_rtx/*gen_reg_rtx*/ (mtcsEmit,reg_mode);
  n_debug("mtcsexpand.c expand_one_ssa_partition 22 mode:%d:%d\n",reg_mode,GET_MODE(x));

  set_rtl (self,var, x);

  /* For a promoted variable, X will not be used directly but wrapped in a
     SUBREG with SUBREG_PROMOTED_VAR_P set, which means that the RTL land
     will assume that its upper bits can be inferred from its lower bits.
     Therefore, if X isn't initialized on every path from the entry, then
     we must do it manually in order to fulfill the above assumption.  */
  if (reg_mode != TYPE_MODE (TREE_TYPE (var))  && bitmap_bit_p (SA.partitions_for_undefined_values, part))
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,x, CONST0_RTX (reg_mode));
}

/*
   此函数负责展开当前函数中所有局部变量, 此展开基于栈中的标记位virtual_stack_vars_rtx,
  局部变量区由高地址=>低地址增长，故virtual_stack_vars_rtx实际上是第一个局部变量的尾地址.
   virtual_stack_vars_rtx 最终会被替换为硬件寄存器fp/sp + 一个固定偏移.
   * 被展开的变量的rtl表达式类似: (mem (plus virtual_stack_vars_rtx, offset))
   * 栈向低地址增长时(即通常,或aarch64下)offset是个负数, 此offset对于每个变量均不同(根据已分配情况计算出来的).
   需要注意的是延迟分配和空间优化可能会导致局部变量分配顺序和定义顺序不同(这里不再展开细节)。
   原文链接：https://blog.csdn.net/lidan113lidan/article/details/123961954
*/
/* Expand all variables used in the function.  */
static rtx_insn *expand_used_vars (MtcsExpand *self,bitmap forced_stack_vars)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsGimpleExpr *mtcsGimpleExpr=mtcs_target_get_gimple_expr(mtcsTarget);

  tree var, outer_block = DECL_INITIAL (current_function_decl);
  auto_vec<tree> maybe_local_decls;
  rtx_insn *var_end_seq = NULL;
  unsigned i;
  unsigned len;

  bool gen_stack_protect_signal = false;
  n_debug("mtcsexpand.c expand_used_vars 00 outer_block:%p\n",outer_block);
  aet_print_tree(outer_block);
  tree rx= BLOCK_VARS (outer_block);
  aet_print_tree(rx);
  testprint(cfun);

  /* Compute the phase of the stack frame for this function.  */
  {
    int align =mtcs_func_get_preferred_stack_boundary/*!PREFERRED_STACK_BOUNDARY*/(mtcsFunc) / BITS_PER_UNIT;
    int off = mtcsTarget->starting_frame_offset/*targetm.starting_frame_offset*/ (mtcsTarget) % align;
    self->frame_phase = off ? align - off : 0;
  }
  n_debug("mtcsexpand.c expand_used_vars 11\n");
  testprint(cfun);

  /* Set TREE_USED on all variables in the local_decls.  */
  FOR_EACH_LOCAL_DECL (cfun, i, var)
    TREE_USED (var) = 1;

  n_debug("mtcsexpand.c expand_used_vars 11--\n");
  testprint(cfun);
  /* Clear TREE_USED on all variables associated with a block scope.  */
  clear_tree_used (DECL_INITIAL (current_function_decl));
  n_debug("mtcsexpand.c expand_used_vars 11aa--\n");
  testprint(cfun);
  init_vars_expansion (self);
  n_debug("mtcsexpand.c expand_used_vars 11bb--\n");
  testprint(cfun);
  if (mtcsTarget->use_pseudo_pic_reg/*targetm.use_pseudo_pic_reg ptx 返回 false*/ (mtcsTarget))
    mtcsRTL->x_pic_offset_table_rtx/*pic_offset_table_rtx*/ = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,
            mtcs_mode_get_Pmode(mtcsMode));
  n_debug("mtcsexpand.c expand_used_vars 22\n");
  testprint(cfun);

  for (i = 0; i < SA.map->num_partitions; i++){
      if (bitmap_bit_p (SA.partitions_for_parm_default_defs, i))
          continue;
      tree var = partition_to_var (SA.map, i);
      n_debug("mtcsexpand.c expand_used_vars 22aa var:%p %p\n",var,SSA_NAME_VAR(var));
      aet_print_tree(var);
      aet_print_tree(SSA_NAME_VAR(var));
      gcc_assert (!virtual_operand_p (var));
      expand_one_ssa_partition (self,var);
  }

  if (flag_stack_protect == SPCT_FLAG_STRONG)
    gen_stack_protect_signal = stack_protect_return_slot_p ();

  n_debug("mtcsexpand.c expand_used_vars 33\n");
  /* At this point all variables on the local_decls with TREE_USED
     set are not associated with any block scope.  Lay them out.  */

  len = vec_safe_length (cfun->local_decls);
  n_debug("mtcsexpand.c expand_used_vars 33 LEN IS :%d cfun:%p\n",len,cfun);
  FOR_EACH_LOCAL_DECL (cfun, i, var){
      bool expand_now = false;

      /* Expanded above already.  */
      if (mtcs_gimple_expr_is_gimple_reg/*!is_gimple_reg*/(mtcsGimpleExpr,var)){
         n_debug("mtcsexpand.c expand_used_vars 33--00 is_gimple_reg %p\n",var);
          aet_print_tree(var);
          TREE_USED (var) = 0;
          goto next;
      }
      /* We didn't set a block for static or extern because it's hard
     to tell the difference between a global variable (re)declared
     in a local scope, and one that's really declared there to
     begin with.  And it doesn't really matter much, since we're
     not giving them stack space.  Expand them now.  */
      else if (TREE_STATIC (var) || DECL_EXTERNAL (var)){
         n_debug("mtcsexpand.c expand_used_vars 33--11 TREE_STATIC (var) || DECL_EXTERNAL (var) %p\n",var);
          expand_now = true;
      }
      /* Expand variables not associated with any block now.  Those created by
     the optimizers could be live anywhere in the function.  Those that
     could possibly have been scoped originally and detached from their
     block will have their allocation deferred so we coalesce them with
     others when optimization is enabled.  */
      else if (TREE_USED (var)){
         n_debug("mtcsexpand.c expand_used_vars 33--22 TREE_USED (var) %p\n",var);
          expand_now = true;
      }

      /* Finally, mark all variables on the list as used.  We'll use
     this in a moment when we expand those associated with scopes.  */
      TREE_USED (var) = 1;

      if (expand_now)
          expand_one_var (self,var, true, true, forced_stack_vars);

    next:
      if (DECL_ARTIFICIAL (var) && !DECL_IGNORED_P (var)){
         rtx rtl = DECL_RTL_IF_SET (var);
         n_debug("mtcsexpand.c expand_used_vars 33--33 TREE_USED (var) %p rtl:%p\n",var,rtl);
         mtcs_print_rtl_single(stderr,rtl);
         /* Keep artificial non-ignored vars in cfun->local_decls
            chain until instantiate_decls.  */
         if (rtl && (MEM_P (rtl) || GET_CODE (rtl) == CONCAT))
           add_local_decl (cfun, var);
         else if (rtl == NULL_RTX)
           /* If rtl isn't set yet, which can happen e.g. with
              -fstack-protector, retry before returning from this
              function.  */
           maybe_local_decls.safe_push (var);
      }
    }

  /* We duplicated some of the decls in CFUN->LOCAL_DECLS.

     +-----------------+-----------------+
     | ...processed... | ...duplicates...|
     +-----------------+-----------------+
                       ^
               +-- LEN points here.

     We just want the duplicates, as those are the artificial
     non-ignored vars that we want to keep until instantiate_decls.
     Move them down and truncate the array.  */
  if (!vec_safe_is_empty (cfun->local_decls))
    cfun->local_decls->block_remove (0, len);

  n_debug("mtcsexpand.c expand_used_vars 33dd\n");

  /* At this point, all variables within the block tree with TREE_USED
     set are actually used by the optimized function.  Lay them out.  */
  expand_used_vars_for_block (self,outer_block, true, forced_stack_vars);
  n_debug("mtcsexpand.c expand_used_vars 33ee\n");

  tree attribs = DECL_ATTRIBUTES (current_function_decl);
  if (self->stack_vars_num > 0){
     n_debug("mtcsexpand.c expand_used_vars 33ff stack_vars_num:%d\n",self->stack_vars_num);

      bool has_addressable_vars = false;

      add_scope_conflicts (self);

      /* If stack protection is enabled, we don't share space between
     vulnerable data and non-vulnerable data.  */
      if (flag_stack_protect != 0
      && !lookup_attribute ("no_stack_protector", attribs)
      && (flag_stack_protect != SPCT_FLAG_EXPLICIT
          || (flag_stack_protect == SPCT_FLAG_EXPLICIT
          && lookup_attribute ("stack_protect", attribs))))
          has_addressable_vars = add_stack_protection_conflicts (self);

      if (flag_stack_protect == SPCT_FLAG_STRONG && has_addressable_vars)
          gen_stack_protect_signal = true;

      /* Now that we have collected all stack variables, and have computed a
     minimal interference graph, attempt to save some stack space.  */
      partition_stack_vars (self);
      if (dump_file)
          dump_stack_var_partition (self);
      n_debug("mtcsexpand.c expand_used_vars 33ff--00 %d %d\n",has_addressable_vars,gen_stack_protect_signal);

  }

  n_debug("mtcsexpand.c expand_used_vars 44\n");

  if (!lookup_attribute ("no_stack_protector", attribs))
    switch (flag_stack_protect){
      case SPCT_FLAG_ALL:
         n_debug("mtcsexpand.c expand_used_vars 44aa\n");
          create_stack_guard (self);
          break;

      case SPCT_FLAG_STRONG:
        if (gen_stack_protect_signal
            || cfun->calls_alloca
            || self->has_protected_decls
            || lookup_attribute ("stack_protect", attribs)){
           n_debug("mtcsexpand.c expand_used_vars 44bb\n");

            create_stack_guard (self);
        }
        break;

      case SPCT_FLAG_DEFAULT:
        if (cfun->calls_alloca
            || self->has_protected_decls
            || lookup_attribute ("stack_protect", attribs)){
           n_debug("mtcsexpand.c expand_used_vars 44cc\n");

          create_stack_guard (self);
        }
        break;

      case SPCT_FLAG_EXPLICIT:
        if (lookup_attribute ("stack_protect", attribs)){
           n_debug("mtcsexpand.c expand_used_vars 44dd\n");

          create_stack_guard (self);
        }
        break;

      default:
          break;
    }
  n_debug("mtcsexpand.c expand_used_vars 55\n");

  /* Assign rtl to each variable based on these partitions.  */
  if (self->stack_vars_num > 0){
     n_debug("mtcsexpand.c expand_used_vars 55aa\n");

      class stack_vars_data data;

      data.asan_base = NULL_RTX;
      data.asan_alignb = 0;

      /* Reorder decls to be protected by iterating over the variables
     array multiple times, and allocating out of each phase in turn.  */
      /* ??? We could probably integrate this into the qsort we did
     earlier, such that we naturally see these variables first,
     and thus naturally allocate things in the right order.  */
      if (self->has_protected_decls){
         n_debug("mtcsexpand.c expand_used_vars 55bb\n");

          /* Phase 1 contains only character arrays.  */
          expand_stack_vars (self,stack_protect_decl_phase_1, &data);

          /* Phase 2 contains other kinds of arrays.  */
          if (!lookup_attribute ("no_stack_protector", attribs)
              && (flag_stack_protect == SPCT_FLAG_ALL
              || flag_stack_protect == SPCT_FLAG_STRONG
              || (flag_stack_protect == SPCT_FLAG_EXPLICIT
                  && lookup_attribute ("stack_protect", attribs)))){
             n_debug("mtcsexpand.c expand_used_vars 55cc\n");

            expand_stack_vars (self,stack_protect_decl_phase_2, &data);
          }
      }

      if (asan_sanitize_stack_p ()){
         n_debug("mtcsexpand.c expand_used_vars 55dd\n");

        /* Phase 3, any partitions that need asan protection
           in addition to phase 1 and 2.  */
        expand_stack_vars (self,asan_decl_phase_3, &data);
      }
      /* ASAN description strings don't yet have a syntax for expressing
     polynomial offsets.  */
      HOST_WIDE_INT prev_offset;
      if (!data.asan_vec.is_empty () && mtcsRtlData->x_frame_offset.is_constant (&prev_offset)){
         n_debug("mtcsexpand.c expand_used_vars 55ee\n");

          HOST_WIDE_INT offset, sz, redzonesz;
          redzonesz = ASAN_RED_ZONE_SIZE;
          sz = data.asan_vec[0] - prev_offset;
          if (data.asan_alignb > ASAN_RED_ZONE_SIZE  && data.asan_alignb <= 4096
              && sz + ASAN_RED_ZONE_SIZE >= (int) data.asan_alignb)
            redzonesz = ((sz + ASAN_RED_ZONE_SIZE + data.asan_alignb - 1) & ~(data.asan_alignb - HOST_WIDE_INT_1)) - sz;
          /* Allocating a constant amount of space from a constant
             starting offset must give a constant result.  */
          offset = (alloc_stack_frame_space (self,redzonesz, ASAN_RED_ZONE_SIZE).to_constant ());
          data.asan_vec.safe_push (prev_offset);
          data.asan_vec.safe_push (offset);
          /* Leave space for alignment if STRICT_ALIGNMENT.  */
          if (STRICT_ALIGNMENT)
            alloc_stack_frame_space (self,
                    (mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/ (mtcsMode,mtcsMode->modes.M_SImode) << ASAN_SHADOW_SHIFT) / BITS_PER_UNIT, 1);

          var_end_seq = asan_emit_stack_protection (mtcs_rtl_get_virtual_stack_args_rtx/*!virtual_stack_vars_rtx*/(mtcsRTL),
                          data.asan_base,
                          data.asan_alignb,
                          data.asan_vec.address (),
                          data.asan_decl_vec.address (),
                          data.asan_vec.length ());
      }
      n_debug("mtcsexpand.c expand_used_vars 55ff\n");

      expand_stack_vars (self,NULL, &data);
  }
  n_debug("mtcsexpand.c expand_used_vars 66 hwasan_sanitize_stack_p ():%d\n",hwasan_sanitize_stack_p ());

  if (hwasan_sanitize_stack_p ())
    hwasan_emit_prologue ();
  if (asan_sanitize_allocas_p () && cfun->calls_alloca){
    n_debug("mtcsexpand.c expand_used_vars 66aa 生成指令\n");
    var_end_seq = asan_emit_allocas_unpoison (mtcs_rtl_get_virtual_stack_dynamic_rtx/*!virtual_stack_dynamic_rtx*/(mtcsRTL),
          mtcs_rtl_get_virtual_stack_args_rtx/*!virtual_stack_vars_rtx*/(mtcsRTL),var_end_seq);
  }else if (hwasan_sanitize_allocas_p () && cfun->calls_alloca){
    /* When using out-of-line instrumentation we only want to emit one function
       call for clearing the tags in a region of shadow stack.  When there are
       alloca calls in this frame we want to emit a call using the
       virtual_stack_dynamic_rtx, but when not we use the hwasan_frame_extent
       rtx we created in expand_stack_vars.  */
     n_debug("mtcsexpand.c expand_used_vars 66bb 生成指令\n");

    var_end_seq = hwasan_emit_untag_frame (mtcs_rtl_get_virtual_stack_dynamic_rtx/*!virtual_stack_dynamic_rtx*/(mtcsRTL),
          mtcs_rtl_get_virtual_stack_args_rtx/*!virtual_stack_vars_rtx*/(mtcsRTL));
  }else if (hwasan_sanitize_stack_p ()){
    /* If no variables were stored on the stack, `hwasan_get_frame_extent`
       will return NULL_RTX and hence `hwasan_emit_untag_frame` will return
       NULL (i.e. an empty sequence).  */
     n_debug("mtcsexpand.c expand_used_vars 66cc 生成指令\n");

    var_end_seq = hwasan_emit_untag_frame (hwasan_get_frame_extent (),
          mtcs_rtl_get_virtual_stack_args_rtx/*!virtual_stack_vars_rtx*/(mtcsRTL));
  }
  fini_vars_expansion (self);
  n_debug("mtcsexpand.c expand_used_vars 77\n");

  /* If there were any artificial non-ignored vars without rtl
     found earlier, see if deferred stack allocation hasn't assigned
     rtl to them.  */
  FOR_EACH_VEC_ELT_REVERSE (maybe_local_decls, i, var){
     n_debug("mtcsexpand.c expand_used_vars 88\n");

      rtx rtl = DECL_RTL_IF_SET (var);
      /* Keep artificial non-ignored vars in cfun->local_decls
     chain until instantiate_decls.  */
      if (rtl && (MEM_P (rtl) || GET_CODE (rtl) == CONCAT))
          add_local_decl (cfun, var);
  }

  /* If the target requires that FRAME_OFFSET be aligned, do it.  */
  if (STACK_ALIGNMENT_NEEDED){
     n_debug("mtcsexpand.c expand_used_vars 99\n");

      HOST_WIDE_INT align = mtcs_func_get_preferred_stack_boundary/*!PREFERRED_STACK_BOUNDARY*/(mtcsFunc) / BITS_PER_UNIT;
      if (mtcs_func_get_frame_grows_downward/*!FRAME_GROWS_DOWNWARD*/(mtcsFunc))
          mtcsRtlData->x_frame_offset = aligned_lower_bound (mtcsRtlData->x_frame_offset, align);
      else
          mtcsRtlData->x_frame_offset = aligned_upper_bound (mtcsRtlData->x_frame_offset, align);
  }

  return var_end_seq;
}

/* Record the association between the RTL generated for partition PART
   and the underlying variable of the SSA_NAME VAR.  */
//原型 adjust_one_expanded_partition_var cfgexpand.cc
static void adjust_one_expanded_partition_var (MtcsExpand *self,tree var)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  if (!var)
    return;
  tree decl = SSA_NAME_VAR (var);
  int part = var_to_partition (SA.map, var);
  if (part == NO_PARTITION)
    return;

  rtx x = SA.partition_to_pseudo[part];
  gcc_assert (x);
  set_rtl(self,var, x);
  if (!REG_P (x))
    return;
  /* Note if the object is a user variable.  */
  if (decl && !DECL_ARTIFICIAL (decl))
    mark_user_reg (x);
  if (POINTER_TYPE_P (decl ? TREE_TYPE (decl) : TREE_TYPE (var)))
      mtcs_rtl_mark_reg_pointer/*!mark_reg_pointer*/(mtcsRTL,x, get_pointer_alignment (var));
}

//原型 expand_main_function cfgexpand.cc
static void expand_main_function (MtcsExpand *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
  MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);

#if (defined(INVOKE__main)              \
     || (!defined(HAS_INIT_SECTION)         \
     && !defined(INIT_SECTION_ASM_OP)       \
     && !defined(INIT_ARRAY_SECTION_ASM_OP)))
    mtcs_calls_emit_library_call/*!emit_library_call*/(mtcsCalls,
            mtcs_libfuncs_init_one_libfunc/*!init_one_libfunc*/(mtcsLibfuncs,NAME__MAIN), LCT_NORMAL, VOIDmode);
#endif
}

/* Expand code to initialize the stack_protect_guard.  This is invoked at
   the beginning of a function to be protected.  */
//原型 stack_protect_prologue cfgexpand.cc
static void stack_protect_prologue (MtcsExpand *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsAsm *mtcsAsm =(MtcsAsm *)mtcs_target_get_asm(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  tree guard_decl = mtcsTarget->stack_protect_guard/*!targetm.stack_protect_guard*/(mtcsTarget);
  rtx x, y;

  mtcsRtlData/*!crtl*/->stack_protect_guard_decl = guard_decl;
  x = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,mtcsRtlData/*!crtl*/->stack_protect_guard);

  if (target_rtx_have_stack_protect_combined_set/*!targetm.have_stack_protect_combined_set*/(mtcsMachine->tmrtx)
          && guard_decl){
      gcc_assert (DECL_P (guard_decl));
      y = mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,guard_decl);

      /* Allow the target to compute address of Y and copy it to X without
     leaking Y into a register.  This combined address + copy pattern
     allows the target to prevent spilling of any intermediate results by
     splitting it after register allocator.  */
      if (rtx_insn *insn =target_rtx_gen_stack_protect_combined_set/*!targetm.gen_stack_protect_combined_set*/
              (mtcsMachine->tmrtx,x, y)){
          mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,insn);
          return;
      }
  }

  if (guard_decl)
    y =mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,guard_decl);
  else
    y = const0_rtx;

  /* Allow the target to copy from Y to X without leaking Y into a
     register.  */
  if (target_rtx_have_stack_protect_set/*!targetm.have_stack_protect_set*/(mtcsMachine->tmrtx))
     if (rtx_insn *insn =target_rtx_gen_stack_protect_set/*!targetm.gen_stack_protect_set*/(mtcsMachine->tmrtx,x, y)){
        mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,insn);
        return;
     }

  /* Otherwise do a straight move.  */
  mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,x, y);
}

/* Create a basic block for initialization code.  */
/*
 * 为此前已发射的代码构建一个新的初始化基本块(INIT_BB), 此基本块被插入到函数的入口基本块(ENTRY_BB)和
 * 第一个真正的代码基本块(FIRST_BB)之间(汇编代码顺序),  插入后当前函数入口的流程就由 entry_bb => first_bb
 *  变为 entry_bb => init_bb => first_bb   当前 get_current_sequence 中已经发射的所有insns,
 *  都归于init_block中，这些insns就是当前函数的初始化代码。
 */
//原型 construct_init_block cfgexpand.cc
static basic_block construct_init_block (MtcsExpand *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
  MtcsCfg *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
  MtcsStmt *mtcsStmt=mtcs_target_get_stmt(mtcsTarget);

  basic_block init_block, first_block;
  edge e = NULL;
  int flags;

  /* Multiple entry points not supported yet.  */
  gcc_assert (EDGE_COUNT (ENTRY_BLOCK_PTR_FOR_FN (cfun)->succs) == 1);
  mtcs_cfg_rtl_init_rtl_bb_info/*!init_rtl_bb_info*/(mtcsCfgRtl,ENTRY_BLOCK_PTR_FOR_FN (cfun));
  mtcs_cfg_rtl_init_rtl_bb_info/*!init_rtl_bb_info*/(mtcsCfgRtl,EXIT_BLOCK_PTR_FOR_FN (cfun));
  ENTRY_BLOCK_PTR_FOR_FN (cfun)->flags |= BB_RTL;
  EXIT_BLOCK_PTR_FOR_FN (cfun)->flags |= BB_RTL;

  e = EDGE_SUCC (ENTRY_BLOCK_PTR_FOR_FN (cfun), 0);

  /* When entry edge points to first basic block, we don't need jump,
     otherwise we have to jump into proper target.  */
  if (e && e->dest != ENTRY_BLOCK_PTR_FOR_FN (cfun)->next_bb){
      tree label = gimple_block_label (e->dest);
      mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,mtcs_stmt_jump_target_rtx/*!jump_target_rtx*/(mtcsStmt,label));
      flags = 0;
  }else
      flags = EDGE_FALLTHRU;

  init_block = mtcs_cfg_context_create_basic_block/*!create_basic_block*/(mtcsCfgContext,
        NEXT_INSN (mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData)),
          mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData),
                   ENTRY_BLOCK_PTR_FOR_FN (cfun));
  init_block->count = ENTRY_BLOCK_PTR_FOR_FN (cfun)->count;
  add_bb_to_loop (init_block, ENTRY_BLOCK_PTR_FOR_FN (cfun)->loop_father);
  n_debug("mtcsexpand.c construct_init_block 00 创建初始块:%p :end insn:%p prev_bb就是ENTRY_BB:%p e:%p\n",
        init_block,mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData),ENTRY_BLOCK_PTR_FOR_FN (cfun),e);

  if (e){
      first_block = e->dest;
      mtcs_cfg_redirect_edge_succ/*!redirect_edge_succ*/(mtcsCfg,e, init_block);
      mtcs_cfg_make_single_succ_edge/*!make_single_succ_edge*/(mtcsCfg,init_block, first_block, flags);
  }else
     mtcs_cfg_make_single_succ_edge/*!make_single_succ_edge*/(mtcsCfg,
           init_block, EXIT_BLOCK_PTR_FOR_FN (cfun),EDGE_FALLTHRU);

  mtcs_cfg_rtl_update_bb_for_insn/*!update_bb_for_insn*/(mtcsCfgRtl,init_block);
  return init_block;
}

/* Return an RTX equivalent to the value of the parameter DECL.  */
//原型 expand_debug_parm_decl
static rtx expand_debug_parm_decl (MtcsExpand *self,tree decl)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  rtx incoming = DECL_INCOMING_RTL (decl);

  if (incoming
      && GET_MODE (incoming) != mtcsMode->modes.M_BLKmode
      && ((REG_P (incoming) && mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,incoming))
      || (MEM_P (incoming)
          && REG_P (XEXP (incoming, 0))
          &&  mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,XEXP (incoming, 0))))){
      rtx rtl = gen_rtx_ENTRY_VALUE (GET_MODE (incoming));

#ifdef HAVE_window_save
      /* DECL_INCOMING_RTL uses the INCOMING_REGNO of parameter registers.
     If the target machine has an explicit window save instruction, the
     actual entry value is the corresponding OUTGOING_REGNO instead.  */
      if (REG_P (incoming)
      && OUTGOING_REGNO (REGNO (incoming)) != REGNO (incoming))
    incoming
      = gen_rtx_REG_offset (incoming, GET_MODE (incoming),
                OUTGOING_REGNO (REGNO (incoming)), 0);
      else if (MEM_P (incoming))
    {
      rtx reg = XEXP (incoming, 0);
      if (OUTGOING_REGNO (REGNO (reg)) != REGNO (reg))
        {
          reg = mtcs_rtl_gen_raw_REG/*!gen_raw_REG*/(mtcsRTL,GET_MODE (reg), OUTGOING_REGNO (REGNO (reg)));
          incoming = replace_equiv_address_nv (incoming, reg);
        }
      else
        incoming = copy_rtx (incoming);
    }
#endif

      ENTRY_VALUE_EXP (rtl) = incoming;
      return rtl;
  }

  if (incoming
      && GET_MODE (incoming) !=  mtcsMode->modes.M_BLKmode
      && !TREE_ADDRESSABLE (decl)
      && MEM_P (incoming)
      && (XEXP (incoming, 0) == mtcs_rtl_get_virtual_incoming_args_rtx/*!virtual_incoming_args_rtx*/(mtcsRTL)
      || (GET_CODE (XEXP (incoming, 0)) == PLUS
          && XEXP (XEXP (incoming, 0), 0) ==  mtcs_rtl_get_virtual_incoming_args_rtx/*!virtual_incoming_args_rtx*/(mtcsRTL)
          && CONST_INT_P (XEXP (XEXP (incoming, 0), 1)))))
    return copy_rtx (incoming);

  return NULL_RTX;
}

/* Return an RTX equivalent to the source bind value of the tree expression
   EXP.  */
//原型 expand_debug_source_expr cfgexpand.cc
static rtx expand_debug_source_expr (MtcsExpand *self,tree exp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

  rtx op0 = NULL_RTX;
  machine_mode mode = VOIDmode, inner_mode;

  switch (TREE_CODE (exp)){
    case VAR_DECL:
      if (DECL_ABSTRACT_ORIGIN (exp))
          return expand_debug_source_expr(self,DECL_ABSTRACT_ORIGIN (exp));
      break;
    case PARM_DECL:
      {
        mode = DECL_MODE (exp);
        op0 = expand_debug_parm_decl(self,exp);
        if (op0)
           break;
        /* See if this isn't an argument that has been completely
           optimized out.  */
        if (!DECL_RTL_SET_P (exp) && !DECL_INCOMING_RTL (exp)
            && DECL_ABSTRACT_ORIGIN (current_function_decl)){
            tree aexp = DECL_ORIGIN (exp);
            if (DECL_CONTEXT (aexp) == DECL_ABSTRACT_ORIGIN (current_function_decl)){
                vec<tree, va_gc> **debug_args;
                unsigned int ix;
                tree ddecl;
                debug_args = decl_debug_args_lookup (current_function_decl);
                if (debug_args != NULL){
                    for (ix = 0; vec_safe_iterate (*debug_args, ix, &ddecl);ix += 2)
                      if (ddecl == aexp)
                          return gen_rtx_DEBUG_PARAMETER_REF (mode, aexp);
                }
            }
        }
        break;
      }
    default:
      break;
  }

  if (op0 == NULL_RTX)
    return NULL_RTX;

  inner_mode = GET_MODE (op0);
  if (mode == inner_mode)
    return op0;

  if (mtcs_mode_is_float_p/*!FLOAT_MODE_P*/(mtcsMode,mode) && mtcs_mode_is_float_p/*!FLOAT_MODE_P*/(mtcsMode,inner_mode)){
      if (mtcs_mode_get_unit_bitsize/*!GET_MODE_UNIT_BITSIZE*/(mtcsMode,mode) ==
              mtcs_mode_get_unit_bitsize/*!GET_MODE_UNIT_BITSIZE*/(mtcsMode,inner_mode))
          op0 = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,mode, op0, inner_mode, 0);
      else if (mtcs_mode_get_unit_bitsize/*!GET_MODE_UNIT_BITSIZE*/(mtcsMode,mode) <
              mtcs_mode_get_unit_bitsize/*!GET_MODE_UNIT_BITSIZE*/(mtcsMode,inner_mode))
          op0 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,FLOAT_TRUNCATE, mode, op0, inner_mode);
      else{
         n_debug("mtcsexpand.c expand_debug_source_expr FLOAT_EXTEND mode:%d inner_mode:%d\n",mode,inner_mode);

          op0 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,FLOAT_EXTEND, mode, op0, inner_mode);
      }
  }else if (mtcs_mode_is_float_p/*!FLOAT_MODE_P*/(mtcsMode,mode))
    gcc_unreachable ();
  else if (mtcs_mode_is_float_p/*!FLOAT_MODE_P*/(mtcsMode,inner_mode)){
      if (TYPE_UNSIGNED (TREE_TYPE (exp)))
          op0 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,UNSIGNED_FIX, mode, op0, inner_mode);
      else
          op0 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,FIX, mode, op0, inner_mode);
  }else if (mtcs_mode_get_unit_precision/*!GET_MODE_UNIT_PRECISION*/(mtcsMode,mode)==
          mtcs_mode_get_unit_precision/*!GET_MODE_UNIT_PRECISION*/(mtcsMode,inner_mode))
    op0 = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, op0, inner_mode);
  else if (mtcs_mode_get_unit_precision/*!GET_MODE_UNIT_PRECISION*/(mtcsMode,mode) <
          mtcs_mode_get_unit_precision/*!GET_MODE_UNIT_PRECISION*/(mtcsMode,inner_mode))
    op0 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,TRUNCATE, mode, op0, inner_mode);
  else if (TYPE_UNSIGNED (TREE_TYPE (exp)))
    op0 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,ZERO_EXTEND, mode, op0, inner_mode);
  else
    op0 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,SIGN_EXTEND, mode, op0, inner_mode);

  return op0;
}

/* Ensure INSN_VAR_LOCATION_LOC (insn) doesn't have unbound complexity.
   Allow 4 levels of rtl nesting for most rtl codes, and if we see anything
   deeper than that, create DEBUG_EXPRs and emit DEBUG_INSNs before INSN.  */
//原型 avoid_complex_debug_insns cfgexpand.cc
static void avoid_complex_debug_insns (MtcsExpand *self,rtx_insn *insn, rtx *exp_p, int depth)
{
  rtx exp = *exp_p;
  if (exp == NULL_RTX)
    return;
  if ((OBJECT_P (exp) && !MEM_P (exp)) || GET_CODE (exp) == CLOBBER)
    return;
  if (depth == 4){
      /* Create DEBUG_EXPR (and DEBUG_EXPR_DECL).  */
      rtx dval = make_debug_expr_from_rtl (exp);
      /* Emit a debug bind insn before INSN.  */
      rtx bind = gen_rtx_VAR_LOCATION (GET_MODE (exp),
                       DEBUG_EXPR_TREE_DECL (dval), exp,
                       VAR_INIT_STATUS_INITIALIZED);
      emit_debug_insn_before (bind, insn);
      *exp_p = dval;
      return;
  }

  const char *format_ptr = GET_RTX_FORMAT (GET_CODE (exp));
  int i, j;
  for (i = 0; i < GET_RTX_LENGTH (GET_CODE (exp)); i++)
      switch (*format_ptr++){
         case 'e':
            avoid_complex_debug_insns (self,insn, &XEXP (exp, i), depth + 1);
            break;

         case 'E':
         case 'V':
            for (j = 0; j < XVECLEN (exp, i); j++)
               avoid_complex_debug_insns (self,insn, &XVECEXP (exp, i, j), depth + 1);
            break;

         default:
            break;
      }
}


/* Expand the _LOCs in debug insns.  We run this after expanding all
   regular insns, so that any variables referenced in the function
   will have their DECL_RTLs set.  */
//原型 expand_debug_locations cfgexpand.cc
static void expand_debug_locations (MtcsExpand *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *opts=mtcsOptions->global_options;

  rtx_insn *insn;
  rtx_insn *last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
  int save_strict_alias = opts->x_flag_strict_aliasing;
  /* New alias sets while setting up memory attributes cause
     -fcompare-debug failures, even though it doesn't bring about any
     codegen changes.  */
  opts->x_flag_strict_aliasing = 0;//改变flag

  for (insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = NEXT_INSN (insn))
     if (DEBUG_BIND_INSN_P (insn)){
        tree value = (tree)INSN_VAR_LOCATION_LOC (insn);
        rtx val;
        rtx_insn *prev_insn, *insn2;
        machine_mode mode;

        if (value == NULL_TREE)
          val = NULL_RTX;
        else{
            if (INSN_VAR_LOCATION_STATUS (insn) == VAR_INIT_STATUS_UNINITIALIZED)
              val = expand_debug_source_expr(self,value);
            /* The avoid_deep_ter_for_debug function inserts
               debug bind stmts after SSA_NAME definition, with the
               SSA_NAME as the whole bind location.  Disable temporarily
               expansion of that SSA_NAME into the DEBUG_EXPR_DECL
               being defined in this DEBUG_INSN.  */
            else if (self->deep_ter_debug_map && TREE_CODE (value) == SSA_NAME){
                tree *slot = self->deep_ter_debug_map->get (value);
                if (slot){
                    if (*slot == INSN_VAR_LOCATION_DECL (insn))
                      *slot = NULL_TREE;
                    else
                      slot = NULL;
                }
                val = expand_debug_expr(self,value);
                if (slot)
                  *slot = INSN_VAR_LOCATION_DECL (insn);
            }else
              val = expand_debug_expr(self,value);
            gcc_assert (last == mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData));
        }

        if (!val)
          val = gen_rtx_UNKNOWN_VAR_LOC ();
        else{
            mode = GET_MODE (INSN_VAR_LOCATION (insn));

            gcc_assert (mode == GET_MODE (val)
                || (GET_MODE (val) == VOIDmode
                    && (CONST_SCALAR_INT_P (val)
                    || GET_CODE (val) == CONST_FIXED
                    || GET_CODE (val) == LABEL_REF)));
        }

        INSN_VAR_LOCATION_LOC (insn) = val;
        prev_insn = PREV_INSN (insn);
        for (insn2 = insn; insn2 != prev_insn; insn2 = PREV_INSN (insn2))
           avoid_complex_debug_insns(self,insn2, &INSN_VAR_LOCATION_LOC (insn2), 0);
     }

  opts->x_flag_strict_aliasing = save_strict_alias;
}

/* Performs swapping operands of commutative operations to expand
   the expensive one first.  */
//原型 reorder_operands cfgexpand.cc
static void reorder_operands (MtcsExpand *self,basic_block bb)
{
  unsigned int *lattice;  /* Hold cost of each statement.  */
  unsigned int i = 0, n = 0;
  gimple_stmt_iterator gsi;
  gimple_seq stmts;
  gimple *stmt;
  bool swap;
  tree op0, op1;
  ssa_op_iter iter;
  use_operand_p use_p;
  gimple *def0, *def1;

  /* Compute cost of each statement using estimate_num_insns.  */
  stmts = bb_seq (bb);
  for (gsi = gsi_start (stmts); !gsi_end_p (gsi); gsi_next (&gsi)){
      stmt = gsi_stmt (gsi);
      if (!is_gimple_debug (stmt))
        gimple_set_uid (stmt, n++);
  }
  lattice = XNEWVEC (unsigned int, n);
  for (gsi = gsi_start (stmts); !gsi_end_p (gsi); gsi_next (&gsi)){
      unsigned cost;
      stmt = gsi_stmt (gsi);
      if (is_gimple_debug (stmt))
          continue;
      cost = estimate_num_insns (stmt, &eni_size_weights);
      lattice[i] = cost;
      FOR_EACH_SSA_USE_OPERAND (use_p, stmt, iter, SSA_OP_USE){
          tree use = USE_FROM_PTR (use_p);
          gimple *def_stmt;
          if (TREE_CODE (use) != SSA_NAME)
            continue;
          def_stmt = get_gimple_for_ssa_name (use);
          if (!def_stmt)
            continue;
          lattice[i] += lattice[gimple_uid (def_stmt)];
      }
      i++;
      if (!is_gimple_assign (stmt) || !commutative_tree_code (gimple_assign_rhs_code (stmt)))
          continue;
      op0 = gimple_op (stmt, 1);
      op1 = gimple_op (stmt, 2);
      if (TREE_CODE (op0) != SSA_NAME  || TREE_CODE (op1) != SSA_NAME)
          continue;
      /* Swap operands if the second one is more expensive.  */
      def0 = get_gimple_for_ssa_name (op0);
      def1 = get_gimple_for_ssa_name (op1);
      if (!def1)
          continue;
      swap = false;
      if (!def0 || lattice[gimple_uid (def1)] > lattice[gimple_uid (def0)])
          swap = true;
      if (swap){
          if (dump_file && (dump_flags & TDF_DETAILS)){
              fprintf (dump_file, "Swap operands in stmt:\n");
              print_gimple_stmt (dump_file, stmt, 0, TDF_SLIM);
              fprintf (dump_file, "Cost left opnd=%d, right opnd=%d\n",
                   def0 ? lattice[gimple_uid (def0)] : 0,
                   lattice[gimple_uid (def1)]);
          }
          swap_ssa_operands (stmt, gimple_assign_rhs1_ptr (stmt),
                     gimple_assign_rhs2_ptr (stmt));
      }
  }
  XDELETE (lattice);
}

/* Emit code to jump to the address
   specified by the pointer expression EXP.  */
//原型 expand_computed_goto cfgexpand.cc
static void expand_computed_goto (MtcsExpand *self,tree exp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr *mtcsExpr =mtcs_target_get_expr(mtcsTarget);
  MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  rtx x = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,exp);
  mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
  mtcs_optabs_emit_indirect_jump/*!emit_indirect_jump*/(mtcsOptabs,x);
}



/* Generate RTL code for a `goto' statement with target label LABEL.
   LABEL should be a LABEL_DECL tree node that was or will later be
   defined with `expand_label'.  */
//原型 expand_goto cfgexpand.cc
static void expand_goto (MtcsExpand *self,tree label)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsStmt *mtcsStmt=mtcs_target_get_stmt(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  if (mtcsOptionsItem->x_flag_checking){
      /* Check for a nonlocal goto to a containing function.  Should have
     gotten translated to __builtin_nonlocal_goto.  */
      tree context = decl_function_context (label);
      gcc_assert (!context || context == current_function_decl);
  }
  mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,mtcs_stmt_jump_target_rtx/*!jump_target_rtx*/(mtcsStmt,label));
}

/* Generate RTL to return from the current function, with value VAL.  */
//原型 expand_value_return cfgexpand.cc
static void expand_value_return (MtcsExpand *self,rtx val)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  n_debug("mtcsexpand.c expand_value_return-- 00\n");
  mtcs_print_rtl_single(stderr,val);
  /* Copy the value to the return location unless it's already there.  */
  tree decl = DECL_RESULT (current_function_decl);
  rtx return_reg = mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl);
  if (return_reg != val){
      tree funtype = TREE_TYPE (current_function_decl);
      tree type = TREE_TYPE (decl);
      int unsignedp = TYPE_UNSIGNED (type);
      machine_mode old_mode = DECL_MODE (decl);
      machine_mode mode;
      if (DECL_BY_REFERENCE (decl))
         mode = mtcs_mode_promote_function_mode/*!promote_function_mode*/(mtcsMode,type, old_mode, &unsignedp, funtype, 2);
      else
         mode = mtcs_mode_promote_function_mode/*!promote_function_mode*/(mtcsMode,type, old_mode, &unsignedp, funtype, 1);

      if (mode != old_mode){
          /* Some ABIs require scalar floating point modes to be returned
             in a wider scalar integer mode.  We need to explicitly
             reinterpret to an integer mode of the correct precision
             before extending to the desired result.  */
          if (mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,mode)
              && mtcs_mode_is_scalar_float_p/*!SCALAR_FLOAT_MODE_P*/(mtcsMode,old_mode)
              && known_gt (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode),
                      mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,old_mode)))
            val = mtcs_expr_convert_float_to_wider_int/*!convert_float_to_wider_int*/(mtcsExpr,mode, old_mode, val);
          else
            val = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,mode, old_mode, val, unsignedp);
      }
      n_debug("mtcsexpand.c expand_value_return-- 11 %d\n",GET_CODE (return_reg) == PARALLEL);
      mtcs_print_rtl_single(stderr,return_reg);
      if (GET_CODE (return_reg) == PARALLEL)
          mtcs_expr_emit_group_load/*!emit_group_load*/(mtcsExpr,return_reg, val, type, int_size_in_bytes (type));
      else
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,return_reg, val);
  }
  expand_null_return_1(self);
}

/* Generate RTL to evaluate the expression RETVAL and return it
   from the current function.  */
//原型 expand_return cfgexpand.cc
static void expand_return (MtcsExpand *self,tree retval)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   rtx result_rtl;
   rtx val = 0;
   tree retval_rhs;

   /* If function wants no value, give it none.  */
   if (VOID_TYPE_P (TREE_TYPE (TREE_TYPE (current_function_decl)))){
      n_debug("mtcsexpand.c expand_return 00\n");
      mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,retval);
      mtcs_expand_expand_null_return/*!expand_null_return*/(self);
      return;
   }

   if (retval == error_mark_node){
      /* Treat this like a return of no value from a function that
      returns a value.  */
      n_debug("mtcsexpand.c expand_return 11\n");
      mtcs_expand_expand_null_return/*!expand_null_return*/(self);
      return;
   }else if ((TREE_CODE (retval) == MODIFY_EXPR
   || TREE_CODE (retval) == INIT_EXPR)
   && TREE_CODE (TREE_OPERAND (retval, 0)) == RESULT_DECL)
      retval_rhs = TREE_OPERAND (retval, 1);
   else
      retval_rhs = retval;

   result_rtl = mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,DECL_RESULT (current_function_decl));
   /* If we are returning the RESULT_DECL, then the value has already
   been stored into it, so we don't have to do anything special.  */
   if (TREE_CODE (retval_rhs) == RESULT_DECL){
      n_debug("mtcsexpand.c expand_return 22\n");

      expand_value_return(self,result_rtl);
      /* If the result is an aggregate that is being returned in one (or more)
      registers, load the registers here.  */
   }else if (retval_rhs != 0  && TYPE_MODE (TREE_TYPE (retval_rhs)) ==mtcsMode->modes.M_BLKmode
   && REG_P (result_rtl)){
      n_debug("mtcsexpand.c expand_return 33\n");
      val = copy_blkmode_to_reg (GET_MODE (result_rtl), retval_rhs);
      if (val){
         n_debug("mtcsexpand.c expand_return 44\n");
         /* Use the mode of the result value on the return register.  */
         mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,result_rtl, GET_MODE (val));
         expand_value_return(self,val);
      }else{
         n_debug("mtcsexpand.c expand_return 55\n");
         mtcs_expand_expand_null_return/*!expand_null_return*/(self);
      }
   }else if (retval_rhs != 0  && !VOID_TYPE_P (TREE_TYPE (retval_rhs))
   && (REG_P (result_rtl)  || (GET_CODE (result_rtl) == PARALLEL))){
      n_debug("mtcsexpand.c expand_return 66\n");
      /* Compute the return value into a temporary (usually a pseudo reg).  */
      val = mtcs_func_assign_temp/*!assign_temp*/(mtcsFunc,TREE_TYPE (DECL_RESULT (current_function_decl)), 0, 1);
      val = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,retval_rhs, val, GET_MODE (val), EXPAND_NORMAL);
      val = mtcs_explow_force_not_mem/*!force_not_mem*/(mtcsExplow,val);
      expand_value_return(self,val);
   }else{
      n_debug("mtcsexpand.c expand_return 77\n");
      /* No hard reg used; calculate value into hard return reg.  */
      mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,retval, const0_rtx, VOIDmode, EXPAND_NORMAL);
      expand_value_return(self,result_rtl);
   }
}

/* Expand a clobber of LHS.  If LHS is stored it in a multi-part
   register, tell the rtl optimizers that its value is no longer
   needed.  */
//原型 expand_clobber cfgexpand.cc
static void expand_clobber (MtcsExpand *self,tree lhs)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  if (DECL_P (lhs)){
      rtx decl_rtl = DECL_RTL_IF_SET (lhs);
      if (decl_rtl && REG_P (decl_rtl)){
          machine_mode decl_mode = GET_MODE (decl_rtl);
          if (maybe_gt (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,decl_mode),
                  mtcs_mode_get_regmode_natural_size/*!REGMODE_NATURAL_SIZE*/(mtcsMode,decl_mode)))
              mtcs_emit_emit_clobber/*!emit_clobber*/(mtcsEmit,decl_rtl);
      }
  }
}


/* Convert X to MODE, that must be Pmode or ptr_mode, without emitting
   any rtl.  */
//原型 convert_debug_memory_address cfgexpand.cc
static rtx convert_debug_memory_address (MtcsExpand *self,scalar_int_mode mode, rtx x,
                  addr_space_t as)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  if(!mtcs_config_ifdef(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED/*!#ifdef POINTERS_EXTEND_UNSIGNED*/)){
      gcc_assert (mode == mtcs_mode_get_Pmode(mtcsMode)
              || mode ==target_addr_space_address_mode/*!targetm.addr_space.address_mode*/(mtcsMachine->addrSpace,as));
      gcc_assert (GET_MODE (x) == mode || GET_MODE (x) == VOIDmode);
  }else/*!#else*/{
      rtx temp;

      gcc_assert (target_addr_space_valid_pointer_mode/*!targetm.addr_space.valid_pointer_mode*/(mtcsMachine->addrSpace,mode, as));

      if (GET_MODE (x) == mode || GET_MODE (x) == VOIDmode)
        return x;

      /* X must have some form of address mode already.  */
      scalar_int_mode xmode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,GET_MODE (x));
      if (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode) < mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,xmode))
        x = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, x, xmode);
      else if (mtcs_config_get_value(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED)/*!POINTERS_EXTEND_UNSIGNED*/ > 0)
        x = gen_rtx_ZERO_EXTEND (mode, x);
      else if (!mtcs_config_get_value(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED)/*!POINTERS_EXTEND_UNSIGNED*/)
        x = gen_rtx_SIGN_EXTEND (mode, x);
      else{
          switch (GET_CODE (x)){
            case SUBREG:
              if ((SUBREG_PROMOTED_VAR_P (x)
                   || (REG_P (SUBREG_REG (x)) && REG_POINTER (SUBREG_REG (x)))
                   || (GET_CODE (SUBREG_REG (x)) == PLUS
                   && REG_P (XEXP (SUBREG_REG (x), 0))
                   && REG_POINTER (XEXP (SUBREG_REG (x), 0))
                   && CONST_INT_P (XEXP (SUBREG_REG (x), 1))))
                  && GET_MODE (SUBREG_REG (x)) == mode)
                return SUBREG_REG (x);
              break;
            case LABEL_REF:
              temp = gen_rtx_LABEL_REF (mode, label_ref_label (x));
              LABEL_REF_NONLOCAL_P (temp) = LABEL_REF_NONLOCAL_P (x);
              return temp;
            case SYMBOL_REF:
              temp = shallow_copy_rtx (x);
              mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,temp, mode);
              return temp;
            case CONST:
              temp = convert_debug_memory_address(self,mode, XEXP (x, 0), as);
              if (temp)
                temp = gen_rtx_CONST (mode, temp);
              return temp;
            case PLUS:
            case MINUS:
              if (CONST_INT_P (XEXP (x, 1))){
                  temp = convert_debug_memory_address(self,mode, XEXP (x, 0), as);
                  if (temp)
                      return gen_rtx_fmt_ee (GET_CODE (x), mode, temp, XEXP (x, 1));
              }
              break;
            default:
              break;
          }
          /* Don't know how to express ptr_extend as operation in debug info.  */
          return NULL;
      }
  }/*!#endif*/ /* POINTERS_EXTEND_UNSIGNED */

  return x;
}

/* Return the difference between the floor and the truncated result of
   a signed division by OP1 with remainder MOD.  */
//原型 floor_sdiv_adjust cfgexpand.cc
static rtx floor_sdiv_adjust (MtcsExpand *self,machine_mode mode, rtx mod, rtx op1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  /* (mod != 0 ? (op1 / mod < 0 ? -1 : 0) : 0) */
  return gen_rtx_IF_THEN_ELSE
    (mode, gen_rtx_NE (mtcsMode->modes.M_BImode, mod, const0_rtx),
     gen_rtx_IF_THEN_ELSE
     (mode, gen_rtx_LT (mtcsMode->modes.M_BImode,
            gen_rtx_DIV (mode, op1, mod),
            const0_rtx),
      constm1_rtx, const0_rtx),
     const0_rtx);
}

/* Return the difference between the ceil and the truncated result of
   an unsigned division by OP1 with remainder MOD.  */
//原型 ceil_udiv_adjust cfgexpand.cc
static rtx ceil_udiv_adjust (MtcsExpand *self,machine_mode mode, rtx mod, rtx op1 ATTRIBUTE_UNUSED)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  /* (mod != 0 ? 1 : 0) */
  return gen_rtx_IF_THEN_ELSE
    (mode, gen_rtx_NE (mtcsMode->modes.M_BImode, mod, const0_rtx),
     const1_rtx, const0_rtx);
}


/* Return the difference between the ceil and the truncated result of
   a signed division by OP1 with remainder MOD.  */
//原型 ceil_sdiv_adjust cfgexpand.cc
static rtx ceil_sdiv_adjust (MtcsExpand *self,machine_mode mode, rtx mod, rtx op1)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

  /* (mod != 0 ? (op1 / mod > 0 ? 1 : 0) : 0) */
  return gen_rtx_IF_THEN_ELSE
    (mode, gen_rtx_NE (mtcsMode->modes.M_BImode, mod, const0_rtx),
     gen_rtx_IF_THEN_ELSE
     (mode, gen_rtx_GT (mtcsMode->modes.M_BImode,
            gen_rtx_DIV (mode, op1, mod),
            const0_rtx),
      const1_rtx, const0_rtx),
     const0_rtx);
}


/* Return the difference between the rounded and the truncated result
   of a signed division by OP1 with remainder MOD.  Halfway cases are
   rounded away from zero, rather than to the nearest even number.  */
//原型 round_sdiv_adjust cfgexpand.cc
static rtx round_sdiv_adjust (MtcsExpand *self,machine_mode mode, rtx mod, rtx op1)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

  /* (abs (mod) >= abs (op1) - abs (mod)
      ? (op1 / mod > 0 ? 1 : -1)
      : 0) */
  return gen_rtx_IF_THEN_ELSE
    (mode, gen_rtx_GE (mtcsMode->modes.M_BImode, gen_rtx_ABS (mode, mod),
               gen_rtx_MINUS (mode,
                      gen_rtx_ABS (mode, op1),
                      gen_rtx_ABS (mode, mod))),
     gen_rtx_IF_THEN_ELSE
     (mode, gen_rtx_GT (mtcsMode->modes.M_BImode,
            gen_rtx_DIV (mode, op1, mod),
            const0_rtx),
      const1_rtx, constm1_rtx),
     const0_rtx);
}

/* Return the difference between the rounded and the truncated result
   of a unsigned division by OP1 with remainder MOD.  Halfway cases
   are rounded away from zero, rather than to the nearest even
   number.  */
//原型 round_udiv_adjust cfgexpand.cc
static rtx round_udiv_adjust (MtcsExpand *self,machine_mode mode, rtx mod, rtx op1)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  /* (mod >= op1 - mod ? 1 : 0) */
  return gen_rtx_IF_THEN_ELSE
    (mode, gen_rtx_GE (mtcsMode->modes.M_BImode, mod,
               gen_rtx_MINUS (mode, op1, mod)),
     const1_rtx, const0_rtx);
}


/* Return an RTX equivalent to the value of the tree expression EXP.  */
//原型 expand_debug_expr cfgexpand.cc
static rtx expand_debug_expr (MtcsExpand *self,tree exp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
  MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  rtx op0 = NULL_RTX, op1 = NULL_RTX, op2 = NULL_RTX;
  machine_mode mode = TYPE_MODE (TREE_TYPE (exp));
  machine_mode inner_mode = VOIDmode;
  int unsignedp = TYPE_UNSIGNED (TREE_TYPE (exp));
  addr_space_t as;
  scalar_int_mode op0_mode, op1_mode, addr_mode;

  switch (TREE_CODE_CLASS (TREE_CODE (exp))){
     case tcc_expression:
        switch (TREE_CODE (exp)){
            case COND_EXPR:
            case DOT_PROD_EXPR:
            case SAD_EXPR:
            case WIDEN_MULT_PLUS_EXPR:
            case WIDEN_MULT_MINUS_EXPR:
               goto ternary;

            case TRUTH_ANDIF_EXPR:
            case TRUTH_ORIF_EXPR:
            case TRUTH_AND_EXPR:
            case TRUTH_OR_EXPR:
            case TRUTH_XOR_EXPR:
               goto binary;
            case TRUTH_NOT_EXPR:
               goto unary;
            default:
               break;
        }
        break;

        ternary:
        op2 = expand_debug_expr(self,TREE_OPERAND (exp, 2));
        if (!op2)
           return NULL_RTX;
        /* Fall through.  */
     binary:
     case tcc_binary:
        if (mode == mtcsMode->modes.M_BLKmode)
            return NULL_RTX;
        op1 = expand_debug_expr(self,TREE_OPERAND (exp, 1));
        if (!op1)
            return NULL_RTX;
        switch (TREE_CODE (exp)){
            case LSHIFT_EXPR:
            case RSHIFT_EXPR:
            case LROTATE_EXPR:
            case RROTATE_EXPR:
            case WIDEN_LSHIFT_EXPR:
              /* Ensure second operand isn't wider than the first one.  */
              inner_mode = TYPE_MODE (TREE_TYPE (TREE_OPERAND (exp, 1)));
              if (mtcs_mode_is_a <scalar_int_mode> (mtcsMode,inner_mode, &op1_mode)
                  && (mtcs_mode_get_unit_precision/*!GET_MODE_UNIT_PRECISION*/(mtcsMode,mode)
                  < mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,op1_mode)))
                op1 = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,
                        mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode), op1, op1_mode);
              break;
            default:
              break;
        }
      /* Fall through.  */

     unary:
     case tcc_unary:
        if (mode == mtcsMode->modes.M_BLKmode)
           return NULL_RTX;
        inner_mode = TYPE_MODE (TREE_TYPE (TREE_OPERAND (exp, 0)));
        op0 = expand_debug_expr(self,TREE_OPERAND (exp, 0));
        if (!op0)
            return NULL_RTX;
      break;

     case tcc_comparison:
        unsignedp = TYPE_UNSIGNED (TREE_TYPE (TREE_OPERAND (exp, 0)));
        goto binary;

     case tcc_type:
     case tcc_statement:
        gcc_unreachable ();

     case tcc_constant:
     case tcc_exceptional:
     case tcc_declaration:
     case tcc_reference:
     case tcc_vl_exp:
        break;
  }

  switch (TREE_CODE (exp)){
     case STRING_CST:
        if (!lookup_constant_def (exp)){
            if (strlen (TREE_STRING_POINTER (exp)) + 1 != (size_t) TREE_STRING_LENGTH (exp))
                return NULL_RTX;
            op0 = gen_rtx_CONST_STRING (Pmode, TREE_STRING_POINTER (exp));
            op0 = gen_rtx_MEM (mtcsMode->modes.M_BLKmode, op0);
            mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,op0, exp, 0);
            return op0;
        }
         /* Fall through.  */

     case INTEGER_CST:
        if (TREE_CODE (TREE_TYPE (exp)) == BITINT_TYPE  && TYPE_MODE (TREE_TYPE (exp)) == mtcsMode->modes.M_BLKmode)
            return NULL;
      /* FALLTHRU */
     case REAL_CST:
     case FIXED_CST:
        op0 = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,exp, NULL_RTX, mode, EXPAND_INITIALIZER);
        return op0;

     case POLY_INT_CST:
        return mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,poly_int_cst_value (exp), mode);

     case COMPLEX_CST:
        gcc_assert (mtcs_mode_is_complex_p/*!COMPLEX_MODE_P*/(mtcsMode,mode));
        op0 = expand_debug_expr(self,TREE_REALPART (exp));
        op1 = expand_debug_expr(self,TREE_IMAGPART (exp));
        return gen_rtx_CONCAT (mode, op0, op1);

     case DEBUG_EXPR_DECL:
        op0 = DECL_RTL_IF_SET (exp);
        if (op0){
           if (GET_MODE (op0) != mode)
              gcc_assert (VECTOR_TYPE_P (TREE_TYPE (exp)));
           else
              return op0;
        }
        op0 = gen_rtx_DEBUG_EXPR (mode);
        DEBUG_EXPR_TREE_DECL (op0) = exp;
        mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,exp, op0);
        return op0;

     case VAR_DECL:
     case PARM_DECL:
     case FUNCTION_DECL:
     case LABEL_DECL:
     case CONST_DECL:
     case RESULT_DECL:
        op0 = DECL_RTL_IF_SET (exp);
        /* This decl was probably optimized away.  */
        if (!op0
        /* At least label RTXen are sometimes replaced by
         NOTE_INSN_DELETED_LABEL.  Any notes here are not
         handled by copy_rtx.  */
        || NOTE_P (op0)){
           if (!VAR_P (exp)
              || DECL_EXTERNAL (exp)
              || !TREE_STATIC (exp)
              || !DECL_NAME (exp)
              || DECL_HARD_REGISTER (exp)
              || DECL_IN_CONSTANT_POOL (exp)
              || mode == VOIDmode
              || symtab_node::get (exp) == NULL)
              return NULL;

           op0 = make_decl_rtl_for_debug (exp);
           if (!MEM_P (op0)
              || GET_CODE (XEXP (op0, 0)) != SYMBOL_REF
              || SYMBOL_REF_DECL (XEXP (op0, 0)) != exp)
            return NULL;
        }else if (VAR_P (exp) && is_global_var (exp) && symtab_node::get (exp) == NULL)
            return NULL;
        else
            op0 = copy_rtx (op0);

        if (GET_MODE (op0) == mtcsMode->modes.M_BLKmode
      /* If op0 is not BLKmode, but mode is, adjust_mode
         below would ICE.  While it is likely a FE bug,
         try to be robust here.  See PR43166.  */
        || mode == mtcsMode->modes.M_BLKmode
        || (mode == VOIDmode && GET_MODE (op0) != VOIDmode)){
            gcc_assert (MEM_P (op0));
            op0 = mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/(mtcsRTL,op0, mode, 0);
            return op0;
        }

        /* Fall through.  */

     adjust_mode:
     case PAREN_EXPR:
     CASE_CONVERT:
      {
        inner_mode = GET_MODE (op0);
        if (mode == inner_mode)
          return op0;

        if (inner_mode == VOIDmode){
            if (TREE_CODE (exp) == SSA_NAME)
              inner_mode = TYPE_MODE (TREE_TYPE (exp));
            else
              inner_mode = TYPE_MODE (TREE_TYPE (TREE_OPERAND (exp, 0)));
            if (mode == inner_mode)
              return op0;
        }

        if (mtcs_mode_is_float_p/*!FLOAT_MODE_P*/(mtcsMode,mode)
                && mtcs_mode_is_float_p/*!FLOAT_MODE_P*/(mtcsMode,inner_mode)){
            if (mtcs_mode_get_unit_bitsize/*!GET_MODE_UNIT_BITSIZE*/(mtcsMode,mode)
              == mtcs_mode_get_unit_bitsize/*!GET_MODE_UNIT_BITSIZE*/(mtcsMode,inner_mode))
               op0 = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,mode, op0, inner_mode, 0);
            else if (mtcs_mode_get_unit_bitsize/*!GET_MODE_UNIT_BITSIZE*/(mtcsMode,mode)
                 < mtcs_mode_get_unit_bitsize/*!GET_MODE_UNIT_BITSIZE*/(mtcsMode,inner_mode))
               op0 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,FLOAT_TRUNCATE, mode, op0, inner_mode);
            else{
               n_debug("mtcsexpand.c expand_debug_expr FLOAT_EXTEND mode:%d inner_mode:%d\n",mode,inner_mode);

               op0 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,FLOAT_EXTEND, mode, op0, inner_mode);
            }
        }else if (mtcs_mode_is_float_p/*!FLOAT_MODE_P*/(mtcsMode,mode)){
            gcc_assert (TREE_CODE (exp) != SSA_NAME);
            if (TYPE_UNSIGNED (TREE_TYPE (TREE_OPERAND (exp, 0))))
              op0 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,UNSIGNED_FLOAT, mode, op0, inner_mode);
            else
              op0 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,FLOAT, mode, op0, inner_mode);
        }else if (mtcs_mode_is_float_p/*!FLOAT_MODE_P*/(mtcsMode,inner_mode)){
            if (unsignedp)
              op0 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,UNSIGNED_FIX, mode, op0, inner_mode);
            else
              op0 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,FIX, mode, op0, inner_mode);
        }else if (mtcs_mode_get_unit_precision/*!GET_MODE_UNIT_PRECISION*/(mtcsMode,mode)
             == mtcs_mode_get_unit_precision/*!GET_MODE_UNIT_PRECISION*/(mtcsMode,inner_mode))
           op0 = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, op0, inner_mode);
        else if (mtcs_mode_get_unit_precision/*!GET_MODE_UNIT_PRECISION*/(mtcsMode,mode)
             < mtcs_mode_get_unit_precision/*!GET_MODE_UNIT_PRECISION*/(mtcsMode,inner_mode))
           op0 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,TRUNCATE, mode, op0, inner_mode);
        else if (UNARY_CLASS_P (exp) ? TYPE_UNSIGNED (TREE_TYPE (TREE_OPERAND (exp, 0))): unsignedp)
           op0 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,ZERO_EXTEND, mode, op0, inner_mode);
        else
           op0 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,SIGN_EXTEND, mode, op0, inner_mode);

        return op0;
      }

     case MEM_REF:
        if (!is_gimple_mem_ref_addr (TREE_OPERAND (exp, 0))){
           tree newexp = mtcs_const_fold_binary/*!fold_binary*/(mtcsConst,MEM_REF, TREE_TYPE (exp),
                         TREE_OPERAND (exp, 0),
                         TREE_OPERAND (exp, 1));
           if (newexp)
              return expand_debug_expr(self,newexp);
        }
      /* FALLTHROUGH */
     case INDIRECT_REF:
        inner_mode = TYPE_MODE (TREE_TYPE (TREE_OPERAND (exp, 0)));
        op0 = expand_debug_expr(self,TREE_OPERAND (exp, 0));
        if (!op0)
            return NULL;

        if (TREE_CODE (exp) == MEM_REF){
           if (GET_CODE (op0) == DEBUG_IMPLICIT_PTR
              || (GET_CODE (op0) == PLUS
              && GET_CODE (XEXP (op0, 0)) == DEBUG_IMPLICIT_PTR))
            /* (mem (debug_implicit_ptr)) might confuse aliasing.
               Instead just use get_inner_reference.  */
              goto component_ref;

            op1 = expand_debug_expr(self,TREE_OPERAND (exp, 1));
            poly_int64 offset;
            if (!op1 || !poly_int_rtx_p (op1, &offset))
               return NULL;

            op0 = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,inner_mode, op0, offset);
        }

        as = TYPE_ADDR_SPACE (TREE_TYPE (TREE_TYPE (TREE_OPERAND (exp, 0))));

        op0 = convert_debug_memory_address(self,
              target_addr_space_address_mode/*!targetm.addr_space.address_mode*/(mtcsMachine->addrSpace,as), op0, as);
        if (op0 == NULL_RTX)
            return NULL;

        op0 = gen_rtx_MEM (mode, op0);
        mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,op0, exp, 0);
        if (TREE_CODE (exp) == MEM_REF  && !is_gimple_mem_ref_addr (TREE_OPERAND (exp, 0)))
            mtcs_rtl_set_mem_expr/*!set_mem_expr*/(mtcsRTL,op0, NULL_TREE);
        mtcs_rtl_set_mem_addr_space/*!set_mem_addr_space*/(mtcsRTL,op0, as);
        return op0;
     case TARGET_MEM_REF:
        if (TREE_CODE (TMR_BASE (exp)) == ADDR_EXPR  && !DECL_RTL_SET_P (TREE_OPERAND (TMR_BASE (exp), 0)))
           return NULL;
        op0 = expand_debug_expr(self,tree_mem_ref_addr (mtcs_tree_build_pointer_type/*!build_pointer_type*/(mtcsTree,TREE_TYPE (exp)), exp));
        if (!op0)
           return NULL;
        as = TYPE_ADDR_SPACE (TREE_TYPE (TREE_TYPE (TREE_OPERAND (exp, 0))));
        op0 = convert_debug_memory_address(self,
              target_addr_space_address_mode/*!targetm.addr_space.address_mode*/(mtcsMachine->addrSpace,as), op0, as);
        if (op0 == NULL_RTX)
           return NULL;
        op0 = gen_rtx_MEM (mode, op0);
        mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,op0, exp, 0);
        mtcs_rtl_set_mem_addr_space/*!set_mem_addr_space*/(mtcsRTL,op0, as);
        return op0;

    component_ref:
     case ARRAY_REF:
     case ARRAY_RANGE_REF:
     case COMPONENT_REF:
     case BIT_FIELD_REF:
     case REALPART_EXPR:
     case IMAGPART_EXPR:
     case VIEW_CONVERT_EXPR:
      {
            machine_mode mode1;
            poly_int64 bitsize, bitpos;
            tree offset;
            int reversep, volatilep = 0;
            tree tem = mtcs_expr_get_inner_reference/*!get_inner_reference*/(mtcsExpr,exp, &bitsize, &bitpos, &offset, &mode1,
                         &unsignedp, &reversep, &volatilep);
            rtx orig_op0;
            if (known_eq (bitsize, 0))
               return NULL;
            orig_op0 = op0 = expand_debug_expr(self,tem);
            if (!op0)
               return NULL;
            if (offset){
                machine_mode addrmode, offmode;
                if (!MEM_P (op0))
                   return NULL;

                op0 = XEXP (op0, 0);
                addrmode = GET_MODE (op0);
                if (addrmode == VOIDmode)
                   addrmode = Pmode;

                op1 = expand_debug_expr(self,offset);
                if (!op1)
                   return NULL;
                offmode = GET_MODE (op1);
                if (offmode == VOIDmode)
                   offmode = TYPE_MODE (TREE_TYPE (offset));
                if (addrmode != offmode)
                   op1 = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,addrmode, op1, offmode);
                /* Don't use offset_address here, we don't need a
                   recognizable address, and we don't want to generate
                   code.  */
                op0 = gen_rtx_MEM (mode, mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, addrmode,op0, op1));
            }

            if (MEM_P (op0)){
                if (mode1 == VOIDmode){
                    if (maybe_gt (bitsize, MAX_BITSIZE_MODE_ANY_INT))
                       return NULL;
                    /* Bitfield.  */
                    mode1 = smallest_int_mode_for_size (bitsize).require ();
                }
                poly_int64 bytepos = bits_to_bytes_round_down (bitpos);
                if (maybe_ne (bytepos, 0)){
                   op0 = mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/(mtcsRTL,op0, mode1, bytepos);
                   bitpos = num_trailing_bits (bitpos);
                }else if (known_eq (bitpos, 0)
                     && known_eq (bitsize, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode)))
                   op0 = mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/(mtcsRTL,op0, mode, 0);
                else if (GET_MODE (op0) != mode1)
                   op0 = mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/(mtcsRTL,op0, mode1, 0);
                else
                   op0 = copy_rtx (op0);
                if (op0 == orig_op0)
                   op0 = shallow_copy_rtx (op0);
                if (TREE_CODE (tem) != SSA_NAME)
                   mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,op0, exp, 0);
            }

            if (known_eq (bitpos, 0) && mode == GET_MODE (op0))
               return op0;

            if (maybe_lt (bitpos, 0))
               return NULL;

            if (GET_MODE (op0) == mtcsMode->modes.M_BLKmode || mode == mtcsMode->modes.M_BLKmode)
               return NULL;

            poly_int64 bytepos;
            if (multiple_p (bitpos, BITS_PER_UNIT, &bytepos)
                && known_eq (bitsize, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode1))){
                machine_mode opmode = GET_MODE (op0);

                if (opmode == VOIDmode)
                   opmode = TYPE_MODE (TREE_TYPE (tem));

                /* This condition may hold if we're expanding the address
                   right past the end of an array that turned out not to
                   be addressable (i.e., the address was only computed in
                   debug stmts).  The gen_subreg below would rightfully
                   crash, and the address doesn't really exist, so just
                   drop it.  */
                if (known_ge (bitpos, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,opmode)))
                   return NULL;

                if (multiple_p (bitpos, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode)))
                   return mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,mode, op0, opmode, bytepos);
            }

            return mtcs_simplify_rtx_gen_ternary/*!simplify_gen_ternary*/(mtcsSimplifyRtx,
                    mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,GET_MODE (op0))
                             && TYPE_UNSIGNED (TREE_TYPE (exp))
                             ? SIGN_EXTRACT
                             : ZERO_EXTRACT, mode,
                             GET_MODE (op0) != VOIDmode
                             ? GET_MODE (op0)
                             : TYPE_MODE (TREE_TYPE (tem)),
                             op0, mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,bitsize, word_mode),
                             mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,bitpos, word_mode));
      }

     case ABS_EXPR:
     case ABSU_EXPR:
        return mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,ABS, mode, op0, mode);

     case NEGATE_EXPR:
        return mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,NEG, mode, op0, mode);

     case BIT_NOT_EXPR:
        return mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,NOT, mode, op0, mode);

     case FLOAT_EXPR:
        return mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,
              TYPE_UNSIGNED (TREE_TYPE (TREE_OPERAND (exp,0)))
                 ? UNSIGNED_FLOAT : FLOAT, mode, op0,inner_mode);

     case FIX_TRUNC_EXPR:
        return mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,
                unsignedp ? UNSIGNED_FIX : FIX, mode, op0,inner_mode);

     case POINTER_PLUS_EXPR:
      /* For the rare target where pointers are not the same size as
     size_t, we need to check for mis-matched modes and correct
     the addend.  */
        if (op0 && op1
        && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (op0), &op0_mode)
        && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (op1), &op1_mode)
        && op0_mode != op1_mode){
           if (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,op0_mode) <
                  mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,op1_mode)
              /* If OP0 is a partial mode, then we must truncate, even
             if it has the same bitsize as OP1 as GCC's
             representation of partial modes is opaque.  */
              || (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,op0_mode) == MODE_PARTIAL_INT
              && (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,op0_mode)
                  == mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,op1_mode))))
               op1 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,TRUNCATE, op0_mode, op1, op1_mode);
           else
            /* We always sign-extend, regardless of the signedness of
               the operand, because the operand is always unsigned
               here even if the original C expression is signed.  */
               op1 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,SIGN_EXTEND, op0_mode, op1, op1_mode);
        }
      /* Fall through.  */
     case PLUS_EXPR:
        return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, mode, op0, op1);

     case MINUS_EXPR:
     case POINTER_DIFF_EXPR:
        return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MINUS, mode, op0, op1);

     case MULT_EXPR:
        return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MULT, mode, op0, op1);

     case RDIV_EXPR:
     case TRUNC_DIV_EXPR:
     case EXACT_DIV_EXPR:
        if (unsignedp)
            return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,UDIV, mode, op0, op1);
        else
            return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,DIV, mode, op0, op1);

     case TRUNC_MOD_EXPR:
        return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,unsignedp ? UMOD : MOD, mode, op0, op1);

     case FLOOR_DIV_EXPR:
        if (unsignedp)
            return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,UDIV, mode, op0, op1);
        else{
           rtx div = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,DIV, mode, op0, op1);
           rtx mod = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MOD, mode, op0, op1);
           rtx adj = floor_sdiv_adjust(self,mode, mod, op1);
           return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, mode, div, adj);
        }

     case FLOOR_MOD_EXPR:
        if (unsignedp)
           return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,UMOD, mode, op0, op1);
        else{
           rtx mod = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MOD, mode, op0, op1);
           rtx adj = floor_sdiv_adjust(self,mode, mod, op1);
           adj = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,NEG, mode,
                        mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MULT, mode, adj, op1),
                        mode);
           return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, mode, mod, adj);
        }

     case CEIL_DIV_EXPR:
        if (unsignedp){
           rtx div = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,UDIV, mode, op0, op1);
           rtx mod = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,UMOD, mode, op0, op1);
           rtx adj = ceil_udiv_adjust(self,mode, mod, op1);
           return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, mode, div, adj);
        }else{
           rtx div = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,DIV, mode, op0, op1);
           rtx mod = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MOD, mode, op0, op1);
           rtx adj = ceil_sdiv_adjust(self,mode, mod, op1);
           return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, mode, div, adj);
        }

     case CEIL_MOD_EXPR:
        if (unsignedp){
          rtx mod = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,UMOD, mode, op0, op1);
          rtx adj = ceil_udiv_adjust(self,mode, mod, op1);
          adj = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,NEG, mode,
                        mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MULT, mode, adj, op1),
                        mode);
          return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, mode, mod, adj);
        }else{
          rtx mod = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MOD, mode, op0, op1);
          rtx adj = ceil_sdiv_adjust(self,mode, mod, op1);
          adj = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,NEG, mode,
                        mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MULT, mode, adj, op1),
                        mode);
          return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, mode, mod, adj);
        }

     case ROUND_DIV_EXPR:
        if (unsignedp){
          rtx div = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,UDIV, mode, op0, op1);
          rtx mod = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,UMOD, mode, op0, op1);
          rtx adj = round_udiv_adjust(self,mode, mod, op1);
          return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, mode, div, adj);
        }else{
          rtx div = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,DIV, mode, op0, op1);
          rtx mod = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MOD, mode, op0, op1);
          rtx adj = round_sdiv_adjust(self,mode, mod, op1);
          return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, mode, div, adj);
        }

     case ROUND_MOD_EXPR:
        if (unsignedp){
          rtx mod = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,UMOD, mode, op0, op1);
          rtx adj = round_udiv_adjust(self,mode, mod, op1);
          adj = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,NEG, mode,
                        mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MULT, mode, adj, op1),
                        mode);
          return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, mode, mod, adj);
        }else{
          rtx mod = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MOD, mode, op0, op1);
          rtx adj = round_sdiv_adjust(self,mode, mod, op1);
          adj = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,NEG, mode,
                        mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MULT, mode, adj, op1),
                        mode);
          return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, mode, mod, adj);
        }

     case LSHIFT_EXPR:
        return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,ASHIFT, mode, op0, op1);

     case RSHIFT_EXPR:
        if (unsignedp)
           return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,LSHIFTRT, mode, op0, op1);
        else
           return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,ASHIFTRT, mode, op0, op1);

     case LROTATE_EXPR:
        return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,ROTATE, mode, op0, op1);

     case RROTATE_EXPR:
        return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,ROTATERT, mode, op0, op1);

     case MIN_EXPR:
        return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,unsignedp ? UMIN : SMIN, mode, op0, op1);

    case MAX_EXPR:
      return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,unsignedp ? UMAX : SMAX, mode, op0, op1);

     case BIT_AND_EXPR:
     case TRUTH_AND_EXPR:
        return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,AND, mode, op0, op1);

     case BIT_IOR_EXPR:
     case TRUTH_OR_EXPR:
        return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,IOR, mode, op0, op1);

     case BIT_XOR_EXPR:
     case TRUTH_XOR_EXPR:
        return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,XOR, mode, op0, op1);

     case TRUTH_ANDIF_EXPR:
        return gen_rtx_IF_THEN_ELSE (mode, op0, op1, const0_rtx);

     case TRUTH_ORIF_EXPR:
        return gen_rtx_IF_THEN_ELSE (mode, op0, const_true_rtx, op1);

     case TRUTH_NOT_EXPR:
        return mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,EQ, mode, inner_mode, op0, const0_rtx);

     case LT_EXPR:
        return mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,unsignedp ? LTU : LT, mode, inner_mode,
                      op0, op1);

     case LE_EXPR:
        return mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,unsignedp ? LEU : LE, mode, inner_mode,
                      op0, op1);

     case GT_EXPR:
        return mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,unsignedp ? GTU : GT, mode, inner_mode,op0, op1);

     case GE_EXPR:
        return mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,unsignedp ? GEU : GE, mode, inner_mode,
                      op0, op1);

     case EQ_EXPR:
        return mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,EQ, mode, inner_mode, op0, op1);

     case NE_EXPR:
        return mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,NE, mode, inner_mode, op0, op1);

    case UNORDERED_EXPR:
      return mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,UNORDERED, mode, inner_mode, op0, op1);

    case ORDERED_EXPR:
      return mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,ORDERED, mode, inner_mode, op0, op1);

    case UNLT_EXPR:
      return mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,UNLT, mode, inner_mode, op0, op1);

    case UNLE_EXPR:
      return mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,UNLE, mode, inner_mode, op0, op1);

    case UNGT_EXPR:
      return mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,UNGT, mode, inner_mode, op0, op1);

    case UNGE_EXPR:
      return mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,UNGE, mode, inner_mode, op0, op1);

    case UNEQ_EXPR:
      return mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,UNEQ, mode, inner_mode, op0, op1);

    case LTGT_EXPR:
      return mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,LTGT, mode, inner_mode, op0, op1);

    case COND_EXPR:
      return gen_rtx_IF_THEN_ELSE (mode, op0, op1, op2);

    case COMPLEX_EXPR:
      gcc_assert (COMPLEX_MODE_P (mode));
      if (GET_MODE (op0) == VOIDmode)
          op0 = gen_rtx_CONST (mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode), op0);
      if (GET_MODE (op1) == VOIDmode)
          op1 = gen_rtx_CONST (mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode), op1);
      return gen_rtx_CONCAT (mode, op0, op1);

     case CONJ_EXPR:
        if (GET_CODE (op0) == CONCAT)
           return gen_rtx_CONCAT (mode, XEXP (op0, 0),
                       mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,NEG,
                               mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode),
                               XEXP (op0, 1),
                               mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode)));
        else{
           scalar_mode imode = mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode);
           rtx re, im;
           if (MEM_P (op0)){
              re = mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/(mtcsRTL,op0, imode, 0);
              im = mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/(mtcsRTL,op0, imode,
                      mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,imode));
           }else{
              scalar_int_mode ifmode;
              scalar_int_mode ihmode;
              rtx halfsize;
              if (!mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,mode).exists (&ifmode)
                || !mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,imode).exists (&ihmode))
                  return NULL;
              halfsize = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,ihmode));
              re = op0;
              if (mode != ifmode)
                  re = mtcs_rtl_gen_rtx_SUBREG/*!gen_rtx_SUBREG*/(mtcsRTL,ifmode, re, 0);
              re = gen_rtx_ZERO_EXTRACT (ihmode, re, halfsize, const0_rtx);
              if (imode != ihmode)
                  re = mtcs_rtl_gen_rtx_SUBREG/*!gen_rtx_SUBREG*/(mtcsRTL,imode, re, 0);
              im = copy_rtx (op0);
              if (mode != ifmode)
                  im = mtcs_rtl_gen_rtx_SUBREG/*!gen_rtx_SUBREG*/(mtcsRTL,ifmode, im, 0);
              im = gen_rtx_ZERO_EXTRACT (ihmode, im, halfsize, halfsize);
              if (imode != ihmode)
                  im = mtcs_rtl_gen_rtx_SUBREG/*!gen_rtx_SUBREG*/(mtcsRTL,imode, im, 0);
           }
           im = gen_rtx_NEG (imode, im);
           return gen_rtx_CONCAT (mode, re, im);
        }

     case ADDR_EXPR:
        op0 = expand_debug_expr(self,TREE_OPERAND (exp, 0));
        if (!op0 || !MEM_P (op0)){
           if ((TREE_CODE (TREE_OPERAND (exp, 0)) == VAR_DECL
               || TREE_CODE (TREE_OPERAND (exp, 0)) == PARM_DECL
               || TREE_CODE (TREE_OPERAND (exp, 0)) == RESULT_DECL)
              && (!TREE_ADDRESSABLE (TREE_OPERAND (exp, 0))
              || target_for_debug_bind (TREE_OPERAND (exp, 0))))
              return gen_rtx_DEBUG_IMPLICIT_PTR (mode, TREE_OPERAND (exp, 0));

           if (handled_component_p (TREE_OPERAND (exp, 0))){
              poly_int64 bitoffset, bitsize, maxsize, byteoffset;
              bool reverse;
              tree decl = get_ref_base_and_extent (TREE_OPERAND (exp, 0), &bitoffset,
                           &bitsize, &maxsize, &reverse);
              if ((VAR_P (decl)  || TREE_CODE (decl) == PARM_DECL || TREE_CODE (decl) == RESULT_DECL)
                && (!TREE_ADDRESSABLE (decl)  || target_for_debug_bind (decl))
                && multiple_p (bitoffset, BITS_PER_UNIT, &byteoffset)
                && known_gt (bitsize, 0)  && known_eq (bitsize, maxsize)){
                  rtx base = gen_rtx_DEBUG_IMPLICIT_PTR (mode, decl);
                  return mtcs_rtl_plus_constant/*plus_constant*/ (mtcsRTL,mode, base, byteoffset);
              }
           }

           if (TREE_CODE (TREE_OPERAND (exp, 0)) == MEM_REF
              && TREE_CODE (TREE_OPERAND (TREE_OPERAND (exp, 0), 0)) == ADDR_EXPR){
              op0 = expand_debug_expr(self,TREE_OPERAND (TREE_OPERAND (exp, 0),0));
              if (op0 != NULL   && (GET_CODE (op0) == DEBUG_IMPLICIT_PTR
                  || (GET_CODE (op0) == PLUS && GET_CODE (XEXP (op0, 0)) == DEBUG_IMPLICIT_PTR
                  && CONST_INT_P (XEXP (op0, 1))))){
                  op1 = expand_debug_expr(self,TREE_OPERAND (TREE_OPERAND (exp, 0),1));
                  poly_int64 offset;
                  if (!op1 || !poly_int_rtx_p (op1, &offset))
                     return NULL;

                  return mtcs_rtl_plus_constant/*plus_constant*/ (mtcsRTL,mode, op0, offset);
              }
           }
           return NULL;
        }

        as = TYPE_ADDR_SPACE (TREE_TYPE (TREE_TYPE (exp)));
        addr_mode = mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,TREE_TYPE (exp));
        op0 = convert_debug_memory_address(self,addr_mode, XEXP (op0, 0), as);

        return op0;

     case VECTOR_CST:
      {
        unsigned HOST_WIDE_INT i, nelts;
        if (!VECTOR_CST_NELTS (exp).is_constant (&nelts))
          return NULL;
        op0 = gen_rtx_CONCATN (mode, rtvec_alloc (nelts));
        for (i = 0; i < nelts; ++i){
            op1 = expand_debug_expr(self,VECTOR_CST_ELT (exp, i));
            if (!op1)
              return NULL;
            XVECEXP (op0, 0, i) = op1;
        }
        return op0;
      }

     case CONSTRUCTOR:
        if (TREE_CLOBBER_P (exp))
           return NULL;
        else if (TREE_CODE (TREE_TYPE (exp)) == VECTOR_TYPE){
           unsigned i;
           unsigned HOST_WIDE_INT nelts;
           tree val;
           if (!TYPE_VECTOR_SUBPARTS (TREE_TYPE (exp)).is_constant (&nelts))
              goto flag_unsupported;

           op0 = gen_rtx_CONCATN (mode, rtvec_alloc (nelts));
           FOR_EACH_CONSTRUCTOR_VALUE (CONSTRUCTOR_ELTS (exp), i, val){
               op1 = expand_debug_expr(self,val);
               if (!op1)
                  return NULL;
               XVECEXP (op0, 0, i) = op1;
           }
           if (i < nelts){
              op1 = expand_debug_expr(self,build_zero_cst (TREE_TYPE (TREE_TYPE (exp))));
              if (!op1)
                  return NULL;
              for (; i < nelts; i++)
                  XVECEXP (op0, 0, i) = op1;
           }
           return op0;
        }else
           goto flag_unsupported;

     case CALL_EXPR:
        /* ??? Maybe handle some builtins?  */
        return NULL;
     case SSA_NAME:
      {
        gimple *g = get_gimple_for_ssa_name (exp);
        if (g){
            tree t = NULL_TREE;
            if (self->deep_ter_debug_map){
                tree *slot = self->deep_ter_debug_map->get (exp);
                if (slot)
                  t = *slot;
            }
            if (t == NULL_TREE)
              t = gimple_assign_rhs_to_tree (g);
            op0 = expand_debug_expr(self,t);
            if (!op0)
              return NULL;
        }else{
            /* If this is a reference to an incoming value of
               parameter that is never used in the code or where the
               incoming value is never used in the code, use
               PARM_DECL's DECL_RTL if set.  */
            if (SSA_NAME_IS_DEFAULT_DEF (exp)
            && SSA_NAME_VAR (exp)
            && TREE_CODE (SSA_NAME_VAR (exp)) == PARM_DECL
            && has_zero_uses (exp)){
                op0 = expand_debug_parm_decl(self,SSA_NAME_VAR (exp));
                if (op0)
                  goto adjust_mode;
                op0 = expand_debug_expr(self,SSA_NAME_VAR (exp));
                if (op0)
                  goto adjust_mode;
            }

            int part = var_to_partition (SA.map, exp);
            if (part == NO_PARTITION)
              return NULL;
            gcc_assert (part >= 0 && (unsigned)part < SA.map->num_partitions);
            op0 = copy_rtx (SA.partition_to_pseudo[part]);
          }
          goto adjust_mode;
        }

     case ERROR_MARK:
        return NULL;

     /* Vector stuff.  For most of the codes we don't have rtl codes.  */
     case REALIGN_LOAD_EXPR:
     case VEC_COND_EXPR:
     case VEC_PACK_FIX_TRUNC_EXPR:
     case VEC_PACK_FLOAT_EXPR:
     case VEC_PACK_SAT_EXPR:
     case VEC_PACK_TRUNC_EXPR:
     case VEC_UNPACK_FIX_TRUNC_HI_EXPR:
     case VEC_UNPACK_FIX_TRUNC_LO_EXPR:
     case VEC_UNPACK_FLOAT_HI_EXPR:
     case VEC_UNPACK_FLOAT_LO_EXPR:
     case VEC_UNPACK_HI_EXPR:
     case VEC_UNPACK_LO_EXPR:
     case VEC_WIDEN_MULT_HI_EXPR:
     case VEC_WIDEN_MULT_LO_EXPR:
     case VEC_WIDEN_MULT_EVEN_EXPR:
     case VEC_WIDEN_MULT_ODD_EXPR:
     case VEC_WIDEN_LSHIFT_HI_EXPR:
     case VEC_WIDEN_LSHIFT_LO_EXPR:
     case VEC_PERM_EXPR:
     case VEC_DUPLICATE_EXPR:
     case VEC_SERIES_EXPR:
     case SAD_EXPR:
        return NULL;

    /* Misc codes.  */
     case ADDR_SPACE_CONVERT_EXPR:
     case FIXED_CONVERT_EXPR:
     case OBJ_TYPE_REF:
     case WITH_SIZE_EXPR:
     case BIT_INSERT_EXPR:
        return NULL;

     case DOT_PROD_EXPR:
        if (mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,GET_MODE (op0))
              && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,mode)){
           op0 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,
                  TYPE_UNSIGNED (TREE_TYPE (TREE_OPERAND (exp,0)))
                      ? ZERO_EXTEND : SIGN_EXTEND, mode, op0,inner_mode);
           op1 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,
                  TYPE_UNSIGNED (TREE_TYPE (TREE_OPERAND (exp,1))) ? ZERO_EXTEND : SIGN_EXTEND, mode, op1,inner_mode);
           op0 = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MULT, mode, op0, op1);
           return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, mode, op0, op2);
        }
        return NULL;

     case WIDEN_MULT_EXPR:
     case WIDEN_MULT_PLUS_EXPR:
     case WIDEN_MULT_MINUS_EXPR:
        if (mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,GET_MODE (op0))
           && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,mode)){
           inner_mode = GET_MODE (op0);
           if (TYPE_UNSIGNED (TREE_TYPE (TREE_OPERAND (exp, 0))))
              op0 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,ZERO_EXTEND, mode, op0, inner_mode);
           else
              op0 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,SIGN_EXTEND, mode, op0, inner_mode);
           if (TYPE_UNSIGNED (TREE_TYPE (TREE_OPERAND (exp, 1))))
              op1 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,ZERO_EXTEND, mode, op1, inner_mode);
           else
              op1 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,SIGN_EXTEND, mode, op1, inner_mode);
           op0 = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MULT, mode, op0, op1);
           if (TREE_CODE (exp) == WIDEN_MULT_EXPR)
             return op0;
           else if (TREE_CODE (exp) == WIDEN_MULT_PLUS_EXPR)
              return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, mode, op0, op2);
           else
              return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MINUS, mode, op2, op0);
        }
        return NULL;

     case MULT_HIGHPART_EXPR:
      /* ??? Similar to the above.  */
      return NULL;

     case WIDEN_SUM_EXPR:
     case WIDEN_LSHIFT_EXPR:
        if (mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,GET_MODE (op0))
          && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,mode)){
           op0 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,
                TYPE_UNSIGNED (TREE_TYPE (TREE_OPERAND (exp,0)))? ZERO_EXTEND : SIGN_EXTEND, mode, op0,inner_mode);
           return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,TREE_CODE (exp) == WIDEN_LSHIFT_EXPR
                      ? ASHIFT : PLUS, mode, op0, op1);
        }
        return NULL;

     default:
    flag_unsupported:
      if (mtcsOptionsItem->x_flag_checking){
          debug_tree (exp);
          gcc_unreachable ();
      }
      return NULL;
  }
}


/* Mark all calls that can have a transaction restart.  */
//原型 mark_transaction_restart_calls cfgexpand.cc
static void mark_transaction_restart_calls (MtcsExpand *self,gimple *stmt)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsStmt *mtcsStmt=mtcs_target_get_stmt(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  struct tm_restart_node dummy;
  tm_restart_node **slot;
  if (!cfun->gimple_df->tm_restart)
    return;
  dummy.stmt = stmt;
  slot = cfun->gimple_df->tm_restart->find_slot (&dummy, NO_INSERT);
  if (slot){
      struct tm_restart_node *n = *slot;
      tree list = n->label_or_list;
      rtx_insn *insn;

      for (insn = next_real_insn (mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData));
       !CALL_P (insn); insn = next_real_insn (insn))
          continue;

      if (TREE_CODE (list) == LABEL_DECL)
          add_reg_note (insn, REG_TM, mtcs_stmt_label_rtx/*!label_rtx*/(mtcsStmt,list));
      else
          for (; list ; list = TREE_CHAIN (list))
              add_reg_note (insn, REG_TM, mtcs_stmt_label_rtx/*!label_rtx*/(mtcsStmt,TREE_VALUE (list)));
  }
}

/* Return the number of times character C occurs in string S.  */
//原型 n_occurrences cfgexpand.cc
static int n_occurrences (int c, const char *s)
{
  int n = 0;
  while (*s)
    n += (*s++ == c);
  return n;
}

/* A subroutine of expand_asm_operands.  Check that all operands have
   the same number of alternatives.  Return true if so.  */
//原型 check_operand_nalternatives cfgexpand.cc
static bool check_operand_nalternatives (const vec<const char *> &constraints)
{
  unsigned len = constraints.length();
  if (len > 0){
      int nalternatives = n_occurrences (',', constraints[0]);
      if (nalternatives + 1 > MAX_RECOG_ALTERNATIVES){
          error ("too many alternatives in %<asm%>");
          return false;
      }

      for (unsigned i = 1; i < len; ++i)
        if (n_occurrences (',', constraints[i]) != nalternatives){
            error ("operand constraints for %<asm%> differ in number of alternatives");
            return false;
        }
  }
  return true;
}


/* Check for overlap between registers marked in CLOBBERED_REGS and
   anything inappropriate in T.  Emit error and return the register
   variable definition for error, NULL_TREE for ok.  */
//原型 tree_conflicts_with_clobbers_p cfgexpand.cc
static bool tree_conflicts_with_clobbers_p (MtcsExpand *self,tree t, HardRegSet *clobbered_regs,
                location_t loc)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsStmt *mtcsStmt=mtcs_target_get_stmt(mtcsTarget);
  /* Conflicts between asm-declared register variables and the clobber
     list are not allowed.  */
  tree overlap = mtcs_stmt_tree_overlaps_hard_reg_set/*!tree_overlaps_hard_reg_set*/(mtcsStmt,t, clobbered_regs);
  if (overlap){
      error_at (loc, "%<asm%> specifier for variable %qE conflicts with "
        "%<asm%> clobber list", DECL_NAME (overlap));
      /* Reset registerness to stop multiple errors emitted for a single
     variable.  */
      DECL_REGISTER (overlap) = 0;
      return true;
  }
  return false;
}

/* Like add_to_hard_reg_set, but use a REGNO/NREGS range instead of
   REGNO and MODE.  */
//原型 inline void add_range_to_hard_reg_set rtl.h
static void addRangeToHardRegSet/*!add_range_to_hard_reg_set*/(MtcsExpand *self,HardRegSet *regs, unsigned int regno,int nregs)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  while (nregs-- > 0)
    mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,regs, regno + nregs);
}

/* Generate RTL for an asm statement (explicit assembler code).
   STRING is a STRING_CST node containing the assembler code text,
   or an ADDR_EXPR containing a STRING_CST.  VOL nonzero means the
   insn is volatile; don't optimize it.  */
//原型 expand_asm_loc cfgexpand.cc
static void expand_asm_loc (MtcsExpand *self,tree string, int vol, location_t locus)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  rtx body;
  body = gen_rtx_ASM_INPUT_loc (VOIDmode, ggc_strdup (TREE_STRING_POINTER (string)),locus);
  MEM_VOLATILE_P (body) = vol;
  /* Non-empty basic ASM implicitly clobbers memory.  */
  if (TREE_STRING_LENGTH (string) != 0){
      rtx asm_op, clob;
      unsigned i, nclobbers;
      auto_vec<rtx> input_rvec, output_rvec;
      auto_vec<machine_mode> input_mode;
      auto_vec<const char *> constraints;
      auto_vec<rtx> use_rvec;
      auto_vec<rtx> clobber_rvec;
      HardRegSet/*!HARD_REG_SET*/ clobbered_regs={mtcs_reg_get_hard_reg_element_count(mtcsReg)};/*!HARD_REG_SET clobbered_regs;*/
      mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&clobbered_regs);
      clob = gen_rtx_MEM (mtcsMode->modes.M_BLKmode, gen_rtx_SCRATCH (VOIDmode));
      clobber_rvec.safe_push (clob);
      if (mtcsTarget/*!targetm.md_asm_adjust*/->md_asm_adjust)
          mtcsTarget/*!targetm.md_asm_adjust*/->md_asm_adjust(mtcsTarget,output_rvec, input_rvec, input_mode,
                   constraints, use_rvec, clobber_rvec,  clobbered_regs, locus);

      asm_op = body;
      nclobbers = clobber_rvec.length ();
      auto nuses = use_rvec.length ();
      body = gen_rtx_PARALLEL (VOIDmode, rtvec_alloc (1 + nuses + nclobbers));

      i = 0;
      XVECEXP (body, 0, i++) = asm_op;
      for (rtx use : use_rvec)
          XVECEXP (body, 0, i++) = gen_rtx_USE (VOIDmode, use);
      for (rtx clobber : clobber_rvec)
          XVECEXP (body, 0, i++) = gen_rtx_CLOBBER (VOIDmode, clobber);
  }

  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,body);
}


/* Check that the given REGNO spanning NREGS is a valid
   asm clobber operand.  Some HW registers cannot be
   saved/restored, hence they should not be clobbered by
   asm statements.  */
//原型 asm_clobber_reg_is_valid
static bool asm_clobber_reg_is_valid (MtcsExpand *self,int regno, int nregs, const char *regname)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  bool is_valid = true;
  HardRegSet/*!HARD_REG_SET*/ regset={mtcs_reg_get_hard_reg_element_count(mtcsReg)};/*!HARD_REG_SET regset;*/
  mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&regset);
  addRangeToHardRegSet/*!add_range_to_hard_reg_set*/(self,&regset, regno, nregs);
  /* Clobbering the PIC register is an error.  */
  if (mtcs_reg_get_pic_offset_table_regnum/*!PIC_OFFSET_TABLE_REGNUM*/(mtcsReg) != INVALID_REGNUM
      && mtcs_reg_overlaps_hard_reg_set_p/*!overlaps_hard_reg_set_p*/(mtcsReg,
              &regset, mtcs_mode_get_Pmode(mtcsMode),
              mtcs_reg_get_pic_offset_table_regnum/*!PIC_OFFSET_TABLE_REGNUM*/(mtcsReg))){
      /* ??? Diagnose during gimplification?  */
      error ("PIC register clobbered by %qs in %<asm%>", regname);
      is_valid = false;
  }else if (!mtcs_reg_in_hard_reg_set_p/*!in_hard_reg_set_p*/(mtcsReg,
         &mtcsReg->hardRegs.x_accessible_reg_set/*!accessible_reg_set*/,
         mtcsReg->hardRegs.x_reg_raw_mode/*!reg_raw_mode*/[regno], regno)){
      /* ??? Diagnose during gimplification?  */
      error ("the register %qs cannot be clobbered in %<asm%>"
         " for the current target", regname);
      is_valid = false;
  }

  /* Clobbering the stack pointer register is deprecated.  GCC expects
     the value of the stack pointer after an asm statement to be the same
     as it was before, so no asm can validly clobber the stack pointer in
     the usual sense.  Adding the stack pointer to the clobber list has
     traditionally had some undocumented and somewhat obscure side-effects.  */
  if (mtcs_reg_overlaps_hard_reg_set_p/*!overlaps_hard_reg_set_p*/(mtcsReg,
          &regset, mtcs_mode_get_Pmode(mtcsMode),
          mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg))){
      mtcsRtlData/*!crtl*/->sp_is_clobbered_by_asm = true;
      if (warning (OPT_Wdeprecated, "listing the stack pointer register"
           " %qs in a clobber list is deprecated", regname))
        inform (input_location, "the value of the stack pointer after"
        " an %<asm%> statement must be the same as it was before"
        " the statement");
  }

  return is_valid;
}


/* Generate RTL for an asm statement with arguments.
   STRING is the instruction template.
   OUTPUTS is a list of output arguments (lvalues); INPUTS a list of inputs.
   Each output or input has an expression in the TREE_VALUE and
   a tree list in TREE_PURPOSE which in turn contains a constraint
   name in TREE_VALUE (or NULL_TREE) and a constraint string
   in TREE_PURPOSE.
   CLOBBERS is a list of STRING_CST nodes each naming a hard register
   that is clobbered by this insn.

   LABELS is a list of labels, and if LABELS is non-NULL, FALLTHRU_BB
   should be the fallthru basic block of the asm goto.

   Not all kinds of lvalue that may appear in OUTPUTS can be stored directly.
   Some elements of OUTPUTS may be replaced with trees representing temporary
   values.  The caller should copy those temporary values to the originally
   specified lvalues.

   VOL nonzero means the insn is volatile; don't optimize it.  */
//原型 expand_asm_stmt cfgexpand.cc
static void expand_asm_stmt (MtcsExpand *self,gasm *stmt)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
  MtcsStmt *mtcsStmt=mtcs_target_get_stmt(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  class save_input_location
  {
    location_t old;
  public:
    explicit save_input_location(location_t where)
    {
      old = input_location;
      input_location = where;
    }
    ~save_input_location()
    {
      input_location = old;
    }
  };

  location_t locus = gimple_location (stmt);
  if (gimple_asm_basic_p (stmt)){
      const char *s = gimple_asm_string (stmt);
      tree string = build_string (strlen (s), s);
      expand_asm_loc(self,string, gimple_asm_volatile_p (stmt), locus);
      return;
  }

  /* There are some legacy diagnostics in here.  */
  save_input_location s_i_l(locus);
  unsigned noutputs = gimple_asm_noutputs (stmt);
  unsigned ninputs = gimple_asm_ninputs (stmt);
  unsigned nlabels = gimple_asm_nlabels (stmt);
  unsigned i;
  bool error_seen = false;

  /* ??? Diagnose during gimplification?  */
  if (ninputs + noutputs + nlabels > MAX_RECOG_OPERANDS){
      error_at (locus, "more than %d operands in %<asm%>", MAX_RECOG_OPERANDS);
      return;
  }
  auto_vec<tree, MAX_RECOG_OPERANDS> output_tvec;
  auto_vec<tree, MAX_RECOG_OPERANDS> input_tvec;
  auto_vec<const char *, MAX_RECOG_OPERANDS> constraints;
  /* Copy the gimple vectors into new vectors that we can manipulate.  */
  output_tvec.safe_grow (noutputs, true);
  input_tvec.safe_grow (ninputs, true);
  constraints.safe_grow (noutputs + ninputs, true);

  for (i = 0; i < noutputs; ++i){
      tree t = gimple_asm_output_op (stmt, i);
      output_tvec[i] = TREE_VALUE (t);
      constraints[i] = TREE_STRING_POINTER (TREE_VALUE (TREE_PURPOSE (t)));
  }
  for (i = 0; i < ninputs; i++){
      tree t = gimple_asm_input_op (stmt, i);
      input_tvec[i] = TREE_VALUE (t);
      constraints[i + noutputs] = TREE_STRING_POINTER (TREE_VALUE (TREE_PURPOSE (t)));
  }
  /* ??? Diagnose during gimplification?  */
  if (! check_operand_nalternatives (constraints))
    return;
  /* Count the number of meaningful clobbered registers, ignoring what
     we would ignore later.  */
  auto_vec<rtx> clobber_rvec;
  HardRegSet/*!HARD_REG_SET*/ clobbered_regs={mtcs_reg_get_hard_reg_element_count(mtcsReg)};/*!HARD_REG_SET hardregs;*/
  mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&clobbered_regs);

  if (unsigned n = gimple_asm_nclobbers (stmt)){
      clobber_rvec.reserve (n);
      for (i = 0; i < n; i++){
          tree t = gimple_asm_clobber_op (stmt, i);
          const char *regname = TREE_STRING_POINTER (TREE_VALUE (t));
          int nregs, j;
          j = mtcs_output_decode_reg_name_and_count/*!decode_reg_name_and_count*/(mtcsOutput,regname, &nregs);
          if (j < 0){
              if (j == -2){
                  /* ??? Diagnose during gimplification?  */
                  error_at (locus, "unknown register name %qs in %<asm%>",regname);
                  error_seen = true;
              }else if (j == -4){
                  rtx x = gen_rtx_MEM (mtcsMode->modes.M_BLKmode, gen_rtx_SCRATCH (VOIDmode));
                  clobber_rvec.safe_push (x);
              }else{
                  /* Otherwise we should have -1 == empty string
                     or -3 == cc, which is not a register.  */
                  gcc_assert (j == -1 || j == -3);
              }
          }else
            for (int reg = j; reg < j + nregs; reg++){
                if (!asm_clobber_reg_is_valid(self,reg, nregs, regname))
                  return;
                mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&clobbered_regs, reg);
                rtx x = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mtcsReg->hardRegs.x_reg_raw_mode/*!reg_raw_mode*/[reg], reg);
                clobber_rvec.safe_push (x);
            }
      }
  }
  /* First pass over inputs and outputs checks validity and sets
     mark_addressable if needed.  */
  /* ??? Diagnose during gimplification?  */
  for (i = 0; i < noutputs; ++i){
      tree val = output_tvec[i];
      tree type = TREE_TYPE (val);
      const char *constraint;
      bool is_inout;
      bool allows_reg;
      bool allows_mem;
      /* Try to parse the output constraint.  If that fails, there's
     no point in going further.  */
      constraint = constraints[i];
      if (!mtcs_stmt_parse_output_constraint/*!parse_output_constraint*/(mtcsStmt,&constraint, i, ninputs, noutputs,
                    &allows_mem, &allows_reg, &is_inout))
          return;
      /* If the output is a hard register, verify it doesn't conflict with
     any other operand's possible hard register use.  */
      if (DECL_P (val)  && REG_P (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,val))
            && mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,val))){
          unsigned j, output_hregno = REGNO (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,val));
          bool early_clobber_p = strchr (constraints[i], '&') != NULL;
          unsigned long match;
          /* Verify the other outputs do not use the same hard register.  */
          for (j = i + 1; j < noutputs; ++j)
             if (DECL_P (output_tvec[j])
               && REG_P (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,output_tvec[j]))
               && mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,output_tvec[j]))
               && output_hregno == REGNO (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,output_tvec[j]))){
                error_at (locus, "invalid hard register usage between output operands");
                error_seen = true;
             }

          /* Verify matching constraint operands use the same hard register
             and that the non-matching constraint operands do not use the same
             hard register if the output is an early clobber operand.  */
          for (j = 0; j < ninputs; ++j)
              if (DECL_P (input_tvec[j])
                && REG_P (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,input_tvec[j]))
                && mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,input_tvec[j]))){
                  unsigned input_hregno = REGNO (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,input_tvec[j]));
                  switch (*constraints[j + noutputs]){
                      case '0':  case '1':  case '2':  case '3':  case '4':
                      case '5':  case '6':  case '7':  case '8':  case '9':
                        match = strtoul (constraints[j + noutputs], NULL, 10);
                        break;
                      default:
                        match = ULONG_MAX;
                        break;
                  }
                  if (i == match  && output_hregno != input_hregno){
                      error_at (locus, "invalid hard register usage between "
                          "output operand and matching constraint operand");
                      error_seen = true;
                  }else if (early_clobber_p  && i != match   && output_hregno == input_hregno){
                      error_at (locus, "invalid hard register usage between "
                          "earlyclobber operand and input operand");
                      error_seen = true;
                  }
              }
      }

      if (! allows_reg && (allows_mem || is_inout  || (DECL_P (val)
          && REG_P (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,val))
          && GET_MODE (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,val)) != TYPE_MODE (type))))
          mark_addressable (val);
  }

  for (i = 0; i < ninputs; ++i){
      bool allows_reg, allows_mem;
      const char *constraint;
      constraint = constraints[i + noutputs];
      if (! mtcs_stmt_parse_input_constraint/*!parse_input_constraint*/(mtcsStmt,&constraint, i, ninputs, noutputs, 0,
                    constraints.address (),&allows_mem, &allows_reg))
          return;

      if (! allows_reg && allows_mem)
          mark_addressable (input_tvec[i]);
  }
  /* Second pass evaluates arguments.  */

  /* Make sure stack is consistent for asm goto.  */
  if (nlabels > 0)
     mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);

  int old_generating_concat_p = generating_concat_p;

  /* Vector of RTX's of evaluated output operands.  */
  auto_vec<rtx, MAX_RECOG_OPERANDS> output_rvec;
  auto_vec<int, MAX_RECOG_OPERANDS> inout_opnum;
  rtx_insn *after_rtl_seq = NULL, *after_rtl_end = NULL;

  output_rvec.safe_grow (noutputs, true);

  for (i = 0; i < noutputs; ++i){
      tree val = output_tvec[i];
      tree type = TREE_TYPE (val);
      bool is_inout, allows_reg, allows_mem, ok;
      rtx op;
      ok = mtcs_stmt_parse_output_constraint/*!parse_output_constraint*/(mtcsStmt,&constraints[i], i, ninputs,
                    noutputs, &allows_mem, &allows_reg,&is_inout);
      gcc_assert (ok);
      /* If an output operand is not a decl or indirect ref and our constraint
     allows a register, make a temporary to act as an intermediate.
     Make the asm insn write into that, then we will copy it to
     the real output operand.  Likewise for promoted variables.  */
      generating_concat_p = 0;
      gcc_assert (TREE_CODE (val) != INDIRECT_REF);
      if (((TREE_CODE (val) == MEM_REF  && TREE_CODE (TREE_OPERAND (val, 0)) != ADDR_EXPR) && allows_mem)
        || (DECL_P (val) && (allows_mem || REG_P (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,val)))
        && ! (REG_P (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,val))
              && GET_MODE (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,val)) != TYPE_MODE (type)))
        || ! allows_reg  || is_inout || TREE_ADDRESSABLE (type)  || (!tree_fits_poly_int64_p (TYPE_SIZE (type))
          && !known_size_p (max_int_size_in_bytes (type))))
      {
          op = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,val, NULL_RTX, VOIDmode, !allows_reg ? EXPAND_MEMORY : EXPAND_WRITE);
          if (MEM_P (op))
            op = mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,op);

          if (! allows_reg && !MEM_P (op)){
              error_at (locus, "output number %d not directly addressable", i);
              error_seen = true;
          }
          if ((! allows_mem && MEM_P (op) && GET_MODE (op) != mtcsMode->modes.M_BLKmode) || GET_CODE (op) == CONCAT){
              rtx old_op = op;
              op = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (op));
              generating_concat_p = old_generating_concat_p;
              if (is_inout)
                  mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,op, old_op);

              mtcs_emit_push_to_sequence2/*!push_to_sequence2*/(mtcsEmit,after_rtl_seq, after_rtl_end);
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,old_op, op);
              after_rtl_seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
              after_rtl_end = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
              mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
          }
      }else{
          op = mtcs_func_assign_temp/*!assign_temp*/(mtcsFunc,type, 0, 1);
          op = mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,op);
          if (!MEM_P (op) && TREE_CODE (val) == SSA_NAME)
             mtcs_rtl_set_reg_attrs_for_decl_rtl/*!set_reg_attrs_for_decl_rtl*/(mtcsRTL,SSA_NAME_VAR (val), op);

          generating_concat_p = old_generating_concat_p;

          mtcs_emit_push_to_sequence2/*!push_to_sequence2*/(mtcsEmit,after_rtl_seq, after_rtl_end);
          mtcs_expr_expand_assignment/*!expand_assignment*/(mtcsExpr,val,
                mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,type, op), false);
          after_rtl_seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
          after_rtl_end = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
          mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      }
      output_rvec[i] = op;
      if (is_inout)
          inout_opnum.safe_push (i);
  }

  const char *str = gimple_asm_string (stmt);
  if (error_seen){
      ninputs = 0;
      noutputs = 0;
      inout_opnum.truncate (0);
      output_rvec.truncate (0);
      clobber_rvec.truncate (0);
      constraints.truncate (0);
      mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&clobbered_regs);
      str = "";
  }

  auto_vec<rtx, MAX_RECOG_OPERANDS> input_rvec;
  auto_vec<machine_mode, MAX_RECOG_OPERANDS> input_mode;

  input_rvec.safe_grow (ninputs, true);
  input_mode.safe_grow (ninputs, true);

  generating_concat_p = 0;

  for (i = 0; i < ninputs; ++i){
      tree val = input_tvec[i];
      tree type = TREE_TYPE (val);
      bool allows_reg, allows_mem, ok;
      const char *constraint;
      rtx op;
      constraint = constraints[i + noutputs];
      ok = mtcs_stmt_parse_input_constraint/*!parse_input_constraint*/(mtcsStmt,&constraint, i, ninputs, noutputs, 0,
                   constraints.address (), &allows_mem, &allows_reg);
      gcc_assert (ok);

      /* EXPAND_INITIALIZER will not generate code for valid initializer
     constants, but will still generate code for other types of operand.
     This is the behavior we want for constant constraints.  */
      op = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,val, NULL_RTX, VOIDmode,
            allows_reg ? EXPAND_NORMAL : allows_mem ? EXPAND_MEMORY : EXPAND_INITIALIZER);

      /* Never pass a CONCAT to an ASM.  */
      if (GET_CODE (op) == CONCAT)
          op = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,GET_MODE (op), op);
      else if (MEM_P (op))
          op = mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,op);

      if (mtcs_recog_asm_operand_ok/*!asm_operand_ok*/(mtcsRecog,op, constraint, NULL) <= 0){
          if (allows_reg && TYPE_MODE (type) != mtcsMode->modes.M_BLKmode)
              op = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,TYPE_MODE (type), op);
          else if (!allows_mem)
              warning_at (locus, 0, "%<asm%> operand %d probably does not match "
                "constraints", i + noutputs);
          else if (MEM_P (op)){
              /* We won't recognize either volatile memory or memory
             with a queued address as available a memory_operand
             at this point.  Ignore it: clearly this *is* a memory.  */
          }else
            gcc_unreachable ();
      }
      input_rvec[i] = op;
      input_mode[i] = TYPE_MODE (type);
  }

  /* For in-out operands, copy output rtx to input rtx.  */
  unsigned ninout = inout_opnum.length ();
  for (i = 0; i < ninout; i++){
      int j = inout_opnum[i];
      rtx o = output_rvec[j];
      input_rvec.safe_push (o);
      input_mode.safe_push (GET_MODE (o));
      char buffer[16];
      sprintf (buffer, "%d", j);
      constraints.safe_push (ggc_strdup (buffer));
  }
  ninputs += ninout;
  /* Sometimes we wish to automatically clobber registers across an asm.
     Case in point is when the i386 backend moved from cc0 to a hard reg --
     maintaining source-level compatibility means automatically clobbering
     the flags register.  */
  rtx_insn *after_md_seq = NULL;
  auto_vec<rtx> use_rvec;
  if (mtcsTarget/*!targetm.md_asm_adjust*/->md_asm_adjust)
    after_md_seq = mtcsTarget/*!targetm.md_asm_adjust*/->md_asm_adjust(mtcsTarget,output_rvec, input_rvec, input_mode,
                 constraints, use_rvec, clobber_rvec,
                 clobbered_regs, locus);

  /* Do not allow the hook to change the output and input count,
     lest it mess up the operand numbering.  */
  gcc_assert (output_rvec.length() == noutputs);
  gcc_assert (input_rvec.length() == ninputs);
  gcc_assert (constraints.length() == noutputs + ninputs);
  /* But it certainly can adjust the uses and clobbers.  */
  unsigned nuses = use_rvec.length ();
  unsigned nclobbers = clobber_rvec.length ();
  /* Third pass checks for easy conflicts.  */
  /* ??? Why are we doing this on trees instead of rtx.  */
  bool clobber_conflict_found = 0;
  for (i = 0; i < noutputs; ++i)
    if (tree_conflicts_with_clobbers_p(self,output_tvec[i], &clobbered_regs, locus))
        clobber_conflict_found = 1;
  for (i = 0; i < ninputs - ninout; ++i)
    if (tree_conflicts_with_clobbers_p(self,input_tvec[i], &clobbered_regs, locus))
        clobber_conflict_found = 1;
  /* Make vectors for the expression-rtx, constraint strings,
     and named operands.  */
  rtvec argvec = rtvec_alloc (ninputs);
  rtvec constraintvec = rtvec_alloc (ninputs);
  rtvec labelvec = rtvec_alloc (nlabels);
  rtx body = gen_rtx_ASM_OPERANDS ((noutputs == 0 ? VOIDmode
                    : GET_MODE (output_rvec[0])),
                   ggc_strdup (str),
                   "", 0, argvec, constraintvec,
                   labelvec, locus);
  MEM_VOLATILE_P (body) = gimple_asm_volatile_p (stmt);
  for (i = 0; i < ninputs; ++i){
      ASM_OPERANDS_INPUT (body, i) = input_rvec[i];
      ASM_OPERANDS_INPUT_CONSTRAINT_EXP (body, i) = gen_rtx_ASM_INPUT_loc (input_mode[i],
                 constraints[i + noutputs],locus);
  }

  /* Copy labels to the vector.  */
  rtx_code_label *fallthru_label = NULL;
  if (nlabels > 0){
      basic_block fallthru_bb = NULL;
      edge fallthru = find_fallthru_edge (gimple_bb (stmt)->succs);
      if (fallthru)
          fallthru_bb = fallthru->dest;

      for (i = 0; i < nlabels; ++i){
          tree label = TREE_VALUE (gimple_asm_label_op (stmt, i));
          rtx_insn *r;
          /* If asm goto has any labels in the fallthru basic block, use
             a label that we emit immediately after the asm goto.  Expansion
             may insert further instructions into the same basic block after
             asm goto and if we don't do this, insertion of instructions on
             the fallthru edge might misbehave.  See PR58670.  */
          if (fallthru_bb && label_to_block (cfun, label) == fallthru_bb){
              if (fallthru_label == NULL_RTX)
                fallthru_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
              r = fallthru_label;
          }else
            r = mtcs_stmt_label_rtx/*!label_rtx*/(mtcsStmt,label);
          ASM_OPERANDS_LABEL (body, i) = gen_rtx_LABEL_REF (Pmode, r);
      }
  }
  /* Now, for each output, construct an rtx
     (set OUTPUT (asm_operands INSN OUTPUTCONSTRAINT OUTPUTNUMBER
                   ARGVEC CONSTRAINTS OPNAMES))
     If there is more than one, put them inside a PARALLEL.  */
  if (noutputs == 0 && nuses == 0 && nclobbers == 0){
      /* No output operands: put in a raw ASM_OPERANDS rtx.  */
      if (nlabels > 0)
          mtcs_emit_emit_jump_insn/*!emit_jump_insn*/(mtcsEmit,body);
      else
          mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,body);
  }else if (noutputs == 1 && nuses == 0 && nclobbers == 0){
      ASM_OPERANDS_OUTPUT_CONSTRAINT (body) = constraints[0];
      if (nlabels > 0)
          mtcs_emit_emit_jump_insn/*!emit_jump_insn*/(mtcsEmit,gen_rtx_SET (output_rvec[0], body));
      else
          mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_rtx_SET (output_rvec[0], body));
  }else{
      rtx obody = body;
      int num = noutputs;
      if (num == 0)
          num = 1;
      body = gen_rtx_PARALLEL (VOIDmode,rtvec_alloc (num + nuses + nclobbers));
      /* For each output operand, store a SET.  */
      for (i = 0; i < noutputs; ++i){
          rtx src, o = output_rvec[i];
          if (i == 0){
              ASM_OPERANDS_OUTPUT_CONSTRAINT (obody) = constraints[0];
              src = obody;
          }else{
              src = gen_rtx_ASM_OPERANDS (GET_MODE (o),
                          ASM_OPERANDS_TEMPLATE (obody),
                          constraints[i], i, argvec,
                          constraintvec, labelvec, locus);
              MEM_VOLATILE_P (src) = gimple_asm_volatile_p (stmt);
          }
          XVECEXP (body, 0, i) = gen_rtx_SET (o, src);
      }
      /* If there are no outputs (but there are some clobbers)
     store the bare ASM_OPERANDS into the PARALLEL.  */
      if (i == 0)
          XVECEXP (body, 0, i++) = obody;
      /* Add the uses specified by the target hook.  No checking should
     be needed since this doesn't come directly from user code.  */
      for (rtx use : use_rvec)
          XVECEXP (body, 0, i++) = gen_rtx_USE (VOIDmode, use);
      /* Store (clobber REG) for each clobbered register specified.  */
      for (unsigned j = 0; j < nclobbers; ++j){
          rtx clobbered_reg = clobber_rvec[j];
          /* Do sanity check for overlap between clobbers and respectively
             input and outputs that hasn't been handled.  Such overlap
             should have been detected and reported above.  */
          if (!clobber_conflict_found && REG_P (clobbered_reg)){
              /* We test the old body (obody) contents to avoid
             tripping over the under-construction body.  */
              for (unsigned k = 0; k < noutputs; ++k)
                if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,clobbered_reg, output_rvec[k]))
                  internal_error ("%<asm%> clobber conflict with output operand");

              for (unsigned k = 0; k < ninputs - ninout; ++k)
                if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,clobbered_reg, input_rvec[k]))
                  internal_error ("%<asm%> clobber conflict with input operand");
          }
          XVECEXP (body, 0, i++) = gen_rtx_CLOBBER (VOIDmode, clobbered_reg);
      }

      if (nlabels > 0)
          mtcs_emit_emit_jump_insn/*!emit_jump_insn*/(mtcsEmit,body);
      else
          mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,body);
  }

  generating_concat_p = old_generating_concat_p;
  if (fallthru_label)
     mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,fallthru_label);

  if (after_md_seq)
    mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,after_md_seq);

  if (after_rtl_seq){
      if (nlabels == 0)
          mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,after_rtl_seq);
      else{
          edge e;
          edge_iterator ei;
          unsigned int cnt = EDGE_COUNT (gimple_bb (stmt)->succs);

          FOR_EACH_EDGE (e, ei, gimple_bb (stmt)->succs){
              rtx_insn *copy;
              if (--cnt == 0)
                  copy = after_rtl_seq;
              else{
                  mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
                  mtcs_cfg_rtl_duplicate_insn_chain/*!duplicate_insn_chain*/(mtcsCfgRtl,after_rtl_seq, after_rtl_end,NULL, NULL);
                  copy = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
                  mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
              }
              mtcs_cfg_rtl_prepend_insn_to_edge/*!prepend_insn_to_edge*/(mtcsCfgRtl,copy, e);
          }
      }
  }
  free_temp_slots ();
  mtcsRtlData/*!crtl*/->has_asm_statement = 1;
}

static void printFnInfo(gcall *stmt)
{
    if(5>3)
       return;
//   inline bool
//   gimple_call_internal_p (const gcall *gs)
//   {
//     return (gs->subcode & GF_CALL_INTERNAL) != 0;
//   }
   bool gimpleInFn=gimple_call_internal_p (stmt);
   n_debug("mtcsexpand.c printFnInfo 00 函数调用来自 gimple_call_internal_p:%d\n",gimpleInFn);
   aet_print_gimple(stmt);
   tree decl = gimple_call_fndecl (stmt);
   n_debug("mtcsexpand.c printFnInfo 11 函数调用来自 gimple_call_internal_p:%d\n",gimpleInFn);
   aet_print_tree(decl);
   if(decl){
//      bool
//      called_as_built_in (tree node)
//      {
//        /* Note that we must use DECL_NAME, not DECL_ASSEMBLER_NAME_SET_P since
//           we want the name used to call the function, not the name it
//           will have. */
//        const char *name = IDENTIFIER_POINTER (DECL_NAME (node));
//        return is_builtin_name (name);
//      }
      bool called= called_as_built_in (decl);
//      inline bool  fndecl_built_in_p (const_tree node)
//      {
//        return DECL_BUILT_IN_CLASS (node) != NOT_BUILT_IN;
//      }
      bool builtfn=fndecl_built_in_p (decl);
      n_debug("mtcsexpand.c printFnInfo 22 函数调用来自 called_as_built_in:%d fndecl_built_in_p:%d\n",called,builtfn);

   }

}

/* A subroutine of expand_gimple_stmt_1, expanding one GIMPLE_CALL
   statement STMT.  */
//原型 expand_call_stmt cfgexpand.cc
static void expand_call_stmt (MtcsExpand *self,gcall *stmt)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
  MtcsTree *mtcsTree = mtcs_target_get_tree(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
  MtcsBuiltins *mtcsBuiltins = mtcs_target_get_builtins(mtcsTarget);
  MtcsInternalFn *mtcsInternalFn = mtcs_target_get_internal_fn(mtcsTarget);

  tree exp, decl, lhs;
  bool builtin_p;
  size_t i;
  printFnInfo(stmt);
  if (gimple_call_internal_p (stmt)){
      n_debug("mtcsexpand.c expand_call_stmt 00 内部函数\n");
      aet_print_gimple(stmt);
      mtcs_internal_fn_expand_internal_call/*!expand_internal_call*/(mtcsInternalFn,stmt);
      return;
  }

  /* If this is a call to a built-in function and it has no effect other
     than setting the lhs, try to implement it using an internal function
     instead.  */
  decl = gimple_call_fndecl (stmt);
  if (gimple_call_lhs (stmt)  && !gimple_has_side_effects (stmt)
      && (mtcsOptionsItem->x_optimize || (decl && called_as_built_in (decl)))){
      internal_fn ifn = mtcs_builtins_replacement_internal_fn/*!replacement_internal_fn*/(mtcsBuiltins,stmt);
      n_debug("mtcsexpand.c expand_call_stmt builtin函数可以用internal实现 ifn:%d IFN_LAST:%d\n",ifn,IFN_LAST);
      if (ifn != IFN_LAST){
          mtcs_internal_fn_expand_internal_call/*!expand_internal_call*/(mtcsInternalFn,ifn, stmt);
          return;
      }
  }

  exp = build_vl_exp (CALL_EXPR, gimple_call_num_args (stmt) + 3);
  CALL_EXPR_FN (exp) = gimple_call_fn (stmt);
  builtin_p = decl && fndecl_built_in_p (decl);
  n_debug("mtcsexpand.cc expand_call_stmt 22 内建函数 decl:%p builtin_p:%d\n",decl,builtin_p);
  /* If this is not a builtin function, the function type through which the
     call is made may be different from the type of the function.  */
  if (!builtin_p)
    CALL_EXPR_FN (exp) = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,
          mtcs_tree_build_pointer_type/*!build_pointer_type*/(mtcsTree,gimple_call_fntype (stmt)),CALL_EXPR_FN (exp));

  TREE_TYPE (exp) = gimple_call_return_type (stmt);
  CALL_EXPR_STATIC_CHAIN (exp) = gimple_call_chain (stmt);
  for (i = 0; i < gimple_call_num_args (stmt); i++){
      tree arg = gimple_call_arg (stmt, i);
      gimple *def;
      /* TER addresses into arguments of builtin functions so we have a
     chance to infer more correct alignment information.  See PR39954.  */
      if (builtin_p  && TREE_CODE (arg) == SSA_NAME
      && (def = get_gimple_for_ssa_name (arg))  && gimple_assign_rhs_code (def) == ADDR_EXPR){
         n_debug("mtcsexpand.c expand_call_stmt 33 内置函数 将 TER 地址放入内置函数的参数中，"
               "以便我们有机会推断出更正确的对齐信息。参见 PR39954 builtin_p:%d\n",builtin_p);
          arg = gimple_assign_rhs1 (def);
      }
      CALL_EXPR_ARG (exp, i) = arg;
  }
  if (gimple_has_side_effects (stmt)
      /* ???  Downstream in expand_expr_real_1 we assume that expressions
     w/o side-effects do not throw so work around this here.  */
      || stmt_could_throw_p (cfun, stmt))
    TREE_SIDE_EFFECTS (exp) = 1;

  n_debug("mtcsexpand.c expand_call_stmt 44 内建函数 decl:%p builtin_p:%d TREE_SIDE_EFFECTS (exp):%d\n",
        decl,builtin_p,TREE_SIDE_EFFECTS (exp));

  if (gimple_call_nothrow_p (stmt))
    TREE_NOTHROW (exp) = 1;
  CALL_EXPR_TAILCALL (exp) = gimple_call_tail_p (stmt);
  CALL_EXPR_MUST_TAIL_CALL (exp) = gimple_call_must_tail_p (stmt);
  CALL_EXPR_RETURN_SLOT_OPT (exp) = gimple_call_return_slot_opt_p (stmt);
  if(decl && fndecl_built_in_p (decl, BUILT_IN_NORMAL))
  n_debug("mtcsexpand.c expand_call_stmt 44aa decl:%p %d %d %d %s\n",
        decl,fndecl_built_in_p (decl, BUILT_IN_NORMAL),
        ALLOCA_FUNCTION_CODE_P (DECL_FUNCTION_CODE (decl)),DECL_FUNCTION_CODE (decl),IDENTIFIER_POINTER(DECL_NAME(decl)));
  if (decl  && fndecl_built_in_p (decl, BUILT_IN_NORMAL) && ALLOCA_FUNCTION_CODE_P (DECL_FUNCTION_CODE (decl))){
     n_debug("mtcsexpand.c expand_call_stmt 55 内建函数 decl:%p builtin_p:%d TREE_SIDE_EFFECTS (exp):%d\n",
           decl,builtin_p,TREE_SIDE_EFFECTS (exp));
     CALL_ALLOCA_FOR_VAR_P (exp) = gimple_call_alloca_for_var_p (stmt);
  }else{
     n_debug("mtcsexpand.c expand_call_stmt 66 内置函数 decl:%p builtin_p:%d TREE_SIDE_EFFECTS (exp):%d\n",
           decl,builtin_p,TREE_SIDE_EFFECTS (exp));
     CALL_FROM_THUNK_P (exp) = gimple_call_from_thunk_p (stmt);
  }
  CALL_EXPR_VA_ARG_PACK (exp) = gimple_call_va_arg_pack_p (stmt);
  CALL_EXPR_BY_DESCRIPTOR (exp) = gimple_call_by_descriptor_p (stmt);
  SET_EXPR_LOCATION (exp, gimple_location (stmt));
  /* Must come after copying location.  */
  copy_warning (exp, stmt);
  /* Ensure RTL is created for debug args.  */
  if (decl && DECL_HAS_DEBUG_ARGS_P (decl)){
      vec<tree, va_gc> **debug_args = decl_debug_args_lookup (decl);
      unsigned int ix;
      tree dtemp;
      if (debug_args)
        for (ix = 1; (*debug_args)->iterate (ix, &dtemp); ix += 2){
            gcc_assert (TREE_CODE (dtemp) == DEBUG_EXPR_DECL);
            expand_debug_expr(self,dtemp);
        }
  }

  rtx_insn *before_call = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
  lhs = gimple_call_lhs (stmt);
  n_debug("mtcsexpand.c expand_call_stmt 77 lhs:%p\n",lhs);

  if (lhs)
      mtcs_expr_expand_assignment/*!expand_assignment*/(mtcsExpr,lhs, exp, false);
  else
      mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,exp, const0_rtx, VOIDmode, EXPAND_NORMAL);

  n_debug("mtcsexpand.c expand_call_stmt 88 主要工作完成，生成 call rtx了吗  builtin_p:%d lhs:%p\n",builtin_p,lhs);

  /* If the gimple call is an indirect call and has 'nocf_check'
     attribute find a generated CALL insn to mark it as no
     control-flow verification is needed.  */
  if (gimple_call_nocf_check_p (stmt) && !gimple_call_fndecl (stmt)){
      rtx_insn *last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
      while (!CALL_P (last)  && last != before_call){
         n_debug("mtcsexpand.c expand_call_stmt 99 主要工作完成，生成 call rtx了吗  builtin_p:%d lhs:%p\n",builtin_p,lhs);

          last = PREV_INSN (last);
      }
      if (last != before_call){
         n_debug("mtcsexpand.c expand_call_stmt 100 主要工作完成，生成 call rtx了吗  builtin_p:%d lhs:%p\n",builtin_p,lhs);

          add_reg_note (last, REG_CALL_NOCF_CHECK, const0_rtx);
      }
  }

  mark_transaction_restart_calls(self,stmt);
}


/* A subroutine of expand_gimple_stmt, expanding one gimple statement
   STMT that doesn't require special handling for outgoing edges.  That
   is no tailcalls and no GIMPLE_COND.  */
//原型 expand_gimple_stmt_1 cfgexpand.cc
static void expand_gimple_stmt_1 (MtcsExpand *self,gimple *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsStmt *mtcsStmt=mtcs_target_get_stmt(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   tree op0;
   mtcs_emit_set_curr_insn_location/*!set_curr_insn_location*/(mtcsEmit,gimple_location (stmt));
   switch (gimple_code (stmt)){
      case GIMPLE_GOTO:
         op0 = gimple_goto_dest (stmt);
         if (TREE_CODE (op0) == LABEL_DECL)
            expand_goto(self,op0);
         else
            expand_computed_goto(self,op0);
         break;
      case GIMPLE_LABEL:
         mtcs_stmt_expand_label/*!expand_label*/(mtcsStmt,gimple_label_label (as_a <glabel *> (stmt)));
         break;
      case GIMPLE_NOP:
      case GIMPLE_PREDICT:
         break;
      case GIMPLE_SWITCH:
      {
         gswitch *swtch = as_a <gswitch *> (stmt);
         if (gimple_switch_num_labels (swtch) == 1)
            expand_goto(self,CASE_LABEL (gimple_switch_default_label (swtch)));
         else
            mtcs_stmt_expand_case/*!expand_case*/(mtcsStmt,swtch);
      }
         break;
      case GIMPLE_ASM:
         expand_asm_stmt(self,as_a <gasm *> (stmt));
         break;
      case GIMPLE_CALL:
         n_debug("mtcsexpand.c expand_gimple_stmt_1 GIMPLE_CALL \n");
         aet_print_gimple(stmt);
         expand_call_stmt(self,as_a <gcall *> (stmt));
         break;

      case GIMPLE_RETURN:
      {
         op0 = gimple_return_retval (as_a <greturn *> (stmt));
         /* If a return doesn't have a location, it very likely represents
         multiple user returns so we cannot let it inherit the location
         of the last statement of the previous basic block in RTL.  */
         if (!gimple_has_location (stmt))
            mtcs_emit_set_curr_insn_location/*!set_curr_insn_location*/(mtcsEmit,cfun->function_end_locus);

         if (op0 && op0 != error_mark_node){
            tree result = DECL_RESULT (current_function_decl);
            /* If we are not returning the current function's RESULT_DECL,
            build an assignment to it.  */
            if (op0 != result){
               /* I believe that a function's RESULT_DECL is unique.  */
               gcc_assert (TREE_CODE (op0) != RESULT_DECL);

               /* ??? We'd like to use simply expand_assignment here,
               but this fails if the value is of BLKmode but the return
               decl is a register.  expand_return has special handling
               for this combination, which eventually should move
               to common code.  See comments there.  Until then, let's
               build a modify expression :-/  */
               op0 = build2 (MODIFY_EXPR, TREE_TYPE (result),result, op0);
            }
         }

         if (!op0)
            mtcs_expand_expand_null_return/*!expand_null_return*/(self);
         else
            expand_return(self,op0);
      }
         break;

      case GIMPLE_ASSIGN:
      {
         gassign *assign_stmt = as_a <gassign *> (stmt);
         tree lhs = gimple_assign_lhs (assign_stmt);
         /* Tree expand used to fiddle with |= and &= of two bitfield
         COMPONENT_REFs here.  This can't happen with gimple, the LHS
         of binary assigns must be a gimple reg.  */
         if (TREE_CODE (lhs) != SSA_NAME || gimple_assign_rhs_class (assign_stmt) == GIMPLE_SINGLE_RHS){
            n_debug("mtcsexpand.c expand_gimple_stmt_1 00 GIMPLE_ASSIGN\n");
            aet_print_gimple(assign_stmt);
            tree rhs = gimple_assign_rhs1 (assign_stmt);
            gcc_assert (gimple_assign_rhs_class (assign_stmt)== GIMPLE_SINGLE_RHS);
            if (gimple_has_location (stmt) && CAN_HAVE_LOCATION_P (rhs)
            /* Do not put locations on possibly shared trees.  */
            && !is_gimple_min_invariant (rhs))
            SET_EXPR_LOCATION (rhs, gimple_location (stmt));
            if (TREE_CLOBBER_P (rhs)){
               n_debug("mtcsexpand.c expand_gimple_stmt_1 11 GIMPLE_ASSIGN\n");

               /* This is a clobber to mark the going out of scope for
               this LHS.  */
               expand_clobber(self,lhs);
            }else{
               n_debug("mtcsexpand.c expand_gimple_stmt_1 22 调用 expand_assignment \n");

               mtcs_expr_expand_assignment/*!expand_assignment*/(mtcsExpr,
                     lhs, rhs, gimple_assign_nontemporal_move_p ( assign_stmt));
               n_debug("mtcsexpand.c expand_gimple_stmt_1 22xxxx GIMPLE_ASSIGN\n");

            }
         }else{
            n_debug("mtcsexpand.c expand_gimple_stmt_1 33 GIMPLE_ASSIGN\n");
            aet_print_gimple(assign_stmt);

            rtx target, temp;
            bool nontemporal = gimple_assign_nontemporal_move_p (assign_stmt);
            bool promoted = false;

            target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
            if (GET_CODE (target) == SUBREG && SUBREG_PROMOTED_VAR_P (target))
               promoted = true;
            n_debug("mtcsexpand.c expand_gimple_stmt_1 44 GIMPLE_ASSIGN target:%p GET_MODE (target):%d promoted:%d\n",
                  target,GET_MODE (target),promoted);
            mtcs_print_rtl_single(stderr,target);

            /* If we want to use a nontemporal store, force the value to
            register first.  If we store into a promoted register,
            don't directly expand to target.  */
            temp = nontemporal || promoted ? NULL_RTX : target;
            temp = mtcs_expr_expand_expr_real_gassign/*!expand_expr_real_gassign*/(mtcsExpr,
                  assign_stmt, temp, GET_MODE (target), EXPAND_NORMAL);
            n_debug("mtcsexpand.c expand_gimple_stmt_1 55 GIMPLE_ASSIGN\n");

            if (temp == target)
               ;
            else if (promoted){
               int unsignedp = SUBREG_PROMOTED_SIGN (target);
               /* If TEMP is a VOIDmode constant, use convert_modes to make
               sure that we properly convert it.  */
               if (CONSTANT_P (temp) && GET_MODE (temp) == VOIDmode){
                  temp = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,GET_MODE (target),TYPE_MODE (TREE_TYPE (lhs)), temp, unsignedp);
                  temp = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,
                  GET_MODE (SUBREG_REG (target)),GET_MODE (target), temp, unsignedp);
               }
               mtcs_expr_convert_move/*!convert_move */(mtcsExpr,SUBREG_REG (target), temp, unsignedp);
            }else if (nontemporal && mtcs_expr_emit_storent_insn/*!emit_storent_insn*/(mtcsExpr,target, temp))
               ;
            else{
               temp = mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,temp, target);
               if (temp != target)
                  mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, temp);
            }
         }
      }
         break;

      default:
         gcc_unreachable ();
   }
}


/* Expand one gimple statement STMT and return the last RTL instruction
   before any of the newly generated ones.

   In addition to generating the necessary RTL instructions this also
   sets REG_EH_REGION notes if necessary and sets the current source
   location for diagnostics.  */
//原型 expand_gimple_stmt cfgexpand.cc
static rtx_insn * expand_gimple_stmt (MtcsExpand *self,gimple *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExcept *mtcsExcept=mtcs_target_get_except(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   location_t saved_location = input_location;
   rtx_insn *last =  mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
   int lp_nr;
   gcc_assert (cfun);
   /* We need to save and restore the current source location so that errors
   discovered during expansion are emitted with the right location.  But
   it would be better if the diagnostic routines used the source location
   embedded in the tree nodes rather than globals.  */
   if (gimple_has_location (stmt))
      input_location = gimple_location (stmt);
   n_debug("mtcsexpand.c expand_gimple_stmt 00 \n");
   aet_print_gimple(stmt);

   expand_gimple_stmt_1(self,stmt);
   /* Free any temporaries used to evaluate this statement.  */
   mtcs_func_free_temp_slots/*!free_temp_slots*/(mtcsFunc);
   input_location = saved_location;
   /* Mark all insns that may trap.  */
   lp_nr = lookup_stmt_eh_lp (stmt);
   if (lp_nr){
      rtx_insn *insn;
      for (insn = next_real_insn (last); insn;insn = next_real_insn (insn)){
         if (! find_reg_note (insn, REG_EH_REGION, NULL_RTX)
         /* If we want exceptions for non-call insns, any
         may_trap_p instruction may throw.  */
         && GET_CODE (PATTERN (insn)) != CLOBBER
         && GET_CODE (PATTERN (insn)) != USE
         && mtcs_except_insn_could_throw_p/*!insn_could_throw_p*/(mtcsExcept,insn))
            make_reg_eh_region_note (insn, 0, lp_nr);
      }
   }
   return last;
}

/* Returns the label_rtx expression for a label starting basic block BB.  */
static rtx_code_label *label_rtx_for_bb (MtcsExpand *self,basic_block bb ATTRIBUTE_UNUSED)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
  MtcsStmt *mtcsStmt=mtcs_target_get_stmt(mtcsTarget);
  n_debug("mtcsexpand.c label_rtx_for_bb 00 bb:%p %d\n",bb,bb->index);

  if (bb->flags & BB_RTL){
     n_debug("mtcsexpand.c label_rtx_for_bb 11 bb:%p %d block_label (bb):%p uid:%d\n",bb,bb->index,
           mtcs_cfg_rtl_block_label/*!block_label*/(mtcsCfgRtl,bb),INSN_UID(mtcs_cfg_rtl_block_label/*!block_label*/(mtcsCfgRtl,bb)));
     return mtcs_cfg_rtl_block_label/*!block_label*/(mtcsCfgRtl,bb);
  }
  rtx_code_label **elt = self->lab_rtx_for_bb->get (bb);
  n_debug("mtcsexpand.c label_rtx_for_bb 22 bb:%p %d elt:%p\n",bb,bb->index,elt);
  if (elt)
    return *elt;
  /* Find the tree label if it is present.  */
  gimple_stmt_iterator gsi = gsi_start_bb (bb);
  glabel *lab_stmt;
  if (!gsi_end_p (gsi)
      && (lab_stmt = dyn_cast <glabel *> (gsi_stmt (gsi)))
      && !DECL_NONLOCAL (gimple_label_label (lab_stmt))){
     n_debug("mtcsexpand.c label_rtx_for_bb 33 bb:%p %d gimple_label_label (lab_stmt):%p\n",bb,bb->index,gimple_label_label (lab_stmt));
    return mtcs_stmt_jump_target_rtx/*!jump_target_rtx*/(mtcsStmt,gimple_label_label (lab_stmt));
  }
  rtx_code_label *l =mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
  n_debug("mtcsexpand.c label_rtx_for_bb 44 新建 rtx_code_label bb:%p %d l:%p\n",bb,bb->index,l);
  self->lab_rtx_for_bb->put (bb, l);
  return l;
}

/* A subroutine of expand_gimple_basic_block.  Expand one GIMPLE_CALL
   that has CALL_EXPR_TAILCALL set.  Returns non-null if we actually
   generated a tail call (something that might be denied by the ABI
   rules governing the call; see calls.cc).

   Sets CAN_FALLTHRU if we generated a *conditional* tail call, and
   can still reach the rest of BB.  The case here is __builtin_sqrt,
   where the NaN result goes through the external function (with a
   tailcall) and the normal result happens via a sqrt instruction.  */
//原型 expand_gimple_tailcall cfgexpand.cc
static basic_block expand_gimple_tailcall (MtcsExpand *self,basic_block bb, gcall *stmt, bool *can_fallthru,rtx_insn *asan_epilog_seq)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr *mtcsExpr =mtcs_target_get_expr(mtcsTarget);
   MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsCfg  *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   rtx_insn *last2, *last;
   rtx_insn *first=mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
   edge e;
   edge_iterator ei;
   profile_probability probability;
   n_debug("mtcsexpand.c expand_gimple_tailcall 00 \n");
   last2 = last = expand_gimple_stmt(self,stmt);
   for (last = NEXT_INSN (last); last; last = NEXT_INSN (last)){
      n_debug("mtcsexpr.c expand_gimple_tailcall --11 last:%p %d\n",last,CALL_P (last));
      mtcs_print_rtl (stderr,last);
      if (CALL_P (last) && SIBLING_CALL_P (last)){
         n_debug("mtcsexpr.c expand_gimple_tailcall 11 last:%p\n",last);
         goto found;
      }
   }
   n_debug("mtcsexpr.c expand_gimple_tailcall 22aa last:%p\n",last);

   maybe_dump_rtl_for_gimple_stmt (stmt, last2);
   *can_fallthru = true;
   return NULL;

found:

   n_debug("mtcsexpr.c expand_gimple_tailcall 22bb last:%p\n",last);
   if (asan_epilog_seq){
      /* We need to emit a copy of the asan_epilog_seq before
      the insns emitted by expand_gimple_stmt above.  The sequence
      can contain labels, which need to be remapped.  */
      hash_map<rtx, rtx> label_map;
      mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
      mtcs_emit_emit_note/*!emit_note*/(mtcsEmit,(int)NOTE_INSN_DELETED);
      for (rtx_insn *insn = asan_epilog_seq; insn; insn = NEXT_INSN (insn))
         switch (GET_CODE (insn)){
            case INSN:
            case CALL_INSN:
            case JUMP_INSN:
               mtcs_emit_emit_copy_of_insn_after/*!emit_copy_of_insn_after*/(mtcsEmit,
                     insn, mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData));
               break;
            case CODE_LABEL:
               label_map.put ((rtx) insn,
                     (rtx) mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL)));
               break;
            case BARRIER:
               mtcs_emit_emit_barrier/*!emit_barrier*/(mtcsEmit);
               break;
            default:
               gcc_unreachable ();
         }
      for (rtx_insn *insn = get_insns (); insn; insn = NEXT_INSN (insn))
         if (JUMP_P (insn)){
            subrtx_ptr_iterator::array_type array;
            FOR_EACH_SUBRTX_PTR (iter, array, &PATTERN (insn), ALL){
               rtx *loc = *iter;
               if (LABEL_REF_P (*loc)){
                  rtx *lab = label_map.get ((rtx) label_ref_label (*loc));
                  gcc_assert (lab);
                  set_label_ref_label (*loc, as_a <rtx_insn *> (*lab));
               }
            }
            if (JUMP_LABEL (insn)){
               rtx *lab = label_map.get (JUMP_LABEL (insn));
               gcc_assert (lab);
               JUMP_LABEL (insn) = *lab;
            }
         }
      asan_epilog_seq = NEXT_INSN (get_insns ());
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,asan_epilog_seq, NEXT_INSN (first));
   }

   /* ??? Wouldn't it be better to just reset any pending stack adjust?
   Any instructions emitted here are about to be deleted.  */
   mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
   /* Remove any non-eh, non-abnormal edges that don't go to exit.  */
   /* ??? I.e. the fallthrough edge.  HOWEVER!  If there were to be
   EH or abnormal edges, we shouldn't have created a tail call in
   the first place.  So it seems to me we should just be removing
   all edges here, or redirecting the existing fallthru edge to
   the exit block.  */

   probability = profile_probability::never ();

   for (ei = ei_start (bb->succs); (e = ei_safe_edge (ei)); ){
      n_debug("mtcsexpr.c expand_gimple_tailcall 22 ei:%p\n",ei);

      if (!(e->flags & (EDGE_ABNORMAL | EDGE_EH))){
         n_debug("mtcsexpr.c expand_gimple_tailcall 33 \n");
         if (e->dest != EXIT_BLOCK_PTR_FOR_FN (cfun))
            e->dest->count -= e->count ();
         probability += e->probability;
         mtcs_cfg_context_remove_edge/*!remove_edge*/(mtcsCfgContext,e);
      }else
         ei_next (&ei);
   }

   /* This is somewhat ugly: the call_expr expander often emits instructions
   after the sibcall (to perform the function return).  These confuse the
   find_many_sub_basic_blocks code, so we need to get rid of these.  */
   last = NEXT_INSN (last);
   gcc_assert (BARRIER_P (last));

   *can_fallthru = false;
   while (NEXT_INSN (last)){
      n_debug("mtcsexpr.c expand_gimple_tailcall 44 \n");
      /* For instance an sqrt builtin expander expands if with
      sibcall in the then and label for `else`.  */
      if (LABEL_P (NEXT_INSN (last))){
         n_debug("mtcsexpr.c expand_gimple_tailcall 55 \n");
         *can_fallthru = true;
         break;
      }
      mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,NEXT_INSN (last));
   }

   e =mtcs_cfg_make_edge/*!make_edge*/(mtcsCfg,bb, EXIT_BLOCK_PTR_FOR_FN (cfun), EDGE_ABNORMAL | EDGE_SIBCALL);
   e->probability = probability;
   BB_END (bb) = last;
   mtcs_cfg_rtl_update_bb_for_insn/*!update_bb_for_insn*/(mtcsCfgRtl,bb);
   if (NEXT_INSN (last)){
      n_debug("mtcsexpr.c expand_gimple_tailcall 66 bb:%p\n",bb);

      bb = mtcs_cfg_context_create_basic_block/*!create_basic_block*/(mtcsCfgContext,
      NEXT_INSN (last),  mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData), bb);
      last = BB_END (bb);
      if (BARRIER_P (last))
         BB_END (bb) = PREV_INSN (last);
   }
   maybe_dump_rtl_for_gimple_stmt (stmt, last2);
   n_debug("mtcsexpr.c expand_gimple_tailcall 77 bb:%p\n",bb);

   return bb;
}


/* Expand basic block BB from GIMPLE trees to RTL.  */
//原型 expand_gimple_basic_block cfgexpand.cc
static basic_block expand_gimple_basic_block (MtcsExpand *self,basic_block bb, rtx_insn *asan_epilog_seq)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   gimple_stmt_iterator gsi;
   gimple_seq stmts;
   gimple *stmt = NULL;
   rtx_note *note = NULL;
   rtx_insn *last;
   edge e;
   edge_iterator ei;
   bool nondebug_stmt_seen = false;

   if (dump_file)
      fprintf (dump_file, "\n;; Generating RTL for gimple basic block %d\n", bb->index);
   /* Note that since we are now transitioning from GIMPLE to RTL, we
   cannot use the gsi_*_bb() routines because they expect the basic
   block to be in GIMPLE, instead of RTL.  Therefore, we need to
   access the BB sequence directly.  */
   if (mtcsOptionsItem->x_optimize)
      reorder_operands(self,bb);
   stmts = bb_seq (bb);
   bb->il.gimple.seq = NULL;
   bb->il.gimple.phi_nodes = NULL;
   mtcs_func_rtl_profile_for_bb/*!rtl_profile_for_bb*/(mtcsFunc,bb);
   mtcs_cfg_rtl_init_rtl_bb_info/*!init_rtl_bb_info*/(mtcsCfgRtl,bb);
   bb->flags |= BB_RTL;

   n_debug("mtcsexpand.c expand_gimple_basic_block 00 \n");
   aet_print_seq(stmts);

   /* Remove the RETURN_EXPR if we may fall though to the exit
   instead.  */
   gsi = gsi_last (stmts);
   if (!gsi_end_p (gsi) && gimple_code (gsi_stmt (gsi)) == GIMPLE_RETURN){
      n_debug("mtcsexpand.c expand_gimple_basic_block 11 !gsi_end_p (gsi)  && gimple_code (gsi_stmt (gsi)) == GIMPLE_RETURN\n");

      greturn *ret_stmt = as_a <greturn *> (gsi_stmt (gsi));
      gcc_assert (single_succ_p (bb));
      gcc_assert (single_succ (bb) == EXIT_BLOCK_PTR_FOR_FN (cfun));
      if (bb->next_bb == EXIT_BLOCK_PTR_FOR_FN (cfun) && !gimple_return_retval (ret_stmt)){
         n_debug("mtcsexpand.c expand_gimple_basic_block 22 \n");
         gsi_remove (&gsi, false);
         single_succ_edge (bb)->flags |= EDGE_FALLTHRU;
      }
   }

   gsi = gsi_start (stmts);
   if (!gsi_end_p (gsi)){
      stmt = gsi_stmt (gsi);
      n_debug("mtcsexpand.c expand_gimple_basic_block 33 gimple_code (stmt) != GIMPLE_LABEL:%d\n", gimple_code (stmt) != GIMPLE_LABEL);
      aet_print_gimple(stmt);
      if (gimple_code (stmt) != GIMPLE_LABEL)
         stmt = NULL;
   }
   rtx_code_label **elt =self->lab_rtx_for_bb->get (bb);
   if (stmt || elt){
      gcc_checking_assert (!note);
      last =  mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
      n_debug("mtcsexpand.c expand_gimple_basic_block 44 (stmt || elt) stmt:%p elt:%p\n",stmt,elt);

      if (stmt){
         expand_gimple_stmt(self,stmt);
         gsi_next (&gsi);
      }

      if (elt)
         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,*elt);

      BB_HEAD (bb) = NEXT_INSN (last);
      if (NOTE_P (BB_HEAD (bb)))
         BB_HEAD (bb) = NEXT_INSN (BB_HEAD (bb));
      gcc_assert (LABEL_P (BB_HEAD (bb)));
      note = mtcs_emit_emit_note_after/*!emit_note_after*/(mtcsEmit,NOTE_INSN_BASIC_BLOCK, BB_HEAD (bb));

      maybe_dump_rtl_for_gimple_stmt (stmt, last);
   }else{
      BB_HEAD (bb) = note = mtcs_emit_emit_note/*!emit_note*/(mtcsEmit,(int)NOTE_INSN_BASIC_BLOCK);
      n_debug("mtcsexpand.c expand_gimple_basic_block 55 设 BB_HEAD(bb) bb:%p\n",bb);
   }

   if (note)
      NOTE_BASIC_BLOCK (note) = bb;

   for (; !gsi_end_p (gsi); gsi_next (&gsi)){
      basic_block new_bb;
      stmt = gsi_stmt (gsi);
      n_debug("mtcsexpand.c expand_gimple_basic_block 66 \n");
      aet_print_gimple(stmt);
      if (!is_gimple_debug (stmt))
         nondebug_stmt_seen = true;

      /* If this statement is a non-debug one, and we generate debug
      insns, then this one might be the last real use of a TERed
      SSA_NAME, but where there are still some debug uses further
      down.  Expanding the current SSA name in such further debug
      uses by their RHS might lead to wrong debug info, as coalescing
      might make the operands of such RHS be placed into the same
      pseudo as something else.  Like so:
      a_1 = a_0 + 1;   // Assume a_1 is TERed and a_0 is dead
      use(a_1);
      a_2 = ...
      #DEBUG ... => a_1
      As a_0 and a_2 don't overlap in lifetime, assume they are coalesced.
      If we now would expand a_1 by it's RHS (a_0 + 1) in the debug use,
      the write to a_2 would actually have clobbered the place which
      formerly held a_0.

      So, instead of that, we recognize the situation, and generate
      debug temporaries at the last real use of TERed SSA names:
      a_1 = a_0 + 1;
      #DEBUG #D1 => a_1
      use(a_1);
      a_2 = ...
      #DEBUG ... => #D1
      */
      if (MAY_HAVE_DEBUG_BIND_INSNS   && SA.values   && !is_gimple_debug (stmt)){
         n_debug("mtcsexpand.c expand_gimple_basic_block 77 \n");

         ssa_op_iter iter;
         tree op;
         gimple *def;
         location_t sloc = curr_insn_location ();
         /* Look for SSA names that have their last use here (TERed
         names always have only one real use).  */
         FOR_EACH_SSA_TREE_OPERAND (op, stmt, iter, SSA_OP_USE)
            if ((def = get_gimple_for_ssa_name (op)) && is_gimple_assign (def)){
               imm_use_iterator imm_iter;
               use_operand_p use_p;
               bool have_debug_uses = false;
               n_debug("mtcsexpand.c expand_gimple_basic_block 88 \n");

               FOR_EACH_IMM_USE_FAST (use_p, imm_iter, op){
                  if (gimple_debug_bind_p (USE_STMT (use_p))){
                     have_debug_uses = true;
                     n_debug("mtcsexpand.c expand_gimple_basic_block 99 \n");
                     break;
                  }
               }

               if (have_debug_uses){
                  /* OP is a TERed SSA name, with DEF its defining
                  statement, and where OP is used in further debug
                  instructions.  Generate a debug temporary, and
                  replace all uses of OP in debug insns with that
                  temporary.  */
                  gimple *debugstmt;
                  tree value = gimple_assign_rhs_to_tree (def);
                  tree vexpr = build_debug_expr_decl (TREE_TYPE (value));
                  rtx val;
                  machine_mode mode;
                  n_debug("mtcsexpand.c expand_gimple_basic_block 100 have_debug_uses=true\n");
                  mtcs_emit_set_curr_insn_location/*!set_curr_insn_location*/(mtcsEmit,gimple_location (def));
                  if (DECL_P (value))
                     mode = DECL_MODE (value);
                  else
                     mode = TYPE_MODE (TREE_TYPE (value));
                  /* FIXME: Is setting the mode really necessary? */
                  SET_DECL_MODE (vexpr, mode);
                  val = gen_rtx_VAR_LOCATION(mode, vexpr, (rtx)value, VAR_INIT_STATUS_INITIALIZED);
                  mtcs_emit_emit_debug_insn/*!emit_debug_insn*/(mtcsEmit,val);
                  FOR_EACH_IMM_USE_STMT (debugstmt, imm_iter, op){
                     if (!gimple_debug_bind_p (debugstmt))
                        continue;
                     FOR_EACH_IMM_USE_ON_STMT (use_p, imm_iter)
                        SET_USE (use_p, vexpr);
                     update_stmt (debugstmt);
                  }
               }//end if (have_debug_uses){
            }//end if ((def = get_gimple_for_ssa_name (op))){
         mtcs_emit_set_curr_insn_location/*!set_curr_insn_location*/(mtcsEmit,sloc);
      }//end if (MAY_HAVE_DEBUG_BIND_INSNS....

      currently_expanding_gimple_stmt = stmt;

      /* Expand this statement, then evaluate the resulting RTL and
      fixup the CFG accordingly.  */
      if (gimple_code (stmt) == GIMPLE_COND){
         n_debug("mtcsexpand.c expand_gimple_basic_block 101 gimple_code (stmt) == GIMPLE_COND \n");
         new_bb = expand_gimple_cond(self,bb, as_a <gcond *> (stmt));

         if (new_bb){
            currently_expanding_gimple_stmt = NULL;
            n_debug("mtcsexpand.c expand_gimple_basic_block 101aa gimple_code (stmt) == GIMPLE_COND bb:%p %d\n",new_bb,new_bb->index);
            return new_bb;
         }
      }else if (is_gimple_debug (stmt)){
         location_t sloc = curr_insn_location ();
         gimple_stmt_iterator nsi = gsi;
         n_debug("mtcsexpand.c expand_gimple_basic_block 102 is_gimple_debug (stmt) \n");

         for (;;){
            tree var;
            tree value = NULL_TREE;
            rtx val = NULL_RTX;
            machine_mode mode;

            if (!gimple_debug_nonbind_marker_p (stmt)){
               if (gimple_debug_bind_p (stmt)){
                  var = gimple_debug_bind_get_var (stmt);
                  if (TREE_CODE (var) != DEBUG_EXPR_DECL
                  && TREE_CODE (var) != LABEL_DECL
                  && !target_for_debug_bind (var))
                     goto delink_debug_stmt;

                  if (DECL_P (var) && !VECTOR_TYPE_P (TREE_TYPE (var)))
                     mode = DECL_MODE (var);
                  else
                     mode = TYPE_MODE (TREE_TYPE (var));

                  if (gimple_debug_bind_has_value_p (stmt))
                     value = gimple_debug_bind_get_value (stmt);

                  val = gen_rtx_VAR_LOCATION(mode, var, (rtx)value, VAR_INIT_STATUS_INITIALIZED);
               }else if (gimple_debug_source_bind_p (stmt)){
                  var = gimple_debug_source_bind_get_var (stmt);
                  value = gimple_debug_source_bind_get_value (stmt);
                  if (!VECTOR_TYPE_P (TREE_TYPE (var)))
                     mode = DECL_MODE (var);
                  else
                     mode = TYPE_MODE (TREE_TYPE (var));

                  val = gen_rtx_VAR_LOCATION (mode, var, (rtx)value,VAR_INIT_STATUS_UNINITIALIZED);
               }else
                  gcc_unreachable ();
            }
            /* If this function was first compiled with markers
            enabled, but they're now disable (e.g. LTO), drop
            them on the floor.  */
            else if (gimple_debug_nonbind_marker_p (stmt) && !MAY_HAVE_DEBUG_MARKER_INSNS)
               goto delink_debug_stmt;
            else if (gimple_debug_begin_stmt_p (stmt))
               val = GEN_RTX_DEBUG_MARKER_BEGIN_STMT_PAT ();
            else if (gimple_debug_inline_entry_p (stmt))
               val = GEN_RTX_DEBUG_MARKER_INLINE_ENTRY_PAT ();
            else
               gcc_unreachable ();

            last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
            mtcs_emit_set_curr_insn_location/*!set_curr_insn_location*/(mtcsEmit,gimple_location (stmt));
            mtcs_emit_emit_debug_insn/*!emit_debug_insn*/(mtcsEmit,val);
            if (dump_file && (dump_flags & TDF_DETAILS)){
               /* We can't dump the insn with a TREE where an RTX
               is expected.  */
               if (GET_CODE (val) == VAR_LOCATION){
                  gcc_checking_assert (PAT_VAR_LOCATION_LOC (val) == (rtx)value);
                  PAT_VAR_LOCATION_LOC (val) = const0_rtx;
               }
               maybe_dump_rtl_for_gimple_stmt (stmt, last);
               if (GET_CODE (val) == VAR_LOCATION)
                  PAT_VAR_LOCATION_LOC (val) = (rtx)value;
            }

delink_debug_stmt:
            /* In order not to generate too many debug temporaries,
            we delink all uses of debug statements we already expanded.
            Therefore debug statements between definition and real
            use of TERed SSA names will continue to use the SSA name,
            and not be replaced with debug temps.  */
            delink_stmt_imm_use (stmt);

            gsi = nsi;
            gsi_next (&nsi);
            if (gsi_end_p (nsi))
               break;
            stmt = gsi_stmt (nsi);
            if (!is_gimple_debug (stmt))
               break;
         }//end for(;;)

         mtcs_emit_set_curr_insn_location/*!set_curr_insn_location*/(mtcsEmit,sloc);
      }else{
         n_debug("mtcsexpand.c expand_gimple_basic_block 103 不是GIMPLE_COND也不是debug\n");
         gcall *call_stmt = dyn_cast <gcall *> (stmt);
         if (call_stmt  && asan_epilog_seq   && gimple_call_tail_p (call_stmt)  && !gimple_call_must_tail_p (call_stmt)){
            n_debug("mtcsexpand.c expand_gimple_basic_block 104 设 调用 tail \n");
            gimple_call_set_tail (call_stmt, false);
         }

         if (call_stmt && gimple_call_tail_p (call_stmt)){
            n_debug("mtcsexpand.c expand_gimple_basic_block 106 是最后一个call_stmt \n");
            bool can_fallthru;
            new_bb = expand_gimple_tailcall(self,bb, call_stmt, &can_fallthru,asan_epilog_seq);
            if (new_bb){
               n_debug("mtcsexpand.c expand_gimple_basic_block 107 new_bb can_fallthru:%d\n",can_fallthru);

               if (can_fallthru)
                  bb = new_bb;
               else{
                  currently_expanding_gimple_stmt = NULL;
                  return new_bb;
               }
            }
         }else{
            def_operand_p def_p;
            def_p = SINGLE_SSA_DEF_OPERAND (stmt, SSA_OP_DEF);
            n_debug("mtcsexpand.c expand_gimple_basic_block 108 def_p:%p\n",def_p);

            if (def_p != NULL){
               /* Ignore this stmt if it is in the list of
               replaceable expressions.  */
               if (SA.values  && bitmap_bit_p (SA.values, SSA_NAME_VERSION (DEF_FROM_PTR (def_p)))){
                  n_debug("mtcsexpand.c expand_gimple_basic_block 109 def_p:%p\n",def_p);
                  continue;
               }
            }
            last = expand_gimple_stmt(self,stmt);
            maybe_dump_rtl_for_gimple_stmt (stmt, last);
         }
      }
   }

   currently_expanding_gimple_stmt = NULL;
   n_debug("mtcsexpand.c expand_gimple_basic_block 110 结束语句expand\n");

   /* Expand implicit goto and convert goto_locus.  */
   FOR_EACH_EDGE (e, ei, bb->succs){
      n_debug("mtcsexpand.c expand_gimple_basic_block 111 FOR_EACH_EDGE (e, ei, bb->succs){ bb:%p %d %d %d %p %p\n",
      bb,(e->flags & EDGE_FALLTHRU),e->goto_locus != UNKNOWN_LOCATION,nondebug_stmt_seen,e->dest,bb->next_bb);

      if (e->goto_locus != UNKNOWN_LOCATION || !nondebug_stmt_seen){
         n_debug("mtcsexpand.c expand_gimple_basic_block 111aa FOR_EACH_EDGE (e, ei, bb->succs){ bb:%p %d\n",
               bb,(e->flags & EDGE_FALLTHRU));
         mtcs_emit_set_curr_insn_location/*!set_curr_insn_location*/(mtcsEmit,e->goto_locus);
      }
      if ((e->flags & EDGE_FALLTHRU) && e->dest != bb->next_bb){
         n_debug("mtcsexpand.c expand_gimple_basic_block 111bb FOR_EACH_EDGE (e, ei, bb->succs){ bb:%p %d\n",
         bb,(e->flags & EDGE_FALLTHRU));
         mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,label_rtx_for_bb(self,e->dest));
         e->flags &= ~EDGE_FALLTHRU;
      }
   }

   /* Expanded RTL can create a jump in the last instruction of block.
   This later might be assumed to be a jump to successor and break edge insertion.
   We need to insert dummy move to prevent this. PR41440. */
   if (single_succ_p (bb)
   && (single_succ_edge (bb)->flags & EDGE_FALLTHRU)
   && (last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData))
   && (JUMP_P (last)
   || (DEBUG_INSN_P (last)
   && JUMP_P (prev_nondebug_insn (last))))){
      n_debug("mtcsexpand.c expand_gimple_basic_block 112 single_succ_p (bb)... \n");
      rtx dummy = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mtcsMode->modes.M_SImode);
      mtcs_emit_emit_insn_after_noloc/*!emit_insn_after_noloc*/(mtcsEmit,
      mtcs_expr_gen_move_insn/*!gen_move_insn*/(mtcsExpr,dummy, dummy), last, NULL);
   }


   /* A __builtin_unreachable () will insert a barrier that should end
      the basic block.  In gimple, any code after it will have already
      deleted, even without optimization.  If we emit additional code
      here, as we would to adjust the stack after a call, it should be
      eventually deleted, but it confuses internal checkers (PR118006)
      and optimizers before it does, because we don't expect to find
      barriers inside basic blocks.  */
   if (!BARRIER_P (mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData))){
      n_debug("mtcsexpand.c expand_gimple_basic_block 113\n");

      mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
   }else{
      n_debug("mtcsexpand.c expand_gimple_basic_block 113xx\n");

      mtcs_dojump_discard_pending_stack_adjust/*!discard_pending_stack_adjust*/(mtcsDojump);
   }


   /* Find the block tail.  The last insn in the block is the insn
   before a barrier and/or table jump insn.  */
   last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
   n_debug("mtcsexpand.c expand_gimple_basic_block 114 last:%p\n",last);

   if (BARRIER_P (last)){
      last = PREV_INSN (last);
      n_debug("mtcsexpand.c expand_gimple_basic_block 115 BARRIER_P (last) last:%p\n",last);
   }
   if (JUMP_TABLE_DATA_P (last)){
      last = PREV_INSN (PREV_INSN (last));
      n_debug("mtcsexpand.c expand_gimple_basic_block 116 JUMP_TABLE_DATA_P (last) last:%p\n",last);
   }
   if (BARRIER_P (last)){
      last = PREV_INSN (last);
      n_debug("mtcsexpand.c expand_gimple_basic_block 117 现一次 BARRIER_P (last) last:%p\n",last);
   }
   BB_END (bb) = last;
   n_debug("mtcsexpand.c expand_gimple_basic_block 118 结束 BB_HEAD==BB_END :%d\n", BB_HEAD (bb)== BB_END (bb));
   mtcs_cfg_rtl_update_bb_for_insn/*!update_bb_for_insn*/(mtcsCfgRtl,bb);
   return bb;
}

/* For each lexical block, set BLOCK_NUMBER to the depth at which it is
   found in the block tree.  */
//原型 set_block_levels cfgexpand.cc
static void set_block_levels (tree block, int level)
{
  while (block)
    {
      BLOCK_NUMBER (block) = level;
      set_block_levels (BLOCK_SUBBLOCKS (block), level + 1);
      block = BLOCK_CHAIN (block);
    }
}

/* This function sets crtl->args.internal_arg_pointer to a virtual
   register if DRAP is needed.  Local register allocator will replace
   virtual_incoming_args_rtx with the virtual register.  */
//原型 expand_stack_alignment cfgexpand.cc
static void expand_stack_alignment (MtcsExpand *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
  MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  rtx drap_rtx;
  unsigned int preferred_stack_boundary;

  if (! mtcs_func_is_support_stack_alignment(mtcsFunc)/*SUPPORTS_STACK_ALIGNMENT*/)
    return;

  if (cfun->calls_alloca
      || cfun->has_nonlocal_label
      || mtcsRtlData/*!crtl*/->has_nonlocal_goto)
      mtcsRtlData/*!crtl*/->need_drap = true;

  /* Call update_stack_boundary here again to update incoming stack
     boundary.  It may set incoming stack alignment to a different
     value after RTL expansion.  TARGET_FUNCTION_OK_FOR_SIBCALL may
     use the minimum incoming stack alignment to check if it is OK
     to perform sibcall optimization since sibcall optimization will
     only align the outgoing stack to incoming stack boundary.  */
  if (mtcsMachine->calls->update_stack_boundary/*!targetm.calls.update_stack_boundary*/)
     target_calls_update_stack_boundary/*!targetm.calls.update_stack_boundary*/(mtcsMachine->calls);

  /* The incoming stack frame has to be aligned at least at
     parm_stack_boundary.  */
  int incomingStackBoundary=mtcs_func_get_incoming_stack_boundary(mtcsFunc);
  int preferredStackBoundary=mtcs_func_get_preferred_stack_boundary(mtcsFunc);

  gcc_assert (mtcsRtlData/*!crtl*/->parm_stack_boundary <= incomingStackBoundary/*!INCOMING_STACK_BOUNDARY*/);

  /* Update crtl->stack_alignment_estimated and use it later to align
     stack.  We check PREFERRED_STACK_BOUNDARY if there may be non-call
     exceptions since callgraph doesn't collect incoming stack alignment
     in this case.  */
  if (cfun->can_throw_non_call_exceptions
      && preferredStackBoundary/*!PREFERRED_STACK_BOUNDARY*/> mtcsRtlData/*!crtl*/->preferred_stack_boundary)
    preferred_stack_boundary = preferredStackBoundary/*!PREFERRED_STACK_BOUNDARY*/;
  else
    preferred_stack_boundary = mtcsRtlData/*!crtl*/->preferred_stack_boundary;
  if (preferred_stack_boundary > mtcsRtlData/*!crtl*/->stack_alignment_estimated)
     mtcsRtlData/*!crtl*/->stack_alignment_estimated = preferred_stack_boundary;
  if (preferred_stack_boundary > mtcsRtlData/*!crtl*/->stack_alignment_needed)
     mtcsRtlData/*!crtl*/->stack_alignment_needed = preferred_stack_boundary;

  gcc_assert (mtcsRtlData/*!crtl*/->stack_alignment_needed
          <= mtcsRtlData/*!crtl*/->stack_alignment_estimated);

  mtcsRtlData/*!crtl*/->stack_realign_needed = incomingStackBoundary/*!INCOMING_STACK_BOUNDARY*/ <
                                                   mtcsRtlData/*!crtl*/->stack_alignment_estimated;
  mtcsRtlData/*!crtl*/->stack_realign_tried = mtcsRtlData/*!crtl*/->stack_realign_needed;

  mtcsRtlData/*!crtl*/->stack_realign_processed = true;

  /* Target has to redefine TARGET_GET_DRAP_RTX to support stack
     alignment.  */
  gcc_assert (mtcsMachine->calls->get_drap_rtx /*!targetm.calls.get_drap_rtx*/!= NULL);
  drap_rtx = target_calls_get_drap_rtx/*!targetm.calls.get_drap_rtx*/(mtcsMachine->calls);

  /* stack_realign_drap and drap_rtx must match.  */
  gcc_assert ((mtcs_align_stack_realign_drap/*!stack_realign_drap*/(mtcsAlign)!= 0) == (drap_rtx != NULL));

  /* Do nothing if NULL is returned, which means DRAP is not needed.  */
  if (drap_rtx != NULL){
      mtcsRtlData/*!crtl*/->args.internal_arg_pointer = drap_rtx;
      /* Call fixup_tail_calls to clean up REG_EQUIV note if DRAP is
         needed. */
      mtcs_calls_fixup_tail_calls/*!fixup_tail_calls*/(mtcsCalls);
  }
}

/* Create a block containing landing pads and similar stuff.  */
//原型 construct_exit_block cfgexpand.cc
static void construct_exit_block (MtcsExpand *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
  MtcsCfg *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);

  rtx_insn *head = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
  rtx_insn *end;
  basic_block exit_block;
  edge e, e2;
  unsigned ix;
  edge_iterator ei;
  basic_block prev_bb = EXIT_BLOCK_PTR_FOR_FN (cfun)->prev_bb;
  rtx_insn *orig_end = BB_END (prev_bb);

  mtcs_func_rtl_profile_for_bb/*!rtl_profile_for_bb*/(mtcsFunc,EXIT_BLOCK_PTR_FOR_FN (cfun));

  /* Make sure the locus is set to the end of the function, so that
     epilogue line numbers and warnings are set properly.  */
  if (LOCATION_LOCUS (cfun->function_end_locus) != UNKNOWN_LOCATION)
    input_location = cfun->function_end_locus;

  struct sequence_stack *seq;
    for (seq =mtcs_rtl_data_get_current_sequence(mtcsRtlData); seq; seq = seq->next){
        n_debug("mtcsexpand.c construct_exit_block 00aa emit.req req:%p seq->first:%p seq->last:%p seq->next:%p\n",
              seq,seq->first,seq->last,seq->next);
    }

  /* Generate rtl for function exit.  */
  mtcs_func_expand_function_end/*!expand_function_end*/(mtcsFunc);

  end = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
  n_debug("mtcsexpand.c construct_exit_block 00 head==end 返回 %d prev_bb:%p orig_end:%p end:%p\n",head==end,prev_bb,orig_end,end);
  for (seq = mtcs_rtl_data_get_current_sequence(mtcsRtlData); seq; seq = seq->next){
      n_debug("mtcsexpand.c construct_exit_block 00bb emit.req req:%p seq->first:%p seq->last:%p seq->next:%p\n",
            seq,seq->first,seq->last,seq->next);
  }

  if (head == end)
    return;
  /* While emitting the function end we could move end of the last basic
     block.  */
  BB_END (prev_bb) = orig_end;
  while (NEXT_INSN (head) && NOTE_P (NEXT_INSN (head)))
    head = NEXT_INSN (head);
  /* But make sure exit_block starts with RETURN_LABEL, otherwise the
     bb count counting will be confused.  Any instructions before that
     label are emitted for the case where PREV_BB falls through into the
     exit block, so append those instructions to prev_bb in that case.  */
  if (NEXT_INSN (head) !=mtcsRtlData->x_return_label){
     n_debug("mtcsexpand.c construct_exit_block 11 NEXT_INSN (head) !=mtcsRtlData->x_return_label\n");
      while (NEXT_INSN (head) != mtcsRtlData->x_return_label){
          if (!NOTE_P (NEXT_INSN (head)))
            BB_END (prev_bb) = NEXT_INSN (head);
          head = NEXT_INSN (head);
      }
  }

  for (seq = mtcs_rtl_data_get_current_sequence(mtcsRtlData); seq; seq = seq->next){
        n_debug("mtcsexpand.c construct_exit_block 11aa emit.req req:%p seq->first:%p seq->last:%p seq->next:%p\n",
              seq,seq->first,seq->last,seq->next);
    }
  exit_block = mtcs_cfg_context_create_basic_block/*!create_basic_block*/(mtcsCfgContext,NEXT_INSN (head), end, prev_bb);
  for (seq = mtcs_rtl_data_get_current_sequence(mtcsRtlData); seq; seq = seq->next){
          n_debug("mtcsexpand.c construct_exit_block 22aa11 emit.req req:%p seq->first:%p seq->last:%p seq->next:%p\n",
                seq,seq->first,seq->last,seq->next);
      }
  exit_block->count = EXIT_BLOCK_PTR_FOR_FN (cfun)->count;
  for (seq = mtcs_rtl_data_get_current_sequence(mtcsRtlData); seq; seq = seq->next){
          n_debug("mtcsexpand.c construct_exit_block 22aa22 emit.req req:%p seq->first:%p seq->last:%p seq->next:%p\n",
                seq,seq->first,seq->last,seq->next);
      }
  add_bb_to_loop (exit_block, EXIT_BLOCK_PTR_FOR_FN (cfun)->loop_father);
  for (seq = mtcs_rtl_data_get_current_sequence(mtcsRtlData); seq; seq = seq->next){
        n_debug("mtcsexpand.c construct_exit_block 22aa emit.req req:%p seq->first:%p seq->last:%p seq->next:%p\n",
              seq,seq->first,seq->last,seq->next);
    }
  n_debug("mtcsexpand.c construct_exit_block 22 创建退出块:%p :end insn:%p prev_bb:%p\n",exit_block,end,prev_bb);

  ix = 0;
  while (ix < EDGE_COUNT (EXIT_BLOCK_PTR_FOR_FN (cfun)->preds)){
      e = EDGE_PRED (EXIT_BLOCK_PTR_FOR_FN (cfun), ix);
      basic_block dest = e->dest;
      n_debug("mtcsexpand.c construct_exit_block 33 ix:%d edge:%p EXIT_BB:%p exit_block:%p %d e->dest:%p dest->preds:%p %d\n",
             ix,e,EXIT_BLOCK_PTR_FOR_FN (cfun),exit_block,(e->flags & EDGE_ABNORMAL),dest,dest->preds,(e->flags & EDGE_ABNORMAL));
      if (!(e->flags & EDGE_ABNORMAL))
         mtcs_cfg_redirect_edge_succ/*!redirect_edge_succ*/(mtcsCfg,e, exit_block);
      else
          ix++;
  }
  for (seq = mtcs_rtl_data_get_current_sequence(mtcsRtlData); seq; seq = seq->next){
        n_debug("mtcsexpand.c construct_exit_block 33aa emit.req req:%p seq->first:%p seq->last:%p seq->next:%p\n",
              seq,seq->first,seq->last,seq->next);
    }
  e = mtcs_cfg_make_single_succ_edge/*!make_single_succ_edge*/(mtcsCfg,
        exit_block, EXIT_BLOCK_PTR_FOR_FN (cfun), EDGE_FALLTHRU);
  FOR_EACH_EDGE (e2, ei, EXIT_BLOCK_PTR_FOR_FN (cfun)->preds)
    if (e2 != e){
       n_debug("mtcsexpand.c construct_exit_block 44 ix:%d\n",ix);

        exit_block->count -= e2->count ();
    }
  mtcs_cfg_rtl_update_bb_for_insn/*!update_bb_for_insn*/(mtcsCfgRtl,exit_block);
}

/* A subroutine of expand_gimple_cond.  Given E, a fallthrough edge
   of a basic block where we just expanded the conditional at the end,
   possibly clean up the CFG and instruction sequence.  LAST is the
   last instruction before the just emitted jump sequence.  */
//原型 maybe_cleanup_end_of_block cfgexpand.cc
static void maybe_cleanup_end_of_block (MtcsExpand *self,edge e, rtx_insn *last)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
    MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
    MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
    MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
    MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);

  /* Special case: when jumpif decides that the condition is
     trivial it emits an unconditional jump (and the necessary
     barrier).  But we still have two edges, the fallthru one is
     wrong.  purge_dead_edges would clean this up later.  Unfortunately
     we have to insert insns (and split edges) before
     find_many_sub_basic_blocks and hence before purge_dead_edges.
     But splitting edges might create new blocks which depend on the
     fact that if there are two edges there's no barrier.  So the
     barrier would get lost and verify_flow_info would ICE.  Instead
     of auditing all edge splitters to care for the barrier (which
     normally isn't there in a cleaned CFG), fix it here.  */
  if (BARRIER_P (mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData))){
      rtx_insn *insn;
      mtcs_cfg_context_remove_edge/*!remove_edge*/(mtcsCfgContext,e);
      /* Now, we have a single successor block, if we have insns to
     insert on the remaining edge we potentially will insert
     it at the end of this block (if the dest block isn't feasible)
     in order to avoid splitting the edge.  This insertion will take
     place in front of the last jump.  But we might have emitted
     multiple jumps (conditional and one unconditional) to the
     same destination.  Inserting in front of the last one then
     is a problem.  See PR 40021.  We fix this by deleting all
     jumps except the last unconditional one.  */
      insn = PREV_INSN (mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData));
      /* Make sure we have an unconditional jump.  Otherwise we're
     confused.  */
      gcc_assert (JUMP_P (insn) && !any_condjump_p (insn));
      for (insn = PREV_INSN (insn); insn != last;){
          insn = PREV_INSN (insn);
          if (JUMP_P (NEXT_INSN (insn))){
              if (!any_condjump_p (NEXT_INSN (insn))){
                  gcc_assert (BARRIER_P (NEXT_INSN (NEXT_INSN (insn))));
                  mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,NEXT_INSN (NEXT_INSN (insn)));
              }
              mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,NEXT_INSN (insn));
          }
      }
  }
}

/* A subroutine of expand_gimple_basic_block.  Expand one GIMPLE_COND.
   Returns a new basic block if we've terminated the current basic
   block and created a new one.  */
//原型 expand_gimple_cond cfgexpand.cc
static basic_block expand_gimple_cond (MtcsExpand *self,basic_block bb, gcond *stmt)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
  MtcsCfg *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  basic_block new_bb, dest;
  edge true_edge;
  edge false_edge;
  rtx_insn *last2, *last;
  enum tree_code code;
  tree op0, op1;

  code = gimple_cond_code (stmt);
  op0 = gimple_cond_lhs (stmt);
  op1 = gimple_cond_rhs (stmt);
  /* We're sometimes presented with such code:
       D.123_1 = x < y;
       if (D.123_1 != 0)
         ...
     This would expand to two comparisons which then later might
     be cleaned up by combine.  But some pattern matchers like if-conversion
     work better when there's only one compare, so make up for this
     here as special exception if TER would have made the same change.  */
  if (SA.values
      && TREE_CODE (op0) == SSA_NAME
      && TREE_CODE (TREE_TYPE (op0)) == BOOLEAN_TYPE
      && TREE_CODE (op1) == INTEGER_CST
      && ((gimple_cond_code (stmt) == NE_EXPR
       && integer_zerop (op1))
      || (gimple_cond_code (stmt) == EQ_EXPR
          && integer_onep (op1)))
      && bitmap_bit_p (SA.values, SSA_NAME_VERSION (op0)))
  {
      gimple *second = SSA_NAME_DEF_STMT (op0);
      n_debug("mtcsexpand.c expand_gimple_cond 00\n");
      aet_print_gimple(second);
      if (gimple_code (second) == GIMPLE_ASSIGN){
          enum tree_code code2 = gimple_assign_rhs_code (second);
          if (TREE_CODE_CLASS (code2) == tcc_comparison){
              code = code2;
              op0 = gimple_assign_rhs1 (second);
              op1 = gimple_assign_rhs2 (second);
          }
          /* If jumps are cheap and the target does not support conditional
             compare, turn some more codes into jumpy sequences.  */
          else if (BRANCH_COST (optimize_insn_for_speed_p (), false) < 4
               && mtcsTarget->have_ccmp/*!targetm.have_ccmp*/(mtcsTarget)){
              if ((code2 == BIT_AND_EXPR
                 && TYPE_PRECISION (TREE_TYPE (op0)) == 1
                 && TREE_CODE (gimple_assign_rhs2 (second)) != INTEGER_CST)
                 || code2 == TRUTH_AND_EXPR){
                  code = TRUTH_ANDIF_EXPR;
                  op0 = gimple_assign_rhs1 (second);
                  op1 = gimple_assign_rhs2 (second);
              }else if (code2 == BIT_IOR_EXPR || code2 == TRUTH_OR_EXPR){
                  code = TRUTH_ORIF_EXPR;
                  op0 = gimple_assign_rhs1 (second);
                  op1 = gimple_assign_rhs2 (second);
              }
          }
      }
  }

  /* Optimize (x % C1) == C2 or (x % C1) != C2 if it is beneficial
     into (x - C2) * C3 < C4.  */
  if ((code == EQ_EXPR || code == NE_EXPR)
      && TREE_CODE (op0) == SSA_NAME
      && TREE_CODE (op1) == INTEGER_CST){
    code = maybe_optimize_mod_cmp (code, &op0, &op1);
    n_debug("mtcsexpand.c expand_gimple_cond 11 code:%d\n",code);
  }

  /* Optimize (x - y) < 0 into x < y if x - y has undefined overflow.  */
  if (!TYPE_UNSIGNED (TREE_TYPE (op0))
      && (code == LT_EXPR || code == LE_EXPR
      || code == GT_EXPR || code == GE_EXPR)
      && integer_zerop (op1)
      && TREE_CODE (op0) == SSA_NAME){
     n_debug("mtcsexpand.c expand_gimple_cond 22 code:%d\n",code);
     maybe_optimize_sub_cmp_0 (code, &op0, &op1);
  }
  last2 = last =mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);

  extract_true_false_edges_from_block (bb, &true_edge, &false_edge);
  mtcs_emit_set_curr_insn_location/*!set_curr_insn_location*/(mtcsEmit,gimple_location (stmt));
  /* These flags have no purpose in RTL land.  */
  true_edge->flags &= ~EDGE_TRUE_VALUE;
  false_edge->flags &= ~EDGE_FALSE_VALUE;
  /* We can either have a pure conditional jump with one fallthru edge or
     two-way jump that needs to be decomposed into two basic blocks.  */
  if (false_edge->dest == bb->next_bb){
     n_debug("mtcsexpand.c expand_gimple_cond 33 alse_edge->dest == bb->next_bb\n");

      mtcs_dojump_jumpif_1/*!jumpif_1*/(mtcsDojump,code, op0, op1,
              label_rtx_for_bb(self,true_edge->dest), true_edge->probability);
      maybe_dump_rtl_for_gimple_stmt (stmt, last);
      if (true_edge->goto_locus != UNKNOWN_LOCATION)
          mtcs_emit_set_curr_insn_location/*!set_curr_insn_location*/(mtcsEmit,true_edge->goto_locus);
      false_edge->flags |= EDGE_FALLTHRU;
      maybe_cleanup_end_of_block(self,false_edge, last);
      return NULL;
  }
  if (true_edge->dest == bb->next_bb){
      n_debug("mtcsexpand.c expand_gimple_cond 44 true_edge->dest == bb->next_bb\n");
      aet_print_gimple(stmt);
      mtcs_dojump_jumpifnot_1/*!jumpifnot_1*/(mtcsDojump,code, op0, op1,
              label_rtx_for_bb(self,false_edge->dest),false_edge->probability);
      maybe_dump_rtl_for_gimple_stmt (stmt, last);
      if (false_edge->goto_locus != UNKNOWN_LOCATION)
          mtcs_emit_set_curr_insn_location/*!set_curr_insn_location*/(mtcsEmit,false_edge->goto_locus);
      true_edge->flags |= EDGE_FALLTHRU;
      maybe_cleanup_end_of_block(self,true_edge, last);
      return NULL;
  }

  mtcs_dojump_jumpif_1/*!jumpif_1*/(mtcsDojump,code, op0, op1,
          label_rtx_for_bb(self,true_edge->dest),true_edge->probability);
  last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
  if (false_edge->goto_locus != UNKNOWN_LOCATION)
      mtcs_emit_set_curr_insn_location/*!set_curr_insn_location*/(mtcsEmit,false_edge->goto_locus);
  mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,label_rtx_for_bb(self,false_edge->dest));

  BB_END (bb) = last;
  if (BARRIER_P (BB_END (bb)))
    BB_END (bb) = PREV_INSN (BB_END (bb));
  mtcs_cfg_rtl_update_bb_for_insn/*!update_bb_for_insn*/(mtcsCfgRtl,bb);

  new_bb = mtcs_cfg_context_create_basic_block/*!create_basic_block*/(mtcsCfgContext,
        NEXT_INSN (last), mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData), bb);
  dest = false_edge->dest;
  mtcs_cfg_redirect_edge_succ/*!redirect_edge_succ*/(mtcsCfg,false_edge, new_bb);
  false_edge->flags |= EDGE_FALLTHRU;
  new_bb->count = false_edge->count ();
  loop_p loop = find_common_loop (bb->loop_father, dest->loop_father);
  add_bb_to_loop (new_bb, loop);
  if (loop->latch == bb && loop->header == dest)
    loop->latch = new_bb;
  mtcs_cfg_make_single_succ_edge/*!make_single_succ_edge*/(mtcsCfg,new_bb, dest, 0);
  if (BARRIER_P (BB_END (new_bb)))
    BB_END (new_bb) = PREV_INSN (BB_END (new_bb));
  mtcs_cfg_rtl_update_bb_for_insn/*!update_bb_for_insn*/(mtcsCfgRtl,new_bb);

  maybe_dump_rtl_for_gimple_stmt (stmt, last2);
  n_debug("mtcsexpand.c expand_gimple_cond 55 \n");

  if (true_edge->goto_locus != UNKNOWN_LOCATION){
      mtcs_emit_set_curr_insn_location/*!set_curr_insn_location*/(mtcsEmit,true_edge->goto_locus);
      true_edge->goto_locus = curr_insn_location ();
  }

  return new_bb;
}


static void printBBGimple()
{
    struct function *nodeFun=cfun;
    edge e;
    edge_iterator ei;
    basic_block bb;

    gimple_stmt_iterator gsi;
    FOR_EACH_BB_FN (bb, nodeFun){
         for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
            gimple *stmt = gsi_stmt (gsi);
            n_debug("mtcsexpand.c printBBGimple 00 bb:%p stmt:%p\n",bb,stmt);
            aet_print_gimple(stmt);
         }

        FOR_EACH_EDGE (e, ei, bb->succs)
         n_debug("mtcsexpand.c printBBGimple 边缘  :%p bb:%p EDGE_FALLTHRU:%d flags:%d e->dest:%p bb->next_bb:%p EXIT_BB:%p\n",
               e,bb ,(e->flags & EDGE_FALLTHRU),e->flags,e->dest,bb->next_bb,EXIT_BLOCK_PTR_FOR_FN (cfun),bb->loop_father);
        if(bb->loop_father){
           n_debug("mtcsexpand.c printBBGimple 00 :bb :%p loop_father:%p loopheader:%p\n",bb,bb->loop_father,bb->loop_father->header);
        }
    }
    int ix=0;
    while (ix < EDGE_COUNT (EXIT_BLOCK_PTR_FOR_FN (cfun)->preds)){
         e = EDGE_PRED (EXIT_BLOCK_PTR_FOR_FN (cfun), ix);
         basic_block dest = e->dest;
         n_debug("mtcsexpand.c printBBGimple 函数退出块中的 preds ix:%d edge:%p EXIT_BB %p e->dest:%p dest->preds:%p\n",
               ix,e,EXIT_BLOCK_PTR_FOR_FN (cfun),dest,dest->preds);
         ix++;
    }

}

//移走最后的返回语句，并加入新的
static void addLastStmt(MtcsExpand *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsCfg *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);

   struct function *nodeFun=cfun;
   enum ir_type type=mtcs_cfg_context_get_state/*!current_ir_type ()*/(mtcsCfgContext);
   n_debug("mtcsexpand.c addLastStmt 00 当前 hooks类型:%d fn->x_current_loops:%p\n",
         type,nodeFun->x_current_loops,number_of_loops(nodeFun));
   mtcs_tool_print_cfun_loop();
   printBBGimple();
   basic_block bb;
   gimple_stmt_iterator gsi;

   FOR_EACH_BB_FN (bb, nodeFun){
      bool find=false;
      for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
         gimple *stmt = gsi_stmt (gsi);

         if(gimple_code (stmt)==GIMPLE_CALL && gimple_call_tail_p (stmt)){
            n_debug("mtcsexpand.c addLastStmt 11 移走函数调用并且是tail语句 所在 bb:%p\n",bb);
            aet_print_gimple(stmt);

            //找到尾部调用 查看下条stmt是不是return NULL;
             gsi_next (&gsi);
             gimple *returnStmt = gsi_stmt (gsi);
             if(returnStmt){
               if(gimple_code (returnStmt)==GIMPLE_RETURN){
                  n_debug("mtcsexpand.c addLastStmt 22 移走函数调用并且是tail语句 找到了 return语句 所在 bb:%p\n",bb);
                  aet_print_gimple(returnStmt);

                  gsi_remove (&gsi, false);
                  //创建一个BB
                  gcc_checking_assert (!(bb->flags & BB_RTL));
                 // gimple_seq seq=  bb->il.gimple.seq;
                  gimple_seq new_seq = NULL;
                  gimple_seq_add_stmt (&new_seq, returnStmt);
                  n_debug("mtcsexpand.c addLastStmt 33 加入原返回语句到新的giple_seq bb:%p ENTRY:%p count preds:%d\n",
                        bb,ENTRY_BLOCK_PTR_FOR_FN(cfun),EDGE_COUNT (EXIT_BLOCK_PTR_FOR_FN (cfun)->preds));
                  basic_block tailbb = mtcs_cfg_context_create_basic_block/*!create_basic_block*/(mtcsCfgContext,new_seq, bb);
                  n_debug("mtcsexpand.c addLastStmt 44 创建新的tailbb后 count preds:%d tailbb:%p tailbb->preds:%d\n",
                                     EDGE_COUNT (EXIT_BLOCK_PTR_FOR_FN (cfun)->preds),tailbb,tailbb->preds);

                  edge res = mtcs_cfg_make_single_succ_edge/*!make_single_succ_edge*/(mtcsCfg,
                        tailbb,  EXIT_BLOCK_PTR_FOR_FN (cfun), 0);
                  n_debug("mtcsexpand.c addLastStmt 55 tailbb->succs:%d exitbb->preds:%d tailbb->preds:%d taibb:%p new edge:%p\n",
                        EDGE_COUNT(tailbb->succs),EDGE_COUNT (EXIT_BLOCK_PTR_FOR_FN (cfun)->preds),
                        tailbb->preds,tailbb,res);

                  edge old=EDGE_SUCC(bb,0);
                  mtcs_cfg_redirect_edge_succ/*!redirect_edge_succ*/(mtcsCfg,old,tailbb);
                  old->flags|=EDGE_FALLTHRU;
                  find=true;

                  class loop *loop = alloc_loop ();
                  loop->header = ENTRY_BLOCK_PTR_FOR_FN(cfun);
                  add_loop (loop, bb->loop_father);
                  tailbb->loop_father = bb->loop_father;

                  //add_bb_to_loop (tailbb, EXIT_BLOCK_PTR_FOR_FN (cfun)->loop_father);

                  break;

               }
             }
         }
      }
      if(find)
         break;
   }
   n_debug("mtcsexpand.c 加入尾部语句后块中的 gimple\n");
   mtcs_tool_print_cfun_loop();
   printBBGimple();
}


/* Walk the instruction chain and verify that bb head/end pointers
  are correct, and that instructions are in exactly one bb and have
  correct block pointers.  */
//检查生成的insn
static bool rtl_verify_bb_insn_chain (MtcsExpand *self)
{
   if(5>3)
      return false;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   basic_block bb;
   bool err = false;
   rtx_insn *x;
   rtx_insn *last_head = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
   basic_block *bb_info;
   const int max_uid = mtcs_rtl_data_get_max_uid/*!get_max_uid*/(mtcsRtlData);

   bb_info = XCNEWVEC (basic_block, max_uid);

   FOR_EACH_BB_REVERSE_FN (bb, cfun){
      rtx_insn *head = BB_HEAD (bb);
      rtx_insn *end = BB_END (bb);

      for (x = last_head; x != NULL_RTX; x = PREV_INSN (x)){
         n_debug("mtcsexpand.cc rtl_verify_bb_insn_chain 00 bb:%p x:%d %p x:%p end:%p\n",bb,BARRIER_P (x),BLOCK_FOR_INSN (x),x,end);
         mtcs_print_rtl_single(stderr,x);


         /* Verify the end of the basic block is in the INSN chain.  */
         if (x == end)
            break;

         /* And that the code outside of basic blocks has NULL bb field.  */
         if (!BARRIER_P (x)   && BLOCK_FOR_INSN (x) != NULL){
            error ("mtcsexpand.cc rtl_verify_bb_insn_chain 11 出错 bb:%p insn %d outside of basic blocks has non-NULL bb field",
                  bb,INSN_UID (x));
            err = true;
         }
      }

      if (!x){
         error ("mtcsexpand.cc rtl_verify_bb_insn_chain 22 出错 bb:%pend insn %d for block %d not found in the insn stream",
               bb,INSN_UID (end), bb->index);
         err = true;
      }

      /* Work backwards from the end to the head of the basic block
      to verify the head is in the RTL chain.  */
      for (; x != NULL_RTX; x = PREV_INSN (x)){
         /* While walking over the insn chain, verify insns appear
         in only one basic block.  */
         if (bb_info[INSN_UID (x)] != NULL){
            n_debug("mtcsexpand.cc rtl_verify_bb_insn_chain 33aa---- bb:%p\n",bb);
            mtcs_print_rtl_single(stderr,x);
            error ("mtcsexpand.cc rtl_verify_bb_insn_chain 33 出错 bb:%p insn %d is in multiple basic blocks (%d and %d)",
                  bb,INSN_UID (x), bb->index, bb_info[INSN_UID (x)]->index);
            err = true;
         }

         n_debug("mtcsexpand.cc rtl_verify_bb_insn_chain 44 给bbinfo赋值 ---- bb:%p uid:%d x:%p head:%p\n",bb,INSN_UID (x),x,head);
         mtcs_print_rtl_single(stderr,x);
         mtcs_print_rtl_single(stderr,head);

         bb_info[INSN_UID (x)] = bb;

         if (x == head)
            break;
      }
      if (!x){
         error ("mtcsexpand.cc rtl_verify_bb_insn_chain 55 出错 bb:%p head insn %d for block %d not found in the insn stream",
               bb,INSN_UID (head), bb->index);
         err = true;
      }

      last_head = PREV_INSN (x);
   }

   for (x = last_head; x != NULL_RTX; x = PREV_INSN (x)){
      /* Check that the code before the first basic block has NULL
      bb field.  */
      if (!BARRIER_P (x) && BLOCK_FOR_INSN (x) != NULL){
         error ("mtcsexpand.cc rtl_verify_bb_insn_chain 66 出错 bb:%p insn %d outside of basic blocks has non-NULL bb field",
               bb,INSN_UID (x));
         err = true;
      }
   }
   free (bb_info);

   return err;
}

static void test_rtl_verify_edges ()
{
  basic_block bb;

  FOR_EACH_BB_REVERSE_FN (bb, cfun){
      int  n_branch = 0;
      int n_eh = 0, n_abnormal = 0;
      edge e, fallthru = NULL;
      edge_iterator ei;


      if(bb->index==13 || bb->index==14 || bb->index==15 || bb->index==16 || bb->index==17) {
         FOR_EACH_EDGE (e, ei, bb->succs){
             if ((e->flags & ~(EDGE_DFS_BACK
                       | EDGE_CAN_FALLTHRU
                       | EDGE_IRREDUCIBLE_LOOP
                       | EDGE_LOOP_EXIT
                       | EDGE_CROSSING
                       | EDGE_PRESERVE)) == 0)
               n_branch++;
         }
        // bool re= BB_END (bb)?any_uncondjump_p (BB_END (bb)):false;
        // fprintf(stderr,"xxx rtl_verify_edges 00 bb:%p index:%d n_branch:%d BB_END (bb):%p any_uncondjump_p:%d\n",
              // bb,bb->index,n_branch,BB_END (bb),re);
      }
     // if(bb->index==14)
       //  break;
//      if (n_branch != 1 && any_uncondjump_p (BB_END (bb))){
//         fprintf(stderr,"rtl_verify_edges 11 bb:%p index:%d n_branch:%d\n", bb,bb->index,n_branch);
//      }
  }
}

static void testprint(struct function *fn)
{
   return;
   if(!fn)
      return;
   basic_block bb;
   test_rtl_verify_edges();
}

static void printbb(basic_block block)
{
   rtx_insn *insn;
   int i=0;
   FOR_BB_INSNS (block, insn){
      if (!INSN_P (insn))
         continue;
      n_debug("mtcsexpand.c 打印块中的指令 i:%d block:%p index:%d flags:%d insn:%p\n",i++,block,block->index,block->flags,insn);
      mtcs_print_rtl_single(stderr,insn);
   }
}


//打印块中的gimple
static void printbbgimple()
{
   if(!n_log_is_debug_file(NULL,NULL))
      return;


     basic_block bb;
      FOR_EACH_BB_FN (bb, cfun){
         n_debug("mtcsexpand.c 打印 块中的gimple 00 bb:%p\n",bb);
         gimple_seq stmts;
          aet_print_seq(stmts);
      }

}


//原型 unsigned int pass_expand::execute (function *fun) cfgexpand.cc
nuint mtcs_expand_execute(MtcsExpand *self,function *fun)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsExcept   *mtcsExcept=mtcs_target_get_except(mtcsTarget);
   MtcsCfgBuild   *mtcsCfgBuild=mtcs_target_get_cfg_build(mtcsTarget);
   MtcsCfgCleanup *mtcsCfgCleanup=mtcs_target_get_cfg_cleanup(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsOutofSsa *mtcsOutofSsa = mtcs_target_get_outof_ssa(mtcsTarget);

   basic_block bb, init_block;
   edge_iterator ei;
   edge e;
   rtx_insn *var_seq, *var_ret_seq;
   unsigned i;
   struct sequence_stack *seq;

   mtcs_outof_ssa_rewrite_out_of_ssa/*!rewrite_out_of_ssa*/(mtcsOutofSsa,&SA);//tree-outof-ssa.h 在cfgexpand.cc 中定义
   SA.partition_to_pseudo = XCNEWVEC (rtx, SA.map->num_partitions);
   n_debug("mtcsexpand.c  mtcs_expand_execute 00 cfun->curr_properties:%d SA.map-num_partitions:%d cfun:%p\n",
         cfun->curr_properties,SA.map->num_partitions,cfun);
   testprint(cfun);
   if (MAY_HAVE_DEBUG_BIND_STMTS && mtcsOptionsItem->x_flag_tree_ter){
      gimple_stmt_iterator gsi;
      FOR_EACH_BB_FN (bb, cfun)
         for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
            if (gimple_debug_bind_p (gsi_stmt (gsi)))
               avoid_deep_ter_for_debug(self,gsi_stmt (gsi), 0);
   }
   /* Mark arrays indexed with non-constant indices with TREE_ADDRESSABLE.  */
   auto_bitmap forced_stack_vars;
   discover_nonconstant_array_refs(self,forced_stack_vars);
   /* Make sure all values used by the optimization passes have sane
   defaults.  */
   //原型 extern short *reg_renumber; 还未处理
   reg_renumber = 0;
   /* Some backends want to know that we are expanding to RTL.  */
   self->currently_expanding_to_rtl = 1;
   /* Dominators are not kept up-to-date as we may create new basic-blocks.  */
   n_debug("mtcsexpand.c  mtcs_expand_execute 11ddd cfun:%p\n",cfun);
   free_dominance_info (CDI_DOMINATORS);
   mtcs_func_rtl_profile_for_bb/*!rtl_profile_for_bb*/(mtcsFunc,ENTRY_BLOCK_PTR_FOR_FN (fun));
   mtcs_emit_insn_locations_init/*insn_locations_init*/(mtcsEmit);
   if (!DECL_IS_UNDECLARED_BUILTIN (current_function_decl)){
      /* Eventually, all FEs should explicitly set function_start_locus.  */
      if (LOCATION_LOCUS (fun->function_start_locus) == UNKNOWN_LOCATION){
         n_debug("mtcsexpand.c  mtcs_expand_execute 11 LOCATION_LOCUS (fun->function_start_locus) == UNKNOWN_LOCATION\n");
         mtcs_emit_set_curr_insn_location/*!set_curr_insn_location*/(mtcsEmit,DECL_SOURCE_LOCATION (current_function_decl));
      }else{
         n_debug("mtcsexpand.c  mtcs_expand_execute 22\n");
         mtcs_emit_set_curr_insn_location/*!set_curr_insn_location*/(mtcsEmit,fun->function_start_locus);
      }
   }else{
      n_debug("mtcsexpand.c  mtcs_expand_execute 33\n");
      mtcs_emit_set_curr_insn_location/*!set_curr_insn_location*/(mtcsEmit,UNKNOWN_LOCATION);
   }
   mtcs_emit_set_prologue_location(mtcsEmit,
         mtcs_emit_curr_insn_location/*!curr_insn_location*/(mtcsEmit));/*!prologue_location = curr_insn_location ();*/
   //#ifdef INSN_SCHEDULING //host=1 nvptx=0 好像只有host才定义？
   //  init_sched_attrs ();
   //#endif

   /* Make sure first insn is a note even if we don't want linenums.
   This makes sure the first insn will never be deleted.
   Also, final expects a note to appear there.  */
   /* 在全局的rtx_insn指令序列中(crtl->emit.seq) 先发射一条 (note NOTE_INSN_DELETED)指令,每个函数的开始都要发射此指令 */
   mtcs_emit_emit_note(mtcsEmit,NOTE_INSN_DELETED);//第一次加入insn /*emit_note (NOTE_INSN_DELETED);*/
   for (seq =mtcs_rtl_data_get_current_sequence(mtcsRtlData); seq; seq = seq->next){
      n_debug("mtcsexpand.c exapnd 33aa emit.req req:%p seq->first:%p seq->last:%p seq->next:%p\n",
      seq,seq->first,seq->last,seq->next);
   }

   mtcsTarget->expand_to_rtl_hook(mtcsTarget);/*!targetm.expand_to_rtl_hook ();*/
   mtcs_rtl_data_init_stack_alignment (mtcsFunc->mtcsRtlData); /*!crtl->init_stack_alignment ();*/
   fun->cfg->max_jumptable_ents = 0;
   /* Resovle the function section.  Some targets, like ARM EABI rely on knowledge
   of the function section at exapnsion time to predict distance of calls.  */
   mtcs_asm_resolve_unique_section/*!resolve_unique_section*/(mtcsAsm,
         current_function_decl, 0, mtcsOptionsItem->x_flag_function_sections);
   /* Expand the variables recorded during gimple lowering.  */
   /* 在ctrl->emit.seq中新开一个指令序列, 记录expand_used_vars过程中产生的新的指令序列(原有指令序列类似栈指针被保存起来) */
   mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
   var_ret_seq = expand_used_vars(self,forced_stack_vars);
   var_seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   /* 获取当前指令序列中所有指令,实际上就是expand_used_vars发射的需要在函数体之前执行的指令(大多数情况下 var_ret_seq/var_seq都为空) */
   /* 关闭当前指令序列,同时恢复之前的指令序列[start_sequence, end_sequence]结束后,二者之间发射的指令序列无法通过get_insns获取,
   * 必须在end_sequence之前显示保存,如这里的 var_seq*/
   mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

   /* Honor stack protection warnings.  */
   if (mtcsOptionsItem->x_warn_stack_protect){
      n_debug("mtcsexpand.c  mtcs_expand_execute 44 fun->calls_alloca:%d\n",fun->calls_alloca);
      if (fun->calls_alloca)
         warning (OPT_Wstack_protector,"stack protector not protecting local variables: variable length buffer");
      if (self->has_short_buffer && !mtcsRtlData/*!crtl*/->stack_protect_guard)
         warning (OPT_Wstack_protector, "stack protector not protecting function: all local arrays are less than %d bytes long",
         (int) mtcsOptionsItem->x_param_ssp_buffer_size);
   }

   /* Temporarily mark PARM_DECLs and RESULT_DECLs we need to expand to
   memory addressable so expand_function_start can emit the required
   copies.  */
   auto_vec<tree, 16> marked_parms;
   for (tree parm = DECL_ARGUMENTS (current_function_decl); parm; parm = DECL_CHAIN (parm))
      if (!TREE_ADDRESSABLE (parm)  && bitmap_bit_p (forced_stack_vars, DECL_UID (parm))){
         n_debug("mtcsexpand.c  mtcs_expand_execute 55 parm:%p\n",parm);
         TREE_ADDRESSABLE (parm) = 1;
         marked_parms.safe_push (parm);
      }

   if (DECL_RESULT (current_function_decl)
   && !TREE_ADDRESSABLE (DECL_RESULT (current_function_decl))
   && bitmap_bit_p (forced_stack_vars, DECL_UID (DECL_RESULT (current_function_decl)))){
      n_debug("mtcsexpand.c  mtcs_expand_execute 66 \n");
      TREE_ADDRESSABLE (DECL_RESULT (current_function_decl)) = 1;
      marked_parms.safe_push (DECL_RESULT (current_function_decl));
   }

   n_debug("mtcsexpand.c  mtcs_expand_execute 77 expand_function_start\n");
   for (seq =mtcs_rtl_data_get_current_sequence(mtcsRtlData); seq; seq = seq->next){
      n_debug("mtcsexpand.c exapnd 77aa emit.req req:%p seq->first:%p seq->last:%p seq->next:%p\n",
      seq,seq->first,seq->last,seq->next);
   }

   /* Set up parameters and prepare for return, for the function.  */
   /*
   此函数根据AAPCS64标准确定:
   * 函数返回值的存储位置(通常是用rtx(REG)代表的R0硬件寄存器)
   * 函数的各个传入参数来自哪个硬件寄存器(R0~R7)或栈内存(也都是通过rtx(REG)表示)
   同时在函数栈/伪寄存器中为传入参数预留空间,并(在函数体指令序列之前)发射指令将传入参数复制到对应的函数栈/伪寄存器中
   最后此函数发射(note NOTE_INSN_FUNCTION_BEG)指令,代表后续指令属于函数体;若编译器指定了如-pg等参数,则在此后还会发射对_mcount函数的调用指令
   */
   mtcs_func_expand_function_start/*!expand_function_start*/(mtcsFunc,current_function_decl);

   /* Clear TREE_ADDRESSABLE again.  */
   while (!marked_parms.is_empty ()){
      n_debug("mtcsexpand.c  mtcs_expand_execute 88  while (!marked_parms.is_empty ())\n");
      TREE_ADDRESSABLE (marked_parms.pop ()) = 0;
   }
   n_debug("mtcsexpand.c  mtcs_expand_execute 88aa expand_function_start\n");

   /* If we emitted any instructions for setting up the variables,
   emit them before the FUNCTION_START note.  */
   if (var_seq){
      n_debug("mtcsexpand.c  mtcs_expand_execute 99 var_seq\n");
      mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,var_seq, parm_birth_insn);
      /* In expand_function_end we'll insert the alloca save/restore
      before parm_birth_insn.  We've just insertted an alloca call.
      Adjust the pointer to match.  */
      parm_birth_insn = var_seq;
   }
   /* Now propagate the RTL assignment of each partition to the
   underlying var of each SSA_NAME.  */
   tree name;
   FOR_EACH_SSA_NAME (i, name, cfun){
      /* We might have generated new SSA names in
      update_alias_info_with_stack_vars.  They will have a NULL
      defining statements, and won't be part of the partitioning,
      so ignore those.  */
      n_debug("mtcsexpand.c  mtcs_expand_execute 100 SSA_NAME_DEF_STMT (name):%p\n",SSA_NAME_DEF_STMT (name));
      if (!SSA_NAME_DEF_STMT (name))
         continue;
      adjust_one_expanded_partition_var(self,name);
   }
   /* Clean up RTL of variables that straddle across multiple
   partitions, and check that the rtl of any PARM_DECLs that are not
   cleaned up is that of their default defs.  */
   FOR_EACH_SSA_NAME (i, name, cfun){
      int part;
      /* We might have generated new SSA names in
      update_alias_info_with_stack_vars.  They will have a NULL
      defining statements, and won't be part of the partitioning,
      so ignore those.  */
      n_debug("mtcsexpand.c  mtcs_expand_execute 101 SSA_NAME_DEF_STMT (name):%p\n",SSA_NAME_DEF_STMT (name));
      if (!SSA_NAME_DEF_STMT (name))
         continue;
      part = var_to_partition (SA.map, name);
      if (part == NO_PARTITION)
         continue;
      n_debug("mtcsexpand.c  mtcs_expand_execute 102 part:%d\n",part);
      /* If this decl was marked as living in multiple places, reset
      this now to NULL.  */
      tree var = SSA_NAME_VAR (name);
      if (var && DECL_RTL_IF_SET (var) == pc_rtx)
         mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,var, NULL);
      /* Check that the pseudos chosen by assign_parms are those of
      the corresponding default defs.  */
      else if (SSA_NAME_IS_DEFAULT_DEF (name)  && (TREE_CODE (var) == PARM_DECL || TREE_CODE (var) == RESULT_DECL)){
         n_debug("mtcsexpand.c  mtcs_expand_execute 103 part:%d\n",part);
         rtx in = DECL_RTL_IF_SET (var);
         gcc_assert (in);
         rtx out = SA.partition_to_pseudo[part];
         gcc_assert (in == out);

         /* Now reset VAR's RTL to IN, so that the _EXPR attrs match
         those expected by debug backends for each parm and for
         the result.  This is particularly important for stabs,
         whose register elimination from parm's DECL_RTL may cause
         -fcompare-debug differences as SET_DECL_RTL changes reg's
         attrs.  So, make sure the RTL already has the parm as the
         EXPR, so that it won't change.  */
         mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,var, NULL_RTX);
         if (MEM_P (in))
            mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,in, var, true);
         mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,var, in);
      }
   }

   /* If this function is `main', emit a call to `__main'
   to run global initializers, etc.  */
   if (DECL_NAME (current_function_decl)
   && MAIN_NAME_P (DECL_NAME (current_function_decl))
   && DECL_FILE_SCOPE_P (current_function_decl))
      expand_main_function (self);

   /* Initialize the stack_protect_guard field.  This must happen after the
   call to __main (if any) so that the external decl is initialized.  */
   if (mtcsRtlData/*!crtl*/->stack_protect_guard
   && mtcsTarget->stack_protect_runtime_enabled_p/*!targetm.stack_protect_runtime_enabled_p*/(mtcsTarget)){
      n_debug("mtcsexpand.c  mtcs_expand_execute 104 mtcsRtlData/*!crtl*/->stack_protect_guard:%p\n",
      mtcsRtlData/*!crtl*/->stack_protect_guard);
      stack_protect_prologue(self);
   }

   /*切换到cfg gimple状态*/
   mtcs_cfg_context_change_gimple_state/*rtl_register_cfg_hooks*/(mtcsCfgContext);
   addLastStmt(self);
   mtcs_outof_ssa_expand_phi_nodes/*!expand_phi_nodes*/(mtcsOutofSsa,&SA);
   /* Release any stale SSA redirection data.  */
   redirect_edge_var_map_empty ();
   /* 重要 改变状态到rtl Register rtl specific functions for cfg.  */
   mtcs_cfg_context_change_rtl_state/*rtl_register_cfg_hooks*/(mtcsCfgContext);
   init_block = construct_init_block(self);
   n_debug("mtcsexpand.c  mtcs_expand_execute 105 初始块创建完成了 %p\n",init_block);
   printbb(init_block);
   printbbgimple();

   /* Clear EDGE_EXECUTABLE on the entry edge(s).  It is cleaned from the
   remaining edges later.  */
   FOR_EACH_EDGE (e, ei, ENTRY_BLOCK_PTR_FOR_FN (fun)->succs){
      n_debug("mtcsexpand.c  mtcs_expand_execute 106 开始块的输出边设flag-- e->flags:%d\n",e->flags);
      e->flags &= ~EDGE_EXECUTABLE;
   }

   /* If the function has too many markers, drop them while expanding.  */
   if (cfun->debug_marker_count >= mtcsOptionsItem->x_param_max_debug_marker_count)
      cfun->debug_nonbind_markers = false;
   /* rtx_code_label是rtx格式的标签位置表达式，在某指令expand过程中若引用到了尚未expand的bb,
   * 则会先为目标bb生成rtx(CODE_LABEL)并保存在此hash表中(若需要) */
   self->lab_rtx_for_bb = new hash_map<basic_block, rtx_code_label *>;

   FOR_BB_BETWEEN (bb, init_block->next_bb, EXIT_BLOCK_PTR_FOR_FN (fun),next_bb){
      n_debug("mtcsexpand.c  mtcs_expand_execute 107 开始expand块中的gimple bb:%p init_block:%p\n",bb,init_block);
      bb = expand_gimple_basic_block(self,bb, var_ret_seq);
      n_debug("mtcsexpand.c  mtcs_expand_execute 108 结束expand块中的gimple bb:%p init_block:%p\n",bb,init_block);
      printbb(bb);
   }

   n_debug("mtcsexpand.c mtcs_expand_execute 109 测试rtl_verify_bb_insn_chain \n");
   rtl_verify_bb_insn_chain(self);

   if (MAY_HAVE_DEBUG_BIND_INSNS){
      n_debug("mtcsexpand.c  mtcs_expand_execute 109 MAY_HAVE_DEBUG_BIND_INSNS=true\n");
      expand_debug_locations(self);
   }

   if (self->deep_ter_debug_map){
      n_debug("mtcsexpand.c  mtcs_expand_execute 110 self->deep_ter_debug_map=true\n");
      delete self->deep_ter_debug_map;
      self->deep_ter_debug_map = NULL;
   }

   /* Free stuff we no longer need after GIMPLE optimizations.  */
   free_dominance_info (CDI_DOMINATORS);
   free_dominance_info (CDI_POST_DOMINATORS);
   delete_tree_cfg_annotations (fun);

   mtcs_outof_ssa_finish_out_of_ssa/*!finish_out_of_ssa*/(mtcsOutofSsa,&SA);

   /* We are no longer in SSA form.  */
   fun->gimple_df->in_ssa_p = false;
   loops_state_clear (LOOP_CLOSED_SSA);

   /* Expansion is used by optimization passes too, set maybe_hot_insn_p
   conservatively to true until they are all profile aware.  */
   delete self->lab_rtx_for_bb;
   free_histograms (fun);
   construct_exit_block (self);
   mtcs_emit_insn_locations_finalize/*!insn_locations_finalize*/(mtcsEmit);
//   for (seq =mtcs_rtl_data_get_current_sequence(mtcsRtlData); seq; seq = seq->next){
//      n_debug("mtcsexpand.c exapnd 107cc emit.req req:%p seq->first:%p seq->last:%p seq->next:%p\n",
//      seq,seq->first,seq->last,seq->next);
//   }

   if (var_ret_seq){
      n_debug("mtcsexpand.c  mtcs_expand_execute 111 var_ret_seq\n");
      rtx_insn *after = mtcsRtlData->x_return_label/*!return_label*/;
      rtx_insn *next = NEXT_INSN (after);
      if (next && NOTE_INSN_BASIC_BLOCK_P (next))
         after = next;
      mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,var_ret_seq, after);
   }

   if (hwasan_sanitize_stack_p ()){
      n_debug("mtcsexpand.c  mtcs_expand_execute 112 \n");
      hwasan_maybe_emit_frame_base_init ();
   }
   /* Zap the tree EH table.  */
   mtcs_except_set_eh_throw_stmt_table/*!set_eh_throw_stmt_table*/(mtcsExcept,fun, NULL);
   /* We need JUMP_LABEL be set in order to redirect jumps, and hence
   split edges which edge insertions might do.  */
   mtcs_dojump_rebuild_jump_labels/*!rebuild_jump_labels*/(mtcsDojump,mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData));
   /* If we have a single successor to the entry block, put the pending insns
   after parm birth, but before NOTE_INSNS_FUNCTION_BEG.  */
   if (single_succ_p (ENTRY_BLOCK_PTR_FOR_FN (fun))){
      n_debug("mtcsexpand.c  mtcs_expand_execute 113 single_succ_p (ENTRY_BLOCK_PTR_FOR_FN (fun)) \n");
      edge e = single_succ_edge (ENTRY_BLOCK_PTR_FOR_FN (fun));
      if (e->insns.r){
         n_debug("mtcsexpand.c  mtcs_expand_execute 114 e->insns.r\n");
         rtx_insn *insns = e->insns.r;
         e->insns.r = NULL;
         mtcs_dojump_rebuild_jump_labels_chain/*!rebuild_jump_labels_chain*/(mtcsDojump,insns);
         if (NOTE_P (parm_birth_insn)
         && NOTE_KIND (parm_birth_insn) == NOTE_INSN_FUNCTION_BEG)
            mtcs_emit_emit_insn_before_noloc/*!emit_insn_before_noloc*/(mtcsEmit,insns, parm_birth_insn, e->dest);
         else
            mtcs_emit_emit_insn_after_noloc/*!emit_insn_after_noloc*/(mtcsEmit,insns, parm_birth_insn, e->dest);
      }
   }
   n_debug("mtcsexpand.c  mtcs_expand_execute 115  mtcs_cfg_rtl_commit_edge_insertions \n");
   /* Otherwise, as well as for other edges, take the usual way.  */
   mtcs_cfg_rtl_commit_edge_insertions/*!commit_edge_insertions*/(mtcsCfgRtl);
   /* We're done expanding trees to RTL.  */
   self->currently_expanding_to_rtl = 0;
   flush_mark_addressable_queue ();
   FOR_BB_BETWEEN (bb, ENTRY_BLOCK_PTR_FOR_FN (fun)->next_bb, EXIT_BLOCK_PTR_FOR_FN (fun), next_bb){
      n_debug("mtcsexpand.c  mtcs_expand_execute 116 \n");
      edge e;
      edge_iterator ei;
      for (ei = ei_start (bb->succs); (e = ei_safe_edge (ei)); ){
         /* Clear EDGE_EXECUTABLE.  This flag is never used in the backend.  */
         e->flags &= ~EDGE_EXECUTABLE;

         /* At the moment not all abnormal edges match the RTL
         representation.  It is safe to remove them here as
         find_many_sub_basic_blocks will rediscover them.
         In the future we should get this fixed properly.  */
         if ((e->flags & EDGE_ABNORMAL)  && !(e->flags & EDGE_SIBCALL)){
            n_debug("mtcsexpand.c  mtcs_expand_execute 117 移走边  remove_edge e:%p\n",e);
            mtcs_cfg_context_remove_edge/*!remove_edge*/(mtcsCfgContext,e);
         }else{
            n_debug("mtcsexpand.c  mtcs_expand_execute 118 ei_next\n");
            ei_next (&ei);
         }
      }
   }

   auto_sbitmap blocks (last_basic_block_for_fn (fun));
   bitmap_ones (blocks);
   mtcs_cfg_build_find_many_sub_basic_blocks/*!find_many_sub_basic_blocks*/(mtcsCfgBuild,blocks);
   mtcs_cfg_rtl_purge_all_dead_edges/*!purge_all_dead_edges*/(mtcsCfgRtl);

   /* After initial rtl generation, call back to finish generating
   exception support code.  We need to do this before cleaning up
   the CFG as the code does not expect dead landing pads.  */
   if (fun->eh->region_tree != NULL){
      n_debug("mtcsexpand.c  mtcs_expand_execute 119  fun->eh->region_tree != NULL\n");
      mtcs_except_finish_eh_generation/*!finish_eh_generation*/(mtcsExcept);
   }

   /* Call expand_stack_alignment after finishing all
   updates to crtl->preferred_stack_boundary.  */
   expand_stack_alignment(self);
   /* Fixup REG_EQUIV notes in the prologue if there are tailcalls in this
   function.  */
   if (mtcsRtlData/*!crtl*/->tail_call_emit){
      n_debug("mtcsexpand.c  mtcs_expand_execute 120 mtcsRtlData/*!crtl*/->tail_call_emit\n");
      mtcs_calls_fixup_tail_calls/*!fixup_tail_calls*/(mtcsCalls);
   }

   HOST_WIDE_INT patch_area_size, patch_area_entry;
   parse_and_check_patch_area (mtcsOptionsItem->x_flag_patchable_function_entry, false,
         &patch_area_size, &patch_area_entry);

   tree patchable_function_entry_attr = lookup_attribute ("patchable_function_entry",DECL_ATTRIBUTES (cfun->decl));
   if (patchable_function_entry_attr){
      n_debug("mtcsexpand.c  mtcs_expand_execute 121 \n");
      tree pp_val = TREE_VALUE (patchable_function_entry_attr);
      tree patchable_function_entry_value1 = TREE_VALUE (pp_val);
      patch_area_size = tree_to_uhwi (patchable_function_entry_value1);
      patch_area_entry = 0;
      if (TREE_CHAIN (pp_val) != NULL_TREE){
         tree patchable_function_entry_value2 = TREE_VALUE (TREE_CHAIN (pp_val));
         patch_area_entry = tree_to_uhwi (patchable_function_entry_value2);
      }
   }

   if (patch_area_entry > patch_area_size){
      n_debug("mtcsexpand.c  mtcs_expand_execute 122 patch_area_size:%d\n",patch_area_size);

      if (patch_area_size > 0)
         warning (OPT_Wattributes, "patchable function entry %wu exceeds size %wu",
      patch_area_entry, patch_area_size);
      patch_area_entry = 0;
   }

   mtcsRtlData/*!crtl*/->patch_area_size = patch_area_size;
   mtcsRtlData/*!crtl*/->patch_area_entry = patch_area_entry;

   /* BB subdivision may have created basic blocks that are only reachable
   from unlikely bbs but not marked as such in the profile.  */
   if (mtcsOptionsItem->x_optimize){
      n_debug("mtcsexpand.c  mtcs_expand_execute 123 mtcsOptionsItem->x_optimize ENTRY_BLOCK_PTR_FOR_FN:%p\n",
            ENTRY_BLOCK_PTR_FOR_FN(cfun));
      propagate_unlikely_bbs_forward ();
   }

   /* Remove unreachable blocks, otherwise we cannot compute dominators
   which are needed for loop state verification.  As a side-effect
   this also compacts blocks.
   ???  We cannot remove trivially dead insns here as for example
   the DRAP reg on i?86 is not magically live at this point.
   gcc.c-torture/execute/ipa-sra-2.c execution, -Os -m32 fails otherwise.  */
   /* 整理可删除的bb和边,这里会删除很多无用的bb */
  // for (seq = mtcs_rtl_data_get_current_sequence(mtcsRtlData); seq; seq = seq->next){
   //   n_debug("mtcsexpand.c exapnd 117xx emit.req req:%p seq->first:%p seq->last:%p seq->next:%p\n",seq,seq->first,seq->last,seq->next);
  // }

   mtcs_cfg_cleanup_cleanup_cfg/*!cleanup_cfg*/(mtcsCfgCleanup,CLEANUP_NO_INSN_DEL);
   mtcs_cfg_context_checking_verify_flow_info/*!checking_verify_flow_info*/(mtcsCfgContext);
   /* Initialize pseudos allocated for hard registers.  */
   mtcs_func_emit_initial_value_sets/*!emit_initial_value_sets*/(mtcsFunc);
   /* And finally unshare all RTL.  */
   mtcs_emit_unshare_all_rtl/*!unshare_all_rtl*/(mtcsEmit);
   /* There's no need to defer outputting this function any more; we
   know we want to output it.  */
   DECL_DEFER_OUTPUT (current_function_decl) = 0;
   /* Now that we're done expanding trees to RTL, we shouldn't have any
   more CONCATs anywhere.  */
   generating_concat_p = 0;

   if (dump_file){
      fprintf (dump_file, "\n\n;;\n;; Full RTL generated for this function:\n;;\n");
      /* And the pass manager will dump RTL for us.  */
   }

   /* If we're emitting a nested function, make sure its parent gets
   emitted as well.  Doing otherwise confuses debug info.  */
   {
      tree parent;
      for (parent = DECL_CONTEXT (current_function_decl);parent != NULL_TREE; parent = get_containing_scope (parent)){
         if (TREE_CODE (parent) == FUNCTION_DECL){
            n_debug("mtcsexpand.c  mtcs_expand_execute 124\n");
            TREE_SYMBOL_REFERENCED (DECL_ASSEMBLER_NAME (parent)) = 1;
         }
      }
   }

   TREE_ASM_WRITTEN (current_function_decl) = 1;

   /* After expanding, the return labels are no longer needed. */
   mtcsRtlData->x_return_label/*!return_label*/ = NULL;
   mtcsRtlData->x_naked_return_label/*!naked_return_label*/ = NULL;
   /* After expanding, the tm_restart map is no longer needed.  */
   if (fun->gimple_df->tm_restart){
      n_debug("mtcsexpand.c  mtcs_expand_execute 125\n");
      fun->gimple_df->tm_restart = NULL;
   }
   n_debug("mtcsexpand.c  mtcs_expand_execute 126\n");

   /* Tag the blocks with a depth number so that change_scope can find
   the common parent easily.  */
   set_block_levels (DECL_INITIAL (fun->decl), 0);
   mtcs_func_default_rtl_profile/*!default_rtl_profile*/(mtcsFunc);

   /* For -dx discard loops now, otherwise IL verify in clean_state will
   ICE.  */
   if (mtcsOptionsItem->x_rtl_dump_and_exit){
      n_debug("mtcsexpand.c  mtcs_expand_execute 127\n");
      cfun->curr_properties &= ~PROP_loops;
      loop_optimizer_finalize ();
   }
   n_debug("mtcsexpand.c  mtcs_expand_execute 128 结束\n");
   return 0;
}

/* Take into account all sizes of partitions and reset DECL_RTLs.  */
//原型 account_stack_vars cfgexpand.cc
static poly_uint64 account_stack_vars (MtcsExpand *self)
{
  size_t si, j, i, n = self->stack_vars_num;
  poly_uint64 size = 0;
  for (si = 0; si < n; ++si){
      i = self->stack_vars_sorted[si];
      /* Skip variables that aren't partition representatives, for now.  */
      if (self->stack_vars[i].representative != i)
          continue;
      size += self->stack_vars[i].size;
      for (j = i; j != EOC; j = self->stack_vars[j].next)
          set_rtl(self,self->stack_vars[j].decl, NULL);
  }
  return size;
}

/* Make a fair guess for the size of the stack frame of the function
   in NODE.  This doesn't have to be exact, the result is only used in
   the inline heuristics.  So we don't want to run the full stack var
   packing algorithm (which is quadratic in the number of stack vars).
   Instead, we calculate the total size of all stack vars.  This turns
   out to be a pretty fair estimate -- packing of stack vars doesn't
   happen very often.  */
//原型 estimated_stack_frame_size cfgexpand.h cfgexpand.cc
HOST_WIDE_INT mtcs_expand_estimated_stack_frame_size (MtcsExpand *self,struct cgraph_node *node)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

  poly_int64 size = 0;
  size_t i;
  tree var;
  struct function *fn = DECL_STRUCT_FUNCTION (node->decl);
  mtcs_func_push_cfun/*!push_cfun*/(mtcsFunc,fn);
  init_vars_expansion(self);
  FOR_EACH_LOCAL_DECL (fn, i, var)
    if (auto_var_in_fn_p (var, fn->decl))
      size += expand_one_var(self,var, true, false);

  if (self->stack_vars_num > 0){
      /* Fake sorting the stack vars for account_stack_vars ().  */
      self->stack_vars_sorted = XNEWVEC (size_t, self->stack_vars_num);
      for (i = 0; i < self->stack_vars_num; ++i)
          self->stack_vars_sorted[i] = i;
      size += account_stack_vars(self);
  }

  fini_vars_expansion(self);
  mtcs_func_pop_cfun/*!pop_cfun*/(mtcsFunc);
  return estimated_poly_value (size);
}

/* Record the RTL assignment X for the default def of PARM.  */
//原型 set_parm_rtl cfgexpand.h cfgexpand.cc
void mtcs_expand_set_parm_rtl (MtcsExpand *self,tree parm, rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

  gcc_assert (TREE_CODE (parm) == PARM_DECL || TREE_CODE (parm) == RESULT_DECL);

  if (x && !MEM_P (x)){
      unsigned int align =  mtcs_mode_get_mininum_alignment/*MINIMUM_ALIGNMENT*/ (mtcsMode,TREE_TYPE (parm),
                          TYPE_MODE (TREE_TYPE (parm)),TYPE_ALIGN (TREE_TYPE (parm)));

     /* If the variable alignment is very large we'll dynamicaly
     allocate it, which means that in-frame portion is just a
     pointer.  ??? We've got a pseudo for sure here, do we
     actually dynamically allocate its spilling area if needed?
     ??? Isn't it a problem when Pmode alignment also exceeds
     MAX_SUPPORTED_STACK_ALIGNMENT, as can happen on cris and lm32?  */
      if (align > mtcs_func_get_max_support_stack_alignment/*!MAX_SUPPORTED_STACK_ALIGNMENT*/(mtcsFunc))
          align = mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mtcs_mode_get_Pmode(mtcsMode));

      record_alignment_for_reg_var (self,align);
  }
  tree ssa = ssa_default_def (cfun, parm);
  if (!ssa)
    return set_rtl(self,parm, x);

  int part = var_to_partition (SA.map, ssa);
  gcc_assert (part != NO_PARTITION);
  bool changed = bitmap_bit_p (SA.partitions_for_parm_default_defs, part);
  gcc_assert (changed);
  set_rtl(self,ssa, x);
  gcc_assert (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,parm) == x);
}

/* Output a return with no value.  */
//原型 expand_null_return_1 cfgexpand.cc
static void expand_null_return_1 (MtcsExpand *self)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
    MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
    MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
    MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

    mtcs_dojump_clear_pending_stack_adjust/*!clear_pending_stack_adjust*/(mtcsDojump);
    mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
    mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,mtcsRtlData->x_return_label/*!return_label*/);
}

/* Generate RTL to return from the current function, with no value.
   (That is, we do not do anything about returning any value.)  */
//原型 expand_null_return rtl.h cfgexpand.cc
void mtcs_expand_expand_null_return (MtcsExpand *self)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
    MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

    /* If this function was declared to return a value, but we
    didn't, clobber the return registers so that they are not
    propagated live to the rest of the function.  */
    mtcs_func_clobber_return_register/*!clobber_return_register*/(mtcsFunc);
    expand_null_return_1 (self);
}

static void mtcsExpandInit(MtcsExpand *self)
{

}

MtcsExpand *mtcs_expand_new(MtcsMode *mtcsMode)
{
     MtcsExpand *self = n_slice_alloc0 (sizeof(MtcsExpand));
     mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
     mtcsExpandInit(self);
     return self;
}

/*-------------------------------- expand -------------------*/
//原型 NEXT_PASS (pass_expand, 1);   RTL_PASS         cfgexpand.cc                 expand                      y  无条件执行
static nuint pass_expand_execute_cb(MtcsPass *mtcsPass,function *func)
{
    MtcsPassExpand *self=(MtcsPassExpand *)mtcsPass;
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsExpand *mtcsExpand =mtcs_target_get_expand(mtcsTarget);
    nuint ret=  mtcs_expand_execute(mtcsExpand,func);
    n_debug("mtcsexpand.cpass_expand_execute_cb 执行 expand pass \n");
    mtcs_asm_print(mtcsTarget->mtcsAsm);
    return ret;
}

static void mtcsPassExpandInit(MtcsPassExpand *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =pass_expand_execute_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            ( PROP_ssa | PROP_gimple_leh | PROP_cfg
                | PROP_gimple_lcx
                | PROP_gimple_lvec
                | PROP_gimple_lva), /* properties_required */
                PROP_rtl,        /* properties_provided */
                ( PROP_ssa | PROP_gimple )        /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
            0, /* todo_flags_start */
            0 /*todo_flags_finish */);
}

MtcsPassExpand *mtcs_pass_expand_new(MtcsMode *mtcsMode)
{
    MtcsPassExpand *self = n_slice_alloc0 (sizeof(MtcsPassExpand));
    mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
    mtcs_pass_init((MtcsPass *)self,RTL_PASS,"expand");
    mtcsPassExpandInit(self);
    return self;
}
