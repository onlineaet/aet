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

#ifndef __GCC_MTCS_LOOP_IV__
#define __GCC_MTCS_LOOP_IV__

#include "../nlib.h"
#include "mtcscomponent.h"

struct mtcs_biv_entry_hasher;

typedef struct _MtcsLoopIv MtcsLoopIv;
struct _MtcsLoopIv
{
    MtcsComponent parent;
    bool clean_slate;// = true;
    unsigned int iv_ref_table_size;// = 0;
    /* Table of rtx_ivs indexed by the df_ref uid field.  */
    class rtx_iv ** iv_ref_table;
    /* The current loop.  */
    class loop *current_loop;
    /* Bivs of the current loop.  */
    hash_table<mtcs_biv_entry_hasher> *bivs;

};

MtcsLoopIv *mtcs_loop_iv_new(MtcsMode *mtcsMode);
//原型 iv_analysis_loop_init cfgloop.h loop-iv.cc
void mtcs_loop_iv_iv_analysis_loop_init (MtcsLoopIv *self,class loop *loop);
//原型 iv_analyze_expr cfgloop.h loop-iv.cc
bool mtcs_loop_iv_iv_analyze_expr (MtcsLoopIv *self,rtx_insn *insn, scalar_int_mode mode, rtx rhs,
       class rtx_iv *iv);
//原型 iv_analyze cfgloop.h loop-iv.cc
bool mtcs_loop_iv_iv_analyze (MtcsLoopIv *self,rtx_insn *insn, scalar_int_mode mode, rtx val, class rtx_iv *iv);
//原型 iv_analyze_result cfgloop.h loop-iv.cc
bool mtcs_loop_iv_iv_analyze_result (MtcsLoopIv *self,rtx_insn *insn, rtx def, class rtx_iv *iv);
//原型 biv_p cfgloop.h loop-iv.cc
bool mtcs_loop_iv_biv_p (MtcsLoopIv *self,rtx_insn *insn, scalar_int_mode mode, rtx reg);
//原型 get_iv_value cfgloop.h loop-iv.cc
rtx mtcs_loop_iv_get_iv_value (MtcsLoopIv *self,class rtx_iv *iv, rtx iteration);
//原型 iv_analysis_done cfgloop.h loop-iv.cc
void mtcs_loop_iv_iv_analysis_done (MtcsLoopIv *self);
//原型 canon_condition rtl.h loop-iv.cc
rtx mtcs_loop_iv_canon_condition (MtcsLoopIv *self,rtx cond);
//原型 simplify_using_condition rtl.h loop-iv.cc
void mtcs_loop_iv_simplify_using_condition (MtcsLoopIv *self,rtx cond, rtx *expr, regset altered);
//原型 get_simple_loop_desc cfgloop.h loop-iv.cc
class niter_desc * mtcs_loop_iv_get_simple_loop_desc (MtcsLoopIv *self,class loop *loop);
//原型 free_simple_loop_desc cfgloop.h loop-iv.cc
void mtcs_loop_iv_free_simple_loop_desc (MtcsLoopIv *self,class loop *loop);

#endif

