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

#ifndef __GCC_TARGET_RTX__
#define __GCC_TARGET_RTX__

#include "../../nlib.h"
#include "machinetarget.h"
#include "../mtcsfuncabi.h"
#include "../mtcsmicro.h"
#include "../mtcscalls.h"

/**
 * 生成RTX
 */
typedef struct _TargetRtx TargetRtx;
struct _TargetRtx
{
   MachineTarget parent;

   //正则表达式 “,(.*)”第一个逗号后所有的内容。以下内容来自 target-insns.def
   enum insn_code code_for_allocate_stack;
   enum insn_code code_for_atomic_test_and_set;
   enum insn_code code_for_builtin_longjmp;
   enum insn_code code_for_builtin_setjmp_receiver;
   enum insn_code code_for_builtin_setjmp_setup;
   enum insn_code code_for_canonicalize_funcptr_for_compare ;
   enum insn_code code_for_call;
   enum insn_code code_for_call_pop;
   enum insn_code code_for_call_value;
   enum insn_code code_for_call_value_pop;
   enum insn_code code_for_casesi;
   enum insn_code code_for_check_stack;
   enum insn_code code_for_clear_cache;
   enum insn_code code_for_doloop_begin;
   enum insn_code code_for_doloop_end;
   enum insn_code code_for_eh_return;
   enum insn_code code_for_epilogue;
   enum insn_code code_for_exception_receiver;
   enum insn_code code_for_extv;
   enum insn_code code_for_extzv;
   enum insn_code code_for_indirect_jump;
   enum insn_code code_for_insv;
   enum insn_code code_for_jump;
   enum insn_code code_for_load_multiple;
   enum insn_code code_for_mem_thread_fence;
   enum insn_code code_for_memory_barrier;
   enum insn_code code_for_memory_blockage;
   enum insn_code code_for_movstr;
   enum insn_code code_for_nonlocal_goto;
   enum insn_code code_for_nonlocal_goto_receiver;
   enum insn_code code_for_oacc_dim_pos;
   enum insn_code code_for_oacc_dim_size;
   enum insn_code code_for_oacc_fork;
   enum insn_code code_for_oacc_join;
   enum insn_code code_for_omp_simt_enter;
   enum insn_code code_for_omp_simt_exit;
   enum insn_code code_for_omp_simt_lane;
   enum insn_code code_for_omp_simt_last_lane;
   enum insn_code code_for_omp_simt_ordered;
   enum insn_code code_for_omp_simt_vote_any;
   enum insn_code code_for_omp_simt_xchg_bfly;
   enum insn_code code_for_omp_simt_xchg_idx;
   enum insn_code code_for_prefetch;
   enum insn_code code_for_probe_stack;
   enum insn_code code_for_probe_stack_address;
   enum insn_code code_for_prologue;
   enum insn_code code_for_ptr_extend;
   enum insn_code code_for_reload_load_address;
   enum insn_code code_for_restore_stack_block;
   enum insn_code code_for_restore_stack_function;
   enum insn_code code_for_restore_stack_nonlocal;
   enum insn_code code_for_return;
   enum insn_code code_for_save_stack_block;
   enum insn_code code_for_save_stack_function;
   enum insn_code code_for_save_stack_nonlocal;
   enum insn_code code_for_sibcall;
   enum insn_code code_for_sibcall_epilogue;
   enum insn_code code_for_sibcall_value;
   enum insn_code code_for_simple_return;
   enum insn_code code_for_split_stack_prologue;
   enum insn_code code_for_split_stack_space_check;
   enum insn_code code_for_stack_protect_combined_set;
   enum insn_code code_for_stack_protect_set;
   enum insn_code code_for_stack_protect_combined_test;
   enum insn_code code_for_stack_protect_test;
   enum insn_code code_for_store_multiple;
   enum insn_code code_for_tablejump;
   enum insn_code code_for_trap;
   enum insn_code code_for_unique;
   enum insn_code code_for_untyped_call;
   enum insn_code code_for_untyped_return;

