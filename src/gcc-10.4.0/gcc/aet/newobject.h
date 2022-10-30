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

#ifndef __GCC_NEW_OBJECT_H__
#define __GCC_NEW_OBJECT_H__

#include "classinfo.h"
#include "classinit.h"
#include "nlib.h"
#include "newstack.h"
#include "newheap.h"
#include "newfield.h"


typedef struct _NewObject NewObject;
struct _NewObject
{
	c_parser *parser;
    NewStack *newStack;
    NewHeap *newHeap;
    NewField *newField;
    nboolean returnNewObject;//标记是不是正在解析return new$ Object() 这样的语句。如果是，new$ Object()返回的是堆内存对象而不是栈内存对象。
    nboolean isParserParmsState;//是不是解析参数据状态。只能用在c-parser.c中。
};

NewObject  *new_object_get();
void        new_object_parser(NewObject *self,tree decl,GenericModel *genericsModel);
nboolean    new_object_ctor(NewObject *self,tree decl);
void        new_object_finish(NewObject *self,CreateClassMethod method,tree func);
nboolean    new_object_modify(NewObject *self,tree decl);
nboolean    new_object_parser_new$(NewObject *self);
void        new_object_set_return_newobject(NewObject *self,nboolean returnNewObject);//语句 return new$ Abc 可以判断是返回指针还是对象变量
char *      new_object_parser_for_static(NewObject *self,tree decl,GenericModel *genericsModel);
void        new_object_set_parser_parms_state(NewObject *self,nboolean isParserParmsState);//当前是不是存在解析参数状态。

void        new_object_set_parser(NewObject *self,c_parser *parser);



#endif

