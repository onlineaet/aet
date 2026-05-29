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

#ifndef __GCC_MTCS_IRA_CONFLICTS__
#define __GCC_MTCS_IRA_CONFLICTS__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "mtcsiraobject.h"


typedef struct _MtcsIraConflicts MtcsIraConflicts;
struct _MtcsIraConflicts
{
   MtcsComponent parent;

   /* This file contains code responsible for allocno conflict creation,
      allocno copy creation and allocno info accumulation on upper level
      regions.  */

   /* ira_allocnos_num array of arrays of bits, recording whether two
      allocno's conflict (can't go in the same hardware register).

      Some arrays will be used as conflict bit vector of the
      corresponding allocnos see function build_object_conflicts.  */
   //原型 conflicts ira-conflicts.cc
   IRA_INT_TYPE **conflicts;

   /* Array used to collect all conflict allocnos for given allocno.  */
   MtcsIraObject **collected_conflict_objects;
};

MtcsIraConflicts *mtcs_ira_conflictst_new(MtcsMode *mtcsMode);

//原型 ira_debug_conflicts ira-int.h ira-conflicts.cc
void mtcs_ira_conflicts_debug_conflicts (MtcsIraConflicts *self,bool reg_p);
//原型 ira_build_conflicts ira-int.h ira-conflicts.cc
void mtcs_ira_conflicts_build_conflicts (MtcsIraConflicts *self);

#endif
