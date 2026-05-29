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

#include <cuda.h>
#include <cuda_runtime.h>
#include "CudaModule.h"
#include "cudamicro.h"

impl$  CudaModule {

   CudaModule(int devNum){
      self->devNum=devNum;
      self->module=NULL;
   }

   CUmodule getModule(){
      return module;
   }

   /**
   * 实现抽象方法 createModule
   */
   public$ aboolean createModule(void *bin,int size){
      printf("cudamodule.c createModule devNum:%d bin:%p size:%d\n",devNum,bin,size);
      CUDA_RUNTIME_CALL(cudaSetDevice(devNum));
      CUDA_DRIVER_CALL(cuModuleLoadData(&module, bin));
      return TRUE;
   }

   /**
   * 实现父类的抽象方法 copyAddressDToH
   */
   public$ void copyAddressDToH(void *host,char *devicePointerVarName,int element){
      printf("CudaModule.c copyAddressDToH 获取设备函数地址 00 devNum:%d devicePointerVarName：%s element:%d\n",
            devNum,devicePointerVarName,element);
      CUDA_RUNTIME_CALL(cudaSetDevice(devNum));
      if(module==0)
         a_error("模块还未创建。");

      CUdeviceptr dptr;
      size_t bsize;
      CUDA_DRIVER_CALL( cuModuleGetGlobal (&dptr, &bsize, module, devicePointerVarName));
      printf("CudaModule.c copyAddressDToH 获取设备函数地址 11 dptr:%p size:%d\n",dptr,bsize);
      CUDA_DRIVER_CALL(cuMemcpyDtoH(host, dptr+sizeof(void*)*element, sizeof(void*)));
      printf("CudaModule.c copyAddressDToH 获取设备函数地址 22 host:%p\n",host);
   }

   void copyAddressDToH(unsigned long *hostParentDeviceAddress,int count,char *destVarName){
      if(hostParentDeviceAddress==NULL || count==0)
         return;

      CUDA_RUNTIME_CALL(cudaSetDevice(devNum));
      printf("CudaModule copyAddressDToH 00 获取设备函数地址 host:%p dest:%s count:%d\n",hostParentDeviceAddress,destVarName,count);
      CUdeviceptr destptr;
      size_t destsize;
      CUDA_DRIVER_CALL( cuModuleGetGlobal (&destptr, &destsize, module, destVarName));
      int psize=sizeof(void*);
      int i;
      for(i=0;i<count;i++){
         char *item=(char*)hostParentDeviceAddress[i];
         if(item==NULL)
            continue;
         char *suffix=strstr(item,"_");
         char indexStr[10];
         strncpy(indexStr,item,strlen(item)-strlen(suffix));
         indexStr[strlen(item)-strlen(suffix)]='\0';
         char *deviceFuncPointersVarName=suffix+1;
         int srcPos=atoi(indexStr);
         printf("CudaModule copyAddressDToH 11 获取设备函数地址 i:%d suffix:%s srcPos:%d deviceFuncPointersVarName:%s\n",
               i,suffix,srcPos,deviceFuncPointersVarName);
         CUdeviceptr srcptr;
         size_t srcsize;
         CUDA_DRIVER_CALL(cuModuleGetGlobal (&srcptr, &srcsize, module, deviceFuncPointersVarName));
         CUDA_DRIVER_CALL(cuMemcpyDtoD(destptr+i*psize, srcptr+psize*srcPos, psize));
      }
   }


};



