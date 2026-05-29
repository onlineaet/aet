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


#ifndef __GCC_AET_LIB_H__
#define __GCC_AET_LIB_H__

#include "nlib.h"

/**
 * 解析.so文件
 */

typedef struct _AetLib AetLib;
/* --- structures --- */
struct _AetLib
{
   NPtrArray *resultArray;
   nboolean haveIfaceData;
   char *implIfaces;//库中实现的iface类名
   nboolean haveGenericObjs;//是否已从库人取出的泛型对象。
   NPtrArray *genObjArray;
   NPtrArray *genInfoAndBlockArray;
   nboolean haveGenericInfoAndBlock;
   nboolean haveClassIfaceImplInfo;//有没有接口实现信息
   char *classIfaceImplInfo;//库中的类实现接口信息
   char *buffer;//aet库中的内容
};

AetLib    *aet_lib_get();
nboolean   aet_lib_have_iface(AetLib *self,char *sysName);
NPtrArray *aet_lib_get_generic_objs(AetLib *self);
NPtrArray *aet_lib_get_generic_info_and_block(AetLib *self);
char      *aet_lib_get_class_iface_impl_info(AetLib *self);

#endif
