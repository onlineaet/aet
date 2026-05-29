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
 * base on expmed.cc
 */


/* This file handles generation of all the assembler code
   *except* the instructions of a lowtion.
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
#include "tree-vector-builder.h"

#include "mtcsexpmed.h"
#include "mtcstarget.h"
#include "mtcsasm.h"

/* Test whether a value is zero of a power of two.  */
//原型 EXACT_POWER_OF_2_OR_ZERO_P expmed.cc
#define EXACT_POWER_OF_2_OR_ZERO_P(x) \
  (((x) & ((x) - HOST_WIDE_INT_1U)) == 0)

struct init_expmed_rtl
{
  rtx reg;
  rtx plus;
  rtx neg;
  rtx mult;
  rtx sdiv;
  rtx udiv;
  rtx sdiv_32;
  rtx smod_32;
  rtx wide_mult;
  rtx wide_lshr;
  rtx wide_trunc;
  rtx shift;
  rtx shift_mult;
  rtx shift_add;
  rtx shift_sub0;
  rtx shift_sub1;
  rtx zext;
  rtx trunc;

  rtx pow2[MAX_BITS_PER_WORD];
  rtx cint[MAX_BITS_PER_WORD];
};

static rtx emit_store_flag_1 (MtcsExpmed *self,rtx target, enum rtx_code code, rtx op0, rtx op1,
           machine_mode mode, int unsignedp, int normalizep,machine_mode target_mode);

//原型 expand_shift_1 expmed.cc
static rtx expand_shift_1 (MtcsExpmed *self,enum tree_code code, machine_mode mode, rtx shifted,
        rtx amount, rtx target, int unsignedp, bool may_fail = false);

//原型 do_cmp_and_jump expmed.cc
static void do_cmp_and_jump (MtcsExpmed *self,rtx arg1, rtx arg2, enum rtx_code op, machine_mode mode,
         rtx_code_label *label);
//原型 invert_mod2n expmed.cc
static unsigned HOST_WIDE_INT invert_mod2n (unsigned HOST_WIDE_INT x, int n);

//原型 expmed_mult_highpart expmed.cc
static rtx expmed_mult_highpart (MtcsExpmed *self,scalar_int_mode mode, rtx op0, rtx op1,
              rtx target, int unsignedp, int max_cost);
//原型 expmed_mult_highpart_optab expmed.cc
static rtx expmed_mult_highpart_optab (MtcsExpmed *self,scalar_int_mode mode, rtx op0, rtx op1,
                rtx target, int unsignedp, int max_cost);
//原型 extract_high_half expmed.cc
static rtx extract_high_half (MtcsExpmed *self,scalar_int_mode mode, rtx op);
//原型 expand_mult_const expmed.cc
static rtx expand_mult_const (MtcsExpmed *self,machine_mode mode, rtx op0, HOST_WIDE_INT val,
           rtx target, const struct algorithm *alg,enum mult_variant variant);
//原型 expand_sdiv_pow2 exmped.cc
static rtx expand_sdiv_pow2 (MtcsExpmed *self,scalar_int_mode mode, rtx op0, HOST_WIDE_INT d);

//原型 synth_mult expmed.cc
static void synth_mult (MtcsExpmed *self,struct algorithm *alg_out, unsigned HOST_WIDE_INT t,
        const struct mult_cost *cost_limit, machine_mode mode);

//原型 expand_smod_pow2 expmed.cc
static rtx expand_smod_pow2 (MtcsExpmed *self,scalar_int_mode mode, rtx op0, HOST_WIDE_INT d);

//原型 expand_sdiv_pow2 exmped.cc
static rtx expand_sdiv_pow2 (MtcsExpmed *self,scalar_int_mode mode, rtx op0, HOST_WIDE_INT d);
//原型 store_bit_field_1 expmed.cc
static bool store_bit_field_1 (MtcsExpmed *self,rtx str_rtx, poly_uint64 bitsize, poly_uint64 bitnum,
           poly_uint64 bitregion_start, poly_uint64 bitregion_end,
           machine_mode fieldmode, rtx value, bool reverse, bool fallback_p, bool undefined_p);
//原型 extract_bit_field_as_subreg expmed.cc
static rtx extract_bit_field_as_subreg (MtcsExpmed *self,machine_mode mode, rtx op0,
                 machine_mode op0_mode,poly_uint64 bitsize, poly_uint64 bitnum);

//原型 store_integral_bit_field expmed.cc
static bool store_integral_bit_field(MtcsExpmed *self,rtx op0, opt_scalar_int_mode op0_mode,
              unsigned HOST_WIDE_INT bitsize,
              unsigned HOST_WIDE_INT bitnum,
              poly_uint64 bitregion_start,
              poly_uint64 bitregion_end,
              machine_mode fieldmode,
              rtx value, bool reverse, bool fallback_p);
//原型 lowpart_bit_field_p expmed.cc
static bool lowpart_bit_field_p (MtcsExpmed *self,poly_uint64 bitnum, poly_uint64 bitsize, machine_mode struct_mode);
//原型 convert_extracted_bit_field expmed.cc
static rtx convert_extracted_bit_field (MtcsExpmed *self,rtx x, machine_mode mode, machine_mode tmode, bool unsignedp);

//原型 simple_mem_bitfield_p expmed.cc
static bool simple_mem_bitfield_p (MtcsExpmed *self,rtx op0, poly_uint64 bitsize, poly_uint64 bitnum,
               machine_mode mode, poly_uint64 *bytenum);

//原型 extract_bit_field_1 expmed.cc
static rtx extract_bit_field_1 (MtcsExpmed *self,rtx str_rtx, poly_uint64 bitsize, poly_uint64 bitnum,
             int unsignedp, rtx target, machine_mode mode, machine_mode tmode, bool reverse, bool fallback_p,rtx *alt_rtl);

//原型 extract_fixed_bit_field expmed.cc
static rtx extract_fixed_bit_field (MtcsExpmed *self,machine_mode tmode, rtx op0,
             opt_scalar_int_mode op0_mode,
             unsigned HOST_WIDE_INT bitsize,
             unsigned HOST_WIDE_INT bitnum, rtx target,
             int unsignedp, bool reverse);
//原型 narrow_bit_field_mem expmed.cc
static rtx narrow_bit_field_mem (MtcsExpmed *self,rtx mem, opt_scalar_int_mode mode,
              unsigned HOST_WIDE_INT bitsize,
              unsigned HOST_WIDE_INT bitnum,
              unsigned HOST_WIDE_INT *new_bitnum);

//原型 store_fixed_bit_field_1 expmed.cc
static void store_fixed_bit_field_1 (MtcsExpmed *self,rtx op0, scalar_int_mode mode,
             unsigned HOST_WIDE_INT bitsize,
             unsigned HOST_WIDE_INT bitnum,
             rtx value, scalar_int_mode value_mode, bool reverse);

//原型 store_fixed_bit_field expmed.cc
static void store_fixed_bit_field (MtcsExpmed *self,rtx op0, opt_scalar_int_mode op0_mode,
               unsigned HOST_WIDE_INT bitsize,
               unsigned HOST_WIDE_INT bitnum,
               poly_uint64 bitregion_start, poly_uint64 bitregion_end,
               rtx value, scalar_int_mode value_mode, bool reverse);

static void mtcsExpmedInit(MtcsExpmed *self)
{
    self->reverse_storage_order_supported=-1;//原型 expmed.cc 缺省=-1
    self->reverse_float_storage_order_supported=-1;//原型 expmed.cc 缺省=-1
}

/* Check whether reverse storage order is supported on the target.  */
//原型 check_reverse_storage_order_support expmed.cc
static void check_reverse_storage_order_support (MtcsExpmed *self)
{
  if (BYTES_BIG_ENDIAN != WORDS_BIG_ENDIAN){
      self->reverse_storage_order_supported = 0;
      sorry ("reverse scalar storage order");
  }else
    self->reverse_storage_order_supported = 1;
}

/* Check whether reverse FP storage order is supported on the target.  */
//原型 check_reverse_float_storage_order_support expmed.cc
static void check_reverse_float_storage_order_support (MtcsExpmed *self)
{
  if (FLOAT_WORDS_BIG_ENDIAN != WORDS_BIG_ENDIAN){
      self->reverse_float_storage_order_supported = 0;
      sorry ("reverse floating-point scalar storage order");
  }else
    self->reverse_float_storage_order_supported = 1;
}

/* Return a constant integer (CONST_INT or CONST_DOUBLE) rtx with the value
   VALUE << BITPOS.  */
//原型 lshift_value expmed.cc
static rtx lshift_value (MtcsExpmed *self,machine_mode mode, unsigned HOST_WIDE_INT value,int bitpos)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  return mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,wi::lshift (value, bitpos), mode);
}

static inline rtx mask_rtx (MtcsExpmed *self,scalar_int_mode mode, int bitpos, int bitsize, bool complement)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  return mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,wi::shifted_mask (bitpos, bitsize, complement,
               mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode)), mode);
}

/* The caller wants to perform insertion or extraction PATTERN on a
   bitfield of size BITSIZE at BITNUM bits into memory operand OP0.
   BITREGION_START and BITREGION_END are as for store_bit_field
   and FIELDMODE is the natural mode of the field.

   Search for a mode that is compatible with the memory access
   restrictions and (where applicable) with a register insertion or
   extraction.  Return the new memory on success, storing the adjusted
   bit position in *NEW_BITNUM.  Return null otherwise.  */
//原型 adjust_bit_field_mem_for_reg expmed.cc
static rtx adjust_bit_field_mem_for_reg (MtcsExpmed *self,enum extraction_pattern pattern,
                  rtx op0, HOST_WIDE_INT bitsize,
                  HOST_WIDE_INT bitnum,
                  poly_uint64 bitregion_start,
                  poly_uint64 bitregion_end,
                  machine_mode fieldmode,
                  unsigned HOST_WIDE_INT *new_bitnum)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

  mtcs_bit_field_mode_iterator iter (mtcsMode,bitsize, bitnum, bitregion_start,
                bitregion_end, mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,op0),
                MEM_VOLATILE_P (op0));
  scalar_int_mode best_mode;
  if (iter.next_mode (&best_mode)){
      /* We can use a memory in BEST_MODE.  See whether this is true for
     any wider modes.  All other things being equal, we prefer to
     use the widest mode possible because it tends to expose more
     CSE opportunities.  */
      if (!iter.prefer_smaller_modes ()){
          /* Limit the search to the mode required by the corresponding
             register insertion or extraction instruction, if any.  */
          scalar_int_mode limit_mode = word_mode;
          extraction_insn insn;
          if (mtcs_optabs_get_best_reg_extraction_insn/*!get_best_reg_extraction_insn*/(mtcsOptabs,&insn, pattern,
                  mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,best_mode),
                            fieldmode))
            limit_mode = insn.field_mode;

          scalar_int_mode wider_mode;
          while (iter.next_mode (&wider_mode) && mtcs_mode_get_size(mtcsMode,wider_mode) <= mtcs_mode_get_size(mtcsMode,limit_mode))
            best_mode = wider_mode;
      }
      return narrow_bit_field_mem(self,op0, best_mode, bitsize, bitnum,new_bitnum);
  }
  return NULL_RTX;
}


/* Store a bit field that is split across multiple accessible memory objects.

   OP0 is the REG, SUBREG or MEM rtx for the first of the objects.
   BITSIZE is the field width; BITPOS the position of its first bit
   (within the word).
   VALUE is the value to store, which has mode VALUE_MODE.
   If OP0_MODE is defined, it is the mode of OP0, otherwise OP0 is
   a BLKmode MEM.

   If REVERSE is true, the store is to be done in reverse order.

   This does not yet handle fields wider than BITS_PER_WORD.  */
//原型 store_split_bit_field expmed.cc
static void store_split_bit_field(MtcsExpmed *self,rtx op0, opt_scalar_int_mode op0_mode,
               unsigned HOST_WIDE_INT bitsize,
               unsigned HOST_WIDE_INT bitpos,
               poly_uint64 bitregion_start, poly_uint64 bitregion_end,
               rtx value, scalar_int_mode value_mode, bool reverse)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

  unsigned int unit, total_bits, bitsdone = 0;

  /* Make sure UNIT isn't larger than BITS_PER_WORD, we can only handle that
     much at a time.  */
  if (REG_P (op0) || GET_CODE (op0) == SUBREG)
    unit = BITS_PER_WORD;
  else
    unit = MIN (mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,op0), BITS_PER_WORD);

  /* If OP0 is a memory with a mode, then UNIT must not be larger than
     OP0's mode as well.  Otherwise, store_fixed_bit_field will call us
     again, and we will mutually recurse forever.  */
  if (MEM_P (op0) && op0_mode.exists ())
    unit = MIN (unit, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,op0_mode.require ()));

  /* If VALUE is a constant other than a CONST_INT, get it into a register in
     WORD_MODE.  If we can do this using gen_lowpart_common, do so.  Note
     that VALUE might be a floating-point constant.  */
  if (CONSTANT_P (value) && !CONST_INT_P (value)){
      rtx word = mtcs_rtl_gen_lowpart_common/*!gen_lowpart_common*/(mtcsRTL,word_mode, value);
      if (word && (value != word))
          value = word;
      else
          value = mtcs_rtl_gen_lowpart_common/*!gen_lowpart_common*/(mtcsRTL,
                word_mode, mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,value_mode, value));
      value_mode = word_mode;
  }

  total_bits =mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,value_mode);

  while (bitsdone < bitsize){
      unsigned HOST_WIDE_INT thissize;
      unsigned HOST_WIDE_INT thispos;
      unsigned HOST_WIDE_INT offset;
      rtx part;

      offset = (bitpos + bitsdone) / unit;
      thispos = (bitpos + bitsdone) % unit;

      /* When region of bytes we can touch is restricted, decrease
     UNIT close to the end of the region as needed.  If op0 is a REG
     or SUBREG of REG, don't do this, as there can't be data races
     on a register and we can expand shorter code in some cases.  */
      if (maybe_ne (bitregion_end, 0U)
          && unit > BITS_PER_UNIT
          && maybe_gt (bitpos + bitsdone - thispos + unit, bitregion_end + 1)
          && !REG_P (op0)
          && (GET_CODE (op0) != SUBREG || !REG_P (SUBREG_REG (op0)))){
          unit = unit / 2;
          continue;
      }

      /* THISSIZE must not overrun a word boundary.  Otherwise,
     store_fixed_bit_field will call us again, and we will mutually
     recurse forever.  */
      thissize = MIN (bitsize - bitsdone, BITS_PER_WORD);
      thissize = MIN (thissize, unit - thispos);

      if (reverse ? !BYTES_BIG_ENDIAN : BYTES_BIG_ENDIAN){
          /* Fetch successively less significant portions.  */
          if (CONST_INT_P (value))
            part = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,((unsigned HOST_WIDE_INT) (INTVAL (value)) >> (bitsize - bitsdone - thissize))
                    & ((HOST_WIDE_INT_1 << thissize) - 1));
              /* Likewise, but the source is little-endian.  */
          else if (reverse)
                part = extract_fixed_bit_field(self,word_mode, value, value_mode,
                                thissize,bitsize - bitsdone - thissize,NULL_RTX, 1, false);
          else
            /* The args are chosen so that the last part includes the
               lsb.  Give extract_bit_field the value it needs (with
               endianness compensation) to fetch the piece we want.  */
            part = extract_fixed_bit_field (self,word_mode, value, value_mode,
                            thissize,total_bits - bitsize + bitsdone,NULL_RTX, 1, false);
      }else{
          /* Fetch successively more significant portions.  */
          if (CONST_INT_P (value))
             part = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,((unsigned HOST_WIDE_INT) (INTVAL (value)) >> bitsdone)
                     & ((HOST_WIDE_INT_1 << thissize) - 1));
          /* Likewise, but the source is big-endian.  */
          else if (reverse)
             part = extract_fixed_bit_field(self,word_mode, value, value_mode,
                            thissize,total_bits - bitsdone - thissize,NULL_RTX, 1, false);
          else
             part = extract_fixed_bit_field(self,word_mode, value, value_mode,
                            thissize, bitsdone, NULL_RTX,1, false);
      }

      /* If OP0 is a register, then handle OFFSET here.  */
      rtx op0_piece = op0;
      opt_scalar_int_mode op0_piece_mode = op0_mode;
      if (SUBREG_P (op0) || REG_P (op0)){
          scalar_int_mode imode;
          if (op0_mode.exists (&imode) && mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,imode) < UNITS_PER_WORD){
              if (offset)
                  op0_piece = const0_rtx;
          }else{
              op0_piece = mtcs_rtl_operand_subword_force/*!operand_subword_force*/(mtcsRTL,op0,offset * unit / BITS_PER_WORD,GET_MODE (op0));
              op0_piece_mode = word_mode;
          }
          offset &= BITS_PER_WORD / unit - 1;
      }

      /* OFFSET is in UNITs, and UNIT is in bits.  If WORD is const0_rtx,
     it is just an out-of-bounds access.  Ignore it.  */
      if (op0_piece != const0_rtx)
        store_fixed_bit_field(self,op0_piece, op0_piece_mode, thissize,
                       offset * unit + thispos, bitregion_start,
                       bitregion_end, part, word_mode, reverse);
      bitsdone += thissize;
  }
}

/* See whether it would be valid to extract the part of OP0 with
   mode OP0_MODE described by BITNUM and BITSIZE into a value of
   mode MODE using a subreg operation.
   Return the subreg if so, otherwise return null.  */
//原型 extract_bit_field_as_subreg expmed.cc
static rtx extract_bit_field_as_subreg (MtcsExpmed *self,machine_mode mode, rtx op0,
                 machine_mode op0_mode,poly_uint64 bitsize, poly_uint64 bitnum)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

  poly_uint64 bytenum;
  if (multiple_p (bitnum, BITS_PER_UNIT, &bytenum)
      && known_eq (bitsize, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode))
      && lowpart_bit_field_p(self,bitnum, bitsize, op0_mode)
      && mtcs_mode_truly_noop_truncation_p/*!TRULY_NOOP_TRUNCATION_MODES_P*/(mtcsMode,mode, op0_mode))
    return mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,mode, op0, op0_mode, bytenum);
  return NULL_RTX;
}

/* Extract a bit field that is split across two words
   and return an RTX for the result.

   OP0 is the REG, SUBREG or MEM rtx for the first of the two words.
   BITSIZE is the field width; BITPOS, position of its first bit, in the word.
   UNSIGNEDP is 1 if should zero-extend the contents; else sign-extend.
   If OP0_MODE is defined, it is the mode of OP0, otherwise OP0 is
   a BLKmode MEM.

   If REVERSE is true, the extraction is to be done in reverse order.  */
//原型 extract_split_bit_field expmed.cc
static rtx extract_split_bit_field (MtcsExpmed *self,rtx op0, opt_scalar_int_mode op0_mode,
             unsigned HOST_WIDE_INT bitsize,unsigned HOST_WIDE_INT bitpos, int unsignedp,bool reverse)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  unsigned int unit;
  unsigned int bitsdone = 0;
  rtx result = NULL_RTX;
  int first = 1;

  /* Make sure UNIT isn't larger than BITS_PER_WORD, we can only handle that
     much at a time.  */
  if (REG_P (op0) || GET_CODE (op0) == SUBREG)
    unit = BITS_PER_WORD;
  else
    unit = MIN (mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,op0), BITS_PER_WORD);

  while (bitsdone < bitsize){
      unsigned HOST_WIDE_INT thissize;
      rtx part;
      unsigned HOST_WIDE_INT thispos;
      unsigned HOST_WIDE_INT offset;

      offset = (bitpos + bitsdone) / unit;
      thispos = (bitpos + bitsdone) % unit;

      /* THISSIZE must not overrun a word boundary.  Otherwise,
     extract_fixed_bit_field will call us again, and we will mutually
     recurse forever.  */
      thissize = MIN (bitsize - bitsdone, BITS_PER_WORD);
      thissize = MIN (thissize, unit - thispos);

      /* If OP0 is a register, then handle OFFSET here.  */
      rtx op0_piece = op0;
      opt_scalar_int_mode op0_piece_mode = op0_mode;
      if (SUBREG_P (op0) || REG_P (op0)){
          op0_piece = mtcs_rtl_operand_subword_force/*!operand_subword_force*/(mtcsRTL,op0, offset, op0_mode.require ());
          op0_piece_mode = word_mode;
          offset = 0;
      }

      /* Extract the parts in bit-counting order,
     whose meaning is determined by BYTES_PER_UNIT.
     OFFSET is in UNITs, and UNIT is in bits.  */
      part = extract_fixed_bit_field(self,word_mode, op0_piece, op0_piece_mode,
                      thissize, offset * unit + thispos,0, 1, reverse);
      bitsdone += thissize;

      /* Shift this part into place for the result.  */
      if (reverse ? !BYTES_BIG_ENDIAN : BYTES_BIG_ENDIAN){
          if (bitsize != bitsdone)
            part =mtcs_expmed_expand_shift(self,LSHIFT_EXPR, word_mode, part,bitsize - bitsdone, 0, 1);
      }else{
          if (bitsdone != thissize)
            part = mtcs_expmed_expand_shift(self,LSHIFT_EXPR, word_mode, part,bitsdone - thissize, 0, 1);
      }

      if (first)
          result = part;
      else
        /* Combine the parts with bitwise or.  This works
           because we extracted each part as an unsigned bit field.  */
        result = mtcs_optabs_expand_binop(mtcsOptabs,word_mode, ior_optab, part, result, NULL_RTX, 1,OPTAB_LIB_WIDEN);

      first = 0;
  }

  /* Unsigned bit field: we are done.  */
  if (unsignedp)
    return result;
  /* Signed bit field: sign-extend with two arithmetic shifts.  */
  result = mtcs_expmed_expand_shift(self,LSHIFT_EXPR, word_mode, result, BITS_PER_WORD - bitsize, NULL_RTX, 0);
  return mtcs_expmed_expand_shift(self,RSHIFT_EXPR, word_mode, result,BITS_PER_WORD - bitsize, NULL_RTX, 0);
}

/* Try to use an ext(z)v pattern to extract a field from OP0.
   Return the extracted value on success, otherwise return null.
   EXTV describes the extraction instruction to use.  If OP0_MODE
   is defined, it is the mode of OP0, otherwise OP0 is a BLKmode MEM.
   The other arguments are as for extract_bit_field.  */
//原型 extract_bit_field_using_extv expmed.cc
static rtx extract_bit_field_using_extv (MtcsExpmed *self,const extraction_insn *extv, rtx op0,
                  opt_scalar_int_mode op0_mode,
                  unsigned HOST_WIDE_INT bitsize,
                  unsigned HOST_WIDE_INT bitnum,
                  int unsignedp, rtx target,
                  machine_mode mode, machine_mode tmode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsRTL *mtcsRTL = mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  class expand_operand ops[4];
  rtx spec_target = target;
  rtx spec_target_subreg = 0;
  scalar_int_mode ext_mode = extv->field_mode;
  unsigned unit = mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,ext_mode);

  if (bitsize == 0 || unit < bitsize)
    return NULL_RTX;

  if (MEM_P (op0))
    /* Get a reference to the first byte of the field.  */
    op0 = narrow_bit_field_mem (self,op0, extv->struct_mode, bitsize, bitnum, &bitnum);
  else{
      /* Convert from counting within OP0 to counting in EXT_MODE.  */
      if (BYTES_BIG_ENDIAN)
          bitnum += unit - mtcs_mode_get_bitsize(mtcsMode,op0_mode.require ());

      /* If op0 is a register, we need it in EXT_MODE to make it
     acceptable to the format of ext(z)v.  */
      if (GET_CODE (op0) == SUBREG && op0_mode.require () != ext_mode)
          return NULL_RTX;
      if (REG_P (op0) && op0_mode.require () != ext_mode)
          op0 = gen_lowpart_SUBREG (ext_mode, op0);
  }

  /* If BITS_BIG_ENDIAN is zero on a BYTES_BIG_ENDIAN machine, we count
     "backwards" from the size of the unit we are extracting from.
     Otherwise, we count bits from the most significant on a
     BYTES/BITS_BIG_ENDIAN machine.  */

  if (BITS_BIG_ENDIAN != BYTES_BIG_ENDIAN)
    bitnum = unit - bitsize - bitnum;

  if (target == 0)
    target = spec_target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,tmode);

  if (GET_MODE (target) != ext_mode){
      rtx temp;
      /* Don't use LHS paradoxical subreg if explicit truncation is needed
     between the mode of the extraction (word_mode) and the target
     mode.  Instead, create a temporary and use convert_move to set
     the target.  */
      if (REG_P (target)
        && mtcs_mode_truly_noop_truncation_p/*!TRULY_NOOP_TRUNCATION_MODES_P*/(mtcsMode,GET_MODE (target), ext_mode)
        && (temp = mtcs_rtl_gen_lowpart_if_possible/*!gen_lowpart_if_possible*/(mtcsRTL,ext_mode, target))){
          target = temp;
          if (mtcs_mode_partial_subreg_p/*!partial_subreg_p*/(mtcsMode,GET_MODE (spec_target), ext_mode))
            spec_target_subreg = target;
      }else
          target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,ext_mode);
  }

  create_output_operand (&ops[0], target, ext_mode);
  create_fixed_operand (&ops[1], op0);
  mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[2], bitsize);
  mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[3], bitnum);
  if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(mtcsOptabs,extv->icode, 4, ops)){
      target = ops[0].value;
      if (target == spec_target)
          return target;
      if (target == spec_target_subreg)
          return spec_target;
      return convert_extracted_bit_field(self,target, mode, tmode, unsignedp);
  }
  return NULL_RTX;
}

/* Use shifts and boolean operations to store VALUE into a bit field of
   width BITSIZE in OP0, starting at bit BITNUM.  If OP0_MODE is defined,
   it is the mode of OP0, otherwise OP0 is a BLKmode MEM.  VALUE_MODE is
   the mode of VALUE.

   If REVERSE is true, the store is to be done in reverse order.  */
//原型 store_fixed_bit_field expmed.cc
static void store_fixed_bit_field (MtcsExpmed *self,rtx op0, opt_scalar_int_mode op0_mode,
               unsigned HOST_WIDE_INT bitsize,
               unsigned HOST_WIDE_INT bitnum,
               poly_uint64 bitregion_start, poly_uint64 bitregion_end,
               rtx value, scalar_int_mode value_mode, bool reverse)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  /* There is a case not handled here:
     a structure with a known alignment of just a halfword
     and a field split across two aligned halfwords within the structure.
     Or likewise a structure with a known alignment of just a byte
     and a field split across two bytes.
     Such cases are not supposed to be able to occur.  */

  scalar_int_mode best_mode;
  if (MEM_P (op0)){
      unsigned int max_bitsize = BITS_PER_WORD;
      scalar_int_mode imode;
      if (op0_mode.exists (&imode) && mtcs_mode_get_bitsize(mtcsMode,imode) < max_bitsize)
          max_bitsize = mtcs_mode_get_bitsize(mtcsMode,imode);

      if (!mtcs_mode_get_best_mode/*!get_best_mode*/(mtcsMode,bitsize, bitnum, bitregion_start, bitregion_end,
              mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,op0), max_bitsize, MEM_VOLATILE_P (op0),
              &best_mode)){
          /* The only way this should occur is if the field spans word
             boundaries.  */
          store_split_bit_field(self,op0, op0_mode, bitsize, bitnum,
                     bitregion_start, bitregion_end,value, value_mode, reverse);
          return;
      }
      op0 = narrow_bit_field_mem(self,op0, best_mode, bitsize, bitnum, &bitnum);
  }else
    best_mode = op0_mode.require ();

  store_fixed_bit_field_1(self,op0, best_mode, bitsize, bitnum,
               value, value_mode, reverse);
}

/* Helper function for store_fixed_bit_field, stores
   the bit field always using MODE, which is the mode of OP0.  The other
   arguments are as for store_fixed_bit_field.  */
//原型 store_fixed_bit_field_1 expmed.cc
static void store_fixed_bit_field_1 (MtcsExpmed *self,rtx op0, scalar_int_mode mode,
             unsigned HOST_WIDE_INT bitsize,
             unsigned HOST_WIDE_INT bitnum,
             rtx value, scalar_int_mode value_mode, bool reverse)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

  rtx temp;
  int all_zero = 0;
  int all_one = 0;

  /* Note that bitsize + bitnum can be greater than GET_MODE_BITSIZE (mode)
     for invalid input, such as f5 from gcc.dg/pr48335-2.c.  */

  if (reverse ? !BYTES_BIG_ENDIAN : BYTES_BIG_ENDIAN)
    /* BITNUM is the distance between our msb
       and that of the containing datum.
       Convert it to the distance from the lsb.  */
    bitnum =mtcs_mode_get_bitsize(mtcsMode,mode) - bitsize - bitnum;

  /* Now BITNUM is always the distance between our lsb
     and that of OP0.  */

  /* Shift VALUE left by BITNUM bits.  If VALUE is not constant,
     we must first convert its mode to MODE.  */

  if (CONST_INT_P (value)){
      unsigned HOST_WIDE_INT v = UINTVAL (value);
      if (bitsize < HOST_BITS_PER_WIDE_INT)
          v &= (HOST_WIDE_INT_1U << bitsize) - 1;

      if (v == 0)
          all_zero = 1;
      else if ((bitsize < HOST_BITS_PER_WIDE_INT
        && v == (HOST_WIDE_INT_1U << bitsize) - 1)
           || (bitsize == HOST_BITS_PER_WIDE_INT
           && v == HOST_WIDE_INT_M1U))
          all_one = 1;

      value = lshift_value(self,mode, v, bitnum);
  }else{
      int must_and = (mtcs_mode_get_bitsize(mtcsMode,value_mode) != bitsize
              && bitnum + bitsize != mtcs_mode_get_bitsize(mtcsMode,mode));

      if (value_mode != mode)
          value =  mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,mode, value, 1);

      if (must_and)
        value =mtcs_optabs_expand_binop(mtcsOptabs,mode, and_optab, value,
                      mask_rtx(self,mode, 0, bitsize, 0), NULL_RTX, 1, OPTAB_LIB_WIDEN);
      if (bitnum > 0)
        value = mtcs_expmed_expand_shift(self,LSHIFT_EXPR, mode, value,bitnum, NULL_RTX, 1);
  }

  if (reverse)
    value =mtcs_expmed_flip_storage_order/*!flip_storage_order*/(self,mode, value);

  /* Now clear the chosen bits in OP0,
     except that if VALUE is -1 we need not bother.  */
  /* We keep the intermediates in registers to allow CSE to combine
     consecutive bitfield assignments.  */

  temp = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,mode, op0);

  if (! all_one){
      rtx mask = mask_rtx(self,mode, bitnum, bitsize, 1);
      if (reverse)
          mask = mtcs_expmed_flip_storage_order/*!flip_storage_order*/(self,mode, mask);
      temp = mtcs_optabs_expand_binop(mtcsOptabs,mode, and_optab, temp, mask,NULL_RTX, 1, OPTAB_LIB_WIDEN);
      temp = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,mode, temp);
  }

  /* Now logical-or VALUE into OP0, unless it is zero.  */

  if (! all_zero){
      temp = mtcs_optabs_expand_binop(mtcsOptabs,mode, ior_optab, temp, value,NULL_RTX, 1, OPTAB_LIB_WIDEN);
      temp = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,mode, temp);
  }

  if (op0 != temp){
      op0 = copy_rtx (op0);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,op0, temp);
  }
}
/* Subroutine of extract_bit_field_1, with the same arguments, except
   that BITSIZE and BITNUM are constant.  Handle cases specific to
   integral modes.  If OP0_MODE is defined, it is the mode of OP0,
   otherwise OP0 is a BLKmode MEM.  */
//原型 extract_integral_bit_field expmed.cc
static rtx extract_integral_bit_field (MtcsExpmed *self,rtx op0, opt_scalar_int_mode op0_mode,
                unsigned HOST_WIDE_INT bitsize,
                unsigned HOST_WIDE_INT bitnum, int unsignedp,
                rtx target, machine_mode mode, machine_mode tmode,
                bool reverse, bool fallback_p)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

  /* Handle fields bigger than a word.  */

  if (bitsize > BITS_PER_WORD){
      /* Here we transfer the words of the field
     in the order least significant first.
     This is because the most significant word is the one which may
     be less than full.  */

      const bool backwards = WORDS_BIG_ENDIAN;
      unsigned int nwords = (bitsize + (BITS_PER_WORD - 1)) / BITS_PER_WORD;
      unsigned int i;
      rtx_insn *last;

      if (target == 0 || !REG_P (target) || !mtcs_optabs_valid_multiword_target_p/*!valid_multiword_target_p*/(mtcsOptabs,target))
          target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

      /* In case we're about to clobber a base register or something
     (see gcc.c-torture/execute/20040625-1.c).   */
      if (reg_mentioned_p (target, op0))
          target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

      /* Indicate for flow that the entire target reg is being set.  */
      mtcs_emit_emit_clobber/*!emit_clobber*/(mtcsEmit,target);

      /* The mode must be fixed-size, since extract_bit_field_1 handles
     extractions from variable-sized objects before calling this
     function.  */
      unsigned int target_size = mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (target)).to_constant ();
      last = mtcs_rtl_data_get_last_insn (mtcsRtlData);
      for (i = 0; i < nwords; i++){
          /* If I is 0, use the low-order word in both field and target;
             if I is 1, use the next to lowest word; and so on.  */
          /* Word number in TARGET to use.  */
          unsigned int wordnum = (backwards ? target_size / UNITS_PER_WORD - i - 1 : i);
          /* Offset from start of field in OP0.  */
          unsigned int bit_offset = (backwards ^ reverse
                         ? MAX ((int) bitsize - ((int) i + 1)* BITS_PER_WORD,0) : (int) i * BITS_PER_WORD);
          rtx target_part =mtcs_rtl_operand_subword/*!operand_subword*/(mtcsRTL,target, wordnum, 1, VOIDmode);
          rtx result_part = extract_bit_field_1(self,op0, MIN (BITS_PER_WORD,bitsize - i * BITS_PER_WORD),
                       bitnum + bit_offset,(unsignedp ? 1 : -1), target_part,
                       mode, word_mode, reverse, fallback_p, NULL);

          gcc_assert (target_part);
          if (!result_part){
              mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
              return NULL;
          }

          if (result_part != target_part)
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target_part, result_part);
      }

      if (unsignedp){
          /* Unless we've filled TARGET, the upper regs in a multi-reg value
             need to be zero'd out.  */
          if (target_size > nwords * UNITS_PER_WORD){
              unsigned int i, total_words;

              total_words = target_size / UNITS_PER_WORD;
              for (i = nwords; i < total_words; i++)
                  mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mtcs_rtl_operand_subword/*!operand_subword*/(mtcsRTL,target,
                        backwards ? total_words - i - 1 : i, 1, VOIDmode),const0_rtx);
          }
          return target;
      }

      /* Signed bit field: sign-extend with two arithmetic shifts.  */
      target = mtcs_expmed_expand_shift(self,LSHIFT_EXPR, mode, target,
                 mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) - bitsize, NULL_RTX, 0);
      return mtcs_expmed_expand_shift(self,RSHIFT_EXPR, mode, target,
              mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) - bitsize, NULL_RTX, 0);
  }

  /* If OP0 is a multi-word register, narrow it to the affected word.
     If the region spans two words, defer to extract_split_bit_field.  */
  if (!MEM_P (op0) && mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,op0_mode.require ()) > UNITS_PER_WORD){
      if (bitnum % BITS_PER_WORD + bitsize > BITS_PER_WORD){
      if (!fallback_p)
        return NULL_RTX;
      target = extract_split_bit_field(self,op0, op0_mode, bitsize, bitnum,unsignedp, reverse);
      return convert_extracted_bit_field(self,target, mode, tmode, unsignedp);
    }
      /* If OP0 is a hard register, copy it to a pseudo before calling
     simplify_gen_subreg.  */
      if (REG_P (op0) && mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,op0))
          op0 = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,op0);
      op0 = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,word_mode, op0, op0_mode.require (),
                 bitnum / BITS_PER_WORD * UNITS_PER_WORD);
      op0_mode = word_mode;
      bitnum %= BITS_PER_WORD;
    }

  /* From here on we know the desired field is smaller than a word.
     If OP0 is a register, it too fits within a word.  */
  enum extraction_pattern pattern = unsignedp ? EP_extzv : EP_extv;
  extraction_insn extv;
  if (!MEM_P (op0)
      && !reverse
      /* ??? We could limit the structure size to the part of OP0 that
     contains the field, with appropriate checks for endianness
     and TARGET_TRULY_NOOP_TRUNCATION.  */
      && mtcs_optabs_get_best_reg_extraction_insn/*!get_best_reg_extraction_insn*/(mtcsOptabs,&extv, pattern,
                       mtcs_mode_get_bitsize(mtcsMode,op0_mode.require ()), tmode)){
          rtx result = extract_bit_field_using_extv(self,&extv, op0, op0_mode,
                             bitsize, bitnum, unsignedp, target, mode, tmode);
          if (result)
              return result;
  }

  /* If OP0 is a memory, try copying it to a register and seeing if a
     cheap register alternative is available.  */
  if (MEM_P (op0) & !reverse){
      if (mtcs_optabs_get_best_mem_extraction_insn/*!get_best_mem_extraction_insn*/(mtcsOptabs,&extv, pattern, bitsize, bitnum,tmode)){
              rtx result = extract_bit_field_using_extv(self,&extv, op0, op0_mode,bitsize, bitnum,unsignedp, target, mode,tmode);
          if (result)
            return result;
      }

      rtx_insn *last =mtcs_rtl_data_get_last_insn (mtcsRtlData);

      /* Try loading part of OP0 into a register and extracting the
     bitfield from that.  */
      unsigned HOST_WIDE_INT bitpos;
      rtx xop0 = adjust_bit_field_mem_for_reg(self,pattern, op0, bitsize, bitnum,0, 0, tmode, &bitpos);
      if (xop0){
          xop0 = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,xop0);
          rtx result = extract_bit_field_1(self,xop0, bitsize, bitpos,
                            unsignedp, target,mode, tmode, reverse, false, NULL);
          if (result)
            return result;
          mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
      }
  }

  if (!fallback_p)
    return NULL;

  /* Find a correspondingly-sized integer field, so we can apply
     shifts and masks to it.  */
  scalar_int_mode int_mode;
  if (!mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,tmode).exists (&int_mode))
    /* If this fails, we should probably push op0 out to memory and then
       do a load.  */
    int_mode = mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,mode).require ();

  target = extract_fixed_bit_field(self,int_mode, op0, op0_mode, bitsize,bitnum, target, unsignedp, reverse);
  /* Complex values must be reversed piecewise, so we need to undo the global
     reversal, convert to the complex mode and reverse again.  */
  if (reverse && mtcs_mode_is_complex_p/*!COMPLEX_MODE_P*/(mtcsMode,tmode))
    {
      target = mtcs_expmed_flip_storage_order/*!flip_storage_order*/(self,int_mode, target);
      target = convert_extracted_bit_field(self,target, mode, tmode, unsignedp);
      target = mtcs_expmed_flip_storage_order/*!flip_storage_order*/(self,tmode, target);
    }
  else
    target = convert_extracted_bit_field(self,target, mode, tmode, unsignedp);

  return target;
}

