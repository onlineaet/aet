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

#ifndef __GCC_MTCS_BUILTINS__
#define __GCC_MTCS_BUILTINS__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcsmicro.h"
#include <mpc.h>


typedef struct _MtcsBuiltins MtcsBuiltins;
struct _MtcsBuiltins
{
    MtcsComponent parent;
    //原型 setjmp_alias_set builtins.cc
    alias_set_type setjmp_alias_set;// = -1;
    /* For each register that may be used for calling a function, this
       gives a mode used to copy the register's value.  VOIDmode indicates
       the register is not used for calling a function.  If the machine
       has register windows, this gives only the outbound registers.
       INCOMING_REGNO gives the corresponding inbound register.  */
    fixed_size_mode_pod x_apply_args_mode[MAX_FIRST_PSEUDO_REGISTER/*!FIRST_PSEUDO_REGISTER*/];

    /* For each register that may be used for returning values, this gives
       a mode used to copy the register's value.  VOIDmode indicates the
       register is not used for returning values.  If the machine has
       register windows, this gives only the outbound registers.
       INCOMING_REGNO gives the corresponding inbound register.  */
    fixed_size_mode_pod x_apply_result_mode[MAX_FIRST_PSEUDO_REGISTER/*!FIRST_PSEUDO_REGISTER*/];

    /* Nonzero iff the arrays above have been initialized.  The _plus_one suffix
       is for zero initialization to make it an unreasonable size, used to signal
       that the size and the corresponding mode array has not been
       initialized.  */
    int x_apply_args_size_plus_one;
    int x_apply_result_size_plus_one;

    //从主机来的 gcall转为平台的call。
    nboolean (*replace_call)(MtcsBuiltins *self, gimple *call);
    //主机的内建函数调用转为平台的内建函数调用，并做优化。例如 expf转成平台的调用
    nboolean (*convert_call)(MtcsBuiltins *self, gimple *call);

    //expand内建函数
    rtx (*expand_builtin_fn)(MtcsBuiltins *self,tree exp, rtx target, rtx subtarget, machine_mode mode, int ignore);
    //expand内部函数
    rtx (*expand_internal_fn)(MtcsBuiltins *self,tree exp, rtx target, rtx subtarget, machine_mode mode, int ignore);
    //是否支持内建函数
    nboolean (*support_builtin_fn)(MtcsBuiltins *self,tree fndecl);

};

void mtcs_builtins_init(MtcsBuiltins *self);
//原型 expand_builtin_trap builtins.h builtins.cc
void mtcs_builtins_expand_builtin_trap (MtcsBuiltins *self);
//原型 expand_builtin_setjmp_receiver builtins.h builtins.cc
void mtcs_builtins_expand_builtin_setjmp_receiver (MtcsBuiltins *self,rtx receiver_label);
//原型 expand_builtin_setjmp_setup builtins.h builtins.cc
void mtcs_builtins_expand_builtin_setjmp_setup (MtcsBuiltins *self,rtx buf_addr, rtx receiver_label);
//原型 expand_builtin_update_setjmp_buf builtins.h builtins.cc
void mtcs_builtins_expand_builtin_update_setjmp_buf (MtcsBuiltins *self,rtx buf_addr);
//原型 expand_builtin builtins.h builtins.cc
rtx mtcs_builtins_expand_builtin (MtcsBuiltins *self,tree exp, rtx target, rtx subtarget,
        machine_mode mode, int ignore);
