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

#ifndef __AET_LANG_A_OBJECT_H__
#define __AET_LANG_A_OBJECT_H__

#include "../../alib.h"
package$ aet.lang;


#define container_of(ptr, type, member) ({          \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type,member) );})


public$ class$ AObject{
   private$ int allocMethod;
   public$ static void     *newObject(int size,int isMtcs,unsigned int platAndDev,char *name);
   public$   asize          objectSize;
   private$  volatile auint refCount;
   public$ void             unref();
   public$ AObject*         ref();
   private$ void            free_child();//在new对象时由编译器赋值的 。AObject不能具体实现free_child方法。由编译器生成。
   public$ void             freeObject();//与newObjec对应。
   public$ auint            getRefCount();
   public$ final$ AClass   *getClass();
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
    private$ aboolean isMtcs;
    private$ asize size;//对象大小
    private$ AClass();
    public$ char    *getName();
    public$ char    *getPackage();
    public$ AClass  *getParent();
    public$ AClass **getInterfaces(int *count);
    public$ aboolean isMtcsClass();
    public$ asize    getSize();
    public$ unsigned long getInterfaceOffset(int index);
};

///////////////---------------以下都是编译器调用的，用户程序不能调用---------------

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
      char *package = interfaceClasses[i]->getPackage();
      char *clName  = interfaceClasses[i]->getName();
      int same = 0;
      if (package){
         size_t len = strlen(package);
         same = strncmp(package, ifaceSysName, strlen(package)) == 0
                  && ifaceSysName[len] == '_'  && strcmp(ifaceSysName + strlen(package) + 1,clName) == 0;
      }else{
         same =  strcmp(clName, ifaceSysName) == 0;
      }
      if(same){
         *offset=class->getInterfaceOffset(i);
         return 1;
      }
   }
   AClass *parentClass=class->getParent();
   return class_to_iface(parentClass,ifaceSysName,offset);
}

/**
 * 由AET编译器生成调用
 * 类转接口
 */
static inline void* dynamic_class_to_iface(void *data,char *ifaceSysName,char *compileFile,int line,int column)
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
 * 动态的类转接口
 */
static inline int _find_class_by_AClass(AClass *class,char *findSysName)
{
   if(class==NULL)
      return 0;
   AClass *parent=class->getParent();
   if(!parent)
      return 0;
   if(parent->getPackage()){
      char sysName[512];
      sprintf(sysName,"%s_%s",parent->getPackage(),parent->getName());
      if(!strcmp(sysName,findSysName))
           return 1;
   }else{
      if(!strcmp(parent->getName(),findSysName))
         return 1;
   }
   return _find_class_by_AClass(parent,findSysName);
}

/**
 * 由AET编译器生成调用
 * 接口转类
 * 接口的第一个域是 _aet_magic$_123 第二个是 _atClass123
 * 是接口指向实现的类"
 * bug 078
 */
static inline void* dynamic_iface_to_class(void *atClass,char *ifaceSysName,char *destSysName,char *compileFile,int line,int column)
{
   AClass *class=((AObject*)atClass)->getClass();
   int find= _find_class_by_AClass(class,destSysName);
   if(!find){
      //动态转化是错的，给用户报位置
      char msg[1024];
      sprintf(msg,"接口 %s不能被转化为类 %s 。\n在文件 %s 第%d行 第%d列。\n",ifaceSysName,destSysName,compileFile,line,column);
      printf(msg);
      exit(1);
   }else{
      return (void *)atClass;
   }
}

void *mtcs_alloc_object(int size,int platformType);
/**
 * 实现AObject.h声明的函数 释放 mtcs_alloc_object 分配的内存
 */
void  mtcs_free_object(void *obj);
void  mtcs_copy_device_func_address(void *dest,char *devicePointerVarName,int element,int platformType);
void  mtcs_copy_device_address_to_super(unsigned long *hostParentDeviceAddress,
      int count,char *destVarName,int platformType);

//E getData(){return NULL},return NULL被转化为 return generic_is_pointer(E)?NULL:_aet_generic_zero_storage
//全局零数组变量名，在so文件中有属性weak,在可执行文件中是强类型
extern const char _aet_generic_zero_storage[];


static inline void printgen(void *data)
{
    //printf("target_expr object is :%p\n",data);
}


#endif /* __N_MEM_H__ */

