/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

   This program based on gcc/c-family/c-common.h and gcc/c-family/c-common.c.

This file is part of AET.

AET is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

AET is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC Exception along with this program; see the file COPYING3.
If not see <http://www.gnu.org/licenses/>.
AET was originally developed  by the zclei@sina.com at guiyang china .
*/

#ifndef GCC_AET_C_COMMON_H
#define GCC_AET_C_COMMON_H

bool aet_c_cpp_diagnostic (cpp_reader *pfile ATTRIBUTE_UNUSED,
		  enum cpp_diagnostic_level level,
		  enum cpp_warning_reason reason,
		  rich_location *richloc,
		  const char *msg, va_list *ap);//no

HOST_WIDE_INT aet_c_common_to_target_charset (HOST_WIDE_INT c);//no
const char *aet_c_get_substring_location (const substring_loc &substr_loc,location_t *out_loc);//no
const char *aet_fname_as_string (int pretty_p);//no
tree aet_fix_string_type (tree value);//no

const char aet_c_addr_space_name (addr_space_t as);//no
void aet_start_fname_decls (void);//no
void aet_finish_fname_decls (void);
tree aet_fname_decl (location_t loc, unsigned int rid, tree id);
bool aet_bool_promoted_to_int_p (tree t);
bool aet_vector_targets_convertible_p (const_tree t1, const_tree t2);
bool aet_vector_types_convertible_p (const_tree t1, const_tree t2, bool emit_lax_note);
tree aet_c_build_vec_perm_expr (location_t loc, tree v0, tree v1, tree mask, bool complain);
tree aet_c_build_vec_convert (location_t loc1, tree expr, location_t loc2, tree type, bool complain);
DEBUG_FUNCTION void aet_verify_sequence_points (tree expr);
tree aet_c_common_fixed_point_type_for_size (unsigned int ibit, unsigned int fbit, int unsignedp, int satp);
tree aet_c_build_bitfield_integer_type (unsigned HOST_WIDE_INT width, int unsignedp);
void aet_c_register_builtin_type (tree type, const char* name);
void aet_binary_op_error (rich_location *richloc, enum tree_code code,tree type0, tree type1);
tree aet_shorten_compare (location_t loc, tree *op0_ptr, tree *op1_ptr,tree *restype_ptr, enum tree_code *rescode_ptr);
tree aet_pointer_int_sum (location_t loc, enum tree_code resultcode, tree ptrop, tree intop, bool complain);
tree aet_c_wrap_maybe_const (tree expr, bool non_const);
void aet_c_apply_type_quals_to_decl (int type_quals, tree decl);
alias_set_type aet_c_common_get_alias_set (tree t);
tree aet_c_sizeof_or_alignof_type (location_t loc, tree type, bool is_sizeof, bool min_alignof, int complain);
tree aet_c_alignof_expr (location_t loc, tree expr);
void aet_c_common_nodes_and_builtins (void);
void aet_set_compound_literal_name (tree decl);
tree aet_build_va_arg (location_t loc, tree expr, tree type);
void aet_disable_builtin_function (const char *name);
bool aet_self_promoting_args_p (const_tree parms);
tree aet_strip_pointer_operator (tree t);
tree aet_strip_pointer_or_array_types (tree t);
int aet_case_compare (splay_tree_key k1, splay_tree_key k2);
tree aet_c_add_case_label (location_t loc, splay_tree cases, tree cond,tree low_value, tree high_value);
bool aet_c_switch_covers_all_cases_p (splay_tree cases, tree type);
tree aet_finish_label_address_expr (tree label, location_t loc);
tree aet_boolean_increment (enum tree_code code, tree arg);
void aet_c_stddef_cpp_builtins(void);
int aet_check_user_alignment (const_tree align, bool objfile, bool warn_zero);
bool aet_c_determine_visibility (tree decl);
bool aet_parse_optimize_options (tree args, bool attr_p);
bool aet_attribute_fallthrough_p (tree attr);
bool aet_check_function_arguments (location_t loc, const_tree fndecl, const_tree fntype, int nargs, tree *argarray, vec<location_t> *arglocs);

bool aet_check_builtin_function_arguments (location_t loc, vec<location_t> arg_loc,
				  tree fndecl, tree orig_fndecl,
				  int nargs, tree *args);

