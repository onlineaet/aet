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

#ifndef GCC_AET_CHECK_H
#define GCC_AET_CHECK_H


/**
 * 检查函数参数类型与实参是否匹配
 * zclei 加的
 */
int  aet_check_match_expr (tree lhs, tree lhstype,enum tree_code modifycode,tree rhs, tree rhs_origtype);
int  aet_check_build_unary_op (location_t location, enum tree_code code, tree xarg,bool noconvert);

#endif /* ! GCC_AET_TYPECK_H */
