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
 * base on rtlanal.cc
 */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "rtlanal.h"
#include "tree.h"
#include "predict.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "emit-rtl.h"  /* FIXME: Can go away once crtl is moved to rtl.h.  */
#include "recog.h"
#include "addresses.h"
#include "rtl-iter.h"
#include "hard-reg-set.h"
#include "function-abi.h"

#include "aet/aetprinttree.h"
#include "mtcsrtlanal.h"
#include "mtcstarget.h"
#include "mtcsprintrtl.h"

static unsigned HOST_WIDE_INT cached_nonzero_bits (MtcsRtlanal *self,const_rtx x, scalar_int_mode mode, const_rtx known_x,
           machine_mode known_mode,
           unsigned HOST_WIDE_INT known_ret);

static void mtcsRtlanalInit(MtcsRtlanal *self)
{

}


/* Compute an approximation for the offset between the register
   FROM and TO for the current function, as it was at the start
   of the routine.  */
//原型 get_initial_register_offset
static poly_int64 get_initial_register_offset (MtcsRtlanal *self,int from, int to)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   /*!
   static const struct elim_table_t
   {
   const int from;
   const int to;
   } table[] = ELIMINABLE_REGS;
   */
   static  struct elim_table_t
   {
      int from;
      int to;
   } table[20];//20足够大了

   poly_int64 offset1, offset2;
   unsigned int i, j;
   //给table赋值
   for(i=0;i<mtcsReg->elimiableRegsCount;i++){
      table[i].from=mtcsReg->eliminableRegs[i].from;
      table[i].to=mtcsReg->eliminableRegs[i].to;
   }

   if (to == from)
      return 0;

   /* It is not safe to call INITIAL_ELIMINATION_OFFSET before the epilogue
   is completed, but we need to give at least an estimate for the stack
   pointer based on the frame size.  */
   if (!epilogue_completed){
      offset1 = mtcsRtlData/*!crtl*/->outgoing_args_size + get_frame_size ();
      if(mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc)==0)
         //#if !STACK_GROWS_DOWNWARD
         offset1 = - offset1;
         //#endif
      if (to == mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg))
         return offset1;
      else if (from == mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg))
         return - offset1;
      else
         return 0;
   }

   for (i = 0; i < mtcsReg->elimiableRegsCount/*!ARRAY_SIZE (table)*/; i++)
      if (table[i].from == from){
         if (table[i].to == to){
            offset1=mtcs_func_initial_elimination_offset(mtcsFunc,table[i].from, table[i].to);
            /*!INITIAL_ELIMINATION_OFFSET(mtcsFunc,table[i].from, table[i].to, offset1);*/
            return offset1;
         }
         for (j = 0; j < ARRAY_SIZE (table); j++){
            if (table[j].to == to  && table[j].from == table[i].to){
               offset1=mtcs_func_initial_elimination_offset(mtcsFunc,table[i].from, table[i].to);
               /*!INITIAL_ELIMINATION_OFFSET (table[i].from, table[i].to,offset1);*/
               offset2=mtcs_func_initial_elimination_offset(mtcsFunc,table[j].from, table[j].to);
               /*! INITIAL_ELIMINATION_OFFSET (table[j].from, table[j].to,offset2);*/
               return offset1 + offset2;
            }
            if (table[j].from == to  && table[j].to == table[i].to){
               offset1=mtcs_func_initial_elimination_offset(mtcsFunc,table[i].from, table[i].to);
               /*!INITIAL_ELIMINATION_OFFSET (table[i].from, table[i].to,offset1);*/
               offset2=mtcs_func_initial_elimination_offset(mtcsFunc,table[j].from, table[j].to);
               /*!INITIAL_ELIMINATION_OFFSET (table[j].from, table[j].to,offset2);*/
               return offset1 - offset2;
            }
         }
      }else if (table[i].to == from){
         if (table[i].from == to){
            offset1=mtcs_func_initial_elimination_offset(mtcsFunc,table[i].from, table[i].to);
            /*!INITIAL_ELIMINATION_OFFSET (table[i].from, table[i].to,offset1);*/
            return - offset1;
         }
         for (j = 0; j < ARRAY_SIZE (table); j++){
            if (table[j].to == to  && table[j].from == table[i].from){
               offset1=mtcs_func_initial_elimination_offset(mtcsFunc,table[i].from, table[i].to);
               /*!INITIAL_ELIMINATION_OFFSET (table[i].from, table[i].to, offset1);*/
               offset2=mtcs_func_initial_elimination_offset(mtcsFunc,table[j].from, table[j].to);
               /*!INITIAL_ELIMINATION_OFFSET (table[j].from, table[j].to, offset2);*/
               return - offset1 + offset2;
            }
            if (table[j].from == to && table[j].to == table[i].from){
               offset1=mtcs_func_initial_elimination_offset(mtcsFunc,table[i].from, table[i].to);
               /*!INITIAL_ELIMINATION_OFFSET (table[i].from, table[i].to, offset1);*/
               offset2=mtcs_func_initial_elimination_offset(mtcsFunc,table[j].from, table[j].to);
               /*!INITIAL_ELIMINATION_OFFSET (table[j].from, table[j].to, offset2);*/
               return - offset1 - offset2;
            }
         }
      }

   /* If the requested register combination was not found,
   try a different more simple combination.  */
   if (from == mtcs_reg_get_arg_pointer_regnum/*!ARG_POINTER_REGNUM*/(mtcsReg))
      return get_initial_register_offset(self,mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg), to);
   else if (to == mtcs_reg_get_arg_pointer_regnum/*!ARG_POINTER_REGNUM*/(mtcsReg))
      return get_initial_register_offset(self,from, mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg));
   else if (from == mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg))
      return get_initial_register_offset(self,mtcs_reg_get_frame_pointer_regnum/*!FRAME_POINTER_REGNUM*/(mtcsReg), to);
   else if (to == mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg))
      return get_initial_register_offset(self,from, mtcs_reg_get_frame_pointer_regnum/*!FRAME_POINTER_REGNUM*/(mtcsReg));
   else
      return 0;
}

/* Return true if the use of X+OFFSET as an address in a MEM with SIZE
   bytes can cause a trap.  MODE is the mode of the MEM (not that of X) and
   UNALIGNED_MEMS controls whether true is returned for unaligned memory
   references on strict alignment machines.  */
//原型 rtx_addr_can_trap_p_1 rtlanal.cc
static bool rtx_addr_can_trap_p_1 (MtcsRtlanal *self,const_rtx x, poly_int64 offset, poly_int64 size,
             machine_mode mode, bool unaligned_mems)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);

   enum rtx_code code = GET_CODE (x);
   gcc_checking_assert (mode == mtcsMode->modes.M_BLKmode || mode == VOIDmode || known_size_p (size));
   poly_int64 const_x1;

   /* The offset must be a multiple of the mode size if we are considering
   unaligned memory references on strict alignment machines.  */
   if (mtcs_align_get_strict_alignment/*!STRICT_ALIGNMENT*/(mtcsAlign)
   && unaligned_mems  && mode != mtcsMode->modes.M_BLKmode   && mode != VOIDmode){
      poly_int64 actual_offset = offset;

#ifdef SPARC_STACK_BOUNDARY_HACK //host=0 nvptx=0
      /* ??? The SPARC port may claim a STACK_BOUNDARY higher than
      the real alignment of %sp.  However, when it does this, the
      alignment of %sp+STACK_POINTER_OFFSET is STACK_BOUNDARY.  */
      if (SPARC_STACK_BOUNDARY_HACK
      && (x == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)
            || x == mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL)))
      actual_offset -= mtcs_func_get_stack_pointer_offset/*!STACK_POINTER_OFFSET*/(mtcsFunc);
#endif

      if (!multiple_p (actual_offset, mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode)))
         return true;
   }

   switch (code){
      case SYMBOL_REF:
         if (SYMBOL_REF_WEAK (x))
            return true;
         if (!CONSTANT_POOL_ADDRESS_P (x) && !SYMBOL_REF_FUNCTION_P (x)){
         tree decl;
         poly_int64 decl_size;

         if (maybe_lt (offset, 0))
            return true;
         if (!known_size_p (size))
            return maybe_ne (offset, 0);

         /* If the size of the access or of the symbol is unknown,
         assume the worst.  */
         decl = SYMBOL_REF_DECL (x);

         /* Else check that the access is in bounds.  TODO: restructure
         expr_size/tree_expr_size/int_expr_size and just use the latter.  */
         if (!decl)
            decl_size = -1;
         else if (DECL_P (decl) && DECL_SIZE_UNIT (decl)){
            if (!poly_int_tree_p (DECL_SIZE_UNIT (decl), &decl_size))
               decl_size = -1;
            }else if (TREE_CODE (decl) == STRING_CST)
               decl_size = TREE_STRING_LENGTH (decl);
            else if (TYPE_SIZE_UNIT (TREE_TYPE (decl)))
               decl_size = int_size_in_bytes (TREE_TYPE (decl));
            else
               decl_size = -1;

            return (!known_size_p (decl_size) || known_eq (decl_size, 0)
                  ? maybe_ne (offset, 0) : !known_subrange_p (offset, size, 0, decl_size));
         }

         return false;

      case LABEL_REF:
         return false;

      case REG:
         /* Stack references are assumed not to trap, but we need to deal with
         nonsensical offsets.  */
         if (x == mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL)
         || x == mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL)
         || x == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)
         /* The arg pointer varies if it is not a fixed register.  */
         || (x == mtcs_rtl_get_arg_pointer_rtx/*!arg_pointer_rtx*/(mtcsRTL)
         && mtcsReg->hardRegs.x_fixed_regs/*!fixed_regs*/[mtcs_reg_get_arg_pointer_regnum/*!ARG_POINTER_REGNUM*/(mtcsReg)])){
            poly_int64 red_zone_size = 0;
            if(mtcs_config_ifdef(mtcsConfig,MTCS_RED_ZONE_SIZE))
               red_zone_size = mtcs_config_get_value(mtcsConfig,MTCS_RED_ZONE_SIZE);
            /*!
            #ifdef RED_ZONE_SIZE
            poly_int64 red_zone_size = RED_ZONE_SIZE;
            #else
            poly_int64 red_zone_size = 0;
            #endif
            */
            poly_int64 stack_boundary = mtcs_func_get_preferred_stack_boundary/*!PREFERRED_STACK_BOUNDARY*/(mtcsFunc) / BITS_PER_UNIT;
            poly_int64 low_bound, high_bound;

            if (!known_size_p (size))
               return true;

            if (x == mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL)){
               if (mtcs_func_get_frame_grows_downward/*!FRAME_GROWS_DOWNWARD*/(mtcsFunc)){
                  high_bound =mtcsTarget/*!targetm.starting_frame_offset*/->starting_frame_offset(mtcsTarget);
                  low_bound  = high_bound - mtcs_func_get_frame_size/*!get_frame_size*/(mtcsFunc);
               }else{
                  low_bound  =mtcsTarget/*!targetm.starting_frame_offset*/->starting_frame_offset(mtcsTarget);
                  high_bound = low_bound + mtcs_func_get_frame_size/*!get_frame_size*/(mtcsFunc);
               }
            }else if (x == mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL)){
               poly_int64 sp_offset  = get_initial_register_offset(self, mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg),
               mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg));
               poly_int64 ap_offset = get_initial_register_offset(self,mtcs_reg_get_arg_pointer_regnum/*!ARG_POINTER_REGNUM*/(mtcsReg),
               mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg));

               if(mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc)!=0){
                  low_bound  = sp_offset - red_zone_size - stack_boundary;
                  high_bound = ap_offset + mtcs_func_get_first_parm_offset/*!FIRST_PARM_OFFSET*/(mtcsFunc,current_function_decl);
                  if(mtcs_func_get_args_grows_downward/*!ARGS_GROW_DOWNWARD*/(mtcsFunc)==0){
                     high_bound+=mtcsRtlData/*!crtl*/->args.size;
                  }
                  high_bound+=stack_boundary;
               }else{
                  high_bound = sp_offset + red_zone_size + stack_boundary;
                  low_bound  = ap_offset  + mtcs_func_get_first_parm_offset/*!FIRST_PARM_OFFSET*/(mtcsFunc,current_function_decl);
                  if(mtcs_func_get_args_grows_downward/*!ARGS_GROW_DOWNWARD*/(mtcsFunc)==0){
                    high_bound-=mtcsRtlData/*!crtl*/->args.size;
                  }
                  high_bound-=stack_boundary;
               }
               /*
            #if STACK_GROWS_DOWNWARD
               low_bound  = sp_offset - red_zone_size - stack_boundary;
               high_bound = ap_offset
               + FIRST_PARM_OFFSET (current_function_decl)
            #if !ARGS_GROW_DOWNWARD
               + crtl->args.size
            #endif
               + stack_boundary;
            #else
               high_bound = sp_offset + red_zone_size + stack_boundary;
               low_bound  = ap_offset
               + FIRST_PARM_OFFSET (current_function_decl)
            #if ARGS_GROW_DOWNWARD
               - crtl->args.size
            #endif
               - stack_boundary;
            #endif*/
            }else if (x == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)){
               poly_int64 ap_offset  = get_initial_register_offset(self,mtcs_reg_get_arg_pointer_regnum/*!ARG_POINTER_REGNUM*/(mtcsReg),
               mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg));
               if(mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc)!=0){
                  low_bound  = - red_zone_size - stack_boundary;
                  high_bound = ap_offset+mtcs_func_get_first_parm_offset/*!FIRST_PARM_OFFSET*/(mtcsFunc,current_function_decl);
                  if(mtcs_func_get_args_grows_downward/*!ARGS_GROW_DOWNWARD*/(mtcsFunc)==0){
                  high_bound+=mtcsRtlData/*!crtl*/->args.size;
                  }
                  high_bound+=stack_boundary;
               }else{
                  high_bound = red_zone_size + stack_boundary;
                  low_bound  = ap_offset + mtcs_func_get_first_parm_offset/*!FIRST_PARM_OFFSET*/(mtcsFunc,current_function_decl);
                  if(mtcs_func_get_args_grows_downward/*!ARGS_GROW_DOWNWARD*/(mtcsFunc)==0){
                     high_bound-=mtcsRtlData/*!crtl*/->args.size;
                  }
                  high_bound-=stack_boundary;
               }
               /*
               #if STACK_GROWS_DOWNWARD
               low_bound  = - red_zone_size - stack_boundary;
               high_bound = ap_offset
               + FIRST_PARM_OFFSET (current_function_decl)
               #if !ARGS_GROW_DOWNWARD
               + crtl->args.size
               #endif
               + stack_boundary;
               #else
               high_bound = red_zone_size + stack_boundary;
               low_bound  = ap_offset
               + FIRST_PARM_OFFSET (current_function_decl)
               #if ARGS_GROW_DOWNWARD
               - crtl->args.size
               #endif
               - stack_boundary;
               #endif*/
            }else{
               /* We assume that accesses are safe to at least the
               next stack boundary.
               Examples are varargs and __builtin_return_address.  */
               if(mtcs_func_get_args_grows_downward/*!ARGS_GROW_DOWNWARD*/(mtcsFunc)!=0){
                  high_bound = mtcs_func_get_first_parm_offset/*!FIRST_PARM_OFFSET*/(mtcsFunc,current_function_decl)
                  + stack_boundary;
                  low_bound  = mtcs_func_get_first_parm_offset/*!FIRST_PARM_OFFSET*/(mtcsFunc,current_function_decl)
                  - mtcsRtlData/*!crtl*/->args.size - stack_boundary;
               }else{
                  high_bound = mtcs_func_get_first_parm_offset/*!FIRST_PARM_OFFSET*/(mtcsFunc,current_function_decl)
                        - stack_boundary;
                  low_bound  = mtcs_func_get_first_parm_offset/*!FIRST_PARM_OFFSET*/(mtcsFunc,current_function_decl)
                        + mtcsRtlData/*!crtl*/->args.size + stack_boundary;
               }
               /*
               #if ARGS_GROW_DOWNWARD
               high_bound = FIRST_PARM_OFFSET (current_function_decl)
               + stack_boundary;
               low_bound  = FIRST_PARM_OFFSET (current_function_decl)
               - crtl->args.size - stack_boundary;
               #else
               low_bound  = FIRST_PARM_OFFSET (current_function_decl)
               - stack_boundary;
               high_bound = FIRST_PARM_OFFSET (current_function_decl)
               + crtl->args.size + stack_boundary;
               #endif
               */
            }

            if (known_ge (offset, low_bound) && known_le (offset, high_bound - size))
               return false;
            return true;
         }
         /* All of the virtual frame registers are stack references.  */
         if (mtcs_reg_virtual_register_p/*!VIRTUAL_REGISTER_P*/(mtcsReg,x))
            return false;
         return true;

      case CONST:
         return rtx_addr_can_trap_p_1(self,XEXP (x, 0), offset, size,mode, unaligned_mems);

      case PLUS:
         /* An address is assumed not to trap if:
         - it is the pic register plus a const unspec without offset.  */
         if (XEXP (x, 0) == mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(mtcsRTL)
         && GET_CODE (XEXP (x, 1)) == CONST
         && GET_CODE (XEXP (XEXP (x, 1), 0)) == UNSPEC
         && known_eq (offset, 0))
            return false;

         /* - or it is an address that can't trap plus a constant integer.  */
         if (poly_int_rtx_p (XEXP (x, 1), &const_x1)
         && !rtx_addr_can_trap_p_1(self,XEXP (x, 0), offset + const_x1,size, mode, unaligned_mems))
            return false;

         return true;

      case LO_SUM:
      case PRE_MODIFY:
         return rtx_addr_can_trap_p_1(self,XEXP (x, 1), offset, size,mode, unaligned_mems);

      case PRE_DEC:
      case PRE_INC:
      case POST_DEC:
      case POST_INC:
      case POST_MODIFY:
         return rtx_addr_can_trap_p_1(self,XEXP (x, 0), offset, size, mode, unaligned_mems);

      default:
         break;
   }

   /* If it isn't one of the case above, it can cause a trap.  */
   return true;
}

/* Return true if, for all OP of mode OP_MODE:

     (vec_select:RESULT_MODE OP SEL)

   is equivalent to the highpart RESULT_MODE of OP.  */
//原型 vec_series_highpart_p rtlanal.h rtlanal.cc
bool mtcs_rtlanal_vec_series_highpart_p(MtcsRtlanal *self,machine_mode result_mode, machine_mode op_mode, rtx sel)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg = mtcs_target_get_reg(mtcsTarget);

  int nunits;
  if (mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,op_mode).is_constant (&nunits)
          && mtcsTarget->can_change_mode_class/*!targetm.can_change_mode_class*/ (mtcsTarget,
                op_mode, result_mode, mtcs_reg_get_all_regs/*!ALL_REGS*/(mtcsReg))){
      int offset = BYTES_BIG_ENDIAN ? 0 : nunits - XVECLEN (sel, 0);
      return rtvec_series_p (XVEC (sel, 0), offset);
  }
  return false;
}

//原型 vec_series_lowpart_p rtlanal.h rtlanal.cc
bool mtcs_rtlanal_vec_series_lowpart_p (MtcsRtlanal *self,machine_mode result_mode, machine_mode op_mode, rtx sel)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsReg *mtcsReg = mtcs_target_get_reg(mtcsTarget);

    int nunits;
    if (mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,op_mode).is_constant (&nunits)
          && mtcsTarget->can_change_mode_class/*!targetm.can_change_mode_class*/ (mtcsTarget,
                op_mode, result_mode, mtcs_reg_get_all_regs/*!ALL_REGS*/(mtcsReg))){
          int offset = BYTES_BIG_ENDIAN ? nunits - XVECLEN (sel, 0) : 0;
          return rtvec_series_p (XVEC (sel, 0), offset);
    }
    return false;
}

/* Return a value indicating whether OP, an operand of a commutative
   operation, is preferred as the first or second operand.  The more
   positive the value, the stronger the preference for being the first
   operand.  */

