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

#include "config.h"
#define INCLUDE_ALGORITHM /* reverse */
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "tree-pass.h"
#include "cfghooks.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "tree-pretty-print.h" /* for dump_function_header */
#include "varasm.h"
#include "insn-attr.h"
#include "conditions.h"
#include "flags.h"
#include "output.h"
#include "except.h"
#include "rtl-error.h"
#include "toplev.h" /* exact_log2, floor_log2 */
#include "reload.h"
#include "intl.h"
#include "cfgrtl.h"
#include "debug.h"
#include "tree-pass.h"
#include "tree-ssa.h"
#include "cfgloop.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "rtl-iter.h"
#include "print-rtl.h"
#include "function-abi.h"
#include "common/common-target.h"
#include "diagnostic.h"
#include "context.h"
#include "options.h"
#include "expmed.h"
#include "expr.h"
#include "fold-const.h"
#include "dwarf2out.h"
#include "common/common-targhooks.h"

#include "aet/aetprinttree.h"
#include "mtcspass.h"
#include "mtcstarget.h"

void      mtcs_pass_init(MtcsPass *self,enum opt_pass_type type,char *name)
{
    self->type=type;
    self->childs=n_ptr_array_new();
    self->name=n_strdup(name);
    self->properties_required=0;
    self->properties_provided=0;
    self->properties_destroyed=0;
    self->wrapper=NULL;
}

//原型 struct pass_data 成员变量 todo_flags_start todo_flags_finish tree-pass.h
void mtcs_pass_set_todo_flags(MtcsPass *self,nuint start ,nuint finish)
{
    self->todo_flags_start=start;
    self->todo_flags_finish=finish;
}

void mtcs_pass_set_properties(MtcsPass *self,nuint required,nuint provided,nuint destroyed)
{
    self->properties_required=required;
    self->properties_provided=provided;
    self->properties_destroyed=destroyed;
}

nuint  mtcs_pass_excute(MtcsPass *self,function *func)
{
    if(self->execute)
        return self->execute(self,func);
    else if(self->wrapper)
      return self->wrapper->execute(func);
    return 0;
}

nboolean  mtcs_pass_gate(MtcsPass *self,function *func)
{
    if(self->gate)
          return self->gate(self,func);
      return TRUE;
}

void mtcs_pass_add_pass(MtcsPass *self,MtcsPass *pass)
{
   MtcsMode   *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsPassMgr *mtcsPassMgr=mtcs_target_get_pass_mgr(mtcsTarget);
   mtcs_pass_mgr_set_todo_flags_start(mtcsPassMgr,pass);
   n_ptr_array_add(self->childs,pass);
}



