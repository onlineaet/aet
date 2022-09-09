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

#define GCC_AET_C_COMMON_C

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "target.h"
#include "function.h"
#include "tree.h"
#include "memmodel.h"
#include "c-family/c-common.h"
#include "gimple-expr.h"
#include "tm_p.h"
#include "stringpool.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "intl.h"
#include "stor-layout.h"
#include "calls.h"
#include "attribs.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-objc.h"
#include "common/common-target.h"
#include "langhooks.h"
#include "tree-inline.h"
#include "toplev.h"
#include "tree-iterator.h"
#include "opts.h"
#include "gimplify.h"
#include "substring-locations.h"
#include "spellcheck.h"
#include "c-family/c-spellcheck.h"
#include "selftest.h"
#include "aet-typeck.h"

tree aet_c_common_type_for_size (unsigned int bits, int unsignedp)
{
  int i;

  if (bits == TYPE_PRECISION (integer_type_node))
    return unsignedp ? unsigned_type_node : integer_type_node;

  if (bits == TYPE_PRECISION (signed_char_type_node))
    return unsignedp ? unsigned_char_type_node : signed_char_type_node;

  if (bits == TYPE_PRECISION (short_integer_type_node))
    return unsignedp ? short_unsigned_type_node : short_integer_type_node;

  if (bits == TYPE_PRECISION (long_integer_type_node))
    return unsignedp ? long_unsigned_type_node : long_integer_type_node;

  if (bits == TYPE_PRECISION (long_long_integer_type_node))
    return (unsignedp ? long_long_unsigned_type_node
	    : long_long_integer_type_node);

  for (i = 0; i < NUM_INT_N_ENTS; i ++)
    if (int_n_enabled_p[i]
	&& bits == int_n_data[i].bitsize)
      return (unsignedp ? int_n_trees[i].unsigned_type
	      : int_n_trees[i].signed_type);

  if (bits == TYPE_PRECISION (widest_integer_literal_type_node))
    return (unsignedp ? widest_unsigned_literal_type_node
	    : widest_integer_literal_type_node);

  if (bits <= TYPE_PRECISION (intQI_type_node))
    return unsignedp ? unsigned_intQI_type_node : intQI_type_node;

  if (bits <= TYPE_PRECISION (intHI_type_node))
    return unsignedp ? unsigned_intHI_type_node : intHI_type_node;

  if (bits <= TYPE_PRECISION (intSI_type_node))
    return unsignedp ? unsigned_intSI_type_node : intSI_type_node;

  if (bits <= TYPE_PRECISION (intDI_type_node))
    return unsignedp ? unsigned_intDI_type_node : intDI_type_node;

  return NULL_TREE;
}

/* Used to help initialize the builtin-types.def table.  When a type of
   the correct size doesn't exist, use error_mark_node instead of NULL.
   The later results in segfaults even when a decl using the type doesn't
   get invoked.  */

tree aet_builtin_type_for_size (int size, bool unsignedp)
{
  tree type = aet_c_common_type_for_size (size, unsignedp);
  return type ? type : error_mark_node;
}

/* Work out the size of the first argument of a call to
   __builtin_speculation_safe_value.  Only pointers and integral types
   are permitted.  Return -1 if the argument type is not supported or
   the size is too large; 0 if the argument type is a pointer or the
   size if it is integral.  */
static enum built_in_function
speculation_safe_value_resolve_call (tree function, vec<tree, va_gc> *params)
{
  /* Type of the argument.  */
  tree type;
  int size;

  if (vec_safe_is_empty (params))
    {
      error ("too few arguments to function %qE", function);
      return BUILT_IN_NONE;
    }

  type = TREE_TYPE ((*params)[0]);
  if (TREE_CODE (type) == ARRAY_TYPE && c_dialect_cxx ())
    {
      /* Force array-to-pointer decay for C++.   */
      (*params)[0] = default_conversion ((*params)[0]);
      type = TREE_TYPE ((*params)[0]);
    }

  if (POINTER_TYPE_P (type))
    return BUILT_IN_SPECULATION_SAFE_VALUE_PTR;

  if (!INTEGRAL_TYPE_P (type))
    goto incompatible;

  if (!COMPLETE_TYPE_P (type))
    goto incompatible;

  size = tree_to_uhwi (TYPE_SIZE_UNIT (type));
  if (size == 1 || size == 2 || size == 4 || size == 8 || size == 16)
    return ((enum built_in_function)
	    ((int) BUILT_IN_SPECULATION_SAFE_VALUE_1 + exact_log2 (size)));

 incompatible:
  /* Issue the diagnostic only if the argument is valid, otherwise
     it would be redundant at best and could be misleading.  */
  if (type != error_mark_node)
    error ("operand type %qT is incompatible with argument %d of %qE",
	   type, 1, function);

  return BUILT_IN_NONE;
}

