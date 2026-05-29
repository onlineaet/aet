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


/* This file handles generation of all the assembler code
   *except* the instructions of a function.
   This includes declarations of variables and their initial values.

   We also subregput the assembler code for constants stored in memory
   and are responsible for combining constants with the same value.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "rtl.h"
#include "tree.h"
#include "cfghooks.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "expmed.h"
#include "insn-config.h"
#include "emit-rtl.h"
#include "recog.h"
#include "cfgrtl.h"
#include "cfgbuild.h"
#include "dce.h"
#include "expr.h"
#include "explow.h"
#include "tree-pass.h"
#include "lower-subreg.h"
#include "rtl-iter.h"
#include "target.h"

#include "mtcslowersubreg.h"
#include "mtcstarget.h"
#include "mtcsdfcore.h"
#include "mtcsdfproblems.h"

#define LOG_COSTS 0
#define FORCE_LOWERING 0

static void mtcsLowerSubregInit(MtcsLowerSubreg *self)
{


}

/* Return true if MODE is a mode we know how to lower.  When returning true,
   store its byte size in *BYTES and its word size in *WORDS.  */
//原型 interesting_mode_p lower-subreg.cc
static inline bool interesting_mode_p (MtcsLowerSubreg *self,machine_mode mode, unsigned int *bytes, unsigned int *words)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  if (!mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,mode).is_constant (bytes))
    return false;
  *words = CEIL (*bytes, UNITS_PER_WORD);
  return true;
}

/* RTXes used while computing costs.  */
struct cost_rtxes {
  /* Source and target registers.  */
  rtx source;
  rtx target;

  /* A twice_word_mode ZERO_EXTEND of SOURCE.  */
  rtx zext;

  /* A shift of SOURCE.  */
  rtx shift;

  /* A SET of TARGET.  */
  rtx set;
};

/* Return the cost of a CODE shift in mode MODE by OP1 bits, using the
   rtxes in RTXES.  SPEED_P selects between the speed and size cost.  */
//原型 shift_cost lower-subreg.cc
static int shift_cost (MtcsLowerSubreg *self,bool speed_p, struct cost_rtxes *rtxes, enum rtx_code code,
        machine_mode mode, int op1)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  PUT_CODE (rtxes->shift, code);
  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,rtxes->shift, mode);
  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,rtxes->source, mode);
  XEXP (rtxes->shift, 1) = mtcs_rtl_gen_int_shift_amount/*!gen_int_shift_amount*/(mtcsRTL,mode, op1);
  return mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,rtxes->shift, mode, speed_p);
}

/* For each X in the range [0, BITS_PER_WORD), set SPLITTING[X]
   to true if it is profitable to split a double-word CODE shift
   of X + BITS_PER_WORD bits.  SPEED_P says whether we are testing
   for speed or size profitability.

   Use the rtxes in RTXES to calculate costs.  WORD_MOVE_ZERO_COST is
   the cost of moving zero into a word-mode register.  WORD_MOVE_COST
   is the cost of moving between word registers.  */
//原型 compute_splitting_shift lower-subreg.cc
static void compute_splitting_shift (MtcsLowerSubreg *self,bool speed_p, struct cost_rtxes *rtxes,
             bool *splitting, enum rtx_code code, int word_move_zero_cost, int word_move_cost)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

  int wide_cost, narrow_cost, upper_cost, i;

  for (i = 0; i < BITS_PER_WORD; i++){
      wide_cost = shift_cost(self,speed_p, rtxes, code, self->x_twice_word_mode, i + BITS_PER_WORD);
      if (i == 0)
          narrow_cost = word_move_cost;
      else
          narrow_cost = shift_cost(self,speed_p, rtxes, code, mtcsMode->word_mode, i);

      if (code != ASHIFTRT)
          upper_cost = word_move_zero_cost;
      else if (i == BITS_PER_WORD - 1)
          upper_cost = word_move_cost;
      else
          upper_cost = shift_cost (self,speed_p, rtxes, code, mtcsMode->word_mode, BITS_PER_WORD - 1);

      //if (LOG_COSTS)
      n_debug("mtcslowersubreg.c compute_splitting_shift %s %s by %d: original cost %d, split cost %d + %d\n",
            mtcs_mode_get_name (mtcsMode,self->x_twice_word_mode), GET_RTX_NAME (code),
         i + BITS_PER_WORD, wide_cost, narrow_cost, upper_cost);

      if (FORCE_LOWERING || wide_cost >= narrow_cost + upper_cost)
          splitting[i] = true;
  }
}


/* Compute what we should do when optimizing for speed or size; SPEED_P
   selects which.  Use RTXES for computing costs.  */
//原型 compute_costs lower-subreg.cc
static void compute_costs (MtcsLowerSubreg *self,bool speed_p, struct cost_rtxes *rtxes)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  unsigned int i;
  int word_move_zero_cost, word_move_cost;

  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,rtxes->target, mtcsMode->word_mode);
  SET_SRC (rtxes->set) = CONST0_RTX (mtcsMode->word_mode);
  word_move_zero_cost = mtcs_rtlanal_set_rtx_cost (mtcsRtlanal,rtxes->set, speed_p);

  SET_SRC (rtxes->set) = rtxes->source;
  word_move_cost = mtcs_rtlanal_set_rtx_cost (mtcsRtlanal,rtxes->set, speed_p);

 // if (LOG_COSTS)
  n_debug("mtcslowersubreg.c compute_costs 00 %s move: from zero cost %d, from reg cost %d\n",
         mtcs_mode_get_name(mtcsMode,mtcsMode->word_mode), word_move_zero_cost, word_move_cost);
  int maxMachineMode=mtcs_mode_get_max_number(mtcsMode);
  for (i = 0; i <maxMachineMode/*!MAX_MACHINE_MODE*/; i++){
      machine_mode mode = (machine_mode) i;
      unsigned int size, factor;
      if (interesting_mode_p (self,mode, &size, &factor) && factor > 1){
          unsigned int mode_move_cost;
          mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,rtxes->target, mode);
          mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,rtxes->source, mode);
          mode_move_cost = mtcs_rtlanal_set_rtx_cost(mtcsRtlanal,rtxes->set, speed_p);

          //if (LOG_COSTS)
          n_debug("mtcslowersubreg.c compute_costs 11 %s move: original cost %d, split cost %d * %d\n",
                    mtcs_mode_get_name (mtcsMode,mode), mode_move_cost, word_move_cost, factor);
          if (FORCE_LOWERING || mode_move_cost >= word_move_cost * factor){
              self->x_choices[speed_p].move_modes_to_split[i] = true;
              self->x_choices[speed_p].something_to_do = true;
          }
      }
  }

  /* For the moves and shifts, the only case that is checked is one
     where the mode of the target is an integer mode twice the width
     of the word_mode.

     If it is not profitable to split a double word move then do not
     even consider the shifts or the zero extension.  */
  if (self->x_choices[speed_p].move_modes_to_split[(int) self->x_twice_word_mode]){
      int zext_cost;

      /* The only case here to check to see if moving the upper part with a
     zero is cheaper than doing the zext itself.  */
      mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,rtxes->source, mtcsMode->word_mode);
      zext_cost = mtcs_rtlanal_set_src_cost(mtcsRtlanal,rtxes->zext, self->x_twice_word_mode, speed_p);

      //if (LOG_COSTS)
      n_debug("mtcslowersubreg.c compute_costs 22 %s %s: original cost %d, split cost %d + %d\n",
            mtcs_mode_get_name(mtcsMode,self->x_twice_word_mode), GET_RTX_NAME (ZERO_EXTEND),
         zext_cost, word_move_cost, word_move_zero_cost);

      if (FORCE_LOWERING || zext_cost >= word_move_cost + word_move_zero_cost)
          self->x_choices[speed_p].splitting_zext = true;

      compute_splitting_shift (self,speed_p, rtxes,
              self->x_choices[speed_p].splitting_ashift, ASHIFT,
                   word_move_zero_cost, word_move_cost);
      compute_splitting_shift (self,speed_p, rtxes,
              self->x_choices[speed_p].splitting_lshiftrt, LSHIFTRT,
                   word_move_zero_cost, word_move_cost);
      compute_splitting_shift (self,speed_p, rtxes,
              self->x_choices[speed_p].splitting_ashiftrt, ASHIFTRT,
                   word_move_zero_cost, word_move_cost);
  }
}

