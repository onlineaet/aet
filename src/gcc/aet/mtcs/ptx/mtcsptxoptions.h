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

#ifndef __GCC_MTCS_PTX_OPTIONS__
#define __GCC_MTCS_PTX_OPTIONS__

#include "aet/nlib.h"
#include "../mtcsoptions.h"


typedef struct _MtcsPtxOptions MtcsPtxOptions;
struct _MtcsPtxOptions
{
    MtcsOptions parent;
    int x_nvptx_softstack_size;//原型 #define nvptx_softstack_size global_options.x_nvptx_softstack_size
    int x_nvptx_alias;//原型 #define nvptx_alias global_options.x_nvptx_alias

};

MtcsPtxOptions     *mtcs_ptx_options_new(MtcsMode *mtcsMode);





#endif

