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

#ifndef __GCC_MTCS_OPINIT__
#define __GCC_MTCS_OPINIT__

#include "../nlib.h"
#include "insn-opinit.h"
#include "mtcsmode.h"
#include "mtcscomponent.h"


typedef struct _MtcsOpinit MtcsOpinit;
struct _MtcsOpinit
{
    MtcsComponent parent;
    //原型   raw_optab_handler (scode); optabs-query.h  各个平台的insn-opinit.cc实现
    enum insn_code (*raw_optab_handler)(MtcsOpinit *self,unsigned scode);
    //原型 extern bool swap_optab_enable (optab, machine_mode, bool); insn-opinit.h insn-opinit.cc
    bool (*swap_optab_enable)(MtcsOpinit *self,optab op, machine_mode m, bool set);
    //原型 extern void init_all_optabs (struct target_optabs *); insn-opinit.h insn-opinit.cc
    void (*init_all_optabs)(MtcsOpinit *self,struct target_optabs *optabs);
    //原型 extern bool partial_vectors_supported_p (); insn-opinit.h insn-opinit.cc
    bool (*partial_vectors_supported_p)(MtcsOpinit *self);


    /**
    * #define NUM_OPTABS          446
    * #define NUM_CONVLIB_OPTABS  15
    * #define NUM_NORMLIB_OPTABS  80
    * #define NUM_OPTAB_PATTERNS  243
    */
    nuint num_optabs;
    nuint num_convlib_optabs;
    nuint num_normlib_optabs;
    nuint num_optab_patterns;
    //原型 default_target_optabs  insn-opinit.h  insn-opinit.h optabs-query.cc 定义
    struct target_optabs default_target_optabs;
    //原型 this_target_optabs  insn-opinit.h   optabs-query.cc 定义
    struct target_optabs *thisTargetOptabs;//optabs;//因为主机的pat_enable[NUM_OPTAB_PATTERNS]总是比nvptx的大，所以可以重用insn-opinit.h的target_optabs
    //原型 this_fn_optabs  insn-opinit.h   optabs-query.cc 定义
    struct target_optabs *this_fn_optabs;
};

void           mtcs_opinit_init(MtcsOpinit *self);
//原型 optab_handler optabs-query.h
enum insn_code mtcs_opinit_optab_handler(MtcsOpinit *self,optab op, mtcs_mode mode);
//原型 convert_optab_handler optab-query.h 重载函数
enum insn_code mtcs_opinit_convert_optab_handler (MtcsOpinit *self,convert_optab op,
             machine_mode to_mode,machine_mode from_mode);
enum insn_code mtcs_opinit_convert_optab_handler (MtcsOpinit *self,convert_optab optab, machine_mode to_mode,
             machine_mode from_mode, optimization_type opt_type);
void           mtcs_opinit_set_number_optabs(MtcsOpinit *self,int number);
void           mtcs_opinit_set_number_convlib_optabs(MtcsOpinit *self,int number);
void           mtcs_opinit_set_number_normlib_optabs(MtcsOpinit *self,int number);
void           mtcs_opinit_set_number_optab_patterns(MtcsOpinit *self,int number);
//原型 extern void init_all_optabs (struct target_optabs *); insn-opinit.h
void           mtcs_opinit_init_all_optabs(MtcsOpinit *self,struct target_optabs *optabs);
//原型 extern bool partial_vectors_supported_p (); insn-opinit.h insn-opinit.cc
bool           mtcs_opinit_partial_vectors_supported_p(MtcsOpinit *self);

/* Return the insn used to implement mode MODE of OP, or CODE_FOR_nothing
   if the target does not have such an insn.  */
//原型 direct_optab_handler optabs-query.h
inline enum insn_code mtcs_opinit_direct_optab_handler (MtcsOpinit *self,direct_optab op, machine_mode mode)
{
  return mtcs_opinit_optab_handler (self,op, mode);
}
//原型  direct_optab_handler optabs-query.h optabs-query.cc 重载函数
enum insn_code mtcs_opinit_direct_optab_handler (MtcsOpinit *self,convert_optab optab, machine_mode mode,
            optimization_type opt_type);
//原型 extern bool swap_optab_enable (optab, machine_mode, bool); insn-opinit.h insn-opinit.cc
bool mtcs_opinit_swap_optab_enable (MtcsOpinit *self,optab op, machine_mode m, bool set);

#endif

