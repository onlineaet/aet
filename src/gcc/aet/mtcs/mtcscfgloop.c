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
 * base on cfgloop.cc
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
#include "dumpfile.h"

#include "mtcscfgloop.h"
#include "mtcstarget.h"

static void mtcsCfgLooInit(MtcsCfgLoop *self)
{

}

/* Return location corresponding to the loop control condition if possible.  */
//原型 get_loop_location cfgloop.h cfgloop.cc
dump_user_location_t mtcs_cfg_loop_get_loop_location (MtcsCfgLoop *self,class loop *loop)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsLoopIv *mtcsLoopIv =mtcs_target_get_loop_iv(mtcsTarget);

   rtx_insn *insn = NULL;
   class niter_desc *desc = NULL;
   edge exit;
   n_debug("mtcscfgloop.c get_loop_location 00 loop:%p\n",loop);
   /* For a for or while loop, we would like to return the location
   of the for or while statement, if possible.  To do this, look
   for the branch guarding the loop back-edge.  */

   /* If this is a simple loop with an in_edge, then the loop control
   branch is typically at the end of its source.  */
   desc = mtcs_loop_iv_get_simple_loop_desc/*!get_simple_loop_desc*/(mtcsLoopIv,loop);
   if (desc->in_edge){
      FOR_BB_INSNS_REVERSE (desc->in_edge->src, insn){
         if (INSN_P (insn) && INSN_HAS_LOCATION (insn))
            return insn;
      }
   }

   n_debug("mtcscfgloop.c get_loop_location 11 loop:%p desc:%p\n",loop,desc);

   /* If loop has a single exit, then the loop control branch
   must be at the end of its source.  */
   if ((exit = single_exit (loop))){
      FOR_BB_INSNS_REVERSE (exit->src, insn){
         if (INSN_P (insn) && INSN_HAS_LOCATION (insn))
            return insn;
      }
   }

   n_debug("mtcscfgloop.c get_loop_location 22 loop:%p desc:%p\n",loop,desc);

   /* Next check the latch, to see if it is non-empty.  */
   FOR_BB_INSNS_REVERSE (loop->latch, insn){
      if (INSN_P (insn) && INSN_HAS_LOCATION (insn))
         return insn;
   }

   n_debug("mtcscfgloop.c get_loop_location 33 loop:%p desc:%p\n",loop,desc);

   /* Finally, if none of the above identifies the loop control branch,
   return the first location in the loop header.  */
   FOR_BB_INSNS (loop->header, insn){
      if (INSN_P (insn) && INSN_HAS_LOCATION (insn))
         return insn;
   }
   /* If all else fails, simply return the current function location.  */
   return dump_user_location_t::from_function_decl (current_function_decl);
}

MtcsCfgLoop *mtcs_cfg_loop_new(MtcsMode *mtcsMode)
{
   MtcsCfgLoop *self = n_slice_alloc0 (sizeof(MtcsCfgLoop));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsCfgLooInit(self);
   return self;
}

