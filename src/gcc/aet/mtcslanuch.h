/*
 * Copyright (C) 2022 , guiyang,wangyong co.,ltd.

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

#ifndef __GCC_MTCS_LANUCH_H__
#define __GCC_MTCS_LANUCH_H__

#include "nlib.h"
#include "classinfo.h"
#include "aetmediator.h"
#include "classfunc.h"

/**
 * 两个主要功能
 * 1.执行kernel代码
 * 2.解析kernel函数声明代码
 */
typedef struct _MtcsLanuch MtcsLanuch;
/* --- structures --- */
struct _MtcsLanuch
{
   NPtrArray *funcLanchParamsArray;//调用核函数和对应的启动参数
   tree lanuchFuncDecl_1;//启动函数 匹配13个参数
};

MtcsLanuch *mtcs_lanuch_new();
void        mtcs_lanuch_parser_launch_param(MtcsLanuch *self,vec<tree, va_gc> **lanuchList);
//缓存调用的函数和启动参数
void       mtcs_lanuch_add_func_and_lanch_params(MtcsLanuch *self,location_t loc,tree funcDeclOrRef, vec<tree, va_gc> *lanchParams);
//替换call为启动函数 MtcsSystem.lanuch
tree       mtcs_lanuch_replace_call(MtcsLanuch *self,tree earlyFuncDeclOrRef,location_t loc,tree call,ClassFunc *callFunc);
nboolean   mtcs_lanuch_exists_earlycall(MtcsLanuch *self,tree earlyFuncDecl);
//bug 074
tree       mtcs_lanuch_replace_call_implicitly(MtcsLanuch *self,tree earlyFuncDeclOrRef,
               location_t loc,vec<tree, va_gc> *exprlist,tree caller,ClassFunc *callee,tree impliciCallee);

#endif


