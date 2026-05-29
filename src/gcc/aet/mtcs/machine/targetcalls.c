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

#include "config.h"
#define INCLUDE_ALGORITHM /* reverse */
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "tree-pass.h"
#include "cfghooks.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "tree-pretty-print.h" /* for dump_function_header */
#include "varasm.h"
#include "insn-attr.h"
#include "conditions.h"
#include "flags.h"
#include "output.h"
#include "except.h"
#include "rtl-error.h"
#include "toplev.h" /* exact_log2, floor_log2 */
#include "reload.h"
#include "intl.h"
#include "cfgrtl.h"
#include "debug.h"
#include "tree-pass.h"
#include "tree-ssa.h"
#include "cfgloop.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "rtl-iter.h"
#include "print-rtl.h"
#include "function-abi.h"
#include "common/common-target.h"
#include "diagnostic.h"
#include "context.h"
#include "options.h"
#include "expmed.h"
#include "expr.h"
#include "fold-const.h"
#include "dwarf2out.h"
#include "common/common-targhooks.h"

#include "cfganal.h"
#include "cfgcleanup.h"
#include "alias.h"
#include "rtlhooks-def.h"
#include "tree-pass.h"
#include "dbgcnt.h"
#include "rtlanal.h"
#include "opts.h"

#include "aet/aetprinttree.h"
#include "../mtcstarget.h"
#include "../mtcsfuncabi.h"
#include "../mtcsmicro.h"
#include "../mtcscalls.h"

#include "targetcalls.h"

static void setupIncomingVarargs_cb(TargetCalls *self,cumulative_args_t cum_v,
        const mtcs_function_arg_info &arg,int *pretend_size ATTRIBUTE_UNUSED, int no_rtl);

/* Choose the mode and rtx to use to zero REGNO, storing tem in PMODE and
   PREGNO_RTX and returning TRUE if successful, otherwise returning FALSE.  If
   the natural mode for REGNO doesn't work, attempt to group it with subsequent
   adjacent registers set in TOZERO.  */
//原型 zcur_select_mode_rtx targhooks.cc
static inline bool zcur_select_mode_rtx (TargetCalls *self,unsigned int regno, machine_mode *pmode,
            rtx *pregno_rtx, HardRegSet *tozero)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   rtx regno_rtx = mtcsRtlData->regno_reg_rtx/*!regno_reg_rtx*/[regno];
   machine_mode mode = GET_MODE (regno_rtx);

   /* If the natural mode doesn't work, try some wider mode.  */
   if (!mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,regno, mode)){
      bool found = false;
      for (int nregs = 2;  !found && nregs <= hard_regno_max_nregs
      && regno + nregs <= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/
      && mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(tozero, regno + nregs - 1); nregs++){
         mode = mtcs_reg_choose_hard_reg_mode/*!choose_hard_reg_mode*/(mtcsReg,regno, nregs, 0);
         if (mode == E_VOIDmode)
            continue;
         gcc_checking_assert (mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,regno, mode));
         regno_rtx = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mode, regno);
         found = true;
      }
      if (!found)
         return false;
   }

   *pmode = mode;
   *pregno_rtx = regno_rtx;
   return true;
}

//原型targetm.calls.struct_value_rtx   #define TARGET_STRUCT_VALUE_RTX hook_rtx_tree_int_null
static rtx structValueRtx_cb (TargetCalls *self,tree fndecl, int incoming)
{
   return NULL;
}

//原型 targetm.calls.allocate_stack_slots_for_args #define TARGET_ALLOCATE_STACK_SLOTS_FOR_ARGS hook_bool_void_true
static bool allocateStackSlotsForForArgs_cb (TargetCalls *self)
{
  return true;
}

//原型 targetm.calls.return_in_memory (type, fntype) #define TARGET_RETURN_IN_MEMORY nvptx_return_in_memory
static bool returnInMemory_cb (TargetCalls *self,const_tree type,const_tree fntype ATTRIBUTE_UNUSED)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   return (TYPE_MODE (type) ==mtcsMode->modes.M_BLKmode);
}

//原型 targetm.calls.function_arg (args_so_far, ptr_arg);#define TARGET_FUNCTION_ARG default_function_arg
static rtx functionArg_cb(TargetCalls *self,cumulative_args_t arg, const mtcs_function_arg_info &info)
{
    gcc_unreachable ();
}

//原型ttargetm.calls.arg_partial_bytes (args_so_far, arg);#define TARGET_ARG_PARTIAL_BYTES hook_int_CUMULATIVE_ARGS_arg_info_0
static int argPartialBytes_cb(TargetCalls *self,cumulative_args_t cum_v, const mtcs_function_arg_info &info)
{
    return 0;
}


//原型targetm.calls.function_arg_padding (passed_mode, type);#define TARGET_FUNCTION_ARG_PADDING default_function_arg_padding
static enum pad_direction  functionArgPadding_cb (TargetCalls *self,machine_mode mode, const_tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   if (!BYTES_BIG_ENDIAN)
      return PAD_UPWARD;
   unsigned HOST_WIDE_INT size;
   if (mode == mtcsMode->modes.M_BLKmode){
      if (!type || TREE_CODE (TYPE_SIZE (type)) != INTEGER_CST)
         return PAD_UPWARD;
      size = int_size_in_bytes (type);
   }else
      /* Targets with variable-sized modes must override this hook
      and handle variable-sized modes explicitly.  */
      size = mtcs_mode_get_size(mtcsMode,mode);//.to_constant ();

   if (size < (mtcs_func_get_parm_boundary/*!PARM_BOUNDARY*/(mtcsFunc) / BITS_PER_UNIT))
      return PAD_DOWNWARD;
   return PAD_UPWARD;
}

