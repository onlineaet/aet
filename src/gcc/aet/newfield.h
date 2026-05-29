/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

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

#ifndef __GCC_NEW_FIELD_H__
#define __GCC_NEW_FIELD_H__

#include "nlib.h"
#include "c-aet.h"
#include "classinit.h"
#include "newstrategy.h"

/**
 * 处理非指针类型的类变量
 * 1.全局变量
 * 2.局部变量(函数内)
 * 3.静态变量（函数内，全局）
 */

typedef struct _NewField NewField;
/* --- structures --- */
struct _NewField
{
	NewStrategy parent;
};


NewField *new_field_new();
nboolean  new_field_modify_object(NewField *self,tree var);
void      new_field_finish(NewField *self,CreateClassMethod method,tree func);

#endif


