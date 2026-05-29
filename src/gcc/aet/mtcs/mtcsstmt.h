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
 * base on stmt.cc
 */

#ifndef __GCC_MTCS_STMT__
#define __GCC_MTCS_STMT__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcsmicro.h"


typedef struct _MtcsStmt MtcsStmt;
struct _MtcsStmt
{
    MtcsComponent parent;
};

MtcsStmt *mtcs_stmt_new(MtcsMode *mtcsMode);
//原型 expand_sjlj_dispatch_table stmt.h stmt.cc
void mtcs_stmt_expand_sjlj_dispatch_table (MtcsStmt *self,rtx dispatch_index,
                vec<tree> dispatch_table);
//原型 parse_output_constraint stmt.h stmt.cc
bool mtcs_stmt_parse_output_constraint (MtcsStmt *self,const char **constraint_p, int operand_num,
             int ninputs, int noutputs, bool *allows_mem, bool *allows_reg, bool *is_inout);

//原型 parse_input_constraint stmt.h stmt.cc
bool mtcs_stmt_parse_input_constraint (MtcsStmt *self,const char **constraint_p, int input_num,
            int ninputs, int noutputs, int ninout, const char * const * constraints,
            bool *allows_mem, bool *allows_reg);

//原型 tree_overlaps_hard_reg_set stmt.h stmt.cc
tree mtcs_stmt_tree_overlaps_hard_reg_set (MtcsStmt *self,tree decl, HardRegSet *regs);
//原型 expand_naked_return rtl.h stmt.cc
void mtcs_stmt_expand_naked_return (MtcsStmt *self);
//原型 jump_target_rtx stmt.h stmt.cc
rtx_code_label * mtcs_stmt_jump_target_rtx (MtcsStmt *self,tree label);
//原型 label_rtx stmt.h stmt.cc
rtx_insn * mtcs_stmt_label_rtx (MtcsStmt *self,tree label);
//原型 expand_label stmt.h stmt.cc
void mtcs_stmt_expand_label (MtcsStmt *self,tree label);
//原型 force_label_rtx stmt.h stmt.cc
rtx_insn * mtcs_stmt_force_label_rtx (MtcsStmt *self,tree label);
//原型 expand_case stmt.h stmt.cc
void mtcs_stmt_expand_case (MtcsStmt *self,gswitch *stmt);

#endif
