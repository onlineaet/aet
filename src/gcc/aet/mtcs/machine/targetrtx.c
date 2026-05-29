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
#include "../ptx/ptx_options.h"
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
#include "opts.h"

#include "aet/aetprinttree.h"
#include "../mtcstarget.h"
#include "../mtcsfuncabi.h"
#include "../mtcsmicro.h"
#include "../mtcscalls.h"

#include "targetrtx.h"

void  target_rtx_init(TargetRtx *self)
{

}

//以下代码由mtcsgentargetdef.c中的方法 createTargetCFileHFile 生成。复制粘贴过来的。
rtx_insn * target_rtx_gen_allocate_stack (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_allocate_stack(self, x0,  x1);
}

rtx_insn * target_rtx_gen_atomic_test_and_set (TargetRtx *self,rtx x0, rtx x1, rtx x2)
{
   return self->gen_atomic_test_and_set(self, x0,  x1,  x2);
}

rtx_insn * target_rtx_gen_builtin_longjmp (TargetRtx *self,rtx x0)
{
   return self->gen_builtin_longjmp(self, x0);
}

rtx_insn * target_rtx_gen_builtin_setjmp_receiver (TargetRtx *self,rtx x0)
{
   return self->gen_builtin_setjmp_receiver(self, x0);
}

rtx_insn * target_rtx_gen_builtin_setjmp_setup (TargetRtx *self,rtx x0)
{
   return self->gen_builtin_setjmp_setup(self, x0);
}

rtx_insn * target_rtx_gen_canonicalize_funcptr_for_compare (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_canonicalize_funcptr_for_compare(self, x0,  x1);
}

rtx_insn * target_rtx_gen_call (TargetRtx *self,rtx x0, rtx opt1, rtx opt2, rtx opt3)
{
   return self->gen_call(self, x0,  opt1,  opt2,  opt3);
}

rtx_insn * target_rtx_gen_call_pop (TargetRtx *self,rtx x0, rtx opt1, rtx opt2, rtx opt3)
{
   return self->gen_call_pop(self, x0,  opt1,  opt2,  opt3);
}

rtx_insn * target_rtx_gen_call_value (TargetRtx *self,rtx x0, rtx x1, rtx opt2, rtx opt3, rtx opt4)
{
   return self->gen_call_value(self, x0,  x1,  opt2,  opt3,  opt4);
}

rtx_insn * target_rtx_gen_call_value_pop (TargetRtx *self,rtx x0, rtx x1, rtx opt2, rtx opt3, rtx opt4)
{
   return self->gen_call_value_pop(self, x0,  x1,  opt2,  opt3,  opt4);
}

rtx_insn * target_rtx_gen_casesi (TargetRtx *self,rtx x0, rtx x1, rtx x2, rtx x3, rtx x4)
{
   return self->gen_casesi(self, x0,  x1,  x2,  x3,  x4);
}

rtx_insn * target_rtx_gen_check_stack (TargetRtx *self,rtx x0)
{
   return self->gen_check_stack(self, x0);
}

rtx_insn * target_rtx_gen_clear_cache (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_clear_cache(self, x0,  x1);
}

rtx_insn * target_rtx_gen_doloop_begin (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_doloop_begin(self, x0,  x1);
}

rtx_insn * target_rtx_gen_doloop_end (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_doloop_end(self, x0,  x1);
}

rtx_insn * target_rtx_gen_eh_return (TargetRtx *self,rtx x0)
{
   return self->gen_eh_return(self, x0);
}

rtx_insn * target_rtx_gen_epilogue (TargetRtx *self)
{
   return self->gen_epilogue(self);
}

rtx_insn * target_rtx_gen_exception_receiver (TargetRtx *self)
{
   return self->gen_exception_receiver(self);
}

rtx_insn * target_rtx_gen_extv (TargetRtx *self,rtx x0, rtx x1, rtx x2, rtx x3)
{
   return self->gen_extv(self, x0,  x1,  x2,  x3);
}

rtx_insn * target_rtx_gen_extzv (TargetRtx *self,rtx x0, rtx x1, rtx x2, rtx x3)
{
   return self->gen_extzv(self, x0,  x1,  x2,  x3);
}

