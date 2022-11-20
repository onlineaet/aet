/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

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

#ifndef GCC_AET_TYPECK_H
#define GCC_AET_TYPECK_H


tree aet_require_complete_type (location_t loc, tree value);
void aet_c_incomplete_type_error (location_t loc, const_tree value, const_tree type);
tree aet_c_type_promotes_to (tree type);
bool aet_c_vla_type_p (const_tree t);
tree aet_composite_type (tree t1, tree t2);
tree aet_common_type (tree t1, tree t2); //no
int aet_comptypes (tree type1, tree type2);
int aet_comptypes_check_different_types (tree type1, tree type2,bool *different_types_p);//no
bool aet_same_translation_unit_p (const_tree t1, const_tree t2);
tree aet_decl_constant_value_1 (tree decl, bool in_init);
tree aet_decl_constant_value (tree decl);//no
void aet_mark_exp_read (tree exp);
struct c_expr aet_default_function_array_conversion (location_t loc, struct c_expr exp);//no
struct c_expr aet_default_function_array_read_conversion (location_t loc, struct c_expr exp);//no
struct c_expr aet_convert_lvalue_to_rvalue (location_t loc, struct c_expr exp, bool convert_p, bool read_p);//no
tree aet_perform_integral_promotions (tree exp);
tree aet_default_conversion (tree exp);
tree aet_build_component_ref (location_t loc, tree datum, tree component,location_t component_loc);//no
tree aet_build_indirect_ref (location_t loc, tree ptr, ref_operator errstring);//no
tree aet_build_array_ref (location_t loc, tree array, tree index);//no
tree aet_build_external_ref (location_t loc, tree id, bool fun, tree *type);//no
void aet_pop_maybe_used (bool used);//no
struct c_expr aet_c_expr_sizeof_expr (location_t loc, struct c_expr expr); //no
struct c_expr aet_c_expr_sizeof_type (location_t loc, struct c_type_name *t);//no
tree aet_build_function_call (location_t loc, tree function, tree params);//no
tree aet_build_function_call_vec (location_t, vec<location_t>, tree,vec<tree, va_gc> *, vec<tree, va_gc> *,void *userData);
tree aet_c_build_function_call_vec (location_t loc, vec<location_t> arg_loc,tree function, vec<tree, va_gc> *params,vec<tree, va_gc> *origtypes);//no
struct c_expr aet_parser_build_unary_op (location_t loc, enum tree_code code, struct c_expr arg);//no
struct c_expr aet_parser_build_binary_op (location_t location, enum tree_code code,struct c_expr arg1, struct c_expr arg2);//no
tree aet_build_unary_op (location_t location, enum tree_code code, tree xarg,bool noconvert);
bool aet_lvalue_p (const_tree ref);
bool aet_c_mark_addressable (tree, bool = false);
tree aet_build_conditional_expr (location_t colon_loc, tree ifexp, bool ifexp_bcp,
			tree op1, tree op1_original_type, location_t op1_loc,
			tree op2, tree op2_original_type, location_t op2_loc);//no
