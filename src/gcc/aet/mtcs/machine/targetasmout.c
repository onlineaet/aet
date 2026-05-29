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
#include "options.h"
#include "expmed.h"
#include "expr.h"
#include "fold-const.h"
#include "dwarf2out.h"
#include "common/common-targhooks.h"

#include "cfganal.h"
#include "cfgcleanup.h"
#include "alias.h"
#include "rtlhooks-def.h"
#include "tree-pass.h"
#include "dbgcnt.h"
#include "rtlanal.h"

#include "targetasmout.h"
#include "aet/aetprinttree.h"
#include "../mtcstarget.h"

//原型 targetm.asm_out.init_sections (); #define TARGET_ASM_INIT_SECTIONS hook_void_void
static void initSections_cb(TargetAsmOut *self)
{

}

//原型 targetm.asm_out.output_anchor#define TARGET_ASM_OUTPUT_ANCHOR default_asm_output_anchor
static  void outputAnchor_cb(TargetAsmOut *self,rtx symbol)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

   mtcs_asm_default_asm_output_anchor(mtcsAsm,symbol);
}

//原型 targetm.asm_out.generate_pic_addr_diff_vec  #define TARGET_ASM_GENERATE_PIC_ADDR_DIFF_VEC default_generate_pic_addr_diff_vec
static bool generatePicAddrDiffVec_cb (TargetAsmOut *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *opts=mtcsOptions->global_options;

   return opts->x_flag_pic;
}

static void globalizeLabel_cb(TargetAsmOut *self,const char *name)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   //mtcs todo fputs (GLOBAL_ASM_OP, mtcsAsm->asmFile);
   mtcs_asm_assemble_name (mtcsAsm, name);
   putc ('\n', mtcsAsm->asmFile);
}

/* This is how to output an internal numbered label where PREFIX is
   the class of label and LABELNO is the number within the class.  */
//原型  targetm.asm_out.internal_label 和#define TARGET_ASM_INTERNAL_LABEL default_internal_label
static void internalLabel_cb (TargetAsmOut *self, const char *prefix,unsigned long labelno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

   char *const buf = (char *) alloca (40 + strlen (prefix));
   mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(mtcsAsm,buf, prefix, labelno);
   //ASM_OUTPUT_INTERNAL_LABEL (stream, buf);
   mtcsAsm->output_internal_label(mtcsAsm,buf);
}

//原型  targetm.asm_out.should_restore_cfa_state () #define TARGET_ASM_SHOULD_RESTORE_CFA_STATE hook_bool_void_false
static bool shouldRestoreCfaState_cb (TargetAsmOut *self)
{
   return false;
}

/**
 *来自宏TARGET_ASM_PRINT_PATCHABLE_FUNCTION_ENTRY和targetm.asm_out.print_patchable_function_entry 缺省ptx实现
 */
static void printPatchableFunctionEntry_cb(TargetAsmOut *self,unsigned HOST_WIDE_INT patch_area_size,bool record_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   const char *nop_templ = 0;
   int code_num;
   rtx_insn *my_nop = mtcs_emit_make_insn_raw/*!make_insn_raw*/(mtcsEmit,gen_nop ());

   /* We use the template alone, relying on the (currently sane) assumption
   that the NOP template does not have variable operands.  */
   code_num = mtcs_recog_recog_memoized/*!recog_memoized*/(mtcsRecog,my_nop);
   nop_templ = mtcs_output_get_insn_template/*!get_insn_template*/(mtcsOutput,code_num, my_nop);

   if (record_p && mtcsMachine->common->have_named_sections/*!targetm.targetm_common.have_named_sections*/){
      char buf[256];
      section *previous_section = in_section;
      const char *asm_op = mtcs_output_integer_asm_op/*!integer_asm_op*/(mtcsOutput,POINTER_SIZE_UNITS, false);
      gcc_assert (asm_op != NULL);
      /* If SECTION_LINK_ORDER is supported, this internal label will
      be filled as the symbol for linked_to section.  */
      mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(mtcsAsm,buf, "LPFE", current_function_funcdef_no);
      unsigned int flags = SECTION_WRITE | SECTION_RELRO;
      if (HAVE_GAS_SECTION_LINK_ORDER)
         flags |= SECTION_LINK_ORDER;

      section *sect = mtcs_asm_get_section (mtcsAsm,"__patchable_function_entries",flags, current_function_decl);
      if (HAVE_COMDAT_GROUP && DECL_COMDAT_GROUP (current_function_decl))
         mtcs_asm_switch_to_comdat_section (mtcsAsm,sect, current_function_decl);
      else
         mtcs_asm_switch_to_section (mtcsAsm,sect,nullptr);
      mtcs_asm_assemble_align (mtcsAsm,POINTER_SIZE);
      fputs (asm_op, mtcsAsm->asmFile);
      mtcs_asm_assemble_name_raw (mtcsAsm, buf);
      fputc ('\n', mtcsAsm->asmFile);
      mtcs_asm_switch_to_section (mtcsAsm,previous_section,nullptr);
      mtcsAsm->output_label(mtcsAsm,buf);
   }

   unsigned i;
   for (i = 0; i < patch_area_size; ++i)
      mtcs_output_asm_insn (mtcsOutput,nop_templ, NULL);
}

//原型  targetm.asm_out.emit_unwind_label  #define TARGET_ASM_EMIT_UNWIND_LABEL default_emit_unwind_label
static void emitUnwindLabel_cb (TargetAsmOut *self, tree decl, int for_eh ,int empty )
{

}

/**
* 原型  targetm.asm_out.post_cfi_startproc  #define TARGET_ASM_POST_CFI_STARTPROC hook_void_FILEptr_tree
* 空实现
 */
static void postCfiStartproc_cb(TargetAsmOut *self,tree decl)
{

}

