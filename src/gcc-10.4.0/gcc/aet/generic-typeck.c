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

#include "config.h"
#include <cstdio>
#define INCLUDE_UNIQUE_PTR
#include "system.h"
#include "coretypes.h"
#include "target.h"
#include "function.h"
#include "tree.h"
#include "timevar.h"
#include "stringpool.h"
#include "cgraph.h"
#include "attribs.h"
#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "c-family/c-common.h"
#include "gimple-expr.h"

#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "c-aet.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"
#include "../libcpp/include/cpplib.h"
#include "tree-iterator.h"
#include "asan.h"

#include "aet-typeck.h"
#include "aet-c-common.h"
#include "c-aet.h"
#include "aet-c-fold.h"
#include "aet-convert.h"
#include "nlib.h"
#include "aetutils.h"
#include "aetprinttree.h"
#include "genericutil.h"
#include "classutil.h"


static bool null_pointer_constant_p (const_tree);
static tree qualify_type (tree, tree);
static int  tagged_types_tu_compatible_p (const_tree, const_tree, bool *, bool *);
static int  comp_target_types (location_t, tree, tree);
static int  function_types_compatible_p (const_tree, const_tree, bool *,	bool *);
static int  type_lists_compatible_p (const_tree, const_tree, bool *, bool *);
static tree valid_compound_expr_initializer (tree, tree);
static int   comptypes_internal (const_tree, const_tree, bool *, bool *);

//zclei
static bool aet_c_reject_gcc_builtin (const_tree expr, location_t loc /* = UNKNOWN_LOCATION */);
static tree aet_c_c_build_qualified_type (tree, int, tree = NULL_TREE, size_t = 0);
static bool aet_c_c_mark_addressable (tree, bool = false);
static bool aet_c_lvalue_p (const_tree ref);
static bool aet_c_same_translation_unit_p (const_tree t1, const_tree t2);
static int aet_c_comptypes (tree type1, tree type2);
static tree aet_c_composite_type (tree t1, tree t2);



/**
 * setData(3.1)
 * 把实参3.1转成地址:({float axt=3.1;&axt;})
 *
 */
static tree convertRealOrIntegerCstToPointer00(location_t loc,tree realOrIntCst)
{
	tree id=aet_utils_create_ident("realOrIntCstToPointer");
    tree numberType=TREE_TYPE(realOrIntCst);
	tree varDecl=build_decl (loc, VAR_DECL, id, numberType);
	DECL_INITIAL(varDecl)=realOrIntCst;
	DECL_CONTEXT(varDecl)=current_function_decl;
	tree pointerType=build_pointer_type(numberType);
	tree tempVarDecl=build_decl (loc, VAR_DECL, NULL_TREE, pointerType);
	DECL_ARTIFICIAL (tempVarDecl)=1;
	tree  stmtList = alloc_stmt_list ();
	tree stmt0 = build_stmt (loc, DECL_EXPR, void_type_node);
	append_to_statement_list_force (stmt0, &stmtList);
	tree addExpr= build1 (ADDR_EXPR, pointerType, varDecl);
    tree modifyExpr= build2 (MODIFY_EXPR, void_type_node, tempVarDecl, addExpr);
	append_to_statement_list_force (modifyExpr, &stmtList);
	tree bindExpr = build3 (BIND_EXPR, void_type_node, varDecl, stmtList, NULL);
	tree targetExpr= build4 (TARGET_EXPR, pointerType, tempVarDecl,bindExpr, NULL_TREE, NULL_TREE);
	aet_print_tree(targetExpr);
	return targetExpr;
}