/* Do one-per-target initialisation.  This involves determining
   which operations on the machine are profitable.  If none are found,
   then the pass just returns when called.  */
//原型 init_lower_subreg rtl.h lower-subreg.cc
void mtcs_lower_subreg_init_lower_subreg (MtcsLowerSubreg *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   struct cost_rtxes rtxes;
   memset (&self->x_twice_word_mode, 0, sizeof (scalar_int_mode_pod));
   memset (&self->x_choices[0], 0, sizeof (struct mtcs_lower_subreg_choices));
   memset (&self->x_choices[1], 0, sizeof (struct mtcs_lower_subreg_choices));


   self->x_twice_word_mode = mtcs_mode_get_2xwider/*!GET_MODE_2XWIDER_MODE (word_mode)*/(mtcsMode,mtcsMode->word_mode).require ();
   int  lastVirtualRegNo=mtcs_reg_get_last_virtual_regno(mtcsReg);

   rtxes.target = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mtcsMode->word_mode, lastVirtualRegNo/*!LAST_VIRTUAL_REGISTER*/ + 1);
   rtxes.source = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mtcsMode->word_mode, lastVirtualRegNo/*!LAST_VIRTUAL_REGISTER*/ + 2);
   rtxes.set = gen_rtx_SET (rtxes.target, rtxes.source);
   rtxes.zext = gen_rtx_ZERO_EXTEND (self->/*!twice_word_mode*/x_twice_word_mode, rtxes.source);
   rtxes.shift = gen_rtx_ASHIFT (self->/*!twice_word_mode*/x_twice_word_mode, rtxes.source, const0_rtx);
   n_debug("mtcslowersubreg.c init_lower_subreg 00 twice_word_mode:%d word_mode:%d LAST_VIRTUAL_REGISTER:%d host word:%d\n",
   self->x_twice_word_mode,mtcsMode->word_mode,lastVirtualRegNo,word_mode);
   //if (LOG_COSTS)
   n_debug("mtcslowersubreg.c \nSize costs\n==========\n\n");
   compute_costs (self,false, &rtxes);

   // if (LOG_COSTS)
   n_debug("mtcslowersubreg.c \nSpeed costs\n===========\n\n");
   compute_costs (self,true, &rtxes);
}

//////////////////////////----------------------------------移植所有方法-----------------------
static bool simple_move_operand (MtcsLowerSubreg *self,rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   if (GET_CODE (x) == SUBREG)
      x = SUBREG_REG (x);

   if (!OBJECT_P (x))
      return false;

   if (GET_CODE (x) == LABEL_REF
   || GET_CODE (x) == SYMBOL_REF
   || GET_CODE (x) == HIGH
   || GET_CODE (x) == CONST)
      return false;

   if (MEM_P (x) && (MEM_VOLATILE_P (x)
   || mtcs_recog_mode_dependent_address_p/*!mode_dependent_address_p*/(mtcsRecog,XEXP (x, 0),
         mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,x))))
      return false;

   return true;
}

/* If X is an operator that can be treated as a simple move that we
   can split, then return the operand that is operated on.  */

static rtx operand_for_swap_move_operator (MtcsLowerSubreg *self,rtx x)
{
   /* A word sized rotate of a register pair is equivalent to swapping
   the registers in the register pair.  */
   if (GET_CODE (x) == ROTATE
   && GET_MODE (x) == self->x_twice_word_mode
   && simple_move_operand(self,XEXP (x, 0))
   && CONST_INT_P (XEXP (x, 1))
   && INTVAL (XEXP (x, 1)) == BITS_PER_WORD)
      return XEXP (x, 0);

   return NULL_RTX;
}

/* If INSN is a single set between two objects that we want to split,
   return the single set.  SPEED_P says whether we are optimizing
   INSN for speed or size.

   INSN should have been passed to recog and extract_insn before this
   is called.  */

static rtx simple_move (MtcsLowerSubreg *self,rtx_insn *insn, bool speed_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   rtx x, op;
   rtx set;
   machine_mode mode;

   if (mtcsRecog->recog_data.n_operands != 2)
      return NULL_RTX;

   set = single_set (insn);
   if (!set)
      return NULL_RTX;

   x = SET_DEST (set);
   if (x != mtcsRecog->recog_data.operand[0] && x != mtcsRecog->recog_data.operand[1])
      return NULL_RTX;
   if (!simple_move_operand(self,x))
      return NULL_RTX;

   x = SET_SRC (set);
   if ((op = operand_for_swap_move_operator(self,x)) != NULL_RTX)
      x = op;

   if (x != mtcsRecog->recog_data.operand[0] && x != mtcsRecog->recog_data.operand[1])
      return NULL_RTX;
   /* For the src we can handle ASM_OPERANDS, and it is beneficial for
   things like x86 rdtsc which returns a DImode value.  */
   if (GET_CODE (x) != ASM_OPERANDS  && !simple_move_operand(self,x))
      return NULL_RTX;

   /* We try to decompose in integer modes, to avoid generating
   inefficient code copying between integer and floating point
   registers.  That means that we can't decompose if this is a
   non-integer mode for which there is no integer mode of the same
   size.  */
   mode = GET_MODE (SET_DEST (set));
   scalar_int_mode int_mode;
   if (!mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,mode)
   && (!mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,
   mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode), 0).exists (&int_mode)
   || !mtcsTarget/*!targetm.modes_tieable_p*/->modes_tieable_p(mtcsTarget,mode, int_mode)))
      return NULL_RTX;

   /* Reject PARTIAL_INT modes.  They are used for processor specific
   purposes and it's probably best not to tamper with them.  */
   if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) == MODE_PARTIAL_INT)
      return NULL_RTX;

   if (!self->x_choices[speed_p].move_modes_to_split[(int) mode])
      return NULL_RTX;

   return set;
}

/* If SET is a copy from one multi-word pseudo-register to another,
   record that in reg_copy_graph.  Return whether it is such a
   copy.  */

static bool find_pseudo_copy (MtcsLowerSubreg *self,rtx set)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   rtx dest = SET_DEST (set);
   rtx src = SET_SRC (set);
   rtx op;
   unsigned int rd, rs;
   bitmap b;
   if ((op = operand_for_swap_move_operator(self,src)) != NULL_RTX)
      src = op;
   if (!REG_P (dest) || !REG_P (src))
      return false;
   rd = REGNO (dest);
   rs = REGNO (src);
   if (mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,rd) || mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,rs))
      return false;
   b = self->reg_copy_graph[rs];
   if (b == NULL){
      b = BITMAP_ALLOC (NULL);
      self->reg_copy_graph[rs] = b;
   }
   bitmap_set_bit (b, rd);
   return true;
}

/* Look through the registers in DECOMPOSABLE_CONTEXT.  For each case
   where they are copied to another register, add the register to
   which they are copied to DECOMPOSABLE_CONTEXT.  Use
   NON_DECOMPOSABLE_CONTEXT to limit this--we don't bother to track
   copies of registers which are in NON_DECOMPOSABLE_CONTEXT.  */

static void propagate_pseudo_copies (MtcsLowerSubreg *self)
{
   auto_bitmap queue, propagate;
   bitmap_copy (queue, self->decomposable_context);
   do{
      bitmap_iterator iter;
      unsigned int i;
      bitmap_clear (propagate);
      EXECUTE_IF_SET_IN_BITMAP (queue, 0, i, iter){
         bitmap b =self->reg_copy_graph[i];
         if (b)
            bitmap_ior_and_compl_into (propagate, b, self->non_decomposable_context);
      }

      bitmap_and_compl (queue, propagate, self->decomposable_context);
      bitmap_ior_into (self->decomposable_context, propagate);
   }while (!bitmap_empty_p (queue));
}

/* A pointer to one of these values is passed to
   find_decomposable_subregs.  */

enum classify_move_insn
{
  /* Not a simple move from one location to another.  */
  NOT_SIMPLE_MOVE,
  /* A simple move we want to decompose.  */
  DECOMPOSABLE_SIMPLE_MOVE,
  /* Any other simple move.  */
  SIMPLE_MOVE
};