//原型targetm.calls.function_arg_round_boundary (passed_mode,type);#define TARGET_FUNCTION_ARG_ROUND_BOUNDARY default_function_arg_round_boundary
static unsigned int functionArgRoundBoundary_cb(TargetCalls *self,machine_mode mode ATTRIBUTE_UNUSED,
                     const_tree type ATTRIBUTE_UNUSED)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   return mtcs_func_get_parm_boundary/*!PARM_BOUNDARY*/(mtcsFunc);
}

//原型 targetm.calls.function_arg_offset (passed_mode, type);#define TARGET_FUNCTION_ARG_OFFSET default_function_arg_offset
static HOST_WIDE_INT functionArgOffset_cb(TargetCalls *self,machine_mode mode ATTRIBUTE_UNUSED, const_tree type ATTRIBUTE_UNUSED)
{
   return 0;
}

//原型 targetm.calls.start_call_args (args_so_far); #define TARGET_START_CALL_ARGS hook_void_CUMULATIVE_ARGS
static void  startCallArgs_cb (TargetCalls *self,cumulative_args_t args)
{
   //空实现
}

//原型 targetm.calls.return_pops_args  #define TARGET_RETURN_POPS_ARGS default_return_pops_args
static poly_int64 returnPopsArgs_cb(TargetCalls *self,tree fundecl, tree funtype, poly_int64 size)
{
   return 0;
}


//原型targetm.calls.return_in_msb (tfom) #define TARGET_RETURN_IN_MSB hook_bool_const_tree_false
static  bool returnInMsb_cb(MtcsTarget *self,const_tree valtype)
{
   return false;
}

//原型  targetm.calls.static_chain (fndecl_or_type, false); #define TARGET_STATIC_CHAIN default_static_chain
static rtx staticChain_cb(TargetCalls *self,const_tree ARG_UNUSED (fndecl_or_type), bool incoming_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   mtcs_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
   if (incoming_p){
#ifdef STATIC_CHAIN_INCOMING_REGNUM //howt=0 nvptx=0
      return mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,pMode, STATIC_CHAIN_INCOMING_REGNUM);
#endif
   }

#ifdef STATIC_CHAIN_REGNUM //host=0 nvptx=1
   return mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,pMode, STATIC_CHAIN_REGNUM);
#endif
   {
      static bool issued_error;
      if (!issued_error){
         issued_error = true;
         sorry ("nested functions not supported on this target");
      }
      /* It really doesn't matter what we return here, so long at it
      doesn't cause the rest of the compiler to crash.  */
      return gen_rtx_MEM (pMode, mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL));
   }
}

//原型  targetm.calls.pretend_outgoing_varargs_named (args_so_far) #define TARGET_PRETEND_OUTGOING_VARARGS_NAMED default_pretend_outgoing_varargs_named
static bool pretendOutgointVarargsNamed_cb(TargetCalls *self,cumulative_args_t ca ATTRIBUTE_UNUSED)
{
    return (self->setup_incoming_varargs/*!targetm.calls.setup_incoming_varargs*/
          != setupIncomingVarargs_cb/*!default_setup_incoming_varargs*/);
}

//原型 targetm.calls.setup_incoming_varargs #define TARGET_SETUP_INCOMING_VARARGS default_setup_incoming_varargs
static void setupIncomingVarargs_cb(TargetCalls *self,cumulative_args_t cum_v,
        const mtcs_function_arg_info &arg,int *pretend_size ATTRIBUTE_UNUSED, int no_rtl)
{

}

//原型 targetm.calls.callee_copies (pack_cumulative_args (ca), arg); #define TARGET_CALLEE_COPIES hook_bool_CUMULATIVE_ARGS_arg_info_false
static bool calleeCopies_cb(TargetCalls *self,cumulative_args_t ca, const mtcs_function_arg_info &arg)
{
   return false;
}

//原型 targetm.calls.internal_arg_pointer() #define TARGET_INTERNAL_ARG_POINTER default_internal_arg_pointer
static rtx internalArgPointer_cb(TargetCalls *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);


   int argPointerRegnum=mtcsReg->normalHardRegsNum.arg_pointer_regnum/*!ARG_POINTER_REGNUM*/;
   int stackPointerRegnum=mtcsReg->normalHardRegsNum.stack_pointer_regnum/*!STACK_POINTER_REGNUM*/;
   int framePointerRegnum=mtcsReg->normalHardRegsNum.frame_pointer_regnum/*!FRAME_POINTER_REGNUM*/;

   /* If the reg that the virtual arg pointer will be translated into is
   not a fixed reg or is the stack pointer, make a copy of the virtual
   arg pointer, and address parms via the copy.  The frame pointer is
   considered fixed even though it is not marked as such.  */
   if ((argPointerRegnum/*!ARG_POINTER_REGNUM*/ == stackPointerRegnum/*!STACK_POINTER_REGNUM*/
   || ! (mtcsReg->hardRegs.x_fixed_regs/*!fixed_regs*/[argPointerRegnum/*!ARG_POINTER_REGNUM*/]
   || argPointerRegnum/*!ARG_POINTER_REGNUM*/ ==framePointerRegnum/*!FRAME_POINTER_REGNUM*/)))
      return mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,
            mtcs_rtl_get_virtual_incoming_args_rtx/*!virtual_incoming_args_rtx*/(mtcsRTL));
   else
      return  mtcs_rtl_get_virtual_incoming_args_rtx/*!virtual_incoming_args_rtx*/(mtcsRTL);
}