//原型targetm.asm_out.assemble_visibility (decl, vis);#define TARGET_ASM_ASSEMBLE_VISIBILITY default_assemble_visibility
static void assembleVisibility_cb(TargetAsmOut *self,tree decl ATTRIBUTE_UNUSED,int vis ATTRIBUTE_UNUSED)
{
    if (!DECL_ARTIFICIAL (decl))
       warning (OPT_Wattributes, "visibility attribute not supported in this configuration; ignored");
}

//原型 targetm.asm_out.named_section TARGET_ASM_NAMED_SECTION
static void namedSection_cb(TargetAsmOut *self,const char *name ATTRIBUTE_UNUSED,unsigned int flags ATTRIBUTE_UNUSED, tree decl ATTRIBUTE_UNUSED)
{
    /* Some object formats don't support named sections at all.  The
       front-end should already have flagged this as an error.  */
    gcc_unreachable ();
}

/////下面是定义在Mtcstarget中的方法
/**
  *原型  targetm.asm_out.emit_except_table_label (asm_out_file);
  *和#define TARGET_ASM_EMIT_EXCEPT_TABLE_LABEL default_emit_except_table_label 有缺省实现
 * 空实现
 */
static void emitExceptTableLabel_cb(TargetAsmOut *self)
{

}

/**
 * 原型targetm.asm_out.output_addr_const_extra TARGET_ASM_OUTPUT_ADDR_CONST_EXTRA
 */
static bool outputAddrConstExtra_cb (TargetAsmOut *self, rtx x)
{
  return false;
}

/**
 *原型 targetm.asm_out.function_switched_text_sections #define TARGET_ASM_FUNCTION_SWITCHED_TEXT_SECTIONS default_function_switched_text_sections
 * 空实现
 */
static void functionSwitchedTextSections_cb (TargetAsmOut *self, tree decl ATTRIBUTE_UNUSED,bool new_is_cold ATTRIBUTE_UNUSED)
{
}

/**
 *原型 targetm.asm_out.function_end_prologue (file); TARGET_ASM_FUNCTION_END_PROLOGUE
 *空实现
 */
static void functionEndPrologue_cb(TargetAsmOut *self)
{

}

/**
 *原型 targetm.asm_out.function_begin_epilogue (file); #define TARGET_ASM_FUNCTION_BEGIN_EPILOGUE nds32_asm_function_begin_epilogue
 *空实现
 */
static void functionBeginEpilogue_cb(TargetAsmOut *self)
{

}

/**
 *原型 targetm.asm_out.function_rodata_section #define TARGET_ASM_FUNCTION_RODATA_SECTION default_function_rodata_section
 */
static section *functionRodataSection_cb (TargetAsmOut *self,tree decl, bool relocatable)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

   return mtcs_asm_default_function_rodata_section (mtcsAsm,decl, relocatable);
}

//原型 targetm.asm_out.function_section #define TARGET_ASM_FUNCTION_SECTION default_function_section
static section *functionSection_cb (TargetAsmOut *self,tree decl, enum node_frequency freq,bool startup, bool exit)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

#if defined HAVE_LD_EH_GC_SECTIONS && defined HAVE_LD_EH_GC_SECTIONS_BUG
   /* Old GNU linkers have buggy --gc-section support, which sometimes
   results in .gcc_except_table* sections being garbage collected.  */
   if (decl && symtab_node::get (decl)->implicit_section)
      return NULL;
#endif

   if (!flag_reorder_functions || !mtcsMachine->common->have_named_sections/*!targetm.targetm_common.have_named_sections*/)
      return NULL;
   /* Startup code should go to startup subsection unless it is
   unlikely executed (this happens especially with function splitting
   where we can split away unnecessary parts of static constructors.  */
   if (startup && freq != NODE_FREQUENCY_UNLIKELY_EXECUTED){
      /* During LTO the tp_first_run profiling will naturally place all
      initialization code first.  Using separate section is counter-productive
      because startup only code may call functions which are no longer
      startup only.  */
      if (!in_lto_p   || !cgraph_node::get (decl)->tp_first_run  || !opt_for_fn (decl, flag_profile_reorder_functions))
         return mtcs_asm_get_named_text_section (mtcsAsm,decl, ".text.startup", NULL);
      else
         return NULL;
   }

   /* Similarly for exit.  */
   if (exit && freq != NODE_FREQUENCY_UNLIKELY_EXECUTED)
      return mtcs_asm_get_named_text_section (mtcsAsm,decl, ".text.exit", NULL);

   /* Group cold functions together, similarly for hot code.  */
   switch (freq){
      case NODE_FREQUENCY_UNLIKELY_EXECUTED:
         return mtcs_asm_get_named_text_section (mtcsAsm,decl, ".text.unlikely", NULL);
      case NODE_FREQUENCY_HOT:
         return mtcs_asm_get_named_text_section (mtcsAsm,decl, ".text.hot", NULL);
         /* FALLTHRU */
      default:
         return NULL;
   }
}

// 原型 targetm.asm_out.reloc_rw_mask   #define TARGET_ASM_RELOC_RW_MASK default_reloc_rw_mask
static int relocRwMask_cb(TargetAsmOut *self)
{
   return flag_pic ? 3 : 0;
}

//原型 targetm.asm_out.function_epilogue (asm_out_file); #define TARGET_ASM_FUNCTION_EPILOGUE default_function_pro_epilogue
static void functionEpilogue_cb(TargetAsmOut *self)
{
}

//原型 targetm.asm_out.ttype  #define TARGET_ASM_TTYPE hook_bool_rtx_false
static bool ttype_cb (TargetAsmOut *self,rtx x)
{
  return false;
}

/* Subroutine of compute_reloc_for_rtx for leaf rtxes.  */

static int compute_reloc_for_rtx_1 (const_rtx x)
{
   switch (GET_CODE (x)){
      case SYMBOL_REF:
         return SYMBOL_REF_LOCAL_P (x) ? 1 : 2;
      case LABEL_REF:
         return 1;
      default:
         return 0;
   }
}

/* Like compute_reloc_for_constant, except for an RTX.  The return value
   is a mask for which bit 1 indicates a global relocation, and bit 0
   indicates a local relocation.  Used by default_select_rtx_section
   and default_elf_select_rtx_section.  */
