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

#ifndef __GCC_MTCS_VAR__
#define __GCC_MTCS_VAR__

#include "../nlib.h"
#include "mtcsmicro.h"
#include "mtcscomponent.h"


typedef struct _MtcsVarNode MtcsVarNode;
struct _MtcsVarNode{
    MtcsNode parent;
    varpool_node *node;
    tree hostDecl;
    int promoteId;
    //由于优化或其它原因由内部创建的变量，一般在pass创建，比如在 tree-switch-conversion.cc 中的 pass "switchconv"
    //创建全局变量CSWTCH.4
    nboolean innerCreate;
};

//typedef enum{
//    MTCS_GLOBAL_MEM,     //卡上，显存       线程，线程块，GRID可见 动态 在c文件和主机函数内声明、创建（用主机函数mtcsmalloc） 静态(大小确定)在c文件声明，
//                         //用mtcsmemcpy在主机函数中赋值
//    MTCS_CONSTANT_MEM,   //卡上，显存(只读)  线程，线程块，GRID可见 静态(大小确定)在c文件声明 主机函数初始化
//    MTCS_TEXTURE_MEM ,   //同constant
//    MTCS_SHARED_MEM,     //片上            线程，线程块可见 核函数内声明 声明上确定大小，不能动态分配
//    MTCS_LOCAL_MEM       //卡上，显存       线程  核函数内声明 声明上确定大小，不能动态分配
//}MtcsMemType;


typedef struct _MtcsVar MtcsVar;


struct _MtcsVar
{
   MtcsComponent parent;
   NPtrArray *varArray;
   NPtrArray *hostArray;
   tree last_assemble_variable_decl;
};

MtcsVar      *mtcs_var_new(MtcsMode *mtcsMode);
//原型 varpool_node * varpool_node::get_create (tree decl) varpool.cc
varpool_node *mtcs_var_clone(MtcsVar *self,struct varpool_node *node);
void          mtcs_var_add_mtcs_node(MtcsVar *self,MtcsVarNode *node);
//原型 node->assemble_decl (); cgraph.h varpool.cc
bool          mtcs_var_assemble_decl (MtcsVar *self,varpool_node *node);
//原型 assemble_undefined_decl output.h varasm.cc
void          mtcs_var_assemble_undefined_decl (MtcsVar *self,tree decl);
//原型 symbol_table::output_variables cgraph.h varpool.cc
//重要方法，生成汇编
bool          mtcs_var_output_variables (MtcsVar *self);
char         *mtcs_var_assemble_local_shared(MtcsVar *self,int prmoteId);
char         *mtcs_var_replace_dot(MtcsVar *self,char *name);


#endif
