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
#include "opts.h"

#include "../mtcstarget.h"
#include "targetptxoption.h"
#include "aet/aetprinttree.h"
#include "gen/ptx-insn-modes.h"
#include "gen/ptx-optionsitem.h"
#include "ptx-common.h"
#include "ptxtool.h"
#include "mtcsptx.h"


struct declared_libfunc_hasher : ggc_cache_ptr_hash<rtx_def>
{
  static hashval_t hash (rtx x) { return htab_hash_pointer (x); }
  static bool equal (rtx a, rtx b) { return a == b; }
};


static GTY((cache))  hash_table<declared_libfunc_hasher> *declared_libfuncs_htab;


static void handle_ptx_version_option (TargetPtxOption *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   int     isa=mtcs_target_get_isa(mtcsTarget);
   int     version=mtcs_target_get_version(mtcsTarget);
   nboolean valid=ptx_tool_valid_isa_version ((PtxIsa)isa,(PtxVersion)version);
   if(!valid){
      PtxVersion first =ptx_tool_get_first_version_supporting_sm ((PtxIsa)isa);
      char *isaStr=ptx_tool_sm_version_to_string ((PtxIsa)isa);
      char *versionStr=ptx_tool_version_to_string ((PtxVersion)version);
      char *firstStr=ptx_tool_version_to_string ((PtxVersion)first);
      error("为了支持架构 sm_%s，PTX 版本至少需要%s，当前设置的是:%s",isaStr,firstStr,versionStr);
   }
}


