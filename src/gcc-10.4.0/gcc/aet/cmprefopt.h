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

#ifndef __GCC_CMP_REF_OPT_H__
#define __GCC_CMP_REF_OPT_H__

#include "nlib.h"

/**
 * 对象方法引用优化
 * 把调用self->xxx 转成函数调用
 */
typedef struct _CmpRefOpt CmpRefOpt;
/* --- structures --- */
struct _CmpRefOpt
{
	c_parser *parser;
    NHashTable *hashTable;
};


CmpRefOpt *cmp_ref_opt_new();
void       cmp_ref_opt_add(CmpRefOpt *self,tree func);
void       cmp_ref_opt_opt(CmpRefOpt *self);
void       cmp_ref_opt_set_parser(CmpRefOpt *self,c_parser *parser);

#endif

