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

/**
 * 关键字 varof$的用法
 * if(var varof$ type)
 * var转成 void*类型 作为 varof_object_or_interface的第一个参数 object
 * type 取类型名作为第二个参数 name
 * name= type 的名字
 * object = var
 * AET类或接口的第一个域是魔数
 */
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
 * 接口的第一个域是 _iface_common_var IFACE_COMMON_STRUCT_NAME = "IfaceCommonData123"
 * _iface_common_var的类型是 IfaceCommonData123，该结体的第一个参数是 _atClass123
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
void  mtcs_copy_device_address_to_super(unsigned long *hostParentDeviceAddress,int count,char *destVarName,int platformType);


static inline void printgen(void *data)
{
    //printf("target_expr object is :%p\n",data);
}


#endif /* __N_MEM_H__ */

