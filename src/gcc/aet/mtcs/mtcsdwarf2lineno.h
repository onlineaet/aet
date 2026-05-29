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

#ifndef __GCC_MTCS_DWARF2_LINENO__
#define __GCC_MTCS_DWARF2_LINENO__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcsmicro.h"
#include "mtcsdebug.h"
#include "mtcsdwarf2out.h"

/* 三个类的关系
   MtcsDebug
      ^
      |
      |
   MtcsDwarf2Out
      ^
      |
      |
   MtcsDwarf2Lineno
*/

typedef struct _MtcsDwarf2Lineno MtcsDwarf2Lineno;
struct _MtcsDwarf2Lineno
{
    MtcsDwarf2Out parent;
};

MtcsDwarf2Lineno *mtcs_dwarf2_lineno_new(MtcsMode *mtcsMode);

#endif
