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

#ifndef __GCC_MTCS_DWARF2_OUT__
#define __GCC_MTCS_DWARF2_OUT__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "dwarf2out.h"
#include "mtcsdebug.h"

struct mtcs_debug_sym_off_pair
{
   const char * GTY((skip)) sym;
   unsigned HOST_WIDE_INT off;
};

struct  mtcs_debug_inline_entry_data_hasher ;
struct  mtcs_debug_addr_hasher;

typedef struct GTY(()) mtcs_debug_die_arg_entry_struct {
    dw_die_ref die;
    tree arg;
} mtcs_debug_die_arg_entry;

struct GTY(()) mtcs_debug_dw_ranges_by_label
{
  const char *begin;
  const char *end;
};

struct GTY(()) mtcs_debug_dw_ranges
{
   const char *label;
   /* If this is positive, it's a block number, otherwise it's a
   bitwise-negated index into mtcs_debug_dw_ranges_by_label.  */
   int num;
   /* If idx is equal to DW_RANGES_IDX_SKELETON, it should be emitted
   into .debug_rnglists section rather than .debug_rnglists.dwo
   for -gsplit-dwarf and DWARF >= 5.  */
#define DW_RANGES_IDX_SKELETON ((1U << 31) - 1)
   /* Index for the range list for DW_FORM_rnglistx.  */
   unsigned int idx : 31;
   /* True if this range might be possibly in a different section
   from previous entry.  */
   unsigned int maybe_new_sec : 1;
   addr_table_entry *begin_entry;
   addr_table_entry *end_entry;
};

/* A structure to hold a macinfo entry.  */

typedef struct GTY(()) mtcs_debug_macinfo_struct
{
   unsigned char code;
   unsigned HOST_WIDE_INT lineno;
   const char *info;
}mtcs_debug_macinfo_entry;


/* The pubname structure */

typedef struct GTY(()) mtcs_debug_pubname_struct
{
   dw_die_ref die;
   const char *name;
}mtcs_debug_pubname_entry;

/* A list of DIEs for which we can't determine ancestry (parent_die
   field) just yet.  Later in dwarf2out_finish we will fill in the
   missing bits.  */
typedef struct GTY(()) mtcs_debug_limbo_die_struct
{
   dw_die_ref die;
   /* The tree for which this DIE was created.  We use this to
   determine ancestry later.  */
   tree created_for;
   struct mtcs_debug_limbo_die_struct *next;
}mtcs_debug_limbo_die_node;

struct GTY(()) dw_line_info_table;
struct dw_loc_list_hasher;
struct decl_loc_hasher;
struct mtcs_debug_block_die_hasher;
struct variable_value_hasher;
struct decl_die_hasher ;
struct mtcs_debug_dwarf_file_hasher;
struct comdat_type_node;
struct mtcs_debug_indirect_string_hasher;

typedef struct _MtcsDwarf2Out MtcsDwarf2Out;
struct _MtcsDwarf2Out
{
   MtcsDebug parent;
   rtx_insn *last_var_location_insn;
   rtx_insn *cached_next_real_insn;
   /* Array of RTXes referenced by the debugging information, which therefore
   must be kept around forever.  */
   GTY(()) vec<rtx, va_gc> *used_rtx_array;
   /* A pointer to the base of a list of incomplete types which might be
   completed at some later time.  incomplete_types_list needs to be a
   vec<tree, va_gc> *because we want to tell the garbage collector about
   it.  */
   GTY(()) vec<tree, va_gc> *incomplete_types;
   /* Pointers to various DWARF2 sections.  */
   GTY(()) section *debug_info_section;
   GTY(()) section *debug_skeleton_info_section;
   GTY(()) section *debug_abbrev_section;
   GTY(()) section *debug_skeleton_abbrev_section;
   GTY(()) section *debug_aranges_section;
   GTY(()) section *debug_addr_section;
   GTY(()) section *debug_macinfo_section;
   const char *debug_macinfo_section_name;
   unsigned macinfo_label_base;
   GTY(()) section *debug_line_section;
   GTY(()) section *debug_skeleton_line_section;
   GTY(()) section *debug_loc_section;
   GTY(()) section *debug_pubnames_section;
   GTY(()) section *debug_pubtypes_section;
   GTY(()) section *debug_str_section;
   GTY(()) section *debug_line_str_section;
   GTY(()) section *debug_str_dwo_section;
   GTY(()) section *debug_str_offsets_section;
   GTY(()) section *debug_ranges_section;
   GTY(()) section *debug_ranges_dwo_section;
   GTY(()) section *debug_frame_section;