static int compute_reloc_for_rtx (const_rtx x)
{
   switch (GET_CODE (x)){
      case SYMBOL_REF:
      case LABEL_REF:
         return compute_reloc_for_rtx_1 (x);

      case CONST:
      {
      int reloc = 0;
         subrtx_iterator::array_type array;
         FOR_EACH_SUBRTX (iter, array, x, ALL)
         reloc |= compute_reloc_for_rtx_1 (*iter);
         return reloc;
      }

      default:
         return 0;
   }
}

//原型 targetm.asm_out.select_rtx_section(desc->mode, desc->constant, desc->align) #define TARGET_ASM_SELECT_RTX_SECTION default_select_rtx_section
static section * selectRtxSection_cb (TargetAsmOut *self,machine_mode mode ATTRIBUTE_UNUSED,
      rtx x,unsigned HOST_WIDE_INT align ATTRIBUTE_UNUSED)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

   if (compute_reloc_for_rtx (x) & self->reloc_rw_mask(self))
      return mtcs_asm_get_data_section(mtcsAsm);
   else
      return mtcs_asm_get_readonly_data_section(mtcsAsm);
}

//原型targetm.asm_out.select_section (exp,mtcs_compute_reloc_for_constant (exp),align); #define TARGET_ASM_SELECT_SECTION default_select_section
static section * selectSection_cb (TargetAsmOut *self,tree decl, int reloc,unsigned HOST_WIDE_INT align ATTRIBUTE_UNUSED)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

   n_debug("mtcstarget.c selectSection_cb decl:%p reloc:%d align:%d\n",decl,reloc,align);
   aet_print_tree(decl);
   if (DECL_P (decl)) {
      if (decl_readonly_section (decl, reloc)){
         n_debug("mtcstarget.c default_select_section 00 readonly_data_section\n");
         return mtcs_asm_get_readonly_data_section(mtcsAsm);
      }
   }else if (TREE_CODE (decl) == CONSTRUCTOR){
      if (! ((flag_pic && reloc) || !TREE_READONLY (decl) || !TREE_CONSTANT (decl))){
         n_debug("mtcstarget.c default_select_section 11 readonly_data_section\n");
         return mtcs_asm_get_readonly_data_section(mtcsAsm);
      }
   }else if (TREE_CODE (decl) == STRING_CST){
      n_debug("mtcstarget.c default_select_section 22 readonly_data_section\n");
      return mtcs_asm_get_readonly_data_section(mtcsAsm);
   }else if (! (flag_pic && reloc)){
      n_debug("mtcstarget.c default_select_section 33 readonly_data_section\n");
      return mtcs_asm_get_readonly_data_section(mtcsAsm);
   }
   n_debug("mtcstarget.c default_select_section 44 data_section\n");
   return mtcs_asm_get_data_section(mtcsAsm);
}

//原型 targetm.asm_out.globalize_decl_name (asm_out_file, decl);#define TARGET_ASM_GLOBALIZE_DECL_NAME default_globalize_decl_name
static void globalizeDeclName_cb(TargetAsmOut *self,tree decl)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

   const char *name = XSTR (XEXP (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl), 0), 0);
   self->globalize_label(self, name);
}

/**
 * 来自varasm.cc
 * mtcsasm mtcsvarasm 都定义有
 */
static inline tree ultimate_transparent_alias_target (tree *alias)
{
  tree target = *alias;

  if (IDENTIFIER_TRANSPARENT_ALIAS (target))
  {
      gcc_assert (TREE_CHAIN (target));
      target = ultimate_transparent_alias_target (&TREE_CHAIN (target));
      gcc_assert (! IDENTIFIER_TRANSPARENT_ALIAS (target) && ! TREE_CHAIN (target));
      *alias = target;
  }
  return target;
}

/* Construct a unique section name based on the decl name and the
   categorization performed above.  */
//原型 targetm.asm_out.unique_section (decl, reloc);#define TARGET_ASM_UNIQUE_SECTION default_unique_section
 //uniqueSection_cb 来自default_unique_section
static void uniqueSection_cb (TargetAsmOut *self,tree decl, int reloc)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);

   /* We only need to use .gnu.linkonce if we don't have COMDAT groups.  */
   bool one_only = DECL_ONE_ONLY (decl) /*&& !HAVE_COMDAT_GROUP*/; //nvptx HAVE_COMDAT_GROUP=0 build/auto-host.h中定义
   const char *prefix, *name, *linkonce;
   char *string;
   tree id;
   enum section_category sca=mtcs_output_categorize_decl_for_section(mtcsOutput,decl, reloc);
   switch (sca/*categorize_decl_for_section (decl, reloc)*/){
      case SECCAT_TEXT:
         prefix = one_only ? ".t" : ".text";
         break;
      case SECCAT_RODATA:
      case SECCAT_RODATA_MERGE_STR:
      case SECCAT_RODATA_MERGE_STR_INIT:
      case SECCAT_RODATA_MERGE_CONST:
         prefix = one_only ? ".r" : ".rodata";
         break;
      case SECCAT_SRODATA:
         prefix = one_only ? ".s2" : ".sdata2";
         break;
      case SECCAT_DATA:
         prefix = one_only ? ".d" : ".data";
         if (DECL_P (decl) && DECL_PERSISTENT_P (decl)){
            prefix = one_only ? ".p" : ".persistent";
            break;
         }
         break;
      case SECCAT_DATA_REL:
         prefix = one_only ? ".d.rel" : ".data.rel";
         break;
      case SECCAT_DATA_REL_LOCAL:
         prefix = one_only ? ".d.rel.local" : ".data.rel.local";
         break;
      case SECCAT_DATA_REL_RO:
         prefix = one_only ? ".d.rel.ro" : ".data.rel.ro";
         break;
      case SECCAT_DATA_REL_RO_LOCAL:
         prefix = one_only ? ".d.rel.ro.local" : ".data.rel.ro.local";
         break;
      case SECCAT_SDATA:
         prefix = one_only ? ".s" : ".sdata";
         break;
      case SECCAT_BSS:
         if (DECL_P (decl) && DECL_NOINIT_P (decl)){
            prefix = one_only ? ".n" : ".noinit";
            break;
         }
         prefix = one_only ? ".b" : ".bss";
         break;
      case SECCAT_SBSS:
         prefix = one_only ? ".sb" : ".sbss";
         break;
      case SECCAT_TDATA:
         prefix = one_only ? ".td" : ".tdata";
         break;
      case SECCAT_TBSS:
         prefix = one_only ? ".tb" : ".tbss";
         break;
      default:
         gcc_unreachable ();
   }

   id = DECL_ASSEMBLER_NAME (decl);
   ultimate_transparent_alias_target (&id);
   name = IDENTIFIER_POINTER (id);
   name = mtcsTarget->strip_name_encoding/*!name = self->strip_name_encoding (self,name)*/(mtcsTarget,name);

   /* If we're using one_only, then there needs to be a .gnu.linkonce
   prefix to the section name.  */
   linkonce = one_only ? ".gnu.linkonce" : "";
   string = ACONCAT ((linkonce, prefix, ".", name, NULL));
   set_decl_section_name (decl, string);
}

