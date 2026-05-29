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


static void mtcsPtxBuiltinsInit(MtcsPtxBuiltins *self)
{
   MtcsBuiltins *mtcsBuiltins=(MtcsBuiltins *)self;
   mtcsBuiltins->replace_call=replaceCall_cb;
   mtcsBuiltins->convert_call=convertCall_cb;
   mtcsBuiltins->expand_builtin_fn=expandBuiltinFn_cb;
   mtcsBuiltins->expand_internal_fn=expandInternalFn_cb;
   mtcsBuiltins->support_builtin_fn=supportBuiltinFn;

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

  rtx src = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,CALL_EXPR_ARG (exp, 0),
             NULL_RTX, mode, EXPAND_NORMAL);
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
  rtx pat = mtcs_ptx_emit_gen_shuffle/*!nvptx_gen_shuffle*/(mtcsPtxEmit,tmp/*!target*/, src, idx,
                   (ptx_shuffle_kind) INTVAL (op));
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

  fprintf(stderr,"-----nvptx.cc -----113--static rtx nvptx_expand_brev (tree exp, rtx target, machine_mode mode, int ignore)\n");
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

  fprintf(stderr,"-----nvptx.cc -----114--static rtx nvptx_expand_shared_addr (tree exp, rtx target,machine_mode ARG_UNUSED (mode), int ignore,int vector))\n");
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

  fprintf(stderr,"-----nvptx.cc -----115-- nvptx_expand_cmp_swap \n");
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

  fprintf(stderr,"-----nvptx.cc -----116--static rtx nvptx_expand_bar_red \n");

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
  emit_insn (gen_rtx_SET (pred, gen_rtx_NE (mtcsMode->modes.M_BImode, redop,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,0))));
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

//实现父类声明的方法
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

static inline rtx_insn * insnify (MtcsPtxBuiltins *self,rtx x)
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
  //return gen_rtx_SET (operand0,
   //gen_rtx_UNSPEC_VOLATILE ((machine_mode)PTX_SImode, gen_rtvec (1, operand1), 14));
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
   fprintf(stderr,"mtcsptxbuiltins.c expand_LocalSharedVar\n");
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

MtcsPtxBuiltins *mtcs_ptx_builtins_new(MtcsMode *mtcsMode)
{
    MtcsPtxBuiltins *self = n_slice_alloc0 (sizeof(MtcsPtxBuiltins));
    mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
    mtcs_builtins_init((MtcsBuiltins *)self);
    mtcsPtxBuiltinsInit(self);
    return self;
}
