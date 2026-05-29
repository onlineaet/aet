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

#ifndef __GCC_TARGET_ASM_OUT__
#define __GCC_TARGET_ASM_OUT__

#include "../../nlib.h"
#include "machinetarget.h"

typedef struct _TargetAsmOut TargetAsmOut;
struct _TargetAsmOut
{
   MachineTarget parent;

   //原型  targetm.asm_out.file_start ();#define TARGET_ASM_FILE_START nvptx_file_start
   void (* file_start)(TargetAsmOut *self);
   //原型targetm.asm_out.print_operand #define TARGET_PRINT_OPERAND
   //default_print_operand(FILE *stream ATTRIBUTE_UNUSED, rtx x ATTRIBUTE_UNUSED,int code ATTRIBUTE_UNUSED);
   void (*print_operand)(TargetAsmOut *self,rtx x ATTRIBUTE_UNUSED,int code ATTRIBUTE_UNUSED);
   //原型targetm.asm_out.print_operand_address #define TARGET_PRINT_OPERAND_ADDRESS
   //void default_print_operand_address (FILE *stream ATTRIBUTE_UNUSED, machine_mode /*mode*/,rtx x ATTRIBUTE_UNUSED)
   void (*print_operand_address) (TargetAsmOut *self,machine_mode mode,  rtx x ATTRIBUTE_UNUSED);
   //原型 targetm.asm_out.init_sections (); #define TARGET_ASM_INIT_SECTIONS hook_void_void
   void (*init_sections)(TargetAsmOut *self);
   //原型 targetm.asm_out.output_anchor#define TARGET_ASM_OUTPUT_ANCHOR default_asm_output_anchor
   void (*output_anchor)(TargetAsmOut *self,rtx symbol);
   //原型 targetm.asm_out.generate_pic_addr_diff_vec  #define TARGET_ASM_GENERATE_PIC_ADDR_DIFF_VEC default_generate_pic_addr_diff_vec
   bool (*generate_pic_addr_diff_vec)(TargetAsmOut *self);
   //原型 targetm.asm_out.declare_constant_name (asm_out_file, label, exp, size);
   //#define TARGET_ASM_DECLARE_CONSTANT_NAME default_asm_declare_constant_name
   //asm_out_file由MtcsAsm定义，不需要作为参数传给declare_constant_name
   void (*declare_constant_name) (TargetAsmOut *self,const char *name,const_tree exp ATTRIBUTE_UNUSED, HOST_WIDE_INT size ATTRIBUTE_UNUSED);
   //原型 TARGET_ASM_DECL_END 无缺省实现
   void (*decl_end)(TargetAsmOut *self);
   //原型targetm.asm_out.globalize_label (file, name); #define TARGET_ASM_GLOBALIZE_LABEL default_globalize_label
   void (*globalize_label)(TargetAsmOut *self,const char *name);
   //原型  targetm.asm_out.internal_label 和#define TARGET_ASM_INTERNAL_LABEL default_internal_label
   void (*internal_label)(TargetAsmOut *self,const char *prefix, unsigned long labelno);
   //原型 targetm.asm_out.constructor #define TARGET_ASM_CONSTRUCTOR aarch64_elf_asm_constructor
   void (*constructor)(TargetAsmOut *self,rtx symbol, int priority);
   //原型 targetm.asm_out.destructor #define TARGET_ASM_CONSTRUCTOR aarch64_elf_asm_constructor
   void (*destructor)(TargetAsmOut *self,rtx symbol, int priority);
   //原型  targetm.asm_out.should_restore_cfa_state () #define TARGET_ASM_SHOULD_RESTORE_CFA_STATE hook_bool_void_false
   bool (*should_restore_cfa_state)(TargetAsmOut *self);
   //原型  targetm.asm_out.print_operand_punct_valid_p ((unsigned char) *p) #define TARGET_PRINT_OPERAND_PUNCT_VALID_P nvptx_print_operand_punct_valid_p
   bool (*print_operand_punct_valid_p)(TargetAsmOut *self,unsigned char c);
   //原型 targetm.asm_out.file_end (); #define TARGET_ASM_FILE_END nvptx_file_end
   void (*file_end)(TargetAsmOut *self);
   //原型 targetm.asm_out.print_patchable_function_entry TARGET_ASM_PRINT_PATCHABLE_FUNCTION_ENTRY
   void (*print_patchable_function_entry)(TargetAsmOut *self,unsigned HOST_WIDE_INT patch_area_size,bool record_p);
   //原型 targetm.asm_out.assemble_undefined_decl (asm_out_file, name, decl); #define TARGET_ASM_ASSEMBLE_UNDEFINED_DECL nvptx_assemble_undefined_decl
   void (*assemble_undefined_decl) (TargetAsmOut *self,const char *name, const_tree decl);
   //原型 targetm.asm_out.integer#define TARGET_ASM_INTEGER nvptx_assemble_integer
   bool (*integer) (TargetAsmOut *self,rtx x ATTRIBUTE_UNUSED,unsigned int size ATTRIBUTE_UNUSED,int aligned_p ATTRIBUTE_UNUSED);
   //原型  targetm.asm_out.emit_unwind_label  #define TARGET_ASM_EMIT_UNWIND_LABEL default_emit_unwind_label
   void (*emit_unwind_label)(TargetAsmOut *self, tree decl, int for_eh,int empty );
   //原型  targetm.asm_out.post_cfi_startproc  #define TARGET_ASM_POST_CFI_STARTPROC hook_void_FILEptr_tree
   void (*post_cfi_startproc)(TargetAsmOut *self, tree decl);
   //原型 targetm.asm_out.make_eh_symbol_indirect #define TARGET_ASM_MAKE_EH_SYMBOL_INDIRECT darwin_make_eh_symbol_indirect
   rtx (*make_eh_symbol_indirect) (TargetAsmOut *self,rtx orig, bool ARG_UNUSED (pubvis));
   //原型 targetm.asm_out.output_dwarf_dtprel  #define TARGET_ASM_OUTPUT_DWARF_DTPREL NULL
   void (*output_dwarf_dtprel) (TargetAsmOut *self,int size, rtx x);
   //原型targetm.asm_out.assemble_visibility (decl, vis);#define TARGET_ASM_ASSEMBLE_VISIBILITY default_assemble_visibility
   void (*assemble_visibility)(TargetAsmOut *self,tree decl ATTRIBUTE_UNUSED,int vis ATTRIBUTE_UNUSED);
   //原型 targetm.asm_out.named_section TARGET_ASM_NAMED_SECTION
   void (*named_section)(TargetAsmOut *self,const char *name ATTRIBUTE_UNUSED,unsigned int flags ATTRIBUTE_UNUSED, tree decl ATTRIBUTE_UNUSED);
   //原型 targetm.asm_out.final_postscan_insn #define TARGET_ASM_FINAL_POSTSCAN_INSN NULL i386 nvptx是空的
   void (*final_postscan_insn) (TargetAsmOut *self, rtx_insn *insn, rtx *, int);
   //原型 targetm.asm_out.aligned_op TARGET_ASM_ALIGNED_INT_OP
   struct asm_int_op aligned_op;
   //原型 targetm.asm_out.unaligned_op TARGET_ASM_UNALIGNED_INT_OP
   struct asm_int_op unaligned_op;
   //原型 targetm.asm_out.byte_op TARGET_ASM_BYTE_OP "\t.byte\t"
   char *byte_op;

