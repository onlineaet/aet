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
#include "gomp-constants.h"
#include "omp-general.h"

#include "aet/aetprinttree.h"
#include "../mtcstarget.h"
#include "mtcsptxemit.h"
#include "ptx-common.h"
#include "mtcsptxpreds.h"
#include "mtcsptxfunc.h"
#include "gen/ptx-insn-modes.h"
#include "gen/ptx-insn-flags.h"
#include "../mtcsprintrtl.h"


//原型 #define BRANCH_COST(speed_p, predictable_p) 1 分支消耗的指令数
static int brachCost_cb(MtcsEmit *mtcsEmit,bool speed_p ,bool predictable_p);

static void mtcsPtxEmitInit(MtcsPtxEmit *self)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsEmit *mtcsEmit=(MtcsEmit *)self;
    mtcs_ptx_emit_set_target(self,(void *)mtcsMode->target);
    //原型 #define BRANCH_COST(speed_p, predictable_p) 1 分支消耗的指令数
    mtcsEmit->brach_cost=brachCost_cb;
}

//原型 #define BRANCH_COST(speed_p, predictable_p) 1 分支消耗的指令数
static int brachCost_cb(MtcsEmit *mtcsEmit,bool speed_p ,bool predictable_p)
{
    return PTX_BRANCH_COST(speed_p,predictable_p);
}



/* Emit a comparison COMPARE, and return the new test to be used in the
   jump.  */
//原型 nvptx_expand_compare nvptx.h nvptx.cc
rtx mtcs_ptx_emit_expand_compare (MtcsPtxEmit *self,rtx compare)
{
    n_debug("mtcsptxemit.c -----nvptx.cc -----40-- rtx nvptx_expand_compare (rtx compare)\n");
    MtcsEmit *mtcsEmit=(MtcsEmit *)self;
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    rtx pred = mtcs_emit_gen_reg_rtx (mtcsEmit,mtcsMode->modes.M_BImode);
    rtx cmp = gen_rtx_fmt_ee (GET_CODE (compare), mtcsMode->modes.M_BImode,XEXP (compare, 0), XEXP (compare, 1));
    mtcs_emit_emit_insn (mtcsEmit,gen_rtx_SET (pred, cmp));
    return gen_rtx_NE (mtcsMode->modes.M_BImode, pred, const0_rtx);
}

/* Emit forking instructions for MASK.  */

static void nvptx_emit_forking (MtcsPtxEmit *self,unsigned mask, bool is_call)
{
  MtcsEmit *mtcsEmit=(MtcsEmit *)self;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  mask &= (GOMP_DIM_MASK (GOMP_DIM_WORKER)| GOMP_DIM_MASK (GOMP_DIM_VECTOR));
  n_debug("mtcsptxemit.c -----nvptx.cc -----6-- nvptx_emit_forking (unsigned mask, bool is_call) mask:%d\n",mask);

  if (mask){
      rtx op = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,mask | (is_call << GOMP_DIM_MAX));
      /* Emit fork at all levels.  This helps form SESE regions, as
     it creates a block with a single successor before entering a
     partitooned region.  That is a good candidate for the end of
     an SESE region.  */
      mtcs_emit_emit_insn (mtcsEmit,ptx_gen_nvptx_fork (op));
      mtcs_emit_emit_insn (mtcsEmit,ptx_gen_nvptx_forked (op));
  }
}

/* Emit joining instructions for MASK.  */
//原型 nvptx_emit_joining nvptx.cc
static void nvptx_emit_joining (MtcsPtxEmit *self,unsigned mask, bool is_call)
{
  MtcsEmit *mtcsEmit=(MtcsEmit *)self;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  mask &= (GOMP_DIM_MASK (GOMP_DIM_WORKER) | GOMP_DIM_MASK (GOMP_DIM_VECTOR));
  n_debug("mtcsptxemit.c -----nvptx.cc -----7-- nvptx_emit_joining (unsigned mask, bool is_call) mask:%d\n",mask);

  if (mask){
     rtx op =mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,mask | (is_call << GOMP_DIM_MAX));
      /* Emit joining for all non-call pars to ensure there's a single
     predecessor for the block the join insn ends up in.  This is
     needed for skipping entire loops.  */
     mtcs_emit_emit_insn (mtcsEmit,ptx_gen_nvptx_joining (op));
     mtcs_emit_emit_insn (mtcsEmit,ptx_gen_nvptx_join (op));
  }
}