/* Validate and coerce PARAMS, the arguments to ORIG_FUNCTION to fit
   the prototype for FUNCTION.  The first argument is mandatory, a second
   argument, if present, must be type compatible with the first.  */
static bool
speculation_safe_value_resolve_params (location_t loc, tree orig_function,
				       vec<tree, va_gc> *params)
{
  tree val;

  if (params->length () == 0)
    {
      error_at (loc, "too few arguments to function %qE", orig_function);
      return false;
    }

  else if (params->length () > 2)
    {
      error_at (loc, "too many arguments to function %qE", orig_function);
      return false;
    }

  val = (*params)[0];
  if (TREE_CODE (TREE_TYPE (val)) == ARRAY_TYPE)
    val = default_conversion (val);
  if (!(TREE_CODE (TREE_TYPE (val)) == POINTER_TYPE
	|| TREE_CODE (TREE_TYPE (val)) == INTEGER_TYPE))
    {
      error_at (loc,
		"expecting argument of type pointer or of type integer "
		"for argument 1");
      return false;
    }
  (*params)[0] = val;

  if (params->length () == 2)
    {
      tree val2 = (*params)[1];
      if (TREE_CODE (TREE_TYPE (val2)) == ARRAY_TYPE)
	val2 = default_conversion (val2);
      if (error_operand_p (val2))
	return false;
      if (!(TREE_TYPE (val) == TREE_TYPE (val2)
	    || useless_type_conversion_p (TREE_TYPE (val), TREE_TYPE (val2))))
	{
	  error_at (loc, "both arguments must be compatible");
	  return false;
	}
      (*params)[1] = val2;
    }

  return true;
}

/* Cast the result of the builtin back to the type of the first argument,
   preserving any qualifiers that it might have.  */
static tree
speculation_safe_value_resolve_return (tree first_param, tree result)
{
  tree ptype = TREE_TYPE (first_param);
  tree rtype = TREE_TYPE (result);
  ptype = TYPE_MAIN_VARIANT (ptype);

  if (tree_int_cst_equal (TYPE_SIZE (ptype), TYPE_SIZE (rtype)))
    return convert (ptype, result);

  return result;
}

/* A helper function for resolve_overloaded_builtin in resolving the
   overloaded __sync_ builtins.  Returns a positive power of 2 if the
   first operand of PARAMS is a pointer to a supported data type.
   Returns 0 if an error is encountered.
   FETCH is true when FUNCTION is one of the _FETCH_OP_ or _OP_FETCH_
   built-ins.  */

static int
sync_resolve_size (tree function, vec<tree, va_gc> *params, bool fetch)
{
  /* Type of the argument.  */
  tree argtype;
  /* Type the argument points to.  */
  tree type;
  int size;

  if (vec_safe_is_empty (params))
    {
      error ("too few arguments to function %qE", function);
      return 0;
    }

  argtype = type = TREE_TYPE ((*params)[0]);
  if (TREE_CODE (type) == ARRAY_TYPE && c_dialect_cxx ())
    {
      /* Force array-to-pointer decay for C++.  */
      (*params)[0] = default_conversion ((*params)[0]);
      type = TREE_TYPE ((*params)[0]);
    }
  if (TREE_CODE (type) != POINTER_TYPE)
    goto incompatible;

  type = TREE_TYPE (type);
  if (!INTEGRAL_TYPE_P (type) && !POINTER_TYPE_P (type))
    goto incompatible;

  if (!COMPLETE_TYPE_P (type))
    goto incompatible;

  if (fetch && TREE_CODE (type) == BOOLEAN_TYPE)
    goto incompatible;

  size = tree_to_uhwi (TYPE_SIZE_UNIT (type));
  if (size == 1 || size == 2 || size == 4 || size == 8 || size == 16)
    return size;

 incompatible:
  /* Issue the diagnostic only if the argument is valid, otherwise
     it would be redundant at best and could be misleading.  */
  if (argtype != error_mark_node)
    error ("operand type %qT is incompatible with argument %d of %qE",
	   argtype, 1, function);
  return 0;
}

/* A helper function for resolve_overloaded_builtin.  Adds casts to
   PARAMS to make arguments match up with those of FUNCTION.  Drops
   the variadic arguments at the end.  Returns false if some error
   was encountered; true on success.  */

