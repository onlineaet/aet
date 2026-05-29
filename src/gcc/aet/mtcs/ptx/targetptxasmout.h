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

#ifndef __GCC_TARGET_PTX_ASM_OUT__
#define __GCC_TARGET_PTX_ASM_OUT__

#include "../../nlib.h"
#include "../machine/targetasmout.h"

typedef struct _TargetPtxAsmOut TargetPtxAsmOut;
struct _TargetPtxAsmOut
{
   TargetAsmOut parent;
   struct
   {
      unsigned HOST_WIDE_INT mask; /* Mask for storing fragment.  */
      unsigned HOST_WIDE_INT val; /* Current fragment value.  */
      unsigned HOST_WIDE_INT remaining; /*  Remaining bytes to be written
      out.  */
      unsigned size;  /* Fragment size to accumulate.  */
      unsigned offset;  /* Offset within current fragment.  */
      bool started;   /* Whether we've output any initializer.  */
   } init_frag;

   bool need_softstack_decl;
   unsigned vector_red_size;
   unsigned vector_red_align;
   unsigned vector_red_partition;
   bool need_unisimt_decl;


};

TargetPtxAsmOut *target_ptx_asm_out_new(MtcsMode *mtcsMode);




#endif