/* If we find a SUBREG in *LOC which we could use to decompose a
   pseudo-register, set a bit in DECOMPOSABLE_CONTEXT.  If we find an
   unadorned register which is not a simple pseudo-register copy,
   DATA will point at the type of move, and we set a bit in
   DECOMPOSABLE_CONTEXT or NON_DECOMPOSABLE_CONTEXT as appropriate.  */

static void find_decomposable_subregs (MtcsLowerSubreg *self,rtx *loc, enum classify_move_insn *pcmi)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   subrtx_var_iterator::array_type array;
   FOR_EACH_SUBRTX_VAR (iter, array, *loc, NONCONST){
      rtx x = *iter;
      if (GET_CODE (x) == SUBREG){
         rtx inner = SUBREG_REG (x);
         unsigned int regno, outer_size, inner_size, outer_words, inner_words;

         if (!REG_P (inner))
            continue;

         regno = REGNO (inner);
         if (mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,regno)){
            iter.skip_subrtxes ();
            continue;
         }

         if (!interesting_mode_p(self,GET_MODE (x), &outer_size, &outer_words)
         || !interesting_mode_p(self,GET_MODE (inner), &inner_size, &inner_words))
            continue;

         /* We only try to decompose single word subregs of multi-word
         registers.  When we find one, we return -1 to avoid iterating
         over the inner register.

         ??? This doesn't allow, e.g., DImode subregs of TImode values
         on 32-bit targets.  We would need to record the way the
         pseudo-register was used, and only decompose if all the uses
         were the same number and size of pieces.  Hopefully this
         doesn't happen much.  */

         if (outer_words == 1 && inner_words > 1
         /* Don't allow to decompose floating point subregs of
         multi-word pseudos if the floating point mode does
         not have word size, because otherwise we'd generate
         a subreg with that floating mode from a different
         sized integral pseudo which is not allowed by
         validate_subreg.  */
         && (!mtcs_mode_is_float_p/*!FLOAT_MODE_P*/(mtcsMode,GET_MODE (x)) || outer_size == UNITS_PER_WORD)){
            bitmap_set_bit (self->decomposable_context, regno);
            iter.skip_subrtxes ();
            continue;
         }

         /* If this is a cast from one mode to another, where the modes
         have the same size, and they are not tieable, then mark this
         register as non-decomposable.  If we decompose it we are
         likely to mess up whatever the backend is trying to do.  */
         if (outer_words > 1  && outer_size == inner_size
         && !mtcsTarget/*!targetm.modes_tieable_p*/->modes_tieable_p(mtcsTarget,GET_MODE (x), GET_MODE (inner))){
            bitmap_set_bit (self->non_decomposable_context, regno);
            bitmap_set_bit (self->subreg_context, regno);
            iter.skip_subrtxes ();
            continue;
         }
      }else if (REG_P (x)){
         unsigned int regno, size, words;

         /* We will see an outer SUBREG before we see the inner REG, so
         when we see a plain REG here it means a direct reference to
         the register.

         If this is not a simple copy from one location to another,
         then we cannot decompose this register.  If this is a simple
         copy we want to decompose, and the mode is right,
         then we mark the register as decomposable.
         Otherwise we don't say anything about this register --
         it could be decomposed, but whether that would be
         profitable depends upon how it is used elsewhere.

         We only set bits in the bitmap for multi-word
         pseudo-registers, since those are the only ones we care about
         and it keeps the size of the bitmaps down.  */

         regno = REGNO (x);
         if (!mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,regno)
         && interesting_mode_p(self,GET_MODE (x), &size, &words)  && words > 1){
            switch (*pcmi){
               case NOT_SIMPLE_MOVE:
                  bitmap_set_bit (self->non_decomposable_context, regno);
                  break;
               case DECOMPOSABLE_SIMPLE_MOVE:
                  if (mtcsTarget/*!targetm.modes_tieable_p*/->modes_tieable_p(mtcsTarget,GET_MODE (x), mtcsMode->word_mode))
                     bitmap_set_bit (self->decomposable_context, regno);
                  break;
               case SIMPLE_MOVE:
                  break;
               default:
                  gcc_unreachable ();
            }
         }
      }else if (MEM_P (x)){
         enum classify_move_insn cmi_mem = NOT_SIMPLE_MOVE;

         /* Any registers used in a MEM do not participate in a
         SIMPLE_MOVE or DECOMPOSABLE_SIMPLE_MOVE.  Do our own recursion
         here, and return -1 to block the parent's recursion.  */
         find_decomposable_subregs(self,&XEXP (x, 0), &cmi_mem);
         iter.skip_subrtxes ();
      }
   }
}

/* Decompose REGNO into word-sized components.  We smash the REG node
   in place.  This ensures that (1) something goes wrong quickly if we
   fail to make some replacement, and (2) the debug information inside
   the symbol table is automatically kept up to date.  */

static void decompose_register (MtcsLowerSubreg *self,unsigned int regno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   rtx reg;
   unsigned int size, words, i;
   rtvec v;

   reg = mtcsRtlData/*!regno_reg_rtx*/->regno_reg_rtx[regno];

   mtcsRtlData/*!regno_reg_rtx*/->regno_reg_rtx[regno] = NULL_RTX;

   if (!interesting_mode_p(self,GET_MODE (reg), &size, &words))
      gcc_unreachable ();

   v = rtvec_alloc (words);
   for (i = 0; i < words; ++i)
      RTVEC_ELT (v, i) = gen_reg_rtx_offset (reg, mtcsMode->word_mode, i * UNITS_PER_WORD);

   PUT_CODE (reg, CONCATN);
   XVEC (reg, 0) = v;

   if (dump_file){
      fprintf (dump_file, "; Splitting reg %u ->", regno);
      for (i = 0; i < words; ++i)
         fprintf (dump_file, " %u", REGNO (XVECEXP (reg, 0, i)));
      fputc ('\n', dump_file);
   }
}

/* Get a SUBREG of a CONCATN.  */

static rtx simplify_subreg_concatn (MtcsLowerSubreg *self,machine_mode outermode, rtx op, poly_uint64 orig_byte)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

   unsigned int outer_size, outer_words, inner_size, inner_words;
   machine_mode innermode, partmode;
   rtx part;
   unsigned int final_offset;
   unsigned int byte;

   innermode = GET_MODE (op);
   if (!interesting_mode_p(self,outermode, &outer_size, &outer_words)
   || !interesting_mode_p(self,innermode, &inner_size, &inner_words))
      gcc_unreachable ();

   /* Must be constant if interesting_mode_p passes.  */
   byte = orig_byte.to_constant ();
   gcc_assert (GET_CODE (op) == CONCATN);
   gcc_assert (byte % outer_size == 0);

   gcc_assert (byte < inner_size);
   if (outer_size > inner_size)
      return NULL_RTX;

   inner_size /= XVECLEN (op, 0);
   part = XVECEXP (op, 0, byte / inner_size);
   partmode = GET_MODE (part);

   final_offset = byte % inner_size;
   if (final_offset + outer_size > inner_size)
      return NULL_RTX;

   /* VECTOR_CSTs in debug expressions are expanded into CONCATN instead of
   regular CONST_VECTORs.  They have vector or integer modes, depending
   on the capabilities of the target.  Cope with them.  */
   if (partmode == VOIDmode && mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,innermode))
      partmode = mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,innermode);
   else if (partmode == VOIDmode)
      partmode = mtcs_mode_for_size/*!mode_for_size*/(mtcsMode,inner_size * BITS_PER_UNIT,
            mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,innermode), 0).require ();

   return mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,outermode, part, partmode, final_offset);
}

/* Wrapper around simplify_gen_subreg which handles CONCATN.  */

