/*
 * Copyright (C) 2022 , guiyang,wangyong co.,ltd.

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

#ifndef __GCC_ACCESS_CONTROLS_H__
#define __GCC_ACCESS_CONTROLS_H__

#include "nlib.h"
#include "classinfo.h"
#include "classfunc.h"
#include "enumparser.h"


/**
 * 两部分功能：
 * 1.保存类中声明的变量 方法 枚举到类实现所在的文件中。
 * 2.对类方法 变量 枚举进行访问控制。
 */

typedef struct _AccessControls AccessControls;
/* --- structures --- */
struct _AccessControls
{
	c_parser *parser;
    NPtrArray *accessArray;//存储访问信息的数组
    NPtrArray *implClassArray;//当前编译文件中实现的类
};

AccessControls *access_controls_get();
void            access_controls_save_access_code(AccessControls *self,ClassName *className);
nboolean        access_controls_access_method(AccessControls *self,location_t loc,ClassFunc *func);
nboolean        access_controls_access_var(AccessControls *self,location_t loc,char *varName,char *calleeSysName);
nboolean        access_controls_access_enum(AccessControls *self,location_t loc,EnumData *enumData,char *enumVar);

void            access_controls_check(AccessControls *self);
void            access_controls_set_parser(AccessControls *self,c_parser *parser);
/**
 * 加入在当前编译文件中实现的类的名称到AccessControls中.
 * 第二次编译接口时，也有接口的初始化方法和AClass的实现，也需要加入到AccessControls中
 */
void            access_controls_add_impl(AccessControls *self,char *sysName);

#endif


