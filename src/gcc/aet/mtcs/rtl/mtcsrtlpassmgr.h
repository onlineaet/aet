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


#ifndef __GCC_MTCS_RTL_PASS_MGR__
#define __GCC_MTCS_RTL_PASS_MGR__

#include "../../nlib.h"
#include "../mtcspass.h"
#include "../mtcscomponent.h"
#include "mtcsfwprop.h"
#include "mtcscprop.h"
#include "mtcsgcse.h"
#include "mtcsifcvt.h"
#include "mtcsdse.h"
#include "mtcscombine.h"
#include "mtcsbbreorder.h"
#include "mtcsreorg.h"

/**
 * 创建外部不引用的组件
 */
typedef struct _MtcsRtlPassMgr  MtcsRtlPassMgr;
struct _MtcsRtlPassMgr
{
   MtcsFwprop *mtcsFwprop;
   MtcsCprop *mtcsCprop;
   MtcsGcse *mtcsGcse;
   MtcsIfcvt *mtcsIfcvt;
   MtcsDse *mtcsDse;
   MtcsCombine *mtcsCombine;
   MtcsBBReorder *mtcsBBReorder;
   MtcsReorg *mtcsReorg;

};

MtcsRtlPassMgr *mtcs_rtl_pass_mgr_new(MtcsMode *mtcsMode);
MtcsFwprop *mtcs_rtl_pass_mgr_get_fwprop(MtcsRtlPassMgr *self);
MtcsCprop *mtcs_rtl_pass_mgr_get_cprop(MtcsRtlPassMgr *self);
MtcsGcse *mtcs_rtl_pass_mgr_get_gcse(MtcsRtlPassMgr *self);
MtcsIfcvt *mtcs_rtl_pass_mgr_get_ifcvt(MtcsRtlPassMgr *self);
MtcsDse *mtcs_rtl_pass_mgr_get_dse(MtcsRtlPassMgr *self);
MtcsCombine *mtcs_rtl_pass_mgr_get_combine(MtcsRtlPassMgr *self);
MtcsBBReorder *mtcs_rtl_pass_mgr_get_bb_reorder(MtcsRtlPassMgr *self);
MtcsReorg *mtcs_rtl_pass_mgr_get_reorg(MtcsRtlPassMgr *self);



//原型 NEXT_PASS (pass_rest_of_compilation, 1);  RTL_PASS  passes.cc   *rest_of_compilation   n  无execute代码
typedef struct _MtcsPassRestOfCompilation MtcsPassRestOfCompilation;
struct _MtcsPassRestOfCompilation
{
   MtcsPass parent;
};
MtcsPassRestOfCompilation *mtcs_pass_rest_of_compilation_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_reginfo_init, 1); RTL_PASS reginfo.cc reginfo n 无条件执行 reginfo_init
typedef struct _MtcsPassRegInfo MtcsPassRegInfo;
struct _MtcsPassRegInfo
{
   MtcsPass parent;
};
MtcsPassRegInfo *mtcs_pass_reg_info_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_initialize_regs, 1);  RTL_PASS  init-regs.cc   init-regs   y  有条件执行 optimize > 0 initialize_uninitialized_regs
typedef struct _MtcsPassInitRegs MtcsPassInitRegs;
struct _MtcsPassInitRegs
{
   MtcsPass parent;
};
MtcsPassInitRegs *mtcs_pass_init_regs_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_stack_ptr_mod, 1);  RTL_PASS  stack-ptr-mod.cc *stack_ptr_mod n 无条件执行 ...crtl->sp_is_unchanging...
typedef struct _MtcsPassStackPtrMod MtcsPassStackPtrMod;
struct _MtcsPassStackPtrMod
{
   MtcsPass parent;
};
MtcsPassStackPtrMod *mtcs_pass_stack_ptr_mod_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_postreload, 1);  RTL_PASS  passes.cc *all-postreload n 无执行代码 reload_completed
typedef struct _MtcsPassPostReload MtcsPassPostReload;
struct _MtcsPassPostReload
{
   MtcsPass parent;
};
MtcsPassPostReload *mtcs_pass_post_reload_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_late_compilation, 1);RTL_PASS  passes.cc *all-late_compilation n 无执行代码 reload_completed || targetm.no_register_allocation;
typedef struct _MtcsPassAllLateCompilation MtcsPassAllLateCompilation;
struct _MtcsPassAllLateCompilation
{
   MtcsPass parent;
};
MtcsPassAllLateCompilation *mtcs_pass_all_late_compilation_new(MtcsMode *mtcsMode);

#endif
