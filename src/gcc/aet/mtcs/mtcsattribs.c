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
 * base on attribs.cc
 */
#define INCLUDE_STRING
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "predict.h"
#include "memmodel.h"
#include "tm_p.h"
#include "stringpool.h"
#include "regs.h"
#include "emit-rtl.h"
#include "cgraph.h"
#include "diagnostic-core.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "varasm.h"
#include "version.h"
#include "flags.h"
#include "stmt.h"
#include "expr.h"
#include "expmed.h"
#include "optabs.h"
#include "output.h"
#include "langhooks.h"
#include "debug.h"
#include "common/common-target.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "rtl-iter.h"
#include "file-prefix-map.h" // remap_debug_filename()
#include "alloc-pool.h"
#include "toplev.h"
#include "opts.h"
#include "asan.h"
#include "recog.h"
#include "dwarf2out.h"
#include "dwarf2.h"
#include "dwarf2asm.h"
#include "except.h"
#include "langhooks-def.h"
#include "ipa-strub.h"


#include "mtcsattribs.h"
#include "mtcstarget.h"
#include "mtcscompile.h"

void mtcs_attribs_init(MtcsAttribs *self)
{
   self->attributes_initialized=false;
}

static tree handle_noreturn_attribute (tree *, tree, tree, int, bool *);
static tree handle_leaf_attribute (tree *, tree, tree, int, bool *);
static tree handle_const_attribute (tree *, tree, tree, int, bool *);
static tree handle_malloc_attribute (tree *, tree, tree, int, bool *);
static tree handle_pure_attribute (tree *, tree, tree, int, bool *);
static tree handle_novops_attribute (tree *, tree, tree, int, bool *);
static tree handle_nonnull_attribute (tree *, tree, tree, int, bool *);
static tree handle_nothrow_attribute (tree *, tree, tree, int, bool *);
static tree handle_sentinel_attribute (tree *, tree, tree, int, bool *);
static tree handle_type_generic_attribute (tree *, tree, tree, int, bool *);
static tree handle_transaction_pure_attribute (tree *, tree, tree, int, bool *);
static tree handle_returns_twice_attribute (tree *, tree, tree, int, bool *);
static tree handle_patchable_function_entry_attribute (tree *, tree, tree,
                         int, bool *);
static tree ignore_attribute (tree *, tree, tree, int, bool *);

static tree handle_format_attribute (tree *, tree, tree, int, bool *);
static tree handle_fnspec_attribute (tree *, tree, tree, int, bool *);
static tree handle_format_arg_attribute (tree *, tree, tree, int, bool *);

/* Helper to define attribute exclusions.  */
#define ATTR_EXCL(name, function, type, variable)  \
  { name, function, type, variable }

/* Define attributes that are mutually exclusive with one another.  */
static const struct attribute_spec::exclusions attr_noreturn_exclusions[] =
{
  ATTR_EXCL ("noreturn", true, true, true),
  ATTR_EXCL ("alloc_align", true, true, true),
  ATTR_EXCL ("alloc_size", true, true, true),
  ATTR_EXCL ("const", true, true, true),
  ATTR_EXCL ("malloc", true, true, true),
  ATTR_EXCL ("pure", true, true, true),
  ATTR_EXCL ("returns_twice", true, true, true),
  ATTR_EXCL ("warn_unused_result", true, true, true),
  ATTR_EXCL (NULL, false, false, false),
};

static const struct attribute_spec::exclusions attr_returns_twice_exclusions[] =
{
  ATTR_EXCL ("noreturn", true, true, true),
  ATTR_EXCL (NULL, false, false, false),
};

static const struct attribute_spec::exclusions attr_const_pure_exclusions[] =
{
  ATTR_EXCL ("const", true, true, true),
  ATTR_EXCL ("noreturn", true, true, true),
  ATTR_EXCL ("pure", true, true, true),
  ATTR_EXCL (NULL, false, false, false)
};

/* Table of machine-independent attributes supported in GIMPLE.  */
static const attribute_spec mtcs_gnu_attributes[] =
{
  /* { name, min_len, max_len, decl_req, type_req, fn_type_req,
       affects_type_identity, handler, exclude } */
  { "noreturn",               0, 0, true,  false, false, false,
               handle_noreturn_attribute,
               attr_noreturn_exclusions },
  { "leaf",          0, 0, true,  false, false, false,
               handle_leaf_attribute, NULL },
  /* The same comments as for noreturn attributes apply to const ones.  */
  { "const",                  0, 0, true,  false, false, false,
               handle_const_attribute,
               attr_const_pure_exclusions },
  { "malloc",                 0, 0, true,  false, false, false,
               handle_malloc_attribute, NULL },
  { "pure",                   0, 0, true,  false, false, false,
               handle_pure_attribute,
               attr_const_pure_exclusions },
  { "no vops",                0, 0, true,  false, false, false,
               handle_novops_attribute, NULL },
  { "nonnull",                0, -1, false, true, true, false,
               handle_nonnull_attribute, NULL },
  { "nothrow",                0, 0, true,  false, false, false,
               handle_nothrow_attribute, NULL },
  { "patchable_function_entry", 1, 2, true, false, false, false,
               handle_patchable_function_entry_attribute,
               NULL },
  { "returns_twice",          0, 0, true,  false, false, false,
               handle_returns_twice_attribute,
               attr_returns_twice_exclusions },
  { "sentinel",               0, 1, false, true, true, false,
               handle_sentinel_attribute, NULL },
  { "type generic",           0, 0, false, true, true, false,
               handle_type_generic_attribute, NULL },
  { "fn spec",          1, 1, false, true, true, false,
               handle_fnspec_attribute, NULL },
  { "transaction_pure",       0, 0, false, true, true, false,
               handle_transaction_pure_attribute, NULL },
  /* For internal use only.  The leading '*' both prevents its usage in
     source code and signals that it may be overridden by machine tables.  */
  { "*tm regparm",            0, 0, false, true, true, false,
               ignore_attribute, NULL }
};

//原型 lto_gnu_attribute_table lto-lang.cc
static const scoped_attribute_specs mtcs_gnu_attribute_table =
{
  "gnu", { mtcs_gnu_attributes }
};


/* Give the specifications for the format attributes, used by C and all
   descendants.  */

static const attribute_spec mtcs_format_attributes[] =
{
  /* { name, min_len, max_len, decl_req, type_req, fn_type_req,
       affects_type_identity, handler, exclude } */
  { "format",                 3, 3, false, true,  true, false,
               handle_format_attribute, NULL },
  { "format_arg",             1, 1, false, true,  true, false,
               handle_format_arg_attribute, NULL },
};

