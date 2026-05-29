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

#ifndef __GCC_MTCS_PTX_ASM__
#define __GCC_MTCS_PTX_ASM__

#include "aet/nlib.h"
#include "../mtcsrtldata.h"
#include "../mtcsasm.h"

typedef struct _MtcsPtxAsm MtcsPtxAsm;
struct _MtcsPtxAsm
{
    MtcsAsm parent;
    NPtrArray *funcDeclRegion;//保存所有函数声明的字符串和名字。
};

MtcsPtxAsm *mtcs_ptx_asm_new(MtcsMode *mtcsMode);
char *mtcs_ptx_asm_get_func_decl_asm(MtcsPtxAsm *self,char *name);
void  mtcs_ptx_asm_add_func_decl_asm(MtcsPtxAsm *self,char *name,NString *str,nboolean isExtern);
char *mtcs_ptx_asm_get_all_fun_decl_asm(MtcsPtxAsm *self);

#endif