//原型 targetm.calls.get_raw_result_mode (regno) #define TARGET_GET_RAW_RESULT_MODE default_get_reg_raw_mode
static fixed_size_mode getRawResultMode_cb (TargetCalls *self,int regno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   /* Targets must override this hook if the underlying register is
   variable-sized.  */
   return mtcs_mode_as_a <fixed_size_mode> (mtcsMode,mtcsReg->hardRegs.x_reg_raw_mode/*!reg_raw_mode*/[regno]);
}

//原型 targetm.calls.get_raw_arg_mode (regno); #define TARGET_GET_RAW_ARG_MODE default_get_reg_raw_mode
static fixed_size_mode getRawArgMode_cb (TargetCalls *self,int regno)
{
   return getRawResultMode_cb(self,regno);
}

//原型 targetm.calls.expand_builtin_saveregs () #define TARGET_EXPAND_BUILTIN_SAVEREGS default_expand_builtin_saveregs
static rtx expandBuiltinSavergs_cb (TargetCalls *self)
{
   error ("%<__builtin_saveregs%> not supported by this target");
   return const0_rtx;
}

//原型 targetm.calls.emit_call_builtin___clear_cache (begin, end);
//#define TARGET_EMIT_CALL_BUILTIN___CLEAR_CACHE default_emit_call_builtin___clear_cache
static void emitCallBuiltin__ClearCache_cb(TargetCalls *self,rtx begin, rtx end)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsBuiltins *mtcsBuiltins=mtcs_target_get_builtins(mtcsTarget);
   mtcs_builtins_default_emit_call_builtin___clear_cache(mtcsBuiltins,begin,end);
}

//原型  targetm.calls.trampoline_init (m_tramp, t_func, r_chain) #define TARGET_TRAMPOLINE_INIT default_trampoline_init
static void trampolineInit_cb(TargetCalls *self,rtx ARG_UNUSED (m_tramp), tree ARG_UNUSED (t_func),rtx ARG_UNUSED (r_chain))
{
   sorry ("nested function trampolines not supported on this target");
}

//原型 targetm.calls.empty_record_p (type) #define TARGET_EMPTY_RECORD_P hook_bool_const_tree_false
static bool emptyRecordP_cb(TargetCalls *self,tree type)
{
   return false;
}


//原型 targetm.calls.zero_call_used_regs (selected_hardregs); #define TARGET_ZERO_CALL_USED_REGS default_zero_call_used_regs
static HardRegSet zeroCallUsedRegs_cb (TargetCalls *self,HardRegSet *need_zeroed_hardregs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsExpr *mtcsExpr = mtcs_target_get_expr(mtcsTarget);
   MtcsRecog *mtcsRecog = mtcs_target_get_recog(mtcsTarget);

   gcc_assert (!mtcs_reg_hard_reg_set_empty_p/*!hard_reg_set_empty_p*/(need_zeroed_hardregs));
   HardRegSet failed={mtcs_reg_get_hard_reg_element_count(mtcsReg)};
   mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&failed);

   bool progress = false;
   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   /* First, try to zero each register in need_zeroed_hardregs by
   loading a zero into it, taking note of any failures in
   FAILED.  */
   for (unsigned int regno = 0; regno <firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; regno++)
      if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(need_zeroed_hardregs, regno)){
         rtx_insn *last_insn = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
         rtx regno_rtx;
         machine_mode mode;

         if (!zcur_select_mode_rtx(self,regno, &mode, &regno_rtx, need_zeroed_hardregs)){
            mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&failed, regno);
            continue;
         }

         rtx zero = CONST0_RTX (mode);
         rtx_insn *insn = mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,regno_rtx, zero);
         if (!mtcs_recog_valid_insn_p/*!valid_insn_p*/(mtcsRecog,insn)){
            mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&failed, regno);
            mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last_insn);
         }else{
            progress = true;
            regno += mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,regno, mode) - 1;
         }
      }

   /* Now retry with copies from zeroed registers, as long as we've
   made some PROGRESS, and registers remain to be zeroed in
   FAILED.  */
   while (progress && !mtcs_reg_hard_reg_set_empty_p/*!hard_reg_set_empty_p*/(&failed)){
      HardRegSet retrying = failed;
      mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&failed);
      progress = false;

      for (unsigned int regno = 0; regno < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; regno++)
         if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&retrying, regno)){
            rtx regno_rtx;
            machine_mode mode;

            /* This might select registers we've already zeroed.  If grouping
            with them is what it takes to get regno zeroed, so be it.  */
            if (!zcur_select_mode_rtx(self,regno, &mode, &regno_rtx,need_zeroed_hardregs)){
               mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&failed, regno);
               continue;
            }

            bool success = false;
            /* Look for a source.  */
            for (unsigned int src = 0; src < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; src++){
               /* If SRC hasn't been zeroed (yet?), skip it.  */
               if (! mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(need_zeroed_hardregs, src))
                  continue;
               if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&retrying, src))
                  continue;

               /* Check that SRC can hold MODE, and that any other
               registers needed to hold MODE in SRC have also been
               zeroed.  */
               if (!mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,src, mode))
                  continue;
               unsigned n = mtcsTarget/*!targetm.hard_regno_nregs*/->hard_regno_nregs(mtcsTarget,src, mode);
               bool ok = true;
               for (unsigned i = 1; ok && i < n; i++)
                  ok = (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(need_zeroed_hardregs, src + i)
                           && !mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&retrying, src + i));
               if (!ok)
                  continue;

               /* SRC is usable, try to copy from it.  */
               rtx_insn *last_insn =mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
               rtx src_rtx = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mode, src);
               rtx_insn *insn = mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,regno_rtx, src_rtx);
               if (!mtcs_recog_valid_insn_p/*!valid_insn_p*/(mtcsRecog,insn))
                  /* It didn't work, remove any inserts.  We'll look
                  for another SRC.  */
                  mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last_insn);
               else{
                  /* We're done for REGNO.  */
                  success = true;
                  break;
               }
            }

            /* If nothing worked for REGNO this round, mark it to be
            retried if we get another round.  */
            if (!success)
               mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&failed, regno);
            else{
               /* Take note so as to enable another round if needed.  */
               progress = true;
               regno += mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,regno, mode) - 1;
            }
         }
   }

   /* If any register remained, report it.  */
   if (!progress){
      static bool issued_error;
      if (!issued_error){
         const char *name = NULL;
         for (unsigned int i = 0; zero_call_used_regs_opts[i].name != NULL; ++i)
            if (mtcsOptionsItem->x_flag_zero_call_used_regs == zero_call_used_regs_opts[i].flag){
               name = zero_call_used_regs_opts[i].name;
               break;
            }

         if (!name)
            name = "";

         issued_error = true;
         sorry ("argument %qs is not supported for %qs on this target",name, "-fzero-call-used-regs");
      }
   }

   return *need_zeroed_hardregs;
}

