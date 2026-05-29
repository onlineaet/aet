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

#ifndef __AET_MTCS_SYSTEM_H__
#define __AET_MTCS_SYSTEM_H__

#include "../../aet.h"
#include "MtcsProvider.h"

package$ aet.mtcs;

#define MAX_PROVIDER 10 //最大供应商数

public$ final$  class$ MtcsSystem{

   public$  static MtcsProvider *mtcsProviders[MAX_PROVIDER];
   public$  static unsigned int providerCount = 0;
   public$  static MtcsProvider *defaultProvider=NULL;
   public$  static void init();
   //用加载动态库的方法，调用平台的初始化方法。
   private$ static void loadProviderFromLib();

   //编译器定义的MTCS平台类型
   public$ enum$ MtcsPlatType{
      MTCS_PLAT_DEFAULT,
      MTCS_PLAT_CUDA,
      MTCS_PLAT_GCN,
      MTCS_PLAT_SPIRV,
      MTCS_PLAT_UNKNOWN
   };

   //由AET生成调用指令，不能直接调用
   public$  static void lanuch(AObject *inClassObj,int staticFunc,char *funcName,dim3 grid,dim3 block,
         auint sharedMemBytes,void *hStream,void **kernelParams,void **extra);
   /**
   * 取取缺省供应商
   */
   public$ static MtcsProvider *getDefaultProvider();
   public$ static MtcsProvider *getProvider(char *name);
   public$ static MtcsProvider *getProvider(int index);
   public$ static void          registerProvider(MtcsProvider *provider);
   public$ static void          setDefaultProvider(char *platName);
   //从缺省的供应商获取当前设备号
   public$ static int           getDevice();
   //在当前线程中设定供应商和设备
   public$ static int           setDevice(int provider,int devNum);
};

#endif /* __N_MEM_H__ */

