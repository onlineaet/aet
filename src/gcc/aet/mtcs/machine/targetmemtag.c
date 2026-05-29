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

#include "targetmemtag.h"
#include "aet/aetprinttree.h"
#include "../mtcstarget.h"

#define HWASAN_SHIFT (mtcs_mode_get_precision(mtcsMode,mtcs_mode_get_Pmode(mtcsMode)) - 8)
//原型 targetm.memtag.add_tag (base, offset,hwasan_current_frame_tag ());#define TARGET_MEMTAG_ADD_TAG default_memtag_add_tag
static rtx addTag_cb(TargetMemTag *self,rtx base, poly_int64 offset, uint8_t tag_offset)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL    *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   /* Need to look into what the most efficient code sequence is.
   This is a code sequence that would be emitted *many* times, so we
   want it as small as possible.

   There are two places where tag overflow is a question:
   - Tagging the shadow stack.
   (both tagging and untagging).
   - Tagging addressable pointers.

   We need to ensure both behaviors are the same (i.e. that the tag that
   ends up in a pointer after "overflowing" the tag bits with a tag addition
   is the same that ends up in the shadow space).

   The aim is that the behavior of tag addition should follow modulo
   wrapping in both instances.

   The libhwasan code doesn't have any path that increments a pointer's tag,
   which means it has no opinion on what happens when a tag increment
   overflows (and hence we can choose our own behavior).  */
   offset += ((uint64_t)tag_offset << HWASAN_SHIFT);
   return mtcs_rtl_plus_constant/*!plus_constant*/ (mtcsRTL,mtcs_mode_get_Pmode(mtcsMode)/*!Pmode*/, base, offset,false);
}

static void targetMemTagInit(TargetMemTag *self)
{
   //原型 targetm.memtag.add_tag (base, offset,hwasan_current_frame_tag ());#define TARGET_MEMTAG_ADD_TAG default_memtag_add_tag
   self->add_tag=addTag_cb;
}

rtx target_mem_tag_add_tag(TargetMemTag *self,rtx base, poly_int64 offset, uint8_t tag_offset)
{
   return self->add_tag(self,base,offset,tag_offset);
}

TargetMemTag *target_mem_tag_new(MtcsMode *mtcsMode)
{
   TargetMemTag *self = n_slice_alloc0 (sizeof(TargetMemTag));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   targetMemTagInit(self);
   return self;
}

