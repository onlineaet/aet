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
 * base on except.cc
 */


/* This file handles generation of all the assembler code
   *except* the instructions of a function.
   This includes declarations of variables and their initial values.

   We also output the assembler code for constants stored in memory
   and are responsible for combining constants with the same value.  */

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
#include "cfgloop.h"
#include "cfghooks.h"

#include "mtcsasm.h"
#include "mtcsexcept.h"
#include "mtcsdwarf2asm.h"
#include "mtcstool.h"
#include "mtcstarget.h"


struct GTY(()) call_site_record_d
{
  rtx landing_pad;
  int action;
};

/* In the following structure and associated functions,
   we represent entries in the action table as 1-based indices.
   Special cases are:

     0: null action record, non-null landing pad; implies cleanups
    -1: null action record, null landing pad; implies no action
    -2: no call-site entry; implies must_not_throw
    -3: we have yet to process outer regions

   Further, no special cases apply to the "next" field of the record.
   For next, 0 means end of list.  */

struct action_record
{
  int offset;
  int filter;
  int next;
};

/* Hashtable helpers.  */

struct action_record_hasher : free_ptr_hash <action_record>
{
  static inline hashval_t hash (const action_record *);
  static inline bool equal (const action_record *, const action_record *);
};

inline hashval_t
action_record_hasher::hash (const action_record *entry)
{
  return entry->next * 1009 + entry->filter;
}

inline bool
action_record_hasher::equal (const action_record *entry,
                 const action_record *data)
{
  return entry->filter == data->filter && entry->next == data->next;
}

typedef hash_table<action_record_hasher> action_hash_type;

static void mtcsExceptInit(MtcsExcept *self)
{

}

static int sjlj_size_of_call_site_table (MtcsExcept *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   int n = vec_safe_length (mtcsRtlData/*!crtl*/->eh.call_site_record_v[0]);
   int size = 0;
   int i;

   for (i = 0; i < n; ++i){
      struct call_site_record_d *cs =(*mtcsRtlData/*!crtl*/->eh.call_site_record_v[0])[i];
      size += size_of_uleb128 (INTVAL (cs->landing_pad));
      size += size_of_uleb128 (cs->action);
   }

   return size;
}


static void dw2_output_call_site_table (MtcsExcept *self,int cs_format, int section)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  int n = vec_safe_length (mtcsRtlData/*!crtl*/->eh.call_site_record_v[section]);
  int i;
  const char *begin;
  if (section == 0)
    begin = current_function_func_begin_label;
  else if (mtcsAsm->first_function_block_is_cold)
    begin = mtcsRtlData/*!crtl*/->subsections.hot_section_label;
  else
    begin = mtcsRtlData/*!crtl*/->subsections.cold_section_label;

  for (i = 0; i < n; ++i){
      struct call_site_record_d *cs = (*mtcsRtlData/*!crtl*/->eh.call_site_record_v[section])[i];
      char reg_start_lab[32];
      char reg_end_lab[32];
      char landing_pad_lab[32];

      mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(mtcsAsm,reg_start_lab, "LEHB",self->call_site_base + i);
      mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(mtcsAsm,reg_end_lab, "LEHE", self->call_site_base + i);

      if (cs->landing_pad)
         mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(mtcsAsm,landing_pad_lab, "L",CODE_LABEL_NUMBER (cs->landing_pad));

      /* ??? Perhaps use insn length scaling if the assembler supports
     generic arithmetic.  */
      /* ??? Perhaps use attr_length to choose data1 or data2 instead of
     data4 if the function is small enough.  */
      if (cs_format == DW_EH_PE_uleb128){
          dw2_asm_output_delta_uleb128 (reg_start_lab, begin,"region %d start", i);
          dw2_asm_output_delta_uleb128 (reg_end_lab, reg_start_lab,"length");
          if (cs->landing_pad)
            dw2_asm_output_delta_uleb128 (landing_pad_lab, begin,"landing pad");
          else
            dw2_asm_output_data_uleb128 (0, "landing pad");
      }else{
          dw2_asm_output_delta (4, reg_start_lab, begin,"region %d start", i);
          dw2_asm_output_delta (4, reg_end_lab, reg_start_lab, "length");
          if (cs->landing_pad)
            dw2_asm_output_delta (4, landing_pad_lab, begin,"landing pad");
          else
            dw2_asm_output_data (4, 0, "landing pad");
      }
      dw2_asm_output_data_uleb128 (cs->action, "action");
  }
  self->call_site_base += n;
}


static int dw2_size_of_call_site_table (MtcsExcept *self,int section)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   int n = vec_safe_length (mtcsRtlData/*!crtl*/->eh.call_site_record_v[section]);
   int size = n * (4 + 4 + 4);
   int i;
   for (i = 0; i < n; ++i){
      struct call_site_record_d *cs = (*mtcsRtlData/*!crtl*/->eh.call_site_record_v[section])[i];
      size += size_of_uleb128 (cs->action);
   }
   return size;
}


/* Switch to the section that should be used for exception tables.  */

static void switch_to_exception_section (MtcsExcept *self,const char * ARG_UNUSED (fnname))
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  section *s;
  section *mtcs_exception_section=mtcs_asm_get_exception_section(mtcsAsm);
  if (mtcs_exception_section)
    s = mtcs_exception_section;
  else{
      int flags;
      if (EH_TABLES_CAN_BE_READ_ONLY){
          int tt_format =mtcs_asm_asm_preferred_eh_data_format/*!ASM_PREFERRED_EH_DATA_FORMAT*/(mtcsAsm,/*code=*/0, /*global=*/1);
          flags = ((! flag_pic || ((tt_format & 0x70) != DW_EH_PE_absptr && (tt_format & 0x70) != DW_EH_PE_aligned)) ? 0 : SECTION_WRITE);
      }else
          flags = SECTION_WRITE;

      /* Compute the section and cache it into exception_section,
     unless it depends on the function name.  */
      if (mtcsMachine->common->have_named_sections/*!targetm.targetm_common.have_named_sections*/){
//#ifdef HAVE_LD_EH_GC_SECTIONS host=1 nvptx=0
//      if (flag_function_sections || (DECL_COMDAT_GROUP (current_function_decl) && HAVE_COMDAT_GROUP)){
//          char *section_name = XNEWVEC (char, strlen (fnname) + 32);
//          /* The EH table must match the code section, so only mark
//         it linkonce if we have COMDAT groups to tie them together.  */
//          if (DECL_COMDAT_GROUP (current_function_decl) && HAVE_COMDAT_GROUP)
//              flags |= SECTION_LINKONCE;
//          sprintf (section_name, ".gcc_except_table.%s", fnname);
//          s = mtcs_asm_get_section (mtcsAsm,section_name, flags, current_function_decl);
//          free (section_name);
//      }else
//#endif
          mtcs_exception_section = s = mtcs_asm_get_section (mtcsAsm,".gcc_except_table", flags, NULL);
  }else
      mtcs_exception_section= s = flags == SECTION_WRITE ? data_section : readonly_data_section;
  }
  mtcs_asm_set_exception_section(mtcsAsm,mtcs_exception_section);
  mtcs_asm_switch_to_section (mtcsAsm,s);
}

static void sjlj_output_call_site_table (MtcsExcept *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsDwarf2Asm *mtcsDwarf2Asm=mtcs_target_get_dwarf2_asm(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  int n = vec_safe_length (mtcsRtlData/*!crtl*/->eh.call_site_record_v[0]);
  int i;
  for (i = 0; i < n; ++i) {
      struct call_site_record_d *cs = (*mtcsRtlData/*!crtl*/->eh.call_site_record_v[0])[i];
      mtcs_dwarf2_asm_output_data_uleb128 (mtcsDwarf2Asm,INTVAL (cs->landing_pad),"region %d landing pad", i);
      mtcs_dwarf2_asm_output_data_uleb128 (mtcsDwarf2Asm,cs->action, "action");
  }
  self->call_site_base += n;
}

/* Output a reference from an exception table to the type_info object TYPE.
   TT_FORMAT and TT_FORMAT_SIZE describe the DWARF encoding method used for
   the value.  */
static void output_ttype (MtcsExcept *self,tree type, int tt_format, int tt_format_size)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
   MtcsDwarf2Asm *mtcsDwarf2Asm=mtcs_target_get_dwarf2_asm(mtcsTarget);

   rtx value;
   bool is_public = true;
   if (type == NULL_TREE)
      value = const0_rtx;
   else{
      /* FIXME lto.  pass_ipa_free_lang_data changes all types to
      runtime types so TYPE should already be a runtime type
      reference.  When pass_ipa_free_lang data is made a default
      pass, we can then remove the call to lookup_type_for_runtime
      below.  */
      if (TYPE_P (type))
         type = lookup_type_for_runtime (type);

      value = expand_expr (type, NULL_RTX, VOIDmode, EXPAND_INITIALIZER);
      /* Let cgraph know that the rtti decl is used.  Not all of the
      paths below go through assemble_integer, which would take
      care of this for us.  */
      STRIP_NOPS (type);
      if (TREE_CODE (type) == ADDR_EXPR){
         type = TREE_OPERAND (type, 0);
         if (VAR_P (type))
            is_public = TREE_PUBLIC (type);
      }else
         gcc_assert (TREE_CODE (type) == INTEGER_CST);
   }

   /* Allow the target to override the type table entry format.  */
   if (target_asm_out_ttype/*!targetm.asm_out.ttype */(mtcsMachine->asmOut,value))
      return;

   if (tt_format == DW_EH_PE_absptr || tt_format == DW_EH_PE_aligned)
      mtcs_asm_assemble_integer (mtcsAsm,value, tt_format_size,tt_format_size * BITS_PER_UNIT, 1);
   else
      mtcs_dwarf2_asm_output_encoded_addr_rtx (mtcsDwarf2Asm,tt_format, value, is_public, NULL);
}


/* Output an exception table for the current function according to SECTION.

   If the function has been partitioned into hot and cold parts, value 0 for
   SECTION refers to the table associated with the hot part while value 1
   refers to the table associated with the cold part.  If the function has
   not been partitioned, value 0 refers to the single exception table.  */

