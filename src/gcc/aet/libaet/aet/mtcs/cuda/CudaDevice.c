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
#include "CudaDevice.h"
#include "CudaMemAlloctor.h"
#include "CudaLanucher.h"
#include "CudaCompiler.h"
#include "cudamicro.h"
#include "CudaStream.h"


impl$  CudaDevice {

   CudaDevice(char *name,int devNum){
      super$(name,devNum);
      //初始化 primaryContext
      CUDA_DRIVER_CALL(cuDeviceGet(&device, devNum));
      CUDA_DRIVER_CALL(cuDevicePrimaryCtxRetain(&primaryContext, device));
      CudaMemAlloctor *alloctor=new$ CudaMemAlloctor(devNum);
      CudaCompiler *compiler=new$ CudaCompiler(devNum);
      CudaLanucher *lancher=new$ CudaLanucher(devNum,(CudaModule *)compiler->getModule());
      printf("CudaDevice init --- compiler:%p\n",compiler);
      setAlloctor(alloctor);
      setLanucher(lancher);
      setCompiler(compiler);
      alloctor->unref();
      lancher->unref();
      compiler->unref();
   }

   /**
    * 实现抽象方法 setProviderNumber
    * num 供应商序号
    */
   public$ void setProviderNumber(int num){
     printf("在 cudadevice setProviderNumber %d\n",num);
     CudaMemAlloctor *alloctor=(CudaMemAlloctor *)getMemAlloctor();
     alloctor->setProviderNumber(num);
     CudaLanucher *lancher=(CudaLanucher *)getLanucher();
     lancher->setProviderNumber(num);
   }

   //实现MtcsDevice的抽象方法
   public$ MtcsStream *createStream(){
       int devNum =getNumber();
       CUDA_RUNTIME_CALL(cudaSetDevice(devNum));
       CudaStream *stream=new$ CudaStream(devNum);
       return stream;
   }

   public$ MtcsStream *createStream(int flag){
      int devNum =getNumber();
       CUDA_RUNTIME_CALL(cudaSetDevice(devNum));
       CudaStream *stream=new$ CudaStream(devNum,flag);
       return stream;
   }

   public$ aint64 getMemFree(){
      CUDA_RUNTIME_CALL(cudaSetDevice(getNumber()));
      size_t free=0;
      size_t total=0;
      CUDA_RUNTIME_CALL(cudaMemGetInfo(&free, &total));
      return free;
   }

   public$ aint64 getMemTotal(){
      CUDA_RUNTIME_CALL(cudaSetDevice(getNumber()));
      size_t free=0;
      size_t total=0;
      CUDA_RUNTIME_CALL(cudaMemGetInfo(&free, &total));
      return total;
   }

   /**
    * 从 CudaProvider mtcs_cuda_init 加入listener
    * mtcs_cuda_init
    *    -->addListener(MtcsProvider)
    *       -->addListener(MtcsDevice)
    *          -->addListener(CudaMemAllocator)
    */
   public$ void  addListener(MtcsEventListener *listener){
       CudaMemAlloctor *alloctor=(CudaMemAlloctor *)getMemAlloctor();
       alloctor->addListener(listener);
       CudaLanucher *lancher=(CudaLanucher *)getLanucher();
       lancher->addListener(listener);
   }

   public$ void  removeListener(MtcsEventListener *listener){
      CudaMemAlloctor *alloctor=(CudaMemAlloctor *)getMemAlloctor();
      alloctor->removeListener(listener);
      CudaLanucher *lancher=(CudaLanucher *)getLanucher();
      lancher->removeListener(listener);
   }

   //把设备设为当前线程的运行设备 返回0成功，否则失败。
   public$ int setDevice(){
      printf("setdevice -- %d\n",getNumber());
      CUDA_RUNTIME_CALL(cudaSetDevice(getNumber()));
      CUDA_RUNTIME_CALL(cudaSetDeviceFlags(cudaDeviceScheduleBlockingSync));
      return 0;
   }

};



