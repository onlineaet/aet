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

#ifndef __GCC_TARGET_ADDR_SPACE__
#define __GCC_TARGET_ADDR_SPACE__

#include "../../nlib.h"
#include "machinetarget.h"

typedef struct _TargetAddrSpace TargetAddrSpace;
struct _TargetAddrSpace
{
   MachineTarget parent;
   //原型 targetm.addr_space.legitimate_address_p (mode, addr, 0, as, ch);#define TARGET_LEGITIMATE_ADDRESS_P nvptx_legitimate_address_p
   bool (*legitimate_address_p)(TargetAddrSpace *self,mtcs_mode mode ,rtx addr ,bool strict ,addr_space_t as, code_helper ch);
   //原型 targetm.addr_space.address_mode*/(MEM_ADDR_SPACE (mem));#define TARGET_ADDR_SPACE_ADDRESS_MODE default_addr_space_address_mode
   scalar_int_mode (*address_mode)(TargetAddrSpace *self,addr_space_t addrspace ATTRIBUTE_UNUSED);
   //原型 targetm.addr_space.legitimize_address #define TARGET_LEGITIMIZE_ADDRESS default_legitimize_address
   //target也有,但没有参数addr_space_t as
   rtx (*legitimize_address)(TargetAddrSpace *self,rtx x, rtx orig_x ATTRIBUTE_UNUSED,mtcs_mode mode ATTRIBUTE_UNUSED,addr_space_t as ATTRIBUTE_UNUSED);
   //原型 targetm.addr_space.pointer_mode (MEM_ADDR_SPACE (target)) #define TARGET_ADDR_SPACE_POINTER_MODE default_addr_space_pointer_mode
   scalar_int_mode (*pointer_mode)(TargetAddrSpace *self,addr_space_t addrspace ATTRIBUTE_UNUSED);
   //原型 targetm.addr_space.subset_p (as_from, as_to) #define TARGET_ADDR_SPACE_SUBSET_P default_addr_space_subset_p
   bool (*subset_p)(TargetAddrSpace *self,addr_space_t subset,addr_space_t superset);
   //原型 targetm.addr_space.convert (op0, treeop0_type, type) #define TARGET_ADDR_SPACE_CONVERT default_addr_space_convert
   rtx (*convert)(TargetAddrSpace *self,rtx op ATTRIBUTE_UNUSED,tree from_type ATTRIBUTE_UNUSED,tree to_type ATTRIBUTE_UNUSED);
   //原型targetm.addr_space.valid_pointer_mode#define TARGET_ADDR_SPACE_VALID_POINTER_MODE default_addr_space_valid_pointer_mode
   bool (*valid_pointer_mode)(TargetAddrSpace *self,scalar_int_mode mode,addr_space_t as ATTRIBUTE_UNUSED);
   //原型  targetm.addr_space.zero_address_valid #define TARGET_ADDR_SPACE_ZERO_ADDRESS_VALID default_addr_space_zero_address_valid
   bool (*zero_address_valid)(TargetAddrSpace *self,addr_space_t as ATTRIBUTE_UNUSED);
};

void target_addr_space_init(TargetAddrSpace *self);
//原型 targetm.addr_space.legitimate_address_p (mode, addr, 0, as, ch);#define TARGET_LEGITIMATE_ADDRESS_P nvptx_legitimate_address_p
bool target_addr_space_legitimate_address_p(TargetAddrSpace *self,mtcs_mode mode ,rtx addr ,bool strict ,addr_space_t as, code_helper ch);
//原型 targetm.addr_space.address_mode*/(MEM_ADDR_SPACE (mem));#define TARGET_ADDR_SPACE_ADDRESS_MODE default_addr_space_address_mode
scalar_int_mode target_addr_space_address_mode(TargetAddrSpace *self,addr_space_t addrspace ATTRIBUTE_UNUSED);
//原型 targetm.addr_space.legitimize_address #define TARGET_LEGITIMIZE_ADDRESS default_legitimize_address
//target也有,但没有参数addr_space_t as
rtx target_addr_space_legitimize_address(TargetAddrSpace *self,rtx x, rtx orig_x ATTRIBUTE_UNUSED,mtcs_mode mode ATTRIBUTE_UNUSED,addr_space_t as ATTRIBUTE_UNUSED);
//原型 targetm.addr_space.pointer_mode (MEM_ADDR_SPACE (target)) #define TARGET_ADDR_SPACE_POINTER_MODE default_addr_space_pointer_mode
scalar_int_mode target_addr_space_pointer_mode(TargetAddrSpace *self,addr_space_t addrspace ATTRIBUTE_UNUSED);
//原型 targetm.addr_space.subset_p (as_from, as_to) #define TARGET_ADDR_SPACE_SUBSET_P default_addr_space_subset_p
bool target_addr_space_subset_p(TargetAddrSpace *self,addr_space_t subset,addr_space_t superset);
//原型 targetm.addr_space.convert (op0, treeop0_type, type) #define TARGET_ADDR_SPACE_CONVERT default_addr_space_convert
rtx target_addr_space_convert(TargetAddrSpace *self,rtx op ATTRIBUTE_UNUSED,tree from_type ATTRIBUTE_UNUSED,tree to_type ATTRIBUTE_UNUSED);
//原型targetm.addr_space.valid_pointer_mode#define TARGET_ADDR_SPACE_VALID_POINTER_MODE default_addr_space_valid_pointer_mode
bool target_addr_space_valid_pointer_mode(TargetAddrSpace *self,scalar_int_mode mode,addr_space_t as ATTRIBUTE_UNUSED);
//原型  targetm.addr_space.zero_address_valid #define TARGET_ADDR_SPACE_ZERO_ADDRESS_VALID default_addr_space_zero_address_valid
bool target_addr_space_zero_address_valid(TargetAddrSpace *self,addr_space_t as ATTRIBUTE_UNUSED);



#endif