/* Emit the sequence for a call to ADDRESS, setting RETVAL.  Keep
   track of whether calls involving static chains or varargs were seen
   in the current function.
   For libcalls, maintain a hash table of decls we have seen, and
   record a function decl for later when encountering a new one.  */
//原型 nvptx_expand_call nvptx.h nvptx.cc
void mtcs_ptx_emit_expand_call (MtcsPtxEmit *self,rtx retval, rtx address)
{
  n_debug("mtcsptxemit.c -----nvptx.cc -----39-- void nvptx_expand_call (rtx retval, rtx address)\n");
  MtcsEmit *mtcsEmit=(MtcsEmit *)self;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
 // struct  ptx_machine_function *machine=(struct  ptx_machine_function *)mtcsFunc->machine;
  struct ptx_machine_function *machine=(struct ptx_machine_function *)cfun->machine;


  rtx callee = XEXP (address, 0);
  rtx varargs = NULL_RTX;
  unsigned parallel = 0;

  if (!mtcs_ptx_preds_call_insn_operand/*!call_insn_operand*/((MtcsPtxPreds*)mtcsPreds,callee, mtcs_mode_get_Pmode(mtcsMode))){
     n_debug("mtcsptxemit.c -----nvptx.cc -----39--00 void nvptx_expand_call\n");

      callee = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,mtcs_mode_get_Pmode(mtcsMode), callee);
      address = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,address, mtcsMode->modes.M_QImode, callee);
  }

  if (GET_CODE (callee) == SYMBOL_REF){
      tree decl = SYMBOL_REF_DECL (callee);
      if (decl != NULL_TREE){
          if (DECL_STATIC_CHAIN (decl))
              machine->/*!cfun->machine*/has_chain = true;
          tree attr = oacc_get_fn_attrib (decl);
          if (attr){
             // n_error("未实现----- oacc \n");
             n_debug("mtcsptxemit.c -----nvptx.cc -----39--11 void nvptx_expand_call\n");

              tree dims = TREE_VALUE (attr);
              parallel = GOMP_DIM_MASK (GOMP_DIM_MAX) - 1;
              for (int ix = 0; ix != GOMP_DIM_MAX; ix++){
                  if (TREE_PURPOSE (dims) && !integer_zerop (TREE_PURPOSE (dims)))
                    break;
                  /* Not on this axis.  */
                  parallel ^= GOMP_DIM_MASK (ix);
                  dims = TREE_CHAIN (dims);
              }
          }
      }
  }

  unsigned nargs =  machine/*!cfun->machine*/->num_args;
  if ( machine/*!cfun->machine*/->is_variadic){
     n_debug("mtcsptxemit.c -----nvptx.cc -----39--22 void nvptx_expand_call\n");
      varargs = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mtcs_mode_get_Pmode(mtcsMode));
      mtcs_expr_emit_move_insn (mtcsExpr,varargs, mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL));
  }

  rtvec vec = rtvec_alloc (nargs + 1);
  rtx pat = gen_rtx_PARALLEL (VOIDmode, vec);
  int vec_pos = 0;

  rtx call = gen_rtx_CALL (VOIDmode, address, const0_rtx);
  rtx tmp_retval = retval;
  if (retval){
     n_debug("mtcsptxemit.c -----nvptx.cc -----39--33 void nvptx_expand_call GET_MODE (retval):%d\n",GET_MODE (retval));

      if (!mtcs_ptx_preds_register_operand/*!nvptx_register_operand*/((MtcsPtxPreds *)mtcsPreds,retval, GET_MODE (retval))){
         n_debug("mtcsptxemit.c -----nvptx.cc -----39--44 void nvptx_expand_call GET_MODE (retval):%d\n",GET_MODE (retval));

          tmp_retval = mtcs_emit_gen_reg_rtx(mtcsEmit,GET_MODE (retval));
      }
      call = gen_rtx_SET (tmp_retval, call);
  }
  XVECEXP (pat, 0, vec_pos++) = call;

  /* Construct the call insn, including a USE for each argument pseudo
     register.  These will be used when printing the insn.  */
  for (rtx arg =machine/*!cfun->machine*/->call_args; arg; arg = XEXP (arg, 1)){\
     n_debug("mtcsptxemit.c -----nvptx.cc -----39--55 void nvptx_expand_call vec_pos:%d\n",vec_pos);

    XVECEXP (pat, 0, vec_pos++) = gen_rtx_USE (VOIDmode, XEXP (arg, 0));
  }

  if (varargs){
     n_debug("mtcsptxemit.c -----nvptx.cc -----39--66 void nvptx_expand_call vec_pos:%d\n",vec_pos);

    XVECEXP (pat, 0, vec_pos++) = gen_rtx_USE (VOIDmode, varargs);
  }

  gcc_assert (vec_pos = XVECLEN (pat, 0));

  nvptx_emit_forking (self,parallel, true);
  mtcs_emit_emit_call_insn (mtcsEmit,pat);
  nvptx_emit_joining (self,parallel, true);
  if (tmp_retval != retval){
     n_debug("mtcsptxemit.c -----nvptx.cc -----39--77 void nvptx_expand_call vec_pos:%d\n",vec_pos);

      mtcs_expr_emit_move_insn (mtcsExpr,retval, tmp_retval);
  }
}



