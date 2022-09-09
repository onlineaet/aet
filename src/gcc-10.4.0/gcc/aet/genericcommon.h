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

#ifndef GCC_AET_GENERIC_COMMON_H
#define GCC_AET_GENERIC_COMMON_H

#include "nlib.h"
#include "genericmodel.h"
#include "classinfo.h"

#define GENERIC_UNKNOWN_STR "unknown"

typedef struct _SetBlockHelp
{
	NString *codes; //这是设置泛型块的函数定义代码:_createAllGenericFunc567
	NPtrArray *setBlockFuncsArray;
	NPtrArray *blockFuncsArray;
	NPtrArray *implSetBlockFuncArray;
	NPtrArray *collectDefines;
	NPtrArray *collectFuncCallDefines;

}SetBlockHelp;

typedef struct _SetGenericBlockFuncInfo
{
	char *sysName;
	char *funcName;
}SetGenericBlockFuncInfo;

typedef struct _CollectDefines
{
	char *sysName;
	char *funcName;
	NPtrArray *definesArray;
}CollectDefines;

#define MAX_GEN_BLOCKS 30

typedef struct _BlockDetail
{
	char *codes;
	char *belongFunc;
	nboolean isFuncGeneric;
}BlockDetail;


typedef struct _BlockContent BlockContent;
struct _BlockContent
{
	char *sysName;
	int blocksCount;
	BlockDetail *content[50];
};


typedef struct _GenericNewClass
{
	char *sysName;
	char *atClass;
	char *atFunc;
	char *id;
	GenericModel *model;
}GenericNewClass;


typedef struct _GenericCallFunc
{
	char *sysName; //被调用的泛型函数所属类
	char *funcName;//被调用的泛型函数名
	char *atClass;//被调用的泛型函数所在的类
	char *atFunc;//被调用的泛型函数所在的函数
	char *id;
	GenericModel *model;
}GenericCallFunc;


#define PARENT_TYPE_CLASS 0
#define PARENT_TYPE_METHOD 1
typedef struct _GenericCM
{
	void *parent;
    int type;
}GenericCM;




NPtrArray        *generic_storage_relove_func_call(char *varName,char *value);
NPtrArray        *generic_storage_relove_new_object(char *varName,char *value);

GenericCallFunc *generic_call_func_new(ClassName *className,char *funcName,GenericModel *defines);
void             generic_call_func_free(GenericCallFunc *self);

GenericNewClass *generic_new_class_new(char *sysName,GenericModel *defines);
void             generic_new_class_free(GenericNewClass *self);
nboolean         generic_new_class_equal(GenericNewClass *self,GenericNewClass *dest);
char            *generic_new_class_get_id(GenericNewClass *self);
nboolean         generic_new_class_exists(NPtrArray *array, GenericNewClass *obj);

void class_and_method_set_parent(GenericCM *self,GenericCM *parent);
GenericCM *class_and_method_get_root(GenericCM *self);


#endif /* ! GCC_AET_TYPECK_H */