//原型 expand_builtin_saveregs builtins.h builtins.cc
rtx mtcs_builtins_expand_builtin_saveregs (MtcsBuiltins *self);
//原型 expand_builtin_memset builtins.h builtins.cc
rtx mtcs_builtins_expand_builtin_memset (MtcsBuiltins *self,tree exp, rtx target, machine_mode mode);
//原型 fold_builtin_next_arg builtins.h butilins.cc
bool mtcs_builtins_fold_builtin_next_arg (MtcsBuiltins *self,tree exp, bool va_start_p);
//原型 std_expand_builtin_va_start builtins.h butilins.cc
void mtcs_builtins_std_expand_builtin_va_start (MtcsBuiltins *self,tree valist, rtx nextarg);
//原型 maybe_emit_call_builtin___clear_cache builtins.h builtins.cc
void mtcs_builtins_maybe_emit_call_builtin___clear_cache (MtcsBuiltins *self,rtx begin, rtx end);
//原型 default_emit_call_builtin___clear_cache targhooks.h builtins.cc
void mtcs_builtins_default_emit_call_builtin___clear_cache (MtcsBuiltins *self,rtx begin, rtx end);
//原型 get_memory_rtx builtins.h builtins.cc
rtx mtcs_builtins_get_memory_rtx (MtcsBuiltins *self,tree exp, tree len);
//原型 builtin_memset_read_str builtins.h builtins.cc 函数指针 by_pieces_constfn 是它的原型
rtx mtcs_builtins_builtin_memset_read_str (void *data, void *prev,
          HOST_WIDE_INT offset ATTRIBUTE_UNUSED, fixed_size_mode mode);
//原型 do_mpc_arg2 builtins.h builtins.cc
tree mtcs_builtins_do_mpc_arg2 (MtcsBuiltins *self,tree arg0, tree arg1, tree type, int do_nonfinite,
        int (*func)(mpc_ptr, mpc_srcptr, mpc_srcptr, mpc_rnd_t));
//原型 get_object_alignment builtins.h builtins.cc
unsigned int mtcs_builtins_get_object_alignment (MtcsBuiltins *self,tree exp);
//原型 get_object_alignment_1 builtins.h builtins.cc
bool mtcs_builtins_get_object_alignment_1 (MtcsBuiltins *self,tree exp, unsigned int *alignp,
         unsigned HOST_WIDE_INT *bitposp);
//原型 get_object_alignment_2 builtins.h builtins.cc
bool mtcs_builtins_get_object_alignment_2 (MtcsBuiltins *self,tree exp, unsigned int *alignp,
         unsigned HOST_WIDE_INT *bitposp, bool addr_p);

//从主机来的 gcall转为平台的call。
nboolean mtcs_builtins_replace_call(MtcsBuiltins *self, gimple *call);
//主机的内建函数调用转为平台的内建函数调用，并做优化
nboolean mtcs_builtins_convert_call(MtcsBuiltins *self, gimple *call);

nboolean mtcs_builtins_support_builtin_fn(MtcsBuiltins *self,tree fndecl);
rtx      mtcs_builtins_expand_mtcs_builtin (MtcsBuiltins *self,tree exp, rtx target, rtx subtarget,
               machine_mode mode, int ignore);
rtx mtcs_builtins_expand_mtcs_internal (MtcsBuiltins *self,tree exp, rtx target, rtx subtarget,
        machine_mode mode, int ignore);
//原型 fold_builtin_call_array builtins.h builtins.cc
tree mtcs_builtins_fold_builtin_call_array (MtcsBuiltins *self,location_t loc, tree,
          tree fn,
          int n,
          tree *argarray);
//原型 fold_builtin_expect builtins.h builtins.cc
tree mtcs_butiltins_fold_builtin_expect (MtcsBuiltins *self,location_t loc, tree arg0, tree arg1, tree arg2,
           tree arg3);

nboolean mtcs_builtins_is_builtin_fn(tree fndecl);
nboolean mtcs_builtins_is_internal_fn(tree fndecl);
int      mtcs_builtins_get_code(tree fndecl);
//原型 get_builtin_sync_mem  butilins.cc 原本是static方法，但mtcsptxbuiltins需要调用该方法
rtx      mtcs_builtins_get_builtin_sync_mem (MtcsBuiltins *self,tree loc, machine_mode mode);
//原型 expand_expr_force_mode  butilins.cc 原本是static方法，但mtcsptxbuiltins需要调用该方法
rtx      mtcs_builtins_expand_expr_force_mode (MtcsBuiltins *self,tree exp, machine_mode mode);
//原型 get_memmodel  butilins.cc 原本是static方法 但mtcsptxbuiltins需要调用该方法
enum memmodel mtcs_builtins_get_memmodel (MtcsBuiltins *self,tree exp);
//原型 replacement_internal_fn builtins.h builtins.cc
internal_fn mtcs_builtins_replacement_internal_fn (MtcsBuiltins *self,gcall *call);

#endif