static void output_one_function_exception_table (MtcsExcept *self,int section)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsDwarf2Asm *mtcsDwarf2Asm=mtcs_target_get_dwarf2_asm(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  int tt_format, cs_format, lp_format, i;
  char ttype_label[32];
  char cs_after_size_label[32];
  char cs_end_label[32];
  int call_site_len;
  int have_tt_data;
  int tt_format_size = 0;

  have_tt_data = (vec_safe_length (cfun->eh->ttype_data) || (mtcsTarget->arm_eabi_unwinder
          ? vec_safe_length (cfun->eh->ehspec_data.arm_eabi) : vec_safe_length (cfun->eh->ehspec_data.other)));

  /* Indicate the format of the @TType entries.  */
  if (! have_tt_data)
    tt_format = DW_EH_PE_omit;
  else {
      tt_format = mtcs_asm_asm_preferred_eh_data_format/*!ASM_PREFERRED_EH_DATA_FORMAT*/(mtcsAsm,/*code=*/0, /*global=*/1);
      //nvptx HAVE_AS_LEB128 所以下面代码屏蔽
      if (mtcs_config_get_value(mtcsConfig,MTCS_HAVE_AS_LEB128))
         mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(mtcsAsm,
               ttype_label,section ? "LLSDATTC" : "LLSDATT",current_function_funcdef_no);

      tt_format_size = size_of_encoded_value (tt_format);
      mtcs_asm_assemble_align (mtcsAsm,tt_format_size * BITS_PER_UNIT);
  }

  target_asm_out_internal_label/*!targetm.asm_out.internal_label*/(mtcsMachine->asmOut,
        section ? "LLSDAC" : "LLSDA",current_function_funcdef_no);

  /* The LSDA header.  */
  /* Indicate the format of the landing pad start pointer.  An omitted
     field implies @LPStart == @Start.  */
  /* Currently we always put @LPStart == @Start.  This field would
     be most useful in moving the landing pads completely out of
     line to another section, but it could also be used to minimize
     the size of uleb128 landing pad offsets.  */
  lp_format = DW_EH_PE_omit;
  mtcs_dwarf2_asm_output_data (mtcsDwarf2Asm,1, lp_format, "@LPStart format (%s)",eh_data_format_name (lp_format));

  /* @LPStart pointer would go here.  */
  mtcs_dwarf2_asm_output_data (mtcsDwarf2Asm,1, tt_format, "@TType format (%s)",eh_data_format_name (tt_format));

  if (mtcs_config_get_value/*!HAVE_AS_LEB128*/(mtcsConfig,MTCS_HAVE_AS_LEB128)){
      if (target_common_except_unwind_info/*!targetm_common.except_unwind_info*/(mtcsMachine->common,
            mtcsOptions->global_options) == UI_SJLJ)
          call_site_len = sjlj_size_of_call_site_table(self);
      else
          call_site_len = dw2_size_of_call_site_table(self,section);
  }

  /* A pc-relative 4-byte displacement to the @TType data.  */
  if (have_tt_data){
      if (mtcs_config_get_value(mtcsConfig,MTCS_HAVE_AS_LEB128)){
          char ttype_after_disp_label[32];
          //ASM_GENERATE_INTERNAL_LABEL (ttype_after_disp_label,section ? "LLSDATTDC" : "LLSDATTD",current_function_funcdef_no);
          mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(mtcsAsm,
                ttype_after_disp_label,section ? "LLSDATTDC" : "LLSDATTD",current_function_funcdef_no);
          mtcs_dwarf2_asm_output_delta_uleb128 (mtcsDwarf2Asm,ttype_label, ttype_after_disp_label,"@TType base offset");
          //ASM_OUTPUT_LABEL (asm_out_file, ttype_after_disp_label);
          mtcsAsm->output_label (mtcsAsm, ttype_after_disp_label);
      }else{
          /* Ug.  Alignment queers things.  */
          unsigned int before_disp, after_disp, last_disp, disp;

          before_disp = 1 + 1;
          after_disp = (1 + size_of_uleb128 (call_site_len)
                + call_site_len + vec_safe_length (mtcsRtlData/*!crtl*/->eh.action_record_data)
                + (vec_safe_length (cfun->eh->ttype_data) * tt_format_size));
          disp = after_disp;
          do{
              unsigned int disp_size, pad;
              last_disp = disp;
              disp_size = size_of_uleb128 (disp);
              pad = before_disp + disp_size + after_disp;
              if (pad % tt_format_size)
                  pad = tt_format_size - (pad % tt_format_size);
              else
                  pad = 0;
              disp = after_disp + pad;
          }while (disp != last_disp);
          mtcs_dwarf2_asm_output_data_uleb128 (mtcsDwarf2Asm,disp, "@TType base offset");
      }
  }

  /* Indicate the format of the call-site offsets.  */
  if (mtcs_config_get_value/*!HAVE_AS_LEB128*/(mtcsConfig,MTCS_HAVE_AS_LEB128))
    cs_format = DW_EH_PE_uleb128;
  else
    cs_format = DW_EH_PE_udata4;

  mtcs_dwarf2_asm_output_data (mtcsDwarf2Asm,1, cs_format, "call-site format (%s)",eh_data_format_name (cs_format));

  if (mtcs_config_get_value/*!HAVE_AS_LEB128*/(mtcsConfig,MTCS_HAVE_AS_LEB128)){
     mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(mtcsAsm,cs_after_size_label,
                   section ? "LLSDACSBC" : "LLSDACSB", current_function_funcdef_no);
     mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(mtcsAsm,cs_end_label,
                   section ? "LLSDACSEC" : "LLSDACSE", current_function_funcdef_no);
      mtcs_dwarf2_asm_output_delta_uleb128 (mtcsDwarf2Asm,cs_end_label, cs_after_size_label,"Call-site table length");
      mtcsAsm->output_label (mtcsAsm, cs_after_size_label);
      if (target_common_except_unwind_info/*!targetm_common.except_unwind_info*/(mtcsMachine->common,
            mtcsOptions->global_options) == UI_SJLJ)
          sjlj_output_call_site_table (self);
      else
          dw2_output_call_site_table (self,cs_format, section);
      mtcsAsm->output_label (mtcsAsm, cs_end_label);
  }else{
      mtcs_dwarf2_asm_output_data_uleb128 (mtcsDwarf2Asm,call_site_len, "Call-site table length");
      if (target_common_except_unwind_info/*!targetm_common.except_unwind_info*/(mtcsMachine->common,
            mtcsOptions->global_options) == UI_SJLJ)
          sjlj_output_call_site_table (self);
      else
          dw2_output_call_site_table (self,cs_format, section);
  }

  /* ??? Decode and interpret the data for flag_debug_asm.  */
  {
    uchar uc;
    FOR_EACH_VEC_ELT (*mtcsRtlData/*!crtl*/->eh.action_record_data, i, uc)
       mtcs_dwarf2_asm_output_data (mtcsDwarf2Asm,1, uc, i ? NULL : "Action record table");
  }

  if (have_tt_data)
      mtcs_asm_assemble_align (mtcsAsm,tt_format_size * BITS_PER_UNIT);

  i = vec_safe_length (cfun->eh->ttype_data);
  while (i-- > 0){
      tree type = (*cfun->eh->ttype_data)[i];
      output_ttype (self,type, tt_format, tt_format_size);
  }

  if (HAVE_AS_LEB128 && have_tt_data)
      mtcsAsm->output_label (mtcsAsm, ttype_label);

  /* ??? Decode and interpret the data for flag_debug_asm.  */
  if (targetm.arm_eabi_unwinder){
      tree type;
      for (i = 0;vec_safe_iterate (cfun->eh->ehspec_data.arm_eabi, i, &type); ++i)
          output_ttype (self,type, tt_format, tt_format_size);
  }else{
      uchar uc;
      for (i = 0;vec_safe_iterate (cfun->eh->ehspec_data.other, i, &uc); ++i)
          mtcs_dwarf2_asm_output_data (mtcsDwarf2Asm,1, uc, i ? NULL : "Exception specification table");
  }
}


/* Output an exception table for the current function according to SECTION,
   switching back and forth from the function section appropriately.

   If the function has been partitioned into hot and cold parts, value 0 for
   SECTION refers to the table associated with the hot part while value 1
   refers to the table associated with the cold part.  If the function has
   not been partitioned, value 0 refers to the single exception table.  */

void mtcs_except_output_function_exception_table (MtcsExcept *self,int section)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);

   const char *fnname =  mtcs_asm_get_fnname_from_decl (mtcsAsm,current_function_decl);
   rtx personality = mtcs_expr_get_personality_function/*!get_personality_function*/(mtcsExpr,current_function_decl);
   /* Not all functions need anything.  */
   //if (!crtl->uses_eh_lsda  || targetm_common.except_unwind_info (&global_options) == UI_NONE)
   if (!mtcsRtlData/*!crtl*/->uses_eh_lsda
   ||target_common_except_unwind_info/*!targetm_common.except_unwind_info*/(mtcsMachine->common,
        mtcsOptions->global_options) == UI_NONE)
      return;

   /* No need to emit any boilerplate stuff for the cold part.  */
   if (section == 1 && !mtcsRtlData/*!crtl*/->eh.call_site_record_v[1])
      return;

   if (personality){
      mtcs_asm_assemble_external_libcall/*!assemble_external_libcall*/(mtcsAsm,personality);
      if (mtcsMachine->asmOut->emit_except_personality/*!targetm.asm_out.emit_except_personality*/)
         target_asm_out_emit_except_personality/*!targetm.asm_out.emit_except_personality*/(mtcsMachine->asmOut,personality);
   }

   switch_to_exception_section (self,fnname);
   /* If the target wants a label to begin the table, emit it here.  */
   target_asm_out_emit_except_table_label/*!targetm.asm_out.emit_except_table_label*/(mtcsMachine->asmOut);
   /* Do the real work.  */
   output_one_function_exception_table (self,section);
   mtcs_asm_switch_to_section (mtcsAsm,mtcs_asm_current_function_section/*!current_function_section*/(mtcsAsm));

}

/* Update the sjlj function context.  This function should be called
   whenever we allocate or deallocate dynamic stack space.  */
//原型 update_sjlj_context except.h except.cc
void mtcs_except_update_sjlj_context (MtcsExcept *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  if (!mtcsOptionsItem/*!flag_exceptions*/->x_flag_exceptions)
    return;

  mtcs_emit_emit_note/*!emit_note*/(mtcsEmit,NOTE_INSN_UPDATE_SJLJ_CONTEXT);
}

//原型 init_eh_for_function except.h except.cc
void mtcs_except_init_eh_for_function (MtcsExcept *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  mtcsFunc/*!cfun*/->currentFun->eh = ggc_cleared_alloc<eh_status> ();
  /* Make sure zero'th entries are used.  */
  vec_safe_push (mtcsFunc->currentFun->eh->region_array, (eh_region)0);
  vec_safe_push (mtcsFunc->currentFun->eh->lp_array, (eh_landing_pad)0);
}

/* Call back from expand_function_end to know where we should put
   the call to unwind_sjlj_unregister_libfunc if needed.  */
//原型 sjlj_emit_function_exit_after except.h except.cc
void mtcs_except_sjlj_emit_function_exit_after (MtcsExcept *self,rtx_insn *after)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  mtcsRtlData/*!crtl*/->eh.sjlj_exit_after = after;
}

/* Expand __builtin_eh_return.  This exit path from the function loads up
   the eh return data registers, adjusts the stack, and branches to a
   given PC other than the normal return address.  */
//原型 expand_eh_return except.h except.cc
void mtcs_except_expand_eh_return (MtcsExcept *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  rtx_code_label *around_label;

  if (! mtcsRtlData/*!crtl*/->eh.ehr_label)
    return;

  mtcsRtlData/*!crtl*/->calls_eh_return = 1;

#ifdef EH_RETURN_STACKADJ_RTX
  mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,EH_RETURN_STACKADJ_RTX, const0_rtx);
#endif

