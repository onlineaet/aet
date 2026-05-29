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
 * base on dwarf2cfi.cc
 */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "target.h"
#include "function.h"
#include "rtl.h"
#include "tree.h"
#include "tree-pass.h"
#include "memmodel.h"
#include "tm_p.h"
#include "emit-rtl.h"
#include "stor-layout.h"
#include "cfgbuild.h"
#include "dwarf2out.h"
#include "dwarf2asm.h"
#include "common/common-target.h"

#include "except.h"     /* expand_builtin_dwarf_sp_column */
#include "profile-count.h" /* For expr.h */
#include "expr.h"    /* init_return_column_size */
#include "output.h"     /* asm_out_file */
#include "debug.h"      /* dwarf2out_do_frame, dwarf2out_do_cfi_asm */
#include "flags.h"      /* dwarf_debuginfo_p */

#include "mtcsdwarf2cfi.h"
#include "mtcstarget.h"
#include "mtcscompile.h"


///* ??? Poison these here until it can be done generically.  They've been
//   totally replaced in this file; make sure it stays that way.  */
//#undef DWARF2_UNWIND_INFO
//#undef DWARF2_FRAME_INFO
//#if (GCC_VERSION >= 3000)
// #pragma GCC poison DWARF2_UNWIND_INFO DWARF2_FRAME_INFO
//#endif
//
//#ifndef INCOMING_RETURN_ADDR_RTX
//#define INCOMING_RETURN_ADDR_RTX  (gcc_unreachable (), NULL_RTX)
//#endif
//
//#ifndef DEFAULT_INCOMING_FRAME_SP_OFFSET
//#define DEFAULT_INCOMING_FRAME_SP_OFFSET INCOMING_FRAME_SP_OFFSET
//#endif

static void dump_cfi_row (MtcsDwarf2Cfi *self,FILE *f, dw_cfi_row *row);

/* Hashtable helpers.  */

struct trace_info_hasher : nofree_ptr_hash <dw_trace_info>
{
  static inline hashval_t hash (const dw_trace_info *);
  static inline bool equal (const dw_trace_info *, const dw_trace_info *);
};

inline hashval_t trace_info_hasher::hash (const dw_trace_info *ti)
{
  return INSN_UID (ti->head);
}

inline bool trace_info_hasher::equal (const dw_trace_info *a, const dw_trace_info *b)
{
  return a->head == b->head;
}

/* A vector of call frame insns for the CIE.  */
//cfi_vec cie_cfi_vec; dwarf2out.cc已定义

/* Hook used by __throw.  */
//原型 expand_builtin_dwarf_sp_column except.h dwarf2cfi.cc
rtx mtcs_dwarf2_cfi_expand_builtin_dwarf_sp_column (MtcsDwarf2Cfi *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   unsigned int dwarf_regnum = mtcs_reg_get_dwarf_frame_regnum/*!DWARF_FRAME_REGNUM*/(mtcsReg,
         mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg));
   return mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,
         mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,dwarf_regnum, 1));
}

/* MEM is a memory reference for the register size table, each element of
   which has mode MODE.  Initialize column C as a return address column.  */
static void init_return_column_size (MtcsDwarf2Cfi *self,scalar_int_mode mode, rtx mem, unsigned int c)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   HOST_WIDE_INT offset = c * mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);
   HOST_WIDE_INT size = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mtcs_mode_get_Pmode(mtcsMode));
   mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,
         mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,mem, mode, offset),
               mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,size, mode));
}

/* Datastructure used by expand_builtin_init_dwarf_reg_sizes and
   init_one_dwarf_reg_size to communicate on what has been done by the
   latter.  */

struct init_one_dwarf_reg_state
{
  /* Whether the dwarf return column was initialized.  */
  bool wrote_return_column;

  /* For each hard register REGNO, whether init_one_dwarf_reg_size
     was given REGNO to process already.  */
  bool processed_regno [MAX_FIRST_PSEUDO_REGISTER];

};

/* Helper for expand_builtin_init_dwarf_reg_sizes.  Generate code to
   initialize the dwarf register size table entry corresponding to register
   REGNO in REGMODE.  TABLE is the table base address, SLOTMODE is the mode to
   use for the size entry to initialize, and INIT_STATE is the communication
   datastructure conveying what we're doing to our caller.  */
static void init_one_dwarf_reg_size (MtcsDwarf2Cfi *self,int regno, machine_mode regmode,
               rtx table, machine_mode slotmode, init_one_dwarf_reg_state *init_state)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   const unsigned int dnum = mtcs_reg_get_dwarf_frame_regnum/*!DWARF_FRAME_REGNUM*/(mtcsReg,regno);
   const unsigned int rnum = mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,dnum, 1);
   const unsigned int dcol = mtcs_reg_get_dwarf_reg_to_unwind_column/*!DWARF_REG_TO_UNWIND_COLUMN*/(mtcsReg,rnum);

   poly_int64 slotoffset = dcol * mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,slotmode);
   poly_int64 regsize = mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,regmode);

   init_state->processed_regno[regno] = true;

   if (rnum >=mtcs_reg_get_dwarf_frame_registers/*!DWARF_FRAME_REGISTERS*/(mtcsReg))
      return;

   if (dnum == mtcs_reg_get_dwarf_frame_return_column/*!DWARF_FRAME_RETURN_COLUMN*/(mtcsReg)){
      if (regmode == VOIDmode)
         return;
      init_state->wrote_return_column = true;
   }

   /* ??? When is this true?  Should it be a test based on DCOL instead?  */
   if (maybe_lt (slotoffset, 0))
      return;

   mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,table, slotmode, slotoffset),
   mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,regsize, slotmode));
}

/* Generate code to initialize the dwarf register size table located
   at the provided ADDRESS.  */
//原型 expand_builtin_init_dwarf_reg_sizes except.h dwarf2cfi.cc
void mtcs_dwarf2_cfi_expand_builtin_init_dwarf_reg_sizes (MtcsDwarf2Cfi *self,tree address)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsConfig *mtcsConfig = mtcs_target_get_config(mtcsTarget);

   unsigned int i;
   scalar_int_mode mode = mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,char_type_node);
   rtx addr = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,address);
   rtx mem = gen_rtx_MEM (mtcsMode->modes.M_BLKmode, addr);

   init_one_dwarf_reg_state init_state;

   memset ((char *)&init_state, 0, sizeof (init_state));

   for (i = 0; i < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i++){
      machine_mode save_mode;
      rtx span;

      /* No point in processing a register multiple times.  This could happen
      with register spans, e.g. when a reg is first processed as a piece of
      a span, then as a register on its own later on.  */

      if (init_state.processed_regno[i])
         continue;

      save_mode = mtcsTarget/*!targetm.dwarf_frame_reg_mode*/->dwarf_frame_reg_mode(mtcsTarget,i);
      span = mtcsTarget/*!targetm.dwarf_register_span*/->dwarf_register_span(mtcsTarget,
            mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,save_mode, i));

      if (!span)
         init_one_dwarf_reg_size(self,i, save_mode, mem, mode, &init_state);
      else{
         for (int si = 0; si < XVECLEN (span, 0); si++){
            rtx reg = XVECEXP (span, 0, si);
            init_one_dwarf_reg_size(self,REGNO (reg), GET_MODE (reg), mem, mode, &init_state);
         }
      }
   }

   if (!init_state.wrote_return_column)
      init_return_column_size(self,mode, mem, mtcs_reg_get_dwarf_frame_return_column/*!DWARF_FRAME_RETURN_COLUMN*/(mtcsReg));
   if(mtcs_config_ifdef(mtcsConfig,MTCS_DWARF_ALT_FRAME_RETURN_COLUMN))
      init_return_column_size(self,mode, mem,
            mtcs_config_get_value/*!DWARF_ALT_FRAME_RETURN_COLUMN*/(mtcsConfig,MTCS_DWARF_ALT_FRAME_RETURN_COLUMN));
   /*!
   #ifdef DWARF_ALT_FRAME_RETURN_COLUMN
   init_return_column_size(self,mode, mem, DWARF_ALT_FRAME_RETURN_COLUMN);
   #endif
   */

   mtcsTarget/*!targetm.init_dwarf_reg_sizes_extra*/->init_dwarf_reg_sizes_extra(mtcsTarget,address);
}

static dw_trace_info * get_trace_info (MtcsDwarf2Cfi *self,rtx_insn *insn)
{
   dw_trace_info dummy;
   dummy.head = insn;
   return self->trace_index->find_with_hash (&dummy, INSN_UID (insn));
}

static bool save_point_p (rtx_insn *insn)
{
   /* Labels, except those that are really jump tables.  */
   if (LABEL_P (insn))
      return inside_basic_block_p (insn);

   /* We split traces at the prologue/epilogue notes because those
   are points at which the unwind info is usually stable.  This
   makes it easier to find spots with identical unwind info so
   that we can use remember/restore_state opcodes.  */
   if (NOTE_P (insn))
      switch (NOTE_KIND (insn)){
         case NOTE_INSN_PROLOGUE_END:
         case NOTE_INSN_EPILOGUE_BEG:
            return true;
      }

   return false;
}

/* Divide OFF by DWARF_CIE_DATA_ALIGNMENT, asserting no remainder.  */
static inline HOST_WIDE_INT div_data_align (MtcsDwarf2Cfi *self,HOST_WIDE_INT off)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAlign  *mtcsAlign=mtcs_target_get_align(mtcsTarget);

   int align = mtcs_align_get_dwarf_cie_data_alignment/*!DWARF_CIE_DATA_ALIGNMENT*/(mtcsAlign);
   HOST_WIDE_INT r = off / align/*!DWARF_CIE_DATA_ALIGNMENT*/;
   gcc_assert (r * align/*!DWARF_CIE_DATA_ALIGNMENT*/ == off);
   return r;
}

/* Return true if we need a signed version of a given opcode
   (e.g. DW_CFA_offset_extended_sf vs DW_CFA_offset_extended).  */
static inline bool need_data_align_sf_opcode (MtcsDwarf2Cfi *self,HOST_WIDE_INT off)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAlign  *mtcsAlign=mtcs_target_get_align(mtcsTarget);

   int align = mtcs_align_get_dwarf_cie_data_alignment/*!DWARF_CIE_DATA_ALIGNMENT*/(mtcsAlign);
   return align/*!DWARF_CIE_DATA_ALIGNMENT*/ < 0 ? off > 0 : off < 0;
}

/* Return a pointer to a newly allocated Call Frame Instruction.  */
static inline dw_cfi_ref new_cfi (void)
{
   dw_cfi_ref cfi = ggc_alloc<dw_cfi_node> ();
   cfi->dw_cfi_oprnd1.dw_cfi_reg_num = 0;
   cfi->dw_cfi_oprnd2.dw_cfi_reg_num = 0;
   return cfi;
}

/* Return a newly allocated CFI row, with no defined data.  */
static dw_cfi_row * new_cfi_row (void)
{
   dw_cfi_row *row = ggc_cleared_alloc<dw_cfi_row> ();
   row->cfa.reg.set_by_dwreg (INVALID_REGNUM);
   return row;
}

/* Return a copy of an existing CFI row.  */
static dw_cfi_row * copy_cfi_row (dw_cfi_row *src)
{
   dw_cfi_row *dst = ggc_alloc<dw_cfi_row> ();
   *dst = *src;
   dst->reg_save = vec_safe_copy (src->reg_save);
   return dst;
}

/* Return a copy of an existing CFA location.  */
static dw_cfa_location *copy_cfa (dw_cfa_location *src)
{
   dw_cfa_location *dst = ggc_alloc<dw_cfa_location> ();
   *dst = *src;
   return dst;
}

/* Generate a new label for the CFI info to refer to.  */
static char *dwarf2out_cfi_label (MtcsDwarf2Cfi *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm = mtcs_target_get_asm(mtcsTarget);

   int num = self->dwarf2out_cfi_label_num++;
   char label[20];
   mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(mtcsAsm,label, "LCFI", num);
   return xstrdup (label);
}

/* Add CFI either to the current insn stream or to a vector, or both.  */
static void add_cfi (MtcsDwarf2Cfi *self,dw_cfi_ref cfi)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit = mtcs_target_get_emit(mtcsTarget);

   self->any_cfis_emitted = true;

   if (self->add_cfi_insn != NULL){
      self->add_cfi_insn = mtcs_emit_emit_note_after/*!emit_note_after*/(mtcsEmit,NOTE_INSN_CFI, self->add_cfi_insn);
      NOTE_CFI (self->add_cfi_insn) = cfi;
   }

   if (self->add_cfi_vec != NULL)
      vec_safe_push (*self->add_cfi_vec, cfi);
}

static void add_cfi_args_size (MtcsDwarf2Cfi *self,poly_int64 size)
{
   /* We don't yet have a representation for polynomial sizes.  */
   HOST_WIDE_INT const_size = size.to_constant ();
   dw_cfi_ref cfi = new_cfi ();
   /* While we can occasionally have args_size < 0 internally, this state
   should not persist at a point we actually need an opcode.  */
   gcc_assert (const_size >= 0);
   cfi->dw_cfi_opc = DW_CFA_GNU_args_size;
   cfi->dw_cfi_oprnd1.dw_cfi_offset = const_size;
   add_cfi(self,cfi);
}

static void add_cfi_restore (MtcsDwarf2Cfi *self,unsigned reg)
{
   dw_cfi_ref cfi = new_cfi ();
   cfi->dw_cfi_opc = (reg & ~0x3f ? DW_CFA_restore_extended : DW_CFA_restore);
   cfi->dw_cfi_oprnd1.dw_cfi_reg_num = reg;
   add_cfi(self,cfi);
}

/* Perform ROW->REG_SAVE[COLUMN] = CFI.  CFI may be null, indicating
   that the register column is no longer saved.  */
static void update_row_reg_save (dw_cfi_row *row, unsigned column, dw_cfi_ref cfi)
{
   if (vec_safe_length (row->reg_save) <= column)
      vec_safe_grow_cleared (row->reg_save, column + 1, true);
   (*row->reg_save)[column] = cfi;
}

/* This function fills in aa dw_cfa_location structure from a dwarf location
   descriptor sequence.  */
static void get_cfa_from_loc_descr (dw_cfa_location *cfa, struct dw_loc_descr_node *loc)
{
   struct dw_loc_descr_node *ptr;
   cfa->offset = 0;
   cfa->base_offset = 0;
   cfa->indirect = 0;
   cfa->reg.set_by_dwreg (INVALID_REGNUM);

   for (ptr = loc; ptr != NULL; ptr = ptr->dw_loc_next){
      enum dwarf_location_atom op = ptr->dw_loc_opc;

      switch (op){
         case DW_OP_reg0:
         case DW_OP_reg1:
         case DW_OP_reg2:
         case DW_OP_reg3:
         case DW_OP_reg4:
         case DW_OP_reg5:
         case DW_OP_reg6:
         case DW_OP_reg7:
         case DW_OP_reg8:
         case DW_OP_reg9:
         case DW_OP_reg10:
         case DW_OP_reg11:
         case DW_OP_reg12:
         case DW_OP_reg13:
         case DW_OP_reg14:
         case DW_OP_reg15:
         case DW_OP_reg16:
         case DW_OP_reg17:
         case DW_OP_reg18:
         case DW_OP_reg19:
         case DW_OP_reg20:
         case DW_OP_reg21:
         case DW_OP_reg22:
         case DW_OP_reg23:
         case DW_OP_reg24:
         case DW_OP_reg25:
         case DW_OP_reg26:
         case DW_OP_reg27:
         case DW_OP_reg28:
         case DW_OP_reg29:
         case DW_OP_reg30:
         case DW_OP_reg31:
            cfa->reg.set_by_dwreg (op - DW_OP_reg0);
            break;
         case DW_OP_regx:
            cfa->reg.set_by_dwreg (ptr->dw_loc_oprnd1.v.val_int);
            break;
         case DW_OP_breg0:
         case DW_OP_breg1:
         case DW_OP_breg2:
         case DW_OP_breg3:
         case DW_OP_breg4:
         case DW_OP_breg5:
         case DW_OP_breg6:
         case DW_OP_breg7:
         case DW_OP_breg8:
         case DW_OP_breg9:
         case DW_OP_breg10:
         case DW_OP_breg11:
         case DW_OP_breg12:
         case DW_OP_breg13:
         case DW_OP_breg14:
         case DW_OP_breg15:
         case DW_OP_breg16:
         case DW_OP_breg17:
         case DW_OP_breg18:
         case DW_OP_breg19:
         case DW_OP_breg20:
         case DW_OP_breg21:
         case DW_OP_breg22:
         case DW_OP_breg23:
         case DW_OP_breg24:
         case DW_OP_breg25:
         case DW_OP_breg26:
         case DW_OP_breg27:
         case DW_OP_breg28:
         case DW_OP_breg29:
         case DW_OP_breg30:
         case DW_OP_breg31:
         case DW_OP_bregx:
            if (cfa->reg.reg == INVALID_REGNUM){
               unsigned regno  = (op == DW_OP_bregx ? ptr->dw_loc_oprnd1.v.val_int : op - DW_OP_breg0);
               cfa->reg.set_by_dwreg (regno);
               cfa->base_offset = ptr->dw_loc_oprnd1.v.val_int;
            }else{
               /* Handle case when span can cover multiple registers.  We
               only support the simple case of consecutive registers
               all with the same size.  DWARF that we are dealing with
               will look something like:
               <DW_OP_bregx: (r49) 0; DW_OP_const1u: 32; DW_OP_shl;
               DW_OP_bregx: (r48) 0; DW_OP_plus> */

               unsigned regno = (op == DW_OP_bregx ? ptr->dw_loc_oprnd1.v.val_int : op - DW_OP_breg0);
               gcc_assert (regno == cfa->reg.reg - 1);
               cfa->reg.span++;
               /* From all the consecutive registers used, we want to set
               cfa->reg.reg to lower number register.  */
               cfa->reg.reg = regno;
               /* The offset was the shift value.  Use it to get the
               span_width and then set it to 0.  */
               cfa->reg.span_width = cfa->offset.to_constant () / 8;
               cfa->offset = 0;
            }
            break;
         case DW_OP_deref:
            cfa->indirect = 1;
            break;
         case DW_OP_shl:
            break;
         case DW_OP_lit0:
         case DW_OP_lit1:
         case DW_OP_lit2:
         case DW_OP_lit3:
         case DW_OP_lit4:
         case DW_OP_lit5:
         case DW_OP_lit6:
         case DW_OP_lit7:
         case DW_OP_lit8:
         case DW_OP_lit9:
         case DW_OP_lit10:
         case DW_OP_lit11:
         case DW_OP_lit12:
         case DW_OP_lit13:
         case DW_OP_lit14:
         case DW_OP_lit15:
         case DW_OP_lit16:
         case DW_OP_lit17:
         case DW_OP_lit18:
         case DW_OP_lit19:
         case DW_OP_lit20:
         case DW_OP_lit21:
         case DW_OP_lit22:
         case DW_OP_lit23:
         case DW_OP_lit24:
         case DW_OP_lit25:
         case DW_OP_lit26:
         case DW_OP_lit27:
         case DW_OP_lit28:
         case DW_OP_lit29:
         case DW_OP_lit30:
         case DW_OP_lit31:
            gcc_assert (known_eq (cfa->offset, 0));
            cfa->offset = op - DW_OP_lit0;
            break;
         case DW_OP_const1u:
         case DW_OP_const1s:
         case DW_OP_const2u:
         case DW_OP_const2s:
         case DW_OP_const4s:
         case DW_OP_const8s:
         case DW_OP_constu:
         case DW_OP_consts:
            gcc_assert (known_eq (cfa->offset, 0));
            cfa->offset = ptr->dw_loc_oprnd1.v.val_int;
            break;
         case DW_OP_minus:
            cfa->offset = -cfa->offset;
            break;
         case DW_OP_plus:
            /* The offset is already in place.  */
            break;
         case DW_OP_plus_uconst:
            cfa->offset = ptr->dw_loc_oprnd1.v.val_unsigned;
            break;
         default:
            gcc_unreachable ();
      }
   }
}

