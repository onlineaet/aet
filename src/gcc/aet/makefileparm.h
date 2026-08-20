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

#ifndef __GCC_MAKEFILE_PARM_H__
#define __GCC_MAKEFILE_PARM_H__

#include "nlib.h"

typedef struct _MakefileParm MakefileParm;
/* --- structures --- */
struct _MakefileParm
{
	NPtrArray *bufferFiles;
	nboolean  isSecondCompile;
	char     *objectFile;//.o文件
	//2025-11-08 在本单元编译将结束时，编译加入的文件 compileFileName 取 compileFileName的内容
	//加入到cpp_buffer中，追加编译，现在用在泛型块的编译上 compileFileName = _block_func__0.o
	char *compileFileName;
	nboolean insertBlockFunc;
   char *gccRootPath;

};


MakefileParm  *makefile_parm_get();
nboolean       makefile_parm_is_second_compile(MakefileParm *self);
char          *makefile_parm_get_object_file(MakefileParm *self);
void           makefile_parm_append_d_file(MakefileParm *self);
//在文件尾插入块函数代码。
void           makefile_parm_insert_block_func_codes(MakefileParm *self);
//获取aet include路径
const char    *makefile_parm_get_aet_include_path(MakefileParm *self);
char          *makefile_parm_get_aetprog(MakefileParm *self);

#endif


