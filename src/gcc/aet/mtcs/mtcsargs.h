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

#ifndef __GCC_MTCS_ARGS__
#define __GCC_MTCS_ARGS__

#include "../nlib.h"
#include "mtcsmode.h"
#include "mtcsmicro.h"



typedef struct _MtcsArgs MtcsArgs;
struct _MtcsArgs
{
    void (*init_cumulative_arg)(MtcsArgs *self,MtcsCumulativeArgs *cum,tree fntype,rtx libname,tree fndecl, int nNamedArgs);
    //原型 PUSH_P(to) expr.cc
    bool (*is_push_p)(MtcsArgs *self,rtx to);
    MtcsCumulativeArgs *(*create_cumulative_args)(MtcsArgs *self);
    void  (*free_cumulative_args)(MtcsArgs *self,MtcsCumulativeArgs *args);

};


void      mtcs_args_init(MtcsArgs *self);
//原型 INIT_CUMULATIVE_ARGS
void mtcs_args_init_cumulative_args(MtcsArgs *self,MtcsCumulativeArgs *cum,tree fntype,rtx libname,tree fndecl, int nNamedArgs);
//原型 PUSH_P (to) expr.cc
bool mtcs_args_is_push_p(MtcsArgs *self,rtx to);
//创建 MtcsCumulativeArgs
MtcsCumulativeArgs *mtcs_args_create_cumulative_args(MtcsArgs *self);
void mtcs_arg_free_cumulative_args(MtcsArgs *self,MtcsCumulativeArgs *args);

#endif
