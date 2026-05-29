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

#ifndef __AET_MTCS_PROVIDER_H__
#define __AET_MTCS_PROVIDER_H__

#include "../../aet.h"
#include "MtcsDevice.h"
#include "MtcsEventListener.h"

package$ aet.mtcs;



public$  abstract$ class$ MtcsProvider{
   public$ static char *CUDA_NAME="cuda";
   public$ static char *GCN_NAME="gcn";
   public$ static int MAX_DEVICES =1000; //最大设备数

   private$ char *name; //供应商名字
   private$ int number;//供应商序号，从0开始, 0也是缺省的供应商
   private$ MtcsDevice *devices[MtcsProvider.MAX_DEVICES];
   private$ int deviceCount;


   public$  MtcsDevice *getDefaultDevice();
   public$  MtcsDevice *getDevice(int devNum);
   //获取当前设备号
   public$ abstract$ int  getDevice();
   public$ int   getDeviceCount();
   public$ void  setName(char *name);
   public$ char *getName();
   public$ void  setNumber(int number);
   public$ int   getNumber();
   public$ void  addListener(MtcsEventListener *listener);
   public$ void  removeListener(MtcsEventListener *listener);
   public$ void  addDevice(MtcsDevice *device);

};

#endif /* __N_MEM_H__ */