rtx_insn * target_rtx_gen_indirect_jump (TargetRtx *self,rtx x0)
{
   return self->gen_indirect_jump(self, x0);
}

rtx_insn * target_rtx_gen_insv (TargetRtx *self,rtx x0, rtx x1, rtx x2, rtx x3)
{
   return self->gen_insv(self, x0,  x1,  x2,  x3);
}

rtx_insn * target_rtx_gen_jump (TargetRtx *self,rtx x0)
{
   return self->gen_jump(self, x0);
}

rtx_insn * target_rtx_gen_load_multiple (TargetRtx *self,rtx x0, rtx x1, rtx x2)
{
   return self->gen_load_multiple(self, x0,  x1,  x2);
}

rtx_insn * target_rtx_gen_mem_thread_fence (TargetRtx *self,rtx x0)
{
   return self->gen_mem_thread_fence(self, x0);
}

rtx_insn * target_rtx_gen_memory_barrier (TargetRtx *self)
{
   return self->gen_memory_barrier(self);
}

rtx_insn * target_rtx_gen_memory_blockage (TargetRtx *self)
{
   return self->gen_memory_blockage(self);
}

rtx_insn * target_rtx_gen_movstr (TargetRtx *self,rtx x0, rtx x1, rtx x2)
{
   return self->gen_movstr(self, x0,  x1,  x2);
}

rtx_insn * target_rtx_gen_nonlocal_goto (TargetRtx *self,rtx x0, rtx x1, rtx x2, rtx x3)
{
   return self->gen_nonlocal_goto(self, x0,  x1,  x2,  x3);
}

rtx_insn * target_rtx_gen_nonlocal_goto_receiver (TargetRtx *self)
{
   return self->gen_nonlocal_goto_receiver(self);
}

rtx_insn * target_rtx_gen_oacc_dim_pos (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_oacc_dim_pos(self, x0,  x1);
}

rtx_insn * target_rtx_gen_oacc_dim_size (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_oacc_dim_size(self, x0,  x1);
}

rtx_insn * target_rtx_gen_oacc_fork (TargetRtx *self,rtx x0, rtx x1, rtx x2)
{
   return self->gen_oacc_fork(self, x0,  x1,  x2);
}

rtx_insn * target_rtx_gen_oacc_join (TargetRtx *self,rtx x0, rtx x1, rtx x2)
{
   return self->gen_oacc_join(self, x0,  x1,  x2);
}

rtx_insn * target_rtx_gen_omp_simt_enter (TargetRtx *self,rtx x0, rtx x1, rtx x2)
{
   return self->gen_omp_simt_enter(self, x0,  x1,  x2);
}

rtx_insn * target_rtx_gen_omp_simt_exit (TargetRtx *self,rtx x0)
{
   return self->gen_omp_simt_exit(self, x0);
}

rtx_insn * target_rtx_gen_omp_simt_lane (TargetRtx *self,rtx x0)
{
   return self->gen_omp_simt_lane(self, x0);
}

rtx_insn * target_rtx_gen_omp_simt_last_lane (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_omp_simt_last_lane(self, x0,  x1);
}

rtx_insn * target_rtx_gen_omp_simt_ordered (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_omp_simt_ordered(self, x0,  x1);
}

rtx_insn * target_rtx_gen_omp_simt_vote_any (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_omp_simt_vote_any(self, x0,  x1);
}

rtx_insn * target_rtx_gen_omp_simt_xchg_bfly (TargetRtx *self,rtx x0, rtx x1, rtx x2)
{
   return self->gen_omp_simt_xchg_bfly(self, x0,  x1,  x2);
}

rtx_insn * target_rtx_gen_omp_simt_xchg_idx (TargetRtx *self,rtx x0, rtx x1, rtx x2)
{
   return self->gen_omp_simt_xchg_idx(self, x0,  x1,  x2);
}

rtx_insn * target_rtx_gen_prefetch (TargetRtx *self,rtx x0, rtx x1, rtx x2)
{
   return self->gen_prefetch(self, x0,  x1,  x2);
}

rtx_insn * target_rtx_gen_probe_stack (TargetRtx *self,rtx x0)
{
   return self->gen_probe_stack(self, x0);
}