/**
 *原型 ASM_OUTPUT_DEBUG_LABEL
 */
static void outputDebugLabel_cb(TargetAsmOut *self,const char *prefix,int num)
{
    self->internal_label/*!targetm.asm_out.internal_label*/(self,prefix,num);
}

/**
 * user_label_prefix 声明在output.h中
 * extern const char *user_label_prefix;
 * 宏ASM_OUTPUT_LABELREF 缺省实现 原型 defaults.h
 */
static  void  outputLabelref_cb(TargetAsmOut *self,char *name)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsOutput *mtcsOputput=mtcs_target_get_output(mtcsTarget);

   fputs (mtcsOputput->user_label_prefix, mtcsAsm->asmFile);
   fputs (name, mtcsAsm->asmFile);
}

void  target_asm_out_init(TargetAsmOut *self)
{
   //原型  targetm.asm_out.file_start ();#define TARGET_ASM_FILE_START nvptx_file_start
   self->file_start = NULL;//子类实现fileSart_cb;
   //原型targetm.asm_out.print_operand #define TARGET_PRINT_OPERAND
   //default_print_operand(FILE *stream ATTRIBUTE_UNUSED, rtx x ATTRIBUTE_UNUSED,int code ATTRIBUTE_UNUSED);
   self->print_operand = NULL;//子类实现printOperand_cb;
   //原型targetm.asm_out.print_operand_address #define TARGET_PRINT_OPERAND_ADDRESS
   //void default_print_operand_address (FILE *stream ATTRIBUTE_UNUSED, machine_mode /*mode*/,rtx x ATTRIBUTE_UNUSED)
   self->print_operand_address = NULL;//子类实现 printOperandAddress_cb;
   //原型 targetm.asm_out.init_sections (); #define TARGET_ASM_INIT_SECTIONS hook_void_void
   self->init_sections=initSections_cb;
   //原型 targetm.asm_out.output_anchor#define TARGET_ASM_OUTPUT_ANCHOR default_asm_output_anchor
   self->output_anchor=outputAnchor_cb;
   //原型 targetm.asm_out.generate_pic_addr_diff_vec  #define TARGET_ASM_GENERATE_PIC_ADDR_DIFF_VEC default_generate_pic_addr_diff_vec
   self->generate_pic_addr_diff_vec=generatePicAddrDiffVec_cb;
   //原型 targetm.asm_out.declare_constant_name (asm_out_file, label, exp, size);
   //#define TARGET_ASM_DECLARE_CONSTANT_NAME default_asm_declare_constant_name
   //asm_out_file由MtcsAsm定义，不需要作为参数传给declare_constant_name
   self->declare_constant_name = NULL;//子类实现 declareConstantName_cb;
   //原型 TARGET_ASM_DECL_END 无缺省实现
   self->decl_end = NULL;//子类实现 declEnd_cb;
   //原型targetm.asm_out.globalize_label (file, name); #define TARGET_ASM_GLOBALIZE_LABEL default_globalize_label
   self->globalize_label=globalizeLabel_cb;
   //原型  targetm.asm_out.internal_label 和#define TARGET_ASM_INTERNAL_LABEL default_internal_label
   self->internal_label = internalLabel_cb;
   //原型 targetm.asm_out.constructor #define TARGET_ASM_CONSTRUCTOR aarch64_elf_asm_constructor
   self->constructor=NULL;//子类实现 constructor_cb
   //原型 targetm.asm_out.destructor #define TARGET_ASM_CONSTRUCTOR aarch64_elf_asm_constructor
   self->destructor=NULL;//子类实现 destructor_cb
   //原型  targetm.asm_out.should_restore_cfa_state () #define TARGET_ASM_SHOULD_RESTORE_CFA_STATE hook_bool_void_false
   self->should_restore_cfa_state=shouldRestoreCfaState_cb;
   //原型  targetm.asm_out.print_operand_punct_valid_p ((unsigned char) *p) #define TARGET_PRINT_OPERAND_PUNCT_VALID_P nvptx_print_operand_punct_valid_p
   self->print_operand_punct_valid_p = NULL;//子类实现 printOperandPunctValidP_cb;
   //原型 targetm.asm_out.file_end (); #define TARGET_ASM_FILE_END nvptx_file_end
   self->file_end = NULL;//子类实现  fileEnd_cb;
   //原型 targetm.asm_out.print_patchable_function_entry TARGET_ASM_PRINT_PATCHABLE_FUNCTION_ENTRY
   self->print_patchable_function_entry=printPatchableFunctionEntry_cb;
   //原型 targetm.asm_out.assemble_undefined_decl (asm_out_file, name, decl); #define TARGET_ASM_ASSEMBLE_UNDEFINED_DECL nvptx_assemble_undefined_decl
   self->assemble_undefined_decl =NULL; //子类实现 assembleUndefinedDecl_cb
   //原型 targetm.asm_out.integer#define TARGET_ASM_INTEGER nvptx_assemble_integer
   self->integer =NULL;//子类实现 integer_cb

   //原型  targetm.asm_out.emit_unwind_label  #define TARGET_ASM_EMIT_UNWIND_LABEL default_emit_unwind_label
   self->emit_unwind_label=emitUnwindLabel_cb;
   //原型  targetm.asm_out.post_cfi_startproc  #define TARGET_ASM_POST_CFI_STARTPROC hook_void_FILEptr_tree
   self->post_cfi_startproc=postCfiStartproc_cb;
   //原型   targetm.asm_out.make_eh_symbol_indirect #define TARGET_ASM_MAKE_EH_SYMBOL_INDIRECT darwin_make_eh_symbol_indirect
   self->make_eh_symbol_indirect=NULL;
   //原型 targetm.asm_out.output_dwarf_dtprel  #define TARGET_ASM_OUTPUT_DWARF_DTPREL NULL
   self->output_dwarf_dtprel=NULL;
   //原型targetm.asm_out.assemble_visibility (decl, vis);#define TARGET_ASM_ASSEMBLE_VISIBILITY default_assemble_visibility
   self->assemble_visibility=assembleVisibility_cb;
   //原型 targetm.asm_out.named_section TARGET_ASM_NAMED_SECTION
   self->named_section=namedSection_cb;
   //原型 targetm.asm_out.final_postscan_insn #define TARGET_ASM_FINAL_POSTSCAN_INSN NULL i386 nvptx是空的
   self->final_postscan_insn=NULL;

   //原型 targetm.asm_out.byte_op TARGET_ASM_BYTE_OP "\t.byte\t"
   self->byte_op = NULL;


   //原型  targetm.asm_out.emit_except_table_label (asm_out_file);和#define TARGET_ASM_EMIT_EXCEPT_TABLE_LABEL default_emit_except_table_label 有缺省实现
   self->emit_except_table_label=emitExceptTableLabel_cb;
   //原型targetm.asm_out.unwind_emit #define TARGET_ASM_UNWIND_EMIT  nvptx没有实现
   self->unwind_emit =NULL;
   //原型targetm.asm_out.output_addr_const_extra TARGET_ASM_OUTPUT_ADDR_CONST_EXTRA
   self->output_addr_const_extra=outputAddrConstExtra_cb;
   //原型 targetm.asm_out.function_switched_text_sections #define TARGET_ASM_FUNCTION_SWITCHED_TEXT_SECTIONS
   //default_function_switched_text_sections
   self->function_switched_text_sections=functionSwitchedTextSections_cb;
   //原型 targetm.asm_out.function_end_prologue (file); TARGET_ASM_FUNCTION_END_PROLOGUE
   self->function_end_prologue=functionEndPrologue_cb;
   //原型 targetm.asm_out.function_begin_epilogue (file); #define TARGET_ASM_FUNCTION_BEGIN_EPILOGUE nds32_asm_function_begin_epilogue
   self->function_begin_epilogue=functionBeginEpilogue_cb;
   //原型 targetm.asm_out.function_rodata_section #define TARGET_ASM_FUNCTION_RODATA_SECTION default_function_rodata_section
   self->function_rodata_section=functionRodataSection_cb;
   //原型 targetm.asm_out.function_section #define TARGET_ASM_FUNCTION_SECTION default_function_section
   self->function_section=functionSection_cb;
   //原型 targetm.asm_out.reloc_rw_mask   #define TARGET_ASM_RELOC_RW_MASK default_reloc_rw_mask
   self->reloc_rw_mask=relocRwMask_cb;
   //原型 targetm.asm_out.function_prologue #define TARGET_ASM_FUNCTION_PROLOGUE default_function_pro_epilogue
   self->function_prologue=target_asm_out_default_function_pro_epilogue;//functionPrologue_cb;
   //原型 targetm.asm_out.function_epilogue (asm_out_file); #define TARGET_ASM_FUNCTION_EPILOGUE default_function_pro_epilogue
   self->function_epilogue=functionEpilogue_cb;
   //原型 targetm.asm_out.ttype  #define TARGET_ASM_TTYPE hook_bool_rtx_false
   self->ttype=ttype_cb;
   //原型 targetm.asm_out.select_rtx_section(desc->mode, desc->constant, desc->align)
   //#define TARGET_ASM_SELECT_RTX_SECTION default_select_rtx_section
   self->select_rtx_section=selectRtxSection_cb;
   //原型targetm.asm_out.select_section (exp,mtcs_compute_reloc_for_constant (exp),align);
   //#define TARGET_ASM_SELECT_SECTION default_select_section
   self->select_section=selectSection_cb;
   //原型 targetm.asm_out.globalize_decl_name (asm_out_file, decl);#define TARGET_ASM_GLOBALIZE_DECL_NAME default_globalize_decl_name
   self->globalize_decl_name=globalizeDeclName_cb;
   //原型 targetm.asm_out.unique_section (decl, reloc);#define TARGET_ASM_UNIQUE_SECTION default_unique_section
   self->unique_section=uniqueSection_cb;

   //gcc 旧实现采用的宏，原来声明在MtcsTarget，现在移到 TargetAsmOut
   //旧实现中用到宏的有 1.ASM_OUTPUT_SKIP
   //原型 #define ASM_OUTPUT_SKIP(FILE, N) nvptx有实现
   self->output_skip=NULL;
   //原型 #define ASM_OUTPUT_ALIGNED_DECL_COMMON(FILE, DECL, NAME, SIZE, ALIGN)   nvptx有实现
   self->output_aligned_decl_common=NULL;
   //原型 #define ASM_DECLARE_OBJECT_NAME(FILE, NAME, DECL)  nvptx_declare_object_name (FILE, NAME, DECL)
   self->declare_object_name=NULL;
   //原型 #define ASM_OUTPUT_ALIGNED_DECL_LOCAL(FILE, DECL, NAME, SIZE, ALIGN)  nvptx_output_aligned_decl (FILE, NAME, DECL, SIZE, ALIGN)
   self->output_aligned_decl_local=NULL;
   //原型 ASM_OUTPUT_DEBUG_LABEL
   self->output_debug_label=outputDebugLabel_cb;
   //ASM_OUTPUT_LABELREF 缺省实现，AMD的gcn实现不同
   self->output_labelref=outputLabelref_cb;
   //原型 #define ASM_OUTPUT_DEF(FILE,LABEL1,LABEL2)
   self->output_def = NULL;
   //原型 #define ASM_OUTPUT_ASCII(FILE, STR, LENGTH) nvptx有实现
   self->output_ascii = NULL;
   //原型 targetm.asm_out.emit_except_personality TARGET_ASM_EMIT_EXCEPT_PERSONALITY nvptx host都是空的 没有实现
   self->emit_except_personality = NULL;
   //实现ASM_DECLARE_FUNCTION_NAME(FILE, NAME, DECL)
   self->declare_function_name =NULL;
   //TARGET_ASM_MARK_DECL_PRESERVED gcc中的实现是hook_void_constcharptr 声明在gcc/targetdef
   self->mark_decl_preserved =NULL;

}

