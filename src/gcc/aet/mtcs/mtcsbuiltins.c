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
 * base on builtins.cc
 */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "predict.h"
#include "memmodel.h"
#include "tm_p.h"
#include "ssa.h"
#include "optabs.h"
#include "expmed.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "alias.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "attribs.h"
#include "varasm.h"
#include "except.h"
#include "insn-attr.h"
#include "dojump.h"
#include "explow.h"
#include "calls.h"
#include "stmt.h"
/* Include expr.h after insn-config.h so we get HAVE_conditional_move.  */
#include "expr.h"
#include "optabs-tree.h"
#include "libfuncs.h"
#include "reload.h"
#include "langhooks.h"
#include "common/common-target.h"
#include "tree-dfa.h"
#include "tree-ssa-live.h"
#include "tree-outof-ssa.h"
#include "tree-ssa-address.h"
#include "builtins.h"
#include "ccmp.h"
#include "gimple-iterator.h"
#include "gimple-fold.h"
#include "rtx-vector-builder.h"
#include "tree-pretty-print.h"
#include "flags.h"
#include "stringpool.h"
#include "tree-vrp.h"
#include "tree-ssanames.h"
#include "diagnostic-core.h"
#include "fold-const-call.h"
#include "gimple-ssa-warn-access.h"
#include "tree-object-size.h"
#include "tree-ssa-strlen.h"
#include "realmpfr.h"
#include "cfgrtl.h"
#include "output.h"
#include "typeclass.h"
#include "value-prof.h"
#include "asan.h"
#include "internal-fn.h"
#include "case-cfn-macros.h"
#include "intl.h"
#include "file-prefix-map.h" /* remap_macro_filename()  */
#include "ipa-strub.h" /* strub_watermark_parm()  */
#include "gomp-constants.h"
#include "omp-general.h"
#include "gimple-ssa.h"
#include "attr-fnspec.h"
#include "demangle.h"
#include "gimple-range.h"
#include "pointer-query.h"

#include "mtcsbuiltins.h"
#include "mtcstarget.h"
#include "mtcscompile.h"
#include "../mtcsinfo.h"
#include "../aetprinttree.h"
#include "mtcsprintrtl.h"

static int flag_isoc2y=0;//原来声明在c-common.h mtcsbuiltins.c不能引入c-ommon.h。所以定义在这里供builtins.def引用

#define MTCS_GEN_FCN(CODE) (mtcsOutput->insn_data[CODE].genfun)

//原型 expand_builtin_memset_args builtins.cc
static rtx expand_builtin_memset_args (MtcsBuiltins *self,tree dest, tree val, tree len,
             rtx target, machine_mode mode, tree orig_exp);
//原型 expand_builtin_mempcpy_args builtins.cc
static rtx expand_builtin_mempcpy_args (MtcsBuiltins *self,tree dest, tree src, tree len,
              rtx target, tree orig_exp, memop_ret retmode);
//原型 apply_args_size builtins.cc
static int apply_args_size (MtcsBuiltins *self);

/* Built-in functions to perform an untyped call and return.  */

/* Wrapper that implicitly applies a delta when getting or setting the
   enclosed value.  */
template <typename T>
class delta_type
{
  T &value; T const delta;
public:
  delta_type (T &val, T dlt) : value (val), delta (dlt) {}
  operator T () const { return value + delta; }
  T operator = (T val) const { value = val - delta; return val; }
};


#define saved_apply_args_size \
  (delta_type<int> (self->x_apply_args_size_plus_one, -1))
#define apply_args_mode \
  (self->x_apply_args_mode)
#define saved_apply_result_size \
  (delta_type<int> (self->x_apply_result_size_plus_one, -1))
#define apply_result_mode \
  (self->x_apply_result_mode)


void mtcs_builtins_init(MtcsBuiltins *self)
{
    self->setjmp_alias_set=-1;
}


/* Similar to save_expr, but assumes that arbitrary code is not executed
   in between the multiple evaluations.  In particular, we assume that a
   non-addressable local variable will not be modified.  */
//原型 builtin_save_expr builtins.cc
static tree builtin_save_expr (tree exp)
{
   if (TREE_CODE (exp) == SSA_NAME
   || (TREE_ADDRESSABLE (exp) == 0
   && (TREE_CODE (exp) == PARM_DECL
   || (VAR_P (exp) && !TREE_STATIC (exp)))))
      return exp;

   return save_expr (exp);
}

/* Conveniently construct a function call expression.  FNDECL names the
   function to be called, N is the number of arguments, and the "..."
   parameters are the argument expressions.  Unlike build_call_exr
   this doesn't fold the call, hence it will always return a CALL_EXPR.  */
//原型 build_call_nofold_loc  butilins.cc
static tree build_call_nofold_loc (MtcsBuiltins *self,location_t loc, tree fndecl, int n, ...)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   va_list ap;
   tree fntype = TREE_TYPE (fndecl);
   tree fn = build1 (ADDR_EXPR, mtcs_tree_build_pointer_type/*!build_pointer_type*/(mtcsTree,fntype), fndecl);

   va_start (ap, n);
   fn = build_call_valist (TREE_TYPE (fntype), fn, n, ap);
   va_end (ap);
   SET_EXPR_LOCATION (fn, loc);
   return fn;
}

//原型 more_const_call_expr_args_p builtins.cc
static inline bool more_const_call_expr_args_p (const const_call_expr_arg_iterator *iter)
{
  return (iter->i < iter->n);
}

/* Validate a single argument ARG against a tree code CODE representing
   a type.  Return true when argument is valid.  */
//原型 validate_arg builtins.cc
static bool validate_arg (const_tree arg, enum tree_code code)
{
  if (!arg)
    return false;
  else if (code == POINTER_TYPE)
    return POINTER_TYPE_P (TREE_TYPE (arg));
  else if (code == INTEGER_TYPE)
    return INTEGRAL_TYPE_P (TREE_TYPE (arg));
  return code == TREE_CODE (TREE_TYPE (arg));
}


/* This function validates the types of a function call argument list
   against a specified list of tree_codes.  If the last specifier is a 0,
   that represents an ellipsis, otherwise the last specifier must be a
   VOID_TYPE.  */
//原型 validate_arglist builtins.cc
static bool validate_arglist (const_tree callexpr, ...)
{
   enum tree_code code;
   bool res = 0;
   va_list ap;
   const_call_expr_arg_iterator iter;
   const_tree arg;

   va_start (ap, callexpr);
   init_const_call_expr_arg_iterator (callexpr, &iter);

   /* Get a bitmap of pointer argument numbers declared attribute nonnull.  */
   tree fn = CALL_EXPR_FN (callexpr);
   bitmap argmap = get_nonnull_args (TREE_TYPE (TREE_TYPE (fn)));

   for (unsigned argno = 1; ; ++argno){
      code = (enum tree_code) va_arg (ap, int);
      switch (code){
         case 0:
            /* This signifies an ellipses, any further arguments are all ok.  */
            res = true;
            goto end;
         case VOID_TYPE:
            /* This signifies an endlink, if no arguments remain, return
            true, otherwise return false.  */
            res = !more_const_call_expr_args_p (&iter);
            goto end;
         case POINTER_TYPE:
            /* The actual argument must be nonnull when either the whole
            called function has been declared nonnull, or when the formal
            argument corresponding to the actual argument has been.  */
            if (argmap    && (bitmap_empty_p (argmap) || bitmap_bit_p (argmap, argno))){
               arg = next_const_call_expr_arg (&iter);
               if (!validate_arg (arg, code) || integer_zerop (arg))
                  goto end;
               break;
            }
         /* FALLTHRU */
         default:
            /* If no parameters remain or the parameter's code does not
            match the specified code, return false.  Otherwise continue
            checking any remaining arguments.  */
            arg = next_const_call_expr_arg (&iter);
            if (!validate_arg (arg, code))
               goto end;
            break;
      }
   }

   /* We need gotos here since we can only have one VA_CLOSE in a
   function.  */
end: ;
   va_end (ap);

   BITMAP_FREE (argmap);
   return res;
}


/* Expand EXP, a call to fabs, fabsf or fabsl.
   Return NULL_RTX if a normal call should be emitted rather than expanding
   the function inline.  If convenient, the result should be placed
   in TARGET.  SUBTARGET may be used as the target for computing
   the operand.  */
//原型 expand_builtin_fabs builtins.cc
static rtx expand_builtin_fabs (MtcsBuiltins *self,tree exp, rtx target, rtx subtarget)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   machine_mode mode;
   tree arg;
   rtx op0;

   if (!validate_arglist (exp, REAL_TYPE, VOID_TYPE))
      return NULL_RTX;

   arg = CALL_EXPR_ARG (exp, 0);
   CALL_EXPR_ARG (exp, 0) = arg = builtin_save_expr (arg);
   mode = TYPE_MODE (TREE_TYPE (arg));
   mode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (arg),mode);
   op0 = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,arg, subtarget, VOIDmode, EXPAND_NORMAL);
   return mtcs_optabs_expand_abs/*!expand_abs*/(mtcsOptabs,mode, op0, target, 0,
                                       mtcs_expr_safe_from_p/*!safe_from_p*/(mtcsExpr,target, arg, 1));
}

//原型 expand_builtin_trap builtins.h builtins.cc
void mtcs_builtins_expand_builtin_trap (MtcsBuiltins *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  if (target_rtx_have_trap/*!targetm.have_trap*/(mtcsMachine->tmrtx)){
      rtx_insn *insn = mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,
            target_rtx_gen_trap/*!targetm.gen_trap*/(mtcsMachine->tmrtx));
      /* For trap insns when not accumulating outgoing args force
     REG_ARGS_SIZE note to prevent crossjumping of calls with
     different args sizes.  */
      if (!mtcs_func_is_accumulate_outgoing_args/*!ACCUMULATE_OUTGOING_ARGS*/(mtcsFunc))
          mtcs_rtlanal_add_args_size_note/*!add_args_size_note*/(mtcsRtlanal,insn,
                  mtcsRtlData->expr.x_stack_pointer_delta/*!stack_pointer_delta*/ );
  }else{
      tree fn = builtin_decl_implicit (BUILT_IN_ABORT);
      tree call_expr = mtcs_tree_build_call_expr/*!build_call_expr*/(mtcsTree,fn, 0);
      mtcs_calls_expand_call/*!expand_call*/(mtcsCalls,call_expr, NULL_RTX, false);
  }
  mtcs_emit_emit_barrier/*!emit_barrier*/(mtcsEmit);
}

/* Construct the trailing part of a __builtin_setjmp call.  This is
   also called directly by the SJLJ exception handling code.
   If RECEIVER_LABEL is NULL, instead contruct a nonlocal goto handler.  */
//原型 expand_builtin_setjmp_receiver builtins.h builtins.cc
void mtcs_builtins_expand_builtin_setjmp_receiver (MtcsBuiltins *self,rtx receiver_label)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  rtx chain;
  /* Mark the FP as used when we get here, so we have to make sure it's
     marked as used by this function.  */
  mtcs_emit_emit_use/*!emit_use*/(mtcsEmit,mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL));
  /* Mark the static chain as clobbered here so life information
     doesn't get messed up for it.  */
  chain =mtcs_calls_rtx_for_static_chain/*!rtx_for_static_chain*/(mtcsCalls,current_function_decl, true);
  if (chain && REG_P (chain))
      mtcs_emit_emit_clobber/*!emit_clobber*/(mtcsEmit,chain);
  int argPointerRegnum=mtcs_reg_get_arg_pointer_regnum/*!ARG_POINTER_REGNUM*/(mtcsReg);
  if (!mtcs_reg_hard_frame_pointer_is_arg_pointer/*!HARD_FRAME_POINTER_IS_ARG_POINTER*/(mtcsReg)
          && mtcsReg->hardRegs.x_fixed_regs[argPointerRegnum/*!ARG_POINTER_REGNUM*/]/*!fixed_regs[ARG_POINTER_REGNUM]*/){
      /* If the argument pointer can be eliminated in favor of the
     frame pointer, we don't need to restore it.  We assume here
     that if such an elimination is present, it can always be used.
     This is the case on all known machines; if we don't make this
     assumption, we do unnecessary saving on many machines.  */
      size_t i;
      for (i = 0; i <mtcsReg->elimiableRegsCount/*!ARRAY_SIZE (elim_regs)*/; i++)
        if (mtcsReg->eliminableRegs[i].from/*!elim_regs[i].from*/ ==argPointerRegnum/*!ARG_POINTER_REGNUM*/
            && mtcsReg->eliminableRegs[i].to/*elim_regs[i].to*/ ==
                    mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg))
          break;

      if (i == mtcsReg->elimiableRegsCount/*!ARRAY_SIZE (elim_regs)*/){
          /* Now restore our arg pointer from the address at which it
             was saved in our stack frame.  */
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mtcsRtlData/*!crtl*/->args.internal_arg_pointer,
                  mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,
                        mtcs_func_get_arg_pointer_save_area/*!get_arg_pointer_save_area*/(mtcsFunc)));
      }
  }

  if (receiver_label != NULL
          && target_rtx_have_builtin_setjmp_receiver/*!targetm.have_builtin_setjmp_receiver*/(mtcsMachine->tmrtx))
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,
            target_rtx_gen_builtin_setjmp_receiver/*!targetm.gen_builtin_setjmp_receiver*/(mtcsMachine->tmrtx,receiver_label));
  else if (target_rtx_have_nonlocal_goto_receiver/*!targetm.have_nonlocal_goto_receiver*/(mtcsMachine->tmrtx))
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,
              target_rtx_gen_nonlocal_goto_receiver/*!targetm.gen_nonlocal_goto_receiver*/(mtcsMachine->tmrtx));
  else
    { /* Nothing */ }

  /* We must not allow the code we just generated to be reordered by
     scheduling.  Specifically, the update of the frame pointer must
     happen immediately, not later.  */
  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_blockage ());
}

/* Construct the leading half of a __builtin_setjmp call.  Control will
   return to RECEIVER_LABEL.  This is also called directly by the SJLJ
   exception handling code.  */
//原型 expand_builtin_setjmp_setup builtins.h builtins.cc
void mtcs_builtins_expand_builtin_setjmp_setup (MtcsBuiltins *self,rtx buf_addr, rtx receiver_label)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  machine_mode sa_mode = mtcs_mode_get_stack_savearea_mode/*!STACK_SAVEAREA_MODE*/(mtcsMode,SAVE_NONLOCAL);
  machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
  scalar_int_mode scalarPMode=mtcs_mode_as_a<scalar_int_mode>(mtcsMode,pMode);

  rtx stack_save;
  rtx mem;
  if (self->setjmp_alias_set == -1)
    self->setjmp_alias_set = mtcs_alias_new_alias_set/*!new_alias_set*/(mtcsAlias);

  buf_addr = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,scalarPMode, buf_addr);
  buf_addr = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,pMode,
          mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,buf_addr, NULL_RTX));
  /* We store the frame pointer and the address of receiver_label in
     the buffer and use the rest of it for the stack save area, which
     is machine-dependent.  */
  mem = gen_rtx_MEM (pMode, buf_addr);
  mtcs_rtl_set_mem_alias_set/*!set_mem_alias_set*/(mtcsRTL,mem, self->setjmp_alias_set);
  mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mem,
          mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL));

  mem = gen_rtx_MEM (pMode, mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, buf_addr,
                       mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,pMode))),
                               mtcs_rtl_set_mem_alias_set/*!set_mem_alias_set*/(mtcsRTL,mem, self->setjmp_alias_set);

  mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,mem),
          mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,pMode, gen_rtx_LABEL_REF (pMode, receiver_label)));

  stack_save = gen_rtx_MEM (sa_mode,
          mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, buf_addr,
                       2 * mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,pMode)));
  mtcs_rtl_set_mem_alias_set/*!set_mem_alias_set*/(mtcsRTL,stack_save, self->setjmp_alias_set);
  mtcs_explow_emit_stack_save/*!emit_stack_save*/(mtcsExplow,SAVE_NONLOCAL, &stack_save);
  /* If there is further processing to do, do it.  */
  if (target_rtx_have_builtin_setjmp_receiver/*!targetm.have_builtin_setjmp_setup*/(mtcsMachine->tmrtx))
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,
            target_rtx_gen_builtin_setjmp_setup/*!targetm.gen_builtin_setjmp_setup*/(mtcsMachine->tmrtx,buf_addr));
  /* We have a nonlocal label.   */
  cfun->has_nonlocal_label = 1;
}

/* __builtin_update_setjmp_buf is passed a pointer to an array of five words
   (not all will be used on all machines) that was passed to __builtin_setjmp.
   It updates the stack pointer in that block to the current value.  This is
   also called directly by the SJLJ exception handling code.  */
//原型 expand_builtin_update_setjmp_buf builtins.h builtins.cc
void mtcs_builtins_expand_builtin_update_setjmp_buf (MtcsBuiltins *self,rtx buf_addr)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

  machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
  scalar_int_mode scalarPMode=mtcs_mode_as_a<scalar_int_mode>(mtcsMode,pMode);
  machine_mode sa_mode = mtcs_mode_get_stack_savearea_mode/*!STACK_SAVEAREA_MODE*/(mtcsMode,SAVE_NONLOCAL);
  buf_addr = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,scalarPMode, buf_addr);
  rtx stack_save= gen_rtx_MEM (sa_mode,
          mtcs_explow_memory_address/*!memory_address*/(mtcsExplow,sa_mode,
                  mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, buf_addr,
                   2 * mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,pMode))));

  mtcs_explow_emit_stack_save/*!emit_stack_save*/(mtcsExplow,SAVE_NONLOCAL, &stack_save);
}

/* Expand EXP, a call to copysign, copysignf, or copysignl.
   Return NULL is a normal call should be emitted rather than expanding the
   function inline.  If convenient, the result should be placed in TARGET.
   SUBTARGET may be used as the target for computing the operand.  */
//原型 expand_builtin_copysign builtins.cc
static rtx expand_builtin_copysign (MtcsBuiltins *self,tree exp, rtx target, rtx subtarget)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

  rtx op0, op1;
  tree arg;

  if (!validate_arglist (exp, REAL_TYPE, REAL_TYPE, VOID_TYPE))
    return NULL_RTX;

  arg = CALL_EXPR_ARG (exp, 0);
  op0 = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,arg, subtarget, VOIDmode, EXPAND_NORMAL);

  arg = CALL_EXPR_ARG (exp, 1);
  op1 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg);
  return mtcs_optabs_expand_copysign/*!expand_copysign*/(mtcsOptabs,op0, op1, target);
}

/* Expand a call to the builtin trinary math functions (fma).
   Return NULL_RTX if a normal call should be emitted rather than expanding the
   function in-line.  EXP is the expression that is a call to the builtin
   function; if convenient, the result should be placed in TARGET.
   SUBTARGET may be used as the target for computing one of EXP's
   operands.  */
//原型 expand_builtin_mathfn_ternary builtins.cc
static rtx expand_builtin_mathfn_ternary (MtcsBuiltins *self,tree exp, rtx target, rtx subtarget)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

  optab builtin_optab;
  rtx op0, op1, op2, result;
  rtx_insn *insns;
  tree fndecl = get_callee_fndecl (exp);
  tree arg0, arg1, arg2;
  machine_mode mode;

  if (!validate_arglist (exp, REAL_TYPE, REAL_TYPE, REAL_TYPE, VOID_TYPE))
    return NULL_RTX;

  arg0 = CALL_EXPR_ARG (exp, 0);
  arg1 = CALL_EXPR_ARG (exp, 1);
  arg2 = CALL_EXPR_ARG (exp, 2);

  switch (DECL_FUNCTION_CODE (fndecl)){
    CASE_FLT_FN (BUILT_IN_FMA):
    CASE_FLT_FN_FLOATN_NX (BUILT_IN_FMA):
      builtin_optab = fma_optab; break;
    default:
      gcc_unreachable ();
  }
  /* Make a suitable register to place result in.  */
  mode = TYPE_MODE (TREE_TYPE (exp));
  mode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (exp),mode);

  /* Before working hard, check whether the instruction is available.  */
  if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,builtin_optab, mode) == CODE_FOR_nothing)
    return NULL_RTX;

  result = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

  /* Always stabilize the argument list.  */
  CALL_EXPR_ARG (exp, 0) = arg0 = builtin_save_expr (arg0);
  CALL_EXPR_ARG (exp, 1) = arg1 = builtin_save_expr (arg1);
  CALL_EXPR_ARG (exp, 2) = arg2 = builtin_save_expr (arg2);

  op0 =  mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,arg0, subtarget, VOIDmode, EXPAND_NORMAL);
  op1 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg1);
  op2 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg2);

  mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
  /* Compute into RESULT.
     Set RESULT to wherever the result comes back.  */
  result = mtcs_optabs_expand_ternary_op/*!expand_ternary_op*/(mtcsOptabs,mode, builtin_optab, op0, op1, op2,
               result, 0);
  /* If we were unable to expand via the builtin, stop the sequence
     (without outputting the insns) and call to the library function
     with the stabilized argument list.  */
  if (result == 0){
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      return mtcs_calls_expand_call/*!expand_call*/(mtcsCalls,exp, target, target == const0_rtx);
  }

  /* Output the entire sequence.  */
  insns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
  mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,insns);
  return result;
}

/* Given an interclass math builtin decl FNDECL and it's argument ARG
   return an RTL instruction code that implements the functionality.
   If that isn't possible or available return CODE_FOR_nothing.  */
//原型 interclass_mathfn_icode builtins.cc
static enum insn_code interclass_mathfn_icode (MtcsBuiltins *self,tree arg, tree fndecl)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

  bool errno_set = false;
  optab builtin_optab = unknown_optab;
  machine_mode mode;

  switch (DECL_FUNCTION_CODE (fndecl)){
    CASE_FLT_FN (BUILT_IN_ILOGB):
      errno_set = true; builtin_optab = ilogb_optab; break;
    CASE_FLT_FN (BUILT_IN_ISINF):
      builtin_optab = isinf_optab; break;
    case BUILT_IN_ISNORMAL:
    case BUILT_IN_ISFINITE:
    CASE_FLT_FN (BUILT_IN_FINITE):
    case BUILT_IN_FINITED32:
    case BUILT_IN_FINITED64:
    case BUILT_IN_FINITED128:
    case BUILT_IN_ISINFD32:
    case BUILT_IN_ISINFD64:
    case BUILT_IN_ISINFD128:
      /* These builtins have no optabs (yet).  */
      break;
    default:
      gcc_unreachable ();
  }
  /* There's no easy way to detect the case we need to set EDOM.  */
  if (mtcsOptionsItem->x_flag_errno_math && errno_set)
     return CODE_FOR_nothing;
  /* Optab mode depends on the mode of the input argument.  */
  mode = TYPE_MODE (TREE_TYPE (arg));
  mode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (arg),mode);

  if (builtin_optab)
    return mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,builtin_optab, mode);
  return CODE_FOR_nothing;
}


/* Expand a call to one of the builtin math functions that operate on
   floating point argument and output an integer result (ilogb, isinf,
   isnan, etc).
   Return 0 if a normal call should be emitted rather than expanding the
   function in-line.  EXP is the expression that is a call to the builtin
   function; if convenient, the result should be placed in TARGET.  */
//原型 expand_builtin_interclass_mathfn builtins.cc
static rtx expand_builtin_interclass_mathfn (MtcsBuiltins *self,tree exp, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

  enum insn_code icode = CODE_FOR_nothing;
  rtx op0;
  tree fndecl = get_callee_fndecl (exp);
  machine_mode mode;
  tree arg;

  if (!validate_arglist (exp, REAL_TYPE, VOID_TYPE))
    return NULL_RTX;

  arg = CALL_EXPR_ARG (exp, 0);
  icode = interclass_mathfn_icode(self,arg, fndecl);
  mode = TYPE_MODE (TREE_TYPE (arg));
  mode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (arg),mode);

  if (icode != CODE_FOR_nothing){
      class expand_operand ops[1];
      rtx_insn *last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
      tree orig_arg = arg;
      /* Wrap the computation of the argument in a SAVE_EXPR, as we may
    need to expand the argument again.  This way, we will not perform
    side-effects more the once.  */
      CALL_EXPR_ARG (exp, 0) = arg = builtin_save_expr (arg);
      op0 = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,arg, NULL_RTX, VOIDmode, EXPAND_NORMAL);
      if (mode != GET_MODE (op0))
         op0 = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,mode, op0, 0);
      machine_mode exprMode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (exp),TYPE_MODE (TREE_TYPE (exp)));

      create_output_operand (&ops[0], target, exprMode/*!TYPE_MODE (TREE_TYPE (exp))*/);
      if (mtcs_optabs_maybe_legitimize_operands/*!maybe_legitimize_operands*/(mtcsOptabs,icode, 0, 1, ops)
         && mtcs_optabs_maybe_emit_unop_insn/*!maybe_emit_unop_insn*/(mtcsOptabs,icode, ops[0].value, op0, UNKNOWN))
         return ops[0].value;

      mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
      CALL_EXPR_ARG (exp, 0) = orig_arg;
  }
  return NULL_RTX;
}


/* Expand the __builtin_issignaling builtin.  This needs to handle
   all floating point formats that do support NaNs (for those that
   don't it just sets target to 0).  */
//原型 expand_builtin_issignaling builtins.cc
static rtx expand_builtin_issignaling (MtcsBuiltins *self,tree exp, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);

  if (!validate_arglist (exp, REAL_TYPE, VOID_TYPE))
    return NULL_RTX;

  tree arg = CALL_EXPR_ARG (exp, 0);
  scalar_float_mode fmode = mtcs_mode_host2device_scalar_float/*!SCALAR_FLOAT_TYPE_MODE (TREE_TYPE (arg))*/(mtcsMode,TREE_TYPE (arg));
  const struct real_format *fmt = mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,fmode);
  /* Expand the argument yielding a RTX expression. */
  rtx temp = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg);
  /* If mode doesn't support NaN, always return 0.
     Don't use !HONOR_SNANS (fmode) here, so there is some possibility of
     __builtin_issignaling working without -fsignaling-nans.  Especially
     when -fno-signaling-nans is the default.
     On the other side, MODE_HAS_NANS (fmode) is unnecessary, with
     -ffinite-math-only even __builtin_isnan or __builtin_fpclassify
     fold to 0 or non-NaN/Inf classification.  */
  if (!mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,fmode)){
     mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, const0_rtx);
      return target;
  }
  /* Check if the back end provides an insn that handles issignaling for the
     argument's mode. */
  enum insn_code icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,issignaling_optab, fmode);
  if (icode != CODE_FOR_nothing){
      rtx_insn *last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
      machine_mode tmode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (exp),TYPE_MODE (TREE_TYPE (exp)));
      rtx this_target =  mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,tmode/*!TYPE_MODE (TREE_TYPE (exp))*/);
      if (mtcs_optabs_maybe_emit_unop_insn/*!maybe_emit_unop_insn*/(mtcsOptabs,icode, this_target, temp, UNKNOWN))
         return this_target;
      mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
  }
  if (mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,fmode)){
      scalar_int_mode imode;
      rtx hi;
      switch (fmt->ieee_bits){
         case 32:
         case 64:
           imode = mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,fmode).require ();
           temp = gen_lowpart (imode, temp);
           break;
         case 128:
           imode = mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,64, 1).require ();
           hi = NULL_RTX;
           /* For decimal128, TImode support isn't always there and even when
              it is, working on the DImode high part is usually better.  */
           if (!MEM_P (temp)){
               if (rtx t = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,imode, temp, fmode,
                     mtcs_mode_subreg_highpart_offset/*!subreg_highpart_offset*/(mtcsMode,imode,fmode)))
                  hi = t;
               else{
                 scalar_int_mode imode2;
                 if ( mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,fmode).exists (&imode2)){
                     rtx temp2 = gen_lowpart (imode2, temp);
                     poly_uint64 off = mtcs_mode_subreg_highpart_offset/*!subreg_highpart_offset*/(mtcsMode,imode, imode2);
                     if (rtx t =  mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,
                           imode, temp2,imode2, off))
                      hi = t;
                 }
               }
               if (!hi){
                 rtx mem = mtcs_func_assign_stack_temp/*!assign_stack_temp*/(mtcsFunc,fmode,
                       mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,fmode));
                 mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mem, temp);
                 temp = mem;
              }
           }
           if (!hi) {
               poly_int64 offset  = mtcs_mode_subreg_highpart_offset/*!subreg_highpart_offset*/(mtcsMode,imode, GET_MODE (temp));
               hi = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,temp, imode, offset);
           }
           temp = hi;
           break;
         default:
           gcc_unreachable ();
      }
      /* In all of decimal{32,64,128}, there is MSB sign bit and sNaN
      have 6 bits below it all set.  */
      rtx val=mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,
            HOST_WIDE_INT_C (0x3f) << (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,imode) - 7));
      temp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,imode, and_optab, temp, val, NULL_RTX, 1, OPTAB_LIB_WIDEN);
      temp = mtcs_expmed_emit_store_flag_force/*!emit_store_flag_force*/(mtcsExpmed,target, EQ, temp, val, imode, 1, 1);
      return temp;
  }
  /* Only PDP11 has these defined differently but doesn't support NaNs.  */
  gcc_assert (FLOAT_WORDS_BIG_ENDIAN == WORDS_BIG_ENDIAN);
  gcc_assert (fmt->signbit_ro > 0 && fmt->b == 2);
  gcc_assert (mtcs_mode_is_composite_p/*!MODE_COMPOSITE_P*/(mtcsMode,fmode)
         || (fmt->pnan == fmt->p  && fmt->signbit_ro == fmt->signbit_rw));
  switch (fmt->p){
    case 106: /* IBM double double  */
      /* For IBM double double, recurse on the most significant double.  */
      gcc_assert (mtcs_mode_is_composite_p/*!MODE_COMPOSITE_P*/(mtcsMode,fmode));
      temp =mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,mtcsMode->modes.M_DFmode, fmode, temp, 0);
      fmode =mtcs_mode_as_a<scalar_float_mode>(mtcsMode, mtcsMode->modes.M_DFmode);
      fmt = mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,mtcsMode->modes.M_DFmode);
      /* FALLTHRU */
    case 8: /* bfloat */
    case 11: /* IEEE half */
    case 24: /* IEEE single */
    case 53: /* IEEE double or Intel extended with rounding to double */
      if (fmt->p == 53 && fmt->signbit_ro == 79)
         goto extended;
      {
         scalar_int_mode imode = mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,fmode).require ();
         temp = gen_lowpart (imode, temp);
         rtx val = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,(HOST_WIDE_INT_M1U << (fmt->p - 2))
                  & ~(HOST_WIDE_INT_M1U << fmt->signbit_ro));
         if (fmt->qnan_msb_set){
             rtx mask = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,~(HOST_WIDE_INT_M1U << fmt->signbit_ro));
             rtx bit = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,HOST_WIDE_INT_1U << (fmt->p - 2));
             /* For non-MIPS/PA IEEE single/double/half or bfloat, expand to:
                ((temp ^ bit) & mask) > val.  */
             temp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,imode, xor_optab, temp, bit,
                   NULL_RTX, 1, OPTAB_LIB_WIDEN);
             temp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,imode, and_optab, temp, mask,
                   NULL_RTX, 1, OPTAB_LIB_WIDEN);
             temp = mtcs_expmed_emit_store_flag_force/*!emit_store_flag_force*/(mtcsExpmed,target,
                   GTU, temp, val, imode, 1, 1);
         }else{
             /* For MIPS/PA IEEE single/double, expand to:
                (temp & val) == val.  */
             temp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,imode, and_optab, temp, val,
                   NULL_RTX, 1, OPTAB_LIB_WIDEN);
             temp = mtcs_expmed_emit_store_flag_force/*!emit_store_flag_force*/(mtcsExpmed,target, EQ, temp, val, imode,
                       1, 1);
         }
      }
      break;
    case 113: /* IEEE quad */
      {
         rtx hi = NULL_RTX, lo = NULL_RTX;
         scalar_int_mode imode = mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,64, 1).require ();
         /* For IEEE quad, TImode support isn't always there and even when
            it is, working on DImode parts is usually better.  */
         if (!MEM_P (temp)){
             hi = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,imode, temp, fmode,
                        mtcs_mode_subreg_highpart_offset/*!subreg_highpart_offset*/(mtcsMode,imode, fmode));
             lo = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,imode, temp, fmode,
                   mtcs_mode_subreg_lowpart_offset/*!subreg_lowpart_offset*/(mtcsMode,imode, fmode));
             if (!hi || !lo){
               scalar_int_mode imode2;
               if (mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,fmode).exists (&imode2)){
                   rtx temp2 = gen_lowpart (imode2, temp);
                   hi = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,imode, temp2, imode2,
                              mtcs_mode_subreg_highpart_offset/*!subreg_highpart_offset*/(mtcsMode,imode,imode2));
                   lo = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,imode, temp2, imode2,
                         mtcs_mode_subreg_lowpart_offset/*!subreg_lowpart_offset*/(mtcsMode,imode,imode2));
               }
             }
             if (!hi || !lo){
               rtx mem = mtcs_func_assign_stack_temp/*!assign_stack_temp*/(mtcsFunc,fmode,
                     mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,fmode));
               mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mem, temp);
               temp = mem;
             }
         }
         if (!hi || !lo){
             poly_int64 offset= mtcs_mode_subreg_highpart_offset/*!subreg_highpart_offset*/(mtcsMode,imode, GET_MODE (temp));
             hi = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,temp, imode, offset);
             offset = mtcs_mode_subreg_lowpart_offset/*!subreg_lowpart_offset*/(mtcsMode,imode, GET_MODE (temp));
             lo = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,temp, imode, offset);
         }
         rtx val =mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,(HOST_WIDE_INT_M1U << (fmt->p - 2 - 64))
                  & ~(HOST_WIDE_INT_M1U << (fmt->signbit_ro - 64)));
         if (fmt->qnan_msb_set){
             rtx mask = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,~(HOST_WIDE_INT_M1U << (fmt->signbit_ro- 64)));
             rtx bit = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,HOST_WIDE_INT_1U << (fmt->p - 2 - 64));
             /* For non-MIPS/PA IEEE quad, expand to:
                (((hi ^ bit) | ((lo | -lo) >> 63)) & mask) > val.  */
             rtx nlo = mtcs_optabs_expand_unop/*!expand_unop*/(mtcsOptabs,imode, neg_optab, lo, NULL_RTX, 0);
             lo = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,imode, ior_optab, lo, nlo,
                      NULL_RTX, 1, OPTAB_LIB_WIDEN);
             lo = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, imode, lo, 63, NULL_RTX, 1);
             temp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,imode, xor_optab, hi, bit,
                   NULL_RTX, 1, OPTAB_LIB_WIDEN);
             temp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,imode, ior_optab, temp, lo,
                   NULL_RTX, 1, OPTAB_LIB_WIDEN);
             temp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,imode, and_optab, temp, mask,
                   NULL_RTX, 1, OPTAB_LIB_WIDEN);
             temp = mtcs_expmed_emit_store_flag_force/*!emit_store_flag_force*/(mtcsExpmed,target, GTU, temp, val, imode,
                       1, 1);
         }else{
             /* For MIPS/PA IEEE quad, expand to:
                (hi & val) == val.  */
             temp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,imode, and_optab, hi, val,
                   NULL_RTX, 1, OPTAB_LIB_WIDEN);
             temp = mtcs_expmed_emit_store_flag_force/*!emit_store_flag_force*/(mtcsExpmed,target, EQ, temp, val, imode,
                       1, 1);
         }
      }
      break;
    case 64: /* Intel or Motorola extended */
    extended:
      {
         rtx ex, hi, lo;
         scalar_int_mode imode = mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,32, 1).require ();
         scalar_int_mode iemode = mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,16, 1).require ();
         if (!MEM_P (temp)){
             rtx mem = mtcs_func_assign_stack_temp/*!assign_stack_temp*/(mtcsFunc,fmode, mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,fmode));
             mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mem, temp);
             temp = mem;
         }
         if (fmt->signbit_ro == 95){
             /* Motorola, always big endian, with 16-bit gap in between
                16-bit sign+exponent and 64-bit mantissa.  */
             ex = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,temp, iemode, 0);
             hi = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,temp, imode, 4);
             lo = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,temp, imode, 8);
         }else if (!WORDS_BIG_ENDIAN){
             /* Intel little endian, 64-bit mantissa followed by 16-bit
                sign+exponent and then either 16 or 48 bits of gap.  */
             ex = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,temp, iemode, 8);
             hi = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,temp, imode, 4);
             lo = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,temp, imode, 0);
         }else{
             /* Big endian Itanium.  */
             ex = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,temp, iemode, 0);
             hi = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,temp, imode, 2);
             lo = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,temp, imode, 6);
         }
         rtx val =mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,HOST_WIDE_INT_M1U << 30);
         gcc_assert (fmt->qnan_msb_set);
         rtx mask = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,0x7fff);
         rtx bit = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,HOST_WIDE_INT_1U << 30);
         /* For Intel/Motorola extended format, expand to:
            (ex & mask) == mask && ((hi ^ bit) | ((lo | -lo) >> 31)) > val.  */
         rtx nlo = mtcs_optabs_expand_unop/*!expand_unop*/(mtcsOptabs,imode, neg_optab, lo, NULL_RTX, 0);
         lo = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,imode, ior_optab, lo, nlo,
                  NULL_RTX, 1, OPTAB_LIB_WIDEN);
         lo = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, imode, lo, 31, NULL_RTX, 1);
         temp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,imode, xor_optab, hi, bit,
                    NULL_RTX, 1, OPTAB_LIB_WIDEN);
         temp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,imode, ior_optab, temp, lo,
                    NULL_RTX, 1, OPTAB_LIB_WIDEN);
         temp = mtcs_expmed_emit_store_flag_force/*!emit_store_flag_force*/(mtcsExpmed,target, GTU, temp, val, imode, 1, 1);
         ex = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,iemode, and_optab, ex, mask,
                  NULL_RTX, 1, OPTAB_LIB_WIDEN);
         ex = mtcs_expmed_emit_store_flag_force/*!emit_store_flag_force*/(mtcsExpmed,
               mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (temp)), EQ,ex, mask, iemode, 1, 1);
         temp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,GET_MODE (temp), and_optab, temp, ex,
                    NULL_RTX, 1, OPTAB_LIB_WIDEN);
      }
      break;
    default:
      gcc_unreachable ();
  }
  return temp;
}

/* Expand a call to one of the builtin rounding functions gcc defines
   as an extension (lfloor and lceil).  As these are gcc extensions we
   do not need to worry about setting errno to EDOM.
   If expanding via optab fails, lower expression to (int)(floor(x)).
   EXP is the expression that is a call to the builtin function;
   if convenient, the result should be placed in TARGET.  */
//原型 expand_builtin_int_roundingfn builtins.cc
static rtx expand_builtin_int_roundingfn (MtcsBuiltins *self,tree exp, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);

   convert_optab builtin_optab;
   rtx op0, tmp;
   rtx_insn *insns;
   tree fndecl = get_callee_fndecl (exp);
   enum built_in_function fallback_fn;
   tree fallback_fndecl;
   machine_mode mode;
   tree arg;

   if (!validate_arglist (exp, REAL_TYPE, VOID_TYPE))
      return NULL_RTX;

   arg = CALL_EXPR_ARG (exp, 0);
   switch (DECL_FUNCTION_CODE (fndecl)){
      CASE_FLT_FN (BUILT_IN_ICEIL):
      CASE_FLT_FN (BUILT_IN_LCEIL):
      CASE_FLT_FN (BUILT_IN_LLCEIL):
         builtin_optab = lceil_optab;
         fallback_fn = BUILT_IN_CEIL;
         break;

      CASE_FLT_FN (BUILT_IN_IFLOOR):
      CASE_FLT_FN (BUILT_IN_LFLOOR):
      CASE_FLT_FN (BUILT_IN_LLFLOOR):
         builtin_optab = lfloor_optab;
         fallback_fn = BUILT_IN_FLOOR;
         break;
      default:
         gcc_unreachable ();
   }

   /* Make a suitable register to place result in.  */
   mode = TYPE_MODE (TREE_TYPE (exp));
   mode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (exp),mode);
   target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
   /* Wrap the computation of the argument in a SAVE_EXPR, as we may
   need to expand the argument again.  This way, we will not perform
   side-effects more the once.  */
   CALL_EXPR_ARG (exp, 0) = arg = builtin_save_expr (arg);
   op0 = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,arg, NULL, VOIDmode, EXPAND_NORMAL);

   mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
   /* Compute into TARGET.  */
   if (mtcs_optabs_expand_sfix_optab/*!expand_sfix_optab*/(mtcsOptabs,target, op0, builtin_optab)){
      /* Output the entire sequence.  */
      insns =  mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,insns);
      return target;
   }
   /* If we were unable to expand via the builtin, stop the sequence
   (without outputting the insns).  */
   mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   /* Fall back to floating point rounding optab.  */
   fallback_fndecl = mathfn_built_in (TREE_TYPE (arg), fallback_fn);
   /* For non-C99 targets we may end up without a fallback fndecl here
   if the user called __builtin_lfloor directly.  In this case emit
   a call to the floor/ceil variants nevertheless.  This should result
   in the best user experience for not full C99 targets.  */
   if (fallback_fndecl == NULL_TREE){
      tree fntype;
      const char *name = NULL;
      switch (DECL_FUNCTION_CODE (fndecl)){
         case BUILT_IN_ICEIL:
         case BUILT_IN_LCEIL:
         case BUILT_IN_LLCEIL:
            name = "ceil";
            break;
         case BUILT_IN_ICEILF:
         case BUILT_IN_LCEILF:
         case BUILT_IN_LLCEILF:
            name = "ceilf";
            break;
         case BUILT_IN_ICEILL:
         case BUILT_IN_LCEILL:
         case BUILT_IN_LLCEILL:
            name = "ceill";
            break;
         case BUILT_IN_IFLOOR:
         case BUILT_IN_LFLOOR:
         case BUILT_IN_LLFLOOR:
            name = "floor";
            break;
         case BUILT_IN_IFLOORF:
         case BUILT_IN_LFLOORF:
         case BUILT_IN_LLFLOORF:
            name = "floorf";
            break;
         case BUILT_IN_IFLOORL:
         case BUILT_IN_LFLOORL:
         case BUILT_IN_LLFLOORL:
            name = "floorl";
            break;
         default:
            gcc_unreachable ();
      }

      fntype = build_function_type_list (TREE_TYPE (arg),TREE_TYPE (arg), NULL_TREE);
      fallback_fndecl = build_fn_decl (name, fntype);
   }

   exp = build_call_nofold_loc(self,EXPR_LOCATION (exp), fallback_fndecl, 1, arg);
   tmp = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,exp);
   tmp = mtcs_expr_maybe_emit_group_store/*!maybe_emit_group_store*/(mtcsExpr,tmp, TREE_TYPE (exp));
   /* Truncate the result of floating point optab to integer
   via expand_fix ().  */
   target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
   mtcs_optabs_expand_fix/*!expand_fix*/(mtcsOptabs,target, tmp, 0);

   return target;
}


/* This helper macro, meant to be used in mathfn_built_in below, determines
   which among a set of builtin math functions is appropriate for a given type
   mode.  The `F' (float) and `L' (long double) are automatically generated
   from the 'double' case.  If a function supports the _Float<N> and _Float<N>X
   types, there are additional types that are considered with 'F32', 'F64',
   'F128', etc. suffixes.  */
#define CASE_MATHFN(MATHFN) \
  CASE_CFN_##MATHFN: \
  fcode = BUILT_IN_##MATHFN; fcodef = BUILT_IN_##MATHFN##F ; \
  fcodel = BUILT_IN_##MATHFN##L ; break;
/* Similar to the above, but also add support for the _Float<N> and _Float<N>X
   types.  */
#define CASE_MATHFN_FLOATN(MATHFN) \
  CASE_CFN_##MATHFN: \
  fcode = BUILT_IN_##MATHFN; fcodef = BUILT_IN_##MATHFN##F ; \
  fcodel = BUILT_IN_##MATHFN##L ; fcodef16 = BUILT_IN_##MATHFN##F16 ; \
  fcodef32 = BUILT_IN_##MATHFN##F32; fcodef64 = BUILT_IN_##MATHFN##F64 ; \
  fcodef128 = BUILT_IN_##MATHFN##F128 ; fcodef32x = BUILT_IN_##MATHFN##F32X ; \
  fcodef64x = BUILT_IN_##MATHFN##F64X ; fcodef128x = BUILT_IN_##MATHFN##F128X ;\
  break;
/* Similar to above, but appends _R after any F/L suffix.  */
#define CASE_MATHFN_REENT(MATHFN) \
  case CFN_BUILT_IN_##MATHFN##_R: \
  case CFN_BUILT_IN_##MATHFN##F_R: \
  case CFN_BUILT_IN_##MATHFN##L_R: \
  fcode = BUILT_IN_##MATHFN##_R; fcodef = BUILT_IN_##MATHFN##F_R ; \
  fcodel = BUILT_IN_##MATHFN##L_R ; break;

/* Return a function equivalent to FN but operating on floating-point
   values of type TYPE, or END_BUILTINS if no such function exists.
   This is purely an operation on function codes; it does not guarantee
   that the target actually has an implementation of the function.  */
//原型 mathfn_built_in_2 builtins.cc
static built_in_function mathfn_built_in_2 (tree type, combined_fn fn)
{
  tree mtype;
  built_in_function fcode, fcodef, fcodel;
  built_in_function fcodef16 = END_BUILTINS;
  built_in_function fcodef32 = END_BUILTINS;
  built_in_function fcodef64 = END_BUILTINS;
  built_in_function fcodef128 = END_BUILTINS;
  built_in_function fcodef32x = END_BUILTINS;
  built_in_function fcodef64x = END_BUILTINS;
  built_in_function fcodef128x = END_BUILTINS;

  /* If <math.h> has been included somehow, HUGE_VAL and NAN definitions
     break the uses below.  */
#undef HUGE_VAL
#undef NAN

  switch (fn)
    {
#define SEQ_OF_CASE_MATHFN       \
    CASE_MATHFN_FLOATN (ACOS)       \
    CASE_MATHFN_FLOATN (ACOSH)         \
    CASE_MATHFN_FLOATN (ASIN)       \
    CASE_MATHFN_FLOATN (ASINH)         \
    CASE_MATHFN_FLOATN (ATAN)       \
    CASE_MATHFN_FLOATN (ATAN2)         \
    CASE_MATHFN_FLOATN (ATANH)         \
    CASE_MATHFN_FLOATN (CBRT)       \
    CASE_MATHFN_FLOATN (CEIL)       \
    CASE_MATHFN (CEXPI)          \
    CASE_MATHFN_FLOATN (COPYSIGN)      \
    CASE_MATHFN_FLOATN (COS)        \
    CASE_MATHFN_FLOATN (COSH)       \
    CASE_MATHFN (DREM)           \
    CASE_MATHFN_FLOATN (ERF)        \
    CASE_MATHFN_FLOATN (ERFC)       \
    CASE_MATHFN_FLOATN (EXP)        \
    CASE_MATHFN (EXP10)          \
    CASE_MATHFN_FLOATN (EXP2)       \
    CASE_MATHFN_FLOATN (EXPM1)         \
    CASE_MATHFN_FLOATN (FABS)       \
    CASE_MATHFN_FLOATN (FDIM)       \
    CASE_MATHFN_FLOATN (FLOOR)         \
    CASE_MATHFN_FLOATN (FMA)        \
    CASE_MATHFN_FLOATN (FMAX)       \
    CASE_MATHFN_FLOATN (FMIN)       \
    CASE_MATHFN_FLOATN (FMOD)       \
    CASE_MATHFN_FLOATN (FREXP)         \
    CASE_MATHFN (GAMMA)          \
    CASE_MATHFN_REENT (GAMMA) /* GAMMA_R */  \
    CASE_MATHFN_FLOATN (HUGE_VAL)      \
    CASE_MATHFN_FLOATN (HYPOT)         \
    CASE_MATHFN_FLOATN (ILOGB)         \
    CASE_MATHFN (ICEIL)          \
    CASE_MATHFN (IFLOOR)         \
    CASE_MATHFN_FLOATN (INF)        \
    CASE_MATHFN (IRINT)          \
    CASE_MATHFN (IROUND)         \
    CASE_MATHFN (ISINF)          \
    CASE_MATHFN (J0)          \
    CASE_MATHFN (J1)          \
    CASE_MATHFN (JN)          \
    CASE_MATHFN (LCEIL)          \
    CASE_MATHFN_FLOATN (LDEXP)         \
    CASE_MATHFN (LFLOOR)         \
    CASE_MATHFN_FLOATN (LGAMMA)        \
    CASE_MATHFN_REENT (LGAMMA) /* LGAMMA_R */   \
    CASE_MATHFN (LLCEIL)         \
    CASE_MATHFN (LLFLOOR)        \
    CASE_MATHFN_FLOATN (LLRINT)        \
    CASE_MATHFN_FLOATN (LLROUND)    \
    CASE_MATHFN_FLOATN (LOG)        \
    CASE_MATHFN_FLOATN (LOG10)         \
    CASE_MATHFN_FLOATN (LOG1P)         \
    CASE_MATHFN_FLOATN (LOG2)       \
    CASE_MATHFN_FLOATN (LOGB)       \
    CASE_MATHFN_FLOATN (LRINT)         \
    CASE_MATHFN_FLOATN (LROUND)        \
    CASE_MATHFN_FLOATN (MODF)       \
    CASE_MATHFN_FLOATN (NAN)        \
    CASE_MATHFN_FLOATN (NANS)       \
    CASE_MATHFN_FLOATN (NEARBYINT)     \
    CASE_MATHFN_FLOATN (NEXTAFTER)     \
    CASE_MATHFN (NEXTTOWARD)        \
    CASE_MATHFN_FLOATN (POW)        \
    CASE_MATHFN (POWI)           \
    CASE_MATHFN (POW10)          \
    CASE_MATHFN_FLOATN (REMAINDER)     \
    CASE_MATHFN_FLOATN (REMQUO)        \
    CASE_MATHFN_FLOATN (RINT)       \
    CASE_MATHFN_FLOATN (ROUND)         \
    CASE_MATHFN_FLOATN (ROUNDEVEN)     \
    CASE_MATHFN (SCALB)          \
    CASE_MATHFN_FLOATN (SCALBLN)    \
    CASE_MATHFN_FLOATN (SCALBN)        \
    CASE_MATHFN (SIGNBIT)        \
    CASE_MATHFN (SIGNIFICAND)       \
    CASE_MATHFN_FLOATN (SIN)        \
    CASE_MATHFN (SINCOS)         \
    CASE_MATHFN_FLOATN (SINH)       \
    CASE_MATHFN_FLOATN (SQRT)       \
    CASE_MATHFN_FLOATN (TAN)        \
    CASE_MATHFN_FLOATN (TANH)       \
    CASE_MATHFN_FLOATN (TGAMMA)        \
    CASE_MATHFN_FLOATN (TRUNC)         \
    CASE_MATHFN (Y0)          \
    CASE_MATHFN (Y1)          \
    CASE_MATHFN (YN)

    SEQ_OF_CASE_MATHFN

    default:
      return END_BUILTINS;
    }

  mtype = TYPE_MAIN_VARIANT (type);
  if (mtype == double_type_node)
    return fcode;
  else if (mtype == float_type_node)
    return fcodef;
  else if (mtype == long_double_type_node)
    return fcodel;
  else if (mtype == float16_type_node)
    return fcodef16;
  else if (mtype == float32_type_node)
    return fcodef32;
  else if (mtype == float64_type_node)
    return fcodef64;
  else if (mtype == float128_type_node)
    return fcodef128;
  else if (mtype == float32x_type_node)
    return fcodef32x;
  else if (mtype == float64x_type_node)
    return fcodef64x;
  else if (mtype == float128x_type_node)
    return fcodef128x;
  else
    return END_BUILTINS;
}

#undef CASE_MATHFN
#undef CASE_MATHFN_FLOATN
#undef CASE_MATHFN_REENT

/* Return mathematic function equivalent to FN but operating directly on TYPE,
   if available.  If IMPLICIT_P is true use the implicit builtin declaration,
   otherwise use the explicit declaration.  If we can't do the conversion,
   return null.  */
//原型 mathfn_built_in_1 builtins.cc
static tree mathfn_built_in_1 (MtcsBuiltins *self,tree type, combined_fn fn, bool implicit_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   built_in_function fcode2 = mathfn_built_in_2 (type, fn);
   if (fcode2 == END_BUILTINS)
      return NULL_TREE;

   if (implicit_p && !builtin_decl_implicit_p (fcode2))
      return NULL_TREE;

   return mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,fcode2);
}


/* Expand a call to one of the builtin math functions doing integer
   conversion (lrint).
   Return 0 if a normal call should be emitted rather than expanding the
   function in-line.  EXP is the expression that is a call to the builtin
   function; if convenient, the result should be placed in TARGET.  */
//原型 expand_builtin_int_roundingfn_2 builtins.cc
static rtx expand_builtin_int_roundingfn_2 (MtcsBuiltins *self,tree exp, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);

   convert_optab builtin_optab;
   rtx op0;
   rtx_insn *insns;
   tree fndecl = get_callee_fndecl (exp);
   tree arg;
   machine_mode mode;
   enum built_in_function fallback_fn = BUILT_IN_NONE;
   if (!validate_arglist (exp, REAL_TYPE, VOID_TYPE))
      return NULL_RTX;
   arg = CALL_EXPR_ARG (exp, 0);
   switch (DECL_FUNCTION_CODE (fndecl)){
      CASE_FLT_FN (BUILT_IN_IRINT):
         fallback_fn = BUILT_IN_LRINT;
         gcc_fallthrough ();
      CASE_FLT_FN (BUILT_IN_LRINT):
      CASE_FLT_FN (BUILT_IN_LLRINT):
         builtin_optab = lrint_optab;
         break;

      CASE_FLT_FN (BUILT_IN_IROUND):
         fallback_fn = BUILT_IN_LROUND;
         gcc_fallthrough ();
      CASE_FLT_FN (BUILT_IN_LROUND):
      CASE_FLT_FN (BUILT_IN_LLROUND):
         builtin_optab = lround_optab;
         break;
      default:
         gcc_unreachable ();
   }
   /* There's no easy way to detect the case we need to set EDOM.  */
   if (mtcsOptionsItem->x_flag_errno_math && fallback_fn == BUILT_IN_NONE)
      return NULL_RTX;
   /* Make a suitable register to place result in.  */
   mode = TYPE_MODE (TREE_TYPE (exp));
   mode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (exp),mode);
   /* There's no easy way to detect the case we need to set EDOM.  */
   if (!mtcsOptionsItem->x_flag_errno_math){
      rtx result = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
      /* Wrap the computation of the argument in a SAVE_EXPR, as we may
      need to expand the argument again.  This way, we will not perform
      side-effects more the once.  */
      CALL_EXPR_ARG (exp, 0) = arg = builtin_save_expr (arg);
      op0 = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,arg, NULL, VOIDmode, EXPAND_NORMAL);
      mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);

      if (mtcs_optabs_expand_sfix_optab/*!expand_sfix_optab*/(mtcsOptabs,result, op0, builtin_optab)){
         /* Output the entire sequence.  */
         insns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
         mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,insns);
         return result;
      }
      /* If we were unable to expand via the builtin, stop the sequence
      (without outputting the insns) and call to the library function
      with the stabilized argument list.  */
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   }

   if (fallback_fn != BUILT_IN_NONE){
      /* Fall back to rounding to long int.  Use implicit_p 0 - for non-C99
      targets, (int) round (x) should never be transformed into
      BUILT_IN_IROUND and if __builtin_iround is called directly, emit
      a call to lround in the hope that the target provides at least some
      C99 functions.  This should result in the best user experience for
      not full C99 targets.
      As scalar float conversions with same mode are useless in GIMPLE,
      we can end up e.g. with _Float32 argument passed to float builtin,
      try to get the type from the builtin prototype first.  */
      tree fallback_fndecl = NULL_TREE;
      if (tree argtypes = TYPE_ARG_TYPES (TREE_TYPE (fndecl)))
         fallback_fndecl= mathfn_built_in_1(self,TREE_VALUE (argtypes),
      as_combined_fn (fallback_fn), 0);
      if (fallback_fndecl == NULL_TREE)
         fallback_fndecl = mathfn_built_in_1(self,TREE_TYPE (arg),as_combined_fn (fallback_fn), 0);
      if (fallback_fndecl){
         exp = build_call_nofold_loc(self,EXPR_LOCATION (exp),fallback_fndecl, 1, arg);
         target =  mtcs_calls_expand_call/*!expand_call*/(mtcsCalls,exp, NULL_RTX, target == const0_rtx);
         target = mtcs_expr_maybe_emit_group_store/*!maybe_emit_group_store*/(mtcsExpr,target, TREE_TYPE (exp));
         return mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,mode, target, 0);
      }
   }
   return mtcs_calls_expand_call/*!expand_call*/(mtcsCalls,exp, target, target == const0_rtx);
}



/* Expand a call to the powi built-in mathematical function.  Return NULL_RTX if
   a normal call should be emitted rather than expanding the function
   in-line.  EXP is the expression that is a call to the builtin
   function; if convenient, the result should be placed in TARGET.  */
//原型 expand_builtin_powi builtins.cc
static rtx expand_builtin_powi (MtcsBuiltins *self,tree exp, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);

   tree arg0, arg1;
   rtx op0, op1;
   machine_mode mode;
   machine_mode mode2;
   if (! validate_arglist (exp, REAL_TYPE, INTEGER_TYPE, VOID_TYPE))
      return NULL_RTX;

   arg0 = CALL_EXPR_ARG (exp, 0);
   arg1 = CALL_EXPR_ARG (exp, 1);
   mode = TYPE_MODE (TREE_TYPE (exp));
   mode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (exp),mode);
   /* Emit a libcall to libgcc.  */
   /* Mode of the 2nd argument must match that of an int.  */
   mode2 = mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,INT_TYPE_SIZE, 0).require ();
   if (target == NULL_RTX)
      target =  mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
   op0 =  mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,arg0, NULL_RTX, mode, EXPAND_NORMAL);
   if (GET_MODE (op0) != mode)
      op0 = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,mode, op0, 0);
   op1 =  mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,arg1, NULL_RTX, mode2, EXPAND_NORMAL);
   if (GET_MODE (op1) != mode2)
      op1 = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,mode2, op1, 0);
   target = mtcs_calls_emit_library_call_value/*!emit_library_call_value*/(mtcsCalls,
         mtcs_libfuncs_optab_libfunc/*!optab_libfunc*/(mtcsLibfuncs,powi_optab, mode),
   target, LCT_CONST, mode, op0, mode, op1, mode2);
   return target;
}


/* Expand a call to the internal cexpi builtin to the sincos math function.
   EXP is the expression that is a call to the builtin function; if convenient,
   the result should be placed in TARGET.  */
//原型 expand_builtin_cexpi builtins.cc
static rtx expand_builtin_cexpi (MtcsBuiltins *self,tree exp, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree fndecl = get_callee_fndecl (exp);
   tree arg, type;
   machine_mode mode;
   rtx op0, op1, op2;
   location_t loc = EXPR_LOCATION (exp);

   if (!validate_arglist (exp, REAL_TYPE, VOID_TYPE))
      return NULL_RTX;

   arg = CALL_EXPR_ARG (exp, 0);
   type = TREE_TYPE (arg);
   mode = TYPE_MODE (TREE_TYPE (arg));
   mode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (arg),mode);

   /* Try expanding via a sincos optab, fall back to emitting a libcall
   to sincos or cexp.  We are sure we have sincos or cexp because cexpi
   is only generated from sincos, cexp or if we have either of them.  */
   if ( mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,sincos_optab, mode) != CODE_FOR_nothing){
      op1 = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
      op2 = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
      op0 = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,arg, NULL_RTX, VOIDmode, EXPAND_NORMAL);
      /* Compute into op1 and op2.  */
      mtcs_optabs_expand_twoval_unop/*!expand_twoval_unop*/(mtcsOptabs,sincos_optab, op0, op2, op1, 0);
   }else if (mtcsTarget/*!targetm.libc_has_function*/->libc_has_function(mtcsTarget,function_sincos, type)){
      tree call, fn = NULL_TREE;
      tree top1, top2;
      rtx op1a, op2a;
      if (DECL_FUNCTION_CODE (fndecl) == BUILT_IN_CEXPIF)
         fn = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_SINCOSF);
      else if (DECL_FUNCTION_CODE (fndecl) == BUILT_IN_CEXPI)
         fn = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_SINCOS);
      else if (DECL_FUNCTION_CODE (fndecl) == BUILT_IN_CEXPIL)
         fn = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_SINCOSL);
      else
         gcc_unreachable ();

      op1 = mtcs_func_assign_temp/*!assign_temp*/(mtcsFunc,TREE_TYPE (arg), 1, 1);
      op2 = mtcs_func_assign_temp/*!assign_temp*/(mtcsFunc,TREE_TYPE (arg), 1, 1);
      op1a = mtcs_explow_copy_addr_to_reg/*!copy_addr_to_reg*/(mtcsExplow,XEXP (op1, 0));
      op2a = mtcs_explow_copy_addr_to_reg/*!copy_addr_to_reg*/(mtcsExplow,XEXP (op2, 0));
      top1 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,
            mtcs_tree_build_pointer_type/*!build_pointer_type*/(mtcsTree,TREE_TYPE (arg)), op1a);
      top2 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,
            mtcs_tree_build_pointer_type/*!build_pointer_type*/(mtcsTree,TREE_TYPE (arg)), op2a);

      /* Make sure not to fold the sincos call again.  */
      call = build1 (ADDR_EXPR, mtcs_tree_build_pointer_type/*!build_pointer_type*/(mtcsTree,TREE_TYPE (fn)), fn);
      mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,build_call_nary (TREE_TYPE (TREE_TYPE (fn)),
      call, 3, arg, top1, top2));
   }else{
      tree call, fn = NULL_TREE, narg;
      tree ctype = build_complex_type (type);
      if (DECL_FUNCTION_CODE (fndecl) == BUILT_IN_CEXPIF)
         fn = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_CEXPF);
      else if (DECL_FUNCTION_CODE (fndecl) == BUILT_IN_CEXPI)
         fn = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_CEXP);
      else if (DECL_FUNCTION_CODE (fndecl) == BUILT_IN_CEXPIL)
         fn = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_CEXPL);
      else
         gcc_unreachable ();
      /* If we don't have a decl for cexp create one.  This is the
      friendliest fallback if the user calls __builtin_cexpi
      without full target C99 function support.  */
      if (fn == NULL_TREE){
         tree fntype;
         const char *name = NULL;
         if (DECL_FUNCTION_CODE (fndecl) == BUILT_IN_CEXPIF)
            name = "cexpf";
         else if (DECL_FUNCTION_CODE (fndecl) == BUILT_IN_CEXPI)
            name = "cexp";
         else if (DECL_FUNCTION_CODE (fndecl) == BUILT_IN_CEXPIL)
            name = "cexpl";

         fntype = build_function_type_list (ctype, ctype, NULL_TREE);
         fn = build_fn_decl (name, fntype);
      }
      narg = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, COMPLEX_EXPR, ctype,build_real (type, dconst0), arg);
      /* Make sure not to fold the cexp call again.  */
      call = build1 (ADDR_EXPR, mtcs_tree_build_pointer_type/*!build_pointer_type*/(mtcsTree,TREE_TYPE (fn)), fn);
      return mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,build_call_nary (ctype, call, 1, narg),
                                                target, VOIDmode, EXPAND_NORMAL);
   }
   /* Now build the proper return type.  */
   return mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,build2 (COMPLEX_EXPR, build_complex_type (type),
         mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,TREE_TYPE (arg), op2),
         mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,TREE_TYPE (arg), op1)),
                                 target, VOIDmode, EXPAND_NORMAL);
}


/* Expand a call to the builtin sin and cos math functions.
   Return NULL_RTX if a normal call should be emitted rather than expanding the
   function in-line.  EXP is the expression that is a call to the builtin
   function; if convenient, the result should be placed in TARGET.
   SUBTARGET may be used as the target for computing one of EXP's
   operands.  */
//原型 expand_builtin_mathfn_3 builtins.cc
static rtx expand_builtin_mathfn_3 (MtcsBuiltins *self,tree exp, rtx target, rtx subtarget)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   optab builtin_optab;
   rtx op0;
   rtx_insn *insns;
   tree fndecl = get_callee_fndecl (exp);
   machine_mode mode;
   tree arg;

   if (!validate_arglist (exp, REAL_TYPE, VOID_TYPE))
      return NULL_RTX;

   arg = CALL_EXPR_ARG (exp, 0);

   switch (DECL_FUNCTION_CODE (fndecl)){
      CASE_FLT_FN (BUILT_IN_SIN):
      CASE_FLT_FN (BUILT_IN_COS):
         builtin_optab = sincos_optab; break;
      default:
         gcc_unreachable ();
   }
   /* Make a suitable register to place result in.  */
   mode = TYPE_MODE (TREE_TYPE (exp));
   mode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (exp),mode);
   /* Check if sincos insn is available, otherwise fallback
   to sin or cos insn.  */
   if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,builtin_optab, mode) == CODE_FOR_nothing)
      switch (DECL_FUNCTION_CODE (fndecl)){
         CASE_FLT_FN (BUILT_IN_SIN):
            builtin_optab = sin_optab; break;
         CASE_FLT_FN (BUILT_IN_COS):
            builtin_optab = cos_optab; break;
         default:
            gcc_unreachable ();
      }

   /* Before working hard, check whether the instruction is available.  */
   if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,builtin_optab, mode) != CODE_FOR_nothing){
      rtx result = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
      /* Wrap the computation of the argument in a SAVE_EXPR, as we may
      need to expand the argument again.  This way, we will not perform
      side-effects more the once.  */
      CALL_EXPR_ARG (exp, 0) = arg = builtin_save_expr (arg);
      op0 = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,arg, subtarget, VOIDmode, EXPAND_NORMAL);
      mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
      /* Compute into RESULT.
      Set RESULT to wherever the result comes back.  */
      if (builtin_optab == sincos_optab){
         int ok;
         switch (DECL_FUNCTION_CODE (fndecl)){
            CASE_FLT_FN (BUILT_IN_SIN):
               ok = mtcs_optabs_expand_twoval_unop/*!expand_twoval_unop*/(mtcsOptabs,builtin_optab, op0, 0, result, 0);
               break;
            CASE_FLT_FN (BUILT_IN_COS):
               ok = mtcs_optabs_expand_twoval_unop/*!expand_twoval_unop*/(mtcsOptabs,builtin_optab, op0, result, 0, 0);
               break;
            default:
               gcc_unreachable ();
         }
         gcc_assert (ok);
      }else
         result =mtcs_optabs_expand_unop/*!expand_unop*/(mtcsOptabs,mode, builtin_optab, op0, result, 0);

      if (result != 0){
         /* Output the entire sequence.  */
         insns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
         mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,insns);
         return result;
      }
      /* If we were unable to expand via the builtin, stop the sequence
      (without outputting the insns) and call to the library function
      with the stabilized argument list.  */
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   }
   return mtcs_calls_expand_call/*!expand_call*/(mtcsCalls,exp, target, target == const0_rtx);
}

/* Expand a call to the builtin sincos math function.
   Return NULL_RTX if a normal call should be emitted rather than expanding the
   function in-line.  EXP is the expression that is a call to the builtin
   function.  */
//原型 expand_builtin_mathfn_3 builtins.cc
static rtx expand_builtin_sincos (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

   rtx op0, op1, op2, target1, target2;
   machine_mode mode;
   tree arg, sinp, cosp;
   int result;
   location_t loc = EXPR_LOCATION (exp);
   tree alias_type, alias_off;
   if (!validate_arglist (exp, REAL_TYPE,POINTER_TYPE, POINTER_TYPE, VOID_TYPE))
      return NULL_RTX;
   arg = CALL_EXPR_ARG (exp, 0);
   sinp = CALL_EXPR_ARG (exp, 1);
   cosp = CALL_EXPR_ARG (exp, 2);
   /* Make a suitable register to place result in.  */
   mode = TYPE_MODE (TREE_TYPE (arg));
   mode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (arg),mode);
   /* Check if sincos insn is available, otherwise emit the call.  */
   if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,sincos_optab, mode) == CODE_FOR_nothing)
      return NULL_RTX;

   target1 = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
   target2 = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
   op0 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg);
   alias_type = mtcs_tree_build_pointer_type_for_mode/*!build_pointer_type_for_mode*/(mtcsTree,TREE_TYPE (arg), ptr_mode, true);
   alias_off = mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,alias_type, 0);
   op1 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,
         loc, MEM_REF, TREE_TYPE (arg),sinp, alias_off));
   op2 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,
         loc, MEM_REF, TREE_TYPE (arg),cosp, alias_off));
   /* Compute into target1 and target2.
   Set TARGET to wherever the result comes back.  */
   result = mtcs_optabs_expand_twoval_unop/*!expand_twoval_unop*/(mtcsOptabs,sincos_optab, op0, target2, target1, 0);
   gcc_assert (result);
   /* Move target1 and target2 to the memory locations indicated
   by op1 and op2.  */
   mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,op1, target1);
   mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,op2, target2);
   return const0_rtx;
}

static nboolean excutePreds( MtcsBuiltins *self,insn_code icode,int opno,rtx target, machine_mode target_mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
   //有三个参数
   insn_operand_predicate_fn fn=mtcsOutput->insn_data[(int) icode].operand[opno].predicate;
   if(!fn)
      return false;
   bool ret;
   if(mtcs_preds_is_common(mtcsPreds,(void*)fn)){
      mtcs_insn_operand_predicate_fn *fx=&fn;
      ret= (*fx)(mtcsPreds,target, target_mode);
   }else{
      mtcs_insn_operand_predicate_fn *fx=&fn;
      ret =fn(target, target_mode);
   }
   return ret;
}

/* Expand call EXP to the fegetround builtin (from C99 fenv.h), returning the
   result and setting it in TARGET.  Otherwise return NULL_RTX on failure.  */
//原型 expand_builtin_fegetround builtins.cc
static rtx expand_builtin_fegetround (MtcsBuiltins *self,tree exp, rtx target, machine_mode target_mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);

   if (!validate_arglist (exp, VOID_TYPE))
      return NULL_RTX;
   insn_code icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,
   fegetround_optab, mtcsMode->modes.M_SImode);
   if (icode == CODE_FOR_nothing)
      return NULL_RTX;

   if (target == 0 || GET_MODE (target) != target_mode
   || !excutePreds/*(*mtcsOutput->insn_data[icode].operand[0].predicate)*/(self,icode,0,target, target_mode))
      target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,target_mode);

   rtx pat = MTCS_GEN_FCN(icode) (target);
   if (!pat)
      return NULL_RTX;
   mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,pat);
   return target;
}

/* Expand call EXP to either feclearexcept or feraiseexcept builtins (from C99
   fenv.h), returning the result and setting it in TARGET.  Otherwise return
   NULL_RTX on failure.  */
//原型 expand_builtin_feclear_feraise_except builtins.cc
static rtx expand_builtin_feclear_feraise_except (MtcsBuiltins *self,tree exp, rtx target,
                   machine_mode target_mode, optab op_optab)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);

   if (!validate_arglist (exp, INTEGER_TYPE, VOID_TYPE))
      return NULL_RTX;
   rtx op0 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,CALL_EXPR_ARG (exp, 0));

   insn_code icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,op_optab, mtcsMode->modes.M_SImode);
   if (icode == CODE_FOR_nothing)
      return NULL_RTX;

   if (!excutePreds/*!(*insn_data[icode].operand[1].predicate)*/(self,icode,1,op0, GET_MODE (op0)))
      return NULL_RTX;

   if (target == 0 || GET_MODE (target) != target_mode
   || !excutePreds/*!(*mtcsOutput->insn_data[icode].operand[0].predicate)*/(self,icode,0,target, target_mode))
      target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,target_mode);

   rtx pat = MTCS_GEN_FCN(icode) (target, op0);
   if (!pat)
      return NULL_RTX;
   mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,pat);
   return target;
}


/* Save the state required to perform an untyped call with the same
   arguments as were passed to the current function.  */
//原型 expand_builtin_apply_args_1 builtins.cc
static rtx expand_builtin_apply_args_1 (MtcsBuiltins *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
   rtx registers, tem;
   int size, align, regno;
   fixed_size_mode mode;
   rtx struct_incoming_value =target_calls_struct_value_rtx/*!targetm.calls.struct_value_rtx*/(mtcsMachine->calls,
         cfun ? TREE_TYPE (cfun->decl) : 0, 1);
   /* Create a block where the arg-pointer, structure value address,
   and argument registers can be saved.  */
   registers = mtcs_func_assign_stack_local/*!assign_stack_local*/(mtcsFunc,
   mtcsMode->modes.M_BLKmode, apply_args_size(self), -1);
   /* Walk past the arg-pointer and structure value address.  */
   size = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,pMode);
   if (target_calls_struct_value_rtx/*!targetm.calls.struct_value_rtx*/(mtcsMachine->calls,
         cfun ? TREE_TYPE (cfun->decl) : 0, 0))
      size += mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,pMode);

   /* Save each register used in calling a function to the block.  */
   for (regno = 0; regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); regno++)
      if ((mode = apply_args_mode[regno]) != VOIDmode){
         align = mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode) / BITS_PER_UNIT;
         if (size % align != 0)
            size = CEIL (size, align) * align;

         tem = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mode, mtcs_reg_get_incoming_regno/*!INCOMING_REGNO*/(mtcsReg,regno));

         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,
         mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,registers, mode, size), tem);
         size += mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);
      }
   /* Save the arg pointer to the block.  */
   tem = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,mtcsRtlData/*!crtl*/->args.internal_arg_pointer);
   /* We need the pointer as the caller actually passed them to us, not
   as we might have pretended they were passed.  Make sure it's a valid
   operand, as emit_move_insn isn't expected to handle a PLUS.  */
   if (mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc))
      tem = mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,
               pMode, tem, mtcsRtlData/*!crtl*/->args.pretend_args_size), NULL_RTX);

   mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,registers, pMode, 0), tem);

   size = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,pMode);

   /* Save the structure value address unless this is passed as an
   "invisible" first argument.  */
   if (struct_incoming_value)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,
            mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,registers, pMode, size),
            mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,struct_incoming_value));

   /* Return the address of the block.  */
   return mtcs_explow_copy_addr_to_reg/*!copy_addr_to_reg*/(mtcsExplow,XEXP (registers, 0));
}

/* __builtin_apply_args returns block of memory allocated on
   the stack into which is stored the arg pointer, structure
   value address, static chain, and all the registers that might
   possibly be used in performing a function call.  The code is
   moved to the start of the function so the incoming values are
   saved.  */
//原型 expand_builtin_apply_args builtins.cc
static rtx expand_builtin_apply_args (MtcsBuiltins *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);

   /* Don't do __builtin_apply_args more than once in a function.
   Save the result of the first call and reuse it.  */
   if (mtcsRtlData/*!crtl*/->expr.x_apply_args_value/*!apply_args_value function.h定义的宏*/!= 0)
      return mtcsRtlData/*!crtl*/->expr.x_apply_args_value/*!apply_args_value function.h定义的宏*/;

   {
      /* When this function is called, it means that registers must be
      saved on entry to this function.  So we migrate the
      call to the first insn of this function.  */
      rtx temp;
      mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
      temp = expand_builtin_apply_args_1(self);
      rtx_insn *seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      mtcsRtlData/*!crtl*/->expr.x_apply_args_value/*!apply_args_value function.h定义的宏*/ = temp;
      /* Put the insns after the NOTE that starts the function.
      If this is inside a start_sequence, make the outer-level insn
      chain current, so the code is placed at the start of the
      function.  If internal_arg_pointer is a non-virtual pseudo,
      it needs to be placed after the function that initializes
      that pseudo.  */
      mtcs_emit_push_topmost_sequence/*!push_topmost_sequence*/(mtcsEmit);
      if (REG_P (mtcsRtlData/*!crtl*/->args.internal_arg_pointer)
        && REGNO (mtcsRtlData/*!crtl*/->args.internal_arg_pointer) >
           mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg))
         mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,seq, parm_birth_insn);
      else
         mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,
               seq, NEXT_INSN (mtcs_cfg_rtl_entry_of_function/*!entry_of_function*/(mtcsCfgRtl)));
      mtcs_emit_pop_topmost_sequence/*!pop_topmost_sequence*/(mtcsEmit);
      return temp;
   }
}




/* Return the size required for the block returned by __builtin_apply_args,
   and initialize apply_args_mode.  */
//原型 apply_args_size builtins.cc
static int apply_args_size (MtcsBuiltins *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   int size = saved_apply_args_size;
   int align;
   unsigned int regno;
   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);

   /* The values computed by this function never change.  */
   if (size < 0){
      /* The first value is the incoming arg-pointer.  */
      size = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,pMode);
      /* The second value is the structure value address unless this is
      passed as an "invisible" first argument.  */
      if (target_calls_struct_value_rtx/*!targetm.calls.struct_value_rtx*/(mtcsMachine->calls,
            cfun ? TREE_TYPE (cfun->decl) : 0, 0))
         size += mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,pMode);

      for (regno = 0; regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); regno++)
         if (mtcs_func_is_function_arg_regno/*!FUNCTION_ARG_REGNO_P*/(mtcsFunc,regno)){
            fixed_size_mode mode =target_calls_get_raw_arg_mode/*!targetm.calls.get_raw_arg_mode*/(mtcsMachine->calls,regno);

            if (mode != VOIDmode){
               align = mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode) / BITS_PER_UNIT;
               if (size % align != 0)
                  size = CEIL (size, align) * align;
               size += mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);
               apply_args_mode[regno] = mode;
            }else
               apply_args_mode[regno] = mtcs_mode_as_a <fixed_size_mode> (mtcsMode,VOIDmode);
         }else
            apply_args_mode[regno] = mtcs_mode_as_a <fixed_size_mode> (mtcsMode,VOIDmode);

      saved_apply_args_size = size;
   }
   return size;
}

/* Return the size required for the block returned by __builtin_apply,
   and initialize apply_result_mode.  */
//原型 apply_result_size builtins.cc
static int apply_result_size (MtcsBuiltins *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   int size = saved_apply_result_size;
   int align, regno;

   /* The values computed by this function never change.  */
   if (size < 0){
      size = 0;
      for (regno = 0; regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); regno++)
         if (target_calls_function_value_regno_p/*!targetm.calls.function_value_regno_p*/(mtcsMachine->calls,regno)){
            fixed_size_mode mode =target_calls_get_raw_result_mode/*!targetm.calls.get_raw_result_mode*/(mtcsMachine->calls,regno);
            if (mode != VOIDmode){
               align = mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode) / BITS_PER_UNIT;
               if (size % align != 0)
                  size = CEIL (size, align) * align;
               size += mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);
               apply_result_mode[regno] = mode;
            }else
               apply_result_mode[regno] = mtcs_mode_as_a <fixed_size_mode> (mtcsMode,VOIDmode);
         }else
            apply_result_mode[regno] = mtcs_mode_as_a <fixed_size_mode> (mtcsMode,VOIDmode);

      /* Allow targets that use untyped_call and untyped_return to override
      the size so that machine-specific information can be stored here.  */
#ifdef APPLY_RESULT_SIZE
      size = APPLY_RESULT_SIZE;
#endif
      saved_apply_result_size = size;
   }
   return size;
}

/* Create a vector describing the result block RESULT.  If SAVEP is true,
   the result block is used to save the values; otherwise it is used to
   restore the values.  */
//原型 result_vector builtins.cc
static rtx result_vector (MtcsBuiltins *self,int savep, rtx result)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int regno, size, align, nelts;
   fixed_size_mode mode;
   rtx reg, mem;
   rtx *savevec = XALLOCAVEC (rtx, mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg));

   size = nelts = 0;
   for (regno = 0; regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); regno++)
      if ((mode = apply_result_mode[regno]) != VOIDmode){
         align = mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode) / BITS_PER_UNIT;
         if (size % align != 0)
            size = CEIL (size, align) * align;
         reg = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,
               mode, savep ? regno : mtcs_reg_get_incoming_regno/*!INCOMING_REGNO*/(mtcsReg,regno));
         mem = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,result, mode, size);
         savevec[nelts++] = (savep ? gen_rtx_SET (mem, reg) : gen_rtx_SET (reg, mem));
         size += mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);
      }
   return gen_rtx_PARALLEL (VOIDmode, gen_rtvec_v (nelts, savevec));
}

/* Perform an untyped call and save the state required to perform an
   untyped return of whatever value was returned by the given function.  */
//原型 expand_builtin_apply builtins.cc
static rtx expand_builtin_apply (MtcsBuiltins *self,rtx function, rtx arguments, rtx argsize)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
   scalar_int_mode scalarPmode=mtcs_mode_as_a<scalar_int_mode>(mtcsMode,pMode);

   int size, align, regno;
   fixed_size_mode mode;
   rtx incoming_args, result, reg, dest, src;
   rtx_call_insn *call_insn;
   rtx old_stack_level = 0;
   rtx call_fusage = 0;
   rtx struct_value = target_calls_struct_value_rtx/*!targetm.calls.struct_value_rtx*/(mtcsMachine->calls,
                        cfun ? TREE_TYPE (cfun->decl) : 0, 0);

   arguments = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,scalarPmode, arguments);
   /* Create a block where the return registers can be saved.  */
   result = mtcs_func_assign_stack_local/*!assign_stack_local*/(mtcsFunc,
   mtcsMode->modes.M_BLKmode, apply_result_size(self), -1);
   /* Fetch the arg pointer from the ARGUMENTS block.  */
   incoming_args = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,pMode);
   mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,incoming_args, gen_rtx_MEM (pMode, arguments));
   if (!mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc))
      incoming_args = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
            pMode, MINUS, incoming_args, argsize,incoming_args, 0, OPTAB_LIB_WIDEN);

   /* Push a new argument block and copy the arguments.  Do not allow
   the (potential) memcpy call below to interfere with our stack
   manipulations.  */
   mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
   /*!NO_DEFER_POP; expr.h 定义*/
   mtcsRtlData/*!crtl*/->expr.x_inhibit_defer_pop+=1;
   /* Save the stack with nonlocal if available.  */
   if (target_rtx_have_save_stack_nonlocal/*!targetm.have_save_stack_nonlocal*/(mtcsMachine->tmrtx))
      mtcs_explow_emit_stack_save/*!emit_stack_save*/(mtcsExplow,SAVE_NONLOCAL, &old_stack_level);
   else
      mtcs_explow_emit_stack_save/*!emit_stack_save*/(mtcsExplow,SAVE_BLOCK, &old_stack_level);

   /* Allocate a block of memory onto the stack and copy the memory
   arguments to the outgoing arguments address.  We can pass TRUE
   as the 4th argument because we just saved the stack pointer
   and will restore it right after the call.  */
   mtcs_explow_allocate_dynamic_stack_space/*!allocate_dynamic_stack_space*/(mtcsExplow,
            argsize, 0, mtcs_align_get_biggest_alignment/*!BIGGEST_ALIGNMENT*/(mtcsAlign), -1, true);

   /* Set DRAP flag to true, even though allocate_dynamic_stack_space
   may have already set current_function_calls_alloca to true.
   current_function_calls_alloca won't be set if argsize is zero,
   so we have to guarantee need_drap is true here.  */
   if (mtcs_func_is_support_stack_alignment/*!SUPPORTS_STACK_ALIGNMENT*/(mtcsFunc))
      mtcsRtlData/*!crtl*/->need_drap = true;

   dest = mtcs_rtl_get_virtual_outgoing_args_rtx/*!virtual_outgoing_args_rtx*/(mtcsRTL);
   if (!mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc)){
      if (CONST_INT_P (argsize))
         dest = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, dest, -INTVAL (argsize));
      else
         dest = gen_rtx_PLUS (pMode, dest, mtcs_expmed_negate_rtx/*!negate_rtx*/(mtcsExpmed,pMode, argsize));
   }
   dest = gen_rtx_MEM (mtcsMode->modes.M_BLKmode, dest);
   mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,dest, mtcs_func_get_parm_boundary/*!PARM_BOUNDARY*/(mtcsFunc));
   src = gen_rtx_MEM (mtcsMode->modes.M_BLKmode, incoming_args);
   mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,src, mtcs_func_get_parm_boundary/*!PARM_BOUNDARY*/(mtcsFunc));
   mtcs_expr_emit_block_move/*!emit_block_move*/(mtcsExpr,dest, src, argsize, BLOCK_OP_NORMAL);

   /* Refer to the argument block.  */
   apply_args_size(self);
   arguments = gen_rtx_MEM (mtcsMode->modes.M_BLKmode, arguments);
   mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,arguments, mtcs_func_get_parm_boundary/*!PARM_BOUNDARY*/(mtcsFunc));

   /* Walk past the arg-pointer and structure value address.  */
   size = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,pMode);
   if (struct_value)
      size += mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,pMode);

   /* Restore each of the registers previously saved.  Make USE insns
   for each of these registers for use in making the call.  */
   for (regno = 0; regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); regno++)
      if ((mode = apply_args_mode[regno]) != VOIDmode){
         align = mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode) / BITS_PER_UNIT;
         if (size % align != 0)
            size = CEIL (size, align) * align;
         reg = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mode, regno);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,reg,
               mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,arguments, mode, size));
         use_reg (&call_fusage, reg);
         size += mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);
      }

   /* Restore the structure value address unless this is passed as an
   "invisible" first argument.  */
   size = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,pMode);
   if (struct_value){
      rtx value = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,pMode);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,value,
            mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,arguments, pMode, size));
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,struct_value, value);
      if (REG_P (struct_value))
         mtcs_expr_use_reg/*!use_reg*/(mtcsExpr,&call_fusage, struct_value);
   }

   /* All arguments and registers used for the call are set up by now!  */
   function = mtcs_calls_prepare_call_address/*!prepare_call_address*/(mtcsCalls,NULL, function, NULL, &call_fusage, 0, 0);
   /* Ensure address is valid.  SYMBOL_REF is already valid, so no need,
   and we don't want to load it into a register as an optimization,
   because prepare_call_address already did it if it should be done.  */
   if (GET_CODE (function) != SYMBOL_REF)
      function = mtcs_explow_memory_address/*!memory_address*/(mtcsExplow,
            mtcs_mode_get_function_mode/*!FUNCTION_MODE*/(mtcsMode), function);

   /* Generate the actual call instruction and save the return value.  */
   if (target_rtx_have_untyped_call/*!targetm.have_untyped_call*/(mtcsMachine->tmrtx)){
      rtx mem = gen_rtx_MEM (mtcs_mode_get_function_mode/*!FUNCTION_MODE*/(mtcsMode), function);
      rtx_insn *seq = target_rtx_gen_untyped_call/*!targetm.gen_untyped_call*/(mtcsMachine->tmrtx,mem, result,
                                       result_vector(self,1, result));
      for (rtx_insn *insn = seq; insn; insn = NEXT_INSN (insn))
         if (CALL_P (insn))
            add_reg_note (insn, REG_UNTYPED_CALL, NULL_RTX);
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,seq);
   }else if (target_rtx_have_call_value/*!targetm.have_call_value */(mtcsMachine->tmrtx)){
      rtx valreg = 0;

      /* Locate the unique return register.  It is not possible to
      express a call that sets more than one return register using
      call_value; use untyped_call for that.  In fact, untyped_call
      only needs to save the return registers in the given block.  */
      for (regno = 0; regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); regno++)
         if ((mode = apply_result_mode[regno]) != VOIDmode){
            gcc_assert (!valreg); /* have_untyped_call required.  */
            valreg = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mode, regno);
         }

      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,
                  target_rtx_gen_call_value/*!targetm.gen_call_value*/(mtcsMachine->tmrtx,
                  valreg, gen_rtx_MEM (mtcs_mode_get_function_mode/*!FUNCTION_MODE*/(mtcsMode), function),
                  const0_rtx, NULL_RTX, const0_rtx));

      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,
                     mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,result, GET_MODE (valreg), 0), valreg);
   }else
      gcc_unreachable ();

   /* Find the CALL insn we just emitted, and attach the register usage
   information.  */
   call_insn = mtcs_rtl_data_last_call_insn/*!last_call_insn*/(mtcsRtlData);
   add_function_usage_to (call_insn, call_fusage);
   /* Restore the stack.  */
   if (target_rtx_have_save_stack_nonlocal/*!targetm.have_save_stack_nonlocal*/(mtcsMachine->tmrtx))
      mtcs_explow_emit_stack_restore/*!emit_stack_restore*/(mtcsExplow,SAVE_NONLOCAL, old_stack_level);
   else
      mtcs_explow_emit_stack_restore/*!emit_stack_restore*/(mtcsExplow,SAVE_BLOCK, old_stack_level);
   mtcs_expr_fixup_args_size_notes/*!fixup_args_size_notes*/(mtcsExpr,
   call_insn, mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData), 0);
   /*!OK_DEFER_POP;expr.h 定义*/
   mtcsRtlData/*!crtl*/->expr.x_inhibit_defer_pop-=1;
   /* Return the address of the result block.  */
   result = mtcs_explow_copy_addr_to_reg/*!copy_addr_to_reg*/(mtcsExplow,XEXP (result, 0));
   return mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,ptr_mode, result);
}



/* Perform an untyped return.  */
//原型 expand_builtin_apply builtins.cc
static void expand_builtin_return (MtcsBuiltins *self,rtx result)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsStmt *mtcsStmt=mtcs_target_get_stmt(mtcsTarget);
   MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

   int size, align, regno;
   fixed_size_mode mode;
   rtx reg;
   rtx_insn *call_fusage = 0;
   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
   scalar_int_mode scalarPmode=mtcs_mode_as_a<scalar_int_mode>(mtcsMode,pMode);

   result = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,scalarPmode, result);
   apply_result_size(self);
   result = gen_rtx_MEM (mtcsMode->modes.M_BLKmode, result);

   if (target_rtx_have_untyped_return/*!targetm.have_untyped_return*/(mtcsMachine->tmrtx)){
      rtx vector = result_vector(self,0, result);
      mtcs_emit_emit_jump_insn/*!emit_jump_insn*/(mtcsEmit,
            target_rtx_gen_untyped_return/*!targetm.gen_untyped_return*/(mtcsMachine->tmrtx,result, vector));
      mtcs_emit_emit_barrier/*!emit_barrier*/(mtcsEmit);
      return;
   }
   /* Restore the return value and note that each value is used.  */
   size = 0;
   for (regno = 0; regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg); regno++)
      if ((mode = apply_result_mode[regno]) != VOIDmode){
         align = mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode) / BITS_PER_UNIT;
         if (size % align != 0)
            size = CEIL (size, align) * align;
         reg = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mode, mtcs_reg_get_incoming_regno/*!INCOMING_REGNO*/(mtcsReg,regno));
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,reg, mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,result, mode, size));

         mtcs_emit_push_to_sequence/*!push_to_sequence*/(mtcsEmit,call_fusage);
         mtcs_emit_emit_use/*!emit_use*/(mtcsEmit,reg);
         call_fusage = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
         mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
         size += mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);
      }

   /* Put the USE insns before the return.  */
   mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,call_fusage);
   /* Return whatever values was restored by jumping directly to the end
   of the function.  */
   mtcs_stmt_expand_naked_return/*!expand_naked_return*/(mtcsStmt);
}

/* Expand a call to __builtin_saveregs, generating the result in TARGET,
   if that's convenient.  */
//原型 expand_builtin_saveregs builtins.h builtins.cc
rtx mtcs_builtins_expand_builtin_saveregs (MtcsBuiltins *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   rtx val;
   rtx_insn *seq;
   //#define saveregs_value (crtl->expr.x_saveregs_value) funciton.h
   /* Don't do __builtin_saveregs more than once in a function.
   Save the result of the first call and reuse it.  */
   if (mtcsRtlData->expr.x_saveregs_value/*!saveregs_value*/ != 0)
      return mtcsRtlData->expr.x_saveregs_value/*!saveregs_value*/;

   /* When this function is called, it means that registers must be
   saved on entry to this function.  So we migrate the call to the
   first insn of this function.  */
   mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
   /* Do whatever the machine needs done in this case.  */
   val = target_calls_expand_builtin_saveregs/*!targetm.calls.expand_builtin_saveregs*/(mtcsMachine->calls);
   seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   mtcsRtlData->expr.x_saveregs_value/*!saveregs_value*/ = val;
   /* Put the insns after the NOTE that starts the function.  If this
   is inside a start_sequence, make the outer-level insn chain current, so
   the code is placed at the start of the function.  */
   mtcs_emit_push_topmost_sequence/*!push_topmost_sequence*/(mtcsEmit);
   mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,
         seq, mtcs_cfg_rtl_entry_of_function/*!entry_of_function*/(mtcsCfgRtl));
   mtcs_emit_pop_topmost_sequence/*!pop_topmost_sequence*/(mtcsEmit);
   return val;
}

/* Expand expression EXP, which is a call to the memset builtin.  Return
   NULL_RTX if we failed the caller should emit a normal call, otherwise
   try to get the result in TARGET, if convenient (and in mode MODE if that's
   convenient).  */
//原型 expand_builtin_memset builtins.h builtins.cc
rtx mtcs_builtins_expand_builtin_memset (MtcsBuiltins *self,tree exp, rtx target, machine_mode mode)
{
   if (!validate_arglist (exp,POINTER_TYPE, INTEGER_TYPE, INTEGER_TYPE, VOID_TYPE))
      return NULL_RTX;
   tree dest = CALL_EXPR_ARG (exp, 0);
   tree val = CALL_EXPR_ARG (exp, 1);
   tree len = CALL_EXPR_ARG (exp, 2);
   return expand_builtin_memset_args(self,dest, val, len, target, mode, exp);
}


/* Expand a call to __builtin_next_arg.  */
//原型 expand_builtin_next_arg builtins.cc
static rtx expand_builtin_next_arg (MtcsBuiltins *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   /* Checking arguments is already done in fold_builtin_next_arg
   that must be called before this function.  */
   return mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,ptr_mode, add_optab,
         mtcsRtlData/*!crtl*/->args.internal_arg_pointer,
         mtcsRtlData/*!crtl*/->args.arg_offset_rtx, NULL_RTX, 0, OPTAB_LIB_WIDEN);
}

/* Expand a call to __builtin___clear_cache.  */
//原型 expand_builtin___clear_cache builtins.cc
static void expand_builtin___clear_cache (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   tree begin, end;
   rtx begin_rtx, end_rtx;
   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);

   /* We must not expand to a library call.  If we did, any
   fallback library function in libgcc that might contain a call to
   __builtin___clear_cache() would recurse infinitely.  */
   if (!validate_arglist (exp, POINTER_TYPE, POINTER_TYPE, VOID_TYPE)){
      error ("both arguments to %<__builtin___clear_cache%> must be pointers");
      return;
   }

   begin = CALL_EXPR_ARG (exp, 0);
   begin_rtx = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,begin, NULL_RTX, pMode, EXPAND_NORMAL);
   end = CALL_EXPR_ARG (exp, 1);
   end_rtx = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,end, NULL_RTX, pMode, EXPAND_NORMAL);
   mtcs_builtins_maybe_emit_call_builtin___clear_cache(self,begin_rtx, end_rtx);
}

/* Expand a call EXP to __builtin_classify_type.  */
//原型 expand_builtin_classify_type builtins.cc
static rtx expand_builtin_classify_type (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   if (call_expr_nargs (exp))
      return mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,type_to_class (TREE_TYPE (CALL_EXPR_ARG (exp, 0))));
   return mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,no_type_class);
}

/* Given TEM, a pointer to a stack frame, follow the dynamic chain COUNT
   times to get the address of either a higher stack frame, or a return
   address located within it (depending on FNDECL_CODE).  */
//原型 expand_builtin_return_addr builtins.cc
static rtx expand_builtin_return_addr (MtcsBuiltins *self,enum built_in_function fndecl_code, int count)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);

   int i;
   rtx tem = INITIAL_FRAME_ADDRESS_RTX;
   if (tem == NULL_RTX){
      /* For a zero count with __builtin_return_address, we don't care what
      frame address we return, because target-specific definitions will
      override us.  Therefore frame pointer elimination is OK, and using
      the soft frame pointer is OK.

      For a nonzero count, or a zero count with __builtin_frame_address,
      we require a stable offset from the current frame pointer to the
      previous one, so we must use the hard frame pointer, and
      we must disable frame pointer elimination.  */
      if (count == 0 && fndecl_code == BUILT_IN_RETURN_ADDRESS)
         tem = mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL);
      else{
         tem = mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL);
         /* Tell reload not to eliminate the frame pointer.  */
         mtcsRtlData/*!crtl*/->accesses_prior_frames = 1;
      }
   }

   if (count > 0)
      mtcs_func_setup_frame_addresses/*!SETUP_FRAME_ADDRESSES*/(mtcsFunc);

   /* On the SPARC, the return address is not in the frame, it is in a
   register.  There is no way to access it off of the current frame
   pointer, but it can be accessed off the previous frame pointer by
   reading the value from the register window save area.  */
   if (RETURN_ADDR_IN_PREVIOUS_FRAME && fndecl_code == BUILT_IN_RETURN_ADDRESS)
      count--;

   /* Scan back COUNT frames to the specified frame.  */
   for (i = 0; i < count; i++){
      /* Assume the dynamic chain pointer is in the word that the
      frame address points to, unless otherwise specified.  */
      tem = DYNAMIC_CHAIN_ADDRESS (tem);
      tem = mtcs_explow_memory_address/*!memory_address*/(mtcsExplow,pMode, tem);
      tem = gen_frame_mem (pMode, tem);
      tem = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,tem);
   }

   /* For __builtin_frame_address, return what we've got.  But, on
   the SPARC for example, we may have to add a bias.  */
   if (fndecl_code == BUILT_IN_FRAME_ADDRESS)
      return FRAME_ADDR_RTX (tem);

   /* For __builtin_return_address, get the return address from that frame.  */
#ifdef RETURN_ADDR_RTX
   tem = RETURN_ADDR_RTX (count, tem);
#else
   tem = mtcs_explow_memory_address/*!memory_address*/(mtcsExplow,pMode,
   mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, tem, mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,pMode)));
   tem = gen_frame_mem (pMode, tem);
#endif
   return tem;
}

/* Expand a call to one of the builtin functions __builtin_frame_address or
   __builtin_return_address.  */
//原型 expand_builtin_frame_address builtins.cc
static rtx expand_builtin_frame_address (MtcsBuiltins *self,tree fndecl, tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

   /* The argument must be a nonnegative integer constant.
   It counts the number of frames to scan up the stack.
   The value is either the frame pointer value or the return
   address saved in that frame.  */
   if (call_expr_nargs (exp) == 0)
      /* Warning about missing arg was already issued.  */
      return const0_rtx;
   else if (! tree_fits_uhwi_p (CALL_EXPR_ARG (exp, 0))){
      error ("invalid argument to %qD", fndecl);
      return const0_rtx;
   }else{
      /* Number of frames to scan up the stack.  */
      unsigned HOST_WIDE_INT count = tree_to_uhwi (CALL_EXPR_ARG (exp, 0));
      rtx tem = expand_builtin_return_addr(self,DECL_FUNCTION_CODE (fndecl), count);
      /* Some ports cannot access arbitrary stack frames.  */
      if (tem == NULL){
         warning (0, "unsupported argument to %qD", fndecl);
         return const0_rtx;
      }
      if (count){
         /* Warn since no effort is made to ensure that any frame
         beyond the current one exists or can be safely reached.  */
         warning (OPT_Wframe_address, "calling %qD with a nonzero argument is unsafe", fndecl);
      }
      /* For __builtin_frame_address, return what we've got.  */
      if (DECL_FUNCTION_CODE (fndecl) == BUILT_IN_FRAME_ADDRESS)
         return tem;

      if (!REG_P (tem) && ! CONSTANT_P (tem))
         tem = mtcs_explow_copy_addr_to_reg/*!copy_addr_to_reg*/(mtcsExplow,tem);
      return tem;
   }
}

//原型 expand_builtin_frame_address builtins.cc
static rtx expand_builtin_stack_address (MtcsBuiltins *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

//#ifdef POINTERS_EXTEND_UNSIGNED
//# define STACK_UNSIGNED POINTERS_EXTEND_UNSIGNED
//#else
//# define STACK_UNSIGNED true
//#endif
   //原型 STACK_UNSIGNED builtins.cc
  int STACK_UNSIGNED=1;
  if(mtcs_config_ifdef(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED))
     STACK_UNSIGNED=mtcs_config_get_value(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED);
  rtx ret = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,ptr_mode,
        mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)),
              STACK_UNSIGNED);

#ifdef STACK_ADDRESS_OFFSET
  /* Unbias the stack pointer, bringing it to the boundary between the
     stack area claimed by the active function calling this builtin,
     and stack ranges that could get clobbered if it called another
     function.  It should NOT encompass any stack red zone, that is
     used in leaf functions.

     On SPARC, the register save area is *not* considered active or
     used by the active function, but rather as akin to the area in
     which call-preserved registers are saved by callees.  This
     enables __strub_leave to clear what would otherwise overlap with
     its own register save area.

     If the address is computed too high or too low, parts of a stack
     range that should be scrubbed may be left unscrubbed, scrubbing
     may corrupt active portions of the stack frame, and stack ranges
     may be doubly-scrubbed by caller and callee.

     In order for it to be just right, the area delimited by
     @code{__builtin_stack_address} and @code{__builtin_frame_address
     (0)} should encompass caller's registers saved by the function,
     local on-stack variables and @code{alloca} stack areas.
     Accumulated outgoing on-stack arguments, preallocated as part of
     a function's own prologue, are to be regarded as part of the
     (caller) function's active area as well, whereas those pushed or
     allocated temporarily for a call are regarded as part of the
     callee's stack range, rather than the caller's.  */
  ret = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,ptr_mode, ret, STACK_ADDRESS_OFFSET);
#endif

  return mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,ptr_mode, ret);
}

/* Expand a call to builtin function __builtin_strub_enter.  */
//原型 expand_builtin_strub_enter builtins.cc
static rtx expand_builtin_strub_enter (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

   if (!validate_arglist (exp, POINTER_TYPE, VOID_TYPE))
      return NULL_RTX;
   if (mtcsOptionsItem->x_optimize < 1 || mtcsOptionsItem->x_flag_no_inline)
      return NULL_RTX;

   rtx stktop = expand_builtin_stack_address(self);
   tree wmptr = CALL_EXPR_ARG (exp, 0);
   tree wmtype = TREE_TYPE (TREE_TYPE (wmptr));
   tree wmtree = mtcs_const_fold_build2(mtcsConst,MEM_REF, wmtype, wmptr,
   mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (wmptr), 0));
   rtx wmark = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,wmtree, NULL_RTX, ptr_mode, EXPAND_MEMORY);
   mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,wmark, stktop);
   return const0_rtx;
}


/* Expand a call to builtin function __builtin_strub_update.  */
//原型 expand_builtin_strub_enter builtins.cc
static rtx expand_builtin_strub_update (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

   if (!validate_arglist (exp, POINTER_TYPE, VOID_TYPE))
      return NULL_RTX;

   if (mtcsOptionsItem->x_optimize < 2 || mtcsOptionsItem->x_flag_no_inline)
      return NULL_RTX;

   rtx stktop = expand_builtin_stack_address(self);

#ifdef RED_ZONE_SIZE
   /* Here's how the strub enter, update and leave functions deal with red zones.

   If it weren't for red zones, update, called from within a strub context,
   would bump the watermark to the top of the stack.  Enter and leave, running
   in the caller, would use the caller's top of stack address both to
   initialize the watermark passed to the callee, and to start strubbing the
   stack afterwards.

   Ideally, we'd update the watermark so as to cover the used amount of red
   zone, and strub starting at the caller's other end of the (presumably
   unused) red zone.  Normally, only leaf functions use the red zone, but at
   this point we can't tell whether a function is a leaf, nor can we tell how
   much of the red zone it uses.  Furthermore, some strub contexts may have
   been inlined so that update and leave are called from the same stack frame,
   and the strub builtins may all have been inlined, turning a strub function
   into a leaf.

   So cleaning the range from the caller's stack pointer (one end of the red
   zone) to the (potentially inlined) callee's (other end of the) red zone
   could scribble over the caller's own red zone.

   We avoid this possibility by arranging for callers that are strub contexts
   to use their own watermark as the strub starting point.  So, if A calls B,
   and B calls C, B will tell A to strub up to the end of B's red zone, and
   will strub itself only the part of C's stack frame and red zone that
   doesn't overlap with B's.  With that, we don't need to know who's leaf and
   who isn't: inlined calls will shrink their strub window to zero, each
   remaining call will strub some portion of the stack, and eventually the
   strub context will return to a caller that isn't a strub context itself,
   that will therefore use its own stack pointer as the strub starting point.
   It's not a leaf, because strub contexts can't be inlined into non-strub
   contexts, so it doesn't use the red zone, and it will therefore correctly
   strub up the callee's stack frame up to the end of the callee's red zone.
   Neat!  */
   if (true /* (flags_from_decl_or_type (current_function_decl) & ECF_LEAF) */)
   {
      poly_int64 red_zone_size = RED_ZONE_SIZE;
      #if STACK_GROWS_DOWNWARD
         red_zone_size = -red_zone_size;
      #endif
      stktop = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,ptr_mode, stktop, red_zone_size);
      stktop = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,ptr_mode, stktop);
   }
#endif

   tree wmptr = CALL_EXPR_ARG (exp, 0);
   tree wmtype = TREE_TYPE (TREE_TYPE (wmptr));
   tree wmtree = mtcs_const_fold_build2(mtcsConst,MEM_REF, wmtype, wmptr,
   mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (wmptr), 0));
   rtx wmark = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,wmtree, NULL_RTX, ptr_mode, EXPAND_MEMORY);

   rtx wmarkr = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,ptr_mode, wmark);
   int STACK_UNSIGNED=1;
   if(mtcs_config_ifdef(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED))
      STACK_UNSIGNED=mtcs_config_get_value(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED);

//#if ! STACK_GROWS_DOWNWARD
//# define STACK_TOPS GT
//#else
//# define STACK_TOPS LT
//#endif

   int STACK_TOPS = LT;
   if(!mtcs_func_get_stack_grows_downward(mtcsFunc))
      STACK_TOPS=GT;


   rtx_code_label *lab = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
   mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,stktop, wmarkr,
         STACK_TOPS, STACK_UNSIGNED,ptr_mode, NULL_RTX, lab, NULL,profile_probability::very_likely ());
   mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,wmark, stktop);

   /* If this is an inlined strub function, also bump the watermark for the
   enclosing function.  This avoids a problem with the following scenario: A
   calls B and B calls C, and both B and C get inlined into A.  B allocates
   temporary stack space before calling C.  If we don't update A's watermark,
   we may use an outdated baseline for the post-C strub_leave, erasing B's
   temporary stack allocation.  We only need this if we're fully expanding
   strub_leave inline.  */
   tree xwmptr = (mtcsOptionsItem->x_optimize > 2
                     ? strub_watermark_parm (current_function_decl) : wmptr);
   if (wmptr != xwmptr){
      wmptr = xwmptr;
      wmtype = TREE_TYPE (TREE_TYPE (wmptr));
      wmtree = mtcs_const_fold_build2(mtcsConst,MEM_REF, wmtype, wmptr,
            mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (wmptr), 0));
      wmark = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,wmtree, NULL_RTX, ptr_mode, EXPAND_MEMORY);
      wmarkr = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,ptr_mode, wmark);

      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,stktop, wmarkr, STACK_TOPS, STACK_UNSIGNED,
             ptr_mode, NULL_RTX, lab, NULL,profile_probability::very_likely ());
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,wmark, stktop);
   }
   mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,lab);
   return const0_rtx;
}

/* Expand a call to builtin function __builtin_strub_leave.  */
//原型 expand_builtin_strub_leave builtins.cc
static rtx expand_builtin_strub_leave (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
   if (!validate_arglist (exp, POINTER_TYPE, VOID_TYPE))
      return NULL_RTX;
   if (mtcsOptionsItem->x_optimize < 2 || mtcsOptionsItem->x_optimize_size || mtcsOptionsItem->x_flag_no_inline)
      return NULL_RTX;
   rtx stktop = NULL_RTX;
   if (tree wmptr = (mtcsOptionsItem->x_optimize ? strub_watermark_parm (current_function_decl) : NULL_TREE)){
      tree wmtype = TREE_TYPE (TREE_TYPE (wmptr));
      tree wmtree = mtcs_const_fold_build2(mtcsConst,MEM_REF, wmtype, wmptr,
            mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (wmptr), 0));
      rtx wmark = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,wmtree, NULL_RTX, ptr_mode, EXPAND_MEMORY);
      stktop = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,ptr_mode, wmark);
   }

   if (!stktop)
      stktop = expand_builtin_stack_address(self);

   tree wmptr = CALL_EXPR_ARG (exp, 0);
   tree wmtype = TREE_TYPE (TREE_TYPE (wmptr));
   tree wmtree = mtcs_const_fold_build2(mtcsConst,MEM_REF, wmtype, wmptr,
         mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (wmptr), 0));
   rtx wmark = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,wmtree, NULL_RTX, ptr_mode, EXPAND_MEMORY);
   rtx wmarkr = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,ptr_mode, wmark);
   rtx base,end;
   if(!mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc)){
//#if ! STACK_GROWS_DOWNWARD
       base = stktop;
       end = wmarkr;
   }else{
//#else
       base = wmarkr;
       end = stktop;
//#endif
   }

   /* We're going to modify it, so make sure it's not e.g. the stack pointer.  */
   base = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,base);
   //原型 STACK_UNSIGNED builtins.cc
   int STACK_UNSIGNED=1;
   if(mtcs_config_ifdef(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED))
      STACK_UNSIGNED=mtcs_config_get_value(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED);

   rtx_code_label *done = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
   mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,base, end, LT, STACK_UNSIGNED,
            ptr_mode, NULL_RTX, done, NULL,profile_probability::very_likely ());

   if (mtcsOptionsItem->x_optimize < 3)
      mtcs_calls_expand_call/*!expand_call*/(mtcsCalls,exp, NULL_RTX, true);
   else{
      /* Ok, now we've determined we want to copy the block, so convert the
      addresses to Pmode, as needed to dereference them to access ptr_mode
      memory locations, so that we don't have to convert anything within the
      loop.  */
      base =  mtcs_explow_memory_address/*!memory_address*/(mtcsExplow,ptr_mode, base);
      end =  mtcs_explow_memory_address/*!memory_address*/(mtcsExplow,ptr_mode, end);

      rtx zero = mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,const0_rtx, NULL_RTX);
      int ulen = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,ptr_mode);

      /* ??? It would be nice to use setmem or similar patterns here,
      but they do not necessarily obey the stack growth direction,
      which has security implications.  We also have to avoid calls
      (memset, bzero or any machine-specific ones), which are
      likely unsafe here (see TARGET_STRUB_MAY_USE_MEMSET).  */
      rtx_code_label *loop;
      if(!mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc)){
   //#if ! STACK_GROWS_DOWNWARD
         rtx incr = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, base, ulen);
         rtx dstm = gen_rtx_MEM (ptr_mode, base);

         loop = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,loop);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,dstm, zero);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,base, mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,incr, NULL_RTX));
   //#else
      }else{
         rtx decr = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, end, -ulen);
         rtx dstm = gen_rtx_MEM (ptr_mode, end);

         loop = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,loop);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,end, mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,decr, NULL_RTX));
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,dstm, zero);
    //#endif
      }
      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,base, end, LT, STACK_UNSIGNED,
            pMode, NULL_RTX, NULL, loop, profile_probability::very_likely ());
   }
   mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,done);
   return const0_rtx;
}

/* Expand EXP, a call to the alloca builtin.  Return NULL_RTX if we
   failed and the caller should emit a normal call.  */
//原型 expand_builtin_alloca builtins.cc
static rtx expand_builtin_alloca (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

   rtx op0;
   rtx result;
   unsigned int align;
   tree fndecl = get_callee_fndecl (exp);
   HOST_WIDE_INT max_size;
   enum built_in_function fcode = DECL_FUNCTION_CODE (fndecl);
   bool alloca_for_var = CALL_ALLOCA_FOR_VAR_P (exp);
   bool valid_arglist  = (fcode == BUILT_IN_ALLOCA_WITH_ALIGN_AND_MAX
   ? validate_arglist (exp, INTEGER_TYPE, INTEGER_TYPE, INTEGER_TYPE, VOID_TYPE)
   : fcode == BUILT_IN_ALLOCA_WITH_ALIGN
   ? validate_arglist (exp, INTEGER_TYPE, INTEGER_TYPE, VOID_TYPE)
   : validate_arglist (exp, INTEGER_TYPE, VOID_TYPE));

   if (!valid_arglist)
      return NULL_RTX;

   /* Compute the argument.  */
   op0 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,CALL_EXPR_ARG (exp, 0));

   /* Compute the alignment.  */
   align = (fcode == BUILT_IN_ALLOCA ? mtcs_align_get_biggest_alignment/*!BIGGEST_ALIGNMENT*/(mtcsAlign)
   : TREE_INT_CST_LOW (CALL_EXPR_ARG (exp, 1)));

   /* Compute the maximum size.  */
   max_size = (fcode == BUILT_IN_ALLOCA_WITH_ALIGN_AND_MAX ? TREE_INT_CST_LOW (CALL_EXPR_ARG (exp, 2)) : -1);

   /* Allocate the desired space.  If the allocation stems from the declaration
   of a variable-sized object, it cannot accumulate.  */
   result = mtcs_explow_allocate_dynamic_stack_space/*!allocate_dynamic_stack_space*/(mtcsExplow,
   op0, 0, align, max_size, alloca_for_var);
   result = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,ptr_mode, result);

   /* Dynamic allocations for variables are recorded during gimplification.  */
   if (!alloca_for_var && (mtcsOptionsItem->x_flag_callgraph_info & CALLGRAPH_INFO_DYNAMIC_ALLOC))
      record_dynamic_alloc (exp);

   return result;
}


/* Emit a call to __asan_allocas_unpoison call in EXP.  Add to second argument
   of the call virtual_stack_dynamic_rtx - stack_pointer_rtx, which is the
   STACK_DYNAMIC_OFFSET value.  See motivation for this in comment to
   handle_builtin_stack_restore function.  */
//原型 expand_builtin_alloca builtins.cc
static rtx expand_asan_emit_allocas_unpoison (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);

   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);

   tree arg0 = CALL_EXPR_ARG (exp, 0);
   tree arg1 = CALL_EXPR_ARG (exp, 1);
   rtx top = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,arg0, NULL_RTX, ptr_mode, EXPAND_NORMAL);
   rtx bot = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,arg1, NULL_RTX, ptr_mode, EXPAND_NORMAL);
   rtx off = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,pMode, MINUS,
   mtcs_rtl_get_virtual_stack_dynamic_rtx/*!virtual_stack_dynamic_rtx*/(mtcsRTL),
      mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL), NULL_RTX, 0, OPTAB_LIB_WIDEN);
   off = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,ptr_mode, pMode, off, 0);
   bot = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,ptr_mode, PLUS, bot, off, NULL_RTX, 0,
   OPTAB_LIB_WIDEN);
   rtx ret = mtcs_libfuncs_init_one_libfunc/*!init_one_libfunc*/(mtcsLibfuncs,"__asan_allocas_unpoison");
   ret = mtcs_calls_emit_library_call_value/*!emit_library_call_value*/(mtcsCalls,ret, NULL_RTX, LCT_NORMAL, ptr_mode,
                           top, ptr_mode, bot, ptr_mode);
   return ret;
}

/* Emit code to save the current value of stack.  */
//原型 expand_stack_save builtins.cc
static rtx expand_stack_save (MtcsBuiltins *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   rtx ret = NULL_RTX;
   mtcs_explow_emit_stack_save/*!emit_stack_save*/(mtcsExplow,SAVE_BLOCK, &ret);
   return ret;
}

/* Emit code to restore the current value of stack.  */
//原型 expand_stack_restore builtins.cc
static void expand_stack_restore (MtcsBuiltins *self,tree var)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

   rtx_insn *prev;
   rtx sa = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,var);
   sa = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,mtcs_mode_get_Pmode(mtcsMode), sa);
   prev = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
   mtcs_explow_emit_stack_restore/*!emit_stack_restore*/(mtcsExplow,SAVE_BLOCK, sa);
   mtcs_explow_record_new_stack_level/*!record_new_stack_level*/(mtcsExplow);
   mtcs_expr_fixup_args_size_notes/*!fixup_args_size_notes*/(mtcsExpr,prev,
         mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData), 0);
}


/* Expand a call to bswap builtin in EXP.
   Return NULL_RTX if a normal call should be emitted rather than expanding the
   function in-line.  If convenient, the result should be placed in TARGET.
   SUBTARGET may be used as the target for computing one of EXP's operands.  */
//原型 expand_builtin_bswap builtins.cc
static rtx expand_builtin_bswap (MtcsBuiltins *self,machine_mode target_mode, tree exp, rtx target,
            rtx subtarget)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

   tree arg;
   rtx op0;
   if (!validate_arglist (exp, INTEGER_TYPE, VOID_TYPE))
      return NULL_RTX;
   arg = CALL_EXPR_ARG (exp, 0);
   op0 = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,arg,
   subtarget && GET_MODE (subtarget) == target_mode ? subtarget : NULL_RTX,target_mode, EXPAND_NORMAL);
   if (GET_MODE (op0) != target_mode)
      op0 = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,target_mode, op0, 1);
   target = mtcs_optabs_expand_unop/*!expand_unop*/(mtcsOptabs,target_mode, bswap_optab, op0, target, 1);
   gcc_assert (target);
   return mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,target_mode, target, 1);
}

/* Expand a call to a unary builtin in EXP.
   Return NULL_RTX if a normal call should be emitted rather than expanding the
   function in-line.  If convenient, the result should be placed in TARGET.
   SUBTARGET may be used as the target for computing one of EXP's operands.  */
//原型 expand_builtin_unop builtins.cc
static rtx expand_builtin_unop (MtcsBuiltins *self,machine_mode target_mode, tree exp, rtx target,
           rtx subtarget, optab op_optab)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

   rtx op0;
   if (!validate_arglist (exp, INTEGER_TYPE, VOID_TYPE))
      return NULL_RTX;
   /* Compute the argument.  */
   machine_mode typeMode=TYPE_MODE (TREE_TYPE (CALL_EXPR_ARG (exp, 0)));
   typeMode= mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (CALL_EXPR_ARG (exp, 0)),typeMode);
   op0 = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,CALL_EXPR_ARG (exp, 0),
           (subtarget  && (typeMode/*!TYPE_MODE (TREE_TYPE (CALL_EXPR_ARG (exp, 0)))*/
           == GET_MODE (subtarget))) ? subtarget : NULL_RTX,VOIDmode, EXPAND_NORMAL);
   /* Compute op, into TARGET if possible.
     Set TARGET to wherever the result comes back.  */
   target = mtcs_optabs_expand_unop/*!expand_unop*/(mtcsOptabs,typeMode/*!TYPE_MODE (TREE_TYPE (CALL_EXPR_ARG (exp, 0)))*/,
         op_optab, op0, target, op_optab != clrsb_optab);
   gcc_assert (target);
   return mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,target_mode, target, 0);
}


/* Expand expression EXP which is a call to the strlen builtin.  Return
   NULL_RTX if we failed and the caller should emit a normal call, otherwise
   try to get the result in TARGET, if convenient.  */
//原型 expand_builtin_strlen builtins.cc
static rtx expand_builtin_strlen (MtcsBuiltins *self,tree exp, rtx target,
             machine_mode target_mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);

   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
   if (!validate_arglist (exp, POINTER_TYPE, VOID_TYPE))
      return NULL_RTX;
   tree src = CALL_EXPR_ARG (exp, 0);
   /* If the length can be computed at compile-time, return it.  */
   if (tree len = c_strlen (src, 0))
      return mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,len, target, target_mode, EXPAND_NORMAL);
   /* If the length can be computed at compile-time and is constant
   integer, but there are side-effects in src, evaluate
   src for side-effects, then return len.
   E.g. x = strlen (i++ ? "xfoo" + 1 : "bar");
   can be optimized into: i++; x = 3;  */
   tree len = c_strlen (src, 1);
   if (len && TREE_CODE (len) == INTEGER_CST){
      mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,src, const0_rtx, VOIDmode, EXPAND_NORMAL);
      return mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,len, target, target_mode, EXPAND_NORMAL);
   }
   unsigned int align = get_pointer_alignment (src) / BITS_PER_UNIT;
   /* If SRC is not a pointer type, don't do this operation inline.  */
   if (align == 0)
      return NULL_RTX;

   /* Bail out if we can't compute strlen in the right mode.  */
   machine_mode insn_mode;
   enum insn_code icode = CODE_FOR_nothing;
   MTCS_FOR_EACH_MODE_FROM/*!FOR_EACH_MODE_FROM*/(mtcsMode,insn_mode, target_mode){
      icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,strlen_optab, insn_mode);
      if (icode != CODE_FOR_nothing)
         break;
   }
   if (insn_mode == VOIDmode)
      return NULL_RTX;

   /* Make a place to hold the source address.  We will not expand
   the actual source until we are sure that the expansion will
   not fail -- there are trees that cannot be expanded twice.  */
   rtx src_reg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,pMode);
   /* Mark the beginning of the strlen sequence so we can emit the
   source operand later.  */
   rtx_insn *before_strlen = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
   class expand_operand ops[4];
   create_output_operand (&ops[0], target, insn_mode);
   create_fixed_operand (&ops[1], gen_rtx_MEM (mtcsMode->modes.M_BLKmode, src_reg));
   mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[2], 0);
   mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[3], align);
   if (!maybe_expand_insn (icode, 4, ops))
      return NULL_RTX;
   /* Check to see if the argument was declared attribute nonstring
   and if so, issue a warning since at this point it's not known
   to be nul-terminated.  */
   maybe_warn_nonstring_arg (get_callee_fndecl (exp), exp);
   /* Now that we are assured of success, expand the source.  */
   mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
   rtx pat = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,src, src_reg, pMode, EXPAND_NORMAL);
   if (pat != src_reg){
      if(mtcs_config_ifdef(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED)){
         //#ifdef POINTERS_EXTEND_UNSIGNED
         if (GET_MODE (pat) != pMode)
            pat = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,pMode, pat,
                 mtcs_config_get_value/*!POINTERS_EXTEND_UNSIGNED*/(mtcsConfig,MTCS_POINTERS_EXTEND_UNSIGNED));
      //#endif
      }
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,src_reg, pat);
   }
   pat = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   if (before_strlen)
      mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,pat, before_strlen);
   else
      mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,
            pat, mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData));
   /* Return the value in the proper mode for this function.  */
   if (GET_MODE (ops[0].value) == target_mode)
      target = ops[0].value;
   else if (target != 0)
      mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,target, ops[0].value, 0);
   else
      target = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,target_mode, ops[0].value, 0);

   return target;
}


/* Expand call EXP to the strnlen built-in, returning the result
   and setting it in TARGET.  Otherwise return NULL_RTX on failure.  */
//原型 expand_builtin_strnlen builtins.cc
static rtx expand_builtin_strnlen (MtcsBuiltins *self,tree exp, rtx target, machine_mode target_mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);

   if (!validate_arglist (exp, POINTER_TYPE, INTEGER_TYPE, VOID_TYPE))
      return NULL_RTX;

   tree src = CALL_EXPR_ARG (exp, 0);
   tree bound = CALL_EXPR_ARG (exp, 1);

   if (!bound)
      return NULL_RTX;

   location_t loc = UNKNOWN_LOCATION;
   if (EXPR_HAS_LOCATION (exp))
      loc = EXPR_LOCATION (exp);
   /* FIXME: Change c_strlen() to return sizetype instead of ssizetype
   so these conversions aren't necessary.  */
   c_strlen_data lendata = { };
   tree len = c_strlen (src, 0, &lendata, 1);
   if (len)
      len = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, TREE_TYPE (bound), len);

   if (TREE_CODE (bound) == INTEGER_CST){
      if (!len)
         return NULL_RTX;
      len = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, MIN_EXPR, size_type_node, len, bound);
      return mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,len, target, target_mode, EXPAND_NORMAL);
   }

   if (TREE_CODE (bound) != SSA_NAME)
      return NULL_RTX;

   wide_int min, max;
   int_range_max r;
   get_global_range_query ()->range_of_expr (r, bound);
   if (r.varying_p () || r.undefined_p ())
      return NULL_RTX;
   min = r.lower_bound ();
   max = r.upper_bound ();

   if (!len || TREE_CODE (len) != INTEGER_CST){
      bool exact;
      lendata.decl = unterminated_array (src, &len, &exact);
      if (!lendata.decl)
         return NULL_RTX;
   }

   if (lendata.decl)
      return NULL_RTX;

   if (wi::gtu_p (min, wi::to_wide (len)))
      return mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,len, target, target_mode, EXPAND_NORMAL);

   len = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, MIN_EXPR, TREE_TYPE (len), len, bound);
   return mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,len, target, target_mode, EXPAND_NORMAL);
}

/* Expand into a movstr instruction, if one is available.  Return NULL_RTX if
   we failed, the caller should emit a normal call, otherwise try to
   get the result in TARGET, if convenient.
   Return value is based on RETMODE argument.  */
//原型 expand_movstr builtins.cc
static rtx expand_movstr (MtcsBuiltins *self,tree dest, tree src, rtx target, memop_ret retmode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);

   class expand_operand ops[3];
   rtx dest_mem;
   rtx src_mem;

   if (!target_rtx_have_movstr/*!targetm.have_movstr*/(mtcsMachine->tmrtx))
      return NULL_RTX;

   dest_mem = mtcs_builtins_get_memory_rtx/*!get_memory_rtx*/(self,dest, NULL);
   src_mem = mtcs_builtins_get_memory_rtx/*!get_memory_rtx*/(self,src, NULL);
   if (retmode == RETURN_BEGIN){
      target = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,pMode, XEXP (dest_mem, 0));
      dest_mem = mtcs_rtl_replace_equiv_address/*!replace_equiv_address*/(mtcsRTL,dest_mem, target);
   }

   create_output_operand (&ops[0],
   retmode != RETURN_BEGIN ? target : NULL_RTX, pMode);
   create_fixed_operand (&ops[1], dest_mem);
   create_fixed_operand (&ops[2], src_mem);
   if (!mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(mtcsOptabs,
           mtcsMachine->tmrtx->code_for_movstr/*!targetm.code_for_movstr*/, 3, ops))
      return NULL_RTX;

   if (retmode != RETURN_BEGIN && target != const0_rtx){
      target = ops[0].value;
      /* movstr is supposed to set end to the address of the NUL
      terminator.  If the caller requested a mempcpy-like return value,
      adjust it.  */
      if (retmode == RETURN_END){
         rtx tem = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,GET_MODE (target),
         gen_lowpart (GET_MODE (target), target), 1);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target,
               mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,tem, NULL_RTX));
      }
   }
   return target;
}


/* Helper function to do the actual work for expand_builtin_strcpy.  The
   arguments to the builtin_strcpy call DEST and SRC are broken out
   so that this can also be called without constructing an actual CALL_EXPR.
   The other arguments and return value are the same as for
   expand_builtin_strcpy.  */
//原型 expand_builtin_strcpy_args builtins.cc
static rtx expand_builtin_strcpy_args (MtcsBuiltins *self,tree, tree dest, tree src, rtx target)
{
  return expand_movstr(self,dest, src, target, /*retmode=*/ RETURN_BEGIN);
}

/* Expand expression EXP, which is a call to the strcpy builtin.  Return
   NULL_RTX if we failed the caller should emit a normal call, otherwise
   try to get the result in TARGET, if convenient (and in mode MODE if that's
   convenient).  */
//原型 expand_builtin_strcpy builtins.cc
static rtx expand_builtin_strcpy (MtcsBuiltins *self,tree exp, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  if (!validate_arglist (exp, POINTER_TYPE, POINTER_TYPE, VOID_TYPE))
    return NULL_RTX;

  tree dest = CALL_EXPR_ARG (exp, 0);
  tree src = CALL_EXPR_ARG (exp, 1);

  return expand_builtin_strcpy_args(self,exp, dest, src, target);
}


/* Expand expression EXP, which is a call to the strncpy builtin.  Return
   NULL_RTX if we failed the caller should emit a normal call.  */
//原型 expand_builtin_strncpy builtins.cc
static rtx expand_builtin_strncpy (MtcsBuiltins *self,tree exp, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);

   location_t loc = EXPR_LOCATION (exp);

   if (!validate_arglist (exp,POINTER_TYPE, POINTER_TYPE, INTEGER_TYPE, VOID_TYPE))
      return NULL_RTX;
   tree dest = CALL_EXPR_ARG (exp, 0);
   tree src = CALL_EXPR_ARG (exp, 1);
   /* The number of bytes to write (not the maximum).  */
   tree len = CALL_EXPR_ARG (exp, 2);
   /* The length of the source sequence.  */
   tree slen = c_strlen (src, 1);
   /* We must be passed a constant len and src parameter.  */
   if (!tree_fits_uhwi_p (len) || !slen || !tree_fits_uhwi_p (slen))
      return NULL_RTX;

   slen = mtcs_const_size_binop_loc/*!size_binop_loc*/(mtcsConst,loc, PLUS_EXPR, slen, ssize_int (1));

   /* We're required to pad with trailing zeros if the requested
   len is greater than strlen(s2)+1.  In that case try to
   use store_by_pieces, if it fails, punt.  */
   if (tree_int_cst_lt (slen, len)){
      unsigned int dest_align = get_pointer_alignment (dest);
      const char *p = c_getstr (src);
      rtx dest_mem;

      if (!p || dest_align == 0 || !tree_fits_uhwi_p (len)
      || !mtcs_expr_can_store_by_pieces/*!can_store_by_pieces*/(mtcsExpr,tree_to_uhwi (len),
      builtin_strncpy_read_str,CONST_CAST (char *, p), dest_align, false))
         return NULL_RTX;

      dest_mem = mtcs_builtins_get_memory_rtx/*!get_memory_rtx*/(self,dest, len);
      mtcs_expr_store_by_pieces/*!store_by_pieces*/(mtcsExpr,dest_mem, tree_to_uhwi (len),
            builtin_strncpy_read_str, CONST_CAST (char *, p), dest_align, false,RETURN_BEGIN);
      dest_mem = mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,XEXP (dest_mem, 0), target);
      dest_mem = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,ptr_mode, dest_mem);
      return dest_mem;
   }
   return NULL_RTX;
}


/* Expand a call EXP to the stpcpy builtin.
   Return NULL_RTX if we failed the caller should emit a normal call,
   otherwise try to get the result in TARGET, if convenient (and in
   mode MODE if that's convenient).  */
//原型 expand_builtin_stpcpy_1 builtins.cc
static rtx expand_builtin_stpcpy_1 (MtcsBuiltins *self,tree exp, rtx target, machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree dst, src;
   location_t loc = EXPR_LOCATION (exp);

   if (!validate_arglist (exp, POINTER_TYPE, POINTER_TYPE, VOID_TYPE))
      return NULL_RTX;

   dst = CALL_EXPR_ARG (exp, 0);
   src = CALL_EXPR_ARG (exp, 1);

   /* If return value is ignored, transform stpcpy into strcpy.  */
   if (target == const0_rtx && builtin_decl_implicit (BUILT_IN_STRCPY)){
      tree fn = builtin_decl_implicit (BUILT_IN_STRCPY);
      tree result = build_call_nofold_loc(self,loc, fn, 2, dst, src);
      return mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,result, target, mode, EXPAND_NORMAL);
   }else{
      tree len, lenp1;
      rtx ret;
      /* Ensure we get an actual string whose length can be evaluated at
      compile-time, not an expression containing a string.  This is
      because the latter will potentially produce pessimized code
      when used to produce the return value.  */
      c_strlen_data lendata = { };
      if (!c_getstr (src) || !(len = c_strlen (src, 0, &lendata, 1)))
         return expand_movstr(self,dst, src, target,/*retmode=*/ RETURN_END_MINUS_ONE);

      lenp1 =mtcs_const_size_binop_loc/*!size_binop_loc*/(mtcsConst,loc, PLUS_EXPR, len, ssize_int (1));
      ret = expand_builtin_mempcpy_args(self,dst, src, lenp1,target, exp,/*retmode=*/ RETURN_END_MINUS_ONE);

      if (ret)
         return ret;

      if (TREE_CODE (len) == INTEGER_CST){
         rtx len_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,len);
         if (CONST_INT_P (len_rtx)){
            ret = expand_builtin_strcpy_args(self,exp, dst, src, target);
            if (ret) {
               if (! target){
                  if (mode != VOIDmode)
                     target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
                  else
                     target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (ret));
               }
               if (GET_MODE (target) != GET_MODE (ret))
                  ret = gen_lowpart (GET_MODE (target), ret);

               ret = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,GET_MODE (ret), ret, INTVAL (len_rtx));
               ret = mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target,
                     mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,ret, NULL_RTX));
               gcc_assert (ret);
               return target;
            }
         }
      }
      return expand_movstr(self,dst, src, target,/*retmode=*/ RETURN_END_MINUS_ONE);
   }
}

/* Expand a call EXP to the stpcpy builtin and diagnose uses of nonstring
   arguments while being careful to avoid duplicate warnings (which could
   be issued if the expander were to expand the call, resulting in it
   being emitted in expand_call().  */
//原型 expand_builtin_stpcpy builtins.cc
static rtx expand_builtin_stpcpy (MtcsBuiltins *self,tree exp, rtx target, machine_mode mode)
{
  if (rtx ret = expand_builtin_stpcpy_1(self,exp, target, mode)){
      /* The call has been successfully expanded.  Check for nonstring
    arguments and issue warnings as appropriate.  */
      maybe_warn_nonstring_arg (get_callee_fndecl (exp), exp);
      return ret;
  }

  return NULL_RTX;
}


/* LEN specify length of the block of memcpy/memset operation.
   Figure out its range and put it into MIN_SIZE/MAX_SIZE.
   In some cases we can make very likely guess on max size, then we
   set it into PROBABLE_MAX_SIZE.  */
//原型 determine_block_size builtins.cc
static void determine_block_size (MtcsBuiltins *self,tree len, rtx len_rtx,
            unsigned HOST_WIDE_INT *min_size,
            unsigned HOST_WIDE_INT *max_size,
            unsigned HOST_WIDE_INT *probable_max_size)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   if (CONST_INT_P (len_rtx)){
      *min_size = *max_size = *probable_max_size = UINTVAL (len_rtx);
      return;
   }else{
      wide_int min, max;
      enum value_range_kind range_type = VR_UNDEFINED;

      /* Determine bounds from the type.  */
      if (tree_fits_uhwi_p (TYPE_MIN_VALUE (TREE_TYPE (len))))
         *min_size = tree_to_uhwi (TYPE_MIN_VALUE (TREE_TYPE (len)));
      else
         *min_size = 0;
      if (tree_fits_uhwi_p (TYPE_MAX_VALUE (TREE_TYPE (len))))
         *probable_max_size = *max_size  = tree_to_uhwi (TYPE_MAX_VALUE (TREE_TYPE (len)));
      else
         *probable_max_size = *max_size = GET_MODE_MASK (GET_MODE (len_rtx));

      if (TREE_CODE (len) == SSA_NAME){
         value_range r;
         tree tmin, tmax;
         get_global_range_query ()->range_of_expr (r, len);
         range_type = get_legacy_range (r, tmin, tmax);
         if (range_type != VR_UNDEFINED){
            min = wi::to_wide (tmin);
            max = wi::to_wide (tmax);
         }
      }
      if (range_type == VR_RANGE){
         if (wi::fits_uhwi_p (min) && *min_size < min.to_uhwi ())
            *min_size = min.to_uhwi ();
         if (wi::fits_uhwi_p (max) && *max_size > max.to_uhwi ())
            *probable_max_size = *max_size = max.to_uhwi ();
      }else if (range_type == VR_ANTI_RANGE){
         /* Code like

         int n;
         if (n < 100)
         memcpy (a, b, n)

         Produce anti range allowing negative values of N.  We still
         can use the information and make a guess that N is not negative.
         */
         if (!wi::leu_p (max, 1 << 30) && wi::fits_uhwi_p (min))
            *probable_max_size = min.to_uhwi () - 1;
      }
   }
   gcc_checking_assert (*max_size <= (unsigned HOST_WIDE_INT)
       mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,GET_MODE (len_rtx)));
}

/* Callback routine for store_by_pieces.  Read GET_MODE_BITSIZE (MODE)
   bytes from bytes at DATA + OFFSET and return it reinterpreted as
   a target constant.  */
//原型 builtin_memcpy_read_str builtins.cc 回调函数
static rtx builtin_memcpy_read_str (void *userData, void *, HOST_WIDE_INT offset,
          fixed_size_mode mode)
{
  BuiltinReadStrData *builtinReadStrData=(BuiltinReadStrData *)userData;
  /* The REPresentation pointed to by DATA need not be a nul-terminated
     string but the caller guarantees it's large enough for MODE.  */
  const char *rep = (const char *) builtinReadStrData->data;

  return c_readstr (rep + offset, mode, /*nul_terminated=*/false);
}

/* Helper function to do the actual work for expand of memory copy family
   functions (memcpy, mempcpy, stpcpy).  Expansing should assign LEN bytes
   of memory from SRC to DEST and assign to TARGET if convenient.  Return
   value is based on RETMODE argument.  */
//原型 expand_builtin_mempcpy_args builtins.cc
static rtx expand_builtin_memory_copy_args (MtcsBuiltins *self,tree dest, tree src, tree len,
             rtx target, tree exp, memop_ret retmode, bool might_overlap)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

   unsigned int src_align = get_pointer_alignment (src);
   unsigned int dest_align = get_pointer_alignment (dest);
   rtx dest_mem, src_mem, dest_addr, len_rtx;
   HOST_WIDE_INT expected_size = -1;
   unsigned int expected_align = 0;
   unsigned HOST_WIDE_INT min_size;
   unsigned HOST_WIDE_INT max_size;
   unsigned HOST_WIDE_INT probable_max_size;

   bool is_move_done;
   /* If DEST is not a pointer type, call the normal function.  */
   if (dest_align == 0)
      return NULL_RTX;
   /* If either SRC is not a pointer type, don't do this
   operation in-line.  */
   if (src_align == 0)
      return NULL_RTX;
   if (currently_expanding_gimple_stmt)
      stringop_block_profile (currently_expanding_gimple_stmt,&expected_align, &expected_size);

   if (expected_align < dest_align)
      expected_align = dest_align;
   dest_mem = mtcs_builtins_get_memory_rtx/*!get_memory_rtx*/(self,dest, len);
   mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,dest_mem, dest_align);
   len_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,len);
   determine_block_size(self,len, len_rtx, &min_size, &max_size, &probable_max_size);

   /* Try to get the byte representation of the constant SRC points to,
   with its byte size in NBYTES.  */
   unsigned HOST_WIDE_INT nbytes;
   const char *rep = getbyterep (src, &nbytes);

   /* If the function's constant bound LEN_RTX is less than or equal
   to the byte size of the representation of the constant argument,
   and if block move would be done by pieces, we can avoid loading
   the bytes from memory and only store the computed constant.
   This works in the overlap (memmove) case as well because
   store_by_pieces just generates a series of stores of constants
   from the representation returned by getbyterep().  */
   BuiltinReadStrData userData={self,CONST_CAST (char *, rep)};

   if (rep && CONST_INT_P (len_rtx) && (unsigned HOST_WIDE_INT) INTVAL (len_rtx) <= nbytes
   && mtcs_expr_can_store_by_pieces/*!can_store_by_pieces*/(mtcsExpr,INTVAL (len_rtx),
   builtin_memcpy_read_str,  &userData/*!CONST_CAST (char *, rep)*/,dest_align, false)){
      BuiltinReadStrData userData_1={self,CONST_CAST (char *, rep)};

      dest_mem = mtcs_expr_store_by_pieces/*!store_by_pieces*/(mtcsExpr,dest_mem, INTVAL (len_rtx),
      builtin_memcpy_read_str, &userData_1/*!CONST_CAST (char *, rep)*/,dest_align, false, retmode);
      dest_mem = mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,XEXP (dest_mem, 0), target);
      dest_mem = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,ptr_mode, dest_mem);
      return dest_mem;
   }

   src_mem = mtcs_builtins_get_memory_rtx/*!get_memory_rtx*/(self,src, len);
   mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,src_mem, src_align);

   /* Copy word part most expediently.  */
   enum block_op_methods method = BLOCK_OP_NORMAL;
   if (CALL_EXPR_TAILCALL (exp)  && (retmode == RETURN_BEGIN || target == const0_rtx))
      method = BLOCK_OP_TAILCALL;

   bool use_mempcpy_call = (mtcsTarget/*!targetm.libc_has_fast_function*/->libc_has_fast_function(mtcsTarget,BUILT_IN_MEMPCPY)
   && retmode == RETURN_END  && !might_overlap && target != const0_rtx);
   if (use_mempcpy_call)
      method = BLOCK_OP_NO_LIBCALL_RET;

   dest_addr = mtcs_expr_emit_block_move_hints/*!emit_block_move_hints*/(mtcsExpr,
   dest_mem, src_mem, len_rtx, method, expected_align, expected_size,
   min_size, max_size, probable_max_size,use_mempcpy_call, &is_move_done,  might_overlap, tree_ctz (len));
   /* Bail out when a mempcpy call would be expanded as libcall and when
   we have a target that provides a fast implementation
   of mempcpy routine.  */
   if (!is_move_done)
      return NULL_RTX;
   if (dest_addr == pc_rtx)
      return NULL_RTX;

   if (dest_addr == 0) {
      dest_addr = mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,XEXP (dest_mem, 0), target);
      dest_addr = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,ptr_mode, dest_addr);
   }

   if (retmode != RETURN_BEGIN && target != const0_rtx){
      dest_addr = gen_rtx_PLUS (ptr_mode, dest_addr, len_rtx);
      /* stpcpy pointer to last byte.  */
      if (retmode == RETURN_END_MINUS_ONE)
         dest_addr = gen_rtx_MINUS (ptr_mode, dest_addr, const1_rtx);
   }

   return dest_addr;
}



/* Expand a call EXP to the memcpy builtin.
   Return NULL_RTX if we failed, the caller should emit a normal call,
   otherwise try to get the result in TARGET, if convenient (and in
   mode MODE if that's convenient).  */
//原型 expand_builtin_memcpy builtins.cc
static rtx expand_builtin_memcpy (MtcsBuiltins *self,tree exp, rtx target)
{
   if (!validate_arglist (exp, POINTER_TYPE, POINTER_TYPE, INTEGER_TYPE, VOID_TYPE))
      return NULL_RTX;
   tree dest = CALL_EXPR_ARG (exp, 0);
   tree src = CALL_EXPR_ARG (exp, 1);
   tree len = CALL_EXPR_ARG (exp, 2);
   return expand_builtin_memory_copy_args(self,dest, src, len, target, exp,
        /*retmode=*/ RETURN_BEGIN, false);
}

/* Check a call EXP to the memmove built-in for validity.
   Return NULL_RTX on both success and failure.  */
//原型 expand_builtin_memmove builtins.cc
static rtx expand_builtin_memmove (MtcsBuiltins *self,tree exp, rtx target)
{
   if (!validate_arglist (exp,POINTER_TYPE, POINTER_TYPE, INTEGER_TYPE, VOID_TYPE))
      return NULL_RTX;

   tree dest = CALL_EXPR_ARG (exp, 0);
   tree src = CALL_EXPR_ARG (exp, 1);
   tree len = CALL_EXPR_ARG (exp, 2);
   return expand_builtin_memory_copy_args(self,dest, src, len, target, exp,
        /*retmode=*/ RETURN_BEGIN, true);
}

//原型 expand_builtin_mempcpy_args builtins.cc
static rtx expand_builtin_mempcpy_args (MtcsBuiltins *self,tree dest, tree src, tree len,
              rtx target, tree orig_exp, memop_ret retmode)
{
   return expand_builtin_memory_copy_args(self,dest, src, len, target, orig_exp,
                 retmode, false);
}


/* Expand a call EXP to the mempcpy builtin.
   Return NULL_RTX if we failed; the caller should emit a normal call,
   otherwise try to get the result in TARGET, if convenient (and in
   mode MODE if that's convenient).  */
//原型 expand_builtin_mempcpy builtins.cc
static rtx expand_builtin_mempcpy (MtcsBuiltins *self,tree exp, rtx target)
{
   if (!validate_arglist (exp,POINTER_TYPE, POINTER_TYPE, INTEGER_TYPE, VOID_TYPE))
      return NULL_RTX;

   tree dest = CALL_EXPR_ARG (exp, 0);
   tree src = CALL_EXPR_ARG (exp, 1);
   tree len = CALL_EXPR_ARG (exp, 2);

   /* Policy does not generally allow using compute_objsize (which
   is used internally by check_memop_size) to change code generation
   or drive optimization decisions.

   In this instance it is safe because the code we generate has
   the same semantics regardless of the return value of
   check_memop_sizes.   Exactly the same amount of data is copied
   and the return value is exactly the same in both cases.

   Furthermore, check_memop_size always uses mode 0 for the call to
   compute_objsize, so the imprecise nature of compute_objsize is
   avoided.  */

   /* Avoid expanding mempcpy into memcpy when the call is determined
   to overflow the buffer.  This also prevents the same overflow
   from being diagnosed again when expanding memcpy.  */
   return expand_builtin_mempcpy_args(self,dest, src, len,target, exp, /*retmode=*/ RETURN_END);
}

/* Return the RTL of a register in MODE generated from PREV in the
   previous iteration.  */
//原型 gen_memset_value_from_prev builtins.cc
static rtx gen_memset_value_from_prev (MtcsBuiltins *self,by_pieces_prev *prev, fixed_size_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

   rtx target = nullptr;
   if (prev != nullptr && prev->data != nullptr){
      /* Use the previous data in the same mode.  */
      if (prev->mode == mode)
         return prev->data;

      fixed_size_mode prev_mode = prev->mode;

      /* Don't use the previous data to write QImode if it is in a
      vector mode.  */
      if (mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,prev_mode) && mode == mtcsMode->modes.M_QImode)
         return target;

      rtx prev_rtx = prev->data;

      if (REG_P (prev_rtx)   && mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,prev_rtx)
        && lowpart_subreg_regno (REGNO (prev_rtx), prev_mode, mode) < 0){
         /* This case occurs when PREV_MODE is a vector and when
         MODE is too small to store using vector operations.
         After register allocation, the code will need to move the
         lowpart of the vector register into a non-vector register.

         Also, the target has chosen to use a hard register
         instead of going with the default choice of using a
         pseudo register.  We should respect that choice and try to
         avoid creating a pseudo register with the same mode as the
         current hard register.

         In principle, we could just use a lowpart MODE subreg of
         the vector register.  However, the vector register mode might
         be too wide for non-vector registers, and we already know
         that the non-vector mode is too small for vector registers.
         It's therefore likely that we'd need to spill to memory in
         the vector mode and reload the non-vector value from there.

         Try to avoid that by reducing the vector register to the
         smallest size that it can hold.  This should increase the
         chances that non-vector registers can hold both the inner
         and outer modes of the subreg that we generate later.  */
         machine_mode m;
         fixed_size_mode candidate;
         MTCS_FOR_EACH_MODE_IN_CLASS/*!FOR_EACH_MODE_IN_CLASS*/(mtcsMode,
                              m,(enum mode_class)mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode))
            if (mtcs_mode_is_a<fixed_size_mode> (mtcsMode,m, &candidate)){
               if (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,candidate)>=
                     mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,prev_mode))
                  break;
               if (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,candidate) >=
                     mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode)
                     && lowpart_subreg_regno (REGNO (prev_rtx),prev_mode, candidate) >= 0){
                  target = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg */(mtcsSimplifyRtx,
                        candidate, prev_rtx,prev_mode);
                  prev_rtx = target;
                  prev_mode = candidate;
                  break;
               }
            }
         if (target == nullptr)
            prev_rtx =  mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,prev_rtx);
      }

      target = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg */(mtcsSimplifyRtx,mode, prev_rtx, prev_mode);
   }
   return target;
}

typedef struct _BuiltinsData
{
   MtcsBuiltins *mtcsBuiltins;
   rtx data;
}BuiltinsData;

/* Callback routine for store_by_pieces.  Return the RTL of a register
   containing GET_MODE_SIZE(MODE) consecutive copies of the unsigned
   char value given in the RTL register data.  For example, if mode is
   4 bytes wide, return the RTL for 0x01010101*data.  If PREV isn't
   nullptr, it has the RTL info from the previous iteration.  */
//原型 builtin_memset_gen_str builtins.cc 回调函数
static rtx builtin_memset_gen_str_cb (void *userData, void *prev,
         HOST_WIDE_INT offset ATTRIBUTE_UNUSED,  fixed_size_mode mode)
{
   BuiltinsData *builtinsData=(BuiltinsData *)userData;
   MtcsBuiltins *self=builtinsData->mtcsBuiltins;
   rtx data=builtinsData->data;

   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

   rtx target, coeff;
   size_t size;
   char *p;

   size = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);
   if (size == 1)
      return (rtx) data;

   target = gen_memset_value_from_prev(self,(by_pieces_prev *) prev, mode);
   if (target != nullptr)
      return target;

   if (mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,mode)){
      gcc_assert (mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode) == mtcsMode->modes.M_QImode);

      /* vec_duplicate_optab is a precondition to pick a vector mode for
      the memset expander.  */
      insn_code icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,vec_duplicate_optab, mode);

      target =  mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
      class expand_operand ops[2];
      create_output_operand (&ops[0], target, mode);
      create_input_operand (&ops[1], (rtx) data, mtcsMode->modes.M_QImode);
      mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,icode, 2, ops);
      if (!rtx_equal_p (target, ops[0].value))
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, ops[0].value);

      return target;
   }

   p = XALLOCAVEC (char, size);
   memset (p, 1, size);
   coeff = c_readstr (p, mode);

   target = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,mode, (rtx) data, 1);
   target = mtcs_expmed_expand_mult/*!expand_mult*/(mtcsExpmed,mode, target, coeff, NULL_RTX, 1);
   return mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,mode, target);
}


/* Cast a target constant CST to target CHAR and if that value fits into
   host char type, return zero and put that value into variable pointed to by
   P.  */
//原型 target_char_cast builtins.cc
static int target_char_cast (tree cst, char *p)
{
   unsigned HOST_WIDE_INT val, hostval;
   if (TREE_CODE (cst) != INTEGER_CST || CHAR_TYPE_SIZE > HOST_BITS_PER_WIDE_INT)
      return 1;
   /* Do not care if it fits or not right here.  */
   val = TREE_INT_CST_LOW (cst);
   if (CHAR_TYPE_SIZE < HOST_BITS_PER_WIDE_INT)
      val &= (HOST_WIDE_INT_1U << CHAR_TYPE_SIZE) - 1;
   hostval = val;
   if (HOST_BITS_PER_CHAR < HOST_BITS_PER_WIDE_INT)
      hostval &= (HOST_WIDE_INT_1U << HOST_BITS_PER_CHAR) - 1;
   if (val != hostval)
      return 1;
   *p = hostval;
   return 0;
}


/* Helper function to do the actual work for expand_builtin_memset.  The
   arguments to the builtin_memset call DEST, VAL, and LEN are broken out
   so that this can also be called without constructing an actual CALL_EXPR.
   The other arguments and return value are the same as for
   expand_builtin_memset.  */
//原型 expand_builtin_memset_args builtins.cc
static rtx expand_builtin_memset_args (MtcsBuiltins *self,tree dest, tree val, tree len,
             rtx target, machine_mode mode, tree orig_exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   tree fndecl, fn;
   enum built_in_function fcode;
   machine_mode val_mode;
   char c;
   unsigned int dest_align;
   rtx dest_mem, dest_addr, len_rtx;
   HOST_WIDE_INT expected_size = -1;
   unsigned int expected_align = 0;
   unsigned HOST_WIDE_INT min_size;
   unsigned HOST_WIDE_INT max_size;
   unsigned HOST_WIDE_INT probable_max_size;

   dest_align = get_pointer_alignment (dest);
   /* If DEST is not a pointer type, don't do this operation in-line.  */
   if (dest_align == 0)
      return NULL_RTX;

   if (currently_expanding_gimple_stmt)
      stringop_block_profile (currently_expanding_gimple_stmt, &expected_align, &expected_size);

   if (expected_align < dest_align)
      expected_align = dest_align;
   /* If the LEN parameter is zero, return DEST.  */
   if (integer_zerop (len)){
      /* Evaluate and ignore VAL in case it has side-effects.  */
      mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,val, const0_rtx, VOIDmode, EXPAND_NORMAL);
      return mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,dest, target, mode, EXPAND_NORMAL);
   }

   /* Stabilize the arguments in case we fail.  */
   dest = builtin_save_expr (dest);
   val = builtin_save_expr (val);
   len = builtin_save_expr (len);

   len_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,len);
   determine_block_size(self,len, len_rtx, &min_size, &max_size, &probable_max_size);
   dest_mem = mtcs_builtins_get_memory_rtx/*!get_memory_rtx*/(self,dest, len);
   val_mode=mtcs_mode_host2device_by_tree(mtcsMode,unsigned_char_type_node,val_mode);

   if (TREE_CODE (val) != INTEGER_CST  || target_char_cast (val, &c)){
      rtx val_rtx;

      val_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,val);
      val_rtx = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,val_mode, val_rtx, 0);

      /* Assume that we can memset by pieces if we can store
      * the coefficients by pieces (in the required modes).
      * We can't pass builtin_memset_gen_str as that emits RTL.  */
      c = 1;
      BuiltinReadStrData userData={self,&c};
      if (tree_fits_uhwi_p (len)
        && mtcs_expr_can_store_by_pieces/*!can_store_by_pieces*/(mtcsExpr,tree_to_uhwi (len),
               mtcs_builtins_builtin_memset_read_str/*!builtin_memset_read_str*/, &userData/*!&c*/,
               dest_align,true)){
         BuiltinsData userData={self,val_rtx};
         val_rtx = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,val_mode, val_rtx);
         mtcs_expr_store_by_pieces/*!store_by_pieces*/(mtcsExpr,dest_mem, tree_to_uhwi (len),
              builtin_memset_gen_str_cb/*!builtin_memset_gen_str*/, &userData/*!val_rtx*/, dest_align,
              true, RETURN_BEGIN);
      }else if (!mtcs_expr_set_storage_via_setmem/*!set_storage_via_setmem*/(mtcsExpr,dest_mem, len_rtx, val_rtx,
         dest_align, expected_align,expected_size, min_size, max_size,probable_max_size)
         && !mtcs_expr_try_store_by_multiple_pieces/*!try_store_by_multiple_pieces*/(mtcsExpr,dest_mem, len_rtx,
         tree_ctz (len), min_size, max_size, val_rtx, 0,dest_align))
         goto do_libcall;

      dest_mem = mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,XEXP (dest_mem, 0), NULL_RTX);
      dest_mem = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,ptr_mode, dest_mem);
      return dest_mem;
   }

   if (c) {
      BuiltinReadStrData userData={self,&c};

      if (tree_fits_uhwi_p (len) && mtcs_expr_can_store_by_pieces/*!can_store_by_pieces*/(mtcsExpr,tree_to_uhwi (len),
            mtcs_builtins_builtin_memset_read_str/*!builtin_memset_read_str*/, &userData/*!&c*/, dest_align,true))
         mtcs_expr_store_by_pieces/*!store_by_pieces*/(mtcsExpr,dest_mem, tree_to_uhwi (len),
         mtcs_builtins_builtin_memset_read_str/*!builtin_memset_read_str*/, &userData/*!&c*/,
         dest_align, true,RETURN_BEGIN);
      else if (!mtcs_expr_set_storage_via_setmem/*!set_storage_via_setmem*/(mtcsExpr,dest_mem, len_rtx,
          mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,c, val_mode), dest_align, expected_align,
          expected_size, min_size, max_size, probable_max_size)
          && !mtcs_expr_try_store_by_multiple_pieces/*!try_store_by_multiple_pieces*/(mtcsExpr,dest_mem, len_rtx,
                 tree_ctz (len), min_size, max_size, NULL_RTX, c,dest_align))
         goto do_libcall;

      dest_mem = mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,XEXP (dest_mem, 0), NULL_RTX);
      dest_mem = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,ptr_mode, dest_mem);
      return dest_mem;
   }

   mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,dest_mem, dest_align);
   dest_addr = mtcs_expr_clear_storage_hints/*!clear_storage_hints*/(mtcsExpr,dest_mem, len_rtx,
   CALL_EXPR_TAILCALL (orig_exp)? BLOCK_OP_TAILCALL : BLOCK_OP_NORMAL,
   expected_align, expected_size,min_size, max_size, probable_max_size, tree_ctz (len));

   if (dest_addr == 0){
      dest_addr = mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,XEXP (dest_mem, 0), NULL_RTX);
      dest_addr = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,ptr_mode, dest_addr);
   }
   return dest_addr;

do_libcall:
   fndecl = get_callee_fndecl (orig_exp);
   fcode = DECL_FUNCTION_CODE (fndecl);
   if (fcode == BUILT_IN_MEMSET)
      fn = build_call_nofold_loc(self,EXPR_LOCATION (orig_exp), fndecl, 3,dest, val, len);
   else if (fcode == BUILT_IN_BZERO)
      fn = build_call_nofold_loc(self,EXPR_LOCATION (orig_exp), fndecl, 2,dest, len);
   else
      gcc_unreachable ();
   gcc_assert (TREE_CODE (fn) == CALL_EXPR);
   CALL_EXPR_TAILCALL (fn) = CALL_EXPR_TAILCALL (orig_exp);
   return mtcs_calls_expand_call/*!expand_call*/(mtcsCalls,fn, target, target == const0_rtx);
}

/* Expand expression EXP, which is a call to the bzero builtin.  Return
   NULL_RTX if we failed the caller should emit a normal call.  */
//原型 expand_builtin_bzero builtins.cc
static rtx expand_builtin_bzero (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);

   if (!validate_arglist (exp, POINTER_TYPE, INTEGER_TYPE, VOID_TYPE))
      return NULL_RTX;
   tree dest = CALL_EXPR_ARG (exp, 0);
   tree size = CALL_EXPR_ARG (exp, 1);
   /* New argument list transforming bzero(ptr x, int y) to
   memset(ptr x, int 0, size_t y).   This is done this way
   so that if it isn't expanded inline, we fallback to
   calling bzero instead of memset.  */
   location_t loc = EXPR_LOCATION (exp);
   return expand_builtin_memset_args(self,dest, integer_zero_node,
        mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, size_type_node, size), const0_rtx, VOIDmode, exp);
}


/* Expand a string compare operation using a sequence of char comparison
   to get rid of the calling overhead, with result going to TARGET if
   that's convenient.

   VAR_STR is the variable string source;
   CONST_STR is the constant string source;
   LENGTH is the number of chars to compare;
   CONST_STR_N indicates which source string is the constant string;
   IS_MEMCMP indicates whether it's a memcmp or strcmp.

   to: (assume const_str_n is 2, i.e., arg2 is a constant string)

   target = (int) (unsigned char) var_str[0]
       - (int) (unsigned char) const_str[0];
   if (target != 0)
     goto ne_label;
     ...
   target = (int) (unsigned char) var_str[length - 2]
       - (int) (unsigned char) const_str[length - 2];
   if (target != 0)
     goto ne_label;
   target = (int) (unsigned char) var_str[length - 1]
       - (int) (unsigned char) const_str[length - 1];
   ne_label:
  */
//原型 inline_string_cmp builtins.cc

static rtx inline_string_cmp (MtcsBuiltins *self,rtx target, tree var_str, const char *const_str,
         unsigned HOST_WIDE_INT length,int const_str_n, machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   HOST_WIDE_INT offset = 0;
   rtx var_rtx_array = get_memory_rtx (var_str, mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,unsigned_type_node,length));
   rtx var_rtx = NULL_RTX;
   rtx const_rtx = NULL_RTX;
   rtx result = target ? target : mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
   rtx_code_label *ne_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
   tree unit_type_node = unsigned_char_type_node;
   machine_mode unitTypeNodeMode = TYPE_MODE (unit_type_node);
   unitTypeNodeMode=mtcs_mode_host2device_by_tree(mtcsMode,unit_type_node,unitTypeNodeMode);
   scalar_int_mode unit_mode = mtcs_mode_as_a <scalar_int_mode>(mtcsMode,unitTypeNodeMode/*!TYPE_MODE (unit_type_node)*/);

   mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);

   for (unsigned HOST_WIDE_INT i = 0; i < length; i++){
      var_rtx = adjust_address (var_rtx_array, unitTypeNodeMode/*!TYPE_MODE (unit_type_node)*/, offset);
      const_rtx = c_readstr (const_str + offset, unit_mode);
      rtx op0 = (const_str_n == 1) ? const_rtx : var_rtx;
      rtx op1 = (const_str_n == 1) ? var_rtx : const_rtx;

      op0 = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,mode, unit_mode, op0, 1);
      op1 = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,mode, unit_mode, op1, 1);
      rtx diff = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,mode, MINUS, op0, op1,
      result, 1, OPTAB_WIDEN);

      /* Force the difference into result register.  We cannot reassign
      result here ("result = diff") or we may end up returning
      uninitialized result when expand_simple_binop allocates a new
      pseudo-register for returning.  */
      if (diff != result)
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,result, diff);

      if (i < length - 1)
         mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,
              result, CONST0_RTX (mode), NE, NULL_RTX, mode, true, ne_label);
      offset += mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,unit_mode);
   }

   mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,ne_label);
   rtx_insn *insns =  mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,insns);

   return result;
}


/* Inline expansion of a call to str(n)cmp and memcmp, with result going
   to TARGET if that's convenient.
   If the call is not been inlined, return NULL_RTX.  */
//原型 inline_expand_builtin_bytecmp builtins.cc
static rtx inline_expand_builtin_bytecmp (MtcsBuiltins *self,tree exp, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsPredict *mtcsPredict =mtcs_target_get_predict(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

   tree fndecl = get_callee_fndecl (exp);
   enum built_in_function fcode = DECL_FUNCTION_CODE (fndecl);
   bool is_ncmp = (fcode == BUILT_IN_STRNCMP || fcode == BUILT_IN_MEMCMP);

   /* Do NOT apply this inlining expansion when optimizing for size or
   optimization level below 2 or if unused *cmp hasn't been DCEd.  */
   if (mtcsOptionsItem->x_optimize < 2
         || mtcs_predict_optimize_insn_for_size_p/*!optimize_insn_for_size_p*/(mtcsPredict)
         || target == const0_rtx)
      return NULL_RTX;

   gcc_checking_assert (fcode == BUILT_IN_STRCMP || fcode == BUILT_IN_STRNCMP  || fcode == BUILT_IN_MEMCMP);

   /* On a target where the type of the call (int) has same or narrower presicion
   than unsigned char, give up the inlining expansion.  */
   if (TYPE_PRECISION (unsigned_char_type_node)>= TYPE_PRECISION (TREE_TYPE (exp)))
      return NULL_RTX;

   tree arg1 = CALL_EXPR_ARG (exp, 0);
   tree arg2 = CALL_EXPR_ARG (exp, 1);
   tree len3_tree = is_ncmp ? CALL_EXPR_ARG (exp, 2) : NULL_TREE;

   unsigned HOST_WIDE_INT len1 = 0;
   unsigned HOST_WIDE_INT len2 = 0;
   unsigned HOST_WIDE_INT len3 = 0;

   /* Get the object representation of the initializers of ARG1 and ARG2
   as strings, provided they refer to constant objects, with their byte
   sizes in LEN1 and LEN2, respectively.  */
   const char *bytes1 = getbyterep (arg1, &len1);
   const char *bytes2 = getbyterep (arg2, &len2);

   /* Fail if neither argument refers to an initialized constant.  */
   if (!bytes1 && !bytes2)
      return NULL_RTX;

   if (is_ncmp){
      /* Fail if the memcmp/strncmp bound is not a constant.  */
      if (!tree_fits_uhwi_p (len3_tree))
         return NULL_RTX;

      len3 = tree_to_uhwi (len3_tree);

      if (fcode == BUILT_IN_MEMCMP){
         /* Fail if the memcmp bound is greater than the size of either
         of the two constant objects.  */
         if ((bytes1 && len1 < len3) || (bytes2 && len2 < len3))
            return NULL_RTX;
      }
   }

   if (fcode != BUILT_IN_MEMCMP){
      /* For string functions (i.e., strcmp and strncmp) reduce LEN1
      and LEN2 to the length of the nul-terminated string stored
      in each.  */
      if (bytes1 != NULL)
         len1 = strnlen (bytes1, len1) + 1;
      if (bytes2 != NULL)
         len2 = strnlen (bytes2, len2) + 1;
   }

   /* See inline_string_cmp.  */
   int const_str_n;
   if (!len1)
      const_str_n = 2;
   else if (!len2)
      const_str_n = 1;
   else if (len2 > len1)
      const_str_n = 1;
   else
      const_str_n = 2;

   /* For strncmp only, compute the new bound as the smallest of
   the lengths of the two strings (plus 1) and the bound provided
   to the function.  */
   unsigned HOST_WIDE_INT bound = (const_str_n == 1) ? len1 : len2;
   if (is_ncmp && len3 < bound)
      bound = len3;

   /* If the bound of the comparison is larger than the threshold,
   do nothing.  */
   if (bound > (unsigned HOST_WIDE_INT) param_builtin_string_cmp_inline_length)
      return NULL_RTX;

   machine_mode mode = TYPE_MODE (TREE_TYPE (exp));
   mode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (exp),mode);

   /* Now, start inline expansion the call.  */
   return inline_string_cmp(self,target, (const_str_n == 1) ? arg2 : arg1,
                           (const_str_n == 1) ? bytes1 : bytes2, bound, const_str_n, mode);
}



/* Expand expression EXP, which is a call to the memcmp built-in function.
   Return NULL_RTX if we failed and the caller should emit a normal call,
   otherwise try to get the result in TARGET, if convenient.
   RESULT_EQ is true if we can relax the returned value to be either zero
   or nonzero, without caring about the sign.  */
//原型 expand_builtin_memcmp builtins.cc
static rtx expand_builtin_memcmp (MtcsBuiltins *self,tree exp, rtx target, bool result_eq)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);

   if (!validate_arglist (exp,POINTER_TYPE, POINTER_TYPE, INTEGER_TYPE, VOID_TYPE))
      return NULL_RTX;

   tree arg1 = CALL_EXPR_ARG (exp, 0);
   tree arg2 = CALL_EXPR_ARG (exp, 1);
   tree len = CALL_EXPR_ARG (exp, 2);
   /* Due to the performance benefit, always inline the calls first
   when result_eq is false.  */
   rtx result = NULL_RTX;
   enum built_in_function fcode = DECL_FUNCTION_CODE (get_callee_fndecl (exp));
   if (!result_eq && fcode != BUILT_IN_BCMP){
      result = inline_expand_builtin_bytecmp(self,exp, target);
      if (result)
         return result;
   }

   machine_mode mode = TYPE_MODE (TREE_TYPE (exp));
   mode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (exp),mode);
   location_t loc = EXPR_LOCATION (exp);
   unsigned int arg1_align = get_pointer_alignment (arg1) / BITS_PER_UNIT;
   unsigned int arg2_align = get_pointer_alignment (arg2) / BITS_PER_UNIT;
   /* If we don't have POINTER_TYPE, call the function.  */
   if (arg1_align == 0 || arg2_align == 0)
      return NULL_RTX;

   rtx arg1_rtx = mtcs_builtins_get_memory_rtx/*!get_memory_rtx*/(self,arg1, len);
   rtx arg2_rtx = mtcs_builtins_get_memory_rtx/*!get_memory_rtx*/(self,arg2, len);
   rtx len_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, sizetype, len));

   /* Set MEM_SIZE as appropriate.  */
   if (CONST_INT_P (len_rtx)){
      mtcs_rtl_set_mem_size/*!set_mem_size*/(mtcsRTL,arg1_rtx, INTVAL (len_rtx));
      mtcs_rtl_set_mem_size/*!set_mem_size*/(mtcsRTL,arg2_rtx, INTVAL (len_rtx));
   }

   by_pieces_constfn constfn = NULL;
   /* Try to get the byte representation of the constant ARG2 (or, only
   when the function's result is used for equality to zero, ARG1)
   points to, with its byte size in NBYTES.  */
   unsigned HOST_WIDE_INT nbytes;
   const char *rep = getbyterep (arg2, &nbytes);
   if (result_eq && rep == NULL){
      /* For equality to zero the arguments are interchangeable.  */
      rep = getbyterep (arg1, &nbytes);
      if (rep != NULL)
         std::swap (arg1_rtx, arg2_rtx);
   }
   /* If the function's constant bound LEN_RTX is less than or equal
   to the byte size of the representation of the constant argument,
   and if block move would be done by pieces, we can avoid loading
   the bytes from memory and only store the computed constant result.  */
   if (rep  && CONST_INT_P (len_rtx)  && (unsigned HOST_WIDE_INT) INTVAL (len_rtx) <= nbytes)
      constfn =builtin_memcpy_read_str;

   BuiltinReadStrData userData={self,CONST_CAST (char *, rep)};
   result = mtcs_expr_emit_block_cmp_hints/*!emit_block_cmp_hints*/(mtcsExpr,arg1_rtx, arg2_rtx, len_rtx,
                TREE_TYPE (len), target,result_eq, constfn, &userData/*!CONST_CAST (char *, rep)*/,tree_ctz (len));
   if (result) {
      /* Return the value in the proper mode for this function.  */
      if (GET_MODE (result) == mode)
         return result;
      if (target != 0){
         mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,target, result, 0);
         return target;
      }
      return mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,mode, result, 0);
   }
   return NULL_RTX;
}


/* Expand expression EXP, which is a call to the strncmp builtin. Return
   NULL_RTX if we failed the caller should emit a normal call, otherwise
   try to get the result in TARGET, if convenient.  */
//原型 expand_builtin_strncmp builtins.cc
static rtx expand_builtin_strncmp (MtcsBuiltins *self,tree exp, ATTRIBUTE_UNUSED rtx target,
         ATTRIBUTE_UNUSED machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);

   if (!validate_arglist (exp, POINTER_TYPE, POINTER_TYPE, INTEGER_TYPE, VOID_TYPE))
      return NULL_RTX;

   tree arg1 = CALL_EXPR_ARG (exp, 0);
   tree arg2 = CALL_EXPR_ARG (exp, 1);
   tree arg3 = CALL_EXPR_ARG (exp, 2);

   location_t loc = EXPR_LOCATION (exp);
   tree len1 = c_strlen (arg1, 1);
   tree len2 = c_strlen (arg2, 1);

   /* Due to the performance benefit, always inline the calls first.  */
   rtx result = NULL_RTX;
   result = inline_expand_builtin_bytecmp(self,exp, target);
   if (result)
      return result;

   /* If c_strlen can determine an expression for one of the string
   lengths, and it doesn't have side effects, then emit cmpstrnsi
   using length MIN(strlen(string)+1, arg3).  */
   insn_code cmpstrn_icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,
                        cmpstrn_optab, mtcsMode->modes.M_SImode);
   if (cmpstrn_icode == CODE_FOR_nothing)
      return NULL_RTX;

   tree len;
   unsigned int arg1_align = get_pointer_alignment (arg1) / BITS_PER_UNIT;
   unsigned int arg2_align = get_pointer_alignment (arg2) / BITS_PER_UNIT;

   if (len1)
      len1 = mtcs_const_size_binop_loc/*!size_binop_loc*/(mtcsConst,loc, PLUS_EXPR, ssize_int (1), len1);
   if (len2)
      len2 = mtcs_const_size_binop_loc/*!size_binop_loc*/(mtcsConst,loc, PLUS_EXPR, ssize_int (1), len2);

   tree len3 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, sizetype, arg3);
   /* If we don't have a constant length for the first, use the length
   of the second, if we know it.  If neither string is constant length,
   use the given length argument.  We don't require a constant for
   this case; some cost analysis could be done if both are available
   but neither is constant.  For now, assume they're equally cheap,
   unless one has side effects.  If both strings have constant lengths,
   use the smaller.  */
   if (!len1 && !len2)
      len = len3;
   else if (!len1)
      len = len2;
   else if (!len2)
      len = len1;
   else if (TREE_SIDE_EFFECTS (len1))
      len = len2;
   else if (TREE_SIDE_EFFECTS (len2))
      len = len1;
   else if (TREE_CODE (len1) != INTEGER_CST)
      len = len2;
   else if (TREE_CODE (len2) != INTEGER_CST)
      len = len1;
   else if (tree_int_cst_lt (len1, len2))
      len = len1;
   else
      len = len2;

   /* If we are not using the given length, we must incorporate it here.
   The actual new length parameter will be MIN(len,arg3) in this case.  */
   if (len != len3){
      len = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, sizetype, len);
      len = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, MIN_EXPR, TREE_TYPE (len), len, len3);
   }
   rtx arg1_rtx = mtcs_builtins_get_memory_rtx/*!get_memory_rtx*/(self,arg1, len);
   rtx arg2_rtx = mtcs_builtins_get_memory_rtx/*!get_memory_rtx*/(self,arg2, len);
   rtx arg3_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,len);
   result = mtcs_expr_expand_cmpstrn_or_cmpmem/*!expand_cmpstrn_or_cmpmem*/(mtcsExpr,cmpstrn_icode, target, arg1_rtx,
                     arg2_rtx, TREE_TYPE (len), arg3_rtx, MIN (arg1_align, arg2_align));

   tree fndecl = get_callee_fndecl (exp);
   if (result){
      /* Return the value in the proper mode for this function.  */
      mode = TYPE_MODE (TREE_TYPE (exp));
      mode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (exp),mode);
      if (GET_MODE (result) == mode)
         return result;
      if (target == 0)
         return mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,mode, result, 0);
      mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,target, result, 0);
      return target;
   }

   /* Expand the library call ourselves using a stabilized argument
   list to avoid re-evaluating the function's arguments twice.  */
   tree call = build_call_nofold_loc(self,loc, fndecl, 3, arg1, arg2, len);
   copy_warning (call, exp);
   gcc_assert (TREE_CODE (call) == CALL_EXPR);
   CALL_EXPR_TAILCALL (call) = CALL_EXPR_TAILCALL (exp);
   return mtcs_calls_expand_call/*!expand_call*/(mtcsCalls,call, target, target == const0_rtx);
}

/* Try to expand cmpstr operation ICODE with the given operands.
   Return the result rtx on success, otherwise return null.  */
//原型 expand_cmpstr builtins.cc
static rtx expand_cmpstr (MtcsBuiltins *self,insn_code icode, rtx target, rtx arg1_rtx, rtx arg2_rtx,
          HOST_WIDE_INT align)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   machine_mode insn_mode =mtcsOutput->insn_data[icode].operand[0].mode;

   if (target && (!REG_P (target) ||mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,target)))
      target = NULL_RTX;

   class expand_operand ops[4];
   create_output_operand (&ops[0], target, insn_mode);
   create_fixed_operand (&ops[1], arg1_rtx);
   create_fixed_operand (&ops[2], arg2_rtx);
   mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[3], align);
   if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(mtcsOptabs,icode, 4, ops))
      return ops[0].value;
   return NULL_RTX;
}

/* Expand expression EXP, which is a call to the strcmp builtin.  Return NULL_RTX
   if we failed the caller should emit a normal call, otherwise try to get
   the result in TARGET, if convenient.  */
//原型 expand_builtin_strcmp builtins.cc
static rtx expand_builtin_strcmp (MtcsBuiltins *self,tree exp, ATTRIBUTE_UNUSED rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

   if (!validate_arglist (exp, POINTER_TYPE, POINTER_TYPE, VOID_TYPE))
      return NULL_RTX;

   tree arg1 = CALL_EXPR_ARG (exp, 0);
   tree arg2 = CALL_EXPR_ARG (exp, 1);

   /* Due to the performance benefit, always inline the calls first.  */
   rtx result = NULL_RTX;
   result = inline_expand_builtin_bytecmp(self,exp, target);
   if (result)
      return result;

   insn_code cmpstr_icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,
               cmpstr_optab, mtcsMode->modes.M_SImode);
   insn_code cmpstrn_icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,
               cmpstrn_optab, mtcsMode->modes.M_SImode);
   if (cmpstr_icode == CODE_FOR_nothing && cmpstrn_icode == CODE_FOR_nothing)
      return NULL_RTX;

   unsigned int arg1_align = get_pointer_alignment (arg1) / BITS_PER_UNIT;
   unsigned int arg2_align = get_pointer_alignment (arg2) / BITS_PER_UNIT;

   /* If we don't have POINTER_TYPE, call the function.  */
   if (arg1_align == 0 || arg2_align == 0)
      return NULL_RTX;

   /* Stabilize the arguments in case gen_cmpstr(n)si fail.  */
   arg1 = builtin_save_expr (arg1);
   arg2 = builtin_save_expr (arg2);

   rtx arg1_rtx = mtcs_builtins_get_memory_rtx/*!get_memory_rtx*/(self,arg1, NULL);
   rtx arg2_rtx = mtcs_builtins_get_memory_rtx/*!get_memory_rtx*/(self,arg2, NULL);

   /* Try to call cmpstrsi.  */
   if (cmpstr_icode != CODE_FOR_nothing)
      result = expand_cmpstr(self,cmpstr_icode, target, arg1_rtx, arg2_rtx,MIN (arg1_align, arg2_align));

   /* Try to determine at least one length and call cmpstrnsi.  */
   if (!result && cmpstrn_icode != CODE_FOR_nothing){
      tree len;
      rtx arg3_rtx;

      tree len1 = c_strlen (arg1, 1);
      tree len2 = c_strlen (arg2, 1);

      if (len1)
         len1 = mtcs_const_size_binop/*!size_binop*/(mtcsConst,PLUS_EXPR, ssize_int (1), len1);
      if (len2)
         len2 = mtcs_const_size_binop/*!size_binop*/(mtcsConst,PLUS_EXPR, ssize_int (1), len2);

      /* If we don't have a constant length for the first, use the length
      of the second, if we know it.  We don't require a constant for
      this case; some cost analysis could be done if both are available
      but neither is constant.  For now, assume they're equally cheap,
      unless one has side effects.  If both strings have constant lengths,
      use the smaller.  */

      if (!len1)
         len = len2;
      else if (!len2)
         len = len1;
      else if (TREE_SIDE_EFFECTS (len1))
         len = len2;
      else if (TREE_SIDE_EFFECTS (len2))
         len = len1;
      else if (TREE_CODE (len1) != INTEGER_CST)
         len = len2;
      else if (TREE_CODE (len2) != INTEGER_CST)
         len = len1;
      else if (tree_int_cst_lt (len1, len2))
         len = len1;
      else
         len = len2;

      /* If both arguments have side effects, we cannot optimize.  */
      if (len && !TREE_SIDE_EFFECTS (len)){
         arg3_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,len);
         result = mtcs_expr_expand_cmpstrn_or_cmpmem/*!expand_cmpstrn_or_cmpmem*/(mtcsExpr,
                   cmpstrn_icode, target, arg1_rtx, arg2_rtx, TREE_TYPE (len),arg3_rtx, MIN (arg1_align, arg2_align));
      }
   }

   tree fndecl = get_callee_fndecl (exp);
   if (result){
      /* Return the value in the proper mode for this function.  */
      machine_mode mode = TYPE_MODE (TREE_TYPE (exp));
      mode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (exp),mode);

      if (GET_MODE (result) == mode)
         return result;
      if (target == 0)
         return mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,mode, result, 0);
      mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,target, result, 0);
      return target;
   }

   /* Expand the library call ourselves using a stabilized argument
   list to avoid re-evaluating the function's arguments twice.  */
   tree fn = build_call_nofold_loc(self,EXPR_LOCATION (exp), fndecl, 2, arg1, arg2);
   copy_warning (fn, exp);
   gcc_assert (TREE_CODE (fn) == CALL_EXPR);
   CALL_EXPR_TAILCALL (fn) = CALL_EXPR_TAILCALL (exp);
   return mtcs_calls_expand_call/*!expand_call*/(mtcsCalls,fn, target, target == const0_rtx);
}

/* __builtin_longjmp is passed a pointer to an array of five words (not
   all will be used on all machines).  It operates similarly to the C
   library function of the same name, but is more efficient.  Much of
   the code below is copied from the handling of non-local gotos.  */
//原型 expand_builtin_longjmp builtins.cc
static void expand_builtin_longjmp (MtcsBuiltins *self,rtx buf_addr, rtx value)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);
   MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

   rtx fp, lab, stack;
   rtx_insn *insn, *last;
   machine_mode sa_mode = mtcs_mode_get_stack_savearea_mode/*!STACK_SAVEAREA_MODE*/(mtcsMode,SAVE_NONLOCAL);
   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
   scalar_int_mode scalarPmode=mtcs_mode_as_a<scalar_int_mode>(mtcsMode,pMode);

   /* DRAP is needed for stack realign if longjmp is expanded to current
   function  */
   if (mtcs_func_is_support_stack_alignment/*!SUPPORTS_STACK_ALIGNMENT*/(mtcsFunc))
      mtcsRtlData/*!crtl*/->need_drap = true;

   if (self->setjmp_alias_set == -1)
      self->setjmp_alias_set = mtcs_alias_new_alias_set/*!new_alias_set*/(mtcsAlias);

   buf_addr = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,scalarPmode, buf_addr);
   buf_addr = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,pMode, buf_addr);
   /* We require that the user must pass a second argument of 1, because
   that is what builtin_setjmp will return.  */
   gcc_assert (value == const1_rtx);

   last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
   if (target_rtx_have_builtin_longjmp/*!targetm.have_builtin_longjmp*/(mtcsMachine->tmrtx))
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,
            target_rtx_gen_builtin_longjmp/*!targetm.gen_builtin_longjmp*/(mtcsMachine->tmrtx,buf_addr));
   else{
      fp = gen_rtx_MEM (pMode, buf_addr);
      lab = gen_rtx_MEM (pMode, mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,
            pMode, buf_addr, mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,pMode)));

      stack = gen_rtx_MEM (sa_mode, mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, buf_addr,
      2 * mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,pMode)));
      mtcs_rtl_set_mem_alias_set/*!set_mem_alias_set*/(mtcsRTL,fp, self->setjmp_alias_set);
      mtcs_rtl_set_mem_alias_set/*!set_mem_alias_set*/(mtcsRTL,lab, self->setjmp_alias_set);
      mtcs_rtl_set_mem_alias_set/*!set_mem_alias_set*/(mtcsRTL,stack, self->setjmp_alias_set);

      /* Pick up FP, label, and SP from the block and jump.  This code is
      from expand_goto in stmt.cc; see there for detailed comments.  */
      if (target_rtx_have_nonlocal_goto/*!targetm.have_nonlocal_goto*/(mtcsMachine->tmrtx))
         /* We have to pass a value to the nonlocal_goto pattern that will
         get copied into the static_chain pointer, but it does not matter
         what that value is, because builtin_setjmp does not use it.  */
         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,
               target_rtx_gen_nonlocal_goto/*!targetm.gen_nonlocal_goto*/(mtcsMachine->tmrtx,value, lab, stack, fp));
      else{
         mtcs_emit_emit_clobber/*!emit_clobber*/(mtcsEmit,gen_rtx_MEM (mtcsMode->modes.M_BLKmode, gen_rtx_SCRATCH (VOIDmode)));
         mtcs_emit_emit_clobber/*!emit_clobber*/(mtcsEmit,gen_rtx_MEM (mtcsMode->modes.M_BLKmode,
               mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL)));

         lab = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,lab);
         /* Restore the frame pointer and stack pointer.  We must use a
         temporary since the setjmp buffer may be a local.  */
         fp = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,fp);
         mtcs_explow_emit_stack_restore/*!emit_stack_restore*/(mtcsExplow,SAVE_NONLOCAL, stack);
         /* Ensure the frame pointer move is not optimized.  */
         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_blockage ());
         mtcs_emit_emit_clobber/*!emit_clobber*/(mtcsEmit,
               mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL));
         mtcs_emit_emit_clobber/*!emit_clobber*/(mtcsEmit,mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL));
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,
               mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL), fp);

         mtcs_emit_emit_use/*!emit_use*/(mtcsEmit,
               mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL));
         mtcs_emit_emit_use/*!emit_use*/(mtcsEmit,mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL));
         mtcs_optabs_emit_indirect_jump/*!emit_indirect_jump*/(mtcsOptabs,lab);
      }
   }

   /* Search backwards and mark the jump insn as a non-local goto.
   Note that this precludes the use of __builtin_longjmp to a
   __builtin_setjmp target in the same function.  However, we've
   already cautioned the user that these functions are for
   internal exception handling use only.  */
   for (insn = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData); insn; insn = PREV_INSN (insn)){
      gcc_assert (insn != last);

      if (JUMP_P (insn)){
         add_reg_note (insn, REG_NON_LOCAL_GOTO, const0_rtx);
         break;
      }else if (CALL_P (insn))
         break;
   }
}

/* Expand a call to __builtin_nonlocal_goto.  We're passed the target label
   and the address of the save area.  */
//原型 expand_builtin_nonlocal_goto builtins.cc
static rtx expand_builtin_nonlocal_goto (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
   scalar_int_mode scalarPmode=mtcs_mode_as_a<scalar_int_mode>(mtcsMode,pMode);

   tree t_label, t_save_area;
   rtx r_label, r_save_area, r_fp, r_sp;
   rtx_insn *insn;

   if (!validate_arglist (exp, POINTER_TYPE, POINTER_TYPE, VOID_TYPE))
      return NULL_RTX;

   t_label = CALL_EXPR_ARG (exp, 0);
   t_save_area = CALL_EXPR_ARG (exp, 1);

   r_label = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,t_label);
   r_label = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,scalarPmode, r_label);
   r_save_area = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,t_save_area);
   r_save_area = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,scalarPmode, r_save_area);
   /* Copy the address of the save location to a register just in case it was
   based on the frame pointer.   */
   r_save_area = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,r_save_area);
   r_fp = gen_rtx_MEM (pMode, r_save_area);
   r_sp = gen_rtx_MEM (mtcs_mode_get_stack_savearea_mode/*!STACK_SAVEAREA_MODE*/(mtcsMode,SAVE_NONLOCAL),
               mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, r_save_area,
               mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,pMode)));

   mtcsRtlData/*!crtl*/->has_nonlocal_goto = 1;

   /* ??? We no longer need to pass the static chain value, afaik.  */
   if (target_rtx_have_nonlocal_goto/*!targetm.have_nonlocal_goto*/(mtcsMachine->tmrtx))
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,
                     target_rtx_gen_nonlocal_goto/*!targetm.gen_nonlocal_goto*/(mtcsMachine->tmrtx,
                     const0_rtx, r_label, r_sp, r_fp));
   else{
      mtcs_emit_emit_clobber/*!emit_clobber*/(mtcsEmit,gen_rtx_MEM (mtcsMode->modes.M_BLKmode,
                     gen_rtx_SCRATCH (VOIDmode)));
      mtcs_emit_emit_clobber/*!emit_clobber*/(mtcsEmit,gen_rtx_MEM (mtcsMode->modes.M_BLKmode,
                  mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL)));

      r_label = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,r_label);

      /* Restore the frame pointer and stack pointer.  We must use a
      temporary since the setjmp buffer may be a local.  */
      r_fp = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,r_fp);
      mtcs_explow_emit_stack_restore/*!emit_stack_restore*/(mtcsExplow,SAVE_NONLOCAL, r_sp);

      /* Ensure the frame pointer move is not optimized.  */
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_blockage ());
      mtcs_emit_emit_clobber/*!emit_clobber*/(mtcsEmit,
                  mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL));
      mtcs_emit_emit_clobber/*!emit_clobber*/(mtcsEmit,mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL));
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,
                  mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL), r_fp);

      /* USE of hard_frame_pointer_rtx added for consistency;
      not clear if really needed.  */
      mtcs_emit_emit_use/*!emit_use*/(mtcsEmit,
                  mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL));
      mtcs_emit_emit_use/*!emit_use*/(mtcsEmit,
      mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL));

      /* If the architecture is using a GP register, we must
      conservatively assume that the target function makes use of it.
      The prologue of functions with nonlocal gotos must therefore
      initialize the GP register to the appropriate value, and we
      must then make sure that this value is live at the point
      of the jump.  (Note that this doesn't necessarily apply
      to targets with a nonlocal_goto pattern; they are free
      to implement it in their own way.  Note also that this is
      a no-op if the GP register is a global invariant.)  */
      unsigned regnum = mtcs_reg_get_pic_offset_table_regnum/*!PIC_OFFSET_TABLE_REGNUM*/(mtcsReg);
      if (regnum != INVALID_REGNUM && mtcsReg->hardRegs.x_fixed_regs/*!fixed_regs*/[regnum])
         mtcs_emit_emit_use/*!emit_use*/(mtcsEmit,mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(mtcsRTL));

      mtcs_optabs_emit_indirect_jump/*!emit_indirect_jump*/(mtcsOptabs,r_label);
   }
   /* Search backwards to the jump insn and mark it as a
   non-local goto.  */
   for (insn = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData); insn; insn = PREV_INSN (insn)){
      if (JUMP_P (insn)){
         add_reg_note (insn, REG_NON_LOCAL_GOTO, const0_rtx);
         break;
      }else if (CALL_P (insn))
         break;
   }
   return const0_rtx;
}


/* Expand a call to __builtin_unreachable.  We do nothing except emit
   a barrier saying that control flow will not pass here.

   It is the responsibility of the program being compiled to ensure
   that control flow does never reach __builtin_unreachable.  */
//原型 expand_builtin_unreachable builtins.cc
static void expand_builtin_unreachable (MtcsBuiltins *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   /* Use gimple_build_builtin_unreachable or builtin_decl_unreachable
   to avoid this.  */
   gcc_checking_assert (!sanitize_flags_p (SANITIZE_UNREACHABLE));
   mtcs_emit_emit_barrier/*!emit_barrier*/(mtcsEmit);
}

/* Expand the call EXP to the built-in signbit, signbitf or signbitl
   function.  The function first checks whether the back end provides
   an insn to implement signbit for the respective mode.  If not, it
   checks whether the floating point format of the value is such that
   the sign bit can be extracted.  If that is not the case, error out.
   EXP is the expression that is a call to the builtin function; if
   convenient, the result should be placed in TARGET.  */
//原型 expand_builtin_signbit builtins.cc
static rtx expand_builtin_signbit (MtcsBuiltins *self,tree exp, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

   const struct real_format *fmt;
   scalar_float_mode fmode;
   scalar_int_mode rmode, imode;
   tree arg;
   int word, bitpos;
   enum insn_code icode;
   rtx temp;
   location_t loc = EXPR_LOCATION (exp);

   if (!validate_arglist (exp, REAL_TYPE, VOID_TYPE))
      return NULL_RTX;

   arg = CALL_EXPR_ARG (exp, 0);
   fmode = mtcs_mode_host2device_scalar_float/*!SCALAR_FLOAT_TYPE_MODE (TREE_TYPE (arg))*/(mtcsMode,TREE_TYPE (arg));
   rmode = mtcs_mode_host2device_scalar_int/*!SCALAR_INT_TYPE_MODE (TREE_TYPE (exp))*/(mtcsMode,TREE_TYPE (exp));
   fmt = mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,fmode);

   arg = builtin_save_expr (arg);

   /* Expand the argument yielding a RTX expression. */
   temp = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg);

   /* Check if the back end provides an insn that handles signbit for the
   argument's mode. */
   icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,signbit_optab, fmode);
   if (icode != CODE_FOR_nothing){
      mtcs_mode mode= mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (exp),TYPE_MODE (TREE_TYPE (exp)));

      rtx_insn *last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
      rtx this_target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode/*!TYPE_MODE (TREE_TYPE (exp))*/);
      if (mtcs_optabs_maybe_emit_unop_insn/*!maybe_emit_unop_insn*/(mtcsOptabs,icode, this_target, temp, UNKNOWN))
         return this_target;
      mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
   }

   /* For floating point formats without a sign bit, implement signbit
   as "ARG < 0.0".  */
   bitpos = fmt->signbit_ro;
   if (bitpos < 0){
      /* But we can't do this if the format supports signed zero.  */
      gcc_assert (!fmt->has_signed_zero || !HONOR_SIGNED_ZEROS (fmode));

      arg = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, LT_EXPR, TREE_TYPE (exp), arg,
      build_real (TREE_TYPE (arg), dconst0));
      return mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,arg, target, VOIDmode, EXPAND_NORMAL);
   }

   if (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,fmode) <= UNITS_PER_WORD){
      imode = mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,fmode).require ();
      temp = gen_lowpart (imode, temp);
   }else {
      imode = word_mode;
      /* Handle targets with different FP word orders.  */
      if (FLOAT_WORDS_BIG_ENDIAN)
         word = (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,fmode) - bitpos) / BITS_PER_WORD;
      else
         word = bitpos / BITS_PER_WORD;
      temp = mtcs_rtl_operand_subword_force/*!operand_subword_force*/(mtcsRTL,temp, word, fmode);
      bitpos = bitpos % BITS_PER_WORD;
   }

   /* Force the intermediate word_mode (or narrower) result into a
   register.  This avoids attempting to create paradoxical SUBREGs
   of floating point modes below.  */
   temp = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,imode, temp);

   /* If the bitpos is within the "result mode" lowpart, the operation
   can be implement with a single bitwise AND.  Otherwise, we need
   a right shift and an AND.  */

   if (bitpos < mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,rmode)){
      wide_int mask = wi::set_bit_in_zero (bitpos, mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,rmode));

      if (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,imode) > mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,rmode))
         temp = gen_lowpart (rmode, temp);
      temp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,rmode, and_optab, temp,
      mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,mask, rmode),
      NULL_RTX, 1, OPTAB_LIB_WIDEN);
   }else{
      /* Perform a logical right shift to place the signbit in the least
      significant bit, then truncate the result to the desired mode
      and mask just this bit.  */
      temp = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, imode, temp, bitpos, NULL_RTX, 1);
      temp = gen_lowpart (rmode, temp);
      temp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,rmode, and_optab, temp, const1_rtx,
                              NULL_RTX, 1, OPTAB_LIB_WIDEN);
   }

   return temp;
}

/* Make it easier for the backends by protecting the valist argument
   from multiple evaluations.  */
//原型 stabilize_va_list_loc builtins.cc
static tree stabilize_va_list_loc (MtcsBuiltins *self,location_t loc, tree valist, int needs_lvalue)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst = mtcs_target_get_const(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree vatype = mtcsTarget/*!targetm.canonical_va_list_type*/->canonical_va_list_type(mtcsTarget,TREE_TYPE (valist));
   /* The current way of determining the type of valist is completely
   bogus.  We should have the information on the va builtin instead.  */
   if (!vatype)
      vatype = mtcsTarget/*!targetm.fn_abi_va_list*/->fn_abi_va_list(mtcsTarget,cfun->decl);

   if (TREE_CODE (vatype) == ARRAY_TYPE){
      if (TREE_SIDE_EFFECTS (valist))
         valist = save_expr (valist);
      /* For this case, the backends will be expecting a pointer to
      vatype, but it's possible we've actually been given an array
      (an actual TARGET_CANONICAL_VA_LIST_TYPE (valist)).
      So fix it.  */
      if (TREE_CODE (TREE_TYPE (valist)) == ARRAY_TYPE){
         tree p1 = mtcs_tree_build_pointer_type/*!build_pointer_type*/(mtcsTree,TREE_TYPE (vatype));
         valist = build_fold_addr_expr_with_type_loc (loc, valist, p1);
      }
   }else {
      tree pt = mtcs_tree_build_pointer_type/*!build_pointer_type*/(mtcsTree,vatype);
      if (! needs_lvalue){
         if (! TREE_SIDE_EFFECTS (valist))
            return valist;
         valist = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(mtcsConst,loc, ADDR_EXPR, pt, valist);
         TREE_SIDE_EFFECTS (valist) = 1;
      }
      if (TREE_SIDE_EFFECTS (valist))
         valist = save_expr (valist);
      valist = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,
            loc, MEM_REF,vatype, valist, mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,pt, 0));
   }
   return valist;
}

/* Expand EXP, a call to __builtin_va_start.  */
//原型 expand_builtin_va_start builtins.cc
static rtx expand_builtin_va_start (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   rtx nextarg;
   tree valist;
   location_t loc = EXPR_LOCATION (exp);
   if (call_expr_nargs (exp) < 2){
      error_at (loc, "too few arguments to function %<va_start%>");
      return const0_rtx;
   }
   if (mtcs_builtins_fold_builtin_next_arg/*!fold_builtin_next_arg*/(self,exp, true))
      return const0_rtx;

   nextarg = expand_builtin_next_arg(self);
   valist = stabilize_va_list_loc(self,loc, CALL_EXPR_ARG (exp, 0), 1);
   if (mtcsTarget/*!targetm.expand_builtin_va_start*/->expand_builtin_va_start)
      mtcsTarget/*!targetm.expand_builtin_va_start*/->expand_builtin_va_start(mtcsTarget,valist, nextarg);
   else
      mtcs_builtins_std_expand_builtin_va_start/*!std_expand_builtin_va_start*/(self,valist, nextarg);
   return const0_rtx;
}

/* Expand EXP, a call to __builtin_va_end.  */
//原型 expand_builtin_va_start builtins.cc
static rtx expand_builtin_va_end (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   tree valist = CALL_EXPR_ARG (exp, 0);
   /* Evaluate for side effects, if needed.  I hate macros that don't
   do that.  */
   if (TREE_SIDE_EFFECTS (valist))
      mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,valist, const0_rtx, VOIDmode, EXPAND_NORMAL);

   return const0_rtx;
}

/* Expand EXP, a call to __builtin_va_copy.  We do this as a
   builtin rather than just as an assignment in stdarg.h because of the
   nastiness of array-type va_list types.  */
//原型 expand_builtin_va_copy builtins.cc
static rtx expand_builtin_va_copy (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);

   tree dst, src, t;
   location_t loc = EXPR_LOCATION (exp);
   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
   scalar_int_mode scalarPmode=mtcs_mode_as_a<scalar_int_mode>(mtcsMode,pMode);

   dst = CALL_EXPR_ARG (exp, 0);
   src = CALL_EXPR_ARG (exp, 1);

   dst = stabilize_va_list_loc(self,loc, dst, 1);
   src = stabilize_va_list_loc(self,loc, src, 0);

   gcc_assert (cfun != NULL && cfun->decl != NULL_TREE);

   if (TREE_CODE (targetm.fn_abi_va_list (cfun->decl)) != ARRAY_TYPE){
      t = build2 (MODIFY_EXPR, targetm.fn_abi_va_list (cfun->decl), dst, src);
      TREE_SIDE_EFFECTS (t) = 1;
      mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,t, const0_rtx, VOIDmode, EXPAND_NORMAL);
   } else {
      rtx dstb, srcb, size;

      /* Evaluate to pointers.  */
      dstb = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,dst, NULL_RTX, pMode, EXPAND_NORMAL);
      srcb = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,src, NULL_RTX, pMode, EXPAND_NORMAL);
      size = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,
            TYPE_SIZE_UNIT (mtcsTarget/*!targetm.fn_abi_va_list*/->fn_abi_va_list(mtcsTarget,cfun->decl)),
            NULL_RTX, VOIDmode, EXPAND_NORMAL);

      dstb = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,scalarPmode, dstb);
      srcb = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,scalarPmode, srcb);

      /* "Dereference" to BLKmode memories.  */
      dstb = gen_rtx_MEM (mtcsMode->modes.M_BLKmode, dstb);
      mtcs_rtl_set_mem_alias_set/*!set_mem_alias_set*/(mtcsRTL,dstb,
            mtcs_alias_get_alias_set/*!get_alias_set*/(mtcsAlias,TREE_TYPE (TREE_TYPE (dst))));
      mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,dstb, TYPE_ALIGN (targetm.fn_abi_va_list (cfun->decl)));
      srcb = gen_rtx_MEM (mtcsMode->modes.M_BLKmode, srcb);
      mtcs_rtl_set_mem_alias_set/*!set_mem_alias_set*/(mtcsRTL,srcb,
            mtcs_alias_get_alias_set/*!get_alias_set*/(mtcsAlias,TREE_TYPE (TREE_TYPE (src))));
      mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,srcb, TYPE_ALIGN (targetm.fn_abi_va_list (cfun->decl)));

      /* Copy.  */
      mtcs_expr_emit_block_move/*!emit_block_move*/(mtcsExpr,dstb, srcb, size, BLOCK_OP_NORMAL);
   }
   return const0_rtx;
}

/* Expand a call to __builtin_expect.  We just return our argument
   as the builtin_expect semantic should've been already executed by
   tree branch prediction pass. */
//原型 expand_builtin_va_copy builtins.cc
static rtx expand_builtin_expect (MtcsBuiltins *self,tree exp, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

   tree arg;
   if (call_expr_nargs (exp) < 2)
      return const0_rtx;
   arg = CALL_EXPR_ARG (exp, 0);
   target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,arg, target, VOIDmode, EXPAND_NORMAL);
   /* When guessing was done, the hints should be already stripped away.  */
   gcc_assert (!mtcsOptionsItem->x_flag_guess_branch_prob || mtcsOptionsItem->x_optimize == 0 || seen_error ());
   return target;
}

/* Expand a call to __builtin_expect_with_probability.  We just return our
   argument as the builtin_expect semantic should've been already executed by
   tree branch prediction pass.  */
//原型 expand_builtin_expect_with_probability builtins.cc
static rtx expand_builtin_expect_with_probability (MtcsBuiltins *self,tree exp, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

   tree arg;
   if (call_expr_nargs (exp) < 3)
      return const0_rtx;
   arg = CALL_EXPR_ARG (exp, 0);
   target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,arg, target, VOIDmode, EXPAND_NORMAL);
   /* When guessing was done, the hints should be already stripped away.  */
   gcc_assert (!mtcsOptionsItem->x_flag_guess_branch_prob || mtcsOptionsItem->x_optimize == 0 || seen_error ());
   return target;
}

/* Expand a call to __builtin_assume_aligned.  We just return our first
   argument as the builtin_assume_aligned semantic should've been already
   executed by CCP.  */
//原型 expand_builtin_assume_aligned builtins.cc
static rtx expand_builtin_assume_aligned (MtcsBuiltins *self, tree exp, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   if (call_expr_nargs (exp) < 2)
      return const0_rtx;
   target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,CALL_EXPR_ARG (exp, 0),
                  target, VOIDmode,EXPAND_NORMAL);
   gcc_assert (!TREE_SIDE_EFFECTS (CALL_EXPR_ARG (exp, 1))
   && (call_expr_nargs (exp) < 3   || !TREE_SIDE_EFFECTS (CALL_EXPR_ARG (exp, 2))));
   return target;
}

/* Expand a call to __builtin_prefetch.  For a target that does not support
   data prefetch, evaluate the memory address argument in case it has side
   effects.  */
//原型 expand_builtin_prefetch builtins.cc
static void expand_builtin_prefetch (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

   tree arg0, arg1, arg2;
   int nargs;
   rtx op0, op1, op2;

   if (!validate_arglist (exp, POINTER_TYPE, 0))
      return;

   arg0 = CALL_EXPR_ARG (exp, 0);

   /* Arguments 1 and 2 are optional; argument 1 (read/write) defaults to
   zero (read) and argument 2 (locality) defaults to 3 (high degree of
   locality).  */
   nargs = call_expr_nargs (exp);
   arg1 = nargs > 1 ? CALL_EXPR_ARG (exp, 1) : NULL_TREE;
   arg2 = nargs > 2 ? CALL_EXPR_ARG (exp, 2) : NULL_TREE;

   /* Argument 0 is an address.  */
   op0 = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,arg0, NULL_RTX,
                         mtcs_mode_get_Pmode(mtcsMode), EXPAND_NORMAL);

   /* Argument 1 (read/write flag) must be a compile-time constant int.  */
   /* Argument 1 (read/write flag) must be a compile-time constant int.  */
   if (arg1 == NULL_TREE)
     op1 = const0_rtx;
   else if (TREE_CODE (arg1) != INTEGER_CST){
      error ("second argument to %<__builtin_prefetch%> must be a constant");
      arg1 = integer_zero_node;
   }else
   op1 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg1);
   /* Argument 1 must be either zero or one.  */
   if (INTVAL (op1) != 0 && INTVAL (op1) != 1){
      warning (0, "invalid second argument to %<__builtin_prefetch%>; using zero");
      op1 = const0_rtx;
   }

   /* Argument 2 (locality) must be a compile-time constant int.  */
   if (arg2 == NULL_TREE)
     op2 = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,3);
   else if (TREE_CODE (arg2) != INTEGER_CST){
      error ("third argument to %<__builtin_prefetch%> must be a constant");
      arg2 = integer_zero_node;
   }else
      op2 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg2);
   /* Argument 2 must be 0, 1, 2, or 3.  */
   if (INTVAL (op2) < 0 || INTVAL (op2) > 3){
      warning (0, "invalid third argument to %<__builtin_prefetch%>; using zero");
      op2 = const0_rtx;
   }

   if (target_rtx_have_prefetch/*!targetm.have_prefetch*/(mtcsMachine->tmrtx)) {
      class expand_operand ops[3];

      create_address_operand (&ops[0], op0);
      mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[1], INTVAL (op1));
      mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[2], INTVAL (op2));
      if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(mtcsOptabs,
            mtcsMachine->tmrtx->code_for_prefetch/*!targetm.code_for_prefetch*/, 3, ops))
      return;
   }
   /* Don't do anything with direct references to volatile memory, but
   generate code to handle other side effects.  */
   if (!MEM_P (op0) && side_effects_p (op0))
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,op0);
}

/* Given a trampoline address, make sure it satisfies TRAMPOLINE_ALIGNMENT.  */
//原型 round_trampoline_addr builtins.cc
static rtx round_trampoline_addr (MtcsBuiltins *self,rtx tramp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   rtx temp, addend, mask;
   nuint trampolineAlignment=mtcs_align_get_trampoline_alignment/*!TRAMPOLINE_ALIGNMENT*/(mtcsAlign);
   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);

   /* If we don't need too much alignment, we'll have been guaranteed
   proper alignment by get_trampoline_type.  */
   if (trampolineAlignment/*!TRAMPOLINE_ALIGNMENT*/ <=mtcs_func_get_stack_boundary/*!STACK_BOUNDARY*/(mtcsFunc))
      return tramp;

   /* Round address up to desired boundary.  */
   temp = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,pMode);
   addend =mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,
               trampolineAlignment/*!TRAMPOLINE_ALIGNMENT*/ / BITS_PER_UNIT - 1, pMode);
   mask = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,
               -trampolineAlignment/*!TRAMPOLINE_ALIGNMENT*/ / BITS_PER_UNIT, pMode);

   temp  = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,pMode, PLUS, tramp, addend,
               temp, 0, OPTAB_LIB_WIDEN);
   tramp = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,pMode, AND, temp, mask,
               temp, 0, OPTAB_LIB_WIDEN);

   return tramp;
}

//原型 expand_builtin_init_trampoline builtins.cc
static rtx expand_builtin_init_trampoline (MtcsBuiltins *self,tree exp, bool onstack)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   tree t_tramp, t_func, t_chain;
   rtx m_tramp, r_tramp, r_chain, tmp;

   if (!validate_arglist (exp, POINTER_TYPE, POINTER_TYPE, POINTER_TYPE, VOID_TYPE))
      return NULL_RTX;

   t_tramp = CALL_EXPR_ARG (exp, 0);
   t_func = CALL_EXPR_ARG (exp, 1);
   t_chain = CALL_EXPR_ARG (exp, 2);

   r_tramp = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,t_tramp);
   m_tramp = gen_rtx_MEM (mtcsMode->modes.M_BLKmode, r_tramp);
   MEM_NOTRAP_P (m_tramp) = 1;

   /* If ONSTACK, the TRAMP argument should be the address of a field
   within the local function's FRAME decl.  Either way, let's see if
   we can fill in the MEM_ATTRs for this memory.  */
   if (TREE_CODE (t_tramp) == ADDR_EXPR)
      mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,m_tramp, TREE_OPERAND (t_tramp, 0), true);

   /* Creator of a heap trampoline is responsible for making sure the
   address is aligned to at least STACK_BOUNDARY.  Normally malloc
   will ensure this anyhow.  */
   tmp = round_trampoline_addr(self,r_tramp);
   if (tmp != r_tramp) {
      nuint trampolineAlignment=mtcs_align_get_trampoline_alignment/*!TRAMPOLINE_ALIGNMENT*/(mtcsAlign);
      nuint trampolineSize=mtcs_align_get_trampoline_size/*!TRAMPOLINE_SIZE*/(mtcsAlign);
      m_tramp = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,m_tramp, mtcsMode->modes.M_BLKmode, tmp);
      mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,m_tramp, trampolineAlignment/*!TRAMPOLINE_ALIGNMENT*/);
      mtcs_rtl_set_mem_size/*!set_mem_size*/(mtcsRTL,m_tramp, trampolineSize/*!TRAMPOLINE_SIZE*/);
   }

   /* The FUNC argument should be the address of the nested function.
   Extract the actual function decl to pass to the hook.  */
   gcc_assert (TREE_CODE (t_func) == ADDR_EXPR);
   t_func = TREE_OPERAND (t_func, 0);
   gcc_assert (TREE_CODE (t_func) == FUNCTION_DECL);

   r_chain = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,t_chain);
   /* Generate insns to initialize the trampoline.  */
   target_calls_trampoline_init/*!targetm.calls.trampoline_init*/(mtcsMachine->calls,m_tramp, t_func, r_chain);
   if (onstack) {
      trampolines_created = 1;

      if (mtcsMachine->calls->custom_function_descriptors/*!targetm.calls.custom_function_descriptors*/ != 0)
         warning_at (DECL_SOURCE_LOCATION (t_func), OPT_Wtrampolines,
                     "trampoline generated for nested function %qD", t_func);
   }
   return const0_rtx;
}

//原型 expand_builtin_adjust_trampoline  butilins.cc
static rtx expand_builtin_adjust_trampoline (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   rtx tramp;
   if (!validate_arglist (exp, POINTER_TYPE, VOID_TYPE))
      return NULL_RTX;
   tramp = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,CALL_EXPR_ARG (exp, 0));
   tramp = round_trampoline_addr(self,tramp);
   if (mtcsMachine->calls->trampoline_adjust_address/*!targetm.calls.trampoline_adjust_address*/)
      tramp = target_calls_trampoline_adjust_address/*!targetm.calls.trampoline_adjust_address*/(mtcsMachine->calls,tramp);
   return tramp;
}


/* Expand a call to the builtin descriptor initialization routine.
   A descriptor is made up of a couple of pointers to the static
   chain and the code entry in this order.  */
//原型 expand_builtin_init_descriptor  butilins.cc
static rtx expand_builtin_init_descriptor (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   tree t_descr, t_func, t_chain;
   rtx m_descr, r_descr, r_func, r_chain;

   if (!validate_arglist (exp, POINTER_TYPE, POINTER_TYPE, POINTER_TYPE, VOID_TYPE))
      return NULL_RTX;

   t_descr = CALL_EXPR_ARG (exp, 0);
   t_func = CALL_EXPR_ARG (exp, 1);
   t_chain = CALL_EXPR_ARG (exp, 2);

   r_descr = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,t_descr);
   m_descr = gen_rtx_MEM (mtcsMode->modes.M_BLKmode, r_descr);
   MEM_NOTRAP_P (m_descr) = 1;
   mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,m_descr,
         mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,ptr_mode));

   r_func = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,t_func);
   r_chain = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,t_chain);

   /* Generate insns to initialize the descriptor.  */
   mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,
               mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/(mtcsRTL,m_descr, ptr_mode, 0), r_chain);
   mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,
               mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/(mtcsRTL,m_descr, ptr_mode,
                        POINTER_SIZE / BITS_PER_UNIT), r_func);

   return const0_rtx;
}


/* Expand a call to the builtin descriptor adjustment routine.  */
//原型 expand_builtin_adjust_descriptor  butilins.cc
static rtx expand_builtin_adjust_descriptor (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   rtx tramp;
   if (!validate_arglist (exp, POINTER_TYPE, VOID_TYPE))
      return NULL_RTX;
   tramp = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,CALL_EXPR_ARG (exp, 0));
   /* Unalign the descriptor to allow runtime identification.  */
   tramp = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,ptr_mode, tramp,
         mtcsMachine->calls->custom_function_descriptors/*!targetm.calls.custom_function_descriptors*/);
   return mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,tramp, NULL_RTX);
}


/* Construct a new CALL_EXPR to FNDECL using the tail of the argument
   list ARGS along with N new arguments in NEWARGS.  SKIP is the number
   of arguments in ARGS to be omitted.  OLDNARGS is the number of
   elements in ARGS.  */
//原型 rewrite_call_expr_valist builtins.cc

static tree rewrite_call_expr_valist (location_t loc, int oldnargs, tree *args,
           int skip, tree fndecl, int n, va_list newargs)
{
   int nargs = oldnargs - skip + n;
   tree *buffer;

   if (n > 0){
      int i, j;
      buffer = XALLOCAVEC (tree, nargs);
      for (i = 0; i < n; i++)
         buffer[i] = va_arg (newargs, tree);
      for (j = skip; j < oldnargs; j++, i++)
         buffer[i] = args[j];
   }else
      buffer = args + skip;

   return build_call_expr_loc_array (loc, fndecl, nargs, buffer);
}

/* Construct a new CALL_EXPR using the tail of the argument list of EXP
   along with N new arguments specified as the "..." parameters.  SKIP
   is the number of arguments in EXP to be omitted.  This function is used
   to do varargs-to-varargs transformations.  */
//原型 rewrite_call_expr builtins.cc
static tree rewrite_call_expr (location_t loc, tree exp, int skip, tree fndecl, int n, ...)
{
   va_list ap;
   tree t;
   va_start (ap, n);
   t = rewrite_call_expr_valist (loc, call_expr_nargs (exp),
           CALL_EXPR_ARGP (exp), skip, fndecl, n, ap);
   va_end (ap);
   return t;
}

/* Expand fork or exec calls.  TARGET is the desired target of the
   call.  EXP is the call. FN is the
   identificator of the actual function.  IGNORE is nonzero if the
   value is to be ignored.  */
//原型 expand_builtin_fork_or_exec  butilins.cc
static rtx expand_builtin_fork_or_exec (MtcsBuiltins *self,tree fn, tree exp, rtx target, int ignore)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsTree *mtcsTree = mtcs_target_get_tree(mtcsTarget);

   tree id, decl;
   tree call;

   /* If we are not profiling, just call the function.  */
   if (!mtcsOptionsItem->x_profile_arc_flag && !mtcsOptionsItem->x_condition_coverage_flag)
      return NULL_RTX;

   /* Otherwise call the wrapper.  This should be equivalent for the rest of
   compiler, so the code does not diverge, and the wrapper may run the
   code necessary for keeping the profiling sane.  */

   switch (DECL_FUNCTION_CODE (fn)){
      case BUILT_IN_FORK:
         id = get_identifier ("__gcov_fork");
         break;

      case BUILT_IN_EXECL:
         id = get_identifier ("__gcov_execl");
         break;

      case BUILT_IN_EXECV:
         id = get_identifier ("__gcov_execv");
         break;

      case BUILT_IN_EXECLP:
         id = get_identifier ("__gcov_execlp");
         break;

      case BUILT_IN_EXECLE:
         id = get_identifier ("__gcov_execle");
         break;

      case BUILT_IN_EXECVP:
         id = get_identifier ("__gcov_execvp");
         break;

      case BUILT_IN_EXECVE:
         id = get_identifier ("__gcov_execve");
         break;

      default:
      gcc_unreachable ();
   }

   decl = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,DECL_SOURCE_LOCATION (fn), FUNCTION_DECL, id, TREE_TYPE (fn));
   DECL_EXTERNAL (decl) = 1;
   TREE_PUBLIC (decl) = 1;
   DECL_ARTIFICIAL (decl) = 1;
   TREE_NOTHROW (decl) = 1;
   DECL_VISIBILITY (decl) = VISIBILITY_DEFAULT;
   DECL_VISIBILITY_SPECIFIED (decl) = 1;
   call = rewrite_call_expr (EXPR_LOCATION (exp), exp, 0, decl, 0);
   return mtcs_calls_expand_call/*!expand_call*/(mtcsCalls,call, target, ignore);
 }

/* Reconstitute a mode for a __sync intrinsic operation.  Since the type of
   the pointer in these functions is void*, the tree optimizers may remove
   casts.  The mode computed in expand_builtin isn't reliable either, due
   to __sync_bool_compare_and_swap.

   FCODE_DIFF should be fcode - base, where base is the FOO_1 code for the
   group of builtins.  This gives us log2 of the mode size.  */
//原型 get_builtin_sync_mode  butilins.cc
static inline machine_mode get_builtin_sync_mode (MtcsBuiltins *self,int fcode_diff)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  /* The size is not negotiable, so ask not to get BLKmode in return
     if the target indicates that a smaller size would be better.  */
   return mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,BITS_PER_UNIT << fcode_diff, 0).require ();
}

/* Expand the memory expression LOC and return the appropriate memory operand
   for the builtin_sync operations.  */
//原型 get_builtin_sync_mem  butilins.cc
static rtx get_builtin_sync_mem (MtcsBuiltins *self,tree loc, machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   rtx addr, mem;
   int addr_space = TYPE_ADDR_SPACE (POINTER_TYPE_P (TREE_TYPE (loc)) ? TREE_TYPE (TREE_TYPE (loc)) : TREE_TYPE (loc));
   scalar_int_mode addr_mode =target_addr_space_address_mode/*!targetm.addr_space.address_mode*/(mtcsMachine->addrSpace,addr_space);

   addr = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,loc, NULL_RTX, addr_mode, EXPAND_SUM);
   addr = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,addr_mode, addr);
   /* Note that we explicitly do not want any alias information for this
   memory, so that we kill all other live memories.  Otherwise we don't
   satisfy the full barrier semantics of the intrinsic.  */
   mem = gen_rtx_MEM (mode, addr);
   mtcs_rtl_set_mem_addr_space/*!set_mem_addr_space*/(mtcsRTL,mem, addr_space);
   mem = mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,mem);
   /* The alignment needs to be at least according to that of the mode.  */
   mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,mem,
   MAX (mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode),get_pointer_alignment (loc)));
   mtcs_rtl_set_mem_alias_set/*!set_mem_alias_set*/(mtcsRTL,mem, ALIAS_SET_MEMORY_BARRIER);
   MEM_VOLATILE_P (mem) = 1;

   return mem;
}

/* Make sure an argument is in the right mode.
   EXP is the tree argument.
   MODE is the mode it should be in.  */
//原型 expand_expr_force_mode  butilins.cc
static rtx expand_expr_force_mode (MtcsBuiltins *self,tree exp, machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   rtx val;
   machine_mode old_mode;

   if (TREE_CODE (exp) == SSA_NAME  && TYPE_MODE (TREE_TYPE (exp)) != mode){
      /* Undo argument promotion if possible, as combine might not
      be able to do it later due to MEM_VOLATILE_P uses in the
      patterns.  */
      gimple *g = get_gimple_for_ssa_name (exp);
      if (g && gimple_assign_cast_p (g)){
         tree rhs = gimple_assign_rhs1 (g);
         tree_code code = gimple_assign_rhs_code (g);
         if (CONVERT_EXPR_CODE_P (code)
            && TYPE_MODE (TREE_TYPE (rhs)) == mode
            && INTEGRAL_TYPE_P (TREE_TYPE (exp))
            && INTEGRAL_TYPE_P (TREE_TYPE (rhs))
            && (TYPE_PRECISION (TREE_TYPE (exp))
            > TYPE_PRECISION (TREE_TYPE (rhs))))
            exp = rhs;
      }
   }

   val = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,exp, NULL_RTX, mode, EXPAND_NORMAL);
   /* If VAL is promoted to a wider mode, convert it back to MODE.  Take care
   of CONST_INTs, where we know the old_mode only from the call argument.  */

   old_mode = GET_MODE (val);
   if (old_mode == VOIDmode)
      old_mode = TYPE_MODE (TREE_TYPE (exp));
   val = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,mode, old_mode, val, 1);
   return val;
}

/* Expand the __sync_xxx_and_fetch and __sync_fetch_and_xxx intrinsics.
   EXP is the CALL_EXPR.  CODE is the rtx code
   that corresponds to the arithmetic or logical operation from the name;
   an exception here is that NOT actually means NAND.  TARGET is an optional
   place for us to store the results; AFTER is true if this is the
   fetch_and_xxx form.  */
//原型 expand_builtin_sync_operation  butilins.cc
static rtx expand_builtin_sync_operation (MtcsBuiltins *self,machine_mode mode, tree exp,
                enum rtx_code code, bool after,  rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

   rtx val, mem;
   location_t loc = EXPR_LOCATION (exp);

   if (code == NOT && warn_sync_nand){
      tree fndecl = get_callee_fndecl (exp);
      enum built_in_function fcode = DECL_FUNCTION_CODE (fndecl);
      static bool warned_f_a_n, warned_n_a_f;

      switch (fcode){
         case BUILT_IN_SYNC_FETCH_AND_NAND_1:
         case BUILT_IN_SYNC_FETCH_AND_NAND_2:
         case BUILT_IN_SYNC_FETCH_AND_NAND_4:
         case BUILT_IN_SYNC_FETCH_AND_NAND_8:
         case BUILT_IN_SYNC_FETCH_AND_NAND_16:
            if (warned_f_a_n)
               break;

            fndecl = builtin_decl_implicit (BUILT_IN_SYNC_FETCH_AND_NAND_N);
            inform (loc, "%qD changed semantics in GCC 4.4", fndecl);
            warned_f_a_n = true;
            break;

         case BUILT_IN_SYNC_NAND_AND_FETCH_1:
         case BUILT_IN_SYNC_NAND_AND_FETCH_2:
         case BUILT_IN_SYNC_NAND_AND_FETCH_4:
         case BUILT_IN_SYNC_NAND_AND_FETCH_8:
         case BUILT_IN_SYNC_NAND_AND_FETCH_16:
            if (warned_n_a_f)
               break;

            fndecl = builtin_decl_implicit (BUILT_IN_SYNC_NAND_AND_FETCH_N);
            inform (loc, "%qD changed semantics in GCC 4.4", fndecl);
            warned_n_a_f = true;
            break;

         default:
            gcc_unreachable ();
      }
   }
   /* Expand the operands.  */
   mem = get_builtin_sync_mem(self,CALL_EXPR_ARG (exp, 0), mode);
   val = expand_expr_force_mode(self,CALL_EXPR_ARG (exp, 1), mode);
   return mtcs_optabs_expand_atomic_fetch_op/*!expand_atomic_fetch_op*/(mtcsOptabs,
         target, mem, val, code, MEMMODEL_SYNC_SEQ_CST, after);
}


/* Expand the __sync_val_compare_and_swap and __sync_bool_compare_and_swap
   intrinsics. EXP is the CALL_EXPR.  IS_BOOL is
   true if this is the boolean form.  TARGET is a place for us to store the
   results; this is NOT optional if IS_BOOL is true.  */
//原型 expand_builtin_compare_and_swap  butilins.cc
static rtx expand_builtin_compare_and_swap (MtcsBuiltins *self,machine_mode mode, tree exp,
             bool is_bool, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

   rtx old_val, new_val, mem;
   rtx *pbool, *poval;
   /* Expand the operands.  */
   mem = get_builtin_sync_mem(self,CALL_EXPR_ARG (exp, 0), mode);
   old_val = expand_expr_force_mode(self,CALL_EXPR_ARG (exp, 1), mode);
   new_val = expand_expr_force_mode(self,CALL_EXPR_ARG (exp, 2), mode);

   pbool = poval = NULL;
   if (target != const0_rtx) {
      if (is_bool)
         pbool = &target;
      else
         poval = &target;
   }
   if (!mtcs_optabs_expand_atomic_compare_and_swap/*!expand_atomic_compare_and_swap*/(mtcsOptabs,
         pbool, poval, mem, old_val, new_val,false, MEMMODEL_SYNC_SEQ_CST, MEMMODEL_SYNC_SEQ_CST))
      return NULL_RTX;

   return target;
}

/* Expand the __sync_lock_test_and_set intrinsic.  Note that the most
   general form is actually an atomic exchange, and some targets only
   support a reduced form with the second argument being a constant 1.
   EXP is the CALL_EXPR; TARGET is an optional place for us to store
   the results.  */
//原型 expand_builtin_sync_lock_test_and_set  butilins.cc
static rtx expand_builtin_sync_lock_test_and_set (MtcsBuiltins *self,machine_mode mode, tree exp,
                   rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   rtx val, mem;
   /* Expand the operands.  */
   mem = get_builtin_sync_mem(self,CALL_EXPR_ARG (exp, 0), mode);
   val = expand_expr_force_mode(self,CALL_EXPR_ARG (exp, 1), mode);
   return mtcs_optabs_expand_sync_lock_test_and_set/*!expand_sync_lock_test_and_set*/(mtcsOptabs,target, mem, val);
}


/* Expand the __sync_lock_release intrinsic.  EXP is the CALL_EXPR.  */
//原型 expand_builtin_sync_lock_release  butilins.cc
static void expand_builtin_sync_lock_release (MtcsBuiltins *self,machine_mode mode, tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   rtx mem;
   /* Expand the operands.  */
   mem = get_builtin_sync_mem(self,CALL_EXPR_ARG (exp, 0), mode);
   mtcs_optabs_expand_atomic_store/*!expand_atomic_store*/(mtcsOptabs,mem, const0_rtx, MEMMODEL_SYNC_RELEASE, true);
}

/* Given an integer representing an ``enum memmodel'', verify its
   correctness and return the memory model enum.  */
//原型 get_memmodel  butilins.cc
static enum memmodel get_memmodel (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   /* If the parameter is not a constant, it's a run time value so we'll just
   convert it to MEMMODEL_SEQ_CST to avoid annoying runtime checking.  */
   if (TREE_CODE (exp) != INTEGER_CST)
      return MEMMODEL_SEQ_CST;

   rtx op = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,exp);

   unsigned HOST_WIDE_INT val = INTVAL (op);
   if (mtcsTarget/*!targetm.memmodel_check*/->memmodel_check)
      val = mtcsTarget/*!targetm.memmodel_check*/->memmodel_check(mtcsTarget,val);
   else if (val & ~MEMMODEL_MASK)
      return MEMMODEL_SEQ_CST;

   /* Should never see a user explicit SYNC memodel model, so >= LAST works. */
   if (memmodel_base (val) >= MEMMODEL_LAST)
      return MEMMODEL_SEQ_CST;

   /* Workaround for Bugzilla 59448. GCC doesn't track consume properly, so
   be conservative and promote consume to acquire.  */
   if (val == MEMMODEL_CONSUME)
      val = MEMMODEL_ACQUIRE;

   return (enum memmodel) val;
}

/* Expand the __atomic_exchange intrinsic:
      TYPE __atomic_exchange (TYPE *object, TYPE desired, enum memmodel)
   EXP is the CALL_EXPR.
   TARGET is an optional place for us to store the results.  */
//原型 expand_builtin_atomic_exchange  butilins.cc
static rtx expand_builtin_atomic_exchange (MtcsBuiltins *self,machine_mode mode, tree exp, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

   rtx val, mem;
   enum memmodel model;
   model = get_memmodel(self,CALL_EXPR_ARG (exp, 2));
   if (!mtcsOptionsItem->x_flag_inline_atomics)
      return NULL_RTX;
   /* Expand the operands.  */
   mem = get_builtin_sync_mem(self,CALL_EXPR_ARG (exp, 0), mode);
   val = expand_expr_force_mode(self,CALL_EXPR_ARG (exp, 1), mode);

   return mtcs_optabs_expand_atomic_exchange/*!expand_atomic_exchange*/(mtcsOptabs,target, mem, val, model);
}

/* Expand the __atomic_compare_exchange intrinsic:
      bool __atomic_compare_exchange (TYPE *object, TYPE *expect,
               TYPE desired, BOOL weak,
               enum memmodel success,
               enum memmodel failure)
   EXP is the CALL_EXPR.
   TARGET is an optional place for us to store the results.  */
//原型 expand_builtin_atomic_compare_exchange  butilins.cc
static rtx expand_builtin_atomic_compare_exchange (MtcsBuiltins *self,machine_mode mode, tree exp,
               rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   rtx expect, desired, mem, oldval;
   rtx_code_label *label;
   tree weak;
   bool is_weak;

   memmodel success = get_memmodel(self,CALL_EXPR_ARG (exp, 4));
   memmodel failure = get_memmodel(self,CALL_EXPR_ARG (exp, 5));
   if (failure > success)
      success = MEMMODEL_SEQ_CST;

   if (is_mm_release (failure) || is_mm_acq_rel (failure)){
      failure = MEMMODEL_SEQ_CST;
      success = MEMMODEL_SEQ_CST;
   }
   if (!mtcsOptionsItem->x_flag_inline_atomics)
      return NULL_RTX;
   /* Expand the operands.  */
   mem = get_builtin_sync_mem(self,CALL_EXPR_ARG (exp, 0), mode);

   expect = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,CALL_EXPR_ARG (exp, 1));
   expect = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,Pmode, expect);
   expect = gen_rtx_MEM (mode, expect);
   desired = expand_expr_force_mode(self,CALL_EXPR_ARG (exp, 2), mode);

   weak = CALL_EXPR_ARG (exp, 3);
   is_weak = false;
   if (tree_fits_shwi_p (weak) && tree_to_shwi (weak) != 0)
      is_weak = true;

   if (target == const0_rtx)
      target = NULL;

   /* Lest the rtl backend create a race condition with an imporoper store
   to memory, always create a new pseudo for OLDVAL.  */
   oldval = NULL;

   if (!mtcs_optabs_expand_atomic_compare_and_swap/*!expand_atomic_compare_and_swap*/(mtcsOptabs,
         &target, &oldval, mem, expect, desired,is_weak, success, failure))
      return NULL_RTX;

   /* Conditionally store back to EXPECT, lest we create a race condition
   with an improper store to memory.  */
   /* ??? With a rearrangement of atomics at the gimple level, we can handle
   the normal case where EXPECT is totally private, i.e. a register.  At
   which point the store can be unconditional.  */
   label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
   mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,
         target, const0_rtx, NE, NULL, GET_MODE (target), 1, label);
   mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,expect, oldval);
   mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);

   return target;
}

/* Expand the __atomic_load intrinsic:
      TYPE __atomic_load (TYPE *object, enum memmodel)
   EXP is the CALL_EXPR.
   TARGET is an optional place for us to store the results.  */
//原型 expand_builtin_atomic_load  butilins.cc
static rtx expand_builtin_atomic_load (MtcsBuiltins *self,machine_mode mode, tree exp, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

   memmodel model = get_memmodel(self,CALL_EXPR_ARG (exp, 1));
   if (is_mm_release (model) || is_mm_acq_rel (model))
      model = MEMMODEL_SEQ_CST;

   if (!mtcsOptionsItem->x_flag_inline_atomics)
      return NULL_RTX;
   /* Expand the operand.  */
   rtx mem = get_builtin_sync_mem(self,CALL_EXPR_ARG (exp, 0), mode);
   return mtcs_optabs_expand_atomic_load/*!expand_atomic_load*/(mtcsOptabs,target, mem, model);
}

/* Expand the __atomic_store intrinsic:
      void __atomic_store (TYPE *object, TYPE desired, enum memmodel)
   EXP is the CALL_EXPR.
   TARGET is an optional place for us to store the results.  */
//原型 expand_builtin_atomic_store  butilins.cc
static rtx expand_builtin_atomic_store (MtcsBuiltins *self,machine_mode mode, tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

   memmodel model = get_memmodel(self,CALL_EXPR_ARG (exp, 2));
   if (!(is_mm_relaxed (model) || is_mm_seq_cst (model) || is_mm_release (model)))
      model = MEMMODEL_SEQ_CST;

   if (!mtcsOptionsItem->x_flag_inline_atomics)
      return NULL_RTX;

   /* Expand the operands.  */
   rtx mem = get_builtin_sync_mem(self,CALL_EXPR_ARG (exp, 0), mode);
   rtx val = expand_expr_force_mode(self,CALL_EXPR_ARG (exp, 1), mode);

   return mtcs_optabs_expand_atomic_store/*!expand_atomic_store*/(mtcsOptabs,mem, val, model, false);
}


/* Expand the __atomic_fetch_XXX intrinsic:
      TYPE __atomic_fetch_XXX (TYPE *object, TYPE val, enum memmodel)
   EXP is the CALL_EXPR.
   TARGET is an optional place for us to store the results.
   CODE is the operation, PLUS, MINUS, ADD, XOR, or IOR.
   FETCH_AFTER is true if returning the result of the operation.
   FETCH_AFTER is false if returning the value before the operation.
   IGNORE is true if the result is not used.
   EXT_CALL is the correct builtin for an external call if this cannot be
   resolved to an instruction sequence.  */
//原型 expand_builtin_atomic_fetch_op  butilins.cc
static rtx expand_builtin_atomic_fetch_op (MtcsBuiltins *self,machine_mode mode, tree exp, rtx target,
            enum rtx_code code, bool fetch_after,   bool ignore, enum built_in_function ext_call)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

   rtx val, mem, ret;
   enum memmodel model;
   tree fndecl;
   tree addr;

   model = get_memmodel(self,CALL_EXPR_ARG (exp, 2));

   /* Expand the operands.  */
   mem = get_builtin_sync_mem(self,CALL_EXPR_ARG (exp, 0), mode);
   val = expand_expr_force_mode(self,CALL_EXPR_ARG (exp, 1), mode);
   n_debug("mtcsbuiltins.c expand_builtin_atomic_fetch_op 00 model:%d mode:%d code:%d fetch_after:%d ignore:%d :%d\n",
         model,mode,code,fetch_after,ignore,ext_call);
   mtcs_print_rtl_single(stderr,target);
   aet_print_tree(exp);
   mtcs_print_rtl_single(stderr,mem);
   mtcs_print_rtl_single(stderr,val);

   /* Only try generating instructions if inlining is turned on.  */
   if (mtcsOptionsItem->x_flag_inline_atomics){
      n_debug("mtcsbuiltins.c expand_builtin_atomic_fetch_op 11\n");
      ret = mtcs_optabs_expand_atomic_fetch_op/*!expand_atomic_fetch_op*/(mtcsOptabs,
            target, mem, val, code, model, fetch_after);
      if (ret)
         return ret;
   }
   /* Return if a different routine isn't needed for the library call.  */
   if (ext_call == BUILT_IN_NONE)
      return NULL_RTX;

   /* Change the call to the specified function.  */
   fndecl = get_callee_fndecl (exp);
   addr = CALL_EXPR_FN (exp);
   STRIP_NOPS (addr);

   gcc_assert (TREE_OPERAND (addr, 0) == fndecl);
   TREE_OPERAND (addr, 0) = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,ext_call);
   /* If we will emit code after the call, the call cannot be a tail call.
   If it is emitted as a tail call, a barrier is emitted after it, and
   then all trailing code is removed.  */
   if (!ignore)
      CALL_EXPR_TAILCALL (exp) = 0;
   /* Expand the call here so we can emit trailing code.  */
   ret = mtcs_calls_expand_call/*!expand_call*/(mtcsCalls,exp, target, ignore);
   /* Replace the original function just in case it matters.  */
   TREE_OPERAND (addr, 0) = fndecl;
   /* Then issue the arithmetic correction to return the right result.  */
   if (!ignore) {
      if (code == NOT){
         ret = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
               mode, AND, ret, val, NULL_RTX, true,OPTAB_LIB_WIDEN);
         ret = mtcs_optabs_expand_simple_unop/*!expand_simple_unop*/(mtcsOptabs,
               mode, NOT, ret, target, true);
      }else
         ret = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
               mode, code, ret, val, target, true,OPTAB_LIB_WIDEN);
   }
   return ret;
}

/* Expand an atomic test_and_set operation.
   bool _atomic_test_and_set (BOOL *obj, enum memmodel)
   EXP is the call expression.  */
//原型 expand_builtin_atomic_test_and_set  butilins.cc
static rtx expand_builtin_atomic_test_and_set (MtcsBuiltins *self,tree exp, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   rtx mem;
   enum memmodel model;
   machine_mode mode;
   mode = mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,BOOL_TYPE_SIZE, 0).require ();
   mem = get_builtin_sync_mem(self,CALL_EXPR_ARG (exp, 0), mode);
   model = get_memmodel(self,CALL_EXPR_ARG (exp, 1));
   return mtcs_optabs_expand_atomic_test_and_set/*!expand_atomic_test_and_set*/(mtcsOptabs,target, mem, model);
}

/* Expand an atomic clear operation.
   void _atomic_clear (BOOL *obj, enum memmodel)
   EXP is the call expression.  */
//原型 expand_builtin_atomic_clear  butilins.cc
static rtx expand_builtin_atomic_clear (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

   machine_mode mode = mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,BOOL_TYPE_SIZE, 0).require ();
   rtx mem = get_builtin_sync_mem(self,CALL_EXPR_ARG (exp, 0), mode);
   memmodel model = get_memmodel(self,CALL_EXPR_ARG (exp, 1));

   if (is_mm_consume (model) || is_mm_acquire (model) || is_mm_acq_rel (model))
      model = MEMMODEL_SEQ_CST;

   /* Try issuing an __atomic_store, and allow fallback to __sync_lock_release.
   Failing that, a store is issued by __atomic_store.  The only way this can
   fail is if the bool type is larger than a word size.  Unlikely, but
   handle it anyway for completeness.  Assume a single threaded model since
   there is no atomic support in this case, and no barriers are required.  */
   rtx ret = mtcs_optabs_expand_atomic_store/*!expand_atomic_store*/(mtcsOptabs,mem, const0_rtx, model, true);
   if (!ret)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mem, const0_rtx);
   return const0_rtx;
}

/* Return true if (optional) argument ARG1 of size ARG0 is always lock free on
   this architecture.  If ARG1 is NULL, use typical alignment for size ARG0.  */
//原型 fold_builtin_atomic_always_lock_free  butilins.cc
static tree fold_builtin_atomic_always_lock_free (MtcsBuiltins *self,tree arg0, tree arg1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

   int size;
   machine_mode mode;
   unsigned int mode_align, type_align;

   if (TREE_CODE (arg0) != INTEGER_CST)
      return NULL_TREE;

   /* We need a corresponding integer mode for the access to be lock-free.  */
   size = INTVAL (mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg0)) * BITS_PER_UNIT;
   if (!mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,size, 0).exists (&mode))
      return boolean_false_node;

   mode_align =mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode);

   if (TREE_CODE (arg1) == INTEGER_CST) {
      unsigned HOST_WIDE_INT val = UINTVAL (mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg1));

      /* Either this argument is null, or it's a fake pointer encoding
      the alignment of the object.  */
      val = least_bit_hwi (val);
      val *= BITS_PER_UNIT;

      if (val == 0 || mode_align < val)
         type_align = mode_align;
      else
         type_align = val;
   }else{
      tree ttype = TREE_TYPE (arg1);

      /* This function is usually invoked and folded immediately by the front
      end before anything else has a chance to look at it.  The pointer
      parameter at this point is usually cast to a void *, so check for that
      and look past the cast.  */
      if (CONVERT_EXPR_P (arg1)   && POINTER_TYPE_P (ttype)  && VOID_TYPE_P (TREE_TYPE (ttype))
            && POINTER_TYPE_P (TREE_TYPE (TREE_OPERAND (arg1, 0))))
         arg1 = TREE_OPERAND (arg1, 0);

      ttype = TREE_TYPE (arg1);
      gcc_assert (POINTER_TYPE_P (ttype));
      /* Get the underlying type of the object.  */
      ttype = TREE_TYPE (ttype);
      type_align = TYPE_ALIGN (ttype);
   }

   /* If the object has smaller alignment, the lock free routines cannot
   be used.  */
   if (type_align < mode_align)
      return boolean_false_node;

   /* Check if a compare_and_swap pattern exists for the mode which represents
   the required size.  The pattern is not allowed to fail, so the existence
   of the pattern indicates support is present.  Also require that an
   atomic load exists for the required size.  */
   if (mtcs_optabs_can_compare_and_swap_p/*!can_compare_and_swap_p*/(mtcsOptabs,mode, true)
     && mtcs_optabs_can_atomic_load_p/*!can_atomic_load_p*/(mtcsOptabs,mode))
      return boolean_true_node;
   else
      return boolean_false_node;
}


/* Return a one or zero if it can be determined that object ARG1 of size ARG
   is lock free on this architecture.  */
//原型 fold_builtin_atomic_is_lock_free  butilins.cc
static tree fold_builtin_atomic_is_lock_free (MtcsBuiltins *self,tree arg0, tree arg1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

   if (!mtcsOptionsItem->x_flag_inline_atomics)
      return NULL_TREE;
   /* If it isn't always lock free, don't generate a result.  */
   if (fold_builtin_atomic_always_lock_free(self,arg0, arg1) == boolean_true_node)
      return boolean_true_node;
   return NULL_TREE;
}

/* Return true if the parameters to call EXP represent an object which will
   always generate lock free instructions.  The first argument represents the
   size of the object, and the second parameter is a pointer to the object
   itself.  If NULL is passed for the object, then the result is based on
   typical alignment for an object of the specified size.  Otherwise return
   NULL*/
//原型 expand_builtin_atomic_is_lock_free  butilins.cc
static rtx expand_builtin_atomic_is_lock_free (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

   tree size;
   tree arg0 = CALL_EXPR_ARG (exp, 0);
   tree arg1 = CALL_EXPR_ARG (exp, 1);

   if (!INTEGRAL_TYPE_P (TREE_TYPE (arg0))){
      error ("non-integer argument 1 to %qs", "__atomic_is_lock_free");
      return NULL_RTX;
   }

   if (!mtcsOptionsItem->x_flag_inline_atomics)
      return NULL_RTX;

   /* If the value is known at compile time, return the RTX for it.  */
   size = fold_builtin_atomic_is_lock_free(self,arg0, arg1);
   if (size == boolean_true_node)
      return const1_rtx;

   return NULL_RTX;
}

/* Return true if the parameters to call EXP represent an object which will
   always generate lock free instructions.  The first argument represents the
   size of the object, and the second parameter is a pointer to the object
   itself.  If NULL is passed for the object, then the result is based on
   typical alignment for an object of the specified size.  Otherwise return
   false.  */
//原型 expand_builtin_atomic_always_lock_free  butilins.cc
static rtx expand_builtin_atomic_always_lock_free (MtcsBuiltins *self,tree exp)
{
   tree size;
   tree arg0 = CALL_EXPR_ARG (exp, 0);
   tree arg1 = CALL_EXPR_ARG (exp, 1);

   if (TREE_CODE (arg0) != INTEGER_CST) {
      error ("non-constant argument 1 to %qs", "__atomic_always_lock_free");
      return const0_rtx;
   }

   size = fold_builtin_atomic_always_lock_free(self,arg0, arg1);
   if (size == boolean_true_node)
      return const1_rtx;
   return const0_rtx;
}

/* Expand the __atomic_thread_fence intrinsic:
      void __atomic_thread_fence (enum memmodel)
   EXP is the CALL_EXPR.  */
//原型 expand_builtin_atomic_thread_fence  butilins.cc
static void expand_builtin_atomic_thread_fence (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

   enum memmodel model = get_memmodel(self,CALL_EXPR_ARG (exp, 0));
   mtcs_optabs_expand_mem_thread_fence/*!expand_mem_thread_fence*/(mtcsOptabs,model);
}

/* Expand the __atomic_signal_fence intrinsic:
      void __atomic_signal_fence (enum memmodel)
   EXP is the CALL_EXPR.  */
//原型 expand_builtin_atomic_signal_fence  butilins.cc
static void expand_builtin_atomic_signal_fence (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   enum memmodel model = get_memmodel(self,CALL_EXPR_ARG (exp, 0));
   mtcs_optabs_expand_mem_signal_fence/*!expand_mem_signal_fence*/(mtcsOptabs,model);
}

/* Expand a call EXP to __builtin_object_size.  */
//原型 expand_builtin_object_size  butilins.cc
static rtx expand_builtin_object_size (MtcsBuiltins *self,tree exp)
{

   tree ost;
   int object_size_type;
   tree fndecl = get_callee_fndecl (exp);

   if (!validate_arglist (exp, POINTER_TYPE, INTEGER_TYPE, VOID_TYPE)){
      error ("first argument of %qD must be a pointer, second integer constant",fndecl);
      mtcs_builtins_expand_builtin_trap/*!expand_builtin_trap*/(self);
      return const0_rtx;
   }

   ost = CALL_EXPR_ARG (exp, 1);
   STRIP_NOPS (ost);

   if (TREE_CODE (ost) != INTEGER_CST || tree_int_cst_sgn (ost) < 0 || compare_tree_int (ost, 3) > 0){
      error ("last argument of %qD is not integer constant between 0 and 3",fndecl);
      mtcs_builtins_expand_builtin_trap/*!expand_builtin_trap*/(self);
      return const0_rtx;
   }

   object_size_type = tree_to_shwi (ost);

   return object_size_type < 2 ? constm1_rtx : const0_rtx;
}

/* Expand EXP, a call to the __mem{cpy,pcpy,move,set}_chk builtin.
   FCODE is the BUILT_IN_* to use.
   Return NULL_RTX if we failed; the caller should emit a normal call,
   otherwise try to get the result in TARGET, if convenient (and in
   mode MODE if that's convenient).  */
//原型 expand_builtin_memory_chk  butilins.cc
static rtx expand_builtin_memory_chk (MtcsBuiltins *self,tree exp, rtx target, machine_mode mode,
            enum built_in_function fcode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   if (!validate_arglist (exp, POINTER_TYPE, fcode == BUILT_IN_MEMSET_CHK
         ? INTEGER_TYPE : POINTER_TYPE,   INTEGER_TYPE, INTEGER_TYPE, VOID_TYPE))
      return NULL_RTX;

   tree dest = CALL_EXPR_ARG (exp, 0);
   tree src = CALL_EXPR_ARG (exp, 1);
   tree len = CALL_EXPR_ARG (exp, 2);
   tree size = CALL_EXPR_ARG (exp, 3);

   /* FIXME: Set access mode to write only for memset et al.  */
   bool sizes_ok = check_access (exp, len, /*maxread=*/NULL_TREE, /*srcstr=*/NULL_TREE, size, access_read_write);

   if (!tree_fits_uhwi_p (size))
      return NULL_RTX;

   if (tree_fits_uhwi_p (len) || integer_all_onesp (size)){
      /* Avoid transforming the checking call to an ordinary one when
      an overflow has been detected or when the call couldn't be
      validated because the size is not constant.  */
      if (!sizes_ok && !integer_all_onesp (size) && tree_int_cst_lt (size, len))
         return NULL_RTX;

      tree fn = NULL_TREE;
      /* If __builtin_mem{cpy,pcpy,move,set}_chk is used, assume
      mem{cpy,pcpy,move,set} is available.  */
      switch (fcode){
         case BUILT_IN_MEMCPY_CHK:
            fn = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_MEMCPY);
            break;
         case BUILT_IN_MEMPCPY_CHK:
            fn = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_MEMPCPY);
            break;
         case BUILT_IN_MEMMOVE_CHK:
            fn = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_MEMMOVE);
            break;
         case BUILT_IN_MEMSET_CHK:
            fn = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_MEMSET);
            break;
         default:
            break;
      }

      if (! fn)
         return NULL_RTX;

      fn = build_call_nofold_loc(self,EXPR_LOCATION (exp), fn, 3, dest, src, len);
      gcc_assert (TREE_CODE (fn) == CALL_EXPR);
      CALL_EXPR_TAILCALL (fn) = CALL_EXPR_TAILCALL (exp);
      return mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,fn, target, mode, EXPAND_NORMAL);
   }else if (fcode == BUILT_IN_MEMSET_CHK)
      return NULL_RTX;
   else{
      unsigned int dest_align = get_pointer_alignment (dest);

      /* If DEST is not a pointer type, call the normal function.  */
      if (dest_align == 0)
         return NULL_RTX;

      /* If SRC and DEST are the same (and not volatile), do nothing.  */
      if (operand_equal_p (src, dest, 0)){
         tree expr;

         if (fcode != BUILT_IN_MEMPCPY_CHK){
            /* Evaluate and ignore LEN in case it has side-effects.  */
            mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,len, const0_rtx, VOIDmode, EXPAND_NORMAL);
            return mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,dest, target, mode, EXPAND_NORMAL);
         }

         expr = mtcs_const_build_pointer_plus/*!fold_build_pointer_plus*/(mtcsConst,dest, len);
         return mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,expr, target, mode, EXPAND_NORMAL);
      }

      /* __memmove_chk special case.  */
      if (fcode == BUILT_IN_MEMMOVE_CHK){
         unsigned int src_align = get_pointer_alignment (src);

         if (src_align == 0)
            return NULL_RTX;

         /* If src is categorized for a readonly section we can use
         normal __memcpy_chk.  */
         if (readonly_data_expr (src)){
            tree fn = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_MEMCPY_CHK);
            if (!fn)
               return NULL_RTX;

            fn = build_call_nofold_loc(self,EXPR_LOCATION (exp), fn, 4,dest, src, len, size);
            gcc_assert (TREE_CODE (fn) == CALL_EXPR);
            CALL_EXPR_TAILCALL (fn) = CALL_EXPR_TAILCALL (exp);

            return mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,fn, target, mode, EXPAND_NORMAL);
         }
      }
      return NULL_RTX;
   }
}


/* Helper to check the sizes of sequences and the destination of calls
   to __builtin_strncat and __builtin___strncat_chk.  Returns true on
   success (no overflow or invalid sizes), false otherwise.  */
//原型 check_strncat_sizes builtins.cc
static bool check_strncat_sizes (MtcsBuiltins *self,tree exp, tree objsize)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

   tree dest = CALL_EXPR_ARG (exp, 0);
   tree src = CALL_EXPR_ARG (exp, 1);
   tree maxread = CALL_EXPR_ARG (exp, 2);

   /* Try to determine the range of lengths that the source expression
   refers to.  */
   c_strlen_data lendata = { };
   get_range_strlen (src, &lendata, /* eltsize = */ 1);

   /* Try to verify that the destination is big enough for the shortest
   string.  */

   access_data data (nullptr, exp, access_read_write, maxread, true);
   if (!objsize && mtcsOptionsItem->x_warn_stringop_overflow){
      /* If it hasn't been provided by __strncat_chk, try to determine
      the size of the destination object into which the source is
      being copied.  */
      objsize = compute_objsize (dest, warn_stringop_overflow - 1, &data.dst);
   }

   /* Add one for the terminating nul.  */
   tree srclen = (lendata.minlen  ? mtcs_const_fold_build2(mtcsConst,PLUS_EXPR, size_type_node, lendata.minlen,
   size_one_node) : NULL_TREE);

   /* The strncat function copies at most MAXREAD bytes and always appends
   the terminating nul so the specified upper bound should never be equal
   to (or greater than) the size of the destination.  */
   if (tree_fits_uhwi_p (maxread) && tree_fits_uhwi_p (objsize)
     && tree_int_cst_equal (objsize, maxread)) {
      location_t loc = EXPR_LOCATION (exp);
      warning_at (loc, OPT_Wstringop_overflow_,   "%qD specified bound %E equals destination size",
      get_callee_fndecl (exp), maxread);

      return false;
   }

   if (!srclen
   || (maxread && tree_fits_uhwi_p (maxread)
   && tree_fits_uhwi_p (srclen)
   && tree_int_cst_lt (maxread, srclen)))
      srclen = maxread;

   /* The number of bytes to write is LEN but check_access will alsoa
   check SRCLEN if LEN's value isn't known.  */
   return check_access (exp, /*dstwrite=*/NULL_TREE, maxread, srclen,
   objsize, data.mode, &data);
}


/* Emit warning if a buffer overflow is detected at compile time.  */
//原型 maybe_emit_chk_warning  butilins.cc
static void maybe_emit_chk_warning (MtcsBuiltins *self,tree exp, enum built_in_function fcode)
{
   /* The source string.  */
   tree srcstr = NULL_TREE;
   /* The size of the destination object returned by __builtin_object_size.  */
   tree objsize = NULL_TREE;
   /* The string that is being concatenated with (as in __strcat_chk)
   or null if it isn't.  */
   tree catstr = NULL_TREE;
   /* The maximum length of the source sequence in a bounded operation
   (such as __strncat_chk) or null if the operation isn't bounded
   (such as __strcat_chk).  */
   tree maxread = NULL_TREE;
   /* The exact size of the access (such as in __strncpy_chk).  */
   tree size = NULL_TREE;
   /* The access by the function that's checked.  Except for snprintf
   both writing and reading is checked.  */
   access_mode mode = access_read_write;

   switch (fcode) {
      case BUILT_IN_STRCPY_CHK:
      case BUILT_IN_STPCPY_CHK:
         srcstr = CALL_EXPR_ARG (exp, 1);
         objsize = CALL_EXPR_ARG (exp, 2);
         break;

      case BUILT_IN_STRCAT_CHK:
         /* For __strcat_chk the warning will be emitted only if overflowing
         by at least strlen (dest) + 1 bytes.  */
         catstr = CALL_EXPR_ARG (exp, 0);
         srcstr = CALL_EXPR_ARG (exp, 1);
         objsize = CALL_EXPR_ARG (exp, 2);
         break;

      case BUILT_IN_STRNCAT_CHK:
         catstr = CALL_EXPR_ARG (exp, 0);
         srcstr = CALL_EXPR_ARG (exp, 1);
         maxread = CALL_EXPR_ARG (exp, 2);
         objsize = CALL_EXPR_ARG (exp, 3);
         break;

      case BUILT_IN_STRNCPY_CHK:
      case BUILT_IN_STPNCPY_CHK:
         srcstr = CALL_EXPR_ARG (exp, 1);
         size = CALL_EXPR_ARG (exp, 2);
         objsize = CALL_EXPR_ARG (exp, 3);
         break;

      case BUILT_IN_SNPRINTF_CHK:
      case BUILT_IN_VSNPRINTF_CHK:
         maxread = CALL_EXPR_ARG (exp, 1);
         objsize = CALL_EXPR_ARG (exp, 3);
         /* The only checked access the write to the destination.  */
         mode = access_write_only;
         break;
      default:
         gcc_unreachable ();
   }

   if (catstr && maxread){
      /* Check __strncat_chk.  There is no way to determine the length
      of the string to which the source string is being appended so
      just warn when the length of the source string is not known.  */
      check_strncat_sizes(self,exp, objsize);
      return;
   }

   check_access (exp, size, maxread, srcstr, objsize, mode);
}

/* Emit warning if a buffer overflow is detected at compile time
   in __sprintf_chk/__vsprintf_chk calls.  */
//原型 maybe_emit_sprintf_chk_warning  butilins.cc
static void maybe_emit_sprintf_chk_warning (MtcsBuiltins *self,tree exp, enum built_in_function fcode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree size, len, fmt;
   const char *fmt_str;
   int nargs = call_expr_nargs (exp);
   /* Verify the required arguments in the original call.  */
   if (nargs < 4)
      return;
   size = CALL_EXPR_ARG (exp, 2);
   fmt = CALL_EXPR_ARG (exp, 3);
   if (! tree_fits_uhwi_p (size) || integer_all_onesp (size))
      return;

   /* Check whether the format is a literal string constant.  */
   fmt_str = c_getstr (fmt);
   if (fmt_str == NULL)
      return;

   if (!init_target_chars ())
      return;

   /* If the format doesn't contain % args or %%, we know its size.  */
   if (strchr (fmt_str, target_percent) == 0)
      len = build_int_cstu (size_type_node, strlen (fmt_str));
   /* If the format is "%s" and first ... argument is a string literal,
   we know it too.  */
   else if (fcode == BUILT_IN_SPRINTF_CHK  && strcmp (fmt_str, target_percent_s) == 0){
      tree arg;

      if (nargs < 5)
         return;
      arg = CALL_EXPR_ARG (exp, 4);
      if (! POINTER_TYPE_P (TREE_TYPE (arg)))
         return;

      len = c_strlen (arg, 1);
      if (!len || ! tree_fits_uhwi_p (len))
         return;
   } else
      return;

   /* Add one for the terminating nul.  */
   len = mtcs_const_fold_build2(mtcsConst,PLUS_EXPR, TREE_TYPE (len), len, size_one_node);

   check_access (exp, /*size=*/NULL_TREE, /*maxread=*/NULL_TREE, len, size, access_write_only);
}

//原型 expand_builtin_thread_pointer  butilins.cc
static rtx expand_builtin_thread_pointer (MtcsBuiltins *self,tree exp, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);

   enum insn_code icode;
   if (!validate_arglist (exp, VOID_TYPE))
      return const0_rtx;
   icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,get_thread_pointer_optab, pMode);
   if (icode != CODE_FOR_nothing){
      class expand_operand op;
      /* If the target is not sutitable then create a new target. */
      if (target == NULL_RTX || !REG_P (target) || GET_MODE (target) != pMode)
         target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,pMode);
      create_output_operand (&op, target, pMode);
      mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,icode, 1, &op);
      return target;
   }
   error ("%<__builtin_thread_pointer%> is not supported on this target");
   return const0_rtx;
}

//原型 expand_builtin_set_thread_pointer  butilins.cc
static void expand_builtin_set_thread_pointer (MtcsBuiltins *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);

   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);

   enum insn_code icode;
   if (!validate_arglist (exp, POINTER_TYPE, VOID_TYPE))
      return;
   icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,set_thread_pointer_optab, pMode);
   if (icode != CODE_FOR_nothing){
      class expand_operand op;
      rtx val = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,
            CALL_EXPR_ARG (exp, 0), NULL_RTX,pMode, EXPAND_NORMAL);
      create_input_operand (&op, val, pMode);
      mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,icode, 1, &op);
      return;
   }
   error ("%<__builtin_set_thread_pointer%> is not supported on this target");
}

/* Expand a call to __builtin_speculation_safe_value_<N>.  MODE
   represents the size of the first argument to that call, or VOIDmode
   if the argument is a pointer.  IGNORE will be true if the result
   isn't used.  */
//原型 expand_speculation_safe_value  butilins.cc
static rtx expand_speculation_safe_value (MtcsBuiltins *self,machine_mode mode, tree exp, rtx target,
                bool ignore)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   rtx val, failsafe;
   unsigned nargs = call_expr_nargs (exp);
   tree arg0 = CALL_EXPR_ARG (exp, 0);
   if (mode == VOIDmode){
      mode = TYPE_MODE (TREE_TYPE (arg0));
      gcc_assert (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) == MODE_INT);
   }
   val = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,arg0, NULL_RTX, mode, EXPAND_NORMAL);
   /* An optional second argument can be used as a failsafe value on
   some machines.  If it isn't present, then the failsafe value is
   assumed to be 0.  */
   if (nargs > 1){
      tree arg1 = CALL_EXPR_ARG (exp, 1);
      failsafe = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,arg1, NULL_RTX, mode, EXPAND_NORMAL);
   }else
      failsafe = const0_rtx;
   /* If the result isn't used, the behavior is undefined.  It would be
   nice to emit a warning here, but path splitting means this might
   happen with legitimate code.  So simply drop the builtin
   expansion in that case; we've handled any side-effects above.  */
   if (ignore)
      return const0_rtx;
   /* If we don't have a suitable target, create one to hold the result.  */
   if (target == NULL || GET_MODE (target) != mode)
      target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

   if (GET_MODE (val) != mode && GET_MODE (val) != VOIDmode)
      val = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,mode, VOIDmode, val, false);

   return mtcsTarget/*!targetm.speculation_safe_value*/->speculation_safe_value(mtcsTarget,
                     mode, target, val, failsafe);
}

/* Expand the __sync_synchronize intrinsic.  */
//原型 expand_builtin_sync_synchronize builtins.cc
static void expand_builtin_sync_synchronize (MtcsBuiltins *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   mtcs_optabs_expand_mem_thread_fence/*!expand_mem_thread_fence*/(mtcsOptabs,MEMMODEL_SYNC_SEQ_CST);
}

/* Expand an expression EXP that calls a built-in function,
   with result going to TARGET if that's convenient
   (and in mode MODE if that's convenient).
   SUBTARGET may be used as the target for computing one of EXP's operands.
   IGNORE is nonzero if the value is to be ignored.  */
//原型 expand_builtin builtins.h builtins.cc
rtx mtcs_builtins_expand_builtin (MtcsBuiltins *self,tree exp, rtx target, rtx subtarget,
        machine_mode mode, int ignore)
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
   MtcsAsm *mtcsAsm =(MtcsAsm *)mtcs_target_get_asm(mtcsTarget);
   MtcsArgs  *mtcsArgs=mtcs_target_get_args(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsCgraph *mtcsCgraph=mtcs_target_get_cgraph(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsExcept *mtcsExcept=mtcs_target_get_except(mtcsTarget);
   MtcsStmt *mtcsStmt=mtcs_target_get_stmt(mtcsTarget);
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree fndecl = get_callee_fndecl (exp);
   machine_mode target_mode = TYPE_MODE (TREE_TYPE (exp));
   int flags;

   n_debug("mtcsbuiltins.c expand_builtin 00 mode:%d ignore:%d target:%p %s %s\n",
         mode,ignore,target,get_tree_code_name(TREE_CODE(exp)),IDENTIFIER_POINTER(DECL_NAME(fndecl)));

   if (DECL_BUILT_IN_CLASS (fndecl) == BUILT_IN_MD)
      return mtcsTarget/*!targetm.expand_builtin*/->expand_builtin(mtcsTarget,exp, target, subtarget, mode, ignore);

   /* When ASan is enabled, we don't want to expand some memory/string
   builtins and rely on libsanitizer's hooks.  This allows us to avoid
   redundant checks and be sure, that possible overflow will be detected
   by ASan.  */

   enum built_in_function fcode = DECL_FUNCTION_CODE (fndecl);
   n_debug("mtcsbuiltins.c expand_builtin 11 built_in_function:%d %d %d\n",
         fcode,param_asan_kernel_mem_intrinsic_prefix,sanitize_flags_p (SANITIZE_KERNEL_ADDRESS | SANITIZE_KERNEL_HWADDRESS));

   if (param_asan_kernel_mem_intrinsic_prefix  && sanitize_flags_p (SANITIZE_KERNEL_ADDRESS | SANITIZE_KERNEL_HWADDRESS))
      switch (fcode){
         rtx save_decl_rtl, ret;
         case BUILT_IN_MEMCPY:
         case BUILT_IN_MEMMOVE:
         case BUILT_IN_MEMSET:
            save_decl_rtl = mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,fndecl);
            mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,fndecl) = asan_memfn_rtl (fndecl);
            ret = mtcs_calls_expand_call/*!expand_call*/(mtcsCalls,exp, target, ignore);
            mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,fndecl) = save_decl_rtl;
            return ret;
         default:
            break;
      }

   n_debug("mtcsbuiltins.c expand_builtin 22 built_in_function:%d %d %d\n",
         fcode,sanitize_flags_p (SANITIZE_ADDRESS | SANITIZE_HWADDRESS),asan_intercepted_p (fcode));

   if (sanitize_flags_p (SANITIZE_ADDRESS | SANITIZE_HWADDRESS) && asan_intercepted_p (fcode))
      return mtcs_calls_expand_call/*!expand_call*/(mtcsCalls,exp, target, ignore);

   n_debug("mtcsbuiltins.c expand_builtin 33 built_in_function:\n");
   /* When not optimizing, generate calls to library functions for a certain
   set of builtins.  */
   if (!mtcsOptionsItem->x_optimize
   && !called_as_built_in (fndecl)
   && fcode != BUILT_IN_FORK
   && fcode != BUILT_IN_EXECL
   && fcode != BUILT_IN_EXECV
   && fcode != BUILT_IN_EXECLP
   && fcode != BUILT_IN_EXECLE
   && fcode != BUILT_IN_EXECVP
   && fcode != BUILT_IN_EXECVE
   && fcode != BUILT_IN_CLEAR_CACHE
   && !ALLOCA_FUNCTION_CODE_P (fcode)
   && fcode != BUILT_IN_FREE
   && (fcode != BUILT_IN_MEMSET
   || !(mtcsOptionsItem->x_flag_inline_stringops & ILSOP_MEMSET))
   && (fcode != BUILT_IN_MEMCPY
   || !(mtcsOptionsItem->x_flag_inline_stringops & ILSOP_MEMCPY))
   && (fcode != BUILT_IN_MEMMOVE
   || !(mtcsOptionsItem->x_flag_inline_stringops & ILSOP_MEMMOVE))
   && (fcode != BUILT_IN_MEMCMP
   || !(mtcsOptionsItem->x_flag_inline_stringops & ILSOP_MEMCMP)))
      return mtcs_calls_expand_call/*!expand_call*/(mtcsCalls,exp, target, ignore);

   n_debug("mtcsbuiltins.c expand_builtin 44 built_in_function:\n");
   /* The built-in function expanders test for target == const0_rtx
   to determine whether the function's result will be ignored.  */
   if (ignore)
      target = const0_rtx;

   /* If the result of a pure or const built-in function is ignored, and
   none of its arguments are volatile, we can avoid expanding the
   built-in call and just evaluate the arguments for side-effects.  */
   if (target == const0_rtx
         && ((flags = flags_from_decl_or_type (fndecl)) & (ECF_CONST | ECF_PURE))
         && !(flags & ECF_LOOPING_CONST_OR_PURE)){
      bool volatilep = false;
      tree arg;
      call_expr_arg_iterator iter;

      FOR_EACH_CALL_EXPR_ARG (arg, iter, exp)
         if (TREE_THIS_VOLATILE (arg)){
            volatilep = true;
            break;
         }

      if (! volatilep){
         FOR_EACH_CALL_EXPR_ARG (arg, iter, exp)
            mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,arg, const0_rtx, VOIDmode, EXPAND_NORMAL);
         return const0_rtx;
      }
   }
   n_debug("mtcsbuiltins.c expand_builtin 55 built_in_function:\n");

   switch (fcode){
      CASE_FLT_FN (BUILT_IN_FABS):
      CASE_FLT_FN_FLOATN_NX (BUILT_IN_FABS):
      case BUILT_IN_FABSD32:
      case BUILT_IN_FABSD64:
      case BUILT_IN_FABSD128:
         target = expand_builtin_fabs(self,exp, target, subtarget);
         if (target)
            return target;
         break;

      CASE_FLT_FN (BUILT_IN_COPYSIGN):
      CASE_FLT_FN_FLOATN_NX (BUILT_IN_COPYSIGN):
         target = expand_builtin_copysign(self,exp, target, subtarget);
         if (target)
            return target;
         break;

      /* Just do a normal library call if we were unable to fold
      the values.  */
      CASE_FLT_FN (BUILT_IN_CABS):
      CASE_FLT_FN_FLOATN_NX (BUILT_IN_CABS):
         break;

      CASE_FLT_FN (BUILT_IN_FMA):
      CASE_FLT_FN_FLOATN_NX (BUILT_IN_FMA):
         target = expand_builtin_mathfn_ternary(self,exp, target, subtarget);
         if (target)
            return target;
         break;

      CASE_FLT_FN (BUILT_IN_ILOGB):
         if (!mtcsOptionsItem->x_flag_unsafe_math_optimizations)
            break;
         gcc_fallthrough ();
      CASE_FLT_FN (BUILT_IN_ISINF):
      CASE_FLT_FN (BUILT_IN_FINITE):
      case BUILT_IN_ISFINITE:
      case BUILT_IN_ISNORMAL:
         target = expand_builtin_interclass_mathfn(self,exp, target);
         if (target)
            return target;
         break;

      case BUILT_IN_ISSIGNALING:
         target = expand_builtin_issignaling(self,exp, target);
         if (target)
            return target;
         break;

      CASE_FLT_FN (BUILT_IN_ICEIL):
      CASE_FLT_FN (BUILT_IN_LCEIL):
      CASE_FLT_FN (BUILT_IN_LLCEIL):
      CASE_FLT_FN (BUILT_IN_LFLOOR):
      CASE_FLT_FN (BUILT_IN_IFLOOR):
      CASE_FLT_FN (BUILT_IN_LLFLOOR):
         target = expand_builtin_int_roundingfn(self,exp, target);
         if (target)
            return target;
         break;

      CASE_FLT_FN (BUILT_IN_IRINT):
      CASE_FLT_FN (BUILT_IN_LRINT):
      CASE_FLT_FN (BUILT_IN_LLRINT):
      CASE_FLT_FN (BUILT_IN_IROUND):
      CASE_FLT_FN (BUILT_IN_LROUND):
      CASE_FLT_FN (BUILT_IN_LLROUND):
         target = expand_builtin_int_roundingfn_2(self,exp, target);
         if (target)
            return target;
         break;

      CASE_FLT_FN (BUILT_IN_POWI):
         target = expand_builtin_powi(self,exp, target);
         if (target)
            return target;
         break;

      CASE_FLT_FN (BUILT_IN_CEXPI):
         target = expand_builtin_cexpi(self,exp, target);
         gcc_assert (target);
         return target;

      CASE_FLT_FN (BUILT_IN_SIN):
      CASE_FLT_FN (BUILT_IN_COS):
         if (! mtcsOptionsItem->x_flag_unsafe_math_optimizations)
            break;
         target = expand_builtin_mathfn_3(self,exp, target, subtarget);
         if (target)
            return target;
         break;

      CASE_FLT_FN (BUILT_IN_SINCOS):
         if (!mtcsOptionsItem->x_flag_unsafe_math_optimizations)
            break;
         target = expand_builtin_sincos(self,exp);//从expand_builtin_sincos开始集中替换
         if (target)
            return target;
         break;

      case BUILT_IN_FEGETROUND:
         target = expand_builtin_fegetround(self,exp, target, target_mode);
         if (target)
            return target;
         break;

      case BUILT_IN_FECLEAREXCEPT:
         target = expand_builtin_feclear_feraise_except(self,exp,
               target, target_mode,feclearexcept_optab);
         if (target)
            return target;
         break;

      case BUILT_IN_FERAISEEXCEPT:
         target = expand_builtin_feclear_feraise_except(self,exp,
               target, target_mode,feraiseexcept_optab);
         if (target)
            return target;
         break;

      case BUILT_IN_APPLY_ARGS:
         return expand_builtin_apply_args(self);

      /* __builtin_apply (FUNCTION, ARGUMENTS, ARGSIZE) invokes
      FUNCTION with a copy of the parameters described by
      ARGUMENTS, and ARGSIZE.  It returns a block of memory
      allocated on the stack into which is stored all the registers
      that might possibly be used for returning the result of a
      function.  ARGUMENTS is the value returned by
      __builtin_apply_args.  ARGSIZE is the number of bytes of
      arguments that must be copied.  ??? How should this value be
      computed?  We'll also need a safe worst case value for varargs
      functions.  */
      case BUILT_IN_APPLY:
         if (!validate_arglist (exp, POINTER_TYPE,POINTER_TYPE, INTEGER_TYPE, VOID_TYPE)
          && !validate_arglist (exp, REFERENCE_TYPE,POINTER_TYPE, INTEGER_TYPE, VOID_TYPE))
            return const0_rtx;
         else{
            rtx ops[3];
            ops[0] = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,CALL_EXPR_ARG (exp, 0));
            ops[1] = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,CALL_EXPR_ARG (exp, 1));
            ops[2] = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,CALL_EXPR_ARG (exp, 2));
            return expand_builtin_apply(self,ops[0], ops[1], ops[2]);
         }

      /* __builtin_return (RESULT) causes the function to return the
      value described by RESULT.  RESULT is address of the block of
      memory returned by __builtin_apply.  */
      case BUILT_IN_RETURN:
         if (validate_arglist (exp, POINTER_TYPE, VOID_TYPE))
            expand_builtin_return(self,mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,CALL_EXPR_ARG (exp, 0)));
         return const0_rtx;

      case BUILT_IN_SAVEREGS:
         return mtcs_builtins_expand_builtin_saveregs/*!expand_builtin_saveregs*/(self);

      case BUILT_IN_VA_ARG_PACK:
         /* All valid uses of __builtin_va_arg_pack () are removed during
         inlining.  */
         error ("invalid use of %<__builtin_va_arg_pack ()%>");
         return const0_rtx;

      case BUILT_IN_VA_ARG_PACK_LEN:
         /* All valid uses of __builtin_va_arg_pack_len () are removed during
         inlining.  */
         error ("invalid use of %<__builtin_va_arg_pack_len ()%>");
         return const0_rtx;

      /* Return the address of the first anonymous stack arg.  */
      case BUILT_IN_NEXT_ARG:
         if (fold_builtin_next_arg (exp, false))
            return const0_rtx;
         return expand_builtin_next_arg(self);

      case BUILT_IN_CLEAR_CACHE:
         expand_builtin___clear_cache(self,exp);
         return const0_rtx;

      case BUILT_IN_CLASSIFY_TYPE:
         return expand_builtin_classify_type(self,exp);

      case BUILT_IN_CONSTANT_P:
         return const0_rtx;

      case BUILT_IN_FRAME_ADDRESS:
      case BUILT_IN_RETURN_ADDRESS:
         return expand_builtin_frame_address(self,fndecl, exp);

      case BUILT_IN_STACK_ADDRESS:
         return expand_builtin_stack_address(self);

      case BUILT_IN___STRUB_ENTER:
         target = expand_builtin_strub_enter(self,exp);
         if (target)
            return target;
         break;

      case BUILT_IN___STRUB_UPDATE:
         target = expand_builtin_strub_update(self,exp);
         if (target)
            return target;
         break;

      case BUILT_IN___STRUB_LEAVE:
         target = expand_builtin_strub_leave(self,exp);
         if (target)
            return target;
         break;

      /* Returns the address of the area where the structure is returned.
      0 otherwise.  */
      case BUILT_IN_AGGREGATE_INCOMING_ADDRESS:
         if (call_expr_nargs (exp) != 0
         || ! AGGREGATE_TYPE_P (TREE_TYPE (TREE_TYPE (current_function_decl)))
         || !MEM_P (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,DECL_RESULT (current_function_decl))))
            return const0_rtx;
         else
            return XEXP (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,DECL_RESULT (current_function_decl)), 0);

      CASE_BUILT_IN_ALLOCA:
         target = expand_builtin_alloca(self,exp);
         if (target)
            return target;
         break;

      case BUILT_IN_ASAN_ALLOCAS_UNPOISON:
         return expand_asan_emit_allocas_unpoison(self,exp);

      case BUILT_IN_STACK_SAVE:
         return expand_stack_save(self);

      case BUILT_IN_STACK_RESTORE:
         expand_stack_restore(self,CALL_EXPR_ARG (exp, 0));
         return const0_rtx;

      case BUILT_IN_BSWAP16:
      case BUILT_IN_BSWAP32:
      case BUILT_IN_BSWAP64:
      case BUILT_IN_BSWAP128:
         target = expand_builtin_bswap(self,target_mode, exp, target, subtarget);
         if (target)
            return target;
         break;

      CASE_INT_FN (BUILT_IN_FFS):
         target = expand_builtin_unop(self,target_mode, exp, target,subtarget, ffs_optab);
         if (target)
            return target;
         break;

      CASE_INT_FN (BUILT_IN_CLZ):
         target = expand_builtin_unop(self,target_mode, exp, target,subtarget, clz_optab);
         if (target)
            return target;
         break;

      CASE_INT_FN (BUILT_IN_CTZ):
         target = expand_builtin_unop(self,target_mode, exp, target,subtarget, ctz_optab);
         if (target)
            return target;
         break;

      CASE_INT_FN (BUILT_IN_CLRSB):
         target = expand_builtin_unop(self,target_mode, exp, target,subtarget, clrsb_optab);
         if (target)
            return target;
         break;

      CASE_INT_FN (BUILT_IN_POPCOUNT):
         target = expand_builtin_unop(self,target_mode, exp, target,subtarget, popcount_optab);
         if (target)
            return target;
         break;

      CASE_INT_FN (BUILT_IN_PARITY):
         target = expand_builtin_unop(self,target_mode, exp, target,subtarget, parity_optab);
         if (target)
            return target;
         break;

      case BUILT_IN_STRLEN:
         target = expand_builtin_strlen(self,exp, target, target_mode);
         if (target)
            return target;
         break;

      case BUILT_IN_STRNLEN:
         target = expand_builtin_strnlen(self,exp, target, target_mode);
         if (target)
            return target;
         break;

      case BUILT_IN_STRCPY:
         target = expand_builtin_strcpy(self,exp, target);
         if (target)
            return target;
         break;

      case BUILT_IN_STRNCPY:
         target = expand_builtin_strncpy(self,exp, target);
         if (target)
            return target;
         break;

      case BUILT_IN_STPCPY:
         target = expand_builtin_stpcpy(self,exp, target, mode);
         if (target)
            return target;
         break;

      case BUILT_IN_MEMCPY:
         target = expand_builtin_memcpy(self,exp, target);
         if (target)
            return target;
         break;

      case BUILT_IN_MEMMOVE:
         target = expand_builtin_memmove(self,exp, target);
         if (target)
            return target;
         break;

      case BUILT_IN_MEMPCPY:
         target = expand_builtin_mempcpy(self,exp, target);
         if (target)
            return target;
         break;

      case BUILT_IN_MEMSET:
         target = mtcs_builtins_expand_builtin_memset/*!expand_builtin_memset*/(self,exp, target, mode);
         if (target)
            return target;
         break;

      case BUILT_IN_BZERO:
         target = expand_builtin_bzero(self,exp);
         if (target)
            return target;
         break;

      /* Expand it as BUILT_IN_MEMCMP_EQ first. If not successful, change it
      back to a BUILT_IN_STRCMP. Remember to delete the 3rd parameter
      when changing it to a strcmp call.  */
      case BUILT_IN_STRCMP_EQ:
         target = expand_builtin_memcmp(self,exp, target, true);
         if (target)
            return target;

         /* Change this call back to a BUILT_IN_STRCMP.  */
         TREE_OPERAND (exp, 1)= mtcs_const_build_fold_addr_expr/*!build_fold_addr_expr*/(mtcsConst,
               mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_STRCMP));

         /* Delete the last parameter.  */
         unsigned int i;
         vec<tree, va_gc> *arg_vec;
         vec_alloc (arg_vec, 2);
         for (i = 0; i < 2; i++)
            arg_vec->quick_push (CALL_EXPR_ARG (exp, i));
         exp = build_call_vec (TREE_TYPE (exp), CALL_EXPR_FN (exp), arg_vec);
         /* FALLTHROUGH */
      case BUILT_IN_STRCMP:
         target = expand_builtin_strcmp(self,exp, target);
         if (target)
            return target;
         break;
      /* Expand it as BUILT_IN_MEMCMP_EQ first. If not successful, change it
      back to a BUILT_IN_STRNCMP.  */
      case BUILT_IN_STRNCMP_EQ:
         target = expand_builtin_memcmp(self,exp, target, true);
         if (target)
            return target;
         /* Change it back to a BUILT_IN_STRNCMP.  */
         TREE_OPERAND (exp, 1)= mtcs_const_build_fold_addr_expr/*!build_fold_addr_expr*/(mtcsConst,
               mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_STRNCMP));
         /* FALLTHROUGH */

      case BUILT_IN_STRNCMP:
         target = expand_builtin_strncmp(self,exp, target, mode);
         if (target)
            return target;
         break;

      case BUILT_IN_BCMP:
      case BUILT_IN_MEMCMP:
      case BUILT_IN_MEMCMP_EQ:
         target = expand_builtin_memcmp(self,exp, target, fcode == BUILT_IN_MEMCMP_EQ);
         if (target)
            return target;
         if (fcode == BUILT_IN_MEMCMP_EQ){
            tree newdecl = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_MEMCMP);
            TREE_OPERAND (exp, 1) = mtcs_const_build_fold_addr_expr/*!build_fold_addr_expr*/(mtcsConst,newdecl);
         }
         break;

      case BUILT_IN_SETJMP:
         /* This should have been lowered to the builtins below.  */
         gcc_unreachable ();

      case BUILT_IN_SETJMP_SETUP:
         /* __builtin_setjmp_setup is passed a pointer to an array of five words
         and the receiver label.  */
         if (validate_arglist (exp, POINTER_TYPE, POINTER_TYPE, VOID_TYPE)){
            rtx buf_addr = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,CALL_EXPR_ARG (exp, 0), subtarget,
                  VOIDmode, EXPAND_NORMAL);
            tree label = TREE_OPERAND (CALL_EXPR_ARG (exp, 1), 0);
            rtx_insn *label_r = mtcs_stmt_label_rtx/*!label_rtx*/(mtcsStmt,label);
            mtcs_builtins_expand_builtin_setjmp_setup/*!expand_builtin_setjmp_setup*/(self,buf_addr, label_r);
            return const0_rtx;
         }
         break;

      case BUILT_IN_SETJMP_RECEIVER:
         /* __builtin_setjmp_receiver is passed the receiver label.  */
         if (validate_arglist (exp, POINTER_TYPE, VOID_TYPE)){
            tree label = TREE_OPERAND (CALL_EXPR_ARG (exp, 0), 0);
            rtx_insn *label_r = mtcs_stmt_label_rtx/*!label_rtx*/(mtcsStmt,label);

            mtcs_builtins_expand_builtin_setjmp_receiver/*!expand_builtin_setjmp_receiver*/(self,label_r);
            mtcsRtlData/*!nonlocal_goto_handler_labels*/->x_nonlocal_goto_handler_labels
              = gen_rtx_INSN_LIST (VOIDmode, label_r, mtcsRtlData/*!nonlocal_goto_handler_labels*/->x_nonlocal_goto_handler_labels);
            /* ??? Do not let expand_label treat us as such since we would
            not want to be both on the list of non-local labels and on
            the list of forced labels.  */
            FORCED_LABEL (label) = 0;
            return const0_rtx;
         }
         break;

      /* __builtin_longjmp is passed a pointer to an array of five words.
      It's similar to the C library longjmp function but works with
      __builtin_setjmp above.  */
      case BUILT_IN_LONGJMP:
         if (validate_arglist (exp, POINTER_TYPE, INTEGER_TYPE, VOID_TYPE)){
            rtx buf_addr = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,CALL_EXPR_ARG (exp, 0), subtarget,
                           VOIDmode, EXPAND_NORMAL);
            rtx value = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,CALL_EXPR_ARG (exp, 1));

            if (value != const1_rtx){
               error ("%<__builtin_longjmp%> second argument must be 1");
               return const0_rtx;
            }
            expand_builtin_longjmp(self,buf_addr, value);
            return const0_rtx;
         }
         break;

      case BUILT_IN_NONLOCAL_GOTO:
         target = expand_builtin_nonlocal_goto(self,exp);
         if (target)
            return target;
         break;

      /* This updates the setjmp buffer that is its argument with the value
      of the current stack pointer.  */
      case BUILT_IN_UPDATE_SETJMP_BUF:
         if (validate_arglist (exp, POINTER_TYPE, VOID_TYPE)){
            rtx buf_addr = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,CALL_EXPR_ARG (exp, 0));
            mtcs_builtins_expand_builtin_update_setjmp_buf/*!expand_builtin_update_setjmp_buf*/(self,buf_addr);
            return const0_rtx;
         }
         break;

      case BUILT_IN_TRAP:
      case BUILT_IN_UNREACHABLE_TRAP:
         mtcs_builtins_expand_builtin_trap/*!expand_builtin_trap*/(self);
         return const0_rtx;

      case BUILT_IN_UNREACHABLE:
         expand_builtin_unreachable(self);
         return const0_rtx;

      CASE_FLT_FN (BUILT_IN_SIGNBIT):
      case BUILT_IN_SIGNBITD32:
      case BUILT_IN_SIGNBITD64:
      case BUILT_IN_SIGNBITD128:
         target = expand_builtin_signbit(self,exp, target);
         if (target)
            return target;
         break;

      /* Various hooks for the DWARF 2 __throw routine.  */
      case BUILT_IN_UNWIND_INIT:
         mtcs_except_expand_builtin_unwind_init/*!expand_builtin_unwind_init*/(mtcsExcept);
         return const0_rtx;
      case BUILT_IN_DWARF_CFA:
         return mtcs_rtl_get_virtual_cfa_rtx/*!virtual_cfa_rtx*/(mtcsRTL);
         /*
      #ifdef DWARF2_UNWIND_INFO //host=1 nvptx=0
      case BUILT_IN_DWARF_SP_COLUMN:
         return expand_builtin_dwarf_sp_column ();
      case BUILT_IN_INIT_DWARF_REG_SIZES:
         expand_builtin_init_dwarf_reg_sizes (CALL_EXPR_ARG (exp, 0));
         return const0_rtx;
      #endif
         */
      case BUILT_IN_FROB_RETURN_ADDR:
         return mtcs_except_expand_builtin_frob_return_addr/*!expand_builtin_frob_return_addr*/
               (mtcsExcept,CALL_EXPR_ARG (exp, 0));
      case BUILT_IN_EXTRACT_RETURN_ADDR:
         return mtcs_except_expand_builtin_extract_return_addr/*!expand_builtin_extract_return_addr*/
               (mtcsExcept,CALL_EXPR_ARG (exp, 0));
      case BUILT_IN_EH_RETURN:
         mtcs_except_expand_builtin_eh_return/*!expand_builtin_eh_return*/
                (mtcsExcept,CALL_EXPR_ARG (exp, 0),CALL_EXPR_ARG (exp, 1));
         return const0_rtx;
      case BUILT_IN_EH_RETURN_DATA_REGNO:
         return mtcs_except_expand_builtin_eh_return_data_regno/*!expand_builtin_eh_return_data_regno*/
               (mtcsExcept,exp);
      case BUILT_IN_EXTEND_POINTER:
         return mtcs_except_expand_builtin_extend_pointer/*!expand_builtin_extend_pointer*/
               (mtcsExcept,CALL_EXPR_ARG (exp, 0));
      case BUILT_IN_EH_POINTER:
         return mtcs_except_expand_builtin_eh_pointer/*!expand_builtin_eh_pointer*/(mtcsExcept,exp);
      case BUILT_IN_EH_FILTER:
         return mtcs_except_expand_builtin_eh_filter/*!expand_builtin_eh_filter*/(mtcsExcept,exp);
      case BUILT_IN_EH_COPY_VALUES:
         return mtcs_except_expand_builtin_eh_copy_values/*!expand_builtin_eh_copy_values*/(mtcsExcept,exp);

      case BUILT_IN_VA_START:
         return expand_builtin_va_start(self,exp);
      case BUILT_IN_VA_END:
         return expand_builtin_va_end(self,exp);
      case BUILT_IN_VA_COPY:
         return expand_builtin_va_copy(self,exp);
      case BUILT_IN_EXPECT:
         return expand_builtin_expect(self,exp, target);
      case BUILT_IN_EXPECT_WITH_PROBABILITY:
         return expand_builtin_expect_with_probability(self,exp, target);
      case BUILT_IN_ASSUME_ALIGNED:
         return expand_builtin_assume_aligned(self,exp, target);
      case BUILT_IN_PREFETCH:
         expand_builtin_prefetch(self,exp);
         return const0_rtx;

      case BUILT_IN_INIT_TRAMPOLINE:
         return expand_builtin_init_trampoline(self,exp, true);
      case BUILT_IN_INIT_HEAP_TRAMPOLINE:
         return expand_builtin_init_trampoline(self,exp, false);
      case BUILT_IN_ADJUST_TRAMPOLINE:
         return expand_builtin_adjust_trampoline(self,exp);

      case BUILT_IN_INIT_DESCRIPTOR:
         return expand_builtin_init_descriptor(self,exp);
      case BUILT_IN_ADJUST_DESCRIPTOR:
         return expand_builtin_adjust_descriptor(self,exp);

      case BUILT_IN_GCC_NESTED_PTR_CREATED:
      case BUILT_IN_GCC_NESTED_PTR_DELETED:
         break; /* At present, no expansion, just call the function.  */

      case BUILT_IN_FORK:
      case BUILT_IN_EXECL:
      case BUILT_IN_EXECV:
      case BUILT_IN_EXECLP:
      case BUILT_IN_EXECLE:
      case BUILT_IN_EXECVP:
      case BUILT_IN_EXECVE:
         target = expand_builtin_fork_or_exec(self,fndecl, exp, target, ignore);
         if (target)
            return target;
         break;

      case BUILT_IN_SYNC_FETCH_AND_ADD_1:
      case BUILT_IN_SYNC_FETCH_AND_ADD_2:
      case BUILT_IN_SYNC_FETCH_AND_ADD_4:
      case BUILT_IN_SYNC_FETCH_AND_ADD_8:
      case BUILT_IN_SYNC_FETCH_AND_ADD_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_SYNC_FETCH_AND_ADD_1);
         target = expand_builtin_sync_operation(self,mode, exp, PLUS, false, target);
         if (target)
            return target;
         break;

      case BUILT_IN_SYNC_FETCH_AND_SUB_1:
      case BUILT_IN_SYNC_FETCH_AND_SUB_2:
      case BUILT_IN_SYNC_FETCH_AND_SUB_4:
      case BUILT_IN_SYNC_FETCH_AND_SUB_8:
      case BUILT_IN_SYNC_FETCH_AND_SUB_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_SYNC_FETCH_AND_SUB_1);
         target = expand_builtin_sync_operation(self,mode, exp, MINUS, false, target);
         if (target)
            return target;
         break;

      case BUILT_IN_SYNC_FETCH_AND_OR_1:
      case BUILT_IN_SYNC_FETCH_AND_OR_2:
      case BUILT_IN_SYNC_FETCH_AND_OR_4:
      case BUILT_IN_SYNC_FETCH_AND_OR_8:
      case BUILT_IN_SYNC_FETCH_AND_OR_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_SYNC_FETCH_AND_OR_1);
         target = expand_builtin_sync_operation(self,mode, exp, IOR, false, target);
         if (target)
            return target;
         break;

      case BUILT_IN_SYNC_FETCH_AND_AND_1:
      case BUILT_IN_SYNC_FETCH_AND_AND_2:
      case BUILT_IN_SYNC_FETCH_AND_AND_4:
      case BUILT_IN_SYNC_FETCH_AND_AND_8:
      case BUILT_IN_SYNC_FETCH_AND_AND_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_SYNC_FETCH_AND_AND_1);
         target = expand_builtin_sync_operation(self,mode, exp, AND, false, target);
         if (target)
            return target;
         break;

      case BUILT_IN_SYNC_FETCH_AND_XOR_1:
      case BUILT_IN_SYNC_FETCH_AND_XOR_2:
      case BUILT_IN_SYNC_FETCH_AND_XOR_4:
      case BUILT_IN_SYNC_FETCH_AND_XOR_8:
      case BUILT_IN_SYNC_FETCH_AND_XOR_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_SYNC_FETCH_AND_XOR_1);
         target = expand_builtin_sync_operation(self,mode, exp, XOR, false, target);
         if (target)
            return target;
         break;

      case BUILT_IN_SYNC_FETCH_AND_NAND_1:
      case BUILT_IN_SYNC_FETCH_AND_NAND_2:
      case BUILT_IN_SYNC_FETCH_AND_NAND_4:
      case BUILT_IN_SYNC_FETCH_AND_NAND_8:
      case BUILT_IN_SYNC_FETCH_AND_NAND_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_SYNC_FETCH_AND_NAND_1);
         target = expand_builtin_sync_operation(self,mode, exp, NOT, false, target);
         if (target)
            return target;
         break;

      case BUILT_IN_SYNC_ADD_AND_FETCH_1:
      case BUILT_IN_SYNC_ADD_AND_FETCH_2:
      case BUILT_IN_SYNC_ADD_AND_FETCH_4:
      case BUILT_IN_SYNC_ADD_AND_FETCH_8:
      case BUILT_IN_SYNC_ADD_AND_FETCH_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_SYNC_ADD_AND_FETCH_1);
         target = expand_builtin_sync_operation(self,mode, exp, PLUS, true, target);
         if (target)
            return target;
         break;

      case BUILT_IN_SYNC_SUB_AND_FETCH_1:
      case BUILT_IN_SYNC_SUB_AND_FETCH_2:
      case BUILT_IN_SYNC_SUB_AND_FETCH_4:
      case BUILT_IN_SYNC_SUB_AND_FETCH_8:
      case BUILT_IN_SYNC_SUB_AND_FETCH_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_SYNC_SUB_AND_FETCH_1);
         target = expand_builtin_sync_operation(self,mode, exp, MINUS, true, target);
         if (target)
            return target;
         break;

      case BUILT_IN_SYNC_OR_AND_FETCH_1:
      case BUILT_IN_SYNC_OR_AND_FETCH_2:
      case BUILT_IN_SYNC_OR_AND_FETCH_4:
      case BUILT_IN_SYNC_OR_AND_FETCH_8:
      case BUILT_IN_SYNC_OR_AND_FETCH_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_SYNC_OR_AND_FETCH_1);
         target = expand_builtin_sync_operation(self,mode, exp, IOR, true, target);
         if (target)
            return target;
         break;

      case BUILT_IN_SYNC_AND_AND_FETCH_1:
      case BUILT_IN_SYNC_AND_AND_FETCH_2:
      case BUILT_IN_SYNC_AND_AND_FETCH_4:
      case BUILT_IN_SYNC_AND_AND_FETCH_8:
      case BUILT_IN_SYNC_AND_AND_FETCH_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_SYNC_AND_AND_FETCH_1);
         target = expand_builtin_sync_operation(self,mode, exp, AND, true, target);
         if (target)
            return target;
         break;

      case BUILT_IN_SYNC_XOR_AND_FETCH_1:
      case BUILT_IN_SYNC_XOR_AND_FETCH_2:
      case BUILT_IN_SYNC_XOR_AND_FETCH_4:
      case BUILT_IN_SYNC_XOR_AND_FETCH_8:
      case BUILT_IN_SYNC_XOR_AND_FETCH_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_SYNC_XOR_AND_FETCH_1);
         target = expand_builtin_sync_operation(self,mode, exp, XOR, true, target);
         if (target)
            return target;
         break;

      case BUILT_IN_SYNC_NAND_AND_FETCH_1:
      case BUILT_IN_SYNC_NAND_AND_FETCH_2:
      case BUILT_IN_SYNC_NAND_AND_FETCH_4:
      case BUILT_IN_SYNC_NAND_AND_FETCH_8:
      case BUILT_IN_SYNC_NAND_AND_FETCH_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_SYNC_NAND_AND_FETCH_1);
         target = expand_builtin_sync_operation(self,mode, exp, NOT, true, target);
         if (target)
            return target;
         break;

      case BUILT_IN_SYNC_BOOL_COMPARE_AND_SWAP_1:
      case BUILT_IN_SYNC_BOOL_COMPARE_AND_SWAP_2:
      case BUILT_IN_SYNC_BOOL_COMPARE_AND_SWAP_4:
      case BUILT_IN_SYNC_BOOL_COMPARE_AND_SWAP_8:
      case BUILT_IN_SYNC_BOOL_COMPARE_AND_SWAP_16:
         if (mode == VOIDmode)
            mode = TYPE_MODE (boolean_type_node);
         if (!target || !register_operand (target, mode))
            target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_SYNC_BOOL_COMPARE_AND_SWAP_1);
         target = expand_builtin_compare_and_swap(self,mode, exp, true, target);
         if (target)
            return target;
         break;

      case BUILT_IN_SYNC_VAL_COMPARE_AND_SWAP_1:
      case BUILT_IN_SYNC_VAL_COMPARE_AND_SWAP_2:
      case BUILT_IN_SYNC_VAL_COMPARE_AND_SWAP_4:
      case BUILT_IN_SYNC_VAL_COMPARE_AND_SWAP_8:
      case BUILT_IN_SYNC_VAL_COMPARE_AND_SWAP_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_SYNC_VAL_COMPARE_AND_SWAP_1);
         target = expand_builtin_compare_and_swap(self,mode, exp, false, target);
         if (target)
            return target;
         break;

      case BUILT_IN_SYNC_LOCK_TEST_AND_SET_1:
      case BUILT_IN_SYNC_LOCK_TEST_AND_SET_2:
      case BUILT_IN_SYNC_LOCK_TEST_AND_SET_4:
      case BUILT_IN_SYNC_LOCK_TEST_AND_SET_8:
      case BUILT_IN_SYNC_LOCK_TEST_AND_SET_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_SYNC_LOCK_TEST_AND_SET_1);
         target = expand_builtin_sync_lock_test_and_set(self,mode, exp, target);
         if (target)
            return target;
         break;

      case BUILT_IN_SYNC_LOCK_RELEASE_1:
      case BUILT_IN_SYNC_LOCK_RELEASE_2:
      case BUILT_IN_SYNC_LOCK_RELEASE_4:
      case BUILT_IN_SYNC_LOCK_RELEASE_8:
      case BUILT_IN_SYNC_LOCK_RELEASE_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_SYNC_LOCK_RELEASE_1);
         expand_builtin_sync_lock_release(self,mode, exp);
         return const0_rtx;

      case BUILT_IN_SYNC_SYNCHRONIZE:
         expand_builtin_sync_synchronize(self);
         return const0_rtx;

      case BUILT_IN_ATOMIC_EXCHANGE_1:
      case BUILT_IN_ATOMIC_EXCHANGE_2:
      case BUILT_IN_ATOMIC_EXCHANGE_4:
      case BUILT_IN_ATOMIC_EXCHANGE_8:
      case BUILT_IN_ATOMIC_EXCHANGE_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_ATOMIC_EXCHANGE_1);
         target = expand_builtin_atomic_exchange(self,mode, exp, target);
         if (target)
            return target;
         break;

      case BUILT_IN_ATOMIC_COMPARE_EXCHANGE_1:
      case BUILT_IN_ATOMIC_COMPARE_EXCHANGE_2:
      case BUILT_IN_ATOMIC_COMPARE_EXCHANGE_4:
      case BUILT_IN_ATOMIC_COMPARE_EXCHANGE_8:
      case BUILT_IN_ATOMIC_COMPARE_EXCHANGE_16:
      {
         unsigned int nargs, z;
         vec<tree, va_gc> *vec;
         mode =get_builtin_sync_mode(self,fcode - BUILT_IN_ATOMIC_COMPARE_EXCHANGE_1);
         target = expand_builtin_atomic_compare_exchange(self,mode, exp, target);
         if (target)
            return target;
         /* If this is turned into an external library call, the weak parameter
         must be dropped to match the expected parameter list.  */
         nargs = call_expr_nargs (exp);
         vec_alloc (vec, nargs - 1);
         for (z = 0; z < 3; z++)
            vec->quick_push (CALL_EXPR_ARG (exp, z));
         /* Skip the boolean weak parameter.  */
         for (z = 4; z < 6; z++)
            vec->quick_push (CALL_EXPR_ARG (exp, z));
         exp = build_call_vec (TREE_TYPE (exp), CALL_EXPR_FN (exp), vec);
         break;
      }

      case BUILT_IN_ATOMIC_LOAD_1:
      case BUILT_IN_ATOMIC_LOAD_2:
      case BUILT_IN_ATOMIC_LOAD_4:
      case BUILT_IN_ATOMIC_LOAD_8:
      case BUILT_IN_ATOMIC_LOAD_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_ATOMIC_LOAD_1);
         target = expand_builtin_atomic_load(self,mode, exp, target);
         if (target)
            return target;
         break;

      case BUILT_IN_ATOMIC_STORE_1:
      case BUILT_IN_ATOMIC_STORE_2:
      case BUILT_IN_ATOMIC_STORE_4:
      case BUILT_IN_ATOMIC_STORE_8:
      case BUILT_IN_ATOMIC_STORE_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_ATOMIC_STORE_1);
         target = expand_builtin_atomic_store(self,mode, exp);
         if (target)
            return const0_rtx;
         break;

      case BUILT_IN_ATOMIC_ADD_FETCH_1:
      case BUILT_IN_ATOMIC_ADD_FETCH_2:
      case BUILT_IN_ATOMIC_ADD_FETCH_4:
      case BUILT_IN_ATOMIC_ADD_FETCH_8:
      case BUILT_IN_ATOMIC_ADD_FETCH_16:
      {
         enum built_in_function lib;
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_ATOMIC_ADD_FETCH_1);
         lib = (enum built_in_function)((int)BUILT_IN_ATOMIC_FETCH_ADD_1 +(fcode - BUILT_IN_ATOMIC_ADD_FETCH_1));
         target = expand_builtin_atomic_fetch_op(self,mode, exp, target, PLUS, true,ignore, lib);
         if (target)
            return target;
         break;
      }
      case BUILT_IN_ATOMIC_SUB_FETCH_1:
      case BUILT_IN_ATOMIC_SUB_FETCH_2:
      case BUILT_IN_ATOMIC_SUB_FETCH_4:
      case BUILT_IN_ATOMIC_SUB_FETCH_8:
      case BUILT_IN_ATOMIC_SUB_FETCH_16:
      {
         enum built_in_function lib;
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_ATOMIC_SUB_FETCH_1);
         lib = (enum built_in_function)((int)BUILT_IN_ATOMIC_FETCH_SUB_1 +(fcode - BUILT_IN_ATOMIC_SUB_FETCH_1));
         target = expand_builtin_atomic_fetch_op(self,mode, exp, target, MINUS, true,
         ignore, lib);
         if (target)
            return target;
         break;
      }
      case BUILT_IN_ATOMIC_AND_FETCH_1:
      case BUILT_IN_ATOMIC_AND_FETCH_2:
      case BUILT_IN_ATOMIC_AND_FETCH_4:
      case BUILT_IN_ATOMIC_AND_FETCH_8:
      case BUILT_IN_ATOMIC_AND_FETCH_16:
      {
         enum built_in_function lib;
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_ATOMIC_AND_FETCH_1);
         lib = (enum built_in_function)((int)BUILT_IN_ATOMIC_FETCH_AND_1 +(fcode - BUILT_IN_ATOMIC_AND_FETCH_1));
         target = expand_builtin_atomic_fetch_op(self,mode, exp, target, AND, true,
         ignore, lib);
         if (target)
            return target;
         break;
      }
      case BUILT_IN_ATOMIC_NAND_FETCH_1:
      case BUILT_IN_ATOMIC_NAND_FETCH_2:
      case BUILT_IN_ATOMIC_NAND_FETCH_4:
      case BUILT_IN_ATOMIC_NAND_FETCH_8:
      case BUILT_IN_ATOMIC_NAND_FETCH_16:
      {
         enum built_in_function lib;
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_ATOMIC_NAND_FETCH_1);
         lib = (enum built_in_function)((int)BUILT_IN_ATOMIC_FETCH_NAND_1 +
         (fcode - BUILT_IN_ATOMIC_NAND_FETCH_1));
         target = expand_builtin_atomic_fetch_op(self,mode, exp, target, NOT, true,
         ignore, lib);
         if (target)
            return target;
         break;
      }
      case BUILT_IN_ATOMIC_XOR_FETCH_1:
      case BUILT_IN_ATOMIC_XOR_FETCH_2:
      case BUILT_IN_ATOMIC_XOR_FETCH_4:
      case BUILT_IN_ATOMIC_XOR_FETCH_8:
      case BUILT_IN_ATOMIC_XOR_FETCH_16:
      {
         enum built_in_function lib;
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_ATOMIC_XOR_FETCH_1);
         lib = (enum built_in_function)((int)BUILT_IN_ATOMIC_FETCH_XOR_1 +
         (fcode - BUILT_IN_ATOMIC_XOR_FETCH_1));
         target = expand_builtin_atomic_fetch_op(self,mode, exp, target, XOR, true,
         ignore, lib);
         if (target)
            return target;
         break;
      }
      case BUILT_IN_ATOMIC_OR_FETCH_1:
      case BUILT_IN_ATOMIC_OR_FETCH_2:
      case BUILT_IN_ATOMIC_OR_FETCH_4:
      case BUILT_IN_ATOMIC_OR_FETCH_8:
      case BUILT_IN_ATOMIC_OR_FETCH_16:
      {
         enum built_in_function lib;
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_ATOMIC_OR_FETCH_1);
         lib = (enum built_in_function)((int)BUILT_IN_ATOMIC_FETCH_OR_1 +
         (fcode - BUILT_IN_ATOMIC_OR_FETCH_1));
         target = expand_builtin_atomic_fetch_op(self,mode, exp, target, IOR, true,
         ignore, lib);
         if (target)
            return target;
         break;
      }
      case BUILT_IN_ATOMIC_FETCH_ADD_1:
      case BUILT_IN_ATOMIC_FETCH_ADD_2:
      case BUILT_IN_ATOMIC_FETCH_ADD_4:
      case BUILT_IN_ATOMIC_FETCH_ADD_8:
      case BUILT_IN_ATOMIC_FETCH_ADD_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_ATOMIC_FETCH_ADD_1);
         target = expand_builtin_atomic_fetch_op(self,mode, exp, target, PLUS, false,
         ignore, BUILT_IN_NONE);
         if (target)
            return target;
         break;

      case BUILT_IN_ATOMIC_FETCH_SUB_1:
      case BUILT_IN_ATOMIC_FETCH_SUB_2:
      case BUILT_IN_ATOMIC_FETCH_SUB_4:
      case BUILT_IN_ATOMIC_FETCH_SUB_8:
      case BUILT_IN_ATOMIC_FETCH_SUB_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_ATOMIC_FETCH_SUB_1);
         target = expand_builtin_atomic_fetch_op(self,mode, exp, target, MINUS, false,
         ignore, BUILT_IN_NONE);
         if (target)
            return target;
         break;

      case BUILT_IN_ATOMIC_FETCH_AND_1:
      case BUILT_IN_ATOMIC_FETCH_AND_2:
      case BUILT_IN_ATOMIC_FETCH_AND_4:
      case BUILT_IN_ATOMIC_FETCH_AND_8:
      case BUILT_IN_ATOMIC_FETCH_AND_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_ATOMIC_FETCH_AND_1);
         target = expand_builtin_atomic_fetch_op(self,mode, exp, target, AND, false,
         ignore, BUILT_IN_NONE);
         if (target)
            return target;
         break;

      case BUILT_IN_ATOMIC_FETCH_NAND_1:
      case BUILT_IN_ATOMIC_FETCH_NAND_2:
      case BUILT_IN_ATOMIC_FETCH_NAND_4:
      case BUILT_IN_ATOMIC_FETCH_NAND_8:
      case BUILT_IN_ATOMIC_FETCH_NAND_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_ATOMIC_FETCH_NAND_1);
         target = expand_builtin_atomic_fetch_op(self,mode, exp, target, NOT, false,
         ignore, BUILT_IN_NONE);
         if (target)
            return target;
         break;

      case BUILT_IN_ATOMIC_FETCH_XOR_1:
      case BUILT_IN_ATOMIC_FETCH_XOR_2:
      case BUILT_IN_ATOMIC_FETCH_XOR_4:
      case BUILT_IN_ATOMIC_FETCH_XOR_8:
      case BUILT_IN_ATOMIC_FETCH_XOR_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_ATOMIC_FETCH_XOR_1);
         target = expand_builtin_atomic_fetch_op(self,mode, exp, target, XOR, false,
         ignore, BUILT_IN_NONE);
         if (target)
            return target;
         break;

      case BUILT_IN_ATOMIC_FETCH_OR_1:
      case BUILT_IN_ATOMIC_FETCH_OR_2:
      case BUILT_IN_ATOMIC_FETCH_OR_4:
      case BUILT_IN_ATOMIC_FETCH_OR_8:
      case BUILT_IN_ATOMIC_FETCH_OR_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_ATOMIC_FETCH_OR_1);
         target = expand_builtin_atomic_fetch_op(self,mode, exp, target, IOR, false,
         ignore, BUILT_IN_NONE);
         if (target)
            return target;
         break;

      case BUILT_IN_ATOMIC_TEST_AND_SET:
         target = expand_builtin_atomic_test_and_set(self,exp, target);
         if (target)
            return target;
         break;

      case BUILT_IN_ATOMIC_CLEAR:
         return expand_builtin_atomic_clear(self,exp);

      case BUILT_IN_ATOMIC_ALWAYS_LOCK_FREE:
         return expand_builtin_atomic_always_lock_free(self,exp);

      case BUILT_IN_ATOMIC_IS_LOCK_FREE:
         target = expand_builtin_atomic_is_lock_free(self,exp);
         if (target)
            return target;
         break;

      case BUILT_IN_ATOMIC_THREAD_FENCE:
         expand_builtin_atomic_thread_fence(self,exp);
         return const0_rtx;

      case BUILT_IN_ATOMIC_SIGNAL_FENCE:
         expand_builtin_atomic_signal_fence(self,exp);
         return const0_rtx;

      case BUILT_IN_OBJECT_SIZE:
      case BUILT_IN_DYNAMIC_OBJECT_SIZE:
         return expand_builtin_object_size(self,exp);

      case BUILT_IN_MEMCPY_CHK:
      case BUILT_IN_MEMPCPY_CHK:
      case BUILT_IN_MEMMOVE_CHK:
      case BUILT_IN_MEMSET_CHK:
         target = expand_builtin_memory_chk(self,exp, target, mode, fcode);
         if (target)
            return target;
         break;

      case BUILT_IN_STRCPY_CHK:
      case BUILT_IN_STPCPY_CHK:
      case BUILT_IN_STRNCPY_CHK:
      case BUILT_IN_STPNCPY_CHK:
      case BUILT_IN_STRCAT_CHK:
      case BUILT_IN_STRNCAT_CHK:
      case BUILT_IN_SNPRINTF_CHK:
      case BUILT_IN_VSNPRINTF_CHK:
         maybe_emit_chk_warning(self,exp, fcode);
         break;
      case BUILT_IN_SPRINTF_CHK:
      case BUILT_IN_VSPRINTF_CHK:
         maybe_emit_sprintf_chk_warning(self,exp, fcode);
         break;

      case BUILT_IN_THREAD_POINTER:
         return expand_builtin_thread_pointer(self,exp, target);

      case BUILT_IN_SET_THREAD_POINTER:
         expand_builtin_set_thread_pointer(self,exp);
         return const0_rtx;

      case BUILT_IN_ACC_ON_DEVICE:
         /* Do library call, if we failed to expand the builtin when
         folding.  */
         break;
       /*
      case BUILT_IN_GOACC_PARLEVEL_ID:
      case BUILT_IN_GOACC_PARLEVEL_SIZE:
         return expand_builtin_goacc_parlevel_id_size (exp, target, ignore);
         */

      case BUILT_IN_SPECULATION_SAFE_VALUE_PTR:
         return expand_speculation_safe_value(self,VOIDmode, exp, target, ignore);

      case BUILT_IN_SPECULATION_SAFE_VALUE_1:
      case BUILT_IN_SPECULATION_SAFE_VALUE_2:
      case BUILT_IN_SPECULATION_SAFE_VALUE_4:
      case BUILT_IN_SPECULATION_SAFE_VALUE_8:
      case BUILT_IN_SPECULATION_SAFE_VALUE_16:
         mode = get_builtin_sync_mode(self,fcode - BUILT_IN_SPECULATION_SAFE_VALUE_1);
         return expand_speculation_safe_value(self,mode, exp, target, ignore);

      default:    /* just do library call, if unknown builtin */
         break;
   }

   n_debug("mtcsbuiltins.c expand_builtin 66 调用 mtcs_calls_expand_call 按正常函数处理 exp:%p target:%p ignore:%d\n",exp,target,ignore);
   /* The switch statement above can drop through to cause the function
   to be called normally.  */
   return mtcs_calls_expand_call/*!expand_call*/(mtcsCalls,exp, target, ignore);
}

/* Emit a call to __builtin___clear_cache, unless the target specifies
   it as do-nothing.  This function can be used by trampoline
   finalizers to duplicate the effects of expanding a call to the
   clear_cache builtin.  */
//原型 maybe_emit_call_builtin___clear_cache builtins.h builtins.cc
void mtcs_builtins_maybe_emit_call_builtin___clear_cache (MtcsBuiltins *self,rtx begin, rtx end)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);

   gcc_assert ((GET_MODE (begin) == ptr_mode || GET_MODE (begin) == pMode
      || CONST_INT_P (begin))
      && (GET_MODE (end) == ptr_mode || GET_MODE (end) == pMode
      || CONST_INT_P (end)));

   if (target_rtx_have_clear_cache/*!targetm.have_clear_cache*/(mtcsMachine->tmrtx)){
      /* We have a "clear_cache" insn, and it will handle everything.  */
      class expand_operand ops[2];

      create_address_operand (&ops[0], begin);
      create_address_operand (&ops[1], end);

      if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(mtcsOptabs,
            mtcsMachine->tmrtx->code_for_clear_cache/*!targetm.code_for_clear_cache*/, 2, ops))
         return;
   }else{
#ifndef CLEAR_INSN_CACHE
      /* There is no "clear_cache" insn, and __clear_cache() in libgcc
      does nothing.  There is no need to call it.  Do nothing.  */
      return;
#endif /* CLEAR_INSN_CACHE */
   }
   target_calls_emit_call_builtin___clear_cache/*!targetm.calls.emit_call_builtin___clear_cache*/(mtcsMachine->calls,begin, end);
}

/* Emit a call to __builtin___clear_cache.  */
//原型 default_emit_call_builtin___clear_cache targhooks.h builtins.cc
void mtcs_builtins_default_emit_call_builtin___clear_cache (MtcsBuiltins *self,rtx begin, rtx end)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);

   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
   rtx callee = gen_rtx_SYMBOL_REF (pMode, BUILTIN_ASM_NAME_PTR(BUILT_IN_CLEAR_CACHE));

   mtcs_calls_emit_library_call/*!emit_library_call*/(mtcsCalls,callee,
         LCT_NORMAL, VOIDmode,
         mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,ptr_mode, begin), ptr_mode,
         mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,ptr_mode, end), ptr_mode);
}

/* Get a MEM rtx for expression EXP which is the address of an operand
   to be used in a string instruction (cmpstrsi, cpymemsi, ..).  LEN is
   the maximum length of the block of memory that might be accessed or
   NULL if unknown.  */
//原型 get_memory_rtx builtins.h builtins.cc
rtx mtcs_builtins_get_memory_rtx (MtcsBuiltins *self,tree exp, tree len)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree orig_exp = exp, base;
   rtx addr, mem;
   gcc_checking_assert(ADDR_SPACE_GENERIC_P (TYPE_ADDR_SPACE (TREE_TYPE (TREE_TYPE (exp)))));
   /* When EXP is not resolved SAVE_EXPR, MEM_ATTRS can be still derived
   from its expression, for expr->a.b only <variable>.a.b is recorded.  */
   if (TREE_CODE (exp) == SAVE_EXPR && !SAVE_EXPR_RESOLVED_P (exp))
      exp = TREE_OPERAND (exp, 0);

   addr = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,orig_exp, NULL_RTX, ptr_mode, EXPAND_NORMAL);
   mem = gen_rtx_MEM (mtcsMode->modes.M_BLKmode,
   mtcs_explow_memory_address/*!memory_address*/(mtcsExplow,mtcsMode->modes.M_BLKmode, addr));

   /* Get an expression we can use to find the attributes to assign to MEM.
   First remove any nops.  */
   while (CONVERT_EXPR_P (exp)  && POINTER_TYPE_P (TREE_TYPE (TREE_OPERAND (exp, 0))))
      exp = TREE_OPERAND (exp, 0);

   /* Build a MEM_REF representing the whole accessed area as a byte blob,
   (as builtin stringops may alias with anything).  */
   exp = mtcs_const_fold_build2(mtcsConst,MEM_REF, build_array_type (char_type_node,
   build_range_type (sizetype, size_one_node, len)),
   exp, mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,ptr_type_node, 0));

   /* If the MEM_REF has no acceptable address, try to get the base object
   from the original address we got, and build an all-aliasing
   unknown-sized access to that one.  */
   if (is_gimple_mem_ref_addr (TREE_OPERAND (exp, 0)))
      mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,mem, exp, 0);
   else if (TREE_CODE (TREE_OPERAND (exp, 0)) == ADDR_EXPR
     && (base = get_base_address (TREE_OPERAND (TREE_OPERAND (exp, 0),0)))){
      unsigned int align = get_pointer_alignment (TREE_OPERAND (exp, 0));
      exp = mtcs_const_build_fold_addr_expr/*!build_fold_addr_expr*/(mtcsConst,base);
      exp = mtcs_const_fold_build2(mtcsConst,MEM_REF, build_array_type (char_type_node,
      build_range_type (sizetype,size_zero_node,NULL)),exp, mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,ptr_type_node, 0));
      mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,mem, exp, 0);
      /* Since we stripped parts make sure the offset is unknown and the
      alignment is computed from the original address.  */
      mtcs_rtl_clear_mem_offset/*!clear_mem_offset*/(mtcsRTL,mem);
      mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,mem, align);
   }
   mtcs_rtl_set_mem_alias_set/*!set_mem_alias_set*/(mtcsRTL,mem, 0);
   return mem;
}

/* Callback routine for store_by_pieces.  Read GET_MODE_BITSIZE (MODE)
   bytes from constant string DATA + OFFSET and return it as target
   constant.  If PREV isn't nullptr, it has the RTL info from the
   previous iteration.  */
//原型 builtin_memset_read_str builtins.h builtins.cc 函数指针 by_pieces_constfn 是它的原型
rtx mtcs_builtins_builtin_memset_read_str (void *userData, void *prev,
          HOST_WIDE_INT offset ATTRIBUTE_UNUSED, fixed_size_mode mode)
{
   BuiltinReadStrData *builtinReadStrData=(BuiltinReadStrData *)userData;
   MtcsBuiltins *self=(MtcsBuiltins *)builtinReadStrData->self;
   const char *c = (const char *) builtinReadStrData->data;

   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   unsigned int size = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);

   rtx target = gen_memset_value_from_prev(self,(by_pieces_prev *) prev, mode);
   if (target != nullptr)
      return target;
   rtx src = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,*c, mtcsMode->modes.M_QImode);

   if (mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,mode)){
      gcc_assert (mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode) == mtcsMode->modes.M_QImode);

      rtx const_vec = mtcs_rtl_gen_const_vec_duplicate/*!gen_const_vec_duplicate*/(mtcsRTL,mode, src);
      if (prev == NULL)
         /* Return CONST_VECTOR when called by a query function.  */
         return const_vec;

      /* Use the move expander with CONST_VECTOR.  */
      target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, const_vec);
      return target;
   }

   char *p = XALLOCAVEC (char, size);
   memset (p, *c, size);
   return c_readstr (p, mode);
}

/* The "standard" implementation of va_start: just assign `nextarg' to
   the variable.  */
//原型 std_expand_builtin_va_start builtins.h butilins.cc
void mtcs_builtins_std_expand_builtin_va_start (MtcsBuiltins *self,tree valist, rtx nextarg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   rtx va_r = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,valist, NULL_RTX, VOIDmode, EXPAND_WRITE);
   mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,va_r, nextarg, 0);
}

/* Fold the next_arg or va_start call EXP. Returns true if there was an error
   produced.  False otherwise.  This is done so that we don't output the error
   or warning twice or three times.  */
//原型 fold_builtin_next_arg builtins.h butilins.cc
bool mtcs_builtins_fold_builtin_next_arg (MtcsBuiltins *self,tree exp, bool va_start_p)
{
   tree fntype = TREE_TYPE (current_function_decl);
   int nargs = call_expr_nargs (exp);
   tree arg;
   /* There is good chance the current input_location points inside the
   definition of the va_start macro (perhaps on the token for
   builtin) in a system header, so warnings will not be emitted.
   Use the location in real source code.  */
   location_t current_location =linemap_unwind_to_first_non_reserved_loc (line_table, input_location,NULL);

   if (!stdarg_p (fntype)){
      error ("%<va_start%> used in function with fixed arguments");
      return true;
   }

   if (va_start_p){
      if (va_start_p && (nargs != 2)){
         error ("wrong number of arguments to function %<va_start%>");
         return true;
      }
      arg = CALL_EXPR_ARG (exp, 1);
   }
   /* We use __builtin_va_start (ap, 0, 0) or __builtin_next_arg (0, 0)
   when we checked the arguments and if needed issued a warning.  */
   else{
      if (nargs == 0){
         /* Evidently an out of date version of <stdarg.h>; can't validate
         va_start's second argument, but can still work as intended.  */
         warning_at (current_location,OPT_Wvarargs,"%<__builtin_next_arg%> called without an argument");
         return true;
      }else if (nargs > 1){
         error ("wrong number of arguments to function %<__builtin_next_arg%>");
         return true;
      }
      arg = CALL_EXPR_ARG (exp, 0);
   }

   if (TREE_CODE (arg) == SSA_NAME && SSA_NAME_VAR (arg))
      arg = SSA_NAME_VAR (arg);

      /* We destructively modify the call to be __builtin_va_start (ap, 0)
      or __builtin_next_arg (0) the first time we see it, after checking
      the arguments and if needed issuing a warning.  */
      if (!integer_zerop (arg)){
         tree last_parm = tree_last (DECL_ARGUMENTS (current_function_decl));

      /* Strip off all nops for the sake of the comparison.  This
      is not quite the same as STRIP_NOPS.  It does more.
      We must also strip off INDIRECT_EXPR for C++ reference
      parameters.  */
      while (CONVERT_EXPR_P (arg) || INDIRECT_REF_P (arg))
         arg = TREE_OPERAND (arg, 0);
      if (arg != last_parm){
         /* FIXME: Sometimes with the tree optimizers we can get the
         not the last argument even though the user used the last
         argument.  We just warn and set the arg to be the last
         argument so that we will get wrong-code because of
         it.  */
         warning_at (current_location,OPT_Wvarargs, "second parameter of %<va_start%> not last named argument");
      }
      /* Undefined by C99 7.15.1.4p4 (va_start):
      "If the parameter parmN is declared with the register storage
      class, with a function or array type, or with a type that is
      not compatible with the type that results after application of
      the default argument promotions, the behavior is undefined."
      */
      else if (DECL_REGISTER (arg)){
         warning_at (current_location, OPT_Wvarargs,
         "undefined behavior when second parameter of "
         "%<va_start%> is declared with %<register%> storage");
      }

      /* We want to verify the second parameter just once before the tree
      optimizers are run and then avoid keeping it in the tree,
      as otherwise we could warn even for correct code like:
      void foo (int i, ...)
      { va_list ap; i++; va_start (ap, i); va_end (ap); }  */
      if (va_start_p)
         CALL_EXPR_ARG (exp, 1) = integer_zero_node;
      else
         CALL_EXPR_ARG (exp, 0) = integer_zero_node;
   }
   return false;
}

/* Helper function for do_mpc_arg*().  Ensure M is a normal complex
   number and no overflow/underflow occurred.  INEXACT is true if M
   was not exactly calculated.  TYPE is the tree type for the result.
   This function assumes that you cleared the MPFR flags and then
   calculated M to see if anything subsequently set a flag prior to
   entering this function.  Return NULL_TREE if any checks fail, if
   FORCE_CONVERT is true, then bypass the checks.  */
//原型 do_mpc_ckconv builtins.cc
static tree do_mpc_ckconv (MtcsBuiltins *self,mpc_srcptr m, tree type, int inexact, int force_convert)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

   /* Proceed iff we get a normal number, i.e. not NaN or Inf and no
   overflow/underflow occurred.  If -frounding-math, proceed iff the
   result of calling FUNC was exact.  */
   if (force_convert   || (mpfr_number_p (mpc_realref (m)) && mpfr_number_p (mpc_imagref (m))
   && !mpfr_overflow_p () && !mpfr_underflow_p ()   && (!mtcsOptionsItem->x_flag_rounding_math || !inexact))){
      REAL_VALUE_TYPE re, im;

      real_from_mpfr (&re, mpc_realref (m), TREE_TYPE (type), MPFR_RNDN);
      real_from_mpfr (&im, mpc_imagref (m), TREE_TYPE (type), MPFR_RNDN);
      /* Proceed iff GCC's REAL_VALUE_TYPE can hold the MPFR values,
      check for overflow/underflow.  If the REAL_VALUE_TYPE is zero
      but the mpfr_t is not, then we underflowed in the
      conversion.  */
      if (force_convert || (real_isfinite (&re) && real_isfinite (&im)
      && (re.cl == rvc_zero) == (mpfr_zero_p (mpc_realref (m)) != 0)
      && (im.cl == rvc_zero) == (mpfr_zero_p (mpc_imagref (m)) != 0))){
         REAL_VALUE_TYPE re_mode, im_mode;

         mtcs_real_real_convert/*!real_convert*/(mtcsReal,&re_mode, TYPE_MODE (TREE_TYPE (type)), &re);
         mtcs_real_real_convert/*!real_convert*/(mtcsReal,&im_mode, TYPE_MODE (TREE_TYPE (type)), &im);
         /* Proceed iff the specified mode can hold the value.  */
         if (force_convert || (real_identical (&re_mode, &re)  && real_identical (&im_mode, &im)))
            return mtcs_tree_build_complex/*!build_complex*/(mtcsTree,
                  type, mtcs_tree_build_real/*!build_real*/(mtcsTree,
                        TREE_TYPE (type), re_mode),  mtcs_tree_build_real/*!build_real*/(mtcsTree,TREE_TYPE (type), im_mode));
      }
   }
   return NULL_TREE;
}

/* If arguments ARG0 and ARG1 are a COMPLEX_CST, call the two-argument
   mpc function FUNC on it and return the resulting value as a tree
   with type TYPE.  The mpfr precision is set to the precision of
   TYPE.  We assume that function FUNC returns zero if the result
   could be calculated exactly within the requested precision.  If
   DO_NONFINITE is true, then fold expressions containing Inf or NaN
   in the arguments and/or results.  */
//原型 do_mpc_arg2 builtins.h builtins.cc
tree mtcs_builtins_do_mpc_arg2 (MtcsBuiltins *self,tree arg0, tree arg1, tree type, int do_nonfinite,
        int (*func)(mpc_ptr, mpc_srcptr, mpc_srcptr, mpc_rnd_t))
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   tree result = NULL_TREE;
   STRIP_NOPS (arg0);
   STRIP_NOPS (arg1);

   /* To proceed, MPFR must exactly represent the target floating point
   format, which only happens when the target base equals two.  */
   if (TREE_CODE (arg0) == COMPLEX_CST && !TREE_OVERFLOW (arg0)
   && SCALAR_FLOAT_TYPE_P (TREE_TYPE (TREE_TYPE (arg0)))
   && TREE_CODE (arg1) == COMPLEX_CST && !TREE_OVERFLOW (arg1)
   && SCALAR_FLOAT_TYPE_P (TREE_TYPE (TREE_TYPE (arg1)))
   && mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,TYPE_MODE (TREE_TYPE (TREE_TYPE (arg0))))->b == 2){
      const REAL_VALUE_TYPE *const re0 = TREE_REAL_CST_PTR (TREE_REALPART (arg0));
      const REAL_VALUE_TYPE *const im0 = TREE_REAL_CST_PTR (TREE_IMAGPART (arg0));
      const REAL_VALUE_TYPE *const re1 = TREE_REAL_CST_PTR (TREE_REALPART (arg1));
      const REAL_VALUE_TYPE *const im1 = TREE_REAL_CST_PTR (TREE_IMAGPART (arg1));

      if (do_nonfinite  || (real_isfinite (re0) && real_isfinite (im0)
      && real_isfinite (re1) && real_isfinite (im1))){
         const struct real_format *const fmt = mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,
               TYPE_MODE (TREE_TYPE (type)));
         const int prec = fmt->p;
         const mpfr_rnd_t rnd = fmt->round_towards_zero ? MPFR_RNDZ : MPFR_RNDN;
         const mpc_rnd_t crnd = fmt->round_towards_zero ? MPC_RNDZZ : MPC_RNDNN;
         int inexact;
         mpc_t m0, m1;

         mpc_init2 (m0, prec);
         mpc_init2 (m1, prec);
         mpfr_from_real (mpc_realref (m0), re0, rnd);
         mpfr_from_real (mpc_imagref (m0), im0, rnd);
         mpfr_from_real (mpc_realref (m1), re1, rnd);
         mpfr_from_real (mpc_imagref (m1), im1, rnd);
         mpfr_clear_flags ();
         inexact = func (m0, m0, m1, crnd);
         result = do_mpc_ckconv(self,m0, type, inexact, do_nonfinite);
         mpc_clear (m0);
         mpc_clear (m1);
      }
   }

   return result;
}

/**
 * 主机的内建函数是否要转化，为其它主机函数
 * 比如 主机put函数在nvptx中，只能转为printf函数
 * 如果要转的函数是主机没有的要如何处理
 */
nboolean mtcs_builtins_replace_call(MtcsBuiltins *self, gimple *call)
{
   return self->replace_call(self,call);
}

//主机的内建函数调用转为平台的内建函数调用，并做优化
nboolean mtcs_builtins_convert_call(MtcsBuiltins *self, gimple *call)
{
   return self->convert_call(self,call);
}

nboolean mtcs_builtins_support_builtin_fn(MtcsBuiltins *self,tree fndecl)
{
   return self->support_builtin_fn(self,fndecl);
}


rtx mtcs_builtins_expand_mtcs_builtin (MtcsBuiltins *self,tree exp, rtx target, rtx subtarget,
        machine_mode mode, int ignore)
{
   tree fndecl = get_callee_fndecl (exp);
   machine_mode target_mode = TYPE_MODE (TREE_TYPE (exp));
   int flags;
   /* When ASan is enabled, we don't want to expand some memory/string
      builtins and rely on libsanitizer's hooks.  This allows us to avoid
      redundant checks and be sure, that possible overflow will be detected
      by ASan.  */
   //enum built_in_function fcode = DECL_FUNCTION_CODE (fndecl);
   /* The built-in function expanders test for target == const0_rtx
      to determine whether the function's result will be ignored.  */
   if (ignore)
     target = const0_rtx;
   n_debug("mtcsbuiltins.c mtcs_builtins_expand_mtcs_builtin 调用子类的 expand_builtin_fn ignore:%d\n",ignore);
   return self->expand_builtin_fn(self,exp,target,subtarget,mode,ignore);
}

rtx mtcs_builtins_expand_mtcs_internal (MtcsBuiltins *self,tree exp, rtx target, rtx subtarget,
        machine_mode mode, int ignore)
{
   tree fndecl = get_callee_fndecl (exp);
   machine_mode target_mode = TYPE_MODE (TREE_TYPE (exp));
   int flags;
   /* When ASan is enabled, we don't want to expand some memory/string
      builtins and rely on libsanitizer's hooks.  This allows us to avoid
      redundant checks and be sure, that possible overflow will be detected
      by ASan.  */
   //enum built_in_function fcode = DECL_FUNCTION_CODE (fndecl);
   /* The built-in function expanders test for target == const0_rtx
      to determine whether the function's result will be ignored.  */
   if (ignore)
     target = const0_rtx;
   n_debug("mtcsbuiltins.c mtcs_builtins_expand_mtcs_internal 调用子类的 expand_internal_fn ignore:%d\n",ignore);
   return self->expand_internal_fn(self,exp,target,subtarget,mode,ignore);
}

int mtcs_builtins_get_code(tree fndecl)
{
   if(!fndecl || TREE_CODE(fndecl)!=FUNCTION_DECL)
      return -1;
   nboolean re1 = mtcs_builtins_is_internal_fn(fndecl);
   nboolean re2 = mtcs_builtins_is_builtin_fn(fndecl);
   if(re1 || re2){
      tree_function_decl &fn = FUNCTION_DECL_CHECK (fndecl)->function_decl;
      return fn.function_code;
   }
   return -1;
}

nboolean mtcs_builtins_is_internal_fn(tree fndecl)
{
   if(!fndecl || TREE_CODE(fndecl)!=FUNCTION_DECL)
      return FALSE;
   tree_function_decl &fn = FUNCTION_DECL_CHECK (fndecl)->function_decl;
   return (fn.built_in_class == NOT_BUILT_IN &&
         fn.function_code>=MTCS_INTERNAL_FN_CODE_START &&
         fn.function_code<=MTCS_INTERNAL_FN_CODE_END);
}

nboolean mtcs_builtins_is_builtin_fn(tree fndecl)
{
   if(!fndecl || TREE_CODE(fndecl)!=FUNCTION_DECL)
      return FALSE;
   tree_function_decl &fn = FUNCTION_DECL_CHECK (fndecl)->function_decl;
   return (fn.built_in_class == NOT_BUILT_IN &&
         fn.function_code>=MTCS_BUILTIN_FN_CODE_START &&
         fn.function_code<=MTCS_BUILTIN_FN_CODE_END);
}

/* Compute values M and N such that M divides (address of EXP - N) and such
   that N < M.  If these numbers can be determined, store M in alignp and N in
   *BITPOSP and return true.  Otherwise return false and store BITS_PER_UNIT to
   *alignp and any bit-offset to *bitposp.

   Note that the address (and thus the alignment) computed here is based
   on the address to which a symbol resolves, whereas DECL_ALIGN is based
   on the address at which an object is actually located.  These two
   addresses are not always the same.  For example, on ARM targets,
   the address &foo of a Thumb function foo() has the lowest bit set,
   whereas foo() itself starts on an even address.

   If ADDR_P is true we are taking the address of the memory reference EXP
   and thus cannot rely on the access taking place.  */
//原型 get_object_alignment_2 builtins.h builtins.cc
bool mtcs_builtins_get_object_alignment_2 (MtcsBuiltins *self,tree exp, unsigned int *alignp,
         unsigned HOST_WIDE_INT *bitposp, bool addr_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   poly_int64 bitsize, bitpos;
   tree offset;
   machine_mode mode;
   int unsignedp, reversep, volatilep;
   unsigned int align = BITS_PER_UNIT;
   bool known_alignment = false;

   /* Get the innermost object and the constant (bitpos) and possibly
   variable (offset) offset of the access.  */
   exp = mtcs_expr_get_inner_reference/*!get_inner_reference*/(mtcsExpr,
         exp, &bitsize, &bitpos, &offset, &mode, &unsignedp, &reversep, &volatilep);

   /* Extract alignment information from the innermost object and
   possibly adjust bitpos and offset.  */
   if (TREE_CODE (exp) == FUNCTION_DECL){
      /* Function addresses can encode extra information besides their
      alignment.  However, if TARGET_PTRMEMFUNC_VBIT_LOCATION
      allows the low bit to be used as a virtual bit, we know
      that the address itself must be at least 2-byte aligned.  */
      if (TARGET_PTRMEMFUNC_VBIT_LOCATION == ptrmemfunc_vbit_in_pfn)
         align = 2 * BITS_PER_UNIT;
   }else if (TREE_CODE (exp) == LABEL_DECL)
      ;
   else if (TREE_CODE (exp) == CONST_DECL){
      /* The alignment of a CONST_DECL is determined by its initializer.  */
      exp = DECL_INITIAL (exp);
      align = TYPE_ALIGN (TREE_TYPE (exp));
      if (CONSTANT_CLASS_P (exp))
         align = mtcsTarget/*!targetm.constant_alignment*/->constant_alignment(mtcsTarget,exp, align);

      known_alignment = true;
   }else if (DECL_P (exp)){
      align = DECL_ALIGN (exp);
      known_alignment = true;
   }else if (TREE_CODE (exp) == INDIRECT_REF || TREE_CODE (exp) == MEM_REF || TREE_CODE (exp) == TARGET_MEM_REF){
      tree addr = TREE_OPERAND (exp, 0);
      unsigned ptr_align;
      unsigned HOST_WIDE_INT ptr_bitpos;
      unsigned HOST_WIDE_INT ptr_bitmask = ~0;

      /* If the address is explicitely aligned, handle that.  */
      if (TREE_CODE (addr) == BIT_AND_EXPR && TREE_CODE (TREE_OPERAND (addr, 1)) == INTEGER_CST){
         ptr_bitmask = TREE_INT_CST_LOW (TREE_OPERAND (addr, 1));
         ptr_bitmask *= BITS_PER_UNIT;
         align = least_bit_hwi (ptr_bitmask);
         addr = TREE_OPERAND (addr, 0);
      }

      known_alignment= mtcs_builtins_get_object_alignment_1/*!get_pointer_alignment_1*/(self,addr, &ptr_align, &ptr_bitpos);
      align = MAX (ptr_align, align);

      /* Re-apply explicit alignment to the bitpos.  */
      ptr_bitpos &= ptr_bitmask;

      /* The alignment of the pointer operand in a TARGET_MEM_REF
      has to take the variable offset parts into account.  */
      if (TREE_CODE (exp) == TARGET_MEM_REF){
         if (TMR_INDEX (exp)){
            unsigned HOST_WIDE_INT step = 1;
            if (TMR_STEP (exp))
               step = TREE_INT_CST_LOW (TMR_STEP (exp));
            align = MIN (align, least_bit_hwi (step) * BITS_PER_UNIT);
         }
         if (TMR_INDEX2 (exp))
            align = BITS_PER_UNIT;
         known_alignment = false;
      }

      /* When EXP is an actual memory reference then we can use
      TYPE_ALIGN of a pointer indirection to derive alignment.
      Do so only if get_pointer_alignment_1 did not reveal absolute
      alignment knowledge and if using that alignment would
      improve the situation.  */
      unsigned int talign;
      if (!addr_p && !known_alignment
      && (talign = min_align_of_type (TREE_TYPE (exp)) * BITS_PER_UNIT) && talign > align)
         align = talign;
      else{
         /* Else adjust bitpos accordingly.  */
         bitpos += ptr_bitpos;
         if (TREE_CODE (exp) == MEM_REF || TREE_CODE (exp) == TARGET_MEM_REF)
            bitpos += mem_ref_offset (exp).force_shwi () * BITS_PER_UNIT;
      }
   }else if (TREE_CODE (exp) == STRING_CST){
      /* STRING_CST are the only constant objects we allow to be not
      wrapped inside a CONST_DECL.  */
      align = TYPE_ALIGN (TREE_TYPE (exp));
      if (CONSTANT_CLASS_P (exp))
         align = mtcsTarget/*!targetm.constant_alignment*/->constant_alignment(mtcsTarget,exp, align);

      known_alignment = true;
   }

   /* If there is a non-constant offset part extract the maximum
   alignment that can prevail.  */
   if (offset){
      unsigned int trailing_zeros = tree_ctz (offset);
      if (trailing_zeros < HOST_BITS_PER_INT){
         unsigned int inner = (1U << trailing_zeros) * BITS_PER_UNIT;
         if (inner)
            align = MIN (align, inner);
      }
   }

   /* Account for the alignment of runtime coefficients, so that the constant
   bitpos is guaranteed to be accurate.  */
   unsigned int alt_align = ::known_alignment (bitpos - bitpos.coeffs[0]);
   if (alt_align != 0 && alt_align < align){
      align = alt_align;
      known_alignment = false;
   }

   *alignp = align;
   *bitposp = bitpos.coeffs[0] & (align - 1);
   return known_alignment;
}



/* For a memory reference expression EXP compute values M and N such that M
   divides (&EXP - N) and such that N < M.  If these numbers can be determined,
   store M in alignp and N in *BITPOSP and return true.  Otherwise return false
   and store BITS_PER_UNIT to *alignp and any bit-offset to *bitposp.  */
//原型 get_object_alignment_1 builtins.h builtins.cc
bool mtcs_builtins_get_object_alignment_1 (MtcsBuiltins *self,tree exp, unsigned int *alignp,
         unsigned HOST_WIDE_INT *bitposp)
{
   /* Strip a WITH_SIZE_EXPR, get_inner_reference doesn't know how to deal
   with it.  */
   if (TREE_CODE (exp) == WITH_SIZE_EXPR)
      exp = TREE_OPERAND (exp, 0);
   return mtcs_builtins_get_object_alignment_2/*!get_object_alignment_2*/(self,exp, alignp, bitposp, false);
}

/* Return the alignment in bits of EXP, an object.  */
//原型 get_object_alignment builtins.h builtins.cc
unsigned int mtcs_builtins_get_object_alignment (MtcsBuiltins *self,tree exp)
{
   unsigned HOST_WIDE_INT bitpos = 0;
   unsigned int align;

   mtcs_builtins_get_object_alignment_1/*!get_object_alignment_1*/(self,exp, &align, &bitpos);

   /* align and bitpos now specify known low bits of the pointer.
   ptr & (align - 1) == bitpos.  */

   if (bitpos != 0)
      align = least_bit_hwi (bitpos);
   return align;
}

/* Fold a call to __builtin_FILE to a constant string.  */

static inline tree fold_builtin_FILE (location_t loc)
{
   if (const char *fname = LOCATION_FILE (loc)){
      /* The documentation says this builtin is equivalent to the preprocessor
      __FILE__ macro so it appears appropriate to use the same file prefix
      mappings.  */
      fname = remap_macro_filename (fname);
      return build_string_literal (fname);
   }

   return build_string_literal ("");
}

/* Fold a call to __builtin_FUNCTION to a constant string.  */
static inline tree fold_builtin_FUNCTION ()
{
   const char *name = "";

   if (current_function_decl)
      name = lang_hooks.decl_printable_name (current_function_decl, 0);

   return build_string_literal (name);
}


/* Fold a call to __builtin_LINE to an integer constant.  */
static inline tree fold_builtin_LINE (MtcsBuiltins *self,location_t loc, tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   return mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,type, LOCATION_LINE (loc));
}


/* Fold a call to __builtin_inf or __builtin_huge_val.  */
static tree fold_builtin_inf (MtcsBuiltins *self,location_t loc, tree type, int warn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   /* __builtin_inff is intended to be usable to define INFINITY on all
   targets.  If an infinity is not available, INFINITY expands "to a
   positive constant of type float that overflows at translation
   time", footnote "In this case, using INFINITY will violate the
   constraint in 6.4.4 and thus require a diagnostic." (C99 7.12#4).
   Thus we pedwarn to ensure this constraint violation is
   diagnosed.  */
   if (!mtcs_mode_has_infinities/*!MODE_HAS_INFINITIES*/(mtcsMode,TYPE_MODE (type)) && warn)
      pedwarn (loc, 0, "target format does not support infinity");

   return mtcs_tree_build_real/*!build_real*/(mtcsTree,type, dconstinf);
}

/* Fold a call to __builtin_classify_type with argument ARG.  */

static tree fold_builtin_classify_type (MtcsBuiltins *self,tree arg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   if (arg == 0)
      return mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,integer_type_node, no_type_class);

   return mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,integer_type_node, type_to_class (TREE_TYPE (arg)));
}

/* Fold a call to built-in function FNDECL with 0 arguments.
   This function returns NULL_TREE if no simplification was possible.  */

static tree fold_builtin_0 (MtcsBuiltins *self,location_t loc, tree fndecl)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree type = TREE_TYPE (TREE_TYPE (fndecl));
   enum built_in_function fcode = DECL_FUNCTION_CODE (fndecl);
   switch (fcode){
      case BUILT_IN_FILE:
         return fold_builtin_FILE (loc);

      case BUILT_IN_FUNCTION:
         return fold_builtin_FUNCTION ();

      case BUILT_IN_LINE:
         return fold_builtin_LINE (self,loc, type);

      CASE_FLT_FN (BUILT_IN_INF):
      CASE_FLT_FN_FLOATN_NX (BUILT_IN_INF):
      case BUILT_IN_INFD32:
      case BUILT_IN_INFD64:
      case BUILT_IN_INFD128:
      case BUILT_IN_INFD64X:
         return fold_builtin_inf(self,loc, type, true);

      CASE_FLT_FN (BUILT_IN_HUGE_VAL):
      CASE_FLT_FN_FLOATN_NX (BUILT_IN_HUGE_VAL):
         return fold_builtin_inf(self,loc, type, false);

      case BUILT_IN_CLASSIFY_TYPE:
         return fold_builtin_classify_type(self,NULL_TREE);

      case BUILT_IN_UNREACHABLE:
         /* Rewrite any explicit calls to __builtin_unreachable.  */
         if (sanitize_flags_p (SANITIZE_UNREACHABLE))
            return mtcs_tree_builtin_decl_unreachable/*!build_builtin_unreachable*/(mtcsTree,loc);
         break;

      default:
         break;
   }
   return NULL_TREE;
}

/* Fold __builtin_{clz,ctz,clrsb,ffs,parity,popcount}g into corresponding
   internal function.  */
static tree fold_builtin_bit_query (MtcsBuiltins *self,location_t loc, enum built_in_function fcode,
         tree arg0, tree arg1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsInternalFn *mtcsInternalFn = mtcs_target_get_internal_fn(mtcsTarget);

   enum internal_fn ifn;
   enum built_in_function fcodei, fcodel, fcodell;
   tree arg0_type = TREE_TYPE (arg0);
   tree cast_type = NULL_TREE;
   int addend = 0;

   switch (fcode){
      case BUILT_IN_CLZG:
         if (arg1 && TREE_CODE (arg1) != INTEGER_CST)
            return NULL_TREE;
         ifn = IFN_CLZ;
         fcodei = BUILT_IN_CLZ;
         fcodel = BUILT_IN_CLZL;
         fcodell = BUILT_IN_CLZLL;
         break;
      case BUILT_IN_CTZG:
         if (arg1 && TREE_CODE (arg1) != INTEGER_CST)
            return NULL_TREE;
         ifn = IFN_CTZ;
         fcodei = BUILT_IN_CTZ;
         fcodel = BUILT_IN_CTZL;
         fcodell = BUILT_IN_CTZLL;
         break;
      case BUILT_IN_CLRSBG:
         ifn = IFN_CLRSB;
         fcodei = BUILT_IN_CLRSB;
         fcodel = BUILT_IN_CLRSBL;
         fcodell = BUILT_IN_CLRSBLL;
         break;
      case BUILT_IN_FFSG:
         ifn = IFN_FFS;
         fcodei = BUILT_IN_FFS;
         fcodel = BUILT_IN_FFSL;
         fcodell = BUILT_IN_FFSLL;
         break;
      case BUILT_IN_PARITYG:
         ifn = IFN_PARITY;
         fcodei = BUILT_IN_PARITY;
         fcodel = BUILT_IN_PARITYL;
         fcodell = BUILT_IN_PARITYLL;
         break;
      case BUILT_IN_POPCOUNTG:
         ifn = IFN_POPCOUNT;
         fcodei = BUILT_IN_POPCOUNT;
         fcodel = BUILT_IN_POPCOUNTL;
         fcodell = BUILT_IN_POPCOUNTLL;
         break;
      default:
         gcc_unreachable ();
   }

   if (TYPE_PRECISION (arg0_type) <= TYPE_PRECISION (long_long_unsigned_type_node)){
      if (TYPE_PRECISION (arg0_type) <= TYPE_PRECISION (unsigned_type_node))
         cast_type = (TYPE_UNSIGNED (arg0_type)? unsigned_type_node : integer_type_node);
      else if (TYPE_PRECISION (arg0_type) <= TYPE_PRECISION (long_unsigned_type_node)){
         cast_type = (TYPE_UNSIGNED (arg0_type)? long_unsigned_type_node : long_integer_type_node); fcodei = fcodel;
      }else{
         cast_type = (TYPE_UNSIGNED (arg0_type)
            ? long_long_unsigned_type_node
            : long_long_integer_type_node);
            fcodei = fcodell;
      }
   }else if (TYPE_PRECISION (arg0_type) <= MAX_FIXED_MODE_SIZE){
      cast_type = build_nonstandard_integer_type (MAX_FIXED_MODE_SIZE,TYPE_UNSIGNED (arg0_type));
      gcc_assert (TYPE_PRECISION (cast_type) == 2 * TYPE_PRECISION (long_long_unsigned_type_node));
      fcodei = END_BUILTINS;
   }else
      fcodei = END_BUILTINS;
   if (cast_type){
      switch (fcode){
         case BUILT_IN_CLZG:
         case BUILT_IN_CLRSBG:
            addend = TYPE_PRECISION (arg0_type) - TYPE_PRECISION (cast_type);
            break;
         default:
            break;
      }
      arg0 = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,cast_type, arg0);
      arg0_type = cast_type;
   }

   if (arg1)
      arg1 = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,integer_type_node, arg1);

   tree arg2 = arg1;
   if (fcode == BUILT_IN_CLZG && addend){
      if (arg1)
         arg0 = save_expr (arg0);
      arg2 = NULL_TREE;
   }
   tree call = NULL_TREE, tem;
   if (TYPE_PRECISION (arg0_type) == MAX_FIXED_MODE_SIZE
   && (TYPE_PRECISION (arg0_type)  == 2 * TYPE_PRECISION (long_long_unsigned_type_node))
   /* If the target supports the optab, then don't do the expansion. */
   && !mtcs_internal_fn_direct_internal_fn_supported_p/*!direct_internal_fn_supported_p*/(mtcsInternalFn,
         ifn, arg0_type, OPTIMIZE_FOR_BOTH)){
      /* __int128 expansions using up to 2 long long builtins.  */
      arg0 = save_expr (arg0);
      tree type = (TYPE_UNSIGNED (arg0_type) ? long_long_unsigned_type_node : long_long_integer_type_node);
      tree hi = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,RSHIFT_EXPR, arg0_type, arg0,
            mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,integer_type_node, MAX_FIXED_MODE_SIZE / 2));
      hi = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,type, hi);
      tree lo = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,type, arg0);
      switch (fcode){
         case BUILT_IN_CLZG:
            call = fold_builtin_bit_query(self,loc, fcode, lo, NULL_TREE);
            call = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,PLUS_EXPR, integer_type_node, call,
                  mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,integer_type_node,MAX_FIXED_MODE_SIZE / 2));
            if (arg2)
               call = fold_build3 (COND_EXPR, integer_type_node,
                     mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,NE_EXPR, boolean_type_node,lo,
                           mtcs_tree_build_zero_cst/*!build_zero_cst*/(mtcsTree,type)),call, arg2);
            call = fold_build3 (COND_EXPR, integer_type_node,
                  mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,NE_EXPR, boolean_type_node,hi,
                        mtcs_tree_build_zero_cst/*!build_zero_cst*/(mtcsTree,type)),
                        fold_builtin_bit_query(self,loc, fcode, hi,NULL_TREE),call);
            break;
         case BUILT_IN_CTZG:
            call = fold_builtin_bit_query(self,loc, fcode, hi, NULL_TREE);
            call = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,PLUS_EXPR, integer_type_node, call,
                  mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,integer_type_node, MAX_FIXED_MODE_SIZE / 2));
            if (arg2)
               call = fold_build3 (COND_EXPR, integer_type_node,
            mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,NE_EXPR, boolean_type_node,hi,
                  mtcs_tree_build_zero_cst/*!build_zero_cst*/(mtcsTree,type)), call, arg2);
            call = fold_build3 (COND_EXPR, integer_type_node,
            mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,NE_EXPR, boolean_type_node,lo,
                  mtcs_tree_build_zero_cst/*!build_zero_cst*/(mtcsTree,type)),fold_builtin_bit_query(self,loc, fcode, lo,NULL_TREE),call);
            break;
         case BUILT_IN_CLRSBG:
            tem = fold_builtin_bit_query(self,loc, fcode, lo, NULL_TREE);
            tem = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,PLUS_EXPR, integer_type_node,tem,
                  mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,integer_type_node,MAX_FIXED_MODE_SIZE / 2));
            tem = fold_build3 (COND_EXPR, integer_type_node,
                  mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,LT_EXPR, boolean_type_node,
                        mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,BIT_XOR_EXPR, type,lo, hi),
            mtcs_tree_build_zero_cst/*!build_zero_cst*/(mtcsTree,type)),
            mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,integer_type_node,MAX_FIXED_MODE_SIZE / 2 - 1),tem);
            call = fold_builtin_bit_query(self,loc, fcode, hi, NULL_TREE);
            call = save_expr (call);
            call = fold_build3 (COND_EXPR, integer_type_node,
                  mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,NE_EXPR, boolean_type_node,call,
                        mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,integer_type_node,MAX_FIXED_MODE_SIZE/ 2 - 1)),call, tem);
            break;
         case BUILT_IN_FFSG:
            call = fold_builtin_bit_query(self,loc, fcode, hi, NULL_TREE);
            call = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,PLUS_EXPR, integer_type_node, call,
            mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,integer_type_node,
            MAX_FIXED_MODE_SIZE / 2));
            call = fold_build3 (COND_EXPR, integer_type_node,
            mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,NE_EXPR, boolean_type_node,
            hi, mtcs_tree_build_zero_cst/*!build_zero_cst*/(mtcsTree,type)),
            call, integer_zero_node);
            call = fold_build3 (COND_EXPR, integer_type_node,
            mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,NE_EXPR, boolean_type_node,
               lo, mtcs_tree_build_zero_cst/*!build_zero_cst*/(mtcsTree,type)),
               fold_builtin_bit_query(self,loc, fcode, lo, NULL_TREE),call);
            break;
         case BUILT_IN_PARITYG:
            call = fold_builtin_bit_query(self,loc, fcode,
                  mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,BIT_XOR_EXPR, type, lo, hi), NULL_TREE);
            break;
         case BUILT_IN_POPCOUNTG:
            call = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,PLUS_EXPR, integer_type_node,
            fold_builtin_bit_query(self,loc, fcode, hi, NULL_TREE),
            fold_builtin_bit_query(self,loc, fcode, lo, NULL_TREE));
            break;
         default:
            gcc_unreachable ();
      }
   }else{
      /* Only keep second argument to IFN_CLZ/IFN_CTZ if it is the
      value defined at zero during GIMPLE, or for large/huge _BitInt
      (which are then lowered during bitint lowering).  */
      if (arg2 && TREE_CODE (TREE_TYPE (arg0)) != BITINT_TYPE){
         int val;
         if (fcode == BUILT_IN_CLZG){
            if (CLZ_DEFINED_VALUE_AT_ZERO (SCALAR_TYPE_MODE (arg0_type), val) != 2 || wi::to_widest (arg2) != val)
               arg2 = NULL_TREE;
         }else if (CTZ_DEFINED_VALUE_AT_ZERO (SCALAR_TYPE_MODE (arg0_type), val) != 2 || wi::to_widest (arg2) != val)
            arg2 = NULL_TREE;
         if (!mtcs_internal_fn_direct_internal_fn_supported_p/*!direct_internal_fn_supported_p*/(mtcsInternalFn,
               ifn, arg0_type, OPTIMIZE_FOR_BOTH))
            arg2 = NULL_TREE;
         if (arg2 == NULL_TREE)
            arg0 = save_expr (arg0);
      }
      if (fcodei == END_BUILTINS || arg2)
         call = build_call_expr_internal_loc (loc, ifn, integer_type_node, arg2 ? 2 : 1, arg0, arg2);
      else
         call = mtcs_tree_build_call_expr/*!build_call_expr_loc*/(mtcsTree,loc, mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,fcodei), 1, arg0);
   }
   if (addend)
      call = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,PLUS_EXPR, integer_type_node, call,
            mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,integer_type_node, addend));
   if (arg1 && arg2 == NULL_TREE)
      call = fold_build3 (COND_EXPR, integer_type_node,
            mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,NE_EXPR, boolean_type_node,  arg0,
                  mtcs_tree_build_zero_cst/*!build_zero_cst*/(mtcsTree,arg0_type)), call, arg1);

   return call;
}


/* Fold a call to __builtin_constant_p, if we know its argument ARG will
   evaluate to a constant.  */
static tree fold_builtin_constant_p (tree arg)
{
   /* We return 1 for a numeric type that's known to be a constant
   value at compile-time or for an aggregate type that's a
   literal constant.  */
   STRIP_NOPS (arg);

   /* If we know this is a constant, emit the constant of one.  */
   if (CONSTANT_CLASS_P (arg) || (TREE_CODE (arg) == CONSTRUCTOR  && TREE_CONSTANT (arg)))
      return integer_one_node;
   if (TREE_CODE (arg) == ADDR_EXPR){
      tree op = TREE_OPERAND (arg, 0);
      if (TREE_CODE (op) == STRING_CST || (TREE_CODE (op) == ARRAY_REF
      && integer_zerop (TREE_OPERAND (op, 1))
      && TREE_CODE (TREE_OPERAND (op, 0)) == STRING_CST))
         return integer_one_node;
   }

   /* If this expression has side effects, show we don't know it to be a
   constant.  Likewise if it's a pointer or aggregate type since in
   those case we only want literals, since those are only optimized
   when generating RTL, not later.
   And finally, if we are compiling an initializer, not code, we
   need to return a definite result now; there's not going to be any
   more optimization done.  */
   if (TREE_SIDE_EFFECTS (arg)
   || AGGREGATE_TYPE_P (TREE_TYPE (arg))
   || POINTER_TYPE_P (TREE_TYPE (arg))
   || cfun == 0
   || folding_initializer
   || force_folding_builtin_constant_p)
      return integer_zero_node;

   return NULL_TREE;
}

/* Fold a call EXPR (which may be null) to __builtin_strlen with argument
   ARG.  */
static tree fold_builtin_strlen (MtcsBuiltins *self,location_t loc, tree expr, tree type, tree arg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

   if (!validate_arg (arg, POINTER_TYPE))
      return NULL_TREE;
   else{
      c_strlen_data lendata = { };
      tree len = c_strlen (arg, 0, &lendata);

      if (len)
         return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, type, len);

      /* TODO: Move this to gimple-ssa-warn-access once the pass runs
      also early enough to detect invalid reads in multimensional
      arrays and struct members.  */
      if (!lendata.decl)
         c_strlen (arg, 1, &lendata);

      if (lendata.decl){
         if (EXPR_HAS_LOCATION (arg))
            loc = EXPR_LOCATION (arg);
         else if (loc == UNKNOWN_LOCATION)
            loc = input_location;
         warn_string_no_nul (loc, expr, "strlen", arg, lendata.decl);
      }

      return NULL_TREE;
   }
}

/* Fold a call to fabs, fabsf or fabsl with argument ARG.  */
static tree fold_builtin_fabs (MtcsBuiltins *self,location_t loc, tree arg, tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

   if (!validate_arg (arg, REAL_TYPE))
      return NULL_TREE;

   arg = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, type, arg);
   return mtcs_const_fold_build1_loc/*!fold_build1_loc*/(mtcsConst,loc, ABS_EXPR, type, arg);
}

/* Fold a call to abs, labs, llabs, imaxabs, uabs, ulabs, ullabs or uimaxabs
   with argument ARG.  */

static tree fold_builtin_abs (MtcsBuiltins *self,location_t loc, tree arg, tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

   if (!validate_arg (arg, INTEGER_TYPE))
      return NULL_TREE;

   if (TYPE_UNSIGNED (type)){
      if (TYPE_PRECISION (TREE_TYPE (arg)) != TYPE_PRECISION (type) || TYPE_UNSIGNED (TREE_TYPE (arg)))
         return NULL_TREE;
      return mtcs_const_fold_build1_loc/*!fold_build1_loc*/(mtcsConst,loc, ABSU_EXPR, type, arg);
   }
   arg = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, type, arg);
   return mtcs_const_fold_build1_loc/*!fold_build1_loc*/(mtcsConst,loc, ABS_EXPR, type, arg);
}


/* Fold a call to builtin carg(a+bi) -> atan2(b,a).  */
static tree fold_builtin_carg (MtcsBuiltins *self,location_t loc, tree arg, tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   if (validate_arg (arg, COMPLEX_TYPE) && SCALAR_FLOAT_TYPE_P (TREE_TYPE (TREE_TYPE (arg)))){
      tree atan2_fn = mathfn_built_in (type, BUILT_IN_ATAN2);
      if (atan2_fn){
         tree new_arg = builtin_save_expr (arg);
         tree r_arg = fold_build1_loc (loc, REALPART_EXPR, type, new_arg);
         tree i_arg = fold_build1_loc (loc, IMAGPART_EXPR, type, new_arg);
         return mtcs_tree_build_call_expr/*!build_call_expr_loc*/(mtcsTree,loc, atan2_fn, 2, i_arg, r_arg);
      }
   }
   return NULL_TREE;
}

/* Fold a call to builtin isascii with argument ARG.  */
static tree fold_builtin_isascii (MtcsBuiltins *self,location_t loc, tree arg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   if (!validate_arg (arg, INTEGER_TYPE))
      return NULL_TREE;
   else{
      /* Transform isascii(c) -> ((c & ~0x7f) == 0).  */
      arg = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,BIT_AND_EXPR, integer_type_node, arg,
            mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,integer_type_node, ~ HOST_WIDE_INT_UC (0x7f)));
      return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, EQ_EXPR, integer_type_node,arg, integer_zero_node);
   }
}


/* Fold a call to builtin toascii with argument ARG.  */

static tree fold_builtin_toascii (MtcsBuiltins *self,location_t loc, tree arg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

  if (!validate_arg (arg, INTEGER_TYPE))
    return NULL_TREE;

  /* Transform toascii(c) -> (c & 0x7f).  */
  return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, BIT_AND_EXPR, integer_type_node, arg,
           mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,integer_type_node, 0x7f));
}

/* Fold a call to builtin isdigit with argument ARG.  */
static tree fold_builtin_isdigit (MtcsBuiltins *self, location_t loc, tree arg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   if (!validate_arg (arg, INTEGER_TYPE))
      return NULL_TREE;
   else{
      /* Transform isdigit(c) -> (unsigned)(c) - '0' <= 9.  */
      /* According to the C standard, isdigit is unaffected by locale.
      However, it definitely is affected by the target character set.  */
      unsigned HOST_WIDE_INT target_digit0 = lang_hooks.to_target_charset ('0');

      if (target_digit0 == 0)
         return NULL_TREE;

      arg = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, unsigned_type_node, arg);
      arg = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,MINUS_EXPR, unsigned_type_node, arg, mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,unsigned_type_node, target_digit0));
      return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, LE_EXPR, integer_type_node, arg, mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,unsigned_type_node, 9));
   }
}

/* Fold a call to __builtin_isnan(), __builtin_isinf, __builtin_finite.
   ARG is the argument for the call.  */
static tree fold_builtin_classify (MtcsBuiltins *self,location_t loc, tree fndecl, tree arg, int builtin_index)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree type = TREE_TYPE (TREE_TYPE (fndecl));

   if (!validate_arg (arg, REAL_TYPE))
      return NULL_TREE;

   switch (builtin_index){
      case BUILT_IN_ISINF:
         if (tree_expr_infinite_p (arg))
            return omit_one_operand_loc (loc, type, integer_one_node, arg);
         if (!tree_expr_maybe_infinite_p (arg))
            return omit_one_operand_loc (loc, type, integer_zero_node, arg);
         return NULL_TREE;

      case BUILT_IN_ISINF_SIGN:
      {
         /* isinf_sign(x) -> isinf(x) ? (signbit(x) ? -1 : 1) : 0 */
         /* In a boolean context, GCC will fold the inner COND_EXPR to
         1.  So e.g. "if (isinf_sign(x))" would be folded to just
         "if (isinf(x) ? 1 : 0)" which becomes "if (isinf(x))". */
         tree signbit_fn = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_SIGNBIT);
         tree isinf_fn = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_ISINF);
         tree tmp = NULL_TREE;

         arg = builtin_save_expr (arg);

         if (signbit_fn && isinf_fn){
            tree signbit_call = mtcs_tree_build_call_expr/*!build_call_expr_loc*/(mtcsTree,loc, signbit_fn, 1, arg);
            tree isinf_call = mtcs_tree_build_call_expr/*!build_call_expr_loc*/(mtcsTree,loc, isinf_fn, 1, arg);

            signbit_call = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, NE_EXPR, integer_type_node,signbit_call, integer_zero_node);
            isinf_call = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, NE_EXPR, integer_type_node, isinf_call, integer_zero_node);

            tmp = fold_build3_loc (loc, COND_EXPR, integer_type_node, signbit_call, integer_minus_one_node, integer_one_node);
            tmp = fold_build3_loc (loc, COND_EXPR, integer_type_node,  isinf_call, tmp, integer_zero_node);
         }

         return tmp;
      }

      case BUILT_IN_ISFINITE:
         if (tree_expr_finite_p (arg))
            return omit_one_operand_loc (loc, type, integer_one_node, arg);
         if (tree_expr_nan_p (arg) || tree_expr_infinite_p (arg))
            return omit_one_operand_loc (loc, type, integer_zero_node, arg);
         return NULL_TREE;

      case BUILT_IN_ISNAN:
         if (tree_expr_nan_p (arg))
            return omit_one_operand_loc (loc, type, integer_one_node, arg);
         if (!tree_expr_maybe_nan_p (arg))
            return omit_one_operand_loc (loc, type, integer_zero_node, arg);

         {
            bool is_ibm_extended = mtcs_mode_is_composite_p/*!MODE_COMPOSITE_P*/(mtcsMode,TYPE_MODE (TREE_TYPE (arg)));
            if (is_ibm_extended){
            /* NaN and Inf are encoded in the high-order double value
            only.  The low-order value is not significant.  */
            arg = fold_build1_loc (loc, NOP_EXPR, double_type_node, arg);
            }
         }
         arg = builtin_save_expr (arg);
         return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, UNORDERED_EXPR, type, arg, arg);

      case BUILT_IN_ISSIGNALING:
         /* Folding to true for REAL_CST is done in fold_const_call_ss.
         Don't use tree_expr_signaling_nan_p (arg) -> integer_one_node
         and !tree_expr_maybe_signaling_nan_p (arg) -> integer_zero_node
         here, so there is some possibility of __builtin_issignaling working
         without -fsignaling-nans.  Especially when -fno-signaling-nans is
         the default.  */
         if (!tree_expr_maybe_nan_p (arg))
            return omit_one_operand_loc (loc, type, integer_zero_node, arg);
         return NULL_TREE;

      default:
         gcc_unreachable ();
   }
}

/* Given a location LOC, an interclass builtin function decl FNDECL
   and its single argument ARG, return an folded expression computing
   the same, or NULL_TREE if we either couldn't or didn't want to fold
   (the latter happen if there's an RTL instruction available).  */
static tree fold_builtin_interclass_mathfn (MtcsBuiltins *self,location_t loc, tree fndecl, tree arg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   machine_mode mode;

   if (!validate_arg (arg, REAL_TYPE))
      return NULL_TREE;

   if (interclass_mathfn_icode(self,arg, fndecl) != CODE_FOR_nothing)
      return NULL_TREE;

   mode = TYPE_MODE (TREE_TYPE (arg));

   bool is_ibm_extended = mtcs_mode_is_composite_p/*!MODE_COMPOSITE_P*/(mtcsMode,mode);

   /* If there is no optab, try generic code.  */
   switch (DECL_FUNCTION_CODE (fndecl)){
      tree result;

      CASE_FLT_FN (BUILT_IN_ISINF):
      {
         /* isinf(x) -> isgreater(fabs(x),DBL_MAX).  */
         tree const isgr_fn = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_ISGREATER);
         tree type = TREE_TYPE (arg);
         REAL_VALUE_TYPE r;
         char buf[128];

         if (is_ibm_extended){
            /* NaN and Inf are encoded in the high-order double value
            only.  The low-order value is not significant.  */
            type = double_type_node;
            mode = mtcsMode->modes.M_DFmode;
            arg = fold_build1_loc (loc, NOP_EXPR, type, arg);
         }
         get_max_float (mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,mode), buf, sizeof (buf), false);
         real_from_string3 (&r, buf, mode);
         result = mtcs_tree_build_call_expr/*!build_call_expr*/(mtcsTree,isgr_fn, 2, fold_build1_loc (loc, ABS_EXPR, type, arg), build_real (type, r));
         return result;
      }
      CASE_FLT_FN (BUILT_IN_FINITE):
      case BUILT_IN_ISFINITE:
      {
         /* isfinite(x) -> islessequal(fabs(x),DBL_MAX).  */
         tree const isle_fn = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_ISLESSEQUAL);
         tree type = TREE_TYPE (arg);
         REAL_VALUE_TYPE r;
         char buf[128];

         if (is_ibm_extended){
            /* NaN and Inf are encoded in the high-order double value
            only.  The low-order value is not significant.  */
            type = double_type_node;
            mode = mtcsMode->modes.M_DFmode;
            arg = fold_build1_loc (loc, NOP_EXPR, type, arg);
         }
         get_max_float (mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,mode), buf, sizeof (buf), false);
         real_from_string3 (&r, buf, mode);
         result = mtcs_tree_build_call_expr/*!build_call_expr*/(mtcsTree,isle_fn, 2,fold_build1_loc (loc, ABS_EXPR, type, arg),build_real (type, r));
         /*result = fold_build2_loc(loc, UNGT_EXPR,
         TREE_TYPE (TREE_TYPE (fndecl)),
         fold_build1_loc (loc, ABS_EXPR, type, arg),
         build_real (type, r));
         result = fold_build1_loc (loc, TRUTH_NOT_EXPR,
         TREE_TYPE (TREE_TYPE (fndecl)),
         result);*/
         return result;
      }
      case BUILT_IN_ISNORMAL:
      {
         /* isnormal(x) -> isgreaterequal(fabs(x),DBL_MIN) &
         islessequal(fabs(x),DBL_MAX).  */
         tree const isle_fn = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_ISLESSEQUAL);
         tree type = TREE_TYPE (arg);
         tree orig_arg, max_exp, min_exp;
         machine_mode orig_mode = mode;
         REAL_VALUE_TYPE rmax, rmin;
         char buf[128];

         orig_arg = arg = builtin_save_expr (arg);
         if (is_ibm_extended){
            /* Use double to test the normal range of IBM extended
            precision.  Emin for IBM extended precision is
            different to emin for IEEE double, being 53 higher
            since the low double exponent is at least 53 lower
            than the high double exponent.  */
            type = double_type_node;
            mode = mtcsMode->modes.M_DFmode;
            arg = fold_build1_loc (loc, NOP_EXPR, type, arg);
         }
         arg = fold_build1_loc (loc, ABS_EXPR, type, arg);

         get_max_float (mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,mode), buf, sizeof (buf), false);
         real_from_string3 (&rmax, buf, mode);
         if (DECIMAL_FLOAT_MODE_P (mode))
            sprintf (buf, "1E%d", mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,orig_mode)->emin - 1);
         else
            sprintf (buf, "0x1p%d", mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,orig_mode)->emin - 1);
         real_from_string3 (&rmin, buf, orig_mode);
         max_exp = build_real (type, rmax);
         min_exp = build_real (type, rmin);

         max_exp = mtcs_tree_build_call_expr/*!build_call_expr*/(mtcsTree,isle_fn, 2, arg, max_exp);
         if (is_ibm_extended){
            /* Testing the high end of the range is done just using
            the high double, using the same test as isfinite().
            For the subnormal end of the range we first test the
            high double, then if its magnitude is equal to the
            limit of 0x1p-969, we test whether the low double is
            non-zero and opposite sign to the high double.  */
            tree const islt_fn = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_ISLESS);
            tree const isgt_fn = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_ISGREATER);
            tree gt_min = mtcs_tree_build_call_expr/*!build_call_expr*/(mtcsTree,isgt_fn, 2, arg, min_exp);
            tree eq_min = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,EQ_EXPR, integer_type_node,arg, min_exp);
            tree as_complex = build1 (VIEW_CONVERT_EXPR,complex_double_type_node, orig_arg);
            tree hi_dbl = build1 (REALPART_EXPR, type, as_complex);
            tree lo_dbl = build1 (IMAGPART_EXPR, type, as_complex);
            tree zero = build_real (type, dconst0);
            tree hilt = mtcs_tree_build_call_expr/*!build_call_expr*/(mtcsTree,islt_fn, 2, hi_dbl, zero);
            tree lolt = mtcs_tree_build_call_expr/*!build_call_expr*/(mtcsTree,islt_fn, 2, lo_dbl, zero);
            tree logt = mtcs_tree_build_call_expr/*!build_call_expr*/(mtcsTree,isgt_fn, 2, lo_dbl, zero);
            tree ok_lo = fold_build1 (TRUTH_NOT_EXPR, integer_type_node,fold_build3 (COND_EXPR,integer_type_node,hilt, logt, lolt));
            eq_min = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,TRUTH_ANDIF_EXPR, integer_type_node,eq_min, ok_lo);
            min_exp = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,TRUTH_ORIF_EXPR, integer_type_node, gt_min, eq_min);
         }else{
            tree const isge_fn = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,BUILT_IN_ISGREATEREQUAL);
            min_exp = mtcs_tree_build_call_expr/*!build_call_expr*/(mtcsTree,isge_fn, 2, arg, min_exp);
         }
         result = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,BIT_AND_EXPR, integer_type_node,max_exp, min_exp);
         return result;
      }
      default:
         break;
   }

   return NULL_TREE;
}


/* Fold a call to built-in function FNDECL with 1 argument, ARG0.
   This function returns NULL_TREE if no simplification was possible.  */
static tree fold_builtin_1 (MtcsBuiltins *self,location_t loc, tree expr, tree fndecl, tree arg0)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree type = TREE_TYPE (TREE_TYPE (fndecl));
   enum built_in_function fcode = DECL_FUNCTION_CODE (fndecl);

   if (error_operand_p (arg0))
      return NULL_TREE;

   if (tree ret = fold_const_call (as_combined_fn (fcode), type, arg0))
      return ret;

   switch (fcode){
      case BUILT_IN_CONSTANT_P:
      {
         tree val = fold_builtin_constant_p (arg0);

         /* Gimplification will pull the CALL_EXPR for the builtin out of
         an if condition.  When not optimizing, we'll not CSE it back.
         To avoid link error types of regressions, return false now.  */
         if (!val && !optimize)
            val = integer_zero_node;

         return val;
      }

      case BUILT_IN_CLASSIFY_TYPE:
         return fold_builtin_classify_type(self,arg0);

      case BUILT_IN_STRLEN:
         return fold_builtin_strlen(self,loc, expr, type, arg0);

      CASE_FLT_FN (BUILT_IN_FABS):
      CASE_FLT_FN_FLOATN_NX (BUILT_IN_FABS):
      case BUILT_IN_FABSD32:
      case BUILT_IN_FABSD64:
      case BUILT_IN_FABSD128:
      case BUILT_IN_FABSD64X:
         return fold_builtin_fabs(self,loc, arg0, type);

      case BUILT_IN_ABS:
      case BUILT_IN_LABS:
      case BUILT_IN_LLABS:
      case BUILT_IN_IMAXABS:
      case BUILT_IN_UABS:
      case BUILT_IN_ULABS:
      case BUILT_IN_ULLABS:
      case BUILT_IN_UIMAXABS:
         return fold_builtin_abs(self,loc, arg0, type);

      CASE_FLT_FN (BUILT_IN_CONJ):
         if (validate_arg (arg0, COMPLEX_TYPE) && TREE_CODE (TREE_TYPE (TREE_TYPE (arg0))) == REAL_TYPE)
            return mtcs_const_fold_build1_loc/*!fold_build1_loc*/(mtcsConst,loc, CONJ_EXPR, type, arg0);
         break;

      CASE_FLT_FN (BUILT_IN_CREAL):
         if (validate_arg (arg0, COMPLEX_TYPE)  && TREE_CODE (TREE_TYPE (TREE_TYPE (arg0))) == REAL_TYPE)
            return non_lvalue_loc (loc, mtcs_const_fold_build1_loc/*!fold_build1_loc*/(mtcsConst,loc, REALPART_EXPR, type, arg0));
         break;

      CASE_FLT_FN (BUILT_IN_CIMAG):
         if (validate_arg (arg0, COMPLEX_TYPE) && TREE_CODE (TREE_TYPE (TREE_TYPE (arg0))) == REAL_TYPE)
            return non_lvalue_loc (loc, mtcs_const_fold_build1_loc/*!fold_build1_loc*/(mtcsConst,loc, IMAGPART_EXPR, type, arg0));
         break;

      CASE_FLT_FN (BUILT_IN_CARG):
      CASE_FLT_FN_FLOATN_NX (BUILT_IN_CARG):
         return fold_builtin_carg(self,loc, arg0, type);

      case BUILT_IN_ISASCII:
         return fold_builtin_isascii(self,loc, arg0);

      case BUILT_IN_TOASCII:
         return fold_builtin_toascii(self,loc, arg0);

      case BUILT_IN_ISDIGIT:
         return fold_builtin_isdigit(self,loc, arg0);

      CASE_FLT_FN (BUILT_IN_FINITE):
      case BUILT_IN_FINITED32:
      case BUILT_IN_FINITED64:
      case BUILT_IN_FINITED128:
      case BUILT_IN_ISFINITE:
      {
         tree ret = fold_builtin_classify(self,loc, fndecl, arg0, BUILT_IN_ISFINITE);
         if (ret)
            return ret;
         return fold_builtin_interclass_mathfn(self,loc, fndecl, arg0);
      }

      CASE_FLT_FN (BUILT_IN_ISINF):
      case BUILT_IN_ISINFD32:
      case BUILT_IN_ISINFD64:
      case BUILT_IN_ISINFD128:
      {
         tree ret = fold_builtin_classify(self,loc, fndecl, arg0, BUILT_IN_ISINF);
         if (ret)
            return ret;
         return fold_builtin_interclass_mathfn(self,loc, fndecl, arg0);
      }

      case BUILT_IN_ISNORMAL:
         return fold_builtin_interclass_mathfn(self,loc, fndecl, arg0);

      case BUILT_IN_ISINF_SIGN:
         return fold_builtin_classify(self,loc, fndecl, arg0, BUILT_IN_ISINF_SIGN);

      CASE_FLT_FN (BUILT_IN_ISNAN):
      case BUILT_IN_ISNAND32:
      case BUILT_IN_ISNAND64:
      case BUILT_IN_ISNAND128:
         return fold_builtin_classify(self,loc, fndecl, arg0, BUILT_IN_ISNAN);

      case BUILT_IN_ISSIGNALING:
         return fold_builtin_classify(self,loc, fndecl, arg0, BUILT_IN_ISSIGNALING);

      case BUILT_IN_FREE:
         if (integer_zerop (arg0))
            return build_empty_stmt (loc);
         break;

      case BUILT_IN_CLZG:
      case BUILT_IN_CTZG:
      case BUILT_IN_CLRSBG:
      case BUILT_IN_FFSG:
      case BUILT_IN_PARITYG:
      case BUILT_IN_POPCOUNTG:
         return fold_builtin_bit_query(self,loc, fcode, arg0, NULL_TREE);

      default:
         break;
   }
   return NULL_TREE;
}

/* Helper function for do_mpfr_arg*().  Ensure M is a normal number
   and no overflow/underflow occurred.  INEXACT is true if M was not
   exactly calculated.  TYPE is the tree type for the result.  This
   function assumes that you cleared the MPFR flags and then
   calculated M to see if anything subsequently set a flag prior to
   entering this function.  Return NULL_TREE if any checks fail.  */
static tree do_mpfr_ckconv (MtcsBuiltins *self,mpfr_srcptr m, tree type, int inexact)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);

   /* Proceed iff we get a normal number, i.e. not NaN or Inf and no
   overflow/underflow occurred.  If -frounding-math, proceed iff the
   result of calling FUNC was exact.  */
   if (mpfr_number_p (m) && !mpfr_overflow_p () && !mpfr_underflow_p ()
   && (!flag_rounding_math || !inexact)){
      REAL_VALUE_TYPE rr;

      real_from_mpfr (&rr, m, type, MPFR_RNDN);
      /* Proceed iff GCC's REAL_VALUE_TYPE can hold the MPFR value,
      check for overflow/underflow.  If the REAL_VALUE_TYPE is zero
      but the mpfr_t is not, then we underflowed in the
      conversion.  */
      if (real_isfinite (&rr) && (rr.cl == rvc_zero) == (mpfr_zero_p (m) != 0)){
         REAL_VALUE_TYPE rmode;

         mtcs_real_real_convert/*!real_convert*/(mtcsReal,&rmode, TYPE_MODE (type), &rr);
         /* Proceed iff the specified mode can hold the value.  */
         if (real_identical (&rmode, &rr))
            return build_real (type, rmode);
      }
   }
   return NULL_TREE;
}

/* If ARG is a REAL_CST, call mpfr_lgamma() on it and return the
   resulting value as a tree with type TYPE.  The mpfr precision is
   set to the precision of TYPE.  We assume that this mpfr function
   returns zero if the result could be calculated exactly within the
   requested precision.  In addition, the integer pointer represented
   by ARG_SG will be dereferenced and set to the appropriate signgam
   (-1,1) value.  */
static tree do_mpfr_lgamma_r (MtcsBuiltins *self,tree arg, tree arg_sg, tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree result = NULL_TREE;

   STRIP_NOPS (arg);

   /* To proceed, MPFR must exactly represent the target floating point
   format, which only happens when the target base equals two.  Also
   verify ARG is a constant and that ARG_SG is an int pointer.  */
   if (mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,TYPE_MODE (type))->b == 2
   && TREE_CODE (arg) == REAL_CST && !TREE_OVERFLOW (arg)
   && TREE_CODE (TREE_TYPE (arg_sg)) == POINTER_TYPE
   && TYPE_MAIN_VARIANT (TREE_TYPE (TREE_TYPE (arg_sg))) == integer_type_node){
      const REAL_VALUE_TYPE *const ra = TREE_REAL_CST_PTR (arg);

      /* In addition to NaN and Inf, the argument cannot be zero or a
      negative integer.  */
      if (real_isfinite (ra)  && ra->cl != rvc_zero  && !(real_isneg (ra) && real_isinteger (ra, TYPE_MODE (type)))){
         const struct real_format *fmt = mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,TYPE_MODE (type));
         const int prec = fmt->p;
         const mpfr_rnd_t rnd = fmt->round_towards_zero? MPFR_RNDZ : MPFR_RNDN;
         int inexact, sg;
         tree result_lg;

         auto_mpfr m (prec);
         mpfr_from_real (m, ra, MPFR_RNDN);
         mpfr_clear_flags ();
         inexact = mpfr_lgamma (m, &sg, m, rnd);
         result_lg = do_mpfr_ckconv(self,m, type, inexact);
         if (result_lg){
            tree result_sg;

            /* Dereference the arg_sg pointer argument.  */
            arg_sg = build_fold_indirect_ref (arg_sg);
            /* Assign the signgam value into *arg_sg. */
            result_sg = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,MODIFY_EXPR, TREE_TYPE (arg_sg), arg_sg, mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (arg_sg), sg));
            TREE_SIDE_EFFECTS (result_sg) = 1;
            /* Combine the signgam assignment with the lgamma result.  */
            result = non_lvalue (mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,COMPOUND_EXPR, type,result_sg, result_lg));
         }
      }
   }

   return result;
}

/* Fold a call to builtin frexp, we can assume the base is 2.  */
static tree fold_builtin_frexp (MtcsBuiltins *self,location_t loc, tree arg0, tree arg1, tree rettype)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   if (! validate_arg (arg0, REAL_TYPE) || ! validate_arg (arg1, POINTER_TYPE))
      return NULL_TREE;

   STRIP_NOPS (arg0);

   if (!(TREE_CODE (arg0) == REAL_CST && ! TREE_OVERFLOW (arg0)))
      return NULL_TREE;

   arg1 = build_fold_indirect_ref_loc (loc, arg1);

   /* Proceed if a valid pointer type was passed in.  */
   if (TYPE_MAIN_VARIANT (TREE_TYPE (arg1)) == integer_type_node){
      const REAL_VALUE_TYPE *const value = TREE_REAL_CST_PTR (arg0);
      tree frac, exp, res;

      switch (value->cl){
         case rvc_zero:
         case rvc_nan:
         case rvc_inf:
            /* For +-0, return (*exp = 0, +-0).  */
            /* For +-NaN or +-Inf, *exp is unspecified, but something should
            be stored there so that it isn't read from uninitialized object.
            As glibc and newlib store *exp = 0 for +-Inf/NaN, storing
            0 here as well is easiest.  */
            exp = integer_zero_node;
            frac = arg0;
            break;
         case rvc_normal:
         {
            /* Since the frexp function always expects base 2, and in
            GCC normalized significands are already in the range
            [0.5, 1.0), we have exactly what frexp wants.  */
            REAL_VALUE_TYPE frac_rvt = *value;
            SET_REAL_EXP (&frac_rvt, 0);
            frac = build_real (rettype, frac_rvt);
            exp = mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,integer_type_node, REAL_EXP (value));
         }
            break;
         default:
            gcc_unreachable ();
      }

      /* Create the COMPOUND_EXPR (*arg1 = trunc, frac). */
      arg1 = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, MODIFY_EXPR, rettype, arg1, exp);
      TREE_SIDE_EFFECTS (arg1) = 1;
      res = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, COMPOUND_EXPR, rettype, arg1, frac);
      suppress_warning (res, OPT_Wunused_value);
      return res;
   }

   return NULL_TREE;
}


/* Fold a call to builtin modf.  */
static tree fold_builtin_modf (MtcsBuiltins *self,location_t loc, tree arg0, tree arg1, tree rettype)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);

   if (! validate_arg (arg0, REAL_TYPE) || ! validate_arg (arg1, POINTER_TYPE))
      return NULL_TREE;

   STRIP_NOPS (arg0);

   if (!(TREE_CODE (arg0) == REAL_CST && ! TREE_OVERFLOW (arg0)))
      return NULL_TREE;

   arg1 = build_fold_indirect_ref_loc (loc, arg1);

   /* Proceed if a valid pointer type was passed in.  */
   if (TYPE_MAIN_VARIANT (TREE_TYPE (arg1)) == TYPE_MAIN_VARIANT (rettype)){
      const REAL_VALUE_TYPE *const value = TREE_REAL_CST_PTR (arg0);
      REAL_VALUE_TYPE trunc, frac;
      tree res;

      switch (value->cl){
         case rvc_nan:
         case rvc_zero:
            /* For +-NaN or +-0, return (*arg1 = arg0, arg0).  */
            trunc = frac = *value;
            break;
         case rvc_inf:
            /* For +-Inf, return (*arg1 = arg0, +-0).  */
            frac = dconst0;
            frac.sign = value->sign;
            trunc = *value;
            break;
         case rvc_normal:
            /* Return (*arg1 = trunc(arg0), arg0-trunc(arg0)).  */
            mtcs_real_real_trunc/*!real_trunc*/(mtcsReal,&trunc, VOIDmode, value);
            real_arithmetic (&frac, MINUS_EXPR, value, &trunc);
            /* If the original number was negative and already
            integral, then the fractional part is -0.0.  */
            if (value->sign && frac.cl == rvc_zero)
               frac.sign = value->sign;
            break;
      }

      /* Create the COMPOUND_EXPR (*arg1 = trunc, frac). */
      arg1 = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, MODIFY_EXPR, rettype, arg1,build_real (rettype, trunc));
      TREE_SIDE_EFFECTS (arg1) = 1;
      res = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, COMPOUND_EXPR, rettype, arg1,build_real (rettype, frac));
      suppress_warning (res, OPT_Wunused_value);
      return res;
   }

   return NULL_TREE;
}


/* Simplify a call to the strspn builtin.  S1 and S2 are the arguments
   to the call.

   Return NULL_TREE if no simplification was possible, otherwise return the
   simplified form of the call as a tree.

   The simplified form may be a constant or other expression which
   computes the same value, but in a more efficient manner (including
   calls to other builtin functions).

   The call may contain arguments which need to be evaluated, but
   which are not useful to determine the result of the call.  In
   this case we return a chain of COMPOUND_EXPRs.  The LHS of each
   COMPOUND_EXPR will be an argument which must be evaluated.
   COMPOUND_EXPRs are chained through their RHS.  The RHS of the last
   COMPOUND_EXPR in the chain will contain the tree for the simplified
   form of the builtin function call.  */

static tree fold_builtin_strspn (location_t loc, tree expr, tree s1, tree s2, tree type)
{
   if (!validate_arg (s1, POINTER_TYPE) || !validate_arg (s2, POINTER_TYPE))
      return NULL_TREE;

   if (!check_nul_terminated_array (expr, s1) || !check_nul_terminated_array (expr, s2))
      return NULL_TREE;

   const char *p1 = c_getstr (s1), *p2 = c_getstr (s2);

   /* If either argument is "", return NULL_TREE.  */
   if ((p1 && *p1 == '\0') || (p2 && *p2 == '\0'))
      /* Evaluate and ignore both arguments in case either one has
      side-effects.  */
      return omit_two_operands_loc (loc, type, size_zero_node, s1, s2);
   return NULL_TREE;
}

/* Simplify a call to the strcspn builtin.  S1 and S2 are the arguments
   to the call.

   Return NULL_TREE if no simplification was possible, otherwise return the
   simplified form of the call as a tree.

   The simplified form may be a constant or other expression which
   computes the same value, but in a more efficient manner (including
   calls to other builtin functions).

   The call may contain arguments which need to be evaluated, but
   which are not useful to determine the result of the call.  In
   this case we return a chain of COMPOUND_EXPRs.  The LHS of each
   COMPOUND_EXPR will be an argument which must be evaluated.
   COMPOUND_EXPRs are chained through their RHS.  The RHS of the last
   COMPOUND_EXPR in the chain will contain the tree for the simplified
   form of the builtin function call.  */

static tree fold_builtin_strcspn (MtcsBuiltins *self,location_t loc, tree expr, tree s1, tree s2, tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   if (!validate_arg (s1, POINTER_TYPE) || !validate_arg (s2, POINTER_TYPE))
      return NULL_TREE;

   if (!check_nul_terminated_array (expr, s1) || !check_nul_terminated_array (expr, s2))
      return NULL_TREE;
   /* If the first argument is "", return NULL_TREE.  */
   const char *p1 = c_getstr (s1);
   if (p1 && *p1 == '\0'){
      /* Evaluate and ignore argument s2 in case it has
      side-effects.  */
      return omit_one_operand_loc (loc, type, size_zero_node, s2);
   }
   /* If the second argument is "", return __builtin_strlen(s1).  */
   const char *p2 = c_getstr (s2);
   if (p2 && *p2 == '\0'){
      tree fn = builtin_decl_implicit (BUILT_IN_STRLEN);
      /* If the replacement _DECL isn't initialized, don't do the
      transformation.  */
      if (!fn)
         return NULL_TREE;
      return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,
            loc, type, mtcs_tree_build_call_expr/*!build_call_expr_loc*/(mtcsTree,loc, fn, 1, s1));
   }
   return NULL_TREE;
}

/* Simplify a call to the strpbrk builtin.  S1 and S2 are the arguments
   to the call, and TYPE is its return type.

   Return NULL_TREE if no simplification was possible, otherwise return the
   simplified form of the call as a tree.

   The simplified form may be a constant or other expression which
   computes the same value, but in a more efficient manner (including
   calls to other builtin functions).

   The call may contain arguments which need to be evaluated, but
   which are not useful to determine the result of the call.  In
   this case we return a chain of COMPOUND_EXPRs.  The LHS of each
   COMPOUND_EXPR will be an argument which must be evaluated.
   COMPOUND_EXPRs are chained through their RHS.  The RHS of the last
   COMPOUND_EXPR in the chain will contain the tree for the simplified
   form of the builtin function call.  */
static tree fold_builtin_strpbrk (MtcsBuiltins *self,location_t loc, tree, tree s1, tree s2, tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   if (!validate_arg (s1, POINTER_TYPE) || !validate_arg (s2, POINTER_TYPE))
      return NULL_TREE;

   tree fn;
   const char *p1, *p2;

   p2 = c_getstr (s2);
   if (p2 == NULL)
      return NULL_TREE;

   p1 = c_getstr (s1);
   if (p1 != NULL){
      const char *r = strpbrk (p1, p2);
      tree tem;

      if (r == NULL)
         return mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (s1), 0);

      /* Return an offset into the constant string argument.  */
      tem = fold_build_pointer_plus_hwi_loc (loc, s1, r - p1);
      return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, type, tem);
   }

   if (p2[0] == '\0')
      /* strpbrk(x, "") == NULL.
      Evaluate and ignore s1 in case it had side-effects.  */
      return omit_one_operand_loc (loc, type, integer_zero_node, s1);

   if (p2[1] != '\0')
      return NULL_TREE;  /* Really call strpbrk.  */

   fn = builtin_decl_implicit (BUILT_IN_STRCHR);
   if (!fn)
      return NULL_TREE;
   /* New argument list transforming strpbrk(s1, s2) to
   strchr(s1, s2[0]).  */
   return mtcs_tree_build_call_expr/*!build_call_expr_loc*/(mtcsTree,
         loc, fn, 2, s1, mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,integer_type_node, p2[0]));
}

/* Fold a call to an unordered comparison function such as
   __builtin_isgreater().  FNDECL is the FUNCTION_DECL for the function
   being called and ARG0 and ARG1 are the arguments for the call.
   UNORDERED_CODE and ORDERED_CODE are comparison codes that give
   the opposite of the desired result.  UNORDERED_CODE is used
   for modes that can hold NaNs and ORDERED_CODE is used for
   the rest.  */
static tree fold_builtin_unordered_cmp (MtcsBuiltins *self,location_t loc, tree fndecl, tree arg0, tree arg1,
             enum tree_code unordered_code,
             enum tree_code ordered_code)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree type = TREE_TYPE (TREE_TYPE (fndecl));
   enum tree_code code;
   tree type0, type1;
   enum tree_code code0, code1;
   tree cmp_type = NULL_TREE;

   type0 = TREE_TYPE (arg0);
   type1 = TREE_TYPE (arg1);

   code0 = TREE_CODE (type0);
   code1 = TREE_CODE (type1);

   if (code0 == REAL_TYPE && code1 == REAL_TYPE)
   /* Choose the wider of two real types.  */
      cmp_type = TYPE_PRECISION (type0) >= TYPE_PRECISION (type1) ? type0 : type1;
   else if (code0 == REAL_TYPE  && (code1 == INTEGER_TYPE || code1 == BITINT_TYPE))
      cmp_type = type0;
   else if ((code0 == INTEGER_TYPE || code0 == BITINT_TYPE)  && code1 == REAL_TYPE)
      cmp_type = type1;

   arg0 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, cmp_type, arg0);
   arg1 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, cmp_type, arg1);

   if (unordered_code == UNORDERED_EXPR){
      if (tree_expr_nan_p (arg0) || tree_expr_nan_p (arg1))
         return omit_two_operands_loc (loc, type, integer_one_node, arg0, arg1);
      if (!tree_expr_maybe_nan_p (arg0) && !tree_expr_maybe_nan_p (arg1))
         return omit_two_operands_loc (loc, type, integer_zero_node, arg0, arg1);
      return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, UNORDERED_EXPR, type, arg0, arg1);
   }

   code = (tree_expr_maybe_nan_p (arg0) || tree_expr_maybe_nan_p (arg1))? unordered_code : ordered_code;
   return fold_build1_loc (loc, TRUTH_NOT_EXPR, type, mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, code, type, arg0, arg1));
}


/* Fold a call to __builtin_iseqsig().  ARG0 and ARG1 are the arguments.
   After choosing the wider floating-point type for the comparison,
   the code is folded to:
     SAVE_EXPR<ARG0> >= SAVE_EXPR<ARG1> && SAVE_EXPR<ARG0> <= SAVE_EXPR<ARG1>  */
static tree fold_builtin_iseqsig (MtcsBuiltins *self, location_t loc, tree arg0, tree arg1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree type0, type1;
   enum tree_code code0, code1;
   tree cmp1, cmp2, cmp_type = NULL_TREE;

   type0 = TREE_TYPE (arg0);
   type1 = TREE_TYPE (arg1);

   code0 = TREE_CODE (type0);
   code1 = TREE_CODE (type1);

   if (code0 == REAL_TYPE && code1 == REAL_TYPE)
      /* Choose the wider of two real types.  */
      cmp_type = TYPE_PRECISION (type0) >= TYPE_PRECISION (type1) ? type0 : type1;
   else if (code0 == REAL_TYPE  && (code1 == INTEGER_TYPE || code1 == BITINT_TYPE))
      cmp_type = type0;
   else if ((code0 == INTEGER_TYPE || code0 == BITINT_TYPE)  && code1 == REAL_TYPE)
      cmp_type = type1;

   arg0 = builtin_save_expr (mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, cmp_type, arg0));
   arg1 = builtin_save_expr (mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, cmp_type, arg1));

   cmp1 = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, GE_EXPR, integer_type_node, arg0, arg1);
   cmp2 = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, LE_EXPR, integer_type_node, arg0, arg1);

   return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, TRUTH_AND_EXPR, integer_type_node, cmp1, cmp2);
}


/* Fold a call to __builtin_object_size with arguments PTR and OST,
   if possible.  */
static tree fold_builtin_object_size (MtcsBuiltins *self, tree ptr, tree ost, enum built_in_function fcode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree bytes;
   int object_size_type;

   if (!validate_arg (ptr, POINTER_TYPE) || !validate_arg (ost, INTEGER_TYPE))
      return NULL_TREE;

   STRIP_NOPS (ost);

   if (TREE_CODE (ost) != INTEGER_CST || tree_int_cst_sgn (ost) < 0 || compare_tree_int (ost, 3) > 0)
      return NULL_TREE;

   object_size_type = tree_to_shwi (ost);

   /* __builtin_object_size doesn't evaluate side-effects in its arguments;
   if there are any side-effects, it returns (size_t) -1 for types 0 and 1
   and (size_t) 0 for types 2 and 3.  */
   if (TREE_SIDE_EFFECTS (ptr))
      return build_int_cst_type (size_type_node, object_size_type < 2 ? -1 : 0);

   if (fcode == BUILT_IN_DYNAMIC_OBJECT_SIZE)
      object_size_type |= OST_DYNAMIC;

   if (TREE_CODE (ptr) == ADDR_EXPR){
      compute_builtin_object_size (ptr, object_size_type, &bytes);
      if ((object_size_type & OST_DYNAMIC) || int_fits_type_p (bytes, size_type_node))
         return mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,size_type_node, bytes);
   }else if (TREE_CODE (ptr) == SSA_NAME){
      /* If object size is not known yet, delay folding until
      later.  Maybe subsequent passes will help determining
      it.  */
      if (compute_builtin_object_size (ptr, object_size_type, &bytes)
      && ((object_size_type & OST_DYNAMIC) || int_fits_type_p (bytes, size_type_node)))
      return mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,size_type_node, bytes);
   }
   return NULL_TREE;
}

/* Folds a call EXPR (which may be null) to built-in function FNDECL
   with 2 arguments, ARG0 and ARG1.  This function returns NULL_TREE
   if no simplification was possible.  */
static tree fold_builtin_2 (MtcsBuiltins *self,location_t loc, tree expr, tree fndecl, tree arg0, tree arg1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree type = TREE_TYPE (TREE_TYPE (fndecl));
   enum built_in_function fcode = DECL_FUNCTION_CODE (fndecl);

   if (error_operand_p (arg0) || error_operand_p (arg1))
      return NULL_TREE;

   if (tree ret = fold_const_call (as_combined_fn (fcode), type, arg0, arg1))
      return ret;

   switch (fcode){
      CASE_FLT_FN_REENT (BUILT_IN_GAMMA): /* GAMMA_R */
      CASE_FLT_FN_REENT (BUILT_IN_LGAMMA): /* LGAMMA_R */
         if (validate_arg (arg0, REAL_TYPE) && validate_arg (arg1, POINTER_TYPE))
            return do_mpfr_lgamma_r(self,arg0, arg1, type);
         break;

      CASE_FLT_FN (BUILT_IN_FREXP):
         return fold_builtin_frexp(self,loc, arg0, arg1, type);

      CASE_FLT_FN (BUILT_IN_MODF):
         return fold_builtin_modf(self,loc, arg0, arg1, type);

      case BUILT_IN_STRSPN:
         return fold_builtin_strspn (loc, expr, arg0, arg1, type);

      case BUILT_IN_STRCSPN:
         return fold_builtin_strcspn(self,loc, expr, arg0, arg1, type);

      case BUILT_IN_STRPBRK:
         return fold_builtin_strpbrk(self,loc, expr, arg0, arg1, type);

      case BUILT_IN_EXPECT:
         return mtcs_butiltins_fold_builtin_expect/*!fold_builtin_expect*/(self,loc, arg0, arg1, NULL_TREE, NULL_TREE);

      case BUILT_IN_ISGREATER:
         return fold_builtin_unordered_cmp(self,loc, fndecl,arg0, arg1, UNLE_EXPR, LE_EXPR);
      case BUILT_IN_ISGREATEREQUAL:
         return fold_builtin_unordered_cmp(self,loc, fndecl,arg0, arg1, UNLT_EXPR, LT_EXPR);
      case BUILT_IN_ISLESS:
         return fold_builtin_unordered_cmp(self,loc, fndecl, arg0, arg1, UNGE_EXPR, GE_EXPR);
      case BUILT_IN_ISLESSEQUAL:
         return fold_builtin_unordered_cmp(self,loc, fndecl, arg0, arg1, UNGT_EXPR, GT_EXPR);
      case BUILT_IN_ISLESSGREATER:
         return fold_builtin_unordered_cmp(self,loc, fndecl, arg0, arg1, UNEQ_EXPR, EQ_EXPR);
      case BUILT_IN_ISUNORDERED:
         return fold_builtin_unordered_cmp(self,loc, fndecl,arg0, arg1, UNORDERED_EXPR,NOP_EXPR);

      case BUILT_IN_ISEQSIG:
         return fold_builtin_iseqsig(self,loc, arg0, arg1);

      /* We do the folding for va_start in the expander.  */
      case BUILT_IN_VA_START:
         break;

      case BUILT_IN_OBJECT_SIZE:
      case BUILT_IN_DYNAMIC_OBJECT_SIZE:
         return fold_builtin_object_size(self,arg0, arg1, fcode);

      case BUILT_IN_ATOMIC_ALWAYS_LOCK_FREE:
      return fold_builtin_atomic_always_lock_free(self,arg0, arg1);

      case BUILT_IN_ATOMIC_IS_LOCK_FREE:
         return fold_builtin_atomic_is_lock_free(self,arg0, arg1);

      case BUILT_IN_CLZG:
      case BUILT_IN_CTZG:
         return fold_builtin_bit_query(self,loc, fcode, arg0, arg1);

      default:
         break;
   }
   return NULL_TREE;
}


/* Fold function call to builtin sincos, sincosf, or sincosl.  Return
   NULL_TREE if no simplification can be made.  */
static tree fold_builtin_sincos (MtcsBuiltins *self,location_t loc,
           tree arg0, tree arg1, tree arg2)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree type;
   tree fndecl, call = NULL_TREE;

   if (!validate_arg (arg0, REAL_TYPE)
   || !validate_arg (arg1, POINTER_TYPE)
   || !validate_arg (arg2, POINTER_TYPE))
      return NULL_TREE;

   type = TREE_TYPE (arg0);

   /* Calculate the result when the argument is a constant.  */
   built_in_function fn = mathfn_built_in_2 (type, CFN_BUILT_IN_CEXPI);
   if (fn == END_BUILTINS)
      return NULL_TREE;

   /* Canonicalize sincos to cexpi.  */
   if (TREE_CODE (arg0) == REAL_CST){
      tree complex_type = build_complex_type (type);
      call = fold_const_call (as_combined_fn (fn), complex_type, arg0);
   }
   if (!call){
      if (!mtcsTarget/*!targetm.libc_has_function*/->libc_has_function(mtcsTarget,function_c99_math_complex, type)
      || !builtin_decl_implicit_p (fn))
         return NULL_TREE;
      fndecl = mtcs_tree_builtin_decl_explicit/*builtin_decl_explicit*/(mtcsTree,fn);
      call = mtcs_tree_build_call_expr/*!build_call_expr_loc*/(mtcsTree,loc, fndecl, 1, arg0);
      call = builtin_save_expr (call);
   }

   tree ptype = mtcs_tree_build_pointer_type/*!build_pointer_type*/(mtcsTree,type);
   arg1 = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,ptype, arg1);
   arg2 = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,ptype, arg2);
   return build2 (COMPOUND_EXPR, void_type_node,
            build2 (MODIFY_EXPR, void_type_node,
            build_fold_indirect_ref_loc (loc, arg1),
            fold_build1_loc (loc, IMAGPART_EXPR, type, call)),
            build2 (MODIFY_EXPR, void_type_node,
            build_fold_indirect_ref_loc (loc, arg2),
            fold_build1_loc (loc, REALPART_EXPR, type, call)));
}

/* Fold function call to builtin memcmp with arguments ARG1 and ARG2.
   Return NULL_TREE if no simplification can be made.  */
static tree fold_builtin_memcmp (MtcsBuiltins *self,location_t loc, tree arg1, tree arg2, tree len)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   if (!validate_arg (arg1, POINTER_TYPE)
   || !validate_arg (arg2, POINTER_TYPE)
   || !validate_arg (len, INTEGER_TYPE))
      return NULL_TREE;

   /* If the LEN parameter is zero, return zero.  */
   if (integer_zerop (len))
      return omit_two_operands_loc (loc, integer_type_node, integer_zero_node,arg1, arg2);

   /* If ARG1 and ARG2 are the same (and not volatile), return zero.  */
   if (operand_equal_p (arg1, arg2, 0))
      return omit_one_operand_loc (loc, integer_type_node, integer_zero_node, len);

   /* If len parameter is one, return an expression corresponding to
   (*(const unsigned char*)arg1 - (const unsigned char*)arg2).  */
   if (tree_fits_uhwi_p (len) && tree_to_uhwi (len) == 1){
      tree cst_uchar_node = build_type_variant (unsigned_char_type_node, 1, 0);
      tree cst_uchar_ptr_node = mtcs_tree_build_pointer_type_for_mode/*!build_pointer_type_for_mode*/(mtcsTree,
            cst_uchar_node, ptr_mode, true);

      tree ind1 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, integer_type_node,
                     build1 (INDIRECT_REF, cst_uchar_node,
                     mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc,
                     cst_uchar_ptr_node,
                     arg1)));
      tree ind2 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, integer_type_node,
                     build1 (INDIRECT_REF, cst_uchar_node,
                     mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc,
                     cst_uchar_ptr_node,
                     arg2)));
      return mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, MINUS_EXPR, integer_type_node, ind1, ind2);
   }

   return NULL_TREE;
}

/* Fold __builtin_{,s,u}{add,sub,mul}{,l,ll}_overflow, either into normal
   arithmetics if it can never overflow, or into internal functions that
   return both result of arithmetics and overflowed boolean flag in
   a complex integer result, or some other check for overflow.
   Similarly fold __builtin_{add,sub,mul}_overflow_p to just the overflow
   checking part of that.  */
static tree fold_builtin_arith_overflow (MtcsBuiltins *self,location_t loc, enum built_in_function fcode,
              tree arg0, tree arg1, tree arg2)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

   enum internal_fn ifn = IFN_LAST;
   /* The code of the expression corresponding to the built-in.  */
   enum tree_code opcode = ERROR_MARK;
   bool ovf_only = false;

   switch (fcode){
      case BUILT_IN_ADD_OVERFLOW_P:
      ovf_only = true;
      /* FALLTHRU */
      case BUILT_IN_ADD_OVERFLOW:
      case BUILT_IN_SADD_OVERFLOW:
      case BUILT_IN_SADDL_OVERFLOW:
      case BUILT_IN_SADDLL_OVERFLOW:
      case BUILT_IN_UADD_OVERFLOW:
      case BUILT_IN_UADDL_OVERFLOW:
      case BUILT_IN_UADDLL_OVERFLOW:
         opcode = PLUS_EXPR;
         ifn = IFN_ADD_OVERFLOW;
         break;
      case BUILT_IN_SUB_OVERFLOW_P:
         ovf_only = true;
         /* FALLTHRU */
      case BUILT_IN_SUB_OVERFLOW:
      case BUILT_IN_SSUB_OVERFLOW:
      case BUILT_IN_SSUBL_OVERFLOW:
      case BUILT_IN_SSUBLL_OVERFLOW:
      case BUILT_IN_USUB_OVERFLOW:
      case BUILT_IN_USUBL_OVERFLOW:
      case BUILT_IN_USUBLL_OVERFLOW:
         opcode = MINUS_EXPR;
         ifn = IFN_SUB_OVERFLOW;
         break;
      case BUILT_IN_MUL_OVERFLOW_P:
         ovf_only = true;
         /* FALLTHRU */
      case BUILT_IN_MUL_OVERFLOW:
      case BUILT_IN_SMUL_OVERFLOW:
      case BUILT_IN_SMULL_OVERFLOW:
      case BUILT_IN_SMULLL_OVERFLOW:
      case BUILT_IN_UMUL_OVERFLOW:
      case BUILT_IN_UMULL_OVERFLOW:
      case BUILT_IN_UMULLL_OVERFLOW:
         opcode = MULT_EXPR;
         ifn = IFN_MUL_OVERFLOW;
         break;
      default:
         gcc_unreachable ();
   }

   /* For the "generic" overloads, the first two arguments can have different
   types and the last argument determines the target type to use to check
   for overflow.  The arguments of the other overloads all have the same
   type.  */
   tree type = ovf_only ? TREE_TYPE (arg2) : TREE_TYPE (TREE_TYPE (arg2));

   /* For the __builtin_{add,sub,mul}_overflow_p builtins, when the first two
   arguments are constant, attempt to fold the built-in call into a constant
   expression indicating whether or not it detected an overflow.  */
   if (ovf_only
   && TREE_CODE (arg0) == INTEGER_CST
   && TREE_CODE (arg1) == INTEGER_CST)
   /* Perform the computation in the target type and check for overflow.  */
      return omit_one_operand_loc (loc, boolean_type_node,
               arith_overflowed_p (opcode, type, arg0, arg1)
               ? boolean_true_node : boolean_false_node,
               arg2);

   tree intres, ovfres;
   if (TREE_CODE (arg0) == INTEGER_CST && TREE_CODE (arg1) == INTEGER_CST){
      intres = fold_binary_loc (loc, opcode, type,
                        mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, type, arg0),
                        mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, type, arg1));
      if (TREE_OVERFLOW (intres))
         intres = drop_tree_overflow (intres);
      ovfres = (arith_overflowed_p (opcode, type, arg0, arg1)? boolean_true_node : boolean_false_node);
   }else{
      tree ctype = build_complex_type (type);
      tree call = build_call_expr_internal_loc (loc, ifn, ctype, 2,
      arg0, arg1);
      tree tgt;
      if (ovf_only){
         tgt = call;
         intres = NULL_TREE;
      }else{
         /* Force SAVE_EXPR even for calls which satisfy tree_invariant_p_1,
         as while the call itself is const, the REALPART_EXPR store is
         certainly not.  And in any case, we want just one call,
         not multiple and trying to CSE them later.  */
         TREE_SIDE_EFFECTS (call) = 1;
         tgt = save_expr (call);
      }
      intres = build1_loc (loc, REALPART_EXPR, type, tgt);
      ovfres = build1_loc (loc, IMAGPART_EXPR, type, tgt);
      ovfres = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, boolean_type_node, ovfres);
   }

   if (ovf_only)
      return omit_one_operand_loc (loc, boolean_type_node, ovfres, arg2);

   tree mem_arg2 = build_fold_indirect_ref_loc (loc, arg2);
   tree store = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, MODIFY_EXPR, void_type_node, mem_arg2, intres);
   return build2_loc (loc, COMPOUND_EXPR, boolean_type_node, store, ovfres);
}

/* If arguments ARG0 and ARG1 are REAL_CSTs, call mpfr_remquo() to set
   the pointer *(ARG_QUO) and return the result.  The type is taken
   from the type of ARG0 and is used for setting the precision of the
   calculation and results.  */

static tree do_mpfr_remquo (MtcsBuiltins *self,tree arg0, tree arg1, tree arg_quo)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree const type = TREE_TYPE (arg0);
   tree result = NULL_TREE;

   STRIP_NOPS (arg0);
   STRIP_NOPS (arg1);

   /* To proceed, MPFR must exactly represent the target floating point
   format, which only happens when the target base equals two.  */
   if (mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,TYPE_MODE (type))->b == 2
   && TREE_CODE (arg0) == REAL_CST && !TREE_OVERFLOW (arg0)
   && TREE_CODE (arg1) == REAL_CST && !TREE_OVERFLOW (arg1)){
      const REAL_VALUE_TYPE *const ra0 = TREE_REAL_CST_PTR (arg0);
      const REAL_VALUE_TYPE *const ra1 = TREE_REAL_CST_PTR (arg1);

      if (real_isfinite (ra0) && real_isfinite (ra1)){
         const struct real_format *fmt = mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,TYPE_MODE (type));
         const int prec = fmt->p;
         const mpfr_rnd_t rnd = fmt->round_towards_zero? MPFR_RNDZ : MPFR_RNDN;
         tree result_rem;
         long integer_quo;
         mpfr_t m0, m1;

         mpfr_inits2 (prec, m0, m1, NULL);
         mpfr_from_real (m0, ra0, MPFR_RNDN);
         mpfr_from_real (m1, ra1, MPFR_RNDN);
         mpfr_clear_flags ();
         mpfr_remquo (m0, &integer_quo, m0, m1, rnd);
         /* Remquo is independent of the rounding mode, so pass
         inexact=0 to do_mpfr_ckconv().  */
         result_rem = do_mpfr_ckconv(self,m0, type, /*inexact=*/ 0);
         mpfr_clears (m0, m1, NULL);
         if (result_rem){
            /* MPFR calculates quo in the host's long so it may
            return more bits in quo than the target int can hold
            if sizeof(host long) > sizeof(target int).  This can
            happen even for native compilers in LP64 mode.  In
            these cases, modulo the quo value with the largest
            number that the target int can hold while leaving one
            bit for the sign.  */
            if (sizeof (integer_quo) * CHAR_BIT > INT_TYPE_SIZE)
               integer_quo %= (long)(1UL << (INT_TYPE_SIZE - 1));

            /* Dereference the quo pointer argument.  */
            arg_quo = build_fold_indirect_ref (arg_quo);
            /* Proceed iff a valid pointer type was passed in.  */
            if (TYPE_MAIN_VARIANT (TREE_TYPE (arg_quo)) == integer_type_node){
               /* Set the value. */
               tree result_quo = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,MODIFY_EXPR, TREE_TYPE (arg_quo), arg_quo,
               mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (arg_quo), integer_quo));
               TREE_SIDE_EFFECTS (result_quo) = 1;
               /* Combine the quo assignment with the rem.  */
               result = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,COMPOUND_EXPR, type, result_quo, result_rem);
               suppress_warning (result, OPT_Wunused_value);
               result = non_lvalue (result);
            }
         }
      }
   }
   return result;
}

/* Fold a call to built-in function FNDECL with 3 arguments, ARG0, ARG1,
   and ARG2.
   This function returns NULL_TREE if no simplification was possible.  */
static tree fold_builtin_3 (MtcsBuiltins *self,location_t loc, tree fndecl,tree arg0, tree arg1, tree arg2)
{
   tree type = TREE_TYPE (TREE_TYPE (fndecl));
   enum built_in_function fcode = DECL_FUNCTION_CODE (fndecl);

   if (error_operand_p (arg0) || error_operand_p (arg1) || error_operand_p (arg2))
      return NULL_TREE;

   if (tree ret = fold_const_call (as_combined_fn (fcode), type, arg0, arg1, arg2))
      return ret;

   switch (fcode){

      CASE_FLT_FN (BUILT_IN_SINCOS):
         return fold_builtin_sincos(self,loc, arg0, arg1, arg2);

      CASE_FLT_FN (BUILT_IN_REMQUO):
         if (validate_arg (arg0, REAL_TYPE)
         && validate_arg (arg1, REAL_TYPE)
         && validate_arg (arg2, POINTER_TYPE))
            return do_mpfr_remquo(self,arg0, arg1, arg2);
         break;

      case BUILT_IN_MEMCMP:
         return fold_builtin_memcmp(self,loc, arg0, arg1, arg2);

      case BUILT_IN_EXPECT:
         return mtcs_butiltins_fold_builtin_expect/*!fold_builtin_expect*/(self,loc, arg0, arg1, arg2, NULL_TREE);

      case BUILT_IN_EXPECT_WITH_PROBABILITY:
         return mtcs_butiltins_fold_builtin_expect/*!fold_builtin_expect*/(self,loc, arg0, arg1, NULL_TREE, arg2);

      case BUILT_IN_ADD_OVERFLOW:
      case BUILT_IN_SUB_OVERFLOW:
      case BUILT_IN_MUL_OVERFLOW:
      case BUILT_IN_ADD_OVERFLOW_P:
      case BUILT_IN_SUB_OVERFLOW_P:
      case BUILT_IN_MUL_OVERFLOW_P:
      case BUILT_IN_SADD_OVERFLOW:
      case BUILT_IN_SADDL_OVERFLOW:
      case BUILT_IN_SADDLL_OVERFLOW:
      case BUILT_IN_SSUB_OVERFLOW:
      case BUILT_IN_SSUBL_OVERFLOW:
      case BUILT_IN_SSUBLL_OVERFLOW:
      case BUILT_IN_SMUL_OVERFLOW:
      case BUILT_IN_SMULL_OVERFLOW:
      case BUILT_IN_SMULLL_OVERFLOW:
      case BUILT_IN_UADD_OVERFLOW:
      case BUILT_IN_UADDL_OVERFLOW:
      case BUILT_IN_UADDLL_OVERFLOW:
      case BUILT_IN_USUB_OVERFLOW:
      case BUILT_IN_USUBL_OVERFLOW:
      case BUILT_IN_USUBLL_OVERFLOW:
      case BUILT_IN_UMUL_OVERFLOW:
      case BUILT_IN_UMULL_OVERFLOW:
      case BUILT_IN_UMULLL_OVERFLOW:
         return fold_builtin_arith_overflow(self,loc, fcode, arg0, arg1, arg2);

      default:
         break;
   }
   return NULL_TREE;
}

/* Fold __builtin_{add,sub}c{,l,ll} into pair of internal functions
   that return both result of arithmetics and overflowed boolean
   flag in a complex integer result.  */
static tree fold_builtin_addc_subc (MtcsBuiltins *self,location_t loc, enum built_in_function fcode,
         tree *args)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

   enum internal_fn ifn;

   switch (fcode){
      case BUILT_IN_ADDC:
      case BUILT_IN_ADDCL:
      case BUILT_IN_ADDCLL:
         ifn = IFN_ADD_OVERFLOW;
         break;
      case BUILT_IN_SUBC:
      case BUILT_IN_SUBCL:
      case BUILT_IN_SUBCLL:
         ifn = IFN_SUB_OVERFLOW;
         break;
      default:
         gcc_unreachable ();
   }

   tree type = TREE_TYPE (args[0]);
   tree ctype = build_complex_type (type);
   tree call = build_call_expr_internal_loc (loc, ifn, ctype, 2,args[0], args[1]);
   /* Force SAVE_EXPR even for calls which satisfy tree_invariant_p_1,
   as while the call itself is const, the REALPART_EXPR store is
   certainly not.  And in any case, we want just one call,
   not multiple and trying to CSE them later.  */
   TREE_SIDE_EFFECTS (call) = 1;
   tree tgt = save_expr (call);
   tree intres = build1_loc (loc, REALPART_EXPR, type, tgt);
   tree ovfres = build1_loc (loc, IMAGPART_EXPR, type, tgt);
   call = build_call_expr_internal_loc (loc, ifn, ctype, 2,intres, args[2]);
   TREE_SIDE_EFFECTS (call) = 1;
   tgt = save_expr (call);
   intres = build1_loc (loc, REALPART_EXPR, type, tgt);
   tree ovfres2 = build1_loc (loc, IMAGPART_EXPR, type, tgt);
   ovfres = build2_loc (loc, BIT_IOR_EXPR, type, ovfres, ovfres2);
   tree mem_arg3 = build_fold_indirect_ref_loc (loc, args[3]);
   tree store = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, MODIFY_EXPR, void_type_node, mem_arg3, ovfres);
   return build2_loc (loc, COMPOUND_EXPR, type, store, intres);
}
/* Fold a call to __builtin_fpclassify(int, int, int, int, int, ...).
   This builtin will generate code to return the appropriate floating
   point classification depending on the value of the floating point
   number passed in.  The possible return values must be supplied as
   int arguments to the call in the following order: FP_NAN, FP_INFINITE,
   FP_NORMAL, FP_SUBNORMAL and FP_ZERO.  The ellipses is for exactly
   one floating point argument which is "type generic".  */
static tree fold_builtin_fpclassify (MtcsBuiltins *self,location_t loc, tree *args, int nargs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree fp_nan, fp_infinite, fp_normal, fp_subnormal, fp_zero, arg, type, res, tmp;
   machine_mode mode;
   REAL_VALUE_TYPE r;
   char buf[128];

   /* Verify the required arguments in the original call.  */
   if (nargs != 6
   || !validate_arg (args[0], INTEGER_TYPE)
   || !validate_arg (args[1], INTEGER_TYPE)
   || !validate_arg (args[2], INTEGER_TYPE)
   || !validate_arg (args[3], INTEGER_TYPE)
   || !validate_arg (args[4], INTEGER_TYPE)
   || !validate_arg (args[5], REAL_TYPE))
      return NULL_TREE;

   fp_nan = args[0];
   fp_infinite = args[1];
   fp_normal = args[2];
   fp_subnormal = args[3];
   fp_zero = args[4];
   arg = args[5];
   type = TREE_TYPE (arg);
   mode = TYPE_MODE (type);
   arg = builtin_save_expr (fold_build1_loc (loc, ABS_EXPR, type, arg));

   /* fpclassify(x) ->
   isnan(x) ? FP_NAN :
   (fabs(x) == Inf ? FP_INFINITE :
   (fabs(x) >= DBL_MIN ? FP_NORMAL :
   (x == 0 ? FP_ZERO : FP_SUBNORMAL))).  */

   tmp = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, EQ_EXPR, integer_type_node, arg,build_real (type, dconst0));
   res = fold_build3_loc (loc, COND_EXPR, integer_type_node,tmp, fp_zero, fp_subnormal);

   if (DECIMAL_FLOAT_MODE_P (mode))
      sprintf (buf, "1E%d", mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,mode)->emin - 1);
   else
      sprintf (buf, "0x1p%d", mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,mode)->emin - 1);
   real_from_string3 (&r, buf, mode);
   tmp = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, GE_EXPR, integer_type_node,arg, build_real (type, r));
   res = fold_build3_loc (loc, COND_EXPR, integer_type_node, tmp,fp_normal, res);

   if (tree_expr_maybe_infinite_p (arg)){
      tmp = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, EQ_EXPR, integer_type_node, arg, build_real (type, dconstinf));
      res = fold_build3_loc (loc, COND_EXPR, integer_type_node, tmp,fp_infinite, res);
   }

   if (tree_expr_maybe_nan_p (arg)){
      tmp = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, ORDERED_EXPR, integer_type_node, arg, arg);
      res = fold_build3_loc (loc, COND_EXPR, integer_type_node, tmp,res, fp_nan);
   }

   return res;
}

/* Builtins with folding operations that operate on "..." arguments
   need special handling; we need to store the arguments in a convenient
   data structure before attempting any folding.  Fortunately there are
   only a few builtins that fall into this category.  FNDECL is the
   function, EXP is the CALL_EXPR for the call.  */
static tree fold_builtin_varargs (MtcsBuiltins *self,location_t loc, tree fndecl, tree *args, int nargs)
{
   enum built_in_function fcode = DECL_FUNCTION_CODE (fndecl);
   tree ret = NULL_TREE;

   switch (fcode){
      case BUILT_IN_FPCLASSIFY:
         ret = fold_builtin_fpclassify(self,loc, args, nargs);
         break;

      case BUILT_IN_ADDC:
      case BUILT_IN_ADDCL:
      case BUILT_IN_ADDCLL:
      case BUILT_IN_SUBC:
      case BUILT_IN_SUBCL:
      case BUILT_IN_SUBCLL:
         return fold_builtin_addc_subc(self,loc, fcode, args);

      default:
         break;
   }
   if (ret){
      ret = build1 (NOP_EXPR, TREE_TYPE (ret), ret);
      SET_EXPR_LOCATION (ret, loc);
      suppress_warning (ret);
      return ret;
   }
   return NULL_TREE;
}


/* Folds a call EXPR (which may be null) to built-in function FNDECL.
   ARGS is an array of NARGS arguments.  IGNORE is true if the result
   of the function call is ignored.  This function returns NULL_TREE
   if no simplification was possible.  */
static tree fold_builtin_n (MtcsBuiltins *self,location_t loc, tree expr, tree fndecl, tree *args,
      int nargs, bool)
{
   tree ret = NULL_TREE;

   switch (nargs){
      case 0:
         ret = fold_builtin_0(self,loc, fndecl);
         break;
      case 1:
         ret = fold_builtin_1(self,loc, expr, fndecl, args[0]);
         break;
      case 2:
         ret = fold_builtin_2(self,loc, expr, fndecl, args[0], args[1]);
         break;
      case 3:
         ret = fold_builtin_3(self,loc, fndecl, args[0], args[1], args[2]);
         break;
      default:
         ret = fold_builtin_varargs(self,loc, fndecl, args, nargs);
         break;
   }
   if (ret){
      ret = build1 (NOP_EXPR, TREE_TYPE (ret), ret);
      SET_EXPR_LOCATION (ret, loc);
      return ret;
   }
   return NULL_TREE;
}


/* Fold a CALL_EXPR with type TYPE with FN as the function expression.
   N arguments are passed in the array ARGARRAY.  Return a folded
   expression or NULL_TREE if no simplification was possible.  */
//原型 fold_builtin_call_array builtins.h builtins.cc
tree mtcs_builtins_fold_builtin_call_array (MtcsBuiltins *self,location_t loc, tree,
          tree fn,
          int n,
          tree *argarray)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   if (TREE_CODE (fn) != ADDR_EXPR)
      return NULL_TREE;

   tree fndecl = TREE_OPERAND (fn, 0);
   if (TREE_CODE (fndecl) == FUNCTION_DECL  && fndecl_built_in_p (fndecl)){
      /* If last argument is __builtin_va_arg_pack (), arguments to this
      function are not finalized yet.  Defer folding until they are.  */
      if (n && TREE_CODE (argarray[n - 1]) == CALL_EXPR){
         tree fndecl2 = get_callee_fndecl (argarray[n - 1]);
         if (fndecl2 && fndecl_built_in_p (fndecl2, BUILT_IN_VA_ARG_PACK))
            return NULL_TREE;
      }
      if (avoid_folding_inline_builtin (fndecl))
         return NULL_TREE;
      if (DECL_BUILT_IN_CLASS (fndecl) == BUILT_IN_MD)
         return mtcsTarget/*!targetm.fold_builtin*/->fold_builtin(mtcsTarget,fndecl, n, argarray, false);
      else
         return fold_builtin_n(self,loc, NULL_TREE, fndecl, argarray, n, false);
   }

   return NULL_TREE;
}


/* Create builtin_expect or builtin_expect_with_probability
   with PRED and EXPECTED as its arguments and return it as a truthvalue.
   Fortran FE can also produce builtin_expect with PREDICTOR as third argument.
   builtin_expect_with_probability instead uses third argument as PROBABILITY
   value.  */
static tree build_builtin_expect_predicate (MtcsBuiltins *self,location_t loc, tree pred, tree expected,
            tree predictor, tree probability)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree fn, arg_types, pred_type, expected_type, call_expr, ret_type;

   fn = builtin_decl_explicit (probability == NULL_TREE ? BUILT_IN_EXPECT: BUILT_IN_EXPECT_WITH_PROBABILITY);
   arg_types = TYPE_ARG_TYPES (TREE_TYPE (fn));
   ret_type = TREE_TYPE (TREE_TYPE (fn));
   pred_type = TREE_VALUE (arg_types);
   expected_type = TREE_VALUE (TREE_CHAIN (arg_types));

   pred = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, pred_type, pred);
   expected = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, expected_type, expected);

   if (probability)
      call_expr = mtcs_tree_build_call_expr/*!build_call_expr_loc*/(mtcsTree,loc, fn, 3, pred, expected, probability);
   else
      call_expr = mtcs_tree_build_call_expr/*!build_call_expr_loc*/(mtcsTree,
            loc, fn, predictor ? 3 : 2, pred, expected,predictor);

   return build2 (NE_EXPR, TREE_TYPE (pred), call_expr,mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,ret_type, 0));
}

/* Fold a call to builtin_expect with arguments ARG0, ARG1, ARG2, ARG3.  Return
   NULL_TREE if no simplification is possible.  */
//原型 fold_builtin_expect builtins.h builtins.cc
tree mtcs_butiltins_fold_builtin_expect (MtcsBuiltins *self,location_t loc, tree arg0, tree arg1, tree arg2,
           tree arg3)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree inner, fndecl, inner_arg0;
   enum tree_code code;

   /* Distribute the expected value over short-circuiting operators.
   See through the cast from truthvalue_type_node to long.  */
   inner_arg0 = arg0;
   while (CONVERT_EXPR_P (inner_arg0)
   && INTEGRAL_TYPE_P (TREE_TYPE (inner_arg0))
   && INTEGRAL_TYPE_P (TREE_TYPE (TREE_OPERAND (inner_arg0, 0))))
      inner_arg0 = TREE_OPERAND (inner_arg0, 0);

   /* If this is a builtin_expect within a builtin_expect keep the
   inner one.  See through a comparison against a constant.  It
   might have been added to create a thruthvalue.  */
   inner = inner_arg0;

   if (COMPARISON_CLASS_P (inner) && TREE_CODE (TREE_OPERAND (inner, 1)) == INTEGER_CST)
      inner = TREE_OPERAND (inner, 0);

   if (TREE_CODE (inner) == CALL_EXPR   && (fndecl = get_callee_fndecl (inner))
   && fndecl_built_in_p (fndecl, BUILT_IN_EXPECT, BUILT_IN_EXPECT_WITH_PROBABILITY))
      return arg0;

   inner = inner_arg0;
   code = TREE_CODE (inner);
   if (code == TRUTH_ANDIF_EXPR || code == TRUTH_ORIF_EXPR){
      tree op0 = TREE_OPERAND (inner, 0);
      tree op1 = TREE_OPERAND (inner, 1);
      arg1 = save_expr (arg1);

      op0 = build_builtin_expect_predicate(self,loc, op0, arg1, arg2, arg3);
      op1 = build_builtin_expect_predicate(self,loc, op1, arg1, arg2, arg3);
      inner = build2 (code, TREE_TYPE (inner), op0, op1);

      return mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, TREE_TYPE (arg0), inner);
   }

   /* If the argument isn't invariant then there's nothing else we can do.  */
   if (!TREE_CONSTANT (inner_arg0))
      return NULL_TREE;

   /* If we expect that a comparison against the argument will fold to
   a constant return the constant.  In practice, this means a true
   constant or the address of a non-weak symbol.  */
   inner = inner_arg0;
   STRIP_NOPS (inner);
   if (TREE_CODE (inner) == ADDR_EXPR){
      do{
         inner = TREE_OPERAND (inner, 0);
      }while (TREE_CODE (inner) == COMPONENT_REF || TREE_CODE (inner) == ARRAY_REF);
      if (VAR_OR_FUNCTION_DECL_P (inner) && DECL_WEAK (inner))
         return NULL_TREE;
   }

   /* Otherwise, ARG0 already has the proper type for the return value.  */
   return arg0;
}

//原型 get_builtin_sync_mem  butilins.cc 原本是static方法，但mtcsptxbuiltins需要调用该方法
rtx      mtcs_builtins_get_builtin_sync_mem (MtcsBuiltins *self,tree loc, machine_mode mode)
{
   return get_builtin_sync_mem(self,loc,mode);
}
//原型 expand_expr_force_mode  butilins.cc 原本是static方法，但mtcsptxbuiltins需要调用该方法
rtx      mtcs_builtins_expand_expr_force_mode (MtcsBuiltins *self,tree exp, machine_mode mode)
{
   return expand_expr_force_mode(self,exp,mode);
}

//原型 get_memmodel  butilins.cc 原本是static方法 但mtcsptxbuiltins需要调用该方法
enum memmodel mtcs_builtins_get_memmodel (MtcsBuiltins *self,tree exp)
{
   return get_memmodel(self,exp);
}

/* If CALL is a call to a BUILT_IN_NORMAL function that could be replaced
   on the current target by a call to an internal function, return the
   code of that internal function, otherwise return IFN_LAST.  The caller
   is responsible for ensuring that any side-effects of the built-in
   call are dealt with correctly.  E.g. if CALL sets errno, the caller
   must decide that the errno result isn't needed or make it available
   in some other way.  */
//原型 replacement_internal_fn builtins.h builtins.cc
internal_fn mtcs_builtins_replacement_internal_fn (MtcsBuiltins *self,gcall *call)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsInternalFn *mtcsInternalFn = mtcs_target_get_internal_fn(mtcsTarget);

   if (gimple_call_builtin_p (call, BUILT_IN_NORMAL)){
      internal_fn ifn = associated_internal_fn (gimple_call_fndecl (call));
      if (ifn != IFN_LAST){
         tree_pair types = direct_internal_fn_types (ifn, call);
         optimization_type opt_type = bb_optimization_type (gimple_bb (call));
         if (mtcs_internal_fn_direct_internal_fn_supported_p/*!direct_internal_fn_supported_p*/(mtcsInternalFn,ifn, types, opt_type)){
            return ifn;
         }
      }
   }
   return IFN_LAST;
}
