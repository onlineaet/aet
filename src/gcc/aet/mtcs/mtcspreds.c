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
   *except* the instructions of a predstion.
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

#include "mtcspreds.h"
#include "mtcstarget.h"
#include "mtcsreg.h"
#include "mtcscompile.h"

void  mtcs_preds_init (MtcsPreds *self)
{

}

/**
 * 原型 recog.cc build/tm-preds.h
 * reload_completed rtl.h final.cc rest_of_clean_state 置为0
 */
bool mtcs_preds_register_operand (MtcsPreds *self,rtx op, mtcs_mode mode)
{
  n_debug("mtcspreds.c mtcs_preds_register_operand 00 op:%p mode:%d\n",op,mode);
  if (GET_CODE (op) == SUBREG){
      rtx sub = SUBREG_REG (op);

      /* Before reload, we can allow (SUBREG (MEM...)) as a register operand
     because it is guaranteed to be reloaded into one.
     Just make sure the MEM is valid in itself.
     (Ideally, (SUBREG (MEM)...) should not exist after reload,
     but currently it does result from (SUBREG (REG)...) where the
     reg went on the stack.)  */
      if (!REG_P (sub) && (reload_completed || !MEM_P (sub)))
          return false;
  }else if (!REG_P (op))
    return false;
  n_debug("mtcspreds.c mtcs_preds_register_operand 11 op:%p mode:%d\n",op,mode);

  return mtcs_preds_general_operand (self,op, mode);
}


/* Return true if OP is a valid general operand for machine mode MODE.
   This is either a register reference, a memory reference,
   or a constant.  In the case of a memory reference, the address
   is checked for general validity for the target machine.

   Register and memory references must have mode MODE in order to be valid,
   but some constants have no machine mode and are valid for any mode.

   If MODE is VOIDmode, OP is checked for validity for whatever mode
   it has.

   The main use of this function is as a predicate in match_operand
   expressions in the machine description.  */
