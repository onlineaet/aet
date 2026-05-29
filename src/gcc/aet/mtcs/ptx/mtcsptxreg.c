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
#include "mtcsptxreg.h"
#include "ptx-common.h"
#include "gen/ptx-insn-preds.h"

static char *registerNames[16]={
            "%value", "%stack", "%frame", "%args",
            "%chain", "%sspslot", "%sspprev", "%hr7",
            "%hr8", "%hr9", "%hr10", "%hr11",
            "%hr12", "%hr13", "%hr14", "%hr15"
    };

#define PTX_FIRST_PSEUDO_REGISTER 16
/**
 * 记录哪些硬件寄存器是代码中不可使用的
 */
#define PTX_FIXED_REGISTERS     { 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
/* 16个需要被恢复 0 表示不需要恢复*/
#define PTX_CALL_USED_REGISTERS { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }


#define PTX_REG_CLASS_NAMES    { "NO_REGS",  "ALL_REGS" }
#define PTX_REG_CLASS_CONTENTS { { 0x0000 }, { 0xFFFF } }
#define PTX_REGNO_REG_CLASS(R) ((void)(R), PTX_ALL_REGS)

static MtcsRegClass ptxRegClass[]={{0,"NO_REGS",{0x0000}},{1,"ALL_REGS",{0xFFFF}}};
#define PTX_N_REG_CLASSES     2
#define PTX_GENERAL_REGS PTX_ALL_REGS

#define PTX_FIRST_VIRTUAL_REGISTER  (PTX_FIRST_PSEUDO_REGISTER)

#define PTX_VIRTUAL_INCOMING_ARGS_REGNUM    (PTX_FIRST_VIRTUAL_REGISTER)
#define PTX_VIRTUAL_STACK_VARS_REGNUM   ((PTX_FIRST_VIRTUAL_REGISTER) + 1)
#define PTX_VIRTUAL_STACK_DYNAMIC_REGNUM    ((PTX_FIRST_VIRTUAL_REGISTER) + 2)
#define PTX_VIRTUAL_OUTGOING_ARGS_REGNUM    ((PTX_FIRST_VIRTUAL_REGISTER) + 3)
#define PTX_VIRTUAL_CFA_REGNUM      ((PTX_FIRST_VIRTUAL_REGISTER) + 4)
#define PTX_VIRTUAL_PREFERRED_STACK_BOUNDARY_REGNUM  ((PTX_FIRST_VIRTUAL_REGISTER) + 5)

//常用寄存器号码 STACK_POINTER_REGNUM FRAME_POINTER_REGNUM FRAME_POINTER_REGNUM(HARD_FRAME_POINTER_REGNUM) ARG_POINTER_REGNUM
static nuint regsNum[10]={PTX_STACK_POINTER_REGNUM,PTX_FRAME_POINTER_REGNUM,PTX_FRAME_POINTER_REGNUM/*HARD_FRAME_POINTER_REGNUM*/,
        PTX_ARG_POINTER_REGNUM,PTX_VIRTUAL_INCOMING_ARGS_REGNUM,PTX_VIRTUAL_STACK_VARS_REGNUM,
        PTX_VIRTUAL_STACK_DYNAMIC_REGNUM,PTX_VIRTUAL_OUTGOING_ARGS_REGNUM,PTX_VIRTUAL_CFA_REGNUM,
        PTX_VIRTUAL_PREFERRED_STACK_BOUNDARY_REGNUM};

static nboolean regnumPtrFrameP_cb(MtcsReg *self,int regnum);
//原型 #define EH_RETURN_DATA_REGNO(N) INVALID_REGNUM default.h
static int ehReturnDataRegno_cb(MtcsReg *self,int n);

static char *getFixedRegisters_cb( MtcsReg *reg)
{
    static const char initial_fixed_regs[] = PTX_FIXED_REGISTERS;
    return (char *)initial_fixed_regs;
}

static char *getCallUsedRegisters_cb( MtcsReg *reg)
{
    static const char initial_call_used_regs[] = PTX_CALL_USED_REGISTERS;
    return (char *)initial_call_used_regs;
}

static char **getRegisterNames_cb( MtcsReg *reg)
{
   return (char **)registerNames;
}

static int getClass_cb( MtcsReg *reg,int regno)
{
   return PTX_REGNO_REG_CLASS(regno);
}