//原型 commutative_operand_precedence rtl.h rtlanal.cc
int mtcs_rtlanal_commutative_operand_precedence (MtcsRtlanal *self,rtx op)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

   enum rtx_code code = GET_CODE (op);

   /* Constants always become the second operand.  Prefer "nice" constants.  */
   if (code == CONST_INT)
      return -10;
   if (code == CONST_WIDE_INT)
      return -9;
   if (code == CONST_POLY_INT)
      return -8;
   if (code == CONST_DOUBLE)
      return -8;
   if (code == CONST_FIXED)
      return -8;
   op = mtcs_simplify_rtx_avoid_constant_pool_reference (mtcsSimplifyRtx,op);
   code = GET_CODE (op);

   switch (GET_RTX_CLASS (code)){
      case RTX_CONST_OBJ:
         if (code == CONST_INT)
            return -7;
         if (code == CONST_WIDE_INT)
            return -6;
         if (code == CONST_POLY_INT)
            return -5;
         if (code == CONST_DOUBLE)
         return -5;
         if (code == CONST_FIXED)
            return -5;
         return -4;

      case RTX_EXTRA:
         /* SUBREGs of objects should come second.  */
         if (code == SUBREG && OBJECT_P (SUBREG_REG (op)))
            return -3;
         return 0;

      case RTX_OBJ:
         /* Complex expressions should be the first, so decrease priority
         of objects.  Prefer pointer objects over non pointer objects.  */
         if ((REG_P (op) && REG_POINTER (op)) || (MEM_P (op) && MEM_POINTER (op)))
            return -1;
         return -2;

      case RTX_COMM_ARITH:
         /* Prefer operands that are themselves commutative to be first.
         This helps to make things linear.  In particular,
         (and (and (reg) (reg)) (not (reg))) is canonical.  */
         return 4;

      case RTX_BIN_ARITH:
         /* If only one operand is a binary expression, it will be the first
         operand.  In particular,  (plus (minus (reg) (reg)) (neg (reg)))
         is canonical, although it will usually be further simplified.  */
         return 2;

      case RTX_UNARY:
         /* Then prefer NEG and NOT.  */
         if (code == NEG || code == NOT)
            return 1;
         /* FALLTHRU */

      default:
         return 0;
   }
}

/* Calculate the cost of a single instruction.  A return value of zero
   indicates an instruction pattern without a known cost.  */
//原型 insn_cost rtl.h rtlanal.cc
int mtcs_rtlanal_insn_cost (MtcsRtlanal *self,rtx_insn *insn, bool speed)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  if (mtcsTarget->/*!targetm.*/insn_cost)
    return mtcsTarget->/*!targetm.*/insn_cost (mtcsTarget,insn, speed);

  return mtcs_rtlanal_pattern_cost/*!pattern_cost*/(self,PATTERN (insn), speed);
}

/* Return true iff it is necessary to swap operands of commutative operation
   in order to canonicalize expression.  */
//原型 swap_commutative_operands_p rtl.h rtlanal.cc
bool mtcs_rtlanal_swap_commutative_operands_p (MtcsRtlanal *self,rtx x, rtx y)
{
  return ( mtcs_rtlanal_commutative_operand_precedence (self,x)
      <  mtcs_rtlanal_commutative_operand_precedence (self,y));
}

/* Add a REG_ARGS_SIZE note to INSN with value VALUE.  */
//原型 add_args_size_note rtl.h rtlanal.cc
void mtcs_rtlanal_add_args_size_note (MtcsRtlanal *self,rtx_insn *insn, poly_int64 value)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  gcc_checking_assert (!find_reg_note (insn, REG_ARGS_SIZE, NULL_RTX));
  add_reg_note (insn, REG_ARGS_SIZE, mtcs_rtl_gen_int_mode (mtcsRTL,value, mtcs_mode_get_Pmode(mtcsMode)));
}

/* Return an estimate of the cost of computing rtx X.
   One use is in cse, to decide which expression to keep in the hash table.
   Another is in rtl generation, to pick the cheapest way to multiply.
   Other uses like the latter are expected in the future.

   X appears as operand OPNO in an expression with code OUTER_CODE.
   SPEED specifies whether costs optimized for speed or size should
   be returned.  */
//原型 rtx_cost rtl.h rtlanal.cc
int mtcs_rtlanal_rtx_cost (MtcsRtlanal *self,rtx x, machine_mode mode, enum rtx_code outer_code, int opno, bool speed)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  int i, j;
  enum rtx_code code;
  const char *fmt;
  int total;
  int factor;
  unsigned mode_size;
  //n_debug("mtcsrtlanal.c mtcs_rtlanal_rtx_cost 00 x:%p\n",x);
  if (x == 0)
    return 0;

  if (GET_CODE (x) == SET)
    /* A SET doesn't have a mode, so let's look at the SET_DEST to get
       the mode for the factor.  */
    mode = GET_MODE (SET_DEST (x));
  else if (GET_MODE (x) != VOIDmode)
    mode = GET_MODE (x);

  mode_size = estimated_poly_value (mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,mode));
  /* A size N times larger than UNITS_PER_WORD likely needs N times as
     many insns, taking N times as long.  */
  factor = mode_size > UNITS_PER_WORD ? mode_size / UNITS_PER_WORD : 1;
  /* Compute the default costs of certain things.
     Note that targetm.rtx_costs can override the defaults.  */

  code = GET_CODE (x);
  //n_debug("mtcsrtlanal.c mtcs_rtlanal_rtx_cost 11 x:%p GET_CODE (x) == SET:%d mode:%d mode_size:%d factor:%d code:%d\n",
          //x,(GET_CODE (x) == SET),mode,mode_size,factor,code);
  switch (code){
    case MULT:
    case FMA:
    case SS_MULT:
    case US_MULT:
    case SMUL_HIGHPART:
    case UMUL_HIGHPART:
      /* Multiplication has time-complexity O(N*N), where N is the
     number of units (translated from digits) when using
     schoolbook long multiplication.  */
      total = factor * factor * COSTS_N_INSNS (5);
      break;
    case DIV:
    case UDIV:
    case MOD:
    case UMOD:
    case SS_DIV:
    case US_DIV:
      /* Similarly, complexity for schoolbook long division.  */
      total = factor * factor * COSTS_N_INSNS (7);
      break;
    case USE:
      /* Used in combine.cc as a marker.  */
      total = 0;
      break;
    default:
      total = factor * COSTS_N_INSNS (1);
  }
 // n_debug("mtcsrtlanal.c mtcs_rtlanal_rtx_cost 22 code:%d total:%d factor:%d COSTS_N_INSNS(1):%d rtxcode:%d %d %d\n",
        //  code,total,factor,COSTS_N_INSNS (1),REG,SUBREG,TRUNCATE);
  switch (code){
    case REG:
      return 0;

    case SUBREG:
      total = 0;
      /* If we can't tie these modes, make this expensive.  The larger
     the mode, the more expensive it is.  */
      if (!mtcsTarget->/*!targetm.modes_tieable_p*/modes_tieable_p(mtcsTarget,mode, GET_MODE (SUBREG_REG (x))))
          return COSTS_N_INSNS (2 + factor);
      break;

    case TRUNCATE:
      if (mtcsTarget->/*!targetm.modes_tieable_p*/modes_tieable_p(mtcsTarget,mode, GET_MODE (XEXP (x, 0)))){
          total = 0;
          break;
      }
      /* FALLTHRU */
    default:
      if (mtcsTarget->rtx_costs/*!targetm.rtx_costs*/(mtcsTarget,x, mode, outer_code, opno, &total, speed))
          return total;
      break;
 }

  /* Sum the costs of the sub-rtx's, plus cost of this operation,
     which is already in total.  */

  fmt = GET_RTX_FORMAT (code);
  for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
    if (fmt[i] == 'e')
      total += mtcs_rtlanal_rtx_cost (self,XEXP (x, i), mode, code, i, speed);
    else if (fmt[i] == 'E')
      for (j = 0; j < XVECLEN (x, i); j++)
          total += mtcs_rtlanal_rtx_cost (self,XVECEXP (x, i, j), mode, code, i, speed);

  return total;
}

/* Return the cost of moving X into a register, relative to the cost
   of a register move.  SPEED_P is true if optimizing for speed rather
   than size.  */
//原型 set_src_cost rtl.h
int mtcs_rtlanal_set_src_cost (MtcsRtlanal *self,rtx x, machine_mode mode, bool speed_p)
{
  return mtcs_rtlanal_rtx_cost (self,x, mode, SET, 1, speed_p);
}

/* Return the cost of SET X.  SPEED_P is true if optimizing for speed
   rather than size.  */
//原型 set_rtx_cost rtl.h
int mtcs_rtlanal_set_rtx_cost (MtcsRtlanal *self,rtx x, bool speed_p)
{
  return mtcs_rtlanal_rtx_cost (self,x, VOIDmode, INSN, 4, speed_p);
}


/* Returns estimate on cost of computing SEQ.  */
//原型 seq_cost rtl.h rtlanal.cc
unsigned mtcs_rtlanal_seq_cost (MtcsRtlanal *self,const rtx_insn *seq, bool speed)
{
  unsigned cost = 0;
  rtx set;
  for (; seq; seq = NEXT_INSN (seq)){
      set = single_set (seq);
      if (set)
        cost += mtcs_rtlanal_set_rtx_cost/*!set_rtx_cost*/(self,set, speed);
      else if (NONDEBUG_INSN_P (seq)){
          int this_cost = mtcs_rtlanal_insn_cost/*!insn_cost*/(self,CONST_CAST_RTX_INSN (seq), speed);
          if (this_cost > 0)
            cost += this_cost;
          else
            cost++;
      }
  }

  return cost;
}

/* Split up a CONST_DOUBLE or integer constant rtx
   into two rtx's for single words,
   storing in *FIRST the word that comes first in memory in the target
   and in *SECOND the other.

   TODO: This function needs to be rewritten to work on any size
   integer.  */
//原型 split_double rtl.h rtlanal.cc
void mtcs_rtlanal_split_double (MtcsRtlanal *self,rtx value, rtx *first, rtx *second)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  if (CONST_INT_P (value)){
      if (HOST_BITS_PER_WIDE_INT >= (2 * BITS_PER_WORD)){
          /* In this case the CONST_INT holds both target words.
             Extract the bits from it into two word-sized pieces.
             Sign extend each half to HOST_WIDE_INT.  */
          unsigned HOST_WIDE_INT low, high;
          unsigned HOST_WIDE_INT mask, sign_bit, sign_extend;
          unsigned bits_per_word = BITS_PER_WORD;

          /* Set sign_bit to the most significant bit of a word.  */
          sign_bit = 1;
          sign_bit <<= bits_per_word - 1;

          /* Set mask so that all bits of the word are set.  We could
             have used 1 << BITS_PER_WORD instead of basing the
             calculation on sign_bit.  However, on machines where
             HOST_BITS_PER_WIDE_INT == BITS_PER_WORD, it could cause a
             compiler warning, even though the code would never be
             executed.  */
          mask = sign_bit << 1;
          mask--;

          /* Set sign_extend as any remaining bits.  */
          sign_extend = ~mask;

          /* Pick the lower word and sign-extend it.  */
          low = INTVAL (value);
          low &= mask;
          if (low & sign_bit)
            low |= sign_extend;

          /* Pick the higher word, shifted to the least significant
             bits, and sign-extend it.  */
          high = INTVAL (value);
          high >>= bits_per_word - 1;
          high >>= 1;
          high &= mask;
          if (high & sign_bit)
            high |= sign_extend;

          /* Store the words in the target machine order.  */
          if (WORDS_BIG_ENDIAN){
              *first = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,high);
              *second = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,low);
          }else{
              *first = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,low);
              *second = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,high);
          }
      }else{
          /* The rule for using CONST_INT for a wider mode
             is that we regard the value as signed.
             So sign-extend it.  */
          rtx high = (INTVAL (value) < 0 ? constm1_rtx : const0_rtx);
          if (WORDS_BIG_ENDIAN){
              *first = high;
              *second = value;
          }else{
              *first = value;
              *second = high;
          }
      }
  }else if (GET_CODE (value) == CONST_WIDE_INT){
      /* All of this is scary code and needs to be converted to
     properly work with any size integer.  */
      gcc_assert (CONST_WIDE_INT_NUNITS (value) == 2);
      if (WORDS_BIG_ENDIAN){
          *first = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,CONST_WIDE_INT_ELT (value, 1));
          *second = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,CONST_WIDE_INT_ELT (value, 0));
      }else{
          *first = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,CONST_WIDE_INT_ELT (value, 0));
          *second = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,CONST_WIDE_INT_ELT (value, 1));
      }
  }else if (!CONST_DOUBLE_P (value)){
      if (WORDS_BIG_ENDIAN){
          *first = const0_rtx;
          *second = value;
      }else{
          *first = value;
          *second = const0_rtx;
      }
  }else if (GET_MODE (value) == VOIDmode
       /* This is the old way we did CONST_DOUBLE integers.  */
       || mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,GET_MODE (value)) == MODE_INT){
      /* In an integer, the words are defined as most and least significant.
     So order them by the target's convention.  */
      if (WORDS_BIG_ENDIAN){
          *first = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,CONST_DOUBLE_HIGH (value));
          *second = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,CONST_DOUBLE_LOW (value));
      }else{
          *first = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,CONST_DOUBLE_LOW (value));
          *second = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,CONST_DOUBLE_HIGH (value));
      }
  }else{
      long l[2];

      /* Note, this converts the REAL_VALUE_TYPE to the target's
     format, splits up the floating point double and outputs
     exactly 32 bits of it into each of l[0] and l[1] --
     not necessarily BITS_PER_WORD bits.  */
      REAL_VALUE_TO_TARGET_DOUBLE (*CONST_DOUBLE_REAL_VALUE (value), l);

      /* If 32 bits is an entire word for the target, but not for the host,
     then sign-extend on the host so that the number will look the same
     way on the host that it would on the target.  See for instance
     simplify_unary_operation.  The #if is needed to avoid compiler
     warnings.  */

#if HOST_BITS_PER_LONG > 32
      if (BITS_PER_WORD < HOST_BITS_PER_LONG && BITS_PER_WORD == 32){
          if (l[0] & ((long) 1 << 31))
            l[0] |= ((unsigned long) (-1) << 32);
          if (l[1] & ((long) 1 << 31))
            l[1] |= ((unsigned long) (-1) << 32);
      }
#endif

      *first = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,l[0]);
      *second = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,l[1]);
  }
}


/* Return true if RTX code CODE has a single sequence of zero or more
   "e" operands and no rtvec operands.  Initialize its rtx_all_subrtx_bounds
   entry in that case.  */
//原型 setup_reg_subrtx_bounds rtlanal.cc
static bool setup_reg_subrtx_bounds (MtcsRtlanal *self,unsigned int code)
{
  const char *format = GET_RTX_FORMAT ((enum rtx_code) code);
  unsigned int i = 0;
  for (; format[i] != 'e'; ++i){
      if (!format[i])
        /* No subrtxes.  Leave start and count as 0.  */
        return true;
      if (format[i] == 'E' || format[i] == 'V')
          return false;
  }

  /* Record the sequence of 'e's.  */
  rtx_all_subrtx_bounds[code].start = i;
  do
    ++i;
  while (format[i] == 'e');
  rtx_all_subrtx_bounds[code].count = i - rtx_all_subrtx_bounds[code].start;
  /* rtl-iter.h relies on this.  */
  gcc_checking_assert (rtx_all_subrtx_bounds[code].count <= 3);

  for (; format[i]; ++i)
    if (format[i] == 'E' || format[i] == 'V' || format[i] == 'e')
      return false;

  return true;
}

/* Initialize the table NUM_SIGN_BIT_COPIES_IN_REP based on
   TARGET_MODE_REP_EXTENDED.

   Note that we assume that the property of
   TARGET_MODE_REP_EXTENDED(B, C) is sticky to the integral modes
   narrower than mode B.  I.e., if A is a mode narrower than B then in
   order to be able to operate on it in mode B, mode A needs to
   satisfy the requirements set by the representation of mode B.  */
//原型 init_num_sign_bit_copies_in_rep rtlanal.cc
static void init_num_sign_bit_copies_in_rep (MtcsRtlanal *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

  opt_scalar_int_mode in_mode_iter;
  scalar_int_mode mode;

  MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,in_mode_iter, MODE_INT)
      MTCS_FOR_EACH_MODE_UNTIL (mtcsMode,mode, in_mode_iter.require ()){
           scalar_int_mode in_mode = in_mode_iter.require ();
           scalar_int_mode i;

        /* Currently, it is assumed that TARGET_MODE_REP_EXTENDED
           extends to the next widest mode.  */
          gcc_assert (mtcsTarget/*!targetm.mode_rep_extended*/->mode_rep_extended(mtcsTarget,mode, in_mode) == UNKNOWN
                || mtcs_mode_get_wider/*!GET_MODE_WIDER_MODE*/(mtcsMode,mode).require () == in_mode);

        /* We are in in_mode.  Count how many bits outside of mode
           have to be copies of the sign-bit.  */
          MTCS_FOR_EACH_MODE (mtcsMode,i, mode, in_mode){
            /* This must always exist (for the last iteration it will be
               IN_MODE).  */
              scalar_int_mode wider = mtcs_mode_get_wider/*!GET_MODE_WIDER_MODE*/(mtcsMode,i).require ();

              if (mtcsTarget/*!targetm.mode_rep_extended*/->mode_rep_extended(mtcsTarget,i, wider) == SIGN_EXTEND
                /* We can only check sign-bit copies starting from the
                   top-bit.  In order to be able to check the bits we
                   have already seen we pretend that subsequent bits
                   have to be sign-bit copies too.  */
              || self->num_sign_bit_copies_in_rep [in_mode][mode])
                  self->num_sign_bit_copies_in_rep [in_mode][mode] += mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,wider)
                                                                   - mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,i);
          }
      }
}

typedef struct _RecordHardRegSetsCallbackData{
   MtcsRtlanal *mtcsRtlanal;
   HardRegSet *pset;
}RecordHardRegSetsCallbackData;
//原型 record_hard_reg_sets rtl.h rtlanal.cc
static void recordHardRegSets_cb(rtx x, const_rtx pat ATTRIBUTE_UNUSED, void *userData)
{
  RecordHardRegSetsCallbackData *info=(RecordHardRegSetsCallbackData *)userData;
  MtcsRtlanal *self=info->mtcsRtlanal;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

  HardRegSet/*!HARD_REG_SET*/ *pset = (HardRegSet *)info->pset;
  if (REG_P (x) && mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,x))
      mtcs_reg_add_to_hard_reg_set/*!add_to_hard_reg_set*/(mtcsReg,pset, GET_MODE (x), REGNO (x));
}

/* Examine INSN, and compute the set of hard registers written by it.
   Store it in *PSET.  Should only be called after reload.

   IMPLICIT is true if we should include registers that are fully-clobbered
   by calls.  This should be used with caution, since it doesn't include
   partially-clobbered registers.  */
//原型 find_all_hard_reg_sets rtl.h rtlanal.c
void mtcs_rtlanal_find_all_hard_reg_sets (MtcsRtlanal *self,const rtx_insn *insn, HardRegSet *pset, bool implicit)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFuncAbi *mtcsFuncAbi=mtcs_target_get_func_abi(mtcsTarget);
  RecordHardRegSetsCallbackData userData={self,pset};
  rtx link;
  mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET (*pset)*/(pset);
  mtcs_rtlanal_note_stores/*!note_stores*/(self,insn,
        recordHardRegSets_cb/*!record_hard_reg_sets*/, &userData/*!pset*/);
  if (CALL_P (insn) && implicit)
    *pset |= mtcs_func_abi_insn_callee_abi/*!insn_callee_abi*/(mtcsFuncAbi,insn).full_reg_clobbers ();
  for (link = REG_NOTES (insn); link; link = XEXP (link, 1))
    if (REG_NOTE_KIND (link) == REG_INC)
       recordHardRegSets_cb/*!record_hard_reg_sets*/(XEXP (link, 0), NULL, &userData/*!pset*/);
}

/* Call FUN on each register or MEM that is stored into or clobbered by X.
   (X would be the pattern of an insn).  DATA is an arbitrary pointer,
   ignored by note_stores, but passed to FUN.

   FUN receives three arguments:
   1. the REG, MEM or PC being stored in or clobbered,
   2. the SET or CLOBBER rtx that does the store,
   3. the pointer DATA provided to note_stores.

  If the item being stored in or clobbered is a SUBREG of a hard register,
  the SUBREG will be passed.  */