tree aet_build_compound_expr (location_t loc, tree expr1, tree expr2);//no
tree aet_build_c_cast (location_t loc, tree type, tree expr);
tree aet_c_cast_expr (location_t loc, struct c_type_name *type_name, tree expr);//no
tree aet_build_modify_expr (location_t location, tree lhs, tree lhs_origtype, enum tree_code modifycode,location_t rhs_loc, tree rhs, tree rhs_origtype);//no
void aet_maybe_warn_string_init (location_t loc, tree type, struct c_expr expr);
void aet_store_init_value (location_t init_loc, tree decl, tree init, tree origtype);//no
void aet_start_init (tree decl, tree asmspec_tree ATTRIBUTE_UNUSED, int top_level,rich_location *richloc);//no
void aet_finish_init (void);//no
void aet_really_start_incremental_init (tree type);//no
void aet_finish_implicit_inits (location_t loc, struct obstack *braced_init_obstack);//no
void aet_push_init_level (location_t loc, int implicit, struct obstack *braced_init_obstack);//no
struct c_expr aet_pop_init_level (location_t loc, int implicit,struct obstack *braced_init_obstack,location_t insert_before);//no
void aet_set_init_index (location_t loc, tree first, tree last,struct obstack *braced_init_obstack);//no
void aet_set_init_label (location_t loc, tree fieldname, location_t fieldname_loc,struct obstack *braced_init_obstack);//no
void aet_process_init_element (location_t loc, struct c_expr value, bool implicit,struct obstack * braced_init_obstack);//no
tree aet_build_asm_stmt (bool is_volatile, tree args);//no
tree aet_build_asm_expr (location_t loc, tree string, tree outputs, tree inputs,tree clobbers, tree labels, bool simple, bool is_inline);//no
tree aet_c_finish_goto_label (location_t loc, tree label);//no
tree aet_c_finish_goto_ptr (location_t loc, tree expr);//no
tree aet_c_finish_return (location_t loc, tree retval, tree origtype);//no
tree aet_c_start_case (location_t switch_loc,location_t switch_cond_loc,tree exp, bool explicit_cast_p);//no
tree aet_do_case (location_t loc, tree low_value, tree high_value);//no
void aet_c_finish_case (tree body, tree type);//no
void aet_c_finish_if_stmt (location_t if_locus, tree cond, tree then_block,tree else_block);//no
void aet_c_finish_loop (location_t start_locus, location_t cond_locus, tree cond,
	       location_t incr_locus, tree incr, tree body, tree blab,
	       tree clab, bool cond_is_first);//no
tree aet_c_finish_bc_stmt (location_t loc, tree *label_p, bool is_break);//no
tree aet_c_process_expr_stmt (location_t loc, tree expr);//no
tree aet_c_finish_expr_stmt (location_t loc, tree expr);//no
tree aet_c_begin_stmt_expr (void);//no
tree aet_c_finish_stmt_expr (location_t loc, tree body);//no
tree aet_c_begin_compound_stmt (bool do_scope);
tree aet_c_end_compound_stmt (location_t loc, tree stmt, bool do_scope);
void aet_push_cleanup (tree decl, tree cleanup, bool eh_only);//no
tree aet_build_binary_op (location_t location, enum tree_code code,tree orig_op0, tree orig_op1, bool convert_p);
tree aet_c_objc_common_truthvalue_conversion (location_t location, tree expr);
tree aet_c_expr_to_decl (tree expr, bool *tc ATTRIBUTE_UNUSED, bool *se);//no
tree aet_c_finish_omp_construct (location_t loc, enum tree_code code, tree body,tree clauses);//no
tree aet_c_finish_oacc_data (location_t loc, tree clauses, tree block);//no
tree aet_c_finish_oacc_host_data (location_t loc, tree clauses, tree block);//no
tree aet_c_begin_omp_parallel (void);//no
tree aet_c_finish_omp_parallel (location_t loc, tree clauses, tree block);//no
tree aet_c_begin_omp_task (void);//no
tree aet_c_finish_omp_task (location_t loc, tree clauses, tree block);//no
void aet_c_finish_omp_cancel (location_t loc, tree clauses);//no
void aet_c_finish_omp_cancellation_point (location_t loc, tree clauses);//no
tree aet_c_finish_omp_clauses (tree clauses, enum c_omp_region_type ort);//no
tree aet_c_omp_clause_copy_ctor (tree clause, tree dst, tree src);//no
tree aet_c_finish_transaction (location_t loc, tree block, int flags);//no
tree aet_c_build_qualified_type (tree, int, tree = NULL_TREE, size_t = 0);
tree aet_c_build_va_arg (location_t loc1, tree expr, location_t loc2, tree type);//no
bool aet_c_tree_equal (tree t1, tree t2);//no
bool aet_c_decl_implicit (const_tree fndecl);//no

/**
 * 检查函数参数类型与实参是否匹配
 * zclei 加的
 */
tree aet_check_funcs_param(location_t loc, vec<location_t> arg_loc,tree function, vec<tree, va_gc> *params,vec<tree, va_gc> *origtypes,void *userData);
tree aet_lookup_field(tree,tree);
tree aet_typeck_func_param_compare(location_t init_loc, tree type, tree init,int require_constant);


#endif /* ! GCC_AET_TYPECK_H */
