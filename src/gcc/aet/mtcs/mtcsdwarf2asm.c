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
 * base on dwarf2asm.cc
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
#include "dwarf2.h"
#include "dwarf2out.h"
#include "dwarf2asm.h"

#include "mtcsasm.h"
#include "mtcsdwarf2asm.h"
#include "mtcstarget.h"

#define MAX_ARTIFICIAL_LABEL_BYTES  40

#ifndef FUNC_SECOND_SECT_LABEL
#define FUNC_SECOND_SECT_LABEL  "LFSB"
#endif


#ifndef XCOFF_DEBUGGING_INFO
#define XCOFF_DEBUGGING_INFO 0
#endif

#if defined(HAVE_GAS_HIDDEN)
# define USE_LINKONCE_INDIRECT (SUPPORTS_ONE_ONLY && !XCOFF_DEBUGGING_INFO)
#else
# define USE_LINKONCE_INDIRECT 0
#endif

#ifndef FUNC_END_LABEL
#define FUNC_END_LABEL      "LFE"
#endif

#define DWARF_LINE_DEFAULT_IS_STMT_START 1


typedef unsigned int var_loc_view;


static char cold_end_label[MAX_ARTIFICIAL_LABEL_BYTES];



/* The entries in the line_info table more-or-less mirror the opcodes
   that are used in the real dwarf line table.  Arrays of these entries
   are collected per section when DWARF2_ASM_LINE_DEBUG_INFO is not
   supported.  */

enum dw_line_info_opcode {
  /* Emit DW_LNE_set_address; the operand is the label index.  */
  LI_set_address,

  /* Emit a row to the matrix with the given line.  This may be done
     via any combination of DW_LNS_copy, DW_LNS_advance_line, and
     special opcodes.  */
  LI_set_line,

  /* Emit a DW_LNS_set_file.  */
  LI_set_file,

  /* Emit a DW_LNS_set_column.  */
  LI_set_column,

  /* Emit a DW_LNS_negate_stmt; the operand is ignored.  */
  LI_negate_stmt,

  /* Emit a DW_LNS_set_prologue_end/epilogue_begin; the operand is ignored.  */
  LI_set_prologue_end,
  LI_set_epilogue_begin,

  /* Emit a DW_LNE_set_discriminator.  */
  LI_set_discriminator,

  /* Output a Fixed Advance PC; the target PC is the label index; the
     base PC is the previous LI_adv_address or LI_set_address entry.
     We only use this when emitting debug views without assembler
     support, at explicit user request.  Ideally, we should only use
     it when the offset might be zero but we can't tell: it's the only
     way to maybe change the PC without resetting the view number.  */
  LI_adv_address
};


typedef struct GTY(()) dw_line_info_struct {
  enum dw_line_info_opcode opcode;
  unsigned int val;
} dw_line_info_entry;

/* Node of the variable location list.  */
struct GTY ((chain_next ("%h.next"))) var_loc_node {
  /* Either NOTE_INSN_VAR_LOCATION, or, for SRA optimized variables,
     EXPR_LIST chain.  For small bitsizes, bitsize is encoded
     in mode of the EXPR_LIST node and first EXPR_LIST operand
     is either NOTE_INSN_VAR_LOCATION for a piece with a known
     location or NULL for padding.  For larger bitsizes,
     mode is 0 and first operand is a CONCAT with bitsize
     as first CONCAT operand and NOTE_INSN_VAR_LOCATION resp.
     NULL as second operand.  */
  rtx GTY (()) loc;
  const char * GTY (()) label;
  struct var_loc_node * GTY (()) next;
  var_loc_view view;
};


/* Variable location list.  */
struct GTY ((for_user)) var_loc_list_def {
  struct var_loc_node * GTY (()) first;

  /* Pointer to the last but one or last element of the
     chained list.  If the list is empty, both first and
     last are NULL, if the list contains just one node
     or the last node certainly is not redundant, it points
     to the last node, otherwise points to the last but one.
     Do not mark it for GC because it is marked through the chain.  */
  struct var_loc_node * GTY ((skip ("%h"))) last;

  /* Pointer to the last element before section switch,
     if NULL, either sections weren't switched or first
     is after section switch.  */
  struct var_loc_node * GTY ((skip ("%h"))) last_before_switch;

  /* DECL_UID of the variable decl.  */
  unsigned int decl_id;
};
typedef struct var_loc_list_def var_loc_list;


struct GTY(()) dw_line_info_table {
  /* The label that marks the end of this section.  */
  const char *end_label;

  /* The values for the last row of the matrix, as collected in the table.
     These are used to minimize the changes to the next row.  */
  unsigned int file_num;
  unsigned int line_num;
  unsigned int column_num;
  int discrim_num;
  bool is_stmt;
  bool in_use;