//原型  targetm.asm_out.file_start ();#define TARGET_ASM_FILE_START nvptx_file_start
void target_asm_out_file_start (TargetAsmOut *self)
{
   self->file_start(self);
}

//原型targetm.asm_out.print_operand #define TARGET_PRINT_OPERAND
//default_print_operand(FILE *stream ATTRIBUTE_UNUSED, rtx x ATTRIBUTE_UNUSED,int code ATTRIBUTE_UNUSED);
void target_asm_out_print_operand (TargetAsmOut *self,rtx x ATTRIBUTE_UNUSED,int code ATTRIBUTE_UNUSED)
{
   self->print_operand(self,x,code);
}

//原型targetm.asm_out.print_operand_address #define TARGET_PRINT_OPERAND_ADDRESS
//void default_print_operand_address (FILE *stream ATTRIBUTE_UNUSED, machine_mode /*mode*/,rtx x ATTRIBUTE_UNUSED)
void target_asm_out_print_operand_address (TargetAsmOut *self,machine_mode mode,  rtx x ATTRIBUTE_UNUSED)
{
   self->print_operand_address(self,mode,x);
}

//原型 targetm.asm_out.init_sections (); #define TARGET_ASM_INIT_SECTIONS hook_void_void
void target_asm_out_init_sections (TargetAsmOut *self)
{
   self->init_sections(self);
}
//原型 targetm.asm_out.output_anchor#define TARGET_ASM_OUTPUT_ANCHOR default_asm_output_anchor
void target_asm_out_output_anchor (TargetAsmOut *self,rtx symbol)
{
   self->output_anchor(self,symbol);
}

