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

#ifndef __GCC_TARGET_COMMON__
#define __GCC_TARGET_COMMON__

#include "../../nlib.h"
#include "machinetarget.h"
#include "../mtcsoptionsitem.h"

typedef struct _TargetCommon TargetCommon;
struct _TargetCommon
{
    MachineTarget parent;
    //原型 targetm_common.default_target_flags; #define TARGET_DEFAULT_TARGET_FLAGS 0
    int default_target_flags;
    //原型 targetm_common.unwind_tables_default; #define TARGET_UNWIND_TABLES_DEFAULT false
    bool unwind_tables_default;
    //原型 targetm_common.have_named_sections
    bool have_named_sections;
    //原型 targetm_common.option_optimization_table, #define TARGET_OPTION_OPTIMIZATION_TABLE empty_optimization_table
    const struct default_options *empty_optimization_table;
    //原型 targetm_common.option_init_struct (opts); #define TARGET_OPTION_INIT_STRUCT hook_void_gcc_optionsp
    void (*option_init_struct)(TargetCommon *self,MtcsOptionsItem *opts);
    //原型 targetm_common.supports_split_stack (true, opts)#define TARGET_SUPPORTS_SPLIT_STACK hook_bool_bool_gcc_optionsp_false
    bool (*supports_split_stack)(TargetCommon *self,bool split, MtcsOptionsItem *opts);
    //原型 targetm_common.except_unwind_info 和#define TARGET_EXCEPT_UNWIND_INFO default_except_unwind_info
    enum unwind_info_type (*except_unwind_info )(TargetCommon *self,MtcsOptionsItem *opts ATTRIBUTE_UNUSED);
    //原型 targetm_common.handle_option (opts, opts_set, decoded, loc);#define TARGET_HANDLE_OPTION default_target_handle_option
    bool (*handle_option)(TargetCommon *self,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,
            const struct cl_decoded_option *decoded ATTRIBUTE_UNUSED,location_t loc ATTRIBUTE_UNUSED);
};

void  target_common_init(TargetCommon *self);
//原型 targetm_common.option_init_struct (opts); #define TARGET_OPTION_INIT_STRUCT hook_void_gcc_optionsp
void  target_common_option_init_struct (TargetCommon *self,MtcsOptionsItem *opts);
//原型 targetm_common.supports_split_stack (true, opts)#define TARGET_SUPPORTS_SPLIT_STACK hook_bool_bool_gcc_optionsp_false
bool  target_common_supports_split_stack (TargetCommon *self,bool split, MtcsOptionsItem *opts);
//原型 targetm_common.except_unwind_info 和#define TARGET_EXCEPT_UNWIND_INFO default_except_unwind_info
enum unwind_info_type target_common_except_unwind_info (TargetCommon *self,MtcsOptionsItem *opts ATTRIBUTE_UNUSED);
//原型 targetm_common.handle_option (opts, opts_set, decoded, loc);#define TARGET_HANDLE_OPTION default_target_handle_option
bool  target_common_handle_option (TargetCommon *self,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,
        const struct cl_decoded_option *decoded ATTRIBUTE_UNUSED,location_t loc ATTRIBUTE_UNUSED);



#endif

