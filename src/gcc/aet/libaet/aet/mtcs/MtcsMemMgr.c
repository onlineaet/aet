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

#include "MtcsMemMgr.h"

typedef struct KeyValue{
   int provider;
   int devNum;
   unsigned long address;
   auint64 size;
}KeyValue;

static void freeKeyValue_cb(KeyValue *value)
{
   a_slice_free(KeyValue,value);
}

impl$  MtcsMemMgr {

   static MtcsMemMgr *getInstance(){
      static MtcsMemMgr *singleton = NULL;
      if (!singleton){
         singleton =new$ MtcsMemMgr();
      }
      return singleton;
   }

   private$ MtcsMemMgr(){
      sourcesHash = new$ AHashTable(AHashTable.directHash,AHashTable.directEqual, NULL,freeKeyValue_cb);
   }

   public$ void add(int provider,int devNum,unsigned long address,auint64 size){
      KeyValue *value =a_slice_new(KeyValue);
      value->provider=provider;
      value->devNum=devNum;
      value->address = address;
      value->size = size;
      if(!sourcesHash->put(AUINT_TO_POINTER(address),value)){
         a_error("地址重复:provider:%d devNum:%d address:%u size:%llu",provider,devNum,address,size);
      }
   }

   /**
   * 实现MtcsEventListener 接口
   */
   void mallocHappend(MtcsEvent *event){
      //printf("mtcsmemmgr.c mallocHappend ----xxx %p\n",event->address);
      int provider=event->providerNum;
      int devNum=event->devNum;
      asize address=event->address;
      add(provider,devNum,address,event->size);
   }


   public$ void get(unsigned long address,int *provider,int *devNum){
      KeyValue *value=sourcesHash->get(AUINT_TO_POINTER(address));
      if(value!=NULL){
         *provider = value->provider;
         *devNum = value->devNum;
         return;
      }
      *provider = -1;
      *devNum = -1;
   }

   public$ void remove(unsigned long address){
      sourcesHash->remove(AUINT_TO_POINTER(address));
   }

};