static enum reg_class baseRegClass_cb (MtcsReg *mtcsReg,machine_mode mode ATTRIBUTE_UNUSED,
      addr_space_t as ATTRIBUTE_UNUSED, enum rtx_code outer_code ATTRIBUTE_UNUSED,
      enum rtx_code index_code ATTRIBUTE_UNUSED,rtx_insn *insn ATTRIBUTE_UNUSED)
{
   return (enum reg_class)PTX_BASE_REG_CLASS;
}

//原型 enum reg_class index_reg_class addresses.h
static enum reg_class indexRegClass_cb (MtcsReg *mtcsReg,rtx_insn *insn ATTRIBUTE_UNUSED)
{
   return PTX_INDEX_REG_CLASS;
}

//原型 enum reg_class ok_for_base_p_1 addresses.h
static bool okForBaseP_1_cb (MtcsReg *mtcsReg,unsigned regno ATTRIBUTE_UNUSED,
       machine_mode mode ATTRIBUTE_UNUSED,
       addr_space_t as ATTRIBUTE_UNUSED,
       enum rtx_code outer_code ATTRIBUTE_UNUSED,
       enum rtx_code index_code ATTRIBUTE_UNUSED,
       rtx_insn* insn ATTRIBUTE_UNUSED /*!= NULL*/)
{
   return PTX_REGNO_OK_FOR_BASE_P(regno);
}

//原型 #define REGNO_OK_FOR_INDEX_P(X) false nvptx.h
static bool regnoOkForIndexP_cb(MtcsReg *mtcsReg,unsigned regno)
{
   return PTX_REGNO_OK_FOR_INDEX_P(regno);
}

//原型 #define DEBUGGER_REGNO(N) N
static int getDebuggerRegno_cb(MtcsReg *self,int regno)
{
   return PTX_DEBUGGER_REGNO(regno);
}

//原型 #define DWARF_FRAME_REGNUM(REG) DEBUGGER_REGNO (REG)
static int getDwarfFrameRegnum_cb(MtcsReg *self,int regno)
{
   return PTX_DEBUGGER_REGNO(regno);
}

//原型 init_reg_class_start_regs reginfo.cc  In insn-preds.cc.
static void  initRegClassStartRegs_cb(MtcsReg *self)
{
   ptx_init_reg_class_start_regs();
}

static void mtcsPtxRegInit(MtcsPtxReg *self)
{
    MtcsReg *reg=(MtcsReg *)self;
    mtcs_reg_set_hard_reg_count(reg,PTX_FIRST_PSEUDO_REGISTER);//ptx=16 gcn=677
    mtcs_reg_set_reg_class(reg,ptxRegClass,PTX_N_REG_CLASSES);
    mtcs_reg_set_general_regs(reg,PTX_GENERAL_REGS);
    mtcs_reg_set_all_regs(reg,PTX_ALL_REGS);
    //原型 #define DWARF_FRAME_REGISTERS FIRST_PSEUDO_REGISTER
    mtcs_reg_set_dwarf_frame_registers(reg,PTX_FIRST_PSEUDO_REGISTER);

   // unsigned int regClassContens[PTX_N_REG_CLASSES][1]=PTX_REG_CLASS_CONTENTS;
   // unsigned int **temp=(unsigned int **)regClassContens;
   // mtcs_reg_copy_reg_class_contents(reg,temp,PTX_N_REG_CLASSES,1);
    //char fixReg[]=PTX_FIXED_REGISTERS;
    //char callUsedReg[]=PTX_CALL_USED_REGISTERS;
    mtcs_reg_set_stack_pointer_to_virtual_regnum(reg,regsNum);
    mtcs_reg_set_return_address_pointer_regnum(reg,-1);
    mtcs_reg_set_pic_offset_table_regnum(reg,-1);
    reg->get_fixed_registers=getFixedRegisters_cb;
    reg->get_call_used_registers=getCallUsedRegisters_cb;
    reg->get_register_names=getRegisterNames_cb;
    reg->get_class=getClass_cb;//原型 REGNO_REG_CLASS
    reg->regnum_ptr_frame_p=regnumPtrFrameP_cb;
    //原型 #define EH_RETURN_DATA_REGNO(N) INVALID_REGNUM default.h
    reg->eh_return_data_regno=ehReturnDataRegno_cb;
    //原型 enum reg_class base_reg_class addresses.h
    reg->base_reg_class=baseRegClass_cb;
    //原型 #define REGNO_OK_FOR_INDEX_P(X) false nvptx.h
    reg->regno_ok_for_index_p=regnoOkForIndexP_cb;
    //原型 enum reg_class index_reg_class addresses.h
    reg->index_reg_class=indexRegClass_cb;
    //原型 enum reg_class ok_for_base_p_1 addresses.h
    reg->ok_for_base_p_1=okForBaseP_1_cb;
    //原型 #define DEBUGGER_REGNO(N) N
    reg->get_debugger_regno=getDebuggerRegno_cb;
    //原型 #define DWARF_FRAME_REGNUM(REG) DEBUGGER_REGNO (REG)
    reg->get_dwarf_frame_regnum=getDwarfFrameRegnum_cb;
    //原型 init_reg_class_start_regs reginfo.cc  In insn-preds.cc.
    reg->init_reg_class_start_regs = initRegClassStartRegs_cb;

    mtcs_reg_set_move_max(reg,PTX_MOVE_MAX);
    mtcs_reg_set_move_max_pieces(reg,PTX_MOVE_MAX);
    mtcs_reg_set_store_max_pieces(reg,MIN (PTX_MOVE_MAX/*!MOVE_MAX_PIECES*/, 2 * sizeof (HOST_WIDE_INT)));
    mtcs_reg_set_max_move_max(reg,PTX_MOVE_MAX);
    mtcs_reg_set_compare_max_pieces(reg,PTX_MOVE_MAX);//原型 default.h
    //原型 HARD_FRAME_POINTER_IS_FRAME_POINTER rtl.h
    mtcs_reg_set_hard_frame_pointer_is_frame_pointer(reg,PTX_HARD_FRAME_POINTER_REGNUM==PTX_FRAME_POINTER_REGNUM);
    //原型 PIC_OFFSET_TABLE_REG_CALL_CLOBBERED default.h 缺省是零
    mtcs_reg_set_pic_offset_table_reg_call_clobbered(reg,0);
    //原型 #define WORD_REGISTER_OPERATIONS 0 defaults.h
    mtcs_reg_set_word_register_operations(reg,0);
    //原型 #define INDEX_REG_CLASS NO_REGS nvptx.h
    mtcs_reg_set_index_reg_class(reg,PTX_INDEX_REG_CLASS);

}