static const scoped_attribute_specs mtcs_format_attribute_table =
{
  "gnu", { mtcs_format_attributes }
};

static const scoped_attribute_specs *const mtcs_attribute_table[] =
{
  &mtcs_gnu_attribute_table,
  &mtcs_format_attribute_table
};



/* Simple hash function to avoid need to scan whole string.  */
//原型 substring_hash attribs.cc
static inline hashval_t substring_hash (const char *str, int l)
{
  return str[0] + str[l - 1] * 256 + l * 65536;
}

inline hashval_t mtcs_attribute_hasher::hash (const attribute_spec *spec)
{
  const int l = strlen (spec->name);
  return substring_hash (spec->name, l);
}

inline bool mtcs_attribute_hasher::equal (const attribute_spec *spec, const mtcs_substring *str)
{
  return (strncmp (spec->name, str->str, str->length) == 0
     && !spec->name[str->length]);
}

/* Attribute handlers.  */

/* Handle a "noreturn" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree handle_noreturn_attribute (tree *node, tree ARG_UNUSED (name),
            tree ARG_UNUSED (args), int ARG_UNUSED (flags),
            bool * ARG_UNUSED (no_add_attrs))
{
   MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree type = TREE_TYPE (*node);
   if (TREE_CODE (*node) == FUNCTION_DECL)
      TREE_THIS_VOLATILE (*node) = 1;
   else if (TREE_CODE (type) == POINTER_TYPE && TREE_CODE (TREE_TYPE (type)) == FUNCTION_TYPE)
      TREE_TYPE (*node) = mtcs_tree_build_pointer_type/*!build_pointer_type*/(mtcsTree,
            mtcs_tree_build_type_variant/*!build_type_variant*/(mtcsTree,
                  TREE_TYPE (type), TYPE_READONLY (TREE_TYPE (type)), 1));
   else
      gcc_unreachable ();

   return NULL_TREE;
}

/* Handle a "leaf" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree handle_leaf_attribute (tree *node, tree name,
             tree ARG_UNUSED (args),
             int ARG_UNUSED (flags), bool *no_add_attrs)
{
   if (TREE_CODE (*node) != FUNCTION_DECL){
      warning (OPT_Wattributes, "%qE attribute ignored", name);
      *no_add_attrs = true;
   }
   if (!TREE_PUBLIC (*node)){
      warning (OPT_Wattributes, "%qE attribute has no effect on unit local functions", name);
      *no_add_attrs = true;
   }
   return NULL_TREE;
}

/* Handle a "const" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree handle_const_attribute (tree *node, tree ARG_UNUSED (name),
         tree ARG_UNUSED (args), int ARG_UNUSED (flags),
         bool * ARG_UNUSED (no_add_attrs))
{
   MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   if (TREE_CODE (*node) != FUNCTION_DECL  || !fndecl_built_in_p (*node))
      inform (UNKNOWN_LOCATION, "%s:%s: %E: %E", __FILE__, __func__, *node, name);

   tree type = TREE_TYPE (*node);
   /* See FIXME comment on noreturn in c_common_attribute_table.  */
   if (TREE_CODE (*node) == FUNCTION_DECL)
      TREE_READONLY (*node) = 1;
   else if (TREE_CODE (type) == POINTER_TYPE
   && TREE_CODE (TREE_TYPE (type)) == FUNCTION_TYPE)
      TREE_TYPE (*node) = mtcs_tree_build_pointer_type/*!build_pointer_type*/(mtcsTree,
            mtcs_tree_build_type_variant/*!build_type_variant*/(mtcsTree,TREE_TYPE (type), 1,
                  TREE_THIS_VOLATILE (TREE_TYPE (type))));
   else
      gcc_unreachable ();

   return NULL_TREE;
}


/* Handle a "malloc" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree handle_malloc_attribute (tree *node, tree ARG_UNUSED (name),
          tree ARG_UNUSED (args), int ARG_UNUSED (flags),
          bool * ARG_UNUSED (no_add_attrs))
{
   if (TREE_CODE (*node) == FUNCTION_DECL  && POINTER_TYPE_P (TREE_TYPE (TREE_TYPE (*node))))
      DECL_IS_MALLOC (*node) = 1;
   else
      gcc_unreachable ();

   return NULL_TREE;
}


/* Handle a "pure" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree handle_pure_attribute (tree *node, tree ARG_UNUSED (name),
             tree ARG_UNUSED (args), int ARG_UNUSED (flags),
             bool * ARG_UNUSED (no_add_attrs))
{
   if (TREE_CODE (*node) == FUNCTION_DECL)
      DECL_PURE_P (*node) = 1;
   else
      gcc_unreachable ();

   return NULL_TREE;
}


/* Handle a "no vops" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree handle_novops_attribute (tree *node, tree ARG_UNUSED (name),
          tree ARG_UNUSED (args), int ARG_UNUSED (flags),
          bool *ARG_UNUSED (no_add_attrs))
{
   gcc_assert (TREE_CODE (*node) == FUNCTION_DECL);
   DECL_IS_NOVOPS (*node) = 1;
   return NULL_TREE;
}


/* Helper for nonnull attribute handling; fetch the operand number
   from the attribute argument list.  */

static bool get_nonnull_operand (tree arg_num_expr, unsigned HOST_WIDE_INT *valp)
{
   /* Verify the arg number is a constant.  */
   if (!tree_fits_uhwi_p (arg_num_expr))
      return false;

   *valp = TREE_INT_CST_LOW (arg_num_expr);
   return true;
}

/* Handle the "nonnull" attribute.  */
static tree handle_nonnull_attribute (tree *node, tree ARG_UNUSED (name),
           tree args, int ARG_UNUSED (flags),
           bool * ARG_UNUSED (no_add_attrs))
{
   tree type = *node;
   /* If no arguments are specified, all pointer arguments should be
   non-null.  Verify a full prototype is given so that the arguments
   will have the correct types when we actually check them later.
   Avoid diagnosing type-generic built-ins since those have no
   prototype.  */
   if (!args){
      gcc_assert (prototype_p (type) || !TYPE_ATTRIBUTES (type)
               || lookup_attribute ("type generic", TYPE_ATTRIBUTES (type)));

      return NULL_TREE;
   }
   /* Argument list specified.  Verify that each argument number references
   a pointer argument.  */
   for (; args; args = TREE_CHAIN (args)){
      tree argument;
      unsigned HOST_WIDE_INT arg_num = 0, ck_num;

      if (!get_nonnull_operand (TREE_VALUE (args), &arg_num))
         gcc_unreachable ();

      argument = TYPE_ARG_TYPES (type);
      if (argument){
         for (ck_num = 1; ; ck_num++){
            if (!argument || ck_num == arg_num)
            break;
            argument = TREE_CHAIN (argument);
         }
         gcc_assert (argument  && TREE_CODE (TREE_VALUE (argument)) == POINTER_TYPE);
      }
   }
   return NULL_TREE;
}


