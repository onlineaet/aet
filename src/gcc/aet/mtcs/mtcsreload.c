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
 * base on reload.cc
 */

/* This file handles generation of all the assembler code
   *except* the instructions of a function.
   This includes declarations of variables and their initial values.

   We also output the assembler code for constants stored in reloadory
   and are responsible for combining constants with the same value.  */


#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "optabs.h"
#include "regs.h"
#include "ira.h"
#include "recog.h"
#include "rtl-error.h"
#include "reload.h"
#include "addresses.h"
#include "function-abi.h"
#include "reload.h"

#include "mtcsreload.h"
#include "mtcsreg.h"
#include "mtcstarget.h"

/* Used to track what is modified by an operand.  */
struct decomposition
{
  int reg_flag;      /* Nonzero if referencing a register.  */
  int safe;    /* Nonzero if this can't conflict with anything.  */
  rtx base;    /* Base address for MEM.  */
  poly_int64 start;  /* Starting offset or register number.  */
  poly_int64 end; /* Ending offset or register number.  */
};


//rld是声明在reload的全局变量，要放在mtcsreload中吗？
//原型 copy_replacements_1 reload.cc
static void copy_replacements_1 ( MtcsReload *self,rtx *px, rtx *py, int orig_replacements);

static void mtcsReloadInit(MtcsReload *self)
{

}

/* If LOC was scheduled to be replaced by something, return the replacement.
   Otherwise, return *LOC.  */
//原型 find_replacement reload.h reload.cc
rtx mtcs_reload_find_replacement (MtcsReload *self,rtx *loc)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

  MtcsReplacement *r;

  for (r = &self->replacements[0]; r < &self->replacements[self->n_replacements]; r++){
      rtx reloadreg = rld[r->what].reg_rtx;

      if (reloadreg && r->where == loc){
          if (r->mode != VOIDmode && GET_MODE (reloadreg) != r->mode)
            reloadreg = mtcs_reload_adjust_reg_for_mode (self,reloadreg, r->mode);
          return reloadreg;
      }else if (reloadreg && GET_CODE (*loc) == SUBREG  && r->where == &SUBREG_REG (*loc)){
          if (r->mode != VOIDmode && GET_MODE (reloadreg) != r->mode)
            reloadreg = mtcs_reload_adjust_reg_for_mode (self,reloadreg, r->mode);

          return mtcs_simplify_rtx_gen_subreg (mtcsSimplifyRtx,GET_MODE (*loc), reloadreg,GET_MODE (SUBREG_REG (*loc)), SUBREG_BYTE (*loc));
      }
  }

  /* If *LOC is a PLUS, MINUS, or MULT, see if a replacement is scheduled for
     what's inside and make a new rtl if so.  */
  if (GET_CODE (*loc) == PLUS || GET_CODE (*loc) == MINUS || GET_CODE (*loc) == MULT){
      rtx x = mtcs_reload_find_replacement (self,&XEXP (*loc, 0));
      rtx y = mtcs_reload_find_replacement (self,&XEXP (*loc, 1));

      if (x != XEXP (*loc, 0) || y != XEXP (*loc, 1))
          return gen_rtx_fmt_ee (GET_CODE (*loc), GET_MODE (*loc), x, y);
  }
  return *loc;
}

/* Find the low part, with mode MODE, of a hard regno RELOADREG.  */
//原型 reload_adjust_reg_for_mode reload.h reload.cc
rtx mtcs_reload_adjust_reg_for_mode (MtcsReload *self,rtx reloadreg, machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  int regno;

  if (GET_MODE (reloadreg) == mode)
    return reloadreg;

  regno = REGNO (reloadreg);

  if (REG_WORDS_BIG_ENDIAN)
    regno += ((int) REG_NREGS (reloadreg)
          - (int) mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/ (mtcsReg,regno, mode));

  return mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mode, regno);
}

/* Make a copy of any replacements being done into X and move those
   copies to locations in Y, a copy of X.  */