//原型 note_pattern_stores rtl.h rtlanal.cc
void mtcs_rtlanal_note_pattern_stores (MtcsRtlanal *self,const_rtx x, void (*fun)(rtx, const_rtx, void *), void *data)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

  int i;

  if (GET_CODE (x) == COND_EXEC)
    x = COND_EXEC_CODE (x);
  if (GET_CODE (x) == SET || GET_CODE (x) == CLOBBER){
      rtx dest = SET_DEST (x);
      while ((GET_CODE (dest) == SUBREG
          && (!REG_P (SUBREG_REG (dest))
          || REGNO (SUBREG_REG (dest)) >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)))
         || GET_CODE (dest) == ZERO_EXTRACT
         || GET_CODE (dest) == STRICT_LOW_PART)
          dest = XEXP (dest, 0);

      /* If we have a PARALLEL, SET_DEST is a list of EXPR_LIST expressions,
     each of whose first operand is a register.  */
      if (GET_CODE (dest) == PARALLEL){
          for (i = XVECLEN (dest, 0) - 1; i >= 0; i--)
            if (XEXP (XVECEXP (dest, 0, i), 0) != 0)
              (*fun) (XEXP (XVECEXP (dest, 0, i), 0), x, data);
      }else
          (*fun) (dest, x, data);
      }
  else if (GET_CODE (x) == PARALLEL)
    for (i = XVECLEN (x, 0) - 1; i >= 0; i--)
        mtcs_rtlanal_note_pattern_stores/*!note_pattern_stores*/(self,XVECEXP (x, 0, i), fun, data);
}

/* Same, but for an instruction.  If the instruction is a call, include
   any CLOBBERs in its CALL_INSN_FUNCTION_USAGE.  */
//原型 note_stores rtl.h rtlanal.cc
void mtcs_rtlanal_note_stores (MtcsRtlanal *self,const rtx_insn *insn, void (*fun) (rtx, const_rtx, void *), void *data)
{
  if (CALL_P (insn))
    for (rtx link = CALL_INSN_FUNCTION_USAGE (insn); link; link = XEXP (link, 1))
      if (GET_CODE (XEXP (link, 0)) == CLOBBER)
          mtcs_rtlanal_note_pattern_stores/*!note_pattern_stores*/(self,XEXP (link, 0), fun, data);
  mtcs_rtlanal_note_pattern_stores/*!note_pattern_stores*/(self,PATTERN (insn), fun, data);
}

/* Initialize rtx_all_subrtx_bounds.  */
//原型 init_rtlanal rtl.h rtlanal.cc
void mtcs_rtlanal_init_rtlanal (MtcsRtlanal *self)
{
    /*与主机相同
  int i;
  for (i = 0; i < NUM_RTX_CODE; i++){
      if (!setup_reg_subrtx_bounds (i))
          rtx_all_subrtx_bounds[i].count = UCHAR_MAX;
      if (GET_RTX_CLASS (i) != RTX_CONST_OBJ)
          rtx_nonconst_subrtx_bounds[i] = rtx_all_subrtx_bounds[i];
  }
  */
  init_num_sign_bit_copies_in_rep(self);
}

/* Return true if register in range [REGNO, ENDREGNO)
   appears either explicitly or implicitly in X
   other than being stored into.

   References contained within the substructure at LOC do not count.
   LOC may be zero, meaning don't ignore anything.  */
//原型 refers_to_regno_p rtl.h rtlanal.cc
bool mtcs_rtlanal_refers_to_regno_p (MtcsRtlanal *self,unsigned int regno, unsigned int endregno, const_rtx x,
           rtx *loc)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

  int i;
  unsigned int x_regno;
  RTX_CODE code;
  const char *fmt;

repeat:
  /* The contents of a REG_NONNEG note is always zero, so we must come here
     upon repeat in case the last REG_NOTE is a REG_NONNEG note.  */
  if (x == 0)
    return false;
  code = GET_CODE (x);
  switch (code){
    case REG:
      x_regno = REGNO (x);
      /* If we modifying the stack, frame, or argument pointer, it will
     clobber a virtual register.  In fact, we could be more precise,
     but it isn't worth it.  */
      if ((x_regno == mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg)
       || (mtcsReg->normalHardRegsNum.frame_pointer_regnum/*! FRAME_POINTER_REGNUM*/ !=
               mtcsReg->normalHardRegsNum.arg_pointer_regnum/*!ARG_POINTER_REGNUM*/
           && x_regno == mtcsReg->normalHardRegsNum.arg_pointer_regnum/*!ARG_POINTER_REGNUM*/)
       || x_regno == mtcsReg->normalHardRegsNum.frame_pointer_regnum/*! FRAME_POINTER_REGNUM*/)
       && mtcs_reg_virtual_register_num_p/*!VIRTUAL_REGISTER_NUM_P*/(mtcsReg,regno))
          return true;
      return endregno > x_regno && regno < END_REGNO (x);
    case SUBREG:
      /* If this is a SUBREG of a hard reg, we can see exactly which
     registers are being modified.  Otherwise, handle normally.  */
      if (REG_P (SUBREG_REG (x))
        && REGNO (SUBREG_REG (x)) < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)){
          unsigned int inner_regno = mtcs_rtlanal_subreg_regno/*!subreg_regno*/(self,x);
          unsigned int inner_endregno = inner_regno + (inner_regno <
                  mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
                     ? mtcs_rtlanal_subreg_nregs/*!subreg_nregs*/(self,x) : 1);
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
           && REGNO (SUBREG_REG (SET_DEST (x))) >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
           && mtcs_rtlanal_refers_to_regno_p/*!refers_to_regno_p*/(self,regno, endregno,
                     SUBREG_REG (SET_DEST (x)), loc))
          || (!REG_P (SET_DEST (x))
          && mtcs_rtlanal_refers_to_regno_p/*!refers_to_regno_p*/(self,regno, endregno, SET_DEST (x), loc))))
          return true;

      if (code == CLOBBER || loc == &SET_SRC (x))
          return false;
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
            if (mtcs_rtlanal_refers_to_regno_p/*!refers_to_regno_p*/(self,regno, endregno, XEXP (x, i), loc))
              return true;
      }else if (fmt[i] == 'E'){
          int j;
          for (j = XVECLEN (x, i) - 1; j >= 0; j--)
             if (loc != &XVECEXP (x, i, j)
              && mtcs_rtlanal_refers_to_regno_p/*!refers_to_regno_p*/(self,regno, endregno, XVECEXP (x, i, j), loc))
              return true;
      }
  }
  return false;
}


/* Rreturn true if modifying X will affect IN.  If X is a register or a SUBREG,
   we check if any register number in X conflicts with the relevant register
   numbers.  If X is a constant, return false.  If X is a MEM, return true iff
   IN contains a MEM (we don't bother checking for memory addresses that can't
   conflict because we expect this to be a rare case.  */
//原型 reg_overlap_mentioned_p rtl.h rtlanal.cc
bool mtcs_rtlanal_reg_overlap_mentioned_p (MtcsRtlanal *self,const_rtx x, const_rtx in)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int firstPseudoRegister= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);
   unsigned int regno, endregno;

   /* If either argument is a constant, then modifying X cannot
   affect IN.  Here we look at IN, we can profitably combine
   CONSTANT_P (x) with the switch statement below.  */
   if (CONSTANT_P (in))
      return false;

recurse:
   switch (GET_CODE (x)){
      case CLOBBER:
      case STRICT_LOW_PART:
      case ZERO_EXTRACT:
      case SIGN_EXTRACT:
         /* Overly conservative.  */
         x = XEXP (x, 0);
         goto recurse;
      case SUBREG:
         regno = REGNO (SUBREG_REG (x));
         if (regno < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
            regno = mtcs_rtlanal_subreg_regno/*!subreg_regno*/(self,x);
         endregno = regno + (regno < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/ ?
               mtcs_rtlanal_subreg_nregs/*!subreg_nregs*/(self,x) : 1);
         goto do_reg;
      case REG:
         regno = REGNO (x);
         endregno = END_REGNO (x);
   do_reg:
         return mtcs_rtlanal_refers_to_regno_p/*!refers_to_regno_p*/(self,regno, endregno, in, (rtx*) 0);

      case MEM:
      {
         const char *fmt;
         int i;
         if (MEM_P (in))
            return true;
         fmt = GET_RTX_FORMAT (GET_CODE (in));
         for (i = GET_RTX_LENGTH (GET_CODE (in)) - 1; i >= 0; i--)
            if (fmt[i] == 'e'){
               if (mtcs_rtlanal_reg_overlap_mentioned_p(self,x, XEXP (in, i)))
                  return true;
            }else if (fmt[i] == 'E'){
               int j;
               for (j = XVECLEN (in, i) - 1; j >= 0; --j)
                  if (mtcs_rtlanal_reg_overlap_mentioned_p(self,x, XVECEXP (in, i, j)))
                     return true;
            }
         return false;
      }

      case SCRATCH:
      case PC:
         return reg_mentioned_p (x, in);

      case PARALLEL:
      {
         int i;
         /* If any register in here refers to it we return true.  */
         for (i = XVECLEN (x, 0) - 1; i >= 0; i--)
            if (XEXP (XVECEXP (x, 0, i), 0) != 0
            && mtcs_rtlanal_reg_overlap_mentioned_p(self,XEXP (XVECEXP (x, 0, i), 0), in))
               return true;
         return false;
      }

      default:
         gcc_assert (CONSTANT_P (x));
         return false;
   }
}

/* Return true if the destination of SET equals the source
   and there are no side effects.  */
//原型 set_noop_p rtl.h rtlanal.cc
bool mtcs_rtlanal_set_noop_p (MtcsRtlanal *self,const_rtx set)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   rtx src = SET_SRC (set);
   rtx dst = SET_DEST (set);

   if (dst == mtcsRTL/*!pc_rtx*/->pc_rtx && src == mtcsRTL/*!pc_rtx*/->pc_rtx)
      return true;

   if (MEM_P (dst) && MEM_P (src))
      return (rtx_equal_p (dst, src) && !side_effects_p (dst) && !side_effects_p (src));

   if (GET_CODE (dst) == ZERO_EXTRACT)
      return (rtx_equal_p (XEXP (dst, 0), src) && !BITS_BIG_ENDIAN && XEXP (dst, 2) == const0_rtx
                           && !side_effects_p (src) && !side_effects_p (XEXP (dst, 0)));

   if (GET_CODE (dst) == STRICT_LOW_PART)
      dst = XEXP (dst, 0);

   if (GET_CODE (src) == SUBREG && GET_CODE (dst) == SUBREG){
      if (maybe_ne (SUBREG_BYTE (src), SUBREG_BYTE (dst)))
         return false;
      src = SUBREG_REG (src);
      dst = SUBREG_REG (dst);
      if (GET_MODE (src) != GET_MODE (dst))
         /* It is hard to tell whether subregs refer to the same bits, so act
         conservatively and return false.  */
         return false;
   }

   /* It is a NOOP if destination overlaps with selected src vector
   elements.  */
   if (GET_CODE (src) == VEC_SELECT
   && REG_P (XEXP (src, 0)) && REG_P (dst)
   && mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,XEXP (src, 0))
   && mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,dst)){
      int i;
      rtx par = XEXP (src, 1);
      rtx src0 = XEXP (src, 0);
      poly_int64 c0;
      if (!poly_int_rtx_p (XVECEXP (par, 0, 0), &c0))
         return false;
      poly_int64 offset = mtcs_mode_get_unit_size/*!GET_MODE_UNIT_SIZE*/(mtcsMode,GET_MODE (src0)) * c0;

      for (i = 1; i < XVECLEN (par, 0); i++){
         poly_int64 c0i;
         if (!poly_int_rtx_p (XVECEXP (par, 0, i), &c0i) || maybe_ne (c0i, c0 + i))
            return false;
      }
      return   mtcs_reg_can_change_mode/*!REG_CAN_CHANGE_MODE_P*/(mtcsReg,REGNO (dst), GET_MODE (src0), GET_MODE (dst))
            && mtcs_rtl_simplify_subreg_regno/*!simplify_subreg_regno*/(mtcsRTL,REGNO (src0), GET_MODE (src0),
                  offset, GET_MODE (dst)) == (int) REGNO (dst);
   }

   return (REG_P (src) && REG_P (dst) && REGNO (src) == REGNO (dst));
}


/* This function returns the regno offset of a subreg expression.
   xregno - A regno of an inner hard subreg_reg (or what will become one).
   xmode  - The mode of xregno.
   offset - The byte offset.
   ymode  - The mode of a top level SUBREG (or what may become one).
   RETURN - The regno offset which would be used.  */
//原型 subreg_regno_offset rtl.h rtlanal.cc
unsigned int mtcs_rtlanal_subreg_regno_offset (MtcsRtlanal *self,unsigned int xregno, machine_mode xmode,
           poly_uint64 offset, machine_mode ymode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   struct subreg_info info;
   mtcs_rtl_subreg_get_info/*!subreg_get_info*/(mtcsRTL,xregno, xmode, offset, ymode, &info);
   return info.offset;
}

/* Return the number of registers that a subreg REG with REGNO
   expression refers to.  This is a copy of the rtlanal.cc:subreg_nregs
   changed so that the regno can be passed in. */
//原型 subreg_nregs_with_regno rtl.h rtlanal.cc
unsigned int mtcs_rtlanal_subreg_nregs_with_regno (MtcsRtlanal *self,unsigned int regno, const_rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   struct subreg_info info;
   rtx subreg = SUBREG_REG (x);
   mtcs_rtl_subreg_get_info/*!subreg_get_info*/(mtcsRTL,
         regno, GET_MODE (subreg), SUBREG_BYTE (x), GET_MODE (x),&info);
   return info.nregs;
}


/* Return the number of registers that a subreg expression refers
   to.  */
//原型 subreg_nregs rtl.h rtlanal.cc
unsigned int mtcs_rtlanal_subreg_nregs (MtcsRtlanal *self,const_rtx x)
{
  return mtcs_rtlanal_subreg_nregs_with_regno/*!subreg_nregs_with_regno*/(self,REGNO (SUBREG_REG (x)), x);
}

/* Return the final regno that a subreg expression refers to.  */
//原型 subreg_regno rtl.h rtlanal.cc
unsigned int mtcs_rtlanal_subreg_regno (MtcsRtlanal *self,const_rtx x)
{
   unsigned int ret;
   rtx subreg = SUBREG_REG (x);
   int regno = REGNO (subreg);
   ret = regno + mtcs_rtlanal_subreg_regno_offset/*!subreg_regno_offset*/(self,regno,
         GET_MODE (subreg),SUBREG_BYTE (x),GET_MODE (x));
   return ret;
}

/* Return true if evaluating rtx X might cause a trap.
   FLAGS controls how to consider MEMs.  A true means the context
   of the access may have changed from the original, such that the
   address may have become invalid.  */
//原型 may_trap_p_1 rtl.h rtlanal.cc
bool  mtcs_rtlanal_may_trap_p_1 (MtcsRtlanal *self,const_rtx x, unsigned flags)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   int i;
   enum rtx_code code;
   const char *fmt;

   /* We make no distinction currently, but this function is part of
   the internal target-hooks ABI so we keep the parameter as
   "unsigned flags".  */
   bool code_changed = flags != 0;

   if (x == 0)
      return false;
   code = GET_CODE (x);
   switch (code){
   /* Handle these cases quickly.  */
   CASE_CONST_ANY:
   case SYMBOL_REF:
   case LABEL_REF:
   case CONST:
   case PC:
   case REG:
   case SCRATCH:
      return false;

   case UNSPEC:
      return mtcsTarget/*!targetm.unspec_may_trap_p*/->unspec_may_trap_p(mtcsTarget,x, flags);

   case UNSPEC_VOLATILE:
   case ASM_INPUT:
   case TRAP_IF:
      return true;

   case ASM_OPERANDS:
      return MEM_VOLATILE_P (x);

   /* Memory ref can trap unless it's a static var or a stack slot.  */
   case MEM:
      /* Recognize specific pattern of stack checking probes.  */
      if (mtcsOptionsItem->x_flag_stack_check  && MEM_VOLATILE_P (x)
      && XEXP (x, 0) ==mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL))
         return true;
      if (/* MEM_NOTRAP_P only relates to the actual position of the memory
      reference; moving it out of context such as when moving code
      when optimizing, might cause its address to become invalid.  */
      code_changed || !MEM_NOTRAP_P (x)){
         poly_int64 size = mtcs_rtl_is_mem_size_known_p/*!MEM_SIZE_KNOWN_P*/(mtcsRTL,x)
               ? mtcs_rtl_get_mem_size/*!MEM_SIZE*/(mtcsRTL,x) : -1;
         return rtx_addr_can_trap_p_1(self,XEXP (x, 0), 0, size,  GET_MODE (x), code_changed);
      }
      return false;

   /* Division by a non-constant might trap.  */
   case DIV:
   case MOD:
   case UDIV:
   case UMOD:
      if (mtcs_mode_honor_snans/*!HONOR_SNANS*/(mtcsMode,x))
         return true;
      if (mtcs_mode_is_float_p/*!FLOAT_MODE_P*/(mtcsMode,GET_MODE (x)))
         return mtcsOptionsItem->x_flag_trapping_math;
      if (!CONSTANT_P (XEXP (x, 1)) || (XEXP (x, 1) == const0_rtx))
         return true;
      if (GET_CODE (XEXP (x, 1)) == CONST_VECTOR){
         /* For CONST_VECTOR, return 1 if any element is or might be zero.  */
         unsigned int n_elts;
         rtx op = XEXP (x, 1);
         if (!mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,GET_MODE (op)).is_constant (&n_elts)){
            if (!CONST_VECTOR_DUPLICATE_P (op))
               return true;
            for (unsigned i = 0; i < (unsigned int) XVECLEN (op, 0); i++)
               if (CONST_VECTOR_ENCODED_ELT (op, i) == const0_rtx)
                  return true;
         }else
            for (unsigned i = 0; i < n_elts; i++)
               if (CONST_VECTOR_ELT (op, i) == const0_rtx)
                  return true;
      }
      break;

   case EXPR_LIST:
      /* An EXPR_LIST is used to represent a function call.  This
      certainly may trap.  */
      return true;

   case GE:
   case GT:
   case LE:
   case LT:
   case LTGT:
   case COMPARE:
   /* Treat min/max similar as comparisons.  */
   case SMIN:
   case SMAX:
      /* Some floating point comparisons may trap.  */
      if (!mtcsOptionsItem->x_flag_trapping_math)
         break;
      /* ??? There is no machine independent way to check for tests that trap
      when COMPARE is used, though many targets do make this distinction.
      For instance, sparc uses CCFPE for compares which generate exceptions
      and CCFP for compares which do not generate exceptions.  */
      if (mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,x))
         return true;
      /* But often the compare has some CC mode, so check operand
      modes as well.  */
      if (mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,XEXP (x, 0))
      || mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,XEXP (x, 1)))
         return true;
      break;

   case EQ:
   case NE:
      if (mtcs_mode_honor_snans/*!HONOR_SNANS*/(mtcsMode,x))
         return true;
      /* Often comparison is CC mode, so check operand modes.  */
      if (mtcs_mode_honor_snans/*!HONOR_SNANS*/(mtcsMode,XEXP (x, 0))
      || mtcs_mode_honor_snans/*!HONOR_SNANS*/(mtcsMode,XEXP (x, 1)))
         return true;
      break;

   case FIX:
   case UNSIGNED_FIX:
      /* Conversion of floating point might trap.  */
      if (mtcsOptionsItem->x_flag_trapping_math && mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,XEXP (x, 0)))
         return true;
      break;

   case NEG:
   case ABS:
   case SUBREG:
   case VEC_MERGE:
   case VEC_SELECT:
   case VEC_CONCAT:
   case VEC_DUPLICATE:
      /* These operations don't trap even with floating point.  */
      break;

   default:
      /* Any floating arithmetic may trap.  */
      if (mtcs_mode_is_float_p/*!FLOAT_MODE_P*/(mtcsMode,GET_MODE (x)) && mtcsOptionsItem->x_flag_trapping_math)
         return true;
   }

   fmt = GET_RTX_FORMAT (code);
   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--){
      if (fmt[i] == 'e'){
         if (mtcs_rtlanal_may_trap_p_1/*!may_trap_p_1*/(self,XEXP (x, i), flags))
            return true;
      }else if (fmt[i] == 'E'){
         int j;
         for (j = 0; j < XVECLEN (x, i); j++)
            if (mtcs_rtlanal_may_trap_p_1/*!may_trap_p_1*/(self,XVECEXP (x, i, j), flags))
               return true;
      }
   }
   return false;
}

/* Return true if evaluating rtx X might cause a trap.  */
//原型 may_trap_p rtl.h rtlanal.cc
bool mtcs_rtlanal_may_trap_p (MtcsRtlanal *self,const_rtx x)
{
  return mtcs_rtlanal_may_trap_p_1/*!may_trap_p_1*/(self,x, 0);
}