/* Find the previous value for the CFA, iteratively.  CFI is the opcode
   to interpret, *LOC will be updated as necessary, *REMEMBER is used for
   one level of remember/restore state processing.  */
//原型 lookup_cfa_1 dwarf2out.h dwarf2cfi.cc
void mtcs_dwarf2_cfi_lookup_cfa_1 (MtcsDwarf2Cfi *self,dw_cfi_ref cfi, dw_cfa_location *loc, dw_cfa_location *remember)
{
   switch (cfi->dw_cfi_opc){
      case DW_CFA_def_cfa_offset:
      case DW_CFA_def_cfa_offset_sf:
         loc->offset = cfi->dw_cfi_oprnd1.dw_cfi_offset;
         break;
      case DW_CFA_def_cfa_register:
         loc->reg.set_by_dwreg (cfi->dw_cfi_oprnd1.dw_cfi_reg_num);
         break;
      case DW_CFA_def_cfa:
      case DW_CFA_def_cfa_sf:
         loc->reg.set_by_dwreg (cfi->dw_cfi_oprnd1.dw_cfi_reg_num);
         loc->offset = cfi->dw_cfi_oprnd2.dw_cfi_offset;
         break;
      case DW_CFA_def_cfa_expression:
         if (cfi->dw_cfi_oprnd2.dw_cfi_cfa_loc)
            *loc = *cfi->dw_cfi_oprnd2.dw_cfi_cfa_loc;
         else
            get_cfa_from_loc_descr (loc, cfi->dw_cfi_oprnd1.dw_cfi_loc);
         break;

      case DW_CFA_remember_state:
         gcc_assert (!remember->in_use);
         *remember = *loc;
         remember->in_use = 1;
         break;
      case DW_CFA_restore_state:
         gcc_assert (remember->in_use);
         *loc = *remember;
         remember->in_use = 0;
         break;

      default:
         break;
   }
}

/* Determine if two dw_cfa_location structures define the same data.  */
//原型 cfa_equal_p dwarf2out.h dwarf2cfi.cc
bool mtcs_dwarf2_cfi_cfa_equal_p (MtcsDwarf2Cfi *self,const dw_cfa_location *loc1, const dw_cfa_location *loc2)
{
   return (loc1->reg == loc2->reg
      && known_eq (loc1->offset, loc2->offset)
      && loc1->indirect == loc2->indirect
      && (loc1->indirect == 0  || known_eq (loc1->base_offset, loc2->base_offset)));
}

/* Determine if two CFI operands are identical.  */
static bool cfi_oprnd_equal_p (MtcsDwarf2Cfi *self,enum dw_cfi_oprnd_type t, dw_cfi_oprnd *a, dw_cfi_oprnd *b)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsDwarf2Out   *mtcsDwarf2Out=mtcs_target_get_dwarf2_out(mtcsTarget);

   switch (t){
      case dw_cfi_oprnd_unused:
         return true;
      case dw_cfi_oprnd_reg_num:
         return a->dw_cfi_reg_num == b->dw_cfi_reg_num;
      case dw_cfi_oprnd_offset:
         return a->dw_cfi_offset == b->dw_cfi_offset;
      case dw_cfi_oprnd_addr:
         return (a->dw_cfi_addr == b->dw_cfi_addr || strcmp (a->dw_cfi_addr, b->dw_cfi_addr) == 0);
      case dw_cfi_oprnd_loc:
         return mtcs_dwarf2_out_loc_descr_equal_p/*!loc_descr_equal_p*/(mtcsDwarf2Out,a->dw_cfi_loc, b->dw_cfi_loc);
      case dw_cfi_oprnd_cfa_loc:
         /* If any of them is NULL, don't dereference either.  */
         if (!a->dw_cfi_cfa_loc || !b->dw_cfi_cfa_loc)
            return a->dw_cfi_cfa_loc == b->dw_cfi_cfa_loc;
         return mtcs_dwarf2_cfi_cfa_equal_p/*!cfa_equal_p*/(self,a->dw_cfi_cfa_loc, b->dw_cfi_cfa_loc);
   }
   gcc_unreachable ();
}

/* Determine if two CFI entries are identical.  */
static bool cfi_equal_p (MtcsDwarf2Cfi *self,dw_cfi_ref a, dw_cfi_ref b)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsDwarf2Out   *mtcsDwarf2Out=mtcs_target_get_dwarf2_out(mtcsTarget);

   enum dwarf_call_frame_info opc;

   /* Make things easier for our callers, including missing operands.  */
   if (a == b)
      return true;
   if (a == NULL || b == NULL)
      return false;

   /* Obviously, the opcodes must match.  */
   opc = a->dw_cfi_opc;
   if (opc != b->dw_cfi_opc)
      return false;

   /* Compare the two operands, re-using the type of the operands as
   already exposed elsewhere.  */
   return (cfi_oprnd_equal_p(self,mtcs_dwarf2_out_dw_cfi_oprnd1_desc/*!dw_cfi_oprnd1_desc*/(mtcsDwarf2Out,opc),
         &a->dw_cfi_oprnd1, &b->dw_cfi_oprnd1)
         && cfi_oprnd_equal_p(self,mtcs_dwarf2_out_dw_cfi_oprnd2_desc/*!dw_cfi_oprnd2_desc*/(mtcsDwarf2Out,opc),
               &a->dw_cfi_oprnd2, &b->dw_cfi_oprnd2));
}

/* Determine if two CFI_ROW structures are identical.  */
static bool cfi_row_equal_p (MtcsDwarf2Cfi *self,dw_cfi_row *a, dw_cfi_row *b)
{
   size_t i, n_a, n_b, n_max;

   if (a->cfa_cfi){
      if (!cfi_equal_p(self,a->cfa_cfi, b->cfa_cfi))
         return false;
   }else if (!mtcs_dwarf2_cfi_cfa_equal_p/*!cfa_equal_p*/(self,&a->cfa, &b->cfa))
      return false;

   n_a = vec_safe_length (a->reg_save);
   n_b = vec_safe_length (b->reg_save);
   n_max = MAX (n_a, n_b);

   for (i = 0; i < n_max; ++i){
      dw_cfi_ref r_a = NULL, r_b = NULL;

      if (i < n_a)
         r_a = (*a->reg_save)[i];
      if (i < n_b)
         r_b = (*b->reg_save)[i];

      if (!cfi_equal_p(self,r_a, r_b))
         return false;
   }

   if (a->window_save != b->window_save)
      return false;

   if (a->ra_state != b->ra_state)
      return false;

   return true;
}

/* The CFA is now calculated from NEW_CFA.  Consider OLD_CFA in determining
   what opcode to emit.  Returns the CFI opcode to effect the change, or
   NULL if NEW_CFA == OLD_CFA.  */
static dw_cfi_ref def_cfa_0 (MtcsDwarf2Cfi *self,dw_cfa_location *old_cfa, dw_cfa_location *new_cfa)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsDwarf2Out   *mtcsDwarf2Out=mtcs_target_get_dwarf2_out(mtcsTarget);

   dw_cfi_ref cfi;

   /* If nothing changed, no need to issue any call frame instructions.  */
   if (mtcs_dwarf2_cfi_cfa_equal_p/*!cfa_equal_p*/(self,old_cfa, new_cfa))
      return NULL;

   cfi = new_cfi ();

   HOST_WIDE_INT const_offset;
   if (new_cfa->reg == old_cfa->reg
   && new_cfa->reg.span == 1
   && !new_cfa->indirect
   && !old_cfa->indirect
   && new_cfa->offset.is_constant (&const_offset)){
      /* Construct a "DW_CFA_def_cfa_offset <offset>" instruction, indicating
      the CFA register did not change but the offset did.  The data
      factoring for DW_CFA_def_cfa_offset_sf happens in output_cfi, or
      in the assembler via the .cfi_def_cfa_offset directive.  */
      if (const_offset < 0)
         cfi->dw_cfi_opc = DW_CFA_def_cfa_offset_sf;
      else
         cfi->dw_cfi_opc = DW_CFA_def_cfa_offset;
      cfi->dw_cfi_oprnd1.dw_cfi_offset = const_offset;
   }else if (new_cfa->offset.is_constant ()
   && known_eq (new_cfa->offset, old_cfa->offset)
   && old_cfa->reg.reg != INVALID_REGNUM
   && new_cfa->reg.span == 1
   && !new_cfa->indirect
   && !old_cfa->indirect){
      /* Construct a "DW_CFA_def_cfa_register <register>" instruction,
      indicating the CFA register has changed to <register> but the
      offset has not changed.  This requires the old CFA to have
      been set as a register plus offset rather than a general
      DW_CFA_def_cfa_expression.  */
      cfi->dw_cfi_opc = DW_CFA_def_cfa_register;
      cfi->dw_cfi_oprnd1.dw_cfi_reg_num = new_cfa->reg.reg;
   }else if (new_cfa->indirect == 0
   && new_cfa->offset.is_constant (&const_offset)
   && new_cfa->reg.span == 1){
      /* Construct a "DW_CFA_def_cfa <register> <offset>" instruction,
      indicating the CFA register has changed to <register> with
      the specified offset.  The data factoring for DW_CFA_def_cfa_sf
      happens in output_cfi, or in the assembler via the .cfi_def_cfa
      directive.  */
      if (const_offset < 0)
         cfi->dw_cfi_opc = DW_CFA_def_cfa_sf;
      else
         cfi->dw_cfi_opc = DW_CFA_def_cfa;
      cfi->dw_cfi_oprnd1.dw_cfi_reg_num = new_cfa->reg.reg;
      cfi->dw_cfi_oprnd2.dw_cfi_offset = const_offset;
   }else{
      /* Construct a DW_CFA_def_cfa_expression instruction to
      calculate the CFA using a full location expression since no
      register-offset pair is available.  */
      struct dw_loc_descr_node *loc_list;

      cfi->dw_cfi_opc = DW_CFA_def_cfa_expression;
      loc_list = mtcs_dwarf2_out_build_cfa_loc/*!build_cfa_loc*/(mtcsDwarf2Out,new_cfa, 0);
      cfi->dw_cfi_oprnd1.dw_cfi_loc = loc_list;
      if (!new_cfa->offset.is_constant () || !new_cfa->base_offset.is_constant ())
      /* It's hard to reconstruct the CFA location for a polynomial
      expression, so just cache it instead.  */
         cfi->dw_cfi_oprnd2.dw_cfi_cfa_loc = copy_cfa (new_cfa);
      else
         cfi->dw_cfi_oprnd2.dw_cfi_cfa_loc = NULL;
   }

   return cfi;
}

/* Similarly, but take OLD_CFA from CUR_ROW, and update it after the fact.  */
static void def_cfa_1 (MtcsDwarf2Cfi *self,dw_cfa_location *new_cfa)
{
   dw_cfi_ref cfi;

   if (self->cur_trace->cfa_store.reg == new_cfa->reg && new_cfa->indirect == 0)
      self->cur_trace->cfa_store.offset = new_cfa->offset;

   cfi = def_cfa_0(self,&self->cur_row->cfa, new_cfa);
   if (cfi){
      self->cur_row->cfa = *new_cfa;
      self->cur_row->cfa_cfi = (cfi->dw_cfi_opc == DW_CFA_def_cfa_expression ? cfi : NULL);
      add_cfi(self,cfi);
   }
}

/* Add the CFI for saving a register.  REG is the CFA column number.
   If SREG is INVALID_REGISTER, the register is saved at OFFSET from the CFA;
   otherwise it is saved in SREG.  */
static void reg_save (MtcsDwarf2Cfi *self,unsigned int reg, struct cfa_reg sreg, poly_int64 offset)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsDwarf2Out   *mtcsDwarf2Out=mtcs_target_get_dwarf2_out(mtcsTarget);

   dw_fde_ref fde = cfun ? cfun->fde : NULL;
   dw_cfi_ref cfi = new_cfi ();

   cfi->dw_cfi_oprnd1.dw_cfi_reg_num = reg;

   if (sreg.reg == INVALID_REGNUM){
      HOST_WIDE_INT const_offset;
      /* When stack is aligned, store REG using DW_CFA_expression with FP.  */
      if (fde && fde->stack_realign){
         cfi->dw_cfi_opc = DW_CFA_expression;
         cfi->dw_cfi_oprnd1.dw_cfi_reg_num = reg;
         cfi->dw_cfi_oprnd2.dw_cfi_loc = mtcs_dwarf2_out_build_cfa_aligned_loc/*!build_cfa_aligned_loc*/(mtcsDwarf2Out,
               &self->cur_row->cfa, offset, fde->stack_realignment);
      }else if (offset.is_constant (&const_offset)){
         if (need_data_align_sf_opcode(self,const_offset))
            cfi->dw_cfi_opc = DW_CFA_offset_extended_sf;
         else if (reg & ~0x3f)
            cfi->dw_cfi_opc = DW_CFA_offset_extended;
         else
            cfi->dw_cfi_opc = DW_CFA_offset;
         cfi->dw_cfi_oprnd2.dw_cfi_offset = const_offset;
      }else{
         cfi->dw_cfi_opc = DW_CFA_expression;
         cfi->dw_cfi_oprnd1.dw_cfi_reg_num = reg;
         cfi->dw_cfi_oprnd2.dw_cfi_loc = mtcs_dwarf2_out_build_cfa_loc/*!build_cfa_loc*/(mtcsDwarf2Out,&self->cur_row->cfa, offset);
      }
   }else if (sreg.reg == reg){
      /* While we could emit something like DW_CFA_same_value or
      DW_CFA_restore, we never expect to see something like that
      in a prologue.  This is more likely to be a bug.  A backend
      can always bypass this by using REG_CFA_RESTORE directly.  */
      gcc_unreachable ();
   }else if (sreg.span > 1){
      cfi->dw_cfi_opc = DW_CFA_expression;
      cfi->dw_cfi_oprnd1.dw_cfi_reg_num = reg;
      cfi->dw_cfi_oprnd2.dw_cfi_loc = mtcs_dwarf2_out_build_span_loc/*!build_span_loc*/(mtcsDwarf2Out,sreg);
   }else{
      cfi->dw_cfi_opc = DW_CFA_register;
      cfi->dw_cfi_oprnd2.dw_cfi_reg_num = sreg.reg;
   }

   add_cfi(self,cfi);
   update_row_reg_save (self->cur_row, reg, cfi);
}

/* A subroutine of scan_trace.  Check INSN for a REG_ARGS_SIZE note
   and adjust data structures to match.  */
static void notice_args_size (MtcsDwarf2Cfi *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   poly_int64 args_size, delta;
   rtx note;
   note = find_reg_note (insn, REG_ARGS_SIZE, NULL);
   if (note == NULL)
      return;

   if (!self->cur_trace->eh_head)
      self->cur_trace->args_size_defined_for_eh = true;

   args_size = get_args_size (note);
   delta = args_size - self->cur_trace->end_true_args_size;
   if (known_eq (delta, 0))
      return;

   self->cur_trace->end_true_args_size = args_size;
   /* If the CFA is computed off the stack pointer, then we must adjust
   the computation of the CFA as well.  */
   if (self->cur_cfa->reg == self->dw_stack_pointer_regnum){
      gcc_assert (!self->cur_cfa->indirect);
      /* Convert a change in args_size (always a positive in the
      direction of stack growth) to a change in stack pointer.  */
      if (!mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc))
         delta = -delta;
      self->cur_cfa->offset += delta;
   }
}

