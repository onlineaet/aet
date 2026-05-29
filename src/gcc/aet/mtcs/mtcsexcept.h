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


#ifndef __GCC_MTCS_EXCEPT__
#define __GCC_MTCS_EXCEPT__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcspass.h"
#include "except.h"

typedef struct _MtcsExcept MtcsExcept;
struct _MtcsExcept
{
    MtcsComponent parent;
    GTY(()) int call_site_base;
    GTY(()) hash_map<tree_hash, tree> *type_to_runtime_map;
    GTY(()) tree setjmp_fn;
    /* Describe the SjLj_Function_Context structure.  */
    GTY(()) tree sjlj_fc_type_node;
    int sjlj_fc_call_site_ofs;
    int sjlj_fc_data_ofs;
    int sjlj_fc_personality_ofs;
    int sjlj_fc_lsda_ofs;
    int sjlj_fc_jbuf_ofs;
    vec<int> sjlj_lp_call_site_index;
};

MtcsExcept *mtcs_except_new(MtcsMode *mtcsMode);
void mtcs_except_output_function_exception_table (MtcsExcept *self,int section);
//原型 update_sjlj_context except.h except.cc
void mtcs_except_update_sjlj_context (MtcsExcept *self);
//原型 init_eh_for_function except.h except.cc
void mtcs_except_init_eh_for_function (MtcsExcept *self);
//原型 sjlj_emit_function_exit_after except.h except.cc
void mtcs_except_sjlj_emit_function_exit_after (MtcsExcept *self,rtx_insn *after);
//原型 expand_eh_return except.h except.cc
void mtcs_except_expand_eh_return (MtcsExcept *self);
//原型 set_eh_throw_stmt_table except.h except.cc
void mtcs_except_set_eh_throw_stmt_table (MtcsExcept *self,function *fun, hash_map<gimple *, int> *table);
//原型 finish_eh_generation except.h except.cc
void mtcs_except_finish_eh_generation (MtcsExcept *self);
//原型 expand_builtin_unwind_init except.h except.cc
void mtcs_except_expand_builtin_unwind_init (MtcsExcept *self);
//原型 expand_builtin_frob_return_addr except.h except.cc
rtx mtcs_except_expand_builtin_frob_return_addr (MtcsExcept *self,tree addr_tree);
//原型 expand_builtin_extract_return_addr except.h except.cc
rtx mtcs_except_expand_builtin_extract_return_addr (MtcsExcept *self,tree addr_tree);
//原型 expand_builtin_eh_return_data_regno except.h except.cc
rtx mtcs_except_expand_builtin_eh_return_data_regno (MtcsExcept *self,tree exp);
//原型 expand_builtin_eh_return except.h except.cc
void mtcs_except_expand_builtin_eh_return (MtcsExcept *self,tree stackadj_tree ATTRIBUTE_UNUSED,
           tree handler_tree);
//原型 expand_builtin_extend_pointer except.h except.cc
rtx mtcs_except_expand_builtin_extend_pointer (MtcsExcept *self,tree addr_tree);
//原型 expand_builtin_eh_pointer except.h except.cc
rtx mtcs_except_expand_builtin_eh_pointer (MtcsExcept *self,tree exp);
//原型 expand_builtin_eh_filter except.h except.cc
rtx mtcs_except_expand_builtin_eh_filter (MtcsExcept *self,tree exp);
//原型 expand_builtin_eh_copy_values except.h except.cc
rtx mtcs_except_expand_builtin_eh_copy_values (MtcsExcept *self,tree exp);
//原型 init_eh topleve.h except.cc
void mtcs_except_init_eh (MtcsExcept *self);
//原型 insn_could_throw_p rtl.h except.cc
bool mtcs_except_insn_could_throw_p (MtcsExcept *self,const_rtx insn);
//原型 copy_reg_eh_region_note_backward rtl.h except.cc
void mtcs_except_copy_reg_eh_region_note_backward (MtcsExcept *self,rtx note_or_insn, rtx_insn *last, rtx first);
//原型 get_eh_region_from_rtx except.h except.cc
eh_region mtcs_except_get_eh_region_from_rtx (MtcsExcept *self,const_rtx insn);
//原型 can_nonlocal_goto rtl.h except.cc
bool mtcs_except_can_nonlocal_goto (MtcsExcept *self,const rtx_insn *insn);

//原型 NEXT_PASS (pass_convert_to_eh_region_ranges, 1);  RTL_PASS  except.cc  eh_ranges   y 有条件执行 cfun->eh->region_tree == NUL..
typedef struct _MtcsPassConvertToEhRegionRanges MtcsPassConvertToEhRegionRanges;
struct _MtcsPassConvertToEhRegionRanges
{
   MtcsPass parent;
};
MtcsPassConvertToEhRegionRanges *mtcs_pass_convert_to_eh_region_ranges_new (MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_set_nothrow_function_flags, 1);  RTL_PASS  except.cc  nothrow   y 无条件执行 set_nothrow_function_flags.
typedef struct _MtcsPassSetNothrowFuncitonFlags MtcsPassSetNothrowFuncitonFlags;
struct _MtcsPassSetNothrowFuncitonFlags
{
   MtcsPass parent;
};
MtcsPassSetNothrowFuncitonFlags *mtcs_pass_set_nothrow_function_flags_new (MtcsMode *mtcsMode);


#endif
