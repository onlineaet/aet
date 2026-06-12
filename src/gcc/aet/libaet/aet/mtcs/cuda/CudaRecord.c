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
#include "CudaRecord.h"
#include "cudamicro.h"
#include <aet/time/Time.h>


impl$  CudaRecord {

   CudaRecord(int devNum){
      self->devNum = devNum;
      cudaEvent_t t=NULL;
      cudaEvent_t t1=NULL;
      CUDA_RUNTIME_CALL(cudaEventCreate(&t));
      CUDA_RUNTIME_CALL(cudaEventCreate(&t1));
      start=(void*)t;
      stop=(void *)t1;
   }


   aboolean start(){
     aboolean re=super$->start();
     if(!re){
       a_error("上一个记录还未停止");
     }
     cudaStream_t cs=stream?(cudaStream_t)stream->getStream():NULL;
     CUDA_RUNTIME_CALL(cudaEventRecord((cudaEvent_t)start,cs));
     return TRUE;
   }

   aboolean stop(){
      aboolean re=super$->stop();
      if(!re){
         a_error("记录还未开始");
      }
      cudaStream_t cs=stream?(cudaStream_t)stream->getStream():NULL;
      CUDA_RUNTIME_CALL(cudaEventRecord((cudaEvent_t)stop,cs));
      CUDA_RUNTIME_CALL(cudaEventSynchronize((cudaEvent_t)stop));
      return TRUE;
   }

   public$ float getElapsedTime(){
      float ms;
      CUDA_RUNTIME_CALL(cudaEventElapsedTime(&ms, start, stop));
      return ms;
   }

   ~CudaRecord(){
      if(start && stop){
         CUDA_RUNTIME_CALL(cudaEventDestroy((cudaEvent_t)start));
         CUDA_RUNTIME_CALL(cudaEventDestroy((cudaEvent_t)stop));
         start=NULL;
         stop = NULL;
      }
   }

};



