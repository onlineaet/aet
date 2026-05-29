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

#ifndef __GCC_MTCS_PASS__
#define __GCC_MTCS_PASS__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "tree-pass.h"


typedef struct _MtcsPass MtcsPass;
struct _MtcsPass
{
    MtcsComponent parent;
    enum opt_pass_type type;
    char *name;
    /* Sets of properties input and output from this pass.  */
    unsigned int properties_required;
    unsigned int properties_provided;
    unsigned int properties_destroyed;
    /* Flags indicating common sets things to do before and after.  */
    //原型 struct pass_data 成员变量 todo_flags_start todo_flags_finish tree-pass.h
    unsigned int todo_flags_start;
    unsigned int todo_flags_finish;
    //MtcsPass *sub;
    NPtrArray *childs;
    nuint (*execute)(MtcsPass *self,function *func);
    nboolean (*gate)(MtcsPass *self,function *func);
    void (*generate_summary) (MtcsPass *self);
    void (*write_summary) (MtcsPass *self);
    void (*read_summary) (MtcsPass *self);
    unsigned int function_transform_todo_flags_start;
    nuint (*function_transform)(MtcsPass *self,struct cgraph_node *node);
    /* Static pass number, used as a fragment of the dump file name.  */
    int static_pass_number;

    opt_pass *wrapper;

};

void      mtcs_pass_init(MtcsPass *self,enum opt_pass_type type,char *name);
nuint     mtcs_pass_excute(MtcsPass *self,function *func);
nboolean  mtcs_pass_gate(MtcsPass *self,function *func);
//原型 struct pass_data 成员变量 todo_flags_start todo_flags_finish tree-pass.h
void      mtcs_pass_set_todo_flags(MtcsPass *self,nuint start ,nuint finish);
void      mtcs_pass_set_properties(MtcsPass *self,nuint required,nuint provided,nuint destroyed);
void      mtcs_pass_add_pass(MtcsPass *self,MtcsPass *pass);

#endif

