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
 * base on optabs.cc
 */


/* This file handles generation of all the assembler code
   *optabs* the instructions of a function.
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
#include "optabs.h"
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
#include "optabs.h"
#include "libfuncs.h"

#include "optabs.h"
#include "expmed.h"
#include "emit-rtl.h"
#include "recog.h"
#include "diagnostic-core.h"
#include "rtx-vector-builder.h"

/* Include insn-config.h before expr.h so that HAVE_conditional_move
   is properly defined.  */
#include "stor-layout.h"
#include "except.h"
#include "dojump.h"
#include "explow.h"
#include "expr.h"
#include "optabs-tree.h"
#include "libfuncs.h"
#include "internal-fn.h"
#include "langhooks.h"
#include "gimple.h"
#include "ssa.h"

#include "mtcsreg.h"
#include "mtcsoptabs.h"
#include "mtcstarget.h"
#include "mtcstool.h"
#include "mtcsexpr.h"
#include "mtcsprintrtl.h"

//原型 no_conflict_data optabs.cc
struct no_conflict_data
{
  rtx target;
  rtx_insn *first, *insn;
  bool must_stay;
  MtcsOptabs *self;
};

/* Structure containing the pointers and values required to process the
   various forms of the atomic_fetch_op and atomic_op_fetch builtins.  */

struct atomic_op_functions
{
  direct_optab mem_fetch_before;
  direct_optab mem_fetch_after;
  direct_optab mem_no_result;
  optab fetch_before;
  optab fetch_after;
  direct_optab no_result;
  enum rtx_code reverse_code;
};


/* Enumerates the possible types of structure operand to an
   extraction_insn.  */
enum extraction_type { ET_unaligned_mem, ET_reg };

#define MTCS_GEN_FCN(CODE) (mtcsOutput->insn_data[CODE].genfun)

static rtx widen_operand (MtcsOptabs *self,rtx op, machine_mode mode, machine_mode oldmode,int unsignedp, bool no_extend);
//原型  emit_libcall_block_1 optabs.cc
static void emit_libcall_block_1 (MtcsOptabs *self,rtx_insn *insns, rtx target, rtx result, rtx equiv,bool equiv_may_trap);
//原型 prepare_libcall_arg optabs.cc
static rtx prepare_libcall_arg (MtcsOptabs *self,rtx arg, int uintp);
//原型 expand_unop_direct optabs.cc
static rtx expand_unop_direct (MtcsOptabs *self,machine_mode mode, optab unoptab, rtx op0, rtx target,int unsignedp);
static bool add_equal_note (MtcsOptabs *self,rtx_insn *insns, rtx target, enum rtx_code code, rtx op0,
                                    rtx op1, machine_mode op0_mode);
//原型 emit_conditional_move_1 optabs.cc
static rtx emit_conditional_move_1 (MtcsOptabs *self,rtx target, rtx comparison,rtx op2, rtx op3, machine_mode mode);
//原型 expand_doubleword_mod optabs.cc
static rtx expand_doubleword_mod (MtcsOptabs *self,machine_mode mode, rtx op0, rtx op1, bool unsignedp);
//原型 expand_doubleword_mult optabs.cc
static rtx expand_doubleword_mult (MtcsOptabs *self,machine_mode mode, rtx op0, rtx op1, rtx target,
               bool umulp, enum optab_methods methods);

//原型 expand_doubleword_shift_condmove optabs.cc
static bool expand_doubleword_shift_condmove (MtcsOptabs *self,scalar_int_mode op1_mode, optab binoptab,
                  enum rtx_code cmp_code, rtx cmp1, rtx cmp2,
                  rtx outof_input, rtx into_input,
                  rtx subword_op1, rtx superword_op1,
                  rtx outof_target, rtx into_target,
                  int unsignedp, enum optab_methods methods,
                  unsigned HOST_WIDE_INT shift_mask);

//原型 expand_superword_shift optabs.cc
static bool expand_superword_shift (MtcsOptabs *self,optab binoptab, rtx outof_input, rtx superword_op1,
            rtx outof_target, rtx into_target,int unsignedp, enum optab_methods methods);

//原型 expand_subword_shift optabs.cc
static bool expand_subword_shift (MtcsOptabs *self,scalar_int_mode op1_mode, optab binoptab,
              rtx outof_input, rtx into_input, rtx op1,
              rtx outof_target, rtx into_target,
              int unsignedp, enum optab_methods methods,
              unsigned HOST_WIDE_INT shift_mask);

//原型 expand_doubleword_shift optabs.cc
static bool expand_doubleword_shift (MtcsOptabs *self,scalar_int_mode op1_mode, optab binoptab,
             rtx outof_input, rtx into_input, rtx op1,
             rtx outof_target, rtx into_target,
             int unsignedp, enum optab_methods methods,
             unsigned HOST_WIDE_INT shift_mask);

//原型 expand_ctz optabs.cc
static rtx expand_ctz (MtcsOptabs *self,scalar_int_mode mode, rtx op0, rtx target);

static void prepare_float_lib_cmp (MtcsOptabs *self,rtx x, rtx y, enum rtx_code comparison,rtx *ptest, machine_mode *pmode);
//原型 expand_doubleword_bswap optabs.cc
static rtx expand_doubleword_bswap (MtcsOptabs *self,machine_mode mode, rtx op, rtx target);
//原型 expand_ffs optabs.cc
static rtx expand_ffs(MtcsOptabs *self,scalar_int_mode mode, rtx op0, rtx target);
//原型 widen_bswap optabs.cc
static rtx widen_bswap (MtcsOptabs *self,scalar_int_mode mode, rtx op0, rtx target);

static void mtcsOptabsInit(MtcsOptabs *self)
{

}


/* Try calculating
    (bswap:narrow x)
   as
    (lshiftrt:wide (bswap:wide x) ((width wide) - (width narrow))).  */
//原型 widen_bswap optabs.cc
static rtx widen_bswap (MtcsOptabs *self,scalar_int_mode mode, rtx op0, rtx target)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsExpmed   *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);

  rtx x;
  rtx_insn *last;
  opt_scalar_int_mode wider_mode_iter;

  MTCS_FOR_EACH_WIDER_MODE (mtcsMode,wider_mode_iter, mode)
    if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,bswap_optab, wider_mode_iter.require ())!= CODE_FOR_nothing)
      break;

  if (!wider_mode_iter.exists ())
    return NULL_RTX;

  scalar_int_mode wider_mode = wider_mode_iter.require ();
  last = mtcs_rtl_data_get_last_insn(mtcsRtlData);

  x = widen_operand(self,op0, wider_mode, mode, true, true);
  x = mtcs_optabs_expand_unop(self,wider_mode, bswap_optab, x, NULL_RTX, true);

  gcc_assert (mtcs_mode_get_precision(mtcsMode,wider_mode) == mtcs_mode_get_bitsize(mtcsMode,wider_mode)
          && mtcs_mode_get_precision(mtcsMode,mode) == mtcs_mode_get_bitsize(mtcsMode,mode));
  if (x != 0)
    x = mtcs_expmed_expand_shift(mtcsExpmed,RSHIFT_EXPR, wider_mode, x,
            mtcs_mode_get_bitsize(mtcsMode,wider_mode)
              - mtcs_mode_get_bitsize(mtcsMode,mode),
              NULL_RTX, true);

  if (x != 0){
      if (target == 0)
          target = mtcs_emit_gen_reg_rtx(mtcsEmit,mode);
      mtcs_expr_emit_move_insn(mtcsExpr,target, gen_lowpart (mode, x));
  }else
     mtcs_rtl_data_delete_insns_since (mtcsRtlData,last);

  return target;
}


/* Called via note_stores by emit_libcall_block.  Set P->must_stay if
   the currently examined clobber / store has to stay in the list of
   insns that constitute the actual libcall block.  */
static void no_conflict_move_test (rtx dest, const_rtx set, void *p0)
{
  struct no_conflict_data *p= (struct no_conflict_data *) p0;
  MtcsOptabs *self=p->self;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  /* If this inns directly contributes to setting the target, it must stay.  */
  if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,p->target, dest))
    p->must_stay = true;
  /* If we haven't committed to keeping any other insns in the list yet,
     there is nothing more to check.  */
  else if (p->insn == p->first)
    return;
  /* If this insn sets / clobbers a register that feeds one of the insns
     already in the list, this insn has to stay too.  */
  else if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,dest, PATTERN (p->first))
       || (CALL_P (p->first) && (mtcs_rtlanal_find_reg_fusage/*!find_reg_fusage*/(mtcsRtlanal,p->first, USE, dest)))
       || reg_used_between_p (dest, p->first, p->insn)
       /* Likewise if this insn depends on a register set by a previous
          insn in the list, or if it sets a result (presumably a hard
          register) that is set or clobbered by a previous insn.
          N.B. the modified_*_p (SET_DEST...) tests applied to a MEM
          SET_DEST perform the former check on the address, and the latter
          check on the MEM.  */
       || (GET_CODE (set) == SET
           && (modified_in_p (SET_SRC (set), p->first)
           || modified_in_p (SET_DEST (set), p->first)
           || mtcs_rtlanal_modified_between_p/*!modified_between_p*/(mtcsRtlanal,SET_SRC (set), p->first, p->insn)
           || mtcs_rtlanal_modified_between_p/*!modified_between_p*/(mtcsRtlanal,SET_DEST (set), p->first, p->insn))))
    p->must_stay = true;
}

/* Try calculating bswap as two bswaps of two word-sized operands.  */
//原型 expand_doubleword_bswap optabs.cc
static rtx expand_doubleword_bswap (MtcsOptabs *self,machine_mode mode, rtx op, rtx target)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

  rtx t0, t1;
  t1 = mtcs_optabs_expand_unop(self,mtcsMode->word_mode, bswap_optab,
          mtcs_rtl_operand_subword_force(mtcsRTL,op, 0, mode), NULL_RTX, true);
  t0 = mtcs_optabs_expand_unop(self,mtcsMode->word_mode, bswap_optab,
          mtcs_rtl_operand_subword_force(mtcsRTL,op, 1, mode), NULL_RTX, true);
  if (target == 0 || !mtcs_optabs_valid_multiword_target_p(self,target))
    target = mtcs_emit_gen_reg_rtx(mtcsEmit,mode);
  if (REG_P (target))
      mtcs_emit_emit_clobber(mtcsEmit,target);
  mtcs_expr_emit_move_insn(mtcsExpr,operand_subword (target, 0, 1, mode), t0);
  mtcs_expr_emit_move_insn(mtcsExpr,operand_subword (target, 1, 1, mode), t1);

  return target;
}

/* Try calculating ffs(x) using ctz(x) if we have that instruction, or
   else with the sequence used by expand_clz.

   The ffs builtin promises to return zero for a zero value and ctz/clz
   may have an undefined value in that case.  If they do not give us a
   convenient value, we have to generate a test and branch.  */
//原型 expand_ffs optabs.cc
static rtx expand_ffs(MtcsOptabs *self,scalar_int_mode mode, rtx op0, rtx target)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

  HOST_WIDE_INT val = 0;
  bool defined_at_zero = false;
  rtx temp;
  rtx_insn *seq;
  if (mtcs_opinit_optab_handler(mtcsOpinit,ctz_optab, mode) != CODE_FOR_nothing){
      mtcs_emit_start_sequence(mtcsEmit);
      temp = expand_unop_direct(self,mode, ctz_optab, op0, 0, true);
      if (!temp)
          goto fail;
      defined_at_zero = (mtcs_mode_ctz_defined_value_at_zero/*!CTZ_DEFINED_VALUE_AT_ZERO*/(mtcsMode,mode, val) == 2);
  }else if (mtcs_opinit_optab_handler(mtcsOpinit,clz_optab, mode) != CODE_FOR_nothing){
      mtcs_emit_start_sequence(mtcsEmit);
      temp = expand_ctz(self,mode, op0, 0);
      if (!temp)
          goto fail;
      if (mtcs_mode_clz_defined_value_at_zero/*!CLZ_DEFINED_VALUE_AT_ZERO*/(mtcsMode,mode,(int*)&val) == 2){
          defined_at_zero = true;
          val = (mtcs_mode_get_precision(mtcsMode,mode) - 1) - val;
      }
  }else
    return 0;

  if (defined_at_zero && val == -1)
    /* No correction needed at zero.  */;
  else{
      /* We don't try to do anything clever with the situation found
     on some processors (eg Alpha) where ctz(0:mode) ==
     bitsize(mode).  If someone can think of a way to send N to -1
     and leave alone all values in the range 0..N-1 (where N is a
     power of two), cheaper than this test-and-branch, please add it.
     The test-and-branch is done after the operation itself, in case
     the operation sets condition codes that can be recycled for this.
     (This is true on i386, for instance.)  */
      rtx_code_label *nonzero_label =mtcs_rtl_gen_label_rtx(mtcsRTL);
      mtcs_optabs_emit_cmp_and_jump_insns(self,op0, CONST0_RTX (mode), NE, 0,mode, true, nonzero_label);
      mtcs_expr_convert_move(mtcsExpr,temp, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,-1), false);
      mtcs_emit_emit_label(mtcsEmit,nonzero_label);
  }
  /* temp now has a value in the range -1..bitsize-1.  ffs is supposed
     to produce a value in the range 0..bitsize.  */
  temp = mtcs_optabs_expand_binop(self,mode, add_optab, temp,
          mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,1, mode),target, false, OPTAB_DIRECT);
  if (!temp)
    goto fail;
  seq = mtcs_rtl_data_get_insns (mtcsRtlData);
  mtcs_emit_end_sequence(mtcsEmit);
  add_equal_note(self,seq, temp, FFS, op0, NULL_RTX, mode);
  mtcs_emit_emit_insn(mtcsEmit,seq);
  return temp;
 fail:
  mtcs_emit_end_sequence(mtcsEmit);
  return 0;
}

/* Try calculating ctz(x) as K - clz(x & -x) ,
   where K is GET_MODE_PRECISION(mode) - 1.

   Both __builtin_ctz and __builtin_clz are undefined at zero, so we
   don't have to worry about what the hardware does in that case.  (If
   the clz instruction produces the usual value at 0, which is K, the
   result of this code sequence will be -1; expand_ffs, below, relies
   on this.  It might be nice to have it be K instead, for consistency
   with the (very few) processors that provide a ctz with a defined
   value, but that would take one more instruction, and it would be
   less convenient for expand_ffs anyway.  */
//原型 expand_ctz optabs.cc
static rtx expand_ctz (MtcsOptabs *self,scalar_int_mode mode, rtx op0, rtx target)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);

  rtx_insn *seq;
  rtx temp;
  if (mtcs_opinit_optab_handler(mtcsOpinit,clz_optab, mode) == CODE_FOR_nothing)
    return 0;
  mtcs_emit_start_sequence(mtcsEmit);
  temp = expand_unop_direct(self,mode, neg_optab, op0, NULL_RTX, true);
  if (temp)
    temp = mtcs_optabs_expand_binop(self,mode, and_optab, op0, temp, NULL_RTX,true, OPTAB_DIRECT);
  if (temp)
    temp = expand_unop_direct(self,mode, clz_optab, temp, NULL_RTX, true);
  if (temp)
    temp = mtcs_optabs_expand_binop(self,mode, sub_optab,mtcs_rtl_gen_int_mode(mtcsRTL,mtcs_mode_get_precision(mtcsMode,mode) - 1,mode),
             temp, target,true, OPTAB_DIRECT);
  if (temp == 0){
      mtcs_emit_end_sequence(mtcsEmit);
      return 0;
  }
  seq = mtcs_rtl_data_get_insns(mtcsRtlData);
  mtcs_emit_end_sequence(mtcsEmit);
  add_equal_note(self,seq, temp, CTZ, op0, NULL_RTX, mode);
  mtcs_emit_emit_insn(mtcsEmit,seq);
  return temp;
}


/* Return true if an optab exists to perform an insertion or extraction
   of type TYPE in mode MODE.  Describe the instruction in *INSN if so.

   REG_OPTAB is the optab to use for register structures and
   MISALIGN_OPTAB is the optab to use for misaligned memory structures.
   POS_OP is the operand number of the bit position.  */
//原型 get_optab_extraction_insn optabs-query.cc
static bool get_optab_extraction_insn (MtcsOptabs *self,class extraction_insn *insn,
               enum extraction_type type,machine_mode mode, direct_optab reg_optab,direct_optab misalign_optab, int pos_op)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);

  direct_optab optab = (type == ET_unaligned_mem ? misalign_optab : reg_optab);
  enum insn_code icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,optab, mode);
  if (icode == CODE_FOR_nothing)
    return false;

  const struct insn_data_d *data = &mtcsOutput->insn_data[icode];/*!&insn_data[icode];*/

  machine_mode pos_mode = data->operand[pos_op].mode;
  if (pos_mode == VOIDmode)
    pos_mode = mtcsMode->word_mode;

  insn->icode = icode;
  insn->field_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,mode);
  if (type == ET_unaligned_mem)
    insn->struct_mode = opt_scalar_int_mode ();
  else
    insn->struct_mode = insn->field_mode;
  insn->pos_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,pos_mode);
  return true;
}

/* Expand a doubleword shift (ashl, ashr or lshr) using word-mode shifts.
   OUTOF_INPUT and INTO_INPUT are the two word-sized halves of the first
   input operand; the shift moves bits in the direction OUTOF_INPUT->
   INTO_TARGET.  OUTOF_TARGET and INTO_TARGET are the equivalent words
   of the target.  OP1 is the shift count and OP1_MODE is its mode.
   If OP1 is constant, it will have been truncated as appropriate
   and is known to be nonzero.

   If SHIFT_MASK is zero, the result of word shifts is undefined when the
   shift count is outside the range [0, BITS_PER_WORD).  This routine must
   avoid generating such shifts for OP1s in the range [0, BITS_PER_WORD * 2).

   If SHIFT_MASK is nonzero, all word-mode shift counts are effectively
   masked by it and shifts in the range [BITS_PER_WORD, SHIFT_MASK) will
   fill with zeros or sign bits as appropriate.

   If SHIFT_MASK is BITS_PER_WORD - 1, this routine will synthesize
   a doubleword shift whose equivalent mask is BITS_PER_WORD * 2 - 1.
   Doing this preserves semantics required by SHIFT_COUNT_TRUNCATED.
   In all other cases, shifts by values outside [0, BITS_PER_UNIT * 2)
   are undefined.

   BINOPTAB, UNSIGNEDP and METHODS are as for expand_binop.  This function
   may not use INTO_INPUT after modifying INTO_TARGET, and similarly for
   OUTOF_INPUT and OUTOF_TARGET.  OUTOF_TARGET can be null if the parent
   function wants to calculate it itself.

   Return true if the shift could be successfully synthesized.  */
//原型 expand_doubleword_shift optabs.cc
static bool expand_doubleword_shift (MtcsOptabs *self,scalar_int_mode op1_mode, optab binoptab,
             rtx outof_input, rtx into_input, rtx op1,
             rtx outof_target, rtx into_target,
             int unsignedp, enum optab_methods methods,
             unsigned HOST_WIDE_INT shift_mask)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  rtx superword_op1, tmp, cmp1, cmp2;
  enum rtx_code cmp_code;

  /* See if word-mode shifts by BITS_PER_WORD...BITS_PER_WORD * 2 - 1 will
     fill the result with sign or zero bits as appropriate.  If so, the value
     of OUTOF_TARGET will always be (SHIFT OUTOF_INPUT OP1).   Recursively call
     this routine to calculate INTO_TARGET (which depends on both OUTOF_INPUT
     and INTO_INPUT), then emit code to set up OUTOF_TARGET.

     This isn't worthwhile for constant shifts since the optimizers will
     cope better with in-range shift counts.  */
  if (shift_mask >= BITS_PER_WORD  && outof_target != 0  && !CONSTANT_P (op1)){
      if (!expand_doubleword_shift(self,op1_mode, binoptab,
                    outof_input, into_input, op1,0, into_target,unsignedp, methods, shift_mask))
          return false;
      if (!mtcs_optabs_force_expand_binop(self,mtcsMode->word_mode, binoptab, outof_input, op1,outof_target, unsignedp, methods))
          return false;
      return true;
  }

  /* Set CMP_CODE, CMP1 and CMP2 so that the rtx (CMP_CODE CMP1 CMP2)
     is true when the effective shift value is less than BITS_PER_WORD.
     Set SUPERWORD_OP1 to the shift count that should be used to shift
     OUTOF_INPUT into INTO_TARGET when the condition is false.  */
  tmp = mtcs_rtl_immed_wide_int_const(mtcsRTL,wi::shwi (BITS_PER_WORD, op1_mode), op1_mode);
  if (!CONSTANT_P (op1) && shift_mask == BITS_PER_WORD - 1){
      /* Set CMP1 to OP1 & BITS_PER_WORD.  The result is zero iff OP1
     is a subword shift count.  */
      cmp1 = mtcs_optabs_simplify_expand_binop(self,op1_mode, and_optab, op1, tmp,0, true, methods);
      cmp2 = CONST0_RTX (op1_mode);
      cmp_code = EQ;
      superword_op1 = op1;
  }else{
      /* Set CMP1 to OP1 - BITS_PER_WORD.  */
      cmp1 = mtcs_optabs_simplify_expand_binop(self,op1_mode, sub_optab, op1, tmp,0, true, methods);
      cmp2 = CONST0_RTX (op1_mode);
      cmp_code = LT;
      superword_op1 = cmp1;
  }
  if (cmp1 == 0)
    return false;

  /* If we can compute the condition at compile time, pick the
     appropriate subroutine.  */
  tmp = mtcs_simplify_rtx_relational_operation(mtcsSimplifyRtx,cmp_code, mtcsMode->modes.M_SImode, op1_mode, cmp1, cmp2);
  if (tmp != 0 && CONST_INT_P (tmp)){
      if (tmp == const0_rtx)
        return expand_superword_shift(self,binoptab, outof_input, superword_op1,
                           outof_target, into_target,unsignedp, methods);
      else
        return expand_subword_shift (self,op1_mode, binoptab,outof_input, into_input, op1,
                         outof_target, into_target,unsignedp, methods, shift_mask);
  }

  /* Try using conditional moves to generate straight-line code.  */
  if (HAVE_conditional_move){
      rtx_insn *start = mtcs_rtl_data_get_last_insn(mtcsRtlData);
      if (expand_doubleword_shift_condmove(self,op1_mode, binoptab,cmp_code, cmp1, cmp2,outof_input, into_input,
                        op1, superword_op1,outof_target, into_target,unsignedp, methods, shift_mask))
          return true;
      mtcs_rtl_data_delete_insns_since (mtcsRtlData,start);
  }

  /* As a last resort, use branches to select the correct alternative.  */
  rtx_code_label *subword_label =mtcs_rtl_gen_label_rtx(mtcsRTL);
  rtx_code_label *done_label =mtcs_rtl_gen_label_rtx(mtcsRTL);

  /*!NO_DEFER_POP; expr.h 定义*/
  mtcsRtlData/*!crtl*/->expr.x_inhibit_defer_pop+=1;
  mtcs_dojump_do_compare_rtx_and_jump(mtcsDojump,cmp1, cmp2, cmp_code, false, op1_mode,
               0, 0, subword_label,profile_probability::uninitialized ());
  /*!OK_DEFER_POP;expr.h 定义*/
  mtcsRtlData/*!crtl*/->expr.x_inhibit_defer_pop-=1;
  if (!expand_superword_shift(self,binoptab, outof_input, superword_op1,
                   outof_target, into_target,unsignedp, methods))
    return false;

  mtcs_emit_emit_jump_insn/*!emit_jump_insn*/(mtcsEmit,target_rtx_gen_jump/*!targetm.gen_jump*/(mtcsMachine->tmrtx,done_label));
  mtcs_emit_emit_barrier(mtcsEmit);
  mtcs_emit_emit_label(mtcsEmit,subword_label);

  if (!expand_subword_shift(self,op1_mode, binoptab, outof_input, into_input, op1,
                 outof_target, into_target,unsignedp, methods, shift_mask))
    return false;

  mtcs_emit_emit_label(mtcsEmit,done_label);
  return true;
}


/* This subroutine of expand_doubleword_shift handles the cases in which
   the effective shift value is < BITS_PER_WORD.  The arguments and return
   value are the same as for the parent routine.  */
//原型 expand_subword_shift optabs.cc
static bool expand_subword_shift (MtcsOptabs *self,scalar_int_mode op1_mode, optab binoptab,
              rtx outof_input, rtx into_input, rtx op1,
              rtx outof_target, rtx into_target,
              int unsignedp, enum optab_methods methods,
              unsigned HOST_WIDE_INT shift_mask)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

  optab reverse_unsigned_shift, unsigned_shift;
  rtx tmp, carries;

  reverse_unsigned_shift = (binoptab == ashl_optab ? lshr_optab : ashl_optab);
  unsigned_shift = (binoptab == ashl_optab ? ashl_optab : lshr_optab);

  /* The low OP1 bits of INTO_TARGET come from the high bits of OUTOF_INPUT.
     We therefore need to shift OUTOF_INPUT by (BITS_PER_WORD - OP1) bits in
     the opposite direction to BINOPTAB.  */
  if (CONSTANT_P (op1) || shift_mask >= BITS_PER_WORD){
      carries = outof_input;
      tmp = mtcs_rtl_immed_wide_int_const(mtcsRTL,wi::shwi (BITS_PER_WORD,op1_mode), op1_mode);
      tmp = mtcs_optabs_simplify_expand_binop(self,op1_mode, sub_optab, tmp, op1,0, true, methods);
  }else{
      /* We must avoid shifting by BITS_PER_WORD bits since that is either
     the same as a zero shift (if shift_mask == BITS_PER_WORD - 1) or
     has unknown behavior.  Do a single shift first, then shift by the
     remainder.  It's OK to use ~OP1 as the remainder if shift counts
     are truncated to the mode size.  */
      carries = mtcs_optabs_simplify_expand_binop(self,mtcsMode->word_mode, reverse_unsigned_shift,
                       outof_input, const1_rtx, 0,unsignedp, methods);
      if (carries == const0_rtx)
          tmp = const0_rtx;
      else if (shift_mask == BITS_PER_WORD - 1)
          tmp = mtcs_optabs_expand_unop(self,op1_mode, one_cmpl_optab, op1, 0, true);
      else{
          tmp = mtcs_rtl_immed_wide_int_const(mtcsRTL,wi::shwi (BITS_PER_WORD - 1,op1_mode), op1_mode);
          tmp = mtcs_optabs_simplify_expand_binop(self,op1_mode, sub_optab, tmp, op1,0, true, methods);
      }
  }
  if (tmp == 0 || carries == 0)
    return false;
  if (carries != const0_rtx && tmp != const0_rtx)
    carries = mtcs_optabs_simplify_expand_binop(self,mtcsMode->word_mode, reverse_unsigned_shift,carries, tmp, 0, unsignedp, methods);
  if (carries == 0)
    return false;

  if (into_input != const0_rtx){
      /* Shift INTO_INPUT logically by OP1.  This is the last use of
     INTO_INPUT so the result can go directly into INTO_TARGET if
     convenient.  */
      tmp = mtcs_optabs_simplify_expand_binop(self,mtcsMode->word_mode, unsigned_shift, into_input,op1, into_target, unsignedp, methods);
      if (tmp == 0)
          return false;

      /* Now OR in the bits carried over from OUTOF_INPUT.  */
      if (!mtcs_optabs_force_expand_binop(self,mtcsMode->word_mode, ior_optab, tmp, carries,into_target, unsignedp, methods))
          return false;
  }else
      mtcs_expr_emit_move_insn(mtcsExpr,into_target, carries);

  /* Use a standard word_mode shift for the out-of half.  */
  if (outof_target != 0)
    if (!mtcs_optabs_force_expand_binop(self,mtcsMode->word_mode, binoptab, outof_input, op1,outof_target, unsignedp, methods))
      return false;

  return true;
}

/* This subroutine of expand_doubleword_shift handles the cases in which
   the effective shift value is >= BITS_PER_WORD.  The arguments and return
   value are the same as for the parent routine, except that SUPERWORD_OP1
   is the shift count to use when shifting OUTOF_INPUT into INTO_TARGET.
   INTO_TARGET may be null if the caller has decided to calculate it.  */
//原型 expand_superword_shift optabs.cc
static bool expand_superword_shift (MtcsOptabs *self,optab binoptab, rtx outof_input, rtx superword_op1,
            rtx outof_target, rtx into_target,
            int unsignedp, enum optab_methods methods)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

  if (into_target != 0)
    if (!mtcs_optabs_force_expand_binop(self,mtcsMode->word_mode, binoptab, outof_input, superword_op1,
                 into_target, unsignedp, methods))
      return false;

  if (outof_target != 0){
      /* For a signed right shift, we must fill OUTOF_TARGET with copies
     of the sign bit, otherwise we must fill it with zeros.  */
      if (binoptab != ashr_optab)
          mtcs_expr_emit_move_insn(mtcsExpr,outof_target, CONST0_RTX (mtcsMode->word_mode));
      else
    if (!mtcs_optabs_force_expand_binop(self,mtcsMode->word_mode, binoptab, outof_input,
            mtcs_rtl_gen_int_shift_amount(mtcsRTL,mtcsMode->word_mode,BITS_PER_WORD - 1),outof_target, unsignedp, methods))
      return false;
  }
  return true;
}

/* Try implementing expand_doubleword_shift using conditional moves.
   The shift is by < BITS_PER_WORD if (CMP_CODE CMP1 CMP2) is true,
   otherwise it is by >= BITS_PER_WORD.  SUBWORD_OP1 and SUPERWORD_OP1
   are the shift counts to use in the former and latter case.  All other
   arguments are the same as the parent routine.  */
//原型 expand_doubleword_shift_condmove optabs.cc
static bool expand_doubleword_shift_condmove (MtcsOptabs *self,scalar_int_mode op1_mode, optab binoptab,
                  enum rtx_code cmp_code, rtx cmp1, rtx cmp2,
                  rtx outof_input, rtx into_input,
                  rtx subword_op1, rtx superword_op1,
                  rtx outof_target, rtx into_target,
                  int unsignedp, enum optab_methods methods,
                  unsigned HOST_WIDE_INT shift_mask)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

  rtx outof_superword, into_superword;

  /* Put the superword version of the output into OUTOF_SUPERWORD and
     INTO_SUPERWORD.  */
  outof_superword = outof_target != 0 ? mtcs_emit_gen_reg_rtx(mtcsEmit,mtcsMode->word_mode) : 0;
  if (outof_target != 0 && subword_op1 == superword_op1){
      /* The value INTO_TARGET >> SUBWORD_OP1, which we later store in
     OUTOF_TARGET, is the same as the value of INTO_SUPERWORD.  */
      into_superword = outof_target;
      if (!expand_superword_shift(self,binoptab, outof_input, superword_op1,
                   outof_superword, 0, unsignedp, methods))
          return false;
  }else{
      into_superword = mtcs_emit_gen_reg_rtx(mtcsEmit,mtcsMode->word_mode);
      if (!expand_superword_shift(self,binoptab, outof_input, superword_op1,
                   outof_superword, into_superword,
                   unsignedp, methods))
          return false;
  }

  /* Put the subword version directly in OUTOF_TARGET and INTO_TARGET.  */
  if (!expand_subword_shift(self,op1_mode, binoptab,
                 outof_input, into_input, subword_op1,
                 outof_target, into_target,
                 unsignedp, methods, shift_mask))
    return false;

  /* Select between them.  Do the INTO half first because INTO_SUPERWORD
     might be the current value of OUTOF_TARGET.  */
  if (!mtcs_optabs_emit_conditional_move(self,into_target, { cmp_code, cmp1, cmp2, op1_mode },
                  into_target, into_superword, mtcsMode->word_mode, false))
    return false;

  if (outof_target != 0)
    if (!mtcs_optabs_emit_conditional_move(self,outof_target,{ cmp_code, cmp1, cmp2, op1_mode },
                outof_target, outof_superword,mtcsMode->word_mode, false))
      return false;

  return true;
}

/* Subroutine of expand_binop.  Perform a double word multiplication of
   operands OP0 and OP1 both of mode MODE, which is exactly twice as wide
   as the target's word_mode.  This function return NULL_RTX if anything
   goes wrong, in which case it may have already emitted instructions
   which need to be deleted.

   If we want to multiply two two-word values and have normal and widening
   multiplies of single-word values, we can do this with three smaller
   multiplications.

   The multiplication proceeds as follows:
                     _______________________
                    [__op0_high_|__op0_low__]
                     _______________________
        *           [__op1_high_|__op1_low__]
        _______________________________________________
                     _______________________
    (1)             [__op0_low__*__op1_low__]
             _______________________
    (2a)        [__op0_low__*__op1_high_]
             _______________________
    (2b)        [__op0_high_*__op1_low__]
         _______________________
    (3) [__op0_high_*__op1_high_]


  This gives a 4-word result.  Since we are only interested in the
  lower 2 words, partial result (3) and the upper words of (2a) and
  (2b) don't need to be calculated.  Hence (2a) and (2b) can be
  calculated using non-widening multiplication.

  (1), however, needs to be calculated with an unsigned widening
  multiplication.  If this operation is not directly supported we
  try using a signed widening multiplication and adjust the result.
  This adjustment works as follows:

      If both operands are positive then no adjustment is needed.

      If the operands have different signs, for example op0_low < 0 and
      op1_low >= 0, the instruction treats the most significant bit of
      op0_low as a sign bit instead of a bit with significance
      2**(BITS_PER_WORD-1), i.e. the instruction multiplies op1_low
      with 2**BITS_PER_WORD - op0_low, and two's complements the
      result.  Conclusion: We need to add op1_low * 2**BITS_PER_WORD to
      the result.

      Similarly, if both operands are negative, we need to add
      (op0_low + op1_low) * 2**BITS_PER_WORD.

      We use a trick to adjust quickly.  We logically shift op0_low right
      (op1_low) BITS_PER_WORD-1 steps to get 0 or 1, and add this to
      op0_high (op1_high) before it is used to calculate 2b (2a).  If no
      logical shift exists, we do an arithmetic right shift and subtract
      the 0 or -1.  */
//原型 expand_doubleword_mult optabs.cc
static rtx expand_doubleword_mult (MtcsOptabs *self,machine_mode mode, rtx op0, rtx op1, rtx target,
               bool umulp, enum optab_methods methods)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

  int low = (WORDS_BIG_ENDIAN ? 1 : 0);
  int high = (WORDS_BIG_ENDIAN ? 0 : 1);
  rtx wordm1 = (umulp ? NULL_RTX: mtcs_rtl_gen_int_shift_amount(mtcsRTL,mtcsMode->word_mode, BITS_PER_WORD - 1));
  rtx product, adjust, product_high, temp;

  rtx op0_high = mtcs_rtl_operand_subword_force(mtcsRTL,op0, high, mode);
  rtx op0_low = mtcs_rtl_operand_subword_force(mtcsRTL,op0, low, mode);
  rtx op1_high = mtcs_rtl_operand_subword_force(mtcsRTL,op1, high, mode);
  rtx op1_low = mtcs_rtl_operand_subword_force(mtcsRTL,op1, low, mode);

  /* If we're using an unsigned multiply to directly compute the product
     of the low-order words of the operands and perform any required
     adjustments of the operands, we begin by trying two more multiplications
     and then computing the appropriate sum.

     We have checked above that the required addition is provided.
     Full-word addition will normally always succeed, especially if
     it is provided at all, so we don't worry about its failure.  The
     multiplication may well fail, however, so we do handle that.  */

  if (!umulp){
      /* ??? This could be done with emit_store_flag where available.  */
      temp = mtcs_optabs_expand_binop(self,mtcsMode->word_mode, lshr_optab, op0_low, wordm1,NULL_RTX, 1, methods);
      if (temp)
          op0_high = mtcs_optabs_expand_binop(self,mtcsMode->word_mode, add_optab, op0_high, temp,NULL_RTX, 0, OPTAB_DIRECT);
      else{
          temp = mtcs_optabs_expand_binop(self,mtcsMode->word_mode, ashr_optab, op0_low, wordm1,NULL_RTX, 0, methods);
          if (!temp)
            return NULL_RTX;
          op0_high = mtcs_optabs_expand_binop(self,mtcsMode->word_mode, sub_optab, op0_high, temp,NULL_RTX, 0, OPTAB_DIRECT);
      }

      if (!op0_high)
          return NULL_RTX;
  }

  adjust = mtcs_optabs_expand_binop(self,mtcsMode->word_mode, smul_optab, op0_high, op1_low,NULL_RTX, 0, OPTAB_DIRECT);
  if (!adjust)
    return NULL_RTX;

  /* OP0_HIGH should now be dead.  */
  if (!umulp){
      /* ??? This could be done with emit_store_flag where available.  */
      temp = mtcs_optabs_expand_binop(self,mtcsMode->word_mode, lshr_optab, op1_low, wordm1,NULL_RTX, 1, methods);
      if (temp)
          op1_high = mtcs_optabs_expand_binop(self,mtcsMode->word_mode, add_optab, op1_high, temp,NULL_RTX, 0, OPTAB_DIRECT);
      else{
          temp = mtcs_optabs_expand_binop(self,mtcsMode->word_mode, ashr_optab, op1_low, wordm1,NULL_RTX, 0, methods);
          if (!temp)
            return NULL_RTX;
          op1_high = mtcs_optabs_expand_binop(self,mtcsMode->word_mode, sub_optab, op1_high, temp,NULL_RTX, 0, OPTAB_DIRECT);
      }

      if (!op1_high)
          return NULL_RTX;
  }

  temp = mtcs_optabs_expand_binop(self,mtcsMode->word_mode, smul_optab, op1_high, op0_low,
               NULL_RTX, 0, OPTAB_DIRECT);
  if (!temp)
    return NULL_RTX;

  /* OP1_HIGH should now be dead.  */

  adjust = mtcs_optabs_expand_binop(self,mtcsMode->word_mode, add_optab, adjust, temp,
             NULL_RTX, 0, OPTAB_DIRECT);

  if (target && !REG_P (target))
    target = NULL_RTX;

  /* *_widen_optab needs to determine operand mode, make sure at least
     one operand has non-VOID mode.  */
  if (GET_MODE (op0_low) == VOIDmode && GET_MODE (op1_low) == VOIDmode)
    op0_low =  mtcs_explow_force_reg(mtcsExplow,mtcsMode->word_mode, op0_low);

  if (umulp)
    product = mtcs_optabs_expand_binop(self,mode, umul_widen_optab, op0_low, op1_low,target, 1, OPTAB_DIRECT);
  else
    product = mtcs_optabs_expand_binop(self,mode, smul_widen_optab, op0_low, op1_low,target, 1, OPTAB_DIRECT);

  if (!product)
    return NULL_RTX;

  product_high = mtcs_rtl_operand_subword/*!operand_subword*/(mtcsRTL,product, high, 1, mode);
  adjust = mtcs_optabs_expand_binop(self,mtcsMode->word_mode, add_optab, product_high, adjust,
             NULL_RTX, 0, OPTAB_DIRECT);
  mtcs_expr_emit_move_insn(mtcsExpr,product_high, adjust);
  return product;
}



/* Subroutine of expand_binop.  Optimize unsigned double-word OP0 % OP1 for
   constant OP1.  If for some bit in [BITS_PER_WORD / 2, BITS_PER_WORD] range
   (prefer higher bits) ((1w << bit) % OP1) == 1, then the modulo can be
   computed in word-mode as ((OP0 & (bit - 1)) + ((OP0 >> bit) & (bit - 1))
   + (OP0 >> (2 * bit))) % OP1.  Whether we need to sum 2, 3 or 4 values
   depends on the bit value, if 2, then carry from the addition needs to be
   added too, i.e. like:
   sum += __builtin_add_overflow (low, high, &sum)

   Optimize signed double-word OP0 % OP1 similarly, just apply some correction
   factor to the sum before doing unsigned remainder, in the form of
   sum += (((signed) OP0 >> (2 * BITS_PER_WORD - 1)) & const);
   then perform unsigned
   remainder = sum % OP1;
   and finally
   remainder += ((signed) OP0 >> (2 * BITS_PER_WORD - 1)) & (1 - OP1);  */
//原型 expand_doubleword_mod optabs.cc
static rtx expand_doubleword_mod (MtcsOptabs *self,machine_mode mode, rtx op0, rtx op1, bool unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsExpmed   *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);

  if (INTVAL (op1) <= 1 || (INTVAL (op1) & 1) == 0)
    return NULL_RTX;

  rtx_insn *last =mtcs_rtl_data_get_last_insn (mtcsRtlData);
  for (int bit = BITS_PER_WORD; bit >= BITS_PER_WORD / 2; bit--){
      wide_int w = wi::shifted_mask (bit, 1, false, 2 * BITS_PER_WORD);
      if (wi::ne_p (wi::umod_trunc (w, INTVAL (op1)), 1))
          continue;
      rtx sum = NULL_RTX, mask = NULL_RTX;
      if (bit == BITS_PER_WORD){
          /* For signed modulo we need to add correction to the sum
             and that might again overflow.  */
          if (!unsignedp)
            continue;
          if (mtcs_opinit_optab_handler(mtcsOpinit,uaddv4_optab, mtcsMode->word_mode) == CODE_FOR_nothing)
            continue;
          tree wtype = lang_hooks.types.type_for_mode (mtcsMode->word_mode, 1);
          if (wtype == NULL_TREE)
            continue;
          tree ctype = build_complex_type (wtype);
          if (TYPE_MODE (ctype) !=mtcs_mode_get_complex/*!GET_MODE_COMPLEX_MODE*/(mtcsMode,mtcsMode->word_mode))
            continue;
          machine_mode cmode = TYPE_MODE (ctype);
          rtx op00 = mtcs_rtl_operand_subword_force(mtcsRTL,op0, 0, mode);
          rtx op01 = mtcs_rtl_operand_subword_force(mtcsRTL,op0, 1, mode);
          rtx cres = gen_rtx_CONCAT (cmode, mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mtcsMode->word_mode),
                  mtcs_emit_gen_reg_rtx(mtcsEmit,mtcsMode->word_mode));
          tree lhs = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,ctype, cres);
          tree arg0 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,wtype, op00);
          tree arg1 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,wtype, op01);
          expand_addsub_overflow (UNKNOWN_LOCATION, PLUS_EXPR, lhs, arg0,
                      arg1, true, true, true, false, NULL);
          sum = mtcs_optabs_expand_simple_binop(self,mtcsMode->word_mode, PLUS, XEXP (cres, 0),
                         XEXP (cres, 1), NULL_RTX, 1,OPTAB_DIRECT);
          if (sum == NULL_RTX)
            return NULL_RTX;
      }else{
          /* Code below uses GEN_INT, so we need the masks to be representable
             in HOST_WIDE_INTs.  */
          if (bit >= HOST_BITS_PER_WIDE_INT)
            continue;
          /* If op0 is e.g. -1 or -2 unsigned, then the 2 additions might
             overflow.  Consider 64-bit -1ULL for word size 32, if we add
             0x7fffffffU + 0x7fffffffU + 3U, it wraps around to 1.  */
          if (bit == BITS_PER_WORD - 1)
            continue;

          int count = (2 * BITS_PER_WORD + bit - 1) / bit;
          rtx sum_corr = NULL_RTX;

          if (!unsignedp){
              /* For signed modulo, compute it as unsigned modulo of
             sum with a correction added to it if OP0 is negative,
             such that the result can be computed as unsigned
             remainder + ((OP1 >> (2 * BITS_PER_WORD - 1)) & (1 - OP1).  */
              w = wi::min_value (2 * BITS_PER_WORD, SIGNED);
              wide_int wmod1 = wi::umod_trunc (w, INTVAL (op1));
              wide_int wmod2 = wi::smod_trunc (w, INTVAL (op1));
              /* wmod2 == -wmod1.  */
              wmod2 = wmod2 + (INTVAL (op1) - 1);
              if (wi::ne_p (wmod1, wmod2)){
                  wide_int wcorr = wmod2 - wmod1;
                  if (wi::neg_p (w))
                    wcorr = wcorr + INTVAL (op1);
                  /* Now verify if the count sums can't overflow, and punt
                     if they could.  */
                  w = wi::mask (bit, false, 2 * BITS_PER_WORD);
                  w = w * (count - 1);
                  w = w + wi::mask (2 * BITS_PER_WORD - (count - 1) * bit,false, 2 * BITS_PER_WORD);
                  w = w + wcorr;
                  w = wi::lrshift (w, BITS_PER_WORD);
                  if (wi::ne_p (w, 0))
                    continue;

                  mask = mtcs_rtl_operand_subword_force(mtcsRTL,op0, WORDS_BIG_ENDIAN ? 0 : 1,mode);
                  mask = mtcs_optabs_expand_simple_binop(self,mtcsMode->word_mode, ASHIFTRT, mask,
                          mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,BITS_PER_WORD - 1),NULL_RTX, 0, OPTAB_DIRECT);
                  if (mask == NULL_RTX)
                    return NULL_RTX;
                  sum_corr = mtcs_rtl_immed_wide_int_const(mtcsRTL,wcorr, mtcsMode->word_mode);
                  sum_corr = mtcs_optabs_expand_simple_binop(self,mtcsMode->word_mode, AND, mask,sum_corr, NULL_RTX, 1,OPTAB_DIRECT);
                  if (sum_corr == NULL_RTX)
                    return NULL_RTX;
              }
          }

          for (int i = 0; i < count; i++){
              rtx v = op0;
              if (i)
                  v = mtcs_optabs_expand_simple_binop(self,mode, LSHIFTRT, v,
                          mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,i * bit),NULL_RTX, 1, OPTAB_DIRECT);
              if (v == NULL_RTX)
                  return NULL_RTX;
              v = mtcs_simplify_rtx_lowpart_subreg(mtcsSimplifyRtx,mtcsMode->word_mode, v, mode);
              if (v == NULL_RTX)
                  return NULL_RTX;
              if (i != count - 1)
                  v = mtcs_optabs_expand_simple_binop(self,mtcsMode->word_mode, AND, v,
                          mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,(HOST_WIDE_INT_1U << bit)- 1), NULL_RTX, 1,OPTAB_DIRECT);
              if (v == NULL_RTX)
                  return NULL_RTX;
              if (sum == NULL_RTX)
                  sum = v;
              else
                  sum = mtcs_optabs_expand_simple_binop(self,mtcsMode->word_mode, PLUS, sum, v, NULL_RTX,1, OPTAB_DIRECT);
              if (sum == NULL_RTX)
                  return NULL_RTX;
          }
          if (sum_corr){
              sum = mtcs_optabs_expand_simple_binop(self,mtcsMode->word_mode, PLUS, sum, sum_corr, NULL_RTX, 1, OPTAB_DIRECT);
              if (sum == NULL_RTX)
                  return NULL_RTX;
          }
      }
      rtx remainder = mtcs_expmed_expand_divmod(mtcsExpmed,1, TRUNC_MOD_EXPR, mtcsMode->word_mode, sum,
              mtcs_rtl_gen_int_mode(mtcsRTL,INTVAL (op1), mtcsMode->word_mode),NULL_RTX, 1, OPTAB_DIRECT);
      if (remainder == NULL_RTX)
          return NULL_RTX;

      if (!unsignedp){
          if (mask == NULL_RTX){
              mask = mtcs_rtl_operand_subword_force(mtcsRTL,op0, WORDS_BIG_ENDIAN ? 0 : 1,mode);
              mask = mtcs_optabs_expand_simple_binop(self,mtcsMode->word_mode, ASHIFTRT, mask,
                      mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,BITS_PER_WORD - 1),NULL_RTX, 0, OPTAB_DIRECT);
              if (mask == NULL_RTX)
                  return NULL_RTX;
          }
          mask = mtcs_optabs_expand_simple_binop(self,mtcsMode->word_mode, AND, mask,
                  mtcs_rtl_gen_int_mode(mtcsRTL,1 - INTVAL (op1),mtcsMode->word_mode),NULL_RTX, 1, OPTAB_DIRECT);
          if (mask == NULL_RTX)
            return NULL_RTX;
          remainder = mtcs_optabs_expand_simple_binop(self,mtcsMode->word_mode, PLUS, remainder,mask, NULL_RTX, 1, OPTAB_DIRECT);
          if (remainder == NULL_RTX)
            return NULL_RTX;
      }

      remainder = mtcs_expr_convert_modes(mtcsExpr,mode, mtcsMode->word_mode, remainder, unsignedp);
      /* Punt if we need any library calls.  */
      if (last)
          last = NEXT_INSN (last);
      else
          last = mtcs_rtl_data_get_insns(mtcsRtlData);
      for (; last; last = NEXT_INSN (last))
        if (CALL_P (last))
          return NULL_RTX;
      return remainder;
  }
  return NULL_RTX;
}
/* Helper for emitting a conditional move.  */
//原型 emit_conditional_move_1 optabs.cc
static rtx emit_conditional_move_1 (MtcsOptabs *self,rtx target, rtx comparison,rtx op2, rtx op3, machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit  *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsExpr  *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

  enum insn_code icode;
  if (comparison == NULL_RTX || !COMPARISON_P (comparison))
    return NULL_RTX;
  /* If the two source operands are identical, that's just a move.
     As the comparison comes in non-canonicalized, we must make
     sure not to discard any possible side effects.  If there are
     side effects, just let the target handle it.  */
  if (!side_effects_p (comparison) && rtx_equal_p (op2, op3)){
      if (!target)
          target = mtcs_emit_gen_reg_rtx(mtcsEmit,mode);
      mtcs_expr_emit_move_insn(mtcsExpr,target, op3);
      return target;
  }
  if (mode == VOIDmode)
    mode = GET_MODE (op2);
  icode = mtcs_opinit_direct_optab_handler(mtcsOpinit,movcc_optab, mode);
  if (icode == CODE_FOR_nothing)
    return NULL_RTX;
  if (!target)
    target = mtcs_emit_gen_reg_rtx(mtcsEmit,mode);

  class expand_operand ops[4];

  create_output_operand (&ops[0], target, mode);
  create_fixed_operand (&ops[1], comparison);
  create_input_operand (&ops[2], op2, mode);
  create_input_operand (&ops[3], op3, mode);
  if (mtcs_optabs_maybe_expand_insn(self,icode, 4, ops)) {
      if (ops[0].value != target)
          mtcs_expr_convert_move(mtcsExpr,target, ops[0].value, false);
      return target;
  }
  return NULL_RTX;
}

/* Promote integer arguments for a libcall if necessary.
   emit_library_call_value cannot do the promotion because it does not
   know if it should do a signed or unsigned promotion.  This is because
   there are no tree types defined for libcalls.  */
//原型 prepare_libcall_arg optabs.cc
static rtx prepare_libcall_arg (MtcsOptabs *self,rtx arg, int uintp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  scalar_int_mode mode;
  machine_mode arg_mode;
  if (mtcs_mode_is_a <scalar_int_mode>(mtcsMode,GET_MODE (arg), &mode)){
      /*  If we need to promote the integer function argument we need to do
      it here instead of inside emit_library_call_value because in
      emit_library_call_value we don't know if we should do a signed or
      unsigned promotion.  */

      int unsigned_p = 0;
      arg_mode = mtcs_mode_promote_function_mode/*!promote_function_mode*/(mtcsMode,NULL_TREE, mode,&unsigned_p, NULL_TREE, 0);
      if (arg_mode != mode)
          return mtcs_expr_convert_to_mode (mtcsExpr,arg_mode, arg, uintp);
  }
  return arg;
}

/* Return whether OP0 and OP1 should be swapped when expanding a commutative
   binop.  Order them according to commutative_operand_precedence and, if
   possible, try to put TARGET or a pseudo first.  */
static bool swap_commutative_operands_with_target (MtcsOptabs *self,rtx target, rtx op0, rtx op1)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  int op0_prec = mtcs_rtlanal_commutative_operand_precedence/*!commutative_operand_precedence*/(mtcsRtlanal,op0);
  int op1_prec = mtcs_rtlanal_commutative_operand_precedence/*!commutative_operand_precedence*/(mtcsRtlanal,op1);

  if (op0_prec < op1_prec)
    return true;

  if (op0_prec > op1_prec)
    return false;

  /* With equal precedence, both orders are ok, but it is better if the
     first operand is TARGET, or if both TARGET and OP0 are pseudos.  */
  if (target == 0 || REG_P (target))
      return (REG_P (op1) && !REG_P (op0)) || target == op1;
  else
    return rtx_equal_p (op1, target);
}

/* Return true if BINOPTAB implements a shift operation.  */

static bool shift_optab_p (optab binoptab)
{
  switch (optab_to_code (binoptab))
    {
    case ASHIFT:
    case SS_ASHIFT:
    case US_ASHIFT:
    case ASHIFTRT:
    case LSHIFTRT:
    case ROTATE:
    case ROTATERT:
      return true;

    default:
      return false;
    }
}

/* Emit code to make a call to a constant function or a library call.

   INSNS is a list containing all insns emitted in the call.
   These insns leave the result in RESULT.  Our block is to copy RESULT
   to TARGET, which is logically equivalent to EQUIV.

   We first emit any insns that set a pseudo on the assumption that these are
   loading constants into registers; doing so allows them to be safely cse'ed
   between blocks.  Then we emit all the other insns in the block, followed by
   an insn to move RESULT to TARGET.  This last insn will have a REQ_EQUAL
   note with an operand of EQUIV.  */

static void emit_libcall_block_1 (MtcsOptabs *self,rtx_insn *insns, rtx target, rtx result, rtx equiv,
              bool equiv_may_trap)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit  *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOutput  *mtcsOutput=mtcs_target_get_output(mtcsTarget);
  MtcsExpr  *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsReg  *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  rtx final_dest = target;
  rtx_insn *next, *last, *insn;

  /* If this is a reg with REG_USERVAR_P set, then it could possibly turn
     into a MEM later.  Protect the libcall block from this change.  */
  if (! REG_P (target) || REG_USERVAR_P (target))
    target = mtcs_emit_gen_reg_rtx (mtcsEmit,GET_MODE (target));

  /* If we're using non-call exceptions, a libcall corresponding to an
     operation that may trap may also trap.  */
  /* ??? See the comment in front of make_reg_eh_region_note.  */
  if (cfun->can_throw_non_call_exceptions   && (equiv_may_trap
        || mtcs_rtlanal_may_trap_p/*!may_trap_p*/(mtcsRtlanal,equiv))){
      for (insn = insns; insn; insn = NEXT_INSN (insn))
        if (CALL_P (insn)){
            rtx note = find_reg_note (insn, REG_EH_REGION, NULL_RTX);
            if (note){
                int lp_nr = INTVAL (XEXP (note, 0));
                if (lp_nr == 0 || lp_nr == INT_MIN)
                  mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,insn, note);
            }
         }
  }else{
      /* Look for any CALL_INSNs in this sequence, and attach a REG_EH_REGION
     reg note to indicate that this call cannot throw or execute a nonlocal
     goto (unless there is already a REG_EH_REGION note, in which case
     we update it).  */
      for (insn = insns; insn; insn = NEXT_INSN (insn))
        if (CALL_P (insn))
          make_reg_eh_region_note_nothrow_nononlocal (insn);
  }

  /* First emit all insns that set pseudos.  Remove them from the list as
     we go.  Avoid insns that set pseudos which were referenced in previous
     insns.  These can be generated by move_by_pieces, for example,
     to update an address.  Similarly, avoid insns that reference things
     set in previous insns.  */

  for (insn = insns; insn; insn = next){
      rtx set = single_set (insn);

      next = NEXT_INSN (insn);

      if (set != 0 && REG_P (SET_DEST (set))  && REGNO (SET_DEST (set)) >=
              mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)){
          struct no_conflict_data data;

          data.target = const0_rtx;
          data.first = insns;
          data.insn = insn;
          data.must_stay = 0;
          data.self=self;
          note_stores (insn, no_conflict_move_test, &data);
          if (! data.must_stay){
              if (PREV_INSN (insn))
                  SET_NEXT_INSN (PREV_INSN (insn)) = next;
              else
                  insns = next;

              if (next)
                  SET_PREV_INSN (next) = PREV_INSN (insn);

              mtcs_emit_add_insn(mtcsEmit,insn);
          }
      }

      /* Some ports use a loop to copy large arguments onto the stack.
     Don't move anything outside such a loop.  */
      if (LABEL_P (insn))
          break;
  }

  /* Write the remaining insns followed by the final copy.  */
  for (insn = insns; insn; insn = next){
      next = NEXT_INSN (insn);
      mtcs_emit_add_insn(mtcsEmit,insn);
  }

  last = mtcs_expr_emit_move_insn(mtcsExpr,target, result);
  if (equiv)
    mtcs_rtl_set_dst_reg_note/*!set_dst_reg_note*/(mtcsRTL,last, REG_EQUAL, copy_rtx (equiv), target);

  if (final_dest != target)
    mtcs_expr_emit_move_insn(mtcsExpr,final_dest, target);
}

/* Return true if the requirements on operands OP1 and OP2 of instruction
   ICODE are similar enough for the result of legitimizing OP1 to be
   reusable for OP2.  OPNO1 and OPNO2 are the operand numbers associated
   with OP1 and OP2 respectively.  */

static inline bool can_reuse_operands_p (MtcsOptabs *self,enum insn_code icode,unsigned int opno1, unsigned int opno2,
              const class expand_operand *op1, const class expand_operand *op2)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
  /* Check requirements that are common to all types.  */
  if (op1->type != op2->type
      || op1->mode != op2->mode
      || (mtcsOutput->insn_data[(int) icode].operand[opno1].mode
      != mtcsOutput->insn_data[(int) icode].operand[opno2].mode))
    return false;

  /* Check the requirements for specific types.  */
  switch (op1->type){
    case EXPAND_OUTPUT:
    case EXPAND_UNDEFINED_INPUT:
      /* Outputs and undefined intputs must remain distinct.  */
      return false;

    case EXPAND_FIXED:
    case EXPAND_INPUT:
    case EXPAND_ADDRESS:
    case EXPAND_INTEGER:
      return true;

    case EXPAND_CONVERT_TO:
    case EXPAND_CONVERT_FROM:
      return op1->unsigned_p == op2->unsigned_p;
  }
  gcc_unreachable ();
}


/* Try calculating (parity x) as (and (popcount x) 1), where
   popcount can also be done in a wider mode.  */
static rtx expand_parity (MtcsOptabs *self,scalar_int_mode mode, rtx op0, rtx target)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);

  enum mode_class mclass = mtcs_mode_get_class(mtcsMode,mode);
  opt_scalar_int_mode wider_mode_iter;
  MTCS_FOR_EACH_MODE_FROM (mtcsMode,wider_mode_iter, mode){
      scalar_int_mode wider_mode = wider_mode_iter.require ();
      if (mtcs_opinit_optab_handler(mtcsOpinit,popcount_optab, wider_mode) != CODE_FOR_nothing){
          rtx xop0, temp;
          rtx_insn *last;
          last = mtcs_rtl_data_get_last_insn (mtcsRtlData);
          if (target == 0 || GET_MODE (target) != wider_mode)
            target = mtcs_emit_gen_reg_rtx (mtcsEmit,wider_mode);
          xop0 = widen_operand(self,op0, wider_mode, mode, true, false);
          temp = mtcs_optabs_expand_unop(self,wider_mode, popcount_optab, xop0, NULL_RTX,true);
          if (temp != 0)
            temp = mtcs_optabs_expand_binop(self,wider_mode, and_optab, temp, const1_rtx,target, true, OPTAB_DIRECT);

          if (temp){
              if (mclass != MODE_INT || !TRULY_NOOP_TRUNCATION_MODES_P (mode, wider_mode))
                  return mtcs_expr_convert_to_mode (mtcsExpr,mode, temp, 0);
              else
                  return gen_lowpart (mode, temp);
          }else
              mtcs_rtl_data_delete_insns_since (mtcsRtlData,last);
      }
  }
  return 0;
}

/* Extract the OMODE lowpart from VAL, which has IMODE.  Under certain
   conditions, VAL may already be a SUBREG against which we cannot generate
   a further SUBREG.  In this case, we expect forcing the value into a
   register will work around the situation.  */

static rtx lowpart_subreg_maybe_copy (MtcsOptabs *self,machine_mode omode, rtx val,machine_mode imode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

  rtx ret;
  ret = mtcs_simplify_rtx_lowpart_subreg(mtcsSimplifyRtx,omode, val, imode);//rtl.h定义最终调simplify_context ().lowpart_subreg (outermode, op, innermode);
  if (ret == NULL){
      val = mtcs_explow_force_reg(mtcsExplow,imode, val);
      ret = mtcs_simplify_rtx_lowpart_subreg(mtcsSimplifyRtx,omode, val, imode);
      gcc_assert (ret != NULL);
  }
  return ret;
}

/* Expand a floating point absolute value or negation operation via a
   logical operation on the sign bit.  */

static rtx expand_absneg_bit (MtcsOptabs *self,enum rtx_code code, scalar_float_mode mode,rtx op0, rtx target)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  const struct real_format *fmt;
  int bitpos, word, nwords, i;
  scalar_int_mode imode;
  rtx temp;
  rtx_insn *insns;

  /* The format has to have a simple sign bit.  */
  fmt = mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,mode);
  if (fmt == NULL)
    return NULL_RTX;

  bitpos = fmt->signbit_rw;
  if (bitpos < 0)
    return NULL_RTX;

  /* Don't create negative zeros if the format doesn't support them.  */
  if (code == NEG && !fmt->has_signed_zero)
    return NULL_RTX;

  if (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode) <= UNITS_PER_WORD){
      if (!mtcs_mode_int_mode_for_mode (mtcsMode,mode).exists (&imode))
          return NULL_RTX;
      word = 0;
      nwords = 1;
  }else{
      imode = mtcsMode->word_mode;

      if (FLOAT_WORDS_BIG_ENDIAN)
          word = (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) - bitpos) / BITS_PER_WORD;
      else
          word = bitpos / BITS_PER_WORD;
      bitpos = bitpos % BITS_PER_WORD;
      nwords = (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) + BITS_PER_WORD - 1) / BITS_PER_WORD;
  }

  wide_int mask = wi::set_bit_in_zero (bitpos, mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,imode));
  if (code == ABS)
    mask = ~mask;

  if (target == 0 || target == op0 || mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,target, op0)
      || (nwords > 1 && !mtcs_optabs_valid_multiword_target_p(self,target)))
    target = mtcs_emit_gen_reg_rtx (mtcsEmit,mode);

  if (nwords > 1){
      mtcs_emit_start_sequence (mtcsEmit);
      for (i = 0; i < nwords; ++i){
          rtx targ_piece = mtcs_rtl_operand_subword(mtcsRTL,target, i, 1, mode);
          rtx op0_piece = mtcs_rtl_operand_subword_force(mtcsRTL,op0, i, mode);

          if (i == word){
              temp = mtcs_optabs_expand_binop(self,imode, code == ABS ? and_optab : xor_optab,
                       op0_piece,mtcs_rtl_immed_wide_int_const (mtcsRTL,mask, imode),targ_piece, 1, OPTAB_LIB_WIDEN);
              if (temp != targ_piece)
                  mtcs_expr_emit_move_insn (mtcsExpr,targ_piece, temp);
          }else
              mtcs_expr_emit_move_insn (mtcsExpr,targ_piece, op0_piece);
      }

      insns = mtcs_rtl_data_get_insns (mtcsRtlData);
      mtcs_emit_end_sequence (mtcsEmit);
      mtcs_emit_emit_insn (mtcsEmit,insns);
  }else{
      temp = mtcs_optabs_expand_binop(self,imode, code == ABS ? and_optab : xor_optab,
               gen_lowpart (imode, op0),mtcs_rtl_immed_wide_int_const (mtcsRTL,mask, imode),
                   gen_lowpart (imode, target), 1, OPTAB_LIB_WIDEN);
      target = lowpart_subreg_maybe_copy (self,mode, temp, imode);

      mtcs_rtl_set_dst_reg_note/*!set_dst_reg_note*/(mtcsRTL,mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData),
              REG_EQUAL,gen_rtx_fmt_e (code, mode, copy_rtx (op0)),target);
  }

  return target;
}


/* Widen OP to MODE and return the rtx for the widened operand.  UNSIGNEDP
   says whether OP is signed or unsigned.  NO_EXTEND is true if we need
   not actually do a sign-extend or zero-extend, but can leave the
   higher-order bits of the result rtx undefined, for example, in the case
   of logical operations, but not right shifts.  */

static rtx widen_operand (MtcsOptabs *self,rtx op, machine_mode mode, machine_mode oldmode,int unsignedp, bool no_extend)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

  rtx result;
  scalar_int_mode int_mode;

  /* If we don't have to extend and this is a constant, return it.  */
  if (no_extend && GET_MODE (op) == VOIDmode)
    return op;

  /* If we must extend do so.  If OP is a SUBREG for a promoted object, also
     extend since it will be more efficient to do so unless the signedness of
     a promoted object differs from our extension.  */
  if (! no_extend
      || !mtcs_mode_is_a <scalar_int_mode>(mtcsMode,mode, &int_mode)
      || (GET_CODE (op) == SUBREG && SUBREG_PROMOTED_VAR_P (op)
      && SUBREG_CHECK_PROMOTED_SIGN (op, unsignedp)))
    return mtcs_expr_convert_modes (mtcsExpr,mode, oldmode, op, unsignedp);

  /* If MODE is no wider than a single word, we return a lowpart or paradoxical
     SUBREG.  */
  if (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,int_mode) <= UNITS_PER_WORD)
    return gen_lowpart (int_mode, mtcs_explow_force_reg(mtcsExplow,GET_MODE (op), op));

  /* Otherwise, get an object of MODE, clobber it, and set the low-order
     part to OP.  */

  result = mtcs_emit_gen_reg_rtx (mtcsEmit,int_mode);
  mtcs_emit_emit_clobber (mtcsEmit,result);
  mtcs_expr_emit_move_insn (mtcsExpr,gen_lowpart (GET_MODE (op), result), op);
  return result;
}

/* Add a REG_EQUAL note to the last insn in INSNS.  TARGET is being set to
   the result of operation CODE applied to OP0 (and OP1 if it is a binary
   operation).  OP0_MODE is OP0's mode.

   If the last insn does not set TARGET, don't do anything, but return true.

   If the last insn or a previous insn sets TARGET and TARGET is one of OP0
   or OP1, don't add the REG_EQUAL note but return false.  Our caller can then
   try again, ensuring that TARGET is not one of the operands.  */

static bool add_equal_note (MtcsOptabs *self,rtx_insn *insns, rtx target, enum rtx_code code, rtx op0,
                                    rtx op1, machine_mode op0_mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  rtx_insn *last_insn;
  rtx set;
  rtx note;

  gcc_assert (insns && INSN_P (insns) && NEXT_INSN (insns));

  if (GET_RTX_CLASS (code) != RTX_COMM_ARITH
      && GET_RTX_CLASS (code) != RTX_BIN_ARITH
      && GET_RTX_CLASS (code) != RTX_COMM_COMPARE
      && GET_RTX_CLASS (code) != RTX_COMPARE
      && GET_RTX_CLASS (code) != RTX_UNARY)
    return true;

  if (GET_CODE (target) == ZERO_EXTRACT)
    return true;

  for (last_insn = insns;  NEXT_INSN (last_insn) != NULL_RTX;last_insn = NEXT_INSN (last_insn))
    ;

  /* If TARGET is in OP0 or OP1, punt.  We'd end up with a note referencing
     a value changing in the insn, so the note would be invalid for CSE.  */
  if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,target, op0) || (op1 && mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,target, op1))){
      if (MEM_P (target)  && (rtx_equal_p (target, op0) || (op1 && rtx_equal_p (target, op1)))){
          /* For MEM target, with MEM = MEM op X, prefer no REG_EQUAL note
             over expanding it as temp = MEM op X, MEM = temp.  If the target
             supports MEM = MEM op X instructions, it is sometimes too hard
             to reconstruct that form later, especially if X is also a memory,
             and due to multiple occurrences of addresses the address might
             be forced into register unnecessarily.
             Note that not emitting the REG_EQUIV note might inhibit
             CSE in some cases.  */
          set = single_set (last_insn);
          if (set
              && GET_CODE (SET_SRC (set)) == code
              && MEM_P (SET_DEST (set))
              && (rtx_equal_p (SET_DEST (set), XEXP (SET_SRC (set), 0))
              || (op1 && rtx_equal_p (SET_DEST (set),
                          XEXP (SET_SRC (set), 1)))))
            return true;
      }
      return false;
  }

  set = set_for_reg_notes (last_insn);
  if (set == NULL_RTX)
    return true;

  if (! rtx_equal_p (SET_DEST (set), target)
      /* For a STRICT_LOW_PART, the REG_NOTE applies to what is inside it.  */
      && (GET_CODE (SET_DEST (set)) != STRICT_LOW_PART  || ! rtx_equal_p (XEXP (SET_DEST (set), 0), target)))
    return true;

  if (GET_RTX_CLASS (code) == RTX_UNARY)
    switch (code){
      case FFS:
      case CLZ:
      case CTZ:
      case CLRSB:
      case POPCOUNT:
      case PARITY:
      case BSWAP:
        if (op0_mode != VOIDmode && GET_MODE (target) != op0_mode){
            note = gen_rtx_fmt_e (code, op0_mode, copy_rtx (op0));
            if (mtcs_mode_get_unit_size/*!GET_MODE_UNIT_SIZE*/(mtcsMode,op0_mode) >
               mtcs_mode_get_unit_size/*!GET_MODE_UNIT_SIZE*/(mtcsMode,GET_MODE (target)))
              note = simplify_gen_unary (TRUNCATE, GET_MODE (target),note, op0_mode);
            else
              note = simplify_gen_unary (ZERO_EXTEND, GET_MODE (target),note, op0_mode);
            break;
        }
        /* FALLTHRU */
      default:
          note = gen_rtx_fmt_e (code, GET_MODE (target), copy_rtx (op0));
          break;
    }
  else
    note = gen_rtx_fmt_ee (code, GET_MODE (target), copy_rtx (op0), copy_rtx (op1));

  mtcs_rtl_set_unique_reg_note/*!set_unique_reg_note*/(mtcsRTL,last_insn, REG_EQUAL, note);

  return true;
}

/* Subroutine of emit_cmp_and_jump_insns; this function is called when we know
   we can do the branch.  */

static void emit_cmp_and_jump_insn_1 (MtcsOptabs *self,rtx test, machine_mode mode, rtx label,
              direct_optab cmp_optab, profile_probability prob,bool test_branch)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);

  machine_mode optab_mode;
  enum mode_class mclass;
  enum insn_code icode;
  rtx_insn *insn;

  mclass = mtcs_mode_get_class(mtcsMode,mode);
  optab_mode = (mclass == MODE_CC) ? CCmode : mode;
  icode = mtcs_opinit_optab_handler (mtcsOpinit,cmp_optab, optab_mode);

  gcc_assert (icode != CODE_FOR_nothing);
  gcc_assert (test_branch || mtcs_optabs_insn_operand_matches (self,icode, 0, test));
  if (test_branch)
    insn = mtcs_emit_emit_jump_insn(mtcsEmit,MTCS_GEN_FCN (icode) (XEXP (test, 0),XEXP (test, 1), label));
  else
    insn = mtcs_emit_emit_jump_insn(mtcsEmit,MTCS_GEN_FCN (icode) (test, XEXP (test, 0),XEXP (test, 1), label));

  if (prob.initialized_p ()
      && profile_status_for_fn (cfun) != PROFILE_ABSENT
      && insn
      && JUMP_P (insn)
      && any_condjump_p (insn)
      && !find_reg_note (insn, REG_BR_PROB, 0))
    add_reg_br_prob_note (insn, prob);
}

/* PTEST points to a comparison that compares its first operand with zero.
   Check to see if it can be performed as a bit-test-and-branch instead.
   On success, return the instruction that performs the bit-test-and-branch
   and replace the second operand of *PTEST with the bit number to test.
   On failure, return CODE_FOR_nothing and leave *PTEST unchanged.

   Note that the comparison described by *PTEST should not be taken
   literally after a successful return.  *PTEST is just a convenient
   place to store the two operands of the bit-and-test.

   VAL must contain the original tree expression for the first operand
   of *PTEST.  */

static enum insn_code validate_test_and_branch (MtcsOptabs *self,tree val, rtx *ptest, machine_mode *pmode, optab *res)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit   *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  if (!val || TREE_CODE (val) != SSA_NAME)
    return CODE_FOR_nothing;
  machine_mode mode = TYPE_MODE (TREE_TYPE (val));
  rtx test = *ptest;
  direct_optab optab;
  if (GET_CODE (test) == EQ)
    optab = tbranch_eq_optab;
  else if (GET_CODE (test) == NE)
    optab = tbranch_ne_optab;
  else
    return CODE_FOR_nothing;

  *res = optab;
  /* If the target supports the testbit comparison directly, great.  */
  auto icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/ (mtcsOpinit,optab, mode);
  if (icode == CODE_FOR_nothing)
    return icode;

  if (tree_zero_one_valued_p (val)){
      auto pos = BITS_BIG_ENDIAN ? mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) - 1 : 0;
      XEXP (test, 1) = mtcs_rtl_gen_int_mode (mtcsRTL,pos, mode);
      *ptest = test;
      *pmode = mode;
      return icode;
  }
  wide_int wcst = get_nonzero_bits (val);
  if (wcst == -1)
    return CODE_FOR_nothing;

  int bitpos;
  if ((bitpos = wi::exact_log2 (wcst)) == -1)
    return CODE_FOR_nothing;

  auto pos = BITS_BIG_ENDIAN ? mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) - 1 - bitpos : bitpos;
  XEXP (test, 1) = mtcs_rtl_gen_int_mode (mtcsRTL,pos, mode);
  *ptest = test;
  *pmode = mode;
  return icode;
}

/* Emit a library call comparison between floating point X and Y.
   COMPARISON is the rtl operator to compare with (EQ, NE, GT, etc.).  */

static void prepare_float_lib_cmp (MtcsOptabs *self,rtx x, rtx y, enum rtx_code comparison,rtx *ptest, machine_mode *pmode)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsOutput   *mtcsOutput=mtcs_target_get_output(mtcsTarget);
    MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
    MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
    MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
    MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
    MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
    MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
    MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
    MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
    MtcsExpmed *mtcsExpmed =mtcs_target_get_expmed(mtcsTarget);
    MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
    MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
    MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

  enum rtx_code swapped = swap_condition (comparison);
  enum rtx_code reversed = reverse_condition_maybe_unordered (comparison);
  machine_mode orig_mode = GET_MODE (x);
  machine_mode mode;
  rtx true_rtx, false_rtx;
  rtx value, target, equiv;
  rtx_insn *insns;
  rtx libfunc = 0;
  bool reversed_p = false;
  scalar_int_mode cmp_mode = mtcsTarget->/*!targetm.libgcc_cmp_return_mode*/libgcc_cmp_return_mode (mtcsTarget);

  MTCS_FOR_EACH_WIDER_MODE_FROM (mtcsMode,mode, orig_mode){
      if (code_to_optab (comparison) && (libfunc =
            mtcs_libfuncs_optab_libfunc/*!optab_libfunc*/(mtcsLibfuncs,code_to_optab (comparison), mode)))
          break;
      if (code_to_optab (swapped) && (libfunc =
            mtcs_libfuncs_optab_libfunc/*!optab_libfunc*/(mtcsLibfuncs,code_to_optab (swapped), mode))){
          std::swap (x, y);
          comparison = swapped;
          break;
      }
      if (code_to_optab (reversed) && (libfunc =
            mtcs_libfuncs_optab_libfunc/*!optab_libfunc*/(mtcsLibfuncs,code_to_optab (reversed), mode))){
          comparison = reversed;
          reversed_p = true;
          break;
      }
  }

  gcc_assert (mode != VOIDmode);
  if (mode != orig_mode){
      x = mtcs_expr_convert_to_mode (mtcsExpr,mode, x, 0);
      y = mtcs_expr_convert_to_mode (mtcsExpr,mode, y, 0);
  }
  /* Attach a REG_EQUAL note describing the semantics of the libcall to
     the RTL.  The allows the RTL optimizers to delete the libcall if the
     condition can be determined at compile-time.  */
  if (comparison == UNORDERED
      || mtcs_mode_float_lib_compare_return_bool/*!FLOAT_LIB_COMPARE_RETURNS_BOOL*/ (mtcsMode,mode, comparison)){
      true_rtx = const_true_rtx;
      false_rtx = const0_rtx;
  }else{
      switch (comparison){
        case EQ:
          true_rtx = const0_rtx;
          false_rtx = const_true_rtx;
          break;

        case NE:
          true_rtx = const_true_rtx;
          false_rtx = const0_rtx;
          break;

        case GT:
          true_rtx = const1_rtx;
          false_rtx = const0_rtx;
          break;

        case GE:
          true_rtx = const0_rtx;
          false_rtx = constm1_rtx;
          break;

        case LT:
          true_rtx = constm1_rtx;
          false_rtx = const0_rtx;
          break;

        case LE:
          true_rtx = const0_rtx;
          false_rtx = const1_rtx;
          break;

        default:
          gcc_unreachable ();
      }
  }

  if (comparison == UNORDERED){
      rtx temp = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,NE, cmp_mode, mode, x, x);
      equiv = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,NE, cmp_mode, mode, y, y);
      equiv = mtcs_simplify_rtx_gen_ternary/*!simplify_gen_ternary*/(mtcsSimplifyRtx,IF_THEN_ELSE, cmp_mode, cmp_mode,temp, const_true_rtx, equiv);
  }else{
      equiv = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,comparison, cmp_mode, mode, x, y);
      if (! mtcs_mode_float_lib_compare_return_bool/*!FLOAT_LIB_COMPARE_RETURNS_BOOL*/ (mtcsMode,mode, comparison))
        equiv = mtcs_simplify_rtx_gen_ternary/*!simplify_gen_ternary*/(mtcsSimplifyRtx,IF_THEN_ELSE, cmp_mode, cmp_mode,equiv, true_rtx, false_rtx);
  }

  mtcs_emit_start_sequence (mtcsEmit);
  value = mtcs_calls_emit_library_call_value/*!emit_library_call_value*/(mtcsCalls,
        libfunc, NULL_RTX, LCT_CONST,cmp_mode, x, mode, y, mode);
  insns = mtcs_rtl_data_get_insns (mtcsRtlData);
  mtcs_emit_end_sequence (mtcsEmit);

  target = mtcs_emit_gen_reg_rtx (mtcsEmit,cmp_mode);
  mtcs_optabs_emit_libcall_block (self,insns, target, value, equiv);

  if (comparison == UNORDERED
      || mtcs_mode_float_lib_compare_return_bool/*!FLOAT_LIB_COMPARE_RETURNS_BOOL*/ (mtcsMode,mode, comparison)
      || reversed_p)
    *ptest = gen_rtx_fmt_ee (reversed_p ? EQ : NE, VOIDmode, target, false_rtx);
  else
    *ptest = gen_rtx_fmt_ee (comparison, VOIDmode, target, const0_rtx);

  *pmode = cmp_mode;
}

/* This function is called when we are going to emit a compare instruction that
   compares the values found in X and Y, using the rtl operator COMPARISON.

   If they have mode BLKmode, then SIZE specifies the size of both operands.

   UNSIGNEDP nonzero says that the operands are unsigned;
   this matters if they need to be widened (as given by METHODS).

   *PTEST is where the resulting comparison RTX is returned or NULL_RTX
   if we failed to produce one.

   *PMODE is the mode of the inputs (in case they are const_int).

   This function performs all the setup necessary so that the caller only has
   to emit a single comparison insn.  This setup can involve doing a BLKmode
   comparison or emitting a library call to perform the comparison if no insn
   is available to handle it.
   The values which are passed in through pointers can be modified; the caller
   should perform the comparison on the modified values.  Constant
   comparisons must have already been folded.  */

static void prepare_cmp_insn (MtcsOptabs *self,rtx x, rtx y, enum rtx_code comparison, rtx size,
          int unsignedp, enum optab_methods methods,rtx *ptest, machine_mode *pmode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOutput   *mtcsOutput=mtcs_target_get_output(mtcsTarget);
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
  MtcsExpmed *mtcsExpmed =mtcs_target_get_expmed(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
  MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);

  machine_mode mode = *pmode;
  rtx libfunc, test;
  machine_mode cmp_mode;
  /* The other methods are not needed.  */
  gcc_assert (methods == OPTAB_DIRECT || methods == OPTAB_WIDEN|| methods == OPTAB_LIB_WIDEN);
  if (CONST_SCALAR_INT_P (y))
      mtcs_expmed_canonicalize_comparison (mtcsExpmed,mode, &comparison, &y);
  /* If we are optimizing, force expensive constants into a register.  */
  if (CONSTANT_P (x) && optimize
      && (mtcs_rtlanal_rtx_cost/*!rtx_cost*/(mtcsRtlanal,x, mode, COMPARE,
              0, optimize_insn_for_speed_p ()) > COSTS_N_INSNS (1))  && can_create_pseudo_p ())
    x = mtcs_explow_force_reg(mtcsExplow,mode, x);

  if (CONSTANT_P (y) && optimize
      && (mtcs_rtlanal_rtx_cost/*!rtx_cost*/(mtcsRtlanal,y, mode, COMPARE, 1,
              optimize_insn_for_speed_p ()) > COSTS_N_INSNS (1)) && can_create_pseudo_p ())
    y = mtcs_explow_force_reg(mtcsExplow,mode, y);
  /* Don't let both operands fail to indicate the mode.  */
  if (GET_MODE (x) == VOIDmode && GET_MODE (y) == VOIDmode)
    x = mtcs_explow_force_reg(mtcsExplow,mode, x);
  if (mode == VOIDmode)
    mode = GET_MODE (x) != VOIDmode ? GET_MODE (x) : GET_MODE (y);
  /* Handle all BLKmode compares.  */
  if (mode == mtcsMode->modes.M_BLKmode){
      machine_mode result_mode;
      enum insn_code cmp_code;
      rtx result;
      rtx opalign = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,MIN (mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,x),
              mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,y)) / BITS_PER_UNIT);
      gcc_assert (size);
      /* Try to use a memory block compare insn - either cmpstr
     or cmpmem will do.  */
      opt_scalar_int_mode cmp_mode_iter;
      MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,cmp_mode_iter, MODE_INT){
          scalar_int_mode cmp_mode = cmp_mode_iter.require ();
          cmp_code = mtcs_opinit_direct_optab_handler(mtcsOpinit,cmpmem_optab, cmp_mode);
          if (cmp_code == CODE_FOR_nothing)
            cmp_code = mtcs_opinit_direct_optab_handler(mtcsOpinit,cmpstr_optab, cmp_mode);
          if (cmp_code == CODE_FOR_nothing)
            cmp_code = mtcs_opinit_direct_optab_handler(mtcsOpinit,cmpstrn_optab, cmp_mode);
          if (cmp_code == CODE_FOR_nothing)
            continue;
              /* Must make sure the size fits the insn's mode.  */
          if (CONST_INT_P (size)? UINTVAL (size) > mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,cmp_mode)
              : (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,
                      mtcs_mode_as_a <scalar_int_mode> (mtcsMode,GET_MODE (size)))>
                       mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,cmp_mode)))
            continue;

          result_mode = mtcsOutput->insn_data[cmp_code].operand[0].mode;
          result = mtcs_emit_gen_reg_rtx (mtcsEmit,result_mode);
          size = mtcs_expr_convert_to_mode (mtcsExpr,cmp_mode, size, 1);
          mtcs_emit_emit_insn (mtcsEmit,MTCS_GEN_FCN (cmp_code) (result, x, y, size, opalign));
         *ptest = gen_rtx_fmt_ee (comparison, VOIDmode, result, const0_rtx);
         *pmode = result_mode;
          return;
      }
      if (methods != OPTAB_LIB && methods != OPTAB_LIB_WIDEN)
          goto fail;
      /* Otherwise call a library function.  */
      result = emit_block_comp_via_libcall (x, y, size);
      x = result;
      y = const0_rtx;
      mode = TYPE_MODE (integer_type_node);
      methods = OPTAB_LIB_WIDEN;
      unsignedp = false;
  }
  /* Don't allow operands to the compare to trap, as that can put the
     compare and branch in different basic blocks.  */
  if (cfun->can_throw_non_call_exceptions){
      if (!can_create_pseudo_p () && (mtcs_rtlanal_may_trap_p/*!may_trap_p*/(mtcsRtlanal,x)
            || mtcs_rtlanal_may_trap_p/*!may_trap_p*/(mtcsRtlanal,y)))
          goto fail;
      if (mtcs_rtlanal_may_trap_p/*!may_trap_p*/(mtcsRtlanal,x))
          x = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,x);
      if (mtcs_rtlanal_may_trap_p/*!may_trap_p*/(mtcsRtlanal,y))
          y = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,y);
  }

  if (mtcs_mode_get_class(mtcsMode,mode) == MODE_CC){
      enum insn_code icode = mtcs_opinit_optab_handler (mtcsOpinit,cbranch_optab, CCmode);
      test = gen_rtx_fmt_ee (comparison, VOIDmode, x, y);
      if (icode != CODE_FOR_nothing && mtcs_optabs_insn_operand_matches (self,icode, 0, test)){
         *ptest = test;
         return;
      } else
        goto fail;
  }

  test = gen_rtx_fmt_ee (comparison, VOIDmode, x, y);
  MTCS_FOR_EACH_WIDER_MODE_FROM (mtcsMode,cmp_mode, mode){
      enum insn_code icode;
      icode = mtcs_opinit_optab_handler (mtcsOpinit,cbranch_optab, cmp_mode);
      if (icode != CODE_FOR_nothing && mtcs_optabs_insn_operand_matches (self,icode, 0, test)){
          rtx_insn *last = mtcs_rtl_data_get_last_insn (mtcsRtlData);
          rtx op0 = mtcs_optabs_prepare_operand (self,icode, x, 1, mode, cmp_mode, unsignedp);
          rtx op1 = mtcs_optabs_prepare_operand (self,icode, y, 2, mode, cmp_mode, unsignedp);
          if (op0 && op1  && mtcs_optabs_insn_operand_matches (self,icode, 1, op0)
              && mtcs_optabs_insn_operand_matches (self,icode, 2, op1)){
              XEXP (test, 0) = op0;
              XEXP (test, 1) = op1;
              *ptest = test;
              *pmode = cmp_mode;
              return;
          }
          mtcs_rtl_data_delete_insns_since (mtcsRtlData,last);
      }
      if (methods == OPTAB_DIRECT)
         break;
  }

  if (methods != OPTAB_LIB_WIDEN)
    goto fail;

  if (mtcs_mode_is_scalar_float_p/*!SCALAR_FLOAT_MODE_P*/(mtcsMode,mode)){
      /* Small trick if UNORDERED isn't implemented by the hardware.  */
      if (comparison == UNORDERED && rtx_equal_p (x, y)){
          prepare_cmp_insn (self,x, y, UNLT, NULL_RTX, unsignedp, OPTAB_WIDEN,ptest, pmode);
          if (*ptest)
            return;
      }
      prepare_float_lib_cmp(self,x, y, comparison, ptest, pmode);
  }else{
      rtx result;
      machine_mode ret_mode;

      /* Handle a libcall just for the mode we are using.  */
      libfunc = mtcs_libfuncs_optab_libfunc/*!optab_libfunc*/(mtcsLibfuncs,cmp_optab, mode);
      gcc_assert (libfunc);

      /* If we want unsigned, and this mode has a distinct unsigned
     comparison routine, use that.  */
      if (unsignedp){
          rtx ulibfunc = mtcs_libfuncs_optab_libfunc/*!optab_libfunc*/(mtcsLibfuncs,ucmp_optab, mode);
          if (ulibfunc)
            libfunc = ulibfunc;
      }

      ret_mode = mtcsTarget->/*!targetm.*/libgcc_cmp_return_mode(mtcsTarget);
      result = mtcs_calls_emit_library_call_value/*!emit_library_call_value*/(mtcsCalls,
            libfunc, NULL_RTX, LCT_CONST,ret_mode, x, mode, y, mode);

      /* There are two kinds of comparison routines. Biased routines
     return 0/1/2, and unbiased routines return -1/0/1. Other parts
     of gcc expect that the comparison operation is equivalent
     to the modified comparison. For signed comparisons compare the
     result against 1 in the biased case, and zero in the unbiased
     case. For unsigned comparisons always compare against 1 after
     biasing the unbiased result by adding 1. This gives us a way to
     represent LTU.
     The comparisons in the fixed-point helper library are always
     biased.  */
      x = result;
      y = const1_rtx;

      if (!TARGET_LIB_INT_CMP_BIASED && !ALL_FIXED_POINT_MODE_P (mode)){
          if (unsignedp)
            x = mtcs_rtl_plus_constant(mtcsRTL,ret_mode, result, 1);
          else
            y = const0_rtx;
      }

      *pmode = ret_mode;
      prepare_cmp_insn (self,x, y, comparison, NULL_RTX, unsignedp, methods,ptest, pmode);
  }

  return;

 fail:
  *ptest = NULL_RTX;
}

/* Make OP describe an input operand that has value INTVAL and that has
   no inherent mode.  This function should only be used for operands that
   are always expand-time constants.  The backend may request that INTVAL
   be copied into a different kind of rtx, but it must specify the mode
   of that rtx if so.  */
//原型 create_integer_operand optabs.h optabs.cc
void mtcs_optabs_create_integer_operand (MtcsOptabs *self,class expand_operand *op, poly_int64 intval)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   create_expand_operand (op, EXPAND_INTEGER,
         mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,
               intval, mtcsMode->modesMinMax.max_INT/*!MAX_MODE_INT*/), VOIDmode, false, intval);
}

/* Like maybe_legitimize_operand, but do not change the code of the
   current rtx value.  */
static bool maybe_legitimize_operand_same_code (MtcsOptabs *self,enum insn_code icode, unsigned int opno,
                    class expand_operand *op)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOutput   *mtcsOutput=mtcs_target_get_output(mtcsTarget);
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);

  /* See if the operand matches in its current form.  */
  if (mtcs_optabs_insn_operand_matches (self,icode, opno, op->value))
    return true;
  /* If the operand is a memory whose address has no side effects,
     try forcing the address into a non-virtual pseudo register.
     The check for side effects is important because copy_to_mode_reg
     cannot handle things like auto-modified addresses.  */
  if (mtcsOutput->insn_data[(int) icode].operand[opno].allows_mem && MEM_P (op->value)){
      rtx addr, mem;
      mem = op->value;
      addr = XEXP (mem, 0);
      if (!(REG_P (addr) && REGNO (addr) >mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg))  && !side_effects_p (addr)){
          rtx_insn *last;
          machine_mode mode;

          last = mtcs_rtl_data_get_last_insn (mtcsRtlData);
          mode = mtcs_rtl_get_address_mode/*!get_address_mode*/(mtcsRTL,mem);
          mem = replace_equiv_address (mem, copy_to_mode_reg (mode, addr));
          if (mtcs_optabs_insn_operand_matches (self,icode, opno, mem)) {
              op->value = mem;
              return true;
          }
          mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
      }
  }
  return false;
}

/* Try to make OP match operand OPNO of instruction ICODE.  Return true
   on success, storing the new operand value back in OP.  */
static bool maybe_legitimize_operand (MtcsOptabs *self,enum insn_code icode, unsigned int opno,class expand_operand *op)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOutput   *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   machine_mode mode, imode, tmode;
   mode = op->mode;
   n_debug("mtcsoptabs.c maybe_legitimize_operand 00 op->type:%d mode:%d\n",op->type,mode);

   switch (op->type){
      case EXPAND_FIXED:
      {
         n_debug("mtcsoptabs.c maybe_legitimize_operand 11 op->type=EXPAND_FIXED\n");
         mtcs_temporary_volatile_ok v (mtcsRecog,true);
         return maybe_legitimize_operand_same_code (self,icode, opno, op);
      }

      case EXPAND_OUTPUT:
         gcc_assert (mode != VOIDmode);
         if (op->value
         && op->value != const0_rtx
         && GET_MODE (op->value) == mode
         && maybe_legitimize_operand_same_code (self,icode, opno, op))
            return true;

         op->value = mtcs_emit_gen_reg_rtx(mtcsEmit,mode);
         op->target = 0;
         break;

      case EXPAND_INPUT:
         input:
         gcc_assert (mode != VOIDmode);
         gcc_assert (GET_MODE (op->value) == VOIDmode || GET_MODE (op->value) == mode);
         if (maybe_legitimize_operand_same_code (self,icode, opno, op))
            return true;

         op->value = mtcs_explow_copy_to_mode_reg (mtcsExplow,mode, op->value);
         break;

      case EXPAND_CONVERT_TO:
         gcc_assert (mode != VOIDmode);
         op->value = mtcs_expr_convert_to_mode(mtcsExpr,mode, op->value, op->unsigned_p);
         goto input;

      case EXPAND_CONVERT_FROM:
         if (GET_MODE (op->value) != VOIDmode)
            mode = GET_MODE (op->value);
         else
            /* The caller must tell us what mode this value has.  */
            gcc_assert (mode != VOIDmode);

         imode = mtcsOutput->insn_data[(int) icode].operand[opno].mode;
         tmode = (mtcs_mode_is_vector_p(mtcsMode,imode) && !mtcs_mode_is_vector_p(mtcsMode,mode)
               ? mtcs_mode_get_inner (mtcsMode,imode) : imode);
         n_debug("mtcsoptabs.c maybe_legitimize_operand 11 op->type:%d mode:%d imode:%d tmode:%d icode:%d opno:%d\n",
               op->type,mode,imode,tmode,icode,opno);

         if (tmode != VOIDmode && tmode != mode){
            op->value = mtcs_expr_convert_modes(mtcsExpr,tmode, mode, op->value, op->unsigned_p);
            mode = tmode;
         }
         if (imode != VOIDmode && imode != mode){
            gcc_assert (mtcs_mode_is_vector_p(mtcsMode,imode) && !mtcs_mode_is_vector_p(mtcsMode,mode));
            op->value = mtcs_optabs_expand_vector_broadcast (self,imode, op->value);
            mode = imode;
         }
         goto input;

      case EXPAND_ADDRESS:
         //rtl.h #define convert_memory_address(to_mode,x)  convert_memory_address_addr_space ((to_mode), (x), ADDR_SPACE_GENERIC)
         op->value = mtcs_explow_convert_memory_address_addr_space/*!convert_memory_address*/
               (mtcsExplow,mtcs_mode_as_a <scalar_int_mode> (mtcsMode,mode),op->value,ADDR_SPACE_GENERIC);
         goto input;

      case EXPAND_INTEGER:
         mode = mtcsOutput->insn_data[(int) icode].operand[opno].mode;
         if (mode != VOIDmode  && known_eq (mtcs_mode_trunc_int_for_mode/*!trunc_int_for_mode*/(mtcsMode,
               op->int_value, mode),op->int_value)){
            op->value = mtcs_rtl_gen_int_mode (mtcsRTL,op->int_value, mode);
            goto input;
         }
         break;

      case EXPAND_UNDEFINED_INPUT:
         /* See if the predicate accepts a SCRATCH rtx, which in this context
         indicates an undefined value.  Use an uninitialized register if not. */
         if (!mtcs_optabs_insn_operand_matches (self,icode, opno, op->value)){
            op->value = mtcs_emit_gen_reg_rtx(mtcsEmit,op->mode);
            goto input;
         }
         return true;
   }
   return mtcs_optabs_insn_operand_matches (self,icode, opno, op->value);
}

/* X is to be used in mode MODE as operand OPN to BINOPTAB.  If we're
   optimizing, and if the operand is a constant that costs more than
   1 instruction, force the constant into a register and return that
   register.  Return X otherwise.  UNSIGNEDP says whether X is unsigned.  */

static rtx avoid_expensive_constant (MtcsOptabs *self,machine_mode mode, optab binoptab, int opn, rtx x, bool unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  bool speed = optimize_insn_for_speed_p ();
  if (mode != VOIDmode && optimize && CONSTANT_P (x)
     && (mtcs_rtlanal_rtx_cost/*!rtx_cost*/(mtcsRtlanal,x, mode, optab_to_code (binoptab),
             opn, speed) > mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,x, mode, speed))){
      if (CONST_INT_P (x)){
          HOST_WIDE_INT intval = mtcs_mode_trunc_int_for_mode/*!trunc_int_for_mode*/(mtcsMode,INTVAL (x), mode);
          if (intval != INTVAL (x))
            x = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,intval);
      }else
          x = mtcs_expr_convert_modes(mtcsExpr,mode, VOIDmode, x, unsignedp);
      x =mtcs_explow_force_reg/*!force_reg*/ (mtcsExplow,mode, x);
  }
  return x;
}

/* Return true if BINOPTAB implements a commutative binary operation.  */

static bool commutative_optab_p (optab binoptab)
{
  return (GET_RTX_CLASS (optab_to_code (binoptab)) == RTX_COMM_ARITH
      || binoptab == smul_widen_optab
      || binoptab == umul_widen_optab
      || binoptab == smul_highpart_optab
      || binoptab == umul_highpart_optab
      || binoptab == vec_widen_sadd_optab
      || binoptab == vec_widen_uadd_optab
      || binoptab == vec_widen_sadd_hi_optab
      || binoptab == vec_widen_sadd_lo_optab
      || binoptab == vec_widen_uadd_hi_optab
      || binoptab == vec_widen_uadd_lo_optab
      || binoptab == vec_widen_sadd_even_optab
      || binoptab == vec_widen_sadd_odd_optab
      || binoptab == vec_widen_uadd_even_optab
      || binoptab == vec_widen_uadd_odd_optab);
}

/* Helper function for expand_binop: handle the case where there
   is an insn ICODE that directly implements the indicated operation.
   Returns null if this is not possible.  */
static rtx expand_binop_directly (MtcsOptabs *self,enum insn_code icode, machine_mode mode, optab binoptab,
               rtx op0, rtx op1, rtx target, int unsignedp, enum optab_methods methods,rtx_insn *last)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOutput   *mtcsOutput=mtcs_target_get_output(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

  machine_mode xmode0 = mtcsOutput->insn_data[(int) icode].operand[1].mode;
  machine_mode xmode1 = mtcsOutput->insn_data[(int) icode].operand[2].mode;
  machine_mode mode0, mode1, tmp_mode;
  class expand_operand ops[3];
  bool commutative_p;
  rtx_insn *pat;
  rtx xop0 = op0, xop1 = op1;
  bool canonicalize_op1 = false;

  /* If it is a commutative operator and the modes would match
     if we would swap the operands, we can save the conversions.  */
  commutative_p = commutative_optab_p (binoptab);
  if (commutative_p  && GET_MODE (xop0) != xmode0 && GET_MODE (xop1) != xmode1   && GET_MODE (xop0) == xmode1 && GET_MODE (xop1) == xmode0)
    std::swap (xop0, xop1);

  /* If we are optimizing, force expensive constants into a register.  */
  xop0 = avoid_expensive_constant (self,xmode0, binoptab, 0, xop0, unsignedp);
  if (!shift_optab_p (binoptab))
    xop1 = avoid_expensive_constant (self,xmode1, binoptab, 1, xop1, unsignedp);
  else
    /* Shifts and rotates often use a different mode for op1 from op0;
       for VOIDmode constants we don't know the mode, so force it
       to be canonicalized using convert_modes.  */
    canonicalize_op1 = true;

  /* In case the insn wants input operands in modes different from
     those of the actual operands, convert the operands.  It would
     seem that we don't need to convert CONST_INTs, but we do, so
     that they're properly zero-extended, sign-extended or truncated
     for their mode.  */

  mode0 = GET_MODE (xop0) != VOIDmode ? GET_MODE (xop0) : mode;
  if (xmode0 != VOIDmode && xmode0 != mode0){
      xop0 = mtcs_expr_convert_modes(mtcsExpr,xmode0, mode0, xop0, unsignedp);
      mode0 = xmode0;
  }

  mode1 = ((GET_MODE (xop1) != VOIDmode || canonicalize_op1) ? GET_MODE (xop1) : mode);
  if (xmode1 != VOIDmode && xmode1 != mode1){
      xop1 = mtcs_expr_convert_modes(mtcsExpr,xmode1, mode1, xop1, unsignedp);
      mode1 = xmode1;
  }

  /* If operation is commutative,
     try to make the first operand a register.
     Even better, try to make it the same as the target.
     Also try to make the last operand a constant.  */
  if (commutative_p   && swap_commutative_operands_with_target (self,target, xop0, xop1))
    std::swap (xop0, xop1);

  /* Now, if insn's predicates don't allow our operands, put them into
     pseudo regs.  */

  if (binoptab == vec_pack_trunc_optab
      || binoptab == vec_pack_usat_optab
      || binoptab == vec_pack_ssat_optab
      || binoptab == vec_pack_ufix_trunc_optab
      || binoptab == vec_pack_sfix_trunc_optab
      || binoptab == vec_packu_float_optab
      || binoptab == vec_packs_float_optab){
      /* The mode of the result is different then the mode of the
     arguments.  */
      tmp_mode = mtcsOutput->insn_data[(int) icode].operand[0].mode;
      if (mtcs_mode_is_vector_p(mtcsMode,mode)
        && maybe_ne (mtcs_mode_get_nunits(mtcsMode,tmp_mode), 2 * mtcs_mode_get_nunits(mtcsMode,mode))){
          mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
          return NULL_RTX;
      }
  }else
    tmp_mode = mode;

  create_output_operand (&ops[0], target, tmp_mode);
  create_input_operand (&ops[1], xop0, mode0);
  create_input_operand (&ops[2], xop1, mode1);
  pat = mtcs_optabs_maybe_gen_insn (self,icode, 3, ops);
  if (pat){
      /* If PAT is composed of more than one insn, try to add an appropriate
     REG_EQUAL note to it.  If we can't because TEMP conflicts with an
     operand, call expand_binop again, this time without a target.  */
      if (INSN_P (pat) && NEXT_INSN (pat) != NULL_RTX
        && ! add_equal_note (self,pat, ops[0].value,optab_to_code (binoptab),ops[1].value, ops[2].value, mode0)){
          mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
          return mtcs_optabs_expand_binop(self,mode, binoptab, op0, op1, NULL_RTX,unsignedp, methods);
      }

      mtcs_emit_emit_insn (mtcsEmit,pat);
      return ops[0].value;
  }
  mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
  return NULL_RTX;
}

/* Given two input operands, OP0 and OP1, determine what the correct from_mode
   for a widening operation would be.  In most cases this would be OP0, but if
   that's a constant it'll be VOIDmode, which isn't useful.  */

static machine_mode widened_mode (MtcsOptabs *self,machine_mode to_mode, rtx op0, rtx op1)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  machine_mode m0 = GET_MODE (op0);
  machine_mode m1 = GET_MODE (op1);
  machine_mode result;

  if (m0 == VOIDmode && m1 == VOIDmode)
    return to_mode;
  else if (m0 == VOIDmode || mtcs_mode_get_unit_size (mtcsMode,m0) < mtcs_mode_get_unit_size (mtcsMode,m1))
    result = m1;
  else
    result = m0;

  if (mtcs_mode_get_unit_size (mtcsMode,result) > mtcs_mode_get_unit_size (mtcsMode,to_mode))
    return to_mode;

  return result;
}

/* As expand_unop, but will fail rather than attempt the operation in a
   different mode or with a libcall.  */
//原型 expand_unop_direct optabs.cc
static rtx expand_unop_direct (MtcsOptabs *self,machine_mode mode, optab unoptab, rtx op0, rtx target,int unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  if (mtcs_opinit_optab_handler/*!optab_handler*/ (mtcsOpinit,unoptab, mode) != CODE_FOR_nothing){
      class expand_operand ops[2];
      enum insn_code icode = mtcs_opinit_optab_handler (mtcsOpinit,unoptab, mode);
      rtx_insn *last = mtcs_rtl_data_get_last_insn (mtcsRtlData);
      rtx_insn *pat;

      create_output_operand (&ops[0], target, mode);
      create_convert_operand_from (&ops[1], op0, mode, unsignedp);
      pat = mtcs_optabs_maybe_gen_insn (self,icode, 2, ops);
      if (pat){
          if (INSN_P (pat) && NEXT_INSN (pat) != NULL_RTX
              && ! add_equal_note (self,pat, ops[0].value,optab_to_code (unoptab),ops[1].value, NULL_RTX, mode)){
              mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
              return mtcs_optabs_expand_unop (self,mode, unoptab, op0, NULL_RTX, unsignedp);
          }
          mtcs_emit_emit_insn (mtcsEmit,pat);
          return ops[0].value;
      }
  }
  return 0;
}

/* Try calculating clz, ctz or ffs of a double-word quantity as two clz, ctz or
   ffs operations on word-sized quantities, choosing which based on whether the
   high (for clz) or low (for ctz and ffs) word is nonzero.  */
static rtx expand_doubleword_clz_ctz_ffs (MtcsOptabs *self,scalar_int_mode mode, rtx op0, rtx target,optab unoptab)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit   *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr  *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  rtx xop0 = mtcs_explow_force_reg (mtcsExplow,mode, op0);
  rtx subhi = mtcs_rtl_gen_highpart (mtcsRTL,mtcsMode->word_mode, xop0);
  rtx sublo = gen_lowpart (mtcsMode->word_mode, xop0); //gen_lowpart rtl.h定义，mtcsrtl.c中重新指向
  rtx_code_label *hi0_label =mtcs_rtl_gen_label_rtx(mtcsRTL);
  rtx_code_label *after_label =mtcs_rtl_gen_label_rtx(mtcsRTL);
  rtx_insn *seq;
  rtx temp, result;
  int addend = 0;

  /* If we were not given a target, use a word_mode register, not a
     'mode' register.  The result will fit, and nobody is expecting
     anything bigger (the return type of __builtin_clz* is int).  */
  if (!target)
    target = mtcs_emit_gen_reg_rtx(mtcsEmit,mtcsMode->word_mode);

  /* In any case, write to a word_mode scratch in both branches of the
     conditional, so we can ensure there is a single move insn setting
     'target' to tag a REG_EQUAL note on.  */
  result = mtcs_emit_gen_reg_rtx(mtcsEmit,mtcsMode->word_mode);

  if (unoptab != clz_optab)
    std::swap (subhi, sublo);

  mtcs_emit_start_sequence (mtcsEmit);

  /* If the high word is not equal to zero,
     then clz of the full value is clz of the high word.  */
  mtcs_optabs_emit_cmp_and_jump_insns(self,subhi, CONST0_RTX (mtcsMode->word_mode), EQ, 0,mtcsMode->word_mode, true, hi0_label);

  if ( mtcs_opinit_optab_handler(mtcsOpinit,unoptab, mtcsMode->word_mode) != CODE_FOR_nothing)
    temp = expand_unop_direct (self,mtcsMode->word_mode, unoptab, subhi, result, true);
  else{
      gcc_assert (unoptab == ffs_optab);
      temp = expand_ffs(self,mtcsMode->word_mode, subhi, result);
  }
  if (!temp)
    goto fail;

  if (temp != result)
    mtcs_expr_convert_move(mtcsExpr,result, temp, true);

  mtcs_emit_emit_jump_insn/*!emit_jump_insn*/(mtcsEmit,target_rtx_gen_jump/*!targetm.gen_jump*/(mtcsMachine->tmrtx,after_label));
  mtcs_emit_emit_barrier/*!emit_barrier*/(mtcsEmit);

  /* Else clz of the full value is clz of the low word plus the number
     of bits in the high word.  Similarly for ctz/ffs of the high word,
     except that ffs should be 0 when both words are zero.  */
  mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,hi0_label);

  if (unoptab == ffs_optab){
      mtcs_expr_convert_move(mtcsExpr,result, const0_rtx, true);
      mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(self,sublo, CONST0_RTX (mtcsMode->word_mode), EQ, 0,
                   mtcsMode->word_mode, true, after_label);
  }

  if ( mtcs_opinit_optab_handler(mtcsOpinit,unoptab, mtcsMode->word_mode) != CODE_FOR_nothing)
    temp = expand_unop_direct(self,mtcsMode->word_mode, unoptab, sublo, NULL_RTX, true);
  else{
      gcc_assert (unoptab == ffs_optab);
      temp = expand_unop_direct(self,mtcsMode->word_mode, ctz_optab, sublo, NULL_RTX, true);
      addend = 1;
  }

  if (!temp)
    goto fail;

  temp = mtcs_optabs_expand_binop(self,mtcsMode->word_mode, add_optab, temp,mtcs_rtl_gen_int_mode (mtcsRTL,mtcs_mode_get_bitsize(mtcsMode,mtcsMode->word_mode) + addend,
                     mtcsMode->word_mode),result, true, OPTAB_DIRECT);
  if (!temp)
    goto fail;
  if (temp != result)
    mtcs_expr_convert_move(mtcsExpr,result, temp, true);

  mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,after_label);
  mtcs_expr_convert_move(mtcsExpr,target, result, true);

  seq = mtcs_rtl_data_get_insns(mtcsRtlData);
  mtcs_emit_end_sequence (mtcsEmit);

  add_equal_note (self,seq, target, optab_to_code (unoptab), xop0, NULL_RTX, mode);
  mtcs_emit_emit_insn (mtcsEmit,seq);
  return target;

 fail:
  mtcs_emit_end_sequence (mtcsEmit);
  return 0;
}

/* Try calculating
    (clz:narrow x)
   as
    (clz:wide (zero_extend:wide x)) - ((width wide) - (width narrow)).

   A similar operation can be used for clrsb.  UNOPTAB says which operation
   we are trying to expand.  */
static rtx widen_leading (MtcsOptabs *self,scalar_int_mode mode, rtx op0, rtx target, optab unoptab)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
  opt_scalar_int_mode wider_mode_iter;
  MTCS_FOR_EACH_WIDER_MODE (mtcsMode,wider_mode_iter, mode){
      scalar_int_mode wider_mode = wider_mode_iter.require ();
      if ( mtcs_opinit_optab_handler(mtcsOpinit,unoptab, wider_mode) != CODE_FOR_nothing){
          rtx xop0, temp;
          rtx_insn *last;
          last = mtcs_rtl_data_get_last_insn(mtcsRtlData);
          if (target == 0)
            target = mtcs_emit_gen_reg_rtx(mtcsEmit,mode);
          xop0 = widen_operand(self,op0, wider_mode, mode,unoptab != clrsb_optab, false);
          temp = mtcs_optabs_expand_unop (self,wider_mode, unoptab, xop0, NULL_RTX,unoptab != clrsb_optab);
          if (temp != 0)
            temp = mtcs_optabs_expand_binop(self,wider_mode, sub_optab, temp,
               mtcs_rtl_gen_int_mode (mtcsRTL,mtcs_mode_get_precision(mtcsMode,wider_mode)
                     - mtcs_mode_get_precision(mtcsMode,mode),wider_mode),target, true, OPTAB_DIRECT);
          if (temp == 0)
            mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
          return temp;
      }
  }
  return 0;
}

/* Attempt to emit (clrsb:mode op0) as
   (plus:mode (clz:mode (xor:mode op0 (ashr:mode op0 (const_int prec-1))))
          (const_int -1))
   if CLZ_DEFINED_VALUE_AT_ZERO (mode, val) is 2 and val is prec,
   or as
   (clz:mode (ior:mode (xor:mode (ashl:mode op0 (const_int 1))
                 (ashr:mode op0 (const_int prec-1)))
               (const_int 1)))
   otherwise.  */

static rtx expand_clrsb_using_clz (MtcsOptabs *self,scalar_int_mode mode, rtx op0, rtx target)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit   *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsPredict *mtcsPredict =mtcs_target_get_predict(mtcsTarget);
  MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  if (mtcs_predict_optimize_insn_for_size_p/*!optimize_insn_for_size_p*/(mtcsPredict)
      ||  mtcs_opinit_optab_handler(mtcsOpinit,clz_optab, mode) == CODE_FOR_nothing)
    return NULL_RTX;

  mtcs_emit_start_sequence (mtcsEmit);
  HOST_WIDE_INT val = 0;
  if (CLZ_DEFINED_VALUE_AT_ZERO (mode, val) != 2 || val != mtcs_mode_get_precision(mtcsMode,mode))
    val = 0;
  else
    val = 1;

  rtx temp2 = op0;
  if (!val){
      temp2 = mtcs_optabs_expand_binop(self,mode, ashl_optab, op0, const1_rtx,NULL_RTX, 0, OPTAB_DIRECT);
      if (!temp2){
    fail:
          mtcs_emit_end_sequence (mtcsEmit);
          return NULL_RTX;
      }
  }

  rtx temp = mtcs_optabs_expand_binop(self,mode, ashr_optab, op0,
          mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,mtcs_mode_get_precision(mtcsMode,mode) - 1),NULL_RTX, 0, OPTAB_DIRECT);
  if (!temp)
    goto fail;

  temp = mtcs_optabs_expand_binop(self,mode, xor_optab, temp2, temp, NULL_RTX, 0,OPTAB_DIRECT);
  if (!temp)
    goto fail;

  if (!val){
      temp = mtcs_optabs_expand_binop(self,mode, ior_optab, temp, const1_rtx,NULL_RTX, 0, OPTAB_DIRECT);
      if (!temp)
          goto fail;
  }
  temp = expand_unop_direct(self,mode, clz_optab, temp, val ? NULL_RTX : target,true);
  if (!temp)
    goto fail;
  if (val){
      temp = mtcs_optabs_expand_binop(self,mode, add_optab, temp, constm1_rtx,target, 0, OPTAB_DIRECT);
      if (!temp)
          goto fail;
  }

  rtx_insn *seq = mtcs_rtl_data_get_insns(mtcsRtlData);
  mtcs_emit_end_sequence (mtcsEmit);
  add_equal_note(self,seq, temp, CLRSB, op0, NULL_RTX, mode);
  mtcs_emit_emit_insn (mtcsEmit,seq);
  return temp;
}


/* Try calculating popcount of a double-word quantity as two popcount's of
   word-sized quantities and summing up the results.  */
static rtx expand_doubleword_popcount (MtcsOptabs *self,scalar_int_mode mode, rtx op0, rtx target)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit   *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  rtx t0, t1, t;
  rtx_insn *seq;

  mtcs_emit_start_sequence (mtcsEmit);

  t0 = expand_unop_direct (self,mtcsMode->word_mode, popcount_optab,
               mtcs_rtl_operand_subword_force (mtcsRTL,op0, 0, mode), NULL_RTX,true);
  t1 = expand_unop_direct (self,mtcsMode->word_mode, popcount_optab,
          mtcs_rtl_operand_subword_force (mtcsRTL,op0, 1, mode), NULL_RTX,true);
  if (!t0 || !t1){
      mtcs_emit_end_sequence (mtcsEmit);
      return NULL_RTX;
  }

  /* If we were not given a target, use a mtcsMode->word_mode register, not a
     'mode' register.  The result will fit, and nobody is expecting
     anything bigger (the return type of __builtin_popcount* is int).  */
  if (!target)
    target = mtcs_emit_gen_reg_rtx(mtcsEmit,mtcsMode->word_mode);

  t = mtcs_optabs_expand_binop(self,mtcsMode->word_mode, add_optab, t0, t1, target, 0, OPTAB_DIRECT);

  seq = mtcs_rtl_data_get_insns(mtcsRtlData);
  mtcs_emit_end_sequence (mtcsEmit);

  add_equal_note(self,seq, t, POPCOUNT, op0, NULL_RTX, mode);
  mtcs_emit_emit_insn (mtcsEmit,seq);
  return t;
}

/* Try calculating
    (parity:wide x)
   as
    (parity:narrow (low (x) ^ high (x))) */
static rtx expand_doubleword_parity (MtcsOptabs *self,scalar_int_mode mode, rtx op0, rtx target)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  rtx t = mtcs_optabs_expand_binop(self,mtcsMode->word_mode, xor_optab,
            mtcs_rtl_operand_subword_force (mtcsRTL,op0, 0, mode),
            mtcs_rtl_operand_subword_force (mtcsRTL,op0, 1, mode),
            NULL_RTX, 0, OPTAB_DIRECT);
  return mtcs_optabs_expand_unop(self,mtcsMode->word_mode, parity_optab, t, target, true);
}


/* Generate code to perform an operation specified by UNOPTAB
   on operand OP0, with result having machine-mode MODE.

   UNSIGNEDP is for the case where we have to widen the operands
   to perform the operation.  It says to use zero-extension.

   If TARGET is nonzero, the value
   is generated there, if it is convenient to do so.
   In all cases an rtx is returned for the locus of the value;
   this may or may not be TARGET.  */
//原型 expand_unop optabs.h optabs.cc
rtx mtcs_optabs_expand_unop (MtcsOptabs *self,machine_mode mode, optab unoptab, rtx op0, rtx target,int unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit   *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsExpmed   *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
  MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);

  enum mode_class mclass =mtcs_mode_get_class(mtcsMode,mode);
  machine_mode wider_mode;
  scalar_int_mode int_mode;
  scalar_float_mode float_mode;
  rtx temp;
  rtx libfunc;

  temp = expand_unop_direct (self,mode, unoptab, op0, target, unsignedp);
  if (temp)
    return temp;

  /* It can't be done in this mode.  Can we open-code it in a wider mode?  */

  /* Widening (or narrowing) clz needs special treatment.  */
  if (unoptab == clz_optab){
      if (mtcs_mode_is_a <scalar_int_mode>(mtcsMode,mode, &int_mode)){
          temp = widen_leading (self,int_mode, op0, target, unoptab);
          if (temp)
            return temp;

          if (mtcs_mode_get_size (mtcsMode,int_mode) == 2 * UNITS_PER_WORD
                  && mtcs_opinit_optab_handler/*!optab_handler*/ (mtcsOpinit,unoptab, mtcsMode->word_mode) != CODE_FOR_nothing){
              temp = expand_doubleword_clz_ctz_ffs(self,int_mode, op0, target,unoptab);
              if (temp)
                  return temp;
          }
      }
      goto try_libcall;
  }

  if (unoptab == clrsb_optab){
      if (mtcs_mode_is_a <scalar_int_mode>(mtcsMode,mode, &int_mode)){
          temp = widen_leading (self,int_mode, op0, target, unoptab);
          if (temp)
            return temp;
          temp = expand_clrsb_using_clz(self,int_mode, op0, target);
          if (temp)
            return temp;
      }
      goto try_libcall;
  }

  if (unoptab == popcount_optab
      && mtcs_mode_is_a <scalar_int_mode>(mtcsMode,mode, &int_mode)
      && mtcs_mode_get_size(mtcsMode,int_mode) == 2 * UNITS_PER_WORD
      && mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,unoptab, mtcsMode->word_mode) != CODE_FOR_nothing
      && optimize_insn_for_speed_p ()) {
      temp = expand_doubleword_popcount(self,int_mode, op0, target);
      if (temp)
          return temp;
  }

  if (unoptab == parity_optab
      && mtcs_mode_is_a <scalar_int_mode>(mtcsMode,mode, &int_mode)
      && mtcs_mode_get_size(mtcsMode,int_mode) == 2 * UNITS_PER_WORD
      && (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,unoptab, mtcsMode->word_mode) != CODE_FOR_nothing
      || mtcs_opinit_optab_handler/*optab_handler*/ (mtcsOpinit,popcount_optab, mtcsMode->word_mode) != CODE_FOR_nothing)
      && optimize_insn_for_speed_p ()){
      temp = expand_doubleword_parity(self,int_mode, op0, target);
      if (temp)
          return temp;
  }

  /* Widening (or narrowing) bswap needs special treatment.  */
  if (unoptab == bswap_optab){
      /* HImode is special because in this mode BSWAP is equivalent to ROTATE
     or ROTATERT.  First try these directly; if this fails, then try the
     obvious pair of shifts with allowed widening, as this will probably
     be always more efficient than the other fallback methods.  */
      if (mode == mtcsMode->modes.M_HImode){
          rtx_insn *last;
          rtx temp1, temp2;

          if (mtcs_opinit_optab_handler/*optab_handler*/(mtcsOpinit,rotl_optab, mode) != CODE_FOR_nothing){
              temp = mtcs_optabs_expand_binop(self,mode, rotl_optab, op0, mtcs_rtl_gen_int_shift_amount (mtcsRTL,mode, 8),target, unsignedp, OPTAB_DIRECT);
              if (temp)
                  return temp;
          }

          if (mtcs_opinit_optab_handler/*optab_handler*/ (mtcsOpinit,rotr_optab, mode) != CODE_FOR_nothing){
              temp = mtcs_optabs_expand_binop (self,mode, rotr_optab, op0, mtcs_rtl_gen_int_shift_amount (mtcsRTL,mode, 8),target, unsignedp, OPTAB_DIRECT);
              if (temp)
                  return temp;
          }

          last = mtcs_rtl_data_get_last_insn (mtcsRtlData);

          temp1 = mtcs_optabs_expand_binop (self,mode, ashl_optab, op0,mtcs_rtl_gen_int_shift_amount (mtcsRTL,mode, 8), NULL_RTX,unsignedp, OPTAB_WIDEN);
          temp2 = mtcs_optabs_expand_binop (self,mode, lshr_optab, op0,mtcs_rtl_gen_int_shift_amount (mtcsRTL,mode, 8), NULL_RTX,unsignedp, OPTAB_WIDEN);
          if (temp1 && temp2){
              temp = mtcs_optabs_expand_binop (self,mode, ior_optab, temp1, temp2, target, unsignedp, OPTAB_WIDEN);
              if (temp)
                  return temp;
          }
          mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
      }

      if (mtcs_mode_is_a <scalar_int_mode>(mtcsMode,mode, &int_mode)){
          temp = widen_bswap(self,int_mode, op0, target);
          if (temp)
            return temp;

          /* We do not provide a 128-bit bswap in libgcc so force the use of
             a double bswap for 64-bit targets.  */
          if (mtcs_mode_get_size(mtcsMode,int_mode) == 2 * UNITS_PER_WORD   && (UNITS_PER_WORD == 8
              ||  mtcs_opinit_optab_handler(mtcsOpinit,unoptab, mtcsMode->word_mode) != CODE_FOR_nothing)){
              temp = expand_doubleword_bswap(self,mode, op0, target);
              if (temp)
                  return temp;
          }
      }
      goto try_libcall;
  }

  if (CLASS_HAS_WIDER_MODES_P (mclass))
      MTCS_FOR_EACH_WIDER_MODE/*!FOR_EACH_WIDER_MODE*/(mtcsMode,wider_mode, mode) {
        if ( mtcs_opinit_optab_handler(mtcsOpinit,unoptab, wider_mode) != CODE_FOR_nothing){
            rtx xop0 = op0;
            rtx_insn *last = mtcs_rtl_data_get_last_insn (mtcsRtlData);
            /* For certain operations, we need not actually extend
               the narrow operand, as long as we will truncate the
               results to the same narrowness.  */
            xop0 = widen_operand(self,xop0, wider_mode, mode, unsignedp,
                      (unoptab == neg_optab || unoptab == one_cmpl_optab) && mclass == MODE_INT);
            temp = mtcs_optabs_expand_unop(self,wider_mode, unoptab, xop0, NULL_RTX,unsignedp);
            if (temp){
                if (mclass != MODE_INT || !mtcs_mode_truly_noop_truncation_p/*!TRULY_NOOP_TRUNCATION_MODES_P*/ (mtcsMode,mode, wider_mode)){
                    if (target == 0)
                      target = mtcs_emit_gen_reg_rtx(mtcsEmit,mode);
                    mtcs_expr_convert_move(mtcsExpr,target, temp, 0);
                    return target;
                }else
                  return gen_lowpart (mode, temp);
            }else
              mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
        }
      }

  /* These can be done a word at a time.  */
  if (unoptab == one_cmpl_optab  && mtcs_mode_is_int_mode (mtcsMode,mode, &int_mode) && mtcs_mode_get_size (mtcsMode,int_mode) > UNITS_PER_WORD
          &&  mtcs_opinit_optab_handler(mtcsOpinit,unoptab, mtcsMode->word_mode) != CODE_FOR_nothing){
      int i;
      rtx_insn *insns;

      if (target == 0 || target == op0 || mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,target, op0)
              || !mtcs_optabs_valid_multiword_target_p(self,target))
          target = mtcs_emit_gen_reg_rtx(mtcsEmit,int_mode);

      mtcs_emit_start_sequence/*start_sequence*/(mtcsEmit);
      /* Do the actual arithmetic.  */
      for (i = 0; i < mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,int_mode) / BITS_PER_WORD; i++){
          rtx target_piece = mtcs_rtl_operand_subword(mtcsRTL,target, i, 1, int_mode);
          rtx x = mtcs_optabs_expand_unop(self,mtcsMode->word_mode, unoptab,
                       mtcs_rtl_operand_subword_force(mtcsRTL,op0, i, int_mode),
                       target_piece, unsignedp);

          if (target_piece != x)
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target_piece, x);
      }
      insns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      mtcs_emit_end_sequence (mtcsEmit);
      mtcs_emit_emit_insn (mtcsEmit,insns);
      return target;
  }

  /* Emit ~op0 as op0 ^ -1.  */
  if (unoptab == one_cmpl_optab
      && (mtcs_mode_is_scalar_int_p (mtcsMode,mode) || mtcs_mode_get_class (mtcsMode,mode) == MODE_VECTOR_INT)
      &&  mtcs_opinit_optab_handler(mtcsOpinit,xor_optab, mode) != CODE_FOR_nothing){
      temp = mtcs_optabs_expand_binop (self,mode, xor_optab, op0, CONSTM1_RTX (mode),target, unsignedp, OPTAB_DIRECT);
      if (temp)
          return temp;
  }

  if (optab_to_code (unoptab) == NEG){
      /* Try negating floating point values by flipping the sign bit.  */
      if (mtcs_mode_is_a <scalar_float_mode>(mtcsMode,mode, &float_mode)){
          temp = expand_absneg_bit (self,NEG, float_mode, op0, target);
          if (temp)
            return temp;
      }
      /* If there is no negation pattern, and we have no negative zero,
     try subtracting from zero.  */
      if (!mtcs_mode_honor_signed_zeros(mtcsMode,mode)){
          temp = mtcs_optabs_expand_binop (self,mode, (unoptab == negv_optab  ? subv_optab : sub_optab),CONST0_RTX (mode), op0, target,unsignedp, OPTAB_DIRECT);
          if (temp)
            return temp;
      }
  }
  /* Try calculating parity (x) as popcount (x) % 2.  */
  if (unoptab == parity_optab && mtcs_mode_is_a <scalar_int_mode>(mtcsMode,mode, &int_mode)){
      temp = expand_parity (self,int_mode, op0, target);
      if (temp)
          return temp;
  }

  /* Try implementing ffs (x) in terms of clz (x).  */
  if (unoptab == ffs_optab && mtcs_mode_is_a <scalar_int_mode>(mtcsMode,mode, &int_mode)){
      temp = expand_ffs(self,int_mode, op0, target);
      if (temp)
          return temp;
  }
  /* Try implementing ctz (x) in terms of clz (x).  */
  if (unoptab == ctz_optab && mtcs_mode_is_a <scalar_int_mode>(mtcsMode,mode, &int_mode)){
      temp = expand_ctz(self,int_mode, op0, target);
      if (temp)
          return temp;
  }

  if ((unoptab == ctz_optab || unoptab == ffs_optab)
      && optimize_insn_for_speed_p ()
      && mtcs_mode_is_a <scalar_int_mode>(mtcsMode,mode, &int_mode)
      && mtcs_mode_get_size (mtcsMode,int_mode) == 2 * UNITS_PER_WORD
      && ( mtcs_opinit_optab_handler(mtcsOpinit,unoptab, mtcsMode->word_mode) != CODE_FOR_nothing
      ||  mtcs_opinit_optab_handler(mtcsOpinit,ctz_optab, mtcsMode->word_mode) != CODE_FOR_nothing)){
      temp = expand_doubleword_clz_ctz_ffs(self,int_mode, op0, target, unoptab);
      if (temp)
          return temp;
  }

 try_libcall:
  /* Now try a library call in this mode.  */
  libfunc = mtcs_libfuncs_optab_libfunc/*!optab_libfunc*/(mtcsLibfuncs,unoptab, mode);
  if (libfunc){
      rtx_insn *insns;
      rtx value;
      rtx eq_value;
      machine_mode outmode = mode;

      /* All of these functions return small values.  Thus we choose to
     have them return something that isn't a double-word.  */
      if (unoptab == ffs_optab || unoptab == clz_optab || unoptab == ctz_optab
         || unoptab == clrsb_optab || unoptab == popcount_optab  || unoptab == parity_optab)
          outmode= GET_MODE (mtcs_explow_hard_libcall_value/*!hard_libcall_value*/(mtcsExplow,
                TYPE_MODE (integer_type_node),mtcs_libfuncs_optab_libfunc/*!optab_libfunc*/(mtcsLibfuncs,unoptab, mode)));

      mtcs_emit_start_sequence (mtcsEmit);

      /* Pass 1 for NO_QUEUE so we don't lose any increments
     if the libcall is cse'd or moved.  */
      value =mtcs_calls_emit_library_call_value/*!emit_library_call_value*/(mtcsCalls,
            libfunc, NULL_RTX, LCT_CONST, outmode,op0, mode);
      insns = mtcs_rtl_data_get_insns(mtcsRtlData);
      mtcs_emit_end_sequence (mtcsEmit);

      target = mtcs_emit_gen_reg_rtx(mtcsEmit,outmode);
      bool trapv = trapv_unoptab_p (unoptab);
      if (trapv)
          eq_value = NULL_RTX;
      else{
          eq_value = gen_rtx_fmt_e (optab_to_code (unoptab), mode, op0);
          if (mtcs_mode_get_unit_size (mtcsMode,outmode) < mtcs_mode_get_unit_size (mtcsMode,mode))
              eq_value = simplify_gen_unary (TRUNCATE, outmode, eq_value, mode);
          else if (mtcs_mode_get_unit_size (mtcsMode,outmode) > mtcs_mode_get_unit_size (mtcsMode,mode))
              eq_value = simplify_gen_unary (ZERO_EXTEND,outmode, eq_value, mode);
      }
      emit_libcall_block_1 (self,insns, target, value, eq_value, trapv);

      return target;
  }

  /* It can't be done in this mode.  Can we do it in a wider mode?  */

  if (CLASS_HAS_WIDER_MODES_P (mclass)){
      MTCS_FOR_EACH_WIDER_MODE (mtcsMode,wider_mode, mode){
          if ( mtcs_opinit_optab_handler(mtcsOpinit,unoptab, wider_mode) != CODE_FOR_nothing
              || mtcs_libfuncs_optab_libfunc/*!optab_libfunc*/(mtcsLibfuncs,unoptab, wider_mode)){
              rtx xop0 = op0;
              rtx_insn *last = mtcs_rtl_data_get_last_insn(mtcsRtlData);

              /* For certain operations, we need not actually extend
             the narrow operand, as long as we will truncate the
             results to the same narrowness.  */
              xop0 = widen_operand(self,xop0, wider_mode, mode, unsignedp,
                        (unoptab == neg_optab
                         || unoptab == one_cmpl_optab
                         || unoptab == bswap_optab)
                        && mclass == MODE_INT);

              temp = mtcs_optabs_expand_unop(self,wider_mode, unoptab, xop0, NULL_RTX,unsignedp);
              /* If we are generating clz using wider mode, adjust the
             result.  Similarly for clrsb.  */
              if ((unoptab == clz_optab || unoptab == clrsb_optab) && temp != 0){
                  scalar_int_mode wider_int_mode= mtcs_mode_as_a <scalar_int_mode> (mtcsMode,wider_mode);
                  int_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,mode);
                  temp = mtcs_optabs_expand_binop/*!expand_binop*/(self,wider_mode, sub_optab, temp,
                        mtcs_rtl_gen_int_mode (mtcsRTL,mtcs_mode_get_precision(mtcsMode,wider_int_mode)
                           - mtcs_mode_get_precision(mtcsMode,int_mode),wider_int_mode),target, true, OPTAB_DIRECT);
              }

              /* Likewise for bswap.  */
              if (unoptab == bswap_optab && temp != 0){
                  scalar_int_mode wider_int_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,wider_mode);
                  int_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,mode);
                  gcc_assert (mtcs_mode_get_precision(mtcsMode,wider_int_mode)
                          == mtcs_mode_get_bitsize(mtcsMode,wider_int_mode)
                          && mtcs_mode_get_precision(mtcsMode,int_mode)
                         == mtcs_mode_get_bitsize(mtcsMode,int_mode));

                  temp = mtcs_expmed_expand_shift(mtcsExpmed,RSHIFT_EXPR, wider_int_mode, temp,
                               mtcs_mode_get_bitsize(mtcsMode,wider_int_mode)- mtcs_mode_get_bitsize(mtcsMode,int_mode),NULL_RTX, true);
              }

              if (temp){
                  if (mclass != MODE_INT){
                      if (target == 0)
                          target = mtcs_emit_gen_reg_rtx(mtcsEmit,mode);
                      mtcs_expr_convert_move(mtcsExpr,target, temp, 0);
                      return target;
                  }else
                    return gen_lowpart (mode, temp);
              }else
                  mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
          }
      }
  }

  /* One final attempt at implementing negation via subtraction,
     this time allowing widening of the operand.  */
  if (optab_to_code (unoptab) == NEG && !mtcs_mode_honor_signed_zeros(mtcsMode,mode)){
      rtx temp;
      temp = mtcs_optabs_expand_binop (self,mode,unoptab == negv_optab ? subv_optab : sub_optab,
                           CONST0_RTX (mode), op0, target, unsignedp, OPTAB_LIB_WIDEN);
      if (temp)
        return temp;
  }

  return 0;
}

/* Generate code to perform an operation specified by BINOPTAB
   on operands OP0 and OP1, with result having machine-mode MODE.

   UNSIGNEDP is for the case where we have to widen the operands
   to perform the operation.  It says to use zero-extension.

   If TARGET is nonzero, the value
   is generated there, if it is convenient to do so.
   In all cases an rtx is returned for the locus of the value;
   this may or may not be TARGET.  */
//原型 expand_binop optabs.h optabs.cc
rtx mtcs_optabs_expand_binop (MtcsOptabs *self,machine_mode mode, optab binoptab, rtx op0, rtx op1,
        rtx target, int unsignedp, int /*!enum optab_methods methods 编译通不过*/ methods)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr  *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsEmit  *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsExpmed   *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
  MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);

  enum optab_methods next_methods = (methods == OPTAB_LIB || methods == OPTAB_LIB_WIDEN ? OPTAB_WIDEN : methods);
  enum mode_class mclass;
  enum insn_code icode;
  machine_mode wider_mode;
  scalar_int_mode int_mode;
  rtx libfunc;
  rtx temp;
  rtx_insn *entry_last = mtcs_rtl_data_get_last_insn(mtcsRtlData);
  rtx_insn *last;

  mclass = mtcs_mode_get_class(mtcsMode,mode);

  /* If subtracting an integer constant, convert this into an addition of
     the negated constant.  */

  if (binoptab == sub_optab && CONST_INT_P (op1)){
      op1 = mtcs_expmed_negate_rtx/*!negate_rtx*/(mtcsExpmed,mode, op1);
      binoptab = add_optab;
  }
  /* For shifts, constant invalid op1 might be expanded from different
     mode than MODE.  As those are invalid, force them to a register
     to avoid further problems during expansion.  */
  else if (CONST_INT_P (op1) && shift_optab_p (binoptab)
       && UINTVAL (op1) >= mtcs_mode_get_bitsize(mtcsMode,mtcs_mode_get_inner(mtcsMode,mode))){
      op1 = mtcs_rtl_gen_int_mode (mtcsRTL,INTVAL (op1), mtcs_mode_get_inner(mtcsMode,mode));
      op1 = mtcs_explow_force_reg(mtcsExplow,mtcs_mode_get_inner(mtcsMode,mode), op1);
  }

  /* Record where to delete back to if we backtrack.  */
  last = mtcs_rtl_data_get_last_insn(mtcsRtlData);

  /* If we can do it with a three-operand insn, do so.  */

  if (methods != OPTAB_MUST_WIDEN){
      if (convert_optab_p (binoptab)){
          machine_mode from_mode = widened_mode (self,mode, op0, op1);
          icode = mtcs_optabs_find_widening_optab_handler_and_mode/*!find_widening_optab_handler*/(self,binoptab, mode, from_mode,NULL);
      }else
          icode =  mtcs_opinit_optab_handler(mtcsOpinit,binoptab, mode);
      if (icode != CODE_FOR_nothing){
          temp = expand_binop_directly (self,icode, mode, binoptab, op0, op1,target, unsignedp, methods, last);
          if (temp)
            return temp;
      }
  }

  /* If we were trying to rotate, and that didn't work, try rotating
     the other direction before falling back to shifts and bitwise-or.  */
  if (((binoptab == rotl_optab
    && (icode =  mtcs_opinit_optab_handler(mtcsOpinit,rotr_optab, mode)) != CODE_FOR_nothing)
       || (binoptab == rotr_optab
       && (icode =  mtcs_opinit_optab_handler(mtcsOpinit,rotl_optab, mode)) != CODE_FOR_nothing))
      && mtcs_mode_is_int_mode (mtcsMode,mode, &int_mode)) {
      optab otheroptab = (binoptab == rotl_optab ? rotr_optab : rotl_optab);
      rtx newop1;
      unsigned int bits = mtcs_mode_get_precision(mtcsMode,int_mode);

      if (CONST_INT_P (op1))
          newop1 = mtcs_rtl_gen_int_shift_amount (mtcsRTL,int_mode, bits - INTVAL (op1));
      else if (mtcsTarget->/*!targetm.shift_truncation_mask*/shift_truncation_mask(mtcsTarget,int_mode) == bits - 1)
          newop1 = mtcs_expmed_negate_rtx/*!negate_rtx*/(mtcsExpmed,GET_MODE (op1), op1);
      else
          newop1 = mtcs_optabs_expand_binop (self,GET_MODE (op1), sub_optab,
                   mtcs_rtl_gen_int_mode (mtcsRTL,bits, GET_MODE (op1)), op1,
                   NULL_RTX, unsignedp, OPTAB_DIRECT);

      temp = expand_binop_directly (self,icode, int_mode, otheroptab, op0, newop1,target, unsignedp, methods, last);
      if (temp)
          return temp;
  }

  /* If this is a multiply, see if we can do a widening operation that
     takes operands of this mode and makes a wider mode.  */

  if (binoptab == smul_optab  && mtcs_mode_get_2xwider/*!GET_MODE_2XWIDER_MODE*/ (mtcsMode,mode).exists (&wider_mode)
      && (mtcs_opinit_convert_optab_handler(mtcsOpinit,(unsignedp
                  ? umul_widen_optab: smul_widen_optab), wider_mode, mode) != CODE_FOR_nothing)){
      /* *_widen_optab needs to determine operand mode, make sure at least
     one operand has non-VOID mode.  */
      if (GET_MODE (op0) == VOIDmode && GET_MODE (op1) == VOIDmode)
          op0 = mtcs_explow_force_reg(mtcsExplow,mode, op0);
      temp = mtcs_optabs_expand_binop (self,wider_mode, unsignedp ? umul_widen_optab : smul_widen_optab,
               op0, op1, NULL_RTX, unsignedp, OPTAB_DIRECT);

      if (temp != 0){
         if (mtcs_mode_get_class(mtcsMode,mode) == MODE_INT
                 && mtcs_mode_truly_noop_truncation_p/*!TRULY_NOOP_TRUNCATION_MODES_P*/(mtcsMode,mode, GET_MODE (temp)))
            return gen_lowpart (mode, temp);
         else
            return mtcs_expr_convert_to_mode(mtcsExpr,mode, temp, unsignedp);
      }
  }

  /* If this is a vector shift by a scalar, see if we can do a vector
     shift by a vector.  If so, broadcast the scalar into a vector.  */
  if (mclass == MODE_VECTOR_INT) {
      optab otheroptab = unknown_optab;
      if (binoptab == ashl_optab)
          otheroptab = vashl_optab;
      else if (binoptab == ashr_optab)
          otheroptab = vashr_optab;
      else if (binoptab == lshr_optab)
          otheroptab = vlshr_optab;
      else if (binoptab == rotl_optab)
          otheroptab = vrotl_optab;
      else if (binoptab == rotr_optab)
          otheroptab = vrotr_optab;

      if (otheroptab && (icode =  mtcs_opinit_optab_handler(mtcsOpinit,otheroptab, mode)) != CODE_FOR_nothing){
          /* The scalar may have been extended to be too wide.  Truncate
             it back to the proper size to fit in the broadcast vector.  */
          scalar_mode inner_mode = mtcs_mode_get_inner(mtcsMode,mode);
          if (!CONST_INT_P (op1)
              && (mtcs_mode_get_bitsize(mtcsMode,mtcs_mode_as_a <scalar_int_mode> (mtcsMode,
                      GET_MODE (op1))) > mtcs_mode_get_bitsize(mtcsMode,inner_mode)))
            op1 = mtcs_explow_force_reg(mtcsExplow,inner_mode,simplify_gen_unary (TRUNCATE, inner_mode, op1,GET_MODE (op1)));
          rtx vop1 = mtcs_optabs_expand_vector_broadcast (self,mode, op1);
          if (vop1){
              temp = expand_binop_directly (self,icode, mode, otheroptab, op0, vop1,target, unsignedp, methods, last);
              if (temp)
                  return temp;
          }
      }
  }

  /* Look for a wider mode of the same class for which we think we
     can open-code the operation.  Check for a widening multiply at the
     wider mode as well.  */

  if (CLASS_HAS_WIDER_MODES_P (mclass) && methods != OPTAB_DIRECT && methods != OPTAB_LIB)
    MTCS_FOR_EACH_WIDER_MODE (mtcsMode,wider_mode, mode){
        machine_mode next_mode;
        if ( mtcs_opinit_optab_handler(mtcsOpinit,binoptab, wider_mode) != CODE_FOR_nothing
            || (binoptab == smul_optab
            && mtcs_mode_get_wider(mtcsMode,wider_mode).exists (&next_mode)
            && (mtcs_optabs_find_widening_optab_handler_and_mode/*!find_widening_optab_handler 这是一个宏*/
                    (self,(unsignedp? umul_widen_optab: smul_widen_optab),next_mode, mode,NULL)!= CODE_FOR_nothing))){
            rtx xop0 = op0, xop1 = op1;
            bool no_extend = false;

            /* For certain integer operations, we need not actually extend
               the narrow operands, as long as we will truncate
               the results to the same narrowness.  */

            if ((binoptab == ior_optab || binoptab == and_optab  || binoptab == xor_optab || binoptab == add_optab
                    || binoptab == sub_optab || binoptab == smul_optab || binoptab == ashl_optab)  && mclass == MODE_INT){
                no_extend = true;
                xop0 = avoid_expensive_constant (self,mode, binoptab, 0,xop0, unsignedp);
                if (binoptab != ashl_optab)
                  xop1 = avoid_expensive_constant (self,mode, binoptab, 1,xop1, unsignedp);
            }

            xop0 = widen_operand(self,xop0, wider_mode, mode, unsignedp, no_extend);

            /* The second operand of a shift must always be extended.  */
            xop1 = widen_operand(self,xop1, wider_mode, mode, unsignedp,no_extend && binoptab != ashl_optab);

            temp = mtcs_optabs_expand_binop (self,wider_mode, binoptab, xop0, xop1, NULL_RTX,
                     unsignedp, OPTAB_DIRECT);
            if (temp){
                if (mclass != MODE_INT
                            || !mtcs_mode_truly_noop_truncation_p/*!TRULY_NOOP_TRUNCATION_MODES_P*/ (mtcsMode,mode, wider_mode)){
                    if (target == 0)
                      target = mtcs_emit_gen_reg_rtx(mtcsEmit,mode);
                    mtcs_expr_convert_move(mtcsExpr,target, temp, 0);
                    return target;
                }else
                  return gen_lowpart (mode, temp);
            }else
              mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
          }
      }

  /* If operation is commutative,
     try to make the first operand a register.
     Even better, try to make it the same as the target.
     Also try to make the last operand a constant.  */
  if (commutative_optab_p (binoptab)  && swap_commutative_operands_with_target (self,target, op0, op1))
    std::swap (op0, op1);

  /* These can be done a word at a time.  */
  if ((binoptab == and_optab || binoptab == ior_optab || binoptab == xor_optab)
      && mtcs_mode_is_int_mode (mtcsMode,mode, &int_mode)
      && mtcs_mode_get_size(mtcsMode,int_mode) > UNITS_PER_WORD
      &&  mtcs_opinit_optab_handler(mtcsOpinit,binoptab, mtcsMode->word_mode) != CODE_FOR_nothing){
      int i;
      rtx_insn *insns;

      /* If TARGET is the same as one of the operands, the REG_EQUAL note
     won't be accurate, so use a new target.  */
      if (target == 0
      || target == op0
      || target == op1
      || mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,target, op0)
      || mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,target, op1)
      || !mtcs_optabs_valid_multiword_target_p(self,target))
          target = mtcs_emit_gen_reg_rtx(mtcsEmit,int_mode);

      mtcs_emit_start_sequence (mtcsEmit);

      /* Do the actual arithmetic.  */
      machine_mode op0_mode = GET_MODE (op0);
      machine_mode op1_mode = GET_MODE (op1);
      if (op0_mode == VOIDmode)
          op0_mode = int_mode;
      if (op1_mode == VOIDmode)
          op1_mode = int_mode;
      for (i = 0; i < mtcs_mode_get_bitsize(mtcsMode,int_mode) / BITS_PER_WORD; i++){
          rtx target_piece = mtcs_rtl_operand_subword(mtcsRTL,target, i, 1, int_mode);
          rtx x = mtcs_optabs_expand_binop (self,mtcsMode->word_mode, binoptab,
                    mtcs_rtl_operand_subword_force(mtcsRTL,op0, i, op0_mode),
                    mtcs_rtl_operand_subword_force(mtcsRTL,op1, i, op1_mode),
                    target_piece, unsignedp, next_methods);

          if (x == 0)
            break;

          if (target_piece != x)
            mtcs_expr_emit_move_insn(mtcsExpr,target_piece, x);
      }

      insns = mtcs_rtl_data_get_insns(mtcsRtlData);
      mtcs_emit_end_sequence (mtcsEmit);

      if (i == mtcs_mode_get_bitsize(mtcsMode,int_mode) / BITS_PER_WORD){
          mtcs_emit_emit_insn (mtcsEmit,insns);
          return target;
      }
  }

  /* Synthesize double word shifts from single word shifts.  */
  if ((binoptab == lshr_optab || binoptab == ashl_optab
       || binoptab == ashr_optab)
      && mtcs_mode_is_int_mode (mtcsMode,mode, &int_mode)
      && (CONST_INT_P (op1) || optimize_insn_for_speed_p ())
      && mtcs_mode_get_size(mtcsMode,int_mode) == 2 * UNITS_PER_WORD
      && mtcs_mode_get_precision(mtcsMode,int_mode) == mtcs_mode_get_bitsize(mtcsMode,int_mode)
      &&  mtcs_opinit_optab_handler(mtcsOpinit,binoptab, mtcsMode->word_mode) != CODE_FOR_nothing
      &&  mtcs_opinit_optab_handler(mtcsOpinit,ashl_optab, mtcsMode->word_mode) != CODE_FOR_nothing
      &&  mtcs_opinit_optab_handler(mtcsOpinit,lshr_optab, mtcsMode->word_mode) != CODE_FOR_nothing)
    {
      unsigned HOST_WIDE_INT shift_mask, double_shift_mask;
      scalar_int_mode op1_mode;

      double_shift_mask = mtcsTarget/*!targetm.shift_truncation_mask*/->shift_truncation_mask(mtcsTarget,int_mode);
      shift_mask = mtcsTarget/*!targetm.shift_truncation_mask*/->shift_truncation_mask(mtcsTarget,mtcsMode->word_mode);
      op1_mode = (GET_MODE (op1) != VOIDmode
          ? mtcs_mode_as_a <scalar_int_mode> (mtcsMode,GET_MODE (op1))
          : mtcsMode->word_mode);

      /* Apply the truncation to constant shifts.  */
      if (double_shift_mask > 0 && CONST_INT_P (op1))
         op1 = mtcs_rtl_gen_int_mode (mtcsRTL,INTVAL (op1) & double_shift_mask, op1_mode);

      if (op1 == CONST0_RTX (op1_mode))
         return op0;

      /* Make sure that this is a combination that expand_doubleword_shift
     can handle.  See the comments there for details.  */
      if (double_shift_mask == 0
      || (shift_mask == BITS_PER_WORD - 1
          && double_shift_mask == BITS_PER_WORD * 2 - 1))
    {
      rtx_insn *insns;
      rtx into_target, outof_target;
      rtx into_input, outof_input;
      int left_shift, outof_word;

      /* If TARGET is the same as one of the operands, the REG_EQUAL note
         won't be accurate, so use a new target.  */
      if (target == 0
          || target == op0
          || target == op1
          || mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,target, op0)
          || mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,target, op1)
          || !mtcs_optabs_valid_multiword_target_p(self,target))
        target = mtcs_emit_gen_reg_rtx(mtcsEmit,int_mode);

      mtcs_emit_start_sequence (mtcsEmit);

      /* OUTOF_* is the word we are shifting bits away from, and
         INTO_* is the word that we are shifting bits towards, thus
         they differ depending on the direction of the shift and
         WORDS_BIG_ENDIAN.  */

      left_shift = binoptab == ashl_optab;
      outof_word = left_shift ^ ! WORDS_BIG_ENDIAN;

      outof_target = mtcs_rtl_operand_subword(mtcsRTL,target, outof_word, 1, int_mode);
      into_target = mtcs_rtl_operand_subword(mtcsRTL,target, 1 - outof_word, 1, int_mode);

      outof_input = mtcs_rtl_operand_subword_force(mtcsRTL,op0, outof_word, int_mode);
      into_input = mtcs_rtl_operand_subword_force(mtcsRTL,op0, 1 - outof_word, int_mode);

      if (expand_doubleword_shift(self,op1_mode, binoptab,
                       outof_input, into_input, op1,
                       outof_target, into_target,
                       unsignedp, next_methods, shift_mask))
        {
          insns = mtcs_rtl_data_get_insns(mtcsRtlData);
          mtcs_emit_end_sequence (mtcsEmit);

          mtcs_emit_emit_insn (mtcsEmit,insns);
          return target;
        }
      mtcs_emit_end_sequence (mtcsEmit);
    }
    }

  /* Synthesize double word rotates from single word shifts.  */
  if ((binoptab == rotl_optab || binoptab == rotr_optab)
      && mtcs_mode_is_int_mode (mtcsMode,mode, &int_mode)
      && CONST_INT_P (op1)
      && mtcs_mode_get_precision(mtcsMode,int_mode) == 2 * BITS_PER_WORD
      &&  mtcs_opinit_optab_handler(mtcsOpinit,ashl_optab, mtcsMode->word_mode) != CODE_FOR_nothing
      &&  mtcs_opinit_optab_handler(mtcsOpinit,lshr_optab, mtcsMode->word_mode) != CODE_FOR_nothing)
    {
      rtx_insn *insns;
      rtx into_target, outof_target;
      rtx into_input, outof_input;
      rtx inter;
      int shift_count, left_shift, outof_word;

      /* If TARGET is the same as one of the operands, the REG_EQUAL note
     won't be accurate, so use a new target. Do this also if target is not
     a REG, first because having a register instead may open optimization
     opportunities, and second because if target and op0 happen to be MEMs
     designating the same location, we would risk clobbering it too early
     in the code sequence we generate below.  */
      if (target == 0
      || target == op0
      || target == op1
      || !REG_P (target)
      || mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,target, op0)
      || mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,target, op1)
      || !mtcs_optabs_valid_multiword_target_p(self,target))
    target = mtcs_emit_gen_reg_rtx(mtcsEmit,int_mode);

      mtcs_emit_start_sequence (mtcsEmit);

      shift_count = INTVAL (op1);

      /* OUTOF_* is the word we are shifting bits away from, and
     INTO_* is the word that we are shifting bits towards, thus
     they differ depending on the direction of the shift and
     WORDS_BIG_ENDIAN.  */

      left_shift = (binoptab == rotl_optab);
      outof_word = left_shift ^ ! WORDS_BIG_ENDIAN;

      outof_target = mtcs_rtl_operand_subword(mtcsRTL,target, outof_word, 1, int_mode);
      into_target = mtcs_rtl_operand_subword(mtcsRTL,target, 1 - outof_word, 1, int_mode);

      outof_input = mtcs_rtl_operand_subword_force(mtcsRTL,op0, outof_word, int_mode);
      into_input = mtcs_rtl_operand_subword_force(mtcsRTL,op0, 1 - outof_word, int_mode);

      if (shift_count == BITS_PER_WORD)
    {
      /* This is just a word swap.  */
      mtcs_expr_emit_move_insn(mtcsExpr,outof_target, into_input);
      mtcs_expr_emit_move_insn(mtcsExpr,into_target, outof_input);
      inter = const0_rtx;
    }
      else
    {
      rtx into_temp1, into_temp2, outof_temp1, outof_temp2;
      HOST_WIDE_INT first_shift_count, second_shift_count;
      optab reverse_unsigned_shift, unsigned_shift;

      reverse_unsigned_shift = (left_shift ^ (shift_count < BITS_PER_WORD)
                    ? lshr_optab : ashl_optab);

      unsigned_shift = (left_shift ^ (shift_count < BITS_PER_WORD)
                ? ashl_optab : lshr_optab);

      if (shift_count > BITS_PER_WORD)
        {
          first_shift_count = shift_count - BITS_PER_WORD;
          second_shift_count = 2 * BITS_PER_WORD - shift_count;
        }
      else
        {
          first_shift_count = BITS_PER_WORD - shift_count;
          second_shift_count = shift_count;
        }
      rtx first_shift_count_rtx
        = mtcs_rtl_gen_int_shift_amount (mtcsRTL,mtcsMode->word_mode, first_shift_count);
      rtx second_shift_count_rtx
        = mtcs_rtl_gen_int_shift_amount (mtcsRTL,mtcsMode->word_mode, second_shift_count);

      into_temp1 = mtcs_optabs_expand_binop (self,mtcsMode->word_mode, unsigned_shift,
                     outof_input, first_shift_count_rtx,
                     NULL_RTX, unsignedp, next_methods);
      into_temp2 = mtcs_optabs_expand_binop (self,mtcsMode->word_mode, reverse_unsigned_shift,
                     into_input, second_shift_count_rtx,
                     NULL_RTX, unsignedp, next_methods);

      if (into_temp1 != 0 && into_temp2 != 0)
        inter = mtcs_optabs_expand_binop (self,mtcsMode->word_mode, ior_optab, into_temp1, into_temp2,
                  into_target, unsignedp, next_methods);
      else
        inter = 0;

      if (inter != 0 && inter != into_target)
        mtcs_expr_emit_move_insn(mtcsExpr,into_target, inter);

      outof_temp1 = mtcs_optabs_expand_binop (self,mtcsMode->word_mode, unsigned_shift,
                      into_input, first_shift_count_rtx,
                      NULL_RTX, unsignedp, next_methods);
      outof_temp2 = mtcs_optabs_expand_binop (self,mtcsMode->word_mode, reverse_unsigned_shift,
                      outof_input, second_shift_count_rtx,
                      NULL_RTX, unsignedp, next_methods);

      if (inter != 0 && outof_temp1 != 0 && outof_temp2 != 0)
        inter = mtcs_optabs_expand_binop (self,mtcsMode->word_mode, ior_optab,
                  outof_temp1, outof_temp2,
                  outof_target, unsignedp, next_methods);

      if (inter != 0 && inter != outof_target)
        mtcs_expr_emit_move_insn(mtcsExpr,outof_target, inter);
    }

      insns = mtcs_rtl_data_get_insns(mtcsRtlData);
      mtcs_emit_end_sequence (mtcsEmit);

      if (inter != 0)
    {
      mtcs_emit_emit_insn (mtcsEmit,insns);
      return target;
    }
    }

  /* These can be done a word at a time by propagating carries.  */
  if ((binoptab == add_optab || binoptab == sub_optab)
      && mtcs_mode_is_int_mode (mtcsMode,mode, &int_mode)
      && mtcs_mode_get_size(mtcsMode,int_mode) >= 2 * UNITS_PER_WORD
      &&  mtcs_opinit_optab_handler(mtcsOpinit,binoptab, mtcsMode->word_mode) != CODE_FOR_nothing){
      unsigned int i;
      optab otheroptab = binoptab == add_optab ? sub_optab : add_optab;
      const unsigned int nwords = mtcs_mode_get_bitsize(mtcsMode,int_mode) / BITS_PER_WORD;
      rtx carry_in = NULL_RTX, carry_out = NULL_RTX;
      rtx xop0, xop1, xtarget;

      /* We can handle either a 1 or -1 value for the carry.  If STORE_FLAG
     value is one of those, use it.  Otherwise, use 1 since it is the
     one easiest to get.  */
#if STORE_FLAG_VALUE == 1 || STORE_FLAG_VALUE == -1
      int normalizep = STORE_FLAG_VALUE;
#else
      int normalizep = 1;
#endif

      /* Prepare the operands.  */
      xop0 = mtcs_explow_force_reg(mtcsExplow,int_mode, op0);
      xop1 = mtcs_explow_force_reg(mtcsExplow,int_mode, op1);

      xtarget = mtcs_emit_gen_reg_rtx(mtcsEmit,int_mode);

      if (target == 0 || !REG_P (target) || !mtcs_optabs_valid_multiword_target_p(self,target))
          target = xtarget;

      /* Indicate for flow that the entire target reg is being set.  */
      if (REG_P (target))
          mtcs_emit_emit_clobber/*!emit_clobber*/(mtcsEmit,xtarget);

      /* Do the actual arithmetic.  */
      for (i = 0; i < nwords; i++){
          int index = (WORDS_BIG_ENDIAN ? nwords - i - 1 : i);
          rtx target_piece = mtcs_rtl_operand_subword(mtcsRTL,xtarget, index, 1, int_mode);
          rtx op0_piece = mtcs_rtl_operand_subword_force(mtcsRTL,xop0, index, int_mode);
          rtx op1_piece = mtcs_rtl_operand_subword_force(mtcsRTL,xop1, index, int_mode);
          rtx x;

          /* Main add/subtract of the input operands.  */
          x = mtcs_optabs_expand_binop (self,mtcsMode->word_mode, binoptab,
                    op0_piece, op1_piece,
                    target_piece, unsignedp, next_methods);
          if (x == 0)
            break;

          if (i + 1 < nwords){
              /* Store carry from main add/subtract.  */
              carry_out = mtcs_emit_gen_reg_rtx(mtcsEmit,mtcsMode->word_mode);
              carry_out = mtcs_expmed_emit_store_flag_force/*!emit_store_flag_force*/(mtcsExpmed,
                    carry_out,(binoptab == add_optab? LT : GT),x, op0_piece,mtcsMode->word_mode, 1, normalizep);
          }

          if (i > 0){
              rtx newx;

              /* Add/subtract previous carry to main result.  */
              newx = mtcs_optabs_expand_binop (self,mtcsMode->word_mode,normalizep == 1 ? binoptab : otheroptab,
                       x, carry_in,NULL_RTX, 1, next_methods);

              if (i + 1 < nwords){
                  /* Get out carry from adding/subtracting carry in.  */
                  rtx carry_tmp = mtcs_emit_gen_reg_rtx(mtcsEmit,mtcsMode->word_mode);
                  carry_tmp = mtcs_expmed_emit_store_flag_force/*!emit_store_flag_force*/(mtcsExpmed,carry_tmp,(binoptab == add_optab
                                      ? LT : GT),newx, x,mtcsMode->word_mode, 1, normalizep);

                  /* Logical-ior the two poss. carry together.  */
                  carry_out = mtcs_optabs_expand_binop (self,mtcsMode->word_mode, ior_optab,carry_out, carry_tmp,carry_out, 0, next_methods);
                  if (carry_out == 0)
                    break;
              }
              mtcs_expr_emit_move_insn(mtcsExpr,target_piece, newx);
          }else{
              if (x != target_piece)
                  mtcs_expr_emit_move_insn(mtcsExpr,target_piece, x);
          }

          carry_in = carry_out;
      }

      if (i == mtcs_mode_get_bitsize(mtcsMode,int_mode) / (unsigned) BITS_PER_WORD){
          if ( mtcs_opinit_optab_handler(mtcsOpinit,mov_optab, int_mode) != CODE_FOR_nothing
              || ! rtx_equal_p (target, xtarget)){
              rtx_insn *temp = mtcs_expr_emit_move_insn(mtcsExpr,target, xtarget);
              mtcs_rtl_set_dst_reg_note/*!set_dst_reg_note*/(mtcsRTL,temp, REG_EQUAL,gen_rtx_fmt_ee (optab_to_code (binoptab),
                            int_mode, copy_rtx (xop0),copy_rtx (xop1)),target);
          }else
            target = xtarget;

          return target;
      }else
          mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
  }

  /* Attempt to synthesize double word multiplies using a sequence of word
     mode multiplications.  We first attempt to generate a sequence using a
     more efficient unsigned widening multiply, and if that fails we then
     try using a signed widening multiply.  */

  if (binoptab == smul_optab  && mtcs_mode_is_int_mode (mtcsMode,mode, &int_mode) && mtcs_mode_get_size(mtcsMode,int_mode) == 2 * UNITS_PER_WORD
      &&  mtcs_opinit_optab_handler(mtcsOpinit,smul_optab, mtcsMode->word_mode) != CODE_FOR_nothing
      &&  mtcs_opinit_optab_handler(mtcsOpinit,add_optab, mtcsMode->word_mode) != CODE_FOR_nothing){
      rtx product = NULL_RTX;
      if (mtcs_opinit_convert_optab_handler(mtcsOpinit,umul_widen_optab, int_mode, mtcsMode->word_mode) != CODE_FOR_nothing){
          product = expand_doubleword_mult(self,int_mode, op0, op1, target,true, methods);
          if (!product)
            mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
      }

      if (product == NULL_RTX && (mtcs_opinit_convert_optab_handler(mtcsOpinit,smul_widen_optab, int_mode, mtcsMode->word_mode)!= CODE_FOR_nothing)){
          product = expand_doubleword_mult(self,int_mode, op0, op1, target,false, methods);
          if (!product)
            mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
      }

      if (product != NULL_RTX){
          if ( mtcs_opinit_optab_handler(mtcsOpinit,mov_optab, int_mode) != CODE_FOR_nothing){
              rtx_insn *move = mtcs_expr_emit_move_insn(mtcsExpr,target ? target : product,product);
              mtcs_rtl_set_dst_reg_note/*!set_dst_reg_note*/(mtcsRTL,move,REG_EQUAL,gen_rtx_fmt_ee (MULT, int_mode,copy_rtx (op0),copy_rtx (op1)),
                    target ? target : product);
          }
          return product;
      }
  }

  /* Attempt to synthetize double word modulo by constant divisor.  */
  if ((binoptab == umod_optab
       || binoptab == smod_optab
       || binoptab == udiv_optab
       || binoptab == sdiv_optab)
      && optimize
      && CONST_INT_P (op1)
      && mtcs_mode_is_int_mode(mtcsMode,mode, &int_mode)
      && mtcs_mode_get_size(mtcsMode,int_mode) == 2 * UNITS_PER_WORD
      &&  mtcs_opinit_optab_handler(mtcsOpinit,(binoptab == umod_optab || binoptab == udiv_optab)
            ? udivmod_optab : sdivmod_optab,int_mode) == CODE_FOR_nothing
      &&  mtcs_opinit_optab_handler(mtcsOpinit,and_optab, mtcsMode->word_mode) != CODE_FOR_nothing
      &&  mtcs_opinit_optab_handler(mtcsOpinit,add_optab, mtcsMode->word_mode) != CODE_FOR_nothing
      && optimize_insn_for_speed_p ()){
      rtx res = NULL_RTX;
      if ((binoptab == umod_optab || binoptab == smod_optab) && (INTVAL (op1) & 1) == 0)
          res = expand_doubleword_mod(self,int_mode, op0, op1,binoptab == umod_optab);
      else{
          rtx quot = mtcs_optabs_expand_doubleword_divmod(self,int_mode, op0, op1, &res,
                               binoptab == umod_optab|| binoptab == udiv_optab);
          if (quot == NULL_RTX)
            res = NULL_RTX;
          else if (binoptab == udiv_optab || binoptab == sdiv_optab)
            res = quot;
      }
      if (res != NULL_RTX){
          if ( mtcs_opinit_optab_handler(mtcsOpinit,mov_optab, int_mode) != CODE_FOR_nothing){
              rtx_insn *move = mtcs_expr_emit_move_insn(mtcsExpr,target ? target : res,res);
              mtcs_rtl_set_dst_reg_note/*!set_dst_reg_note*/(mtcsRTL,move, REG_EQUAL,gen_rtx_fmt_ee (optab_to_code (binoptab),
                            int_mode, copy_rtx (op0), op1),target ? target : res);
          }
          return res;
      }else
          mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
  }

  /* It can't be open-coded in this mode.
     Use a library call if one is available and caller says that's ok.  */

  libfunc = mtcs_libfuncs_optab_libfunc/*!optab_libfunc*/(mtcsLibfuncs,binoptab, mode);
  if (libfunc && (methods == OPTAB_LIB || methods == OPTAB_LIB_WIDEN)){
      rtx_insn *insns;
      rtx op1x = op1;
      machine_mode op1_mode = mode;
      rtx value;

      mtcs_emit_start_sequence (mtcsEmit);

      if (shift_optab_p (binoptab)){
          op1_mode = targetm.libgcc_shift_count_mode ();
          /* Specify unsigned here,
             since negative shift counts are meaningless.  */
          op1x = mtcs_expr_convert_to_mode(mtcsExpr,op1_mode, op1, 1);
      }

      if (GET_MODE (op0) != VOIDmode  && GET_MODE (op0) != mode)
          op0 = mtcs_expr_convert_to_mode(mtcsExpr,mode, op0, unsignedp);

      /* Pass 1 for NO_QUEUE so we don't lose any increments
     if the libcall is cse'd or moved.  */
      value = mtcs_calls_emit_library_call_value/*!emit_library_call_value*/(mtcsCalls,
            libfunc,NULL_RTX, LCT_CONST, mode,op0, mode, op1x, op1_mode);

      insns = mtcs_rtl_data_get_insns(mtcsRtlData);
      mtcs_emit_end_sequence (mtcsEmit);

      bool trapv = trapv_binoptab_p (binoptab);
      target = mtcs_emit_gen_reg_rtx(mtcsEmit,mode);
      emit_libcall_block_1 (self,insns, target, value,trapv ? NULL_RTX: gen_rtx_fmt_ee (optab_to_code (binoptab),
                          mode, op0, op1), trapv);

      return target;
  }

  mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
  /* It can't be done in this mode.  Can we do it in a wider mode?  */
  if (! (methods == OPTAB_WIDEN || methods == OPTAB_LIB_WIDEN || methods == OPTAB_MUST_WIDEN)){
      /* Caller says, don't even try.  */
      mtcs_rtl_data_delete_insns_since(mtcsRtlData,entry_last);
      return 0;
  }

  /* Compute the value of METHODS to pass to recursive calls.
     Don't allow widening to be tried recursively.  */

  methods = (methods == OPTAB_LIB_WIDEN ? OPTAB_LIB : OPTAB_DIRECT);
  /* Look for a wider mode of the same class for which it appears we can do
     the operation.  */
  if (CLASS_HAS_WIDER_MODES_P (mclass)){
      /* This code doesn't make sense for conversion optabs, since we
     wouldn't then want to extend the operands to be the same size
     as the result.  */
      gcc_assert (!convert_optab_p (binoptab));
      MTCS_FOR_EACH_WIDER_MODE (mtcsMode,wider_mode, mode){
          if ( mtcs_opinit_optab_handler(mtcsOpinit,binoptab, wider_mode)
              || (methods == OPTAB_LIB && mtcs_libfuncs_optab_libfunc/*!optab_libfunc*/(mtcsLibfuncs,binoptab, wider_mode))){
              rtx xop0 = op0, xop1 = op1;
              bool no_extend = false;

              /* For certain integer operations, we need not actually extend
             the narrow operands, as long as we will truncate
             the results to the same narrowness.  */

              if ((binoptab == ior_optab || binoptab == and_optab
               || binoptab == xor_optab
               || binoptab == add_optab || binoptab == sub_optab
               || binoptab == smul_optab || binoptab == ashl_optab)
              && mclass == MODE_INT)
                  no_extend = true;

              xop0 = widen_operand(self,xop0, wider_mode, mode, unsignedp, no_extend);

              /* The second operand of a shift must always be extended.  */
              xop1 = widen_operand(self,xop1, wider_mode, mode, unsignedp,no_extend && binoptab != ashl_optab);

              temp = mtcs_optabs_expand_binop (self,wider_mode, binoptab, xop0, xop1, NULL_RTX,unsignedp, methods);
              if (temp){
                  if (mclass != MODE_INT || !mtcs_mode_truly_noop_truncation_p(mtcsMode,mode, wider_mode)){
                      if (target == 0)
                          target = mtcs_emit_gen_reg_rtx(mtcsEmit,mode);
                      mtcs_expr_convert_move(mtcsExpr,target, temp, 0);
                      return target;
                  }else
                    return gen_lowpart (mode, temp);
              }else
                  mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
          }
      }
  }

  mtcs_rtl_data_delete_insns_since(mtcsRtlData,entry_last);
  return 0;
}

/* Find a widening optab even if it doesn't widen as much as we want.
   E.g. if from_mode is HImode, and to_mode is DImode, and there is no
   direct HI->SI insn, then return SI->DI, if that exists.  */
//原型 find_widening_optab_handler_and_mode optabs-query.h optabs-query.cc
enum insn_code mtcs_optabs_find_widening_optab_handler_and_mode (MtcsOptabs *self,optab op, machine_mode to_mode,
                      machine_mode from_mode, machine_mode *found_mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  machine_mode limit_mode = to_mode;
  if (mtcs_mode_is_a <scalar_int_mode> (mtcsMode,from_mode)){
      gcc_checking_assert (mtcs_mode_is_a <scalar_int_mode>(mtcsMode,to_mode)
              && known_lt (mtcs_mode_get_precision(mtcsMode,from_mode), mtcs_mode_get_precision(mtcsMode,to_mode)));
      /* The modes after FROM_MODE are all MODE_INT, so the only
     MODE_PARTIAL_INT mode we consider is FROM_MODE itself.
     If LIMIT_MODE is MODE_PARTIAL_INT, stop at the containing
     MODE_INT.  */
      if (mtcs_mode_get_class(mtcsMode,limit_mode) == MODE_PARTIAL_INT)
          limit_mode = mtcs_mode_get_wider(mtcsMode,limit_mode).require ();
  }else
    gcc_checking_assert (mtcs_mode_get_class(mtcsMode,from_mode) == mtcs_mode_get_class(mtcsMode,to_mode) && from_mode < to_mode);

  MTCS_FOR_EACH_MODE (mtcsMode,from_mode, from_mode, limit_mode){
      enum insn_code handler = mtcs_opinit_convert_optab_handler(mtcsOpinit,op, to_mode, from_mode);
      if (handler != CODE_FOR_nothing){
          if (found_mode)
            *found_mode = from_mode;
          return handler;
      }
  }
  return CODE_FOR_nothing;
}

//原型 find_widening_optab_handler optabs-query.h
//#define find_widening_optab_handler(A, B, C) \
  //find_widening_optab_handler_and_mode (A, B, C, NULL)
enum insn_code mtcs_optabs_find_widening_optab_handler (MtcsOptabs *self,optab op, machine_mode to_mode,machine_mode from_mode)
{
    return mtcs_optabs_find_widening_optab_handler_and_mode(self,op,to_mode,from_mode,NULL);
}

/* Generate an instruction whose insn-code is INSN_CODE,
   with two operands: an output TARGET and an input OP0.
   TARGET *must* be nonzero, and the output is always stored there.
   CODE is an rtx code such that (CODE OP0) is an rtx that describes
   the value that is stored into TARGET.  */
//原型 emit_unop_insn optabs.h optabs.cc
void mtcs_optabs_emit_unop_insn (MtcsOptabs *self,enum insn_code icode, rtx target, rtx op0, enum rtx_code code)
{
   n_debug("mtcsoptabs.c mtcs_optabs_emit_unop_insn 00 code:%d\n",code);
  bool ok = mtcs_optabs_maybe_emit_unop_insn (self,icode, target, op0, code);
  gcc_assert (ok);
}

/* Generate an instruction whose insn-code is INSN_CODE,
   with two operands: an output TARGET and an input OP0.
   TARGET *must* be nonzero, and the output is always stored there.
   CODE is an rtx code such that (CODE OP0) is an rtx that describes
   the value that is stored into TARGET.

   Return false if expansion failed.  */
//原型 maybe_emit_unop_insn optabs.h optabs.cc
bool mtcs_optabs_maybe_emit_unop_insn (MtcsOptabs *self,enum insn_code icode, rtx target, rtx op0,enum rtx_code code)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  class expand_operand ops[2];
  rtx_insn *pat;
  n_debug("mtcsoptabs.c mtcs_optabs_maybe_emit_unop_insn 00 pat:%p icode:%d code:%d %d %d\n",
        pat,icode,code,GET_MODE (target),GET_MODE (op0));
  create_output_operand (&ops[0], target, GET_MODE (target));
  create_input_operand (&ops[1], op0, GET_MODE (op0));
  pat = mtcs_optabs_maybe_gen_insn (self,icode, 2, ops);
  n_debug("mtcsoptabs.c mtcs_optabs_maybe_emit_unop_insn 11 pat:%p icode:%d code:%d %d %d\n",
        pat,icode,code,GET_MODE (target),GET_MODE (op0));

  if (!pat)
    return false;

  if (INSN_P (pat) && NEXT_INSN (pat) != NULL_RTX   && code != UNKNOWN){
     n_debug("mtcsoptabs.c mtcs_optabs_maybe_emit_unop_insn 22 pat:%p icode:%d code:%d %d %d\n",
           pat,icode,code,GET_MODE (target),GET_MODE (op0));
    add_equal_note(self,pat, ops[0].value, code, ops[1].value, NULL_RTX,GET_MODE (op0));
  }
  mtcs_print_rtl_single(stderr,pat);
  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,pat);

  if (ops[0].value != target){
     n_debug("mtcsoptabs.c mtcs_optabs_maybe_emit_unop_insn 33 pat:%p icode:%d code:%d %d %d\n",
           pat,icode,code,GET_MODE (target),GET_MODE (op0));

    mtcs_expr_emit_move_insn (mtcsExpr,target, ops[0].value);
  }

  return true;
}

/* Try to generate instruction ICODE, using operands [OPS, OPS + NOPS)
   as its operands.  Return the instruction pattern on success,
   and emit any necessary set-up code.  Return null and emit no
   code on failure.  */
//原型 maybe_gen_insn optabs.h optabs.cc
rtx_insn *mtcs_optabs_maybe_gen_insn (MtcsOptabs *self,enum insn_code icode, unsigned int nops,class expand_operand *ops)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOutput   *mtcsOutput=mtcs_target_get_output(mtcsTarget);
  gcc_assert (nops == (unsigned int) mtcsOutput->insn_data[(int) icode].n_generator_args);
  n_debug("mtcsoptabs.c mtcs_optabs_maybe_gen_insn 00 icode:%d nops:%d\n",icode,nops);
  if (!mtcs_optabs_maybe_legitimize_operands (self,icode, 0, nops, ops))
    return NULL;
  n_debug("mtcsoptabs.c mtcs_optabs_maybe_gen_insn 11 icode:%d nops:%d\n",icode,nops);

  switch (nops) {
    case 0:
      return MTCS_GEN_FCN (icode) ();
    case 1:
      return MTCS_GEN_FCN (icode) (ops[0].value);
    case 2:
      return MTCS_GEN_FCN (icode) (ops[0].value, ops[1].value);
    case 3:
      return MTCS_GEN_FCN (icode) (ops[0].value, ops[1].value, ops[2].value);
    case 4:
      return MTCS_GEN_FCN (icode) (ops[0].value, ops[1].value, ops[2].value,
                  ops[3].value);
    case 5:
      return MTCS_GEN_FCN (icode) (ops[0].value, ops[1].value, ops[2].value,
                  ops[3].value, ops[4].value);
    case 6:
      return MTCS_GEN_FCN (icode) (ops[0].value, ops[1].value, ops[2].value,
                  ops[3].value, ops[4].value, ops[5].value);
    case 7:
      return MTCS_GEN_FCN (icode) (ops[0].value, ops[1].value, ops[2].value,
                  ops[3].value, ops[4].value, ops[5].value,
                  ops[6].value);
    case 8:
      return MTCS_GEN_FCN (icode) (ops[0].value, ops[1].value, ops[2].value,
                  ops[3].value, ops[4].value, ops[5].value,
                  ops[6].value, ops[7].value);
    case 9:
      return MTCS_GEN_FCN (icode) (ops[0].value, ops[1].value, ops[2].value,
                  ops[3].value, ops[4].value, ops[5].value,
                  ops[6].value, ops[7].value, ops[8].value);
    case 10:
      return MTCS_GEN_FCN (icode) (ops[0].value, ops[1].value, ops[2].value,
                  ops[3].value, ops[4].value, ops[5].value,
                  ops[6].value, ops[7].value, ops[8].value,
                  ops[9].value);
    case 11:
      return MTCS_GEN_FCN (icode) (ops[0].value, ops[1].value, ops[2].value,
                  ops[3].value, ops[4].value, ops[5].value,
                  ops[6].value, ops[7].value, ops[8].value,
                  ops[9].value, ops[10].value);
  }
  gcc_unreachable ();
}

/* Try to make operands [OPS, OPS + NOPS) match operands [OPNO, OPNO + NOPS)
   of instruction ICODE.  Return true on success, leaving the new operand
   values in the OPS themselves.  Emit no code on failure.  */
//原型 maybe_legitimize_operands optabs.h optabs.cc
bool mtcs_optabs_maybe_legitimize_operands (MtcsOptabs *self,enum insn_code icode, unsigned int opno,
               unsigned int nops, class expand_operand *ops)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  rtx_insn *last = mtcs_rtl_data_get_last_insn (mtcsRtlData);
  rtx *orig_values = XALLOCAVEC (rtx, nops);
  for (unsigned int i = 0; i < nops; i++) {
      orig_values[i] = ops[i].value;
      n_debug("mtcsoptabs.c mtcs_optabs_maybe_legitimize_operands 00 nops:%d i:%d\n",nops,i);
      /* First try reusing the result of an earlier legitimization.
     This avoids duplicate rtl and ensures that tied operands
     remain tied.

     This search is linear, but NOPS is bounded at compile time
     to a small number (current a single digit).  */
      unsigned int j = 0;
      for (; j < i; ++j)
        if (can_reuse_operands_p (self,icode, opno + j, opno + i, &ops[j], &ops[i])
            && rtx_equal_p (orig_values[j], orig_values[i])
            && ops[j].value   && mtcs_optabs_insn_operand_matches (self,icode, opno + i, ops[j].value)){
           n_debug("mtcsoptabs.c mtcs_optabs_maybe_legitimize_operands 11 改变了 ops[i] i:%d j:%d\n",i,j);
            mtcs_print_rtl(stderr,ops[i].value );
            ops[i].value = copy_rtx (ops[j].value);
            n_debug("mtcsoptabs.c mtcs_optabs_maybe_legitimize_operands 22 改变了 ops[i] i:%d j:%d\n",i,j);
            mtcs_print_rtl(stderr,ops[i].value);
            break;
        }

      /* Otherwise try legitimizing the operand on its own.  */
      if (j == i && !maybe_legitimize_operand (self,icode, opno + i, &ops[i])){
         n_debug("mtcsoptabs.c mtcs_optabs_maybe_legitimize_operands 33 删除last i:%d j:%d\n",i,j);
          mtcs_print_rtl(stderr,last);
          mtcs_rtl_data_delete_insns_since(mtcsRtlData,last);
          return false;
      }
  }
  return true;
}


/* Return true if OPERAND is suitable for operand number OPNO of
   instruction ICODE.  */
//原型 insn_operand_matches optabs.h optabs.cc
bool mtcs_optabs_insn_operand_matches (MtcsOptabs *self,enum insn_code icode, unsigned int opno, rtx operand)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOutput   *mtcsOutput=mtcs_target_get_output(mtcsTarget);
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
  n_debug("mtcsoptabs.c mtcs_optabs_insn_operand_matches 00  icode:%d opno:%d operand:%p REGNO:%d\n",icode,opno,operand,REGNO(operand));
  n_debug("mtcsoptabs.c mtcs_optabs_insn_operand_matches 11 predicate:%p\n", mtcsOutput->insn_data[(int) icode].operand[opno].predicate);
  n_debug("mtcsoptabs.c mtcs_optabs_insn_operand_matches 22 mode:%d %s rtx mode:%d\n",mtcsOutput->insn_data[(int) icode].operand[opno].mode,
        mtcs_mode_get_name(mtcsMode,mtcsOutput->insn_data[(int) icode].operand[opno].mode),GET_MODE (operand));
  n_debug("mtcsoptabs.c mtcs_optabs_insn_operand_matches 33 name:%s\n",mtcsOutput->insn_data[(int) icode].name);
  mtcs_print_rtl(stderr,operand);

 /*! return (!mtcsOutput->insn_data[(int) icode].operand[opno].predicate
     || (mtcsOutput->insn_data[(int) icode].operand[opno].predicate(operand, mtcsOutput->insn_data[(int) icode].operand[opno].mode)));
   */
  //insn_data[..predicate原型是insn_operand_predicate_fn(recog.h)是两个参数，实际predicate是函数mtcs_insn_operand_predicate_fn
  //有三个参数
  insn_operand_predicate_fn fn=mtcsOutput->insn_data[(int) icode].operand[opno].predicate;
  if(!fn)
     return true;
  bool ret;
  if(mtcs_preds_is_common(mtcsPreds,(void*)fn)){
     mtcs_insn_operand_predicate_fn *fx=&fn;
     ret= (*fx)(mtcsPreds,operand, mtcsOutput->insn_data[(int) icode].operand[opno].mode);
  }else{
     mtcs_insn_operand_predicate_fn *fx=&fn;
     ret =fn(operand, mtcsOutput->insn_data[(int) icode].operand[opno].mode);
  }
  //mtcs_insn_operand_predicate_fn *fn=(mtcs_insn_operand_predicate_fn*)&mtcsOutput->insn_data[(int) icode].operand[opno].predicate;
 // bool ret= (!fn || (*fn)(mtcsPreds,operand, mtcsOutput->insn_data[(int) icode].operand[opno].mode));
  n_debug("mtcsoptabs.c mtcs_optabs_insn_operand_matches 44 fn:%p %s %d\n",fn,mtcsOutput->insn_data[(int) icode].name,ret);
  mtcs_print_rtl(stderr,operand);

  return ret;
}

/* Create a new vector value in VMODE with all elements set to OP.  The
   mode of OP must be the element mode of VMODE.  If OP is a constant,
   then the return value will be a constant.  */
//原型 expand_vector_broadcast optabs.h optabs.cc
rtx mtcs_optabs_expand_vector_broadcast (MtcsOptabs *self,machine_mode vmode, rtx op)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit  *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOutput  *mtcsOutput=mtcs_target_get_output(mtcsTarget);

  int n;
  rtvec vec;
  gcc_checking_assert (mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,vmode));
  if (valid_for_const_vector_p (vmode, op)) //valid_for_const_vector_p不需要移植 rtl.h emit-rtl.cc
    return mtcs_rtl_gen_const_vec_duplicate/*!gen_const_vec_duplicate*/ (mtcsRTL,vmode, op);

  insn_code icode = mtcs_opinit_optab_handler (mtcsOpinit,vec_duplicate_optab, vmode);
  if (icode != CODE_FOR_nothing){
      class expand_operand ops[2];
      create_output_operand (&ops[0], NULL_RTX, vmode);
      create_input_operand (&ops[1], op, GET_MODE (op));
      mtcs_optabs_expand_insn/*!expand_insn*/(self,icode, 2, ops);
      return ops[0].value;
  }

  if (!mtcs_mode_get_nunits(mtcsMode,vmode).is_constant (&n))
    return NULL;

  /* ??? If the target doesn't have a vec_init, then we have no easy way
     of performing this operation.  Most of this sort of generic support
     is hidden away in the vector lowering support in gimple.  */
  icode =  mtcs_opinit_convert_optab_handler (mtcsOpinit,vec_init_optab, vmode,mtcs_mode_get_inner (mtcsMode,vmode));
  if (icode == CODE_FOR_nothing)
    return NULL;

  vec = rtvec_alloc (n);
  for (int i = 0; i < n; ++i)
    RTVEC_ELT (vec, i) = op;
  rtx ret = mtcs_emit_gen_reg_rtx (mtcsEmit,vmode);
  mtcs_emit_emit_insn (mtcsEmit,MTCS_GEN_FCN (icode) (ret, gen_rtx_PARALLEL (vmode, vec)));

  return ret;
}

/* Try to emit instruction ICODE, using operands [OPS, OPS + NOPS)
   as its operands.  Return true on success and emit no code on failure.  */
//原型 maybe_expand_insn optabs.h optabs.cc
bool mtcs_optabs_maybe_expand_insn (MtcsOptabs *self,enum insn_code icode, unsigned int nops,class expand_operand *ops)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit  *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  rtx_insn *pat = mtcs_optabs_maybe_gen_insn (self,icode, nops, ops);
  if (pat){
      mtcs_emit_emit_insn (mtcsEmit,pat);
      return true;
  }
  return false;
}

/* Emit instruction ICODE, using operands [OPS, OPS + NOPS)
   as its operands.  */
//原型 expand_insn optabs.h optabs.cc
void mtcs_optabs_expand_insn (MtcsOptabs *self,enum insn_code icode, unsigned int nops,class expand_operand *ops)
{
  if (!mtcs_optabs_maybe_expand_insn(self,icode, nops, ops))
      gcc_unreachable ();
}

/* Generate code to compare X with Y so that the condition codes are
   set and to jump to LABEL if the condition is true.  If X is a
   constant and Y is not a constant, then the comparison is swapped to
   ensure that the comparison RTL has the canonical form.

   UNSIGNEDP nonzero says that X and Y are unsigned; this matters if they
   need to be widened.  UNSIGNEDP is also used to select the proper
   branch condition code.

   If X and Y have mode BLKmode, then SIZE specifies the size of both X and Y.

   MODE is the mode of the inputs (in case they are const_int).

   COMPARISON is the rtl operator to compare with (EQ, NE, GT, etc.).
   It will be potentially converted into an unsigned variant based on
   UNSIGNEDP to select a proper jump instruction.

   PROB is the probability of jumping to LABEL.  If the comparison is against
   zero then VAL contains the expression from which the non-zero RTL is
   derived.  */
//原型 emit_cmp_and_jump_insns optabs.h optabs.cc 重载函数 多一个 tree val参数
void mtcs_optabs_emit_cmp_and_jump_insns (MtcsOptabs *self,rtx x, rtx y, enum rtx_code comparison, rtx size,
             machine_mode mode, int unsignedp, tree val, rtx label,profile_probability prob)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsRtlanal   *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  rtx op0 = x, op1 = y;
  rtx test;

  /* Swap operands and condition to ensure canonical RTL.  */
  if (mtcs_rtlanal_swap_commutative_operands_p(mtcsRtlanal,x, y)
          && mtcs_optabs_can_compare_p/*!can_compare_p*/(self,swap_condition (comparison), mode, ccp_jump)){
      op0 = y, op1 = x;
      comparison = swap_condition (comparison);
  }

  /* If OP0 is still a constant, then both X and Y must be constants
     or the opposite comparison is not supported.  Force X into a register
     to create canonical RTL.  */
  if (CONSTANT_P (op0))
    op0 = mtcs_explow_force_reg (mtcsExplow,mode, op0);

  if (unsignedp)
    comparison = unsigned_condition (comparison);//unsigned_condition rtl.h jump.cc不用移植

  prepare_cmp_insn (self,op0, op1, comparison, size, unsignedp, OPTAB_LIB_WIDEN, &test, &mode);

  /* Check if we're comparing a truth type with 0, and if so check if
     the target supports tbranch.  */
  machine_mode tmode = mode;
  direct_optab optab;
  if (op1 == CONST0_RTX (GET_MODE (op1))
          && validate_test_and_branch (self,val, &test, &tmode,&optab) != CODE_FOR_nothing){
      emit_cmp_and_jump_insn_1 (self,test, tmode, label, optab, prob, true);
      return;
  }

  emit_cmp_and_jump_insn_1 (self,test, mode, label, cbranch_optab, prob, false);
}


/* Overloaded version of emit_cmp_and_jump_insns in which VAL is unknown.  */
//原型 emit_cmp_and_jump_insns optabs.h optabs.cc 重载函数
void mtcs_optabs_emit_cmp_and_jump_insns (MtcsOptabs *self,rtx x, rtx y, enum rtx_code comparison, rtx size,
             machine_mode mode, int unsignedp, rtx label, profile_probability prob)
{
    mtcs_optabs_emit_cmp_and_jump_insns (self,x, y, comparison, size, mode, unsignedp, NULL,label, prob);
}

/* True if we can perform a comparison of mode MODE straightforwardly.
   PURPOSE describes how this comparison will be used.  CODE is the rtx
   comparison code we will be using.

   ??? Actually, CODE is slightly weaker than that.  A target is still
   required to implement all of the normal bcc operations, but not
   required to implement all (or any) of the unordered bcc operations.  */
//原型 can_compare_p optabs.h optabs.cc
bool mtcs_optabs_can_compare_p (MtcsOptabs *self,enum rtx_code code, machine_mode mode,int /*enum can_compare_purpose编译通不过*/ purpose)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  rtx test;
  test = gen_rtx_fmt_ee (code, mode, const0_rtx, const0_rtx);
  do{
      enum insn_code icode;

      if (purpose == ccp_jump  && (icode = mtcs_opinit_optab_handler (mtcsOpinit,cbranch_optab, mode)) != CODE_FOR_nothing
          && mtcs_optabs_insn_operand_matches (self,icode, 0, test))
          return true;
      if (purpose == ccp_store_flag   && (icode = mtcs_opinit_optab_handler(mtcsOpinit,cstore_optab, mode)) != CODE_FOR_nothing
          && mtcs_optabs_insn_operand_matches (self,icode, 1, test))
          return true;
      if (purpose == ccp_cmov && mtcs_opinit_optab_handler(mtcsOpinit,cmov_optab, mode) != CODE_FOR_nothing)
          return true;

      mode = mtcs_mode_get_wider/*!GET_MODE_WIDER_MODE*/ (mtcsMode,mode).else_void ();
      mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,test, mode);
  }while (mode != VOIDmode);

  return false;
}


/* Return the insn code used to extend FROM_MODE to TO_MODE.
   UNSIGNEDP specifies zero-extension instead of sign-extension.  If
   no such operation exists, CODE_FOR_nothing will be returned.  */
//原型 can_extend_p optabs-query.h optabs-query.cc
enum insn_code mtcs_optabs_can_extend_p (MtcsOptabs *self,machine_mode to_mode, machine_mode from_mode, int unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  if (unsignedp < 0 && target_rtx_have_ptr_extend/*!targetm.have_ptr_extend*/(mtcsMachine->tmrtx))
    return mtcsMachine->tmrtx->code_for_ptr_extend/*!targetm.code_for_ptr_extend*/;

  convert_optab tab = unsignedp ? zext_optab : sext_optab;
  return mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,tab, to_mode, from_mode);
}

/* Wrapper around expand_binop which takes an rtx code to specify
   the operation to perform, not an optab pointer.  All other
   arguments are the same.  */
//原型 expand_simple_binop optabs.h optabs.cc
//原型 expand_simple_binop optabs.h optabs.cc 不支持enum optab_methods methods编译不通过改成int
rtx mtcs_optabs_expand_simple_binop(MtcsOptabs *self,machine_mode mode, enum rtx_code code, rtx op0,
             rtx op1, rtx target, int unsignedp, int optabMethods /*!enum optab_methods methods*/)
{
  enum optab_methods methods=(enum optab_methods)optabMethods;
  optab binop = code_to_optab (code);
  gcc_assert (binop);

  return mtcs_optabs_expand_binop (self,mode, binop, op0, op1, target, unsignedp, methods);
}

/* Before emitting an insn with code ICODE, make sure that X, which is going
   to be used for operand OPNUM of the insn, is converted from mode MODE to
   WIDER_MODE (UNSIGNEDP determines whether it is an unsigned conversion), and
   that it is accepted by the operand predicate.  Return the new value.  */
//原型 prepare_operand optabs.h optabs.cc
rtx mtcs_optabs_prepare_operand (MtcsOptabs *self,enum insn_code icode, rtx x, int opnum, machine_mode mode,
         machine_mode wider_mode, int unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);

  if (mode != wider_mode)
    x = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,wider_mode, mode, x, unsignedp);

  if (!mtcs_optabs_insn_operand_matches (self,icode, opnum, x)){
      machine_mode op_mode = mtcsOutput->insn_data[(int) icode].operand[opnum].mode;
      if (reload_completed)
          return NULL_RTX;
      if (GET_MODE (x) != op_mode && GET_MODE (x) != VOIDmode)
          return NULL_RTX;
      x = mtcs_explow_copy_to_mode_reg (mtcsExplow,op_mode, x);
  }

  return x;
}

//原型 emit_libcall_block optabs.h optabs.cc
void mtcs_optabs_emit_libcall_block (MtcsOptabs *self,rtx_insn *insns, rtx target, rtx result, rtx equiv)
{
  emit_libcall_block_1 (self,insns, target, result, equiv, false);
}

/* Wrapper around expand_unop which takes an rtx code to specify
   the operation to perform, not an optab pointer.  All other
   arguments are the same.  */
//原型 expand_simple_unop optabs.h optabs.cc
rtx mtcs_optabs_expand_simple_unop (MtcsOptabs *self,machine_mode mode, enum rtx_code code, rtx op0,
            rtx target, int unsignedp)
{
  optab unop = code_to_optab (code);
  gcc_assert (unop);

  return mtcs_optabs_expand_unop (self,mode, unop, op0, target, unsignedp);
}

/* Emit a conditional move instruction if the machine supports one for that
   condition and machine mode.

   OP0 and OP1 are the operands that should be compared using CODE.  CMODE is
   the mode to use should they be constants.  If it is VOIDmode, they cannot
   both be constants.

   OP2 should be stored in TARGET if the comparison is true, otherwise OP3
   should be stored there.  MODE is the mode to use should they be constants.
   If it is VOIDmode, they cannot both be constants.

   The result is either TARGET (perhaps modified) or NULL_RTX if the operation
   is not supported.  */
//原型 emit_conditional_move optabs.h optabs.cc 重载函数
rtx mtcs_optabs_emit_conditional_move(MtcsOptabs *self,rtx target, struct rtx_comparison comp,
               rtx op2, rtx op3,  machine_mode mode, int unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsRtlanal   *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

  rtx comparison;
  rtx_insn *last;
  enum insn_code icode;
  enum rtx_code reversed;
  /* If the two source operands are identical, that's just a move.  */
  if (rtx_equal_p (op2, op3)){
      if (!target)
          target = mtcs_emit_gen_reg_rtx(mtcsEmit,mode);
      mtcs_expr_emit_move_insn(mtcsExpr,target, op3);
      return target;
  }
  /* If one operand is constant, make it the second one.  Only do this
     if the other operand is not constant as well.  */
  if (mtcs_rtlanal_swap_commutative_operands_p(mtcsRtlanal,comp.op0, comp.op1)){
      std::swap (comp.op0, comp.op1);
      comp.code = swap_condition (comp.code);
  }

  /* get_condition will prefer to generate LT and GT even if the old
     comparison was against zero, so undo that canonicalization here since
     comparisons against zero are cheaper.  */

  if (comp.code == LT && comp.op1 == const1_rtx)
    comp.code = LE, comp.op1 = const0_rtx;
  else if (comp.code == GT && comp.op1 == constm1_rtx)
    comp.code = GE, comp.op1 = const0_rtx;

  if (comp.mode == VOIDmode)
    comp.mode = GET_MODE (comp.op0);

  enum rtx_code orig_code = comp.code;
  bool swapped = false;
  if (mtcs_rtlanal_swap_commutative_operands_p(mtcsRtlanal,op2, op3)
      && ((reversed =mtcs_dojump_reversed_comparison_code_parts(mtcsDojump,comp.code, comp.op0, comp.op1, NULL))!= UNKNOWN)){
      std::swap (op2, op3);
      comp.code = reversed;
      swapped = true;
  }

  if (mode == VOIDmode)
    mode = GET_MODE (op2);

  icode = mtcs_opinit_direct_optab_handler(mtcsOpinit,movcc_optab, mode);

  if (icode == CODE_FOR_nothing)
    return NULL_RTX;

  if (!target)
    target = mtcs_emit_gen_reg_rtx(mtcsEmit,mode);

  for (int pass = 0; ; pass++){
      comp.code = unsignedp ? unsigned_condition (comp.code) : comp.code;
      comparison =mtcs_simplify_rtx_gen_relational (mtcsSimplifyRtx,comp.code, VOIDmode,comp.mode, comp.op0, comp.op1);

      /* We can get const0_rtx or const_true_rtx in some circumstances.  Just
     punt and let the caller figure out how best to deal with this
     situation.  */
      if (COMPARISON_P (comparison)){
          saved_pending_stack_adjust save;
          save_pending_stack_adjust (&save);
          last = mtcs_rtl_data_get_last_insn (mtcsRtlData);
          mtcs_dojump_do_pending_stack_adjust(mtcsDojump);
          machine_mode cmpmode = comp.mode;
          rtx orig_op0 = XEXP (comparison, 0);
          rtx orig_op1 = XEXP (comparison, 1);
          rtx op2p = op2;
          rtx op3p = op3;
          /* If we are optimizing, force expensive constants into a register
             but preserve an eventual equality with op2/op3.  */
          if (CONSTANT_P (orig_op0) && optimize  && cmpmode == mode
                  && (mtcs_rtlanal_rtx_cost/*!rtx_cost*/(mtcsRtlanal,orig_op0, mode,
                          COMPARE, 0,optimize_insn_for_speed_p ()) > COSTS_N_INSNS (1))
                  && can_create_pseudo_p ()){
              if (rtx_equal_p (orig_op0, op2))
                  op2p = XEXP (comparison, 0) = force_reg (cmpmode, orig_op0);
              else if (rtx_equal_p (orig_op0, op3))
                  op3p = XEXP (comparison, 0) = force_reg (cmpmode, orig_op0);
          }
          if (CONSTANT_P (orig_op1) && optimize
              && cmpmode == mode  && (mtcs_rtlanal_rtx_cost/*!rtx_cost*/(mtcsRtlanal,orig_op1, mode,
                      COMPARE, 0, optimize_insn_for_speed_p ())
              > COSTS_N_INSNS (1)) && can_create_pseudo_p ()){
              if (rtx_equal_p (orig_op1, op2))
                  op2p = XEXP (comparison, 1) = force_reg (cmpmode, orig_op1);
              else if (rtx_equal_p (orig_op1, op3))
                  op3p = XEXP (comparison, 1) = force_reg (cmpmode, orig_op1);
          }
          prepare_cmp_insn (self,XEXP (comparison, 0), XEXP (comparison, 1),
                    GET_CODE (comparison), NULL_RTX, unsignedp,OPTAB_WIDEN, &comparison, &cmpmode);
          if (comparison){
               rtx res = emit_conditional_move_1 (self,target, comparison,op2p, op3p, mode);
               if (res != NULL_RTX)
                   return res;
          }
          mtcs_rtl_data_delete_insns_since (mtcsRtlData,last);
          restore_pending_stack_adjust (&save);
      }

      if (pass == 1)
          return NULL_RTX;

      /* If the preferred op2/op3 order is not usable, retry with other
     operand order, perhaps it will expand successfully.  */
      if (swapped)
          comp.code = orig_code;
      else if ((reversed = mtcs_dojump_reversed_comparison_code_parts(mtcsDojump,orig_code, comp.op0, comp.op1,NULL)) != UNKNOWN)
          comp.code = reversed;
      else
          return NULL_RTX;
      std::swap (op2, op3);
  }
}

/* Helper function that, in addition to COMPARISON, also tries
   the reversed REV_COMPARISON with swapped OP2 and OP3.  As opposed
   to when we pass the specific constituents of a comparison, no
   additional insns are emitted for it.  It might still be necessary
   to emit more than one insn for the final conditional move, though.  */
//原型 emit_conditional_move optabs.h optabs.cc 重载函数
rtx mtcs_optabs_emit_conditional_move(MtcsOptabs *self,rtx target, rtx comparison, rtx rev_comparison,rtx op2, rtx op3, machine_mode mode)
{
  rtx res = emit_conditional_move_1(self,target, comparison, op2, op3, mode);
  if (res != NULL_RTX)
    return res;
  return emit_conditional_move_1(self,target, rev_comparison, op3, op2, mode);
}


/* Similarly to the above function, but compute both quotient and remainder.
   Quotient can be computed from the remainder as:
   rem = op0 % op1;  // Handled using expand_doubleword_mod
   quot = (op0 - rem) * inv; // inv is multiplicative inverse of op1 modulo
                 // 2 * BITS_PER_WORD

   We can also handle cases where op1 is a multiple of power of two constant
   and constant handled by expand_doubleword_mod.
   op11 = 1 << __builtin_ctz (op1);
   op12 = op1 / op11;
   rem1 = op0 % op12;  // Handled using expand_doubleword_mod
   quot1 = (op0 - rem1) * inv; // inv is multiplicative inverse of op12 modulo
                   // 2 * BITS_PER_WORD
   rem = (quot1 % op11) * op12 + rem1;
   quot = quot1 / op11;  */
//原型 expand_doubleword_divmod optabs.h optabs.cc
rtx mtcs_optabs_expand_doubleword_divmod(MtcsOptabs *self,machine_mode mode, rtx op0, rtx op1, rtx *rem,bool unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);

  *rem = NULL_RTX;

  /* Negative dividend should have been optimized into positive,
     similarly modulo by 1 and modulo by power of two is optimized
     differently too.  */
  if (INTVAL (op1) <= 1 || pow2p_hwi (INTVAL (op1)))
    return NULL_RTX;

  rtx op11 = const1_rtx;
  rtx op12 = op1;
  if ((INTVAL (op1) & 1) == 0){
      int bit = ctz_hwi (INTVAL (op1));
      op11 = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,HOST_WIDE_INT_1 << bit);
      op12 = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,INTVAL (op1) >> bit);
  }

  rtx rem1 = expand_doubleword_mod(self,mode, op0, op12, unsignedp);
  if (rem1 == NULL_RTX)
    return NULL_RTX;

  int prec = 2 * BITS_PER_WORD;
  wide_int a = wide_int::from (INTVAL (op12), prec + 1, UNSIGNED);
  wide_int b = wi::shifted_mask (prec, 1, false, prec + 1);
  wide_int m = wide_int::from (wi::mod_inv (a, b), prec, UNSIGNED);
  rtx inv = mtcs_rtl_immed_wide_int_const(mtcsRTL,m, mode);

  rtx_insn *last = mtcs_rtl_data_get_last_insn(mtcsRtlData);
  rtx quot1 = mtcs_optabs_expand_simple_binop(self,mode, MINUS, op0, rem1, NULL_RTX, unsignedp, OPTAB_DIRECT);
  if (quot1 == NULL_RTX)
    return NULL_RTX;

  quot1 = mtcs_optabs_expand_simple_binop(self,mode, MULT, quot1, inv,NULL_RTX, unsignedp, OPTAB_DIRECT);
  if (quot1 == NULL_RTX)
    return NULL_RTX;

  if (op11 != const1_rtx){
      rtx rem2 = mtcs_expmed_expand_divmod(mtcsExpmed,1, TRUNC_MOD_EXPR, mode, quot1, op11, NULL_RTX, unsignedp, OPTAB_DIRECT);
      if (rem2 == NULL_RTX)
          return NULL_RTX;

      rem2 = mtcs_optabs_expand_simple_binop(self,mode, MULT, rem2, op12, NULL_RTX,unsignedp, OPTAB_DIRECT);
      if (rem2 == NULL_RTX)
          return NULL_RTX;

      rem2 = mtcs_optabs_expand_simple_binop(self,mode, PLUS, rem2, rem1, NULL_RTX,unsignedp, OPTAB_DIRECT);
      if (rem2 == NULL_RTX)
          return NULL_RTX;

      rtx quot2 = mtcs_expmed_expand_divmod(mtcsExpmed,0, TRUNC_DIV_EXPR, mode, quot1, op11,NULL_RTX, unsignedp, OPTAB_DIRECT);
      if (quot2 == NULL_RTX)
          return NULL_RTX;

      rem1 = rem2;
      quot1 = quot2;
  }

  /* Punt if we need any library calls.  */
  if (last)
    last = NEXT_INSN (last);
  else
    last = mtcs_rtl_data_get_insns(mtcsRtlData);
  for (; last; last = NEXT_INSN (last))
    if (CALL_P (last))
      return NULL_RTX;

  *rem = rem1;
  return quot1;
}

/* Like simplify_expand_binop, but always put the result in TARGET.
   Return true if the expansion succeeded.  */
//原型 force_expand_binop optabs.h optabs.cc
bool mtcs_optabs_force_expand_binop ( MtcsOptabs *self,machine_mode mode, optab binoptab,
            rtx op0, rtx op1, rtx target, int unsignedp, int /*!enum optab_methods编译通不过*/ methods)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  rtx x = mtcs_optabs_simplify_expand_binop(self,mode, binoptab, op0, op1,
                 target, unsignedp, methods);
  if (x == 0)
    return false;
  if (x != target)
      mtcs_expr_emit_move_insn(mtcsExpr,target, x);
  return true;
}

/* Like expand_binop, but return a constant rtx if the result can be
   calculated at compile time.  The arguments and return value are
   otherwise the same as for expand_binop.  */
//原型 simplify_expand_binop optabs.h optabs.cc
rtx mtcs_optabs_simplify_expand_binop (MtcsOptabs *self,machine_mode mode, optab binoptab,
               rtx op0, rtx op1, rtx target, int unsignedp,   int /*!enum optab_methods编译通不过*/ methods)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

  if (CONSTANT_P (op0) && CONSTANT_P (op1)){
      rtx x = mtcs_simplify_rtx_binary_operation (mtcsSimplifyRtx,optab_to_code (binoptab),mode, op0, op1);
      if (x)
          return x;
  }
  return mtcs_optabs_expand_binop(self,mode, binoptab, op0, op1, target, unsignedp, methods);
}

/* TARGET is a target of a multiword operation that we are going to
   implement as a series of word-mode operations.  Return true if
   TARGET is suitable for this purpose.  */
//原型 valid_multiword_target_p optabs.h optabs.cc
bool mtcs_optabs_valid_multiword_target_p (MtcsOptabs *self,rtx target)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  machine_mode mode;
  int i, size;
  mode = GET_MODE (target);
  if (!mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,mode).is_constant (&size))
    return false;
  for (i = 0; i < size; i += UNITS_PER_WORD)
    if (!mtcs_rtl_validate_subreg/*!validate_subreg*/(mtcsRTL,mtcsMode->word_mode, mode, target, i))
      return false;
  return true;
}



/* Check whether insv, extv or extzv pattern ICODE can be used for an
   insertion or extraction of type TYPE on a structure of mode MODE.
   Return true if so and fill in *INSN accordingly.  STRUCT_OP is the
   operand number of the structure (the first sign_extract or zero_extract
   operand) and FIELD_OP is the operand number of the field (the other
   side of the set from the sign_extract or zero_extract).  */
//原型 get_traditional_extraction_insn optabs-query.cc
static bool get_traditional_extraction_insn (MtcsOptabs *self,extraction_insn *insn,
                 enum extraction_type type,
                 machine_mode mode,
                 enum insn_code icode,
                 int struct_op, int field_op)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
  const struct insn_data_d *data = &mtcsOutput->insn_data[icode];

  machine_mode struct_mode = data->operand[struct_op].mode;
  if (struct_mode == VOIDmode)
    struct_mode = mtcsMode->word_mode;
  if (mode != struct_mode)
    return false;

  machine_mode field_mode = data->operand[field_op].mode;
  if (field_mode == VOIDmode)
    field_mode = mtcsMode->word_mode;

  machine_mode pos_mode = data->operand[struct_op + 2].mode;
  if (pos_mode == VOIDmode)
    pos_mode = mtcsMode->word_mode;

  insn->icode = icode;
  insn->field_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,field_mode);
  if (type == ET_unaligned_mem)
    insn->struct_mode = byte_mode;
  else if (struct_mode == mtcsMode->modes.M_BLKmode)
    insn->struct_mode = opt_scalar_int_mode ();
  else
    insn->struct_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,struct_mode);
  insn->pos_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,pos_mode);
  return true;
}

/* Return true if an instruction exists to perform an insertion or
   extraction (PATTERN says which) of type TYPE in mode MODE.
   Describe the instruction in *INSN if so.  */
//原型 get_extraction_insn optabs-query.cc
static bool get_extraction_insn (MtcsOptabs *self,extraction_insn *insn,
             enum extraction_pattern pattern,enum extraction_type type,machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  switch (pattern){
    case EP_insv:
      if (target_rtx_have_insv/*!targetm.have_insv*/(mtcsMachine->tmrtx)
      && get_traditional_extraction_insn (self,insn, type, mode,
                         mtcsMachine->tmrtx->code_for_insv/*!targetm.code_for_insv*/, 0, 3))
    return true;
      return get_optab_extraction_insn (self,insn, type, mode, insv_optab,
                    insvmisalign_optab, 2);

    case EP_extv:
      if (target_rtx_have_extv/*!targetm.have_extv*/(mtcsMachine->tmrtx)
      && get_traditional_extraction_insn (self,insn, type, mode,
            mtcsMachine->tmrtx->code_for_extv/*!targetm.code_for_extv*/, 1, 0))
    return true;
      return get_optab_extraction_insn (self,insn, type, mode, extv_optab,
                    extvmisalign_optab, 3);

    case EP_extzv:
      if (target_rtx_have_extzv/*!targetm.have_extzv*/(mtcsMachine->tmrtx)
      && get_traditional_extraction_insn(self,insn, type, mode,
            mtcsMachine->tmrtx->code_for_extzv/*!targetm.code_for_extzv*/, 1, 0))
    return true;
      return get_optab_extraction_insn(self,insn, type, mode, extzv_optab,
                    extzvmisalign_optab, 3);

    default:
      gcc_unreachable ();
  }
}

/* Return true if an instruction exists to access a field of mode
   FIELDMODE in a structure that has STRUCT_BITS significant bits.
   Describe the "best" such instruction in *INSN if so.  PATTERN and
   TYPE describe the type of insertion or extraction we want to perform.

   For an insertion, the number of significant structure bits includes
   all bits of the target.  For an extraction, it need only include the
   most significant bit of the field.  Larger widths are acceptable
   in both cases.  */
//原型 get_best_extraction_insn optabs-query.cc
static bool get_best_extraction_insn (MtcsOptabs *self,extraction_insn *insn,
              enum extraction_pattern pattern,
              enum extraction_type type,
              unsigned HOST_WIDE_INT struct_bits,
              machine_mode field_mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  opt_scalar_int_mode mode_iter;
  MTCS_FOR_EACH_MODE_FROM (mtcsMode,mode_iter,
          mtcs_mode_smallest_int_mode_for_size/*!smallest_int_mode_for_size*/(mtcsMode,struct_bits)){
      scalar_int_mode mode = mode_iter.require ();
      if (get_extraction_insn(self,insn, pattern, type, mode)){
          MTCS_FOR_EACH_MODE_FROM (mtcsMode,mode_iter, mode){
              mode = mode_iter.require ();
              if (maybe_gt (mtcs_mode_get_size(mtcsMode,mode), mtcs_mode_get_size(mtcsMode,field_mode))
                || mtcs_mode_truly_noop_truncation_p/*!TRULY_NOOP_TRUNCATION_MODES_P*/(mtcsMode,insn->field_mode,field_mode))
                  break;
              get_extraction_insn(self,insn, pattern, type, mode);
          }
          return true;
      }
  }
  return false;
}


/* Return true if an instruction exists to access a field of BITSIZE
   bits starting BITNUM bits into a memory structure.  Describe the
   "best" such instruction in *INSN if so.  PATTERN describes the type
   of insertion or extraction we want to perform and FIELDMODE is the
   natural mode of the extracted field.

   The instructions considered here only access bytes that overlap
   the bitfield; they do not touch any surrounding bytes.  */
//原型 get_best_mem_extraction_insn optabs-query.h optabs-query.cc
bool mtcs_optabs_get_best_mem_extraction_insn (MtcsOptabs *self,extraction_insn *insn,
                  enum extraction_pattern pattern,
                  HOST_WIDE_INT bitsize, HOST_WIDE_INT bitnum,
                  machine_mode field_mode)
{
  unsigned HOST_WIDE_INT struct_bits = (bitnum % BITS_PER_UNIT
                    + bitsize
                    + BITS_PER_UNIT - 1);
  struct_bits -= struct_bits % BITS_PER_UNIT;
  return get_best_extraction_insn(self,insn, pattern, ET_unaligned_mem,
                   struct_bits, field_mode);
}

/* Return true if an instruction exists to access a field of mode
   FIELDMODE in a register structure that has STRUCT_BITS significant bits.
   Describe the "best" such instruction in *INSN if so.  PATTERN describes
   the type of insertion or extraction we want to perform.

   For an insertion, the number of significant structure bits includes
   all bits of the target.  For an extraction, it need only include the
   most significant bit of the field.  Larger widths are acceptable
   in both cases.  */
//原型 get_best_reg_extraction_insn optabs-query.h optabs-query.cc
bool mtcs_optabs_get_best_reg_extraction_insn (MtcsOptabs *self,extraction_insn *insn,
                  enum extraction_pattern pattern,
                  unsigned HOST_WIDE_INT struct_bits,
                  machine_mode field_mode)
{
  return get_best_extraction_insn(self,insn, pattern, ET_reg, struct_bits,field_mode);
}


/* Generate code to convert FROM to fixed point and store in TO.  FROM
   must be floating point.  */
//原型 expand_fix optabs.h optabs.cc
void mtcs_optabs_expand_fix (MtcsOptabs *self,rtx to, rtx from, int unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
  MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);;
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);
  MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);

  enum insn_code icode;
  rtx target = to;
  machine_mode fmode, imode;
  opt_scalar_mode fmode_iter;
  bool must_trunc = false;

  /* We first try to find a pair of modes, one real and one integer, at
     least as wide as FROM and TO, respectively, in which we can open-code
     this conversion.  If the integer mode is wider than the mode of TO,
     we can do the conversion either signed or unsigned.  */

  MTCS_FOR_EACH_MODE_FROM (mtcsMode,fmode, GET_MODE (from))
    MTCS_FOR_EACH_MODE_FROM (mtcsMode,imode, GET_MODE (to)){
        int doing_unsigned = unsignedp;
        icode =mtcs_optabs_can_fix_p/*!can_fix_p*/(self,imode, fmode, unsignedp, &must_trunc);
        n_debug("mtcsoptabs.c expand_fix 00 imode:%d icode:%d to:%d,doing_unsigned:%d\n",imode,icode,GET_MODE (to),doing_unsigned);

        if (icode == CODE_FOR_nothing && imode != GET_MODE (to) && unsignedp){
           n_debug("mtcsoptabs.c expand_fix 11 imode:%d icode:%d to:%d,doing_unsigned:%d\n",imode,icode,GET_MODE (to),doing_unsigned);
          icode = mtcs_optabs_can_fix_p/*!can_fix_p*/(self,imode, fmode, 0, &must_trunc), doing_unsigned = 0;
        }
        if (icode != CODE_FOR_nothing){
            rtx_insn *last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
            rtx from1 = from;
            n_debug("mtcsoptabs.c expand_fix 22 imode:%d icode:%d fmode:%d,GET_MODE (from):%d\n",imode,icode,fmode,GET_MODE (from));

            if (fmode != GET_MODE (from)){
                if (mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,GET_MODE (from)) == &arm_bfloat_half_format
                    && mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,fmode) == &ieee_single_format)
                  /* The BF -> SF conversions can be just a shift, doesn't
                     need to handle sNANs.  */
                {
                   n_debug("mtcsoptabs.c expand_fix 33 imode:%d icode:%d fmode:%d,GET_MODE (from):%d\n",imode,icode,fmode,GET_MODE (from));
                    int save_flag_finite_math_only = flag_finite_math_only;
                    flag_finite_math_only = true;
                    from1 = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,fmode, from, 0);
                    flag_finite_math_only = save_flag_finite_math_only;
                }else{
                   n_debug("mtcsoptabs.c expand_fix 44 imode:%d icode:%d fmode:%d,GET_MODE (from):%d\n",imode,icode,fmode,GET_MODE (from));
                  from1 = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,fmode, from, 0);
                }
            }

            if (must_trunc){
               n_debug("mtcsoptabs.c expand_fix 55 imode:%d icode:%d fmode:%d,GET_MODE (from):%d\n",imode,icode,fmode,GET_MODE (from));
                rtx temp = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (from1));
                from1 = mtcs_optabs_expand_unop/*!expand_unop*/(self,GET_MODE (from1), ftrunc_optab, from1,temp, 0);
            }

            if (imode != GET_MODE (to))
              target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,imode);

            if (mtcs_optabs_maybe_emit_unop_insn/*!maybe_emit_unop_insn*/(self,
                    icode, target, from1,doing_unsigned ? UNSIGNED_FIX : FIX)){
               n_debug("mtcsoptabs.c expand_fix 66 imode:%d to:%d target != to:%d\n",imode,GET_MODE (to),target != to);

                if (target != to)
                  mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,to, target, unsignedp);
                return;
            }
            mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
        }
    }

  /* For an unsigned conversion, there is one more way to do it.
     If we have a signed conversion, we generate code that compares
     the real value to the largest representable positive number.  If if
     is smaller, the conversion is done normally.  Otherwise, subtract
     one plus the highest signed number, convert, and add it back.

     We only need to check all real modes, since we know we didn't find
     anything with a wider integer mode.

     This code used to extend FP value into mode wider than the destination.
     This is needed for decimal float modes which cannot accurately
     represent one plus the highest signed number of the same size, but
     not for binary modes.  Consider, for instance conversion from SFmode
     into DImode.

     The hot path through the code is dealing with inputs smaller than 2^63
     and doing just the conversion, so there is no bits to lose.

     In the other path we know the value is positive in the range 2^63..2^64-1
     inclusive.  (as for other input overflow happens and result is undefined)
     So we know that the most important bit set in mantissa corresponds to
     2^63.  The subtraction of 2^63 should not generate any rounding as it
     simply clears out that bit.  The rest is trivial.  */

  scalar_int_mode to_mode;
  if (unsignedp  && mtcs_mode_is_a <scalar_int_mode>(mtcsMode,GET_MODE (to), &to_mode)
      && mtcs_mode_is_hwi_computable_p/*!HWI_COMPUTABLE_MODE_P*/(mtcsMode,to_mode))
      MTCS_FOR_EACH_MODE_FROM (mtcsMode,fmode_iter, mtcs_mode_as_a <scalar_mode> (mtcsMode,GET_MODE (from))){
          scalar_mode fmode = fmode_iter.require ();
          if (CODE_FOR_nothing !=mtcs_optabs_can_fix_p/*!can_fix_p*/(self,to_mode, fmode,0, &must_trunc)
            && (!mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,fmode)
                || (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,fmode) >
          mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,to_mode))))
          {
                int bitsize;
                REAL_VALUE_TYPE offset;
                rtx limit;
                rtx_code_label *lab1, *lab2;
                rtx_insn *insn;

                bitsize =mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,to_mode);
                real_2expN (&offset, bitsize - 1, fmode);
                limit =mtcs_rtl_const_double_from_real_value/*!const_double_from_real_value*/(mtcsRTL,offset, fmode);
                lab1 =mtcs_rtl_gen_label_rtx(mtcsRTL);
                lab2 =mtcs_rtl_gen_label_rtx(mtcsRTL);
                if (fmode != GET_MODE (from)){
                    if (mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,GET_MODE (from))
                      == &arm_bfloat_half_format
                      && mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,fmode) == &ieee_single_format)
                    /* The BF -> SF conversions can be just a shift, doesn't
                    need to handle sNANs.  */
                    {
                        int save_flag_finite_math_only = flag_finite_math_only;
                        flag_finite_math_only = true;
                        from = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,fmode, from, 0);
                        flag_finite_math_only = save_flag_finite_math_only;
                    }else
                        from = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,fmode, from, 0);
                }

                /* See if we need to do the subtraction.  */
                mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
                mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(self,from, limit, GE, NULL_RTX,
                        GET_MODE (from), 0, lab1);

                /* If not, do the signed "fix" and branch around fixup code.  */
                mtcs_optabs_expand_fix/*!expand_fix*/(self,to, from, 0);
                mtcs_emit_emit_jump_insn/*!emit_jump_insn*/(mtcsEmit,
                      target_rtx_gen_jump/*!targetm.gen_jump*/(mtcsMachine->tmrtx,lab2));
                mtcs_emit_emit_barrier/*!emit_barrier*/(mtcsEmit);

                /* Otherwise, subtract 2**(N-1), convert to signed number,
                then add 2**(N-1).  Do the addition using XOR since this
                will often generate better code.  */
                mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,lab1);
                target = mtcs_optabs_expand_binop/*!expand_binop*/(self,GET_MODE (from), sub_optab, from, limit,
                        NULL_RTX, 0, OPTAB_LIB_WIDEN);
                mtcs_optabs_expand_fix/*!expand_fix*/(self,to, target, 0);
                target = mtcs_optabs_expand_binop/*!expand_binop*/(self,to_mode, xor_optab, to,
                mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,HOST_WIDE_INT_1 << (bitsize - 1),to_mode),to, 1, OPTAB_LIB_WIDEN);

                if (target != to)
                    mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,to, target);

                mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,lab2);

                if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,mov_optab, to_mode) != CODE_FOR_nothing){
                    /* Make a place for a REG_NOTE and add it.  */
                    insn = mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,to, to);
                    mtcs_rtl_set_dst_reg_note/*!set_dst_reg_note*/(mtcsRTL,
                          insn, REG_EQUAL,gen_rtx_fmt_e (UNSIGNED_FIX, to_mode,copy_rtx (from)),to);
                }
                return;
          }
      }

#ifdef HAVE_SFmode
  if (mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,GET_MODE (from)) == &arm_bfloat_half_format
      && mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,mtcsMode->modes.M_SFmode) == &ieee_single_format)
    /* We don't have BF -> TI library functions, use BF -> SF -> TI
       instead but the BF -> SF conversion can be just a shift, doesn't
       need to handle sNANs.  */
  {
      int save_flag_finite_math_only = flag_finite_math_only;
      flag_finite_math_only = true;
      from = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,mtcsMode->modes.M_SFmode, from, 0);
      flag_finite_math_only = save_flag_finite_math_only;
      mtcs_optabs_expand_fix/*!expand_fix*/(self,to, from, unsignedp);
      return;
  }
#endif

  /* We can't do it with an insn, so use a library call.  But first ensure
     that the mode of TO is at least as wide as SImode, since those are the
     only library calls we know about.  */

  if (mtcs_mode_is_narrower_int_mode/*!is_narrower_int_mode*/(mtcsMode,GET_MODE (to), mtcsMode->modes.M_SImode)){
      target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mtcsMode->modes.M_SImode);
      mtcs_optabs_expand_fix/*!expand_fix*/(self,target, from, unsignedp);
  }else{
      rtx_insn *insns;
      rtx value;
      rtx libfunc;
      convert_optab tab = unsignedp ? ufix_optab : sfix_optab;
      libfunc =mtcs_libfuncs_convert_optab_libfunc/*!convert_optab_libfunc*/(mtcsLibfuncs,tab, GET_MODE (to), GET_MODE (from));
      gcc_assert (libfunc);

      mtcs_emit_start_sequence(mtcsEmit);

      value = mtcs_calls_emit_library_call_value/*!emit_library_call_value*/(mtcsCalls,libfunc, NULL_RTX, LCT_CONST,
                       GET_MODE (to), from, GET_MODE (from));
      insns = mtcs_rtl_data_get_insns (mtcsRtlData);
      mtcs_emit_end_sequence (mtcsEmit);

      mtcs_optabs_emit_libcall_block/*!emit_libcall_block*/(self,insns, target, value,
              gen_rtx_fmt_e (unsignedp ? UNSIGNED_FIX : FIX, GET_MODE (to), from));
  }

  if (target != to){
      if (GET_MODE (to) == GET_MODE (target))
          mtcs_expr_emit_move_insn(mtcsExpr,to, target);
      else
        mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,to, target, 0);
  }
}

/* Return the insn code to convert floating-point mode FLTMODE to fixed-point
   mode FIXMODE, or CODE_FOR_nothing if no such instruction exists.
   UNSIGNEDP specifies whether FIXMODE is unsigned.

   On a successful return, set *TRUNCP_PTR to true if it is necessary to
   output an explicit FTRUNC before the instruction.  */
//原型 can_fix_p optabs-query.h optabs-query.cc
enum insn_code mtcs_optabs_can_fix_p (MtcsOptabs *self,machine_mode fixmode, machine_mode fltmode,
       int unsignedp, bool *truncp_ptr)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);

  convert_optab tab;
  enum insn_code icode;
  tab = unsignedp ? ufixtrunc_optab : sfixtrunc_optab;
  icode = mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,tab, fixmode, fltmode);
  if (icode != CODE_FOR_nothing){
      *truncp_ptr = false;
      return icode;
  }
  /* FIXME: This requires a port to define both FIX and FTRUNC pattern
     for this to work.  We need to rework the fix* and ftrunc* patterns
     and documentation.  */
  tab = unsignedp ? ufix_optab : sfix_optab;
  icode = mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,tab, fixmode, fltmode);
  if (icode != CODE_FOR_nothing
          && mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,ftrunc_optab, fltmode) != CODE_FOR_nothing){
      *truncp_ptr = true;
      return icode;
  }
  return CODE_FOR_nothing;
}

/* Return the insn code to convert fixed-point mode FIXMODE to floating-point
   mode FLTMODE, or CODE_FOR_nothing if no such instruction exists.
   UNSIGNEDP specifies whether FIXMODE is unsigned.  */
//原型 can_float_p optabs-query.h optabs-query.cc
enum insn_code mtcs_optabs_can_float_p (MtcsOptabs *self,machine_mode fltmode, machine_mode fixmode,int unsignedp)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
    convert_optab tab = unsignedp ? ufloat_optab : sfloat_optab;
    return mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,tab, fltmode, fixmode);
}

/* Generate code to convert FROM to floating point
   and store in TO.  FROM must be fixed point and not VOIDmode.
   UNSIGNEDP nonzero means regard FROM as unsigned.
   Normally this is done by correcting the final value
   if it is negative.  */
//原型 expand_float optabs.h optabs.cc
void mtcs_optabs_expand_float (MtcsOptabs *self,rtx to, rtx from, int unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
  MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);;
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsReg  *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpmed   *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  enum insn_code icode;
  rtx target = to;
  scalar_mode from_mode, to_mode;
  machine_mode fmode, imode;
  bool can_do_signed = false;

  /* Crash now, because we won't be able to decide which mode to use.  */
  gcc_assert (GET_MODE (from) != VOIDmode);

  /* Look for an insn to do the conversion.  Do it in the specified
     modes if possible; otherwise convert either input, output or both to
     wider mode.  If the integer mode is wider than the mode of FROM,
     we can do the conversion signed even if the input is unsigned.  */

  MTCS_FOR_EACH_MODE_FROM (mtcsMode,fmode, GET_MODE (to))
      MTCS_FOR_EACH_MODE_FROM (mtcsMode,imode, GET_MODE (from)){
            int doing_unsigned = unsignedp;
            //不能直接调用significand_size(mode) real.h 中 format_helper 调用 REAL_MODE_FORMAT
            const struct real_format *rf=fmode==VOIDmode?0:mtcs_mode_get_real_format (mtcsMode,fmode);
            format_helper help(rf);
            n_debug("mtcsoptabs.c expand_float 00 to:%d from:%d fmode:%d imode:%d\n",GET_MODE (to),GET_MODE (from),fmode,imode);
            if (fmode != GET_MODE (to) && (significand_size(help/*!fmode*/)
                          < mtcs_mode_get_precision/*!GET_MODE_UNIT_PRECISION*/(mtcsMode,GET_MODE (from)))){
               n_debug("mtcsoptabs.c expand_float 100aa continue to:%d from:%d\n",GET_MODE (to),GET_MODE (from));
               continue;
            }

            icode =mtcs_optabs_can_float_p/*!can_float_p*/(self,fmode, imode, unsignedp);
            if (icode == CODE_FOR_nothing && unsignedp){
                enum insn_code scode = mtcs_optabs_can_float_p/*!can_float_p*/(self,fmode, imode, 0);
                if (scode != CODE_FOR_nothing)
                  can_do_signed = true;
                if (imode != GET_MODE (from))
                  icode = scode, doing_unsigned = 0;
            }
            if (icode != CODE_FOR_nothing){
               n_debug("mtcsoptabs.c  expand_float 11 to:%d from:%d imode:%d icode:%d\n",GET_MODE (to),GET_MODE (from),imode,icode);

                if (imode != GET_MODE (from))
                 //  n_debug("mtcsoptabs.c  expand_float 11aa\n");
                  from = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,imode, from, unsignedp);

                if (fmode != GET_MODE (to))
                   //n_debug("mtcsoptabs.c  expand_float 11bb\n");

                  target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,fmode);

                mtcs_optabs_emit_unop_insn/*!emit_unop_insn*/(self,icode, target, from,doing_unsigned ? UNSIGNED_FLOAT : FLOAT);
                if (target != to)
                  // n_debug("mtcsoptabs.c  expand_float 11cc\n");

                    mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,to, target, 0);

                n_debug("mtcsoptabs.c mtcs_optabs_expand_float 22\n");
                return;
            }
      }

  /* Unsigned integer, and no way to convert directly.  Convert as signed,
     then unconditionally adjust the result.  */
  if (unsignedp  && can_do_signed  && mtcs_mode_is_a <scalar_mode>(mtcsMode,GET_MODE (to), &to_mode)
          && mtcs_mode_is_a <scalar_mode>(mtcsMode,GET_MODE (from), &from_mode)){
      opt_scalar_mode fmode_iter;
      rtx_code_label *label =mtcs_rtl_gen_label_rtx(mtcsRTL);
      rtx temp;
      REAL_VALUE_TYPE offset;
      /* Look for a usable floating mode FMODE wider than the source and at
     least as wide as the target.  Using FMODE will avoid rounding woes
     with unsigned values greater than the signed maximum value.  */
      MTCS_FOR_EACH_MODE_FROM (mtcsMode,fmode_iter, to_mode){
          scalar_mode fmode = fmode_iter.require ();
          if (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,from_mode) < mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,fmode)
              && mtcs_optabs_can_float_p/*!can_float_p*/(self,fmode, from_mode, 0) != CODE_FOR_nothing)
            break;
      }

      if (!fmode_iter.exists (&fmode)){
          /* There is no such mode.  Pretend the target is wide enough.  */
          fmode = to_mode;
          const struct real_format *rf=fmode==VOIDmode?0:mtcs_mode_get_real_format (mtcsMode,fmode);
          format_helper help(rf);
          /* Avoid double-rounding when TO is narrower than FROM.  */
          if ((significand_size (help/*!fmode*/) + 1) < mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,from_mode)){
              rtx temp1;
              rtx_code_label *neglabel =mtcs_rtl_gen_label_rtx(mtcsRTL);

              /* Don't use TARGET if it isn't a register, is a hard register,
             or is the wrong mode.  */
              if (!REG_P (target) || REGNO (target) < mtcs_reg_get_first_pseudo_register(mtcsReg)/*!FIRST_PSEUDO_REGISTER*/
                      || GET_MODE (target) != fmode)
                  target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,fmode);

              imode = from_mode;
              mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);

              /* Test whether the sign bit is set.  */
              mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(self,from, const0_rtx, LT, NULL_RTX, imode,
                           0, neglabel);

              /* The sign bit is not set.  Convert as signed.  */
              mtcs_optabs_expand_float/*!expand_float*/(self,target, from, 0);
              mtcs_emit_emit_jump_insn/*!emit_jump_insn*/(mtcsEmit,
                    target_rtx_gen_jump/*!targetm.gen_jump*/(mtcsMachine->tmrtx,label));
              mtcs_emit_emit_barrier/*!emit_barrier*/(mtcsEmit);

              /* The sign bit is set.
             Convert to a usable (positive signed) value by shifting right
             one bit, while remembering if a nonzero bit was shifted
             out; i.e., compute  (from & 1) | (from >> 1).  */

              mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,neglabel);
              temp =mtcs_optabs_expand_binop/*!expand_binop*/(self,imode, and_optab, from, const1_rtx,
                       NULL_RTX, 1, OPTAB_LIB_WIDEN);
              temp1 =  mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, imode, from, 1, NULL_RTX, 1);
              temp = mtcs_optabs_expand_binop/*!expand_binop*/(self,imode, ior_optab, temp, temp1, temp, 1,
                       OPTAB_LIB_WIDEN);
              mtcs_optabs_expand_float/*!expand_float*/(self,target, temp, 0);

              /* Multiply by 2 to undo the shift above.  */
              temp = mtcs_optabs_expand_binop/*!expand_binop*/(self,fmode, add_optab, target, target,
                       target, 0, OPTAB_LIB_WIDEN);
              if (temp != target)
                  mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, temp);

              mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
              mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
              n_debug("mtcsoptabs.c mtcs_optabs_expand_float 33\n");

              goto done;
          }
      }
      /* If we are about to do some arithmetic to correct for an
     unsigned operand, do it in a pseudo-register.  */
      if (to_mode != fmode
              || !REG_P (to) || REGNO (to) < mtcs_reg_get_first_pseudo_register(mtcsReg)/*!FIRST_PSEUDO_REGISTER*/)
          target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,fmode);
      /* Convert as signed integer to floating.  */
      mtcs_optabs_expand_float/*!expand_float*/(self,target, from, 0);
      /* If FROM is negative (and therefore TO is negative),
     correct its value by 2**bitwidth.  */
      mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
      mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(self,from, const0_rtx, GE, NULL_RTX, from_mode,
                   0, label);
      real_2expN (&offset, mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,from_mode), fmode);
      temp = mtcs_optabs_expand_binop/*!expand_binop*/(self,fmode, add_optab, target,
              mtcs_rtl_const_double_from_real_value/*!const_double_from_real_value*/(mtcsRTL,offset, fmode),target, 0, OPTAB_LIB_WIDEN);
      if (temp != target)
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, temp);

      mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
      n_debug("mtcsoptabs.c mtcs_optabs_expand_float 44\n");

      goto done;
  }

  /* No hardware instruction available; call a library routine.  */
  {
      rtx libfunc;
      rtx_insn *insns;
      rtx value;
      convert_optab tab = unsignedp ? ufloat_optab : sfloat_optab;
      n_debug("mtcsoptabs.c mtcs_optabs_expand_float 33\n");

      if (mtcs_mode_is_narrower_int_mode/*!is_narrower_int_mode*/(mtcsMode,GET_MODE (from), mtcsMode->modes.M_SImode))
          from =  mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,mtcsMode->modes.M_SImode, from, unsignedp);
      libfunc = mtcs_libfuncs_convert_optab_libfunc/*!convert_optab_libfunc*/(mtcsLibfuncs,tab, GET_MODE (to), GET_MODE (from));
      gcc_assert (libfunc);
      mtcs_emit_start_sequence(mtcsEmit);
      value = mtcs_calls_emit_library_call_value/*!emit_library_call_value*/(mtcsCalls,libfunc, NULL_RTX, LCT_CONST,
                       GET_MODE (to), from, GET_MODE (from));
      insns = mtcs_rtl_data_get_insns (mtcsRtlData);
      mtcs_emit_end_sequence(mtcsEmit);
      mtcs_optabs_emit_libcall_block/*!emit_libcall_block*/(self,insns, target, value,
              gen_rtx_fmt_e (unsignedp ? UNSIGNED_FLOAT : FLOAT,GET_MODE (to), from));
  }

done:
  /* Copy result to requested destination
     if we have been computing in a temp location.  */
  if (target != to){
      if (GET_MODE (target) == GET_MODE (to))
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,to, target);
      else
          mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,to, target, 0);
  }
}

/* These functions attempt to generate an insn body, rather than
   emitting the insn, but if the gen function already emits them, we
   make no attempt to turn them back into naked patterns.  */

/* Generate and return an insn body to add Y to X.  */
//原型 gen_add2_insn optabs.h optabs.cc
rtx_insn *mtcs_optabs_gen_add2_insn (MtcsOptabs *self,rtx x, rtx y)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);

  enum insn_code icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,add_optab, GET_MODE (x));
  gcc_assert (mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(self,icode, 0, x));
  gcc_assert (mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(self,icode, 1, x));
  gcc_assert (mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(self,icode, 2, y));
  return MTCS_GEN_FCN/*!GEN_FCN*/(icode) (x, x, y);
}



/* Use the current target and options to initialize
   TREE_OPTIMIZATION_OPTABS (OPTNODE).  */
//原型 init_tree_optimization_optabs optabs-tree.h optabs-tree.cc
void mtcs_optabs_init_tree_optimization_optabs (MtcsOptabs *self,tree optnode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);

  /* Quick exit if we have already computed optabs for this target.  */
  if (TREE_OPTIMIZATION_BASE_OPTABS (optnode) == mtcsOpinit->thisTargetOptabs/*!this_target_optabs*/)
    return;

  /* Forget any previous information and set up for the current target.  */
  TREE_OPTIMIZATION_BASE_OPTABS (optnode) = mtcsOpinit->thisTargetOptabs/*!this_target_optabs*/;
  struct target_optabs *tmp_optabs = (struct target_optabs *) TREE_OPTIMIZATION_OPTABS (optnode);
  if (tmp_optabs)
    memset (tmp_optabs, 0, sizeof (struct target_optabs));
  else
    tmp_optabs = ggc_cleared_alloc<target_optabs> ();

  /* Generate a new set of optabs into tmp_optabs.  */
  mtcs_opinit_init_all_optabs/*!init_all_optabs*/(mtcsOpinit,tmp_optabs);

  /* If the optabs changed, record it.  */
  if (memcmp (tmp_optabs, mtcsOpinit->thisTargetOptabs/*!this_target_optabs*/, sizeof (struct target_optabs)))
    TREE_OPTIMIZATION_OPTABS (optnode) = tmp_optabs;
  else{
      TREE_OPTIMIZATION_OPTABS (optnode) = NULL;
      ggc_free (tmp_optabs);
  }
}

/* Generate the body of an insn to extend Y (with mode MFROM)
   into X (with mode MTO).  Do zero-extension if UNSIGNEDP is nonzero.  */
//原型 gen_extend_insn optabs.h optabs.cc
rtx_insn *mtcs_optabs_gen_extend_insn (MtcsOptabs *self,rtx x, rtx y, machine_mode mto,
         machine_mode mfrom, int unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);

  enum insn_code icode = mtcs_optabs_can_extend_p/*!can_extend_p*/(self,mto, mfrom, unsignedp);
  return MTCS_GEN_FCN/*!GEN_FCN*/(icode) (x, y);
}

/* Like maybe_expand_insn, but for jumps.  */
//原型 maybe_expand_jump_insn optabs.h optabs.cc
bool mtcs_optabs_maybe_expand_jump_insn (MtcsOptabs *self,enum insn_code icode, unsigned int nops,
            class expand_operand *ops)
{
  rtx_insn *pat = mtcs_optabs_maybe_gen_insn/*!maybe_gen_insn*/(self,icode, nops, ops);
  if (pat){
      emit_jump_insn (pat);
      return true;
  }
  return false;
}

/* Like expand_insn, but for jumps.  */
//原型 expand_jump_insn optabs.h optabs.cc
void mtcs_optabs_expand_jump_insn (MtcsOptabs *self,enum insn_code icode, unsigned int nops,
          class expand_operand *ops)
{
  if (!mtcs_optabs_maybe_expand_jump_insn(self,icode, nops, ops))
    gcc_unreachable ();
}

/* Generate code to indirectly jump to a location given in the rtx LOC.  */
//原型 emit_indirect_jump optabs.h optabs.cc
void mtcs_optabs_emit_indirect_jump (MtcsOptabs *self,rtx loc)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  if (!target_rtx_have_indirect_jump/*!targetm.have_indirect_jump*/(mtcsMachine->tmrtx))
    sorry ("indirect jumps are not available on this target");
  else{
      class expand_operand ops[1];
      create_address_operand (&ops[0], loc);
      mtcs_optabs_expand_jump_insn(self,mtcsMachine->tmrtx->code_for_indirect_jump/*!targetm.code_for_indirect_jump*/, 1, ops);
      mtcs_emit_emit_barrier/*!emit_barrier*/(mtcsEmit);
  }
}

/* Expand vector widening operations.

   There are two different classes of operations handled here:
   1) Operations whose result is wider than all the arguments to the operation.
      Examples: VEC_UNPACK_HI/LO_EXPR, VEC_WIDEN_MULT_HI/LO_EXPR
      In this case OP0 and optionally OP1 would be initialized,
      but WIDE_OP wouldn't (not relevant for this case).
   2) Operations whose result is of the same size as the last argument to the
      operation, but wider than all the other arguments to the operation.
      Examples: WIDEN_SUM_EXPR, VEC_DOT_PROD_EXPR.
      In the case WIDE_OP, OP0 and optionally OP1 would be initialized.

   E.g, when called to expand the following operations, this is how
   the arguments will be initialized:
                                nops    OP0     OP1     WIDE_OP
   widening-sum                 2       oprnd0  -       oprnd1
   widening-dot-product         3       oprnd0  oprnd1  oprnd2
   widening-mult                2       oprnd0  oprnd1  -
   type-promotion (vec-unpack)  1       oprnd0  -       -  */
//原型 expand_widen_pattern_expr optabs.h optabs.cc
rtx mtcs_optabs_expand_widen_pattern_expr (MtcsOptabs *self,sepops ops, rtx op0, rtx op1, rtx wide_op,
               rtx target, int unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

  class expand_operand eops[4];
  tree oprnd0, oprnd1, oprnd2;
  machine_mode wmode = VOIDmode, tmode0, tmode1 = VOIDmode;
  optab widen_pattern_optab;
  enum insn_code icode;
  int nops = TREE_CODE_LENGTH (ops->code);
  int op;
  bool sbool = false;

  oprnd0 = ops->op0;
  oprnd1 = nops >= 2 ? ops->op1 : NULL_TREE;
  oprnd2 = nops >= 3 ? ops->op2 : NULL_TREE;

  tmode0 = TYPE_MODE (TREE_TYPE (oprnd0));
  if (ops->code == VEC_UNPACK_FIX_TRUNC_HI_EXPR
      || ops->code == VEC_UNPACK_FIX_TRUNC_LO_EXPR)
    /* The sign is from the result type rather than operand's type
       for these ops.  */
    widen_pattern_optab  = optab_for_tree_code (ops->code, ops->type, optab_default);
  else if ((ops->code == VEC_UNPACK_HI_EXPR
        || ops->code == VEC_UNPACK_LO_EXPR)
       && VECTOR_BOOLEAN_TYPE_P (ops->type)
       && VECTOR_BOOLEAN_TYPE_P (TREE_TYPE (oprnd0))
       && TYPE_MODE (ops->type) == TYPE_MODE (TREE_TYPE (oprnd0))
       && SCALAR_INT_MODE_P (TYPE_MODE (ops->type))){
      /* For VEC_UNPACK_{LO,HI}_EXPR if the mode of op0 and result is
     the same scalar mode for VECTOR_BOOLEAN_TYPE_P vectors, use
     vec_unpacks_sbool_{lo,hi}_optab, so that we can pass in
     the pattern number of elements in the wider vector.  */
      widen_pattern_optab = (ops->code == VEC_UNPACK_HI_EXPR
       ? vec_unpacks_sbool_hi_optab : vec_unpacks_sbool_lo_optab);
      sbool = true;
  }else if (ops->code == DOT_PROD_EXPR){
      enum optab_subtype subtype = optab_default;
      signop sign1 = TYPE_SIGN (TREE_TYPE (oprnd0));
      signop sign2 = TYPE_SIGN (TREE_TYPE (oprnd1));
      if (sign1 == sign2)
          ;
      else if (sign1 == SIGNED && sign2 == UNSIGNED){
          subtype = optab_vector_mixed_sign;
          /* Same as optab_vector_mixed_sign but flip the operands.  */
          std::swap (op0, op1);
      }else if (sign1 == UNSIGNED && sign2 == SIGNED)
          subtype = optab_vector_mixed_sign;
      else
          gcc_unreachable ();

      widen_pattern_optab = optab_for_tree_code (ops->code, TREE_TYPE (oprnd0), subtype);
  }else
      widen_pattern_optab   = optab_for_tree_code (ops->code, TREE_TYPE (oprnd0), optab_default);
  if (ops->code == WIDEN_MULT_PLUS_EXPR  || ops->code == WIDEN_MULT_MINUS_EXPR)
    icode = mtcs_optabs_find_widening_optab_handler/*!find_widening_optab_handler*/(self,widen_pattern_optab,
                     TYPE_MODE (TREE_TYPE (ops->op2)), tmode0);
  else
    icode =mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,widen_pattern_optab, tmode0);
  gcc_assert (icode != CODE_FOR_nothing);

  if (nops >= 2)
    tmode1 = TYPE_MODE (TREE_TYPE (oprnd1));
  else if (sbool){
      nops = 2;
      op1 = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,TYPE_VECTOR_SUBPARTS (TREE_TYPE (oprnd0)).to_constant ());
      tmode1 = tmode0;
  }

  /* The last operand is of a wider mode than the rest of the operands.  */
  if (nops == 2)
    wmode = tmode1;
  else if (nops == 3){
      gcc_assert (tmode1 == tmode0);
      gcc_assert (op1);
      wmode = TYPE_MODE (TREE_TYPE (oprnd2));
  }

  op = 0;
  create_output_operand (&eops[op++], target, TYPE_MODE (ops->type));
  create_convert_operand_from (&eops[op++], op0, tmode0, unsignedp);
  if (op1)
    create_convert_operand_from (&eops[op++], op1, tmode1, unsignedp);
  if (wide_op)
    create_convert_operand_from (&eops[op++], wide_op, wmode, unsignedp);
  mtcs_optabs_expand_insn/*!expand_insn*/(self,icode, op, eops);
  return eops[0].value;
}

/* Expand a highpart multiply.  */
//原型 expand_mult_highpart optabs.h optabs.cc
rtx mtcs_optabs_expand_mult_highpart (MtcsOptabs *self,machine_mode mode, rtx op0, rtx op1,
              rtx target, bool uns_p)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  class expand_operand eops[3];
  enum insn_code icode;
  int method, i;
  machine_mode wmode;
  rtx m1, m2;
  optab tab1, tab2;

  method = mtcs_optabs_can_mult_highpart_p/*!can_mult_highpart_p*/(self,mode, uns_p);
  switch (method){
    case 0:
      return NULL_RTX;
    case 1:
      tab1 = uns_p ? umul_highpart_optab : smul_highpart_optab;
      return mtcs_optabs_expand_binop/*!expand_binop*/(self,mode, tab1, op0, op1, target, uns_p,
               OPTAB_LIB_WIDEN);
    case 2:
      tab1 = uns_p ? vec_widen_umult_even_optab : vec_widen_smult_even_optab;
      tab2 = uns_p ? vec_widen_umult_odd_optab : vec_widen_smult_odd_optab;
      break;
    case 3:
      tab1 = uns_p ? vec_widen_umult_lo_optab : vec_widen_smult_lo_optab;
      tab2 = uns_p ? vec_widen_umult_hi_optab : vec_widen_smult_hi_optab;
      if (BYTES_BIG_ENDIAN)
          std::swap (tab1, tab2);
      break;
    default:
      gcc_unreachable ();
  }

  icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,tab1, mode);
  wmode =  mtcsOutput->insn_data[(int) icode].operand[0].mode/*!insn_data[icode].operand[0].mode*/;

  gcc_checking_assert (known_eq (2 * mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,wmode),
          mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,mode)));
  gcc_checking_assert (known_eq (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,wmode),
          mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode)));

  create_output_operand (&eops[0], mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,wmode), wmode);
  create_input_operand (&eops[1], op0, mode);
  create_input_operand (&eops[2], op1, mode);
  mtcs_optabs_expand_insn/*!expand_insn*/(self,icode, 3, eops);
  m1 = gen_lowpart (mode, eops[0].value);

  create_output_operand (&eops[0], mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,wmode), wmode);
  create_input_operand (&eops[1], op0, mode);
  create_input_operand (&eops[2], op1, mode);
  mtcs_optabs_expand_insn/*!expand_insn*/(self,mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,tab2, mode), 3, eops);
  m2 = gen_lowpart (mode, eops[0].value);

  vec_perm_builder sel;
  if (method == 2){
      /* The encoding has 2 interleaved stepped patterns.  */
      sel.new_vector (mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,mode), 2, 3);
      for (i = 0; i < 6; ++i)
          sel.quick_push (!BYTES_BIG_ENDIAN + (i & ~1)
            + ((i & 1) ? mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,mode) : 0));
  }else{
      /* The encoding has a single interleaved stepped pattern.  */
      sel.new_vector (mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,mode), 1, 3);
      for (i = 0; i < 3; ++i)
          sel.quick_push (2 * i + (BYTES_BIG_ENDIAN ? 0 : 1));
  }

  return mtcs_optabs_expand_vec_perm_const/*!expand_vec_perm_const*/(self,mode, m1, m2, sel, mtcsMode->modes.M_BLKmode, target);
}

/* Return non-zero if a highpart multiply is supported of can be synthisized.
   For the benefit of expand_mult_highpart, the return value is 1 for direct,
   2 for even/odd widening, and 3 for hi/lo widening.  */
//原型 can_mult_highpart_p optabs-query.h optabs-query.cc
int mtcs_optabs_can_mult_highpart_p (MtcsOptabs *self,machine_mode mode, bool uns_p)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);

  optab op;
  op = uns_p ? umul_highpart_optab : smul_highpart_optab;
  if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,op, mode) != CODE_FOR_nothing)
    return 1;

  /* If the mode is an integral vector, synth from widening operations.  */
  if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) != MODE_VECTOR_INT)
    return 0;

  poly_int64 nunits = mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,mode);

  op = uns_p ? vec_widen_umult_even_optab : vec_widen_smult_even_optab;
  if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,op, mode) != CODE_FOR_nothing){
      op = uns_p ? vec_widen_umult_odd_optab : vec_widen_smult_odd_optab;
      if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,op, mode) != CODE_FOR_nothing){
          /* The encoding has 2 interleaved stepped patterns.  */
          vec_perm_builder sel (nunits, 2, 3);
          for (unsigned int i = 0; i < 6; ++i)
            sel.quick_push (!BYTES_BIG_ENDIAN
                    + (i & ~1)
                    + ((i & 1) ? nunits : 0));
          vec_perm_indices indices (sel, 2, nunits);
          if (mtcs_optabs_can_vec_perm_const_p/*!can_vec_perm_const_p*/(self,mode, mode, indices))
            return 2;
      }
  }

  op = uns_p ? vec_widen_umult_hi_optab : vec_widen_smult_hi_optab;
  if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,op, mode) != CODE_FOR_nothing){
      op = uns_p ? vec_widen_umult_lo_optab : vec_widen_smult_lo_optab;
      if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,op, mode) != CODE_FOR_nothing){
          /* The encoding has a single stepped pattern.  */
          vec_perm_builder sel (nunits, 1, 3);
          for (unsigned int i = 0; i < 3; ++i)
            sel.quick_push (2 * i + (BYTES_BIG_ENDIAN ? 0 : 1));
          vec_perm_indices indices (sel, 2, nunits);
          if (mtcs_optabs_can_vec_perm_const_p/*!can_vec_perm_const_p*/(self,mode, mode, indices))
            return 3;
      }
  }
  return 0;
}

/* Return true if the target directly supports VEC_PERM_EXPRs on vectors
   of mode OP_MODE and result vector of mode MODE using the selector SEL.
   ALLOW_VARIABLE_P is true if it is acceptable to force the selector into a
   register and use a variable permute (if the target supports that).

   Note that additional permutations representing whole-vector shifts may
   also be handled via the vec_shr or vec_shl optab, but only where the
   second input vector is entirely constant zeroes; this case is not dealt
   with here.  */
//原型 can_vec_perm_const_p optabs-query.h optabs-query.cc
bool mtcs_optabs_can_vec_perm_const_p (MtcsOptabs *self,machine_mode mode, machine_mode op_mode,
              const vec_perm_indices &sel, bool allow_variable_p)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  /* If the target doesn't implement a vector mode for the vector type,
     then no operations are supported.  */
  if (!mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,mode))
    return false;
  /* It's probably cheaper to test for the variable case first.  */
  if (op_mode == mode && allow_variable_p
          && mtcs_optabs_selector_fits_mode_p/*!selector_fits_mode_p*/(self,mode, sel)){
      if (mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,vec_perm_optab, mode) != CODE_FOR_nothing)
          return true;
      /* Unlike can_vec_perm_var_p, we don't need to test for optabs
     related computing the QImode selector, since that happens at
     compile time.  */
      machine_mode qimode;
      if (mtcs_optabs_qimode_for_vec_perm/*!qimode_for_vec_perm*/(self,mode).exists (&qimode)){
          vec_perm_indices qimode_indices;
          qimode_indices.new_expanded_vector (sel,mtcs_mode_get_unit_size/*!GET_MODE_UNIT_SIZE*/(mtcsMode,mode));
          if (mtcs_optabs_selector_fits_mode_p/*!selector_fits_mode_p*/(self,qimode, qimode_indices)
              && (mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,vec_perm_optab, qimode)
              != CODE_FOR_nothing))
            return true;
      }
  }

  if (target_vectorize_have_vec_perm_const/*!targetm.vectorize.vec_perm_const!=NULL*/(mtcsMachine->vectorize)){
      if (target_vectorize_vec_perm_const/*!targetm.vectorize.vec_perm_const*/(mtcsMachine->vectorize,
              mode, op_mode, NULL_RTX, NULL_RTX, NULL_RTX, sel))
          return true;
      /* ??? For completeness, we ought to check the QImode version of
     vec_perm_const_optab.  But all users of this implicit lowering
     feature implement the variable vec_perm_optab, and the ia64
     port specifically doesn't want us to lower V2SF operations
     into integer operations.  */
  }
  return false;
}

/* Return true if selector SEL can be represented in the integer
   equivalent of vector mode MODE.  */
//原型 selector_fits_mode_p optabs-query.h optabs-query.cc
bool mtcs_optabs_selector_fits_mode_p (MtcsOptabs *self,machine_mode mode, const vec_perm_indices &sel)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  unsigned HOST_WIDE_INT mask = mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,
          mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode));
  return (mask == HOST_WIDE_INT_M1U  || sel.all_in_range_p (0, mask + 1));
}

/* If a target doesn't implement a permute on a vector with multibyte
   elements, we can try to do the same permute on byte elements.
   If this makes sense for vector mode MODE then return the appropriate
   byte vector mode.  */
//原型 qimode_for_vec_perm optabs-query.h optabs-query.cc
opt_machine_mode mtcs_optabs_qimode_for_vec_perm (MtcsOptabs *self,machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  if (mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode) != mtcsMode->modes.M_QImode)
    return mtcs_mode_related_vector_mode/*!related_vector_mode*/(mtcsMode,mode, mtcsMode->modes.M_QImode,
            mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode));
  return opt_machine_mode ();
}

/* A subroutine of expand_vec_perm_var for expanding one vec_perm insn.  */
//原型 expand_vec_perm_1 optabs.cc
static rtx expand_vec_perm_1 (MtcsOptabs *self,enum insn_code icode, rtx target,
           rtx v0, rtx v1, rtx sel)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

  machine_mode tmode = GET_MODE (target);
  machine_mode smode = GET_MODE (sel);
  class expand_operand ops[4];

  gcc_assert (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,smode) == MODE_VECTOR_INT
          || mtcs_mode_related_int_vector_mode/*!related_int_vector_mode*/(mtcsMode,tmode).require () == smode);
  create_output_operand (&ops[0], target, tmode);
  create_input_operand (&ops[3], sel, smode);

  /* Make an effort to preserve v0 == v1.  The target expander is able to
     rely on this to determine if we're permuting a single input operand.  */
  if (rtx_equal_p (v0, v1)){
      if (!mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(self,icode, 1, v0))
        v0 = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,tmode, v0);
      gcc_checking_assert (mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(self,icode, 1, v0));
      gcc_checking_assert (mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(self,icode, 2, v0));

      create_fixed_operand (&ops[1], v0);
      create_fixed_operand (&ops[2], v0);
  }else{
      create_input_operand (&ops[1], v0, tmode);
      create_input_operand (&ops[2], v1, tmode);
  }

  if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(self,icode, 4, ops))
    return ops[0].value;
  return NULL_RTX;
}

/* Check if vec_perm mask SEL is a constant equivalent to a shift of
   the first vec_perm operand, assuming the second operand (for left shift
   first operand) is a constant vector of zeros.  Return the shift distance
   in bits if so, or NULL_RTX if the vec_perm is not a shift.  MODE is the
   mode of the value being shifted.  SHIFT_OPTAB is vec_shr_optab for right
   shift or vec_shl_optab for left shift.  */
//原型 shift_amt_for_vec_perm_mask optabs.cc
static rtx shift_amt_for_vec_perm_mask (MtcsOptabs *self,machine_mode mode, const vec_perm_indices &sel,
                 optab shift_optab)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

  unsigned int bitsize = mtcs_mode_get_unit_bitsize/*!GET_MODE_UNIT_BITSIZE*/(mtcsMode,mode);
  poly_int64 first = sel[0];
  if (maybe_ge (sel[0], mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,mode)))
    return NULL_RTX;

  if (shift_optab == vec_shl_optab){
      unsigned int nelt;
      if (!mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,mode).is_constant (&nelt))
          return NULL_RTX;
      unsigned firstidx = 0;
      for (unsigned int i = 0; i < nelt; i++){
          if (known_eq (sel[i], nelt)){
              if (i == 0 || firstidx)
                  return NULL_RTX;
              firstidx = i;
          }else if (firstidx
               ? maybe_ne (sel[i], nelt + i - firstidx)
               : maybe_ge (sel[i], nelt))
            return NULL_RTX;
     }

      if (firstidx == 0)
          return NULL_RTX;
      first = firstidx;
  }else if (!sel.series_p (0, 1, first, 1)){
      unsigned int nelt;
      if (!mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,mode).is_constant (&nelt))
          return NULL_RTX;
      for (unsigned int i = 1; i < nelt; i++){
          poly_int64 expected = i + first;
          /* Indices into the second vector are all equivalent.  */
          if (maybe_lt (sel[i], nelt)
              ? maybe_ne (sel[i], expected)
              : maybe_lt (expected, nelt))
            return NULL_RTX;
      }
  }

  return gen_int_shift_amount (mode, first * bitsize);
}

/* Implement a permutation of vectors v0 and v1 using the permutation
   vector in SEL and return the result.  Use TARGET to hold the result
   if nonnull and convenient.

   MODE is the mode of the vectors being permuted (V0 and V1).  SEL_MODE
   is the TYPE_MODE associated with SEL, or BLKmode if SEL isn't known
   to have a particular mode.  */
//原型 expand_vec_perm_const optabs.h optabs.cc
rtx mtcs_optabs_expand_vec_perm_const (MtcsOptabs *self,machine_mode mode, rtx v0, rtx v1,
               const vec_perm_builder &sel, machine_mode sel_mode,rtx target)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
  MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  if (!target || !mtcs_preds_register_operand/*!register_operand*/(mtcsPreds,target, mode))
    target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

  /* Set QIMODE to a different vector mode with byte elements.
     If no such mode, or if MODE already has byte elements, use VOIDmode.  */
  machine_mode qimode;
  if (!mtcs_optabs_qimode_for_vec_perm/*!qimode_for_vec_perm*/(self,mode).exists (&qimode))
    qimode = VOIDmode;

  rtx_insn *last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);

  bool single_arg_p = rtx_equal_p (v0, v1);
  /* Always specify two input vectors here and leave the target to handle
     cases in which the inputs are equal.  Not all backends can cope with
     the single-input representation when testing for a double-input
     target instruction.  */
  vec_perm_indices indices (sel, 2, mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,mode));

  /* See if this can be handled with a vec_shr or vec_shl.  We only do this
     if the second (for vec_shr) or first (for vec_shl) vector is all
     zeroes.  */
  insn_code shift_code = CODE_FOR_nothing;
  insn_code shift_code_qi = CODE_FOR_nothing;
  optab shift_optab = unknown_optab;
  rtx v2 = v0;
  if (v1 == CONST0_RTX (GET_MODE (v1)))
    shift_optab = vec_shr_optab;
  else if (v0 == CONST0_RTX (GET_MODE (v0))){
      shift_optab = vec_shl_optab;
      v2 = v1;
  }
  if (shift_optab != unknown_optab){
      shift_code = mtcs_opinit_optab_handler/*!optab_handler*/ (mtcsOpinit,shift_optab, mode);
      shift_code_qi = ((qimode != VOIDmode && qimode != mode)
               ? mtcs_opinit_optab_handler/*!optab_handler*/ (mtcsOpinit,shift_optab, qimode)
               : CODE_FOR_nothing);
  }
  if (shift_code != CODE_FOR_nothing || shift_code_qi != CODE_FOR_nothing){
      rtx shift_amt = shift_amt_for_vec_perm_mask(self,mode, indices, shift_optab);
      if (shift_amt){
          class expand_operand ops[3];
          if (shift_amt == const0_rtx)
            return v2;
          if (shift_code != CODE_FOR_nothing){
              create_output_operand (&ops[0], target, mode);
              create_input_operand (&ops[1], v2, mode);
              create_convert_operand_from_type (&ops[2], shift_amt, sizetype);
              if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(self,shift_code, 3, ops))
                  return ops[0].value;
          }
          if (shift_code_qi != CODE_FOR_nothing){
              rtx tmp = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,qimode);
              create_output_operand (&ops[0], tmp, qimode);
              create_input_operand (&ops[1], gen_lowpart (qimode, v2), qimode);
              create_convert_operand_from_type (&ops[2], shift_amt, sizetype);
              if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(self,shift_code_qi, 3, ops))
                  return gen_lowpart (mode, ops[0].value);
          }
      }
  }

  if (target_vectorize_have_vec_perm_const/*!targetm.vectorize.vec_perm_const!=NULL*/(mtcsMachine->vectorize)){
      if (single_arg_p)
          v1 = v0;
      gcc_checking_assert (GET_MODE (v0) == GET_MODE (v1));
      machine_mode op_mode = GET_MODE (v0);
      if (target_vectorize_vec_perm_const/*!targetm.vectorize.vec_perm_const*/(mtcsMachine->vectorize,
              mode, op_mode, target, v0, v1,indices))
          return target;
  }

  /* Fall back to a constant byte-based permutation.  */
  vec_perm_indices qimode_indices;
  rtx target_qi = NULL_RTX, v0_qi = NULL_RTX, v1_qi = NULL_RTX;
  if (qimode != VOIDmode){
      qimode_indices.new_expanded_vector (indices,mtcs_mode_get_unit_size/*!GET_MODE_UNIT_SIZE*/(mtcsMode,mode));
      target_qi = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,qimode);
      v0_qi = gen_lowpart (qimode, v0);
      v1_qi = gen_lowpart (qimode, v1);
      if (target_vectorize_have_vec_perm_const/*!targetm.vectorize.vec_perm_const!=NULL*/(mtcsMachine->vectorize)
        && target_vectorize_vec_perm_const/*!targetm.vectorize.vec_perm_const*/(mtcsMachine->vectorize,
              qimode, qimode, target_qi, v0_qi,v1_qi, qimode_indices))
          return gen_lowpart (mode, target_qi);
  }

  v0 = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,mode, v0);
  if (single_arg_p)
    v1 = v0;
  v1 = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,mode, v1);

  /* Otherwise expand as a fully variable permuation.  */

  /* The optabs are only defined for selectors with the same width
     as the values being permuted.  */
  machine_mode required_sel_mode;
  if (!mtcs_mode_related_int_vector_mode/*!related_int_vector_mode*/(mtcsMode,mode).exists(&required_sel_mode)){
      mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
      return NULL_RTX;
  }

  /* We know that it is semantically valid to treat SEL as having SEL_MODE.
     If that isn't the mode we want then we need to prove that using
     REQUIRED_SEL_MODE is OK.  */
  if (sel_mode != required_sel_mode){
      if (!mtcs_optabs_selector_fits_mode_p/*!selector_fits_mode_p*/(self,required_sel_mode, indices)){
          mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
          return NULL_RTX;
      }
      sel_mode = required_sel_mode;
  }

  insn_code icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,vec_perm_optab, mode);
  if (icode != CODE_FOR_nothing){
      rtx sel_rtx = vec_perm_indices_to_rtx (sel_mode, indices);
      rtx tmp = expand_vec_perm_1(self,icode, target, v0, v1, sel_rtx);
      if (tmp)
          return tmp;
  }

  if (qimode != VOIDmode
      && mtcs_optabs_selector_fits_mode_p/*!selector_fits_mode_p*/(self,qimode, qimode_indices)){
      icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,vec_perm_optab, qimode);
      if (icode != CODE_FOR_nothing){
          rtx sel_qi = vec_perm_indices_to_rtx (qimode, qimode_indices);
          rtx tmp = expand_vec_perm_1(self,icode, target_qi, v0_qi, v1_qi, sel_qi);
          if (tmp)
            return gen_lowpart (mode, tmp);
      }
  }

  mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
  return NULL_RTX;
}

/* Generate code to convert FROM or TO a fixed-point.
   If UINTP is true, either TO or FROM is an unsigned integer.
   If SATP is true, we need to saturate the result.  */
//原型 expand_fixed_convert optabs.h optabs.cc
void mtcs_optabs_expand_fixed_convert (MtcsOptabs *self,rtx to, rtx from, int uintp, int satp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);

  machine_mode to_mode = GET_MODE (to);
  machine_mode from_mode = GET_MODE (from);
  convert_optab tab;
  enum rtx_code this_code;
  enum insn_code code;
  rtx_insn *insns;
  rtx value;
  rtx libfunc;

  if (to_mode == from_mode){
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr, to, from);
      return;
  }

  if (uintp){
      tab = satp ? satfractuns_optab : fractuns_optab;
      this_code = satp ? UNSIGNED_SAT_FRACT : UNSIGNED_FRACT_CONVERT;
  }else{
      tab = satp ? satfract_optab : fract_optab;
      this_code = satp ? SAT_FRACT : FRACT_CONVERT;
  }
  code = mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,tab, to_mode, from_mode);
  if (code != CODE_FOR_nothing){
      mtcs_optabs_emit_unop_insn/*!emit_unop_insn*/(self,code, to, from, this_code);
      return;
  }

  libfunc =mtcs_libfuncs_convert_optab_libfunc/*!convert_optab_libfunc*/(mtcsLibfuncs,tab, to_mode, from_mode);
  gcc_assert (libfunc);

  from = prepare_libcall_arg(self,from, uintp);
  from_mode = GET_MODE (from);
  mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
  value = mtcs_calls_emit_library_call_value/*!emit_library_call_value*/(mtcsCalls,libfunc, NULL_RTX, LCT_CONST, to_mode,
                   from, from_mode);
  insns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);

  mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

  mtcs_optabs_emit_libcall_block/*!emit_libcall_block*/(self,insns, to, value,
              gen_rtx_fmt_e (optab_to_code (tab), to_mode, from));
}

/* Emit code to compute the absolute value of OP0, with result to
   TARGET if convenient.  (TARGET may be 0.)  The return value says
   where the result actually is to be found.

   MODE is the mode of the operand; the mode of the result is
   different but can be deduced from MODE.
 */
//原型 expand_abs_nojump optabs.h optabs.cc
rtx mtcs_optabs_expand_abs_nojump (MtcsOptabs *self,machine_mode mode, rtx op0, rtx target,
           int result_unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
  MtcsExpmed   *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  rtx temp;
  if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) != MODE_INT || ! mtcsOptionsItem->x_flag_trapv)
    result_unsignedp = 1;
  /* First try to do it with a special abs instruction.  */
  temp = mtcs_optabs_expand_unop/*!expand_unop*/(self,mode, result_unsignedp ? abs_optab : absv_optab,
                      op0, target, 0);
  if (temp != 0)
    return temp;
  /* For floating point modes, try clearing the sign bit.  */
  scalar_float_mode float_mode;
  if (mtcs_mode_is_a <scalar_float_mode> (mtcsMode,mode, &float_mode)){
      temp = expand_absneg_bit(self,ABS, float_mode, op0, target);
      if (temp)
          return temp;
  }
  /* If we have a MAX insn, we can do this as MAX (x, -x).  */
  if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,smax_optab, mode) != CODE_FOR_nothing
      && !mtcs_mode_honor_signed_zeros/*!HONOR_SIGNED_ZEROS*/(mtcsMode,mode)){
      rtx_insn *last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
      temp = mtcs_optabs_expand_unop/*!expand_unop*/(self,mode, result_unsignedp ? neg_optab : negv_optab,
              op0, NULL_RTX, 0);
      if (temp != 0)
          temp = mtcs_optabs_expand_binop/*!expand_binop*/(self,mode, smax_optab, op0, temp, target, 0,
                 OPTAB_WIDEN);
      if (temp != 0)
          return temp;
      mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
  }
  /* If this machine has expensive jumps, we can do integer absolute
     value of X as (((signed) x >> (W-1)) ^ x) - ((signed) x >> (W-1)),
     where W is the width of MODE.  */
  scalar_int_mode int_mode;
  if (mtcs_mode_is_int_mode/*!is_int_mode*/(mtcsMode,mode, &int_mode)
      && mtcs_emit_branch_cost/*!BRANCH_COST*/(mtcsEmit,optimize_insn_for_speed_p (),false) >= 2){
      rtx extended = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, int_mode, op0,
              mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,int_mode) - 1,NULL_RTX, 0);

      temp = mtcs_optabs_expand_binop/*!expand_binop*/(self,int_mode, xor_optab, extended, op0, target, 0,
               OPTAB_LIB_WIDEN);
      if (temp != 0)
          temp = mtcs_optabs_expand_binop/*!expand_binop*/(self,int_mode,
                 result_unsignedp ? sub_optab : subv_optab,temp, extended, target, 0, OPTAB_LIB_WIDEN);

      if (temp != 0)
          return temp;
  }
  return NULL_RTX;
}
//原型 expand_abs optabs.h optabs.cc
rtx mtcs_optabs_expand_abs (MtcsOptabs *self,machine_mode mode, rtx op0, rtx target,
        int result_unsignedp, int safe)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsReg  *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  rtx temp;
  rtx_code_label *op1;

  if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) != MODE_INT
      || ! mtcsOptionsItem->x_flag_trapv)
    result_unsignedp = 1;

  temp = mtcs_optabs_expand_abs_nojump/*!expand_abs_nojump*/(self,mode, op0, target, result_unsignedp);
  if (temp != 0)
    return temp;

  /* If that does not win, use conditional jump and negate.  */

  /* It is safe to use the target if it is the same
     as the source if this is also a pseudo register */
  if (op0 == target && REG_P (op0)
      && REGNO (op0) >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg))
    safe = 1;

  op1 = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
  if (target == 0 || ! safe
      || GET_MODE (target) != mode
      || (MEM_P (target) && MEM_VOLATILE_P (target))
      || (REG_P (target)
      && REGNO (target) < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)))
    target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

  mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, op0);
  /*!NO_DEFER_POP; expr.h 定义*/
  mtcsRtlData/*!crtl*/->expr.x_inhibit_defer_pop+=1;

  mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,target, CONST0_RTX (mode), GE, 0, mode,
               NULL_RTX, NULL, op1,
               profile_probability::uninitialized ());

  op0 = mtcs_optabs_expand_unop/*!expand_unop*/(self,mode, result_unsignedp ? neg_optab : negv_optab,
                     target, target, 0);
  if (op0 != target)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, op0);
  mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,op1);
  /*!OK_DEFER_POP;expr.h 定义*/
  mtcsRtlData/*!crtl*/->expr.x_inhibit_defer_pop-=1;

  return target;
}

/* Return nonzero if a conditional move of mode MODE is supported.

   This function is for combine so it can tell whether an insn that looks
   like a conditional move is actually supported by the hardware.  If we
   guess wrong we lose a bit on optimization, but that's it.  */
/* ??? sparc64 supports conditionally moving integers values based on fp
   comparisons, and vice versa.  How do we handle them?  */
//原型 can_conditionally_move_p optabs-query.h optabs-query.cc
bool mtcs_optabs_can_conditionally_move_p (MtcsOptabs *self,machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  return  mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,movcc_optab, mode) != CODE_FOR_nothing;
}

/* Report whether we have an instruction to perform the operation
   specified by CODE on operands of mode MODE.  */
//原型 have_insn_for optabs.h optabs.cc
bool mtcs_optabs_have_insn_for (MtcsOptabs *self,enum rtx_code code, machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  return (code_to_optab (code)
      && (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,code_to_optab (code), mode)
          != CODE_FOR_nothing));
}

/* Return insn code for a comparison operator with VMODE
   resultin MASK_MODE, unsigned if UNS is true.  */
//原型 get_vec_cmp_icode optabs-query.h
enum insn_code mtcs_optabs_get_vec_cmp_icode (MtcsOptabs *self,machine_mode vmode, machine_mode mask_mode, bool uns)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  optab tab = uns ? vec_cmpu_optab : vec_cmp_optab;
  return mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,tab, vmode, mask_mode);
}

/* Return insn code for a comparison operator with VMODE
   resultin MASK_MODE (only for EQ/NE).  */
//原型 get_vec_cmp_eq_icode optabs-query.h
enum insn_code mtcs_optabs_get_vec_cmp_eq_icode (MtcsOptabs *self, machine_mode vmode, machine_mode mask_mode)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
    return mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,vec_cmpeq_optab, vmode, mask_mode);
}

/* Return a comparison rtx of mode CMP_MODE for COND.  Use UNSIGNEDP to
   select signed or unsigned operators.  OPNO holds the index of the
   first comparison operand for insn ICODE.  Do not generate the
   compare instruction itself.  */
//原型 vector_compare_rtx optabs.h optabs.cc
rtx mtcs_optabs_vector_compare_rtx (MtcsOptabs *self,machine_mode cmp_mode, enum tree_code tcode,
            tree t_op0, tree t_op1, bool unsignedp,
            enum insn_code icode, unsigned int opno)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

  class expand_operand ops[2];
  rtx rtx_op0, rtx_op1;
  machine_mode m0, m1;
  enum rtx_code rcode = get_rtx_code (tcode, unsignedp);

  gcc_assert (TREE_CODE_CLASS (tcode) == tcc_comparison);

  /* Expand operands.  For vector types with scalar modes, e.g. where int64x1_t
     has mode DImode, this can produce a constant RTX of mode VOIDmode; in such
     cases, use the original mode.  */
  rtx_op0 = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,t_op0, NULL_RTX, TYPE_MODE (TREE_TYPE (t_op0)),
             EXPAND_STACK_PARM);
  m0 = GET_MODE (rtx_op0);
  if (m0 == VOIDmode)
    m0 = TYPE_MODE (TREE_TYPE (t_op0));

  rtx_op1 = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,t_op1, NULL_RTX, TYPE_MODE (TREE_TYPE (t_op1)),
             EXPAND_STACK_PARM);
  m1 = GET_MODE (rtx_op1);
  if (m1 == VOIDmode)
    m1 = TYPE_MODE (TREE_TYPE (t_op1));

  create_input_operand (&ops[0], rtx_op0, m0);
  create_input_operand (&ops[1], rtx_op1, m1);
  if (!mtcs_optabs_maybe_legitimize_operands/*!maybe_legitimize_operands*/(self,icode, opno, 2, ops))
    gcc_unreachable ();
  return gen_rtx_fmt_ee (rcode, cmp_mode, ops[0].value, ops[1].value);
}

/* Generate insns for a vector comparison into a mask.  */
//原型 expand_vec_cmp_expr optabs.h optabs.cc
rtx mtcs_optabs_expand_vec_cmp_expr (MtcsOptabs *self,tree type, tree exp, rtx target)
{
  class expand_operand ops[4];
  enum insn_code icode;
  rtx comparison;
  machine_mode mask_mode = TYPE_MODE (type);
  machine_mode vmode;
  bool unsignedp;
  tree op0a, op0b;
  enum tree_code tcode;

  op0a = TREE_OPERAND (exp, 0);
  op0b = TREE_OPERAND (exp, 1);
  tcode = TREE_CODE (exp);

  unsignedp = TYPE_UNSIGNED (TREE_TYPE (op0a));
  vmode = TYPE_MODE (TREE_TYPE (op0a));

  icode = mtcs_optabs_get_vec_cmp_icode/*!get_vec_cmp_icode*/(self,vmode, mask_mode, unsignedp);
  if (icode == CODE_FOR_nothing){
      if (tcode == EQ_EXPR || tcode == NE_EXPR)
          icode = mtcs_optabs_get_vec_cmp_eq_icode/*!get_vec_cmp_eq_icode*/(self,vmode, mask_mode);
      if (icode == CODE_FOR_nothing)
          return 0;
  }

  comparison = mtcs_optabs_vector_compare_rtx/*!vector_compare_rtx*/(self,mask_mode, tcode, op0a, op0b,
                   unsignedp, icode, 2);
  create_output_operand (&ops[0], target, mask_mode);
  create_fixed_operand (&ops[1], comparison);
  create_fixed_operand (&ops[2], XEXP (comparison, 0));
  create_fixed_operand (&ops[3], XEXP (comparison, 1));
  mtcs_optabs_expand_insn/*!expand_insn*/(self,icode, 4, ops);
  return ops[0].value;
}

/* Implement a permutation of vectors v0 and v1 using the permutation
   vector in SEL and return the result.  Use TARGET to hold the result
   if nonnull and convenient.

   MODE is the mode of the vectors being permuted (V0 and V1).
   SEL must have the integer equivalent of MODE and is known to be
   unsuitable for permutes with a constant permutation vector.  */
//原型 expand_vec_perm_var optabs.h optabs.cc
rtx mtcs_optabs_expand_vec_perm_var (MtcsOptabs *self,machine_mode mode, rtx v0, rtx v1, rtx sel, rtx target)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  enum insn_code icode;
  unsigned int i, u;
  rtx tmp, sel_qi;

  u = mtcs_mode_get_unit_size/*!GET_MODE_UNIT_SIZE*/(mtcsMode,mode);

  if (!target || GET_MODE (target) != mode)
    target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

  icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,vec_perm_optab, mode);
  if (icode != CODE_FOR_nothing){
      tmp = expand_vec_perm_1(self,icode, target, v0, v1, sel);
      if (tmp)
          return tmp;
  }
  /* As a special case to aid several targets, lower the element-based
     permutation to a byte-based permutation and try again.  */
  machine_mode qimode;
  if (!mtcs_optabs_qimode_for_vec_perm/*!qimode_for_vec_perm*/(self,mode).exists (&qimode)
      || maybe_gt (mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,qimode),
              mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,mtcsMode->modes.M_QImode) + 1))
     return NULL_RTX;
  icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,vec_perm_optab, qimode);
  if (icode == CODE_FOR_nothing)
    return NULL_RTX;
  /* Multiply each element by its byte size.  */
  machine_mode selmode = GET_MODE (sel);
  if (u == 2)
    sel = mtcs_optabs_expand_simple_binop(self,selmode, PLUS, sel, sel,
                   NULL, 0, OPTAB_DIRECT);
  else
    sel = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(self,selmode, ASHIFT, sel,
            mtcs_rtl_gen_int_shift_amount/*!gen_int_shift_amount*/(mtcsRTL,selmode, exact_log2 (u)),
                   NULL, 0, OPTAB_DIRECT);
  gcc_assert (sel != NULL);

  /* Broadcast the low byte each element into each of its bytes.
     The encoding has U interleaved stepped patterns, one for each
     byte of an element.  */
  vec_perm_builder const_sel (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode), u, 3);
  unsigned int low_byte_in_u = BYTES_BIG_ENDIAN ? u - 1 : 0;
  for (i = 0; i < 3; ++i)
    for (unsigned int j = 0; j < u; ++j)
      const_sel.quick_push (i * u + low_byte_in_u);
  sel = gen_lowpart (qimode, sel);
  sel = mtcs_optabs_expand_vec_perm_const/*!expand_vec_perm_const*/(self,qimode, sel, sel, const_sel, qimode, NULL);
  gcc_assert (sel != NULL);

  /* Add the byte offset to each byte element.  */
  /* Note that the definition of the indicies here is memory ordering,
     so there should be no difference between big and little endian.  */
  rtx_vector_builder byte_indices (qimode, u, 1);
  for (i = 0; i < u; ++i)
    byte_indices.quick_push (GEN_INT (i));
  tmp = byte_indices.build ();
  sel_qi = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(self,qimode, PLUS, sel, tmp,
                sel, 0, OPTAB_DIRECT);
  gcc_assert (sel_qi != NULL);

  tmp = mode != qimode ? mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,qimode) : target;
  tmp = expand_vec_perm_1(self,icode, tmp, gen_lowpart (qimode, v0),
               gen_lowpart (qimode, v1), sel_qi);
  if (tmp)
    tmp = gen_lowpart (mode, tmp);
  return tmp;
}


/* Generate code to perform an operation specified by TERNARY_OPTAB
   on operands OP0, OP1 and OP2, with result having machine-mode MODE.

   UNSIGNEDP is for the case where we have to widen the operands
   to perform the operation.  It says to use zero-extension.

   If TARGET is nonzero, the value
   is generated there, if it is convenient to do so.
   In all cases an rtx is returned for the locus of the value;
   this may or may not be TARGET.  */
//原型 expand_ternary_op optabs.h optabs.cc
rtx mtcs_optabs_expand_ternary_op (MtcsOptabs *self,machine_mode mode, optab ternary_optab, rtx op0,
           rtx op1, rtx op2, rtx target, int unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);

  class expand_operand ops[4];
  enum insn_code icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,ternary_optab, mode);

  gcc_assert (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,ternary_optab, mode) != CODE_FOR_nothing);

  create_output_operand (&ops[0], target, mode);
  create_convert_operand_from (&ops[1], op0, mode, unsignedp);
  create_convert_operand_from (&ops[2], op1, mode, unsignedp);
  create_convert_operand_from (&ops[3], op2, mode, unsignedp);
  mtcs_optabs_expand_insn/*!expand_insn*/(self,icode, 4, ops);
  return ops[0].value;
}

/* Generate VEC_SERIES_EXPR <OP0, OP1>, returning a value of mode VMODE.
   Use TARGET for the result if nonnull and convenient.  */
//原型 expand_vec_series_expr optabs.h optabs.cc
rtx mtcs_optabs_expand_vec_series_expr (MtcsOptabs *self,machine_mode vmode, rtx op0, rtx op1, rtx target)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);

  class expand_operand ops[3];
  enum insn_code icode;
  machine_mode emode =mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,vmode);
  icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,vec_series_optab, vmode);
  gcc_assert (icode != CODE_FOR_nothing);
  create_output_operand (&ops[0], target, vmode);
  create_input_operand (&ops[1], op0, emode);
  create_input_operand (&ops[2], op1, emode);
  mtcs_optabs_expand_insn/*!expand_insn*/(self,icode, 3, ops);
  return ops[0].value;
}

/* A subroutine of expand_copysign, perform the entire copysign operation
   with integer bitmasks.  BITPOS is the position of the sign bit; OP0_IS_ABS
   is true if op0 is known to have its sign bit clear.  */
//原型 expand_copysign_bit optabs.cc
static rtx expand_copysign_bit (MtcsOptabs *self,scalar_float_mode mode, rtx op0, rtx op1, rtx target,
           int bitpos, bool op0_is_abs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  scalar_int_mode imode;
  int word, nwords, i;
  rtx temp;
  rtx_insn *insns;

  if (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode) <= UNITS_PER_WORD){
      if (!mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,mode).exists (&imode))
         return NULL_RTX;
      word = 0;
      nwords = 1;
  }else{
      imode = mtcsMode->word_mode;
      if (FLOAT_WORDS_BIG_ENDIAN)
         word = (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) - bitpos) / BITS_PER_WORD;
      else
         word = bitpos / BITS_PER_WORD;
      bitpos = bitpos % BITS_PER_WORD;
      nwords = (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) + BITS_PER_WORD - 1) / BITS_PER_WORD;
  }

  wide_int mask = wi::set_bit_in_zero (bitpos, mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,imode));

  if (target == 0
      || target == op0
      || target == op1
      || mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,target, op0)
      || mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,target, op1)
      || (nwords > 1 && !mtcs_optabs_valid_multiword_target_p/*!valid_multiword_target_p*/(self,target)))
    target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

  if (nwords > 1){
     mtcs_emit_start_sequence/*start_sequence*/ (mtcsEmit);
     for (i = 0; i < nwords; ++i){
        rtx targ_piece = mtcs_rtl_operand_subword/*!operand_subword*/(mtcsRTL,target, i, 1, mode);
        rtx op0_piece = mtcs_rtl_operand_subword_force/*!operand_subword_force*/(mtcsRTL,op0, i, mode);

        if (i == word){
            if (!op0_is_abs)
               op0_piece   = mtcs_optabs_expand_binop/*!expand_binop*/(self,imode, and_optab, op0_piece,
                 mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,~mask, imode),
                 NULL_RTX, 1, OPTAB_LIB_WIDEN);
            op1 =mtcs_optabs_expand_binop/*!expand_binop*/(self,imode, and_optab,
                  mtcs_rtl_operand_subword_force/*!operand_subword_force*/(mtcsRTL,op1, i, mode),
                 mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,mask, imode),
                 NULL_RTX, 1, OPTAB_LIB_WIDEN);

            temp = mtcs_optabs_expand_binop/*!expand_binop*/(self,imode, ior_optab, op0_piece, op1,
                  targ_piece, 1, OPTAB_LIB_WIDEN);
            if (temp != targ_piece)
               mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,targ_piece, temp);
        }else
           mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,targ_piece, op0_piece);
     }

      insns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,insns);
  }else{
      op1 = mtcs_optabs_expand_binop/*!expand_binop*/(self,imode, and_optab, gen_lowpart (imode, op1),
            mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,mask, imode),
                NULL_RTX, 1, OPTAB_LIB_WIDEN);

      op0 = gen_lowpart (imode, op0);
      if (!op0_is_abs)
         op0 = mtcs_optabs_expand_binop/*!expand_binop*/(self,imode, and_optab, op0,
              mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,~mask, imode),
              NULL_RTX, 1, OPTAB_LIB_WIDEN);

      temp = mtcs_optabs_expand_binop/*!expand_binop*/(self,imode, ior_optab, op0, op1,
            gen_lowpart (imode, target), 1, OPTAB_LIB_WIDEN);
      target = lowpart_subreg_maybe_copy(self,mode, temp, imode);
  }
  return target;
}


/* A subroutine of expand_copysign, perform the copysign operation using the
   abs and neg primitives advertised to exist on the target.  The assumption
   is that we have a split register file, and leaving op0 in fp registers,
   and not playing with subregs so much, will help the register allocator.  */
//原型 expand_copysign_absneg optabs.cc
static rtx expand_copysign_absneg (MtcsOptabs *self,scalar_float_mode mode, rtx op0, rtx op1, rtx target,
              int bitpos, bool op0_is_abs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

   scalar_int_mode imode;
   enum insn_code icode;
   rtx sign;
   rtx_code_label *label;

   if (target == op1)
     target = NULL_RTX;

   /* Check if the back end provides an insn that handles signbit for the
      argument's mode. */
   icode =mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,signbit_optab, mode);
   if (icode != CODE_FOR_nothing){
       imode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,mtcsOutput->insn_data[(int) icode].operand[0].mode);
       sign = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,imode);
       mtcs_optabs_emit_unop_insn/*!emit_unop_insn*/(self,icode, sign, op1, UNKNOWN);
   }else{
       if (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode) <= UNITS_PER_WORD){
          if (!mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,mode).exists (&imode))
             return NULL_RTX;
          op1 = gen_lowpart (imode, op1);
       }else{
          int word;

          imode = mtcsMode->word_mode;
          if (FLOAT_WORDS_BIG_ENDIAN)
             word = (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) - bitpos) / BITS_PER_WORD;
          else
             word = bitpos / BITS_PER_WORD;
          bitpos = bitpos % BITS_PER_WORD;
          op1 = mtcs_rtl_operand_subword_force/*!operand_subword_force*/(mtcsRTL,op1, word, mode);
       }

       wide_int mask = wi::set_bit_in_zero (bitpos,mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,imode));
       sign = mtcs_optabs_expand_binop/*!expand_binop*/(self,imode, and_optab, op1,
             mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,mask, imode),
             NULL_RTX, 1, OPTAB_LIB_WIDEN);
   }

   if (!op0_is_abs){
       op0 = mtcs_optabs_expand_unop/*!expand_unop*/(self,mode, abs_optab, op0, target, 0);
       if (op0 == NULL)
          return NULL_RTX;
       target = op0;
   }else{
       if (target == NULL_RTX)
         target = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,op0);
       else
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, op0);
   }

   label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
   mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(self,sign, const0_rtx, EQ, NULL_RTX, imode, 1, label);

   if (CONST_DOUBLE_AS_FLOAT_P (op0))
     op0 = mtcs_simplify_rtx_unary_operation/*!simplify_unary_operation*/(mtcsSimplifyRtx,NEG, mode, op0, mode);
   else
     op0 =  mtcs_optabs_expand_unop/*!expand_unop*/(self,mode, neg_optab, op0, target, 0);
   if (op0 != target)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, op0);

   mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);

   return target;
}



/* Expand the C99 copysign operation.  OP0 and OP1 must be the same
   scalar floating point mode.  Return NULL if we do not know how to
   expand the operation inline.  */
//原型 expand_copysign optabs.h optabs.cc
rtx mtcs_optabs_expand_copysign (MtcsOptabs *self,rtx op0, rtx op1, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

  scalar_float_mode mode;
  const struct real_format *fmt;
  bool op0_is_abs;
  rtx temp;

  mode = mtcs_mode_as_a <scalar_float_mode> (mtcsMode,GET_MODE (op0));
  gcc_assert (GET_MODE (op1) == mode);

  /* First try to do it with a special instruction.  */
  temp = mtcs_optabs_expand_binop/*!expand_binop*/(self,mode, copysign_optab, op0, op1,
             target, 0, OPTAB_DIRECT);
  if (temp)
    return temp;

  fmt = mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,mode);
  if (fmt == NULL || !fmt->has_signed_zero)
    return NULL_RTX;

  op0_is_abs = false;
  if (CONST_DOUBLE_AS_FLOAT_P (op0)){
      if (real_isneg (CONST_DOUBLE_REAL_VALUE (op0)))
         op0 = mtcs_simplify_rtx_unary_operation/*!simplify_unary_operation*/(mtcsSimplifyRtx,ABS, mode, op0, mode);
      op0_is_abs = true;
  }

  if (fmt->signbit_ro >= 0  && (CONST_DOUBLE_AS_FLOAT_P (op0)
     || (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,neg_optab, mode) != CODE_FOR_nothing
         && mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,abs_optab, mode) != CODE_FOR_nothing))){
      temp = expand_copysign_absneg(self,mode, op0, op1, target,fmt->signbit_ro, op0_is_abs);
      if (temp)
         return temp;
  }

  if (fmt->signbit_rw < 0)
    return NULL_RTX;
  return expand_copysign_bit(self,mode, op0, op1, target,
               fmt->signbit_rw, op0_is_abs);
}

/* Generate code to convert FROM to fixed point and store in TO.  FROM
   must be floating point, TO must be signed.  Use the conversion optab
   TAB to do the conversion.  */
//原型 expand_sfix_optab optabs.h optabs.cc
bool mtcs_optabs_expand_sfix_optab (MtcsOptabs *self,rtx to, rtx from, convert_optab tab)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   enum insn_code icode;
   rtx target = to;
   machine_mode fmode, imode;
   /* We first try to find a pair of modes, one real and one integer, at
   least as wide as FROM and TO, respectively, in which we can open-code
   this conversion.  If the integer mode is wider than the mode of TO,
   we can do the conversion either signed or unsigned.  */

   MTCS_FOR_EACH_MODE_FROM/*!FOR_EACH_MODE_FROM*/(mtcsMode,fmode, GET_MODE (from))
      MTCS_FOR_EACH_MODE_FROM/*!FOR_EACH_MODE_FROM*/(mtcsMode,imode, GET_MODE (to)){
         icode = mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,tab,
               imode, fmode,insn_optimization_type ());
         if (icode != CODE_FOR_nothing){
            rtx_insn *last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
            if (fmode != GET_MODE (from))
               from = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,fmode, from, 0);

            if (imode != GET_MODE (to))
               target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,imode);

            if (!mtcs_optabs_maybe_emit_unop_insn/*!maybe_emit_unop_insn*/(self,icode, target, from, UNKNOWN)){
               mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
               continue;
            }
            if (target != to)
               mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,to, target, 0);
            return true;
         }
      }

   return false;
}

/* Generate code to perform an operation specified by UNOPPTAB
   on operand OP0, with two results to TARG0 and TARG1.
   We assume that the order of the operands for the instruction
   is TARG0, TARG1, OP0.

   Either TARG0 or TARG1 may be zero, but what that means is that
   the result is not actually wanted.  We will generate it into
   a dummy pseudo-reg and discard it.  They may not both be zero.

   Returns true if this operation can be performed; false if not.  */
//原型 expand_twoval_unop optabs.h optabs.cc
bool mtcs_optabs_expand_twoval_unop (MtcsOptabs *self,optab unoptab, rtx op0, rtx targ0, rtx targ1,
          int unsignedp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   machine_mode mode = GET_MODE (targ0 ? targ0 : targ1);
   enum mode_class mclass;
   machine_mode wider_mode;
   rtx_insn *entry_last =  mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
   rtx_insn *last;
   mclass = mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode);
   if (!targ0)
      targ0 = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
   if (!targ1)
      targ1 = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
   /* Record where to go back to if we fail.  */
   last =  mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
   if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,unoptab, mode) != CODE_FOR_nothing){
      class expand_operand ops[3];
      enum insn_code icode =mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,unoptab, mode);

      create_fixed_operand (&ops[0], targ0);
      create_fixed_operand (&ops[1], targ1);
      create_convert_operand_from (&ops[2], op0, mode, unsignedp);
      if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(self,icode, 3, ops))
            return true;
   }
   /* It can't be done in this mode.  Can we do it in a wider mode?  */
   if (CLASS_HAS_WIDER_MODES_P (mclass)){
      MTCS_FOR_EACH_WIDER_MODE/*!FOR_EACH_WIDER_MODE*/(mtcsMode,wider_mode, mode){
         if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,unoptab, wider_mode) != CODE_FOR_nothing){
            rtx t0 = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,wider_mode);
            rtx t1 = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,wider_mode);
            rtx cop0 =mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,wider_mode, mode, op0, unsignedp);
            if (mtcs_optabs_expand_twoval_unop/*!expand_twoval_unop*/(self,unoptab, cop0, t0, t1, unsignedp)){
               mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,targ0, t0, unsignedp);
               mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,targ1, t1, unsignedp);
               return true;
            }else
               mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
         }
      }
   }
   mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,entry_last);
   return false;
}

/* This function tries to emit an atomic_exchange intruction.  VAL is written
   to *MEM using memory model MODEL. The previous contents of *MEM are returned,
   using TARGET if possible.  */
//原型 maybe_emit_atomic_exchange optabs.cc
static rtx maybe_emit_atomic_exchange (MtcsOptabs *self,rtx target, rtx mem, rtx val, enum memmodel model)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);

   machine_mode mode = GET_MODE (mem);
   enum insn_code icode;
   /* If the target supports the exchange directly, great.  */
   icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,atomic_exchange_optab, mode);
   if (icode != CODE_FOR_nothing){
      class expand_operand ops[4];

      create_output_operand (&ops[0], target, mode);
      create_fixed_operand (&ops[1], mem);
      create_input_operand (&ops[2], val, mode);
      mtcs_optabs_create_integer_operand/*!create_integer_operand*/(self,&ops[3], model);
      if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(self,icode, 4, ops))
         return ops[0].value;
   }
   return NULL_RTX;
}


/* This is a helper function for the other atomic operations.  This function
   emits a loop that contains SEQ that iterates until a compare-and-swap
   operation at the end succeeds.  MEM is the memory to be modified.  SEQ is
   a set of instructions that takes a value from OLD_REG as an input and
   produces a value in NEW_REG as an output.  Before SEQ, OLD_REG will be
   set to the current contents of MEM.  After SEQ, a compare-and-swap will
   attempt to update MEM with NEW_REG.  The function returns true when the
   loop was generated successfully.  */
//原型 expand_compare_and_swap_loop optabs.cc
static bool expand_compare_and_swap_loop (MtcsOptabs *self,rtx mem, rtx old_reg, rtx new_reg, rtx seq)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

  machine_mode mode = GET_MODE (mem);
  rtx_code_label *label;
  rtx cmp_reg, success, oldval;

  /* The loop we want to generate looks like

   cmp_reg = mem;
      label:
        old_reg = cmp_reg;
   seq;
   (success, cmp_reg) = compare-and-swap(mem, old_reg, new_reg)
   if (success)
     goto label;

     Note that we only do the plain load from memory once.  Subsequent
     iterations use the value loaded by the compare-and-swap pattern.  */

  label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
  cmp_reg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

  mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,cmp_reg, mem);
  mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
  mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,old_reg, cmp_reg);
  if (seq)
     mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,seq);

  success = NULL_RTX;
  oldval = cmp_reg;
  if (!mtcs_optabs_expand_atomic_compare_and_swap/*!expand_atomic_compare_and_swap*/(self,
        &success, &oldval, mem, old_reg, new_reg, false, MEMMODEL_SYNC_SEQ_CST,MEMMODEL_RELAXED))
    return false;

  if (oldval != cmp_reg)
     mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,cmp_reg, oldval);

  /* Mark this jump predicted not taken.  */
  mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(self,success, const0_rtx, EQ, const0_rtx,
            GET_MODE (success), 1, label,profile_probability::guessed_never ());
  return true;
}


/* This function tries to implement an atomic exchange operation using a
   compare_and_swap loop. VAL is written to *MEM.  The previous contents of
   *MEM are returned, using TARGET if possible.  No memory model is required
   since a compare_and_swap loop is seq-cst.  */
//原型 maybe_emit_compare_and_swap_exchange_loop optabs.cc
static rtx maybe_emit_compare_and_swap_exchange_loop(MtcsOptabs *self,rtx target, rtx mem, rtx val)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   machine_mode mode = GET_MODE (mem);
   if (mtcs_optabs_can_compare_and_swap_p/*!can_compare_and_swap_p*/(self,mode, true)){
      if (!target || !mtcs_preds_register_operand/*!register_operand*/(mtcsPreds,target, mode))
         target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
      if (expand_compare_and_swap_loop(self,mem, target, val, NULL_RTX))
         return target;
   }
   return NULL_RTX;
}

/* Generate asm volatile("" : : : "memory") as the memory blockage.  */
//原型 expand_asm_memory_blockage optabs.cc

static void expand_asm_memory_blockage (MtcsOptabs *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   rtx asm_op, clob;
   asm_op = gen_rtx_ASM_OPERANDS (VOIDmode, "", "", 0,
          rtvec_alloc (0), rtvec_alloc (0), rtvec_alloc (0), UNKNOWN_LOCATION);
   MEM_VOLATILE_P (asm_op) = 1;
   clob = gen_rtx_SCRATCH (VOIDmode);
   clob = gen_rtx_MEM (mtcsMode->modes.M_BLKmode, clob);
   clob = gen_rtx_CLOBBER (VOIDmode, clob);
   mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_rtx_PARALLEL (VOIDmode, gen_rtvec (2, asm_op, clob)));
}


/* Do not propagate memory accesses across this point.  */
//原型 expand_memory_blockage optabs.cc
static void expand_memory_blockage (MtcsOptabs *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

   if (target_rtx_have_memory_blockage/*!targetm.have_memory_blockage*/(mtcsMachine->tmrtx))
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,
               target_rtx_gen_memory_blockage/*!targetm.gen_memory_blockage*/(mtcsMachine->tmrtx));
   else
      expand_asm_memory_blockage(self);
}

/* This function expands the atomic store operation:
   Atomically store VAL in MEM.
   MEMMODEL is the memory model variant to use.
   USE_RELEASE is true if __sync_lock_release can be used as a fall back.
   function returns const0_rtx if a pattern was emitted.  */
//原型 expand_atomic_store optabs.h optabs.cc
rtx mtcs_optabs_expand_atomic_store (MtcsOptabs *self,rtx mem, rtx val, enum memmodel model, bool use_release)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   machine_mode mode = GET_MODE (mem);
   enum insn_code icode;
   class expand_operand ops[3];

   /* If the target supports the store directly, great.  */
   icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,atomic_store_optab, mode);
   if (icode != CODE_FOR_nothing){
      rtx_insn *last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
      if (!is_mm_relaxed (model))
         expand_memory_blockage(self);
      create_fixed_operand (&ops[0], mem);
      create_input_operand (&ops[1], val, mode);
      mtcs_optabs_create_integer_operand/*!create_integer_operand*/(self,&ops[2], model);
      if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(self,icode, 3, ops)){
         if (is_mm_seq_cst (model))
            expand_memory_blockage(self);
         return const0_rtx;
      }
      mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
   }

   /* If using __sync_lock_release is a viable alternative, try it.
   Note that this will not be set to true if we are expanding a generic
   __atomic_store_n.  */
   if (use_release){
      icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,sync_lock_release_optab, mode);
      if (icode != CODE_FOR_nothing){
         create_fixed_operand (&ops[0], mem);
         create_input_operand (&ops[1], const0_rtx, mode);
         if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(self,icode, 2, ops)){
            /* lock_release is only a release barrier.  */
            if (is_mm_seq_cst (model))
               mtcs_optabs_expand_mem_thread_fence/*!expand_mem_thread_fence*/(self,model);
            return const0_rtx;
         }
      }
   }

   /* If the size of the object is greater than word size on this target,
   a default store will not be atomic.  */
   if (maybe_gt (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode), BITS_PER_WORD)){
      /* If loads are atomic or we are called to provide a __sync builtin,
      we can try a atomic_exchange and throw away the result.  Otherwise,
      don't do anything so that we do not create an inconsistency between
      loads and stores.  */
      if (mtcs_optabs_can_atomic_load_p/*!can_atomic_load_p*/(self,mode) || is_mm_sync (model)){
         rtx target = maybe_emit_atomic_exchange(self,NULL_RTX, mem, val, model);
         if (!target)
            target = maybe_emit_compare_and_swap_exchange_loop(self,NULL_RTX, mem,val);
         if (target)
            return const0_rtx;
      }
      return NULL_RTX;
   }
   /* Otherwise assume stores are atomic, and emit the proper barriers.  */
   mtcs_optabs_expand_mem_thread_fence/*!expand_mem_thread_fence*/(self,model);
   mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mem, val);
   /* For SEQ_CST, also emit a barrier after the store.  */
   if (is_mm_seq_cst (model))
      mtcs_optabs_expand_mem_thread_fence/*!expand_mem_thread_fence*/(self,model);

   return const0_rtx;
}


/* This function expands the atomic exchange operation:
   atomically store VAL in MEM and return the previous value in MEM.

   MEMMODEL is the memory model variant to use.
   TARGET is an optional place to stick the return value.  */
//原型 expand_atomic_exchange optabs.h optabs.cc
rtx mtcs_optabs_expand_atomic_exchange (MtcsOptabs *self,rtx target, rtx mem, rtx val, enum memmodel model)
{
  machine_mode mode = GET_MODE (mem);
  rtx ret;
  /* If loads are not atomic for the required size and we are not called to
     provide a __sync builtin, do not do anything so that we stay consistent
     with atomic loads of the same size.  */
  if (!mtcs_optabs_can_atomic_load_p/*!can_atomic_load_p*/(self,mode) && !is_mm_sync (model))
    return NULL_RTX;

  ret = maybe_emit_atomic_exchange(self,target, mem, val, model);
  /* Next try a compare-and-swap loop for the exchange.  */
  if (!ret)
    ret = maybe_emit_compare_and_swap_exchange_loop(self,target, mem, val);

  return ret;
}

/* Return true if an atomic load can be performed without falling back to
   a compare-and-swap.  */
//原型 can_atomic_load_p optabs-query.h optabs-query.cc
bool mtcs_optabs_can_atomic_load_p (MtcsOptabs *self,machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);

   enum insn_code icode;
   /* Does the target supports the load directly?  */
   icode =  mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,atomic_load_optab, mode);
   if (icode != CODE_FOR_nothing)
      return true;
   /* If the size of the object is greater than word size on this target,
   then we assume that a load will not be atomic.  Also see
   expand_atomic_load.  */
   return known_le (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode), BITS_PER_WORD);
}

/* Return true if there is a compare_and_swap pattern.  */
//原型 can_compare_and_swap_p optabs-query.h optabs-query.cc
bool mtcs_optabs_can_compare_and_swap_p (MtcsOptabs *self,machine_mode mode, bool allow_libcall)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
   MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);

   enum insn_code icode;
   /* Check for __atomic_compare_and_swap.  */
   icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,atomic_compare_and_swap_optab, mode);
   if (icode != CODE_FOR_nothing)
      return true;
   /* Check for __sync_compare_and_swap.  */
   icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,sync_compare_and_swap_optab, mode);
   if (icode != CODE_FOR_nothing)
      return true;
   if (allow_libcall &&
         mtcs_libfuncs_optab_libfunc/*!optab_libfunc*/(mtcsLibfuncs,sync_compare_and_swap_optab, mode))
      return true;
   /* No inline compare and swap.  */
   return false;
}

/* This routine will either emit the mem_thread_fence pattern or issue a
   sync_synchronize to generate a fence for memory model MEMMODEL.  */
//原型 expand_mem_thread_fence optabs.h optabs.cc
void mtcs_optabs_expand_mem_thread_fence (MtcsOptabs *self,enum memmodel model)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

   if (is_mm_relaxed (model))
      return;
   if (target_rtx_have_mem_thread_fence/*!targetm.have_mem_thread_fence*/(mtcsMachine->tmrtx)){
      emit_insn (target_rtx_gen_mem_thread_fence/*!targetm.gen_mem_thread_fence*/(mtcsMachine->tmrtx,
      mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,model)));
      expand_memory_blockage(self);
   }else if (target_rtx_have_memory_barrier/*!targetm.have_memory_barrier*/(mtcsMachine->tmrtx))
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,target_rtx_gen_memory_barrier/*!targetm.gen_memory_barrier*/(mtcsMachine->tmrtx));
   else if (synchronize_libfunc != NULL_RTX)
      mtcs_calls_emit_library_call/*!emit_library_call*/(mtcsCalls,synchronize_libfunc, LCT_NORMAL, VOIDmode);
   else
      expand_memory_blockage(self);
}

typedef struct _FindCCData{
    MtcsOptabs *mtcsOptabs;
    rtx *data;
}FindCCData;
/* Helper function to find the MODE_CC set in a sync_compare_and_swap
   pattern.  */
//原型 find_cc_set optabs.cc 回调函数
static void find_cc_set_cb (rtx x, const_rtx pat, void *userData)
{
   FindCCData *findCCData=(FindCCData *)userData;
   MtcsOptabs *self=findCCData->mtcsOptabs;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

  if (REG_P (x) && mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,GET_MODE (x)) == MODE_CC
      && GET_CODE (pat) == SET){
      rtx *p_cc_reg = (rtx *)findCCData->data;
      gcc_assert (!*p_cc_reg);
      *p_cc_reg = x;
  }
}

/* This function expands the atomic compare exchange operation:

   *PTARGET_BOOL is an optional place to store the boolean success/failure.
   *PTARGET_OVAL is an optional place to store the old value from memory.
   Both target parameters may be NULL or const0_rtx to indicate that we do
   not care about that return value.  Both target parameters are updated on
   success to the actual location of the corresponding result.

   MEMMODEL is the memory model variant to use.

   The return value of the function is true for success.  */
//原型 expand_atomic_compare_and_swap optabs.h optabs.cc
bool mtcs_optabs_expand_atomic_compare_and_swap (MtcsOptabs *self,rtx *ptarget_bool, rtx *ptarget_oval,
            rtx mem, rtx expected, rtx desired, bool is_weak, enum memmodel succ_model,enum memmodel fail_model)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
   MtcsExpmed   *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   machine_mode mode = GET_MODE (mem);
   class expand_operand ops[8];
   enum insn_code icode;
   rtx target_oval, target_bool = NULL_RTX;
   rtx libfunc;

   /* If loads are not atomic for the required size and we are not called to
   provide a __sync builtin, do not do anything so that we stay consistent
   with atomic loads of the same size.  */
   if (!mtcs_optabs_can_atomic_load_p/*!can_atomic_load_p*/(self,mode) && !is_mm_sync (succ_model))
      return false;
   /* Load expected into a register for the compare and swap.  */
   if (MEM_P (expected))
      expected = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,expected);
   /* Make sure we always have some place to put the return oldval.
   Further, make sure that place is distinct from the input expected,
   just in case we need that path down below.  */
   if (ptarget_oval && *ptarget_oval == const0_rtx)
      ptarget_oval = NULL;

   if (ptarget_oval == NULL
   || (target_oval = *ptarget_oval) == NULL
   || mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,expected, target_oval))
      target_oval = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

   icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,atomic_compare_and_swap_optab, mode);
   if (icode != CODE_FOR_nothing){
      machine_mode bool_mode = mtcsOutput->insn_data[icode].operand[0].mode;
      if (ptarget_bool && *ptarget_bool == const0_rtx)
         ptarget_bool = NULL;
      /* Make sure we always have a place for the bool operand.  */
      if (ptarget_bool == NULL
      || (target_bool = *ptarget_bool) == NULL
      || GET_MODE (target_bool) != bool_mode)
         target_bool =mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,bool_mode);

      /* Emit the compare_and_swap.  */
      create_output_operand (&ops[0], target_bool, bool_mode);
      create_output_operand (&ops[1], target_oval, mode);
      create_fixed_operand (&ops[2], mem);
      create_input_operand (&ops[3], expected, mode);
      create_input_operand (&ops[4], desired, mode);
      mtcs_optabs_create_integer_operand/*!create_integer_operand*/(self,&ops[5], is_weak);
      mtcs_optabs_create_integer_operand/*!create_integer_operand*/(self,&ops[6], succ_model);
      mtcs_optabs_create_integer_operand/*!create_integer_operand*/(self,&ops[7], fail_model);
      if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(self,icode, 8, ops)){
         /* Return success/failure.  */
         target_bool = ops[0].value;
         target_oval = ops[1].value;
         goto success;
      }
   }

   /* Otherwise fall back to the original __sync_val_compare_and_swap
   which is always seq-cst.  */
   icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,sync_compare_and_swap_optab, mode);
   if (icode != CODE_FOR_nothing){
      rtx cc_reg;

      create_output_operand (&ops[0], target_oval, mode);
      create_fixed_operand (&ops[1], mem);
      create_input_operand (&ops[2], expected, mode);
      create_input_operand (&ops[3], desired, mode);
      if (!mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(self,icode, 4, ops))
         return false;

      target_oval = ops[0].value;

      /* If the caller isn't interested in the boolean return value,
      skip the computation of it.  */
      if (ptarget_bool == NULL)
         goto success;

      /* Otherwise, work out if the compare-and-swap succeeded.  */
      cc_reg = NULL_RTX;
      if (mtcs_optabs_have_insn_for/*!have_insn_for*/(self,COMPARE, mtcsMode->modes.M_CCmode)){
         FindCCData userData={self,&cc_reg};
         note_stores (mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData),
               find_cc_set_cb,&userData/*!&cc_reg*/);
      }
      if (cc_reg){
         target_bool = mtcs_expmed_emit_store_flag_force/*!emit_store_flag_force*/(mtcsExpmed,
               target_bool, EQ, cc_reg, const0_rtx, VOIDmode, 0, 1);
         goto success;
      }
      goto success_bool_from_val;
   }

   /* Also check for library support for __sync_val_compare_and_swap.  */
   libfunc = mtcs_libfuncs_optab_libfunc/*!optab_libfunc*/(mtcsLibfuncs,sync_compare_and_swap_optab, mode);
   if (libfunc != NULL){
      rtx addr = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,ptr_mode, XEXP (mem, 0));
      rtx target = mtcs_calls_emit_library_call_value/*!emit_library_call_value*/(mtcsCalls,libfunc, NULL_RTX, LCT_NORMAL,
      mode, addr, ptr_mode,expected, mode, desired, mode);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target_oval, target);

      /* Compute the boolean return value only if requested.  */
      if (ptarget_bool)
         goto success_bool_from_val;
      else
         goto success;
   }
   /* Failure.  */
   return false;
success_bool_from_val:
   target_bool = mtcs_expmed_emit_store_flag_force/*!emit_store_flag_force*/(mtcsExpmed,
         target_bool, EQ, target_oval,expected, VOIDmode, 1, 1);
success:
   /* Make sure that the oval output winds up where the caller asked.  */
   if (ptarget_oval)
      *ptarget_oval = target_oval;
   if (ptarget_bool)
      *ptarget_bool = target_bool;
   return true;
}


/* Fill in structure pointed to by OP with the various optab entries for an
   operation of type CODE.  */
//原型 get_atomic_op_for_code optabs.cc
static void get_atomic_op_for_code (struct atomic_op_functions *op, enum rtx_code code)
{
  gcc_assert (op!= NULL);
   n_debug("mtcsoptabs get_atomic_op_for_code code:%d %d\n",code,PLUS);
  /* If SWITCHABLE_TARGET is defined, then subtargets can be switched
     in the source code during compilation, and the optab entries are not
     computable until runtime.  Fill in the values at runtime.  */
  switch (code){
    case PLUS:
      op->mem_fetch_before = atomic_fetch_add_optab;
      op->mem_fetch_after = atomic_add_fetch_optab;
      op->mem_no_result = atomic_add_optab;
      op->fetch_before = sync_old_add_optab;
      op->fetch_after = sync_new_add_optab;
      op->no_result = sync_add_optab;
      op->reverse_code = MINUS;
      break;
    case MINUS:
      op->mem_fetch_before = atomic_fetch_sub_optab;
      op->mem_fetch_after = atomic_sub_fetch_optab;
      op->mem_no_result = atomic_sub_optab;
      op->fetch_before = sync_old_sub_optab;
      op->fetch_after = sync_new_sub_optab;
      op->no_result = sync_sub_optab;
      op->reverse_code = PLUS;
      break;
    case XOR:
      op->mem_fetch_before = atomic_fetch_xor_optab;
      op->mem_fetch_after = atomic_xor_fetch_optab;
      op->mem_no_result = atomic_xor_optab;
      op->fetch_before = sync_old_xor_optab;
      op->fetch_after = sync_new_xor_optab;
      op->no_result = sync_xor_optab;
      op->reverse_code = XOR;
      break;
    case AND:
      op->mem_fetch_before = atomic_fetch_and_optab;
      op->mem_fetch_after = atomic_and_fetch_optab;
      op->mem_no_result = atomic_and_optab;
      op->fetch_before = sync_old_and_optab;
      op->fetch_after = sync_new_and_optab;
      op->no_result = sync_and_optab;
      op->reverse_code = UNKNOWN;
      break;
    case IOR:
      op->mem_fetch_before = atomic_fetch_or_optab;
      op->mem_fetch_after = atomic_or_fetch_optab;
      op->mem_no_result = atomic_or_optab;
      op->fetch_before = sync_old_ior_optab;
      op->fetch_after = sync_new_ior_optab;
      op->no_result = sync_ior_optab;
      op->reverse_code = UNKNOWN;
      break;
    case NOT:
      op->mem_fetch_before = atomic_fetch_nand_optab;
      op->mem_fetch_after = atomic_nand_fetch_optab;
      op->mem_no_result = atomic_nand_optab;
      op->fetch_before = sync_old_nand_optab;
      op->fetch_after = sync_new_nand_optab;
      op->no_result = sync_nand_optab;
      op->reverse_code = UNKNOWN;
      break;
    default:
      gcc_unreachable ();
    }
}

/* See if there is a more optimal way to implement the operation "*MEM CODE VAL"
   using memory order MODEL.  If AFTER is true the operation needs to return
   the value of *MEM after the operation, otherwise the previous value.
   TARGET is an optional place to place the result.  The result is unused if
   it is const0_rtx.
   Return the result if there is a better sequence, otherwise NULL_RTX.  */
//原型 maybe_optimize_fetch_op optabs.cc
static rtx maybe_optimize_fetch_op (MtcsOptabs *self,rtx target, rtx mem, rtx val, enum rtx_code code,
          enum memmodel model, bool after)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   /* If the value is prefetched, or not used, it may be possible to replace
   the sequence with a native exchange operation.  */
   if (!after || target == const0_rtx){
      /* fetch_and (&x, 0, m) can be replaced with exchange (&x, 0, m).  */
      if (code == AND && val == const0_rtx){
         if (target == const0_rtx)
            target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (mem));
         return maybe_emit_atomic_exchange(self,target, mem, val, model);
      }
   /* fetch_or (&x, -1, m) can be replaced with exchange (&x, -1, m).  */
      if (code == IOR && val == constm1_rtx){
         if (target == const0_rtx)
            target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (mem));
         return maybe_emit_atomic_exchange(self,target, mem, val, model);
      }
   }
   return NULL_RTX;
}

/* Try to emit an instruction for a specific operation varaition.
   OPTAB contains the OP functions.
   TARGET is an optional place to return the result. const0_rtx means unused.
   MEM is the memory location to operate on.
   VAL is the value to use in the operation.
   USE_MEMMODEL is TRUE if the variation with a memory model should be tried.
   MODEL is the memory model, if used.
   AFTER is true if the returned result is the value after the operation.  */
//原型 maybe_emit_op optabs.cc
static rtx maybe_emit_op (MtcsOptabs *self,const struct atomic_op_functions *optab, rtx target, rtx mem,
          rtx val, bool use_memmodel, enum memmodel model, bool after)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);

   machine_mode mode = GET_MODE (mem);
   class expand_operand ops[4];
   enum insn_code icode;
   int op_counter = 0;
   int num_ops;

   /* Check to see if there is a result returned.  */
   if (target == const0_rtx){
      if (use_memmodel){
         icode =  mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,optab->mem_no_result, mode);
         n_debug("mtcsoptabs.c maybe_emit_op -- icode:%d mode:%d\n",icode,mode);
         mtcs_optabs_create_integer_operand/*!create_integer_operand*/(self,&ops[2], model);
         num_ops = 3;
      } else {
         icode =  mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,optab->no_result, mode);
         num_ops = 2;
      }
   }
   /* Otherwise, we need to generate a result.  */
   else {
      if (use_memmodel)  {
         icode =  mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,
               after ? optab->mem_fetch_after: optab->mem_fetch_before, mode);
         mtcs_optabs_create_integer_operand/*!create_integer_operand*/(self,&ops[3], model);
         num_ops = 4;
      }else{
         icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,after ? optab->fetch_after : optab->fetch_before, mode);
         num_ops = 3;
      }
      create_output_operand (&ops[op_counter++], target, mode);
   }
   if (icode == CODE_FOR_nothing)
      return NULL_RTX;

   create_fixed_operand (&ops[op_counter++], mem);
   /* VAL may have been promoted to a wider mode.  Shrink it if so.  */
   create_convert_operand_to (&ops[op_counter++], val, mode, true);

   if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(self,icode, num_ops, ops))
      return (target == const0_rtx ? const0_rtx : ops[0].value);

   return NULL_RTX;
}


/* This function expands an atomic fetch_OP or OP_fetch operation:
   TARGET is an option place to stick the return value.  const0_rtx indicates
   the result is unused.
   atomically fetch MEM, perform the operation with VAL and return it to MEM.
   CODE is the operation being performed (OP)
   MEMMODEL is the memory model variant to use.
   AFTER is true to return the result of the operation (OP_fetch).
   AFTER is false to return the value before the operation (fetch_OP).

   This function will *only* generate instructions if there is a direct
   optab. No compare and swap loops or libcalls will be generated. */
//原型 expand_atomic_fetch_op_no_fallback optabs.cc
static rtx expand_atomic_fetch_op_no_fallback (MtcsOptabs *self,rtx target, rtx mem, rtx val,
                enum rtx_code code, enum memmodel model,bool after)
{
   machine_mode mode = GET_MODE (mem);
   struct atomic_op_functions optab;
   rtx result;
   bool unused_result = (target == const0_rtx);
   get_atomic_op_for_code (&optab, code);
   /* Check to see if there are any better instructions.  */
   result = maybe_optimize_fetch_op(self,target, mem, val, code, model, after);
   if (result)
      return result;
   n_debug("mtcsoptabs.c expand_atomic_fetch_op_no_fallback 00 %d %d %d unused_result:%d\n",code,model,after,unused_result);

   /* Check for the case where the result isn't used and try those patterns.  */
   if (unused_result){
      /* Try the memory model variant first.  */
      result = maybe_emit_op(self,&optab, target, mem, val, true, model, true);
      if (result)
         return result;

      n_debug("mtcsoptabs.c expand_atomic_fetch_op_no_fallback 11 %d %d %d unused_result:%d\n",code,model,after,unused_result);

      /* Next try the old style withuot a memory model.  */
      result = maybe_emit_op(self,&optab, target, mem, val, false, model, true);
      if (result)
         return result;
      /* There is no no-result pattern, so try patterns with a result.  */
      target = NULL_RTX;
   }
   /* Try the __atomic version.  */
   result = maybe_emit_op(self,&optab, target, mem, val, true, model, after);
   if (result)
      return result;
   /* Try the older __sync version.  */
   result = maybe_emit_op(self,&optab, target, mem, val, false, model, after);
   if (result)
      return result;

   /* If the fetch value can be calculated from the other variation of fetch,
   try that operation.  */
   if (after || unused_result || optab.reverse_code != UNKNOWN){
      /* Try the __atomic version, then the older __sync version.  */
      result = maybe_emit_op(self,&optab, target, mem, val, true, model, !after);
      if (!result)
         result = maybe_emit_op(self,&optab, target, mem, val, false, model, !after);

      if (result){
         /* If the result isn't used, no need to do compensation code.  */
         if (unused_result)
            return result;

         /* Issue compensation code.  Fetch_after  == fetch_before OP val.
         Fetch_before == after REVERSE_OP val.  */
         if (!after)
            code = optab.reverse_code;
         if (code == NOT){
            result = mtcs_optabs_expand_simple_binop(self,mode, AND, result, val, NULL_RTX,true, OPTAB_LIB_WIDEN);
            result = mtcs_optabs_expand_simple_unop/*!expand_simple_unop*/(self,mode, NOT, result, target, true);
         }else
            result = mtcs_optabs_expand_simple_binop(self,mode, code, result, val, target,true, OPTAB_LIB_WIDEN);
         return result;
      }
   }
   /* No direct opcode can be generated.  */
   return NULL_RTX;
}

/* This function expands an atomic fetch_OP or OP_fetch operation:
   TARGET is an option place to stick the return value.  const0_rtx indicates
   the result is unused.
   atomically fetch MEM, perform the operation with VAL and return it to MEM.
   CODE is the operation being performed (OP)
   MEMMODEL is the memory model variant to use.
   AFTER is true to return the result of the operation (OP_fetch).
   AFTER is false to return the value before the operation (fetch_OP).  */
//原型 expand_atomic_fetch_op optabs.h optabs.cc
rtx mtcs_optabs_expand_atomic_fetch_op (MtcsOptabs *self,rtx target, rtx mem, rtx val, enum rtx_code code,
         enum memmodel model, bool after)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
   MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);

   machine_mode mode = GET_MODE (mem);
   rtx result;
   bool unused_result = (target == const0_rtx);
   /* If loads are not atomic for the required size and we are not called to
   provide a __sync builtin, do not do anything so that we stay consistent
   with atomic loads of the same size.  */
   if (!mtcs_optabs_can_atomic_load_p/*!can_atomic_load_p*/(self,mode) && !is_mm_sync (model))
      return NULL_RTX;
   result = expand_atomic_fetch_op_no_fallback(self,target, mem, val, code, model,after);
   if (result)
      return result;
   /* Add/sub can be implemented by doing the reverse operation with -(val).  */
   if (code == PLUS || code == MINUS){
      rtx tmp;
      enum rtx_code reverse = (code == PLUS ? MINUS : PLUS);
      mtcs_emit_start_sequence/*start_sequence*/ (mtcsEmit);
      tmp = mtcs_optabs_expand_simple_unop/*!expand_simple_unop*/(self,mode, NEG, val, NULL_RTX, true);
      result = expand_atomic_fetch_op_no_fallback(self,target, mem, tmp, reverse, model, after);
      if (result){
         /* PLUS worked so emit the insns and return.  */
         tmp = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
         mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,tmp);
         return result;
      }
      /* PLUS did not work, so throw away the negation code and continue.  */
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   }
   /* Try the __sync libcalls only if we can't do compare-and-swap inline.  */
   if (!mtcs_optabs_can_compare_and_swap_p/*!can_compare_and_swap_p*/(self,mode, false)){
      rtx libfunc;
      bool fixup = false;
      enum rtx_code orig_code = code;
      struct atomic_op_functions optab;

      get_atomic_op_for_code (&optab, code);
      libfunc = mtcs_libfuncs_optab_libfunc/*!optab_libfunc*/(mtcsLibfuncs,
            after ? optab.fetch_after : optab.fetch_before, mode);
      if (libfunc == NULL && (after || unused_result || optab.reverse_code != UNKNOWN)){
         fixup = true;
         if (!after)
            code = optab.reverse_code;
         libfunc =mtcs_libfuncs_optab_libfunc/*!optab_libfunc*/(mtcsLibfuncs,
               after ? optab.fetch_before : optab.fetch_after, mode);
      }
      if (libfunc != NULL){
         rtx addr = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,ptr_mode, XEXP (mem, 0));
         result = mtcs_calls_emit_library_call_value/*!emit_library_call_value*/(mtcsCalls,
               libfunc, NULL, LCT_NORMAL, mode, addr, ptr_mode, val, mode);

         if (!unused_result && fixup)
            result = mtcs_optabs_expand_simple_binop(self,mode, code, result, val, target,true, OPTAB_LIB_WIDEN);
         return result;
      }
      /* We need the original code for any further attempts.  */
      code = orig_code;
   }
   /* If nothing else has succeeded, default to a compare and swap loop.  */
   if (mtcs_optabs_can_compare_and_swap_p/*!can_compare_and_swap_p*/(self,mode, true)){
      rtx_insn *insn;
      rtx t0 =  mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode), t1;

      mtcs_emit_start_sequence/*start_sequence*/ (mtcsEmit);
      /* If the result is used, get a register for it.  */
      if (!unused_result){
         if (!target || !mtcs_preds_register_operand/*!register_operand*/(mtcsPreds,target, mode))
            target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
         /* If fetch_before, copy the value now.  */
         if (!after)
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, t0);
      }else
         target = const0_rtx;

      t1 = t0;
      if (code == NOT){
         t1 = mtcs_optabs_expand_simple_binop(self,mode, AND, t1, val, NULL_RTX,
         true, OPTAB_LIB_WIDEN);
         t1 = mtcs_optabs_expand_simple_unop/*!expand_simple_unop*/(self,mode, code, t1, NULL_RTX, true);
      }else
         t1 = mtcs_optabs_expand_simple_binop(self,mode, code, t1, val, NULL_RTX, true,OPTAB_LIB_WIDEN);
      /* For after, copy the value now.  */
      if (!unused_result && after)
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, t1);
      insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

      if (t1 != NULL && expand_compare_and_swap_loop(self,mem, t0, t1, insn))
         return target;
   }
   return NULL_RTX;
}


/* This function tries to implement an atomic exchange operation using
   __sync_lock_test_and_set. VAL is written to *MEM using memory model MODEL.
   The previous contents of *MEM are returned, using TARGET if possible.
   Since this instructionn is an acquire barrier only, stronger memory
   models may require additional barriers to be emitted.  */
//原型 expand_sync_lock_test_and_set optabs.cc
static rtx maybe_emit_sync_lock_test_and_set (MtcsOptabs *self,rtx target, rtx mem, rtx val,
               enum memmodel model)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
   MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
   MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);

   machine_mode mode = GET_MODE (mem);
   enum insn_code icode;
   rtx_insn *last_insn = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);

   icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,sync_lock_test_and_set_optab, mode);

   /* Legacy sync_lock_test_and_set is an acquire barrier.  If the pattern
   exists, and the memory model is stronger than acquire, add a release
   barrier before the instruction.  */
   if (is_mm_seq_cst (model) || is_mm_release (model) || is_mm_acq_rel (model))
      mtcs_optabs_expand_mem_thread_fence/*!expand_mem_thread_fence*/(self,model);

   if (icode != CODE_FOR_nothing){
      class expand_operand ops[3];
      create_output_operand (&ops[0], target, mode);
      create_fixed_operand (&ops[1], mem);
      create_input_operand (&ops[2], val, mode);
      if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(self,icode, 3, ops))
         return ops[0].value;
   }
   /* If an external test-and-set libcall is provided, use that instead of
   any external compare-and-swap that we might get from the compare-and-
   swap-loop expansion later.  */
   if (!mtcs_optabs_can_compare_and_swap_p/*!can_compare_and_swap_p*/(self,mode, false)){
      rtx libfunc = mtcs_libfuncs_optab_libfunc/*!optab_libfunc*/(mtcsLibfuncs,
            sync_lock_test_and_set_optab, mode);
      if (libfunc != NULL){
         rtx addr;

         addr = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,ptr_mode, XEXP (mem, 0));
         return  mtcs_calls_emit_library_call_value/*!emit_library_call_value*/(mtcsCalls,
               libfunc, NULL_RTX, LCT_NORMAL,mode, addr, ptr_mode, val, mode);
      }
   }
   /* If the test_and_set can't be emitted, eliminate any barrier that might
   have been emitted.  */
   mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last_insn);
   return NULL_RTX;
}

/* This function tries to implement an atomic test-and-set operation
   using the atomic_test_and_set instruction pattern.  A boolean value
   is returned from the operation, using TARGET if possible.  */
//原型 maybe_emit_atomic_test_and_set optabs.cc
static rtx maybe_emit_atomic_test_and_set (MtcsOptabs *self,rtx target, rtx mem, enum memmodel model)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

   machine_mode pat_bool_mode;
   class expand_operand ops[3];

   if (!target_rtx_have_atomic_test_and_set/*!targetm.have_atomic_test_and_set*/(mtcsMachine->tmrtx))
      return NULL_RTX;

   /* While we always get QImode from __atomic_test_and_set, we get
   other memory modes from __sync_lock_test_and_set.  Note that we
   use no endian adjustment here.  This matches the 4.6 behavior
   in the Sparc backend.  */
   enum insn_code icode = mtcsMachine->tmrtx->code_for_atomic_test_and_set/*!targetm.code_for_atomic_test_and_set*/;
   gcc_checking_assert (mtcsOutput->insn_data[icode].operand[1].mode == mtcsMode->modes.M_QImode);
   if (GET_MODE (mem) != mtcsMode->modes.M_QImode)
      mem = mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/(mtcsRTL,mem, mtcsMode->modes.M_QImode, 0);

   pat_bool_mode = mtcsOutput->insn_data[icode].operand[0].mode;
   create_output_operand (&ops[0], target, pat_bool_mode);
   create_fixed_operand (&ops[1], mem);
   mtcs_optabs_create_integer_operand/*!create_integer_operand*/(self,&ops[2], model);

   if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(self,icode, 3, ops))
      return ops[0].value;
   return NULL_RTX;
}

/* This function expands the legacy _sync_lock test_and_set operation which is
   generally an atomic exchange.  Some limited targets only allow the
   constant 1 to be stored.  This is an ACQUIRE operation.

   TARGET is an optional place to stick the return value.
   MEM is where VAL is stored.  */
//原型 expand_sync_lock_test_and_set optabs.h optabs.cc
rtx mtcs_optabs_expand_sync_lock_test_and_set (MtcsOptabs *self,rtx target, rtx mem, rtx val)
{
   rtx ret;
   /* Try an atomic_exchange first.  */
   ret = maybe_emit_atomic_exchange(self,target, mem, val, MEMMODEL_SYNC_ACQUIRE);
   if (ret)
      return ret;
   ret = maybe_emit_sync_lock_test_and_set(self,target, mem, val,MEMMODEL_SYNC_ACQUIRE);
   if (ret)
      return ret;
   ret = maybe_emit_compare_and_swap_exchange_loop(self,target, mem, val);
   if (ret)
      return ret;
   /* If there are no other options, try atomic_test_and_set if the value
   being stored is 1.  */
   if (val == const1_rtx)
      ret = maybe_emit_atomic_test_and_set(self,target, mem, MEMMODEL_SYNC_ACQUIRE);

   return ret;
}

/* Emit a signal fence with given memory model.  */
//原型 expand_mem_signal_fence optabs.h optabs.cc
void mtcs_optabs_expand_mem_signal_fence (MtcsOptabs *self,enum memmodel model)
{
  /* No machine barrier is required to implement a signal fence, but
     a compiler memory barrier must be issued, except for relaxed MM.  */
  if (!is_mm_relaxed (model))
    expand_memory_blockage(self);
}

/* This function expands the atomic test_and_set operation:
   atomically store a boolean TRUE into MEM and return the previous value.

   MEMMODEL is the memory model variant to use.
   TARGET is an optional place to stick the return value.  */
//原型 expand_atomic_test_and_set optabs.h optabs.cc
rtx mtcs_optabs_expand_atomic_test_and_set (MtcsOptabs *self,rtx target, rtx mem, enum memmodel model)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpmed   *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);

   machine_mode mode = GET_MODE (mem);
   rtx ret, trueval, subtarget;

   ret = maybe_emit_atomic_test_and_set(self,target, mem, model);
   if (ret)
      return ret;

   /* Be binary compatible with non-default settings of trueval, and different
   cpu revisions.  E.g. one revision may have atomic-test-and-set, but
   another only has atomic-exchange.  */
   if (mtcsTarget/*!targetm.atomic_test_and_set_trueval*/->atomic_test_and_set_trueval == 1){
      trueval = const1_rtx;
      subtarget = target ? target : mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
   }else {
      trueval = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,
      mtcsTarget/*!targetm.atomic_test_and_set_trueval*/->atomic_test_and_set_trueval, mode);
      subtarget = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
   }
   /* Try the atomic-exchange optab...  */
   ret = maybe_emit_atomic_exchange(self,subtarget, mem, trueval, model);
   /* ... then an atomic-compare-and-swap loop ... */
   if (!ret)
      ret = maybe_emit_compare_and_swap_exchange_loop(self,subtarget, mem, trueval);
   /* ... before trying the vaguely defined legacy lock_test_and_set. */
   if (!ret)
      ret = maybe_emit_sync_lock_test_and_set(self,subtarget, mem, trueval, model);

   /* Recall that the legacy lock_test_and_set optab was allowed to do magic
   things with the value 1.  Thus we try again without trueval.  */
   if (!ret && mtcsTarget/*!targetm.atomic_test_and_set_trueval*/->atomic_test_and_set_trueval != 1){
      ret = maybe_emit_sync_lock_test_and_set(self,subtarget, mem, const1_rtx, model);
      if (ret) {
         /* Rectify the not-one trueval.  */
         ret = mtcs_expmed_emit_store_flag_force/*!emit_store_flag_force*/(mtcsExpmed,target, NE, ret, const0_rtx, mode, 0, 1);
         gcc_assert (ret);
      }
   }

   return ret;
}


/* This function expands the atomic load operation:
   return the atomically loaded value in MEM.

   MEMMODEL is the memory model variant to use.
   TARGET is an option place to stick the return value.  */
//原型 expand_atomic_load optabs.h optabs.cc
rtx mtcs_optabs_expand_atomic_load (MtcsOptabs *self,rtx target, rtx mem, enum memmodel model)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   machine_mode mode = GET_MODE (mem);
   enum insn_code icode;

   /* If the target supports the load directly, great.  */
   icode =  mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,atomic_load_optab, mode);
   if (icode != CODE_FOR_nothing){
      class expand_operand ops[3];
      rtx_insn *last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
      if (is_mm_seq_cst (model))
         expand_memory_blockage(self);

      create_output_operand (&ops[0], target, mode);
      create_fixed_operand (&ops[1], mem);
      mtcs_optabs_create_integer_operand/*!create_integer_operand*/(self,&ops[2], model);
      if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(self,icode, 3, ops)){
         if (!is_mm_relaxed (model))
            expand_memory_blockage(self);
         return ops[0].value;
      }
      mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
   }

   /* If the size of the object is greater than word size on this target,
   then we assume that a load will not be atomic.  We could try to
   emulate a load with a compare-and-swap operation, but the store that
   doing this could result in would be incorrect if this is a volatile
   atomic load or targetting read-only-mapped memory.  */
   if (maybe_gt (GET_MODE_PRECISION (mode), BITS_PER_WORD))
      /* If there is no atomic load, leave the library call.  */
      return NULL_RTX;

   /* Otherwise assume loads are atomic, and emit the proper barriers.  */
   if (!target || target == const0_rtx)
      target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

   /* For SEQ_CST, emit a barrier before the load.  */
   if (is_mm_seq_cst (model))
      mtcs_optabs_expand_mem_thread_fence/*!expand_mem_thread_fence*/(self,model);

   mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, mem);
   /* Emit the appropriate barrier after the load.  */
   mtcs_optabs_expand_mem_thread_fence/*!expand_mem_thread_fence*/(self,model);
   return target;
}


/* Emit a conditional negate or bitwise complement using the
   negcc or notcc optabs if available.  Return NULL_RTX if such operations
   are not available.  Otherwise return the RTX holding the result.
   TARGET is the desired destination of the result.  COMP is the comparison
   on which to negate.  If COND is true move into TARGET the negation
   or bitwise complement of OP1.  Otherwise move OP2 into TARGET.
   CODE is either NEG or NOT.  MODE is the machine mode in which the
   operation is performed.  */
//原型 emit_conditional_neg_or_complement optabs.h optabs.cc
rtx mtcs_optabs_emit_conditional_neg_or_complement (MtcsOptabs *self,rtx target, rtx_code code,
                 machine_mode mode, rtx cond, rtx op1,rtx op2)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   optab op = unknown_optab;
   if (code == NEG)
      op = negcc_optab;
   else if (code == NOT)
      op = notcc_optab;
   else
      gcc_unreachable ();

   insn_code icode =mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,op, mode);

   if (icode == CODE_FOR_nothing)
      return NULL_RTX;

   if (!target)
      target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

   rtx_insn *last =mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
   class expand_operand ops[4];

   create_output_operand (&ops[0], target, mode);
   create_fixed_operand (&ops[1], cond);
   create_input_operand (&ops[2], op1, mode);
   create_input_operand (&ops[3], op2, mode);

   if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(self,icode, 4, ops)){
      if (ops[0].value != target)
         mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,target, ops[0].value, false);
      return target;
   }
   mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
   return NULL_RTX;
}


/* Emit a conditional addition instruction if the machine supports one for that
   condition and machine mode.

   OP0 and OP1 are the operands that should be compared using CODE.  CMODE is
   the mode to use should they be constants.  If it is VOIDmode, they cannot
   both be constants.

   OP2 should be stored in TARGET if the comparison is false, otherwise OP2+OP3
   should be stored there.  MODE is the mode to use should they be constants.
   If it is VOIDmode, they cannot both be constants.

   The result is either TARGET (perhaps modified) or NULL_RTX if the operation
   is not supported.  */
//原型 emit_conditional_add optabs.h optabs.cc
rtx mtcs_optabs_emit_conditional_add (MtcsOptabs *self,rtx target, enum rtx_code code, rtx op0, rtx op1,
            machine_mode cmode, rtx op2, rtx op3, machine_mode mode, int unsignedp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   rtx comparison;
   rtx_insn *last;
   enum insn_code icode;

   /* If one operand is constant, make it the second one.  Only do this
   if the other operand is not constant as well.  */

   if (mtcs_rtlanal_swap_commutative_operands_p/*!swap_commutative_operands_p*/(mtcsRtlanal, op0, op1)){
      std::swap (op0, op1);
      code = swap_condition (code);
   }

   /* get_condition will prefer to generate LT and GT even if the old
   comparison was against zero, so undo that canonicalization here since
   comparisons against zero are cheaper.  */
   if (code == LT && op1 == const1_rtx)
      code = LE, op1 = const0_rtx;
   else if (code == GT && op1 == constm1_rtx)
      code = GE, op1 = const0_rtx;

   if (cmode == VOIDmode)
      cmode = GET_MODE (op0);

   if (mode == VOIDmode)
      mode = GET_MODE (op2);

   icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,addcc_optab, mode);

   if (icode == CODE_FOR_nothing)
      return 0;

   if (!target)
      target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

   code = unsignedp ? unsigned_condition (code) : code;
   comparison = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,code, VOIDmode, cmode, op0, op1);

   /* We can get const0_rtx or const_true_rtx in some circumstances.  Just
   return NULL and let the caller figure out how best to deal with this
   situation.  */
   if (!COMPARISON_P (comparison))
      return NULL_RTX;

   mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
   last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
   prepare_cmp_insn(self,XEXP (comparison, 0), XEXP (comparison, 1),
         GET_CODE (comparison), NULL_RTX, unsignedp, OPTAB_WIDEN, &comparison, &cmode);
   if (comparison){
      class expand_operand ops[4];

      create_output_operand (&ops[0], target, mode);
      create_fixed_operand (&ops[1], comparison);
      create_input_operand (&ops[2], op2, mode);
      create_input_operand (&ops[3], op3, mode);
      if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(self,icode, 4, ops)){
         if (ops[0].value != target)
            mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,target, ops[0].value, false);
         return target;
      }
   }
   mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
   return NULL_RTX;
}

/* Emit code to compute the one's complement absolute value of OP0
   (if (OP0 < 0) OP0 = ~OP0), with result to TARGET if convenient.
   (TARGET may be NULL_RTX.)  The return value says where the result
   actually is to be found.

   MODE is the mode of the operand; the mode of the result is
   different but can be deduced from MODE.  */
//原型 expand_one_cmpl_abs_nojump optabs.h optabs.cc
rtx mtcs_optabs_expand_one_cmpl_abs_nojump (MtcsOptabs *self,machine_mode mode, rtx op0, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
   MtcsExpmed   *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);

   rtx temp;

   /* Not applicable for floating point modes.  */
   if (mtcs_mode_is_float_p/*!FLOAT_MODE_P*/(mtcsMode,mode))
      return NULL_RTX;

   /* If we have a MAX insn, we can do this as MAX (x, ~x).  */
   if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,smax_optab, mode) != CODE_FOR_nothing){
      rtx_insn *last =mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);

      temp = mtcs_optabs_expand_unop/*!expand_unop*/(self,mode, one_cmpl_optab, op0, NULL_RTX, 0);
      if (temp != 0)
         temp = mtcs_optabs_expand_binop/*!expand_binop*/(self,mode, smax_optab, op0, temp, target, 0,OPTAB_WIDEN);

      if (temp != 0)
         return temp;

      mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
   }
   /* If this machine has expensive jumps, we can do one's complement
   absolute value of X as (((signed) x >> (W-1)) ^ x).  */
   scalar_int_mode int_mode;
   if (mtcs_mode_is_int_mode/*!is_int_mode*/(mtcsMode,mode, &int_mode)
   && BRANCH_COST (optimize_insn_for_speed_p (),false) >= 2){
      rtx extended = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, int_mode, op0,
            mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,int_mode) - 1,NULL_RTX, 0);
      temp =mtcs_optabs_expand_binop/*!expand_binop*/(self,int_mode, xor_optab, extended, op0, target, 0,OPTAB_LIB_WIDEN);
      if (temp != 0)
         return temp;
   }
   return NULL_RTX;
}

/* Generate insns to trap with code TCODE if OP1 and OP2 satisfy condition
   CODE.  Return 0 on failure.  */
//原型 gen_cond_trap optabs.h optabs.cc
rtx_insn * mtcs_optabs_gen_cond_trap (MtcsOptabs *self,enum rtx_code code, rtx op1, rtx op2, rtx tcode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);

   machine_mode mode = GET_MODE (op1);
   enum insn_code icode;
   rtx_insn *insn;
   rtx trap_rtx;

   if (mode == VOIDmode)
      return 0;

   icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,ctrap_optab, mode);
   if (icode == CODE_FOR_nothing)
      return 0;

   /* Some targets only accept a zero trap code.  */
   if (!mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(self,icode, 3, tcode))
      return 0;

   mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
   mtcs_emit_start_sequence/*start_sequence*/(mtcsEmit);
   prepare_cmp_insn(self,op1, op2, code, NULL_RTX, false, OPTAB_DIRECT, &trap_rtx, &mode);
   if (!trap_rtx)
      insn = NULL;
   else
      insn = MTCS_GEN_FCN/*!GEN_FCN*/(icode)(trap_rtx, XEXP (trap_rtx, 0), XEXP (trap_rtx, 1),tcode);

   /* If that failed, then give up.  */
   if (insn == 0){
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      return 0;
   }

   mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,insn);
   insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   return insn;
}

/* Generate and return an insn body to add r1 and c,
   storing the result in r0.  */
//原型 gen_add3_insn optabs.h optabs.cc
rtx_insn * mtcs_optabs_gen_add3_insn (MtcsOptabs *self,rtx r0, rtx r1, rtx c)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);

   enum insn_code icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,add_optab, GET_MODE (r0));

   if (icode == CODE_FOR_nothing
   || !mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(self,icode, 0, r0)
   || !mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(self,icode, 1, r1)
   || !mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(self,icode, 2, c))
      return NULL;

   return MTCS_GEN_FCN/*!GEN_FCN*/(icode) (r0, r1, c);
}

/* Generate asm volatile("" : : : "memory") as a memory blockage, at the
   same time clobbering the register set specified by REGS.  */
//原型 expand_asm_reg_clobber_mem_blockage optabs.h optabs.cc
void mtcs_optabs_expand_asm_reg_clobber_mem_blockage (MtcsOptabs *self,HardRegSet *regs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit  *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsReg  *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   rtx asm_op, clob_mem;

   unsigned int num_of_regs = 0;
   for (unsigned int i = 0; i <  mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i++)
      if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(regs, i))
         num_of_regs++;

   asm_op = gen_rtx_ASM_OPERANDS (VOIDmode, "", "", 0, rtvec_alloc (0), rtvec_alloc (0), rtvec_alloc (0), UNKNOWN_LOCATION);
   MEM_VOLATILE_P (asm_op) = 1;

   rtvec v = rtvec_alloc (num_of_regs + 2);

   clob_mem = gen_rtx_SCRATCH (VOIDmode);
   clob_mem = gen_rtx_MEM (BLKmode, clob_mem);
   clob_mem = gen_rtx_CLOBBER (VOIDmode, clob_mem);

   RTVEC_ELT (v, 0) = asm_op;
   RTVEC_ELT (v, 1) = clob_mem;

   if (num_of_regs > 0){
      unsigned int j = 2;
      for (unsigned int i = 0; i <  mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i++)
         if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(regs, i)){
            RTVEC_ELT (v, j) = gen_rtx_CLOBBER (VOIDmode, regno_reg_rtx[i]);
            j++;
         }
      gcc_assert (j == (num_of_regs + 2));
   }

   mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_rtx_PARALLEL (VOIDmode, v));
}

/* Generate code to perform an operation specified by BINOPTAB
   on operands OP0 and OP1, with two results to TARG1 and TARG2.
   We assume that the order of the operands for the instruction
   is TARG0, OP0, OP1, TARG1, which would fit a pattern like
   [(set TARG0 (operate OP0 OP1)) (set TARG1 (operate ...))].

   Either TARG0 or TARG1 may be zero, but what that means is that
   the result is not actually wanted.  We will generate it into
   a dummy pseudo-reg and discard it.  They may not both be zero.

   Returns true if this operation can be performed; false if not.  */
//原型 expand_twoval_binop optabs.h optabs.cc
bool mtcs_optabs_expand_twoval_binop (MtcsOptabs *self,optab binoptab, rtx op0, rtx op1, rtx targ0, rtx targ1,
           int unsignedp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);

   machine_mode mode = GET_MODE (targ0 ? targ0 : targ1);
   enum mode_class mclass;
   machine_mode wider_mode;
   rtx_insn *entry_last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
   rtx_insn *last;

   mclass = mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode);

   if (!targ0)
      targ0 = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
   if (!targ1)
      targ1 = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

   /* Record where to go back to if we fail.  */
   last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);

   if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,binoptab, mode) != CODE_FOR_nothing){
      class expand_operand ops[4];
      enum insn_code icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,binoptab, mode);
      machine_mode mode0 = mtcsOutput->insn_data[icode].operand[1].mode;
      machine_mode mode1 = mtcsOutput->insn_data[icode].operand[2].mode;
      rtx xop0 = op0, xop1 = op1;

      /* If we are optimizing, force expensive constants into a register.  */
      xop0 = avoid_expensive_constant(self,mode0, binoptab, 0, xop0, unsignedp);
      xop1 = avoid_expensive_constant(self,mode1, binoptab, 1, xop1, unsignedp);

      create_fixed_operand (&ops[0], targ0);
      create_convert_operand_from (&ops[1], xop0, mode, unsignedp);
      create_convert_operand_from (&ops[2], xop1, mode, unsignedp);
      create_fixed_operand (&ops[3], targ1);

      if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(self,icode, 4, ops))
         return true;
      mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
   }

   /* It can't be done in this mode.  Can we do it in a wider mode?  */

   if (CLASS_HAS_WIDER_MODES_P (mclass)){
      MTCS_FOR_EACH_WIDER_MODE/*!FOR_EACH_WIDER_MODE*/(mtcsMode,wider_mode, mode){
         if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,binoptab, wider_mode) != CODE_FOR_nothing){
            rtx t0 = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,wider_mode);
            rtx t1 = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,wider_mode);
            rtx cop0 = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,wider_mode, mode, op0, unsignedp);
            rtx cop1 = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,wider_mode, mode, op1, unsignedp);

            if (mtcs_optabs_expand_twoval_binop/*!expand_twoval_binop*/(self,binoptab, cop0, cop1,t0, t1, unsignedp)){
               mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,targ0, t0, unsignedp);
               mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,targ1, t1, unsignedp);
               return true;
            }else
               mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
         }
      }
   }

   mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,entry_last);
   return false;
}

/* Expand a binary operator which has both signed and unsigned forms.
   UOPTAB is the optab for unsigned operations, and SOPTAB is for
   signed operations.

   If we widen unsigned operands, we may use a signed wider operation instead
   of an unsigned wider operation, since the result would be the same.  */
//原型 sign_expand_binop optabs.h optabs.cc
rtx mtcs_optabs_sign_expand_binop (MtcsOptabs *self,machine_mode mode, optab uoptab, optab soptab,
         rtx op0, rtx op1, rtx target, int unsignedp,int methods/*!enum methods*/)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit  *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);

   rtx temp;
   optab direct_optab = unsignedp ? uoptab : soptab;
   bool save_enable;

   /* Do it without widening, if possible.  */
   temp = mtcs_optabs_expand_binop/*!expand_binop*/(self,mode, direct_optab, op0, op1, target,
   unsignedp, OPTAB_DIRECT);
   if (temp || methods == OPTAB_DIRECT)
      return temp;

   /* Try widening to a signed int.  Disable any direct use of any
   signed insn in the current mode.  */
   save_enable = mtcs_opinit_swap_optab_enable/*!swap_optab_enable*/(mtcsOpinit,soptab, mode, false);

   temp = mtcs_optabs_expand_binop/*!expand_binop*/(self,mode, soptab, op0, op1, target,unsignedp, OPTAB_WIDEN);

   /* For unsigned operands, try widening to an unsigned int.  */
   if (!temp && unsignedp)
      temp = mtcs_optabs_expand_binop/*!expand_binop*/(self,mode, uoptab, op0, op1, target,unsignedp, OPTAB_WIDEN);
   if (temp || methods == OPTAB_WIDEN)
      goto egress;

   /* Use the right width libcall if that exists.  */
   temp = mtcs_optabs_expand_binop/*!expand_binop*/(self,mode, direct_optab, op0, op1, target,unsignedp, OPTAB_LIB);
   if (temp || methods == OPTAB_LIB)
      goto egress;

   /* Must widen and use a libcall, use either signed or unsigned.  */
   temp = mtcs_optabs_expand_binop/*!expand_binop*/(self,mode, soptab, op0, op1, target,unsignedp, methods);
   if (!temp && unsignedp)
      temp = mtcs_optabs_expand_binop/*!expand_binop*/(self,mode, uoptab, op0, op1, target,unsignedp, methods);

egress:
   /* Undo the fiddling above.  */
   if (save_enable)
      mtcs_opinit_swap_optab_enable/*!swap_optab_enable*/(mtcsOpinit,soptab, mode, true);
   return temp;
}

MtcsOptabs *mtcs_optabs_new(MtcsMode *mtcsMode)
{
     MtcsOptabs *self = n_slice_alloc0 (sizeof(MtcsOptabs));
     mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
     mtcsOptabsInit(self);
     return self;
}