/* A subroutine of scan_trace.  INSN is can_throw_internal.  Update the
   data within the trace related to EH insns and args_size.  */
static void notice_eh_throw (MtcsDwarf2Cfi *self,rtx_insn *insn)
{
   poly_int64 args_size = self->cur_trace->end_true_args_size;
   if (self->cur_trace->eh_head == NULL){
      self->cur_trace->eh_head = insn;
      self->cur_trace->beg_delay_args_size = args_size;
      self->cur_trace->end_delay_args_size = args_size;
   }else if (maybe_ne (self->cur_trace->end_delay_args_size, args_size)){
      self->cur_trace->end_delay_args_size = args_size;
      /* ??? If the CFA is the stack pointer, search backward for the last
      CFI note and insert there.  Given that the stack changed for the
      args_size change, there *must* be such a note in between here and
      the last eh insn.  */
      add_cfi_args_size(self,args_size);
   }
}

/* Short-hand inline for the very common D_F_R (REGNO (x)) operation.  */
/* ??? This ought to go into dwarf2out.h, except that dwarf2out.h is
   used in places where rtl is prohibited.  */
static inline unsigned dwf_regno (MtcsDwarf2Cfi *self,const_rtx reg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg = mtcs_target_get_reg(mtcsTarget);

   gcc_assert (REGNO (reg) < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg));
   return mtcs_reg_get_dwarf_frame_regnum/*!DWARF_FRAME_REGNUM*/(mtcsReg,REGNO (reg));
}


/* Like dwf_regno, but when the value can span multiple registers.  */
static struct cfa_reg dwf_cfa_reg (MtcsDwarf2Cfi *self,rtx reg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg = mtcs_target_get_reg(mtcsTarget);

   struct cfa_reg result;
   result.reg = dwf_regno(self,reg);
   result.span = 1;
   result.span_width = 0;

   rtx span = mtcsTarget/*!targetm.dwarf_register_span*/->dwarf_register_span(mtcsTarget,reg);
   if (span){
      /* We only support the simple case of consecutive registers all with the
      same size.  */
      result.span = XVECLEN (span, 0);
      result.span_width =mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (XVECEXP (span, 0, 0))).to_constant ();

      if (CHECKING_P){
         /* Ensure that the above assumption is accurate.  */
         for (unsigned int i = 0; i < result.span; i++){
            gcc_assert (mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,
                  GET_MODE (XVECEXP (span, 0, i))).to_constant ()  == result.span_width);
            gcc_assert (REG_P (XVECEXP (span, 0, i)));
            gcc_assert (dwf_regno(self,XVECEXP (span, 0, i)) == result.reg + i);
         }
      }
   }
   return result;
}
/* More efficient comparisons that don't call targetm.dwarf_register_span
   unnecessarily.  These cfa_reg vs. rtx comparisons should be done at
   least for call-saved REGs that might not be CFA related (like stack
   pointer, hard frame pointer or DRAP registers are), in other cases it is
   just a compile time and memory optimization.  */
static bool operator== (cfa_reg &cfa, rtx reg)
{
  MtcsTarget *mtcsTarget = mtcs_compile_get_current_target(mtcs_compile_get());
  MtcsDwarf2Cfi *mtcsDwarf2Cfi = mtcs_target_get_dwarf2_cfi(mtcsTarget);
  unsigned int regno = dwf_regno(mtcsDwarf2Cfi,reg);
  if (cfa.reg != regno)
    return false;
  struct cfa_reg other = dwf_cfa_reg(mtcsDwarf2Cfi,reg);
  return cfa == other;
}

static inline bool operator!= (cfa_reg &cfa, rtx reg)
{
  return !(cfa == reg);
}

/* Compare X and Y for equivalence.  The inputs may be REGs or PC_RTX.  */
static bool compare_reg_or_pc (rtx x, rtx y)
{
   if (REG_P (x) && REG_P (y))
      return REGNO (x) == REGNO (y);
   return x == y;
}

/* Record SRC as being saved in DEST.  DEST may be null to delete an
   existing entry.  SRC may be a register or PC_RTX.  */
static void record_reg_saved_in_reg (MtcsDwarf2Cfi *self,rtx dest, rtx src)
{
   reg_saved_in_data *elt;
   size_t i;

   FOR_EACH_VEC_ELT (self->cur_trace->regs_saved_in_regs, i, elt)
      if (compare_reg_or_pc (elt->orig_reg, src)){
         if (dest == NULL)
            self->cur_trace->regs_saved_in_regs.unordered_remove (i);
         else
            elt->saved_in_reg = dest;
         return;
      }

   if (dest == NULL)
      return;

   reg_saved_in_data e = {src, dest};
   self->cur_trace->regs_saved_in_regs.safe_push (e);
}

/* Add an entry to QUEUED_REG_SAVES saying that REG is now saved at
   SREG, or if SREG is NULL then it is saved at OFFSET to the CFA.  */
static void queue_reg_save (MtcsDwarf2Cfi *self,rtx reg, rtx sreg, poly_int64 offset)
{
   queued_reg_save *q;
   queued_reg_save e = {reg, sreg, offset};
   size_t i;

   /* Duplicates waste space, but it's also necessary to remove them
   for correctness, since the queue gets output in reverse order.  */
   FOR_EACH_VEC_ELT (self->queued_reg_saves, i, q)
      if (compare_reg_or_pc (q->reg, reg)){
         *q = e;
         return;
      }

   self->queued_reg_saves.safe_push (e);
}

/* Output all the entries in QUEUED_REG_SAVES.  */
static void dwarf2out_flush_queued_reg_saves (MtcsDwarf2Cfi *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   queued_reg_save *q;
   size_t i;
   FOR_EACH_VEC_ELT (self->queued_reg_saves, i, q){
      unsigned int reg;
      struct cfa_reg sreg;

      record_reg_saved_in_reg(self,q->saved_reg, q->reg);

      if (q->reg == pc_rtx)
         reg = mtcs_reg_get_dwarf_frame_return_column/*!DWARF_FRAME_RETURN_COLUMN*/(mtcsReg);
      else
         reg = dwf_regno(self,q->reg);
      if (q->saved_reg)
         sreg = dwf_cfa_reg(self,q->saved_reg);
      else
         sreg.set_by_dwreg (INVALID_REGNUM);
      reg_save(self,reg, sreg, q->cfa_offset);
   }
   self->queued_reg_saves.truncate (0);
}

/* Does INSN clobber any register which QUEUED_REG_SAVES lists a saved
   location for?  Or, does it clobber a register which we've previously
   said that some other register is saved in, and for which we now
   have a new location for?  */
static bool clobbers_queued_reg_save (MtcsDwarf2Cfi *self,const_rtx insn)
{
   queued_reg_save *q;
   size_t iq;
   FOR_EACH_VEC_ELT (self->queued_reg_saves, iq, q){
      size_t ir;
      reg_saved_in_data *rir;
      if (modified_in_p (q->reg, insn))
         return true;
      FOR_EACH_VEC_ELT (self->cur_trace->regs_saved_in_regs, ir, rir)
         if (compare_reg_or_pc (q->reg, rir->orig_reg)  && modified_in_p (rir->saved_in_reg, insn))
            return true;
   }
   return false;
}

/* What register, if any, is currently saved in REG?  */
static rtx reg_saved_in (MtcsDwarf2Cfi *self,rtx reg)
{
   unsigned int regn = REGNO (reg);
   queued_reg_save *q;
   reg_saved_in_data *rir;
   size_t i;

   FOR_EACH_VEC_ELT (self->queued_reg_saves, i, q)
      if (q->saved_reg && regn == REGNO (q->saved_reg))
         return q->reg;

   FOR_EACH_VEC_ELT (self->cur_trace->regs_saved_in_regs, i, rir)
      if (regn == REGNO (rir->saved_in_reg))
         return rir->orig_reg;

   return NULL_RTX;
}

/* A subroutine of dwarf2out_frame_debug, process a REG_DEF_CFA note.  */
static void dwarf2out_frame_debug_def_cfa (MtcsDwarf2Cfi *self,rtx pat)
{
   memset (self->cur_cfa, 0, sizeof (*self->cur_cfa));
   pat = strip_offset (pat, &self->cur_cfa->offset);
   if (MEM_P (pat)){
      self->cur_cfa->indirect = 1;
      pat = strip_offset (XEXP (pat, 0), &self->cur_cfa->base_offset);
   }
   /* ??? If this fails, we could be calling into the _loc functions to
   define a full expression.  So far no port does that.  */
   gcc_assert (REG_P (pat));
   self->cur_cfa->reg = dwf_cfa_reg(self,pat);
}

/* A subroutine of dwarf2out_frame_debug, process a REG_ADJUST_CFA note.  */
static void dwarf2out_frame_debug_adjust_cfa (MtcsDwarf2Cfi *self,rtx pat)
{
   rtx src, dest;

   gcc_assert (GET_CODE (pat) == SET);
   dest = XEXP (pat, 0);
   src = XEXP (pat, 1);

   switch (GET_CODE (src)){
      case PLUS:
         gcc_assert (self->cur_cfa->reg == XEXP (src, 0));
         self->cur_cfa->offset -= rtx_to_poly_int64 (XEXP (src, 1));
         break;

      case REG:
         break;

      default:
         gcc_unreachable ();
   }

   self->cur_cfa->reg = dwf_cfa_reg(self,dest);
   gcc_assert (self->cur_cfa->indirect == 0);
}

/* A subroutine of dwarf2out_frame_debug, process a REG_CFA_OFFSET note.  */
static void dwarf2out_frame_debug_cfa_offset (MtcsDwarf2Cfi *self,rtx set)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   poly_int64 offset;
   rtx src, addr, span;
   unsigned int sregno;

   src = XEXP (set, 1);
   addr = XEXP (set, 0);
   gcc_assert (MEM_P (addr));
   addr = XEXP (addr, 0);

   /* As documented, only consider extremely simple addresses.  */
   switch (GET_CODE (addr)){
      case REG:
         gcc_assert (self->cur_cfa->reg == addr);
         offset = -self->cur_cfa->offset;
         break;
      case PLUS:
         gcc_assert (self->cur_cfa->reg == XEXP (addr, 0));
         offset = rtx_to_poly_int64 (XEXP (addr, 1)) - self->cur_cfa->offset;
         break;
      default:
         gcc_unreachable ();
   }

   if (src == pc_rtx){
      span = NULL;
      sregno = mtcs_reg_get_dwarf_frame_return_column/*!DWARF_FRAME_RETURN_COLUMN*/(mtcsReg);
   }else{
      span = mtcsTarget/*!targetm.dwarf_register_span*/->dwarf_register_span(mtcsTarget,src);
      sregno = dwf_regno(self,src);
   }

   /* ??? We'd like to use queue_reg_save, but we need to come up with
   a different flushing heuristic for epilogues.  */
   struct cfa_reg invalid;
   invalid.set_by_dwreg (INVALID_REGNUM);
   if (!span)
      reg_save(self,sregno, invalid, offset);
   else{
      /* We have a PARALLEL describing where the contents of SRC live.
      Adjust the offset for each piece of the PARALLEL.  */
      poly_int64 span_offset = offset;

      gcc_assert (GET_CODE (span) == PARALLEL);

      const int par_len = XVECLEN (span, 0);
      for (int par_index = 0; par_index < par_len; par_index++){
         rtx elem = XVECEXP (span, 0, par_index);
         sregno = dwf_regno(self,src);
         reg_save(self,sregno, invalid, span_offset);
         span_offset += mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (elem));
      }
   }
}

/* A subroutine of dwarf2out_frame_debug, process a REG_CFA_REGISTER note.  */
static void dwarf2out_frame_debug_cfa_register (MtcsDwarf2Cfi *self,rtx set)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   rtx src, dest;
   unsigned sregno;
   struct cfa_reg dregno;
   src = XEXP (set, 1);
   dest = XEXP (set, 0);
   record_reg_saved_in_reg(self,dest, src);
   if (src == pc_rtx)
      sregno = mtcs_reg_get_dwarf_frame_return_column/*!DWARF_FRAME_RETURN_COLUMN*/(mtcsReg);
   else
      sregno = dwf_regno(self,src);
   dregno = dwf_cfa_reg(self,dest);
   /* ??? We'd like to use queue_reg_save, but we need to come up with
   a different flushing heuristic for epilogues.  */
   reg_save(self,sregno, dregno, 0);
}

/* A subroutine of dwarf2out_frame_debug, process a REG_CFA_EXPRESSION note.  */
static void dwarf2out_frame_debug_cfa_expression (MtcsDwarf2Cfi *self,rtx set)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsDwarf2Out   *mtcsDwarf2Out=mtcs_target_get_dwarf2_out(mtcsTarget);

   rtx src, dest, span;
   dw_cfi_ref cfi = new_cfi ();
   unsigned regno;
   dest = SET_DEST (set);
   src = SET_SRC (set);

   gcc_assert (REG_P (src));
   gcc_assert (MEM_P (dest));
   span = mtcsTarget/*!targetm.dwarf_register_span*/->dwarf_register_span(mtcsTarget,src);
   gcc_assert (!span);
   regno = dwf_regno(self,src);
   cfi->dw_cfi_opc = DW_CFA_expression;
   cfi->dw_cfi_oprnd1.dw_cfi_reg_num = regno;
   cfi->dw_cfi_oprnd2.dw_cfi_loc = mtcs_dwarf2_out_mem_loc_descriptor/*!mem_loc_descriptor*/(mtcsDwarf2Out,XEXP (dest, 0),
         mtcs_rtl_get_address_mode/*!get_address_mode*/(mtcsRTL,dest),GET_MODE (dest), VAR_INIT_STATUS_INITIALIZED);

   /* ??? We'd like to use queue_reg_save, were the interface different,
   and, as above, we could manage flushing for epilogues.  */
   add_cfi(self,cfi);
   update_row_reg_save (self->cur_row, regno, cfi);
}

/* A subroutine of dwarf2out_frame_debug, process a REG_CFA_VAL_EXPRESSION
   note.  */
static void dwarf2out_frame_debug_cfa_val_expression (MtcsDwarf2Cfi *self,rtx set)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsDwarf2Out *mtcsDwarf2Out =mtcs_target_get_dwarf2_out(mtcsTarget);

   rtx dest = SET_DEST (set);
   gcc_assert (REG_P (dest));

   rtx span = mtcsTarget/*!targetm.dwarf_register_span*/->dwarf_register_span(mtcsTarget,dest);
   gcc_assert (!span);

   rtx src = SET_SRC (set);
   dw_cfi_ref cfi = new_cfi ();
   cfi->dw_cfi_opc = DW_CFA_val_expression;
   cfi->dw_cfi_oprnd1.dw_cfi_reg_num = dwf_regno(self,dest);
   cfi->dw_cfi_oprnd2.dw_cfi_loc = mtcs_dwarf2_out_mem_loc_descriptor/*!mem_loc_descriptor*/(mtcsDwarf2Out,
         src, GET_MODE (src), GET_MODE (dest), VAR_INIT_STATUS_INITIALIZED);
   add_cfi(self,cfi);
   update_row_reg_save (self->cur_row, dwf_regno(self,dest), cfi);
}

/* A subroutine of dwarf2out_frame_debug, process a REG_CFA_RESTORE
   note. When called with EMIT_CFI set to false emitting a CFI
   statement is suppressed.  */
static void dwarf2out_frame_debug_cfa_restore (MtcsDwarf2Cfi *self,rtx reg, bool emit_cfi)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;

   gcc_assert (REG_P (reg));

   rtx span = mtcsTarget/*!targetm.dwarf_register_span*/->dwarf_register_span(mtcsTarget,reg);
   if (!span){
      unsigned int regno = dwf_regno(self,reg);
      if (emit_cfi)
         add_cfi_restore(self,regno);
      update_row_reg_save (self->cur_row, regno, NULL);
   }else{
      /* We have a PARALLEL describing where the contents of REG live.
      Restore the register for each piece of the PARALLEL.  */
      gcc_assert (GET_CODE (span) == PARALLEL);

      const int par_len = XVECLEN (span, 0);
      for (int par_index = 0; par_index < par_len; par_index++){
         reg = XVECEXP (span, 0, par_index);
         gcc_assert (REG_P (reg));
         unsigned int regno = dwf_regno(self,reg);
         if (emit_cfi)
            add_cfi_restore(self,regno);
         update_row_reg_save (self->cur_row, regno, NULL);
      }
   }
}

/* A subroutine of dwarf2out_frame_debug, process a REG_CFA_WINDOW_SAVE.

   ??? Perhaps we should note in the CIE where windows are saved (instead
   of assuming 0(cfa)) and what registers are in the window.  */
static void dwarf2out_frame_debug_cfa_window_save (MtcsDwarf2Cfi *self)
{
   dw_cfi_ref cfi = new_cfi ();
   cfi->dw_cfi_opc = DW_CFA_GNU_window_save;
   add_cfi(self,cfi);
   self->cur_row->window_save = true;
}

/* A subroutine of dwarf2out_frame_debug, process a REG_CFA_TOGGLE_RA_MANGLE.
   Note: DW_CFA_GNU_window_save dwarf opcode is reused for toggling RA mangle
   state, this is a target specific operation on AArch64 and can only be used
   on other targets if they don't use the window save operation otherwise.  */
//gcc14的代码，被gcc15dwarf2out_frame_debug_cfa_negate_ra_state替换
//static void dwarf2out_frame_debug_cfa_toggle_ra_mangle (MtcsDwarf2Cfi *self)
//{
//   dw_cfi_ref cfi = new_cfi ();
//   cfi->dw_cfi_opc = DW_CFA_GNU_window_save;
//   add_cfi(self,cfi);
//   self->cur_row->ra_state = !self->cur_row->ra_state;
//}