//原型 argetm.calls.call_offset_return_label #define TARGET_CALL_OFFSET_RETURN_LABEL hook_int_rtx_insn_0
static int callOffsetReturnLabel_cb (TargetCalls *self,rtx_insn *x)
{
   return 0;
}

void  target_calls_init(TargetCalls *self)
{
   //原型 targetm.calls.custom_function_descriptors #define TARGET_CUSTOM_FUNCTION_DESCRIPTORS -1
   self->custom_function_descriptors = 0;
   //原型 targetm.calls.omit_struct_return_reg #define TARGET_OMIT_STRUCT_RETURN_REG true
   self->omit_struct_return_reg = false;
   //原型targetm.calls.struct_value_rtx   #define TARGET_STRUCT_VALUE_RTX hook_rtx_tree_int_null
   self->struct_value_rtx = structValueRtx_cb;
   //原型 targetm.calls.allocate_stack_slots_for_args #define TARGET_ALLOCATE_STACK_SLOTS_FOR_ARGS hook_bool_void_true
   self->allocate_stack_slots_for_args = allocateStackSlotsForForArgs_cb;
   //原型 targetm.calls.promote_function_mode (NULL_TREE, mode, punsignedp, funtype,for_return);#define TARGET_PROMOTE_FUNCTION_MODE default_promote_function_mode
   self->promote_function_mode = NULL;
   //原型 targetm.calls.return_in_memory (type, fntype) #define TARGET_RETURN_IN_MEMORY nvptx_return_in_memory
   self->return_in_memory = returnInMemory_cb;
   //原型targetm.calls.function_value (valtype, func ? func : fntype, outgoing); #define TARGET_FUNCTION_VALUE default_function_value
   self->function_value = NULL;
   //原型 targetm.calls.libcall_value (mode, fun); #define TARGET_LIBCALL_VALUE default_libcall_value
   self->libcall_value = NULL;
   //原型 targetm.calls.function_arg (args_so_far, ptr_arg);#define TARGET_FUNCTION_ARG default_function_arg
   self->function_arg = functionArg_cb ;
   //原型targetm.calls.function_arg_advance (args_so_far, ptr_arg);#define TARGET_FUNCTION_ARG_ADVANCE nvptx_function_arg_advance
   self->function_arg_advance = NULL;
   //原型ttargetm.calls.arg_partial_bytes (args_so_far, arg);#define TARGET_ARG_PARTIAL_BYTES hook_int_CUMULATIVE_ARGS_arg_info_0
   self->arg_partial_bytes = argPartialBytes_cb;
   //原型targetm.calls.function_arg_padding (passed_mode, type);#define TARGET_FUNCTION_ARG_PADDING default_function_arg_padding
   self->function_arg_padding = functionArgPadding_cb;
   //原型targetm.calls.function_arg_boundary (passed_mode, type);#define TARGET_FUNCTION_ARG_BOUNDARY nvptx_function_arg_boundary
   self->function_arg_boundary = NULL;
   //原型targetm.calls.function_arg_round_boundary (passed_mode,type);#define TARGET_FUNCTION_ARG_ROUND_BOUNDARY default_function_arg_round_boundary
   self->function_arg_round_boundary = functionArgRoundBoundary_cb;
   //原型 targetm.calls.function_arg_offset (passed_mode, type);#define TARGET_FUNCTION_ARG_OFFSET default_function_arg_offset
   self->function_arg_offset = functionArgOffset_cb;
   //原型 targetm.calls.push_argument(unsigned int );#define TARGET_PUSH_ARGUMENT default_push_argument
   self->push_argument = NULL;
   //原型 targetm.calls.start_call_args (args_so_far); #define TARGET_START_CALL_ARGS hook_void_CUMULATIVE_ARGS
   self->start_call_args = startCallArgs_cb;
   //原型 targetm.calls.call_args (args_so_far, argvec[i].reg, NULL_TREE); #define TARGET_CALL_ARGS hook_void_CUMULATIVE_ARGS_rtx_tree
   self->call_args = NULL;
   //原型 targetm.calls.return_pops_args  #define TARGET_RETURN_POPS_ARGS default_return_pops_args
   self->return_pops_args = returnPopsArgs_cb;
   //原型 targetm.calls.return_in_msb (tfom) #define TARGET_RETURN_IN_MSB hook_bool_const_tree_false
   self->return_in_msb = returnInMsb_cb;
   //原型 targetm.calls.end_call_args (args_so_far); #define TARGET_END_CALL_ARGS hook_void_CUMULATIVE_ARGS
   self->end_call_args = NULL;
   //原型  targetm.calls.static_chain (fndecl_or_type, false); #define TARGET_STATIC_CHAIN default_static_chain
   self->static_chain = staticChain_cb;
   //原型 (targetm.calls.split_complex_arg) #define TARGET_SPLIT_COMPLEX_ARG hook_bool_const_tree_true
   self->split_complex_arg = NULL;
   //原型 targetm.calls.strict_argument_naming (args_so_far) #define TARGET_STRICT_ARGUMENT_NAMING nvptx_strict_argument_naming
   self->strict_argument_naming = NULL;
   //原型  targetm.calls.pretend_outgoing_varargs_named (args_so_far) #define TARGET_PRETEND_OUTGOING_VARARGS_NAMED default_pretend_outgoing_varargs_named
   self->pretend_outgoing_varargs_named = pretendOutgointVarargsNamed_cb;
   //原型 targetm.calls.setup_incoming_varargs #define TARGET_SETUP_INCOMING_VARARGS default_setup_incoming_varargs
   self->setup_incoming_varargs = setupIncomingVarargs_cb;
   //原型 targetm.calls.callee_copies (pack_cumulative_args (ca), arg); #define TARGET_CALLEE_COPIES hook_bool_CUMULATIVE_ARGS_arg_info_false
   self->callee_copies = calleeCopies_cb;
   //原型 targetm.calls.pass_by_reference (pack_cumulative_args (ca), arg); #define TARGET_PASS_BY_REFERENCE nvptx_pass_by_reference
   self->pass_by_reference = NULL;
   //原型  targetm.calls.warn_parameter_passing_abi (args_so_far, type);#define TARGET_WARN_PARAMETER_PASSING_ABI hook_void_CUMULATIVE_ARGS_tree
   self->warn_parameter_passing_abi = NULL;
   //原型 targetm.calls.function_incoming_arg #define TARGET_FUNCTION_INCOMING_ARG default_function_incoming_arg
   self->function_incoming_arg = NULL;
   //原型 targetm.calls.must_pass_in_stack (arg); #define TARGET_MUST_PASS_IN_STACK must_pass_in_stack_var_size_or_pad
   self->must_pass_in_stack = NULL;
   //原型 targetm.calls.fntype_abi
   self->fntype_abi = NULL;
   //原型 targetm.calls.insn_callee_abi (insn);
   self->insn_callee_abi = NULL;
   //原型 targetm.calls.internal_arg_pointer() #define TARGET_INTERNAL_ARG_POINTER default_internal_arg_pointer
   self->internal_arg_pointer = internalArgPointer_cb;
   //原型 targetm.calls.update_stack_boundary (); #define TARGET_UPDATE_STACK_BOUNDARY NULL
   self->update_stack_boundary = NULL;
   //原型 targetm.calls.function_value_regno_p #define TARGET_FUNCTION_VALUE_REGNO_P nvptx_function_value_regno_p
   self->function_value_regno_p = NULL;
   //原型 targetm.calls.get_raw_result_mode (regno) #define TARGET_GET_RAW_RESULT_MODE default_get_reg_raw_mode
   self->get_raw_result_mode = getRawResultMode_cb;
   //原型 targetm.calls.get_raw_arg_mode (regno); #define TARGET_GET_RAW_ARG_MODE default_get_reg_raw_mode
   self->get_raw_arg_mode = getRawArgMode_cb;
   //原型 targetm.calls.expand_builtin_saveregs () #define TARGET_EXPAND_BUILTIN_SAVEREGS default_expand_builtin_saveregs
   self->expand_builtin_saveregs = expandBuiltinSavergs_cb;
   //原型 targetm.calls.emit_call_builtin___clear_cache (begin, end);
   //#define TARGET_EMIT_CALL_BUILTIN___CLEAR_CACHE default_emit_call_builtin___clear_cache
   self->emit_call_builtin___clear_cache = emitCallBuiltin__ClearCache_cb;
   //原型  targetm.calls.trampoline_init (m_tramp, t_func, r_chain) #define TARGET_TRAMPOLINE_INIT default_trampoline_init
   self->trampoline_init = trampolineInit_cb;
   //原型 targetm.calls.trampoline_adjust_address #define TARGET_TRAMPOLINE_ADJUST_ADDRESS NULL
   self->trampoline_adjust_address = NULL;
   //原型 targetm.calls.empty_record_p (type) #define TARGET_EMPTY_RECORD_P hook_bool_const_tree_false
   self->empty_record_p = emptyRecordP_cb;
   //原型 targetm.calls.get_drap_rtx #define TARGET_GET_DRAP_RTX nvptx_get_drap_rtx
   self->get_drap_rtx = NULL;
   //原型 targetm.calls.zero_call_used_regs (selected_hardregs); #define TARGET_ZERO_CALL_USED_REGS default_zero_call_used_regs
   self->zero_call_used_regs = zeroCallUsedRegs_cb;
   //原型 argetm.calls.call_offset_return_label #define TARGET_CALL_OFFSET_RETURN_LABEL hook_int_rtx_insn_0
   self->call_offset_return_label = callOffsetReturnLabel_cb;
}