/* Expand the oacc fork & join primitive into ptx-required unspecs.  */
//原型 nvptx_expand_oacc_fork nvptx-protos.h nvptx.cc
void mtcs_ptx_emit_expand_oacc_fork (MtcsPtxEmit *self,unsigned mode)
{
    n_debug("mtcsptxemit.c -----nvptx.cc -----41-- void nvptx_expand_oacc_fork (unsigned mode)\n");

  nvptx_emit_forking (self,GOMP_DIM_MASK (mode), false);
}
//原型 nvptx_expand_oacc_join nvptx-protos.h nvptx.cc
void mtcs_ptx_emit_expand_oacc_join (MtcsPtxEmit *self,unsigned mode)
{
    n_debug("mtcsptxemit.c -----nvptx.cc -----42-- void nvptx_expand_oacc_join (unsigned mode)\n");

  nvptx_emit_joining (self,GOMP_DIM_MASK (mode), false);
}


/* Generate instruction(s) to unpack a 64 bit object into 2 32 bit
   objects.  */
//原型 nvptx_gen_unpack nvptx.cc
static rtx nvptx_gen_unpack (rtx dst0, rtx dst1, rtx src)
{
  n_debug("mtcsptxemit.c -----nvptx.cc -----124-- rtx nvptx_gen_unpack (rtx dst0, rtx dst1, rtx src)\n");
  rtx res;
  switch (GET_MODE (src)){
    case PTX_DImode:
      res = ptx_gen_unpackdisi2 (dst0, dst1, src);
      break;
    case PTX_DFmode:
      res = ptx_gen_unpackdfsi2 (dst0, dst1, src);
      break;
    default: gcc_unreachable ();
  }
  return res;
}

/* Generate instruction(s) to pack 2 32 bit objects into a 64 bit
   object.  */
//原型 nvptx_gen_pack nvptx.cc
static rtx nvptx_gen_pack (rtx dst, rtx src0, rtx src1)
{
  n_debug("mtcsptxemit.c -----nvptx.cc -----125-- rtx nvptx_gen_pack (rtx dst, rtx src0, rtx src1)\n");
  rtx res;
  switch (GET_MODE (dst)){
    case PTX_DImode:
      res = ptx_gen_packsidi2 (dst, src0, src1);
      break;
    case PTX_DFmode:
      res = ptx_gen_packsidf2 (dst, src0, src1);
      break;
    default: gcc_unreachable ();
  }
  return res;
}

