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

#include "../mtcstarget.h"

#include "aet/aetprinttree.h"
#include "gen/ptx-insn-modes.h"
#include "gen/ptx-optionsitem.h"
#include "gen/ptx-insn-unspec.h"
#include "ptx-common.h"
#include "ptxtool.h"
#include "mtcsptx.h"
#include "mtcsptxargs.h"
#include "mtcsptxfunc.h"

#include "targetptxcalls.h"

//原型 targetm.calls.libcall_value (mode, fun); #define TARGET_LIBCALL_VALUE default_libcall_value
static rtx libcallValue_cb/*!nvptx_libcall_value*/ (TargetCalls *targetCalls,machine_mode mode, const_rtx x);

/* A non-memory argument of mode MODE is being passed, determine the mode it
   should be promoted to.  This is also used for determining return
   type promotion.
*/
static mtcs_mode promote_arg (TargetPtxCalls *self,mtcs_mode mode, bool prototyped)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   if (!prototyped && mode == mtcsMode->modes.M_SFmode)
   /* K&R float promotion for unprototyped functions.  */
      mode = mtcsMode->modes.M_DFmode;
   else if (mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode)
         < mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mtcsMode->modes.M_SImode))
      mode = mtcsMode->modes.M_SImode;
   return mode;
}

/* A non-memory return type of MODE is being returned.  Determine the
   mode it should be promoted to.  */
static mtcs_mode promote_return (TargetPtxCalls *self,mtcs_mode mode)
{
   return promote_arg (self,mode, true);
}


//原型 targetm.calls.promote_function_mode (NULL_TREE, mode, punsignedp, funtype,for_return);#define TARGET_PROMOTE_FUNCTION_MODE default_promote_function_mode
static mtcs_mode promoteFunctionMode_cb(TargetCalls *targetCalls,const_tree type ATTRIBUTE_UNUSED, mtcs_mode mode,
        int *punsignedp ATTRIBUTE_UNUSED,const_tree funtype ATTRIBUTE_UNUSED,int for_return ATTRIBUTE_UNUSED)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(targetCalls);
   n_debug("targetptxcalls.c -----nvptx.c -----18-- targetm.calls.promote_function_mode mode:%d for_return:%d\n",mode,for_return);
   return promote_arg ((TargetPtxCalls *)targetCalls,mode, for_return || !type || TYPE_ARG_TYPES (funtype));
}

//原型 targetm.calls.return_in_memory (type, fntype) #define TARGET_RETURN_IN_MEMORY nvptx_return_in_memory
static bool returnInMemory_cb (TargetCalls *targetCalls,const_tree type,const_tree fntype ATTRIBUTE_UNUSED)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(targetCalls);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   machine_mode mode=mtcs_mode_host2device_by_tree(mtcsMode,type,TYPE_MODE (type));
   n_debug("targetptxcalls.c -----nvptx.cc -----17-- TARGET_RETURN_IN_MEMORY \
          bool nvptx_return_in_memory (const_tree type, const_tree):host:%d device:%d\n",
          TYPE_MODE (type),mode);
   return mtcs_ptx_pass_in_memory ((MtcsPtx *)mtcsTarget,mode, type, true);
}

//原型targetm.calls.function_value (valtype, func ? func : fntype, outgoing); #define TARGET_FUNCTION_VALUE default_function_value
static rtx functionValue_cb(TargetCalls *targetCalls,const_tree ret_type ATTRIBUTE_UNUSED,
            const_tree fn_decl_or_type, bool outgoing ATTRIBUTE_UNUSED)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(targetCalls);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   machine_mode deviceMode=mtcs_mode_host2device_by_tree(mtcsMode,ret_type,TYPE_MODE (ret_type));

   n_debug("targetptxcalls.c -----nvptx.cc -----122-- TARGET_FUNCTION_VALUE rtx nvptx_function_value \
       (const_tree type, const_tree ARG_UNUSED (func), bool outgoing) outgoint:%d \n",outgoing);
   machine_mode mode = promote_return ((TargetPtxCalls *)targetCalls,deviceMode);
   if (outgoing){
      gcc_assert (cfun);
      //cfun->machine->return_mode = mode;
      struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)cfun->machine;
      nvptxMachine->return_mode = mode;
      return mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mode, PTX_NVPTX_RETURN_REGNUM);
   }
   return libcallValue_cb/*!nvptx_libcall_value*/(targetCalls,mode, NULL_RTX);
}