//原型 general_operand build/tm-preds.h recog.cc实现
bool mtcs_preds_general_operand (MtcsPreds *self,rtx op, mtcs_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg    *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsRecog   *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsConfig  *mtcsConfig=mtcs_target_get_config(mtcsTarget);
  MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *opts=mtcsOptions->global_options;

  enum rtx_code code = GET_CODE (op);
  //fprintf(stderr,"mtcs_preds_general_operand 00 mode:%d %d\n",mode, GET_MODE (op));
  if (mode == mtcsMode->modes.M_VOIDmode)
    mode = GET_MODE (op);
 // fprintf(stderr,"mtcs_preds_general_operand 11 mode:%d %d\n",mode, GET_MODE (op));

  /* Don't accept CONST_INT or anything similar
     if the caller wants something floating.  */
  if (GET_MODE (op) == mtcsMode->modes.M_VOIDmode && mode != mtcsMode->modes.M_VOIDmode
      && mtcs_mode_get_class/*GET_MODE_CLASS*/ (mtcsMode,mode) != MODE_INT
      && mtcs_mode_get_class/*GET_MODE_CLASS*/ (mtcsMode,mode) != MODE_PARTIAL_INT)
    return false;
 // fprintf(stderr,"mtcs_preds_general_operand 22 mode:%d %d\n",mode, GET_MODE (op));

  if (CONST_INT_P (op)  && mode != mtcsMode->modes.M_VOIDmode
      && mtcs_mode_trunc_int_for_mode (mtcsMode,INTVAL (op), mode) != INTVAL (op))
    return false;
 // fprintf(stderr,"mtcs_preds_general_operand 33 mode:%d %d\n",mode, GET_MODE (op));

  if (CONSTANT_P (op))
    return ((GET_MODE (op) == mtcsMode->modes.M_VOIDmode || GET_MODE (op) == mode
         || mode == mtcsMode->modes.M_VOIDmode)
        && (!opts->x_flag_pic || mtcs_recog_is_legitimate_pic_operand_p/*!LEGITIMATE_PIC_OPERAND_P*/(mtcsRecog,op))
        && mtcsTarget->legitimate_constant_p/*targetm.legitimate_constant_p*/ (mtcsTarget,mode == mtcsMode->modes.M_VOIDmode
                          ? GET_MODE (op): mode, op));
 // fprintf(stderr,"mtcs_preds_general_operand 44 mode:%d %d\n",mode, GET_MODE (op));

  /* Except for certain constants with VOIDmode, already checked for,
     OP's mode must match MODE if MODE specifies a mode.  */

  if (GET_MODE (op) != mode)
    return false;
 // fprintf(stderr,"mtcs_preds_general_operand 55 mode:%d %d\n",mode, GET_MODE (op));

  if (code == SUBREG){
 //    fprintf(stderr,"mtcs_preds_general_operand 66 mode:%d %d\n",mode, GET_MODE (op));

      rtx sub = SUBREG_REG (op);
      if(mtcs_config_ifdef(mtcsConfig,MTCS_INSN_SCHEDULING)){ /*!#ifdef INSN_SCHEDULING host INSN_SCHEDULING=1 nvptx=0*/
              /* On machines that have insn scheduling, we want all memory
             reference to be explicit, so outlaw paradoxical SUBREGs.
             However, we must allow them after reload so that they can
             get cleaned up by cleanup_subreg_operands.  */
              if (!reload_completed && MEM_P (sub)   && mtcs_rtl_paradoxical_subreg_p/*paradoxical_subreg_p*/ (mtcsRTL,op))
                  return false;
      }/*!#endif*/
      /* Avoid memories with nonzero SUBREG_BYTE, as offsetting the memory
         may result in incorrect reference.  We should simplify all valid
         subregs of MEM anyway.  But allow this after reload because we
         might be called from cleanup_subreg_operands.
       ??? This is a kludge.  */
      if (!reload_completed && maybe_ne (SUBREG_BYTE (op), 0) && MEM_P (sub))
          return false;

      if (REG_P (sub)
        && REGNO (sub) < mtcs_reg_get_first_pseudo_register/*  FIRST_PSEUDO_REGISTER*/(mtcsReg)
        && !mtcs_reg_can_change_mode/*!REG_CAN_CHANGE_MODE_P*/(mtcsReg,REGNO (sub), GET_MODE (sub), mode)
        && mtcs_mode_get_class/*GET_MODE_CLASS*/ (mtcsMode,GET_MODE (sub)) != MODE_COMPLEX_INT
        && mtcs_mode_get_class/*GET_MODE_CLASS */(mtcsMode,GET_MODE (sub)) != MODE_COMPLEX_FLOAT
        /* LRA can generate some invalid SUBREGS just for matched
         operand reload presentation.  LRA needs to treat them as
         valid.  */
        && ! LRA_SUBREG_P (op))
          return false;

      /* FLOAT_MODE subregs can't be paradoxical.  Combine will occasionally
     create such rtl, and we must reject it.  */
      if (mtcs_mode_is_float_p/*SCALAR_FLOAT_MODE_P*/ (mtcsMode,GET_MODE (op))
        /* LRA can use subreg to store a floating point value in an
         integer mode.  Although the floating point and the
         integer modes need the same number of hard registers, the
         size of floating point mode can be less than the integer
         mode.  */
        && ! lra_in_progress  && mtcs_rtl_paradoxical_subreg_p (mtcsRTL,op))
          return false;

      op = sub;
      code = GET_CODE (op);
  }

  if (code == REG){
    // fprintf(stderr,"mtcspreds.c mtcs_preds_general_operand 77 op:%p mode:%d\n",op,mode);

    return (REGNO (op) >= mtcs_reg_get_first_pseudo_register(mtcsReg)/*FIRST_PSEUDO_REGISTER*/
            || mtcs_reg_in_hard_reg_set_p/*in_hard_reg_set_p*/ (mtcsReg,
                    &mtcsReg->hardRegs.x_operand_reg_set/* operand_reg_set*/, GET_MODE (op), REGNO (op)));
  }
  if (code == MEM){
   //  fprintf(stderr,"mtcs_preds_general_operand 88 mode:%d %d MEM_VOLATILE_P (op):%d\n",mode, GET_MODE (op),MEM_VOLATILE_P (op));

      rtx y = XEXP (op, 0);

      if (! mtcsRecog->/*!volatile_ok*/volatile_ok && MEM_VOLATILE_P (op))
          return false;

      /* Use the mem's mode, since it will be reloaded thus.  LRA can
     generate move insn with invalid addresses which is made valid
     and efficiently calculated by LRA through further numerous
     transformations.  */
      int addrSpace=mtcs_rtl_get_mem_addr_space(mtcsRTL,op);//替换MEM_ADDR_SPACE (op))
      if (lra_in_progress || mtcs_recog_memory_address_addr_space_p/*memory_address_addr_space_p*/
              (mtcsRecog,GET_MODE (op), y,addrSpace,ERROR_MARK)){ //code_helper = ERROR_MARK
         //fprintf(stderr,"mtcs_preds_general_operand 99 mode:%d %d\n",mode, GET_MODE (op));
          return true;
      }

  }

  return false;
}


