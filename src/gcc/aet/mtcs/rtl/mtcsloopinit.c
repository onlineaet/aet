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
 * base on loop-init.cc
 */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "cfghooks.h"
#include "df.h"
#include "regs.h"
#include "cfgcleanup.h"
#include "cfgloop.h"
#include "tree-pass.h"
#include "tree-ssa-loop-niter.h"
#include "loop-unroll.h"
#include "tree-scalar-evolution.h"
#include "tree-cfgcleanup.h"

#include "aet/aetprinttree.h"
#include "mtcsloopinit.h"
#include "../mtcstarget.h"
#include "../mtcsdfcore.h"
#include "../mtcsdfproblems.h"

/* Apply FLAGS to the loop state.  */

static void apply_loop_flags (MtcsLoopinit *self,unsigned flags)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgLoopManip *mtcsCfgLoopManip=mtcs_target_get_cfg_loop_manip(mtcsTarget);

   if (flags & LOOPS_MAY_HAVE_MULTIPLE_LATCHES){
      /* If the loops may have multiple latches, we cannot canonicalize
      them further (and most of the loop manipulation functions will
      not work).  However, we avoid modifying cfg, which some
      passes may want.  */
      gcc_assert ((flags & ~(LOOPS_MAY_HAVE_MULTIPLE_LATCHES
                     | LOOPS_HAVE_RECORDED_EXITS
                     | LOOPS_HAVE_MARKED_IRREDUCIBLE_REGIONS)) == 0);
                     loops_state_set (LOOPS_MAY_HAVE_MULTIPLE_LATCHES);
   }else
      disambiguate_loops_with_multiple_latches ();

   /* Create pre-headers.  */
   if (flags & LOOPS_HAVE_PREHEADERS){
      int cp_flags = CP_SIMPLE_PREHEADERS;

      if (flags & LOOPS_HAVE_FALLTHRU_PREHEADERS)
         cp_flags |= CP_FALLTHRU_PREHEADERS;

      mtcs_cfg_loop_mainip_create_preheader/*!create_preheaders*/(mtcsCfgLoopManip,cp_flags);
   }

   /* Force all latches to have only single successor.  */
   if (flags & LOOPS_HAVE_SIMPLE_LATCHES)
      mtcs_cfg_loop_mainip_force_single_succ_latches/*!force_single_succ_latches*/(mtcsCfgLoopManip);

   /* Mark irreducible loops.  */
   if (flags & LOOPS_HAVE_MARKED_IRREDUCIBLE_REGIONS)
      mark_irreducible_loops ();

   if (flags & LOOPS_HAVE_RECORDED_EXITS)
      record_loop_exits ();
}
/* Initialize loop structures.  This is used by the tree and RTL loop
   optimizers.  FLAGS specify what properties to compute and/or ensure for
   loops.  */
//原型 loop_optimizer_init cfgloop.h loop-init.cc
void mtcs_loopinit_loop_optimizer_init (MtcsLoopinit *self,unsigned flags)
{
   if (!current_loops){
      gcc_assert (!(cfun->curr_properties & PROP_loops));
      /* Find the loops.  */
      current_loops = flow_loops_find (NULL);
   }else{
      bool recorded_exits = loops_state_satisfies_p (LOOPS_HAVE_RECORDED_EXITS);
      bool needs_fixup = loops_state_satisfies_p (LOOPS_NEED_FIXUP);

      gcc_assert (cfun->curr_properties & PROP_loops);
      /* Ensure that the dominators are computed, like flow_loops_find does.  */
      calculate_dominance_info (CDI_DOMINATORS);

      if (!needs_fixup)
         checking_verify_loop_structure ();

      /* Clear all flags.  */
      if (recorded_exits)
         release_recorded_exits (cfun);
      loops_state_clear (~0U);

      if (needs_fixup){
         /* Apply LOOPS_MAY_HAVE_MULTIPLE_LATCHES early as fix_loop_structure
         re-applies flags.  */
         loops_state_set (flags & LOOPS_MAY_HAVE_MULTIPLE_LATCHES);
         fix_loop_structure (NULL);
      }
   }

   /* Apply flags to loops.  */
   apply_loop_flags (self,flags);
   /* Dump loops.  */
   flow_loops_dump (dump_file, NULL, 1);
   checking_verify_loop_structure ();

}

static void mtcsLoopInitInit(MtcsLoopinit *self)
{

}

MtcsLoopinit *mtcs_loopinit_new(MtcsMode *mtcsMode)
{
   MtcsLoopinit *self = n_slice_alloc0 (sizeof(MtcsLoopinit));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcsLoopInitInit(self);
   return self;
}



//原型 NEXT_PASS (pass_loop2, 1); RTL_PASS loop-init.cc y 无执行代码  optimize > 0..
static nboolean loop2_gate_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   if (mtcsOptionsItem->x_optimize > 0
   && (mtcsOptionsItem->x_flag_move_loop_invariants
   || mtcsOptionsItem->x_flag_unswitch_loops
   || mtcsOptionsItem->x_flag_unroll_loops
   || (mtcsOptionsItem->x_flag_branch_on_count_reg
         && target_rtx_have_doloop_end/*!targetm.have_doloop_end*/(mtcsMachine->tmrtx))
   || cfun->has_unroll))
      return true;
   else{
      /* No longer preserve loops, remove them now.  */
      fun->curr_properties &= ~PROP_loops;
      if (current_loops)
         loop_optimizer_finalize ();
      return false;
   }
}