   GTY(()) vec<dw_fde_ref, va_gc> *fde_vec;

   GTY (()) hash_table<mtcs_debug_indirect_string_hasher> *debug_str_hash;

   GTY (()) hash_table<mtcs_debug_indirect_string_hasher> *debug_line_str_hash;

   /* With split_debug_info, both the comp_dir and dwo_name go in the
   main object file, rather than the dwo, similar to the force_direct
   parameter elsewhere but with additional complications:

   1) The string is needed in both the main object file and the dwo.
   That is, the comp_dir and dwo_name will appear in both places.

   2) Strings can use four forms: DW_FORM_string, DW_FORM_strp,
   DW_FORM_line_strp or DW_FORM_strx/GNU_str_index.

   3) GCC chooses the form to use late, depending on the size and
   reference count.

   Rather than forcing the all debug string handling functions and
   callers to deal with these complications, simply use a separate,
   special-cased string table for any attribute that should go in the
   main object file.  This limits the complexity to just the places
   that need it.  */

   GTY (()) hash_table<mtcs_debug_indirect_string_hasher> *skeleton_debug_str_hash;

   GTY(()) int dw2_string_counter;

   /* True if the compilation unit places functions in more than one section.  */
   GTY(()) bool have_multiple_function_sections;

   /* The default cold text section.  */
   GTY(()) section *cold_text_section;

   /* True if currently in text section.  */
   GTY(()) bool in_text_section_p ;

   /* Last debug-on location in corresponding section.  */
   GTY(()) const char *last_text_label;
   GTY(()) const char *last_cold_label;

   /* Mark debug-on/off locations per section.
   NULL means the section is not used at all.  */
   GTY(()) vec<const char *, va_gc> *switch_text_ranges;
   GTY(()) vec<const char *, va_gc> *switch_cold_ranges;

   /* The DIE for C++14 'auto' in a function return type.  */
   GTY(()) dw_die_ref auto_die;

   /* The DIE for C++14 'decltype(auto)' in a function return type.  */
   GTY(()) dw_die_ref decltype_auto_die;

   /* Personality decl of current unit.  Used only when assembler does not support
   personality CFI.  */
   GTY(()) rtx current_unit_personality;

   /* Whether an eh_frame section is required.  */
   GTY(()) bool do_eh_frame ;

   /* .debug_rnglists next index.  */
   unsigned int rnglist_idx;

   /* This is an upper bound for view numbers that the assembler may
   assign to symbolic views output in this translation.  It is used to
   decide how big a field to use to represent view numbers in
   symview-classed attributes.  */

   var_loc_view symview_upper_bound;

   /* Set to TRUE while dwarf2out_early_global_decl is running.  */
   bool early_dwarf;
   bool early_dwarf_finished;

   /* A bit is set in ZERO_VIEW_P if we are using the assembler-supported
   view computation, and it refers to a view identifier for which we
   will not emit a label because it is known to map to a view number
   zero.  We won't allocate the bitmap if we're not using assembler
   support for location views, but we have to make the variable
   visible for GGC and for code that will be optimized out for lack of
   support but that's still parsed and compiled.  We could abstract it
   out with macros, but it's not worth it.  */
   GTY(()) bitmap zero_view_p;

   /* This location is used by calc_die_sizes() to keep track
   the offset of each DIE within the .debug_info section.  */
   unsigned long next_die_offset;

