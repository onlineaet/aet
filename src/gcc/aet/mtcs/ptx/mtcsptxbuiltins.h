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

#ifndef __GCC_MTCS_PTX_BUILTINS__
#define __GCC_MTCS_PTX_BUILTINS__

#include "aet/nlib.h"
#include "../mtcsbuiltins.h"


typedef struct _MtcsPtxBuiltins MtcsPtxBuiltins;
struct _MtcsPtxBuiltins
{
    MtcsBuiltins parent;
    tree nvptx_builtin_decls[30];//最大是 NVPTX_BUILTIN_MAX
};

MtcsPtxBuiltins *mtcs_ptx_builtins_new(MtcsMode *mtcsMode);
//原型 targetm.expand_builtin (exp, target, subtarget, mode, ignore) #define TARGET_EXPAND_BUILTIN nvptx_expand_builtin
rtx mtcs_ptx_builtins_expand_builtin (MtcsPtxBuiltins *self,tree exp, rtx target, rtx subtarget,
        machine_mode mode, int ignore);
//原型 targetm.init_builtins ();#define TARGET_INIT_BUILTINS nvptx_init_builtins
void mtcs_ptx_builtins_init_builtins(MtcsPtxBuiltins *self);
char *mtcs_ptx_builtins_get_asm_dim(MtcsPtxBuiltins *self,int pos);
void  mtcs_ptx_builtins_expand_local_shared  (MtcsPtxBuiltins *self,int id,int reserver);

#endif