/* Handle a "nothrow" attribute; arguments as in
   struct attribute_spec.handler.  */
static tree handle_nothrow_attribute (tree *node, tree ARG_UNUSED (name),
           tree ARG_UNUSED (args), int ARG_UNUSED (flags),
           bool * ARG_UNUSED (no_add_attrs))
{
   if (TREE_CODE (*node) == FUNCTION_DECL)
      TREE_NOTHROW (*node) = 1;
   else
      gcc_unreachable ();

   return NULL_TREE;
}


/* Handle a "sentinel" attribute.  */

static tree handle_sentinel_attribute (tree *node, tree ARG_UNUSED (name), tree args,
            int ARG_UNUSED (flags),
            bool * ARG_UNUSED (no_add_attrs))
{
   gcc_assert (stdarg_p (*node));
   if (args){
      tree position = TREE_VALUE (args);
      gcc_assert (TREE_CODE (position) == INTEGER_CST);
      if (tree_int_cst_lt (position, integer_zero_node))
         gcc_unreachable ();
   }

   return NULL_TREE;
}

/* Handle a "type_generic" attribute.  */

static tree handle_type_generic_attribute (tree *node, tree ARG_UNUSED (name),
                tree ARG_UNUSED (args), int ARG_UNUSED (flags),
                bool * ARG_UNUSED (no_add_attrs))
{
   /* Ensure we have a function type.  */
   gcc_assert (TREE_CODE (*node) == FUNCTION_TYPE);
   /* Ensure we have a variadic function.  */
   gcc_assert (!prototype_p (*node) || stdarg_p (*node));
   return NULL_TREE;
}

/* Handle a "transaction_pure" attribute.  */
static tree handle_transaction_pure_attribute (tree *node, tree ARG_UNUSED (name),
               tree ARG_UNUSED (args),
               int ARG_UNUSED (flags),
               bool * ARG_UNUSED (no_add_attrs))
{
   /* Ensure we have a function type.  */
   gcc_assert (TREE_CODE (*node) == FUNCTION_TYPE);
   return NULL_TREE;
}

/* Handle a "returns_twice" attribute.  */

static tree handle_returns_twice_attribute (tree *node, tree ARG_UNUSED (name),
            tree ARG_UNUSED (args),
            int ARG_UNUSED (flags),
            bool * ARG_UNUSED (no_add_attrs))
{
   gcc_assert (TREE_CODE (*node) == FUNCTION_DECL);
   DECL_IS_RETURNS_TWICE (*node) = 1;
   return NULL_TREE;
}

static tree handle_patchable_function_entry_attribute (tree *, tree, tree, int, bool *)
{
   /* Nothing to be done here.  */
   return NULL_TREE;
}

/* Ignore the given attribute.  Used when this attribute may be usefully
   overridden by the target, but is not used generically.  */

static tree ignore_attribute (tree * ARG_UNUSED (node), tree ARG_UNUSED (name),
        tree ARG_UNUSED (args), int ARG_UNUSED (flags),
        bool *no_add_attrs)
{
   *no_add_attrs = true;
   return NULL_TREE;
}

/* Handle a "format" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree handle_format_attribute (tree * ARG_UNUSED (node), tree ARG_UNUSED (name),
          tree ARG_UNUSED (args), int ARG_UNUSED (flags),
          bool *no_add_attrs)
{
   *no_add_attrs = true;
   return NULL_TREE;
}


/* Handle a "format_arg" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree handle_format_arg_attribute (tree * ARG_UNUSED (node), tree ARG_UNUSED (name),
              tree ARG_UNUSED (args), int ARG_UNUSED (flags),
              bool *no_add_attrs)
{
   *no_add_attrs = true;
   return NULL_TREE;
}


/* Handle a "fn spec" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree handle_fnspec_attribute (tree *node ATTRIBUTE_UNUSED, tree ARG_UNUSED (name),
          tree args, int ARG_UNUSED (flags),
          bool *no_add_attrs ATTRIBUTE_UNUSED)
{
   gcc_assert (args
         && TREE_CODE (TREE_VALUE (args)) == STRING_CST
         && !TREE_CHAIN (args));
   return NULL_TREE;
}

/* Make some sanity checks on the attribute tables.  */
//原型 check_attribute_tables attribs.cc
static void check_attribute_tables (MtcsAttribs *self)
{
   hash_set<pair_hash<nofree_string_hash, nofree_string_hash>> names;

   for (auto scoped_array : self->attribute_tables)
      for (auto scoped_attributes : scoped_array)
         for (const attribute_spec &attribute : scoped_attributes->attributes){
            /* The name must not begin and end with __.  */
            const char *name = attribute.name;
            int len = strlen (name);

            gcc_assert (!(name[0] == '_' && name[1] == '_' && name[len - 1] == '_' && name[len - 2] == '_'));

            /* The minimum and maximum lengths must be consistent.  */
            gcc_assert (attribute.min_length >= 0);

            gcc_assert (attribute.max_length == -1 || attribute.max_length >= attribute.min_length);

            /* An attribute cannot require both a DECL and a TYPE.  */
            gcc_assert (!attribute.decl_required  || !attribute.type_required);

            /* If an attribute requires a function type, in particular
            it requires a type.  */
            gcc_assert (!attribute.function_type_required || attribute.type_required);

            /* Check that no name occurs more than once.  Names that
            begin with '*' are exempt, and may be overridden.  */
            const char *ns = scoped_attributes->ns;
            if (name[0] != '*' && names.add ({ ns ? ns : "", name }))
               gcc_unreachable ();
         }
}

/* Initialize attribute tables, and make some sanity checks if checking is
   enabled.  */
//原型 init_attributes attribs.h attribs.cc
void mtcs_attribs_init_attributes (MtcsAttribs *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   if (self->attributes_initialized)
      return;

   self->attribute_tables[0] = mtcs_attribute_table/*!lang_hooks.attribute_table*/;
   //原型 #define TARGET_ATTRIBUTE_TABLE nvptx_attribute_table
   self->set_attribute_table(self);/*!attribute_tables[1] =targetm.attribute_table;*/


   if (mtcsOptionsItem->x_flag_checking)
      check_attribute_tables(self);

   for (auto scoped_array : self->attribute_tables)
      for (auto scoped_attributes : scoped_array)
         mtcs_attribs_register_scoped_attributes/*!register_scoped_attributes*/(self,*scoped_attributes);

   vec<char *> *ignored = (vec<char *> *) mtcsOptionsItem->x_flag_ignored_attributes;
   handle_ignored_attributes_option (ignored);

   self->attributes_initialized = true;
}

