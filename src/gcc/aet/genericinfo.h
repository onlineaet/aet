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

#ifndef __GCC_GENERIC_INFO_H__
#define __GCC_GENERIC_INFO_H__

#include "nlib.h"
#include "c-aet.h"
#include "classinfo.h"
#include "genericblock.h"


#define MAX_GEN_BLOCKS 30


typedef struct _GenericInfo GenericInfo;
/* --- structures --- */
struct _GenericInfo
{
	c_parser *parser;
	ClassName *className;
	tree genStructDecl;
	GenericBlock *blocks[MAX_GEN_BLOCKS];
	int blocksCount;


	//className对应的类声明所在的文件。用在编译middlefile时
	char *classDeclBelongFile;
	char *oFile;
	int isGenericClass;//是不是泛型类
   char *includeStr;//.c文件中的include文件
};


GenericInfo     *generic_info_new(ClassName *className);
//加入泛型块
GenericBlock    *generic_info_add_block(GenericInfo *self,tree lhs,vec<tree, va_gc> *exprlist,
                           char *body,char *belongFunc,nboolean isFuncGeneric);
int              generic_info_get_block_count(GenericInfo *self);

GenericBlock    *generic_info_get_block(GenericInfo *self,char *name);
GenericBlock    *generic_info_get_block_by_index(GenericInfo *self,int index);
tree             generic_info_get_field(GenericInfo *self,char *name);

nboolean         generic_info_same(GenericInfo *self,ClassName *className);
char            *generic_info_save(GenericInfo *self);

//新版 11-05
NPtrArray       *generic_info_create_info(char *content);
//根据函数名取所属的块数量
int              generic_info_get_block_count_by_belong(GenericInfo *self,char *managleFuncName);
GenericBlock    *generic_info_get_first_block_by_belong(GenericInfo *self,char *managleFuncName);

#endif