//原型targetm.calls.struct_value_rtx   #define TARGET_STRUCT_VALUE_RTX hook_rtx_tree_int_null
rtx target_calls_struct_value_rtx (TargetCalls *self,tree fndecl,int incoming)
{
   return self->struct_value_rtx(self,fndecl,incoming);
}

//原型 targetm.calls.allocate_stack_slots_for_args #define TARGET_ALLOCATE_STACK_SLOTS_FOR_ARGS hook_bool_void_true
bool target_calls_allocate_stack_slots_for_args (TargetCalls *self)
{
   return self->allocate_stack_slots_for_args(self);
}

//原型 targetm.calls.promote_function_mode (NULL_TREE, mode, punsignedp, funtype,for_return);#define TARGET_PROMOTE_FUNCTION_MODE default_promote_function_mode
mtcs_mode target_calls_promote_function_mode (TargetCalls *self,const_tree type ATTRIBUTE_UNUSED, mtcs_mode mode,
                     int *punsignedp ATTRIBUTE_UNUSED,const_tree funtype ATTRIBUTE_UNUSED,int for_return ATTRIBUTE_UNUSED)
{
   return self->promote_function_mode(self,type,mode,punsignedp,funtype,for_return);
}

//原型 targetm.calls.return_in_memory (type, fntype) #define TARGET_RETURN_IN_MEMORY nvptx_return_in_memory
bool target_calls_return_in_memory (TargetCalls *self,const_tree type, const_tree fntype)
{
   return self->return_in_memory(self,type,fntype);
}

