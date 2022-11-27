
#include <string.h>
#include <stdlib.h>
#include "AArray.h"
#include "AQSort.h"
#include "AAssert.h"

#define MIN_ARRAY_SIZE  16
#define element_len(vector,i) ((vector)->elementSize * (i))
#define element_pos(vector,i) ((vector)->data + element_len((vector),(i)))
#define element_zero(vector, pos, len)  (memset (element_pos ((vector), pos), 0, element_len ((vector), len)))

impl$ AArray{

   static auint nearestPow (auint num){
	  auint n = num - 1;
	  a_assert (num > 0);
	  n |= n >> 1;
	  n |= n >> 2;
	  n |= n >> 4;
	  n |= n >> 8;
	  n |= n >> 16;
	  return n + 1;
   }

   static inline auint getElementLen(int count){
	   return self->elementSize*count;
   }

   static inline void setZeroTerminate(){
	   if(zero_terminated){
		  int elementLen=getElementLen(self->len);
		  char *newData=(char*)(self->data+elementLen);
		  memset(newData,0,self->elementSize);
	   }
   }


   static void maybeExpand(auint eleCount){
	  auint want_alloc;
	  /* Detect potential overflow */
	  if A_UNLIKELY ((A_MAXUINT - self->elementCount) < eleCount)
	    a_error ("加 %u 到数组将溢出。", eleCount);
	  want_alloc = self->elementSize*(self->elementCount + eleCount);
	  if (want_alloc > self->alloc){
	     want_alloc = nearestPow (want_alloc);
	     want_alloc = MAX (want_alloc, MIN_ARRAY_SIZE);
         self->data = a_realloc (self->data, want_alloc);//realloc 保留原来的数据返加扩大的内存
       //  printf("maybeExpand %d %p\n",want_alloc,self->data);
	     memset (self->data + self->alloc, 0, want_alloc - self->alloc);
	     self->alloc = want_alloc;
	  }
   }

   static void clear(int index){
	  if (self->clearFunc != NULL){
		void *data=element_pos (self, index);
		if(isPointer){
			unsigned long temp=((unsigned long*)data)[0];
			data=(void*)temp;
		}
		self->clearFunc (data);
	  }
   }

   static void  removeRange (auint index_,auint removeCount){
	  a_return_if_fail (self);
	  a_return_if_fail (index_ <= self->elementCount);
	  a_return_if_fail (index_ + removeCount <= self->elementCount);
      if (self->clearFunc != NULL){
         auint i;
         for (i = 0; i < removeCount; i++){
        	clear (index_ + i);
         }
      }
      if (index_ + removeCount != self->elementCount)
         memmove (element_pos (self, index_),element_pos (self, index_ + removeCount),(self->elementCount - (index_ + removeCount)) * self->elementSize);
      self->elementCount -= removeCount;
   	  element_zero (self,self->elementCount, removeCount);
   }

   static void setSize(auint newEleCount){
      a_return_if_fail (self);
      if (newEleCount > self->elementCount){
         maybeExpand (newEleCount - self->elementCount);
       	 element_zero (self, self->elementCount, newEleCount - self->elementCount);
      }else if (newEleCount < self->elementCount)
    	  removeRange (newEleCount, self->elementCount - newEleCount);
      self->elementCount = newEleCount;
   }

   static void init(auint capacity,ADestroyNotify clearFunc){
	  self->data            = NULL;
	  self->elementCount    = 0;
	  self->alloc           = 0;
	  self->elementSize     = sizeof(E);
	  self->isPointer       =generic_is_pointer(E);
	  self->clearFunc       = clearFunc;
	  if (capacity != 0){
		 maybeExpand (capacity);
	  }
   }

   AArray(){
	  init(5,NULL);
   }


   AArray(auint capacity){
	   init(capacity,NULL);
   }

   AArray(auint capacity,ADestroyNotify clearFunc){
	   init(capacity,clearFunc);
   }

