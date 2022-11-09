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
#include "function.h"
#include "tree-core.h"
#include "tree.h"
#include "stringpool.h"
#include "attribs.h"
#include "c/c-tree.h"
#include "c-family/c-pragma.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "c-aet.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"
#include "aetinfo.h"
#include "aetmangle.h"
#include "aetutils.h"
#include "aet-typeck.h"
#include "classmgr.h"
#include "aetprinttree.h"
#include "classutil.h"
#include "enumparser.h"


static void write_encoding (AetMangle *self,const tree decl);
static void write_name (AetMangle *self,tree decl, const int ignore_local_scope);
static void write_type (AetMangle *self,tree type);
static void write_prefix (AetMangle *self,const tree node);
static void write_function_type (AetMangle *self,const tree type);
static void add_substitution (AetMangle *self,tree node);
static int find_substitution (AetMangle *self,tree node);
static tree canonicalize_for_substitution (tree node);
static int cp_type_quals (const_tree type);

#define CP_TYPE_RESTRICT_P(NODE)   ((cp_type_quals (NODE) & TYPE_QUAL_RESTRICT) != 0)
#define CP_TYPE_VOLATILE_P(NODE)   ((cp_type_quals (NODE) & TYPE_QUAL_VOLATILE) != 0)
#define CP_TYPE_CONST_P(NODE)	  ((cp_type_quals (NODE) & TYPE_QUAL_CONST) != 0)
#define CP_DECL_CONTEXT(NODE)  (!DECL_FILE_SCOPE_P (NODE) ? DECL_CONTEXT (NODE) : global_namespace)
#define DECL_FILE_SCOPE_P(EXP) SCOPE_FILE_SCOPE_P (DECL_CONTEXT (EXP))
#define DECL_DISCRIMINATOR_P(NODE)	(TREE_CODE (NODE) == VAR_DECL && TREE_STATIC (NODE))
#define DECL_NONSTATIC_MEMBER_FUNCTION_P(NODE)   (TREE_CODE (TREE_TYPE (NODE)) == METHOD_TYPE)
#define DECL_VOLATILE_MEMFUNC_P(NODE)					 \
  (DECL_NONSTATIC_MEMBER_FUNCTION_P (NODE)				 \
   && CP_TYPE_VOLATILE_P (TREE_TYPE (TREE_VALUE				 \
				  (TYPE_ARG_TYPES (TREE_TYPE (NODE))))))
#define TYPE_NAME_STRING(NODE) (IDENTIFIER_POINTER (TYPE_IDENTIFIER (NODE)))
#define TYPE_NAME_LENGTH(NODE) (IDENTIFIER_LENGTH (TYPE_IDENTIFIER (NODE)))
/* Returns true if NODE is a reference.  */
#define TYPE_REF_P(NODE)		  (TREE_CODE (NODE) == REFERENCE_TYPE)
#define DECL_CONST_MEMFUNC_P(NODE)					 \
  (DECL_NONSTATIC_MEMBER_FUNCTION_P (NODE)				 \
   && CP_TYPE_CONST_P (TREE_TYPE (TREE_VALUE				 \
				  (TYPE_ARG_TYPES (TREE_TYPE (NODE))))))

/* Nonzero for FUNCTION_DECL means that this member function
   has `this' as volatile X *const.  */
#define DECL_VOLATILE_MEMFUNC_P(NODE)					 \
  (DECL_NONSTATIC_MEMBER_FUNCTION_P (NODE)				 \
   && CP_TYPE_VOLATILE_P (TREE_TYPE (TREE_VALUE				 \
				  (TYPE_ARG_TYPES (TREE_TYPE (NODE))))))
#define FUNCTION_REF_QUALIFIED(NODE)   TREE_LANG_FLAG_4 (FUNC_OR_METHOD_CHECK (NODE))

#define FUNCTION_RVALUE_QUALIFIED(NODE)   TREE_LANG_FLAG_5 (FUNC_OR_METHOD_CHECK (NODE))
#define TYPENAME_TYPE_FULLNAME(NODE)  (TYPE_VALUES_RAW (TYPENAME_TYPE_CHECK (NODE)))

#define write_unsigned_number(SELF,NUMBER)	 write_number (SELF,(NUMBER), /*unsigned_p=*/1, 10)
#define IDENTIFIER_LAMBDA_P(NODE)  (IDENTIFIER_NODE_CHECK(NODE)->base.protected_flag)



#define DECL_DECOMPOSITION_P(NODE) \
  (VAR_P (NODE) && DECL_LANG_SPECIFIC (NODE)			\
   ? DECL_LANG_SPECIFIC (NODE)->u.base.selector == lds_decomp		\
   : false)

#define TYPE_UNNAMED_P(NODE)					\
  (TYPE_ANON_P (NODE)						\
   && !IDENTIFIER_LAMBDA_P (TYPE_LINKAGE_IDENTIFIER (NODE)))
#define TYPE_LINKAGE_IDENTIFIER(NODE) \
  (TYPE_IDENTIFIER (TYPE_MAIN_VARIANT (NODE)))
/* Based off of TYPE_UNNAMED_P.  */
#define LAMBDA_TYPE_P(NODE)			\
  (TREE_CODE (NODE) == RECORD_TYPE		\
   && TYPE_LINKAGE_IDENTIFIER (NODE)				\
   && IDENTIFIER_LAMBDA_P (TYPE_LINKAGE_IDENTIFIER (NODE)))

#define TYPE_ANON_P(NODE)					\
  (TYPE_LINKAGE_IDENTIFIER (NODE)				\
   && IDENTIFIER_ANON_P (TYPE_LINKAGE_IDENTIFIER (NODE)))
#define FUNCTION_FIRST_USER_PARMTYPE(NODE)   skip_artificial_parms_for ((NODE), TYPE_ARG_TYPES (TREE_TYPE (NODE)))

#define TYPE_PTRMEMFUNC_FLAG(NODE)   (TYPE_LANG_FLAG_2 (RECORD_TYPE_CHECK (NODE)))
#define TYPE_PTRDATAMEM_P(NODE)			  (TREE_CODE (NODE) == OFFSET_TYPE)
#define TYPE_PTRMEMFUNC_P(NODE)		 (TREE_CODE (NODE) == RECORD_TYPE	   && TYPE_PTRMEMFUNC_FLAG (NODE))
#define TYPE_PTR_P(NODE)			  (TREE_CODE (NODE) == POINTER_TYPE)
#define DECLTYPE_TYPE_ID_EXPR_OR_MEMBER_ACCESS_P(NODE)   (DECLTYPE_TYPE_CHECK (NODE))->type_common.string_flag
#define DECLTYPE_TYPE_EXPR(NODE) (TYPE_VALUES_RAW (DECLTYPE_TYPE_CHECK (NODE)))


static const char integer_type_codes[itk_none] =
{
  'c',  /* itk_char */
  'a',  /* itk_signed_char */
  'h',  /* itk_unsigned_char */
  's',  /* itk_short */
  't',  /* itk_unsigned_short */
  'i',  /* itk_int */
  'j',  /* itk_unsigned_int */
  'l',  /* itk_long */
  'm',  /* itk_unsigned_long */
  'x',  /* itk_long_long */
  'y',  /* itk_unsigned_long_long */
  /* __intN types are handled separately */
  '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'
};



static void freeClass_cb(npointer userData)
{
  printf("aet_managle free free %p\n",userData);
}

static void aetInit(AetMangle *self)
{
	self->strBuf=n_string_new("");
	vec_alloc (self->substitutions, 0);
}



static tree decl_mangling_context (AetMangle *self,tree decl)
{
	tree tcontext;
	bool fileScope=DECL_FILE_SCOPE_P (decl);
	if(!fileScope)
		tcontext=DECL_CONTEXT(decl);
	else
		tcontext=NULL_TREE;
	return NULL_TREE;
}

static void write_char(AetMangle *self,char type)
{
	n_string_append_c(self->strBuf,type);
}

static void write_chars(AetMangle *self,char *buf,int len)
{
	  n_string_append_len(self->strBuf,buf, len);

}

static void write_string(AetMangle *self,char *buf)
{
	n_string_append(self->strBuf,buf);
}

static int hwint_to_ascii (unsigned HOST_WIDE_INT number, const unsigned int base,char *buffer, const unsigned int min_digits)
{
  static const char base_digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  unsigned digits = 0;

  while (number)
    {
      unsigned HOST_WIDE_INT d = number / base;

      *--buffer = base_digits[number - d * base];
      digits++;
      number = d;
    }
  while (digits < min_digits)
    {
      *--buffer = base_digits[0];
      digits++;
    }
  return digits;
}