static void mtcsPassLoop2Init(MtcsPassLoop2 *self)
{
   MtcsPass *mtcsPass= (MtcsPass *)self;
   mtcsPass->gate =loop2_gate_cb;
   mtcs_pass_set_properties(mtcsPass,
      0,/* properties_required */
      0, /* properties_provided */
      0 /* properties_destroyed */);
   mtcs_pass_set_todo_flags(mtcsPass,
      0, /* todo_flags_start */
      0 /*todo_flags_finish */);
}

MtcsPassLoop2 *mtcs_pass_loop2_new(MtcsMode *mtcsMode)
{
   MtcsPassLoop2 *self = n_slice_alloc0 (sizeof(MtcsPassLoop2));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"loop2");
   mtcsPassLoop2Init(self);
   return self;
}

//原型 NEXT_PASS (pass_rtl_loop_init, 1); RTL_PASS loop-init.cc loop2_init  n 无条件执行  rtl_loop_init
//原型 rtl_loop_init loop-init.cc
static nuint loop_init_execute_cb(MtcsPass *mtcsPass,function *func)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsLoopinit *mtcsLoopinit = mtcs_target_get_loopinit(mtcsTarget);
   n_debug("mtcsloopinit.c loop_init_execute_cb state :%d\n",mtcs_cfg_context_get_state(mtcsCfgContext));
   gcc_assert (mtcs_cfg_context_get_state/*!current_ir_type ()*/(mtcsCfgContext) == IR_RTL_CFGLAYOUT);
   if (dump_file){
      dump_reg_info (dump_file);
      mtcs_cfg_context_dump_flow_info/*!dump_flow_info*/(mtcsCfgContext,dump_file, dump_flags);
   }
   mtcs_loopinit_loop_optimizer_init/*!loop_optimizer_init*/(mtcsLoopinit,LOOPS_NORMAL | LOOPS_HAVE_RECORDED_EXITS);
   return 0;
}

static void mtcsPassLoopInitInit(MtcsPassLoopInit *self)
{
   MtcsPass *mtcsPass= (MtcsPass *)self;
   mtcsPass->execute =loop_init_execute_cb;
   mtcs_pass_set_properties(mtcsPass,
      0,/* properties_required */
      0, /* properties_provided */
      0 /* properties_destroyed */);
   mtcs_pass_set_todo_flags(mtcsPass,
      0, /* todo_flags_start */
      0 /*todo_flags_finish */);
}

MtcsPassLoopInit *mtcs_pass_loop_init_new(MtcsMode *mtcsMode)
{
   MtcsPassLoopInit *self = n_slice_alloc0 (sizeof(MtcsPassLoopInit));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"loop2_init");
   mtcsPassLoopInitInit(self);
   return self;
}


//原型 NEXT_PASS (pass_rtl_doloop, 1); RTL_PASS loop-init.cc loop2_doloop  y 有条件执行flag_branch_on_count_reg && targetm.have_doloop_end
//doloop_optimize_loops
static nuint doloop_execute_cb(MtcsPass *mtcsPass,function *fun)
{
   if (number_of_loops (fun) > 1)
      doloop_optimize_loops ();
   return 0;
}

static nboolean doloop_gate_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   return (mtcsOptionsItem->x_flag_branch_on_count_reg
         && target_rtx_have_doloop_end/*!targetm.have_doloop_end*/(mtcsMachine->tmrtx));
}

static void mtcsPassDoloopInit(MtcsPassDoloop *self)
{
   MtcsPass *mtcsPass= (MtcsPass *)self;
   mtcsPass->execute =doloop_execute_cb;
   mtcsPass->gate =doloop_gate_cb;
   mtcs_pass_set_properties(mtcsPass,
      0,/* properties_required */
      0, /* properties_provided */
      0 /* properties_destroyed */);
   mtcs_pass_set_todo_flags(mtcsPass,
      0, /* todo_flags_start */
      0 /*todo_flags_finish */);
}

MtcsPassDoloop *mtcs_pass_doloop_new(MtcsMode *mtcsMode)
{
   MtcsPassDoloop *self = n_slice_alloc0 (sizeof(MtcsPassDoloop));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"loop2_doloop");
   mtcsPassDoloopInit(self);
   return self;
}

//原型 NEXT_PASS (pass_rtl_loop_done, 1); RTL_PASS loop-init.cc loop2_done n 无条件执行 loop_optimizer_finalize  cleanup_cfg (0);
static nuint loop_done_execute_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgCleanup *mtcsCfgCleanup=mtcs_target_get_cfg_cleanup(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);

   /* No longer preserve loops, remove them now.  */
   fun->curr_properties &= ~PROP_loops;
   loop_optimizer_finalize ();
   free_dominance_info (CDI_DOMINATORS);

   mtcs_cfg_cleanup_cleanup_cfg/*!cleanup_cfg*/(mtcsCfgCleanup,0);
   if (dump_file){
      dump_reg_info (dump_file);
      mtcs_cfg_context_dump_flow_info/*!dump_flow_info*/(mtcsCfgContext,dump_file, dump_flags);
   }
   return 0;
}

static void mtcsPassLoopDoneInit(MtcsPassLoopDone *self)
{
   MtcsPass *mtcsPass= (MtcsPass *)self;
   mtcsPass->execute =loop_done_execute_cb;
   mtcs_pass_set_properties(mtcsPass,
      0,/* properties_required */
      0, /* properties_provided */
      PROP_loops /* properties_destroyed */);
   mtcs_pass_set_todo_flags(mtcsPass,
      0, /* todo_flags_start */
      0 /*todo_flags_finish */);
}

MtcsPassLoopDone *mtcs_pass_loop_done_new(MtcsMode *mtcsMode)
{
   MtcsPassLoopDone *self = n_slice_alloc0 (sizeof(MtcsPassLoopDone));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"loop2_done");
   mtcsPassLoopDoneInit(self);
   return self;
}