static bool
sync_resolve_params (location_t loc, tree orig_function, tree function,
		     vec<tree, va_gc> *params, bool orig_format)
{
  function_args_iterator iter;
  tree ptype;
  unsigned int parmnum;

  function_args_iter_init (&iter, TREE_TYPE (function));
  /* We've declared the implementation functions to use "volatile void *"
     as the pointer parameter, so we shouldn't get any complaints from the
     call to check_function_arguments what ever type the user used.  */
  function_args_iter_next (&iter);
  ptype = TREE_TYPE (TREE_TYPE ((*params)[0]));
  ptype = TYPE_MAIN_VARIANT (ptype);

  /* For the rest of the values, we need to cast these to FTYPE, so that we
     don't get warnings for passing pointer types, etc.  */
  parmnum = 0;
  while (1)
    {
      tree val, arg_type;

      arg_type = function_args_iter_cond (&iter);
      /* XXX void_type_node belies the abstraction.  */
      if (arg_type == void_type_node)
	break;

      ++parmnum;
      if (params->length () <= parmnum)
	{
	  error_at (loc, "too few arguments to function %qE", orig_function);
	  return false;
	}

      /* Only convert parameters if arg_type is unsigned integer type with
	 new format sync routines, i.e. don't attempt to convert pointer
	 arguments (e.g. EXPECTED argument of __atomic_compare_exchange_n),
	 bool arguments (e.g. WEAK argument) or signed int arguments (memmodel
	 kinds).  */
      if (TREE_CODE (arg_type) == INTEGER_TYPE && TYPE_UNSIGNED (arg_type))
	{
	  /* Ideally for the first conversion we'd use convert_for_assignment
	     so that we get warnings for anything that doesn't match the pointer
	     type.  This isn't portable across the C and C++ front ends atm.  */
	  val = (*params)[parmnum];
	  val = convert (ptype, val);
	  val = convert (arg_type, val);
	  (*params)[parmnum] = val;
	}

      function_args_iter_next (&iter);
    }

  /* __atomic routines are not variadic.  */
  if (!orig_format && params->length () != parmnum + 1)
    {
      error_at (loc, "too many arguments to function %qE", orig_function);
      return false;
    }

  /* The definition of these primitives is variadic, with the remaining
     being "an optional list of variables protected by the memory barrier".
     No clue what that's supposed to mean, precisely, but we consider all
     call-clobbered variables to be protected so we're safe.  */
  params->truncate (parmnum + 1);

  return true;
}

/* A helper function for resolve_overloaded_builtin.  Adds a cast to
   RESULT to make it match the type of the first pointer argument in
   PARAMS.  */

static tree
sync_resolve_return (tree first_param, tree result, bool orig_format)
{
  tree ptype = TREE_TYPE (TREE_TYPE (first_param));
  tree rtype = TREE_TYPE (result);
  ptype = TYPE_MAIN_VARIANT (ptype);

  /* New format doesn't require casting unless the types are the same size.  */
  if (orig_format || tree_int_cst_equal (TYPE_SIZE (ptype), TYPE_SIZE (rtype)))
    return convert (ptype, result);
  else
    return result;
}

/* This function verifies the PARAMS to generic atomic FUNCTION.
   It returns the size if all the parameters are the same size, otherwise
   0 is returned if the parameters are invalid.  */