/* Helper function for extract_fixed_bit_field, extracts
   the bit field always using MODE, which is the mode of OP0.
   If UNSIGNEDP is -1, the result need not be sign or zero extended.
   The other arguments are as for extract_fixed_bit_field.  */
//原型 extract_fixed_bit_field_1 expmed.cc
static rtx extract_fixed_bit_field_1 (MtcsExpmed *self,machine_mode tmode, rtx op0, scalar_int_mode mode,
               unsigned HOST_WIDE_INT bitsize,unsigned HOST_WIDE_INT bitnum, rtx target,int unsignedp, bool reverse)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

  /* Note that bitsize + bitnum can be greater than GET_MODE_BITSIZE (mode)
     for invalid input, such as extract equivalent of f5 from
     gcc.dg/pr48335-2.c.  */

  if (reverse ? !BYTES_BIG_ENDIAN : BYTES_BIG_ENDIAN)
    /* BITNUM is the distance between our msb and that of OP0.
       Convert it to the distance from the lsb.  */
    bitnum = mtcs_mode_get_size/*!GET_MODE_BITSIZE*/(mtcsMode,mode) - bitsize - bitnum;

  /* Now BITNUM is always the distance between the field's lsb and that of OP0.
     We have reduced the big-endian case to the little-endian case.  */
  if (reverse)
    op0 = mtcs_expmed_flip_storage_order/*!flip_storage_order*/(self,mode, op0);

  if (unsignedp){
      if (bitnum){
          /* If the field does not already start at the lsb,
             shift it so it does.  */
          /* Maybe propagate the target for the shift.  */
          rtx subtarget = (target != 0 && REG_P (target) ? target : 0);
          if (tmode != mode)
            subtarget = 0;
          op0 = mtcs_expmed_expand_shift/*!expand_shift*/(self,RSHIFT_EXPR, mode, op0, bitnum, subtarget, 1);
      }
      /* Convert the value to the desired mode.  TMODE must also be a
     scalar integer for this conversion to make sense, since we
     shouldn't reinterpret the bits.  */
      scalar_int_mode new_mode = mtcs_mode_as_a <scalar_int_mode>(mtcsMode,tmode);
      if (mode != new_mode)
          op0 = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,new_mode, op0, 1);

      /* Unless the msb of the field used to be the msb when we shifted,
     mask out the upper bits.  */

      if (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) != bitnum + bitsize  && unsignedp != -1)
        return mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,new_mode, and_optab, op0,
                     mask_rtx(self,new_mode, 0, bitsize, 0), target, 1, OPTAB_LIB_WIDEN);
      return op0;
  }

  /* To extract a signed bit-field, first shift its msb to the msb of the word,
     then arithmetic-shift its lsb to the lsb of the word.  */
  op0 = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,mode, op0);

  /* Find the narrowest integer mode that contains the field.  */

  opt_scalar_int_mode mode_iter;
  MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,mode_iter, MODE_INT)
    if (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode_iter.require ()) >= bitsize + bitnum)
      break;

  mode = mode_iter.require ();
  op0 = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,mode, op0, 0);

  if (mode != tmode)
    target = 0;

  if (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) != (bitsize + bitnum)){
      int amount = mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) - (bitsize + bitnum);
      /* Maybe propagate the target for the shift.  */
      rtx subtarget = (target != 0 && REG_P (target) ? target : 0);
      op0 = mtcs_expmed_expand_shift/*!expand_shift*/(self,LSHIFT_EXPR, mode, op0, amount, subtarget, 1);
  }

  return mtcs_expmed_expand_shift/*!expand_shift*/(self,RSHIFT_EXPR, mode, op0,
          mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) - bitsize, target, 0);
}


/* Use shifts and boolean operations to extract a field of BITSIZE bits
   from bit BITNUM of OP0.  If OP0_MODE is defined, it is the mode of OP0,
   otherwise OP0 is a BLKmode MEM.

   UNSIGNEDP is nonzero for an unsigned bit field (don't sign-extend value).
   If REVERSE is true, the extraction is to be done in reverse order.

   If TARGET is nonzero, attempts to store the value there
   and return TARGET, but this is not guaranteed.
   If TARGET is not used, create a pseudo-reg of mode TMODE for the value.  */
//原型 extract_fixed_bit_field expmed.cc
static rtx extract_fixed_bit_field (MtcsExpmed *self,machine_mode tmode, rtx op0,
             opt_scalar_int_mode op0_mode,
             unsigned HOST_WIDE_INT bitsize,
             unsigned HOST_WIDE_INT bitnum, rtx target,
             int unsignedp, bool reverse)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  scalar_int_mode mode;
  if (MEM_P (op0)){
      if (!mtcs_mode_get_best_mode/*!get_best_mode*/(mtcsMode,bitsize, bitnum, 0, 0,
              mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,op0),BITS_PER_WORD, MEM_VOLATILE_P (op0), &mode))
        /* The only way this should occur is if the field spans word
           boundaries.  */
        return extract_split_bit_field(self,op0, op0_mode, bitsize, bitnum,unsignedp, reverse);
      op0 = narrow_bit_field_mem(self,op0, mode, bitsize, bitnum, &bitnum);
  }else
    mode = op0_mode.require ();

  return extract_fixed_bit_field_1(self,tmode, op0, mode, bitsize, bitnum,target, unsignedp, reverse);
}


/* A subroutine of extract_bit_field, with the same arguments.
   If UNSIGNEDP is -1, the result need not be sign or zero extended.
   If FALLBACK_P is true, fall back to extract_fixed_bit_field
   if we can find no other means of implementing the operation.
   if FALLBACK_P is false, return NULL instead.  */
//原型 extract_bit_field_1 expmed.cc
static rtx extract_bit_field_1 (MtcsExpmed *self,rtx str_rtx, poly_uint64 bitsize, poly_uint64 bitnum,
             int unsignedp, rtx target, machine_mode mode, machine_mode tmode, bool reverse, bool fallback_p,rtx *alt_rtl)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  rtx op0 = str_rtx;
  machine_mode mode1;
  if (tmode == VOIDmode)
    tmode = mode;
  while (GET_CODE (op0) == SUBREG){
      bitnum += SUBREG_BYTE (op0) * BITS_PER_UNIT;
      op0 = SUBREG_REG (op0);
  }
  /* If we have an out-of-bounds access to a register, just return an
     uninitialized register of the required mode.  This can occur if the
     source code contains an out-of-bounds access to a small array.  */
  if (REG_P (op0) && known_ge (bitnum,mtcs_mode_get_bitsize/*GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (op0))))
    return mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,tmode);

  if (REG_P (op0)  && mode == GET_MODE (op0) && known_eq (bitnum, 0U)
      && known_eq (bitsize, mtcs_mode_get_bitsize/*GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (op0)))){
      if (reverse)
          op0 = mtcs_expmed_flip_storage_order/*!flip_storage_order*/(self,mode, op0);
      /* We're trying to extract a full register from itself.  */
      return op0;
  }
  /* First try to check for vector from vector extractions.  */
  if (mtcs_mode_is_vector_p(mtcsMode,GET_MODE (op0))
      && !MEM_P (op0)
      && mtcs_mode_is_vector_p(mtcsMode,tmode)
      && known_eq (bitsize, mtcs_mode_get_precision(mtcsMode,tmode))
      && maybe_gt (mtcs_mode_get_size(mtcsMode,GET_MODE (op0)), mtcs_mode_get_size(mtcsMode,tmode))){
      machine_mode new_mode = GET_MODE (op0);
      if (mtcs_mode_get_inner(mtcsMode,new_mode) != mtcs_mode_get_inner(mtcsMode,tmode)){
          scalar_mode inner_mode = mtcs_mode_get_inner(mtcsMode,tmode);
          poly_uint64 nunits;
          if (!multiple_p (mtcs_mode_get_bitsize/*GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (op0)),
                   mtcs_mode_get_unit_bitsize/*GET_MODE_UNIT_BITSIZE*/(mtcsMode,tmode), &nunits)
              || !mtcs_mode_related_vector_mode/*!related_vector_mode*/(mtcsMode,tmode, inner_mode,nunits).exists (&new_mode)
              || maybe_ne (mtcs_mode_get_size(mtcsMode,new_mode),mtcs_mode_get_size(mtcsMode,GET_MODE (op0))))
            new_mode = VOIDmode;
      }
      poly_uint64 pos;
      if (new_mode != VOIDmode
         && (mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,vec_extract_optab, new_mode, tmode)
          != CODE_FOR_nothing)
         && multiple_p (bitnum, mtcs_mode_get_bitsize/*GET_MODE_BITSIZE*/(mtcsMode,tmode), &pos)){
          class expand_operand ops[3];
          machine_mode outermode = new_mode;
          machine_mode innermode = tmode;
          enum insn_code icode= mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,vec_extract_optab, outermode, innermode);

          if (new_mode != GET_MODE (op0))
            op0 = gen_lowpart (new_mode, op0);
          create_output_operand (&ops[0], target, innermode);
          ops[0].target = 1;
          create_input_operand (&ops[1], op0, outermode);
          mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[2], pos);
          if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(mtcsOptabs,icode, 3, ops)){
              if (alt_rtl && ops[0].target)
                  *alt_rtl = target;
              target = ops[0].value;
              if (GET_MODE (target) != mode)
                  return gen_lowpart (tmode, target);
              return target;
          }
      }
  }
  /* See if we can get a better vector mode before extracting.  */
  if (mtcs_mode_is_vector_p(mtcsMode,GET_MODE (op0))
      && !MEM_P (op0) && mtcs_mode_get_inner(mtcsMode,GET_MODE (op0)) != tmode){
      machine_mode new_mode;
      unsigned char cl=mtcs_mode_get_class(mtcsMode,tmode);
      if (cl == MODE_FLOAT)
          new_mode = mtcsMode->modesMinMax.min_VECTOR_FLOAT/*!MIN_MODE_VECTOR_FLOAT*/;
      else if (cl== MODE_FRACT)
          new_mode = mtcsMode->modesMinMax.min_VECTOR_FRACT/*!MIN_MODE_VECTOR_FRACT*/;
      else if (cl == MODE_UFRACT)
          new_mode = mtcsMode->modesMinMax.min_VECTOR_UFRACT/*!MIN_MODE_VECTOR_UFRACT*/;
      else if (cl == MODE_ACCUM)
          new_mode = mtcsMode->modesMinMax.min_VECTOR_ACCUM/*!MIN_MODE_VECTOR_ACCUM*/;
      else if (cl == MODE_UACCUM)
          new_mode = mtcsMode->modesMinMax.min_VECTOR_UACCUM/*!MIN_MODE_VECTOR_UACCUM*/;
      else
          new_mode = mtcsMode->modesMinMax.min_VECTOR_INT/*!MIN_MODE_VECTOR_INT*/;

      MTCS_FOR_EACH_MODE_FROM (mtcsMode,new_mode, new_mode)
         if (known_eq (mtcs_mode_get_size(mtcsMode,new_mode), mtcs_mode_get_size(mtcsMode,GET_MODE (op0)))
            && known_eq (mtcs_mode_get_unit_size(mtcsMode,new_mode), mtcs_mode_get_size(mtcsMode,tmode))
            && known_eq (bitsize, mtcs_mode_get_unit_precision/*!GET_MODE_UNIT_PRECISION*/(mtcsMode,new_mode))
            && multiple_p (bitnum, mtcs_mode_get_unit_precision/*!GET_MODE_UNIT_PRECISION*/(mtcsMode,new_mode))
            && mtcsTarget->/*!targetm.vector_mode_supported_p*/vector_mode_supported_p(mtcsTarget,new_mode)
            && mtcsTarget->/*!targetm.modes_tieable_p*/modes_tieable_p(mtcsTarget,GET_MODE (op0), new_mode))
              break;
      if (new_mode != VOIDmode)
        op0 = gen_lowpart (new_mode, op0);
  }
  /* Use vec_extract patterns for extracting parts of vectors whenever
     available.  If that fails, see whether the current modes and bitregion
     give a natural subreg.  */
  machine_mode outermode = GET_MODE (op0);
  if (mtcs_mode_is_vector_p(mtcsMode,outermode) && !MEM_P (op0)){
      scalar_mode innermode = mtcs_mode_get_inner(mtcsMode,outermode);

      enum insn_code icode =mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,vec_extract_optab, outermode, innermode);

      poly_uint64 pos;
      if (icode != CODE_FOR_nothing  && known_eq (bitsize, mtcs_mode_get_precision(mtcsMode,innermode))
          && multiple_p (bitnum, mtcs_mode_get_precision(mtcsMode,innermode), &pos)){
          class expand_operand ops[3];
          create_output_operand (&ops[0], target,mtcsOutput->insn_data/*!insn_data*/[icode].operand[0].mode);
          ops[0].target = 1;
          create_input_operand (&ops[1], op0, outermode);
          mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[2], pos);
          if (maybe_expand_insn (icode, 3, ops)){
              if (alt_rtl && ops[0].target)
                  *alt_rtl = target;
              target = ops[0].value;
              if (GET_MODE (target) != mode)
                  return gen_lowpart (tmode, target);
              return target;
          }
      }
      /* Using subregs is useful if we're extracting one register vector
     from a multi-register vector.  extract_bit_field_as_subreg checks
     for valid bitsize and bitnum, so we don't need to do that here.  */
      if (mtcs_mode_is_vector_p(mtcsMode,mode)){
          rtx sub = extract_bit_field_as_subreg(self,mode, op0, outermode,bitsize, bitnum);
          if (sub)
            return sub;
      }
  }
  /* Make sure we are playing with integral modes.  Pun with subregs
     if we aren't.  */
  opt_scalar_int_mode op0_mode = mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,GET_MODE (op0));
  scalar_int_mode imode;
  if (!op0_mode.exists (&imode) || imode != GET_MODE (op0)){
      if (MEM_P (op0))
          op0 = mtcs_rtl_adjust_bitfield_address_size/*!adjust_bitfield_address_size*/(mtcsRTL,op0,
                  op0_mode.else_blk (), 0,mtcs_rtl_get_mem_size/*!MEM_SIZE*/(mtcsRTL,op0));
      else if (op0_mode.exists (&imode)){
          op0 = gen_lowpart (imode, op0);

          /* If we got a SUBREG, force it into a register since we
             aren't going to be able to do another SUBREG on it.  */
          if (GET_CODE (op0) == SUBREG)
            op0 =  mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,imode, op0);
      }else{
          poly_int64 size = mtcs_mode_get_size(mtcsMode,GET_MODE (op0));
          rtx mem = mtcs_func_assign_stack_temp/*!assign_stack_temp*/(mtcsFunc,GET_MODE (op0), size);
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mem, op0);
          op0 = mtcs_rtl_adjust_bitfield_address_size/*!adjust_bitfield_address_size*/(mtcsRTL,mem, mtcsMode->modes.M_BLKmode, 0, size);
      }
  }
  /* ??? We currently assume TARGET is at least as big as BITSIZE.
     If that's wrong, the solution is to test for it and set TARGET to 0
     if needed.  */

  /* Get the mode of the field to use for atomic access or subreg
     conversion.  */
  if (!mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,tmode)
      || !mtcs_mode_mode_for_size/*!mode_for_size*/(mtcsMode,bitsize, mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,tmode), 0).exists (&mode1))
    mode1 = mode;
  gcc_assert (mode1 != mtcsMode->modes.M_BLKmode);

  /* Extraction of a full MODE1 value can be done with a subreg as long
     as the least significant bit of the value is the least significant
     bit of either OP0 or a word of OP0.  */
  if (!MEM_P (op0) && !reverse && op0_mode.exists (&imode)){
      rtx sub = extract_bit_field_as_subreg(self,mode1, op0, imode, bitsize, bitnum);
      if (sub)
          return convert_extracted_bit_field(self,sub, mode, tmode, unsignedp);
  }

  /* Extraction of a full MODE1 value can be done with a load as long as
     the field is on a byte boundary and is sufficiently aligned.  */
  poly_uint64 bytenum;
  if (simple_mem_bitfield_p(self,op0, bitsize, bitnum, mode1, &bytenum)){
      op0 = mtcs_rtl_adjust_bitfield_address/*!adjust_bitfield_address*/(mtcsRTL,op0, mode1, bytenum);
      if (reverse)
          op0 = mtcs_expmed_flip_storage_order/*!flip_storage_order*/(self,mode1, op0);
      return convert_extracted_bit_field(self,op0, mode, tmode, unsignedp);
  }

  /* If we have a memory source and a non-constant bit offset, restrict
     the memory to the referenced bytes.  This is a worst-case fallback
     but is useful for things like vector booleans.  */
  if (MEM_P (op0) && !bitnum.is_constant ()){
      bytenum = bits_to_bytes_round_down (bitnum);
      bitnum = num_trailing_bits (bitnum);
      poly_uint64 bytesize = bits_to_bytes_round_up (bitnum + bitsize);
      op0 = mtcs_rtl_adjust_bitfield_address_size/*!adjust_bitfield_address_size*/(mtcsRTL,op0, mtcsMode->modes.M_BLKmode, bytenum, bytesize);
      op0_mode = opt_scalar_int_mode ();
  }
  /* It's possible we'll need to handle other cases here for
     polynomial bitnum and bitsize.  */
  /* From here on we need to be looking at a fixed-size insertion.  */
  return extract_integral_bit_field(self,op0, op0_mode, bitsize.to_constant (),
                     bitnum.to_constant (), unsignedp,
                     target, mode, tmode, reverse, fallback_p);
}

/* A subroutine of extract_bit_field_1 that converts return value X
   to either MODE or TMODE.  MODE, TMODE and UNSIGNEDP are arguments
   to extract_bit_field.  */
//原型 convert_extracted_bit_field expmed.cc
static rtx convert_extracted_bit_field (MtcsExpmed *self,rtx x, machine_mode mode, machine_mode tmode, bool unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  if (GET_MODE (x) == tmode || GET_MODE (x) == mode)
    return x;

  /* If the x mode is not a scalar integral, first convert to the
     integer mode of that size and then access it as a floating-point
     value via a SUBREG.  */
  if (!mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,tmode)){
      scalar_int_mode int_mode = mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,tmode).require ();
      x = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,int_mode, x, unsignedp);
      x = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,int_mode, x);
      return gen_lowpart (tmode, x);
  }
  return mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,tmode, x, unsignedp);
}

/* If MODE is set, adjust bitfield memory MEM so that it points to the
   first unit of mode MODE that contains a bitfield of size BITSIZE at
   bit position BITNUM.  If MODE is not set, return a BLKmode reference
   to every byte in the bitfield.  Set *NEW_BITNUM to the bit position
   of the field within the new memory.  */
//原型 narrow_bit_field_mem expmed.cc
static rtx narrow_bit_field_mem (MtcsExpmed *self,rtx mem, opt_scalar_int_mode mode,
              unsigned HOST_WIDE_INT bitsize,
              unsigned HOST_WIDE_INT bitnum,
              unsigned HOST_WIDE_INT *new_bitnum)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  scalar_int_mode imode;
  if (mode.exists (&imode)){
      unsigned int unit = mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,imode);
      *new_bitnum = bitnum % unit;
      HOST_WIDE_INT offset = (bitnum - *new_bitnum) / BITS_PER_UNIT;
      return mtcs_rtl_adjust_bitfield_address/*!adjust_bitfield_address*/(mtcsRTL,mem, imode, offset);
  }else{
      *new_bitnum = bitnum % BITS_PER_UNIT;
      HOST_WIDE_INT offset = bitnum / BITS_PER_UNIT;
      HOST_WIDE_INT size = ((*new_bitnum + bitsize + BITS_PER_UNIT - 1) / BITS_PER_UNIT);
      return mtcs_rtl_adjust_bitfield_address_size/*!adjust_bitfield_address_size*/(mtcsRTL,mem, mtcsMode->modes.M_BLKmode, offset, size);
    }
}

/* Return true if OP is a memory and if a bitfield of size BITSIZE at
   bit number BITNUM can be treated as a simple value of mode MODE.
   Store the byte offset in *BYTENUM if so.  */
//原型 simple_mem_bitfield_p expmed.cc
static bool simple_mem_bitfield_p (MtcsExpmed *self,rtx op0, poly_uint64 bitsize, poly_uint64 bitnum,
               machine_mode mode, poly_uint64 *bytenum)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  return (MEM_P (op0)
      && multiple_p (bitnum, BITS_PER_UNIT, bytenum)
      && known_eq (bitsize, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode))
      && (!mtcsTarget/*!targetm.slow_unaligned_access*/->slow_unaligned_access(mtcsTarget,mode, mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,op0))
          || (multiple_p (bitnum, mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode))
          && mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,op0) >= mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode))));
}

/* A subroutine of store_bit_field, with the same arguments.  Return true
   if the operation could be implemented.

   If FALLBACK_P is true, fall back to store_fixed_bit_field if we have
   no other way of implementing the operation.  If FALLBACK_P is false,
   return false instead.

   if UNDEFINED_P is true then STR_RTX is undefined and may be set using
   a subreg instead.  */
//原型 store_bit_field_1 expmed.cc
static bool store_bit_field_1 (MtcsExpmed *self,rtx str_rtx, poly_uint64 bitsize, poly_uint64 bitnum,
           poly_uint64 bitregion_start, poly_uint64 bitregion_end,
           machine_mode fieldmode, rtx value, bool reverse, bool fallback_p, bool undefined_p)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

  rtx op0 = str_rtx;
  while (GET_CODE (op0) == SUBREG){
      bitnum += mtcs_rtl_subreg_memory_offset_with_rtx/*!subreg_memory_offset*/(mtcsRTL,op0) * BITS_PER_UNIT;
      op0 = SUBREG_REG (op0);
  }

  /* No action is needed if the target is a register and if the field
     lies completely outside that register.  This can occur if the source
     code contains an out-of-bounds access to a small array.  */
  if (REG_P (op0) && known_ge (bitnum, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (op0))))
    return true;

  /* Use vec_set patterns for inserting parts of vectors whenever
     available.  */
  machine_mode outermode = GET_MODE (op0);
  scalar_mode innermode = mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,outermode);
  poly_uint64 pos;
  if (mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,outermode)
      && !MEM_P (op0)
      && mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,vec_set_optab, outermode) != CODE_FOR_nothing
      && fieldmode == innermode
      && known_eq (bitsize, mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,innermode))
      && multiple_p (bitnum, mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,innermode), &pos)){
      class expand_operand ops[3];
      enum insn_code icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,vec_set_optab, outermode);

      create_fixed_operand (&ops[0], op0);
      create_input_operand (&ops[1], value, innermode);
      mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[2], pos);
      if (maybe_expand_insn (icode, 3, ops))
          return true;
  }

  /* If the target is a register, overwriting the entire object, or storing
     a full-word or multi-word field can be done with just a SUBREG.  */
  if (!MEM_P (op0)  && known_eq (bitsize, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,fieldmode))){
      /* Use the subreg machinery either to narrow OP0 to the required
     words or to cope with mode punning between equal-sized modes.
     In the latter case, use subreg on the rhs side, not lhs.  */
      rtx sub;
      poly_uint64 bytenum;
      poly_uint64 regsize = mtcs_mode_get_regmode_natural_size/*REGMODE_NATURAL_SIZE*/(mtcsMode,GET_MODE (op0));
      if (known_eq (bitnum, 0U) && known_eq (bitsize, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (op0)))){
          sub = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,GET_MODE (op0), value, fieldmode, 0);
          if (sub){
              if (reverse)
                  sub =mtcs_expmed_flip_storage_order/*!flip_storage_order*/(self,GET_MODE (op0), sub);
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,op0, sub);
              return true;
          }
      }else if (multiple_p (bitnum, BITS_PER_UNIT, &bytenum)
           && (undefined_p
           || (multiple_p (bitnum, regsize * BITS_PER_UNIT)
               && multiple_p (bitsize, regsize * BITS_PER_UNIT)))
           && known_ge (mtcs_mode_get_bitsize/*GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (op0)), bitsize)){
          sub = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,fieldmode, op0, GET_MODE (op0), bytenum);
          if (sub){
              if (reverse)
                  value =mtcs_expmed_flip_storage_order/*!flip_storage_order*/(self,fieldmode, value);
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,sub, value);
              return true;
          }
      }
  }

  /* If the target is memory, storing any naturally aligned field can be
     done with a simple store.  For targets that support fast unaligned
     memory, any naturally sized, unit aligned field can be done directly.  */
  poly_uint64 bytenum;
  if (simple_mem_bitfield_p(self,op0, bitsize, bitnum, fieldmode, &bytenum)){
      op0 = mtcs_rtl_adjust_bitfield_address/*!adjust_bitfield_address*/(mtcsRTL,op0, fieldmode, bytenum);
      if (reverse)
          value = mtcs_expmed_flip_storage_order/*!flip_storage_order*/(self,fieldmode, value);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,op0, value);
      return true;
  }

  /* It's possible we'll need to handle other cases here for
     polynomial bitnum and bitsize.  */

  /* From here on we need to be looking at a fixed-size insertion.  */
  unsigned HOST_WIDE_INT ibitsize = bitsize.to_constant ();
  unsigned HOST_WIDE_INT ibitnum = bitnum.to_constant ();

  /* Make sure we are playing with integral modes.  Pun with subregs
     if we aren't.  This must come after the entire register case above,
     since that case is valid for any mode.  The following cases are only
     valid for integral modes.  */
  opt_scalar_int_mode op0_mode = mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,GET_MODE (op0));
  scalar_int_mode imode;
  if (!op0_mode.exists (&imode) || imode != GET_MODE (op0)){
      if (MEM_P (op0))
          op0 = mtcs_rtl_adjust_bitfield_address_size/*!adjust_bitfield_address_size*/(mtcsRTL,
                op0, op0_mode.else_blk (), 0, mtcs_rtl_get_mem_size/*!MEM_SIZE*/(mtcsRTL,op0));
      else if (!op0_mode.exists ()){
          if (ibitnum == 0 && known_eq (ibitsize, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (op0)))
              && MEM_P (value) && !reverse){
              value = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,value, GET_MODE (op0), 0);
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,op0, value);
              return true;
          }
          if (!fallback_p)
            return false;
          rtx temp = mtcs_func_assign_stack_temp/*!assign_stack_temp*/(mtcsFunc,GET_MODE (op0),
                        mtcs_mode_get_size(mtcsMode,GET_MODE (op0)));
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,temp, op0);
          store_bit_field_1(self,temp, bitsize, bitnum, 0, 0, fieldmode, value,reverse, fallback_p, undefined_p);
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,op0, temp);
          return true;
      }else
          op0 = gen_lowpart (op0_mode.require (), op0);
  }

  return store_integral_bit_field (self,op0, op0_mode, ibitsize, ibitnum,
                   bitregion_start, bitregion_end, fieldmode, value, reverse, fallback_p);
}


/* Expand signed modulus of OP0 by a power of two D in mode MODE.  */
//原型 expand_smod_pow2 expmed.cc
static rtx expand_smod_pow2 (MtcsExpmed *self,scalar_int_mode mode, rtx op0, HOST_WIDE_INT d)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  rtx result, temp, shift;
  rtx_code_label *label;
  int logd;
  int prec = mtcs_mode_get_precision(mtcsMode,mode);

  logd = floor_log2 (d);
  result = mtcs_emit_gen_reg_rtx(mtcsEmit,mode);

  /* Avoid conditional branches when they're expensive.  */
  if (BRANCH_COST (optimize_insn_for_speed_p (), false) >= 2 && optimize_insn_for_speed_p ()){
      rtx signmask = mtcs_expmed_emit_store_flag(self,result, LT, op0, const0_rtx,mode, 0, -1);
      if (signmask){
          HOST_WIDE_INT masklow = (HOST_WIDE_INT_1 << logd) - 1;
          signmask = mtcs_explow_force_reg(mtcsExplow,mode, signmask);
          shift = mtcs_rtl_gen_int_shift_amount(mtcsRTL,mode, mtcs_mode_get_bitsize/*GET_MODE_BITSIZE*/(mtcsMode,mode) - logd);

          /* Use the rtx_cost of a LSHIFTRT instruction to determine
             which instruction sequence to use.  If logical right shifts
             are expensive the use 2 XORs, 2 SUBs and an AND, otherwise
             use a LSHIFTRT, 1 ADD, 1 SUB and an AND.  */
              temp = gen_rtx_LSHIFTRT (mode, result, shift);
          if (mtcs_opinit_optab_handler (mtcsOpinit,lshr_optab, mode) == CODE_FOR_nothing
              || (mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,temp, mode, optimize_insn_for_speed_p ())> COSTS_N_INSNS (2))){
              temp = mtcs_optabs_expand_binop(mtcsOptabs,mode, xor_optab, op0, signmask,NULL_RTX, 1, OPTAB_LIB_WIDEN);
              temp = mtcs_optabs_expand_binop(mtcsOptabs,mode, sub_optab, temp, signmask,NULL_RTX, 1, OPTAB_LIB_WIDEN);
              temp = mtcs_optabs_expand_binop(mtcsOptabs,mode, and_optab, temp,
                      mtcs_rtl_gen_int_mode(mtcsRTL,masklow, mode),NULL_RTX, 1, OPTAB_LIB_WIDEN);
              temp = mtcs_optabs_expand_binop(mtcsOptabs,mode, xor_optab, temp, signmask, NULL_RTX, 1, OPTAB_LIB_WIDEN);
              temp = mtcs_optabs_expand_binop(mtcsOptabs,mode, sub_optab, temp, signmask,NULL_RTX, 1, OPTAB_LIB_WIDEN);
          }else{
              signmask = mtcs_optabs_expand_binop(mtcsOptabs,mode, lshr_optab, signmask, shift,NULL_RTX, 1, OPTAB_LIB_WIDEN);
              signmask = mtcs_explow_force_reg(mtcsExplow,mode, signmask);

              temp = mtcs_optabs_expand_binop(mtcsOptabs,mode, add_optab, op0, signmask,NULL_RTX, 1, OPTAB_LIB_WIDEN);
              temp = mtcs_optabs_expand_binop(mtcsOptabs,mode, and_optab, temp,
                      mtcs_rtl_gen_int_mode(mtcsRTL,masklow, mode),NULL_RTX, 1, OPTAB_LIB_WIDEN);
              temp = mtcs_optabs_expand_binop(mtcsOptabs,mode, sub_optab, temp, signmask, NULL_RTX, 1, OPTAB_LIB_WIDEN);
          }
          return temp;
      }
  }

  /* Mask contains the mode's signbit and the significant bits of the
     modulus.  By including the signbit in the operation, many targets
     can avoid an explicit compare operation in the following comparison
     against zero.  */
  wide_int mask = wi::mask (logd, false, prec);
  mask = wi::set_bit (mask, prec - 1);

  temp = mtcs_optabs_expand_binop(mtcsOptabs,mode, and_optab, op0,
          mtcs_rtl_immed_wide_int_const (mtcsRTL,mask, mode),result, 1, OPTAB_LIB_WIDEN);
  if (temp != result)
      mtcs_expr_emit_move_insn(mtcsExpr,result, temp);

  label =mtcs_rtl_gen_label_rtx(mtcsRTL);
  do_cmp_and_jump (self,result, const0_rtx, GE, mode, label);

  temp = mtcs_optabs_expand_binop(mtcsOptabs,mode, sub_optab, result, const1_rtx, result,0, OPTAB_LIB_WIDEN);

  mask = wi::mask (logd, true, prec);
  temp = mtcs_optabs_expand_binop(mtcsOptabs,mode, ior_optab, temp,
          mtcs_rtl_immed_wide_int_const (mtcsRTL,mask, mode),result, 1, OPTAB_LIB_WIDEN);
  temp = mtcs_optabs_expand_binop(mtcsOptabs,mode, add_optab, temp, const1_rtx, result,0, OPTAB_LIB_WIDEN);
  if (temp != result)
      mtcs_expr_emit_move_insn(mtcsExpr,result, temp);
  mtcs_emit_emit_label(mtcsEmit,label);
  return result;
}

/* Expand signed division of OP0 by a power of two D in mode MODE.
   This routine is only called for positive values of D.  */
//原型 expand_sdiv_pow2 exmped.cc
static rtx expand_sdiv_pow2 (MtcsExpmed *self,scalar_int_mode mode, rtx op0, HOST_WIDE_INT d)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  rtx temp;
  rtx_code_label *label;
  int logd;

  logd = floor_log2 (d);

  if (d == 2  && BRANCH_COST (optimize_insn_for_speed_p (),false) >= 1){
      temp = mtcs_emit_gen_reg_rtx(mtcsEmit,mode);
      temp = mtcs_expmed_emit_store_flag(self,temp, LT, op0, const0_rtx, mode, 0, 1);
      if (temp != NULL_RTX){
          temp = mtcs_optabs_expand_binop(mtcsOptabs,mode, add_optab, temp, op0, NULL_RTX, 0, OPTAB_LIB_WIDEN);
          return mtcs_expmed_expand_shift(self,RSHIFT_EXPR, mode, temp, logd, NULL_RTX, 0);
      }
  }

  if (HAVE_conditional_move && BRANCH_COST (optimize_insn_for_speed_p (), false) >= 2){
      rtx temp2;

      mtcs_emit_start_sequence (mtcsEmit);
      temp2 = mtcs_explow_copy_to_mode_reg(mtcsExplow,mode, op0);
      temp = mtcs_optabs_expand_binop(mtcsOptabs,mode, add_optab, temp2, mtcs_rtl_gen_int_mode(mtcsRTL,d - 1, mode),
               NULL_RTX, 0, OPTAB_LIB_WIDEN);
      temp = mtcs_explow_force_reg(mtcsExplow,mode, temp);

      /* Construct "temp2 = (temp2 < 0) ? temp : temp2".  */
      temp2 = emit_conditional_move (temp2, { LT, temp2, const0_rtx, mode },
                     temp, temp2, mode, 0);
      if (temp2){
          rtx_insn *seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
          mtcs_emit_end_sequence (mtcsEmit);
          mtcs_emit_emit_insn (mtcsEmit,seq);
          return mtcs_expmed_expand_shift(self,RSHIFT_EXPR, mode, temp2, logd, NULL_RTX, 0);
      }
      mtcs_emit_end_sequence (mtcsEmit);
  }

  if (BRANCH_COST (optimize_insn_for_speed_p (), false) >= 2){
      int ushift = mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) - logd;

      temp = mtcs_emit_gen_reg_rtx(mtcsEmit,mode);
      temp = mtcs_expmed_emit_store_flag(self,temp, LT, op0, const0_rtx, mode, 0, -1);
      if (temp != NULL_RTX){
      if (mtcs_mode_get_bitsize (mtcsMode,mode) >= BITS_PER_WORD
          || mtcs_expmed_shift_cost/*!shift_cost*/(self,optimize_insn_for_speed_p (), mode, ushift)
          > COSTS_N_INSNS (1))
        temp = mtcs_optabs_expand_binop(mtcsOptabs,mode, and_optab, temp,
                 mtcs_rtl_gen_int_mode(mtcsRTL,d - 1, mode),
                 NULL_RTX, 0, OPTAB_LIB_WIDEN);
      else
        temp = mtcs_expmed_expand_shift(self,RSHIFT_EXPR, mode, temp,
                 ushift, NULL_RTX, 1);
      temp = mtcs_optabs_expand_binop(mtcsOptabs,mode, add_optab, temp, op0, NULL_RTX,
                   0, OPTAB_LIB_WIDEN);
      return mtcs_expmed_expand_shift(self,RSHIFT_EXPR, mode, temp, logd, NULL_RTX, 0);
    }
    }

  label =mtcs_rtl_gen_label_rtx(mtcsRTL);
  temp = mtcs_explow_copy_to_mode_reg(mtcsExplow,mode, op0);
  do_cmp_and_jump (self,temp, const0_rtx, GE, mode, label);
  mtcs_expmed_expand_inc (self,temp, mtcs_rtl_gen_int_mode(mtcsRTL,d - 1, mode));
  mtcs_emit_emit_label(mtcsEmit,label);
  return mtcs_expmed_expand_shift(self,RSHIFT_EXPR, mode, temp, logd, NULL_RTX, 0);
}

