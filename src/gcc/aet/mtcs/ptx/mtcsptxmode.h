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

#ifndef __GCC_MTCS_PTX_MODE__
#define __GCC_MTCS_PTX_MODE__

#include "aet/nlib.h"
#include "../mtcsmode.h"

//介绍machine_mode
//https://blog.csdn.net/wuhui_gdnt/article/details/5319053

#define PTX_ABI64 64
#define PTX_Pmode (PTX_ABI64 ? (machine_mode)PTX_DImode : (machine_mode)PTX_SImode)

typedef struct _MtcsPtxMode MtcsPtxMode;
struct _MtcsPtxMode
{
    MtcsMode parent;
};

MtcsPtxMode *mtcs_ptx_mode_new();
bool mtcs_ptx_mode_libgcc_floating_mode_supported_p(MtcsPtxMode *self,scalar_mode mode);





#endif

