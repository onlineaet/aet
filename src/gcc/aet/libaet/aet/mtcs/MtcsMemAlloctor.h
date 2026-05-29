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

#ifndef __AET_MTCS_MEM_ALLOCTOR_H__
#define __AET_MTCS_MEM_ALLOCTOR_H__

#include "../../aet.h"
#include "MtcsStream.h"

package$ aet.mtcs;



public$  interface$ MtcsMemAlloctor{
  /**
   * isDevice 分配设备内存，否则分配托管内存。
   */
  public$ void  *malloc(size_t size,aboolean useDeviceMem);
  public$ void  *calloc(size_t size,aboolean useDeviceMem);
  public$ void  *memset(void *data,int c,size_t size);
  public$ void  *memcpy(void *data,void *src,size_t size,int direct);
  public$ void  *memcpyAsync(void *data,void *src,size_t size,int direct,MtcsStream *steam);
  public$ void   free(void *data);
  /**
   * 返回内存所在的设备
   */
  public$ int getDevice(unsigned long address);

};

#endif /* __N_MEM_H__ */

