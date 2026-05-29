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


#ifndef __GCC_MTCS_EXPAND__
#define __GCC_MTCS_EXPAND__

#include "../nlib.h"
#include "bitmap.h"
#include "mtcscomponent.h"
#include "mtcspass.h"


typedef struct _StackVar  StackVar;
struct _StackVar
{
    /* The Variable.  */
      tree decl;

      /* Initially, the size of the variable.  Later, the size of the partition,
         if this variable becomes it's partition's representative.  */
      poly_uint64 size;

      /* The *byte* alignment required for this variable.  Or as, with the
         size, the alignment for this partition.  */
      unsigned int alignb;

      /* The partition representative.  */
      size_t representative;

      /* The next stack variable in the partition, or EOC.  */
      size_t next;

      /* The numbers of conflicting stack variables.  */
      bitmap conflicts;
};

//原型 cfgexpand.cc
typedef struct _MtcsExpand MtcsExpand;
struct _MtcsExpand
{
    MtcsComponent parent;
    //原型 currently_expanding_to_rtl rtl.h rtl.cc
    int currently_expanding_to_rtl;
    /* The phase of the stack frame.  This is the known misalignment of
       virtual_stack_vars_rtx from PREFERRED_STACK_BOUNDARY.  That is,
       (frame_offset+frame_phase) % PREFERRED_STACK_BOUNDARY == 0.  */
    int frame_phase;
    /* Conflict bitmaps go on this obstack.  This allows us to destroy
       all of them in one big sweep.  */
    bitmap_obstack stack_var_bitmap_obstack;
    StackVar *stack_vars;
    size_t stack_vars_alloc;
    size_t stack_vars_num;
    hash_map<tree, size_t> *decl_to_stack_part;
    /* Used during expand_used_vars to remember if we saw any decls for
       which we'd like to enable stack smashing protection.  */
    bool has_protected_decls;
    /* Used during expand_used_vars.  Remember if we say a character buffer
       smaller than our cutoff threshold.  Used for -Wstack-protector.  */
    bool has_short_buffer;
    size_t *stack_vars_sorted;
    //原型 deep_ter_debug_map cfgexpand.cc
    hash_map<tree, tree> *deep_ter_debug_map;
    //原型 lab_rtx_for_bb cfgexpand.cc
    hash_map<basic_block, rtx_code_label *> *lab_rtx_for_bb;

};

MtcsExpand *mtcs_expand_new(MtcsMode *mtcsMode);
//原型 estimated_stack_frame_size cfgexpand.h cfgexpand.cc
HOST_WIDE_INT mtcs_expand_estimated_stack_frame_size (MtcsExpand *self,struct cgraph_node *node);
//原型 unsigned int pass_expand::execute (function *fun) cfgexpand.cc
nuint mtcs_expand_execute(MtcsExpand *self,function *fun);
//原型 set_parm_rtl cfgexpand.h cfgexpand.cc
void mtcs_expand_set_parm_rtl (MtcsExpand *self,tree parm, rtx x);
//原型 expand_null_return rtl.h cfgexpand.cc
void mtcs_expand_expand_null_return (MtcsExpand *self);

//原型 NEXT_PASS (pass_expand, 1);   RTL_PASS    cfgexpand.cc    expand     y  无条件执行
typedef struct _MtcsPassExpand  MtcsPassExpand;
struct _MtcsPassExpand
{
   MtcsPass parent;
};
MtcsPassExpand *mtcs_pass_expand_new(MtcsMode *mtcsMode);

#endif
