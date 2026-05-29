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
 * base on internal-fn.cc
 */


#include "config.h"
#define INCLUDE_MEMORY
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "predict.h"
#include "stringpool.h"
#include "tree-vrp.h"
#include "tree-ssanames.h"
#include "expmed.h"
#include "memmodel.h"
#include "optabs.h"
#include "emit-rtl.h"
#include "diagnostic-core.h"
#include "fold-const.h"
#include "internal-fn.h"
#include "stor-layout.h"
#include "dojump.h"
#include "expr.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "ubsan.h"
#include "recog.h"
#include "builtins.h"
#include "optabs-tree.h"
#include "gimple-ssa.h"
#include "tree-phinodes.h"
#include "ssa-iterators.h"
#include "explow.h"
#include "rtl-iter.h"
#include "gimple-range.h"
#include "fold-const-call.h"
#include "tree-ssa-live.h"
#include "tree-outof-ssa.h"
#include "gcc-urlifier.h"

/* For lang_hooks.types.type_for_mode.  */
#include "langhooks.h"

#include "mtcsptxinternalfn.h"
#include "../mtcsprintrtl.h"
#include "../mtcsoutput.h"
#include "../mtcstarget.h"

#include "../../aetprinttree.h"


//rtx q = operands[0];
//rtx a = operands[1];
//rtx b = operands[2];
//rtx r = operands[3];
//
//rtx tmp = gen_reg_rtx (<MODE>mode);
//
//emit_insn (MTCS_GEN_FCN (PTX_CODE_FOR_div<mode>3) (q, a, b));
//emit_insn (MTCS_GEN_FCN (PTX_CODE_FOR_mul<mode>3) (tmp, q, b));
//emit_insn (MTCS_GEN_FCN (PTX_CODE_FOR_sub<mode>3) (r, a, tmp));
static void expandDIVMOD_cb(MtcsInternalFn *mtcsInternalFn,rtx q,rtx a,rtx b,rtx r,rtx tmp,machine_mode mode,int unsignedp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsInternalFn);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);

   enum insn_code icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,unsignedp?udiv_optab:sdiv_optab, mode);
   if(icode!=CODE_FOR_nothing){
      insn_gen_fn m_gen_fun = MTCS_GEN_FCN (icode);
      /* q = a / b */
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,m_gen_fun(q,a,b));
   }

   icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,smul_optab, mode);
   if(icode!=CODE_FOR_nothing){
      insn_gen_fn m_gen_fun = MTCS_GEN_FCN (icode);
      /* tmp = q * b */
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,m_gen_fun(tmp,q,b));
   }

   icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,sub_optab, mode);
   if(icode!=CODE_FOR_nothing){
      insn_gen_fn m_gen_fun = MTCS_GEN_FCN (icode);
      /* r = a - tmp */
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,m_gen_fun(r,a,tmp));
   }
}



static void mtcsPtxInternalFnInit(MtcsPtxInternalFn *self)
{
   MtcsInternalFn *mtcsInternalFn=(MtcsInternalFn *)self;
   mtcsInternalFn->expandDIVMOD=expandDIVMOD_cb;
}


MtcsPtxInternalFn *mtcs_ptx_internal_fn_new(MtcsMode *mtcsMode)
{
   MtcsPtxInternalFn *self = n_slice_alloc0 (sizeof(MtcsPtxInternalFn));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcsPtxInternalFnInit(self);
   return self;
}