static rtx simplify_gen_subreg_concatn (MtcsLowerSubreg *self,machine_mode outermode, rtx op,
              machine_mode innermode, unsigned int byte)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   rtx ret;

   /* We have to handle generating a SUBREG of a SUBREG of a CONCATN.
   If OP is a SUBREG of a CONCATN, then it must be a simple mode
   change with the same size and offset 0, or it must extract a
   part.  We shouldn't see anything else here.  */
   if (GET_CODE (op) == SUBREG && GET_CODE (SUBREG_REG (op)) == CONCATN){
      rtx op2;

      if (known_eq (GET_MODE_SIZE (GET_MODE (op)), GET_MODE_SIZE (GET_MODE (SUBREG_REG (op))))
      && known_eq (SUBREG_BYTE (op), 0))
         return simplify_gen_subreg_concatn (self,outermode, SUBREG_REG (op),GET_MODE (SUBREG_REG (op)), byte);

      op2 = simplify_subreg_concatn(self,GET_MODE (op), SUBREG_REG (op),SUBREG_BYTE (op));
      if (op2 == NULL_RTX){
         /* We don't handle paradoxical subregs here.  */
         gcc_assert (!mtcs_mode_paradoxical_subreg_p/*!paradoxical_subreg_p*/(mtcsMode,outermode, GET_MODE (op)));
         gcc_assert (!mtcs_rtl_paradoxical_subreg_p/*!paradoxical_subreg_p*/(mtcsRTL,op));
         op2 = simplify_subreg_concatn(self,outermode, SUBREG_REG (op),byte + SUBREG_BYTE (op));
         gcc_assert (op2 != NULL_RTX);
         return op2;
      }

      op = op2;
      gcc_assert (op != NULL_RTX);
      gcc_assert (innermode == GET_MODE (op));
   }

   if (GET_CODE (op) == CONCATN)
      return simplify_subreg_concatn(self,outermode, op, byte);

   ret = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,outermode, op, innermode, byte);

   /* If we see an insn like (set (reg:DI) (subreg:DI (reg:SI) 0)) then
   resolve_simple_move will ask for the high part of the paradoxical
   subreg, which does not have a value.  Just return a zero.  */
   if (ret == NULL_RTX  && mtcs_rtl_paradoxical_subreg_p/*!paradoxical_subreg_p*/(mtcsRTL,op))
      return CONST0_RTX (outermode);

   gcc_assert (ret != NULL_RTX);
   return ret;
}

/* Return whether we should resolve X into the registers into which it
   was decomposed.  */
static bool resolve_reg_p (rtx x)
{
  return GET_CODE (x) == CONCATN;
}

/* Return whether X is a SUBREG of a register which we need to
   resolve.  */
static bool resolve_subreg_p (rtx x)
{
  if (GET_CODE (x) != SUBREG)
    return false;
  return resolve_reg_p (SUBREG_REG (x));
}

/* Look for SUBREGs in *LOC which need to be decomposed.  */

static bool resolve_subreg_use (MtcsLowerSubreg *self,rtx *loc, rtx insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   subrtx_ptr_iterator::array_type array;
   FOR_EACH_SUBRTX_PTR (iter, array, loc, NONCONST){
      rtx *loc = *iter;
      rtx x = *loc;
      if (resolve_subreg_p (x)){
         x = simplify_subreg_concatn(self,GET_MODE (x), SUBREG_REG (x),SUBREG_BYTE (x));
         /* It is possible for a note to contain a reference which we can
         decompose.  In this case, return 1 to the caller to indicate
         that the note must be removed.  */
         if (!x){
            gcc_assert (!insn);
            return true;
         }
         mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, loc, x, 1);
         iter.skip_subrtxes ();
      }else if (resolve_reg_p (x))
         /* Return 1 to the caller to indicate that we found a direct
         reference to a register which is being decomposed.  This can
         happen inside notes, multiword shift or zero-extend
         instructions.  */
         return true;
   }
   return false;
}

/* Resolve any decomposed registers which appear in register notes on
   INSN.  */
static void resolve_reg_notes (MtcsLowerSubreg *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);

   rtx *pnote, note;
   note = find_reg_equal_equiv_note (insn);
   if (note){
      int old_count = mtcs_recog_num_validated_changes/*!num_validated_changes*/(mtcsRecog);
      if (resolve_subreg_use(self,&XEXP (note, 0), NULL_RTX))
         mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,insn, note);
      else
         if (old_count != mtcs_recog_num_validated_changes/*!num_validated_changes*/(mtcsRecog))
            mtcs_dfscan_df_notes_rescan/*!df_notes_rescan*/(mtcsDfscan,insn);
   }
   pnote = &REG_NOTES (insn);
   while (*pnote != NULL_RTX){
      bool del = false;

      note = *pnote;
      switch (REG_NOTE_KIND (note)){
         case REG_DEAD:
         case REG_UNUSED:
            if (resolve_reg_p (XEXP (note, 0)))
               del = true;
            break;

         default:
            break;
      }
      if (del)
         *pnote = XEXP (note, 1);
      else
         pnote = &XEXP (note, 1);
   }
}

/* Return whether X can be decomposed into subwords.  */
static bool can_decompose_p (MtcsLowerSubreg *self,rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   if (REG_P (x)){
      unsigned int regno = REGNO (x);
      if (mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,regno)){
         unsigned int byte, num_bytes, num_words;
         if (!interesting_mode_p(self,GET_MODE (x), &num_bytes, &num_words))
            return false;
         for (byte = 0; byte < num_bytes; byte += UNITS_PER_WORD)
            if (mtcs_rtl_simplify_subreg_regno/*!simplify_subreg_regno*/(mtcsRTL,
                  regno, GET_MODE (x), byte, mtcsMode->word_mode) < 0)
               return false;
         return true;
      }else
         return !bitmap_bit_p (self->subreg_context, regno);
   }
   return true;
}

/* OPND is a concatn operand this is used with a simple move operator.
   Return a new rtx with the concatn's operands swapped.  */
static rtx resolve_operand_for_swap_move_operator (rtx opnd)
{
   gcc_assert (GET_CODE (opnd) == CONCATN);
   rtx concatn = copy_rtx (opnd);
   rtx op0 = XVECEXP (concatn, 0, 0);
   rtx op1 = XVECEXP (concatn, 0, 1);
   XVECEXP (concatn, 0, 0) = op1;
   XVECEXP (concatn, 0, 1) = op0;
   return concatn;
}

/* Decompose the registers used in a simple move SET within INSN.  If
   we don't change anything, return INSN, otherwise return the start
   of the sequence of moves.  */
