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
 * base on loop-iv.cc
 */


#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "rtl.h"
#include "df.h"
#include "memmodel.h"
#include "emit-rtl.h"
#include "diagnostic-core.h"
#include "cfgloop.h"
#include "intl.h"
#include "dumpfile.h"
#include "rtl-iter.h"
#include "tree-ssa-loop-niter.h"
#include "regs.h"
#include "function-abi.h"
#include "tree.h"

#include "mtcsdfcore.h"
#include "mtcsdfproblems.h"
#include "mtcstarget.h"
#include "mtcsprintrtl.h"
#include "mtcsloopiv.h"

/* Possible return values of iv_get_reaching_def.  */

enum iv_grd_result
{
  /* More than one reaching def, or reaching def that does not
     dominate the use.  */
  GRD_INVALID,

  /* The use is trivial invariant of the loop, i.e. is not changed
     inside the loop.  */
  GRD_INVARIANT,

  /* The use is reached by initial value and a value from the
     previous iteration.  */
  GRD_MAYBE_BIV,

  /* The use has single dominating def.  */
  GRD_SINGLE_DOM
};

/* Information about a biv.  */

class biv_entry
{
public:
  unsigned regno; /* The register of the biv.  */
  class rtx_iv iv;   /* Value of the biv.  */
};

/* Induction variable stored at the reference.  */
#define DF_REF_IV(REF) self->iv_ref_table[DF_REF_ID (REF)]
#define DF_REF_IV_SET(REF, IV) self->iv_ref_table[DF_REF_ID (REF)] = (IV)

/* Hashtable helper.  */
struct mtcs_biv_entry_hasher : free_ptr_hash <biv_entry>
{
  typedef rtx_def *compare_type;
  static inline hashval_t hash (const biv_entry *);
  static inline bool equal (const biv_entry *, const rtx_def *);
};

/* Returns hash value for biv B.  */

inline hashval_t mtcs_biv_entry_hasher::hash (const biv_entry *b)
{
  return b->regno;
}

/* Compares biv B and register R.  */
inline bool mtcs_biv_entry_hasher::equal (const biv_entry *b, const rtx_def *r)
{
  return b->regno == REGNO (r);
}

static bool iv_analyze_op (MtcsLoopIv *self,rtx_insn *, scalar_int_mode, rtx, class rtx_iv *);

/* Return the RTX code corresponding to the IV extend code EXTEND.  */
static inline enum rtx_code iv_extend_to_rtx_code (enum iv_extend_code extend)
{
   switch (extend){
      case IV_SIGN_EXTEND:
         return SIGN_EXTEND;
      case IV_ZERO_EXTEND:
         return ZERO_EXTEND;
      case IV_UNKNOWN_EXTEND:
         return UNKNOWN;
   }
   gcc_unreachable ();
}

/* Dumps information about IV to FILE.  */
//extern void dump_iv_info (FILE *, class rtx_iv *);
static void dump_iv_info (MtcsLoopIv *self,FILE *file, class rtx_iv *iv)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   if (!iv->base){
      fprintf (file, "not simple");
      return;
   }

   if (iv->step == const0_rtx  && !iv->first_special)
      fprintf (file, "invariant ");

   mtcs_print_rtl/*!print_rtl*/(file, iv->base);
   if (iv->step != const0_rtx){
      fprintf (file, " + ");
      mtcs_print_rtl/*!print_rtl*/(file, iv->step);
      fprintf (file, " * iteration");
   }
   fprintf (file, " (in %s)", mtcs_mode_get_name/*!GET_MODE_NAME*/(mtcsMode,iv->mode));

   if (iv->mode != iv->extend_mode)
      fprintf (file, " %s to %s",
            rtx_name[iv_extend_to_rtx_code (iv->extend)],mtcs_mode_get_name/*!GET_MODE_NAME*/(mtcsMode,iv->extend_mode));

   if (iv->mult != const1_rtx){
      fprintf (file, " * ");
      mtcs_print_rtl/*!print_rtl*/(file, iv->mult);
   }
   if (iv->delta != const0_rtx){
      fprintf (file, " + ");
      mtcs_print_rtl/*!print_rtl*/(file, iv->delta);
   }
   if (iv->first_special)
      fprintf (file, " (first special)");
}

static void check_iv_ref_table_size (MtcsLoopIv *self)
{
   if (self->iv_ref_table_size < DF_DEFS_TABLE_SIZE ()){
      unsigned int new_size = DF_DEFS_TABLE_SIZE () + (DF_DEFS_TABLE_SIZE () / 4);
      self->iv_ref_table = XRESIZEVEC (class rtx_iv *, self->iv_ref_table, new_size);
      memset (&self->iv_ref_table[self->iv_ref_table_size],
            0,(new_size - self->iv_ref_table_size) * sizeof (class rtx_iv *));
      self->iv_ref_table_size = new_size;
   }
}


/* Checks whether REG is a well-behaved register.  */
static bool simple_reg_p (MtcsLoopIv *self,rtx reg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   unsigned r;

   if (GET_CODE (reg) == SUBREG){
      if (!mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,reg))
         return false;
      reg = SUBREG_REG (reg);
   }

   if (!REG_P (reg))
      return false;

   r = REGNO (reg);
   if (mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,r))
      return false;

   if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,GET_MODE (reg)) != MODE_INT)
      return false;

   return true;
}

/* Clears the information about ivs stored in df.  */
static void clear_iv_info (MtcsLoopIv *self)
{
   unsigned i, n_defs = DF_DEFS_TABLE_SIZE ();
   class rtx_iv *iv;

   check_iv_ref_table_size(self);
   for (i = 0; i < n_defs; i++){
      iv = self->iv_ref_table[i];
      if (iv){
         free (iv);
         self->iv_ref_table[i] = NULL;
      }
   }
   self->bivs->empty ();
}


/* Prepare the data for an induction variable analysis of a LOOP.  */
//原型 iv_analysis_loop_init cfgloop.h loop-iv.cc
void mtcs_loop_iv_iv_analysis_loop_init (MtcsLoopIv *self,class loop *loop)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsDfproblems *mtcsDfproblems=mtcs_target_get_dfproblems(mtcsTarget);

   self->current_loop = loop;

   /* Clear the information from the analysis of the previous loop.  */
   if (self->clean_slate){
      n_debug("mtcsloopiv.c iv_analysis_loop_init 00 因为 clean_slate =true\n");
      mtcs_dfcore_df_set_flags/*!df_set_flags*/(mtcsDfcore,DF_EQ_NOTES + DF_DEFER_INSN_RESCAN);
      self->bivs = new hash_table<mtcs_biv_entry_hasher> (10);
      self->clean_slate = false;
   }else{
      n_debug("mtcsloopiv.c iv_analysis_loop_init 11 因为 clean_slate =false\n");
      clear_iv_info(self);
   }
   /* Get rid of the ud chains before processing the rescans.  Then add
   the problem back.  */
   n_debug("mtcsloopiv.c iv_analysis_loop_init 22 \n");

   mtcs_dfcore_df_remove_problem/*!df_remove_problem*/(mtcsDfcore,df_chain);
   n_debug("mtcsloopiv.c iv_analysis_loop_init 33 \n");

   mtcs_dfscan_df_process_deferred_rescans/*!df_process_deferred_rescans*/(mtcsDfscan);
   n_debug("mtcsloopiv.c iv_analysis_loop_init 44 \n");

   mtcs_dfcore_df_set_flags/*!df_set_flags*/(mtcsDfcore,DF_RD_PRUNE_DEAD_DEFS);
   n_debug("mtcsloopiv.c iv_analysis_loop_init 55 \n");

   mtcs_dfproblems_df_chain_add_problem/*!df_chain_add_problem*/(mtcsDfproblems,DF_UD_CHAIN);
   n_debug("mtcsloopiv.c iv_analysis_loop_init 66 \n");

   mtcs_dfproblems_df_note_add_problem/*!df_note_add_problem*/(mtcsDfproblems);
   n_debug("mtcsloopiv.c iv_analysis_loop_init 77 \n");

   mtcs_dfcore_df_analyze_loop/*!df_analyze_loop*/(mtcsDfcore,loop);
   if (dump_file)
      mtcs_dfcore_df_dump_region/*!df_dump_region*/(mtcsDfcore,dump_file);
   n_debug("mtcsloopiv.c iv_analysis_loop_init 88 \n");

   check_iv_ref_table_size(self);
}

/* Finds the definition of REG that dominates loop latch and stores
   it to DEF.  Returns false if there is not a single definition
   dominating the latch.  If REG has no definition in loop, DEF
   is set to NULL and true is returned.  */
static bool latch_dominating_def (MtcsLoopIv *self,rtx reg, df_ref *def)
{
   df_ref single_rd = NULL, adef;
   unsigned regno = REGNO (reg);
   class df_rd_bb_info *bb_info = DF_RD_BB_INFO (self->current_loop->latch);

   for (adef = DF_REG_DEF_CHAIN (regno); adef; adef = DF_REF_NEXT_REG (adef)){
      if (!bitmap_bit_p (df->blocks_to_analyze, DF_REF_BBNO (adef))
      || !bitmap_bit_p (&bb_info->out, DF_REF_ID (adef)))
         continue;

      /* More than one reaching definition.  */
      if (single_rd)
         return false;

      if (!just_once_each_iteration_p (self->current_loop, DF_REF_BB (adef)))
         return false;

      single_rd = adef;
   }

   *def = single_rd;
   return true;
}

/* Gets definition of REG reaching its use in INSN and stores it to DEF.  */

static enum iv_grd_result iv_get_reaching_def (MtcsLoopIv *self,rtx_insn *insn, rtx reg, df_ref *def)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);

   df_ref use, adef;
   basic_block def_bb, use_bb;
   rtx_insn *def_insn;
   bool dom_p;

   *def = NULL;
   if (!simple_reg_p(self,reg))
      return GRD_INVALID;
   if (GET_CODE (reg) == SUBREG)
      reg = SUBREG_REG (reg);
   gcc_assert (REG_P (reg));

   use = mtcs_dfcore_df_find_use/*!df_find_use*/(mtcsDfcore,insn, reg);
   gcc_assert (use != NULL);

   if (!DF_REF_CHAIN (use))
      return GRD_INVARIANT;

   /* More than one reaching def.  */
   if (DF_REF_CHAIN (use)->next)
      return GRD_INVALID;

   adef = DF_REF_CHAIN (use)->ref;

   /* We do not handle setting only part of the register.  */
   if (DF_REF_FLAGS (adef) & DF_REF_READ_WRITE)
      return GRD_INVALID;

   def_insn = DF_REF_INSN (adef);
   def_bb = DF_REF_BB (adef);
   use_bb = BLOCK_FOR_INSN (insn);

   if (use_bb == def_bb)
      dom_p = (DF_INSN_LUID (def_insn) < DF_INSN_LUID (insn));
   else
      dom_p = dominated_by_p (CDI_DOMINATORS, use_bb, def_bb);

   if (dom_p){
      *def = adef;
      return GRD_SINGLE_DOM;
   }

   /* The definition does not dominate the use.  This is still OK if
   this may be a use of a biv, i.e. if the def_bb dominates loop
   latch.  */
   if (just_once_each_iteration_p (self->current_loop, def_bb))
      return GRD_MAYBE_BIV;

   return GRD_INVALID;
}

/* Sets IV to invariant CST in MODE.  Always returns true (just for
   consistency with other iv manipulation functions that may fail).  */

static bool iv_constant (class rtx_iv *iv, scalar_int_mode mode, rtx cst)
{
   iv->mode = mode;
   iv->base = cst;
   iv->step = const0_rtx;
   iv->first_special = false;
   iv->extend = IV_UNKNOWN_EXTEND;
   iv->extend_mode = iv->mode;
   iv->delta = const0_rtx;
   iv->mult = const1_rtx;

   return true;
}

/* Evaluates application of subreg to MODE on IV.  */

