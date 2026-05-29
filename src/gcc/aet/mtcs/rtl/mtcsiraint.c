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
 * base on ira-int.h
 */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "ira.h"
#include "ira-int.h"
#include "diagnostic-core.h"
#include "cfgrtl.h"
#include "cfgbuild.h"
#include "cfgcleanup.h"
#include "expr.h"
#include "tree-pass.h"
#include "output.h"
#include "reload.h"
#include "cfgloop.h"
#include "lra.h"
#include "dce.h"
#include "dbgcnt.h"
#include "rtl-iter.h"
#include "shrink-wrap.h"
#include "print-rtl.h"


#include "../mtcstarget.h"
#include "mtcsiraint.h"
#include "mtcsira.h"

static void mtcsIraIntInit(MtcsIraInt *self)
{

}


/* Free ira_max_register_move_cost, ira_may_move_in_cost and
   mtcsIraInt->x_ira_may_move_out_cost for each mode.  */
//原型 free_register_move_costs ira-int.h ira.cc
void mtcs_ira_int_free_register_move_costs (MtcsIraInt *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int mode, i;
   /* Reset move_cost and friends, making sure we only free shared
   table entries once.  */
   for (mode = 0; mode < mtcs_mode_get_max_number/*!MAX_MACHINE_MODE*/(mtcsMode); mode++)
      if (self->x_ira_register_move_cost[mode]){
         for (i = 0; i < mode && (self->x_ira_register_move_cost[i] != self->x_ira_register_move_cost[mode]); i++)
            ;
         if (i == mode){
            free (self->x_ira_register_move_cost[mode]);
            free (self->x_ira_may_move_in_cost[mode]);
            free (self->x_ira_may_move_out_cost[mode]);
         }
      }
   memset (self->x_ira_register_move_cost, 0, sizeof self->x_ira_register_move_cost);
   memset (self->x_ira_may_move_in_cost, 0, sizeof self->x_ira_may_move_in_cost);
   memset (self->x_ira_may_move_out_cost, 0, sizeof self->x_ira_may_move_out_cost);
   self->x_last_mode_for_init_move_cost/*!last_mode_for_init_move_cost定义在ira.cc的宏*/ = -1;
}

/* Initialize register costs for MODE if necessary.  */
//原型 ira_init_register_move_cost_if_necessary ira-int.h
void mtcs_ira_int_init_register_move_cost_if_necessary (MtcsIraInt *self,machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   if (self->x_ira_register_move_cost[mode] == NULL)
      mtcs_ira_init_register_move_cost/*!ira_init_register_move_cost*/(mtcsIra,mode);
}

// 原型 target_ira_int::~target_ira_int   ira-int.h ira.cc
void mtcs_ira_int_free (MtcsIraInt *self)
{
   mtcs_ira_int_free_ira_costs/*!free_ira_costs*/(self);
   mtcs_ira_int_free_register_move_costs/*!free_register_move_costs*/(self);
}

/* Free allocated temporary cost vectors.  */
//原型 void target_ira_int::free_ira_costs () ira-int.h ira-costs.cc
void mtcs_ira_int_free_ira_costs (MtcsIraInt *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   int i;

   free (self->x_init_cost);
   self->x_init_cost = NULL;
   for (i = 0; i < mtcs_recog_get_max_recog_operands/*!MAX_RECOG_OPERANDS*/(mtcsRecog); i++){
      free (self->x_op_costs[i]);
      free (self->x_this_op_costs[i]);
      self->x_op_costs[i] = self->x_this_op_costs[i] = NULL;
   }
   free (self->x_temp_costs);
   self->x_temp_costs = NULL;
}


MtcsIraInt *mtcs_ira_int_new(MtcsMode *mtcsMode)
{
   MtcsIraInt *self = n_slice_alloc0 (sizeof(MtcsIraInt));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsIraIntInit(self);
   return self;
}