   /* Record the root of the DIE's built for the current compilation unit.  */
   GTY(()) dw_die_ref single_comp_unit_die;

   /* A list of type DIEs that have been separated into comdat sections.  */
   GTY(()) comdat_type_node *comdat_type_list;

   /* A list of CU DIEs that have been separated.  */
   GTY(()) mtcs_debug_limbo_die_node *cu_die_list;

   /* A list of DIEs with a NULL parent waiting to be relocated.  */
   GTY(()) mtcs_debug_limbo_die_node *limbo_die_list;

   /* A list of DIEs for which we may have to generate
   DW_AT_{,MIPS_}linkage_name once their DECL_ASSEMBLER_NAMEs are set.  */
   GTY(()) mtcs_debug_limbo_die_node *deferred_asm_name;
   /* Filenames referenced by this compilation unit.  */
   GTY(()) hash_table<mtcs_debug_dwarf_file_hasher> *file_table;
   /* A hash table of references to DIE's that describe declarations.
   The key is a DECL_UID() which is a unique number identifying each decl.  */
   GTY (()) hash_table<decl_die_hasher> *decl_die_table;
   /* A hash table of DIEs that contain DW_OP_GNU_variable_value with
   dw_val_class_decl_ref class, indexed by FUNCTION_DECLs which is
   DECL_CONTEXT of the referenced VAR_DECLs.  */
   GTY (()) hash_table<variable_value_hasher> *variable_value_hash;
   /* A hash table of references to DIE's that describe COMMON blocks.
   The key is DECL_UID() ^ die_parent.  */
   GTY (()) hash_table<mtcs_debug_block_die_hasher> *common_block_die_table;
   /* Table of decl location linked lists.  */
   GTY (()) hash_table<decl_loc_hasher> *decl_loc_table;


   /* Head and tail of call_arg_loc chain.  */
   GTY (()) struct call_arg_loc_node *call_arg_locations;
   struct call_arg_loc_node *call_arg_loc_last;

   /* Number of call sites in the current function.  */
   int call_site_count;
   /* Number of tail call sites in the current function.  */
   int tail_call_site_count;

   /* Table of cached location lists.  */
   GTY (()) hash_table<dw_loc_list_hasher> *cached_dw_loc_list_table;

   /* A vector of references to DIE's that are uniquely identified by their tag,
   presence/absence of children DIE's, and list of attribute/value pairs.  */
   GTY(()) vec<dw_die_ref, va_gc> *abbrev_die_table;

   /* A hash map to remember the stack usage for DWARF procedures.  The value
   stored is the stack size difference between before the DWARF procedure
   invokation and after it returned.  In other words, for a DWARF procedure
   that consumes N stack slots and that pushes M ones, this stores M - N.  */
   hash_map<dw_die_ref, int> *dwarf_proc_stack_usage_map;

   /* A global counter for generating labels for line number data.  */
   unsigned int line_info_label_num;

   /* The current table to which we should emit line number information
   for the current function.  This will be set up at the beginning of
   assembly for the function.  */
   GTY(()) dw_line_info_table *cur_line_info_table;

   /* The two default tables of line number info.  */
   GTY(()) dw_line_info_table *text_section_line_info;
   GTY(()) dw_line_info_table *cold_text_section_line_info;

   /* The set of all non-default tables of line number info.  */
   GTY(()) vec<dw_line_info_table *, va_gc> *separate_line_info;

   /* A flag to tell pubnames/types export if there is an info section to
   refer to.  */
   bool info_section_emitted;

   /* A pointer to the base of a table that contains a list of publicly
   accessible names.  */
   GTY (()) vec<mtcs_debug_pubname_entry, va_gc> *pubname_table;

   /* A pointer to the base of a table that contains a list of publicly
   accessible types.  */
   GTY (()) vec<mtcs_debug_pubname_entry, va_gc> *pubtype_table;

   /* A pointer to the base of a table that contains a list of macro
   defines/undefines (and file start/end markers).  */
   GTY (()) vec<mtcs_debug_macinfo_entry, va_gc> *macinfo_table;