/* Return true if OP is a valid operand that stands for pushing a
   value of mode MODE onto the stack.

   The main use of this function is as a predicate in match_operand
   expressions in the machine description.  */
//原型 recog.cc build/tm-preds.h
bool mtcs_preds_push_operand (MtcsPreds *self,rtx op, mtcs_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

  if (!MEM_P (op))
    return false;

  if (mode != mtcsMode->modes.M_VOIDmode && GET_MODE (op) != mode)
    return false;

  poly_int64 rounded_size = mtcs_mode_get_size/*GET_MODE_SIZE*/ (mtcsMode,mode);

//#ifdef PUSH_ROUNDING host=1 nvptx=0
//  rounded_size = PUSH_ROUNDING (MACRO_INT (rounded_size));
//#endif

  op = XEXP (op, 0);

  if (known_eq (rounded_size, mtcs_mode_get_size/*GET_MODE_SIZE*/ (mtcsMode,mode))){
      if (GET_CODE (op) != mtcs_func_get_stack_push_code/*!STACK_PUSH_CODE*/(mtcsFunc))
          return false;
  }else{
      poly_int64 offset;
      if (GET_CODE (op) != PRE_MODIFY
      || GET_CODE (XEXP (op, 1)) != PLUS
      || XEXP (XEXP (op, 1), 0) != XEXP (op, 0)
      || !poly_int_rtx_p (XEXP (XEXP (op, 1), 1), &offset)
      || (mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc)
          ? maybe_ne (offset, -rounded_size)
          : maybe_ne (offset, rounded_size)))
          return false;
  }

  return XEXP (op, 0) == mtcs_rtl_get_stack_pointer_rtx(mtcsRTL);/*stack_pointer_rtx 见rtl.h*/;
}

/* Return true if OP is a valid memory address for a memory reference
   of mode MODE.

   The main use of this function is as a predicate in match_operand
   expressions in the machine description.  */
//原型 address_operand tm-preds.h recog.cc
bool mtcs_preds_address_operand (MtcsPreds *self,rtx op, machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  /* Wrong mode for an address expr.  */
  if (GET_MODE (op) != VOIDmode
      && ! mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,GET_MODE (op)))
    return false;
  return mtcs_recog_memory_address_p/*!memory_address_p*/(mtcsRecog,mode,op);
}

/* Return true for a register in Pmode; ignore the tested mode.  */
//原型 pmode_register_operand tm-preds.h recog.cc
bool mtcs_preds_pmode_register_operand (MtcsPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  return mtcs_preds_register_operand (self,op,mtcs_mode_get_Pmode/*!Pmode*/(mtcsMode));
}

/* Return true if OP should match a MATCH_SCRATCH, i.e., if it is a SCRATCH
   or a hard register.  */
//原型 scratch_operand tm-preds.h recog.cc
bool mtcs_preds_scratch_operand (MtcsPreds *self,rtx op, machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  if (GET_MODE (op) != mode && mode != VOIDmode)
    return false;

  return (GET_CODE (op) == SCRATCH
      || (REG_P (op)
          && (lra_in_progress
          || (REGNO (op) < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
              && mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,REGNO (op)) != NO_REGS))));
}

/* Return true if OP is a valid immediate operand for mode MODE.

   The main use of this function is as a predicate in match_operand
   expressions in the machine description.  */
