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

#ifndef GCC_CLASS_WARNING_H
#define GCC_CLASS_WARNING_H

#include "nlib.h"
#include "c-aet.h"


/**
 * 在C中从不同的指针转化会出现各种警告信息。
 * 但在aet中，从类转类、类转接口以及接口转类是可能是合法的。
 * 如果通过判断是合法转化，清除原来c中的警告信息。
 */
int  clear_warning_modify(tree type,tree lhs);//当调用build_modify_expr是，判断函数赋值时的参数是否匹配。清除不合理的警告信息


#endif /* ! GCC_C_AET_H */