  /* This denotes the NEXT view number.

     If it is 0, it is known that the NEXT view will be the first view
     at the given PC.

     If it is -1, we're forcing the view number to be reset, e.g. at a
     function entry.

     The meaning of other nonzero values depends on whether we're
     computing views internally or leaving it for the assembler to do
     so.  If we're emitting them internally, view denotes the view
     number since the last known advance of PC.  If we're leaving it
     for the assembler, it denotes the LVU label number that we're
     going to ask the assembler to assign.  */
  var_loc_view view;

  /* This counts the number of symbolic views emitted in this table
     since the latest view reset.  Its max value, over all tables,
     sets symview_upper_bound.  */
  var_loc_view symviews_since_reset;

#define FORCE_RESET_NEXT_VIEW(x) ((x) = (var_loc_view)-1)
#define RESET_NEXT_VIEW(x) ((x) = (var_loc_view)0)
#define FORCE_RESETTING_VIEW_P(x) ((x) == (var_loc_view)-1)
#define RESETTING_VIEW_P(x) ((x) == (var_loc_view)0 || FORCE_RESETTING_VIEW_P (x))

  vec<dw_line_info_entry, va_gc> *entries;
};

struct decl_loc_hasher : ggc_ptr_hash<var_loc_list>
{
  typedef const_tree compare_type;

  static hashval_t hash (var_loc_list *);
  static bool equal (var_loc_list *, const_tree);
};

static GTY (()) hash_table<decl_loc_hasher> *decl_loc_table;

inline hashval_t decl_loc_hasher::hash (var_loc_list *x)
{
  return (hashval_t) x->decl_id;
}

/* Return true if decl_id of var_loc_list X is the same as
   UID of decl *Y.  */

inline bool decl_loc_hasher::equal (var_loc_list *x, const_tree y)
{
  return (x->decl_id == DECL_UID (y));
}

static void mtcsDwarf2AsmInit(MtcsDwarf2Asm *self)
{
   self->have_multiple_function_sections = false;
   self->in_text_section_p = false;
}

/* Output an unsigned LEB128 quantity, but only the byte values.  */
//原型 dw2_asm_output_data_uleb128_raw dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_data_uleb128_raw (MtcsDwarf2Asm *self,unsigned HOST_WIDE_INT value)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   while (1){
      int byte = (value & 0x7f);
      value >>= 7;
      if (value != 0)
         /* More bytes to follow.  */
         byte |= 0x80;
      fprintf (mtcsAsm->asmFile, "%#x", byte);
      if (value == 0)
         break;
      fputc (',', mtcsAsm->asmFile);
   }
}

//原型 dw2_asm_output_delta_uleb128 dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_delta_uleb128 (MtcsDwarf2Asm *self,const char *lab1 ATTRIBUTE_UNUSED,
                  const char *lab2 ATTRIBUTE_UNUSED, const char *comment, ...)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   va_list ap;
   va_start (ap, comment);
   gcc_assert (mtcs_config_get_value(mtcsConfig,MTCS_HAVE_AS_LEB128));
   fputs ("\t.uleb128 ", mtcsAsm->asmFile);
   mtcs_asm_assemble_name (mtcsAsm, lab1);
   putc ('-', mtcsAsm->asmFile);
   /* dwarf2out.cc might give us a label expression (e.g. .LVL548-1)
   as second argument.  If so, make it a subexpression, to make
   sure the substraction is done in the right order.  */
   if (strchr (lab2, '-') != NULL){
      putc ('(', asm_out_file);
      mtcs_asm_assemble_name (mtcsAsm, lab2);
      putc (')', mtcsAsm->asmFile);
   }else
      mtcs_asm_assemble_name (mtcsAsm, lab2);

   if (mtcsOptionsItem->x_flag_debug_asm && comment){
      fprintf (mtcsAsm->asmFile, "\t%s ", mtcs_asm_get_comment_start/*!ASM_COMMENT_START*/(mtcsAsm));
      vfprintf (mtcsAsm->asmFile, comment, ap);
   }
   fputc ('\n', mtcsAsm->asmFile);
   va_end (ap);
}

//原型 dw2_asm_output_data dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_data (MtcsDwarf2Asm *self,int size, unsigned HOST_WIDE_INT value, const char *comment, ...)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   va_list ap;
   const char *op = mtcs_output_integer_asm_op/*!integer_asm_op*/(mtcsOutput,size, false);

   va_start (ap, comment);

   if (size * 8 < HOST_BITS_PER_WIDE_INT)
      value &= ~(HOST_WIDE_INT_M1U << (size * 8));

   if (op){
      fputs (op, mtcsAsm->asmFile);
      fprint_whex (mtcsAsm->asmFile, value);
   }else
      mtcs_asm_assemble_integer/*!assemble_integer*/(mtcsAsm,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,value), size, BITS_PER_UNIT, 1);

   if (mtcsOptionsItem->x_flag_debug_asm && comment){
      //fputs ("\t" ASM_COMMENT_START " ", mtcsAsm->asmFile);
      fprintf (mtcsAsm->asmFile, "\t%s ", mtcs_asm_get_comment_start/*!ASM_COMMENT_START*/(mtcsAsm));
      vfprintf (mtcsAsm->asmFile, comment, ap);
   }
   putc ('\n', mtcsAsm->asmFile);

   va_end (ap);
}

