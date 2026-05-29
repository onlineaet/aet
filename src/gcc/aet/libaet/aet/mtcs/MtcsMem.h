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

#ifndef __AET_MTCS_MEM_H__
#define __AET_MTCS_MEM_H__

#include "../../aet.h"
#include "MtcsStream.h"

package$ aet.mtcs;

typedef struct _MtcsMallocAttribute
{
   int provider;
   int devNum;
   aboolean isDevice;//使用设备内存或托管内存。
}MtcsMallocAttribute;

public$ enum$ MtcsCpyKind{
  HOST2HOST,
  HOST2DEV,
  DEV2HOST,
  DEV2DEV
};


public$  class$ MtcsMem{

   public$ static void *malloc(size_t size);
   public$ static void *malloc(size_t size,aboolean deviceMem);
   public$ static void *malloc(size_t size,MtcsMallocAttribute att);
   public$ static void *calloc(size_t size);
   public$ static void *calloc(size_t size,aboolean deviceMem);
   public$ static void *calloc(size_t size,MtcsMallocAttribute att);
   public$ static void *memset(void *data,int c, size_t n);
   public$ static void *memcpy(void *dest,void *src,size_t n,MtcsCpyKind direct);
   public$ static void *memcpyAsync(void *dest,void *src,size_t n,MtcsCpyKind direct,MtcsStream *stream);
   public$ static void  free(void *data);
   private$ static aboolean getDevice(void *address,int *provider,int *devNumber);

};

#endif /* __N_MEM_H__ */

