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

#ifndef __GCC_MTCS_PTX_RECOG__
#define __GCC_MTCS_PTX_RECOG__


#include "../mtcsrecog.h"
#include "aet/nlib.h"

typedef struct _MtcsPtxRecog MtcsPtxRecog;
struct _MtcsPtxRecog
{
    MtcsRecog parent;
};

MtcsPtxRecog *mtcs_ptx_recog_new(MtcsMode *mtcsMode);
//原型 extern void add_clobbers (rtx, int); recog.h 由mtcsgenemit实现
void ptx_add_clobbers (rtx pattern ATTRIBUTE_UNUSED, int insn_code_number);
//原型 extern bool added_clobbers_hard_reg_p (int);recog.h 由mtcsgenemit实现
bool ptx_added_clobbers_hard_reg_p (int insn_code_number);
//原型extern rtx_insn *peephole2_insns (rtx, rtx_insn *, int *); recog.h insn-recog.cc
rtx_insn *ptx_peephole2_insns(rtx x1 ATTRIBUTE_UNUSED, rtx_insn *insn ATTRIBUTE_UNUSED, int *pmatch_len_ ATTRIBUTE_UNUSED);
//原型rtx_insn *split_insns (rtx x1 ATTRIBUTE_UNUSED, rtx_insn *insn ATTRIBUTE_UNUSED) recog.h insn-recog.cc
rtx_insn *ptx_split_insns(rtx x1 ATTRIBUTE_UNUSED, rtx_insn *insn ATTRIBUTE_UNUSED);
//原型int recog (rtx x1 ATTRIBUTE_UNUSED,rtx_insn *insn ATTRIBUTE_UNUSED,int *pnum_clobbers ATTRIBUTE_UNUSED) recog.h insn-recog.cc
int ptx_recog(rtx x1 ATTRIBUTE_UNUSED,rtx_insn *insn ATTRIBUTE_UNUSED,int *pnum_clobbers ATTRIBUTE_UNUSED);
//原型 insn_extract recog.h recog.cc 由mtcsgenextract.c 生成文件 平台-insn-extract.c文件实现。
void ptx_insn_extract_cb(MtcsRecog *self,rtx_insn *insn);

//由ptx-insn-recog.c实现
void mtcs_ptx_recog_set_target(MtcsPtxRecog *self, void *target);

#endif

