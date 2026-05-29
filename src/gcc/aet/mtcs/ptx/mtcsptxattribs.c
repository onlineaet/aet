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
#include "target-def.h"


#include "aet/aetprinttree.h"
#include "../mtcstool.h"
#include "mtcsptxattribs.h"
#include "ptx-common.h"

static tree nvptx_handle_kernel_attribute (tree *node, tree name, tree ARG_UNUSED (args),
                   int ARG_UNUSED (flags), bool *no_add_attrs);
static tree nvptx_handle_shared_attribute (tree *node, tree name, tree ARG_UNUSED (args),
                   int ARG_UNUSED (flags), bool *no_add_attrs);

/* Table of valid machine attributes.  */
TARGET_GNU_ATTRIBUTES (nvptx_attribute_table,
{
  /* { name, min_len, max_len, decl_req, type_req, fn_type_req,
       affects_type_identity, handler, exclude } */
  { "kernel", 0, 0, true, false,  false, false, nvptx_handle_kernel_attribute,
    NULL },
  { "shared", 0, 0, true, false,  false, false, nvptx_handle_shared_attribute,
    NULL }
});

static void setAttributeTable_cb(MtcsAttribs *self);


static void mtcsPtxAttribsInit(MtcsPtxAttribs *self)
{
   MtcsAttribs *mtcsAttribs=(MtcsAttribs *)self;
   //原型 #define TARGET_ATTRIBUTE_TABLE nvptx_attribute_table
   mtcsAttribs->set_attribute_table=setAttributeTable_cb;
}

static void setAttributeTable_cb(MtcsAttribs *mtcsAttribs)
{
   mtcsAttribs->attribute_tables[1] = nvptx_attribute_table;
}

/* Handle a "kernel" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree nvptx_handle_kernel_attribute (tree *node, tree name, tree ARG_UNUSED (args),
                   int ARG_UNUSED (flags), bool *no_add_attrs)
{
   fprintf(stderr,"-----nvptx.cc -----137-- nvptx_handle_kernel_attribute\n");
   tree decl = *node;
   if (TREE_CODE (decl) != FUNCTION_DECL){
      error ("%qE attribute only applies to functions", name);
      *no_add_attrs = true;
   }else if (!VOID_TYPE_P (TREE_TYPE (TREE_TYPE (decl)))){
      error ("%qE attribute requires a void return type", name);
      *no_add_attrs = true;
   }
   return NULL_TREE;
}


/* Handle a "shared" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree nvptx_handle_shared_attribute (tree *node, tree name, tree ARG_UNUSED (args),
                   int ARG_UNUSED (flags), bool *no_add_attrs)
{
   fprintf(stderr,"-----nvptx.cc -----136-- nvptx_handle_shared_attribute\n");
   tree decl = *node;
   if (TREE_CODE (decl) != VAR_DECL){
      error ("%qE attribute only applies to variables", name);
      *no_add_attrs = true;
   }else if (!(TREE_PUBLIC (decl) || TREE_STATIC (decl))){
      error ("%qE attribute not allowed with auto storage class", name);
      *no_add_attrs = true;
   }
   return NULL_TREE;
}


MtcsPtxAttribs *mtcs_ptx_attribs_new(MtcsMode *mtcsMode)
{
     MtcsPtxAttribs *self = n_slice_alloc0 (sizeof(MtcsPtxAttribs));
     mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
     mtcs_attribs_init((MtcsAttribs *)self);
     mtcsPtxAttribsInit(self);
     return self;
}


