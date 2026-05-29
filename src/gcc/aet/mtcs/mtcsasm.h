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

#ifndef __GCC_MTCS_ASM__
#define __GCC_MTCS_ASM__

#include "../nlib.h"
#include "mtcscomponent.h"


typedef struct _MtcsAsm MtcsAsm;

typedef void (*AppEnable)(MtcsAsm *self,npointer userData);
typedef void (*AppDisable)(MtcsAsm *self,npointer userData);

struct _MtcsAsm
{
   MtcsComponent parent;
   /* Assign a unique number to each insn that is output.
      This can be used to generate unique local labels.  */
   FILE *asmFile;
   char *asmFileName;
   FILE *asmVarDeclFile;//写入变量声明到此文件，最后与asmFile合并。
   char *asmVarDeclFileName;

   /* This TREE_LIST contains any weak symbol declarations waiting
      to be emitted.  */
   GTY(()) tree weak_decls;

   AppEnable  appEnableCallback;
   AppDisable appDisableCallback;
   npointer userData;
   //原型 ASM_OUTPUT_LABEL 缺省ptx实现
   void (* output_label)(MtcsAsm *self,const char *name);
   //原型 ASM_OUTPUT_INTERNAL_LABEL ptx没有自已的实现
   void (*output_internal_label)(MtcsAsm *self,const char *name);
   //原型 ASM_OUTPUT_EXTERNAL (asm_out_file, decl, XSTR (XEXP (rtl, 0), 0));
   void (*output_external)(MtcsAsm *self,FILE *file,tree dec,const char *name);
   //原型 ASM_OUTPUT_REG_PUSH nvptx=0
   void (*output_reg_push)(MtcsAsm *self,int regno);
   //原型 #define ASM_OUTPUT_REG_POP(STREAM, REGNO)  各平台自定义 nvptx没定义
   void (*output_reg_pop)(MtcsAsm *self,int regno);
   //原型 #define ASM_WEAKEN_LABEL(FILE,NAME)
   void (* weaken_label)(MtcsAsm *self,const char *name);
   //原型 #define ASM_OUTPUT_TYPE_DIRECTIVE(STREAM, NAME, TYPE)
   void (* output_type_directive)(MtcsAsm *self,const char *name,const char *type);
   //原型  #define ASM_OUTPUT_ALIGN_WITH_NOP 各平台自定义 nvptx没定义
   void (* output_align_with_nop)(MtcsAsm *self,int align);
   //原型  #define  ASM_OUTPUT_MAX_SKIP_ALIGN 各平台自定义 nvptx没定义
   void (* output_max_skip_align)(MtcsAsm *self,int align,int maxSkip);
   //原型 #define ASM_OUTPUT_ALIGN(FILE, POWER)  各平台自定义 nvptx有定义
   void (* output_align)(MtcsAsm *self,int power);
   //原型 #define ASM_APP_ON "\t// #APP \n"
   void (* app_on)(MtcsAsm *self);
   //原型 #define ASM_APP_OFF "\t// #NO_APP \n"
   void (* app_off)(MtcsAsm *self);
   //原型 #define ASM_OUTPUT_ADDR_VEC
   void (* output_addr_vec)(MtcsAsm *self,int value,rtx body);
   //原型 #define ASM_OUTPUT_ADDR_DIFF_VEC
   void (* output_addr_diff_vec)(MtcsAsm *self,int value, int rel);
   //原型 #define ASM_OUTPUT_CASE_LABEL //host=1 nvptx=gcn=0
   void (* output_case_label)(MtcsAsm *self,const char *name,int num, rtx_jump_table_data *table);
   //原型 #define ASM_OUTPUT_ADDR_VEC_ELT host=1 nvptx=0 gcn=1
   void (* output_addr_vec_elt)(MtcsAsm *self,int value);
   //原型 #define ASM_OUTPUT_ADDR_DIFF_ELT host=1 nvptx=0
   void (* output_addr_vec_diff_elt)(MtcsAsm *self,rtx body,int value,int rel);
   //原型 #define ASM_OUTPUT_CASE_END host=1 nvptx=0 gcn=1
   void (* output_case_end)(MtcsAsm *self,int value,rtx_insn *insn);
   //原型 #define JUMP_TABLES_IN_TEXT_SECTION 0
   int (* jump_tables_in_text_section)(MtcsAsm *self);
   //原型 #define ASM_PREFERRED_EH_DATA_FORMAT(CODE, GLOBAL) asm_preferred_eh_data_format ((CODE), (GLOBAL))
   int (* asm_preferred_eh_data_format)(MtcsAsm *self,int code, int global);
   //ASM_GENERATE_INTERNAL_LABEL 无缺省实现
   void (* generate_internal_label)(MtcsAsm *self,char *buffer,char *prefix,int num);
  //原型 #define ASM_DECLARE_FUNCTION_SIZE(STREAM, NAME, DECL)  nvptx_function_end (STREAM)
   void (* declare_function_size)(MtcsAsm *self,const char *name, tree decl);
   //原型 #ifdef ASM_DECLARE_COLD_FUNCTION_NAME
   void (* declare_cold_function_name)(MtcsAsm *self,const char *name, tree decl);
   //原型 #ifdef ASM_DECLARE_COLD_FUNCTION_SIZE
   void (* declare_cold_function_size)(MtcsAsm *self,const char *name, tree decl);
   //原型 ASM_OUTPUT_SYMVER_DIRECTIVE 平台实现
   void (*asm_output_symver_directive)(MtcsAsm *self,char *name ,char *name2);
   //原型 #ifdef ASM_OUTPUT_WEAKREF
   void (*asm_output_weakref)(MtcsAsm *self,tree decl,char *name ,char *name2);
   //原型  #define ASM_OUTPUT_DEF_FROM_DECLS(STREAM, NAME, VALUE)  nvptx_asm_output_def_from_decls (STREAM, NAME, VALUE)
   void (*asm_output_def_from_decls)(MtcsAsm *self,tree name,tree value ATTRIBUTE_UNUSED);
   //原型 #ifdef ASM_OUTPUT_DWARF_DELTA
   void (*asm_output_dwarf_delta)(MtcsAsm *self,int size, const char *lab1, const char *lab2);
   //原型 #ifdef ASM_OUTPUT_DWARF_OFFSET
   void (*asm_output_dwarf_offset)(MtcsAsm *self,int size, const char *label, HOST_WIDE_INT offset,section *base);
   //原型 #define ASM_OUTPUT_DWARF_TABLE_REF rs6000_aix_asm_output_dwarf_table_ref
   void (*asm_output_dwarf_table_ref)(MtcsAsm *self,char * frame_table_labe);

