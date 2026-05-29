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

#ifndef __AET_MTCS_DEVICE_H__
#define __AET_MTCS_DEVICE_H__

#include "../../aet.h"
#include "MtcsMemAlloctor.h"
#include "MtcsLanucher.h"
#include "MtcsCompiler.h"
#include "MtcsEventListener.h"
#include "MtcsStream.h"

package$ aet.mtcs;

public$  abstract$ class$ MtcsDevice{
   private$  char *name;
   private$  char *vendor;
   private$  char *description;
   private$  char *version;
   private$  int   devNum;//卡号
   private$  MtcsMemAlloctor *alloctor;//设备内存分配器
   private$  MtcsLanucher *lanucher;//内核启动器
   private$  MtcsCompiler *compiler;//mtcs编译器

   public$ MtcsDevice(const char *name,int devNum);
   public$ void setAlloctor(MtcsMemAlloctor *alloctor);
   public$ void setLanucher(MtcsLanucher *lanucher);
   public$ void setCompiler(MtcsCompiler *compiler);

   public$ MtcsMemAlloctor *getMemAlloctor();
   public$ MtcsLanucher    *getLanucher();
   public$ MtcsCompiler    *getCompiler();
   public$ int              getNumber();

   public$ abstract$ aint64 getMemFree();
   public$ abstract$ aint64 getMemTotal();

   public$ abstract$ void   setProviderNumber(int num);//设供应商序号
   public$ abstract$ void   addListener(MtcsEventListener *listener);
   public$ abstract$ void   removeListener(MtcsEventListener *listener);
   //把设备设为当前线程的运行设备 返回0成功，否则失败。
   public$ abstract$ int    setDevice();
   public$ abstract$ MtcsStream *createStream();
   public$ abstract$ MtcsStream *createStream(int flag);

};

#endif /* __N_MEM_H__ */