static void write_number (AetMangle *self,unsigned HOST_WIDE_INT number, const int unsigned_p,const unsigned int base)
{
  char buffer[sizeof (HOST_WIDE_INT) * 8];
  unsigned count = 0;

  if (!unsigned_p && (HOST_WIDE_INT) number < 0)
    {
      write_char (self,'n');
      number = -((HOST_WIDE_INT) number);
    }
  count = hwint_to_ascii (number, base, buffer + sizeof (buffer), 1);
  write_chars (self,buffer + sizeof (buffer) - count, count);
}

static void write_identifier (AetMangle *self,const char *identifier)
{
   write_string (self,identifier);
}

static void  write_source_name (AetMangle *self,tree identifier)
{
   ClassInfo *info=class_mgr_get_class_info_by_underline_sys_name(class_mgr_get(),IDENTIFIER_POINTER (identifier));
   if(info!=NULL){
	  write_unsigned_number (self,strlen (info->className.userName));
	  write_identifier (self,info->className.userName);
   }else{
      write_unsigned_number (self,IDENTIFIER_LENGTH (identifier));
      write_identifier (self,IDENTIFIER_POINTER (identifier));
   }
}

static void write_unqualified_id (AetMangle *self,tree identifier)
{
    write_source_name (self,identifier);
}

struct GTY(()) lang_identifier {
  struct c_common_identifier common_id;
  struct c_binding *symbol_binding; /* vars, funcs, constants, typedefs */
  struct c_binding *tag_binding;    /* struct/union/enum tags */
  struct c_binding *label_binding;  /* labels */
};


inline lang_identifier* identifier_p (tree t)
{
  if (TREE_CODE (t) == IDENTIFIER_NODE)
    return (lang_identifier*) t;
  return NULL;
}


/* As the list of identifiers for the structured binding declaration
   DECL is likely gone, try to recover the DC <source-name>+ E portion
   from its mangled name.  Return pointer to the DC and set len to
   the length up to and including the terminating E.  On failure
   return NULL.  */

static const char *find_decomp_unqualified_name (tree decl, size_t *len)
{
  const char *p = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl));
  const char *end = p + IDENTIFIER_LENGTH (DECL_ASSEMBLER_NAME (decl));
  bool nested = false;
  if (strncmp (p, "_Z", 2))
    return NULL;
  p += 2;
  if (!strncmp (p, "St", 2))
    p += 2;
  else if (*p == 'N')
    {
      nested = true;
      ++p;
      while (ISDIGIT (p[0]))
	{
	  char *e;
	  long num = strtol (p, &e, 10);
	  if (num >= 1 && num < end - e)
	    p = e + num;
	  else
	    break;
	}
    }
  if (strncmp (p, "DC", 2))
    return NULL;
  if (nested)
    {
      if (end[-1] != 'E')
	return NULL;
      --end;
    }
  if (end[-1] != 'E')
    return NULL;
  *len = end - p;
  return p;
}

static void write_unnamed_type_name (AetMangle *self,const tree type)
{
//  int discriminator;
//  if (TYPE_FUNCTION_SCOPE_P (type))
//    discriminator = discriminator_for_local_entity (TYPE_NAME (type));
//  else if (TYPE_CLASS_SCOPE_P (type))
//    discriminator = nested_anon_class_index (type);
//  else
//    {
//      gcc_assert (no_linkage_check (type, /*relaxed_p=*/true));
//      /* Just use the old mangling at namespace scope.  */
//      write_source_name (TYPE_IDENTIFIER (type));
//      return;
//    }
//
//  write_string ("Ut");
//  write_compact_number (discriminator);
}

static void write_closure_type_name (AetMangle *self,const tree type)
{
//  tree fn = lambda_function (type);
//  tree lambda = CLASSTYPE_LAMBDA_EXPR (type);
//  tree parms = TYPE_ARG_TYPES (TREE_TYPE (fn));
//
//  MANGLE_TRACE_TREE ("closure-type-name", type);
//
//  write_string ("Ul");
//  write_method_parms (parms, /*method_p=*/1, fn);
//  write_char ('E');
//  write_compact_number (LAMBDA_EXPR_DISCRIMINATOR (lambda));
}



static void write_unqualified_name (AetMangle *self,tree decl)
{
    if (identifier_p (decl)){
        write_unqualified_id (self,decl);
        return;
    }

    bool found = false;
    if (DECL_NAME (decl) == NULL_TREE){
       found = true;
       gcc_assert (DECL_ASSEMBLER_NAME_SET_P (decl));
       const char *decomp_str = NULL;
       size_t decomp_len = 0;
       if (VAR_P (decl) /*&& DECL_DECOMPOSITION_P (decl)*/ && DECL_NAME (decl) == NULL_TREE  /*&& DECL_NAMESPACE_SCOPE_P (decl)*/)
	      decomp_str = find_decomp_unqualified_name (decl, &decomp_len);
       if (decomp_str)
	      write_chars (self,decomp_str, decomp_len);
       else
	     write_source_name (self,DECL_ASSEMBLER_NAME (decl));
    }

     if (found)
      /* OK */;
     else if (VAR_OR_FUNCTION_DECL_P (decl) && ! TREE_PUBLIC (decl) /* && DECL_NAMESPACE_SCOPE_P (decl) */ /*&& decl_linkage (decl) == lk_internal*/){
      write_char (self,'L');
      write_source_name (self,DECL_NAME (decl));
      /* The default discriminator is 1, and that's all we ever use,
	 so there's no code to output one here.  */
    }else{
       tree type = TREE_TYPE (decl);
       if (TREE_CODE (decl) == TYPE_DECL && TYPE_UNNAMED_P (type))
           write_unnamed_type_name (self,type);
       else if (TREE_CODE (decl) == TYPE_DECL && LAMBDA_TYPE_P (type))
         write_closure_type_name (self,type);
      else{
    	 //printf("write_unqualified_name 00 ---%s\n",IDENTIFIER_POINTER(DECL_NAME (decl)));
         write_source_name (self,DECL_NAME (decl));
      }
    }


}


/* <unscoped-name> ::= <unqualified-name>
		   ::= St <unqualified-name>   # ::std::  */

static void write_unscoped_name (AetMangle *self,const tree decl)
{
    tree context =NULL_TREE;// decl_mangling_context (decl);
  /* Is DECL in ::std?  */
    if (0/*DECL_NAMESPACE_STD_P (context)*/){
      write_string (self,"St");
      write_unqualified_name (self,decl);
    }else{
      /* If not, it should be either in the global namespace, or directly
	 in a local function scope.  A lambda can also be mangled in the
	 scope of a default argument.  */
//      gcc_assert (context == global_namespace
//		  || TREE_CODE (context) == PARM_DECL
//		  || TREE_CODE (context) == FUNCTION_DECL);

      write_unqualified_name (self,decl);
   }
}

static void write_discriminator (AetMangle *self,const int discriminator)
{
  /* If discriminator is zero, don't write anything.  Otherwise...  */
  if (discriminator > 0){
      write_char (self,'_');
      if (discriminator - 1 >= 10){
	    write_char (self,'_');
	 }
     write_unsigned_number (self,discriminator - 1);
	 write_char (self,'_');
  }
}

static int discriminator_for_string_literal (tree /*function*/, tree /*string*/)
{
  /* For now, we don't discriminate amongst string literals.  */
  return 0;
}

static int discriminator_for_local_entity (tree entity)
{
  if (!DECL_LANG_SPECIFIC (entity))
    {
      /* Some decls, like __FUNCTION__, don't need a discriminator.  */
      gcc_checking_assert (DECL_ARTIFICIAL (entity));
      return 0;
    }
  else
    /* The first entity with a particular name doesn't get
       DECL_DISCRIMINATOR set up.  */
    return 0;
}

static void write_compact_number (AetMangle *self,int num)
{
  if (num > 0)
    write_unsigned_number (self,num - 1);
  printf("write_compact_number ---%d\n",num);
  write_char (self,'_');
}