//原型 copy_replacements reload.h reload.cc
void mtcs_reload_copy_replacements ( MtcsReload *self,rtx x, rtx y)
{
  copy_replacements_1 (self,&x, &y, self->n_replacements);
}
//原型 copy_replacements_1 reload.cc
static void copy_replacements_1 ( MtcsReload *self,rtx *px, rtx *py, int orig_replacements)
{
  int i, j;
  rtx x, y;
  MtcsReplacement *r;
  enum rtx_code code;
  const char *fmt;

  for (j = 0; j < orig_replacements; j++)
    if (self->replacements[j].where == px){
        r = &self->replacements[self->n_replacements++];
        r->where = py;
        r->what = self->replacements[j].what;
        r->mode = self->replacements[j].mode;
    }

  x = *px;
  y = *py;
  code = GET_CODE (x);
  fmt = GET_RTX_FORMAT (code);

  for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--){
      if (fmt[i] == 'e')
          copy_replacements_1 (self,&XEXP (x, i), &XEXP (y, i), orig_replacements);
      else if (fmt[i] == 'E')
        for (j = XVECLEN (x, i); --j >= 0; )
          copy_replacements_1 (self,&XVECEXP (x, i, j), &XVECEXP (y, i, j),orig_replacements);
  }
}

//原型 init_reload reload.h reload1.cc 依赖ira_use_lra_p
void mtcs_reload_init_reload (MtcsReload *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   int i;

   /* Often (MEM (REG n)) is still valid even if (REG n) is put on the stack.
   Set spill_indirect_levels to the number of levels such addressing is
   permitted, zero if it is not permitted at all.  */
   machine_mode pMode=(machine_mode)mtcs_mode_get_Pmode(mtcsMode);
   nuint lastVirtualRegister=mtcs_reg_get_last_virtual_regno(mtcsReg);
   rtx tem= gen_rtx_MEM (pMode,  gen_rtx_PLUS (pMode, mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,pMode,
         lastVirtualRegister/*!LAST_VIRTUAL_REGISTER*/ + 1), mtcs_rtl_gen_int_mode/*!gen_int_mode*/ (mtcsRTL,4, pMode)));
   self->target_reload.x_spill_indirect_levels = 0;
   while (mtcs_recog_memory_address_p/*!memory_address_p*/(mtcsRecog,mtcsMode->modes.M_QImode/*!QImode*/,tem)){
      self->target_reload.x_spill_indirect_levels++;
      tem = gen_rtx_MEM (pMode, tem);
   }

   /* See if indirect addressing is valid for (MEM (SYMBOL_REF ...)).  */

   tem = gen_rtx_MEM (pMode, gen_rtx_SYMBOL_REF (pMode, "foo"));
   self->target_reload.x_indirect_symref_ok = mtcs_recog_memory_address_p/*!memory_address_p*/(mtcsRecog,
   mtcsMode->modes.M_QImode/*!QImode*/,tem);

   /* See if reg+reg is a valid (and offsettable) address.  */
   int  firstPseudoRegister=  mtcs_reg_get_first_pseudo_register(mtcsReg);
   for (i = 0; i < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; i++){
      tem = gen_rtx_PLUS (pMode,
         mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,pMode, mtcsReg->normalHardRegsNum.hard_frame_pointer_regnum/*HARD_FRAME_POINTER_REGNUM*/),
         mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,pMode, i));

      /* This way, we make sure that reg+reg is an offsettable address.  */
      tem = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, tem, 4);

      for (int mode = 0; mode < mtcs_mode_get_max_number/*!MAX_MACHINE_MODE*/(mtcsMode); mode++)
         if (!self->target_reload.x_double_reg_address_ok/*!double_reg_address_ok*/[mode]
                   && mtcs_recog_memory_address_p/*!memory_address_p*/(mtcsRecog,(enum machine_mode)mode, tem))
            self->target_reload.x_double_reg_address_ok/*!double_reg_address_ok*/[mode] = 1;
   }

   /* Initialize obstack for our rtl allocation.  */
//   if (reload_startobj == NULL){
//      gcc_obstack_init (&reload_obstack);
//      reload_startobj = XOBNEWVAR (&reload_obstack, char, 0);
//   }
//
//   INIT_REG_SET (&spilled_pseudos);
//   INIT_REG_SET (&changed_allocation_pseudos);
//   INIT_REG_SET (&pseudos_counted);
}


/* Like rtx_equal_p except that it allows a REG and a SUBREG to match
   if they are the same hard reg, and has special hacks for
   autoincrement and autodecrement.
   This is specifically intended for find_reloads to use
   in determining whether two operands match.
   X is the operand whose number is the lower of the two.

   The value is 2 if Y contains a pre-increment that matches
   a non-incrementing address in X.  */

