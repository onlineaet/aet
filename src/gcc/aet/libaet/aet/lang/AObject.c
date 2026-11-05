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

#include "AObject.h"

/**
 * objectSize=0;由编译器来设大小
 */
impl$ AObject{

   AObject(){
      refCount=1;
   }

   ~AObject(){
      a_debug("析构函数执行。");
   }
   /**
    * 为新对象分配内存，isMtcs=true 调用 mtcs_alloc_object
    * 在cuda平台分配的是托管内存
    */
   public$ static void  *newObject(int size,int isMtcs,unsigned int platAndDev,char *name){
      if(!isMtcs){
         void *obj=a_slice_alloc0(size);//分配的对象置0，必须这样才安全。
         return obj;
      }
      AObject *obj=(AObject*)mtcs_alloc_object(size,platAndDev);
      obj->allocMethod = 1;//标记这是MTCS分配的内存。
      return obj;
   }

   void freeObject(){
      if(objectSize>0){
         if(!allocMethod){
            a_debug("释放主机内存，objSize:%d %p\n",objectSize,self);
            a_slice_free1(objectSize,(apointer)self);
         }else{
            //printf("释放MTCS内存，objSize:%d %p\n",objectSize,self);
            mtcs_free_object((void*)self);
         }
      }
   }

   auint getRefCount(){
      aint old_ref;
      old_ref = a_atomic_int_get ((int *)&refCount);
      return old_ref;
   }

   void unref(){
      aint old_ref;
      /* 这里我们想要自动做: if (refCount>1) { refCount--; return; } */
retry_atomic_decrement1:
      old_ref = a_atomic_int_get (&refCount);
      if (old_ref > 1){
         /* 如果此unref调用和toggle refs拥有最后2个ref，则有效 */
         if (!a_atomic_int_compare_and_exchange ((int *)&refCount, old_ref, old_ref - 1))
            goto retry_atomic_decrement1;
         a_debug("引用计数器不是1 ------ :%d %d %p",old_ref,refCount,self);
         return;
      }else{
         /*可能同时被重新引用 */
retry_atomic_decrement2:
         old_ref = a_atomic_int_get ((int *)&refCount);
         if (old_ref > 1){
            /* 如果此unref调用和toggle_ref拥有最后2个ref，则有效*/
            aboolean has_toggle_ref = FALSE;//OBJECT_HAS_TOGGLE_REF (object);
            a_debug("引用计数器不是1 -- :%d %d %p",old_ref,refCount,self);
            if (!a_atomic_int_compare_and_exchange ((int *)&refCount, old_ref, old_ref - 1))
               goto retry_atomic_decrement2;
            return;
         }
         /* 递减最后一个引用 */
         old_ref = a_atomic_int_add (&refCount, -1);
         if(old_ref<=0){
            printf("unref fail ref:%d\n",old_ref);
            return;
         }
         /* 可能同时被重新引用 */
         if (A_LIKELY (old_ref == 1)){
          //  a_debug("在这里真正的释放了。%p %s",self,myClass->getName());
            free_child();//是在new对象时由编译器赋值的 。AObject不能具体实现free_child方法。由编译器生成。指向的是~XXX析构函数
            freeObject();
         }
      }
   }

   AObject *ref(){
      AObject *object = self;
      aint old_val;
      aboolean object_already_finalized;
      old_val = a_atomic_int_add (&object->refCount, 1);
      object_already_finalized = (old_val <= 0);
      if(object_already_finalized)
         return NULL;
      return object;
   }
};

