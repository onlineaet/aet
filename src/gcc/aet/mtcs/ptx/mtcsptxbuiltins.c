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

#include "../mtcstarget.h"
#include "ptx-common.h"
#include "mtcsptxbuiltins.h"
#include "mtcsptxemit.h"
#include "mtcsptx.h"
#include "mtcsptxfunc.h"
#include "gen/ptx-insn-flags.h"
#include "gen/ptx-insn-modes.h"
#include "gen/ptx-insn-unspec.h"

#include "../../aetprinttree.h"
#include "../../aetprintgimple.h"
#include "../../mtcsinfo.h"
#include "../mtcsprintrtl.h"
#include "ptx-common.h"
#include "ptxtool.h"

/* Codes for all the NVPTX builtins.  */
enum nvptx_builtins
{
  NVPTX_BUILTIN_SHUFFLE,
  NVPTX_BUILTIN_SHUFFLELL,
  NVPTX_BUILTIN_WORKER_ADDR,
  NVPTX_BUILTIN_VECTOR_ADDR,
  NVPTX_BUILTIN_CMP_SWAP,
  NVPTX_BUILTIN_CMP_SWAPLL,
  NVPTX_BUILTIN_MEMBAR_GL,
  NVPTX_BUILTIN_MEMBAR_CTA,
  NVPTX_BUILTIN_BAR_RED_AND,
  NVPTX_BUILTIN_BAR_RED_OR,
  NVPTX_BUILTIN_BAR_RED_POPC,
  NVPTX_BUILTIN_BREV,
  NVPTX_BUILTIN_BREVLL,
  NVPTX_BUILTIN_MAX
};

static nboolean replaceCall_cb(MtcsBuiltins *mtcsBuiltins, gimple *call);
static gimple  *convertCall_cb(MtcsBuiltins *mtcsBuiltins, gimple *call);
static nboolean supportBuiltinFn(MtcsBuiltins *mtcsBuiltins, tree fndecl);
static rtx expandBuiltinFn_cb(MtcsBuiltins *mtcsBuiltins,tree exp, rtx target, rtx subtarget, machine_mode mode, int ignore);
static rtx expandInternalFn_cb(MtcsBuiltins *mtcsBuiltins,tree exp, rtx target, rtx subtarget, machine_mode mode, int ignore);

//原型 emit_memset i386-expand.cc
static rtx emit_memset (MtcsPtxBuiltins *self,rtx destmem, rtx destptr, rtx promoted_val,
        HOST_WIDE_INT size_to_move);
//原型 emit_memmov i386-expand.cc
static rtx emit_memmov (MtcsPtxBuiltins *self,rtx destmem, rtx *srcmem, rtx destptr, rtx srcptr,
        HOST_WIDE_INT size_to_move);
//原型 expand_set_or_cpymem_via_loop i386-expand.cc
static void expand_set_or_cpymem_via_loop (MtcsPtxBuiltins *self,rtx destmem, rtx srcmem,
                rtx destptr, rtx srcptr, rtx value,
                rtx count, machine_mode mode, int unroll,
                int expected_size, bool issetmem);
//原型 ix86_zero_extend_to_Pmode i386-protos.h i385.cc
static rtx ptx_zero_extend_to_Pmode (MtcsPtxBuiltins *self,rtx exp);

static void mtcsPtxBuiltinsInit(MtcsPtxBuiltins *self)
{
   MtcsBuiltins *mtcsBuiltins=(MtcsBuiltins *)self;
   mtcsBuiltins->replace_call=replaceCall_cb;
   mtcsBuiltins->convert_call=convertCall_cb;
   mtcsBuiltins->expand_builtin_fn=expandBuiltinFn_cb;
   mtcsBuiltins->expand_internal_fn=expandInternalFn_cb;
   mtcsBuiltins->support_builtin_fn=supportBuiltinFn;
}

static void printrtl(MtcsPtxBuiltins *self)
{
   return;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   rtx_insn *insn;
   for (insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = NEXT_INSN (insn))
      mtcs_print_rtl_single_skip_debug(stderr,insn);
}


/* Expander for the shuffle builtins.  */
//原型 nvptx_expand_shuffle nvptx.cc
static rtx nvptx_expand_shuffle (MtcsPtxBuiltins *self,tree exp, rtx target, machine_mode mode, int ignore)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsPtxEmit *mtcsPtxEmit=(MtcsPtxEmit *)mtcsEmit;

   n_debug("-----nvptx.cc -----112--nvptx_expand_shuffle:mode:%d ignore:%d\n",mode,ignore);
   if (ignore)
      return target;

   rtx src = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,CALL_EXPR_ARG (exp, 0),NULL_RTX, mode, EXPAND_NORMAL);
   mtcs_print_rtl(stderr,src);

   if (!REG_P (src))
      src =mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,mode, src);

   rtx idx = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,CALL_EXPR_ARG (exp, 1),
            NULL_RTX, mtcsMode->modes.M_SImode, EXPAND_NORMAL);
   rtx op = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,CALL_EXPR_ARG  (exp, 2),
            NULL_RTX, mtcsMode->modes.M_SImode, EXPAND_NORMAL);

   n_debug("-----nvptx.cc -----112-aa:%d\n",mode,ignore);
   mtcs_print_rtl(stderr,target);
   mtcs_print_rtl(stderr,src);
   mtcs_print_rtl(stderr,idx);
   mtcs_print_rtl(stderr,op);

   if (!REG_P (idx) && GET_CODE (idx) != CONST_INT)
      idx = mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,mtcsMode->modes.M_SImode, idx);
   rtx tmp  = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mtcsMode->modes.M_SImode);
   rtx pat = mtcs_ptx_emit_gen_shuffle/*!nvptx_gen_shuffle*/(mtcsPtxEmit,
         tmp/*!target*/, src, idx,(ptx_shuffle_kind) INTVAL (op));
   n_debug("nvptx_expand_shuffle---\n");
   mtcs_print_rtl(stderr,pat);
   if (pat)
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,pat);
   mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, tmp);

   return target;
}


/* Expander for the bit reverse builtins.  */
//原型 nvptx_expand_brev nvptx.cc
static rtx nvptx_expand_brev (MtcsPtxBuiltins *self,tree exp, rtx target, machine_mode mode, int ignore)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsPtxEmit *mtcsPtxEmit=(MtcsPtxEmit *)mtcsEmit;
  MtcsPtx *mtcsPtx=(MtcsPtx *)mtcsTarget;

  n_debug("-----nvptx.cc -----113--static rtx nvptx_expand_brev (tree exp, rtx target, machine_mode mode, int ignore)\n");
  if (ignore)
    return target;

  rtx arg = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,CALL_EXPR_ARG (exp, 0),
             NULL_RTX, mode, EXPAND_NORMAL);
  if (!REG_P (arg))
    arg = mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,mode, arg);
  if (!target)
    target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
  rtx pat;
  if (mode == mtcsMode->modes.M_SImode)
    pat = ptx_gen_bitrevsi2/*!gen_bitrevsi2*/(target, arg);
  else
    pat = ptx_gen_bitrevdi2/*!gen_bitrevdi2*/(target, arg);
  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,pat);
  return target;
}

/* Shared-memory reduction address expander.  */
//原型 nvptx_expand_shared_addr nvptx.cc
static rtx nvptx_expand_shared_addr (MtcsPtxBuiltins *self,tree exp, rtx target,
                                       machine_mode ARG_UNUSED (mode), int ignore,int vector)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsPtxEmit *mtcsPtxEmit=(MtcsPtxEmit *)mtcsEmit;
  MtcsPtx *mtcsPtx=(MtcsPtx *)mtcsTarget;

  n_debug("-----nvptx.cc -----114--static rtx nvptx_expand_shared_addr\
         (tree exp, rtx target,machine_mode ARG_UNUSED (mode), int ignore,int vector))\n");
  if (ignore)
    return target;

  unsigned align = TREE_INT_CST_LOW (CALL_EXPR_ARG (exp, 2));
  unsigned offset = TREE_INT_CST_LOW (CALL_EXPR_ARG (exp, 0));
  unsigned size = TREE_INT_CST_LOW (CALL_EXPR_ARG (exp, 1));
  rtx addr =mtcsPtx->worker_red_sym;
  machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
  if (vector){
      unsigned int psize = ROUND_UP (size + offset, align);
      unsigned int pnum = mtcs_ptx_get_mach_max_workers/*!nvptx_mach_max_workers*/(mtcsPtx);
      mtcsPtx->vector_red_partition = MAX (mtcsPtx->vector_red_partition, psize);
      mtcsPtx->vector_red_size = MAX (mtcsPtx->vector_red_size, psize * pnum);
      mtcsPtx->vector_red_align = MAX (mtcsPtx->vector_red_align, align);
      struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)cfun->machine;

      if (nvptxMachine/*!cfun->machine->*/->red_partition == NULL)
          nvptxMachine/*!cfun->machine->*/->red_partition =mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,pMode);

      addr = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,pMode);
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,
              ptx_gen_nvptx_red_partition/*!gen_nvptx_red_partition*/(addr,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,offset)));
  }else{
      mtcsPtx->worker_red_align = MAX (mtcsPtx->worker_red_align, align);
      mtcsPtx->worker_red_size = MAX (mtcsPtx->worker_red_size, size + offset);

      if (offset){
          addr = gen_rtx_PLUS (pMode, addr, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,offset));
          addr = gen_rtx_CONST (pMode, addr);
      }
  }

  mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, addr);
  return target;
}

/* Expand the CMP_SWAP PTX builtins.  We have our own versions that do
   not require taking the address of any object, other than the memory
   cell being operated on.  */
//原型 nvptx_expand_cmp_swap nvptx.cc
static rtx nvptx_expand_cmp_swap (MtcsPtxBuiltins *self,tree exp, rtx target,machine_mode ARG_UNUSED (m), int ARG_UNUSED (ignore))
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsPtxEmit *mtcsPtxEmit=(MtcsPtxEmit *)mtcsEmit;
  MtcsPtx *mtcsPtx=(MtcsPtx *)mtcsTarget;

  n_debug("-----nvptx.cc -----115-- nvptx_expand_cmp_swap \n");
  mtcs_mode mode = mtcs_mode_host2device_by_tree(mtcsMode, TREE_TYPE (exp),TYPE_MODE(TREE_TYPE(exp)));
  machine_mode pMode = mtcs_mode_get_Pmode(mtcsMode);
  if (!target)
    target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

  rtx mem = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,CALL_EXPR_ARG (exp, 0),
             NULL_RTX, pMode, EXPAND_NORMAL);
  rtx cmp = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,CALL_EXPR_ARG (exp, 1),
             NULL_RTX, mode, EXPAND_NORMAL);
  rtx src = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,CALL_EXPR_ARG (exp, 2),
             NULL_RTX, mode, EXPAND_NORMAL);
  rtx pat;

  mem = gen_rtx_MEM (mode, mem);
  if (!REG_P (cmp))
    cmp = mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,mode, cmp);
  if (!REG_P (src))
    src = mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,mode, src);

  if (mode == mtcsMode->modes.M_SImode)
    pat = gen_atomic_compare_and_swapsi_1 (target, mem, cmp, src, const0_rtx);
  else
    pat = gen_atomic_compare_and_swapdi_1 (target, mem, cmp, src, const0_rtx);

  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,pat);

  return target;
}

/* Expander for 'bar.red' instruction builtins.  */
//原型 nvptx_expand_bar_red nvptx.cc
static rtx nvptx_expand_bar_red (MtcsPtxBuiltins *self,tree exp, rtx target,machine_mode ARG_UNUSED (m), int ARG_UNUSED (ignore))
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsPtxEmit *mtcsPtxEmit=(MtcsPtxEmit *)mtcsEmit;
  MtcsPtx *mtcsPtx=(MtcsPtx *)mtcsTarget;

  n_debug("-----nvptx.cc -----116--static rtx nvptx_expand_bar_red \n");

  int code = DECL_MD_FUNCTION_CODE (TREE_OPERAND (CALL_EXPR_FN (exp), 0));
  mtcs_mode mode = mtcs_mode_host2device_by_tree(mtcsMode, TREE_TYPE (exp),TYPE_MODE(TREE_TYPE(exp)));

  if (!target)
    target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

  rtx pred, dst;
  rtx bar =  mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,CALL_EXPR_ARG (exp, 0),
             NULL_RTX, mtcsMode->modes.M_SImode, EXPAND_NORMAL);
  rtx nthr = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,CALL_EXPR_ARG (exp, 1),
              NULL_RTX, mtcsMode->modes.M_SImode, EXPAND_NORMAL);
  rtx cpl =  mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,CALL_EXPR_ARG (exp, 2),
             NULL_RTX, mtcsMode->modes.M_SImode, EXPAND_NORMAL);
  rtx redop =  mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,CALL_EXPR_ARG (exp, 3),
               NULL_RTX, mtcsMode->modes.M_SImode, EXPAND_NORMAL);
  if (CONST_INT_P (bar)){
      if (INTVAL (bar) < 0 || INTVAL (bar) > 15){
          error_at (EXPR_LOCATION (exp), "barrier value must be within [0,15]");
          return const0_rtx;
      }
  }else if (!REG_P (bar))
    bar = mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,mtcsMode->modes.M_SImode, bar);

  if (!CONST_INT_P (nthr) && !REG_P (nthr))
    nthr = mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,mtcsMode->modes.M_SImode, nthr);

  if (!CONST_INT_P (cpl)){
      error_at (EXPR_LOCATION (exp),"complement argument must be constant");
      return const0_rtx;
  }

  pred = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mtcsMode->modes.M_BImode);
  if (!REG_P (redop))
    redop = mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,mtcsMode->modes.M_SImode, redop);
  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_rtx_SET (pred, gen_rtx_NE (mtcsMode->modes.M_BImode,
        redop,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,0))));
  redop = pred;

  rtx pat;
  switch (code){
    case NVPTX_BUILTIN_BAR_RED_AND:
      dst = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mtcsMode->modes.M_BImode);
      pat = ptx_gen_nvptx_barred_and/*!gen_nvptx_barred_and*/(dst, bar, nthr, cpl, redop);
      break;
    case NVPTX_BUILTIN_BAR_RED_OR:
      dst = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mtcsMode->modes.M_BImode);
      pat = ptx_gen_nvptx_barred_or/*!gen_nvptx_barred_or*/(dst, bar, nthr, cpl, redop);
      break;
    case NVPTX_BUILTIN_BAR_RED_POPC:
      dst = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mtcsMode->modes.M_SImode);
      pat = ptx_gen_nvptx_barred_popc (dst, bar, nthr, cpl, redop);
      break;
    default:
      gcc_unreachable ();
  }
  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,pat);
  if (GET_MODE (dst) == mtcsMode->modes.M_BImode){
      rtx tmp = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_rtx_SET (tmp,
                     gen_rtx_NE (mode, dst, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,0))));
      dst = tmp;
  }
  mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, dst);
  return target;
}


