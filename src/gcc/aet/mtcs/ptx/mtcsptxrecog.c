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
#define IN_TARGET_CODE 1 //不加这句在machmode.h中的GET_MODE_SIZE编译到poly_uint16(poly_int) 因为poly_int没有重载>号，所以编译报错
//insn-modes.h由nvptx生成，但i386生成的类型全覆盖nvptx的insn-modes.h,不需要平台的？？？
#include "config.h"
#define INCLUDE_ALGORITHM /* reverse */
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "tree-pass.h"
#include "cfghooks.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "tree-pretty-print.h" /* for dump_function_header */
#include "varasm.h"
#include "insn-attr.h"
#include "conditions.h"
#include "flags.h"
#include "output.h"
#include "except.h"
#include "rtl-error.h"
#include "toplev.h" /* exact_log2, floor_log2 */
#include "reload.h"
#include "intl.h"
#include "cfgrtl.h"
#include "debug.h"
#include "tree-pass.h"
#include "tree-ssa.h"
#include "cfgloop.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "rtl-iter.h"
#include "print-rtl.h"
#include "function-abi.h"
#include "common/common-target.h"
#include "diagnostic.h"
#include "context.h"
#include "options.h"
#include "machmode.h"
#include "poly-int-types.h"
#include "opts.h"

#include "aet/aetprinttree.h"
#include "../mtcstool.h"
#include "../mtcstarget.h"
#include "mtcsptxrecog.h"
#include "ptx-common.h"
#include "gen/ptx-insn-modes.h"
#include "../mtcsprintrtl.h"

#define PTX_MAX_RECOG_OPERANDS 30


  //原型 LEGITIMATE_PIC_OPERAND_P default.h 每个平台都有
static nboolean isLegitimatePicOperandP_cb(MtcsRecog *mtcsRecog,rtx op);
//原型 extern void add_clobbers (rtx, int); recog.h
static void addClobbers_cb(MtcsRecog *mtcsRecog,rtx pattern ATTRIBUTE_UNUSED, int insn_code_number);
//原型 extern bool added_clobbers_hard_reg_p (int);recog.h
static bool addedClobbersHardRegP_cb(MtcsRecog *mtcsRecog,int insn_code_number);
//原型extern rtx_insn *peephole2_insns (rtx, rtx_insn *, int *); recog.h insn-recog.cc
static rtx_insn *peephole2Insns_cb (MtcsRecog *mtcsRecog, rtx x1 ATTRIBUTE_UNUSED,
    rtx_insn *insn ATTRIBUTE_UNUSED, int *pmatch_len_ ATTRIBUTE_UNUSED);
//原型rtx_insn *split_insns (rtx x1 ATTRIBUTE_UNUSED, rtx_insn *insn ATTRIBUTE_UNUSED) recog.h insn-recog.cc
static rtx_insn *splitInsns_cb (MtcsRecog *mtcsRecog,rtx x1 ATTRIBUTE_UNUSED, rtx_insn *insn ATTRIBUTE_UNUSED);
//原型int recog (rtx x1 ATTRIBUTE_UNUSED,rtx_insn *insn ATTRIBUTE_UNUSED,int *pnum_clobbers ATTRIBUTE_UNUSED) recog.h insn-recog.cc
static int recog_cb (MtcsRecog *mtcsRecog,rtx x1 ATTRIBUTE_UNUSED,rtx_insn *insn ATTRIBUTE_UNUSED,int *pnum_clobbers ATTRIBUTE_UNUSED);

static void mtcsPtxRecogInit(MtcsPtxRecog *self)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsRecog *mtcsRecog=(MtcsRecog*)self;
  //原型 LEGITIMATE_PIC_OPERAND_P default.h 每个平台都有
    mtcsRecog->is_legitimate_pic_operand_p=isLegitimatePicOperandP_cb;
   //原型 extern void add_clobbers (rtx, int); recog.h
    mtcsRecog->add_clobbers=addClobbers_cb;
   //原型 extern bool added_clobbers_hard_reg_p (int);recog.h
    mtcsRecog->added_clobbers_hard_reg_p=addedClobbersHardRegP_cb;
    //原型extern rtx_insn *peephole2_insns (rtx, rtx_insn *, int *); recog.h insn-recog.cc
    mtcsRecog->peephole2_insns= peephole2Insns_cb;
    //原型rtx_insn *split_insns (rtx x1 ATTRIBUTE_UNUSED, rtx_insn *insn ATTRIBUTE_UNUSED) recog.h insn-recog.cc
    mtcsRecog->split_insns= splitInsns_cb;
    //原型int recog (rtx x1 ATTRIBUTE_UNUSED,rtx_insn *insn ATTRIBUTE_UNUSED,int *pnum_clobbers ATTRIBUTE_UNUSED) recog.h insn-recog.cc
    mtcsRecog->recog= recog_cb;
    //原型 insn_extract recog.h recog.cc 由mtcsgenextract.c 生成文件 平台-insn-extract.c文件实现。
    mtcsRecog->insn_extract= ptx_insn_extract_cb;//ptx_insn_extract_cb由mtcsgenextract.c生成

    mtcs_ptx_recog_set_target(self,mtcsTarget);//为生成的代码ptx-insn-recog.c设目标
    mtcs_recog_set_max_recog_operands(mtcsRecog,PTX_MAX_RECOG_OPERANDS);
}

