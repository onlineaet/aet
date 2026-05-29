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

#include "MtcsDevice.h"

impl$  MtcsDevice {

   public$ MtcsDevice(){
      name=NULL;
      devNum=0;
   }

   public$ MtcsDevice(const char *name,int devNum){
      self->name=strdup(name);
      self->devNum = devNum;
   }

   public$ void setAlloctor(MtcsMemAlloctor *alloctor){
      if(self->alloctor!=NULL){
         a_error("alloctor 已存在。:%s",__FUNCTION__);
         abort();
      }
      self->alloctor = alloctor->ref();
   }

   public$ void setLanucher(MtcsLanucher *lanucher){
      if(self->lanucher!=NULL){
         a_error("lanucher 已存在。:%s",__FUNCTION__);
      }
      self->lanucher = lanucher->ref();
   }

   public$ void setCompiler(MtcsCompiler *compiler){
      if(self->compiler!=NULL){
         a_error("compiler 已存在。:%s",__FUNCTION__);
      }
      self->compiler = compiler->ref();
   }

   public$ MtcsMemAlloctor *getMemAlloctor(){
      return alloctor;
   }

   public$ MtcsLanucher *getLanucher(){
      return lanucher;
   }

   public$ MtcsCompiler *getCompiler(){
      return compiler;
   }


   /**
    * 获取设备号
    */
   public$ int  getNumber(){
      return devNum;
   }
};



