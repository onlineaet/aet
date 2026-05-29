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
#include "predict.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "ira.h"
#include "ira-int.h"
#include "sparseset.h"
#include "addresses.h"

#include "mtcsiracommon.h"
#include "../mtcstarget.h"
#include "../mtcscompile.h"
#include "mtcsira.h"
#include "mtcsiracosts.h"
#include "mtcsiraint.h"

static void mtcsIraMgrInit(MtcsIraMgr *self)
{
   self->mtcsIra = NULL;
   self->mtcsIraInt = NULL;
   self->mtcsIraBuild = NULL;
   self->mtcsIraConflicts = NULL;
   self->mtcsIraLives = NULL;
   self->mtcsIraEmit = NULL;
   self->mtcsIraCosts = NULL;
   self->mtcsIraColor = NULL;
   self->init = FALSE;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   self->mtcsIra =(void*)mtcs_ira_new(mtcsMode);
   self->mtcsIraCosts =(void*)mtcs_ira_costs_new(mtcsMode);
   self->mtcsIraInt =(void*)mtcs_ira_int_new(mtcsMode);

}


MtcsIraMgr *mtcs_ira_mgr_new(MtcsMode *mtcsMode)
{
   MtcsIraMgr *self = n_slice_alloc0 (sizeof(MtcsIraMgr));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsIraMgrInit(self);
   return self;
}

MtcsIraGlobal *mtcs_ira_mgr_get_global(MtcsIraMgr *self)
{
   return self->mtcsIraGlobal;
}

#define IRA_OBSTACK \
   mtcs_target_get_ira_mgr(mtcs_compile_get_current_target(mtcs_compile_get()))->mtcsIraGlobal->ira_obstack

#define IRA_BITMAP_OBSTACK \
   mtcs_target_get_ira_mgr(mtcs_compile_get_current_target(mtcs_compile_get()))->mtcsIraGlobal->ira_bitmap_obstack

/* Allocate memory of size LEN for IRA data.  */
//原型 ira_allocate ira-int.h ira.cc
void *mtcs_ira_allocate (size_t len)
{
  MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
  MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
  void *res;
  res = obstack_alloc (&IRA_OBSTACK, len);
  return res;
}

/* Free memory ADDR allocated for IRA data.  */
//原型 ira_ira_free ira-int.h ira.cc
void mtcs_ira_free (void *addr ATTRIBUTE_UNUSED)
{
  free (addr);
}


/* Allocate and returns bitmap for IRA.  */
//原型 ira_allocate_bitmap ira-int.h ira.cc
bitmap mtcs_ira_allocate_bitmap ()
{
  return BITMAP_ALLOC (&IRA_BITMAP_OBSTACK);
}

/* Free bitmap B allocated for IRA.  */
//原型 ira_free_bitmap ira-int.h ira.cc
void mtcs_ira_free_bitmap (bitmap b ATTRIBUTE_UNUSED)
{
  /* do nothing */
}
