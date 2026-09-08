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
   //对应编译  _RandomGenerator_2962277235__impl_iface.c 时生成的变量 LIB_GLOBAL_IFACE_VAR_NAME_PREFIX
   //说明库中已经有这个接口的实现。
   nboolean haveIfaceData;
   //被 IFACE_START...IFACE_END 包裹的字符串，来自编译_RandomGenerator_2962277235__impl_iface.c时生成
   char *implIfaces;

   nboolean haveGenericObjs;//是否已从库人取出的泛型对象。
   NPtrArray *genObjArray;
   NPtrArray *genInfoAndBlockArray;
   NPtrArray *funcWithGbArray;//库中的带泛型块函数源代码
   nboolean haveGenericInfoAndBlock;
   nboolean haveClassIfaceImplInfo;//有没有接口实现信息
   nboolean haveFuncWithGb;//存在带泛型块函数的源代码吗？

   char *classIfaceImplInfo;//库中的类实现接口信息
   char *buffer;//aet库中的内容
   /* 返回所有库中变量 _aet_generic_zero_storage 声明最大数组元素个数*/
   int genericZeroStorageSize;
};

AetLib    *aet_lib_get();
nboolean   aet_lib_have_iface(AetLib *self,char *sysName);
NPtrArray *aet_lib_get_generic_objs(AetLib *self);
NPtrArray *aet_lib_get_generic_info_and_block(AetLib *self);
char      *aet_lib_get_class_iface_impl_info(AetLib *self);
NPtrArray *aet_lib_get_func_with_gb(AetLib *self);
int        aet_lib_get_generic_zero_storage_size(AetLib *self);

#endif