/* Same as above, but additionally return true if evaluating rtx X might
   cause a fault.  We define a fault for the purpose of this function as a
   erroneous execution condition that cannot be encountered during the normal
   execution of a valid program; the typical example is an unaligned memory
   access on a strict alignment machine.  The compiler guarantees that it
   doesn't generate code that will fault from a valid program, but this
   guarantee doesn't mean anything for individual instructions.  Consider
   the following example:

      struct S { int d; union { char *cp; int *ip; }; };

      int foo(struct S *s)
      {
   if (s->d == 1)
     return *s->ip;
   else
     return *s->cp;
      }

   on a strict alignment machine.  In a valid program, foo will never be
   invoked on a structure for which d is equal to 1 and the underlying
   unique field of the union not aligned on a 4-byte boundary, but the
   expression *s->ip might cause a fault if considered individually.

   At the RTL level, potentially problematic expressions will almost always
   verify may_trap_p; for example, the above dereference can be emitted as
   (mem:SI (reg:P)) and this expression is may_trap_p for a generic register.
   However, suppose that foo is inlined in a caller that causes s->cp to
   point to a local character variable and guarantees that s->d is not set
   to 1; foo may have been effectively translated into pseudo-RTL as:

      if ((reg:SI) == 1)
   (set (reg:SI) (mem:SI (%fp - 7)))
      else
   (set (reg:QI) (mem:QI (%fp - 7)))

   Now (mem:SI (%fp - 7)) is considered as not may_trap_p since it is a
   memory reference to a stack slot, but it will certainly cause a fault
   on a strict alignment machine.  */
//原型 may_trap_or_fault_p rtl.h rtlanal.cc
bool mtcs_rtlanal_may_trap_or_fault_p (MtcsRtlanal *self,const_rtx x)
{
  return mtcs_rtlanal_may_trap_p_1/*!may_trap_p_1*/(self,x, 1);
}


/* Return true if REG is set or clobbered inside INSN.  */
//原型 reg_set_p rtl.h rtlanal.cc
bool mtcs_rtlanal_reg_set_p (MtcsRtlanal *self,const_rtx reg, const_rtx insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFuncAbi *mtcsFuncAbi=mtcs_target_get_func_abi(mtcsTarget);

   /* After delay slot handling, call and branch insns might be in a
   sequence.  Check all the elements there.  */
   if (INSN_P (insn) && GET_CODE (PATTERN (insn)) == SEQUENCE){
      for (int i = 0; i < XVECLEN (PATTERN (insn), 0); ++i)
         if (mtcs_rtlanal_reg_set_p/*!reg_set_p*/(self,reg, XVECEXP (PATTERN (insn), 0, i)))
            return true;

      return false;
   }

   /* We can be passed an insn or part of one.  If we are passed an insn,
   check if a side-effect of the insn clobbers REG.  */
   if (INSN_P (insn)
   && (FIND_REG_INC_NOTE (insn, reg)
   || (CALL_P (insn)
   && ((REG_P (reg)
   && REGNO (reg) < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
   && (mtcs_func_abi_insn_callee_abi/*!insn_callee_abi*/(mtcsFuncAbi,
         as_a<const rtx_insn *> (insn)).clobbers_reg_p (GET_MODE (reg), REGNO (reg))))
   || MEM_P (reg)
   || mtcs_rtlanal_find_reg_fusage/*!find_reg_fusage*/(self,insn, CLOBBER, reg)))))
      return true;
   rtx stackPointerRtx=mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL);
   /* There are no REG_INC notes for SP autoinc.  */
   if (reg == stackPointerRtx/*!stack_pointer_rtx*/ && INSN_P (insn)){
      subrtx_var_iterator::array_type array;
      FOR_EACH_SUBRTX_VAR (iter, array, PATTERN (insn), NONCONST){
         rtx mem = *iter;
         if (mem  && MEM_P (mem) && GET_RTX_CLASS (GET_CODE (XEXP (mem, 0))) == RTX_AUTOINC){
            if (XEXP (XEXP (mem, 0), 0) == stackPointerRtx/*!stack_pointer_rtx*/)
               return true;
            iter.skip_subrtxes ();
         }
      }
   }

   return mtcs_rtlanal_set_of/*!set_of*/(self,reg, insn) != NULL_RTX;
}

/* Helper function for set_of.  */
struct set_of_data
{
   const_rtx found;
   const_rtx pat;
   MtcsRtlanal *mtcsRtlanal;
};
//原型 set_of_1
static void set_of_1 (rtx x, const_rtx pat, void *data1)
{
   struct set_of_data *const data = (struct set_of_data *) (data1);
   MtcsRtlanal *self=data->mtcsRtlanal;
   if (rtx_equal_p (x, data->pat)
         || (!MEM_P (x) && mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(self,data->pat, x)))
      data->found = pat;
}

/* Give an INSN, return a SET or CLOBBER expression that does modify PAT
   (either directly or via STRICT_LOW_PART and similar modifiers).  */
//原型 set_of rtl.h rtlanal.cc
const_rtx mtcs_rtlanal_set_of (MtcsRtlanal *self,const_rtx pat, const_rtx insn)
{
   struct set_of_data data;
   data.found = NULL_RTX;
   data.pat = pat;
   data.mtcsRtlanal=self;
   mtcs_rtlanal_note_pattern_stores/*!note_pattern_stores*/(self,
         INSN_P (insn) ? PATTERN (insn) : insn, set_of_1, &data);
   return data.found;
}

/* Return true if DATUM, or any overlap of DATUM, of kind CODE is found
   in the CALL_INSN_FUNCTION_USAGE information of INSN.  */
//原型 find_reg_fusage rtl.h rtlanal.cc
bool mtcs_rtlanal_find_reg_fusage (MtcsRtlanal *self,const_rtx insn, enum rtx_code code, const_rtx datum)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   /* If it's not a CALL_INSN, it can't possibly have a
   CALL_INSN_FUNCTION_USAGE field, so don't bother checking.  */
   if (!CALL_P (insn))
      return false;

   gcc_assert (datum);

   if (!REG_P (datum)){
      rtx link;

   for (link = CALL_INSN_FUNCTION_USAGE (insn);
   link;
   link = XEXP (link, 1))
   if (GET_CODE (XEXP (link, 0)) == code && rtx_equal_p (datum, XEXP (XEXP (link, 0), 0)))
      return true;
   }else{
      unsigned int regno = REGNO (datum);
      /* CALL_INSN_FUNCTION_USAGE information cannot contain references
      to pseudo registers, so don't bother checking.  */
      if (regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)){
         unsigned int end_regno = END_REGNO (datum);
         unsigned int i;

         for (i = regno; i < end_regno; i++)
            if (mtcs_rtlanal_find_regno_fusage/*!find_regno_fusage*/(self,insn, code, i))
               return true;
      }
   }

   return false;
}

/* Return true if REGNO, or any overlap of REGNO, of kind CODE is found
   in the CALL_INSN_FUNCTION_USAGE information of INSN.  */
//原型 find_regno_fusage rtl.h rtlanal.cc
bool mtcs_rtlanal_find_regno_fusage (MtcsRtlanal *self,const_rtx insn, enum rtx_code code, unsigned int regno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   rtx link;
   /* CALL_INSN_FUNCTION_USAGE information cannot contain references
   to pseudo registers, so don't bother checking.  */
   if (regno >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg) || !CALL_P (insn) )
      return false;

   for (link = CALL_INSN_FUNCTION_USAGE (insn); link; link = XEXP (link, 1)){
      rtx op, reg;
      if (GET_CODE (op = XEXP (link, 0)) == code
            && REG_P (reg = XEXP (op, 0))  && REGNO (reg) <= regno  && END_REGNO (reg) > regno)
      return true;
   }
   return false;
}

/* Return true if X is a SUBREG and if storing a value to X would
   preserve some of its SUBREG_REG.  For example, on a normal 32-bit
   target, using a SUBREG to store to one half of a DImode REG would
   preserve the other half.  */
//原型 read_modify_subreg_p rtl.h rtlanal.cc
bool mtcs_rtlanal_read_modify_subreg_p (MtcsRtlanal *self,const_rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   if (GET_CODE (x) != SUBREG)
      return false;
   poly_uint64 isize = mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (SUBREG_REG (x)));
   poly_uint64 osize =mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (x));
   poly_uint64 regsize =mtcs_mode_get_regmode_natural_size/*!REGMODE_NATURAL_SIZE*/(mtcsMode,GET_MODE (SUBREG_REG (x)));
   /* The inner and outer modes of a subreg must be ordered, so that we
   can tell whether they're paradoxical or partial.  */
   gcc_checking_assert (ordered_p (isize, osize));
   return (maybe_gt (isize, osize) && maybe_gt (isize, regsize));
}

/* Return true if the old value of X, a register, is referenced in BODY.  If X
   is entirely replaced by a new value and the only use is as a SET_DEST,
   we do not consider it a reference.  */
//原型 reg_referenced_p rtl.h rtlanal.cc
bool mtcs_rtlanal_reg_referenced_p (MtcsRtlanal *self,const_rtx x, const_rtx body)
{
   int i;

   switch (GET_CODE (body)){
      case SET:
         if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(self,x, SET_SRC (body)))
            return true;

         /* If the destination is anything other than PC, a REG or a SUBREG
         of a REG that occupies all of the REG, the insn references X if
         it is mentioned in the destination.  */
         if (GET_CODE (SET_DEST (body)) != PC
         && !REG_P (SET_DEST (body))
         && ! (GET_CODE (SET_DEST (body)) == SUBREG
         && REG_P (SUBREG_REG (SET_DEST (body)))
         && !mtcs_rtlanal_read_modify_subreg_p/*!read_modify_subreg_p*/(self,SET_DEST (body)))
         && mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(self,x, SET_DEST (body)))
            return true;
         return false;

      case ASM_OPERANDS:
         for (i = ASM_OPERANDS_INPUT_LENGTH (body) - 1; i >= 0; i--)
            if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(self,x, ASM_OPERANDS_INPUT (body, i)))
               return true;
         return false;

      case CALL:
      case USE:
      case IF_THEN_ELSE:
         return mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(self,x, body);

      case TRAP_IF:
         return mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(self,x, TRAP_CONDITION (body));

      case PREFETCH:
         return mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(self,x, XEXP (body, 0));

      case UNSPEC:
      case UNSPEC_VOLATILE:
         for (i = XVECLEN (body, 0) - 1; i >= 0; i--)
            if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(self,x, XVECEXP (body, 0, i)))
               return true;
         return false;

      case PARALLEL:
         for (i = XVECLEN (body, 0) - 1; i >= 0; i--)
            if (mtcs_rtlanal_reg_referenced_p/*!reg_referenced_p*/(self,x, XVECEXP (body, 0, i)))
               return true;
         return false;

      case CLOBBER:
         if (MEM_P (XEXP (body, 0)))
            if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(self,x, XEXP (XEXP (body, 0), 0)))
               return true;
         return false;

      case COND_EXEC:
         if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(self,x, COND_EXEC_TEST (body)))
            return true;
         return mtcs_rtlanal_reg_referenced_p/*!reg_referenced_p*/(self,x, COND_EXEC_CODE (body));

      default:
         return false;
   }
}

/* Similar to reg_set_p, but check all registers in X.  Return false only if
   none of them are modified in INSN.  Return true if X contains a MEM; this
   routine does use memory aliasing.  */
//原型 modified_in_p rtl.h rtlanal.cc
bool mtcs_rtlanal_modified_in_p (MtcsRtlanal *self,const_rtx x, const_rtx insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);

   const enum rtx_code code = GET_CODE (x);
   const char *fmt;
   int i, j;

   switch (code){
      CASE_CONST_ANY:
      case CONST:
      case SYMBOL_REF:
      case LABEL_REF:
         return false;

      case PC:
         return true;

      case MEM:
         if (mtcs_rtlanal_modified_in_p/*!modified_in_p*/(self,XEXP (x, 0), insn))
            return true;
         if (MEM_READONLY_P (x))
            return false;
         if (mtcs_alias_memory_modified_in_insn_p/*!memory_modified_in_insn_p*/(mtcsAlias,x, insn))
            return true;
         return false;

      case REG:
         return mtcs_rtlanal_reg_set_p/*!reg_set_p*/(self,x, insn);

      default:
         break;
   }

   fmt = GET_RTX_FORMAT (code);
   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--){
      if (fmt[i] == 'e' && mtcs_rtlanal_modified_in_p/*!modified_in_p*/(self,XEXP (x, i), insn))
         return true;
      else if (fmt[i] == 'E')
         for (j = XVECLEN (x, i) - 1; j >= 0; j--)
            if (mtcs_rtlanal_modified_in_p/*!modified_in_p*/(self,XVECEXP (x, i, j), insn))
               return true;
   }

   return false;
}

/* Return cost of address expression X.
   Expect that X is properly formed address reference.

   SPEED parameter specify whether costs optimized for speed or size should
   be returned.  */
//原型 address_cost rtl.h rtlanal.cc
int mtcs_rtlanal_address_cost (MtcsRtlanal *self,rtx x, machine_mode mode, addr_space_t as, bool speed)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   /* We may be asked for cost of various unusual addresses, such as operands
   of push instruction.  It is not worthwhile to complicate writing
   of the target hook by such cases.  */

   if (!mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/(mtcsRecog,mode, x, as))
      return 1000;

   return mtcsTarget/*!targetm.address_cost*/->address_cost(mtcsTarget,x, mode, as, speed);
}

/* Return true if X contains a paradoxical subreg.  */
//原型 contains_paradoxical_subreg_p rtlanal.h rtlanal.cc
bool mtcs_rtlanal_contains_paradoxical_subreg_p (MtcsRtlanal *self,rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   subrtx_var_iterator::array_type array;
   FOR_EACH_SUBRTX_VAR (iter, array, x, NONCONST){
      x = *iter;
      if (SUBREG_P (x) && mtcs_rtl_paradoxical_subreg_p/*!paradoxical_subreg_p*/(mtcsRTL,x))
         return true;
   }
   return false;
}

/* Return TRUE iff DEST is a register or subreg of a register, is a
   complete rather than read-modify-write destination, and contains
   register TEST_REGNO.  */
//原型 covers_regno_no_parallel_p rtlanal.cc
static bool covers_regno_no_parallel_p (MtcsRtlanal *self,const_rtx dest, unsigned int test_regno)
{
  unsigned int regno, endregno;

  if (GET_CODE (dest) == SUBREG && !mtcs_rtlanal_read_modify_subreg_p/*!read_modify_subreg_p*/(self,dest))
    dest = SUBREG_REG (dest);

  if (!REG_P (dest))
    return false;

  regno = REGNO (dest);
  endregno = END_REGNO (dest);
  return (test_regno >= regno && test_regno < endregno);
}

/* Check whether instruction pattern PAT contains a SET with the following
   properties:

   - the SET is executed unconditionally; and
   - either:
     - the destination of the SET is a REG that contains REGNO; or
     - both:
       - the destination of the SET is a SUBREG of such a REG; and
       - writing to the subreg clobbers all of the SUBREG_REG
    (in other words, read_modify_subreg_p is false).

   If PAT does have a SET like that, return the set, otherwise return null.

   This is intended to be an alternative to single_set for passes that
   can handle patterns with multiple_sets.  */
//原型 simple_regno_set rtl.h rtlanal.cc
rtx mtcs_rtlanal_simple_regno_set (MtcsRtlanal *self,rtx pat, unsigned int regno)
{
   if (GET_CODE (pat) == PARALLEL){
      int last = XVECLEN (pat, 0) - 1;
      for (int i = 0; i < last; ++i)
         if (rtx set = mtcs_rtlanal_simple_regno_set/*!simple_regno_set*/(self,XVECEXP (pat, 0, i), regno))
            return set;

      pat = XVECEXP (pat, 0, last);
   }

   if (GET_CODE (pat) == SET && covers_regno_no_parallel_p(self,SET_DEST (pat), regno))
      return pat;

   return nullptr;
}

/* Return true if X has a value that can vary even between two
   executions of the program.  false means X can be compared reliably
   against certain constants or near-constants.
   FOR_ALIAS is nonzero if we are called from alias analysis; if it is
   zero, we are slightly more conservative.
   The frame pointer and the arg pointer are considered constant.  */
//原型 rtx_varies_p rtl.h rtlanal.cc
bool mtcs_rtlanal_rtx_varies_p (MtcsRtlanal *self,const_rtx x, bool for_alias)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   RTX_CODE code;
   int i;
   const char *fmt;

   if (!x)
      return false;

   code = GET_CODE (x);
   switch (code){
      case MEM:
         return !MEM_READONLY_P (x) || mtcs_rtlanal_rtx_varies_p/*!rtx_varies_p*/(self,XEXP (x, 0), for_alias);

      case CONST:
      CASE_CONST_ANY:
      case SYMBOL_REF:
      case LABEL_REF:
         return false;

      case REG:
         /* Note that we have to test for the actual rtx used for the frame
         and arg pointers and not just the register number in case we have
         eliminated the frame and/or arg pointer and are using it
         for pseudos.  */
         if (x ==mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL)
         || x == mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL)
         /* The arg pointer varies if it is not a fixed register.  */
         || (x == mtcs_rtl_get_arg_pointer_rtx/*!arg_pointer_rtx*/(mtcsRTL)
         && mtcsReg->hardRegs.x_fixed_regs/*!fixed_regs*/[mtcs_reg_get_arg_pointer_regnum/*!ARG_POINTER_REGNUM*/(mtcsReg)]))
            return false;
         if (x == mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(mtcsRTL)
         /* ??? When call-clobbered, the value is stable modulo the restore
         that must happen after a call.  This currently screws up
         local-alloc into believing that the restore is not needed, so we
         must return 0 only if we are called from alias analysis.  */
         && (!mtcs_reg_get_pic_offset_table_reg_call_clobbered/*!PIC_OFFSET_TABLE_REG_CALL_CLOBBERED*/(mtcsReg) || for_alias))
            return false;
         return true;

      case LO_SUM:
         /* The operand 0 of a LO_SUM is considered constant
         (in fact it is related specifically to operand 1)
         during alias analysis.  */
         return (! for_alias && mtcs_rtlanal_rtx_varies_p/*!rtx_varies_p*/(self,XEXP (x, 0), for_alias))
         || mtcs_rtlanal_rtx_varies_p/*!rtx_varies_p*/(self,XEXP (x, 1), for_alias);

      case ASM_OPERANDS:
         if (MEM_VOLATILE_P (x))
            return true;

      /* Fall through.  */

      default:
         break;
   }

   fmt = GET_RTX_FORMAT (code);
   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
      if (fmt[i] == 'e'){
         if (mtcs_rtlanal_rtx_varies_p/*!rtx_varies_p*/(self,XEXP (x, i), for_alias))
            return true;
      }else if (fmt[i] == 'E'){
         int j;
         for (j = 0; j < XVECLEN (x, i); j++)
            if (mtcs_rtlanal_rtx_varies_p/*!rtx_varies_p*/(self,XVECEXP (x, i, j), for_alias))
               return true;
      }

   return false;
}

/* Remove register note NOTE from the REG_NOTES of INSN.  */
//原型 remove_note rtl.h rtlanal.cc
void mtcs_rtlanal_remove_note (MtcsRtlanal *self,rtx_insn *insn, const_rtx note)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);

   rtx link;
   if (note == NULL_RTX)
      return;

   if (REG_NOTES (insn) == note)
      REG_NOTES (insn) = XEXP (note, 1);
   else
      for (link = REG_NOTES (insn); link; link = XEXP (link, 1))
         if (XEXP (link, 1) == note){
            XEXP (link, 1) = XEXP (note, 1);
            break;
         }

   switch (REG_NOTE_KIND (note)){
      case REG_EQUAL:
      case REG_EQUIV:
         mtcs_dfscan_df_notes_rescan/*!df_notes_rescan*/(mtcsDfscan,insn);
         break;
      default:
         break;
   }
}

/* Remove REG_EQUAL and/or REG_EQUIV notes if INSN has such notes.
   If NO_RESCAN is false and any notes were removed, call
   df_notes_rescan.  Return true if any note has been removed.  */