static void write_local_name (AetMangle *self,tree function, const tree local_entity, const tree entity)
{
    tree parm = NULL_TREE;
    if (TREE_CODE (function) == PARM_DECL){
        parm = function;
        function = DECL_CONTEXT (parm);
    }

    write_char (self,'Z');
    write_encoding (self,function);
    write_char (self,'E');

  /* For this purpose, parameters are numbered from right-to-left.  */
  if (parm)
    {
      tree t;
      int i = 0;
      for (t = DECL_ARGUMENTS (function); t; t = DECL_CHAIN (t))
	{
	  if (t == parm)
	    i = 1;
	  else if (i)
	    ++i;
	}
      write_char (self,'d');
      write_compact_number (self,i - 1);
    }

    if (TREE_CODE (entity) == STRING_CST){
      write_char (self,'s');
      write_discriminator (self,discriminator_for_string_literal (function,entity));
    }else{
       /* Now the <entity name>.  Let write_name know its being called
	    from <local-name>, so it doesn't try to process the enclosing
	    function scope again.  */
      write_name (self,entity, /*ignore_local_scope=*/1);
      if (DECL_DISCRIMINATOR_P (local_entity) && !(TREE_CODE (local_entity) == TYPE_DECL  /*&& TYPE_ANON_P (TREE_TYPE (local_entity))*/))
	    write_discriminator (self,discriminator_for_local_entity (local_entity));
    }
}

static void write_prefix (AetMangle *self,const tree node)
{
  tree decl;
  /* Non-NULL if NODE represents a template-id.  */
  tree template_info = NULL;
  if (node == NULL /*zclei || node == global_namespace*/)
    return;

  if (TREE_CODE (node) == DECLTYPE_TYPE){
      write_type (self,node);
      return;
  }

  if (find_substitution (self,node))
    return;

   if (DECL_P (node)){
      /* If this is a function or parm decl, that means we've hit function
	 scope, so this prefix must be for a local name.  In this
	 case, we're under the <local-name> production, which encodes
	 the enclosing function scope elsewhere.  So don't continue
	 here.  */
      if (TREE_CODE (node) == FUNCTION_DECL  || TREE_CODE (node) == PARM_DECL)
	       return;

      decl = node;
    }else{
      /* Node is a type.  */
      decl = TYPE_NAME (node);
    }
    write_prefix (self,decl_mangling_context (self,decl));
    write_unqualified_name (self,decl);
    if (VAR_P (decl) || TREE_CODE (decl) == FIELD_DECL){
	      /* <data-member-prefix> := <member source-name> M */
	     write_char (self,'M');
	     return;
	}
    add_substitution (self,node);
}


static void write_nested_name (AetMangle *self,const tree decl)
{
  tree template_info;

  write_char (self,'N');

  /* Write CV-qualifiers, if this is a member function.  */
  if (TREE_CODE (decl) == FUNCTION_DECL  && DECL_NONSTATIC_MEMBER_FUNCTION_P (decl)){
      if (DECL_VOLATILE_MEMFUNC_P (decl))
	      write_char (self,'V');
      if (DECL_CONST_MEMFUNC_P (decl))
	      write_char (self,'K');
      if (FUNCTION_REF_QUALIFIED (TREE_TYPE (decl))){
	     if (FUNCTION_RVALUE_QUALIFIED (TREE_TYPE (decl)))
	       write_char (self,'O');
	    else
	       write_char (self,'R');
	  }
  }

   if (TREE_CODE (decl) == TYPE_DECL  && TREE_CODE (TREE_TYPE (decl)) == TYPENAME_TYPE){
          tree name = TYPENAME_TYPE_FULLNAME (TREE_TYPE (decl));
	      write_prefix (self,decl_mangling_context (self,decl));
	      write_unqualified_name (self,decl);

     } else{
        /* No, just use <prefix>  */
        write_prefix (self,decl_mangling_context (self,decl));
        write_unqualified_name (self,decl);
    }
    write_char (self,'E');
}

static void write_name (AetMangle *self,tree decl, const int ignore_local_scope)
{
    tree context;
    if (TREE_CODE (decl) == TYPE_DECL){
      /* In case this is a typedef, fish out the corresponding
	  TYPE_DECL for the main variant.  */
      n_debug("write_name 00 TREE_CODE (decl) == TYPE_DECL %s",get_tree_code_name(TREE_CODE(decl)));
      decl = TYPE_NAME (TYPE_MAIN_VARIANT (TREE_TYPE (decl)));
    }
    context = decl_mangling_context (self,decl);
    //gcc_assert (context != NULL_TREE);


  /* A decl in :: or ::std scope is treated specially.  The former is
     mangled using <unscoped-name> or <unscoped-template-name>, the
     latter with a special substitution.  Also, a name that is
     directly in a local function scope is also mangled with
     <unscoped-name> rather than a full <nested-name>.  */
   if(/*context == global_namespace || DECL_NAMESPACE_STD_P (context) || */
		   (ignore_local_scope && (TREE_CODE (context) == FUNCTION_DECL || TREE_CODE (context) == PARM_DECL ) )){
	   n_debug("write_name 11 ignore_local_scope %s",get_tree_code_name(TREE_CODE(decl)));

	/* Everything else gets an <unqualified-name>.  */
	    write_unscoped_name (self,decl);
   }else{
       /* Handle local names, unless we asked not to (that is, invoked
	     under <local-name>, to handle only the part of the name under
	     the local scope).  */
       if (!ignore_local_scope){
    	 // n_debug("write_name 22 %s",get_tree_code_name(TREE_CODE(decl)));

		  /* Scan up the list of scope context, looking for a
			 function.  If we find one, this entity is in local
			 function scope.  local_entity tracks context one scope
			 level down, so it will contain the element that's
			 directly in that function's scope, either decl or one of
			 its enclosing scopes.  */
		  tree local_entity = decl;
		  while (context != NULL_TREE/*zclei global_namespace*/){
			  /* Make sure we're always dealing with decls.  */
			  if (TYPE_P (context))
				  context = TYPE_NAME (context);
			  /* Is this a function?  */
			  if (TREE_CODE (context) == FUNCTION_DECL || TREE_CODE (context) == PARM_DECL){
			  /* Yes, we have local scope.  Use the <local-name>
				 production for the innermost function scope.  */
					n_debug("write_name 44 %s",get_tree_code_name(TREE_CODE(decl)));

				 write_local_name (self,context, local_entity, decl);
				 return;
			  }
			  /* Up one scope level.  */
			  local_entity = context;
			  context = decl_mangling_context (self,context);
			}

	      /* No local scope found?  Fall through to <nested-name>.  */
	    }

      /* Other decls get a <nested-name> to encode their scope.  */
      n_debug("write_name 55 %s",decl?get_tree_code_name(TREE_CODE(decl)):"null");
        write_nested_name (self,decl);
    }
}

static tree skip_artificial_parms_for (const_tree fn, tree list)
{
  if (DECL_NONSTATIC_MEMBER_FUNCTION_P (fn))
    list = TREE_CHAIN (list);
  else
    return list;

//  if (DECL_HAS_IN_CHARGE_PARM_P (fn))
//    list = TREE_CHAIN (list);
//  if (DECL_HAS_VTT_PARM_P (fn))
//    list = TREE_CHAIN (list);
  return list;
}

/* Given a FUNCTION_DECL, returns the first TREE_LIST out of TYPE_ARG_TYPES
   which refers to a user-written parameter.  */

static void write_method_parms (AetMangle *self,tree parm_types, const int method_p, const tree decl)
{
  tree first_parm_type;
  tree parm_decl = decl ? DECL_ARGUMENTS (decl) : NULL_TREE;

  /* Assume this parameter type list is variable-length.  If it ends
     with a void type, then it's not.  */
  int varargs_p = 1;

  /* If this is a member function, skip the first arg, which is the
     this pointer.
       "Member functions do not encode the type of their implicit this
       parameter."

     Similarly, there's no need to mangle artificial parameters, like
     the VTT parameters for constructors and destructors.  */
  if (method_p)
    {
      parm_types = TREE_CHAIN (parm_types);
      parm_decl = parm_decl ? DECL_CHAIN (parm_decl) : NULL_TREE;

      while (parm_decl && DECL_ARTIFICIAL (parm_decl))
	{
	  parm_types = TREE_CHAIN (parm_types);
	  parm_decl = DECL_CHAIN (parm_decl);
	}

      if (decl /*zclei&& ctor_omit_inherited_parms (decl)*/)
	/* Bring back parameters omitted from an inherited ctor.  */
	parm_types = FUNCTION_FIRST_USER_PARMTYPE (DECL_ORIGIN (decl));
    }

  for (first_parm_type = parm_types;
       parm_types;
       parm_types = TREE_CHAIN (parm_types))
    {
      tree parm = TREE_VALUE (parm_types);
      if (parm == void_type_node)
	{
	  /* "Empty parameter lists, whether declared as () or
	     conventionally as (void), are encoded with a void parameter
	     (v)."  */
	  if (parm_types == first_parm_type)
	    write_type (self,parm);
	  /* If the parm list is terminated with a void type, it's
	     fixed-length.  */
	  varargs_p = 0;
	  /* A void type better be the last one.  */
	  gcc_assert (TREE_CHAIN (parm_types) == NULL);
	}
      else
	write_type (self,parm);
    }

  if (varargs_p)
    /* <builtin-type> ::= z  # ellipsis  */
    write_char (self,'z');
}


