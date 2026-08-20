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
#include "aetparser.h"
/**
 * 性能优化
 * 把调用self->xxx 转成函数调用
 */
typedef struct _CmpRefOpt CmpRefOpt;
/* --- structures --- */
struct _CmpRefOpt
{
	 AetParser *parser;
    NHashTable *hashTable;
    //不在aet类中调用类方法，存储所在的函数
    NHashTable *noAtAetCallTable;

};


CmpRefOpt *cmp_ref_opt_new();
void       cmp_ref_opt_add(CmpRefOpt *self,tree func);
void       cmp_ref_opt_opt(CmpRefOpt *self);
void       cmp_ref_opt_print(CmpRefOpt *self,tree func);
//不在isAet中调用类方法，是否可以优化为函数调用
tree       cmp_ref_opt_outside(CmpRefOpt *self,tree compref,
                       ClassFunc *classFunc,vec<tree, va_gc> *exprlist,location_t loc);

#endif