//原型 targetm.asm_out.generate_pic_addr_diff_vec  #define TARGET_ASM_GENERATE_PIC_ADDR_DIFF_VEC default_generate_pic_addr_diff_vec
bool target_asm_out_generate_pic_addr_diff_vec (TargetAsmOut *self)
{
   return self->generate_pic_addr_diff_vec(self);
}

//原型 targetm.asm_out.declare_constant_name (asm_out_file, label, exp, size);
//#define TARGET_ASM_DECLARE_CONSTANT_NAME default_asm_declare_constant_name
//asm_out_file由MtcsAsm定义，不需要作为参数传给declare_constant_name
void target_asm_out_declare_constant_name (TargetAsmOut *self,const char *name,
      const_tree exp ATTRIBUTE_UNUSED, HOST_WIDE_INT size ATTRIBUTE_UNUSED)
{
   self->declare_constant_name(self,name,exp,size);
}

//原型 TARGET_ASM_DECL_END 无缺省实现
void target_asm_out_decl_end (TargetAsmOut *self)
{
   self->decl_end(self);
}

//原型targetm.asm_out.globalize_label (file, name); #define TARGET_ASM_GLOBALIZE_LABEL default_globalize_label
void target_asm_out_globalize_label (TargetAsmOut *self,const char *name)
{
   self->globalize_label(self,name);

}

//原型  targetm.asm_out.internal_label 和#define TARGET_ASM_INTERNAL_LABEL default_internal_label
void target_asm_out_internal_label (TargetAsmOut *self,const char *prefix, unsigned long labelno)
{
   self->internal_label(self,prefix,labelno);
}

//原型 targetm.asm_out.constructor #define TARGET_ASM_CONSTRUCTOR aarch64_elf_asm_constructor
void target_asm_out_constructor (TargetAsmOut *self,rtx symbol, int priority)
{
   self->constructor(self,symbol,priority);
}

//原型 targetm.asm_out.destructor #define TARGET_ASM_CONSTRUCTOR aarch64_elf_asm_constructor
void target_asm_out_destructor (TargetAsmOut *self,rtx symbol, int priority)
{
   self->destructor(self,symbol,priority);
}

//原型  targetm.asm_out.should_restore_cfa_state () #define TARGET_ASM_SHOULD_RESTORE_CFA_STATE hook_bool_void_false
bool target_asm_out_should_restore_cfa_state (TargetAsmOut *self)
{
   return self->should_restore_cfa_state(self);
}

//原型  targetm.asm_out.print_operand_punct_valid_p ((unsigned char) *p) #define TARGET_PRINT_OPERAND_PUNCT_VALID_P nvptx_print_operand_punct_valid_p
bool target_asm_out_print_operand_punct_valid_p (TargetAsmOut *self,unsigned char c)
{
   return self->print_operand_punct_valid_p(self,c);
}

//原型 targetm.asm_out.file_end (); #define TARGET_ASM_FILE_END nvptx_file_end
void target_asm_out_file_end (TargetAsmOut *self)
{
   self->file_end(self);
}

