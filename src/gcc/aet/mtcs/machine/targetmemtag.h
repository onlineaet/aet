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

#ifndef __GCC_TARGET_MEM_TAG__
#define __GCC_TARGET_MEM_TAG__

#include "../../nlib.h"
#include "machinetarget.h"

typedef struct _TargetMemTag TargetMemTag;
struct _TargetMemTag
{
   MachineTarget parent;
   //原型 targetm.memtag.add_tag (base, offset,hwasan_current_frame_tag ());#define TARGET_MEMTAG_ADD_TAG default_memtag_add_tag
   rtx (*add_tag)(TargetMemTag *self,rtx base, poly_int64 offset, uint8_t tag_offset);
};

TargetMemTag *target_mem_tag_new(MtcsMode *mtcsMode);
rtx           target_mem_tag_add_tag(TargetMemTag *self,rtx base, poly_int64 offset, uint8_t tag_offset);




#endif