static rtx_insn *resolve_simple_move (MtcsLowerSubreg *self,rtx set, rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   rtx src, dest, real_dest, src_op;
   rtx_insn *insns;
   machine_mode orig_mode, dest_mode;
   unsigned int orig_size, words;
   bool pushing;

   src = SET_SRC (set);
   dest = SET_DEST (set);
   orig_mode = GET_MODE (dest);

   if (!interesting_mode_p(self,orig_mode, &orig_size, &words))
      gcc_unreachable ();
   gcc_assert (words > 1);
   mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
   /* We have to handle copying from a SUBREG of a decomposed reg where
   the SUBREG is larger than word size.  Rather than assume that we
   can take a word_mode SUBREG of the destination, we copy to a new
   register and then copy that to the destination.  */

   real_dest = NULL_RTX;
   if ((src_op = operand_for_swap_move_operator(self,src)) != NULL_RTX){
      if (resolve_reg_p (dest)){
         /* DEST is a CONCATN, so swap its operands and strip
         SRC's operator.  */
         dest = resolve_operand_for_swap_move_operator (dest);
         src = src_op;
         if (resolve_reg_p (src)){
            gcc_assert (GET_CODE (src) == CONCATN);
            if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,
                  XVECEXP (dest, 0, 0),XVECEXP (src, 0, 1))){
               /* If there is overlap between the first half of the
               destination and what will be stored to the second one,
               use a temporary pseudo.  See PR114211.  */
               rtx tem = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (XVECEXP (src, 0, 1)));
               mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,tem, XVECEXP (src, 0, 1));
               src = copy_rtx (src);
               XVECEXP (src, 0, 1) = tem;
            }
         }
      }else if (resolve_reg_p (src_op)){
         /* SRC is an operation on a CONCATN, so strip the operator and
         swap the CONCATN's operands.  */
         src = resolve_operand_for_swap_move_operator (src_op);
      }
   }

   if (GET_CODE (src) == SUBREG
   && resolve_reg_p (SUBREG_REG (src))
   && (maybe_ne (SUBREG_BYTE (src), 0)
   || maybe_ne (orig_size, GET_MODE_SIZE (GET_MODE (SUBREG_REG (src)))))){
      real_dest = dest;
      dest = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,orig_mode);
      if (REG_P (real_dest))
         REG_ATTRS (dest) = REG_ATTRS (real_dest);
   }

   /* Similarly if we are copying to a SUBREG of a decomposed reg where
   the SUBREG is larger than word size.  */

   if (GET_CODE (dest) == SUBREG
   && resolve_reg_p (SUBREG_REG (dest))
   && (maybe_ne (SUBREG_BYTE (dest), 0)
   || maybe_ne (orig_size,
   mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (SUBREG_REG (dest)))))){
      rtx reg, smove;
      rtx_insn *minsn;

      reg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,orig_mode);
      minsn = mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,reg, src);
      smove = single_set (minsn);
      gcc_assert (smove != NULL_RTX);
      resolve_simple_move(self,smove, minsn);
      src = reg;
   }
   /* If we didn't have any big SUBREGS of decomposed registers, and
   neither side of the move is a register we are decomposing, then
   we don't have to do anything here.  */
   if (src == SET_SRC (set)
   && dest == SET_DEST (set)
   && !resolve_reg_p (src)
   && !resolve_subreg_p (src)
   && !resolve_reg_p (dest)
   && !resolve_subreg_p (dest)){
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      return insn;
   }
   /* It's possible for the code to use a subreg of a decomposed
   register while forming an address.  We need to handle that before
   passing the address to emit_move_insn.  We pass NULL_RTX as the
   insn parameter to resolve_subreg_use because we cannot validate
   the insn yet.  */
   if (MEM_P (src) || MEM_P (dest)){
      int acg;

      if (MEM_P (src))
         resolve_subreg_use(self,&XEXP (src, 0), NULL_RTX);
      if (MEM_P (dest))
         resolve_subreg_use(self,&XEXP (dest, 0), NULL_RTX);
      acg = mtcs_recog_apply_change_group/*!apply_change_group*/(mtcsRecog);
      gcc_assert (acg);
   }

   /* If SRC is a register which we can't decompose, or has side
   effects, we need to move via a temporary register.  */
   if (!can_decompose_p(self,src)
   || side_effects_p (src)
   || GET_CODE (src) == ASM_OPERANDS){
      rtx reg;
      reg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,orig_mode);
      if (AUTO_INC_DEC){
         rtx_insn *move = mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,reg, src);
         if (MEM_P (src)){
            rtx note = find_reg_note (insn, REG_INC, NULL_RTX);
            if (note)
               add_reg_note (move, REG_INC, XEXP (note, 0));
         }
      }else
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,reg, src);

      src = reg;
   }

   /* If DEST is a register which we can't decompose, or has side
   effects, we need to first move to a temporary register.  We
   handle the common case of pushing an operand directly.  We also
   go through a temporary register if it holds a floating point
   value.  This gives us better code on systems which can't move
   data easily between integer and floating point registers.  */
   dest_mode = orig_mode;
   pushing = mtcs_preds_push_operand/*!push_operand*/(mtcsPreds,dest, dest_mode);
   if (!can_decompose_p(self,dest)
   || (side_effects_p (dest) && !pushing)
   || (!mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,dest_mode)
   && !resolve_reg_p (dest)
   && !resolve_subreg_p (dest))){
      if (real_dest == NULL_RTX)
         real_dest = dest;
      if (!mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,dest_mode))
         dest_mode = mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,dest_mode).require ();
      dest = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,dest_mode);
      if (REG_P (real_dest))
         REG_ATTRS (dest) = REG_ATTRS (real_dest);
   }

   if (pushing){
      unsigned int i, j, jinc;

      gcc_assert (orig_size % UNITS_PER_WORD == 0);
      gcc_assert (GET_CODE (XEXP (dest, 0)) != PRE_MODIFY);
      gcc_assert (GET_CODE (XEXP (dest, 0)) != POST_MODIFY);

      if (WORDS_BIG_ENDIAN == mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc)){
         j = 0;
         jinc = 1;
      }else{
         j = words - 1;
         jinc = -1;
      }
      for (i = 0; i < words; ++i, j += jinc){
         rtx temp;

         temp = copy_rtx (XEXP (dest, 0));
         temp = mtcs_rtl_adjust_automodify_address_nv/*!adjust_automodify_address_nv*/(mtcsRTL,
               dest, mtcsMode->word_mode, temp,j * UNITS_PER_WORD);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,temp,
         simplify_gen_subreg_concatn(self,mtcsMode->word_mode, src,orig_mode,j * UNITS_PER_WORD));
      }
   }else{
      unsigned int i;
      for (i = 0; i < words; ++i){
         rtx t = simplify_gen_subreg_concatn(self,mtcsMode->word_mode, dest,dest_mode, i * UNITS_PER_WORD);
         /* simplify_gen_subreg_concatn can return (const_int 0) for
         some sub-objects of paradoxical subregs.  As a source operand,
         that's fine.  As a destination it must be avoided.  Those are
         supposed to be don't care bits, so we can just drop that store
         on the floor.  */
         if (t != CONST0_RTX (mtcsMode->word_mode))
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,t, simplify_gen_subreg_concatn(self,
                  mtcsMode->word_mode, src, orig_mode,i * UNITS_PER_WORD));
      }
   }

   if (real_dest != NULL_RTX){
      rtx mdest, smove;
      rtx_insn *minsn;

      if (dest_mode == orig_mode)
         mdest = dest;
      else
         mdest = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,orig_mode, dest, GET_MODE (dest), 0);
      minsn = mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,real_dest, mdest);

      if (AUTO_INC_DEC && MEM_P (real_dest)
      && !(resolve_reg_p (real_dest) || resolve_subreg_p (real_dest))){
         rtx note = find_reg_note (insn, REG_INC, NULL_RTX);
         if (note)
            add_reg_note (minsn, REG_INC, XEXP (note, 0));
      }
      smove = single_set (minsn);
      gcc_assert (smove != NULL_RTX);
      resolve_simple_move(self,smove, minsn);
   }

   insns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   copy_reg_eh_region_note_forward (insn, insns, NULL_RTX);
   mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,insns, insn);
   /* If we get here via self-recursion, then INSN is not yet in the insns
   chain and delete_insn will fail.  We only want to remove INSN from the
   current sequence.  See PR56738.  */
   if (in_sequence_p ())
      mtcs_emit_remove_insn/*!remove_insn*/(mtcsEmit,insn);
   else
      mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,insn);

   return insns;
}

/* Change a CLOBBER of a decomposed register into a CLOBBER of the
   component registers.  Return whether we changed something.  */

static bool resolve_clobber (MtcsLowerSubreg *self,rtx pat, rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);

   rtx reg;
   machine_mode orig_mode;
   unsigned int orig_size, words, i;
   int ret;
   reg = XEXP (pat, 0);
   /* For clobbers we can look through paradoxical subregs which
   we do not handle in simplify_gen_subreg_concatn.  */
   if (mtcs_rtl_paradoxical_subreg_p/*!paradoxical_subreg_p*/(mtcsRTL,reg))
      reg = SUBREG_REG (reg);
   if (!resolve_reg_p (reg) && !resolve_subreg_p (reg))
      return false;
   orig_mode = GET_MODE (reg);
   if (!interesting_mode_p(self,orig_mode, &orig_size, &words))
      gcc_unreachable ();
   ret = mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,NULL_RTX, &XEXP (pat, 0),
   simplify_gen_subreg_concatn(self,mtcsMode->word_mode, reg,orig_mode, 0),0);
   mtcs_dfscan_df_insn_rescan/*!df_insn_rescan*/(mtcsDfscan,insn);
   gcc_assert (ret != 0);
   for (i = words - 1; i > 0; --i){
      rtx x;
      x = simplify_gen_subreg_concatn(self,mtcsMode->word_mode, reg, orig_mode,i * UNITS_PER_WORD);
      x = gen_rtx_CLOBBER (VOIDmode, x);
      mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,x, insn);
   }
   resolve_reg_notes(self,insn);
   return true;
}

/* A USE of a decomposed register is no longer meaningful.  Return
   whether we changed something.  */

static bool resolve_use (MtcsLowerSubreg *self,rtx pat, rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);

   if (resolve_reg_p (XEXP (pat, 0)) || resolve_subreg_p (XEXP (pat, 0))){
      mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,insn);
      return true;
   }
   resolve_reg_notes(self,insn);
   return false;
}

/* A VAR_LOCATION can be simplified.  */

