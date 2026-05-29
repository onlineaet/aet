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
#include "cudamicro.h"
#include "CudaCompiler.h"
#include "CudaModule.h"
#include "../ElfFile.h"

#define NVPTXCOMPILER_SAFE_CALL(x)                                       \
    do {                                                                 \
        nvPTXCompileResult result = x;                                   \
        if (result != NVPTXCOMPILE_SUCCESS) {                            \
            printf("error: %s failed with error code %d\n", #x, result); \
            exit(1);                                                     \
        }                                                                \
    } while(0)


#define NVJITLINK_SAFE_CALL(h,x)                                  \
  do {                                                            \
    nvJitLinkResult result = x;                                   \
    if (result != NVJITLINK_SUCCESS) {                            \
      printf("error: %s failed with error \n", #x);        \
      size_t lsize;                                               \
      result = nvJitLinkGetErrorLogSize(h, &lsize);               \
      if (result == NVJITLINK_SUCCESS && lsize > 0) {             \
        char *log = (char*)malloc(lsize);                         \
          result = nvJitLinkGetErrorLog(h, log);                        \
          if (result == NVJITLINK_SUCCESS) {                            \
             printf("error: %s failed with error xxx %s\n", #x, log);       \
            free(log);                                                  \
          }                                                             \
      }                                                           \
      exit(1);                                                    \
    }                                                             \
  } while(0)


impl$  CudaCompiler {

   CudaCompiler(int devNum){
        self->devNum = devNum;
        self->cubin=NULL;
        self->cubinSize=0;
        self->mtcsModule=new$ CudaModule(devNum);
   }

   char *compile(AArray *ptxCodes,int *binSize){
      CUdevice cuDevice;
      //设置设备 Runtime API 会自动管理上下文
      CUDA_DRIVER_CALL(cuDeviceGet(&cuDevice, devNum));
      struct cudaDeviceProp prop;
      CUDA_RUNTIME_CALL(cudaGetDeviceProperties(&prop, devNum));       // 获取GPU的特性，看是否支持地址映射
      if (!prop.canMapHostMemory){
         printf("managedMemory 失败--- %d\n",prop.managedMemory);
         abort();
      }
      // Load the generated LTO IR and the LTO IR generated offline
      // and link them together.
      nvJitLinkHandle handle;
      // Dynamically determine the arch to link for
      int major = 0;
      int minor = 0;
      CUDA_DRIVER_CALL(cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, cuDevice));
      CUDA_DRIVER_CALL(cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, cuDevice));
      int arch =  major*10 + minor;
      printf("arch is :%d ptxCodes->size():%d\n",arch,ptxCodes->size());
      char smbuf[100];
      sprintf(smbuf, "-arch=sm_%d", arch);
      //const char *lopts[] = {"-lto", smbuf};
      //const char *lopts[] = {smbuf,"-O3","-split-compile=0","-maxrregcount=512","-no-cache","-fma=3"};
      const char *lopts[] = {smbuf,"-O3"};
      //const char *lopts[] = {smbuf};

      NVJITLINK_SAFE_CALL(handle, nvJitLinkCreate(&handle, 2, lopts));

      int i;
      int files=ptxCodes->size();
      for(i=0;i<files;i++){
         char *code = ptxCodes->get(i);
         char name[255];
         sprintf(name,"ptx_%d",i);
         printf("ptx -- i:%d codes:\n%s\n\n",i,code);
         NVJITLINK_SAFE_CALL(handle, nvJitLinkAddData(handle, NVJITLINK_INPUT_PTX,(void *)code,strlen(code), name));
      }

#if 0
      char buffer[4096];
      readFile("/home/sns/a.ptx",buffer,4096);
      printf("code is 00 :%s\n",buffer);
      NVJITLINK_SAFE_CALL(handle, nvJitLinkAddData(handle, NVJITLINK_INPUT_PTX,(void *)buffer,strlen(buffer), "testptx1"));
      readFile("/home/sns/b.ptx",buffer,4096);
      printf("code is 11 :%s\n",buffer);
      NVJITLINK_SAFE_CALL(handle, nvJitLinkAddData(handle, NVJITLINK_INPUT_PTX,(void *)buffer,strlen(buffer), "testptx2"));

#endif

      // The call to nvJitLinkComplete causes linker to link together the two
      // LTO IR modules (offline and online), do optimization on the linked LTO IR,
      // and generate cubin from it.
      NVJITLINK_SAFE_CALL(handle, nvJitLinkComplete(handle));
      size_t cubinSize;
      NVJITLINK_SAFE_CALL(handle, nvJitLinkGetLinkedCubinSize(handle, &cubinSize));
      void *cubin = malloc(cubinSize);
      NVJITLINK_SAFE_CALL(handle, nvJitLinkGetLinkedCubin(handle, cubin));
      NVJITLINK_SAFE_CALL(handle, nvJitLinkDestroy(&handle));
      *binSize = cubinSize;
      return cubin;
   }

   void readFile(char *fileName,char *buffer,int len){
      FILE *f=fopen(fileName,"r");
      size_t rev=fread(buffer,1,len,f);
      buffer[rev]='\0';
   }

   /**
    * 从当前可执行文件取ptx汇编代码
    */
   private$ aboolean getProcessFile(char *processDir,int len){
      char *fileName;
      ssize_t size=readlink("/proc/self/exe",processDir,len);
      if(size<=0){
        return FALSE;
      }
      processDir[size]='\0';
      printf("程序路径:%s\n",processDir);
      return TRUE;
   }

   AArray *getPtxCode(char *fileName){
      printf("获取文件的ptx汇编代码 :%s\n",fileName);
      ElfFile *elfFile=new$ ElfFile(fileName);
      AArray *ret =elfFile->getCode("cuda");
      elfFile->unref();
      return ret;
   }

   public$ void compile(){
      if(cubin==NULL){
         char processDir[1024];
         getProcessFile(processDir,1024);
         AArray *codes=getPtxCode(processDir);
         cubin=compile(codes,&self->cubinSize);
         codes->unref();
         printf("compile size:%d %p\n",self->cubinSize,cubin);
         setBin(cubin,cubinSize);
      }
   }

   public$ char *getName(){
      return "cuda";
   }


};