/* Generate an instruction or sequence to broadcast register REG
   across the vectors of a single warp.  */
//原型 nvptx_gen_shuffle nvptx-protos.h nvptx.cc .md引用
rtx  mtcs_ptx_emit_gen_shuffle (MtcsPtxEmit *self,rtx dst, rtx src, rtx idx, enum ptx_shuffle_kind kind)
{
  MtcsEmit *mtcsEmit=(MtcsEmit *)self;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  //struct  ptx_machine_function *machine=(struct  ptx_machine_function *)mtcsFunc->machine;
  struct ptx_machine_function *machine=(struct ptx_machine_function *)cfun->machine;

  rtx res;
  n_debug("mtcsptxemit.c -----nvptx.cc -----44-- mtcs_ptx_emit_gen_shuffle kind:%d\n",kind,GET_MODE (dst));
  mtcs_print_rtl(stderr,dst);
  mtcs_print_rtl(stderr,src);
  mtcs_print_rtl(stderr,idx);

  switch (GET_MODE (dst)){
      case PTX_DCmode:
      case PTX_CDImode:
        {
          gcc_assert (GET_CODE (dst) == CONCAT);
          gcc_assert (GET_CODE (src) == CONCAT);
          rtx dst_real = XEXP (dst, 0);
          rtx dst_imag = XEXP (dst, 1);
          rtx src_real = XEXP (src, 0);
          rtx src_imag = XEXP (src, 1);

          mtcs_emit_start_sequence (mtcsEmit);
          mtcs_emit_emit_insn (mtcsEmit,mtcs_ptx_emit_gen_shuffle (self,dst_real, src_real, idx, kind));
          mtcs_emit_emit_insn (mtcsEmit,mtcs_ptx_emit_gen_shuffle (self,dst_imag, src_imag, idx, kind));
          res = mtcs_rtl_data_get_insns (mtcsRtlData);
          mtcs_emit_end_sequence (mtcsEmit);
        }
        break;
    case PTX_SImode:
      res = ptx_gen_nvptx_shufflesi/*!gen_nvptx_shufflesi*/(dst, src, idx, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,kind));
      break;
    case PTX_SFmode:
      res = ptx_gen_nvptx_shufflesf/*!gen_nvptx_shufflesf*/(dst, src, idx, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,kind));
      break;
    case PTX_DImode:
    case PTX_DFmode:
      {
        rtx tmp0 = mtcs_emit_gen_reg_rtx (mtcsEmit,PTX_SImode);
        rtx tmp1 = mtcs_emit_gen_reg_rtx (mtcsEmit,PTX_SImode);

        mtcs_emit_start_sequence (mtcsEmit);
        mtcs_emit_emit_insn(mtcsEmit,nvptx_gen_unpack (tmp0, tmp1, src));
        mtcs_emit_emit_insn(mtcsEmit,mtcs_ptx_emit_gen_shuffle/*!nvptx_gen_shuffle*/(self,tmp0, tmp0, idx, kind));
        mtcs_emit_emit_insn(mtcsEmit,mtcs_ptx_emit_gen_shuffle/*!nvptx_gen_shuffle*/(self,tmp1, tmp1, idx, kind));
        mtcs_emit_emit_insn(mtcsEmit,nvptx_gen_pack (dst, tmp0, tmp1));
        res = mtcs_rtl_data_get_insns (mtcsRtlData);
        mtcs_emit_end_sequence (mtcsEmit);
      }
      break;
    case PTX_V2SImode:
      {
        rtx src0 = mtcs_rtl_gen_rtx_SUBREG/*!gen_rtx_SUBREG*/(mtcsRTL,(machine_mode)PTX_SImode, src, 0);
        rtx src1 = mtcs_rtl_gen_rtx_SUBREG/*!gen_rtx_SUBREG*/(mtcsRTL,(machine_mode)PTX_SImode, src, 4);
        rtx dst0 = mtcs_rtl_gen_rtx_SUBREG/*!gen_rtx_SUBREG*/(mtcsRTL,(machine_mode)PTX_SImode, dst, 0);
        rtx dst1 = mtcs_rtl_gen_rtx_SUBREG/*!gen_rtx_SUBREG*/(mtcsRTL,(machine_mode)PTX_SImode, dst, 4);
        rtx tmp0 = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,(machine_mode)PTX_SImode);
        rtx tmp1 = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,(machine_mode)PTX_SImode);
        mtcs_emit_start_sequence (mtcsEmit);
        mtcs_emit_emit_insn(mtcsEmit,ptx_gen_movsi (tmp0, src0));
        mtcs_emit_emit_insn(mtcsEmit,ptx_gen_movsi (tmp1, src1));
        mtcs_emit_emit_insn(mtcsEmit,mtcs_ptx_emit_gen_shuffle/*!nvptx_gen_shuffle*/(self,tmp0, tmp0, idx, kind));
        mtcs_emit_emit_insn(mtcsEmit,mtcs_ptx_emit_gen_shuffle/*!nvptx_gen_shuffle*/(self,tmp1, tmp1, idx, kind));
        mtcs_emit_emit_insn(mtcsEmit,ptx_gen_movsi (dst0, tmp0));
        mtcs_emit_emit_insn(mtcsEmit,ptx_gen_movsi (dst1, tmp1));
        res = mtcs_rtl_data_get_insns (mtcsRtlData);
        mtcs_emit_end_sequence (mtcsEmit);
      }
      break;
    case PTX_V2DImode:
      {
        rtx src0 = mtcs_rtl_gen_rtx_SUBREG/*!gen_rtx_SUBREG*/(mtcsRTL,(machine_mode)PTX_DImode, src, 0);
        rtx src1 = mtcs_rtl_gen_rtx_SUBREG/*!gen_rtx_SUBREG*/(mtcsRTL,(machine_mode)PTX_DImode, src, 8);
        rtx dst0 = mtcs_rtl_gen_rtx_SUBREG/*!gen_rtx_SUBREG*/(mtcsRTL,(machine_mode)PTX_DImode, dst, 0);
        rtx dst1 = mtcs_rtl_gen_rtx_SUBREG/*!gen_rtx_SUBREG*/(mtcsRTL,(machine_mode)PTX_DImode, dst, 8);
        rtx tmp0 = mtcs_emit_gen_reg_rtx (mtcsEmit,PTX_DImode);
        rtx tmp1 = mtcs_emit_gen_reg_rtx (mtcsEmit,PTX_DImode);
        mtcs_emit_start_sequence (mtcsEmit);
        mtcs_emit_emit_insn(mtcsEmit,gen_movdi (tmp0, src0));
        mtcs_emit_emit_insn(mtcsEmit,gen_movdi (tmp1, src1));
        mtcs_emit_emit_insn(mtcsEmit,mtcs_ptx_emit_gen_shuffle (self,tmp0, tmp0, idx, kind));
        mtcs_emit_emit_insn(mtcsEmit,mtcs_ptx_emit_gen_shuffle (self,tmp1, tmp1, idx, kind));
        mtcs_emit_emit_insn(mtcsEmit,ptx_gen_movdi (dst0, tmp0));
        mtcs_emit_emit_insn(mtcsEmit,ptx_gen_movdi (dst1, tmp1));
        res = mtcs_rtl_data_get_insns (mtcsRtlData);
        mtcs_emit_end_sequence (mtcsEmit);
      }
      break;
    case PTX_BImode:
      {
        rtx tmp = mtcs_emit_gen_reg_rtx (mtcsEmit,PTX_SImode);
        mtcs_emit_start_sequence (mtcsEmit);
        mtcs_emit_emit_insn(mtcsEmit,ptx_gen_sel_truesi (tmp, src, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,1), const0_rtx));
        mtcs_emit_emit_insn(mtcsEmit,mtcs_ptx_emit_gen_shuffle (self,tmp, tmp, idx, kind));
        mtcs_emit_emit_insn(mtcsEmit,gen_rtx_SET (dst, gen_rtx_NE (mtcsMode->modes.M_BImode, tmp, const0_rtx)));
        res = mtcs_rtl_data_get_insns (mtcsRtlData);
        mtcs_emit_end_sequence (mtcsEmit);
      }
      break;
    case PTX_QImode:
    case PTX_HImode:
      {
        rtx tmp = mtcs_emit_gen_reg_rtx (mtcsEmit,PTX_SImode);
        mtcs_emit_start_sequence (mtcsEmit);
        mtcs_emit_emit_insn(mtcsEmit,gen_rtx_SET (tmp, gen_rtx_fmt_e (ZERO_EXTEND, (machine_mode)PTX_SImode, src)));
        mtcs_emit_emit_insn(mtcsEmit,mtcs_ptx_emit_gen_shuffle (self,tmp, tmp, idx, kind));
        mtcs_emit_emit_insn(mtcsEmit,gen_rtx_SET (dst, gen_rtx_fmt_e (TRUNCATE, GET_MODE (dst),tmp)));
        res = mtcs_rtl_data_get_insns (mtcsRtlData);
        mtcs_emit_end_sequence (mtcsEmit);
      }
      break;
    default:
      gcc_unreachable ();
  }
  return res;
}