/* Mark the ranges of non-debug subsections in the std text sections.  */
void mtcs_dwarf2_asm_mark_ignored_debug_section (MtcsDwarf2Asm *self,dw_fde_ref fde, bool second)
{
  bool std_section;
  const char *begin_label, *end_label;
  const char **last_end_label;
  vec<const char *, va_gc> **switch_ranges;

  if (second){
      std_section = fde->second_in_std_section;
      begin_label = fde->dw_fde_second_begin;
      end_label   = fde->dw_fde_second_end;
  }else{
      std_section = fde->in_std_section;
      begin_label = fde->dw_fde_begin;
      end_label   = fde->dw_fde_end;
  }

  if (!std_section)
    return;

  if (self->in_text_section_p){
      last_end_label = &self->last_text_label;
      switch_ranges  = &self->switch_text_ranges;
  }else{
      last_end_label = &self->last_cold_label;
      switch_ranges  = &self->switch_cold_ranges;
  }

  if (fde->ignored_debug){
      if (*switch_ranges && !(vec_safe_length (*switch_ranges) & 1))
          vec_safe_push (*switch_ranges, *last_end_label);
  }else{
      *last_end_label = end_label;
      if (!*switch_ranges)
          vec_alloc (*switch_ranges, 16);
      else if (vec_safe_length (*switch_ranges) & 1)
          vec_safe_push (*switch_ranges, begin_label);
  }
}


/* Put X, a SYMBOL_REF, in memory.  Return a SYMBOL_REF to the allocated
   memory.  Differs from force_const_mem in that a single pool is used for
   the entire unit of translation, and the memory is not guaranteed to be
   "near" the function in any interesting sense.  IS_PUBLIC controls whether
   the symbol can be shared across the entire application (or DSO).  */

rtx mtcs_dwarf2_asm_force_const_mem (MtcsDwarf2Asm *self,rtx x, bool is_public)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

  const char *key;
  tree decl_id;
  if (! self->indirect_pool)
    self->indirect_pool = hash_map<const char *, tree>::create_ggc (64);

  gcc_assert (GET_CODE (x) == SYMBOL_REF);
  key = XSTR (x, 0);
  tree *slot =self->indirect_pool->get (key);
  if (slot)
    decl_id = *slot;
  else {
      tree id;
      const char *str =mtcsTarget->strip_name_encoding/*!targetm.strip_name_encoding*/(mtcsTarget,key);
      if (is_public && USE_LINKONCE_INDIRECT){
          char *ref_name = XALLOCAVEC (char, strlen (str) + sizeof "DW.ref.");
          sprintf (ref_name, "DW.ref.%s", str);
          gcc_assert (!maybe_get_identifier (ref_name));
          decl_id = get_identifier (ref_name);
          TREE_PUBLIC (decl_id) = 1;
      }else{
          char label[32];
          mtcs_asm_generate_internal_label(mtcsAsm,label, "LDFCM", self->dw2_const_labelno);
          ++self->dw2_const_labelno;
          gcc_assert (!maybe_get_identifier (label));
          decl_id = get_identifier (label);
      }

      id = maybe_get_identifier (str);
      if (id)
          TREE_SYMBOL_REFERENCED (id) = 1;

      self->indirect_pool->put (key, decl_id);
  }
  return gen_rtx_SYMBOL_REF (Pmode, IDENTIFIER_POINTER (decl_id));
}


/* The current table to which we should emit line number information
   for the current function.  This will be set up at the beginning of
   assembly for the function.  */
static GTY(()) dw_line_info_table *cur_line_info_table;
static GTY(()) dw_line_info_table *text_section_line_info;
static GTY(()) dw_line_info_table *cold_text_section_line_info;
/* The set of all non-default tables of line number info.  */
static GTY(()) vec<dw_line_info_table *, va_gc> *separate_line_info;

/* Put X, a SYMBOL_REF, in memory.  Return a SYMBOL_REF to the allocated
   memory.  Differs from force_const_mem in that a single pool is used for
   the entire unit of translation, and the memory is not guaranteed to be
   "near" the function in any interesting sense.  IS_PUBLIC controls whether
   the symbol can be shared across the entire application (or DSO).  */

