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

#ifndef __GCC_MTCS_ALIAS__
#define __GCC_MTCS_ALIAS__

#include "../nlib.h"
#include "mtcscomponent.h"

struct GTY(()) alias_set_entry;

typedef struct _MtcsAlias MtcsAlias;
struct _MtcsAlias
{
    MtcsComponent parent;
    struct {
      unsigned long long num_alias_zero;
      unsigned long long num_same_alias_set;
      unsigned long long num_same_objects;
      unsigned long long num_volatile;
      unsigned long long num_dag;
      unsigned long long num_universal;
      unsigned long long num_disambiguated;
    } alias_stats;

    /* reg_base_value[N] gives an address to which register N is related.
       If all sets after the first add or subtract to the current value
       or otherwise modify it so it does not point to a different top level
       object, reg_base_value[N] is equal to the address part of the source
       of the first set.

       A base address can be an ADDRESS, SYMBOL_REF, or LABEL_REF.  ADDRESS
       expressions represent three types of base:

         1. incoming arguments.  There is just one ADDRESS to represent all
       arguments, since we do not know at this level whether accesses
       based on different arguments can alias.  The ADDRESS has id 0.

         2. stack_pointer_rtx, frame_pointer_rtx, hard_frame_pointer_rtx
       (if distinct from frame_pointer_rtx) and arg_pointer_rtx.
       Each of these rtxes has a separate ADDRESS associated with it,
       each with a negative id.

       GCC is (and is required to be) precise in which register it
       chooses to access a particular region of stack.  We can therefore
       assume that accesses based on one of these rtxes do not alias
       accesses based on another of these rtxes.

         3. bases that are derived from malloc()ed memory (REG_NOALIAS).
       Each such piece of memory has a separate ADDRESS associated
       with it, each with an id greater than 0.

       Accesses based on one ADDRESS do not alias accesses based on other
       ADDRESSes.  Accesses based on ADDRESSes in groups (2) and (3) do not
       alias globals either; the ADDRESSes have Pmode to indicate this.
       The ADDRESS in group (1) _may_ alias globals; it has VOIDmode to
       indicate this.  */

     GTY(()) vec<rtx, va_gc> *reg_base_value;
     rtx *new_reg_base_value;

    /* The single VOIDmode ADDRESS that represents all argument bases.
       It has id 0.  */
     GTY(()) rtx arg_base_value;

    /* Used to allocate unique ids to each REG_NOALIAS ADDRESS.  */
     int unique_id;

    /* We preserve the copy of old array around to avoid amount of garbage
       produced.  About 8% of garbage produced were attributed to this
       array.  */
     GTY((deletable)) vec<rtx, va_gc> *old_reg_base_value;
     /* Vector indexed by N giving the initial (unchanging) value known for
        pseudo-register N.  This vector is initialized in init_alias_analysis,
        and does not change until end_alias_analysis is called.  */
      GTY(()) vec<rtx, va_gc> *reg_known_value;



      /* Vector recording for each reg_known_value whether it is due to a
         REG_EQUIV note.  Future passes (viz., reload) may replace the
         pseudo with the equivalent expression and so we account for the
         dependences that would be introduced if that happens.

         The REG_EQUIV notes created in assign_parms may mention the arg
         pointer, and there are explicit insns in the RTL that modify the
         arg pointer.  Thus we must ensure that such insns don't get
         scheduled across each other because that would invalidate the
         REG_EQUIV notes.  One could argue that the REG_EQUIV notes are
         wrong, but solving the problem in the scheduler will likely give
         better code, so we do it here.  */
       sbitmap reg_known_equiv_p;

      /* True when scanning insns from the start of the rtl to the
         NOTE_INSN_FUNCTION_BEG note.  */
       bool copying_arguments;


      /* The splay-tree used to store the various alias set entries.  */
       GTY (()) vec<alias_set_entry *, va_gc> *alias_sets;

       /* Allocate an alias set for use in storing and reading from the varargs
          spill area.  */

        GTY(()) alias_set_type varargs_set;
        /* Likewise, but used for the fixed portions of the frame, e.g., register
           save areas.  */

        GTY(()) alias_set_type frame_set;

        /* Called from init_alias_analysis indirectly through note_stores,
           or directly if DEST is a register with a REG_NOALIAS note attached.
           SET is null in the latter case.  */

        /* While scanning insns to find base values, reg_seen[N] is nonzero if
           register N has been set in this function.  */
        sbitmap reg_seen;


};


