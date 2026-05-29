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
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "tree-pretty-print.h" /* for dump_function_header */
#include "varasm.h"
#include "insn-attr.h"
#include "conditions.h"
#include "flags.h"
#include "regs.h"
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
#include "mtcsptxreal.h"
#include "ptx-common.h"


static REAL_VALUE_TYPE floatStoreFlagValue_cb(MtcsReal *mtcsReal,mtcs_mode mode);

static void mtcsPtxRealInit(MtcsPtxReal *self)
{
    MtcsReal *mtcsReal=(MtcsReal *)self;
    mtcsReal->float_store_flag_value=floatStoreFlagValue_cb;
    //原型 STORE_FLAG_VALUE 每个平台不一样 default.h STORE_FLAG_VALUE=1 gcn STORE_FLAG_VALUE=-1;
    mtcs_real_set_store_flag_value(mtcsReal,PTX_STORE_FLAG_VALUE);
}

//原型 #define FLOAT_STORE_FLAG_VALUE(MODE) REAL_VALUE_ATOF("1.0", (MODE)) ptx.h
static REAL_VALUE_TYPE floatStoreFlagValue_cb(MtcsReal *mtcsReal,mtcs_mode mode)
{
    REAL_VALUE_TYPE r=mtcs_real_real_from_string2(mtcsReal,"1.0",mode);
    return r;
}

MtcsPtxReal *mtcs_ptx_real_new(MtcsMode *mtcsMode)
{
    MtcsPtxReal *self = n_slice_alloc0 (sizeof(MtcsPtxReal));
    mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
    mtcs_real_init((MtcsReal *)self);
    mtcsPtxRealInit(self);
    return self;
}


