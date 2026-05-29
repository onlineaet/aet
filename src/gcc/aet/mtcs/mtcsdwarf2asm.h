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

#ifndef __GCC_MTCS_DWARF2_ASM__
#define __GCC_MTCS_DWARF2_ASM__

#include "../nlib.h"
#include "dwarf2out.h"
#include "mtcscomponent.h"


typedef struct _MtcsDwarf2Asm MtcsDwarf2Asm;


struct _MtcsDwarf2Asm
{
   MtcsComponent parent;
   GTY(()) bool have_multiple_function_sections;
   GTY(()) bool in_text_section_p;
   GTY(()) hash_map<const char *, tree> *indirect_pool;
   GTY(()) int dw2_const_labelno;
   GTY(()) const char *last_text_label;
   GTY(()) const char *last_cold_label;
   GTY(()) vec<const char *, va_gc> *switch_text_ranges;
   GTY(()) vec<const char *, va_gc> *switch_cold_ranges;
};



MtcsDwarf2Asm *mtcs_dwarf2_asm_new(MtcsMode *mtcsMode);

//原型 dw2_asm_output_data_uleb128_raw dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_data_uleb128_raw (MtcsDwarf2Asm *self,unsigned HOST_WIDE_INT value);
//原型 dw2_asm_output_data_raw dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_data_raw (MtcsDwarf2Asm *self,int size, unsigned HOST_WIDE_INT value);
//原型 dw2_asm_output_data_sleb128 dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_data_sleb128 (MtcsDwarf2Asm *self,HOST_WIDE_INT value, const char *comment, ...);
//原型 dw2_asm_output_data_sleb128_raw dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_data_sleb128_raw (MtcsDwarf2Asm *self,HOST_WIDE_INT value);
//原型 dw2_asm_output_delta_uleb128 dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_delta_uleb128 (MtcsDwarf2Asm *self,const char *lab1 ATTRIBUTE_UNUSED,
                  const char *lab2 ATTRIBUTE_UNUSED, const char *comment, ...);
//原型 dw2_asm_output_data_uleb128 dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_data_uleb128 (MtcsDwarf2Asm *self,unsigned HOST_WIDE_INT value,const char *comment, ...);
//原型 dw2_asm_output_delta dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_delta (MtcsDwarf2Asm *self,int size, const char *lab1, const char *lab2,const char *comment, ...);
//原型 dw2_force_const_mem dearf2asm.h dwarf2asm.cc
rtx  mtcs_dwarf2_asm_force_const_mem (MtcsDwarf2Asm *self,rtx x, bool is_public);
void mtcs_dwarf2_asm_mark_ignored_debug_section (MtcsDwarf2Asm *self,dw_fde_ref fde, bool second);
//原型 dw2_asm_output_data dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_data (MtcsDwarf2Asm *self,int size, unsigned HOST_WIDE_INT value, const char *comment, ...);
void mtcs_dwarf2_asm_assemble_integer (MtcsDwarf2Asm *self,int size, rtx x);
//原型 dw2_asm_output_encoded_addr_rtx dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_encoded_addr_rtx (MtcsDwarf2Asm *self,int encoding, rtx addr, bool is_public, const char *comment, ...);
//原型 dw2_asm_output_offset dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_offset (MtcsDwarf2Asm *self,int size, const char *label,
             section *base ATTRIBUTE_UNUSED, const char *comment, ...);
void mtcs_dwarf2_asm_output_offset (MtcsDwarf2Asm *self,int size, const char *label, HOST_WIDE_INT offset,
             section *base ATTRIBUTE_UNUSED,const char *comment, ...);
//原型 dw2_asm_output_addr dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_addr (MtcsDwarf2Asm *self,int size, const char *label, const char *comment, ...);
//原型 dw2_asm_output_addr_rtx dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_addr_rtx (MtcsDwarf2Asm *self,int size, rtx addr, const char *comment, ...);
//原型 dw2_asm_output_vms_delta dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_vms_delta (MtcsDwarf2Asm *self,int size ATTRIBUTE_UNUSED,
           const char *lab1, const char *lab2, const char *comment, ...);
//原型 dw2_asm_output_symname_uleb128 dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_symname_uleb128 (MtcsDwarf2Asm *self,const char *lab1 ATTRIBUTE_UNUSED, const char *comment, ...);
//原型 dw2_asm_output_nstring dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_nstring (MtcsDwarf2Asm *self,const char *str, size_t orig_len,const char *comment, ...);

#endif