//原型 immediate_operand tm-preds.h recog.cc
bool mtcs_preds_immediate_operand (MtcsPreds *self,rtx op, machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

  /* Don't accept CONST_INT or anything similar
     if the caller wants something floating.  */
  if (GET_MODE (op) == VOIDmode && mode != VOIDmode
      && mtcs_mode_get_class(mtcsMode,mode) != MODE_INT
      && mtcs_mode_get_class (mtcsMode,mode) != MODE_PARTIAL_INT)
    return false;

  if (CONST_INT_P (op)
      && mode != VOIDmode
      && mtcs_mode_trunc_int_for_mode (mtcsMode,INTVAL (op), mode) != INTVAL (op))
    return false;

  return (CONSTANT_P (op) && (GET_MODE (op) == mode || mode == VOIDmode
          || GET_MODE (op) == VOIDmode) && (! flag_pic || mtcs_recog_is_legitimate_pic_operand_p/*!LEGITIMATE_PIC_OPERAND_P*/(mtcsRecog,op))
          && mtcsTarget->legitimate_constant_p/*!targetm.legitimate_constant_p*/ (mtcsTarget,mode == VOIDmode? GET_MODE (op): mode, op));
}

/* Return true if OP is an operand that is a CONST_INT of mode MODE.  */
//原型 const_int_operand tm-preds.h recog.cc
bool mtcs_preds_const_int_operand (MtcsPreds *self,rtx op, machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  if (!CONST_INT_P (op))
    return false;

  if (mode != VOIDmode
      && mtcs_mode_trunc_int_for_mode (mtcsMode,INTVAL (op), mode) != INTVAL (op))
    return false;

  return true;
}

//原型 const_int_operand tm-preds.h recog.cc #if TARGET_SUPPORTS_WIDE_INT
bool mtcs_preds_const_scalar_int_operand (MtcsPreds *self,rtx op, machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  if (!CONST_SCALAR_INT_P (op))
    return false;

  if (CONST_INT_P (op))
    return mtcs_preds_const_int_operand (self,op, mode);

  if (mode != VOIDmode){
      scalar_int_mode int_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,mode);
      int prec = mtcs_mode_get_precision(mtcsMode,int_mode);
      int bitsize = mtcs_mode_get_bitsize(mtcsMode,int_mode);

      if (CONST_WIDE_INT_NUNITS (op) * HOST_BITS_PER_WIDE_INT > bitsize)
          return false;

      if (prec == bitsize)
          return true;
      else{
          /* Multiword partial int.  */
          HOST_WIDE_INT x = CONST_WIDE_INT_ELT (op, CONST_WIDE_INT_NUNITS (op) - 1);
          return (sext_hwi (x, prec & (HOST_BITS_PER_WIDE_INT - 1)) == x);
      }
  }
  return true;
}

//原型 const_double_operand  tm-preds.h recog.cc 。
//bool const_double_operand (rtx op, machine_mode mode)
bool mtcs_preds_const_double_operand (MtcsPreds *self,rtx op, machine_mode mode)
{
  return (GET_CODE (op) == CONST_DOUBLE)
      && (GET_MODE (op) == mode || mode == VOIDmode);
}

//原型 nonimmediate_operand tm-preds.h recog.cc
bool mtcs_preds_nonimmediate_operand (MtcsPreds *self,rtx op, machine_mode mode)
{
  return (mtcs_preds_general_operand (self,op, mode) && ! CONSTANT_P (op));
}

/* Return true if OP is a register reference or
   immediate value of mode MODE.  */
//原型 nonmemory_operand tm-preds.h recog.cc
bool mtcs_preds_nonmemory_operand (MtcsPreds *self,rtx op, machine_mode mode)
{
  if (CONSTANT_P (op))
    return mtcs_preds_immediate_operand (self,op, mode);
  return mtcs_preds_register_operand (self,op, mode);
}


//原型 pop_operand  tm-preds.h recog.cc
/*原型 STACK_POP_CODE
#ifndef STACK_POP_CODE
#if STACK_GROWS_DOWNWARD
#define STACK_POP_CODE POST_INC
#else
#define STACK_POP_CODE POST_DEC
#endif
#endif
*/
bool mtcs_preds_pop_operand (MtcsPreds *self,rtx op, machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);


  if (!MEM_P (op))
    return false;

  if (mode != VOIDmode && GET_MODE (op) != mode)
    return false;

  op = XEXP (op, 0);

  if (GET_CODE (op) !=  mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc)?POST_INC:POST_DEC/*!STACK_POP_CODE*/)
    return false;

  return XEXP (op, 0) == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL);
}
/* Return true if OP is a valid memory reference with mode MODE,
   including a valid address.

   The main use of this function is as a predicate in match_operand
   expressions in the machine description.  */