   //提升到全局的变量，输出汇编字符串
   char *(*output_promote_decl)(MtcsAsm *self,const_tree decl, const char *name, HOST_WIDE_INT size, unsigned align);
   //原型 #define ASM_COMMENT_START "//"
   char *asmCommentStart;

   bool in_cold_section_p;// bool in_cold_section_p varasm.cc全局定义
   bool first_function_block_is_cold;//mtcs del 已在varasm.cc中声明
   tree cold_function_name;//已在varasm.cc中声明

   section *text_section;
   section *data_section;
   section *readonly_data_section;
   section *sdata_section;
   section *ctors_section;
   section *dtors_section;
   section *bss_section;
   section *sbss_section;
   /* Various forms of common section.  All are guaranteed to be nonnull.  */
    section *tls_comm_section;
    section *comm_section;
    section *lcomm_section;
    section *in_section;
    section *exception_section;
    section *eh_frame_section;//原型 output.h
    /* A linked list of all the unnamed sections.  */
    GTY(()) section *unnamed_sections;

    /* A SECTION_NOSWITCH section used for declaring global BSS variables.
       May be null.  */
    section *bss_noswitch_section;

    /* Number for making the label on the next
       constant that is stored in memory.  */
    GTY(()) int const_labelno;
    GTY(()) int anchor_labelno;

    NHashTable *funcHashTable;//存入函数
    /* Whether we saw any functions with no_split_stack.  */
    bool saw_no_split_stack;
    GTY(()) tree weakref_targets;
    //MtcsFunc *currentFunc;
    /* Hash table of named sections.  */
    NHashTable *section_htab;//原型 static GTY(()) hash_table<section_hasher> *section_htab; varasm.cc
    NHashTable *object_block_htab;//原型 static GTY(()) hash_table<mtcs_object_block_hasher> *object_block_htab; varasm.cc
    NHashTable *const_desc_htab;//原型 static GTY(()) hash_table<tree_descriptor_hasher> *const_desc_htab; varasm.cc