//原型 targetm.asm_out.print_patchable_function_entry TARGET_ASM_PRINT_PATCHABLE_FUNCTION_ENTRY
void target_asm_out_print_patchable_function_entry (TargetAsmOut *self,unsigned HOST_WIDE_INT patch_area_size,bool record_p)
{
   self->print_patchable_function_entry(self,patch_area_size,record_p);
}

//原型 targetm.asm_out.assemble_undefined_decl (asm_out_file, name, decl); #define TARGET_ASM_ASSEMBLE_UNDEFINED_DECL nvptx_assemble_undefined_decl
void target_asm_out_assemble_undefined_decl (TargetAsmOut *self,const char *name, const_tree decl)
{
   self->assemble_undefined_decl(self,name,decl);
}

//原型 targetm.asm_out.integer#define TARGET_ASM_INTEGER nvptx_assemble_integer
bool target_asm_out_integer (TargetAsmOut *self,rtx x ATTRIBUTE_UNUSED,
      unsigned int size ATTRIBUTE_UNUSED,int aligned_p ATTRIBUTE_UNUSED)
{
   return self->integer(self,x,size,aligned_p);
}

//原型  targetm.asm_out.emit_unwind_label  #define TARGET_ASM_EMIT_UNWIND_LABEL default_emit_unwind_label
void target_asm_out_emit_unwind_label (TargetAsmOut *self, tree decl, int for_eh,int empty )
{
   self->emit_unwind_label(self,decl,for_eh,empty);
}

//原型  targetm.asm_out.post_cfi_startproc  #define TARGET_ASM_POST_CFI_STARTPROC hook_void_FILEptr_tree
void target_asm_out_post_cfi_startproc (TargetAsmOut *self, tree decl)
{
   self->post_cfi_startproc(self,decl);
}

//原型 targetm.asm_out.make_eh_symbol_indirect #define TARGET_ASM_MAKE_EH_SYMBOL_INDIRECT darwin_make_eh_symbol_indirect
rtx target_asm_out_make_eh_symbol_indirect (TargetAsmOut *self,rtx orig, bool ARG_UNUSED (pubvis))
{
   return self->make_eh_symbol_indirect(self,orig,pubvis);
}

//原型 targetm.asm_out.output_dwarf_dtprel  #define TARGET_ASM_OUTPUT_DWARF_DTPREL NULL
void target_asm_out_output_dwarf_dtprel (TargetAsmOut *self,int size, rtx x)
{
   self->output_dwarf_dtprel(self,size,x);
}

//原型targetm.asm_out.assemble_visibility (decl, vis);#define TARGET_ASM_ASSEMBLE_VISIBILITY default_assemble_visibility
void target_asm_out_assemble_visibility (TargetAsmOut *self,tree decl ATTRIBUTE_UNUSED,int vis ATTRIBUTE_UNUSED)
{
   self->assemble_visibility(self,decl,vis);
}

//原型 targetm.asm_out.named_section TARGET_ASM_NAMED_SECTION
void target_asm_out_named_section (TargetAsmOut *self,const char *name ATTRIBUTE_UNUSED,
      unsigned int flags ATTRIBUTE_UNUSED, tree decl ATTRIBUTE_UNUSED)
{
   self->named_section(self,name,flags,decl);
}

//原型 targetm.asm_out.final_postscan_insn #define TARGET_ASM_FINAL_POSTSCAN_INSN NULL i386 nvptx是空的
void target_asm_out_final_postscan_insn (TargetAsmOut *self, rtx_insn *insn, rtx *r, int v)
{
   self->final_postscan_insn(self,insn,r,v);
}

//下面是声明在MtcsTarget中的方法，
//原型  targetm.asm_out.emit_except_table_label (asm_out_file);和#define TARGET_ASM_EMIT_EXCEPT_TABLE_LABEL default_emit_except_table_label 有缺省实现
void target_asm_out_emit_except_table_label(TargetAsmOut *self)
{
   self->emit_except_table_label(self);
}

//原型targetm.asm_out.unwind_emit #define TARGET_ASM_UNWIND_EMIT  nvptx没有实现
void target_asm_out_unwind_emit(TargetAsmOut *self,rtx_insn *insn)
{
   self->unwind_emit(self,insn);
}
//原型targetm.asm_out.output_addr_const_extra TARGET_ASM_OUTPUT_ADDR_CONST_EXTRA
bool target_asm_out_output_addr_const_extra(TargetAsmOut *self, rtx x)
{
   self->output_addr_const_extra(self,x);
}

//原型 targetm.asm_out.function_switched_text_sections #define TARGET_ASM_FUNCTION_SWITCHED_TEXT_SECTIONS default_function_switched_text_sections
void target_asm_out_function_switched_text_sections(TargetAsmOut *self,tree decl ATTRIBUTE_UNUSED, bool new_is_cold ATTRIBUTE_UNUSED)
{
   self->function_switched_text_sections(self,decl,new_is_cold);
}

//原型 targetm.asm_out.function_end_prologue (file); TARGET_ASM_FUNCTION_END_PROLOGUE
void target_asm_out_function_end_prologue(TargetAsmOut *self)
{
   self->function_end_prologue(self);
}

//原型 targetm.asm_out.function_begin_epilogue (file); #define TARGET_ASM_FUNCTION_BEGIN_EPILOGUE nds32_asm_function_begin_epilogue
void target_asm_out_function_begin_epilogue(TargetAsmOut *self)
{
   self->function_begin_epilogue(self);
}
//原型 targetm.asm_out.function_rodata_section #define TARGET_ASM_FUNCTION_RODATA_SECTION default_function_rodata_section
section *target_asm_out_function_rodata_section(TargetAsmOut *self,tree decl, bool relocatable)
{
   return self->function_rodata_section(self,decl,relocatable);
}

//原型 targetm.asm_out.function_section #define TARGET_ASM_FUNCTION_SECTION default_function_section
section *target_asm_out_function_section(TargetAsmOut *self,tree decl, enum node_frequency freq,bool startup, bool exit)
{
   return self->function_section(self,decl,freq,startup,exit);
}