static void write_bare_function_type (AetMangle *self,const tree type, const int include_return_type_p,
			  const tree decl)
{

  /* Mangle the return type, if requested.  */
  if (include_return_type_p)
    write_type (self,TREE_TYPE (type));

  /* Now mangle the types of the arguments.  */
  ++self->parm_depth;
  write_method_parms (self,TYPE_ARG_TYPES (type),TREE_CODE (type) == METHOD_TYPE,decl);
  --self->parm_depth;
}

static bool mangle_return_type_p (tree decl)
{
//  return (!DECL_CONSTRUCTOR_P (decl)
//	  && !DECL_DESTRUCTOR_P (decl)
//	  && !DECL_CONV_FN_P (decl)
//	  && decl_is_template_id (decl, NULL));
	return true;
}

static void write_encoding (AetMangle *self,const tree decl)
{
   if (DECL_LANG_SPECIFIC (decl) /*&& DECL_EXTERN_C_FUNCTION_P (decl)*/){
      /* For overloaded operators write just the mangled name
	 without arguments.  */
      //zclei if (DECL_OVERLOADED_OPERATOR_P (decl))
	   //   write_name (self,decl, /*ignore_local_scope=*/0);
     // else

    	  n_info("这是一个 write_encoding 00 %s",IDENTIFIER_POINTER (DECL_NAME (decl)));

	     write_source_name (self,DECL_NAME (decl));
   	  n_info("这是一个 write_encoding 11 %s",IDENTIFIER_POINTER (DECL_NAME (decl)));

      return;
   }
	  n_info("这是一个 write_encoding 33 %s",IDENTIFIER_POINTER (DECL_NAME (decl)));

  write_name (self,decl, /*ignore_local_scope=*/0);
  n_info("这是一个 write_encoding 44 %s",IDENTIFIER_POINTER (DECL_NAME (decl)));

  if (TREE_CODE (decl) == FUNCTION_DECL)
    {
      tree fn_type;
      tree d;

	  fn_type = TREE_TYPE (decl);
	  d = decl;

   	  n_info("这是一个 write_encoding 55 %s",IDENTIFIER_POINTER (DECL_NAME (decl)));

      write_bare_function_type (self,fn_type,mangle_return_type_p (decl),d);
    }
}



/**
 * 内部建立的VOID_TYPE BOOLEAN_TYPE INTEGER_TYPE
 */
static void write_builtin_type (AetMangle *self,tree type)
{
  if (TYPE_CANONICAL (type))
    type = TYPE_CANONICAL (type);

  switch (TREE_CODE (type)){
    case VOID_TYPE:
      write_char (self,'v');
      break;

    case BOOLEAN_TYPE:
      write_char (self,'b');
      break;

    case INTEGER_TYPE:
      /* TYPE may still be wchar_t, char8_t, char16_t, or char32_t, since that
	    isn't in integer_type_nodes.  */
      if (type == wchar_type_node)
	      write_char (self,'w');
      else if (type == char8_type_node)
	      write_string (self,"Du");
      else if (type == char16_type_node)
	      write_string (self,"Ds");
      else if (type == char32_type_node)
	      write_string (self,"Di");
      else{
	      size_t itk;
	      /* Assume TYPE is one of the shared integer type nodes.  Find
	        it in the array of these nodes.  */
	      iagain:
	         for (itk = 0; itk < itk_none; ++itk)
	            if (integer_types[itk] != NULL_TREE	&& integer_type_codes[itk] != '\0'&& type == integer_types[itk]){
		           /* Print the corresponding single-letter code.  */
		           write_char (self,integer_type_codes[itk]);
		           break;
	            }

	         if (itk == itk_none){
	             tree t = c_common_type_for_mode (TYPE_MODE (type),TYPE_UNSIGNED (type));
	             if (type != t){
		           type = t;
		           goto iagain;
		         }
	             if (TYPE_PRECISION (type) == 128)
		            write_char (self,TYPE_UNSIGNED (type) ? 'o' : 'n');
	             else{
		          /* Allow for cases where TYPE is not one of the shared
		             integer type nodes and write a "vendor extended builtin
		             type" with a name the form intN or uintN, respectively.
		             Situations like this can happen if you have an
		             __attribute__((__mode__(__SI__))) type and use exotic
		             switches like '-mint8' on AVR.  Of course, this is
		             undefined by the C++ ABI (and '-mint8' is not even
		             Standard C conforming), but when using such special
		             options you're pretty much in nowhere land anyway.  */
		             const char *prefix;
		             char prec[11];	/* up to ten digits for an unsigned */
		             prefix = TYPE_UNSIGNED (type) ? "uint" : "int";
		             sprintf (prec, "%u", (unsigned) TYPE_PRECISION (type));
		             write_char (self,'u');	/* "vendor extended builtin type" */
		             write_unsigned_number (self,strlen (prefix) + strlen (prec));
		             write_string (self,prefix);
		             write_string (self,prec);
		         }
	         }//end itk == itk_none
	  }
      break;

    case REAL_TYPE:
      if (type == float_type_node)
	      write_char (self,'f');
      else if (type == double_type_node)
	      write_char (self,'d');
      else if (type == long_double_type_node)
	      write_char (self,'e');
      else if (type == dfloat32_type_node /*zclei|| type == fallback_dfloat32_type*/)
	      write_string (self,"Df");
      else if (type == dfloat64_type_node /*zclei|| type == fallback_dfloat64_type*/)
	      write_string (self,"Dd");
      else if (type == dfloat128_type_node /*zclei|| type == fallback_dfloat128_type*/)
	      write_string (self,"De");
      else
	      gcc_unreachable ();
      break;

    case FIXED_POINT_TYPE:
         write_string (self,"DF");
      if (GET_MODE_IBIT (TYPE_MODE (type)) > 0)
	       write_unsigned_number (self,GET_MODE_IBIT (TYPE_MODE (type)));
      if (type == fract_type_node
	  || type == sat_fract_type_node
	  || type == accum_type_node
	  || type == sat_accum_type_node)
	     write_char (self,'i');
      else if (type == unsigned_fract_type_node
	       || type == sat_unsigned_fract_type_node
	       || type == unsigned_accum_type_node
	       || type == sat_unsigned_accum_type_node)
	     write_char (self,'j');
      else if (type == short_fract_type_node
	       || type == sat_short_fract_type_node
	       || type == short_accum_type_node
	       || type == sat_short_accum_type_node)
	     write_char (self,'s');
      else if (type == unsigned_short_fract_type_node
	       || type == sat_unsigned_short_fract_type_node
	       || type == unsigned_short_accum_type_node
	       || type == sat_unsigned_short_accum_type_node)
	     write_char (self,'t');
      else if (type == long_fract_type_node
	       || type == sat_long_fract_type_node
	       || type == long_accum_type_node
	       || type == sat_long_accum_type_node)
	     write_char (self,'l');
      else if (type == unsigned_long_fract_type_node
	       || type == sat_unsigned_long_fract_type_node
	       || type == unsigned_long_accum_type_node
	       || type == sat_unsigned_long_accum_type_node)
	     write_char (self,'m');
      else if (type == long_long_fract_type_node
	       || type == sat_long_long_fract_type_node
	       || type == long_long_accum_type_node
	       || type == sat_long_long_accum_type_node)
	     write_char (self,'x');
      else if (type == unsigned_long_long_fract_type_node
	       || type == sat_unsigned_long_long_fract_type_node
	       || type == unsigned_long_long_accum_type_node
	       || type == sat_unsigned_long_long_accum_type_node)
	     write_char (self,'y');
      else
	      sorry ("mangling unknown fixed point type");
      write_unsigned_number (self,GET_MODE_FBIT (TYPE_MODE (type)));
      if (TYPE_SATURATING (type))
	     write_char (self,'s');
      else
	     write_char (self,'n');
       break;

    default:
      gcc_unreachable ();
    }
}