/* Implement TARGET_LIBCALL_VALUE.  */
//原型 targetm.calls.libcall_value (mode, fun); #define TARGET_LIBCALL_VALUE default_libcall_value
static rtx libcallValue_cb/*!nvptx_libcall_value*/ (TargetCalls *targetCalls,machine_mode mode, const_rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(targetCalls);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   n_debug("targetptxcalls.c -----nvptx.cc -----121-- TARGET_LIBCALL_VALUE \
         rtx nvptx_libcall_value (machine_mode mode, const_rtx) cfun:%p\n",cfun);
   //if (!cfun || !cfun->machine->doing_call)
   if (!cfun || !((struct ptx_machine_function *)cfun->machine)->doing_call)
      /* Pretend to return in a hard reg for early uses before pseudos can be
      generated.  */
      return mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mode, PTX_NVPTX_RETURN_REGNUM);
   n_debug("targetptxcalls.c -----nvptx.cc -----121aa-- TARGET_LIBCALL_VALUE rtx nvptx_libcall_value (machine_mode mode, const_rtx)\n");
   return mtcs_emit_gen_reg_rtx (mtcsEmit,mode);
}

//确定参数的位置是从寄存器传递 匿名参数肯定不是寄存器参数
//bool end_marker_p () const { return mode == VOIDmode; }
//原型 targetm.calls.function_arg (args_so_far, ptr_arg);#define TARGET_FUNCTION_ARG default_function_arg
static rtx functionArg_cb(TargetCalls *targetCalls,cumulative_args_t info, const mtcs_function_arg_info &arg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(targetCalls);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   n_debug("targetptxcalls.c -----nvptx.cc -----119-- TARGET_FUNCTION_ARG nvptx_function_arg 返回空是栈参数否则是寄存器参数mode:%d type:%p 是不是命名参数:%d\n",
         arg.mode,arg.type,arg.named);
   if (arg.end_marker_p () || !arg.named)
     return NULL_RTX;
   return mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/ (mtcsEmit,arg.mode);
}

/* Implement TARGET_FUNCTION_ARG_ADVANCE.  */
//原型 nvptx_function_arg_advance
//原型targetm.calls.function_arg_advance (args_so_far, ptr_arg);#define TARGET_FUNCTION_ARG_ADVANCE nvptx_function_arg_advance
static void functionArgAdvance_cb(TargetCalls *targetCalls,cumulative_args_t cum_v, const mtcs_function_arg_info &info)
{
   n_debug("targetptxcalls.c -----nvptx.cc -----10-- TARGET_FUNCTION_ARG_ADVANCE nvptx_function_arg_advance\
         (cumulative_args_t cum_v, const mtcs_function_arg_info &)\n");
   //CUMULATIVE_ARGS *cum = get_cumulative_args (cum_v);
   MtcsPtxCumulativeArgs *cum = (MtcsPtxCumulativeArgs *)get_cumulative_args (cum_v);
   cum->count++;
}

/* Implement TARGET_FUNCTION_ARG_BOUNDARY.
   For nvptx This is only used for variadic args.  The type has already
   been promoted and/or converted to invisible reference.  */
//原型targetm.calls.function_arg_boundary (passed_mode, type);#define TARGET_FUNCTION_ARG_BOUNDARY nvptx_function_arg_boundary
static unsigned functionArgBoundary_cb(TargetCalls *targetCalls,machine_mode mode ATTRIBUTE_UNUSED, const_tree type ATTRIBUTE_UNUSED)
{
    n_debug("targetptxcalls.c -----nvptx.cc -----11-- TARGET_FUNCTION_ARG_BOUNDARY unsigned nvptx_function_arg_boundary mode:%d tree:%s\n",
           mode,get_tree_code_name(TREE_CODE(type)));
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(targetCalls);
    return mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode);
}

//原型 targetm.calls.push_argument(unsigned int );#define TARGET_PUSH_ARGUMENT default_push_argument
static bool pushArgument_cb(TargetCalls *targetCalls,unsigned int npush)
{
   return false;
}

