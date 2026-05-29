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

#ifndef __GCC_MTCS_CONFIG__
#define __GCC_MTCS_CONFIG__

#include "../nlib.h"

typedef struct _MtcsConfig MtcsConfig;

typedef enum MtcsConfigEnum{
     MTCS_HAVE_COMDAT_GROUP,//原型  HAVE_COMDAT_GROUP auto-host.h
     MTCS_HAVE_isl,//原型  HAVE_isl auto-host.h
     MTCS_HAVE_AS_LINE_ZERO, //原型  HAVE_AS_LINE_ZERO auto-host.h
     MTCS_HAVE_GAS_DISCRIMINATOR, //原型  HAVE_GAS_DISCRIMINATOR auto-host.h
     MTCS_HAVE_AS_LEB128,//原型 #define HAVE_AS_LEB128 0 auto-host.h
     MTCS_HAVE_GAS_CFI_SECTIONS_DIRECTIVE,//原型 #define HAVE_GAS_CFI_SECTIONS_DIRECTIVE 0 auto-host.h
     MTCS_HAVE_GAS_CFI_PERSONALITY_DIRECTIVE,//原型 #define HAVE_GAS_CFI_PERSONALITY_DIRECTIVE 0 auto-host.h
     MTCS_INSN_SCHEDULING, //原型 INSN_SCHEDULING insn-attr-common.h
     MTCS_HAVE_FHARDENED_SUPPORT,//原型  HAVE_COMDAT_GROUP auto-host.h
     MTCS_DWARF2_DEBUGGING_INFO,//原型 DWARF2_DEBUGGING_INFO darwin.h
     MTCS_DWARF2_FRAME_INFO,    //原型 DWARF2_FRAME_INFO 各平台定义 i386=nvptx=undefine
     MTCS_CTF_DEBUGGING_INFO,//原型 CTF_DEBUGGING_INFO elfos.h
     MTCS_BTF_DEBUGGING_INFO,//原型 BTF_DEBUGGING_INFO elfos.h
     MTCS_VMS_DEBUGGING_INFO,//原型 VMS_DEBUGGING_INFO vms.h
     MTCS_DWARF2_LINENO_DEBUGGING_INFO,//原型 DWARF2_LINENO_DEBUGGING_INFO nvptx.h
     MTCS_POERS_EXTEND_UNSIGNED, //原型 POERS_EXTEND_UNSIGNED 每个平台定义 i386 =1
     MTCS_POINTERS_EXTEND_UNSIGNED,//原型 #define POINTERS_EXTEND_UNSIGNED 1 i386=define 1, nvptx=undefine
     MTCS_RED_ZONE_SIZE,//原型 #define RED_ZONE_SIZE 128 i386.h ,nvptx=undefine;
     MTCS_FLOAT_STORE_FLAG_VALUE,//原型 #define FLOAT_STORE_FLAG_VALUE nvptx定义 i386无
     MTCS_AVOID_CCMODE_COPIES, //原型 #define AVOID_CCMODE_COPIES config/i386.h nvptx undefine
     MTCS_STACK_REGS, //原型 #define STACK_REGS config/i386.h nvptx undefine
     MTCS_REG_ALLOC_ORDER,//原型 #define REG_ALLOC_ORDER config/i386.h nvptx undefine
     MTCS_MEMORY_MOVE_COST ,//原型 #define MEMORY_MOVE_COST host(i386) undefine nvptx undefine
     MTCS_REGISTER_MOVE_COST,//原型 #define REGISTER_MOVE_COST host(i386) undefine nvptx undefine
     MTCS_NUM_REGISTER_FILTERS,//原型 #define NUM_REGISTER_FILTERS host(i386) undefine nvptx = 0
     MTCS_HARD_FRAME_POINTER_REGNUM, //原型 #define HARD_FRAME_POINTER_REGNUM BP_REG host(i386) undefine nvptx = 0
     MTCS_CALL_REALLY_USED_REGISTERS,//原型 CALL_REALLY_USED_REGISTERS i386 nvptx都没定义
     MTCS_SELECT_CC_MODE,//原型 SELECT_CC_MODE i386 =1  nvptx = 0
     MTCS_NO_PROFILE_COUNTERS,//原型 NO_PROFILE_COUNTERS i386 =1  nvptx = 0
     MTCS_ASSEMBLER_DIALECT,//原型 #ifdef ASSEMBLER_DIALECT //host=1 nvptx=0
     MTCS_PC_REGNUM,//原型 #define PC_REGNUM  //host=0 nvptx=0
     MTCS_DWARF_ALT_FRAME_RETURN_COLUMN, //原型 #define DWARF_ALT_FRAME_RETURN_COLUMN  //host=0 nvptx=0
     //以下来自insn-config.h
     MTCS_HAVE_conditional_move,//原型 HAVE_conditional_move insn-config.h
     MTCS_HAVE_lo_sum,//原型 HAVE_lo_sum insn-config.h
     MTCS_HAVE_peephole,//原型 HAVE_peephole insn-config.h
     MTCS_TARGET_SUPPORTS_WIDE_INT,//原型 #define TARGET_SUPPORTS_WIDE_INT host=1 nvptx=1
     MTCS_PUSH_ROUNDING,//原型 PUSH_ROUNDING host=1 nvptx=0
     MTCS_CODEVIEW_DEBUGGING_INFO,//原型 MTCS_CODEVIEW_DEBUGGING_INFO host=1 nvptx=0
     MTCS_CONFIG_MAX
};


struct _MtcsConfig
{
    int configs[MTCS_CONFIG_MAX];
};

void      mtcs_config_init(MtcsConfig *self);
nboolean  mtcs_config_ifdef(MtcsConfig *self,MtcsConfigEnum define);
nboolean  mtcs_config_ifndef(MtcsConfig *self,MtcsConfigEnum value);
nboolean  mtcs_config_if(MtcsConfig *self,MtcsConfigEnum value);
void      mtcs_config_undefine(MtcsConfig *self,MtcsConfigEnum value);
void      mtcs_config_define(MtcsConfig *self,MtcsConfigEnum config);
void      mtcs_config_define_with_value(MtcsConfig *self,MtcsConfigEnum config,int value);
int       mtcs_config_get_value(MtcsConfig *self,MtcsConfigEnum config);
//等同于 #if defined(xxx)
nboolean   mtcs_config_ifdefine(MtcsConfig *self,MtcsConfigEnum value);









#endif

