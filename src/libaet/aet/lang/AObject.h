

#ifndef __AET_LANG_A_OBJECT_H__
#define __AET_LANG_A_OBJECT_H__

#include <alib.h>

package$ aet.lang;

//#define offsetof(type, field)   ((long) &((type *)0)->field)

#define container_of(ptr, type, member) ({          \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type,member) );})

typedef struct _SuperCallData{
	 char *sysName;
     char *rawMangleName;
     unsigned long  address;
}SuperCallData;

public$ class$ AObject{
   public$ static void * newObject(asize size);
   private$  asize    objectSize;
   private$  volatile auint refCount;
   public$ void       unref();
   public$ AObject*   ref();
   public$ void       free_child();
   public$ void       freeObject();//与newObjec对应。
   public$ auint      getRefCount();
   public$ final$ AClass *getClass();
   private$ SuperCallData **superCallData;
   private$ unsigned long *_superFuncAddr123[10];//存放每个继承类的原始函数地址。这些地址来自superCallData
};

class$ AClass{
    private$ char *name;
    private$ char *packageName;
    private$ AClass();
    public$ char *getName();
};


typedef struct _AArrayCleanup
{
	void *arrays;
	int count;
	int size;
}AArrayCleanup;

static inline void a_object_cleanup_local_object_from_static_or_stack(AObject *obj)
{
	//printf("a_object_cleanup_local_object_from_static_or_stack %p\n",obj);
	obj->unref();
	obj=NULL;
}

typedef struct _IfaceCommonData123 IfaceCommonData123;
struct _IfaceCommonData123{
	void *_atClass123;
};

/**
 * 因为用户调用方式如下:
 * Iface *var=objectIface->ref();
 * 原来的AObject返回的是对象。这里需要返回IfaceCommonData123
 * 因为IfaceCommonData123是所有接口的第一个结构体变量。
 */
static inline  IfaceCommonData123 * _iface_reserve_ref_func_define_123(IfaceCommonData123 *iface)
{
	//printf("_iface_reserve_ref_func_define_123 %p\n",iface);
	((AObject*)iface->_atClass123)->ref();
	return iface;
}

static inline  void _iface_reserve_unref_func_define_123(IfaceCommonData123 *iface)
{
	((AObject*)iface->_atClass123)->unref();
}

/**
 * 初始化superData，在AObject的init_1234ergR5678方法中调用。
 * 详见编译器的classinit.c
 */
static inline void _init_super_data_123(AObject *self)
{
    self->superCallData=NULL;
    memset(self->_superFuncAddr123,0,sizeof(long)*10);
}

static inline  void _free_super_data(AObject *self)
{
	if(self){
		int i=0;
		if(self->superCallData){
		    while(self->superCallData[i]!=NULL){
		    	SuperCallData *item=self->superCallData[i];
		    	a_free(item->sysName);
		    	item->sysName=NULL;
		       	a_free(item->rawMangleName);
		        item->rawMangleName=NULL;
		        a_free(item);
		        self->superCallData[i]=NULL;
		        i++;
		    }
			a_free(self->superCallData);
			self->superCallData=NULL;
		}
		i=0;
		while(self->_superFuncAddr123[i]!=NULL){
			void *data=self->_superFuncAddr123[i-1];
			a_free(data);
			self->_superFuncAddr123[i]=NULL;
			i++;
		}
	}
}

static inline void add_super_method(AObject *self,char *sysName,char *rawMangleName,unsigned long address)
{
	 if(self->superCallData==NULL){
		  //printf("add_super_method 第一次 self:%p sysName:%s rawMangleName:%s add:%lu\n",self,sysName,rawMangleName,address);
          self->superCallData=a_malloc0(sizeof(SuperCallData*)*2);
	      self->superCallData[0]=a_malloc0(sizeof(SuperCallData));
          self->superCallData[0]->sysName=strdup(sysName);
          self->superCallData[0]->rawMangleName=strdup(rawMangleName);
          self->superCallData[0]->address=address;
          self->superCallData[1]=NULL;
	 }else{
		  int count=0;
		  while(self->superCallData[count++]!=NULL);
		  //printf("add_super_method 第二次 count:%d self:%p sysName:%s rawMangleName:%s add:%lu\n",count,self,sysName,rawMangleName,address);
	      self->superCallData=realloc(self->superCallData,sizeof(SuperCallData*)*(count+1));
	      self->superCallData[count-1]=a_malloc0(sizeof(SuperCallData));
          self->superCallData[count-1]->sysName=strdup(sysName);
          self->superCallData[count-1]->rawMangleName=strdup(rawMangleName);
          self->superCallData[count-1]->address=address;
	      self->superCallData[count]=NULL;
	 }
}

static inline unsigned long _get_super_address123(AObject *self,char *sysName,char *rawMangleName)
{
	  if(self->superCallData==NULL)
		  return 0;
	  int count=0;
	  while(self->superCallData[count]!=NULL){
		  SuperCallData *item=self->superCallData[count];
		  if(!strcmp(item->sysName,sysName) && !strcmp(item->rawMangleName,rawMangleName)){
			  //printf("_get_super_address123 count:%d %s %s\n",count,sysName,rawMangleName);
			  return item->address;
		  }
		  count++;
	  }
	  return 0;
}

static  inline int _fill_super_method_address123(AObject *self,char *fromSysName,int distance,int superCallCount,int index,char *sysName,char *rawMangleName)
{
	unsigned long *addresses=NULL;
    unsigned long **data=self->_superFuncAddr123;
    if(data[distance]==NULL){
//  	    printf("_fill_super_method_address123 第一次初始化 fromSysName:%s long * self:%p distance:%d superCallCount:%d index:%d sysName:%s rawMangleName:%s \n",
//  	    		fromSysName,self,distance, superCallCount,index,sysName,rawMangleName);
    	data[distance]=malloc(sizeof(long)*superCallCount);
    	memset(data[distance],0,sizeof(long)*superCallCount);
    }
    addresses=data[distance];
    unsigned long addr=_get_super_address123(self,sysName,rawMangleName);
//	  printf("_fill_super_method_address123 fromSysName:%s self:%p distance:%d superCallCount:%d index:%d sysName:%s rawMangleName:%s add:%lu\n",
//			  fromSysName,self,distance, superCallCount,index,sysName,rawMangleName,addr);
    if(addr>0){
    	addresses[index]=addr;
    	return 1;
    }
    return 0;
}


static inline void printgen(void *data)
{
    //printf("target_expr object is :%p\n",data);
}


#endif /* __N_MEM_H__ */