rtx_insn * target_rtx_gen_probe_stack_address (TargetRtx *self,rtx x0)
{
   return self->gen_probe_stack_address(self, x0);
}

rtx_insn * target_rtx_gen_prologue (TargetRtx *self)
{
   return self->gen_prologue(self);
}

rtx_insn * target_rtx_gen_ptr_extend (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_ptr_extend(self, x0,  x1);
}

rtx_insn * target_rtx_gen_reload_load_address (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_reload_load_address(self, x0,  x1);
}

rtx_insn * target_rtx_gen_restore_stack_block (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_restore_stack_block(self, x0,  x1);
}

rtx_insn * target_rtx_gen_restore_stack_function (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_restore_stack_function(self, x0,  x1);
}

rtx_insn * target_rtx_gen_restore_stack_nonlocal (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_restore_stack_nonlocal(self, x0,  x1);
}

rtx_insn * target_rtx_gen_return (TargetRtx *self)
{
   return self->gen_return(self);
}

rtx_insn * target_rtx_gen_save_stack_block (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_save_stack_block(self, x0,  x1);
}

rtx_insn * target_rtx_gen_save_stack_function (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_save_stack_function(self, x0,  x1);
}

rtx_insn * target_rtx_gen_save_stack_nonlocal (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_save_stack_nonlocal(self, x0,  x1);
}

rtx_insn * target_rtx_gen_sibcall (TargetRtx *self,rtx x0, rtx opt1, rtx opt2, rtx opt3)
{
   return self->gen_sibcall(self, x0,  opt1,  opt2,  opt3);
}

rtx_insn * target_rtx_gen_sibcall_epilogue (TargetRtx *self)
{
   return self->gen_sibcall_epilogue(self);
}

rtx_insn * target_rtx_gen_sibcall_value (TargetRtx *self,rtx x0, rtx x1, rtx opt2, rtx opt3, rtx opt4)
{
   return self->gen_sibcall_value(self, x0,  x1,  opt2,  opt3,  opt4);
}

rtx_insn * target_rtx_gen_simple_return (TargetRtx *self)
{
   return self->gen_simple_return(self);
}

rtx_insn * target_rtx_gen_split_stack_prologue (TargetRtx *self)
{
   return self->gen_split_stack_prologue(self);
}

rtx_insn * target_rtx_gen_split_stack_space_check (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_split_stack_space_check(self, x0,  x1);
}

rtx_insn * target_rtx_gen_stack_protect_combined_set (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_stack_protect_combined_set(self, x0,  x1);
}

rtx_insn * target_rtx_gen_stack_protect_set (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_stack_protect_set(self, x0,  x1);
}

rtx_insn * target_rtx_gen_stack_protect_combined_test (TargetRtx *self,rtx x0, rtx x1, rtx x2)
{
   return self->gen_stack_protect_combined_test(self, x0,  x1,  x2);
}

rtx_insn * target_rtx_gen_stack_protect_test (TargetRtx *self,rtx x0, rtx x1, rtx x2)
{
   return self->gen_stack_protect_test(self, x0,  x1,  x2);
}

rtx_insn * target_rtx_gen_store_multiple (TargetRtx *self,rtx x0, rtx x1, rtx x2)
{
   return self->gen_store_multiple(self, x0,  x1,  x2);
}

rtx_insn * target_rtx_gen_tablejump (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_tablejump(self, x0,  x1);
}

rtx_insn * target_rtx_gen_trap (TargetRtx *self)
{
   return self->gen_trap(self);
}

rtx_insn * target_rtx_gen_unique (TargetRtx *self)
{
   return self->gen_unique(self);
}

rtx_insn * target_rtx_gen_untyped_call (TargetRtx *self,rtx x0, rtx x1, rtx x2)
{
   return self->gen_untyped_call(self, x0,  x1,  x2);
}

rtx_insn * target_rtx_gen_untyped_return (TargetRtx *self,rtx x0, rtx x1)
{
   return self->gen_untyped_return(self, x0,  x1);
}

bool target_rtx_have_allocate_stack (TargetRtx *self)
{
   return self->have_allocate_stack(self);
}