//原型 nvptx_gen_shuffle nvptx-protos.h nvptx.cc .md引用
rtx  mtcs_ptx_emit_shfl_xor_sync(MtcsPtxEmit *self,rtx target, rtx src, rtx laneMask, rtx memberMask)
{
  MtcsEmit *mtcsEmit=(MtcsEmit *)self;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  //struct  ptx_machine_function *machine=(struct  ptx_machine_function *)mtcsFunc->machine;
  struct ptx_machine_function *machine=(struct ptx_machine_function *)cfun->machine;

  rtx res;
  n_debug("mtcsptxemit.c mtcs_ptx_emit_shfl_xor_sync 00 target mode:%d\n",GET_MODE (target));
  mtcs_print_rtl(stderr,target);
  mtcs_print_rtl(stderr,src);
  mtcs_print_rtl(stderr,laneMask);
  mtcs_print_rtl(stderr,memberMask);

  switch (GET_MODE (target)){

    case PTX_SImode:
    {
       n_debug("mtcsptxemit.c mtcs_ptx_emit_shfl_xor_sync 11\n");
      res = ptx_gen_shfl_xor_syncsi/*!gen_nvptx_shufflesi*/(target, src, laneMask, memberMask);
    }

      break;
    case PTX_SFmode:
       n_debug("mtcsptxemit.c mtcs_ptx_emit_shfl_xor_sync 22\n");
       res = ptx_gen_shfl_xor_syncsf/*!gen_nvptx_shufflesi*/(target, src, laneMask, memberMask);
      break;
    default:
      gcc_unreachable ();
  }
  return res;
}

static ptx_data_area nvptx_mem_data_area (const_rtx x)
{
  gcc_assert (GET_CODE (x) == MEM);

  const_rtx addr = XEXP (x, 0);
  subrtx_iterator::array_type array;
  FOR_EACH_SUBRTX (iter, array, addr, ALL)
    if (SYMBOL_REF_P (*iter))
      return PTX_SYMBOL_DATA_AREA (*iter);

  return PTX_DATA_AREA_GENERIC;
}

//原型 nvptx_mem_maybe_shared_p nvptx-protos.h nvptx.cc
bool mtcs_ptx_emit_mem_maybe_shared_p (MtcsPtxEmit *self,const_rtx x)
{
  ptx_data_area area = nvptx_mem_data_area (x);
  return area == PTX_DATA_AREA_SHARED || area == PTX_DATA_AREA_GENERIC;
}


MtcsPtxEmit *mtcs_ptx_emit_new(MtcsMode *mtcsMode)
{
    MtcsPtxEmit *self = n_slice_alloc0 (sizeof(MtcsPtxEmit));
    mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
    mtcs_emit_init((MtcsEmit *)self);
    mtcsPtxEmitInit(self);
    return self;
}