static bool iv_subreg (MtcsLoopIv *self,class rtx_iv *iv, scalar_int_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

   /* If iv is invariant, just calculate the new value.  */
   if (iv->step == const0_rtx  && !iv->first_special){
      rtx val = mtcs_loop_iv_get_iv_value/*!get_iv_value*/(self,iv, const0_rtx);
      val = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,
            mode, val,iv->extend == IV_UNKNOWN_EXTEND ? iv->mode : iv->extend_mode);

      iv->base = val;
      iv->extend = IV_UNKNOWN_EXTEND;
      iv->mode = iv->extend_mode = mode;
      iv->delta = const0_rtx;
      iv->mult = const1_rtx;
      return true;
   }

   if (iv->extend_mode == mode)
      return true;

   if (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) >
         mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,iv->mode))
      return false;

   iv->extend = IV_UNKNOWN_EXTEND;
   iv->mode = mode;

   iv->base = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,
         PLUS, iv->extend_mode, iv->delta, mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,
               MULT, iv->extend_mode,iv->base, iv->mult));
   iv->step = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MULT, iv->extend_mode, iv->step, iv->mult);
   iv->mult = const1_rtx;
   iv->delta = const0_rtx;
   iv->first_special = false;

   return true;
}

/* Evaluates application of EXTEND to MODE on IV.  */

static bool iv_extend (MtcsLoopIv *self,class rtx_iv *iv, enum iv_extend_code extend, scalar_int_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

   /* If iv is invariant, just calculate the new value.  */
   if (iv->step == const0_rtx  && !iv->first_special){
      rtx val = mtcs_loop_iv_get_iv_value/*!get_iv_value*/(self,iv, const0_rtx);
      if (iv->extend_mode != iv->mode
      && iv->extend != IV_UNKNOWN_EXTEND
      && iv->extend != extend)
         val = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,iv->mode, val, iv->extend_mode);
      val = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,
            iv_extend_to_rtx_code (extend), mode,val,iv->extend == extend ? iv->extend_mode : iv->mode);
      iv->base = val;
      iv->extend = IV_UNKNOWN_EXTEND;
      iv->mode = iv->extend_mode = mode;
      iv->delta = const0_rtx;
      iv->mult = const1_rtx;
      return true;
   }

   if (mode != iv->extend_mode)
      return false;

   if (iv->extend != IV_UNKNOWN_EXTEND && iv->extend != extend)
      return false;

   iv->extend = extend;

   return true;
}

/* Evaluates negation of IV.  */
static bool iv_neg (MtcsLoopIv *self,class rtx_iv *iv)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

   if (iv->extend == IV_UNKNOWN_EXTEND){
      iv->base = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,
            NEG, iv->extend_mode,iv->base, iv->extend_mode);
      iv->step = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,
            NEG, iv->extend_mode, iv->step, iv->extend_mode);
   }else{
      iv->delta = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,
            NEG, iv->extend_mode,iv->delta, iv->extend_mode);
      iv->mult = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,
            NEG, iv->extend_mode, iv->mult, iv->extend_mode);
   }

   return true;
}

/* Evaluates addition or subtraction (according to OP) of IV1 to IV0.  */

static bool iv_add (MtcsLoopIv *self,class rtx_iv *iv0, class rtx_iv *iv1, enum rtx_code op)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

   scalar_int_mode mode;
   rtx arg;

   /* Extend the constant to extend_mode of the other operand if necessary.  */
   if (iv0->extend == IV_UNKNOWN_EXTEND
   && iv0->mode == iv0->extend_mode
   && iv0->step == const0_rtx
   && mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,iv0->extend_mode) <
   mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,iv1->extend_mode)){
      iv0->extend_mode = iv1->extend_mode;
      iv0->base = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,
            ZERO_EXTEND, iv0->extend_mode,iv0->base, iv0->mode);
   }
   if (iv1->extend == IV_UNKNOWN_EXTEND
   && iv1->mode == iv1->extend_mode
   && iv1->step == const0_rtx
   && mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,iv1->extend_mode) <
   mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,iv0->extend_mode)){
      iv1->extend_mode = iv0->extend_mode;
      iv1->base = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,
            ZERO_EXTEND, iv1->extend_mode,iv1->base, iv1->mode);
   }

   mode = iv0->extend_mode;
   if (mode != iv1->extend_mode)
      return false;

   if (iv0->extend == IV_UNKNOWN_EXTEND && iv1->extend == IV_UNKNOWN_EXTEND){
      if (iv0->mode != iv1->mode)
         return false;

      iv0->base = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,op, mode, iv0->base, iv1->base);
      iv0->step = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,op, mode, iv0->step, iv1->step);

      return true;
   }

   /* Handle addition of constant.  */
   if (iv1->extend == IV_UNKNOWN_EXTEND  && iv1->mode == mode  && iv1->step == const0_rtx){
      iv0->delta = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,op, mode, iv0->delta, iv1->base);
      return true;
   }

   if (iv0->extend == IV_UNKNOWN_EXTEND  && iv0->mode == mode  && iv0->step == const0_rtx){
      arg = iv0->base;
      *iv0 = *iv1;
      if (op == MINUS && !iv_neg(self,iv0))
         return false;

      iv0->delta = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, mode, iv0->delta, arg);
      return true;
   }

   return false;
}

/* Evaluates multiplication of IV by constant CST.  */
static bool iv_mult (MtcsLoopIv *self,class rtx_iv *iv, rtx mby)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

   scalar_int_mode mode = iv->extend_mode;

   if (GET_MODE (mby) != VOIDmode && GET_MODE (mby) != mode)
      return false;

   if (iv->extend == IV_UNKNOWN_EXTEND){
      iv->base = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MULT, mode, iv->base, mby);
      iv->step = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MULT, mode, iv->step, mby);
   }else{
      iv->delta = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MULT, mode, iv->delta, mby);
      iv->mult = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MULT, mode, iv->mult, mby);
   }

   return true;
}

/* Evaluates shift of IV by constant CST.  */

static bool iv_shift (MtcsLoopIv *self,class rtx_iv *iv, rtx mby)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

   scalar_int_mode mode = iv->extend_mode;

   if (GET_MODE (mby) != VOIDmode  && GET_MODE (mby) != mode)
      return false;

   if (iv->extend == IV_UNKNOWN_EXTEND){
      iv->base = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,ASHIFT, mode, iv->base, mby);
      iv->step = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,ASHIFT, mode, iv->step, mby);
   }else{
      iv->delta = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,ASHIFT, mode, iv->delta, mby);
      iv->mult = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,ASHIFT, mode, iv->mult, mby);
   }

   return true;
}

/* The recursive part of get_biv_step.  Gets the value of the single value
   defined by DEF wrto initial value of REG inside loop, in shape described
   at get_biv_step.  */
static bool get_biv_step_1 (MtcsLoopIv *self,df_ref def, scalar_int_mode outer_mode, rtx reg,
      rtx *inner_step, scalar_int_mode *inner_mode,  enum iv_extend_code *extend, rtx *outer_step)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   rtx set, rhs, op0 = NULL_RTX, op1 = NULL_RTX;
   rtx next, nextr;
   enum rtx_code code, prev_code = UNKNOWN;
   rtx_insn *insn = DF_REF_INSN (def);
   df_ref next_def;
   enum iv_grd_result res;

   set = single_set (insn);
   if (!set)
      return false;

   rhs = find_reg_equal_equiv_note (insn);
   if (rhs)
      rhs = XEXP (rhs, 0);
   else
      rhs = SET_SRC (set);

   code = GET_CODE (rhs);
   switch (code){
      case SUBREG:
      case REG:
         next = rhs;
         break;

      case PLUS:
      case MINUS:
         op0 = XEXP (rhs, 0);
         op1 = XEXP (rhs, 1);

         if (code == PLUS && CONSTANT_P (op0))
            std::swap (op0, op1);

         if (!simple_reg_p(self,op0) || !CONSTANT_P (op1))
            return false;

         if (GET_MODE (rhs) != outer_mode){
            /* ppc64 uses expressions like

            (set x:SI (plus:SI (subreg:SI y:DI) 1)).

            this is equivalent to

            (set x':DI (plus:DI y:DI 1))
            (set x:SI (subreg:SI (x':DI)).  */
            if (GET_CODE (op0) != SUBREG)
               return false;
            if (GET_MODE (SUBREG_REG (op0)) != outer_mode)
               return false;
         }

         next = op0;
         break;

      case SIGN_EXTEND:
      case ZERO_EXTEND:
         if (GET_MODE (rhs) != outer_mode)
            return false;

         op0 = XEXP (rhs, 0);

         /* rv64 wraps SImode arithmetic inside an extension to DImode.
         This matches the actual hardware semantics.  So peek inside
         the extension and see if we have simple arithmetic that we
         can analyze.  */
         if (GET_CODE (op0) == PLUS){
            rhs = op0;
            op0 = XEXP (rhs, 0);
            op1 = XEXP (rhs, 1);

            if (CONSTANT_P (op0))
               std::swap (op0, op1);

            if (!simple_reg_p(self,op0) || !CONSTANT_P (op1))
               return false;

            op1 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,code, outer_mode, op1, GET_MODE (rhs));
            prev_code = code;
            code = PLUS;
         }

         if (!simple_reg_p(self,op0))
            return false;

         next = op0;
         break;

      default:
         return false;
   }

   if (GET_CODE (next) == SUBREG){
      if (!mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,next))
         return false;

      nextr = SUBREG_REG (next);
      if (GET_MODE (nextr) != outer_mode)
         return false;
   }else
      nextr = next;

   res = iv_get_reaching_def(self,insn, nextr, &next_def);

   if (res == GRD_INVALID || res == GRD_INVARIANT)
      return false;

   if (res == GRD_MAYBE_BIV){
      if (!rtx_equal_p (nextr, reg))
         return false;

      *inner_step = const0_rtx;
      *extend = IV_UNKNOWN_EXTEND;
      *inner_mode = outer_mode;
      *outer_step = const0_rtx;
   }else if (!get_biv_step_1(self,next_def, outer_mode, reg,inner_step, inner_mode, extend,outer_step))
      return false;

   if (GET_CODE (next) == SUBREG){
      scalar_int_mode amode;
      if (!mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (next), &amode)
      || mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,amode) > mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,*inner_mode))
         return false;

      *inner_mode = amode;
      *inner_step = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, outer_mode, *inner_step, *outer_step);
      *outer_step = const0_rtx;
      *extend = IV_UNKNOWN_EXTEND;
   }

   switch (code){
      case REG:
      case SUBREG:
         break;

      case PLUS:
      case MINUS:
         if (*inner_mode == outer_mode
         /* See comment in previous switch.  */
         || GET_MODE (rhs) != outer_mode)
            *inner_step = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,code, outer_mode, *inner_step, op1);
         else
            *outer_step = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,code, outer_mode,  *outer_step, op1);

         if (prev_code == SIGN_EXTEND)
            *extend = IV_SIGN_EXTEND;
         else if (prev_code == ZERO_EXTEND)
            *extend = IV_ZERO_EXTEND;
         break;

      case SIGN_EXTEND:
      case ZERO_EXTEND:
         gcc_assert (GET_MODE (op0) == *inner_mode  && *extend == IV_UNKNOWN_EXTEND  && *outer_step == const0_rtx);

         *extend = (code == SIGN_EXTEND) ? IV_SIGN_EXTEND : IV_ZERO_EXTEND;
         break;

      default:
         return false;
   }

   return true;
}

/* Gets the operation on register REG inside loop, in shape

   OUTER_STEP + EXTEND_{OUTER_MODE} (SUBREG_{INNER_MODE} (REG + INNER_STEP))

   If the operation cannot be described in this shape, return false.
   LAST_DEF is the definition of REG that dominates loop latch.  */
static bool get_biv_step (MtcsLoopIv *self,df_ref last_def, scalar_int_mode outer_mode, rtx reg,
         rtx *inner_step, scalar_int_mode *inner_mode, enum iv_extend_code *extend, rtx *outer_step)
{
   if (!get_biv_step_1(self,last_def, outer_mode, reg, inner_step, inner_mode, extend, outer_step))
      return false;

   gcc_assert ((*inner_mode == outer_mode) != (*extend != IV_UNKNOWN_EXTEND));
   gcc_assert (*inner_mode != outer_mode || *outer_step == const0_rtx);

   return true;
}

