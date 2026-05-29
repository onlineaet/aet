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

#include "mtcsptxpreds.h"
#include "ptx-common.h"
#include "mtcsptx.h"
#include "ptxtool.h"
#include "../mtcscompile.h"
#include "gen/ptx-insn-preds.h"
#include "gen/ptx-insn-modes.h"

/**
 * mtcsptxpreds 是tm-preds.h tm-constrs.h insn-preds.cc 三者的集合
 */
//原型 test_register_filters tm-preds.h ira-color.cc引用
static bool testRegisterFilters_cb(MtcsPreds *self,unsigned int mask, unsigned int regno);
//原型 lookup_constraint lookup_constraint tm-preds.h
static int lookupConstraint_cb(MtcsPreds *mtcsPreds,unsigned char *p);
//原型  constraint_satisfied_p tm-preds.h
static bool constraintSatisfiedP_cb(MtcsPreds *mtcsPreds,rtx x, int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
//原型  insn_extra_register_constraint tm-preds.h
static bool insnExtraRegisterConstraint_cb(MtcsPreds *mtcsPreds,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
//原型  insn_extra_memory_constraint tm-preds.h
static bool insnExtraMemoryConstraint_cb(MtcsPreds *mtcsPreds,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
//原型  insn_extra_special_memory_constraint tm-preds.h
static bool insnExtraSpecialMemoryConstraint_cb(MtcsPreds *mtcsPreds,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
//原型  insn_extra_relaxed_memory_constraint tm-preds.h
static bool insnExtraRelaxedMemoryConstraint_cb (MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
//原型  insn_extra_address_constraint tm-preds.h
static bool insnExtraAddressConstraint_cb(MtcsPreds *mtcsPreds,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
//原型  insn_extra_constraint_allows_reg_mem tm-preds.h
static void insnExtraConstraintAllowsRegMem_cb(MtcsPreds *mtcsPreds,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/,
                   bool *allows_reg, bool *allows_mem);
//原型 #define CONSTRAINT_LEN(c_,s_) insn_constraint_len (c_,s_) tm-preds.h
static int insnConstraintLen_cb(MtcsPreds *mtcsPreds,char fc, const char *str ATTRIBUTE_UNUSED);
//原型 reg_class_for_constraint tm-preds.h
static mtcs_reg_class regClassForConstraint_cb(MtcsPreds *mtcsPreds,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);
static int  getConstraintType_cb (MtcsPreds *mtcsPreds,int constraint_num/*!enum constraint_num 每个平台都不一样，所以改为int*/);
//原型 get_register_filter tm-preds.h
static const HardRegSet *getRegisterFilter_cb (MtcsPreds *self, int constraint_num/*!enum constraint_num 每个平台都不一样，所以改为int*/);
//原型 get_register_filter_id tm-preds.h
static int getRegisterFilterId_cb (MtcsPreds *mtcsPreds,int constraint_num/*!enum constraint_num 每个平台都不一样，所以改为int*/);
//原型 extern bool aligned_register_operand (rtx, machine_mode); tm-preds.h insn-preds.cc 来自common.md
static bool alignedRegisterOperand_cb(MtcsPreds *mtcsPreds,rtx op, machine_mode mode);



//原型 insn_const_int_ok_for_constraint (HOST_WIDE_INT, enum constraint_num); tm-preds.h
static bool insnConstIntOkForConstraint_cb(MtcsPreds *mtcsPreds,HOST_WIDE_INT ival,
        int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/);


static void mtcsPtxPredsInit(MtcsPtxPreds *self)
{
    MtcsPreds *mtcsPreds=(MtcsPreds *)self;
    //原型 test_register_filters tm-preds.h ira-color.cc引用
    mtcsPreds->test_register_filters =testRegisterFilters_cb;
    mtcsPreds->lookup_constraint =lookupConstraint_cb;
    mtcsPreds->constraint_satisfied_p =constraintSatisfiedP_cb;
    mtcsPreds->insn_extra_register_constraint =insnExtraRegisterConstraint_cb;
    mtcsPreds->insn_extra_memory_constraint =insnExtraMemoryConstraint_cb;
    mtcsPreds->insn_extra_special_memory_constraint =insnExtraSpecialMemoryConstraint_cb;
    mtcsPreds->insn_extra_relaxed_memory_constraint =insnExtraRelaxedMemoryConstraint_cb;
    mtcsPreds->insn_extra_address_constraint =insnExtraAddressConstraint_cb;
    mtcsPreds->insn_extra_constraint_allows_reg_mem =insnExtraConstraintAllowsRegMem_cb;
    mtcsPreds->insn_constraint_len =insnConstraintLen_cb;

    mtcsPreds->reg_class_for_constraint =regClassForConstraint_cb;
    mtcsPreds->insn_const_int_ok_for_constraint =insnConstIntOkForConstraint_cb;
    mtcsPreds->get_constraint_type =getConstraintType_cb;
    mtcsPreds->get_register_filter =getRegisterFilter_cb;
    mtcsPreds->get_register_filter_id=getRegisterFilterId_cb;
    //原型 extern bool aligned_register_operand (rtx, machine_mode); tm-preds.h insn-preds.cc 来自common.md
    mtcsPreds->aligned_register_operand=alignedRegisterOperand_cb;
}

//原型 test_register_filters tm-preds.h ira-color.cc引用
static bool testRegisterFilters_cb(MtcsPreds *self,unsigned int mask, unsigned int regno)
{
   return ptx_test_register_filters(mask,regno);
}

//原型 lookup_constraint lookup_constraint tm-preds.h
static int lookupConstraint_cb(MtcsPreds *mtcsPreds,unsigned char *p)
{
   return (int)ptx_lookup_constraint(p);
}

/* Return true if X satisfies constraint C.  */
//原型  constraint_satisfied_p tm-preds.h
static bool constraintSatisfiedP_cb(MtcsPreds *self,rtx x, int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{
     return ptx_constraint_satisfied_p (x, (enum ptx_constraint_num)constraintNum);
}

//原型  insn_extra_register_constraint tm-preds.h
static bool insnExtraRegisterConstraint_cb(MtcsPreds *mtcsPreds,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{
    return  ptx_insn_extra_register_constraint ((enum ptx_constraint_num)constraintNum);
}

//原型  insn_extra_memory_constraint tm-preds.h
static bool insnExtraMemoryConstraint_cb(MtcsPreds *mtcsPreds,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{
   return  ptx_insn_extra_memory_constraint ((enum ptx_constraint_num)constraintNum);
}

//原型  insn_extra_special_memory_constraint tm-preds.h
static bool insnExtraSpecialMemoryConstraint_cb(MtcsPreds *mtcsPreds,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{
   return  ptx_insn_extra_special_memory_constraint ((enum ptx_constraint_num)constraintNum);
}

//原型  insn_extra_relaxed_memory_constraint tm-preds.h
static bool insnExtraRelaxedMemoryConstraint_cb (MtcsPreds *mtcsPreds,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{
   return  ptx_insn_extra_relaxed_memory_constraint ((enum ptx_constraint_num)constraintNum);
}

//原型  insn_extra_address_constraint tm-preds.h
static bool insnExtraAddressConstraint_cb(MtcsPreds *mtcsPreds,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{
   return  ptx_insn_extra_address_constraint ((enum ptx_constraint_num)constraintNum);
}

//原型  insn_extra_constraint_allows_reg_mem tm-preds.h
static void insnExtraConstraintAllowsRegMem_cb(MtcsPreds *mtcsPreds,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/,
                   bool *allows_reg, bool *allows_mem)
{
   ptx_insn_extra_constraint_allows_reg_mem((enum ptx_constraint_num)constraintNum,allows_reg,allows_mem);
}

//原型 #define CONSTRAINT_LEN(c_,s_) insn_constraint_len (c_,s_)  tm-preds.h
static int insnConstraintLen_cb(MtcsPreds *mtcsPreds,char fc, const char *str ATTRIBUTE_UNUSED)
{
   return ptx_insn_constraint_len(fc,str);
}

//原型 reg_class_for_constraint tm-preds.h
static mtcs_reg_class regClassForConstraint_cb(MtcsPreds *mtcsPreds,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{
     return (mtcs_reg_class)ptx_reg_class_for_constraint((enum ptx_constraint_num)constraintNum);
}

//原型 get_constraint_type tm-preds.h
static int getConstraintType_cb (MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{
   return (mtcs_reg_class)ptx_get_constraint_type((enum ptx_constraint_num)constraintNum);
}

//原型 get_register_filter tm-preds.h
static const HardRegSet *getRegisterFilter_cb (MtcsPreds *self, int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{
   return ptx_get_register_filter((enum ptx_constraint_num)constraintNum);
}

//原型 get_register_filter_id tm-preds.h
static int getRegisterFilterId_cb (MtcsPreds *mtcsPreds,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{
   return ptx_get_register_filter_id((enum ptx_constraint_num)constraintNum);
}

//原型 nvptx_register_operand tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_register_operand (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED)
{
  return ptx_nvptx_register_operand(op,mode);
}

//原型 nvptx_register_or_complex_di_df_register_operand tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_register_or_complex_di_df_register_operand (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED)
{
  return ptx_nvptx_register_or_complex_di_df_register_operand(op,mode);
}

//原型 nvptx_nonimmediate_operand tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_nonimmediate_operand (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED)
{
   return ptx_nvptx_nonimmediate_operand(op,mode);
}

//原型 nvptx_nonmemory_operand tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_nonmemory_operand (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED)
{
  return ptx_nvptx_nonmemory_operand(op,mode);
}

//原型 const0_operand tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_const0_operand (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED)
{
  return ptx_const0_operand(op,mode);
}

//原型 predicate_operator tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_predicate_operator (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED)
{
   return ptx_predicate_operator(op,mode);
}

//原型 ne_operator tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_ne_operator (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED)
{
   return ptx_ne_operator(op,mode);
}

//原型 nvptx_comparison_operator tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_comparison_operator (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED)
{
   return ptx_nvptx_comparison_operator(op,mode);
}

//原型 nvptx_float_comparison_operator tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_float_comparison_operator (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED)
{
   return ptx_nvptx_float_comparison_operator(op,mode);
}
//原型 nvptx_vector_index_operand tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_vector_index_operand (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED)
{
   return ptx_nvptx_vector_index_operand(op,mode);
}

//原型 call_insn_operand tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_call_insn_operand (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED)
{
   return ptx_call_insn_operand(op,mode);

}

//原型 call_operation tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_call_operation (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED)
{
   return ptx_call_operation(op,mode);
}

//原型 ptx_symbol_ref_function_operand tm-preds.h insn-preds.cc
bool mtcs_ptx_preds_symbol_ref_function_operand (MtcsPtxPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED)
{
   return ptx_symbol_ref_function_operand(op,mode);

}

//原型 insn_const_int_ok_for_constraint (HOST_WIDE_INT, enum constraint_num); tm-preds.h
//代码来自insn-preds.cc
static bool insnConstIntOkForConstraint_cb(MtcsPreds *mtcsPreds,HOST_WIDE_INT ival,
        int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{

  return  ptx_insn_const_int_ok_for_constraint(ival,(enum ptx_constraint_num)constraintNum);
}

//原型 extern bool aligned_register_operand (rtx, machine_mode); tm-preds.h insn-preds.cc 来自common.md
static bool alignedRegisterOperand_cb(MtcsPreds *mtcsPreds,rtx op, machine_mode mode)
{
   return ptx_aligned_register_operand(op,mode);
}


MtcsPtxPreds *mtcs_ptx_preds_new(MtcsMode *mtcsMode)
{
     MtcsPtxPreds *self = n_slice_alloc0 (sizeof(MtcsPreds));
     mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
     mtcs_preds_init((MtcsPreds *)self);
     mtcsPtxPredsInit(self);
     ptx_preds_set_target((void*)mtcsMode->target);
     return self;
}


