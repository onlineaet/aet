#include "AObject.h"
#include "../../aslice.h"
#include "../../aatomic.h"
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

  static void *newObject(asize size){
	   void * obj=a_slice_alloc0(size);
	   return obj;
  }

  void freeObject(){
      AClass *myClass = getClass();
	  a_debug("释放内存,如果objectSize>0是堆内存， 否则是栈或全局:objectSize:%d %s",objectSize,myClass->getName());
	  _free_super_data(self);
	  if(objectSize>0){
	    a_slice_free1 (objectSize,(apointer)self);
	  }
  }

  void toggle_refs_notify(aboolean isLastRef){

  }

  void free_child(){

  }

  auint getRefCount(){
      aint old_ref;
      old_ref = a_atomic_int_get ((int *)&refCount);
      return old_ref;
  }

  void unref(){
     AClass *myClass = getClass();
     aint old_ref;
     /* 这里我们想要自动做: if (refCount>1) { refCount--; return; } */
     retry_atomic_decrement1:
        old_ref = a_atomic_int_get (&refCount);
        if (old_ref > 1){
          /* 如果此unref调用和toggle refs拥有最后2个ref，则有效 */
        	aboolean has_toggle_ref =FALSE;// OBJECT_HAS_TOGGLE_REF (object);

           if (!a_atomic_int_compare_and_exchange ((int *)&refCount, old_ref, old_ref - 1))
   	          goto retry_atomic_decrement1;
      	 a_debug("引用计数器不是1 ------ :%d %d %p %s",old_ref,refCount,self,myClass->getName());

          /* 如果我们从2->1开始，我们需要通知toggle refs */
           if (old_ref == 2 && has_toggle_ref) /* 本例中持有的最后一个ref由toggle_ref所有 */
   	          toggle_refs_notify (TRUE);
        }else{
           /*可能同时被重新引用 */
       retry_atomic_decrement2:
         old_ref = a_atomic_int_get ((int *)&refCount);
         if (old_ref > 1){
             /* 如果此unref调用和toggle_ref拥有最后2个ref，则有效*/
             aboolean has_toggle_ref = FALSE;//OBJECT_HAS_TOGGLE_REF (object);
             a_debug("引用计数器不是1 -- :%d %d %p %s",old_ref,refCount,self,myClass->getName());
             if (!a_atomic_int_compare_and_exchange ((int *)&refCount, old_ref, old_ref - 1))
   	            goto retry_atomic_decrement2;
             /* 如果我们从2->1开始，我们需要通知toggle refs */
             if (old_ref == 2 && has_toggle_ref) /* 本例中持有的最后一个ref由toggle_ref所有 */
   	           toggle_refs_notify (TRUE);

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
        	 a_debug("在这里真正的释放了。%p %s",self,myClass->getName());
        	 free_child();//是在new对象时赋值的
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