void aet_c_parse_error (const char *gmsgid, enum cpp_ttype token_type,
	       tree value, unsigned char token_flags,
	       rich_location *richloc);

void aet_complete_flexible_array_elts (tree init);

int aet_complete_array_type (tree *ptype, tree initial_value, bool do_default);
tree aet_fold_offsetof (tree expr, tree type, enum tree_code ctx);
bool aet_vector_types_compatible_elements_p (tree t1, tree t2);
bool aet_check_missing_format_attribute (tree ltype, tree rtype);
void aet_set_underlying_type (tree x);
bool aet_user_facing_original_type_p (const_tree type);
void aet_record_types_used_by_current_var_decl (tree decl);
void aet_release_tree_vector (vec<tree, va_gc> *vec);
vec<tree, va_gc> * aet_make_tree_vector_single (tree t);
vec<tree, va_gc> * aet_make_tree_vector_from_list (tree list);
vec<tree, va_gc> * aet_make_tree_vector_from_ctor (tree ctor);
vec<tree, va_gc> *aet_make_tree_vector_copy (const vec<tree, va_gc> *orig);
bool aet_keyword_begins_type_specifier (enum rid keyword);
vec<tree, va_gc> * aet_make_tree_vector (void);
bool aet_keyword_is_decl_specifier (enum rid keyword);
void aet_c_common_init_ts (void);
tree aet_build_userdef_literal (tree suffix_id, tree value,enum overflow_type overflow, tree num_string);
bool aet_convert_vector_to_array_for_subscript (location_t loc,tree *vecp, tree index);
enum stv_conv aet_scalar_to_vector (location_t loc, enum tree_code code, tree op0, tree op1, bool complain);
bool aet_cxx_fundamental_alignment_p (unsigned align);
bool aet_pointer_to_zero_sized_aggr_p (tree t);
bool aet_reject_gcc_builtin (const_tree expr, location_t loc /* = UNKNOWN_LOCATION */);
void aet_invalid_array_size_error (location_t loc, cst_size_error error, const_tree size, const_tree name);
bool aet_valid_array_size_p (location_t loc, const_tree t, tree name, bool complain);
time_t aet_cb_get_source_date_epoch (cpp_reader *pfile ATTRIBUTE_UNUSED);
const char *aet_cb_get_suggestion (cpp_reader *, const char *goal, const char *const *candidates);
int aet_c_flt_eval_method (bool maybe_c11_only_p);
void aet_maybe_suggest_missing_token_insertion (rich_location *richloc,
				       enum cpp_ttype token_type,
				       location_t prev_token_loc);

void aet_maybe_add_include_fixit (rich_location *richloc, const char *header, bool override_location);
tree aet_braced_lists_to_strings (tree type, tree ctor);
enum flt_eval_method aet_excess_precision_mode_join (enum flt_eval_method x, enum flt_eval_method y);
unsigned aet_max_align_t_align ();
bool aet_keyword_is_storage_class_specifier (enum rid keyword);
bool aet_keyword_is_type_qualifier (enum rid keyword);

void aet_c_common_mark_addressable_vec (tree t);
tree aet_c_common_get_narrower (tree op, int *unsignedp_ptr);
bool aet_get_attribute_operand (tree arg_num_expr, unsigned HOST_WIDE_INT *valp);

void aet_check_function_arguments_recurse (void (*callback)
				  (void *, tree, unsigned HOST_WIDE_INT),
				  void *ctx, tree param,
				  unsigned HOST_WIDE_INT param_num);

tree aet_c_common_truthvalue_conversion (location_t location, tree expr);
bool aet_c_promoting_integer_type_p (const_tree t);
tree aet_c_common_signed_type (tree type);
tree aet_c_common_unsigned_type (tree type);
tree aet_c_common_type_for_mode (machine_mode mode, int unsignedp);
tree aet_c_common_signed_or_unsigned_type (int unsignedp, tree type);

//////////////////////////

tree aet_resolve_overloaded_builtin (location_t loc, tree function, vec<tree, va_gc> *params);
tree aet_c_common_type_for_size (unsigned int bits, int unsignedp);
tree aet_builtin_type_for_size (int size, bool unsignedp);



#endif /* ! GCC_AET_TYPECK_H */