static int cp_type_quals (const_tree type)
{
  int quals;
  /* This CONST_CAST is okay because strip_array_types returns its
     argument unmodified and we assign it to a const_tree.  */
  type = strip_array_types (CONST_CAST_TREE (type));
  if (type == error_mark_node
      /* Quals on a FUNCTION_TYPE are memfn quals.  */
      || TREE_CODE (type) == FUNCTION_TYPE)
    return TYPE_UNQUALIFIED;
  quals = TYPE_QUALS (type);
  /* METHOD and REFERENCE_TYPEs should never have quals.  */
  gcc_assert ((TREE_CODE (type) != METHOD_TYPE
	       && !TYPE_REF_P (type))
	      || ((quals & (TYPE_QUAL_CONST|TYPE_QUAL_VOLATILE))
		  == TYPE_UNQUALIFIED));
  return quals;
}


static void dump_substitution_candidates (AetMangle *self)
{
  unsigned i;
  tree el;

  fprintf (stderr, "  ++ substitutions  ");
  FOR_EACH_VEC_ELT (*self->substitutions, i, el){
      const char *name = "???";
      if (i > 0)
	    fprintf (stderr, "                    ");
      if (DECL_P (el))
	    name = IDENTIFIER_POINTER (DECL_NAME (el));
      else if (TREE_CODE (el) == TREE_LIST)
	    name = IDENTIFIER_POINTER (DECL_NAME (TREE_VALUE (el)));
      else if (TYPE_NAME (el))
	    name = TYPE_NAME_STRING (el);
      fprintf (stderr, " S%d_ = ", i - 1);
      if (TYPE_P (el) && (CP_TYPE_RESTRICT_P (el)  || CP_TYPE_VOLATILE_P (el)  || CP_TYPE_CONST_P (el)))
	     fprintf (stderr, "CV-");
      fprintf (stderr, "%s (%s at %p)\n",name, get_tree_code_name (TREE_CODE (el)), (void *) el);
  }
}

#define same_type_p(TYPE1, TYPE2)   comptypes ((TYPE1), (TYPE2))

static void add_substitution (AetMangle *self,tree node)
{
    tree c;
    fprintf (stderr, "  ++ add_substitution (%s at %10p)\n", get_tree_code_name (TREE_CODE (node)), (void *) node);

    /* Get the canonicalized substitution candidate for NODE.  */
    c = canonicalize_for_substitution (node);
    if ( c != node)
       fprintf (stderr, "  ++ using candidate (%s at %10p)\n",get_tree_code_name (TREE_CODE (node)), (void *) node);
    node = c;

  /* Make sure NODE isn't already a candidate.  */
  if (flag_checking){
      int i;
      tree candidate;

      FOR_EACH_VEC_SAFE_ELT (self->substitutions, i, candidate){
	    gcc_assert (!(DECL_P (node) && node == candidate));
	    gcc_assert (!(TYPE_P (node) && TYPE_P (candidate) && same_type_p (node, candidate)));
	  }
   }

  /* Put the decl onto the varray of substitution candidates.  */
  vec_safe_push (self->substitutions, node);

  dump_substitution_candidates (self);
}



/* Done with mangling. If WARN is true, and the name of G.entity will
   be mangled differently in a future version of the ABI, issue a
   warning.  */

static void finish_mangling_internal (AetMangle *self)
{
  /* Clear all the substitutions.  */
  //vec_safe_truncate (G.substitutions, 0);

  /* Null-terminate the string.  */
  //write_char ('\0');
}


/* Start mangling ENTITY.  */

static inline void startMangling (AetMangle *self,const tree entity)
{
	self->entity=entity;
	//n_string_erase(self->strBuf,0,-1);//这个方法引用内存错误
	n_string_free(self->strBuf,TRUE);
	self->strBuf=n_string_new("");
//  G.entity = entity;
//  G.need_abi_warning = false;
//  G.need_cxx17_warning = false;
//  obstack_free (&name_obstack, name_base);
//  mangle_obstack = &name_obstack;
//  name_base = obstack_alloc (&name_obstack, 0);
}



static bool unmangled_name_p (const tree decl)
{
  if (TREE_CODE (decl) == FUNCTION_DECL){
      /* The names of `extern "C"' functions are not mangled.  */
	  n_info("这是一个 FUNCTION_DECL unmangled_name_p");
      return true; //(DECL_EXTERN_C_FUNCTION_P (decl)  /* But overloaded operator names *are* mangled.  *//*zclei   && !DECL_OVERLOADED_OPERATOR_P (decl)*/);
  }else if (VAR_P (decl)){
	  n_info("这是一个 VAR_P unmangled_name_p ");

      /* static variables are mangled.  */
//      if (!DECL_EXTERNAL_LINKAGE_P (decl))
//	     return false;
//
//      /* extern "C" declarations aren't mangled.  */
//      if (DECL_EXTERN_C_P (decl))
//	      return true;
//
//      /* Other variables at non-global scope are mangled.  */
//      if (CP_DECL_CONTEXT (decl) !=NULL_TREE/*zclei global_namespace*/)
//	       return false;

      /* Variable template instantiations are mangled.  */
//      if (DECL_LANG_SPECIFIC (decl) && DECL_TEMPLATE_INFO (decl) && variable_template_p (DECL_TI_TEMPLATE (decl)))
//	     return false;

      /* Declarations with ABI tags are mangled.  */
//      if (get_abi_tags (decl))
//	      return false;

      /* The names of non-static global variables aren't mangled.  */
      return true;
    }

    return false;
}


static inline tree canonicalize_for_substitution (tree node)
{
  /* For a TYPE_DECL, use the type instead.  */
  if (TREE_CODE (node) == TYPE_DECL)
    node = TREE_TYPE (node);
  if (TYPE_P (node)  && TYPE_CANONICAL (node) != node   && TYPE_MAIN_VARIANT (node) != node){
      tree orig = node;
      /* Here we want to strip the topmost typedef only.
         We need to do that so is_std_substitution can do proper
         name matching.  */
      if (TREE_CODE (node) == FUNCTION_TYPE)
	/* Use build_qualified_type and TYPE_QUALS here to preserve
	   the old buggy mangling of attribute noreturn with abi<5.  */
	      node = build_qualified_type (TYPE_MAIN_VARIANT (node),TYPE_QUALS (node));
      else
	     ;//zclei node = cp_build_qualified_type (TYPE_MAIN_VARIANT (node),cp_type_quals (node));
      if (FUNC_OR_METHOD_TYPE_P (node)){
//zclei	      node = build_ref_qualified_type (node, type_memfn_rqual (orig));
//	      tree r = canonical_eh_spec (TYPE_RAISES_EXCEPTIONS (orig));
//	      if (flag_noexcept_type)
//	         node = build_exception_variant (node, r);
//	      else
//	    	  ;
	    /* Set the warning flag if appropriate.  */
	         //write_exception_spec (r);
	   }
  }
  return node;
}

#define NESTED_TEMPLATE_MATCH(NODE1, NODE2)				\
  (TREE_CODE (NODE1) == TREE_LIST					\
   && TREE_CODE (NODE2) == TREE_LIST					\
   && ((TYPE_P (TREE_PURPOSE (NODE1))					\
	&& same_type_p (TREE_PURPOSE (NODE1), TREE_PURPOSE (NODE2)))	\
       || TREE_PURPOSE (NODE1) == TREE_PURPOSE (NODE2))			\
   && TREE_VALUE (NODE1) == TREE_VALUE (NODE2))


static void write_substitution (AetMangle *self,const int seq_id)
{
  write_char (self,'S');
  if (seq_id > 0)
    write_number (self,seq_id - 1, /*unsigned=*/1, 36);
  write_char (self,'_');
}