/* Compute and return the best algorithm for multiplying by T.
   The algorithm must cost less than cost_limit
   If retval.cost >= COST_LIMIT, no algorithm was found and all
   other field of the returned struct are undefined.
   MODE is the machine mode of the multiplication.  */
//原型 synth_mult expmed.cc
static void synth_mult (MtcsExpmed *self,struct algorithm *alg_out, unsigned HOST_WIDE_INT t,
        const struct mult_cost *cost_limit, machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

  int m;
  struct algorithm *alg_in, *best_alg;
  struct mult_cost best_cost;
  struct mult_cost new_limit;
  int op_cost, op_latency;
  unsigned HOST_WIDE_INT orig_t = t;
  unsigned HOST_WIDE_INT q;
  int maxm, hash_index;
  bool cache_hit = false;
  enum alg_code cache_alg = alg_zero;
  bool speed = optimize_insn_for_speed_p ();
  scalar_int_mode imode;
  struct alg_hash_entry *entry_ptr;

  /* Indicate that no algorithm is yet found.  If no algorithm
     is found, this value will be returned and indicate failure.  */
  alg_out->cost.cost = cost_limit->cost + 1;
  alg_out->cost.latency = cost_limit->latency + 1;

  if (cost_limit->cost < 0 || (cost_limit->cost == 0 && cost_limit->latency <= 0))
    return;

  /* Be prepared for vector modes.  */
  imode = mtcs_mode_as_a <scalar_int_mode>(mtcsMode,(machine_mode)mtcs_mode_get_inner/*!GET_MODE_INNER*/ (mtcsMode,mode));

  maxm = MIN (BITS_PER_WORD, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,imode));

  /* Restrict the bits of "t" to the multiplication's mode.  */
  t &= mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,imode);

  /* t == 1 can be done in zero cost.  */
  if (t == 1){
      alg_out->ops = 1;
      alg_out->cost.cost = 0;
      alg_out->cost.latency = 0;
      alg_out->op[0] = alg_m;
      return;
  }

  /* t == 0 sometimes has a cost.  If it does and it exceeds our limit,
     fail now.  */
  if (t == 0){
      if (MULT_COST_LESS (cost_limit, mtcs_expmed_zero_cost/*!zero_cost*/(self,speed)))
          return;
      else{
          alg_out->ops = 1;
          alg_out->cost.cost = mtcs_expmed_zero_cost/*!zero_cost*/(self,speed);
          alg_out->cost.latency = mtcs_expmed_zero_cost/*!zero_cost*/(self,speed);
          alg_out->op[0] = alg_zero;
          return;
      }
  }

  /* We'll be needing a couple extra algorithm structures now.  */

  alg_in = XALLOCA (struct algorithm);
  best_alg = XALLOCA (struct algorithm);
  best_cost = *cost_limit;

  /* Compute the hash index.  */
  hash_index = (t ^ (unsigned int) mode ^ (speed * 256)) % NUM_ALG_HASH_ENTRIES;

  /* See if we already know what to do for T.  */
  entry_ptr = alg_hash_entry_ptr (hash_index);
  if (entry_ptr->t == t  && entry_ptr->mode == mode
      && entry_ptr->speed == speed  && entry_ptr->alg != alg_unknown){
      cache_alg = entry_ptr->alg;

      if (cache_alg == alg_impossible){
          /* The cache tells us that it's impossible to synthesize
             multiplication by T within entry_ptr->cost.  */
          if (!CHEAPER_MULT_COST (&entry_ptr->cost, cost_limit))
            /* COST_LIMIT is at least as restrictive as the one
               recorded in the hash table, in which case we have no
               hope of synthesizing a multiplication.  Just
               return.  */
            return;
          /* If we get here, COST_LIMIT is less restrictive than the
             one recorded in the hash table, so we may be able to
             synthesize a multiplication.  Proceed as if we didn't
             have the cache entry.  */
      }else{
          if (CHEAPER_MULT_COST (cost_limit, &entry_ptr->cost))
            /* The cached algorithm shows that this multiplication
               requires more cost than COST_LIMIT.  Just return.  This
               way, we don't clobber this cache entry with
               alg_impossible but retain useful information.  */
            return;

          cache_hit = true;
          switch (cache_alg){
            case alg_shift:
              goto do_alg_shift;
            case alg_add_t_m2:
            case alg_sub_t_m2:
              goto do_alg_addsub_t_m2;
            case alg_add_factor:
            case alg_sub_factor:
              goto do_alg_addsub_factor;
            case alg_add_t2_m:
              goto do_alg_add_t2_m;
            case alg_sub_t2_m:
              goto do_alg_sub_t2_m;
            default:
              gcc_unreachable ();
          }
      }
  }

  /* If we have a group of zero bits at the low-order part of T, try
     multiplying by the remaining bits and then doing a shift.  */

  if ((t & 1) == 0){
    do_alg_shift:
      m = ctz_or_zero (t); /* m = number of low zero bits */
      if (m < maxm){
          q = t >> m;
          /* The function expand_shift will choose between a shift and
             a sequence of additions, so the observed cost is given as
             MIN (m * add_cost(speed, mode), shift_cost(speed, mode, m)).  */
          op_cost = m * mtcs_expmed_add_cost/*!add_cost*/(self,speed, mode);
          if (mtcs_expmed_shift_cost/*!shift_cost*/(self,speed, mode, m) < op_cost)
            op_cost = mtcs_expmed_shift_cost/*!shift_cost*/(self,speed, mode, m);
          new_limit.cost = best_cost.cost - op_cost;
          new_limit.latency = best_cost.latency - op_cost;
          synth_mult (self,alg_in, q, &new_limit, mode);

          alg_in->cost.cost += op_cost;
          alg_in->cost.latency += op_cost;
          if (CHEAPER_MULT_COST (&alg_in->cost, &best_cost)){
              best_cost = alg_in->cost;
              std::swap (alg_in, best_alg);
              best_alg->log[best_alg->ops] = m;
              best_alg->op[best_alg->ops] = alg_shift;
          }

          /* See if treating ORIG_T as a signed number yields a better
             sequence.  Try this sequence only for a negative ORIG_T
             as it would be useless for a non-negative ORIG_T.  */
          if ((HOST_WIDE_INT) orig_t < 0){
              /* Shift ORIG_T as follows because a right shift of a
             negative-valued signed type is implementation
             defined.  */
              q = ~(~orig_t >> m);
              /* The function expand_shift will choose between a shift
             and a sequence of additions, so the observed cost is
             given as MIN (m * add_cost(speed, mode),
             shift_cost(speed, mode, m)).  */
              op_cost = m * mtcs_expmed_add_cost/*!add_cost*/(self,speed, mode);
              if (mtcs_expmed_shift_cost/*!shift_cost*/(self,speed, mode, m) < op_cost)
                  op_cost = mtcs_expmed_shift_cost/*!shift_cost*/(self,speed, mode, m);
              new_limit.cost = best_cost.cost - op_cost;
              new_limit.latency = best_cost.latency - op_cost;
              synth_mult (self,alg_in, q, &new_limit, mode);

              alg_in->cost.cost += op_cost;
              alg_in->cost.latency += op_cost;
              if (CHEAPER_MULT_COST (&alg_in->cost, &best_cost)){
                  best_cost = alg_in->cost;
                  std::swap (alg_in, best_alg);
                  best_alg->log[best_alg->ops] = m;
                  best_alg->op[best_alg->ops] = alg_shift;
              }
          }
      }
      if (cache_hit)
          goto done;
  }

  /* If we have an odd number, add or subtract one.  */
  if ((t & 1) != 0){
      unsigned HOST_WIDE_INT w;

    do_alg_addsub_t_m2:
      for (w = 1; (w & t) != 0; w <<= 1)
          ;
      /* If T was -1, then W will be zero after the loop.  This is another
     case where T ends with ...111.  Handling this with (T + 1) and
     subtract 1 produces slightly better code and results in algorithm
     selection much faster than treating it like the ...0111 case
     below.  */
      if (w == 0 || (w > 2
          /* Reject the case where t is 3.
         Thus we prefer addition in that case.  */
          && t != 3)){
          /* T ends with ...111.  Multiply by (T + 1) and subtract T.  */
          op_cost = mtcs_expmed_add_cost/*!add_cost*/(self,speed, mode);
          new_limit.cost = best_cost.cost - op_cost;
          new_limit.latency = best_cost.latency - op_cost;
          synth_mult (self,alg_in, t + 1, &new_limit, mode);

          alg_in->cost.cost += op_cost;
          alg_in->cost.latency += op_cost;
          if (CHEAPER_MULT_COST (&alg_in->cost, &best_cost)){
              best_cost = alg_in->cost;
              std::swap (alg_in, best_alg);
              best_alg->log[best_alg->ops] = 0;
              best_alg->op[best_alg->ops] = alg_sub_t_m2;
          }
      }else{
          /* T ends with ...01 or ...011.  Multiply by (T - 1) and add T.  */

          op_cost = mtcs_expmed_add_cost/*!add_cost*/(self,speed, mode);
          new_limit.cost = best_cost.cost - op_cost;
          new_limit.latency = best_cost.latency - op_cost;
          synth_mult (self,alg_in, t - 1, &new_limit, mode);

          alg_in->cost.cost += op_cost;
          alg_in->cost.latency += op_cost;
          if (CHEAPER_MULT_COST (&alg_in->cost, &best_cost)){
              best_cost = alg_in->cost;
              std::swap (alg_in, best_alg);
              best_alg->log[best_alg->ops] = 0;
              best_alg->op[best_alg->ops] = alg_add_t_m2;
          }
      }

      /* We may be able to calculate a * -7, a * -15, a * -31, etc
     quickly with a - a * n for some appropriate constant n.  */
      m = exact_log2 (-orig_t + 1);
      if (m >= 0 && m < maxm){
          op_cost = mtcs_expmed_add_cost/*!add_cost*/(self,speed, mode) + mtcs_expmed_shift_cost/*!shift_cost*/(self,speed, mode, m);
          /* If the target has a cheap shift-and-subtract insn use
             that in preference to a shift insn followed by a sub insn.
             Assume that the shift-and-sub is "atomic" with a latency
             equal to it's cost, otherwise assume that on superscalar
             hardware the shift may be executed concurrently with the
             earlier steps in the algorithm.  */
          if (mtcs_expmed_shiftsub1_cost/*!shiftsub1_cost*/(self,speed, mode, m) <= op_cost){
              op_cost = mtcs_expmed_shiftsub1_cost/*!shiftsub1_cost*/(self,speed, mode, m);
              op_latency = op_cost;
          }else
            op_latency = mtcs_expmed_add_cost/*!add_cost*/(self,speed, mode);

          new_limit.cost = best_cost.cost - op_cost;
          new_limit.latency = best_cost.latency - op_latency;
          synth_mult (self,alg_in, (unsigned HOST_WIDE_INT) (-orig_t + 1) >> m,&new_limit, mode);
          alg_in->cost.cost += op_cost;
          alg_in->cost.latency += op_latency;
          if (CHEAPER_MULT_COST (&alg_in->cost, &best_cost)){
              best_cost = alg_in->cost;
              std::swap (alg_in, best_alg);
              best_alg->log[best_alg->ops] = m;
              best_alg->op[best_alg->ops] = alg_sub_t_m2;
          }
      }

      if (cache_hit)
          goto done;
  }

  /* Look for factors of t of the form
     t = q(2**m +- 1), 2 <= m <= floor(log2(t - 1)).
     If we find such a factor, we can multiply by t using an algorithm that
     multiplies by q, shift the result by m and add/subtract it to itself.

     We search for large factors first and loop down, even if large factors
     are less probable than small; if we find a large factor we will find a
     good sequence quickly, and therefore be able to prune (by decreasing
     COST_LIMIT) the search.  */

 do_alg_addsub_factor:
  for (m = floor_log2 (t - 1); m >= 2; m--){
      unsigned HOST_WIDE_INT d;

      d = (HOST_WIDE_INT_1U << m) + 1;
      if (t % d == 0 && t > d && m < maxm && (!cache_hit || cache_alg == alg_add_factor)){
          op_cost = mtcs_expmed_add_cost/*!add_cost*/(self,speed, mode) + mtcs_expmed_shift_cost/*!shift_cost*/(self,speed, mode, m);
          if (mtcs_expmed_shiftadd_cost/*!shiftadd_cost*/(self,speed, mode, m) <= op_cost)
            op_cost = mtcs_expmed_shiftadd_cost/*!shiftadd_cost*/(self,speed, mode, m);

          op_latency = op_cost;
          new_limit.cost = best_cost.cost - op_cost;
          new_limit.latency = best_cost.latency - op_latency;
          synth_mult (self,alg_in, t / d, &new_limit, mode);
          alg_in->cost.cost += op_cost;
          alg_in->cost.latency += op_latency;
          if (alg_in->cost.latency < op_cost)
            alg_in->cost.latency = op_cost;
          if (CHEAPER_MULT_COST (&alg_in->cost, &best_cost)){
              best_cost = alg_in->cost;
              std::swap (alg_in, best_alg);
              best_alg->log[best_alg->ops] = m;
              best_alg->op[best_alg->ops] = alg_add_factor;
          }
          /* Other factors will have been taken care of in the recursion.  */
          break;
      }

      d = (HOST_WIDE_INT_1U << m) - 1;
      if (t % d == 0 && t > d && m < maxm  && (!cache_hit || cache_alg == alg_sub_factor)){
          op_cost = mtcs_expmed_add_cost/*!add_cost*/(self,speed, mode) + mtcs_expmed_shift_cost/*!shift_cost*/(self,speed, mode, m);
          if (mtcs_expmed_shiftsub0_cost/*!shiftsub0_cost*/(self,speed, mode, m) <= op_cost)
            op_cost = mtcs_expmed_shiftsub0_cost/*!shiftsub0_cost*/(self,speed, mode, m);

          op_latency = op_cost;
          new_limit.cost = best_cost.cost - op_cost;
          new_limit.latency = best_cost.latency - op_latency;
          synth_mult (self,alg_in, t / d, &new_limit, mode);
          alg_in->cost.cost += op_cost;
          alg_in->cost.latency += op_latency;
          if (alg_in->cost.latency < op_cost)
            alg_in->cost.latency = op_cost;
          if (CHEAPER_MULT_COST (&alg_in->cost, &best_cost)){
              best_cost = alg_in->cost;
              std::swap (alg_in, best_alg);
              best_alg->log[best_alg->ops] = m;
              best_alg->op[best_alg->ops] = alg_sub_factor;
          }
          break;
      }
  }

  if (cache_hit)
    goto done;

  /* Try shift-and-add (load effective address) instructions,
     i.e. do a*3, a*5, a*9.  */
  if ((t & 1) != 0){
    do_alg_add_t2_m:
      q = t - 1;
      m = ctz_hwi (q);
      if (q && m < maxm){
          op_cost = mtcs_expmed_shiftadd_cost/*!shiftadd_cost*/(self,speed, mode, m);
          new_limit.cost = best_cost.cost - op_cost;
          new_limit.latency = best_cost.latency - op_cost;
          synth_mult (self,alg_in, (t - 1) >> m, &new_limit, mode);

          alg_in->cost.cost += op_cost;
          alg_in->cost.latency += op_cost;
          if (CHEAPER_MULT_COST (&alg_in->cost, &best_cost)){
              best_cost = alg_in->cost;
              std::swap (alg_in, best_alg);
              best_alg->log[best_alg->ops] = m;
              best_alg->op[best_alg->ops] = alg_add_t2_m;
          }
      }
      if (cache_hit)
          goto done;

    do_alg_sub_t2_m:
      q = t + 1;
      m = ctz_hwi (q);
      if (q && m < maxm){
          op_cost = mtcs_expmed_shiftsub0_cost/*!shiftsub0_cost*/(self,speed, mode, m);
          new_limit.cost = best_cost.cost - op_cost;
          new_limit.latency = best_cost.latency - op_cost;
          synth_mult (self,alg_in, (t + 1) >> m, &new_limit, mode);

          alg_in->cost.cost += op_cost;
          alg_in->cost.latency += op_cost;
          if (CHEAPER_MULT_COST (&alg_in->cost, &best_cost)){
              best_cost = alg_in->cost;
              std::swap (alg_in, best_alg);
              best_alg->log[best_alg->ops] = m;
              best_alg->op[best_alg->ops] = alg_sub_t2_m;
          }
      }
      if (cache_hit)
          goto done;
  }

 done:
  /* If best_cost has not decreased, we have not found any algorithm.  */
  if (!CHEAPER_MULT_COST (&best_cost, cost_limit)){
      /* We failed to find an algorithm.  Record alg_impossible for
     this case (that is, <T, MODE, COST_LIMIT>) so that next time
     we are asked to find an algorithm for T within the same or
     lower COST_LIMIT, we can immediately return to the
     caller.  */
      entry_ptr->t = t;
      entry_ptr->mode = mode;
      entry_ptr->speed = speed;
      entry_ptr->alg = alg_impossible;
      entry_ptr->cost = *cost_limit;
      return;
  }

  /* Cache the result.  */
  if (!cache_hit){
      entry_ptr->t = t;
      entry_ptr->mode = mode;
      entry_ptr->speed = speed;
      entry_ptr->alg = best_alg->op[best_alg->ops];
      entry_ptr->cost.cost = best_cost.cost;
      entry_ptr->cost.latency = best_cost.latency;
  }

  /* If we are getting a too long sequence for `struct algorithm'
     to record, make this search fail.  */
  if (best_alg->ops == MAX_BITS_PER_WORD)
    return;

  /* Copy the algorithm from temporary space to the space at alg_out.
     We avoid using structure assignment because the majority of
     best_alg is normally undefined, and this is a critical function.  */
  alg_out->ops = best_alg->ops + 1;
  alg_out->cost = best_cost;
  memcpy (alg_out->op, best_alg->op, alg_out->ops * sizeof *alg_out->op);
  memcpy (alg_out->log, best_alg->log,alg_out->ops * sizeof *alg_out->log);
}


/* A subroutine of expand_mult, used for constant multiplications.
   Multiply OP0 by VAL in mode MODE, storing the result in TARGET if
   convenient.  Use the shift/add sequence described by ALG and apply
   the final fixup specified by VARIANT.  */
//原型 expand_mult_const expmed.cc
static rtx expand_mult_const (MtcsExpmed *self,machine_mode mode, rtx op0, HOST_WIDE_INT val,
           rtx target, const struct algorithm *alg,enum mult_variant variant)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

  unsigned HOST_WIDE_INT val_so_far;
  rtx_insn *insn;
  rtx accum, tem;
  int opno;
  machine_mode nmode;
  /* Avoid referencing memory ver and over and invalid sharing
     on SUBREGs.  */
  op0 = mtcs_explow_force_reg(mtcsExplow,mode, op0);
  /* ACCUM starts out either as OP0 or as a zero, depending on
     the first operation.  */
  if (alg->op[0] == alg_zero){
      accum = mtcs_explow_copy_to_mode_reg(mtcsExplow,mode, CONST0_RTX (mode));
      val_so_far = 0;
  }else if (alg->op[0] == alg_m){
      accum = mtcs_explow_copy_to_mode_reg(mtcsExplow,mode, op0);
      val_so_far = 1;
  }else
    gcc_unreachable ();

  for (opno = 1; opno < alg->ops; opno++){
      int log = alg->log[opno];
      rtx shift_subtarget = optimize ? 0 : accum;
      rtx add_target = (opno == alg->ops - 1 && target != 0 && variant != add_variant && !optimize)
      ? target : 0;
      rtx accum_target = optimize ? 0 : accum;
      rtx accum_inner;
      switch (alg->op[opno]){
        case alg_shift:
          tem = mtcs_expmed_expand_shift(self,LSHIFT_EXPR, mode, accum, log, NULL_RTX, 0);
          /* REG_EQUAL note will be attached to the following insn.  */
          mtcs_expr_emit_move_insn(mtcsExpr,accum, tem);
          val_so_far <<= log;
          break;

        case alg_add_t_m2:
          tem = mtcs_expmed_expand_shift(self,LSHIFT_EXPR, mode, op0, log, NULL_RTX, 0);
          accum = mtcs_expr_force_operand(mtcsExpr,gen_rtx_PLUS (mode, accum, tem),add_target ? add_target : accum_target);
          val_so_far += HOST_WIDE_INT_1U << log;
          break;

        case alg_sub_t_m2:
          tem = mtcs_expmed_expand_shift(self,LSHIFT_EXPR, mode, op0, log, NULL_RTX, 0);
          accum = mtcs_expr_force_operand(mtcsExpr,gen_rtx_MINUS (mode, accum, tem),add_target ? add_target : accum_target);
          val_so_far -= HOST_WIDE_INT_1U << log;
          break;

        case alg_add_t2_m:
          accum = mtcs_expmed_expand_shift(self,LSHIFT_EXPR, mode, accum, log, shift_subtarget, 0);
          accum = mtcs_expr_force_operand(mtcsExpr,gen_rtx_PLUS (mode, accum, op0), add_target ? add_target : accum_target);
          val_so_far = (val_so_far << log) + 1;
          break;

        case alg_sub_t2_m:
          accum = mtcs_expmed_expand_shift(self,LSHIFT_EXPR, mode, accum, log, shift_subtarget, 0);
          accum = mtcs_expr_force_operand(mtcsExpr,gen_rtx_MINUS (mode, accum, op0),add_target ? add_target : accum_target);
          val_so_far = (val_so_far << log) - 1;
          break;

        case alg_add_factor:
          tem = mtcs_expmed_expand_shift(self,LSHIFT_EXPR, mode, accum, log, NULL_RTX, 0);
          accum = mtcs_expr_force_operand(mtcsExpr,gen_rtx_PLUS (mode, accum, tem),
                     add_target ? add_target : accum_target);
          val_so_far += val_so_far << log;
          break;

        case alg_sub_factor:
          tem = mtcs_expmed_expand_shift(self,LSHIFT_EXPR, mode, accum, log, NULL_RTX, 0);
          accum = mtcs_expr_force_operand(mtcsExpr,gen_rtx_MINUS (mode, tem, accum), (add_target? add_target : (optimize ? 0 : tem)));
          val_so_far = (val_so_far << log) - val_so_far;
          break;

        default:
          gcc_unreachable ();
      }

      if (mtcs_mode_is_scalar_int_p(mtcsMode,mode)){
          /* Write a REG_EQUAL note on the last insn so that we can cse
             multiplication sequences.  Note that if ACCUM is a SUBREG,
             we've set the inner register and must properly indicate that.  */
          tem = op0, nmode = mode;
          accum_inner = accum;
          if (GET_CODE (accum) == SUBREG){
              accum_inner = SUBREG_REG (accum);
              nmode = GET_MODE (accum_inner);
              tem = gen_lowpart (nmode, op0);
          }

          /* Don't add a REG_EQUAL note if tem is a paradoxical SUBREG.
             In that case, only the low bits of accum would be guaranteed to
             be equal to the content of the REG_EQUAL note, the upper bits
             can be anything.  */
          if (! mtcs_rtl_paradoxical_subreg_p/*!paradoxical_subreg_p*/(mtcsRTL,tem)){
              insn = mtcs_rtl_data_get_last_insn (mtcsRtlData);
              wide_int wval_so_far = wi::uhwi (val_so_far,
                      mtcs_mode_get_precision(mtcsMode,mtcs_mode_as_a <scalar_mode>(mtcsMode,nmode)));
              rtx c = mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/ (mtcsRTL,wval_so_far, nmode);
              mtcs_rtl_set_dst_reg_note/*!set_dst_reg_note*/(mtcsRTL,insn, REG_EQUAL, gen_rtx_MULT (nmode, tem, c),accum_inner);
          }
      }
  }

  if (variant == negate_variant){
      val_so_far = -val_so_far;
      accum = mtcs_optabs_expand_unop(mtcsOptabs,mode, neg_optab, accum, target, 0);
  }else if (variant == add_variant){
      val_so_far = val_so_far + 1;
      accum = mtcs_expr_force_operand(mtcsExpr,gen_rtx_PLUS (mode, accum, op0), target);
  }

  /* Compare only the bits of val and val_so_far that are significant
     in the result mode, to avoid sign-/zero-extension confusion.  */
  nmode = mtcs_mode_get_inner/*!GET_MODE_INNER*/ (mtcsMode,mode);
  val &= mtcs_mode_get_mask/*!GET_MODE_MASK*/ (mtcsMode,nmode);
  val_so_far &= mtcs_mode_get_mask/*!GET_MODE_MASK*/ (mtcsMode,nmode);
  gcc_assert (val == (HOST_WIDE_INT) val_so_far);

  return accum;
}


/* Subroutine of expmed_mult_highpart.  Return the MODE high part of OP.  */
//原型 extract_high_half expmed.cc
static rtx extract_high_half (MtcsExpmed *self,scalar_int_mode mode, rtx op)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  if (mode == word_mode)
    return mtcs_rtl_gen_highpart/*!gen_highpart*/(mtcsRTL,mode, op);

  scalar_int_mode wider_mode = mtcs_mode_get_wider/*!GET_MODE_WIDER_MODE*/(mtcsMode,mode).require ();

  op = mtcs_expmed_expand_shift (self,RSHIFT_EXPR, wider_mode, op,
             mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode), 0, 1);
  return mtcs_expr_convert_modes (mtcsExpr,mode, wider_mode, op, 0);
}


/* Like expmed_mult_highpart, but only consider using a multiplication
   optab.  OP1 is an rtx for the constant operand.  */
//原型 expmed_mult_highpart_optab expmed.cc

static rtx expmed_mult_highpart_optab (MtcsExpmed *self,scalar_int_mode mode, rtx op0, rtx op1,
                rtx target, int unsignedp, int max_cost)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);

  rtx narrow_op1 = mtcs_rtl_gen_int_mode (mtcsRTL,INTVAL (op1), mode);
  optab moptab;
  rtx tem;
  int size;
  bool speed = optimize_insn_for_speed_p ();

  scalar_int_mode wider_mode =mtcs_mode_get_wider/*!GET_MODE_WIDER_MODE*/(mtcsMode,mode).require ();

  size = mtcs_mode_get_bitsize (mtcsMode,mode);

  /* Firstly, try using a multiplication insn that only generates the needed
     high part of the product, and in the sign flavor of unsignedp.  */
  if (mtcs_expmed_mul_highpart_cost/*!mul_highpart_cost*/(self,speed, mode) < max_cost){
      moptab = unsignedp ? umul_highpart_optab : smul_highpart_optab;
      tem = mtcs_optabs_expand_binop(mtcsOptabs,mode, moptab, op0, narrow_op1, target,unsignedp, OPTAB_DIRECT);
      if (tem)
          return tem;
  }

  /* Secondly, same as above, but use sign flavor opposite of unsignedp.
     Need to adjust the result after the multiplication.  */
  if (size - 1 < BITS_PER_WORD
      && (mtcs_expmed_mul_highpart_cost/*!mul_highpart_cost*/(self,speed, mode) + 2
            * mtcs_expmed_shift_cost/*!shift_cost*/(self,speed, mode, size-1)
      + 4 * mtcs_expmed_add_cost/*!add_cost*/(self,speed, mode) < max_cost)){
      moptab = unsignedp ? smul_highpart_optab : umul_highpart_optab;
      tem = mtcs_optabs_expand_binop(mtcsOptabs,mode, moptab, op0, narrow_op1, target,unsignedp, OPTAB_DIRECT);
      if (tem)
        /* We used the wrong signedness.  Adjust the result.  */
        return mtcs_expmed_expand_mult_highpart_adjust (self,mode, tem, op0, narrow_op1,tem, unsignedp);
  }

  /* Try widening multiplication.  */
  moptab = unsignedp ? umul_widen_optab : smul_widen_optab;
  if (mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,moptab, wider_mode, mode) != CODE_FOR_nothing
      && mtcs_expmed_mul_widen_cost/*!mul_widen_cost*/(self,speed, wider_mode) < max_cost){
      tem = mtcs_optabs_expand_binop(mtcsOptabs,wider_mode, moptab, op0, narrow_op1, 0,unsignedp, OPTAB_WIDEN);
      if (tem)
          return extract_high_half(self,mode, tem);
  }

  /* Try widening the mode and perform a non-widening multiplication.  */
  if (optab_handler (smul_optab, wider_mode) != CODE_FOR_nothing
      && size - 1 < BITS_PER_WORD && (mtcs_expmed_mul_cost/*!mul_cost*/(self,speed, wider_mode)
            + mtcs_expmed_shift_cost/*!shift_cost*/(self,speed, mode, size-1) < max_cost)){
      rtx_insn *insns;
      rtx wop0, wop1;

      /* We need to widen the operands, for example to ensure the
     constant multiplier is correctly sign or zero extended.
     Use a sequence to clean-up any instructions emitted by
     the conversions if things don't work out.  */
      start_sequence ();
      wop0 = convert_modes (wider_mode, mode, op0, unsignedp);
      wop1 = convert_modes (wider_mode, mode, op1, unsignedp);
      tem = mtcs_optabs_expand_binop(mtcsOptabs,wider_mode, smul_optab, wop0, wop1, 0,
              unsignedp, OPTAB_WIDEN);
      insns = get_insns ();
      end_sequence ();

      if (tem){
          emit_insn (insns);
          return extract_high_half(self,mode, tem);
      }
  }

  /* Try widening multiplication of opposite signedness, and adjust.  */
  moptab = unsignedp ? smul_widen_optab : umul_widen_optab;
  if (mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,moptab, wider_mode, mode) != CODE_FOR_nothing
      && size - 1 < BITS_PER_WORD
      && (mtcs_expmed_mul_widen_cost/*!mul_widen_cost*/(self,speed, wider_mode)+ 2
            * mtcs_expmed_shift_cost/*!shift_cost*/(self,speed, mode, size-1)
      + 4 * mtcs_expmed_add_cost/*!add_cost*/(self,speed, mode) < max_cost)){
      tem = mtcs_optabs_expand_binop(mtcsOptabs,wider_mode, moptab, op0, narrow_op1,NULL_RTX, ! unsignedp, OPTAB_WIDEN);
      if (tem != 0){
          tem = extract_high_half(self,mode, tem);
          /* We used the wrong signedness.  Adjust the result.  */
          return mtcs_expmed_expand_mult_highpart_adjust (self,mode, tem, op0, narrow_op1,target, unsignedp);
      }
  }

  return 0;
}


/* Emit code to multiply OP0 and OP1 (where OP1 is an integer constant),
   putting the high half of the result in TARGET if that is convenient,
   and return where the result is.  If the operation cannot be performed,
   0 is returned.

   MODE is the mode of operation and result.

   UNSIGNEDP nonzero means unsigned multiply.

   MAX_COST is the total allowed cost for the expanded RTL.  */
//原型 expmed_mult_highpart expmed.cc
static rtx expmed_mult_highpart (MtcsExpmed *self,scalar_int_mode mode, rtx op0, rtx op1,
              rtx target, int unsignedp, int max_cost)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

  unsigned HOST_WIDE_INT cnst1;
  int extra_cost;
  bool sign_adjust = false;
  enum mult_variant variant;
  struct algorithm alg;
  rtx tem;
  bool speed = optimize_insn_for_speed_p ();

  /* We can't support modes wider than HOST_BITS_PER_INT.  */
  gcc_assert (mtcs_mode_is_hwi_computable_p/*!HWI_COMPUTABLE_MODE_P*/(mtcsMode,mode));

  cnst1 = INTVAL (op1) & mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,mode);

  /* We can't optimize modes wider than BITS_PER_WORD.
     ??? We might be able to perform double-word arithmetic if
     mode == word_mode, however all the cost calculations in
     synth_mult etc. assume single-word operations.  */
  scalar_int_mode wider_mode = mtcs_mode_get_wider/*!GET_MODE_WIDER_MODE*/(mtcsMode,mode).require ();
  if (mtcs_mode_get_bitsize(mtcsMode,wider_mode) > BITS_PER_WORD)
    return expmed_mult_highpart_optab (self,mode, op0, op1, target,unsignedp, max_cost);

  extra_cost = mtcs_expmed_shift_cost/*!shift_cost*/(self,speed, mode, mtcs_mode_get_bitsize(mtcsMode,mode) - 1);

  /* Check whether we try to multiply by a negative constant.  */
  if (!unsignedp && ((cnst1 >> (mtcs_mode_get_bitsize(mtcsMode,mode) - 1)) & 1)){
      sign_adjust = true;
      extra_cost += mtcs_expmed_add_cost/*!add_cost*/(self,speed, mode);
  }

  /* See whether shift/add multiplication is cheap enough.  */
  if (mtcs_expmed_choose_mult_variant (self,wider_mode, cnst1, &alg, (int *)&variant,max_cost - extra_cost)){
      /* See whether the specialized multiplication optabs are
     cheaper than the shift/add version.  */
      tem = expmed_mult_highpart_optab (self,mode, op0, op1, target, unsignedp,alg.cost.cost + extra_cost);
      if (tem)
          return tem;

      tem = mtcs_expr_convert_to_mode (mtcsExpr,wider_mode, op0, unsignedp);
      tem = expand_mult_const(self,wider_mode, tem, cnst1, 0, &alg, variant);
      tem = extract_high_half(self,mode, tem);

      /* Adjust result for signedness.  */
      if (sign_adjust)
          tem = mtcs_expr_force_operand(mtcsExpr,gen_rtx_MINUS (mode, tem, op0), tem);

      return tem;
  }
  return expmed_mult_highpart_optab (self,mode, op0, op1, target,unsignedp, max_cost);
}


/* Compute the inverse of X mod 2**n, i.e., find Y such that X * Y is
   congruent to 1 (mod 2**N).  */
//原型 invert_mod2n expmed.cc
static unsigned HOST_WIDE_INT invert_mod2n (unsigned HOST_WIDE_INT x, int n)
{
  /* Solve x*y == 1 (mod 2^n), where x is odd.  Return y.  */

  /* The algorithm notes that the choice y = x satisfies
     x*y == 1 mod 2^3, since x is assumed odd.
     Each iteration doubles the number of bits of significance in y.  */

  unsigned HOST_WIDE_INT mask;
  unsigned HOST_WIDE_INT y = x;
  int nbit = 3;

  mask = (n == HOST_BITS_PER_WIDE_INT? HOST_WIDE_INT_M1U: (HOST_WIDE_INT_1U << n) - 1);

  while (nbit < n){
      y = y * (2 - x*y) & mask;     /* Modulo 2^N */
      nbit *= 2;
  }
  return y;
}


/* Perform possibly multi-word comparison and conditional jump to LABEL
   if ARG1 OP ARG2 true where ARG1 and ARG2 are of mode MODE.  This is
   now a thin wrapper around do_compare_rtx_and_jump.  */
//原型 do_cmp_and_jump expmed.cc
static void do_cmp_and_jump (MtcsExpmed *self,rtx arg1, rtx arg2, enum rtx_code op, machine_mode mode,
         rtx_code_label *label)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  int unsignedp = (op == LTU || op == LEU || op == GTU || op == GEU);
  mtcs_dojump_do_compare_rtx_and_jump (mtcsDojump,arg1, arg2, op, unsignedp, mode, NULL_RTX,
               NULL, label, profile_probability::uninitialized ());
}

/* Output a shift instruction for expression code CODE,
   with SHIFTED being the rtx for the value to shift,
   and AMOUNT the rtx for the amount to shift by.
   Store the result in the rtx TARGET, if that is convenient.
   If UNSIGNEDP is nonzero, do a logical shift; otherwise, arithmetic.
   Return the rtx for where the value is.
   If that cannot be done, abort the compilation unless MAY_FAIL is true,
   in which case 0 is returned.  */
