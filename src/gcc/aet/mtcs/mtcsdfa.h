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

#ifndef __GCC_MTCS_DFA__
#define __GCC_MTCS_DFA__

#include "../nlib.h"
#include "mtcscomponent.h"

/*
 * 原型 fold-dfa.h fold-dfa.cc
 */
typedef struct _MtcsDfa MtcsDfa;
struct _MtcsDfa
{
    MtcsComponent parent;


};

MtcsDfa *mtcs_dfa_new(MtcsMode *mtcsMode);
//原型 get_addr_base_and_unit_offset tree-dfa.h tree-dfa.cc
tree mtcs_dfa_get_addr_base_and_unit_offset (MtcsDfa *self,tree exp, poly_int64 *poffset);
//原型 get_addr_base_and_unit_offset_1 tree-dfa.h tree-dfa.cc
tree mtcs_dfa_get_addr_base_and_unit_offset_1 (MtcsDfa *self,tree exp, poly_int64 *poffset,
             tree (*valueize) (tree));
#endif

