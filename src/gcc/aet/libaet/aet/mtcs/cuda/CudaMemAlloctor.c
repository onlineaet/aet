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
#include <pthread.h>
#include <cuda_runtime.h>
#include "CudaMemAlloctor.h"
#include "cudamicro.h"
#include "../MtcsEvent.h"
#include "../MtcsMem.h"


impl$  CudaMemAlloctor {

   CudaMemAlloctor(int devNum){
      self->devNum = devNum;
      self->level = getUnifiedMemoryLevel(devNum);
   }

   void setProviderNumber(int providerNum){
      self->providerNum = providerNum;
   }

   /**
    * 启动分配事件
    */
   void fireMemEvent(unsigned long address,auint64 size){
      int i;
      for(i=0;i<listenersCount;i++){
         MtcsEvent event=new$ MtcsEvent(MtcsEvent.EventType.MALLOC,providerNum,devNum);
         //printf("memCallback_cb --- 启动事件 %d %d %d %p\n",listenersCount,providerNum,devNum,address);
         event.address=address;
         event.size = size;
         listeners[i]->mallocHappend(&event);
      }
   }

   /**
    * 分配在设备devNum上的全局内存即显存
    * 实现MtcsMemAlloctor 接口
    */
   public$ void *malloc(size_t size,aboolean isDevice){
      void *data=NULL;
      if(!isDevice){
         if(level==UNIFIED_MEMORY_FULL || level==UNIFIED_MEMORY_MANAGED ){
             CUDA_RUNTIME_CALL(cudaMallocManaged(&data, size,cudaMemAttachGlobal));
         }else
             perror("不支持统一内存");
      }else{
         CUDA_RUNTIME_CALL(cudaMalloc(&data, size));
      }
      if(data)
         fireMemEvent((unsigned long)data,size);
      return data;
   }

   public$ void *calloc(size_t size,aboolean isDevice){
     // printf("cudamemalloctor.c calloc :devNum:%d size:%d level:%d isDevice:%d\n",devNum,size,level,isDevice);
      void *data=NULL;
      if(!isDevice){
         if(level==UNIFIED_MEMORY_FULL || level==UNIFIED_MEMORY_MANAGED ){
             CUDA_RUNTIME_CALL(cudaMallocManaged(&data, size,cudaMemAttachGlobal));
         }else
             perror("不支持统一内存");
      }else{
         CUDA_RUNTIME_CALL(cudaMalloc(&data, size));
      }
      if(data){
         CUDA_RUNTIME_CALL(cudaMemset(data, 0, size));
         fireMemEvent((unsigned long)data,size);
      }
      return data;
   }


   public$ void *memset(void *data,int c,size_t n ){
      CUDA_RUNTIME_CALL(cudaMemset(data, c, n));
      return data;
   }

   public$ void *memcpy(void *dest,void *src,size_t size ,int direct){
      if(direct==MtcsCpyKind.HOST2DEV)
         CUDA_RUNTIME_CALL(cudaMemcpy(dest, src, size, cudaMemcpyHostToDevice));
      else if(direct==MtcsCpyKind.DEV2HOST)
         CUDA_RUNTIME_CALL(cudaMemcpy(dest, src, size, cudaMemcpyDeviceToHost));
      else if(direct==MtcsCpyKind.DEV2DEV)
         CUDA_RUNTIME_CALL(cudaMemcpy(dest, src, size, cudaMemcpyDeviceToDevice));
      return dest;
   }

   public$ void *memcpyAsync(void *dest,void *src,size_t size ,int direct,MtcsStream *mtcsStream){
      cudaStream_t stream=(cudaStream_t)(mtcsStream!=NULL?(mtcsStream->getStream()):NULL);
     // if(mtcsStream)
        // stream = mtcsStream->stream;
      if(direct==MtcsCpyKind.HOST2DEV)
         CUDA_RUNTIME_CALL(cudaMemcpyAsync(dest, src, size, cudaMemcpyHostToDevice,stream));
      else if(direct==MtcsCpyKind.DEV2HOST)
         CUDA_RUNTIME_CALL(cudaMemcpyAsync(dest, src, size, cudaMemcpyDeviceToHost,stream));
      else if(direct==MtcsCpyKind.DEV2DEV)
         CUDA_RUNTIME_CALL(cudaMemcpyAsync(dest, src, size, cudaMemcpyDeviceToDevice,stream));
      return dest;
   }

