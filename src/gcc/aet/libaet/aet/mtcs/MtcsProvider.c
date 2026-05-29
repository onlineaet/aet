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

#include "MtcsProvider.h"

impl$  MtcsProvider {

   MtcsProvider(){
      name=NULL;
      number= -1;
      deviceCount = 0;
   }

   public$ void setName(char *name){
      if(self->name==NULL)
         self->name=strdup(name);
      else{
         abort();
      }
   }

   public$ char *getName(){
      return name;
   }
   /**
     * 设供应商序号
     */
    public$ void setNumber(int number){
       self->number=number;
       int i;
       for(i=0;i<deviceCount;i++){
          MtcsDevice *dev=devices[i];
          dev->setProviderNumber(number);
       }
    }

   public$ int  getNumber(){
      return number;
   }

   public$ void  addListener(MtcsEventListener *listener){
      int i;
      for(i=0;i<deviceCount;i++){
         MtcsDevice *dev=devices[i];
         dev->addListener(listener);
      }
   }

   public$ void  removeListener(MtcsEventListener *listener){
      int i;
      for(i=0;i<deviceCount;i++){
         MtcsDevice *dev=devices[i];
         dev->removeListener(listener);
      }
   }

   public$ void  addDevice(MtcsDevice *device){
      devices[deviceCount++]=device->ref();
   }

   /**
    * 实现抽象方法 getDevice(int devNum)
    */
   public$ MtcsDevice *getDevice(int devNum){
      if(devNum<0 || devNum>=deviceCount){
         printf("设备号不正确:%d 范围是:0-%d\n",devNum,deviceCount-1);
         abort();
      }
      return devices[devNum];
   }


   /**
    * 实现抽象方法 getDefaultDevice
    */
   public$ MtcsDevice *getDefaultDevice(){
      if(deviceCount>0)
        return devices[0];
      return NULL;
   }

   /**
    * 所有设备数
    */
   public$ int   getDeviceCount(){
      return deviceCount;
   }

};



