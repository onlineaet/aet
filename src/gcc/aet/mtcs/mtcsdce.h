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


#ifndef __GCC_MTCS_DCE__
#define __GCC_MTCS_DCE__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcsmicro.h"
#include "mtcspass.h"


/*
 * 无效代码删除（DCE）
 */
typedef struct _MtcsDce MtcsDce;
struct _MtcsDce
{
    MtcsComponent parent;

    /* -------------------------------------------------------------------------
       Core mark/delete routines
       ------------------------------------------------------------------------- */

    /* True if we are invoked while the df engine is running; in this case,
       we don't want to reenter it.  */
     bool df_in_progress;// = false;

    /* True if we are allowed to alter the CFG in this pass.  */
     bool can_alter_cfg;// = false;

    /* Instructions that have been marked but whose dependencies have not
       yet been processed.  */
     vec<rtx_insn *> worklist;

    /* Bitmap of instructions marked as needed indexed by INSN_UID.  */
     sbitmap marked;

    /* Bitmap obstacks used for block processing by the fast algorithm.  */
    bitmap_obstack dce_blocks_bitmap_obstack;
    bitmap_obstack dce_tmp_bitmap_obstack;
};

MtcsDce *mtcs_dce_new(MtcsMode *mtcsMode);
//原型 run_word_dce dce.h dce.cc
void mtcs_dce_run_word_dce (MtcsDce *self);
//原型 run_fast_df_dce dce.h dce.cc
void mtcs_dce_run_fast_df_dce (MtcsDce *self);
//原型 run_fast_dce dce.h dce.cc
void mtcs_dce_run_fast_dce (MtcsDce *self);

/**********************以下是基于mtcsdce的rtl pass-------*/

//原型 NEXT_PASS (pass_ud_rtl_dce, 1);  RTL_PASS  ud_dce dce.cc y 有条件执行 optimize > 1 && flag_dce && dbg_cnt (dce_ud);rest_of_handle_ud_dce ()
typedef struct _MtcsPassUdDce MtcsPassUdDce;
struct _MtcsPassUdDce
{
   MtcsPass parent;
};
MtcsPassUdDce *mtcs_pass_ud_dce_new(MtcsMode *mtcsMode);

#endif

