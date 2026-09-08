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
#include <string.h>
#include <stdlib.h>
#include "../lang/AQSort.h"
#include "AArray.h"

//element_pos中必须转化成char * 因为array是void类型
#define MIN_ARRAY_SIZE  16

#define element_count()  (( (char*)finish - (char*)start ) / elementSize)

static auint nearestPow (auint num)
{
   auint n = num - 1;
   //a_assert (num > 0);
   n |= n >> 1;
   n |= n >> 2;
   n |= n >> 4;
   n |= n >> 8;
   n |= n >> 16;
   return n + 1;
}


impl$ AArray{
   AArray(){
      init(5,NULL);
   }

   AArray(auint capacity){
      init(capacity,NULL);
   }

   AArray(auint capacity,ADestroyNotify destroyFunc){
      init(capacity,destroyFunc);
   }

   void init(auint capacity,ADestroyNotify destroyFunc){
      self->elementSize     = sizeof(E);
      self->isPointer       =generic_is_pointer(E);
      self->destroyFunc       = destroyFunc;
      self->start = finish = end_of_storage= NULL;
      if (capacity != 0)
         maybeExpand (capacity);
   }

   /**
    * 扩大array到新的大小
    */
   void maybeExpand(auint eleCount) {
       // 当前已有元素个数
       auint currentCount = element_count();   // 即 (finish - start) / elementSize
       // 需要的总元素个数
       auint needCount = currentCount + eleCount;
       // 当前容量（元素个数）
       auint currentCapacity = 0;
       if (start != NULL)
           currentCapacity = ((char*)end_of_storage - (char*)start) / elementSize;
       // 如果容量足够，直接返回
       if (needCount <= currentCapacity)
           return;

       // 检查溢出
       if (A_UNLIKELY((A_MAXUINT - currentCount) < eleCount)) {
           a_error("加 %u 到数组溢出。\n", eleCount);
           return;
       }
       // 计算新的容量（字节）
       auint want_alloc = elementSize * needCount;
       want_alloc = nearestPow(want_alloc);
       want_alloc = MAX(want_alloc, MIN_ARRAY_SIZE);
       // 重新分配
       void *new_array = a_realloc(start, want_alloc);
       // 更新三个指针
       auint oldSizeBytes = (char*)finish - (char*)start;
       start          = new_array;
       finish         =(E *) ((char*)start + oldSizeBytes);
       end_of_storage = (E *)((char*)start + want_alloc);
   }

   /**
    * 释放元素index的内存
    */
   void clear(int index){
      if (destroyFunc != NULL){
         destroyFunc(isPointer?start[index]:(char*)start+index*elementSize);
      }
   }

   /**
    * 从 index_ 开始移除 removeCount 个元素
    */
   void removeRange(auint index_, auint removeCount){
       auint sz = element_count();
       a_return_if_fail(index_ <= sz);
       a_return_if_fail(index_ + removeCount <= sz);
       // 调用销毁回调
       if (destroyFunc != NULL) {
           for (auint i = 0; i < removeCount; ++i) {
               clear(index_ + i);
           }
       }

       // 把后面的元素前移
       if (index_ + removeCount < sz) {
           memmove((char*)start + index_ * elementSize,
                   (char*)start + (index_ + removeCount) * elementSize,
                   (sz - index_ - removeCount) * elementSize);
       }
       // 回退 finish 指针
       finish = (E*)((char*)finish - removeCount * elementSize);
   }

   /**
    * 基于当前size()的大小 设置元素个数据
    */
   void resize(auint newSize){
      auint currentCount = element_count();   // 即 (finish - start) / elementSize
      if (newSize > currentCount){
         int expandSize  = newSize - currentCount;
         maybeExpand (expandSize);
         memset((char*)finish, 0, expandSize*elementSize);
         finish = (E*)((char*)finish + expandSize * elementSize);
      }else if (newSize < currentCount)
         removeRange (newSize, currentCount - newSize);
   }

   /**
    * 加元素到数组的0号位置
    */
   void addFirst(E value){
       // 容量不足时扩容（与 add 完全一致）
       if (finish == end_of_storage)
           maybeExpand(1);
       // 把现有元素整体向后挪一个位置
       // （空数组时 finish == start，memmove 长度为 0，安全）
       memmove((char*)start + elementSize,start,(char*)finish - (char*)start);
       // 在头部写入新值
       genericblock$(value) {
           start[0] = value;
           finish = (void **)((char*)finish + sizeof(E));
       };
   }

   /**
    * 获取元素的大小 sizeof(E);
    */
   auint getESize(){
      return elementSize;
   }

   void removeAll(){
      removeRange(0,element_count());
   }

