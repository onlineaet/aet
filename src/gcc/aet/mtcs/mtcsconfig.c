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
#include "config.h"
#include "expmed.h"
#include "expr.h"
#include "fold-const.h"
#include "dwarf2out.h"
#include "common/common-targhooks.h"
#include "math.h"

#include "aet/aetprinttree.h"
#include "mtcsconfig.h"

#define NODEFINE INT_MAX

void   mtcs_config_init(MtcsConfig *self)
{
   int i;
   for(i=0;i<MTCS_CONFIG_MAX;i++)
       self->configs[i]=NODEFINE;
}

typedef struct _Pair{
    MtcsConfigEnum value;
    char *name;
}Pair;

static Pair pairs[MTCS_CONFIG_MAX]={
   {MTCS_HAVE_COMDAT_GROUP            , "MTCS_HAVE_COMDAT_GROUP"},
   {MTCS_HAVE_isl                     , "MTCS_HAVE_isl"},
   {MTCS_HAVE_AS_LINE_ZERO            , "MTCS_HAVE_AS_LINE_ZERO"},//原型  HAVE_AS_LINE_ZERO auto-host.h
   {MTCS_HAVE_GAS_DISCRIMINATOR       , "MTCS_HAVE_GAS_DISCRIMINATOR"},//原型  HAVE_GAS_DISCRIMINATOR auto-host.h
   {MTCS_HAVE_AS_LEB128               , "MTCS_HAVE_AS_LEB128"},//原型 #define HAVE_AS_LEB128 0 auto-host.h
   //原型 #define HAVE_GAS_CFI_SECTIONS_DIRECTIVE 0 auto-host.h
   {MTCS_HAVE_GAS_CFI_SECTIONS_DIRECTIVE, "MTCS_HAVE_GAS_CFI_SECTIONS_DIRECTIVE"},
   //原型 #define HAVE_GAS_CFI_PERSONALITY_DIRECTIVE 0 auto-host.h
   {MTCS_HAVE_GAS_CFI_PERSONALITY_DIRECTIVE, "MTCS_HAVE_GAS_CFI_PERSONALITY_DIRECTIVE"},
   {MTCS_INSN_SCHEDULING              , "MTCS_INSN_SCHEDULING"},
   {MTCS_HAVE_FHARDENED_SUPPORT       , "MTCS_HAVE_FHARDENED_SUPPORT"},
   {MTCS_DWARF2_DEBUGGING_INFO        , "MTCS_DWARF2_DEBUGGING_INFO"},
   {MTCS_DWARF2_FRAME_INFO            , "MTCS_DWARF2_FRAME_INFO"},  //原型 DWARF2_FRAME_INFO 各平台定义 i386=nvptx=undefine
   {MTCS_CTF_DEBUGGING_INFO           , "MTCS_CTF_DEBUGGING_INFO"},
   {MTCS_BTF_DEBUGGING_INFO           , "MTCS_BTF_DEBUGGING_INFO"},
   {MTCS_VMS_DEBUGGING_INFO           , "MTCS_VMS_DEBUGGING_INFO"},
   {MTCS_DWARF2_LINENO_DEBUGGING_INFO , "MTCS_DWARF2_LINENO_DEBUGGING_INFO"},
   {MTCS_POERS_EXTEND_UNSIGNED        , "MTCS_POERS_EXTEND_UNSIGNED"},
   {MTCS_POINTERS_EXTEND_UNSIGNED     , "MTCS_POINTERS_EXTEND_UNSIGNED"}, //原型 #define POINTERS_EXTEND_UNSIGNED 1 i386=define 1, nvptx=undefine
   {MTCS_RED_ZONE_SIZE                , "MTCS_RED_ZONE_SIZE"},
   {MTCS_FLOAT_STORE_FLAG_VALUE       , "MTCS_FLOAT_STORE_FLAG_VALUE"},//原型 #define FLOAT_STORE_FLAG_VALUE nvptx定义 i386无
   {MTCS_AVOID_CCMODE_COPIES          , "MTCS_AVOID_CCMODE_COPIES"},//原型 #define AVOID_CCMODE_COPIES config/i386.h nvptx undefine
   {MTCS_STACK_REGS                   , "MTCS_STACK_REGS"},//原型 #define STACK_REGS config/i386.h nvptx undefine
   {MTCS_REG_ALLOC_ORDER              , "MTCS_REG_ALLOC_ORDER"},//原型 #define REG_ALLOC_ORDER config/i386.h nvptx undefine
   {MTCS_MEMORY_MOVE_COST             , "MTCS_MEMORY_MOVE_COST"},//原型 #define MEMORY_MOVE_COST host(i386) undefine nvptx undefine
   {MTCS_REGISTER_MOVE_COST           , "MTCS_MEMORY_MOVE_COST"},//原型 #define REGISTER_MOVE_COST host(i386) undefine nvptx undefine
   {MTCS_NUM_REGISTER_FILTERS         , "MTCS_NUM_REGISTER_FILTERS"},//原型 #define NUM_REGISTER_FILTERS host(i386) undefine nvptx = 0
   {MTCS_HARD_FRAME_POINTER_REGNUM    , "MTCS_HARD_FRAME_POINTER_REGNUM"},//原型 #define HARD_FRAME_POINTER_REGNUM BP_REG host(i386) undefine nvptx = 0
   {MTCS_CALL_REALLY_USED_REGISTERS   , "MTCS_CALL_REALLY_USED_REGISTERS"}, //原型 CALL_REALLY_USED_REGISTERS i386 nvptx都没定义
   {MTCS_SELECT_CC_MODE               , "MTCS_SELECT_CC_MODE"},             //原型 SELECT_CC_MODE i386 =1  nvptx=0
   {MTCS_NO_PROFILE_COUNTERS          , "MTCS_NO_PROFILE_COUNTERS"},//原型 NO_PROFILE_COUNTERS i386 =1  nvptx = 0
   {MTCS_ASSEMBLER_DIALECT            , "MTCS_ASSEMBLER_DIALECT"},//原型 #ifdef ASSEMBLER_DIALECT //host=1 nvptx=0
   {MTCS_PC_REGNUM                    , "MTCS_PC_REGNUM"},//原型 #define PC_REGNUM  //host=0 nvptx=0
   {MTCS_DWARF_ALT_FRAME_RETURN_COLUMN, "MTCS_DWARF_ALT_FRAME_RETURN_COLUMN"},//原型 #define DWARF_ALT_FRAME_RETURN_COLUMN  //host=0 nvptx=0
   //以下来自insn-config.h
   {MTCS_HAVE_conditional_move        ,"MTCS_HAVE_conditional_move"},//原型 HAVE_conditional_move insn-config.h
   {MTCS_HAVE_lo_sum                  ,"MTCS_HAVE_lo_sum"},//原型 HAVE_lo_sum insn-config.h
   {MTCS_HAVE_peephole                ,"MTCS_HAVE_peephole"},//原型 HAVE_peephole insn-config.h
   {MTCS_TARGET_SUPPORTS_WIDE_INT     ,"MTCS_TARGET_SUPPORTS_WIDE_INT"},//原型 #define TARGET_SUPPORTS_WIDE_INT host=1 nvptx=1 nvptx.h i386.h
   {MTCS_PUSH_ROUNDING                ,"MTCS_PUSH_ROUNDING"},//原型 PUSH_ROUNDING host=1 nvptx=0
   {MTCS_CODEVIEW_DEBUGGING_INFO      ,"MTCS_CODEVIEW_DEBUGGING_INFO"}//原型 MTCS_CODEVIEW_DEBUGGING_INFO host=1 nvptx=0
};