//原型 LEGITIMATE_PIC_OPERAND_P default.h 每个平台都有
static nboolean isLegitimatePicOperandP_cb(MtcsRecog *mtcsRecog,rtx op)
{
    return PTX_LEGITIMATE_PIC_OPERAND_P(op);
}
//原型 extern void add_clobbers (rtx, int); recog.h
static void addClobbers_cb(MtcsRecog *mtcsRecog,rtx pattern ATTRIBUTE_UNUSED, int insn_code_number)
{
    return ptx_add_clobbers(pattern,insn_code_number);
}
//原型 extern bool added_clobbers_hard_reg_p (int);recog.h
static bool addedClobbersHardRegP_cb(MtcsRecog *mtcsRecog,int insn_code_number)
{
    return ptx_added_clobbers_hard_reg_p(insn_code_number);
}

//原型extern rtx_insn *peephole2_insns (rtx, rtx_insn *, int *); recog.h insn-recog.cc
static rtx_insn *peephole2Insns_cb (MtcsRecog *mtcsRecog, rtx x1 ATTRIBUTE_UNUSED,
    rtx_insn *insn ATTRIBUTE_UNUSED, int *pmatch_len_ ATTRIBUTE_UNUSED)
{
    return ptx_peephole2_insns(x1,insn,pmatch_len_);
}

//原型rtx_insn *split_insns (rtx x1 ATTRIBUTE_UNUSED, rtx_insn *insn ATTRIBUTE_UNUSED) recog.h insn-recog.cc
static rtx_insn *splitInsns_cb (MtcsRecog *mtcsRecog,rtx x1 ATTRIBUTE_UNUSED, rtx_insn *insn ATTRIBUTE_UNUSED)
{
    return ptx_split_insns(x1,insn);
}

//原型int recog (rtx x1 ATTRIBUTE_UNUSED,rtx_insn *insn ATTRIBUTE_UNUSED,int *pnum_clobbers ATTRIBUTE_UNUSED) recog.h insn-recog.cc
static int recog_cb (MtcsRecog *mtcsRecog,rtx x1 ATTRIBUTE_UNUSED,rtx_insn *insn ATTRIBUTE_UNUSED,int *pnum_clobbers ATTRIBUTE_UNUSED)
{
//   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsRecog);
//   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
//   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
//
//   rtx * const operands  = &mtcsTarget->mtcsRecog->recog_data.operand[0];
//   rtx x2, x3, x4;
//   int res ;
//   x2 = XEXP (x1, 1);
//   operands[1] = x2;
//   x3 = XEXP (x1, 0);
//   operands[0] = x3;
//   int mode = GET_MODE (operands[0]);
//   fprintf(stderr,"mtcsptxrecog.c  recog_cb  x1:%d %s %s\n",GET_CODE (x1),GET_RTX_NAME(GET_CODE(x1)),GET_RTX_FORMAT(GET_CODE(x1)));
////   if(x2)
////      fprintf(stderr,"mtcsptxrecog.c  recog_cb  x2:%d %s %s\n",GET_CODE (x2),GET_RTX_NAME(GET_CODE(x2)),GET_RTX_FORMAT(GET_CODE(x2)));
////   if(x3)
////      fprintf(stderr,"mtcsptxrecog.c  recog_cb  x3:%d %s %s\n",GET_CODE (x3),GET_RTX_NAME(GET_CODE(x3)),GET_RTX_FORMAT(GET_CODE(x3)));
//
//   // mtcsptxrecog.c  recog_cb code:25 :set x2:42 reg 6 6 测试用
//   if(GET_CODE (x1)==SET && GET_CODE (x2)==REG && mode==PTX_SImode){
//      fprintf(stderr,"mtcsptxrecog.c  recog_cb code:%d :%s x2:%d %s %d %d volatile_ok:%d\n",GET_CODE (x1),GET_RTX_NAME(GET_CODE(x1)),
//      GET_CODE (x2),GET_RTX_NAME(GET_CODE(x2)),mode,PTX_SImode,mtcsRecog->volatile_ok);
//      mtcs_print_rtl_single(stderr,operands[0]);
//      n_debug("testprint------operands[1]");
//      mtcs_print_rtl_single(stderr,operands[1]);
//
//      nboolean r1=mtcs_preds_nonimmediate_operand/*!nonimmediate_operand*/(mtcsTarget->mtcsPreds,operands[0], (machine_mode)PTX_SImode);
//      nboolean r2=  mtcs_preds_general_operand/*!general_operand*/(mtcsTarget->mtcsPreds,operands[1], (machine_mode)PTX_SImode);
//      nboolean r3=MEM_P (operands[0]);
//      nboolean r4=REG_P (operands[1]);
//      nboolean r5 =mtcs_preds_general_operand (mtcsTarget->mtcsPreds,operands[0], (machine_mode)PTX_SImode);
//      nboolean r6 =CONSTANT_P (operands[0]);
//
//      fprintf(stderr,"mtcsptxrecog.c  recog_cb --- movsi_insn :%d %d %d %d %d %d\n",r1,r2,r3,r4,r5,r6);
//   }

   //    if (mtcs_preds_nonimmediate_operand/*!nonimmediate_operand*/(mtcsTarget->mtcsPreds,operands[0], (machine_mode)PTX_SImode)
   //          && mtcs_preds_general_operand/*!general_operand*/(mtcsTarget->mtcsPreds,operands[1], (machine_mode)PTX_SImode)
   //          &&
   //#line 262 "../../gcc-15/gcc/aet/mtcs/ptx/mtcs_ptx.md"
   //(!MEM_P (operands[0]) || REG_P (operands[1])))
   return ptx_recog(x1,insn,pnum_clobbers);
}

MtcsPtxRecog *mtcs_ptx_recog_new(MtcsMode *mtcsMode)
{
     MtcsPtxRecog *self = n_slice_alloc0 (sizeof(MtcsPtxRecog));
     mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
     mtcsPtxRecogInit(self);
     return self;
}


