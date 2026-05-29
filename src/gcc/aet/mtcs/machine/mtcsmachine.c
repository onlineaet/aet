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
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "predict.h"
#include "memmodel.h"
#include "tm_p.h"
#include "ssa.h"
#include "optabs.h"
#include "expmed.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "alias.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "attribs.h"
#include "varasm.h"
#include "except.h"
#include "insn-attr.h"
#include "dojump.h"
#include "explow.h"
#include "calls.h"
#include "stmt.h"
/* Include expr.h after insn-config.h so we get HAVE_conditional_move.  */
#include "expr.h"
#include "optabs-tree.h"
#include "libfuncs.h"
#include "reload.h"
#include "langhooks.h"
#include "common/common-target.h"
#include "tree-dfa.h"
#include "tree-ssa-live.h"
#include "tree-outof-ssa.h"
#include "tree-ssa-address.h"
#include "builtins.h"
#include "ccmp.h"
#include "gimple-iterator.h"
#include "gimple-fold.h"
#include "rtx-vector-builder.h"
#include "tree-pretty-print.h"
#include "flags.h"
#include "tree-cfg.h"
#include "cfganal.h"

#include "mtcsmachine.h"
#include "../mtcstarget.h"
#include "aet/aetprinttree.h"
#include "aet/aetprintgimple.h"


static void mtcsMachineInit (MtcsMachine *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   self->memTag = target_mem_tag_new(mtcsMode);
   self->c = target_c_new(mtcsMode);
   self->emutls=target_emutls_new(mtcsMode);
}


void  mtcs_machine_set_vectorize(MtcsMachine *self,TargetVectorize *vectorize)
{
   self->vectorize = vectorize;
}

void   mtcs_machine_set_addr_space(MtcsMachine *self,TargetAddrSpace *addrSpace)
{
   self->addrSpace = addrSpace;
}

void  mtcs_machine_set_option(MtcsMachine *self,TargetOption *option)
{
   self->option = option;
}

void  mtcs_machine_set_common(MtcsMachine *self,TargetCommon *common)
{
   self->common = common;

}

void  mtcs_machine_set_asm_out(MtcsMachine *self,TargetAsmOut *asmOut)
{
   self->asmOut = asmOut;
}

void mtcs_machine_set_calls(MtcsMachine *self,TargetCalls *calls)
{
   self->calls = calls;
}

void  mtcs_machine_set_tmrtx(MtcsMachine *self,TargetRtx *tmrtx)
{
   self->tmrtx = tmrtx;
}

MtcsMachine *mtcs_machine_new(MtcsMode *mtcsMode)
{
   MtcsMachine *self = n_slice_alloc0 (sizeof(MtcsMachine));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcsMachineInit(self);
   return self;
}
