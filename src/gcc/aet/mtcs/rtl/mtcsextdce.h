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


#ifndef __GCC_MTCS_EXT_DCE__
#define __GCC_MTCS_EXT_DCE__

#include "../../nlib.h"
#include "../mtcspass.h"
#include "../mtcsmicro.h"

//原型 NEXT_PASS (pass_ext_dce, 1); RTL_PASS ext-dce.cc ext_dce n 有条件执行 (flag_ext_dce && optimize > 0);ext_dce_execute

typedef struct _MtcsExtDce  MtcsExtDce;
struct _MtcsExtDce
{
   MtcsPass parent;
   /* These should probably move into a C++ class.  */
   vec<bitmap_head> livein;
   bitmap all_blocks;
   bitmap livenow;
   bitmap changed_pseudos;
   bool modify;

};


MtcsExtDce *mtcs_ext_dce_new(MtcsMode *mtcsMode);


#endif