static tree convertRealOrIntegerCstToPointer_new(location_t loc,tree realOrIntCst)
{
  tree type= TREE_TYPE(realOrIntCst);
  tree pointerType=build_pointer_type(type);
  tree varDecl=build_decl(loc, VAR_DECL, NULL_TREE, pointerType);
  DECL_ARTIFICIAL (varDecl)=1;
  TREE_USED (varDecl)=1;
    tree indextype= build_index_type (size_int(0));
	tree arrayType=build_array_type(type,indextype);
	TYPE_SIZE (arrayType)=build_int_cst (integer_type_node, 0);
	tree bindVarDecl = build_decl (0,VAR_DECL,aet_utils_create_ident("realOrIntCstToPointer"), arrayType);

 TREE_USED (bindVarDecl)=1;
 DECL_EXTERNAL(bindVarDecl)=0;
 TREE_STATIC(bindVarDecl)=0;
 TREE_PUBLIC(bindVarDecl)=0;
 DECL_CONTEXT(bindVarDecl)=current_function_decl;

 tree stmtList=alloc_stmt_list();
 tree stmt0 = build_stmt (loc, DECL_EXPR, void_type_node);
 append_to_statement_list_force (stmt0, &stmtList);

	{
		   unsigned HOST_WIDE_INT value;
		   value=0;
		   tree rvalue=realOrIntCst;//build_int_cst (integer_type_node, value);
		   tree index=build_int_cst(integer_type_node,0);
		   tree result = build4 (ARRAY_REF, type, bindVarDecl, index, NULL_TREE,NULL_TREE);
		   tree stmt2 = build_modify_expr (loc, result, TREE_TYPE(rvalue),NOP_EXPR,loc,rvalue,TREE_TYPE(rvalue));
		   append_to_statement_list_force (stmt2, &stmtList);
	}

   tree op0 = build1 (ADDR_EXPR, build_pointer_type(arrayType), bindVarDecl);// @104 op component_ref
   tree noexpr = build1 (NOP_EXPR, pointerType,op0);//@47 strcpy的第一个参数

   //tree stmt3 = build_modify_expr (loc, varDecl, void_type_node,NOP_EXPR,loc,bindVarDecl,TREE_TYPE(bindVarDecl));
   //tree stmt3 = build_modify_expr (loc, varDecl, void_type_node,NOP_EXPR,loc,noexpr,TREE_TYPE(noexpr));
  // tree stmt3 = build_modify_expr (loc, varDecl, void_type_node,NOP_EXPR,loc,noexpr,TREE_TYPE(noexpr));
   tree stmt3 = build_modify_expr (loc, varDecl, NULL_TREE,NOP_EXPR,loc,noexpr,NULL_TREE);
   //printf("这是临时的 TREE_TYPE(stmt3)=void_type_node\n");
   TREE_TYPE(stmt3)=void_type_node;
   append_to_statement_list_force (stmt3, &stmtList);
   tree bind = build3 (BIND_EXPR, void_type_node, bindVarDecl, stmtList, NULL_TREE);
   tree target = build4 (TARGET_EXPR, pointerType, varDecl, bind, NULL_TREE, NULL_TREE);
   return target;
}




static int tempVarNameCount=0;

static tree convertRealOrIntegerCstToPointer(location_t loc,tree realOrIntCst,nboolean replace)
{
	if(replace){
//		char varName[128];
//		sprintf(varName,"realOrIntCstToPointer_%d",tempVarNameCount++);
//		tree id=aet_utils_create_ident(varName);
//		tree numberType=TREE_TYPE(realOrIntCst);
//		tree varDecl=build_decl (loc, VAR_DECL, id, numberType);
//		DECL_INITIAL(varDecl)=realOrIntCst;
//		DECL_CONTEXT(varDecl)=current_function_decl;
//		varDecl = pushdecl (varDecl);
//		//add_stmt (build_stmt (DECL_SOURCE_LOCATION (varDecl),DECL_EXPR, varDecl));
//		finish_decl (varDecl, loc, realOrIntCst,numberType, NULL_TREE);
//		tree xx=lookup_name(id);
//		 printf("convertRealOrIntegerCstToPointer 加入新语句 %p\n",xx);
//		tree pointerType=build_pointer_type(numberType);
//		tree addExpr= build1 (ADDR_EXPR, pointerType, varDecl);
//		return addExpr;
		tree numberType=TREE_TYPE(realOrIntCst);
		char *typeName=NULL;
		class_util_get_type_name(numberType,&typeName);
        char *codes=n_strdup_printf("({%s realOrIntCstToPointer[0];realOrIntCstToPointer[0]=3.213;realOrIntCstToPointer;}))\n",typeName);
		tree target=generic_util_create_target(codes);
	    tree bind=TREE_OPERAND (target, 1);
	    tree body=TREE_OPERAND (bind, 1);
	    tree_stmt_iterator it;
	    int i=0;
		for (i = 0, it = tsi_start (body); !tsi_end_p (it); tsi_next (&it), i++){
			if(i==1){
			   tree modify= tsi_stmt (it);
			   TREE_OPERAND (modify, 1)=realOrIntCst;
			}
		}
		return target;

	}else{
		printf("convertRealOrIntegerCstToPointer 不加入新语句--\n");
		//return convertRealOrIntegerCstToPointer00(loc,realOrIntCst);
		return convertRealOrIntegerCstToPointer_new(loc,realOrIntCst);
	}

}


