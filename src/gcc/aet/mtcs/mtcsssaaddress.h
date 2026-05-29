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

#ifndef __GCC_MTCS_SSA_ADDRESS__
#define __GCC_MTCS_SSA_ADDRESS__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "tree-affine.h"


typedef struct _MtcsSsaAddress  MtcsSsaAddress;
struct _MtcsSsaAddress
{
   MtcsComponent parent;
};

MtcsSsaAddress *mtcs_ssa_address_new(MtcsMode *mtcsMode);
//原型 addr_for_mem_ref tree-ssa-address.h tree-ssa-address.cc
//重载函数
rtx mtcs_ssa_address_addr_for_mem_ref (MtcsSsaAddress *self,struct mem_address *addr, addr_space_t as,
        bool really_expand);
//原型 addr_for_mem_ref tree-ssa-address.h tree-ssa-address.cc
//重载函数
rtx mtcs_ssa_address_addr_for_mem_ref (MtcsSsaAddress *self,tree exp, addr_space_t as, bool really_expand);
//原型 valid_mem_ref_p tree-ssa-address.h tree-ssa-address.cc
bool mtcs_ssa_address_valid_mem_ref_p (MtcsSsaAddress *self,machine_mode mode, addr_space_t as,
       struct mem_address *addr, code_helper = ERROR_MARK);
//原型 move_fixed_address_to_symbol tree-ssa-address.h tree-ssa-address.cc
void mtcs_ssa_address_move_fixed_address_to_symbol (MtcsSsaAddress *self,struct mem_address *parts, aff_tree *addr);
//原型 create_mem_ref tree-ssa-address.h tree-ssa-address.cc
tree mtcs_ssa_address_create_mem_ref (MtcsSsaAddress *self, gimple_stmt_iterator *gsi, tree type, aff_tree *addr,
      tree alias_ptr_type, tree iv_cand, tree base_hint, bool speed);
//原型 get_address_description tree-ssa-address.h tree-ssa-address.cc
void mtcs_ssa_address_get_address_description (MtcsSsaAddress *self,tree op, struct mem_address *addr);
//原型 copy_ref_info tree-ssa-address.h tree-ssa-address.cc
void mtcs_ssa_address_copy_ref_info (MtcsSsaAddress *self, tree new_ref, tree old_ref);
//原型 maybe_fold_tmr tree-ssa-address.h tree-ssa-address.cc
tree  mtcs_ssa_address_maybe_fold_tmr (MtcsSsaAddress *self,tree ref);
//原型 preferred_mem_scale_factor tree-ssa-address.h tree-ssa-address.cc
unsigned int mtcs_ssa_address_preferred_mem_scale_factor (MtcsSsaAddress *self,tree base, machine_mode mem_mode,
             bool speed);
//原型 dump_mem_address tree-ssa-address.cc
void mtcs_ssa_address_dump_mem_address (FILE *file, struct mem_address *parts);
#endif
