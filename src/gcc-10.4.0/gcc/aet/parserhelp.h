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


#ifndef __GCC_PARSER_HELP_H__
#define __GCC_PARSER_HELP_H__

#include "nlib.h"


nboolean parser_help_compare(tree funcType1,tree funcType2);
void     parser_help_add_magic();//为类声明和接口声明加一个变量作用魔数
nboolean parser_help_set_class_or_enum_type(c_token *who);
void     parser_help_set_forbidden(nboolean is);
nboolean parser_help_parser_left_package_dot_class();
nboolean parser_help_parser_right_package_dot_class(char *firstId);


#endif

