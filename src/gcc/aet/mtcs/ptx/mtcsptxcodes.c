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
#include "config.h"
#define INCLUDE_ALGORITHM /* reverse */
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "tree-pass.h"
#include "cfghooks.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "tree-pretty-print.h" /* for dump_function_header */
#include "varasm.h"
#include "insn-attr.h"
#include "conditions.h"
#include "flags.h"
#include "output.h"
#include "except.h"
#include "rtl-error.h"
#include "toplev.h" /* exact_log2, floor_log2 */
#include "reload.h"
#include "intl.h"
#include "cfgrtl.h"
#include "debug.h"
#include "tree-pass.h"
#include "tree-ssa.h"
#include "cfgloop.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "rtl-iter.h"
#include "print-rtl.h"
#include "function-abi.h"
#include "common/common-target.h"
#include "diagnostic.h"
#include "context.h"
#include "options.h"
#include "expmed.h"
#include "expr.h"
#include "fold-const.h"
#include "dwarf2out.h"
#include "common/common-targhooks.h"

#include "aet/aetprinttree.h"
#include "gen/ptx-insn-codes.h"
#include "../mtcstarget.h"
#include "ptx-common.h"
#include "mtcsptxcodes.h"

//原型 targetm.code_for_allocate_stack #define TARGET_CODE_FOR_ALLOCATE_STACK CODE_FOR_allocate_stack
static int getCodeForAllocateStack_cb(MtcsCodes *mtcsCodes);

static void mtcsPtxCodeInit(MtcsPtxCodes *self)
{
   MtcsCodes *mtcsCodes=(MtcsCodes *)self;
   mtcs_codes_set_number(mtcsCodes,PTX_NUM_INSN_CODES);
   mtcsCodes->get_code_for_allocate_stack=getCodeForAllocateStack_cb;
}

///home/sns/workspace/gcc-14-20240421/src/build-nvptx-gcc/gcc/insn-target-def.h CODE_FOR_allocate_stack
//home/sns/workspace/gcc-14-20240421/src/build-nvptx-gcc/gcc/target-hooks-def.h CODE_FOR_nothing
static int getCodeForAllocateStack_cb(MtcsCodes *mtcsCodes)
{
   return PTX_CODE_FOR_allocate_stack;
}


MtcsPtxCodes  *mtcs_ptx_codes_new(MtcsMode *mtcsMode)
{
   MtcsPtxCodes *self = n_slice_alloc0 (sizeof(MtcsPtxCodes));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_codes_init((MtcsCodes *)self);
   mtcsPtxCodeInit(self);
   return self;
}