static int
get_atomic_generic_size (location_t loc, tree function,
			 vec<tree, va_gc> *params)
{
  unsigned int n_param;
  unsigned int n_model;
  unsigned int x;
  int size_0;
  tree type_0;

  /* Determine the parameter makeup.  */
  switch (DECL_FUNCTION_CODE (function))
    {
    case BUILT_IN_ATOMIC_EXCHANGE:
      n_param = 4;
      n_model = 1;
      break;
    case BUILT_IN_ATOMIC_LOAD:
    case BUILT_IN_ATOMIC_STORE:
      n_param = 3;
      n_model = 1;
      break;
    case BUILT_IN_ATOMIC_COMPARE_EXCHANGE:
      n_param = 6;
      n_model = 2;
      break;
    default:
      gcc_unreachable ();
    }

  if (vec_safe_length (params) != n_param)
    {
      error_at (loc, "incorrect number of arguments to function %qE", function);
      return 0;
    }

  /* Get type of first parameter, and determine its size.  */
  type_0 = TREE_TYPE ((*params)[0]);
  if (TREE_CODE (type_0) == ARRAY_TYPE && c_dialect_cxx ())
    {
      /* Force array-to-pointer decay for C++.  */
      (*params)[0] = default_conversion ((*params)[0]);
      type_0 = TREE_TYPE ((*params)[0]);
    }
  if (TREE_CODE (type_0) != POINTER_TYPE || VOID_TYPE_P (TREE_TYPE (type_0)))
    {
      error_at (loc, "argument 1 of %qE must be a non-void pointer type",
		function);
      return 0;
    }

  /* Types must be compile time constant sizes. */
  if (TREE_CODE ((TYPE_SIZE_UNIT (TREE_TYPE (type_0)))) != INTEGER_CST)
    {
      error_at (loc, 
		"argument 1 of %qE must be a pointer to a constant size type",
		function);
      return 0;
    }

  size_0 = tree_to_uhwi (TYPE_SIZE_UNIT (TREE_TYPE (type_0)));

  /* Zero size objects are not allowed.  */
  if (size_0 == 0)
    {
      error_at (loc, 
		"argument 1 of %qE must be a pointer to a nonzero size object",
		function);
      return 0;
    }

  /* Check each other parameter is a pointer and the same size.  */
  for (x = 0; x < n_param - n_model; x++)
    {
      int size;
      tree type = TREE_TYPE ((*params)[x]);
      /* __atomic_compare_exchange has a bool in the 4th position, skip it.  */
      if (n_param == 6 && x == 3)
        continue;
      if (TREE_CODE (type) == ARRAY_TYPE && c_dialect_cxx ())
	{
	  /* Force array-to-pointer decay for C++.  */
	  (*params)[x] = default_conversion ((*params)[x]);
	  type = TREE_TYPE ((*params)[x]);
	}
      if (!POINTER_TYPE_P (type))
	{
	  error_at (loc, "argument %d of %qE must be a pointer type", x + 1,
		    function);
	  return 0;
	}
      else if (TYPE_SIZE_UNIT (TREE_TYPE (type))
	       && TREE_CODE ((TYPE_SIZE_UNIT (TREE_TYPE (type))))
		  != INTEGER_CST)
	{
	  error_at (loc, "argument %d of %qE must be a pointer to a constant "
		    "size type", x + 1, function);
	  return 0;
	}
      else if (FUNCTION_POINTER_TYPE_P (type))
	{
	  error_at (loc, "argument %d of %qE must not be a pointer to a "
		    "function", x + 1, function);
	  return 0;
	}
      tree type_size = TYPE_SIZE_UNIT (TREE_TYPE (type));
      size = type_size ? tree_to_uhwi (type_size) : 0;
      if (size != size_0)
	{
	  error_at (loc, "size mismatch in argument %d of %qE", x + 1,
		    function);
	  return 0;
	}
    }

  /* Check memory model parameters for validity.  */
  for (x = n_param - n_model ; x < n_param; x++)
    {
      tree p = (*params)[x];
      if (!INTEGRAL_TYPE_P (TREE_TYPE (p)))
	{
	  error_at (loc, "non-integer memory model argument %d of %qE", x + 1,
		    function);
	  return 0;
	}
      p = fold_for_warn (p);
      if (TREE_CODE (p) == INTEGER_CST)
	{
	  /* memmodel_base masks the low 16 bits, thus ignore any bits above
	     it by using TREE_INT_CST_LOW instead of tree_to_*hwi.  Those high
	     bits will be checked later during expansion in target specific
	     way.  */
	  if (memmodel_base (TREE_INT_CST_LOW (p)) >= MEMMODEL_LAST)
	    warning_at (loc, OPT_Winvalid_memory_model,
			"invalid memory model argument %d of %qE", x + 1,
			function);
	}
    }

  return size_0;
}


/* This will take an __atomic_ generic FUNCTION call, and add a size parameter N
   at the beginning of the parameter list PARAMS representing the size of the
   objects.  This is to match the library ABI requirement.  LOC is the location
   of the function call.  
   The new function is returned if it needed rebuilding, otherwise NULL_TREE is
   returned to allow the external call to be constructed.  */

static tree
add_atomic_size_parameter (unsigned n, location_t loc, tree function, 
			   vec<tree, va_gc> *params)
{
  tree size_node;

  /* Insert a SIZE_T parameter as the first param.  If there isn't
     enough space, allocate a new vector and recursively re-build with that.  */
  if (!params->space (1))
    {
      unsigned int z, len;
      vec<tree, va_gc> *v;
      tree f;

      len = params->length ();
      vec_alloc (v, len + 1);
      v->quick_push (build_int_cst (size_type_node, n));
      for (z = 0; z < len; z++)
	v->quick_push ((*params)[z]);
      f = build_function_call_vec (loc, vNULL, function, v, NULL);
      vec_free (v);
      return f;
    }

  /* Add the size parameter and leave as a function call for processing.  */
  size_node = build_int_cst (size_type_node, n);
  params->quick_insert (0, size_node);
  return NULL_TREE;
}


/* Return whether atomic operations for naturally aligned N-byte
   arguments are supported, whether inline or through libatomic.  */
static bool
atomic_size_supported_p (int n)
{
  switch (n)
    {
    case 1:
    case 2:
    case 4:
    case 8:
      return true;

    case 16:
      return targetm.scalar_mode_supported_p (TImode);

    default:
      return false;
    }
}

