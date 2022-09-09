/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

   This program based on gcc/c/c-typeck.c.

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

#include "aet-check.h"

#include "aet-typeck.h"
#include "aet-c-common.h"
#include "c-aet.h"
#include "aet-c-fold.h"
#include "aet-convert.h"
#include "aet-warn.h"


//WarnAndError argsFuncsInfo;
/* Possible cases of implicit bad conversions.  Used to select
   diagnostic messages in convert_for_assignment.  */
enum impl_conv {
  ic_argpass,
  ic_assign,
  ic_init,
  ic_return
};




static bool null_pointer_constant_p (const_tree);
static int tagged_types_tu_compatible_p (const_tree, const_tree, bool *,bool *);
static int comp_target_types (location_t, tree, tree);
static int function_types_compatible_p (const_tree, const_tree, bool *,
					bool *);
static int type_lists_compatible_p (const_tree, const_tree, bool *, bool *);
static tree valid_compound_expr_initializer (tree, tree);
static void readonly_warning (tree, enum lvalue_use);
static int comptypes_internal (const_tree, const_tree, bool *, bool *);

//zclei
static bool aet_reject_gcc_builtin0 (const_tree expr, location_t loc /* = UNKNOWN_LOCATION */);

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

static inline tree remove_c_maybe_const_expr (tree expr)
{
  if (TREE_CODE (expr) == C_MAYBE_CONST_EXPR)
    return C_MAYBE_CONST_EXPR_EXPR (expr);
  else
    return expr;
}