static void resolve_debug (MtcsLowerSubreg *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);

   subrtx_ptr_iterator::array_type array;
   FOR_EACH_SUBRTX_PTR (iter, array, &PATTERN (insn), NONCONST){
      rtx *loc = *iter;
      rtx x = *loc;
      if (resolve_subreg_p (x)){
         x = simplify_subreg_concatn(self,GET_MODE (x), SUBREG_REG (x),SUBREG_BYTE (x));

         if (x)
            *loc = x;
         else
            x = copy_rtx (*loc);
      }
      if (resolve_reg_p (x))
         *loc = copy_rtx (x);
   }
   mtcs_dfscan_df_insn_rescan/*!df_insn_rescan*/(mtcsDfscan,insn);
   resolve_reg_notes(self,insn);
}

/* Check if INSN is a decomposable multiword-shift or zero-extend and
   set the decomposable_context bitmap accordingly.  SPEED_P is true
   if we are optimizing INSN for speed rather than size.  Return true
   if INSN is decomposable.  */

static bool find_decomposable_shift_zext (MtcsLowerSubreg *self,rtx_insn *insn, bool speed_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   rtx set;
   rtx op;
   rtx op_operand;
   set = single_set (insn);
   if (!set)
      return false;

   op = SET_SRC (set);
   if (GET_CODE (op) != ASHIFT
   && GET_CODE (op) != LSHIFTRT
   && GET_CODE (op) != ASHIFTRT
   && GET_CODE (op) != ZERO_EXTEND)
      return false;

   op_operand = XEXP (op, 0);
   if (!REG_P (SET_DEST (set)) || !REG_P (op_operand)
   || mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,REGNO (SET_DEST (set)))
   || mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,REGNO (op_operand))
   || GET_MODE (op) != self->/*!twice_word_mode*/x_twice_word_mode)
      return false;

   if (GET_CODE (op) == ZERO_EXTEND){
      if (GET_MODE (op_operand) != mtcsMode->word_mode || !self->x_choices[speed_p].splitting_zext)
         return false;
   }else /* left or right shift */{
      bool *splitting = (GET_CODE (op) == ASHIFT
                           ? self->x_choices[speed_p].splitting_ashift
                           : GET_CODE (op) == ASHIFTRT
                           ? self->x_choices[speed_p].splitting_ashiftrt
                           : self->x_choices[speed_p].splitting_lshiftrt);
      if (!CONST_INT_P (XEXP (op, 1))
      || !IN_RANGE (INTVAL (XEXP (op, 1)), BITS_PER_WORD, 2 * BITS_PER_WORD - 1)
      || !splitting[INTVAL (XEXP (op, 1)) - BITS_PER_WORD])
         return false;

      bitmap_set_bit (self->decomposable_context, REGNO (op_operand));
   }

   bitmap_set_bit (self->decomposable_context, REGNO (SET_DEST (set)));
   return true;
}

/* Decompose a more than word wide shift (in INSN) of a multiword
   pseudo or a multiword zero-extend of a wordmode pseudo into a move
   and 'set to zero' insn.  SPEED_P says whether we are optimizing
   for speed or size, when checking if a ZERO_EXTEND is preferable.
   Return a pointer to the new insn when a replacement was done.  */

static rtx_insn * resolve_shift_zext (MtcsLowerSubreg *self,rtx_insn *insn, bool speed_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   rtx set;
   rtx op;
   rtx op_operand;
   rtx_insn *insns;
   rtx src_reg, dest_reg, dest_upper, upper_src = NULL_RTX;
   int src_reg_num, dest_reg_num, offset1, offset2, src_offset;
   scalar_int_mode inner_mode;

   set = single_set (insn);
   if (!set)
      return NULL;

   op = SET_SRC (set);
   if (GET_CODE (op) != ASHIFT
   && GET_CODE (op) != LSHIFTRT
   && GET_CODE (op) != ASHIFTRT
   && GET_CODE (op) != ZERO_EXTEND)
      return NULL;

   op_operand = XEXP (op, 0);
   if (!mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (op_operand), &inner_mode))
      return NULL;

   /* We can tear this operation apart only if the regs were already
   torn apart.  */
   if (!resolve_reg_p (SET_DEST (set)) && !resolve_reg_p (op_operand))
      return NULL;

   /* src_reg_num is the number of the word mode register which we
   are operating on.  For a left shift and a zero_extend on little
   endian machines this is register 0.  */
   src_reg_num = (GET_CODE (op) == LSHIFTRT || GET_CODE (op) == ASHIFTRT) ? 1 : 0;

   if (WORDS_BIG_ENDIAN && GET_MODE_SIZE (inner_mode) > UNITS_PER_WORD)
      src_reg_num = 1 - src_reg_num;

   if (GET_CODE (op) == ZERO_EXTEND)
      dest_reg_num = WORDS_BIG_ENDIAN ? 1 : 0;
   else
      dest_reg_num = 1 - src_reg_num;

   offset1 = UNITS_PER_WORD * dest_reg_num;
   offset2 = UNITS_PER_WORD * (1 - dest_reg_num);
   src_offset = UNITS_PER_WORD * src_reg_num;

   mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);

   dest_reg = simplify_gen_subreg_concatn(self,mtcsMode->word_mode, SET_DEST (set),GET_MODE (SET_DEST (set)),offset1);
   dest_upper = simplify_gen_subreg_concatn(self,mtcsMode->word_mode, SET_DEST (set),GET_MODE (SET_DEST (set)),offset2);
   src_reg = simplify_gen_subreg_concatn(self,mtcsMode->word_mode, op_operand,GET_MODE (op_operand),src_offset);
   if (GET_CODE (op) == ASHIFTRT && INTVAL (XEXP (op, 1)) != 2 * BITS_PER_WORD - 1)
      upper_src = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR,
            mtcsMode->word_mode, copy_rtx (src_reg),BITS_PER_WORD - 1, NULL_RTX, 0);

   if (GET_CODE (op) != ZERO_EXTEND){
      int shift_count = INTVAL (XEXP (op, 1));
      if (shift_count > BITS_PER_WORD)
         src_reg = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,GET_CODE (op) == ASHIFT ?
               LSHIFT_EXPR : RSHIFT_EXPR,mtcsMode->word_mode, src_reg,shift_count - BITS_PER_WORD,
               dest_reg, GET_CODE (op) != ASHIFTRT);
   }

   /* Consider using ZERO_EXTEND instead of setting DEST_UPPER to zero
   if this is considered reasonable.  */
   if (GET_CODE (op) == LSHIFTRT
   && GET_MODE (op) == self->/*!twice_word_mode*/x_twice_word_mode
   && REG_P (SET_DEST (set))
   && !self->x_choices[speed_p].splitting_zext){
      rtx tmp = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,mtcsMode->word_mode, copy_rtx (src_reg));
      tmp = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,
            ZERO_EXTEND, self->/*!twice_word_mode*/x_twice_word_mode, tmp, mtcsMode->word_mode);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,SET_DEST (set), tmp);
   }else{
      if (dest_reg != src_reg)
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,dest_reg, src_reg);
      if (GET_CODE (op) != ASHIFTRT)
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,dest_upper, CONST0_RTX (mtcsMode->word_mode));
      else if (INTVAL (XEXP (op, 1)) == 2 * BITS_PER_WORD - 1)
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,dest_upper, copy_rtx (src_reg));
      else
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,dest_upper, upper_src);
   }

   insns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,insns, insn);

   if (dump_file){
      rtx_insn *in;
      fprintf (dump_file, "; Replacing insn: %d with insns: ", INSN_UID (insn));
      for (in = insns; in != insn; in = NEXT_INSN (in))
         fprintf (dump_file, "%d ", INSN_UID (in));
      fprintf (dump_file, "\n");
   }

   mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,insn);
   return insns;
}

/* Print to dump_file a description of what we're doing with shift code CODE.
   SPLITTING[X] is true if we are splitting shifts by X + BITS_PER_WORD.  */

static void dump_shift_choices (MtcsLowerSubreg *self,enum rtx_code code, bool *splitting)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   int i;
   const char *sep;
   fprintf (dump_file, "  Splitting mode %s for %s lowering with shift amounts = ",
         mtcs_mode_get_name/*!GET_MODE_NAME*/(mtcsMode,self->/*!twice_word_mode*/x_twice_word_mode), GET_RTX_NAME (code));
   sep = "";
   for (i = 0; i < BITS_PER_WORD; i++)
      if (splitting[i]){
         fprintf (dump_file, "%s%d", sep, i + BITS_PER_WORD);
         sep = ",";
      }
   fprintf (dump_file, "\n");
}