//原型 targetm.expand_builtin (exp, target, subtarget, mode, ignore) #define TARGET_EXPAND_BUILTIN nvptx_expand_builtin
rtx mtcs_ptx_builtins_expand_builtin (MtcsPtxBuiltins *self,tree exp, rtx target, rtx subtarget,
        machine_mode mode, int ignore)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  tree fndecl = TREE_OPERAND (CALL_EXPR_FN (exp), 0);
  switch (DECL_MD_FUNCTION_CODE (fndecl)){
    case NVPTX_BUILTIN_SHUFFLE:
    case NVPTX_BUILTIN_SHUFFLELL:
      return nvptx_expand_shuffle(self,exp, target, mode, ignore);

    case NVPTX_BUILTIN_WORKER_ADDR:
      return nvptx_expand_shared_addr(self,exp, target, mode, ignore, false);

    case NVPTX_BUILTIN_VECTOR_ADDR:
      return nvptx_expand_shared_addr(self,exp, target, mode, ignore, true);

    case NVPTX_BUILTIN_CMP_SWAP:
    case NVPTX_BUILTIN_CMP_SWAPLL:
      return nvptx_expand_cmp_swap(self,exp, target, mode, ignore);

    case NVPTX_BUILTIN_MEMBAR_GL:
        mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,ptx_gen_nvptx_membar_gl/*!gen_nvptx_membar_gl*/());
      return NULL_RTX;

    case NVPTX_BUILTIN_MEMBAR_CTA:
        mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,ptx_gen_nvptx_membar_cta/*!gen_nvptx_membar_cta*/());
      return NULL_RTX;

    case NVPTX_BUILTIN_BAR_RED_AND:
    case NVPTX_BUILTIN_BAR_RED_OR:
    case NVPTX_BUILTIN_BAR_RED_POPC:
      return nvptx_expand_bar_red(self,exp, target, mode, ignore);

    case NVPTX_BUILTIN_BREV:
    case NVPTX_BUILTIN_BREVLL:
      return nvptx_expand_brev(self,exp, target, mode, ignore);

    default: gcc_unreachable ();
  }
}

/* Set up all builtin functions for this target.  */
//原型 targetm.init_builtins ();#define TARGET_INIT_BUILTINS nvptx_init_builtins
void mtcs_ptx_builtins_init_builtins(MtcsPtxBuiltins *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

#define DEF(ID, NAME, T)                        \
  (self->nvptx_builtin_decls[NVPTX_BUILTIN_ ## ID]                \
   = mtcs_tree_add_builtin_function (mtcsTree,"__builtin_nvptx_" NAME,         \
         mtcs_tree_build_function_type_list T,          \
               NVPTX_BUILTIN_ ## ID, BUILT_IN_MD, NULL, NULL))
#define ST mtcs_sizetype
#define UINT mtcs_unsigned_type_node
#define LLUINT mtcs_long_long_unsigned_type_node
#define PTRVOID mtcs_ptr_type_node
#define VOID mtcs_void_type_node

  DEF (SHUFFLE, "shuffle", (mtcsTree,UINT, UINT, UINT, UINT, NULL_TREE));
  DEF (SHUFFLELL, "shufflell", (mtcsTree,LLUINT, LLUINT, UINT, UINT, NULL_TREE));
  DEF (WORKER_ADDR, "worker_addr",(mtcsTree,PTRVOID, ST, UINT, UINT, NULL_TREE));
  DEF (VECTOR_ADDR, "vector_addr",(mtcsTree,PTRVOID, ST, UINT, UINT, NULL_TREE));
  DEF (CMP_SWAP, "cmp_swap", (mtcsTree,UINT, PTRVOID, UINT, UINT, NULL_TREE));
  DEF (CMP_SWAPLL, "cmp_swapll", (mtcsTree,LLUINT, PTRVOID, LLUINT, LLUINT, NULL_TREE));
  DEF (MEMBAR_GL, "membar_gl", (mtcsTree,VOID, VOID, NULL_TREE));
  DEF (MEMBAR_CTA, "membar_cta", (mtcsTree,VOID, VOID, NULL_TREE));

  DEF (BAR_RED_AND, "bar_red_and",(mtcsTree,UINT, UINT, UINT, UINT, UINT, NULL_TREE));
  DEF (BAR_RED_OR, "bar_red_or", (mtcsTree,UINT, UINT, UINT, UINT, UINT, NULL_TREE));
  DEF (BAR_RED_POPC, "bar_red_popc", (mtcsTree,UINT, UINT, UINT, UINT, UINT, NULL_TREE));

  DEF (BREV, "brev", (mtcsTree,UINT, UINT, NULL_TREE));
  DEF (BREVLL, "brevll", (mtcsTree,LLUINT, LLUINT, NULL_TREE));

#undef DEF
#undef ST
#undef UINT
#undef LLUINT
#undef PTRVOID
}

/**
 * 在这里必调用的是主机的 build_fold_addr_expr 因为 mtcsclones的都是主机tree
 */
static void replaceFndecl (MtcsPtxBuiltins *self, gcall *s ,tree fn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);

   if (TREE_CODE (fn) == FUNCTION_DECL)
      fn = build_fold_addr_expr(fn);
   gimple_set_op (s, 1, fn);
   gimple_call_set_fntype (s, TREE_TYPE (TREE_TYPE (fn)));
   gimple_call_reset_alias_info (s);
}

/**
 * 从主机编译的builtin函数，需要再转化成平台的内置函数
 * 有多少内置函数参见 c-decl.cc的方法header_for_builtin_fn (tree fndecl)
 * 参见gimple_fold_builtin
 */
static nboolean replaceCall_cb(MtcsBuiltins *mtcsBuiltins, gimple *call)
{
   MtcsPtxBuiltins *self=(MtcsPtxBuiltins *)mtcsBuiltins;
   gcall *stmt = as_a <gcall *>(call);
   tree callee = gimple_call_fndecl (stmt);
   if(callee==NULL_TREE)
      return FALSE;
   const char *fnName=IDENTIFIER_POINTER(DECL_NAME(callee));
   location_t loc = gimple_location (call);
   n_debug("mtcsptxbuiltin.c convertCall_cb 00 转化调用 fndecl:%p %s\n",callee,fnName);

   /* Give up for always_inline inline builtins until they are
   inlined.  */
   if (avoid_folding_inline_builtin (callee))
      return FALSE;
   unsigned n = gimple_call_num_args (stmt);
   enum built_in_function fcode = DECL_FUNCTION_CODE (callee);
   n_debug("mtcsptxbuiltin.c convertCall_cb 11 转化调用 fcode:%d BUILT_IN_PRINTF:%d\n",fcode,BUILT_IN_PRINTF);
   switch (fcode){
      case BUILT_IN_PRINTF:
      {
         tree fnPrintf= builtin_decl_implicit (BUILT_IN_PRINTF);
         if(callee!=fnPrintf){
            n_debug("mtcsptxbuiltin.c convertCall_cb printf fnPrintf:%p\n",fnPrintf);
            replaceFndecl(self,stmt,fnPrintf);
            return TRUE;
         }
      }
         return FALSE;
      case BUILT_IN_PUTS:
      {
         n_debug("mtcsptxbuiltin.c convertCall_cb 22 BUILT_IN_PUTS call:%p\n",call);
         tree arg = gimple_call_arg (stmt, 0);
         const char *str = c_getstr (arg);
         size_t len = strlen (str);
         if(str!=NULL && len>=1){
            char newstr[len+10];
            sprintf(newstr,"%s\n",str);
            tree newarg = build_string_literal (strlen(newstr), newstr);
            gimple_call_set_arg (stmt, 0, newarg);
         }
         n_debug("mtcsptxbuiltin.c convertCall_cb 33 BUILT_IN_PUTS:%s\n",str);
         tree fnPrintf= builtin_decl_implicit (BUILT_IN_PRINTF);
         enum built_in_function fcodex = DECL_FUNCTION_CODE (fnPrintf);
         n_debug("mtcsptxbuiltin.c convertCall_cb 44 BUILT_IN_PRINTF:%d %p\n",fcodex,fnPrintf);
         replaceFndecl(self,stmt,fnPrintf);
         n_debug("mtcsptxbuiltin.c convertCall_cb 55 BUILT_IN_PUTS:%s stmt:%p\n",str,stmt);

      }
         return TRUE;
      case BUILT_IN_FPRINTF:
      case BUILT_IN_FPRINTF_UNLOCKED:
      case BUILT_IN_VFPRINTF:
      case BUILT_IN_FWRITE:
         error_at(loc,"ptx 不支持\"%s\"",fnName);
         break;
      case BUILT_IN_VPRINTF:
         error_at(loc,"ptx 不支持\"%s\"",fnName);
         break;
      default:
         break;
   }
   return FALSE;
}

/**
 * 实现父类声明的方法    nboolean (*convert_call)(MtcsBuiltins *self, gimple *call);
 * 在mtcsclones.c中调用mtcs_builtins_convert_call
 */
static gimple  *convertCall_cb(MtcsBuiltins *mtcsBuiltins, gimple *call)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsBuiltins);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsPtx *mtcsPtx=(MtcsPtx*)mtcsTarget;
   MtcsPtxMath *mtcsPtxMath = mtcsPtx->mtcsPtxMath;
   return  mtcs_ptx_math_convert_call(mtcsPtxMath,call);
}


/**
 * 实现父类MtcsBuiltins的抽象方法support_builtin_fn
 */
static nboolean supportBuiltinFn(MtcsBuiltins *mtcsBuiltins, tree fndecl)
{
   if(!fndecl || !fndecl_built_in_p (fndecl))
      return FALSE;
   enum built_in_function fcode = DECL_FUNCTION_CODE (fndecl);
   //n_debug("mtcsptxbuiltins.c supportBuiltinFn  fncode:%d BUILT_IN_STRCHR:%d\n",fcode,BUILT_IN_STRCHR);
   switch(fcode){
      case BUILT_IN_STRCHR:
         return FALSE;
      default:
         break;
   }
   return TRUE;
}

static inline rtx_insn *insnify (MtcsPtxBuiltins *self,rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   if (!x)
      return NULL;
   if (rtx_insn *insn = dyn_cast <rtx_insn *> (x))
      return insn;
   mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
   mtcs_emit_emit/*!emit*/(mtcsEmit,x, false);
   rtx_insn *res = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   return res;
}

/* ../../gcc-151/gcc/config/nvptx/nvptx.md:1863 */
static rtx gen_dim (rtx operand0 ATTRIBUTE_UNUSED,rtx operand1 ATTRIBUTE_UNUSED)
{
   return gen_rtx_SET (operand0,
         gen_rtx_UNSPEC_VOLATILE ((machine_mode)PTX_SImode, gen_rtvec (1, operand1), PTX_UNSPECV_MTCS_DIM));
}

static rtx_insn *target_gen_dim (MtcsPtxBuiltins *self,rtx x0, rtx x1)
{
   return insnify (self,gen_dim/*!gen_oacc_dim_pos*/(x0, x1));
}

/**
 * threadIdx.x
 */
static rtx expand_Dim(MtcsPtxBuiltins *self, tree exp, rtx target, int ignore)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   const char *name;
   rtx fallback_retval;
   rtx_insn *(*gen_fn) (MtcsPtxBuiltins *,rtx, rtx);
   gen_fn = target_gen_dim;

   //第一个参数 表示区域 0 = matDim 1 = unitDim 2=unitIdex 3=threadIdx
   tree arg0 = CALL_EXPR_ARG (exp, 0);
   int region = TREE_INT_CST_LOW (arg0);
   tree arg1 = CALL_EXPR_ARG (exp, 1);
   int dim = TREE_INT_CST_LOW (arg1);
    //把两个参数据合成一个 region 1 dim 1 合成11
   char r[10];
   sprintf(r,"%d%d",region,dim);
   dim = atoi(r);
   if (ignore)
      return target;
   if (target == NULL_RTX)
      target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,TYPE_MODE (TREE_TYPE (exp)));
   rtx reg = MEM_P (target) ? mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (target)) : target;
   n_debug("mtcsptxbuiltins.c expand_Dim 00 dim:%d\n",dim);
   mtcs_print_rtl_single(stderr,target);
   n_debug("mtcsptxbuiltins.c expand_Dim 11\n");
   mtcs_print_rtl_single(stderr,reg);
   mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_fn (self,reg, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,dim)));
   if (reg != target)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, reg);

   return target;
}


