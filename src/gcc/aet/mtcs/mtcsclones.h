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

#ifndef __GCC_MTCS_CLONES__
#define __GCC_MTCS_CLONES__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcsreplace.h"

/*!调用对象MtcsClones的方法时，已与平台有关了 mtcs_clones_clone_kernel方法是在平台下被执行的*/
typedef struct _MtcsClones MtcsClones;

struct _MtcsClones
{
    MtcsComponent parent;
    //主机的函数声明是否已用来创建新的cgraph_node decl对应一个唯一个的mtcs节点。
    NPtrArray *hostDeclArray;
    //记录主机函数和克隆出的MTCS函数
    NPtrArray *mtcsNodeInfoArray;
    MtcsReplace *mtcsReplace;
};



MtcsClones          *mtcs_clones_new(MtcsMode *mtcsMode);
struct cgraph_node  *mtcs_clones_clone_func (MtcsClones *self,struct cgraph_node *origNode);
struct varpool_node *mtcs_clones_clone_var(MtcsClones *self,struct varpool_node *origNode);

void                 mtcs_clones_test_edge(MtcsClones *self);

#endif