/* A subroutine of dwarf2out_frame_debug, process REG_CFA_NEGATE_RA_STATE.  */
//gcc15     Rename REG_CFA_TOGGLE_RA_MANGLE to REG_CFA_NEGATE_RA_STATE
static void dwarf2out_frame_debug_cfa_negate_ra_state (MtcsDwarf2Cfi *self)
{
  dw_cfi_ref cfi = new_cfi ();
  cfi->dw_cfi_opc = DW_CFA_AARCH64_negate_ra_state;
  self->cur_row->ra_state
    = (self->cur_row->ra_state == ra_no_signing
      ? ra_signing_sp
      : ra_no_signing);
  add_cfi (self,cfi);
}


/* Record call frame debugging information for an expression EXPR,
   which either sets SP or FP (adjusting how we calculate the frame
   address) or saves a register to the stack or another register.
   LABEL indicates the address of EXPR.

   This function encodes a state machine mapping rtxes to actions on
   cfa, cfa_store, and cfa_temp.reg.  We describe these rules so
   users need not read the source code.

  The High-Level Picture

  Changes in the register we use to calculate the CFA: Currently we
  assume that if you copy the CFA register into another register, we
  should take the other one as the new CFA register; this seems to
  work pretty well.  If it's wrong for some target, it's simple
  enough not to set RTX_FRAME_RELATED_P on the insn in question.

  Changes in the register we use for saving registers to the stack:
  This is usually SP, but not always.  Again, we deduce that if you
  copy SP into another register (and SP is not the CFA register),
  then the new register is the one we will be using for register
  saves.  This also seems to work.

  Register saves: There's not much guesswork about this one; if
  RTX_FRAME_RELATED_P is set on an insn which modifies memory, it's a
  register save, and the register used to calculate the destination
  had better be the one we think we're using for this purpose.
  It's also assumed that a copy from a call-saved register to another
  register is saving that register if RTX_FRAME_RELATED_P is set on
  that instruction.  If the copy is from a call-saved register to
  the *same* register, that means that the register is now the same
  value as in the caller.

  Except: If the register being saved is the CFA register, and the
  offset is nonzero, we are saving the CFA, so we assume we have to
  use DW_CFA_def_cfa_expression.  If the offset is 0, we assume that
  the intent is to save the value of SP from the previous frame.

  In addition, if a register has previously been saved to a different
  register,

  Invariants / Summaries of Rules

  cfa        current rule for calculating the CFA.  It usually
          consists of a register and an offset.  This is
          actually stored in *self->cur_cfa, but abbreviated
          for the purposes of this documentation.
  cfa_store    register used by prologue code to save things to the stack
          cfa_store.offset is the offset from the value of
          cfa_store.reg to the actual CFA
  cfa_temp     register holding an integral value.  cfa_temp.offset
          stores the value, which will be used to adjust the
          stack pointer.  cfa_temp is also used like cfa_store,
          to track stores to the stack via fp or a temp reg.

  Rules  1- 4: Setting a register's value to cfa.reg or an expression
          with cfa.reg as the first operand changes the cfa.reg and its
          cfa.offset.  Rule 1 and 4 also set cfa_temp.reg and
          cfa_temp.offset.

  Rules  6- 9: Set a non-cfa.reg register value to a constant or an
          expression yielding a constant.  This sets cfa_temp.reg
          and cfa_temp.offset.

  Rule 5:      Create a new register cfa_store used to save items to the
          stack.

  Rules 10-14: Save a register to the stack.  Define offset as the
          difference of the original location and cfa_store's
          location (or cfa_temp's location if cfa_temp is used).

  Rules 16-20: If AND operation happens on sp in prologue, we assume
          stack is realigned.  We will use a group of DW_OP_XXX
          expressions to represent the location of the stored
          register instead of CFA+offset.

  The Rules

  "{a,b}" indicates a choice of a xor b.
  "<reg>:cfa.reg" indicates that <reg> must equal cfa.reg.

  Rule 1:
  (set <reg1> <reg2>:cfa.reg)
  effects: cfa.reg = <reg1>
      cfa.offset unchanged
      cfa_temp.reg = <reg1>
      cfa_temp.offset = cfa.offset

  Rule 2:
  (set sp ({minus,plus,losum} {sp,fp}:cfa.reg
               {<const_int>,<reg>:cfa_temp.reg}))
  effects: cfa.reg = sp if fp used
      cfa.offset += {+/- <const_int>, cfa_temp.offset} if cfa.reg==sp
      cfa_store.offset += {+/- <const_int>, cfa_temp.offset}
        if cfa_store.reg==sp

  Rule 3:
  (set fp ({minus,plus,losum} <reg>:cfa.reg <const_int>))
  effects: cfa.reg = fp
      cfa_offset += +/- <const_int>

  Rule 4:
  (set <reg1> ({plus,losum} <reg2>:cfa.reg <const_int>))
  constraints: <reg1> != fp
          <reg1> != sp
  effects: cfa.reg = <reg1>
      cfa_temp.reg = <reg1>
      cfa_temp.offset = cfa.offset

  Rule 5:
  (set <reg1> (plus <reg2>:cfa_temp.reg sp:cfa.reg))
  constraints: <reg1> != fp
          <reg1> != sp
  effects: cfa_store.reg = <reg1>
      cfa_store.offset = cfa.offset - cfa_temp.offset

  Rule 6:
  (set <reg> <const_int>)
  effects: cfa_temp.reg = <reg>
      cfa_temp.offset = <const_int>

  Rule 7:
  (set <reg1>:cfa_temp.reg (ior <reg2>:cfa_temp.reg <const_int>))
  effects: cfa_temp.reg = <reg1>
      cfa_temp.offset |= <const_int>

  Rule 8:
  (set <reg> (high <exp>))
  effects: none

  Rule 9:
  (set <reg> (lo_sum <exp> <const_int>))
  effects: cfa_temp.reg = <reg>
      cfa_temp.offset = <const_int>

  Rule 10:
  (set (mem ({pre,post}_modify sp:cfa_store (???? <reg1> <const_int>))) <reg2>)
  effects: cfa_store.offset -= <const_int>
      cfa.offset = cfa_store.offset if cfa.reg == sp
      cfa.reg = sp
      cfa.base_offset = -cfa_store.offset

  Rule 11:
  (set (mem ({pre_inc,pre_dec,post_dec} sp:cfa_store.reg)) <reg>)
  effects: cfa_store.offset += -/+ mode_size(mem)
      cfa.offset = cfa_store.offset if cfa.reg == sp
      cfa.reg = sp
      cfa.base_offset = -cfa_store.offset

  Rule 12:
  (set (mem ({minus,plus,losum} <reg1>:{cfa_store,cfa_temp} <const_int>))

       <reg2>)
  effects: cfa.reg = <reg1>
      cfa.base_offset = -/+ <const_int> - {cfa_store,cfa_temp}.offset

  Rule 13:
  (set (mem <reg1>:{cfa_store,cfa_temp}) <reg2>)
  effects: cfa.reg = <reg1>
      cfa.base_offset = -{cfa_store,cfa_temp}.offset

  Rule 14:
  (set (mem (post_inc <reg1>:cfa_temp <const_int>)) <reg2>)
  effects: cfa.reg = <reg1>
      cfa.base_offset = -cfa_temp.offset
      cfa_temp.offset -= mode_size(mem)

  Rule 15:
  (set <reg> {unspec, unspec_volatile})
  effects: target-dependent

  Rule 16:
  (set sp (and: sp <const_int>))
  constraints: cfa_store.reg == sp
  effects: cfun->fde.stack_realign = 1
           cfa_store.offset = 0
      fde->drap_reg = cfa.reg if cfa.reg != sp and cfa.reg != fp

  Rule 17:
  (set (mem ({pre_inc, pre_dec} sp)) (mem (plus (cfa.reg) (const_int))))
  effects: cfa_store.offset += -/+ mode_size(mem)

  Rule 18:
  (set (mem ({pre_inc, pre_dec} sp)) fp)
  constraints: fde->stack_realign == 1
  effects: cfa_store.offset = 0
      cfa.reg != HARD_FRAME_POINTER_REGNUM

  Rule 19:
  (set (mem ({pre_inc, pre_dec} sp)) cfa.reg)
  constraints: fde->stack_realign == 1
               && cfa.offset == 0
               && cfa.indirect == 0
               && cfa.reg != HARD_FRAME_POINTER_REGNUM
  effects: Use DW_CFA_def_cfa_expression to define cfa
      cfa.reg == fde->drap_reg  */
static void dwarf2out_frame_debug_expr (MtcsDwarf2Cfi *self,rtx expr)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   rtx src, dest, span;
   poly_int64 offset;
   dw_fde_ref fde;

   /* If RTX_FRAME_RELATED_P is set on a PARALLEL, process each member of
   the PARALLEL independently. The first element is always processed if
   it is a SET. This is for backward compatibility.   Other elements
   are processed only if they are SETs and the RTX_FRAME_RELATED_P
   flag is set in them.  */
   if (GET_CODE (expr) == PARALLEL || GET_CODE (expr) == SEQUENCE){
      int par_index;
      int limit = XVECLEN (expr, 0);
      rtx elem;

      /* PARALLELs have strict read-modify-write semantics, so we
      ought to evaluate every rvalue before changing any lvalue.
      It's cumbersome to do that in general, but there's an
      easy approximation that is enough for all current users:
      handle register saves before register assignments.  */
      if (GET_CODE (expr) == PARALLEL)
         for (par_index = 0; par_index < limit; par_index++){
            elem = XVECEXP (expr, 0, par_index);
            if (GET_CODE (elem) == SET && MEM_P (SET_DEST (elem)) && (RTX_FRAME_RELATED_P (elem) || par_index == 0))
               dwarf2out_frame_debug_expr(self,elem);
         }

      for (par_index = 0; par_index < limit; par_index++){
         elem = XVECEXP (expr, 0, par_index);
         if (GET_CODE (elem) == SET
         && (!MEM_P (SET_DEST (elem)) || GET_CODE (expr) == SEQUENCE)
         && (RTX_FRAME_RELATED_P (elem) || par_index == 0))
            dwarf2out_frame_debug_expr(self,elem);
      }
      return;
   }

   gcc_assert (GET_CODE (expr) == SET);

   src = SET_SRC (expr);
   dest = SET_DEST (expr);

   if (REG_P (src)){
      rtx rsi = reg_saved_in(self,src);
      if (rsi)
         src = rsi;
   }

   fde = cfun->fde;

   switch (GET_CODE (dest)){
      case REG:
         switch (GET_CODE (src)){
            /* Setting FP from SP.  */
            case REG:
               if (self->cur_cfa->reg == src){
                  /* Rule 1 */
                  /* Update the CFA rule wrt SP or FP.  Make sure src is
                  relative to the current CFA register.

                  We used to require that dest be either SP or FP, but the
                  ARM copies SP to a temporary register, and from there to
                  FP.  So we just rely on the backends to only set
                  RTX_FRAME_RELATED_P on appropriate insns.  */
                  self->cur_cfa->reg = dwf_cfa_reg(self,dest);
                  self->cur_trace->cfa_temp.reg = self->cur_cfa->reg;
                  self->cur_trace->cfa_temp.offset = self->cur_cfa->offset;
               }else{
                  /* Saving a register in a register.  */
                  gcc_assert (!mtcsReg->hardRegs.x_fixed_regs/*!fixed_regs*/ [REGNO (dest)]
                  /* For the SPARC and its register window.  */
                  || (dwf_regno(self,src) == mtcs_reg_get_dwarf_frame_return_column/*!DWARF_FRAME_RETURN_COLUMN*/(mtcsReg)));

                  /* After stack is aligned, we can only save SP in FP
                  if drap register is used.  In this case, we have
                  to restore stack pointer with the CFA value and we
                  don't generate this DWARF information.  */
                  if (fde && fde->stack_realign
                  && REGNO (src) == mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg)){
                     gcc_assert (REGNO (dest) == mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg)
                           && fde->drap_reg != INVALID_REGNUM && self->cur_cfa->reg != src  && fde->rule18);
                     fde->rule18 = 0;
                     /* The save of hard frame pointer has been deferred
                     until this point when Rule 18 applied.  Emit it now.  */
                     queue_reg_save(self,dest, NULL_RTX, 0);
                     /* And as the instruction modifies the hard frame pointer,
                     flush the queue as well.  */
                     dwarf2out_flush_queued_reg_saves(self);
                  }else
                     queue_reg_save(self,src, dest, 0);
               }
               break;

            case PLUS:
            case MINUS:
            case LO_SUM:
               if (dest == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)){
                  /* Rule 2 */
                  /* Adjusting SP.  */
                  if (REG_P (XEXP (src, 1))){
                     gcc_assert (self->cur_trace->cfa_temp.reg == XEXP (src, 1));
                     offset = self->cur_trace->cfa_temp.offset;
                  }else if (!poly_int_rtx_p (XEXP (src, 1), &offset))
                     gcc_unreachable ();

                  if (XEXP (src, 0) == mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL)){
                     /* Restoring SP from FP in the epilogue.  */
                     gcc_assert (self->cur_cfa->reg == self->dw_frame_pointer_regnum);
                     self->cur_cfa->reg = self->dw_stack_pointer_regnum;
                  }else if (GET_CODE (src) == LO_SUM)
                     /* Assume we've set the source reg of the LO_SUM from sp.  */
                     ;
                  else
                     gcc_assert (XEXP (src, 0) == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL));

                  if (GET_CODE (src) != MINUS)
                     offset = -offset;
                  if (self->cur_cfa->reg == self->dw_stack_pointer_regnum)
                     self->cur_cfa->offset += offset;
                  if (self->cur_trace->cfa_store.reg == self->dw_stack_pointer_regnum)
                     self->cur_trace->cfa_store.offset += offset;
               }else if (dest == mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL)){
                  /* Rule 3 */
                  /* Either setting the FP from an offset of the SP,
                  or adjusting the FP */
                  gcc_assert (mtcsRtlData->x_frame_pointer_needed/*!frame_pointer_needed*/);

                  gcc_assert (REG_P (XEXP (src, 0)) && self->cur_cfa->reg == XEXP (src, 0));
                  offset = rtx_to_poly_int64 (XEXP (src, 1));
                  if (GET_CODE (src) != MINUS)
                     offset = -offset;
                  self->cur_cfa->offset += offset;
                  self->cur_cfa->reg = self->dw_frame_pointer_regnum;
               }else{
                  gcc_assert (GET_CODE (src) != MINUS);

                  /* Rule 4 */
                  if (REG_P (XEXP (src, 0))  && self->cur_cfa->reg == XEXP (src, 0)
                  && poly_int_rtx_p (XEXP (src, 1), &offset)){
                     /* Setting a temporary CFA register that will be copied
                     into the FP later on.  */
                     offset = -offset;
                     self->cur_cfa->offset += offset;
                     self->cur_cfa->reg = dwf_cfa_reg(self,dest);
                     /* Or used to save regs to the stack.  */
                     self->cur_trace->cfa_temp.reg = self->cur_cfa->reg;
                     self->cur_trace->cfa_temp.offset = self->cur_cfa->offset;
                  }/* Rule 5 */else if (REG_P (XEXP (src, 0)) && self->cur_trace->cfa_temp.reg == XEXP (src, 0)
                  && XEXP (src, 1) == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)){
                     /* Setting a scratch register that we will use instead
                     of SP for saving registers to the stack.  */
                     gcc_assert (self->cur_cfa->reg == self->dw_stack_pointer_regnum);
                     self->cur_trace->cfa_store.reg = dwf_cfa_reg(self,dest);
                     self->cur_trace->cfa_store.offset = self->cur_cfa->offset - self->cur_trace->cfa_temp.offset;
                  }/* Rule 9 */else if (GET_CODE (src) == LO_SUM
                     && poly_int_rtx_p (XEXP (src, 1), &self->cur_trace->cfa_temp.offset))
                     self->cur_trace->cfa_temp.reg = dwf_cfa_reg(self,dest);
                  else
                     gcc_unreachable ();
               }
               break;

            /* Rule 6 */
            case CONST_INT:
            case CONST_POLY_INT:
               self->cur_trace->cfa_temp.reg = dwf_cfa_reg(self,dest);
               self->cur_trace->cfa_temp.offset = rtx_to_poly_int64 (src);
               break;

            /* Rule 7 */
            case IOR:
               gcc_assert (REG_P (XEXP (src, 0)) && self->cur_trace->cfa_temp.reg == XEXP (src, 0) && CONST_INT_P (XEXP (src, 1)));

               self->cur_trace->cfa_temp.reg = dwf_cfa_reg(self,dest);
               if (!can_ior_p (self->cur_trace->cfa_temp.offset, INTVAL (XEXP (src, 1)), &self->cur_trace->cfa_temp.offset))
                  /* The target shouldn't generate this kind of CFI note if we
                  can't represent it.  */
                  gcc_unreachable ();
               break;

            /* Skip over HIGH, assuming it will be followed by a LO_SUM,
            which will fill in all of the bits.  */
            /* Rule 8 */
            case HIGH:
               break;

            /* Rule 15 */
            case UNSPEC:
            case UNSPEC_VOLATILE:
               /* All unspecs should be represented by REG_CFA_* notes.  */
               gcc_unreachable ();
               return;

            /* Rule 16 */
            case AND:
               /* If this AND operation happens on stack pointer in prologue,
               we assume the stack is realigned and we extract the
               alignment.  */
               if (fde && XEXP (src, 0) == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)){
                  /* We interpret reg_save differently with stack_realign set.
                  Thus we must flush whatever we have queued first.  */
                  dwarf2out_flush_queued_reg_saves(self);

                  gcc_assert (self->cur_trace->cfa_store.reg  == XEXP (src, 0));
                  fde->stack_realign = 1;
                  fde->stack_realignment = INTVAL (XEXP (src, 1));
                  self->cur_trace->cfa_store.offset = 0;

                  if (self->cur_cfa->reg != self->dw_stack_pointer_regnum  && self->cur_cfa->reg != self->dw_frame_pointer_regnum){
                     gcc_assert (self->cur_cfa->reg.span == 1);
                     fde->drap_reg = self->cur_cfa->reg.reg;
                  }
               }
               return;

            default:
               gcc_unreachable ();
         }
         break;

      case MEM:

         /* Saving a register to the stack.  Make sure dest is relative to the
         CFA register.  */
         switch (GET_CODE (XEXP (dest, 0))){
            /* Rule 10 */
            /* With a push.  */
            case PRE_MODIFY:
            case POST_MODIFY:
               /* We can't handle variable size modifications.  */
               offset = -rtx_to_poly_int64 (XEXP (XEXP (XEXP (dest, 0), 1), 1));

               gcc_assert (REGNO (XEXP (XEXP (dest, 0), 0)) == mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg)
               && self->cur_trace->cfa_store.reg == self->dw_stack_pointer_regnum);

               self->cur_trace->cfa_store.offset += offset;
               if (self->cur_cfa->reg == self->dw_stack_pointer_regnum)
                  self->cur_cfa->offset = self->cur_trace->cfa_store.offset;

               if (GET_CODE (XEXP (dest, 0)) == POST_MODIFY)
                  offset -= self->cur_trace->cfa_store.offset;
               else
                  offset = -self->cur_trace->cfa_store.offset;
               break;

            /* Rule 11 */
            case PRE_INC:
            case PRE_DEC:
            case POST_DEC:
               offset = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (dest));
               if (GET_CODE (XEXP (dest, 0)) == PRE_INC)
                  offset = -offset;

               gcc_assert ((REGNO (XEXP (XEXP (dest, 0), 0)) == mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg))
                     && self->cur_trace->cfa_store.reg == self->dw_stack_pointer_regnum);

               self->cur_trace->cfa_store.offset += offset;

               /* Rule 18: If stack is aligned, we will use FP as a
               reference to represent the address of the stored
               regiser.  */
               if (fde   && fde->stack_realign  && REG_P (src)
               && REGNO (src) == mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg)){
                  gcc_assert (self->cur_cfa->reg != self->dw_frame_pointer_regnum);
                  self->cur_trace->cfa_store.offset = 0;
                  fde->rule18 = 1;
               }

               if (self->cur_cfa->reg == self->dw_stack_pointer_regnum)
                  self->cur_cfa->offset = self->cur_trace->cfa_store.offset;

               if (GET_CODE (XEXP (dest, 0)) == POST_DEC)
                  offset += -self->cur_trace->cfa_store.offset;
               else
                  offset = -self->cur_trace->cfa_store.offset;
               break;

            /* Rule 12 */
            /* With an offset.  */
            case PLUS:
            case MINUS:
            case LO_SUM:
            {
               struct cfa_reg regno;

               gcc_assert (REG_P (XEXP (XEXP (dest, 0), 0)));
               offset = rtx_to_poly_int64 (XEXP (XEXP (dest, 0), 1));
               if (GET_CODE (XEXP (dest, 0)) == MINUS)
                  offset = -offset;

               regno = dwf_cfa_reg(self,XEXP (XEXP (dest, 0), 0));

               if (self->cur_cfa->reg == regno)
                  offset -= self->cur_cfa->offset;
               else if (self->cur_trace->cfa_store.reg == regno)
                  offset -= self->cur_trace->cfa_store.offset;
               else{
                  gcc_assert (self->cur_trace->cfa_temp.reg == regno);
                  offset -= self->cur_trace->cfa_temp.offset;
               }
            }
               break;

            /* Rule 13 */
            /* Without an offset.  */
            case REG:
            {
               struct cfa_reg regno = dwf_cfa_reg(self,XEXP (dest, 0));

               if (self->cur_cfa->reg == regno)
                  offset = -self->cur_cfa->offset;
               else if (self->cur_trace->cfa_store.reg == regno)
                  offset = -self->cur_trace->cfa_store.offset;
               else{
                  gcc_assert (self->cur_trace->cfa_temp.reg == regno);
                  offset = -self->cur_trace->cfa_temp.offset;
               }
            }
            break;

            /* Rule 14 */
            case POST_INC:
               gcc_assert (self->cur_trace->cfa_temp.reg == XEXP (XEXP (dest, 0), 0));
               offset = -self->cur_trace->cfa_temp.offset;
               self->cur_trace->cfa_temp.offset -= mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (dest));
               break;

            default:
            gcc_unreachable ();
         }

         /* Rule 17 */
         /* If the source operand of this MEM operation is a memory,
         we only care how much stack grew.  */
         if (MEM_P (src))
            break;

         if (REG_P (src)
         && REGNO (src) != mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg)
         && REGNO (src) != mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg)
         && self->cur_cfa->reg == src){
            /* We're storing the current CFA reg into the stack.  */

            if (known_eq (self->cur_cfa->offset, 0)){
               /* Rule 19 */
               /* If stack is aligned, putting CFA reg into stack means
               we can no longer use reg + offset to represent CFA.
               Here we use DW_CFA_def_cfa_expression instead.  The
               result of this expression equals to the original CFA
               value.  */
               if (fde
               && fde->stack_realign
               && self->cur_cfa->indirect == 0
               && self->cur_cfa->reg != self->dw_frame_pointer_regnum){
                  gcc_assert (fde->drap_reg == self->cur_cfa->reg.reg);

                  self->cur_cfa->indirect = 1;
                  self->cur_cfa->reg = self->dw_frame_pointer_regnum;
                  self->cur_cfa->base_offset = offset;
                  self->cur_cfa->offset = 0;

                  fde->drap_reg_saved = 1;
                  break;
               }

               /* If the source register is exactly the CFA, assume
               we're saving SP like any other register; this happens
               on the ARM.  */
               queue_reg_save(self,mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL), NULL_RTX, offset);
               break;
            }else{
               /* Otherwise, we'll need to look in the stack to
               calculate the CFA.  */
               rtx x = XEXP (dest, 0);

               if (!REG_P (x))
                  x = XEXP (x, 0);
               gcc_assert (REG_P (x));

               self->cur_cfa->reg = dwf_cfa_reg(self,x);
               self->cur_cfa->base_offset = offset;
               self->cur_cfa->indirect = 1;
               break;
            }
         }

         if (REG_P (src))
            span = mtcsTarget/*!targetm.dwarf_register_span*/->dwarf_register_span(mtcsTarget,src);
         else
            span = NULL;

         if (!span){
            if (fde->rule18)
               /* Just verify the hard frame pointer save when doing dynamic
               realignment uses expected offset.  The actual queue_reg_save
               needs to be deferred until the instruction that sets
               hard frame pointer to stack pointer, see PR99334 for
               details.  */
               gcc_assert (known_eq (offset, 0));
            else
               queue_reg_save(self,src, NULL_RTX, offset);
         }else{
            /* We have a PARALLEL describing where the contents of SRC live.
            Queue register saves for each piece of the PARALLEL.  */
            poly_int64 span_offset = offset;

            gcc_assert (GET_CODE (span) == PARALLEL);

            const int par_len = XVECLEN (span, 0);
            for (int par_index = 0; par_index < par_len; par_index++){
               rtx elem = XVECEXP (span, 0, par_index);
               queue_reg_save(self,elem, NULL_RTX, span_offset);
               span_offset += mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (elem));
            }
         }
         break;

      default:
         gcc_unreachable ();
   }
}