MtcsAlias *mtcs_alias_new(MtcsMode *mtcsMode);
//原型 refs_same_for_tbaa_p alias.h alias.cc
bool mtcs_alias_refs_same_for_tbaa_p (MtcsAlias *self,tree earlier, tree later);
//原型 mems_same_for_tbaa_p alias.h alias.cc
bool mtcs_alias_mems_same_for_tbaa_p (MtcsAlias *self,rtx earlier, rtx later);
//原型 alias_set_subset_of alias.h alias.cc
bool mtcs_alias_alias_set_subset_of (MtcsAlias *self,alias_set_type set1, alias_set_type set2);
//原型 alias_sets_conflict_p alias.h alias.cc
bool mtcs_alias_alias_sets_conflict_p (MtcsAlias *self,alias_set_type set1, alias_set_type set2);
//原型 alias_sets_must_conflict_p alias.h alias.cc
bool mtcs_alias_alias_sets_must_conflict_p (MtcsAlias *self,alias_set_type set1, alias_set_type set2);
//原型 objects_must_conflict_p alias.h alias.cc
bool mtcs_alias_objects_must_conflict_p (MtcsAlias *self,tree t1, tree t2);
//原型 ends_tbaa_access_path_p alias.h alias.cc
bool mtcs_alias_ends_tbaa_access_path_p (MtcsAlias *self,const_tree t);
//原型 component_uses_parent_alias_set_from alias.h alias.cc
tree mtcs_alias_component_uses_parent_alias_set_from (MtcsAlias *self,const_tree t);
//原型 get_deref_alias_set alias.h alias.cc
alias_set_type mtcs_alias_get_deref_alias_set (MtcsAlias *self,tree t);
//原型 reference_alias_ptr_type_1 alias.h alias.cc
tree mtcs_alias_reference_alias_ptr_type_1 (MtcsAlias *self,tree *t);
//原型 reference_alias_ptr_type alias.h alias.cc
tree mtcs_alias_reference_alias_ptr_type (MtcsAlias *self,tree t);
//原型 alias_ptr_types_compatible_p alias.h alias.cc
bool mtcs_alias_alias_ptr_types_compatible_p (MtcsAlias *self,tree t1, tree t2);
//原型 init_alias_set_entry alias.h alias.cc
alias_set_entry * mtcs_alias_init_alias_set_entry (MtcsAlias *self,alias_set_type set);
//原型 get_alias_set alias.h alias.cc
alias_set_type mtcs_alias_get_alias_set (MtcsAlias *self,tree t);
//原型 new_alias_set alias.h alias.cc
alias_set_type mtcs_alias_new_alias_set (MtcsAlias *self);
//原型 record_alias_subset alias.h alias.cc
void mtcs_alias_record_alias_subset (MtcsAlias *self,alias_set_type superset, alias_set_type subset);
//原型 record_component_aliases alias.h alias.cc
void mtcs_alias_record_component_aliases (MtcsAlias *self,tree type);
//原型 get_varargs_alias_set alias.h alias.cc
alias_set_type mtcs_alias_get_varargs_alias_set (MtcsAlias *self);
//原型 get_frame_alias_set alias.h alias.cc
alias_set_type mtcs_alias_get_frame_alias_set (MtcsAlias *self);
//原型 get_reg_base_value rtl.h alias.cc
rtx mtcs_alias_get_reg_base_value (MtcsAlias *self,unsigned int regno);
//原型 get_reg_known_value rtl.h alias.cc
rtx mtcs_alias_get_reg_known_value (MtcsAlias *self,unsigned int regno);
//原型 get_reg_known_equiv_p rtl.h alias.cc
bool mtcs_alias_get_reg_known_equiv_p (MtcsAlias *self,unsigned int regno);
//原型 canon_rtx rtl.h alias.cc
rtx mtcs_alias_canon_rtx (MtcsAlias *self,rtx x);
//原型 compare_base_decls alias.h alias.cc
int mtcs_alias_compare_base_decls (MtcsAlias *self,tree base1, tree base2);
//原型 may_be_sp_based_p rtl.h alias.cc
bool mtcs_alias_may_be_sp_based_p (MtcsAlias *self,rtx x);
//原型 get_addr rtl.h alias.cc
rtx mtcs_alias_get_addr (MtcsAlias *self,rtx x);
//原型 read_dependence rtl.h alias.cc
bool mtcs_alias_read_dependence (MtcsAlias *self,const_rtx mem, const_rtx x);
//原型 nonoverlapping_memrefs_p alias.h alias.cc
bool mtcs_alias_nonoverlapping_memrefs_p (MtcsAlias *self,const_rtx x, const_rtx y, bool loop_invariant);
//原型 true_dependence rtl.h alias.cc
bool mtcs_alias_true_dependence (MtcsAlias *self,const_rtx mem, machine_mode mem_mode, const_rtx x);
//原型 canon_true_dependence rtl.h alias.cc
bool  mtcs_alias_canon_true_dependence (MtcsAlias *self,const_rtx mem, machine_mode mem_mode, rtx mem_addr,
             const_rtx x, rtx x_addr);
//原型 anti_dependence rtl.h alias.cc
bool mtcs_alias_anti_dependence (MtcsAlias *self,const_rtx mem, const_rtx x);
//原型 canon_anti_dependence rtl.h alias.cc
bool mtcs_alias_canon_anti_dependence (MtcsAlias *self,const_rtx mem, bool mem_canonicalized,
             const_rtx x, machine_mode x_mode, rtx x_addr);
//原型 output_dependence rtl.h alias.cc
bool mtcs_alias_output_dependence (MtcsAlias *self,const_rtx mem, const_rtx x);
//原型 canon_output_dependence rtl.h alias.cc
bool mtcs_alias_canon_output_dependence (MtcsAlias *self,const_rtx mem, bool mem_canonicalized,
          const_rtx x, machine_mode x_mode, rtx x_addr);
//原型 may_alias_p rtl.h alias.cc
bool mtcs_alias_may_alias_p (MtcsAlias *self,const_rtx mem, const_rtx x);
//原型 memory_modified_in_insn_p rtl.h alias.cc
bool mtcs_alias_memory_modified_in_insn_p (MtcsAlias *self,const_rtx mem, const_rtx insn);
//原型 init_alias_analysis rtl.h alias.cc
void mtcs_alias_init_alias_analysis (MtcsAlias *self);
//原型 vt_equate_reg_base_value rtl.h alias.cc
void mtcs_alias_vt_equate_reg_base_value (MtcsAlias *self,const_rtx reg1, const_rtx reg2);
//原型 end_alias_analysis rtl.h alias.cc
void mtcs_alias_end_alias_analysis (MtcsAlias *self);
//原型 dump_alias_stats_in_alias_c alias.h alias.cc
void mtcs_alias_dump_alias_stats_in_alias_c (MtcsAlias *self,FILE *s);

#endif

