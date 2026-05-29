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

#ifndef __GCC_PTX_COMMON__
#define __GCC_PTX_COMMON__

#include "aet/nlib.h"



#define PTXABI64 1

#define PTX_SHORT_TYPE_SIZE 16
#define PTX_INT_TYPE_SIZE 32
#define PTX_LONG_TYPE_SIZE (PTXABI64 ? 64 : 32)
#define PTX_LONG_LONG_TYPE_SIZE 64
#define PTX_FLOAT_TYPE_SIZE 32
#define PTX_DOUBLE_TYPE_SIZE 64
#define PTX_LONG_DOUBLE_TYPE_SIZE 64
#define PTX_TARGET_SUPPORTS_WIDE_INT 1

#define PTX_SIZE_TYPE (PTXABI64 ? "long unsigned int" : "unsigned int")

#define PTX_PTRDIFF_TYPE (PTXABI64 ? "long int" : "int")
#define PTX_POINTER_SIZE (PTXABI64 ? 64 : 32)


/* Chosen such that we won't have to deal with multi-word subregs.  */
#define PTX_UNITS_PER_WORD 8

/* Alignments in bits.  */
#define PTX_PARM_BOUNDARY 32
#define PTX_STACK_BOUNDARY 128
#define PTX_FUNCTION_BOUNDARY 32
#define PTX_BIGGEST_ALIGNMENT 128
#define PTX_STRICT_ALIGNMENT 1


#define PTX_MAX_STACK_ALIGNMENT (1024 * 8)

#define PTX_MAX_SUPPORTED_STACK_ALIGNMENT PTX_MAX_STACK_ALIGNMENT

#define PTX_DEFAULT_SIGNED_CHAR 1

/* Stack and Calling.  */

#define PTX_FRAME_GROWS_DOWNWARD 0
#define PTX_STACK_GROWS_DOWNWARD 1

#define PTX_NVPTX_RETURN_REGNUM 0
#define PTX_STACK_POINTER_REGNUM 1
#define PTX_FRAME_POINTER_REGNUM 2
#define PTX_ARG_POINTER_REGNUM 3
#define PTX_STATIC_CHAIN_REGNUM 4
#define PTX_HARD_FRAME_POINTER_REGNUM PTX_FRAME_POINTER_REGNUM

#define PTX_TRAMPOLINE_SIZE 32
#define PTX_TRAMPOLINE_ALIGNMENT 256

/* We don't run reload, so this isn't actually used, but it still needs to be
   defined.  Showing an argp->fp elimination also stops
   expand_builtin_setjmp_receiver from generating invalid insns.  */
#define PTX_ELIMINABLE_REGS                 \
  {                         \
    { PTX_ARG_POINTER_REGNUM, PTX_FRAME_POINTER_REGNUM} \
  }

/* This register points to the shared memory location with the current warp's
   soft stack pointer (__nvptx_stacks[tid.y]).  */
#define PTX_SOFTSTACK_SLOT_REGNUM 5
/* This register is used to save the previous value of the soft stack pointer
   in the prologue and restore it when returning.  */
#define PTX_SOFTSTACK_PREV_REGNUM 6

#define PTX_BITS_PER_WORD (BITS_PER_UNIT * PTX_UNITS_PER_WORD)

#define PTX_ASM_COMMENT_START "//"


extern int mtcs_ptx_isa_option; //对应 PtxIsa 中的某个值 mtcsptxoptions.c中定义
extern int mtcs_ptx_version_option; //对应 PtxVersion 中的某个值 mtcsptxoptions.c中定义

//原型 enum ptx_isa nvptx-opts.h nvptx-sm.def
typedef enum {
   MTCS_PTX_ISA_SM_30,
   MTCS_PTX_ISA_SM_35,
   MTCS_PTX_ISA_SM_37,
   MTCS_PTX_ISA_SM_52,
   MTCS_PTX_ISA_SM_53,
   MTCS_PTX_ISA_SM_61,
   MTCS_PTX_ISA_SM_70,
   MTCS_PTX_ISA_SM_75,
   MTCS_PTX_ISA_SM_80,
   MTCS_PTX_ISA_SM_89
}PtxIsa;

//原型 enum ptx_version  nvptx-opts.h
typedef enum {
   MTCS_PTX_VERSION_3_1,
   MTCS_PTX_VERSION_4_1,
   MTCS_PTX_VERSION_4_2,
   MTCS_PTX_VERSION_5_0,
   MTCS_PTX_VERSION_6_0,
   MTCS_PTX_VERSION_6_3,
   MTCS_PTX_VERSION_7_0,
   MTCS_PTX_VERSION_7_3,
   MTCS_PTX_VERSION_7_8
}PtxVersion;

//mtcsptxoptions.c 需要下面的宏
#define PTX_VERSION_3_1      MTCS_PTX_VERSION_3_1
#define PTX_VERSION_4_1      MTCS_PTX_VERSION_4_1
#define PTX_VERSION_4_2      MTCS_PTX_VERSION_4_2
#define PTX_VERSION_5_0      MTCS_PTX_VERSION_5_0
#define PTX_VERSION_6_0      MTCS_PTX_VERSION_6_0
#define PTX_VERSION_6_3      MTCS_PTX_VERSION_6_3
#define PTX_VERSION_7_0      MTCS_PTX_VERSION_7_0
#define PTX_VERSION_7_3      MTCS_PTX_VERSION_7_3
#define PTX_VERSION_7_8      MTCS_PTX_VERSION_7_8
#define PTX_VERSION_default  MTCS_PTX_VERSION_3_1