    npointer shared_constant_pool;//原型  rtx_constant_pool *shared_constant_pool varasm.cc
    hash_set<tree> *pending_assemble_externals_set;//原型 pending_assemble_externals_set varasm.cc

    struct{
        char *text;//原型 #ifdef TEXT_SECTION_ASM_OP 字符串
        char *data;//原型 #ifdef DATA_SECTION_ASM_OP  字符串
        char *sdata;//原型 #ifdef SDATA_SECTION_ASM_OP 字符串
        char *readonly_data;//原型 #ifdef READONLY_DATA_SECTION_ASM_OP 字符串
        char *ctors;//原型 #ifdef CTORS_SECTION_ASM_OP 字符串
        char *dtors;//原型 #ifdef DTORS_SECTION_ASM_OP 字符串
        char *bss;//原型 #ifdef BSS_SECTION_ASM_OP 字符串
        char *sbss;//原型 #ifdef SBSS_SECTION_ASM_OP 字符串
    }sectionAsmOp;

    //原型 ASM_OUTPUT_ALIGNED_BSS
    nboolean asmOutputAlignedBss;
    //原型 ASM_OUTPUT_EXTERNAL
    nboolean asmOutputExternal;

    /* We delay assemble_external processing until
       the compilation unit is finalized.  This is the best we can do for
       right now (i.e. stage 3 of GCC 4.0) - the right thing is to delay
       it all the way to final.  See PR 17982 for further discussion.  */
    GTY(()) tree pending_assemble_externals;

    /* A similar list of pending libcall symbols.  We only want to declare
       symbols that are actually used in the final assembly.  */
    GTY(()) rtx pending_libcall_symbols;

    /* Some targets delay some output to final using TARGET_ASM_FILE_END.
       As a result, assemble_external can be called after the list of externals
       is processed and the pointer set destroyed.  */
    bool pending_assemble_externals_processed;
};

void     mtcs_asm_init(MtcsAsm *self);
//原型 #define ASM_APP_ON "\t// #APP \n"
void     mtcs_asm_app_on(MtcsAsm *self);
//原型 #define ASM_APP_OFF "\t// #NO_APP \n"
void     mtcs_asm_app_off(MtcsAsm *self);
void     mtcs_asm_start_function(MtcsAsm *self,tree decl, const char *fnname);
void     mtcs_asm_set_app_callback(MtcsAsm *self,AppEnable enable,AppDisable disable,npointer userData);
//原型 assemble_external_libcall output.h varasm.cc
void     mtcs_asm_assemble_external_libcall (MtcsAsm *self,rtx fun);
section *mtcs_asm_get_exception_section (MtcsAsm *self);
void     mtcs_asm_set_exception_section (MtcsAsm *self,section *section);
//原型 switch_to_section output.h varasm.cc
void     mtcs_asm_switch_to_section(MtcsAsm *self,section *section, tree =nullptr);
void     mtcs_asm_assemble_align (MtcsAsm *self,unsigned int align);
void     mtcs_asm_assemble_name (MtcsAsm *self,const char *name);

