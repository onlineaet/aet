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

#include "MtcsMem.h"
#include "MtcsMemMgr.h"
#include "MtcsMemAlloctor.h"
#include "MtcsSystem.h"
#include "MtcsProvider.h"
#include "MtcsDevice.h"

impl$  MtcsMem {

   public$ static void *malloc(size_t size){
      //获取缺省的供应商
      MtcsProvider *provider = MtcsSystem.getDefaultProvider();
      MtcsDevice *device=provider->getDefaultDevice();
      MtcsMemAlloctor *alloctor = device->getMemAlloctor();
      return alloctor->malloc(size,FALSE);
   }

   public$ static void *malloc(size_t size,aboolean useDeviceMem){
      //获取缺省的供应商
      MtcsProvider *provider = MtcsSystem.getDefaultProvider();
      MtcsDevice *device=provider->getDefaultDevice();
      MtcsMemAlloctor *alloctor = device->getMemAlloctor();
      return alloctor->malloc(size,useDeviceMem);
   }


   public$ static void *malloc(size_t size,MtcsMallocAttribute att){
      MtcsProvider *provider = MtcsSystem.getProvider(att.provider);
      MtcsDevice *device=provider->getDevice(att.devNum);
      MtcsMemAlloctor *alloctor = device->getMemAlloctor();
      return alloctor->malloc(size,att.isDevice);
   }


   public$ static void *calloc(size_t size){
      //获取缺省的供应商
      MtcsProvider *provider = MtcsSystem.getDefaultProvider();
      MtcsDevice *device=provider->getDefaultDevice();
      MtcsMemAlloctor *alloctor = device->getMemAlloctor();
      return alloctor->calloc(size,FALSE);
   }

   public$ static void *calloc(size_t size,aboolean useDeviceMem){
      //获取缺省的供应商
      MtcsProvider *provider = MtcsSystem.getDefaultProvider();
      MtcsDevice *device=provider->getDefaultDevice();
      MtcsMemAlloctor *alloctor = device->getMemAlloctor();
      return alloctor->calloc(size,useDeviceMem);
   }

   public$ static void *calloc(size_t size,MtcsMallocAttribute att){
      MtcsProvider *provider = MtcsSystem.getProvider(att.provider);
      MtcsDevice *device=provider->getDevice(att.devNum);
      MtcsMemAlloctor *alloctor = device->getMemAlloctor();
      return alloctor->calloc(size,att.isDevice);
   }

   public$ static void *memset(void *data,int c, size_t n){
      if(data==NULL)
         return data;
      int providerNum,devNum;
      MtcsMemMgr.getInstance()->get((unsigned long)data,&providerNum,&devNum);
      if(providerNum<0){
         a_error("memset 不是Mtcs内存\n");
         return data;
      }
      //printf("获取的供应商及设备---- %d %d\n",providerNum,devNum);
      MtcsProvider *provider = MtcsSystem.getProvider(providerNum);
      MtcsDevice *device=provider->getDevice(devNum);
      MtcsMemAlloctor *alloctor = device->getMemAlloctor();
      return alloctor->memset(data,c,n);
   }

   /**
   * 获取地址所在的设备和供应商
   */
   aboolean getDevice(void *address,int *providerNum,int *devNumber){
      int i;
      for(i=0;i<MtcsSystem.providerCount;i++){
         MtcsProvider *provider=MtcsSystem.mtcsProviders[i];
         int deviceCount= provider->getDeviceCount();
         int j;
         for(j=0;j<deviceCount;j++){
            MtcsDevice *dev=provider->getDevice(j);
            MtcsMemAlloctor *alloctor=dev->getMemAlloctor();
            int dnum=alloctor->getDevice((unsigned long)address);
            if(dnum>=0){
               int n=provider->getNumber();
               *providerNum=n;
               *devNumber=dnum;
               return TRUE;
            }
         }
      }
      return FALSE;
   }