//原型 remove_reg_equal_equiv_notes rtl.h rtlanal.cc
bool mtcs_rtlanal_remove_reg_equal_equiv_notes (MtcsRtlanal *self,rtx_insn *insn, bool no_rescan)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);

   rtx *loc;
   bool ret = false;

   loc = &REG_NOTES (insn);
   while (*loc){
      enum reg_note kind = REG_NOTE_KIND (*loc);
      if (kind == REG_EQUAL || kind == REG_EQUIV){
         *loc = XEXP (*loc, 1);
         ret = true;
      }else
         loc = &XEXP (*loc, 1);
   }
   if (ret && !no_rescan)
      mtcs_dfscan_df_notes_rescan/*!df_notes_rescan*/(mtcsDfscan,insn);
   return ret;
}


/* Remove all REG_EQUAL and REG_EQUIV notes referring to REGNO.  */
//原型 remove_reg_equal_equiv_notes_for_regno rtl.h rtlanal.cc
void mtcs_rtlanal_remove_reg_equal_equiv_notes_for_regno (MtcsRtlanal *self,unsigned int regno)
{
   df_ref eq_use;

   if (!df)
      return;

   /* This loop is a little tricky.  We cannot just go down the chain because
   it is being modified by some actions in the loop.  So we just iterate
   over the head.  We plan to drain the list anyway.  */
   while ((eq_use = DF_REG_EQ_USE_CHAIN (regno)) != NULL){
      rtx_insn *insn = DF_REF_INSN (eq_use);
      rtx note = find_reg_equal_equiv_note (insn);

      /* This assert is generally triggered when someone deletes a REG_EQUAL
      or REG_EQUIV note by hacking the list manually rather than calling
      remove_note.  */
      gcc_assert (note);

      mtcs_rtlanal_remove_note/*!remove_note*/(self,insn, note);
   }
}

/* Given an insn INSN and condition COND, return the condition in a
   canonical form to simplify testing by callers.  Specifically:

   (1) The code will always be a comparison operation (EQ, NE, GT, etc.).
   (2) Both operands will be machine operands.
   (3) If an operand is a constant, it will be the second operand.
   (4) (LE x const) will be replaced with (LT x <const+1>) and similarly
       for GE, GEU, and LEU.

   If the condition cannot be understood, or is an inequality floating-point
   comparison which needs to be reversed, 0 will be returned.

   If REVERSE is nonzero, then reverse the condition prior to canonizing it.

   If EARLIEST is nonzero, it is a pointer to a place where the earliest
   insn used in locating the condition was found.  If a replacement test
   of the condition is desired, it should be placed in front of that
   insn and we will be sure that the inputs are still valid.

   If WANT_REG is nonzero, we wish the condition to be relative to that
   register, if possible.  Therefore, do not canonicalize the condition
   further.  If ALLOW_CC_MODE is nonzero, allow the condition returned
   to be a compare to a CC mode register.

   If VALID_AT_INSN_P, the condition must be valid at both *EARLIEST
   and at INSN.  */
//原型 canonicalize_condition rtl.h rtlanal.cc
rtx mtcs_rtlanal_canonicalize_condition (MtcsRtlanal *self,rtx_insn *insn, rtx cond, int reverse,
         rtx_insn **earliest,rtx want_reg, int allow_cc_mode, int valid_at_insn_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsReal *mtcsReal =mtcs_target_get_real(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

   enum rtx_code code;
   rtx_insn *prev = insn;
   const_rtx set;
   rtx tem;
   rtx op0, op1;
   int reverse_code = 0;
   machine_mode mode;
   basic_block bb = BLOCK_FOR_INSN (insn);

   code = GET_CODE (cond);
   mode = GET_MODE (cond);
   op0 = XEXP (cond, 0);
   op1 = XEXP (cond, 1);

   if (reverse)
      code = mtcs_dojump_reversed_comparison_code/*!reversed_comparison_code*/(mtcsDojump,cond, insn);
   if (code == UNKNOWN)
      return 0;

   if (earliest)
      *earliest = insn;

   /* If we are comparing a register with zero, see if the register is set
   in the previous insn to a COMPARE or a comparison operation.  Perform
   the same tests as a function of STORE_FLAG_VALUE as find_comparison_args
   in cse.cc  */

   while ((GET_RTX_CLASS (code) == RTX_COMPARE || GET_RTX_CLASS (code) == RTX_COMM_COMPARE)
   && op1 == CONST0_RTX (GET_MODE (op0))  && op0 != want_reg){
      /* Set nonzero when we find something of interest.  */
      rtx x = 0;

      /* If this is a COMPARE, pick up the two things being compared.  */
      if (GET_CODE (op0) == COMPARE){
         op1 = XEXP (op0, 1);
         op0 = XEXP (op0, 0);
         continue;
      }else if (!REG_P (op0))
         break;

      /* Go back to the previous insn.  Stop if it is not an INSN.  We also
      stop if it isn't a single set or if it has a REG_INC note because
      we don't want to bother dealing with it.  */

      prev = prev_nonnote_nondebug_insn (prev);

      if (prev == 0 || !NONJUMP_INSN_P (prev) || FIND_REG_INC_NOTE (prev, NULL_RTX)
      /* In cfglayout mode, there do not have to be labels at the
      beginning of a block, or jumps at the end, so the previous
      conditions would not stop us when we reach bb boundary.  */
      || BLOCK_FOR_INSN (prev) != bb)
         break;

      set = mtcs_rtlanal_set_of/*!set_of*/(self,op0, prev);

      if (set  && (GET_CODE (set) != SET || !rtx_equal_p (SET_DEST (set), op0)))
         break;

      /* If this is setting OP0, get what it sets it to if it looks
      relevant.  */
      if (set){
         machine_mode inner_mode = GET_MODE (SET_DEST (set));
         //#ifdef FLOAT_STORE_FLAG_VALUE
         REAL_VALUE_TYPE fsfv;
        // #endif

         /* ??? We may not combine comparisons done in a CCmode with
         comparisons not done in a CCmode.  This is to aid targets
         like Alpha that have an IEEE compliant EQ instruction, and
         a non-IEEE compliant BEQ instruction.  The use of CCmode is
         actually artificial, simply to prevent the combination, but
         should not affect other platforms.

         However, we must allow VOIDmode comparisons to match either
         CCmode or non-CCmode comparison, because some ports have
         modeless comparisons inside branch patterns.

         ??? This mode check should perhaps look more like the mode check
         in simplify_comparison in combine.  */
         if (((mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) == MODE_CC)
         != (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,inner_mode) == MODE_CC))
         && mode != VOIDmode   && inner_mode != VOIDmode)
            break;

         bool cond=true;
         bool cond1=true;

         if(mtcs_config_ifdef(mtcsConfig,MTCS_FLOAT_STORE_FLAG_VALUE)){
            cond=(code == LT && mtcs_mode_is_scalar_float_p/*!SCALAR_FLOAT_MODE_P*/(mtcsMode,inner_mode)
              && (fsfv = mtcs_real_float_store_flag_value/*!FLOAT_STORE_FLAG_VALUE*/(mtcsReal,inner_mode), REAL_VALUE_NEGATIVE (fsfv)));
            cond1=(code == GE && mtcs_mode_is_scalar_float_p/*!SCALAR_FLOAT_MODE_P*/(mtcsMode,inner_mode)
              && (fsfv = mtcs_real_float_store_flag_value/*!FLOAT_STORE_FLAG_VALUE*/(mtcsReal,inner_mode), REAL_VALUE_NEGATIVE (fsfv)));
         }

         if (GET_CODE (SET_SRC (set)) == COMPARE
         || (((code == NE || (code == LT
         && mtcs_simplify_rtx_val_signbit_known_set_p/*!val_signbit_known_set_p*/(mtcsSimplifyRtx,inner_mode, STORE_FLAG_VALUE))
         || cond
         /*
         #ifdef FLOAT_STORE_FLAG_VALUE
         || (code == LT
         && SCALAR_FLOAT_MODE_P (inner_mode)
         && (fsfv = FLOAT_STORE_FLAG_VALUE (inner_mode),
         REAL_VALUE_NEGATIVE (fsfv)))
         #endif
         */
         ))
         && COMPARISON_P (SET_SRC (set))))
            x = SET_SRC (set);
         else if (((code == EQ  || (code == GE
          && mtcs_simplify_rtx_val_signbit_known_set_p/*!val_signbit_known_set_p*/(mtcsSimplifyRtx,inner_mode, STORE_FLAG_VALUE)) || cond1
         /*!
         #ifdef FLOAT_STORE_FLAG_VALUE
         || (code == GE
         && SCALAR_FLOAT_MODE_P (inner_mode)
         && (fsfv = FLOAT_STORE_FLAG_VALUE (inner_mode),
         REAL_VALUE_NEGATIVE (fsfv)))
         #endif
         */
         ))
         && COMPARISON_P (SET_SRC (set))){
            reverse_code = 1;
            x = SET_SRC (set);
         } else if ((code == EQ || code == NE)  && GET_CODE (SET_SRC (set)) == XOR)
            /* Handle sequences like:

            (set op0 (xor X Y))
            ...(eq|ne op0 (const_int 0))...

            in which case:

            (eq op0 (const_int 0)) reduces to (eq X Y)
            (ne op0 (const_int 0)) reduces to (ne X Y)

            This is the form used by MIPS16, for example.  */
            x = SET_SRC (set);
         else
            break;
      }else if (mtcs_rtlanal_reg_set_p/*!reg_set_p*/(self,op0, prev))
         /* If this sets OP0, but not directly, we have to give up.  */
         break;

      if (x){
         /* If the caller is expecting the condition to be valid at INSN,
         make sure X doesn't change before INSN.  */
         if (valid_at_insn_p)
            if (modified_in_p (x, prev) || mtcs_rtlanal_modified_between_p/*!modified_between_p*/(self,x, prev, insn))
               break;
         if (COMPARISON_P (x))
            code = GET_CODE (x);
         if (reverse_code){
            code = mtcs_dojump_reversed_comparison_code/*!reversed_comparison_code*/(mtcsDojump,x, prev);
            if (code == UNKNOWN)
               return 0;
            reverse_code = 0;
         }

         op0 = XEXP (x, 0), op1 = XEXP (x, 1);
         if (earliest)
            *earliest = prev;
      }
   }

   /* If constant is first, put it last.  */
   if (CONSTANT_P (op0))
      code = swap_condition (code), tem = op0, op0 = op1, op1 = tem;

   /* If OP0 is the result of a comparison, we weren't able to find what
   was really being compared, so fail.  */
   if (!allow_cc_mode  && mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,GET_MODE (op0)) == MODE_CC)
      return 0;

   /* Canonicalize any ordered comparison with integers involving equality
   if we can do computations in the relevant mode and we do not
   overflow.  */

   scalar_int_mode op0_mode;
   if (CONST_INT_P (op1)
   && mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (op0), &op0_mode)
   && mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,op0_mode) <= HOST_BITS_PER_WIDE_INT){
      HOST_WIDE_INT const_val = INTVAL (op1);
      unsigned HOST_WIDE_INT uconst_val = const_val;
      unsigned HOST_WIDE_INT max_val = (unsigned HOST_WIDE_INT) mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,op0_mode);

      switch (code){
         case LE:
            if ((unsigned HOST_WIDE_INT) const_val != max_val >> 1)
               code = LT, op1 = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,const_val + 1, op0_mode);
            break;

         /* When cross-compiling, const_val might be sign-extended from
         BITS_PER_WORD to HOST_BITS_PER_WIDE_INT */
         case GE:
            if ((const_val & max_val)
            != (HOST_WIDE_INT_1U << (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,op0_mode) - 1)))
               code = GT, op1 = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,const_val - 1, op0_mode);
            break;

         case LEU:
            if (uconst_val < max_val)
               code = LTU, op1 = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,uconst_val + 1, op0_mode);
            break;

         case GEU:
            if (uconst_val != 0)
               code = GTU, op1 = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,uconst_val - 1, op0_mode);
            break;

         default:
            break;
      }
   }

   /* We promised to return a comparison.  */
   rtx ret = gen_rtx_fmt_ee (code, VOIDmode, op0, op1);
   if (COMPARISON_P (ret))
      return ret;
   return 0;
}


/* Given a jump insn JUMP, return the condition that will cause it to branch
   to its JUMP_LABEL.  If the condition cannot be understood, or is an
   inequality floating-point comparison which needs to be reversed, 0 will
   be returned.

   If EARLIEST is nonzero, it is a pointer to a place where the earliest
   insn used in locating the condition was found.  If a replacement test
   of the condition is desired, it should be placed in front of that
   insn and we will be sure that the inputs are still valid.  If EARLIEST
   is null, the returned condition will be valid at INSN.

   If ALLOW_CC_MODE is nonzero, allow the condition returned to be a
   compare CC mode register.

   VALID_AT_INSN_P is the same as for canonicalize_condition.  */
//原型 get_condition rtl.h rtlanal.cc
rtx mtcs_rtlanal_get_condition ( MtcsRtlanal *self,rtx_insn *jump, rtx_insn **earliest, int allow_cc_mode,
          int valid_at_insn_p)
{
   rtx cond;
   int reverse;
   rtx set;
   /* If this is not a standard conditional jump, we can't parse it.  */
   if (!JUMP_P (jump) || ! any_condjump_p (jump))
      return 0;
   set = pc_set (jump);
   cond = XEXP (SET_SRC (set), 0);
   /* If this branches to JUMP_LABEL when the condition is false, reverse
   the condition.  */
   reverse  = GET_CODE (XEXP (SET_SRC (set), 2)) == LABEL_REF
         && label_ref_label (XEXP (SET_SRC (set), 2)) == JUMP_LABEL (jump);

   return mtcs_rtlanal_canonicalize_condition/*!canonicalize_condition*/(self,
         jump, cond, reverse, earliest, NULL_RTX, allow_cc_mode, valid_at_insn_p);
}

/* Calculate the rtx_cost of a single instruction pattern.  A return value of
   zero indicates an instruction pattern without a known cost.  */
//原型 pattern_cost rtl.h rtlanal.cc
int mtcs_rtlanal_pattern_cost (MtcsRtlanal *self,rtx pat, bool speed)
{
   int i, cost;
   rtx set;

   /* Extract the single set rtx from the instruction pattern.  We
   can't use single_set since we only have the pattern.  We also
   consider PARALLELs of a normal set and a single comparison.  In
   that case we use the cost of the non-comparison SET operation,
   which is most-likely to be the real cost of this operation.  */
   if (GET_CODE (pat) == SET)
      set = pat;
   else if (GET_CODE (pat) == PARALLEL){
      set = NULL_RTX;
      rtx comparison = NULL_RTX;

      for (i = 0; i < XVECLEN (pat, 0); i++){
         rtx x = XVECEXP (pat, 0, i);
         if (GET_CODE (x) == SET){
            if (GET_CODE (SET_SRC (x)) == COMPARE){
               if (comparison)
                  return 0;
               comparison = x;
            }else{
               if (set)
                  return 0;
               set = x;
            }
         }
      }

      if (!set && comparison)
         set = comparison;

      if (!set)
         return 0;
   }else
      return 0;

   cost = mtcs_rtlanal_set_src_cost/*!set_src_cost*/(self,SET_SRC (set), GET_MODE (SET_DEST (set)), speed);
   return cost > 0 ? cost : COSTS_N_INSNS (1);
}

/* Return true if an insn consists only of SETs, each of which only sets a
   value to itself.  */
//原型 noop_move_p rtl.h rtlanal.cc
bool mtcs_rtlanal_noop_move_p (MtcsRtlanal *self,const rtx_insn *insn)
{
   rtx pat = PATTERN (insn);
   if (INSN_CODE (insn) == NOOP_MOVE_INSN_CODE)
      return true;
   /* Check the code to be executed for COND_EXEC.  */
   if (GET_CODE (pat) == COND_EXEC)
      pat = COND_EXEC_CODE (pat);
   if (GET_CODE (pat) == SET && mtcs_rtlanal_set_noop_p/*!set_noop_p*/(self,pat))
      return true;
   if (GET_CODE (pat) == PARALLEL){
      int i;
      /* If nothing but SETs of registers to themselves,
      this insn can also be deleted.  */
      for (i = 0; i < XVECLEN (pat, 0); i++){
         rtx tem = XVECEXP (pat, 0, i);
         if (GET_CODE (tem) == USE || GET_CODE (tem) == CLOBBER)
            continue;
         if (GET_CODE (tem) != SET || ! mtcs_rtlanal_set_noop_p/*!set_noop_p*/(self,tem))
            return false;
      }
      return true;
   }
   return false;
}

/* If M is a bitmask that selects a field of low-order bits within an item but
   not the entire word, return the length of the field.  Return -1 otherwise.
   M is used in machine mode MODE.  */
//原型 low_bitmask_len rtl.h rtlanal.cc
int mtcs_rtlanal_low_bitmask_len (MtcsRtlanal *self,machine_mode mode, unsigned HOST_WIDE_INT m)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   if (mode != VOIDmode){
      if (!mtcs_mode_is_hwi_computable_p/*!HWI_COMPUTABLE_MODE_P*/(mtcsMode,mode))
         return -1;
      m &= mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,mode);
   }

   return exact_log2 (m + 1);
}

static unsigned int cached_num_sign_bit_copies (MtcsRtlanal *self,const_rtx, scalar_int_mode,
                  const_rtx, machine_mode,  unsigned int);
static unsigned int num_sign_bit_copies1 (MtcsRtlanal *self,const_rtx, scalar_int_mode,
                 const_rtx, machine_mode, unsigned int);

/* Return true if num_sign_bit_copies1 might recurse into both operands
   of X.  */

static inline bool num_sign_bit_copies_binary_arith_p (const_rtx x)
{
   if (!ARITHMETIC_P (x))
      return false;
   switch (GET_CODE (x)){
      case IOR:
      case AND:
      case XOR:
      case SMIN:
      case SMAX:
      case UMIN:
      case UMAX:
      case PLUS:
      case MINUS:
      case MULT:
         return true;
      default:
         return false;
   }
}

/* The function cached_num_sign_bit_copies is a wrapper around
   num_sign_bit_copies1.  It avoids exponential behavior in
   num_sign_bit_copies1 when X has identical subexpressions on the
   first or the second level.  */
//原型 cached_num_sign_bit_copies rtlanal.cc
static unsigned int cached_num_sign_bit_copies (MtcsRtlanal *self,const_rtx x, scalar_int_mode mode,
             const_rtx known_x, machine_mode known_mode,  unsigned int known_ret)
{
   if (x == known_x && mode == known_mode)
      return known_ret;

   /* Try to find identical subexpressions.  If found call
   num_sign_bit_copies1 on X with the subexpressions as KNOWN_X and
   the precomputed value for the subexpression as KNOWN_RET.  */

   if (num_sign_bit_copies_binary_arith_p (x)){
      rtx x0 = XEXP (x, 0);
      rtx x1 = XEXP (x, 1);

      /* Check the first level.  */
      if (x0 == x1)
         return
      num_sign_bit_copies1(self,x, mode, x0, mode,cached_num_sign_bit_copies(self,x0, mode, known_x,known_mode, known_ret));

      /* Check the second level.  */
      if (num_sign_bit_copies_binary_arith_p (x0) && (x1 == XEXP (x0, 0) || x1 == XEXP (x0, 1)))
         return
      num_sign_bit_copies1(self,x, mode, x1, mode, cached_num_sign_bit_copies(self,x1, mode, known_x, known_mode, known_ret));

      if (num_sign_bit_copies_binary_arith_p (x1)  && (x0 == XEXP (x1, 0) || x0 == XEXP (x1, 1)))
         return
      num_sign_bit_copies1(self,x, mode, x0, mode,cached_num_sign_bit_copies(self,x0, mode, known_x,known_mode, known_ret));
   }

   return num_sign_bit_copies1(self,x, mode, known_x, known_mode, known_ret);
}


/* Return the number of bits at the high-order end of X that are known to
   be equal to the sign bit.  X will be used in mode MODE.  The returned
   value will always be between 1 and the number of bits in MODE.  */
