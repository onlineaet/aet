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
 * base on cfgloopanal.cc
 */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "rtl.h"
#include "tree.h"
#include "predict.h"
#include "memmodel.h"
#include "emit-rtl.h"
#include "cfgloop.h"
#include "explow.h"
#include "expr.h"
#include "graphds.h"
#include "sreal.h"
#include "regs.h"
#include "function-abi.h"

#include "mtcscfgloopanal.h"
#include "mtcstarget.h"

static void mtcsCfgLoopanalInit(MtcsCfgLoopanal *self)
{
    self->targetCfgLoop=XNEW(struct target_cfgloop);
    memset(self->targetCfgLoop,0,sizeof(struct target_cfgloop));
}

//#define target_avail_regs \
//  (this_target_cfgloop->x_target_avail_regs)
//#define target_clobbered_regs \
//  (this_target_cfgloop->x_target_clobbered_regs)
//#define target_res_regs \
//  (this_target_cfgloop->x_target_res_regs)
//#define target_reg_cost \
//  (this_target_cfgloop->x_target_reg_cost)
//#define target_spill_cost \
//  (this_target_cfgloop->x_target_spill_cost)

/* Initialize the constants for computing set costs.  */
//原型 init_set_costs cfgloop.h cfgloopanal.cc
void mtcs_cfg_loopanal_init_set_costs (MtcsCfgLoopanal *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsFuncAbi *mtcsFuncAbi=mtcs_target_get_func_abi(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  mtcs_predefined_function_abi *abi= mtcs_func_abi_get_default(mtcsFuncAbi);

  nuint pMode=mtcs_mode_get_Pmode(mtcsMode);
  //原型 LAST_VIRTUAL_REGISTER  rtl.h
  int lastVirtualRegno= mtcs_reg_get_last_virtual_regno(mtcsReg);
  int speed;
  rtx_insn *seq;
  rtx reg1 = mtcs_rtl_gen_raw_REG/*gen_raw_REG*/(mtcsRTL,mtcsMode->modes.M_SImode, lastVirtualRegno/*!LAST_VIRTUAL_REGISTER*/ + 1);
  rtx reg2 = mtcs_rtl_gen_raw_REG/*gen_raw_REG*/(mtcsRTL,mtcsMode->modes.M_SImode, lastVirtualRegno/*!LAST_VIRTUAL_REGISTER*/ + 2);
  rtx addr = mtcs_rtl_gen_raw_REG/*gen_raw_REG*/(mtcsRTL,pMode, lastVirtualRegno/*!LAST_VIRTUAL_REGISTER*/ + 3);
  n_debug("mtcscfgloopanal.c init_set_costs 00 SImode:%d Pmode:%d LAST_VIRTUAL_REGISTER:%d\n",
          mtcsMode->modes.M_SImode,pMode,lastVirtualRegno);
  rtx mem = mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,gen_rtx_MEM (mtcsMode->modes.M_SImode, addr));
  unsigned i;
  int    firstPseudoRegister=  mtcs_reg_get_first_pseudo_register(mtcsReg);

  n_debug("mtcscfgloopanal.c init_set_costs 11 reg1:%d %d reg2:%d %d addr:%d %d mem:%d %d\n",
          reg1->code,reg1->mode,reg2->code,reg2->mode,addr->code,addr->mode,mem->code,mem->mode);
  self->targetCfgLoop->x_target_avail_regs/*!target_avail_regs*/ = 0;
  self->targetCfgLoop->x_target_clobbered_regs/*!target_clobbered_regs*/ = 0;
  //GENERAL_REGS每个平台不一样 nvptx 来自 enum reg_class {  NO_REGS,    ALL_REGS, LIM_REG_CLASSES };
  //#define GENERAL_REGS ALL_REGS
  nuint generalRegs=mtcs_reg_get_general_regs(mtcsReg);/*!GENERAL_REGS*/
  char *fixedRegs=mtcsReg->hardRegs.x_fixed_regs;
  for (i = 0; i < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; i++)
    if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(
            &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[generalRegs/*!GENERAL_REGS*/], i)
            && !fixedRegs[i]/*!fixed_regs[i]*/){
        self->targetCfgLoop->x_target_avail_regs++;
        /* ??? This is only a rough heuristic.  It doesn't cope well
           with alternative ABIs, but that's an optimization rather than
           correctness issue.  */
        n_debug("mtcscfgloopanal.c init_set_costs 22 i:%d FIRST_PSEUDO_REGISTER:%d  target_avail_regs:%d\n",
                    i,firstPseudoRegister,self->targetCfgLoop->x_target_avail_regs);
        if (abi/*!default_function_abi*/->clobbers_full_reg_p (i)){
           n_debug("mtcscfgloopanal.c init_set_costs 33 i:%d FIRST_PSEUDO_REGISTER:%d  target_avail_regs:%d target_clobbered_regs:%d\n",
                                        i,firstPseudoRegister,self->targetCfgLoop->x_target_avail_regs,
                                        self->targetCfgLoop->x_target_clobbered_regs);
            self->targetCfgLoop->x_target_clobbered_regs++;
        }
    }

  self->targetCfgLoop->x_target_res_regs = 3;
  n_debug("mtcscfgloopanal.c init_set_costs 44 \n");

  for (speed = 0; speed < 2; speed++){
      mtcsRtlData/*!crtl*/->maybe_hot_insn_p = speed;
      /* Set up the costs for using extra registers:

     1) If not many free registers remain, we should prefer having an
        additional move to decreasing the number of available registers.
        (TARGET_REG_COST).
     2) If no registers are available, we need to spill, which may require
        storing the old value to memory and loading it back
        (TARGET_SPILL_COST).  */

      mtcs_emit_start_sequence (mtcsEmit);
      n_debug("mtcscfgloopanal.c init_set_costs 55 \n");

      mtcs_expr_emit_move_insn (mtcsExpr,reg1, reg2);
      n_debug("mtcscfgloopanal.c init_set_costs 66 \n");

      seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      n_debug("mtcscfgloopanal.c init_set_costs 77 \n");

      mtcs_emit_end_sequence(mtcsEmit);
      n_debug("mtcscfgloopanal.c init_set_costs 88 \n");

      self->targetCfgLoop->x_target_reg_cost [speed] = mtcs_rtlanal_seq_cost/*!seq_cost*/(mtcsRtlanal,seq, speed);
      n_debug("mtcscfgloopanal.c init_set_costs 99 \n");

      mtcs_emit_start_sequence (mtcsEmit);
      n_debug("mtcscfgloopanal.c init_set_costs 100 \n");

      mtcs_expr_emit_move_insn (mtcsExpr,mem, reg1);
      n_debug("mtcscfgloopanal.c init_set_costs 101 \n");

      mtcs_expr_emit_move_insn (mtcsExpr,reg2, mem);
      n_debug("mtcscfgloopanal.c init_set_costs 102 \n");

      seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      n_debug("mtcscfgloopanal.c init_set_costs 103 \n");

      mtcs_emit_end_sequence (mtcsEmit);
      n_debug("mtcscfgloopanal.c init_set_costs 104 \n");

      self->targetCfgLoop->x_target_spill_cost [speed] = seq_cost (seq, speed);
  }
  mtcs_func_default_rtl_profile/*!default_rtl_profile ()*/(mtcsFunc);
}

MtcsCfgLoopanal *mtcs_cfg_loopanal_new(MtcsMode *mtcsMode)
{
   MtcsCfgLoopanal *self = n_slice_alloc0 (sizeof(MtcsCfgLoopanal));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsCfgLoopanalInit(self);
   return self;
}