//内置函数 .UNIQUE生成RTL流程
//expand_UNIQUE (第一个参数是类型 生成FORK)
//  -->targetm.gen_oacc_fork
//     -->gen_oacc_fork(insn-emit5.cc)
//        -->nvptx_expand_oacc_fork(nvptx.cc)
//          -->nvptx_emit_forking(nvptx.cc)
//             -->gen_nvptx_fork(insn-emit5.cc)
/**
 * ptx_gen_mtcs_local_shared  (define_expand "mtcs_local_shared" mtcs_ptx.md 2619行
 * 调用 mtcs_ptx_builtins_expand_local_shared 再调用 ptx_gen_mtcs_shared
 * 生成的rtl:
 * (insn 8 4 9 2 (unspec_volatile:SI [
            (const_int 0 [0])
        ] PTX_UNSPECV_MTCS_LOCAL_SHARED) 352 {mtcs_shared}
     (nil))
 */
//ptx_gen_mtcs_local_shared 是在 mtcs_ptx.md 中的 (define_expand "mtcs_local_shared" 中声明的
// ptx_gen_mtcs_local_shared 调用 mtcs_ptx_builtins_expand_local_shared
static rtx_insn *target_gen_shared (MtcsPtxBuiltins *self,rtx x0, rtx x1, rtx x2)
{
   return insnify (self,ptx_gen_mtcs_local_shared/*!gen_oacc_dim_pos*/(x0, x1,x2));
}

//expand 调用 __builtin_mtcs_shared_var
//与expand_Dim 区别是一个有左值，一个没有左值，__builtin_mtcs_shared_var 返回值是void
static rtx expand_LocalSharedVar(MtcsPtxBuiltins *self, tree exp, rtx target, int ignore)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   target = const0_rtx;
   //第一个参数 表示 id
   tree arg0 = CALL_EXPR_ARG (exp, 0);
   int id = TREE_INT_CST_LOW (arg0);
   tree arg1 = CALL_EXPR_ARG (exp, 1);
   int reserver = TREE_INT_CST_LOW (arg1); //暂时用不上

   rtx data_dep=mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,id);
   rtx axis=mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,reserver);
   rtx pattern= target_gen_shared (self,target,data_dep,axis);
   if (pattern)
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,pattern);
   n_debug("mtcsptxbuiltins.c expand_LocalSharedVar\n");
   mtcs_print_rtl(stderr,pattern);
   return target;
}

static void expand_syncthreads(MtcsPtxBuiltins *self,tree exp,rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   if (target != const0_rtx){
      error("__syncthreads 没有返回值。");
   }
   int firstParm = 0;
   rtx op0=mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,firstParm);
   int secondParm = 0;
   rtx op1=mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,secondParm);
   rtx_insn *pattern = insnify (self,ptx_gen_nvptx_barsync(op0,op1));
   mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,pattern);
}

/**
 * __atomic_fetch_and (指针 (atomic), float (val), __ATOMIC_SEQ_CST);
 * enum memmodel model= __ATOMIC_SEQ_CST...
 */
static rtx expand_atomic_fetch_add_fs(MtcsPtxBuiltins *self,tree exp,rtx target,machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsBuiltins *mtcsBuiltins=(MtcsBuiltins *)self;
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   tree fndecl = get_callee_fndecl (exp);
   machine_mode target_mode = TYPE_MODE (TREE_TYPE (exp));
   rtx val, mem, ret;
   enum memmodel model;
   if(mode==0 && mode!=target_mode)
      mode =target_mode;
   model = mtcs_builtins_get_memmodel/*!get_memmodel*/(mtcsBuiltins,CALL_EXPR_ARG (exp, 2));
   mem = mtcs_builtins_get_builtin_sync_mem/*!get_builtin_sync_mem*/(mtcsBuiltins,CALL_EXPR_ARG (exp, 0), mode);
   val =mtcs_builtins_expand_expr_force_mode/*!expand_expr_force_mode*/(mtcsBuiltins,CALL_EXPR_ARG (exp, 1), mode);
   rtx returnReg = MEM_P (target) ? mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mtcsMode->modes.M_SFmode) : target;
   rtx op0=returnReg;
   rtx op1=mem;
   rtx op2=val;
   rtx op3=mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,model);
   //      {
   //         //"%.\\tatom%A1.add%t0\\t%x0, %1, %2;";指令中的A1匹配区域，用户不加修饰 global shared
   //         //默认区域是 generic.所以不会输出像nvcc带global修饰的atom add指令。
   //         //nvcc : atom.global.add.f32  %f4, [%r1], %f3;
   //         //因此在这里判断如果XEXP (mem, 0)是 SYMBOL_REF并且是generic就加区域为 PTX_DATA_AREA_GLOBAL
   //         n_debug("mtcsptxbuiltins.c expandBuiltinFn_cb 00 fndecl:%p tmode:%d mode:%d\n",fndecl,target_mode,mode);
   //         mtcs_print_rtl(stderr,mem);
   //         rtx  x = XEXP (mem, 0);
   //         n_debug("mtcsptxbuiltins.c expandBuiltinFn_cb 11 fndecl:%p tmode:%d mode:%d\n",fndecl,target_mode,mode);
   //         mtcs_print_rtl(stderr,x);
   //         if (GET_CODE (x) == CONST)
   //               x = XEXP (x, 0);
   //         if (GET_CODE (x) == PLUS)
   //               x = XEXP (x, 0);
   //         n_debug("mtcsptxbuiltins.c expandBuiltinFn_cb 22 fndecl:%p tmode:%d mode:%d\n",fndecl,target_mode,mode);
   //         mtcs_print_rtl(stderr,x);
   //         if (GET_CODE (x) == SYMBOL_REF){
   //            char *area=ptx_tool_section_for_sym(x);
   //            if(area && strlen(area)==0){
   //               //设为global
   //               PTX_SET_SYMBOL_DATA_AREA (x, PTX_DATA_AREA_GLOBAL);
   //            }
   //         }
   //      }
   rtx_insn *pattern = insnify (self,ptx_gen_atomic_fetch_addsf(op0,op1,op2,op3));
   mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,pattern);
   if (returnReg != target)
        mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, returnReg);
   return target;
}

/* Expander for the shuffle builtins.  */
//原型  nvptx_expand_shuffle nvptx.cc
static rtx expand_shfl_xor_sync (MtcsPtxBuiltins *self,tree exp, rtx target, machine_mode mode, int ignore)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsPtxEmit *mtcsPtxEmit=(MtcsPtxEmit *)mtcsEmit;

  n_debug("mtcsptxbuiltin.c expand_shfl_xor_sync 00 mode:%d ignore:%d\n",mode,ignore);

  if (ignore)
    return target;

  //参数1=member mask 例如 0xffffffff 在 shfl.sync.bfly.b32    %r5|%p1, %r1, %r3, %r2, %r4; 是最后一个参数%r4
  rtx memberMask = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,CALL_EXPR_ARG (exp, 0),
             NULL_RTX, mtcsMode->modes.M_SImode, EXPAND_NORMAL);

  //参数2=src 可能是si或sf shfl.sync.bfly.b32   %r5, %r1, %r3, %r2, %r4; 在指令中是第一个位置 %r1
  rtx src = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,CALL_EXPR_ARG (exp, 1),
             NULL_RTX, mode, EXPAND_NORMAL);
  mtcs_print_rtl(stderr,src);
  if (!REG_P (src))
    src =mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,mode, src);
  //参数3=xor的lane mask 32 16 8 shfl.sync.bfly.b32   %r5, %r1, %r3, %r2, %r4; 在指令中是第二个位置 %r3
  rtx laneMask = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,CALL_EXPR_ARG  (exp, 2),
            NULL_RTX, mtcsMode->modes.M_SImode, EXPAND_NORMAL);

  n_debug("mtcsptxbuiltin.c expand_shfl_xor_sync 11 mode:%d ignore:%d\n",mode,ignore);
  mtcs_print_rtl(stderr,target);

  mtcs_print_rtl(stderr,memberMask);
  mtcs_print_rtl(stderr,src);
  mtcs_print_rtl(stderr,laneMask);

  if (!REG_P (memberMask) && GET_CODE (memberMask) != CONST_INT)
     memberMask = mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,mtcsMode->modes.M_SImode, memberMask);

  if (!REG_P (laneMask) && GET_CODE (laneMask) != CONST_INT)
     laneMask = mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,mtcsMode->modes.M_SImode, laneMask);

  rtx returnReg = MEM_P (target) ? mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode) : target;

  rtx pat = mtcs_ptx_emit_shfl_xor_sync/*!nvptx_gen_shuffle*/(mtcsPtxEmit,returnReg, src, laneMask, memberMask);
  n_debug("mtcsptxbuiltin.c expand_shfl_xor_sync 22 mode:%d ignore:%d\n",mode,ignore);
  mtcs_print_rtl(stderr,pat);

  if (pat)
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,pat);
  if (returnReg != target)
       mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, returnReg);
  return target;
}


//原型 expand_builtin builtins.h builtins.cc
static rtx expandBuiltinFn_cb(MtcsBuiltins *mtcsBuiltins,tree exp, rtx target, rtx subtarget, machine_mode mode, int ignore)
{
   MtcsPtxBuiltins *self = (MtcsPtxBuiltins *)mtcsBuiltins;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);

   tree fndecl = get_callee_fndecl (exp);
   const char *funName=IDENTIFIER_POINTER(DECL_NAME(fndecl));
   if(!strcmp(funName,"__syncthreads")){
      n_debug("mtcsptxbuiltins.c expandBuiltinFn_cb 00 %s ignore:%d\n",funName,ignore);
      expand_syncthreads(self,exp,target);
   }else if(!strcmp(funName,"__atomic_fetch_add_fs")){
      return expand_atomic_fetch_add_fs(self,exp,target,mode);
   }else if(!strcmp(funName,"__shfl_xor_sync") || !strcmp(funName,"__shfl_xor_sync_fs")){
      //return nvptx_expand_shuffle(self,exp, target, mode, ignore);
      return expand_shfl_xor_sync(self,exp, target, mode, ignore);
   }
   return NULL;
}



static rtx expandInternalFn_cb(MtcsBuiltins *mtcsBuiltins,tree exp, rtx target, rtx subtarget, machine_mode mode, int ignore)
{
   MtcsPtxBuiltins *self = (MtcsPtxBuiltins *)mtcsBuiltins;
   tree fndecl = get_callee_fndecl (exp);
   int fncode = mtcs_builtins_get_code(fndecl);
   if(fncode == INTERNEL_CODE_DIM){
      n_debug("mtcsptxbuiltins.c expandInternalFn_cb 00 %s ignore:%d\n",IDENTIFIER_POINTER(DECL_NAME(fndecl)),ignore);
      return expand_Dim(self,exp,target,ignore);
   }else if(fncode==INTERNAL_CODE_SHARED_VAR){
      n_debug("mtcsptxbuiltins.c expandInternalFn_cb 11 %s ignore:%d\n",IDENTIFIER_POINTER(DECL_NAME(fndecl)),ignore);
      return expand_LocalSharedVar(self,exp,target,ignore);
   }
   return NULL;
}

/**源代码
 * uint x = gridDim.x;
 * gridDim.x翻译为汇编代码 "%.\tmov.u32\t%0, %%nctaid.x;"
 * mtcs_ptx_builtins_get_asm_dim 在 mtcs_ptx.md中被调用
 */
char *mtcs_ptx_builtins_get_asm_dim(MtcsPtxBuiltins *self,int pos)
{
   n_debug("mtcsptxbuiltin.c 进这里可以生成griddim block thread 汇编代码了---- %d\n",pos);
   if(pos==0)
      return "%.\tmov.u32\t%0, %%nctaid.x;"; //matDim.x = gridDim.x
   else if(pos==1)
      return "%.\tmov.u32\t%0, %%nctaid.y;"; //matDim.y = gridDim.y
   else if(pos==2)
      return "%.\tmov.u32\t%0, %%nctaid.y;"; //matDim.z = gridDim.z
   else if(pos==10)
      return "%.\tmov.u32\t%0, %%ntid.x;";  //unitDim.x = blockDim.x
   else if(pos==11)
      return "%.\tmov.u32\t%0, %%ntid.y;";  //unitDim.y = blockDim.y
   else if(pos==12)
      return "%.\tmov.u32\t%0, %%ntid.z;"; //unitDim.z = blockDim.z
   else if(pos==20)
      return "%.\tmov.u32\t%0, %%ctaid.x;"; //unitIdx.x = blockIdx.x
   else if(pos==21)
      return "%.\tmov.u32\t%0, %%ctaid.y;"; //unitIdx.y = blockIdx.y
   else if(pos==22)
      return "%.\tmov.u32\t%0, %%ctaid.z;"; //unitIdx.z = blockIdx.z
   else if(pos==30)
      return "%.\tmov.u32\t%0, %%tid.x;"; //threadIdx.x = threadIdx.x
   else if(pos==31)
      return "%.\tmov.u32\t%0, %%tid.y;"; //threadIdx.y = threadIdx.y
   else if(pos==32)
      return "%.\tmov.u32\t%0, %%tid.z;"; //threadIdx.z = threadIdx.z
   else
      gcc_unreachable();
   return "";
}


//mtcs_ptx.md中被调用 具体是方法 ptx_gen_mtcs_local_shared 调用
//原型 nvptx_expand_oacc_fork mtcs_
void mtcs_ptx_builtins_expand_local_shared  (MtcsPtxBuiltins *self,int id,int reserver)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   rtx op=mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,id);
   mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,ptx_gen_mtcs_shared (op));
}

///////////////以下实现 memcpy memset--------------------------
enum ptx_stringop_alg
{
   ptx_no_stringop,
   ptx_libcall,
   ptx_loop_1_byte,
   ptx_loop,
   ptx_unrolled_loop,
   ptx_vector_loop,
   ptx_last_alg,
};

