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
 * base on optabs-query.cc
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
#include "optabs-query.h"

#include "aet/aetprinttree.h"
#include "mtcsopinit.h"
#include "mtcstarget.h"


void     mtcs_opinit_init(MtcsOpinit *self)
{
    self->this_fn_optabs = &self->default_target_optabs;
    self->thisTargetOptabs=&self->default_target_optabs;
}

/**
 * 原型 optab_handler optabs-query.h
 * enum optab_tag 在insn-opinit.h中定义 host和nvptx相同
 */
enum insn_code mtcs_opinit_optab_handler(MtcsOpinit *self,optab op, mtcs_mode mode)
{
  unsigned scode = (op << 20) | mode;
  gcc_assert (op > LAST_CONV_OPTAB);
  return self->raw_optab_handler (self,scode);
}

/* Return the insn used to perform conversion OP from mode FROM_MODE
   to mode TO_MODE; return CODE_FOR_nothing if the target does not have
   such an insn.  */
//原型 convert_optab_handler optabs-query.h 重载函数
enum insn_code mtcs_opinit_convert_optab_handler (MtcsOpinit *self,convert_optab op, machine_mode to_mode,machine_mode from_mode)
{
  unsigned scode = (op << 20) | (from_mode << 10) | to_mode;
  gcc_assert (convert_optab_p (op));
  n_debug("mtcsopinit.c mtcs_opinit_convert_optab_handler op:%d scode:%d from_mode:%d to_mode:%d\n",op,scode,from_mode,to_mode);
  return self->raw_optab_handler (self,scode);
}

/* Return the insn used to perform conversion OP from mode FROM_MODE
   to mode TO_MODE; return CODE_FOR_nothing if the target does not have
   such an insn, or if it is unsuitable for optimization type OPT_TYPE.  */
//原型 convert_optab_handler optabs-query.h optabs-query.cc
enum insn_code mtcs_opinit_convert_optab_handler (MtcsOpinit *self,convert_optab optab, machine_mode to_mode,
             machine_mode from_mode, optimization_type opt_type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   enum insn_code icode = mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(self,optab, to_mode, from_mode);
   if (icode == CODE_FOR_nothing
            || !mtcsTarget/*!targetm.optab_supported_p*/->optab_supported_p(mtcsTarget,optab, to_mode, from_mode, opt_type))
      return CODE_FOR_nothing;
   return icode;
}

void   mtcs_opinit_set_number_optabs(MtcsOpinit *self,int number)
{
    self->num_optabs=number;
}

void   mtcs_opinit_set_number_convlib_optabs(MtcsOpinit *self,int number)
{
    self->num_convlib_optabs=number;

}
void  mtcs_opinit_set_number_normlib_optabs(MtcsOpinit *self,int number)
{
    self->num_normlib_optabs=number;

}
void  mtcs_opinit_set_number_optab_patterns(MtcsOpinit *self,int number)
{
    self->num_optab_patterns=number;
}

//原型 extern void init_all_optabs (struct target_optabs *); insn-opinit.h
void  mtcs_opinit_init_all_optabs(MtcsOpinit *self,struct target_optabs *optabs)
{
     self->init_all_optabs(self,optabs);
}

//原型 extern bool partial_vectors_supported_p (); insn-opinit.h insn-opinit.cc
bool  mtcs_opinit_partial_vectors_supported_p(MtcsOpinit *self)
{
   return self->partial_vectors_supported_p(self);
}

//原型 extern bool swap_optab_enable (optab, machine_mode, bool); insn-opinit.h insn-opinit.cc
bool mtcs_opinit_swap_optab_enable (MtcsOpinit *self,optab op, machine_mode m, bool set)
{
   return self->swap_optab_enable(self,op,m,set);
}

/* Return the insn used to implement mode MODE of OP; return
   CODE_FOR_nothing if the target does not have such an insn,
   or if it is unsuitable for optimization type OPT_TYPE.  */
//原型  direct_optab_handler optabs-query.h optabs-query.cc 重载函数
enum insn_code mtcs_opinit_direct_optab_handler (MtcsOpinit *self,convert_optab optab, machine_mode mode,
            optimization_type opt_type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   insn_code icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(self,optab, mode);
   n_debug("mtcs_opinit_direct_optab_handler --- %d %d %d mtcsTarget:%p\n",optab,mode,icode,mtcsTarget);
   if (icode == CODE_FOR_nothing
   || !mtcsTarget/*!targetm.optab_supported_p*/->optab_supported_p(mtcsTarget,(int)optab, mode, mode, opt_type))
      return CODE_FOR_nothing;

   return icode;
}
