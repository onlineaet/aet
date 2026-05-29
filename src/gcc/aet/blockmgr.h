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
#include "aetparser.h"
#include "genericparser.h"


typedef struct _BlockMgr BlockMgr;
/* --- structures --- */
struct _BlockMgr
{
	AetParser *parser;
	GenericInfo *genericInfos[20];
	int infoCount;
	tree lhs;
	char *currentBlockName;
	//新版
	char *saveString;
	NPtrArray *outputArray;//所有的genericinfo

	char *blockFileName;

};


BlockMgr      *block_mgr_get();
struct c_expr  block_mgr_parser(BlockMgr *self);
void           block_mgr_save(BlockMgr *self);
void           block_mgr_ready(BlockMgr *self);
char          *block_mgr_get_save(BlockMgr *self);
NPtrArray     *block_mgr_get_output_generic_info(BlockMgr *self);
nboolean       block_mgr_parser_goto(BlockMgr *self,nboolean start_attr_ok,AetGotoTag re);
void           block_mgr_set_lhs(BlockMgr *self,tree lhs);
int            block_mgr_get_block_count(BlockMgr *self,ClassName *className);
int            block_mgr_have_block(BlockMgr *self,ClassName *className);



#endif