/**
 * 创建real转指针。
 * 从变量转指针，如:float value=5.1;setData(value);
 * 把实参转成形如：&value的地址;
 */
static tree convertRealorIntVarToPointer(location_t loc,tree var)
{
    tree realOrIntType=TREE_TYPE(var);
	tree pointerType=build_pointer_type(realOrIntType);
	tree addExpr= build1 (ADDR_EXPR, pointerType, var);
	aet_print_tree(addExpr);
	return addExpr;
}

static tree convertNopExprToPointer(location_t loc,tree nopExpr)
{
    tree nopExprType=TREE_TYPE(nopExpr);
    tree op0=TREE_OPERAND (nopExpr, 0);
	if(TREE_CODE(nopExprType)==INTEGER_TYPE){
	   if(TREE_CODE(op0)==VAR_DECL){
		  tree vtype=TREE_TYPE(op0);
		  if(TREE_CODE(vtype)==INTEGER_TYPE){
				tree pointerType=build_pointer_type(vtype);
				tree addExpr= build1 (ADDR_EXPR, pointerType, op0);
				aet_print_tree(addExpr);
				return addExpr;
		  }else{
			  error_at(loc,"不能处理NOP_EXPR的OP是变量的类型。%qs",get_tree_code_name(TREE_CODE(vtype)));
		  }


	   }else{
		  error_at(loc,"不能处理NOP_EXPR的OP。%qs",get_tree_code_name(TREE_CODE(op0)));

	   }
	}else{
		error_at(loc,"不能处理NOP_EXPR的类型。%qs",get_tree_code_name(TREE_CODE(nopExprType)));
	}
	return NULL_TREE;
}



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

  return aet_c_c_build_qualified_type (type,
				 TYPE_QUALS_NO_ADDR_SPACE (type)
				 | TYPE_QUALS_NO_ADDR_SPACE_NO_ATOMIC (like)
				 | ENCODE_QUAL_ADDR_SPACE (as_common));
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