   public$ static void *memcpy(void *dest,void *src,size_t size,MtcsCpyKind kind){
      //获取缺省的供应商
      int providerNum1,devNum1;
      int providerNum2,devNum2;

      MtcsMemMgr.getInstance()->get((unsigned long)dest,&providerNum1,&devNum1);
      aboolean dHost=FALSE;
      if(providerNum1<0){
         dHost=!getDevice(dest,&providerNum1,&devNum1);
      }

      //printf("MtcsMem.memcpy 目标内存所在设备 -- %d %d dHost:%d\n",providerNum1,devNum1,dHost);

      MtcsMemMgr.getInstance()->get((unsigned long)src,&providerNum2,&devNum2);
      aboolean sHost=FALSE;
      if(providerNum2<0){
         sHost=!getDevice(src,&providerNum2,&devNum2);
      }

      //printf("MtcsMem.memcpy 源内存所在设备 -- %d %d sHost:%d\n",providerNum2,devNum2,sHost);
      if(dHost && sHost){
         a_error("不支持两个主机的内存复制。");
         return NULL;
      }else if(dHost && !sHost){
         if(kind!=MtcsCpyKind.DEV2HOST){
            a_error("设备内存到主机内存的复制 kind:%d\n",kind);
         }else{
            MtcsProvider *provider = MtcsSystem.getProvider(providerNum2);
            MtcsDevice *device=provider->getDevice(devNum2);
            MtcsMemAlloctor *alloctor = device->getMemAlloctor();
            return alloctor->memcpy(dest,src,size,kind);
         }
      }else if(!dHost && sHost){
         if(kind!=MtcsCpyKind.HOST2DEV){
            a_error("主机内存到设备内存的复制 kind:%d\n",kind);
         }else{
            MtcsProvider *provider = MtcsSystem.getProvider(providerNum1);
            MtcsDevice *device=provider->getDevice(devNum1);
            MtcsMemAlloctor *alloctor = device->getMemAlloctor();
            return alloctor->memcpy(dest,src,size,kind);
         }
      }else{
         if(providerNum2!=providerNum1){
            a_error("不支持跨供应商之间的内存复制。dest provider:%d src provider:%d",providerNum1,providerNum2);
            return NULL;
         }
         if(devNum1!=devNum2){
            a_error("不支持跨设备之间的内存复制。dest dev:%d src dev:%d",providerNum1,providerNum2);
            return NULL;
         }

         if(kind!=MtcsCpyKind.DEV2DEV){
            a_error("设备内存到设德内存的复制 kind:%d\n",kind);
         }else{
            MtcsProvider *provider = MtcsSystem.getProvider(providerNum1);
            MtcsDevice *device=provider->getDevice(devNum1);
            MtcsMemAlloctor *alloctor = device->getMemAlloctor();
            return alloctor->memcpy(dest,src,size,kind);
         }
      }
      return NULL;
   }

   public$ static void *memcpyAsync(void *dest,void *src,size_t size,MtcsCpyKind kind,MtcsStream *mtcsStream){
      //获取缺省的供应商
      int providerNum1,devNum1;
      int providerNum2,devNum2;

      MtcsMemMgr.getInstance()->get((unsigned long)dest,&providerNum1,&devNum1);
      aboolean dHost=FALSE;
      if(providerNum1<0){
         dHost=!getDevice(dest,&providerNum1,&devNum1);
      }

      //printf("MtcsMem.memcpy 目标内存所在设备 -- %d %d dHost:%d\n",providerNum1,devNum1,dHost);

      MtcsMemMgr.getInstance()->get((unsigned long)src,&providerNum2,&devNum2);
      aboolean sHost=FALSE;
      if(providerNum2<0){
         sHost=!getDevice(src,&providerNum2,&devNum2);
      }
     // printf("MtcsMem.memcpyAsync 源内存所在设备 -- %d %d sHost:%d\n",providerNum2,devNum2,sHost);

      if(dHost && sHost){
         a_error("不支持两个主机的内存复制。");
         return NULL;
      }else if(dHost && !sHost){
         if(kind!=MtcsCpyKind.DEV2HOST){
            a_error("设备内存到主机内存的复制 kind:%d\n",kind);
         }else{
            MtcsProvider *provider = MtcsSystem.getProvider(providerNum2);
            MtcsDevice *device=provider->getDevice(devNum2);
            MtcsMemAlloctor *alloctor = device->getMemAlloctor();
            return alloctor->memcpyAsync(dest,src,size,kind,mtcsStream);
         }
      }else if(!dHost && sHost){
         if(kind!=MtcsCpyKind.HOST2DEV){
            a_error("主机内存到设备内存的复制 kind:%d\n",kind);
         }else{
            MtcsProvider *provider = MtcsSystem.getProvider(providerNum1);
            MtcsDevice *device=provider->getDevice(devNum1);
            MtcsMemAlloctor *alloctor = device->getMemAlloctor();
            return alloctor->memcpyAsync(dest,src,size,kind,mtcsStream);
         }
      }else{
         if(providerNum2!=providerNum1){
            a_error("不支持跨供应商之间的内存复制。dest provider:%d src provider:%d",providerNum1,providerNum2);
            return NULL;
         }
         if(devNum1!=devNum2){
            a_error("不支持跨设备之间的内存复制。dest dev:%d src dev:%d",providerNum1,providerNum2);
            return NULL;
         }

         if(kind!=MtcsCpyKind.DEV2DEV){
            a_error("设备内存到设德内存的复制 kind:%d\n",kind);
         }else{
            MtcsProvider *provider = MtcsSystem.getProvider(providerNum1);
            MtcsDevice *device=provider->getDevice(devNum1);
            MtcsMemAlloctor *alloctor = device->getMemAlloctor();
            return alloctor->memcpyAsync(dest,src,size,kind,mtcsStream);
         }
      }
      return NULL;
   }

   public$ static void  free(void *data){
      if(data==NULL)
         return;
      int providerNum,devNum;
      MtcsMemMgr.getInstance()->get((unsigned long)data,&providerNum,&devNum);
      //printf("获取的供应商及设备---- free %d %d data:%p\n",providerNum,devNum,data);
      MtcsProvider *provider = MtcsSystem.getProvider(providerNum);
      MtcsDevice *device=provider->getDevice(devNum);
      MtcsMemAlloctor *alloctor = device->getMemAlloctor();
      alloctor->free(data);
      MtcsMemMgr.getInstance()->remove((unsigned long)data);
   }

};