/* ??? To be completely correct, we should arrange to pass
   for X the output operand and for Y the input operand.
   For now, we assume that the output operand has the lower number
   because that is natural in (SET output (... input ...)).  */
//原型 operands_match_p reload.h reload.c
int mtcs_reload_operands_match_p (MtcsReload *self,rtx x, rtx y)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   int firstPseudoRegister= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   int i;
   RTX_CODE code = GET_CODE (x);
   const char *fmt;
   int success_2;

   if (x == y)
      return 1;
   if ((code == REG || (code == SUBREG && REG_P (SUBREG_REG (x))))
   && (REG_P (y) || (GET_CODE (y) == SUBREG && REG_P (SUBREG_REG (y))))){
      int j;

      if (code == SUBREG){
         i = REGNO (SUBREG_REG (x));
         if (i >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
            goto slow;
         i += mtcs_rtlanal_subreg_regno_offset/*!subreg_regno_offset*/(mtcsRtlanal,REGNO (SUBREG_REG (x)),
               GET_MODE (SUBREG_REG (x)),SUBREG_BYTE (x),GET_MODE (x));
      }else
         i = REGNO (x);

      if (GET_CODE (y) == SUBREG){
         j = REGNO (SUBREG_REG (y));
         if (j >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
            goto slow;
         j +=mtcs_rtlanal_subreg_regno_offset/*!subreg_regno_offset*/(mtcsRtlanal,REGNO (SUBREG_REG (y)),
               GET_MODE (SUBREG_REG (y)),SUBREG_BYTE (y),GET_MODE (y));
      }else
         j = REGNO (y);

      /* On a REG_WORDS_BIG_ENDIAN machine, point to the last register of a
      multiple hard register group of scalar integer registers, so that
      for example (reg:DI 0) and (reg:SI 1) will be considered the same
      register.  */
      scalar_int_mode xmode;
      if (REG_WORDS_BIG_ENDIAN
      && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (x), &xmode)
      && mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,xmode) > UNITS_PER_WORD
      && i < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
         i += mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,i, xmode) - 1;
      scalar_int_mode ymode;
      if (REG_WORDS_BIG_ENDIAN
      && mtcs_mode_is_a<scalar_int_mode>(mtcsMode,GET_MODE (y), &ymode)
      && mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,ymode) > UNITS_PER_WORD
      && j < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
         j += mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,j, ymode) - 1;

      return i == j;
   }
   /* If two operands must match, because they are really a single
   operand of an assembler insn, then two postincrements are invalid
   because the assembler insn would increment only once.
   On the other hand, a postincrement matches ordinary indexing
   if the postincrement is the output operand.  */
   if (code == POST_DEC || code == POST_INC || code == POST_MODIFY)
      return mtcs_reload_operands_match_p/*!operands_match_p*/(self,XEXP (x, 0), y);
   /* Two preincrements are invalid
   because the assembler insn would increment only once.
   On the other hand, a preincrement matches ordinary indexing
   if the preincrement is the input operand.
   In this case, return 2, since some callers need to do special
   things when this happens.  */
   if (GET_CODE (y) == PRE_DEC || GET_CODE (y) == PRE_INC || GET_CODE (y) == PRE_MODIFY)
      return mtcs_reload_operands_match_p/*!operands_match_p*/(self,x, XEXP (y, 0)) ? 2 : 0;

