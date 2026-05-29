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

#ifndef __GCC_MTCS_REPLACE__
#define __GCC_MTCS_REPLACE__

#include "../nlib.h"
#include "mtcscomponent.h"

/**
 * 替换主机的gimple
 */
typedef struct _MtcsReplace MtcsReplace;

struct _MtcsReplace
{
    MtcsComponent parent;
    char *platformName;//平台名称，比如cuda、gcn、spirv等
};



MtcsReplace  *mtcs_replace_new(MtcsMode *mtcsMode);
//替换
//1.gimple_assign <var_decl, _TSecond_parent__superFuncAddressArray.2_1, _TSecond_parent__superFuncAddressArray, NULL, NULL>
//2.gimple_assign <mem_ref, _2, MEM[(long unsigned int *)_TSecond_parent__superFuncAddressArray.2_1 + 32B], NULL, NULL>
//为
//gimple_assign <array_ref, _1, _TSecond_parent__superDeviceAddressArray[4], NULL, NULL>
void          mtcs_replace_parent_device_func_array(MtcsReplace *self,struct cgraph_node *newNode);
void          mtcs_replace_switch(MtcsReplace *self,struct cgraph_node *newNode);

#endif