/* Return the IDENTIFIER_NODE for the gnu namespace.  */
//原型 get_gnu_namespace attribs.cc
static tree get_gnu_namespace (MtcsAttribs *self)
{
  if (!self->gnu_namespace_cache)
    self->gnu_namespace_cache = get_identifier ("gnu");
  return self->gnu_namespace_cache;
}


/* Return the namespace which name is NS, NULL if none exist.  */

static mtcs_scoped_attributes* find_attribute_namespace (MtcsAttribs *self,const char* ns)
{
   for (mtcs_scoped_attributes &iter : self->attributes_table)
      if (ns == iter.ns || (iter.ns != NULL  && ns != NULL   && !strcmp (iter.ns, ns)))
         return &iter;
   return NULL;
}

/* Return true iff we should not complain about unknown attributes
   coming from the attribute namespace NS.  This is the case for
   the -Wno-attributes=ns:: command-line option.  */
//原型 attr_namespace_ignored_p attribs.cc
static bool attr_namespace_ignored_p (MtcsAttribs *self,tree ns)
{
  if (ns == NULL_TREE)
    return false;
  mtcs_scoped_attributes *r = find_attribute_namespace(self,IDENTIFIER_POINTER (ns));
  return r && r->ignored_p;
}


/* Return the spec for the scoped attribute with namespace NS and
   name NAME.   */
//原型 lookup_scoped_attribute_spec attribs.cc
static const struct attribute_spec *lookup_scoped_attribute_spec (MtcsAttribs *self,const_tree ns, const_tree name)
{
   struct mtcs_substring attr;
   mtcs_scoped_attributes *attrs;
   const char *ns_str = (ns != NULL_TREE) ? IDENTIFIER_POINTER (ns): NULL;
   attrs = find_attribute_namespace (self,ns_str);
   if (attrs == NULL)
      return NULL;
   attr.str = IDENTIFIER_POINTER (name);
   attr.length = IDENTIFIER_LENGTH (name);
   return attrs->attribute_hash->find_with_hash (&attr,substring_hash (attr.str, attr.length));
}


/* Return the spec for the attribute named NAME.  If NAME is a TREE_LIST,
   it also specifies the attribute namespace.  */
//原型 lookup_attribute_spec attribs.h attribs.cc
const struct attribute_spec *mtcs_attribs_lookup_attribute_spec (MtcsAttribs *self,const_tree name)
{
   tree ns;
   if (TREE_CODE (name) == TREE_LIST){
      ns = TREE_PURPOSE (name);
      name = TREE_VALUE (name);
   }else
      ns = get_gnu_namespace(self);
   return lookup_scoped_attribute_spec(self,ns, name);
}

/* See whether LIST contains at least one instance of attribute ATTR
   (possibly with different arguments).  Return the first such attribute
   if so, otherwise return null.  */
//原型 find_same_attribute attribs.cc
static tree find_same_attribute (const_tree attr, tree list)
{
  if (list == NULL_TREE)
    return NULL_TREE;
  tree ns = get_attribute_namespace (attr);
  tree name = get_attribute_name (attr);
  return private_lookup_attribute (ns ? IDENTIFIER_POINTER (ns) : nullptr,
               IDENTIFIER_POINTER (name),
               ns ? IDENTIFIER_LENGTH (ns) : 0,
               IDENTIFIER_LENGTH (name), list);
}

/* Check LAST_DECL and NODE of the same symbol for attributes that are
   recorded in SPEC to be mutually exclusive with ATTRNAME, diagnose
   them, and return true if any have been found.  NODE can be a DECL
   or a TYPE.  */
//原型 diag_attr_exclusions attribs.cc
static bool diag_attr_exclusions (tree last_decl, tree node, tree attrname,
            const attribute_spec *spec)
{
   const attribute_spec::exclusions *excl = spec->exclude;

   tree_code code = TREE_CODE (node);

   if ((code == FUNCTION_DECL && !excl->function
   && (!excl->type || !spec->affects_type_identity))
   || (code == VAR_DECL && !excl->variable
   && (!excl->type || !spec->affects_type_identity))
   || (((code == TYPE_DECL || RECORD_OR_UNION_TYPE_P (node)) && !excl->type)))
      return false;

   /* True if an attribute that's mutually exclusive with ATTRNAME
   has been found.  */
   bool found = false;

   if (last_decl && last_decl != node && TREE_TYPE (last_decl) != node){
      /* Check both the last DECL and its type for conflicts with
      the attribute being added to the current decl or type.  */
      found |= diag_attr_exclusions (last_decl, last_decl, attrname, spec);
      tree decl_type = TREE_TYPE (last_decl);
      found |= diag_attr_exclusions (last_decl, decl_type, attrname, spec);
   }

   /* NODE is either the current DECL to which the attribute is being
   applied or its TYPE.  For the former, consider the attributes on
   both the DECL and its type.  */
   tree attrs[2];

   if (DECL_P (node)){
      attrs[0] = DECL_ATTRIBUTES (node);
      if (TREE_TYPE (node))
         attrs[1] = TYPE_ATTRIBUTES (TREE_TYPE (node));
      else
         /* TREE_TYPE can be NULL e.g. while processing attributes on
         enumerators.  */
         attrs[1] = NULL_TREE;
   }else{
      attrs[0] = TYPE_ATTRIBUTES (node);
      attrs[1] = NULL_TREE;
   }

   /* Iterate over the mutually exclusive attribute names and verify
   that the symbol doesn't contain it.  */
   for (unsigned i = 0; i != ARRAY_SIZE (attrs); ++i){
      if (!attrs[i])
         continue;

      for ( ; excl->name; ++excl){
         /* Avoid checking the attribute against itself.  */
         if (is_attribute_p (excl->name, attrname))
            continue;

         if (!lookup_attribute (excl->name, attrs[i]))
            continue;

         /* An exclusion may apply either to a function declaration,
         type declaration, or a field/variable declaration, or
         any subset of the three.  */
         if (TREE_CODE (node) == FUNCTION_DECL  && !excl->function)
            continue;

         if (TREE_CODE (node) == TYPE_DECL && !excl->type)
            continue;

         if ((TREE_CODE (node) == FIELD_DECL  || VAR_P (node))  && !excl->variable)
            continue;

         found = true;

         /* Print a note?  */
         bool note = last_decl != NULL_TREE;
         auto_diagnostic_group d;
         if (TREE_CODE (node) == FUNCTION_DECL && fndecl_built_in_p (node))
            note &= warning (OPT_Wattributes,
            "ignoring attribute %qE in declaration of "
            "a built-in function %qD because it conflicts "
            "with attribute %qs",
            attrname, node, excl->name);
         else
            note &= warning (OPT_Wattributes,
            "ignoring attribute %qE because "
            "it conflicts with attribute %qs",
            attrname, excl->name);

         if (note)
            inform (DECL_SOURCE_LOCATION (last_decl),"previous declaration here");
      }
   }

   return found;
}


