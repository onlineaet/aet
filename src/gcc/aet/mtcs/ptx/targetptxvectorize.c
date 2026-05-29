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

#include "targetptxvectorize.h"
#include "aet/aetprinttree.h"
#include "gen/ptx-insn-modes.h"


//原型 targetm.vectorize.preferred_simd_mode (smode); #define TARGET_VECTORIZE_PREFERRED_SIMD_MODE default_preferred_simd_mode
static machine_mode preferredSimdMode_cb (TargetVectorize *targetVectorize,scalar_mode mode)
{
   n_debug("-----nvptx.cc -----70-- TARGET_VECTORIZE_PREFERRED_SIMD_MODE machine_mode nvptx_preferred_simd_mode (scalar_mode mode)\n");
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(targetVectorize);
   switch (mode){
      case PTX_DImode:
         return (machine_mode)PTX_V2DImode;
      case PTX_SImode:
         return (machine_mode)PTX_V2SImode;

      default:
         return word_mode/*!default_preferred_simd_mode (mode)*/;
   }
}

static void targetPtxVectorizeInit(TargetPtxVectorize *self)
{
   TargetVectorize *targetVectorize=(TargetVectorize *)self;
   //原型 targetm.vectorize.preferred_simd_mode (smode); #define TARGET_VECTORIZE_PREFERRED_SIMD_MODE default_preferred_simd_mode
   targetVectorize->preferred_simd_mode = preferredSimdMode_cb;
}

TargetPtxVectorize *target_ptx_vectorize_new(MtcsMode *mtcsMode)
{
   TargetPtxVectorize *self = n_slice_alloc0 (sizeof(TargetPtxVectorize));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   target_vectorize_init((TargetVectorize*)self);
   targetPtxVectorizeInit(self);
   return self;
}