/* Print to dump_file a description of what we're doing when optimizing
   for speed or size; SPEED_P says which.  DESCRIPTION is a description
   of the SPEED_P choice.  */

static void dump_choices (MtcsLowerSubreg *self,bool speed_p, const char *description)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   unsigned int size, factor, i;
   fprintf (dump_file, "Choices when optimizing for %s:\n", description);
   for (i = 0; i < mtcs_mode_get_max_number/*!MAX_MACHINE_MODE*/(mtcsMode); i++)
      if (interesting_mode_p(self,(machine_mode) i, &size, &factor)  && factor > 1)
         fprintf (dump_file, "  %s mode %s for copy lowering.\n",
   self->x_choices[speed_p].move_modes_to_split[i] ?
         "Splitting" : "Skipping", mtcs_mode_get_name/*!GET_MODE_NAME*/(mtcsMode,(machine_mode) i));

   fprintf (dump_file, "  %s mode %s for zero_extend lowering.\n",
   self->x_choices[speed_p].splitting_zext ? "Splitting" : "Skipping",
         mtcs_mode_get_name/*!GET_MODE_NAME*/(mtcsMode,self->/*!twice_word_mode*/x_twice_word_mode));

   dump_shift_choices(self,ASHIFT, self->x_choices[speed_p].splitting_ashift);
   dump_shift_choices(self,LSHIFTRT, self->x_choices[speed_p].splitting_lshiftrt);
   dump_shift_choices(self,ASHIFTRT, self->x_choices[speed_p].splitting_ashiftrt);
   fprintf (dump_file, "\n");
}

/* Look for registers which are always accessed via word-sized SUBREGs
   or -if DECOMPOSE_COPIES is true- via copies.  Decompose these
   registers into several word-sized pseudo-registers.  */

static void decompose_multiword_subregs (MtcsLowerSubreg *self,bool decompose_copies)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsDce *mtcsDce=mtcs_target_get_dce(mtcsTarget);
   MtcsCfgBuild *mtcsCfgBuild=mtcs_target_get_cfg_build(mtcsTarget);

   unsigned int max;
   basic_block bb;
   bool speed_p;

   if (dump_file){
      dump_choices(self,false, "size");
      dump_choices(self,true, "speed");
   }

   /* Check if this target even has any modes to consider lowering.   */
   if (!self->x_choices[false].something_to_do && !self->x_choices[true].something_to_do){
      if (dump_file)
         fprintf (dump_file, "Nothing to do!\n");
      return;
   }

   max = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);

   /* First see if there are any multi-word pseudo-registers.  If there
   aren't, there is nothing we can do.  This should speed up this
   pass in the normal case, since it should be faster than scanning
   all the insns.  */
   {
      unsigned int i;
      bool useful_modes_seen = false;

      for (i = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); i < max; ++i)
         if (mtcsRtlData/*!regno_reg_rtx*/->regno_reg_rtx[i] != NULL){
            machine_mode mode = GET_MODE (mtcsRtlData/*!regno_reg_rtx*/->regno_reg_rtx[i]);
            if (self->x_choices[false].move_modes_to_split[(int) mode]
            || self->x_choices[true].move_modes_to_split[(int) mode]){
               useful_modes_seen = true;
               break;
            }
         }

      if (!useful_modes_seen){
         if (dump_file)
            fprintf (dump_file, "Nothing to lower in this function.\n");
         return;
      }
   }

   if (df){
      mtcs_dfcore_df_set_flags/*!df_set_flags*/(mtcsDfcore,DF_DEFER_INSN_RESCAN);
      mtcs_dce_run_word_dce/*!run_word_dce*/(mtcsDce);
   }

   /* FIXME: It may be possible to change this code to look for each
   multi-word pseudo-register and to find each insn which sets or
   uses that register.  That should be faster than scanning all the
   insns.  */

   self->decomposable_context = BITMAP_ALLOC (NULL);
   self->non_decomposable_context = BITMAP_ALLOC (NULL);
   self->subreg_context = BITMAP_ALLOC (NULL);

   self->reg_copy_graph.create (max);
   self->reg_copy_graph.safe_grow_cleared (max, true);
   memset (self->reg_copy_graph.address (), 0, sizeof (bitmap) * max);

   speed_p = optimize_function_for_speed_p (cfun);
   FOR_EACH_BB_FN (bb, cfun){
      rtx_insn *insn;

      FOR_BB_INSNS (bb, insn){
         rtx set;
         enum classify_move_insn cmi;
         int i, n;

         if (!INSN_P (insn)
         || GET_CODE (PATTERN (insn)) == CLOBBER
         || GET_CODE (PATTERN (insn)) == USE)
            continue;

         mtcs_recog_recog_memoized/*!recog_memoized*/(mtcsRecog,insn);

         if (find_decomposable_shift_zext(self,insn, speed_p))
            continue;

         mtcs_recog_extract_insn/*!extract_insn*/(mtcsRecog,insn);

         set = simple_move(self,insn, speed_p);

         if (!set)
            cmi = NOT_SIMPLE_MOVE;
         else{
            /* We mark pseudo-to-pseudo copies as decomposable during the
            second pass only.  The first pass is so early that there is
            good chance such moves will be optimized away completely by
            subsequent optimizations anyway.

            However, we call find_pseudo_copy even during the first pass
            so as to properly set up the reg_copy_graph.  */
            if (find_pseudo_copy(self,set))
               cmi = decompose_copies? DECOMPOSABLE_SIMPLE_MOVE : SIMPLE_MOVE;
            else
               cmi = SIMPLE_MOVE;
         }

         n =  mtcsRecog->recog_data.n_operands;
         for (i = 0; i < n; ++i){
            find_decomposable_subregs(self,& mtcsRecog->recog_data.operand[i], &cmi);
            /* We handle ASM_OPERANDS as a special case to support
            things like x86 rdtsc which returns a DImode value.
            We can decompose the output, which will certainly be
            operand 0, but not the inputs.  */

            if (cmi == SIMPLE_MOVE  && GET_CODE (SET_SRC (set)) == ASM_OPERANDS){
               gcc_assert (i == 0);
               cmi = NOT_SIMPLE_MOVE;
            }
         }
      }
   }

   bitmap_and_compl_into (self->decomposable_context, self->non_decomposable_context);
   if (!bitmap_empty_p (self->decomposable_context)){
      unsigned int i;
      sbitmap_iterator sbi;
      bitmap_iterator iter;
      unsigned int regno;

      propagate_pseudo_copies(self);

      auto_sbitmap sub_blocks (last_basic_block_for_fn (cfun));
      bitmap_clear (sub_blocks);

      EXECUTE_IF_SET_IN_BITMAP (self->decomposable_context, 0, regno, iter)
         decompose_register(self,regno);

      FOR_EACH_BB_FN (bb, cfun){
         rtx_insn *insn;

         FOR_BB_INSNS (bb, insn){
            rtx pat;
            if (!INSN_P (insn))
               continue;
            pat = PATTERN (insn);
            if (GET_CODE (pat) == CLOBBER)
               resolve_clobber(self,pat, insn);
            else if (GET_CODE (pat) == USE)
               resolve_use(self,pat, insn);
            else if (DEBUG_INSN_P (insn))
               resolve_debug(self,insn);
            else{
               rtx set;
               int i;
               mtcs_recog_recog_memoized/*!recog_memoized*/(mtcsRecog,insn);
               mtcs_recog_extract_insn/*!extract_insn*/(mtcsRecog,insn);

               set = simple_move(self,insn, speed_p);
               if (set){
                  rtx_insn *orig_insn = insn;
                  bool cfi = mtcs_cfg_build_control_flow_insn_p/*!control_flow_insn_p*/(mtcsCfgBuild,insn);

                  /* We can end up splitting loads to multi-word pseudos
                  into separate loads to machine word size pseudos.
                  When this happens, we first had one load that can
                  throw, and after resolve_simple_move we'll have a
                  bunch of loads (at least two).  All those loads may
                  trap if we can have non-call exceptions, so they
                  all will end the current basic block.  We split the
                  block after the outer loop over all insns, but we
                  make sure here that we will be able to split the
                  basic block and still produce the correct control
                  flow graph for it.  */
                  gcc_assert (!cfi || (cfun->can_throw_non_call_exceptions && can_throw_internal (insn)));

                  insn = resolve_simple_move(self,set, insn);
                  if (insn != orig_insn){
                     mtcs_recog_recog_memoized/*!recog_memoized*/(mtcsRecog,insn);
                     mtcs_recog_extract_insn/*!extract_insn*/(mtcsRecog,insn);

                     if (cfi)
                     bitmap_set_bit (sub_blocks, bb->index);
                  }
               }else{
                  rtx_insn *decomposed_shift;

                  decomposed_shift = resolve_shift_zext(self,insn, speed_p);
                  if (decomposed_shift != NULL_RTX){
                     insn = decomposed_shift;
                     mtcs_recog_recog_memoized/*!recog_memoized*/(mtcsRecog,insn);
                     mtcs_recog_extract_insn/*!extract_insn*/(mtcsRecog,insn);
                  }
               }

               for (i = mtcsRecog->recog_data.n_operands - 1; i >= 0; --i)
                  resolve_subreg_use(self,mtcsRecog->recog_data.operand_loc[i], insn);

               resolve_reg_notes(self,insn);

               if (mtcs_recog_num_validated_changes/*!num_validated_changes*/(mtcsRecog) > 0){
                  for (i = mtcsRecog->recog_data.n_dups - 1; i >= 0; --i){
                     rtx *pl = mtcsRecog->recog_data.dup_loc[i];
                     int dup_num = mtcsRecog->recog_data.dup_num[i];
                     rtx *px = mtcsRecog->recog_data.operand_loc[dup_num];
                     mtcs_recog_validate_unshare_change/*!validate_unshare_change*/(mtcsRecog,insn, pl, *px, 1);
                  }
                  i = mtcs_recog_apply_change_group/*!apply_change_group*/(mtcsRecog);
                  gcc_assert (i);
               }
            }//end  if (GET_CODE (pat) == CLOBBER)
         }//end FOR_BB_INSNS
      } //end FOR_EACH_BB_FN

      /* If we had insns to split that caused control flow insns in the middle
      of a basic block, split those blocks now.  Note that we only handle
      the case where splitting a load has caused multiple possibly trapping
      loads to appear.  */
      EXECUTE_IF_SET_IN_BITMAP (sub_blocks, 0, i, sbi){
         rtx_insn *insn, *end;
         edge fallthru;

         bb = BASIC_BLOCK_FOR_FN (cfun, i);
         insn = BB_HEAD (bb);
         end = BB_END (bb);

         while (insn != end){
            if (mtcs_cfg_build_control_flow_insn_p/*!control_flow_insn_p*/(mtcsCfgBuild,insn)){
               /* Split the block after insn.  There will be a fallthru
               edge, which is OK so we keep it.  We have to create the
               exception edges ourselves.  */
               fallthru = mtcs_cfg_context_split_block/*!split_block*/(mtcsCfgContext,bb, insn);
               mtcs_cfg_build_rtl_make_eh_edge/*!rtl_make_eh_edge*/(mtcsCfgBuild,NULL, bb, BB_END (bb));
               bb = fallthru->dest;
               insn = BB_HEAD (bb);
            }else
               insn = NEXT_INSN (insn);
         }
      }
   }//end    if (!bitmap_empty_p (self->decomposable_context)){


   for (bitmap b : self->reg_copy_graph)
      if (b)
         BITMAP_FREE (b);

   self->reg_copy_graph.release ();

   BITMAP_FREE (self->decomposable_context);
   BITMAP_FREE (self->non_decomposable_context);
   BITMAP_FREE (self->subreg_context);
}

