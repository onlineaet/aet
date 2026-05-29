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

#ifndef __GCC_MTCS_PTX_OUTPUT__
#define __GCC_MTCS_PTX_OUTPUT__

#include "aet/nlib.h"
#include "../mtcsoutput.h"

/*原型 nvptx-proto.h
extern void nvptx_expand_oacc_fork (unsigned);
extern void nvptx_expand_oacc_join (unsigned);
extern void nvptx_expand_call (rtx, rtx);
extern rtx nvptx_gen_shuffle (rtx, rtx, rtx, nvptx_shuffle_kind);
extern rtx nvptx_expand_compare (rtx);
extern const char *nvptx_ptx_type_from_mode (machine_mode, bool);
extern const char *nvptx_output_mov_insn (rtx, rtx);
extern const char *nvptx_output_call_insn (rtx_insn *, rtx, rtx);
extern const char *nvptx_output_fake_ptx_alloca (void);
extern const char *nvptx_output_return (void);
extern const char *nvptx_output_set_softstack (unsigned);
extern const char *nvptx_output_simt_enter (rtx, rtx, rtx);
extern const char *nvptx_output_simt_exit (rtx);
extern const char *nvptx_output_red_partition (rtx, rtx);
extern const char *nvptx_output_atomic_insn (const char *, rtx *, int, int);
extern bool nvptx_mem_local_p (rtx);
extern bool nvptx_mem_maybe_shared_p (const_rtx);//该方法由mtcsptxemit.h 声明 mtcsptxemit.c实现
*/
extern const struct insn_data_d ptx_insn_data[];


typedef struct _MtcsPtxOutput MtcsPtxOutput;
struct _MtcsPtxOutput
{
    MtcsOutput parent;
    NString *debugFileStr;
};

MtcsPtxOutput     *mtcs_ptx_output_new(MtcsMode *mtcsMode);
//原型 nvptx_output_mov_insn nvptx-protos.h nvptx.cc
const char *mtcs_ptx_output_mov_insn (MtcsPtxOutput *self,rtx dst, rtx src);
//原型 nvptx_output_call_insn nvptx-protos.h nvptx.cc
const char *mtcs_ptx_output_call_insn (MtcsPtxOutput *self,rtx_insn *insn, rtx result, rtx callee);
//原型 nvptx_output_fake_ptx_alloca nvptx-protos.h nvptx.cc gcc15 新加的
const char *mtcs_ptx_output_fake_ptx_alloca (MtcsPtxOutput *self);
//原型 nvptx_output_return nvptx-protos.h nvptx.cc
const char *mtcs_ptx_output_return (MtcsPtxOutput *self);
//原型 nvptx_output_set_softstack nvptx-protos.h nvptx.cc
const char *mtcs_ptx_output_set_softstack (MtcsPtxOutput *self,unsigned src_regno);
//原型 nvptx_output_simt_enter nvptx-protos.h nvptx.cc
const char *mtcs_ptx_output_simt_enter (MtcsPtxOutput *self,rtx dest, rtx size, rtx align);
//原型 nvptx_output_simt_exit nvptx-protos.h nvptx.cc
const char *mtcs_ptx_output_simt_exit (MtcsPtxOutput *self,rtx src);
//原型 nvptx_output_red_partition nvptx-protos.h nvptx.cc
const char *mtcs_ptx_output_red_partition (MtcsPtxOutput *self,rtx dst, rtx offset);
//原型 nvptx_output_atomic_insn nvptx-protos.h nvptx.cc
const char *mtcs_ptx_output_atomic_insn (MtcsPtxOutput *self,const char *asm_template, rtx *operands, int mem_pos,int memmodel_pos);
//原型 nvptx_mem_local_p nvptx-protos.h nvptx.cc
bool        mtcs_ptx_output_mem_local_p (MtcsPtxOutput *self,rtx mem);

/* Output a register, subreg, or register pair (with optional
   enclosing braces).  */
void mtcs_ptx_output_output_reg (MtcsPtxOutput *self,unsigned regno, mtcs_mode inner_mode,int subreg_offset = -1);
//原型 static void nvptx_print_operand (FILE file, rtx x, int code) nvptx.cc
void mtcs_ptx_output_print_operand (MtcsPtxOutput *self, rtx x, int code);
//原型 nvptx_print_address_operand (FILE *file, rtx x, machine_mode) nvptx.cc
void mtcs_ptx_output_print_address_operand (MtcsPtxOutput *self,rtx x, machine_mode);

//由ptx-insn-output.c实现
void mtcs_ptx_output_set_target(MtcsPtxOutput *self, void *target);
//解决 bug 082
char *mtcs_ptx_output_get_debug_file(MtcsPtxOutput *self);

//由ptx-insn-output.c实现
const char *ptx_get_insn_name(int mode);


#endif

