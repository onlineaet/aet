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

#ifndef __GCC_MTCS_OUTPUT__
#define __GCC_MTCS_OUTPUT__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "output.h"

#define MTCS_GEN_FCN(CODE) (mtcsTarget->mtcsOutput->insn_data[CODE].genfun)

typedef struct _MtcsOutput MtcsOutput;


struct _MtcsOutput
{
   MtcsComponent parent;
   int insn_counter;
   const rtx_insn *this_is_asm_operands;
   unsigned int insn_noperands;
   rtx_insn *debug_insn;
   rtx current_insn_predicate;//原型 rtx current_insn_predicate output.h
   rtx_insn *current_output_insn;

   struct insn_data_d *insn_data;
   int count;
   //原型 get_insn_name rtl.h insn-output.cc
   const char *(*get_insn_name)(MtcsOutput *self,int code);
   //原型 targetm.section_type_flags  #define TARGET_SECTION_TYPE_FLAGS default_section_type_flags
   unsigned int (*section_type_flags) (MtcsOutput *self,tree decl, const char *name, int reloc);
   //原型  targetm.encode_section_info (exp, rtl, true); #define TARGET_ENCODE_SECTION_INFO default_encode_section_info
   //nvptx有实现
   void (*encode_section_info) (MtcsOutput *self,tree decl, rtx rtl, int first ATTRIBUTE_UNUSED);
   /* User label prefix in effect for this compilation.  */
   //原型 user_label_prefix output.h toplev.cc
   char *user_label_prefix;
   void (*output_debug_file)(MtcsOutput *self,int emitted_number,char *fileName);

};


void     mtcs_output_init(MtcsOutput *self);
void mtcs_output_operand_lossage (MtcsOutput *self,const char *cmsgid, ...);
//原型 output_asm_insn oput.h final.cc
void mtcs_output_asm_insn (MtcsOutput *self,const char *templ, rtx *operands);
void mtcs_output_asm_label (MtcsOutput *self,rtx x);
//原型 output_address output.h final.cc
void mtcs_output_address (MtcsOutput *self,machine_mode mode, rtx x);
//原型 output_addr_const output.h final.cc
void mtcs_output_addr_const (MtcsOutput *self, rtx x);
void     mtcs_output_add_insn_count(MtcsOutput *self,int count);

void   mtcs_output_set_insn_data(MtcsOutput *self,struct insn_data_d *insn_data,int count);
//原型 get_insn_name rtl.h insn-output.cc
const char *mtcs_output_get_insn_name(MtcsOutput *self,int code);


char mtcs_output_get_n_generator_args(MtcsOutput *self,int code);
char mtcs_output_get_n_operands(MtcsOutput *self,int code);
char mtcs_output_get_n_dups(MtcsOutput *self,int code);
char mtcs_output_get_n_alternatives(MtcsOutput *self,int code);
char mtcs_output_get_output_format(MtcsOutput *self,int code);

//md中需要的方法
/*insn_data 还调用output_asm_insn 定义在final.cc
 * output_asm_insn 如果加mtcs_ 与mtcsoutput.h中声明的方法mtcs_ouput_asm_insn冲突。改为加nvptx_
 * 原型是output_asm_insn
 */
void        md_output_asm_insn (const char *templ, rtx *operands);

void mtcs_output_set_user_label_prefix(MtcsOutput *self,char *label);

//原型 decode_reg_name output.h varasm.cc
int mtcs_output_decode_reg_name (MtcsOutput *self,const char *name);
//原型 decode_reg_name_and_count output.h varasm.cc
int mtcs_output_decode_reg_name_and_count (MtcsOutput *self,const char *asmspec, int *pnregs);
//原型 get_insn_template output.h final.cc
const char * mtcs_output_get_insn_template (MtcsOutput *self,int code, rtx_insn *insn);
//原型 leaf_function_p output.h final.cc
bool mtcs_output_leaf_function_p (MtcsOutput *self);
//原型 alter_subreg output.h final.cc
rtx mtcs_output_alter_subreg (MtcsOutput *self,rtx *xp, bool final_p);
//原型 cleanup_subreg_operands reload.h final.cc
void mtcs_output_cleanup_subreg_operands (MtcsOutput *self,rtx_insn *insn);
//原型 mark_symbol_refs_as_used output.h final.cc
void mtcs_output_mark_symbol_refs_as_used (MtcsOutput *self,rtx x);
//原型 output_operand output.h final.cc
void mtcs_output_output_operand (MtcsOutput *self,rtx x, int code ATTRIBUTE_UNUSED);
//原型 integer_asm_op output.h varasm.cc
const char *mtcs_output_integer_asm_op (MtcsOutput *self,int size, int aligned_p);
//原型 categorize_decl_for_section output.h varasm.cc
enum section_category mtcs_output_categorize_decl_for_section (MtcsOutput *self,const_tree decl, int reloc);
//原型 default_section_type_flags output.h varasm.cc
unsigned int mtcs_output_default_section_type_flags (MtcsOutput *self,tree decl, const char *name, int reloc);
//替换 targetm.section_type_flags
//原型 targetm.section_type_flags  #define TARGET_SECTION_TYPE_FLAGS default_section_type_flags
unsigned int mtcs_output_section_type_flags (MtcsOutput *self,tree decl, const char *name, int reloc);
//原型 default_encode_section_info output.h varasm.cc
void mtcs_output_default_encode_section_info(MtcsOutput *self,tree decl, rtx rtl, int first ATTRIBUTE_UNUSED);
//替换 targetm.encode_section_info nvptx有实现
//原型  targetm.encode_section_info (exp, rtl, true); #define TARGET_ENCODE_SECTION_INFO default_encode_section_info
void mtcs_output_encode_section_info (MtcsOutput *self,tree decl, rtx rtl, int first ATTRIBUTE_UNUSED);
//解决 bug 082
void mtcs_output_debug_file(MtcsOutput *self,int emitted_number,char *fileName);

#endif
