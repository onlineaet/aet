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

#ifndef __GCC_BLOCK_MGR_H__
#define __GCC_BLOCK_MGR_H__

#include "nlib.h"
#include "classinfo.h"
#include "c-aet.h"
#include "genericinfo.h"
#include "classfunc.h"


typedef struct _BlockMgr BlockMgr;
/* --- structures --- */
struct _BlockMgr
{
	c_parser *parser;
	GenericInfo *genericInfos[20];
	int infoCount;
	tree lhs;
	char *currentBlockName;
	NPtrArray *collectNewObject;
	NPtrArray *defineFuncCallArray;
};


BlockMgr      *block_mgr_get();
struct c_expr  block_mgr_parser(BlockMgr *self);

void           block_mgr_save_define_newobject_and_func_call(BlockMgr *self);

nboolean       block_mgr_parser_goto(BlockMgr *self,nboolean start_attr_ok,AetGotoTag re);


NPtrArray     *block_mgr_read_define_new_object(BlockMgr *self,char *sysName);
void           block_mgr_add_define_new_object(BlockMgr *self,ClassName *className,GenericModel *defines);
int            block_mgr_get_define_newobj_count(BlockMgr *self,ClassName *className);
char         **block_mgr_get_define_newobj_source_codes(BlockMgr *self,ClassName *className);

void           block_mgr_set_lhs(BlockMgr *self,tree lhs);
char         **block_mgr_get_block_source(BlockMgr *self,ClassName *className,int index);
int            block_mgr_get_block_count(BlockMgr *self,ClassName *className);
BlockContent  *block_mgr_read_block_content(BlockMgr *self,char *sysName);
int            block_mgr_have_block(BlockMgr *self,ClassName *className);

NPtrArray     *block_mgr_read_undefine_new_object(BlockMgr *self,char *belongSysName);
void           block_mgr_add_undefine_new_object(BlockMgr *self,ClassName *className,GenericModel *defines);
int            block_mgr_get_undefine_newobj_count(BlockMgr *self,ClassName *className);
char         **block_mgr_get_undefine_newobj_source_codes(BlockMgr *self,ClassName *className);

void           block_mgr_set_parser(BlockMgr *self,c_parser *parser);


char          *block_mgr_read_file_class_located(BlockMgr *self,char *sysName);//读取类所在的文件

//新泛型方法
void           block_mgr_add_undefine_func_call(BlockMgr *self,ClassName *className,ClassFunc *func,GenericModel *defines);
int            block_mgr_get_undefine_func_call_count(BlockMgr *self,ClassName *className);
char         **block_mgr_get_undefine_func_call_source_codes(BlockMgr *self,ClassName *className);
NPtrArray     *block_mgr_read_undefine_func_call(BlockMgr *self,char *sysName);


void           block_mgr_add_define_func_call(BlockMgr *self,ClassName *className,ClassFunc *func,GenericModel *defines);
int            block_mgr_get_define_func_call_count(BlockMgr *self,ClassName *className);
char         **block_mgr_get_define_func_call_source_codes(BlockMgr *self,ClassName *className);
NPtrArray     *block_mgr_read_define_func_call(BlockMgr *self,char *sysName);

vec<tree, va_gc> * block_mgr_get_expr_list(BlockMgr *self);

#endif