//原型 targetm.calls.call_args (args_so_far, argvec[i].reg, NULL_TREE); #define TARGET_CALL_ARGS hook_void_CUMULATIVE_ARGS_rtx_tree
static   void callArgs_cb(TargetCalls *targetCalls,cumulative_args_t cargs, rtx arg, tree fntype)
{
   n_debug("targetptxcalls.c -----nvptx.cc -----37-- TARGET_CALL_ARGS void nvptx_call_args (cumulative_args_t, rtx arg, tree fntype)\n");
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(targetCalls);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   //  struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)mtcsFunc->machine;
   struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)cfun->machine;

   if (!nvptxMachine->doing_call){
      nvptxMachine->doing_call = true;
      nvptxMachine->is_variadic = false;
      nvptxMachine->num_args = 0;

      if (fntype && stdarg_p (fntype)){
         nvptxMachine->is_variadic = true;
         nvptxMachine->has_variadic = true;
         nvptxMachine->num_args++;
      }
   }

   if (REG_P (arg) && arg != pc_rtx){
      nvptxMachine->num_args++;
      nvptxMachine->call_args = alloc_EXPR_LIST (VOIDmode, arg,nvptxMachine->call_args);
   }
}

/* Implement the corresponding END_CALL_ARGS hook.  Clear and free the
   information we recorded.  */
//原型 targetm.calls.end_call_args (args_so_far); #define TARGET_END_CALL_ARGS hook_void_CUMULATIVE_ARGS
static void endCallArgs_cb(TargetCalls *targetCalls,cumulative_args_t args)
{
    n_debug("targetptxcalls.c -----nvptx.cc -----38-- TARGET_END_CALL_ARGS void nvptx_end_call_args (cumulative_args_t)\n");
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(targetCalls);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
    //struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)mtcsFunc->machine;
    struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)cfun->machine;

    nvptxMachine/*!cfun->machine->doing_call*/->doing_call = false;
    free_EXPR_LIST_list (&nvptxMachine/*!&cfun->machine->call_args*/->call_args);
}

//原型  targetm.calls.static_chain (fndecl_or_type, false); #define TARGET_STATIC_CHAIN default_static_chain
static rtx staticChain_cb(TargetCalls *targetCalls,const_tree ARG_UNUSED (fndecl_or_type), bool incoming_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(targetCalls);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   mtcs_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
   //  if (incoming_p){
   //#ifdef STATIC_CHAIN_INCOMING_REGNUM //howt=0 nvptx=0
   //      return gen_rtx_REG (pMode, STATIC_CHAIN_INCOMING_REGNUM);
   //#endif
   //  }

   //#ifdef STATIC_CHAIN_REGNUM //host=0 nvptx=1
   return mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,pMode, PTX_STATIC_CHAIN_REGNUM);
   //#endif

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

//原型 (targetm.calls.split_complex_arg) #define TARGET_SPLIT_COMPLEX_ARG hook_bool_const_tree_true
static bool splitComplexArg_cb(TargetCalls *targetCalls,const_tree valtype)
{
    return true;
}

//原型 targetm.calls.strict_argument_naming (args_so_far) #define TARGET_STRICT_ARGUMENT_NAMING nvptx_strict_argument_naming
static bool strictArgumentNaming_cb (TargetCalls *targetCalls,cumulative_args_t cum_v)
{
   n_debug("targetptxcalls.c -----nvptx.cc -----12-- TARGET_STRICT_ARGUMENT_NAMING bool nvptx_strict_argument_naming (cumulative_args_t cum_v)\n");
   MtcsPtxCumulativeArgs *cum = (MtcsPtxCumulativeArgs *)get_cumulative_args (cum_v);
   return cum->fntype == NULL_TREE || stdarg_p (cum->fntype);
}


//原型 targetm.calls.pass_by_reference (pack_cumulative_args (ca), arg); #define TARGET_PASS_BY_REFERENCE nvptx_pass_by_reference
static bool passByReference_cb (TargetCalls *targetCalls,cumulative_args_t ca, const mtcs_function_arg_info &arg)
{
  n_debug("targetptxcalls.c -----nvptx.cc -----16-- TARGET_PASS_BY_REFERENCE bool nvptx_pass_by_reference arg.mode:%d arg.type:%p\n",
          arg.mode,arg.type);
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(targetCalls);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsPtx *mtcsPtx=(MtcsPtx *)mtcsTarget;

  return mtcs_ptx_pass_in_memory/*!pass_in_memory*/(mtcsPtx,arg.mode, arg.type, false);

}