//rtx mtcs_dwarf2_asm_force_const_mem (MtcsDwarf2Asm *self,rtx x, bool is_public)
//{
//MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
//MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
//  const char *key;
//  tree decl_id;
//
//  if (! self->indirect_pool)
//      self->indirect_pool = hash_map<const char *, tree>::create_ggc (64);
//
//  gcc_assert (GET_CODE (x) == SYMBOL_REF);
//
//  key = XSTR (x, 0);
//  tree *slot = self->indirect_pool->get (key);
//  if (slot)
//    decl_id = *slot;
//  else{
//      tree id;
//      const char *str = targetm.strip_name_encoding (key);
//
//      if (is_public && USE_LINKONCE_INDIRECT){
//          char *ref_name = XALLOCAVEC (char, strlen (str) + sizeof "DW.ref.");
//
//          sprintf (ref_name, "DW.ref.%s", str);
//          gcc_assert (!maybe_get_identifier (ref_name));
//          decl_id = get_identifier (ref_name);
//          TREE_PUBLIC (decl_id) = 1;
//      }else{
//          char label[32];
//
//          mtcsTarget->generate_internal_label(mtcsTarget,label, "LDFCM", self->dw2_const_labelno);
//          ++self->dw2_const_labelno;
//          gcc_assert (!maybe_get_identifier (label));
//          decl_id = get_identifier (label);
//      }
//
//      id = maybe_get_identifier (str);
//      if (id)
//          TREE_SYMBOL_REFERENCED (id) = 1;
//      self->indirect_pool->put (key, decl_id);
//    }
//
//  return gen_rtx_SYMBOL_REF (Pmode, IDENTIFIER_POINTER (decl_id));
//}

/* Output an unsigned LEB128 quantity.  */
//原型 dw2_asm_output_data_uleb128 dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_data_uleb128 (MtcsDwarf2Asm *self,unsigned HOST_WIDE_INT value,const char *comment, ...)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   va_list ap;

   va_start (ap, comment);

   if (mtcs_config_get_value(mtcsConfig,MTCS_HAVE_AS_LEB128)){
      fputs ("\t.uleb128 ", mtcsAsm->asmFile);
      fprint_whex (mtcsAsm->asmFile, value);
      if (mtcsOptionsItem->x_flag_debug_asm && comment){
         fprintf (mtcsAsm->asmFile, "\t%s ", mtcs_asm_get_comment_start/*!ASM_COMMENT_START*/(mtcsAsm));
         vfprintf (mtcsAsm->asmFile, comment, ap);
      }
   }else{
      unsigned HOST_WIDE_INT work = value;
      const char *byte_op = mtcsMachine->asmOut->byte_op/*!targetm.asm_out.byte_op*/;
      if (byte_op)
         fputs (byte_op, mtcsAsm->asmFile);
      do{
         int byte = (work & 0x7f);
         work >>= 7;
         if (work != 0)
            /* More bytes to follow.  */
            byte |= 0x80;

         if (byte_op){
            fprintf (mtcsAsm->asmFile, "%#x", byte);
            if (work != 0)
               fputc (',', mtcsAsm->asmFile);
         }else
            mtcs_asm_assemble_integer (mtcsAsm,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,byte), 1, BITS_PER_UNIT, 1);
      }while (work != 0);

      if (mtcsOptionsItem->x_flag_debug_asm){
         fprintf (mtcsAsm->asmFile, "\t%s uleb128 " HOST_WIDE_INT_PRINT_HEX,
               mtcs_asm_get_comment_start/*!ASM_COMMENT_START*/(mtcsAsm), value);
         if (comment){
            fputs ("; ", mtcsAsm->asmFile);
            vfprintf (mtcsAsm->asmFile, comment, ap);
         }
      }
   }
   putc ('\n', mtcsAsm->asmFile);
   va_end (ap);
}


/* Output a value of a given size in target byte order.  */
//原型 dw2_asm_output_data_raw dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_data_raw (MtcsDwarf2Asm *self,int size, unsigned HOST_WIDE_INT value)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

   unsigned char bytes[8];
   int i;

   for (i = 0; i < 8; ++i){
      bytes[i] = value & 0xff;
      value >>= 8;
   }

   if (BYTES_BIG_ENDIAN){
      for (i = size - 1; i > 0; --i)
         fprintf (mtcsAsm->asmFile, "%#x,", bytes[i]);
      fprintf (mtcsAsm->asmFile, "%#x", bytes[0]);
   }else{
      for (i = 0; i < size - 1; ++i)
         fprintf (mtcsAsm->asmFile, "%#x,", bytes[i]);
      fprintf (mtcsAsm->asmFile, "%#x", bytes[i]);
   }
}