/* Process the attributes listed in ATTRIBUTES and install them in *NODE,
   which is either a DECL (including a TYPE_DECL) or a TYPE.  If a DECL,
   it should be modified in place; if a TYPE, a copy should be created
   unless ATTR_FLAG_TYPE_IN_PLACE is set in FLAGS.  FLAGS gives further
   information, in the form of a bitwise OR of flags in enum attribute_flags
   from tree.h.  Depending on these flags, some attributes may be
   returned to be applied at a later stage (for example, to apply
   a decl attribute to the declaration rather than to its type).  */
//原型 decl_attributes attribs.h attribs.cc
tree mtcs_attribs_decl_attributes (MtcsAttribs *self,tree *node, tree attributes, int flags,
       tree last_decl /* = NULL_TREE */)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsOpts *mtcsOpts=mtcs_target_get_opts(mtcsTarget);
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsOptionsItem *opts_set = mtcsOptions->global_options_set;

   tree returned_attrs = NULL_TREE;
   if (TREE_TYPE (*node) == mtcs_error_mark_node || attributes == mtcs_error_mark_node)
      return NULL_TREE;


   if (!self->attributes_initialized)
      mtcs_attribs_init_attributes/*!init_attributes*/(self);

   /* If this is a function and the user used #pragma GCC optimize, add the
   options to the attribute((optimize(...))) list.  */
   if (TREE_CODE (*node) == FUNCTION_DECL && mtcs_current_optimize_pragma){
      tree cur_attr = lookup_attribute ("optimize", attributes);
      tree opts = copy_list (mtcs_current_optimize_pragma);
      if (! cur_attr)
         attributes = tree_cons (get_identifier ("optimize"), opts, attributes);
      else
         TREE_VALUE (cur_attr) = chainon (opts, TREE_VALUE (cur_attr));
   }

   if (TREE_CODE (*node) == FUNCTION_DECL
   && (mtcs_optimization_current_node != mtcs_optimization_default_node
   || mtcs_target_option_current_node != mtcs_target_option_default_node)
   && !DECL_FUNCTION_SPECIFIC_OPTIMIZATION (*node)){
      DECL_FUNCTION_SPECIFIC_OPTIMIZATION (*node) = mtcs_optimization_current_node;
      /* Don't set DECL_FUNCTION_SPECIFIC_TARGET for targets that don't
      support #pragma GCC target or target attribute.  */
      if (mtcs_target_option_default_node){
         tree cur_tree   =mtcs_opts_build_optimization_node/*!build_target_option_node*/(mtcsOpts,mtcsOptionsItem, opts_set);
         tree old_tree = DECL_FUNCTION_SPECIFIC_TARGET (*node);
         if (!old_tree)
            old_tree = mtcs_target_option_default_node;
         /* The changes on optimization options can cause the changes in
         target options, update it accordingly if it's changed.  */
         if (old_tree != cur_tree)
            DECL_FUNCTION_SPECIFIC_TARGET (*node) = cur_tree;
      }
   }

   /* If this is a function and the user used #pragma GCC target, add the
   options to the attribute((target(...))) list.  */
   if (TREE_CODE (*node) == FUNCTION_DECL && mtcs_current_target_pragma
   && target_option_valid_attribute_p/*!targetm.target_option.valid_attribute_p*/(mtcsMachine->option,*node,
                  get_identifier ("target"), mtcs_current_target_pragma, 0)){
      tree cur_attr = lookup_attribute ("target", attributes);
      tree opts = copy_list (mtcs_current_target_pragma);

      if (! cur_attr)
         attributes = tree_cons (get_identifier ("target"), opts, attributes);
      else
         TREE_VALUE (cur_attr) = chainon (opts, TREE_VALUE (cur_attr));
   }

   /* A "naked" function attribute implies "noinline" and "noclone" for
   those targets that support it.  */
   if (TREE_CODE (*node) == FUNCTION_DECL
   && attributes
   && lookup_attribute ("naked", attributes) != NULL
   && mtcs_attribs_lookup_attribute_spec/*!lookup_attribute_spec*/(self,get_identifier ("naked"))
   && lookup_attribute ("noipa", attributes) == NULL)
      attributes = tree_cons (get_identifier ("noipa"), NULL, attributes);

   /* A "noipa" function attribute implies "noinline", "noclone" and "no_icf"
   for those targets that support it.  */
   if (TREE_CODE (*node) == FUNCTION_DECL
   && attributes
   && lookup_attribute ("noipa", attributes) != NULL
   && mtcs_attribs_lookup_attribute_spec/*!lookup_attribute_spec*/(self,get_identifier ("noipa"))){
      if (lookup_attribute ("noinline", attributes) == NULL)
         attributes = tree_cons (get_identifier ("noinline"), NULL, attributes);

      if (lookup_attribute ("noclone", attributes) == NULL)
         attributes = tree_cons (get_identifier ("noclone"),  NULL, attributes);

      if (lookup_attribute ("no_icf", attributes) == NULL)
         attributes = tree_cons (get_identifier ("no_icf"),  NULL, attributes);
   }

   mtcsTarget/*!targetm.insert_attributes*/->insert_attributes(mtcsTarget,*node, &attributes);

   /* Note that attributes on the same declaration are not necessarily
   in the same order as in the source.  */
   for (tree attr = attributes; attr; attr = TREE_CHAIN (attr)){
      tree ns = get_attribute_namespace (attr);
      tree name = get_attribute_name (attr);
      tree args = TREE_VALUE (attr);
      tree *anode = node;
      const struct attribute_spec *spec  = lookup_scoped_attribute_spec(self,ns, name);
      int fn_ptr_quals = 0;
      tree fn_ptr_tmp = NULL_TREE;
      const bool cxx11_attr_p = cxx11_attribute_p (attr);

      if (spec == NULL){
         if (!(flags & (int) ATTR_FLAG_BUILT_IN)   && !attr_namespace_ignored_p(self,ns)){
            if (ns == NULL_TREE || !cxx11_attr_p)
               warning (OPT_Wattributes, "%qE attribute directive ignored", name);
            else if ((flag_openmp || flag_openmp_simd) && is_attribute_p ("omp", ns)
                  && is_attribute_p ("directive", name) && (VAR_P (*node) || TREE_CODE (*node) == FUNCTION_DECL))
               continue;
            else
               warning (OPT_Wattributes,"%<%E::%E%> scoped attribute directive ignored",ns, name);
         }
         continue;
      }else{
         int nargs = list_length (args);
         if (nargs < spec->min_length   || (spec->max_length >= 0  && nargs > spec->max_length)) {
            auto_diagnostic_group d;
            error ("wrong number of arguments specified for %qE attribute",name);
            if (spec->max_length < 0)
               inform (input_location, "expected %i or more, found %i",spec->min_length, nargs);
            else if (spec->min_length == spec->max_length)
               inform (input_location, "expected %i, found %i",spec->min_length, nargs);
            else
               inform (input_location, "expected between %i and %i, found %i",spec->min_length, spec->max_length, nargs);
            continue;
         }
      }
      gcc_assert (is_attribute_p (spec->name, name));

      if (spec->decl_required && !DECL_P (*anode)){
         if (flags & ((int) ATTR_FLAG_DECL_NEXT
         | (int) ATTR_FLAG_FUNCTION_NEXT
         | (int) ATTR_FLAG_ARRAY_NEXT)){
            /* Pass on this attribute to be tried again.  */
            tree attr = tree_cons (name, args, NULL_TREE);
            returned_attrs = chainon (returned_attrs, attr);
            continue;
         }else{
            warning (OPT_Wattributes, "%qE attribute does not apply to types",name);
            continue;
         }
      }

      /* If we require a type, but were passed a decl, set up to make a
      new type and update the one in the decl.  ATTR_FLAG_TYPE_IN_PLACE
      would have applied if we'd been passed a type, but we cannot modify
      the decl's type in place here.  */
      if (spec->type_required && DECL_P (*anode)){
         anode = &TREE_TYPE (*anode);
         flags &= ~(int) ATTR_FLAG_TYPE_IN_PLACE;
      }

      if (spec->function_type_required  && !FUNC_OR_METHOD_TYPE_P (*anode)){
         if (TREE_CODE (*anode) == POINTER_TYPE   && FUNC_OR_METHOD_TYPE_P (TREE_TYPE (*anode))){
            /* OK, this is a bit convoluted.  We can't just make a copy
            of the pointer type and modify its TREE_TYPE, because if
            we change the attributes of the target type the pointer
            type needs to have a different TYPE_MAIN_VARIANT.  So we
            pull out the target type now, frob it as appropriate, and
            rebuild the pointer type later.

            This would all be simpler if attributes were part of the
            declarator, grumble grumble.  */
            fn_ptr_tmp = TREE_TYPE (*anode);
            fn_ptr_quals = TYPE_QUALS (*anode);
            anode = &fn_ptr_tmp;
            flags &= ~(int) ATTR_FLAG_TYPE_IN_PLACE;
         }else if (flags & (int) ATTR_FLAG_FUNCTION_NEXT){
            /* Pass on this attribute to be tried again.  */
            tree attr = tree_cons (name, args, NULL_TREE);
            returned_attrs = chainon (returned_attrs, attr);
            continue;
         }

         if (TREE_CODE (*anode) != FUNCTION_TYPE  && TREE_CODE (*anode) != METHOD_TYPE){
            warning (OPT_Wattributes, "%qE attribute only applies to function types",name);
            continue;
         }
      }

      if (TYPE_P (*anode) && (flags & (int) ATTR_FLAG_TYPE_IN_PLACE) && COMPLETE_TYPE_P (*anode)){
         warning (OPT_Wattributes, "type attributes ignored after type is already defined");
         continue;
      }

      bool no_add_attrs = false;

      /* Check for exclusions with other attributes on the current
      declation as well as the last declaration of the same
      symbol already processed (if one exists).  Detect and
      reject incompatible attributes.  */
      bool built_in = flags & ATTR_FLAG_BUILT_IN;
      if (spec->exclude && (mtcsOptionsItem->x_flag_checking || !built_in)
            && !mtcs_tree_error_operand_p/*!error_operand_p*/(mtcsTree,last_decl)){
         /* Always check attributes on user-defined functions.
         Check them on built-ins only when -fchecking is set.
         Ignore __builtin_unreachable -- it's both const and
         noreturn.  */

         if (!built_in
         || !DECL_P (*anode)
         || DECL_BUILT_IN_CLASS (*anode) != BUILT_IN_NORMAL
         || (DECL_FUNCTION_CODE (*anode) != BUILT_IN_UNREACHABLE
         && DECL_FUNCTION_CODE (*anode) != BUILT_IN_UNREACHABLE_TRAP
         && (DECL_FUNCTION_CODE (*anode) != BUILT_IN_UBSAN_HANDLE_BUILTIN_UNREACHABLE))){
            bool no_add = diag_attr_exclusions (last_decl, *anode, name, spec);
            if (!no_add && anode != node)
               no_add = diag_attr_exclusions (last_decl, *node, name, spec);
            no_add_attrs |= no_add;
         }
      }

      if (no_add_attrs
      /* Don't add attributes registered just for -Wno-attributes=foo::bar
      purposes.  */
      || mtcs_attribs_attribute_ignored_p/*!attribute_ignored_p*/(self,attr))
         continue;

      if (spec->handler != NULL){
         int cxx11_flag = (cxx11_attr_p ? ATTR_FLAG_CXX11 : 0);

         /* Pass in an array of the current declaration followed
         by the last pushed/merged declaration if one exists.
         For calls that modify the type attributes of a DECL
         and for which *ANODE is *NODE's type, also pass in
         the DECL as the third element to use in diagnostics.
         If the handler changes CUR_AND_LAST_DECL[0] replace
         *ANODE with its value.  */
         tree cur_and_last_decl[3] = { *anode, last_decl };
         if (anode != node && DECL_P (*node))
            cur_and_last_decl[2] = *node;

         tree ret = (spec->handler) (cur_and_last_decl, name, args,  flags|cxx11_flag, &no_add_attrs);

         /* Fix up typedefs clobbered by attribute handlers.  */
         if (TREE_CODE (*node) == TYPE_DECL
         && anode == &TREE_TYPE (*node)
         && DECL_ORIGINAL_TYPE (*node)
         && TYPE_NAME (*anode) == *node
         && TYPE_NAME (cur_and_last_decl[0]) != *node){
            tree t = cur_and_last_decl[0];
            DECL_ORIGINAL_TYPE (*node) = t;
            tree tt = build_variant_type_copy (t);
            cur_and_last_decl[0] = tt;
            TREE_TYPE (*node) = tt;
            TYPE_NAME (tt) = *node;
         }

         if (*anode != cur_and_last_decl[0]){
            /* Even if !spec->function_type_required, allow the attribute
            handler to request the attribute to be applied to the function
            type, rather than to the function pointer type, by setting
            cur_and_last_decl[0] to the function type.  */
            if (!fn_ptr_tmp
            && POINTER_TYPE_P (*anode)
            && TREE_TYPE (*anode) == cur_and_last_decl[0]
            && FUNC_OR_METHOD_TYPE_P (TREE_TYPE (*anode))){
               fn_ptr_tmp = TREE_TYPE (*anode);
               fn_ptr_quals = TYPE_QUALS (*anode);
               anode = &fn_ptr_tmp;
            }
            *anode = cur_and_last_decl[0];
         }

         if (ret == error_mark_node){
            warning (OPT_Wattributes, "%qE attribute ignored", name);
            no_add_attrs = true;
         }else
            returned_attrs = chainon (ret, returned_attrs);
      }

      /* Layout the decl in case anything changed.  */
      if (spec->type_required && DECL_P (*node)
      && (VAR_P (*node)
      || TREE_CODE (*node) == PARM_DECL
      || TREE_CODE (*node) == RESULT_DECL))
         mtcs_stor_layout_relayout_decl/*!relayout_decl*/(mtcsStorLayout,*node);

      if (!no_add_attrs){
         tree old_attrs;
         tree a;

         if (DECL_P (*anode))
            old_attrs = DECL_ATTRIBUTES (*anode);
         else
            old_attrs = TYPE_ATTRIBUTES (*anode);

         for (a = find_same_attribute (attr, old_attrs); a != NULL_TREE;
               a = find_same_attribute (attr, TREE_CHAIN (a))){
            if (simple_cst_equal (TREE_VALUE (a), args) == 1)
               break;
         }

         if (a == NULL_TREE){
            /* This attribute isn't already in the list.  */
            tree r;
            /* Preserve the C++11 form.  */
            if (cxx11_attr_p)
               r = tree_cons (build_tree_list (ns, name), args, old_attrs);
            else
               r = tree_cons (name, args, old_attrs);

            if (DECL_P (*anode))
               DECL_ATTRIBUTES (*anode) = r;
            else if (flags & (int) ATTR_FLAG_TYPE_IN_PLACE){
               TYPE_ATTRIBUTES (*anode) = r;
               /* If this is the main variant, also push the attributes
               out to the other variants.  */
               if (*anode == TYPE_MAIN_VARIANT (*anode)){
                  for (tree variant = *anode; variant; variant = TYPE_NEXT_VARIANT (variant)){
                     if (TYPE_ATTRIBUTES (variant) == old_attrs)
                        TYPE_ATTRIBUTES (variant) = TYPE_ATTRIBUTES (*anode);
                     else if (!find_same_attribute (attr, TYPE_ATTRIBUTES (variant)))
                        TYPE_ATTRIBUTES (variant) = tree_cons (name, args, TYPE_ATTRIBUTES (variant));
                  }
               }
            }else
               *anode = build_type_attribute_variant (*anode, r);
         }
      }

      if (fn_ptr_tmp){
         /* Rebuild the function pointer type and put it in the
         appropriate place.  */
         fn_ptr_tmp = mtcs_tree_build_pointer_type/*!build_pointer_type*/(mtcsTree,fn_ptr_tmp);
         if (fn_ptr_quals)
            fn_ptr_tmp = mtcs_tree_build_qualified_type/*!build_qualified_type*/(mtcsTree,fn_ptr_tmp, fn_ptr_quals);
         if (DECL_P (*node))
            TREE_TYPE (*node) = fn_ptr_tmp;
         else{
            gcc_assert (TREE_CODE (*node) == POINTER_TYPE);
            *node = fn_ptr_tmp;
         }
      }
   }//end for

   return returned_attrs;
}

