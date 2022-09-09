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

#ifndef __GCC_CLASS_INIT_H__
#define __GCC_CLASS_INIT_H__

#include "nlib.h"
#include "classinfo.h"


/**
 * 重要的改变：2022-02-26之前
 * 给父类的域方法或父类实现的接口方法赋值都是在newstrategy.c的
 * addMiddleCodes方法调用class_init_override_parent_ref.
 * 但这会丢失在子类中定义的fromImplDefine函数(见ClassFunc.c)
 * 所以给父类赋值改成在子类的实现后的初始化方法中。就不会出现函数“丢失”
 * 的问题。
 */

typedef struct _ClassInit ClassInit;
/* --- structures --- */
struct _ClassInit
{
	c_parser *parser;
};



ClassInit *class_init_new();
void       class_init_create_init_decl(ClassInit *self,ClassName *className);
void       class_init_create_init_define(ClassInit *self,ClassName *className,NPtrArray *ifaceCodes,char *superCodes,NString *buf);
char      *class_init_override_parent_ref(ClassInit *self,ClassName *childName,char *varName);
char      *class_init_modify_root_object_free_child(ClassInit *self,ClassName *className,char *varName);
void       class_init_create_init_define_for_interface(ClassInit *self,char *sysName,NString *buf);


#endif


