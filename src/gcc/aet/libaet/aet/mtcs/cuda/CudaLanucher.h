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

#ifndef __AET_CUDA_LANUCHER_H__
#define __AET_CUDA_LANUCHER_H__

#include "../../../aet.h"
#include "../../util/AHashTable.h"
#include "../MtcsLanucher.h"
#include "../MtcsEventListener.h"
#include "CudaModule.h"

package$ aet.mtcs.cuda;

public$  class$ CudaLanucher implements$ MtcsLanucher{

   int providerNum;
   int devNum;
   char *cubin;//二进制代码
   int cubinSize;//大小
   private$ AHashTable *funcHash;
   private$ CudaModule *cudaModule;
   private$ MtcsEventListener *listeners[20];
   private$ int listenersCount;
   CudaLanucher(int devNum,CudaModule *cudaModule);
   void setProviderNumber(int providerNum);
   public$ void  addListener(MtcsEventListener *listener);
   public$ void  removeListener(MtcsEventListener *listener);


};

#endif /* __N_MEM_H__ */