   //下面是声明在MtcsTarget中的方法，
   //原型  targetm.asm_out.emit_except_table_label (asm_out_file);和#define TARGET_ASM_EMIT_EXCEPT_TABLE_LABEL default_emit_except_table_label 有缺省实现
   void (*emit_except_table_label)(TargetAsmOut *self);
   //原型targetm.asm_out.unwind_emit #define TARGET_ASM_UNWIND_EMIT  nvptx没有实现
   void (*unwind_emit)(TargetAsmOut *self,rtx_insn *insn);
   //原型targetm.asm_out.output_addr_const_extra TARGET_ASM_OUTPUT_ADDR_CONST_EXTRA
   bool (*output_addr_const_extra) (TargetAsmOut *self, rtx x);
   //原型 targetm.asm_out.function_switched_text_sections #define TARGET_ASM_FUNCTION_SWITCHED_TEXT_SECTIONS default_function_switched_text_sections
   void (*function_switched_text_sections) (TargetAsmOut *self,tree decl ATTRIBUTE_UNUSED, bool new_is_cold ATTRIBUTE_UNUSED);
   //原型 targetm.asm_out.function_end_prologue (file); TARGET_ASM_FUNCTION_END_PROLOGUE
   void (*function_end_prologue)(TargetAsmOut *self);
   //原型 targetm.asm_out.function_begin_epilogue (file); #define TARGET_ASM_FUNCTION_BEGIN_EPILOGUE nds32_asm_function_begin_epilogue
   void (*function_begin_epilogue)(TargetAsmOut *self);
   //原型 targetm.asm_out.function_rodata_section #define TARGET_ASM_FUNCTION_RODATA_SECTION default_function_rodata_section
   section *(*function_rodata_section)(TargetAsmOut *self,tree decl, bool relocatable);
   //原型 targetm.asm_out.function_section #define TARGET_ASM_FUNCTION_SECTION default_function_section
   section *(*function_section)(TargetAsmOut *self,tree decl, enum node_frequency freq,bool startup, bool exit);
   //原型 targetm.asm_out.reloc_rw_mask   #define TARGET_ASM_RELOC_RW_MASK default_reloc_rw_mask
   int (*reloc_rw_mask) (TargetAsmOut *self);
   //原型 targetm.asm_out.function_prologue #define TARGET_ASM_FUNCTION_PROLOGUE default_function_pro_epilogue
   void (*function_prologue)(TargetAsmOut *self);
   //原型 targetm.asm_out.function_epilogue (asm_out_file); #define TARGET_ASM_FUNCTION_EPILOGUE default_function_pro_epilogue
   void (*function_epilogue)(TargetAsmOut *self);
   //原型 targetm.asm_out.ttype  #define TARGET_ASM_TTYPE hook_bool_rtx_false
   bool (*ttype)(TargetAsmOut *self,rtx x);
   //原型 targetm.asm_out.select_rtx_section(desc->mode, desc->constant, desc->align) #define TARGET_ASM_SELECT_RTX_SECTION default_select_rtx_section
   section * (*select_rtx_section) (TargetAsmOut *self,machine_mode mode ATTRIBUTE_UNUSED,rtx x,unsigned HOST_WIDE_INT align ATTRIBUTE_UNUSED);
   //原型targetm.asm_out.select_section (exp,mtcs_compute_reloc_for_constant (exp),align); #define TARGET_ASM_SELECT_SECTION default_select_section
   section * (*select_section) (TargetAsmOut *self,tree decl, int reloc,unsigned HOST_WIDE_INT align ATTRIBUTE_UNUSED);
   //原型 targetm.asm_out.globalize_decl_name (asm_out_file, decl);#define TARGET_ASM_GLOBALIZE_DECL_NAME default_globalize_decl_name
   void (*globalize_decl_name)(TargetAsmOut *self,tree decl);
   //原型 targetm.asm_out.unique_section (decl, reloc);#define TARGET_ASM_UNIQUE_SECTION default_unique_section
   void (*unique_section)(TargetAsmOut *self,tree decl, int reloc);

