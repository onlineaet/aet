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

#ifndef __GCC_MTCS_PTX_EMIT__
#define __GCC_MTCS_PTX_EMIT__

#include "aet/nlib.h"
#include "../mtcsemit.h"
#include "ptx-common.h"

//原型 insn-flags.h
//gen_... 替换成 pgx_gen_...

/*
*(define_insn "@xxxfun_ 格式会生成:
*maybe_gen_xxxfun 函数声明在insn-opinit.h中
*gen_xxxfun函数定义在insn-opinit.h中
*maybe_code_for_xxxfun 函数声明在insn-opinit.h中
*code_for_xxxfun函数定义在insn-opinit.h中
* maybe_gen_xxxfun  maybe_code_for_xxxfun实现由genemit自动生成代码在文件insn-emit.cc中
*/

typedef struct _MtcsPtxEmit MtcsPtxEmit;
struct _MtcsPtxEmit
{
    MtcsEmit parent;
};

MtcsPtxEmit     *mtcs_ptx_emit_new(MtcsMode *mtcsMode);
//原型 nvptx_expand_compare nvptx.h nvptx.cc
rtx mtcs_ptx_emit_expand_compare (MtcsPtxEmit *self,rtx compare);
//原型 nvptx_expand_call nvptx.h nvptx.cc
void mtcs_ptx_emit_expand_call (MtcsPtxEmit *self,rtx retval, rtx address);
//原型 nvptx_expand_oacc_fork nvptx-protos.h nvptx.cc .md文件调用
void mtcs_ptx_emit_expand_oacc_fork (MtcsPtxEmit *self,unsigned mode);
//原型 nvptx_expand_oacc_join nvptx-protos.h nvptx.cc .md文件调用
void mtcs_ptx_emit_expand_oacc_join (MtcsPtxEmit *self,unsigned mode);
//原型 nvptx_gen_shuffle nvptx-protos.h nvptx.cc .md引用
rtx  mtcs_ptx_emit_gen_shuffle (MtcsPtxEmit *self,rtx dst, rtx src, rtx idx, enum ptx_shuffle_kind kind);
//原型 nvptx_mem_maybe_shared_p nvptx-protos.h nvptx.cc
bool mtcs_ptx_emit_mem_maybe_shared_p (MtcsPtxEmit *self,const_rtx x);

//原型 nvptx_gen_shuffle nvptx-protos.h nvptx.cc .md引用
rtx  mtcs_ptx_emit_shfl_xor_sync(MtcsPtxEmit *self,rtx target, rtx src, rtx laneMask, rtx memberMask);
//由ptx-insn-emit.c实现
void mtcs_ptx_emit_set_target(MtcsPtxEmit *self, void *target);

#endif

