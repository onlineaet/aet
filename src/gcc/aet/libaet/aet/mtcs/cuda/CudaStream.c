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
#include "CudaStream.h"
#include "cudamicro.h"


impl$  CudaStream {

   CudaStream(int devNum){
      self->devNum = devNum;
      cudaStream_t stream;
      cudaError_t status = cudaStreamCreate(&stream);
      if (status != cudaSuccess) {
         const char *s = cudaGetErrorString(status);
         printf("cudaStreamCreate Error status:%d %s\n",status, s);
         status = cudaStreamCreateWithFlags(&stream, cudaStreamNonBlocking);    // cudaStreamDefault
         CUDA_RUNTIME_CALL(status);
      }
      self->stream=(void*)stream;
   }

   CudaStream(int devNum,int flag){
      self->devNum = devNum;
      // CUDA_RUNTIME_CALL(cudaSetDevice(devNum));
      cudaStream_t stream;
      cudaError_t status = cudaStreamCreateWithFlags(&stream,flag);
      if (status != cudaSuccess) {
         const char *s = cudaGetErrorString(status);
         printf("cudaStreamCreate Error status:%d %s flag:%d\n",status, s,flag);
         status = cudaStreamCreateWithFlags(&stream, cudaStreamNonBlocking);    // cudaStreamDefault
         CUDA_RUNTIME_CALL(status);
      }
      self->stream=(void*)stream;
   }

   public$ void sync(){
      CUDA_RUNTIME_CALL(cudaStreamSynchronize((cudaStream_t)stream));
   }

   ~CudaStream(){
      CUDA_RUNTIME_CALL(cudaStreamDestroy((cudaStream_t)stream));
      stream=NULL;
   }

};



