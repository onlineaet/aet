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
tree            class_interface_transport_ref_and_unref(ClassInterface *self,tree component,tree expr);
void            class_interface_add_var_ref_unref_method(ClassInterface *self,location_t loc);
//是一个接口中声明的host device函数吗
nboolean class_interface_is_host_device_func(ClassInterface *self,ClassInfo *info,tree decls);
tree class_interface_divide_host_device_func(ClassInterface *self,location_t loc,ClassInfo *info,tree decls);
//接口方法加入到类中
int class_interface_add_to_class(ClassInterface *self,location_t loc,ClassInfo *info,tree *classMethods);

#endif