slow:

   /* Now we have disposed of all the cases in which different rtx codes
   can match.  */
   if (code != GET_CODE (y))
      return 0;

   /* (MULT:SI x y) and (MULT:HI x y) are NOT equivalent.  */
   if (GET_MODE (x) != GET_MODE (y))
      return 0;

   /* MEMs referring to different address space are not equivalent.  */
   if (code == MEM && MEM_ADDR_SPACE (x) != MEM_ADDR_SPACE (y))
      return 0;

   switch (code){
      CASE_CONST_UNIQUE:
         return 0;

      case CONST_VECTOR:
         if (!same_vector_encodings_p (x, y))
            return false;
         break;

      case LABEL_REF:
         return label_ref_label (x) == label_ref_label (y);
      case SYMBOL_REF:
         return XSTR (x, 0) == XSTR (y, 0);

      default:
         break;
   }

   /* Compare the elements.  If any pair of corresponding elements
   fail to match, return 0 for the whole things.  */

   success_2 = 0;
   fmt = GET_RTX_FORMAT (code);
   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--){
      int val, j;
      switch (fmt[i]){
         case 'w':
            if (XWINT (x, i) != XWINT (y, i))
               return 0;
            break;

         case 'i':
            if (XINT (x, i) != XINT (y, i))
               return 0;
            break;

         case 'p':
            if (maybe_ne (SUBREG_BYTE (x), SUBREG_BYTE (y)))
               return 0;
            break;

         case 'e':
            val =mtcs_reload_operands_match_p/*!operands_match_p*/(self,XEXP (x, i), XEXP (y, i));
            if (val == 0)
               return 0;
            /* If any subexpression returns 2,
            we should return 2 if we are successful.  */
            if (val == 2)
               success_2 = 1;
            break;

         case '0':
            break;

         case 'E':
            if (XVECLEN (x, i) != XVECLEN (y, i))
               return 0;
            for (j = XVECLEN (x, i) - 1; j >= 0; --j){
               val = mtcs_reload_operands_match_p/*!operands_match_p*/(self,XVECEXP (x, i, j), XVECEXP (y, i, j));
               if (val == 0)
                  return 0;
               if (val == 2)
                  success_2 = 1;
            }
            break;

         /* It is believed that rtx's at this level will never
         contain anything but integers and other rtx's,
         except for within LABEL_REFs and SYMBOL_REFs.  */
         default:
            gcc_unreachable ();
      }
   }
   return 1 + success_2;
}

/* Return nonzero if register in range [REGNO, ENDREGNO)
   appears either explicitly or implicitly in X
   other than being stored into (except for earlyclobber operands).

   References contained within the substructure at LOC do not count.
   LOC may be zero, meaning don't ignore anything.

   This is similar to refers_to_regno_p in rtlanal.cc except that we
   look at equivalences for pseudos that didn't get hard registers.  */
//原型 refers_to_regno_for_reload_p reload.cc
static int refers_to_regno_for_reload_p (MtcsReload *self,unsigned int regno, unsigned int endregno,
               rtx x, rtx *loc)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   int firstPseudoRegister= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);
   int i;
   unsigned int r;
   RTX_CODE code;
   const char *fmt;

   if (x == 0)
      return 0;

repeat:
   code = GET_CODE (x);

   switch (code){
      case REG:
         r = REGNO (x);

         /* If this is a pseudo, a hard register must not have been allocated.
         X must therefore either be a constant or be in memory.  */
         if (r >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/){
            if (reg_equiv_memory_loc (r))
               return refers_to_regno_for_reload_p(self,regno, endregno,reg_equiv_memory_loc (r), (rtx*) 0);

            gcc_assert (reg_equiv_constant (r) || reg_equiv_invariant (r));
            return 0;
         }

         return endregno > r && regno < END_REGNO (x);

      case SUBREG:
         /* If this is a SUBREG of a hard reg, we can see exactly which
         registers are being modified.  Otherwise, handle normally.  */
         if (REG_P (SUBREG_REG (x)) && REGNO (SUBREG_REG (x)) < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/){
            unsigned int inner_regno = mtcs_rtlanal_subreg_regno/*!subreg_regno*/(mtcsRtlanal,x);
            unsigned int inner_endregno = inner_regno +
                  (inner_regno < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/ ?
                        mtcs_rtlanal_subreg_nregs/*!subreg_nregs*/(mtcsRtlanal,x) : 1);

            return endregno > inner_regno && regno < inner_endregno;
         }
         break;

      case CLOBBER:
      case SET:
         if (&SET_DEST (x) != loc
         /* Note setting a SUBREG counts as referring to the REG it is in for
         a pseudo but not for hard registers since we can
         treat each word individually.  */
         && ((GET_CODE (SET_DEST (x)) == SUBREG
         && loc != &SUBREG_REG (SET_DEST (x))
         && REG_P (SUBREG_REG (SET_DEST (x)))
         && REGNO (SUBREG_REG (SET_DEST (x))) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/
         && refers_to_regno_for_reload_p(self,regno, endregno, SUBREG_REG (SET_DEST (x)), loc))
         /* If the output is an earlyclobber operand, this is
         a conflict.  */
         || ((!REG_P (SET_DEST (x))
         || mtcs_reload_earlyclobber_operand_p/*!earlyclobber_operand_p*/(self,SET_DEST (x)))
         && refers_to_regno_for_reload_p(self,regno, endregno,SET_DEST (x), loc))))
            return 1;

         if (code == CLOBBER || loc == &SET_SRC (x))
            return 0;
         x = SET_SRC (x);
         goto repeat;

      default:
         break;
   }

   /* X does not match, so try its subexpressions.  */

   fmt = GET_RTX_FORMAT (code);
   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--){
      if (fmt[i] == 'e' && loc != &XEXP (x, i)){
         if (i == 0){
            x = XEXP (x, 0);
            goto repeat;
         }else
            if (refers_to_regno_for_reload_p(self,regno, endregno,XEXP (x, i), loc))
               return 1;
      }else if (fmt[i] == 'E'){
         int j;
         for (j = XVECLEN (x, i) - 1; j >= 0; j--)
            if (loc != &XVECEXP (x, i, j) && refers_to_regno_for_reload_p(self,regno, endregno,XVECEXP (x, i, j), loc))
               return 1;
      }
   }
   return 0;
}