   /**
    * 实现MtcsMemAlloctor 接口 free方法。
    */
   public$  void free(void *data){
      if(data==NULL)
         return;
      struct cudaPointerAttributes attr;
      CUDA_RUNTIME_CALL(cudaPointerGetAttributes(&attr, data));
      switch(attr.type) {
         case cudaMemoryTypeHost:
            //"Unified: CUDA Host or Registered Memory" :
            CUDA_RUNTIME_CALL(cudaFreeHost(data));
            break;
         case cudaMemoryTypeDevice:
            //"Not Unified: CUDA Device Memory";
            CUDA_RUNTIME_CALL(cudaFree(data));
            break;

         case cudaMemoryTypeManaged:
            //"Not Unified: CUDA Managed Memory";
            CUDA_RUNTIME_CALL(cudaFree(data));
            break;
         case cudaMemoryTypeUnregistered:
            //"Unified: System-Allocated Memory 对应 主机 malloc" :
            //__managed__ int managed_var = 5;的类型也是cudaMemoryTypeUnregistered
            free(data);
            break;
         default:
            CUDA_RUNTIME_CALL(cudaFree(data));
            break;
      }
   }

   /**
    * 返回内存所在的设备 实现接口 MtcsMemAlloctor
    */
   int getDevice(unsigned long address){
      CUDA_RUNTIME_CALL(cudaSetDevice(devNum));
      struct cudaPointerAttributes attr;
      CUDA_RUNTIME_CALL(cudaPointerGetAttributes(&attr, (void *)address));
      //printf("attr.device --- device:%d %p %p type:%d\n",attr.device,attr.devicePointer,attr.hostPointer,attr.type);
      return attr.device;
   }

   //获取设备的统一内存级别，共有4级
   private$ int getUnifiedMemoryLevel(int devNum){
      cudaGetDevice(&devNum);
      int pma = 0;
      cudaDeviceGetAttribute(&pma, cudaDevAttrPageableMemoryAccess, devNum);
      printf("Full Unified Memory Support: %s\n", pma == 1? "YES" : "NO");
      int cma = 0;
      cudaDeviceGetAttribute(&cma, cudaDevAttrConcurrentManagedAccess, devNum);
      printf("CUDA Managed Memory with full support: %s\n", cma == 1? "YES" : "NO");
      int device =devNum;
      struct cudaDeviceProp prop;
      cudaGetDeviceProperties(&prop, device);
      printf("CUDA device properties pageableMemoryAccess: %d\n", prop.pageableMemoryAccess);
      printf("CUDA device properties hostNativeAtomicSupported: %d\n", prop.hostNativeAtomicSupported);
      printf("CUDA device properties pageableMemoryAccessUsesHostPageTables: %d\n", prop.pageableMemoryAccessUsesHostPageTables);
      printf("CUDA device properties directManagedMemAccessFromHost: %d\n", prop.directManagedMemAccessFromHost);
      printf("CUDA device properties concurrentManagedAccess: %d\n", prop.concurrentManagedAccess);
      printf("CUDA device properties managedMemory: %d\n", prop.managedMemory);
      printf("CUDA device properties concurrentManagedAccess: %d\n", prop.concurrentManagedAccess);
      printf("CUDA device properties managedMemory: %d\n", prop.managedMemory);
      if(pma==1)
         return UNIFIED_MEMORY_FULL;
      else if(cma==1)
         return UNIFIED_MEMORY_MANAGED;
      return -1;
   }

   private$ aboolean exists(MtcsEventListener *listener){
      int i;
      for(i=0;i<listenersCount;i++)
         if(listeners[i]==listener)
            return TRUE;
      return FALSE;
   }

   public$ void  addListener(MtcsEventListener *listener){
      if(!exists(listener))
        listeners[listenersCount++]=listener->ref();
   }

   public$ void  removeListener(MtcsEventListener *listener){

   }

};