   //gcc 旧实现采用的宏，原来声明在MtcsTarget，现在移到 TargetAsmOut
   //旧实现中用到宏的有
   //1.ASM_OUTPUT_SKIP 2.ASM_OUTPUT_ALIGNED_DECL_COMMON 3.ASM_DECLARE_OBJECT_NAME 4.ASM_OUTPUT_ALIGNED_DECL_LOCAL
   //5.ASM_OUTPUT_DEBUG_LABEL 6.ASM_OUTPUT_LABELREF 7.ASM_OUTPUT_DEF 8.ASM_OUTPUT_ASCII
   //原型 #define ASM_OUTPUT_SKIP(FILE, N) nvptx有实现
   void (*output_skip)(TargetAsmOut *self,unsigned HOST_WIDE_INT size);
   //原型 #define ASM_OUTPUT_ALIGNED_DECL_COMMON(FILE, DECL, NAME, SIZE, ALIGN)   nvptx有实现
   void (*output_aligned_decl_common)(TargetAsmOut *self,const_tree decl, const char *name, HOST_WIDE_INT size, unsigned align);
   //原型 #define ASM_DECLARE_OBJECT_NAME(FILE, NAME, DECL)  nvptx_declare_object_name (FILE, NAME, DECL)
   void (*declare_object_name)(TargetAsmOut *self, const char *name, const_tree decl);
   //原型 #define ASM_OUTPUT_ALIGNED_DECL_LOCAL(FILE, DECL, NAME, SIZE, ALIGN)  nvptx_output_aligned_decl (FILE, NAME, DECL, SIZE, ALIGN)
   void (*output_aligned_decl_local)(TargetAsmOut *self,const_tree decl,const char *name, HOST_WIDE_INT size, unsigned align);
   //原型 ASM_OUTPUT_DEBUG_LABEL
   void (*output_debug_label)(TargetAsmOut *self,const char *prefix,int num);
   //ASM_OUTPUT_LABELREF  amd的gcn实现不一样 有缺省实现
   void (* output_labelref)(TargetAsmOut *self,char *name);
   //原型 #define ASM_OUTPUT_DEF(FILE,LABEL1,LABEL2)
   void (*output_def) (TargetAsmOut *self,const char *label1,const char *label2);
   //原型 #define ASM_OUTPUT_ASCII(FILE, STR, LENGTH) nvptx有实现
   void (*output_ascii)(TargetAsmOut *self,const char *str, unsigned HOST_WIDE_INT size);