/* Describe the range of registers or memory referenced by X.
   If X is a register, set REG_FLAG and put the first register
   number into START and the last plus one into END.
   If X is a memory reference, put a base address into BASE
   and a range of integer offsets into START and END.
   If X is pushing on the stack, we can assume it causes no trouble,
   so we set the SAFE field.  */
//原型 decompose reload.cc
static struct decomposition decompose (MtcsReload *self,rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);

   int firstPseudoRegister= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);
   int stackPointerRegnum=mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg);
   struct decomposition val;
   int all_const = 0, regno;

   memset (&val, 0, sizeof (val));

   switch (GET_CODE (x)){
      case MEM:
      {
         rtx base = NULL_RTX, offset = 0;
         rtx addr = XEXP (x, 0);

         if (GET_CODE (addr) == PRE_DEC || GET_CODE (addr) == PRE_INC
         || GET_CODE (addr) == POST_DEC || GET_CODE (addr) == POST_INC){
            val.base = XEXP (addr, 0);
            val.start = -mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (x));
            val.end = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (x));
            val.safe = REGNO (val.base) == stackPointerRegnum/*!STACK_POINTER_REGNUM*/;
            return val;
         }

         if (GET_CODE (addr) == PRE_MODIFY || GET_CODE (addr) == POST_MODIFY){
            if (GET_CODE (XEXP (addr, 1)) == PLUS
            && XEXP (addr, 0) == XEXP (XEXP (addr, 1), 0)
            && CONSTANT_P (XEXP (XEXP (addr, 1), 1))){
               val.base  = XEXP (addr, 0);
               val.start = -INTVAL (XEXP (XEXP (addr, 1), 1));
               val.end   = INTVAL (XEXP (XEXP (addr, 1), 1));
               val.safe  = REGNO (val.base) == stackPointerRegnum/*!STACK_POINTER_REGNUM*/;
               return val;
            }
         }

         if (GET_CODE (addr) == CONST){
            addr = XEXP (addr, 0);
            all_const = 1;
         }
         if (GET_CODE (addr) == PLUS){
            if (CONSTANT_P (XEXP (addr, 0))){
               base = XEXP (addr, 1);
               offset = XEXP (addr, 0);
            }else if (CONSTANT_P (XEXP (addr, 1))){
               base = XEXP (addr, 0);
               offset = XEXP (addr, 1);
            }
         }

         if (offset == 0){
            base = addr;
            offset = const0_rtx;
         }
         if (GET_CODE (offset) == CONST)
            offset = XEXP (offset, 0);
         if (GET_CODE (offset) == PLUS){
            if (CONST_INT_P (XEXP (offset, 0))){
               base = gen_rtx_PLUS (GET_MODE (base), base, XEXP (offset, 1));
               offset = XEXP (offset, 0);
            }else if (CONST_INT_P (XEXP (offset, 1))){
               base = gen_rtx_PLUS (GET_MODE (base), base, XEXP (offset, 0));
               offset = XEXP (offset, 1);
            }else{
               base = gen_rtx_PLUS (GET_MODE (base), base, offset);
               offset = const0_rtx;
            }
         }else if (!CONST_INT_P (offset)){
            base = gen_rtx_PLUS (GET_MODE (base), base, offset);
            offset = const0_rtx;
         }

         if (all_const && GET_CODE (base) == PLUS)
            base = gen_rtx_CONST (GET_MODE (base), base);

         gcc_assert (CONST_INT_P (offset));

         val.start = INTVAL (offset);
         val.end = val.start + mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (x));
         val.base = base;
      }
         break;

      case REG:
         val.reg_flag = 1;
         regno = mtcs_dojump_true_regnum/*!true_regnum*/(mtcsDojump,x);
         if (regno < 0 || regno >=firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/){
            /* A pseudo with no hard reg.  */
            val.start = REGNO (x);
            val.end = val.start + 1;
         }else{
            /* A hard reg.  */
            val.start = regno;
            val.end = mtcs_reg_end_hard_regno/*!end_hard_regno*/(mtcsReg,GET_MODE (x), regno);
         }
         break;

      case SUBREG:
         if (!REG_P (SUBREG_REG (x)))
            /* This could be more precise, but it's good enough.  */
            return decompose(self,SUBREG_REG (x));
         regno = mtcs_dojump_true_regnum/*!true_regnum*/(mtcsDojump,x);
         if (regno < 0 || regno >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
            return decompose(self,SUBREG_REG (x));

         /* A hard reg.  */
         val.reg_flag = 1;
         val.start = regno;
         val.end = regno + mtcs_rtlanal_subreg_nregs/*!subreg_nregs*/(mtcsRtlanal,x);
         break;

      case SCRATCH:
         /* This hasn't been assigned yet, so it can't conflict yet.  */
         val.safe = 1;
         break;

      default:
         gcc_assert (CONSTANT_P (x));
         val.safe = 1;
         break;
   }
   return val;
}