   /* Vector of dies for which we should generate .debug_ranges info.  */
   GTY (()) vec<mtcs_debug_dw_ranges, va_gc> *ranges_table;

   /* Vector of pairs of labels referenced in ranges_table.  */
   GTY (()) vec<mtcs_debug_dw_ranges_by_label, va_gc> *ranges_by_label;

   /* Whether we have location lists that need outputting */
   GTY(()) bool have_location_lists;

   /* Unique label counter.  */
   GTY(()) unsigned int loclabel_num;

   /* Unique label counter for point-of-call tables.  */
   GTY(()) unsigned int poc_label_num;

   /* The last file entry emitted by maybe_emit_file().  */
   GTY(()) struct dwarf_file_data * last_emitted_file;

   /* Number of internal labels generated by gen_internal_sym().  */
   GTY(()) int label_num;

   GTY(()) vec<mtcs_debug_die_arg_entry, va_gc> *tmpl_value_parm_die_table;

   /* Instances of generic types for which we need to generate debug
   info that describe their generic parameters and arguments. That
   generation needs to happen once all types are properly laid out so
   we do it at the end of compilation.  */
   GTY(()) vec<tree, va_gc> *generic_type_instances;

   /* Offset from the "steady-state frame pointer" to the frame base,
   within the current function.  */
   poly_int64 frame_pointer_fb_offset;
   bool frame_pointer_fb_offset_valid;

   vec<dw_die_ref> base_types;

   const char *(*demangle_name_func) (const char *);
   /* Table of entries into the .debug_addr section.  */
   GTY (()) hash_table<mtcs_debug_addr_hasher> *addr_index_table;
   GTY(()) hash_map<tree, mtcs_debug_sym_off_pair/*!sym_off_pair*/> *external_die_map;

   /* During assign_location_list_indexes and output_loclists_offset the
   current index, after it the number of assigned indexes (i.e. how
   large the .debug_loclists* offset table should be).  */
   unsigned int loc_list_idx;
   /* The following 3 variables are temporaries that are computed only during the
   build_abbrev_table call and used and released during the following
   optimize_abbrev_table call.  */

   /* First abbrev_id that can be optimized based on usage.  */
   unsigned int abbrev_opt_start;

   /* Maximum abbrev_id of a base type plus one (we can't optimize DIEs with
   abbrev_id smaller than this, because they must be already sized
   during build_abbrev_table).  */
   unsigned int abbrev_opt_base_type_end;

   /* Vector of usage counts during build_abbrev_table.  Indexed by
   abbrev_id - abbrev_opt_start.  */
   vec<unsigned int> abbrev_usage_count;

   /* Vector of all DIEs added with die_abbrev >= abbrev_opt_start.  */
   vec<dw_die_ref> sorted_abbrev_dies;
   unsigned int output_line_info_generation;
   /* Inlined entry points pending DIE creation in this compilation unit.  */
   GTY(()) hash_table<mtcs_debug_inline_entry_data_hasher> *inline_entry_data_table;

   char *producer_string;

   /* True if before or during processing of the first function being emitted.  */
   bool in_first_function_p ;
   /* True if loc_note during dwarf2out_var_location call might still be
   before first real instruction at address equal to .Ltext0.  */
   bool maybe_at_text_label_p ;
   /* One above highest N where .LVLN label might be equal to .Ltext0 label.  */
   unsigned int first_loclabel_num_not_at_text_label;
   /* Temporary holder for dwarf2out_register_main_translation_unit.  Used to let
   front-ends register a translation unit even before dwarf2out_init is
   called.  */
   tree main_translation_unit;
   /* As init_sections_and_labels may get called multiple times, have a
   generation count for labels.  */
   unsigned init_sections_and_labels_generation;
};



MtcsDwarf2Out *mtcs_dwarf2_out_new(MtcsMode *mtcsMode);
void           mtcs_dwarf2_out_init (MtcsDwarf2Out *self);

