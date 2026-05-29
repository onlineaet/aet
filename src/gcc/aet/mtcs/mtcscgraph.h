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

#ifndef __GCC_MTCS_CGRAPH__
#define __GCC_MTCS_CGRAPH__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcsmicro.h"


typedef struct _MtcsCgraph MtcsCgraph;

struct _MtcsCgraph
{
    MtcsComponent parent;
    NPtrArray *rtlInfoArray;

};
MtcsCgraph  *mtcs_cgraph_new(MtcsMode *mtcsMode);
//原型   static struct cgraph_rtl_info *rtl_info (const_tree); cgraph.h cgraph.cc
MtcsCgraphRtlInfo *mtcs_cgraph_get_rtl_info (MtcsCgraph *self,const_tree fndecl);
//原型 redirect_call_stmt_to_callee cgraph.h cgraph.cc
gimple *mtcs_cgraph_redirect_call_stmt_to_callee (MtcsCgraph *self,cgraph_edge *e,hash_set <tree> *killed_ssas = nullptr);
//原型 cgraph_node::get_body cgraph.h cgraph.cc
bool mtcs_cgraph_get_body(MtcsCgraph *self,struct cgraph_node *node);
//原型 cgraph_node::get_untransformed_body cgraph.h cgraph.cc
bool mtcs_cgraph_get_untransformed_body(MtcsCgraph *self,struct cgraph_node *node);
/* Expand function specified by node.  */
//原型 node->expad cgraph.h cgraphunit.cc
void mtcs_cgraph_node_expand(MtcsCgraph *self, struct cgraph_node *node);

#endif