//原型 expand_shift_1 expmed.cc
static rtx expand_shift_1 (MtcsExpmed *self,enum tree_code code, machine_mode mode, rtx shifted,
        rtx amount, rtx target, int unsignedp, bool may_fail = false)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

  rtx op1, temp = 0;
  int left = (code == LSHIFT_EXPR || code == LROTATE_EXPR);
  int rotate = (code == LROTATE_EXPR || code == RROTATE_EXPR);
  optab lshift_optab = ashl_optab;
  optab rshift_arith_optab = ashr_optab;
  optab rshift_uns_optab = lshr_optab;
  optab lrotate_optab = rotl_optab;
  optab rrotate_optab = rotr_optab;
  machine_mode op1_mode;
  scalar_mode scalar_mode = mtcs_mode_get_inner(mtcsMode,mode);
  int attempt;
  bool speed = optimize_insn_for_speed_p ();

  op1 = amount;
  op1_mode = GET_MODE (op1);

  /* Determine whether the shift/rotate amount is a vector, or scalar.  If the
     shift amount is a vector, use the vector/vector shift patterns.  */
  if (mtcs_mode_is_vector_p(mtcsMode,mode) && mtcs_mode_is_vector_p(mtcsMode,op1_mode)){
      lshift_optab = vashl_optab;
      rshift_arith_optab = vashr_optab;
      rshift_uns_optab = vlshr_optab;
      lrotate_optab = vrotl_optab;
      rrotate_optab = vrotr_optab;
  }

  /* Previously detected shift-counts computed by NEGATE_EXPR
     and shifted in the other direction; but that does not work
     on all machines.  */

  if (SHIFT_COUNT_TRUNCATED){
      if (CONST_INT_P (op1)
         && ((unsigned HOST_WIDE_INT) INTVAL (op1) >=(unsigned HOST_WIDE_INT) mtcs_mode_get_bitsize(mtcsMode,scalar_mode)))
          op1 = mtcs_rtl_gen_int_shift_amount (mtcsRTL,mode,(unsigned HOST_WIDE_INT) INTVAL (op1) % mtcs_mode_get_bitsize(mtcsMode,scalar_mode));
      else if (GET_CODE (op1) == SUBREG
           && mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,op1)
           && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/ (mtcsMode,GET_MODE (SUBREG_REG (op1)))
           && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/ (mtcsMode,GET_MODE (op1)))
    op1 = SUBREG_REG (op1);
  }

  /* Canonicalize rotates by constant amount.  We may canonicalize
     to reduce the immediate or if the ISA can rotate by constants
     in only on direction.  */
  if (rotate && mtcs_simplify_rtx_reverse_rotate_by_imm_p (mtcsSimplifyRtx,scalar_mode, left, op1)){
      op1 = mtcs_rtl_gen_int_shift_amount (mtcsRTL,mode, (mtcs_mode_get_bitsize(mtcsMode,scalar_mode)
                     - INTVAL (op1)));
      left = !left;
      code = left ? LROTATE_EXPR : RROTATE_EXPR;
  }

  /* Rotation of 16bit values by 8 bits is effectively equivalent to a bswaphi.
     Note that this is not the case for bigger values.  For instance a rotation
     of 0x01020304 by 16 bits gives 0x03040102 which is different from
     0x04030201 (bswapsi).  */
  if (rotate
      && CONST_INT_P (op1)
      && INTVAL (op1) == BITS_PER_UNIT
      && mtcs_mode_get_size(mtcsMode,scalar_mode) == 2
      && mtcs_opinit_optab_handler (mtcsOpinit,bswap_optab, mode) != CODE_FOR_nothing)
    return mtcs_optabs_expand_unop/*!expand_unop*/ (mtcsOptabs,mode, bswap_optab, shifted, NULL_RTX, unsignedp);

  if (op1 == const0_rtx)
    return shifted;

  /* Check whether its cheaper to implement a left shift by a constant
     bit count by a sequence of additions.  */
  if (code == LSHIFT_EXPR
      && CONST_INT_P (op1)
      && INTVAL (op1) > 0
      && INTVAL (op1) < mtcs_mode_get_precision(mtcsMode,scalar_mode)
      && INTVAL (op1) < MAX_BITS_PER_WORD
      && (mtcs_expmed_shift_cost/*!shift_cost*/(self,speed, mode, INTVAL (op1))
      > INTVAL (op1) * mtcs_expmed_add_cost/*!add_cost*/(self,speed, mode))
      && mtcs_expmed_shift_cost/*!shift_cost*/(self,speed, mode, INTVAL (op1)) != MAX_COST){
      int i;
      for (i = 0; i < INTVAL (op1); i++){
          temp = mtcs_explow_force_reg/*!force_reg*/ (mtcsExplow,mode, shifted);
          shifted = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,mode, add_optab, temp, temp, NULL_RTX,unsignedp, OPTAB_LIB_WIDEN);
      }
      return shifted;
  }

  for (attempt = 0; temp == 0 && attempt < 3; attempt++){
      enum optab_methods methods;
      if (attempt == 0)
          methods = OPTAB_DIRECT;
      else if (attempt == 1)
          methods = OPTAB_WIDEN;
      else
          methods = OPTAB_LIB_WIDEN;

      if (rotate){
          /* Widening does not work for rotation.  */
          if (methods == OPTAB_WIDEN)
            continue;
          else if (methods == OPTAB_LIB_WIDEN){
              /* If we have been unable to open-code this by a rotation,
             do it as the IOR of two shifts.  I.e., to rotate A
             by N bits, compute
             (A << N) | ((unsigned) A >> ((-N) & (C - 1)))
             where C is the bitsize of A.

             It is theoretically possible that the target machine might
             not be able to perform either shift and hence we would
             be making two libcalls rather than just the one for the
             shift (similarly if IOR could not be done).  We will allow
             this extremely unlikely lossage to avoid complicating the
             code below.  */
              rtx subtarget = target == shifted ? 0 : target;
              rtx new_amount, other_amount;
              rtx temp1;

              new_amount = op1;
              if (op1 == const0_rtx)
                  return shifted;
              else if (CONST_INT_P (op1))
                  other_amount = gen_int_shift_amount(mode, mtcs_mode_get_bitsize(mtcsMode,scalar_mode) - INTVAL (op1));
              else{
                  other_amount = mtcs_simplify_rtx_gen_unary (mtcsSimplifyRtx,NEG, GET_MODE (op1),op1, GET_MODE (op1));
                  HOST_WIDE_INT mask = mtcs_mode_get_precision(mtcsMode,scalar_mode) - 1;
                  other_amount = mtcs_simplify_rtx_gen_binary (mtcsSimplifyRtx,AND, GET_MODE (op1),
                          other_amount,mtcs_rtl_gen_int_mode (mtcsRTL,mask, GET_MODE (op1)));
              }

              shifted = mtcs_explow_force_reg(mtcsExplow,mode, shifted);
              temp = expand_shift_1 (self,left ? LSHIFT_EXPR : RSHIFT_EXPR, mode, shifted, new_amount, 0, 1);
              temp1 = expand_shift_1 (self,left ? RSHIFT_EXPR : LSHIFT_EXPR,mode, shifted, other_amount,subtarget, 1);
              return  mtcs_optabs_expand_binop(mtcsOptabs,mode, ior_optab, temp, temp1, target,unsignedp, methods);
          }

          temp =  mtcs_optabs_expand_binop(mtcsOptabs,mode,left ? lrotate_optab : rrotate_optab,shifted, op1, target, unsignedp, methods);
      }else if (unsignedp)
          temp =  mtcs_optabs_expand_binop(mtcsOptabs,mode,left ? lshift_optab : rshift_uns_optab,shifted, op1, target, unsignedp, methods);

      /* Do arithmetic shifts.
     Also, if we are going to widen the operand, we can just as well
     use an arithmetic right-shift instead of a logical one.  */
      if (temp == 0 && ! rotate && (! unsignedp || (! left && methods == OPTAB_WIDEN))){
          enum optab_methods methods1 = methods;
              /* If trying to widen a log shift to an arithmetic shift,
             don't accept an arithmetic shift of the same size.  */
          if (unsignedp)
            methods1 = OPTAB_MUST_WIDEN;
          /* Arithmetic shift */
          temp =  mtcs_optabs_expand_binop(mtcsOptabs,mode,left ? lshift_optab : rshift_arith_optab, shifted, op1, target, unsignedp, methods1);
      }

      /* We used to try extzv here for logical right shifts, but that was
     only useful for one machine, the VAX, and caused poor code
     generation there for lshrdi3, so the code was deleted and a
     define_expand for lshrsi3 was added to vax.md.  */
  }

  gcc_assert (temp != NULL_RTX || may_fail);
  return temp;
}



/* A subroutine of emit_store_flag only including "tricks" that do not
   need a recursive call.  These are kept separate to avoid infinite
   loops.  */
//原型 emit_store_flag_1 expmed.cc
static rtx emit_store_flag_1 (MtcsExpmed *self,rtx target, enum rtx_code code, rtx op0, rtx op1,
           machine_mode mode, int unsignedp, int normalizep,machine_mode target_mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);

  rtx subtarget;
  enum insn_code icode;
  machine_mode compare_mode;
  enum mode_class mclass;
  enum rtx_code scode;

  if (unsignedp)
    code = unsigned_condition (code);
  scode = swap_condition (code);

  /* If one operand is constant, make it the second one.  Only do this
     if the other operand is not constant as well.  */

  if (mtcs_rtlanal_swap_commutative_operands_p (mtcsRtlanal,op0, op1)){
      std::swap (op0, op1);
      code = swap_condition (code);
  }

  if (mode == VOIDmode)
    mode = GET_MODE (op0);

  if (CONST_SCALAR_INT_P (op1))
      mtcs_expmed_canonicalize_comparison (self,mode, &code, &op1);

  /* For some comparisons with 1 and -1, we can convert this to
     comparisons with zero.  This will often produce more opportunities for
     store-flag insns.  */

  switch (code){
    case LT:
      if (op1 == const1_rtx)
          op1 = const0_rtx, code = LE;
      break;
    case LE:
      if (op1 == constm1_rtx)
          op1 = const0_rtx, code = LT;
      break;
    case GE:
      if (op1 == const1_rtx)
          op1 = const0_rtx, code = GT;
      break;
    case GT:
      if (op1 == constm1_rtx)
          op1 = const0_rtx, code = GE;
      break;
    case GEU:
      if (op1 == const1_rtx)
          op1 = const0_rtx, code = NE;
      break;
    case LTU:
      if (op1 == const1_rtx)
          op1 = const0_rtx, code = EQ;
      break;
    default:
      break;
  }

  /* If this is A < 0 or A >= 0, we can do this by taking the ones
     complement of A (for GE) and shifting the sign bit to the low bit.  */
  scalar_int_mode int_mode;
  if (op1 == const0_rtx && (code == LT || code == GE)
      && mtcs_mode_is_int_mode (mtcsMode,mode, &int_mode)
      && (normalizep || STORE_FLAG_VALUE == 1
      || mtcs_simplify_rtx_val_signbit_p (mtcsSimplifyRtx,int_mode, STORE_FLAG_VALUE))){
      scalar_int_mode int_target_mode;
      subtarget = target;

      if (!target)
          int_target_mode = int_mode;
      else {
          /* If the result is to be wider than OP0, it is best to convert it
             first.  If it is to be narrower, it is *incorrect* to convert it
             first.  */
          int_target_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,target_mode);
          if (mtcs_mode_get_size(mtcsMode,int_target_mode) > mtcs_mode_get_size (mtcsMode,int_mode)){
              op0 = mtcs_expr_convert_modes (mtcsExpr,int_target_mode, int_mode, op0, 0);
              int_mode = int_target_mode;
          }
      }

      if (int_target_mode != int_mode)
          subtarget = 0;

      if (code == GE)
        op0 = mtcs_optabs_expand_unop (mtcsOptabs,int_mode, one_cmpl_optab, op0,
                   ((STORE_FLAG_VALUE == 1 || normalizep) ? 0 : subtarget), 0);

      if (STORE_FLAG_VALUE == 1 || normalizep)
        /* If we are supposed to produce a 0/1 value, we want to do
           a logical shift from the sign bit to the low-order bit; for
           a -1/0 value, we do an arithmetic shift.  */
        op0 = mtcs_expmed_expand_shift (self,RSHIFT_EXPR, int_mode, op0,
                    mtcs_mode_get_bitsize(mtcsMode,int_mode) - 1,
                    subtarget, normalizep != -1);

      if (int_mode != int_target_mode)
          op0 = mtcs_expr_convert_modes (mtcsExpr,int_target_mode, int_mode, op0, 0);

      return op0;
  }

  /* Next try expanding this via the backend's cstore<mode>4.  */
  mclass =mtcs_mode_get_class(mtcsMode,mode);
  MTCS_FOR_EACH_WIDER_MODE_FROM (mtcsMode,compare_mode, mode){
     machine_mode optab_mode = mclass == MODE_CC ? CCmode : compare_mode;
     icode = mtcs_opinit_optab_handler (mtcsOpinit,cstore_optab, optab_mode);
     if (icode != CODE_FOR_nothing){
          mtcs_dojump_do_pending_stack_adjust (mtcsDojump);
          rtx tem = mtcs_expmed_emit_cstore (self,target, icode, code, mode, compare_mode,
                     unsignedp, op0, op1, normalizep, target_mode);
          if (tem)
            return tem;

          if (mtcs_mode_get_class (mtcsMode,mode) == MODE_FLOAT){
              tem = mtcs_expmed_emit_cstore (self,target, icode, scode, mode, compare_mode,
                     unsignedp, op1, op0, normalizep, target_mode);
              if (tem)
                return tem;
          }
          break;
     }
  }

  /* If we are comparing a double-word integer with zero or -1, we can
     convert the comparison into one involving a single word.  */
  if (mtcs_mode_is_int_mode (mtcsMode,mode, &int_mode)
      && mtcs_mode_get_bitsize(mtcsMode,int_mode) == BITS_PER_WORD * 2
      && (!MEM_P (op0) || ! MEM_VOLATILE_P (op0))){
      rtx tem;
      if ((code == EQ || code == NE) && (op1 == const0_rtx || op1 == constm1_rtx)){
          rtx op00, op01;

          /* Do a logical OR or AND of the two words and compare the
             result.  */
          op00 = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,word_mode, op0, int_mode, 0);
          op01 = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,word_mode, op0, int_mode, UNITS_PER_WORD);
          tem =  mtcs_optabs_expand_binop(mtcsOptabs,word_mode,
                      op1 == const0_rtx ? ior_optab : and_optab,
                      op00, op01, NULL_RTX, unsignedp,
                      OPTAB_DIRECT);

          if (tem != 0)
            tem = mtcs_expmed_emit_store_flag(self,NULL_RTX, code, tem, op1, word_mode,
                       unsignedp, normalizep);
      }else if ((code == LT || code == GE) && op1 == const0_rtx){
          rtx op0h;

          /* If testing the sign bit, can just test on high word.  */
          op0h = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,word_mode, op0, int_mode,
                          subreg_highpart_offset (word_mode,
                                      int_mode));
          tem = mtcs_expmed_emit_store_flag(self,NULL_RTX, code, op0h, op1, word_mode,
                     unsignedp, normalizep);
      }else
          tem = NULL_RTX;

      if (tem){
          if (target_mode == VOIDmode || GET_MODE (tem) == target_mode)
            return tem;
          if (!target)
            target = mtcs_emit_gen_reg_rtx(mtcsEmit,target_mode);

          mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,target, tem,
                !mtcs_simplify_rtx_val_signbit_known_set_p/*!val_signbit_known_set_p*/(mtcsSimplifyRtx,word_mode,
                              (normalizep ? normalizep
                               : STORE_FLAG_VALUE)));
          return target;
      }
  }

  return 0;
}


/* Helper function for canonicalize_cmp_for_target.  Swap between inclusive
   and exclusive ranges in order to create an equivalent comparison.  See
   canonicalize_cmp_for_target for the possible cases.  */
//原型 equivalent_cmp_code expmed.cc
static enum rtx_code equivalent_cmp_code (enum rtx_code code)
{
  switch (code)
    {
    case GT:
      return GE;
    case GE:
      return GT;
    case LT:
      return LE;
    case LE:
      return LT;
    case GTU:
      return GEU;
    case GEU:
      return GTU;
    case LTU:
      return LEU;
    case LEU:
      return LTU;

    default:
      return code;
    }
}

/* Emit the code to divide OP0 by OP1, putting the result in TARGET
   if that is convenient, and returning where the result is.
   You may request either the quotient or the remainder as the result;
   specify REM_FLAG nonzero to get the remainder.

   CODE is the expression code for which kind of division this is;
   it controls how rounding is done.  MODE is the machine mode to use.
   UNSIGNEDP nonzero means do unsigned division.  */

/* ??? For CEIL_MOD_EXPR, can compute incorrect remainder with ANDI
   and then correct it by or'ing in missing high bits
   if result of ANDI is nonzero.
   For ROUND_MOD_EXPR, can use ANDI and then sign-extend the result.
   This could optimize to a bfexts instruction.
   But C doesn't use these operations, so their optimizations are
   left for later.  */
/* ??? For modulo, we don't actually need the highpart of the first product,
   the low part will do nicely.  And for small divisors, the second multiply
   can also be a low-part only multiply or even be completely left out.
   E.g. to calculate the remainder of a division by 3 with a 32 bit
   multiply, multiply with 0x55555556 and extract the upper two bits;
   the result is exact for inputs up to 0x1fffffff.
   The input range can be reduced by using cross-sum rules.
   For odd divisors >= 3, the following table gives right shift counts
   so that if a number is shifted by an integer multiple of the given
   amount, the remainder stays the same:
   2, 4, 3, 6, 10, 12, 4, 8, 18, 6, 11, 20, 18, 0, 5, 10, 12, 0, 12, 20,
   14, 12, 23, 21, 8, 0, 20, 18, 0, 0, 6, 12, 0, 22, 0, 18, 20, 30, 0, 0,
   0, 8, 0, 11, 12, 10, 36, 0, 30, 0, 0, 12, 0, 0, 0, 0, 44, 12, 24, 0,
   20, 0, 7, 14, 0, 18, 36, 0, 0, 46, 60, 0, 42, 0, 15, 24, 20, 0, 0, 33,
   0, 20, 0, 0, 18, 0, 60, 0, 0, 0, 0, 0, 40, 18, 0, 0, 12

   Cross-sum rules for even numbers can be derived by leaving as many bits
   to the right alone as the divisor has zeros to the right.
   E.g. if x is an unsigned 32 bit number:
   (x mod 12) == (((x & 1023) + ((x >> 8) & ~3)) * 0x15555558 >> 2 * 3) >> 28
   */
//原型 expand_divmod expmed.h expmed.cc
rtx mtcs_expmed_expand_divmod (MtcsExpmed *self,int rem_flag, enum tree_code code, machine_mode mode,
           rtx op0, rtx op1, rtx target, int unsignedp, int /*!enum optab_methods编译通不过*/ methods)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  machine_mode compute_mode;
  rtx tquotient;
  rtx quotient = 0, remainder = 0;
  rtx_insn *last;
  rtx_insn *insn;
  optab optab1, optab2;
  int op1_is_constant, op1_is_pow2 = 0;
  int max_cost, extra_cost;
  static HOST_WIDE_INT last_div_const = 0;
  bool speed = optimize_insn_for_speed_p ();

  op1_is_constant = CONST_INT_P (op1);
  if (op1_is_constant){
      wide_int ext_op1 = mtcs_rtx_mode_t/*!rtx_mode_t*/(op1, mode);
      op1_is_pow2 = (wi::popcount (ext_op1) == 1|| (! unsignedp && wi::popcount (wi::neg (ext_op1)) == 1));
  }

  /*
     This is the structure of expand_divmod:

     First comes code to fix up the operands so we can perform the operations
     correctly and efficiently.

     Second comes a switch statement with code specific for each rounding mode.
     For some special operands this code emits all RTL for the desired
     operation, for other cases, it generates only a quotient and stores it in
     QUOTIENT.  The case for trunc division/remainder might leave quotient = 0,
     to indicate that it has not done anything.

     Last comes code that finishes the operation.  If QUOTIENT is set and
     REM_FLAG is set, the remainder is computed as OP0 - QUOTIENT * OP1.  If
     QUOTIENT is not set, it is computed using trunc rounding.

     We try to generate special code for division and remainder when OP1 is a
     constant.  If |OP1| = 2**n we can use shifts and some other fast
     operations.  For other values of OP1, we compute a carefully selected
     fixed-point approximation m = 1/OP1, and generate code that multiplies OP0
     by m.

     In all cases but EXACT_DIV_EXPR, this multiplication requires the upper
     half of the product.  Different strategies for generating the product are
     implemented in expmed_mult_highpart.

     If what we actually want is the remainder, we generate that by another
     by-constant multiplication and a subtraction.  */

  /* We shouldn't be called with OP1 == const1_rtx, but some of the
     code below will malfunction if we are, so check here and handle
     the special case if so.  */
  if (op1 == const1_rtx)
    return rem_flag ? const0_rtx : op0;

    /* When dividing by -1, we could get an overflow.
     negv_optab can handle overflows.  */
  if (! unsignedp && op1 == constm1_rtx){
      if (rem_flag)
          return const0_rtx;
      return mtcs_optabs_expand_unop/*!expand_unop*/ (mtcsOptabs,mode,
              flag_trapv && mtcs_mode_get_class(mtcsMode,mode) == MODE_INT ? negv_optab : neg_optab, op0, target, 0);
  }

  if (target
      /* Don't use the function value register as a target
     since we have to read it as well as write it,
     and function-inlining gets confused by this.  */
      && ((REG_P (target) && REG_FUNCTION_VALUE_P (target))
      /* Don't clobber an operand while doing a multi-step calculation.  */
      || ((rem_flag || op1_is_constant) && (reg_mentioned_p (target, op0) || (MEM_P (op0) && MEM_P (target))))
      || reg_mentioned_p (target, op1)
      || (MEM_P (op1) && MEM_P (target))))
    target = 0;

  /* Get the mode in which to perform this computation.  Normally it will
     be MODE, but sometimes we can't do the desired operation in MODE.
     If so, pick a wider mode in which we can do the operation.  Convert
     to that mode at the start to avoid repeated conversions.

     First see what operations we need.  These depend on the expression
     we are evaluating.  (We assume that divxx3 insns exist under the
     same conditions that modxx3 insns and that these insns don't normally
     fail.  If these assumptions are not correct, we may generate less
     efficient code in some cases.)

     Then see if we find a mode in which we can open-code that operation
     (either a division, modulus, or shift).  Finally, check for the smallest
     mode for which we can do the operation with a library call.  */

  /* We might want to refine this now that we have division-by-constant
     optimization.  Since expmed_mult_highpart tries so many variants, it is
     not straightforward to generalize this.  Maybe we should make an array
     of possible modes in init_expmed?  Save this for GCC 2.7.  */

  optab1 = (op1_is_pow2
        ? (unsignedp ? lshr_optab : ashr_optab)
        : (unsignedp ? udiv_optab : sdiv_optab));
  optab2 = (op1_is_pow2 ? optab1
        : (unsignedp ? udivmod_optab : sdivmod_optab));

  if (methods == OPTAB_WIDEN || methods == OPTAB_LIB_WIDEN){
      MTCS_FOR_EACH_MODE_FROM (mtcsMode,compute_mode, mode)
          if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,optab1, compute_mode) != CODE_FOR_nothing
              || mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,optab2, compute_mode) != CODE_FOR_nothing)
              break;

      if (compute_mode == VOIDmode && methods == OPTAB_LIB_WIDEN)
          MTCS_FOR_EACH_MODE_FROM (mtcsMode,compute_mode, mode)
            if (mtcs_libfuncs_optab_libfunc/*!optab_libfunc*/(mtcsLibfuncs,optab1, compute_mode)
                  || mtcs_libfuncs_optab_libfunc/*!optab_libfunc*/(mtcsLibfuncs,optab2, compute_mode))
              break;
  }else
    compute_mode = mode;

  /* If we still couldn't find a mode, use MODE, but expand_binop will
     probably die.  */
  if (compute_mode == VOIDmode)
    compute_mode = mode;

  if (target && GET_MODE (target) == compute_mode)
    tquotient = target;
  else
    tquotient = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,compute_mode);

#if 0
  /* It should be possible to restrict the precision to GET_MODE_BITSIZE
     (mode), and thereby get better code when OP1 is a constant.  Do that
     later.  It will require going over all usages of SIZE below.  */
  size = mtcs_mode_get_bitsize(mtcsMode,mode);
