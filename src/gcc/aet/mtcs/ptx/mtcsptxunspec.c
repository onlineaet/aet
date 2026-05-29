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
#define IN_TARGET_CODE 1 //不加这句在machmode.h中的GET_MODE_SIZE编译到poly_uint16(poly_int) 因为poly_int没有重载>号，所以编译报错
//insn-modes.h由nvptx生成，但i386生成的类型全覆盖nvptx的insn-modes.h,不需要平台的？？？
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
#include "machmode.h"
#include "poly-int-types.h"
#include "opts.h"

#include "aet/aetprinttree.h"
#include "../mtcstool.h"
#include "mtcsptxunspec.h"
#include "ptx-common.h"
#include "gen/ptx-insn-unspec.h"

static void mtcsUnspecInit(MtcsPtxUnspec *self)
{
   MtcsUnspec *mtcsUnspec=(MtcsUnspec *)self;
   mtcs_unspec_set_unspec_string(mtcsUnspec,ptx_unspec_strings,PTX_NUM_UNSPEC_VALUES);
   mtcs_unspec_set_unspecv_string(mtcsUnspec,ptx_unspecv_strings,PTX_NUM_UNSPECV_VALUES);
}



MtcsPtxUnspec *mtcs_ptx_unspec_new(MtcsMode *mtcsMode)
{
   MtcsPtxUnspec *self = n_slice_alloc0 (sizeof(MtcsPtxUnspec));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_unspec_init((MtcsUnspec *)self);
   mtcsUnspecInit(self);
   return self;
}


