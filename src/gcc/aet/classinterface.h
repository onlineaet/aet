/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

This file is part of AET.

AET is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

AET is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC Exception along with this program; see the file COPYING3.
If not see <http://www.gnu.org/licenses/>.
AET was originally developed  by the zclei@sina.com at guiyang china .
*/

#ifndef __GCC_CLASS_INTERFACE_H__
#define __GCC_CLASS_INTERFACE_H__

#include "nlib.h"
#include "aetmangle.h"
#include "aetparser.h"


typedef struct _IFaceSourceCode
{
    char *define;
    char *modify;
}IFaceSourceCode;

typedef struct _ClassInterface ClassInterface;
/* --- structures --- */
struct _ClassInterface
{
	AetParser *parser;

};


ClassInterface *class_interface_new();
int             class_interface_parser(ClassInterface *self,ClassInfo *info,char **interface);
char           *class_interface_create_codes(ClassInterface *self,ClassName *className);
//是一个接口中声明的host device函数吗
nboolean        class_interface_is_host_device_func(ClassInterface *self,ClassInfo *info,tree decls);
tree            class_interface_divide_host_device_func(ClassInterface *self,location_t loc,ClassInfo *info,tree decls);
//接口方法加入到类中
int             class_interface_add_to_class(ClassInterface *self,location_t loc,ClassInfo *info,tree *classMethods);
void            class_interface_add_sys_field(ClassInterface *self,ClassName *iface,NString *codes);

inline nboolean class_interface_is_reserve_var(ClassInterface *self,char *var)
{
   return !strcmp(var,AET_MAGIC_NAME) || !strcmp(var,IFACE_AT_CLASS);
}

//每个接口都动态的创建一个ref函数定义
tree class_interface_create_ref_function_define(ClassInterface *self,location_t loc,
      ClassName *className,char *funcName);


#endif


