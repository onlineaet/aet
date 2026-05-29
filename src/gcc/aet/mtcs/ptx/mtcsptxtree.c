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
//#define IN_TARGET_CODE 1 //不加这句在machmode.h中的GET_MODE_SIZE编译到poly_uint16(poly_int) 因为poly_int没有重载>号，所以编译报错
//insn-modes.h由nvptx生成，但i386生成的类型全覆盖nvptx的insn-modes.h,不需要平台的？？？
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

#include "mtcsptxtree.h"
#include "ptx-common.h"

//原型  struct lang_hooks...tree (*builtin_function) (tree decl);#define LANG_HOOKS_BUILTIN_FUNCTION lhd_builtin_function
static tree builtinFunction_cb (MtcsTree *mtcsTree,tree decl);

static void mtcsPtxTreeInit(MtcsPtxTree *self)
{
    MtcsTree *mtcsTree=(MtcsTree *)self;
    //原型 INT_TYPE_SIZE; default.h 平台定义 nvptx.h
    mtcs_tree_set_int_type_size(mtcsTree,PTX_INT_TYPE_SIZE);
    //原型 LONG_TYPE_SIZE; default.h 平台定义 nvptx.h
    mtcs_tree_set_long_type_size(mtcsTree,PTX_LONG_TYPE_SIZE);
    //原型 LONG_LONG_TYPE_SIZE; default.h 平台定义 nvptx.h
    mtcs_tree_set_long_long_type_size(mtcsTree,PTX_LONG_LONG_TYPE_SIZE);
    //原型 SHORT_TYPE_SIZE; default.h 平台定义 nvptx.h
    mtcs_tree_set_short_type_size(mtcsTree,PTX_SHORT_TYPE_SIZE);
    //原型 #define SIZETYPE SIZE_TYPE  defaults.h SIZE_TYPE 每个平台定义不一样
    mtcs_tree_set_sizetype_string(mtcsTree,PTX_SIZE_TYPE);
    //#define PTRDIFF_TYPE (TARGET_ABI64 ? "long int" : "int")
    mtcs_tree_set_prt_diff_type_string(mtcsTree,PTX_PTRDIFF_TYPE);
    //原型 #define POINTER_SIZE BITS_PER_WORD
    mtcs_tree_set_pointer_size(mtcsTree,PTX_POINTER_SIZE);
    //原型 #define FLOAT_TYPE_SIZE BITS_PER_WORD
    mtcs_tree_set_float_type_size(mtcsTree, PTX_FLOAT_TYPE_SIZE);
    //原型 #define DOUBLE_TYPE_SIZE (BITS_PER_WORD * 2)
    mtcs_tree_set_double_type_size(mtcsTree,PTX_DOUBLE_TYPE_SIZE);
    //原型 #define LONG_DOUBLE_TYPE_SIZE (BITS_PER_WORD * 2)
    mtcs_tree_set_long_double_type_size(mtcsTree,PTX_LONG_DOUBLE_TYPE_SIZE);
    //原型 #define POINTER_SIZE_UNITS ((POINTER_SIZE + BITS_PER_UNIT - 1) / BITS_PER_UNIT)
    mtcs_tree_set_pointer_size_units(mtcsTree, ((PTX_POINTER_SIZE + BITS_PER_UNIT - 1) / BITS_PER_UNIT));

    //原型  struct lang_hooks...tree (*builtin_function) (tree decl);#define LANG_HOOKS_BUILTIN_FUNCTION lhd_builtin_function
    mtcsTree->builtin_function=builtinFunction_cb;
}

//原型  struct lang_hooks...tree (*builtin_function) (tree decl);#define LANG_HOOKS_BUILTIN_FUNCTION lhd_builtin_function
static tree builtinFunction_cb (MtcsTree *mtcsTree,tree decl)
{
   //fprintf(stderr,"mtcsptxtreeg.cc builtin_function %s\n",IDENTIFIER_POINTER(DECL_NAME(decl)));
   return decl;
}

MtcsPtxTree *mtcs_ptx_tree_new(MtcsMode *mtcsMode)
{
   MtcsPtxTree *self = n_slice_alloc0 (sizeof(MtcsPtxTree));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_tree_init((MtcsTree *)self);
   mtcsPtxTreeInit(self);
   return self;
}

