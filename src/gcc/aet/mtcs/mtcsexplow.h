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


#ifndef __GCC_MTCS_EXPLOW__
#define __GCC_MTCS_EXPLOW__

#include "../nlib.h"
#include "mtcscomponent.h"


typedef struct _MtcsExplow MtcsExplow;


struct _MtcsExplow
{
   MtcsComponent parent;
   bool suppress_reg_args_size;
   GTY(()) rtx stack_check_libfunc;//原型 explow.cc

};


MtcsExplow *mtcs_explow_new(MtcsMode *mtcsMode);
//原型 force_reg explow.h explow.cc
rtx mtcs_explow_force_reg (MtcsExplow *self,machine_mode mode, rtx x);
//原型 copy_to_reg explow.h explow.cc
rtx mtcs_explow_copy_to_reg (MtcsExplow *self,rtx x);
//原型 memory_address_addr_space explow.h explow.cc
rtx mtcs_explow_memory_address_addr_space (MtcsExplow *self,machine_mode mode, rtx x, addr_space_t as);
//原型 convert_memory_address_addr_space rtl.h explow.cc
rtx  mtcs_explow_convert_memory_address_addr_space (MtcsExplow *self,scalar_int_mode to_mode, rtx x,addr_space_t as);

//原型 #define convert_memory_address(to_mode,x)  convert_memory_address_addr_space ((to_mode), (x), ADDR_SPACE_GENERIC) rtl.h
inline rtx  mtcs_explow_convert_memory_address(MtcsExplow *self,scalar_int_mode to_mode, rtx x)
{
    return mtcs_explow_convert_memory_address_addr_space(self,to_mode,x,ADDR_SPACE_GENERIC);
}
/* Like memory_address_addr_space, except assume the memory address points to
   the generic named address space.  */
//原型 #define memory_address(MODE,RTX)    memory_address_addr_space ((MODE), (RTX), ADDR_SPACE_GENERIC) explow.h
rtx mtcs_explow_memory_address(MtcsExplow *self,machine_mode mode, rtx x);

//原型 rtl.h explow.cc
//POINTERS_EXTEND_UNSIGNED host=1 nvptx=0 gen=? spirv=? 实现gen或spirv时重做
rtx mtcs_explow_convert_memory_address_addr_space_1 (MtcsExplow *self,scalar_int_mode to_mode ATTRIBUTE_UNUSED,
                     rtx x, addr_space_t as ATTRIBUTE_UNUSED, bool in_const ATTRIBUTE_UNUSED,bool no_emit ATTRIBUTE_UNUSED);

//原型explow.h explow.cc
rtx mtcs_explow_eliminate_constant_term (MtcsExplow *self,rtx x, rtx *constptr);
//原型 copy_to_mode_reg explow.h explow.cc
rtx mtcs_explow_copy_to_mode_reg ( MtcsExplow *self,machine_mode mode, rtx x);
//原型 hard_libcall_value explow.h explow.cc
rtx mtcs_explow_hard_libcall_value (MtcsExplow *self,machine_mode mode, rtx fun);

//原型 validize_mem explow.h explow.cc
rtx mtcs_explow_validize_mem (MtcsExplow *self,rtx ref);
//原型 rtx use_anchored_address (rtx x) explow.h explow.cc
rtx mtcs_explow_use_anchored_address (MtcsExplow *self,rtx x);

//原型 adjust_stack explow.h explow.cc
void mtcs_explow_adjust_stack (MtcsExplow *self,rtx adjust);
//原型 copy_addr_to_reg explow.h explow.cc
rtx mtcs_explow_copy_addr_to_reg (MtcsExplow *self,rtx x);
//原型 anti_adjust_stack exprlow.h exprlow.cc
void mtcs_explow_anti_adjust_stack (MtcsExplow *self,rtx adjust);
//原型 force_not_mem explow.h explow.cc
rtx mtcs_explow_force_not_mem (MtcsExplow *self,rtx x);
//原型 emit_stack_save explow.h explow.cc
void mtcs_explow_emit_stack_save (MtcsExplow *self,enum save_level save_level, rtx *psave);
//原型 allocate_dynamic_stack_space explow.h explow.cc
rtx mtcs_explow_allocate_dynamic_stack_space(MtcsExplow *self,rtx size, unsigned size_align,
                  unsigned required_align,HOST_WIDE_INT max_size,bool cannot_accumulate);
//原型 get_dynamic_stack_size explow.h explow.cc
void mtcs_explow_get_dynamic_stack_size (MtcsExplow *self,rtx *psize, unsigned size_align,
            unsigned required_align,HOST_WIDE_INT *pstack_usage_size);
//原型 probe_stack_range explow.h explow.cc
void mtcs_explow_probe_stack_range (MtcsExplow *self,HOST_WIDE_INT first, rtx size);
//原型 emit_stack_probe explow.h explow.cc
void mtcs_explow_emit_stack_probe (MtcsExplow *self,rtx address);
//原型 get_stack_check_protect rtl.h explow.cc
HOST_WIDE_INT mtcs_explow_get_stack_check_protect (MtcsExplow *self);
//原型 align_dynamic_address explow.h explow.cc
rtx mtcs_explow_align_dynamic_address (MtcsExplow *self,rtx target, unsigned required_align);
//原型 record_new_stack_level explow.h explow.cc
void mtcs_explow_record_new_stack_level (MtcsExplow *self);
//原型 emit_stack_restore explow.h explow.cc
void mtcs_explow_emit_stack_restore (MtcsExplow *self,enum save_level save_level, rtx sa);
//原型 update_nonlocal_goto_save_area explow.h explow.cc
void mtcs_explow_update_nonlocal_goto_save_area (MtcsExplow *sef);
//原型 anti_adjust_stack_and_probe explow.h explow.cc
void mtcs_explow_anti_adjust_stack_and_probe (MtcsExplow *self,rtx size, bool adjust_back);

#endif