static tree aet_c_composite_type (tree t1, tree t2)
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
	tree target = aet_c_composite_type (pointed_to_1, pointed_to_2);
        t1 = build_pointer_type_for_mode (target, TYPE_MODE (t1), false);
	t1 = build_type_attribute_variant (t1, attributes);
	return qualify_type (t1, t2);
      }

    case ARRAY_TYPE:
      {
	tree elt = aet_c_composite_type (TREE_TYPE (t1), TREE_TYPE (t2));
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
	unqual_elt = aet_c_c_build_qualified_type (elt, TYPE_UNQUALIFIED);
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
	t1 = aet_c_c_build_qualified_type (t1, quals);
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
	tree valtype = aet_c_composite_type (TREE_TYPE (t1), TREE_TYPE (t2));
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
		    if (aet_c_comptypes (mv3, mv2))
		      {
			TREE_VALUE (n) = aet_c_composite_type (TREE_TYPE (memb),
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
		    if (aet_c_comptypes (mv3, mv1))
		      {
			TREE_VALUE (n) = aet_c_composite_type (TREE_TYPE (memb),
							 TREE_VALUE (p1));
			pedwarn (input_location, OPT_Wpedantic,
				 "function types not truly compatible in ISO C");
			goto parm_done;
		      }
		  }
	      }
	    TREE_VALUE (n) = aet_c_composite_type (TREE_VALUE (p1), TREE_VALUE (p2));
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

static int aet_c_comptypes (tree type1, tree type2)
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

static void printNode(tree node)
{
      if(!node || node==NULL_TREE){
    	  printf("comptypes_internal null tree\n");
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
    	printf("comptypes_internal 为什么进这里 %s %s %d\n",__FILE__,__FUNCTION__,__LINE__);
    	//printNode(t1);
    	//printNode(t2);
      if (val != 1 && !aet_c_same_translation_unit_p (t1, t2))
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
    val_ped = aet_c_comptypes (mvl, mvr);

  /* Qualifiers on element types of array types that are
     pointer targets are lost by taking their TYPE_MAIN_VARIANT.  */

  mvl = (TYPE_ATOMIC (strip_array_types (mvl))
	 ? aet_c_c_build_qualified_type (TYPE_MAIN_VARIANT (mvl), TYPE_QUAL_ATOMIC)
	 : TYPE_MAIN_VARIANT (mvl));

  mvr = (TYPE_ATOMIC (strip_array_types (mvr))
	 ? aet_c_c_build_qualified_type (TYPE_MAIN_VARIANT (mvr), TYPE_QUAL_ATOMIC)
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

static bool aet_c_same_translation_unit_p (const_tree t1, const_tree t2)
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
  //printf("t2 is null? 11 %s %d %d\n",get_tree_code_name(TREE_CODE(t2)),TREE_CODE(t2),TRANSLATION_UNIT_DECL);
  //printf("t2 is null? 22 %p %d\n",t2,TREE_CODE_CLASS (TREE_CODE (t2)));

  while (t2 && TREE_CODE (t2) != TRANSLATION_UNIT_DECL){
    switch (TREE_CODE_CLASS (TREE_CODE (t2))){
      case tcc_declaration:
	    t2 = DECL_CONTEXT (t2);
	    printf("tcc_declaration %p\n",t2);
	    break;
      case tcc_type:
	    t2 = TYPE_CONTEXT (t2);
	    printf("tcc_type %p %s\n",t2,t2?get_tree_code_name(TREE_CODE(t2)):"NULL");

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
	       ? aet_c_c_build_qualified_type (TYPE_MAIN_VARIANT (mv1),
					 TYPE_QUAL_ATOMIC)
	       : TYPE_MAIN_VARIANT (mv1));
      if (mv2 && mv2 != error_mark_node && TREE_CODE (mv2) != ARRAY_TYPE)
	mv2 = (TYPE_ATOMIC (mv2)
	       ? aet_c_c_build_qualified_type (TYPE_MAIN_VARIANT (mv2),
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
			   ? aet_c_c_build_qualified_type (TYPE_MAIN_VARIANT (mv3),
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
			   ? aet_c_c_build_qualified_type (TYPE_MAIN_VARIANT (mv3),
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

/* Return nonzero if REF is an lvalue valid for this language.
   Lvalues can be assigned, unless their type has TYPE_READONLY.
   Lvalues can have their address taken, unless they have C_DECL_REGISTER.  */

static bool aet_c_lvalue_p (const_tree ref)
{
  const enum tree_code code = TREE_CODE (ref);

  switch (code)
    {
    case REALPART_EXPR:
    case IMAGPART_EXPR:
    case COMPONENT_REF:
      return aet_c_lvalue_p (TREE_OPERAND (ref, 0));

    case C_MAYBE_CONST_EXPR:
      return aet_c_lvalue_p (TREE_OPERAND (ref, 1));

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




/* Mark EXP saying that we need to be able to take the
   address of it; it should not be allocated in a register.
   Returns true if successful.  ARRAY_REF_P is true if this
   is for ARRAY_REF construction - in that case we don't want
   to look through VIEW_CONVERT_EXPR from VECTOR_TYPE to ARRAY_TYPE,
   it is fine to use ARRAY_REFs for vector subscripts on vector
   register variables.  */

static bool aet_c_c_mark_addressable (tree exp, bool array_ref_p)
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
			? aet_c_c_build_qualified_type (TREE_TYPE (field),
						  TYPE_QUAL_ATOMIC)
			: TYPE_MAIN_VARIANT (TREE_TYPE (field)));
      if (DECL_NAME (field) == NULL
	  && aet_c_comptypes (type, fieldtype))
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
		   ? aet_c_c_build_qualified_type (TREE_TYPE (type),
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
			? aet_c_c_build_qualified_type (TREE_TYPE (field),
						  TYPE_QUAL_ATOMIC)
			: TYPE_MAIN_VARIANT (TREE_TYPE (field)));
      if (aet_c_comptypes (lhs_main_type, fieldtype))
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



/* Make a variant type in the proper way for C/C++, propagating qualifiers
   down to the element type of an array.  If ORIG_QUAL_TYPE is not
   NULL, then it should be used as the qualified type
   ORIG_QUAL_INDIRECT levels down in array type derivation (to
   preserve information about the typedef name from which an array
   type was derived).  */

static tree aet_c_c_build_qualified_type (tree type, int type_quals, tree orig_qual_type,size_t orig_qual_indirect)
{
  if (type == error_mark_node)
    return type;

  if (TREE_CODE (type) == ARRAY_TYPE)
    {
      tree t;
      tree element_type = aet_c_c_build_qualified_type (TREE_TYPE (type),
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
                = aet_c_c_build_qualified_type (unqualified_canon, type_quals);
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
static bool aet_c_reject_gcc_builtin (const_tree expr, location_t loc /* = UNKNOWN_LOCATION */)
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
      error_at (loc, "built-in function %qE must be directly called", expr);
      return true;
    }

  return false;
}


static tree convertForAssignment(location_t location, location_t expr_loc, tree type,tree rhs,bool null_pointer_constant,nboolean replace)
{
  enum tree_code codel = TREE_CODE (type);
  tree orig_rhs = rhs;
  tree rhstype;
  enum tree_code coder;
  tree rname = NULL_TREE;
  bool objc_ok = false;
  n_debug("convertForAssignment 00 start----type:%s rhs:%s ",get_tree_code_name(TREE_CODE(type)),get_tree_code_name(TREE_CODE(rhs)));
    int warncount=0;
    if (TREE_CODE (rhs) == EXCESS_PRECISION_EXPR)
       rhs = TREE_OPERAND (rhs, 0);

    rhstype = TREE_TYPE (rhs);
    coder = TREE_CODE (rhstype);

    if (coder == ERROR_MARK){
  	   error_at(location,"error_or_warn_unknown");
       return error_mark_node;
    }

    tree checktype =  rhstype;
    if (checktype != error_mark_node && TREE_CODE (type) == ENUMERAL_TYPE && TYPE_MAIN_VARIANT (checktype) != TYPE_MAIN_VARIANT (type))
	   warncount++;

	if (checktype != error_mark_node && TREE_CODE (checktype) == ENUMERAL_TYPE &&
			TREE_CODE (type) == ENUMERAL_TYPE && TYPE_MAIN_VARIANT (checktype) != TYPE_MAIN_VARIANT (type)){
		warncount++;
    }


    if (TYPE_MAIN_VARIANT (type) == TYPE_MAIN_VARIANT (rhstype)){
        n_debug("convertForAssignment 11 ---+++-实参与型参匹配  %s %s  %s",
			  get_tree_code_name(TREE_CODE(type)),get_tree_code_name(TREE_CODE(rhstype)),
			  get_tree_code_name(TREE_CODE(TYPE_MAIN_VARIANT (type))));
        aet_print_tree(rhs);
       return rhs;
    }

    if (coder == VOID_TYPE){
    	n_info("convertForAssignment 22 ----错误 coder == VOID_TYPE");
        return error_mark_node;
    }
    rhs = aet_require_complete_type (location, rhs);
    if (rhs == error_mark_node)
       return error_mark_node;

    if (coder == POINTER_TYPE && aet_c_reject_gcc_builtin (rhs,UNKNOWN_LOCATION))
       return error_mark_node;
    n_debug("convertForAssignment 33  codel:%s coder:%s ",get_tree_code_name(codel),get_tree_code_name(coder));

  /* A non-reference type can convert to a reference.  This handles
     va_start, va_copy and possibly port built-ins.  */
    if (codel == REFERENCE_TYPE && coder != REFERENCE_TYPE){
    	n_debug("convertForAssignment 44 在这里递归调用convertForAssignment codel == REFERENCE_TYPE && coder != REFERENCE_TYPE");
       if (!aet_c_lvalue_p (rhs)){
	      return error_mark_node;
	   }
       if (!aet_c_c_mark_addressable (rhs))
	      return error_mark_node;
       rhs = build1 (ADDR_EXPR, build_pointer_type (TREE_TYPE (rhs)), rhs);
       SET_EXPR_LOCATION (rhs, location);
       rhs = convertForAssignment (location, expr_loc,
				    build_pointer_type (TREE_TYPE (type)),
				    rhs, null_pointer_constant,replace);
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
    	n_debug("convertForAssignment 55----cccc codel %s %s %s codel:%d coder:%d orig_rhs:%d",
			  get_tree_code_name(codel),get_tree_code_name(coder),get_tree_code_name(TREE_CODE(orig_rhs)),codel,coder,TREE_CODE(orig_rhs));
	    if((codel== INTEGER_TYPE && coder==REAL_TYPE) || (codel== REAL_TYPE && coder==INTEGER_TYPE)){
  	      n_warning("convertForAssignment 66 int_to_real_or_real_to_int");
  		   warncount++;
	    }
        bool save = in_late_binary_op;
        if (codel == BOOLEAN_TYPE || codel == COMPLEX_TYPE  || (coder == REAL_TYPE  &&
    		  (codel == INTEGER_TYPE || codel == ENUMERAL_TYPE) && sanitize_flags_p (SANITIZE_FLOAT_CAST)))
	        in_late_binary_op = true;
        tree ret = convert_and_check (expr_loc != UNKNOWN_LOCATION ? expr_loc : location, type, orig_rhs);
        in_late_binary_op = save;
        return ret;
    }
    n_debug("convertForAssignment 77 ");

  /* Aggregates in different TUs might need conversion.  */
    if ((codel == RECORD_TYPE || codel == UNION_TYPE) && codel == coder && aet_c_comptypes (type, rhstype))
       return convert_and_check (expr_loc != UNKNOWN_LOCATION? expr_loc : location, type, rhs);
    n_debug("convertForAssignment 88 ");

    /* Conversion to a transparent union or record from its member types.
     This applies only to function arguments.  */
    if (((codel == UNION_TYPE || codel == RECORD_TYPE) && TYPE_TRANSPARENT_AGGR (type))/* zclei && errtype == ic_argpass*/){
    	n_debug("convertForAssignment 99 ((codel == UNION_TYPE || codel == RECORD_TYPE)");
       tree memb, marginal_memb = NULL_TREE;
       for (memb = TYPE_FIELDS (type); memb ; memb = DECL_CHAIN (memb)){
	      tree memb_type = TREE_TYPE (memb);
	      if (aet_c_comptypes (TYPE_MAIN_VARIANT (memb_type),TYPE_MAIN_VARIANT (rhstype)))
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
		   		     warncount++;
		     }else if (TYPE_QUALS_NO_ADDR_SPACE (ttr) & ~TYPE_QUALS_NO_ADDR_SPACE (ttl))
	   		     warncount++;
	         memb = marginal_memb;
	      }//end !memb

	      rhs = fold_convert_loc (location, TREE_TYPE (memb), rhs);
	      return build_constructor_single (type, memb, rhs);
	   }// end memb || marginal_memb
    }else if ((codel == POINTER_TYPE || codel == REFERENCE_TYPE) && (coder == codel)){   /* Conversions among pointers */
		  /* If RHS refers to a built-in declared without a prototype
		 BLTIN is the declaration of the built-in with a prototype
		 and RHSTYPE is set to the actual type of the built-in.  */
    	n_debug("convertForAssignment 100 ((codel == UNION_TYPE || codel == RECORD_TYPE)");

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
	      mvl = (TYPE_ATOMIC (mvl)? aet_c_c_build_qualified_type (TYPE_MAIN_VARIANT (mvl),TYPE_QUAL_ATOMIC): TYPE_MAIN_VARIANT (mvl));
       if (TREE_CODE (mvr) != ARRAY_TYPE)
	      mvr = (TYPE_ATOMIC (mvr) ? aet_c_c_build_qualified_type (TYPE_MAIN_VARIANT (mvr),TYPE_QUAL_ATOMIC): TYPE_MAIN_VARIANT (mvr));
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
       n_debug("convertForAssignment 101 !null_pointer_constant_p (rhs)  && asr != asl && !targetm.addr_space.subset_p (asr, asl)");
       if (VOID_TYPE_P (ttr) && rhs != null_pointer_node && !VOID_TYPE_P (ttl))
 		     warncount++;
      /* See if the pointers point to incompatible address spaces.  */
       asl = TYPE_ADDR_SPACE (ttl);
       asr = TYPE_ADDR_SPACE (ttr);
       if (!null_pointer_constant_p (rhs) && asr != asl && !targetm.addr_space.subset_p (asr, asl)){
    	   n_warning("convertForAssignment 102 !null_pointer_constant_p (rhs)  && asr != asl && !targetm.addr_space.subset_p (asr, asl)");
	      return error_mark_node;
	   }
       n_debug("convertForAssignment 103 !null_pointer_constant_p (rhs)  && asr != asl && !targetm.addr_space.subset_p (asr, asl)");

      /* Check if the right-hand side has a format attribute but the
	 left-hand side doesn't.  */
       if (warn_suggest_attribute_format  && check_missing_format_attribute (type, rhstype)){
    	   n_debug("convertForAssignment 104 arn_suggest_attribute_format  && check_missing_format_attribute (type, rhstype)");
		   warncount++;

	   }
       n_debug("convert_for_assignment 105 ");

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
    	   n_debug("convertForAssignment 105 VOID_TYPE_P (ttl) && !TYPE_ATOMIC (ttl))");

	      if (TREE_CODE (ttr) == ARRAY_TYPE){
	         ttr = strip_array_types (ttr);
	         ttl = strip_array_types (ttl);
	         if (TYPE_QUALS_NO_ADDR_SPACE_NO_ATOMIC (ttr) & ~TYPE_QUALS_NO_ADDR_SPACE_NO_ATOMIC (ttl))
	  		    warncount++;
          }else if (pedantic  && ((VOID_TYPE_P (ttl) && TREE_CODE (ttr) == FUNCTION_TYPE)
		     ||(VOID_TYPE_P (ttr)&& !null_pointer_constant && TREE_CODE (ttl) == FUNCTION_TYPE)))
   		      warncount++;
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
	  		      warncount++;
	         }else if (VOID_TYPE_P (ttl) || VOID_TYPE_P (ttr) || target_cmp) /* If this is not a case of ignoring a mismatch in signedness,no warning.  */
		        ;
	           /* If there is a mismatch, do warn.  */
	         else if (warn_pointer_sign)
	  		    warncount++;

	      }else if (TREE_CODE (ttl) == FUNCTION_TYPE && TREE_CODE (ttr) == FUNCTION_TYPE) {
			  /* Because const and volatile on functions are restrictions
			 that say the function will not do certain things,
			 it is okay to use a const or volatile function
			 where an ordinary one is wanted, but not vice-versa.  */
	         if (TYPE_QUALS_NO_ADDR_SPACE (ttl) & ~TYPE_QUALS_NO_ADDR_SPACE (ttr))
	  		    warncount++;
	      }
	   }else if (!objc_ok){/* Avoid warning about the volatile ObjC EH puts on decls.  */
          n_debug("convertForAssignment 107 ");
		   warncount++;
	   }
       n_debug("convertForAssignment 108 ");

		  /* If RHS isn't an address, check pointer or array of packed
		 struct or union.  */
       warn_for_address_or_pointer_of_packed_member (type, orig_rhs);
       return aet_convert (type, rhs);
    }else if (codel == POINTER_TYPE && coder == ARRAY_TYPE){
		  /* ??? This should not be an error when inlining calls to
		 unprototyped functions.  */
       n_debug("convertForAssignment 109 invalid use of non-lvalue array");
       return error_mark_node;
    }else if (codel == POINTER_TYPE && coder == INTEGER_TYPE){
		  /* An explicit constant 0 can convert to a pointer,
		 or one that results from arithmetic, even including
		 a cast to integer type.  */
       n_debug("convertForAssignment 110 泛型 从 INTEGER_TYPE 转指针 替换吗:%d",replace);
       aet_print_tree(rhs);
	   if(TREE_CODE(rhs)==INTEGER_CST){
		   n_debug("convertForAssignment 110 --XXX00 泛型 从常数转integer_type的指针");
		   return convertRealOrIntegerCstToPointer(location,rhs,replace);
	   }else if(TREE_CODE(rhs)==VAR_DECL){
		   n_debug("convertForAssignment 110 --XXX11 从int类型的变量转integer_type的指针");
		   return convertRealorIntVarToPointer(location,rhs);
	   }else if(TREE_CODE(rhs)==NOP_EXPR){
		   n_debug("convertForAssignment 111 --XXX11 从char short类型的变量转char short的指针");
		   return convertNopExprToPointer(location,rhs);
	   }else{
		   error("不能从%qS转到指针",rhs);
	   }
       //return aet_convert (type, rhs);
    }else if (codel == INTEGER_TYPE && coder == POINTER_TYPE){
       n_debug("convertForAssignment 111 ");
       return aet_convert (type, rhs);
    }else if (codel == BOOLEAN_TYPE && coder == POINTER_TYPE){
       tree ret;
       bool save = in_late_binary_op;
       in_late_binary_op = true;
       ret = aet_convert (type, rhs);
       in_late_binary_op = save;
       return ret;
    }else if (codel == POINTER_TYPE && coder == REAL_TYPE){
       n_debug("convertForAssignment 112从 real_type转指针 替换吗:%d",replace);
       if(TREE_CODE(rhs)==REAL_CST){
           n_debug("convertForAssignment 112 --XXX00 从常数转real_type的指针");
           return convertRealOrIntegerCstToPointer(location,rhs,replace);
       }else if(TREE_CODE(rhs)==VAR_DECL){
           n_debug("convertForAssignment 112 --XXX11 从real类型的变量转real_type的指针");
           return convertRealorIntVarToPointer(location,rhs);
       }else{
    	   error("不能从%qS转到指针",rhs);
       }
    }

    n_debug("convertForAssignment 113 结束了返回 error_mark_node");
    return error_mark_node;
}

/**
 * replace:创建临时变量。如: setData(100) 变成了:int realOrIntCstToPointer_1=100; setData(&realOrIntCstToPointer);
 */
tree generic_conv_convert_argument (location_t ploc, tree fundecl,tree type, tree val,nboolean npc, nboolean excess_precision,nboolean replace)
{
  /* Formal parm type is specified by a function prototype.  */

    if (type == error_mark_node || !COMPLETE_TYPE_P (type)){
       //zclei error_at (ploc, "type of formal parameter %d is incomplete",parmnum + 1);
	   argsFuncsInfo.errors[argsFuncsInfo.errorCount++]=type_of_formal_parameter_xx_is_incomplete;
       return error_mark_node;
    }
    tree valtype=TREE_TYPE(val);

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
    tree parmval = convertForAssignment(ploc, ploc, type,val,npc,replace);

    if (targetm.calls.promote_prototypes (fundecl ? TREE_TYPE (fundecl) : 0)  &&
    		INTEGRAL_TYPE_P (type) && (TYPE_PRECISION (type) < TYPE_PRECISION (integer_type_node)))
       parmval = aet_default_conversion (parmval);
    return parmval;
}



