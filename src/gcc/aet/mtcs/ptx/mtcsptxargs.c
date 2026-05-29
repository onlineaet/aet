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

#include "mtcsptxargs.h"
#include "ptx-common.h"




static void initCumulativeArg_cb(MtcsArgs *mtcsArgs,MtcsCumulativeArgs *cum,tree fntype,rtx libname, tree fndecl,int nNamedArgs);
//原型 PUSH_P(to) expr.cc
static bool isPushP_cb(MtcsArgs *self,rtx to);
static MtcsCumulativeArgs *createCumulativeArgs_cb(MtcsArgs *self);
static void freeCumulativeArgs_cb(MtcsArgs *self ,MtcsCumulativeArgs *args);

static void mtcsPtxArgsInit(MtcsPtxArgs *self)
{
    MtcsArgs *mtcsArgs=(MtcsArgs *)self;
    mtcsArgs->init_cumulative_arg=initCumulativeArg_cb;
    //原型 PUSH_P(to) expr.cc
    mtcsArgs->is_push_p=isPushP_cb;
    mtcsArgs->create_cumulative_args=createCumulativeArgs_cb;
    mtcsArgs->free_cumulative_args=freeCumulativeArgs_cb;
}

//原型
//#define INIT_CUMULATIVE_ARGS(CUM, FNTYPE, LIBNAME, FNDECL, N_NAMED_ARGS) \
//  ((CUM).fntype = (FNTYPE), (CUM).count = 0, (void)0)
//nvptx.h
static void initCumulativeArg_cb(MtcsArgs *mtcsArgs,MtcsCumulativeArgs *cum,tree fntype,rtx libname,tree fndecl, int nNamedArgs)
{
    MtcsPtxCumulativeArgs *ptxCum=(MtcsPtxCumulativeArgs *)cum;
    ptxCum->fntype=fntype;
    ptxCum->count=0;
}

//原型 PUSH_P(to) expr.cc
static bool isPushP_cb(MtcsArgs *self,rtx to)
{
    return false;
}

static MtcsCumulativeArgs *createCumulativeArgs_cb(MtcsArgs *self)
{
    MtcsPtxCumulativeArgs *args = n_slice_alloc0 (sizeof(MtcsPtxCumulativeArgs));
    return (MtcsCumulativeArgs *)args;
}

static void freeCumulativeArgs_cb(MtcsArgs *self ,MtcsCumulativeArgs *args)
{
    n_slice_free(MtcsPtxCumulativeArgs,(MtcsPtxCumulativeArgs*)args);
}

MtcsPtxArgs *mtcs_ptx_args_new()
{
     MtcsPtxArgs *self = n_slice_alloc0 (sizeof(MtcsArgs));
     mtcs_args_init((MtcsArgs *)self);
     mtcsPtxArgsInit(self);
     return self;
}
