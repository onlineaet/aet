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


#ifndef __GCC_MTCS_GIMPLE_EXPR__
#define __GCC_MTCS_GIMPLE_EXPR__

#include "../nlib.h"
#include "mtcsmicro.h"
#include "mtcscomponent.h"
typedef struct _MtcsGimpleExpr MtcsGimpleExpr;

struct _MtcsGimpleExpr
{
    MtcsComponent parent;
};


MtcsGimpleExpr    *mtcs_gimple_expr_new(MtcsMode *mtcsMode);
//原型 is_gimple_reg gimple.h gimple-expr.cc
bool mtcs_gimple_expr_is_gimple_reg (MtcsGimpleExpr *self,tree t);
//原型 needs_to_live_in_memory tree.h tree.cc
bool mtcs_gimple_expr_needs_to_live_in_memory (MtcsGimpleExpr *self,const_tree t);

#endif