   rtx_insn *(*gen_allocate_stack) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_atomic_test_and_set) (TargetRtx *self,rtx x0, rtx x1, rtx x2);
   rtx_insn *(*gen_builtin_longjmp) (TargetRtx *self,rtx x0);
   rtx_insn *(*gen_builtin_setjmp_receiver) (TargetRtx *self,rtx x0);
   rtx_insn *(*gen_builtin_setjmp_setup) (TargetRtx *self,rtx x0);
   rtx_insn *(*gen_canonicalize_funcptr_for_compare) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_call) (TargetRtx *self,rtx x0, rtx opt1, rtx opt2, rtx opt3);
   rtx_insn *(*gen_call_pop) (TargetRtx *self,rtx x0, rtx opt1, rtx opt2, rtx opt3);
   rtx_insn *(*gen_call_value) (TargetRtx *self,rtx x0, rtx x1, rtx opt2, rtx opt3, rtx opt4);
   rtx_insn *(*gen_call_value_pop) (TargetRtx *self,rtx x0, rtx x1, rtx opt2, rtx opt3, rtx opt4);
   rtx_insn *(*gen_casesi) (TargetRtx *self,rtx x0, rtx x1, rtx x2, rtx x3, rtx x4);
   rtx_insn *(*gen_check_stack) (TargetRtx *self,rtx x0);
   rtx_insn *(*gen_clear_cache) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_doloop_begin) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_doloop_end) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_eh_return) (TargetRtx *self,rtx x0);
   rtx_insn *(*gen_epilogue) (TargetRtx *self);
   rtx_insn *(*gen_exception_receiver) (TargetRtx *self);
   rtx_insn *(*gen_extv) (TargetRtx *self,rtx x0, rtx x1, rtx x2, rtx x3);
   rtx_insn *(*gen_extzv) (TargetRtx *self,rtx x0, rtx x1, rtx x2, rtx x3);
   rtx_insn *(*gen_indirect_jump) (TargetRtx *self,rtx x0);
   rtx_insn *(*gen_insv) (TargetRtx *self,rtx x0, rtx x1, rtx x2, rtx x3);
   rtx_insn *(*gen_jump) (TargetRtx *self,rtx x0);
   rtx_insn *(*gen_load_multiple) (TargetRtx *self,rtx x0, rtx x1, rtx x2);
   rtx_insn *(*gen_mem_thread_fence) (TargetRtx *self,rtx x0);
   rtx_insn *(*gen_memory_barrier) (TargetRtx *self);
   rtx_insn *(*gen_memory_blockage) (TargetRtx *self);
   rtx_insn *(*gen_movstr) (TargetRtx *self,rtx x0, rtx x1, rtx x2);
   rtx_insn *(*gen_nonlocal_goto) (TargetRtx *self,rtx x0, rtx x1, rtx x2, rtx x3);
   rtx_insn *(*gen_nonlocal_goto_receiver) (TargetRtx *self);
   rtx_insn *(*gen_oacc_dim_pos) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_oacc_dim_size) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_oacc_fork) (TargetRtx *self,rtx x0, rtx x1, rtx x2);
   rtx_insn *(*gen_oacc_join) (TargetRtx *self,rtx x0, rtx x1, rtx x2);
   rtx_insn *(*gen_omp_simt_enter) (TargetRtx *self,rtx x0, rtx x1, rtx x2);
   rtx_insn *(*gen_omp_simt_exit) (TargetRtx *self,rtx x0);
   rtx_insn *(*gen_omp_simt_lane) (TargetRtx *self,rtx x0);
   rtx_insn *(*gen_omp_simt_last_lane) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_omp_simt_ordered) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_omp_simt_vote_any) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_omp_simt_xchg_bfly) (TargetRtx *self,rtx x0, rtx x1, rtx x2);
   rtx_insn *(*gen_omp_simt_xchg_idx) (TargetRtx *self,rtx x0, rtx x1, rtx x2);
   rtx_insn *(*gen_prefetch) (TargetRtx *self,rtx x0, rtx x1, rtx x2);
   rtx_insn *(*gen_probe_stack) (TargetRtx *self,rtx x0);
   rtx_insn *(*gen_probe_stack_address) (TargetRtx *self,rtx x0);
   rtx_insn *(*gen_prologue) (TargetRtx *self);
   rtx_insn *(*gen_ptr_extend) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_reload_load_address) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_restore_stack_block) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_restore_stack_function) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_restore_stack_nonlocal) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_return) (TargetRtx *self);
   rtx_insn *(*gen_save_stack_block) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_save_stack_function) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_save_stack_nonlocal) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_sibcall) (TargetRtx *self,rtx x0, rtx opt1, rtx opt2, rtx opt3);
   rtx_insn *(*gen_sibcall_epilogue) (TargetRtx *self);
   rtx_insn *(*gen_sibcall_value) (TargetRtx *self,rtx x0, rtx x1, rtx opt2, rtx opt3,rtx opt4);
   rtx_insn *(*gen_simple_return) (TargetRtx *self);
   rtx_insn *(*gen_split_stack_prologue) (TargetRtx *self);
   rtx_insn *(*gen_split_stack_space_check) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_stack_protect_combined_set) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_stack_protect_set) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_stack_protect_combined_test) (TargetRtx *self,rtx x0, rtx x1, rtx x2);
   rtx_insn *(*gen_stack_protect_test) (TargetRtx *self,rtx x0, rtx x1, rtx x2);
   rtx_insn *(*gen_store_multiple) (TargetRtx *self,rtx x0, rtx x1, rtx x2);
   rtx_insn *(*gen_tablejump) (TargetRtx *self,rtx x0, rtx x1);
   rtx_insn *(*gen_trap) (TargetRtx *self);
   rtx_insn *(*gen_unique) (TargetRtx *self);
   rtx_insn *(*gen_untyped_call) (TargetRtx *self,rtx x0, rtx x1, rtx x2);
   rtx_insn *(*gen_untyped_return) (TargetRtx *self,rtx x0, rtx x1);


   bool (*have_allocate_stack) (TargetRtx  *self);
   bool (*have_atomic_test_and_set) (TargetRtx  *self);
   bool (*have_builtin_longjmp) (TargetRtx  *self);
   bool (*have_builtin_setjmp_receiver) (TargetRtx  *self);
   bool (*have_builtin_setjmp_setup) (TargetRtx  *self);
   bool (*have_canonicalize_funcptr_for_compare) (TargetRtx  *self);
   bool (*have_call) (TargetRtx  *self);
   bool (*have_call_pop) (TargetRtx  *self);
   bool (*have_call_value) (TargetRtx  *self);
   bool (*have_call_value_pop) (TargetRtx  *self);
   bool (*have_casesi) (TargetRtx  *self);
   bool (*have_check_stack) (TargetRtx  *self);
   bool (*have_clear_cache) (TargetRtx  *self);
   bool (*have_doloop_begin) (TargetRtx  *self);
   bool (*have_doloop_end) (TargetRtx  *self);
   bool (*have_eh_return) (TargetRtx  *self);
   bool (*have_epilogue) (TargetRtx  *self);
   bool (*have_exception_receiver) (TargetRtx  *self);
   bool (*have_extv) (TargetRtx  *self);
   bool (*have_extzv) (TargetRtx  *self);
   bool (*have_indirect_jump) (TargetRtx  *self);
   bool (*have_insv) (TargetRtx  *self);
   bool (*have_jump) (TargetRtx  *self);
   bool (*have_load_multiple) (TargetRtx  *self);
   bool (*have_mem_thread_fence) (TargetRtx  *self);
   bool (*have_memory_barrier) (TargetRtx  *self);
   bool (*have_memory_blockage) (TargetRtx  *self);
   bool (*have_movstr) (TargetRtx  *self);
   bool (*have_nonlocal_goto) (TargetRtx  *self);
   bool (*have_nonlocal_goto_receiver) (TargetRtx  *self);
   bool (*have_oacc_dim_pos) (TargetRtx  *self);
   bool (*have_oacc_dim_size) (TargetRtx  *self);
   bool (*have_oacc_fork) (TargetRtx  *self);
   bool (*have_oacc_join) (TargetRtx  *self);
   bool (*have_omp_simt_enter) (TargetRtx  *self);
   bool (*have_omp_simt_exit) (TargetRtx  *self);
   bool (*have_omp_simt_lane) (TargetRtx  *self);
   bool (*have_omp_simt_last_lane) (TargetRtx  *self);
   bool (*have_omp_simt_ordered) (TargetRtx  *self);
   bool (*have_omp_simt_vote_any) (TargetRtx  *self);
   bool (*have_omp_simt_xchg_bfly) (TargetRtx  *self);
   bool (*have_omp_simt_xchg_idx) (TargetRtx  *self);
   bool (*have_prefetch) (TargetRtx  *self);
   bool (*have_probe_stack) (TargetRtx  *self);
   bool (*have_probe_stack_address) (TargetRtx  *self);
   bool (*have_prologue) (TargetRtx  *self);
   bool (*have_ptr_extend) (TargetRtx  *self);
   bool (*have_reload_load_address) (TargetRtx  *self);
   bool (*have_restore_stack_block) (TargetRtx  *self);
   bool (*have_restore_stack_function) (TargetRtx  *self);
   bool (*have_restore_stack_nonlocal) (TargetRtx  *self);
   bool (*have_return) (TargetRtx  *self);
   bool (*have_save_stack_block) (TargetRtx  *self);
   bool (*have_save_stack_function) (TargetRtx  *self);
   bool (*have_save_stack_nonlocal) (TargetRtx  *self);
   bool (*have_sibcall) (TargetRtx  *self);
   bool (*have_sibcall_epilogue) (TargetRtx  *self);
   bool (*have_sibcall_value) (TargetRtx  *self);
   bool (*have_simple_return) (TargetRtx  *self);
   bool (*have_split_stack_prologue) (TargetRtx  *self);
   bool (*have_split_stack_space_check) (TargetRtx  *self);
   bool (*have_stack_protect_combined_set) (TargetRtx  *self);
   bool (*have_stack_protect_set) (TargetRtx  *self);
   bool (*have_stack_protect_combined_test) (TargetRtx  *self);
   bool (*have_stack_protect_test) (TargetRtx  *self);
   bool (*have_store_multiple) (TargetRtx  *self);
   bool (*have_tablejump) (TargetRtx  *self);
   bool (*have_trap) (TargetRtx  *self);
   bool (*have_unique) (TargetRtx  *self);
   bool (*have_untyped_call) (TargetRtx  *self);
   bool (*have_untyped_return) (TargetRtx  *self);
};

