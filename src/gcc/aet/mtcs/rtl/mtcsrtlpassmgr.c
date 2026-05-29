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
#include "diagnostic-core.h"

#include "mtcsrtlpassmgr.h"
#include "../mtcstarget.h"
#include "../mtcsdfcore.h"
#include "../mtcsdfproblems.h"


MtcsFwprop *mtcs_rtl_pass_mgr_get_fwprop(MtcsRtlPassMgr *self)
{
   return self->mtcsFwprop;
}

MtcsCprop *mtcs_rtl_pass_mgr_get_cprop(MtcsRtlPassMgr *self)
{
   return self->mtcsCprop;
}

MtcsGcse *mtcs_rtl_pass_mgr_get_gcse(MtcsRtlPassMgr *self)
{
   return self->mtcsGcse;
}

MtcsIfcvt *mtcs_rtl_pass_mgr_get_ifcvt(MtcsRtlPassMgr *self)
{
   return self->mtcsIfcvt;
}

MtcsDse *mtcs_rtl_pass_mgr_get_dse(MtcsRtlPassMgr *self)
{
   return self->mtcsDse;
}

MtcsCombine *mtcs_rtl_pass_mgr_get_combine(MtcsRtlPassMgr *self)
{
   return self->mtcsCombine;
}

MtcsBBReorder *mtcs_rtl_pass_mgr_get_bb_reorder(MtcsRtlPassMgr *self)
{
   return self->mtcsBBReorder;
}

MtcsReorg *mtcs_rtl_pass_mgr_get_reorg(MtcsRtlPassMgr *self)
{
   return self->mtcsReorg;
}

static void mtcsRtlPassMgrInit(MtcsRtlPassMgr *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   self->mtcsFwprop=mtcs_fwprop_new(mtcsMode);
   self->mtcsCprop=mtcs_cprop_new(mtcsMode);
   self->mtcsGcse=mtcs_gcse_new(mtcsMode);
   self->mtcsIfcvt=mtcs_ifcvt_new(mtcsMode);
   self->mtcsDse=mtcs_dse_new(mtcsMode);
   self->mtcsCombine = mtcs_combine_new(mtcsMode);
   self->mtcsBBReorder = mtcs_bb_reorder_new(mtcsMode);
   self->mtcsReorg = mtcs_reorg_new(mtcsMode);

}

MtcsRtlPassMgr *mtcs_rtl_pass_mgr_new(MtcsMode *mtcsMode)
{
   MtcsRtlPassMgr *self = n_slice_alloc0 (sizeof(MtcsRtlPassMgr));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcsRtlPassMgrInit(self);
   return self;
}

/*-------------------------------- *rest_of_compilation -------------------*/
//原型 NEXT_PASS (pass_rest_of_compilation, 1);  RTL_PASS  passes.cc   *rest_of_compilation   n  无execute代码
static nboolean pass_rest_of_compilation_gate_cb(MtcsPass *mtcsPass,function *fun)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
    MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
    return !(mtcsOptionsItem->x_rtl_dump_and_exit || mtcsOptionsItem->x_flag_syntax_only || seen_error ());
}

static void mtcsPassRestOfCompilationInit(MtcsPassRestOfCompilation *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->gate=pass_rest_of_compilation_gate_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            PROP_rtl,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassRestOfCompilation *mtcs_pass_rest_of_compilation_new(MtcsMode *mtcsMode)
{
   MtcsPassRestOfCompilation *self = n_slice_alloc0 (sizeof(MtcsPassRestOfCompilation));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"*rest_of_compilation");
   mtcsPassRestOfCompilationInit(self);
   return self;
}

//原型 NEXT_PASS (pass_reginfo_init, 1); RTL_PASS reginfo.cc reginfo n 无条件执行 reginfo_init
static nuint reginfo_execute_cb(MtcsPass *mtcsPass,function *func)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   return mtcs_reg_reginfo_init (mtcsReg);
}

static void mtcsPassRegInfoInit(MtcsPassRegInfo *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =reginfo_execute_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassRegInfo *mtcs_pass_reg_info_new(MtcsMode *mtcsMode)
{
   MtcsPassRegInfo *self = n_slice_alloc0 (sizeof(MtcsPassRegInfo));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"reginfo");
   mtcsPassRegInfoInit(self);
   return self;
}

//原型 NEXT_PASS (pass_initialize_regs, 1);  RTL_PASS  init-regs.cc   init-regs   y  有条件执行 optimize > 0 initialize_uninitialized_regs