//原型 dw2_asm_output_data_sleb128_raw dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_data_sleb128_raw (MtcsDwarf2Asm *self,HOST_WIDE_INT value)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

   int byte, more;
   while (1){
      byte = (value & 0x7f);
      value >>= 7;
      more = !((value == 0 && (byte & 0x40) == 0) || (value == -1 && (byte & 0x40) != 0));
      if (more)
         byte |= 0x80;

      fprintf (mtcsAsm->asmFile, "%#x", byte);
      if (!more)
         break;
      fputc (',', mtcsAsm->asmFile);
   }
}

/* Like dw2_asm_output_addr_rtx, but encode the pointer as directed.
   If PUBLIC is set and the encoding is DW_EH_PE_indirect, the indirect
   reference is shared across the entire application (or DSO).  */
//原型 dw2_asm_output_encoded_addr_rtx dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_encoded_addr_rtx (MtcsDwarf2Asm *self,int encoding, rtx addr, bool is_public, const char *comment, ...)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  int size;
  va_list ap;
  va_start (ap, comment);
  size = size_of_encoded_value (encoding);

  if (encoding == DW_EH_PE_aligned){
      mtcs_asm_assemble_align (mtcsAsm,POINTER_SIZE);
      mtcs_asm_assemble_integer (mtcsAsm,addr, size, POINTER_SIZE, 1);
      va_end (ap);
      return;
  }

  /* NULL is _always_ represented as a plain zero, as is 1 for Ada's
     "all others".  */
  if (addr == const0_rtx || addr == const1_rtx)
      mtcs_asm_assemble_integer (mtcsAsm,addr, size, BITS_PER_UNIT, 1);
  else{
    restart:
      /* Allow the target first crack at emitting this.  Some of the
     special relocations require special directives instead of
     just ".4byte" or whatever.  */
#ifdef ASM_MAYBE_OUTPUT_ENCODED_ADDR_RTX
      ASM_MAYBE_OUTPUT_ENCODED_ADDR_RTX (asm_out_file, encoding, size,addr, done);
#endif

      /* Indirection is used to get dynamic relocations out of a
     read-only section.  */
      if (encoding & DW_EH_PE_indirect){
          /* It is very tempting to use force_const_mem so that we share data
             with the normal constant pool.  However, we've already emitted
             the constant pool for this function.  Moreover, we'd like to
             share these constants across the entire unit of translation and
             even, if possible, across the entire application (or DSO).  */
          addr = mtcs_dwarf2_asm_force_const_mem (self,addr, is_public);
          encoding &= ~DW_EH_PE_indirect;
          goto restart;
      }

      switch (encoding & 0xF0){
    case DW_EH_PE_absptr:
        mtcs_dwarf2_asm_assemble_integer (self,size, addr);
      break;

#ifdef ASM_OUTPUT_DWARF_DATAREL
    case DW_EH_PE_datarel:
      gcc_assert (GET_CODE (addr) == SYMBOL_REF);
      ASM_OUTPUT_DWARF_DATAREL (asm_out_file, size, XSTR (addr, 0));
      break;
#endif

    case DW_EH_PE_pcrel:
      gcc_assert (GET_CODE (addr) == SYMBOL_REF);
#ifdef ASM_OUTPUT_DWARF_PCREL
      ASM_OUTPUT_DWARF_PCREL (asm_out_file, size, XSTR (addr, 0));
#else
      mtcs_dwarf2_asm_assemble_integer (self,size, gen_rtx_MINUS (Pmode, addr, pc_rtx));
#endif
      break;

    default:
      /* Other encodings should have been handled by
         ASM_MAYBE_OUTPUT_ENCODED_ADDR_RTX.  */
      gcc_unreachable ();
    }

#ifdef ASM_MAYBE_OUTPUT_ENCODED_ADDR_RTX
    done:;
#endif
    }

  if (flag_debug_asm && comment)
    {
      fprintf (mtcsAsm->asmFile, "\t%s ", mtcs_asm_get_comment_start/*!ASM_COMMENT_START*/(mtcsAsm));
      vfprintf (mtcsAsm->asmFile, comment, ap);
    }
  fputc ('\n', mtcsAsm->asmFile);

  va_end (ap);
}

/* Output an unaligned integer with the given value and size.  Prefer not
   to print a newline, since the caller may want to add a comment.  */

