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

#ifndef __GCC_MTCS_PTX_PREDS__
#define __GCC_MTCS_PTX_PREDS__

#include "aet/nlib.h"
#include "../mtcspreds.h"



typedef struct _MtcsPtxPreds MtcsPtxPreds;


struct _MtcsPtxPreds
{
    MtcsPreds parent;
};

/*
#ifdef HAVE_MACHINE_MODES
extern bool general_operand (rtx, machine_mode); //公共 recog.cc实现  aet mtcspreds.c
extern bool address_operand (rtx, machine_mode); //公共 recog.cc实现  aet还没实现
extern bool register_operand (rtx, machine_mode); //公共 recog.cc实现 aet mtcspreds.c
extern bool pmode_register_operand (rtx, machine_mode);//公共 recog.cc实现  aet还没实现
extern bool scratch_operand (rtx, machine_mode);       //公共 recog.cc实现  aet还没实现
extern bool immediate_operand (rtx, machine_mode);     //公共 recog.cc实现  aet还没实现
extern bool const_int_operand (rtx, machine_mode);      //公共 recog.cc实现  aet还没实现
extern bool const_scalar_int_operand (rtx, machine_mode);  //公共 recog.cc实现  aet还没实现
extern bool const_double_operand (rtx, machine_mode);     //公共 recog.cc实现  aet还没实现
extern bool nonimmediate_operand (rtx, machine_mode);   //公共 recog.cc实现  aet还没实现
extern bool nonmemory_operand (rtx, machine_mode);      //公共 recog.cc实现  aet还没实现
extern bool push_operand (rtx, machine_mode);           //公共 recog.cc实现  aet mtcspreds.c
extern bool pop_operand (rtx, machine_mode);             //公共 recog.cc实现  aet还没实现
extern bool memory_operand (rtx, machine_mode);          //公共 recog.cc实现  aet还没实现
extern bool indirect_operand (rtx, machine_mode);        //公共 recog.cc实现  aet还没实现
extern bool ordered_comparison_operator (rtx, machine_mode); //公共 recog.cc实现  aet还没实现
extern bool comparison_operator (rtx, machine_mode);         //公共 recog.cc实现  aet还没实现
以上来自文件gensupport.cc中定义，固定实现
aligned_register_operand来自common.md
extern bool aligned_register_operand (rtx, machine_mode);     //平台 insn-preds.cc  aet还没实现
//下面来自nvptx_md
extern bool nvptx_register_operand (rtx, machine_mode);       //平台 insn-preds.cc  aet还没实现
extern bool nvptx_register_or_complex_di_df_register_operand (rtx, machine_mode); //平台 insn-preds.cc  aet还没实现
extern bool nvptx_nonimmediate_operand (rtx, machine_mode); //平台 insn-preds.cc  aet还没实现
extern bool nvptx_nonmemory_operand (rtx, machine_mode); //平台 insn-preds.cc  aet还没实现
extern bool const0_operand (rtx, machine_mode);          //平台 insn-preds.cc  aet还没实现
extern bool predicate_operator (rtx, machine_mode);      //平台 insn-preds.cc  aet还没实现
extern bool ne_operator (rtx, machine_mode);             //平台 insn-preds.cc  aet还没实现
extern bool nvptx_comparison_operator (rtx, machine_mode);        //平台 insn-preds.cc  aet还没实现
extern bool nvptx_float_comparison_operator (rtx, machine_mode);  //平台 insn-preds.cc  aet还没实现
extern bool nvptx_vector_index_operand (rtx, machine_mode);        //平台 insn-preds.cc  aet还没实现
extern bool call_insn_operand (rtx, machine_mode);                 //平台 insn-preds.cc  aet还没实现
extern bool call_operation (rtx, machine_mode);                      //平台 insn-preds.cc  aet还没实现
extern bool symbol_ref_function_operand (rtx, machine_mode);   //平台 insn-preds.cc  aet还没实现
#endif /* HAVE_MACHINE_MODES
*/

MtcsPtxPreds *mtcs_ptx_preds_new(MtcsMode *mtcsMode);
//原型 nvptx_register_operand tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_register_operand (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED);
//原型 nvptx_register_or_complex_di_df_register_operand tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_register_or_complex_di_df_register_operand (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED);
//原型 nvptx_nonimmediate_operand tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_nonimmediate_operand (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED);
//原型 nvptx_nonmemory_operand tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_nonmemory_operand (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED);
//原型 const0_operand tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_const0_operand (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED);
//原型 predicate_operator tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_predicate_operator (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED);
//原型 ne_operator tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_ne_operator (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED);
//原型 nvptx_comparison_operator tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_comparison_operator (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED);
//原型 nvptx_float_comparison_operator tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_float_comparison_operator (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED);
//原型 nvptx_vector_index_operand tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_vector_index_operand (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED);
//原型 call_insn_operand tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_call_insn_operand (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED);
//原型 call_insn_operand tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_call_operation (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED);
//原型 symbol_ref_function_operand tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_symbol_ref_function_operand (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED);



#endif