/* Check all of the uses of pseudo variables.  If any use that is MUST
   uninitialized, add a store of 0 immediately before it.  For
   subregs, this makes combine happy.  For full word regs, this makes
   other optimizations, like the register allocator and the reg-stack
   happy as well as papers over some problems on the arm and other
   processors where certain isa constraints cannot be handled by gcc.
   These are of the form where two operands to an insn my not be the
   same.  The ra will only make them the same if they do not
   interfere, and this can only happen if one is not initialized.

   There is also the unfortunate consequence that this may mask some
   buggy programs where people forget to initialize stack variable.
   Any programmer with half a brain would look at the uninitialized
   variable warnings.  */
static void initialize_uninitialized_regs (MtcsPassInitRegs *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr *mtcsExpr =mtcs_target_get_expr(mtcsTarget);
   MtcsDfcore *mtcsDfcore =mtcs_target_get_dfcore (mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsDfproblems *mtcsDfproblems =mtcs_target_get_dfproblems(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   basic_block bb;
   auto_bitmap already_genned;

   if (mtcsOptionsItem->x_optimize == 1){
      mtcs_dfproblems_df_live_add_problem/*!df_live_add_problem*/(mtcsDfproblems);
      mtcs_dfproblems_df_live_set_all_dirty/*!df_live_set_all_dirty*/(mtcsDfproblems);
   }

   mtcs_dfcore_df_analyze/*!df_analyze*/(mtcsDfcore);

   FOR_EACH_BB_FN (bb, cfun){
      rtx_insn *insn;
      bitmap lr = DF_LR_IN (bb);
      bitmap ur = DF_LIVE_IN (bb);
      bitmap_clear (already_genned);

      FOR_BB_INSNS (bb, insn){
         df_ref use;
         if (!NONDEBUG_INSN_P (insn))
            continue;

         FOR_EACH_INSN_USE (use, insn){
            unsigned int regno = DF_REF_REGNO (use);

            /* Only do this for the pseudos.  */
            if (regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg))
               continue;

            /* Ignore pseudo PIC register.  */
            if (mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(mtcsRTL)
            && regno == REGNO (mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(mtcsRTL)))
               continue;

            /* Do not generate multiple moves for the same regno.
            This is common for sequences of subreg operations.
            They would be deleted during combine but there is no
            reason to churn the system.  */
            if (bitmap_bit_p (already_genned, regno))
               continue;

            /* A use is MUST uninitialized if it reaches the top of
            the block from the inside of the block (the lr test)
            and no def for it reaches the top of the block from
            outside of the block (the ur test).  */
            if (bitmap_bit_p (lr, regno)  && (!bitmap_bit_p (ur, regno))){
               rtx_insn *move_insn;
               rtx reg = DF_REF_REAL_REG (use);

               bitmap_set_bit (already_genned, regno);

               mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
               mtcs_emit_emit_clobber/*!emit_clobber*/(mtcsEmit,reg);
               /* PR98872: Only emit an initialization if MODE has a
               CONST0_RTX defined.  */
               if (CONST0_RTX (GET_MODE (reg)))
                  mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,reg, CONST0_RTX (GET_MODE (reg)));
               move_insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
               mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
               mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,move_insn, insn);
               if (dump_file)
                  fprintf (dump_file, "adding initialization in %s of reg %d at in block %d for insn %d.\n",
                              current_function_name (), regno, bb->index,INSN_UID (insn));
            }
         }
      }
   }

   if (mtcsOptionsItem->x_optimize == 1){
      if (dump_file)
         mtcs_dfcore_df_dump/*!df_dump*/(mtcsDfcore,dump_file);
      mtcs_dfcore_df_remove_problem/*!df_remove_problem*/(mtcsDfcore,df_live);
   }
}

static nuint init_regs_execute_cb(MtcsPass *mtcsPass,function *func)
{
   MtcsPassInitRegs *self= (MtcsPassInitRegs *)mtcsPass;
   initialize_uninitialized_regs (self);
   return 0;
}

static nboolean init_regs_gate_cb(MtcsPass *mtcsPass,function *fun)
{
    MtcsPassInitRegs *self=(MtcsPassInitRegs *)mtcsPass;
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
    MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
    return mtcsOptionsItem->x_optimize>0;
}

static void mtcsPassInitRegsInit(MtcsPassInitRegs *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =init_regs_execute_cb;
    mtcsPass->gate=init_regs_gate_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          TODO_df_finish /*todo_flags_finish */);
}

MtcsPassInitRegs *mtcs_pass_init_regs_new(MtcsMode *mtcsMode)
{
   MtcsPassInitRegs *self = n_slice_alloc0 (sizeof(MtcsPassInitRegs));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"init-regs");
   mtcsPassInitRegsInit(self);
   return self;
}