//原型 num_sign_bit_copies1 rtlanal.cc
static unsigned int num_sign_bit_copies1 (MtcsRtlanal *self,const_rtx x, scalar_int_mode mode, const_rtx known_x,
            machine_mode known_mode,
            unsigned int known_ret)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsReg *mtcsReg = mtcs_target_get_reg(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   machine_mode pMode= mtcs_mode_get_Pmode(mtcsMode);

   enum rtx_code code = GET_CODE (x);
   unsigned int bitwidth = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode);
   int num0, num1, result;
   unsigned HOST_WIDE_INT nonzero;

   if (CONST_INT_P (x)){
      /* If the constant is negative, take its 1's complement and remask.
      Then see how many zero bits we have.  */
      nonzero = UINTVAL (x) & mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,mode);
      if (bitwidth <= HOST_BITS_PER_WIDE_INT  && (nonzero & (HOST_WIDE_INT_1U << (bitwidth - 1))) != 0)
         nonzero = (~nonzero) & mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,mode);

      return (nonzero == 0 ? bitwidth : bitwidth - floor_log2 (nonzero) - 1);
   }

   scalar_int_mode xmode, inner_mode;
   if (!mtcs_mode_is_a <scalar_int_mode>(mtcsMode,GET_MODE (x), &xmode))
      return 1;

   unsigned int xmode_width = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,xmode);

   /* For a smaller mode, just ignore the high bits.  */
   if (bitwidth < xmode_width){
      num0 = cached_num_sign_bit_copies(self,x, xmode,known_x, known_mode, known_ret);
      return MAX (1, num0 - (int) (xmode_width - bitwidth));
   }

   if (bitwidth > xmode_width){
      /* If this machine does not do all register operations on the entire
      register and MODE is wider than the mode of X, we can say nothing
      at all about the high-order bits.  We extend this reasoning to RISC
      machines for operations that might not operate on full registers.  */
      if (!(mtcs_reg_get_word_register_operations/*!WORD_REGISTER_OPERATIONS*/(mtcsReg) && word_register_operation_p (x)))
         return 1;

      /* Likewise on machines that do, if the mode of the object is smaller
      than a word and loads of that size don't sign extend, we can say
      nothing about the high order bits.  */
      if (xmode_width < BITS_PER_WORD && mtcs_rtl_load_extend_op/*!load_extend_op*/(mtcsRTL,xmode) != SIGN_EXTEND)
         return 1;
   }

   /* Please keep num_sign_bit_copies_binary_arith_p above in sync with
   the code in the switch below.  */
   switch (code){
      case REG:
         if(mtcs_config_ifdefine(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED)){
//#if defined(POINTERS_EXTEND_UNSIGNED)
            /* If pointers extend signed and this is a pointer in Pmode, say that
            all the bits above ptr_mode are known to be sign bit copies.  */
            /* As we do not know which address space the pointer is referring to,
            we can do this only if the target does not support different pointer
            or address modes depending on the address space.  */
            if (mtcs_target_target_default_pointer_address_modes_p/*!target_default_pointer_address_modes_p*/(mtcsTarget)
            && ! mtcs_config_get_value/*!POINTERS_EXTEND_UNSIGNED*/(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED) && xmode == pMode
            && mode == pMode && REG_POINTER (x)
            && !target_rtx_have_ptr_extend/*!targetm.have_ptr_extend*/(mtcsMachine->tmrtx))
            return mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,pMode)
                        - mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,ptr_mode) + 1;
//#endif
         }
         {
            unsigned int copies_for_hook = 1, copies = 1;
            rtx new_rtx = rtl_hooks.reg_num_sign_bit_copies (x, xmode, mode,&copies_for_hook);

            if (new_rtx)
               copies = cached_num_sign_bit_copies(self,new_rtx, mode, known_x,known_mode, known_ret);

            if (copies > 1 || copies_for_hook > 1)
               return MAX (copies, copies_for_hook);

            /* Else, use nonzero_bits to guess num_sign_bit_copies (see below).  */
         }
         break;

      case MEM:
         /* Some RISC machines sign-extend all loads of smaller than a word.  */
         if (mtcs_rtl_load_extend_op/*!load_extend_op*/(mtcsRTL,xmode) == SIGN_EXTEND)
            return MAX (1, ((int) bitwidth - (int) xmode_width + 1));
         break;

      case SUBREG:
         /* If this is a SUBREG for a promoted object that is sign-extended
         and we are looking at it in a wider mode, we know that at least the
         high-order bits are known to be sign bit copies.  */

         if (SUBREG_PROMOTED_VAR_P (x) && SUBREG_PROMOTED_SIGNED_P (x)){
            num0 = cached_num_sign_bit_copies(self,SUBREG_REG (x), mode,known_x, known_mode, known_ret);
            return MAX ((int) bitwidth - (int) xmode_width + 1, num0);
         }

         if (mtcs_mode_is_a <scalar_int_mode>(mtcsMode,GET_MODE (SUBREG_REG (x)), &inner_mode)){
            /* For a smaller object, just ignore the high bits.  */
            if (bitwidth <= mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,inner_mode)){
               num0 = cached_num_sign_bit_copies(self,SUBREG_REG (x), inner_mode, known_x, known_mode,known_ret);
               return MAX (1, num0 - (int) (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,inner_mode) - bitwidth));
            }

            /* For paradoxical SUBREGs on machines where all register operations
            affect the entire register, just look inside.  Note that we are
            passing MODE to the recursive call, so the number of sign bit
            copies will remain relative to that mode, not the inner mode.

            This works only if loads sign extend.  Otherwise, if we get a
            reload for the inner part, it may be loaded from the stack, and
            then we lose all sign bit copies that existed before the store
            to the stack.  */
            if (mtcs_reg_get_word_register_operations/*!WORD_REGISTER_OPERATIONS*/(mtcsReg)
            && mtcs_rtl_load_extend_op/*!load_extend_op*/(mtcsRTL,inner_mode) == SIGN_EXTEND
            && mtcs_rtl_paradoxical_subreg_p/*!paradoxical_subreg_p*/(mtcsRTL,x)
            && MEM_P (SUBREG_REG (x)))
               return cached_num_sign_bit_copies(self,SUBREG_REG (x), mode, known_x, known_mode, known_ret);
         }
         break;

      case SIGN_EXTRACT:
         if (CONST_INT_P (XEXP (x, 1)))
            return MAX (1, (int) bitwidth - INTVAL (XEXP (x, 1)));
         break;

      case SIGN_EXTEND:
         if (mtcs_mode_is_a <scalar_int_mode>(mtcsMode,GET_MODE (XEXP (x, 0)), &inner_mode))
            return (bitwidth - mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,inner_mode)
                     + cached_num_sign_bit_copies(self,XEXP (x, 0), inner_mode,known_x, known_mode, known_ret));
         break;

      case TRUNCATE:
         /* For a smaller object, just ignore the high bits.  */
         inner_mode = mtcs_mode_as_a <scalar_int_mode>(mtcsMode,GET_MODE (XEXP (x, 0)));
         num0 = cached_num_sign_bit_copies(self,XEXP (x, 0), inner_mode, known_x, known_mode, known_ret);
         return MAX (1, (num0 - (int) (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,inner_mode)  - bitwidth)));

      case NOT:
         return cached_num_sign_bit_copies(self,XEXP (x, 0), mode, known_x, known_mode, known_ret);

      case ROTATE:       case ROTATERT:
         /* If we are rotating left by a number of bits less than the number
         of sign bit copies, we can just subtract that amount from the
         number.  */
         if (CONST_INT_P (XEXP (x, 1))  && INTVAL (XEXP (x, 1)) >= 0 && INTVAL (XEXP (x, 1)) < (int) bitwidth){
            num0 = cached_num_sign_bit_copies(self,XEXP (x, 0), mode, known_x, known_mode, known_ret);
            return MAX (1, num0 - (code == ROTATE ? INTVAL (XEXP (x, 1)) : (int) bitwidth - INTVAL (XEXP (x, 1))));
         }
         break;

      case NEG:
         /* In general, this subtracts one sign bit copy.  But if the value
         is known to be positive, the number of sign bit copies is the
         same as that of the input.  Finally, if the input has just one bit
         that might be nonzero, all the bits are copies of the sign bit.  */
         num0 = cached_num_sign_bit_copies(self,XEXP (x, 0), mode,known_x, known_mode, known_ret);
         if (bitwidth > HOST_BITS_PER_WIDE_INT)
            return num0 > 1 ? num0 - 1 : 1;

         nonzero = mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(self,XEXP (x, 0), mode);
         if (nonzero == 1)
            return bitwidth;

         if (num0 > 1  && ((HOST_WIDE_INT_1U << (bitwidth - 1)) & nonzero))
            num0--;

         return num0;

      case IOR:   case AND:   case XOR:
      case SMIN:  case SMAX:  case UMIN:  case UMAX:
         /* Logical operations will preserve the number of sign-bit copies.
         MIN and MAX operations always return one of the operands.  */
         num0 = cached_num_sign_bit_copies(self,XEXP (x, 0), mode, known_x, known_mode, known_ret);
         num1 = cached_num_sign_bit_copies(self,XEXP (x, 1), mode, known_x, known_mode, known_ret);

         /* If num1 is clearing some of the top bits then regardless of
         the other term, we are guaranteed to have at least that many
         high-order zero bits.  */
         if (code == AND  && num1 > 1  && bitwidth <= HOST_BITS_PER_WIDE_INT   && CONST_INT_P (XEXP (x, 1))
         && (UINTVAL (XEXP (x, 1))  & (HOST_WIDE_INT_1U << (bitwidth - 1))) == 0)
            return num1;

         /* Similarly for IOR when setting high-order bits.  */
         if (code == IOR  && num1 > 1  && bitwidth <= HOST_BITS_PER_WIDE_INT
         && CONST_INT_P (XEXP (x, 1))  && (UINTVAL (XEXP (x, 1))  & (HOST_WIDE_INT_1U << (bitwidth - 1))) != 0)
            return num1;

         return MIN (num0, num1);

      case PLUS:  case MINUS:
         /* For addition and subtraction, we can have a 1-bit carry.  However,
         if we are subtracting 1 from a positive number, there will not
         be such a carry.  Furthermore, if the positive number is known to
         be 0 or 1, we know the result is either -1 or 0.  */

         if (code == PLUS && XEXP (x, 1) == constm1_rtx  && bitwidth <= HOST_BITS_PER_WIDE_INT){
            nonzero = mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(self,XEXP (x, 0), mode);
            if (((HOST_WIDE_INT_1U << (bitwidth - 1)) & nonzero) == 0)
               return (nonzero == 1 || nonzero == 0 ? bitwidth : bitwidth - floor_log2 (nonzero) - 1);
         }

         num0 = cached_num_sign_bit_copies(self,XEXP (x, 0), mode, known_x, known_mode, known_ret);
         num1 = cached_num_sign_bit_copies(self,XEXP (x, 1), mode, known_x, known_mode, known_ret);
         result = MAX (1, MIN (num0, num1) - 1);

         return result;

      case MULT:
         /* The number of bits of the product is the sum of the number of
         bits of both terms.  However, unless one of the terms if known
         to be positive, we must allow for an additional bit since negating
         a negative number can remove one sign bit copy.  */

         num0 = cached_num_sign_bit_copies(self,XEXP (x, 0), mode,known_x, known_mode, known_ret);
         num1 = cached_num_sign_bit_copies(self,XEXP (x, 1), mode,known_x, known_mode, known_ret);

         result = bitwidth - (bitwidth - num0) - (bitwidth - num1);
         if (result > 0  && (bitwidth > HOST_BITS_PER_WIDE_INT
         || (((mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(self,XEXP (x, 0), mode) & (HOST_WIDE_INT_1U << (bitwidth - 1))) != 0)
         && ((mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(self,XEXP (x, 1), mode)  & (HOST_WIDE_INT_1U << (bitwidth - 1)))  != 0))))
            result--;

         return MAX (1, result);

      case UDIV:
         /* The result must be <= the first operand.  If the first operand
         has the high bit set, we know nothing about the number of sign
         bit copies.  */
         if (bitwidth > HOST_BITS_PER_WIDE_INT)
            return 1;
         else if ((mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(self,XEXP (x, 0), mode) & (HOST_WIDE_INT_1U << (bitwidth - 1))) != 0)
            return 1;
         else
            return cached_num_sign_bit_copies(self,XEXP (x, 0), mode, known_x, known_mode, known_ret);

      case UMOD:
         /* The result must be <= the second operand.  If the second operand
         has (or just might have) the high bit set, we know nothing about
         the number of sign bit copies.  */
         if (bitwidth > HOST_BITS_PER_WIDE_INT)
            return 1;
         else if ((mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(self,XEXP (x, 1), mode) & (HOST_WIDE_INT_1U << (bitwidth - 1))) != 0)
            return 1;
         else
            return cached_num_sign_bit_copies(self,XEXP (x, 1), mode,known_x, known_mode, known_ret);

      case DIV:
         /* Similar to unsigned division, except that we have to worry about
         the case where the divisor is negative, in which case we have
         to add 1.  */
         result = cached_num_sign_bit_copies(self,XEXP (x, 0), mode,known_x, known_mode, known_ret);
         if (result > 1
         && (bitwidth > HOST_BITS_PER_WIDE_INT || (mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(self,XEXP (x, 1), mode) & (HOST_WIDE_INT_1U << (bitwidth - 1))) != 0))
            result--;

         return result;

      case MOD:
         result = cached_num_sign_bit_copies(self,XEXP (x, 1), mode,known_x, known_mode, known_ret);
         if (result > 1  && (bitwidth > HOST_BITS_PER_WIDE_INT
               || (mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(self,XEXP (x, 1), mode) & (HOST_WIDE_INT_1U << (bitwidth - 1))) != 0))
            result--;

         return result;

      case ASHIFTRT:
         /* Shifts by a constant add to the number of bits equal to the
         sign bit.  */
         num0 = cached_num_sign_bit_copies(self,XEXP (x, 0), mode,known_x, known_mode, known_ret);
         if (CONST_INT_P (XEXP (x, 1)) && INTVAL (XEXP (x, 1)) > 0  && INTVAL (XEXP (x, 1)) < xmode_width)
            num0 = MIN ((int) bitwidth, num0 + INTVAL (XEXP (x, 1)));

         return num0;

      case ASHIFT:
         /* Left shifts destroy copies.  */
         if (!CONST_INT_P (XEXP (x, 1))
         || INTVAL (XEXP (x, 1)) < 0
         || INTVAL (XEXP (x, 1)) >= (int) bitwidth
         || INTVAL (XEXP (x, 1)) >= xmode_width)
            return 1;

         num0 = cached_num_sign_bit_copies(self,XEXP (x, 0), mode, known_x, known_mode, known_ret);
         return MAX (1, num0 - INTVAL (XEXP (x, 1)));

      case IF_THEN_ELSE:
         num0 = cached_num_sign_bit_copies(self,XEXP (x, 1), mode,known_x, known_mode, known_ret);
         num1 = cached_num_sign_bit_copies(self,XEXP (x, 2), mode,known_x, known_mode, known_ret);
         return MIN (num0, num1);

      case EQ:  case NE:  case GE:  case GT:  case LE:  case LT:
      case UNEQ:  case LTGT:  case UNGE:  case UNGT:  case UNLE:  case UNLT:
      case GEU: case GTU: case LEU: case LTU:
      case UNORDERED: case ORDERED:
         /* If the constant is negative, take its 1's complement and remask.
         Then see how many zero bits we have.  */
         nonzero = STORE_FLAG_VALUE;
         if (bitwidth <= HOST_BITS_PER_WIDE_INT  && (nonzero & (HOST_WIDE_INT_1U << (bitwidth - 1))) != 0)
            nonzero = (~nonzero) & mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,mode);

         return (nonzero == 0 ? bitwidth : bitwidth - floor_log2 (nonzero) - 1);

      default:
         break;
   }

   /* If we haven't been able to figure it out by one of the above rules,
   see if some of the high-order bits are known to be zero.  If so,
   count those bits and return one less than that amount.  If we can't
   safely compute the mask for this mode, always return BITWIDTH.  */

   bitwidth = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode);
   if (bitwidth > HOST_BITS_PER_WIDE_INT)
      return 1;

   nonzero = mtcs_rtlanal_nonzero_bits/*!nonzero_bits*/(self,x, mode);
   return nonzero & (HOST_WIDE_INT_1U << (bitwidth - 1)) ? 1 : bitwidth - floor_log2 (nonzero) - 1;
}

//原型 num_sign_bit_copies rtl.h rtlanal.cc
unsigned int mtcs_rtlanal_num_sign_bit_copies (MtcsRtlanal *self,const_rtx x, machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   if (mode == VOIDmode)
      mode = GET_MODE (x);
   scalar_int_mode int_mode;
   if (!mtcs_mode_is_a <scalar_int_mode>(mtcsMode,mode, &int_mode))
      return 1;
   return cached_num_sign_bit_copies (self,x, int_mode, NULL_RTX, VOIDmode, 0);
}

/* Remove register number REGNO from the dead registers list of INSN.

   Return the note used to record the death, if there was one.  */
//原型 remove_death rtl.h combine.cc
rtx mtcs_rtlanal_remove_death (MtcsRtlanal *self,unsigned int regno, rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   rtx note = find_regno_note (insn, REG_DEAD, regno);
   if (note)
      mtcs_rtlanal_remove_note/*!remove_note*/(self,insn, note);
   return note;
}

/* Fill in the structure C with information about both speed and size rtx
   costs for X, which is operand OPNO in an expression with code OUTER.  */
//原型 get_full_rtx_cost rtl.h rtlanal.cc
void mtcs_rtlanal_get_full_rtx_cost (MtcsRtlanal *self,rtx x, machine_mode mode, enum rtx_code outer, int opno,
         struct full_rtx_costs *c)
{
  c->speed = mtcs_rtlanal_rtx_cost/*!rtx_cost*/(self,x, mode, outer, opno, true);
  c->size = mtcs_rtlanal_rtx_cost/*!rtx_cost*/(self,x, mode, outer, opno, false);
}

/* Like set_rtx_cost, but return both the speed and size costs in C.  */
//原型 get_full_set_rtx_cost rtl.h
void mtcs_rtlanal_get_full_set_rtx_cost (MtcsRtlanal *self,rtx x, struct full_rtx_costs *c)
{
   mtcs_rtlanal_get_full_rtx_cost/*!get_full_rtx_cost*/(self,x, VOIDmode, INSN, 4, c);
}

//原型 get_full_set_src_cost rtl.h
void mtcs_rtlanal_get_full_set_src_cost (MtcsRtlanal *self,rtx x, machine_mode mode, struct full_rtx_costs *c)
{
   mtcs_rtlanal_get_full_rtx_cost/*!get_full_rtx_cost*/(self,x, mode, SET, 1, c);
}

/* Replace any occurrence of FROM in X with TO.  The function does
   not enter into CONST_DOUBLE for the replace.

   Note that copying is not done so X must not be shared unless all copies
   are to be modified.

   ALL_REGS is true if we want to replace all REGs equal to FROM, not just
   those pointer-equal ones.  */
//原型 replace_rtx rtl.h rtlanal.cc
rtx mtcs_rtlanal_replace_rtx (MtcsRtlanal *self,rtx x, rtx from, rtx to, bool all_regs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

   int i, j;
   const char *fmt;

   if (x == from)
      return to;

   /* Allow this function to make replacements in EXPR_LISTs.  */
   if (x == 0)
      return 0;

   if (all_regs
   && REG_P (x)
   && REG_P (from)
   && REGNO (x) == REGNO (from)){
   gcc_assert (GET_MODE (x) == GET_MODE (from));
      return to;
   }else if (GET_CODE (x) == SUBREG){
      rtx new_rtx = mtcs_rtlanal_replace_rtx/*!replace_rtx*/(self,SUBREG_REG (x), from, to, all_regs);

      if (CONST_SCALAR_INT_P (new_rtx)){
         x = mtcs_simplify_rtx_subreg/*!simplify_subreg*/(mtcsSimplifyRtx,
               GET_MODE (x), new_rtx, GET_MODE (SUBREG_REG (x)), SUBREG_BYTE (x));
         gcc_assert (x);
      }else
         SUBREG_REG (x) = new_rtx;

      return x;
   }else if (GET_CODE (x) == ZERO_EXTEND){
      rtx new_rtx = mtcs_rtlanal_replace_rtx/*!replace_rtx*/(self,XEXP (x, 0), from, to, all_regs);

      if (CONST_SCALAR_INT_P (new_rtx)){
         x = mtcs_simplify_rtx_unary_operation/*!simplify_unary_operation*/(mtcsSimplifyRtx,
               ZERO_EXTEND, GET_MODE (x),new_rtx, GET_MODE (XEXP (x, 0)));
         gcc_assert (x);
      }else
         XEXP (x, 0) = new_rtx;

      return x;
   }

   fmt = GET_RTX_FORMAT (GET_CODE (x));
   for (i = GET_RTX_LENGTH (GET_CODE (x)) - 1; i >= 0; i--){
      if (fmt[i] == 'e')
         XEXP (x, i) = mtcs_rtlanal_replace_rtx/*!replace_rtx*/(self,XEXP (x, i), from, to, all_regs);
      else if (fmt[i] == 'E')
         for (j = XVECLEN (x, i) - 1; j >= 0; j--)
            XVECEXP (x, i, j) = mtcs_rtlanal_replace_rtx/*!replace_rtx*/(self,XVECEXP (x, i, j),from, to, all_regs);
   }

   return x;
}

