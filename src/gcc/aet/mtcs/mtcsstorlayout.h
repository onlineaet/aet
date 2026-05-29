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

#ifndef __GCC_MTCS_STOR_LAYOUT__
#define __GCC_MTCS_STOR_LAYOUT__

#include "../nlib.h"
#include "mtcscomponent.h"

typedef struct _MtcsStorLayout MtcsStorLayout;
struct _MtcsStorLayout
{
  MtcsComponent parent;

  /* An array of functions used for self-referential size computation.  */
  //原型 size_functions stor-layout.cc
  GTY(()) vec<tree, va_gc> *size_functions;
};

MtcsStorLayout    *mtcs_stor_layout_new(MtcsMode *mtcsMode);
//原型 layout_type stor-layout.h stor-layout.cc
void mtcs_stor_layout_layout_type (MtcsStorLayout *self ,tree type);
//原型 mode_for_size_tree stor-layout.h stor-layout.cc
opt_machine_mode mtcs_stor_layout_mode_for_size_tree (MtcsStorLayout *self,const_tree size, enum mode_class mclass, int limit);
//原型 variable_size stor-layout.h stor-layout.cc
tree mtcs_stor_layout_variable_size (MtcsStorLayout *self,tree size);
//原型 make_signed_type stor-layout.h stor-layout.cc
tree mtcs_stor_layout_make_signed_type (MtcsStorLayout *self,int precision);
//原型 make_unsigned_type stor-layout.h stor-layout.cc
tree mtcs_stor_layout_make_unsigned_type (MtcsStorLayout *self,int precision);
//原型 fixup_unsigned_type stor-layout.h stor-layout.cc
void mtcs_stor_layout_fixup_unsigned_type (MtcsStorLayout *self,tree type);
//原型 fixup_signed_type stor-layout.h stor-layout.cc
void mtcs_stor_layout_fixup_signed_type (MtcsStorLayout *self,tree type);
//原型 finish_record_layout stor-layout.h stor-layout.cc
void mtcs_stor_layout_finish_record_layout (MtcsStorLayout *self,record_layout_info rli, int free_p);
//原型 compute_record_mode stor-layout.h stor-layout.cc
void mtcs_stor_layout_compute_record_mode (MtcsStorLayout *self,tree type);
//原型 finish_bitfield_layout stor-layout.h stor-layout.cc
void mtcs_stor_layout_finish_bitfield_layout (MtcsStorLayout *self,tree t);
//原型 layout_decl stor-layout.h stor-layout.cc
void mtcs_stor_layout_layout_decl (MtcsStorLayout *self,tree decl, unsigned int known_align);
//原型 make_fract_type stor-layout.h stor-layout.cc
tree mtcs_stor_layout_make_fract_type (MtcsStorLayout *self,int precision, int unsignedp, int satp);
//原型 make_accum_type stor-layout.h stor-layout.cc
tree mtcs_stor_layout_make_accum_type (MtcsStorLayout *self,int precision, int unsignedp, int satp);
//原型 relayout_decl stor-layout.h stor-layout.cc
void mtcs_stor_layout_relayout_decl (MtcsStorLayout *self,tree decl);

#endif