   void remove(auint index,auint removeCount){
      removeRange(index,removeCount);
   }

   /**
    * 清除 index 位置的元素，后面的元素前移
    */
   void remove(auint index){
       auint sz =element_count();
       //调用销毁回调（如果有）
       clear(index);
       // 把 index 后面的元素整体前移一位
       if (index + 1 < sz) {
           memmove((char*)start + index * elementSize,
                   (char*)start + (index + 1) * elementSize,
                   (sz - index - 1) * elementSize);
       }
       genericblock$(){
          finish = (void**)((char*)finish-sizeof(E));
       };
   }

   /**
    * 比较是否相同，相同则移走（只删除第一个匹配的）
    */
   aboolean removeData(E data){
      auint sz =element_count();
      for (auint i = 0; i < sz; ++i) {
         aboolean find = genericblock$(data, i) {
            return start[i] == data;
         };
         if (find) {
            remove(i);
            return TRUE;          // 找到并删除后立即返回
         }
      }
      return FALSE;
   }

   /**
    * 放入泛型块中，size()可以进入内联
    */
   auint size(){
      return  genericblock$() {
         return (( (char*)finish - (char*)start ) / sizeof(E));
      };
   }

   void add(E value){
      // 判断是否需要扩容（finish 已经到达容量末尾）
      if (finish == end_of_storage)
         maybeExpand(1);
      genericblock$(value){
         finish = value;
         finish = (void**)((char*)finish+sizeof(E));
      };
   }

   E get(int index){
      return genericblock$(index) {
         return start[index];  // 直接通过 start 指针访问
      };
   }

   E getAt(int index){
      return genericblock$(index) {
         auint sz =(( (char*)finish - (char*)start ) / sizeof(E));
         if (index < 0 || index >= sz){
            a_error("下标越界:%d 大小:%d\n",index,sz);
         }
         return start[index];  // 直接通过 start 指针访问
      };
   }

   void insert(E data, int index) {
       auint sz = element_count();
       if (index < -1 || index > (int)sz) {
           a_error("插入位置越界 index:%d 大于 %d\n", index, sz);
       }
       // -1表示尾插
       if (index < 0)
           index = sz;
       // 尾部直接add
       if (index == sz) {
           add(data);
           return;
       }
       if (finish == end_of_storage)
          maybeExpand(1);
       /*
        * 后移 index 后面的元素
        *
        * 目标:
        * [0 ... index-1][index ... finish]
        *
        * 变成:
        * [0 ... index-1][空][index ... finish]
        */
       memmove( (char*)start + (index + 1) * elementSize,
           (char*)start + index * elementSize,
           (sz - index) * elementSize);

       genericblock$(data,index){
           start[index] = data;
           finish = (void **)((char*)finish + sizeof(E));
       };
   }

   aboolean isEmpty(){
      return finish==start;
   }

   void foreach (AFunc func,apointer userData){
       char *p=(char*)start;
       while(p < (char*)finish){
           (*func)( isPointer ? *(void**)p:p, userData);
           p += elementSize;
       }
   }

   void sort(ACompareFunc compareFunc){
      sort(compareFunc,NULL);
   }

   void sort(ACompareFunc compareFunc,apointer userData){
      apointer src=(void*)start;
      AQSort.sort(src,element_count(),self->elementSize,(ACompareDataFunc)compareFunc,userData);
   }

   /**
    * 弹出最后一个数据，并且清除
    */
   E popBack(){
      if ((char*)finish <= (char*)start)
         return NULL;
      return genericblock$(){
         finish = (void **)((char*)finish - sizeof(E));
         return finish;
      };
   }

   /**
    * 取最后一个数据，如果无数据返回空，不执行清除工作，因为有返回值 E 由调用者决定
    */
   E back(){
       if (finish <= start){
           // 空数组处理
           return NULL;
       }
       return genericblock$(){
         return (char*)finish-sizeof(E);
       };
   }

   /**
    * 给数组在index位置赋值,不做边界检查
    */
   void set(E value,int index){
      genericblock$(value,index){
         start[index] = value;
      };
   }

   /**
     * 给数组在index位置赋值,做边界检查
     */
   void setAt(E value,int index){
      genericblock$(value,index){
         auint sz =(( (char*)finish - (char*)start ) / sizeof(E));
         if (index < 0 || index >= sz){
            a_error("下标越界:%d 大小:%d\n",index,sz);
         }
         start[index] = value;
      };
   }

   ~AArray(){
      if(start!=NULL){
         a_free(start);
         start=NULL;
         finish=NULL;
         end_of_storage = NULL;
      }
   }

};