//原型targetm.calls.function_value (valtype, func ? func : fntype, outgoing); #define TARGET_FUNCTION_VALUE default_function_value
rtx target_calls_function_value (TargetCalls *self,const_tree ret_type ATTRIBUTE_UNUSED,
                        const_tree fn_decl_or_type, bool outgoing ATTRIBUTE_UNUSED)
{
   return self->function_value(self,ret_type,fn_decl_or_type,outgoing);
}

//原型 targetm.calls.libcall_value (mode, fun); #define TARGET_LIBCALL_VALUE default_libcall_value
rtx target_calls_libcall_value (TargetCalls *self,machine_mode mode, const_rtx r)
{
   return self->libcall_value(self,mode,r);
}
//原型 targetm.calls.function_arg (args_so_far, ptr_arg);#define TARGET_FUNCTION_ARG default_function_arg
rtx target_calls_function_arg (TargetCalls *self,cumulative_args_t arg, const mtcs_function_arg_info &info)
{
   return self->function_arg(self,arg,info);
}

//原型targetm.calls.function_arg_advance (args_so_far, ptr_arg);#define TARGET_FUNCTION_ARG_ADVANCE nvptx_function_arg_advance
void target_calls_function_arg_advance (TargetCalls *self,cumulative_args_t cum_v, const mtcs_function_arg_info &info)
{
   self->function_arg_advance(self,cum_v,info);
}

//原型ttargetm.calls.arg_partial_bytes (args_so_far, arg);#define TARGET_ARG_PARTIAL_BYTES hook_int_CUMULATIVE_ARGS_arg_info_0
int target_calls_arg_partial_bytes (TargetCalls *self,cumulative_args_t cum_v, const mtcs_function_arg_info &info)
{
   return self->arg_partial_bytes(self,cum_v,info);
}

//原型targetm.calls.function_arg_padding (passed_mode, type);#define TARGET_FUNCTION_ARG_PADDING default_function_arg_padding
enum pad_direction target_calls_function_arg_padding (TargetCalls *self,machine_mode mode, const_tree type)
{
   return self->function_arg_padding(self,mode,type);
}

//原型targetm.calls.function_arg_boundary (passed_mode, type);#define TARGET_FUNCTION_ARG_BOUNDARY nvptx_function_arg_boundary
unsigned target_calls_function_arg_boundary (TargetCalls *self,machine_mode mode ATTRIBUTE_UNUSED, const_tree type ATTRIBUTE_UNUSED)
{
   return self->function_arg_boundary(self,mode,type);
}

