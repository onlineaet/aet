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

#ifndef __GCC_FUNC_POINTER_H__
#define __GCC_FUNC_POINTER_H__

#include "nlib.h"

/**
 * 检查用类中的静态函数赋值给函数指针是否合法。
 * 1.参数个数
 * 2.返回值
 * 3.每个参数的比较
 */

int func_pointer_check(tree lhs,tree rhs,int *paramNum);

#endif


