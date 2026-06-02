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

#include <cuda_runtime.h>
#include "CudaProvider.h"
#include "CudaDevice.h"
#include "cudamicro.h"
#include "../MtcsMemMgr.h"
#include "../MtcsSystem.h"

impl$  CudaProvider {

    CudaProvider(){
       setName(MtcsProvider.CUDA_NAME);
       cuInit(0);
       createDevices();
    }
    /**
     * 创建设备
     */
    private$ void createDevices() {
        int deviceCount = 0;
        CUDA_RUNTIME_CALL(cudaGetDeviceCount(&deviceCount));
        int i;
        for(i=0;i<deviceCount;i++){
           struct cudaDeviceProp deviceProp; //获取设备名称
           CUDA_RUNTIME_CALL(cudaGetDeviceProperties(&deviceProp, i));
           CudaDevice *d=new$ CudaDevice(deviceProp.name,i);
           printf("device name :%d %s\n",i,deviceProp.name);
           addDevice(d);
           d->unref();
        }
    }

    //实现父类的抽象方法
    public$  int  getDevice(){
        int n = 0;
        CUDA_RUNTIME_CALL(cudaGetDevice(&n));
        return n;
    }


};

void mtcs_cuda_init(int number)
{
   CudaProvider *provider=new$ CudaProvider();
   provider->setNumber(number);
   printf("mtcs_cuda_init 初始化 number:%d\n",number);
   //为供应商加入监听器
   provider->addListener(MtcsMemMgr.getInstance());
   MtcsDevice *device= provider->getDevice(0);
   //设为当前运行的设备
   device->setDevice();
   MtcsCompiler *compiler=device->getCompiler();
   compiler->compile();
   int binSize=0;
   char *bin=compiler->getBin(&binSize);
   MtcsModule *module=compiler->getModule();
   module->createModule(bin,binSize);
   MtcsSystem.registerProvider(provider);
}