#ifdef EH_RETURN_TAKEN_RTX
  mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,EH_RETURN_TAKEN_RTX, const0_rtx);
#endif

  around_label =mtcs_rtl_gen_label_rtx(mtcsRTL);
  mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,around_label);

  mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,mtcsRtlData/*!crtl*/->eh.ehr_label);
  mtcs_func_clobber_return_register/*!clobber_return_register*/(mtcsFunc);

#ifdef EH_RETURN_STACKADJ_RTX
  mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,EH_RETURN_STACKADJ_RTX, mtcsRtlData/*!crtl*/->eh.ehr_stackadj);
#endif

#ifdef EH_RETURN_TAKEN_RTX
  mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,EH_RETURN_TAKEN_RTX, const1_rtx);
#endif

  if (target_rtx_have_eh_return/*!targetm.have_eh_return*/(mtcsMachine->tmrtx))
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,
              target_rtx_gen_eh_return/*!targetm.gen_eh_return*/(mtcsMachine->tmrtx,mtcsRtlData/*!crtl*/->eh.ehr_handler));
  else{
      if (rtx handler = EH_RETURN_HANDLER_RTX)
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,handler, mtcsRtlData/*!crtl*/->eh.ehr_handler);
      else
          error ("%<__builtin_eh_return%> not supported on this target");
  }

#ifdef EH_RETURN_TAKEN_RTX
  rtx_code_label *eh_done_label =mtcs_rtl_gen_label_rtx(mtcsRTL);
  mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,eh_done_label);
#endif

  mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,around_label);

#ifdef EH_RETURN_TAKEN_RTX
  for (rtx tmp : { EH_RETURN_STACKADJ_RTX, EH_RETURN_HANDLER_RTX })
    if (tmp && REG_P (tmp))
        mtcs_emit_emit_clobber/*!emit_clobber*/(mtcsEmit,tmp);
  mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,eh_done_label);
#endif
}

//原型 set_eh_throw_stmt_table except.h except.cc
void mtcs_except_set_eh_throw_stmt_table (MtcsExcept *self,function *fun, hash_map<gimple *, int> *table)
{
  fun->eh->throw_stmt_table = table;
}

//原型 push_sleb128 except.cc
static void push_sleb128 (vec<uchar, va_gc> **data_area, int value)
{
  unsigned char byte;
  int more;

  do{
      byte = value & 0x7f;
      value >>= 7;
      more = ! ((value == 0 && (byte & 0x40) == 0) || (value == -1 && (byte & 0x40) != 0));
      if (more)
          byte |= 0x80;
      vec_safe_push (*data_area, byte);
  }while (more);
}

//原型 add_action_record except.cc
static int add_action_record (MtcsExcept *self,action_hash_type *ar_hash, int filter, int next)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  struct action_record **slot, *new_ar, tmp;
  tmp.filter = filter;
  tmp.next = next;
  slot = ar_hash->find_slot (&tmp, INSERT);
  if ((new_ar = *slot) == NULL){
      new_ar = XNEW (struct action_record);
      new_ar->offset = mtcsRtlData/*!crtl*/->eh.action_record_data->length () + 1;
      new_ar->filter = filter;
      new_ar->next = next;
      *slot = new_ar;
      /* The filter value goes in untouched.  The link to the next
      record is a "self-relative" byte offset, or zero to indicate
      that there is no next record.  So convert the absolute 1 based
      indices we've been carrying around into a displacement.  */
      push_sleb128 (&mtcsRtlData/*!crtl*/->eh.action_record_data, filter);
      if (next)
          next -= mtcsRtlData/*!crtl*/->eh.action_record_data->length () + 1;
      push_sleb128 (&mtcsRtlData/*!crtl*/->eh.action_record_data, next);
  }
  return new_ar->offset;
}

//原型 collect_one_action_chain except.cc
static int collect_one_action_chain (MtcsExcept *self,action_hash_type *ar_hash, eh_region region)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;

  int next;
  /* If we've reached the top of the region chain, then we have
     no actions, and require no landing pad.  */
  if (region == NULL)
    return -1;

  switch (region->type){
    case ERT_CLEANUP:
      {
        eh_region r;
        /* A cleanup adds a zero filter to the beginning of the chain, but
           there are special cases to look out for.  If there are *only*
           cleanups along a path, then it compresses to a zero action.
           Further, if there are multiple cleanups along a path, we only
           need to represent one of them, as that is enough to trigger
           entry to the landing pad at runtime.  */
        next = collect_one_action_chain/*!collect_one_action_chain*/(self,ar_hash, region->outer);
        if (next <= 0)
          return 0;
        for (r = region->outer; r ; r = r->outer)
          if (r->type == ERT_CLEANUP)
            return next;
        return add_action_record(self,ar_hash, 0, next);
      }

    case ERT_TRY:
      {
        eh_catch c;
        /* Process the associated catch regions in reverse order.
           If there's a catch-all handler, then we don't need to
           search outer regions.  Use a magic -3 value to record
           that we haven't done the outer search.  */
        next = -3;
        for (c = region->u.eh_try.last_catch; c ; c = c->prev_catch){
            if (c->type_list == NULL){
                /* Retrieve the filter from the head of the filter list
                   where we have stored it (see assign_filter_values).  */
                int filter = TREE_INT_CST_LOW (TREE_VALUE (c->filter_list));
                next = add_action_record(self,ar_hash, filter, 0);
            }else{
                /* Once the outer search is done, trigger an action record for
                   each filter we have.  */
                tree flt_node;
                if (next == -3){
                    next = collect_one_action_chain(self,ar_hash, region->outer);
                    /* If there is no next action, terminate the chain.  */
                    if (next == -1)
                       next = 0;
                    /* If all outer actions are cleanups or must_not_throw,
                       we'll have no action record for it, since we had wanted
                       to encode these states in the call-site record directly.
                       Add a cleanup action to the chain to catch these.  */
                    else if (next <= 0)
                       next = add_action_record(self,ar_hash, 0, 0);
                }

                flt_node = c->filter_list;
                for (; flt_node; flt_node = TREE_CHAIN (flt_node)){
                    int filter = TREE_INT_CST_LOW (TREE_VALUE (flt_node));
                    next = add_action_record(self,ar_hash, filter, next);
                }
            }
        }
        return next;
      }

    case ERT_ALLOWED_EXCEPTIONS:
      /* An exception specification adds its filter to the
     beginning of the chain.  */
      next = collect_one_action_chain(self,ar_hash, region->outer);
      /* If there is no next action, terminate the chain.  */
      if (next == -1)
          next = 0;
      /* If all outer actions are cleanups or must_not_throw,
     we'll have no action record for it, since we had wanted
     to encode these states in the call-site record directly.
     Add a cleanup action to the chain to catch these.  */
      else if (next <= 0)
          next = add_action_record(self,ar_hash, 0, 0);
      return add_action_record(self,ar_hash, region->u.allowed.filter, next);

    case ERT_MUST_NOT_THROW:
      /* A must-not-throw region with no inner handlers or cleanups
     requires no call-site entry.  Note that this differs from
     the no handler or cleanup case in that we do require an lsda
     to be generated.  Return a magic -2 value to record this.  */
      return -2;
  }

  gcc_unreachable ();
}

//原型 add_call_site except.cc
static int add_call_site (MtcsExcept *self,rtx landing_pad, int action, int section)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  call_site_record record;
  record = ggc_alloc<call_site_record_d> ();
  record->landing_pad = landing_pad;
  record->action = action;
  vec_safe_push (mtcsRtlData/*!crtl*/->eh.call_site_record_v[section], record);
  return self->call_site_base + mtcsRtlData/*!crtl*/->eh.call_site_record_v[section]->length () - 1;
}

/* Process all active landing pads.  Assign each one a compact dispatch
   index, and a call-site index.  */
//原型 sjlj_assign_call_site_values except.cc
static int sjlj_assign_call_site_values (MtcsExcept *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  action_hash_type ar_hash (31);
  int i, disp_index;
  eh_landing_pad lp;

  vec_alloc (mtcsRtlData/*!crtl*/->eh.action_record_data, 64);

  disp_index = 0;
  self->call_site_base = 1;
  for (i = 1; vec_safe_iterate (cfun->eh->lp_array, i, &lp); ++i)
    if (lp && lp->post_landing_pad){
        int action, call_site;
        /* First: build the action table.  */
        action = collect_one_action_chain(self,&ar_hash, lp->region);
        /* Next: assign call-site values.  If dwarf2 terms, this would be
           the region number assigned by convert_to_eh_region_ranges, but
           handles no-action and must-not-throw differently.  */
        /* Map must-not-throw to otherwise unused call-site index 0.  */
        if (action == -2)
          call_site = 0;
        /* Map no-action to otherwise unused call-site index -1.  */
        else if (action == -1)
          call_site = -1;
        /* Otherwise, look it up in the table.  */
        else
          call_site = add_call_site(self,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,disp_index), action, 0);
        self->sjlj_lp_call_site_index[i] = call_site;

        disp_index++;
    }

  return disp_index;
}

/* Extract all EH information from INSN.  Return true if the insn
   was marked NOTHROW.  */
//原型 get_eh_region_and_lp_from_rtx except.cc
static bool get_eh_region_and_lp_from_rtx (MtcsExcept *self,const_rtx insn, eh_region *pr, eh_landing_pad *plp)
{
  eh_landing_pad lp = NULL;
  eh_region r = NULL;
  bool ret = false;
  rtx note;
  int lp_nr;
  if (! INSN_P (insn))
     goto egress;
  if (NONJUMP_INSN_P (insn)  && GET_CODE (PATTERN (insn)) == SEQUENCE)
     insn = XVECEXP (PATTERN (insn), 0, 0);
  note = find_reg_note (insn, REG_EH_REGION, NULL_RTX);
  if (!note){
      ret = !mtcs_except_insn_could_throw_p/*!insn_could_throw_p*/(self,insn);
      goto egress;
  }
  lp_nr = INTVAL (XEXP (note, 0));
  if (lp_nr == 0 || lp_nr == INT_MIN){
      ret = true;
      goto egress;
  }
  if (lp_nr < 0)
    r = (*cfun->eh->region_array)[-lp_nr];
  else{
      lp = (*cfun->eh->lp_array)[lp_nr];
      r = lp->region;
  }

 egress:
  *plp = lp;
  *pr = r;
  return ret;
}

/* Emit code to record the current call-site index before every
   insn that can throw.  */
