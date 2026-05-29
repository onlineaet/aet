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

#ifndef __GCC_MTCS_ATTRIBS__
#define __GCC_MTCS_ATTRIBS__

#include "../nlib.h"
#include "mtcsmicro.h"
#include "mtcscomponent.h"

/* Scoped attribute name representation.  */
/* Used for attribute_hash.  */
/* Substring representation.  */
struct mtcs_substring
{
  const char *str;
  int length;
};

struct mtcs_attribute_hasher : nofree_ptr_hash <attribute_spec>
{
  typedef mtcs_substring *compare_type;
  static inline hashval_t hash (const attribute_spec *);
  static inline bool equal (const attribute_spec *, const mtcs_substring *);
};

struct mtcs_scoped_attributes
{
  const char *ns;
  vec<attribute_spec> attributes;
  hash_table<mtcs_attribute_hasher> *attribute_hash;
  /* True if we should not warn about unknown attributes in this NS.  */
  bool ignored_p;
};

typedef struct _MtcsAttribs MtcsAttribs;
struct _MtcsAttribs
{
   MtcsComponent parent;
   //原型 attributes_initialized attribs.cc
   bool attributes_initialized;// = false;
   //原型 attribute_tables attribs.cc
   array_slice<const scoped_attribute_specs *const> attribute_tables[2];
   //原型 #define TARGET_ATTRIBUTE_TABLE nvptx_attribute_table
   void (*set_attribute_table)(MtcsAttribs *self);
   //原型 gnu_namespace_cache attribs.cc
   GTY(()) tree gnu_namespace_cache;

   //原型 attributes_table attribs.cc
   vec<mtcs_scoped_attributes> attributes_table;
};



void mtcs_attribs_init(MtcsAttribs *self);

//原型 decl_attributes attribs.h attribs.cc
tree mtcs_attribs_decl_attributes (MtcsAttribs *self,tree *node, tree attributes, int flags,
       tree last_decl  = NULL_TREE);
//原型 init_attributes attribs.h attribs.cc
void mtcs_attribs_init_attributes (MtcsAttribs *self);
//原型 lookup_attribute_spec attribs.h attribs.cc
const struct attribute_spec *mtcs_attribs_lookup_attribute_spec (MtcsAttribs *self,const_tree name);
//原型 attribute_ignored_p attribs.h attribs.cc
bool mtcs_attribs_attribute_ignored_p (MtcsAttribs *self,tree attr);
//原型 register_scoped_attributes attribs.h attribs.cc
mtcs_scoped_attributes *mtcs_attribs_register_scoped_attributes (MtcsAttribs *self,const scoped_attribute_specs &specs,
             bool ignored_p =false);
//原型 build_type_attribute_qual_variant tree.h attribs.cc
tree mtcs_attribs_build_type_attribute_qual_variant (MtcsAttribs *self,tree otype, tree attribute, int quals);
//原型 build_type_attribute_variant attribs.h attribs.cc
tree mtcs_attribs_build_type_attribute_variant (MtcsAttribs *self,tree ttype, tree attribute);
//原型 comp_type_attributes attribs.h attribs.cc
int mtcs_attribs_comp_type_attributes (MtcsAttribs *self,const_tree type1, const_tree type2);

#endif