//原型 targetm.target_option.override (); #define TARGET_OPTION_OVERRIDE nvptx_option_override
//代码来自nvptx.cc  nvptx_option_override
static void override_cb(TargetOption *targetOption)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(targetOption);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsPtx *mtcsPtx=(MtcsPtx *)mtcsTarget;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *opts=mtcsOptions->global_options;
   MtcsOptionsItem *opts_set=mtcsOptions->global_options_set;
   PtxOptionsItem *ptx_opts_set=(PtxOptionsItem *)mtcsOptions->global_options_set;

   //不再需要 init_machine_status = nvptx_init_machine_status; //init_machine_status在function.h中声明
   n_debug("mtcsptx.c  -----nvptx.cc -----1-- TARGET_OPTION_OVERRIDE nvptx_option_override (void) ptx_isa_option:%d\n",
               ptx_opts_set->x_ptx_isa_option);
   /* Via nvptx 'OPTION_DEFAULT_SPECS', '-misa' always appears on the command
   line; but handle the case that the compiler is not run via the driver.  */
   // if (!ptx_opts_set->x_ptx_isa_option/*! OPTION_SET_P (ptx_isa_option)*/)
   //  fatal_error (UNKNOWN_LOCATION, "%<-march=%> must be specified");
   //    -----nvptx.cc -----1-- TARGET_OPTION_OVERRIDE nvptx_option_override (void)
   //    -----nvptx.cc -----1aa-- flag_toplevel_reorder:1 flag_no_common:1
   //    -----nvptx.cc -----1bb-- patch_area_size:0 patch_area_entry:0 nvptx_optimize:-1
   //    -----nvptx.cc -----1cc-- optimize:3 nvptx_optimize:1
   handle_ptx_version_option((TargetPtxOption *)targetOption);

   /* Set toplevel_reorder, unless explicitly disabled.  We need
   reordering so that we emit necessary assembler decls of
   undeclared variables. */

   n_debug("mtcsptx.c  -----nvptx.cc -----1aabefore-- flag_toplevel_reorder:%d flag_no_common:%d\n",
            opts->x_flag_toplevel_reorder,opts->x_flag_no_common);

   if (!opts_set->x_flag_toplevel_reorder/*!OPTION_SET_P (flag_toplevel_reorder)*/)
      opts->x_flag_toplevel_reorder = 1;

   opts->x_debug_nonbind_markers_p = 0;

   /* Set flag_no_common, unless explicitly disabled.  We fake common
   using .weak, and that's not entirely accurate, so avoid it
   unless forced.  */
   if (!opts_set->x_flag_no_common/*!OPTION_SET_P (flag_no_common)*/)
      opts->x_flag_no_common = 1;
   n_debug("mtcsptx.c -----nvptx.cc -----1aa-- flag_toplevel_reorder:%d flag_no_common:%d\n",
               opts->x_flag_toplevel_reorder, opts->x_flag_no_common);

   /* The patch area requires nops, which we don't have.  */
   HOST_WIDE_INT patch_area_size, patch_area_entry;
   parse_and_check_patch_area (opts->x_flag_patchable_function_entry, false,&patch_area_size, &patch_area_entry);
   if (patch_area_size > 0)
      sorry ("not generating patch area, nops not supported");
   n_debug("mtcsptx.c -----nvptx.cc -----1bb-- patch_area_size:%d patch_area_entry:%d \n",patch_area_size,patch_area_entry);
   n_debug("mtcsptx.c -----nvptx.cc -----1cc-- flag_var_tracking:%d\n",opts->x_flag_var_tracking);

   /* Assumes that it will see only hard registers.  */
   opts->x_flag_var_tracking = 0;
   if (mtcsPtx->nvptxOptimize < 0)
      mtcsPtx->nvptxOptimize = opts->x_optimize > 0;
   n_debug("mtcsptx.c -----nvptx.cc -----1cc-- optimize:%d nvptx_optimize:%d\n",opts->x_optimize,mtcsPtx->nvptxOptimize);
   declared_libfuncs_htab = hash_table<declared_libfunc_hasher>::create_ggc (17);
   //     oacc_bcast_sym = gen_rtx_SYMBOL_REF (Pmode, "__oacc_bcast");
   //     SET_SYMBOL_DATA_AREA (oacc_bcast_sym, DATA_AREA_SHARED);
   //     oacc_bcast_align = GET_MODE_ALIGNMENT (SImode) / BITS_PER_UNIT;
   //     oacc_bcast_partition = 0;
   nuint pmode=mtcs_mode_get_Pmode(mtcsMode);
   mtcsPtx->worker_red_sym = gen_rtx_SYMBOL_REF ((machine_mode)pmode, "__worker_red");
   PTX_SET_SYMBOL_DATA_AREA (mtcsPtx->worker_red_sym, PTX_DATA_AREA_SHARED);
   mtcsPtx->worker_red_align = mtcs_mode_get_alignment (mtcsMode,mtcsMode->modes.M_SImode) / BITS_PER_UNIT;

   mtcsPtx->vector_red_sym = gen_rtx_SYMBOL_REF ((machine_mode)pmode, "__vector_red");
   PTX_SET_SYMBOL_DATA_AREA (mtcsPtx->vector_red_sym, PTX_DATA_AREA_SHARED);
   mtcsPtx->vector_red_align = mtcs_mode_get_alignment (mtcsMode,mtcsMode->modes.M_SImode) / BITS_PER_UNIT;
   mtcsPtx->vector_red_partition = 0;

   mtcsPtx->gang_private_shared_sym = gen_rtx_SYMBOL_REF ((machine_mode)pmode, "__gang_private_shared");
   PTX_SET_SYMBOL_DATA_AREA (mtcsPtx->gang_private_shared_sym, PTX_DATA_AREA_SHARED);
   mtcsPtx->gang_private_shared_align = mtcs_mode_get_alignment (mtcsMode,mtcsMode->modes.M_SImode) / BITS_PER_UNIT;
}

static void targetPtxOptionInit(TargetPtxOption *self)
{
   TargetOption *targetOption =(TargetOption *)self;
   //原型 targetm.target_option.override (); #define TARGET_OPTION_OVERRIDE nvptx_option_override
   targetOption->override=override_cb;
}

TargetPtxOption *target_ptx_option_new(MtcsMode *mtcsMode)
{
   TargetPtxOption *self = n_slice_alloc0 (sizeof(TargetPtxOption));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   target_option_init((TargetOption *)self);
   targetPtxOptionInit(self);
   return self;
}