#endif

  /* Only deduct something for a REM if the last divide done was
     for a different constant.   Then set the constant of the last
     divide.  */
  max_cost = (unsignedp ? mtcs_expmed_udiv_cost/*!udiv_cost*/(self,speed, compute_mode)
            : mtcs_expmed_sdiv_cost/*!sdiv_cost*/(self,speed, compute_mode));
  if (rem_flag && ! (last_div_const != 0 && op1_is_constant && INTVAL (op1) == last_div_const))
    max_cost -= (mtcs_expmed_mul_cost/*!mul_cost*/(self,speed, compute_mode)
              + mtcs_expmed_add_cost/*!add_cost*/(self,speed, compute_mode));

  last_div_const = ! rem_flag && op1_is_constant ? INTVAL (op1) : 0;

  /* Now convert to the best mode to use.  */
  if (compute_mode != mode){
      op0 = mtcs_expr_convert_modes/*!convert_modes*/ (mtcsExpr,compute_mode, mode, op0, unsignedp);
      op1 = mtcs_expr_convert_modes/*!convert_modes*/ (mtcsExpr,compute_mode, mode, op1, unsignedp);

      /* convert_modes may have placed op1 into a register, so we
     must recompute the following.  */
      op1_is_constant = CONST_INT_P (op1);
      if (op1_is_constant){
          wide_int ext_op1 = mtcs_rtx_mode_t/*!rtx_mode_t*/(op1, compute_mode);
          op1_is_pow2 = (wi::popcount (ext_op1) == 1 || (! unsignedp && wi::popcount (wi::neg (ext_op1)) == 1));
      }else
          op1_is_pow2 = 0;
  }

  /* If one of the operands is a volatile MEM, copy it into a register.  */

  if (MEM_P (op0) && MEM_VOLATILE_P (op0))
    op0 = mtcs_explow_force_reg(mtcsExplow,compute_mode, op0);
  if (MEM_P (op1) && MEM_VOLATILE_P (op1))
    op1 = mtcs_explow_force_reg(mtcsExplow,compute_mode, op1);

  /* If we need the remainder or if OP1 is constant, we need to
     put OP0 in a register in case it has any queued subexpressions.  */
  if (rem_flag || op1_is_constant)
    op0 = mtcs_explow_force_reg(mtcsExplow,compute_mode, op0);

  last = mtcs_rtl_data_get_last_insn (mtcsRtlData);

  /* Promote floor rounding to trunc rounding for unsigned operations.  */
  if (unsignedp){
      if (code == FLOOR_DIV_EXPR)
          code = TRUNC_DIV_EXPR;
      if (code == FLOOR_MOD_EXPR)
          code = TRUNC_MOD_EXPR;
      if (code == EXACT_DIV_EXPR && op1_is_pow2)
          code = TRUNC_DIV_EXPR;
  }

  if (op1 != const0_rtx)
    switch (code){
      case TRUNC_MOD_EXPR:
      case TRUNC_DIV_EXPR:
        if (op1_is_constant){
            scalar_int_mode int_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,compute_mode);
            int size = mtcs_mode_get_bitsize(mtcsMode,int_mode);
            if (unsignedp){
                unsigned HOST_WIDE_INT mh, ml;
                int pre_shift, post_shift;
                wide_int wd = mtcs_rtx_mode_t/*!rtx_mode_t*/(op1, int_mode);
                unsigned HOST_WIDE_INT d = wd.to_uhwi ();

                if (wi::popcount (wd) == 1){
                    pre_shift = floor_log2 (d);
                    if (rem_flag){
                        unsigned HOST_WIDE_INT mask = (HOST_WIDE_INT_1U << pre_shift) - 1;
                        remainder = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,int_mode, and_optab, op0,
                                mtcs_rtl_gen_int_mode/*!gen_int_mode*/ (mtcsRTL,mask, int_mode),
                                  remainder, 1, methods);
                        if (remainder)
                          return gen_lowpart (mode, remainder);
                    }
                    quotient = mtcs_expmed_expand_shift(self,RSHIFT_EXPR, int_mode, op0,pre_shift, tquotient, 1);
                }else if (size <= HOST_BITS_PER_WIDE_INT){
                    if (d >= (HOST_WIDE_INT_1U << (size - 1))){
                        /* Most significant bit of divisor is set; emit an scc
                           insn.  */
                        quotient = mtcs_expmed_emit_store_flag_force(self,tquotient, GEU, op0, op1,int_mode, 1, 1);
                    }else{
                        /* Find a suitable multiplier and right shift count
                           instead of multiplying with D.  */
                        mh = choose_multiplier (d, size, size, &ml, &post_shift);

                        /* If the suggested multiplier is more than SIZE bits,
                           we can do better for even divisors, using an
                           initial right shift.  */
                        if (mh != 0 && (d & 1) == 0){
                            pre_shift = ctz_or_zero (d);
                            mh = choose_multiplier (d >> pre_shift, size,size - pre_shift,&ml, &post_shift);
                            gcc_assert (!mh);
                        }else
                            pre_shift = 0;

                        if (mh != 0) {
                            rtx t1, t2, t3, t4;
                            if (post_shift - 1 >= BITS_PER_WORD)
                              goto fail1;

                            extra_cost = (mtcs_expmed_shift_cost/*!shift_cost*/(self,speed, int_mode, post_shift - 1)
                                  + mtcs_expmed_shift_cost/*!shift_cost*/(self,speed, int_mode, 1)
                                  + 2 * mtcs_expmed_add_cost/*!add_cost*/(self,speed, int_mode));
                            t1 = expmed_mult_highpart(self,int_mode,
                                  op0, mtcs_rtl_gen_int_mode (mtcsRTL,ml, int_mode),NULL_RTX, 1, max_cost - extra_cost);
                            if (t1 == 0)
                              goto fail1;
                            t2 = mtcs_expr_force_operand (mtcsExpr,gen_rtx_MINUS (int_mode,op0, t1),NULL_RTX);
                            t3 = mtcs_expmed_expand_shift (self,RSHIFT_EXPR, int_mode, t2, 1, NULL_RTX, 1);
                            t4 = mtcs_expr_force_operand (mtcsExpr,gen_rtx_PLUS (int_mode,t1, t3),NULL_RTX);
                            quotient = mtcs_expmed_expand_shift(self,RSHIFT_EXPR, int_mode, t4,post_shift - 1, tquotient, 1);
                        }else{
                            rtx t1, t2;

                            if (pre_shift >= BITS_PER_WORD || post_shift >= BITS_PER_WORD)
                              goto fail1;

                            t1 = mtcs_expmed_expand_shift(self,RSHIFT_EXPR, int_mode, op0,pre_shift, NULL_RTX, 1);
                            extra_cost= (mtcs_expmed_shift_cost/*!shift_cost*/(self,
                                  speed, int_mode, pre_shift)+ mtcs_expmed_shift_cost/*!shift_cost*/(self,speed, int_mode, post_shift));
                            t2 = expmed_mult_highpart(self,int_mode, t1,
                                  mtcs_rtl_gen_int_mode(mtcsRTL,ml, int_mode),NULL_RTX, 1, max_cost - extra_cost);
                            if (t2 == 0)
                              goto fail1;
                            quotient = mtcs_expmed_expand_shift(self,RSHIFT_EXPR, int_mode, t2,post_shift, tquotient, 1);
                        }
                    }
                }else        /* Too wide mode to use tricky code 结束  if (wi::popcount (wd) == 1){*/
                  break;

                insn = mtcs_rtl_data_get_last_insn (mtcsRtlData);
                if (insn != last)
                  mtcs_rtl_set_dst_reg_note/*!set_dst_reg_note*/(mtcsRTL,insn, REG_EQUAL,gen_rtx_UDIV (int_mode, op0, op1),quotient);
            }else{        /* TRUNC_DIV, signed */

                unsigned HOST_WIDE_INT ml;
                int post_shift;
                rtx mlr;
                HOST_WIDE_INT d = INTVAL (op1);
                unsigned HOST_WIDE_INT abs_d;

                /* Not prepared to handle division/remainder by
                   0xffffffffffffffff8000000000000000 etc.  */
                if (d == HOST_WIDE_INT_MIN && size > HOST_BITS_PER_WIDE_INT)
                  break;

                /* Since d might be INT_MIN, we have to cast to
                   unsigned HOST_WIDE_INT before negating to avoid
                   undefined signed overflow.  */
                abs_d = (d >= 0? (unsigned HOST_WIDE_INT) d : - (unsigned HOST_WIDE_INT) d);

                /* n rem d = n rem -d */
                if (rem_flag && d < 0){
                    d = abs_d;
                    op1 = mtcs_rtl_gen_int_mode(mtcsRTL,abs_d, int_mode);
                }

                if (d == 1)
                  quotient = op0;
                else if (d == -1)
                  quotient = mtcs_optabs_expand_unop(mtcsOptabs,int_mode, neg_optab, op0, tquotient, 0);
                else if (size <= HOST_BITS_PER_WIDE_INT && abs_d == HOST_WIDE_INT_1U << (size - 1)){
                    /* This case is not handled correctly below.  */
                    quotient = mtcs_expmed_emit_store_flag(self,tquotient, EQ, op0, op1,int_mode, 1, 1);
                    if (quotient == 0)
                      goto fail1;
                }else if (EXACT_POWER_OF_2_OR_ZERO_P (d) && (size <= HOST_BITS_PER_WIDE_INT || d >= 0)
                     && (rem_flag ? mtcs_expmed_smod_pow2_cheap/*!smod_pow2_cheap*/(self,speed, int_mode)
                           : mtcs_expmed_sdiv_pow2_cheap/*!sdiv_pow2_cheap*/(self,speed, int_mode))
                     /* We assume that cheap metric is true if the
                        optab has an expander for this mode.  */
                     && ((mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,
                           (rem_flag ? smod_optab : sdiv_optab),int_mode) != CODE_FOR_nothing)
                         || (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,sdivmod_optab, int_mode)!= CODE_FOR_nothing)))
                  ;
                else if (EXACT_POWER_OF_2_OR_ZERO_P (abs_d)){
                    if (rem_flag){
                        remainder = expand_smod_pow2(self,int_mode, op0, d);
                        if (remainder)
                          return gen_lowpart (mode, remainder);
                    }

                    if (mtcs_expmed_sdiv_pow2_cheap/*!sdiv_pow2_cheap*/(self,speed, int_mode)
                        && ((mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,sdiv_optab, int_mode)!= CODE_FOR_nothing)
                        || (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,sdivmod_optab, int_mode)!= CODE_FOR_nothing)))
                        quotient = mtcs_expmed_expand_divmod (self,0, TRUNC_DIV_EXPR,
                                int_mode, op0,mtcs_rtl_gen_int_mode (mtcsRTL,abs_d,int_mode),NULL_RTX, 0);
                    else
                      quotient = expand_sdiv_pow2(self,int_mode, op0, abs_d);

                    /* We have computed OP0 / abs(OP1).  If OP1 is negative,
                       negate the quotient.  */
                    if (d < 0){
                        insn = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
                        if (insn != last && abs_d < (HOST_WIDE_INT_1U << (HOST_BITS_PER_WIDE_INT - 1)))
                          mtcs_rtl_set_dst_reg_note/*!set_dst_reg_note*/(mtcsRTL,
                                insn, REG_EQUAL,gen_rtx_DIV (int_mode, op0,
                                      mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,abs_d,int_mode)),quotient);

                        quotient = mtcs_optabs_expand_unop(mtcsOptabs,int_mode, neg_optab,quotient, quotient, 0);
                    }
                }else if (size <= HOST_BITS_PER_WIDE_INT){
                    choose_multiplier (abs_d, size, size - 1, &ml, &post_shift);
                    if (ml < HOST_WIDE_INT_1U << (size - 1)){
                        rtx t1, t2, t3;

                        if (post_shift >= BITS_PER_WORD || size - 1 >= BITS_PER_WORD)
                          goto fail1;

                        extra_cost = (mtcs_expmed_shift_cost/*!shift_cost*/(self,speed, int_mode, post_shift)
                                  + mtcs_expmed_shift_cost/*!shift_cost*/(self,
                                        speed, int_mode, size - 1)+ mtcs_expmed_add_cost/*!add_cost*/(self,speed, int_mode));
                        t1 = expmed_mult_highpart(self,int_mode, op0,
                              mtcs_rtl_gen_int_mode(mtcsRTL,ml, int_mode),NULL_RTX, 0, max_cost - extra_cost);
                        if (t1 == 0)
                          goto fail1;
                        t2 = mtcs_expmed_expand_shift(self,RSHIFT_EXPR, int_mode, t1,
                           post_shift, NULL_RTX, 0);
                        t3 = mtcs_expmed_expand_shift(self,RSHIFT_EXPR, int_mode, op0,
                           size - 1, NULL_RTX, 0);
                        if (d < 0)
                          quotient= mtcs_expr_force_operand(mtcsExpr,gen_rtx_MINUS (int_mode, t3, t2),tquotient);
                        else
                          quotient = mtcs_expr_force_operand(mtcsExpr,gen_rtx_MINUS (int_mode, t2, t3),tquotient);
                    }else{
                        rtx t1, t2, t3, t4;

                        if (post_shift >= BITS_PER_WORD || size - 1 >= BITS_PER_WORD)
                          goto fail1;

                        ml |= HOST_WIDE_INT_M1U << (size - 1);
                        mlr = mtcs_rtl_gen_int_mode(mtcsRTL,ml, int_mode);
                        extra_cost = (mtcs_expmed_shift_cost/*!shift_cost*/(self,speed, int_mode, post_shift)
                                  + mtcs_expmed_shift_cost/*!shift_cost*/(self,speed, int_mode, size - 1) + 2 * mtcs_expmed_add_cost/*!add_cost*/(self,speed, int_mode));
                        t1 = expmed_mult_highpart(self,int_mode, op0, mlr,NULL_RTX, 0,max_cost - extra_cost);
                        if (t1 == 0)
                          goto fail1;
                        t2 = mtcs_expr_force_operand(mtcsExpr,gen_rtx_PLUS (int_mode, t1, op0),NULL_RTX);
                        t3 = mtcs_expmed_expand_shift(self,RSHIFT_EXPR, int_mode, t2,post_shift, NULL_RTX, 0);
                        t4 = mtcs_expmed_expand_shift(self,RSHIFT_EXPR, int_mode, op0,size - 1, NULL_RTX, 0);
                        if (d < 0)
                          quotient = mtcs_expr_force_operand(mtcsExpr,gen_rtx_MINUS (int_mode, t4, t3),tquotient);
                        else
                          quotient = mtcs_expr_force_operand(mtcsExpr,gen_rtx_MINUS (int_mode, t3, t4),tquotient);
                    }
                }else        /* Too wide mode to use tricky code */
                  break;

            insn = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
            if (insn != last)
              mtcs_rtl_set_dst_reg_note/*!set_dst_reg_note*/(mtcsRTL,insn, REG_EQUAL,gen_rtx_DIV (int_mode, op0, op1),quotient);
           }
            break;
        }
      fail1:
        mtcs_rtl_data_delete_insns_since (mtcsRtlData,last);
        break;

      case FLOOR_DIV_EXPR:
      case FLOOR_MOD_EXPR:
          /* We will come here only for signed operations.  */
        if (op1_is_constant && mtcs_mode_is_hwi_computable_p/*!HWI_COMPUTABLE_MODE_P*/(mtcsMode,compute_mode)){
            scalar_int_mode int_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,compute_mode);
            int size = mtcs_mode_get_bitsize(mtcsMode,int_mode);
            unsigned HOST_WIDE_INT mh, ml;
            int pre_shift, lgup, post_shift;
            HOST_WIDE_INT d = INTVAL (op1);
            if (d > 0){
                /* We could just as easily deal with negative constants here,
                   but it does not seem worth the trouble for GCC 2.6.  */
                if (EXACT_POWER_OF_2_OR_ZERO_P (d)){
                    pre_shift = floor_log2 (d);
                    if (rem_flag){
                        unsigned HOST_WIDE_INT mask= (HOST_WIDE_INT_1U << pre_shift) - 1;
                        remainder = mtcs_optabs_expand_binop(mtcsOptabs,int_mode, and_optab, op0,
                                mtcs_rtl_gen_int_mode(mtcsRTL,mask, int_mode),remainder, 0, methods);
                        if (remainder)
                          return gen_lowpart (mode, remainder);
                    }
                    quotient = mtcs_expmed_expand_shift(self,RSHIFT_EXPR, int_mode, op0,pre_shift, tquotient, 0);
                }else{
                    rtx t1, t2, t3, t4;
                    mh = choose_multiplier (d, size, size - 1,&ml, &post_shift);
                    gcc_assert (!mh);
                    if (post_shift < BITS_PER_WORD && size - 1 < BITS_PER_WORD){
                        t1 = mtcs_expmed_expand_shift(self,RSHIFT_EXPR, int_mode, op0,size - 1, NULL_RTX, 0);
                        t2 = mtcs_optabs_expand_binop(mtcsOptabs,int_mode, xor_optab, op0, t1, NULL_RTX, 0, OPTAB_WIDEN);
                        extra_cost = (mtcs_expmed_shift_cost/*!shift_cost*/(self,
                              speed, int_mode, post_shift) + mtcs_expmed_shift_cost/*!shift_cost*/(self,speed, int_mode, size - 1)
                                  + 2 * mtcs_expmed_add_cost/*!add_cost*/(self,speed, int_mode));
                        t3 = expmed_mult_highpart(self,int_mode, t2,
                              mtcs_rtl_gen_int_mode(mtcsRTL,ml, int_mode),NULL_RTX, 1, max_cost - extra_cost);
                        if (t3 != 0){
                            t4 = mtcs_expmed_expand_shift(self,RSHIFT_EXPR, int_mode, t3, post_shift, NULL_RTX, 1);
                            quotient = mtcs_optabs_expand_binop(mtcsOptabs,int_mode, xor_optab,
                                         t4, t1, tquotient, 0,OPTAB_WIDEN);
                        }
                    }
                }
            }else{// if (d > 0){
                rtx nsign, t1, t2, t3, t4;
                t1 = mtcs_expr_force_operand(mtcsExpr,gen_rtx_PLUS (int_mode,op0, constm1_rtx), NULL_RTX);
                t2 = mtcs_optabs_expand_binop(mtcsOptabs,int_mode, ior_optab, op0, t1, NULL_RTX,0, OPTAB_WIDEN);
                nsign = mtcs_expmed_expand_shift(self,RSHIFT_EXPR, int_mode, t2,size - 1, NULL_RTX, 0);
                t3 = mtcs_expr_force_operand(mtcsExpr,gen_rtx_MINUS (int_mode, t1, nsign),NULL_RTX);
                t4 = mtcs_expmed_expand_divmod (self,0, TRUNC_DIV_EXPR, int_mode, t3, op1,NULL_RTX, 0);
                if (t4){
                    rtx t5;
                    t5 = mtcs_optabs_expand_unop(mtcsOptabs,int_mode, one_cmpl_optab, nsign,NULL_RTX, 0);
                    quotient = mtcs_expr_force_operand(mtcsExpr,gen_rtx_PLUS (int_mode, t4, t5), tquotient);
                }
            }//end // if (d > 0){
        }//end if (op1_is_constant && mtcs_mode_is_hwi_computable_p/*!HWI_COMPUTABLE_MODE_P*/(mtcsMode,compute_mode)){

        if (quotient != 0)
          break;
        mtcs_rtl_data_delete_insns_since (mtcsRtlData,last);

        /* Try using an instruction that produces both the quotient and
           remainder, using truncation.  We can easily compensate the quotient
           or remainder to get floor rounding, once we have the remainder.
           Notice that we compute also the final remainder value here,
           and return the result right away.  */
        if (target == 0 || GET_MODE (target) != compute_mode)
          target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,compute_mode);

        if (rem_flag){
            remainder = REG_P (target) ? target : mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode);
            quotient = mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode);
        }else{
            quotient = REG_P (target) ? target : mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode);
            remainder = mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode);
        }

        if (mtcs_optabs_expand_twoval_binop/*!expand_twoval_binop*/(mtcsOptabs,
              sdivmod_optab, op0, op1,quotient, remainder, 0)){
            /* This could be computed with a branch-less sequence.
               Save that for later.  */
            rtx tem;
            rtx_code_label *label =mtcs_rtl_gen_label_rtx(mtcsRTL);
            do_cmp_and_jump(self,remainder, const0_rtx, EQ, compute_mode, label);
            tem = mtcs_optabs_expand_binop(mtcsOptabs,compute_mode, xor_optab, op0, op1,NULL_RTX, 0, OPTAB_WIDEN);
            do_cmp_and_jump(self,tem, const0_rtx, GE, compute_mode, label);
            mtcs_expmed_expand_dec(self,quotient, const1_rtx);
            mtcs_expmed_expand_inc(self,remainder, op1);
            mtcs_emit_emit_label(mtcsEmit,label);
            return gen_lowpart (mode, rem_flag ? remainder : quotient);
        }

        /* No luck with division elimination or divmod.  Have to do it
           by conditionally adjusting op0 *and* the result.  */
        {
          rtx_code_label *label1, *label2, *label3, *label4, *label5;
          rtx adjusted_op0;
          rtx tem;

          quotient = mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode);
          adjusted_op0 = mtcs_explow_copy_to_mode_reg(mtcsExplow,compute_mode, op0);
          label1 =mtcs_rtl_gen_label_rtx(mtcsRTL);
          label2 =mtcs_rtl_gen_label_rtx(mtcsRTL);
          label3 =mtcs_rtl_gen_label_rtx(mtcsRTL);
          label4 =mtcs_rtl_gen_label_rtx(mtcsRTL);
          label5 =mtcs_rtl_gen_label_rtx(mtcsRTL);
          do_cmp_and_jump(self,op1, const0_rtx, LT, compute_mode, label2);
          do_cmp_and_jump(self,adjusted_op0, const0_rtx, LT, compute_mode, label1);
          tem = mtcs_optabs_expand_binop(mtcsOptabs,compute_mode, sdiv_optab, adjusted_op0, op1,
                      quotient, 0, methods);
          if (tem != quotient)
            mtcs_expr_emit_move_insn(mtcsExpr,quotient, tem);
          mtcs_emit_emit_jump_insn(mtcsEmit,target_rtx_gen_jump/*!targetm.gen_jump*/(mtcsMachine->tmrtx,label5));
          mtcs_emit_emit_barrier(mtcsEmit);
          mtcs_emit_emit_label(mtcsEmit,label1);
          mtcs_expmed_expand_inc(self,adjusted_op0, const1_rtx);
          mtcs_emit_emit_jump_insn(mtcsEmit,target_rtx_gen_jump/*!targetm.gen_jump*/(mtcsMachine->tmrtx,label4));
          mtcs_emit_emit_barrier(mtcsEmit);
          mtcs_emit_emit_label(mtcsEmit,label2);
          do_cmp_and_jump(self,adjusted_op0, const0_rtx, GT, compute_mode, label3);
          tem = mtcs_optabs_expand_binop(mtcsOptabs,compute_mode, sdiv_optab, adjusted_op0, op1,
                      quotient, 0, methods);
          if (tem != quotient)
            mtcs_expr_emit_move_insn(mtcsExpr,quotient, tem);
          mtcs_emit_emit_jump_insn(mtcsEmit,target_rtx_gen_jump/*!targetm.gen_jump*/(mtcsMachine->tmrtx,label5));
          mtcs_emit_emit_barrier(mtcsEmit);
          mtcs_emit_emit_label(mtcsEmit,label3);
          mtcs_expmed_expand_dec(self,adjusted_op0, const1_rtx);
          mtcs_emit_emit_label(mtcsEmit,label4);
          tem = mtcs_optabs_expand_binop(mtcsOptabs,compute_mode, sdiv_optab, adjusted_op0, op1,
                      quotient, 0, methods);
          if (tem != quotient)
            mtcs_expr_emit_move_insn(mtcsExpr,quotient, tem);
          mtcs_expmed_expand_dec(self,quotient, const1_rtx);
          mtcs_emit_emit_label(mtcsEmit,label5);
        }
        break;

      case CEIL_DIV_EXPR:
      case CEIL_MOD_EXPR:
        if (unsignedp){
            if (op1_is_constant  && EXACT_POWER_OF_2_OR_ZERO_P (INTVAL (op1))
                && (mtcs_mode_is_hwi_computable_p/*!HWI_COMPUTABLE_MODE_P*/(mtcsMode,compute_mode)
                || INTVAL (op1) >= 0)) {
                scalar_int_mode int_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,compute_mode);
                rtx t1, t2, t3;
                unsigned HOST_WIDE_INT d = INTVAL (op1);
                t1 = mtcs_expmed_expand_shift(self,RSHIFT_EXPR, int_mode, op0,floor_log2 (d), tquotient, 1);
                t2 = mtcs_optabs_expand_binop(mtcsOptabs,int_mode, and_optab, op0,
                        mtcs_rtl_gen_int_mode(mtcsRTL,d - 1, int_mode),NULL_RTX, 1, methods);
                t3 = mtcs_emit_gen_reg_rtx(mtcsEmit,int_mode);
                t3 = mtcs_expmed_emit_store_flag (self,t3, NE, t2, const0_rtx, int_mode, 1, 1);
                if (t3 == 0){
                    rtx_code_label *lab;
                    lab =mtcs_rtl_gen_label_rtx(mtcsRTL);
                    do_cmp_and_jump(self,t2, const0_rtx, EQ, int_mode, lab);
                    mtcs_expmed_expand_inc(self,t1, const1_rtx);
                    mtcs_emit_emit_label(mtcsEmit,lab);
                    quotient = t1;
                }else
                  quotient = mtcs_expr_force_operand(mtcsExpr,gen_rtx_PLUS (int_mode, t1, t3),tquotient);
                break;
            }

            /* Try using an instruction that produces both the quotient and
               remainder, using truncation.  We can easily compensate the
               quotient or remainder to get ceiling rounding, once we have the
               remainder.  Notice that we compute also the final remainder
               value here, and return the result right away.  */
            if (target == 0 || GET_MODE (target) != compute_mode)
              target = mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode);

            if (rem_flag){
                remainder = (REG_P (target)? target : mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode));
                quotient = mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode);
            }else{
                quotient = (REG_P (target)? target : mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode));
                remainder = mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode);
            }

            if (mtcs_optabs_expand_twoval_binop/*!expand_twoval_binop*/(mtcsOptabs,
                  udivmod_optab, op0, op1, quotient,remainder, 1)){
                /* This could be computed with a branch-less sequence.
                   Save that for later.  */
                rtx_code_label *label =mtcs_rtl_gen_label_rtx(mtcsRTL);
                do_cmp_and_jump(self,remainder, const0_rtx, EQ,compute_mode, label);
                mtcs_expmed_expand_inc(self,quotient, const1_rtx);
                mtcs_expmed_expand_dec(self,remainder, op1);
                mtcs_emit_emit_label(mtcsEmit,label);
                return gen_lowpart (mode, rem_flag ? remainder : quotient);
            }

            /* No luck with division elimination or divmod.  Have to do it
               by conditionally adjusting op0 *and* the result.  */
            {
              rtx_code_label *label1, *label2;
              rtx adjusted_op0, tem;

              quotient = mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode);
              adjusted_op0 = mtcs_explow_copy_to_mode_reg(mtcsExplow,compute_mode, op0);
              label1 =mtcs_rtl_gen_label_rtx(mtcsRTL);
              label2 =mtcs_rtl_gen_label_rtx(mtcsRTL);
              do_cmp_and_jump(self,adjusted_op0, const0_rtx, NE,compute_mode, label1);
              mtcs_expr_emit_move_insn(mtcsExpr,quotient, const0_rtx);
              mtcs_emit_emit_jump_insn(mtcsEmit,target_rtx_gen_jump/*!targetm.gen_jump*/(mtcsMachine->tmrtx,label2));
              mtcs_emit_emit_barrier(mtcsEmit);
              mtcs_emit_emit_label(mtcsEmit,label1);
              mtcs_expmed_expand_dec(self,adjusted_op0, const1_rtx);
              tem = mtcs_optabs_expand_binop(mtcsOptabs,compute_mode, udiv_optab, adjusted_op0, op1,quotient, 1, methods);
              if (tem != quotient)
                  mtcs_expr_emit_move_insn(mtcsExpr,quotient, tem);
              mtcs_expmed_expand_inc(self,quotient, const1_rtx);
              mtcs_emit_emit_label(mtcsEmit,label2);
            }
        }else /* signed */{
            if (op1_is_constant && EXACT_POWER_OF_2_OR_ZERO_P (INTVAL (op1)) && INTVAL (op1) >= 0){
                /* This is extremely similar to the code for the unsigned case
                   above.  For 2.7 we should merge these variants, but for
                   2.6.1 I don't want to touch the code for unsigned since that
                   get used in C.  The signed case will only be used by other
                   languages (Ada).  */

                rtx t1, t2, t3;
                unsigned HOST_WIDE_INT d = INTVAL (op1);
                t1 = mtcs_expmed_expand_shift(self,RSHIFT_EXPR, compute_mode, op0,floor_log2 (d), tquotient, 0);
                t2 = mtcs_optabs_expand_binop(mtcsOptabs,compute_mode, and_optab, op0,mtcs_rtl_gen_int_mode(mtcsRTL,d - 1, compute_mode),
                           NULL_RTX, 1, methods);
                t3 = mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode);
                t3 = mtcs_expmed_emit_store_flag(self,t3, NE, t2, const0_rtx,compute_mode, 1, 1);
                if (t3 == 0){
                    rtx_code_label *lab;
                    lab =mtcs_rtl_gen_label_rtx(mtcsRTL);
                    do_cmp_and_jump(self,t2, const0_rtx, EQ, compute_mode, lab);
                    mtcs_expmed_expand_inc(self,t1, const1_rtx);
                    mtcs_emit_emit_label(mtcsEmit,lab);
                    quotient = t1;
                }else
                  quotient = mtcs_expr_force_operand(mtcsExpr,gen_rtx_PLUS (compute_mode,t1, t3),tquotient);
                break;
            }

            /* Try using an instruction that produces both the quotient and
               remainder, using truncation.  We can easily compensate the
               quotient or remainder to get ceiling rounding, once we have the
               remainder.  Notice that we compute also the final remainder
               value here, and return the result right away.  */
            if (target == 0 || GET_MODE (target) != compute_mode)
                target = mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode);
            if (rem_flag){
                remainder= (REG_P (target)? target : mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode));
                quotient = mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode);
            }else{
                quotient = (REG_P (target) ? target : mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode));
                remainder = mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode);
            }

            if (mtcs_optabs_expand_twoval_binop/*!expand_twoval_binop*/(mtcsOptabs,
                  sdivmod_optab, op0, op1, quotient,remainder, 0)){
                /* This could be computed with a branch-less sequence.
                   Save that for later.  */
                rtx tem;
                rtx_code_label *label =mtcs_rtl_gen_label_rtx(mtcsRTL);
                do_cmp_and_jump(self,remainder, const0_rtx, EQ,compute_mode, label);
                tem = mtcs_optabs_expand_binop(mtcsOptabs,compute_mode, xor_optab, op0, op1,NULL_RTX, 0, OPTAB_WIDEN);
                do_cmp_and_jump(self,tem, const0_rtx, LT, compute_mode, label);
                mtcs_expmed_expand_inc(self,quotient, const1_rtx);
                mtcs_expmed_expand_dec(self,remainder, op1);
                mtcs_emit_emit_label(mtcsEmit,label);
                return gen_lowpart (mode, rem_flag ? remainder : quotient);
            }

            /* No luck with division elimination or divmod.  Have to do it
               by conditionally adjusting op0 *and* the result.  */
            {
              rtx_code_label *label1, *label2, *label3, *label4, *label5;
              rtx adjusted_op0;
              rtx tem;

              quotient = mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode);
              adjusted_op0 = mtcs_explow_copy_to_mode_reg(mtcsExplow,compute_mode, op0);
              label1 =mtcs_rtl_gen_label_rtx(mtcsRTL);
              label2 =mtcs_rtl_gen_label_rtx(mtcsRTL);
              label3 =mtcs_rtl_gen_label_rtx(mtcsRTL);
              label4 =mtcs_rtl_gen_label_rtx(mtcsRTL);
              label5 =mtcs_rtl_gen_label_rtx(mtcsRTL);
              do_cmp_and_jump(self,op1, const0_rtx, LT, compute_mode, label2);
              do_cmp_and_jump(self,adjusted_op0, const0_rtx, GT,compute_mode, label1);
              tem = mtcs_optabs_expand_binop(mtcsOptabs,compute_mode, sdiv_optab, adjusted_op0, op1,
                      quotient, 0, methods);
              if (tem != quotient)
                  mtcs_expr_emit_move_insn(mtcsExpr,quotient, tem);
              mtcs_emit_emit_jump_insn(mtcsEmit,target_rtx_gen_jump/*!targetm.gen_jump*/(mtcsMachine->tmrtx,label5));
              mtcs_emit_emit_barrier(mtcsEmit);
              mtcs_emit_emit_label(mtcsEmit,label1);
              mtcs_expmed_expand_dec(self,adjusted_op0, const1_rtx);
              mtcs_emit_emit_jump_insn(mtcsEmit,target_rtx_gen_jump/*!targetm.gen_jump*/(mtcsMachine->tmrtx,label4));
              mtcs_emit_emit_barrier(mtcsEmit);
              mtcs_emit_emit_label(mtcsEmit,label2);
              do_cmp_and_jump(self,adjusted_op0, const0_rtx, LT,compute_mode, label3);
              tem = mtcs_optabs_expand_binop(mtcsOptabs,compute_mode, sdiv_optab, adjusted_op0, op1,
                      quotient, 0, methods);
              if (tem != quotient)
                  mtcs_expr_emit_move_insn(mtcsExpr,quotient, tem);
              mtcs_emit_emit_jump_insn(mtcsEmit,target_rtx_gen_jump/*!targetm.gen_jump*/(mtcsMachine->tmrtx,label5));
              mtcs_emit_emit_barrier(mtcsEmit);
              mtcs_emit_emit_label(mtcsEmit,label3);
              mtcs_expmed_expand_inc(self,adjusted_op0, const1_rtx);
              mtcs_emit_emit_label(mtcsEmit,label4);
              tem = mtcs_optabs_expand_binop(mtcsOptabs,compute_mode, sdiv_optab, adjusted_op0, op1,
                      quotient, 0, methods);
              if (tem != quotient)
                  mtcs_expr_emit_move_insn(mtcsExpr,quotient, tem);
              mtcs_expmed_expand_inc(self,quotient, const1_rtx);
              mtcs_emit_emit_label(mtcsEmit,label5);
            }
        }//end if (unsignedp){
        break;

      case EXACT_DIV_EXPR:
        if (op1_is_constant && mtcs_mode_is_hwi_computable_p/*!HWI_COMPUTABLE_MODE_P*/(mtcsMode,compute_mode)){
            scalar_int_mode int_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,compute_mode);
            int size = mtcs_mode_get_bitsize(mtcsMode,int_mode);
            HOST_WIDE_INT d = INTVAL (op1);
            unsigned HOST_WIDE_INT ml;
            int pre_shift;
            rtx t1;

            pre_shift = ctz_or_zero (d);
            ml = invert_mod2n (d >> pre_shift, size);
            t1 = mtcs_expmed_expand_shift(self,RSHIFT_EXPR, int_mode, op0,
                       pre_shift, NULL_RTX, unsignedp);
            quotient = mtcs_expmed_expand_mult/*!expand_mult*/(self,int_mode, t1, mtcs_rtl_gen_int_mode(mtcsRTL,ml, int_mode),
                        NULL_RTX, 1);

            insn = mtcs_rtl_data_get_last_insn (mtcsRtlData);
            mtcs_rtl_set_dst_reg_note/*!set_dst_reg_note*/(mtcsRTL,
                  insn, REG_EQUAL, gen_rtx_fmt_ee (unsignedp ? UDIV : DIV,int_mode, op0, op1),quotient);
        }
        break;

      case ROUND_DIV_EXPR:
      case ROUND_MOD_EXPR:
        if (unsignedp){
            scalar_int_mode int_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,compute_mode);
            rtx tem;
            rtx_code_label *label;
            label =mtcs_rtl_gen_label_rtx(mtcsRTL);
            quotient = mtcs_emit_gen_reg_rtx(mtcsEmit,int_mode);
            remainder = mtcs_emit_gen_reg_rtx(mtcsEmit,int_mode);
            if (mtcs_optabs_expand_twoval_binop/*!expand_twoval_binop*/(mtcsOptabs,
                  udivmod_optab, op0, op1, quotient, remainder, 1) == 0){
                rtx tem;
                quotient = mtcs_optabs_expand_binop(mtcsOptabs,int_mode, udiv_optab, op0, op1,quotient, 1, methods);
                tem = mtcs_expmed_expand_mult/*!expand_mult*/(self,int_mode, quotient, op1, NULL_RTX, 1);
                remainder = mtcs_optabs_expand_binop(mtcsOptabs,int_mode, sub_optab, op0, tem,remainder, 1, methods);
            }
            tem = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,int_mode, op1, -1);
            tem = mtcs_expmed_expand_shift(self,RSHIFT_EXPR, int_mode, tem, 1, NULL_RTX, 1);
            do_cmp_and_jump(self,remainder, tem, LEU, int_mode, label);
            mtcs_expmed_expand_inc(self,quotient, const1_rtx);
            mtcs_expmed_expand_dec(self,remainder, op1);
            mtcs_emit_emit_label(mtcsEmit,label);
        }else{
            scalar_int_mode int_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,compute_mode);
            int size = mtcs_mode_get_bitsize(mtcsMode,int_mode);
            rtx abs_rem, abs_op1, tem, mask;
            rtx_code_label *label;
            label =mtcs_rtl_gen_label_rtx(mtcsRTL);
            quotient = mtcs_emit_gen_reg_rtx(mtcsEmit,int_mode);
            remainder = mtcs_emit_gen_reg_rtx(mtcsEmit,int_mode);
            if (mtcs_optabs_expand_twoval_binop/*!expand_twoval_binop*/(mtcsOptabs,
                  sdivmod_optab, op0, op1, quotient, remainder, 0) == 0){
                rtx tem;
                quotient = mtcs_optabs_expand_binop(mtcsOptabs,int_mode, sdiv_optab, op0, op1,quotient, 0, methods);
                tem = mtcs_expmed_expand_mult/*!expand_mult*/(self,int_mode, quotient, op1, NULL_RTX, 0);
                remainder = mtcs_optabs_expand_binop(mtcsOptabs,int_mode, sub_optab, op0, tem,remainder, 0, methods);
            }
            abs_rem = mtcs_optabs_expand_abs/*!expand_abs*/(mtcsOptabs,int_mode, remainder, NULL_RTX, 1, 0);
            abs_op1 = mtcs_optabs_expand_abs/*!expand_abs*/(mtcsOptabs,int_mode, op1, NULL_RTX, 1, 0);
            tem = mtcs_expmed_expand_shift(self,LSHIFT_EXPR, int_mode, abs_rem, 1, NULL_RTX, 1);
            do_cmp_and_jump(self,tem, abs_op1, LTU, int_mode, label);
            tem = mtcs_optabs_expand_binop(mtcsOptabs,int_mode, xor_optab, op0, op1,NULL_RTX, 0, OPTAB_WIDEN);
            mask = mtcs_expmed_expand_shift(self,RSHIFT_EXPR, int_mode, tem, size - 1, NULL_RTX, 0);
            tem = mtcs_optabs_expand_binop(mtcsOptabs,int_mode, xor_optab, mask, const1_rtx, NULL_RTX, 0, OPTAB_WIDEN);
            tem = mtcs_optabs_expand_binop(mtcsOptabs,int_mode, sub_optab, tem, mask, NULL_RTX, 0, OPTAB_WIDEN);
            mtcs_expmed_expand_inc(self,quotient, tem);
            tem = mtcs_optabs_expand_binop(mtcsOptabs,int_mode, xor_optab, mask, op1, NULL_RTX, 0, OPTAB_WIDEN);
            tem = mtcs_optabs_expand_binop(mtcsOptabs,int_mode, sub_optab, tem, mask,NULL_RTX, 0, OPTAB_WIDEN);
            mtcs_expmed_expand_dec(self,remainder, tem);
            mtcs_emit_emit_label(mtcsEmit,label);
        }
        return gen_lowpart (mode, rem_flag ? remainder : quotient);
      default:
          gcc_unreachable ();
  }

  if (quotient == 0){
      if (target && GET_MODE (target) != compute_mode)
          target = 0;

      if (rem_flag){
          /* Try to produce the remainder without producing the quotient.
             If we seem to have a divmod pattern that does not require widening,
             don't try widening here.  We should really have a WIDEN argument
             to expand_twoval_binop, since what we'd really like to do here is
             1) try a mod insn in compute_mode
             2) try a divmod insn in compute_mode
             3) try a div insn in compute_mode and multiply-subtract to get
                remainder
             4) try the same things with widening allowed.  */
          remainder = mtcs_optabs_sign_expand_binop/*!sign_expand_binop*/(mtcsOptabs,
                compute_mode, umod_optab, smod_optab,    op0, op1, target,unsignedp,
                  ((mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,optab2, compute_mode) != CODE_FOR_nothing) ? OPTAB_DIRECT : OPTAB_WIDEN));
          if (remainder == 0){
              /* No luck there.  Can we do remainder and divide at once
             without a library call?  */
              remainder = mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode);
              if (! mtcs_optabs_expand_twoval_binop/*!expand_twoval_binop*/(mtcsOptabs,
                    (unsignedp ? udivmod_optab : sdivmod_optab),op0, op1, NULL_RTX, remainder, unsignedp))
                  remainder = 0;
          }

          if (remainder)
            return gen_lowpart (mode, remainder);
      }

      /* Produce the quotient.  Try a quotient insn, but not a library call.
     If we have a divmod in this mode, use it in preference to widening
     the div (for this test we assume it will not fail). Note that optab2
     is set to the one of the two optabs that the call below will use.  */
      quotient  = mtcs_optabs_sign_expand_binop/*!sign_expand_binop*/(mtcsOptabs,compute_mode, udiv_optab, sdiv_optab,op0, op1, rem_flag ? NULL_RTX : target,unsignedp,
                 ((mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,optab2, compute_mode)!= CODE_FOR_nothing) ? OPTAB_DIRECT : OPTAB_WIDEN));

      if (quotient == 0){
          /* No luck there.  Try a quotient-and-remainder insn,
             keeping the quotient alone.  */
          quotient = mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode);
          if (! mtcs_optabs_expand_twoval_binop/*!expand_twoval_binop*/(mtcsOptabs,
                unsignedp ? udivmod_optab : sdivmod_optab,op0, op1,quotient, NULL_RTX, unsignedp)){
              quotient = 0;
              if (! rem_flag)
                /* Still no luck.  If we are not computing the remainder,
                   use a library call for the quotient.  */
                quotient = mtcs_optabs_sign_expand_binop/*!sign_expand_binop*/(mtcsOptabs,compute_mode, udiv_optab, sdiv_optab,
                                  op0, op1, target,unsignedp, methods);
          }
      }
  }

  if (rem_flag){
      if (target && GET_MODE (target) != compute_mode)
          target = 0;

      if (quotient == 0){
          /* No divide instruction either.  Use library for remainder.  */
          remainder = mtcs_optabs_sign_expand_binop/*!sign_expand_binop*/(mtcsOptabs,compute_mode, umod_optab, smod_optab,
                         op0, op1, target,
                         unsignedp, methods);
          /* No remainder function.  Try a quotient-and-remainder
             function, keeping the remainder.  */
          if (!remainder && (methods == OPTAB_LIB || methods == OPTAB_LIB_WIDEN)){
              remainder = mtcs_emit_gen_reg_rtx(mtcsEmit,compute_mode);
              if (!expand_twoval_binop_libfunc (unsignedp ? udivmod_optab : sdivmod_optab,
                 op0, op1, NULL_RTX, remainder,unsignedp ? UMOD : MOD))
                  remainder = NULL_RTX;
          }
      }else{
          /* We divided.  Now finish doing X - Y * (X / Y).  */
          remainder = mtcs_expmed_expand_mult/*!expand_mult*/(self,compute_mode, quotient, op1, NULL_RTX, unsignedp);
          remainder = mtcs_optabs_expand_binop(mtcsOptabs,compute_mode, sub_optab, op0,
                        remainder, target, unsignedp, methods);
      }
  }

  if (methods != OPTAB_LIB_WIDEN  && (rem_flag ? remainder : quotient) == NULL_RTX)
    return NULL_RTX;

  return gen_lowpart (mode, rem_flag ? remainder : quotient);
}

/* Return a tree node with data type TYPE, describing the value of X.
   Usually this is an VAR_DECL, if there is no obvious better choice.
   X may be an expression, however we only support those expressions
   generated by loop.c.  */
//原型 make_tree expmed.h expmed.cc
tree mtcs_expmed_make_tree (MtcsExpmed *self,tree type, rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
  MtcsTree *mtcsTree = mtcs_target_get_tree(mtcsTarget);
  MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);

  tree t;

  switch (GET_CODE (x)){
    case CONST_INT:
    case CONST_WIDE_INT:
      t = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,type, mtcs_rtx_mode_t/*!rtx_mode_t*/(x, TYPE_MODE (type)));
      return t;

    case CONST_DOUBLE:
      gcc_assert/*!STATIC_ASSERT*/ (HOST_BITS_PER_WIDE_INT * 2 <= mtcs_mode_get_max_bitsize_mode_any_int/*!MAX_BITSIZE_MODE_ANY_INT*/(mtcsMode));
      if (mtcs_config_get_value/*!TARGET_SUPPORTS_WIDE_INT*/(mtcsConfig,MTCS_TARGET_SUPPORTS_WIDE_INT) == 0 && GET_MODE (x) == VOIDmode)
          t = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,
                type,wide_int::from_array (&CONST_DOUBLE_LOW (x), 2,HOST_BITS_PER_WIDE_INT * 2));
      else
          t = mtcs_tree_build_real/*!build_real*/(mtcsTree,type, *CONST_DOUBLE_REAL_VALUE (x));

      return t;

    case CONST_VECTOR:
      {
        unsigned int npatterns = CONST_VECTOR_NPATTERNS (x);
        unsigned int nelts_per_pattern = CONST_VECTOR_NELTS_PER_PATTERN (x);
        tree itype = TREE_TYPE (type);
        /* Build a tree with vector elements.  */
        tree_vector_builder elts (type, npatterns, nelts_per_pattern);
        unsigned int count = elts.encoded_nelts ();
        for (unsigned int i = 0; i < count; ++i){
            rtx elt = CONST_VECTOR_ELT (x, i);
            elts.quick_push (mtcs_expmed_make_tree(self,itype, elt));
        }
        return elts.build ();
      }

    case PLUS:
      return mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,PLUS_EXPR, type, mtcs_expmed_make_tree(self,type, XEXP (x, 0)),
              mtcs_expmed_make_tree(self,type, XEXP (x, 1)));

    case MINUS:
      return mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,MINUS_EXPR, type, mtcs_expmed_make_tree(self,type, XEXP (x, 0)),
              mtcs_expmed_make_tree(self,type, XEXP (x, 1)));

    case NEG:
      return fold_build1 (NEGATE_EXPR, type, mtcs_expmed_make_tree(self,type, XEXP (x, 0)));

    case MULT:
      return mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,MULT_EXPR, type, mtcs_expmed_make_tree(self,type, XEXP (x, 0)),
              mtcs_expmed_make_tree(self,type, XEXP (x, 1)));

    case ASHIFT:
      return mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,LSHIFT_EXPR, type, mtcs_expmed_make_tree(self,type, XEXP (x, 0)),
              mtcs_expmed_make_tree(self,type, XEXP (x, 1)));

    case LSHIFTRT:
      t = unsigned_type_for (type);
      return mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,type, build2 (RSHIFT_EXPR, t,
                         mtcs_expmed_make_tree(self,t, XEXP (x, 0)),
                         mtcs_expmed_make_tree(self,type, XEXP (x, 1))));

    case ASHIFTRT:
      t = signed_type_for (type);
      return mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,type, build2 (RSHIFT_EXPR, t,
                     mtcs_expmed_make_tree(self,t, XEXP (x, 0)),
                         mtcs_expmed_make_tree(self,type, XEXP (x, 1))));

    case DIV:
      if (TREE_CODE (type) != REAL_TYPE)
          t = signed_type_for (type);
      else
          t = type;

      return mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,type, build2 (TRUNC_DIV_EXPR, t,
                         mtcs_expmed_make_tree(self,t, XEXP (x, 0)),
                         mtcs_expmed_make_tree(self,t, XEXP (x, 1))));
    case UDIV:
      t = unsigned_type_for (type);
      return mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,type, build2 (TRUNC_DIV_EXPR, t,
                         mtcs_expmed_make_tree(self,t, XEXP (x, 0)),
                         mtcs_expmed_make_tree(self,t, XEXP (x, 1))));

    case SIGN_EXTEND:
    case ZERO_EXTEND:
      t = lang_hooks.types.type_for_mode (GET_MODE (XEXP (x, 0)),GET_CODE (x) == ZERO_EXTEND);
      return mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,type, mtcs_expmed_make_tree(self,t, XEXP (x, 0)));

    case CONST:
      return mtcs_expmed_make_tree(self,type, XEXP (x, 0));

    case SYMBOL_REF:
      t = SYMBOL_REF_DECL (x);
      if (t)
          return mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,type, mtcs_const_build_fold_addr_expr/*!build_fold_addr_expr*/(mtcsConst,t));
      /* fall through.  */

    default:
      if (CONST_POLY_INT_P (x))
          return mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,t, const_poly_int_value (x));

      t = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,RTL_LOCATION (x), VAR_DECL, NULL_TREE, type);

      /* If TYPE is a POINTER_TYPE, we might need to convert X from
     address mode to pointer mode.  */
      if (POINTER_TYPE_P (type))
          x = mtcs_explow_convert_memory_address_addr_space/*!convert_memory_address_addr_space*/
             (mtcsExplow,mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,type), x, TYPE_ADDR_SPACE (TREE_TYPE (type)));

      /* Note that we do *not* use SET_DECL_RTL here, because we do not
     want set_decl_rtl to go adjusting REG_ATTRS for this temporary.  */
      t->decl_with_rtl.rtl = x;

      return t;
  }
}



