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

#ifndef __GCC_MTCS_DWARF2_CODEVIEW__
#define __GCC_MTCS_DWARF2_CODEVIEW__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "dwarf2out.h"

typedef struct _MtcsDwarf2Codeview MtcsDwarf2Codeview;

struct _MtcsDwarf2Codeview
{
   MtcsComponent parent;
};

MtcsDwarf2Codeview *mtcs_dwarf2_codeview_new(MtcsMode *mtcsMode);

//原型 codeview_debug_finish dwarf2codeview.h dwarf2codeview.cc
void mtcs_dwarf2_codeview_debug_finish (MtcsDwarf2Codeview *self);
//原型 codeview_source_line dwarf2codeview.h dwarf2codeview.cc
void mtcs_dwarf2_codeview_source_line (MtcsDwarf2Codeview *self,unsigned int line_no, const char *filename);
//原型 codeview_switch_text_section dwarf2codeview.h dwarf2codeview.cc
void mtcs_dwarf2_codeview_switch_text_section (MtcsDwarf2Codeview *self);
//原型 codeview_end_epilogue dwarf2codeview.h dwarf2codeview.cc
void mtcs_dwarf2_codeview_codeview_end_epilogue (MtcsDwarf2Codeview *self);
//原型 codeview_source_line dwarf2codeview.h dwarf2codeview.cc
void mtcs_dwarf2_codeview_debug_early_finish (MtcsDwarf2Codeview *self,dw_die_ref die);
//原型 codeview_begin_block dwarf2codeview.h dwarf2codeview.cc
void mtcs_dwarf2_codeview_begin_block (MtcsDwarf2Codeview *self,unsigned int line ATTRIBUTE_UNUSED,unsigned int blocknum, tree block);
//原型 codeview_end_block dwarf2codeview.h dwarf2codeview.cc
void  mtcs_dwarf2_codeview_end_block (MtcsDwarf2Codeview *self,unsigned int line ATTRIBUTE_UNUSED, unsigned int blocknum);
//原型 codeview_abstract_function dwarf2codeview.h dwarf2codeview.cc
void mtcs_dwarf2_codeview_abstract_function (MtcsDwarf2Codeview *self, tree t);

//原型 codeview_start_source_file dwarf2codeview.h dwarf2codeview.cc
void mtcs_dwarf2_codeview_start_source_file (MtcsDwarf2Codeview *sel , const char *filename);


#endif