/* Records information that DEF is induction variable IV.  */
static void record_iv (MtcsLoopIv *self,df_ref def, class rtx_iv *iv)
{
   class rtx_iv *recorded_iv = XNEW (class rtx_iv);

   *recorded_iv = *iv;
   check_iv_ref_table_size(self);
   DF_REF_IV_SET (def, recorded_iv);
}

/* If DEF was already analyzed for bivness, store the description of the biv to
   IV and return true.  Otherwise return false.  */
static bool analyzed_for_bivness_p (MtcsLoopIv *self,rtx def, class rtx_iv *iv)
{
   class biv_entry *biv = self->bivs->find_with_hash (def, REGNO (def));

   if (!biv)
      return false;

   *iv = biv->iv;
   return true;
}

static void record_biv (MtcsLoopIv *self,rtx def, class rtx_iv *iv)
{
   class biv_entry *biv = XNEW (class biv_entry);
   biv_entry **slot = self->bivs->find_slot_with_hash (def, REGNO (def), INSERT);

   biv->regno = REGNO (def);
   biv->iv = *iv;
   gcc_assert (!*slot);
   *slot = biv;
}

/* Determines whether DEF is a biv and if so, stores its description
   to *IV.  OUTER_MODE is the mode of DEF.  */

static bool iv_analyze_biv (MtcsLoopIv *self,scalar_int_mode outer_mode, rtx def, class rtx_iv *iv)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

   rtx inner_step, outer_step;
   scalar_int_mode inner_mode;
   enum iv_extend_code extend;
   df_ref last_def;

   if (dump_file){
      fprintf (dump_file, "Analyzing ");
      mtcs_print_rtl/*!print_rtl*/(dump_file, def);
      fprintf (dump_file, " for bivness.\n");
   }

   if (!REG_P (def)){
      if (!CONSTANT_P (def))
         return false;

      return iv_constant (iv, outer_mode, def);
   }

   if (!latch_dominating_def(self,def, &last_def)){
      if (dump_file)
         fprintf (dump_file, "  not simple.\n");
      return false;
   }

   if (!last_def)
      return iv_constant (iv, outer_mode, def);

   if (analyzed_for_bivness_p(self,def, iv)){
      if (dump_file)
         fprintf (dump_file, "  already analysed.\n");
      return iv->base != NULL_RTX;
   }

   if (!get_biv_step(self,last_def, outer_mode, def, &inner_step, &inner_mode,&extend, &outer_step)){
      iv->base = NULL_RTX;
      goto end;
   }

   /* Loop transforms base to es (base + inner_step) + outer_step,
   where es means extend of subreg between inner_mode and outer_mode.
   The corresponding induction variable is

   es ((base - outer_step) + i * (inner_step + outer_step)) + outer_step  */

   iv->base = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MINUS, outer_mode, def, outer_step);
   iv->step = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, outer_mode, inner_step, outer_step);
   iv->mode = inner_mode;
   iv->extend_mode = outer_mode;
   iv->extend = extend;
   iv->mult = const1_rtx;
   iv->delta = outer_step;
   iv->first_special = inner_mode != outer_mode;

end:
   if (dump_file){
      fprintf (dump_file, "  ");
      dump_iv_info(self,dump_file, iv);
      fprintf (dump_file, "\n");
   }

   record_biv(self,def, iv);
   return iv->base != NULL_RTX;
}

/* Analyzes expression RHS used at INSN and stores the result to *IV.
   The mode of the induction variable is MODE.  */
//原型 iv_analyze_expr cfgloop.h loop-iv.cc
bool mtcs_loop_iv_iv_analyze_expr (MtcsLoopIv *self,rtx_insn *insn, scalar_int_mode mode, rtx rhs,
       class rtx_iv *iv)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   rtx mby = NULL_RTX;
   rtx op0 = NULL_RTX, op1 = NULL_RTX;
   class rtx_iv iv0, iv1;
   enum rtx_code code = GET_CODE (rhs);
   scalar_int_mode omode = mode;

   iv->base = NULL_RTX;
   iv->step = NULL_RTX;

   gcc_assert (GET_MODE (rhs) == mode || GET_MODE (rhs) == VOIDmode);

   if (CONSTANT_P (rhs)  || REG_P (rhs) || code == SUBREG)
      return iv_analyze_op(self,insn, mode, rhs, iv);

   switch (code){
   case REG:
      op0 = rhs;
      break;

   case SIGN_EXTEND:
   case ZERO_EXTEND:
   case NEG:
      op0 = XEXP (rhs, 0);
      /* We don't know how many bits there are in a sign-extended constant.  */
      if (!mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (op0), &omode))
         return false;
      break;

   case PLUS:
   case MINUS:
      op0 = XEXP (rhs, 0);
      op1 = XEXP (rhs, 1);
      break;

   case MULT:
      op0 = XEXP (rhs, 0);
      mby = XEXP (rhs, 1);
      if (!CONSTANT_P (mby))
         std::swap (op0, mby);
      if (!CONSTANT_P (mby))
         return false;
      break;

   case ASHIFT:
      op0 = XEXP (rhs, 0);
      mby = XEXP (rhs, 1);
      if (!CONSTANT_P (mby))
         return false;
      break;

   default:
      return false;
   }

   if (op0  && !mtcs_loop_iv_iv_analyze_expr/*!iv_analyze_expr*/(self,insn, omode, op0, &iv0))
      return false;

   if (op1  && !mtcs_loop_iv_iv_analyze_expr/*!iv_analyze_expr*/(self,insn, omode, op1, &iv1))
      return false;

   switch (code){
      case SIGN_EXTEND:
         if (!iv_extend(self,&iv0, IV_SIGN_EXTEND, mode))
            return false;
         break;

      case ZERO_EXTEND:
         if (!iv_extend(self,&iv0, IV_ZERO_EXTEND, mode))
            return false;
         break;

      case NEG:
         if (!iv_neg(self,&iv0))
            return false;
         break;

      case PLUS:
      case MINUS:
         if (!iv_add(self,&iv0, &iv1, code))
            return false;
         break;

      case MULT:
         if (!iv_mult(self,&iv0, mby))
            return false;
         break;

      case ASHIFT:
         if (!iv_shift(self,&iv0, mby))
            return false;
         break;

      default:
         break;
   }

   *iv = iv0;
   return iv->base != NULL_RTX;
}

/* Analyzes iv DEF and stores the result to *IV.  */
static bool iv_analyze_def (MtcsLoopIv *self,df_ref def, class rtx_iv *iv)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   rtx_insn *insn = DF_REF_INSN (def);
   rtx reg = DF_REF_REG (def);
   rtx set, rhs;

   if (dump_file){
      fprintf (dump_file, "Analyzing def of ");
      mtcs_print_rtl/*!print_rtl*/(dump_file, reg);
      fprintf (dump_file, " in insn ");
      print_rtl_single (dump_file, insn);
   }

   check_iv_ref_table_size(self);
   if (DF_REF_IV (def)){
      if (dump_file)
         fprintf (dump_file, "  already analysed.\n");
      *iv = *DF_REF_IV (def);
      return iv->base != NULL_RTX;
   }

   iv->base = NULL_RTX;
   iv->step = NULL_RTX;

   scalar_int_mode mode;
   if (!REG_P (reg) || !mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (reg), &mode))
      return false;

   set = single_set (insn);
   if (!set)
      return false;

   if (!REG_P (SET_DEST (set)))
      return false;

   gcc_assert (SET_DEST (set) == reg);
   rhs = find_reg_equal_equiv_note (insn);
   if (rhs)
      rhs = XEXP (rhs, 0);
   else
      rhs = SET_SRC (set);

   mtcs_loop_iv_iv_analyze_expr/*!iv_analyze_expr*/(self,insn, mode, rhs, iv);
   record_iv(self,def, iv);

   if (dump_file){
      mtcs_print_rtl/*!print_rtl*/(dump_file, reg);
      fprintf (dump_file, " in insn ");
      print_rtl_single (dump_file, insn);
      fprintf (dump_file, "  is ");
      dump_iv_info(self,dump_file, iv);
      fprintf (dump_file, "\n");
   }

   return iv->base != NULL_RTX;
}

/* Analyzes operand OP of INSN and stores the result to *IV.  MODE is the
   mode of OP.  */
static bool iv_analyze_op (MtcsLoopIv *self,rtx_insn *insn, scalar_int_mode mode, rtx op, class rtx_iv *iv)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReload1 *mtcsReload1=mtcs_target_get_reload1(mtcsTarget);

   df_ref def = NULL;
   enum iv_grd_result res;

   if (dump_file){
      fprintf (dump_file, "Analyzing operand ");
      mtcs_print_rtl/*!print_rtl*/(dump_file, op);
      fprintf (dump_file, " of insn ");
      print_rtl_single (dump_file, insn);
   }

   if (mtcs_reload1_function_invariant_p/*!function_invariant_p*/(mtcsReload1,op))
      res = GRD_INVARIANT;
   else if (GET_CODE (op) == SUBREG){
      scalar_int_mode inner_mode;
      if (!mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,op)
            || !mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (SUBREG_REG (op)), &inner_mode))
         return false;

      if (!iv_analyze_op(self,insn, inner_mode, SUBREG_REG (op), iv))
         return false;

      return iv_subreg(self,iv, mode);
   }else{
      res = iv_get_reaching_def(self,insn, op, &def);
      if (res == GRD_INVALID){
         if (dump_file)
            fprintf (dump_file, "  not simple.\n");
         return false;
      }
   }

   if (res == GRD_INVARIANT){
      iv_constant (iv, mode, op);

      if (dump_file){
         fprintf (dump_file, "  ");
         dump_iv_info(self,dump_file, iv);
         fprintf (dump_file, "\n");
      }
      return true;
   }

   if (res == GRD_MAYBE_BIV)
      return iv_analyze_biv(self,mode, op, iv);

   return iv_analyze_def(self,def, iv);
}

/* Analyzes value VAL at INSN and stores the result to *IV.  MODE is the
   mode of VAL.  */
//原型 iv_analyze cfgloop.h loop-iv.cc
bool mtcs_loop_iv_iv_analyze (MtcsLoopIv *self,rtx_insn *insn, scalar_int_mode mode, rtx val, class rtx_iv *iv)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);

   rtx reg;

   /* We must find the insn in that val is used, so that we get to UD chains.
   Since the function is sometimes called on result of get_condition,
   this does not necessarily have to be directly INSN; scan also the
   following insns.  */
   if (simple_reg_p(self,val)){
      if (GET_CODE (val) == SUBREG)
         reg = SUBREG_REG (val);
      else
         reg = val;

      while (!mtcs_dfcore_df_find_use/*!df_find_use*/(mtcsDfcore,insn, reg))
         insn = NEXT_INSN (insn);
   }

   return iv_analyze_op(self,insn, mode, val, iv);
}

/* Analyzes definition of DEF in INSN and stores the result to IV.  */
//原型 iv_analyze_result cfgloop.h loop-iv.cc
bool mtcs_loop_iv_iv_analyze_result (MtcsLoopIv *self,rtx_insn *insn, rtx def, class rtx_iv *iv)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);

   df_ref adef;

   adef = mtcs_dfcore_df_find_def/*!df_find_def*/(mtcsDfcore,insn, def);
   if (!adef)
      return false;

   return iv_analyze_def(self,adef, iv);
}

/* Checks whether definition of register REG in INSN is a basic induction
   variable.  MODE is the mode of REG.

   IV analysis must have been initialized (via a call to
   iv_analysis_loop_init) for this function to produce a result.  */