//原型 sjlj_mark_call_sites except.cc
static void sjlj_mark_call_sites (MtcsExcept *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsBuiltins   *mtcsBuiltins=mtcs_target_get_builtins(mtcsTarget);

  int last_call_site = -2;
  rtx_insn *insn;
  rtx mem;
  for (insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn ; insn = NEXT_INSN (insn)){
      eh_landing_pad lp;
      eh_region r;
      bool nothrow;
      int this_call_site;
      rtx_insn *before, *p;
      /* Reset value tracking at extended basic block boundaries.  */
      if (LABEL_P (insn))
          last_call_site = -2;
      /* If the function allocates dynamic stack space, the context must
     be updated after every allocation/deallocation accordingly.  */
      if (NOTE_P (insn) && NOTE_KIND (insn) == NOTE_INSN_UPDATE_SJLJ_CONTEXT){
          rtx buf_addr;
          mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
          buf_addr = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,
                  mtcs_mode_get_Pmode(mtcsMode), XEXP (mtcsRtlData/*!crtl*/->eh.sjlj_fc, 0), self->sjlj_fc_jbuf_ofs);
          mtcs_builtins_expand_builtin_update_setjmp_buf/*!expand_builtin_update_setjmp_buf*/(mtcsBuiltins,buf_addr);
          p = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
          mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
          mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,p, insn);
      }

      if (! INSN_P (insn))
          continue;

      nothrow = get_eh_region_and_lp_from_rtx(self,insn, &r, &lp);
      if (nothrow)
          continue;
      if (lp)
          this_call_site = self->sjlj_lp_call_site_index[lp->index];
      else if (r == NULL){
          /* Calls (and trapping insns) without notes are outside any
             exception handling region in this function.  Mark them as
             no action.  */
          this_call_site = -1;
      }else{
          gcc_assert (r->type == ERT_MUST_NOT_THROW);
          this_call_site = 0;
      }
      if (this_call_site != -1)
          mtcsRtlData/*!crtl*/->uses_eh_lsda = 1;
      if (this_call_site == last_call_site)
          continue;
      /* Don't separate a call from it's argument loads.  */
      before = insn;
      if (CALL_P (insn))
          before = find_first_parameter_load (insn, NULL);

      mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
      mem = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,mtcsRtlData/*!crtl*/->eh.sjlj_fc, TYPE_MODE (integer_type_node),
                self->sjlj_fc_call_site_ofs);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mem,
            mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,this_call_site, GET_MODE (mem)));
      p = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

      mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,p, before);
      last_call_site = this_call_site;
  }
}


/* Construct the SjLj_Function_Context.  */
//原型 sjlj_emit_function_enter except.cc
static void sjlj_emit_function_enter (MtcsExcept *self,rtx_code_label *dispatch_label)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsLibfuncs *mtcsLibfuncs =mtcs_target_get_libfuncs(mtcsTarget);
  MtcsBuiltins *mtcsBuiltins =mtcs_target_get_builtins(mtcsTarget);
  MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);

  rtx_insn *fn_begin, *seq;
  rtx fc, mem;
  bool fn_begin_outside_block;
  machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);

  rtx personality = mtcs_expr_get_personality_function/*!get_personality_function*/(mtcsExpr,current_function_decl);
  fc = mtcsRtlData/*!crtl*/->eh.sjlj_fc;
  mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
  /* We're storing this libcall's address into memory instead of
     calling it directly.  Thus, we must call assemble_external_libcall
     here, as we cannot depend on emit_library_call to do it for us.  */
  mtcs_asm_assemble_external_libcall/*!assemble_external_libcall*/(mtcsAsm,personality);
  mem =  mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,fc, pMode, self->sjlj_fc_personality_ofs);
  mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mem, personality);
  mem =  mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,fc, pMode, self->sjlj_fc_lsda_ofs);
  if (mtcsRtlData/*!crtl*/->uses_eh_lsda){
      char buf[20];
      rtx sym;
      mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(mtcsAsm,buf, "LLSDA", current_function_funcdef_no);
      sym = gen_rtx_SYMBOL_REF (pMode, ggc_strdup (buf));
      SYMBOL_REF_FLAGS (sym) = SYMBOL_FLAG_LOCAL;
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mem, sym);
  }else
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mem, const0_rtx);

  if (dispatch_label){
      rtx addr =mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, XEXP (fc, 0), self->sjlj_fc_jbuf_ofs);

#ifdef DONT_USE_BUILTIN_SETJMP
      addr =mtcs_explow_copy_addr_to_reg/*!copy_addr_to_reg*/(mtcsExplow,addr);
      addr =mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,ptr_mode, addr);
      tree addr_tree = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,ptr_type_node, addr);

      tree call_expr = mtcs_tree_build_call_expr/*!build_call_expr*/(mtcsTree,setjmp_fn, 1, addr_tree);
      rtx x = mtcs_calls_expand_call/*!expand_call*/(mtcsCalls,call_expr, NULL_RTX, false);

      mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,x, const0_rtx, NE, 0,
                   TYPE_MODE (integer_type_node), 0,
                   dispatch_label,
                   profile_probability::unlikely ());
#else
      mtcs_builtins_expand_builtin_setjmp_setup/*!expand_builtin_setjmp_setup*/(mtcsBuiltins,addr, dispatch_label);
#endif
  }

  mtcs_calls_emit_library_call/*!emit_library_call*/(mtcsCalls,
          mtcsLibfuncs->x_libfunc_table[LTI_unwind_sjlj_register]/*!unwind_sjlj_register_libfunc*/, LCT_NORMAL, VOIDmode,
             XEXP (fc, 0), pMode);

  seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
  mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
  /* ??? Instead of doing this at the beginning of the function,
     do this in a block that is at loop level 0 and dominates all
     can_throw_internal instructions.  */
  fn_begin_outside_block = true;
  for (fn_begin = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); ; fn_begin = NEXT_INSN (fn_begin))
    if (NOTE_P (fn_begin)){
        if (NOTE_KIND (fn_begin) == NOTE_INSN_FUNCTION_BEG)
          break;
        else if (NOTE_INSN_BASIC_BLOCK_P (fn_begin))
          fn_begin_outside_block = false;
    }

#ifdef DONT_USE_BUILTIN_SETJMP
  if (dispatch_label){
      /* The sequence contains a branch in the middle so we need to force
     the creation of a new basic block by means of BB_SUPERBLOCK.  */
      if (fn_begin_outside_block){
          basic_block bb  = mtcs_cfg_context_split_edge/*!split_edge*/(mtcsCfgContext,
                single_succ_edge (ENTRY_BLOCK_PTR_FOR_FN (cfun)));
          if (JUMP_P (BB_END (bb)))
             mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,seq, BB_END (bb));
          else
             mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,seq, BB_END (bb));
      }else
         mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,seq, fn_begin);

      single_succ (ENTRY_BLOCK_PTR_FOR_FN (cfun))->flags |= BB_SUPERBLOCK;
      return;
  }
#endif

  if (fn_begin_outside_block)
      mtcs_cfg_rtl_insert_insn_on_edge/*!insert_insn_on_edge*/(mtcsCfgRtl,
              seq, single_succ_edge (ENTRY_BLOCK_PTR_FOR_FN (cfun)));
  else
     mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,seq, fn_begin);
}

/* Emit SEQ into basic block just before INSN (that is assumed to be
   first instruction of some existing BB and return the newly
   produced block.  */
//原型 emit_to_new_bb_before except.cc
static basic_block emit_to_new_bb_before (MtcsExcept *self,rtx_insn *seq, rtx_insn *insn)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  rtx_insn *next, *last;
  basic_block bb;
  edge e;
  edge_iterator ei;
  /* If there happens to be a fallthru edge (possibly created by cleanup_cfg
     call), we don't want it to go into newly created landing pad or other EH
     construct.  */
  for (ei = ei_start (BLOCK_FOR_INSN (insn)->preds); (e = ei_safe_edge (ei)); )
    if (e->flags & EDGE_FALLTHRU)
       mtcs_cfg_context_force_nonfallthru/*!force_nonfallthru*/(mtcsCfgContext,e);
    else
      ei_next (&ei);

  /* Make sure to put the location of INSN or a subsequent instruction on SEQ
     to avoid inheriting the location of the previous instruction.  */
  next = insn;
  while (next && !NONDEBUG_INSN_P (next))
    next = NEXT_INSN (next);
  if (next)
    last = emit_insn_before_setloc (seq, insn, INSN_LOCATION (next));
  else
    last = mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,seq, insn);
  if (BARRIER_P (last))
    last = PREV_INSN (last);
  bb =mtcs_cfg_context_create_basic_block/*!create_basic_block*/(mtcsCfgContext,
        seq, last, BLOCK_FOR_INSN (insn)->prev_bb);
  mtcs_cfg_rtl_update_bb_for_insn/*!update_bb_for_insn*/(mtcsCfgRtl,bb);
  bb->flags |= BB_SUPERBLOCK;
  return bb;
}

//原型 sjlj_emit_dispatch_table except.cc
static void sjlj_emit_dispatch_table (MtcsExcept *self,rtx_code_label *dispatch_label, int num_dispatch)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsStmt *mtcsStmt=mtcs_target_get_stmt(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsBuiltins *mtcsBuiltins=mtcs_target_get_builtins(mtcsTarget);
  MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
  MtcsCfg *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
  MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

  scalar_int_mode unwind_word_mode =mtcs_mode_unwind_word_mode/*!targetm.unwind_word_mode*/(mtcsMode);
  scalar_int_mode filter_mode =mtcs_mode_eh_return_filter_mode/*!targetm.eh_return_filter_mode*/(mtcsMode);
  eh_landing_pad lp;
  rtx mem, fc, exc_ptr_reg, filter_reg;
  rtx_insn *seq;
  basic_block bb;
  eh_region r;
  int i, disp_index;
  vec<tree> dispatch_labels = vNULL;

  fc = mtcsRtlData/*!crtl*/->eh.sjlj_fc;

  mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);

  mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,dispatch_label);

#ifndef DONT_USE_BUILTIN_SETJMP
  mtcs_builtins_expand_builtin_setjmp_receiver/*!expand_builtin_setjmp_receiver*/(mtcsBuiltins,dispatch_label);

  /* The caller of expand_builtin_setjmp_receiver is responsible for
     making sure that the label doesn't vanish.  The only other caller
     is the expander for __builtin_setjmp_receiver, which places this
     label on the nonlocal_goto_label list.  Since we're modeling these
     CFG edges more exactly, we can use the forced_labels list instead.  */
  LABEL_PRESERVE_P (dispatch_label) = 1;
  vec_safe_push<rtx_insn *> (forced_labels, dispatch_label);
