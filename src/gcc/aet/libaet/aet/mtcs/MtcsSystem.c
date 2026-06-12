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

#include <dlfcn.h>

#include "MtcsSystem.h"
#include "MtcsMem.h"
#include "MtcsMemMgr.h"

#if TEST_CUDA
#include "cuda/CudaProvider.h"
#include "cuda/CudaCompiler.h"
#endif

typedef void (*mtcs_plat_init)(int number);

impl$  MtcsSystem {

   public$  static void lanuch(AObject *inClassObj,int staticFunc,char *funcName,dim3 grid,dim3 block,
         auint sharedMemBytes,void *hStream,void **kernelParams,void **extra){
      aboolean find=FALSE;//find用户设的平台类型和设备号
      int platform=0;
      int devNum=0;
      AObject *obj=NULL;
      if(staticFunc){ //表示调用的是类的静态核函数，没有self对象
         if(inClassObj){
            AClass *cl=inClassObj->getClass();
            if(cl->isMtcsClass()){
               platform = (unsigned char)((inClassObj->mtcsPlatformType >> 24) & 0xFF);
               devNum  = (unsigned char)((inClassObj->mtcsPlatformType >> 16) & 0xFF);
               find=TRUE;
            }
         }
      }else{
         void **parm1=(void**)kernelParams[0];
         obj=(AObject *)(*parm1);
         //printf("self is --- %p %u\n",obj,obj->mtcsPlatformType);
         platform = (unsigned char)((obj->mtcsPlatformType >> 24) & 0xFF);
         devNum  = (unsigned char)((obj->mtcsPlatformType >> 16) & 0xFF);
         find=TRUE;
      }

      void *cloneObject=NULL;
      if(obj){
         if(obj->objectSize==0){
            //说明是栈内存对象
            AClass *cl=obj->getClass();
            asize size=cl->getSize();
            MtcsMallocAttribute att={platform,devNum,FALSE};
            cloneObject = MtcsMem.malloc(size,att);
            MtcsMem.memcpy(cloneObject,(void*)obj,size,MtcsCpyKind.HOST2DEV);
            kernelParams[0]=&cloneObject;
            printf("MtcsSystem.c lanuch 88 栈对象复制到托管内存了--- 对象大小:%d\n",size);
         }
      }
//      printf("lanuch 00 -- inClassObj:%p staticFunc:%d funcName:%s grid(%d,%d,%d) block(%d,%d,%d)\n",
//            inClassObj,staticFunc,funcName,grid.x,grid.y,grid.z,block.x,block.y,block.z);
//      printf("lanuch 11 -- find:%d platform:%d devNum:%d sharedMemBytes:%d hStream:%p\n",
//            find,platform,devNum,sharedMemBytes,hStream);
      MtcsProvider *provider=getProvider(platform);
      MtcsDevice *device=provider->getDevice(devNum);
      MtcsLanucher *lanucher=device->getLanucher();
      lanucher->lanuch(funcName,grid.x,grid.y,grid.z,block.x,block.y,block.z,sharedMemBytes,hStream,kernelParams,extra);
      if(cloneObject){
         MtcsMem.free(cloneObject);
         kernelParams[0]=&obj;
      }
   }

   public$ static MtcsDevice *getDevice(char *providerName,int devNum){
      int i;
      for(i=0;i<providerCount;i++){
         MtcsProvider *provider=mtcsProviders[i];
         if(strcmp(provider->getName(),providerName)==0)
            return  provider->getDevice(devNum);
      }
      return NULL;
   }

   public$ static MtcsProvider *getProvider(char *name){
      int i;
      if(name==NULL)
         return  mtcsProviders[0];
      for(i=0;i<providerCount;i++){
         if(strcmp(mtcsProviders[i]->getName(),name)==0)
            return mtcsProviders[i];
      }
      printf("错误，没有供应商:%s\n",name);
      return NULL;
   }

   /**
    * 如果defaultProvider是空的 0号供应商就是缺省的供应商
    */
   public$  static MtcsProvider *getDefaultProvider(){
      if(!defaultProvider);
         return mtcsProviders[0];
      return defaultProvider;
   }

   public$ static MtcsProvider *getProvider(int index){
      if(index<0 || index>=providerCount){
         printf("出错了 index:%d 供应商数:%d %s\n",index,providerCount,__FUNCTION__);
         abort();
      }
      return mtcsProviders[index];
   }