//原型 dw_cfi_oprnd1_desc dwarf2out.h dwarf2out.cc
enum dw_cfi_oprnd_type mtcs_dwarf2_out_dw_cfi_oprnd1_desc (MtcsDwarf2Out *self,dwarf_call_frame_info cfi);
//原型 dw_cfi_oprnd2_desc dwarf2out.h dwarf2out.cc
enum dw_cfi_oprnd_type mtcs_dwarf2_out_dw_cfi_oprnd2_desc (MtcsDwarf2Out *self,dwarf_call_frame_info cfi);
void mtcs_dwarf2_out_output_loc_sequence_raw (MtcsDwarf2Out *self,dw_loc_descr_ref loc);
section *mtcs_dwarf2_out_get_cold_text_section(MtcsDwarf2Out *self);
//原型 cfa_equal_p dwarf2out.h dwarf2cfi.cc
bool mtcs_dwarf2_out_cfa_equal_p (MtcsDwarf2Out *self,const dw_cfa_location *loc1, const dw_cfa_location *loc2);
//原型 dwarf2out_alloc_current_fde dwarf2out.h dwarf2out.cc
dw_fde_ref mtcs_dwarf2_out_dwarf2out_alloc_current_fde (MtcsDwarf2Out *self);
//原型 dwarf2out_begin_prologue debug.h dwarf2out.cc
void mtcs_dwarf2_out_dwarf2out_begin_prologue (MtcsDwarf2Out *self,unsigned int line ATTRIBUTE_UNUSED,
           unsigned int column ATTRIBUTE_UNUSED,
           const char *file ATTRIBUTE_UNUSED);
//原型 dwarf2out_vms_end_prologue debug.h dwarf2out.cc
void mtcs_dwarf2_out_dwarf2out_vms_end_prologue (MtcsDwarf2Out *self,
      unsigned int line ATTRIBUTE_UNUSED, const char *file ATTRIBUTE_UNUSED);
//原型 dwarf2out_vms_begin_epilogue debug.h dwarf2out.cc
void mtcs_dwarf2_out_dwarf2out_vms_begin_epilogue (MtcsDwarf2Out *self,unsigned int line ATTRIBUTE_UNUSED,
           const char *file ATTRIBUTE_UNUSED);
//原型 dwarf2out_end_epilogue debug.h dwarf2out.cc
void mtcs_dwarf2_out_dwarf2out_end_epilogue (MtcsDwarf2Out *self,unsigned int line ATTRIBUTE_UNUSED,
         const char *file ATTRIBUTE_UNUSED);
//原型 dwarf2out_frame_finish debug.h dwarf2out.cc toplev.cc调用
void mtcs_dwarf2_out_dwarf2out_frame_finish (MtcsDwarf2Out *self);
//原型 dwarf2out_switch_text_section debug.h dwarf2out.cc
void mtcs_dwarf2_out_dwarf2out_switch_text_section (MtcsDwarf2Out *self);
//原型 loc_descr_equal_p dwarf2out.h dwarf2out.cc
bool mtcs_dwarf2_out_loc_descr_equal_p (MtcsDwarf2Out *self,dw_loc_descr_ref a, dw_loc_descr_ref b);
//原型 output_loc_sequence dwarf2out.h dwarf2out.cc
void mtcs_dwarf2_out_output_loc_sequence (MtcsDwarf2Out *self,dw_loc_descr_ref loc, int for_eh_or_skip);
//原型 output_loc_sequence_raw dwarf2out.h dwarf2out.cc
void mtcs_dwarf2_out_output_loc_sequence_raw (MtcsDwarf2Out *self,dw_loc_descr_ref loc);
//原型 build_cfa_aligned_loc dwarf2out.h dwarf2out.cc
struct dw_loc_descr_node * mtcs_dwarf2_out_build_cfa_aligned_loc (MtcsDwarf2Out *self,dw_cfa_location *cfa,
             poly_int64 offset, HOST_WIDE_INT alignment);