/* Return true if the attribute ATTR should not be warned about.  */
//原型 attribute_ignored_p attribs.h attribs.cc
bool mtcs_attribs_attribute_ignored_p (MtcsAttribs *self,tree attr)
{
   if (!cxx11_attribute_p (attr))
   return false;
   if (tree ns = get_attribute_namespace (attr)){
      const attribute_spec *as = mtcs_attribs_lookup_attribute_spec/*!lookup_attribute_spec*/(self,TREE_PURPOSE (attr));
      if (as == NULL && attr_namespace_ignored_p(self,ns))
         return true;
      if (as && as->max_length == -2)
         return true;
   }
   return false;
}



/* Insert a single attribute ATTR into a namespace of attributes.  */
//原型 register_scoped_attribute attribs.cc
static void register_scoped_attribute (MtcsAttribs *self,const struct attribute_spec *attr,
            mtcs_scoped_attributes *name_space)
{
  struct mtcs_substring str;
  attribute_spec **slot;

  gcc_assert (attr != NULL && name_space != NULL);

  gcc_assert (name_space->attribute_hash);

  str.str = attr->name;
  str.length = strlen (str.str);

  /* Attribute names in the table must be in the form 'text' and not
     in the form '__text__'.  */
  gcc_checking_assert (!canonicalize_attr_name (str.str, str.length));

  slot = name_space->attribute_hash->find_slot_with_hash (&str,
        substring_hash (str.str, str.length),INSERT);
  gcc_assert (!*slot || attr->name[0] == '*');
  *slot = CONST_CAST (struct attribute_spec *, attr);
}