void mtcs_dwarf2_asm_assemble_integer (MtcsDwarf2Asm *self,int size, rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);

   if (size == 2 * (int) DWARF2_ADDR_SIZE && !CONST_SCALAR_INT_P (x)){
      /* On 32-bit targets with -gdwarf64, DImode values with
      relocations usually result in assembler errors.  Assume
      all such values are positive and emit the relocation only
      in the least significant half.  */
      const char *op = mtcs_output_integer_asm_op/*!integer_asm_op*/(mtcsOutput,DWARF2_ADDR_SIZE, false);
      if (BYTES_BIG_ENDIAN){
         if (op){
            fputs (op, mtcsAsm->asmFile);
            fprint_whex (mtcsAsm->asmFile, 0);
            fputs (", ", mtcsAsm->asmFile);
            mtcs_output_addr_const (mtcsOutput, x);
         }else{
            mtcs_asm_assemble_integer (mtcsAsm,const0_rtx, DWARF2_ADDR_SIZE,BITS_PER_UNIT, 1);
            putc ('\n', mtcsAsm->asmFile);
            mtcs_asm_assemble_integer (mtcsAsm,x, DWARF2_ADDR_SIZE,BITS_PER_UNIT, 1);
         }
      }else{
         if (op){
            fputs (op, mtcsAsm->asmFile);
            mtcs_output_addr_const (mtcsOutput, x);
            fputs (", ", mtcsAsm->asmFile);
            fprint_whex (mtcsAsm->asmFile, 0);
         }else{
            mtcs_asm_assemble_integer (mtcsAsm,x, DWARF2_ADDR_SIZE,BITS_PER_UNIT, 1);
            putc ('\n', mtcsAsm->asmFile);
            mtcs_asm_assemble_integer (mtcsAsm,const0_rtx, DWARF2_ADDR_SIZE,BITS_PER_UNIT, 1);
         }
      }
      return;
   }

   const char *op = mtcs_output_integer_asm_op/*!integer_asm_op*/(mtcsOutput,size, false);

   if (op){
      fputs (op, mtcsAsm->asmFile);
      if (CONST_INT_P (x))
         fprint_whex (mtcsAsm->asmFile, (unsigned HOST_WIDE_INT) INTVAL (x));
      else
         mtcs_output_addr_const (mtcsOutput, x);
   }else
      mtcs_asm_assemble_integer (mtcsAsm,x, size, BITS_PER_UNIT, 1);
}

/* Output the difference between two symbols in a given size.  */
/* ??? There appear to be assemblers that do not like such
   subtraction, but do support ASM_SET_OP.  It's unfortunately
   impossible to do here, since the ASM_SET_OP for the difference
   symbol must appear after both symbols are defined.  */
//原型 dw2_asm_output_delta dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_delta (MtcsDwarf2Asm *self,int size, const char *lab1, const char *lab2,
            const char *comment, ...)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   va_list ap;

   va_start (ap, comment);
   if(mtcsAsm->asm_output_dwarf_delta)
//#ifdef ASM_OUTPUT_DWARF_DELTA
      mtcs_asm_output_dwarf_delta/*!ASM_OUTPUT_DWARF_DELTA (asm_out_file, */(mtcsAsm,size, lab1, lab2);
//#else
   else
      mtcs_dwarf2_asm_assemble_integer/*!dw2_assemble_integer*/(self,size,
            gen_rtx_MINUS (mtcs_mode_get_Pmode(mtcsMode),
            gen_rtx_SYMBOL_REF (mtcs_mode_get_Pmode(mtcsMode), lab1),
            gen_rtx_SYMBOL_REF (mtcs_mode_get_Pmode(mtcsMode), lab2)));
   //#endif
   if (mtcsOptionsItem->x_flag_debug_asm && comment){
      fprintf (mtcsAsm->asmFile, "\t%s ", ASM_COMMENT_START);
      vfprintf (mtcsAsm->asmFile, comment, ap);
   }
   fputc ('\n', mtcsAsm->asmFile);
   va_end (ap);
}

/* Output a section-relative reference to a LABEL, which was placed in
   BASE.  In general this can only be done for debugging symbols.
   E.g. on most targets with the GNU linker, this is accomplished with
   a direct reference and the knowledge that the debugging section
   will be placed at VMA 0.  Some targets have special relocations for
   this that we must use.  */
//原型 dw2_asm_output_offset dwarf2asm.h dwarf2asm.cc
void  mtcs_dwarf2_asm_output_offset (MtcsDwarf2Asm *self,int size, const char *label,
             section *base ATTRIBUTE_UNUSED, const char *comment, ...)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   va_list ap;

   va_start (ap, comment);
   if(mtcsAsm->asm_output_dwarf_offset)
//#ifdef ASM_OUTPUT_DWARF_OFFSET
      mtcs_asm_output_dwarf_offset/*ASM_OUTPUT_DWARF_OFFSET (asm_out_file,*/(mtcsAsm,size, label, 0, base);
   else
//#else
      mtcs_dwarf2_asm_assemble_integer/*!dw2_assemble_integer*/(self,size, gen_rtx_SYMBOL_REF (mtcs_mode_get_Pmode(mtcsMode), label));