/* Replace occurrences of the OLD_LABEL in *LOC with NEW_LABEL.  Also track
   the change in LABEL_NUSES if UPDATE_LABEL_NUSES.  */
//原型 replace_label rtl.h rtlanal.cc
void mtcs_rtlanal_replace_label (MtcsRtlanal *self,rtx *loc, rtx old_label, rtx new_label, bool update_label_nuses)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

   /* Handle jump tables specially, since ADDR_{DIFF_,}VECs can be long.  */
   rtx x = *loc;
   if (JUMP_TABLE_DATA_P (x)) {
      x = PATTERN (x);
      rtvec vec = XVEC (x, GET_CODE (x) == ADDR_DIFF_VEC);
      int len = GET_NUM_ELEM (vec);
      for (int i = 0; i < len; ++i){
         rtx ref = RTVEC_ELT (vec, i);
         if (XEXP (ref, 0) == old_label){
            XEXP (ref, 0) = new_label;
            if (update_label_nuses){
               ++LABEL_NUSES (new_label);
               --LABEL_NUSES (old_label);
            }
         }
      }
      return;
   }

   /* If this is a JUMP_INSN, then we also need to fix the JUMP_LABEL
   field.  This is not handled by the iterator because it doesn't
   handle unprinted ('0') fields.  */
   if (JUMP_P (x) && JUMP_LABEL (x) == old_label)
      JUMP_LABEL (x) = new_label;

   subrtx_ptr_iterator::array_type array;
   FOR_EACH_SUBRTX_PTR (iter, array, loc, ALL){
      rtx *loc = *iter;
      if (rtx x = *loc){
         if (GET_CODE (x) == SYMBOL_REF && CONSTANT_POOL_ADDRESS_P (x)){
            rtx c = get_pool_constant (x);
            if (rtx_referenced_p (old_label, c)){
               /* Create a copy of constant C; replace the label inside
               but do not update LABEL_NUSES because uses in constant pool
               are not counted.  */
               rtx new_c = copy_rtx (c);
               mtcs_rtlanal_replace_label/*!replace_label*/(self,&new_c, old_label, new_label, false);

               /* Add the new constant NEW_C to constant pool and replace
               the old reference to constant by new reference.  */
               rtx new_mem = mtcs_asm_force_const_mem/*!force_const_mem*/(mtcsAsm,get_pool_mode (x), new_c);
               *loc = mtcs_rtlanal_replace_rtx/*!replace_rtx*/(self,x, x, XEXP (new_mem, 0));
            }
         }

         if ((GET_CODE (x) == LABEL_REF || GET_CODE (x) == INSN_LIST) && XEXP (x, 0) == old_label){
            XEXP (x, 0) = new_label;
            if (update_label_nuses){
               ++LABEL_NUSES (new_label);
               --LABEL_NUSES (old_label);
            }
         }
      }
   }
}

//原型 replace_label_in_insn rtl.h rtlanal.cc
void mtcs_rtlanal_replace_label_in_insn (MtcsRtlanal *self,rtx_insn *insn, rtx_insn *old_label,
             rtx_insn *new_label, bool update_label_nuses)
{
   rtx insn_as_rtx = insn;
   mtcs_rtlanal_replace_label/*!replace_label*/(self,&insn_as_rtx, old_label, new_label, update_label_nuses);
   gcc_checking_assert (insn_as_rtx == insn);
}

/* Given a subreg X, return the bit offset where the subreg begins
   (counting from the least significant bit of the reg).  */
//原型 subreg_lsb rtl.h rtlanal.cc
poly_uint64 mtcs_rtlanal_subreg_lsb (MtcsRtlanal *self,const_rtx x)
{
   return subreg_lsb_1 (GET_MODE (x), GET_MODE (SUBREG_REG (x)), SUBREG_BYTE (x));
}

//原型 subreg_lsb_1 rtl.h
poly_uint64 mtcs_rtlanal_subreg_lsb_1 (MtcsRtlanal *self,machine_mode outer_mode, machine_mode inner_mode,
         poly_uint64 subreg_byte)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   return subreg_size_lsb (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,outer_mode),
         mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,inner_mode), subreg_byte);
}


/* We let num_sign_bit_copies recur into nonzero_bits as that is useful.
   We don't let nonzero_bits recur into num_sign_bit_copies, because that
   is less useful.  We can't allow both, because that results in exponential
   run time recursion.  There is a nullstone testcase that triggered
   this.  This macro avoids accidental uses of num_sign_bit_copies.  */
#define cached_num_sign_bit_copies sorry_i_am_preventing_exponential_behavior

/* Given an expression, X, compute which bits in X can be nonzero.
   We don't care about bits outside of those defined in MODE.

   For most X this is simply GET_MODE_MASK (GET_MODE (X)), but if X is
   an arithmetic operation, we can do better.  */

static unsigned HOST_WIDE_INT nonzero_bits1 (MtcsRtlanal *self,const_rtx x, scalar_int_mode mode, const_rtx known_x,
          machine_mode known_mode, unsigned HOST_WIDE_INT known_ret)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsReal *mtcsReal =mtcs_target_get_real(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   unsigned HOST_WIDE_INT nonzero = mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,mode);
   unsigned HOST_WIDE_INT inner_nz;
   enum rtx_code code = GET_CODE (x);
   machine_mode inner_mode;
   unsigned int inner_width;
   scalar_int_mode xmode;

   unsigned int mode_width = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode);


   n_debug("mtcsrtlanal.c  unit info HOST_BITS_PER_WIDE_INT :%d HOST_WIDE_INT_M1U:%d HOST_WIDE_INT_1U:%d\n",
         HOST_BITS_PER_WIDE_INT,HOST_WIDE_INT_M1U,HOST_WIDE_INT_1U);

   n_debug("mtcsrtlanal.c nonzero_bits1 00 code:%d %s mode:%d mode_width:%d nonzero:"HOST_WIDE_INT_PRINT_UNSIGNED"\n",
         code,GET_RTX_NAME(code),mode,mode_width,nonzero);

   if (CONST_INT_P (x)){
      if (SHORT_IMMEDIATES_SIGN_EXTEND
      && INTVAL (x) > 0
      && mode_width < BITS_PER_WORD
      && (UINTVAL (x) & (HOST_WIDE_INT_1U << (mode_width - 1))) != 0)
         return UINTVAL (x) | (HOST_WIDE_INT_M1U << mode_width);

      return UINTVAL (x);
   }
   n_debug("mtcsrtlanal.c nonzero_bits1 11 code:%d mode:%d\n",code,mode);

   if (!mtcs_mode_is_a <scalar_int_mode> (mtcsMode,GET_MODE (x), &xmode))
      return nonzero;
   unsigned int xmode_width = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,xmode);
   n_debug("mtcsrtlanal.c nonzero_bits1 22 code:%d mode:%d xmode:%d xmode_width:%d\n",code,mode,xmode,xmode_width);

   /* If X is wider than MODE, use its mode instead.  */
   if (xmode_width > mode_width){
      mode = xmode;
      nonzero = mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,mode);
      mode_width = xmode_width;
      n_debug("mtcsrtlanal.c nonzero_bits1 33 code:%d mode:%d xmode:%d xmode_width:%d\n",code,mode,xmode,xmode_width);

   }

   if (mode_width > HOST_BITS_PER_WIDE_INT)
      /* Our only callers in this case look for single bit values.  So
      just return the mode mask.  Those tests will then be false.  */
      return nonzero;
   n_debug("mtcsrtlanal.c nonzero_bits1 44 code:%d mode:%d xmode:%d xmode_width:%d\n",code,mode,xmode,xmode_width);

   /* If MODE is wider than X, but both are a single word for both the host
   and target machines, we can compute this from which bits of the object
   might be nonzero in its own mode, taking into account the fact that, on
   CISC machines, accessing an object in a wider mode generally causes the
   high-order bits to become undefined, so they are not known to be zero.
   We extend this reasoning to RISC machines for operations that might not
   operate on the full registers.  */
   if (mode_width > xmode_width
   && xmode_width <= BITS_PER_WORD
   && xmode_width <= HOST_BITS_PER_WIDE_INT
   && !(mtcs_reg_get_word_register_operations/*!WORD_REGISTER_OPERATIONS*/(mtcsReg) && word_register_operation_p (x)))
   {
      nonzero &= cached_nonzero_bits(self,x, xmode,known_x, known_mode, known_ret);
      nonzero |= mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,mode) & ~mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,xmode);
      return nonzero;
   }
   n_debug("mtcsrtlanal.c nonzero_bits1 55 code:%d mode:%d xmode:%d xmode_width:%d\n",code,mode,xmode,xmode_width);

   /* Please keep nonzero_bits_binary_arith_p above in sync with
   the code in the switch below.  */
   switch (code){
      case REG:
         if(mtcs_config_ifdefine(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED)){

            //#if defined(POINTERS_EXTEND_UNSIGNED)
            /* If pointers extend unsigned and this is a pointer in Pmode, say that
            all the bits above ptr_mode are known to be zero.  */
            /* As we do not know which address space the pointer is referring to,
            we can do this only if the target does not support different pointer
            or address modes depending on the address space.  */
            if (mtcs_target_target_default_pointer_address_modes_p/*!target_default_pointer_address_modes_p*/(mtcsTarget)
            && mtcs_config_get_value/*!POINTERS_EXTEND_UNSIGNED*/(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED)
            && xmode == mtcs_mode_get_Pmode(mtcsMode)
            && REG_POINTER (x)
            && !target_rtx_have_ptr_extend/*!targetm.have_ptr_extend*/(mtcsMachine->tmrtx))
            nonzero &= mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,ptr_mode);
            n_debug("mtcsrtlanal.c nonzero_bits1 66 code:%d mode:%d xmode:%d xmode_width:%d %ld ptr_mode:%d\n",
                  code,mode,xmode,xmode_width,nonzero,ptr_mode);
            //#endif
         }

         /* Include declared information about alignment of pointers.  */
         /* ??? We don't properly preserve REG_POINTER changes across
         pointer-to-integer casts, so we can't trust it except for
         things that we know must be pointers.  See execute/960116-1.c.  */
         if ((x == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)
         || x == mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL)
         || x == mtcs_rtl_get_arg_pointer_rtx/*!arg_pointer_rtx*/(mtcsRTL))
         && mtcs_rtl_data_get_regno_pointer_align/*!REGNO_POINTER_ALIGN*/(mtcsRtlData,REGNO (x))){
            unsigned HOST_WIDE_INT alignment = mtcs_rtl_data_get_regno_pointer_align/*!REGNO_POINTER_ALIGN*/(mtcsRtlData,
                  REGNO (x)) / BITS_PER_UNIT;
            n_debug("mtcsrtlanal.c nonzero_bits1 77 code:%d mode:%d xmode:%d xmode_width:%d alignment:%d\n",
                  code,mode,xmode,xmode_width,alignment);
            if(mtcs_config_ifdef(mtcsConfig,MTCS_PUSH_ROUNDING)){
               //#ifdef PUSH_ROUNDING
               /* If PUSH_ROUNDING is defined, it is possible for the
               stack to be momentarily aligned only to that amount,
               so we pick the least alignment.  */
               if (x == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)
               && target_calls_push_argument/*!targetm.calls.push_argument*/(mtcsMachine->calls,0)){
                  poly_uint64 rounded_1 = mtcs_mode_push_rounding/*!PUSH_ROUNDING*/(mtcsMode,poly_int64 (1));
                  alignment = MIN (known_alignment (rounded_1), alignment);
                  n_debug("mtcsrtlanal.c nonzero_bits1 88 code:%d mode:%d xmode:%d xmode_width:%d alignment:%d\n",
                            code,mode,xmode,xmode_width,alignment);
               }
               //#endif
            }

            nonzero &= ~(alignment - 1);
         }

         {
            unsigned HOST_WIDE_INT nonzero_for_hook = nonzero;
            //mtcsrtl  已把 reg_nonzero_bits 指向 regNonzeroBits_cb
            rtx new_rtx = rtl_hooks.reg_nonzero_bits (x, xmode, mode,&nonzero_for_hook);
            n_debug("mtcsrtlanal.c nonzero_bits1 99 code:%d mode:%d xmode:%d xmode_width:%d"
                  " nonzero_for_hook:"HOST_WIDE_INT_PRINT_UNSIGNED"new_rtx:%p "HOST_WIDE_INT_PRINT_UNSIGNED"\n",
                      code,mode,xmode,xmode_width,nonzero_for_hook,new_rtx,nonzero);
            if (new_rtx)
               nonzero_for_hook &= cached_nonzero_bits(self,new_rtx, mode, known_x, known_mode, known_ret);
            return nonzero_for_hook;
         }

      case MEM:
         /* In many, if not most, RISC machines, reading a byte from memory
         zeros the rest of the register.  Noticing that fact saves a lot
         of extra zero-extends.  */
         if (mtcs_rtl_load_extend_op/*!load_extend_op*/(mtcsRTL,xmode) == ZERO_EXTEND)
            nonzero &= mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,xmode);
         break;

      case EQ:  case NE:
      case UNEQ:  case LTGT:
      case GT:  case GTU:  case UNGT:
      case LT:  case LTU:  case UNLT:
      case GE:  case GEU:  case UNGE:
      case LE:  case LEU:  case UNLE:
      case UNORDERED: case ORDERED:
         /* If this produces an integer result, we know which bits are set.
         Code here used to clear bits outside the mode of X, but that is
         now done above.  */
         /* Mind that MODE is the mode the caller wants to look at this
         operation in, and not the actual operation mode.  We can wind
         up with (subreg:DI (gt:V4HI x y)), and we don't have anything
         that describes the results of a vector compare.  */
         n_debug("mtcsrtlanal.c nonzero_bits1 66 code:%d xmode:%d\n",code,xmode);
         if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,xmode) == MODE_INT
         && mode_width <= HOST_BITS_PER_WIDE_INT){
            nonzero = mtcs_real_get_store_flag_value/*!STORE_FLAG_VALUE*/(mtcsReal);
            n_debug("mtcsrtlanal.c nonzero_bits1 77 code:%d xmode:%d %ld\n",code,xmode,nonzero);

         }
         break;

      case NEG:
         #if 0
         /* Disabled to avoid exponential mutual recursion between nonzero_bits
         and num_sign_bit_copies.  */
         if (num_sign_bit_copies (XEXP (x, 0), xmode) == xmode_width)
            nonzero = 1;
         #endif

         if (xmode_width < mode_width)
            nonzero |= (mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,mode) & ~mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,xmode));
         break;

      case ABS:
         #if 0
         /* Disabled to avoid exponential mutual recursion between nonzero_bits
         and num_sign_bit_copies.  */
         if (num_sign_bit_copies (XEXP (x, 0), xmode) == xmode_width)
            nonzero = 1;
         #endif
         break;

      case TRUNCATE:
         nonzero &= (cached_nonzero_bits(self,XEXP (x, 0), mode, known_x, known_mode, known_ret)
               & mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,mode));
         break;

      case ZERO_EXTEND:
         nonzero &= cached_nonzero_bits(self,XEXP (x, 0), mode,known_x, known_mode, known_ret);
         if (GET_MODE (XEXP (x, 0)) != VOIDmode)
            nonzero &= mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,GET_MODE (XEXP (x, 0)));
         break;

      case SIGN_EXTEND:
         /* If the sign bit is known clear, this is the same as ZERO_EXTEND.
         Otherwise, show all the bits in the outer mode but not the inner
         may be nonzero.  */
         inner_nz = cached_nonzero_bits(self,XEXP (x, 0), mode,known_x, known_mode, known_ret);
         if (GET_MODE (XEXP (x, 0)) != VOIDmode){
            inner_nz &= mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,GET_MODE (XEXP (x, 0)));
            if (mtcs_simplify_rtx_val_signbit_known_set_p/*!val_signbit_known_set_p*/(mtcsSimplifyRtx,GET_MODE (XEXP (x, 0)), inner_nz))
               inner_nz |= (mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,mode) &
                     ~mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,GET_MODE (XEXP (x, 0))));
         }

         nonzero &= inner_nz;
         break;

      case AND:
         nonzero &= cached_nonzero_bits(self,XEXP (x, 0), mode,known_x, known_mode, known_ret)
         & cached_nonzero_bits(self,XEXP (x, 1), mode,known_x, known_mode, known_ret);
         break;

      case XOR:   case IOR:
      case UMIN:  case UMAX:  case SMIN:  case SMAX:
      {
         unsigned HOST_WIDE_INT nonzero0 = cached_nonzero_bits(self,XEXP (x, 0), mode,known_x, known_mode, known_ret);

         /* Don't call nonzero_bits for the second time if it cannot change
         anything.  */
         if ((nonzero & nonzero0) != nonzero)
            nonzero &= nonzero0 | cached_nonzero_bits(self,XEXP (x, 1), mode,known_x, known_mode, known_ret);
      }
         break;

      case PLUS:  case MINUS:
      case MULT:
      case DIV:   case UDIV:
      case MOD:   case UMOD:
      /* We can apply the rules of arithmetic to compute the number of
      high- and low-order zero bits of these operations.  We start by
      computing the width (position of the highest-order nonzero bit)
      and the number of low-order zero bits for each value.  */
      {
         unsigned HOST_WIDE_INT nz0 = cached_nonzero_bits(self,XEXP (x, 0), mode,known_x, known_mode, known_ret);
         unsigned HOST_WIDE_INT nz1 = cached_nonzero_bits(self,XEXP (x, 1), mode,known_x, known_mode, known_ret);
         int sign_index = xmode_width - 1;
         int width0 = floor_log2 (nz0) + 1;
         int width1 = floor_log2 (nz1) + 1;
         int low0 = ctz_or_zero (nz0);
         int low1 = ctz_or_zero (nz1);
         unsigned HOST_WIDE_INT op0_maybe_minusp  = nz0 & (HOST_WIDE_INT_1U << sign_index);
         unsigned HOST_WIDE_INT op1_maybe_minusp  = nz1 & (HOST_WIDE_INT_1U << sign_index);
         unsigned int result_width = mode_width;
         int result_low = 0;

         switch (code){
            case PLUS:
               result_width = MAX (width0, width1) + 1;
               result_low = MIN (low0, low1);
               break;
            case MINUS:
               result_low = MIN (low0, low1);
               break;
            case MULT:
               result_width = width0 + width1;
               result_low = low0 + low1;
               break;
            case DIV:
               if (width1 == 0)
                  break;
               if (!op0_maybe_minusp && !op1_maybe_minusp)
                  result_width = width0;
               break;
            case UDIV:
               if (width1 == 0)
                  break;
               result_width = width0;
               break;
            case MOD:
               if (width1 == 0)
                  break;
               if (!op0_maybe_minusp && !op1_maybe_minusp)
                  result_width = MIN (width0, width1);
               result_low = MIN (low0, low1);
               break;
            case UMOD:
               if (width1 == 0)
                  break;
               result_width = MIN (width0, width1);
               result_low = MIN (low0, low1);
               break;
            default:
               gcc_unreachable ();
         }

         /* Note that mode_width <= HOST_BITS_PER_WIDE_INT, see above.  */
         if (result_width < mode_width)
            nonzero &= (HOST_WIDE_INT_1U << result_width) - 1;

         if (result_low > 0){
            if (result_low < HOST_BITS_PER_WIDE_INT)
               nonzero &= ~((HOST_WIDE_INT_1U << result_low) - 1);
            else
               nonzero = 0;
         }
      }
         break;

      case ZERO_EXTRACT:
         if (CONST_INT_P (XEXP (x, 1))  && INTVAL (XEXP (x, 1)) < HOST_BITS_PER_WIDE_INT)
            nonzero &= (HOST_WIDE_INT_1U << INTVAL (XEXP (x, 1))) - 1;
         break;

      case SUBREG:
         /* If this is a SUBREG formed for a promoted variable that has
         been zero-extended, we know that at least the high-order bits
         are zero, though others might be too.  */
         if (SUBREG_PROMOTED_VAR_P (x) && SUBREG_PROMOTED_UNSIGNED_P (x))
            nonzero = mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,xmode)
                  & cached_nonzero_bits(self,SUBREG_REG (x), xmode, known_x, known_mode, known_ret);

         /* If the inner mode is a single word for both the host and target
         machines, we can compute this from which bits of the inner
         object might be nonzero.  */
         inner_mode = GET_MODE (SUBREG_REG (x));
         if (mtcs_mode_get_precision_poly/*!GET_MODE_PRECISION*/(mtcsMode,inner_mode).is_constant (&inner_width)
         && inner_width <= BITS_PER_WORD
         && inner_width <= HOST_BITS_PER_WIDE_INT){
            nonzero &= cached_nonzero_bits(self,SUBREG_REG (x), mode, known_x, known_mode, known_ret);

            /* On a typical CISC machine, accessing an object in a wider mode
            causes the high-order bits to become undefined.  So they are
            not known to be zero.

            On a typical RISC machine, we only have to worry about the way
            loads are extended.  Otherwise, if we get a reload for the inner
            part, it may be loaded from the stack, and then we may lose all
            the zero bits that existed before the store to the stack.  */
            rtx_code extend_op;
            if ((!mtcs_reg_get_word_register_operations/*!WORD_REGISTER_OPERATIONS*/(mtcsReg)
            || ((extend_op = mtcs_rtl_load_extend_op/*!load_extend_op*/(mtcsRTL,inner_mode)) == SIGN_EXTEND
            ? mtcs_simplify_rtx_val_signbit_known_set_p/*!val_signbit_known_set_p*/(mtcsSimplifyRtx,inner_mode, nonzero)
            : extend_op != ZERO_EXTEND)
            || !MEM_P (SUBREG_REG (x)))
            && xmode_width > inner_width)
               nonzero |= (mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,GET_MODE (x))
                     & ~mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,inner_mode));
         }
         break;

      case ASHIFT:
      case ASHIFTRT:
      case LSHIFTRT:
      case ROTATE:
      case ROTATERT:
         /* The nonzero bits are in two classes: any bits within MODE
         that aren't in xmode are always significant.  The rest of the
         nonzero bits are those that are significant in the operand of
         the shift when shifted the appropriate number of bits.  This
         shows that high-order bits are cleared by the right shift and
         low-order bits by left shifts.  */
         if (CONST_INT_P (XEXP (x, 1))
         && INTVAL (XEXP (x, 1)) >= 0
         && INTVAL (XEXP (x, 1)) < HOST_BITS_PER_WIDE_INT
         && INTVAL (XEXP (x, 1)) < xmode_width){
            int count = INTVAL (XEXP (x, 1));
            unsigned HOST_WIDE_INT mode_mask = mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,xmode);
            unsigned HOST_WIDE_INT op_nonzero = cached_nonzero_bits(self,XEXP (x, 0), mode,known_x, known_mode, known_ret);
            unsigned HOST_WIDE_INT inner = op_nonzero & mode_mask;
            unsigned HOST_WIDE_INT outer = 0;

            if (mode_width > xmode_width)
               outer = (op_nonzero & nonzero & ~mode_mask);

            switch (code){
               case ASHIFT:
                  inner <<= count;
                  break;

               case LSHIFTRT:
                  inner >>= count;
                  break;

               case ASHIFTRT:
                  inner >>= count;

                  /* If the sign bit may have been nonzero before the shift, we
                  need to mark all the places it could have been copied to
                  by the shift as possibly nonzero.  */
                  if (inner & (HOST_WIDE_INT_1U << (xmode_width - 1 - count)))
                     inner |= (((HOST_WIDE_INT_1U << count) - 1)<< (xmode_width - count));
                  break;

               case ROTATE:
                  inner = (inner << (count % xmode_width) | (inner >> (xmode_width - (count % xmode_width)))) & mode_mask;
                  break;

               case ROTATERT:
                  inner = (inner >> (count % xmode_width) | (inner << (xmode_width - (count % xmode_width)))) & mode_mask;
                  break;

               default:
                  gcc_unreachable ();
            }

            nonzero &= (outer | inner);
         }
         break;

      case FFS:
      case POPCOUNT:
         /* This is at most the number of bits in the mode.  */
         nonzero = (HOST_WIDE_INT_UC (2) << (floor_log2 (mode_width))) - 1;
         break;

      case CLZ:
         /* If CLZ has a known value at zero, then the nonzero bits are
         that value, plus the number of bits in the mode minus one.  */
         if (mtcs_mode_clz_defined_value_at_zero/*!CLZ_DEFINED_VALUE_AT_ZERO*/(mtcsMode,mode, nonzero))
            nonzero  |= (HOST_WIDE_INT_1U << (floor_log2 (mode_width))) - 1;
         else
            nonzero = -1;
         break;

      case CTZ:
         /* If CTZ has a known value at zero, then the nonzero bits are
         that value, plus the number of bits in the mode minus one.  */
         if (mtcs_mode_ctz_defined_value_at_zero/*!CTZ_DEFINED_VALUE_AT_ZERO*/(mtcsMode,mode, nonzero))
            nonzero |= (HOST_WIDE_INT_1U << (floor_log2 (mode_width))) - 1;
         else
            nonzero = -1;
         break;

      case CLRSB:
         /* This is at most the number of bits in the mode minus 1.  */
         nonzero = (HOST_WIDE_INT_1U << (floor_log2 (mode_width))) - 1;
         break;

      case PARITY:
         nonzero = 1;
         break;

      case IF_THEN_ELSE:
      {
         unsigned HOST_WIDE_INT nonzero_true  = cached_nonzero_bits(self,XEXP (x, 1), mode, known_x, known_mode, known_ret);

         /* Don't call nonzero_bits for the second time if it cannot change
         anything.  */
         if ((nonzero & nonzero_true) != nonzero)
            nonzero &= nonzero_true  | cached_nonzero_bits(self,XEXP (x, 2), mode, known_x, known_mode, known_ret);
      }
         break;

      default:
         break;
   }

   return nonzero;
}