/* Insert SPECS into its namespace.  IGNORED_P is true iff all unknown
   attributes in this namespace should be ignored for the purposes of
   -Wattributes.  The function returns the namespace into which the
   attributes have been registered.  */
//原型 register_scoped_attributes attribs.h attribs.cc
mtcs_scoped_attributes *mtcs_attribs_register_scoped_attributes (MtcsAttribs *self,const scoped_attribute_specs &specs,
             bool ignored_p /*=false*/)
{
   mtcs_scoped_attributes *result = NULL;

   /* See if we already have attributes in the namespace NS.  */
   result = find_attribute_namespace(self,specs.ns);

   if (result == NULL){
      /* We don't have any namespace NS yet.  Create one.  */
      mtcs_scoped_attributes sa;

      if (self->attributes_table.is_empty ())
         self->attributes_table.create (64);

      memset (&sa, 0, sizeof (sa));
      sa.ns = specs.ns;
      sa.attributes.create (64);
      sa.ignored_p = ignored_p;
      result = self->attributes_table.safe_push (sa);
      result->attribute_hash = new hash_table<mtcs_attribute_hasher> (200);
   }else
      result->ignored_p |= ignored_p;

   /* Really add the attributes to their namespace now.  */
   for (const attribute_spec &attribute : specs.attributes){
      result->attributes.safe_push (attribute);
      register_scoped_attribute(self,&attribute, result);
   }
   gcc_assert (result != NULL);
   return result;
}