//rtl pass subreg1 subreg2 subreg3 需要调用的函数
void mtcs_lower_subreg_decompose_multiword_subregs(MtcsLowerSubreg *self,bool decompose_copies )
{
   decompose_multiword_subregs (self,decompose_copies);
}


MtcsLowerSubreg *mtcs_lower_subreg_new(MtcsMode *mtcsMode)
{
    MtcsLowerSubreg *self = n_slice_alloc0 (sizeof(MtcsLowerSubreg));
    mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
    mtcsLowerSubregInit(self);
    return self;
}


//原型 NEXT_PASS (pass_lower_subreg, 1);    RTL_PASS   lower-subreg.cc   subreg1 subreg2 subreg3   n  有条件执行 flag_split_wide_types != 0 decompose_multiword_subregs
static nboolean pass_lower_subreg_gate_1_cb(MtcsPass *mtcsPass,function *fun)
{
    MtcsPassLowerSubreg *self=(MtcsPassLowerSubreg *)mtcsPass;
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
    MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
    return mtcsOptionsItem->x_flag_split_wide_types != 0;
}

static nuint pass_lower_subreg_execute_1_cb(MtcsPass *mtcsPass,function *func)
{
     MtcsPassLowerSubreg *self=(MtcsPassLowerSubreg *)mtcsPass;
     MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
     MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
     MtcsLowerSubreg *mtcsLowerSubreg=mtcs_target_get_lower_subreg(mtcsTarget);
     mtcs_lower_subreg_decompose_multiword_subregs(mtcsLowerSubreg,false);
     return 0;
}

static nboolean pass_lower_subreg_gate_2_cb(MtcsPass *mtcsPass,function *fun)
{
    MtcsPassLowerSubreg *self=(MtcsPassLowerSubreg *)mtcsPass;
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
    MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
    return mtcsOptionsItem->x_flag_split_wide_types && mtcsOptionsItem->x_flag_split_wide_types_early;
}

static nuint pass_lower_subreg_execute_2_cb(MtcsPass *mtcsPass,function *func)
{
     MtcsPassLowerSubreg *self=(MtcsPassLowerSubreg *)mtcsPass;
     MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
     MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
     MtcsLowerSubreg *mtcsLowerSubreg=mtcs_target_get_lower_subreg(mtcsTarget);
     mtcs_lower_subreg_decompose_multiword_subregs(mtcsLowerSubreg,true);
     return 0;
}

static nboolean pass_lower_subreg_gate_3_cb(MtcsPass *mtcsPass,function *fun)
{
    MtcsPassLowerSubreg *self=(MtcsPassLowerSubreg *)mtcsPass;
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
    MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
    return mtcsOptionsItem->x_flag_split_wide_types;
}

static void mtcsPassLowerSubregInit(MtcsPassLowerSubreg *self,int num)
{
      MtcsPass *mtcsPass=(MtcsPass *)self;
      if(num==1){
        mtcsPass->execute =pass_lower_subreg_execute_1_cb;
        mtcsPass->gate=pass_lower_subreg_gate_1_cb;
      }else if(num==2){
         mtcsPass->execute =pass_lower_subreg_execute_2_cb;
         mtcsPass->gate=pass_lower_subreg_gate_2_cb;
      }else{
         mtcsPass->execute =pass_lower_subreg_execute_2_cb;
         mtcsPass->gate=pass_lower_subreg_gate_3_cb;
      }

      mtcs_pass_set_properties((MtcsPass *)self,
              0,/* properties_required */
              0, /* properties_provided */
              0 /* properties_destroyed */);
      if(num==1){
         mtcs_pass_set_todo_flags((MtcsPass *)self,
                    0, /* todo_flags_start */
                    0 /*todo_flags_finish */);
      }else{
         mtcs_pass_set_todo_flags((MtcsPass *)self,
                   0, /* todo_flags_start */
                   TODO_df_finish /*todo_flags_finish */);
      }
}

MtcsPassLowerSubreg *mtcs_pass_lower_subreg_new(MtcsMode *mtcsMode,int num)
{
   MtcsPassLowerSubreg *self = n_slice_alloc0 (sizeof(MtcsPassLowerSubreg));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   if(num==1)
     mtcs_pass_init((MtcsPass *)self,RTL_PASS,"subreg1");
   else if(num==2)
     mtcs_pass_init((MtcsPass *)self,RTL_PASS,"subreg2");
   else
    mtcs_pass_init((MtcsPass *)self,RTL_PASS,"subreg3");

   mtcsPassLowerSubregInit(self,num);
   return self;
}
