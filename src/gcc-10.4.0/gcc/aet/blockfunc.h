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
#ifndef __GCC_BLOCK_FUNC_H__
#define __GCC_BLOCK_FUNC_H__

#include "nlib.h"
#include "classinfo.h"
#include "c-aet.h"
#include "genericinfo.h"
#include "genericcommon.h"


typedef struct _BlockFunc BlockFunc;
/* --- structures --- */
struct _BlockFunc
{
	char *funcSource;
	char *sysClassName;
	char *orgiFuncName;//_test_AObject__inner_generic_func_1
	char *returnStr;
	char *parmStr;
	char *body;
	void *parmInfos[20];
	int   parmInfoCount;
	NPtrArray *condArray;
	NPtrArray *condIndexArray;
	char *belongFunc;//属于那个函数中的块
	nboolean isFuncGeneric; //所属函数是不是泛型函数

	NPtrArray *collectDefine;
	int index;//类中的第几个泛型块 从1开始

	char *funcDefineName; //_test_AObject__inner_generic_func_1_abc123
    char *funcDefineDecl;//static void  _test_AObject__inner_generic_func_1_abc123 (test_AObject *self,FuncGenParmInfo tempFgpi1234,int abc);

	char *typedefFuncName;// _test_AObject__inner_generic_func_1_typedecl
	char *typedefFuncDecl;// typedef void (*_test_AObject__inner_generic_func_1_typedecl)(test_AObject *self,FuncGenParmInfo tempFgpi1234,int abc);

};


BlockFunc     *block_func_new(int index,char *sysName,BlockDetail *detail);
void           block_func_ready_parm(BlockFunc *self,NPtrArray *collectArray,NPtrArray *collectFuncCallArray);
char          *block_func_create_codes(BlockFunc *self);
GenericUnit   *block_func_get_define(BlockFunc *self,char *parmName,char *genStr,int pos);
nboolean       block_func_is_same(BlockFunc *self,char *orgiFuncName);
nboolean       block_func_have_func_decl_tree(BlockFunc *self);
nboolean       block_func_have_typedef_func_decl(BlockFunc *self);

#endif