static int lvalue_or_else (location_t loc, const_tree ref, enum lvalue_use use)
{
  int win = aet_lvalue_p (ref);

  if (!win)
    lvalue_error (loc, use);

  return win;
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
  printf("maybe_warn_builtin_no_proto_arg ---www00 promoted:%s argcode:%s %d %d\n",
		  get_tree_code_name(x1c),get_tree_code_name(x2c),x1c,x2c);
  /* Avoid warning for enum arguments that promote to an integer type
     of the same size/mode.  */
  if (parmcode == INTEGER_TYPE && argcode == ENUMERAL_TYPE && TYPE_MODE (parmtype) == TYPE_MODE (argtype))
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
//zclei  if (warning_at (loc, OPT_Wbuiltin_declaration_mismatch,
//		  TYPE_MAIN_VARIANT (promoted) == argtype
//		  ? G_("%qD argument %d type is %qT where %qT is expected "
//		       "in a call to built-in function declared without "
//		       "prototype")
//		  : G_("%qD argument %d promotes to %qT where %qT is expected "
//		       "in a call to built-in function declared without "
//		       "prototype"),
//		  fundecl, parmnum, promoted, parmtype))
//    inform (DECL_SOURCE_LOCATION (fundecl),
//	    "built-in %qD declared here",
//	    fundecl);
     if(TYPE_MAIN_VARIANT (promoted) == argtype){
	    argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=xx_argument_x_type_is_xx_where_xx_is_expected_in_a_call_to_built_in_function_declared_without_prototype;
     }else{
  	    argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=xx_argument_x_promotes_to_xx_where_xx_is_expected_in_a_call_to_built_in_function_declared_without_prototype;
     }
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


//zclei
static bool aet_reject_gcc_builtin0 (const_tree expr, location_t loc /* = UNKNOWN_LOCATION */)
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

static tree aet_not_convert_but_check (location_t loc, tree type, tree expr)
{
  tree result;
  tree expr_for_warning;

  /* Convert from a value with possible excess precision rather than
     via the semantic type, but do not warn about values not fitting
     exactly in the semantic type.  */
  if (TREE_CODE (expr) == EXCESS_PRECISION_EXPR){
	  printf("aet_not_convert_but_check 00\n");

      tree orig_type = TREE_TYPE (expr);
      expr = TREE_OPERAND (expr, 0);
      expr_for_warning = aet_convert (orig_type, expr);
      if (orig_type == type)
	    return expr_for_warning;
  }
  else
    expr_for_warning = expr;

  if (TREE_TYPE (expr) == type)
    return expr;
  printf("aet_not_convert_but_check 11\n");
  result = aet_convert (type, expr);
  if (c_inhibit_evaluation_warnings == 0 && !TREE_OVERFLOW_P (expr) && result != error_mark_node){
	  printf("aet_not_convert_but_check 22 警告转\n");
     aet_warnings_for_convert_and_check (loc, type, expr_for_warning, result);
  }

  return result;
}

static tree check_type_match (tree type,tree rhs, tree origtype,enum impl_conv errtype,bool null_pointer_constant)
{
  enum tree_code codel = TREE_CODE (type);
  tree orig_rhs = rhs;
  tree rhstype;
  enum tree_code coder;
  tree rname = NULL_TREE;
  bool objc_ok = false;
  printf("check_type_match 00 start----\n");
    if (TREE_CODE (rhs) == EXCESS_PRECISION_EXPR)
       rhs = TREE_OPERAND (rhs, 0);

    rhstype = TREE_TYPE (rhs);
    coder = TREE_CODE (rhstype);
    if (coder == ERROR_MARK){
  	   argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=error_or_warn_unknown;
       return error_mark_node;
    }



    printf("check_type_match 11 ----\n");
    tree checktype = origtype != NULL_TREE ? origtype : rhstype;
    if (checktype != error_mark_node && TREE_CODE (type) == ENUMERAL_TYPE && TYPE_MAIN_VARIANT (checktype) != TYPE_MAIN_VARIANT (type)){
	   switch (errtype){
		 case ic_argpass:
			argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=enum_conversion_when_passing_argument_x_of_y_is_invalid_in_Cplusplus;
			break;
		 case ic_assign:
			argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=enum_conversion_from_x_to_y_in_assignment_is_invalid_in_Cplusplus;
			break;
		 case ic_init:
			argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=enum_conversion_from_x_to_y_in_initialization_is_invalid_in_Cplusplus;
			break;
		 case ic_return:
			argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=enum_conversion_from_x_to_y_in_return_is_invalid_in_Cplusplus;
			break;
		 default:
			argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=error_or_warn_unknown;
			return error_mark_node;
	   }
    }

    checktype = origtype != NULL_TREE ? origtype : rhstype;
    if (checktype != error_mark_node && TREE_CODE (checktype) == ENUMERAL_TYPE &&
        		TREE_CODE (type) == ENUMERAL_TYPE && TYPE_MAIN_VARIANT (checktype) != TYPE_MAIN_VARIANT (type)){
	         printf("check_type_match 22 \n");
 			argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=implicit_conversion_from_x_to_y;
    }

    if (TYPE_MAIN_VARIANT (type) == TYPE_MAIN_VARIANT (rhstype)){
    	aet_warn_for_address_or_pointer_of_packed_member (type, orig_rhs);
	    printf("check_type_match 33 ---+++-实参与型参匹配  %s %s  %s\n",
			  get_tree_code_name(TREE_CODE(type)),get_tree_code_name(TREE_CODE(rhstype)),
			  get_tree_code_name(TREE_CODE(TYPE_MAIN_VARIANT (type))));
       return rhs;
    }

    if (coder == VOID_TYPE){
	   printf("check_type_match 44 ----错误 coder == VOID_TYPE\n");
  	   argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=void_value_not_ignored_as_it_ought_to_be;
       return error_mark_node;
    }
    rhs = aet_require_complete_type (UNKNOWN_LOCATION, rhs);
    if (rhs == error_mark_node)
       return error_mark_node;

    if (coder == POINTER_TYPE && aet_reject_gcc_builtin0 (rhs,UNKNOWN_LOCATION))
       return error_mark_node;
    printf("check_type_match 55 \n");

  /* A non-reference type can convert to a reference.  This handles
     va_start, va_copy and possibly port built-ins.  */
    if (codel == REFERENCE_TYPE && coder != REFERENCE_TYPE){
	   printf("check_type_match 66 codel == REFERENCE_TYPE && coder != REFERENCE_TYPE\n");
       if (!aet_lvalue_p (rhs)){
  	      argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=cannot_pass_rvalue_to_reference_parameter;
	      return error_mark_node;
	   }
       if (!aet_c_mark_addressable (rhs))
	      return error_mark_node;
       rhs = build1 (ADDR_EXPR, build_pointer_type (TREE_TYPE (rhs)), rhs);
       SET_EXPR_LOCATION (rhs, 0);
       rhs = check_type_match(build_pointer_type (TREE_TYPE (type)),rhs, origtype,errtype,null_pointer_constant);
       if (rhs == error_mark_node)
	      return error_mark_node;
       return rhs;
    }else if (codel == VECTOR_TYPE && coder == VECTOR_TYPE  && vector_types_convertible_p (type, TREE_TYPE (rhs), true)){
    	  /* Some types can interconvert without explicit casts.  */
    	return rhs;
      /* Arithmetic types all interconvert, and enum is treated like int.  */
    }else if ((codel == INTEGER_TYPE || codel == REAL_TYPE
	    || codel == FIXED_POINT_TYPE
	    || codel == ENUMERAL_TYPE || codel == COMPLEX_TYPE
	    || codel == BOOLEAN_TYPE)
	   && (coder == INTEGER_TYPE || coder == REAL_TYPE
	       || coder == FIXED_POINT_TYPE
	       || coder == ENUMERAL_TYPE || coder == COMPLEX_TYPE
	       || coder == BOOLEAN_TYPE)){
	  printf("check_type_match 77----cccc codel ic_argpass:%d %s %s %s codel:%d coder:%d orig_rhs:%d\n",
			  errtype == ic_argpass,get_tree_code_name(codel),
			  get_tree_code_name(coder),get_tree_code_name(TREE_CODE(orig_rhs)),codel,coder,TREE_CODE(orig_rhs));
	   if((codel== INTEGER_TYPE && coder==REAL_TYPE) || (codel== REAL_TYPE && coder==INTEGER_TYPE)){
	  	    argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=int_to_real_or_real_to_int;
	   }
       if (errtype == ic_argpass){
	      maybe_warn_builtin_no_proto_arg (0, NULL_TREE, 0, type,rhstype);
       }
       bool save = in_late_binary_op;
       if (codel == BOOLEAN_TYPE || codel == COMPLEX_TYPE  || (coder == REAL_TYPE  &&
    		  (codel == INTEGER_TYPE || codel == ENUMERAL_TYPE) && sanitize_flags_p (SANITIZE_FLOAT_CAST)))
	        in_late_binary_op = true;
       tree ret = aet_not_convert_but_check (UNKNOWN_LOCATION , type, orig_rhs);
       //in_late_binary_op = save;
       return rhs;//ret;
    }
    printf("check_type_match 88 \n");

  /* Aggregates in different TUs might need conversion.  */
    if ((codel == RECORD_TYPE || codel == UNION_TYPE) && codel == coder && aet_comptypes (type, rhstype))
       return rhs;//convert_and_check (expr_loc != UNKNOWN_LOCATION? expr_loc : location, type, rhs);
    printf("check_type_match 99 \n");

    /* Conversion to a transparent union or record from its member types.
     This applies only to function arguments.  */
    if (((codel == UNION_TYPE || codel == RECORD_TYPE) && TYPE_TRANSPARENT_AGGR (type)) && errtype == ic_argpass){
 	   printf("check_type_match 100 ((codel == UNION_TYPE || codel == RECORD_TYPE)\n");
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
	         if ((VOID_TYPE_P (ttl) && !TYPE_ATOMIC (ttl)) || (VOID_TYPE_P (ttr) && !TYPE_ATOMIC (ttr)) || comp_target_types (UNKNOWN_LOCATION, memb_type, rhstype)){
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
			  	    argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=error_or_warn_unknown;
		     }else if (TYPE_QUALS_NO_ADDR_SPACE (ttr) & ~TYPE_QUALS_NO_ADDR_SPACE (ttl))
			  	    argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=error_or_warn_unknown;

	         memb = marginal_memb;
	      }//end !memb
	  	  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=ISO_C_prohibits_argument_conversion_to_union_type;
	      return rhs;
	   }// end memb || marginal_memb
    }else if ((codel == POINTER_TYPE || codel == REFERENCE_TYPE) && (coder == codel)){   /* Conversions among pointers */
		  /* If RHS refers to a built-in declared without a prototype
		 BLTIN is the declaration of the built-in with a prototype
		 and RHSTYPE is set to the actual type of the built-in.  */
  	   printf("check_type_match 101 ((codel == UNION_TYPE || codel == RECORD_TYPE)\n");

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
	      tree new_rhs = convert_to_anonymous_field (UNKNOWN_LOCATION, type, rhs);
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
 	    printf("check_type_match 102 !null_pointer_constant_p (rhs)  && asr != asl && !targetm.addr_space.subset_p (asr, asl)\n");
       if (VOID_TYPE_P (ttr) && rhs != null_pointer_node && !VOID_TYPE_P (ttl))
 	  	  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=request_for_implicit_conversion_from_x_to_t_not_permitted_in_Cplusplus;
      /* See if the pointers point to incompatible address spaces.  */
       asl = TYPE_ADDR_SPACE (ttl);
       asr = TYPE_ADDR_SPACE (ttr);
       if (!null_pointer_constant_p (rhs) && asr != asl && !targetm.addr_space.subset_p (asr, asl)){
    	  printf("check_type_match 103 !null_pointer_constant_p (rhs)  && asr != asl && !targetm.addr_space.subset_p (asr, asl)\n");
	      switch (errtype){
	         case ic_argpass:
	        	argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=passing_argument_x_of_y_from_pointer_to_non_enclosed_address_space;
	            break;
	         case ic_assign:
		        argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=assignment_from_pointer_to_non_enclosed_address_space;
		        break;
	         case ic_init:
	           	argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=initialization_from_pointer_to_on_enclosed_address_space;
		        break;
	         case ic_return:
        	   argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=return_from_pointer_to_non_enclosed_address_space;
		        break;
	         default:
	 			argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=error_or_warn_unknown;
	      }
	      return error_mark_node;
	   }
 	   printf("check_type_match 104 !null_pointer_constant_p (rhs)  && asr != asl && !targetm.addr_space.subset_p (asr, asl)\n");

      /* Check if the right-hand side has a format attribute but the
	 left-hand side doesn't.  */
       if (warn_suggest_attribute_format  && check_missing_format_attribute (type, rhstype)){
          printf("check_type_match 105 arn_suggest_attribute_format  && check_missing_format_attribute (type, rhstype)\n");
	      switch (errtype){
	         case ic_argpass:
	   	  	     argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=argument_x_of_y_might_be_a_candidate_for_a_format_attribute;
	            break;
	         case ic_assign:
		   	  	argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=assignment_left_hand_side_might_be_a_candidate_for_a_format_attribute;
	            break;
	         case ic_init:
		   	  	  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=initialization_left_hand_side_might_be_a_candidate_for_a_format_attribute;
	            break;
	         case ic_return:
		   	  	  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=return_type_might_be_a_candidate_for_a_format_attribute;
	            break;
	         default:
		 	   argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=error_or_warn_unknown;
	      }
	   }
	   printf("check_type_match 106 \n");

		  /* Any non-function converts to a [const][volatile] void *
		 and vice versa; otherwise, targets must be the same.
		 Meanwhile, the lhs target must have all the qualifiers of the rhs.  */
       if ((VOID_TYPE_P (ttl) && !TYPE_ATOMIC (ttl))
	     || (VOID_TYPE_P (ttr) && !TYPE_ATOMIC (ttr))
	     || (target_cmp = comp_target_types (UNKNOWN_LOCATION, type, rhstype))
	     || is_opaque_pointer   || ((c_common_unsigned_type (mvl)== c_common_unsigned_type (mvr))
	      && (c_common_signed_type (mvl)== c_common_signed_type (mvr)) && TYPE_ATOMIC (mvl) == TYPE_ATOMIC (mvr))){
	  /* Warn about loss of qualifers from pointers to arrays with
	     qualifiers on the element type. */
    	  printf("check_type_match 107 VOID_TYPE_P (ttl) && !TYPE_ATOMIC (ttl))\n");

	      if (TREE_CODE (ttr) == ARRAY_TYPE){
	         ttr = strip_array_types (ttr);
	         ttl = strip_array_types (ttl);
	         if (TYPE_QUALS_NO_ADDR_SPACE_NO_ATOMIC (ttr) & ~TYPE_QUALS_NO_ADDR_SPACE_NO_ATOMIC (ttl))
		   	  	  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=error_or_warn_unknown;

          }else if (pedantic  && ((VOID_TYPE_P (ttl) && TREE_CODE (ttr) == FUNCTION_TYPE)
		     ||(VOID_TYPE_P (ttr)&& !null_pointer_constant && TREE_CODE (ttl) == FUNCTION_TYPE)))
	   	  	  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=error_or_warn_unknown;

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
		   	  	  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=error_or_warn_unknown;

	         }else if (VOID_TYPE_P (ttl) || VOID_TYPE_P (ttr) || target_cmp) /* If this is not a case of ignoring a mismatch in signedness,no warning.  */
		        ;
	           /* If there is a mismatch, do warn.  */
	         else if (warn_pointer_sign)
		        switch (errtype){
  	               printf("check_type_match 108 \n");
		           case ic_argpass:
				   	  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=pointer_targets_in_passing_argument_x_of_y_differ_in_signedness;
		              break;
		           case ic_assign:
				   	  	  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=pointer_targets_in_assignment_from_x_to_y_differ_in_signedness;
		              break;
		           case ic_init:
				   	  	  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=pointer_targets_in_initialization_of_x_from_y_differ_in_signedness;
		              break;
		           case ic_return:
				   	  	  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=pointer_targets_in_returning_x_from_a_function_with_return_type_y_differ_in_signedness;
		              break;
		           default:
				 	   argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=error_or_warn_unknown;
		        }
	      }else if (TREE_CODE (ttl) == FUNCTION_TYPE && TREE_CODE (ttr) == FUNCTION_TYPE) {
			  /* Because const and volatile on functions are restrictions
			 that say the function will not do certain things,
			 it is okay to use a const or volatile function
			 where an ordinary one is wanted, but not vice-versa.  */
	         if (TYPE_QUALS_NO_ADDR_SPACE (ttl) & ~TYPE_QUALS_NO_ADDR_SPACE (ttr))
		   	  	  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=error_or_warn_unknown;
	      }
	}else if (!objc_ok){/* Avoid warning about the volatile ObjC EH puts on decls.  */
          printf("check_type_match 109 \n");

	   switch (errtype){
	      case ic_argpass:
	   	  	  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=passing_argument_x_of_y_from_incompatible_pointer_type;
	         break;
	      case ic_assign:
	         if (bltin)
		   	  	  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=assignment_to_x_from_pointer_to_y_with_incompatible_type_z;
	         else
		   	  	  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=assignment_to_x_from_incompatible_pointer_type_y;
	         break;
	      case ic_init:
	         if (bltin)
		   	  	  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=initialization_of_x_from_pointer_to_y_with_incompatible_type_z;
	         else
		   	  	  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=initialization_of_x_from_incompatible_pointer_type_y;
	         break;
	      case ic_return:
	         if (bltin)
		   	  	  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=returning_pointer_to_x_of_type_y_from_a_function_with_incompatible_type_z;

	         else
		   	  	  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=returning_x_from_a_function_with_incompatible_return_type_y;
	         break;
	      default:
		 	   argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=error_or_warn_unknown;
	           return error_mark_node;
	    }
	}
       printf("check_type_match 110 \n");

      /* If RHS isn't an address, check pointer or array of packed
	 struct or union.  */
       aet_warn_for_address_or_pointer_of_packed_member (type, orig_rhs);
      return rhs;//aet_convert (type, rhs);
    }else if (codel == POINTER_TYPE && coder == ARRAY_TYPE){
      /* ??? This should not be an error when inlining calls to
	 unprototyped functions.  */
       printf("check_type_match 111 \n");
 	   argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=invalid_use_of_non_lvalue_array;
       return error_mark_node;
    }else if (codel == POINTER_TYPE && coder == INTEGER_TYPE){
      /* An explicit constant 0 can convert to a pointer,
	 or one that results from arithmetic, even including
	 a cast to integer type.  */
       printf("check_type_match 112 \n");
       if (!null_pointer_constant)
	      switch (errtype){
	         case ic_argpass:
	            argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=passing_argument_x_of_y_makes_pointer_from_integer_without_a_cast;
	            break;
	         case ic_assign:
	            //pedwarn (location, OPT_Wint_conversion,"assignment to %qT from %qT makes pointer from integer without a cast", type, rhstype);
	            argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=assignment_to_x_from_y_makes_pointer_from_integer_without_a_cast;
	            break;
	         case ic_init:
	            argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=initialization_of_x_from_y_makes_pointer_from_integer_without_a_cast;
	            break;
	         case ic_return:
	            //pedwarn (location, OPT_Wint_conversion, "returning %qT from a function with return type %qT makes pointer from integer without a cast", rhstype, type);
		        argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=returning_x_from_a_function_with_return_type_y_makes_pointer_from_integer_without_a_cast;
	            break;
	         default:
	        	argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=go_unreachable;
	            return error_mark_node;
	      }
       return rhs;//aet_convert (type, rhs);
    }else if (codel == INTEGER_TYPE && coder == POINTER_TYPE){
        printf("check_type_match 113 \n");
       switch (errtype){
		 case ic_argpass:
			argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=passing_argument_x_of_y_makes_integer_from_pointer_without_a_cast;
			break;
		 case ic_assign:
			argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=assignment_to_x_from_y_makes_integer_from_pointer_without_a_cast;
			break;
		 case ic_init:
			argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=initialization_of_x_from_y_makes_integer_from_pointer_without_a_cast;
			break;
		 case ic_return:
			argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=returning_x_from_a_function_with_return_type_y_makes_integer_from_pointer_without_a_cast;
			break;
		 default:
			argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=go_unreachable;
			return error_mark_node;
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
    printf("check_type_match 114 \n");
    switch (errtype){
       case ic_argpass:
		  argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=incompatible_type_for_argument_x_of_y;
          break;
       case ic_assign:
		  argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=incompatible_types_when_assigning_to_type_x_from_type_y;
	      break;
       case ic_init:
		  argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=incompatible_types_when_initializing_type_x_using_type_y;
	      break;
       case ic_return:
			argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=incompatible_types_when_returning_type_x_but_y_was_expected;
			break;
       default:
			argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=go_unreachable;
    }
    return error_mark_node;
}


int  aet_check_match_expr (tree lhs, tree lhstype,enum tree_code modifycode,tree rhs, tree rhs_origtype)
{
	  tree result;
	  tree newrhs;
	  tree rhseval = NULL_TREE;
	  tree olhstype = lhstype;
	  bool npc;
	  bool is_atomic_op;
	  printf("aet_check_match_expr 00 lhs:%s lhstype:%s rhs:%s %s %s %d\n",get_tree_code_name(TREE_CODE(lhs)),
			  get_tree_code_name(TREE_CODE(lhstype)),get_tree_code_name(TREE_CODE(rhs)), __FILE__,__FUNCTION__,__LINE__);
	  lhs = require_complete_type (0, lhs);
	  /* Avoid duplicate error messages from operands that had errors.  */
	  if (TREE_CODE (lhs) == ERROR_MARK || TREE_CODE (rhs) == ERROR_MARK)
	    return -1;
	  /* Ensure an error for assigning a non-lvalue array to an array in
	     C90.  */
	  if (TREE_CODE (lhstype) == ARRAY_TYPE)
	      return -1;
	  is_atomic_op = false;//really_atomic_lvalue (lhs);
	  newrhs = rhs;
	  if (TREE_CODE (lhs) == C_MAYBE_CONST_EXPR){
		  //由系统处理 0
	      return -1;
	  }

      newrhs = check_type_match (lhstype, rhs,rhs_origtype, ic_assign, false);
      if(5>3)
    	  return 0;


	  /* If a binary op has been requested, combine the old LHS value with the RHS
	     producing the value we should actually store into the LHS.  */

	  if (modifycode != NOP_EXPR){
		  printf("aet_check_match_expr 11 modifycode != NOP_EXPR lhs:%s rhs:%s is_atomic_op:%d %s %s %d\n",get_tree_code_name(TREE_CODE(lhs)),
				  get_tree_code_name(TREE_CODE(rhs)),is_atomic_op,__FILE__,__FUNCTION__,__LINE__);
	      lhs = c_fully_fold (lhs, false, NULL, true);
	      lhs = stabilize_reference (lhs);
	      /* Construct the RHS for any non-atomic compound assignemnt. */
	      if (!is_atomic_op){
		  /* If in LHS op= RHS the RHS has side-effects, ensure they
		     are preevaluated before the rest of the assignment expression's
		     side-effects, because RHS could contain e.g. function calls
		     that modify LHS.  */
		     if (TREE_SIDE_EFFECTS (rhs)){
		        if (TREE_CODE (rhs) == EXCESS_PRECISION_EXPR)
			       newrhs = save_expr (TREE_OPERAND (rhs, 0));
		        else
			       newrhs = save_expr (rhs);
		        rhseval = newrhs;
		        if (TREE_CODE (rhs) == EXCESS_PRECISION_EXPR)
			      newrhs = build1 (EXCESS_PRECISION_EXPR, TREE_TYPE (rhs),newrhs);
		     }
		     newrhs = build_binary_op (0,modifycode, lhs, newrhs, true);

			  /* The original type of the right hand side is no longer
				 meaningful.  */
		    rhs_origtype = NULL_TREE;
		 }
	  }

	  printf("aet_check_match_expr 22 lhs:%s rhs:%s is_atomic_op:%d %s %s %d\n",get_tree_code_name(TREE_CODE(lhs)),
				  get_tree_code_name(TREE_CODE(rhs)),is_atomic_op,__FILE__,__FUNCTION__,__LINE__);
	  /* Give an error for storing in something that is 'const'.  */

	  if (TYPE_READONLY (lhstype) || (RECORD_OR_UNION_TYPE_P (lhstype) && C_TYPE_FIELDS_READONLY (lhstype))){
	      return -1;
	  }else if (TREE_READONLY (lhs)){
	      readonly_warning (lhs, lv_assign);
	  }

	  /* If storing into a structure or union member,
	     it has probably been given type `int'.
	     Compute the type that would go with
	     the actual amount of storage the member occupies.  */
	  printf("aet_check_match_expr 33 lhs:%s lhstype:%s rhs:%s is_atomic_op:%d %s %s %d\n",get_tree_code_name(TREE_CODE(lhs)),
			  get_tree_code_name(TREE_CODE(lhstype)),get_tree_code_name(TREE_CODE(rhs)),is_atomic_op,__FILE__,__FUNCTION__,__LINE__);
	  /* If storing in a field that is in actuality a short or narrower than one,
	     we must store in the field in its actual type.  */

	  if (lhstype != TREE_TYPE (lhs)){
	      lhs = copy_node (lhs);
	      TREE_TYPE (lhs) = lhstype;
		  printf("aet_check_match_expr 44 lhs:%s lhstype:%s rhs:%s is_atomic_op:%d %s %s %d\n",get_tree_code_name(TREE_CODE(lhs)),
				  get_tree_code_name(TREE_CODE(lhstype)),get_tree_code_name(TREE_CODE(rhs)),is_atomic_op,__FILE__,__FUNCTION__,__LINE__);
	  }
	  /* If the lhs is atomic, remove that qualifier.  */
	  if (is_atomic_op){
	      lhstype = build_qualified_type (lhstype,(TYPE_QUALS (lhstype) & ~TYPE_QUAL_ATOMIC));
	      olhstype = build_qualified_type (olhstype, (TYPE_QUALS (lhstype)& ~TYPE_QUAL_ATOMIC));
	  }

	  /* Convert new value to destination type.  Fold it first, then
	     restore any excess precision information, for the sake of
	     conversion warnings.  */
	  bool change=false;
	  if (!(is_atomic_op && modifycode != NOP_EXPR)){
	      tree rhs_semantic_type = NULL_TREE;
	      if (TREE_CODE (newrhs) == EXCESS_PRECISION_EXPR){
		    rhs_semantic_type = TREE_TYPE (newrhs);
		    newrhs = TREE_OPERAND (newrhs, 0);
		  }
	      npc = null_pointer_constant_p (newrhs);
	      newrhs = c_fully_fold (newrhs, false, NULL);
	      if (rhs_semantic_type)
		     newrhs = build1 (EXCESS_PRECISION_EXPR, rhs_semantic_type, newrhs);
		  printf("aet_check_match_expr 55 lhs:%s lhstype:%s rhs:%s is_atomic_op:%d %s %s %d\n",get_tree_code_name(TREE_CODE(lhs)),
				  get_tree_code_name(TREE_CODE(lhstype)),get_tree_code_name(TREE_CODE(rhs)),is_atomic_op,__FILE__,__FUNCTION__,__LINE__);
	      newrhs = check_type_match (lhstype, newrhs,rhs_origtype, ic_assign, npc);
	      change=true;
	      if (TREE_CODE (newrhs) == ERROR_MARK)
		    return -1;
	  }

	  /* Scan operands.  */
	  if (is_atomic_op)
	      result = NULL;//build_atomic_assign (0, lhs, modifycode, newrhs, false);
	  else{
		  printf("aet_check_match_expr 66 lhs:%s lhstype:%s rhs:%s is_atomic_op:%d %s %s %d\n",get_tree_code_name(TREE_CODE(lhs)),
					  get_tree_code_name(TREE_CODE(lhstype)),get_tree_code_name(TREE_CODE(rhs)),is_atomic_op,__FILE__,__FUNCTION__,__LINE__);
	      result = build2 (MODIFY_EXPR, lhstype, lhs, newrhs);
	  }

	  /* If we got the LHS in a different type for storing in,
	     convert the result back to the nominal type of LHS
	     so that the value we return always has the same type
	     as the LHS argument.  */

	  if (newrhs && olhstype == TREE_TYPE (newrhs)){
	      return 0;
	  }
	  if(change)
		  return 0;
	  printf("aet_check_match_expr 77 lhs:%s lhstype:%s rhs:%s is_atomic_op:%d %s %s %d\n",get_tree_code_name(TREE_CODE(lhs)),
					  get_tree_code_name(TREE_CODE(lhstype)),get_tree_code_name(TREE_CODE(rhs)),is_atomic_op,__FILE__,__FUNCTION__,__LINE__);
	  result = check_type_match (olhstype, result,rhs_origtype, ic_assign, false);
	  if (TREE_CODE (result) == ERROR_MARK)
	    return -1;
	  return 0;
}

/**
 * self->x++;
 */
static tree checkUnaryOp(location_t location, enum tree_code code, tree xarg,bool noconvert)
{
	  /* No default_conversion here.  It causes trouble for ADDR_EXPR.  */
	tree arg = xarg;
	tree argtype = NULL_TREE;
	enum tree_code typecode;
	tree val;
	tree ret = error_mark_node;
	tree eptype = NULL_TREE;
	const char *invalid_op_diag;
	bool int_operands;
	printf("checkUnaryOp 00 %s\n",get_tree_code_name(TREE_CODE(arg)));

	int_operands = EXPR_INT_CONST_OPERANDS (xarg);
	if (int_operands)
	   arg = remove_c_maybe_const_expr (arg);
	if (code != ADDR_EXPR)
	   arg = require_complete_type (location, arg);
	  printf("checkUnaryOp 11 %s\n",get_tree_code_name(TREE_CODE(arg)));

	typecode = TREE_CODE (TREE_TYPE (arg));
	if (typecode == ERROR_MARK)
	   return error_mark_node;
	if (typecode == ENUMERAL_TYPE || typecode == BOOLEAN_TYPE)
	   typecode = INTEGER_TYPE;
	  printf("checkUnaryOp 22 typecode: %s\n",get_tree_code_name(typecode));

	if ((invalid_op_diag= targetm.invalid_unary_op (code, TREE_TYPE (xarg)))){
        argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=invalid_op_diag;
	   return error_mark_node;
	}

	if (TREE_CODE (arg) == EXCESS_PRECISION_EXPR){
	   eptype = TREE_TYPE (arg);
	   arg = TREE_OPERAND (arg, 0);
	}
	printf("checkUnaryOp 33 typecode: %s code:%s\n",get_tree_code_name(typecode),get_tree_code_name(code));

	switch (code){
	   case CONVERT_EXPR:
	      /* This is used for unary plus, because a CONVERT_EXPR
		   is enough to prevent anybody from looking inside for
		   associativity, but won't generate any code.  */
	      if (!(typecode == INTEGER_TYPE || typecode == REAL_TYPE|| typecode == FIXED_POINT_TYPE ||
	    		  typecode == COMPLEX_TYPE|| gnu_vector_type_p (TREE_TYPE (arg)))){
	          argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=wrong_type_argument_to_unary_plus;
		     return error_mark_node;
		  }else if (!noconvert)
		     arg = aet_default_conversion (arg);
	      arg = non_lvalue_loc (location, arg);
	      break;
	   case NEGATE_EXPR:
	      if (!(typecode == INTEGER_TYPE || typecode == REAL_TYPE|| typecode == FIXED_POINT_TYPE || typecode == COMPLEX_TYPE
	  	    || gnu_vector_type_p (TREE_TYPE (arg)))){
             argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=wrong_type_argument_to_unary_minus;
	  	     return error_mark_node;
	  	  }else if (!noconvert)
	  	    arg = aet_default_conversion (arg);
	      break;
	   case BIT_NOT_EXPR:
	      /* ~ works on integer types and non float vectors. */
	      if (typecode == INTEGER_TYPE || (gnu_vector_type_p (TREE_TYPE (arg)) && !VECTOR_FLOAT_TYPE_P (TREE_TYPE (arg)))){
		     tree e = arg;
		     /* Warn if the expression has boolean value.  */
		     while (TREE_CODE (e) == COMPOUND_EXPR)
		        e = TREE_OPERAND (e, 1);

		     if ((TREE_CODE (TREE_TYPE (arg)) == BOOLEAN_TYPE || truth_value_p (TREE_CODE (e)))){
		        auto_diagnostic_group d;
		        if (warning_at (location, OPT_Wbool_operation,"%<~%> on a boolean expression")){
				    argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=x_on_a_boolean_expression_did_you_mean_to_use_logical_not;
			    }
		     }
		     if (!noconvert)
		       arg = aet_default_conversion (arg);
		  }else if (typecode == COMPLEX_TYPE){
		     code = CONJ_EXPR;
			 argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=ISO_C_does_not_support_x_for_complex_conjugation;
		     if (!noconvert)
		        arg = aet_default_conversion (arg);
		  }else{
		     argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=wrong_type_argument_to_bit_complement;
		     return error_mark_node;
		  }
	      break;
	   case ABS_EXPR:
	      if (!(typecode == INTEGER_TYPE || typecode == REAL_TYPE)){
			 argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=wrong_type_argument_to_abs;
	 	     return error_mark_node;
	 	  }else if (!noconvert)
	 	     arg = aet_default_conversion (arg);
	      break;
	   case ABSU_EXPR:
	      if (!(typecode == INTEGER_TYPE)){
		     argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=wrong_type_argument_to_absu;
	  	     return error_mark_node;
	  	  }else if (!noconvert)
	  	     arg = aet_default_conversion (arg);
	      break;
	   case CONJ_EXPR:
	      /* Conjugating a real value is a no-op, but allow it anyway.  */
	      if (!(typecode == INTEGER_TYPE || typecode == REAL_TYPE || typecode == COMPLEX_TYPE)){
			 argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=wrong_type_argument_to_conjugation;
		     return error_mark_node;
		  }else if (!noconvert)
		     arg = aet_default_conversion (arg);
	      break;
	   case TRUTH_NOT_EXPR:
	      if (typecode != INTEGER_TYPE && typecode != FIXED_POINT_TYPE && typecode != REAL_TYPE && typecode != POINTER_TYPE && typecode != COMPLEX_TYPE){
			 argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=wrong_type_argument_to_unary_exclamation_mark;
		     return error_mark_node;
		  }
	      if (int_operands){
		     arg = aet_c_objc_common_truthvalue_conversion (location, xarg);
		     arg = remove_c_maybe_const_expr (arg);
		  }else
		     arg = aet_c_objc_common_truthvalue_conversion (location, arg);
	      ret = invert_truthvalue_loc (location, arg);
	      /* If the TRUTH_NOT_EXPR has been folded, reset the location.  */
	      if (EXPR_P (ret) && EXPR_HAS_LOCATION (ret))
		     location = EXPR_LOCATION (ret);
	      goto return_build_unary_op;
	   case REALPART_EXPR:
	   case IMAGPART_EXPR:
	      ret = build_real_imag_expr (location, code, arg);//重新设计
	      if (ret == error_mark_node)
	  	     return error_mark_node;
	      if (eptype && TREE_CODE (eptype) == COMPLEX_TYPE)
	  	     eptype = TREE_TYPE (eptype);
	      goto return_build_unary_op;
	   case PREINCREMENT_EXPR:
	   case POSTINCREMENT_EXPR:
	   case PREDECREMENT_EXPR:
	   case POSTDECREMENT_EXPR:
	      if (TREE_CODE (arg) == C_MAYBE_CONST_EXPR){
	    		printf("checkUnaryOp 44 typecode: %s code:%s\n",get_tree_code_name(typecode),get_tree_code_name(code));

		     tree inner = checkUnaryOp (location, code,C_MAYBE_CONST_EXPR_EXPR (arg),noconvert);
		     if (inner == error_mark_node)
		        return error_mark_node;
		     ret = build2 (C_MAYBE_CONST_EXPR, TREE_TYPE (inner),C_MAYBE_CONST_EXPR_PRE (arg), inner);
		     gcc_assert (!C_MAYBE_CONST_EXPR_INT_OPERANDS (arg));
		     C_MAYBE_CONST_EXPR_NON_CONST (ret) = 1;
		     goto return_build_unary_op;
		  }
	      if (!objc_is_property_ref (arg) && !lvalue_or_else (location,arg, ((code == PREINCREMENT_EXPR|| code == POSTINCREMENT_EXPR)? lv_increment:lv_decrement)))
	    	 return error_mark_node;
  		printf("checkUnaryOp 55 typecode: %s code:%s\n",get_tree_code_name(typecode),get_tree_code_name(code));

          if (TREE_CODE (TREE_TYPE (arg)) == ENUMERAL_TYPE){
	    	 if (code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR)
				argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=increment_of_enumeration_value_is_invalid_in_Cplusplus;
	    	 else
				argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=decrement_of_enumeration_value_is_invalid_in_Cplusplus;
	      }
          if (TREE_CODE (TREE_TYPE (arg)) == BOOLEAN_TYPE){
    	     if (code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR)
 				argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=increment_of_a_boolean_expression;
 	    	 else
 				argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=decrement_of_a_boolean_expressio;
    	  }
          /* Ensure the argument is fully folded inside any SAVE_EXPR.  */
          arg = c_fully_fold (arg, false, NULL, true);
          bool atomic_op;
          atomic_op = really_atomic_lvalue (arg);

             /* Increment or decrement the real part of the value,
       	    and don't change the imaginary part.  */
          if (typecode == COMPLEX_TYPE){

       	     tree real, imag;
			 argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=ISO_C_does_not_support_x_and_y_on_complex_types;
       	     if (!atomic_op){
       	        arg = stabilize_reference (arg);
       	        real = checkUnaryOp (EXPR_LOCATION (arg), REALPART_EXPR, arg,true);
       	        imag = checkUnaryOp (EXPR_LOCATION (arg), IMAGPART_EXPR, arg,true);
       	        real = checkUnaryOp (EXPR_LOCATION (arg), code, real, true);
       	        if (real == error_mark_node || imag == error_mark_node)
       		       return error_mark_node;
       	        ret = build2 (COMPLEX_EXPR, TREE_TYPE (arg),real, imag);
       	        goto return_build_unary_op;
       	     }
       	  }
           /* Report invalid types.  */
          if (typecode != POINTER_TYPE && typecode != FIXED_POINT_TYPE  && typecode != INTEGER_TYPE && typecode != REAL_TYPE
    	       && typecode != COMPLEX_TYPE  && !gnu_vector_type_p (TREE_TYPE (arg))){
    	     if (code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR)
    			argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=wrong_type_argument_to_increment;
    	     else
 			    argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=wrong_type_argument_to_decrement;
    	     return error_mark_node;
    	  }

          {
        	 tree inc;
        	 argtype = TREE_TYPE (arg);
        	 /* Compute the increment.  */
        	 if (typecode == POINTER_TYPE){
        	      /* If pointer target is an incomplete type,
        	       we just cannot know how to do the arithmetic.  */
 	    		printf("checkUnaryOp 66 typecode: %s code:%s\n",get_tree_code_name(typecode),get_tree_code_name(TREE_CODE(argtype)));

        	    if (!COMPLETE_OR_VOID_TYPE_P (TREE_TYPE (argtype))){
        		   if (code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR)
           			  argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=increment_of_pointer_to_an_incomplete_type_x;
        		   else
   	    			  argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=decrement_of_pointer_to_an_incomplete_type_x;
        	    }else if (TREE_CODE (TREE_TYPE (argtype)) == FUNCTION_TYPE|| TREE_CODE (TREE_TYPE (argtype)) == VOID_TYPE){
        		   if (code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR)
        		      argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=wrong_type_argument_to_increment;
        		   else
    				  argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=wrong_type_argument_to_decrement;
        	    }else
        	      verify_type_context (location, TCTX_POINTER_ARITH,TREE_TYPE (argtype));

        	    inc = c_size_in_bytes (TREE_TYPE (argtype));
        	    inc = convert_to_ptrofftype_loc (location, inc);
        	 }else if (FRACT_MODE_P (TYPE_MODE (argtype))){
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
       	     }else{
       	        inc = VECTOR_TYPE_P (argtype)? build_one_cst (argtype): integer_one_node;
       	        inc = aet_convert (argtype, inc);
       	     }
        		/* Report a read-only lvalue.  */
             if (TYPE_READONLY (argtype)){
         	    argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=error_or_warn_unknown;
        		return error_mark_node;
        	 }else if (TREE_READONLY (arg))
   		        argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=error_or_warn_unknown;
         	 if (atomic_op){
           	    ret=integer_zero_node;//绕开build_atomic_assign 只是检查不需要真的调用 build_atomic_assign也没有警告和错误
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
	      if (VOID_TYPE_P (TREE_TYPE (arg)) && TYPE_QUALS (TREE_TYPE (arg)) == TYPE_UNQUALIFIED && (!INDIRECT_REF_P (arg) || !flag_isoc99))
 		     argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=taking_address_of_expression_of_type_void;

	      /* Let &* cancel out to simplify resulting code.  */
	      if (INDIRECT_REF_P (arg)){
		    /* Don't let this be an lvalue.  */
		     if (aet_lvalue_p (TREE_OPERAND (arg, 0)))
		        return non_lvalue_loc (location, TREE_OPERAND (arg, 0));
		     ret = TREE_OPERAND (arg, 0);
		     goto return_build_unary_op;
		  }
	      /* Anything not already handled and not a true memory reference
	    	 or a non-lvalue array is an error.  */
	      if (typecode != FUNCTION_TYPE && !noconvert && !lvalue_or_else (location, arg, lv_addressof))
	    	 return error_mark_node;
	      if (TREE_CODE (arg) == C_MAYBE_CONST_EXPR){
		     tree inner = checkUnaryOp (location, code,C_MAYBE_CONST_EXPR_EXPR (arg),noconvert);
		     ret = build2 (C_MAYBE_CONST_EXPR, TREE_TYPE (inner),C_MAYBE_CONST_EXPR_PRE (arg), inner);
		     gcc_assert (!C_MAYBE_CONST_EXPR_INT_OPERANDS (arg));
		     C_MAYBE_CONST_EXPR_NON_CONST (ret) = C_MAYBE_CONST_EXPR_NON_CONST (arg);
		     goto return_build_unary_op;
		  }
	      argtype = TREE_TYPE (arg);

	       /* If the lvalue is const or volatile, merge that into the type
	 	    to which the address will point.  This is only needed
	 	    for function types.  */
	      if ((DECL_P (arg) || REFERENCE_CLASS_P (arg)) && (TREE_READONLY (arg) || TREE_THIS_VOLATILE (arg)) && TREE_CODE (argtype) == FUNCTION_TYPE){
	 	     int orig_quals = TYPE_QUALS (strip_array_types (argtype));
	 	     int quals = orig_quals;
	 	     if (TREE_READONLY (arg))
	 	        quals |= TYPE_QUAL_CONST;
	 	     if (TREE_THIS_VOLATILE (arg))
	 	        quals |= TYPE_QUAL_VOLATILE;
	 	     argtype = aet_c_build_qualified_type (argtype, quals);
	 	  }
	      switch (TREE_CODE (arg)){
	   	     case COMPONENT_REF:
	   	        if (DECL_C_BIT_FIELD (TREE_OPERAND (arg, 1))){
	         	   argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=cannot_take_address_of_bit_field_x;
	   	           return error_mark_node;
	   	        }
	   		 case ARRAY_REF:
	   		    if (TYPE_REVERSE_STORAGE_ORDER (TREE_TYPE (TREE_OPERAND (arg, 0)))){
	   		       if (!AGGREGATE_TYPE_P (TREE_TYPE (arg))){
		         	  argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=cannot_take_address_of_scalar_with_reverse_storage_order;
	   			      return error_mark_node;
	   			   }
	   		       if (TREE_CODE (TREE_TYPE (arg)) == ARRAY_TYPE && TYPE_REVERSE_STORAGE_ORDER (TREE_TYPE (arg)))
	   	 		     argsFuncsInfo.warns[argsFuncsInfo.warnCount++]=address_of_array_with_reverse_scalar_storage_order_requested;
	   		    }
	   		 default:
	   		    break;
	      }
	      if (!aet_c_mark_addressable (arg))
	 	     return error_mark_node;

	      gcc_assert (TREE_CODE (arg) != COMPONENT_REF || !DECL_C_BIT_FIELD (TREE_OPERAND (arg, 1)));
	      argtype = build_pointer_type (argtype);

	       /* ??? Cope with user tricks that amount to offsetof.  Delete this
	 	 when we have proper support for integer constant expressions.  */
	      val = get_base_address (arg);
	      if (val && INDIRECT_REF_P (val) && TREE_CONSTANT (TREE_OPERAND (val, 0))){
	 	     ret = fold_offsetof (arg, argtype);
	 	     goto return_build_unary_op;
	 	  }
	      val = build1 (ADDR_EXPR, argtype, arg);
	      ret = val;
	      goto return_build_unary_op;
       default:
	     argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=go_unreachable;
 	     return error_mark_node;

	}

	  ret =integer_zero_node;
return_build_unary_op:
	  return ret;
}

int aet_check_build_unary_op (location_t location, enum tree_code code, tree xarg,bool noconvert)
{
	tree result=checkUnaryOp(location, code, xarg,noconvert);
	if(result==error_mark_node){
		printf("aet_check_build_unary_op 00 出错了 code:%s\n",get_tree_code_name(code));
		return -1;
	}
	return 0;
}



