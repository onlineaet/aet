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

#include "cfganal.h"
#include "cfgcleanup.h"
#include "alias.h"
#include "rtlhooks-def.h"
#include "tree-pass.h"
#include "dbgcnt.h"
#include "rtlanal.h"

#include "aet/aetprinttree.h"
#include "mtcstarget.h"

void mtcs_unspec_init(MtcsUnspec *self)
{
   self->numUnspec=0;
   self->unspecStrings=NULL;
   self->numUnspecv=0;
   self->unspecvStrings=NULL;

}

void mtcs_unspec_set_unspec_string(MtcsUnspec *self,char **values,int nums)
{
   self->unspecStrings=values;
   self->numUnspec=nums;
}

void mtcs_unspec_set_unspecv_string(MtcsUnspec *self,char **values,int nums)
{
   self->unspecvStrings=values;
   self->numUnspecv=nums;
}

char *mtcs_unspec_get_unspec_string(MtcsUnspec *self,int index)
{
   if(index<0 || index>=self->numUnspec){
       n_error("索引不正确:%d 范围在 0-%d\n",index,self->numUnspec);
    }
    return self->unspecStrings[index];
}

char *mtcs_unspec_get_unspecv_string(MtcsUnspec *self,int index)
{
   if(index<0 || index>=self->numUnspecv){
      n_error("索引不正确:%d 范围在 0-%d\n",index,self->numUnspecv);
   }
   return self->unspecvStrings[index];
}

int mtcs_unspec_get_unspec_num(MtcsUnspec *self)
{
   return self->numUnspec;
}

int mtcs_unspec_get_unspecv_num(MtcsUnspec *self)
{
   return self->numUnspecv;
}





