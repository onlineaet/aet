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


#ifndef __GCC_MTCS_FWPROP__
#define __GCC_MTCS_FWPROP__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "../mtcspass.h"

typedef struct _MtcsFwprop MtcsFwprop;
struct _MtcsFwprop
{
    MtcsComponent parent;
    int num_changes;

 };

MtcsFwprop *mtcs_fwprop_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_rtl_fwprop, 1); RTL_PASS fwprop.cc fwprop1 n 有条件执行 optimize > 0 && flag_forward_propagate;   fwprop (false);
typedef struct _MtcsPassFwprop1 MtcsPassFwprop1;
struct _MtcsPassFwprop1
{
   MtcsPass parent;
};
MtcsPassFwprop1 *mtcs_pass_fwprop1_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_rtl_fwprop_addr, 1); RTL_PASS fwprop.cc fwprop2 n 有条件执行 optimize > 0 && flag_forward_propagate;   fwprop (true);
typedef struct _MtcsPassFwprop2 MtcsPassFwprop2;
struct _MtcsPassFwprop2
{
   MtcsPass parent;
};
MtcsPassFwprop2 *mtcs_pass_fwprop2_new(MtcsMode *mtcsMode);


#endif

