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

#include "mtcsiraobject.h"
#include "mtcsira.h"
#include "mtcsiraint.h"
#include "../mtcstarget.h"

/**
 * 创建 Object时target已经完成了各个组件的创建，所以在初始化的地方可以引用组件。
 */
static void mtcsIraLoopTreeNodeInit(MtcsIraLoopTreeNode *self)
{
   self->regno_allocno_map = NULL;
   memset (self->reg_pressure, 0,sizeof (self->reg_pressure));
   self->all_allocnos = NULL;
   self->modified_regnos = NULL;
   self->border_allocnos = NULL;
   self->local_copies = NULL;
}

/* Free the loop tree node of a loop.  */
//原型 finish_loop_tree_node ira-build.cc
void mtcs_ira_loop_tree_node_free(MtcsIraLoopTreeNode *self)
{
   if (self->local_copies != NULL)
      ira_free_bitmap (self->local_copies);
   if (self->border_allocnos != NULL)
      ira_free_bitmap (self->border_allocnos);
   if (self->modified_regnos != NULL)
      ira_free_bitmap (self->modified_regnos);
   if (self->all_allocnos != NULL)
      ira_free_bitmap (self->all_allocnos);
   if (self->regno_allocno_map != NULL)
      ira_free (self->regno_allocno_map);

   n_slice_free(MtcsIraLoopTreeNode,self);
}

/* Return TRUE if NODE is inside PARENT.  */
//原型 static bool loop_is_inside_p (MtcsIraLoopTreeNode * node, MtcsIraLoopTreeNode * parent) ira-build.cc
bool mtcs_ira_loop_tree_node_loop_is_inside_p (MtcsIraLoopTreeNode * self, MtcsIraLoopTreeNode * parent)
{
   for (self = self->parent; self != NULL; self = self->parent)
      if (self == parent)
         return true;
   return false;
}


MtcsIraLoopTreeNode *mtcs_ira_loop_tree_node_new(MtcsMode *mtcsMode)
{
   MtcsIraLoopTreeNode *self = n_slice_alloc0 (sizeof(MtcsIraLoopTreeNode));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsIraLoopTreeNodeInit(self);
   return self;
}
