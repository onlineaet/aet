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

#ifndef __GCC_MTCS_SSA_COALESCE__
#define __GCC_MTCS_SSA_COALESCE__

#include "../nlib.h"
#include "mtcscomponent.h"

struct ssa_conflicts;

typedef struct _MtcsSsaCoalesce  MtcsSsaCoalesce;
struct _MtcsSsaCoalesce
{
   MtcsComponent parent;
   /* The narrow API of the qsort comparison function doesn't allow easy
      access to additional arguments.  So we have two globals (ick) to hold
      the data we need.  They're initialized before the call to qsort and
      wiped immediately after.  */
   ssa_conflicts *conflicts_;
   var_map map_;
};


MtcsSsaCoalesce *mtcs_ssa_coalesce_new(MtcsMode *mtcsMode);
void mtcs_ssa_coalesce_coalesce_ssa_name (MtcsSsaCoalesce *self,var_map map);
//原型 gimple_can_coalesce_p tree-ssa-coalesce.h tree-ssa-coalesce.cc
bool mtcs_ssa_coalesce_gimple_can_coalesce_p (MtcsSsaCoalesce *self,tree name1, tree name2);


#endif
