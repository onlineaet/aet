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

#ifndef __GCC_TARGET_OPTION__
#define __GCC_TARGET_OPTION__

#include "../../nlib.h"
#include "machinetarget.h"

typedef struct _TargetOption TargetOption;
struct _TargetOption
{
    MachineTarget parent;
   //原型 targetm.target_option.override (); #define TARGET_OPTION_OVERRIDE nvptx_option_override
    void (*override)(TargetOption *self);
    //原型  targetm.target_option.relayout_function (fndecl); #define TARGET_RELAYOUT_FUNCTION hook_void_tree
    void (*relayout_function)(TargetOption *self,tree fndecl);
    //原型 targetm.target_option.need_ipa_fn_target_info (node->decl,info->target_info);#define TARGET_NEED_IPA_FN_TARGET_INFO default_need_ipa_fn_target_info
    bool (*need_ipa_fn_target_info)(TargetOption *self,const_tree fndecl,unsigned int &a);
    //原型 targetm.target_option.update_ipa_fn_target_info(info->target_info, stmt);#define TARGET_UPDATE_IPA_FN_TARGET_INFO default_update_ipa_fn_target_info
    bool (*update_ipa_fn_target_info)(TargetOption *self,unsigned int &a, const gimple *gmp);
    //原型 targetm.target_option.can_inline_p (caller->decl, callee->decl) #define TARGET_CAN_INLINE_P default_target_can_inline_p
    bool (*can_inline_p)(TargetOption *self,tree caller, tree callee);
    //原型  targetm.target_option.valid_attribute_p  #define TARGET_OPTION_VALID_ATTRIBUTE_P default_target_option_valid_attribute_p
    bool (*valid_attribute_p)(TargetOption *self, tree ARG_UNUSED (fndecl), tree ARG_UNUSED (name),
                    tree ARG_UNUSED (args), int ARG_UNUSED (flags));
};

void  target_option_init(TargetOption *self);
//原型 targetm.target_option.override (); #define TARGET_OPTION_OVERRIDE nvptx_option_override
void target_option_override(TargetOption *self);
//原型  targetm.target_option.relayout_function (fndecl); #define TARGET_RELAYOUT_FUNCTION hook_void_tree
void target_option_relayout_function(TargetOption *self,tree fndecl);
//原型 targetm.target_option.need_ipa_fn_target_info (node->decl,info->target_info);#define TARGET_NEED_IPA_FN_TARGET_INFO default_need_ipa_fn_target_info
bool target_option_need_ipa_fn_target_info(TargetOption *self,const_tree fndecl,unsigned int &a);
//原型 targetm.target_option.update_ipa_fn_target_info(info->target_info, stmt);#define TARGET_UPDATE_IPA_FN_TARGET_INFO default_update_ipa_fn_target_info
bool target_option_update_ipa_fn_target_info(TargetOption *self,unsigned int &a, const gimple *gmp);
//原型 targetm.target_option.can_inline_p (caller->decl, callee->decl) #define TARGET_CAN_INLINE_P default_target_can_inline_p
bool target_option_can_inline_p(TargetOption *self,tree caller, tree callee);
//原型  targetm.target_option.valid_attribute_p  #define TARGET_OPTION_VALID_ATTRIBUTE_P default_target_option_valid_attribute_p
bool target_option_valid_attribute_p(TargetOption *self, tree ARG_UNUSED (fndecl), tree ARG_UNUSED (name),
                tree ARG_UNUSED (args), int ARG_UNUSED (flags));



#endif

