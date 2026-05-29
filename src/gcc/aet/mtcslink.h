/*
 * Copyright (C) 2022 , guiyang,wangyong co.,ltd.

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

#ifndef __GCC_MTCS_LINK_H__
#define __GCC_MTCS_LINK_H__

#include "nlib.h"

/**
 * 两个主要功能
 * 1.执行kernel代码
 * 2.解析kernel函数声明代码
 */
typedef struct _MtcsLink MtcsLink;
/* --- structures --- */
struct _MtcsLink
{
    char *collectMtcsLinkFile;
};

MtcsLink *mtcs_link_new();
void  mtcs_link_add(MtcsLink *self,const char *linkFuncNames,int version,int isa,const char *platName);
void  mtcs_link_link(MtcsLink *self);

#endif