/* Record call frame debugging information for INSN, which either sets
   SP or FP (adjusting how we calculate the frame address) or saves a
   register to the stack.  */
static void dwarf2out_frame_debug (MtcsDwarf2Cfi *self,rtx_insn *insn)
{
   rtx note, n, pat;
   bool handled_one = false;

   for (note = REG_NOTES (insn); note; note = XEXP (note, 1))
      switch (REG_NOTE_KIND (note)){
         case REG_FRAME_RELATED_EXPR:
            pat = XEXP (note, 0);
            goto do_frame_expr;

         case REG_CFA_DEF_CFA:
            dwarf2out_frame_debug_def_cfa(self,XEXP (note, 0));
            handled_one = true;
            break;

         case REG_CFA_ADJUST_CFA:
            n = XEXP (note, 0);
            if (n == NULL){
               n = PATTERN (insn);
               if (GET_CODE (n) == PARALLEL)
                  n = XVECEXP (n, 0, 0);
            }
            dwarf2out_frame_debug_adjust_cfa(self,n);
            handled_one = true;
            break;

         case REG_CFA_OFFSET:
            n = XEXP (note, 0);
            if (n == NULL)
               n = single_set (insn);
            dwarf2out_frame_debug_cfa_offset(self,n);
            handled_one = true;
            break;

         case REG_CFA_REGISTER:
            n = XEXP (note, 0);
            if (n == NULL){
               n = PATTERN (insn);
               if (GET_CODE (n) == PARALLEL)
                  n = XVECEXP (n, 0, 0);
            }
            dwarf2out_frame_debug_cfa_register(self,n);
            handled_one = true;
            break;

         case REG_CFA_EXPRESSION:
         case REG_CFA_VAL_EXPRESSION:
            n = XEXP (note, 0);
            if (n == NULL)
               n = single_set (insn);

            if (REG_NOTE_KIND (note) == REG_CFA_EXPRESSION)
               dwarf2out_frame_debug_cfa_expression(self,n);
            else
               dwarf2out_frame_debug_cfa_val_expression(self,n);

            handled_one = true;
            break;

         case REG_CFA_RESTORE:
         case REG_CFA_NO_RESTORE:
            n = XEXP (note, 0);
            if (n == NULL){
               n = PATTERN (insn);
               if (GET_CODE (n) == PARALLEL)
                  n = XVECEXP (n, 0, 0);
               n = XEXP (n, 0);
            }
            dwarf2out_frame_debug_cfa_restore(self,n, REG_NOTE_KIND (note) == REG_CFA_RESTORE);
            handled_one = true;
            break;

         case REG_CFA_SET_VDRAP:
            n = XEXP (note, 0);
            if (REG_P (n)){
               dw_fde_ref fde = cfun->fde;
               if (fde){
                  gcc_assert (fde->vdrap_reg == INVALID_REGNUM);
                  if (REG_P (n))
                     fde->vdrap_reg = dwf_regno(self,n);
               }
            }
            handled_one = true;
            break;
         //gcc15 重命名 REG_CFA_TOGGLE_RA_MANGLE to REG_CFA_NEGATE_RA_STATE
         /*!
         case REG_CFA_TOGGLE_RA_MANGLE:
            dwarf2out_frame_debug_cfa_toggle_ra_mangle(self);
            handled_one = true;
            break;
            */
         case REG_CFA_NEGATE_RA_STATE:
            dwarf2out_frame_debug_cfa_negate_ra_state(self);
               handled_one = true;
               break;

         case REG_CFA_WINDOW_SAVE:
            dwarf2out_frame_debug_cfa_window_save(self);
            handled_one = true;
            break;

         case REG_CFA_FLUSH_QUEUE:
            /* The actual flush happens elsewhere.  */
            handled_one = true;
            break;

         default:
            break;
      }

   if (!handled_one){
      pat = PATTERN (insn);
      do_frame_expr:
      dwarf2out_frame_debug_expr(self,pat);

      /* Check again.  A parallel can save and update the same register.
      We could probably check just once, here, but this is safer than
      removing the check at the start of the function.  */
      if (clobbers_queued_reg_save(self,pat))
         dwarf2out_flush_queued_reg_saves(self);
   }
}

/* Emit CFI info to change the state from OLD_ROW to NEW_ROW.  */
static void change_cfi_row (MtcsDwarf2Cfi *self,dw_cfi_row *old_row, dw_cfi_row *new_row)
{
   size_t i, n_old, n_new, n_max;
   dw_cfi_ref cfi;

   if (new_row->cfa_cfi && !cfi_equal_p(self,old_row->cfa_cfi, new_row->cfa_cfi))
      add_cfi(self,new_row->cfa_cfi);
   else{
      cfi = def_cfa_0(self,&old_row->cfa, &new_row->cfa);
      if (cfi)
         add_cfi(self,cfi);
   }

   n_old = vec_safe_length (old_row->reg_save);
   n_new = vec_safe_length (new_row->reg_save);
   n_max = MAX (n_old, n_new);

   for (i = 0; i < n_max; ++i){
      dw_cfi_ref r_old = NULL, r_new = NULL;

      if (i < n_old)
         r_old = (*old_row->reg_save)[i];
      if (i < n_new)
         r_new = (*new_row->reg_save)[i];

      if (r_old == r_new)
         ;
      else if (r_new == NULL)
         add_cfi_restore(self,i);
      else if (!cfi_equal_p(self,r_old, r_new))
         add_cfi(self,r_new);
   }

   if (!old_row->window_save && new_row->window_save){
      dw_cfi_ref cfi = new_cfi ();

      gcc_assert (!old_row->ra_state && !new_row->ra_state);
      cfi->dw_cfi_opc = DW_CFA_GNU_window_save;
      add_cfi(self,cfi);
   }

   if (old_row->ra_state != new_row->ra_state){
      dw_cfi_ref cfi = new_cfi ();

      gcc_assert (!old_row->window_save && !new_row->window_save);
      /* DW_CFA_GNU_window_save is reused for toggling RA mangle state.  */
      cfi->dw_cfi_opc = DW_CFA_GNU_window_save;
      add_cfi(self,cfi);
   }
}

/* Examine CFI and return true if a cfi label and set_loc is needed
   beforehand.  Even when generating CFI assembler instructions, we
   still have to add the cfi to the list so that lookup_cfa_1 works
   later on.  When -g2 and above we even need to force emitting of
   CFI labels and add to list a DW_CFA_set_loc for convert_cfa_to_fb_loc_list
   purposes.  If we're generating DWARF3 output we use DW_OP_call_frame_cfa
   and so don't use convert_cfa_to_fb_loc_list.  */
static bool cfi_label_required_p (MtcsDwarf2Cfi *self,dw_cfi_ref cfi)
{
   if (!mtcs_dwarf2_cfi_dwarf2out_do_cfi_asm/*!dwarf2out_do_cfi_asm*/(self))
      return true;

   if (dwarf_version == 2  && debug_info_level > DINFO_LEVEL_TERSE  && dwarf_debuginfo_p ()){
      switch (cfi->dw_cfi_opc){
         case DW_CFA_def_cfa_offset:
         case DW_CFA_def_cfa_offset_sf:
         case DW_CFA_def_cfa_register:
         case DW_CFA_def_cfa:
         case DW_CFA_def_cfa_sf:
         case DW_CFA_def_cfa_expression:
         case DW_CFA_restore_state:
            return true;
         default:
            return false;
      }
   }
   return false;
}

/* Walk the function, looking for NOTE_INSN_CFI notes.  Add the CFIs to the
   function's FDE, adding CFI labels and set_loc/advance_loc opcodes as
   necessary.  */
static void add_cfis_to_fde (MtcsDwarf2Cfi *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   dw_fde_ref fde = cfun->fde;
   rtx_insn *insn, *next;

   for (insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = next){
      next = NEXT_INSN (insn);

      if (NOTE_P (insn) && NOTE_KIND (insn) == NOTE_INSN_SWITCH_TEXT_SECTIONS)
         fde->dw_fde_switch_cfi_index = vec_safe_length (fde->dw_fde_cfi);

      if (NOTE_P (insn) && NOTE_KIND (insn) == NOTE_INSN_CFI){
         bool required = cfi_label_required_p(self,NOTE_CFI (insn));
         while (next)
         if (NOTE_P (next) && NOTE_KIND (next) == NOTE_INSN_CFI){
            required |= cfi_label_required_p(self,NOTE_CFI (next));
            next = NEXT_INSN (next);
         }else if (active_insn_p (next) || (NOTE_P (next) && (NOTE_KIND (next) == NOTE_INSN_SWITCH_TEXT_SECTIONS)))
            break;
         else
            next = NEXT_INSN (next);
         if (required){
            int num = self->dwarf2out_cfi_label_num;
            const char *label = dwarf2out_cfi_label(self);
            dw_cfi_ref xcfi;

            /* Set the location counter to the new label.  */
            xcfi = new_cfi ();
            xcfi->dw_cfi_opc = DW_CFA_advance_loc4;
            xcfi->dw_cfi_oprnd1.dw_cfi_addr = label;
            vec_safe_push (fde->dw_fde_cfi, xcfi);

            rtx_note *tmp = mtcs_emit_emit_note_before/*!emit_note_before*/(mtcsEmit,NOTE_INSN_CFI_LABEL, insn);
            NOTE_LABEL_NUMBER (tmp) = num;
         }

         do{
            if (NOTE_P (insn) && NOTE_KIND (insn) == NOTE_INSN_CFI)
               vec_safe_push (fde->dw_fde_cfi, NOTE_CFI (insn));
            insn = NEXT_INSN (insn);
         }while (insn != next);
      }
   }
}


/* If LABEL is the start of a trace, then initialize the state of that
   trace from CUR_TRACE and CUR_ROW.  */