static char *getDefineStr(MtcsConfig *self,int value)
{
    if(value<0 || value>=MTCS_CONFIG_MAX){
        n_error("不是有效的配置选项值。最大:%d 当前值:%d",MTCS_CONFIG_MAX,value);
        return NULL;
    }
    return pairs[value].name;

}

static nboolean valid(int value)
{
   return value!=NODEFINE;
}

nboolean  mtcs_config_ifdef(MtcsConfig *self,MtcsConfigEnum value)
{
    if(value<0 || value>=MTCS_CONFIG_MAX){
        n_error("不是有效的配置选项值。最大:%d 当前值:%d",MTCS_CONFIG_MAX,value);
        return FALSE;
    }
    return valid(self->configs[value]);
}

nboolean  mtcs_config_ifndef(MtcsConfig *self,MtcsConfigEnum value)
{
    if(value<0 || value>=MTCS_CONFIG_MAX){
        n_error("不是有效的配置选项值。最大:%d 当前值:%d",MTCS_CONFIG_MAX,value);
        return FALSE;
    }
    return !valid(self->configs[value]);
}

//等同于 #if defined(xxx)
nboolean   mtcs_config_ifdefine(MtcsConfig *self,MtcsConfigEnum value)
{
   if(value<0 || value>=MTCS_CONFIG_MAX){
       n_error("不是有效的配置选项值。最大:%d 当前值:%d",MTCS_CONFIG_MAX,value);
       return FALSE;
   }
   return valid(self->configs[value]) && self->configs[value]!=0;
}


nboolean  mtcs_config_if(MtcsConfig *self,MtcsConfigEnum value)
{
    if(value<0 || value>=MTCS_CONFIG_MAX){
       n_error("不是有效的配置选项值。最大:%d 当前值:%d",MTCS_CONFIG_MAX,value);
       return FALSE;
    }
    if(!valid(self->configs[value])){
        n_debug("选项:%s 在当前平台没有定义。",getDefineStr(self,value));
        return FALSE;
    }
    return self->configs[value]!=0;
}


void mtcs_config_undefine(MtcsConfig *self,MtcsConfigEnum value)
{
    if(value<0 || value>=MTCS_CONFIG_MAX){
          n_error("不是有效的配置选项值。最大:%d 当前值:%d",MTCS_CONFIG_MAX,value);
          return ;
    }
    self->configs[value]=NODEFINE;
}

void      mtcs_config_define_with_value(MtcsConfig *self,MtcsConfigEnum config,int value)
{
     if(config<0 || config>=MTCS_CONFIG_MAX){
            n_error("不是有效的配置选项值。最大:%d 当前值:%d",MTCS_CONFIG_MAX,value);
            return ;
      }
      self->configs[config]=value;
}

int mtcs_config_get_value(MtcsConfig *self,MtcsConfigEnum config)
{
    if(config<0 || config>=MTCS_CONFIG_MAX){
        n_error("不是有效的配置选项值。最大:%d 当前值:%d",MTCS_CONFIG_MAX,config);
        return 0;
    }

    if(!valid(self->configs[config])){
        n_error("选项:%s 在当前平台没有定义。",getDefineStr(self,config));
        return 0;
    }
    return  self->configs[config];
}

void mtcs_config_define(MtcsConfig *self,MtcsConfigEnum config)
{
    mtcs_config_define_with_value(self,config,0);
}