//原型  targetm.calls.warn_parameter_passing_abi (args_so_far, type);#define TARGET_WARN_PARAMETER_PASSING_ABI hook_void_CUMULATIVE_ARGS_tree
//hook_void_CUMULATIVE_ARGS_tree targhooks.cc
static void warnParameterPassingAbi_cb(TargetCalls *targetCalls,cumulative_args_t ca ATTRIBUTE_UNUSED, tree ATTRIBUTE_UNUSED)
{

}

//原型 targetm.calls.function_incoming_arg #define TARGET_FUNCTION_INCOMING_ARG default_function_incoming_arg
//原型 rtx default_function_incoming_arg (cumulative_args_t, const mtcs_function_arg_info &) targhooks.cc
static rtx functionIncomingArg_cb(TargetCalls *targetCalls,cumulative_args_t cum_v, const mtcs_function_arg_info & arg)
{
  n_debug("targetptxcalls.c -----nvptx.cc -----120-- TARGET_FUNCTION_INCOMING_ARG marker_p:%d named:%d mode:%d \n",
          arg.end_marker_p (),arg.named,arg.mode);
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(targetCalls);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsPtxCumulativeArgs *cum = (MtcsPtxCumulativeArgs *)get_cumulative_args (cum_v);
  if (arg.end_marker_p () || !arg.named)
    return NULL_RTX;
  /* No need to deal with split modes here, the only case that can
     happen is complex modes and those are dealt with by
     TARGET_SPLIT_COMPLEX_ARG.  */
  return gen_rtx_UNSPEC (arg.mode, gen_rtvec (1, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,cum->count)),PTX_UNSPEC_ARG_REG/*!UNSPEC_ARG_REG*/);
}

//原型 targetm.calls.must_pass_in_stack (arg); #define TARGET_MUST_PASS_IN_STACK must_pass_in_stack_var_size_or_pad
//原型 must_pass_in_stack_var_size_or_pad calls.h calls.cc
static bool mustPassInStack_cb(TargetCalls *targetCalls,const mtcs_function_arg_info &arg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(targetCalls);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   return mtcs_calls_must_pass_in_stack_var_size_or_pad/*!must_pass_in_stack_var_size_or_pad*/(mtcsCalls,arg);
}

//原型 targetm.calls.function_value_regno_p #define TARGET_FUNCTION_VALUE_REGNO_P nvptx_function_value_regno_p
static bool functionValueRegnoP_cb(TargetCalls *targetCalls,const unsigned int regno)
{
   n_debug("targetptxcalls.c -----nvptx.cc -----15-- TARGET_FUNCTION_VALUE_REGNO_P bool nvptx_function_value_regno_p regno:%d\n",regno);
   return regno == PTX_NVPTX_RETURN_REGNUM;
}

//原型 targetm.calls.get_drap_rtx #define TARGET_GET_DRAP_RTX nvptx_get_drap_rtx
static rtx getDrapRtx_cb(TargetCalls *targetCalls)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(targetCalls);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   int tss=mtcs_options_target_soft_stack/*!TARGET_SOFT_STACK*/(mtcsOptions);
   //原型 #define stack_realign_drap (crtl->stack_realign_needed && crtl->need_drap)
   bool srd=mtcs_align_stack_realign_drap/*!stack_realign_drap*/(mtcsAlign);
   n_debug("targetptxcalls.c -----nvptx.cc -----123-- TARGET_GET_DRAP_RTX rtx nvptx_get_drap_rtx (void) TARGET_SOFT_STACK:%d stack_realign_drap:%d\n",
         tss,srd);
   if (tss/*!TARGET_SOFT_STACK*/ && srd/*!stack_realign_drap*/)
      return mtcs_rtl_get_arg_pointer_rtx/*!arg_pointer_rtx*/(mtcsRTL);
   return NULL_RTX;
}