/* See the macro definition above.  */
#undef cached_num_sign_bit_copies

/* Return true if nonzero_bits1 might recurse into both operands
   of X.  */

static inline bool nonzero_bits_binary_arith_p (const_rtx x)
{
   if (!ARITHMETIC_P (x))
      return false;
   switch (GET_CODE (x)){
      case AND:
      case XOR:
      case IOR:
      case UMIN:
      case UMAX:
      case SMIN:
      case SMAX:
      case PLUS:
      case MINUS:
      case MULT:
      case DIV:
      case UDIV:
      case MOD:
      case UMOD:
         return true;
      default:
         return false;
   }
}


/* The function cached_nonzero_bits is a wrapper around nonzero_bits1.
   It avoids exponential behavior in nonzero_bits1 when X has
   identical subexpressions on the first or the second level.  */
static unsigned HOST_WIDE_INT cached_nonzero_bits (MtcsRtlanal *self,const_rtx x, scalar_int_mode mode, const_rtx known_x,
           machine_mode known_mode,
           unsigned HOST_WIDE_INT known_ret)
{
   if (x == known_x && mode == known_mode)
      return known_ret;

   /* Try to find identical subexpressions.  If found call
   nonzero_bits1 on X with the subexpressions as KNOWN_X and the
   precomputed value for the subexpression as KNOWN_RET.  */
   n_debug("mtcsrtlanal.c cached_nonzero_bits 00 %d\n",mode);
   if (nonzero_bits_binary_arith_p (x)){
      rtx x0 = XEXP (x, 0);
      rtx x1 = XEXP (x, 1);
      n_debug("mtcsrtlanal.c cached_nonzero_bits 11 %d\n",mode);
      mtcs_print_rtl_single(stderr,x0);
      mtcs_print_rtl_single(stderr,x1);

      /* Check the first level.  */
      if (x0 == x1)
         return nonzero_bits1(self,x, mode, x0, mode, cached_nonzero_bits(self,x0, mode, known_x,known_mode, known_ret));
      n_debug("mtcsrtlanal.c cached_nonzero_bits 22 %d\n",mode);

      /* Check the second level.  */
      if (nonzero_bits_binary_arith_p (x0) && (x1 == XEXP (x0, 0) || x1 == XEXP (x0, 1)))
         return nonzero_bits1(self,x, mode, x1, mode, cached_nonzero_bits(self,x1, mode, known_x,known_mode, known_ret));
      n_debug("mtcsrtlanal.c cached_nonzero_bits 33 %d\n",mode);

      if (nonzero_bits_binary_arith_p (x1) && (x0 == XEXP (x1, 0) || x0 == XEXP (x1, 1)))
         return nonzero_bits1(self,x, mode, x0, mode,cached_nonzero_bits(self,x0, mode, known_x,known_mode, known_ret));
   }
   n_debug("mtcsrtlanal.c cached_nonzero_bits 44 %d\n",mode);

   return nonzero_bits1(self,x, mode, known_x, known_mode, known_ret);
}

//原型 nonzero_bits rtl.h rtlanal.cc
unsigned HOST_WIDE_INT mtcs_rtlanal_nonzero_bits (MtcsRtlanal *self,const_rtx x, machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   if (mode == VOIDmode)
      mode = GET_MODE (x);
   scalar_int_mode int_mode;
   if (!mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode))
      return mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,mode);
   unsigned HOST_WIDE_INT ret = cached_nonzero_bits(self,x, int_mode, NULL_RTX, VOIDmode, 0);
   n_debug("mtcsrtlanal.c nonzero_bits mode:%d ret:%lu\n",mode,ret);
   return ret;
}


/* Like covers_regno_no_parallel_p, but also handles PARALLELs where
   any member matches the covers_regno_no_parallel_p criteria.  */
//原型 covers_regno_p rtlanal.cc
static bool covers_regno_p (MtcsRtlanal *self,const_rtx dest, unsigned int test_regno)
{
   if (GET_CODE (dest) == PARALLEL){
      /* Some targets place small structures in registers for return
      values of functions, and those registers are wrapped in
      PARALLELs that we may see as the destination of a SET.  */
      int i;
      for (i = XVECLEN (dest, 0) - 1; i >= 0; i--){
         rtx inner = XEXP (XVECEXP (dest, 0, i), 0);
         if (inner != NULL_RTX  && covers_regno_no_parallel_p(self,inner, test_regno))
            return true;
      }
      return false;
   }else
      return covers_regno_no_parallel_p(self,dest, test_regno);
}


/* Utility function for dead_or_set_p to check an individual register. */
//原型 dead_or_set_regno_p rtl.h rtlanal.cc
bool mtcs_rtlanal_dead_or_set_regno_p (MtcsRtlanal *self,const rtx_insn *insn, unsigned int test_regno)
{
   const_rtx pattern;

   /* See if there is a death note for something that includes TEST_REGNO.  */
   if (find_regno_note (insn, REG_DEAD, test_regno))
      return true;

   if (CALL_P (insn) && find_regno_fusage (insn, CLOBBER, test_regno))
      return true;

   pattern = PATTERN (insn);

   /* If a COND_EXEC is not executed, the value survives.  */
   if (GET_CODE (pattern) == COND_EXEC)
      return false;

   if (GET_CODE (pattern) == SET || GET_CODE (pattern) == CLOBBER)
      return covers_regno_p(self,SET_DEST (pattern), test_regno);
   else if (GET_CODE (pattern) == PARALLEL){
      int i;

      for (i = XVECLEN (pattern, 0) - 1; i >= 0; i--){
         rtx body = XVECEXP (pattern, 0, i);

         if (GET_CODE (body) == COND_EXEC)
            body = COND_EXEC_CODE (body);

         if ((GET_CODE (body) == SET || GET_CODE (body) == CLOBBER)
         && covers_regno_p(self,SET_DEST (body), test_regno))
            return true;
      }
   }

   return false;
}


/* Return true if X's old contents don't survive after INSN.
   This will be true if X is a register and X dies in INSN or because
   INSN entirely sets X.

   "Entirely set" means set directly and not through a SUBREG, or
   ZERO_EXTRACT, so no trace of the old contents remains.
   Likewise, REG_INC does not count.

   REG may be a hard or pseudo reg.  Renumbering is not taken into account,
   but for this use that makes no difference, since regs don't overlap
   during their lifetimes.  Therefore, this function may be used
   at any time after deaths have been computed.

   If REG is a hard reg that occupies multiple machine registers, this
   function will only return true if each of those registers will be replaced
   by INSN.  */
//原型 dead_or_set_p rtl.h rtlanal.cc
bool mtcs_rtlanal_dead_or_set_p (MtcsRtlanal *self,const rtx_insn *insn, const_rtx x)
{
   unsigned int regno, end_regno;
   unsigned int i;

   gcc_assert (REG_P (x));

   regno = REGNO (x);
   end_regno = END_REGNO (x);
   for (i = regno; i < end_regno; i++)
      if (! mtcs_rtlanal_dead_or_set_regno_p/*!dead_or_set_regno_p*/(self,insn, i))
         return false;

   return true;
}


/* MEM has a PRE/POST-INC/DEC/MODIFY address X.  Extract the operands of
   the equivalent add insn and pass the result to FN, using DATA as the
   final argument.  */
//原型 for_each_inc_dec_find_inc_dec rtlanal.cc
static int for_each_inc_dec_find_inc_dec (MtcsRtlanal *self,rtx mem, for_each_inc_dec_fn fn, void *data)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   rtx x = XEXP (mem, 0);
   switch (GET_CODE (x)){
      case PRE_INC:
      case POST_INC:
      {
         poly_int64 size = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (mem));
         rtx r1 = XEXP (x, 0);
         rtx c = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,size, GET_MODE (r1));
         return fn (mem, x, r1, r1, c, data);
      }

      case PRE_DEC:
      case POST_DEC:
      {
         poly_int64 size = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (mem));
         rtx r1 = XEXP (x, 0);
         rtx c = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,-size, GET_MODE (r1));
         return fn (mem, x, r1, r1, c, data);
      }

      case PRE_MODIFY:
      case POST_MODIFY:
      {
         rtx r1 = XEXP (x, 0);
         rtx add = XEXP (x, 1);
         return fn (mem, x, r1, add, NULL, data);
      }

      default:
         gcc_unreachable ();
   }
}

/* Traverse *LOC looking for MEMs that have autoinc addresses.
   For each such autoinc operation found, call FN, passing it
   the innermost enclosing MEM, the operation itself, the RTX modified
   by the operation, two RTXs (the second may be NULL) that, once
   added, represent the value to be held by the modified RTX
   afterwards, and DATA.  FN is to return 0 to continue the
   traversal or any other value to have it returned to the caller of
   for_each_inc_dec.  */
//原型 for_each_inc_dec rtl.h rtlanal.cc
int mtcs_rtlanal_for_each_inc_dec (MtcsRtlanal *self,rtx x,for_each_inc_dec_fn fn, void *data)
{
   subrtx_var_iterator::array_type array;
   FOR_EACH_SUBRTX_VAR (iter, array, x, NONCONST){
      rtx mem = *iter;
      if (mem && MEM_P (mem)
      && GET_RTX_CLASS (GET_CODE (XEXP (mem, 0))) == RTX_AUTOINC){
         int res = for_each_inc_dec_find_inc_dec(self,mem, fn, data);
         if (res != 0)
            return res;
         iter.skip_subrtxes ();
      }
   }
   return 0;
}


/* Return true if register REG is set or clobbered in an insn between
   FROM_INSN and TO_INSN (exclusive of those two).  */
//原型 reg_set_between_p rtl.h rtlanal.cc
bool mtcs_rtlanal_reg_set_between_p (MtcsRtlanal *self,const_rtx reg, const rtx_insn *from_insn,
         const rtx_insn *to_insn)
{
   const rtx_insn *insn;

   if (from_insn == to_insn)
      return false;

   for (insn = NEXT_INSN (from_insn); insn != to_insn; insn = NEXT_INSN (insn))
      if (INSN_P (insn) && mtcs_rtlanal_reg_set_p/*!reg_set_p*/(self,reg, insn))
         return true;
   return false;
}

/* Similar to reg_set_between_p, but check all registers in X.  Return false
   only if none of them are modified between START and END.  Return true if
   X contains a MEM; this routine does use memory aliasing.  */
//原型 modified_between_p rtl.h rtlanal.cc
bool mtcs_rtlanal_modified_between_p (MtcsRtlanal *self,const_rtx x, const rtx_insn *start, const rtx_insn *end)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);

   const enum rtx_code code = GET_CODE (x);
   const char *fmt;
   int i, j;
   rtx_insn *insn;

   if (start == end)
      return false;

   switch (code){
      CASE_CONST_ANY:
      case CONST:
      case SYMBOL_REF:
      case LABEL_REF:
         return false;

      case PC:
         return true;

      case MEM:
         if (mtcs_rtlanal_modified_between_p/*!modified_between_p*/(self,XEXP (x, 0), start, end))
            return true;
         if (MEM_READONLY_P (x))
            return false;
         for (insn = NEXT_INSN (start); insn != end; insn = NEXT_INSN (insn))
            if (mtcs_alias_memory_modified_in_insn_p/*!memory_modified_in_insn_p*/(mtcsAlias,x, insn))
               return true;
         return false;

      case REG:
         return mtcs_rtlanal_reg_set_between_p/*!reg_set_between_p*/(self,x, start, end);

      default:
         break;
   }

   fmt = GET_RTX_FORMAT (code);
   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--){
      if (fmt[i] == 'e' && mtcs_rtlanal_modified_between_p/*!modified_between_p*/(self,XEXP (x, i), start, end))
         return true;

   else if (fmt[i] == 'E')
      for (j = XVECLEN (x, i) - 1; j >= 0; j--)
         if (mtcs_rtlanal_modified_between_p/*!modified_between_p*/(self,XVECEXP (x, i, j), start, end))
            return true;
   }

   return false;
}

/* Return true if X is an address that is known to not be zero.  */
//原型 nonzero_address_p rtl.h rtlanal.cc
bool mtcs_rtlanal_nonzero_address_p (MtcsRtlanal *self,const_rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   const enum rtx_code code = GET_CODE (x);

   switch (code){
      case SYMBOL_REF:
         return flag_delete_null_pointer_checks && !SYMBOL_REF_WEAK (x);

      case LABEL_REF:
         return true;

      case REG:
         /* As in rtx_varies_p, we have to use the actual rtx, not reg number.  */
         if (x == mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL)
         || x == mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL)
         || x == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)
         || (x == mtcs_rtl_get_arg_pointer_rtx/*!arg_pointer_rtx*/(mtcsRTL) &&
         mtcsReg->hardRegs.x_fixed_regs/*!fixed_regs*/[mtcs_reg_get_arg_pointer_regnum/*!ARG_POINTER_REGNUM*/(mtcsReg)]))
            return true;
         /* All of the virtual frame registers are stack references.  */
         if (mtcs_reg_virtual_register_p/*!VIRTUAL_REGISTER_P*/(mtcsReg,x))
            return true;
         return false;

      case CONST:
         return mtcs_rtlanal_nonzero_address_p/*!nonzero_address_p*/(self,XEXP (x, 0));

      case PLUS:
         /* Handle PIC references.  */
         if (XEXP (x, 0) == mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(mtcsRTL)
         && CONSTANT_P (XEXP (x, 1)))
            return true;
         return false;

      case PRE_MODIFY:
         /* Similar to the above; allow positive offsets.  Further, since
         auto-inc is only allowed in memories, the register must be a
         pointer.  */
         if (CONST_INT_P (XEXP (x, 1)) && INTVAL (XEXP (x, 1)) > 0)
            return true;
         return mtcs_rtlanal_nonzero_address_p/*!nonzero_address_p*/(self,XEXP (x, 0));

      case PRE_INC:
         /* Similarly.  Further, the offset is always positive.  */
         return true;

      case PRE_DEC:
      case POST_DEC:
      case POST_INC:
      case POST_MODIFY:
         return mtcs_rtlanal_nonzero_address_p/*!nonzero_address_p*/(self,XEXP (x, 0));

      case LO_SUM:
         return mtcs_rtlanal_nonzero_address_p/*!nonzero_address_p*/(self,XEXP (x, 1));

      default:
         break;
   }

   /* If it isn't one of the case above, might be zero.  */
   return false;
}

MtcsRtlanal *mtcs_rtlanal_new(MtcsMode *mtcsMode)
{
   MtcsRtlanal *self = n_slice_alloc0 (sizeof(MtcsRtlanal));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcsRtlanalInit(self);
   return self;
}
