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

#ifndef __GCC_MTCS_RELOAD1__
#define __GCC_MTCS_RELOAD1__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcsmicro.h"

typedef struct _MtcsReload1 MtcsReload1;
struct _MtcsReload1
{
  MtcsComponent parent;
};



MtcsReload1 *mtcs_reload1_new(MtcsMode *mtcsMode);
//原型 grow_reg_equivs reload.h reload1.cc
void mtcs_reload1_grow_reg_equivs (MtcsReload1 *self);
//原型 calculate_elim_costs_all_insns reload.h reload1.cc
//void mtcs_reload1_calculate_elim_costs_all_insns (MtcsReload1 *self);
//原型 compute_use_by_pseudos reload.h reload1.cc
void mtcs_reload1_compute_use_by_pseudos (MtcsReload1 *self,HardRegSet *to, regset from);
//原型 function_invariant_p rtl.h reload1.cc
bool  mtcs_reload1_function_invariant_p (MtcsReload1 *self,const_rtx x);

#endif
