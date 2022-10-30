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

#ifndef __GCC_OBJECT_RETURN_H__
#define __GCC_OBJECT_RETURN_H__

#include "nlib.h"
#include "classcast.h"


/**
 * 当AObject obj=new$ AObject();时加入了cleanup，当退出作用域时
 * cleanup将被调用，如果obj作为返回值，这时cleanup不能调用，该类
 * 就是用于解决是否要移走cleanup然后把cleanup转移到被赋值的对象中。
 */


typedef struct _ObjectReturn ObjectReturn;
/* --- structures --- */
struct _ObjectReturn
{
	c_parser *parser;
	ClassCast *classCast;
	NPtrArray *dataArray;

};


ObjectReturn    *object_return_get();
void             object_return_finish_function(ObjectReturn *self,tree fndecl);
/**
 * 只有对象变量才能加入处理
 */
void             object_return_add_return(ObjectReturn *self,tree retExpr);
/**
 * 只有对象指针才能根据函数返回值声明转化
 */
tree             object_return_convert(ObjectReturn *self,tree retExpr);



#endif

