

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
   public$  asize     objectSize;
   private$  volatile auint refCount;
   public$ void       unref();
   public$ AObject*   ref();
   public$ void       free_child();//在new对象时由编译器赋值的 。AObject不能具体实现free_child方法。由编译器生成。
   public$ void       freeObject();//与newObjec对应。
   public$ auint      getRefCount();
   public$ final$ AClass *getClass();
   private$ SuperCallData **superCallData;
   private$ unsigned long *_superFuncAddr123[10];//存放每个继承类的原始函数地址。这些地址来自superCallData
};

/**
 * 编译器完成AClass实例化和属性的赋值。
 * 具体在_createAClass_debug_RandomGenerator_123方法中。
 * 所以用户是不能创建AClass对象的。
 */
public$ class$ AClass{
    private$ char *name;
    private$ char *packageName;
    private$ AClass *parent;
    private$ AClass *interfaces[10];
    private$ unsigned long interfacesOffset[10];
    private$ int interfaceCount;
    private$ AClass();
    public$ char    *getName();
    public$ char    *getPackage();
    public$ AClass  *getParent();
    public$ AClass **getInterfaces(int *count);
    public$ unsigned long getInterfaceOffset(int index);
};

///////////////---------------以下都是编译器调用的，用户程序不能调用---------------
/**
 * 配合编译器，清除实参new$ Object()。
 * void **values会报警告
 * 警告：传递‘a_object_cleanup_nameless_object’的第 1 个参数时在不兼容的指针类型间转换 [-Wincompatible-pointer-types]
 */
static inline void a_object_cleanup_nameless_object(AObject **values)
{
    AObject *obj=*values;
    obj->unref();
    obj=NULL;
}

static inline void a_object_cleanup_local_object_from_static_or_stack(AObject *obj)
{
    //printf("a_object_cleanup_local_object_from_static_or_stack ---%p\n",obj);
    obj->unref();
    obj=NULL;
}

typedef struct _IfaceCommonData123 IfaceCommonData123;
struct _IfaceCommonData123{
    int   _aet_magic$_123;
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

static inline int is_aet_class(AClass *class,char *name)
{
        char sysName[512];
        char *package=class->getPackage();
        if(package!=NULL)
           sprintf(sysName,"%s_%s",package,class->getName());
        else
           sprintf(sysName,"%s",class->getName());
        return strcmp(sysName,name)==0;
}

static inline int object_varof(AClass *class,char *name)
{
    if(class==NULL)
        return 0;
    if(is_aet_class(class,name))
        return 1;
    AClass *parentClass=class->getParent();
    if(parentClass!=NULL){
       if(is_aet_class(parentClass,name))
          return 1;
    }
       int ifaceCount=0;
       AClass **interfaceClasses=class->getInterfaces(&ifaceCount);
       int i;
       for(i=0;i<ifaceCount;i++){
           if(is_aet_class(interfaceClasses[i],name))
                return 1;
       }
       return object_varof(parentClass,name);
}

static inline int  varof_object_or_interface (void *object,char *name)
{
    char *magic=(char*)object;
    int magicNum =0;
    memcpy(&magicNum,magic,sizeof(int));
    if(magicNum!=1725348960 && magicNum!=1725348961)
        return 0;
    AObject *dest=NULL;
    if(magicNum==1725348960){
        dest=(AObject*)object;
    }else if(magicNum==1725348961){
        IfaceCommonData123 *iface=(IfaceCommonData123*)magic;
        dest=(AObject*)iface->_atClass123;
    }
    if(dest==NULL)
        return 0;
    return object_varof(dest->getClass(),name);
}

/**
 * 动态的类转接口
 */
static inline int class_to_iface(AClass *class,char *ifaceSysName,unsigned long *offset)
{
    if(class==NULL)
          return 0;
    int ifaceCount=0;
    AClass **interfaceClasses=class->getInterfaces(&ifaceCount);
    int i;
    for(i=0;i<ifaceCount;i++){
      if(is_aet_class(interfaceClasses[i],ifaceSysName)){
           *offset=class->getInterfaceOffset(i);
           return 1;
      }
    }
    AClass *parentClass=class->getParent();
    return class_to_iface(parentClass,ifaceSysName,offset);
}

static inline void* dynamic_cast_iface(void *data,char *ifaceSysName,char *compileFile,int line,int column)
{
     AObject *obj=(AObject*)data;
     AClass *class=obj->getClass();
     unsigned long offset=0;
     int find= class_to_iface(class,ifaceSysName,&offset);
     if(!find){
         //动态转化是错的，给用户报位置
         char msg[1024];
         sprintf(msg,"类 %s不能被转化为接口 %s 。\n在文件 %s 第%d行 第%d列。\n",class->getName(),ifaceSysName,compileFile,line,column);
         printf(msg);
         exit(1);
     }else{
         return (void *)(((unsigned long)data)+offset);
     }
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
           // printf("_free_super_data ---- %p\n",self);
            while(self->superCallData[i]!=NULL){
                SuperCallData *item=self->superCallData[i];
                a_free(item->sysName);
                item->sysName=NULL;
                a_free(item->rawMangleName);
                item->rawMangleName=NULL;
                a_free(item);
                self->superCallData[i]=NULL;
                i++;
                //printf("_free_super_data--- %d %p\n",i,self);
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
         // printf("add_super_method 第一次 self:%p sysName:%s rawMangleName:%s add:%lu\n",self,sysName,rawMangleName,address);
          self->superCallData=a_malloc0(sizeof(SuperCallData*)*2);
          self->superCallData[0]=a_malloc0(sizeof(SuperCallData));
          self->superCallData[0]->sysName=a_strdup(sysName);
          self->superCallData[0]->rawMangleName=a_strdup(rawMangleName);
          self->superCallData[0]->address=address;
          self->superCallData[1]=NULL;
     }else{
          int count=0;
          while(self->superCallData[count++]!=NULL);
          //printf("add_super_method 第二次 count:%d self:%p sysName:%s rawMangleName:%s add:%lu\n",count,self,sysName,rawMangleName,address);
          self->superCallData=realloc(self->superCallData,sizeof(SuperCallData*)*(count+1));
          self->superCallData[count-1]=a_malloc0(sizeof(SuperCallData));
          self->superCallData[count-1]->sysName=a_strdup(sysName);
          self->superCallData[count-1]->rawMangleName=a_strdup(rawMangleName);
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
//          printf("_fill_super_method_address123 第一次初始化 fromSysName:%s long * self:%p distance:%d superCallCount:%d index:%d sysName:%s rawMangleName:%s \n",
//                  fromSysName,self,distance, superCallCount,index,sysName,rawMangleName);
        data[distance]=malloc(sizeof(long)*superCallCount);
        memset(data[distance],0,sizeof(long)*superCallCount);
    }
    addresses=data[distance];
    unsigned long addr=_get_super_address123(self,sysName,rawMangleName);
//    printf("_fill_super_method_address123 fromSysName:%s self:%p distance:%d superCallCount:%d index:%d sysName:%s rawMangleName:%s add:%lu\n",
//            fromSysName,self,distance, superCallCount,index,sysName,rawMangleName,addr);
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

