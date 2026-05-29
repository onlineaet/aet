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
#include "emit-rtl.h"

#include "mtcsargs.h"
#include "mtcstarget.h"


void  mtcs_args_init(MtcsArgs *self)
{

}

//#define INIT_CUMULATIVE_ARGS(CUM, FNTYPE, LIBNAME, FNDECL, N_NAMED_ARGS) \
 // ((CUM).fntype = (FNTYPE), (CUM).count = 0, (void)0)
//原型 INIT_CUMULATIVE_ARGS
void mtcs_args_init_cumulative_args(MtcsArgs *self,MtcsCumulativeArgs *cum,tree fntype,rtx libname,tree fndecl, int nNamedArgs)
{
    self->init_cumulative_arg(self,cum,fntype,libname,fndecl,nNamedArgs);
}

//原型 PUSH_P (to) expr.cc
bool mtcs_args_is_push_p(MtcsArgs *self,rtx to)
{
    return self->is_push_p(self,to);
}

//创建 MtcsCumulativeArgs
MtcsCumulativeArgs *mtcs_args_create_cumulative_args(MtcsArgs *self)
{
    return self->create_cumulative_args(self);
}

void mtcs_arg_free_cumulative_args(MtcsArgs *self,MtcsCumulativeArgs *args)
{
    self->free_cumulative_args(self,args);
}
