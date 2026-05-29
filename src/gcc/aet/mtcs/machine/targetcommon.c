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

#include "targetcommon.h"
#include "aet/aetprinttree.h"
#include "../mtcstarget.h"


//原型 targetm_common.option_init_struct (opts); #define TARGET_OPTION_INIT_STRUCT hook_void_gcc_optionsp
static void optionInitStruct_cb(TargetCommon *self,MtcsOptionsItem *opts)
{

}

//原型 targetm_common.supports_split_stack (true, opts)#define TARGET_SUPPORTS_SPLIT_STACK hook_bool_bool_gcc_optionsp_false
static bool supportsSplitStack_cb(TargetCommon *self,bool split, MtcsOptionsItem *opts)
{
    return false;
}

void  target_common_init(TargetCommon *self)
{
   //原型 targetm_common.have_named_sections
   self->have_named_sections=true;
   //原型 targetm_common.default_target_flags; #define TARGET_DEFAULT_TARGET_FLAGS 0
   self->default_target_flags=0;
   //原型 targetm_common.unwind_tables_default; #define TARGET_UNWIND_TABLES_DEFAULT false
   self->unwind_tables_default=false;
   //原型 targetm_common.option_optimization_table, #define TARGET_OPTION_OPTIMIZATION_TABLE empty_optimization_table
   self->empty_optimization_table=empty_optimization_table;
   //原型 targetm_common.option_init_struct (opts); #define TARGET_OPTION_INIT_STRUCT hook_void_gcc_optionsp
   self->option_init_struct=optionInitStruct_cb;
   //原型 targetm_common.supports_split_stack (true, opts)#define TARGET_SUPPORTS_SPLIT_STACK hook_bool_bool_gcc_optionsp_false
   self->supports_split_stack=supportsSplitStack_cb;
}

//原型 targetm_common.option_init_struct (opts); #define TARGET_OPTION_INIT_STRUCT hook_void_gcc_optionsp
void target_common_option_init_struct(TargetCommon *self,MtcsOptionsItem *opts)
{
   self->option_init_struct(self,opts);
}

//原型 targetm_common.supports_split_stack (true, opts)#define TARGET_SUPPORTS_SPLIT_STACK hook_bool_bool_gcc_optionsp_false
bool target_common_supports_split_stack(TargetCommon *self,bool split, MtcsOptionsItem *opts)
{
   return self->supports_split_stack(self,split,opts);
}

//原型 targetm_common.except_unwind_info 和#define TARGET_EXCEPT_UNWIND_INFO default_except_unwind_info
enum unwind_info_type target_common_except_unwind_info(TargetCommon *self,MtcsOptionsItem *opts ATTRIBUTE_UNUSED)
{
   return self->except_unwind_info(self,opts);
}

//原型 targetm_common.handle_option (opts, opts_set, decoded, loc);#define TARGET_HANDLE_OPTION default_target_handle_option
bool target_common_handle_option(TargetCommon *self,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,
        const struct cl_decoded_option *decoded ATTRIBUTE_UNUSED,location_t loc ATTRIBUTE_UNUSED)
{
   return self->handle_option(self,opts,opts_set,decoded,loc);
}