//原型 biv_p cfgloop.h loop-iv.cc
bool mtcs_loop_iv_biv_p (MtcsLoopIv *self,rtx_insn *insn, scalar_int_mode mode, rtx reg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);

   class rtx_iv iv;
   df_ref def, last_def;

   if (!simple_reg_p(self,reg))
      return false;

   def = mtcs_dfcore_df_find_def/*!df_find_def*/(mtcsDfcore,insn, reg);
   gcc_assert (def != NULL);
   if (!latch_dominating_def(self,reg, &last_def))
      return false;
   if (last_def != def)
      return false;

   if (!iv_analyze_biv(self,mode, reg, &iv))
      return false;

   return iv.step != const0_rtx;
}

/* Calculates value of IV at ITERATION-th iteration.  */
//原型 get_iv_value cfgloop.h loop-iv.cc
rtx mtcs_loop_iv_get_iv_value (MtcsLoopIv *self,class rtx_iv *iv, rtx iteration)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

   rtx val;

   /* We would need to generate some if_then_else patterns, and so far
   it is not needed anywhere.  */
   gcc_assert (!iv->first_special);

   if (iv->step != const0_rtx && iteration != const0_rtx)
      val = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, iv->extend_mode, iv->base,
            mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MULT, iv->extend_mode,iv->step, iteration));
   else
      val = iv->base;

   if (iv->extend_mode == iv->mode)
      return val;

   val = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,iv->mode, val, iv->extend_mode);

   if (iv->extend == IV_UNKNOWN_EXTEND)
      return val;

   val = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,
         iv_extend_to_rtx_code (iv->extend),iv->extend_mode, val, iv->mode);
   val = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,
         PLUS, iv->extend_mode, iv->delta,mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,
               MULT, iv->extend_mode,iv->mult, val));

   return val;
}

/* Free the data for an induction variable analysis.  */
//原型 iv_analysis_done cfgloop.h loop-iv.cc
void mtcs_loop_iv_iv_analysis_done (MtcsLoopIv *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);

   if (!self->clean_slate){
      clear_iv_info(self);
      self->clean_slate = true;
      mtcs_dfcore_df_finish_pass/*!df_finish_pass*/(mtcsDfcore,true);
      delete self->bivs;
      self->bivs = NULL;
      free (self->iv_ref_table);
      self->iv_ref_table = NULL;
      self->iv_ref_table_size = 0;
   }
}

/* Computes inverse to X modulo (1 << MOD).  */
static uint64_t inverse (uint64_t x, int mod)
{
   uint64_t mask =((uint64_t) 1 << (mod - 1) << 1) - 1;
   uint64_t rslt = 1;
   int i;
   for (i = 0; i < mod - 1; i++){
      rslt = (rslt * x) & mask;
      x = (x * x) & mask;
   }
   return rslt;
}

/* Checks whether any register in X is in set ALT.  */
static bool altered_reg_used (const_rtx x, bitmap alt)
{
   subrtx_iterator::array_type array;
   FOR_EACH_SUBRTX (iter, array, x, NONCONST){
      const_rtx x = *iter;
      if (REG_P (x) && REGNO_REG_SET_P (alt, REGNO (x)))
         return true;
   }
   return false;
}

/* Marks registers altered by EXPR in set ALT.  */
static void mark_altered (rtx expr, const_rtx by ATTRIBUTE_UNUSED, void *alt)
{
   if (GET_CODE (expr) == SUBREG)
      expr = SUBREG_REG (expr);
   if (!REG_P (expr))
      return;

   SET_REGNO_REG_SET ((bitmap) alt, REGNO (expr));
}

/* Checks whether RHS is simple enough to process.  */
static bool simple_rhs_p (MtcsLoopIv *self,rtx rhs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsReload1 *mtcsReload1=mtcs_target_get_reload1(mtcsTarget);

   rtx op0, op1;

   if (mtcs_reload1_function_invariant_p/*!function_invariant_p*/(mtcsReload1,rhs)
   || (REG_P (rhs) && !mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,rhs)))
      return true;

   switch (GET_CODE (rhs)){
      case PLUS:
      case MINUS:
      case AND:
         op0 = XEXP (rhs, 0);
         op1 = XEXP (rhs, 1);
         /* Allow reg OP const and reg OP reg.  */
         if (!(REG_P (op0) && !mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,op0))
               && !mtcs_reload1_function_invariant_p/*!function_invariant_p*/(mtcsReload1,op0))
            return false;
         if (!(REG_P (op1) && !mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,op1))
               && !mtcs_reload1_function_invariant_p/*!function_invariant_p*/(mtcsReload1,op1))
            return false;

         return true;

      case ASHIFT:
      case ASHIFTRT:
      case LSHIFTRT:
      case MULT:
         op0 = XEXP (rhs, 0);
         op1 = XEXP (rhs, 1);
         /* Allow reg OP const.  */
         if (!(REG_P (op0) && !mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,op0)))
            return false;
         if (!mtcs_reload1_function_invariant_p/*!function_invariant_p*/(mtcsReload1,op1))
            return false;

         return true;

      default:
         return false;
   }
}

/* If any registers in *EXPR that have a single definition, try to replace
   them with the known-equivalent values.  */
static void replace_single_def_regs (MtcsLoopIv *self,rtx *expr)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);

   subrtx_var_iterator::array_type array;
repeat:
   FOR_EACH_SUBRTX_VAR (iter, array, *expr, NONCONST){
      rtx x = *iter;
      if (REG_P (x))
         if (rtx new_x = mtcs_dfcore_df_find_single_def_src/*!df_find_single_def_src*/(mtcsDfcore,x)){
            *expr = mtcs_simplify_rtx_simplify_replace_rtx/*!simplify_replace_rtx*/(mtcsSimplifyRtx,*expr, x, new_x);
            goto repeat;
         }
   }
}

/* A subroutine of simplify_using_initial_values, this function examines INSN
   to see if it contains a suitable set that we can use to make a replacement.
   If it is suitable, return true and set DEST and SRC to the lhs and rhs of
   the set; return false otherwise.  */
static bool suitable_set_for_replacement (MtcsLoopIv *self,rtx_insn *insn, rtx *dest, rtx *src)
{
   rtx set = single_set (insn);
   rtx lhs = NULL_RTX, rhs;

   if (!set)
      return false;

   lhs = SET_DEST (set);
   if (!REG_P (lhs))
      return false;

   rhs = find_reg_equal_equiv_note (insn);
   if (rhs)
      rhs = XEXP (rhs, 0);
   else
      rhs = SET_SRC (set);

   if (!simple_rhs_p(self,rhs))
      return false;

   *dest = lhs;
   *src = rhs;
   return true;
}

/* Using the data returned by suitable_set_for_replacement, replace DEST
   with SRC in *EXPR and return the new expression.  Also call
   replace_single_def_regs if the replacement changed something.  */
static void replace_in_expr (MtcsLoopIv *self,rtx *expr, rtx dest, rtx src)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

   rtx old = *expr;
   *expr = mtcs_simplify_rtx_simplify_replace_rtx/*!simplify_replace_rtx*/(mtcsSimplifyRtx,*expr, dest, src);
   if (old == *expr)
      return;
   replace_single_def_regs(self,expr);
}

/* Checks whether A implies B.  */
static bool implies_p (MtcsLoopIv *self,rtx a, rtx b)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

   rtx op0, op1, opb0, opb1;
   machine_mode mode;

   if (rtx_equal_p (a, b))
      return true;

   if (GET_CODE (a) == EQ){
      op0 = XEXP (a, 0);
      op1 = XEXP (a, 1);

      if (REG_P (op0) || (GET_CODE (op0) == SUBREG  && REG_P (SUBREG_REG (op0)))){
         rtx r = mtcs_simplify_rtx_simplify_replace_rtx/*!simplify_replace_rtx*/(mtcsSimplifyRtx,b, op0, op1);
         if (r == const_true_rtx)
            return true;
      }

      if (REG_P (op1) || (GET_CODE (op1) == SUBREG && REG_P (SUBREG_REG (op1)))){
         rtx r = mtcs_simplify_rtx_simplify_replace_rtx/*!simplify_replace_rtx*/(mtcsSimplifyRtx,b, op1, op0);
         if (r == const_true_rtx)
            return true;
      }
   }

   if (b == const_true_rtx)
      return true;

   if ((GET_RTX_CLASS (GET_CODE (a)) != RTX_COMM_COMPARE
   && GET_RTX_CLASS (GET_CODE (a)) != RTX_COMPARE)
   || (GET_RTX_CLASS (GET_CODE (b)) != RTX_COMM_COMPARE
   && GET_RTX_CLASS (GET_CODE (b)) != RTX_COMPARE))
      return false;

   op0 = XEXP (a, 0);
   op1 = XEXP (a, 1);
   opb0 = XEXP (b, 0);
   opb1 = XEXP (b, 1);

   mode = GET_MODE (op0);
   if (mode != GET_MODE (opb0))
      mode = VOIDmode;
   else if (mode == VOIDmode){
      mode = GET_MODE (op1);
      if (mode != GET_MODE (opb1))
         mode = VOIDmode;
   }

   /* A < B implies A + 1 <= B.  */
   if ((GET_CODE (a) == GT || GET_CODE (a) == LT)  && (GET_CODE (b) == GE || GET_CODE (b) == LE)){

      if (GET_CODE (a) == GT)
         std::swap (op0, op1);

      if (GET_CODE (b) == GE)
         std::swap (opb0, opb1);

      if (mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,mode)  && rtx_equal_p (op1, opb1)
      && mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MINUS, mode, opb0, op0) == const1_rtx)
         return true;
      return false;
   }

   /* A < B or A > B imply A != B.  TODO: Likewise
   A + n < B implies A != B + n if neither wraps.  */
   if (GET_CODE (b) == NE   && (GET_CODE (a) == GT || GET_CODE (a) == GTU  || GET_CODE (a) == LT || GET_CODE (a) == LTU)){
      if (rtx_equal_p (op0, opb0) && rtx_equal_p (op1, opb1))
         return true;
   }

   /* For unsigned comparisons, A != 0 implies A > 0 and A >= 1.  */
   if (GET_CODE (a) == NE  && op1 == const0_rtx){
      if ((GET_CODE (b) == GTU  && opb1 == const0_rtx) || (GET_CODE (b) == GEU  && opb1 == const1_rtx))
         return rtx_equal_p (op0, opb0);
   }

   /* A != N is equivalent to A - (N + 1) <u -1.  */
   if (GET_CODE (a) == NE
   && CONST_INT_P (op1)
   && GET_CODE (b) == LTU
   && opb1 == constm1_rtx
   && GET_CODE (opb0) == PLUS
   && CONST_INT_P (XEXP (opb0, 1))
   /* Avoid overflows.  */
   && ((unsigned HOST_WIDE_INT) INTVAL (XEXP (opb0, 1)) != (HOST_WIDE_INT_1U << (HOST_BITS_PER_WIDE_INT - 1)) - 1)
   && INTVAL (XEXP (opb0, 1)) + 1 == -INTVAL (op1))
      return rtx_equal_p (op0, XEXP (opb0, 0));

   /* Likewise, A != N implies A - N > 0.  */
   if (GET_CODE (a) == NE && CONST_INT_P (op1)){
      if (GET_CODE (b) == GTU
      && GET_CODE (opb0) == PLUS
      && opb1 == const0_rtx
      && CONST_INT_P (XEXP (opb0, 1))
      /* Avoid overflows.  */
      && ((unsigned HOST_WIDE_INT) INTVAL (XEXP (opb0, 1)) != (HOST_WIDE_INT_1U << (HOST_BITS_PER_WIDE_INT - 1)))
      && rtx_equal_p (XEXP (opb0, 0), op0))
         return INTVAL (op1) == -INTVAL (XEXP (opb0, 1));
      if (GET_CODE (b) == GEU
      && GET_CODE (opb0) == PLUS
      && opb1 == const1_rtx
      && CONST_INT_P (XEXP (opb0, 1))
      /* Avoid overflows.  */
      && ((unsigned HOST_WIDE_INT) INTVAL (XEXP (opb0, 1))
      != (HOST_WIDE_INT_1U << (HOST_BITS_PER_WIDE_INT - 1)))
      && rtx_equal_p (XEXP (opb0, 0), op0))
         return INTVAL (op1) == -INTVAL (XEXP (opb0, 1));
   }

   /* A >s X, where X is positive, implies A <u Y, if Y is negative.  */
   if ((GET_CODE (a) == GT || GET_CODE (a) == GE)
   && CONST_INT_P (op1)
   && ((GET_CODE (a) == GT && op1 == constm1_rtx)
   || INTVAL (op1) >= 0)
   && GET_CODE (b) == LTU
   && CONST_INT_P (opb1)
   && rtx_equal_p (op0, opb0))
      return INTVAL (opb1) < 0;

   return false;
}

