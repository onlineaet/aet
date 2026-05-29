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


#ifndef __GCC_MTCS_DWARF2_CFI__
#define __GCC_MTCS_DWARF2_CFI__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcspass.h"
#include "dwarf2out.h"

struct queued_reg_save ;
struct dw_trace_info;
struct trace_info_hasher ;
struct GTY(()) reg_saved_in_data;
struct GTY(()) dw_cfi_row;

/* Signing method used for return address authentication.
   (AArch64 extension)  */
typedef enum
{
  ra_no_signing = 0x0,
  ra_signing_sp = 0x1,
} mtcs_ra_signing_method_t;

/* A collected description of an entire row of the abstract CFI table.  */
struct GTY(()) dw_cfi_row
{
  /* The expression that computes the CFA, expressed in two different ways.
     The CFA member for the simple cases, and the full CFI expression for
     the complex cases.  The later will be a DW_CFA_cfa_expression.  */
  dw_cfa_location cfa;
  dw_cfi_ref cfa_cfi;
  /* The expressions for any register column that is saved.  */
  cfi_vec reg_save;
  /* True if the register window is saved.  */
  bool window_save;
  /* True if the return address is in a mangled state.  */
  mtcs_ra_signing_method_t ra_state;
};

/* The caller's ORIG_REG is saved in SAVED_IN_REG.  */
struct GTY(()) reg_saved_in_data {
  rtx orig_reg;
  rtx saved_in_reg;
};


/* Since we no longer have a proper CFG, we're going to create a facsimile
   of one on the fly while processing the frame-related insns.

   We create dw_trace_info structures for each extended basic block beginning
   and ending at a "save point".  Save points are labels, barriers, certain
   notes, and of course the beginning and end of the function.

   As we encounter control transfer insns, we propagate the "current"
   row state across the edges to the starts of traces.  When checking is
   enabled, we validate that we propagate the same data from all sources.

   All traces are members of the TRACE_INFO array, in the order in which
   they appear in the instruction stream.

   All save points are present in the TRACE_INDEX hash, mapping the insn
   starting a trace to the dw_trace_info describing the trace.  */

struct dw_trace_info
{
  /* The insn that begins the trace.  */
  rtx_insn *head;
  /* The row state at the beginning and end of the trace.  */
  dw_cfi_row *beg_row, *end_row;
  /* Tracking for DW_CFA_GNU_args_size.  The "true" sizes are those we find
     while scanning insns.  However, the args_size value is irrelevant at
     any point except can_throw_internal_p insns.  Therefore the "delay"
     sizes the values that must actually be emitted for this trace.  */
  poly_int64 beg_true_args_size, end_true_args_size;
  poly_int64 beg_delay_args_size, end_delay_args_size;

  /* The first EH insn in the trace, where beg_delay_args_size must be set.  */
  rtx_insn *eh_head;

  /* The following variables contain data used in interpreting frame related
     expressions.  These are not part of the "real" row state as defined by
     Dwarf, but it seems like they need to be propagated into a trace in case
     frame related expressions have been sunk.  */
  /* ??? This seems fragile.  These variables are fragments of a larger
     expression.  If we do not keep the entire expression together, we risk
     not being able to put it together properly.  Consider forcing targets
     to generate self-contained expressions and dropping all of the magic
     interpretation code in this file.  Or at least refusing to shrink wrap
     any frame related insn that doesn't contain a complete expression.  */

  /* The register used for saving registers to the stack, and its offset
     from the CFA.  */
  dw_cfa_location cfa_store;
  /* A temporary register holding an integral value used in adjusting SP
     or setting up the store_reg.  The "offset" field holds the integer
     value, not an offset.  */
  dw_cfa_location cfa_temp;
  /* A set of registers saved in other registers.  This is the inverse of
     the row->reg_save info, if the entry is a DW_CFA_register.  This is
     implemented as a flat array because it normally contains zero or 1
     entry, depending on the target.  IA-64 is the big spender here, using
     a maximum of 5 entries.  */
  vec<reg_saved_in_data> regs_saved_in_regs;
  /* An identifier for this trace.  Used only for debugging dumps.  */
  unsigned id;
  /* True if this trace immediately follows NOTE_INSN_SWITCH_TEXT_SECTIONS.  */
  bool switch_sections;
  /* True if we've seen different values incoming to beg_true_args_size.  */
  bool args_size_undefined;
  /* True if we've seen an insn with a REG_ARGS_SIZE note before EH_HEAD.  */
  bool args_size_defined_for_eh;
};

/* We delay emitting a register save until either (a) we reach the end
   of the prologue or (b) the register is clobbered.  This clusters
   register saves so that there are fewer pc advances.  */
struct queued_reg_save {
  rtx reg;
  rtx saved_reg;
  poly_int64 cfa_offset;
};



typedef struct _MtcsDwarf2Cfi MtcsDwarf2Cfi;