   static void addFirst(E value,auint addCount){
      if(addCount == 0 || data==NULL)
         return ;
      maybeExpand (addCount);
      void *data=value;
	  if(isPointer){
		 aulong add=(aulong)value;
		 data=&add;
	  }
      memmove (element_pos (self, addCount), element_pos (self, 0),element_len (self, self->elementCount));
      memcpy (element_pos (self, 0), data, element_len (self, addCount));
      self->elementCount += addCount;
   }

   void addFirst(E data){
	    addFirst(data,1);
   }

   auint getElementSize(){
      a_return_val_if_fail (self, 0);
      return elementSize;
   }

   void removeAll(){
	   removeRange(0,elementCount);
   }

   void remove(auint   index){
      a_return_if_fail (self);
	  a_return_if_fail (index< self->elementCount);
      clear(index);
      if (index != self->elementCount - 1)
         memmove (element_pos (self, index),element_pos (self, index + 1),element_len (self, self->elementCount - index - 1));
      self->elementCount -= 1;
   	  element_zero (self, self->elementCount, 1);
   }

   void remove(auint index,auint removeCount){
	   removeRange(index,removeCount);
   }

   aboolean   remove(E data){
     auint i;
     for (i = 0; i < self->elementCount; i += 1){
	    void *item=self->data+elementSize*i;
	    aboolean canRemove=FALSE;
	    if(isPointer){
	   	     aulong add=(aulong)item;
	   	     item=&add;
	   	     canRemove=(item == data);
	  	}else{
	  		 canRemove=(memcmp(item,data,elementSize)==0);
	  	}
	    if (canRemove){
		   self->remove (i);//与stdio.h中的remove冲突。所以加self->
		   return TRUE;
	    }
     }
     return FALSE;
   }

   auint size(){
	   return elementCount;
   }


   void add(E value){
	  maybeExpand (1);
	  int pos=self->elementSize*self->elementCount;
	  self->elementCount += 1;
	  void *addData=value;
	  if(isPointer){
	     unsigned long add=(unsigned long)addData;
	      memcpy (self->data+pos, &add, self->elementSize);
	  }else{
         memcpy (self->data+pos, addData, self->elementSize);
	  }
   }


   E get(int index) {
	  if(index<0 || index>self->elementCount-1)
		  return NULL;
	  char *posData=(char *)(self->data+index*self->elementSize);
	  if(isPointer){
	       unsigned long temp=0;
	       memcpy(&temp,posData,self->elementSize);
	       posData=(void*)temp;
	  }
	  return posData;
   }

   void insert(E data, int index) {
      if (index <-1 || index> (int)elementCount) {
           a_error("插入位置越界 index:%d 大于 %d",index,elementCount);
           return;
      }
      if (index < 0)
        index =self->elementCount;
	  if (index == self->elementCount){
		maybeExpand(1);
		setSize (index);
		add(data);
		return;
	  }
	  maybeExpand (1);
	  memmove (element_pos (self, 1 + index),element_pos (self, index),element_len (self, self->elementCount - index));
	  void *addData=data;
	  if(isPointer){
		 aulong add=(aulong)data;
		 addData=&add;
	  }
	  memcpy (element_pos (self, index), addData, self->elementSize);
	  self->elementCount += 1;
   }

   aboolean isEmpty(){
       return elementCount == 0;
   }

   void foreach (AFunc func,apointer userData){
     auint i;
     for (i = 0; i < elementCount; i++){
    	 void *data=element_pos (self, i);
		 if(isPointer){
			unsigned long temp=((unsigned long*)data)[0];
			data=(void*)temp;
		 }
         (*func) (data, userData);
     }
   }

   void sort(ACompareFunc compareFunc){
		 sort(compareFunc,NULL);
   }

   void sort(ACompareFunc compareFunc,apointer userData){
	     apointer src=self->data;
		 if(isPointer){
			apointer *srcp=(apointer)src;
	        AQSort.sort(srcp,self->elementCount,self->elementSize,(ACompareDataFunc)compareFunc,userData);
		 }else{
            AQSort.sort(src,self->elementCount,self->elementSize,(ACompareDataFunc)compareFunc,userData);
		 }
   }


   ~AArray(){
	   if(data!=NULL){
		  a_free(data);
		  data=NULL;
	   }
   }


};