/* Copy the address to a Pmode register.  This is used for x32 to
   truncate DImode TLS address to a SImode register. */
//原型 ix86_copy_addr_to_reg i386-expand.cc
static rtx ptx_copy_addr_to_reg (MtcsPtxBuiltins *self,rtx addr)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   mtcs_mode pMode=mtcs_mode_get_Pmode(mtcsMode);

   rtx reg;
   if (GET_MODE (addr) == pMode || GET_MODE (addr) == VOIDmode){
      reg = mtcs_explow_copy_addr_to_reg/*!copy_addr_to_reg*/(mtcsExplow,addr);
      REG_POINTER (reg) = 1;
      return reg;
   }else{
      gcc_assert (GET_MODE (addr) ==mtcsMode->modes.M_DImode && pMode == mtcsMode->modes.M_SImode);
      reg = mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,mtcsMode->modes.M_DImode, addr);
      REG_POINTER (reg) = 1;
      return mtcs_rtl_gen_rtx_SUBREG/*!gen_rtx_SUBREG*/(mtcsRTL,mtcsMode->modes.M_SImode, reg, 0);
   }
}

/* Given COUNT and EXPECTED_SIZE, decide on codegen of string operation.  */
//原型 decide_alg i386-expand.cc
static enum ptx_stringop_alg decide_alg (HOST_WIDE_INT count, HOST_WIDE_INT expected_size,
       unsigned HOST_WIDE_INT min_size, unsigned HOST_WIDE_INT max_size,
       bool memset, bool zero_memset, bool have_as,
       int *dynamic_check, bool *noalign, bool recur)
{
   bool optimize_for_speed;
   int max = 0;
   int i;
   bool any_alg_usable_p = false;

   *noalign = false;
   *dynamic_check = -1;

   return ptx_loop_1_byte;//ptx_unrolled_loop;//;
}


/* Decide on alignment.  We know that the operand is already aligned to ALIGN
   (ALIGN can be based on profile feedback and thus it is not 100% guaranteed).  */
//原型 decide_alignment i386-expand.cc
static int decide_alignment (MtcsPtxBuiltins *self,int align,
        enum ptx_stringop_alg alg,
        int expected_size,
        machine_mode move_mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem  *mtcsOptionsItem=mtcsOptions->global_options;

   int desired_align = 0;

   gcc_assert (alg != ptx_no_stringop);

   if (alg == ptx_libcall)
      return 0;
   if (move_mode == VOIDmode)
      return 0;

   desired_align = mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,move_mode);
   /* PentiumPro has special logic triggering for 8 byte aligned blocks.
   copying whole cacheline at once.  */
   /*
   if (TARGET_CPU_P (PENTIUMPRO) && (alg == rep_prefix_4_byte || alg == rep_prefix_1_byte))
   desired_align = 8;
   */

   if (mtcsOptionsItem->x_optimize_size)
      desired_align = 1;
   if (desired_align < align)
      desired_align = align;
   if (expected_size != -1 && expected_size < 4)
      desired_align = align;

   return desired_align;
}

/* Return mode for the memcpy/memset loop counter.  Prefer SImode over
   DImode for constant loop counts.  */
//原型 counter_mode i386-expand.cc
static machine_mode counter_mode(MtcsPtxBuiltins *self,rtx count_exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   if (GET_MODE (count_exp) != VOIDmode)
      return GET_MODE (count_exp);
   if (!CONST_INT_P (count_exp))
      return mtcs_mode_get_Pmode(mtcsMode);

   if (TARGET_64BIT && (INTVAL (count_exp) & ~0xffffffff))
      return mtcsMode->modes.M_DImode;
   return mtcsMode->modes.M_SImode;
}

/* Helper function for memcpy.  For QImode value 0xXY produce
   0xXYXYXYXY of wide specified by MODE.  This is essentially
   a * 0x10101010, but we can do slightly better than
   synth_mult by unwinding the sequence by hand on CPUs with
   slow multiply.  */
//原型 promote_duplicated_reg i386-expand.cc
static rtx promote_duplicated_reg (MtcsPtxBuiltins *self,machine_mode mode, rtx val)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   machine_mode valmode = GET_MODE (val);
   rtx tmp;
   int nops = mode == mtcsMode->modes.M_DImode ? 3 : 2;

   gcc_assert (mode == mtcsMode->modes.M_SImode || mode == mtcsMode->modes.M_DImode || val == const0_rtx);
   if (val == const0_rtx)
      return mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,mode, CONST0_RTX (mode));
   if (CONST_INT_P (val)){
      HOST_WIDE_INT v = INTVAL (val) & 255;

      v |= v << 8;
      v |= v << 16;
      if (mode == mtcsMode->modes.M_DImode)
         v |= (v << 16) << 16;
      return mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,
            mode, mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,v, mode));
   }

   if (valmode == VOIDmode)
      valmode = mtcsMode->modes.M_QImode;
   if (valmode != mtcsMode->modes.M_QImode)
      val = gen_lowpart (mtcsMode->modes.M_QImode, val);
   if (mode == mtcsMode->modes.M_QImode)
      return val;
   //if (!TARGET_PARTIAL_REG_STALL)
      nops--;
   if (/*!ix86_cost->mult_init[mode == DImode ? 3 : 2]
   + ix86_cost->mult_bit * (mode == DImode ? 8 : 4)
   <= (ix86_cost->shift_const + ix86_cost->add) * nops
   + (COSTS_N_INSNS (TARGET_PARTIAL_REG_STALL == 0))*/5>3){
      rtx reg = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,mode, mtcsMode->modes.M_QImode, val, true);
      tmp = promote_duplicated_reg(self,mode, const1_rtx);
      return mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
            mode, MULT, reg, tmp, NULL, 1, OPTAB_DIRECT);
   }else{
      rtx reg = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,mode, mtcsMode->modes.M_QImode, val, true);

      if (!TARGET_PARTIAL_REG_STALL)
        n_error("不支持 promote_duplicated_reg");/*!emit_insn (gen_insv_1 (mode, reg, reg));*/
      else{
         tmp = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,mode,
               ASHIFT, reg, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,8),NULL, 1, OPTAB_DIRECT);
         reg = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
               mode, IOR, reg, tmp, reg, 1,OPTAB_DIRECT);
      }
      tmp = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
            mode, ASHIFT, reg, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,16), NULL, 1, OPTAB_DIRECT);
      reg = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
            mode, IOR, reg, tmp, reg, 1, OPTAB_DIRECT);
      if (mode == mtcsMode->modes.M_SImode)
         return reg;
      tmp = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
            mode, ASHIFT, reg, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,32), NULL, 1, OPTAB_DIRECT);
      reg = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
            mode, IOR, reg, tmp, reg, 1, OPTAB_DIRECT);
      return reg;
   }
}

/* Duplicate value VAL using promote_duplicated_reg into maximal size that will
   be needed by main loop copying SIZE_NEEDED chunks and prologue getting
   alignment from ALIGN to DESIRED_ALIGN.  */
//原型 promote_duplicated_reg_to_size i386-expand.cc
static rtx promote_duplicated_reg_to_size (MtcsPtxBuiltins *self,
      rtx val, int size_needed, int desired_align,int align)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   rtx promoted_val;

   if (TARGET_64BIT
   && (size_needed > 4 || (desired_align > align && desired_align > 4)))
      promoted_val = promote_duplicated_reg(self,mtcsMode->modes.M_DImode, val);
   else if (size_needed > 2 || (desired_align > align && desired_align > 2))
      promoted_val = promote_duplicated_reg(self,mtcsMode->modes.M_SImode, val);
   else if (size_needed > 1 || (desired_align > align && desired_align > 1))
      promoted_val = promote_duplicated_reg(self,mtcsMode->modes.M_HImode, val);
   else
      promoted_val = val;

   return promoted_val;
}

/* Predict just emitted jump instruction to be taken with probability PROB.  */
//原型  predict_jump i386-expand.cc
static void predict_jump (MtcsPtxBuiltins *self,int prob)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   rtx_insn *insn = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
   gcc_assert (JUMP_P (insn));
   add_reg_br_prob_note (insn, profile_probability::from_reg_br_prob_base (prob));
}

/* Helper function for the string operations below.  Dest VARIABLE whether
   it is aligned to VALUE bytes.  If true, jump to the label.  */
//原型 ix86_expand_aligntest i386-expand.cc
static rtx_code_label * ptx_expand_aligntest (MtcsPtxBuiltins *self,rtx variable, int value, bool epilogue)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   rtx_code_label *label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
   rtx tmpcount = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (variable));
   if (GET_MODE (variable) == mtcsMode->modes.M_DImode)
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,ptx_gen_anddi3/*!gen_anddi3*/(tmpcount, variable, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,value)));
   else
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,ptx_gen_andsi3/*!gen_andsi3*/(tmpcount,
            variable, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,value)));
   mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,
         tmpcount, const0_rtx, EQ, 0, GET_MODE (variable), 1, label);
   if (epilogue)
      predict_jump(self,REG_BR_PROB_BASE * 50 / 100);
   else
      predict_jump(self,REG_BR_PROB_BASE * 90 / 100);
   return label;
}


/* Output code to copy at most count & (max_size - 1) bytes from SRC to DEST.  */
//原型 expand_cpymem_epilogue i386-expand.cc
static void expand_cpymem_epilogue (MtcsPtxBuiltins *self,rtx destmem, rtx srcmem,
         rtx destptr, rtx srcptr, rtx count, int max_size)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);

   rtx src, dest;
   if (CONST_INT_P (count)){
      HOST_WIDE_INT countval = INTVAL (count);
      HOST_WIDE_INT epilogue_size = countval % max_size;
      int i;

      /* For now MAX_SIZE should be a power of 2.  This assert could be
      relaxed, but it'll require a bit more complicated epilogue
      expanding.  */
      gcc_assert ((max_size & (max_size - 1)) == 0);
      for (i = max_size; i >= 1; i >>= 1){
         if (epilogue_size & i)
            destmem = emit_memmov(self,destmem, &srcmem, destptr, srcptr, i);
      }
      return;
   }
   if (max_size > 8){
      count = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
            GET_MODE (count), AND, count, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,max_size - 1),
               count, 1, OPTAB_DIRECT);
      expand_set_or_cpymem_via_loop(self,destmem, srcmem, destptr, srcptr, NULL, count,
            mtcsMode->modes.M_QImode, 1, 4, false);
      return;
   }

   /* When there are stringops, we can cheaply increase dest and src pointers.
   Otherwise we save code size by maintaining offset (zero is readily
   available from preceding rep operation) and using x86 addressing modes.
   */
   if (TARGET_SINGLE_STRINGOP){
      if (max_size > 4){
         rtx_code_label *label = ptx_expand_aligntest/*!ix86_expand_aligntest*/(self,count, 4, true);
         src = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,srcmem, mtcsMode->modes.M_SImode, srcptr);
         dest = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,destmem, mtcsMode->modes.M_SImode, destptr);
         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_strmov (destptr, dest, srcptr, src));
         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
         LABEL_NUSES (label) = 1;
      }
      if (max_size > 2){
         rtx_code_label *label = ptx_expand_aligntest/*!ix86_expand_aligntest*/(self,count, 2, true);
         src = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,srcmem, mtcsMode->modes.M_HImode, srcptr);
         dest = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,destmem, mtcsMode->modes.M_HImode, destptr);
         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_strmov (destptr, dest, srcptr, src));
         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
         LABEL_NUSES (label) = 1;
      }
      if (max_size > 1){
         rtx_code_label *label = ptx_expand_aligntest/*!ix86_expand_aligntest*/(self,count, 1, true);
         src = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,srcmem, mtcsMode->modes.M_QImode, srcptr);
         dest = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,destmem, mtcsMode->modes.M_QImode, destptr);
         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_strmov (destptr, dest, srcptr, src));
         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
         LABEL_NUSES (label) = 1;
      }
   }else{
      rtx offset = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,pMode, const0_rtx);
      rtx tmp;

      if (max_size > 4){
         rtx_code_label *label = ptx_expand_aligntest/*!ix86_expand_aligntest*/(self,count, 4, true);
         src = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,srcmem, mtcsMode->modes.M_SImode, srcptr);
         dest = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,destmem, mtcsMode->modes.M_SImode, destptr);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,dest, src);
         tmp = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
               pMode, PLUS, offset, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,4), NULL,true, OPTAB_LIB_WIDEN);
         if (tmp != offset)
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,offset, tmp);
         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
         LABEL_NUSES (label) = 1;
      }
      if (max_size > 2){
         rtx_code_label *label = ptx_expand_aligntest/*!ix86_expand_aligntest*/(self,count, 2, true);
         tmp = gen_rtx_PLUS (pMode, srcptr, offset);
         src = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,srcmem, mtcsMode->modes.M_HImode, tmp);
         tmp = gen_rtx_PLUS (pMode, destptr, offset);
         dest = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,destmem, mtcsMode->modes.M_HImode, tmp);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,dest, src);
         tmp = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
               pMode, PLUS, offset, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,2), tmp,true, OPTAB_LIB_WIDEN);
         if (tmp != offset)
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,offset, tmp);
         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
         LABEL_NUSES (label) = 1;
      }
      if (max_size > 1){
         rtx_code_label *label = ptx_expand_aligntest/*!ix86_expand_aligntest*/(self,count, 1, true);
         tmp = gen_rtx_PLUS (pMode, srcptr, offset);
         src = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,srcmem, mtcsMode->modes.M_QImode, tmp);
         tmp = gen_rtx_PLUS (pMode, destptr, offset);
         dest = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,destmem, mtcsMode->modes.M_QImode, tmp);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,dest, src);
         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
         LABEL_NUSES (label) = 1;
      }
   }
}