/* Return 1 if altering Y will not modify the value of X.
   Y is also described by YDATA, which should be decompose (Y).  */
//原型 immune_p reload.cc
static int immune_p (MtcsReload *self,rtx x, rtx y, struct decomposition ydata)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   struct decomposition xdata;

   if (ydata.reg_flag)
      /* In this case the decomposition structure contains register
      numbers rather than byte offsets.  */
      return !refers_to_regno_for_reload_p(self,ydata.start.to_constant (),ydata.end.to_constant (),x, (rtx *) 0);
   if (ydata.safe)
      return 1;

   gcc_assert (MEM_P (y));
   /* If Y is memory and X is not, Y can't affect X.  */
   if (!MEM_P (x))
      return 1;

   xdata = decompose(self,x);

   if (! rtx_equal_p (xdata.base, ydata.base)){
      /* If bases are distinct symbolic constants, there is no overlap.  */
      if (CONSTANT_P (xdata.base) && CONSTANT_P (ydata.base))
         return 1;
      /* Constants and stack slots never overlap.  */
      if (CONSTANT_P (xdata.base)
      && (ydata.base == mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL)
      || ydata.base == mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL)
      || ydata.base == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)))
         return 1;
      if (CONSTANT_P (ydata.base)
      && (xdata.base == mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL)
      || xdata.base == mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL)
      || xdata.base == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)))
         return 1;
      /* If either base is variable, we don't know anything.  */
      return 0;
   }

   return known_ge (xdata.start, ydata.end) || known_ge (ydata.start, xdata.end);
}


/* Similar, but calls decompose.  */
//原型 safe_from_earlyclobber reload.h reload.cc
int mtcs_reload_safe_from_earlyclobber (MtcsReload *self,rtx op, rtx clobber)
{
  struct decomposition early_data;

  early_data = decompose(self,clobber);
  return immune_p(self,op, clobber, early_data);
}

/* This page contains subroutines used mainly for determining
   whether the IN or an OUT of a reload can serve as the
   reload register.  */

/* Return 1 if X is an operand of an insn that is being earlyclobbered.  */
//原型 earlyclobber_operand_p reload.h reload.cc
int mtcs_reload_earlyclobber_operand_p (MtcsReload *self,rtx x)
{
  int i;
  for (i = 0; i < self->n_earlyclobbers; i++)
    if (self->reload_earlyclobbers[i] == x)
      return 1;

  return 0;
}

/* Compute cost of moving registers to/from memory.  */
//原型 memory_move_cost reload.h reginfo.cc
int mtcs_reload_memory_move_cost (MtcsReload *self,machine_mode mode, reg_class_t rclass, bool in)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   return mtcsTarget/*!targetm.memory_move_cost*/->memory_move_cost(mtcsTarget,mode, rclass, in);
}


/* ICODE is the insn_code of a reload pattern.  Check that it has exactly
   three operands, verify that operand 2 is an output operand, and return
   its register class.
   ??? We'd like to be able to handle any pattern with at least 2 operands,
   for zero or more scratch registers, but that needs more infrastructure.  */
