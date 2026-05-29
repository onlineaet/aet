/*
 * Copyright (C) 2026  zclei
 * This file is part of AET.

 * AET is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3, or (at your option) any later
 * version.

 * AET is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.

 * You should have received a copy of the GNU General Public License
 * along with GCC Exception along with this program; see the file COPYING3.
 * If not see <http://www.gnu.org/licenses/>.
 * AET was originally developed  by the onlineaet@163.com
 */

#ifndef __GCC_MTCS_TOOL_H__
#define __GCC_MTCS_TOOL_H__

#include "../nlib.h"

//写入汇编代码的变量名前缀 mtcs/ElfFile.c 也定义该宏 两者应一样。
#define MTCS_ASM_VARNAME_PREFIX "mtcs_asm_code"

const char *mtcs_tool_get_fnname_from_decl(tree decl);
//如果有__global__ 或 _global_ 或
nboolean   mtcs_tool_is_mtcs_var(tree decl);//是不是矩阵芯片的变量
tree       mtcs_tool_copy_var(tree var);
void       mtcs_tool_print_cfun_loop();
//创建写入汇编代码的变量名，每个文件有1到n个，具体数量由需要编译的平台数决定
char      *mtcs_tool_create_asm_varname(char *platform,int isa,int version,char *fname);
int        mtcs_tool_get_isa_and_version(int **isaAndVersion,int platform);





#endif /* ! __GCC_MTCS_TOOL_H__ */