bool target_rtx_have_atomic_test_and_set (TargetRtx *self)
{
   return self->have_atomic_test_and_set(self);
}

bool target_rtx_have_builtin_longjmp (TargetRtx *self)
{
   return self->have_builtin_longjmp(self);
}

bool target_rtx_have_builtin_setjmp_receiver (TargetRtx *self)
{
   return self->have_builtin_setjmp_receiver(self);
}

bool target_rtx_have_builtin_setjmp_setup (TargetRtx *self)
{
   return self->have_builtin_setjmp_setup(self);
}

bool target_rtx_have_canonicalize_funcptr_for_compare (TargetRtx *self)
{
   return self->have_canonicalize_funcptr_for_compare(self);
}

bool target_rtx_have_call (TargetRtx *self)
{
   return self->have_call(self);
}

bool target_rtx_have_call_pop (TargetRtx *self)
{
   return self->have_call_pop(self);
}

bool target_rtx_have_call_value (TargetRtx *self)
{
   return self->have_call_value(self);
}

bool target_rtx_have_call_value_pop (TargetRtx *self)
{
   return self->have_call_value_pop(self);
}

bool target_rtx_have_casesi (TargetRtx *self)
{
   return self->have_casesi(self);
}

bool target_rtx_have_check_stack (TargetRtx *self)
{
   return self->have_check_stack(self);
}

bool target_rtx_have_clear_cache (TargetRtx *self)
{
   return self->have_clear_cache(self);
}

bool target_rtx_have_doloop_begin (TargetRtx *self)
{
   return self->have_doloop_begin(self);
}

bool target_rtx_have_doloop_end (TargetRtx *self)
{
   return self->have_doloop_end(self);
}

bool target_rtx_have_eh_return (TargetRtx *self)
{
   return self->have_eh_return(self);
}

bool target_rtx_have_epilogue (TargetRtx *self)
{
   return self->have_epilogue(self);
}

bool target_rtx_have_exception_receiver (TargetRtx *self)
{
   return self->have_exception_receiver(self);
}

bool target_rtx_have_extv (TargetRtx *self)
{
   return self->have_extv(self);
}

bool target_rtx_have_extzv (TargetRtx *self)
{
   return self->have_extzv(self);
}

bool target_rtx_have_indirect_jump (TargetRtx *self)
{
   return self->have_indirect_jump(self);
}

bool target_rtx_have_insv (TargetRtx *self)
{
   return self->have_insv(self);
}

bool target_rtx_have_jump (TargetRtx *self)
{
   return self->have_jump(self);
}

bool target_rtx_have_load_multiple (TargetRtx *self)
{
   return self->have_load_multiple(self);
}

bool target_rtx_have_mem_thread_fence (TargetRtx *self)
{
   return self->have_mem_thread_fence(self);
}

bool target_rtx_have_memory_barrier (TargetRtx *self)
{
   return self->have_memory_barrier(self);
}

bool target_rtx_have_memory_blockage (TargetRtx *self)
{
   return self->have_memory_blockage(self);
}

bool target_rtx_have_movstr (TargetRtx *self)
{
   return self->have_movstr(self);
}

bool target_rtx_have_nonlocal_goto (TargetRtx *self)
{
   return self->have_nonlocal_goto(self);
}

bool target_rtx_have_nonlocal_goto_receiver (TargetRtx *self)
{
   return self->have_nonlocal_goto_receiver(self);
}

bool target_rtx_have_oacc_dim_pos (TargetRtx *self)
{
   return self->have_oacc_dim_pos(self);
}

bool target_rtx_have_oacc_dim_size (TargetRtx *self)
{
   return self->have_oacc_dim_size(self);
}

bool target_rtx_have_oacc_fork (TargetRtx *self)
{
   return self->have_oacc_fork(self);
}

bool target_rtx_have_oacc_join (TargetRtx *self)
{
   return self->have_oacc_join(self);
}

bool target_rtx_have_omp_simt_enter (TargetRtx *self)
{
   return self->have_omp_simt_enter(self);
}

bool target_rtx_have_omp_simt_exit (TargetRtx *self)
{
   return self->have_omp_simt_exit(self);
}

bool target_rtx_have_omp_simt_lane (TargetRtx *self)
{
   return self->have_omp_simt_lane(self);
}

