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

impl$ AClass{

   char *getName(){
      return name;
   }

   char *getPackage(){
      return packageName;
   }

   AClass *getParent(){
      return parent;
   }

   public$ asize    getSize(){
      return size;
   }

   AClass **getInterfaces(int *count){
      if(interfaceCount==0)
         return NULL;
      *count=interfaceCount;
      return  interfaces;
   }

   public$ unsigned long getInterfaceOffset(int index){
      if(interfaceCount==0){
         printf("获取接口在类中的偏移地址出错,类%s没有接口 index:%d\n",name,index);
         exit(0);
         return 0;
      }
      if(index<0 || index>=interfaceCount){
         printf("获取接口在类中的偏移地址出错,类%s实现了%d个接口 index:%d\n",name,interfaceCount,index);
         exit(0);
         return 0;
      }
      return interfacesOffset[index];
   }

   public$ aboolean isMtcsClass(){
      if(isMtcs)
         return TRUE;
      AClass *parent= getParent();
      while(parent!=NULL){
         if(parent->isMtcsClass())
            return TRUE;
         parent=parent->getParent();
      }
      return FALSE;
   }

   AClass(){
      name=NULL;
      packageName=NULL;
      parent=NULL;
      interfaceCount=0;
      isMtcs = 0;
   }

};
