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


#ifndef __GCC_MTCS_LOOP_UNROLL__
#define __GCC_MTCS_LOOP_UNROLL__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "../mtcspass.h"

//原型 NEXT_PASS (pass_rtl_unroll_loops, 1); RTL_PASS loop-init.cc loop2_unroll  y 有条件执行 flag_unroll_loops || flag_unrol...
//unroll_loops
typedef struct _MtcsLoopUnroll MtcsLoopUnroll;
struct _MtcsLoopUnroll
{
    MtcsPass parent;
};

MtcsLoopUnroll *mtcs_loop_unroll_new(MtcsMode *mtcsMode);



#endif