static int find_substitution (AetMangle *self,tree node)
{
   int i;
   const int size = vec_safe_length (self->substitutions);
   tree decl;
   tree type;
   const char *abbr = NULL;
   fprintf (stderr, "  ++ find_substitution (%s at %p)\n",get_tree_code_name (TREE_CODE (node)), (void *) node);

  /* Obtain the canonicalized substitution representation for NODE.
     This is what we'll compare against.  */
   node = canonicalize_for_substitution (node);

  /* Check for builtin substitutions.  */

   decl = TYPE_P (node) ? TYPE_NAME (node) : node;
   type = TYPE_P (node) ? node : TREE_TYPE (node);

  /* Check for std::allocator.  */
//zclei   if (decl && is_std_substitution (decl, SUBID_ALLOCATOR)  && !CLASSTYPE_USE_TEMPLATE (TREE_TYPE (decl)))
//      abbr = "Sa";
//  /* Check for std::basic_string.  */
//  else if (decl && is_std_substitution (decl, SUBID_BASIC_STRING)){
//      if (TYPE_P (node)){
//	  /* If this is a type (i.e. a fully-qualified template-id),
//	     check for
//		 std::basic_string <char,
//				    std::char_traits<char>,
//				    std::allocator<char> > .  */
//	      if (cp_type_quals (type) == TYPE_UNQUALIFIED && CLASSTYPE_USE_TEMPLATE (type)){
//	           tree args = CLASSTYPE_TI_ARGS (type);
//	           if (TREE_VEC_LENGTH (args) == 3  && (TREE_CODE (TREE_VEC_ELT (args, 0)) == TREE_CODE (char_type_node))
//		           && same_type_p (TREE_VEC_ELT (args, 0), char_type_node)  && is_std_substitution_char (TREE_VEC_ELT (args, 1),SUBID_CHAR_TRAITS)
//		            && is_std_substitution_char (TREE_VEC_ELT (args, 2),SUBID_ALLOCATOR))
//		       abbr = "Ss";
//	      }
//	   } else
//	      /* Substitute for the template name only if this isn't a type.  */
//	      abbr = "Sb";
//  }  /* Check for basic_{i,o,io}stream.  */ else
//   if (TYPE_P (node)  && cp_type_quals (type) == TYPE_UNQUALIFIED  && CLASS_TYPE_P (type)
//       && CLASSTYPE_USE_TEMPLATE (type) && CLASSTYPE_TEMPLATE_INFO (type) != NULL){
//         /* First, check for the template
//	    args <char, std::char_traits<char> > .  */
//        tree args = CLASSTYPE_TI_ARGS (type);
//         if (TREE_VEC_LENGTH (args) == 2 && TREE_CODE (TREE_VEC_ELT (args, 0)) == TREE_CODE (char_type_node)
//	            && same_type_p (TREE_VEC_ELT (args, 0), char_type_node)
//	               && is_std_substitution_char (TREE_VEC_ELT (args, 1),SUBID_CHAR_TRAITS)){
//	          /* Got them.  Is this basic_istream?  */
//	         if (is_std_substitution (decl, SUBID_BASIC_ISTREAM))
//	            abbr = "Si";
//	              /* Or basic_ostream?  */
//	         else if (is_std_substitution (decl, SUBID_BASIC_OSTREAM))
//	           abbr = "So";
//	         /* Or basic_iostream?  */
//	         else if (is_std_substitution (decl, SUBID_BASIC_IOSTREAM))
//	            abbr = "Sd";
//	    }
//    }
//
//  /* Check for namespace std.  */
//  else
//   if (decl && DECL_NAMESPACE_STD_P (decl))
//    {
//      write_string (self,"St");
//      return 1;
//    }

  tree tags = NULL_TREE;
//  if (OVERLOAD_TYPE_P (node) || DECL_CLASS_TEMPLATE_P (node))
//    tags = get_abi_tags (type);
  /* Now check the list of available substitutions for this mangling
     operation.  */
  if (!abbr || tags) for (i = 0; i < size; ++i)
    {
      tree candidate = (*self->substitutions)[i];
      /* NODE is a matched to a candidate if it's the same decl node or
	 if it's the same type.  */
      if (decl == candidate	  || (TYPE_P (candidate) && type && TYPE_P (node)   && same_type_p (type, candidate))
	  || NESTED_TEMPLATE_MATCH (node, candidate))
	{
	  write_substitution (self,i);
	  return 1;
	}
    }

  if (!abbr)
    /* No substitution found.  */
    return 0;

  write_string (self,abbr);
  if (tags)
    {
      /* If there are ABI tags on the abbreviation, it becomes
	 a substitution candidate.  */
      //write_abi_tags (tags);
      add_substitution (self,node);
    }
  return 1;
}

static int write_CV_qualifiers_for_type (AetMangle *self,const tree type)
{
  int num_qualifiers = 0;

  /* The order is specified by:

       "In cases where multiple order-insensitive qualifiers are
       present, they should be ordered 'K' (closest to the base type),
       'V', 'r', and 'U' (farthest from the base type) ..."  */

  /* Mangle attributes that affect type identity as extended qualifiers.

     We don't do this with classes and enums because their attributes
     are part of their definitions, not something added on.  */

//  if (!OVERLOAD_TYPE_P (type))
//    {
//      auto_vec<tree> vec;
//      for (tree a = TYPE_ATTRIBUTES (type); a; a = TREE_CHAIN (a))
//	if (mangle_type_attribute_p (get_attribute_name (a)))
//	  vec.safe_push (a);
//      if (abi_warn_or_compat_version_crosses (10) && !vec.is_empty ())
//	G.need_abi_warning = true;
//      if (abi_version_at_least (10))
//	{
//	  vec.qsort (attr_strcmp);
//	  while (!vec.is_empty())
//	    {
//	      tree a = vec.pop();
//	      const attribute_spec *as
//		= lookup_attribute_spec (get_attribute_name (a));
//
//	      write_char ('U');
//	      write_unsigned_number (strlen (as->name));
//	      write_string (as->name);
//	      if (TREE_VALUE (a))
//		{
//		  write_char ('I');
//		  for (tree args = TREE_VALUE (a); args;
//		       args = TREE_CHAIN (args))
//		    {
//		      tree arg = TREE_VALUE (args);
//		      write_template_arg (arg);
//		    }
//		  write_char ('E');
//		}
//
//	      ++num_qualifiers;
//	    }
//	}
//    }

  /* Note that we do not use cp_type_quals below; given "const
     int[3]", the "const" is emitted with the "int", not with the
     array.  */
  int quals = TYPE_QUALS (type);

  if (quals & TYPE_QUAL_RESTRICT)
    {
      write_char (self,'r');
      ++num_qualifiers;
    }
  if (quals & TYPE_QUAL_VOLATILE)
    {
      write_char (self,'V');
      ++num_qualifiers;
    }
  if (quals & TYPE_QUAL_CONST)
    {
      write_char (self,'K');
      ++num_qualifiers;
    }

  return num_qualifiers;
}

static bool tx_safe_fn_type_p (tree t)
{
	return false;
}

static void write_expression (AetMangle *self,tree expr)
{

}

static void write_array_type (AetMangle *self,const tree type)
{
  write_char (self,'A');
  if (TYPE_DOMAIN (type)){
      tree index_type;

      index_type = TYPE_DOMAIN (type);
      /* The INDEX_TYPE gives the upper and lower bounds of the array.
	 It's null for flexible array members which have no upper bound
	 (this is a change from GCC 5 and prior where such members were
	 incorrectly mangled as zero-length arrays).  */
      if (tree max = TYPE_MAX_VALUE (index_type)){
	     if (TREE_CODE (max) == INTEGER_CST){
	      /* The ABI specifies that we should mangle the number of
		 elements in the array, not the largest allowed index.  */
	       //zclei  offset_int wmax = wi::to_offset (max) + 1;
	      /* Truncate the result - this will mangle [0, SIZE_INT_MAX]
		   number of elements as zero.  */
	        //wmax = wi::zext (wmax, TYPE_PRECISION (TREE_TYPE (max)));
	       // gcc_assert (wi::fits_uhwi_p (wmax));
	        //write_unsigned_number (wmax.to_uhwi ());
	    }else{
	      max = TREE_OPERAND (max, 0);
	      write_expression (self,max);
	    }
	 }
  }
  write_char (self,'_');
  write_type (self,TREE_TYPE (type));
}

static void write_pointer_to_member_type (AetMangle *self,const tree type)
{
  printf("write_pointer_to_member_type\n");
  write_char (self,'M');
  //zclei write_type (self,TYPE_PTRMEM_CLASS_TYPE (type));
  //write_type (self,TYPE_PTRMEM_POINTED_TO_TYPE (type));
}