//原型 memory_operand tm-preds.h recog.cc
bool mtcs_preds_memory_operand (MtcsPreds *self,rtx op, machine_mode mode)
{
  rtx inner;

  if (! reload_completed)
    /* Note that no SUBREG is a memory operand before end of reload pass,
       because (SUBREG (MEM...)) forces reloading into a register.  */
    return MEM_P (op) && mtcs_preds_general_operand (self,op, mode);

  if (mode != VOIDmode && GET_MODE (op) != mode)
    return false;

  inner = op;
  if (GET_CODE (inner) == SUBREG)
    inner = SUBREG_REG (inner);

  return (MEM_P (inner) && mtcs_preds_general_operand (self,op, mode));
}

/* Return true if OP is a valid indirect memory reference with mode MODE;
   that is, a memory reference whose address is a general_operand.  */
//原型 indirect_operand tm-preds.h recog.cc
bool mtcs_preds_indirect_operand (MtcsPreds *self,rtx op, machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  /* Before reload, a SUBREG isn't in memory (see memory_operand, above).  */
  if (! reload_completed && GET_CODE (op) == SUBREG && MEM_P (SUBREG_REG (op))){
      if (mode != VOIDmode && GET_MODE (op) != mode)
          return false;

      /* The only way that we can have a general_operand as the resulting
     address is if OFFSET is zero and the address already is an operand
     or if the address is (plus Y (const_int -OFFSET)) and Y is an
     operand.  */
      poly_int64 offset;
      rtx addr = strip_offset (XEXP (SUBREG_REG (op), 0), &offset);
      return (known_eq (offset + SUBREG_BYTE (op), 0)
          && mtcs_preds_general_operand (self,addr, mtcs_mode_get_Pmode(mtcsMode)));
   }
  return (MEM_P (op) && mtcs_preds_memory_operand (self,op, mode) &&
          mtcs_preds_general_operand (self,XEXP (op, 0), mtcs_mode_get_Pmode(mtcsMode)));
}

//原型 bool aligned_register_operand (rtx op, machine_mode mode) tm-preds.h insn-preds.cc
bool mtcs_preds_aligned_register_operand (MtcsPreds *self,rtx op, machine_mode mode ATTRIBUTE_UNUSED)
{
   return self->aligned_register_operand(self,op,mode);
}

