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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <cuda.h>
#include <nvPTXCompiler.h>
#include <cuda_runtime.h>
#include <nvJitLink.h>
#include <aet/time/Time.h>
#include "CudaLanucher.h"
#include "cudamicro.h"
#include "../MtcsStream.h"

#include "../ElfFile.h"

impl$  CudaLanucher {

   CudaLanucher(int devNum,CudaModule *cudaModule){
      self->devNum = devNum;
      self->cubin=NULL;
      self->cubinSize=0;
      self->cudaModule=cudaModule->ref();
      self->funcHash=new$ AHashTable(AHashTable.strHash,AHashTable.strEqual);
   }

   public$  void lanuch(char *funcName,auint gridX,auint gridY,auint gridZ,
         auint blockX,auint blockY,auint blockZ,auint sharedMemBytes,void *hStream,void **kernelParams,void **extra){
      //找到设备对应的驱动
      //printf("CudaLanucher lanuch funcName 00 funcName:%s kernel:%p mtcsStreamData:%p\n",funcName,kernel,hStream);
      CUfunction kernel = funcHash->get(funcName);
      //printf("lanuch kernel--:%s %p\n",funcName,kernel);
      if(kernel==NULL){
         CUmodule module=cudaModule->getModule();
         CUDA_DRIVER_CALL(cuModuleGetFunction(&kernel, module, funcName));//得到函数
         funcHash->put(strdup(funcName),kernel);
      }
      cudaStream_t stream=NULL;
      if(hStream!=NULL){
         MtcsStream *data=(MtcsStream *)hStream;
         stream=(cudaStream_t)data->getStream();
      }
      //printf("CudaLanucher lanuch funcName 11 funcName:%s kernel:%p stream:%p grid:%dx%dx%d block:%dx%dx%d\n",
            //funcName,kernel,stream,gridX,  gridY, gridZ,blockX, blockY, blockZ);
      CUDA_DRIVER_CALL( cuLaunchKernel(kernel,
      gridX,  gridY, gridZ, // grid dim
      blockX, blockY, blockZ, // block dim
      sharedMemBytes, stream, // shared mem and stream
      kernelParams, extra)); // arguments
   }
   
   /**
    * 实现MtcsLanucher中定义的接口
    */
   public$ char *getName(){
      return "cuda";
   }
   
   void setProviderNumber(int providerNum){
      self->providerNum = providerNum;
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

   /**
    * 实现MtcsLanucher接口
    */
   public$ void setBin(char *bin,int size){
      self->cubin=bin;
      self->cubinSize=size;
   }
};



