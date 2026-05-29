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

#include "MtcsStream.h"
#include "MtcsProvider.h"
#include "MtcsSystem.h"


impl$  MtcsStream {

   public$ MtcsStream(){
      self->providerNum=0;
      self->devNum=0;
      self->stream=NULL;
   }

   public$ static MtcsStream *buildStream(){
      //获取缺省的供应商
      MtcsProvider *provider = MtcsSystem.getDefaultProvider();
      MtcsDevice *device=provider->getDefaultDevice();
      return device->createStream();
   }


   public$ static MtcsStream *buildStream(int devNum){
      //获取缺省的供应商
      MtcsProvider *provider = MtcsSystem.getDefaultProvider();
      MtcsDevice *device=provider->getDevice(devNum);
      return device->createStream();
   }

   public$ static MtcsStream *buildStream(int providerNum,int devNum){
      //获取缺省的供应商
      MtcsProvider *provider = MtcsSystem.getProvider(providerNum);
      MtcsDevice *device=provider->getDevice(devNum);
      return device->createStream();
   }

   public$ static MtcsStream *buildStreamWithFlag(int flag){
      MtcsProvider *provider = MtcsSystem.getDefaultProvider();
      MtcsDevice *device=provider->getDefaultDevice();
      return device->createStream(flag);

   }

   public$ static MtcsStream *buildStreamWithFlag(int devNum,int flag){
      MtcsProvider *provider = MtcsSystem.getDefaultProvider();
      MtcsDevice *device=provider->getDevice(devNum);
      return device->createStream(flag);

   }

   public$ static MtcsStream *buildStreamWithFlag(int providerNum,int devNum,int flag){
      MtcsProvider *provider = MtcsSystem.getProvider(providerNum);
      MtcsDevice *device=provider->getDevice(devNum);
      return device->createStream(flag);
   }

   public$ void *getStream(){
      return stream;
   }


};