//原型 get_register_filter_id tm-preds.h
int mtcs_preds_get_register_filter_id (MtcsPreds *self,int constraint_num/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{
    return self->get_register_filter_id(self,constraint_num);
}

//原型 get_constraint_type tm-preds.h
int mtcs_preds_get_constraint_type (MtcsPreds *self,int constraint_num/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{
    return self->get_constraint_type(self,constraint_num);
}

//原型 test_register_filters tm-preds.h ira-color.cc引用
bool mtcs_preds_test_register_filters(MtcsPreds *self,unsigned int mask, unsigned int regno)
{
   return self->test_register_filters(self,mask,regno);
}

//原型 lookup_constraint lookup_constraint tm-preds.h
int mtcs_preds_lookup_constraint(MtcsPreds *self,unsigned char *p)
{
    return self->lookup_constraint(self,p);
}

/* Return true if X satisfies constraint C.  */
//原型  constraint_satisfied_p tm-preds.h
bool mtcs_preds_constraint_satisfied_p (MtcsPreds *self,rtx x, int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{
    return self->constraint_satisfied_p (self,x,constraintNum);
}

//原型  insn_extra_register_constraint tm-preds.h
bool mtcs_preds_insn_extra_register_constraint(MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{
    return self->insn_extra_register_constraint(self,constraintNum);
}

//原型  insn_extra_memory_constraint tm-preds.h
bool mtcs_preds_insn_extra_memory_constraint(MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{
    return self->insn_extra_memory_constraint(self,constraintNum);
}

//原型  insn_extra_special_memory_constraint tm-preds.h
bool mtcs_preds_insn_extra_special_memory_constraint(MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{
    return self->insn_extra_special_memory_constraint(self,constraintNum);
}

//原型  insn_extra_relaxed_memory_constraint tm-preds.h
bool mtcs_preds_insn_extra_relaxed_memory_constraint (MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{
    return self->insn_extra_relaxed_memory_constraint (self,constraintNum);

}

//原型  insn_extra_address_constraint tm-preds.h
bool mtcs_preds_insn_extra_address_constraint (MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{
    return self->insn_extra_address_constraint(self,constraintNum);
}


//原型  insn_extra_constraint_allows_reg_mem tm-preds.h
void mtcs_preds_insn_extra_constraint_allows_reg_mem(MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/,
                   bool *allows_reg, bool *allows_mem)
{
    self->insn_extra_constraint_allows_reg_mem(self,constraintNum,allows_reg,allows_mem);
}


//原型 #define CONSTRAINT_LEN(c_,s_) insn_constraint_len (c_,s_) tm-preds.h
int mtcs_preds_insn_constraint_len(MtcsPreds *self,char fc, const char *str ATTRIBUTE_UNUSED)
{
    return self->insn_constraint_len(self,fc,str);
}

//原型 reg_class_for_constraint tm-preds.h
mtcs_reg_class mtcs_preds_reg_class_for_constraint (MtcsPreds *self,int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{
    return self->reg_class_for_constraint(self,constraintNum);
}


//原型 insn_const_int_ok_for_constraint (HOST_WIDE_INT, enum constraint_num); tm-preds.h
bool mtcs_preds_insn_const_int_ok_for_constraint(MtcsPreds *self,HOST_WIDE_INT ival,
        int constraintNum/*!enum constraint_num 每个平台都不一样，所以改为int*/)
{
    return self->insn_const_int_ok_for_constraint(self,ival,constraintNum);
}

/* Return true if this is an ordered comparison operator (not including
   ORDERED and UNORDERED).  */
//原型  ordered_comparison_operator tm-preds.h recog.cc
bool mtcs_preds_ordered_comparison_operator (MtcsPreds *self,rtx op, machine_mode mode)
{
  if (mode != VOIDmode && GET_MODE (op) != mode)
    return false;
  switch (GET_CODE (op))
    {
    case EQ:
    case NE:
    case LT:
    case LTU:
    case LE:
    case LEU:
    case GT:
    case GTU:
    case GE:
    case GEU:
      return true;
    default:
      return false;
    }
}

/* Return true if this is a comparison operator.  This allows the use of
   MATCH_OPERATOR to recognize all the branch insns.  */
//原型  comparison_operator tm-preds.h recog.cc
bool mtcs_preds_comparison_operator (MtcsPreds *self,rtx op, machine_mode mode)
{
  return ((mode == VOIDmode || GET_MODE (op) == mode)
      && COMPARISON_P (op));
}

//原型 get_register_filter tm-preds.h
const HardRegSet *mtcs_preds_get_register_filter (MtcsPreds *self, int constraint_num)
{
  return self->get_register_filter(self,constraint_num);
}

//在ptx-insn-output.c中定义的insn_data 中，关于谓词部分有带两个参数和三个参数的函数之分
//例如:mtcs_preds_nonimmediate_operand 带三个参数, ptx_nvptx_register_operand只带两个，
//把带三个参数的定义为公共谓词函数，两个的是平台函数
nboolean mtcs_preds_is_common(MtcsPreds *self,void *func)
{
   static void *data[]={
         mtcs_preds_general_operand,
         mtcs_preds_register_operand,
         mtcs_preds_push_operand,
         mtcs_preds_address_operand,
         mtcs_preds_pmode_register_operand,
         mtcs_preds_scratch_operand,
         mtcs_preds_immediate_operand,
         mtcs_preds_const_int_operand,
         mtcs_preds_const_scalar_int_operand,
         mtcs_preds_const_double_operand,
         mtcs_preds_pop_operand,
         mtcs_preds_nonimmediate_operand,
         mtcs_preds_nonmemory_operand,
         mtcs_preds_memory_operand,
         mtcs_preds_indirect_operand,
         mtcs_preds_aligned_register_operand,
         mtcs_preds_ordered_comparison_operator,
         mtcs_preds_comparison_operator,
         NULL
   };
   int i=0;
   while(data[i]){
      if(data[i]==func)
         return TRUE;
      i++;
   }
   return FALSE;
}