/* This will process an __atomic_exchange function call, determine whether it
   needs to be mapped to the _N variation, or turned into a library call.
   LOC is the location of the builtin call.
   FUNCTION is the DECL that has been invoked;
   PARAMS is the argument list for the call.  The return value is non-null
   TRUE is returned if it is translated into the proper format for a call to the
   external library, and NEW_RETURN is set the tree for that function.
   FALSE is returned if processing for the _N variation is required, and 
   NEW_RETURN is set to the return value the result is copied into.  */
static bool
resolve_overloaded_atomic_exchange (location_t loc, tree function, 
				    vec<tree, va_gc> *params, tree *new_return)
{	
  tree p0, p1, p2, p3;
  tree I_type, I_type_ptr;
  int n = get_atomic_generic_size (loc, function, params);

  /* Size of 0 is an error condition.  */
  if (n == 0)
    {
      *new_return = error_mark_node;
      return true;
    }

  /* If not a lock-free size, change to the library generic format.  */
  if (!atomic_size_supported_p (n))
    {
      *new_return = add_atomic_size_parameter (n, loc, function, params);
      return true;
    }

  /* Otherwise there is a lockfree match, transform the call from:
       void fn(T* mem, T* desired, T* return, model)
     into
       *return = (T) (fn (In* mem, (In) *desired, model))  */

  p0 = (*params)[0];
  p1 = (*params)[1];
  p2 = (*params)[2];
  p3 = (*params)[3];
  
  /* Create pointer to appropriate size.  */
  I_type = aet_builtin_type_for_size (BITS_PER_UNIT * n, 1);
  I_type_ptr = build_pointer_type (I_type);

  /* Convert object pointer to required type.  */
  p0 = build1 (VIEW_CONVERT_EXPR, I_type_ptr, p0);
  (*params)[0] = p0; 
  /* Convert new value to required type, and dereference it.  */
  p1 = build_indirect_ref (loc, p1, RO_UNARY_STAR);
  p1 = build1 (VIEW_CONVERT_EXPR, I_type, p1);
  (*params)[1] = p1;

  /* Move memory model to the 3rd position, and end param list.  */
  (*params)[2] = p3;
  params->truncate (3);

  /* Convert return pointer and dereference it for later assignment.  */
  *new_return = build_indirect_ref (loc, p2, RO_UNARY_STAR);

  return false;
}


/* This will process an __atomic_compare_exchange function call, determine 
   whether it needs to be mapped to the _N variation, or turned into a lib call.
   LOC is the location of the builtin call.
   FUNCTION is the DECL that has been invoked;
   PARAMS is the argument list for the call.  The return value is non-null
   TRUE is returned if it is translated into the proper format for a call to the
   external library, and NEW_RETURN is set the tree for that function.
   FALSE is returned if processing for the _N variation is required.  */

static bool
resolve_overloaded_atomic_compare_exchange (location_t loc, tree function, 
					    vec<tree, va_gc> *params, 
					    tree *new_return)
{	
  tree p0, p1, p2;
  tree I_type, I_type_ptr;
  int n = get_atomic_generic_size (loc, function, params);

  /* Size of 0 is an error condition.  */
  if (n == 0)
    {
      *new_return = error_mark_node;
      return true;
    }

  /* If not a lock-free size, change to the library generic format.  */
  if (!atomic_size_supported_p (n))
    {
      /* The library generic format does not have the weak parameter, so 
	 remove it from the param list.  Since a parameter has been removed,
	 we can be sure that there is room for the SIZE_T parameter, meaning
	 there will not be a recursive rebuilding of the parameter list, so
	 there is no danger this will be done twice.  */
      if (n > 0)
        {
	  (*params)[3] = (*params)[4];
	  (*params)[4] = (*params)[5];
	  params->truncate (5);
	}
      *new_return = add_atomic_size_parameter (n, loc, function, params);
      return true;
    }

  /* Otherwise, there is a match, so the call needs to be transformed from:
       bool fn(T* mem, T* desired, T* return, weak, success, failure)
     into
       bool fn ((In *)mem, (In *)expected, (In) *desired, weak, succ, fail)  */

  p0 = (*params)[0];
  p1 = (*params)[1];
  p2 = (*params)[2];
  
  /* Create pointer to appropriate size.  */
  I_type = aet_builtin_type_for_size (BITS_PER_UNIT * n, 1);
  I_type_ptr = build_pointer_type (I_type);

  /* Convert object pointer to required type.  */
  p0 = build1 (VIEW_CONVERT_EXPR, I_type_ptr, p0);
  (*params)[0] = p0;

  /* Convert expected pointer to required type.  */
  p1 = build1 (VIEW_CONVERT_EXPR, I_type_ptr, p1);
  (*params)[1] = p1;

  /* Convert desired value to required type, and dereference it.  */
  p2 = build_indirect_ref (loc, p2, RO_UNARY_STAR);
  p2 = build1 (VIEW_CONVERT_EXPR, I_type, p2);
  (*params)[2] = p2;

  /* The rest of the parameters are fine. NULL means no special return value
     processing.*/
  *new_return = NULL;
  return false;
}