#endif

  /* Load up exc_ptr and filter values from the function context.  */
  mem = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,fc, unwind_word_mode, self->sjlj_fc_data_ofs);
  if (unwind_word_mode != ptr_mode){
     if(mtcs_config_ifdef(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED/*!#ifdef POINTERS_EXTEND_UNSIGNED*/))
        mem = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,ptr_mode, mem);
     else/*!#else*/
        mem = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,ptr_mode, mem, 0);
     /*!#endif*/
  }
  exc_ptr_reg = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,ptr_mode, mem);

  mem = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,fc, unwind_word_mode,
            self->sjlj_fc_data_ofs + mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,unwind_word_mode));
  if (unwind_word_mode != filter_mode)
     mem =mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,filter_mode, mem, 0);
  filter_reg = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,filter_mode, mem);

  /* Jump to one of the directly reachable regions.  */
  disp_index = 0;
  rtx_code_label *first_reachable_label = NULL;

  /* If there's exactly one call site in the function, don't bother
     generating a switch statement.  */
  if (num_dispatch > 1)
     dispatch_labels.create (num_dispatch);

  for (i = 1; vec_safe_iterate (cfun->eh->lp_array, i, &lp); ++i)
     if (lp && lp->post_landing_pad){
        rtx_insn *seq2;
        rtx_code_label *label;
        mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
        lp->landing_pad = dispatch_label;
        if (num_dispatch > 1){
            tree t_label, case_elt, t;
            t_label = create_artificial_label (UNKNOWN_LOCATION);
            t = mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,integer_type_node, disp_index);
            case_elt = build_case_label (t, NULL, t_label);
            dispatch_labels.quick_push (case_elt);
            label = mtcs_stmt_jump_target_rtx/*!jump_target_rtx*/(mtcsStmt,t_label);
        }else
            label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);

        if (disp_index == 0)
           first_reachable_label = label;
        mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);

        r = lp->region;
        if (r->exc_ptr_reg)
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,r->exc_ptr_reg, exc_ptr_reg);
        if (r->filter_reg)
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,r->filter_reg, filter_reg);

        seq2 = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
        mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

        rtx_insn *before = mtcs_stmt_label_rtx/*!label_rtx*/(mtcsStmt,lp->post_landing_pad);
        bb = emit_to_new_bb_before(self,seq2, before);
        mtcs_cfg_make_single_succ_edge/*!make_single_succ_edge*/(mtcsCfg,bb, bb->next_bb, EDGE_FALLTHRU);
        if (current_loops){
            class loop *loop = bb->next_bb->loop_father;
            /* If we created a pre-header block, add the new block to the
               outer loop, otherwise to the loop itself.  */
            if (bb->next_bb == loop->header)
              add_bb_to_loop (bb, loop_outer (loop));
            else
              add_bb_to_loop (bb, loop);
            /* ???  For multiple dispatches we will end up with edges
               from the loop tree root into this loop, making it a
               multiple-entry loop.  Discard all affected loops.  */
            if (num_dispatch > 1){
                for (loop = bb->loop_father;
                     loop_outer (loop); loop = loop_outer (loop))
                  mark_loop_for_removal (loop);
            }
        }

        disp_index++;
     }
  gcc_assert (disp_index == num_dispatch);

  if (num_dispatch > 1){
      rtx disp = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,fc, TYPE_MODE (integer_type_node),
                 self->sjlj_fc_call_site_ofs);
      mtcs_stmt_expand_sjlj_dispatch_table/*!expand_sjlj_dispatch_table*/(mtcsStmt,disp, dispatch_labels);
  }
  seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
  mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
  bb = emit_to_new_bb_before(self,seq, first_reachable_label);
  if (num_dispatch == 1){
      mtcs_cfg_make_single_succ_edge/*!make_single_succ_edge*/(mtcsCfg,bb, bb->next_bb, EDGE_FALLTHRU);
      if (current_loops){
          class loop *loop = bb->next_bb->loop_father;
          /* If we created a pre-header block, add the new block to the
             outer loop, otherwise to the loop itself.  */
          if (bb->next_bb == loop->header)
            add_bb_to_loop (bb, loop_outer (loop));
          else
            add_bb_to_loop (bb, loop);
      }
  }else{
      /* We are not wiring up edges here, but as the dispatcher call
         is at function begin simply associate the block with the
     outermost (non-)loop.  */
      if (current_loops)
          add_bb_to_loop (bb, current_loops->tree_root);
  }
}

//原型 sjlj_emit_function_exit except.cc
static void sjlj_emit_function_exit (MtcsExcept *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
  MtcsLibfuncs *mtcsLibfuncs =mtcs_target_get_libfuncs(mtcsTarget);

  rtx_insn *seq, *insn;
  mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
  mtcs_calls_emit_library_call/*!emit_library_call*/(mtcsCalls,
          mtcsLibfuncs->x_libfunc_table[LTI_unwind_sjlj_unregister]/*!unwind_sjlj_unregister_libfunc*/, LCT_NORMAL, VOIDmode,
             XEXP (mtcsRtlData/*!crtl*/->eh.sjlj_fc, 0), mtcs_mode_get_Pmode(mtcsMode));

  seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
  mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
  /* ??? Really this can be done in any block at loop level 0 that
     post-dominates all can_throw_internal instructions.  This is
     the last possible moment.  */
  insn = mtcsRtlData/*!crtl*/->eh.sjlj_exit_after;
  if (LABEL_P (insn))
    insn = NEXT_INSN (insn);

  mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,seq, insn);
}


//原型 sjlj_build_landing_pads except.cc
static void sjlj_build_landing_pads (MtcsExcept *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
    MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
    MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
    MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
    MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
    MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
    MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
    MtcsOptionsItem *opts=mtcsOptions->global_options;
    MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);

  int num_dispatch;

  num_dispatch = vec_safe_length (cfun->eh->lp_array);
  if (num_dispatch == 0)
    return;
  self->sjlj_lp_call_site_index.safe_grow_cleared (num_dispatch, true);

  num_dispatch = sjlj_assign_call_site_values(self);
  if (num_dispatch > 0){
      rtx_code_label *dispatch_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
      int align = mtcs_align_get_stack_slot_alignment/*!STACK_SLOT_ALIGNMENT*/(mtcsAlign,self->sjlj_fc_type_node,
                    TYPE_MODE (self->sjlj_fc_type_node),TYPE_ALIGN (self->sjlj_fc_type_node));
      mtcsRtlData/*!crtl*/->eh.sjlj_fc =mtcs_func_assign_stack_local/*!assign_stack_local*/(mtcsFunc,
              TYPE_MODE (self->sjlj_fc_type_node),int_size_in_bytes (self->sjlj_fc_type_node),align);
      sjlj_mark_call_sites(self);
      sjlj_emit_function_enter(self,dispatch_label);
      sjlj_emit_dispatch_table(self,dispatch_label, num_dispatch);
      sjlj_emit_function_exit(self);
  }

  /* If we do not have any landing pads, we may still need to register a
     personality routine and (empty) LSDA to handle must-not-throw regions.  */
  else if (function_needs_eh_personality (cfun) != eh_personality_none){
      int align = STACK_SLOT_ALIGNMENT (self->sjlj_fc_type_node,
                    TYPE_MODE (self->sjlj_fc_type_node),
                    TYPE_ALIGN (self->sjlj_fc_type_node));
     mtcsRtlData/*!crtl*/->eh.sjlj_fc  = mtcs_func_assign_stack_local/*!assign_stack_local*/(mtcsFunc,
             TYPE_MODE (self->sjlj_fc_type_node), int_size_in_bytes (self->sjlj_fc_type_node),align);

      sjlj_mark_call_sites(self);
      sjlj_emit_function_enter (self,NULL);
      sjlj_emit_function_exit(self);
  }

  self->sjlj_lp_call_site_index.release ();
}

/* A subroutine of dw2_build_landing_pads, also used for edge splitting
   at the rtl level.  Emit the code required by the target at a landing
   pad for the given region.  */
//原型 expand_dw2_landing_pad_for_region except.cc

static void expand_dw2_landing_pad_for_region (MtcsExcept *self,eh_region region)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

   if (target_rtx_have_exception_receiver/*!targetm.have_exception_receiver*/(mtcsMachine->tmrtx))
      emit_insn (target_rtx_gen_exception_receiver/*!targetm.gen_exception_receiver*/(mtcsMachine->tmrtx));
   else if (target_rtx_have_nonlocal_goto_receiver/*!targetm.have_nonlocal_goto_receiver*/(mtcsMachine->tmrtx))
      emit_insn (target_rtx_gen_nonlocal_goto_receiver/*!targetm.gen_nonlocal_goto_receiver*/(mtcsMachine->tmrtx));
   else
   { /* Nothing */ }

   if (region->exc_ptr_reg)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,region->exc_ptr_reg,
         mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,ptr_mode,
         mtcs_reg_get_eh_return_data_regno/*!EH_RETURN_DATA_REGNO*/(mtcsReg,0)));
   if (region->filter_reg)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,region->filter_reg,
         mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mtcs_mode_eh_return_filter_mode/*!targetm.eh_return_filter_mode*/(mtcsMode),
         mtcs_reg_get_eh_return_data_regno/*!EH_RETURN_DATA_REGNO*/(mtcsReg,1)));
}


/* Expand the extra code needed at landing pads for dwarf2 unwinding.  */
//原型 dw2_build_landing_pads except.cc
static void dw2_build_landing_pads (MtcsExcept *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsCfg *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
  MtcsStmt *mtcsStmt=mtcs_target_get_stmt(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *opts=mtcsOptions->global_options;

  int i;
  eh_landing_pad lp;
  int e_flags = EDGE_FALLTHRU;
  /* If we're going to partition blocks, we need to be able to add
     new landing pads later, which means that we need to hold on to
     the post-landing-pad block.  Prevent it from being merged away.
     We'll remove this bit after partitioning.  */
  if (opts->x_flag_reorder_blocks_and_partition)
     e_flags |= EDGE_PRESERVE;
  for (i = 1; vec_safe_iterate (cfun->eh->lp_array, i, &lp); ++i){
      basic_block bb;
      rtx_insn *seq;
      if (lp == NULL || lp->post_landing_pad == NULL)
          continue;
      mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
      lp->landing_pad = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,lp->landing_pad);
      LABEL_PRESERVE_P (lp->landing_pad) = 1;
      expand_dw2_landing_pad_for_region(self,lp->region);
      seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      bb = emit_to_new_bb_before(self,seq, mtcs_stmt_label_rtx/*!label_rtx*/(mtcsStmt,lp->post_landing_pad));
      bb->count = bb->next_bb->count;
      mtcs_cfg_make_single_succ_edge/*!make_single_succ_edge*/(mtcsCfg,bb, bb->next_bb, e_flags);
      if (current_loops){
          class loop *loop = bb->next_bb->loop_father;
          /* If we created a pre-header block, add the new block to the
             outer loop, otherwise to the loop itself.  */
          if (bb->next_bb == loop->header)
             add_bb_to_loop (bb, loop_outer (loop));
          else
             add_bb_to_loop (bb, loop);
      }
  }
}


/* After initial rtl generation, call back to finish generating
   exception support code.  */