static void maybe_record_trace_start (MtcsDwarf2Cfi *self,rtx_insn *start, rtx_insn *origin)
{
   dw_trace_info *ti;

   ti = get_trace_info(self,start);
   gcc_assert (ti != NULL);
   if (dump_file){
      fprintf (dump_file, "   saw edge from trace %u to %u (via %s %d)\n",
            self->cur_trace->id, ti->id,(origin ? rtx_name[(int) GET_CODE (origin)] : "fallthru"),(origin ? INSN_UID (origin) : 0));
   }

   poly_int64 args_size = self->cur_trace->end_true_args_size;
   if (ti->beg_row == NULL){
      /* This is the first time we've encountered this trace.  Propagate
      state across the edge and push the trace onto the work list.  */
      ti->beg_row = copy_cfi_row (self->cur_row);
      ti->beg_true_args_size = args_size;
      ti->cfa_store = self->cur_trace->cfa_store;
      ti->cfa_temp = self->cur_trace->cfa_temp;
      ti->regs_saved_in_regs = self->cur_trace->regs_saved_in_regs.copy ();
      self->trace_work_list.safe_push (ti);
      if (dump_file)
         fprintf (dump_file, "\tpush trace %u to worklist\n", ti->id);
   }else{

      /* We ought to have the same state incoming to a given trace no
      matter how we arrive at the trace.  Anything else means we've
      got some kind of optimization error.  */
#if CHECKING_P
      if (!cfi_row_equal_p(self,self->cur_row, ti->beg_row)){
         if (dump_file){
            fprintf (dump_file, "Inconsistent CFI state!\n");
            fprintf (dump_file, "SHOULD have:\n");
            dump_cfi_row(self,dump_file, ti->beg_row);
            fprintf (dump_file, "DO have:\n");
            dump_cfi_row(self,dump_file, self->cur_row);
         }

         gcc_unreachable ();
      }
#endif

      /* The args_size is allowed to conflict if it isn't actually used.  */
      if (maybe_ne (ti->beg_true_args_size, args_size))
         ti->args_size_undefined = true;
   }
}

/* Similarly, but handle the args_size and CFA reset across EH
   and non-local goto edges.  */
static void maybe_record_trace_start_abnormal (MtcsDwarf2Cfi *self,rtx_insn *start, rtx_insn *origin)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   poly_int64 save_args_size, delta;
   dw_cfa_location save_cfa;
   save_args_size = self->cur_trace->end_true_args_size;
   if (known_eq (save_args_size, 0)){
      maybe_record_trace_start(self,start, origin);
      return;
   }
   delta = -save_args_size;
   self->cur_trace->end_true_args_size = 0;
   save_cfa = self->cur_row->cfa;
   if (self->cur_row->cfa.reg == self->dw_stack_pointer_regnum){
      /* Convert a change in args_size (always a positive in the
      direction of stack growth) to a change in stack pointer.  */
      if (!mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc))
         delta = -delta;
      self->cur_row->cfa.offset += delta;
   }
   maybe_record_trace_start(self,start, origin);
   self->cur_trace->end_true_args_size = save_args_size;
   self->cur_row->cfa = save_cfa;
}

/* Propagate CUR_TRACE state to the destinations implied by INSN.  */
/* ??? Sadly, this is in large part a duplicate of make_edges.  */
static void create_trace_edges (MtcsDwarf2Cfi *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
    MtcsRtlData *mtcsRtlData = mtcs_func_get_rtl_data(mtcsFunc);

   rtx tmp;
   int i, n;

   if (JUMP_P (insn)){
      rtx_jump_table_data *table;

      if (find_reg_note (insn, REG_NON_LOCAL_GOTO, NULL_RTX))
         return;

      if (tablejump_p (insn, NULL, &table)){
         rtvec vec = table->get_labels ();

         n = GET_NUM_ELEM (vec);
         for (i = 0; i < n; ++i){
            rtx_insn *lab = as_a <rtx_insn *> (XEXP (RTVEC_ELT (vec, i), 0));
            maybe_record_trace_start(self,lab, insn);
         }

         /* Handle casesi dispatch insns.  */
         if ((tmp = tablejump_casesi_pattern (insn)) != NULL_RTX){
            rtx_insn * lab = label_ref_label (XEXP (SET_SRC (tmp), 2));
            maybe_record_trace_start(self,lab, insn);
         }
      }else if (computed_jump_p (insn)){
         rtx_insn *temp;
         unsigned int i;
         FOR_EACH_VEC_SAFE_ELT (forced_labels, i, temp)
            maybe_record_trace_start(self,temp, insn);
      }else if (returnjump_p (insn))
         ;
      else if ((tmp = extract_asm_operands (PATTERN (insn))) != NULL){
         n = ASM_OPERANDS_LABEL_LENGTH (tmp);
         for (i = 0; i < n; ++i){
            rtx_insn *lab =  as_a <rtx_insn *> (XEXP (ASM_OPERANDS_LABEL (tmp, i), 0));
            maybe_record_trace_start(self,lab, insn);
         }
      }else{
         rtx_insn *lab = JUMP_LABEL_AS_INSN (insn);
         gcc_assert (lab != NULL);
         maybe_record_trace_start(self,lab, insn);
      }
   }else if (CALL_P (insn)){
      /* Sibling calls don't have edges inside this function.  */
      if (SIBLING_CALL_P (insn))
         return;

      /* Process non-local goto edges.  */
      if (can_nonlocal_goto (insn))
         for (rtx_insn_list *lab = mtcsRtlData/*!nonlocal_goto_handler_labels*/->x_nonlocal_goto_handler_labels; lab; lab = lab->next ())
            maybe_record_trace_start_abnormal(self,lab->insn (), insn);
   }else if (rtx_sequence *seq = dyn_cast <rtx_sequence *> (PATTERN (insn))){
      int i, n = seq->len ();
      for (i = 0; i < n; ++i)
         create_trace_edges(self,seq->insn (i));
      return;
   }

   /* Process EH edges.  */
   if (CALL_P (insn) || cfun->can_throw_non_call_exceptions){
      eh_landing_pad lp = get_eh_landing_pad_from_rtx (insn);
      if (lp)
         maybe_record_trace_start_abnormal(self,lp->landing_pad, insn);
   }
}

/* A subroutine of scan_trace.  Do what needs to be done "after" INSN.  */
static void scan_insn_after (MtcsDwarf2Cfi *self,rtx_insn *insn)
{
   if (RTX_FRAME_RELATED_P (insn))
      dwarf2out_frame_debug(self,insn);
   notice_args_size(self,insn);
}

/* Scan the trace beginning at INSN and create the CFI notes for the
   instructions therein.  */
static void scan_trace (MtcsDwarf2Cfi *self,dw_trace_info *trace, bool entry)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   rtx_insn *prev, *insn = trace->head;
   dw_cfa_location this_cfa;

   if (dump_file)
   fprintf (dump_file, "Processing trace %u : start at %s %d\n",trace->id, rtx_name[(int) GET_CODE (insn)],INSN_UID (insn));

   trace->end_row = copy_cfi_row (trace->beg_row);
   trace->end_true_args_size = trace->beg_true_args_size;

   self->cur_trace = trace;
   self->cur_row = trace->end_row;

   this_cfa = self->cur_row->cfa;
   self->cur_cfa = &this_cfa;

   /* If the current function starts with a non-standard incoming frame
   sp offset, emit a note before the first instruction.  */
   if (entry
   && mtcs_func_get_default_incoming_frame_sp_offset/*!DEFAULT_INCOMING_FRAME_SP_OFFSET*/(mtcsFunc)
   != mtcs_func_get_incoming_frame_sp_offset/*!INCOMING_FRAME_SP_OFFSET*/(mtcsFunc)){
      self->add_cfi_insn = insn;
      gcc_assert (NOTE_P (insn) && NOTE_KIND (insn) == NOTE_INSN_DELETED);
      this_cfa.offset = mtcs_func_get_incoming_frame_sp_offset/*!INCOMING_FRAME_SP_OFFSET*/(mtcsFunc);
      def_cfa_1(self,&this_cfa);
   }

   for (prev = insn, insn = NEXT_INSN (insn); insn; prev = insn, insn = NEXT_INSN (insn)){
      rtx_insn *control;

      /* Do everything that happens "before" the insn.  */
      self->add_cfi_insn = prev;

      /* Notice the end of a trace.  */
      if (BARRIER_P (insn)){
         /* Don't bother saving the unneeded queued registers at all.  */
         self->queued_reg_saves.truncate (0);
         break;
      }
      if (save_point_p (insn)){
         /* Propagate across fallthru edges.  */
         dwarf2out_flush_queued_reg_saves(self);
         maybe_record_trace_start(self,insn, NULL);
         break;
      }

      if (DEBUG_INSN_P (insn) || !inside_basic_block_p (insn))
         continue;

      /* Handle all changes to the row state.  Sequences require special
      handling for the positioning of the notes.  */
      if (rtx_sequence *pat = dyn_cast <rtx_sequence *> (PATTERN (insn))){
         rtx_insn *elt;
         int i, n = pat->len ();

         control = pat->insn (0);
         if (can_throw_internal (control))
            notice_eh_throw(self,control);
         dwarf2out_flush_queued_reg_saves(self);

         if (JUMP_P (control) && INSN_ANNULLED_BRANCH_P (control)){
            /* ??? Hopefully multiple delay slots are not annulled.  */
            gcc_assert (n == 2);
            gcc_assert (!RTX_FRAME_RELATED_P (control));
            gcc_assert (!find_reg_note (control, REG_ARGS_SIZE, NULL));

            elt = pat->insn (1);

            if (INSN_FROM_TARGET_P (elt)){
               cfi_vec save_row_reg_save;

               /* If ELT is an instruction from target of an annulled
               branch, the effects are for the target only and so
               the args_size and CFA along the current path
               shouldn't change.  */
               self->add_cfi_insn = NULL;
               poly_int64 restore_args_size = self->cur_trace->end_true_args_size;
               self->cur_cfa = &self->cur_row->cfa;
               save_row_reg_save = vec_safe_copy (self->cur_row->reg_save);
               scan_insn_after(self,elt);
               /* ??? Should we instead save the entire row state?  */
               gcc_assert (!self->queued_reg_saves.length ());
               create_trace_edges(self,control);

               self->cur_trace->end_true_args_size = restore_args_size;
               self->cur_row->cfa = this_cfa;
               self->cur_row->reg_save = save_row_reg_save;
               self->cur_cfa = &this_cfa;
            }else{
               /* If ELT is a annulled branch-taken instruction (i.e.
               executed only when branch is not taken), the args_size
               and CFA should not change through the jump.  */
               create_trace_edges(self,control);

               /* Update and continue with the trace.  */
               self->add_cfi_insn = insn;
               scan_insn_after(self,elt);
               def_cfa_1(self,&this_cfa);
            }
            continue;
         }

         /* The insns in the delay slot should all be considered to happen
         "before" a call insn.  Consider a call with a stack pointer
         adjustment in the delay slot.  The backtrace from the callee
         should include the sp adjustment.  Unfortunately, that leaves
         us with an unavoidable unwinding error exactly at the call insn
         itself.  For jump insns we'd prefer to avoid this error by
         placing the notes after the sequence.  */
         if (JUMP_P (control))
            self->add_cfi_insn = insn;

         for (i = 1; i < n; ++i){
            elt = pat->insn (i);
            scan_insn_after(self,elt);
         }

         /* Make sure any register saves are visible at the jump target.  */
         dwarf2out_flush_queued_reg_saves(self);
         self->any_cfis_emitted = false;

         /* However, if there is some adjustment on the call itself, e.g.
         a call_pop, that action should be considered to happen after
         the call returns.  */
         self->add_cfi_insn = insn;
         scan_insn_after(self,control);
      }else{
         /* Flush data before calls and jumps, and of course if necessary.  */
         if (can_throw_internal (insn)){
            notice_eh_throw(self,insn);
            dwarf2out_flush_queued_reg_saves(self);
         } else if (!NONJUMP_INSN_P (insn)  || clobbers_queued_reg_save(self,insn) || find_reg_note (insn, REG_CFA_FLUSH_QUEUE, NULL))
            dwarf2out_flush_queued_reg_saves(self);
         self->any_cfis_emitted = false;

         self->add_cfi_insn = insn;
         scan_insn_after(self,insn);
         control = insn;
      }

      /* Between frame-related-p and args_size we might have otherwise
      emitted two cfa adjustments.  Do it now.  */
      def_cfa_1(self,&this_cfa);

      /* Minimize the number of advances by emitting the entire queue
      once anything is emitted.  */
      if (self->any_cfis_emitted  || find_reg_note (insn, REG_CFA_FLUSH_QUEUE, NULL))
         dwarf2out_flush_queued_reg_saves(self);

      /* Note that a test for control_flow_insn_p does exactly the
      same tests as are done to actually create the edges.  So
      always call the routine and let it not create edges for
      non-control-flow insns.  */
      create_trace_edges(self,control);
   }

   gcc_assert (!cfun->fde || !cfun->fde->rule18);
   self->add_cfi_insn = NULL;
   self->cur_row = NULL;
   self->cur_trace = NULL;
   self->cur_cfa = NULL;
}

/* Scan the function and create the initial set of CFI notes.  */
static void create_cfi_notes (MtcsDwarf2Cfi *self)
{
   dw_trace_info *ti;

   gcc_checking_assert (!self->queued_reg_saves.exists ());
   gcc_checking_assert (!self->trace_work_list.exists ());

   /* Always begin at the entry trace.  */
   ti = &self->trace_info[0];
   scan_trace(self,ti, true);

   while (!self->trace_work_list.is_empty ()){
      ti = self->trace_work_list.pop ();
      scan_trace(self,ti, false);
   }

   self->queued_reg_saves.release ();
   self->trace_work_list.release ();
}

/* Return the insn before the first NOTE_INSN_CFI after START.  */
static rtx_insn *before_next_cfi_note (rtx_insn *start)
{
   rtx_insn *prev = start;
   while (start){
      if (NOTE_P (start) && NOTE_KIND (start) == NOTE_INSN_CFI)
         return prev;
      prev = start;
      start = NEXT_INSN (start);
   }
   gcc_unreachable ();
}

/* Insert CFI notes between traces to properly change state between them.  */
static void connect_traces (MtcsDwarf2Cfi *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   unsigned i, n;
   dw_trace_info *prev_ti, *ti;

   /* ??? Ideally, we should have both queued and processed every trace.
   However the current representation of constant pools on various targets
   is indistinguishable from unreachable code.  Assume for the moment that
   we can simply skip over such traces.  */
   /* ??? Consider creating a DATA_INSN rtx code to indicate that
   these are not "real" instructions, and should not be considered.
   This could be generically useful for tablejump data as well.  */
   /* Remove all unprocessed traces from the list.  */
   unsigned ix, ix2;
   VEC_ORDERED_REMOVE_IF_FROM_TO (self->trace_info, ix, ix2, ti, 1, self->trace_info.length (), ti->beg_row == NULL);
   FOR_EACH_VEC_ELT (self->trace_info, ix, ti)
      gcc_assert (ti->end_row != NULL);

   /* Work from the end back to the beginning.  This lets us easily insert
   remember/restore_state notes in the correct order wrt other notes.  */
   n = self->trace_info.length ();
   prev_ti = &self->trace_info[n - 1];
   for (i = n - 1; i > 0; --i){
      dw_cfi_row *old_row;

      ti = prev_ti;
      prev_ti = &self->trace_info[i - 1];

      self->add_cfi_insn = ti->head;

      /* In dwarf2out_switch_text_section, we'll begin a new FDE
      for the portion of the function in the alternate text
      section.  The row state at the very beginning of that
      new FDE will be exactly the row state from the CIE.  */
      if (ti->switch_sections)
         old_row = self->cie_cfi_row;
      else{
         old_row = prev_ti->end_row;
         /* If there's no change from the previous end state, fine.  */
         if (cfi_row_equal_p(self,old_row, ti->beg_row))
            ;
         /* Otherwise check for the common case of sharing state with
         the beginning of an epilogue, but not the end.  Insert
         remember/restore opcodes in that case.  */
         else if (cfi_row_equal_p(self,prev_ti->beg_row, ti->beg_row)){
            dw_cfi_ref cfi;

            /* Note that if we blindly insert the remember at the
            start of the trace, we can wind up increasing the
            size of the unwind info due to extra advance opcodes.
            Instead, put the remember immediately before the next
            state change.  We know there must be one, because the
            state at the beginning and head of the trace differ.  */
            self->add_cfi_insn = before_next_cfi_note (prev_ti->head);
            cfi = new_cfi ();
            cfi->dw_cfi_opc = DW_CFA_remember_state;
            add_cfi(self,cfi);

            self->add_cfi_insn = ti->head;
            cfi = new_cfi ();
            cfi->dw_cfi_opc = DW_CFA_restore_state;
            add_cfi(self,cfi);

            /* If the target unwinder does not save the CFA as part of the
            register state, we need to restore it separately.  */
            if (target_asm_out_should_restore_cfa_state/*!targetm.asm_out.should_restore_cfa_state*/(mtcsMachine->asmOut)
                  && (cfi = def_cfa_0(self,&old_row->cfa, &ti->beg_row->cfa)))
               add_cfi(self,cfi);

            old_row = prev_ti->beg_row;
         }
      /* Otherwise, we'll simply change state from the previous end.  */
      }

      change_cfi_row(self,old_row, ti->beg_row);
      if (dump_file && self->add_cfi_insn != ti->head){
         rtx_insn *note;
         fprintf (dump_file, "Fixup between trace %u and %u:\n",prev_ti->id, ti->id);
         note = ti->head;
         do{
            note = NEXT_INSN (note);
            gcc_assert (NOTE_P (note) && NOTE_KIND (note) == NOTE_INSN_CFI);
            mtcs_dwarf2_cfi_output_cfi_directive/*!output_cfi_directive*/(self,dump_file, NOTE_CFI (note));
         }while (note != self->add_cfi_insn);
      }
   }

   /* Connect args_size between traces that have can_throw_internal insns.  */
   if (cfun->eh->lp_array){
      poly_int64 prev_args_size = 0;

      for (i = 0; i < n; ++i){
         ti = &self->trace_info[i];

         if (ti->switch_sections)
            prev_args_size = 0;

         if (ti->eh_head == NULL)
            continue;
         /* We require either the incoming args_size values to match or the
         presence of an insn setting it before the first EH insn.  */
         gcc_assert (!ti->args_size_undefined || ti->args_size_defined_for_eh);
         /* In the latter case, we force the creation of a CFI note.  */
         if (ti->args_size_undefined || maybe_ne (ti->beg_delay_args_size, prev_args_size)){
            /* ??? Search back to previous CFI note.  */
            self->add_cfi_insn = PREV_INSN (ti->eh_head);
            add_cfi_args_size(self,ti->beg_delay_args_size);
         }
         prev_args_size = ti->end_delay_args_size;
      }
   }
}