/* This will process an __atomic_load function call, determine whether it
   needs to be mapped to the _N variation, or turned into a library call.
   LOC is the location of the builtin call.
   FUNCTION is the DECL that has been invoked;
   PARAMS is the argument list for the call.  The return value is non-null
   TRUE is returned if it is translated into the proper format for a call to the
   external library, and NEW_RETURN is set the tree for that function.
   FALSE is returned if processing for the _N variation is required, and 
   NEW_RETURN is set to the return value the result is copied into.  */

static bool resolve_overloaded_atomic_load (location_t loc, tree function,
				vec<tree, va_gc> *params, tree *new_return)
{	
  tree p0, p1, p2;
  tree I_type, I_type_ptr;
  int n = get_atomic_generic_size (loc, function, params);

  /* Size of 0 is an error condition.  */
  if (n == 0)
    {
      *new_return = error_mark_node;
      return true;
    }

  /* If not a lock-free size, change to the library generic format.  */
  if (!atomic_size_supported_p (n))
    {
      *new_return = add_atomic_size_parameter (n, loc, function, params);
      return true;
    }

  /* Otherwise, there is a match, so the call needs to be transformed from:
       void fn(T* mem, T* return, model)
     into
       *return = (T) (fn ((In *) mem, model))  */

  p0 = (*params)[0];
  p1 = (*params)[1];
  p2 = (*params)[2];
  
  /* Create pointer to appropriate size.  */
  I_type = aet_builtin_type_for_size (BITS_PER_UNIT * n, 1);
  I_type_ptr = build_pointer_type (I_type);

  /* Convert object pointer to required type.  */
  p0 = build1 (VIEW_CONVERT_EXPR, I_type_ptr, p0);
  (*params)[0] = p0;

  /* Move memory model to the 2nd position, and end param list.  */
  (*params)[1] = p2;
  params->truncate (2);

  /* Convert return pointer and dereference it for later assignment.  */
  *new_return = build_indirect_ref (loc, p1, RO_UNARY_STAR);

  return false;
}


/* This will process an __atomic_store function call, determine whether it
   needs to be mapped to the _N variation, or turned into a library call.
   LOC is the location of the builtin call.
   FUNCTION is the DECL that has been invoked;
   PARAMS is the argument list for the call.  The return value is non-null
   TRUE is returned if it is translated into the proper format for a call to the
   external library, and NEW_RETURN is set the tree for that function.
   FALSE is returned if processing for the _N variation is required, and 
   NEW_RETURN is set to the return value the result is copied into.  */

static bool resolve_overloaded_atomic_store (location_t loc, tree function,
				 vec<tree, va_gc> *params, tree *new_return)
{	
  tree p0, p1;
  tree I_type, I_type_ptr;
  int n = get_atomic_generic_size (loc, function, params);

  /* Size of 0 is an error condition.  */
  if (n == 0)
    {
      *new_return = error_mark_node;
      return true;
    }

  /* If not a lock-free size, change to the library generic format.  */
  if (!atomic_size_supported_p (n))
    {
      *new_return = add_atomic_size_parameter (n, loc, function, params);
      return true;
    }

  /* Otherwise, there is a match, so the call needs to be transformed from:
       void fn(T* mem, T* value, model)
     into
       fn ((In *) mem, (In) *value, model)  */

  p0 = (*params)[0];
  p1 = (*params)[1];
  
  /* Create pointer to appropriate size.  */
  I_type = aet_builtin_type_for_size (BITS_PER_UNIT * n, 1);
  I_type_ptr = build_pointer_type (I_type);

  /* Convert object pointer to required type.  */
  p0 = build1 (VIEW_CONVERT_EXPR, I_type_ptr, p0);
  (*params)[0] = p0;

  /* Convert new value to required type, and dereference it.  */
  p1 = build_indirect_ref (loc, p1, RO_UNARY_STAR);
  p1 = build1 (VIEW_CONVERT_EXPR, I_type, p1);
  (*params)[1] = p1;
  
  /* The memory model is in the right spot already. Return is void.  */
  *new_return = NULL_TREE;

  return false;
}


/* Some builtin functions are placeholders for other expressions.  This
   function should be called immediately after parsing the call expression
   before surrounding code has committed to the type of the expression.

   LOC is the location of the builtin call.

   FUNCTION is the DECL that has been invoked; it is known to be a builtin.
   PARAMS is the argument list for the call.  The return value is non-null
   when expansion is complete, and null if normal processing should
   continue.  */