//原型 finish_eh_generation except.h except.cc
void mtcs_except_finish_eh_generation (MtcsExcept *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
  MtcsCfg *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
  MtcsStmt *mtcsStmt=mtcs_target_get_stmt(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *opts=mtcsOptions->global_options;

  basic_block bb;
  /* Construct the landing pads.  */
  if (target_common_except_unwind_info/*!targetm_common.except_unwind_info*/(mtcsMachine->common,
          opts/*!&global_options*/) == UI_SJLJ)
    sjlj_build_landing_pads(self);
  else
    dw2_build_landing_pads(self);

  mtcs_cfg_rtl_break_superblocks/*!break_superblocks*/(mtcsCfgRtl);
  /* Redirect all EH edges from the post_landing_pad to the landing pad.  */
  FOR_EACH_BB_FN (bb, cfun){
      eh_landing_pad lp;
      edge_iterator ei;
      edge e;
      lp = get_eh_landing_pad_from_rtx (BB_END (bb));
      FOR_EACH_EDGE (e, ei, bb->succs)
          if (e->flags & EDGE_EH)
              break;
      /* We should not have generated any new throwing insns during this
      pass, and we should not have lost any EH edges, so we only need
      to handle two cases here:
      (1) reachable handler and an existing edge to post-landing-pad,
      (2) no reachable handler and no edge.  */
      gcc_assert ((lp != NULL) == (e != NULL));
      if (lp != NULL){
          gcc_assert (BB_HEAD (e->dest) == mtcs_stmt_label_rtx/*!label_rtx*/(mtcsStmt,lp->post_landing_pad));
          mtcs_cfg_redirect_edge_succ/*!redirect_edge_succ*/(mtcsCfg,e, BLOCK_FOR_INSN (lp->landing_pad));
          e->flags |= (CALL_P (BB_END (bb))
                   ? EDGE_ABNORMAL | EDGE_ABNORMAL_CALL : EDGE_ABNORMAL);
      }
  }

  if (target_common_except_unwind_info/*!targetm_common.except_unwind_info*/(mtcsMachine->common,
          opts/*!&global_options*/) == UI_SJLJ
      /* Kludge for Alpha (see alpha_gp_save_rtx).  */
      || single_succ_edge (ENTRY_BLOCK_PTR_FOR_FN (cfun))->insns.r)
      mtcs_cfg_rtl_commit_edge_insertions/*!commit_edge_insertions*/(mtcsCfgRtl);
}

/* Do any necessary initialization to access arbitrary stack frames.
   On the SPARC, this means flushing the register windows.  */
//原型 expand_builtin_unwind_init except.h except.cc
void mtcs_except_expand_builtin_unwind_init (MtcsExcept *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  /* Set this so all the registers get saved in our frame; we need to be
     able to copy the saved values for any registers from frames we unwind.  */
   mtcsRtlData/*!crtl*/->saves_all_registers = 1;
   mtcs_func_setup_frame_addresses/*!SETUP_FRAME_ADDRESSES*/(mtcsFunc);
}

/* Given an actual address in addr_tree, do any necessary encoding
   and return the value to be stored in the return address register or
   stack slot so the epilogue will return to that address.  */
//原型 expand_builtin_frob_return_addr except.h except.cc
rtx mtcs_except_expand_builtin_frob_return_addr (MtcsExcept *self,tree addr_tree)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

  rtx addr = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,addr_tree, NULL_RTX, ptr_mode, EXPAND_NORMAL);
  machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
  scalar_int_mode scalarPmode=mtcs_mode_as_a<scalar_int_mode>(mtcsMode,pMode);
  addr =mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,scalarPmode, addr);
  int returnAddrOffset=mtcs_func_get_return_addr_offset/*!RETURN_ADDR_OFFSET*/(mtcsFunc);
  if (returnAddrOffset/*!RETURN_ADDR_OFFSET*/){
      addr = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,pMode, addr);
      addr = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, addr, -returnAddrOffset/*!RETURN_ADDR_OFFSET*/);
  }
  return addr;
}

/* Given a value extracted from the return address register or stack slot,
   return the actual address encoded in that value.  */
//原型 expand_builtin_extract_return_addr except.h except.cc
rtx mtcs_except_expand_builtin_extract_return_addr (MtcsExcept *self,tree addr_tree)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);

   int returnAddrOffset=mtcs_func_get_return_addr_offset/*!RETURN_ADDR_OFFSET*/(mtcsFunc);
   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
   scalar_int_mode scalarPmode=mtcs_mode_as_a<scalar_int_mode>(mtcsMode,pMode);

  rtx addr = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,addr_tree, NULL_RTX, pMode, EXPAND_NORMAL);

  if (GET_MODE (addr) != Pmode  && GET_MODE (addr) != VOIDmode){
//#ifdef POINTERS_EXTEND_UNSIGNED
     if(mtcs_config_ifdef(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED))
        addr =mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,scalarPmode, addr);
     else
//#else
        addr = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,pMode, addr, 0);
//#endif
  }

  /* First mask out any unwanted bits.  */
  rtx mask = MASK_RETURN_ADDR;
  if (mask)
     mtcs_expmed_expand_and/*!expand_and*/(mtcsExpmed,pMode, addr, mask, addr);

  /* Then adjust to find the real return address.  */
  if (returnAddrOffset/*!RETURN_ADDR_OFFSET*/)
    addr = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, addr, returnAddrOffset/*!RETURN_ADDR_OFFSET*/);

  return addr;
}

/* Map a non-negative number to an eh return data register number; expands
   to -1 if no return data register is associated with the input number.
   At least the inputs 0 and 1 must be mapped; the target may provide more.  */
//原型 expand_builtin_eh_return_data_regno except.h except.cc
rtx mtcs_except_expand_builtin_eh_return_data_regno (MtcsExcept *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg = mtcs_target_get_reg(mtcsTarget);

   tree which = CALL_EXPR_ARG (exp, 0);
   unsigned HOST_WIDE_INT iwhich;
   if (TREE_CODE (which) != INTEGER_CST){
      error ("argument of %<__builtin_eh_return_regno%> must be constant");
      return constm1_rtx;
   }

   if (!tree_fits_uhwi_p (which))
      return constm1_rtx;

   iwhich = tree_to_uhwi (which);
   iwhich = EH_RETURN_DATA_REGNO (iwhich);
   if (iwhich == INVALID_REGNUM)
      return constm1_rtx;

   if(mtcsReg->get_dwarf_frame_regnum)/*!#ifdef DWARF_FRAME_REGNUM*/
      iwhich = mtcs_reg_get_dwarf_frame_regnum/*!DWARF_FRAME_REGNUM*/(mtcsReg,iwhich);
   else //#else
      iwhich = mtcs_reg_get_debugger_regno/*!DEBUGGER_REGNO*/(mtcsReg,iwhich);
   //#endif

   return mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,iwhich);
}

/* Set up the epilogue with the magic bits we'll need to return to the
   exception handler.  */
//原型 expand_builtin_eh_return except.h except.cc
void mtcs_except_expand_builtin_eh_return (MtcsExcept *self,tree stackadj_tree ATTRIBUTE_UNUSED,
           tree handler_tree)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
   scalar_int_mode scalarPmode=mtcs_mode_as_a<scalar_int_mode>(mtcsMode,pMode);
  rtx tmp;

#ifdef EH_RETURN_STACKADJ_RTX
  tmp = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,stackadj_tree, mtcsRtlData/*!crtl*/->eh.ehr_stackadj,
           VOIDmode, EXPAND_NORMAL);
  tmp = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,scalarPmode, tmp);
  if (!mtcsRtlData/*!crtl*/->eh.ehr_stackadj)
     mtcsRtlData/*!crtl*/->eh.ehr_stackadj =mtcs_explow_copy_addr_to_reg/*!copy_addr_to_reg*/(mtcsExplow,tmp);
  else if (tmp != mtcsRtlData/*!crtl*/->eh.ehr_stackadj)
     mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mtcsRtlData/*!crtl*/->eh.ehr_stackadj, tmp);
#endif

  tmp = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,handler_tree, mtcsRtlData/*!crtl*/->eh.ehr_handler,
           VOIDmode, EXPAND_NORMAL);
  tmp = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,scalarPmode, tmp);
  if (!mtcsRtlData/*!crtl*/->eh.ehr_handler)
     mtcsRtlData/*!crtl*/->eh.ehr_handler = mtcs_explow_copy_addr_to_reg/*!copy_addr_to_reg*/(mtcsExplow,tmp);
  else if (tmp != mtcsRtlData/*!crtl*/->eh.ehr_handler)
     mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mtcsRtlData/*!crtl*/->eh.ehr_handler, tmp);

  if (!mtcsRtlData/*!crtl*/->eh.ehr_label)
     mtcsRtlData/*!crtl*/->eh.ehr_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
  mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,mtcsRtlData/*!crtl*/->eh.ehr_label);
}

/* Convert a ptr_mode address ADDR_TREE to a Pmode address controlled by
   POINTERS_EXTEND_UNSIGNED and return it.  */
//原型 expand_builtin_extend_pointer except.h except.cc
rtx mtcs_except_expand_builtin_extend_pointer (MtcsExcept *self,tree addr_tree)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);

   rtx addr = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,addr_tree, NULL_RTX, ptr_mode, EXPAND_NORMAL);
   int extend;

   //#ifdef POINTERS_EXTEND_UNSIGNED
   if(mtcs_config_ifdef(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED))
      extend = mtcs_config_get_value/*!POINTERS_EXTEND_UNSIGNED*/(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED);
   //#else
   /* The previous EH code did an unsigned extend by default, so we do this also
   for consistency.  */
   else
      extend = 1;
   //#endif

   return mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,
                        mtcsTarget/*!targetm.unwind_word_mode*/->unwind_word_mode(mtcsTarget), ptr_mode, addr, extend);
}

/* Various hooks for unwind library.  */

/* Expand the EH support builtin functions:
   __builtin_eh_pointer and __builtin_eh_filter.  */
//原型 expand_builtin_eh_common except.cc
static eh_region expand_builtin_eh_common (MtcsExcept *self,tree region_nr_t)
{

  HOST_WIDE_INT region_nr;
  eh_region region;
  gcc_assert (tree_fits_shwi_p (region_nr_t));
  region_nr = tree_to_shwi (region_nr_t);
  region = (*cfun->eh->region_array)[region_nr];
  /* ??? We shouldn't have been able to delete a eh region without
     deleting all the code that depended on it.  */
  gcc_assert (region != NULL);
  return region;
}

/* Expand to the exc_ptr value from the given eh region.  */
//原型 expand_builtin_eh_pointer except.h except.cc
rtx mtcs_except_expand_builtin_eh_pointer (MtcsExcept *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   eh_region region= expand_builtin_eh_common(self,CALL_EXPR_ARG (exp, 0));
   if (region->exc_ptr_reg == NULL)
      region->exc_ptr_reg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,ptr_mode);
   return region->exc_ptr_reg;
}

/* Expand to the filter value from the given eh region.  */
//原型 expand_builtin_eh_filter except.h except.cc
rtx mtcs_except_expand_builtin_eh_filter (MtcsExcept *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  eh_region region= expand_builtin_eh_common (self,CALL_EXPR_ARG (exp, 0));
  if (region->filter_reg == NULL)
    region->filter_reg =mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,
          mtcsTarget/*!targetm.eh_return_filter_mode*/->eh_return_filter_mode(mtcsTarget));
  return region->filter_reg;
}

/* Copy the exc_ptr and filter values from one landing pad's registers
   to another.  This is used to inline the resx statement.  */