//原型 dwarf2out_default_as_loc_support debug.h dwarf2out.cc
bool mtcs_dwarf2_out_dwarf2out_default_as_loc_support (MtcsDwarf2Out *self);
//原型 mtcs_dwarf2_out_dwarf2out_default_as_locview_support debug.h dwarf2out.cc
bool mtcs_dwarf2_out_dwarf2out_default_as_locview_support (MtcsDwarf2Out *self);
//原型 dwarf2out_set_demangle_name_func dwarf2out.h dwarf2out.cc
void mtcs_dwarf2_out_dwarf2out_set_demangle_name_func (MtcsDwarf2Out *self,const char *(*func) (const char *));
//原型 lookup_decl_die dwarf2out.h dwarf2out.cc
dw_die_ref mtcs_dwarf2_out_lookup_decl_die (MtcsDwarf2Out *self,tree decl);
//原型 base_type_die dwarf2out.h dwarf2out.cc
dw_die_ref mtcs_dwarf2_out_base_type_die (MtcsDwarf2Out *self,tree type, bool reverse);
//原型 build_span_loc dwarf2out.h dwarf2out.cc
struct dw_loc_descr_node *mtcs_dwarf2_out_build_span_loc (MtcsDwarf2Out *self,struct cfa_reg reg);
//原型 build_cfa_loc dwarf2out.h dwarf2out.cc
struct dw_loc_descr_node *mtcs_dwarf2_out_build_cfa_loc (MtcsDwarf2Out *self,dw_cfa_location *cfa, poly_int64 offset);
//原型 mem_loc_descriptor dwarf2out.h dwarf2out.cc
dw_loc_descr_ref mtcs_dwarf2_out_mem_loc_descriptor (MtcsDwarf2Out *self,rtx rtl,
      machine_mode mode,machine_mode mem_mode,enum var_init_status initialized);
//原型 add_name_attribute dwarf2out.h dwarf2out.cc
void mtcs_dwarf2_out_add_name_attribute (MtcsDwarf2Out *self, dw_die_ref die, const char *name_string);
//原型 dwarf2out_cc_finalize dwarf2out.h dwarf2out.cc
void mtcs_dwarf2_out_dwarf2out_cc_finalize (MtcsDwarf2Out *self);
//原型 debug_dwarf dwarf2out.h dwarf2out.cc
DEBUG_FUNCTION void mtcs_dwarf2_out_debug_dwarf (MtcsDwarf2Out *self);
//原型 size_of_locs dwarf2out.h dwarf2out.cc
unsigned long mtcs_dwarf2_out_size_of_locs (MtcsDwarf2Out *self,dw_loc_descr_ref loc);
//原型 debug_dwarf_loc_descr dwarf2out.h dwarf2out.cc
DEBUG_FUNCTION void mtcs_dwarf2_out_debug_dwarf_loc_descr (MtcsDwarf2Out *self,dw_loc_descr_ref loc);
//原型 debug_dwarf_die dwarf2out.h dwarf2out.cc
DEBUG_FUNCTION void mtcs_dwarf2_out_debug_dwarf_die (MtcsDwarf2Out *self,dw_die_ref die);
DEBUG_FUNCTION void mtcs_dwarf2_out_debug (MtcsDwarf2Out *self,die_struct &ref);
DEBUG_FUNCTION void mtcs_dwarf2_out_debug (MtcsDwarf2Out *self,die_struct *ptr);
//原型 dw_get_die_tag dwarf2out.h dwarf2out.cc
enum dwarf_tag mtcs_dwarf2_out_dw_get_die_tag (dw_die_ref die);
//原型 dw_get_die_child dwarf2out.h dwarf2out.cc
dw_die_ref mtcs_dwarf2_out_dw_get_die_child (dw_die_ref die);
//原型 dw_get_die_sib dwarf2out.h dwarf2out.cc
dw_die_ref mtcs_dwarf2_out_dw_dw_get_die_sib (dw_die_ref die);
//原型 dw_get_die_parent dwarf2out.h dwarf2out.cc
dw_die_ref mtcs_dwaf2_out_dw_get_die_parent (dw_die_ref die);

#endif
