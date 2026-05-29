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

#include "ptx-common.h"
#include "mtcsptxmode.h"
#include "mtcsptxconfig.h"
#include "mtcsptxcodes.h"

static void mtcsPtxConfigInit(MtcsPtxConfig *self)
{
    MtcsConfig *mtcsConfig=(MtcsConfig *)self;
    mtcs_config_define(mtcsConfig,MTCS_HAVE_COMDAT_GROUP); //原型  HAVE_COMDAT_GROUP auto-host.h
    mtcs_config_define(mtcsConfig,MTCS_HAVE_FHARDENED_SUPPORT);//原型  HAVE_COMDAT_GROUP auto-host.h
    mtcs_config_define_with_value(mtcsConfig,MTCS_HAVE_isl,1);//原型  HAVE_isl auto-host.h
    mtcs_config_define_with_value(mtcsConfig,MTCS_DWARF2_LINENO_DEBUGGING_INFO,1);//原型 DWARF2_LINENO_DEBUGGING_INFO nvptx.h
    mtcs_config_define(mtcsConfig,MTCS_FLOAT_STORE_FLAG_VALUE);//原型 #define FLOAT_STORE_FLAG_VALUE nvptx定义 i386无
    mtcs_config_define_with_value(mtcsConfig,MTCS_HAVE_AS_LEB128,0);//原型 #define HAVE_AS_LEB128 0 auto-host.h
    //原型 #define HAVE_GAS_CFI_SECTIONS_DIRECTIVE 0 auto-host.h
    mtcs_config_define_with_value(mtcsConfig,MTCS_HAVE_GAS_CFI_SECTIONS_DIRECTIVE,0);
    //原型 #define HAVE_GAS_CFI_PERSONALITY_DIRECTIVE 0 auto-host.h
    mtcs_config_define_with_value(mtcsConfig,MTCS_HAVE_GAS_CFI_PERSONALITY_DIRECTIVE,0);
    //以下来自insn-config.h
    mtcs_config_define_with_value(mtcsConfig,MTCS_HAVE_conditional_move,1);//原型 HAVE_conditional_move insn-config.h
    mtcs_config_define_with_value(mtcsConfig,MTCS_HAVE_lo_sum,0);//原型 HAVE_lo_sum insn-config.h
    mtcs_config_define_with_value(mtcsConfig,MTCS_HAVE_peephole,0);//原型 HAVE_peephole insn-config.h
    //原型 #define TARGET_SUPPORTS_WIDE_INT host=1 nvptx=1 nvptx.h i386.h
    mtcs_config_define_with_value(mtcsConfig,MTCS_TARGET_SUPPORTS_WIDE_INT,1);
}

MtcsPtxConfig *mtcs_ptx_config_new()
{
     MtcsPtxConfig *self = n_slice_alloc0 (sizeof(MtcsPtxConfig));
     mtcs_config_init((MtcsConfig *)self);
     mtcsPtxConfigInit(self);
     return self;
}