bool target_rtx_have_omp_simt_last_lane (TargetRtx *self)
{
   return self->have_omp_simt_last_lane(self);
}

bool target_rtx_have_omp_simt_ordered (TargetRtx *self)
{
   return self->have_omp_simt_ordered(self);
}

bool target_rtx_have_omp_simt_vote_any (TargetRtx *self)
{
   return self->have_omp_simt_vote_any(self);
}

bool target_rtx_have_omp_simt_xchg_bfly (TargetRtx *self)
{
   return self->have_omp_simt_xchg_bfly(self);
}

bool target_rtx_have_omp_simt_xchg_idx (TargetRtx *self)
{
   return self->have_omp_simt_xchg_idx(self);
}

bool target_rtx_have_prefetch (TargetRtx *self)
{
   return self->have_prefetch(self);
}

bool target_rtx_have_probe_stack (TargetRtx *self)
{
   return self->have_probe_stack(self);
}

bool target_rtx_have_probe_stack_address (TargetRtx *self)
{
   return self->have_probe_stack_address(self);
}

bool target_rtx_have_prologue (TargetRtx *self)
{
   return self->have_prologue(self);
}

bool target_rtx_have_ptr_extend (TargetRtx *self)
{
   return self->have_ptr_extend(self);
}

bool target_rtx_have_reload_load_address (TargetRtx *self)
{
   return self->have_reload_load_address(self);
}

bool target_rtx_have_restore_stack_block (TargetRtx *self)
{
   return self->have_restore_stack_block(self);
}

bool target_rtx_have_restore_stack_function (TargetRtx *self)
{
   return self->have_restore_stack_function(self);
}

bool target_rtx_have_restore_stack_nonlocal (TargetRtx *self)
{
   return self->have_restore_stack_nonlocal(self);
}

bool target_rtx_have_return (TargetRtx *self)
{
   return self->have_return(self);
}

bool target_rtx_have_save_stack_block (TargetRtx *self)
{
   return self->have_save_stack_block(self);
}

bool target_rtx_have_save_stack_function (TargetRtx *self)
{
   return self->have_save_stack_function(self);
}

bool target_rtx_have_save_stack_nonlocal (TargetRtx *self)
{
   return self->have_save_stack_nonlocal(self);
}

bool target_rtx_have_sibcall (TargetRtx *self)
{
   return self->have_sibcall(self);
}

bool target_rtx_have_sibcall_epilogue (TargetRtx *self)
{
   return self->have_sibcall_epilogue(self);
}

bool target_rtx_have_sibcall_value (TargetRtx *self)
{
   return self->have_sibcall_value(self);
}

bool target_rtx_have_simple_return (TargetRtx *self)
{
   return self->have_simple_return(self);
}

bool target_rtx_have_split_stack_prologue (TargetRtx *self)
{
   return self->have_split_stack_prologue(self);
}

bool target_rtx_have_split_stack_space_check (TargetRtx *self)
{
   return self->have_split_stack_space_check(self);
}

bool target_rtx_have_stack_protect_combined_set (TargetRtx *self)
{
   return self->have_stack_protect_combined_set(self);
}

bool target_rtx_have_stack_protect_set (TargetRtx *self)
{
   return self->have_stack_protect_set(self);
}

bool target_rtx_have_stack_protect_combined_test (TargetRtx *self)
{
   return self->have_stack_protect_combined_test(self);
}

bool target_rtx_have_stack_protect_test (TargetRtx *self)
{
   return self->have_stack_protect_test(self);
}

bool target_rtx_have_store_multiple (TargetRtx *self)
{
   return self->have_store_multiple(self);
}

bool target_rtx_have_tablejump (TargetRtx *self)
{
   return self->have_tablejump(self);
}

bool target_rtx_have_trap (TargetRtx *self)
{
   return self->have_trap(self);
}

bool target_rtx_have_unique (TargetRtx *self)
{
   return self->have_unique(self);
}

bool target_rtx_have_untyped_call (TargetRtx *self)
{
   return self->have_untyped_call(self);
}

bool target_rtx_have_untyped_return (TargetRtx *self)
{
   return self->have_untyped_return(self);
}