   //原型 targetm.asm_out.emit_except_personality TARGET_ASM_EMIT_EXCEPT_PERSONALITY nvptx host都是空的 没有实现
   void (*emit_except_personality)(TargetAsmOut *self,rtx personality);
   //实现ASM_DECLARE_FUNCTION_NAME(FILE, NAME, DECL)
   void (*declare_function_name) (TargetAsmOut *self, const char *name, const_tree decl);
   //TARGET_ASM_MARK_DECL_PRESERVED gcc中的实现是hook_void_constcharptr 声明在gcc/targetdef
   void (* mark_decl_preserved)(TargetAsmOut *self,const char *name);
};

void  target_asm_out_init(TargetAsmOut *self);
//查找 替换 asmOut.file_start
//原型  targetm.asm_out.file_start ();#define TARGET_ASM_FILE_START nvptx_file_start
void target_asm_out_file_start (TargetAsmOut *self);
//原型targetm.asm_out.print_operand #define TARGET_PRINT_OPERAND
//default_print_operand(FILE *stream ATTRIBUTE_UNUSED, rtx x ATTRIBUTE_UNUSED,int code ATTRIBUTE_UNUSED);
void target_asm_out_print_operand (TargetAsmOut *self,rtx x ATTRIBUTE_UNUSED,int code ATTRIBUTE_UNUSED);
//原型targetm.asm_out.print_operand_address #define TARGET_PRINT_OPERAND_ADDRESS
//void default_print_operand_address (FILE *stream ATTRIBUTE_UNUSED, machine_mode /*mode*/,rtx x ATTRIBUTE_UNUSED)
void target_asm_out_print_operand_address (TargetAsmOut *self,machine_mode mode,  rtx x ATTRIBUTE_UNUSED);
//原型 targetm.asm_out.init_sections (); #define TARGET_ASM_INIT_SECTIONS hook_void_void
void target_asm_out_init_sections (TargetAsmOut *self);
//原型 targetm.asm_out.output_anchor#define TARGET_ASM_OUTPUT_ANCHOR default_asm_output_anchor
void target_asm_out_output_anchor (TargetAsmOut *self,rtx symbol);
//原型 targetm.asm_out.generate_pic_addr_diff_vec  #define TARGET_ASM_GENERATE_PIC_ADDR_DIFF_VEC default_generate_pic_addr_diff_vec
bool target_asm_out_generate_pic_addr_diff_vec (TargetAsmOut *self);
//原型 targetm.asm_out.declare_constant_name (asm_out_file, label, exp, size);
//#define TARGET_ASM_DECLARE_CONSTANT_NAME default_asm_declare_constant_name
//asm_out_file由MtcsAsm定义，不需要作为参数传给declare_constant_name
void target_asm_out_declare_constant_name (TargetAsmOut *self,const char *name,
      const_tree exp ATTRIBUTE_UNUSED, HOST_WIDE_INT size ATTRIBUTE_UNUSED);