/* Output code to set at most count & (max_size - 1) bytes starting by DEST.  */
//原型 expand_setmem_epilogue_via_loop i386-expand.cc
static void expand_setmem_epilogue_via_loop (MtcsPtxBuiltins *self,rtx destmem, rtx destptr, rtx value,
             rtx count, int max_size)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

   count = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,counter_mode(self,count), AND, count,
            mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,max_size - 1), count, 1, OPTAB_DIRECT);
   expand_set_or_cpymem_via_loop(self,destmem, NULL, destptr, NULL,
            gen_lowpart (mtcsMode->modes.M_QImode, value), count, mtcsMode->modes.M_QImode,
                  1, max_size / 2, true);
}

//原型  gen_strset i386平台
static rtx ptx_gen_strset(rtx operand0,
   rtx operand1,
   rtx operand2)
{
   n_error("ptx没实现 gen_strset");
   return NULL;
}

/* Output code to set at most count & (max_size - 1) bytes starting by DEST.  */
//原型 expand_setmem_epilogue i386-expand.cc
static void expand_setmem_epilogue (MtcsPtxBuiltins *self,rtx destmem, rtx destptr, rtx value, rtx vec_value,
         rtx count, int max_size)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

   rtx dest;

   if (CONST_INT_P (count)){
      HOST_WIDE_INT countval = INTVAL (count);
      HOST_WIDE_INT epilogue_size = countval % max_size;
      int i;

      /* For now MAX_SIZE should be a power of 2.  This assert could be
      relaxed, but it'll require a bit more complicated epilogue
      expanding.  */
      gcc_assert ((max_size & (max_size - 1)) == 0);
      for (i = max_size; i >= 1; i >>= 1){
         if (epilogue_size & i){
            if (vec_value && i > mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,GET_MODE (value)))
               destmem = emit_memset(self,destmem, destptr, vec_value, i);
            else
               destmem = emit_memset(self,destmem, destptr, value, i);
         }
      }
      return;
   }
   if (max_size > 32){
      expand_setmem_epilogue_via_loop(self,destmem, destptr, value, count, max_size);
      return;
   }
   if (max_size > 16){
      rtx_code_label *label = ptx_expand_aligntest/*!ix86_expand_aligntest*/(self,count, 16, true);
      if (TARGET_64BIT){
         dest =  mtcs_rtl_change_address/*!change_address*/(mtcsRTL,destmem, DImode, destptr);
         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,ptx_gen_strset(destptr, dest, value));
         dest = mtcs_rtl_adjust_automodify_address_nv/*!adjust_automodify_address_nv*/(mtcsRTL,dest, DImode, destptr, 8);
         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,ptx_gen_strset(destptr, dest, value));
      }else{
         dest =  mtcs_rtl_change_address/*!change_address*/(mtcsRTL,destmem, mtcsMode->modes.M_SImode, destptr);
         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,ptx_gen_strset(destptr, dest, value));
         dest = mtcs_rtl_adjust_automodify_address_nv/*!adjust_automodify_address_nv*/(mtcsRTL,
               dest, mtcsMode->modes.M_SImode, destptr, 4);
         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,ptx_gen_strset(destptr, dest, value));
         dest = mtcs_rtl_adjust_automodify_address_nv/*!adjust_automodify_address_nv*/(mtcsRTL,
               dest, mtcsMode->modes.M_SImode, destptr, 8);
         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,ptx_gen_strset(destptr, dest, value));
         dest = mtcs_rtl_adjust_automodify_address_nv/*!adjust_automodify_address_nv*/(mtcsRTL,
               dest, mtcsMode->modes.M_SImode, destptr, 12);
         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,ptx_gen_strset(destptr, dest, value));
      }
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
      LABEL_NUSES (label) = 1;
   }
   if (max_size > 8){
      rtx_code_label *label = ptx_expand_aligntest/*!ix86_expand_aligntest*/(self,count, 8, true);
      if (TARGET_64BIT){
         dest =  mtcs_rtl_change_address/*!change_address*/(mtcsRTL,destmem, DImode, destptr);
         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,ptx_gen_strset(destptr, dest, value));
      }else{
         dest =  mtcs_rtl_change_address/*!change_address*/(mtcsRTL,destmem, mtcsMode->modes.M_SImode, destptr);
         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,ptx_gen_strset(destptr, dest, value));
         dest = mtcs_rtl_adjust_automodify_address_nv/*!adjust_automodify_address_nv*/(mtcsRTL,
               dest, mtcsMode->modes.M_SImode, destptr, 4);
         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,ptx_gen_strset(destptr, dest, value));
      }
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
      LABEL_NUSES (label) = 1;
   }
   if (max_size > 4){
      rtx_code_label *label = ptx_expand_aligntest/*!ix86_expand_aligntest*/(self,count, 4, true);
      dest =  mtcs_rtl_change_address/*!change_address*/(mtcsRTL,destmem, mtcsMode->modes.M_SImode, destptr);
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,
            ptx_gen_strset(destptr, dest, gen_lowpart (mtcsMode->modes.M_SImode, value)));
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
      LABEL_NUSES (label) = 1;
   }
   if (max_size > 2){
      rtx_code_label *label = ptx_expand_aligntest/*!ix86_expand_aligntest*/(self,count, 2, true);
      dest =  mtcs_rtl_change_address/*!change_address*/(mtcsRTL,destmem, HImode, destptr);
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,ptx_gen_strset(destptr, dest, gen_lowpart (HImode, value)));
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
      LABEL_NUSES (label) = 1;
   }
   if (max_size > 1){
      rtx_code_label *label = ptx_expand_aligntest/*!ix86_expand_aligntest*/(self,count, 1, true);
      dest =  mtcs_rtl_change_address/*!change_address*/(mtcsRTL,destmem, mtcsMode->modes.M_QImode, destptr);
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,ptx_gen_strset(destptr, dest, gen_lowpart (mtcsMode->modes.M_QImode, value)));
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
      LABEL_NUSES (label) = 1;
   }
}

/* When ISSETMEM is FALSE, output simple loop to move memory pointer to SRCPTR
   to DESTPTR via chunks of MODE unrolled UNROLL times, overall size is COUNT
   specified in bytes.  When ISSETMEM is TRUE, output the equivalent loop to set
   memory by VALUE (supposed to be in MODE).

   The size is rounded down to whole number of chunk size moved at once.
   SRCMEM and DESTMEM provide MEMrtx to feed proper aliasing info.  */
//原型 expand_set_or_cpymem_via_loop i386-expand.cc
static void expand_set_or_cpymem_via_loop (MtcsPtxBuiltins *self,rtx destmem, rtx srcmem,
                rtx destptr, rtx srcptr, rtx value,
                rtx count, machine_mode mode, int unroll,
                int expected_size, bool issetmem)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
   n_debug("expand_set_or_cpymem_via_loop 00 mode:%d %s unroll:%d expected_size:%d\n",
         mode,mtcs_mode_get_name(mtcsMode,mode),unroll,expected_size);
   printrtl(self);

   rtx_code_label *out_label, *top_label;
   rtx iter, tmp;
   machine_mode iter_mode = counter_mode(self,count);
   int piece_size_n = mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode) * unroll;
   rtx piece_size = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,piece_size_n);
   rtx piece_size_mask = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,
         ~((mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode) * unroll) - 1));
   rtx size;
   int i;

   top_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
   out_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
   iter = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,iter_mode);

   size = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
         iter_mode, AND, count, piece_size_mask,NULL, 1, OPTAB_DIRECT);
   /* Those two should combine.  */
   if (piece_size == const1_rtx){
      mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,
            size, const0_rtx, EQ, NULL_RTX, iter_mode,true, out_label);
      predict_jump(self,REG_BR_PROB_BASE * 10 / 100);
   }
   mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,iter, const0_rtx);
   mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,top_label);
   tmp = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,pMode, iter_mode, iter, true);

   /* This assert could be relaxed - in this case we'll need to compute
   smallest power of two, containing in PIECE_SIZE_N and pass it to
   offset_address.  */
   gcc_assert ((piece_size_n & (piece_size_n - 1)) == 0);
   destmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,destmem, tmp, piece_size_n);
   destmem = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,destmem, mode, 0);
   n_debug("expand_set_or_cpymem_via_loop 11 mode:%d %s unroll:%d expected_size:%d\n",
         mode,mtcs_mode_get_name(mtcsMode,mode),unroll,expected_size);
   printrtl(self);
   if (!issetmem){
      srcmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,srcmem, copy_rtx (tmp), piece_size_n);
      srcmem = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,srcmem, mode, 0);

      /* When unrolling for chips that reorder memory reads and writes,
      we can save registers by using single temporary.
      Also using 4 temporaries is overkill in 32bit mode.  */
      if (!TARGET_64BIT && 0){
         for (i = 0; i < unroll; i++){
            if (i){
               destmem = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,copy_rtx (destmem), mode,
               mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode));
               srcmem = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,copy_rtx (srcmem), mode,
               mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode));
            }
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,destmem, srcmem);
         }
      }else{
         rtx tmpreg[4];
         gcc_assert (unroll <= 4);
         for (i = 0; i < unroll; i++){
            tmpreg[i] = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
            if (i)
               srcmem = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,copy_rtx (srcmem), mode,
                              mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode));
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,tmpreg[i], srcmem);
         }
         for (i = 0; i < unroll; i++){
            if (i)
               destmem = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,copy_rtx (destmem), mode,
                           mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode));
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,destmem, tmpreg[i]);
         }
      }
   }else
      for (i = 0; i < unroll; i++){
         if (i)
            destmem = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,copy_rtx (destmem), mode,
                  mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode));
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,destmem, value);
      }
   n_debug("expand_set_or_cpymem_via_loop 22 mode:%d %s unroll:%d expected_size:%d\n",
         mode,mtcs_mode_get_name(mtcsMode,mode),unroll,expected_size);
   printrtl(self);
   tmp = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
         iter_mode, PLUS, iter, piece_size, iter,true, OPTAB_LIB_WIDEN);
   if (tmp != iter)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,iter, tmp);

   mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,
         iter, size, LT, NULL_RTX, iter_mode,true, top_label);
   if (expected_size != -1){
      expected_size /= mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode) * unroll;
      if (expected_size == 0)
         predict_jump(self,0);
      else if (expected_size > REG_BR_PROB_BASE)
         predict_jump(self,REG_BR_PROB_BASE - 1);
      else
         predict_jump(self,REG_BR_PROB_BASE - (REG_BR_PROB_BASE + expected_size / 2) / expected_size);
   }else
      predict_jump(self,REG_BR_PROB_BASE * 80 / 100);
   iter = ptx_zero_extend_to_Pmode/*!ix86_zero_extend_to_Pmode*/(self,iter);
   tmp = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
         pMode, PLUS, destptr, iter, destptr,true, OPTAB_LIB_WIDEN);
   if (tmp != destptr)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,destptr, tmp);
   if (!issetmem){
      tmp = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
            pMode, PLUS, srcptr, iter, srcptr,true, OPTAB_LIB_WIDEN);
      if (tmp != srcptr)
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,srcptr, tmp);
   }
   mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,out_label);
}

/* Test if COUNT&SIZE is nonzero and if so, expand movme
   or setmem sequence that is valid for SIZE..2*SIZE-1 bytes
   and jump to DONE_LABEL.  */
//原型 expand_small_cpymem_or_setmem i386-expand.cc
static void expand_small_cpymem_or_setmem (MtcsPtxBuiltins *self,rtx destmem, rtx srcmem,
                rtx destptr, rtx srcptr,
                rtx value, rtx vec_value,
                rtx count, int size,
                rtx done_label, bool issetmem)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);

   rtx_code_label *label = ptx_expand_aligntest/*!ix86_expand_aligntest*/(self,count, size, false);
   machine_mode mode = mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,size * BITS_PER_UNIT, 1).else_blk ();
   rtx modesize;
   int n;

   /* If we do not have vector value to copy, we must reduce size.  */
   if (issetmem){
      if (!vec_value){
         if (GET_MODE (value) == VOIDmode && size > 8)
            mode = pMode;
         else if (mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode) >
                     mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,GET_MODE (value)))
            mode = GET_MODE (value);
      }else
         mode = GET_MODE (vec_value), value = vec_value;
   }else{
      /* Choose appropriate vector mode.  */
      /*! ptx没有 avx
      if (size >= 32)
         mode = TARGET_AVX ? V32QImode : TARGET_SSE ? V16QImode : DImode;
      else if (size >= 16)
         mode = TARGET_SSE ? V16QImode : DImode;
      */
      if (size >= 32)
          mode = mtcsMode->modes.M_DImode;
       else if (size >= 16)
          mode =  mtcsMode->modes.M_DImode;
      n_debug("expand_small_cpymem_or_setmem 00aa mode:%d size:%d\n",mode,size);

      srcmem =  mtcs_rtl_change_address/*!change_address*/(mtcsRTL,srcmem, mode, srcptr);
   }
   destmem =  mtcs_rtl_change_address/*!change_address*/(mtcsRTL,destmem, mode, destptr);
   modesize = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode));
   gcc_assert (mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode) <= size);
   for (n = 0; n * mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode) < size; n++){
      if (issetmem)
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,destmem, gen_lowpart (mode, value));
      else{
         n_debug("expand_small_cpymem_or_setmem 00 %p %p\n",destmem,srcmem);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,destmem, srcmem);
         srcmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,
               srcmem, modesize, mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode));
      }
      destmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,
            destmem, modesize, mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode));
   }

   destmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,destmem, count, 1);
   destmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,
         destmem, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,-2 * size),
         mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode));
   if (!issetmem){
      srcmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,srcmem, count, 1);
      srcmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,
            srcmem, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,-2 * size),
            mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode));
   }
   for (n = 0; n * mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode) < size; n++){
      if (issetmem)
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,destmem, gen_lowpart (mode, value));
      else{
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,destmem, srcmem);
         srcmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,
               srcmem, modesize, mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode));
      }
      destmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,
            destmem, modesize, mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode));
   }
   mtcs_emit_emit_jump_insn/*!emit_jump_insn*/(mtcsEmit,gen_jump (done_label));
   mtcs_emit_emit_barrier/*!emit_barrier*/(mtcsEmit);

   mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
   LABEL_NUSES (label) = 1;
}

