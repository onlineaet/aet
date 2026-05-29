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
#include "predict.h"
#include "memmodel.h"
#include "tm_p.h"
#include "stringpool.h"
#include "regs.h"
#include "emit-rtl.h"
#include "cgraph.h"
#include "diagnostic-core.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "varasm.h"
#include "version.h"
#include "flags.h"
#include "stmt.h"
#include "expr.h"
#include "expmed.h"
#include "optabs.h"
#include "output.h"
#include "langhooks.h"
#include "debug.h"
#include "common/common-target.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "rtl-iter.h"
#include "file-prefix-map.h" /* remap_debug_filename()  */
#include "alloc-pool.h"
#include "toplev.h"
#include "opts.h"
#include "asan.h"
#include "recog.h"
#include "dwarf2out.h"
#include "dwarf2.h"
#include "dwarf2asm.h"
#include "except.h"
#include "insn-opinit.h" //主机定义的

#include "ptx-common.h"
#include "gen/ptx-insn-flags.h"
#include "gen/ptx-insn-opinit.h"
#include "mtcsptxmode.h"
#include "mtcsptxopinit.h"
#include "mtcsptxcodes.h"

//原型   raw_optab_handler (scode); optabs-query.h  各个平台的insn-opinit.c实现
static enum insn_code rawOptabHandler_cb (MtcsOpinit *mtcsOpinit,unsigned scode)
{
  MtcsPtxOpinit *self=(MtcsPtxOpinit *)mtcsOpinit;
  int i = ptx_lookup_optab/*!lookup_handler*/(scode);
  n_debug("mtcsptxopinit.c rawOptabHandler_cb scode:%d i:%d %d\n",scode,i,mtcsOpinit->this_fn_optabs->pat_enable[i]);
  return (i >= 0 && mtcsOpinit->this_fn_optabs->pat_enable[i] ? ptx_get_insn_code(i) : CODE_FOR_nothing);
}

//原型 extern bool swap_optab_enable (optab, machine_mode, bool); insn-opinit.h insn-opinit.cc
static bool swapOptabEnable_cb(MtcsOpinit *mtcsOpinit,optab op, machine_mode m, bool set)
{
   unsigned scode = (op << 20) | m;
   int i = ptx_lookup_optab/*!lookup_handler*/(scode);
   if (i >= 0){
      bool ret = mtcsOpinit->this_fn_optabs->pat_enable[i];
      mtcsOpinit->this_fn_optabs->pat_enable[i] = set;
      return ret;
   }else{
      gcc_assert (!set);
      return false;
   }
}

//原型 extern void init_all_optabs (struct target_optabs *); insn-opinit.h insn-opinit.cc
static void initAllOptabs_cb(MtcsOpinit *mtcsOpinit,struct target_optabs *optabs)
{
    ptx_init_all_optabs(optabs);
}

//原型 extern bool partial_vectors_supported_p (); insn-opinit.h insn-opinit.cc
static bool partialVectorsSupportedP_cb(MtcsOpinit *mtcsOpinit)
{
   return ptx_partial_vectors_supported_p();
}


//#define PTX_NUM_OPTABS          453
//#define PTX_NUM_CONVLIB_OPTABS  17
//#define PTX_NUM_NORMLIB_OPTABS  80
//#define PTX_NUM_OPTAB_PATTERNS  243

static void mtcsPtxOpinitInit(MtcsPtxOpinit *self)
{
    MtcsOpinit *mtcsOpinit=(MtcsOpinit *)self;
    //原型   raw_optab_handler (scode); optabs-query.h  各个平台的insn-opinit.c实现
    mtcsOpinit->raw_optab_handler=rawOptabHandler_cb;
    //原型 extern bool swap_optab_enable (optab, machine_mode, bool); insn-opinit.h insn-opinit.cc
    mtcsOpinit->swap_optab_enable=swapOptabEnable_cb;
    //原型 extern void init_all_optabs (struct target_optabs *); insn-opinit.h insn-opinit.cc
    mtcsOpinit->init_all_optabs=initAllOptabs_cb;
    //原型 extern bool partial_vectors_supported_p (); insn-opinit.h insn-opinit.cc
    mtcsOpinit->partial_vectors_supported_p=partialVectorsSupportedP_cb;

    mtcs_opinit_set_number_optabs(mtcsOpinit,PTX_NUM_OPTABS);
    mtcs_opinit_set_number_convlib_optabs(mtcsOpinit,PTX_NUM_CONVLIB_OPTABS);
    mtcs_opinit_set_number_normlib_optabs(mtcsOpinit,PTX_NUM_NORMLIB_OPTABS);
    mtcs_opinit_set_number_optab_patterns(mtcsOpinit,PTX_NUM_OPTAB_PATTERNS);
    ptx_init_all_optabs(&mtcsOpinit->default_target_optabs);
    n_debug("mtcsPtxOpinitInit 00 %d\n",mtcsOpinit->default_target_optabs.pat_enable[229]);
    n_debug("mtcsPtxOpinitInit 11 %d\n",mtcsOpinit->this_fn_optabs->pat_enable[229]);

}

MtcsPtxOpinit *mtcs_ptx_opinit_new(MtcsMode *mtcsMode)
{
     MtcsPtxOpinit *self = n_slice_alloc0 (sizeof(MtcsPtxOpinit));
     mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
     mtcs_opinit_init((MtcsOpinit *)self);
     mtcsPtxOpinitInit(self);
     return self;
}
