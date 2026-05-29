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

#ifndef __GCC_MTCS_PREDS__
#define __GCC_MTCS_PREDS__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcsmicro.h"


typedef struct _MtcsPreds MtcsPreds;
//原型 insn_operand_predicate_fn recog.h
//insn_operand_predicate_fn 被insn_operand_data引用 只有两个参数 rtx,machine_mode
//insn_data_d中的insn_operand_data被赋值为mtcs_preds_register_operand、mtcs_preds_push_operand等
//所以在调用insn_operand_predicate_fn时强转为mtcs_insn_operand_predicate_fn
//insn_operand_data赋值在xxx-insn-output.c中 代码:static const struct insn_operand_data operand_data[] = ...
typedef bool (*mtcs_insn_operand_predicate_fn) (MtcsPreds *self,rtx x, machine_mode mode);

struct _MtcsPreds
{
    MtcsComponent parent;
    //原型 test_register_filters tm-preds.h ira-color.cc引用
    bool (*test_register_filters)(MtcsPreds *self,unsigned int mask, unsigned int regno);
    //原型 lookup_constraint tm-preds.h
    int (*lookup_constraint)(MtcsPreds *self,unsigned char *p);
    //原型  constraint_satisfied_p tm-preds.h
    bool  (*constraint_satisfied_p) (MtcsPreds *self,rtx x, int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
    //原型  insn_extra_register_constraint tm-preds.h
    bool (*insn_extra_register_constraint)(MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
    //原型  insn_extra_memory_constraint tm-preds.h
    bool (*insn_extra_memory_constraint)(MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
    //原型  insn_extra_special_memory_constraint tm-preds.h
    bool (*insn_extra_special_memory_constraint)(MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
    //原型  insn_extra_relaxed_memory_constraint tm-preds.h
    bool (*insn_extra_relaxed_memory_constraint) (MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
    //原型  insn_extra_address_constraint tm-preds.h
    bool (*insn_extra_address_constraint)(MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
    //原型  insn_extra_constraint_allows_reg_mem tm-preds.h
    void (*insn_extra_constraint_allows_reg_mem)(MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/,
                       bool *allows_reg, bool *allows_mem);
    //原型 insn_constraint_len tm-preds.h
    int (*insn_constraint_len)(MtcsPreds *self,char fc, const char *str ATTRIBUTE_UNUSED);
    //原型 get_register_filter_id tm-preds.h
    int (*get_register_filter_id)(MtcsPreds *self,int constraint_num);
    //原型 get_register_filter tm-preds.h
    const HardRegSet *(*get_register_filter) (MtcsPreds *self, int constraint_num);
    //原型 get_constraint_type tm-preds.h
    int  (*get_constraint_type)(MtcsPreds *self,int constraint_num);
    mtcs_reg_class (*reg_class_for_constraint)(MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
    //原型 insn_const_int_ok_for_constraint (HOST_WIDE_INT, enum constraint_num); tm-preds.h
    bool (*insn_const_int_ok_for_constraint)(MtcsPreds *self,HOST_WIDE_INT ival,
            int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
    //原型 extern bool aligned_register_operand (rtx, machine_mode); tm-preds.h insn-preds.cc 来自common.md
    bool (*aligned_register_operand)(MtcsPreds *self,rtx op, machine_mode mode);



};
void  mtcs_preds_init (MtcsPreds *self);

//原型 general_operand build/tm-preds.h recog.cc实现
bool mtcs_preds_general_operand (MtcsPreds *self,rtx op, mtcs_mode mode);
//原型 register_operand tm-preds.h recog.cc
bool mtcs_preds_register_operand (MtcsPreds *self,rtx op, mtcs_mode mode);
//原型 push_operand recog.cc build/tm-preds.h
bool mtcs_preds_push_operand (MtcsPreds *self,rtx op, mtcs_mode mode);
//原型 address_operand tm-preds.h recog.cc
bool mtcs_preds_address_operand (MtcsPreds *self,rtx op, machine_mode mode);
//原型 pmode_register_operand tm-preds.h recog.cc
bool mtcs_preds_pmode_register_operand (MtcsPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED);
//原型 scratch_operand tm-preds.h recog.cc
bool mtcs_preds_scratch_operand (MtcsPreds *self,rtx op, machine_mode mode);
//原型 immediate_operand tm-preds.h recog.cc
bool mtcs_preds_immediate_operand (MtcsPreds *self,rtx op, machine_mode mode);
//原型 const_int_operand tm-preds.h recog.cc
bool mtcs_preds_const_int_operand (MtcsPreds *self,rtx op, machine_mode mode);
//原型 const_int_operand tm-preds.h recog.cc #if TARGET_SUPPORTS_WIDE_INT
bool mtcs_preds_const_scalar_int_operand (MtcsPreds *self,rtx op, machine_mode mode);
//bool const_double_operand (rtx op, machine_mode mode)
bool mtcs_preds_const_double_operand (MtcsPreds *self,rtx op, machine_mode mode);
//原型 pop_operand  tm-preds.h recog.cc
bool mtcs_preds_pop_operand (MtcsPreds *self,rtx op, machine_mode mode);
//原型 nonimmediate_operand tm-preds.h recog.cc
bool mtcs_preds_nonimmediate_operand (MtcsPreds *self,rtx op, machine_mode mode);
//原型 nonmemory_operand tm-preds.h recog.cc
bool mtcs_preds_nonmemory_operand (MtcsPreds *self,rtx op, machine_mode mode);
//原型 memory_operand tm-preds.h recog.cc
bool mtcs_preds_memory_operand (MtcsPreds *self,rtx op, machine_mode mode);
//原型 indirect_operand tm-preds.h recog.cc
bool mtcs_preds_indirect_operand (MtcsPreds *self,rtx op, machine_mode mode);
//原型 bool aligned_register_operand (rtx op, machine_mode mode) tm-preds.h insn-preds.cc
bool mtcs_preds_aligned_register_operand (MtcsPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED);
//原型  ordered_comparison_operator tm-preds.h recog.cc
bool mtcs_preds_ordered_comparison_operator (MtcsPreds *self,rtx op, machine_mode mode);
//原型  comparison_operator tm-preds.h recog.cc
bool mtcs_preds_comparison_operator (MtcsPreds *self,rtx op, machine_mode mode);
//以上是gensupport定义的谓词

//以下是tm-preds.h中定义的方法
//原型 test_register_filters tm-preds.h ira-color.cc引用
bool mtcs_preds_test_register_filters(MtcsPreds *self,unsigned int mask, unsigned int regno);
//原型 lookup_constraint lookup_constraint tm-preds.h
int mtcs_preds_lookup_constraint(MtcsPreds *self,unsigned char *p);
/* Return true if X satisfies constraint C.  */
//原型  constraint_satisfied_p tm-preds.h
bool mtcs_preds_constraint_satisfied_p (MtcsPreds *self,rtx x, int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
//原型  insn_extra_register_constraint tm-preds.h
bool mtcs_preds_insn_extra_register_constraint(MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
//原型  insn_extra_memory_constraint tm-preds.h
bool mtcs_preds_insn_extra_memory_constraint(MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
//原型  insn_extra_special_memory_constraint tm-preds.h
bool mtcs_preds_insn_extra_special_memory_constraint(MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
//原型  insn_extra_relaxed_memory_constraint tm-preds.h
bool mtcs_preds_insn_extra_relaxed_memory_constraint (MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
//原型  insn_extra_address_constraint tm-preds.h
bool mtcs_preds_insn_extra_address_constraint (MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
//原型  insn_extra_constraint_allows_reg_mem tm-preds.h
void mtcs_preds_insn_extra_constraint_allows_reg_mem(MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/,
                   bool *allows_reg, bool *allows_mem);
//原型 #define CONSTRAINT_LEN(c_,s_) insn_constraint_len (c_,s_) tm-preds.h
int mtcs_preds_insn_constraint_len(MtcsPreds *self,char fc, const char *str ATTRIBUTE_UNUSED);
//原型 reg_class_for_constraint tm-preds.h
mtcs_reg_class mtcs_preds_reg_class_for_constraint (MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
//原型 get_register_filter_id tm-preds.h
int mtcs_preds_get_register_filter_id (MtcsPreds *self,int constraint_num/*!enum constraint_num 每个平台都不一样，所以改为int*/);
//原型 get_constraint_type tm-preds.h
int  mtcs_preds_get_constraint_type (MtcsPreds *self,int constraint_num/*!enum constraint_num 每个平台都不一样，所以改为int*/);

//原型 insn_const_int_ok_for_constraint (HOST_WIDE_INT, enum constraint_num); tm-preds.h
bool mtcs_preds_insn_const_int_ok_for_constraint(MtcsPreds *self,HOST_WIDE_INT ival,
        int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
//原型 get_register_filter tm-preds.h
const HardRegSet *mtcs_preds_get_register_filter (MtcsPreds *self, int constraint_num);
//在ptx-insn-output.c中定义的insn_data 中，关于谓词部分有带两个参数和三个参数的函数之分
//例如:mtcs_preds_nonimmediate_operand 带三个参数, ptx_nvptx_register_operand只带两个，
//把带三个参数的定义为公共谓词函数，两个的是平台函数
nboolean mtcs_preds_is_common(MtcsPreds *self,void *func);

#endif