/* Choose the more appropiate immediate in scalar integer comparisons.  The
   purpose of this is to end up with an immediate which can be loaded into a
   register in fewer moves, if possible.

   For each integer comparison there exists an equivalent choice:
     i)   a >  b or a >= b + 1
     ii)  a <= b or a <  b + 1
     iii) a >= b or a >  b - 1
     iv)  a <  b or a <= b - 1

   MODE is the mode of the first operand.
   CODE points to the comparison code.
   IMM points to the rtx containing the immediate.  *IMM must satisfy
   CONST_SCALAR_INT_P on entry and continues to satisfy CONST_SCALAR_INT_P
   on exit.  */
//原型 canonicalize_comparison expmed.h expmed.cc
void mtcs_expmed_canonicalize_comparison (MtcsExpmed *self ,machine_mode mode, enum rtx_code *code, rtx *imm)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  if (!mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,mode))
    return;
  int to_add = 0;
  enum signop sgn = unsigned_condition_p (*code) ? UNSIGNED : SIGNED;

  /* Extract the immediate value from the rtx.  */
  wide_int imm_val = mtcs_rtx_mode_t/*!rtx_mode_t*/(*imm, (mtcs_mode)mode);
  if (*code == GT || *code == GTU || *code == LE || *code == LEU)
    to_add = 1;
  else if (*code == GE || *code == GEU || *code == LT || *code == LTU)
    to_add = -1;
  else
    return;

  /* Check for overflow/underflow in the case of signed values and
     wrapping around in the case of unsigned values.  If any occur
     cancel the optimization.  */
  wi::overflow_type overflow = wi::OVF_NONE;
  wide_int imm_modif;
  if (to_add == 1)
    imm_modif = wi::add (imm_val, 1, sgn, &overflow);
  else
    imm_modif = wi::sub (imm_val, 1, sgn, &overflow);

  if (overflow)
    return;

  /* The following creates a pseudo; if we cannot do that, bail out.  */
  if (!can_create_pseudo_p ())
    return;

  rtx reg = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mode, mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg) + 1);
  rtx new_imm = mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,imm_modif, mode);

  rtx_insn *old_rtx = mtcs_expr_gen_move_insn/*!gen_move_insn*/(mtcsExpr,reg, *imm);
  rtx_insn *new_rtx = mtcs_expr_gen_move_insn/*!gen_move_insn*/(mtcsExpr,reg, new_imm);

  /* Update the immediate and the code.  */
  if (mtcs_rtlanal_insn_cost/*!insn_cost*/(mtcsRtlanal,old_rtx, true) > mtcs_rtlanal_insn_cost/*!insn_cost*/(mtcsRtlanal,new_rtx, true)){
      *code = equivalent_cmp_code (*code);
      *imm = new_imm;
  }
}


/* Emit a store-flags instruction for comparison CODE on OP0 and OP1
   and storing in TARGET.  Normally return TARGET.
   Return 0 if that cannot be done.

   MODE is the mode to use for OP0 and OP1 should they be CONST_INTs.  If
   it is VOIDmode, they cannot both be CONST_INT.

   UNSIGNEDP is for the case where we have to widen the operands
   to perform the operation.  It says to use zero-extension.

   NORMALIZEP is 1 if we should convert the result to be either zero
   or one.  Normalize is -1 if we should convert the result to be
   either zero or -1.  If NORMALIZEP is zero, the result will be left
   "raw" out of the scc insn.  */
//原型 emit_store_flag expmed.h expmed.cc
rtx mtcs_expmed_emit_store_flag (MtcsExpmed *self,rtx target, enum rtx_code code, rtx op0, rtx op1,
         machine_mode mode, int unsignedp, int normalizep)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  machine_mode target_mode = target ? GET_MODE (target) : VOIDmode;
  enum rtx_code rcode;
  rtx subtarget;
  rtx tem, trueval;
  rtx_insn *last;

  /* If we compare constants, we shouldn't use a store-flag operation,
     but a constant load.  We can get there via the vanilla route that
     usually generates a compare-branch sequence, but will in this case
     fold the comparison to a constant, and thus elide the branch.  */
  if (CONSTANT_P (op0) && CONSTANT_P (op1))
    return NULL_RTX;

  tem = emit_store_flag_1 (self,target, code, op0, op1, mode, unsignedp, normalizep,target_mode);
  if (tem)
    return tem;

  /* If we reached here, we can't do this with a scc insn, however there
     are some comparisons that can be done in other ways.  Don't do any
     of these cases if branches are very cheap.  */
  if (BRANCH_COST (optimize_insn_for_speed_p (), false) == 0)
    return 0;

  /* See what we need to return.  We can only return a 1, -1, or the
     sign bit.  */

  if (normalizep == 0){
      if (STORE_FLAG_VALUE == 1 || STORE_FLAG_VALUE == -1)
          normalizep = STORE_FLAG_VALUE;
      else if (mtcs_simplify_rtx_val_signbit_p (mtcsSimplifyRtx,mode, STORE_FLAG_VALUE))
          ;
      else
          return 0;
  }

  last = mtcs_rtl_data_get_last_insn (mtcsRtlData);

  /* If optimizing, use different pseudo registers for each insn, instead
     of reusing the same pseudo.  This leads to better CSE, but slows
     down the compiler, since there are more pseudos.  */
  subtarget = (!optimize  && (target_mode == mode)) ? target : NULL_RTX;
  trueval = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,normalizep ? normalizep : STORE_FLAG_VALUE);

  /* For floating-point comparisons, try the reverse comparison or try
     changing the "orderedness" of the comparison.  */
  if (mtcs_mode_get_class/*!GET_MODE_CLASS*/ (mtcsMode,mode) == MODE_FLOAT){
      enum rtx_code first_code;
      bool and_them;

      rcode = reverse_condition_maybe_unordered (code);
      if (mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,rcode, mode, ccp_store_flag)
          && (code == ORDERED || code == UNORDERED
          || (! mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,mode) && (code == LTGT || code == UNEQ))
          || (! mtcs_mode_honor_snans/*!HONOR_SNANS*/ (mtcsMode,mode) && (code == EQ || code == NE)))) {
          int want_add = ((STORE_FLAG_VALUE == 1 && normalizep == -1)
                  || (STORE_FLAG_VALUE == -1 && normalizep == 1));

          /* For the reverse comparison, use either an addition or a XOR.  */
          if (want_add   && mtcs_rtlanal_rtx_cost/*!rtx_cost*/(mtcsRtlanal,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,normalizep),
                  mode, PLUS, 1, optimize_insn_for_speed_p ()) == 0){
              tem = emit_store_flag_1 (self,subtarget, rcode, op0, op1, mode, 0,STORE_FLAG_VALUE, target_mode);
              if (tem)
                return mtcs_optabs_expand_binop(mtcsOptabs,target_mode, add_optab, tem,
                        mtcs_rtl_gen_int_mode (mtcsRTL,normalizep, target_mode),target, 0, OPTAB_WIDEN);
          }else if (!want_add  && mtcs_rtlanal_rtx_cost/*!rtx_cost*/(mtcsRtlanal,
                  trueval, mode, XOR, 1,optimize_insn_for_speed_p ()) == 0){
              tem = emit_store_flag_1 (self,subtarget, rcode, op0, op1, mode, 0,normalizep, target_mode);
              if (tem)
                return mtcs_optabs_expand_binop(mtcsOptabs,target_mode,
                        xor_optab, tem, trueval,target, INTVAL (trueval) >= 0,OPTAB_WIDEN);
          }
      }

      mtcs_rtl_data_delete_insns_since (mtcsRtlData,last);

      /* Cannot split ORDERED and UNORDERED, only try the above trick.  */
      if (code == ORDERED || code == UNORDERED)
          return 0;

      and_them = mtcs_dojump_split_comparison (mtcsDojump,code, mode, &first_code, &code);

      /* If there are no NaNs, the first comparison should always fall through.
     Effectively change the comparison to the other one.  */
      if (!mtcs_mode_honor_nans/*!HONOR_NANS*/ (mtcsMode,mode)){
          gcc_assert (first_code == (and_them ? ORDERED : UNORDERED));
          return emit_store_flag_1 (self,target, code, op0, op1, mode, 0, normalizep,target_mode);
      }

      if (!HAVE_conditional_move)
          return 0;

      /* Do not turn a trapping comparison into a non-trapping one.  */
      if ((code != EQ && code != NE && code != UNEQ && code != LTGT)  && flag_trapping_math)
          return 0;

      /* Try using a setcc instruction for ORDERED/UNORDERED, followed by a
     conditional move.  */
      tem = emit_store_flag_1 (self,subtarget, first_code, op0, op1, mode, 0,normalizep, target_mode);
      if (tem == 0)
          return 0;

      if (and_them)
          tem = emit_conditional_move (target, { code, op0, op1, mode },tem, const0_rtx, GET_MODE (tem), 0);
      else
          tem = emit_conditional_move (target, { code, op0, op1, mode },trueval, tem, GET_MODE (tem), 0);

      if (tem == 0)
         mtcs_rtl_data_delete_insns_since (mtcsRtlData,last);
      return tem;
  }

  /* The remaining tricks only apply to integer comparisons.  */

  scalar_int_mode int_mode;
  if (mtcs_mode_is_int_mode (mtcsMode,mode, &int_mode))
    return mtcs_expmed_emit_store_flag_int (self,target, subtarget, code, op0, op1, int_mode,unsignedp, normalizep, trueval);

  return 0;
}

/* Like emit_store_flag, but always succeeds.  */
//原型 emit_store_flag_force expmed.h expmed.cc
rtx mtcs_expmed_emit_store_flag_force (MtcsExpmed *self,rtx target, enum rtx_code code, rtx op0, rtx op1,
               machine_mode mode, int unsignedp, int normalizep)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  rtx tem;
  rtx_code_label *label;
  rtx trueval, falseval;

  /* First see if emit_store_flag can do the job.  */
  tem = mtcs_expmed_emit_store_flag (self,target, code, op0, op1, mode, unsignedp, normalizep);
  if (tem != 0)
    return tem;

  /* If one operand is constant, make it the second one.  Only do this
     if the other operand is not constant as well.  */
  if (mtcs_rtlanal_swap_commutative_operands_p/*!swap_commutative_operands_p*/(mtcsRtlanal,op0, op1)){
      std::swap (op0, op1);
      code = swap_condition (code);
  }

  if (mode == VOIDmode)
    mode = GET_MODE (op0);

  if (!target)
    target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,word_mode);

  /* If this failed, we have to do this with set/compare/jump/set code.
     For foo != 0, if foo is in OP0, just replace it with 1 if nonzero.  */
  trueval = normalizep ? mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,normalizep) : const1_rtx;
  if (code == NE  && mtcs_mode_get_class(mtcsMode,mode) == MODE_INT
      && REG_P (target)  && op0 == target && op1 == const0_rtx){
      label =mtcs_rtl_gen_label_rtx(mtcsRTL);
      mtcs_dojump_do_compare_rtx_and_jump (mtcsDojump,target, const0_rtx, EQ, unsignedp, mode,
                   NULL_RTX, NULL, label,profile_probability::uninitialized ());
      mtcs_expr_emit_move_insn/*!emit_move_insn*/ (mtcsExpr,target, trueval);
      mtcs_emit_emit_label(mtcsEmit,label);
      return target;
  }

  if (!REG_P (target) || reg_mentioned_p (target, op0) || reg_mentioned_p (target, op1))
    target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/ (mtcsEmit,GET_MODE (target));

  /* Jump in the right direction if the target cannot implement CODE
     but can jump on its reverse condition.  */
  falseval = const0_rtx;
  if (!mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,code, mode, ccp_jump)
      && (! mtcs_mode_is_float_p(mtcsMode,mode)
          || code == ORDERED || code == UNORDERED
          || (! mtcs_mode_honor_nans/*!HONOR_NANS*/ (mtcsMode,mode) && (code == LTGT || code == UNEQ))
          || (! mtcs_mode_honor_snans/*!HONOR_SNANS*/ (mtcsMode,mode) && (code == EQ || code == NE)))){
      enum rtx_code rcode;
      if (mtcs_mode_is_float_p(mtcsMode,mode))
        rcode = reverse_condition_maybe_unordered (code);
      else
        rcode = reverse_condition (code);

      /* Canonicalize to UNORDERED for the libcall.  */
      if (mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,rcode, mode, ccp_jump)
          || (code == ORDERED && ! mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,ORDERED, mode, ccp_jump))) {
          falseval = trueval;
          trueval = const0_rtx;
          code = rcode;
      }
  }

  mtcs_expr_emit_move_insn (mtcsExpr,target, trueval);
  label =mtcs_rtl_gen_label_rtx(mtcsRTL);
  mtcs_dojump_do_compare_rtx_and_jump (mtcsDojump,op0, op1, code, unsignedp, mode, NULL_RTX, NULL,
               label, profile_probability::uninitialized ());

  mtcs_expr_emit_move_insn (mtcsExpr,target, falseval);
  mtcs_emit_emit_label(mtcsEmit,label);

  return target;
}

/* Output a shift instruction for expression code CODE,
   with SHIFTED being the rtx for the value to shift,
   and AMOUNT the amount to shift by.
   Store the result in the rtx TARGET, if that is convenient.
   If UNSIGNEDP is nonzero, do a logical shift; otherwise, arithmetic.
   Return the rtx for where the value is.  */
//原型 expand_shift expmed.h expmed.cc
rtx mtcs_expmed_expand_shift (MtcsExpmed *self,enum tree_code code, machine_mode mode, rtx shifted,
          poly_int64 amount, rtx target, int unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  return expand_shift_1 (self,code, mode, shifted,
             mtcs_rtl_gen_int_shift_amount (mtcsRTL,mode, amount),
             target, unsignedp);
}

/* Helper function for emit_store_flag.  */
//原型 emit_cstore expmed.h expmed.cc
rtx mtcs_expmed_emit_cstore (MtcsExpmed *self,rtx target, enum insn_code icode, enum rtx_code code,
         machine_mode mode, machine_mode compare_mode,int unsignedp, rtx x, rtx y, int normalizep,machine_mode target_mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

  class expand_operand ops[4];
  rtx op0, comparison, subtarget;
  rtx_insn *last;
  scalar_int_mode result_mode = mtcsTarget->/*!targetm.*/cstore_mode (mtcsTarget,icode);
  scalar_int_mode int_target_mode;

  last = mtcs_rtl_data_get_last_insn (mtcsRtlData);
  x = mtcs_optabs_prepare_operand (mtcsOptabs,icode, x, 2, mode, compare_mode, unsignedp);
  y = mtcs_optabs_prepare_operand (mtcsOptabs,icode, y, 3, mode, compare_mode, unsignedp);
  if (!x || !y){
      mtcs_rtl_data_delete_insns_since (mtcsRtlData,last);
      return NULL_RTX;
  }

  if (target_mode == VOIDmode)
    int_target_mode = result_mode;
  else
    int_target_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,target_mode);
  if (!target)
    target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,int_target_mode);

  comparison = gen_rtx_fmt_ee (code, result_mode, x, y);

  create_output_operand (&ops[0], optimize ? NULL_RTX : target, result_mode);
  create_fixed_operand (&ops[1], comparison);
  create_fixed_operand (&ops[2], x);
  create_fixed_operand (&ops[3], y);
  if (!mtcs_optabs_maybe_expand_insn (mtcsOptabs,icode, 4, ops)){
      mtcs_rtl_data_delete_insns_since (mtcsRtlData,last);
      return NULL_RTX;
  }
  subtarget = ops[0].value;

  /* If we are converting to a wider mode, first convert to
     INT_TARGET_MODE, then normalize.  This produces better combining
     opportunities on machines that have a SIGN_EXTRACT when we are
     testing a single bit.  This mostly benefits the 68k.

     If STORE_FLAG_VALUE does not have the sign bit set when
     interpreted in MODE, we can do this conversion as unsigned, which
     is usually more efficient.  */
  if (mtcs_mode_get_precision(mtcsMode,int_target_mode) > mtcs_mode_get_precision(mtcsMode,result_mode)){
      gcc_assert (mtcs_mode_get_precision(mtcsMode,result_mode) != 1
          || STORE_FLAG_VALUE == 1 || STORE_FLAG_VALUE == -1);

      bool unsignedp = (STORE_FLAG_VALUE >= 0);
      mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,target, subtarget, unsignedp);
      op0 = target;
      result_mode = int_target_mode;
  }else
      op0 = subtarget;

  /* If we want to keep subexpressions around, don't reuse our last
     target.  */
  if (optimize)
    subtarget = 0;

  /* Now normalize to the proper value in MODE.  Sometimes we don't
     have to do anything.  */
  if (normalizep == 0 || normalizep == STORE_FLAG_VALUE)
    ;
  /* STORE_FLAG_VALUE might be the most negative number, so write
     the comparison this way to avoid a compiler-time warning.  */
  else if (- normalizep == STORE_FLAG_VALUE)
    op0 = mtcs_optabs_expand_unop (mtcsOptabs,result_mode, neg_optab, op0, subtarget, 0);

  /* We don't want to use STORE_FLAG_VALUE < 0 below since this makes
     it hard to use a value of just the sign bit due to ANSI integer
     constant typing rules.  */
  else if (mtcs_simplify_rtx_val_signbit_known_set_p/*!val_signbit_known_set_p*/(mtcsSimplifyRtx,result_mode, STORE_FLAG_VALUE))
    op0 = mtcs_expmed_expand_shift (self,RSHIFT_EXPR, result_mode, op0,
            mtcs_mode_get_bitsize(mtcsMode,result_mode) - 1, subtarget,
            normalizep == 1);
  else{
      gcc_assert (STORE_FLAG_VALUE & 1);
      op0 = mtcs_expmed_expand_and/*!expand_and*/(self,result_mode, op0, const1_rtx, subtarget);
      if (normalizep == -1)
          op0 = mtcs_optabs_expand_unop (mtcsOptabs,result_mode, neg_optab, op0, op0, 0);
  }

  /* If we were converting to a smaller mode, do the conversion now.  */
  if (int_target_mode != result_mode){
      mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,target, op0, 0);
      return target;
  }else
    return op0;
}

/* Compute the logical-and of OP0 and OP1, storing it in TARGET
   and returning TARGET.

   If TARGET is 0, a pseudo-register or constant is returned.  */
//原型 expand_and expmed.h expmed.cc
rtx mtcs_expmed_expand_and (MtcsExpmed *self,machine_mode mode, rtx op0, rtx op1, rtx target)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

  rtx tem = 0;
  if (GET_MODE (op0) == VOIDmode && GET_MODE (op1) == VOIDmode)
    tem = mtcs_simplify_rtx_binary_operation/*!simplify_binary_operation*/(mtcsSimplifyRtx,AND, mode, op0, op1);
  n_debug("mtcsexpmed.c mtcs_expmed_expand_and 00 %d tem:%p target:%p\n",mode,tem,target);

  if (tem == 0)
    tem = mtcs_optabs_expand_binop/*!expand_binop*/ (mtcsOptabs,mode, and_optab, op0, op1, target, 0, OPTAB_LIB_WIDEN);
  n_debug("mtcsexpmed.c mtcs_expmed_expand_and 11 %d tem:%p\n",mode,tem);
  if (target == 0)
    target = tem;
  else if (tem != target)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/ (mtcsExpr,target, tem);
  return target;
}

/* Likewise, but return 0 if that cannot be done.  */
//原型 maybe_expand_shift expmed.h expmed.cc
rtx mtcs_expmed_maybe_expand_shift (MtcsExpmed *self,enum tree_code code, machine_mode mode, rtx shifted,int amount, rtx target, int unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  return expand_shift_1 (self,code, mode,
             shifted, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,amount), target, unsignedp, true);
}

/* Subroutine of emit_store_flag that handles cases in which the operands
   are scalar integers.  SUBTARGET is the target to use for temporary
   operations and TRUEVAL is the value to store when the condition is
   true.  All other arguments are as for emit_store_flag.  */
//原型 emit_store_flag_int expmed.h expmed.cc
rtx mtcs_expmed_emit_store_flag_int (MtcsExpmed *self,rtx target, rtx subtarget, enum rtx_code code, rtx op0,
             rtx op1, scalar_int_mode mode, int unsignedp,int normalizep, rtx trueval)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  machine_mode target_mode = target ? GET_MODE (target) : VOIDmode;
  rtx_insn *last = mtcs_rtl_data_get_last_insn (mtcsRtlData);

  /* If this is an equality comparison of integers, we can try to exclusive-or
     (or subtract) the two operands and use a recursive call to try the
     comparison with zero.  Don't do any of these cases if branches are
     very cheap.  */
  if ((code == EQ || code == NE) && op1 != const0_rtx){
      rtx tem = mtcs_optabs_expand_binop(mtcsOptabs,mode, xor_optab, op0, op1, subtarget, 1,OPTAB_WIDEN);

      if (tem == 0)
          tem = mtcs_optabs_expand_binop(mtcsOptabs,mode, sub_optab, op0, op1, subtarget, 1,OPTAB_WIDEN);
      if (tem != 0)
          tem = mtcs_expmed_emit_store_flag (self,target, code, tem, const0_rtx,mode, unsignedp, normalizep);
      if (tem != 0)
          return tem;

      mtcs_rtl_data_delete_insns_since (mtcsRtlData,last);
  }

  /* For integer comparisons, try the reverse comparison.  However, for
     small X and if we'd have anyway to extend, implementing "X != 0"
     as "-(int)X >> 31" is still cheaper than inverting "(int)X == 0".  */
  rtx_code rcode = reverse_condition (code);
  if (mtcs_optabs_can_compare_p/*!can_compare_p*/ (mtcsOptabs,rcode, mode, ccp_store_flag)
        && ! (mtcs_opinit_optab_handler(mtcsOpinit,cstore_optab, mode) == CODE_FOR_nothing
        && code == NE && mtcs_mode_get_size(mtcsMode,mode) < UNITS_PER_WORD && op1 == const0_rtx)){
      int want_add = ((STORE_FLAG_VALUE == 1 && normalizep == -1) || (STORE_FLAG_VALUE == -1 && normalizep == 1));

      /* Again, for the reverse comparison, use either an addition or a XOR.  */
      if (want_add  && mtcs_rtlanal_rtx_cost/*!rtx_cost*/(mtcsRtlanal,
              mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,normalizep), mode, PLUS, 1,optimize_insn_for_speed_p ()) == 0){
          rtx tem = emit_store_flag_1 (self,subtarget, rcode, op0, op1, mode, 0,STORE_FLAG_VALUE, target_mode);
          if (tem != 0)
            tem = mtcs_optabs_expand_binop(mtcsOptabs,target_mode, add_optab, tem,
                    mtcs_rtl_gen_int_mode (mtcsRTL,normalizep, target_mode),target, 0, OPTAB_WIDEN);
          if (tem != 0)
            return tem;
      }else if (!want_add && mtcs_rtlanal_rtx_cost/*!rtx_cost*/(mtcsRtlanal,
              trueval, mode, XOR, 1,optimize_insn_for_speed_p ()) == 0){
          rtx tem = emit_store_flag_1 (self,subtarget, rcode, op0, op1, mode, 0,normalizep, target_mode);
          if (tem != 0)
            tem = mtcs_optabs_expand_binop(mtcsOptabs,target_mode, xor_optab, tem, trueval, target,INTVAL (trueval) >= 0, OPTAB_WIDEN);
          if (tem != 0)
            return tem;
      }
      mtcs_rtl_data_delete_insns_since (mtcsRtlData,last);
  }
  /* Some other cases we can do are EQ, NE, LE, and GT comparisons with
     the constant zero.  Reject all other comparisons at this point.  Only
     do LE and GT if branches are expensive since they are expensive on
     2-operand machines.  */
  if (op1 != const0_rtx || (code != EQ && code != NE
      && (BRANCH_COST (optimize_insn_for_speed_p (), false) <= 1 || (code != LE && code != GT))))
    return 0;
  /* Try to put the result of the comparison in the sign bit.  Assume we can't
     do the necessary operation below.  */
  rtx tem = 0;
  /* To see if A <= 0, compute (A | (A - 1)).  A <= 0 iff that result has
     the sign bit set.  */
  if (code == LE){
      /* This is destructive, so SUBTARGET can't be OP0.  */
      if (rtx_equal_p (subtarget, op0))
          subtarget = 0;

      tem = mtcs_optabs_expand_binop(mtcsOptabs,mode, sub_optab, op0, const1_rtx, subtarget, 0,OPTAB_WIDEN);
      if (tem)
          tem = mtcs_optabs_expand_binop(mtcsOptabs,mode, ior_optab, op0, tem, subtarget, 0,OPTAB_WIDEN);
  }

  /* To see if A > 0, compute (((signed) A) << BITS) - A, where BITS is the
     number of bits in the mode of OP0, minus one.  */

  if (code == GT){
      if (rtx_equal_p (subtarget, op0))
          subtarget = 0;

      tem = mtcs_expmed_maybe_expand_shift (self,RSHIFT_EXPR, mode, op0,mtcs_mode_get_bitsize(mtcsMode,mode) - 1,subtarget, 0);
      if (tem)
          tem = mtcs_optabs_expand_binop(mtcsOptabs,mode, sub_optab, tem, op0, subtarget, 0,OPTAB_WIDEN);
  }

  if (code == EQ || code == NE){
      /* For EQ or NE, one way to do the comparison is to apply an operation
     that converts the operand into a positive number if it is nonzero
     or zero if it was originally zero.  Then, for EQ, we subtract 1 and
     for NE we negate.  This puts the result in the sign bit.  Then we
     normalize with a shift, if needed.

     Two operations that can do the above actions are ABS and FFS, so try
     them.  If that doesn't work, and MODE is smaller than a full word,
     we can use zero-extension to the wider mode (an unsigned conversion)
     as the operation.  */

      /* Note that ABS doesn't yield a positive number for INT_MIN, but
     that is compensated by the subsequent overflow when subtracting
     one / negating.  */

      if (mtcs_opinit_optab_handler(mtcsOpinit,abs_optab, mode) != CODE_FOR_nothing)
          tem = mtcs_optabs_expand_unop (mtcsOptabs,mode, abs_optab, op0, subtarget, 1);
      else if (mtcs_opinit_optab_handler(mtcsOpinit,ffs_optab, mode) != CODE_FOR_nothing)
          tem = mtcs_optabs_expand_unop (mtcsOptabs,mode, ffs_optab, op0, subtarget, 1);
      else if (mtcs_mode_get_size(mtcsMode,mode) < UNITS_PER_WORD){
          tem = mtcs_expr_convert_modes (mtcsExpr,word_mode, mode, op0, 1);
          mode = word_mode;
      }

      if (tem != 0){
          if (code == EQ)
            tem = mtcs_optabs_expand_binop(mtcsOptabs,mode, sub_optab, tem, const1_rtx, subtarget,0, OPTAB_WIDEN);
          else
            tem = mtcs_optabs_expand_unop (mtcsOptabs,mode, neg_optab, tem, subtarget, 0);
      }

      /* If we couldn't do it that way, for NE we can "or" the two's complement
     of the value with itself.  For EQ, we take the one's complement of
     that "or", which is an extra insn, so we only handle EQ if branches
     are expensive.  */
      if (tem == 0  && (code == NE || BRANCH_COST (optimize_insn_for_speed_p (),false) > 1)){
          if (rtx_equal_p (subtarget, op0))
            subtarget = 0;

          tem = mtcs_optabs_expand_unop (mtcsOptabs,mode, neg_optab, op0, subtarget, 0);
          tem = mtcs_optabs_expand_binop(mtcsOptabs,mode, ior_optab, tem, op0, subtarget, 0,OPTAB_WIDEN);

          if (tem && code == EQ)
            tem = mtcs_optabs_expand_unop (mtcsOptabs,mode, one_cmpl_optab, tem, subtarget, 0);
      }
  }

  if (tem && normalizep)
    tem = mtcs_expmed_maybe_expand_shift (self,RSHIFT_EXPR, mode, tem,
                  mtcs_mode_get_bitsize(mtcsMode,mode) - 1,subtarget, normalizep == 1);

  if (tem){
      if (!target)
          ;
      else if (GET_MODE (tem) != target_mode){
          mtcs_expr_convert_move (mtcsExpr,target, tem, 0);
          tem = target;
      }else if (!subtarget){
          mtcs_expr_emit_move_insn (mtcsExpr,target, tem);
          tem = target;
      }
  }else
      mtcs_rtl_data_delete_insns_since (mtcsRtlData,last);

  return tem;
}

/* Add INC into TARGET.  */
//原型 expand_inc rtl.h expmed.cc
void mtcs_expmed_expand_inc (MtcsExpmed *self,rtx target, rtx inc)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

  rtx value = mtcs_optabs_expand_binop/*!expand_binop*/ (mtcsOptabs,GET_MODE (target), add_optab,
                target, inc,target, 0, OPTAB_LIB_WIDEN);
  if (value != target)
      mtcs_expr_emit_move_insn (mtcsExpr,target, value);
}

/* Subtract DEC from TARGET.  */
//原型 expand_dec rtl.h expmed.cc
void mtcs_expmed_expand_dec (MtcsExpmed *self,rtx target, rtx dec)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

  rtx value = mtcs_optabs_expand_binop (mtcsOptabs,GET_MODE (target), sub_optab,
                target, dec,target, 0, OPTAB_LIB_WIDEN);
  if (value != target)
      mtcs_expr_emit_move_insn (mtcsExpr,target, value);
}

/* Emit code to adjust ADJ_OPERAND after multiplication of wrong signedness
   flavor of OP0 and OP1.  ADJ_OPERAND is already the high half of the
   product OP0 x OP1.  If UNSIGNEDP is nonzero, adjust the signed product
   to become unsigned, if UNSIGNEDP is zero, adjust the unsigned product to
   become signed.

   The result is put in TARGET if that is convenient.

   MODE is the mode of operation.  */
//原型 expand_mult_highpart_adjust expmed.h expmed.cc
rtx mtcs_expmed_expand_mult_highpart_adjust (MtcsExpmed *self,scalar_int_mode mode, rtx adj_operand, rtx op0,
                 rtx op1, rtx target, int unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

  rtx tem;
  enum rtx_code adj_code = unsignedp ? PLUS : MINUS;
  tem = mtcs_expmed_maybe_expand_shift (self,RSHIFT_EXPR, mode, op0,mtcs_mode_get_bitsize(mtcsMode,mode) - 1, NULL_RTX, 0);
  tem = mtcs_expmed_expand_and/*!expand_and*/(self,mode, tem, op1, NULL_RTX);
  adj_operand= mtcs_expr_force_operand (mtcsExpr,gen_rtx_fmt_ee (adj_code, mode, adj_operand, tem), adj_operand);

  tem = mtcs_expmed_maybe_expand_shift (self,RSHIFT_EXPR, mode, op1,mtcs_mode_get_bitsize(mtcsMode,mode) - 1, NULL_RTX, 0);
  tem = mtcs_expmed_expand_and/*!expand_and*/(self,mode, tem, op0, NULL_RTX);
  target = mtcs_expr_force_operand (mtcsExpr,gen_rtx_fmt_ee (adj_code, mode, adj_operand, tem),target);
  return target;
}

/* Find the cheapest way of multiplying a value of mode MODE by VAL.
   Try three variations:

       - a shift/add sequence based on VAL itself
       - a shift/add sequence based on -VAL, followed by a negation
       - a shift/add sequence based on VAL - 1, followed by an addition.

   Return true if the cheapest of these cost less than MULT_COST,
   describing the algorithm in *ALG and final fixup in *VARIANT.  */
//原型 choose_mult_variant expmed.h expmed.cc
bool mtcs_expmed_choose_mult_variant (MtcsExpmed *self,machine_mode mode, HOST_WIDE_INT val,
             struct algorithm *alg, int /*!enum mult_variant*/ *variant,int mult_cost)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  struct algorithm alg2;
  struct mult_cost limit;
  int op_cost;
  bool speed = optimize_insn_for_speed_p ();

  /* Fail quickly for impossible bounds.  */
  if (mult_cost < 0)
    return false;

  /* Ensure that mult_cost provides a reasonable upper bound.
     Any constant multiplication can be performed with less
     than 2 * bits additions.  */
  op_cost = 2 * mtcs_mode_get_unit_bitsize/*!GET_MODE_UNIT_BITSIZE*/(mtcsMode,mode)
              * mtcs_expmed_add_cost/*!add_cost*/(self,speed, mode);
  if (mult_cost > op_cost)
    mult_cost = op_cost;

  *variant = basic_variant;
  limit.cost = mult_cost;
  limit.latency = mult_cost;
  synth_mult (self,alg, val, &limit, mode);

  /* This works only if the inverted value actually fits in an
     `unsigned int' */
  if (HOST_BITS_PER_INT >= mtcs_mode_get_unit_bitsize (mtcsMode,mode)){
      op_cost = mtcs_expmed_neg_cost/*!neg_cost*/(self,speed, mode);
      if (MULT_COST_LESS (&alg->cost, mult_cost)){
          limit.cost = alg->cost.cost - op_cost;
          limit.latency = alg->cost.latency - op_cost;
      }else{
          limit.cost = mult_cost - op_cost;
          limit.latency = mult_cost - op_cost;
      }

      synth_mult (self,&alg2, -val, &limit, mode);
      alg2.cost.cost += op_cost;
      alg2.cost.latency += op_cost;
      if (CHEAPER_MULT_COST (&alg2.cost, &alg->cost))
          *alg = alg2, *variant = negate_variant;
  }

  /* This proves very useful for division-by-constant.  */
  op_cost = mtcs_expmed_add_cost/*!add_cost*/(self,speed, mode);
  if (MULT_COST_LESS (&alg->cost, mult_cost)){
      limit.cost = alg->cost.cost - op_cost;
      limit.latency = alg->cost.latency - op_cost;
  }else{
      limit.cost = mult_cost - op_cost;
      limit.latency = mult_cost - op_cost;
  }

  if (val != HOST_WIDE_INT_MIN|| mtcs_mode_get_unit_precision/*!GET_MODE_UNIT_PRECISION*/(mtcsMode,mode) == HOST_BITS_PER_WIDE_INT){
      synth_mult (self,&alg2, val - HOST_WIDE_INT_1U, &limit, mode);
      alg2.cost.cost += op_cost;
      alg2.cost.latency += op_cost;
      if (CHEAPER_MULT_COST (&alg2.cost, &alg->cost))
          *alg = alg2, *variant = add_variant;
  }

  return MULT_COST_LESS (&alg->cost, mult_cost);
}


/* Subroutine of {set_,}zero_cost.  Not to be used otherwise.  */
//原型 set_zero_cost expmed.h
int *mtcs_expmed_zero_cost_ptr (MtcsExpmed *self,bool speed)
{
  return &self->x_zero_cost[speed];
}

/* Set the COST of loading zero when optimizing for SPEED.  */
//原型 set_zero_cost expmed.h
void mtcs_expmed_set_zero_cost (MtcsExpmed *self,bool speed, int cost)
{
  *mtcs_expmed_zero_cost_ptr (self,speed) = cost;
}

/* Return the COST of loading zero when optimizing for SPEED.  */
//原型 zero_cost expmed.h
int mtcs_expmed_zero_cost (MtcsExpmed *self,bool speed)
{
  return *mtcs_expmed_zero_cost_ptr/*!zero_cost_ptr*/(self,speed);
}