//原型 targetm.asm_out.reloc_rw_mask   #define TARGET_ASM_RELOC_RW_MASK default_reloc_rw_mask
int target_asm_out_reloc_rw_mask(TargetAsmOut *self)
{
   return self->reloc_rw_mask(self);
}

//原型 targetm.asm_out.function_prologue #define TARGET_ASM_FUNCTION_PROLOGUE default_function_pro_epilogue
void target_asm_out_function_prologue(TargetAsmOut *self)
{
   self->function_prologue(self);
}

//原型 targetm.asm_out.function_epilogue (asm_out_file); #define TARGET_ASM_FUNCTION_EPILOGUE default_function_pro_epilogue
void target_asm_out_function_epilogue(TargetAsmOut *self)
{
   self->function_epilogue(self);

}

//原型 targetm.asm_out.ttype  #define TARGET_ASM_TTYPE hook_bool_rtx_false
bool target_asm_out_ttype(TargetAsmOut *self,rtx x)
{
   return self->ttype(self,x);
}

//原型 targetm.asm_out.select_rtx_section(desc->mode, desc->constant, desc->align) #define TARGET_ASM_SELECT_RTX_SECTION default_select_rtx_section
section * target_asm_out_select_rtx_section(TargetAsmOut *self,machine_mode mode ATTRIBUTE_UNUSED,
      rtx x,unsigned HOST_WIDE_INT align ATTRIBUTE_UNUSED)
{
   return self->select_rtx_section(self,mode,x,align);
}

//原型targetm.asm_out.select_section (exp,mtcs_compute_reloc_for_constant (exp),align); #define TARGET_ASM_SELECT_SECTION default_select_section
section * target_asm_out_select_section(TargetAsmOut *self,tree decl, int reloc,unsigned HOST_WIDE_INT align ATTRIBUTE_UNUSED)
{
   return self->select_section(self,decl,reloc,align);
}

//原型 targetm.asm_out.globalize_decl_name (asm_out_file, decl);#define TARGET_ASM_GLOBALIZE_DECL_NAME default_globalize_decl_name
void target_asm_out_globalize_decl_name(TargetAsmOut *self,tree decl)
{
   self->globalize_decl_name(self,decl);
}

//原型 targetm.asm_out.unique_section (decl, reloc);#define TARGET_ASM_UNIQUE_SECTION default_unique_section
void target_asm_out_unique_section(TargetAsmOut *self,tree decl, int reloc)
{
   self->unique_section(self,decl,reloc);
}

//gcc 旧实现采用的宏，原来声明在MtcsTarget，现在移到 TargetAsmOut
//旧实现中用到宏的有 1.ASM_OUTPUT_SKIP
//原型 #define ASM_OUTPUT_SKIP(FILE, N) nvptx有实现
void target_asm_out_output_skip (TargetAsmOut *self,unsigned HOST_WIDE_INT size)
{
   self->output_skip(self,size);
}

//原型 #define ASM_OUTPUT_ALIGNED_DECL_COMMON(FILE, DECL, NAME, SIZE, ALIGN)   nvptx有实现
void target_asm_out_output_aligned_decl_common (TargetAsmOut *self,const_tree decl,
         const char *name, HOST_WIDE_INT size, unsigned align)
{
   self->output_aligned_decl_common(self,decl,name,size,align);
}

//原型 #define ASM_DECLARE_OBJECT_NAME(FILE, NAME, DECL)  nvptx_declare_object_name (FILE, NAME, DECL)
void target_asm_out_declare_object_name (TargetAsmOut *self, const char *name, const_tree decl)
{
   self->declare_object_name(self,name,decl);
}

//原型 #define ASM_OUTPUT_ALIGNED_DECL_LOCAL(FILE, DECL, NAME, SIZE, ALIGN)  nvptx_output_aligned_decl (FILE, NAME, DECL, SIZE, ALIGN)
void target_asm_out_output_aligned_decl_local (TargetAsmOut *self,const_tree decl,
      const char *name, HOST_WIDE_INT size, unsigned align)
{
   self->output_aligned_decl_local(self,decl,name,size,align);
}

//原型 ASM_OUTPUT_DEBUG_LABEL
void target_asm_out_output_debug_label (TargetAsmOut *self,const char *prefix,int num)
{
   self->output_debug_label(self,prefix,num);
}

//ASM_OUTPUT_LABELREF  amd的gcn实现不一样 有缺省实现
void target_asm_out_output_labelref (TargetAsmOut *self,char *name)
{
   self->output_labelref(self,name);
}

//原型 #define ASM_OUTPUT_DEF(FILE,LABEL1,LABEL2)
void target_asm_out_output_def (TargetAsmOut *self,const char *label1,const char *label2)
{
   self->output_def(self,label1,label2);
}

//原型 #define ASM_OUTPUT_ASCII(FILE, STR, LENGTH) nvptx有实现
void target_asm_out_output_ascii (TargetAsmOut *self,const char *str, unsigned HOST_WIDE_INT size)
{
   self->output_ascii(self,str,size);
}

//原型 targetm.asm_out.emit_except_personality TARGET_ASM_EMIT_EXCEPT_PERSONALITY nvptx host都是空的 没有实现
void target_asm_out_emit_except_personality (TargetAsmOut *self,rtx personality)
{
   self->emit_except_personality(self,personality);
}

//实现ASM_DECLARE_FUNCTION_NAME(FILE, NAME, DECL)
void target_asm_out_declare_function_name (TargetAsmOut *self, const char *name, const_tree decl)
{
   self->declare_function_name(self,name,decl);
}

//TARGET_ASM_MARK_DECL_PRESERVED gcc中的实现是hook_void_constcharptr 声明在gcc/targetdef
void target_asm_out_mark_decl_preserved (TargetAsmOut *self,const char *name)
{
   self->mark_decl_preserved (self,name);
}

//原型 targetm.asm_out.function_prologue #define TARGET_ASM_FUNCTION_PROLOGUE default_function_pro_epilogue
void target_asm_out_default_function_pro_epilogue(TargetAsmOut *self)
{

}