//原型 TARGET_ASM_DECL_END 无缺省实现
void target_asm_out_decl_end (TargetAsmOut *self);
//原型targetm.asm_out.globalize_label (file, name); #define TARGET_ASM_GLOBALIZE_LABEL default_globalize_label
void target_asm_out_globalize_label (TargetAsmOut *self,const char *name);
//原型  targetm.asm_out.internal_label 和#define TARGET_ASM_INTERNAL_LABEL default_internal_label
void target_asm_out_internal_label (TargetAsmOut *self,const char *prefix, unsigned long labelno);
//原型 targetm.asm_out.constructor #define TARGET_ASM_CONSTRUCTOR aarch64_elf_asm_constructor
void target_asm_out_constructor (TargetAsmOut *self,rtx symbol, int priority);
//原型 targetm.asm_out.destructor #define TARGET_ASM_CONSTRUCTOR aarch64_elf_asm_constructor
void target_asm_out_destructor (TargetAsmOut *self,rtx symbol, int priority);
//原型  targetm.asm_out.should_restore_cfa_state () #define TARGET_ASM_SHOULD_RESTORE_CFA_STATE hook_bool_void_false
bool target_asm_out_should_restore_cfa_state (TargetAsmOut *self);
//原型  targetm.asm_out.print_operand_punct_valid_p ((unsigned char) *p) #define TARGET_PRINT_OPERAND_PUNCT_VALID_P nvptx_print_operand_punct_valid_p
bool target_asm_out_print_operand_punct_valid_p (TargetAsmOut *self,unsigned char c);
//原型 targetm.asm_out.file_end (); #define TARGET_ASM_FILE_END nvptx_file_end
void target_asm_out_file_end (TargetAsmOut *self);
//原型 targetm.asm_out.print_patchable_function_entry TARGET_ASM_PRINT_PATCHABLE_FUNCTION_ENTRY
void target_asm_out_print_patchable_function_entry (TargetAsmOut *self,unsigned HOST_WIDE_INT patch_area_size,bool record_p);
//原型 targetm.asm_out.assemble_undefined_decl (asm_out_file, name, decl); #define TARGET_ASM_ASSEMBLE_UNDEFINED_DECL nvptx_assemble_undefined_decl
void target_asm_out_assemble_undefined_decl (TargetAsmOut *self,const char *name, const_tree decl);
//原型 targetm.asm_out.integer#define TARGET_ASM_INTEGER nvptx_assemble_integer
bool target_asm_out_integer (TargetAsmOut *self,rtx x ATTRIBUTE_UNUSED,unsigned int size ATTRIBUTE_UNUSED,int aligned_p ATTRIBUTE_UNUSED);
//原型  targetm.asm_out.emit_unwind_label  #define TARGET_ASM_EMIT_UNWIND_LABEL default_emit_unwind_label
void target_asm_out_emit_unwind_label (TargetAsmOut *self, tree decl, int for_eh,int empty );
//原型  targetm.asm_out.post_cfi_startproc  #define TARGET_ASM_POST_CFI_STARTPROC hook_void_FILEptr_tree
void target_asm_out_post_cfi_startproc (TargetAsmOut *self, tree decl);
//原型 targetm.asm_out.make_eh_symbol_indirect #define TARGET_ASM_MAKE_EH_SYMBOL_INDIRECT darwin_make_eh_symbol_indirect
rtx target_asm_out_make_eh_symbol_indirect (TargetAsmOut *self,rtx orig, bool ARG_UNUSED (pubvis));
//原型 targetm.asm_out.output_dwarf_dtprel  #define TARGET_ASM_OUTPUT_DWARF_DTPREL NULL
void target_asm_out_output_dwarf_dtprel (TargetAsmOut *self,int size, rtx x);
//原型targetm.asm_out.assemble_visibility (decl, vis);#define TARGET_ASM_ASSEMBLE_VISIBILITY default_assemble_visibility
void target_asm_out_assemble_visibility (TargetAsmOut *self,tree decl ATTRIBUTE_UNUSED,int vis ATTRIBUTE_UNUSED);
//原型 targetm.asm_out.named_section TARGET_ASM_NAMED_SECTION
void target_asm_out_named_section (TargetAsmOut *self,const char *name ATTRIBUTE_UNUSED,
      unsigned int flags ATTRIBUTE_UNUSED, tree decl ATTRIBUTE_UNUSED);