/* Handle small memcpy (up to SIZE that is supposed to be small power of 2.
   and get ready for the main memcpy loop by copying iniital DESIRED_ALIGN-ALIGN
   bytes and last SIZE bytes adjusitng DESTPTR/SRCPTR/COUNT in a way we can
   proceed with an loop copying SIZE bytes at once. Do moves in MODE.
   DONE_LABEL is a label after the whole copying sequence. The label is created
   on demand if *DONE_LABEL is NULL.
   MIN_SIZE is minimal size of block copied.  This value gets adjusted for new
   bounds after the initial copies.

   DESTMEM/SRCMEM are memory expressions pointing to the copies block,
   DESTPTR/SRCPTR are pointers to the block. DYNAMIC_CHECK indicate whether
   we will dispatch to a library call for large blocks.

   In pseudocode we do:

   if (COUNT < SIZE)
     {
       Assume that SIZE is 4. Bigger sizes are handled analogously
       if (COUNT & 4)
    {
       copy 4 bytes from SRCPTR to DESTPTR
       copy 4 bytes from SRCPTR + COUNT - 4 to DESTPTR + COUNT - 4
       goto done_label
    }
       if (!COUNT)
    goto done_label;
       copy 1 byte from SRCPTR to DESTPTR
       if (COUNT & 2)
    {
       copy 2 bytes from SRCPTR to DESTPTR
       copy 2 bytes from SRCPTR + COUNT - 2 to DESTPTR + COUNT - 2
    }
     }
   else
     {
       copy at least DESIRED_ALIGN-ALIGN bytes from SRCPTR to DESTPTR
       copy SIZE bytes from SRCPTR + COUNT - SIZE to DESTPTR + COUNT -SIZE

       OLD_DESPTR = DESTPTR;
       Align DESTPTR up to DESIRED_ALIGN
       SRCPTR += DESTPTR - OLD_DESTPTR
       COUNT -= DEST_PTR - OLD_DESTPTR
       if (DYNAMIC_CHECK)
    Round COUNT down to multiple of SIZE
       << optional caller supplied zero size guard is here >>
       << optional caller supplied dynamic check is here >>
       << caller supplied main copy loop is here >>
     }
   done_label:
  */
static void expand_set_or_cpymem_prologue_epilogue_by_misaligned_moves(MtcsPtxBuiltins *self,
                         rtx destmem, rtx srcmem,
                         rtx *destptr, rtx *srcptr,
                         machine_mode mode,
                         rtx value, rtx vec_value,
                         rtx *count,
                         rtx_code_label **done_label,
                         int size,
                         int desired_align,
                         int align,
                         unsigned HOST_WIDE_INT *min_size,
                         bool dynamic_check,
                         bool issetmem)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

   n_debug("expand_set_or_cpymem_prologue_epilogue_by_misaligned_moves 00\n");
   rtx_code_label *loop_label = NULL, *label;
   int n;
   rtx modesize;
   int prolog_size = 0;
   rtx mode_value;
   /* Chose proper value to copy.  */
   if (issetmem && mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,mode))
      mode_value = vec_value;
   else
      mode_value = value;
   gcc_assert (mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode) <= size);

   /* See if block is big or small, handle small blocks.  */
   if (!CONST_INT_P (*count) && *min_size < (unsigned HOST_WIDE_INT)size){
      int size2 = size;
      loop_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
      if (!*done_label)
         *done_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
      mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,
            *count, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,size2), GE, 0, GET_MODE (*count),1, loop_label);
      size2 >>= 1;
      /* Handle sizes > 3.  */
      for (;size2 > 2; size2 >>= 1)
         expand_small_cpymem_or_setmem(self,destmem, srcmem,
                  *destptr, *srcptr,
                  value, vec_value,
                  *count,
                  size2, *done_label, issetmem);
      /* Nothing to copy?  Jump to DONE_LABEL if so */
      mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,
            *count, const0_rtx, EQ, 0, GET_MODE (*count),1, *done_label);
      /* Do a byte copy.  */
      destmem = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,destmem, mtcsMode->modes.M_QImode, *destptr);
      if (issetmem)
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,
               destmem, gen_lowpart (mtcsMode->modes.M_QImode, value));
      else{
         srcmem = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,srcmem, mtcsMode->modes.M_QImode, *srcptr);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,destmem, srcmem);
      }
      /* Handle sizes 2 and 3.  */
      label =ptx_expand_aligntest/*!ix86_expand_aligntest*/(self,*count, 2, false);
      destmem = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,destmem, mtcsMode->modes.M_HImode, *destptr);
      destmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,destmem, *count, 1);
      destmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,destmem, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,-2), 2);
      if (issetmem)
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,destmem, gen_lowpart (HImode, value));
      else {
         srcmem = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,srcmem, mtcsMode->modes.M_HImode, *srcptr);
         srcmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,srcmem, *count, 1);
         srcmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,srcmem, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,-2), 2);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,destmem, srcmem);
      }
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
      LABEL_NUSES (label) = 1;
      mtcs_emit_emit_jump_insn/*!emit_jump_insn*/(mtcsEmit,gen_jump (*done_label));
      mtcs_emit_emit_barrier/*!emit_barrier*/(mtcsEmit);
   }else
      gcc_assert (*min_size >= (unsigned HOST_WIDE_INT)size || UINTVAL (*count) >= (unsigned HOST_WIDE_INT)size);

   /* Start memcpy for COUNT >= SIZE.  */
   if (loop_label){
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,loop_label);
      LABEL_NUSES (loop_label) = 1;
   }

   /* Copy first desired_align bytes.  */
   if (!issetmem)
      srcmem = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,srcmem, mode, *srcptr);
   destmem = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,destmem, mode, *destptr);
   modesize = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode));
   for (n = 0; prolog_size < desired_align - align; n++){
      if (issetmem)
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,destmem, mode_value);
      else{
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,destmem, srcmem);
         srcmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,
               srcmem, modesize, mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode));
      }
      destmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,
            destmem, modesize, mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode));
      prolog_size += mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode);
   }

   /* Copy last SIZE bytes.  */
   destmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,destmem, *count, 1);
   destmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,destmem,
               mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,-size - prolog_size),1);
   if (issetmem)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,destmem, mode_value);
   else{
      srcmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,srcmem, *count, 1);
      srcmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,srcmem,
      mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,-size - prolog_size),1);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,destmem, srcmem);
   }
   for (n = 1; n * mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,mode) < size; n++){
      destmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,destmem, modesize, 1);
      if (issetmem)
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,destmem, mode_value);
      else{
         srcmem = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,srcmem, modesize, 1);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,destmem, srcmem);
      }
   }

   /* Align destination.  */
   if (desired_align > 1 && desired_align > align){
      rtx saveddest = *destptr;

      gcc_assert (desired_align <= size);
      /* Align destptr up, place it to new register.  */
      *destptr = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
            GET_MODE (*destptr), PLUS, *destptr,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,prolog_size),
               NULL_RTX, 1, OPTAB_DIRECT);
      if (REG_P (*destptr) && REG_P (saveddest) && REG_POINTER (saveddest))
         REG_POINTER (*destptr) = 1;
      *destptr = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
            GET_MODE (*destptr), AND, *destptr,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,-desired_align),
            *destptr, 1, OPTAB_DIRECT);
      /* See how many bytes we skipped.  */
      saveddest = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
            GET_MODE (*destptr), MINUS, saveddest,*destptr,NULL_RTX, 1, OPTAB_DIRECT);
      /* Adjust srcptr and count.  */
      if (!issetmem)
         *srcptr = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
               GET_MODE (*srcptr), MINUS, *srcptr,saveddest, *srcptr, 1, OPTAB_DIRECT);
      *count = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
            GET_MODE (*count), PLUS, *count,saveddest, *count, 1, OPTAB_DIRECT);
      /* We copied at most size + prolog_size.  */
      if (*min_size > (unsigned HOST_WIDE_INT)(size + prolog_size))
         *min_size = ROUND_DOWN (*min_size - size, (unsigned HOST_WIDE_INT)size);
      else
         *min_size = 0;

      /* Our loops always round down the block size, but for dispatch to
      library we need precise value.  */
      if (dynamic_check)
         *count = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
               GET_MODE (*count), AND, *count,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,-size), *count, 1, OPTAB_DIRECT);
   }else{
      gcc_assert (prolog_size == 0);
      /* Decrease count, so we won't end up copying last word twice.  */
      if (!CONST_INT_P (*count))
         *count = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
               GET_MODE (*count), PLUS, *count,constm1_rtx, *count, 1, OPTAB_DIRECT);
      else
         *count = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,ROUND_DOWN (UINTVAL (*count) - 1,(unsigned HOST_WIDE_INT)size));
      if (*min_size)
         *min_size = ROUND_DOWN (*min_size - 1, (unsigned HOST_WIDE_INT)size);
   }
}

/* Adjust COUNTER by the VALUE.  */
//原型 ix86_adjust_counter i386-expand.cc
static void ptx_adjust_counter (MtcsPtxBuiltins *self,rtx countreg, HOST_WIDE_INT value)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

   mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,
         mtcs_optabs_gen_add2_insn/*!gen_add2_insn*/(mtcsOptabs,countreg,
               mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,-value)));
}

/* Depending on ISSETMEM, copy enough from SRCMEM to DESTMEM or set enough to
   DESTMEM to align it to DESIRED_ALIGNMENT.  Original alignment is ALIGN.
   Depending on ISSETMEM, either arguments SRCMEM/SRCPTR or VALUE/VEC_VALUE are
   ignored.
   Return value is updated DESTMEM.  */
static rtx expand_set_or_cpymem_prologue (MtcsPtxBuiltins *self,rtx destmem, rtx srcmem,
              rtx destptr, rtx srcptr, rtx value,
              rtx vec_value, rtx count, int align,
              int desired_alignment, bool issetmem)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   int i;
   for (i = 1; i < desired_alignment; i <<= 1){
      if (align <= i){
         rtx_code_label *label = ptx_expand_aligntest/*!ix86_expand_aligntest*/(self,destptr, i, false);
         if (issetmem){
            if (vec_value && i > mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,GET_MODE (value)))
               destmem = emit_memset(self,destmem, destptr, vec_value, i);
            else
               destmem = emit_memset(self,destmem, destptr, value, i);
         }else
            destmem = emit_memmov(self,destmem, &srcmem, destptr, srcptr, i);
         ptx_adjust_counter/*!ix86_adjust_counter*/(self,count, i);
         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
         LABEL_NUSES (label) = 1;
         mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,destmem, i * 2 * BITS_PER_UNIT);
      }
   }
   return destmem;
}

/* This function emits moves to fill SIZE_TO_MOVE bytes starting from DESTMEM
   with value PROMOTED_VAL.
   SRC is passed by pointer to be updated on return.
   Return value is updated DST.  */
//原型 emit_memset i386-expand.cc
static rtx emit_memset (MtcsPtxBuiltins *self,rtx destmem, rtx destptr, rtx promoted_val,
        HOST_WIDE_INT size_to_move)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);

   rtx dst = destmem;
   enum insn_code code;
   machine_mode move_mode;
   int piece_size, i;
   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
   /* Find the widest mode in which we could perform moves.
   Start with the biggest power of 2 less than SIZE_TO_MOVE and half
   it until move of such size is supported.  */
   move_mode = GET_MODE (promoted_val);
   if (move_mode == VOIDmode)
      move_mode =mtcsMode->modes.M_QImode;
   if (size_to_move < mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,move_mode)){
      unsigned int move_bits = size_to_move * BITS_PER_UNIT;
      move_mode = mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,move_bits, 0).require ();
      promoted_val = gen_lowpart (move_mode, promoted_val);
   }
   piece_size = mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,move_mode);
   code = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,mov_optab, move_mode);
   gcc_assert (code != CODE_FOR_nothing && promoted_val != NULL_RTX);
   dst = mtcs_rtl_adjust_automodify_address_nv/*!adjust_automodify_address_nv*/(mtcsRTL,dst, move_mode, destptr, 0);
   /* Emit moves.  We'll need SIZE_TO_MOVE/PIECE_SIZES moves.  */
   gcc_assert (size_to_move % piece_size == 0);

   for (i = 0; i < size_to_move; i += piece_size){
      if (piece_size <= mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,word_mode)){
         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,ptx_gen_strset(destptr, dst, promoted_val));
         dst = mtcs_rtl_adjust_automodify_address_nv/*!adjust_automodify_address_nv*/(mtcsRTL,
               dst, move_mode, destptr,piece_size);
         continue;
      }

      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,MTCS_GEN_FCN/*!GEN_FCN*/(code) (dst, promoted_val));
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,
            destptr,mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, copy_rtx (destptr), piece_size));
      dst = mtcs_rtl_adjust_automodify_address_nv/*!adjust_automodify_address_nv*/(mtcsRTL,
            dst, move_mode, destptr,piece_size);
   }
   /* Update DST rtx.  */
   return dst;
}

/* This function emits moves to copy SIZE_TO_MOVE bytes from SRCMEM to
   DESTMEM.
   SRC is passed by pointer to be updated on return.
   Return value is updated DST.  */