/* Compute an index into the cost arrays by mode class.  */
//原型 expmed_mode_index expmed.h
int mtcs_expmed_mode_index (MtcsExpmed *self,machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
//#define NUM_MODE_IP_INT (NUM_MODE_INT + NUM_MODE_PARTIAL_INT)
//#define NUM_MODE_IPV_INT (NUM_MODE_IP_INT + NUM_MODE_VECTOR_INT)
  // n_debug("mtcsexpmed.c mtcs_expmed_mode_index NUM_MODE_INT:%d parital:%d vector:%d\n",
    //     mtcsMode->modesNum.num_INT,mtcsMode->modesNum.num_PARTIAL_INT,mtcsMode->modesNum.num_VECTOR_INT);
   switch (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode)){
      case MODE_INT:
         return mode - mtcsMode->modesMinMax.min_INT;
      case MODE_PARTIAL_INT:
         /* If there are no partial integer modes, help the compiler
         to figure out this will never happen.  See PR59934.  */
         if (mtcsMode->modesMinMax.min_PARTIAL_INT != VOIDmode)
            return mode - mtcsMode->modesMinMax.min_PARTIAL_INT + mtcsMode->modesNum.num_INT;
         break;
      case MODE_VECTOR_INT:
         /* If there are no vector integer modes, help the compiler
         to figure out this will never happen.  See PR59934.  */
         if (mtcsMode->modesMinMax.min_VECTOR_INT/*!MIN_MODE_VECTOR_INT*/ != VOIDmode)
            return mode - mtcsMode->modesMinMax.min_VECTOR_INT/*!MIN_MODE_VECTOR_INT*/
                  + mtcs_mode_get_num_ip_int/*!NUM_MODE_IP_INT*/(mtcsMode);
         break;
      default:
         break;
   }
   gcc_unreachable ();
}



/* Return a pointer to a boolean contained in EOC indicating whether
   a particular operation performed in MODE is cheap when optimizing
   for SPEED.  */
//原型 expmed_op_cheap_ptr expmed.h
bool *mtcs_expmed_op_cheap_ptr (MtcsExpmed *self,struct mtcs_expmed_op_cheap *eoc, bool speed, machine_mode mode)
{
  int idx = mtcs_expmed_mode_index/*!expmed_mode_index*/(self,mode);
  n_debug("mtcsexpmed.c mtcs_expmed_op_cheap_ptr mode:%d idx:%d\n",mode,idx);
  if(idx>6)
     exit(0);
  return &eoc->cheap[speed][idx];
}

/* Return a pointer to a cost contained in COSTS when a particular
   operation is performed in MODE when optimizing for SPEED.  */
//原型 expmed_op_cost_ptr expmed.h
int *mtcs_expmed_op_cost_ptr (MtcsExpmed *self,struct mtcs_expmed_op_costs *costs, bool speed,machine_mode mode)
{
  int idx =mtcs_expmed_mode_index/*!expmed_mode_index*/(self,mode);
  n_debug("mtcsexpmed.c mtcs_expmed_op_cost_ptr mode:%d idx:%d\n",mode,idx);
  if(idx>6)
       exit(0);
  return &costs->cost[speed][idx];
}

/* Subroutine of {set_,}add_cost.  Not to be used otherwise.  */
//原型 add_cost_ptr expmed.h
int *mtcs_expmed_add_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode)
{
  return mtcs_expmed_op_cost_ptr (self,&self/*this_target_expmed*/->x_add_cost, speed, mode);
}

/* Set the COST of computing an add in MODE when optimizing for SPEED.  */
//原型 set_add_cost expmed.h
void mtcs_expmed_set_add_cost (MtcsExpmed *self,bool speed, machine_mode mode, int cost)
{
  *mtcs_expmed_add_cost_ptr(self,speed, mode) = cost;
}

/* Return the cost of computing an add in MODE when optimizing for SPEED.  */
//原型 add_cost expmed.h
int mtcs_expmed_add_cost (MtcsExpmed *self,bool speed, machine_mode mode)
{
  return *mtcs_expmed_add_cost_ptr (self,speed, mode);
}

/* Subroutine of {set_,}neg_cost.  Not to be used otherwise.  */
//原型 neg_cost_ptr expmed.h
int *mtcs_expmed_neg_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode)
{
  return mtcs_expmed_op_cost_ptr (self,&self/*!this_target_expmed*/->x_neg_cost, speed, mode);
}

/* Set the COST of computing a negation in MODE when optimizing for SPEED.  */
//原型 set_neg_cost expmed.h
void mtcs_expmed_set_neg_cost (MtcsExpmed *self,bool speed, machine_mode mode, int cost)
{
  *mtcs_expmed_neg_cost_ptr (self,speed, mode) = cost;
}

/* Return the cost of computing a negation in MODE when optimizing for
   SPEED.  */
//原型 neg_cost expmed.h
int mtcs_expmed_neg_cost (MtcsExpmed *self,bool speed, machine_mode mode)
{
  return *mtcs_expmed_neg_cost_ptr (self,speed, mode);
}

/* Subroutine of {set_,}mul_cost.  Not to be used otherwise.  */
//原型 expmed_mul_cost_ptr expmed.h
int *mtcs_expmed_mul_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode)
{
  return mtcs_expmed_op_cost_ptr (self,&self/*!this_target_expmed*/->x_mul_cost, speed, mode);
}

/* Set the COST of doing a multiplication in MODE when optimizing for
   SPEED.  */
//原型 set_mul_cost expmed.h
void mtcs_expmed_set_mul_cost (MtcsExpmed *self,bool speed, machine_mode mode, int cost)
{
  *mtcs_expmed_mul_cost_ptr (self,speed, mode) = cost;
}

/* Return the cost of doing a multiplication in MODE when optimizing
   for SPEED.  */
//原型 mul_cost expmed.h
int mtcs_expmed_mul_cost (MtcsExpmed *self,bool speed, machine_mode mode)
{
  return *mtcs_expmed_mul_cost_ptr (self,speed, mode);
}

/* Subroutine of {set_,}sdiv_cost.  Not to be used otherwise.  */
//原型 expmed_sdiv_cost_ptr expmed.h
int *mtcs_expmed_sdiv_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode)
{
  return mtcs_expmed_op_cost_ptr (self,&self/*!this_target_expmed*/->x_sdiv_cost, speed, mode);
}

/* Set the COST of doing a signed division in MODE when optimizing
   for SPEED.  */
//原型 set_sdiv_cost expmed.h
void mtcs_expmed_set_sdiv_cost (MtcsExpmed *self,bool speed, machine_mode mode, int cost)
{
  *mtcs_expmed_sdiv_cost_ptr (self,speed, mode) = cost;
}

/* Return the cost of doing a signed division in MODE when optimizing
   for SPEED.  */
//原型 sdiv_cost expmed.h
int mtcs_expmed_sdiv_cost (MtcsExpmed *self,bool speed, machine_mode mode)
{
  return *mtcs_expmed_sdiv_cost_ptr (self,speed, mode);
}

/* Subroutine of {set_,}udiv_cost.  Not to be used otherwise.  */
//原型 udiv_cost_ptr expmed.h
int *mtcs_expmed_udiv_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode)
{
  return mtcs_expmed_op_cost_ptr (self,&self/*!this_target_expmed*/->x_udiv_cost, speed, mode);
}

/* Set the COST of doing an unsigned division in MODE when optimizing
   for SPEED.  */
//原型 set_udiv_cost expmed.h
void mtcs_expmed_set_udiv_cost (MtcsExpmed *self,bool speed, machine_mode mode, int cost)
{
  *mtcs_expmed_udiv_cost_ptr (self,speed, mode) = cost;
}

/* Return the cost of doing an unsigned division in MODE when
   optimizing for SPEED.  */
//原型 udiv_cost expmed.h
int mtcs_expmed_udiv_cost (MtcsExpmed *self,bool speed, machine_mode mode)
{
  return *mtcs_expmed_udiv_cost_ptr (self,speed, mode);
}

/* Subroutine of {set_,}sdiv_pow2_cheap.  Not to be used otherwise.  */
//原型 sdiv_pow2_cheap_ptr expmed.h
bool *mtcs_expmed_sdiv_pow2_cheap_ptr (MtcsExpmed *self,bool speed, machine_mode mode)
{
  return mtcs_expmed_op_cheap_ptr (self,&self/*!this_target_expmed*/->x_sdiv_pow2_cheap,speed, mode);
}

/* Set whether a signed division by a power of 2 is cheap in MODE
   when optimizing for SPEED.  */
//原型 set_sdiv_pow2_cheap expmed.h
void mtcs_expmed_set_sdiv_pow2_cheap (MtcsExpmed *self,bool speed, machine_mode mode, bool cheap_p)
{
  *mtcs_expmed_sdiv_pow2_cheap_ptr (self,speed, mode) = cheap_p;
}

/* Return whether a signed division by a power of 2 is cheap in MODE
   when optimizing for SPEED.  */
//原型 sdiv_pow2_cheap expmed.h
bool mtcs_expmed_sdiv_pow2_cheap (MtcsExpmed *self,bool speed, machine_mode mode)
{
  return *mtcs_expmed_sdiv_pow2_cheap_ptr (self,speed, mode);
}

/* Subroutine of {set_,}smod_pow2_cheap.  Not to be used otherwise.  */
//原型 smod_pow2_cheap_ptr expmed.h
bool *mtcs_expmed_smod_pow2_cheap_ptr (MtcsExpmed *self,bool speed, machine_mode mode)
{
  return mtcs_expmed_op_cheap_ptr (self,&self/*!this_target_expmed*/->x_smod_pow2_cheap,
                  speed, mode);
}

/* Set whether a signed modulo by a power of 2 is CHEAP in MODE when
   optimizing for SPEED.  */
//原型 set_smod_pow2_cheap expmed.h

void mtcs_expmed_set_smod_pow2_cheap (MtcsExpmed *self,bool speed, machine_mode mode, bool cheap)
{
  *mtcs_expmed_smod_pow2_cheap_ptr (self,speed, mode) = cheap;
}

/* Return whether a signed modulo by a power of 2 is cheap in MODE
   when optimizing for SPEED.  */
//原型 smod_pow2_cheap expmed.h
bool mtcs_expmed_smod_pow2_cheap (MtcsExpmed *self,bool speed, machine_mode mode)
{
  return *mtcs_expmed_smod_pow2_cheap_ptr (self,speed, mode);
}

/* Subroutine of {set_,}shift_cost.  Not to be used otherwise.  */
//原型 shift_cost_ptr expmed.h
int *mtcs_expmed_shift_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode, int bits)
{
  int midx = mtcs_expmed_mode_index (self,mode);
  return &self/*!this_target_expmed*/->x_shift_cost[speed][midx][bits];
}

/* Set the COST of doing a shift in MODE by BITS when optimizing for SPEED.  */
//原型 set_shift_cost expmed.h
void mtcs_expmed_set_shift_cost (MtcsExpmed *self,bool speed, machine_mode mode, int bits, int cost)
{
  *mtcs_expmed_shift_cost_ptr (self,speed, mode, bits) = cost;
}

/* Return the cost of doing a shift in MODE by BITS when optimizing for
   SPEED.  */
//原型 shift_cost expmed.h
int mtcs_expmed_shift_cost (MtcsExpmed *self,bool speed, machine_mode mode, int bits)
{
  return *mtcs_expmed_shift_cost_ptr (self,speed, mode, bits);
}

/* Subroutine of {set_,}shiftadd_cost.  Not to be used otherwise.  */
//原型 shiftadd_cost_ptr expmed.h

int *mtcs_expmed_shiftadd_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode, int bits)
{
  int midx = mtcs_expmed_mode_index (self,mode);
  return &self/*!this_target_expmed*/->x_shiftadd_cost[speed][midx][bits];
}

/* Set the COST of doing a shift in MODE by BITS followed by an add when
   optimizing for SPEED.  */
//原型 set_shiftadd_cost expmed.h

void mtcs_expmed_set_shiftadd_cost (MtcsExpmed *self,bool speed, machine_mode mode, int bits, int cost)
{
  *mtcs_expmed_shiftadd_cost_ptr (self,speed, mode, bits) = cost;
}

/* Return the cost of doing a shift in MODE by BITS followed by an add
   when optimizing for SPEED.  */
//原型 shiftadd_cost expmed.h

int mtcs_expmed_shiftadd_cost (MtcsExpmed *self,bool speed, machine_mode mode, int bits)
{
  return *mtcs_expmed_shiftadd_cost_ptr (self,speed, mode, bits);
}

/* Subroutine of {set_,}shiftsub0_cost.  Not to be used otherwise.  */
//原型 shiftsub0_cost_ptr expmed.h
int *mtcs_expmed_shiftsub0_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode, int bits)
{
  int midx = mtcs_expmed_mode_index (self,mode);
  return &self->x_shiftsub0_cost[speed][midx][bits];
}

/* Set the COST of doing a shift in MODE by BITS and then subtracting a
   value when optimizing for SPEED.  */
//原型 set_shiftsub0_cost expmed.h
void mtcs_expmed_set_shiftsub0_cost (MtcsExpmed *self,bool speed, machine_mode mode, int bits, int cost)
{
  *mtcs_expmed_shiftsub0_cost_ptr (self,speed, mode, bits) = cost;
}

/* Return the cost of doing a shift in MODE by BITS and then subtracting
   a value when optimizing for SPEED.  */
//原型 shiftsub0_cost expmed.h
int mtcs_expmed_shiftsub0_cost (MtcsExpmed *self,bool speed, machine_mode mode, int bits)
{
  return *mtcs_expmed_shiftsub0_cost_ptr (self,speed, mode, bits);
}

/* Subroutine of {set_,}shiftsub1_cost.  Not to be used otherwise.  */
//原型 shiftsub1_cost_ptr expmed.h
int *mtcs_expmed_shiftsub1_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode, int bits)
{
  int midx = mtcs_expmed_mode_index (self,mode);
  return &self->x_shiftsub1_cost[speed][midx][bits];
}

/* Set the COST of subtracting a shift in MODE by BITS from a value when
   optimizing for SPEED.  */
//原型 set_shiftsub1_cost expmed.h
void mtcs_expmed_set_shiftsub1_cost (MtcsExpmed *self,bool speed, machine_mode mode, int bits, int cost)
{
  *mtcs_expmed_shiftsub1_cost_ptr (self,speed, mode, bits) = cost;
}

/* Return the cost of subtracting a shift in MODE by BITS from a value
   when optimizing for SPEED.  */
//原型 shiftsub1_cost expmed.h
int mtcs_expmed_shiftsub1_cost (MtcsExpmed *self,bool speed, machine_mode mode, int bits)
{
  return *mtcs_expmed_shiftsub1_cost_ptr (self,speed, mode, bits);
}

/* Subroutine of {set_,}mul_widen_cost.  Not to be used otherwise.  */
//原型 mul_widen_cost_ptr expmed.h
int *mtcs_expmed_mul_widen_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  gcc_assert (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) == MODE_INT);
  return &self/*!this_target_expmed*/->x_mul_widen_cost[speed][mode - mtcsMode->modesMinMax.min_INT/*!MIN_MODE_INT*/];
}

/* Set the COST for computing a widening multiplication in MODE when
   optimizing for SPEED.  */
//原型 set_mul_widen_cost expmed.h
void mtcs_expmed_set_mul_widen_cost (MtcsExpmed *self,bool speed, machine_mode mode, int cost)
{
  *mtcs_expmed_mul_widen_cost_ptr (self,speed, mode) = cost;
}

/* Return the cost for computing a widening multiplication in MODE when
   optimizing for SPEED.  */
//原型 mul_widen_cost expmed.h
int mtcs_expmed_mul_widen_cost (MtcsExpmed *self,bool speed, machine_mode mode)
{
  return *mtcs_expmed_mul_widen_cost_ptr (self,speed, mode);
}

/* Subroutine of {set_,}mul_highpart_cost.  Not to be used otherwise.  */
//原型 mul_highpart_cost_ptr expmed.h
int *mtcs_expmed_mul_highpart_cost_ptr (MtcsExpmed *self,bool speed, machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  gcc_assert (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) == MODE_INT);
  int m = mode - mtcsMode->modesMinMax.min_INT/*!MIN_MODE_INT*/;
  gcc_assert (m <mtcsMode->modesNum.num_INT/*!NUM_MODE_INT*/);
  return &self/*!this_target_expmed*/->x_mul_highpart_cost[speed][m];
}

/* Set the COST for computing the high part of a multiplication in MODE
   when optimizing for SPEED.  */
//原型 set_mul_highpart_cost expmed.h
void mtcs_expmed_set_mul_highpart_cost (MtcsExpmed *self,bool speed, machine_mode mode, int cost)
{
  *mtcs_expmed_mul_highpart_cost_ptr (self,speed, mode) = cost;
}

/* Return the cost for computing the high part of a multiplication in MODE
   when optimizing for SPEED.  */
//原型 mul_highpart_cost expmed.h
int mtcs_expmed_mul_highpart_cost (MtcsExpmed *self,bool speed, machine_mode mode)
{
  return *mtcs_expmed_mul_highpart_cost_ptr (self,speed, mode);
}

/* Subroutine of {set_,}convert_cost.  Not to be used otherwise.  */
//原型 convert_cost_ptr expmed.h
int *mtcs_expmed_convert_cost_ptr (MtcsExpmed *self,machine_mode to_mode, machine_mode from_mode,bool speed)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  int to_idx = mtcs_expmed_mode_index (self,to_mode);
  int from_idx = mtcs_expmed_mode_index (self,from_mode);

  gcc_assert (IN_RANGE (to_idx, 0, mtcs_mode_get_num_ip_int/*!NUM_MODE_IP_INT*/(mtcsMode) - 1));
  gcc_assert (IN_RANGE (from_idx, 0, mtcs_mode_get_num_ip_int/*!NUM_MODE_IP_INT*/(mtcsMode) - 1));

  return &self/*!this_target_expmed*/->x_convert_cost[speed][to_idx][from_idx];
}

/* Set the COST for converting from FROM_MODE to TO_MODE when optimizing
   for SPEED.  */
//原型 set_convert_cost expmed.h
void mtcs_expmed_set_convert_cost (MtcsExpmed *self,machine_mode to_mode, machine_mode from_mode, bool speed, int cost)
{
  *mtcs_expmed_convert_cost_ptr (self,to_mode, from_mode, speed) = cost;
}

/* Return the cost for converting from FROM_MODE to TO_MODE when optimizing
   for SPEED.  */
//原型 convert_cost expmed.h
int mtcs_expmed_convert_cost (MtcsExpmed *self,machine_mode to_mode, machine_mode from_mode, bool speed)
{
  return *mtcs_expmed_convert_cost_ptr (self,to_mode, from_mode, speed);
}

/* Return true if the x_alg_hash field might have been used.  */
//原型 alg_hash_used_p expmed.h
bool mtcs_expmed_alg_hash_used_p (MtcsExpmed *self)
{
  return self/*!this_target_expmed*/->x_alg_hash_used_p;
}

/* Return a pointer to the alg_hash_entry at IDX.  */
//原型 alg_hash_entry_ptr expmed.h
struct alg_hash_entry *mtcs_expmed_alg_hash_entry_ptr (MtcsExpmed *self,int idx)
{
  return &self->x_alg_hash[idx];
}
/* Set whether the x_alg_hash field might have been used.  */
//原型 set_alg_hash_used_p expmed.h
void mtcs_expmed_set_alg_hash_used_p (MtcsExpmed *self,bool usedp)
{
  self->x_alg_hash_used_p = usedp;
}


//原型 init_expmed_one_conv expmed.cc
static void init_expmed_one_conv (MtcsExpmed *self,struct init_expmed_rtl *all, scalar_int_mode to_mode,
              scalar_int_mode from_mode, bool speed)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  int to_size, from_size;
  rtx which;

  to_size =mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,to_mode);
  from_size = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,from_mode);
  n_debug("mtcsexpmed.c init_expmed_one_conv 00 to_mode:%d from_mode:%d speed:%d to_size:%d from_size:%d\n",
          to_mode,from_mode,speed, to_size,from_size);
  /* Most partial integers have a precision less than the "full"
     integer it requires for storage.  In case one doesn't, for
     comparison purposes here, reduce the bit size by one in that
     case.  */
  if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,to_mode) == MODE_PARTIAL_INT  && pow2p_hwi (to_size))
    to_size --;
  if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,from_mode) == MODE_PARTIAL_INT && pow2p_hwi (from_size))
    from_size --;

  /* Assume cost of zero-extend and sign-extend is the same.  */
  which = (to_size < from_size ? all->trunc : all->zext);
  n_debug("mtcsexpmed.c init_expmed_one_conv 11 to_mode:%d from_mode:%d speed:%d to_size:%d from_size:%d which:%d %d\n",
          to_mode,from_mode,speed, to_size,from_size,which->mode,which->code);

  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->reg, from_mode);
  mtcs_expmed_set_convert_cost (self,to_mode, from_mode, speed,mtcs_rtlanal_set_src_cost (mtcsRtlanal,which, to_mode, speed));
  /* Restore all->reg's mode.  */
  n_debug("mtcsexpmed.c init_expmed_one_conv 22 to_mode:%d from_mode:%d speed:%d to_size:%d from_size:%d reg:%d %d\n",
          to_mode,from_mode,speed, to_size,from_size,all->reg->mode,all->reg->code);
  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->reg, to_mode);
}



//原型 init_expmed_one_mode expmed.cc
static void init_expmed_one_mode (MtcsExpmed *self,struct init_expmed_rtl *all,machine_mode mode, int speed)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  int m, n, mode_bitsize;
  machine_mode mode_from;

  mode_bitsize =mtcs_mode_get_unit_bitsize/*!GET_MODE_UNIT_BITSIZE*/(mtcsMode,mode);
  n_debug("mtcsexpmed.c init_expmed_one_mode 00 mode:%d mode_bitsize:%d\n",mode,mode_bitsize);

  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->reg, mode);
  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->plus, mode);
  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->neg, mode);
  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->mult, mode);
  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->sdiv, mode);
  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->udiv, mode);
  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->sdiv_32, mode);
  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->smod_32, mode);
  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->wide_trunc, mode);
  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->shift, mode);
  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->shift_mult, mode);
  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->shift_add, mode);
  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->shift_sub0, mode);
  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->shift_sub1, mode);
  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->zext, mode);
  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->trunc, mode);

  mtcs_expmed_set_add_cost (self,speed, mode, mtcs_rtlanal_set_src_cost (mtcsRtlanal,all->plus, mode, speed));
  mtcs_expmed_set_neg_cost (self,speed, mode, mtcs_rtlanal_set_src_cost (mtcsRtlanal,all->neg, mode, speed));
  mtcs_expmed_set_mul_cost (self,speed, mode, mtcs_rtlanal_set_src_cost (mtcsRtlanal,all->mult, mode, speed));
  mtcs_expmed_set_sdiv_cost (self,speed, mode, mtcs_rtlanal_set_src_cost (mtcsRtlanal,all->sdiv, mode, speed));
  mtcs_expmed_set_udiv_cost (self,speed, mode,  mtcs_rtlanal_set_src_cost (mtcsRtlanal,all->udiv, mode, speed));

  mtcs_expmed_set_sdiv_pow2_cheap(self,speed, mode, (mtcs_rtlanal_set_src_cost (mtcsRtlanal,all->sdiv_32, mode, speed)
                     <= 2 * mtcs_expmed_add_cost (self,speed, mode)));
  mtcs_expmed_set_smod_pow2_cheap(self,speed, mode, (mtcs_rtlanal_set_src_cost (mtcsRtlanal,all->smod_32, mode, speed)
                     <= 4 * mtcs_expmed_add_cost (self,speed, mode)));
  n_debug("mtcsexpmed.c init_expmed_one_mode 11 mode:%d mode_bitsize:%d\n",mode,mode_bitsize);

  mtcs_expmed_set_shift_cost (self,speed, mode, 0, 0);
  {
    int cost = mtcs_expmed_add_cost (self,speed, mode);
    mtcs_expmed_set_shiftadd_cost (self,speed, mode, 0, cost);
    mtcs_expmed_set_shiftsub0_cost (self,speed, mode, 0, cost);
    mtcs_expmed_set_shiftsub1_cost (self,speed, mode, 0, cost);
  }

  n = MIN (MAX_BITS_PER_WORD, mode_bitsize);
  n_debug("mtcsexpmed.c init_expmed_one_mode 22 mode:%d mode_bitsize:%d n:%d\n",mode,mode_bitsize,n);
  for (m = 1; m < n; m++){
      XEXP (all->shift, 1) = all->cint[m];
      XEXP (all->shift_mult, 1) = all->pow2[m];
      mtcs_expmed_set_shift_cost (self,speed, mode, m, mtcs_rtlanal_set_src_cost (mtcsRtlanal,all->shift, mode, speed));
      mtcs_expmed_set_shiftadd_cost (self,speed, mode, m, mtcs_rtlanal_set_src_cost (mtcsRtlanal,all->shift_add, mode,speed));
      mtcs_expmed_set_shiftsub0_cost (self,speed, mode, m, mtcs_rtlanal_set_src_cost (mtcsRtlanal,all->shift_sub0, mode,speed));
      mtcs_expmed_set_shiftsub1_cost (self,speed, mode, m, mtcs_rtlanal_set_src_cost (mtcsRtlanal,all->shift_sub1, mode,speed));
  }

  scalar_int_mode int_mode_to;
  if (mtcs_mode_is_a/*!is_a*/ <scalar_int_mode>(mtcsMode,mode, &int_mode_to)){
      n_debug("mtcsexpmed.c init_expmed_one_mode 33 mode:%d mode_bitsize:%d int_mode_to:%d\n",mode,mode_bitsize,int_mode_to);
      for (mode_from =mtcsMode->modesMinMax.min_INT/*! MIN_MODE_INT*/;
              mode_from <= mtcsMode->modesMinMax.max_INT/*!MAX_MODE_INT*/;mode_from = (machine_mode)(mode_from + 1))
          init_expmed_one_conv (self,all, int_mode_to,mtcs_mode_as_a/*!as_a*/<scalar_int_mode>(mtcsMode,mode_from), speed);

      scalar_int_mode wider_mode;
      if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,int_mode_to) == MODE_INT
              && mtcs_mode_get_wider/*!GET_MODE_WIDER_MODE*/ (mtcsMode,int_mode_to).exists (&wider_mode)){
        n_debug("mtcsexpmed.c init_expmed_one_mode 44 mode:%d mode_bitsize:%d wider_mode:%d\n",mode,mode_bitsize,wider_mode);
        mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->reg, mode);
        mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->zext, wider_mode);
        mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->wide_mult, wider_mode);
        mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,all->wide_lshr, wider_mode);
        XEXP (all->wide_lshr, 1)= mtcs_rtl_gen_int_shift_amount/*!gen_int_shift_amount*/(mtcsRTL,wider_mode, mode_bitsize);

        mtcs_expmed_set_mul_widen_cost(self,speed, wider_mode,mtcs_rtlanal_set_src_cost (mtcsRtlanal,all->wide_mult, wider_mode, speed));
        mtcs_expmed_set_mul_highpart_cost(self,speed, int_mode_to,mtcs_rtlanal_set_src_cost (mtcsRtlanal,all->wide_trunc,int_mode_to, speed));
      }
  }
}

//原型 init_expmed rtl.h expmed.cc
void mtcs_expmed_init_expmed (MtcsExpmed *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  struct init_expmed_rtl all;
  machine_mode mode =(machine_mode)mtcsMode->modes.M_QImode;
  int m, speed;

  memset (&all, 0, sizeof all);
  for (m = 1; m < MAX_BITS_PER_WORD; m++){
      all.pow2[m] = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,HOST_WIDE_INT_1 << m);
      all.cint[m] = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,m);
  }

  /* Avoid using hard regs in ways which may be unsupported.  */
  all.reg = mtcs_rtl_gen_raw_REG/*gen_raw_REG*/(mtcsRTL,mode, mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg) + 1);
  all.plus = gen_rtx_PLUS (mode, all.reg, all.reg);
  all.neg = gen_rtx_NEG (mode, all.reg);
  all.mult = gen_rtx_MULT (mode, all.reg, all.reg);
  all.sdiv = gen_rtx_DIV (mode, all.reg, all.reg);
  all.udiv = gen_rtx_UDIV (mode, all.reg, all.reg);
  all.sdiv_32 = gen_rtx_DIV (mode, all.reg, all.pow2[5]);
  all.smod_32 = gen_rtx_MOD (mode, all.reg, all.pow2[5]);
  all.zext = gen_rtx_ZERO_EXTEND (mode, all.reg);
  all.wide_mult = gen_rtx_MULT (mode, all.zext, all.zext);
  all.wide_lshr = gen_rtx_LSHIFTRT (mode, all.wide_mult, all.reg);
  all.wide_trunc = gen_rtx_TRUNCATE (mode, all.wide_lshr);
  all.shift = gen_rtx_ASHIFT (mode, all.reg, all.reg);
  all.shift_mult = gen_rtx_MULT (mode, all.reg, all.reg);
  all.shift_add = gen_rtx_PLUS (mode, all.shift_mult, all.reg);
  all.shift_sub0 = gen_rtx_MINUS (mode, all.shift_mult, all.reg);
  all.shift_sub1 = gen_rtx_MINUS (mode, all.reg, all.shift_mult);
  all.trunc = gen_rtx_TRUNCATE (mode, all.reg);
  for (speed = 0; speed < 2; speed++){
      mtcsRtlData/*!crtl*/->maybe_hot_insn_p = speed;
      mtcs_expmed_set_zero_cost/*!set_zero_cost*/ (self,speed,
              mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,const0_rtx, mode, speed));
      n_debug("mtcsexpmed.c init_expmed 00 MIN_MODE_INT:%d MAX_MODE_INT:%d\n",mtcsMode->modesMinMax.min_INT,mtcsMode->modesMinMax.max_INT);

      for (mode =mtcsMode->modesMinMax.min_INT/*!MIN_MODE_INT*/;
              mode <= mtcsMode->modesMinMax.max_INT/*!MAX_MODE_INT*/; mode = (machine_mode)(mode + 1))
          init_expmed_one_mode (self,&all, mode, speed);
      n_debug("mtcsexpmed.c init_expmed 11 MIN_MODE_PARTIAL_INT:%d MIN_MODE_PARTIAL_INT:%d\n",
              mtcsMode->modesMinMax.min_PARTIAL_INT,mtcsMode->modesMinMax.max_PARTIAL_INT);

      if (mtcsMode->modesMinMax.min_PARTIAL_INT/*!MIN_MODE_PARTIAL_INT*/ != VOIDmode)
        for (mode = mtcsMode->modesMinMax.min_PARTIAL_INT/*!MIN_MODE_PARTIAL_INT*/;
                mode <= mtcsMode->modesMinMax.max_PARTIAL_INT/*!MAX_MODE_PARTIAL_INT*/;mode = (machine_mode)(mode + 1))
          init_expmed_one_mode (self,&all, mode, speed);

      n_debug("mtcsexpmed.c init_expmed 22 MIN_MODE_VECTOR_INT:%d MAX_MODE_VECTOR_INT:%d\n",
              mtcsMode->modesMinMax.min_VECTOR_INT,mtcsMode->modesMinMax.max_VECTOR_INT);
      if (mtcsMode->modesMinMax.min_VECTOR_INT/*!MIN_MODE_VECTOR_INT*/ != VOIDmode)
        for (mode = mtcsMode->modesMinMax.min_VECTOR_INT/*!MIN_MODE_VECTOR_INT*/;
                mode <= mtcsMode->modesMinMax.max_VECTOR_INT/*!MAX_MODE_VECTOR_INT*/; mode = (machine_mode)(mode + 1))
            init_expmed_one_mode (self,&all, mode, speed);
  }

  if (mtcs_expmed_alg_hash_used_p (self)){
      struct alg_hash_entry *p = mtcs_expmed_alg_hash_entry_ptr (self,0);
      memset (p, 0, sizeof (*p) * NUM_ALG_HASH_ENTRIES);
  }else
      mtcs_expmed_set_alg_hash_used_p (self,true);

  n_debug("mtcsexpmed.c init_expmed 33\n");
  mtcs_func_default_rtl_profile/*!default_rtl_profile ()*/(mtcsFunc);

  ggc_free (all.trunc);
  ggc_free (all.shift_sub1);
  ggc_free (all.shift_sub0);
  ggc_free (all.shift_add);
  ggc_free (all.shift_mult);
  ggc_free (all.shift);
  ggc_free (all.wide_trunc);
  ggc_free (all.wide_lshr);
  ggc_free (all.wide_mult);
  ggc_free (all.zext);
  ggc_free (all.smod_32);
  ggc_free (all.sdiv_32);
  ggc_free (all.udiv);
  ggc_free (all.sdiv);
  ggc_free (all.mult);
  ggc_free (all.neg);
  ggc_free (all.plus);
  ggc_free (all.reg);
}

/* Return an rtx representing minus the value of X.
   MODE is the intended mode of the result,
   useful if X is a CONST_INT.  */
//原型 negate_rtx expmed.h expmed.cc
rtx mtcs_expmed_negate_rtx (MtcsExpmed *self,machine_mode mode, rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

  rtx result = mtcs_simplify_rtx_unary_operation/*!simplify_unary_operation*/(mtcsSimplifyRtx,NEG, mode, x, mode);
  if (result == 0)
    result = mtcs_optabs_expand_unop/*!expand_unop*/(mtcsOptabs,mode, neg_optab, x, NULL_RTX, 0);

  return result;
}

/* Return true if a bitfield of size BITSIZE at bit number BITNUM within
   a structure of mode STRUCT_MODE represents a lowpart subreg.   The subreg
   offset is then BITNUM / BITS_PER_UNIT.  */
//原型 lowpart_bit_field_p expmed.cc
static bool lowpart_bit_field_p (MtcsExpmed *self,poly_uint64 bitnum, poly_uint64 bitsize, machine_mode struct_mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  poly_uint64 regsize = mtcs_mode_get_regmode_natural_size/*!REGMODE_NATURAL_SIZE*/(mtcsMode,struct_mode);
  if (BYTES_BIG_ENDIAN)
    return (multiple_p (bitnum, BITS_PER_UNIT)
        && (known_eq (bitnum + bitsize, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,struct_mode))
        || multiple_p (bitnum + bitsize,
                   regsize * BITS_PER_UNIT)));
  else
    return multiple_p (bitnum, regsize * BITS_PER_UNIT);
}

/* Try to use instruction INSV to store VALUE into a field of OP0.
   If OP0_MODE is defined, it is the mode of OP0, otherwise OP0 is a
   BLKmode MEM.  VALUE_MODE is the mode of VALUE.  BITSIZE and BITNUM
   are as for store_bit_field.  */
//原型 store_bit_field_using_insv expmed.cc
static bool store_bit_field_using_insv (MtcsExpmed *self,const extraction_insn *insv, rtx op0,
                opt_scalar_int_mode op0_mode,
                unsigned HOST_WIDE_INT bitsize,
                unsigned HOST_WIDE_INT bitnum,
                rtx value, scalar_int_mode value_mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

  class expand_operand ops[4];
  rtx value1;
  rtx xop0 = op0;
  rtx_insn *last = mtcs_rtl_data_get_last_insn (mtcsRtlData);
  bool copy_back = false;

  scalar_int_mode op_mode = insv->field_mode;
  unsigned int unit = mtcs_mode_get_bitsize(mtcsMode,op_mode);
  if (bitsize == 0 || bitsize > unit)
    return false;

  if (MEM_P (xop0))
    /* Get a reference to the first byte of the field.  */
    xop0 = narrow_bit_field_mem(self,xop0, insv->struct_mode, bitsize, bitnum,&bitnum);
  else{
      /* Convert from counting within OP0 to counting in OP_MODE.  */
      if (BYTES_BIG_ENDIAN)
          bitnum += unit - mtcs_mode_get_bitsize(mtcsMode,op0_mode.require ());

      /* If xop0 is a register, we need it in OP_MODE
     to make it acceptable to the format of insv.  */
      if (GET_CODE (xop0) == SUBREG){
          /* If such a SUBREG can't be created, give up.  */
          if (!mtcs_rtl_validate_subreg/*!validate_subreg*/(mtcsRTL,op_mode,
                  GET_MODE (SUBREG_REG (xop0)),SUBREG_REG (xop0), SUBREG_BYTE (xop0)))
            return false;
          /* We can't just change the mode, because this might clobber op0,
             and we will need the original value of op0 if insv fails.  */
          xop0 = mtcs_rtl_gen_rtx_SUBREG/*!gen_rtx_SUBREG*/(mtcsRTL,op_mode, SUBREG_REG (xop0),SUBREG_BYTE (xop0));
      }
      if (REG_P (xop0) && GET_MODE (xop0) != op_mode)
          xop0 = gen_lowpart_SUBREG (op_mode, xop0);
  }

  /* If the destination is a paradoxical subreg such that we need a
     truncate to the inner mode, perform the insertion on a temporary and
     truncate the result to the original destination.  Note that we can't
     just truncate the paradoxical subreg as (truncate:N (subreg:W (reg:N
     X) 0)) is (reg:N X).  */
  if (GET_CODE (xop0) == SUBREG  && REG_P (SUBREG_REG (xop0))
      && !mtcs_mode_truly_noop_truncation_p/*!TRULY_NOOP_TRUNCATION_MODES_P*/(mtcsMode,GET_MODE (SUBREG_REG (xop0)),op_mode)){
      rtx tem = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,op_mode);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,tem, xop0);
      xop0 = tem;
      copy_back = true;
  }

  /* There are similar overflow check at the start of store_bit_field_1,
     but that only check the situation where the field lies completely
     outside the register, while there do have situation where the field
     lies partialy in the register, we need to adjust bitsize for this
     partial overflow situation.  Without this fix, pr48335-2.c on big-endian
     will broken on those arch support bit insert instruction, like arm, aarch64
     etc.  */
  if (bitsize + bitnum > unit && bitnum < unit){
      warning (OPT_Wextra, "write of %wu-bit data outside the bound of "
           "destination object, data truncated into %wu-bit",
           bitsize, unit - bitnum);
      bitsize = unit - bitnum;
  }

  /* If BITS_BIG_ENDIAN is zero on a BYTES_BIG_ENDIAN machine, we count
     "backwards" from the size of the unit we are inserting into.
     Otherwise, we count bits from the most significant on a
     BYTES/BITS_BIG_ENDIAN machine.  */

  if (BITS_BIG_ENDIAN != BYTES_BIG_ENDIAN)
    bitnum = unit - bitsize - bitnum;

  /* Convert VALUE to op_mode (which insv insn wants) in VALUE1.  */
  value1 = value;
  if (value_mode != op_mode){
      if (mtcs_mode_get_bitsize(mtcsMode,value_mode) >= bitsize){
          rtx tmp;
          /* Optimization: Don't bother really extending VALUE
             if it has all the bits we will actually use.  However,
             if we must narrow it, be sure we do it correctly.  */

          if (mtcs_mode_get_size(mtcsMode,value_mode) < mtcs_mode_get_size(mtcsMode,op_mode)){
              tmp = mtcs_simplify_rtx_subreg/*!simplify_subreg*/(mtcsSimplifyRtx,op_mode, value1, value_mode, 0);
              if (! tmp)
                tmp = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,op_mode,
                        mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,value_mode, value1), value_mode, 0);
          }else{
              tmp = mtcs_rtl_gen_lowpart_if_possible/*!gen_lowpart_if_possible*/(mtcsRTL,op_mode, value1);
              if (! tmp)
                  tmp = gen_lowpart (op_mode, mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,value_mode, value1));
          }
          value1 = tmp;
      }else if (CONST_INT_P (value))
          value1 = mtcs_rtl_gen_int_mode/*!gen_int_mode*/ (mtcsRTL,INTVAL (value), op_mode);
      else
    /* Parse phase is supposed to make VALUE's data type
       match that of the component reference, which is a type
       at least as wide as the field; so VALUE should have
       a mode that corresponds to that type.  */
          gcc_assert (CONSTANT_P (value));
  }

  create_fixed_operand (&ops[0], xop0);
  mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[1], bitsize);
  mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[2], bitnum);
  create_input_operand (&ops[3], value1, op_mode);
  if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(mtcsOptabs,insv->icode, 4, ops)){
      if (copy_back)
          mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,op0, xop0, true);
      return true;
  }
  mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
  return false;
}