/* Canonicalizes COND so that

   (1) Ensure that operands are ordered according to
       swap_commutative_operands_p.
   (2) (LE x const) will be replaced with (LT x <const+1>) and similarly
       for GE, GEU, and LEU.  */
//原型 canon_condition rtl.h loop-iv.cc
rtx mtcs_loop_iv_canon_condition (MtcsLoopIv *self,rtx cond)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   rtx op0, op1;
   enum rtx_code code;
   machine_mode mode;

   code = GET_CODE (cond);
   op0 = XEXP (cond, 0);
   op1 = XEXP (cond, 1);

   if (mtcs_rtlanal_swap_commutative_operands_p/*!swap_commutative_operands_p*/(mtcsRtlanal,op0, op1)){
      code = swap_condition (code);
      std::swap (op0, op1);
   }

   mode = GET_MODE (op0);
   if (mode == VOIDmode)
   mode = GET_MODE (op1);
   gcc_assert (mode != VOIDmode);

   if (CONST_SCALAR_INT_P (op1) && mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) != MODE_CC){
      mtcs_rtx_mode_t/*!rtx_mode_t*/ const_val (op1, mode);
      unsigned short precision =mtcs_mode_get_precision(mtcsMode,mtcs_mode_as_a <scalar_mode>(mtcsMode,mode));
      //原wi::max_value (mode, SIGNED) 该方法定义在 rtl.h
      //原型 return max_value (GET_MODE_PRECISION (as_a <scalar_mode> (mode)), sgn);
      //在MTCS 内不能调用 GET_MODE_PRECISION 所以在外部生成 precision后再调 max_value 直接进入了 wide-int.cc中的实现
      switch (code){
         case LE:
            if (wi::ne_p (const_val, wi::max_value (precision, SIGNED))){
               code = LT;
               op1 = mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,wi::add (const_val, 1),  mode);
            }
            break;

         case GE:
            if (wi::ne_p (const_val, wi::min_value (precision, SIGNED))){
               code = GT;
               op1 = mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,wi::sub (const_val, 1), mode);
            }
            break;

         case LEU:
            if (wi::ne_p (const_val, -1)){
               code = LTU;
               op1 = mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,wi::add (const_val, 1), mode);
            }
            break;

         case GEU:
            if (wi::ne_p (const_val, 0)){
               code = GTU;
               op1 = mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,wi::sub (const_val, 1), mode);
            }
            break;

         default:
            break;
      }
   }

   if (op0 != XEXP (cond, 0)
   || op1 != XEXP (cond, 1)
   || code != GET_CODE (cond)
   || GET_MODE (cond) != mtcsMode->modes.M_SImode)
   cond = gen_rtx_fmt_ee (code, mtcsMode->modes.M_SImode, op0, op1);

   return cond;
}

/* Reverses CONDition; returns NULL if we cannot.  */

static rtx reversed_condition (MtcsLoopIv *self,rtx cond)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);

   enum rtx_code reversed;
   reversed = mtcs_dojump_reversed_comparison_code/*!reversed_comparison_code*/(mtcsDojump,cond, NULL);
   if (reversed == UNKNOWN)
      return NULL_RTX;
   else
      return gen_rtx_fmt_ee (reversed, GET_MODE (cond), XEXP (cond, 0),XEXP (cond, 1));
}

/* Tries to use the fact that COND holds to simplify EXPR.  ALTERED is the
   set of altered regs.  */
//原型 simplify_using_condition rtl.h loop-iv.cc
void mtcs_loop_iv_simplify_using_condition (MtcsLoopIv *self,rtx cond, rtx *expr, regset altered)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

   rtx rev, reve, exp = *expr;
   /* If some register gets altered later, we do not really speak about its
   value at the time of comparison.  */
   if (altered && altered_reg_used (cond, altered))
      return;

   if (GET_CODE (cond) == EQ && REG_P (XEXP (cond, 0)) && CONSTANT_P (XEXP (cond, 1))){
      *expr = mtcs_simplify_rtx_simplify_replace_rtx/*!simplify_replace_rtx*/(mtcsSimplifyRtx,
            *expr, XEXP (cond, 0), XEXP (cond, 1));
      return;
   }

   if (!COMPARISON_P (exp))
      return;

   rev = reversed_condition(self,cond);
   reve = reversed_condition(self,exp);

   cond = mtcs_loop_iv_canon_condition/*!canon_condition*/(self,cond);
   exp = mtcs_loop_iv_canon_condition/*!canon_condition*/(self,exp);
   if (rev)
      rev = mtcs_loop_iv_canon_condition/*!canon_condition*/(self,rev);
   if (reve)
      reve = mtcs_loop_iv_canon_condition/*!canon_condition*/(self,reve);

   if (rtx_equal_p (exp, cond)){
      *expr = const_true_rtx;
      return;
   }

   if (rev && rtx_equal_p (exp, rev)){
      *expr = const0_rtx;
      return;
   }

   if (implies_p(self,cond, exp)){
      *expr = const_true_rtx;
      return;
   }

   if (reve && implies_p(self,cond, reve)){
      *expr = const0_rtx;
      return;
   }
   /* A proof by contradiction.  If *EXPR implies (not cond), *EXPR must
   be false.  */
   if (rev && implies_p(self,exp, rev)){
      *expr = const0_rtx;
      return;
   }
   /* Similarly, If (not *EXPR) implies (not cond), *EXPR must be true.  */
   if (rev && reve && implies_p(self,reve, rev)){
      *expr = const_true_rtx;
      return;
   }
   /* We would like to have some other tests here.  TODO.  */
   return;
}

/* Use relationship between A and *B to eventually eliminate *B.
   OP is the operation we consider.  */
static void eliminate_implied_condition (MtcsLoopIv *self,enum rtx_code op, rtx a, rtx *b)
{
   switch (op){
      case AND:
         /* If A implies *B, we may replace *B by true.  */
         if (implies_p(self,a, *b))
            *b = const_true_rtx;
         break;
      case IOR:
         /* If *B implies A, we may replace *B by false.  */
         if (implies_p(self,*b, a))
            *b = const0_rtx;
         break;
      default:
         gcc_unreachable ();
   }
}

/* Eliminates the conditions in TAIL that are implied by HEAD.  OP is the
   operation we consider.  */
static void eliminate_implied_conditions (MtcsLoopIv *self,enum rtx_code op, rtx *head, rtx tail)
{
   rtx elt;
   for (elt = tail; elt; elt = XEXP (elt, 1))
      eliminate_implied_condition(self,op, *head, &XEXP (elt, 0));
   for (elt = tail; elt; elt = XEXP (elt, 1))
      eliminate_implied_condition(self,op, XEXP (elt, 0), head);
}

//原型 IOR_REG_SET_HRS regset.h
#define MTCS_IOR_REG_SET_HRS(TO, FROM)   bitmap_ior_into (TO,bitmap_view<HardRegSet> (FROM))

/* Simplifies *EXPR using initial values at the start of the LOOP.  If *EXPR
   is a list, its elements are assumed to be combined using OP.  */
static void simplify_using_initial_values (MtcsLoopIv *self,class loop *loop, enum rtx_code op, rtx *expr)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsFuncAbi *mtcsFuncAbi=mtcs_target_get_func_abi(mtcsTarget);

   bool expression_valid;
   rtx head, tail, last_valid_expr;
   rtx_expr_list *cond_list;
   rtx_insn *insn;
   rtx neutral, aggr;
   regset altered, this_altered;
   edge e;

   if (!*expr)
      return;
   if (CONSTANT_P (*expr))
      return;

   if (GET_CODE (*expr) == EXPR_LIST){
      head = XEXP (*expr, 0);
      tail = XEXP (*expr, 1);
      eliminate_implied_conditions(self,op, &head, tail);
      switch (op){
         case AND:
            neutral = const_true_rtx;
            aggr = const0_rtx;
            break;
         case IOR:
            neutral = const0_rtx;
            aggr = const_true_rtx;
            break;
         default:
            gcc_unreachable ();
      }

      simplify_using_initial_values(self,loop, UNKNOWN, &head);
      if (head == aggr){
         XEXP (*expr, 0) = aggr;
         XEXP (*expr, 1) = NULL_RTX;
         return;
      }else if (head == neutral){
         *expr = tail;
         simplify_using_initial_values(self,loop, op, expr);
         return;
      }
      simplify_using_initial_values(self,loop, op, &tail);
      if (tail && XEXP (tail, 0) == aggr){
         *expr = tail;
         return;
      }
      XEXP (*expr, 0) = head;
      XEXP (*expr, 1) = tail;
      return;
   }

   gcc_assert (op == UNKNOWN);

   replace_single_def_regs(self,expr);
   if (CONSTANT_P (*expr))
      return;

   e = loop_preheader_edge (loop);
   if (e->src == ENTRY_BLOCK_PTR_FOR_FN (cfun))
      return;

   altered = ALLOC_REG_SET (&reg_obstack);
   this_altered = ALLOC_REG_SET (&reg_obstack);

   expression_valid = true;
   last_valid_expr = *expr;
   cond_list = NULL;
   while (1){
      insn = BB_END (e->src);
      if (any_condjump_p (insn) && onlyjump_p (insn)){
         rtx cond = mtcs_rtlanal_get_condition/*!get_condition*/(mtcsRtlanal,BB_END (e->src), NULL, false, true);

         if (cond && (e->flags & EDGE_FALLTHRU))
            cond = reversed_condition(self,cond);
         if (cond){
            rtx old = *expr;
            mtcs_loop_iv_simplify_using_condition/*!simplify_using_condition*/(self,cond, expr, altered);
            if (old != *expr){
               rtx note;
               if (CONSTANT_P (*expr))
                  goto out;
               for (note = cond_list; note; note = XEXP (note, 1)){
                  mtcs_loop_iv_simplify_using_condition/*!simplify_using_condition*/(self,XEXP (note, 0), expr, altered);
                  if (CONSTANT_P (*expr))
                     goto out;
               }
            }
            cond_list = alloc_EXPR_LIST (0, cond, cond_list);
         }
      }

      FOR_BB_INSNS_REVERSE (e->src, insn){
         rtx src, dest;
         rtx old = *expr;

         if (!INSN_P (insn))
            continue;

         CLEAR_REG_SET (this_altered);
         mtcs_rtlanal_note_stores/*!note_stores*/(mtcsRtlanal,insn, mark_altered, this_altered);
         if (CALL_P (insn)){
            /* Kill all registers that might be clobbered by the call.
            We don't track modes of hard registers, so we need to be
            conservative and assume that partial kills are full kills.  */
            mtcs_function_abi callee_abi = mtcs_func_abi_insn_callee_abi/*!insn_callee_abi*/(mtcsFuncAbi,insn);
            MTCS_IOR_REG_SET_HRS/*!IOR_REG_SET_HRS*/(this_altered,callee_abi.full_and_partial_reg_clobbers ());
         }

         if (suitable_set_for_replacement(self,insn, &dest, &src)){
            rtx_expr_list **pnote, **pnote_next;

            replace_in_expr(self,expr, dest, src);
            if (CONSTANT_P (*expr))
               goto out;

            for (pnote = &cond_list; *pnote; pnote = pnote_next){
               rtx_expr_list *note = *pnote;
               rtx old_cond = XEXP (note, 0);

               pnote_next = (rtx_expr_list **)&XEXP (note, 1);
               replace_in_expr(self,&XEXP (note, 0), dest, src);
               /* We can no longer use a condition that has been simplified
               to a constant, and simplify_using_condition will abort if
               we try.  */
               if (CONSTANT_P (XEXP (note, 0))){
                  *pnote = *pnote_next;
                  pnote_next = pnote;
                  free_EXPR_LIST_node (note);
               }
               /* Retry simplifications with this condition if either the
               expression or the condition changed.  */
               else if (old_cond != XEXP (note, 0) || old != *expr)
                  mtcs_loop_iv_simplify_using_condition/*!simplify_using_condition*/(self,XEXP (note, 0), expr, altered);
            }
         }else{
            rtx_expr_list **pnote, **pnote_next;

            /* If we did not use this insn to make a replacement, any overlap
            between stores in this insn and our expression will cause the
            expression to become invalid.  */
            if (altered_reg_used (*expr, this_altered))
               goto out;

            /* Likewise for the conditions.  */
            for (pnote = &cond_list; *pnote; pnote = pnote_next){
               rtx_expr_list *note = *pnote;
               rtx old_cond = XEXP (note, 0);

               pnote_next = (rtx_expr_list **)&XEXP (note, 1);
               if (altered_reg_used (old_cond, this_altered)){
                  *pnote = *pnote_next;
                  pnote_next = pnote;
                  free_EXPR_LIST_node (note);
               }
            }
         }

         if (CONSTANT_P (*expr))
            goto out;

         IOR_REG_SET (altered, this_altered);

         /* If the expression now contains regs that have been altered, we
         can't return it to the caller.  However, it is still valid for
         further simplification, so keep searching to see if we can
         eventually turn it into a constant.  */
         if (altered_reg_used (*expr, altered))
            expression_valid = false;
         if (expression_valid)
            last_valid_expr = *expr;
      }

      if (!single_pred_p (e->src)|| single_pred (e->src) == ENTRY_BLOCK_PTR_FOR_FN (cfun))
         break;
      e = single_pred_edge (e->src);
   }