void  target_rtx_init(TargetRtx *self);
//以下代码由mtcsgentargetdef.c中的方法 createTargetCFileHFile 生成。复制粘贴过来的。
rtx_insn * target_rtx_gen_allocate_stack (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_atomic_test_and_set (TargetRtx *self,rtx x0, rtx x1, rtx x2);
rtx_insn * target_rtx_gen_builtin_longjmp (TargetRtx *self,rtx x0);
rtx_insn * target_rtx_gen_builtin_setjmp_receiver (TargetRtx *self,rtx x0);
rtx_insn * target_rtx_gen_builtin_setjmp_setup (TargetRtx *self,rtx x0);
rtx_insn * target_rtx_gen_canonicalize_funcptr_for_compare (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_call (TargetRtx *self,rtx x0, rtx opt1, rtx opt2, rtx opt3);
rtx_insn * target_rtx_gen_call_pop (TargetRtx *self,rtx x0, rtx opt1, rtx opt2, rtx opt3);
rtx_insn * target_rtx_gen_call_value (TargetRtx *self,rtx x0, rtx x1, rtx opt2, rtx opt3, rtx opt4);
rtx_insn * target_rtx_gen_call_value_pop (TargetRtx *self,rtx x0, rtx x1, rtx opt2, rtx opt3, rtx opt4);
rtx_insn * target_rtx_gen_casesi (TargetRtx *self,rtx x0, rtx x1, rtx x2, rtx x3, rtx x4);
rtx_insn * target_rtx_gen_check_stack (TargetRtx *self,rtx x0);
rtx_insn * target_rtx_gen_clear_cache (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_doloop_begin (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_doloop_end (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_eh_return (TargetRtx *self,rtx x0);
rtx_insn * target_rtx_gen_epilogue (TargetRtx *self);
rtx_insn * target_rtx_gen_exception_receiver (TargetRtx *self);
rtx_insn * target_rtx_gen_extv (TargetRtx *self,rtx x0, rtx x1, rtx x2, rtx x3);
rtx_insn * target_rtx_gen_extzv (TargetRtx *self,rtx x0, rtx x1, rtx x2, rtx x3);
rtx_insn * target_rtx_gen_indirect_jump (TargetRtx *self,rtx x0);
rtx_insn * target_rtx_gen_insv (TargetRtx *self,rtx x0, rtx x1, rtx x2, rtx x3);
rtx_insn * target_rtx_gen_jump (TargetRtx *self,rtx x0);
rtx_insn * target_rtx_gen_load_multiple (TargetRtx *self,rtx x0, rtx x1, rtx x2);
rtx_insn * target_rtx_gen_mem_thread_fence (TargetRtx *self,rtx x0);
rtx_insn * target_rtx_gen_memory_barrier (TargetRtx *self);
rtx_insn * target_rtx_gen_memory_blockage (TargetRtx *self);
rtx_insn * target_rtx_gen_movstr (TargetRtx *self,rtx x0, rtx x1, rtx x2);
rtx_insn * target_rtx_gen_nonlocal_goto (TargetRtx *self,rtx x0, rtx x1, rtx x2, rtx x3);
rtx_insn * target_rtx_gen_nonlocal_goto_receiver (TargetRtx *self);
rtx_insn * target_rtx_gen_oacc_dim_pos (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_oacc_dim_size (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_oacc_fork (TargetRtx *self,rtx x0, rtx x1, rtx x2);
rtx_insn * target_rtx_gen_oacc_join (TargetRtx *self,rtx x0, rtx x1, rtx x2);
rtx_insn * target_rtx_gen_omp_simt_enter (TargetRtx *self,rtx x0, rtx x1, rtx x2);
rtx_insn * target_rtx_gen_omp_simt_exit (TargetRtx *self,rtx x0);
rtx_insn * target_rtx_gen_omp_simt_lane (TargetRtx *self,rtx x0);
rtx_insn * target_rtx_gen_omp_simt_last_lane (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_omp_simt_ordered (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_omp_simt_vote_any (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_omp_simt_xchg_bfly (TargetRtx *self,rtx x0, rtx x1, rtx x2);
rtx_insn * target_rtx_gen_omp_simt_xchg_idx (TargetRtx *self,rtx x0, rtx x1, rtx x2);
rtx_insn * target_rtx_gen_prefetch (TargetRtx *self,rtx x0, rtx x1, rtx x2);
rtx_insn * target_rtx_gen_probe_stack (TargetRtx *self,rtx x0);
rtx_insn * target_rtx_gen_probe_stack_address (TargetRtx *self,rtx x0);
rtx_insn * target_rtx_gen_prologue (TargetRtx *self);
rtx_insn * target_rtx_gen_ptr_extend (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_reload_load_address (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_restore_stack_block (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_restore_stack_function (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_restore_stack_nonlocal (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_return (TargetRtx *self);
rtx_insn * target_rtx_gen_save_stack_block (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_save_stack_function (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_save_stack_nonlocal (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_sibcall (TargetRtx *self,rtx x0, rtx opt1, rtx opt2, rtx opt3);
rtx_insn * target_rtx_gen_sibcall_epilogue (TargetRtx *self);
rtx_insn * target_rtx_gen_sibcall_value (TargetRtx *self,rtx x0, rtx x1, rtx opt2, rtx opt3, rtx opt4);
rtx_insn * target_rtx_gen_simple_return (TargetRtx *self);
rtx_insn * target_rtx_gen_split_stack_prologue (TargetRtx *self);
rtx_insn * target_rtx_gen_split_stack_space_check (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_stack_protect_combined_set (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_stack_protect_set (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_stack_protect_combined_test (TargetRtx *self,rtx x0, rtx x1, rtx x2);
rtx_insn * target_rtx_gen_stack_protect_test (TargetRtx *self,rtx x0, rtx x1, rtx x2);
rtx_insn * target_rtx_gen_store_multiple (TargetRtx *self,rtx x0, rtx x1, rtx x2);
rtx_insn * target_rtx_gen_tablejump (TargetRtx *self,rtx x0, rtx x1);
rtx_insn * target_rtx_gen_trap (TargetRtx *self);
rtx_insn * target_rtx_gen_unique (TargetRtx *self);
rtx_insn * target_rtx_gen_untyped_call (TargetRtx *self,rtx x0, rtx x1, rtx x2);
rtx_insn * target_rtx_gen_untyped_return (TargetRtx *self,rtx x0, rtx x1);

bool target_rtx_have_allocate_stack (TargetRtx *self);
bool target_rtx_have_atomic_test_and_set (TargetRtx *self);
bool target_rtx_have_builtin_longjmp (TargetRtx *self);
bool target_rtx_have_builtin_setjmp_receiver (TargetRtx *self);
bool target_rtx_have_builtin_setjmp_setup (TargetRtx *self);
bool target_rtx_have_canonicalize_funcptr_for_compare (TargetRtx *self);
bool target_rtx_have_call (TargetRtx *self);
bool target_rtx_have_call_pop (TargetRtx *self);
bool target_rtx_have_call_value (TargetRtx *self);
bool target_rtx_have_call_value_pop (TargetRtx *self);
bool target_rtx_have_casesi (TargetRtx *self);
bool target_rtx_have_check_stack (TargetRtx *self);
bool target_rtx_have_clear_cache (TargetRtx *self);
bool target_rtx_have_doloop_begin (TargetRtx *self);
bool target_rtx_have_doloop_end (TargetRtx *self);
bool target_rtx_have_eh_return (TargetRtx *self);
bool target_rtx_have_epilogue (TargetRtx *self);
bool target_rtx_have_exception_receiver (TargetRtx *self);
bool target_rtx_have_extv (TargetRtx *self);
bool target_rtx_have_extzv (TargetRtx *self);
bool target_rtx_have_indirect_jump (TargetRtx *self);
bool target_rtx_have_insv (TargetRtx *self);
bool target_rtx_have_jump (TargetRtx *self);
bool target_rtx_have_load_multiple (TargetRtx *self);
bool target_rtx_have_mem_thread_fence (TargetRtx *self);
bool target_rtx_have_memory_barrier (TargetRtx *self);
bool target_rtx_have_memory_blockage (TargetRtx *self);
bool target_rtx_have_movstr (TargetRtx *self);
bool target_rtx_have_nonlocal_goto (TargetRtx *self);
bool target_rtx_have_nonlocal_goto_receiver (TargetRtx *self);
bool target_rtx_have_oacc_dim_pos (TargetRtx *self);
bool target_rtx_have_oacc_dim_size (TargetRtx *self);
bool target_rtx_have_oacc_fork (TargetRtx *self);
bool target_rtx_have_oacc_join (TargetRtx *self);
bool target_rtx_have_omp_simt_enter (TargetRtx *self);
bool target_rtx_have_omp_simt_exit (TargetRtx *self);
bool target_rtx_have_omp_simt_lane (TargetRtx *self);
bool target_rtx_have_omp_simt_last_lane (TargetRtx *self);
bool target_rtx_have_omp_simt_ordered (TargetRtx *self);
bool target_rtx_have_omp_simt_vote_any (TargetRtx *self);
bool target_rtx_have_omp_simt_xchg_bfly (TargetRtx *self);
bool target_rtx_have_omp_simt_xchg_idx (TargetRtx *self);
bool target_rtx_have_prefetch (TargetRtx *self);
bool target_rtx_have_probe_stack (TargetRtx *self);
bool target_rtx_have_probe_stack_address (TargetRtx *self);
bool target_rtx_have_prologue (TargetRtx *self);
bool target_rtx_have_ptr_extend (TargetRtx *self);
bool target_rtx_have_reload_load_address (TargetRtx *self);
bool target_rtx_have_restore_stack_block (TargetRtx *self);
bool target_rtx_have_restore_stack_function (TargetRtx *self);
bool target_rtx_have_restore_stack_nonlocal (TargetRtx *self);
bool target_rtx_have_return (TargetRtx *self);
bool target_rtx_have_save_stack_block (TargetRtx *self);
bool target_rtx_have_save_stack_function (TargetRtx *self);
bool target_rtx_have_save_stack_nonlocal (TargetRtx *self);
bool target_rtx_have_sibcall (TargetRtx *self);
bool target_rtx_have_sibcall_epilogue (TargetRtx *self);
bool target_rtx_have_sibcall_value (TargetRtx *self);
bool target_rtx_have_simple_return (TargetRtx *self);
bool target_rtx_have_split_stack_prologue (TargetRtx *self);
bool target_rtx_have_split_stack_space_check (TargetRtx *self);
bool target_rtx_have_stack_protect_combined_set (TargetRtx *self);
bool target_rtx_have_stack_protect_set (TargetRtx *self);
bool target_rtx_have_stack_protect_combined_test (TargetRtx *self);
bool target_rtx_have_stack_protect_test (TargetRtx *self);
bool target_rtx_have_store_multiple (TargetRtx *self);
bool target_rtx_have_tablejump (TargetRtx *self);
bool target_rtx_have_trap (TargetRtx *self);
bool target_rtx_have_unique (TargetRtx *self);
bool target_rtx_have_untyped_call (TargetRtx *self);
bool target_rtx_have_untyped_return (TargetRtx *self);


#endif

