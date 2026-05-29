/*
 * Copyright (C) 2026  zclei
 * This file is part of AET.

 * AET is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3, or (at your option) any later
 * version.

 * AET is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.

 * You should have received a copy of the GNU General Public License
 * along with GCC Exception along with this program; see the file COPYING3.
 * If not see <http://www.gnu.org/licenses/>.
 * AET was originally developed  by the zclei@sina.com
 */
#ifndef __AET_CUDA_MEM_ALLOCTOR_H__
#define __AET_CUDA_MEM_ALLOCTOR_H__

#include "../../../aet.h"
#include "../MtcsMemAlloctor.h"
#include "../MtcsEventListener.h"

package$ aet.mtcs.cuda;

#define UNIFIED_MEMORY_FULL    0 //完整的 CUDA 统一内存
#define UNIFIED_MEMORY_MANAGED 1 //仅 CUDA 管理内存具有完全支持

public$  class$ CudaMemAlloctor implements$ MtcsMemAlloctor{
   static void memCallback_cb(unsigned long address,void *userData);
   int providerNum;
   int devNum;
   int level;
   private$ MtcsEventListener *listeners[20];
   private$ int listenersCount;
   CudaMemAlloctor(int devNum);
   void setProviderNumber(int providerNum);
   public$ void  addListener(MtcsEventListener *listener);
   public$ void  removeListener(MtcsEventListener *listener);

 };

#endif /* __N_MEM_H__ */

