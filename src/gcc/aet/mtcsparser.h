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

#ifndef __GCC_MTCS_PARSER_H__
#define __GCC_MTCS_PARSER_H__

#include "nlib.h"
#include "classinfo.h"
#include "aetmediator.h"
#include "mtcslanuch.h"
#include "mtcsbuiltintree.h"
#include "aetparser.h"
#include "classfunc.h"
#include "mtcslink.h"

/**
 * 两个主要功能
 * 1.执行kernel代码
 * 2.解析kernel函数声明代码
 */
typedef struct _MtcsParser MtcsParser;
/* --- structures --- */
struct _MtcsParser
{
    AetMediatorUser mediatorUser; //实现中介者接口
    AetParser *aetParser;
    NPtrArray *mtcsPromoteLocalVarArray;//函数体中的局部mtcs变量
    int enterFuncBody;//进入mtcs函数体
    nboolean haveMtcsFunc;
    nboolean haveMtcsVar; //有没有mtcs变量
    MtcsLanuch *mtcsLanuch;//解析启动核函数的类
    MtcsBuiltinTree *mtcsBuiltinTree;
    int localVarAssembleNumber;//本地变量的汇编序号
    MtcsLink *mtcsLink;
};

MtcsParser *mtcs_parser_get();
//供funccall回调
nboolean   mtcs_parser_is_attribute(MtcsParser *self,c_token *token);
nboolean   mtcs_parser_add_attribute(MtcsParser *self);
void       mtcs_parser_check(MtcsParser *self,tree decl,nboolean finishDecl);

void       mtcs_parser_enter_function_body(MtcsParser *self);
void       mtcs_parser_exit_function_body(MtcsParser *self);
nboolean   mtcs_parser_is_compiling(MtcsParser *self);
void       mtcs_parser_ast_end(MtcsParser *self);
char      *mtcs_parser_add_buitlins_tree(MtcsParser *self);
//如果是mtcs内部变量，比如 matDim,unitDim,unitIdx,threadIdx(nvptx中的gridDim blockIdx,blockDim,threadIdx)
tree       mtcs_parser_vars_parser(MtcsParser *self,location_t loc,tree id);
void       mtcs_parser_modify_check(MtcsParser *self,location_t loc,tree lhs,tree rhs);
void       mtcs_parser_postfix_expression (MtcsParser *self,location_t loc,tree value);

void       mtcs_parser_parser_launch_param(MtcsParser *self,vec<tree, va_gc> **lanuchList);
/**
 * 把>>>替换为)
 */
void       mtcs_parser_replace_rshift_and_greater(MtcsParser *self);
/**
 * 为AObject创建平台类型变量 AET_MTCS_PLATFORM_TYPE_VAR_NAME mtcsPlatformType
 */
tree mtcs_parser_create_platform_type_var(MtcsParser *self,location_t loc);
/**
 * 创建全局设备函数地址变量
 *static __device__ void *_TFirst_deviceFuncPointers[]={_Z6TFirst10testkernelEPN6TFirstE};
 */
char      *mtcs_parser_create_device_func_pointers_var(MtcsParser *self,ClassName *className);
/**
 * 创建的代码嵌入到类初始化方法中。
 */
char      *mtcs_parser_modify_host_device_func_var_array(MtcsParser *self,ClassName *className);
void       mtcs_parser_link_func(MtcsParser *self);
nboolean   mtcs_parser_have_mtcs(MtcsParser *self);

#endif


