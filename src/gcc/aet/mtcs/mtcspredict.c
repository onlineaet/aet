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
 * base on predict.cc
 */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "cfghooks.h"
#include "tree-pass.h"
#include "ssa.h"
#include "memmodel.h"
#include "emit-rtl.h"
#include "cgraph.h"
#include "coverage.h"
#include "diagnostic-core.h"
#include "gimple-predict.h"
#include "fold-const.h"
#include "calls.h"
#include "cfganal.h"
#include "profile.h"
#include "sreal.h"
#include "cfgloop.h"
#include "gimple-iterator.h"
#include "tree-cfg.h"
#include "tree-ssa-loop-niter.h"
#include "tree-ssa-loop.h"
#include "tree-scalar-evolution.h"
#include "ipa-utils.h"
#include "gimple-pretty-print.h"
#include "selftest.h"
#include "cfgrtl.h"
#include "stringpool.h"
#include "attribs.h"

#include "mtcspredict.h"
#include "mtcstarget.h"

static void mtcsPredictInit(MtcsPredict *self)
{

}

/* Return TRUE if the current function is optimized for size.  */
//原型 optimize_insn_for_size_p predict.h predict.cc
optimize_size_level mtcs_predict_optimize_insn_for_size_p (MtcsPredict *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   enum optimize_size_level ret = optimize_function_for_size_p (cfun);
   if (ret < OPTIMIZE_SIZE_BALANCED && !mtcsRtlData/*!crtl*/->maybe_hot_insn_p)
      ret = OPTIMIZE_SIZE_BALANCED;
   return ret;
}

/* Return TRUE if the current function is optimized for speed.  */
//原型 optimize_insn_for_speed_p predict.h predict.cc
bool mtcs_predict_optimize_insn_for_speed_p (MtcsPredict *self)
{
   return !mtcs_predict_optimize_insn_for_size_p/*!optimize_insn_for_size_p*/(self);
}

MtcsPredict *mtcs_predict_new(MtcsMode *mtcsMode)
{
   MtcsPredict *self = n_slice_alloc0 (sizeof(MtcsPredict));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcsPredictInit(self);
   return self;
}
