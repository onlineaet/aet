/*
   Copyright (C) 2022 zclei@sina.com.

This file is part of AET.

AET is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

AET is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC Exception along with this program; see the file COPYING3.
If not see <http://www.gnu.org/licenses/>.
AET was originally developed  by the zclei@sina.com at guiyang china .
*/

#ifndef __GCC_NRVO_H__
#define __GCC_NRVO_H__

#include "nlib.h"
#include "classinfo.h"

/**
 * 返回值优化
 * https://blog.csdn.net/yao_zou/article/details/50759301
 */

typedef struct _Nrvo Nrvo;
/* --- structures --- */
struct _Nrvo
{
	c_parser *parser;
};


Nrvo *nvro_get();




#endif

