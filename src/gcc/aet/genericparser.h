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


#ifndef __GCC_GENERIC_PARSER_H__
#define __GCC_GENERIC_PARSER_H__

#include "nlib.h"
#include "c-aet.h"
#include "aetparser.h"
#include "classfunc.h"

typedef struct _GenericParser GenericParser;
/* --- structures --- */
struct _GenericParser
{
	AetParser *parser;
	char *currentDefineStr;
	//新版 E_int_0_5{,F_float_1_8}
	void *directives[10];
	int directiveCount;
	//当编译完整个单元的ast，收集有块函数的classFunc并存入funcWithBlockArray;
   NPtrArray *localFwgbArray;
   //本项目+库的fwgb数组
	NPtrArray *funcWithBlockArray;
	char *funcWithGBFileName;

	nboolean isAddBlock;//是否加入泛型块
	nboolean isRegisterPlugin;//是否注册了判断函数是否可迁移的插件

	NPtrArray *constDeclArray;
};

//描述带泛型块的函数
typedef struct _FuncWithGbData
{
   char *file; //所在的源文件
   char *className;//所在的类
   int blockCount; //有多少个泛型块
   int firstNumber;//带泛型块的函数的第一个泛型块序号
   char *code; //带泛型块的函数的源代码
   char *mangleFunName;//带泛型块的函数的混淆名
   ClassFunc *func;//带泛型块的函数，只在编译单元时存在，在编译 temp_func_track_45.c是空的
}FuncWithGbData;

GenericParser  *generic_parser_get();
void           generic_parser_cast_by_token(GenericParser *self,c_token *token);
void           generic_parser_replace(GenericParser *self,char *genStr);
void           generic_parser_parser_typeof(GenericParser *self);

//新版2025-11-10
void           generic_parser_enter(GenericParser *self);

//编译第单元时调用
void           generic_parser_register_fwg(GenericParser *self);
void           generic_parser_save_fwgb(GenericParser *self,ClassName *className);
//这两个方法处于编译temp_func_track_45.c时调用
void           generic_parser_ready(GenericParser *self);
char          *generic_parser_get_fwg_source(GenericParser *self);
/**
 * 由aetlib库调用,从库中生成FuncWithGbData
 */
NPtrArray     *generic_parser_create_fwg(char *content);
int            generic_parser_get_func(GenericParser *self,char *sysName,FuncWithGbData **data);
/**
 * 根据泛型声明返回真实的类型,调用该方法应处于编译泛型块函数期。
 */
char *generic_parser_get_true_type(GenericParser *self,char *genStr,int *pointerCount);
//获取index泛型的定义大小
int   generic_parser_get_true_type_size(GenericParser *self,int index);
void generic_parser_modify(GenericParser *self,tree *mlhs,tree *mrhs);
void generic_parser_parm(GenericParser *self,vec<tree, va_gc> *params, vec<tree, va_gc> *origtypes);
tree generic_parser_initializer(GenericParser *self,tree decl,tree init);
void generic_parser_binary_op(GenericParser *self,enum tree_code code,tree *lhs,tree *rhs);
void generic_parser_return (GenericParser *self,tree *expr);

void generic_parser_record_const_decl(GenericParser *self,location_t loc,tree id,tree ref);

#endif