//原型 assemble_integer output.h varasm.cc
bool     mtcs_asm_assemble_integer (MtcsAsm *self,rtx x, unsigned int size, unsigned int align, int force);
void     mtcs_asm_assemble_zeros (MtcsAsm *self,unsigned HOST_WIDE_INT size);
void     mtcs_asm_assemble_name_raw (MtcsAsm *self,const char *name);
void     mtcs_asm_assemble_label (MtcsAsm *self, const char *name);
//原型 assemble_real output.h varasm.cc
void     mtcs_asm_assemble_real (MtcsAsm *self,REAL_VALUE_TYPE d, scalar_float_mode mode, unsigned int align, bool reverse);
//原型 assemble_string output.h  varasm.cc
void     mtcs_asm_assemble_string (MtcsAsm *self,const char *p, int size);
//原型 assemble_name_resolve output.h varasm.cc
const char *mtcs_asm_assemble_name_resolve (MtcsAsm *self,const char *name);
section *mtcs_asm_get_section (MtcsAsm *self,const char *name, unsigned int flags, tree decl,bool not_existing = false);
//原型 assemble_external output.h varasm.cc
void     mtcs_asm_assemble_external (MtcsAsm *self,tree decl ATTRIBUTE_UNUSED);
//原型 default_function_rodata_section output.h varasm.cc
section *mtcs_asm_default_function_rodata_section (MtcsAsm *self,tree decl, bool relocatable);
//原型 function_section output.h varasm.cc
section *mtcs_ams_function_section (MtcsAsm *self,tree decl);
//原型 get_named_section output.h varasm.cc
section *mtcs_asm_get_named_section (MtcsAsm *self,tree decl, const char *name, int reloc);
section *mtcs_asm_get_named_text_section (MtcsAsm *self,tree decl,const char *text_section_name,const char *named_section_suffix);
section *mtcs_asm_get_text_section (MtcsAsm *self);
section *mtcs_asm_get_data_section (MtcsAsm *self);
section *mtcs_asm_get_readonly_data_section (MtcsAsm *self);

void     mtcs_asm_assemble_end_function (MtcsAsm *self,tree decl, const char *fnname ATTRIBUTE_UNUSED);
//原型 current_function_section output.h varasm.cc
section *mtcs_asm_current_function_section (MtcsAsm *self);
section *mtcs_asm_get_variable_section (MtcsAsm *self,tree decl, bool prefer_noswitch_p);

/* Tell assembler to switch to unlikely-to-be-executed text section.  */

section *mtcs_asm_unlikely_text_section (MtcsAsm *self);
//原型 output_constant_def rtl.h varasm.cc
rtx      mtcs_asm_output_constant_def (MtcsAsm *self,tree exp, int defer);
void     mtcs_asm_place_block_symbol (MtcsAsm *self,rtx symbol);
//原型 align_variable output.h varasm.cc
void     mtcs_asm_align_variable (MtcsAsm *self,tree decl, bool dont_output_data);
int      mtcs_asm_compute_reloc_for_constant (MtcsAsm *self,tree exp);
//原型 decl_binds_to_current_def_p varasm.h varasm.cc
bool     mtcs_asm_decl_binds_to_current_def_p (MtcsAsm *self,const_tree decl);
void     mtcs_asm_notice_global_symbol (MtcsAsm *self,tree decl);
bool     mtcs_asm_maybe_assemble_visibility (MtcsAsm *self,tree decl);
void     mtcs_asm_output_addressed_constants (MtcsAsm *self,tree exp, int defer);
void     mtcs_asm_output_constant_def_contents (MtcsAsm *self,rtx symbol);
unsigned int mtcs_asm_get_variable_align (MtcsAsm *self,tree decl);
void     mtcs_asm_switch_to_comdat_section (MtcsAsm *self,section *sect, tree decl);
unsigned HOST_WIDE_INT mtcs_asm_output_constant (MtcsAsm *self,tree exp, unsigned HOST_WIDE_INT size,
                           unsigned int align,bool reverse, bool merge_strings);
//原型 make_decl_rtl varasm.h varasm.cc
void     mtcs_asm_make_decl_rtl (MtcsAsm *self,tree decl);
const char *mtcs_asm_get_fnname_from_decl (MtcsAsm *self,tree decl);

void         mtcs_asm_resolve_unique_section (MtcsAsm *self,tree decl, int reloc ATTRIBUTE_UNUSED,int flag_function_or_data_sections);
//原型 force_const_mem rtl.h varasm.cc
rtx         mtcs_asm_force_const_mem (MtcsAsm *self,machine_mode in_mode, rtx x);
//原型 get_section_anchor output.h varasm.cc
rtx mtcs_asm_get_section_anchor (MtcsAsm *self,struct object_block *block, HOST_WIDE_INT offset,enum tls_model model);

void     mtcs_asm_print(MtcsAsm *self);
//原型 ASM_OUTPUT_ALIGNED_BSS
void mtcs_asm_set_asm_output_aligned_bss(MtcsAsm *self,nboolean is);