//原型 emit_memmov i386-expand.cc
static rtx emit_memmov (MtcsPtxBuiltins *self,rtx destmem, rtx *srcmem, rtx destptr, rtx srcptr,
        HOST_WIDE_INT size_to_move)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   rtx dst = destmem, src = *srcmem, tempreg;
   enum insn_code code;
   machine_mode move_mode;
   int piece_size, i;

   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);

   /* Find the widest mode in which we could perform moves.
   Start with the biggest power of 2 less than SIZE_TO_MOVE and half
   it until move of such size is supported.  */
   piece_size = 1 << floor_log2 (size_to_move);
   while (!mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,
         piece_size * BITS_PER_UNIT, 0).exists (&move_mode)
   || (code = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,mov_optab, move_mode)) == CODE_FOR_nothing){
      gcc_assert (piece_size > 1);
      piece_size >>= 1;
   }

   /* Find the corresponding vector mode with the same size as MOVE_MODE.
   MOVE_MODE is an integer mode at the moment (SI, DI, TI, etc.).  */
   if (mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,move_mode) >
         mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,word_mode)){
      int nunits = mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,move_mode) /
            mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,word_mode);
      if (!mode_for_vector (word_mode, nunits).exists (&move_mode)
               || (code = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,mov_optab, move_mode))
               == CODE_FOR_nothing){
         move_mode = word_mode;
         piece_size = mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,move_mode);
         code = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,mov_optab, move_mode);
      }
   }
   gcc_assert (code != CODE_FOR_nothing);

   dst = mtcs_rtl_adjust_automodify_address_nv/*!adjust_automodify_address_nv*/(mtcsRTL,dst, move_mode, destptr, 0);
   src = mtcs_rtl_adjust_automodify_address_nv/*!adjust_automodify_address_nv*/(mtcsRTL,src, move_mode, srcptr, 0);

   /* Emit moves.  We'll need SIZE_TO_MOVE/PIECE_SIZES moves.  */
   gcc_assert (size_to_move % piece_size == 0);

   for (i = 0; i < size_to_move; i += piece_size){
      /* We move from memory to memory, so we'll need to do it via
      a temporary register.  */
      tempreg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,move_mode);
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,MTCS_GEN_FCN/*!GEN_FCN*/(code) (tempreg, src));
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,MTCS_GEN_FCN/*!GEN_FCN*/(code) (dst, tempreg));

      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,destptr,
               mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, copy_rtx (destptr), piece_size));
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,srcptr,
               mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, copy_rtx (srcptr), piece_size));

      dst = mtcs_rtl_adjust_automodify_address_nv/*!adjust_automodify_address_nv*/(mtcsRTL,
            dst, move_mode, destptr,piece_size);
      src = mtcs_rtl_adjust_automodify_address_nv/*!adjust_automodify_address_nv*/(mtcsRTL,
            src, move_mode, srcptr,piece_size);
   }

   /* Update DST and SRC rtx.  */
   *srcmem = src;
   return dst;
}

/* This function is like the previous one, except here we know how many bytes
   need to be copied.  That allows us to update alignment not only of DST, which
   is returned, but also of SRC, which is passed as a pointer for that
   reason.  */
//原型 expand_set_or_cpymem_constant_prologue i386-expand.cc
static rtx expand_set_or_cpymem_constant_prologue (MtcsPtxBuiltins *self,
                  rtx dst, rtx *srcp, rtx destreg,
                  rtx srcreg, rtx value, rtx vec_value,
                  int desired_align, int align_bytes,
                  bool issetmem)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsAlign  *mtcsAlign=mtcs_target_get_align(mtcsTarget);

   rtx src = NULL;
   rtx orig_dst = dst;
   rtx orig_src = NULL;
   int piece_size = 1;
   int copied_bytes = 0;

   if (!issetmem){
      gcc_assert (srcp != NULL);
      src = *srcp;
      orig_src = src;
   }

   for (piece_size = 1;
            piece_size <= desired_align && copied_bytes < align_bytes;
            piece_size <<= 1){
      if (align_bytes & piece_size){
         if (issetmem){
            if (vec_value && piece_size > mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,GET_MODE (value)))
               dst = emit_memset(self,dst, destreg, vec_value, piece_size);
            else
               dst = emit_memset(self,dst, destreg, value, piece_size);
         }else
            dst = emit_memmov(self,dst, &src, destreg, srcreg, piece_size);
         copied_bytes += piece_size;
      }
   }
   if (mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,dst) < (unsigned int) desired_align * BITS_PER_UNIT)
      mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,dst, desired_align * BITS_PER_UNIT);
   if (mtcs_rtl_is_mem_size_known_p/*!MEM_SIZE_KNOWN_P*/(mtcsRTL,orig_dst))
      mtcs_rtl_set_mem_size/*!set_mem_size*/(mtcsRTL,
            dst, mtcs_rtl_get_mem_size/*!MEM_SIZE*/(mtcsRTL,orig_dst) - align_bytes);

   if (!issetmem){
      int src_align_bytes = mtcs_align_get_mem_align_offset/*!get_mem_align_offset*/(mtcsAlign,
            src, desired_align * BITS_PER_UNIT);
      if (src_align_bytes >= 0)
         src_align_bytes = desired_align - src_align_bytes;
      if (src_align_bytes >= 0){
         unsigned int src_align;
         for (src_align = desired_align; src_align >= 2; src_align >>= 1){
            if ((src_align_bytes & (src_align - 1)) == (align_bytes & (src_align - 1)))
               break;
         }
         if (src_align > (unsigned int) desired_align)
            src_align = desired_align;
         if (mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,src) < src_align * BITS_PER_UNIT)
            mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,src, src_align * BITS_PER_UNIT);
      }
      if (mtcs_rtl_is_mem_size_known_p/*!MEM_SIZE_KNOWN_P*/(mtcsRTL,orig_src))
         mtcs_rtl_set_mem_size/*!set_mem_size*/(mtcsRTL,
               src, mtcs_rtl_get_mem_size/*!MEM_SIZE*/(mtcsRTL,orig_src) - align_bytes);
      *srcp = src;
   }

   return dst;
}

/* Zero extend possibly SImode EXP to Pmode register.  */
//原型 ix86_zero_extend_to_Pmode i386-protos.h i385.cc
static rtx ptx_zero_extend_to_Pmode (MtcsPtxBuiltins *self,rtx exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

   machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
   return mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,pMode,
            mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,pMode, exp, 1));
}

/* Divide COUNTREG by SCALE.  */
//原型 scale_counter i386-expand.cc
static rtx scale_counter (MtcsPtxBuiltins *self,rtx countreg, int scale)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

   rtx sc;

   if (scale == 1)
      return countreg;
   if (CONST_INT_P (countreg))
      return mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,INTVAL (countreg) / scale);
   gcc_assert (REG_P (countreg));

   sc = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,GET_MODE (countreg), LSHIFTRT, countreg,
                  mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,exact_log2 (scale)),NULL, 1, OPTAB_DIRECT);
   return sc;
}

/**
 * 匹配 mtcs_ptx.md中的define expand指令 cpymem
 */