static void write_enum (AetMangle *self,const tree type)
{
    if(!aet_utils_valid_tree(TYPE_NAME(type))){
    	n_warning("枚举不正常 TYPE_NAME(type)是空的!!!");
    	aet_print_tree(type);
    }
    write_char (self,'E');
    if(TREE_CODE(TYPE_NAME(type))==IDENTIFIER_NODE){
    	char *name=IDENTIFIER_POINTER(TYPE_NAME(type));
    	char *origName=enum_parser_get_orig_name(enum_parser_get(),name);
    	if(origName==NULL)
        	origName=enum_parser_get_orig_name(enum_parser_get(),name+1);//去一个下划线
    	if(origName==NULL)
           write_source_name (self,TYPE_NAME(type));
    	else{
            write_source_name (self,get_identifier(origName));
    	}
    }else
        write_source_name (self,DECL_NAME (TYPE_NAME(type)));
}


static void write_record_union(AetMangle *self,const tree type)
{
	tree typeName=TYPE_NAME (type);
	if(TREE_CODE(typeName)==TYPE_DECL)
		typeName=DECL_NAME(typeName);
   write_name (self,typeName, /*ignore_local_scope=*/0);
}

static void write_function_type (AetMangle *self,const tree type)
{
  /* For a pointer to member function, the function type may have
     cv-qualifiers, indicating the quals for the artificial 'this'
     parameter.  */
  if (TREE_CODE (type) == METHOD_TYPE)
    {
	  printf("write_function_type 00\n");
      /* The first parameter must be a POINTER_TYPE pointing to the
	 `this' parameter.  */
//zclei      tree this_type = class_of_this_parm (type);
//      write_CV_qualifiers_for_type (this_type);
    }

 // write_exception_spec (TYPE_RAISES_EXCEPTIONS (type));

  if (tx_safe_fn_type_p (type))
    write_string (self,"Dx");

  write_char (self,'F');
  /* We don't track whether or not a type is `extern "C"'.  Note that
     you can have an `extern "C"' function that does not have
     `extern "C"' type, and vice versa:

       extern "C" typedef void function_t();
       function_t f; // f has C++ linkage, but its type is
		     // `extern "C"'

       typedef void function_t();
       extern "C" function_t f; // Vice versa.

     See [dcl.link].  */
    write_bare_function_type (self,type, /*include_return_type_p=*/1, /*decl=*/NULL);
    if (FUNCTION_REF_QUALIFIED (type)){
      if (FUNCTION_RVALUE_QUALIFIED (type))
	     write_char (self,'O');
      else
	    write_char (self,'R');
   }
   write_char (self,'E');
}


static tree cp_build_type_attribute_variant00 (tree type, tree attributes)
{
  tree new_type;

  new_type = build_type_attribute_variant (type, attributes);
//  if (FUNC_OR_METHOD_TYPE_P (new_type))
//    gcc_checking_assert (cxx_type_hash_eq00 (type, new_type));
//
//  /* Making a new main variant of a class type is broken.  */
//  gcc_assert (!CLASS_TYPE_P (type) || new_type == type);

  return new_type;
}


static void write_type(AetMangle *self,tree type)
{
  /* This gets set to nonzero if TYPE turns out to be a (possibly
     CV-qualified) builtin type.  */
   int is_builtin_type = 0;
   if (type == error_mark_node)
    return;
   type = canonicalize_for_substitution (type);
   if (write_CV_qualifiers_for_type (self,type) > 0){
    /* If TYPE was CV-qualified, we just wrote the qualifiers; now
       mangle the unqualified type.  The recursive call is needed here
       since both the qualified and unqualified types are substitution
       candidates.  */
         //printf("进这里 00 code:%s\n",get_tree_code_name(TREE_CODE(type)));
         tree t = TYPE_MAIN_VARIANT (type);
         //printf("进这里 11 code:%s code:%s\n",get_tree_code_name(TREE_CODE(type)),get_tree_code_name(TREE_CODE(t)));

         if (TYPE_ATTRIBUTES (t) /*zclei && !OVERLOAD_TYPE_P (t)*/){
	         tree attrs = NULL_TREE;
           if (tx_safe_fn_type_p (type))
        	   attrs = tree_cons (get_identifier ("transaction_safe"),NULL_TREE, attrs);
	         t = cp_build_type_attribute_variant00 (t, attrs);
	     }
         gcc_assert (t != type);
         if (FUNC_OR_METHOD_TYPE_P (t)){
//zclei 	         t = build_ref_qualified_type (t, type_memfn_rqual (type));
//	         if (flag_noexcept_type){
//	            tree r = TYPE_RAISES_EXCEPTIONS (type);
//	            t = build_exception_variant (t, r);
//	         }
	         if (type == TYPE_MAIN_VARIANT (type)){
	              /* Avoid adding the unqualified function type as a substitution.  */
	             //printf("进这里 22 code:%s code:%s\n",get_tree_code_name(TREE_CODE(type)),get_tree_code_name(TREE_CODE(t)));
	            write_function_type (self,t);
	         }else{
	             //printf("进这里 33 code:%s code:%s\n",get_tree_code_name(TREE_CODE(type)),get_tree_code_name(TREE_CODE(t)));
	            write_type(self,t);
	         }
	     }else{
             //printf("进这里 44 code:%s code:%s\n",get_tree_code_name(TREE_CODE(type)),get_tree_code_name(TREE_CODE(t)));
	         write_type (self,t);
	     }
   }else if (TREE_CODE (type) == ARRAY_TYPE){
    /* It is important not to use the TYPE_MAIN_VARIANT of TYPE here
       so that the cv-qualification of the element type is available
       in write_array_type.  */
        //printf("进这里 55 code:%s\n",get_tree_code_name(TREE_CODE(type)));
        write_array_type (self,type);
   }else{
       tree type_orig = type;
      /* See through any typedefs.  */
       type = TYPE_MAIN_VARIANT (type);
       //printf("进这里 66 code:%s\n",get_tree_code_name(TREE_CODE(type)));
       if(TREE_CODE(type)==ENUMERAL_TYPE && !aet_utils_valid_tree(TYPE_NAME(type))){
    	   if(aet_utils_valid_tree(TYPE_NAME(type_orig)) && TREE_CODE(type_orig)==ENUMERAL_TYPE){
        	   n_warning("这是一个枚举。TYPE_NAME(type)是空的。");
    		   type=type_orig;
    	   }
       }
       //printf("进这里 66ddddd code:%s\n",get_tree_code_name(TREE_CODE(type)));

       //zclei if (FUNC_OR_METHOD_TYPE_P (type))
	     //type = cxx_copy_lang_qualifiers (type, type_orig);
       //printf("进这里 66 code:%s\n",get_tree_code_name(TREE_CODE(type)));
       /* According to the C++ ABI, some library classes are passed the
	   same as the scalar type of their single member and use the same
	   mangling.  */
      if (TREE_CODE (type) == RECORD_TYPE && TYPE_TRANSPARENT_AGGR (type)){
          //printf("进这里 77 code:%s\n",get_tree_code_name(TREE_CODE(type)));
	      type = TREE_TYPE (first_field (type));
          //printf("进这里 88 code:%s\n",get_tree_code_name(TREE_CODE(type)));
      }

      if (TYPE_PTRDATAMEM_P (type)){
	       write_pointer_to_member_type (self,type);
      }else{

	  switch (TREE_CODE (type)){
	      case VOID_TYPE:
	      case BOOLEAN_TYPE:
	      case INTEGER_TYPE:  /* Includes wchar_t.  */
	      case REAL_TYPE:
	      case FIXED_POINT_TYPE:
	        {
		     /* If this is a typedef, TYPE may not be one of
		     the standard builtin type nodes, but an alias of one.  Use
		     TYPE_MAIN_VARIANT to get to the underlying builtin type.  */
		       write_builtin_type (self,TYPE_MAIN_VARIANT (type));
		       ++is_builtin_type;
	         }
	         break;
	      case COMPLEX_TYPE:
	         write_char (self,'C');
	         write_type (self,TREE_TYPE (type));
	         break;
	      case FUNCTION_TYPE:
	      case METHOD_TYPE:
	          write_function_type (self,type);
	         break;
	      case UNION_TYPE:
	      case RECORD_TYPE:
	    	  write_record_union(self,TYPE_NAME(type)==NULL?type_orig:type);
	    	  break;
	      case ENUMERAL_TYPE:
	    	  write_enum (self,type);
	          break;
	      case TYPENAME_TYPE:
	      case UNBOUND_CLASS_TEMPLATE:
	         /* We handle TYPENAME_TYPEs and UNBOUND_CLASS_TEMPLATEs like
		       ordinary nested names.  */
	         write_nested_name (self,TYPE_STUB_DECL (type));
	         break;
	      case POINTER_TYPE:
	      case REFERENCE_TYPE:
	          if (TYPE_PTR_P (type))
		        write_char (self,'P');
	          else if (TYPE_REF_IS_RVALUE (type))
		         write_char (self,'O');
              else
                write_char (self,'R');
	          {
		         tree target = TREE_TYPE (type);
		         /* Attribute const/noreturn are not reflected in mangling.
		        We strip them here rather than at a lower level because
		        a typedef or template argument can have function type
		        with function-cv-quals (that use the same representation),
		        but you can't have a pointer/reference to such a type.  */
		        if (TREE_CODE (target) == FUNCTION_TYPE){
		               //zclei if (abi_version_at_least (5))
		                   target = build_qualified_type (target, TYPE_UNQUALIFIED);
		         }
		         write_type (self,target);
	          }
	          break;
	    case VECTOR_TYPE:
		  write_string (self,"Dv");
		  /* Non-constant vector size would be encoded with
		     _ expression, but we don't support that yet.  */
		  write_unsigned_number (self,TYPE_VECTOR_SUBPARTS (type).to_constant ());
		  write_char (self,'_');
	      write_type (self,TREE_TYPE (type));
	      break;
        case DECLTYPE_TYPE:
	      /* These shouldn't make it into mangling.  */
	       //zclei gcc_assert (!DECLTYPE_FOR_LAMBDA_CAPTURE (type)  && !DECLTYPE_FOR_LAMBDA_PROXY (type));

	      /* In ABI <5, we stripped decltype of a plain decl.  */
	      if (DECLTYPE_TYPE_ID_EXPR_OR_MEMBER_ACCESS_P (type))
		{
	          //printf("进这里 102 code:%s\n",get_tree_code_name(TREE_CODE(type)));

		  tree expr = DECLTYPE_TYPE_EXPR (type);
		  tree etype = NULL_TREE;
		  switch (TREE_CODE (expr))
		    {
		    case VAR_DECL:
		    case PARM_DECL:
		    case RESULT_DECL:
		    case FUNCTION_DECL:
		    case CONST_DECL:
		    case TEMPLATE_PARM_INDEX:
		      etype = TREE_TYPE (expr);
		      break;

		    default:
		      break;
		    }

		  if (etype /*zclei&& !type_uses_auto (etype)*/)
		    {
	          printf("进这里 103 code:%s\n",get_tree_code_name(TREE_CODE(type)));
			  write_type (self,etype);
			  return;
		    }
		}

              write_char (self,'D');
              if (DECLTYPE_TYPE_ID_EXPR_OR_MEMBER_ACCESS_P (type))
                write_char (self,'t');
              else
                write_char (self,'T');
	     // ++cp_unevaluated_operand;
              write_expression (self,DECLTYPE_TYPE_EXPR (type));
	      //--cp_unevaluated_operand;
              write_char (self,'E');
              break;

	    case NULLPTR_TYPE:
	      write_string (self,"Dn");
//	      if (abi_version_at_least (7))
//		++is_builtin_type;
//	      if (abi_warn_or_compat_version_crosses (7))
//		G.need_abi_warning = 1;
	      break;

	    case TYPEOF_TYPE:
	      sorry ("mangling %<typeof%>, use %<decltype%> instead");
	      break;

	    case UNDERLYING_TYPE:
	      sorry ("mangling %<__underlying_type%>");
	      break;

	    case LANG_TYPE:
	      /* fall through.  */

	    default:
	      gcc_unreachable ();
	    }
	}
    }

  /* Types other than builtin types are substitution candidates.  */
//  if (!is_builtin_type)
//    add_substitution (self,type);
}