//原型 targetm.asm_out.final_postscan_insn #define TARGET_ASM_FINAL_POSTSCAN_INSN NULL i386 nvptx是空的
void target_asm_out_final_postscan_insn (TargetAsmOut *self, rtx_insn *insn, rtx *, int);

//下面是声明在MtcsTarget中的方法，
//原型  targetm.asm_out.emit_except_table_label (asm_out_file);和#define TARGET_ASM_EMIT_EXCEPT_TABLE_LABEL default_emit_except_table_label 有缺省实现
void target_asm_out_emit_except_table_label(TargetAsmOut *self);
//原型targetm.asm_out.unwind_emit #define TARGET_ASM_UNWIND_EMIT  nvptx没有实现
void target_asm_out_unwind_emit(TargetAsmOut *self,rtx_insn *insn);
//原型targetm.asm_out.output_addr_const_extra TARGET_ASM_OUTPUT_ADDR_CONST_EXTRA
bool target_asm_out_output_addr_const_extra(TargetAsmOut *self, rtx x);
//原型 targetm.asm_out.function_switched_text_sections #define TARGET_ASM_FUNCTION_SWITCHED_TEXT_SECTIONS default_function_switched_text_sections
void target_asm_out_function_switched_text_sections (TargetAsmOut *self,tree decl ATTRIBUTE_UNUSED, bool new_is_cold ATTRIBUTE_UNUSED);
//原型 targetm.asm_out.function_end_prologue (file); TARGET_ASM_FUNCTION_END_PROLOGUE
void target_asm_out_function_end_prologue(TargetAsmOut *self);
//原型 targetm.asm_out.function_begin_epilogue (file); #define TARGET_ASM_FUNCTION_BEGIN_EPILOGUE nds32_asm_function_begin_epilogue
void target_asm_out_function_begin_epilogue(TargetAsmOut *self);
//原型 targetm.asm_out.function_rodata_section #define TARGET_ASM_FUNCTION_RODATA_SECTION default_function_rodata_section
section *target_asm_out_function_rodata_section(TargetAsmOut *self,tree decl, bool relocatable);
//原型 targetm.asm_out.function_section #define TARGET_ASM_FUNCTION_SECTION default_function_section
section *target_asm_out_function_section(TargetAsmOut *self,tree decl, enum node_frequency freq,bool startup, bool exit);
//原型 targetm.asm_out.reloc_rw_mask   #define TARGET_ASM_RELOC_RW_MASK default_reloc_rw_mask
int target_asm_out_reloc_rw_mask(TargetAsmOut *self);
//原型 targetm.asm_out.function_prologue #define TARGET_ASM_FUNCTION_PROLOGUE default_function_pro_epilogue
void target_asm_out_function_prologue(TargetAsmOut *self);
//原型 targetm.asm_out.function_epilogue (asm_out_file); #define TARGET_ASM_FUNCTION_EPILOGUE default_function_pro_epilogue
void target_asm_out_function_epilogue(TargetAsmOut *self);
//原型 targetm.asm_out.ttype  #define TARGET_ASM_TTYPE hook_bool_rtx_false
bool target_asm_out_ttype(TargetAsmOut *self,rtx x);
//原型 targetm.asm_out.select_rtx_section(desc->mode, desc->constant, desc->align) #define TARGET_ASM_SELECT_RTX_SECTION default_select_rtx_section
section * target_asm_out_select_rtx_section(TargetAsmOut *self,machine_mode mode ATTRIBUTE_UNUSED,rtx x,unsigned HOST_WIDE_INT align ATTRIBUTE_UNUSED);
//原型targetm.asm_out.select_section (exp,mtcs_compute_reloc_for_constant (exp),align); #define TARGET_ASM_SELECT_SECTION default_select_section
section * target_asm_out_select_section(TargetAsmOut *self,tree decl, int reloc,unsigned HOST_WIDE_INT align ATTRIBUTE_UNUSED);
//原型 targetm.asm_out.globalize_decl_name (asm_out_file, decl);#define TARGET_ASM_GLOBALIZE_DECL_NAME default_globalize_decl_name
void target_asm_out_globalize_decl_name(TargetAsmOut *self,tree decl);
//原型 targetm.asm_out.unique_section (decl, reloc);#define TARGET_ASM_UNIQUE_SECTION default_unique_section
void target_asm_out_unique_section(TargetAsmOut *self,tree decl, int reloc);