tree aet_resolve_overloaded_builtin (location_t loc, tree function, vec<tree, va_gc> *params)
{
  /* Is function one of the _FETCH_OP_ or _OP_FETCH_ built-ins?
     Those are not valid to call with a pointer to _Bool (or C++ bool)
     and so must be rejected.  */
  bool fetch_op = true;
  bool orig_format = true;
  tree new_return = NULL_TREE;
  switch (DECL_BUILT_IN_CLASS (function))
    {
     printf("resolve_overloaded_builtin 00 %d\n",DECL_BUILT_IN_CLASS (function));
    case BUILT_IN_NORMAL:
      break;
    case BUILT_IN_MD:
      if (targetm.resolve_overloaded_builtin)
	return targetm.resolve_overloaded_builtin (loc, function, params);
      else
	return NULL_TREE;
    default:
      return NULL_TREE;
    }

  /* Handle BUILT_IN_NORMAL here.  */
  enum built_in_function orig_code = DECL_FUNCTION_CODE (function);
  switch (orig_code)
    {
  printf("resolve_overloaded_builtin 11 %d\n",orig_code);

    case BUILT_IN_SPECULATION_SAFE_VALUE_N:
      {
	tree new_function, first_param, result;
	enum built_in_function fncode
	  = speculation_safe_value_resolve_call (function, params);

	if (fncode == BUILT_IN_NONE)
	  return error_mark_node;

	first_param = (*params)[0];
	if (!speculation_safe_value_resolve_params (loc, function, params))
	  return error_mark_node;

	if (targetm.have_speculation_safe_value (true))
	  {
	    new_function = builtin_decl_explicit (fncode);
	    result = aet_build_function_call_vec (loc, vNULL, new_function, params,
					      NULL);

	    if (result == error_mark_node)
	      return result;

	    return speculation_safe_value_resolve_return (first_param, result);
	  }
	else
	  {
	    /* This target doesn't have, or doesn't need, active mitigation
	       against incorrect speculative execution.  Simply return the
	       first parameter to the builtin.  */
	    if (!targetm.have_speculation_safe_value (false))
	      /* The user has invoked __builtin_speculation_safe_value
		 even though __HAVE_SPECULATION_SAFE_VALUE is not
		 defined: emit a warning.  */
	      warning_at (input_location, 0,
			  "this target does not define a speculation barrier; "
			  "your program will still execute correctly, "
			  "but incorrect speculation may not be "
			  "restricted");

	    /* If the optional second argument is present, handle any side
	       effects now.  */
	    if (params->length () == 2
		&& TREE_SIDE_EFFECTS ((*params)[1]))
	      return build2 (COMPOUND_EXPR, TREE_TYPE (first_param),
			     (*params)[1], first_param);

	    return first_param;
	  }
      }

    case BUILT_IN_ATOMIC_EXCHANGE:
    case BUILT_IN_ATOMIC_COMPARE_EXCHANGE:
    case BUILT_IN_ATOMIC_LOAD:
    case BUILT_IN_ATOMIC_STORE:
      {
	/* Handle these 4 together so that they can fall through to the next
	   case if the call is transformed to an _N variant.  */
        switch (orig_code)
	  {
	  case BUILT_IN_ATOMIC_EXCHANGE:
	    {
	      if (resolve_overloaded_atomic_exchange (loc, function, params,
						      &new_return))
		return new_return;
	      /* Change to the _N variant.  */
	      orig_code = BUILT_IN_ATOMIC_EXCHANGE_N;
	      break;
	    }

	  case BUILT_IN_ATOMIC_COMPARE_EXCHANGE:
	    {
	      if (resolve_overloaded_atomic_compare_exchange (loc, function,
							      params,
							      &new_return))
		return new_return;
	      /* Change to the _N variant.  */
	      orig_code = BUILT_IN_ATOMIC_COMPARE_EXCHANGE_N;
	      break;
	    }
	  case BUILT_IN_ATOMIC_LOAD:
	    {
	      if (resolve_overloaded_atomic_load (loc, function, params,
						  &new_return))
		return new_return;
	      /* Change to the _N variant.  */
	      orig_code = BUILT_IN_ATOMIC_LOAD_N;
	      break;
	    }
	  case BUILT_IN_ATOMIC_STORE:
	    {
	      if (resolve_overloaded_atomic_store (loc, function, params,
						   &new_return))
		return new_return;
	      /* Change to the _N variant.  */
	      orig_code = BUILT_IN_ATOMIC_STORE_N;
	      break;
	    }
	  default:
	    gcc_unreachable ();
	  }
      }
      /* FALLTHRU */
    case BUILT_IN_ATOMIC_EXCHANGE_N:
    case BUILT_IN_ATOMIC_COMPARE_EXCHANGE_N:
    case BUILT_IN_ATOMIC_LOAD_N:
    case BUILT_IN_ATOMIC_STORE_N:
      fetch_op = false;
      /* FALLTHRU */
    case BUILT_IN_ATOMIC_ADD_FETCH_N:
    case BUILT_IN_ATOMIC_SUB_FETCH_N:
    case BUILT_IN_ATOMIC_AND_FETCH_N:
    case BUILT_IN_ATOMIC_NAND_FETCH_N:
    case BUILT_IN_ATOMIC_XOR_FETCH_N:
    case BUILT_IN_ATOMIC_OR_FETCH_N:
    case BUILT_IN_ATOMIC_FETCH_ADD_N:
    case BUILT_IN_ATOMIC_FETCH_SUB_N:
    case BUILT_IN_ATOMIC_FETCH_AND_N:
    case BUILT_IN_ATOMIC_FETCH_NAND_N:
    case BUILT_IN_ATOMIC_FETCH_XOR_N:
    case BUILT_IN_ATOMIC_FETCH_OR_N:
      orig_format = false;
      /* FALLTHRU */
    case BUILT_IN_SYNC_FETCH_AND_ADD_N:
    case BUILT_IN_SYNC_FETCH_AND_SUB_N:
    case BUILT_IN_SYNC_FETCH_AND_OR_N:
    case BUILT_IN_SYNC_FETCH_AND_AND_N:
    case BUILT_IN_SYNC_FETCH_AND_XOR_N:
    case BUILT_IN_SYNC_FETCH_AND_NAND_N:
    case BUILT_IN_SYNC_ADD_AND_FETCH_N:
    case BUILT_IN_SYNC_SUB_AND_FETCH_N:
    case BUILT_IN_SYNC_OR_AND_FETCH_N:
    case BUILT_IN_SYNC_AND_AND_FETCH_N:
    case BUILT_IN_SYNC_XOR_AND_FETCH_N:
    case BUILT_IN_SYNC_NAND_AND_FETCH_N:
    case BUILT_IN_SYNC_BOOL_COMPARE_AND_SWAP_N:
    case BUILT_IN_SYNC_VAL_COMPARE_AND_SWAP_N:
    case BUILT_IN_SYNC_LOCK_TEST_AND_SET_N:
    case BUILT_IN_SYNC_LOCK_RELEASE_N:
      {
	/* The following are not _FETCH_OPs and must be accepted with
	   pointers to _Bool (or C++ bool).  */
	if (fetch_op)
	  fetch_op =
	    (orig_code != BUILT_IN_SYNC_BOOL_COMPARE_AND_SWAP_N
	     && orig_code != BUILT_IN_SYNC_VAL_COMPARE_AND_SWAP_N
	     && orig_code != BUILT_IN_SYNC_LOCK_TEST_AND_SET_N
	     && orig_code != BUILT_IN_SYNC_LOCK_RELEASE_N);

	int n = sync_resolve_size (function, params, fetch_op);
	tree new_function, first_param, result;
	enum built_in_function fncode;

	if (n == 0)
	  return error_mark_node;

	fncode = (enum built_in_function)((int)orig_code + exact_log2 (n) + 1);
	new_function = builtin_decl_explicit (fncode);
	if (!sync_resolve_params (loc, function, new_function, params,
				  orig_format))
	  return error_mark_node;

	first_param = (*params)[0];
	result = aet_build_function_call_vec (loc, vNULL, new_function, params,NULL);
	if (result == error_mark_node)
	  return result;
	if (orig_code != BUILT_IN_SYNC_BOOL_COMPARE_AND_SWAP_N
	    && orig_code != BUILT_IN_SYNC_LOCK_RELEASE_N
	    && orig_code != BUILT_IN_ATOMIC_STORE_N
	    && orig_code != BUILT_IN_ATOMIC_COMPARE_EXCHANGE_N)
	  result = sync_resolve_return (first_param, result, orig_format);

	if (fetch_op)
	  /* Prevent -Wunused-value warning.  */
	  TREE_USED (result) = true;

	/* If new_return is set, assign function to that expr and cast the
	   result to void since the generic interface returned void.  */
	if (new_return)
	  {
	    /* Cast function result from I{1,2,4,8,16} to the required type.  */
	    result = build1 (VIEW_CONVERT_EXPR, TREE_TYPE (new_return), result);
	    result = build2 (MODIFY_EXPR, TREE_TYPE (new_return), new_return,
			     result);
	    TREE_SIDE_EFFECTS (result) = 1;
	    protected_set_expr_location (result, loc);
	    result = convert (void_type_node, result);
	  }
	return result;
      }

    default:
    	 // printf("resolve_overloaded_builtin 22 %d\n",orig_code);

      return NULL_TREE;
    }
}

//#include "gt-c-family-c-common.h"
