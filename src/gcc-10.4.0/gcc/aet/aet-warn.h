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

#ifndef GCC_AET_WARN_H
#define GCC_AET_WARN_H


void aet_warn_for_address_or_pointer_of_packed_member (tree type, tree rhs);
void aet_warn_for_multistatement_macros (location_t body_loc, location_t next_loc,location_t guard_loc, enum rid keyword);//no
tree aet_do_warn_duplicated_branches_r (tree *tp, int *, void *);//no
bool aet_warn_for_restrict (unsigned param_pos, tree *argarray, unsigned nargs);
void aet_maybe_warn_bool_compare (location_t loc, enum tree_code code, tree op0,tree op1);//no
bool aet_maybe_warn_shift_overflow (location_t loc, tree op0, tree op1);
bool aet_diagnose_mismatched_attributes (tree olddecl, tree newdecl);
void aet_warn_duplicated_cond_add_or_warn (location_t loc, tree cond, vec<tree> **chain);
void aet_maybe_warn_unused_local_typedefs (void);
void aet_maybe_record_typedef_use (tree t);
void aet_record_locally_defined_typedef (tree decl);
void aet_do_warn_unused_parameter (tree fn);
void aet_do_warn_double_promotion (tree result_type, tree type1, tree type2,const char *gmsgid, location_t loc);
void aet_warn_for_sign_compare (location_t location,
		       tree orig_op0, tree orig_op1,
		       tree op0, tree op1,
		       tree result_type, enum tree_code resultcode);
void aet_warn_for_memset (location_t loc, tree arg0, tree arg2, int literal_zero_mask);
void aet_warn_for_div_by_zero (location_t loc, tree divisor);
void aet_warn_for_unused_label (tree label);
void aet_warn_about_parentheses (location_t loc, enum tree_code code,
			enum tree_code code_left, tree arg_left,
			enum tree_code code_right, tree arg_right);
void aet_warn_array_subscript_with_type_char (location_t loc, tree index);
void aet_invalid_indirection_error (location_t loc, tree type, ref_operator errstring);
void aet_lvalue_error (location_t loc, enum lvalue_use use);
void aet_c_do_switch_warnings (splay_tree cases, location_t switch_location,tree type, tree cond, bool bool_cond_p);
void aet_warn_for_omitted_condop (location_t location, tree cond);
void aet_readonly_error (location_t loc, tree arg, enum lvalue_use use);
void aet_warnings_for_convert_and_check (location_t loc, tree type, tree expr,tree result);
void aet_check_main_parameter_types (tree decl);

void aet_sizeof_pointer_memaccess_warning (location_t *sizeof_arg_loc, tree callee,
				  vec<tree, va_gc> *params, tree *sizeof_arg,
				  bool (*comp_types) (tree, tree));

bool aet_strict_aliasing_warning (location_t loc, tree type, tree expr);
bool aet_warn_if_unused_value (const_tree exp, location_t locus);
void aet_warn_logical_not_parentheses (location_t location, enum tree_code code,tree lhs, tree rhs);
void aet_warn_tautological_cmp (const op_location_t &loc, enum tree_code code,tree lhs, tree rhs);
void aet_warn_logical_operator (location_t location, enum tree_code code, tree type,
		       enum tree_code code_left, tree op_left,
		       enum tree_code ARG_UNUSED (code_right), tree op_right);

void aet_overflow_warning (location_t loc, tree value, tree expr);
void aet_constant_expression_warning (tree value);
void aet_constant_expression_error (tree value);

#endif /* ! GCC_AET_TYPECK_H */