/* Set up the pseudo-cfg of instruction traces, as described at the
   block comment at the top of the file.  */
static void create_pseudo_cfg (MtcsDwarf2Cfi *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   bool saw_barrier, switch_sections;
   dw_trace_info ti;
   rtx_insn *insn;
   unsigned i;

   /* The first trace begins at the start of the function,
   and begins with the CIE row state.  */
   self->trace_info.create (16);
   memset (&ti, 0, sizeof (ti));
   ti.head = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   ti.beg_row = self->cie_cfi_row;
   ti.cfa_store = self->cie_cfi_row->cfa;
   ti.cfa_temp.reg.set_by_dwreg (INVALID_REGNUM);
   self->trace_info.quick_push (ti);

   if (self->cie_return_save)
      ti.regs_saved_in_regs.safe_push (*self->cie_return_save);

   /* Walk all the insns, collecting start of trace locations.  */
   saw_barrier = false;
   switch_sections = false;
   for (insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = NEXT_INSN (insn)){
      if (BARRIER_P (insn))
         saw_barrier = true;
      else if (NOTE_P (insn)   && NOTE_KIND (insn) == NOTE_INSN_SWITCH_TEXT_SECTIONS){
         /* We should have just seen a barrier.  */
         gcc_assert (saw_barrier);
         switch_sections = true;
      }
      /* Watch out for save_point notes between basic blocks.
      In particular, a note after a barrier.  Do not record these,
      delaying trace creation until the label.  */
      else if (save_point_p (insn)  && (LABEL_P (insn) || !saw_barrier)){
         memset (&ti, 0, sizeof (ti));
         ti.head = insn;
         ti.switch_sections = switch_sections;
         ti.id = self->trace_info.length ();
         self->trace_info.safe_push (ti);

         saw_barrier = false;
         switch_sections = false;
      }
   }

   /* Create the trace index after we've finished building self->trace_info,
   avoiding stale pointer problems due to reallocation.  */
   self->trace_index = new hash_table<trace_info_hasher> (self->trace_info.length ());
   dw_trace_info *tp;
   FOR_EACH_VEC_ELT (self->trace_info, i, tp){
      dw_trace_info **slot;

      if (dump_file)
         fprintf (dump_file, "Creating trace %u : start at %s %d%s\n", tp->id,
               rtx_name[(int) GET_CODE (tp->head)], INSN_UID (tp->head), tp->switch_sections ? " (section switch)" : "");

      slot = self->trace_index->find_slot_with_hash (tp, INSN_UID (tp->head), INSERT);
      gcc_assert (*slot == NULL);
      *slot = tp;
   }
}

/* Record the initial position of the return address.  RTL is
   INCOMING_RETURN_ADDR_RTX.  */
static void initial_return_save (MtcsDwarf2Cfi *self,rtx rtl)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   struct cfa_reg reg;
   reg.set_by_dwreg (INVALID_REGNUM);
   poly_int64 offset = 0;

   switch (GET_CODE (rtl)){
      case REG:
         /* RA is in a register.  */
         reg = dwf_cfa_reg(self,rtl);
         break;

      case MEM:
         /* RA is on the stack.  */
         rtl = XEXP (rtl, 0);
         switch (GET_CODE (rtl)){
            case REG:
               gcc_assert (REGNO (rtl) == mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg));
               offset = 0;
               break;

            case PLUS:
               gcc_assert (REGNO (XEXP (rtl, 0)) == mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg));
               offset = rtx_to_poly_int64 (XEXP (rtl, 1));
               break;

            case MINUS:
               gcc_assert (REGNO (XEXP (rtl, 0)) == mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg));
               offset = -rtx_to_poly_int64 (XEXP (rtl, 1));
               break;

            default:
               gcc_unreachable ();
         }

         break;

      case PLUS:
         /* The return address is at some offset from any value we can
         actually load.  For instance, on the SPARC it is in %i7+8. Just
         ignore the offset for now; it doesn't matter for unwinding frames.  */
         gcc_assert (CONST_INT_P (XEXP (rtl, 1)));
         initial_return_save(self,XEXP (rtl, 0));
         return;

      default:
         gcc_unreachable ();
   }

   if (reg.reg != mtcs_reg_get_dwarf_frame_return_column/*!DWARF_FRAME_RETURN_COLUMN*/(mtcsReg)){
      if (reg.reg != INVALID_REGNUM)
         record_reg_saved_in_reg(self,rtl, pc_rtx);
      reg_save(self,mtcs_reg_get_dwarf_frame_return_column/*!DWARF_FRAME_RETURN_COLUMN*/(mtcsReg),
            reg, offset - self->cur_row->cfa.offset);
   }
}

static void create_cie_data (MtcsDwarf2Cfi *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   dw_cfa_location loc;
   dw_trace_info cie_trace;

   self->dw_stack_pointer_regnum = dwf_cfa_reg(self,mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL));

   memset (&cie_trace, 0, sizeof (cie_trace));
   self->cur_trace = &cie_trace;

   self->add_cfi_vec = &cie_cfi_vec;
   self->cie_cfi_row = self->cur_row = new_cfi_row ();

   /* On entry, the Canonical Frame Address is at SP.  */
   memset (&loc, 0, sizeof (loc));
   loc.reg = self->dw_stack_pointer_regnum;
   /* create_cie_data is called just once per TU, and when using .cfi_startproc
   is even done by the assembler rather than the compiler.  If the target
   has different incoming frame sp offsets depending on what kind of
   function it is, use a single constant offset for the target and
   if needed, adjust before the first instruction in insn stream.  */
   loc.offset = mtcs_func_get_default_incoming_frame_sp_offset/*!DEFAULT_INCOMING_FRAME_SP_OFFSET*/(mtcsFunc);
   def_cfa_1(self,&loc);

   if (mtcsTarget/*!targetm.debug_unwind_info*/->debug_unwind_info(mtcsTarget) == UI_DWARF2
   ||target_common_except_unwind_info/*!targetm_common.except_unwind_info*/(mtcsMachine->common,mtcsOptionsItem) == UI_DWARF2){
      initial_return_save(self,mtcs_rtl_incoming_return_addr_rtx/*!INCOMING_RETURN_ADDR_RTX*/(mtcsRTL));

      /* For a few targets, we have the return address incoming into a
      register, but choose a different return column.  This will result
      in a DW_CFA_register for the return, and an entry in
      regs_saved_in_regs to match.  If the target later stores that
      return address register to the stack, we want to be able to emit
      the DW_CFA_offset against the return column, not the intermediate
      save register.  Save the contents of regs_saved_in_regs so that
      we can re-initialize it at the start of each function.  */
      switch (cie_trace.regs_saved_in_regs.length ()){
         case 0:
            break;
         case 1:
            self->cie_return_save = ggc_alloc<reg_saved_in_data> ();
            *self->cie_return_save = cie_trace.regs_saved_in_regs[0];
            cie_trace.regs_saved_in_regs.release ();
            break;
         default:
            gcc_unreachable ();
      }
   }

   self->add_cfi_vec = NULL;
   self->cur_row = NULL;
   self->cur_trace = NULL;
}

/* Annotate the function with NOTE_INSN_CFI notes to record the CFI
   state at each location within the function.  These notes will be
   emitted during pass_final.  */
static void execute_dwarf2_frame (MtcsDwarf2Cfi *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsDwarf2Out   *mtcsDwarf2Out=mtcs_target_get_dwarf2_out(mtcsTarget);

   /* Different HARD_FRAME_POINTER_REGNUM might coexist in the same file.  */
   self->dw_frame_pointer_regnum = dwf_cfa_reg(self,mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL));

   /* The first time we're called, compute the incoming frame state.  */
   if (cie_cfi_vec == NULL)
      create_cie_data(self);

   mtcs_dwarf2_out_dwarf2out_alloc_current_fde/*!dwarf2out_alloc_current_fde*/(mtcsDwarf2Out);
   create_pseudo_cfg(self);
   /* Do the work.  */
   create_cfi_notes(self);
   connect_traces(self);
   add_cfis_to_fde(self);

   /* Free all the data we allocated.  */
   {
      size_t i;
      dw_trace_info *ti;

      FOR_EACH_VEC_ELT (self->trace_info, i, ti)
         ti->regs_saved_in_regs.release ();
   }
   self->trace_info.release ();
   delete self->trace_index;
   self->trace_index = NULL;
}

/* Convert a DWARF call frame info. operation to its string name */

static const char *dwarf_cfi_name (unsigned int cfi_opc)
{
   const char *name = get_DW_CFA_name (cfi_opc);
   if (name != NULL)
      return name;
   return "DW_CFA_<unknown>";
}

/* This routine will generate the correct assembly data for a location
   description based on a cfi entry with a complex address.  */
static void output_cfa_loc (MtcsDwarf2Cfi *self,dw_cfi_ref cfi, int for_eh)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsDwarf2Asm *mtcsDwarf2Asm=mtcs_target_get_dwarf2_asm(mtcsTarget);
   MtcsDwarf2Out *mtcsDwarf2Out=mtcs_target_get_dwarf2_out(mtcsTarget);

   dw_loc_descr_ref loc;
   unsigned long size;

   if (cfi->dw_cfi_opc == DW_CFA_expression  || cfi->dw_cfi_opc == DW_CFA_val_expression){
      unsigned r =
      mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,cfi->dw_cfi_oprnd1.dw_cfi_reg_num, for_eh);
      dw2_asm_output_data (1, r, NULL);
      loc = cfi->dw_cfi_oprnd2.dw_cfi_loc;
   }else
      loc = cfi->dw_cfi_oprnd1.dw_cfi_loc;

   /* Output the size of the block.  */
   size = size_of_locs (loc);
   mtcs_dwarf2_asm_output_data_uleb128/*!dw2_asm_output_data_uleb128*/(mtcsDwarf2Asm,size, NULL);
   /* Now output the operations themselves.  */
   mtcs_dwarf2_out_output_loc_sequence/*!output_loc_sequence*/(mtcsDwarf2Out,loc, for_eh);
}

/* Similar, but used for .cfi_escape.  */
static void output_cfa_loc_raw (MtcsDwarf2Cfi *self,dw_cfi_ref cfi)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsDwarf2Asm *mtcsDwarf2Asm=mtcs_target_get_dwarf2_asm(mtcsTarget);
   MtcsDwarf2Out *mtcsDwarf2Out=mtcs_target_get_dwarf2_out(mtcsTarget);

   dw_loc_descr_ref loc;
   unsigned long size;

   if (cfi->dw_cfi_opc == DW_CFA_expression || cfi->dw_cfi_opc == DW_CFA_val_expression){
      unsigned r =
      mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,cfi->dw_cfi_oprnd1.dw_cfi_reg_num, 1);
      fprintf (asm_out_file, "%#x,", r);
      loc = cfi->dw_cfi_oprnd2.dw_cfi_loc;
   }else
      loc = cfi->dw_cfi_oprnd1.dw_cfi_loc;

   /* Output the size of the block.  */
   size = size_of_locs (loc);
   mtcs_dwarf2_asm_output_data_uleb128_raw/*!dw2_asm_output_data_uleb128_raw*/(mtcsDwarf2Asm,size);
   fputc (',', asm_out_file);
   /* Now output the operations themselves.  */
   mtcs_dwarf2_out_output_loc_sequence_raw/*!output_loc_sequence_raw*/(mtcsDwarf2Out,loc);
}

/* Output a Call Frame Information opcode and its operand(s).  */
//原型 output_cfi dwarf2out.h
void mtcs_dwarf2_cfi_output_cfi (MtcsDwarf2Cfi *self,dw_cfi_ref cfi, dw_fde_ref fde, int for_eh)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsDwarf2Asm *mtcsDwarf2Asm =mtcs_target_get_dwarf2_asm(mtcsTarget);

   unsigned long r;
   HOST_WIDE_INT off;

   if (cfi->dw_cfi_opc == DW_CFA_advance_loc)
      dw2_asm_output_data (1, (cfi->dw_cfi_opc  | (cfi->dw_cfi_oprnd1.dw_cfi_offset & 0x3f)),
            "DW_CFA_advance_loc " HOST_WIDE_INT_PRINT_HEX,
            ((unsigned HOST_WIDE_INT) cfi->dw_cfi_oprnd1.dw_cfi_offset));
   else if (cfi->dw_cfi_opc == DW_CFA_offset){
      r = mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,cfi->dw_cfi_oprnd1.dw_cfi_reg_num, for_eh);
      dw2_asm_output_data (1, (cfi->dw_cfi_opc | (r & 0x3f)),"DW_CFA_offset, column %#lx", r);
      off = div_data_align(self,cfi->dw_cfi_oprnd2.dw_cfi_offset);
      mtcs_dwarf2_asm_output_data_uleb128/*!dw2_asm_output_data_uleb128*/(mtcsDwarf2Asm,off, NULL);
   }else if (cfi->dw_cfi_opc == DW_CFA_restore){
      r = mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,cfi->dw_cfi_oprnd1.dw_cfi_reg_num, for_eh);
      dw2_asm_output_data (1, (cfi->dw_cfi_opc | (r & 0x3f)),"DW_CFA_restore, column %#lx", r);
   }else{
      dw2_asm_output_data (1, cfi->dw_cfi_opc, "%s", dwarf_cfi_name (cfi->dw_cfi_opc));

      switch (cfi->dw_cfi_opc){
         case DW_CFA_set_loc:
            if (for_eh)
               mtcs_dwarf2_asm_output_encoded_addr_rtx/*!dw2_asm_output_encoded_addr_rtx*/(mtcsDwarf2Asm,
                     mtcs_asm_asm_preferred_eh_data_format/*!ASM_PREFERRED_EH_DATA_FORMAT*/(mtcsAsm,
                     /*code=*/1, /*global=*/0),
                     gen_rtx_SYMBOL_REF (mtcs_mode_get_Pmode(mtcsMode), cfi->dw_cfi_oprnd1.dw_cfi_addr),false, NULL);
            else
               mtcs_dwarf2_asm_output_addr/*!dw2_asm_output_addr*/(mtcsDwarf2Asm,DWARF2_ADDR_SIZE, cfi->dw_cfi_oprnd1.dw_cfi_addr, NULL);
            fde->dw_fde_current_label = cfi->dw_cfi_oprnd1.dw_cfi_addr;
            break;

         case DW_CFA_advance_loc1:
            dw2_asm_output_delta (1, cfi->dw_cfi_oprnd1.dw_cfi_addr,fde->dw_fde_current_label, NULL);
            fde->dw_fde_current_label = cfi->dw_cfi_oprnd1.dw_cfi_addr;
            break;

         case DW_CFA_advance_loc2:
            dw2_asm_output_delta (2, cfi->dw_cfi_oprnd1.dw_cfi_addr,fde->dw_fde_current_label, NULL);
            fde->dw_fde_current_label = cfi->dw_cfi_oprnd1.dw_cfi_addr;
            break;

         case DW_CFA_advance_loc4:
            dw2_asm_output_delta (4, cfi->dw_cfi_oprnd1.dw_cfi_addr,fde->dw_fde_current_label, NULL);
            fde->dw_fde_current_label = cfi->dw_cfi_oprnd1.dw_cfi_addr;
            break;

         case DW_CFA_MIPS_advance_loc8:
            dw2_asm_output_delta (8, cfi->dw_cfi_oprnd1.dw_cfi_addr,fde->dw_fde_current_label, NULL);
            fde->dw_fde_current_label = cfi->dw_cfi_oprnd1.dw_cfi_addr;
            break;

         case DW_CFA_offset_extended:
            r = mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,cfi->dw_cfi_oprnd1.dw_cfi_reg_num, for_eh);
            mtcs_dwarf2_asm_output_data_uleb128/*!dw2_asm_output_data_uleb128*/(mtcsDwarf2Asm,r, NULL);
            off = div_data_align(self,cfi->dw_cfi_oprnd2.dw_cfi_offset);
            mtcs_dwarf2_asm_output_data_uleb128/*!dw2_asm_output_data_uleb128*/(mtcsDwarf2Asm,off, NULL);
            break;

         case DW_CFA_def_cfa:
            r = mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,cfi->dw_cfi_oprnd1.dw_cfi_reg_num, for_eh);
            mtcs_dwarf2_asm_output_data_uleb128/*!dw2_asm_output_data_uleb128*/(mtcsDwarf2Asm,r, NULL);
            mtcs_dwarf2_asm_output_data_uleb128/*!dw2_asm_output_data_uleb128*/(mtcsDwarf2Asm,cfi->dw_cfi_oprnd2.dw_cfi_offset, NULL);
            break;

         case DW_CFA_offset_extended_sf:
            r = mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,cfi->dw_cfi_oprnd1.dw_cfi_reg_num, for_eh);
            mtcs_dwarf2_asm_output_data_uleb128/*!dw2_asm_output_data_uleb128*/(mtcsDwarf2Asm,r, NULL);
            off = div_data_align(self,cfi->dw_cfi_oprnd2.dw_cfi_offset);
            dw2_asm_output_data_sleb128 (off, NULL);
            break;

         case DW_CFA_def_cfa_sf:
            r = mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,cfi->dw_cfi_oprnd1.dw_cfi_reg_num, for_eh);
            mtcs_dwarf2_asm_output_data_uleb128/*!dw2_asm_output_data_uleb128*/(mtcsDwarf2Asm,r, NULL);
            off = div_data_align(self,cfi->dw_cfi_oprnd2.dw_cfi_offset);
            dw2_asm_output_data_sleb128 (off, NULL);
            break;

         case DW_CFA_restore_extended:
         case DW_CFA_undefined:
         case DW_CFA_same_value:
         case DW_CFA_def_cfa_register:
            r = mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,cfi->dw_cfi_oprnd1.dw_cfi_reg_num, for_eh);
            mtcs_dwarf2_asm_output_data_uleb128/*!dw2_asm_output_data_uleb128*/(mtcsDwarf2Asm,r, NULL);
            break;

         case DW_CFA_register:
            r = mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,cfi->dw_cfi_oprnd1.dw_cfi_reg_num, for_eh);
            mtcs_dwarf2_asm_output_data_uleb128/*!dw2_asm_output_data_uleb128*/(mtcsDwarf2Asm,r, NULL);
            r = mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,cfi->dw_cfi_oprnd2.dw_cfi_reg_num, for_eh);
            mtcs_dwarf2_asm_output_data_uleb128/*!dw2_asm_output_data_uleb128*/(mtcsDwarf2Asm,r, NULL);
            break;

         case DW_CFA_def_cfa_offset:
         case DW_CFA_GNU_args_size:
            mtcs_dwarf2_asm_output_data_uleb128/*!dw2_asm_output_data_uleb128*/(mtcsDwarf2Asm,cfi->dw_cfi_oprnd1.dw_cfi_offset, NULL);
            break;

         case DW_CFA_def_cfa_offset_sf:
            off = div_data_align(self,cfi->dw_cfi_oprnd1.dw_cfi_offset);
            dw2_asm_output_data_sleb128 (off, NULL);
            break;

         case DW_CFA_GNU_window_save:
            break;

         case DW_CFA_def_cfa_expression:
         case DW_CFA_expression:
         case DW_CFA_val_expression:
            output_cfa_loc(self,cfi, for_eh);
            break;

         case DW_CFA_GNU_negative_offset_extended:
            /* Obsoleted by DW_CFA_offset_extended_sf.  */
            gcc_unreachable ();

         default:
            break;
      }
   }
}