nboolean mtcs_ptx_builtins_expand_set_or_cpymem (MtcsPtxBuiltins *self,
            rtx dst, rtx src, rtx count_exp, rtx val_exp,
            rtx align_exp, rtx expected_align_exp,
            rtx expected_size_exp, rtx min_size_exp,
            rtx max_size_exp, rtx probable_max_size_exp,
            bool issetmem)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsAlign  *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   rtx destreg;
   rtx srcreg = NULL;
   rtx_code_label *label = NULL;
   rtx tmp;
   rtx_code_label *jump_around_label = NULL;
   HOST_WIDE_INT align = 1;
   unsigned HOST_WIDE_INT count = 0;
   HOST_WIDE_INT expected_size = -1;
   int size_needed = 0, epilogue_size_needed;
   int desired_align = 0, align_bytes = 0;
   enum ptx_stringop_alg alg;
   rtx promoted_val = NULL;
   rtx vec_promoted_val = NULL;
   bool force_loopy_epilogue = false;
   int dynamic_check;
   bool need_zero_guard = false;
   bool noalign;
   machine_mode move_mode = VOIDmode;
   machine_mode wider_mode;
   int unroll_factor = 1;
   /* TODO: Once value ranges are available, fill in proper data.  */
   unsigned HOST_WIDE_INT min_size = 0;
   unsigned HOST_WIDE_INT max_size = -1;
   unsigned HOST_WIDE_INT probable_max_size = -1;
   bool misaligned_prologue_used = false;
   bool have_as;

   if (CONST_INT_P (align_exp))
      align = INTVAL (align_exp);
   /* i386 can do misaligned access on reasonably increased cost.  */
   if (CONST_INT_P (expected_align_exp) && INTVAL (expected_align_exp) > align)
      align = INTVAL (expected_align_exp);
   /* ALIGN is the minimum of destination and source alignment, but we care here
   just about destination alignment.  */
   else if (!issetmem
   && mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,dst) > (unsigned HOST_WIDE_INT) align * BITS_PER_UNIT)
      align = mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,dst) / BITS_PER_UNIT;

   if (CONST_INT_P (count_exp)){
      min_size = max_size = probable_max_size = count = expected_size  = INTVAL (count_exp);
      /* When COUNT is 0, there is nothing to do.  */
      n_debug("ptx_memcpy 00 INTVAL (count_exp):%d count:%d\n",INTVAL (count_exp),count);
      if (!count)
         return true;
   }else{
      if (min_size_exp)
         min_size = INTVAL (min_size_exp);
      if (max_size_exp)
         max_size = INTVAL (max_size_exp);
      if (probable_max_size_exp)
         probable_max_size = INTVAL (probable_max_size_exp);
      if (CONST_INT_P (expected_size_exp))
         expected_size = INTVAL (expected_size_exp);
      n_debug("ptx_memcpy 11 min:%d max:%d probal:%llu expected:%d\n",
            min_size,max_size,probable_max_size,expected_size);
      printrtl(self);
   }

   /* Make sure we don't need to care about overflow later on.  */
   if (count > (HOST_WIDE_INT_1U << 30))
      return false;

   have_as = !ADDR_SPACE_GENERIC_P (mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,dst));
   if (!issetmem)
      have_as |= !ADDR_SPACE_GENERIC_P (mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,src));

   /* Step 0: Decide on preferred algorithm, desired alignment and
   size of chunks to be copied by main loop.  */
   alg = decide_alg (count, expected_size, min_size, probable_max_size,
         issetmem, issetmem && val_exp == const0_rtx, have_as, &dynamic_check, &noalign, false);

   n_debug( "ptx_memcpy 22 alg:%d have_as:%d\n",alg,have_as);

   if (alg ==ptx_libcall)
      return false;
   gcc_assert (alg !=ptx_no_stringop);

   /* For now vector-version of memset is generated only for memory zeroing, as
   creating of promoted vector value is very cheap in this case.  */
   if (issetmem && alg == ptx_vector_loop && val_exp != const0_rtx)
      alg = ptx_unrolled_loop;

   if (!count)
      count_exp = mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,GET_MODE (count_exp), count_exp);

   destreg = ptx_copy_addr_to_reg/*!ix86_copy_addr_to_reg*/(self,XEXP (dst, 0));
   if (!issetmem)
      srcreg = ptx_copy_addr_to_reg/*!ix86_copy_addr_to_reg*/(self,XEXP (src, 0));

   unroll_factor = 1;
   move_mode = word_mode;
   switch (alg){
      case ptx_libcall:
      case ptx_no_stringop:
      case ptx_last_alg:
         gcc_unreachable ();
      case ptx_loop_1_byte:
         need_zero_guard = true;
         move_mode = mtcsMode->modes.M_QImode;
         break;
      case ptx_loop:
         need_zero_guard = true;
         break;
      case ptx_unrolled_loop:
         need_zero_guard = true;
         unroll_factor = (TARGET_64BIT ? 4 : 2);
         break;
      case ptx_vector_loop:
         need_zero_guard = true;
         unroll_factor = 4;
         /* Find the widest supported mode.  */
         move_mode = word_mode;
         while (mtcs_mode_get_wider/*!GET_MODE_WIDER_MODE*/(mtcsMode,move_mode).exists (&wider_mode)
         && mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,mov_optab, wider_mode) != CODE_FOR_nothing)
            move_mode = wider_mode;
         /*!
         if (TARGET_AVX256_SPLIT_REGS && GET_MODE_BITSIZE (move_mode) > 128)
         move_mode = TImode;
         if (TARGET_AVX512_SPLIT_REGS && GET_MODE_BITSIZE (move_mode) > 256)
         move_mode = OImode;
         */

         /* Find the corresponding vector mode with the same size as MOVE_MODE.
         MOVE_MODE is an integer mode at the moment (SI, DI, TI, etc.).  */
         if (mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,move_mode) >
                  mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,word_mode)){
            int nunits = mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,move_mode) /
                              mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,word_mode);
            if (!mtcs_mode_mode_for_vector/*!mode_for_vector*/(mtcsMode,word_mode, nunits).exists (&move_mode)
                     || mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,mov_optab, move_mode) == CODE_FOR_nothing)
               move_mode = word_mode;
         }
         gcc_assert (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,mov_optab, move_mode) != CODE_FOR_nothing);
         break;
   }

   size_needed = mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,move_mode) * unroll_factor;
   epilogue_size_needed = size_needed;

   /* If we are going to call any library calls conditionally, make sure any
   pending stack adjustment happen before the first conditional branch,
   otherwise they will be emitted before the library call only and won't
   happen from the other branches.  */
   if (dynamic_check != -1)
      mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);

   desired_align = decide_alignment(self,align, alg, expected_size, move_mode);
   if (!TARGET_ALIGN_STRINGOPS || noalign)
      align = desired_align;
   n_debug( "ptx_memcpy 33 size_needed:%d dynamic_check:%d desired_align:%d align:%d\n",
         size_needed,dynamic_check,desired_align,align);

   /* Step 1: Prologue guard.  */

   /* Alignment code needs count to be in register.  */
   if (CONST_INT_P (count_exp) && desired_align > align) {
      n_debug( "ptx_memcpy 44 CONST_INT_P (count_exp) && desired_align > align\n");
      if (INTVAL (count_exp) > desired_align && INTVAL (count_exp) > size_needed){
         align_bytes = mtcs_align_get_mem_align_offset/*!get_mem_align_offset*/(mtcsAlign,
               dst, desired_align * BITS_PER_UNIT);
         if (align_bytes <= 0)
            align_bytes = 0;
         else
            align_bytes = desired_align - align_bytes;
      }
      if (align_bytes == 0)
         count_exp = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,counter_mode(self,count_exp), count_exp);
   }
   gcc_assert (desired_align >= 1 && align >= 1);

   /* Misaligned move sequences handle both prologue and epilogue at once.
   Default code generation results in a smaller code for large alignments
   and also avoids redundant job when sizes are known precisely.  */
   misaligned_prologue_used = (MAX (desired_align, epilogue_size_needed) <= 32
               && desired_align <= epilogue_size_needed
               && ((desired_align > align && !align_bytes)
               || (!count && epilogue_size_needed > 1)));
   /* Do the cheap promotion to allow better CSE across the
   main loop and epilogue (ie one load of the big constant in the
   front of all code.
   For now the misaligned move sequences do not have fast path
   without broadcasting.  */
   if (issetmem && ((CONST_INT_P (val_exp) || misaligned_prologue_used))){
      if (alg == ptx_vector_loop){
         gcc_assert (val_exp == const0_rtx);
         vec_promoted_val = promote_duplicated_reg(self,move_mode, val_exp);
                  promoted_val = promote_duplicated_reg_to_size(self,val_exp,
                  mtcs_mode_get_size/*GET_MODE_SIZE*/(mtcsMode,word_mode),
                  desired_align, align);
      }else{
         promoted_val = promote_duplicated_reg_to_size(self,val_exp, size_needed,desired_align, align);
      }
   }
   /* Misaligned move sequences handles both prologues and epilogues at once.
   Default code generation results in smaller code for large alignments and
   also avoids redundant job when sizes are known precisely.  */
   if (misaligned_prologue_used){
      /* Misaligned move prologue handled small blocks by itself.  */
      expand_set_or_cpymem_prologue_epilogue_by_misaligned_moves(self,dst, src, &destreg, &srcreg,
                  move_mode, promoted_val, vec_promoted_val,
                  &count_exp,
                  &jump_around_label,
                  desired_align < align
                  ? MAX (desired_align, epilogue_size_needed) : epilogue_size_needed,
                  desired_align, align, &min_size, dynamic_check, issetmem);
      if (!issetmem)
         src = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,src, mtcsMode->modes.M_BLKmode, srcreg);
      dst = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,dst, mtcsMode->modes.M_BLKmode, destreg);
      mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,dst, desired_align * BITS_PER_UNIT);
      epilogue_size_needed = 0;
      if (need_zero_guard && min_size < (unsigned HOST_WIDE_INT) size_needed){
         /* It is possible that we copied enough so the main loop will not
         execute.  */
         gcc_assert (size_needed > 1);
         if (jump_around_label == NULL_RTX)
            jump_around_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,count_exp,
                  mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,size_needed),
                  LTU, 0, counter_mode(self,count_exp), 1, jump_around_label);
         if (expected_size == -1 || expected_size < (desired_align - align) / 2 + size_needed)
            predict_jump(self,REG_BR_PROB_BASE * 20 / 100);
         else
            predict_jump(self,REG_BR_PROB_BASE * 60 / 100);
      }
   }
   /* Ensure that alignment prologue won't copy past end of block.  */
   else if (size_needed > 1 || (desired_align > 1 && desired_align > align)){
      epilogue_size_needed = MAX (size_needed - 1, desired_align - align);
      /* Epilogue always copies COUNT_EXP & EPILOGUE_SIZE_NEEDED bytes.
      Make sure it is power of 2.  */
      epilogue_size_needed = 1 << (floor_log2 (epilogue_size_needed) + 1);
      n_debug( "ptx_memcpy 55 epilogue_size_needed:%d\n",epilogue_size_needed);

      /* To improve performance of small blocks, we jump around the VAL
      promoting mode.  This mean that if the promoted VAL is not constant,
      we might not use it in the epilogue and have to use byte
      loop variant.  */
      if (issetmem && epilogue_size_needed > 2 && !promoted_val)
         force_loopy_epilogue = true;
      n_debug( "ptx_memcpy 66 force_loopy_epilogue:%d\n",force_loopy_epilogue);

      if ((count && count < (unsigned HOST_WIDE_INT) epilogue_size_needed)
      || max_size < (unsigned HOST_WIDE_INT) epilogue_size_needed){
         /* If main algorithm works on QImode, no epilogue is needed.
         For small sizes just don't align anything.  */
         n_debug( "ptx_memcpy 77 size_needed:%d desired_align:%d align:%d\n",size_needed,desired_align,align);

         if (size_needed == 1)
            desired_align = align;
         else
            goto epilogue;
      }else if (!count  && min_size < (unsigned HOST_WIDE_INT) epilogue_size_needed){
         n_debug( "ptx_memcpy 88 epilogue_size_needed:%d\n",epilogue_size_needed);

         label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,count_exp,
                  mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,epilogue_size_needed),
                  LTU, 0, counter_mode(self,count_exp), 1, label);
         if (expected_size == -1 || expected_size < epilogue_size_needed)
            predict_jump(self,REG_BR_PROB_BASE * 60 / 100);
         else
            predict_jump(self,REG_BR_PROB_BASE * 20 / 100);
      }
   }

   /* Emit code to decide on runtime whether library call or inline should be
   used.  */
   if (dynamic_check != -1){
      if (!issetmem && CONST_INT_P (count_exp)){
         if (UINTVAL (count_exp) >= (unsigned HOST_WIDE_INT)dynamic_check){
            n_debug( "ptx_memcpy 99 dynamic_check:%d\n",dynamic_check);
            mtcs_expr_emit_block_copy_via_libcall/*!emit_block_copy_via_libcall*/(mtcsExpr,dst, src, count_exp);
            count_exp = const0_rtx;
            goto epilogue;
         }
      }else{
         rtx_code_label *hot_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         n_debug( "ptx_memcpy 99 dynamic_check:%d jump_around_label:%p\n",dynamic_check,jump_around_label);
         if (jump_around_label == NULL_RTX)
            jump_around_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,
                  count_exp, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,dynamic_check - 1),
                  LEU, 0, counter_mode(self,count_exp),1, hot_label);
         predict_jump(self,REG_BR_PROB_BASE * 90 / 100);
         if (issetmem)
            mtcs_expr_set_storage_via_libcall/*!set_storage_via_libcall*/(mtcsExpr,dst, count_exp, val_exp);
         else
            mtcs_expr_emit_block_copy_via_libcall/*!emit_block_copy_via_libcall*/(mtcsExpr,dst, src, count_exp);
         mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,jump_around_label);
         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,hot_label);
      }
   }

   /* Step 2: Alignment prologue.  */
   /* Do the expensive promotion once we branched off the small blocks.  */
   if (issetmem && !promoted_val)
      promoted_val = promote_duplicated_reg_to_size(self,val_exp, size_needed, desired_align, align);

   if (desired_align > align && !misaligned_prologue_used){
      if (align_bytes == 0){
         n_debug( "ptx_memcpy 100 desired_align:%d align:%d\n",desired_align,align);

         /* Except for the first move in prologue, we no longer know
         constant offset in aliasing info.  It don't seems to worth
         the pain to maintain it for the first move, so throw away
         the info early.  */
         dst = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,dst, mtcsMode->modes.M_BLKmode, destreg);
         if (!issetmem)
            src = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,src, mtcsMode->modes.M_BLKmode, srcreg);
         dst = expand_set_or_cpymem_prologue(self,dst, src, destreg, srcreg,
                  promoted_val, vec_promoted_val,
                  count_exp, align, desired_align,
                  issetmem);
         /* At most desired_align - align bytes are copied.  */
         if (min_size < (unsigned)(desired_align - align))
            min_size = 0;
         else
            min_size -= desired_align - align;
      }else{
         n_debug( "ptx_memcpy 101 desired_align:%d align:%d\n",desired_align,align);

         /* If we know how many bytes need to be stored before dst is
         sufficiently aligned, maintain aliasing info accurately.  */
         dst = expand_set_or_cpymem_constant_prologue(self,dst, &src, destreg,
                                 srcreg,
                                 promoted_val,
                                 vec_promoted_val,
                                 desired_align,
                                 align_bytes,
                                 issetmem);

         count_exp = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,counter_mode(self,count_exp),
         count_exp, -align_bytes);
         count -= align_bytes;
         min_size -= align_bytes;
         max_size -= align_bytes;
      }
      if (need_zero_guard
      && min_size < (unsigned HOST_WIDE_INT) size_needed
      && (count < (unsigned HOST_WIDE_INT) size_needed
      || (align_bytes == 0 && count < ((unsigned HOST_WIDE_INT) size_needed + desired_align - align)))){
         /* It is possible that we copied enough so the main loop will not
         execute.  */
         n_debug( "ptx_memcpy 102 desired_align:%d align:%d\n",desired_align,align);

         gcc_assert (size_needed > 1);
         if (label == NULL_RTX)
            label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,count_exp,
                     mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,size_needed),
                     LTU, 0, counter_mode(self,count_exp), 1, label);
         if (expected_size == -1 || expected_size < (desired_align - align) / 2 + size_needed)
            predict_jump(self,REG_BR_PROB_BASE * 20 / 100);
         else
            predict_jump(self,REG_BR_PROB_BASE * 60 / 100);
      }
   }
   if (label && size_needed == 1){
      n_debug( "ptx_memcpy 103 desired_align:%d align:%d\n",desired_align,align);

      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
      LABEL_NUSES (label) = 1;
      label = NULL;
      epilogue_size_needed = 1;
      if (issetmem)
         promoted_val = val_exp;
   } else if (label == NULL_RTX && !misaligned_prologue_used)
      epilogue_size_needed = size_needed;
   n_debug( "ptx_memcpy 104 desired_align:%d align:%d epilogue_size_needed:%d %d\n",desired_align,align,epilogue_size_needed,size_needed);

   /* Step 3: Main loop.  */
   switch (alg)   {
      case ptx_libcall:
      case ptx_no_stringop:
      case ptx_last_alg:
         gcc_unreachable ();
      case ptx_loop_1_byte:
      case ptx_loop:
      case ptx_unrolled_loop:
         expand_set_or_cpymem_via_loop(self,dst, src, destreg, srcreg, promoted_val,
                  count_exp, move_mode, unroll_factor, expected_size, issetmem);
         break;
      case ptx_vector_loop:
         expand_set_or_cpymem_via_loop(self,dst, src, destreg, srcreg,
                  vec_promoted_val, count_exp, move_mode, unroll_factor, expected_size, issetmem);
         break;
   }
   /* Adjust properly the offset of src and dest memory for aliasing.  */
   if (CONST_INT_P (count_exp)){
      if (!issetmem)
         src = mtcs_rtl_adjust_automodify_address_nv/*!adjust_automodify_address_nv*/(mtcsRTL,
               src, mtcsMode->modes.M_BLKmode, srcreg,(count / size_needed) * size_needed);
      dst = mtcs_rtl_adjust_automodify_address_nv/*!adjust_automodify_address_nv*/(mtcsRTL,
            dst, mtcsMode->modes.M_BLKmode, destreg,(count / size_needed) * size_needed);
   }else{
      if (!issetmem)
         src = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,src, mtcsMode->modes.M_BLKmode, srcreg);
      dst = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,dst, mtcsMode->modes.M_BLKmode, destreg);
   }

   /* Step 4: Epilogue to copy the remaining bytes.  */
epilogue:
   if (label){
      /* When the main loop is done, COUNT_EXP might hold original count,
      while we want to copy only COUNT_EXP & SIZE_NEEDED bytes.
      Epilogue code will actually copy COUNT_EXP & EPILOGUE_SIZE_NEEDED
      bytes. Compensate if needed.  */
      if (size_needed < epilogue_size_needed){
         tmp = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
               counter_mode(self,count_exp), AND, count_exp,
               mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,size_needed - 1), count_exp, 1,OPTAB_DIRECT);
         if (tmp != count_exp)
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,count_exp, tmp);
      }
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
      LABEL_NUSES (label) = 1;
   }

   if (count_exp != const0_rtx && epilogue_size_needed > 1){
      if (force_loopy_epilogue)
         expand_setmem_epilogue_via_loop(self,dst, destreg, val_exp, count_exp,epilogue_size_needed);
      else {
         if (issetmem)
            expand_setmem_epilogue(self,dst, destreg, promoted_val,vec_promoted_val, count_exp,epilogue_size_needed);
         else
            expand_cpymem_epilogue(self,dst, src, destreg, srcreg, count_exp,epilogue_size_needed);
      }
   }

   if (jump_around_label)
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,jump_around_label);
   return true;
}

MtcsPtxBuiltins *mtcs_ptx_builtins_new(MtcsMode *mtcsMode)
{
    MtcsPtxBuiltins *self = n_slice_alloc0 (sizeof(MtcsPtxBuiltins));
    mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
    mtcs_builtins_init((MtcsBuiltins *)self);
    mtcsPtxBuiltinsInit(self);
    return self;
}

//TARGET_PARTIAL_REG_STALL = 0
//TARGET_MISALIGNED_MOVE_STRING_PRO_EPILOGUES =1
