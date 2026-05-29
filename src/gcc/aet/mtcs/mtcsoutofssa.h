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


#ifndef __GCC_MTCS_OUTOF_SSA__
#define __GCC_MTCS_OUTOF_SSA__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcsmicro.h"


typedef struct _MtcsOutofSsa MtcsOutofSsa;
struct _MtcsOutofSsa
{
    MtcsComponent parent;
};

MtcsOutofSsa *mtcs_outof_ssa_new(MtcsMode *mtcsMode);
//原型 ssa_is_replaceable_p tree-outof-ssa.h tree-outof-ssa.cc
bool mtcs_outof_ssa_ssa_is_replaceable_p (MtcsOutofSsa *self,gimple *stmt);
//原型 rewrite_out_of_ssa tree-outof-ssa.h tree-outof-ssa.cc
unsigned int mtcs_outof_ssa_rewrite_out_of_ssa (MtcsOutofSsa *self,struct ssaexpand *sa);
//原型 finish_out_of_ssa tree-outof-ssa.h tree-outof-ssa.cc
void mtcs_outof_ssa_finish_out_of_ssa (MtcsOutofSsa *self,struct ssaexpand *sa);
//原型 expand_phi_nodes tree-outof-ssa.h tree-outof-ssa.cc
void mtcs_outof_ssa_expand_phi_nodes (MtcsOutofSsa *self,struct ssaexpand *sa);

#endif

