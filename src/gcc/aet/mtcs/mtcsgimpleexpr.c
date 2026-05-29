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
#include "tree.h"
#include "gimple.h"
#include "stringpool.h"
#include "gimple-ssa.h"
#include "fold-const.h"
#include "tree-eh.h"
#include "gimplify.h"
#include "stor-layout.h"
#include "demangle.h"
#include "hash-set.h"
#include "rtl.h"
#include "tree-pass.h"
#include "stringpool.h"
#include "attribs.h"
#include "target.h"

#include "mtcstarget.h"
#include "mtcsgimpleexpr.h"


/* Return true if T (assumed to be a DECL) must be assigned a memory
   location.  */
//原型 needs_to_live_in_memory tree.h tree.cc
bool mtcs_gimple_expr_needs_to_live_in_memory (MtcsGimpleExpr *self,const_tree t)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   return (TREE_ADDRESSABLE (t)
      || is_global_var (t)
      || (TREE_CODE (t) == RESULT_DECL
      && !DECL_BY_REFERENCE (t)
      && mtcs_func_aggregate_value_p/*!aggregate_value_p*/(mtcsFunc,t, current_function_decl)));
}

/* Return true if T is a non-aggregate register variable.  */
//原型 is_gimple_reg gimple.h gimple-expr.cc
bool mtcs_gimple_expr_is_gimple_reg (MtcsGimpleExpr *self,tree t)
{
   if (virtual_operand_p (t))
      return false;

   if (TREE_CODE (t) == SSA_NAME)
      return true;

   if (!is_gimple_variable (t))
      return false;

   if (!is_gimple_reg_type (TREE_TYPE (t)))
      return false;

   /* A volatile decl is not acceptable because we can't reuse it as
   needed.  We need to copy it into a temp first.  */
   if (TREE_THIS_VOLATILE (t))
      return false;

   /* We define "registers" as things that can be renamed as needed,
   which with our infrastructure does not apply to memory.  */
   if (mtcs_gimple_expr_needs_to_live_in_memory/*!needs_to_live_in_memory*/(self,t))
      return false;

   /* Hard register variables are an interesting case.  For those that
   are call-clobbered, we don't know where all the calls are, since
   we don't (want to) take into account which operations will turn
   into libcalls at the rtl level.  For those that are call-saved,
   we don't currently model the fact that calls may in fact change
   global hard registers, nor do we examine ASM_CLOBBERS at the tree
   level, and so miss variable changes that might imply.  All around,
   it seems safest to not do too much optimization with these at the
   tree level at all.  We'll have to rely on the rtl optimizers to
   clean this up, as there we've got all the appropriate bits exposed.  */
   if (VAR_P (t) && DECL_HARD_REGISTER (t))
      return false;

   /* Variables can be marked as having partial definitions, avoid
   putting them into SSA form.  */
   return !DECL_NOT_GIMPLE_REG_P (t);
}

static void mtcsGimpleExprInit(MtcsGimpleExpr *self)
{

}


MtcsGimpleExpr    *mtcs_gimple_expr_new(MtcsMode *mtcsMode)
{
   MtcsGimpleExpr *self = n_slice_alloc0 (sizeof(MtcsGimpleExpr));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsGimpleExprInit(self);
   return self;
}