//原型 NEXT_PASS (pass_stack_ptr_mod, 1);  RTL_PASS  stack-ptr-mod.cc *stack_ptr_mod n 无条件执行 ...crtl->sp_is_unchanging...
/* Determine if the stack pointer is constant over the life of the function.
   Only useful before prologues have been emitted.  */
//原型 notice_stack_pointer_modification_1 stack-ptr-mode.cc
static void notice_stack_pointer_modification_1 (rtx x, const_rtx pat ATTRIBUTE_UNUSED,
                 void *userData ATTRIBUTE_UNUSED)
{
   MtcsPassStackPtrMod *self=(MtcsPassStackPtrMod *)userData;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL = mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   if (x == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)
   /* The stack pointer is only modified indirectly as the result
   of a push until later.  See the comments in rtl.texi
   regarding Embedded Side-Effects on Addresses.  */
   || (MEM_P (x)
   && GET_RTX_CLASS (GET_CODE (XEXP (x, 0))) == RTX_AUTOINC
   && XEXP (XEXP (x, 0), 0) == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)))
      mtcsRtlData/*!crtl*/->sp_is_unchanging = 0;
}

static nuint stack_ptr_mod_execute_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsPassStackPtrMod *self=(MtcsPassStackPtrMod *)mtcsPass;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan *mtcsDfscan = mtcs_target_get_dfscan(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   basic_block bb;
   rtx_insn *insn;

   /* Assume that the stack pointer is unchanging if alloca hasn't
   been used.  */
   mtcsRtlData/*!crtl*/->sp_is_unchanging = !fun->calls_alloca;
   if (mtcsRtlData/*!crtl*/->sp_is_unchanging)
      FOR_EACH_BB_FN (bb, fun)
         FOR_BB_INSNS (bb, insn){
            if (INSN_P (insn)){
               /* Check if insn modifies the stack pointer.  */
               mtcs_rtlanal_note_stores/*!note_stores*/(mtcsRtlanal,insn, notice_stack_pointer_modification_1, (void*)self/*!NULL*/);
               if (! mtcsRtlData/*!crtl*/->sp_is_unchanging)
                  return 0;
            }
         }
   /* The value coming into this pass was 0, and the exit block uses
   are based on this.  If the value is now 1, we need to redo the
   exit block uses.  */
   if (df && mtcsRtlData/*!crtl*/->sp_is_unchanging)
      mtcs_dfscan_df_update_exit_block_uses/*!df_update_exit_block_uses*/(mtcsDfscan);
   return 0;
}

static void mtcsPassStackPtrModInit(MtcsPassStackPtrMod *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =stack_ptr_mod_execute_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassStackPtrMod *mtcs_pass_stack_ptr_mod_new(MtcsMode *mtcsMode)
{
   MtcsPassStackPtrMod *self = n_slice_alloc0 (sizeof(MtcsPassStackPtrMod));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"*stack_ptr_mod");
   mtcsPassStackPtrModInit(self);
   return self;
}

//原型 NEXT_PASS (pass_postreload, 1);  RTL_PASS  passes.cc *all-postreload n 无执行代码 reload_completed
static nboolean post_reload_gate_cb(MtcsPass *mtcsPass,function *fun)
{
    return reload_completed;
}

static void mtcsPassPostReloadInit(MtcsPassPostReload *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->gate=post_reload_gate_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
          PROP_rtl,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassPostReload *mtcs_pass_post_reload_new(MtcsMode *mtcsMode)
{
   MtcsPassPostReload *self = n_slice_alloc0 (sizeof(MtcsPassPostReload));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"*all-postreload");
   mtcsPassPostReloadInit(self);
   return self;
}

//原型 NEXT_PASS (pass_late_compilation, 1);RTL_PASS  passes.cc *all-late_compilation n 无执行代码 reload_completed || targetm.no_register_allocation;
static nboolean all_late_compilation_gate_cb(MtcsPass *mtcsPass,function *fun)
{
     MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
     MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    return reload_completed || mtcsTarget/*!targetm.no_register_allocation*/->no_register_allocation;
}

static void mtcsPassAllLateCompilationInit(MtcsPassAllLateCompilation *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->gate=all_late_compilation_gate_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
          PROP_rtl,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassAllLateCompilation *mtcs_pass_all_late_compilation_new(MtcsMode *mtcsMode)
{
   MtcsPassAllLateCompilation *self = n_slice_alloc0 (sizeof(MtcsPassAllLateCompilation));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"*all-late_compilation");
   mtcsPassAllLateCompilationInit(self);
   return self;
}