//原型 expand_builtin_eh_copy_values except.h except.cc
rtx mtcs_except_expand_builtin_eh_copy_values (MtcsExcept *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

  eh_region dst = expand_builtin_eh_common (self,CALL_EXPR_ARG (exp, 0));
  eh_region src = expand_builtin_eh_common (self,CALL_EXPR_ARG (exp, 1));
  scalar_int_mode fmode = mtcsTarget/*!targetm.eh_return_filter_mode*/->eh_return_filter_mode(mtcsTarget);

  if (dst->exc_ptr_reg == NULL)
    dst->exc_ptr_reg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,ptr_mode);
  if (src->exc_ptr_reg == NULL)
    src->exc_ptr_reg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,ptr_mode);

  if (dst->filter_reg == NULL)
    dst->filter_reg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,fmode);
  if (src->filter_reg == NULL)
    src->filter_reg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,fmode);

  mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,dst->exc_ptr_reg, src->exc_ptr_reg);
  mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,dst->filter_reg, src->filter_reg);

  return const0_rtx;
}

//原型 init_eh topleve.h except.cc
void mtcs_except_init_eh (MtcsExcept *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsTree *mtcsTree = mtcs_target_get_tree(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  if (! mtcsOptionsItem->x_flag_exceptions)
    return;

  self->type_to_runtime_map = hash_map<tree_hash, tree>::create_ggc (31);

  /* Create the SjLj_Function_Context structure.  This should match
     the definition in unwind-sjlj.c.  */
  if (target_common_except_unwind_info/*!targetm_common.except_unwind_info*/(mtcsMachine->common,
        mtcsOptionsItem) == UI_SJLJ){
      tree f_jbuf, f_per, f_lsda, f_prev, f_cs, f_data, tmp;

      self->sjlj_fc_type_node = lang_hooks.types.make_type (RECORD_TYPE);

      f_prev = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,BUILTINS_LOCATION, FIELD_DECL, get_identifier ("__prev"),
            build_pointer_type (self->sjlj_fc_type_node));
      DECL_FIELD_CONTEXT (f_prev) = self->sjlj_fc_type_node;

      f_cs = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,BUILTINS_LOCATION, FIELD_DECL, get_identifier ("__call_site"),integer_type_node);
      DECL_FIELD_CONTEXT (f_cs) = self->sjlj_fc_type_node;

      tmp = build_index_type (size_int (4 - 1));
      tmp = build_array_type (lang_hooks.types.type_for_mode(targetm.unwind_word_mode (), 1),tmp);
      f_data = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,BUILTINS_LOCATION,FIELD_DECL, get_identifier ("__data"), tmp);
      DECL_FIELD_CONTEXT (f_data) = self->sjlj_fc_type_node;

      f_per = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,BUILTINS_LOCATION, FIELD_DECL, get_identifier ("__personality"),ptr_type_node);
      DECL_FIELD_CONTEXT (f_per) = self->sjlj_fc_type_node;

      f_lsda = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,BUILTINS_LOCATION,FIELD_DECL, get_identifier ("__lsda"),ptr_type_node);
      DECL_FIELD_CONTEXT (f_lsda) = self->sjlj_fc_type_node;

#ifdef DONT_USE_BUILTIN_SETJMP
#ifdef JMP_BUF_SIZE
      tmp = size_int (JMP_BUF_SIZE - 1);
#else
      /* Should be large enough for most systems, if it is not,
    JMP_BUF_SIZE should be defined with the proper value.  It will
    also tend to be larger than necessary for most systems, a more
    optimal port will define JMP_BUF_SIZE.  */
      tmp = size_int (mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg) + 2 - 1);
#endif
#else
      /* Compute a minimally sized jump buffer.  We need room to store at
    least 3 pointers - stack pointer, frame pointer and return address.
    Plus for some targets we need room for an extra pointer - in the
    case of MIPS this is the global pointer.  This makes a total of four
    pointers, but to be safe we actually allocate room for 5.

    If pointers are smaller than words then we allocate enough room for
    5 words, just in case the backend needs this much room.  For more
    discussion on this issue see:
    http://gcc.gnu.org/ml/gcc-patches/2014-05/msg00313.html.  */
      if (POINTER_SIZE > BITS_PER_WORD)
         tmp = size_int (5 - 1);
      else
         tmp = size_int ((5 * BITS_PER_WORD / POINTER_SIZE) - 1);
#endif

      tmp = build_index_type (tmp);
      tmp = build_array_type (ptr_type_node, tmp);
      f_jbuf = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,BUILTINS_LOCATION,FIELD_DECL, get_identifier ("__jbuf"), tmp);
#ifdef DONT_USE_BUILTIN_SETJMP //host=0 nvptx=0
      /* We don't know what the alignment requirements of the
    runtime's jmp_buf has.  Overestimate.  */
      SET_DECL_ALIGN (f_jbuf, BIGGEST_ALIGNMENT);
      DECL_USER_ALIGN (f_jbuf) = 1;
#endif
      DECL_FIELD_CONTEXT (f_jbuf) = self->sjlj_fc_type_node;

      TYPE_FIELDS (self->sjlj_fc_type_node) = f_prev;
      TREE_CHAIN (f_prev) = f_cs;
      TREE_CHAIN (f_cs) = f_data;
      TREE_CHAIN (f_data) = f_per;
      TREE_CHAIN (f_per) = f_lsda;
      TREE_CHAIN (f_lsda) = f_jbuf;

      layout_type (self->sjlj_fc_type_node);

      /* Cache the interesting field offsets so that we have
    easy access from rtl.  */
      self->sjlj_fc_call_site_ofs  = (tree_to_uhwi (DECL_FIELD_OFFSET (f_cs))
      + tree_to_uhwi (DECL_FIELD_BIT_OFFSET (f_cs)) / BITS_PER_UNIT);
      self->sjlj_fc_data_ofs = (tree_to_uhwi (DECL_FIELD_OFFSET (f_data))
      + tree_to_uhwi (DECL_FIELD_BIT_OFFSET (f_data)) / BITS_PER_UNIT);
      self->sjlj_fc_personality_ofs = (tree_to_uhwi (DECL_FIELD_OFFSET (f_per))
      + tree_to_uhwi (DECL_FIELD_BIT_OFFSET (f_per)) / BITS_PER_UNIT);
      self->sjlj_fc_lsda_ofs = (tree_to_uhwi (DECL_FIELD_OFFSET (f_lsda))
      + tree_to_uhwi (DECL_FIELD_BIT_OFFSET (f_lsda)) / BITS_PER_UNIT);
      self->sjlj_fc_jbuf_ofs  = (tree_to_uhwi (DECL_FIELD_OFFSET (f_jbuf))
      + tree_to_uhwi (DECL_FIELD_BIT_OFFSET (f_jbuf)) / BITS_PER_UNIT);

#ifdef DONT_USE_BUILTIN_SETJMP
      tmp = build_function_type_list (integer_type_node, TREE_TYPE (f_jbuf),
                  NULL);
      setjmp_fn = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,BUILTINS_LOCATION, FUNCTION_DECL,
               get_identifier ("setjmp"), tmp);
      TREE_PUBLIC (setjmp_fn) = 1;
      DECL_EXTERNAL (setjmp_fn) = 1;
      DECL_ASSEMBLER_NAME (setjmp_fn);
#endif
    }
}

/* Return true if INSN could throw, assuming no REG_EH_REGION note
   to the contrary.  */
//原型 insn_could_throw_p rtl.h except.cc
bool mtcs_except_insn_could_throw_p (MtcsExcept *self,const_rtx insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   if (!mtcsOptionsItem->x_flag_exceptions)
      return false;
   if (CALL_P (insn))
      return true;
   if (INSN_P (insn) && cfun->can_throw_non_call_exceptions)
      return mtcs_rtlanal_may_trap_p/*!may_trap_p*/(mtcsRtlanal,PATTERN (insn));
   return false;
}

/* Likewise, but iterate backward.  */
//原型 copy_reg_eh_region_note_backward rtl.h except.cc
void mtcs_except_copy_reg_eh_region_note_backward (MtcsExcept *self,rtx note_or_insn, rtx_insn *last, rtx first)
{
   rtx_insn *insn;
   rtx note = note_or_insn;

   if (INSN_P (note_or_insn)){
      note = find_reg_note (note_or_insn, REG_EH_REGION, NULL_RTX);
      if (note == NULL)
         return;
   }else if (is_a <rtx_insn *> (note_or_insn))
      return;
   note = XEXP (note, 0);

   for (insn = last; insn != first; insn = PREV_INSN (insn))
      if (mtcs_except_insn_could_throw_p/*!insn_could_throw_p*/(self,insn))
         add_reg_note (insn, REG_EH_REGION, note);
}

/* Return the region to which INSN may go, or NULL if it does not
   have a reachable region within this function.  */
//原型 get_eh_region_from_rtx except.h except.cc
eh_region mtcs_except_get_eh_region_from_rtx (MtcsExcept *self,const_rtx insn)
{
   eh_landing_pad lp;
   eh_region r;

   get_eh_region_and_lp_from_rtx(self,insn, &r, &lp);
   return r;
}

/* Return true if INSN can perform a non-local goto.  */
/* ??? This test is here in this file because it (ab)uses REG_EH_REGION.  */
//原型 can_nonlocal_goto rtl.h except.cc
bool mtcs_except_can_nonlocal_goto (MtcsExcept *self,const rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   if (mtcsRtlData/*!nonlocal_goto_handler_labels*/->x_nonlocal_goto_handler_labels && CALL_P (insn)){
      rtx note = find_reg_note (insn, REG_EH_REGION, NULL_RTX);
      if (!note || INTVAL (XEXP (note, 0)) != INT_MIN)
         return true;
   }
   return false;
}


static rtx_note *emit_note_eh_region_end (MtcsExcept *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit = mtcs_target_get_emit(mtcsTarget);

   return mtcs_emit_emit_note_after/*!emit_note_after*/(mtcsEmit,NOTE_INSN_EH_REGION_END, insn);
}


/* Add NOP after NOTE_INSN_SWITCH_TEXT_SECTIONS when the cold section starts
   with landing pad.
   With landing pad being at offset 0 from the start label of the section
   we would miss EH delivery because 0 is special and means no landing pad.  */
static bool maybe_add_nop_after_section_switch (MtcsExcept *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   if (!mtcsRtlData/*!crtl*/->uses_eh_lsda || !mtcsRtlData/*!crtl*/->eh.call_site_record_v[1])
         return false;
   int n = vec_safe_length (mtcsRtlData/*!crtl*/->eh.call_site_record_v[1]);
   hash_set<rtx_insn *> visited;

   for (int i = 0; i < n; ++i){
      struct call_site_record_d *cs = (*mtcsRtlData/*!crtl*/->eh.call_site_record_v[1])[i];
      if (cs->landing_pad){
         rtx_insn *insn = as_a <rtx_insn *> (cs->landing_pad);
         while (true){
            /* Landing pads have LABEL_PRESERVE_P flag set.  This check make
            sure that we do not walk past landing pad visited earlier
            which would result in possible quadratic behaviour.  */
            if (LABEL_P (insn) && LABEL_PRESERVE_P (insn) && visited.add (insn))
               break;

            /* Conservatively assume that ASM insn may be empty.  We have
            now way to tell what they contain.  */
            if (active_insn_p (insn)
            && GET_CODE (PATTERN (insn)) != ASM_INPUT
            && GET_CODE (PATTERN (insn)) != ASM_OPERANDS)
               break;

            /* If we reached the start of hot section, then NOP will be
            needed.  */
            if (GET_CODE (insn) == NOTE  && NOTE_KIND (insn) == NOTE_INSN_SWITCH_TEXT_SECTIONS){
               mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,gen_nop (), insn);
               break;
            }

            /* We visit only labels from cold section.  We should never hit
            begining of the insn stream here.  */
            insn = PREV_INSN (insn);
         }
      }
   }
   return false;
}