out:
   free_EXPR_LIST_list (&cond_list);
   if (!CONSTANT_P (*expr))
      *expr = last_valid_expr;
   FREE_REG_SET (altered);
   FREE_REG_SET (this_altered);
}

/* Transforms invariant IV into MODE.  Adds assumptions based on the fact
   that IV occurs as left operands of comparison COND and its signedness
   is SIGNED_P to DESC.  */
static void shorten_into_mode (MtcsLoopIv *self,class rtx_iv *iv, scalar_int_mode mode,
         enum rtx_code cond, bool signed_p, class niter_desc *desc)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   rtx mmin, mmax, cond_over, cond_under;

   mtc_rtl_get_mode_bounds/*!get_mode_bounds*/(mtcsRTL,mode, signed_p, iv->extend_mode, &mmin, &mmax);
   cond_under = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,
         LT, mtcsMode->modes.M_SImode, iv->extend_mode,iv->base, mmin);
   cond_over = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,
         GT, mtcsMode->modes.M_SImode, iv->extend_mode,iv->base, mmax);

   switch (cond){
      case LE:
      case LT:
      case LEU:
      case LTU:
         if (cond_under != const0_rtx)
            desc->infinite = alloc_EXPR_LIST (0, cond_under, desc->infinite);
         if (cond_over != const0_rtx)
            desc->noloop_assumptions =  alloc_EXPR_LIST (0, cond_over, desc->noloop_assumptions);
         break;

      case GE:
      case GT:
      case GEU:
      case GTU:
         if (cond_over != const0_rtx)
            desc->infinite = alloc_EXPR_LIST (0, cond_over, desc->infinite);
         if (cond_under != const0_rtx)
            desc->noloop_assumptions = alloc_EXPR_LIST (0, cond_under, desc->noloop_assumptions);
         break;

      case NE:
         if (cond_over != const0_rtx)
            desc->infinite = alloc_EXPR_LIST (0, cond_over, desc->infinite);
         if (cond_under != const0_rtx)
            desc->infinite = alloc_EXPR_LIST (0, cond_under, desc->infinite);
         break;

      default:
         gcc_unreachable ();
   }

   iv->mode = mode;
   iv->extend = signed_p ? IV_SIGN_EXTEND : IV_ZERO_EXTEND;
}

/* Transforms IV0 and IV1 compared by COND so that they are both compared as
   subregs of the same mode if possible (sometimes it is necessary to add
   some assumptions to DESC).  */
static bool canonicalize_iv_subregs (MtcsLoopIv *self,class rtx_iv *iv0, class rtx_iv *iv1,
          enum rtx_code cond, class niter_desc *desc)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

   scalar_int_mode comp_mode;
   bool signed_p;

   /* If the ivs behave specially in the first iteration, or are
   added/multiplied after extending, we ignore them.  */
   if (iv0->first_special || iv0->mult != const1_rtx || iv0->delta != const0_rtx)
      return false;
   if (iv1->first_special || iv1->mult != const1_rtx || iv1->delta != const0_rtx)
      return false;

   /* If there is some extend, it must match signedness of the comparison.  */
   switch (cond){
      case LE:
      case LT:
         if (iv0->extend == IV_ZERO_EXTEND || iv1->extend == IV_ZERO_EXTEND)
            return false;
         signed_p = true;
         break;

      case LEU:
      case LTU:
         if (iv0->extend == IV_SIGN_EXTEND || iv1->extend == IV_SIGN_EXTEND)
            return false;
         signed_p = false;
         break;

      case NE:
         if (iv0->extend != IV_UNKNOWN_EXTEND && iv1->extend != IV_UNKNOWN_EXTEND && iv0->extend != iv1->extend)
            return false;

         signed_p = false;
         if (iv0->extend != IV_UNKNOWN_EXTEND)
            signed_p = iv0->extend == IV_SIGN_EXTEND;
         if (iv1->extend != IV_UNKNOWN_EXTEND)
            signed_p = iv1->extend == IV_SIGN_EXTEND;
         break;

      default:
         gcc_unreachable ();
   }

   /* Values of both variables should be computed in the same mode.  These
   might indeed be different, if we have comparison like

   (compare (subreg:SI (iv0)) (subreg:SI (iv1)))

   and iv0 and iv1 are both ivs iterating in SI mode, but calculated
   in different modes.  This does not seem impossible to handle, but
   it hardly ever occurs in practice.

   The only exception is the case when one of operands is invariant.
   For example pentium 3 generates comparisons like
   (lt (subreg:HI (reg:SI)) 100).  Here we assign HImode to 100, but we
   definitely do not want this prevent the optimization.  */
   comp_mode = iv0->extend_mode;
   if (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,comp_mode) <
         mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,iv1->extend_mode))
      comp_mode = iv1->extend_mode;

   if (iv0->extend_mode != comp_mode){
      if (iv0->mode != iv0->extend_mode || iv0->step != const0_rtx)
         return false;

      iv0->base = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,
            signed_p ? SIGN_EXTEND : ZERO_EXTEND,comp_mode, iv0->base, iv0->mode);
      iv0->extend_mode = comp_mode;
   }

   if (iv1->extend_mode != comp_mode){
      if (iv1->mode != iv1->extend_mode || iv1->step != const0_rtx)
         return false;

      iv1->base = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,
            signed_p ? SIGN_EXTEND : ZERO_EXTEND,comp_mode, iv1->base, iv1->mode);
      iv1->extend_mode = comp_mode;
   }

   /* Check that both ivs belong to a range of a single mode.  If one of the
   operands is an invariant, we may need to shorten it into the common
   mode.  */
   if (iv0->mode == iv0->extend_mode && iv0->step == const0_rtx && iv0->mode != iv1->mode)
      shorten_into_mode(self,iv0, iv1->mode, cond, signed_p, desc);

   if (iv1->mode == iv1->extend_mode && iv1->step == const0_rtx && iv0->mode != iv1->mode)
      shorten_into_mode(self,iv1, iv0->mode, swap_condition (cond), signed_p, desc);

   if (iv0->mode != iv1->mode)
      return false;

   desc->mode = iv0->mode;
   desc->signed_p = signed_p;

   return true;
}

/* Tries to estimate the maximum number of iterations in LOOP, and return the
   result.  This function is called from iv_number_of_iterations with
   a number of fields in DESC already filled in.  OLD_NITER is the original
   expression for the number of iterations, before we tried to simplify it.  */
static uint64_t determine_max_iter (MtcsLoopIv *self,class loop *loop, class niter_desc *desc, rtx old_niter)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   rtx niter = desc->niter_expr;
   rtx mmin, mmax, cmp;
   uint64_t nmax, inc;
   uint64_t andmax = 0;

   /* We used to look for constant operand 0 of AND,
   but canonicalization should always make this impossible.  */
   gcc_checking_assert (GET_CODE (niter) != AND  || !CONST_INT_P (XEXP (niter, 0)));

   if (GET_CODE (niter) == AND && CONST_INT_P (XEXP (niter, 1))){
      andmax = UINTVAL (XEXP (niter, 1));
      niter = XEXP (niter, 0);
   }

   mtc_rtl_get_mode_bounds/*!get_mode_bounds*/(mtcsRTL,desc->mode, desc->signed_p, desc->mode, &mmin, &mmax);
   nmax = UINTVAL (mmax) - UINTVAL (mmin);

   if (GET_CODE (niter) == UDIV){
      if (!CONST_INT_P (XEXP (niter, 1)))
         return nmax;
      inc = INTVAL (XEXP (niter, 1));
      niter = XEXP (niter, 0);
   }else
      inc = 1;

   /* We could use a binary search here, but for now improving the upper
   bound by just one eliminates one important corner case.  */
   cmp = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,
         desc->signed_p ? LT : LTU, VOIDmode,desc->mode, old_niter, mmax);
   simplify_using_initial_values(self,loop, UNKNOWN, &cmp);
   if (cmp == const_true_rtx){
      nmax--;
      if (dump_file)
         fprintf (dump_file, ";; improved upper bound by one.\n");
   }
   nmax /= inc;
   if (andmax)
      nmax = MIN (nmax, andmax);
   if (dump_file)
      fprintf (dump_file, ";; Determined upper bound %" PRId64".\n", nmax);
   return nmax;
}

/* Computes number of iterations of the CONDITION in INSN in LOOP and stores
   the result into DESC.  Very similar to determine_number_of_iterations
   (basically its rtl version), complicated by things like subregs.  */