   static void loadProviderFromLib(){
      void * lib = dlopen("libaet_cuda.so", RTLD_LAZY);
      if(lib == NULL){
         //printf("dlopen failed_%s\n", dlerror());
         return;
      }
      mtcs_plat_init mtcsPlatInit = (mtcs_plat_init)dlsym(lib, "mtcs_cuda_init");
      mtcsPlatInit(0);
      dlclose(lib);
   }

   /**
    * 初始化供应商
    */
   public$ static void init(){
#if TEST_CUDA
      mtcs_cuda_init(0);
      setDefaultProvider("cuda");
#else
      loadProviderFromLib();
#endif
   }

   /**
    * 由平台实始化时调用该方法来注册自已到MtcsSystem
    */
   public$ static void registerProvider(MtcsProvider *provider){
      mtcsProviders[providerCount++]=provider;
   }

   public$ static void  setDefaultProvider(char *platName){
      int i;
      for(i=0;i<providerCount;i++){
         MtcsProvider *pri=mtcsProviders[i];
         if(strcmp(pri->getName(),platName)==0){
            defaultProvider=pri;
            return;
         }
      }
   }

   public$ static  void synchronize(){
      MtcsProvider *provider = getDefaultProvider();
      MtcsDevice *df=provider->getDefaultDevice();
      df->synchronize();
   }

   //从缺省的供应商获取当前设备号
   public$ static int  getDevice(){
      MtcsProvider *provider = getDefaultProvider();
      return provider->getDevice();
   }

   //在当前线程中设定供应商和设备
   public$ static int   setDevice(int provider,int devNum){
      if(provider<0 || provider>providerCount)
         a_error("未知供应商:%d\n",provider);
      MtcsProvider *pri=mtcsProviders[provider];
      MtcsDevice *device=pri->getDevice(devNum);
      return device->setDevice();
   }
};

/**
 * 实现AObject.h声明的函数 为MTCS类分配内存
 */
void *mtcs_alloc_object(int size,int platformType)
{
   unsigned char  platform = (unsigned char)((platformType >> 24) & 0xFF);
   unsigned char  devNum  = (unsigned char)((platformType >> 16) & 0xFF);
   MtcsProvider *provider=MtcsSystem.getProvider(platform);
   MtcsDevice *device=provider->getDevice(devNum);
   MtcsMemAlloctor *memAlloctor=device->getMemAlloctor();
   void *ret= memAlloctor->calloc(size,FALSE);
   return ret;
}

/**
 * 实现AObject.h声明的函数 释放 mtcs_alloc_object 分配的内存
 */
void mtcs_free_object(void *obj)
{
   MtcsMem.free(obj);
}

/**
 * 复制mtcs模块中的设备函数地址到主机
 * 在AObject.h中声明，由编译器生成的代码中调用
 */
void mtcs_copy_device_func_address(void *dest,char *devicePointerVarName,int element,int platformType)
{
   unsigned char  platform = (unsigned char)((platformType >> 24) & 0xFF);
   unsigned char  devNum  = (unsigned char)((platformType >> 16) & 0xFF);
   printf("mtcs_copy_device_func_address devNum:%d platform:%d devicePointerVarName:%s elem:%d\n",devNum,platform,devicePointerVarName,element);
   MtcsProvider *provider=MtcsSystem.getProvider(platform);
   MtcsDevice *device=provider->getDevice(devNum);
   MtcsCompiler *compiler=device->getCompiler();
   MtcsModule *module=compiler->getModule();
   module->copyAddressDToH(dest,devicePointerVarName,element);
}

void mtcs_copy_device_address_to_super(unsigned long *hostParentDeviceAddress,int count,char *destVarName,int platformType)
{
   if(hostParentDeviceAddress==NULL || count==0)
      return;
   unsigned char  platform = (unsigned char)((platformType >> 24) & 0xFF);
   unsigned char  devNum  = (unsigned char)((platformType >> 16) & 0xFF);
   MtcsProvider *provider=MtcsSystem.getProvider(platform);
   MtcsDevice *device=provider->getDevice(devNum);
   MtcsCompiler *compiler=device->getCompiler();
   MtcsModule *module=compiler->getModule();
   module->copyAddressDToH(hostParentDeviceAddress,count,destVarName);
}

static volatile int initLibrary=0;

// 优先级可以是 0-65535，数字越小优先级越高，不写默认在普通构造函数后执行
__attribute__((constructor(1))) void mtcs_library_init(void)
{
   if(!({int v1=1;int v2=1;__atomic_exchange(&initLibrary,&v1,&v2,__ATOMIC_SEQ_CST);v2;})){
      //printf("库已被加载，这是第一个执行的函数！\n");
      MtcsSystem.init();
   }
}