static void targetPtxCallsInit(TargetPtxCalls *self)
{
   TargetCalls *targetCalls =(TargetCalls *)self;
   //原型 targetm.calls.omit_struct_return_reg #define TARGET_OMIT_STRUCT_RETURN_REG true
   targetCalls->omit_struct_return_reg = true;
   //原型 targetm.calls.promote_function_mode (NULL_TREE, mode, punsignedp, funtype,for_return);#define TARGET_PROMOTE_FUNCTION_MODE default_promote_function_mode
   targetCalls->promote_function_mode = promoteFunctionMode_cb;
   //原型 targetm.calls.return_in_memory (type, fntype) #define TARGET_RETURN_IN_MEMORY nvptx_return_in_memory
   targetCalls->return_in_memory = returnInMemory_cb;
   //原型targetm.calls.function_value (valtype, func ? func : fntype, outgoing); #define TARGET_FUNCTION_VALUE default_function_value
   targetCalls->function_value = functionValue_cb;
   //原型 targetm.calls.libcall_value (mode, fun); #define TARGET_LIBCALL_VALUE default_libcall_value
   targetCalls->libcall_value = libcallValue_cb;
   //原型 targetm.calls.function_arg (args_so_far, ptr_arg);#define TARGET_FUNCTION_ARG default_function_arg
   targetCalls->function_arg = functionArg_cb ;
   //原型targetm.calls.function_arg_advance (args_so_far, ptr_arg);#define TARGET_FUNCTION_ARG_ADVANCE nvptx_function_arg_advance
   targetCalls->function_arg_advance = functionArgAdvance_cb;
   //原型targetm.calls.function_arg_boundary (passed_mode, type);#define TARGET_FUNCTION_ARG_BOUNDARY nvptx_function_arg_boundary
   targetCalls->function_arg_boundary = functionArgBoundary_cb;
   //原型 targetm.calls.push_argument(unsigned int );#define TARGET_PUSH_ARGUMENT default_push_argument
   targetCalls->push_argument = pushArgument_cb;
   //原型 targetm.calls.call_args (args_so_far, argvec[i].reg, NULL_TREE); #define TARGET_CALL_ARGS hook_void_CUMULATIVE_ARGS_rtx_tree
   targetCalls->call_args = callArgs_cb;
   //原型 targetm.calls.end_call_args (args_so_far); #define TARGET_END_CALL_ARGS hook_void_CUMULATIVE_ARGS
   targetCalls->end_call_args = endCallArgs_cb;
   //原型  targetm.calls.static_chain (fndecl_or_type, false); #define TARGET_STATIC_CHAIN default_static_chain
   targetCalls->static_chain = staticChain_cb;
   //原型 (targetm.calls.split_complex_arg) #define TARGET_SPLIT_COMPLEX_ARG hook_bool_const_tree_true
   targetCalls->split_complex_arg = splitComplexArg_cb;
   //原型 targetm.calls.strict_argument_naming (args_so_far) #define TARGET_STRICT_ARGUMENT_NAMING nvptx_strict_argument_naming
   targetCalls->strict_argument_naming = strictArgumentNaming_cb;
   //原型 targetm.calls.pass_by_reference (pack_cumulative_args (ca), arg); #define TARGET_PASS_BY_REFERENCE nvptx_pass_by_reference
   targetCalls->pass_by_reference = passByReference_cb;
   //原型  targetm.calls.warn_parameter_passing_abi (args_so_far, type);#define TARGET_WARN_PARAMETER_PASSING_ABI hook_void_CUMULATIVE_ARGS_tree
   targetCalls->warn_parameter_passing_abi = warnParameterPassingAbi_cb;
   //原型 targetm.calls.function_incoming_arg #define TARGET_FUNCTION_INCOMING_ARG default_function_incoming_arg
   targetCalls->function_incoming_arg = functionIncomingArg_cb;
   //原型 targetm.calls.must_pass_in_stack (arg); #define TARGET_MUST_PASS_IN_STACK must_pass_in_stack_var_size_or_pad
   targetCalls->must_pass_in_stack = mustPassInStack_cb;
   //原型 targetm.calls.function_value_regno_p #define TARGET_FUNCTION_VALUE_REGNO_P nvptx_function_value_regno_p
   targetCalls->function_value_regno_p = functionValueRegnoP_cb;
   //原型 targetm.calls.get_drap_rtx #define TARGET_GET_DRAP_RTX nvptx_get_drap_rtx
   targetCalls->get_drap_rtx = getDrapRtx_cb;
}

TargetPtxCalls *target_ptx_calls_new(MtcsMode *mtcsMode)
{
   TargetPtxCalls *self = n_slice_alloc0 (sizeof(TargetPtxCalls));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   target_calls_init((TargetCalls *)self);
   targetPtxCallsInit(self);
   return self;
}