#define PTX_ISA_SM30         MTCS_PTX_ISA_SM_30
#define PTX_ISA_SM35         MTCS_PTX_ISA_SM_35
#define PTX_ISA_SM37         MTCS_PTX_ISA_SM_37
#define PTX_ISA_SM52         MTCS_PTX_ISA_SM_52
#define PTX_ISA_SM53         MTCS_PTX_ISA_SM_53
#define PTX_ISA_SM61         MTCS_PTX_ISA_SM_61
#define PTX_ISA_SM70         MTCS_PTX_ISA_SM_70
#define PTX_ISA_SM75         MTCS_PTX_ISA_SM_75
#define PTX_ISA_SM80         MTCS_PTX_ISA_SM_80
#define PTX_ISA_SM89         MTCS_PTX_ISA_SM_89



//原型 nvptx-gen.h
#define TARGET_SM30 (mtcs_ptx_isa_option >= PTX_ISA_SM30)
#define TARGET_SM35 (mtcs_ptx_isa_option >= PTX_ISA_SM35)
#define TARGET_SM37 (mtcs_ptx_isa_option >= PTX_ISA_SM37)
#define TARGET_SM52 (mtcs_ptx_isa_option >= PTX_ISA_SM52)
#define TARGET_SM53 (mtcs_ptx_isa_option >= PTX_ISA_SM53)
#define TARGET_SM61 (mtcs_ptx_isa_option >= PTX_ISA_SM61)
#define TARGET_SM70 (mtcs_ptx_isa_option >= PTX_ISA_SM70)
#define TARGET_SM75 (mtcs_ptx_isa_option >= PTX_ISA_SM75)
#define TARGET_SM80 (mtcs_ptx_isa_option >= PTX_ISA_SM80)
#define TARGET_SM89 (mtcs_ptx_isa_option >= PTX_ISA_SM89)

//原型 nvptx.h
#define TARGET_PTX_4_1 (mtcs_ptx_version_option >= PTX_VERSION_4_1)
#define TARGET_PTX_4_2 (mtcs_ptx_version_option >= PTX_VERSION_4_2)
#define TARGET_PTX_5_0 (mtcs_ptx_version_option >= PTX_VERSION_5_0)
#define TARGET_PTX_6_0 (mtcs_ptx_version_option >= PTX_VERSION_6_0)
#define TARGET_PTX_6_3 (mtcs_ptx_version_option >= PTX_VERSION_6_3)
#define TARGET_PTX_7_0 (mtcs_ptx_version_option >= PTX_VERSION_7_0)
#define TARGET_PTX_7_3 (mtcs_ptx_version_option >= PTX_VERSION_7_3)
#define TARGET_PTX_7_8 (mtcs_ptx_version_option >= PTX_VERSION_7_8)




#define PTX_STORE_FLAG_VALUE 1 //原型 STORE_FLAG_VALUE
#define PTX_ACCUMULATE_OUTGOING_ARGS 1 //原型 ACCUMULATE_OUTGOING_ARGS


enum ptx_shuffle_kind
{
  PTX_SHUFFLE_UP,
  PTX_SHUFFLE_DOWN,
  PTX_SHUFFLE_BFLY,
  PTX_SHUFFLE_IDX,
  PTX_SHUFFLE_MAX
};


/* The various PTX memory areas an object might reside in.  */
enum ptx_data_area
{
  PTX_DATA_AREA_GENERIC,
  PTX_DATA_AREA_GLOBAL,
  PTX_DATA_AREA_SHARED,
  PTX_DATA_AREA_LOCAL,
  PTX_DATA_AREA_CONST,
  PTX_DATA_AREA_PARAM,
  PTX_DATA_AREA_MAX
};

/*  We record the data area in the target symbol flags.  */
#define PTX_SYMBOL_DATA_AREA(SYM) \
  (ptx_data_area)((SYMBOL_REF_FLAGS (SYM) >> SYMBOL_FLAG_MACH_DEP_SHIFT) \
            & 7)
#define PTX_SET_SYMBOL_DATA_AREA(SYM,AREA) \
  (SYMBOL_REF_FLAGS (SYM) |= (AREA) << SYMBOL_FLAG_MACH_DEP_SHIFT)

#define PTX_MASK_SOFT_STACK (1U << 2)

#define PTX_DEBUGGER_REGNO(N) N

/* Register Classes.  */
enum  ptx_reg_class             {  PTX_NO_REGS,    PTX_ALL_REGS, PTX_LIM_REG_CLASSES };
#define PTX_BASE_REG_CLASS PTX_ALL_REGS

#define PTX_LEGITIMATE_PIC_OPERAND_P(X) 1
#define PTX_INDEX_REG_CLASS (enum reg_class)PTX_NO_REGS
#define PTX_REGNO_OK_FOR_BASE_P(X) true
#define PTX_REGNO_OK_FOR_INDEX_P(X) false

#define PTX_MOVE_MAX 8
#define PTX_FUNCTION_MODE PTX_QImode
#define PTX_BRANCH_COST(speed_p, predictable_p) 6



#endif

