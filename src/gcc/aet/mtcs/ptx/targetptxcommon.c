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

#include "cfganal.h"
#include "cfgcleanup.h"
#include "alias.h"
#include "rtlhooks-def.h"
#include "tree-pass.h"
#include "dbgcnt.h"
#include "rtlanal.h"

#include "targetptxcommon.h"
#include "aet/aetprinttree.h"
#include "gen/ptx-insn-modes.h"

/* Determine the exception handling mechanism for the target.
*原型 targetm_common.except_unwind_info 和#define TARGET_EXCEPT_UNWIND_INFO default_except_unwind_info
*/
static enum unwind_info_type exceptUnwindInfo_cb(TargetCommon *targetCommon,MtcsOptionsItem *opts ATTRIBUTE_UNUSED)
{
    return UI_NONE;
}

//原型 targetm_common.handle_option (opts, opts_set, decoded, loc);#define TARGET_HANDLE_OPTION default_target_handle_option
static bool handleOption_cb(TargetCommon *targetCommon,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,
        const struct cl_decoded_option *decoded ATTRIBUTE_UNUSED,location_t loc ATTRIBUTE_UNUSED)
{
    return true;
}

static void targetPtxCommonInit(TargetPtxCommon *self)
{
   TargetCommon *targetCommon=(TargetCommon *)self;
   //原型 targetm_common.except_unwind_info 和#define TARGET_EXCEPT_UNWIND_INFO default_except_unwind_info
   targetCommon->except_unwind_info=exceptUnwindInfo_cb;
   //原型 targetm_common.handle_option (opts, opts_set, decoded, loc);#define TARGET_HANDLE_OPTION default_target_handle_option
   targetCommon->handle_option=handleOption_cb;
   targetCommon->have_named_sections=false;//来自gcc/common/config/nvptx/nvptx-common.cc

}

TargetPtxCommon *target_ptx_common_new(MtcsMode *mtcsMode)
{
   TargetPtxCommon *self = n_slice_alloc0 (sizeof(TargetPtxCommon));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   target_common_init((TargetCommon *)self);
   targetPtxCommonInit(self);
   return self;
}

