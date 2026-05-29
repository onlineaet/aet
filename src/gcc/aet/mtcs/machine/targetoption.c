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

#include "config.h"
#define INCLUDE_ALGORITHM /* reverse */
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "tree-pass.h"
#include "cfghooks.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "tree-pretty-print.h" /* for dump_function_header */
#include "varasm.h"
#include "insn-attr.h"
#include "conditions.h"
#include "flags.h"
#include "output.h"
#include "except.h"
#include "rtl-error.h"
#include "toplev.h" /* exact_log2, floor_log2 */
#include "reload.h"
#include "intl.h"
#include "cfgrtl.h"
#include "debug.h"
#include "tree-pass.h"
#include "tree-ssa.h"
#include "cfgloop.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "rtl-iter.h"
#include "print-rtl.h"
#include "function-abi.h"
#include "common/common-target.h"
#include "diagnostic.h"
#include "context.h"
#include "options.h"
#include "expmed.h"
#include "expr.h"
#include "fold-const.h"
#include "dwarf2out.h"
#include "common/common-targhooks.h"

#include "cfganal.h"
#include "cfgcleanup.h"
#include "alias.h"
#include "rtlhooks-def.h"
#include "tree-pass.h"
#include "dbgcnt.h"
#include "rtlanal.h"

#include "targetoption.h"
#include "aet/aetprinttree.h"
#include "../mtcstarget.h"

//原型  targetm.target_option.relayout_function (fndecl); #define TARGET_RELAYOUT_FUNCTION hook_void_tree
static  void relayoutFunction_cb(TargetOption *self,tree fndecl)
{

}

//原型 targetm.target_option.need_ipa_fn_target_info (node->decl,info->target_info);#define TARGET_NEED_IPA_FN_TARGET_INFO default_need_ipa_fn_target_info
static bool needIpaFnTargetInfo_cb(TargetOption *self,const_tree fndecl,unsigned int &a)
{
    return false;
}

//原型 targetm.target_option.update_ipa_fn_target_info(info->target_info, stmt);#define TARGET_UPDATE_IPA_FN_TARGET_INFO default_update_ipa_fn_target_info
static bool updateIpaFnTargetInfo_cb(TargetOption *self,unsigned int &a, const gimple *gmp)
{
    return false;
}


//原型 targetm.target_option.can_inline_p (caller->decl, callee->decl) #define TARGET_CAN_INLINE_P default_target_can_inline_p
static bool canInlineP_cb(TargetOption *self,tree caller, tree callee)
{
    tree callee_opts = DECL_FUNCTION_SPECIFIC_TARGET (callee);
    tree caller_opts = DECL_FUNCTION_SPECIFIC_TARGET (caller);
    if (! callee_opts)
      callee_opts = target_option_default_node;
    if (! caller_opts)
      caller_opts = target_option_default_node;

    /* If both caller and callee have attributes, assume that if the
       pointer is different, the two functions have different target
       options since build_target_option_node uses a hash table for the
       options.  */
    return callee_opts == caller_opts;
}

//原型  targetm.target_option.valid_attribute_p  #define TARGET_OPTION_VALID_ATTRIBUTE_P default_target_option_valid_attribute_p
static bool validAttributeP_cb (TargetOption *self,tree ARG_UNUSED (fndecl), tree ARG_UNUSED (name),
                tree ARG_UNUSED (args),int ARG_UNUSED (flags))
{
   warning (OPT_Wattributes, "%<target%> attribute is not supported on this machine");
   return false;
}


void  target_option_init(TargetOption *self)
{
   //原型 targetm.target_option.override (); #define TARGET_OPTION_OVERRIDE nvptx_option_override
    self->override = NULL;
    //原型  targetm.target_option.relayout_function (fndecl); #define TARGET_RELAYOUT_FUNCTION hook_void_tree
    self->relayout_function=relayoutFunction_cb;
    //原型 targetm.target_option.need_ipa_fn_target_info (node->decl,info->target_info);#define TARGET_NEED_IPA_FN_TARGET_INFO default_need_ipa_fn_target_info
    self->need_ipa_fn_target_info=needIpaFnTargetInfo_cb;
    //原型 targetm.target_option.update_ipa_fn_target_info(info->target_info, stmt);#define TARGET_UPDATE_IPA_FN_TARGET_INFO default_update_ipa_fn_target_info
    self->update_ipa_fn_target_info=updateIpaFnTargetInfo_cb;
    //原型 targetm.target_option.can_inline_p (caller->decl, callee->decl) #define TARGET_CAN_INLINE_P default_target_can_inline_p
    self->can_inline_p=canInlineP_cb;
    //原型  targetm.target_option.valid_attribute_p  #define TARGET_OPTION_VALID_ATTRIBUTE_P default_target_option_valid_attribute_p
    self->valid_attribute_p=validAttributeP_cb;
}

//原型 targetm.target_option.override (); #define TARGET_OPTION_OVERRIDE nvptx_option_override
void target_option_override(TargetOption *self)
{
   self->override(self);
}
//原型  targetm.target_option.relayout_function (fndecl); #define TARGET_RELAYOUT_FUNCTION hook_void_tree
void target_option_relayout_function(TargetOption *self,tree fndecl)
{
   self->relayout_function(self,fndecl);
}

//原型 targetm.target_option.need_ipa_fn_target_info (node->decl,info->target_info);#define TARGET_NEED_IPA_FN_TARGET_INFO default_need_ipa_fn_target_info
bool target_option_need_ipa_fn_target_info(TargetOption *self,const_tree fndecl,unsigned int &a)
{
   return self->need_ipa_fn_target_info(self,fndecl,a);
}

//原型 targetm.target_option.update_ipa_fn_target_info(info->target_info, stmt);#define TARGET_UPDATE_IPA_FN_TARGET_INFO default_update_ipa_fn_target_info
bool target_option_update_ipa_fn_target_info(TargetOption *self,unsigned int &a, const gimple *gmp)
{
   return self->update_ipa_fn_target_info(self,a,gmp);
}

//原型 targetm.target_option.can_inline_p (caller->decl, callee->decl) #define TARGET_CAN_INLINE_P default_target_can_inline_p
bool target_option_can_inline_p(TargetOption *self,tree caller, tree callee)
{
   return self->can_inline_p(self,caller,callee);
}

//原型  targetm.target_option.valid_attribute_p  #define TARGET_OPTION_VALID_ATTRIBUTE_P default_target_option_valid_attribute_p
bool target_option_valid_attribute_p(TargetOption *self, tree ARG_UNUSED (fndecl), tree ARG_UNUSED (name),
                tree ARG_UNUSED (args), int ARG_UNUSED (flags))
{
   return self->valid_attribute_p(self,fndecl,name,args,flags);
}