//#endif

   if (mtcsOptionsItem->x_flag_debug_asm && comment){
      fprintf (mtcsAsm->asmFile, "\t%s ", ASM_COMMENT_START);
      vfprintf (mtcsAsm->asmFile, comment, ap);
   }
   fputc ('\n', mtcsAsm->asmFile);
   va_end (ap);
}

void mtcs_dwarf2_asm_output_offset (MtcsDwarf2Asm *self,int size, const char *label, HOST_WIDE_INT offset,
             section *base ATTRIBUTE_UNUSED,
             const char *comment, ...)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   va_list ap;

   va_start (ap, comment);
   if(mtcsAsm->asm_output_dwarf_offset)
// #ifdef ASM_OUTPUT_DWARF_OFFSET
      mtcs_asm_output_dwarf_offset/*ASM_OUTPUT_DWARF_OFFSET (asm_out_file,*/(mtcsAsm,size, label, offset, base);
//#else
   else
      mtcs_dwarf2_asm_assemble_integer/*!dw2_assemble_integer*/(self,size, gen_rtx_PLUS (mtcs_mode_get_Pmode(mtcsMode),
      gen_rtx_SYMBOL_REF (mtcs_mode_get_Pmode(mtcsMode), label),
      mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,offset, mtcs_mode_get_Pmode(mtcsMode))));
// #endif

   if (mtcsOptionsItem->x_flag_debug_asm && comment){
      fprintf (mtcsAsm->asmFile, "\t%s ", ASM_COMMENT_START);
      vfprintf (mtcsAsm->asmFile, comment, ap);
   }
   fputc ('\n', mtcsAsm->asmFile);
   va_end (ap);
}

/* Output an absolute reference to a label.  */
//原型 dw2_asm_output_addr dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_addr (MtcsDwarf2Asm *self,int size, const char *label, const char *comment, ...)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   va_list ap;
   va_start (ap, comment);
   mtcs_dwarf2_asm_assemble_integer/*!dw2_assemble_integer*/(self,size, gen_rtx_SYMBOL_REF (mtcs_mode_get_Pmode(mtcsMode), label));
   if (mtcsOptionsItem->x_flag_debug_asm && comment){
      fprintf (mtcsAsm->asmFile, "\t%s ", ASM_COMMENT_START);
      vfprintf (mtcsAsm->asmFile, comment, ap);
   }
   fputc ('\n', mtcsAsm->asmFile);
   va_end (ap);
}

/* Output a signed LEB128 quantity.  */
//原型 dw2_asm_output_data_sleb128 dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_data_sleb128 (MtcsDwarf2Asm *self,HOST_WIDE_INT value, const char *comment, ...)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   va_list ap;

   va_start (ap, comment);
   if (mtcs_config_get_value(mtcsConfig,MTCS_HAVE_AS_LEB128)){
      fprintf (mtcsAsm->asmFile, "\t.sleb128 " HOST_WIDE_INT_PRINT_DEC, value);

      if (mtcsOptionsItem->x_flag_debug_asm && comment){
         fprintf (mtcsAsm->asmFile, "\t%s ", ASM_COMMENT_START);
         vfprintf (mtcsAsm->asmFile, comment, ap);
      }
   }else{
      HOST_WIDE_INT work = value;
      int more, byte;
      const char *byte_op = mtcsMachine->asmOut->byte_op/*!targetm.asm_out.byte_op*/;

      if (byte_op)
         fputs (byte_op, mtcsAsm->asmFile);
      do{
         byte = (work & 0x7f);
         /* arithmetic shift */
         work >>= 7;
         more = !((work == 0 && (byte & 0x40) == 0) || (work == -1 && (byte & 0x40) != 0));
         if (more)
            byte |= 0x80;

         if (byte_op){
            fprintf (mtcsAsm->asmFile, "%#x", byte);
            if (more)
               fputc (',', mtcsAsm->asmFile);
         }else
            mtcs_asm_assemble_integer/*!assemble_integer*/(mtcsAsm,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,byte), 1, BITS_PER_UNIT, 1);
      }while (more);

      if (mtcsOptionsItem->x_flag_debug_asm){
         fprintf (mtcsAsm->asmFile, "\t%s sleb128 " HOST_WIDE_INT_PRINT_DEC,ASM_COMMENT_START, value);
         if (comment){
            fputs ("; ", mtcsAsm->asmFile);
            vfprintf (mtcsAsm->asmFile, comment, ap);
         }
      }
   }
   fputc ('\n', mtcsAsm->asmFile);
   va_end (ap);
}