/* Nonzero if REGNUM is a pointer into the stack frame.  */
//#define REGNO_PTR_FRAME_P(REGNUM)       \
//  ((REGNUM) == STACK_POINTER_REGNUM     \
//   || (REGNUM) == FRAME_POINTER_REGNUM      \
//   || (REGNUM) == HARD_FRAME_POINTER_REGNUM \
//   || (REGNUM) == ARG_POINTER_REGNUM        \
//   || VIRTUAL_REGISTER_NUM_P (REGNUM))
//原型 REGNO_PTR_FRAME_P rtl.h
static  nboolean regnumPtrFrameP_cb(MtcsReg *mtcsReg,int regnum)
{
    return regnum==PTX_STACK_POINTER_REGNUM ||  regnum==PTX_FRAME_POINTER_REGNUM
            ||regnum==PTX_HARD_FRAME_POINTER_REGNUM ||regnum==PTX_ARG_POINTER_REGNUM
            || mtcs_reg_virtual_register_num_p(mtcsReg,regnum);
}

//原型 #define EH_RETURN_DATA_REGNO(N) INVALID_REGNUM default.h
static int ehReturnDataRegno_cb(MtcsReg *self,int n)
{
    return INVALID_REGNUM;
}

//原型 ELIMINABLE_REGS 每个平台设置不一样的ELIMINABLE_REGS
static void initEliminableRegs(MtcsPtxReg *self)
{
    MtcsReg *mtcsReg=(MtcsReg *)self;
    static const struct {const int from, to; } eliminables[] =PTX_ELIMINABLE_REGS;
    int i;
    for(i=0;i<ARRAY_SIZE (eliminables);i++){
        mtcsReg->eliminableRegs[i].from=eliminables[i].from;
        mtcsReg->eliminableRegs[i].to=eliminables[i].to;
    }
    mtcsReg->elimiableRegsCount=ARRAY_SIZE (eliminables);
}


MtcsPtxReg *mtcs_ptx_reg_new(MtcsMode *mtcsMode)
{
     MtcsPtxReg *self = n_slice_alloc0 (sizeof(MtcsPtxReg));
     mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
     mtcs_reg_init((MtcsReg *)self);
     mtcsPtxRegInit(self);
     initEliminableRegs(self);
     return self;
}


