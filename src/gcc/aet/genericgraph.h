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

#ifndef __GCC_GENERIC_GRAPH_H__
#define __GCC_GENERIC_GRAPH_H__

#include "nlib.h"
#include "c-aet.h"
#include "classinfo.h"
#include "classfunc.h"
#include "aetparser.h"
#include "genericimpl.h"

#define GENOBJ_START "genobj start:"
#define GENOBJ_END   "genobj end:"

typedef struct _GenericGraph GenericGraph;
/* --- structures --- */
struct _GenericGraph
{
	AetParser *aetParser;
   NPtrArray *collectGenArray;
   char *saveContent;//保存到全局变量 LIB_GLOBAL_GENERIC_VAR_NAME_PREFIX 的内容
   NPtrArray *outputArray;//本项目输出的所有定义泛型对象。
   char *collectFileName;//保存new信息的文件，每个编译文件一个

};

typedef enum
{
   NEW_OBJECT,
   GEN_FUNC,
   PARENT_FROM_OBJECT,//来自对象的父类
}GenericObjType;

typedef struct _GenericObj
{
   GenericObjType type;//0 新建对象 1 调用泛型  2 新建对象为父类设置的泛型
   RunGenericInfo **infos;
   int infoLen;
   ClassFunc *callee;   //被调用的泛型函数;
   ClassInfo *newObject;//新建的class;
   ClassFunc *atFunc;   //调用泛型函数或新建对象时所在的类函数 可以是空的，但如果不为空atClass也应该不为空。
   ClassInfo *atClass;  //调用泛型函数或新建对象时所在的类 可以是空的
   GenericModel *origModel;//泛型类或泛型函数的泛型模型。
   char *declClassFile;//类声明所在的文件.h或.c
   nboolean ref;//生成可达图是标注已经加入过。

}GenericObj;


GenericGraph  *generic_graph_get();
//加入调用泛型函数
void           generic_graph_add_func_call(GenericGraph *self,RunGenericInfo **infos,
                           ClassFunc *callee,ClassFunc* atFunc,ClassInfo *atClass);
//加入创建泛型类
void           generic_graph_add_new_class(GenericGraph *self,RunGenericInfo **infos,
                           ClassInfo *info,ClassFunc* atFunc,ClassInfo *atClass,nboolean isParent);
void           generic_graph_print(GenericGraph *self);
void           generic_graph_save(GenericGraph *self);
void           generic_graph_ready(GenericGraph *self);
char          *generic_graph_get_output_string(GenericGraph *self);
NPtrArray     *generic_graph_get_output_generic_obj(GenericGraph *self);
NPtrArray     *generic_graph_read(char *content);

void           generic_obj_free(GenericObj *self);
void           generic_obj_print(GenericObj *self);


#endif