/* Similar, but use an RTX expression instead of a text label.  */
//原型 dw2_asm_output_addr_rtx dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_addr_rtx (MtcsDwarf2Asm *self,int size, rtx addr, const char *comment, ...)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   va_list ap;
   va_start (ap, comment);
   mtcs_dwarf2_asm_assemble_integer/*!dw2_assemble_integer*/(self,size, addr);
   if (mtcsOptionsItem->x_flag_debug_asm && comment){
      fprintf (mtcsAsm->asmFile, "\t%s ", ASM_COMMENT_START);
      vfprintf (mtcsAsm->asmFile, comment, ap);
   }
   fputc ('\n', mtcsAsm->asmFile);
   va_end (ap);
}

#ifdef ASM_OUTPUT_DWARF_VMS_DELTA
/* Output the difference between two symbols in instruction units
   in a given size.  */
//原型 dw2_asm_output_vms_delta dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_vms_delta (MtcsDwarf2Asm *self,int size ATTRIBUTE_UNUSED,
           const char *lab1, const char *lab2, const char *comment, ...)
{
   va_list ap;

   va_start (ap, comment);

   //不未实现 ASM_OUTPUT_DWARF_VMS_DELTA (asm_out_file, size, lab1, lab2);
   if (flag_debug_asm && comment){
      fprintf (asm_out_file, "\t%s ", ASM_COMMENT_START);
      vfprintf (asm_out_file, comment, ap);
   }
   fputc ('\n', asm_out_file);

   va_end (ap);
}
#endif

/* Output symbol LAB1 as an unsigned LEB128 quantity.  LAB1 should be
   an assembler-computed constant, e.g. a view number, because we
   can't have relocations in LEB128 quantities.  */
//原型 dw2_asm_output_symname_uleb128 dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_symname_uleb128 (MtcsDwarf2Asm *self,const char *lab1 ATTRIBUTE_UNUSED, const char *comment, ...)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   va_list ap;
   va_start (ap, comment);
   if (mtcs_config_get_value/*!HAVE_AS_LEB128*/(mtcsConfig,MTCS_HAVE_AS_LEB128)){

//#ifdef HAVE_AS_LEB128
      fputs ("\t.uleb128 ", mtcsAsm->asmFile);
      mtcs_asm_assemble_name/*!assemble_name (asm_out_file,*/(mtcsAsm,lab1);
//#else
   }else
      gcc_unreachable ();
//#endif

   if (mtcsOptionsItem->x_flag_debug_asm && comment){
      fprintf (mtcsAsm->asmFile, "\t%s ", ASM_COMMENT_START);
      vfprintf (mtcsAsm->asmFile, comment, ap);
   }
   fputc ('\n', mtcsAsm->asmFile);
   va_end (ap);
}

/* Output the first ORIG_LEN characters of STR as a string.
   If ORIG_LEN is equal to -1, ignore this parameter and output
   the entire STR instead.
   If COMMENT is not NULL and comments in the debug information
   have been requested by the user, append the given COMMENT
   to the generated output.  */
//原型 dw2_asm_output_nstring dwarf2asm.h dwarf2asm.cc
void mtcs_dwarf2_asm_output_nstring (MtcsDwarf2Asm *self,const char *str, size_t orig_len,const char *comment, ...)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   size_t i, len;
   va_list ap;
   va_start (ap, comment);
   len = orig_len;
   if (len == (size_t) -1)
      len = strlen (str);

   if (flag_debug_asm && comment){
      if (XCOFF_DEBUGGING_INFO)
         fputs ("\t.byte \"", asm_out_file);
      else
         fputs ("\t.ascii \"", asm_out_file);

      for (i = 0; i < len; i++){
         int c = str[i];
         if (c == '\"')
            fputc (XCOFF_DEBUGGING_INFO ? '\"' : '\\', asm_out_file);
         else if (c == '\\')
            fputc ('\\', asm_out_file);
         if (ISPRINT (c))
            fputc (c, asm_out_file);
         else
            fprintf (asm_out_file, "\\%o", c);
      }
      fprintf (asm_out_file, "\\0\"\t%s ", ASM_COMMENT_START);
      vfprintf (asm_out_file, comment, ap);
      fputc ('\n', asm_out_file);
   }else{
      /* If an explicit length was given, we can't assume there
      is a null termination in the string buffer.  */
      if (orig_len == (size_t) -1)
         len += 1;
      target_asm_out_output_ascii/*!ASM_OUTPUT_ASCII*/(mtcsMachine->asmOut,str, len);
      if (orig_len != (size_t) -1)
         mtcs_asm_assemble_integer/*!assemble_integer*/(mtcsAsm,const0_rtx, 1, BITS_PER_UNIT, 1);
   }
   va_end (ap);
}

MtcsDwarf2Asm *mtcs_dwarf2_asm_new(MtcsMode *mtcsMode)
{
     MtcsDwarf2Asm *self = n_slice_alloc0 (sizeof(MtcsDwarf2Asm));
     mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
     mtcsDwarf2AsmInit(self);
     return self;
}