//原型 scratch_reload_class reload.h reload.cc
enum reg_class mtcs_reload_scratch_reload_class (MtcsReload *self,enum insn_code icode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsPreds  *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);

   const char *scratch_constraint;
   enum reg_class rclass;

   gcc_assert (mtcsOutput->insn_data[(int) icode].n_operands == 3);
   scratch_constraint = mtcsOutput->insn_data[(int) icode].operand[2].constraint;
   gcc_assert (*scratch_constraint == '=');
   scratch_constraint++;
   if (*scratch_constraint == '&')
      scratch_constraint++;
   rclass = mtcs_preds_reg_class_for_constraint/*!reg_class_for_constraint*/(mtcsPreds,
         mtcs_preds_lookup_constraint/*!lookup_constraint*/(mtcsPreds,scratch_constraint));
   gcc_assert (rclass != NO_REGS);
   return rclass;
}

/* If a secondary reload is needed, return its class.  If both an intermediate
   register and a scratch register is needed, we return the class of the
   intermediate register.  */
//原型 secondary_reload_class reload.h reload.cc
reg_class_t mtcs_reload_secondary_reload_class (MtcsReload *self,bool in_p,
      reg_class_t rclass, machine_mode mode,rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   enum insn_code icode;
   secondary_reload_info sri;

   sri.icode = CODE_FOR_nothing;
   sri.prev_sri = NULL;
   rclass = (enum reg_class)mtcsTarget->/*!targetm.secondary_reload*/secondary_reload(mtcsTarget,in_p, x, rclass, mode, &sri);
   icode = (enum insn_code) sri.icode;

   /* If there are no secondary reloads at all, we return NO_REGS.
   If an intermediate register is needed, we return its class.  */
   if (icode == CODE_FOR_nothing || rclass != NO_REGS)
      return rclass;

   /* No intermediate register is needed, but we have a special reload
   pattern, which we assume for now needs a scratch register.  */
   return mtcs_reload_scratch_reload_class/*!scratch_reload_class*/(self,icode);
}

/* Compute cost of moving data from a register of class FROM to one of
   TO, using MODE.  */
//原型 register_move_cost reload.h reginfo.cc
int mtcs_reload_register_move_cost (MtcsReload *self,machine_mode mode, reg_class_t from, reg_class_t to)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   return mtcsTarget/*!targetm.register_move_cost*/->register_move_cost(mtcsTarget,mode, from, to);
}

/* Compute extra cost of moving registers to/from memory due to reloads.
   Only needed if secondary reloads are required for memory moves.  */
//原型 memory_move_secondary_cost reload.h reginfo.cc
int mtcs_reload_memory_move_secondary_cost (MtcsReload *self,machine_mode mode, reg_class_t rclass,
             bool in)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   reg_class_t altclass;
   int partial_cost = 0;
   /* We need a memory reference to feed to SECONDARY... macros.  */
   /* mem may be unused even if the SECONDARY_ macros are defined.  */
   rtx mem ATTRIBUTE_UNUSED =mtcsRTL->x_top_of_stack/*!top_of_stack*/[(int) mode];

   altclass = mtcs_reload_secondary_reload_class/*!secondary_reload_class*/(self,in ? 1 : 0, rclass, mode, mem);

   if (altclass == NO_REGS)
      return 0;

   if (in)
      partial_cost = mtcs_reload_register_move_cost/*!register_move_cost*/(self,mode, altclass, rclass);
   else
      partial_cost = mtcs_reload_register_move_cost/*!register_move_cost*/(self,mode, rclass, altclass);

   if (rclass == altclass)
      /* This isn't simply a copy-to-temporary situation.  Can't guess
      what it is, so TARGET_MEMORY_MOVE_COST really ought not to be
      calling here in that case.

      I'm tempted to put in an assert here, but returning this will
      probably only give poor estimates, which is what we would've
      had before this code anyways.  */
      return partial_cost;

   /* Check if the secondary reload register will also need a
   secondary reload.  */
   return mtcs_reload_memory_move_secondary_cost/*!memory_move_secondary_cost*/(self,mode, altclass, in) + partial_cost;
}

MtcsReload *mtcs_reload_new(MtcsMode *mtcsMode)
{
     MtcsReload *self = n_slice_alloc0 (sizeof(MtcsReload));
     mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
     mtcsReloadInit(self);
     return self;
}