//gcc 旧实现采用的宏，原来声明在MtcsTarget，现在移到 TargetAsmOut
//旧实现中用到宏的有 1.ASM_OUTPUT_SKIP
//原型 #define ASM_OUTPUT_SKIP(FILE, N) nvptx有实现
void target_asm_out_output_skip (TargetAsmOut *self,unsigned HOST_WIDE_INT size);
//原型 #define ASM_OUTPUT_ALIGNED_DECL_COMMON(FILE, DECL, NAME, SIZE, ALIGN)   nvptx有实现
void target_asm_out_output_aligned_decl_common (TargetAsmOut *self,const_tree decl,
         const char *name, HOST_WIDE_INT size, unsigned align);
//原型 #define ASM_DECLARE_OBJECT_NAME(FILE, NAME, DECL)  nvptx_declare_object_name (FILE, NAME, DECL)
void target_asm_out_declare_object_name (TargetAsmOut *self, const char *name, const_tree decl);
//原型 #define ASM_OUTPUT_ALIGNED_DECL_LOCAL(FILE, DECL, NAME, SIZE, ALIGN)  nvptx_output_aligned_decl (FILE, NAME, DECL, SIZE, ALIGN)
void target_asm_out_output_aligned_decl_local (TargetAsmOut *self,const_tree decl,const char *name, HOST_WIDE_INT size, unsigned align);
//原型 ASM_OUTPUT_DEBUG_LABEL
void target_asm_out_output_debug_label (TargetAsmOut *self,const char *prefix,int num);
//ASM_OUTPUT_LABELREF  amd的gcn实现不一样 有缺省实现
void target_asm_out_output_labelref (TargetAsmOut *self,char *name);
//原型 #define ASM_OUTPUT_DEF(FILE,LABEL1,LABEL2)
void target_asm_out_output_def (TargetAsmOut *self,const char *label1,const char *label2);
//原型 #define ASM_OUTPUT_ASCII(FILE, STR, LENGTH) nvptx有实现
void target_asm_out_output_ascii (TargetAsmOut *self,const char *str, unsigned HOST_WIDE_INT size);
//原型 targetm.asm_out.emit_except_personality TARGET_ASM_EMIT_EXCEPT_PERSONALITY nvptx host都是空的 没有实现
void target_asm_out_emit_except_personality (TargetAsmOut *self,rtx personality);
//实现ASM_DECLARE_FUNCTION_NAME(FILE, NAME, DECL)
void target_asm_out_declare_function_name (TargetAsmOut *self, const char *name, const_tree decl);
//ASM_GENERATE_INTERNAL_LABEL 无缺省实现
void target_asm_out_generate_internal_label (TargetAsmOut *self,char *buffer,char *prefix,int num);
//TARGET_ASM_MARK_DECL_PRESERVED gcc中的实现是hook_void_constcharptr 声明在gcc/targetdef
void target_asm_out_mark_decl_preserved (TargetAsmOut *self,const char *name);
//原型 #define TARGET_ASM_FUNCTION_PROLOGUE default_function_pro_epilogue
void target_asm_out_default_function_pro_epilogue(TargetAsmOut *self);

#endif