struct _MtcsDwarf2Cfi
{
    MtcsComponent parent;
    /* The variables making up the pseudo-cfg, as described above.  */
     vec<dw_trace_info> trace_info;
     vec<dw_trace_info *> trace_work_list;
     hash_table<trace_info_hasher> *trace_index;


     /* The state of the first row of the FDE table, which includes the
        state provided by the CIE.  */
      GTY(()) dw_cfi_row *cie_cfi_row;

      GTY(()) reg_saved_in_data *cie_return_save;

      GTY(()) unsigned long dwarf2out_cfi_label_num;

     /* The insn after which a new CFI note should be emitted.  */
      rtx_insn *add_cfi_insn;

     /* When non-null, add_cfi will add the CFI to this vector.  */
      cfi_vec *add_cfi_vec;

     /* The current instruction trace.  */
      dw_trace_info *cur_trace;

     /* The current, i.e. most recently generated, row of the CFI table.  */
      dw_cfi_row *cur_row;

     /* A copy of the current CFA, for use during the processing of a
        single insn.  */
      dw_cfa_location *cur_cfa;


       vec<queued_reg_save> queued_reg_saves;

      /* True if any CFI directives were emitted at the current insn.  */
       bool any_cfis_emitted;

      /* Short-hand for commonly used register numbers.  */
       struct cfa_reg dw_stack_pointer_regnum;
       struct cfa_reg dw_frame_pointer_regnum;
       /* Save the result of dwarf2out_do_frame across PCH.
          This variable is tri-state, with 0 unset, >0 true, <0 false.  */
       GTY(()) signed char saved_do_cfi_asm;// = 0;

};



MtcsDwarf2Cfi *mtcs_dwarf2_cfi_new(MtcsMode *mtcsMode);

//原型 expand_builtin_dwarf_sp_column except.h dwarf2cfi.cc
rtx mtcs_dwarf2_cfi_expand_builtin_dwarf_sp_column (MtcsDwarf2Cfi *self);
//原型 expand_builtin_init_dwarf_reg_sizes except.h dwarf2cfi.cc
void mtcs_dwarf2_cfi_expand_builtin_init_dwarf_reg_sizes (MtcsDwarf2Cfi *self,tree address);
//原型 lookup_cfa_1 dwarf2out.h dwarf2cfi.cc
void mtcs_dwarf2_cfi_lookup_cfa_1 (MtcsDwarf2Cfi *self,dw_cfi_ref cfi, dw_cfa_location *loc, dw_cfa_location *remember);
//原型 cfa_equal_p dwarf2out.h dwarf2cfi.cc
bool mtcs_dwarf2_cfi_cfa_equal_p (MtcsDwarf2Cfi *self,const dw_cfa_location *loc1, const dw_cfa_location *loc2);
//原型 output_cfi dwarf2out.h
void mtcs_dwarf2_cfi_output_cfi (MtcsDwarf2Cfi *self,dw_cfi_ref cfi, dw_fde_ref fde, int for_eh);
//原型 output_cfi_directive dwarf2out.h  dwarf2cfi.cc
void mtcs_dwarf2_cfi_output_cfi_directive (MtcsDwarf2Cfi *self, FILE *f,dw_cfi_ref cfi);
//原型 dwarf2out_emit_cfi dwarf2out.h dwarf2cfi.cc
void mtcs_dwarf2_cfi_out_emit_cfi (MtcsDwarf2Cfi *self,dw_cfi_ref cfi);
//原型 dwarf2out_do_eh_frame debug.h dwarf2cfi.cc
bool mtcs_dwarf2_cfi_dwarf2out_do_eh_frame (MtcsDwarf2Cfi *self);
//原型 dwarf2out_do_frame debug.h dwarf2cfi.cc
bool mtcs_dwarf2_cfi_dwarf2out_do_frame (MtcsDwarf2Cfi *self);
//原型 dwarf2out_do_cfi_asm debug.h dwarf2cfi.cc
bool mtcs_dwarf2_cfi_dwarf2out_do_cfi_asm (MtcsDwarf2Cfi *self);
//原型 dwarf2cfi_cc_finalize dwarf2out.h dwarf2cfi.cc
void mtcs_dwarf2_cfi_dwarf2cfi_cc_finalize (MtcsDwarf2Cfi *self);



//原型 NEXT_PASS (pass_dwarf2_frame, 1);  RTL_PASS  dwarf2cfi.cc dwarf2   y 有条件执行 targetm.have_prologue execute_dwarf2_frame
typedef struct _MtcsPassDwarf2Frame  MtcsPassDwarf2Frame;
struct _MtcsPassDwarf2Frame
{
   MtcsPass parent;
};
MtcsPassDwarf2Frame *mtcs_pass_dwarf2_frame_new (MtcsMode *mtcsMode);

#endif