/* Similar, but do it via assembler directives instead.  */
//原型 output_cfi_directive dwarf2out.h  dwarf2cfi.cc
void mtcs_dwarf2_cfi_output_cfi_directive (MtcsDwarf2Cfi *self, FILE *f,dw_cfi_ref cfi)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsDwarf2Asm *mtcsDwarf2Asm=mtcs_target_get_dwarf2_asm(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   unsigned long r, r2;

   switch (cfi->dw_cfi_opc){
      case DW_CFA_advance_loc:
      case DW_CFA_advance_loc1:
      case DW_CFA_advance_loc2:
      case DW_CFA_advance_loc4:
      case DW_CFA_MIPS_advance_loc8:
      case DW_CFA_set_loc:
         /* Should only be created in a code path not followed when emitting
         via directives.  The assembler is going to take care of this for
         us.  But this routines is also used for debugging dumps, so
         print something.  */
         gcc_assert (f != asm_out_file);
         fprintf (f, "\t.cfi_advance_loc\n");
         break;

      case DW_CFA_offset:
      case DW_CFA_offset_extended:
      case DW_CFA_offset_extended_sf:
         r = mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,cfi->dw_cfi_oprnd1.dw_cfi_reg_num, 1);
         fprintf (f, "\t.cfi_offset %lu, " HOST_WIDE_INT_PRINT_DEC"\n",r, cfi->dw_cfi_oprnd2.dw_cfi_offset);
         break;

      case DW_CFA_restore:
      case DW_CFA_restore_extended:
         r = mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,cfi->dw_cfi_oprnd1.dw_cfi_reg_num, 1);
         fprintf (f, "\t.cfi_restore %lu\n", r);
         break;

      case DW_CFA_undefined:
         r = mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,cfi->dw_cfi_oprnd1.dw_cfi_reg_num, 1);
         fprintf (f, "\t.cfi_undefined %lu\n", r);
         break;

      case DW_CFA_same_value:
         r = mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,cfi->dw_cfi_oprnd1.dw_cfi_reg_num, 1);
         fprintf (f, "\t.cfi_same_value %lu\n", r);
         break;

      case DW_CFA_def_cfa:
      case DW_CFA_def_cfa_sf:
         r = mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,cfi->dw_cfi_oprnd1.dw_cfi_reg_num, 1);
         fprintf (f, "\t.cfi_def_cfa %lu, " HOST_WIDE_INT_PRINT_DEC"\n", r, cfi->dw_cfi_oprnd2.dw_cfi_offset);
         break;

      case DW_CFA_def_cfa_register:
         r = mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,cfi->dw_cfi_oprnd1.dw_cfi_reg_num, 1);
         fprintf (f, "\t.cfi_def_cfa_register %lu\n", r);
         break;

      case DW_CFA_register:
         r = mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,cfi->dw_cfi_oprnd1.dw_cfi_reg_num, 1);
         r2 = mtcs_reg_get_dwarf2_frame_reg_out/*!DWARF2_FRAME_REG_OUT*/(mtcsReg,cfi->dw_cfi_oprnd2.dw_cfi_reg_num, 1);
         fprintf (f, "\t.cfi_register %lu, %lu\n", r, r2);
         break;

      case DW_CFA_def_cfa_offset:
      case DW_CFA_def_cfa_offset_sf:
         fprintf (f, "\t.cfi_def_cfa_offset " HOST_WIDE_INT_PRINT_DEC"\n", cfi->dw_cfi_oprnd1.dw_cfi_offset);
         break;

      case DW_CFA_remember_state:
         fprintf (f, "\t.cfi_remember_state\n");
         break;
      case DW_CFA_restore_state:
         fprintf (f, "\t.cfi_restore_state\n");
         break;

      case DW_CFA_GNU_args_size:
         if (f ==mtcsAsm/*!asm_out_file*/->asmFile){
            fprintf (f, "\t.cfi_escape %#x,", DW_CFA_GNU_args_size);
            mtcs_dwarf2_asm_output_data_uleb128_raw/*!dw2_asm_output_data_uleb128_raw*/(mtcsDwarf2Asm,cfi->dw_cfi_oprnd1.dw_cfi_offset);
            if (mtcsOptionsItem->x_flag_debug_asm)
            fprintf (f, "\t%s args_size " HOST_WIDE_INT_PRINT_DEC,
                  mtcs_asm_get_comment_start/*!ASM_COMMENT_START*/(mtcsAsm), cfi->dw_cfi_oprnd1.dw_cfi_offset);
            fputc ('\n', f);
         }else{
            fprintf (f, "\t.cfi_GNU_args_size " HOST_WIDE_INT_PRINT_DEC "\n",cfi->dw_cfi_oprnd1.dw_cfi_offset);
         }
         break;

      case DW_CFA_GNU_window_save:
         fprintf (f, "\t.cfi_window_save\n");
         break;

      case DW_CFA_def_cfa_expression:
      case DW_CFA_expression:
      case DW_CFA_val_expression:
         if (f != mtcsAsm->/*!asm_out_file*/asmFile){
            fprintf (f, "\t.cfi_%scfa_%sexpression ...\n",
            cfi->dw_cfi_opc == DW_CFA_def_cfa_expression ? "def_" : "",
            cfi->dw_cfi_opc == DW_CFA_val_expression ? "val_" : "");
            break;
         }
         fprintf (f, "\t.cfi_escape %#x,", cfi->dw_cfi_opc);
         output_cfa_loc_raw(self,cfi);
         fputc ('\n', f);
         break;

      default:
         gcc_unreachable ();
   }
}

//原型 dwarf2out_emit_cfi dwarf2out.h dwarf2cfi.cc
void mtcs_dwarf2_cfi_out_emit_cfi (MtcsDwarf2Cfi *self,dw_cfi_ref cfi)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

  if (mtcs_dwarf2_cfi_dwarf2out_do_cfi_asm(self))
      mtcs_dwarf2_cfi_output_cfi_directive (self,mtcsAsm->asmFile, cfi);
}

static void dump_cfi_row (MtcsDwarf2Cfi *self,FILE *f, dw_cfi_row *row)
{
   dw_cfi_ref cfi;
   unsigned i;

   cfi = row->cfa_cfi;
   if (!cfi){
      dw_cfa_location dummy;
      memset (&dummy, 0, sizeof (dummy));
      dummy.reg.set_by_dwreg (INVALID_REGNUM);
      cfi = def_cfa_0(self,&dummy, &row->cfa);
   }
   mtcs_dwarf2_cfi_output_cfi_directive/*!output_cfi_directive*/(self,f, cfi);

   FOR_EACH_VEC_SAFE_ELT (row->reg_save, i, cfi)
      if (cfi)
         mtcs_dwarf2_cfi_output_cfi_directive/*!output_cfi_directive*/(self,f, cfi);
}



/* Decide whether to emit EH frame unwind information for the current
   translation unit.  */
//原型 dwarf2out_do_eh_frame debug.h dwarf2cfi.cc
bool mtcs_dwarf2_cfi_dwarf2out_do_eh_frame (MtcsDwarf2Cfi *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   n_debug("mtcsdwarf2cfi.c dwarf2out_do_eh_frame %d %d %d %d\n",mtcsOptionsItem->x_flag_unwind_tables ,
   mtcsOptionsItem->x_flag_exceptions,
   target_common_except_unwind_info/*!targetm_common.except_unwind_info*/(mtcsMachine->common,mtcsOptionsItem)  == UI_DWARF2,
   target_common_except_unwind_info/*!targetm_common.except_unwind_info*/(mtcsMachine->common,mtcsOptionsItem));
   return    (mtcsOptionsItem->x_flag_unwind_tables  || mtcsOptionsItem->x_flag_exceptions)
   && target_common_except_unwind_info/*!targetm_common.except_unwind_info*/(mtcsMachine->common,mtcsOptionsItem) == UI_DWARF2;
}

/* Decide whether we want to emit frame unwind information for the current
   translation unit.  */

/* Decide whether we want to emit frame unwind information for the current
   translation unit.  */
//原型 dwarf2out_do_frame debug.h dwarf2cfi.cc
bool mtcs_dwarf2_cfi_dwarf2out_do_frame (MtcsDwarf2Cfi *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpts *mtcsOpts = mtcs_target_get_opts(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   /* We want to emit correct CFA location expressions or lists, so we
   have to return true if we're going to generate debug info, even if
   we're not going to output frame or unwind info.  */
   if (mtcs_opts_dwarf_debuginfo_p/*!dwarf_debuginfo_p*/(mtcsOpts,mtcsOptionsItem) || dwarf_based_debuginfo_p ()){
      n_debug("mtcsdwarf2cfi.c  dwarf2out_do_frame 00  true if (dwarf_debuginfo_p () || dwarf_based_debuginfo_p ())\n");
      return true;
   }

   if (self->saved_do_cfi_asm > 0){
      n_debug("mtcsdwarf2cfi.c  dwarf2out_do_frame 11  true if (saved_do_cfi_asm > 0)\n");
      return true;
   }

   if (mtcsTarget/*!targetm.debug_unwind_info*/->debug_unwind_info(mtcsTarget) == UI_DWARF2){
      n_debug("mtcsdwarf2cfi.c  dwarf2out_do_frame 22  true if (targetm.debug_unwind_info () == UI_DWARF2)\n");
      return true;
   }

   if (mtcs_dwarf2_cfi_dwarf2out_do_eh_frame/*!dwarf2out_do_eh_frame*/(self)){
      n_debug("mtcsdwarf2cfi.c  dwarf2out_do_frame 33  true  if (dwarf2out_do_eh_frame ())\n");
      return true;
   }
   n_debug("mtcsdwarf2cfi.c  dwarf2out_do_frame 44  false\n");

   return false;
}

/* Decide whether to emit frame unwind via assembler directives.  */
//原型 dwarf2out_do_cfi_asm debug.h dwarf2cfi.cc
bool mtcs_dwarf2_cfi_dwarf2out_do_cfi_asm (MtcsDwarf2Cfi *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm = mtcs_target_get_asm(mtcsTarget);
   MtcsConfig *mtcsConfig = mtcs_target_get_config(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   int enc;

   if (self->saved_do_cfi_asm != 0)
      return self->saved_do_cfi_asm > 0;

   /* Assume failure for a moment.  */
   self->saved_do_cfi_asm = -1;

   if (!mtcsOptionsItem->x_flag_dwarf2_cfi_asm || !mtcs_dwarf2_cfi_dwarf2out_do_frame/*!dwarf2out_do_frame*/(self))
      return false;
   if (!mtcs_config_get_value/*!HAVE_GAS_CFI_PERSONALITY_DIRECTIVE*/(mtcsConfig,MTCS_HAVE_GAS_CFI_PERSONALITY_DIRECTIVE))
      return false;

   /* Make sure the personality encoding is one the assembler can support.
   In particular, aligned addresses can't be handled.  */
   enc = mtcs_asm_asm_preferred_eh_data_format/*!ASM_PREFERRED_EH_DATA_FORMAT*/(mtcsAsm,/*code=*/2,/*global=*/1);
   if ((enc & 0x70) != 0 && (enc & 0x70) != DW_EH_PE_pcrel)
      return false;
   enc = mtcs_asm_asm_preferred_eh_data_format/*!ASM_PREFERRED_EH_DATA_FORMAT*/(mtcsAsm,/*code=*/0,/*global=*/0);
   if ((enc & 0x70) != 0 && (enc & 0x70) != DW_EH_PE_pcrel)
      return false;

   /* If we can't get the assembler to emit only .debug_frame, and we don't need
   dwarf2 unwind info for exceptions, then emit .debug_frame by hand.  */
   if (!mtcs_config_get_value/*!HAVE_GAS_CFI_SECTIONS_DIRECTIVE*/(mtcsConfig,MTCS_HAVE_GAS_CFI_SECTIONS_DIRECTIVE)
         && !mtcs_dwarf2_cfi_dwarf2out_do_eh_frame/*!dwarf2out_do_eh_frame*/(self))
      return false;

   /* Success!  */
   self->saved_do_cfi_asm = 1;
   return true;
}

//原型 dwarf2cfi_cc_finalize dwarf2out.h dwarf2cfi.cc
void mtcs_dwarf2_cfi_dwarf2cfi_cc_finalize (MtcsDwarf2Cfi *self)
{
  self->add_cfi_insn = NULL;
  self->add_cfi_vec = NULL;
  self->cur_trace = NULL;
  self->cur_row = NULL;
  self->cur_cfa = NULL;
}


static void mtcsDwarf2CfiInit(MtcsDwarf2Cfi *self)
{
    self->saved_do_cfi_asm=0;
}


MtcsDwarf2Cfi *mtcs_dwarf2_cfi_new(MtcsMode *mtcsMode)
{
    MtcsDwarf2Cfi *self = n_slice_alloc0 (sizeof(MtcsDwarf2Cfi));
    mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
    mtcsDwarf2CfiInit(self);
    return self;
}



//原型 NEXT_PASS (pass_dwarf2_frame, 1);  RTL_PASS  dwarf2cfi.cc dwarf2   y 有条件执行 targetm.have_prologue execute_dwarf2_frame
static nboolean dwarf2_frame_gate_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDwarf2Cfi *mtcsDwarf2Cfi =mtcs_target_get_dwarf2_cfi(mtcsTarget);
   MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

   /* Targets which still implement the prologue in assembler text
   cannot use the generic dwarf2 unwinding.  */
   if (!target_rtx_have_prologue/*!targetm.have_prologue*/(mtcsMachine->tmrtx))
      return false;

   /* ??? What to do for UI_TARGET unwinding?  They might be able to benefit
   from the optimized shrink-wrapping annotations that we will compute.
   For now, only produce the CFI notes for dwarf2.  */
   return mtcs_dwarf2_cfi_dwarf2out_do_frame/*!dwarf2out_do_frame*/(mtcsDwarf2Cfi);
}

static nuint dwarf2_frame_execute_cb(MtcsPass *mtcsPass,function *func)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlPassMgr *mtcsRtlPassMgr=mtcs_target_get_rtl_pass_mgr(mtcsTarget);
   MtcsDwarf2Cfi *mtcsDwarf2Cfi =mtcs_target_get_dwarf2_cfi(mtcsTarget);

   execute_dwarf2_frame (mtcsDwarf2Cfi);
   return 0;
}

static void mtcsPassDwarf2FrameInit(MtcsPassDwarf2Frame *self)
{
   MtcsPass *mtcsPass= (MtcsPass *)self;
   mtcsPass->execute =dwarf2_frame_execute_cb;
   mtcsPass->gate =dwarf2_frame_gate_cb;
   mtcs_pass_set_properties(mtcsPass,
      0,/* properties_required */
      0, /* properties_provided */
      0 /* properties_destroyed */);
   mtcs_pass_set_todo_flags(mtcsPass,
      0, /* todo_flags_start */
      0 /*todo_flags_finish */);
}

MtcsPassDwarf2Frame *mtcs_pass_dwarf2_frame_new (MtcsMode *mtcsMode)
{
   MtcsPassDwarf2Frame *self = n_slice_alloc0 (sizeof(MtcsPassDwarf2Frame));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"dwarf2");
   mtcsPassDwarf2FrameInit(self);
   return self;
}