static void iv_number_of_iterations (MtcsLoopIv *self,class loop *loop, rtx_insn *insn, rtx condition,
          class niter_desc *desc)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   rtx op0, op1, delta, step, bound, may_xform, tmp, tmp0, tmp1;
   class rtx_iv iv0, iv1;
   rtx assumption, may_not_xform;
   enum rtx_code cond;
   machine_mode nonvoid_mode;
   scalar_int_mode comp_mode;
   rtx mmin, mmax, mode_mmin, mode_mmax;
   uint64_t s, size, d, inv, max, up, down;
   int64_t inc, step_val;
   int was_sharp = false;
   rtx old_niter;
   bool step_is_pow2;

   /* The meaning of these assumptions is this:
   if !assumptions
   then the rest of information does not have to be valid
   if noloop_assumptions then the loop does not roll
   if infinite then this exit is never used */

   desc->assumptions = NULL_RTX;
   desc->noloop_assumptions = NULL_RTX;
   desc->infinite = NULL_RTX;
   desc->simple_p = true;

   desc->const_iter = false;
   desc->niter_expr = NULL_RTX;

   cond = GET_CODE (condition);
   gcc_assert (COMPARISON_P (condition));

   nonvoid_mode = GET_MODE (XEXP (condition, 0));
   if (nonvoid_mode == VOIDmode)
      nonvoid_mode = GET_MODE (XEXP (condition, 1));
   /* The constant comparisons should be folded.  */
   gcc_assert (nonvoid_mode != VOIDmode);

   /* We only handle integers or pointers.  */
   scalar_int_mode mode;
   if (!mtcs_mode_is_a<scalar_int_mode>(mtcsMode,nonvoid_mode, &mode))
      goto fail;

   op0 = XEXP (condition, 0);
   if (!mtcs_loop_iv_iv_analyze/*!iv_analyze*/(self,insn, mode, op0, &iv0))
      goto fail;

   op1 = XEXP (condition, 1);
   if (!mtcs_loop_iv_iv_analyze/*!iv_analyze*/(self,insn, mode, op1, &iv1))
      goto fail;

   if (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,iv0.extend_mode) > HOST_BITS_PER_WIDE_INT
   || mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,iv1.extend_mode) > HOST_BITS_PER_WIDE_INT)
      goto fail;

   /* Check condition and normalize it.  */

   switch (cond){
      case GE:
      case GT:
      case GEU:
      case GTU:
         std::swap (iv0, iv1);
         cond = swap_condition (cond);
         break;
      case NE:
      case LE:
      case LEU:
      case LT:
      case LTU:
         break;
      default:
         goto fail;
   }

   /* Handle extends.  This is relatively nontrivial, so we only try in some
   easy cases, when we can canonicalize the ivs (possibly by adding some
   assumptions) to shape subreg (base + i * step).  This function also fills
   in desc->mode and desc->signed_p.  */

   if (!canonicalize_iv_subregs(self,&iv0, &iv1, cond, desc))
      goto fail;

   comp_mode = iv0.extend_mode;
   mode = iv0.mode;
   size = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode);
   mtc_rtl_get_mode_bounds/*!get_mode_bounds*/(mtcsRTL,mode, (cond == LE || cond == LT), comp_mode, &mmin, &mmax);
   mode_mmin = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, mmin, comp_mode);
   mode_mmax = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, mmax, comp_mode);

   if (!CONST_INT_P (iv0.step) || !CONST_INT_P (iv1.step))
      goto fail;

   /* We can take care of the case of two induction variables chasing each other
   if the test is NE. I have never seen a loop using it, but still it is
   cool.  */
   if (iv0.step != const0_rtx && iv1.step != const0_rtx){
      if (cond != NE)
         goto fail;

      iv0.step = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MINUS, comp_mode, iv0.step, iv1.step);
      iv1.step = const0_rtx;
   }

   iv0.step = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, iv0.step, comp_mode);
   iv1.step = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, iv1.step, comp_mode);

   /* This is either infinite loop or the one that ends immediately, depending
   on initial values.  Unswitching should remove this kind of conditions.  */
   if (iv0.step == const0_rtx && iv1.step == const0_rtx)
      goto fail;

   if (cond != NE){
      if (iv0.step == const0_rtx)
         step_val = -INTVAL (iv1.step);
      else
         step_val = INTVAL (iv0.step);

      /* Ignore loops of while (i-- < 10) type.  */
      if (step_val < 0)
         goto fail;

      step_is_pow2 = !(step_val & (step_val - 1));
   }else{
      /* We do not care about whether the step is power of two in this
      case.  */
      step_is_pow2 = false;
      step_val = 0;
   }

   /* Some more condition normalization.  We must record some assumptions
   due to overflows.  */
   switch (cond){
      case LT:
      case LTU:
         /* We want to take care only of non-sharp relationals; this is easy,
         as in cases the overflow would make the transformation unsafe
         the loop does not roll.  Seemingly it would make more sense to want
         to take care of sharp relationals instead, as NE is more similar to
         them, but the problem is that here the transformation would be more
         difficult due to possibly infinite loops.  */
         if (iv0.step == const0_rtx){
            tmp = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, iv0.base, comp_mode);
            assumption = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,
                  EQ, mtcsMode->modes.M_SImode, mode, tmp,mode_mmax);
            if (assumption == const_true_rtx)
               goto zero_iter_simplify;
            iv0.base = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, comp_mode,iv0.base, const1_rtx);
         }else{
            tmp = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, iv1.base, comp_mode);
            assumption = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,
                  EQ, mtcsMode->modes.M_SImode, mode, tmp,mode_mmin);
            if (assumption == const_true_rtx)
               goto zero_iter_simplify;
            iv1.base = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, comp_mode,iv1.base, constm1_rtx);
         }

         if (assumption != const0_rtx)
            desc->noloop_assumptions = alloc_EXPR_LIST (0, assumption, desc->noloop_assumptions);
         cond = (cond == LT) ? LE : LEU;

         /* It will be useful to be able to tell the difference once more in
         LE -> NE reduction.  */
         was_sharp = true;
         break;
      default: ;
   }

   /* Take care of trivially infinite loops.  */
   if (cond != NE){
      if (iv0.step == const0_rtx){
         tmp = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, iv0.base, comp_mode);
         if (rtx_equal_p (tmp, mode_mmin)){
            desc->infinite = alloc_EXPR_LIST (0, const_true_rtx, NULL_RTX);
            /* Fill in the remaining fields somehow.  */
            goto zero_iter_simplify;
         }
      }else{
         tmp = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, iv1.base, comp_mode);
         if (rtx_equal_p (tmp, mode_mmax)){
            desc->infinite = alloc_EXPR_LIST (0, const_true_rtx, NULL_RTX);
            /* Fill in the remaining fields somehow.  */
            goto zero_iter_simplify;
         }
      }
   }

   /* If we can we want to take care of NE conditions instead of size
   comparisons, as they are much more friendly (most importantly
   this takes care of special handling of loops with step 1).  We can
   do it if we first check that upper bound is greater or equal to
   lower bound, their difference is constant c modulo step and that
   there is not an overflow.  */
   if (cond != NE){
   if (iv0.step == const0_rtx)
      step = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,NEG, comp_mode, iv1.step, comp_mode);
   else
      step = iv0.step;
   step = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, step, comp_mode);
   delta = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MINUS, comp_mode, iv1.base, iv0.base);
   delta = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, delta, comp_mode);
   delta = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,UMOD, mode, delta, step);
   may_xform = const0_rtx;
   may_not_xform = const_true_rtx;

   if (CONST_INT_P (delta)){
      if (was_sharp && INTVAL (delta) == INTVAL (step) - 1){
         /* A special case.  We have transformed condition of type
         for (i = 0; i < 4; i += 4)
         into
         for (i = 0; i <= 3; i += 4)
         obviously if the test for overflow during that transformation
         passed, we cannot overflow here.  Most importantly any
         loop with sharp end condition and step 1 falls into this
         category, so handling this case specially is definitely
         worth the troubles.  */
         may_xform = const_true_rtx;
      }else if (iv0.step == const0_rtx){
         bound = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, comp_mode, mmin, step);
         bound = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MINUS, comp_mode, bound, delta);
         bound = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, bound, comp_mode);
         tmp = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, iv0.base, comp_mode);
         may_xform = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,
               cond, mtcsMode->modes.M_SImode, mode,bound, tmp);
         may_not_xform = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,
               reverse_condition (cond),mtcsMode->modes.M_SImode, mode,bound, tmp);
      }else{
         bound = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MINUS, comp_mode, mmax, step);
         bound = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, comp_mode, bound, delta);
         bound = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, bound, comp_mode);
         tmp = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, iv1.base, comp_mode);
         may_xform = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,
               cond, mtcsMode->modes.M_SImode, mode,tmp, bound);
         may_not_xform = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,
               reverse_condition (cond),mtcsMode->modes.M_SImode, mode,tmp, bound);
      }
   }

   if (may_xform != const0_rtx){
      /* We perform the transformation always provided that it is not
      completely senseless.  This is OK, as we would need this assumption
      to determine the number of iterations anyway.  */
      if (may_xform != const_true_rtx){
         /* If the step is a power of two and the final value we have
         computed overflows, the cycle is infinite.  Otherwise it
         is nontrivial to compute the number of iterations.  */
         if (step_is_pow2)
            desc->infinite = alloc_EXPR_LIST (0, may_not_xform,desc->infinite);
         else
            desc->assumptions = alloc_EXPR_LIST (0, may_xform, desc->assumptions);
         }

         /* We are going to lose some information about upper bound on
         number of iterations in this step, so record the information
         here.  */
         inc = INTVAL (iv0.step) - INTVAL (iv1.step);
         if (CONST_INT_P (iv1.base))
            up = INTVAL (iv1.base);
         else
            up = INTVAL (mode_mmax) - inc;
         down = INTVAL (CONST_INT_P (iv0.base)? iv0.base: mode_mmin);
         max = (up - down) / inc + 1;
         if (!desc->infinite  && !desc->assumptions)
            record_niter_bound (loop, max, false, true);

         if (iv0.step == const0_rtx){
            iv0.base = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, comp_mode, iv0.base, delta);
            iv0.base = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MINUS, comp_mode, iv0.base, step);
         }else{
            iv1.base = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MINUS, comp_mode, iv1.base, delta);
            iv1.base = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, comp_mode, iv1.base, step);
         }

         tmp0 = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, iv0.base, comp_mode);
         tmp1 = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, iv1.base, comp_mode);
         assumption = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,
               reverse_condition (cond),mtcsMode->modes.M_SImode, mode, tmp0, tmp1);
         if (assumption == const_true_rtx)
            goto zero_iter_simplify;
         else if (assumption != const0_rtx)
            desc->noloop_assumptions = alloc_EXPR_LIST (0, assumption, desc->noloop_assumptions);
         cond = NE;
      }
   }

   /* Count the number of iterations.  */
   if (cond == NE){
      /* Everything we do here is just arithmetics modulo size of mode.  This
      makes us able to do more involved computations of number of iterations
      than in other cases.  First transform the condition into shape
      s * i <> c, with s positive.  */
      iv1.base = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MINUS, comp_mode, iv1.base, iv0.base);
      iv0.base = const0_rtx;
      iv0.step = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MINUS, comp_mode, iv0.step, iv1.step);
      iv1.step = const0_rtx;
      if (INTVAL (iv0.step) < 0){
         iv0.step = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,NEG, comp_mode, iv0.step, comp_mode);
         iv1.base = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,NEG, comp_mode, iv1.base, comp_mode);
      }
      iv0.step = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, iv0.step, comp_mode);

      /* Let nsd (s, size of mode) = d.  If d does not divide c, the loop
      is infinite.  Otherwise, the number of iterations is
      (inverse(s/d) * (c/d)) mod (size of mode/d).  */
      s = INTVAL (iv0.step); d = 1;
      while (s % 2 != 1){
         s /= 2;
         d *= 2;
         size--;
      }
      bound = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,((uint64_t) 1 << (size - 1) << 1) - 1, mode);

      tmp1 = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, iv1.base, comp_mode);
      tmp = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,UMOD, mode, tmp1, mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,d, mode));
      assumption = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,
            NE, mtcsMode->modes.M_SImode, mode, tmp, const0_rtx);
      desc->infinite = alloc_EXPR_LIST (0, assumption, desc->infinite);

      tmp = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,UDIV, mode, tmp1, mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,d, mode));
      inv = inverse (s, size);
      tmp = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MULT, mode, tmp, mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,inv, mode));
      desc->niter_expr = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,AND, mode, tmp, bound);
   }else{
      if (iv1.step == const0_rtx){
      /* Condition in shape a + s * i <= b
      We must know that b + s does not overflow and a <= b + s and then we
      can compute number of iterations as (b + s - a) / s.  (It might
      seem that we in fact could be more clever about testing the b + s
      overflow condition using some information about b - a mod s,
      but it was already taken into account during LE -> NE transform).  */
         step = iv0.step;
         tmp0 = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, iv0.base, comp_mode);
         tmp1 = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, iv1.base, comp_mode);

         bound = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MINUS, mode, mode_mmax,
         mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, step,comp_mode));
         if (step_is_pow2){
            rtx t0, t1;

            /* If s is power of 2, we know that the loop is infinite if
            a % s <= b % s and b + s overflows.  */
            assumption = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,
                  reverse_condition (cond),mtcsMode->modes.M_SImode, mode,tmp1, bound);

            t0 = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,UMOD, mode, copy_rtx (tmp0), step);
            t1 = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,UMOD, mode, copy_rtx (tmp1), step);
            tmp = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,cond, mtcsMode->modes.M_SImode, mode, t0, t1);
            assumption = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,AND, mtcsMode->modes.M_SImode, assumption, tmp);
            desc->infinite = alloc_EXPR_LIST (0, assumption, desc->infinite);
         }else{
            assumption = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,
                  cond, mtcsMode->modes.M_SImode, mode,tmp1, bound);
            desc->assumptions = alloc_EXPR_LIST (0, assumption, desc->assumptions);
         }

         tmp = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, comp_mode, iv1.base, iv0.step);
         tmp = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, tmp, comp_mode);
         assumption = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,
               reverse_condition (cond), mtcsMode->modes.M_SImode, mode, tmp0, tmp);

         delta = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, mode, tmp1, step);
         delta = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MINUS, mode, delta, tmp0);
      }else{
         /* Condition in shape a <= b - s * i
         We must know that a - s does not overflow and a - s <= b and then
         we can again compute number of iterations as (b - (a - s)) / s.  */
         step = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,NEG, mode, iv1.step, mode);
         tmp0 = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, iv0.base, comp_mode);
         tmp1 = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, iv1.base, comp_mode);

         bound = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, mode, mode_mmin,
         mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, step, comp_mode));
         if (step_is_pow2){
            rtx t0, t1;

            /* If s is power of 2, we know that the loop is infinite if
            a % s <= b % s and a - s overflows.  */
            assumption = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,
                  reverse_condition (cond),mtcsMode->modes.M_SImode, mode, bound, tmp0);

            t0 = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,UMOD, mode, copy_rtx (tmp0), step);
            t1 = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,UMOD, mode, copy_rtx (tmp1), step);
            tmp = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,cond, mtcsMode->modes.M_SImode, mode, t0, t1);
            assumption = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,AND, mtcsMode->modes.M_SImode, assumption, tmp);
            desc->infinite = alloc_EXPR_LIST (0, assumption, desc->infinite);
         }else{
            assumption = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,
                  cond, mtcsMode->modes.M_SImode, mode, bound, tmp0);
            desc->assumptions = alloc_EXPR_LIST (0, assumption, desc->assumptions);
         }

         tmp = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, comp_mode, iv0.base, iv1.step);
         tmp = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, tmp, comp_mode);
         assumption = mtcs_simplify_rtx_gen_relational/*!simplify_gen_relational*/(mtcsSimplifyRtx,
               reverse_condition (cond),mtcsMode->modes.M_SImode, mode,tmp, tmp1);
         delta = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MINUS, mode, tmp0, step);
         delta = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MINUS, mode, tmp1, delta);
      }
      if (assumption == const_true_rtx)
         goto zero_iter_simplify;
      else if (assumption != const0_rtx)
         desc->noloop_assumptions =
      alloc_EXPR_LIST (0, assumption, desc->noloop_assumptions);
      delta = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,UDIV, mode, delta, step);
      desc->niter_expr = delta;
   }

   old_niter = desc->niter_expr;

   simplify_using_initial_values(self,loop, AND, &desc->assumptions);
   if (desc->assumptions && XEXP (desc->assumptions, 0) == const0_rtx)
      goto fail;
   simplify_using_initial_values(self,loop, IOR, &desc->noloop_assumptions);
   simplify_using_initial_values(self,loop, IOR, &desc->infinite);
   simplify_using_initial_values(self,loop, UNKNOWN, &desc->niter_expr);

   /* Rerun the simplification.  Consider code (created by copying loop headers)

   i = 0;

   if (0 < n)
   {
   do
   {
   i++;
   } while (i < n);
   }

   The first pass determines that i = 0, the second pass uses it to eliminate
   noloop assumption.  */

   simplify_using_initial_values(self,loop, AND, &desc->assumptions);
   if (desc->assumptions  && XEXP (desc->assumptions, 0) == const0_rtx)
      goto fail;
   simplify_using_initial_values(self,loop, IOR, &desc->noloop_assumptions);
   simplify_using_initial_values(self,loop, IOR, &desc->infinite);
   simplify_using_initial_values(self,loop, UNKNOWN, &desc->niter_expr);

   if (desc->noloop_assumptions && XEXP (desc->noloop_assumptions, 0) == const_true_rtx)
      goto zero_iter;

   if (CONST_INT_P (desc->niter_expr)){
      uint64_t val = INTVAL (desc->niter_expr);

      desc->const_iter = true;
      desc->niter = val & GET_MODE_MASK (desc->mode);
      if (!desc->infinite && !desc->assumptions)
         record_niter_bound (loop, desc->niter, false, true);
   }else{
      max = determine_max_iter(self,loop, desc, old_niter);
      if (!max)
         goto zero_iter_simplify;
      if (!desc->infinite  && !desc->assumptions)
         record_niter_bound (loop, max, false, true);

      /* simplify_using_initial_values does a copy propagation on the registers
      in the expression for the number of iterations.  This prolongs life
      ranges of registers and increases register pressure, and usually
      brings no gain (and if it happens to do, the cse pass will take care
      of it anyway).  So prevent this behavior, unless it enabled us to
      derive that the number of iterations is a constant.  */
      desc->niter_expr = old_niter;
   }

   return;

