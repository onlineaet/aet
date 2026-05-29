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


#ifndef __GCC_MTCS_LOOPINIT__
#define __GCC_MTCS_LOOPINIT__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "../mtcspass.h"

typedef struct _MtcsLoopinit MtcsLoopinit;
struct _MtcsLoopinit
{
    MtcsComponent parent;
};

MtcsLoopinit *mtcs_loopinit_new(MtcsMode *mtcsMode);
//原型 loop_optimizer_init cfgloop.h loop-init.cc
void mtcs_loopinit_loop_optimizer_init (MtcsLoopinit *self,unsigned flags);

//原型 NEXT_PASS (pass_loop2, 1); RTL_PASS loop-init.cc loop2 y 无执行代码  optimize > 0..
typedef struct _MtcsPassLoop2 MtcsPassLoop2;
struct _MtcsPassLoop2
{
   MtcsPass parent;
};
MtcsPassLoop2 *mtcs_pass_loop2_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_rtl_loop_init, 1); RTL_PASS loop-init.cc loop2_init  n 无条件执行  rtl_loop_init
typedef struct _MtcsPassLoopInit MtcsPassLoopInit;
struct _MtcsPassLoopInit
{
   MtcsPass parent;
};
MtcsPassLoopInit *mtcs_pass_loop_init_new(MtcsMode *mtcsMode);





//原型 NEXT_PASS (pass_rtl_doloop, 1); RTL_PASS loop-init.cc loop2_doloop  y 有条件执行flag_branch_on_count_reg && targetm.have_doloop_end
//doloop_optimize_loops
typedef struct _MtcsPassDoloop  MtcsPassDoloop;
struct _MtcsPassDoloop
{
   MtcsPass parent;
};
MtcsPassDoloop *mtcs_pass_doloop_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_rtl_loop_done, 1); RTL_PASS loop-init.cc loop2_done n 无条件执行 loop_optimizer_finalize  cleanup_cfg (0);
typedef struct _MtcsPassLoopDone  MtcsPassLoopDone;
struct _MtcsPassLoopDone
{
   MtcsPass parent;
};
MtcsPassLoopDone *mtcs_pass_loop_done_new(MtcsMode *mtcsMode);



#endif