static void write_method_parms00 (AetMangle *self,tree parm_types,int skip)
{
   tree first_parm_type;
   /* Assume this parameter type list is variable-length.  If it ends
     with a void type, then it's not.  */
   int varargs_p = 1;
   int count=0;
   for (first_parm_type = parm_types; parm_types;parm_types = TREE_CHAIN (parm_types)){
      tree parm = TREE_VALUE (parm_types);
	  count++;
      //n_debug("write_method_parms00 00 count:%d 参数： %s skip:%d",count,get_tree_code_name(TREE_CODE(parm)),skip);
      if(count==skip){
    	  continue;
      }
      if (parm == void_type_node){
          n_debug("write_method_parms00 11  parm == void_type_node) count:%d 参数： %s skip:%d (parm_types == first_parm_type):%d",
        		  count,get_tree_code_name(TREE_CODE(parm)),skip,(parm_types == first_parm_type));

	  /* "Empty parameter lists, whether declared as () or
	     conventionally as (void), are encoded with a void parameter
	     (v)."  */
	    if (parm_types == first_parm_type)
	      write_type (self,parm);
	     /* If the parm list is terminated with a void type, it's
	       fixed-length.  */
	    varargs_p = 0;
	  /* A void type better be the last one.  */
	    gcc_assert (TREE_CHAIN (parm_types) == NULL);
	  } else
	     write_type (self,parm);
  }
  if(count==0 || (skip>0 && count==2)){
      write_type (self,void_type_node);
	  varargs_p = 0;
  }

  if (varargs_p)
    /* <builtin-type> ::= z  # ellipsis  */
    write_char (self,'z');
}


static void write_function_name_by_str(AetMangle *self,const tree funName,const char* className)
{
    write_string (self,"_Z");
    write_unsigned_number (self,strlen(className)); //aet设计的class名前有一个下划线所以减去
    write_identifier (self,className);
    write_unsigned_number (self,IDENTIFIER_LENGTH (funName));
    write_identifier (self,IDENTIFIER_POINTER (funName));
    write_char (self,'E');
}


int aet_mangle_get_orgi_func_and_class_name(AetMangle *self,char *newName,char *className,char *funcName)
{
	if(newName[0]!='_')
		return 0;
	if(newName[1]!='Z')
		return 0;
    if(!n_ascii_isdigit(newName[2]))
    	return 0;
    char classNameLen[strlen(newName)];
    int i=2;
    int count=0;
    while(n_ascii_isdigit(newName[i])){
    	classNameLen[count++]=newName[i];
        i++;
    }
    classNameLen[count]='\0';
    int len=atoi((const char*)classNameLen);
    memcpy(className,newName+i,len);
    className[len]='\0';
    i+=len;
    count=0;
    while(n_ascii_isdigit(newName[i])){
    	classNameLen[count++]=newName[i];
        i++;
    }
    classNameLen[count]='\0';
    len=atoi((const char*)classNameLen);
    memcpy(funcName,newName+i,len);
    funcName[len]='\0';
    return len;
}



/**
 * 用系统名（带包名的）生成函数名，而field生成函数名用的是用户名
 */
char *aet_mangle_create_static_var_name(AetMangle *self,ClassName *className,tree varName,tree type)
{
	startMangling(self,NULL_TREE);
	write_string (self,"_V");
	write_unsigned_number (self,strlen(className->sysName)); //aet设计的class名前有一个下划线所以减去
	write_identifier (self,className->sysName);
	write_unsigned_number (self,IDENTIFIER_LENGTH (varName));
	write_identifier (self,IDENTIFIER_POINTER (varName));
	write_char (self,'E');
	write_type (self,type);
	char *newVarName=n_strdup(self->strBuf->str);
	return newVarName;
}

/**
 * 把function_type中的参数写成字符
 */
char *aet_mangle_create_parm_string(AetMangle *self,tree funcType)
{
	startMangling(self,NULL_TREE);
	for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
	   tree parm = TREE_VALUE (al);
	   //printf("aet_mangle_create_parm_string 参数： %s\n",get_tree_code_name(TREE_CODE(parm)));
	   if (parm != void_type_node){
		 write_type (self,parm);
	   }
	}
    char *newVarName=n_strdup(self->strBuf->str);
    return newVarName;
}



char *aet_mangle_create(AetMangle *self,tree funName,tree argTypes,char *className)
{
	startMangling(self,NULL_TREE);
	write_function_name_by_str(self,funName,className);
	write_method_parms00(self,argTypes,-1);
	char *newName=n_strdup(self->strBuf->str);
	return newName;
}

char *aet_mangle_create_skip(AetMangle *self,tree funName,tree argTypes,char *className,int skip)
{
	startMangling(self,NULL_TREE);
	write_function_name_by_str(self,funName,className);
	write_method_parms00(self,argTypes,skip);
	char *newName=n_strdup(self->strBuf->str);
	return newName;
}


AetMangle *aet_mangle_new()
{
	AetMangle *self =n_slice_alloc0 (sizeof(AetMangle));
	aetInit(self);
	return self;
}