//原型targetm.calls.function_arg_round_boundary (passed_mode,type);#define TARGET_FUNCTION_ARG_ROUND_BOUNDARY default_function_arg_round_boundary
unsigned target_calls_function_arg_round_boundary (TargetCalls *self,machine_mode mode ATTRIBUTE_UNUSED, const_tree type ATTRIBUTE_UNUSED)
{
   return self->function_arg_round_boundary(self,mode,type);
}

//原型 targetm.calls.function_arg_offset (passed_mode, type);#define TARGET_FUNCTION_ARG_OFFSET default_function_arg_offset
HOST_WIDE_INT target_calls_function_arg_offset (TargetCalls *self,machine_mode mode ATTRIBUTE_UNUSED, const_tree type ATTRIBUTE_UNUSED)
{
   return self->function_arg_offset(self,mode,type);
}

//原型 targetm.calls.push_argument(unsigned int );#define TARGET_PUSH_ARGUMENT default_push_argument
bool     target_calls_push_argument (TargetCalls *self,unsigned int npush)
{
   return self->push_argument(self,npush);
}
//原型 targetm.calls.start_call_args (args_so_far); #define TARGET_START_CALL_ARGS hook_void_CUMULATIVE_ARGS
void     target_calls_start_call_args (TargetCalls *self,cumulative_args_t args)
{
   self->start_call_args(self,args);
}

//原型 targetm.calls.call_args (args_so_far, argvec[i].reg, NULL_TREE); #define TARGET_CALL_ARGS hook_void_CUMULATIVE_ARGS_rtx_tree
void     target_calls_call_args (TargetCalls *self,cumulative_args_t cargs, rtx arg, tree fntype)
{
   self->call_args(self,cargs,arg,fntype);
}

//原型 targetm.calls.return_pops_args  #define TARGET_RETURN_POPS_ARGS default_return_pops_args
poly_int64 target_calls_return_pops_args (TargetCalls *self,tree fundecl, tree funtype, poly_int64 size)
{
   return self->return_pops_args(self,fundecl,funtype,size);
}

//原型 targetm.calls.return_in_msb (tfom) #define TARGET_RETURN_IN_MSB hook_bool_const_tree_false
bool     target_calls_return_in_msb (TargetCalls *self,const_tree valtype)
{
   return self->return_in_msb(self,valtype);
}

//原型 targetm.calls.end_call_args (args_so_far); #define TARGET_END_CALL_ARGS hook_void_CUMULATIVE_ARGS
void     target_calls_end_call_args (TargetCalls *self,cumulative_args_t cargs)
{
   self->end_call_args(self,cargs);
}

//原型  targetm.calls.static_chain (fndecl_or_type, false); #define TARGET_STATIC_CHAIN default_static_chain
rtx target_calls_static_chain (TargetCalls *self,const_tree ARG_UNUSED (fndecl_or_type), bool incoming_p)
{
   return self->static_chain(self,fndecl_or_type,incoming_p);
}

//原型 (targetm.calls.split_complex_arg) #define TARGET_SPLIT_COMPLEX_ARG hook_bool_const_tree_true
bool     target_calls_split_complex_arg (TargetCalls *self,const_tree valtype)
{
   return self->split_complex_arg(self,valtype);
}

//原型 targetm.calls.strict_argument_naming (args_so_far) #define TARGET_STRICT_ARGUMENT_NAMING nvptx_strict_argument_naming
bool     target_calls_strict_argument_naming (TargetCalls *self,cumulative_args_t cum_v)
{
   return self->strict_argument_naming(self,cum_v);
}

//原型  targetm.calls.pretend_outgoing_varargs_named (args_so_far) #define TARGET_PRETEND_OUTGOING_VARARGS_NAMED default_pretend_outgoing_varargs_named
bool target_calls_pretend_outgoing_varargs_named (TargetCalls *self,cumulative_args_t ca ATTRIBUTE_UNUSED)
{
   return self->pretend_outgoing_varargs_named(self,ca);
}

//原型 targetm.calls.setup_incoming_varargs #define TARGET_SETUP_INCOMING_VARARGS default_setup_incoming_varargs
void target_calls_setup_incoming_varargs (TargetCalls *self,cumulative_args_t cum_v, const mtcs_function_arg_info &arg,
                  int *pretend_size ATTRIBUTE_UNUSED, int no_rtl)
{
   self->setup_incoming_varargs(self,cum_v,arg,pretend_size,no_rtl);
}

//原型 targetm.calls.callee_copies (pack_cumulative_args (ca), arg); #define TARGET_CALLEE_COPIES hook_bool_CUMULATIVE_ARGS_arg_info_false
bool target_calls_callee_copies (TargetCalls *self,cumulative_args_t ca, const mtcs_function_arg_info &arg)
{
   return self->callee_copies(self,ca,arg);
}

//原型 targetm.calls.pass_by_reference (pack_cumulative_args (ca), arg); #define TARGET_PASS_BY_REFERENCE nvptx_pass_by_reference
bool target_calls_pass_by_reference (TargetCalls *self,cumulative_args_t ca, const mtcs_function_arg_info &arg)
{
   return self->pass_by_reference(self,ca,arg);
}

//原型  targetm.calls.warn_parameter_passing_abi (args_so_far, type);#define TARGET_WARN_PARAMETER_PASSING_ABI hook_void_CUMULATIVE_ARGS_tree
void target_calls_warn_parameter_passing_abi (TargetCalls *self,cumulative_args_t ca ATTRIBUTE_UNUSED, tree t ATTRIBUTE_UNUSED)
{
   self->warn_parameter_passing_abi(self,ca,t);
}