//原型 init_varasm_once rtl.h varasm.c
void mtcs_asm_init_varasm_once (MtcsAsm *self);
//原型 default_asm_output_anchor output.h varasm.cc
void mtcs_asm_default_asm_output_anchor (MtcsAsm *self,rtx symbol);
//原型 init_varasm_status varasm.h varasm.cc
void mtcs_asm_init_varasm_status (MtcsAsm *self);
//原型 decide_function_section rtl.h varasm.cc
void mtcs_asm_decide_function_section (MtcsAsm *self,tree decl);
//原型 #define JUMP_TABLES_IN_TEXT_SECTION 0
int  mtcs_asm_jump_tables_in_text_section (MtcsAsm *self);
//原型 #define ASM_PREFERRED_EH_DATA_FORMAT(CODE, GLOBAL) asm_preferred_eh_data_format ((CODE), (GLOBAL))
int mtcs_asm_asm_preferred_eh_data_format (MtcsAsm *self,int code, int global);
//原型 #define ASM_GENERATE_INTERNAL_LABEL 无缺省实现
void mtcs_asm_generate_internal_label (MtcsAsm *self,char *buffer,char *prefix,int num);
//原型 #define ASM_COMMENT_START "//"
void  mtcs_asm_set_comment_start (MtcsAsm *self,const char *value);
const char *mtcs_asm_get_comment_start(MtcsAsm *self);
//原型 static void finalize () toplev.cc
void mtcs_asm_close(MtcsAsm *self);
//提升到全局的变量，输出汇编字符串
char *mtcs_asm_output_promote_decl(MtcsAsm *self,const_tree decl, const char *name, HOST_WIDE_INT size, unsigned align);
//原型 #define ASM_OUTPUT_ALIGN(FILE, POWER)  各平台自定义 nvptx有定义
void mtcs_asm_output_align(MtcsAsm *self,int power);
//原型 ASM_OUTPUT_LABEL 缺省ptx实现
void mtcs_asm_output_label(MtcsAsm *self,const char *name);
//原型 #ifdef ASM_OUTPUT_DWARF_DELTA
void mtcs_asm_output_dwarf_delta(MtcsAsm *self,int size, const char *lab1, const char *lab2);
//原型 #ifdef ASM_OUTPUT_DWARF_OFFSET
void mtcs_asm_output_dwarf_offset(MtcsAsm *self,int size, const char *label, HOST_WIDE_INT offset,section *base);
//原型 #define ASM_OUTPUT_DWARF_TABLE_REF rs6000_aix_asm_output_dwarf_table_ref
void mtcs_asm_output_dwarf_table_ref(MtcsAsm *self,char * frame_table_labe);
//原型 make_decl_rtl_for_debug varasm.h varasm.cc
rtx mtcs_asm_make_decl_rtl_for_debug (MtcsAsm *self,tree decl);
//保存编译好的ptx到变量
void mtcs_asm_create_asm_var(MtcsAsm *self);

/* Holds the RTL expression for the value of a variable or function.
   This value can be evaluated lazily for functions, variables with
   static storage duration, and labels.  */
//原型 DECL_RTL tree.h
//#define DECL_RTL(NODE)                  \
//  (DECL_WRTL_CHECK (NODE)->decl_with_rtl.rtl        \
//   ? (NODE)->decl_with_rtl.rtl                  \
//   : (make_decl_rtl (NODE), (NODE)->decl_with_rtl.rtl))
//inline rtx mtcs_asm_decl_rtl(MtcsAsm *self,tree decl)
//{
//    if(DECL_WRTL_CHECK (decl)->decl_with_rtl.rtl)
//        return decl->decl_with_rtl.rtl;
//    mtcs_asm_make_decl_rtl(self,decl);
//    gcc_assert (DECL_WRTL_CHECK (decl)->decl_with_rtl.rtl);
//    return decl->decl_with_rtl.rtl;
//}

#define mtcs_asm_decl_rtl(MTCSASM,NODE)              \
  (DECL_WRTL_CHECK (NODE)->decl_with_rtl.rtl    \
   ? (NODE)->decl_with_rtl.rtl               \
   : (mtcs_asm_make_decl_rtl (MTCSASM,NODE), (NODE)->decl_with_rtl.rtl))

#endif