/* Turn REG_EH_REGION notes back into NOTE_INSN_EH_REGION notes.
   The new note numbers will not refer to region numbers, but
   instead to call site entries.  */
static unsigned int convert_to_eh_region_ranges (MtcsExcept *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   rtx insn;
   rtx_insn *iter;
   rtx_note *note;
   action_hash_type ar_hash (31);
   int last_action = -3;
   rtx_insn *last_action_insn = NULL;
   rtx last_landing_pad = NULL_RTX;
   rtx_insn *first_no_action_insn = NULL;
   int call_site = 0;
   int cur_sec = 0;
   rtx_insn *section_switch_note = NULL;
   rtx_insn *first_no_action_insn_before_switch = NULL;
   rtx_insn *last_no_action_insn_before_switch = NULL;
   int saved_call_site_base = self->call_site_base;

   vec_alloc (mtcsRtlData/*!crtl*/->eh.action_record_data, 64);

   for (iter = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); iter ; iter = NEXT_INSN (iter))
      if (INSN_P (iter)){
         eh_landing_pad lp;
         eh_region region;
         bool nothrow;
         int this_action;
         rtx_code_label *this_landing_pad;

         insn = iter;
         if (NONJUMP_INSN_P (insn)  && GET_CODE (PATTERN (insn)) == SEQUENCE)
            insn = XVECEXP (PATTERN (insn), 0, 0);

         nothrow = get_eh_region_and_lp_from_rtx(self,insn, &region, &lp);
         if (nothrow)
            continue;
         if (region)
            this_action = collect_one_action_chain(self,&ar_hash, region);
         else
            this_action = -1;

         /* Existence of catch handlers, or must-not-throw regions
         implies that an lsda is needed (even if empty).  */
         if (this_action != -1)
            mtcsRtlData/*!crtl*/->uses_eh_lsda = 1;
         /* Delay creation of region notes for no-action regions
         until we're sure that an lsda will be required.  */
         else if (last_action == -3){
            first_no_action_insn = iter;
            last_action = -1;
         }

         if (this_action >= 0)
            this_landing_pad = lp->landing_pad;
         else
            this_landing_pad = NULL;

         /* Differing actions or landing pads implies a change in call-site
         info, which implies some EH_REGION note should be emitted.  */
         if (last_action != this_action || last_landing_pad != this_landing_pad){
            /* If there is a queued no-action region in the other section
            with hot/cold partitioning, emit it now.  */
            if (first_no_action_insn_before_switch){
               gcc_assert (this_action != -1 && last_action == (first_no_action_insn ? -1 : -3));
               call_site = add_call_site(self,NULL_RTX, 0, 0);
               note = mtcs_emit_emit_note_before/*!emit_note_before*/(mtcsEmit,NOTE_INSN_EH_REGION_BEG,first_no_action_insn_before_switch);
               NOTE_EH_HANDLER (note) = call_site;
               note = emit_note_eh_region_end(self,last_no_action_insn_before_switch);
               NOTE_EH_HANDLER (note) = call_site;
               gcc_assert (last_action != -3  || (last_action_insn == last_no_action_insn_before_switch));
               first_no_action_insn_before_switch = NULL;
               last_no_action_insn_before_switch = NULL;
               self->call_site_base++;
            }
            /* If we'd not seen a previous action (-3) or the previous
            action was must-not-throw (-2), then we do not need an
            end note.  */
            if (last_action >= -1){
               /* If we delayed the creation of the begin, do it now.  */
               if (first_no_action_insn){
                  call_site = add_call_site(self,NULL_RTX, 0, cur_sec);
                  note = mtcs_emit_emit_note_before/*!emit_note_before*/(mtcsEmit,NOTE_INSN_EH_REGION_BEG,first_no_action_insn);
                  NOTE_EH_HANDLER (note) = call_site;
                  first_no_action_insn = NULL;
               }

               note = emit_note_eh_region_end(self,last_action_insn);
               NOTE_EH_HANDLER (note) = call_site;
            }

            /* If the new action is must-not-throw, then no region notes
            are created.  */
            if (this_action >= -1){
               call_site = add_call_site(self,this_landing_pad,this_action < 0 ? 0 : this_action,cur_sec);
               note = mtcs_emit_emit_note_before/*!emit_note_before*/(mtcsEmit,NOTE_INSN_EH_REGION_BEG, iter);
               NOTE_EH_HANDLER (note) = call_site;
            }

            last_action = this_action;
            last_landing_pad = this_landing_pad;
         }
         last_action_insn = iter;
      }else if (NOTE_P (iter) && NOTE_KIND (iter) == NOTE_INSN_SWITCH_TEXT_SECTIONS){
         gcc_assert (section_switch_note == NULL_RTX);
         gcc_assert (flag_reorder_blocks_and_partition);
         section_switch_note = iter;
         if (first_no_action_insn){
            first_no_action_insn_before_switch = first_no_action_insn;
            last_no_action_insn_before_switch = last_action_insn;
            first_no_action_insn = NULL;
            gcc_assert (last_action == -1);
            last_action = -3;
         }
         /* Force closing of current EH region before section switch and
         opening a new one afterwards.  */
         else if (last_action != -3)
            last_landing_pad = pc_rtx;
         if (mtcsRtlData/*!crtl*/->eh.call_site_record_v[cur_sec])
            self->call_site_base += mtcsRtlData/*!crtl*/->eh.call_site_record_v[cur_sec]->length ();
         cur_sec++;
         gcc_assert (mtcsRtlData/*!crtl*/->eh.call_site_record_v[cur_sec] == NULL);
         vec_alloc (mtcsRtlData/*!crtl*/->eh.call_site_record_v[cur_sec], 10);
      }

   if (last_action >= -1 && ! first_no_action_insn){
      note = emit_note_eh_region_end(self,last_action_insn);
      NOTE_EH_HANDLER (note) = call_site;
   }

   self->call_site_base = saved_call_site_base;

   return 0;
}

/* Set TREE_NOTHROW and crtl->all_throwers_are_sibcalls.  */
static unsigned int set_nothrow_function_flags (MtcsExcept *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   rtx_insn *insn;
   mtcsRtlData/*!crtl*/->nothrow = 1;
   /* Assume crtl->all_throwers_are_sibcalls until we encounter
   something that can throw an exception.  We specifically exempt
   CALL_INSNs that are SIBLING_CALL_P, as these are really jumps,
   and can't throw.  Most CALL_INSNs are not SIBLING_CALL_P, so this
   is optimistic.  */
   mtcsRtlData/*!crtl*/->all_throwers_are_sibcalls = 1;

   /* If we don't know that this implementation of the function will
   actually be used, then we must not set TREE_NOTHROW, since
   callers must not assume that this function does not throw.  */
   if (TREE_NOTHROW (current_function_decl))
      return 0;

   if (! mtcsOptionsItem->x_flag_exceptions)
      return 0;

   for (insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = NEXT_INSN (insn))
      if (can_throw_external (insn)){
         mtcsRtlData/*!crtl*/->nothrow = 0;

         if (!CALL_P (insn) || !SIBLING_CALL_P (insn)){
            mtcsRtlData/*!crtl*/->all_throwers_are_sibcalls = 0;
            return 0;
         }
      }

   if (mtcsRtlData/*!crtl*/->nothrow
   && (cgraph_node::get (current_function_decl)->get_availability () >= AVAIL_AVAILABLE)){
      struct cgraph_node *node = cgraph_node::get (current_function_decl);
      struct cgraph_edge *e;
      for (e = node->callers; e; e = e->next_caller)
         e->can_throw_external = false;
      node->set_nothrow_flag (true);

      if (dump_file)
         fprintf (dump_file, "Marking function nothrow: %s\n\n", current_function_name ());
   }
   return 0;
}

MtcsExcept *mtcs_except_new(MtcsMode *mtcsMode)
{
     MtcsExcept *self = n_slice_alloc0 (sizeof(MtcsExcept));
     mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
     mtcsExceptInit(self);
     return self;
}

//原型 NEXT_PASS (pass_convert_to_eh_region_ranges, 1);  RTL_PASS  except.cc  eh_ranges   y 有条件执行 cfun->eh->region_tree == NUL..
static nuint convert_to_eh_region_ranges_execute_cb(MtcsPass *mtcsPass,function *func)
{
   MtcsPassCleanupBarriers *self=(MtcsPassCleanupBarriers *)mtcsPass;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExcept *mtcsExcept=mtcs_target_get_except(mtcsTarget);
   int ret = convert_to_eh_region_ranges(mtcsExcept);
   maybe_add_nop_after_section_switch(mtcsExcept);
   return ret;
}

static nboolean convert_to_eh_region_ranges_gate_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   /* Nothing to do for SJLJ exceptions or if no regions created.  */
   if (cfun->eh->region_tree == NULL)
      return false;
   if (target_common_except_unwind_info/*!targetm_common.except_unwind_info*/(mtcsMachine->common,mtcsOptionsItem) == UI_SJLJ)
      return false;
   return true;

}

static void mtcsPassConvertToEhRegionRangesInit(MtcsPassConvertToEhRegionRanges *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =convert_to_eh_region_ranges_execute_cb;
    mtcsPass->gate =convert_to_eh_region_ranges_gate_cb;

    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassConvertToEhRegionRanges *mtcs_pass_convert_to_eh_region_ranges_new (MtcsMode *mtcsMode)
{
   MtcsPassConvertToEhRegionRanges *self = n_slice_alloc0 (sizeof(MtcsPassConvertToEhRegionRanges));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"eh_ranges");
   mtcsPassConvertToEhRegionRangesInit(self);
   return self;
}

//原型 NEXT_PASS (pass_set_nothrow_function_flags, 1);  RTL_PASS  except.cc  nothrow   y 无条件执行 set_nothrow_function_flags.
static nuint set_nothrow_function_flags_execute_cb(MtcsPass *mtcsPass,function *func)
{
   MtcsPassSetNothrowFuncitonFlags *self=(MtcsPassSetNothrowFuncitonFlags *)mtcsPass;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExcept *mtcsExcept=mtcs_target_get_except(mtcsTarget);
   return set_nothrow_function_flags(mtcsExcept);
}

static void mtcsPassSetNothrowFuncitonFlagsInit(MtcsPassSetNothrowFuncitonFlags *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =set_nothrow_function_flags_execute_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassSetNothrowFuncitonFlags *mtcs_pass_set_nothrow_function_flags_new (MtcsMode *mtcsMode)
{
   MtcsPassSetNothrowFuncitonFlags *self = n_slice_alloc0 (sizeof(MtcsPassSetNothrowFuncitonFlags));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"nothrow");
   mtcsPassSetNothrowFuncitonFlagsInit(self);
   return self;
}