zero_iter_simplify:
   /* Simplify the assumptions.  */
   simplify_using_initial_values(self,loop, AND, &desc->assumptions);
   if (desc->assumptions && XEXP (desc->assumptions, 0) == const0_rtx)
      goto fail;
   simplify_using_initial_values(self,loop, IOR, &desc->infinite);

   /* Fallthru.  */
zero_iter:
   desc->const_iter = true;
   desc->niter = 0;
   record_niter_bound (loop, 0, true, true);
   desc->noloop_assumptions = NULL_RTX;
   desc->niter_expr = const0_rtx;
   return;

fail:
   desc->simple_p = false;
   return;
}

/* Checks whether E is a simple exit from LOOP and stores its description
   into DESC.  */

static void check_simple_exit (MtcsLoopIv *self,class loop *loop, edge e, class niter_desc *desc)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   basic_block exit_bb;
   rtx condition;
   rtx_insn *at;
   edge ein;

   exit_bb = e->src;
   desc->simple_p = false;
   /* It must belong directly to the loop.  */
   if (exit_bb->loop_father != loop)
      return;
   /* It must be tested (at least) once during any iteration.  */
   if (!dominated_by_p (CDI_DOMINATORS, loop->latch, exit_bb))
      return;
   /* It must end in a simple conditional jump.  */
   if (!any_condjump_p (BB_END (exit_bb)) || !onlyjump_p (BB_END (exit_bb)))
      return;

   ein = EDGE_SUCC (exit_bb, 0);
   if (ein == e)
      ein = EDGE_SUCC (exit_bb, 1);

   desc->out_edge = e;
   desc->in_edge = ein;
   /* Test whether the condition is suitable.  */
   if (!(condition = mtcs_rtlanal_get_condition/*!get_condition*/(mtcsRtlanal,BB_END (ein->src), &at, false, false)))
      return;

   if (ein->flags & EDGE_FALLTHRU){
      condition = reversed_condition(self,condition);
      if (!condition)
         return;
   }
   /* Check that we are able to determine number of iterations and fill
   in information about it.  */
   iv_number_of_iterations(self,loop, at, condition, desc);
}

/* Finds a simple exit of LOOP and stores its description into DESC.  */
static void find_simple_exit (MtcsLoopIv *self,class loop *loop, class niter_desc *desc)
{
   unsigned i;
   basic_block *body;
   edge e;
   class niter_desc act;
   bool any = false;
   edge_iterator ei;

   desc->simple_p = false;
   body = get_loop_body (loop);

   for (i = 0; i < loop->num_nodes; i++){
      FOR_EACH_EDGE (e, ei, body[i]->succs){
         if (flow_bb_inside_loop_p (loop, e->dest))
            continue;

         check_simple_exit(self,loop, e, &act);
         if (!act.simple_p)
            continue;

         if (!any)
            any = true;
         else{
            /* Prefer constant iterations; the less the better.  */
            if (!act.const_iter || (desc->const_iter && act.niter >= desc->niter))
               continue;
            /* Also if the actual exit may be infinite, while the old one
            not, prefer the old one.  */
            if (act.infinite && !desc->infinite)
               continue;
         }
         *desc = act;
      }
   }

   if (dump_file){
      if (desc->simple_p){
         fprintf (dump_file, "Loop %d is simple:\n", loop->num);
         fprintf (dump_file, "  simple exit %d -> %d\n",desc->out_edge->src->index,desc->out_edge->dest->index);
         if (desc->assumptions){
            fprintf (dump_file, "  assumptions: ");
            mtcs_print_rtl/*!print_rtl*/(dump_file, desc->assumptions);
            fprintf (dump_file, "\n");
         }
         if (desc->noloop_assumptions){
            fprintf (dump_file, "  does not roll if: ");
            mtcs_print_rtl/*!print_rtl*/(dump_file, desc->noloop_assumptions);
            fprintf (dump_file, "\n");
         }
         if (desc->infinite){
            fprintf (dump_file, "  infinite if: ");
            mtcs_print_rtl/*!print_rtl*/(dump_file, desc->infinite);
            fprintf (dump_file, "\n");
         }

         fprintf (dump_file, "  number of iterations: ");
         mtcs_print_rtl/*!print_rtl*/(dump_file, desc->niter_expr);
         fprintf (dump_file, "\n");

         fprintf (dump_file, "  upper bound: %li\n",(long)get_max_loop_iterations_int (loop));
         fprintf (dump_file, "  likely upper bound: %li\n",(long)get_likely_max_loop_iterations_int (loop));
         fprintf (dump_file, "  realistic bound: %li\n", (long)get_estimated_loop_iterations_int (loop));
      }else
         fprintf (dump_file, "Loop %d is not simple.\n", loop->num);
   }

   /* Fix up the finiteness if possible.  We can only do it for single exit,
   since the loop is finite, but it's possible that we predicate one loop
   exit to be finite which can not be determined as finite in middle-end as
   well.  It results in incorrect predicate information on the exit condition
   expression.  For example, if says [(int) _1 + -8, + , -8] != 0 finite,
   it means _1 can exactly divide -8.  */
   if (desc->infinite && single_exit (loop) && finite_loop_p (loop)){
      desc->infinite = NULL_RTX;
      if (dump_file)
         fprintf (dump_file, "  infinite updated to finite.\n");
   }

   free (body);
}

/* Creates a simple loop description of LOOP if it was not computed
   already.  */
//原型 get_simple_loop_desc cfgloop.h loop-iv.cc
class niter_desc * mtcs_loop_iv_get_simple_loop_desc (MtcsLoopIv *self,class loop *loop)
{
   n_debug("mtcsloopiv.c get_simple_loop_desc 00 loop:%p\n",loop);

   class niter_desc *desc = simple_loop_desc (loop);
   n_debug("mtcsloopiv.c get_simple_loop_desc 11 loop:%p desc:%p\n",loop,desc);

   if (desc)
      return desc;
   n_debug("mtcsloopiv.c get_simple_loop_desc 22 loop:%p desc:%p\n",loop,desc);

   /* At least desc->infinite is not always initialized by
   find_simple_loop_exit.  */
   desc = ggc_cleared_alloc<niter_desc> ();
   mtcs_loop_iv_iv_analysis_loop_init/*!iv_analysis_loop_init*/(self,loop);
   n_debug("mtcsloopiv.c get_simple_loop_desc 33 loop:%p desc:%p\n",loop,desc);

   find_simple_exit (self,loop, desc);
   loop->simple_loop_desc = desc;
   return desc;
}

/* Releases simple loop description for LOOP.  */
//原型 free_simple_loop_desc cfgloop.h loop-iv.cc
void mtcs_loop_iv_free_simple_loop_desc (MtcsLoopIv *self,class loop *loop)
{
  class niter_desc *desc = simple_loop_desc (loop);
  if (!desc)
    return;
  ggc_free (desc);
  loop->simple_loop_desc = NULL;
}

static void mtcsLoopIvInit(MtcsLoopIv *self)
{
   self->clean_slate = true;
   self->iv_ref_table_size = 0;
}

MtcsLoopIv *mtcs_loop_iv_new(MtcsMode *mtcsMode)
{
   MtcsLoopIv *self = n_slice_alloc0 (sizeof(MtcsLoopIv));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcsLoopIvInit(self);
   return self;
}