//原型 targetm.calls.function_incoming_arg #define TARGET_FUNCTION_INCOMING_ARG default_function_incoming_arg
rtx target_calls_function_incoming_arg (TargetCalls *self,cumulative_args_t ca , const mtcs_function_arg_info &arg)
{
   return self->function_incoming_arg(self,ca,arg);
}

//原型 targetm.calls.must_pass_in_stack (arg); #define TARGET_MUST_PASS_IN_STACK must_pass_in_stack_var_size_or_pad
bool target_calls_must_pass_in_stack (TargetCalls *self,const mtcs_function_arg_info &arg)
{
   return self->must_pass_in_stack(self,arg);
}

//原型 targetm.calls.fntype_abi
mtcs_predefined_function_abi target_calls_fntype_abi (TargetCalls *self,const_tree type)
{
   return self->fntype_abi(self,type);
}

//原型 targetm.calls.insn_callee_abi (insn);
mtcs_function_abi target_calls_insn_callee_abi (TargetCalls *self,rtx_insn *insn)
{
   return self->insn_callee_abi(self,insn);
}

//原型 targetm.calls.internal_arg_pointer() #define TARGET_INTERNAL_ARG_POINTER default_internal_arg_pointer
rtx target_calls_internal_arg_pointer (TargetCalls *self)
{
   return self->internal_arg_pointer(self);
}
//原型 targetm.calls.update_stack_boundary (); #define TARGET_UPDATE_STACK_BOUNDARY NULL
void target_calls_update_stack_boundary (TargetCalls *self) //nvptx是空的
{
   self->update_stack_boundary(self);
}

//原型 targetm.calls.function_value_regno_p #define TARGET_FUNCTION_VALUE_REGNO_P nvptx_function_value_regno_p
bool target_calls_function_value_regno_p (TargetCalls *self,const unsigned int regno)
{
   return self->function_value_regno_p(self,regno);
}

//原型 targetm.calls.get_raw_result_mode (regno) #define TARGET_GET_RAW_RESULT_MODE default_get_reg_raw_mode
fixed_size_mode target_calls_get_raw_result_mode (TargetCalls *self,int regno)
{
   return self->get_raw_result_mode(self,regno);
}

//原型 targetm.calls.get_raw_arg_mode (regno); #define TARGET_GET_RAW_ARG_MODE default_get_reg_raw_mode
fixed_size_mode target_calls_get_raw_arg_mode (TargetCalls *self,int regno)
{
   return self->get_raw_arg_mode(self,regno);
}

//原型 targetm.calls.expand_builtin_saveregs () #define TARGET_EXPAND_BUILTIN_SAVEREGS default_expand_builtin_saveregs
rtx target_calls_expand_builtin_saveregs (TargetCalls *self)
{
   return self->expand_builtin_saveregs(self);
}
//原型 targetm.calls.emit_call_builtin___clear_cache (begin, end);
//#define TARGET_EMIT_CALL_BUILTIN___CLEAR_CACHE default_emit_call_builtin___clear_cache
void target_calls_emit_call_builtin___clear_cache (TargetCalls *self,rtx begin, rtx end)
{
   self->emit_call_builtin___clear_cache(self,begin,end);
}

//原型  targetm.calls.trampoline_init (m_tramp, t_func, r_chain) #define TARGET_TRAMPOLINE_INIT default_trampoline_init
void target_calls_trampoline_init (TargetCalls *self,rtx ARG_UNUSED (m_tramp), tree ARG_UNUSED (t_func),rtx ARG_UNUSED (r_chain))
{
   self->trampoline_init(self,m_tramp,t_func,r_chain);
}

//原型 targetm.calls.trampoline_adjust_address #define TARGET_TRAMPOLINE_ADJUST_ADDRESS NULL
rtx target_calls_trampoline_adjust_address (TargetCalls *self,rtx addr)
{
   return self->trampoline_adjust_address(self,addr);
}

//原型 targetm.calls.empty_record_p (type) #define TARGET_EMPTY_RECORD_P hook_bool_const_tree_false
bool target_calls_empty_record_p (TargetCalls *self,tree type)
{
   return self->empty_record_p(self,type);
}

//原型 targetm.calls.get_drap_rtx #define TARGET_GET_DRAP_RTX nvptx_get_drap_rtx
rtx target_calls_get_drap_rtx (TargetCalls *self)
{
   return self->get_drap_rtx(self);
}

//原型 targetm.calls.zero_call_used_regs (selected_hardregs); #define TARGET_ZERO_CALL_USED_REGS default_zero_call_used_regs
HardRegSet target_calls_zero_call_used_regs (TargetCalls *self,HardRegSet *need_zeroed_hardregs)
{
   return self->zero_call_used_regs(self,need_zeroed_hardregs);
}

//原型 argetm.calls.call_offset_return_label #define TARGET_CALL_OFFSET_RETURN_LABEL hook_int_rtx_insn_0
int target_calls_call_offset_return_label (TargetCalls *self,rtx_insn *x)
{
  return self->call_offset_return_label(self,x);
}


void target_calls_set_custom_function_descriptors(TargetCalls *self,int value)
{
    self->custom_function_descriptors = value;
}
