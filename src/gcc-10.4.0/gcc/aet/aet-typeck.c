/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

   This program based on gcc/c/c-typeck.c

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

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "memmodel.h"
#include "target.h"
#include "function.h"
#include "bitmap.h"
#include "c/c-tree.h"
#include "gimple-expr.h"
#include "predict.h"
#include "stor-layout.h"
#include "trans-mem.h"
#include "varasm.h"
#include "stmt.h"
#include "langhooks.h"
#include "intl.h"
#include "tree-iterator.h"
#include "gimplify.h"
#include "tree-inline.h"
#include "omp-general.h"
#include "c-family/c-objc.h"
#include "c-family/c-ubsan.h"
#include "gomp-constants.h"
#include "spellcheck-tree.h"
#include "gcc-rich-location.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "ubsan.h"
#include "c/c-lang.h"
#include "aet-typeck.h"
#include "aet-c-common.h"
#include "c-aet.h"
#include "aet-c-fold.h"
#include "aet-convert.h"
#include "genericcall.h"
#include "genericutil.h"
#include "classutil.h"
#include "aetmicro.h"



WarnAndError argsFuncsInfo;
/* Possible cases of implicit bad conversions.  Used to select
   diagnostic messages in convert_for_assignment.  */
enum impl_conv {
  ic_argpass,
  ic_assign,
  ic_init,
  ic_return
};
static void printNode(tree node)
{
	   if(!n_log_is_debug())
		   return;
      if(!node || node==NULL_TREE){
    	  printf("printNode tree is null tree\n");
    	  return;
      }
	  int i;
	  FILE *dump_orig;
	  dump_flags_t local_dump_flags;
	  dump_orig = get_dump_info (TDI_original, &local_dump_flags);
	  dump_orig = dump_begin (TDI_original, &local_dump_flags);
	  if(!dump_orig)
		  dump_orig=stderr;
	  dump_node(node,TDF_ALL_VALUES|local_dump_flags,dump_orig);
}


/* The argument of last parsed sizeof expression, only to be tested
   if expr.original_code == SIZEOF_EXPR.  */
//tree c_last_sizeof_arg;
//location_t c_last_sizeof_loc;

/* Nonzero if we might need to print a "missing braces around
   initializer" message within this initializer.  */
static int found_missing_braces;

static int require_constant_value;
static int require_constant_elements;

static bool  null_pointer_constant_p (const_tree);
static tree  qualify_type (tree, tree);
static int   tagged_types_tu_compatible_p (const_tree, const_tree, bool *, bool *);
static int   comp_target_types (location_t, tree, tree);
static int   function_types_compatible_p (const_tree, const_tree, bool *,bool *);
static int   type_lists_compatible_p (const_tree, const_tree, bool *, bool *);
static tree  lookup_field (tree, tree);
static int   convert_arguments (location_t, vec<location_t>, tree,vec<tree, va_gc> *, vec<tree, va_gc> *, tree,tree,void *userData);
static tree  pointer_diff (location_t, tree, tree, tree *);
static tree  convert_for_assignment (location_t, location_t, tree, tree, tree,enum impl_conv, bool, tree, tree, int,int = 0);
static tree  valid_compound_expr_initializer (tree, tree);
static int   spelling_length (void);
static char *print_spelling (char *);
static void  warning_init (location_t, int, const char *);
static tree  digest_init (location_t, tree, tree, tree, bool, bool, int);
static void  output_init_element (location_t, tree, tree, bool, tree, tree, bool,bool, struct obstack *);
static void  output_pending_init_elements (int, struct obstack *);
static void  add_pending_init (location_t, tree, tree, tree, bool,struct obstack *);
static void  set_nonincremental_init (struct obstack *);
static void  readonly_warning (tree, enum lvalue_use);
static int   lvalue_or_else (location_t, const_tree, enum lvalue_use);
static int   comptypes_internal (const_tree, const_tree, bool *, bool *);

//zclei
static bool  aet_reject_gcc_builtin (const_tree expr, location_t loc /* = UNKNOWN_LOCATION */);

/* Return true if EXP is a null pointer constant, false otherwise.  */

static bool null_pointer_constant_p (const_tree expr)
{
  /* This should really operate on c_expr structures, but they aren't
     yet available everywhere required.  */
  tree type = TREE_TYPE (expr);
  return (TREE_CODE (expr) == INTEGER_CST
	  && !TREE_OVERFLOW (expr)
	  && integer_zerop (expr)
	  && (INTEGRAL_TYPE_P (type)
	      || (TREE_CODE (type) == POINTER_TYPE
		  && VOID_TYPE_P (TREE_TYPE (type))
		  && TYPE_QUALS (TREE_TYPE (type)) == TYPE_UNQUALIFIED)));
}

/* EXPR may appear in an unevaluated part of an integer constant
   expression, but not in an evaluated part.  Wrap it in a
   C_MAYBE_CONST_EXPR, or mark it with TREE_OVERFLOW if it is just an
   INTEGER_CST and we cannot create a C_MAYBE_CONST_EXPR.  */

static tree note_integer_operands (tree expr)
{
  tree ret;
  if (TREE_CODE (expr) == INTEGER_CST && in_late_binary_op)
    {
      ret = copy_node (expr);
      TREE_OVERFLOW (ret) = 1;
    }
  else
    {
      ret = build2 (C_MAYBE_CONST_EXPR, TREE_TYPE (expr), NULL_TREE, expr);
      C_MAYBE_CONST_EXPR_INT_OPERANDS (ret) = 1;
    }
  return ret;
}

/* Having checked whether EXPR may appear in an unevaluated part of an
   integer constant expression and found that it may, remove any
   C_MAYBE_CONST_EXPR noting this fact and return the resulting
   expression.  */

static inline tree remove_c_maybe_const_expr (tree expr)
{
  if (TREE_CODE (expr) == C_MAYBE_CONST_EXPR)
    return C_MAYBE_CONST_EXPR_EXPR (expr);
  else
    return expr;
}

/* This is a cache to hold if two types are compatible or not.  */

struct tagged_tu_seen_cache {
  const struct tagged_tu_seen_cache * next;
  const_tree t1;
  const_tree t2;
  /* The return value of tagged_types_tu_compatible_p if we had seen
     these two types already.  */
  int val;
};

static const struct tagged_tu_seen_cache * tagged_tu_seen_base;
static void free_all_tagged_tu_seen_up_to (const struct tagged_tu_seen_cache *);

/* Do `exp = require_complete_type (loc, exp);' to make sure exp
   does not have an incomplete type.  (That includes void types.)
   LOC is the location of the use.  */

tree aet_require_complete_type (location_t loc, tree value)
{
  tree type = TREE_TYPE (value);

  if (error_operand_p (value))
    return error_mark_node;
  /* First, detect a valid value with a complete type.  */
  //printf("value is xxx 00 %p %s\n",type,(type!=NULL_TREE && type && type!=error_mark_node)?get_tree_code_name(TREE_CODE(type)):"null");
  if (COMPLETE_TYPE_P (type))
    return value;
  printf("aet_require_complete_type 出错了 value is xxx 11 %p %s\n",
		  type,(type!=NULL_TREE && type && type!=error_mark_node)?get_tree_code_name(TREE_CODE(type)):"null");

  aet_c_incomplete_type_error (loc, value, type);
  return error_mark_node;
}

/* Print an error message for invalid use of an incomplete type.
   VALUE is the expression that was used (or 0 if that isn't known)
   and TYPE is the type that was invalid.  LOC is the location for
   the error.  */

void aet_c_incomplete_type_error (location_t loc, const_tree value, const_tree type)
{
  /* Avoid duplicate error message.  */
  if (TREE_CODE (type) == ERROR_MARK)
    return;

  if (value != NULL_TREE && (VAR_P (value) || TREE_CODE (value) == PARM_DECL)){
    //zcleierror_at (loc, "%qD has an incomplete type %qT", value, type);
    argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=has_an_incomplete_type;

  }else{
    retry:
      /* We must print an error message.  Be clever about what it says.  */

      switch (TREE_CODE (type))
	{
	case RECORD_TYPE:
	case UNION_TYPE:
	case ENUMERAL_TYPE:
	  break;

	case VOID_TYPE:
		//zcleierror_at (loc, "invalid use of void expression");
	    argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=invalid_use_of_void_expression;

	  return;

	case ARRAY_TYPE:
	  if (TYPE_DOMAIN (type))
	    {
	      if (TYPE_MAX_VALUE (TYPE_DOMAIN (type)) == NULL)
		{
	    	  //zcleierror_at (loc, "invalid use of flexible array member");
	  	    argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=invalid_use_of_flexible_array_member;

		  return;
		}
	      type = TREE_TYPE (type);
	      goto retry;
	    }
	  //zcleierror_at (loc, "invalid use of array with unspecified bounds");
	    argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=invalid_use_of_array_with_unspecified_bounds;

	  return;

	default:
	  gcc_unreachable ();
	}

      if (TREE_CODE (TYPE_NAME (type)) == IDENTIFIER_NODE)
    	  //zcleierror_at (loc, "invalid use of undefined type %qT", type);
  	    argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=invalid_use_of_undefined_type;

      else
	/* If this type has a typedef-name, the TYPE_NAME is a TYPE_DECL.  */
    	  //zcleierror_at (loc, "invalid use of incomplete typedef %qT", type);
  	    argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=invalid_use_of_incomplete_typedef;

    }
}

/* Given a type, apply default promotions wrt unnamed function
   arguments and return the new type.  */

tree aet_c_type_promotes_to (tree type)
{
    tree ret = NULL_TREE;
    if (TYPE_MAIN_VARIANT (type) == float_type_node){
    	printf("aet_c_type_promotes_to -TYPE_MAIN_VARIANT (type) == float_type_node\n");
      ret = double_type_node;
    }else if (c_promoting_integer_type_p (type)){
    	printf("aet_c_type_promotes_to c_promoting_integer_type_p\n");

      /* Preserve unsignedness if not really getting any wider.  */
       if (TYPE_UNSIGNED (type) && (TYPE_PRECISION (type) == TYPE_PRECISION (integer_type_node)))
	      ret = unsigned_type_node;
       else
	      ret = integer_type_node;
    }

    if (ret != NULL_TREE)
       return (TYPE_ATOMIC (type)? aet_c_build_qualified_type (ret, TYPE_QUAL_ATOMIC): ret);
    return type;
}

/* Return true if between two named address spaces, whether there is a superset
   named address space that encompasses both address spaces.  If there is a
   superset, return which address space is the superset.  */

static bool addr_space_superset (addr_space_t as1, addr_space_t as2, addr_space_t *common)
{
  if (as1 == as2)
    {
      *common = as1;
      return true;
    }
  else if (targetm.addr_space.subset_p (as1, as2))
    {
      *common = as2;
      return true;
    }
  else if (targetm.addr_space.subset_p (as2, as1))
    {
      *common = as1;
      return true;
    }
  else
    return false;
}

/* Return a variant of TYPE which has all the type qualifiers of LIKE
   as well as those of TYPE.  */

static tree qualify_type (tree type, tree like)
{
  addr_space_t as_type = TYPE_ADDR_SPACE (type);
  addr_space_t as_like = TYPE_ADDR_SPACE (like);
  addr_space_t as_common;

  /* If the two named address spaces are different, determine the common
     superset address space.  If there isn't one, raise an error.  */
  if (!addr_space_superset (as_type, as_like, &as_common))
    {
      as_common = as_type;
      error ("%qT and %qT are in disjoint named address spaces",type, like);
    }

  return aet_c_build_qualified_type (type,
				 TYPE_QUALS_NO_ADDR_SPACE (type)
				 | TYPE_QUALS_NO_ADDR_SPACE_NO_ATOMIC (like)
				 | ENCODE_QUAL_ADDR_SPACE (as_common));
}

/* Return true iff the given tree T is a variable length array.  */

bool aet_c_vla_type_p (const_tree t)
{
  if (TREE_CODE (t) == ARRAY_TYPE
      && C_TYPE_VARIABLE_SIZE (t))
    return true;
  return false;
}

/* If NTYPE is a type of a non-variadic function with a prototype
   and OTYPE is a type of a function without a prototype and ATTRS
   contains attribute format, diagnosess and removes it from ATTRS.
   Returns the result of build_type_attribute_variant of NTYPE and
   the (possibly) modified ATTRS.  */

static tree build_functype_attribute_variant (tree ntype, tree otype, tree attrs)
{
  if (!prototype_p (otype)
      && prototype_p (ntype)
      && lookup_attribute ("format", attrs))
    {
      warning_at (input_location, OPT_Wattributes,
		  "%qs attribute cannot be applied to a function that "
		  "does not take variable arguments", "format");
      attrs = remove_attribute ("format", attrs);
    }
  return build_type_attribute_variant (ntype, attrs);

}
/* Return the composite type of two compatible types.

   We assume that comptypes has already been done and returned
   nonzero; if that isn't so, this may crash.  In particular, we
   assume that qualifiers match.  */

tree aet_composite_type (tree t1, tree t2)
{
  enum tree_code code1;
  enum tree_code code2;
  tree attributes;

  /* Save time if the two types are the same.  */

  if (t1 == t2) return t1;

  /* If one type is nonsense, use the other.  */
  if (t1 == error_mark_node)
    return t2;
  if (t2 == error_mark_node)
    return t1;

  code1 = TREE_CODE (t1);
  code2 = TREE_CODE (t2);

  /* Merge the attributes.  */
  attributes = targetm.merge_type_attributes (t1, t2);

  /* If one is an enumerated type and the other is the compatible
     integer type, the composite type might be either of the two
     (DR#013 question 3).  For consistency, use the enumerated type as
     the composite type.  */

  if (code1 == ENUMERAL_TYPE && code2 == INTEGER_TYPE)
    return t1;
  if (code2 == ENUMERAL_TYPE && code1 == INTEGER_TYPE)
    return t2;

  gcc_assert (code1 == code2);

  switch (code1)
    {
    case POINTER_TYPE:
      /* For two pointers, do this recursively on the target type.  */
      {
	tree pointed_to_1 = TREE_TYPE (t1);
	tree pointed_to_2 = TREE_TYPE (t2);
	tree target = aet_composite_type (pointed_to_1, pointed_to_2);
        t1 = build_pointer_type_for_mode (target, TYPE_MODE (t1), false);
	t1 = build_type_attribute_variant (t1, attributes);
	return qualify_type (t1, t2);
      }

    case ARRAY_TYPE:
      {
	tree elt = aet_composite_type (TREE_TYPE (t1), TREE_TYPE (t2));
	int quals;
	tree unqual_elt;
	tree d1 = TYPE_DOMAIN (t1);
	tree d2 = TYPE_DOMAIN (t2);
	bool d1_variable, d2_variable;
	bool d1_zero, d2_zero;
	bool t1_complete, t2_complete;

	/* We should not have any type quals on arrays at all.  */
	gcc_assert (!TYPE_QUALS_NO_ADDR_SPACE (t1)
		    && !TYPE_QUALS_NO_ADDR_SPACE (t2));

	t1_complete = COMPLETE_TYPE_P (t1);
	t2_complete = COMPLETE_TYPE_P (t2);

	d1_zero = d1 == NULL_TREE || !TYPE_MAX_VALUE (d1);
	d2_zero = d2 == NULL_TREE || !TYPE_MAX_VALUE (d2);

	d1_variable = (!d1_zero
		       && (TREE_CODE (TYPE_MIN_VALUE (d1)) != INTEGER_CST
			   || TREE_CODE (TYPE_MAX_VALUE (d1)) != INTEGER_CST));
	d2_variable = (!d2_zero
		       && (TREE_CODE (TYPE_MIN_VALUE (d2)) != INTEGER_CST
			   || TREE_CODE (TYPE_MAX_VALUE (d2)) != INTEGER_CST));
	d1_variable = d1_variable || (d1_zero && aet_c_vla_type_p (t1));
	d2_variable = d2_variable || (d2_zero && aet_c_vla_type_p (t2));

	/* Save space: see if the result is identical to one of the args.  */
	if (elt == TREE_TYPE (t1) && TYPE_DOMAIN (t1)
	    && (d2_variable || d2_zero || !d1_variable))
	  return build_type_attribute_variant (t1, attributes);
	if (elt == TREE_TYPE (t2) && TYPE_DOMAIN (t2)
	    && (d1_variable || d1_zero || !d2_variable))
	  return build_type_attribute_variant (t2, attributes);

	if (elt == TREE_TYPE (t1) && !TYPE_DOMAIN (t2) && !TYPE_DOMAIN (t1))
	  return build_type_attribute_variant (t1, attributes);
	if (elt == TREE_TYPE (t2) && !TYPE_DOMAIN (t2) && !TYPE_DOMAIN (t1))
	  return build_type_attribute_variant (t2, attributes);

	/* Merge the element types, and have a size if either arg has
	   one.  We may have qualifiers on the element types.  To set
	   up TYPE_MAIN_VARIANT correctly, we need to form the
	   composite of the unqualified types and add the qualifiers
	   back at the end.  */
	quals = TYPE_QUALS (strip_array_types (elt));
	unqual_elt = aet_c_build_qualified_type (elt, TYPE_UNQUALIFIED);
	t1 = build_array_type (unqual_elt,
			       TYPE_DOMAIN ((TYPE_DOMAIN (t1)
					     && (d2_variable
						 || d2_zero
						 || !d1_variable))
					    ? t1
					    : t2));
	/* Ensure a composite type involving a zero-length array type
	   is a zero-length type not an incomplete type.  */
	if (d1_zero && d2_zero
	    && (t1_complete || t2_complete)
	    && !COMPLETE_TYPE_P (t1))
	  {
	    TYPE_SIZE (t1) = bitsize_zero_node;
	    TYPE_SIZE_UNIT (t1) = size_zero_node;
	  }
	t1 = aet_c_build_qualified_type (t1, quals);
	return build_type_attribute_variant (t1, attributes);
      }

    case ENUMERAL_TYPE:
    case RECORD_TYPE:
    case UNION_TYPE:
      if (attributes != NULL)
	{
	  /* Try harder not to create a new aggregate type.  */
	  if (attribute_list_equal (TYPE_ATTRIBUTES (t1), attributes))
	    return t1;
	  if (attribute_list_equal (TYPE_ATTRIBUTES (t2), attributes))
	    return t2;
	}
      return build_type_attribute_variant (t1, attributes);

    case FUNCTION_TYPE:
      /* Function types: prefer the one that specified arg types.
	 If both do, merge the arg types.  Also merge the return types.  */
      {
	tree valtype = aet_composite_type (TREE_TYPE (t1), TREE_TYPE (t2));
	tree p1 = TYPE_ARG_TYPES (t1);
	tree p2 = TYPE_ARG_TYPES (t2);
	int len;
	tree newargs, n;
	int i;

	/* Save space: see if the result is identical to one of the args.  */
	if (valtype == TREE_TYPE (t1) && !TYPE_ARG_TYPES (t2))
	  return build_functype_attribute_variant (t1, t2, attributes);
	if (valtype == TREE_TYPE (t2) && !TYPE_ARG_TYPES (t1))
	  return build_functype_attribute_variant (t2, t1, attributes);

	/* Simple way if one arg fails to specify argument types.  */
	if (TYPE_ARG_TYPES (t1) == NULL_TREE)
	 {
	    t1 = build_function_type (valtype, TYPE_ARG_TYPES (t2));
	    t1 = build_type_attribute_variant (t1, attributes);
	    return qualify_type (t1, t2);
	 }
	if (TYPE_ARG_TYPES (t2) == NULL_TREE)
	 {
	   t1 = build_function_type (valtype, TYPE_ARG_TYPES (t1));
	   t1 = build_type_attribute_variant (t1, attributes);
	   return qualify_type (t1, t2);
	 }

	/* If both args specify argument types, we must merge the two
	   lists, argument by argument.  */

	for (len = 0, newargs = p1;
	     newargs && newargs != void_list_node;
	     len++, newargs = TREE_CHAIN (newargs))
	  ;

	for (i = 0; i < len; i++)
	  newargs = tree_cons (NULL_TREE, NULL_TREE, newargs);

	n = newargs;

	for (; p1 && p1 != void_list_node;
	     p1 = TREE_CHAIN (p1), p2 = TREE_CHAIN (p2), n = TREE_CHAIN (n))
	  {
	    /* A null type means arg type is not specified.
	       Take whatever the other function type has.  */
	    if (TREE_VALUE (p1) == NULL_TREE)
	      {
		TREE_VALUE (n) = TREE_VALUE (p2);
		goto parm_done;
	      }
	    if (TREE_VALUE (p2) == NULL_TREE)
	      {
		TREE_VALUE (n) = TREE_VALUE (p1);
		goto parm_done;
	      }

	    /* Given  wait (union {union wait *u; int *i} *)
	       and  wait (union wait *),
	       prefer  union wait *  as type of parm.  */
	    if (TREE_CODE (TREE_VALUE (p1)) == UNION_TYPE
		&& TREE_VALUE (p1) != TREE_VALUE (p2))
	      {
		tree memb;
		tree mv2 = TREE_VALUE (p2);
		if (mv2 && mv2 != error_mark_node
		    && TREE_CODE (mv2) != ARRAY_TYPE)
		  mv2 = TYPE_MAIN_VARIANT (mv2);
		for (memb = TYPE_FIELDS (TREE_VALUE (p1));
		     memb; memb = DECL_CHAIN (memb))
		  {
		    tree mv3 = TREE_TYPE (memb);
		    if (mv3 && mv3 != error_mark_node
			&& TREE_CODE (mv3) != ARRAY_TYPE)
		      mv3 = TYPE_MAIN_VARIANT (mv3);
		    if (aet_comptypes (mv3, mv2))
		      {
			TREE_VALUE (n) = aet_composite_type (TREE_TYPE (memb),
							 TREE_VALUE (p2));
			pedwarn (input_location, OPT_Wpedantic,
				 "function types not truly compatible in ISO C");
			goto parm_done;
		      }
		  }
	      }
	    if (TREE_CODE (TREE_VALUE (p2)) == UNION_TYPE
		&& TREE_VALUE (p2) != TREE_VALUE (p1))
	      {
		tree memb;
		tree mv1 = TREE_VALUE (p1);
		if (mv1 && mv1 != error_mark_node
		    && TREE_CODE (mv1) != ARRAY_TYPE)
		  mv1 = TYPE_MAIN_VARIANT (mv1);
		for (memb = TYPE_FIELDS (TREE_VALUE (p2));
		     memb; memb = DECL_CHAIN (memb))
		  {
		    tree mv3 = TREE_TYPE (memb);
		    if (mv3 && mv3 != error_mark_node
			&& TREE_CODE (mv3) != ARRAY_TYPE)
		      mv3 = TYPE_MAIN_VARIANT (mv3);
		    if (aet_comptypes (mv3, mv1))
		      {
			TREE_VALUE (n) = aet_composite_type (TREE_TYPE (memb),
							 TREE_VALUE (p1));
			pedwarn (input_location, OPT_Wpedantic,
				 "function types not truly compatible in ISO C");
			goto parm_done;
		      }
		  }
	      }
	    TREE_VALUE (n) = aet_composite_type (TREE_VALUE (p1), TREE_VALUE (p2));
	  parm_done: ;
	  }

	t1 = build_function_type (valtype, newargs);
	t1 = qualify_type (t1, t2);
      }
      /* FALLTHRU */

    default:
      return build_type_attribute_variant (t1, attributes);
    }

}

/* Return the type of a conditional expression between pointers to
   possibly differently qualified versions of compatible types.

   We assume that comp_target_types has already been done and returned
   nonzero; if that isn't so, this may crash.  */

static tree common_pointer_type (tree t1, tree t2)
{
  tree attributes;
  tree pointed_to_1, mv1;
  tree pointed_to_2, mv2;
  tree target;
  unsigned target_quals;
  addr_space_t as1, as2, as_common;
  int quals1, quals2;

  /* Save time if the two types are the same.  */

  if (t1 == t2) return t1;

  /* If one type is nonsense, use the other.  */
  if (t1 == error_mark_node)
    return t2;
  if (t2 == error_mark_node)
    return t1;

  gcc_assert (TREE_CODE (t1) == POINTER_TYPE
	      && TREE_CODE (t2) == POINTER_TYPE);

  /* Merge the attributes.  */
  attributes = targetm.merge_type_attributes (t1, t2);

  /* Find the composite type of the target types, and combine the
     qualifiers of the two types' targets.  Do not lose qualifiers on
     array element types by taking the TYPE_MAIN_VARIANT.  */
  mv1 = pointed_to_1 = TREE_TYPE (t1);
  mv2 = pointed_to_2 = TREE_TYPE (t2);
  if (TREE_CODE (mv1) != ARRAY_TYPE)
    mv1 = TYPE_MAIN_VARIANT (pointed_to_1);
  if (TREE_CODE (mv2) != ARRAY_TYPE)
    mv2 = TYPE_MAIN_VARIANT (pointed_to_2);
  target = aet_composite_type (mv1, mv2);

  /* Strip array types to get correct qualifier for pointers to arrays */
  quals1 = TYPE_QUALS_NO_ADDR_SPACE (strip_array_types (pointed_to_1));
  quals2 = TYPE_QUALS_NO_ADDR_SPACE (strip_array_types (pointed_to_2));

  /* For function types do not merge const qualifiers, but drop them
     if used inconsistently.  The middle-end uses these to mark const
     and noreturn functions.  */
  if (TREE_CODE (pointed_to_1) == FUNCTION_TYPE)
    target_quals = (quals1 & quals2);
  else
    target_quals = (quals1 | quals2);

  /* If the two named address spaces are different, determine the common
     superset address space.  This is guaranteed to exist due to the
     assumption that comp_target_type returned non-zero.  */
  as1 = TYPE_ADDR_SPACE (pointed_to_1);
  as2 = TYPE_ADDR_SPACE (pointed_to_2);
  if (!addr_space_superset (as1, as2, &as_common))
    gcc_unreachable ();

  target_quals |= ENCODE_QUAL_ADDR_SPACE (as_common);

  t1 = build_pointer_type (aet_c_build_qualified_type (target, target_quals));
  return build_type_attribute_variant (t1, attributes);
}

/* Return the common type for two arithmetic types under the usual
   arithmetic conversions.  The default conversions have already been
   applied, and enumerated types converted to their compatible integer
   types.  The resulting type is unqualified and has no attributes.

   This is the type for the result of most arithmetic operations
   if the operands have the given two types.  */

static tree c_common_type (tree t1, tree t2)
{
  enum tree_code code1;
  enum tree_code code2;

  /* If one type is nonsense, use the other.  */
  if (t1 == error_mark_node)
    return t2;
  if (t2 == error_mark_node)
    return t1;

  if (TYPE_QUALS (t1) != TYPE_UNQUALIFIED)
    t1 = TYPE_MAIN_VARIANT (t1);

  if (TYPE_QUALS (t2) != TYPE_UNQUALIFIED)
    t2 = TYPE_MAIN_VARIANT (t2);

  if (TYPE_ATTRIBUTES (t1) != NULL_TREE)
    t1 = build_type_attribute_variant (t1, NULL_TREE);

  if (TYPE_ATTRIBUTES (t2) != NULL_TREE)
    t2 = build_type_attribute_variant (t2, NULL_TREE);

  /* Save time if the two types are the same.  */

  if (t1 == t2) return t1;

  code1 = TREE_CODE (t1);
  code2 = TREE_CODE (t2);

  gcc_assert (code1 == VECTOR_TYPE || code1 == COMPLEX_TYPE
	      || code1 == FIXED_POINT_TYPE || code1 == REAL_TYPE
	      || code1 == INTEGER_TYPE);
  gcc_assert (code2 == VECTOR_TYPE || code2 == COMPLEX_TYPE
	      || code2 == FIXED_POINT_TYPE || code2 == REAL_TYPE
	      || code2 == INTEGER_TYPE);

  /* When one operand is a decimal float type, the other operand cannot be
     a generic float type or a complex type.  We also disallow vector types
     here.  */
  if ((DECIMAL_FLOAT_TYPE_P (t1) || DECIMAL_FLOAT_TYPE_P (t2))
      && !(DECIMAL_FLOAT_TYPE_P (t1) && DECIMAL_FLOAT_TYPE_P (t2)))
    {
      if (code1 == VECTOR_TYPE || code2 == VECTOR_TYPE)
	{
	  error ("cannot mix operands of decimal floating and vector types");
	  return error_mark_node;
	}
      if (code1 == COMPLEX_TYPE || code2 == COMPLEX_TYPE)
	{
	  error ("cannot mix operands of decimal floating and complex types");
	  return error_mark_node;
	}
      if (code1 == REAL_TYPE && code2 == REAL_TYPE)
	{
	  error ("cannot mix operands of decimal floating "
		 "and other floating types");
	  return error_mark_node;
	}
    }

  /* If one type is a vector type, return that type.  (How the usual
     arithmetic conversions apply to the vector types extension is not
     precisely specified.)  */
  if (code1 == VECTOR_TYPE)
    return t1;

  if (code2 == VECTOR_TYPE)
    return t2;

  /* If one type is complex, form the common type of the non-complex
     components, then make that complex.  Use T1 or T2 if it is the
     required type.  */
  if (code1 == COMPLEX_TYPE || code2 == COMPLEX_TYPE)
    {
      tree subtype1 = code1 == COMPLEX_TYPE ? TREE_TYPE (t1) : t1;
      tree subtype2 = code2 == COMPLEX_TYPE ? TREE_TYPE (t2) : t2;
      tree subtype = c_common_type (subtype1, subtype2);

      if (code1 == COMPLEX_TYPE && TREE_TYPE (t1) == subtype)
	return t1;
      else if (code2 == COMPLEX_TYPE && TREE_TYPE (t2) == subtype)
	return t2;
      else
	return build_complex_type (subtype);
    }

  /* If only one is real, use it as the result.  */

  if (code1 == REAL_TYPE && code2 != REAL_TYPE)
    return t1;

  if (code2 == REAL_TYPE && code1 != REAL_TYPE)
    return t2;

  /* If both are real and either are decimal floating point types, use
     the decimal floating point type with the greater precision. */

  if (code1 == REAL_TYPE && code2 == REAL_TYPE)
    {
      if (TYPE_MAIN_VARIANT (t1) == dfloat128_type_node
	  || TYPE_MAIN_VARIANT (t2) == dfloat128_type_node)
	return dfloat128_type_node;
      else if (TYPE_MAIN_VARIANT (t1) == dfloat64_type_node
	       || TYPE_MAIN_VARIANT (t2) == dfloat64_type_node)
	return dfloat64_type_node;
      else if (TYPE_MAIN_VARIANT (t1) == dfloat32_type_node
	       || TYPE_MAIN_VARIANT (t2) == dfloat32_type_node)
	return dfloat32_type_node;
    }

  /* Deal with fixed-point types.  */
  if (code1 == FIXED_POINT_TYPE || code2 == FIXED_POINT_TYPE)
    {
      unsigned int unsignedp = 0, satp = 0;
      scalar_mode m1, m2;
      unsigned int fbit1, ibit1, fbit2, ibit2, max_fbit, max_ibit;

      m1 = SCALAR_TYPE_MODE (t1);
      m2 = SCALAR_TYPE_MODE (t2);

      /* If one input type is saturating, the result type is saturating.  */
      if (TYPE_SATURATING (t1) || TYPE_SATURATING (t2))
	satp = 1;

      /* If both fixed-point types are unsigned, the result type is unsigned.
	 When mixing fixed-point and integer types, follow the sign of the
	 fixed-point type.
	 Otherwise, the result type is signed.  */
      if ((TYPE_UNSIGNED (t1) && TYPE_UNSIGNED (t2)
	   && code1 == FIXED_POINT_TYPE && code2 == FIXED_POINT_TYPE)
	  || (code1 == FIXED_POINT_TYPE && code2 != FIXED_POINT_TYPE
	      && TYPE_UNSIGNED (t1))
	  || (code1 != FIXED_POINT_TYPE && code2 == FIXED_POINT_TYPE
	      && TYPE_UNSIGNED (t2)))
	unsignedp = 1;

      /* The result type is signed.  */
      if (unsignedp == 0)
	{
	  /* If the input type is unsigned, we need to convert to the
	     signed type.  */
	  if (code1 == FIXED_POINT_TYPE && TYPE_UNSIGNED (t1))
	    {
	      enum mode_class mclass = (enum mode_class) 0;
	      if (GET_MODE_CLASS (m1) == MODE_UFRACT)
		mclass = MODE_FRACT;
	      else if (GET_MODE_CLASS (m1) == MODE_UACCUM)
		mclass = MODE_ACCUM;
	      else
		gcc_unreachable ();
	      m1 = as_a <scalar_mode>
		(mode_for_size (GET_MODE_PRECISION (m1), mclass, 0));
	    }
	  if (code2 == FIXED_POINT_TYPE && TYPE_UNSIGNED (t2))
	    {
	      enum mode_class mclass = (enum mode_class) 0;
	      if (GET_MODE_CLASS (m2) == MODE_UFRACT)
		mclass = MODE_FRACT;
	      else if (GET_MODE_CLASS (m2) == MODE_UACCUM)
		mclass = MODE_ACCUM;
	      else
		gcc_unreachable ();
	      m2 = as_a <scalar_mode>
		(mode_for_size (GET_MODE_PRECISION (m2), mclass, 0));
	    }
	}

      if (code1 == FIXED_POINT_TYPE)
	{
	  fbit1 = GET_MODE_FBIT (m1);
	  ibit1 = GET_MODE_IBIT (m1);
	}
      else
	{
	  fbit1 = 0;
	  /* Signed integers need to subtract one sign bit.  */
	  ibit1 = TYPE_PRECISION (t1) - (!TYPE_UNSIGNED (t1));
	}

      if (code2 == FIXED_POINT_TYPE)
	{
	  fbit2 = GET_MODE_FBIT (m2);
	  ibit2 = GET_MODE_IBIT (m2);
	}
      else
	{
	  fbit2 = 0;
	  /* Signed integers need to subtract one sign bit.  */
	  ibit2 = TYPE_PRECISION (t2) - (!TYPE_UNSIGNED (t2));
	}

      max_ibit = ibit1 >= ibit2 ?  ibit1 : ibit2;
      max_fbit = fbit1 >= fbit2 ?  fbit1 : fbit2;
      return c_common_fixed_point_type_for_size (max_ibit, max_fbit, unsignedp,
						 satp);
    }

  /* Both real or both integers; use the one with greater precision.  */

  if (TYPE_PRECISION (t1) > TYPE_PRECISION (t2))
    return t1;
  else if (TYPE_PRECISION (t2) > TYPE_PRECISION (t1))
    return t2;

  /* Same precision.  Prefer long longs to longs to ints when the
     same precision, following the C99 rules on integer type rank
     (which are equivalent to the C90 rules for C90 types).  */

  if (TYPE_MAIN_VARIANT (t1) == long_long_unsigned_type_node
      || TYPE_MAIN_VARIANT (t2) == long_long_unsigned_type_node)
    return long_long_unsigned_type_node;

  if (TYPE_MAIN_VARIANT (t1) == long_long_integer_type_node
      || TYPE_MAIN_VARIANT (t2) == long_long_integer_type_node)
    {
      if (TYPE_UNSIGNED (t1) || TYPE_UNSIGNED (t2))
	return long_long_unsigned_type_node;
      else
	return long_long_integer_type_node;
    }

  if (TYPE_MAIN_VARIANT (t1) == long_unsigned_type_node
      || TYPE_MAIN_VARIANT (t2) == long_unsigned_type_node)
    return long_unsigned_type_node;

  if (TYPE_MAIN_VARIANT (t1) == long_integer_type_node
      || TYPE_MAIN_VARIANT (t2) == long_integer_type_node)
    {
      /* But preserve unsignedness from the other type,
	 since long cannot hold all the values of an unsigned int.  */
      if (TYPE_UNSIGNED (t1) || TYPE_UNSIGNED (t2))
	return long_unsigned_type_node;
      else
	return long_integer_type_node;
    }

  /* For floating types of the same TYPE_PRECISION (which we here
     assume means either the same set of values, or sets of values
     neither a subset of the other, with behavior being undefined in
     the latter case), follow the rules from TS 18661-3: prefer
     interchange types _FloatN, then standard types long double,
     double, float, then extended types _FloatNx.  For extended types,
     check them starting with _Float128x as that seems most consistent
     in spirit with preferring long double to double; for interchange
     types, also check in that order for consistency although it's not
     possible for more than one of them to have the same
     precision.  */
  tree mv1 = TYPE_MAIN_VARIANT (t1);
  tree mv2 = TYPE_MAIN_VARIANT (t2);

  for (int i = NUM_FLOATN_TYPES - 1; i >= 0; i--)
    if (mv1 == FLOATN_TYPE_NODE (i) || mv2 == FLOATN_TYPE_NODE (i))
      return FLOATN_TYPE_NODE (i);

  /* Likewise, prefer long double to double even if same size.  */
  if (mv1 == long_double_type_node || mv2 == long_double_type_node)
    return long_double_type_node;

  /* Likewise, prefer double to float even if same size.
     We got a couple of embedded targets with 32 bit doubles, and the
     pdp11 might have 64 bit floats.  */
  if (mv1 == double_type_node || mv2 == double_type_node)
    return double_type_node;

  if (mv1 == float_type_node || mv2 == float_type_node)
    return float_type_node;

  for (int i = NUM_FLOATNX_TYPES - 1; i >= 0; i--)
    if (mv1 == FLOATNX_TYPE_NODE (i) || mv2 == FLOATNX_TYPE_NODE (i))
      return FLOATNX_TYPE_NODE (i);

  /* Otherwise prefer the unsigned one.  */

  if (TYPE_UNSIGNED (t1))
    return t1;
  else
    return t2;
}


/* Return 1 if TYPE1 and TYPE2 are compatible types for assignment
   or various other operations.  Return 2 if they are compatible
   but a warning may be needed if you use them together.  */

int aet_comptypes (tree type1, tree type2)
{
  const struct tagged_tu_seen_cache * tagged_tu_seen_base1 = tagged_tu_seen_base;
  int val;

  val = comptypes_internal (type1, type2, NULL, NULL);
  free_all_tagged_tu_seen_up_to (tagged_tu_seen_base1);

  return val;
}

/* Like comptypes, but if it returns non-zero because enum and int are
   compatible, it sets *ENUM_AND_INT_P to true.  */

static int comptypes_check_enum_int (tree type1, tree type2, bool *enum_and_int_p)
{
  const struct tagged_tu_seen_cache * tagged_tu_seen_base1 = tagged_tu_seen_base;
  int val;

  val = comptypes_internal (type1, type2, enum_and_int_p, NULL);
  free_all_tagged_tu_seen_up_to (tagged_tu_seen_base1);

  return val;
}


/* Return 1 if TYPE1 and TYPE2 are compatible types for assignment
   or various other operations.  Return 2 if they are compatible
   but a warning may be needed if you use them together.  If
   ENUM_AND_INT_P is not NULL, and one type is an enum and the other a
   compatible integer type, then this sets *ENUM_AND_INT_P to true;
   *ENUM_AND_INT_P is never set to false.  If DIFFERENT_TYPES_P is not
   NULL, and the types are compatible but different enough not to be
   permitted in C11 typedef redeclarations, then this sets
   *DIFFERENT_TYPES_P to true; *DIFFERENT_TYPES_P is never set to
   false, but may or may not be set if the types are incompatible.
   This differs from comptypes, in that we don't free the seen
   types.  */
static int comptypes_internal (const_tree type1, const_tree type2, bool *enum_and_int_p,bool *different_types_p)
{
  const_tree t1 = type1;
  const_tree t2 = type2;
  int attrval, val;

  /* Suppress errors caused by previously reported errors.  */

  if (t1 == t2 || !t1 || !t2 || TREE_CODE (t1) == ERROR_MARK || TREE_CODE (t2) == ERROR_MARK)
    return 1;

  /* Enumerated types are compatible with integer types, but this is
     not transitive: two enumerated types in the same translation unit
     are compatible with each other only if they are the same type.  */

  if (TREE_CODE (t1) == ENUMERAL_TYPE && TREE_CODE (t2) != ENUMERAL_TYPE)
    {
      t1 = c_common_type_for_size (TYPE_PRECISION (t1), TYPE_UNSIGNED (t1));
      if (TREE_CODE (t2) != VOID_TYPE)
	{
	  if (enum_and_int_p != NULL)
	    *enum_and_int_p = true;
	  if (different_types_p != NULL)
	    *different_types_p = true;
	}
    }
  else if (TREE_CODE (t2) == ENUMERAL_TYPE && TREE_CODE (t1) != ENUMERAL_TYPE)
    {
      t2 = c_common_type_for_size (TYPE_PRECISION (t2), TYPE_UNSIGNED (t2));
      if (TREE_CODE (t1) != VOID_TYPE)
	{
	  if (enum_and_int_p != NULL)
	    *enum_and_int_p = true;
	  if (different_types_p != NULL)
	    *different_types_p = true;
	}
    }

  if (t1 == t2)
    return 1;

  /* Different classes of types can't be compatible.  */

  if (TREE_CODE (t1) != TREE_CODE (t2))
    return 0;

  /* Qualifiers must match. C99 6.7.3p9 */

  if (TYPE_QUALS (t1) != TYPE_QUALS (t2))
    return 0;

  /* Allow for two different type nodes which have essentially the same
     definition.  Note that we already checked for equality of the type
     qualifiers (just above).  */

  if (TREE_CODE (t1) != ARRAY_TYPE
      && TYPE_MAIN_VARIANT (t1) == TYPE_MAIN_VARIANT (t2))
    return 1;

  /* 1 if no need for warning yet, 2 if warning cause has been seen.  */
  if (!(attrval = comp_type_attributes (t1, t2)))
     return 0;

  /* 1 if no need for warning yet, 2 if warning cause has been seen.  */
  val = 0;

  switch (TREE_CODE (t1))
    {
    case INTEGER_TYPE:
    case FIXED_POINT_TYPE:
    case REAL_TYPE:
      /* With these nodes, we can't determine type equivalence by
	 looking at what is stored in the nodes themselves, because
	 two nodes might have different TYPE_MAIN_VARIANTs but still
	 represent the same type.  For example, wchar_t and int could
	 have the same properties (TYPE_PRECISION, TYPE_MIN_VALUE,
	 TYPE_MAX_VALUE, etc.), but have different TYPE_MAIN_VARIANTs
	 and are distinct types.  On the other hand, int and the
	 following typedef

	   typedef int INT __attribute((may_alias));

	 have identical properties, different TYPE_MAIN_VARIANTs, but
	 represent the same type.  The canonical type system keeps
	 track of equivalence in this case, so we fall back on it.  */
      return TYPE_CANONICAL (t1) == TYPE_CANONICAL (t2);

    case POINTER_TYPE:
      /* Do not remove mode information.  */
      if (TYPE_MODE (t1) != TYPE_MODE (t2))
	break;
      val = (TREE_TYPE (t1) == TREE_TYPE (t2)
	     ? 1 : comptypes_internal (TREE_TYPE (t1), TREE_TYPE (t2),
				       enum_and_int_p, different_types_p));
      break;

    case FUNCTION_TYPE:
      val = function_types_compatible_p (t1, t2, enum_and_int_p,
					 different_types_p);
      break;

    case ARRAY_TYPE:
      {
	tree d1 = TYPE_DOMAIN (t1);
	tree d2 = TYPE_DOMAIN (t2);
	bool d1_variable, d2_variable;
	bool d1_zero, d2_zero;
	val = 1;

	/* Target types must match incl. qualifiers.  */
	if (TREE_TYPE (t1) != TREE_TYPE (t2)
	    && (val = comptypes_internal (TREE_TYPE (t1), TREE_TYPE (t2),
					  enum_and_int_p,
					  different_types_p)) == 0)
	  return 0;

	if (different_types_p != NULL
	    && (d1 == NULL_TREE) != (d2 == NULL_TREE))
	  *different_types_p = true;
	/* Sizes must match unless one is missing or variable.  */
	if (d1 == NULL_TREE || d2 == NULL_TREE || d1 == d2)
	  break;

	d1_zero = !TYPE_MAX_VALUE (d1);
	d2_zero = !TYPE_MAX_VALUE (d2);

	d1_variable = (!d1_zero
		       && (TREE_CODE (TYPE_MIN_VALUE (d1)) != INTEGER_CST
			   || TREE_CODE (TYPE_MAX_VALUE (d1)) != INTEGER_CST));
	d2_variable = (!d2_zero
		       && (TREE_CODE (TYPE_MIN_VALUE (d2)) != INTEGER_CST
			   || TREE_CODE (TYPE_MAX_VALUE (d2)) != INTEGER_CST));
	d1_variable = d1_variable || (d1_zero && aet_c_vla_type_p (t1));
	d2_variable = d2_variable || (d2_zero && aet_c_vla_type_p (t2));

	if (different_types_p != NULL
	    && d1_variable != d2_variable)
	  *different_types_p = true;
	if (d1_variable || d2_variable)
	  break;
	if (d1_zero && d2_zero)
	  break;
	if (d1_zero || d2_zero
	    || !tree_int_cst_equal (TYPE_MIN_VALUE (d1), TYPE_MIN_VALUE (d2))
	    || !tree_int_cst_equal (TYPE_MAX_VALUE (d1), TYPE_MAX_VALUE (d2)))
	  val = 0;

	break;
      }

    case ENUMERAL_TYPE:
    case RECORD_TYPE:
    case UNION_TYPE:
      if (val != 1 && !aet_same_translation_unit_p (t1, t2))
	{
	  tree a1 = TYPE_ATTRIBUTES (t1);
	  tree a2 = TYPE_ATTRIBUTES (t2);

	  if (! attribute_list_contained (a1, a2)
	      && ! attribute_list_contained (a2, a1))
	    break;

	  if (attrval != 2)
	    return tagged_types_tu_compatible_p (t1, t2, enum_and_int_p,
						 different_types_p);
	  val = tagged_types_tu_compatible_p (t1, t2, enum_and_int_p,
					      different_types_p);
	}
      break;

    case VECTOR_TYPE:
      val = (known_eq (TYPE_VECTOR_SUBPARTS (t1), TYPE_VECTOR_SUBPARTS (t2))
	     && comptypes_internal (TREE_TYPE (t1), TREE_TYPE (t2),
				    enum_and_int_p, different_types_p));
      break;

    default:
      break;
    }
  return attrval == 2 && val == 1 ? 2 : val;
}

/* Return 1 if TTL and TTR are pointers to types that are equivalent, ignoring
   their qualifiers, except for named address spaces.  If the pointers point to
   different named addresses, then we must determine if one address space is a
   subset of the other.  */

static int comp_target_types (location_t location, tree ttl, tree ttr)
{
  int val;
  int val_ped;
  tree mvl = TREE_TYPE (ttl);
  tree mvr = TREE_TYPE (ttr);
  addr_space_t asl = TYPE_ADDR_SPACE (mvl);
  addr_space_t asr = TYPE_ADDR_SPACE (mvr);
  addr_space_t as_common;
  bool enum_and_int_p;

  /* Fail if pointers point to incompatible address spaces.  */
  if (!addr_space_superset (asl, asr, &as_common))
    return 0;

  /* For pedantic record result of comptypes on arrays before losing
     qualifiers on the element type below. */
  val_ped = 1;

  if (TREE_CODE (mvl) == ARRAY_TYPE
      && TREE_CODE (mvr) == ARRAY_TYPE)
    val_ped = aet_comptypes (mvl, mvr);

  /* Qualifiers on element types of array types that are
     pointer targets are lost by taking their TYPE_MAIN_VARIANT.  */

  mvl = (TYPE_ATOMIC (strip_array_types (mvl))
	 ? aet_c_build_qualified_type (TYPE_MAIN_VARIANT (mvl), TYPE_QUAL_ATOMIC)
	 : TYPE_MAIN_VARIANT (mvl));

  mvr = (TYPE_ATOMIC (strip_array_types (mvr))
	 ? aet_c_build_qualified_type (TYPE_MAIN_VARIANT (mvr), TYPE_QUAL_ATOMIC)
	 : TYPE_MAIN_VARIANT (mvr));

  enum_and_int_p = false;
  val = comptypes_check_enum_int (mvl, mvr, &enum_and_int_p);

  if (val == 1 && val_ped != 1)
    pedwarn (location, OPT_Wpedantic, "pointers to arrays with different qualifiers "
                                      "are incompatible in ISO C");

  if (val == 2)
    pedwarn (location, OPT_Wpedantic, "types are not quite compatible");

  if (val == 1 && enum_and_int_p && warn_cxx_compat)
    warning_at (location, OPT_Wc___compat,
		"pointer target types incompatible in C++");

  return val;
}

/* Subroutines of `comptypes'.  */

/* Determine whether two trees derive from the same translation unit.
   If the CONTEXT chain ends in a null, that tree's context is still
   being parsed, so if two trees have context chains ending in null,
   they're in the same translation unit.  */

bool aet_same_translation_unit_p (const_tree t1, const_tree t2)
{
  while (t1 && TREE_CODE (t1) != TRANSLATION_UNIT_DECL)
    switch (TREE_CODE_CLASS (TREE_CODE (t1)))
      {
      case tcc_declaration:
	t1 = DECL_CONTEXT (t1); break;
      case tcc_type:
	t1 = TYPE_CONTEXT (t1); break;
      case tcc_exceptional:
	t1 = BLOCK_SUPERCONTEXT (t1); break;  /* assume block */
      default: gcc_unreachable ();
      }
  //printf("t2 is null? 00 %p\n",t2);
 // printf("t2 is null? 11 %s %d %d\n",get_tree_code_name(TREE_CODE(t2)),TREE_CODE(t2),TRANSLATION_UNIT_DECL);
  //printf("t2 is null? 22 %p %d\n",t2,TREE_CODE_CLASS (TREE_CODE (t2)));

  while (t2 && TREE_CODE (t2) != TRANSLATION_UNIT_DECL){
    switch (TREE_CODE_CLASS (TREE_CODE (t2))){
      case tcc_declaration:
	    t2 = DECL_CONTEXT (t2);
	    //printf("tcc_declaration %p\n",t2);
	    break;
      case tcc_type:
	    t2 = TYPE_CONTEXT (t2);
	   // printf("tcc_type %p %s\n",t2,t2?get_tree_code_name(TREE_CODE(t2)):"NULL");

	    break;
      case tcc_exceptional:
	    t2 = BLOCK_SUPERCONTEXT (t2);
	    printf("tcc_exceptional %p\n",t2);

	    break;  /* assume block */
      default:
  	    printf("gcc_unreachable %p\n",t2);

    	gcc_unreachable ();
    }
  }
  return t1 == t2;
}

/* Allocate the seen two types, assuming that they are compatible. */

static struct tagged_tu_seen_cache *alloc_tagged_tu_seen_cache (const_tree t1, const_tree t2)
{
  struct tagged_tu_seen_cache *tu = XNEW (struct tagged_tu_seen_cache);
  tu->next = tagged_tu_seen_base;
  tu->t1 = t1;
  tu->t2 = t2;

  tagged_tu_seen_base = tu;

  /* The C standard says that two structures in different translation
     units are compatible with each other only if the types of their
     fields are compatible (among other things).  We assume that they
     are compatible until proven otherwise when building the cache.
     An example where this can occur is:
     struct a
     {
       struct a *next;
     };
     If we are comparing this against a similar struct in another TU,
     and did not assume they were compatible, we end up with an infinite
     loop.  */
  tu->val = 1;
  return tu;
}

/* Free the seen types until we get to TU_TIL. */

static void free_all_tagged_tu_seen_up_to (const struct tagged_tu_seen_cache *tu_til)
{
  const struct tagged_tu_seen_cache *tu = tagged_tu_seen_base;
  while (tu != tu_til)
    {
      const struct tagged_tu_seen_cache *const tu1
	= (const struct tagged_tu_seen_cache *) tu;
      tu = tu1->next;
      free (CONST_CAST (struct tagged_tu_seen_cache *, tu1));
    }
  tagged_tu_seen_base = tu_til;
}

/* Return 1 if two 'struct', 'union', or 'enum' types T1 and T2 are
   compatible.  If the two types are not the same (which has been
   checked earlier), this can only happen when multiple translation
   units are being compiled.  See C99 6.2.7 paragraph 1 for the exact
   rules.  ENUM_AND_INT_P and DIFFERENT_TYPES_P are as in
   comptypes_internal.  */

static int tagged_types_tu_compatible_p (const_tree t1, const_tree t2,
			      bool *enum_and_int_p, bool *different_types_p)
{
  tree s1, s2;
  bool needs_warning = false;

  /* We have to verify that the tags of the types are the same.  This
     is harder than it looks because this may be a typedef, so we have
     to go look at the original type.  It may even be a typedef of a
     typedef...
     In the case of compiler-created builtin structs the TYPE_DECL
     may be a dummy, with no DECL_ORIGINAL_TYPE.  Don't fault.  */
  while (TYPE_NAME (t1)
	 && TREE_CODE (TYPE_NAME (t1)) == TYPE_DECL
	 && DECL_ORIGINAL_TYPE (TYPE_NAME (t1)))
    t1 = DECL_ORIGINAL_TYPE (TYPE_NAME (t1));

  while (TYPE_NAME (t2)
	 && TREE_CODE (TYPE_NAME (t2)) == TYPE_DECL
	 && DECL_ORIGINAL_TYPE (TYPE_NAME (t2)))
    t2 = DECL_ORIGINAL_TYPE (TYPE_NAME (t2));

  /* C90 didn't have the requirement that the two tags be the same.  */
  if (flag_isoc99 && TYPE_NAME (t1) != TYPE_NAME (t2))
    return 0;

  /* C90 didn't say what happened if one or both of the types were
     incomplete; we choose to follow C99 rules here, which is that they
     are compatible.  */
  if (TYPE_SIZE (t1) == NULL
      || TYPE_SIZE (t2) == NULL)
    return 1;

  {
    const struct tagged_tu_seen_cache * tts_i;
    for (tts_i = tagged_tu_seen_base; tts_i != NULL; tts_i = tts_i->next)
      if (tts_i->t1 == t1 && tts_i->t2 == t2)
	return tts_i->val;
  }

  switch (TREE_CODE (t1))
    {
    case ENUMERAL_TYPE:
      {
	struct tagged_tu_seen_cache *tu = alloc_tagged_tu_seen_cache (t1, t2);
	/* Speed up the case where the type values are in the same order.  */
	tree tv1 = TYPE_VALUES (t1);
	tree tv2 = TYPE_VALUES (t2);

	if (tv1 == tv2)
	  {
	    return 1;
	  }

	for (;tv1 && tv2; tv1 = TREE_CHAIN (tv1), tv2 = TREE_CHAIN (tv2))
	  {
	    if (TREE_PURPOSE (tv1) != TREE_PURPOSE (tv2))
	      break;
	    if (simple_cst_equal (TREE_VALUE (tv1), TREE_VALUE (tv2)) != 1)
	      {
		tu->val = 0;
		return 0;
	      }
	  }

	if (tv1 == NULL_TREE && tv2 == NULL_TREE)
	  {
	    return 1;
	  }
	if (tv1 == NULL_TREE || tv2 == NULL_TREE)
	  {
	    tu->val = 0;
	    return 0;
	  }

	if (list_length (TYPE_VALUES (t1)) != list_length (TYPE_VALUES (t2)))
	  {
	    tu->val = 0;
	    return 0;
	  }

	for (s1 = TYPE_VALUES (t1); s1; s1 = TREE_CHAIN (s1))
	  {
	    s2 = purpose_member (TREE_PURPOSE (s1), TYPE_VALUES (t2));
	    if (s2 == NULL
		|| simple_cst_equal (TREE_VALUE (s1), TREE_VALUE (s2)) != 1)
	      {
		tu->val = 0;
		return 0;
	      }
	  }
	return 1;
      }

    case UNION_TYPE:
      {
	struct tagged_tu_seen_cache *tu = alloc_tagged_tu_seen_cache (t1, t2);
	if (list_length (TYPE_FIELDS (t1)) != list_length (TYPE_FIELDS (t2)))
	  {
	    tu->val = 0;
	    return 0;
	  }

	/*  Speed up the common case where the fields are in the same order. */
	for (s1 = TYPE_FIELDS (t1), s2 = TYPE_FIELDS (t2); s1 && s2;
	     s1 = DECL_CHAIN (s1), s2 = DECL_CHAIN (s2))
	  {
	    int result;

	    if (DECL_NAME (s1) != DECL_NAME (s2))
	      break;
	    result = comptypes_internal (TREE_TYPE (s1), TREE_TYPE (s2),
					 enum_and_int_p, different_types_p);

	    if (result != 1 && !DECL_NAME (s1))
	      break;
	    if (result == 0)
	      {
		tu->val = 0;
		return 0;
	      }
	    if (result == 2)
	      needs_warning = true;

	    if (TREE_CODE (s1) == FIELD_DECL
		&& simple_cst_equal (DECL_FIELD_BIT_OFFSET (s1),
				     DECL_FIELD_BIT_OFFSET (s2)) != 1)
	      {
		tu->val = 0;
		return 0;
	      }
	  }
	if (!s1 && !s2)
	  {
	    tu->val = needs_warning ? 2 : 1;
	    return tu->val;
	  }

	for (s1 = TYPE_FIELDS (t1); s1; s1 = DECL_CHAIN (s1))
	  {
	    bool ok = false;

	    for (s2 = TYPE_FIELDS (t2); s2; s2 = DECL_CHAIN (s2))
	      if (DECL_NAME (s1) == DECL_NAME (s2))
		{
		  int result;

		  result = comptypes_internal (TREE_TYPE (s1), TREE_TYPE (s2),
					       enum_and_int_p,
					       different_types_p);

		  if (result != 1 && !DECL_NAME (s1))
		    continue;
		  if (result == 0)
		    {
		      tu->val = 0;
		      return 0;
		    }
		  if (result == 2)
		    needs_warning = true;

		  if (TREE_CODE (s1) == FIELD_DECL
		      && simple_cst_equal (DECL_FIELD_BIT_OFFSET (s1),
					   DECL_FIELD_BIT_OFFSET (s2)) != 1)
		    break;

		  ok = true;
		  break;
		}
	    if (!ok)
	      {
		tu->val = 0;
		return 0;
	      }
	  }
	tu->val = needs_warning ? 2 : 10;
	return tu->val;
      }

    case RECORD_TYPE:
      {
	struct tagged_tu_seen_cache *tu = alloc_tagged_tu_seen_cache (t1, t2);

	for (s1 = TYPE_FIELDS (t1), s2 = TYPE_FIELDS (t2);
	     s1 && s2;
	     s1 = DECL_CHAIN (s1), s2 = DECL_CHAIN (s2))
	  {
	    int result;
	    if (TREE_CODE (s1) != TREE_CODE (s2)
		|| DECL_NAME (s1) != DECL_NAME (s2))
	      break;
	    result = comptypes_internal (TREE_TYPE (s1), TREE_TYPE (s2),
					 enum_and_int_p, different_types_p);
	    if (result == 0)
	      break;
	    if (result == 2)
	      needs_warning = true;

	    if (TREE_CODE (s1) == FIELD_DECL
		&& simple_cst_equal (DECL_FIELD_BIT_OFFSET (s1),
				     DECL_FIELD_BIT_OFFSET (s2)) != 1)
	      break;
	  }
	if (s1 && s2)
	  tu->val = 0;
	else
	  tu->val = needs_warning ? 2 : 1;
	return tu->val;
      }

    default:
      gcc_unreachable ();
    }
}

/* Return 1 if two function types F1 and F2 are compatible.
   If either type specifies no argument types,
   the other must specify a fixed number of self-promoting arg types.
   Otherwise, if one type specifies only the number of arguments,
   the other must specify that number of self-promoting arg types.
   Otherwise, the argument types must match.
   ENUM_AND_INT_P and DIFFERENT_TYPES_P are as in comptypes_internal.  */

static int function_types_compatible_p (const_tree f1, const_tree f2, bool *enum_and_int_p, bool *different_types_p)
{
  tree args1, args2;
  /* 1 if no need for warning yet, 2 if warning cause has been seen.  */
  int val = 1;
  int val1;
  tree ret1, ret2;

  ret1 = TREE_TYPE (f1);
  ret2 = TREE_TYPE (f2);

  /* 'volatile' qualifiers on a function's return type used to mean
     the function is noreturn.  */
  if (TYPE_VOLATILE (ret1) != TYPE_VOLATILE (ret2))
    pedwarn (input_location, 0, "function return types not compatible due to %<volatile%>");
  if (TYPE_VOLATILE (ret1))
    ret1 = build_qualified_type (TYPE_MAIN_VARIANT (ret1),
				 TYPE_QUALS (ret1) & ~TYPE_QUAL_VOLATILE);
  if (TYPE_VOLATILE (ret2))
    ret2 = build_qualified_type (TYPE_MAIN_VARIANT (ret2),
				 TYPE_QUALS (ret2) & ~TYPE_QUAL_VOLATILE);
  val = comptypes_internal (ret1, ret2, enum_and_int_p, different_types_p);
  if (val == 0)
    return 0;

  args1 = TYPE_ARG_TYPES (f1);
  args2 = TYPE_ARG_TYPES (f2);

  if (different_types_p != NULL
      && (args1 == NULL_TREE) != (args2 == NULL_TREE))
    *different_types_p = true;

  /* An unspecified parmlist matches any specified parmlist
     whose argument types don't need default promotions.  */

  if (args1 == NULL_TREE)
    {
      if (!self_promoting_args_p (args2))
	return 0;
      /* If one of these types comes from a non-prototype fn definition,
	 compare that with the other type's arglist.
	 If they don't match, ask for a warning (but no error).  */
      if (TYPE_ACTUAL_ARG_TYPES (f1)
	  && type_lists_compatible_p (args2, TYPE_ACTUAL_ARG_TYPES (f1),
				      enum_and_int_p, different_types_p) != 1)
	val = 2;
      return val;
    }
  if (args2 == NULL_TREE)
    {
      if (!self_promoting_args_p (args1))
	return 0;
      if (TYPE_ACTUAL_ARG_TYPES (f2)
	  && type_lists_compatible_p (args1, TYPE_ACTUAL_ARG_TYPES (f2),
				      enum_and_int_p, different_types_p) != 1)
	val = 2;
      return val;
    }

  /* Both types have argument lists: compare them and propagate results.  */
  val1 = type_lists_compatible_p (args1, args2, enum_and_int_p,
				  different_types_p);
  return val1 != 1 ? val1 : val;
}

/* Check two lists of types for compatibility, returning 0 for
   incompatible, 1 for compatible, or 2 for compatible with
   warning.  ENUM_AND_INT_P and DIFFERENT_TYPES_P are as in
   comptypes_internal.  */

static int type_lists_compatible_p (const_tree args1, const_tree args2, bool *enum_and_int_p, bool *different_types_p)
{
  /* 1 if no need for warning yet, 2 if warning cause has been seen.  */
  int val = 1;
  int newval = 0;

  while (1)
    {
      tree a1, mv1, a2, mv2;
      if (args1 == NULL_TREE && args2 == NULL_TREE)
	return val;
      /* If one list is shorter than the other,
	 they fail to match.  */
      if (args1 == NULL_TREE || args2 == NULL_TREE)
	return 0;
      mv1 = a1 = TREE_VALUE (args1);
      mv2 = a2 = TREE_VALUE (args2);
      if (mv1 && mv1 != error_mark_node && TREE_CODE (mv1) != ARRAY_TYPE)
	mv1 = (TYPE_ATOMIC (mv1)
	       ? aet_c_build_qualified_type (TYPE_MAIN_VARIANT (mv1),
					 TYPE_QUAL_ATOMIC)
	       : TYPE_MAIN_VARIANT (mv1));
      if (mv2 && mv2 != error_mark_node && TREE_CODE (mv2) != ARRAY_TYPE)
	mv2 = (TYPE_ATOMIC (mv2)
	       ? aet_c_build_qualified_type (TYPE_MAIN_VARIANT (mv2),
					 TYPE_QUAL_ATOMIC)
	       : TYPE_MAIN_VARIANT (mv2));
      /* A null pointer instead of a type
	 means there is supposed to be an argument
	 but nothing is specified about what type it has.
	 So match anything that self-promotes.  */
      if (different_types_p != NULL
	  && (a1 == NULL_TREE) != (a2 == NULL_TREE))
	*different_types_p = true;
      if (a1 == NULL_TREE)
	{
	  if (aet_c_type_promotes_to (a2) != a2)
	    return 0;
	}
      else if (a2 == NULL_TREE)
	{
	  if (aet_c_type_promotes_to (a1) != a1)
	    return 0;
	}
      /* If one of the lists has an error marker, ignore this arg.  */
      else if (TREE_CODE (a1) == ERROR_MARK
	       || TREE_CODE (a2) == ERROR_MARK)
	;
      else if (!(newval = comptypes_internal (mv1, mv2, enum_and_int_p,
					      different_types_p)))
	{
	  if (different_types_p != NULL)
	    *different_types_p = true;
	  /* Allow  wait (union {union wait *u; int *i} *)
	     and  wait (union wait *)  to be compatible.  */
	  if (TREE_CODE (a1) == UNION_TYPE
	      && (TYPE_NAME (a1) == NULL_TREE
		  || TYPE_TRANSPARENT_AGGR (a1))
	      && TREE_CODE (TYPE_SIZE (a1)) == INTEGER_CST
	      && tree_int_cst_equal (TYPE_SIZE (a1),
				     TYPE_SIZE (a2)))
	    {
	      tree memb;
	      for (memb = TYPE_FIELDS (a1);
		   memb; memb = DECL_CHAIN (memb))
		{
		  tree mv3 = TREE_TYPE (memb);
		  if (mv3 && mv3 != error_mark_node
		      && TREE_CODE (mv3) != ARRAY_TYPE)
		    mv3 = (TYPE_ATOMIC (mv3)
			   ? aet_c_build_qualified_type (TYPE_MAIN_VARIANT (mv3),
						     TYPE_QUAL_ATOMIC)
			   : TYPE_MAIN_VARIANT (mv3));
		  if (comptypes_internal (mv3, mv2, enum_and_int_p,
					  different_types_p))
		    break;
		}
	      if (memb == NULL_TREE)
		return 0;
	    }
	  else if (TREE_CODE (a2) == UNION_TYPE
		   && (TYPE_NAME (a2) == NULL_TREE
		       || TYPE_TRANSPARENT_AGGR (a2))
		   && TREE_CODE (TYPE_SIZE (a2)) == INTEGER_CST
		   && tree_int_cst_equal (TYPE_SIZE (a2),
					  TYPE_SIZE (a1)))
	    {
	      tree memb;
	      for (memb = TYPE_FIELDS (a2);
		   memb; memb = DECL_CHAIN (memb))
		{
		  tree mv3 = TREE_TYPE (memb);
		  if (mv3 && mv3 != error_mark_node
		      && TREE_CODE (mv3) != ARRAY_TYPE)
		    mv3 = (TYPE_ATOMIC (mv3)
			   ? aet_c_build_qualified_type (TYPE_MAIN_VARIANT (mv3),
						     TYPE_QUAL_ATOMIC)
			   : TYPE_MAIN_VARIANT (mv3));
		  if (comptypes_internal (mv3, mv1, enum_and_int_p,
					  different_types_p))
		    break;
		}
	      if (memb == NULL_TREE)
		return 0;
	    }
	  else
	    return 0;
	}

      /* comptypes said ok, but record if it said to warn.  */
      if (newval > val)
	val = newval;

      args1 = TREE_CHAIN (args1);
      args2 = TREE_CHAIN (args2);
    }
}

/* Compute the size to increment a pointer by.  When a function type or void
   type or incomplete type is passed, size_one_node is returned.
   This function does not emit any diagnostics; the caller is responsible
   for that.  */

static tree c_size_in_bytes (const_tree type)
{
  enum tree_code code = TREE_CODE (type);

  if (code == FUNCTION_TYPE || code == VOID_TYPE || code == ERROR_MARK
      || !COMPLETE_TYPE_P (type))
    return size_one_node;

  /* Convert in case a char is more than one unit.  */
  return size_binop_loc (input_location, CEIL_DIV_EXPR, TYPE_SIZE_UNIT (type),
			 size_int (TYPE_PRECISION (char_type_node)
				   / BITS_PER_UNIT));
}

/* Return either DECL or its known constant value (if it has one).  */

tree aet_decl_constant_value_1 (tree decl, bool in_init)
{
  if (/* Note that DECL_INITIAL isn't valid for a PARM_DECL.  */
      TREE_CODE (decl) != PARM_DECL
      && !TREE_THIS_VOLATILE (decl)
      && TREE_READONLY (decl)
      && DECL_INITIAL (decl) != NULL_TREE
      && !error_operand_p (DECL_INITIAL (decl))
      /* This is invalid if initial value is not constant.
	 If it has either a function call, a memory reference,
	 or a variable, then re-evaluating it could give different results.  */
      && TREE_CONSTANT (DECL_INITIAL (decl))
      /* Check for cases where this is sub-optimal, even though valid.  */
      && (in_init || TREE_CODE (DECL_INITIAL (decl)) != CONSTRUCTOR))
    return DECL_INITIAL (decl);
  return decl;
}


/* Convert the array expression EXP to a pointer.  */
static tree array_to_pointer_conversion (location_t loc, tree exp)
{
  tree orig_exp = exp;
  tree type = TREE_TYPE (exp);
  tree adr;
  tree restype = TREE_TYPE (type);
  tree ptrtype;

  gcc_assert (TREE_CODE (type) == ARRAY_TYPE);

  STRIP_TYPE_NOPS (exp);

  if (TREE_NO_WARNING (orig_exp))
    TREE_NO_WARNING (exp) = 1;

  ptrtype = build_pointer_type (restype);

  if (INDIRECT_REF_P (exp))
    return aet_convert (ptrtype, TREE_OPERAND (exp, 0));

  /* In C++ array compound literals are temporary objects unless they are
     const or appear in namespace scope, so they are destroyed too soon
     to use them for much of anything  (c++/53220).  */
  if (warn_cxx_compat && TREE_CODE (exp) == COMPOUND_LITERAL_EXPR)
    {
      tree decl = TREE_OPERAND (TREE_OPERAND (exp, 0), 0);
      if (!TREE_READONLY (decl) && !TREE_STATIC (decl))
	warning_at (DECL_SOURCE_LOCATION (decl), OPT_Wc___compat,
		    "converting an array compound literal to a pointer "
		    "is ill-formed in C++");
    }

  adr = aet_build_unary_op (loc, ADDR_EXPR, exp, true);
  return aet_convert (ptrtype, adr);
}

/* Convert the function expression EXP to a pointer.  */
static tree function_to_pointer_conversion (location_t loc, tree exp)
{
  tree orig_exp = exp;

  gcc_assert (TREE_CODE (TREE_TYPE (exp)) == FUNCTION_TYPE);

  STRIP_TYPE_NOPS (exp);

  if (TREE_NO_WARNING (orig_exp))
    TREE_NO_WARNING (exp) = 1;

  return aet_build_unary_op (loc, ADDR_EXPR, exp, false);
}

/* Mark EXP as read, not just set, for set but not used -Wunused
   warning purposes.  */

void aet_mark_exp_read (tree exp)
{
  switch (TREE_CODE (exp))
    {
    case VAR_DECL:
    case PARM_DECL:
      DECL_READ_P (exp) = 1;
      break;
    case ARRAY_REF:
    case COMPONENT_REF:
    case MODIFY_EXPR:
    case REALPART_EXPR:
    case IMAGPART_EXPR:
    CASE_CONVERT:
    case ADDR_EXPR:
    case VIEW_CONVERT_EXPR:
      aet_mark_exp_read (TREE_OPERAND (exp, 0));
      break;
    case COMPOUND_EXPR:
    case C_MAYBE_CONST_EXPR:
      aet_mark_exp_read (TREE_OPERAND (exp, 1));
      break;
    default:
      break;
    }
}


/* Return whether EXPR should be treated as an atomic lvalue for the
   purposes of load and store handling.  */

static bool really_atomic_lvalue (tree expr)
{
  if (error_operand_p (expr))
    return false;
  if (!TYPE_ATOMIC (TREE_TYPE (expr)))
    return false;
  if (!aet_lvalue_p (expr))
    return false;

  /* Ignore _Atomic on register variables, since their addresses can't
     be taken so (a) atomicity is irrelevant and (b) the normal atomic
     sequences wouldn't work.  Ignore _Atomic on structures containing
     bit-fields, since accessing elements of atomic structures or
     unions is undefined behavior (C11 6.5.2.3#5), but it's unclear if
     it's undefined at translation time or execution time, and the
     normal atomic sequences again wouldn't work.  */
  while (handled_component_p (expr))
    {
      if (TREE_CODE (expr) == COMPONENT_REF
	  && DECL_C_BIT_FIELD (TREE_OPERAND (expr, 1)))
	return false;
      expr = TREE_OPERAND (expr, 0);
    }
  if (DECL_P (expr) && C_DECL_REGISTER (expr))
    return false;
  return true;
}


/* EXP is an expression of integer type.  Apply the integer promotions
   to it and return the promoted value.  */

tree aet_perform_integral_promotions (tree exp)
{
  tree type = TREE_TYPE (exp);
  enum tree_code code = TREE_CODE (type);

  gcc_assert (INTEGRAL_TYPE_P (type));

  /* Normally convert enums to int,
     but convert wide enums to something wider.  */
  if (code == ENUMERAL_TYPE){
	  //c_common_type_for_size 里没有改变，也没有警告和错误
      type = c_common_type_for_size (MAX (TYPE_PRECISION (type),
					  TYPE_PRECISION (integer_type_node)),
				     ((TYPE_PRECISION (type)
				       >= TYPE_PRECISION (integer_type_node))
				      && TYPE_UNSIGNED (type)));
      return aet_convert (type, exp);
   }

  /* ??? This should no longer be needed now bit-fields have their
     proper types.  */
  if (TREE_CODE (exp) == COMPONENT_REF
      && DECL_C_BIT_FIELD (TREE_OPERAND (exp, 1))
      /* If it's thinner than an int, promote it like a
	 c_promoting_integer_type_p, otherwise leave it alone.  */
      && compare_tree_int (DECL_SIZE (TREE_OPERAND (exp, 1)),
			   TYPE_PRECISION (integer_type_node)) < 0)
    return aet_convert (integer_type_node, exp);

  if (c_promoting_integer_type_p (type)){
      /* Preserve unsignedness if not really getting any wider.  */
      if (TYPE_UNSIGNED (type) && TYPE_PRECISION (type) == TYPE_PRECISION (integer_type_node))
	      return aet_convert (unsigned_type_node, exp);
      return aet_convert (integer_type_node, exp);
  }
  return exp;
}


/* Perform default promotions for C data used in expressions.
   Enumeral types or short or char are converted to int.
   In addition, manifest constants symbols are replaced by their values.  */

tree aet_default_conversion (tree exp)
{
  tree orig_exp;
  tree type = TREE_TYPE (exp);
  enum tree_code code = TREE_CODE (type);
  tree promoted_type;

  aet_mark_exp_read (exp);

  /* Functions and arrays have been converted during parsing.  */
  gcc_assert (code != FUNCTION_TYPE);
  if (code == ARRAY_TYPE)
    return exp;

  /* Constants can be used directly unless they're not loadable.  */
  if (TREE_CODE (exp) == CONST_DECL)
    exp = DECL_INITIAL (exp);

  /* Strip no-op conversions.  */
  orig_exp = exp;
  STRIP_TYPE_NOPS (exp);

  if (TREE_NO_WARNING (orig_exp))
    TREE_NO_WARNING (exp) = 1;

  if (code == VOID_TYPE)
    {
     // error_at (EXPR_LOC_OR_LOC (exp, input_location),"void value not ignored as it ought to be");
      argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=void_value_not_ignored_as_it_ought_to_be;
      return error_mark_node;
    }

  exp = aet_require_complete_type (EXPR_LOC_OR_LOC (exp, input_location), exp);
  if (exp == error_mark_node)
    return error_mark_node;

  promoted_type = targetm.promoted_type (type);
  if (promoted_type)
    return aet_convert (promoted_type, exp);

  if (INTEGRAL_TYPE_P (type))
    return aet_perform_integral_promotions (exp);

  return exp;
}


/* Give a note about the location of the declaration of DECL.  */

static void inform_declaration (tree decl)
{
  if (decl && (TREE_CODE (decl) != FUNCTION_DECL || !DECL_IS_BUILTIN (decl)))
    inform (DECL_SOURCE_LOCATION (decl), "declared here");
}

/* Build a function call to function FUNCTION with parameters PARAMS.
   If FUNCTION is the result of resolving an overloaded target built-in,
   ORIG_FUNDECL is the original function decl, otherwise it is null.
   ORIGTYPES, if not NULL, is a vector of types; each element is
   either NULL or the original type of the corresponding element in
   PARAMS.  The original type may differ from TREE_TYPE of the
   parameter for enums.  FUNCTION's data type may be a function type
   or pointer-to-function.  This function changes the elements of
   PARAMS.  */

tree aet_build_function_call_vec (location_t loc, vec<location_t> arg_loc,tree function,
		vec<tree, va_gc> *params,vec<tree, va_gc> *origtypes,void *userData)
{
	tree orig_fundecl=NULL;
  tree fntype, fundecl = NULL_TREE;
  tree name = NULL_TREE, result;
  tree tem;
  int nargs;
  tree *argarray;
  //printf("aet_build_function_call_vec 00 origtypes:%p function:%s %p\n",origtypes,get_tree_code_name(TREE_CODE (function)),function);
  /* Strip NON_LVALUE_EXPRs, etc., since we aren't using as an lvalue.  */
  STRIP_TYPE_NOPS (function);

  /* Convert anything with function type to a pointer-to-function.  */
  if (TREE_CODE (function) == FUNCTION_DECL){
      name = DECL_NAME (function);
      //printf("aet_build_function_call_vec 11 %s %p\n",IDENTIFIER_POINTER(name),function);
      if (flag_tm)
	     tm_malloc_replacement (function);
      fundecl = function;
      if (!orig_fundecl)
	     orig_fundecl = fundecl;
      /* Atomic functions have type checking/casting already done.  They are 
	 often rewritten and don't match the original parameter list.  */
      if (name && !strncmp (IDENTIFIER_POINTER (name), "__atomic_", 9))
        origtypes = NULL;
  }
  char *funName=name?IDENTIFIER_POINTER(name):"unknown";
  if (TREE_CODE (TREE_TYPE (function)) == FUNCTION_TYPE){
      //printf("aet_build_function_call_vec 22 %s %s %p\n",funName,get_tree_code_name(TREE_CODE (function)),function);
      function = function_to_pointer_conversion (loc, function);
      //printf("aet_build_function_call_vec ---22 %s %s %p\n",funName,get_tree_code_name(TREE_CODE (function)),function);
  }

  /* For Objective-C, convert any calls via a cast to OBJC_TYPE_REF
     expressions, like those used for ObjC messenger dispatches.  */
  if (params && !params->is_empty ()){
      //printf("aet_build_function_call_vec 33 %s %p\n",IDENTIFIER_POINTER(name),function);
      function = objc_rewrite_function_call (function, (*params)[0]);
  }
  function = c_fully_fold (function, false, NULL);
  fntype = TREE_TYPE (function);
  if (TREE_CODE (fntype) == ERROR_MARK){
	 printf("aet_build_function_call_vec 00 有错误 %s fntype code:%s function:%s %p\n",
			  funName,get_tree_code_name(TREE_CODE (fntype)),get_tree_code_name(TREE_CODE (function)),function);
     return error_mark_node;
  }

  if (!(TREE_CODE (fntype) == POINTER_TYPE	&& TREE_CODE (TREE_TYPE (fntype)) == FUNCTION_TYPE)){
	  //printf("aet_build_function_call_vec 55 %s\n",funName);

//      zclei if (!flag_diagnostics_show_caret)
//	     error_at (loc,"called object %qE is not a function or function pointer",function);
//      else if (DECL_P (function)){
//	     error_at (loc,"called object %qD is not a function or function pointer",function);
//	     inform_declaration (function);
//	  }else
//	     error_at (loc,"called object is not a function or function pointer");
      argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=called_object_is_not_a_function_or_function_pointer;
      return error_mark_node;
  }
  //printf("aet_build_function_call_vec 66 %s %p\n",funName,function);

  if (fundecl && TREE_THIS_VOLATILE (fundecl))
      current_function_returns_abnormally = 1;

      /* fntype now gets the type of function pointed to.  */
   fntype = TREE_TYPE (fntype);

      /* Convert the parameters to the types declared in the
        function prototype, or apply default promotions.  */
   //printf("aet_build_function_call_vec 77 %s %p %p \n",funName,function,fundecl);

   nargs = convert_arguments (loc, arg_loc, TYPE_ARG_TYPES (fntype), params,origtypes, function, fundecl,userData);
   if (nargs < 0)
        return error_mark_node;
      //printf("aet_build_function_call_vec 77 %s\n",name?IDENTIFIER_POINTER(name):"unknown");

       /* Check that the function is called through a compatible prototype.
        If it is not, warn.  */
   if (CONVERT_EXPR_P (function) && TREE_CODE (tem = TREE_OPERAND (function, 0)) == ADDR_EXPR
		  && TREE_CODE (tem = TREE_OPERAND (tem, 0)) == FUNCTION_DECL && !aet_comptypes (fntype, TREE_TYPE (tem))){
          tree return_type = TREE_TYPE (fntype);

		  /* This situation leads to run-time undefined behavior.  We can't,
		 therefore, simply error unless we can prove that all possible
		 executions of the program must execute the code.  */
          //zclei warning_at (loc, 0, "function called through a non-compatible type");
          argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=function_called_through_a_non_compatible_type;
          if (VOID_TYPE_P (return_type) && TYPE_QUALS (return_type) != TYPE_UNQUALIFIED)
	         //zclei pedwarn (loc, 0,"function with qualified void return type called");
             argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=function_with_qualified_void_return_type_called;

   }

   argarray = vec_safe_address (params);

     /* Check that arguments to builtin functions match the expectations.  */
  if (fundecl && fndecl_built_in_p (fundecl) && !check_builtin_function_arguments (loc, arg_loc, fundecl,orig_fundecl, nargs, argarray))
         return error_mark_node;
   //printf("aet_build_function_call_vec 88 %s\n",name?IDENTIFIER_POINTER(name):"unknown");

  /* Check that the arguments to the function are valid.  */
  bool warned_p = check_function_arguments (loc, fundecl, fntype, nargs, argarray, &arg_loc);
  if (name != NULL_TREE  && !strncmp (IDENTIFIER_POINTER (name), "__builtin_", 10)){
	  n_debug("aet_build_function_call_vec 99 调用__builtin_ %s",name?IDENTIFIER_POINTER(name):"unknown");
	 if (require_constant_value)
		 result= fold_build_call_array_initializer_loc (loc, TREE_TYPE (fntype),function, nargs, argarray);
	 else
		 result = fold_build_call_array_loc (loc, TREE_TYPE (fntype),function, nargs, argarray);
	 if (TREE_CODE (result) == NOP_EXPR && TREE_CODE (TREE_OPERAND (result, 0)) == INTEGER_CST)
		STRIP_TYPE_NOPS (result);
   }else{
	 n_debug("aet_build_function_call_vec 100 调用数组 %s",name?IDENTIFIER_POINTER(name):"unknown");
     result = build_call_array_loc (loc, TREE_TYPE (fntype),function, nargs, argarray);
   }
		  /* If -Wnonnull warning has been diagnosed, avoid diagnosing it again
			 later.  */
    if (warned_p && TREE_CODE (result) == CALL_EXPR)
           TREE_NO_WARNING (result) = 1;

  /* In this improbable scenario, a nested function returns a VM type.
     Create a TARGET_EXPR so that the call always has a LHS, much as
     what the C++ FE does for functions returning non-PODs.  */
   if (variably_modified_type_p (TREE_TYPE (fntype), NULL_TREE)){
	   n_debug("aet_build_function_call_vec 101 %s",name?IDENTIFIER_POINTER(name):"unknown");
	  tree tmp = create_tmp_var_raw (TREE_TYPE (fntype));
	  result = build4 (TARGET_EXPR, TREE_TYPE (fntype), tmp, result, NULL_TREE, NULL_TREE);
   }

   if (VOID_TYPE_P (TREE_TYPE (result))){
	   //printf("aet_build_function_call_vec 102 %s\n",name?IDENTIFIER_POINTER(name):"unknown");
        if (TYPE_QUALS (TREE_TYPE (result)) != TYPE_UNQUALIFIED)
	       //zclei pedwarn (loc, 0,"function with qualified void return type called");
           argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=function_with_qualified_void_return_type_called;
        return result;
   }
   //printf("aet_build_function_call_vec over over 103 %s\n",name?IDENTIFIER_POINTER(name):"unknown");
   return aet_require_complete_type (loc, result);
}

tree aet_check_funcs_param(location_t loc, vec<location_t> arg_loc,tree function, vec<tree, va_gc> *params,vec<tree, va_gc> *origtypes,void *userData)
{
	  /* Strip NON_LVALUE_EXPRs, etc., since we aren't using as an lvalue.  */
	   STRIP_TYPE_NOPS (function);
	  /* Convert anything with function type to a pointer-to-function.  */
	  if (TREE_CODE (function) == FUNCTION_DECL){
	      /* Implement type-directed function overloading for builtins.
		 resolve_overloaded_builtin and targetm.resolve_overloaded_builtin
		 handle all the type checking.  The result is a complete expression
		 that implements this function call.  */
	      tree tem = aet_resolve_overloaded_builtin (loc, function, params);
	      if (tem)
		   return tem;
	   }
	   return aet_build_function_call_vec (loc, arg_loc, function, params, origtypes,userData);
}


/* Helper for convert_arguments called to convert the VALue of argument
   number ARGNUM from ORIGTYPE to the corresponding parameter number
   PARMNUM and TYPE.
   PLOC is the location where the conversion is being performed.
   FUNCTION and FUNDECL are the same as in convert_arguments.
   VALTYPE is the original type of VAL before the conversion and,
   for EXCESS_PRECISION_EXPR, the operand of the expression.
   NPC is true if VAL represents the null pointer constant (VAL itself
   will have been folded to an integer constant).
   RNAME is the same as FUNCTION except in Objective C when it's
   the function selector.
   EXCESS_PRECISION is true when VAL was originally represented
   as EXCESS_PRECISION_EXPR.
   WARNOPT is the same as in convert_for_assignment.  */
//function 被改过的原函数 fundecl未改过的函数声明 type 函数参数类型  origtype 实参的类型 val具体的实参 valtype具体实参的类型 npc实参是否是空指针
//rname =function parmnum当前第几个参数 argnum 当前第几个参数+1 excess_precision 超精度 0 是否警告
static tree convert_argument (location_t ploc, tree function, tree fundecl,
		  tree type, tree origtype, tree val, tree valtype,
		  bool npc, tree rname, int parmnum, int argnum,
		  bool excess_precision, int warnopt)
{
  /* Formal parm type is specified by a function prototype.  */

    if (type == error_mark_node || !COMPLETE_TYPE_P (type)){
       //zclei error_at (ploc, "type of formal parameter %d is incomplete",parmnum + 1);
	   argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=type_of_formal_parameter_xx_is_incomplete;
       return val;
    }

  /* Optionally warn about conversions that differ from the default
     conversions.  */
    if (warn_traditional_conversion || warn_traditional){

       unsigned int formal_prec = TYPE_PRECISION (type);
       if (INTEGRAL_TYPE_P (type) && TREE_CODE (valtype) == REAL_TYPE)
    	 //warning_at (ploc, OPT_Wtraditional_conversion,"passing argument %d of %qE as integer rather than floating due to prototype",argnum, rname);
   	      argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=passing_argument_xx_of_xx_as_integer_rather_than_floating_due_to_prototype;
       if (INTEGRAL_TYPE_P (type) && TREE_CODE (valtype) == COMPLEX_TYPE)
	      //warning_at (ploc, OPT_Wtraditional_conversion,"passing argument %d of %qE as integer rather than complex due to prototype",argnum, rname);
    	   argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=passing_argument_xx_of_xx_as_integer_rather_than_complex_due_to_prototype;

       else if (TREE_CODE (type) == COMPLEX_TYPE  && TREE_CODE (valtype) == REAL_TYPE)
	      //warning_at (ploc, OPT_Wtraditional_conversion,"passing argument %d of %qE as complex rather than floating due to prototype",argnum, rname);
    	   argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=passing_argument_xx_of_xx_as_complex_rather_than_floating_due_to_prototype;

       else if (TREE_CODE (type) == REAL_TYPE  && INTEGRAL_TYPE_P (valtype))
	     //warning_at (ploc, OPT_Wtraditional_conversion,"passing argument %d of %qE as floating rather than integer due to prototype",argnum, rname);
    	   argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=passing_argument_xx_of_xx_as_floating_rather_than_integer_due_to_prototype;

       else if (TREE_CODE (type) == COMPLEX_TYPE  && INTEGRAL_TYPE_P (valtype))
	     //warning_at (ploc, OPT_Wtraditional_conversion,"passing argument %d of %qE as complex rather than integer due to prototype",argnum, rname);
    	   argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=passing_argument_xx_of_xx_as_complex_rather_than_integer_due_to_prototype;

       else if (TREE_CODE (type) == REAL_TYPE  && TREE_CODE (valtype) == COMPLEX_TYPE)
	      //warning_at (ploc, OPT_Wtraditional_conversion,"passing argument %d of %qE as floating rather than complex due to prototype",argnum, rname);
    	   argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=passing_argument_xx_of_xx_as_floating_rather_than_complex_due_to_prototype;

      /* ??? At some point, messages should be written about
	 conversions between complex types, but that's too messy
	 to do now.  */
       else if (TREE_CODE (type) == REAL_TYPE && TREE_CODE (valtype) == REAL_TYPE){
	  /* Warn if any argument is passed as `float',
	     since without a prototype it would be `double'.  */
	      if (formal_prec == TYPE_PRECISION (float_type_node) && type != dfloat32_type_node)
	         //warning_at (ploc, 0,"passing argument %d of %qE as %<float%> rather than %<double%> due to prototype",argnum, rname);
   	         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=passing_argument_xx_of_xx_as_float_rather_than_double_due_to_prototype;

	  /* Warn if mismatch between argument and prototype
	     for decimal float types.  Warn of conversions with
	     binary float types and of precision narrowing due to
	     prototype.  */
	      else if (type != valtype
		      && (type == dfloat32_type_node
		       || type == dfloat64_type_node
		       || type == dfloat128_type_node
		       || valtype == dfloat32_type_node
		       || valtype == dfloat64_type_node
		       || valtype == dfloat128_type_node)
		      && (formal_prec  <= TYPE_PRECISION (valtype)
		       || (type == dfloat128_type_node
			   && (valtype
			       != dfloat64_type_node
			       && (valtype
				   != dfloat32_type_node)))
		       || (type == dfloat64_type_node
			   && (valtype
			       != dfloat32_type_node))))
	        // warning_at (ploc, 0,"passing argument %d of %qE as %qT rather than %qT due to prototype",argnum, rname, type, valtype);
	         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=passing_argument_xx_of_xx_as_xx_rather_than_xx_due_to_prototype;

	   }
      /* Detect integer changing in width or signedness.
	 These warnings are only activated with
	 -Wtraditional-conversion, not with -Wtraditional.  */
      else if (warn_traditional_conversion && INTEGRAL_TYPE_P (type) && INTEGRAL_TYPE_P (valtype)){
	      tree would_have_been = aet_default_conversion (val);
	      tree type1 = TREE_TYPE (would_have_been);
	      if (val == error_mark_node)
	         /* VAL could have been of incomplete type.  */;
	      else if (TREE_CODE (type) == ENUMERAL_TYPE && (TYPE_MAIN_VARIANT (type) == TYPE_MAIN_VARIANT (valtype)))
	    /* No warning if function asks for enum
	       and the actual arg is that enum type.  */
	         ;
	      else if (formal_prec != TYPE_PRECISION (type1))
	         // warning_at (ploc, OPT_Wtraditional_conversion,"passing argument %d of %qE with different width due to prototype",argnum, rname);
	         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=passing_argument_xx_of_xx_with_different_width_due_to_prototype;
	      else if (TYPE_UNSIGNED (type) == TYPE_UNSIGNED (type1))
	           ;
		  /* Don't complain if the formal parameter type
			 is an enum, because we can't tell now whether
			 the value was an enum--even the same enum.  */
	      else if (TREE_CODE (type) == ENUMERAL_TYPE)
	         ;
	      else if (TREE_CODE (val) == INTEGER_CST && int_fits_type_p (val, type))
			/* Change in signedness doesn't matter
			   if a constant value is unaffected.  */
	        ;
		  /* If the value is extended from a narrower
			 unsigned type, it doesn't matter whether we
			 pass it as signed or unsigned; the value
			 certainly is the same either way.  */
	     else if (TYPE_PRECISION (valtype) < TYPE_PRECISION (type)  && TYPE_UNSIGNED (valtype))
	        ;
	     else if (TYPE_UNSIGNED (type))
	        //warning_at (ploc, OPT_Wtraditional_conversion,"passing argument %d of %qE as unsigned due to prototype",argnum, rname);
	         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=passing_argument_xx_of_xx_as_unsigned_due_to_prototype;
	     else
	        //warning_at (ploc, OPT_Wtraditional_conversion,"passing argument %d of %qE as signed due to prototype",argnum, rname);
	         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=passing_argument_xx_of_xx_as_signed_due_to_prototype;

	   }
    }

  /* Possibly restore an EXCESS_PRECISION_EXPR for the
     sake of better warnings from convert_and_check.  */
    if (excess_precision)
       val = build1 (EXCESS_PRECISION_EXPR, valtype, val);

    tree parmval = convert_for_assignment (ploc, ploc, type,
					 val, origtype, ic_argpass,
					 npc, fundecl, function,
					 parmnum + 1, warnopt);

    if (targetm.calls.promote_prototypes (fundecl ? TREE_TYPE (fundecl) : 0)  && INTEGRAL_TYPE_P (type) && (TYPE_PRECISION (type) < TYPE_PRECISION (integer_type_node)))
       parmval = aet_default_conversion (parmval);

    return parmval;
}

/* Convert the argument expressions in the vector VALUES
   to the types in the list TYPELIST.

   If TYPELIST is exhausted, or when an element has NULL as its type,
   perform the default conversions.

   ORIGTYPES is the original types of the expressions in VALUES.  This
   holds the type of enum values which have been converted to integral
   types.  It may be NULL.

   FUNCTION is a tree for the called function.  It is used only for
   error messages, where it is formatted with %qE.

   This is also where warnings about wrong number of args are generated.

   ARG_LOC are locations of function arguments (if any).

   Returns the actual number of arguments processed (which may be less
   than the length of VALUES in some error situations), or -1 on
   failure.  */

static int convert_arguments (location_t loc, vec<location_t> arg_loc, tree typelist,
		   vec<tree, va_gc> *values, vec<tree, va_gc> *origtypes, tree function, tree fundecl,void *userData)
{
    unsigned int parmnum;
    bool error_args = false;
    const bool type_generic = fundecl && lookup_attribute ("type generic", TYPE_ATTRIBUTES (TREE_TYPE (fundecl)));
    bool type_generic_remove_excess_precision = false;
    bool type_generic_overflow_p = false;
    /* Change pointer to function to the function itself for
     diagnostics.  */
    if(TREE_CODE (function) == ADDR_EXPR  && TREE_CODE (TREE_OPERAND (function, 0)) == FUNCTION_DECL){
       function = TREE_OPERAND (function, 0);
       n_debug("aet-typeck convert_arguments 00 function:%p fundecl:%p",function,fundecl);
    }

  /* For a call to a built-in function declared without a prototype,
     set to the built-in function's argument list.  */
    tree builtin_typelist = NULL_TREE;

  /* For type-generic built-in functions, determine whether excess
     precision should be removed (classification) or not
     (comparison).  */
    if (fundecl   && fndecl_built_in_p (fundecl, BUILT_IN_NORMAL)){
        printf("convert_arguments 00 fundecl:%s origtypes:%p %s %s %d\n",get_tree_code_name(TREE_CODE(fundecl)),origtypes,__FILE__,__FUNCTION__,__LINE__);
       built_in_function code = DECL_FUNCTION_CODE (fundecl);
       if (C_DECL_BUILTIN_PROTOTYPE (fundecl)){
	  /* For a call to a built-in function declared without a prototype
	     use the types of the parameters of the internal built-in to
	     match those of the arguments to.  */
           printf("convert_arguments 22 fundecl:%s origtypes:%p code:%d %s %s %d\n",
    	  	        		 get_tree_code_name(TREE_CODE(fundecl)),origtypes,code,__FILE__,__FUNCTION__,__LINE__);
	      if (tree bdecl = builtin_decl_explicit (code))
	         builtin_typelist = TYPE_ARG_TYPES (TREE_TYPE (bdecl));
	   }

      /* For type-generic built-in functions, determine whether excess
	 precision should be removed (classification) or not
	 (comparison).  */
       if (type_generic){
           printf("convert_arguments 33 fundecl:%s origtypes:%p %s %s %d\n",
    	  	        		 get_tree_code_name(TREE_CODE(fundecl)),origtypes,__FILE__,__FUNCTION__,__LINE__);
	      switch (code){
	         case BUILT_IN_ISFINITE:
	         case BUILT_IN_ISINF:
	         case BUILT_IN_ISINF_SIGN:
	         case BUILT_IN_ISNAN:
	         case BUILT_IN_ISNORMAL:
	         case BUILT_IN_FPCLASSIFY:
	            type_generic_remove_excess_precision = true;
	            break;

	         case BUILT_IN_ADD_OVERFLOW_P:
	         case BUILT_IN_SUB_OVERFLOW_P:
	         case BUILT_IN_MUL_OVERFLOW_P:
			/* The last argument of these type-generic builtins
			   should not be promoted.  */
	            type_generic_overflow_p = true;
	            break;
	         default:
	            break;
	      }
       }
    }
    nboolean isFuncGeneric=generic_util_is_generic_func(function);
    nboolean isQueryGenFunc=generic_util_is_query_generic_func(function);
    char *funcNameStr=IDENTIFIER_POINTER(DECL_NAME(function));

  /* Scan the given expressions (VALUES) and types (TYPELIST), producing
     individual converted arguments.  */
    tree typetail, builtin_typetail, val;
    for (typetail = typelist,builtin_typetail = builtin_typelist,parmnum = 0; values && values->iterate (parmnum, &val);++parmnum){
        /* The type of the function parameter (if it was declared with one).  */
       tree type = typetail ? TREE_VALUE (typetail) : NULL_TREE;
        /* The type of the built-in function parameter (if the function
	   is a built-in).  Used to detect type incompatibilities in
	   calls to built-ins declared without a prototype.  */
       tree builtin_type = (builtin_typetail ? TREE_VALUE (builtin_typetail) : NULL_TREE);
        /* The original type of the argument being passed to the function.  */
       tree valtype = TREE_TYPE (val);
        /* The called function (or function selector in Objective C).  */
       tree rname = function;
       int argnum = ((isFuncGeneric || isQueryGenFunc)&& parmnum==1)?parmnum+2:parmnum + 1;//泛型加两个参数
       const char *invalid_func_diag;
       /* Set for EXCESS_PRECISION_EXPR arguments.  */
       bool excess_precision = false;
       /* The value of the argument after conversion to the type
	    of the function parameter it is passed to.  */
       tree parmval;
        /* Some __atomic_* builtins have additional hidden argument at
	    position 0.  */
       location_t ploc= !arg_loc.is_empty () && values->length () == arg_loc.length ()
	  ? expansion_point_location_if_in_system_header (arg_loc[parmnum]): input_location;

       if (type == void_type_node){
          //zclei error_at (loc, "too many arguments to function %qE", function);
	      //inform_declaration (fundecl);
	      n_debug("convert_arguments 55 进入循环 错误 type == void_type_node 直接返回 parmnum:%d error_args:%d",parmnum,error_args);
	      argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=too_many_arguments_to_function;
	      return error_args ? -1 : (int) parmnum;
	   }
       if (builtin_type == void_type_node){
	      //zclei if (warning_at (loc, OPT_Wbuiltin_declaration_mismatch, "too many arguments to built-in function %qE expecting %d", function, parmnum))
	       //  inform_declaration (fundecl);
	      printf("convert_arguments 66 进入循环 错误 builtin_type == void_type_node parmnum:%d error_args:%d",parmnum,error_args);
	      argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=too_many_arguments_to_built_in_function_expecting;
	      builtin_typetail = NULL_TREE;
	   }


      /* Determine if VAL is a null pointer constant before folding it.  */
       bool npc = null_pointer_constant_p (val);
		  /* If there is excess precision and a prototype, convert once to
		 the required type rather than converting via the semantic
		 type.  Likewise without a prototype a float value represented
		 as long double should be converted once to double.  But for
		 type-generic classification functions excess precision must
		 be removed here.  */
       if (TREE_CODE (val) == EXCESS_PRECISION_EXPR && (type || !type_generic || !type_generic_remove_excess_precision)){
	      val = TREE_OPERAND (val, 0);
	      excess_precision = true;
	   }
       tree tempval = aet_c_fully_fold (val, false, NULL);
       n_debug("convert_arguments aet_c_fully_fold  tempval:%p val:%p 未改变:%d function:%p funcName:%s npc:%d parmnum:%d",
               tempval,val,tempval==val,function,funcNameStr,npc,parmnum);
       val=tempval;
       STRIP_TYPE_NOPS (val);
       val = aet_require_complete_type (ploc, val);
       if(val==error_mark_node)
    	   return -1;

      /* Some floating-point arguments must be promoted to double when
	   no type is specified by a prototype.  This applies to
	   arguments of type float, and to architecture-specific types
	   (ARM __fp16), but not to _FloatN or _FloatNx types.  */
       bool promote_float_arg = false;
       if (type == NULL_TREE && TREE_CODE (valtype) == REAL_TYPE
    	  && (TYPE_PRECISION (valtype)<= TYPE_PRECISION (double_type_node))
	      && TYPE_MAIN_VARIANT (valtype) != double_type_node
	      && TYPE_MAIN_VARIANT (valtype) != long_double_type_node
	      && !DECIMAL_FLOAT_MODE_P (TYPE_MODE (valtype))){
	  /* Promote this argument, unless it has a _FloatN or
	     _FloatNx type.  */
    	  n_debug("convert_arguments 77 进入循环  parmnum:%d error_args:%d",parmnum,error_args);
	      promote_float_arg = true;
	      for (int i = 0; i < NUM_FLOATN_NX_TYPES; i++)
	         if (TYPE_MAIN_VARIANT (valtype) == FLOATN_NX_TYPE_NODE (i)){
		        promote_float_arg = false;
		        break;
	         }
	   }

       if (type != NULL_TREE){
	      tree origtype = (!origtypes) ? NULL_TREE : (*origtypes)[parmnum];
	      //function 被改过的原函数 fundecl未改过的函数声明 type 函数参数类型  origtype 实参的类型 val具体的实参 valtype具体实参的类型 npc实参是否是空指针
	      //rname =function parmnum当前第几个参数 argnum 总的参数个数+1 excess_precision 超精度 0 是否警告
	      nboolean isGenericType=generic_util_is_generic_pointer(type);
	      n_debug("convert_arguments 88 进入循环 调convert_argument origtype code:%s  parmnum:%d error_args:%d 是不是泛型:%d function:%p isFuncGeneric:%d isQueryGenFunc:%d\n",
		         			  origtype?get_tree_code_name(TREE_CODE(origtype)):"NULL", parmnum,error_args,isGenericType,function,isFuncGeneric,isQueryGenFunc);
		  if(isGenericType){
		      if(userData!=NULL){
                  CheckParamCallback *check=(CheckParamCallback *)userData;
                  parmval=check->callback(check,1,ploc,function,type,val,npc,excess_precision);
		      }
			  if(parmval==error_mark_node){
				 argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=error_or_warn_unknown;
				 return -1;
			  }
		  }else{
//		     printf("check -----形参:\n");
//		     printNode(origtype);
//             printf("check -----实参:\n\n");
//             printNode(val);
//             printf("check -----type:\n\n");
//             printNode(type);
//             printf("check -----valtype:\n\n");
//             printNode(valtype);
            // printf("check -----is static func:%d\n\n",is);
		      if(userData!=NULL){
                   CheckParamCallback *check=(CheckParamCallback *)userData;
                   if(check->addFuncPointer(check,parmnum,val,type)){
                       parmval=val;
                   }else{
                       parmval=convert_argument(ploc, function, fundecl, type, origtype,val, valtype, npc, rname, parmnum, argnum,excess_precision, 0);
                   }
		      }else{
	              parmval=convert_argument(ploc, function, fundecl, type, origtype,val, valtype, npc, rname, parmnum, argnum,excess_precision, 0);
		      }
		  }
	   }else if (promote_float_arg){
	      if (type_generic)
	         parmval = val;
	      else{
	          /* Convert `float' to `double'.  */
	    	  printf("convert_arguments 99 进入循环 Convert `float' to `double  parmnum:%d error_args:%d",parmnum,error_args);
	         if (warn_double_promotion && !c_inhibit_evaluation_warnings){
		        //zclei warning_at (ploc, OPT_Wdouble_promotion,"implicit conversion from %qT to %qT when passing argument to function",valtype, double_type_node);
			    argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=implicit_conversion_from_xx_to_xx_when_passing_argument_to_function;
	         }
	         parmval = aet_convert (double_type_node, val);
	      }
	   }else if ((excess_precision && !type_generic)|| (type_generic_overflow_p && parmnum == 2)){
			/* A "double" argument with excess precision being passed
			   without a prototype or in variable arguments.
			   The last argument of __builtin_*_overflow_p should not be
			   promoted.  */
	       n_debug("convert_arguments 100 进入循环 还是关于double  parmnum:%d error_args:%d",parmnum,error_args);
	         parmval = aet_convert (valtype, val);
	   }else if ((invalid_func_diag =targetm.calls.invalid_arg_for_unprototyped_fn (typelist, fundecl, val))){
	       n_debug("convert_arguments 101 进入循环 出错了返回-1 invalid_func_diag =targetm.calls.invalid_a... parmnum:%d error_args:%d",parmnum,error_args);
	      //zclei error (invalid_func_diag);
		  argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=invalid_func_diag;
	      return -1;
	   }else if (TREE_CODE (val) == ADDR_EXPR && reject_gcc_builtin (val)){
	       n_debug("convert_arguments 102 进入循环 出错了返回-1 TREE_CODE (val) == ADDR_EXPR && reject_gcc_builtin (val) parmnum:%d error_args:%d",
		  		  	    		     	     parmnum,error_args);
		   argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=error_or_warn_unknown;
	      return -1;
	   }else{
	      /* Convert `short' and `char' to full-size `int'.  */
	       n_debug("convert_arguments 103 进入循环  Convert `short' and `char' to full-size `int parmnum:%d error_args:%d",parmnum,error_args);
	       parmval = aet_default_conversion (val);
	   }

       //这句不能用,否则改变了实参，下个函数不能匹配了
       //(*values)[parmnum] = parmval;
       if (parmval == error_mark_node){
    	   n_debug("convert_arguments 106 出错了 返回-1 parmval == error_mark_node parmnum:%d",parmnum);
	       error_args = true;
		   argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=error_or_warn_unknown;
	      //zclei add 直接返回了
	       return -1;
       }

       if (!type && builtin_type && TREE_CODE (builtin_type) != VOID_TYPE){
	  /* For a call to a built-in function declared without a prototype,
	     perform the conversions from the argument to the expected type
	     but issue warnings rather than errors for any mismatches.
	     Ignore the converted argument and use the PARMVAL obtained
	     above by applying default conversions instead.  */
    	   n_debug("convert_arguments 104 进入循环  调用 convert_argument parmnum:%d error_args:%d",parmnum,error_args);
	      tree origtype = (!origtypes) ? NULL_TREE : (*origtypes)[parmnum];
	      convert_argument (ploc, function, fundecl, builtin_type, origtype,
			    val, valtype, npc, rname, parmnum, argnum,excess_precision,OPT_Wbuiltin_declaration_mismatch);
	   }
	   n_debug("convert_arguments 105 进入循环  结束一次循环 aet-typeck.c typetail:%p builtin_typetail:%p parmnum:%d error_args:%d",
			   typetail, builtin_typetail,parmnum,error_args);
       if (typetail){
	      typetail = TREE_CHAIN (typetail);
	      if((isFuncGeneric || isQueryGenFunc)&& parmnum==0){
		      typetail = TREE_CHAIN (typetail);
		      n_debug("convert_arguments 105 跳过泛型函数的第二参数\n");
	      }
       }
       if (builtin_typetail){
	      builtin_typetail = TREE_CHAIN (builtin_typetail);
       }

    }//end for

    gcc_assert (parmnum == vec_safe_length (values));

    if (typetail != NULL_TREE && TREE_VALUE (typetail) != void_type_node){
       //zcleierror_at (loc, "too few arguments to function %qE", function);
       //inform_declaration (fundecl);
        n_debug("convert_arguments 106 出错了 返回-1 typetail != NULL_TREE && TREE_VALUE (typetail) != void_type_node parmnum:%d error_args:%d",
			   parmnum,error_args);
	   argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=too_few_arguments_to_function;
      return -1;
    }

    if (builtin_typetail && TREE_VALUE (builtin_typetail) != void_type_node){
 	   printf("convert_arguments 107 出错了 builtin_typetail && TREE_VALUE (builtin_typetail) != void_type_node parmnum:%d error_args:%d",
 			   parmnum,error_args);
       unsigned nargs = parmnum;
       for (tree t = builtin_typetail; t; t = TREE_CHAIN (t))
	      ++nargs;
       //zclei if (warning_at (loc, OPT_Wbuiltin_declaration_mismatch,"too few arguments to built-in function %qE expecting %u", function, nargs - 1))
	    //zclei inform_declaration (fundecl);
	    argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=too_few_arguments_to_built_in_function_xx_expecting_xx;

    }
    return error_args ? -1 : (int) parmnum;
}

//
/* Returns true if TYPE is a character type, *not* including wchar_t.  */

static bool char_type_p (tree type)
{
  return (type == char_type_node
	  || type == unsigned_char_type_node
	  || type == signed_char_type_node
	  || type == char16_type_node
	  || type == char32_type_node);
}


/* Return a tree for the difference of pointers OP0 and OP1.
   The resulting tree has type ptrdiff_t.  If POINTER_SUBTRACT sanitization is
   enabled, assign to INSTRUMENT_EXPR call to libsanitizer.  */

static tree pointer_diff (location_t loc, tree op0, tree op1, tree *instrument_expr)
{
  tree restype = ptrdiff_type_node;
  tree result, inttype;

  addr_space_t as0 = TYPE_ADDR_SPACE (TREE_TYPE (TREE_TYPE (op0)));
  addr_space_t as1 = TYPE_ADDR_SPACE (TREE_TYPE (TREE_TYPE (op1)));
  tree target_type = TREE_TYPE (TREE_TYPE (op0));
  tree orig_op0 = op0;
  tree orig_op1 = op1;

  /* If the operands point into different address spaces, we need to
     explicitly convert them to pointers into the common address space
     before we can subtract the numerical address values.  */
  if (as0 != as1)
    {
      addr_space_t as_common;
      tree common_type;

      /* Determine the common superset address space.  This is guaranteed
	 to exist because the caller verified that comp_target_types
	 returned non-zero.  */
      if (!addr_space_superset (as0, as1, &as_common))
	gcc_unreachable ();

      common_type = common_pointer_type (TREE_TYPE (op0), TREE_TYPE (op1));
      op0 = aet_convert (common_type, op0);
      op1 = aet_convert (common_type, op1);
    }

  /* Determine integer type result of the subtraction.  This will usually
     be the same as the result type (ptrdiff_t), but may need to be a wider
     type if pointers for the address space are wider than ptrdiff_t.  */
  if (TYPE_PRECISION (restype) < TYPE_PRECISION (TREE_TYPE (op0)))
    inttype = c_common_type_for_size (TYPE_PRECISION (TREE_TYPE (op0)), 0);
  else
    inttype = restype;

  if (TREE_CODE (target_type) == VOID_TYPE)
    pedwarn (loc, OPT_Wpointer_arith,
	     "pointer of type %<void *%> used in subtraction");
  if (TREE_CODE (target_type) == FUNCTION_TYPE)
    pedwarn (loc, OPT_Wpointer_arith,
	     "pointer to a function used in subtraction");

  if (sanitize_flags_p (SANITIZE_POINTER_SUBTRACT))
    {
      gcc_assert (current_function_decl != NULL_TREE);

      op0 = save_expr (op0);
      op1 = save_expr (op1);

      tree tt = builtin_decl_explicit (BUILT_IN_ASAN_POINTER_SUBTRACT);
      *instrument_expr = build_call_expr_loc (loc, tt, 2, op0, op1);
    }

  /* First do the subtraction, then build the divide operator
     and only convert at the very end.
     Do not do default conversions in case restype is a short type.  */

  /* POINTER_DIFF_EXPR requires a signed integer type of the same size as
     pointers.  If some platform cannot provide that, or has a larger
     ptrdiff_type to support differences larger than half the address
     space, cast the pointers to some larger integer type and do the
     computations in that type.  */
  if (TYPE_PRECISION (inttype) > TYPE_PRECISION (TREE_TYPE (op0)))
    op0 = aet_build_binary_op (loc, MINUS_EXPR, aet_convert (inttype, op0),
			   aet_convert (inttype, op1), false);
  else
    {
      /* Cast away qualifiers.  */
      op0 = aet_convert (c_common_type (TREE_TYPE (op0), TREE_TYPE (op0)), op0);
      op1 = aet_convert (c_common_type (TREE_TYPE (op1), TREE_TYPE (op1)), op1);
      op0 = build2_loc (loc, POINTER_DIFF_EXPR, inttype, op0, op1);
    }

  /* This generates an error if op1 is pointer to incomplete type.  */
  if (!COMPLETE_OR_VOID_TYPE_P (TREE_TYPE (TREE_TYPE (orig_op1))))
    error_at (loc, "arithmetic on pointer to an incomplete type");
  else if (verify_type_context (loc, TCTX_POINTER_ARITH,
				TREE_TYPE (TREE_TYPE (orig_op0))))
    verify_type_context (loc, TCTX_POINTER_ARITH,
			 TREE_TYPE (TREE_TYPE (orig_op1)));

  op1 = c_size_in_bytes (target_type);

  if (pointer_to_zero_sized_aggr_p (TREE_TYPE (orig_op1)))
    error_at (loc, "arithmetic on pointer to an empty aggregate");

  /* Divide by the size, in easiest possible way.  */
  result = fold_build2_loc (loc, EXACT_DIV_EXPR, inttype,op0, aet_convert (inttype, op1));

  /* Convert to final result type if necessary.  */
  return aet_convert (restype, result);
}

/* Expand atomic compound assignments into an appropriate sequence as
   specified by the C11 standard section 6.5.16.2.

       _Atomic T1 E1
       T2 E2
       E1 op= E2

  This sequence is used for all types for which these operations are
  supported.

  In addition, built-in versions of the 'fe' prefixed routines may
  need to be invoked for floating point (real, complex or vector) when
  floating-point exceptions are supported.  See 6.5.16.2 footnote 113.

  T1 newval;
  T1 old;
  T1 *addr
  T2 val
  fenv_t fenv

  addr = &E1;
  val = (E2);
  __atomic_load (addr, &old, SEQ_CST);
  feholdexcept (&fenv);
loop:
    newval = old op val;
    if (__atomic_compare_exchange_strong (addr, &old, &newval, SEQ_CST,
					  SEQ_CST))
      goto done;
    feclearexcept (FE_ALL_EXCEPT);
    goto loop:
done:
  feupdateenv (&fenv);

  The compiler will issue the __atomic_fetch_* built-in when possible,
  otherwise it will generate the generic form of the atomic operations.
  This requires temp(s) and has their address taken.  The atomic processing
  is smart enough to figure out when the size of an object can utilize
  a lock-free version, and convert the built-in call to the appropriate
  lock-free routine.  The optimizers will then dispose of any temps that
  are no longer required, and lock-free implementations are utilized as
  long as there is target support for the required size.

  If the operator is NOP_EXPR, then this is a simple assignment, and
  an __atomic_store is issued to perform the assignment rather than
  the above loop.  */

/* Build an atomic assignment at LOC, expanding into the proper
   sequence to store LHS MODIFYCODE= RHS.  Return a value representing
   the result of the operation, unless RETURN_OLD_P, in which case
   return the old value of LHS (this is only for postincrement and
   postdecrement).  */

static tree build_atomic_assign (location_t loc, tree lhs, enum tree_code modifycode,tree rhs, bool return_old_p)
{
  tree fndecl, func_call;
  vec<tree, va_gc> *params;
  tree val, nonatomic_lhs_type, nonatomic_rhs_type, newval, newval_addr;
  tree old, old_addr;
  tree compound_stmt;
  tree stmt, goto_stmt;
  tree loop_label, loop_decl, done_label, done_decl;

  tree lhs_type = TREE_TYPE (lhs);
  tree lhs_addr = aet_build_unary_op (loc, ADDR_EXPR, lhs, false);
  tree seq_cst = build_int_cst (integer_type_node, MEMMODEL_SEQ_CST);
  tree rhs_semantic_type = TREE_TYPE (rhs);
  tree nonatomic_rhs_semantic_type;
  tree rhs_type;

  gcc_assert (TYPE_ATOMIC (lhs_type));

  if (return_old_p)
    gcc_assert (modifycode == PLUS_EXPR || modifycode == MINUS_EXPR);

  /* Allocate enough vector items for a compare_exchange.  */
  vec_alloc (params, 6);

  /* Create a compound statement to hold the sequence of statements
     with a loop.  */
  compound_stmt = aet_c_begin_compound_stmt (false);

  /* Remove any excess precision (which is only present here in the
     case of compound assignments).  */
  if (TREE_CODE (rhs) == EXCESS_PRECISION_EXPR)
    {
      gcc_assert (modifycode != NOP_EXPR);
      rhs = TREE_OPERAND (rhs, 0);
    }
  rhs_type = TREE_TYPE (rhs);

  /* Fold the RHS if it hasn't already been folded.  */
  if (modifycode != NOP_EXPR)
    rhs = c_fully_fold (rhs, false, NULL);

  /* Remove the qualifiers for the rest of the expressions and create
     the VAL temp variable to hold the RHS.  */
  nonatomic_lhs_type = build_qualified_type (lhs_type, TYPE_UNQUALIFIED);
  nonatomic_rhs_type = build_qualified_type (rhs_type, TYPE_UNQUALIFIED);
  nonatomic_rhs_semantic_type = build_qualified_type (rhs_semantic_type,
						      TYPE_UNQUALIFIED);
  val = create_tmp_var_raw (nonatomic_rhs_type);
  TREE_ADDRESSABLE (val) = 1;
  TREE_NO_WARNING (val) = 1;
  rhs = build4 (TARGET_EXPR, nonatomic_rhs_type, val, rhs, NULL_TREE,
		NULL_TREE);
  SET_EXPR_LOCATION (rhs, loc);
  add_stmt (rhs);

  /* NOP_EXPR indicates it's a straight store of the RHS. Simply issue
     an atomic_store.  */
  if (modifycode == NOP_EXPR)
    {
      /* Build __atomic_store (&lhs, &val, SEQ_CST)  */
      rhs = aet_build_unary_op (loc, ADDR_EXPR, val, false);
      fndecl = builtin_decl_explicit (BUILT_IN_ATOMIC_STORE);
      params->quick_push (lhs_addr);
      params->quick_push (rhs);
      params->quick_push (seq_cst);
      func_call = c_build_function_call_vec (loc, vNULL, fndecl, params, NULL);
      add_stmt (func_call);

      /* Finish the compound statement.  */
      compound_stmt = aet_c_end_compound_stmt (loc, compound_stmt, false);

      /* VAL is the value which was stored, return a COMPOUND_STMT of
	 the statement and that value.  */
      return build2 (COMPOUND_EXPR, nonatomic_lhs_type, compound_stmt, val);
    }

  /* Attempt to implement the atomic operation as an __atomic_fetch_* or
     __atomic_*_fetch built-in rather than a CAS loop.  atomic_bool type
     isn't applicable for such builtins.  ??? Do we want to handle enums?  */
  if ((TREE_CODE (lhs_type) == INTEGER_TYPE || POINTER_TYPE_P (lhs_type))
      && TREE_CODE (rhs_type) == INTEGER_TYPE)
    {
      built_in_function fncode;
      switch (modifycode)
	{
	case PLUS_EXPR:
	case POINTER_PLUS_EXPR:
	  fncode = (return_old_p
		    ? BUILT_IN_ATOMIC_FETCH_ADD_N
		    : BUILT_IN_ATOMIC_ADD_FETCH_N);
	  break;
	case MINUS_EXPR:
	  fncode = (return_old_p
		    ? BUILT_IN_ATOMIC_FETCH_SUB_N
		    : BUILT_IN_ATOMIC_SUB_FETCH_N);
	  break;
	case BIT_AND_EXPR:
	  fncode = (return_old_p
		    ? BUILT_IN_ATOMIC_FETCH_AND_N
		    : BUILT_IN_ATOMIC_AND_FETCH_N);
	  break;
	case BIT_IOR_EXPR:
	  fncode = (return_old_p
		    ? BUILT_IN_ATOMIC_FETCH_OR_N
		    : BUILT_IN_ATOMIC_OR_FETCH_N);
	  break;
	case BIT_XOR_EXPR:
	  fncode = (return_old_p
		    ? BUILT_IN_ATOMIC_FETCH_XOR_N
		    : BUILT_IN_ATOMIC_XOR_FETCH_N);
	  break;
	default:
	  goto cas_loop;
	}

      /* We can only use "_1" through "_16" variants of the atomic fetch
	 built-ins.  */
      unsigned HOST_WIDE_INT size = tree_to_uhwi (TYPE_SIZE_UNIT (lhs_type));
      if (size != 1 && size != 2 && size != 4 && size != 8 && size != 16)
	goto cas_loop;

      /* If this is a pointer type, we need to multiply by the size of
	 the pointer target type.  */
      if (POINTER_TYPE_P (lhs_type))
	{
	  if (!COMPLETE_TYPE_P (TREE_TYPE (lhs_type))
	      /* ??? This would introduce -Wdiscarded-qualifiers
		 warning: __atomic_fetch_* expect volatile void *
		 type as the first argument.  (Assignments between
		 atomic and non-atomic objects are OK.) */
	      || TYPE_RESTRICT (lhs_type))
	    goto cas_loop;
	  tree sz = TYPE_SIZE_UNIT (TREE_TYPE (lhs_type));
	  rhs = fold_build2_loc (loc, MULT_EXPR, ptrdiff_type_node,
				 aet_convert (ptrdiff_type_node, rhs),
				 aet_convert (ptrdiff_type_node, sz));
	}

      /* Build __atomic_fetch_* (&lhs, &val, SEQ_CST), or
	 __atomic_*_fetch (&lhs, &val, SEQ_CST).  */
      fndecl = builtin_decl_explicit (fncode);
      params->quick_push (lhs_addr);
      params->quick_push (rhs);
      params->quick_push (seq_cst);
      func_call = c_build_function_call_vec (loc, vNULL, fndecl, params, NULL);

      newval = create_tmp_var_raw (nonatomic_lhs_type);
      TREE_ADDRESSABLE (newval) = 1;
      TREE_NO_WARNING (newval) = 1;
      rhs = build4 (TARGET_EXPR, nonatomic_lhs_type, newval, func_call,
		    NULL_TREE, NULL_TREE);
      SET_EXPR_LOCATION (rhs, loc);
      add_stmt (rhs);

      /* Finish the compound statement.  */
      compound_stmt = aet_c_end_compound_stmt (loc, compound_stmt, false);

      /* NEWVAL is the value which was stored, return a COMPOUND_STMT of
	 the statement and that value.  */
      return build2 (COMPOUND_EXPR, nonatomic_lhs_type, compound_stmt, newval);
    }

cas_loop:
  /* Create the variables and labels required for the op= form.  */
  old = create_tmp_var_raw (nonatomic_lhs_type);
  old_addr = aet_build_unary_op (loc, ADDR_EXPR, old, false);
  TREE_ADDRESSABLE (old) = 1;
  TREE_NO_WARNING (old) = 1;

  newval = create_tmp_var_raw (nonatomic_lhs_type);
  newval_addr = aet_build_unary_op (loc, ADDR_EXPR, newval, false);
  TREE_ADDRESSABLE (newval) = 1;
  TREE_NO_WARNING (newval) = 1;

  loop_decl = create_artificial_label (loc);
  loop_label = build1 (LABEL_EXPR, void_type_node, loop_decl);

  done_decl = create_artificial_label (loc);
  done_label = build1 (LABEL_EXPR, void_type_node, done_decl);

  /* __atomic_load (addr, &old, SEQ_CST).  */
  fndecl = builtin_decl_explicit (BUILT_IN_ATOMIC_LOAD);
  params->quick_push (lhs_addr);
  params->quick_push (old_addr);
  params->quick_push (seq_cst);
  func_call = c_build_function_call_vec (loc, vNULL, fndecl, params, NULL);
  old = build4 (TARGET_EXPR, nonatomic_lhs_type, old, func_call, NULL_TREE,
		NULL_TREE);
  add_stmt (old);
  params->truncate (0);

  /* Create the expressions for floating-point environment
     manipulation, if required.  */
  bool need_fenv = (flag_trapping_math
		    && (FLOAT_TYPE_P (lhs_type) || FLOAT_TYPE_P (rhs_type)));
  tree hold_call = NULL_TREE, clear_call = NULL_TREE, update_call = NULL_TREE;
  if (need_fenv)
    targetm.atomic_assign_expand_fenv (&hold_call, &clear_call, &update_call);

  if (hold_call)
    add_stmt (hold_call);

  /* loop:  */
  add_stmt (loop_label);

  /* newval = old + val;  */
  if (rhs_type != rhs_semantic_type)
    val = build1 (EXCESS_PRECISION_EXPR, nonatomic_rhs_semantic_type, val);
  rhs = aet_build_binary_op (loc, modifycode, old, val, true);
  if (TREE_CODE (rhs) == EXCESS_PRECISION_EXPR)
    {
      tree eptype = TREE_TYPE (rhs);
      rhs = c_fully_fold (TREE_OPERAND (rhs, 0), false, NULL);
      rhs = build1 (EXCESS_PRECISION_EXPR, eptype, rhs);
    }
  else
    rhs = c_fully_fold (rhs, false, NULL);
  rhs = convert_for_assignment (loc, UNKNOWN_LOCATION, nonatomic_lhs_type,
				rhs, NULL_TREE, ic_assign, false, NULL_TREE,
				NULL_TREE, 0);
  if (rhs != error_mark_node)
    {
      rhs = build4 (TARGET_EXPR, nonatomic_lhs_type, newval, rhs, NULL_TREE,
		    NULL_TREE);
      SET_EXPR_LOCATION (rhs, loc);
      add_stmt (rhs);
    }

  /* if (__atomic_compare_exchange (addr, &old, &new, false, SEQ_CST, SEQ_CST))
       goto done;  */
  fndecl = builtin_decl_explicit (BUILT_IN_ATOMIC_COMPARE_EXCHANGE);
  params->quick_push (lhs_addr);
  params->quick_push (old_addr);
  params->quick_push (newval_addr);
  params->quick_push (integer_zero_node);
  params->quick_push (seq_cst);
  params->quick_push (seq_cst);
  func_call = c_build_function_call_vec (loc, vNULL, fndecl, params, NULL);

  goto_stmt = build1 (GOTO_EXPR, void_type_node, done_decl);
  SET_EXPR_LOCATION (goto_stmt, loc);

  stmt = build3 (COND_EXPR, void_type_node, func_call, goto_stmt, NULL_TREE);
  SET_EXPR_LOCATION (stmt, loc);
  add_stmt (stmt);

  if (clear_call)
    add_stmt (clear_call);

  /* goto loop;  */
  goto_stmt  = build1 (GOTO_EXPR, void_type_node, loop_decl);
  SET_EXPR_LOCATION (goto_stmt, loc);
  add_stmt (goto_stmt);

  /* done:  */
  add_stmt (done_label);

  if (update_call)
    add_stmt (update_call);

  /* Finish the compound statement.  */
  compound_stmt = aet_c_end_compound_stmt (loc, compound_stmt, false);

  /* NEWVAL is the value that was successfully stored, return a
     COMPOUND_EXPR of the statement and the appropriate value.  */
  return build2 (COMPOUND_EXPR, nonatomic_lhs_type, compound_stmt,
		 return_old_p ? old : newval);
}

/* Construct and perhaps optimize a tree representation
   for a unary operation.  CODE, a tree_code, specifies the operation
   and XARG is the operand.
   For any CODE other than ADDR_EXPR, NOCONVERT suppresses the default
   promotions (such as from short to int).
   For ADDR_EXPR, the default promotions are not applied; NOCONVERT allows
   non-lvalues; this is only used to handle conversion of non-lvalue arrays
   to pointers in C99.

   LOCATION is the location of the operator.  */

tree aet_build_unary_op (location_t location, enum tree_code code, tree xarg,bool noconvert)
{
  /* No aet_default_conversion here.  It causes trouble for ADDR_EXPR.  */
  tree arg = xarg;
  tree argtype = NULL_TREE;
  enum tree_code typecode;
  tree val;
  tree ret = error_mark_node;
  tree eptype = NULL_TREE;
  const char *invalid_op_diag;
  bool int_operands;

  int_operands = EXPR_INT_CONST_OPERANDS (xarg);
  if (int_operands)
    arg = remove_c_maybe_const_expr (arg);

  if (code != ADDR_EXPR)
    arg = aet_require_complete_type (location, arg);

  typecode = TREE_CODE (TREE_TYPE (arg));
  if (typecode == ERROR_MARK)
    return error_mark_node;
  if (typecode == ENUMERAL_TYPE || typecode == BOOLEAN_TYPE)
    typecode = INTEGER_TYPE;

  if ((invalid_op_diag
       = targetm.invalid_unary_op (code, TREE_TYPE (xarg))))
    {
      error_at (location, invalid_op_diag);
      return error_mark_node;
    }

  if (TREE_CODE (arg) == EXCESS_PRECISION_EXPR)
    {
      eptype = TREE_TYPE (arg);
      arg = TREE_OPERAND (arg, 0);
    }

  switch (code)
    {
    case CONVERT_EXPR:
      /* This is used for unary plus, because a CONVERT_EXPR
	 is enough to prevent anybody from looking inside for
	 associativity, but won't generate any code.  */
      if (!(typecode == INTEGER_TYPE || typecode == REAL_TYPE
	    || typecode == FIXED_POINT_TYPE || typecode == COMPLEX_TYPE
	    || gnu_vector_type_p (TREE_TYPE (arg))))
	{
	  error_at (location, "wrong type argument to unary plus");
	  return error_mark_node;
	}
      else if (!noconvert)
	arg = aet_default_conversion (arg);
      arg = non_lvalue_loc (location, arg);
      break;

    case NEGATE_EXPR:
      if (!(typecode == INTEGER_TYPE || typecode == REAL_TYPE
	    || typecode == FIXED_POINT_TYPE || typecode == COMPLEX_TYPE
	    || gnu_vector_type_p (TREE_TYPE (arg))))
	{
	  error_at (location, "wrong type argument to unary minus");
	  return error_mark_node;
	}
      else if (!noconvert)
	arg = aet_default_conversion (arg);
      break;

    case BIT_NOT_EXPR:
      /* ~ works on integer types and non float vectors. */
      if (typecode == INTEGER_TYPE
	  || (gnu_vector_type_p (TREE_TYPE (arg))
	      && !VECTOR_FLOAT_TYPE_P (TREE_TYPE (arg))))
	{
	  tree e = arg;

	  /* Warn if the expression has boolean value.  */
	  while (TREE_CODE (e) == COMPOUND_EXPR)
	    e = TREE_OPERAND (e, 1);

	  if ((TREE_CODE (TREE_TYPE (arg)) == BOOLEAN_TYPE
	       || truth_value_p (TREE_CODE (e))))
	    {
	      auto_diagnostic_group d;
	      if (warning_at (location, OPT_Wbool_operation,
				"%<~%> on a boolean expression"))
		{
		  gcc_rich_location richloc (location);
		  richloc.add_fixit_insert_before (location, "!");
		  inform (&richloc, "did you mean to use logical not?");
		}
	    }
	  if (!noconvert)
	    arg = aet_default_conversion (arg);
	}
      else if (typecode == COMPLEX_TYPE)
	{
	  code = CONJ_EXPR;
	  pedwarn (location, OPT_Wpedantic,
		   "ISO C does not support %<~%> for complex conjugation");
	  if (!noconvert)
	    arg = aet_default_conversion (arg);
	}
      else
	{
	  error_at (location, "wrong type argument to bit-complement");
	  return error_mark_node;
	}
      break;

    case ABS_EXPR:
      if (!(typecode == INTEGER_TYPE || typecode == REAL_TYPE))
	{
	  error_at (location, "wrong type argument to abs");
	  return error_mark_node;
	}
      else if (!noconvert)
	arg = aet_default_conversion (arg);
      break;

    case ABSU_EXPR:
      if (!(typecode == INTEGER_TYPE))
	{
	  error_at (location, "wrong type argument to absu");
	  return error_mark_node;
	}
      else if (!noconvert)
	arg = aet_default_conversion (arg);
      break;

    case CONJ_EXPR:
      /* Conjugating a real value is a no-op, but allow it anyway.  */
      if (!(typecode == INTEGER_TYPE || typecode == REAL_TYPE
	    || typecode == COMPLEX_TYPE))
	{
	  error_at (location, "wrong type argument to conjugation");
	  return error_mark_node;
	}
      else if (!noconvert)
	arg = aet_default_conversion (arg);
      break;

    case TRUTH_NOT_EXPR:
      if (typecode != INTEGER_TYPE && typecode != FIXED_POINT_TYPE
	  && typecode != REAL_TYPE && typecode != POINTER_TYPE
	  && typecode != COMPLEX_TYPE)
	{
	  error_at (location,
		    "wrong type argument to unary exclamation mark");
	  return error_mark_node;
	}
      if (int_operands)
	{
	  arg = aet_c_objc_common_truthvalue_conversion (location, xarg);
	  arg = remove_c_maybe_const_expr (arg);
	}
      else
	arg = aet_c_objc_common_truthvalue_conversion (location, arg);
      ret = invert_truthvalue_loc (location, arg);
      /* If the TRUTH_NOT_EXPR has been folded, reset the location.  */
      if (EXPR_P (ret) && EXPR_HAS_LOCATION (ret))
	location = EXPR_LOCATION (ret);
      goto return_build_unary_op;

    case REALPART_EXPR:
    case IMAGPART_EXPR:
      ret = build_real_imag_expr (location, code, arg);
      if (ret == error_mark_node)
	return error_mark_node;
      if (eptype && TREE_CODE (eptype) == COMPLEX_TYPE)
	eptype = TREE_TYPE (eptype);
      goto return_build_unary_op;

    case PREINCREMENT_EXPR:
    case POSTINCREMENT_EXPR:
    case PREDECREMENT_EXPR:
    case POSTDECREMENT_EXPR:

      if (TREE_CODE (arg) == C_MAYBE_CONST_EXPR)
	{
	  tree inner = aet_build_unary_op (location, code,
				       C_MAYBE_CONST_EXPR_EXPR (arg),
				       noconvert);
	  if (inner == error_mark_node)
	    return error_mark_node;
	  ret = build2 (C_MAYBE_CONST_EXPR, TREE_TYPE (inner),
			C_MAYBE_CONST_EXPR_PRE (arg), inner);
	  gcc_assert (!C_MAYBE_CONST_EXPR_INT_OPERANDS (arg));
	  C_MAYBE_CONST_EXPR_NON_CONST (ret) = 1;
	  goto return_build_unary_op;
	}

      /* Complain about anything that is not a true lvalue.  In
	 Objective-C, skip this check for property_refs.  */
      if (!objc_is_property_ref (arg)
	  && !lvalue_or_else (location,
			      arg, ((code == PREINCREMENT_EXPR
				     || code == POSTINCREMENT_EXPR)
				    ? lv_increment
				    : lv_decrement)))
	return error_mark_node;

      if (warn_cxx_compat && TREE_CODE (TREE_TYPE (arg)) == ENUMERAL_TYPE)
	{
	  if (code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR)
	    warning_at (location, OPT_Wc___compat,
			"increment of enumeration value is invalid in C++");
	  else
	    warning_at (location, OPT_Wc___compat,
			"decrement of enumeration value is invalid in C++");
	}

      if (TREE_CODE (TREE_TYPE (arg)) == BOOLEAN_TYPE)
	{
	  if (code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR)
	    warning_at (location, OPT_Wbool_operation,
			"increment of a boolean expression");
	  else
	    warning_at (location, OPT_Wbool_operation,
			"decrement of a boolean expression");
	}

      /* Ensure the argument is fully folded inside any SAVE_EXPR.  */
      arg = c_fully_fold (arg, false, NULL, true);

      bool atomic_op;
      atomic_op = really_atomic_lvalue (arg);

      /* Increment or decrement the real part of the value,
	 and don't change the imaginary part.  */
      if (typecode == COMPLEX_TYPE)
	{
	  tree real, imag;

	  pedwarn (location, OPT_Wpedantic,
		   "ISO C does not support %<++%> and %<--%> on complex types");

	  if (!atomic_op)
	    {
	      arg = stabilize_reference (arg);
	      real = aet_build_unary_op (EXPR_LOCATION (arg), REALPART_EXPR, arg,
				     true);
	      imag = aet_build_unary_op (EXPR_LOCATION (arg), IMAGPART_EXPR, arg,
				     true);
	      real = aet_build_unary_op (EXPR_LOCATION (arg), code, real, true);
	      if (real == error_mark_node || imag == error_mark_node)
		return error_mark_node;
	      ret = build2 (COMPLEX_EXPR, TREE_TYPE (arg),
			    real, imag);
	      goto return_build_unary_op;
	    }
	}

      /* Report invalid types.  */

      if (typecode != POINTER_TYPE && typecode != FIXED_POINT_TYPE
	  && typecode != INTEGER_TYPE && typecode != REAL_TYPE
	  && typecode != COMPLEX_TYPE
	  && !gnu_vector_type_p (TREE_TYPE (arg)))
	{
	  if (code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR)
	    error_at (location, "wrong type argument to increment");
	  else
	    error_at (location, "wrong type argument to decrement");

	  return error_mark_node;
	}

      {
	tree inc;

	argtype = TREE_TYPE (arg);

	/* Compute the increment.  */

	if (typecode == POINTER_TYPE)
	  {
	    /* If pointer target is an incomplete type,
	       we just cannot know how to do the arithmetic.  */
	    if (!COMPLETE_OR_VOID_TYPE_P (TREE_TYPE (argtype)))
	      {
		if (code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR)
		  error_at (location,
			    "increment of pointer to an incomplete type %qT",
			    TREE_TYPE (argtype));
		else
		  error_at (location,
			    "decrement of pointer to an incomplete type %qT",
			    TREE_TYPE (argtype));
	      }
	    else if (TREE_CODE (TREE_TYPE (argtype)) == FUNCTION_TYPE
		     || TREE_CODE (TREE_TYPE (argtype)) == VOID_TYPE)
	      {
		if (code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR)
		  pedwarn (location, OPT_Wpointer_arith,
			   "wrong type argument to increment");
		else
		  pedwarn (location, OPT_Wpointer_arith,
			   "wrong type argument to decrement");
	      }
	    else
	      verify_type_context (location, TCTX_POINTER_ARITH,
				   TREE_TYPE (argtype));

	    inc = c_size_in_bytes (TREE_TYPE (argtype));
	    inc = convert_to_ptrofftype_loc (location, inc);
	  }
	else if (FRACT_MODE_P (TYPE_MODE (argtype)))
	  {
	    /* For signed fract types, we invert ++ to -- or
	       -- to ++, and change inc from 1 to -1, because
	       it is not possible to represent 1 in signed fract constants.
	       For unsigned fract types, the result always overflows and
	       we get an undefined (original) or the maximum value.  */
	    if (code == PREINCREMENT_EXPR)
	      code = PREDECREMENT_EXPR;
	    else if (code == PREDECREMENT_EXPR)
	      code = PREINCREMENT_EXPR;
	    else if (code == POSTINCREMENT_EXPR)
	      code = POSTDECREMENT_EXPR;
	    else /* code == POSTDECREMENT_EXPR  */
	      code = POSTINCREMENT_EXPR;

	    inc = integer_minus_one_node;
	    inc = aet_convert (argtype, inc);
	  }
	else
	  {
	    inc = VECTOR_TYPE_P (argtype)
	      ? build_one_cst (argtype)
	      : integer_one_node;
	    inc = aet_convert (argtype, inc);
	  }

	/* Report a read-only lvalue.  */
	if (TYPE_READONLY (argtype))
	  {
	    readonly_error (location, arg,
			    ((code == PREINCREMENT_EXPR
			      || code == POSTINCREMENT_EXPR)
			     ? lv_increment : lv_decrement));
	    return error_mark_node;
	  }
	else if (TREE_READONLY (arg))
	  readonly_warning (arg,
			    ((code == PREINCREMENT_EXPR
			      || code == POSTINCREMENT_EXPR)
			     ? lv_increment : lv_decrement));

	/* If the argument is atomic, use the special code sequences for
	   atomic compound assignment.  */
	if (atomic_op)
	  {
	    arg = stabilize_reference (arg);
	    ret = build_atomic_assign (location, arg,
				       ((code == PREINCREMENT_EXPR
					 || code == POSTINCREMENT_EXPR)
					? PLUS_EXPR
					: MINUS_EXPR),
				       (FRACT_MODE_P (TYPE_MODE (argtype))
					? inc
					: integer_one_node),
				       (code == POSTINCREMENT_EXPR
					|| code == POSTDECREMENT_EXPR));
	    goto return_build_unary_op;
	  }

	if (TREE_CODE (TREE_TYPE (arg)) == BOOLEAN_TYPE)
	  val = boolean_increment (code, arg);
	else
	  val = build2 (code, TREE_TYPE (arg), arg, inc);
	TREE_SIDE_EFFECTS (val) = 1;
	if (TREE_CODE (val) != code)
	  TREE_NO_WARNING (val) = 1;
	ret = val;
	goto return_build_unary_op;
      }

    case ADDR_EXPR:
      /* Note that this operation never does aet_default_conversion.  */

      /* The operand of unary '&' must be an lvalue (which excludes
	 expressions of type void), or, in C99, the result of a [] or
	 unary '*' operator.  */
      if (VOID_TYPE_P (TREE_TYPE (arg))
	  && TYPE_QUALS (TREE_TYPE (arg)) == TYPE_UNQUALIFIED
	  && (!INDIRECT_REF_P (arg) || !flag_isoc99))
	pedwarn (location, 0, "taking address of expression of type %<void%>");

      /* Let &* cancel out to simplify resulting code.  */
      if (INDIRECT_REF_P (arg))
	{
	  /* Don't let this be an lvalue.  */
	  if (aet_lvalue_p (TREE_OPERAND (arg, 0)))
	    return non_lvalue_loc (location, TREE_OPERAND (arg, 0));
	  ret = TREE_OPERAND (arg, 0);
	  goto return_build_unary_op;
	}

      /* Anything not already handled and not a true memory reference
	 or a non-lvalue array is an error.  */
      if (typecode != FUNCTION_TYPE && !noconvert
	  && !lvalue_or_else (location, arg, lv_addressof))
	return error_mark_node;

      /* Move address operations inside C_MAYBE_CONST_EXPR to simplify
	 folding later.  */
      if (TREE_CODE (arg) == C_MAYBE_CONST_EXPR)
	{
	  tree inner = aet_build_unary_op (location, code,
				       C_MAYBE_CONST_EXPR_EXPR (arg),
				       noconvert);
	  ret = build2 (C_MAYBE_CONST_EXPR, TREE_TYPE (inner),
			C_MAYBE_CONST_EXPR_PRE (arg), inner);
	  gcc_assert (!C_MAYBE_CONST_EXPR_INT_OPERANDS (arg));
	  C_MAYBE_CONST_EXPR_NON_CONST (ret)
	    = C_MAYBE_CONST_EXPR_NON_CONST (arg);
	  goto return_build_unary_op;
	}

      /* Ordinary case; arg is a COMPONENT_REF or a decl.  */
      argtype = TREE_TYPE (arg);

      /* If the lvalue is const or volatile, merge that into the type
	 to which the address will point.  This is only needed
	 for function types.  */
      if ((DECL_P (arg) || REFERENCE_CLASS_P (arg))
	  && (TREE_READONLY (arg) || TREE_THIS_VOLATILE (arg))
	  && TREE_CODE (argtype) == FUNCTION_TYPE)
	{
	  int orig_quals = TYPE_QUALS (strip_array_types (argtype));
	  int quals = orig_quals;

	  if (TREE_READONLY (arg))
	    quals |= TYPE_QUAL_CONST;
	  if (TREE_THIS_VOLATILE (arg))
	    quals |= TYPE_QUAL_VOLATILE;

	  argtype = aet_c_build_qualified_type (argtype, quals);
	}

      switch (TREE_CODE (arg))
	{
	case COMPONENT_REF:
	  if (DECL_C_BIT_FIELD (TREE_OPERAND (arg, 1)))
	    {
	      error_at (location, "cannot take address of bit-field %qD",
			TREE_OPERAND (arg, 1));
	      return error_mark_node;
	    }

	  /* fall through */

	case ARRAY_REF:
	  if (TYPE_REVERSE_STORAGE_ORDER (TREE_TYPE (TREE_OPERAND (arg, 0))))
	    {
	      if (!AGGREGATE_TYPE_P (TREE_TYPE (arg))
		  && !VECTOR_TYPE_P (TREE_TYPE (arg)))
		{
		  error_at (location, "cannot take address of scalar with "
			    "reverse storage order");
		  return error_mark_node;
		}

	      if (TREE_CODE (TREE_TYPE (arg)) == ARRAY_TYPE
		  && TYPE_REVERSE_STORAGE_ORDER (TREE_TYPE (arg)))
		warning_at (location, OPT_Wscalar_storage_order,
			    "address of array with reverse scalar storage "
			    "order requested");
	    }

	default:
	  break;
	}

      if (!aet_c_mark_addressable (arg))
	return error_mark_node;

      gcc_assert (TREE_CODE (arg) != COMPONENT_REF
		  || !DECL_C_BIT_FIELD (TREE_OPERAND (arg, 1)));

      argtype = build_pointer_type (argtype);

      /* ??? Cope with user tricks that amount to offsetof.  Delete this
	 when we have proper support for integer constant expressions.  */
      val = get_base_address (arg);
      if (val && INDIRECT_REF_P (val)
          && TREE_CONSTANT (TREE_OPERAND (val, 0)))
	{
	  ret = fold_offsetof (arg, argtype);
	  goto return_build_unary_op;
	}

      val = build1 (ADDR_EXPR, argtype, arg);

      ret = val;
      goto return_build_unary_op;

    default:
      gcc_unreachable ();
    }

  if (argtype == NULL_TREE)
    argtype = TREE_TYPE (arg);
  if (TREE_CODE (arg) == INTEGER_CST)
    ret = (require_constant_value
	   ? fold_build1_initializer_loc (location, code, argtype, arg)
	   : fold_build1_loc (location, code, argtype, arg));
  else
    ret = build1 (code, argtype, arg);
 return_build_unary_op:
  gcc_assert (ret != error_mark_node);
  if (TREE_CODE (ret) == INTEGER_CST && !TREE_OVERFLOW (ret)
      && !(TREE_CODE (xarg) == INTEGER_CST && !TREE_OVERFLOW (xarg)))
    ret = build1 (NOP_EXPR, TREE_TYPE (ret), ret);
  else if (TREE_CODE (ret) != INTEGER_CST && int_operands)
    ret = note_integer_operands (ret);
  if (eptype)
    ret = build1 (EXCESS_PRECISION_EXPR, eptype, ret);
  protected_set_expr_location (ret, location);
  return ret;
}

/* Return nonzero if REF is an lvalue valid for this language.
   Lvalues can be assigned, unless their type has TYPE_READONLY.
   Lvalues can have their address taken, unless they have C_DECL_REGISTER.  */

bool aet_lvalue_p (const_tree ref)
{
  const enum tree_code code = TREE_CODE (ref);

  switch (code)
    {
    case REALPART_EXPR:
    case IMAGPART_EXPR:
    case COMPONENT_REF:
      return aet_lvalue_p (TREE_OPERAND (ref, 0));

    case C_MAYBE_CONST_EXPR:
      return aet_lvalue_p (TREE_OPERAND (ref, 1));

    case COMPOUND_LITERAL_EXPR:
    case STRING_CST:
      return true;

    case INDIRECT_REF:
    case ARRAY_REF:
    case VAR_DECL:
    case PARM_DECL:
    case RESULT_DECL:
    case ERROR_MARK:
      return (TREE_CODE (TREE_TYPE (ref)) != FUNCTION_TYPE
	      && TREE_CODE (TREE_TYPE (ref)) != METHOD_TYPE);

    case BIND_EXPR:
      return TREE_CODE (TREE_TYPE (ref)) == ARRAY_TYPE;

    default:
      return false;
    }
}

/* Give a warning for storing in something that is read-only in GCC
   terms but not const in ISO C terms.  */

static void readonly_warning (tree arg, enum lvalue_use use)
{
  switch (use)
    {
    case lv_assign:
      warning (0, "assignment of read-only location %qE", arg);
      break;
    case lv_increment:
      warning (0, "increment of read-only location %qE", arg);
      break;
    case lv_decrement:
      warning (0, "decrement of read-only location %qE", arg);
      break;
    default:
      gcc_unreachable ();
    }
  return;
}


/* Return nonzero if REF is an lvalue valid for this language;
   otherwise, print an error message and return zero.  USE says
   how the lvalue is being used and so selects the error message.
   LOCATION is the location at which any error should be reported.  */

static int lvalue_or_else (location_t loc, const_tree ref, enum lvalue_use use)
{
  int win = aet_lvalue_p (ref);

  if (!win)
    lvalue_error (loc, use);

  return win;
}

/* Mark EXP saying that we need to be able to take the
   address of it; it should not be allocated in a register.
   Returns true if successful.  ARRAY_REF_P is true if this
   is for ARRAY_REF construction - in that case we don't want
   to look through VIEW_CONVERT_EXPR from VECTOR_TYPE to ARRAY_TYPE,
   it is fine to use ARRAY_REFs for vector subscripts on vector
   register variables.  */

bool aet_c_mark_addressable (tree exp, bool array_ref_p)
{
  tree x = exp;

  while (1)
    switch (TREE_CODE (x))
      {
      case VIEW_CONVERT_EXPR:
	if (array_ref_p
	    && TREE_CODE (TREE_TYPE (x)) == ARRAY_TYPE
	    && VECTOR_TYPE_P (TREE_TYPE (TREE_OPERAND (x, 0))))
	  return true;
	/* FALLTHRU */
      case COMPONENT_REF:
      case ADDR_EXPR:
      case ARRAY_REF:
      case REALPART_EXPR:
      case IMAGPART_EXPR:
	x = TREE_OPERAND (x, 0);
	break;

      case COMPOUND_LITERAL_EXPR:
	TREE_ADDRESSABLE (x) = 1;
	TREE_ADDRESSABLE (COMPOUND_LITERAL_EXPR_DECL (x)) = 1;
	return true;

      case CONSTRUCTOR:
	TREE_ADDRESSABLE (x) = 1;
	return true;

      case VAR_DECL:
      case CONST_DECL:
      case PARM_DECL:
      case RESULT_DECL:
	if (C_DECL_REGISTER (x) && DECL_NONLOCAL (x)) {
	    if (TREE_PUBLIC (x) || is_global_var (x)){
		    //zclei error ("global register variable %qD used in nested function", x);
		  	argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=global_register_variable_xx_used_in_nested_function;
		    return false;
	    }
	    //zclei pedwarn (input_location, 0, "register variable %qD used in nested function", x);
	  	argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=register_variable_xx_used_in_nested_function;

    }else if (C_DECL_REGISTER (x)){
	    if (TREE_PUBLIC (x) || is_global_var (x))
	      //zclei error ("address of global register variable %qD requested", x);
	  	argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=address_of_global_register_variable_xx_requested;

	    else
	      //zclei error ("address of register variable %qD requested", x);
	  	 argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=address_of_register_variable_xx_requested;

	    return false;
    }

	/* FALLTHRU */
      case FUNCTION_DECL:
	TREE_ADDRESSABLE (x) = 1;
	/* FALLTHRU */
      default:
	return true;
    }
}

/* Convert EXPR to TYPE, warning about conversion problems with
   constants.  SEMANTIC_TYPE is the type this conversion would use
   without excess precision. If SEMANTIC_TYPE is NULL, this function
   is equivalent to convert_and_check. This function is a wrapper that
   handles conversions that may be different than
   the usual ones because of excess precision.  */

static tree ep_convert_and_check (location_t loc, tree type, tree expr,
		      tree semantic_type)
{
  if (TREE_TYPE (expr) == type)
    return expr;

  /* For C11, integer conversions may have results with excess
     precision.  */
  if (flag_isoc11 || !semantic_type)
    return convert_and_check (loc, type, expr);

  if (TREE_CODE (TREE_TYPE (expr)) == INTEGER_TYPE
      && TREE_TYPE (expr) != semantic_type)
    {
      /* For integers, we need to check the real conversion, not
	 the conversion to the excess precision type.  */
      expr = convert_and_check (loc, semantic_type, expr);
    }
  /* Result type is the excess precision type, which should be
     large enough, so do not check.  */
  return aet_convert (type, expr);
}

/* If EXPR refers to a built-in declared without a prototype returns
   the actual type of the built-in and, if non-null, set *BLTIN to
   a pointer to the built-in.  Otherwise return the type of EXPR
   and clear *BLTIN if non-null.  */

static tree type_or_builtin_type (tree expr, tree *bltin = NULL)
{
  tree dummy;
  if (!bltin)
    bltin = &dummy;

  *bltin = NULL_TREE;

  tree type = TREE_TYPE (expr);
  if (TREE_CODE (expr) != ADDR_EXPR)
    return type;

  tree oper = TREE_OPERAND (expr, 0);
  if (!DECL_P (oper)
      || TREE_CODE (oper) != FUNCTION_DECL
      || !fndecl_built_in_p (oper, BUILT_IN_NORMAL))
    return type;

  built_in_function code = DECL_FUNCTION_CODE (oper);
  if (!C_DECL_BUILTIN_PROTOTYPE (oper))
    return type;

  if ((*bltin = builtin_decl_implicit (code)))
    type = build_pointer_type (TREE_TYPE (*bltin));

  return type;
}



/* Issue -Wcast-qual warnings when appropriate.  TYPE is the type to
   which we are casting.  OTYPE is the type of the expression being
   cast.  Both TYPE and OTYPE are pointer types.  LOC is the location
   of the cast.  -Wcast-qual appeared on the command line.  Named
   address space qualifiers are not handled here, because they result
   in different warnings.  */

static void handle_warn_cast_qual (location_t loc, tree type, tree otype)
{
  tree in_type = type;
  tree in_otype = otype;
  int added = 0;
  int discarded = 0;
  bool is_const;

  /* Check that the qualifiers on IN_TYPE are a superset of the
     qualifiers of IN_OTYPE.  The outermost level of POINTER_TYPE
     nodes is uninteresting and we stop as soon as we hit a
     non-POINTER_TYPE node on either type.  */
  do
    {
      in_otype = TREE_TYPE (in_otype);
      in_type = TREE_TYPE (in_type);

      /* GNU C allows cv-qualified function types.  'const' means the
	 function is very pure, 'volatile' means it can't return.  We
	 need to warn when such qualifiers are added, not when they're
	 taken away.  */
      if (TREE_CODE (in_otype) == FUNCTION_TYPE
	  && TREE_CODE (in_type) == FUNCTION_TYPE)
	added |= (TYPE_QUALS_NO_ADDR_SPACE (in_type)
		  & ~TYPE_QUALS_NO_ADDR_SPACE (in_otype));
      else
	discarded |= (TYPE_QUALS_NO_ADDR_SPACE (in_otype)
		      & ~TYPE_QUALS_NO_ADDR_SPACE (in_type));
    }
  while (TREE_CODE (in_type) == POINTER_TYPE
	 && TREE_CODE (in_otype) == POINTER_TYPE);

  if (added)
    warning_at (loc, OPT_Wcast_qual,
		"cast adds %q#v qualifier to function type", added);

  if (discarded)
    /* There are qualifiers present in IN_OTYPE that are not present
       in IN_TYPE.  */
    warning_at (loc, OPT_Wcast_qual,
		"cast discards %qv qualifier from pointer target type",
		discarded);

  if (added || discarded)
    return;

  /* A cast from **T to const **T is unsafe, because it can cause a
     const value to be changed with no additional warning.  We only
     issue this warning if T is the same on both sides, and we only
     issue the warning if there are the same number of pointers on
     both sides, as otherwise the cast is clearly unsafe anyhow.  A
     cast is unsafe when a qualifier is added at one level and const
     is not present at all outer levels.

     To issue this warning, we check at each level whether the cast
     adds new qualifiers not already seen.  We don't need to special
     case function types, as they won't have the same
     TYPE_MAIN_VARIANT.  */

  if (TYPE_MAIN_VARIANT (in_type) != TYPE_MAIN_VARIANT (in_otype))
    return;
  if (TREE_CODE (TREE_TYPE (type)) != POINTER_TYPE)
    return;

  in_type = type;
  in_otype = otype;
  is_const = TYPE_READONLY (TREE_TYPE (in_type));
  do
    {
      in_type = TREE_TYPE (in_type);
      in_otype = TREE_TYPE (in_otype);
      if ((TYPE_QUALS (in_type) &~ TYPE_QUALS (in_otype)) != 0
	  && !is_const)
	{
	  warning_at (loc, OPT_Wcast_qual,
		      "to be safe all intermediate pointers in cast from "
                      "%qT to %qT must be %<const%> qualified",
		      otype, type);
	  break;
	}
      if (is_const)
	is_const = TYPE_READONLY (in_type);
    }
  while (TREE_CODE (in_type) == POINTER_TYPE);
}

/* Heuristic check if two parameter types can be considered ABI-equivalent.  */

static bool c_safe_arg_type_equiv_p (tree t1, tree t2)
{
  t1 = TYPE_MAIN_VARIANT (t1);
  t2 = TYPE_MAIN_VARIANT (t2);

  if (TREE_CODE (t1) == POINTER_TYPE
      && TREE_CODE (t2) == POINTER_TYPE)
    return true;

  /* The signedness of the parameter matters only when an integral
     type smaller than int is promoted to int, otherwise only the
     precision of the parameter matters.
     This check should make sure that the callee does not see
     undefined values in argument registers.  */
  if (INTEGRAL_TYPE_P (t1)
      && INTEGRAL_TYPE_P (t2)
      && TYPE_PRECISION (t1) == TYPE_PRECISION (t2)
      && (TYPE_UNSIGNED (t1) == TYPE_UNSIGNED (t2)
	  || !targetm.calls.promote_prototypes (NULL_TREE)
	  || TYPE_PRECISION (t1) >= TYPE_PRECISION (integer_type_node)))
    return true;

  return aet_comptypes (t1, t2);
}

/* Check if a type cast between two function types can be considered safe.  */

static bool c_safe_function_type_cast_p (tree t1, tree t2)
{
  if (TREE_TYPE (t1) == void_type_node &&
      TYPE_ARG_TYPES (t1) == void_list_node)
    return true;

  if (TREE_TYPE (t2) == void_type_node &&
      TYPE_ARG_TYPES (t2) == void_list_node)
    return true;

  if (!c_safe_arg_type_equiv_p (TREE_TYPE (t1), TREE_TYPE (t2)))
    return false;

  for (t1 = TYPE_ARG_TYPES (t1), t2 = TYPE_ARG_TYPES (t2);
       t1 && t2;
       t1 = TREE_CHAIN (t1), t2 = TREE_CHAIN (t2))
    if (!c_safe_arg_type_equiv_p (TREE_VALUE (t1), TREE_VALUE (t2)))
      return false;

  return true;
}

/* Build an expression representing a cast to type TYPE of expression EXPR.
   LOC is the location of the cast-- typically the open paren of the cast.  */

tree aet_build_c_cast (location_t loc, tree type, tree expr)
{
  tree value;

  bool int_operands = EXPR_INT_CONST_OPERANDS (expr);

  if (TREE_CODE (expr) == EXCESS_PRECISION_EXPR)
    expr = TREE_OPERAND (expr, 0);

  value = expr;
  if (int_operands)
    value = remove_c_maybe_const_expr (value);

  if (type == error_mark_node || expr == error_mark_node)
    return error_mark_node;

  /* The ObjC front-end uses TYPE_MAIN_VARIANT to tie together types differing
     only in <protocol> qualifications.  But when constructing cast expressions,
     the protocols do matter and must be kept around.  */
  if (objc_is_object_ptr (type) && objc_is_object_ptr (TREE_TYPE (expr)))
    return build1 (NOP_EXPR, type, expr);

  type = TYPE_MAIN_VARIANT (type);

  if (TREE_CODE (type) == ARRAY_TYPE)
    {
      error_at (loc, "cast specifies array type");
      return error_mark_node;
    }

  if (TREE_CODE (type) == FUNCTION_TYPE)
    {
      error_at (loc, "cast specifies function type");
      return error_mark_node;
    }

  if (!VOID_TYPE_P (type))
    {
      value = aet_require_complete_type (loc, value);
      if (value == error_mark_node)
	return error_mark_node;
    }

  if (type == TYPE_MAIN_VARIANT (TREE_TYPE (value)))
    {
      if (RECORD_OR_UNION_TYPE_P (type))
	pedwarn (loc, OPT_Wpedantic,
		 "ISO C forbids casting nonscalar to the same type");

      /* Convert to remove any qualifiers from VALUE's type.  */
      value = aet_convert (type, value);
    }
  else if (TREE_CODE (type) == UNION_TYPE)
    {
      tree field;

      for (field = TYPE_FIELDS (type); field; field = DECL_CHAIN (field))
	if (TREE_TYPE (field) != error_mark_node
	    && aet_comptypes (TYPE_MAIN_VARIANT (TREE_TYPE (field)),
			  TYPE_MAIN_VARIANT (TREE_TYPE (value))))
	  break;

      if (field)
	{
	  tree t;
	  bool maybe_const = true;

	  pedwarn (loc, OPT_Wpedantic, "ISO C forbids casts to union type");
	  t = c_fully_fold (value, false, &maybe_const);
	  t = build_constructor_single (type, field, t);
	  if (!maybe_const)
	    t = c_wrap_maybe_const (t, true);
	  t = digest_init (loc, type, t,
			   NULL_TREE, false, true, 0);
	  TREE_CONSTANT (t) = TREE_CONSTANT (value);
	  return t;
	}
      error_at (loc, "cast to union type from type not present in union");
      return error_mark_node;
    }
  else
    {
      tree otype, ovalue;

      if (type == void_type_node)
	{
	  tree t = build1 (CONVERT_EXPR, type, value);
	  SET_EXPR_LOCATION (t, loc);
	  return t;
	}

      otype = TREE_TYPE (value);

      /* Optionally warn about potentially worrisome casts.  */
      if (warn_cast_qual
	  && TREE_CODE (type) == POINTER_TYPE
	  && TREE_CODE (otype) == POINTER_TYPE)
	handle_warn_cast_qual (loc, type, otype);

      /* Warn about conversions between pointers to disjoint
	 address spaces.  */
      if (TREE_CODE (type) == POINTER_TYPE
	  && TREE_CODE (otype) == POINTER_TYPE
	  && !null_pointer_constant_p (value))
	{
	  addr_space_t as_to = TYPE_ADDR_SPACE (TREE_TYPE (type));
	  addr_space_t as_from = TYPE_ADDR_SPACE (TREE_TYPE (otype));
	  addr_space_t as_common;

	  if (!addr_space_superset (as_to, as_from, &as_common))
	    {
	      if (ADDR_SPACE_GENERIC_P (as_from))
		warning_at (loc, 0, "cast to %s address space pointer "
			    "from disjoint generic address space pointer",
			    c_addr_space_name (as_to));

	      else if (ADDR_SPACE_GENERIC_P (as_to))
		warning_at (loc, 0, "cast to generic address space pointer "
			    "from disjoint %s address space pointer",
			    c_addr_space_name (as_from));

	      else
		warning_at (loc, 0, "cast to %s address space pointer "
			    "from disjoint %s address space pointer",
			    c_addr_space_name (as_to),
			    c_addr_space_name (as_from));
	    }
	}

      /* Warn about possible alignment problems.  */
      if ((STRICT_ALIGNMENT || warn_cast_align == 2)
	  && TREE_CODE (type) == POINTER_TYPE
	  && TREE_CODE (otype) == POINTER_TYPE
	  && TREE_CODE (TREE_TYPE (otype)) != VOID_TYPE
	  && TREE_CODE (TREE_TYPE (otype)) != FUNCTION_TYPE
	  /* Don't warn about opaque types, where the actual alignment
	     restriction is unknown.  */
	  && !(RECORD_OR_UNION_TYPE_P (TREE_TYPE (otype))
	       && TYPE_MODE (TREE_TYPE (otype)) == VOIDmode)
	  && min_align_of_type (TREE_TYPE (type))
	     > min_align_of_type (TREE_TYPE (otype)))
	warning_at (loc, OPT_Wcast_align,
		    "cast increases required alignment of target type");

      if (TREE_CODE (type) == INTEGER_TYPE
	  && TREE_CODE (otype) == POINTER_TYPE
	  && TYPE_PRECISION (type) != TYPE_PRECISION (otype))
      /* Unlike conversion of integers to pointers, where the
         warning is disabled for converting constants because
         of cases such as SIG_*, warn about converting constant
         pointers to integers. In some cases it may cause unwanted
         sign extension, and a warning is appropriate.  */
	warning_at (loc, OPT_Wpointer_to_int_cast,
		    "cast from pointer to integer of different size");

      if (TREE_CODE (value) == CALL_EXPR
	  && TREE_CODE (type) != TREE_CODE (otype))
	warning_at (loc, OPT_Wbad_function_cast,
		    "cast from function call of type %qT "
		    "to non-matching type %qT", otype, type);

      if (TREE_CODE (type) == POINTER_TYPE
	  && TREE_CODE (otype) == INTEGER_TYPE
	  && TYPE_PRECISION (type) != TYPE_PRECISION (otype)
	  /* Don't warn about converting any constant.  */
	  && !TREE_CONSTANT (value))
	warning_at (loc,
		    OPT_Wint_to_pointer_cast, "cast to pointer from integer "
		    "of different size");

      if (warn_strict_aliasing <= 2)
        strict_aliasing_warning (EXPR_LOCATION (value), type, expr);

      /* If pedantic, warn for conversions between function and object
	 pointer types, except for converting a null pointer constant
	 to function pointer type.  */
      if (pedantic
	  && TREE_CODE (type) == POINTER_TYPE
	  && TREE_CODE (otype) == POINTER_TYPE
	  && TREE_CODE (TREE_TYPE (otype)) == FUNCTION_TYPE
	  && TREE_CODE (TREE_TYPE (type)) != FUNCTION_TYPE)
	pedwarn (loc, OPT_Wpedantic, "ISO C forbids "
		 "conversion of function pointer to object pointer type");

      if (pedantic
	  && TREE_CODE (type) == POINTER_TYPE
	  && TREE_CODE (otype) == POINTER_TYPE
	  && TREE_CODE (TREE_TYPE (type)) == FUNCTION_TYPE
	  && TREE_CODE (TREE_TYPE (otype)) != FUNCTION_TYPE
	  && !null_pointer_constant_p (value))
	pedwarn (loc, OPT_Wpedantic, "ISO C forbids "
		 "conversion of object pointer to function pointer type");

      if (TREE_CODE (type) == POINTER_TYPE
	  && TREE_CODE (otype) == POINTER_TYPE
	  && TREE_CODE (TREE_TYPE (type)) == FUNCTION_TYPE
	  && TREE_CODE (TREE_TYPE (otype)) == FUNCTION_TYPE
	  && !c_safe_function_type_cast_p (TREE_TYPE (type),
					   TREE_TYPE (otype)))
	warning_at (loc, OPT_Wcast_function_type,
		    "cast between incompatible function types"
		    " from %qT to %qT", otype, type);

      ovalue = value;
      value = aet_convert (type, value);

      /* Ignore any integer overflow caused by the cast.  */
      if (TREE_CODE (value) == INTEGER_CST && !FLOAT_TYPE_P (otype))
	{
	  if (CONSTANT_CLASS_P (ovalue) && TREE_OVERFLOW (ovalue))
	    {
	      if (!TREE_OVERFLOW (value))
		{
		  /* Avoid clobbering a shared constant.  */
		  value = copy_node (value);
		  TREE_OVERFLOW (value) = TREE_OVERFLOW (ovalue);
		}
	    }
	  else if (TREE_OVERFLOW (value))
	    /* Reset VALUE's overflow flags, ensuring constant sharing.  */
	    value = wide_int_to_tree (TREE_TYPE (value), wi::to_wide (value));
	}
    }

  /* Don't let a cast be an lvalue.  */
  if (aet_lvalue_p (value))
    value = non_lvalue_loc (loc, value);

  /* Don't allow the results of casting to floating-point or complex
     types be confused with actual constants, or casts involving
     integer and pointer types other than direct integer-to-integer
     and integer-to-pointer be confused with integer constant
     expressions and null pointer constants.  */
  if (TREE_CODE (value) == REAL_CST
      || TREE_CODE (value) == COMPLEX_CST
      || (TREE_CODE (value) == INTEGER_CST
	  && !((TREE_CODE (expr) == INTEGER_CST
		&& INTEGRAL_TYPE_P (TREE_TYPE (expr)))
	       || TREE_CODE (expr) == REAL_CST
	       || TREE_CODE (expr) == COMPLEX_CST)))
      value = build1 (NOP_EXPR, type, value);

  /* If the expression has integer operands and so can occur in an
     unevaluated part of an integer constant expression, ensure the
     return value reflects this.  */
  if (int_operands
      && INTEGRAL_TYPE_P (type)
      && !EXPR_INT_CONST_OPERANDS (value))
    value = note_integer_operands (value);

  protected_set_expr_location (value, loc);
  return value;
}


/* Return whether STRUCT_TYPE has an anonymous field with type TYPE.
   This is used to implement -fplan9-extensions.  */

static bool find_anonymous_field_with_type (tree struct_type, tree type)
{
  tree field;
  bool found;

  gcc_assert (RECORD_OR_UNION_TYPE_P (struct_type));
  found = false;
  for (field = TYPE_FIELDS (struct_type);
       field != NULL_TREE;
       field = TREE_CHAIN (field))
    {
      tree fieldtype = (TYPE_ATOMIC (TREE_TYPE (field))
			? aet_c_build_qualified_type (TREE_TYPE (field),
						  TYPE_QUAL_ATOMIC)
			: TYPE_MAIN_VARIANT (TREE_TYPE (field)));
      if (DECL_NAME (field) == NULL
	  && aet_comptypes (type, fieldtype))
	{
	  if (found)
	    return false;
	  found = true;
	}
      else if (DECL_NAME (field) == NULL
	       && RECORD_OR_UNION_TYPE_P (TREE_TYPE (field))
	       && find_anonymous_field_with_type (TREE_TYPE (field), type))
	{
	  if (found)
	    return false;
	  found = true;
	}
    }
  return found;
}

/* RHS is an expression whose type is pointer to struct.  If there is
   an anonymous field in RHS with type TYPE, then return a pointer to
   that field in RHS.  This is used with -fplan9-extensions.  This
   returns NULL if no conversion could be found.  */

static tree convert_to_anonymous_field (location_t location, tree type, tree rhs)
{
  tree rhs_struct_type, lhs_main_type;
  tree field, found_field;
  bool found_sub_field;
  tree ret;

  gcc_assert (POINTER_TYPE_P (TREE_TYPE (rhs)));
  rhs_struct_type = TREE_TYPE (TREE_TYPE (rhs));
  gcc_assert (RECORD_OR_UNION_TYPE_P (rhs_struct_type));

  gcc_assert (POINTER_TYPE_P (type));
  lhs_main_type = (TYPE_ATOMIC (TREE_TYPE (type))
		   ? aet_c_build_qualified_type (TREE_TYPE (type),
					     TYPE_QUAL_ATOMIC)
		   : TYPE_MAIN_VARIANT (TREE_TYPE (type)));

  found_field = NULL_TREE;
  found_sub_field = false;
  for (field = TYPE_FIELDS (rhs_struct_type);
       field != NULL_TREE;
       field = TREE_CHAIN (field))
    {
      if (DECL_NAME (field) != NULL_TREE
	  || !RECORD_OR_UNION_TYPE_P (TREE_TYPE (field)))
	continue;
      tree fieldtype = (TYPE_ATOMIC (TREE_TYPE (field))
			? aet_c_build_qualified_type (TREE_TYPE (field),
						  TYPE_QUAL_ATOMIC)
			: TYPE_MAIN_VARIANT (TREE_TYPE (field)));
      if (aet_comptypes (lhs_main_type, fieldtype))
	{
	  if (found_field != NULL_TREE)
	    return NULL_TREE;
	  found_field = field;
	}
      else if (find_anonymous_field_with_type (TREE_TYPE (field),
					       lhs_main_type))
	{
	  if (found_field != NULL_TREE)
	    return NULL_TREE;
	  found_field = field;
	  found_sub_field = true;
	}
    }

  if (found_field == NULL_TREE)
    return NULL_TREE;

  ret = fold_build3_loc (location, COMPONENT_REF, TREE_TYPE (found_field),
			 build_fold_indirect_ref (rhs), found_field,
			 NULL_TREE);
  ret = build_fold_addr_expr_loc (location, ret);

  if (found_sub_field)
    {
      ret = convert_to_anonymous_field (location, type, ret);
      gcc_assert (ret != NULL_TREE);
    }

  return ret;
}

/* Issue an error message for a bad initializer component.
   GMSGID identifies the message.
   The component name is taken from the spelling stack.  */

static void ATTRIBUTE_GCC_DIAG (2,0)
error_init (location_t loc, const char *gmsgid, ...)
{
  char *ofwhat;

  auto_diagnostic_group d;

  /* The gmsgid may be a format string with %< and %>. */
  va_list ap;
  va_start (ap, gmsgid);
  bool warned = emit_diagnostic_valist (DK_ERROR, loc, -1, gmsgid, &ap);
  va_end (ap);

  ofwhat = print_spelling ((char *) alloca (spelling_length () + 1));
  if (*ofwhat && warned)
    inform (loc, "(near initialization for %qs)", ofwhat);
}

/* Issue a pedantic warning for a bad initializer component.  OPT is
   the option OPT_* (from options.h) controlling this warning or 0 if
   it is unconditionally given.  GMSGID identifies the message.  The
   component name is taken from the spelling stack.  */

static void ATTRIBUTE_GCC_DIAG (3,0)
pedwarn_init (location_t loc, int opt, const char *gmsgid, ...)
{
  /* Use the location where a macro was expanded rather than where
     it was defined to make sure macros defined in system headers
     but used incorrectly elsewhere are diagnosed.  */
  location_t exploc = expansion_point_location_if_in_system_header (loc);
  auto_diagnostic_group d;
  va_list ap;
  va_start (ap, gmsgid);
  bool warned = emit_diagnostic_valist (DK_PEDWARN, exploc, opt, gmsgid, &ap);
  va_end (ap);
  char *ofwhat = print_spelling ((char *) alloca (spelling_length () + 1));
  if (*ofwhat && warned)
    inform (exploc, "(near initialization for %qs)", ofwhat);
}

/* Issue a warning for a bad initializer component.

   OPT is the OPT_W* value corresponding to the warning option that
   controls this warning.  GMSGID identifies the message.  The
   component name is taken from the spelling stack.  */

static void warning_init (location_t loc, int opt, const char *gmsgid)
{
  char *ofwhat;
  bool warned;

  auto_diagnostic_group d;

  /* Use the location where a macro was expanded rather than where
     it was defined to make sure macros defined in system headers
     but used incorrectly elsewhere are diagnosed.  */
  location_t exploc = expansion_point_location_if_in_system_header (loc);

  /* The gmsgid may be a format string with %< and %>. */
  warned = warning_at (exploc, opt, gmsgid);
  ofwhat = print_spelling ((char *) alloca (spelling_length () + 1));
  if (*ofwhat && warned)
    inform (exploc, "(near initialization for %qs)", ofwhat);
}

/* If TYPE is an array type and EXPR is a parenthesized string
   constant, warn if pedantic that EXPR is being used to initialize an
   object of type TYPE.  */

void aet_maybe_warn_string_init (location_t loc, tree type, struct c_expr expr)
{
  if (pedantic
      && TREE_CODE (type) == ARRAY_TYPE
      && TREE_CODE (expr.value) == STRING_CST
      && expr.original_code != STRING_CST)
    pedwarn_init (loc, OPT_Wpedantic,
		  "array initialized from parenthesized string constant");
}

/* Attempt to locate the parameter with the given index within FNDECL,
   returning DECL_SOURCE_LOCATION (FNDECL) if it can't be found.  */

static location_t get_fndecl_argument_location (tree fndecl, int argnum)
{
  int i;
  tree param;

  /* Locate param by index within DECL_ARGUMENTS (fndecl).  */
  for (i = 0, param = DECL_ARGUMENTS (fndecl);
       i < argnum && param;
       i++, param = TREE_CHAIN (param))
    ;

  /* If something went wrong (e.g. if we have a builtin and thus no arguments),
     return DECL_SOURCE_LOCATION (FNDECL).  */
  if (param == NULL)
    return DECL_SOURCE_LOCATION (fndecl);

  return DECL_SOURCE_LOCATION (param);
}

/* Issue a note about a mismatching argument for parameter PARMNUM
   to FUNDECL, for types EXPECTED_TYPE and ACTUAL_TYPE.
   Attempt to issue the note at the pertinent parameter of the decl;
   failing that issue it at the location of FUNDECL; failing that
   issue it at PLOC.  */

static void inform_for_arg (tree fundecl, location_t ploc, int parmnum,
		tree expected_type, tree actual_type)
{
  location_t loc;
  if (fundecl && !DECL_IS_BUILTIN (fundecl))
    loc = get_fndecl_argument_location (fundecl, parmnum - 1);
  else
    loc = ploc;

  inform (loc,
	  "expected %qT but argument is of type %qT",
	  expected_type, actual_type);
}

/* Issue a warning when an argument of ARGTYPE is passed to a built-in
   function FUNDECL declared without prototype to parameter PARMNUM of
   PARMTYPE when ARGTYPE does not promote to PARMTYPE.  */

static void maybe_warn_builtin_no_proto_arg (location_t loc, tree fundecl, int parmnum,
				 tree parmtype, tree argtype)
{
  tree_code parmcode = TREE_CODE (parmtype);
  tree_code argcode = TREE_CODE (argtype);
  tree promoted = aet_c_type_promotes_to (argtype);
  tree_code promotedcode =TREE_CODE (promoted);
  printf("maybe_warn_builtin_no_proto_arg 00 parmcode:%s argcode:%s %d %d\n",
		  get_tree_code_name(parmcode),get_tree_code_name(argcode),parmcode,argcode);
  printf("maybe_warn_builtin_no_proto_arg ---00 promoted:%s argcode:%s %d %d\n",
		  get_tree_code_name(promotedcode),get_tree_code_name(argcode),promotedcode,argcode);
  tree x1=TYPE_MAIN_VARIANT (parmtype);
  tree x2=TYPE_MAIN_VARIANT (promoted);
  tree_code x1c = TREE_CODE (x1);
  tree_code x2c = TREE_CODE (x2);
  printf("maybe_warn_builtin_no_proto_arg ---www00 promoted:%s argcode:%s %d %d %p %p\n",
		  get_tree_code_name(x1c),get_tree_code_name(x2c),x1c,x2c,x1,x2);
  /* Avoid warning for enum arguments that promote to an integer type
     of the same size/mode.  */
  if (parmcode == INTEGER_TYPE && argcode == ENUMERAL_TYPE  && TYPE_MODE (parmtype) == TYPE_MODE (argtype))
    return;
  printf("maybe_warn_builtin_no_proto_arg 11 parmcode:%s argcode:%s %d %d\n",
 		  get_tree_code_name(parmcode),get_tree_code_name(argcode),parmcode,argcode);
  if ((parmcode == argcode || (parmcode == INTEGER_TYPE && argcode == ENUMERAL_TYPE)) && TYPE_MAIN_VARIANT (parmtype) == TYPE_MAIN_VARIANT (promoted))
    return;
  printf("maybe_warn_builtin_no_proto_arg 22 parmcode:%s argcode:%s %d %d\n",
  		  get_tree_code_name(parmcode),get_tree_code_name(argcode),parmcode,argcode);
  /* This diagnoses even signed/unsigned mismatches.  Those might be
     safe in many cases but GCC may emit suboptimal code for them so
     warning on those cases drives efficiency improvements.  */
  if (warning_at (loc, OPT_Wbuiltin_declaration_mismatch,
		  TYPE_MAIN_VARIANT (promoted) == argtype
		  ? G_("%qD argument %d type is %qT where %qT is expected "
		       "in a call to built-in function declared without "
		       "prototype")
		  : G_("%qD argument %d promotes to %qT where %qT is expected "
		       "in a call to built-in function declared without "
		       "prototype"),
		  fundecl, parmnum, promoted, parmtype))
    inform (DECL_SOURCE_LOCATION (fundecl),"built-in %qD declared here",fundecl);
     if(TYPE_MAIN_VARIANT (promoted) == argtype){
	    argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=xx_argument_x_type_is_xx_where_xx_is_expected_in_a_call_to_built_in_function_declared_without_prototype;
     }else{
  	    argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=xx_argument_x_promotes_to_xx_where_xx_is_expected_in_a_call_to_built_in_function_declared_without_prototype;

     }
}

/* Convert value RHS to type TYPE as preparation for an assignment to
   an lvalue of type TYPE.  If ORIGTYPE is not NULL_TREE, it is the
   original type of RHS; this differs from TREE_TYPE (RHS) for enum
   types.  NULL_POINTER_CONSTANT says whether RHS was a null pointer
   constant before any folding.
   The real work of conversion is done by `convert'.
   The purpose of this function is to generate error messages
   for assignments that are not allowed in C.
   ERRTYPE says whether it is argument passing, assignment,
   initialization or return.

   In the following example, '~' denotes where EXPR_LOC and '^' where
   LOCATION point to:

     f (var);      [ic_argpass]
     ^  ~~~
     x = var;      [ic_assign]
       ^ ~~~;
     int x = var;  [ic_init]
	     ^^^
     return x;     [ic_return]
	    ^

   FUNCTION is a tree for the function being called.
   PARMNUM is the number of the argument, for printing in error messages.
   WARNOPT may be set to a warning option to issue the corresponding warning
   rather than an error for invalid conversions.  Used for calls to built-in
   functions declared without a prototype.  */

//function 被改过的原函数 fundecl未改过的函数声明 type  实参的类型 rhs具体的实参 函数参数类型  origtype  npc实参是否是空指针
//rname =function parmnum当前第几个参数 argnum 当前第几个参数+1 excess_precision 超精度 0 是否警告
static tree convert_for_assignment (location_t location, location_t expr_loc, tree type,
			tree rhs, tree origtype, enum impl_conv errtype,
			bool null_pointer_constant, tree fundecl,
			tree function, int parmnum, int warnopt /* = 0 */)
{
  enum tree_code codel = TREE_CODE (type);
  tree orig_rhs = rhs;
  tree rhstype;
  enum tree_code coder;
  tree rname = NULL_TREE;
  bool objc_ok = false;
  n_debug("convert_for_assignment 00 start----%s %s %s",get_tree_code_name(TREE_CODE(type)),
		  get_tree_code_name(TREE_CODE(rhs)),origtype?get_tree_code_name(TREE_CODE(origtype)):"null");

  /* Use the expansion point location to handle cases such as user's
     function returning a wrong-type macro defined in a system header.  */
  location = expansion_point_location_if_in_system_header (location);

  if (errtype == ic_argpass){
	  n_debug("convert_for_assignment 11 start----");
      tree selector;
      /* Change pointer to function to the function itself for
	 diagnostics.  */
      if (TREE_CODE (function) == ADDR_EXPR && TREE_CODE (TREE_OPERAND (function, 0)) == FUNCTION_DECL)
	    function = TREE_OPERAND (function, 0);

      /* Handle an ObjC selector specially for diagnostics.  */
      selector = objc_message_selector ();
      rname = function;
      if (selector && parmnum > 2){
	    rname = selector;
	    parmnum -= 2;
	  }
 }

  /* This macro is used to emit diagnostics to ensure that all format
     strings are complete sentences, visible to gettext and checked at
     compile time.  */
#define PEDWARN_FOR_ASSIGNMENT(LOCATION, PLOC, OPT, AR, AS, IN, RE)	 \
  do {                                                                   \
    switch (errtype)                                                     \
      {                                                                  \
      case ic_argpass:                                                   \
	{								\
	  auto_diagnostic_group d;						\
	  if (pedwarn (PLOC, OPT, AR, parmnum, rname))		\
	    inform_for_arg (fundecl, (PLOC), parmnum, type, rhstype); \
	}								\
        break;                                                           \
      case ic_assign:                                                    \
        pedwarn (LOCATION, OPT, AS);                                     \
        break;                                                           \
      case ic_init:                                                      \
        pedwarn_init (LOCATION, OPT, IN);                                \
        break;                                                           \
      case ic_return:                                                    \
        pedwarn (LOCATION, OPT, RE);					 \
        break;                                                           \
      default:                                                           \
        gcc_unreachable ();                                              \
      }                                                                  \
  } while (0)

  /* This macro is used to emit diagnostics to ensure that all format
     strings are complete sentences, visible to gettext and checked at
     compile time.  It is the same as PEDWARN_FOR_ASSIGNMENT but with an
     extra parameter to enumerate qualifiers.  */
#define PEDWARN_FOR_QUALIFIERS(LOCATION, PLOC, OPT, AR, AS, IN, RE, QUALS) \
  do {                                                                   \
    switch (errtype)                                                     \
      {                                                                  \
      case ic_argpass:                                                   \
	{								\
	auto_diagnostic_group d;						\
	if (pedwarn (PLOC, OPT, AR, parmnum, rname, QUALS))		\
	  inform_for_arg (fundecl, (PLOC), parmnum, type, rhstype);	\
	}								\
        break;                                                           \
      case ic_assign:                                                    \
        pedwarn (LOCATION, OPT, AS, QUALS);				 \
        break;                                                           \
      case ic_init:                                                      \
        pedwarn (LOCATION, OPT, IN, QUALS);				 \
        break;                                                           \
      case ic_return:                                                    \
        pedwarn (LOCATION, OPT, RE, QUALS);				 \
        break;                                                           \
      default:                                                           \
        gcc_unreachable ();                                              \
      }                                                                  \
  } while (0)

  /* This macro is used to emit diagnostics to ensure that all format
     strings are complete sentences, visible to gettext and checked at
     compile time.  It is the same as PEDWARN_FOR_QUALIFIERS but uses
     warning_at instead of pedwarn.  */
#define WARNING_FOR_QUALIFIERS(LOCATION, PLOC, OPT, AR, AS, IN, RE, QUALS) \
  do {                                                                   \
    switch (errtype)                                                     \
      {                                                                  \
      case ic_argpass:                                                   \
	{								\
	  auto_diagnostic_group d;						\
	  if (warning_at (PLOC, OPT, AR, parmnum, rname, QUALS))	\
	    inform_for_arg (fundecl, (PLOC), parmnum, type, rhstype); \
	}								\
        break;                                                           \
      case ic_assign:                                                    \
        warning_at (LOCATION, OPT, AS, QUALS);                           \
        break;                                                           \
      case ic_init:                                                      \
        warning_at (LOCATION, OPT, IN, QUALS);                           \
        break;                                                           \
      case ic_return:                                                    \
        warning_at (LOCATION, OPT, RE, QUALS);                           \
        break;                                                           \
      default:                                                           \
        gcc_unreachable ();                                              \
      }                                                                  \
  } while (0)

    n_debug("convert_for_assignment 22 start----");

    if (TREE_CODE (rhs) == EXCESS_PRECISION_EXPR)
       rhs = TREE_OPERAND (rhs, 0);

    rhstype = TREE_TYPE (rhs);
    coder = TREE_CODE (rhstype);

    if (coder == ERROR_MARK){
  	   argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=error_or_warn_unknown;
       return error_mark_node;
    }

    if (warn_cxx_compat){
    	n_debug("convert_for_assignment 33 ----");
       tree checktype = origtype != NULL_TREE ? origtype : rhstype;
       if (checktype != error_mark_node && TREE_CODE (type) == ENUMERAL_TYPE && TYPE_MAIN_VARIANT (checktype) != TYPE_MAIN_VARIANT (type))
	      switch (errtype){
	         case ic_argpass:
	            if (pedwarn (expr_loc, OPT_Wc___compat, "enum conversion when passing argument %d of %qE is invalid in C++",parmnum, rname))
	               inform ((fundecl && !DECL_IS_BUILTIN (fundecl)) ? DECL_SOURCE_LOCATION (fundecl) : expr_loc,
	            		   "expected %qT but argument is of type %qT",type, rhstype);
	            break;
	         case ic_assign:
	            pedwarn (location, OPT_Wc___compat, "enum conversion from %qT to %qT in assignment is invalid in C++", rhstype, type);
	            break;
	         case ic_init:
	            pedwarn_init (location, OPT_Wc___compat, "enum conversion from %qT to %qT in initialization is invalid in C++",rhstype, type);
	            break;
	         case ic_return:
	            pedwarn (location, OPT_Wc___compat, "enum conversion from %qT to %qT in return is invalid in C++", rhstype, type);
	            break;
	         default:
	           //gcc_unreachable ();
	        	argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=error_or_warn_unknown;
	        	return error_mark_node;
	      }
    }

    if (warn_enum_conversion){
        tree checktype = origtype != NULL_TREE ? origtype : rhstype;
        if (checktype != error_mark_node && TREE_CODE (checktype) == ENUMERAL_TYPE &&
        		TREE_CODE (type) == ENUMERAL_TYPE && TYPE_MAIN_VARIANT (checktype) != TYPE_MAIN_VARIANT (type)){
	      gcc_rich_location loc (location);
	      n_debug("convert_for_assignment 44 ");
	      //warning_at (&loc, OPT_Wenum_conversion,"implicit conversion from %qT to %qT",checktype, type);
          argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=implicit_conversion_from_x_to_y;
       }
    }

    if (TYPE_MAIN_VARIANT (type) == TYPE_MAIN_VARIANT (rhstype)){
        warn_for_address_or_pointer_of_packed_member (type, orig_rhs);
        n_debug("convert_for_assignment 55 ---+++-实参与型参匹配  %s %s  %s",
			  get_tree_code_name(TREE_CODE(type)),get_tree_code_name(TREE_CODE(rhstype)),
			  get_tree_code_name(TREE_CODE(TYPE_MAIN_VARIANT (type))));
       return rhs;
    }

    if (coder == VOID_TYPE){
    	n_info("convert_for_assignment 66 ----错误 coder == VOID_TYPE");

		  /* Except for passing an argument to an unprototyped function,
		 this is a constraint violation.  When passing an argument to
		 an unprototyped function, it is compile-time undefined;
		 making it a constraint in that case was rejected in
		 DR#252.  */
		//zclei      const char msg[] = "void value not ignored as it ought to be";
		//      if (warnopt)
		//	warning_at (location, warnopt, msg);
		//      else
		//	error_at (location, msg);
  	   argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=void_value_not_ignored_as_it_ought_to_be;
       return error_mark_node;
    }
    rhs = aet_require_complete_type (location, rhs);
    if (rhs == error_mark_node)
       return error_mark_node;

    if (coder == POINTER_TYPE && aet_reject_gcc_builtin (rhs,UNKNOWN_LOCATION))
       return error_mark_node;
    n_debug("convert_for_assignment 77 ");

  /* A non-reference type can convert to a reference.  This handles
     va_start, va_copy and possibly port built-ins.  */
    if (codel == REFERENCE_TYPE && coder != REFERENCE_TYPE){
    	n_debug("convert_for_assignment 88 codel == REFERENCE_TYPE && coder != REFERENCE_TYPE");
       if (!aet_lvalue_p (rhs)){
		//zclei	  const char msg[] = "cannot pass rvalue to reference parameter";
		//	  if (warnopt)
		//	    warning_at (location, warnopt, msg);
		//	  else
		//	    error_at (location, msg);
  	      argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=cannot_pass_rvalue_to_reference_parameter;
	      return error_mark_node;
	   }
       if (!aet_c_mark_addressable (rhs))
	      return error_mark_node;
       rhs = build1 (ADDR_EXPR, build_pointer_type (TREE_TYPE (rhs)), rhs);
       SET_EXPR_LOCATION (rhs, location);

       rhs = convert_for_assignment (location, expr_loc,
				    build_pointer_type (TREE_TYPE (type)),
				    rhs, origtype, errtype,
				    null_pointer_constant, fundecl, function,
				    parmnum, warnopt);
       if (rhs == error_mark_node)
	      return error_mark_node;

       rhs = build1 (NOP_EXPR, type, rhs);
       SET_EXPR_LOCATION (rhs, location);
       return rhs;
    }else if (codel == VECTOR_TYPE && coder == VECTOR_TYPE  && vector_types_convertible_p (type, TREE_TYPE (rhs), true)){
    	  /* Some types can interconvert without explicit casts.  */
    	return aet_convert (type, rhs);
      /* Arithmetic types all interconvert, and enum is treated like int.  */
    }else if ((codel == INTEGER_TYPE || codel == REAL_TYPE
	    || codel == FIXED_POINT_TYPE
	    || codel == ENUMERAL_TYPE || codel == COMPLEX_TYPE
	    || codel == BOOLEAN_TYPE)
	   && (coder == INTEGER_TYPE || coder == REAL_TYPE
	       || coder == FIXED_POINT_TYPE
	       || coder == ENUMERAL_TYPE || coder == COMPLEX_TYPE
	       || coder == BOOLEAN_TYPE)){
    	n_debug("convert_for_assignment 99----cccc codel warnopt:%d ic_argpass:%d %s %s %s codel:%d coder:%d orig_rhs:%d",
			  warnopt,errtype == ic_argpass,get_tree_code_name(codel),
			  get_tree_code_name(coder),get_tree_code_name(TREE_CODE(orig_rhs)),codel,coder,TREE_CODE(orig_rhs));
	   if((codel== INTEGER_TYPE && coder==REAL_TYPE) || (codel== REAL_TYPE && coder==INTEGER_TYPE)){
	  	    argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=int_to_real_or_real_to_int;
	   }
       if (warnopt && errtype == ic_argpass){
	       maybe_warn_builtin_no_proto_arg (expr_loc, fundecl, parmnum, type,rhstype);
       }

       bool save = in_late_binary_op;
       if (codel == BOOLEAN_TYPE || codel == COMPLEX_TYPE  || (coder == REAL_TYPE  &&
    		  (codel == INTEGER_TYPE || codel == ENUMERAL_TYPE) && sanitize_flags_p (SANITIZE_FLOAT_CAST)))
	        in_late_binary_op = true;
       tree ret = convert_and_check (expr_loc != UNKNOWN_LOCATION ? expr_loc : location, type, orig_rhs);
       in_late_binary_op = save;
       return ret;
    }
    n_debug("convert_for_assignment 100 ");

  /* Aggregates in different TUs might need conversion.  */
    if ((codel == RECORD_TYPE || codel == UNION_TYPE) && codel == coder && aet_comptypes (type, rhstype))
       return convert_and_check (expr_loc != UNKNOWN_LOCATION? expr_loc : location, type, rhs);
    n_debug("convert_for_assignment 101 ");

    /* Conversion to a transparent union or record from its member types.
     This applies only to function arguments.  */
    if (((codel == UNION_TYPE || codel == RECORD_TYPE) && TYPE_TRANSPARENT_AGGR (type)) && errtype == ic_argpass){
    	n_debug("convert_for_assignment 102 ((codel == UNION_TYPE || codel == RECORD_TYPE)");
       tree memb, marginal_memb = NULL_TREE;
       for (memb = TYPE_FIELDS (type); memb ; memb = DECL_CHAIN (memb)){
	      tree memb_type = TREE_TYPE (memb);
	      if (aet_comptypes (TYPE_MAIN_VARIANT (memb_type),TYPE_MAIN_VARIANT (rhstype)))
	         break;

	      if (TREE_CODE (memb_type) != POINTER_TYPE)
	         continue;

	      if (coder == POINTER_TYPE){
	         tree ttl = TREE_TYPE (memb_type);
	         tree ttr = TREE_TYPE (rhstype);

				  /* Any non-function converts to a [const][volatile] void *
				 and vice versa; otherwise, targets must be the same.
				 Meanwhile, the lhs target must have all the qualifiers of
				 the rhs.  */
	         if ((VOID_TYPE_P (ttl) && !TYPE_ATOMIC (ttl)) || (VOID_TYPE_P (ttr) && !TYPE_ATOMIC (ttr)) || comp_target_types (location, memb_type, rhstype)){
		        int lquals = TYPE_QUALS (ttl) & ~TYPE_QUAL_ATOMIC;
		        int rquals = TYPE_QUALS (ttr) & ~TYPE_QUAL_ATOMIC;
		         /* If this type won't generate any warnings, use it.  */
		        if (lquals == rquals || ((TREE_CODE (ttr) == FUNCTION_TYPE && TREE_CODE (ttl) == FUNCTION_TYPE)? ((lquals | rquals) == rquals): ((lquals | rquals) == lquals)))
		           break;

		        /* Keep looking for a better type, but remember this one.  */
		        if (!marginal_memb)
		           marginal_memb = memb;
		     }
	      }

	      /* Can convert integer zero to any pointer type.  */
	      if (null_pointer_constant){
	         rhs = null_pointer_node;
	         break;
	      }
	   }// end for

       if (memb || marginal_memb){
	      if (!memb){
			  /* We have only a marginally acceptable member type;
			 it needs a warning.  */
	         tree ttl = TREE_TYPE (TREE_TYPE (marginal_memb));
	         tree ttr = TREE_TYPE (rhstype);

			  /* Const and volatile mean something different for function
			 types, so the usual warnings are not appropriate.  */
	         if (TREE_CODE (ttr) == FUNCTION_TYPE && TREE_CODE (ttl) == FUNCTION_TYPE){
			  /* Because const and volatile on functions are
				 restrictions that say the function will not do
				 certain things, it is okay to use a const or volatile
				 function where an ordinary one is wanted, but not
				 vice-versa.  */
		        if (TYPE_QUALS_NO_ADDR_SPACE (ttl)& ~TYPE_QUALS_NO_ADDR_SPACE (ttr))
		           PEDWARN_FOR_QUALIFIERS (location, expr_loc,
					    OPT_Wdiscarded_qualifiers,
					    G_("passing argument %d of %qE "
					       "makes %q#v qualified function "
					       "pointer from unqualified"),
					    G_("assignment makes %q#v qualified "
					       "function pointer from "
					       "unqualified"),
					    G_("initialization makes %q#v qualified "
					       "function pointer from "
					       "unqualified"),
					    G_("return makes %q#v qualified function "
					       "pointer from unqualified"),
					    TYPE_QUALS (ttl) & ~TYPE_QUALS (ttr));
		     }else if (TYPE_QUALS_NO_ADDR_SPACE (ttr) & ~TYPE_QUALS_NO_ADDR_SPACE (ttl))
		         PEDWARN_FOR_QUALIFIERS (location, expr_loc,
				        OPT_Wdiscarded_qualifiers,
				        G_("passing argument %d of %qE discards "
					   "%qv qualifier from pointer target type"),
				        G_("assignment discards %qv qualifier "
					   "from pointer target type"),
				        G_("initialization discards %qv qualifier "
					   "from pointer target type"),
				        G_("return discards %qv qualifier from "
					   "pointer target type"),
				        TYPE_QUALS (ttr) & ~TYPE_QUALS (ttl));

	         memb = marginal_memb;
	      }//end !memb

	      if (!fundecl || !DECL_IN_SYSTEM_HEADER (fundecl))
	         pedwarn (location, OPT_Wpedantic,"ISO C prohibits argument conversion to union type");

	      rhs = fold_convert_loc (location, TREE_TYPE (memb), rhs);
	      return build_constructor_single (type, memb, rhs);
	   }// end memb || marginal_memb
    }else if ((codel == POINTER_TYPE || codel == REFERENCE_TYPE) && (coder == codel)){   /* Conversions among pointers */
		  /* If RHS refers to a built-in declared without a prototype
		 BLTIN is the declaration of the built-in with a prototype
		 and RHSTYPE is set to the actual type of the built-in.  */
    	n_debug("convert_for_assignment 103 ((codel == UNION_TYPE || codel == RECORD_TYPE)");

       tree bltin;
       rhstype = type_or_builtin_type (rhs, &bltin);

       tree ttl = TREE_TYPE (type);
       tree ttr = TREE_TYPE (rhstype);
       tree mvl = ttl;
       tree mvr = ttr;
       bool is_opaque_pointer;
       int target_cmp = 0;   /* Cache comp_target_types () result.  */
       addr_space_t asl;
       addr_space_t asr;

       if (TREE_CODE (mvl) != ARRAY_TYPE)
	      mvl = (TYPE_ATOMIC (mvl)? aet_c_build_qualified_type (TYPE_MAIN_VARIANT (mvl),TYPE_QUAL_ATOMIC): TYPE_MAIN_VARIANT (mvl));
       if (TREE_CODE (mvr) != ARRAY_TYPE)
	      mvr = (TYPE_ATOMIC (mvr) ? aet_c_build_qualified_type (TYPE_MAIN_VARIANT (mvr),TYPE_QUAL_ATOMIC): TYPE_MAIN_VARIANT (mvr));
        /* Opaque pointers are treated like void pointers.  */
       is_opaque_pointer = vector_targets_convertible_p (ttl, ttr);

      /* The Plan 9 compiler permits a pointer to a struct to be
	 automatically converted into a pointer to an anonymous field
	 within the struct.  */
       if (flag_plan9_extensions && RECORD_OR_UNION_TYPE_P (mvl) && RECORD_OR_UNION_TYPE_P (mvr) && mvl != mvr){
	      tree new_rhs = convert_to_anonymous_field (location, type, rhs);
	      if (new_rhs != NULL_TREE){
	         rhs = new_rhs;
	         rhstype = TREE_TYPE (rhs);
	         coder = TREE_CODE (rhstype);
	         ttr = TREE_TYPE (rhstype);
	         mvr = TYPE_MAIN_VARIANT (ttr);
	      }
	   }

      /* C++ does not allow the implicit conversion void* -> T*.  However,
	 for the purpose of reducing the number of false positives, we
	 tolerate the special case of

		int *p = NULL;

	   where NULL is typically defined in C to be '(void *) 0'.  */
       n_debug("convert_for_assignment 104 !null_pointer_constant_p (rhs)  && asr != asl && !targetm.addr_space.subset_p (asr, asl)");
       if (VOID_TYPE_P (ttr) && rhs != null_pointer_node && !VOID_TYPE_P (ttl))
	        //warning_at (errtype == ic_argpass ? expr_loc : location,OPT_Wc___compat,"request for implicit conversion from %qT to %qT not permitted in C++", rhstype, type);
            argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=request_for_implicit_conversion_from_x_to_t_not_permitted_in_Cplusplus;

      /* See if the pointers point to incompatible address spaces.  */
       asl = TYPE_ADDR_SPACE (ttl);
       asr = TYPE_ADDR_SPACE (ttr);
       if (!null_pointer_constant_p (rhs) && asr != asl && !targetm.addr_space.subset_p (asr, asl)){
    	   n_debug("convert_for_assignment 105 !null_pointer_constant_p (rhs)  && asr != asl && !targetm.addr_space.subset_p (asr, asl)");
	      switch (errtype){
	         case ic_argpass:
	         {
		        //const char msg[] = G_("passing argument %d of %qE from pointer to non-enclosed address space");
		        if (warnopt)
			         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=passing_argument_x_of_y_from_pointer_to_non_enclosed_address_space;
		        else
		       	     argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=passing_argument_x_of_y_from_pointer_to_non_enclosed_address_space;
	            break;
	         }
	         case ic_assign:
	         {
		        //const char msg[] = G_("assignment from pointer to non-enclosed address space");
		        if (warnopt)
			         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=assignment_from_pointer_to_non_enclosed_address_space;
		        else
		       	   argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=assignment_from_pointer_to_non_enclosed_address_space;
		        break;
	         }
	         case ic_init:
	         {
		        //const char msg[] = G_("initialization from pointer to on-enclosed address space");
		        if (warnopt)
			         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=initialization_from_pointer_to_on_enclosed_address_space;
		        else
		       	   argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=initialization_from_pointer_to_on_enclosed_address_space;
		        break;
	         }
	         case ic_return:
	         {
		        //const char msg[] = G_("return from pointer to non-enclosed address space");
		        if (warnopt)
			         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=return_from_pointer_to_non_enclosed_address_space;
		        else
		       	   argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=return_from_pointer_to_non_enclosed_address_space;
		        break;
	         }
	         default:
	            gcc_unreachable ();
	      }
	      return error_mark_node;
	   }
       n_debug("convert_for_assignment 106 !null_pointer_constant_p (rhs)  && asr != asl && !targetm.addr_space.subset_p (asr, asl)");

      /* Check if the right-hand side has a format attribute but the
	 left-hand side doesn't.  */
       if (warn_suggest_attribute_format  && check_missing_format_attribute (type, rhstype)){
    	   n_debug("convert_for_assignment 107 arn_suggest_attribute_format  && check_missing_format_attribute (type, rhstype)");
	      switch (errtype){
	         case ic_argpass:
	            warning_at (expr_loc, OPT_Wsuggest_attribute_format,"argument %d of %qE might be a candidate for a format attribute",parmnum, rname);
	            break;
	         case ic_assign:
	            warning_at (location, OPT_Wsuggest_attribute_format,"assignment left-hand side might be a candidate for a format attribute");
	            break;
	         case ic_init:
	            warning_at (location, OPT_Wsuggest_attribute_format,"initialization left-hand side might be a candidate for a format attribute");
	            break;
	         case ic_return:
	            warning_at (location, OPT_Wsuggest_attribute_format,"return type might be a candidate for a format attribute");
	            break;
	         default:
	            gcc_unreachable ();
	      }
	   }
       n_debug("convert_for_assignment 108 ");

		  /* Any non-function converts to a [const][volatile] void *
		 and vice versa; otherwise, targets must be the same.
		 Meanwhile, the lhs target must have all the qualifiers of the rhs.  */
       if ((VOID_TYPE_P (ttl) && !TYPE_ATOMIC (ttl))
	     || (VOID_TYPE_P (ttr) && !TYPE_ATOMIC (ttr))
	     || (target_cmp = comp_target_types (location, type, rhstype))
	     || is_opaque_pointer   || ((c_common_unsigned_type (mvl)== c_common_unsigned_type (mvr))
	      && (c_common_signed_type (mvl)== c_common_signed_type (mvr)) && TYPE_ATOMIC (mvl) == TYPE_ATOMIC (mvr))){
	  /* Warn about loss of qualifers from pointers to arrays with
	     qualifiers on the element type. */
    	   n_debug("convert_for_assignment 109 VOID_TYPE_P (ttl) && !TYPE_ATOMIC (ttl))");

	      if (TREE_CODE (ttr) == ARRAY_TYPE){
	         ttr = strip_array_types (ttr);
	         ttl = strip_array_types (ttl);
	         if (TYPE_QUALS_NO_ADDR_SPACE_NO_ATOMIC (ttr) & ~TYPE_QUALS_NO_ADDR_SPACE_NO_ATOMIC (ttl))
		        WARNING_FOR_QUALIFIERS (location, expr_loc,
				        OPT_Wdiscarded_array_qualifiers,
				        G_("passing argument %d of %qE discards "
					   "%qv qualifier from pointer target type"),
				        G_("assignment discards %qv qualifier "
					   "from pointer target type"),
				        G_("initialization discards %qv qualifier "
					   "from pointer target type"),
				        G_("return discards %qv qualifier from "
					   "pointer target type"),
                                        TYPE_QUALS (ttr) & ~TYPE_QUALS (ttl));
          }else if (pedantic  && ((VOID_TYPE_P (ttl) && TREE_CODE (ttr) == FUNCTION_TYPE)
		     ||(VOID_TYPE_P (ttr)&& !null_pointer_constant && TREE_CODE (ttl) == FUNCTION_TYPE)))
	         PEDWARN_FOR_ASSIGNMENT (location, expr_loc, OPT_Wpedantic,
				    G_("ISO C forbids passing argument %d of "
				       "%qE between function pointer "
				       "and %<void *%>"),
				    G_("ISO C forbids assignment between "
				       "function pointer and %<void *%>"),
				    G_("ISO C forbids initialization between "
				       "function pointer and %<void *%>"),
				    G_("ISO C forbids return between function "
				       "pointer and %<void *%>"));
	       /* Const and volatile mean something different for function types,
	       so the usual warnings are not appropriate.  */
	      else if (TREE_CODE (ttr) != FUNCTION_TYPE  && TREE_CODE (ttl) != FUNCTION_TYPE){
			  /* Don't warn about loss of qualifier for conversions from
			 qualified void* to pointers to arrays with corresponding
			 qualifier on the element type. */
	         if (!pedantic)
	            ttl = strip_array_types (ttl);
	      /* Assignments between atomic and non-atomic objects are OK.  */
	         if (TYPE_QUALS_NO_ADDR_SPACE_NO_ATOMIC (ttr) & ~TYPE_QUALS_NO_ADDR_SPACE_NO_ATOMIC (ttl)){
	             n_debug("convert_for_assignment wwfdsss0000\n");

		        PEDWARN_FOR_QUALIFIERS (location, expr_loc,
				          OPT_Wdiscarded_qualifiers,
				          G_("passing argument %d of %qE discards "
					     "%qv qualifier from pointer target type"),
				          G_("assignment discards %qv qualifier "
					     "from pointer target type"),
				          G_("initialization discards %qv qualifier "
					     "from pointer target type"),
				          G_("return discards %qv qualifier from "
					     "pointer target type"),
				          TYPE_QUALS (ttr) & ~TYPE_QUALS (ttl));
	         }else if (VOID_TYPE_P (ttl) || VOID_TYPE_P (ttr) || target_cmp) /* If this is not a case of ignoring a mismatch in signedness,no warning.  */
		        ;
	           /* If there is a mismatch, do warn.  */
	         else if (warn_pointer_sign)
		        switch (errtype){
		           case ic_argpass:
		           {
		              auto_diagnostic_group d;
		              range_label_for_type_mismatch rhs_label (rhstype, type);
		              gcc_rich_location richloc (expr_loc, &rhs_label);
		             // if (pedwarn (&richloc, OPT_Wpointer_sign,"pointer targets in passing argument %d of %qE differ in signedness", parmnum, rname))
			            // inform_for_arg (fundecl, expr_loc, parmnum, type,rhstype);
		               argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=pointer_targets_in_assignment_from_$parmnum$_to_$rename$_differ_in_signedness;
		           }
		              break;
		           case ic_assign:
		              //pedwarn (location, OPT_Wpointer_sign,"pointer targets in assignment from %qT to %qT differ in signedness", rhstype, type);
                      argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=pointer_targets_in_assignment_from_$rhstype$_to_$type$_differ_in_signedness;
		              break;
		           case ic_init:
		              //pedwarn_init (location, OPT_Wpointer_sign,"pointer targets in initialization of %qT from %qT differ in signedness", type,rhstype);
                      argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=pointer_targets_in_initialization_of_$type$_from_$rhstype$_differ_in_signedness;
		              break;
		           case ic_return:
		              //pedwarn (location, OPT_Wpointer_sign, "pointer targets in returning %qT from a function with return type %qT differ in signedness", rhstype, type);
                      argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=pointer_targets_in_returning_$rhstype$__from_a_function_with_return_type_$type$_differ_in_signedness;
		              break;
		           default:
		             gcc_unreachable ();
		        }
	      }else if (TREE_CODE (ttl) == FUNCTION_TYPE && TREE_CODE (ttr) == FUNCTION_TYPE) {
			  /* Because const and volatile on functions are restrictions
			 that say the function will not do certain things,
			 it is okay to use a const or volatile function
			 where an ordinary one is wanted, but not vice-versa.  */
	         if (TYPE_QUALS_NO_ADDR_SPACE (ttl) & ~TYPE_QUALS_NO_ADDR_SPACE (ttr))
		        PEDWARN_FOR_QUALIFIERS (location, expr_loc,
				        OPT_Wdiscarded_qualifiers,
				        G_("passing argument %d of %qE makes "
					   "%q#v qualified function pointer "
					   "from unqualified"),
				        G_("assignment makes %q#v qualified function "
					   "pointer from unqualified"),
				        G_("initialization makes %q#v qualified "
					   "function pointer from unqualified"),
				        G_("return makes %q#v qualified function "
					   "pointer from unqualified"),
				        TYPE_QUALS (ttl) & ~TYPE_QUALS (ttr));
	      }
	}else if (!objc_ok){/* Avoid warning about the volatile ObjC EH puts on decls.  */
	   switch (errtype){
	      case ic_argpass:
	      {
		     auto_diagnostic_group d;
		     range_label_for_type_mismatch rhs_label (rhstype, type);
		     gcc_rich_location richloc (expr_loc, &rhs_label);
//		     if (pedwarn (&richloc, OPT_Wincompatible_pointer_types,"passing argument %d of %qE from incompatible pointer type", parmnum, rname))
//		        inform_for_arg (fundecl, expr_loc, parmnum, type, rhstype);
		     int result=class_util_check_param_type(type,rhstype);
		     if(result==COMPARE_ARGUMENT_ORIG_PARM_ERROR){
		         argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=compare_aet_class_argument_orig_parm_error;
		     }else if(result==COMPARE_ARGUMENT_ORIG_PARM_SKIP){
		         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=passing_argument_xx_of_xx_from_incompatible_pointer_type;
		     }

	      }
	         break;
	      case ic_assign:
	         if (bltin)
		         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=assignment_to_$type$_from_pointer_to_$bltin$_with_incompatible_type_$rhstype$;
		       // pedwarn (location, OPT_Wincompatible_pointer_types,"assignment to %qT from pointer to %qD with incompatible type %qT",type, bltin, rhstype);
	         else
		         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=assignment_to_$type$_from_incompatible_pointer_type_$rhstype$;
		        //pedwarn (location, OPT_Wincompatible_pointer_types, "assignment to %qT from incompatible pointer type %qT",type, rhstype);
	         break;
	      case ic_init:
	         if (bltin)
		         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=initialization_of_$type$_from_pointer_to_$bltin$_with_incompatible_type_$rhstype$;
		        //pedwarn_init (location, OPT_Wincompatible_pointer_types,"initialization of %qT from pointer to %qD with incompatible type %qT",type, bltin, rhstype);
	         else
		         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=initialization_of_$type$_from_incompatible_pointer_type_$rhstype$;
		        //pedwarn_init (location, OPT_Wincompatible_pointer_types,"initialization of %qT from incompatible pointer type %qT",type, rhstype);
	         break;
	      case ic_return:
	         if (bltin)
		         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=returning_pointer_to_$bltin$_of_type_$rhstype$_from_a_function_with_incompatible_type_$type$;
		        //pedwarn (location, OPT_Wincompatible_pointer_types,"returning pointer to %qD of type %qT from a function with incompatible type %qT",bltin, rhstype, type);
	         else
		         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=returning_$rhstype$_from_a_function_with_incompatible_return_type_$type$;
		        //pedwarn (location, OPT_Wincompatible_pointer_types,"returning %qT from a function with incompatible return type %qT", rhstype, type);
	         break;
	      default:
	        gcc_unreachable ();
	    }
	}
       n_debug("convert_for_assignment 112 ");

      /* If RHS isn't an address, check pointer or array of packed
	 struct or union.  */
      warn_for_address_or_pointer_of_packed_member (type, orig_rhs);
      return aet_convert (type, rhs);
    }else if (codel == POINTER_TYPE && coder == ARRAY_TYPE){
      /* ??? This should not be an error when inlining calls to
	 unprototyped functions.  */
    	n_debug("convert_for_assignment 113 ");

       //const char msg[] = "invalid use of non-lvalue array";
       if (warnopt)
	     //warning_at (location, warnopt, msg);
         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=invalid_use_of_non_lvalue_array;
       else
	     //error_at (location, msg);
         argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=invalid_use_of_non_lvalue_array;

       return error_mark_node;
    }else if (codel == POINTER_TYPE && coder == INTEGER_TYPE){
      /* An explicit constant 0 can convert to a pointer,
	 or one that results from arithmetic, even including
	 a cast to integer type.  */
    	n_debug("convert_for_assignment 114 null_pointer_constant:%d",null_pointer_constant);
       //if (!null_pointer_constant)
	      switch (errtype){
	         case ic_argpass:
	            argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=passing_argument_x_of_y_makes_pointer_from_integer_without_a_cast;
	            break;
	         case ic_assign:
	            //pedwarn (location, OPT_Wint_conversion,"assignment to %qT from %qT makes pointer from integer without a cast", type, rhstype);
	            argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=assignment_to_x_from_y_makes_pointer_from_integer_without_a_cast;
	            break;
	         case ic_init:
	            //pedwarn_init (location, OPT_Wint_conversion,"initialization of %qT from %qT makes pointer from integer without a cast", type, rhstype);
	            argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=initialization_of_x_from_y_makes_pointer_from_integer_without_a_cast;
	            break;
	         case ic_return:
	            //pedwarn (location, OPT_Wint_conversion, "returning %qT from a function with return type %qT makes pointer from integer without a cast", rhstype, type);
		        argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=returning_x_from_a_function_with_return_type_y_makes_pointer_from_integer_without_a_cast;
	            break;
	         default:
	        	argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=go_unreachable;
	            return error_mark_node;
	            //gcc_unreachable ();
	      }
       return rhs;//aet_convert (type, rhs);
    }else if (codel == INTEGER_TYPE && coder == POINTER_TYPE){
    	n_debug("convert_for_assignment 115 ");
       switch (errtype){
		 case ic_argpass:
			argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=passing_argument_x_of_y_makes_integer_from_pointer_without_a_cast;
			break;
		 case ic_assign:
			//pedwarn (location, OPT_Wint_conversion,"assignment to %qT from %qT makes pointer from integer without a cast", type, rhstype);
			argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=assignment_to_x_from_y_makes_integer_from_pointer_without_a_cast;
			break;
		 case ic_init:
			//pedwarn_init (location, OPT_Wint_conversion,"initialization of %qT from %qT makes pointer from integer without a cast", type, rhstype);
			argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=initialization_of_x_from_y_makes_integer_from_pointer_without_a_cast;
			break;
		 case ic_return:
			//pedwarn (location, OPT_Wint_conversion, "returning %qT from a function with return type %qT makes pointer from integer without a cast", rhstype, type);
			argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=returning_x_from_a_function_with_return_type_y_makes_integer_from_pointer_without_a_cast;
			break;
		 default:
			argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=go_unreachable;
			return error_mark_node;
     	            //gcc_unreachable ();
       }
       return rhs;//aet_convert (type, rhs);
    }else if (codel == BOOLEAN_TYPE && coder == POINTER_TYPE){
//      tree ret;
//      bool save = in_late_binary_op;
//      in_late_binary_op = true;
//      ret = aet_convert (type, rhs);
//      in_late_binary_op = save;
//      return ret;
    	return rhs;
    }
    n_debug("convert_for_assignment 116 ");
    switch (errtype){
       case ic_argpass:
       {
	      auto_diagnostic_group d;
	      range_label_for_type_mismatch rhs_label (rhstype, type);
	      gcc_rich_location richloc (expr_loc, &rhs_label);
	      //const char msg[] = G_("incompatible type for argument %d of %qE");
	      if (warnopt)
	         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=incompatible_type_for_argument_x_of_y;
	      else
			 argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=incompatible_type_for_argument_x_of_y;
	      //inform_for_arg (fundecl, expr_loc, parmnum, type, rhstype);
       }
          break;
       case ic_assign:
       {
	      //const char msg[]= G_("incompatible types when assigning to type %qT from type %qT");
	      if (warnopt)
		         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=incompatible_types_when_assigning_to_type_x_from_type_y;
	      else
				 argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=incompatible_types_when_assigning_to_type_x_from_type_y;
	      break;
       }
       case ic_init:
       {
	      //const char msg[]= G_("incompatible types when initializing type %qT using type %qT");
	      if (warnopt)
		         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=incompatible_types_when_initializing_type_x_using_type_y;
	      else
				 argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=incompatible_types_when_initializing_type_x_using_type_y;
	      break;
       }
       case ic_return:
       {
	      //const char msg[]= G_("incompatible types when returning type %qT but %qT was expected");
	      if (warnopt)
		         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=incompatible_types_when_returning_type_x_but_y_was_expected;
	      else
				 argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=incompatible_types_when_returning_type_x_but_y_was_expected;
	      break;
      }
       default:
          gcc_unreachable ();
    }
    return error_mark_node;
}

/* If VALUE is a compound expr all of whose expressions are constant, then
   return its value.  Otherwise, return error_mark_node.

   This is for handling COMPOUND_EXPRs as initializer elements
   which is allowed with a warning when -pedantic is specified.  */

static tree valid_compound_expr_initializer (tree value, tree endtype)
{
  if (TREE_CODE (value) == COMPOUND_EXPR)
    {
      if (valid_compound_expr_initializer (TREE_OPERAND (value, 0), endtype)
	  == error_mark_node)
	return error_mark_node;
      return valid_compound_expr_initializer (TREE_OPERAND (value, 1),
					      endtype);
    }
  else if (!initializer_constant_valid_p (value, endtype))
    return error_mark_node;
  else
    return value;
}


/* Methods for storing and printing names for error messages.  */

/* Implement a spelling stack that allows components of a name to be pushed
   and popped.  Each element on the stack is this structure.  */

struct spelling
{
  int kind;
  union
    {
      unsigned HOST_WIDE_INT i;
      const char *s;
    } u;
};

#define SPELLING_STRING 1
#define SPELLING_MEMBER 2
#define SPELLING_BOUNDS 3

static struct spelling *spelling;	/* Next stack element (unused).  */
static struct spelling *spelling_base;	/* Spelling stack base.  */
static int spelling_size;		/* Size of the spelling stack.  */

/* Macros to save and restore the spelling stack around push_... functions.
   Alternative to SAVE_SPELLING_STACK.  */

#define SPELLING_DEPTH() (spelling - spelling_base)
#define RESTORE_SPELLING_DEPTH(DEPTH) (spelling = spelling_base + (DEPTH))

/* Push an element on the spelling stack with type KIND and assign VALUE
   to MEMBER.  */

#define PUSH_SPELLING(KIND, VALUE, MEMBER)				\
{									\
  int depth = SPELLING_DEPTH ();					\
									\
  if (depth >= spelling_size)						\
    {									\
      spelling_size += 10;						\
      spelling_base = XRESIZEVEC (struct spelling, spelling_base,	\
				  spelling_size);			\
      RESTORE_SPELLING_DEPTH (depth);					\
    }									\
									\
  spelling->kind = (KIND);						\
  spelling->MEMBER = (VALUE);						\
  spelling++;								\
}




/* Compute the maximum size in bytes of the printed spelling.  */

static int spelling_length (void)
{
  int size = 0;
  struct spelling *p;

  for (p = spelling_base; p < spelling; p++)
    {
      if (p->kind == SPELLING_BOUNDS)
	size += 25;
      else
	size += strlen (p->u.s) + 1;
    }

  return size;
}

/* Print the spelling to BUFFER and return it.  */

static char * print_spelling (char *buffer)
{
  char *d = buffer;
  struct spelling *p;

  for (p = spelling_base; p < spelling; p++)
    if (p->kind == SPELLING_BOUNDS)
      {
	sprintf (d, "[" HOST_WIDE_INT_PRINT_UNSIGNED "]", p->u.i);
	d += strlen (d);
      }
    else
      {
	const char *s;
	if (p->kind == SPELLING_MEMBER)
	  *d++ = '.';
	for (s = p->u.s; (*d = *s++); d++)
	  ;
      }
  *d++ = '\0';
  return buffer;
}

/* Digest the parser output INIT as an initializer for type TYPE.
   Return a C expression of type TYPE to represent the initial value.

   If ORIGTYPE is not NULL_TREE, it is the original type of INIT.

   NULL_POINTER_CONSTANT is true if INIT is a null pointer constant.

   If INIT is a string constant, STRICT_STRING is true if it is
   unparenthesized or we should not warn here for it being parenthesized.
   For other types of INIT, STRICT_STRING is not used.

   INIT_LOC is the location of the INIT.

   REQUIRE_CONSTANT requests an error if non-constant initializers or
   elements are seen.  */

static tree digest_init (location_t init_loc, tree type, tree init, tree origtype,
    	     bool null_pointer_constant, bool strict_string,
	     int require_constant)
{
  enum tree_code code = TREE_CODE (type);
  tree inside_init = init;
  tree semantic_type = NULL_TREE;
  bool maybe_const = true;

  if (type == error_mark_node
      || !init
      || error_operand_p (init))
    return error_mark_node;

  STRIP_TYPE_NOPS (inside_init);

  if (TREE_CODE (inside_init) == EXCESS_PRECISION_EXPR)
    {
      semantic_type = TREE_TYPE (inside_init);
      inside_init = TREE_OPERAND (inside_init, 0);
    }
  inside_init = c_fully_fold (inside_init, require_constant, &maybe_const);

  /* Initialization of an array of chars from a string constant
     optionally enclosed in braces.  */

  if (code == ARRAY_TYPE && inside_init
      && TREE_CODE (inside_init) == STRING_CST)
    {
      tree typ1
	= (TYPE_ATOMIC (TREE_TYPE (type))
	   ? aet_c_build_qualified_type (TYPE_MAIN_VARIANT (TREE_TYPE (type)),
				     TYPE_QUAL_ATOMIC)
	   : TYPE_MAIN_VARIANT (TREE_TYPE (type)));
      /* Note that an array could be both an array of character type
	 and an array of wchar_t if wchar_t is signed char or unsigned
	 char.  */
      bool char_array = (typ1 == char_type_node
			 || typ1 == signed_char_type_node
			 || typ1 == unsigned_char_type_node);
      bool wchar_array = !!aet_comptypes (typ1, wchar_type_node);
      bool char16_array = !!aet_comptypes (typ1, char16_type_node);
      bool char32_array = !!aet_comptypes (typ1, char32_type_node);

      if (char_array || wchar_array || char16_array || char32_array)
	{
	  struct c_expr expr;
	  tree typ2 = TYPE_MAIN_VARIANT (TREE_TYPE (TREE_TYPE (inside_init)));
	  bool incompat_string_cst = false;
	  expr.value = inside_init;
	  expr.original_code = (strict_string ? STRING_CST : ERROR_MARK);
	  expr.original_type = NULL;
	  aet_maybe_warn_string_init (init_loc, type, expr);

	  if (TYPE_DOMAIN (type) && !TYPE_MAX_VALUE (TYPE_DOMAIN (type)))
	    pedwarn_init (init_loc, OPT_Wpedantic,
			  "initialization of a flexible array member");

	  if (aet_comptypes (TYPE_MAIN_VARIANT (TREE_TYPE (inside_init)),
			 TYPE_MAIN_VARIANT (type)))
	    return inside_init;

	  if (char_array)
	    {
	      if (typ2 != char_type_node)
		incompat_string_cst = true;
	    }
	  else if (!aet_comptypes (typ1, typ2))
	    incompat_string_cst = true;

          if (incompat_string_cst)
            {
	      error_init (init_loc, "cannot initialize array of %qT from "
			  "a string literal with type array of %qT",
			  typ1, typ2);
	      return error_mark_node;
            }

	  if (TYPE_DOMAIN (type) != NULL_TREE
	      && TYPE_SIZE (type) != NULL_TREE
	      && TREE_CODE (TYPE_SIZE (type)) == INTEGER_CST)
	    {
	      unsigned HOST_WIDE_INT len = TREE_STRING_LENGTH (inside_init);
	      unsigned unit = TYPE_PRECISION (typ1) / BITS_PER_UNIT;

	      /* Subtract the size of a single (possibly wide) character
		 because it's ok to ignore the terminating null char
		 that is counted in the length of the constant.  */
	      if (compare_tree_int (TYPE_SIZE_UNIT (type), len - unit) < 0)
		pedwarn_init (init_loc, 0,
			      ("initializer-string for array of %qT "
			       "is too long"), typ1);
	      else if (warn_cxx_compat
		       && compare_tree_int (TYPE_SIZE_UNIT (type), len) < 0)
		warning_at (init_loc, OPT_Wc___compat,
			    ("initializer-string for array of %qT "
			     "is too long for C++"), typ1);
	      if (compare_tree_int (TYPE_SIZE_UNIT (type), len) < 0)
		{
		  unsigned HOST_WIDE_INT size
		    = tree_to_uhwi (TYPE_SIZE_UNIT (type));
		  const char *p = TREE_STRING_POINTER (inside_init);

		  inside_init = build_string (size, p);
		}
	    }

	  TREE_TYPE (inside_init) = type;
	  return inside_init;
	}
      else if (INTEGRAL_TYPE_P (typ1))
	{
	  error_init (init_loc, "array of inappropriate type initialized "
		      "from string constant");
	  return error_mark_node;
	}
    }

  /* Build a VECTOR_CST from a *constant* vector constructor.  If the
     vector constructor is not constant (e.g. {1,2,3,foo()}) then punt
     below and handle as a constructor.  */
  if (code == VECTOR_TYPE
      && VECTOR_TYPE_P (TREE_TYPE (inside_init))
      && vector_types_convertible_p (TREE_TYPE (inside_init), type, true)
      && TREE_CONSTANT (inside_init))
    {
      if (TREE_CODE (inside_init) == VECTOR_CST
	  && aet_comptypes (TYPE_MAIN_VARIANT (TREE_TYPE (inside_init)),
			TYPE_MAIN_VARIANT (type)))
	return inside_init;

      if (TREE_CODE (inside_init) == CONSTRUCTOR)
	{
	  unsigned HOST_WIDE_INT ix;
	  tree value;
	  bool constant_p = true;

	  /* Iterate through elements and check if all constructor
	     elements are *_CSTs.  */
	  FOR_EACH_CONSTRUCTOR_VALUE (CONSTRUCTOR_ELTS (inside_init), ix, value)
	    if (!CONSTANT_CLASS_P (value))
	      {
		constant_p = false;
		break;
	      }

	  if (constant_p)
	    return build_vector_from_ctor (type,
					   CONSTRUCTOR_ELTS (inside_init));
	}
    }

  if (warn_sequence_point)
    verify_sequence_points (inside_init);

  /* Any type can be initialized
     from an expression of the same type, optionally with braces.  */

  if (inside_init && TREE_TYPE (inside_init) != NULL_TREE
      && (aet_comptypes (TYPE_MAIN_VARIANT (TREE_TYPE (inside_init)),
		     TYPE_MAIN_VARIANT (type))
	  || (code == ARRAY_TYPE
	      && aet_comptypes (TREE_TYPE (inside_init), type))
	  || (gnu_vector_type_p (type)
	      && aet_comptypes (TREE_TYPE (inside_init), type))
	  || (code == POINTER_TYPE
	      && TREE_CODE (TREE_TYPE (inside_init)) == ARRAY_TYPE
	      && aet_comptypes (TREE_TYPE (TREE_TYPE (inside_init)),
			    TREE_TYPE (type)))))
    {
      if (code == POINTER_TYPE)
	{
	  if (TREE_CODE (TREE_TYPE (inside_init)) == ARRAY_TYPE)
	    {
	      if (TREE_CODE (inside_init) == STRING_CST
		  || TREE_CODE (inside_init) == COMPOUND_LITERAL_EXPR)
		inside_init = array_to_pointer_conversion
		  (init_loc, inside_init);
	      else
		{
		  error_init (init_loc, "invalid use of non-lvalue array");
		  return error_mark_node;
		}
	    }
	}

      if (code == VECTOR_TYPE)
	/* Although the types are compatible, we may require a
	   conversion.  */
	inside_init = aet_convert (type, inside_init);

      if (require_constant
	  && TREE_CODE (inside_init) == COMPOUND_LITERAL_EXPR)
	{
	  /* As an extension, allow initializing objects with static storage
	     duration with compound literals (which are then treated just as
	     the brace enclosed list they contain).  Also allow this for
	     vectors, as we can only assign them with compound literals.  */
	  if (flag_isoc99 && code != VECTOR_TYPE)
	    pedwarn_init (init_loc, OPT_Wpedantic, "initializer element "
			  "is not constant");
	  tree decl = COMPOUND_LITERAL_EXPR_DECL (inside_init);
	  inside_init = DECL_INITIAL (decl);
	}

      if (code == ARRAY_TYPE && TREE_CODE (inside_init) != STRING_CST
	  && TREE_CODE (inside_init) != CONSTRUCTOR)
	{
	  error_init (init_loc, "array initialized from non-constant array "
		      "expression");
	  return error_mark_node;
	}

      /* Compound expressions can only occur here if -Wpedantic or
	 -pedantic-errors is specified.  In the later case, we always want
	 an error.  In the former case, we simply want a warning.  */
      if (require_constant && pedantic
	  && TREE_CODE (inside_init) == COMPOUND_EXPR)
	{
	  inside_init
	    = valid_compound_expr_initializer (inside_init,
					       TREE_TYPE (inside_init));
	  if (inside_init == error_mark_node)
	    error_init (init_loc, "initializer element is not constant");
	  else
	    pedwarn_init (init_loc, OPT_Wpedantic,
			  "initializer element is not constant");
	  if (flag_pedantic_errors)
	    inside_init = error_mark_node;
	}
      else if (require_constant
	       && !initializer_constant_valid_p (inside_init,
						 TREE_TYPE (inside_init)))
	{
	  error_init (init_loc, "initializer element is not constant");
	  inside_init = error_mark_node;
	}
      else if (require_constant && !maybe_const)
	pedwarn_init (init_loc, OPT_Wpedantic,
		      "initializer element is not a constant expression");

      /* Added to enable additional -Wsuggest-attribute=format warnings.  */
      if (TREE_CODE (TREE_TYPE (inside_init)) == POINTER_TYPE)
	inside_init = convert_for_assignment (init_loc, UNKNOWN_LOCATION,
					      type, inside_init, origtype,
					      ic_init, null_pointer_constant,
					      NULL_TREE, NULL_TREE, 0);
      return inside_init;
    }

  /* Handle scalar types, including conversions.  */

  if (code == INTEGER_TYPE || code == REAL_TYPE || code == FIXED_POINT_TYPE
      || code == POINTER_TYPE || code == ENUMERAL_TYPE || code == BOOLEAN_TYPE
      || code == COMPLEX_TYPE || code == VECTOR_TYPE)
    {
      if (TREE_CODE (TREE_TYPE (init)) == ARRAY_TYPE
	  && (TREE_CODE (init) == STRING_CST
	      || TREE_CODE (init) == COMPOUND_LITERAL_EXPR))
	inside_init = init = array_to_pointer_conversion (init_loc, init);
      if (semantic_type)
	inside_init = build1 (EXCESS_PRECISION_EXPR, semantic_type,
			      inside_init);
      inside_init
	= convert_for_assignment (init_loc, UNKNOWN_LOCATION, type,
				  inside_init, origtype, ic_init,
				  null_pointer_constant, NULL_TREE, NULL_TREE,
				  0);

      /* Check to see if we have already given an error message.  */
      if (inside_init == error_mark_node)
	;
      else if (require_constant && !TREE_CONSTANT (inside_init))
	{
	  error_init (init_loc, "initializer element is not constant");
	  inside_init = error_mark_node;
	}
      else if (require_constant
	       && !initializer_constant_valid_p (inside_init,
						 TREE_TYPE (inside_init)))
	{
	  error_init (init_loc, "initializer element is not computable at "
		      "load time");
	  inside_init = error_mark_node;
	}
      else if (require_constant && !maybe_const)
	pedwarn_init (init_loc, OPT_Wpedantic,
		      "initializer element is not a constant expression");

      return inside_init;
    }

  /* Come here only for records and arrays.  */

  if (COMPLETE_TYPE_P (type) && TREE_CODE (TYPE_SIZE (type)) != INTEGER_CST)
    {
      error_init (init_loc, "variable-sized object may not be initialized");
      return error_mark_node;
    }

  error_init (init_loc, "invalid initializer");
  return error_mark_node;
}

/* Handle initializers that use braces.  */

/* Type of object we are accumulating a constructor for.
   This type is always a RECORD_TYPE, UNION_TYPE or ARRAY_TYPE.  */
static tree constructor_type;


/* For a RECORD_TYPE, this is the first field not yet written out.  */
static tree constructor_unfilled_fields;

/* For an ARRAY_TYPE, this is the index of the first element
   not yet written out.  */
static tree constructor_unfilled_index;


/* If we are saving up the elements rather than allocating them,
   this is the list of elements so far (in reverse order,
   most recent first).  */
static vec<constructor_elt, va_gc> *constructor_elements;

/* 1 if constructor should be incrementally stored into a constructor chain,
   0 if all the elements should be kept in AVL tree.  */
static int constructor_incremental;

/* 1 if so far this constructor's elements are all compile-time constants.  */
static int constructor_constant;

/* 1 if so far this constructor's elements are all valid address constants.  */
static int constructor_simple;

/* 1 if this constructor has an element that cannot be part of a
   constant expression.  */
static int constructor_nonconst;

/* 1 if this constructor is erroneous so far.  */
static int constructor_erroneous;


/* Structure for managing pending initializer elements, organized as an
   AVL tree.  */

struct init_node
{
  struct init_node *left, *right;
  struct init_node *parent;
  int balance;
  tree purpose;
  tree value;
  tree origtype;
};

/* Tree of pending elements at this constructor level.
   These are elements encountered out of order
   which belong at places we haven't reached yet in actually
   writing the output.
   Will never hold tree nodes across GC runs.  */
static struct init_node *constructor_pending_elts;


/* Add a new initializer to the tree of pending initializers.  PURPOSE
   identifies the initializer, either array index or field in a structure.
   VALUE is the value of that index or field.  If ORIGTYPE is not
   NULL_TREE, it is the original type of VALUE.

   IMPLICIT is true if value comes from aet_pop_init_level (1),
   the new initializer has been merged with the existing one
   and thus no warnings should be emitted about overriding an
   existing initializer.  */

static void add_pending_init (location_t loc, tree purpose, tree value, tree origtype, bool implicit, struct obstack *braced_init_obstack)
{
  struct init_node *p, **q, *r;

  q = &constructor_pending_elts;
  p = 0;

  if (TREE_CODE (constructor_type) == ARRAY_TYPE)
    {
      while (*q != 0)
	{
	  p = *q;
	  if (tree_int_cst_lt (purpose, p->purpose))
	    q = &p->left;
	  else if (tree_int_cst_lt (p->purpose, purpose))
	    q = &p->right;
	  else
	    {
	      if (!implicit)
		{
		  if (TREE_SIDE_EFFECTS (p->value))
		    warning_init (loc, OPT_Woverride_init_side_effects,
				  "initialized field with side-effects "
				  "overwritten");
		  else if (warn_override_init)
		    warning_init (loc, OPT_Woverride_init,
				  "initialized field overwritten");
		}
	      p->value = value;
	      p->origtype = origtype;
	      return;
	    }
	}
    }
  else
    {
      tree bitpos;

      bitpos = bit_position (purpose);
      while (*q != NULL)
	{
	  p = *q;
	  if (tree_int_cst_lt (bitpos, bit_position (p->purpose)))
	    q = &p->left;
	  else if (p->purpose != purpose)
	    q = &p->right;
	  else
	    {
	      if (!implicit)
		{
		  if (TREE_SIDE_EFFECTS (p->value))
		    warning_init (loc, OPT_Woverride_init_side_effects,
				  "initialized field with side-effects "
				  "overwritten");
		  else if (warn_override_init)
		    warning_init (loc, OPT_Woverride_init,
				  "initialized field overwritten");
		}
	      p->value = value;
	      p->origtype = origtype;
	      return;
	    }
	}
    }

  r = (struct init_node *) obstack_alloc (braced_init_obstack,
					  sizeof (struct init_node));
  r->purpose = purpose;
  r->value = value;
  r->origtype = origtype;

  *q = r;
  r->parent = p;
  r->left = 0;
  r->right = 0;
  r->balance = 0;

  while (p)
    {
      struct init_node *s;

      if (r == p->left)
	{
	  if (p->balance == 0)
	    p->balance = -1;
	  else if (p->balance < 0)
	    {
	      if (r->balance < 0)
		{
		  /* L rotation.  */
		  p->left = r->right;
		  if (p->left)
		    p->left->parent = p;
		  r->right = p;

		  p->balance = 0;
		  r->balance = 0;

		  s = p->parent;
		  p->parent = r;
		  r->parent = s;
		  if (s)
		    {
		      if (s->left == p)
			s->left = r;
		      else
			s->right = r;
		    }
		  else
		    constructor_pending_elts = r;
		}
	      else
		{
		  /* LR rotation.  */
		  struct init_node *t = r->right;

		  r->right = t->left;
		  if (r->right)
		    r->right->parent = r;
		  t->left = r;

		  p->left = t->right;
		  if (p->left)
		    p->left->parent = p;
		  t->right = p;

		  p->balance = t->balance < 0;
		  r->balance = -(t->balance > 0);
		  t->balance = 0;

		  s = p->parent;
		  p->parent = t;
		  r->parent = t;
		  t->parent = s;
		  if (s)
		    {
		      if (s->left == p)
			s->left = t;
		      else
			s->right = t;
		    }
		  else
		    constructor_pending_elts = t;
		}
	      break;
	    }
	  else
	    {
	      /* p->balance == +1; growth of left side balances the node.  */
	      p->balance = 0;
	      break;
	    }
	}
      else /* r == p->right */
	{
	  if (p->balance == 0)
	    /* Growth propagation from right side.  */
	    p->balance++;
	  else if (p->balance > 0)
	    {
	      if (r->balance > 0)
		{
		  /* R rotation.  */
		  p->right = r->left;
		  if (p->right)
		    p->right->parent = p;
		  r->left = p;

		  p->balance = 0;
		  r->balance = 0;

		  s = p->parent;
		  p->parent = r;
		  r->parent = s;
		  if (s)
		    {
		      if (s->left == p)
			s->left = r;
		      else
			s->right = r;
		    }
		  else
		    constructor_pending_elts = r;
		}
	      else /* r->balance == -1 */
		{
		  /* RL rotation */
		  struct init_node *t = r->left;

		  r->left = t->right;
		  if (r->left)
		    r->left->parent = r;
		  t->right = r;

		  p->right = t->left;
		  if (p->right)
		    p->right->parent = p;
		  t->left = p;

		  r->balance = (t->balance < 0);
		  p->balance = -(t->balance > 0);
		  t->balance = 0;

		  s = p->parent;
		  p->parent = t;
		  r->parent = t;
		  t->parent = s;
		  if (s)
		    {
		      if (s->left == p)
			s->left = t;
		      else
			s->right = t;
		    }
		  else
		    constructor_pending_elts = t;
		}
	      break;
	    }
	  else
	    {
	      /* p->balance == -1; growth of right side balances the node.  */
	      p->balance = 0;
	      break;
	    }
	}

      r = p;
      p = p->parent;
    }
}

/* Build AVL tree from a sorted chain.  */

static void set_nonincremental_init (struct obstack * braced_init_obstack)
{
  unsigned HOST_WIDE_INT ix;
  tree index, value;

  if (TREE_CODE (constructor_type) != RECORD_TYPE
      && TREE_CODE (constructor_type) != ARRAY_TYPE)
    return;

  FOR_EACH_CONSTRUCTOR_ELT (constructor_elements, ix, index, value)
    add_pending_init (input_location, index, value, NULL_TREE, true,
		      braced_init_obstack);
  constructor_elements = NULL;
  if (TREE_CODE (constructor_type) == RECORD_TYPE)
    {
      constructor_unfilled_fields = TYPE_FIELDS (constructor_type);
      /* Skip any nameless bit fields at the beginning.  */
      while (constructor_unfilled_fields != NULL_TREE
	     && DECL_UNNAMED_BIT_FIELD (constructor_unfilled_fields))
	constructor_unfilled_fields = TREE_CHAIN (constructor_unfilled_fields);

    }
  else if (TREE_CODE (constructor_type) == ARRAY_TYPE)
    {
      if (TYPE_DOMAIN (constructor_type))
	constructor_unfilled_index
	    = aet_convert (bitsizetype,
		       TYPE_MIN_VALUE (TYPE_DOMAIN (constructor_type)));
      else
	constructor_unfilled_index = bitsize_zero_node;
    }
  constructor_incremental = 0;
}


/* "Output" the next constructor element.
   At top level, really output it to assembler code now.
   Otherwise, collect it in a list from which we will make a CONSTRUCTOR.
   If ORIGTYPE is not NULL_TREE, it is the original type of VALUE.
   TYPE is the data type that the containing data type wants here.
   FIELD is the field (a FIELD_DECL) or the index that this element fills.
   If VALUE is a string constant, STRICT_STRING is true if it is
   unparenthesized or we should not warn here for it being parenthesized.
   For other types of VALUE, STRICT_STRING is not used.

   PENDING if true means output pending elements that belong
   right after this element.  (PENDING is normally true;
   it is false while outputting pending elements, to avoid recursion.)

   IMPLICIT is true if value comes from aet_pop_init_level (1),
   the new initializer has been merged with the existing one
   and thus no warnings should be emitted about overriding an
   existing initializer.  */

static void output_init_element (location_t loc, tree value, tree origtype,
		     bool strict_string, tree type, tree field, bool pending,
		     bool implicit, struct obstack * braced_init_obstack)
{
  tree semantic_type = NULL_TREE;
  bool maybe_const = true;
  bool npc;

  if (type == error_mark_node || value == error_mark_node)
    {
      constructor_erroneous = 1;
      return;
    }
  if (TREE_CODE (TREE_TYPE (value)) == ARRAY_TYPE
      && (TREE_CODE (value) == STRING_CST
	  || TREE_CODE (value) == COMPOUND_LITERAL_EXPR)
      && !(TREE_CODE (value) == STRING_CST
	   && TREE_CODE (type) == ARRAY_TYPE
	   && INTEGRAL_TYPE_P (TREE_TYPE (type)))
      && !aet_comptypes (TYPE_MAIN_VARIANT (TREE_TYPE (value)),
		     TYPE_MAIN_VARIANT (type)))
    value = array_to_pointer_conversion (input_location, value);

  if (TREE_CODE (value) == COMPOUND_LITERAL_EXPR
      && require_constant_value && pending)
    {
      /* As an extension, allow initializing objects with static storage
	 duration with compound literals (which are then treated just as
	 the brace enclosed list they contain).  */
      if (flag_isoc99)
	pedwarn_init (loc, OPT_Wpedantic, "initializer element is not "
		      "constant");
      tree decl = COMPOUND_LITERAL_EXPR_DECL (value);
      value = DECL_INITIAL (decl);
    }

  npc = null_pointer_constant_p (value);
  if (TREE_CODE (value) == EXCESS_PRECISION_EXPR)
    {
      semantic_type = TREE_TYPE (value);
      value = TREE_OPERAND (value, 0);
    }
  value = c_fully_fold (value, require_constant_value, &maybe_const);

  if (value == error_mark_node)
    constructor_erroneous = 1;
  else if (!TREE_CONSTANT (value))
    constructor_constant = 0;
  else if (!initializer_constant_valid_p (value,
					  TREE_TYPE (value),
					  AGGREGATE_TYPE_P (constructor_type)
					  && TYPE_REVERSE_STORAGE_ORDER
					     (constructor_type))
	   || (RECORD_OR_UNION_TYPE_P (constructor_type)
	       && DECL_C_BIT_FIELD (field)
	       && TREE_CODE (value) != INTEGER_CST))
    constructor_simple = 0;
  if (!maybe_const)
    constructor_nonconst = 1;

  /* Digest the initializer and issue any errors about incompatible
     types before issuing errors about non-constant initializers.  */
  tree new_value = value;
  if (semantic_type)
    new_value = build1 (EXCESS_PRECISION_EXPR, semantic_type, value);
  new_value = digest_init (loc, type, new_value, origtype, npc, strict_string,
			   require_constant_value);
  if (new_value == error_mark_node)
    {
      constructor_erroneous = 1;
      return;
    }
  if (require_constant_value || require_constant_elements)
    constant_expression_warning (new_value);

  /* Proceed to check the constness of the original initializer.  */
  if (!initializer_constant_valid_p (value, TREE_TYPE (value)))
    {
      if (require_constant_value)
	{
	  error_init (loc, "initializer element is not constant");
	  value = error_mark_node;
	}
      else if (require_constant_elements)
	pedwarn (loc, OPT_Wpedantic,
		 "initializer element is not computable at load time");
    }
  else if (!maybe_const
	   && (require_constant_value || require_constant_elements))
    pedwarn_init (loc, OPT_Wpedantic,
		  "initializer element is not a constant expression");

  /* Issue -Wc++-compat warnings about initializing a bitfield with
     enum type.  */
  if (warn_cxx_compat
      && field != NULL_TREE
      && TREE_CODE (field) == FIELD_DECL
      && DECL_BIT_FIELD_TYPE (field) != NULL_TREE
      && (TYPE_MAIN_VARIANT (DECL_BIT_FIELD_TYPE (field))
	  != TYPE_MAIN_VARIANT (type))
      && TREE_CODE (DECL_BIT_FIELD_TYPE (field)) == ENUMERAL_TYPE)
    {
      tree checktype = origtype != NULL_TREE ? origtype : TREE_TYPE (value);
      if (checktype != error_mark_node
	  && (TYPE_MAIN_VARIANT (checktype)
	      != TYPE_MAIN_VARIANT (DECL_BIT_FIELD_TYPE (field))))
	warning_init (loc, OPT_Wc___compat,
		      "enum conversion in initialization is invalid in C++");
    }

  /* If this field is empty and does not have side effects (and is not at
     the end of structure), don't do anything other than checking the
     initializer.  */
  if (field
      && (TREE_TYPE (field) == error_mark_node
	  || (COMPLETE_TYPE_P (TREE_TYPE (field))
	      && integer_zerop (TYPE_SIZE (TREE_TYPE (field)))
	      && !TREE_SIDE_EFFECTS (new_value)
	      && (TREE_CODE (constructor_type) == ARRAY_TYPE
		  || DECL_CHAIN (field)))))
    return;

  /* Finally, set VALUE to the initializer value digested above.  */
  value = new_value;

  /* If this element doesn't come next in sequence,
     put it on constructor_pending_elts.  */
  if (TREE_CODE (constructor_type) == ARRAY_TYPE
      && (!constructor_incremental
	  || !tree_int_cst_equal (field, constructor_unfilled_index)))
    {
      if (constructor_incremental
	  && tree_int_cst_lt (field, constructor_unfilled_index))
	set_nonincremental_init (braced_init_obstack);

      add_pending_init (loc, field, value, origtype, implicit,
			braced_init_obstack);
      return;
    }
  else if (TREE_CODE (constructor_type) == RECORD_TYPE
	   && (!constructor_incremental
	       || field != constructor_unfilled_fields))
    {
      /* We do this for records but not for unions.  In a union,
	 no matter which field is specified, it can be initialized
	 right away since it starts at the beginning of the union.  */
      if (constructor_incremental)
	{
	  if (!constructor_unfilled_fields)
	    set_nonincremental_init (braced_init_obstack);
	  else
	    {
	      tree bitpos, unfillpos;

	      bitpos = bit_position (field);
	      unfillpos = bit_position (constructor_unfilled_fields);

	      if (tree_int_cst_lt (bitpos, unfillpos))
		set_nonincremental_init (braced_init_obstack);
	    }
	}

      add_pending_init (loc, field, value, origtype, implicit,
			braced_init_obstack);
      return;
    }
  else if (TREE_CODE (constructor_type) == UNION_TYPE
	   && !vec_safe_is_empty (constructor_elements))
    {
      if (!implicit)
	{
	  if (TREE_SIDE_EFFECTS (constructor_elements->last ().value))
	    warning_init (loc, OPT_Woverride_init_side_effects,
			  "initialized field with side-effects overwritten");
	  else if (warn_override_init)
	    warning_init (loc, OPT_Woverride_init,
			  "initialized field overwritten");
	}

      /* We can have just one union field set.  */
      constructor_elements = NULL;
    }

  /* Otherwise, output this element either to
     constructor_elements or to the assembler file.  */

  constructor_elt celt = {field, value};
  vec_safe_push (constructor_elements, celt);

  /* Advance the variable that indicates sequential elements output.  */
  if (TREE_CODE (constructor_type) == ARRAY_TYPE)
    constructor_unfilled_index
      = size_binop_loc (input_location, PLUS_EXPR, constructor_unfilled_index,
			bitsize_one_node);
  else if (TREE_CODE (constructor_type) == RECORD_TYPE)
    {
      constructor_unfilled_fields
	= DECL_CHAIN (constructor_unfilled_fields);

      /* Skip any nameless bit fields.  */
      while (constructor_unfilled_fields != NULL_TREE
	     && DECL_UNNAMED_BIT_FIELD (constructor_unfilled_fields))
	constructor_unfilled_fields =
	  DECL_CHAIN (constructor_unfilled_fields);
    }
  else if (TREE_CODE (constructor_type) == UNION_TYPE)
    constructor_unfilled_fields = NULL_TREE;

  /* Now output any pending elements which have become next.  */
  if (pending)
    output_pending_init_elements (0, braced_init_obstack);
}

/* For two FIELD_DECLs in the same chain, return -1 if field1
   comes before field2, 1 if field1 comes after field2 and
   0 if field1 == field2.  */

static int init_field_decl_cmp (tree field1, tree field2)
{
  if (field1 == field2)
    return 0;

  tree bitpos1 = bit_position (field1);
  tree bitpos2 = bit_position (field2);
  if (tree_int_cst_equal (bitpos1, bitpos2))
    {
      /* If one of the fields has non-zero bitsize, then that
	 field must be the last one in a sequence of zero
	 sized fields, fields after it will have bigger
	 bit_position.  */
      if (TREE_TYPE (field1) != error_mark_node
	  && COMPLETE_TYPE_P (TREE_TYPE (field1))
	  && integer_nonzerop (TREE_TYPE (field1)))
	return 1;
      if (TREE_TYPE (field2) != error_mark_node
	  && COMPLETE_TYPE_P (TREE_TYPE (field2))
	  && integer_nonzerop (TREE_TYPE (field2)))
	return -1;
      /* Otherwise, fallback to DECL_CHAIN walk to find out
	 which field comes earlier.  Walk chains of both
	 fields, so that if field1 and field2 are close to each
	 other in either order, it is found soon even for large
	 sequences of zero sized fields.  */
      tree f1 = field1, f2 = field2;
      while (1)
	{
	  f1 = DECL_CHAIN (f1);
	  f2 = DECL_CHAIN (f2);
	  if (f1 == NULL_TREE)
	    {
	      gcc_assert (f2);
	      return 1;
	    }
	  if (f2 == NULL_TREE)
	    return -1;
	  if (f1 == field2)
	    return -1;
	  if (f2 == field1)
	    return 1;
	  if (!tree_int_cst_equal (bit_position (f1), bitpos1))
	    return 1;
	  if (!tree_int_cst_equal (bit_position (f2), bitpos1))
	    return -1;
	}
    }
  else if (tree_int_cst_lt (bitpos1, bitpos2))
    return -1;
  else
    return 1;
}

/* Output any pending elements which have become next.
   As we output elements, constructor_unfilled_{fields,index}
   advances, which may cause other elements to become next;
   if so, they too are output.

   If ALL is 0, we return when there are
   no more pending elements to output now.

   If ALL is 1, we output space as necessary so that
   we can output all the pending elements.  */
static void output_pending_init_elements (int all, struct obstack * braced_init_obstack)
{
  struct init_node *elt = constructor_pending_elts;
  tree next;

 retry:

  /* Look through the whole pending tree.
     If we find an element that should be output now,
     output it.  Otherwise, set NEXT to the element
     that comes first among those still pending.  */

  next = NULL_TREE;
  while (elt)
    {
      if (TREE_CODE (constructor_type) == ARRAY_TYPE)
	{
	  if (tree_int_cst_equal (elt->purpose,
				  constructor_unfilled_index))
	    output_init_element (input_location, elt->value, elt->origtype,
				 true, TREE_TYPE (constructor_type),
				 constructor_unfilled_index, false, false,
				 braced_init_obstack);
	  else if (tree_int_cst_lt (constructor_unfilled_index,
				    elt->purpose))
	    {
	      /* Advance to the next smaller node.  */
	      if (elt->left)
		elt = elt->left;
	      else
		{
		  /* We have reached the smallest node bigger than the
		     current unfilled index.  Fill the space first.  */
		  next = elt->purpose;
		  break;
		}
	    }
	  else
	    {
	      /* Advance to the next bigger node.  */
	      if (elt->right)
		elt = elt->right;
	      else
		{
		  /* We have reached the biggest node in a subtree.  Find
		     the parent of it, which is the next bigger node.  */
		  while (elt->parent && elt->parent->right == elt)
		    elt = elt->parent;
		  elt = elt->parent;
		  if (elt && tree_int_cst_lt (constructor_unfilled_index,
					      elt->purpose))
		    {
		      next = elt->purpose;
		      break;
		    }
		}
	    }
	}
      else if (RECORD_OR_UNION_TYPE_P (constructor_type))
	{
	  /* If the current record is complete we are done.  */
	  if (constructor_unfilled_fields == NULL_TREE)
	    break;

	  int cmp = init_field_decl_cmp (constructor_unfilled_fields,
					 elt->purpose);
	  if (cmp == 0)
	    output_init_element (input_location, elt->value, elt->origtype,
				 true, TREE_TYPE (elt->purpose),
				 elt->purpose, false, false,
				 braced_init_obstack);
	  else if (cmp < 0)
	    {
	      /* Advance to the next smaller node.  */
	      if (elt->left)
		elt = elt->left;
	      else
		{
		  /* We have reached the smallest node bigger than the
		     current unfilled field.  Fill the space first.  */
		  next = elt->purpose;
		  break;
		}
	    }
	  else
	    {
	      /* Advance to the next bigger node.  */
	      if (elt->right)
		elt = elt->right;
	      else
		{
		  /* We have reached the biggest node in a subtree.  Find
		     the parent of it, which is the next bigger node.  */
		  while (elt->parent && elt->parent->right == elt)
		    elt = elt->parent;
		  elt = elt->parent;
		  if (elt
		      && init_field_decl_cmp (constructor_unfilled_fields,
					      elt->purpose) < 0)
		    {
		      next = elt->purpose;
		      break;
		    }
		}
	    }
	}
    }

  /* Ordinarily return, but not if we want to output all
     and there are elements left.  */
  if (!(all && next != NULL_TREE))
    return;

  /* If it's not incremental, just skip over the gap, so that after
     jumping to retry we will output the next successive element.  */
  if (RECORD_OR_UNION_TYPE_P (constructor_type))
    constructor_unfilled_fields = next;
  else if (TREE_CODE (constructor_type) == ARRAY_TYPE)
    constructor_unfilled_index = next;

  /* ELT now points to the node in the pending tree with the next
     initializer to output.  */
  goto retry;
}

/* Begin and end compound statements.  This is as simple as pushing
   and popping new statement lists from the tree.  */

tree aet_c_begin_compound_stmt (bool do_scope)
{
  tree stmt = push_stmt_list ();
  if (do_scope)
    push_scope ();
  return stmt;
}

/* End a compound statement.  STMT is the statement.  LOC is the
   location of the compound statement-- this is usually the location
   of the opening brace.  */

tree aet_c_end_compound_stmt (location_t loc, tree stmt, bool do_scope)
{
  tree block = NULL;

  if (do_scope)
    {
      if (c_dialect_objc ())
	objc_clear_super_receiver ();
      block = pop_scope ();
    }

  stmt = pop_stmt_list (stmt);
  stmt = c_build_bind_expr (loc, block, stmt);

  /* If this compound statement is nested immediately inside a statement
     expression, then force a BIND_EXPR to be created.  Otherwise we'll
     do the wrong thing for ({ { 1; } }) or ({ 1; { } }).  In particular,
     STATEMENT_LISTs merge, and thus we can lose track of what statement
     was really last.  */
  if (building_stmt_list_p ()
      && STATEMENT_LIST_STMT_EXPR (cur_stmt_list)
      && TREE_CODE (stmt) != BIND_EXPR)
    {
      stmt = build3 (BIND_EXPR, void_type_node, NULL, stmt, NULL);
      TREE_SIDE_EFFECTS (stmt) = 1;
      SET_EXPR_LOCATION (stmt, loc);
    }

  return stmt;
}



/* Build a vector comparison of ARG0 and ARG1 using CODE opcode
   into a value of TYPE type.  Comparison is done via VEC_COND_EXPR.  */

static tree build_vec_cmp (tree_code code, tree type,tree arg0, tree arg1)
{
  tree zero_vec = build_zero_cst (type);
  tree minus_one_vec = build_minus_one_cst (type);
  tree cmp_type = truth_type_for (type);
  tree cmp = build2 (code, cmp_type, arg0, arg1);
  return build3 (VEC_COND_EXPR, type, cmp, minus_one_vec, zero_vec);
}

/* Build a binary-operation expression without default conversions.
   CODE is the kind of expression to build.
   LOCATION is the operator's location.
   This function differs from `build' in several ways:
   the data type of the result is computed and recorded in it,
   warnings are generated if arg data types are invalid,
   special handling for addition and subtraction of pointers is known,
   and some optimization is done (operations on narrow ints
   are done in the narrower type when that gives the same result).
   Constant folding is also done before the result is returned.

   Note that the operands will never have enumeral types, or function
   or array types, because either they will have the default conversions
   performed or they have both just been converted to some other type in which
   the arithmetic is to be done.  */

tree aet_build_binary_op (location_t location, enum tree_code code,tree orig_op0, tree orig_op1, bool convert_p)
{
  tree type0, type1, orig_type0, orig_type1;
  tree eptype;
  enum tree_code code0, code1;
  tree op0, op1;
  tree ret = error_mark_node;
  const char *invalid_op_diag;
  bool op0_int_operands, op1_int_operands;
  bool int_const, int_const_or_overflow, int_operands;

  /* Expression code to give to the expression when it is built.
     Normally this is CODE, which is what the caller asked for,
     but in some special cases we change it.  */
  enum tree_code resultcode = code;

  /* Data type in which the computation is to be performed.
     In the simplest cases this is the common type of the arguments.  */
  tree result_type = NULL;

  /* When the computation is in excess precision, the type of the
     final EXCESS_PRECISION_EXPR.  */
  tree semantic_result_type = NULL;

  /* Nonzero means operands have already been type-converted
     in whatever way is necessary.
     Zero means they need to be converted to RESULT_TYPE.  */
  int converted = 0;

  /* Nonzero means create the expression with this type, rather than
     RESULT_TYPE.  */
  tree build_type = NULL_TREE;

  /* Nonzero means after finally constructing the expression
     convert it to this type.  */
  tree final_type = NULL_TREE;

  /* Nonzero if this is an operation like MIN or MAX which can
     safely be computed in short if both args are promoted shorts.
     Also implies COMMON.
     -1 indicates a bitwise operation; this makes a difference
     in the exact conditions for when it is safe to do the operation
     in a narrower mode.  */
  int shorten = 0;

  /* Nonzero if this is a comparison operation;
     if both args are promoted shorts, compare the original shorts.
     Also implies COMMON.  */
  int short_compare = 0;

  /* Nonzero if this is a right-shift operation, which can be computed on the
     original short and then promoted if the operand is a promoted short.  */
  int short_shift = 0;

  /* Nonzero means set RESULT_TYPE to the common type of the args.  */
  int common = 0;

  /* True means types are compatible as far as ObjC is concerned.  */
  bool objc_ok;

  /* True means this is an arithmetic operation that may need excess
     precision.  */
  bool may_need_excess_precision;

  /* True means this is a boolean operation that converts both its
     operands to truth-values.  */
  bool boolean_op = false;

  /* Remember whether we're doing / or %.  */
  bool doing_div_or_mod = false;

  /* Remember whether we're doing << or >>.  */
  bool doing_shift = false;

  /* Tree holding instrumentation expression.  */
  tree instrument_expr = NULL;

  if (location == UNKNOWN_LOCATION)
    location = input_location;

  op0 = orig_op0;
  op1 = orig_op1;

  op0_int_operands = EXPR_INT_CONST_OPERANDS (orig_op0);
  if (op0_int_operands)
    op0 = remove_c_maybe_const_expr (op0);
  op1_int_operands = EXPR_INT_CONST_OPERANDS (orig_op1);
  if (op1_int_operands)
    op1 = remove_c_maybe_const_expr (op1);
  int_operands = (op0_int_operands && op1_int_operands);
  if (int_operands)
    {
      int_const_or_overflow = (TREE_CODE (orig_op0) == INTEGER_CST
			       && TREE_CODE (orig_op1) == INTEGER_CST);
      int_const = (int_const_or_overflow
		   && !TREE_OVERFLOW (orig_op0)
		   && !TREE_OVERFLOW (orig_op1));
    }
  else
    int_const = int_const_or_overflow = false;

  /* Do not apply default conversion in mixed vector/scalar expression.  */
  if (convert_p
      && VECTOR_TYPE_P (TREE_TYPE (op0)) == VECTOR_TYPE_P (TREE_TYPE (op1)))
    {
      op0 = aet_default_conversion (op0);
      op1 = aet_default_conversion (op1);
    }

  orig_type0 = type0 = TREE_TYPE (op0);

  orig_type1 = type1 = TREE_TYPE (op1);

  /* The expression codes of the data types of the arguments tell us
     whether the arguments are integers, floating, pointers, etc.  */
  code0 = TREE_CODE (type0);
  code1 = TREE_CODE (type1);

  /* Strip NON_LVALUE_EXPRs, etc., since we aren't using as an lvalue.  */
  STRIP_TYPE_NOPS (op0);
  STRIP_TYPE_NOPS (op1);

  /* If an error was already reported for one of the arguments,
     avoid reporting another error.  */

  if (code0 == ERROR_MARK || code1 == ERROR_MARK)
    return error_mark_node;

  if (code0 == POINTER_TYPE
      && reject_gcc_builtin (op0, EXPR_LOCATION (orig_op0)))
    return error_mark_node;

  if (code1 == POINTER_TYPE
      && reject_gcc_builtin (op1, EXPR_LOCATION (orig_op1)))
    return error_mark_node;

  if ((invalid_op_diag
       = targetm.invalid_binary_op (code, type0, type1)))
    {
      error_at (location, invalid_op_diag);
      return error_mark_node;
    }

  switch (code)
    {
    case PLUS_EXPR:
    case MINUS_EXPR:
    case MULT_EXPR:
    case TRUNC_DIV_EXPR:
    case CEIL_DIV_EXPR:
    case FLOOR_DIV_EXPR:
    case ROUND_DIV_EXPR:
    case EXACT_DIV_EXPR:
      may_need_excess_precision = true;
      break;

    case EQ_EXPR:
    case NE_EXPR:
    case LE_EXPR:
    case GE_EXPR:
    case LT_EXPR:
    case GT_EXPR:
      /* Excess precision for implicit conversions of integers to
	 floating point in C11 and later.  */
      may_need_excess_precision = (flag_isoc11
				   && (ANY_INTEGRAL_TYPE_P (type0)
				       || ANY_INTEGRAL_TYPE_P (type1)));
      break;

    default:
      may_need_excess_precision = false;
      break;
    }
  if (TREE_CODE (op0) == EXCESS_PRECISION_EXPR)
    {
      op0 = TREE_OPERAND (op0, 0);
      type0 = TREE_TYPE (op0);
    }
  else if (may_need_excess_precision
	   && (eptype = excess_precision_type (type0)) != NULL_TREE)
    {
      type0 = eptype;
      op0 = aet_convert (eptype, op0);
    }
  if (TREE_CODE (op1) == EXCESS_PRECISION_EXPR)
    {
      op1 = TREE_OPERAND (op1, 0);
      type1 = TREE_TYPE (op1);
    }
  else if (may_need_excess_precision
	   && (eptype = excess_precision_type (type1)) != NULL_TREE)
    {
      type1 = eptype;
      op1 = aet_convert (eptype, op1);
    }

  objc_ok = objc_compare_types (type0, type1, -3, NULL_TREE);

  /* In case when one of the operands of the binary operation is
     a vector and another is a scalar -- convert scalar to vector.  */
  if ((gnu_vector_type_p (type0) && code1 != VECTOR_TYPE)
      || (gnu_vector_type_p (type1) && code0 != VECTOR_TYPE))
    {
      enum stv_conv convert_flag = scalar_to_vector (location, code, op0, op1,
						     true);

      switch (convert_flag)
	{
	  case stv_error:
	    return error_mark_node;
	  case stv_firstarg:
	    {
              bool maybe_const = true;
              tree sc;
              sc = c_fully_fold (op0, false, &maybe_const);
              sc = save_expr (sc);
              sc = aet_convert (TREE_TYPE (type1), sc);
              op0 = build_vector_from_val (type1, sc);
              if (!maybe_const)
                op0 = c_wrap_maybe_const (op0, true);
              orig_type0 = type0 = TREE_TYPE (op0);
              code0 = TREE_CODE (type0);
              converted = 1;
              break;
	    }
	  case stv_secondarg:
	    {
	      bool maybe_const = true;
	      tree sc;
	      sc = c_fully_fold (op1, false, &maybe_const);
	      sc = save_expr (sc);
	      sc = aet_convert (TREE_TYPE (type0), sc);
	      op1 = build_vector_from_val (type0, sc);
	      if (!maybe_const)
		op1 = c_wrap_maybe_const (op1, true);
	      orig_type1 = type1 = TREE_TYPE (op1);
	      code1 = TREE_CODE (type1);
	      converted = 1;
	      break;
	    }
	  default:
	    break;
	}
    }

  switch (code)
    {
    case PLUS_EXPR:
      /* Handle the pointer + int case.  */
      if (code0 == POINTER_TYPE && code1 == INTEGER_TYPE)
	{
	  ret = pointer_int_sum (location, PLUS_EXPR, op0, op1);
	  goto return_build_binary_op;
	}
      else if (code1 == POINTER_TYPE && code0 == INTEGER_TYPE)
	{
	  ret = pointer_int_sum (location, PLUS_EXPR, op1, op0);
	  goto return_build_binary_op;
	}
      else
	common = 1;
      break;

    case MINUS_EXPR:
      /* Subtraction of two similar pointers.
	 We must subtract them as integers, then divide by object size.  */
      if (code0 == POINTER_TYPE && code1 == POINTER_TYPE
	  && comp_target_types (location, type0, type1))
	{
	  ret = pointer_diff (location, op0, op1, &instrument_expr);
	  goto return_build_binary_op;
	}
      /* Handle pointer minus int.  Just like pointer plus int.  */
      else if (code0 == POINTER_TYPE && code1 == INTEGER_TYPE)
	{
	  ret = pointer_int_sum (location, MINUS_EXPR, op0, op1);
	  goto return_build_binary_op;
	}
      else
	common = 1;
      break;

    case MULT_EXPR:
      common = 1;
      break;

    case TRUNC_DIV_EXPR:
    case CEIL_DIV_EXPR:
    case FLOOR_DIV_EXPR:
    case ROUND_DIV_EXPR:
    case EXACT_DIV_EXPR:
      doing_div_or_mod = true;
      warn_for_div_by_zero (location, op1);

      if ((code0 == INTEGER_TYPE || code0 == REAL_TYPE
	   || code0 == FIXED_POINT_TYPE
	   || code0 == COMPLEX_TYPE
	   || gnu_vector_type_p (type0))
	  && (code1 == INTEGER_TYPE || code1 == REAL_TYPE
	      || code1 == FIXED_POINT_TYPE
	      || code1 == COMPLEX_TYPE
	      || gnu_vector_type_p (type1)))
	{
	  enum tree_code tcode0 = code0, tcode1 = code1;

	  if (code0 == COMPLEX_TYPE || code0 == VECTOR_TYPE)
	    tcode0 = TREE_CODE (TREE_TYPE (TREE_TYPE (op0)));
	  if (code1 == COMPLEX_TYPE || code1 == VECTOR_TYPE)
	    tcode1 = TREE_CODE (TREE_TYPE (TREE_TYPE (op1)));

	  if (!((tcode0 == INTEGER_TYPE && tcode1 == INTEGER_TYPE)
	      || (tcode0 == FIXED_POINT_TYPE && tcode1 == FIXED_POINT_TYPE)))
	    resultcode = RDIV_EXPR;
	  else
	    /* Although it would be tempting to shorten always here, that
	       loses on some targets, since the modulo instruction is
	       undefined if the quotient can't be represented in the
	       computation mode.  We shorten only if unsigned or if
	       dividing by something we know != -1.  */
	    shorten = (TYPE_UNSIGNED (TREE_TYPE (orig_op0))
		       || (TREE_CODE (op1) == INTEGER_CST
			   && !integer_all_onesp (op1)));
	  common = 1;
	}
      break;

    case BIT_AND_EXPR:
    case BIT_IOR_EXPR:
    case BIT_XOR_EXPR:
      if (code0 == INTEGER_TYPE && code1 == INTEGER_TYPE)
	shorten = -1;
      /* Allow vector types which are not floating point types.   */
      else if (gnu_vector_type_p (type0)
	       && gnu_vector_type_p (type1)
	       && !VECTOR_FLOAT_TYPE_P (type0)
	       && !VECTOR_FLOAT_TYPE_P (type1))
	common = 1;
      break;

    case TRUNC_MOD_EXPR:
    case FLOOR_MOD_EXPR:
      doing_div_or_mod = true;
      warn_for_div_by_zero (location, op1);

      if (gnu_vector_type_p (type0)
	  && gnu_vector_type_p (type1)
	  && TREE_CODE (TREE_TYPE (type0)) == INTEGER_TYPE
	  && TREE_CODE (TREE_TYPE (type1)) == INTEGER_TYPE)
	common = 1;
      else if (code0 == INTEGER_TYPE && code1 == INTEGER_TYPE)
	{
	  /* Although it would be tempting to shorten always here, that loses
	     on some targets, since the modulo instruction is undefined if the
	     quotient can't be represented in the computation mode.  We shorten
	     only if unsigned or if dividing by something we know != -1.  */
	  shorten = (TYPE_UNSIGNED (TREE_TYPE (orig_op0))
		     || (TREE_CODE (op1) == INTEGER_CST
			 && !integer_all_onesp (op1)));
	  common = 1;
	}
      break;

    case TRUTH_ANDIF_EXPR:
    case TRUTH_ORIF_EXPR:
    case TRUTH_AND_EXPR:
    case TRUTH_OR_EXPR:
    case TRUTH_XOR_EXPR:
      if ((code0 == INTEGER_TYPE || code0 == POINTER_TYPE
	   || code0 == REAL_TYPE || code0 == COMPLEX_TYPE
	   || code0 == FIXED_POINT_TYPE)
	  && (code1 == INTEGER_TYPE || code1 == POINTER_TYPE
	      || code1 == REAL_TYPE || code1 == COMPLEX_TYPE
	      || code1 == FIXED_POINT_TYPE))
	{
	  /* Result of these operations is always an int,
	     but that does not mean the operands should be
	     converted to ints!  */
	  result_type = integer_type_node;
	  if (op0_int_operands)
	    {
	      op0 = aet_c_objc_common_truthvalue_conversion (location, orig_op0);
	      op0 = remove_c_maybe_const_expr (op0);
	    }
	  else
	    op0 = aet_c_objc_common_truthvalue_conversion (location, op0);
	  if (op1_int_operands)
	    {
	      op1 = aet_c_objc_common_truthvalue_conversion (location, orig_op1);
	      op1 = remove_c_maybe_const_expr (op1);
	    }
	  else
	    op1 = aet_c_objc_common_truthvalue_conversion (location, op1);
	  converted = 1;
	  boolean_op = true;
	}
      if (code == TRUTH_ANDIF_EXPR)
	{
	  int_const_or_overflow = (int_operands
				   && TREE_CODE (orig_op0) == INTEGER_CST
				   && (op0 == truthvalue_false_node
				       || TREE_CODE (orig_op1) == INTEGER_CST));
	  int_const = (int_const_or_overflow
		       && !TREE_OVERFLOW (orig_op0)
		       && (op0 == truthvalue_false_node
			   || !TREE_OVERFLOW (orig_op1)));
	}
      else if (code == TRUTH_ORIF_EXPR)
	{
	  int_const_or_overflow = (int_operands
				   && TREE_CODE (orig_op0) == INTEGER_CST
				   && (op0 == truthvalue_true_node
				       || TREE_CODE (orig_op1) == INTEGER_CST));
	  int_const = (int_const_or_overflow
		       && !TREE_OVERFLOW (orig_op0)
		       && (op0 == truthvalue_true_node
			   || !TREE_OVERFLOW (orig_op1)));
	}
      break;

      /* Shift operations: result has same type as first operand;
	 always convert second operand to int.
	 Also set SHORT_SHIFT if shifting rightward.  */

    case RSHIFT_EXPR:
      if (gnu_vector_type_p (type0)
	  && gnu_vector_type_p (type1)
	  && TREE_CODE (TREE_TYPE (type0)) == INTEGER_TYPE
	  && TREE_CODE (TREE_TYPE (type1)) == INTEGER_TYPE
	  && known_eq (TYPE_VECTOR_SUBPARTS (type0),
		       TYPE_VECTOR_SUBPARTS (type1)))
	{
	  result_type = type0;
	  converted = 1;
	}
      else if ((code0 == INTEGER_TYPE || code0 == FIXED_POINT_TYPE
		|| (gnu_vector_type_p (type0)
		    && TREE_CODE (TREE_TYPE (type0)) == INTEGER_TYPE))
	       && code1 == INTEGER_TYPE)
	{
	  doing_shift = true;
	  if (TREE_CODE (op1) == INTEGER_CST)
	    {
	      if (tree_int_cst_sgn (op1) < 0)
		{
		  int_const = false;
		  if (c_inhibit_evaluation_warnings == 0)
		    warning_at (location, OPT_Wshift_count_negative,
				"right shift count is negative");
		}
	      else if (code0 == VECTOR_TYPE)
		{
		  if (compare_tree_int (op1,
					TYPE_PRECISION (TREE_TYPE (type0)))
		      >= 0)
		    {
		      int_const = false;
		      if (c_inhibit_evaluation_warnings == 0)
			warning_at (location, OPT_Wshift_count_overflow,
				    "right shift count >= width of vector element");
		    }
		}
	      else
		{
		  if (!integer_zerop (op1))
		    short_shift = 1;

		  if (compare_tree_int (op1, TYPE_PRECISION (type0)) >= 0)
		    {
		      int_const = false;
		      if (c_inhibit_evaluation_warnings == 0)
			warning_at (location, OPT_Wshift_count_overflow,
				    "right shift count >= width of type");
		    }
		}
	    }

	  /* Use the type of the value to be shifted.  */
	  result_type = type0;
	  /* Avoid converting op1 to result_type later.  */
	  converted = 1;
	}
      break;

    case LSHIFT_EXPR:
      if (gnu_vector_type_p (type0)
	  && gnu_vector_type_p (type1)
	  && TREE_CODE (TREE_TYPE (type0)) == INTEGER_TYPE
	  && TREE_CODE (TREE_TYPE (type1)) == INTEGER_TYPE
	  && known_eq (TYPE_VECTOR_SUBPARTS (type0),
		       TYPE_VECTOR_SUBPARTS (type1)))
	{
	  result_type = type0;
	  converted = 1;
	}
      else if ((code0 == INTEGER_TYPE || code0 == FIXED_POINT_TYPE
		|| (gnu_vector_type_p (type0)
		    && TREE_CODE (TREE_TYPE (type0)) == INTEGER_TYPE))
	       && code1 == INTEGER_TYPE)
	{
	  doing_shift = true;
	  if (TREE_CODE (op0) == INTEGER_CST
	      && tree_int_cst_sgn (op0) < 0)
	    {
	      /* Don't reject a left shift of a negative value in a context
		 where a constant expression is needed in C90.  */
	      if (flag_isoc99)
		int_const = false;
	      if (c_inhibit_evaluation_warnings == 0)
		warning_at (location, OPT_Wshift_negative_value,
			    "left shift of negative value");
	    }
	  if (TREE_CODE (op1) == INTEGER_CST)
	    {
	      if (tree_int_cst_sgn (op1) < 0)
		{
		  int_const = false;
		  if (c_inhibit_evaluation_warnings == 0)
		    warning_at (location, OPT_Wshift_count_negative,
				"left shift count is negative");
		}
	      else if (code0 == VECTOR_TYPE)
		{
		  if (compare_tree_int (op1,
					TYPE_PRECISION (TREE_TYPE (type0)))
		      >= 0)
		    {
		      int_const = false;
		      if (c_inhibit_evaluation_warnings == 0)
			warning_at (location, OPT_Wshift_count_overflow,
				    "left shift count >= width of vector element");
		    }
		}
	      else if (compare_tree_int (op1, TYPE_PRECISION (type0)) >= 0)
		{
		  int_const = false;
		  if (c_inhibit_evaluation_warnings == 0)
		    warning_at (location, OPT_Wshift_count_overflow,
				"left shift count >= width of type");
		}
	      else if (TREE_CODE (op0) == INTEGER_CST
		       && maybe_warn_shift_overflow (location, op0, op1)
		       && flag_isoc99)
		int_const = false;
	    }

	  /* Use the type of the value to be shifted.  */
	  result_type = type0;
	  /* Avoid converting op1 to result_type later.  */
	  converted = 1;
	}
      break;

    case EQ_EXPR:
    case NE_EXPR:
      if (gnu_vector_type_p (type0) && gnu_vector_type_p (type1))
        {
          tree intt;
	  if (!vector_types_compatible_elements_p (type0, type1))
            {
              error_at (location, "comparing vectors with different "
                                  "element types");
              return error_mark_node;
            }

	  if (maybe_ne (TYPE_VECTOR_SUBPARTS (type0),
			TYPE_VECTOR_SUBPARTS (type1)))
            {
              error_at (location, "comparing vectors with different "
                                  "number of elements");
              return error_mark_node;
            }

	  /* It's not precisely specified how the usual arithmetic
	     conversions apply to the vector types.  Here, we use
	     the unsigned type if one of the operands is signed and
	     the other one is unsigned.  */
	  if (TYPE_UNSIGNED (type0) != TYPE_UNSIGNED (type1))
	    {
	      if (!TYPE_UNSIGNED (type0))
		op0 = build1 (VIEW_CONVERT_EXPR, type1, op0);
	      else
		op1 = build1 (VIEW_CONVERT_EXPR, type0, op1);
	      warning_at (location, OPT_Wsign_compare, "comparison between "
			  "types %qT and %qT", type0, type1);
	    }

          /* Always construct signed integer vector type.  */
          intt = c_common_type_for_size (GET_MODE_BITSIZE
					 (SCALAR_TYPE_MODE
					  (TREE_TYPE (type0))), 0);
	  if (!intt)
	    {
	      error_at (location, "could not find an integer type "
				  "of the same size as %qT",
			TREE_TYPE (type0));
	      return error_mark_node;
	    }
          result_type = build_opaque_vector_type (intt,
						  TYPE_VECTOR_SUBPARTS (type0));
          converted = 1;
	  ret = build_vec_cmp (resultcode, result_type, op0, op1);
	  goto return_build_binary_op;
        }
      if (FLOAT_TYPE_P (type0) || FLOAT_TYPE_P (type1))
	warning_at (location,
		    OPT_Wfloat_equal,
		    "comparing floating-point with %<==%> or %<!=%> is unsafe");
      /* Result of comparison is always int,
	 but don't convert the args to int!  */
      build_type = integer_type_node;
      if ((code0 == INTEGER_TYPE || code0 == REAL_TYPE
	   || code0 == FIXED_POINT_TYPE || code0 == COMPLEX_TYPE)
	  && (code1 == INTEGER_TYPE || code1 == REAL_TYPE
	      || code1 == FIXED_POINT_TYPE || code1 == COMPLEX_TYPE))
	short_compare = 1;
      else if (code0 == POINTER_TYPE && null_pointer_constant_p (orig_op1))
	{
	  if (TREE_CODE (op0) == ADDR_EXPR
	      && decl_with_nonnull_addr_p (TREE_OPERAND (op0, 0))
	      && !from_macro_expansion_at (location))
	    {
	      if (code == EQ_EXPR)
		warning_at (location,
			    OPT_Waddress,
			    "the comparison will always evaluate as %<false%> "
			    "for the address of %qD will never be NULL",
			    TREE_OPERAND (op0, 0));
	      else
		warning_at (location,
			    OPT_Waddress,
			    "the comparison will always evaluate as %<true%> "
			    "for the address of %qD will never be NULL",
			    TREE_OPERAND (op0, 0));
	    }
	  result_type = type0;
	}
      else if (code1 == POINTER_TYPE && null_pointer_constant_p (orig_op0))
	{
	  if (TREE_CODE (op1) == ADDR_EXPR
	      && decl_with_nonnull_addr_p (TREE_OPERAND (op1, 0))
	      && !from_macro_expansion_at (location))
	    {
	      if (code == EQ_EXPR)
		warning_at (location,
			    OPT_Waddress,
			    "the comparison will always evaluate as %<false%> "
			    "for the address of %qD will never be NULL",
			    TREE_OPERAND (op1, 0));
	      else
		warning_at (location,
			    OPT_Waddress,
			    "the comparison will always evaluate as %<true%> "
			    "for the address of %qD will never be NULL",
			    TREE_OPERAND (op1, 0));
	    }
	  result_type = type1;
	}
      else if (code0 == POINTER_TYPE && code1 == POINTER_TYPE)
	{
	  tree tt0 = TREE_TYPE (type0);
	  tree tt1 = TREE_TYPE (type1);
	  addr_space_t as0 = TYPE_ADDR_SPACE (tt0);
	  addr_space_t as1 = TYPE_ADDR_SPACE (tt1);
	  addr_space_t as_common = ADDR_SPACE_GENERIC;

	  /* Anything compares with void *.  void * compares with anything.
	     Otherwise, the targets must be compatible
	     and both must be object or both incomplete.  */
	  if (comp_target_types (location, type0, type1))
	    result_type = common_pointer_type (type0, type1);
	  else if (!addr_space_superset (as0, as1, &as_common))
	    {
	      error_at (location, "comparison of pointers to "
			"disjoint address spaces");
	      return error_mark_node;
	    }
	  else if (VOID_TYPE_P (tt0) && !TYPE_ATOMIC (tt0))
	    {
	      if (pedantic && TREE_CODE (tt1) == FUNCTION_TYPE)
		pedwarn (location, OPT_Wpedantic, "ISO C forbids "
			 "comparison of %<void *%> with function pointer");
	    }
	  else if (VOID_TYPE_P (tt1) && !TYPE_ATOMIC (tt1))
	    {
	      if (pedantic && TREE_CODE (tt0) == FUNCTION_TYPE)
		pedwarn (location, OPT_Wpedantic, "ISO C forbids "
			 "comparison of %<void *%> with function pointer");
	    }
	  else
	    /* Avoid warning about the volatile ObjC EH puts on decls.  */
	    if (!objc_ok)
	      pedwarn (location, 0,
		       "comparison of distinct pointer types lacks a cast");

	  if (result_type == NULL_TREE)
	    {
	      int qual = ENCODE_QUAL_ADDR_SPACE (as_common);
	      result_type = build_pointer_type
			      (build_qualified_type (void_type_node, qual));
	    }
	}
      else if (code0 == POINTER_TYPE && code1 == INTEGER_TYPE)
	{
	  result_type = type0;
	  pedwarn (location, 0, "comparison between pointer and integer");
	}
      else if (code0 == INTEGER_TYPE && code1 == POINTER_TYPE)
	{
	  result_type = type1;
	  pedwarn (location, 0, "comparison between pointer and integer");
	}
      if ((TREE_CODE (TREE_TYPE (orig_op0)) == BOOLEAN_TYPE
	   || truth_value_p (TREE_CODE (orig_op0)))
	  ^ (TREE_CODE (TREE_TYPE (orig_op1)) == BOOLEAN_TYPE
	     || truth_value_p (TREE_CODE (orig_op1))))
	maybe_warn_bool_compare (location, code, orig_op0, orig_op1);
      break;

    case LE_EXPR:
    case GE_EXPR:
    case LT_EXPR:
    case GT_EXPR:
      if (gnu_vector_type_p (type0) && gnu_vector_type_p (type1))
        {
          tree intt;
	  if (!vector_types_compatible_elements_p (type0, type1))
            {
              error_at (location, "comparing vectors with different "
                                  "element types");
              return error_mark_node;
            }

	  if (maybe_ne (TYPE_VECTOR_SUBPARTS (type0),
			TYPE_VECTOR_SUBPARTS (type1)))
            {
              error_at (location, "comparing vectors with different "
                                  "number of elements");
              return error_mark_node;
            }

	  /* It's not precisely specified how the usual arithmetic
	     conversions apply to the vector types.  Here, we use
	     the unsigned type if one of the operands is signed and
	     the other one is unsigned.  */
	  if (TYPE_UNSIGNED (type0) != TYPE_UNSIGNED (type1))
	    {
	      if (!TYPE_UNSIGNED (type0))
		op0 = build1 (VIEW_CONVERT_EXPR, type1, op0);
	      else
		op1 = build1 (VIEW_CONVERT_EXPR, type0, op1);
	      warning_at (location, OPT_Wsign_compare, "comparison between "
			  "types %qT and %qT", type0, type1);
	    }

          /* Always construct signed integer vector type.  */
          intt = c_common_type_for_size (GET_MODE_BITSIZE
					 (SCALAR_TYPE_MODE
					  (TREE_TYPE (type0))), 0);
	  if (!intt)
	    {
	      error_at (location, "could not find an integer type "
				  "of the same size as %qT",
			TREE_TYPE (type0));
	      return error_mark_node;
	    }
          result_type = build_opaque_vector_type (intt,
						  TYPE_VECTOR_SUBPARTS (type0));
          converted = 1;
	  ret = build_vec_cmp (resultcode, result_type, op0, op1);
	  goto return_build_binary_op;
        }
      build_type = integer_type_node;
      if ((code0 == INTEGER_TYPE || code0 == REAL_TYPE
	   || code0 == FIXED_POINT_TYPE)
	  && (code1 == INTEGER_TYPE || code1 == REAL_TYPE
	      || code1 == FIXED_POINT_TYPE))
	short_compare = 1;
      else if (code0 == POINTER_TYPE && code1 == POINTER_TYPE)
	{
	  addr_space_t as0 = TYPE_ADDR_SPACE (TREE_TYPE (type0));
	  addr_space_t as1 = TYPE_ADDR_SPACE (TREE_TYPE (type1));
	  addr_space_t as_common;

	  if (comp_target_types (location, type0, type1))
	    {
	      result_type = common_pointer_type (type0, type1);
	      if (!COMPLETE_TYPE_P (TREE_TYPE (type0))
		  != !COMPLETE_TYPE_P (TREE_TYPE (type1)))
		pedwarn (location, 0,
			 "comparison of complete and incomplete pointers");
	      else if (TREE_CODE (TREE_TYPE (type0)) == FUNCTION_TYPE)
		pedwarn (location, OPT_Wpedantic, "ISO C forbids "
			 "ordered comparisons of pointers to functions");
	      else if (null_pointer_constant_p (orig_op0)
		       || null_pointer_constant_p (orig_op1))
		warning_at (location, OPT_Wextra,
			    "ordered comparison of pointer with null pointer");

	    }
	  else if (!addr_space_superset (as0, as1, &as_common))
	    {
	      error_at (location, "comparison of pointers to "
			"disjoint address spaces");
	      return error_mark_node;
	    }
	  else
	    {
	      int qual = ENCODE_QUAL_ADDR_SPACE (as_common);
	      result_type = build_pointer_type
			      (build_qualified_type (void_type_node, qual));
	      pedwarn (location, 0,
		       "comparison of distinct pointer types lacks a cast");
	    }
	}
      else if (code0 == POINTER_TYPE && null_pointer_constant_p (orig_op1))
	{
	  result_type = type0;
	  if (pedantic)
	    pedwarn (location, OPT_Wpedantic,
		     "ordered comparison of pointer with integer zero");
	  else if (extra_warnings)
	    warning_at (location, OPT_Wextra,
			"ordered comparison of pointer with integer zero");
	}
      else if (code1 == POINTER_TYPE && null_pointer_constant_p (orig_op0))
	{
	  result_type = type1;
	  if (pedantic)
	    pedwarn (location, OPT_Wpedantic,
		     "ordered comparison of pointer with integer zero");
	  else if (extra_warnings)
	    warning_at (location, OPT_Wextra,
			"ordered comparison of pointer with integer zero");
	}
      else if (code0 == POINTER_TYPE && code1 == INTEGER_TYPE)
	{
	  result_type = type0;
	  pedwarn (location, 0, "comparison between pointer and integer");
	}
      else if (code0 == INTEGER_TYPE && code1 == POINTER_TYPE)
	{
	  result_type = type1;
	  pedwarn (location, 0, "comparison between pointer and integer");
	}

      if ((code0 == POINTER_TYPE || code1 == POINTER_TYPE)
	  && sanitize_flags_p (SANITIZE_POINTER_COMPARE))
	{
	  op0 = save_expr (op0);
	  op1 = save_expr (op1);

	  tree tt = builtin_decl_explicit (BUILT_IN_ASAN_POINTER_COMPARE);
	  instrument_expr = build_call_expr_loc (location, tt, 2, op0, op1);
	}

      if ((TREE_CODE (TREE_TYPE (orig_op0)) == BOOLEAN_TYPE
	   || truth_value_p (TREE_CODE (orig_op0)))
	  ^ (TREE_CODE (TREE_TYPE (orig_op1)) == BOOLEAN_TYPE
	     || truth_value_p (TREE_CODE (orig_op1))))
	maybe_warn_bool_compare (location, code, orig_op0, orig_op1);
      break;

    default:
      gcc_unreachable ();
    }

  if (code0 == ERROR_MARK || code1 == ERROR_MARK)
    return error_mark_node;

  if (gnu_vector_type_p (type0)
      && gnu_vector_type_p (type1)
      && (!tree_int_cst_equal (TYPE_SIZE (type0), TYPE_SIZE (type1))
	  || !vector_types_compatible_elements_p (type0, type1)))
    {
      gcc_rich_location richloc (location);
      maybe_range_label_for_tree_type_mismatch
	label_for_op0 (orig_op0, orig_op1),
	label_for_op1 (orig_op1, orig_op0);
      richloc.maybe_add_expr (orig_op0, &label_for_op0);
      richloc.maybe_add_expr (orig_op1, &label_for_op1);
      binary_op_error (&richloc, code, type0, type1);
      return error_mark_node;
    }

  if ((code0 == INTEGER_TYPE || code0 == REAL_TYPE || code0 == COMPLEX_TYPE
       || code0 == FIXED_POINT_TYPE
       || gnu_vector_type_p (type0))
      &&
      (code1 == INTEGER_TYPE || code1 == REAL_TYPE || code1 == COMPLEX_TYPE
       || code1 == FIXED_POINT_TYPE
       || gnu_vector_type_p (type1)))
    {
      bool first_complex = (code0 == COMPLEX_TYPE);
      bool second_complex = (code1 == COMPLEX_TYPE);
      int none_complex = (!first_complex && !second_complex);

      if (shorten || common || short_compare)
	{
	  result_type = c_common_type (type0, type1);
	  do_warn_double_promotion (result_type, type0, type1,
				    "implicit conversion from %qT to %qT "
				    "to match other operand of binary "
				    "expression",
				    location);
	  if (result_type == error_mark_node)
	    return error_mark_node;
	}

      if (first_complex != second_complex
	  && (code == PLUS_EXPR
	      || code == MINUS_EXPR
	      || code == MULT_EXPR
	      || (code == TRUNC_DIV_EXPR && first_complex))
	  && TREE_CODE (TREE_TYPE (result_type)) == REAL_TYPE
	  && flag_signed_zeros)
	{
	  /* An operation on mixed real/complex operands must be
	     handled specially, but the language-independent code can
	     more easily optimize the plain complex arithmetic if
	     -fno-signed-zeros.  */
	  tree real_type = TREE_TYPE (result_type);
	  tree real, imag;
	  if (type0 != orig_type0 || type1 != orig_type1)
	    {
	      gcc_assert (may_need_excess_precision && common);
	      semantic_result_type = c_common_type (orig_type0, orig_type1);
	    }
	  if (first_complex)
	    {
	      if (TREE_TYPE (op0) != result_type)
		op0 = convert_and_check (location, result_type, op0);
	      if (TREE_TYPE (op1) != real_type)
		op1 = convert_and_check (location, real_type, op1);
	    }
	  else
	    {
	      if (TREE_TYPE (op0) != real_type)
		op0 = convert_and_check (location, real_type, op0);
	      if (TREE_TYPE (op1) != result_type)
		op1 = convert_and_check (location, result_type, op1);
	    }
	  if (TREE_CODE (op0) == ERROR_MARK || TREE_CODE (op1) == ERROR_MARK)
	    return error_mark_node;
	  if (first_complex)
	    {
	      op0 = save_expr (op0);
	      real = aet_build_unary_op (EXPR_LOCATION (orig_op0), REALPART_EXPR,
				     op0, true);
	      imag = aet_build_unary_op (EXPR_LOCATION (orig_op0), IMAGPART_EXPR,
				     op0, true);
	      switch (code)
		{
		case MULT_EXPR:
		case TRUNC_DIV_EXPR:
		  op1 = save_expr (op1);
		  imag = build2 (resultcode, real_type, imag, op1);
		  /* Fall through.  */
		case PLUS_EXPR:
		case MINUS_EXPR:
		  real = build2 (resultcode, real_type, real, op1);
		  break;
		default:
		  gcc_unreachable();
		}
	    }
	  else
	    {
	      op1 = save_expr (op1);
	      real = aet_build_unary_op (EXPR_LOCATION (orig_op1), REALPART_EXPR,
				     op1, true);
	      imag = aet_build_unary_op (EXPR_LOCATION (orig_op1), IMAGPART_EXPR,
				     op1, true);
	      switch (code)
		{
		case MULT_EXPR:
		  op0 = save_expr (op0);
		  imag = build2 (resultcode, real_type, op0, imag);
		  /* Fall through.  */
		case PLUS_EXPR:
		  real = build2 (resultcode, real_type, op0, real);
		  break;
		case MINUS_EXPR:
		  real = build2 (resultcode, real_type, op0, real);
		  imag = build1 (NEGATE_EXPR, real_type, imag);
		  break;
		default:
		  gcc_unreachable();
		}
	    }
	  ret = build2 (COMPLEX_EXPR, result_type, real, imag);
	  goto return_build_binary_op;
	}

      /* For certain operations (which identify themselves by shorten != 0)
	 if both args were extended from the same smaller type,
	 do the arithmetic in that type and then extend.

	 shorten !=0 and !=1 indicates a bitwise operation.
	 For them, this optimization is safe only if
	 both args are zero-extended or both are sign-extended.
	 Otherwise, we might change the result.
	 Eg, (short)-1 | (unsigned short)-1 is (int)-1
	 but calculated in (unsigned short) it would be (unsigned short)-1.  */

      if (shorten && none_complex)
	{
	  final_type = result_type;
	  result_type = shorten_binary_op (result_type, op0, op1,
					   shorten == -1);
	}

      /* Shifts can be shortened if shifting right.  */

      if (short_shift)
	{
	  int unsigned_arg;
	  tree arg0 = get_narrower (op0, &unsigned_arg);

	  final_type = result_type;

	  if (arg0 == op0 && final_type == TREE_TYPE (op0))
	    unsigned_arg = TYPE_UNSIGNED (TREE_TYPE (op0));

	  if (TYPE_PRECISION (TREE_TYPE (arg0)) < TYPE_PRECISION (result_type)
	      && tree_int_cst_sgn (op1) > 0
	      /* We can shorten only if the shift count is less than the
		 number of bits in the smaller type size.  */
	      && compare_tree_int (op1, TYPE_PRECISION (TREE_TYPE (arg0))) < 0
	      /* We cannot drop an unsigned shift after sign-extension.  */
	      && (!TYPE_UNSIGNED (final_type) || unsigned_arg))
	    {
	      /* Do an unsigned shift if the operand was zero-extended.  */
	      result_type
		= c_common_signed_or_unsigned_type (unsigned_arg,
						    TREE_TYPE (arg0));
	      /* Convert value-to-be-shifted to that type.  */
	      if (TREE_TYPE (op0) != result_type)
		op0 = aet_convert (result_type, op0);
	      converted = 1;
	    }
	}

      /* Comparison operations are shortened too but differently.
	 They identify themselves by setting short_compare = 1.  */

      if (short_compare)
	{
	  /* Don't write &op0, etc., because that would prevent op0
	     from being kept in a register.
	     Instead, make copies of the our local variables and
	     pass the copies by reference, then copy them back afterward.  */
	  tree xop0 = op0, xop1 = op1, xresult_type = result_type;
	  enum tree_code xresultcode = resultcode;
	  tree val
	    = shorten_compare (location, &xop0, &xop1, &xresult_type,
			       &xresultcode);

	  if (val != NULL_TREE)
	    {
	      ret = val;
	      goto return_build_binary_op;
	    }

	  op0 = xop0, op1 = xop1;
	  converted = 1;
	  resultcode = xresultcode;

	  if (c_inhibit_evaluation_warnings == 0)
	    {
	      bool op0_maybe_const = true;
	      bool op1_maybe_const = true;
	      tree orig_op0_folded, orig_op1_folded;

	      if (in_late_binary_op)
		{
		  orig_op0_folded = orig_op0;
		  orig_op1_folded = orig_op1;
		}
	      else
		{
		  /* Fold for the sake of possible warnings, as in
		     aet_build_conditional_expr.  This requires the
		     "original" values to be folded, not just op0 and
		     op1.  */
		  c_inhibit_evaluation_warnings++;
		  op0 = c_fully_fold (op0, require_constant_value,
				      &op0_maybe_const);
		  op1 = c_fully_fold (op1, require_constant_value,
				      &op1_maybe_const);
		  c_inhibit_evaluation_warnings--;
		  orig_op0_folded = c_fully_fold (orig_op0,
						  require_constant_value,
						  NULL);
		  orig_op1_folded = c_fully_fold (orig_op1,
						  require_constant_value,
						  NULL);
		}

	      if (warn_sign_compare)
		warn_for_sign_compare (location, orig_op0_folded,
				       orig_op1_folded, op0, op1,
				       result_type, resultcode);
	      if (!in_late_binary_op && !int_operands)
		{
		  if (!op0_maybe_const || TREE_CODE (op0) != INTEGER_CST)
		    op0 = c_wrap_maybe_const (op0, !op0_maybe_const);
		  if (!op1_maybe_const || TREE_CODE (op1) != INTEGER_CST)
		    op1 = c_wrap_maybe_const (op1, !op1_maybe_const);
		}
	    }
	}
    }

  /* At this point, RESULT_TYPE must be nonzero to avoid an error message.
     If CONVERTED is zero, both args will be converted to type RESULT_TYPE.
     Then the expression will be built.
     It will be given type FINAL_TYPE if that is nonzero;
     otherwise, it will be given type RESULT_TYPE.  */

  if (!result_type)
    {
      /* Favor showing any expression locations that are available. */
      op_location_t oploc (location, UNKNOWN_LOCATION);
      binary_op_rich_location richloc (oploc, orig_op0, orig_op1, true);
      binary_op_error (&richloc, code, TREE_TYPE (op0), TREE_TYPE (op1));
      return error_mark_node;
    }

  if (build_type == NULL_TREE)
    {
      build_type = result_type;
      if ((type0 != orig_type0 || type1 != orig_type1)
	  && !boolean_op)
	{
	  gcc_assert (may_need_excess_precision && common);
	  semantic_result_type = c_common_type (orig_type0, orig_type1);
	}
    }

  if (!converted)
    {
      op0 = ep_convert_and_check (location, result_type, op0,
				  semantic_result_type);
      op1 = ep_convert_and_check (location, result_type, op1,
				  semantic_result_type);

      /* This can happen if one operand has a vector type, and the other
	 has a different type.  */
      if (TREE_CODE (op0) == ERROR_MARK || TREE_CODE (op1) == ERROR_MARK)
	return error_mark_node;
    }

  if (sanitize_flags_p ((SANITIZE_SHIFT
			 | SANITIZE_DIVIDE | SANITIZE_FLOAT_DIVIDE))
      && current_function_decl != NULL_TREE
      && (doing_div_or_mod || doing_shift)
      && !require_constant_value)
    {
      /* OP0 and/or OP1 might have side-effects.  */
      op0 = save_expr (op0);
      op1 = save_expr (op1);
      op0 = c_fully_fold (op0, false, NULL);
      op1 = c_fully_fold (op1, false, NULL);
      if (doing_div_or_mod && (sanitize_flags_p ((SANITIZE_DIVIDE
						  | SANITIZE_FLOAT_DIVIDE))))
	instrument_expr = ubsan_instrument_division (location, op0, op1);
      else if (doing_shift && sanitize_flags_p (SANITIZE_SHIFT))
	instrument_expr = ubsan_instrument_shift (location, code, op0, op1);
    }

  /* Treat expressions in initializers specially as they can't trap.  */
  if (int_const_or_overflow)
    ret = (require_constant_value
	   ? fold_build2_initializer_loc (location, resultcode, build_type,
					  op0, op1)
	   : fold_build2_loc (location, resultcode, build_type, op0, op1));
  else
    ret = build2 (resultcode, build_type, op0, op1);
  if (final_type != NULL_TREE)
    ret = aet_convert (final_type, ret);

 return_build_binary_op:
  gcc_assert (ret != error_mark_node);
  if (TREE_CODE (ret) == INTEGER_CST && !TREE_OVERFLOW (ret) && !int_const)
    ret = (int_operands
	   ? note_integer_operands (ret)
	   : build1 (NOP_EXPR, TREE_TYPE (ret), ret));
  else if (TREE_CODE (ret) != INTEGER_CST && int_operands
	   && !in_late_binary_op)
    ret = note_integer_operands (ret);
  protected_set_expr_location (ret, location);

  if (instrument_expr != NULL)
    ret = fold_build2 (COMPOUND_EXPR, TREE_TYPE (ret),
		       instrument_expr, ret);

  if (semantic_result_type)
    ret = build1_loc (location, EXCESS_PRECISION_EXPR,
		      semantic_result_type, ret);

  return ret;
}


/* Convert EXPR to be a truth-value, validating its type for this
   purpose.  LOCATION is the source location for the expression.  */

tree aet_c_objc_common_truthvalue_conversion (location_t location, tree expr)
{
  bool int_const, int_operands;

  switch (TREE_CODE (TREE_TYPE (expr)))
    {
    case ARRAY_TYPE:
	   argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=used_array_that_cannot_be_converted_to_pointer_where_scalar_is_required;
      return error_mark_node;
    case RECORD_TYPE:
      argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=used_struct_type_value_where_scalar_is_required;
      return error_mark_node;
    case UNION_TYPE:
	  argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=used_union_type_value_where_scalar_is_required;
      return error_mark_node;
    case VOID_TYPE:
	  argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=void_value_not_ignored_as_it_ought_to_be;
      return error_mark_node;
    case POINTER_TYPE:
      if (reject_gcc_builtin (expr))
	       return error_mark_node;
      break;
    case FUNCTION_TYPE:
  	  argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=go_unreachable;
      return error_mark_node;
    case VECTOR_TYPE:
  	  argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=used_vector_type_where_scalar_is_required;
      return error_mark_node;
    default:
      break;
    }

  int_const = (TREE_CODE (expr) == INTEGER_CST && !TREE_OVERFLOW (expr));
  int_operands = EXPR_INT_CONST_OPERANDS (expr);
  if (int_operands && TREE_CODE (expr) != INTEGER_CST)
    {
      expr = remove_c_maybe_const_expr (expr);
      expr = build2 (NE_EXPR, integer_type_node, expr,
		     aet_convert (TREE_TYPE (expr), integer_zero_node));
      expr = note_integer_operands (expr);
    }
  else
    /* ??? Should we also give an error for vectors rather than leaving
       those to give errors later?  */
    expr = c_common_truthvalue_conversion (location, expr);

  if (TREE_CODE (expr) == INTEGER_CST && int_operands && !int_const)
    {
      if (TREE_OVERFLOW (expr))
	     return expr;
      else
	     return note_integer_operands (expr);
    }
  if (TREE_CODE (expr) == INTEGER_CST && !int_const)
    return build1 (NOP_EXPR, TREE_TYPE (expr), expr);
  return expr;
}


/* Make a variant type in the proper way for C/C++, propagating qualifiers
   down to the element type of an array.  If ORIG_QUAL_TYPE is not
   NULL, then it should be used as the qualified type
   ORIG_QUAL_INDIRECT levels down in array type derivation (to
   preserve information about the typedef name from which an array
   type was derived).  */

tree aet_c_build_qualified_type (tree type, int type_quals, tree orig_qual_type,size_t orig_qual_indirect)
{
  if (type == error_mark_node)
    return type;

  if (TREE_CODE (type) == ARRAY_TYPE)
    {
      tree t;
      tree element_type = aet_c_build_qualified_type (TREE_TYPE (type),
						  type_quals, orig_qual_type,
						  orig_qual_indirect - 1);

      /* See if we already have an identically qualified type.  */
      if (orig_qual_type && orig_qual_indirect == 0)
	t = orig_qual_type;
      else
	for (t = TYPE_MAIN_VARIANT (type); t; t = TYPE_NEXT_VARIANT (t))
	  {
	    if (TYPE_QUALS (strip_array_types (t)) == type_quals
		&& TYPE_NAME (t) == TYPE_NAME (type)
		&& TYPE_CONTEXT (t) == TYPE_CONTEXT (type)
		&& attribute_list_equal (TYPE_ATTRIBUTES (t),
					 TYPE_ATTRIBUTES (type)))
	      break;
	  }
      if (!t)
	{
          tree domain = TYPE_DOMAIN (type);

	  t = build_variant_type_copy (type);
	  TREE_TYPE (t) = element_type;

          if (TYPE_STRUCTURAL_EQUALITY_P (element_type)
              || (domain && TYPE_STRUCTURAL_EQUALITY_P (domain)))
            SET_TYPE_STRUCTURAL_EQUALITY (t);
          else if (TYPE_CANONICAL (element_type) != element_type
                   || (domain && TYPE_CANONICAL (domain) != domain))
            {
              tree unqualified_canon
                = build_array_type (TYPE_CANONICAL (element_type),
                                    domain? TYPE_CANONICAL (domain)
                                          : NULL_TREE);
              if (TYPE_REVERSE_STORAGE_ORDER (type))
                {
                  unqualified_canon
                    = build_distinct_type_copy (unqualified_canon);
                  TYPE_REVERSE_STORAGE_ORDER (unqualified_canon) = 1;
                }
              TYPE_CANONICAL (t)
                = aet_c_build_qualified_type (unqualified_canon, type_quals);
            }
          else
            TYPE_CANONICAL (t) = t;
	}
      return t;
    }

  /* A restrict-qualified pointer type must be a pointer to object or
     incomplete type.  Note that the use of POINTER_TYPE_P also allows
     REFERENCE_TYPEs, which is appropriate for C++.  */
  if ((type_quals & TYPE_QUAL_RESTRICT)
      && (!POINTER_TYPE_P (type)
	  || !C_TYPE_OBJECT_OR_INCOMPLETE_P (TREE_TYPE (type))))
    {
      error ("invalid use of %<restrict%>");
      type_quals &= ~TYPE_QUAL_RESTRICT;
    }

  tree var_type = (orig_qual_type && orig_qual_indirect == 0
		   ? orig_qual_type
		   : build_qualified_type (type, type_quals));
  /* A variant type does not inherit the list of incomplete vars from the
     type main variant.  */
  if ((RECORD_OR_UNION_TYPE_P (var_type)
       || TREE_CODE (var_type) == ENUMERAL_TYPE)
      && TYPE_MAIN_VARIANT (var_type) != var_type)
    C_TYPE_INCOMPLETE_VARS (var_type) = 0;
  return var_type;
}

//zclei
static bool aet_reject_gcc_builtin (const_tree expr, location_t loc /* = UNKNOWN_LOCATION */)
{
  if (TREE_CODE (expr) == ADDR_EXPR)
    expr = TREE_OPERAND (expr, 0);

  STRIP_ANY_LOCATION_WRAPPER (expr);

  if (TREE_TYPE (expr)
      && TREE_CODE (TREE_TYPE (expr)) == FUNCTION_TYPE
      && TREE_CODE (expr) == FUNCTION_DECL
      /* The intersection of DECL_BUILT_IN and DECL_IS_BUILTIN avoids
	 false positives for user-declared built-ins such as abs or
	 strlen, and for C++ operators new and delete.
	 The c_decl_implicit() test avoids false positives for implicitly
	 declared built-ins with library fallbacks (such as abs).  */
      && fndecl_built_in_p (expr)
      && DECL_IS_BUILTIN (expr)
      && !c_decl_implicit (expr)
      && !DECL_ASSEMBLER_NAME_SET_P (expr))
    {
      if (loc == UNKNOWN_LOCATION)
	loc = EXPR_LOC_OR_LOC (expr, input_location);

      /* Reject arguments that are built-in functions with
	 no library fallback.  */
      //zclei error_at (loc, "built-in function %qE must be directly called", expr);
  	  argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=built_in_function_xx_must_be_directly_called;

      return true;
    }

  return false;
}

static tree lookup_field (tree type, tree component)
{
   tree field;

   /* If TYPE_LANG_SPECIFIC is set, then it is a sorted array of pointers
     to the field elements.  Use a binary search on this array to quickly
     find the element.  Otherwise, do a linear search.  TYPE_LANG_SPECIFIC
     will always be set for structures which have many elements.

     Duplicate field checking replaces duplicates with NULL_TREE so
     TYPE_LANG_SPECIFIC arrays are potentially no longer sorted.  In that
     case just iterate using DECL_CHAIN.  */
  if (TYPE_LANG_SPECIFIC (type) && TYPE_LANG_SPECIFIC (type)->s  && !seen_error ()){
      int bot, top, half;
      tree *field_array = &TYPE_LANG_SPECIFIC (type)->s->elts[0];
      field = TYPE_FIELDS (type);
      bot = 0;
      top = TYPE_LANG_SPECIFIC (type)->s->len;
      while (top - bot > 1){
	     half = (top - bot + 1) >> 1;
	     field = field_array[bot+half];
   	     if (DECL_NAME (field) == NULL_TREE){
	        /* Step through all anon unions in linear fashion.  */
	        while (DECL_NAME (field_array[bot]) == NULL_TREE){
		      field = field_array[bot++];
		      if (RECORD_OR_UNION_TYPE_P (TREE_TYPE (field))) {
		          tree anon = lookup_field (TREE_TYPE (field), component);
		          if (anon)
			         return tree_cons (NULL_TREE, field, anon);

		         /* The Plan 9 compiler permits referring
			      directly to an anonymous struct/union field
			     using a typedef name.  */
		         if (flag_plan9_extensions  && TYPE_NAME (TREE_TYPE (field)) != NULL_TREE  &&
		                (TREE_CODE (TYPE_NAME (TREE_TYPE (field)))== TYPE_DECL)  && (DECL_NAME (TYPE_NAME (TREE_TYPE (field))) == component))
			        break;
		       }
		    }

	         /* Entire record is only anon unions.  */
	        if (bot > top)
		       return NULL_TREE;
	         /* Restart the binary search, with new lower bound.  */
	        continue;
	     }
          printNode(field);
	     if (DECL_NAME (field) == component)
	         break;
	     if (DECL_NAME(field) < component)
	        bot += half;
	     else
	       top = bot + half;
	 }

      if (DECL_NAME (field_array[bot]) == component)
	     field = field_array[bot];
      else if (DECL_NAME (field) != component)
	    return NULL_TREE;
   } else{
      for (field = TYPE_FIELDS (type); field; field = DECL_CHAIN (field)){
	     if (DECL_NAME (field) == NULL_TREE && RECORD_OR_UNION_TYPE_P (TREE_TYPE (field))){
	         tree anon = lookup_field (TREE_TYPE (field), component);
	    	 n_warning("在这里 field 11 不应该走这里  DECL_NAME (field) == NULL_TREE && RECORD_OR_UNION_TYPE_P (TREE_TYPE (field)) %p",anon);
	         if (anon)
		       return tree_cons (NULL_TREE, field, anon);

	      /* The Plan 9 compiler permits referring directly to an
		 anonymous struct/union field using a typedef
		 name.  */
	         if (flag_plan9_extensions  && TYPE_NAME (TREE_TYPE (field)) != NULL_TREE
		        && TREE_CODE (TYPE_NAME (TREE_TYPE (field))) == TYPE_DECL  && (DECL_NAME (TYPE_NAME (TREE_TYPE (field))) == component))
		      break;
	     }
	     if (DECL_NAME (field) == component)
	          break;
	 }

      if (field == NULL_TREE)
	    return NULL_TREE;
   }

  return tree_cons (NULL_TREE, field, NULL_TREE);
}

tree aet_lookup_field (tree type, tree component)
{
  return lookup_field(type,component);
}

//////////////////////////////---------------------------------比较两个函数的参数是符匹配---------------------
/**
 * 右边的参数与左边的比较。如果正确返回 init
 */
tree aet_typeck_func_param_compare(location_t init_loc, tree type, tree init,int require_constant)
{
  bool strict_string=true;
  bool  null_pointer_constant = false;
  if(init)
      null_pointer_constant=null_pointer_constant_p (init);
  enum tree_code code = TREE_CODE (type);
  tree inside_init = init;
  tree semantic_type = NULL_TREE;
  bool maybe_const = true;
  if(type == error_mark_node || !init || error_operand_p (init))
      return error_mark_node;
  STRIP_TYPE_NOPS (inside_init);
  if (TREE_CODE (inside_init) == EXCESS_PRECISION_EXPR){
      semantic_type = TREE_TYPE (inside_init);
      inside_init = TREE_OPERAND (inside_init, 0);
  }


  inside_init = c_fully_fold (inside_init, require_constant, &maybe_const);

  /* Initialization of an array of chars from a string constant
     optionally enclosed in braces.  */

  if (code == ARRAY_TYPE && inside_init  && TREE_CODE (inside_init) == STRING_CST){
      tree typ1= (TYPE_ATOMIC (TREE_TYPE (type))? aet_c_build_qualified_type (TYPE_MAIN_VARIANT (TREE_TYPE (type)),TYPE_QUAL_ATOMIC):
                TYPE_MAIN_VARIANT (TREE_TYPE (type)));

      /* Note that an array could be both an array of character type
         and an array of wchar_t if wchar_t is signed char or unsigned
         char.  */
      bool char_array = (typ1 == char_type_node || typ1 == signed_char_type_node || typ1 == unsigned_char_type_node);
      bool wchar_array = !!aet_comptypes (typ1, wchar_type_node);
      bool char16_array = !!aet_comptypes (typ1, char16_type_node);
      bool char32_array = !!aet_comptypes (typ1, char32_type_node);
      if (char_array || wchar_array || char16_array || char32_array){
          struct c_expr expr;
          tree typ2 = TYPE_MAIN_VARIANT (TREE_TYPE (TREE_TYPE (inside_init)));
          bool incompat_string_cst = false;
          expr.value = inside_init;
          expr.original_code = (strict_string ? STRING_CST : ERROR_MARK);
          expr.original_type = NULL;
          aet_maybe_warn_string_init (init_loc, type, expr);
          if (TYPE_DOMAIN (type) && !TYPE_MAX_VALUE (TYPE_DOMAIN (type)))
              //pedwarn_init (init_loc, OPT_Wpedantic,"initialization of a flexible array member");
              argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=initialization_of_a_flexible_array_member;
          if (aet_comptypes (TYPE_MAIN_VARIANT (TREE_TYPE (inside_init)),TYPE_MAIN_VARIANT (type)))
               return inside_init;

          if (char_array){
              if (typ2 != char_type_node)
                incompat_string_cst = true;
          }else if (!aet_comptypes (typ1, typ2))
             incompat_string_cst = true;
          if (incompat_string_cst){
              //error_init (init_loc, "cannot initialize array of %qT from a string literal with type array of %qT",typ1, typ2);
              argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=cannot_initialize_array_of_$qT_from_a_string_literal_with_type_array_of_$qT;
              return error_mark_node;
          }

          if (TYPE_DOMAIN (type) != NULL_TREE  && TYPE_SIZE (type) != NULL_TREE && TREE_CODE (TYPE_SIZE (type)) == INTEGER_CST){
              unsigned HOST_WIDE_INT len = TREE_STRING_LENGTH (inside_init);
              unsigned unit = TYPE_PRECISION (typ1) / BITS_PER_UNIT;
                  /* Subtract the size of a single (possibly wide) character
                 because it's ok to ignore the terminating null char
                 that is counted in the length of the constant.  */
              if (compare_tree_int (TYPE_SIZE_UNIT (type), len - unit) < 0)
                  //pedwarn_init (init_loc, 0,("initializer-string for array of %qT is too long"), typ1);
                  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=initializer_string_for_array_of_$qT_is_too_long;
              else if (warn_cxx_compat && compare_tree_int (TYPE_SIZE_UNIT (type), len) < 0)
                  //warning_at (init_loc, OPT_Wc___compat,("initializer-string for array of %qT is too long for C++"), typ1);
                  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=initializer_string_for_array_of_$qT_is_too_long_for_CPP;
              if (compare_tree_int (TYPE_SIZE_UNIT (type), len) < 0){
                  unsigned HOST_WIDE_INT size = tree_to_uhwi (TYPE_SIZE_UNIT (type));
                  const char *p = TREE_STRING_POINTER (inside_init);
                  inside_init = build_string (size, p);
              }
          }
          TREE_TYPE (inside_init) = type;
          return inside_init;
      }else if (INTEGRAL_TYPE_P (typ1)){
         //error_init (init_loc, "array of inappropriate type initialized from string constant");
         argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=array_of_inappropriate_type_initialized_from_string_constant;
         return error_mark_node;
      }
  }

  /* Build a VECTOR_CST from a *constant* vector constructor.  If the
     vector constructor is not constant (e.g. {1,2,3,foo()}) then punt
     below and handle as a constructor.  */
  if (code == VECTOR_TYPE && VECTOR_TYPE_P (TREE_TYPE (inside_init))
      && vector_types_convertible_p (TREE_TYPE (inside_init), type, true)  && TREE_CONSTANT (inside_init)){
      if (TREE_CODE (inside_init) == VECTOR_CST && aet_comptypes (TYPE_MAIN_VARIANT (TREE_TYPE (inside_init)),TYPE_MAIN_VARIANT (type)))
         return inside_init;

      if (TREE_CODE (inside_init) == CONSTRUCTOR){
          unsigned HOST_WIDE_INT ix;
          tree value;
          bool constant_p = true;

          /* Iterate through elements and check if all constructor
             elements are *_CSTs.  */
         FOR_EACH_CONSTRUCTOR_VALUE (CONSTRUCTOR_ELTS (inside_init), ix, value)
             if(!CONSTANT_CLASS_P (value)){
                constant_p = false;
                break;
             }

         if (constant_p)
             return build_vector_from_ctor (type, CONSTRUCTOR_ELTS (inside_init));
      }
  }

  if (warn_sequence_point)
    verify_sequence_points (inside_init);


  /* Any type can be initialized
     from an expression of the same type, optionally with braces.  */
  if (inside_init && TREE_TYPE (inside_init) != NULL_TREE
      && (aet_comptypes (TYPE_MAIN_VARIANT (TREE_TYPE (inside_init)),TYPE_MAIN_VARIANT (type))
      || (code == ARRAY_TYPE && aet_comptypes (TREE_TYPE (inside_init), type))
      || (gnu_vector_type_p (type) && aet_comptypes (TREE_TYPE (inside_init), type))
      || (code == POINTER_TYPE  && TREE_CODE (TREE_TYPE (inside_init)) == ARRAY_TYPE
          && aet_comptypes (TREE_TYPE (TREE_TYPE (inside_init)),TREE_TYPE (type))))){

      if (code == POINTER_TYPE){
          if (TREE_CODE (TREE_TYPE (inside_init)) == ARRAY_TYPE){
              if (TREE_CODE (inside_init) == STRING_CST || TREE_CODE (inside_init) == COMPOUND_LITERAL_EXPR)
                 inside_init = array_to_pointer_conversion(init_loc, inside_init);
              else{
                 //error_init (init_loc, "invalid use of non-lvalue array");
                 argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=invalid_use_of_non_lvalue_array;
                 return error_mark_node;
              }
          }
      }

      if (code == VECTOR_TYPE)
        /* Although the types are compatible, we may require a
           conversion.  */
         inside_init = aet_convert (type, inside_init);

      if (require_constant && TREE_CODE (inside_init) == COMPOUND_LITERAL_EXPR){
          /* As an extension, allow initializing objects with static storage
             duration with compound literals (which are then treated just as
             the brace enclosed list they contain).  Also allow this for
             vectors, as we can only assign them with compound literals.  */
          if (flag_isoc99 && code != VECTOR_TYPE)
             //pedwarn_init (init_loc, OPT_Wpedantic, "initializer element is not constant");
             argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=initializer_element_is_not_constant;

          tree decl = COMPOUND_LITERAL_EXPR_DECL (inside_init);
          inside_init = DECL_INITIAL (decl);
      }

      if (code == ARRAY_TYPE && TREE_CODE (inside_init) != STRING_CST  && TREE_CODE (inside_init) != CONSTRUCTOR){
           //error_init (init_loc, "array initialized from non-constant array expression");
           argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=array_initialized_from_non_constant_array_expression;
           return error_mark_node;
      }

      /* Compound expressions can only occur here if -Wpedantic or
     -pedantic-errors is specified.  In the later case, we always want
     an error.  In the former case, we simply want a warning.  */
      if (require_constant && pedantic   && TREE_CODE (inside_init) == COMPOUND_EXPR){
         inside_init = valid_compound_expr_initializer (inside_init,TREE_TYPE (inside_init));
         if (inside_init == error_mark_node)
            //error_init (init_loc, "initializer element is not constant");
            argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=initializer_element_is_not_constant;
         else
            //pedwarn_init (init_loc, OPT_Wpedantic,"initializer element is not constant");
            argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=initializer_element_is_not_constant;

         if (flag_pedantic_errors)
            inside_init = error_mark_node;
      }else if (require_constant && !initializer_constant_valid_p (inside_init, TREE_TYPE (inside_init))){
         //error_init (init_loc, "initializer element is not constant");
         argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=initializer_element_is_not_constant;
         inside_init = error_mark_node;
      }else if (require_constant && !maybe_const)
         //pedwarn_init (init_loc, OPT_Wpedantic,"initializer element is not a constant expression");
         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=initializer_element_is_not_a_constant_expression;

      /* Added to enable additional -Wsuggest-attribute=format warnings.  */
      if (TREE_CODE (TREE_TYPE (inside_init)) == POINTER_TYPE)
         inside_init = convert_for_assignment (init_loc, UNKNOWN_LOCATION,type, inside_init, NULL_TREE/*origtype*/,ic_init, null_pointer_constant,NULL_TREE, NULL_TREE, 0);
      return inside_init;
  }

  /* Handle scalar types, including conversions.  */
  if (code == INTEGER_TYPE || code == REAL_TYPE || code == FIXED_POINT_TYPE
      || code == POINTER_TYPE || code == ENUMERAL_TYPE || code == BOOLEAN_TYPE
      || code == COMPLEX_TYPE || code == VECTOR_TYPE){

      if (TREE_CODE (TREE_TYPE (init)) == ARRAY_TYPE && (TREE_CODE (init) == STRING_CST || TREE_CODE (init) == COMPOUND_LITERAL_EXPR))
           inside_init = init = array_to_pointer_conversion (init_loc, init);
      if (semantic_type)
           inside_init = build1 (EXCESS_PRECISION_EXPR, semantic_type,inside_init);
      inside_init= convert_for_assignment (init_loc, UNKNOWN_LOCATION, type,inside_init, NULL_TREE/*origtype*/, ic_init,null_pointer_constant, NULL_TREE, NULL_TREE,0);

      /* Check to see if we have already given an error message.  */
      if (inside_init == error_mark_node)
         ;
      else if (require_constant && !TREE_CONSTANT (inside_init)){
         //error_init (init_loc, "initializer element is not constant");
         argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=initializer_element_is_not_constant;
         inside_init = error_mark_node;
      }else if (require_constant && !initializer_constant_valid_p (inside_init,TREE_TYPE (inside_init))){
         //error_init (init_loc, "initializer element is not computable at load time");
         argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=initializer_element_is_not_computable_at_load_time;
         inside_init = error_mark_node;
      }else if (require_constant && !maybe_const)
         //pedwarn_init (init_loc, OPT_Wpedantic,"initializer element is not a constant expression");
         argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=initializer_element_is_not_a_constant_expression;


      return inside_init;
  }


   /* Come here only for records and arrays.  */
   if (COMPLETE_TYPE_P (type) && TREE_CODE (TYPE_SIZE (type)) != INTEGER_CST){
      //error_init (init_loc, "variable-sized object may not be initialized");
      argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=variable_sized_object_may_not_be_initialized;
      return error_mark_node;
   }
   //error_init (init_loc, "invalid initializer");
   argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=invalid_initializer;
   return error_mark_node;
}