/* Subroutine of store_bit_field_1, with the same arguments, except
   that BITSIZE and BITNUM are constant.  Handle cases specific to
   integral modes.  If OP0_MODE is defined, it is the mode of OP0,
   otherwise OP0 is a BLKmode MEM.  */
//原型 store_integral_bit_field expmed.cc
static bool store_integral_bit_field(MtcsExpmed *self,rtx op0, opt_scalar_int_mode op0_mode,
              unsigned HOST_WIDE_INT bitsize,
              unsigned HOST_WIDE_INT bitnum,
              poly_uint64 bitregion_start,
              poly_uint64 bitregion_end,
              machine_mode fieldmode,
              rtx value, bool reverse, bool fallback_p)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

  /* Storing an lsb-aligned field in a register
     can be done with a movstrict instruction.  */

  if (!MEM_P (op0)
      && !reverse
      && lowpart_bit_field_p(self,bitnum, bitsize, op0_mode.require ())
      && known_eq (bitsize, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,fieldmode))
      && mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,movstrict_optab, fieldmode) != CODE_FOR_nothing){
      class expand_operand ops[2];
      enum insn_code icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,movstrict_optab, fieldmode);
      rtx arg0 = op0;
      unsigned HOST_WIDE_INT subreg_off;
      if (GET_CODE (arg0) == SUBREG){
          /* Else we've got some float mode source being extracted into
             a different float mode destination -- this combination of
             subregs results in Severe Tire Damage.  */
          gcc_assert (GET_MODE (SUBREG_REG (arg0)) == fieldmode
                  || mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,fieldmode) == MODE_INT
                  || mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,fieldmode) == MODE_PARTIAL_INT);
          arg0 = SUBREG_REG (arg0);
      }

      subreg_off = bitnum / BITS_PER_UNIT;
      if (validate_subreg (fieldmode, GET_MODE (arg0), arg0, subreg_off)
          /* STRICT_LOW_PART must have a non-paradoxical subreg as
             operand.  */
          && !mtcs_mode_paradoxical_subreg_p/*!paradoxical_subreg_p*/(mtcsMode,fieldmode, GET_MODE (arg0))){

          arg0 = mtcs_rtl_gen_rtx_SUBREG/*!gen_rtx_SUBREG*/(mtcsRTL,fieldmode, arg0, subreg_off);
          create_fixed_operand (&ops[0], arg0);
          /* Shrink the source operand to FIELDMODE.  */
          create_convert_operand_to (&ops[1], value, fieldmode, false);
          if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(mtcsOptabs,icode, 2, ops))
            return true;
      }
  }

  /* Handle fields bigger than a word.  */

  if (bitsize > BITS_PER_WORD){
      /* Here we transfer the words of the field
     in the order least significant first.
     This is because the most significant word is the one which may
     be less than full.
     However, only do that if the value is not BLKmode.  */

      const bool backwards = WORDS_BIG_ENDIAN && fieldmode != mtcsMode->modes.M_BLKmode;
      const int nwords = (bitsize + (BITS_PER_WORD - 1)) / BITS_PER_WORD;
      rtx_insn *last;

      /* This is the mode we must force value to, so that there will be enough
     subwords to extract.  Note that fieldmode will often (always?) be
     VOIDmode, because that is what store_field uses to indicate that this
     is a bit field, but passing VOIDmode to operand_subword_force
     is not allowed.

     The mode must be fixed-size, since insertions into variable-sized
     objects are meant to be handled before calling this function.  */
      fixed_size_mode value_mode = mtcs_mode_as_a <fixed_size_mode> (mtcsMode,GET_MODE (value));
      if (value_mode == VOIDmode)
          value_mode = mtcs_mode_smallest_int_mode_for_size/*!smallest_int_mode_for_size*/(mtcsMode,nwords * BITS_PER_WORD);

      last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
      for (int i = 0; i < nwords; i++){
          /* Number of bits to be stored in this iteration, i.e. BITS_PER_WORD
             except maybe for the last iteration.  */
          const unsigned HOST_WIDE_INT new_bitsize= MIN (BITS_PER_WORD, bitsize - i * BITS_PER_WORD);
          /* Bit offset from the starting bit number in the target.  */
          const unsigned int bit_offset = backwards ^ reverse
              ? MAX ((int) bitsize - (i + 1) * BITS_PER_WORD, 0)
              : i * BITS_PER_WORD;
          /* Starting word number in the value.  */
          const unsigned int wordnum = backwards ? mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,value_mode) / UNITS_PER_WORD - (i + 1) : i;
          /* The chunk of the value in word_mode.  We use bit-field extraction
              in BLKmode to handle unaligned memory references and to shift the
              last chunk right on big-endian machines if need be.  */
          rtx value_word = fieldmode ==mtcsMode->modes.M_BLKmode
              ?mtcs_expmed_extract_bit_field/*!extract_bit_field*/(self,value, new_bitsize, wordnum * BITS_PER_WORD,
                       1, NULL_RTX, word_mode, word_mode, false, NULL)
              : mtcs_rtl_operand_subword_force/*!operand_subword_force*/(mtcsRTL,value, wordnum, value_mode);

          if (!store_bit_field_1(self,op0, new_bitsize,
                      bitnum + bit_offset,
                      bitregion_start, bitregion_end,
                      word_mode,
                      value_word, reverse, fallback_p, false)){
              mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
              return false;
          }
      }
      return true;
  }

  /* If VALUE has a floating-point or complex mode, access it as an
     integer of the corresponding size.  This can occur on a machine
     with 64 bit registers that uses SFmode for float.  It can also
     occur for unaligned float or complex fields.  */
  rtx orig_value = value;
  scalar_int_mode value_mode;
  if (GET_MODE (value) == VOIDmode)
    /* By this point we've dealt with values that are bigger than a word,
       so word_mode is a conservatively correct choice.  */
    value_mode = word_mode;
  else if (!mtcs_mode_is_a <scalar_int_mode>(mtcsMode,GET_MODE (value), &value_mode)){
      value_mode = mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,GET_MODE (value)).require ();
      value = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,value_mode);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,gen_lowpart (GET_MODE (orig_value), value), orig_value);
  }

  /* If OP0 is a multi-word register, narrow it to the affected word.
     If the region spans two words, defer to store_split_bit_field.
     Don't do this if op0 is a single hard register wider than word
     such as a float or vector register.  */
  if (!MEM_P (op0)
      && mtcs_mode_get_size(mtcsMode,op0_mode.require ()) > UNITS_PER_WORD
      && (!REG_P (op0)  || !mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,op0)
      || mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,REGNO (op0), op0_mode.require ()) != 1)){

      if (bitnum % BITS_PER_WORD + bitsize > BITS_PER_WORD){
          if (!fallback_p)
            return false;

          store_split_bit_field(self,op0, op0_mode, bitsize, bitnum,bitregion_start, bitregion_end,
                     value, value_mode, reverse);
          return true;
      }
      op0 = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,word_mode, op0, op0_mode.require (),
                 bitnum / BITS_PER_WORD * UNITS_PER_WORD);
      gcc_assert (op0);
      op0_mode = word_mode;
      bitnum %= BITS_PER_WORD;
  }

  /* From here on we can assume that the field to be stored in fits
     within a word.  If the destination is a register, it too fits
     in a word.  */

  extraction_insn insv;
  if (!MEM_P (op0)
      && !reverse
      && mtcs_optabs_get_best_reg_extraction_insn/*!get_best_reg_extraction_insn*/(mtcsOptabs,&insv, EP_insv,
                       mtcs_mode_get_bitsize/*GET_MODE_BITSIZE*/(mtcsMode,op0_mode.require ()),fieldmode)
      && store_bit_field_using_insv(self,&insv, op0, op0_mode,bitsize, bitnum, value, value_mode))
    return true;

  /* If OP0 is a memory, try copying it to a register and seeing if a
     cheap register alternative is available.  */
  if (MEM_P (op0) && !reverse){
      if (mtcs_optabs_get_best_mem_extraction_insn/*!get_best_mem_extraction_insn*/(mtcsOptabs,&insv, EP_insv, bitsize, bitnum,fieldmode)
        && store_bit_field_using_insv(self,&insv, op0, op0_mode, bitsize, bitnum, value, value_mode))
          return true;

      rtx_insn *last = mtcs_rtl_data_get_last_insn (mtcsRtlData);

      /* Try loading part of OP0 into a register, inserting the bitfield
     into that, and then copying the result back to OP0.  */
      unsigned HOST_WIDE_INT bitpos;
      rtx xop0 = adjust_bit_field_mem_for_reg(self,EP_insv, op0, bitsize, bitnum,
                           bitregion_start, bitregion_end,fieldmode, &bitpos);
      if (xop0){
          rtx tempreg = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,xop0);
          if (store_bit_field_1(self,tempreg, bitsize, bitpos,
                     bitregion_start, bitregion_end,fieldmode, orig_value, reverse, false, false)){
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,xop0, tempreg);
              return true;
          }
          mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
      }
  }

  if (!fallback_p)
    return false;

  store_fixed_bit_field(self,op0, op0_mode, bitsize, bitnum, bitregion_start,
             bitregion_end, value, value_mode, reverse);
  return true;
}

/* Return true if -fstrict-volatile-bitfields applies to an access of OP0
   containing BITSIZE bits starting at BITNUM, with field mode FIELDMODE.
   Return false if the access would touch memory outside the range
   BITREGION_START to BITREGION_END for conformance to the C++ memory
   model.  */
//原型 strict_volatile_bitfield_p expmed.cc
static bool strict_volatile_bitfield_p (MtcsExpmed *self,rtx op0, unsigned HOST_WIDE_INT bitsize,
                unsigned HOST_WIDE_INT bitnum,
                scalar_int_mode fieldmode,
                poly_uint64 bitregion_start,
                poly_uint64 bitregion_end)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  unsigned HOST_WIDE_INT modesize = mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,fieldmode);

  /* -fstrict-volatile-bitfields must be enabled and we must have a
     volatile MEM.  */
  if (!MEM_P (op0) || !MEM_VOLATILE_P (op0)
          || mtcsOptionsItem->x_flag_strict_volatile_bitfields <= 0)
    return false;

  /* The bit size must not be larger than the field mode, and
     the field mode must not be larger than a word.  */
  if (bitsize > modesize || modesize > BITS_PER_WORD)
    return false;

  /* Check for cases of unaligned fields that must be split.  */
  if (bitnum % modesize + bitsize > modesize)
    return false;

  /* The memory must be sufficiently aligned for a MODESIZE access.
     This condition guarantees, that the memory access will not
     touch anything after the end of the structure.  */
  if (mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,op0) < modesize)
    return false;

  /* Check for cases where the C++ memory model applies.  */
  if (maybe_ne (bitregion_end, 0U)
      && (maybe_lt (bitnum - bitnum % modesize, bitregion_start)
      || maybe_gt (bitnum - bitnum % modesize + modesize - 1,bitregion_end)))
    return false;

  return true;
}

/* Return an rtx representing value of X with reverse storage order.
   MODE is the intended mode of the result,
   useful if X is a CONST_INT.  */
//原型 flip_storage_order expmed.h expmed.cc
rtx mtcs_expmed_flip_storage_order(MtcsExpmed *self,machine_mode mode, rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

  scalar_int_mode int_mode;
  rtx result;

  if (mode ==mtcsMode->modes.M_QImode)
    return x;

  if (mtcs_mode_is_complex_p/*!COMPLEX_MODE_P*/(mtcsMode,mode)){
      rtx real = read_complex_part (x, false);
      rtx imag = read_complex_part (x, true);

      real = mtcs_expmed_flip_storage_order(self,mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode), real);
      imag = mtcs_expmed_flip_storage_order(self,mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode), imag);
      return gen_rtx_CONCAT (mode, real, imag);
  }

  if (UNLIKELY (self->reverse_storage_order_supported < 0))
    check_reverse_storage_order_support(self);

  if (!mtcs_mode_is_a <scalar_int_mode>(mtcsMode,mode, &int_mode)){
      if (mtcs_mode_is_float_p/*FLOAT_MODE_P*/(mtcsMode,mode)
        && UNLIKELY (self->reverse_float_storage_order_supported < 0))
          check_reverse_float_storage_order_support(self);

      if (!mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,mtcs_mode_get_precision(mtcsMode,mode), 0).exists (&int_mode)
         || !mtcsTarget->/*!targetm.scalar_mode_supported_p*/scalar_mode_supported_p(mtcsTarget,int_mode)){
          sorry ("reverse storage order for %smode", mtcs_mode_get_name/*!GET_MODE_NAME*/(mtcsMode,mode));
          return x;
      }
      x = gen_lowpart (int_mode, x);
  }

  result = mtcs_simplify_rtx_unary_operation/*!simplify_unary_operation*/(mtcsSimplifyRtx,BSWAP, int_mode, x, int_mode);
  if (result == 0)
    result = mtcs_optabs_expand_unop/*!expand_unop*/(mtcsOptabs,int_mode, bswap_optab, x, NULL_RTX, 1);

  if (int_mode != mode)
    result = gen_lowpart (mode, result);

  return result;
}

/* Generate code to store value from rtx VALUE
   into a bit-field within structure STR_RTX
   containing BITSIZE bits starting at bit BITNUM.

   BITREGION_START is bitpos of the first bitfield in this region.
   BITREGION_END is the bitpos of the ending bitfield in this region.
   These two fields are 0, if the C++ memory model does not apply,
   or we are not interested in keeping track of bitfield regions.

   FIELDMODE is the machine-mode of the FIELD_DECL node for this field.

   If REVERSE is true, the store is to be done in reverse order.

   If UNDEFINED_P is true then STR_RTX is currently undefined.  */
//原型 store_bit_field expmed.h expmed.cc
void mtcs_expmed_store_bit_field (MtcsExpmed *self,rtx str_rtx, poly_uint64 bitsize, poly_uint64 bitnum,
         poly_uint64 bitregion_start, poly_uint64 bitregion_end,
         machine_mode fieldmode,  rtx value, bool reverse, bool undefined_p)
{

  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

  /* Handle -fstrict-volatile-bitfields in the cases where it applies.  */
  unsigned HOST_WIDE_INT ibitsize = 0, ibitnum = 0;
  scalar_int_mode int_mode;
  if (bitsize.is_constant (&ibitsize)
      && bitnum.is_constant (&ibitnum)
      && mtcs_mode_is_a <scalar_int_mode>(mtcsMode,fieldmode, &int_mode)
      && strict_volatile_bitfield_p(self,str_rtx, ibitsize, ibitnum, int_mode,
                     bitregion_start, bitregion_end)){
      /* Storing of a full word can be done with a simple store.
     We know here that the field can be accessed with one single
     instruction.  For targets that support unaligned memory,
     an unaligned access may be necessary.  */
      if (ibitsize ==mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,int_mode)){
          str_rtx = mtcs_rtl_adjust_bitfield_address/*!adjust_bitfield_address*/(mtcsRTL,str_rtx, int_mode, ibitnum / BITS_PER_UNIT);
          if (reverse)
            value = mtcs_expmed_flip_storage_order(self,int_mode, value);
          gcc_assert (ibitnum % BITS_PER_UNIT == 0);
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,str_rtx, value);
      }else{
          rtx temp;

          str_rtx = narrow_bit_field_mem (self,str_rtx, int_mode, ibitsize,ibitnum, &ibitnum);
          gcc_assert (ibitnum + ibitsize <= mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,int_mode));
          temp = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,str_rtx);
          if (!store_bit_field_1(self,temp, ibitsize, ibitnum, 0, 0,
                      int_mode, value, reverse, true, undefined_p))
            gcc_unreachable ();

          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,str_rtx, temp);
      }

      return;
  }

  /* Under the C++0x memory model, we must not touch bits outside the
     bit region.  Adjust the address to start at the beginning of the
     bit region.  */
  if (MEM_P (str_rtx) && maybe_ne (bitregion_start, 0U)){
      scalar_int_mode best_mode;
      machine_mode addr_mode = VOIDmode;

      poly_uint64 offset = exact_div (bitregion_start, BITS_PER_UNIT);
      bitnum -= bitregion_start;
      poly_int64 size = bits_to_bytes_round_up (bitnum + bitsize);
      bitregion_end -= bitregion_start;
      bitregion_start = 0;
      if (bitsize.is_constant (&ibitsize)
         && bitnum.is_constant (&ibitnum)
         && mtcs_mode_get_best_mode/*!get_best_mode*/(mtcsMode,ibitsize, ibitnum,
                bitregion_start, bitregion_end,
                mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,str_rtx), INT_MAX,
                MEM_VOLATILE_P (str_rtx), &best_mode))
          addr_mode = best_mode;
      str_rtx = mtcs_rtl_adjust_bitfield_address_size/*!adjust_bitfield_address_size*/(mtcsRTL,str_rtx, addr_mode,offset, size);
  }

  if (!store_bit_field_1(self,str_rtx, bitsize, bitnum,
              bitregion_start, bitregion_end,fieldmode, value, reverse, true, undefined_p))
    gcc_unreachable ();
}

/* Generate code to extract a byte-field from STR_RTX
   containing BITSIZE bits, starting at BITNUM,
   and put it in TARGET if possible (if TARGET is nonzero).
   Regardless of TARGET, we return the rtx for where the value is placed.

   STR_RTX is the structure containing the byte (a REG or MEM).
   UNSIGNEDP is nonzero if this is an unsigned bit field.
   MODE is the natural mode of the field value once extracted.
   TMODE is the mode the caller would like the value to have;
   but the value may be returned with type MODE instead.

   If REVERSE is true, the extraction is to be done in reverse order.

   If a TARGET is specified and we can store in it at no extra cost,
   we do so, and return TARGET.
   Otherwise, we return a REG of mode TMODE or MODE, with TMODE preferred
   if they are equally easy.

   If the result can be stored at TARGET, and ALT_RTL is non-NULL,
   then *ALT_RTL is set to TARGET (before legitimziation).  */
//原型 extract_bit_field expmed.h expmed.cc
rtx mtcs_expmed_extract_bit_field (MtcsExpmed *self,rtx str_rtx, poly_uint64 bitsize, poly_uint64 bitnum,
           int unsignedp, rtx target, machine_mode mode,machine_mode tmode, bool reverse, rtx *alt_rtl)
{

  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  machine_mode mode1;

  /* Handle -fstrict-volatile-bitfields in the cases where it applies.  */
  if (maybe_ne(mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (str_rtx)), 0))
    mode1 = GET_MODE (str_rtx);
  else if (target && maybe_ne (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (target)), 0))
    mode1 = GET_MODE (target);
  else
    mode1 = tmode;

  unsigned HOST_WIDE_INT ibitsize, ibitnum;
  scalar_int_mode int_mode;
  if (bitsize.is_constant (&ibitsize)
      && bitnum.is_constant (&ibitnum)
      && mtcs_mode_is_a <scalar_int_mode>(mtcsMode,mode1, &int_mode)
      && strict_volatile_bitfield_p(self,str_rtx, ibitsize, ibitnum,int_mode, 0, 0)){
      /* Extraction of a full INT_MODE value can be done with a simple load.
     We know here that the field can be accessed with one single
     instruction.  For targets that support unaligned memory,
     an unaligned access may be necessary.  */
      if (ibitsize == mtcs_mode_get_bitsize(mtcsMode,int_mode)){
          rtx result = mtcs_rtl_adjust_bitfield_address/*!adjust_bitfield_address*/(mtcsRTL,str_rtx, int_mode,ibitnum / BITS_PER_UNIT);
          if (reverse)
            result = mtcs_expmed_flip_storage_order/*!flip_storage_order*/(self,int_mode, result);
          gcc_assert (ibitnum % BITS_PER_UNIT == 0);
          return convert_extracted_bit_field(self,result, mode, tmode, unsignedp);
      }

      str_rtx = narrow_bit_field_mem(self,str_rtx, int_mode, ibitsize, ibitnum,
                      &ibitnum);
      gcc_assert (ibitnum + ibitsize <= mtcs_mode_get_bitsize/*GET_MODE_BITSIZE*/(mtcsMode,int_mode));
      str_rtx = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,str_rtx);
      return extract_bit_field_1(self,str_rtx, ibitsize, ibitnum, unsignedp,target, mode, tmode, reverse, true, alt_rtl);
  }

  return extract_bit_field_1(self,str_rtx, bitsize, bitnum, unsignedp,target, mode, tmode, reverse, true, alt_rtl);
}


/* Perform a multiplication and return an rtx for the result.
   MODE is mode of value; OP0 and OP1 are what to multiply (rtx's);
   TARGET is a suggestion for where to store the result (an rtx).

   We check specially for a constant integer as OP1.
   If you want this check for OP0 as well, then before calling
   you should swap the two operands if OP0 would be constant.  */
//原型 expand_mult expmed.h expmed.cc
rtx mtcs_expmed_expand_mult (MtcsExpmed *self,machine_mode mode, rtx op0, rtx op1, rtx target,
         int unsignedp, bool no_libcall)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);

  enum mult_variant variant;
  struct algorithm algorithm;
  rtx scalar_op1;
  int max_cost;
  bool speed = optimize_insn_for_speed_p ();
  bool do_trapv = mtcsOptionsItem/*!flag_trapv*/->x_flag_trapv
          && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,mode) && !unsignedp;

  if (CONSTANT_P (op0))
    std::swap (op0, op1);

  /* For vectors, there are several simplifications that can be made if
     all elements of the vector constant are identical.  */
  scalar_op1 = unwrap_const_vec_duplicate (op1);

  if (mtcs_mode_is_integral_p/*!INTEGRAL_MODE_P*/(mtcsMode,mode)){
      rtx fake_reg;
      HOST_WIDE_INT coeff;
      bool is_neg;
      int mode_bitsize;

      if (op1 == CONST0_RTX (mode))
          return op1;
      if (op1 == CONST1_RTX (mode))
          return op0;
      if (op1 == CONSTM1_RTX (mode))
          return mtcs_optabs_expand_unop/*!expand_unop*/ (mtcsOptabs,mode, do_trapv ? negv_optab : neg_optab,op0, target, 0);

      if (do_trapv)
          goto skip_synth;

      /* If mode is integer vector mode, check if the backend supports
     vector lshift (by scalar or vector) at all.  If not, we can't use
     synthetized multiply.  */
      if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) == MODE_VECTOR_INT
      && mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,vashl_optab, mode) == CODE_FOR_nothing
      && mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,ashl_optab, mode) == CODE_FOR_nothing)
          goto skip_synth;

      /* These are the operations that are potentially turned into
     a sequence of shifts and additions.  */
      mode_bitsize = mtcs_mode_get_unit_bitsize/*!GET_MODE_UNIT_BITSIZE*/(mtcsMode,mode);

      /* synth_mult does an `unsigned int' multiply.  As long as the mode is
     less than or equal in size to `unsigned int' this doesn't matter.
     If the mode is larger than `unsigned int', then synth_mult works
     only if the constant value exactly fits in an `unsigned int' without
     any truncation.  This means that multiplying by negative values does
     not work; results are off by 2^32 on a 32 bit machine.  */
      if (CONST_INT_P (scalar_op1)){
          coeff = INTVAL (scalar_op1);
          is_neg = coeff < 0;
      }
      /*
#if TARGET_SUPPORTS_WIDE_INT //host=1 nvptx=1
      else if ( CONST_WIDE_INT_P (scalar_op1))
#else
      else if (CONST_DOUBLE_AS_INT_P (scalar_op1))
#endif
         */
      else if((mtcs_config_get_value(mtcsConfig,MTCS_TARGET_SUPPORTS_WIDE_INT)!=0 && CONST_WIDE_INT_P (scalar_op1))||
            (mtcs_config_get_value(mtcsConfig,MTCS_TARGET_SUPPORTS_WIDE_INT)==0 && CONST_DOUBLE_AS_INT_P (scalar_op1)))
      {
          int shift = wi::exact_log2 (mtcs_rtx_mode_t/*!rtx_mode_t*/(scalar_op1, mode));
          /* Perfect power of 2 (other than 1, which is handled above).  */
          if (shift > 0)
            return mtcs_expmed_expand_shift/*!expand_shift*/(self,LSHIFT_EXPR, mode, op0,shift, target, unsignedp);
          else
            goto skip_synth;
      }else
          goto skip_synth;

      /* We used to test optimize here, on the grounds that it's better to
     produce a smaller program when -O is not used.  But this causes
     such a terrible slowdown sometimes that it seems better to always
     use synth_mult.  */

      /* Special case powers of two.  */
      if (EXACT_POWER_OF_2_OR_ZERO_P (coeff) && !(is_neg && mode_bitsize > HOST_BITS_PER_WIDE_INT))
          return mtcs_expmed_expand_shift/*!expand_shift*/(self,LSHIFT_EXPR, mode, op0,
                     floor_log2 (coeff), target, unsignedp);

      fake_reg = mtcs_rtl_gen_raw_REG/*gen_raw_REG*/(mtcsRTL,
            mode, mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg) + 1);

      /* Attempt to handle multiplication of DImode values by negative
     coefficients, by performing the multiplication by a positive
     multiplier and then inverting the result.  */
      if (is_neg && mode_bitsize > HOST_BITS_PER_WIDE_INT){
          /* Its safe to use -coeff even for INT_MIN, as the
             result is interpreted as an unsigned coefficient.
             Exclude cost of op0 from max_cost to match the cost
             calculation of the synth_mult.  */
          coeff = -(unsigned HOST_WIDE_INT) coeff;
          max_cost = (mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,gen_rtx_MULT (mode, fake_reg, op1),mode, speed)
                - mtcs_expmed_neg_cost/*!neg_cost*/(self,speed, mode));
          if (max_cost <= 0)
            goto skip_synth;

          /* Special case powers of two.  */
          if (EXACT_POWER_OF_2_OR_ZERO_P (coeff)){
              rtx temp = mtcs_expmed_expand_shift/*!expand_shift*/(self,LSHIFT_EXPR, mode, op0,
                           floor_log2 (coeff), target, unsignedp);
              return mtcs_optabs_expand_unop(mtcsOptabs,mode, neg_optab, temp, target, 0);
          }

          if (mtcs_expmed_choose_mult_variant/*!choose_mult_variant*/(self,mode, coeff, &algorithm, (int*)&variant, max_cost)){
              rtx temp = expand_mult_const(self,mode, op0, coeff, NULL_RTX,&algorithm, variant);
              return mtcs_optabs_expand_unop(mtcsOptabs,mode, neg_optab, temp, target, 0);
          }
          goto skip_synth;
      }

      /* Exclude cost of op0 from max_cost to match the cost
     calculation of the synth_mult.  */
      max_cost = mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,gen_rtx_MULT (mode, fake_reg, op1), mode, speed);
      if (mtcs_expmed_choose_mult_variant/*!choose_mult_variant*/(self,mode, coeff, &algorithm, (int*)&variant, max_cost))
          return expand_mult_const(self,mode, op0, coeff, target, &algorithm, variant);
    }
 skip_synth:

  /* Expand x*2.0 as x+x.  */
  if (CONST_DOUBLE_AS_FLOAT_P (scalar_op1)
      && real_equal (CONST_DOUBLE_REAL_VALUE (scalar_op1), &dconst2)){
      op0 = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,GET_MODE (op0), op0);
      return mtcs_optabs_expand_binop(mtcsOptabs,mode, add_optab, op0, op0,
               target, unsignedp,
               no_libcall ? OPTAB_WIDEN : OPTAB_LIB_WIDEN);
    }

  /* This used to use umul_optab if unsigned, but for non-widening multiply
     there is no difference between signed and unsigned.  */
  op0 = mtcs_optabs_expand_binop(mtcsOptabs,mode, do_trapv ? smulv_optab : smul_optab,
              op0, op1, target, unsignedp,
              no_libcall ? OPTAB_WIDEN : OPTAB_LIB_WIDEN);
  gcc_assert (op0 || no_libcall);
  return op0;
}

/* Perform a widening multiplication and return an rtx for the result.
   MODE is mode of value; OP0 and OP1 are what to multiply (rtx's);
   TARGET is a suggestion for where to store the result (an rtx).
   THIS_OPTAB is the optab we should use, it must be either umul_widen_optab
   or smul_widen_optab.

   We check specially for a constant integer as OP1, comparing the
   cost of a widening multiply against the cost of a sequence of shifts
   and adds.  */
//原型 expand_widening_mult optabs.h expmed.cc
rtx mtcs_expmed_expand_widening_mult (MtcsExpmed *self,machine_mode mode, rtx op0, rtx op1, rtx target,
              int unsignedp, optab this_optab)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

  bool speed = optimize_insn_for_speed_p ();
  rtx cop1;

  if (CONST_INT_P (op1)
      && GET_MODE (op0) != VOIDmode
      && (cop1 = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,mode, GET_MODE (op0), op1,
                this_optab == umul_widen_optab))
      && CONST_INT_P (cop1)
      && (INTVAL (cop1) >= 0
      || mtcs_mode_is_hwi_computable_p/*!HWI_COMPUTABLE_MODE_P*/(mtcsMode,mode))){
      HOST_WIDE_INT coeff = INTVAL (cop1);
      int max_cost;
      enum mult_variant variant;
      struct algorithm algorithm;

      if (coeff == 0)
          return CONST0_RTX (mode);

      /* Special case powers of two.  */
      if (EXACT_POWER_OF_2_OR_ZERO_P (coeff)){
          op0 = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,mode, op0, this_optab == umul_widen_optab);
          return mtcs_expmed_expand_shift/*!expand_shift*/(self,LSHIFT_EXPR, mode, op0,
                       floor_log2 (coeff), target, unsignedp);
      }

      /* Exclude cost of op0 from max_cost to match the cost
     calculation of the synth_mult.  */
      max_cost = mtcs_expmed_mul_widen_cost/*!mul_widen_cost*/(self,speed, mode);
      if (mtcs_expmed_choose_mult_variant/*!choose_mult_variant*/(self,mode,
              coeff, &algorithm, (int*)&variant,max_cost)){
          op0 = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,mode, op0, this_optab == umul_widen_optab);
          return expand_mult_const (self,mode, op0, coeff, target, &algorithm, variant);
      }
  }
  return mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,mode, this_optab, op0, op1, target,
               unsignedp, OPTAB_LIB_WIDEN);
}

/* Output a shift instruction for expression code CODE,
   with SHIFTED being the rtx for the value to shift,
   and AMOUNT the tree for the amount to shift by.
   Store the result in the rtx TARGET, if that is convenient.
   If UNSIGNEDP is nonzero, do a logical shift; otherwise, arithmetic.
   Return the rtx for where the value is.  */
//原型 expand_variable_shift expmed.h expmed.cc
rtx mtcs_expmed_expand_variable_shift (MtcsExpmed *self,enum tree_code code, machine_mode mode, rtx shifted,
               tree amount, rtx target, int unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  return expand_shift_1 (self,code, mode,
             shifted, mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,amount), target, unsignedp);
}

/* Try to read the low bits of SRC as an rvalue of mode MODE, preserving
   the bit pattern.  SRC_MODE is the mode of SRC; if this is smaller than
   MODE, fill the upper bits with zeros.  Fail if the layout of either
   mode is unknown (as for CC modes) or if the extraction would involve
   unprofitable mode punning.  Return the value on success, otherwise
   return null.

   This is different from gen_lowpart* in these respects:

     - the returned value must always be considered an rvalue

     - when MODE is wider than SRC_MODE, the extraction involves
       a zero extension

     - when MODE is smaller than SRC_MODE, the extraction involves
       a truncation (and is thus subject to TARGET_TRULY_NOOP_TRUNCATION).

   In other words, this routine performs a computation, whereas the
   gen_lowpart* routines are conceptually lvalue or rvalue subreg
   operations.  */
//原型 extract_low_bits expmed.h expmed.cc
rtx mtcs_expmed_extract_low_bits (MtcsExpmed *self,machine_mode mode, machine_mode src_mode, rtx src)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

   scalar_int_mode int_mode, src_int_mode;

   if (mode == src_mode)
      return src;

   if (CONSTANT_P (src)){
      /* simplify_gen_subreg can't be used here, as if simplify_subreg
      fails, it will happily create (subreg (symbol_ref)) or similar
      invalid SUBREGs.  */
      poly_uint64 byte = mtcs_mode_subreg_lowpart_offset/*!subreg_lowpart_offset*/(mtcsMode,mode, src_mode);
      rtx ret =mtcs_simplify_rtx_subreg/*!simplify_subreg*/(mtcsSimplifyRtx,mode, src, src_mode, byte);
      if (ret)
         return ret;

      if (GET_MODE (src) == VOIDmode
      || !mtcs_rtl_validate_subreg/*!validate_subreg*/(mtcsRTL,mode, src_mode, src, byte))
         return NULL_RTX;

      src = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,GET_MODE (src), src);
      return mtcs_rtl_gen_rtx_SUBREG/*!gen_rtx_SUBREG*/(mtcsRTL,mode, src, byte);
   }

   if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) == MODE_CC
         || mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,src_mode) == MODE_CC)
      return NULL_RTX;

   if (known_eq (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode),
         mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,src_mode))
   && mtcsTarget/*!targetm.modes_tieable_p*/->modes_tieable_p(mtcsTarget,mode, src_mode)){
      rtx x = mtcs_rtl_gen_lowpart_common/*!gen_lowpart_common*/(mtcsRTL,mode, src);
      if (x)
         return x;
   }

   if (!mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,src_mode).exists (&src_int_mode)
   || !mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,mode).exists (&int_mode))
      return NULL_RTX;

   if (!mtcsTarget/*!targetm.modes_tieable_p*/->modes_tieable_p(mtcsTarget, src_int_mode, src_mode))
         return NULL_RTX;
   if (!mtcsTarget/*!targetm.modes_tieable_p*/->modes_tieable_p(mtcsTarget, int_mode, mode))
         return NULL_RTX;

   src = gen_lowpart (src_int_mode, src);
   if (!mtcs_rtl_validate_subreg/*!validate_subreg*/(mtcsRTL,int_mode, src_int_mode, src,
   mtcs_mode_subreg_lowpart_offset/*!subreg_lowpart_offset*/(mtcsMode,int_mode, src_int_mode)))
      return NULL_RTX;

   src = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,int_mode, src_int_mode, src, true);
   src = gen_lowpart (mode, src);
   return src;
}


MtcsExpmed *mtcs_expmed_new(MtcsMode *mtcsMode)
{
    MtcsExpmed *self = n_slice_alloc0 (sizeof(MtcsExpmed));
    mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
    mtcsExpmedInit(self);
    return self;
}