/* Return a type like TTYPE except that its TYPE_ATTRIBUTE
   is ATTRIBUTE and its qualifiers are QUALS.

   Record such modified types already made so we don't make duplicates.  */
//原型 build_type_attribute_qual_variant tree.h attribs.cc
tree mtcs_attribs_build_type_attribute_qual_variant (MtcsAttribs *self,tree otype, tree attribute, int quals)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsLang *mtcsLang=mtcs_target_get_lang(mtcsTarget);

   tree ttype = otype;
   if (! attribute_list_equal (TYPE_ATTRIBUTES (ttype), attribute)){
      tree ntype;

   /* Building a distinct copy of a tagged type is inappropriate; it
   causes breakage in code that expects there to be a one-to-one
   relationship between a struct and its fields.
   build_duplicate_type is another solution (as used in
   handle_transparent_union_attribute), but that doesn't play well
   with the stronger C++ type identity model.  */
   if (RECORD_OR_UNION_TYPE_P (ttype) || TREE_CODE (ttype) == ENUMERAL_TYPE){
      warning (OPT_Wattributes, "ignoring attributes applied to %qT after definition",
            TYPE_MAIN_VARIANT (ttype));
      return mtcs_tree_build_qualified_type/*!build_qualified_type*/(mtcsTree,ttype, quals);
   }

   ttype = mtcs_tree_build_qualified_type/*!build_qualified_type*/(mtcsTree,ttype, TYPE_UNQUALIFIED);
   if (mtcsLang/*!lang_hooks.types.copy_lang_qualifiers*/->types.copy_lang_qualifiers
   && otype != TYPE_MAIN_VARIANT (otype))
      ttype = (mtcsLang/*!lang_hooks.types.copy_lang_qualifiers*/->types.copy_lang_qualifiers(mtcsLang,
            ttype, TYPE_MAIN_VARIANT (otype)));

   tree dtype = ntype = build_distinct_type_copy (ttype);

   TYPE_ATTRIBUTES (ntype) = attribute;

   hashval_t hash =mtcs_tree_type_hash_canon_hash/*!type_hash_canon_hash*/(mtcsTree,ntype);
   ntype = mtcs_tree_type_hash_canon/*!type_hash_canon*/(mtcsTree,hash, ntype);

   if (ntype != dtype)
      /* This variant was already in the hash table, don't mess with
      TYPE_CANONICAL.  */
      ;
   else if (TYPE_STRUCTURAL_EQUALITY_P (ttype)
   || !mtcs_attribs_comp_type_attributes/*!comp_type_attributes*/(self,ntype, ttype))
      /* If the target-dependent attributes make NTYPE different from
      its canonical type, we will need to use structural equality
      checks for this type.

      We shouldn't get here for stripping attributes from a type;
      the no-attribute type might not need structural comparison.  But
      we can if was discarded from type_hash_table.  */
      SET_TYPE_STRUCTURAL_EQUALITY (ntype);
   else if (TYPE_CANONICAL (ntype) == ntype)
      TYPE_CANONICAL (ntype) = TYPE_CANONICAL (ttype);

   ttype = mtcs_tree_build_qualified_type/*!build_qualified_type*/(mtcsTree,ntype, quals);
   if (mtcsLang/*!lang_hooks.types.copy_lang_qualifiers*/->types.copy_lang_qualifiers
   && otype != TYPE_MAIN_VARIANT (otype))
      ttype = mtcsLang/*!lang_hooks.types.copy_lang_qualifiers*/->types.copy_lang_qualifiers(mtcsLang,ttype, otype);
   }else if (TYPE_QUALS (ttype) != quals)
      ttype = mtcs_tree_build_qualified_type/*!build_qualified_type*/(mtcsTree,ttype, quals);

   return ttype;
}

//原型 build_type_attribute_variant attribs.h attribs.cc
tree mtcs_attribs_build_type_attribute_variant (MtcsAttribs *self,tree ttype, tree attribute)
{
  return mtcs_attribs_build_type_attribute_qual_variant/*!build_type_attribute_qual_variant*/(self,
        ttype, attribute, TYPE_QUALS (ttype));
}

/* Return 0 if the attributes for two types are incompatible, 1 if they
   are compatible, and 2 if they are nearly compatible (which causes a
   warning to be generated).  */
//原型 comp_type_attributes attribs.h attribs.cc
int mtcs_attribs_comp_type_attributes (MtcsAttribs *self,const_tree type1, const_tree type2)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   const_tree a1 = TYPE_ATTRIBUTES (type1);
   const_tree a2 = TYPE_ATTRIBUTES (type2);
   const_tree a;

   if (a1 == a2)
      return 1;
   for (a = a1; a != NULL_TREE; a = TREE_CHAIN (a)){
      const struct attribute_spec *as;
      const_tree attr;

      as = mtcs_attribs_lookup_attribute_spec/*!lookup_attribute_spec*/(self,TREE_PURPOSE (a));
      if (!as || as->affects_type_identity == false)
         continue;

      attr = find_same_attribute (a, CONST_CAST_TREE (a2));
      if (!attr || !attribute_value_equal (a, attr))
         break;
   }
   if (!a){
      for (a = a2; a != NULL_TREE; a = TREE_CHAIN (a)){
         const struct attribute_spec *as;

         as = lookup_attribute_spec (TREE_PURPOSE (a));
         if (!as || as->affects_type_identity == false)
            continue;

         if (!find_same_attribute (a, CONST_CAST_TREE (a1)))
            break;
         /* We don't need to compare trees again, as we did this
         already in first loop.  */
      }
      /* All types - affecting identity - are equal, so
      there is no need to call target hook for comparison.  */
      if (!a)
         return 1;
   }
   if (lookup_attribute ("transaction_safe", CONST_CAST_TREE (a)))
      return 0;
   if ((lookup_attribute ("nocf_check", TYPE_ATTRIBUTES (type1)) != NULL)
   ^ (lookup_attribute ("nocf_check", TYPE_ATTRIBUTES (type2)) != NULL))
      return 0;
   int strub_ret = strub_comptypes (CONST_CAST_TREE (type1),
   CONST_CAST_TREE (type2));
   if (strub_ret == 0)
      return strub_ret;
   /* As some type combinations - like default calling-convention - might
   be compatible, we have to call the target hook to get the final result.  */
   int target_ret = mtcsTarget/*!targetm.comp_type_attributes*/->comp_type_attributes(mtcsTarget,type1, type2);
   if (target_ret == 0)
      return target_ret;
   if (strub_ret == 2 || target_ret == 2)
      return 2;
   if (strub_ret == 1 && target_ret == 1)
      return 1;
   gcc_unreachable ();
}


