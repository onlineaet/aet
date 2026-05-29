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

#ifndef __GCC_TARGET_CALLS__
#define __GCC_TARGET_CALLS__

#include "../../nlib.h"
#include "machinetarget.h"
#include "../mtcsfuncabi.h"
#include "../mtcsmicro.h"
#include "../mtcscalls.h"

typedef struct _TargetCalls TargetCalls;
struct _TargetCalls
{
   MachineTarget parent;
   //原型 targetm.calls.custom_function_descriptors #define TARGET_CUSTOM_FUNCTION_DESCRIPTORS -1
   int custom_function_descriptors;
   //原型 targetm.calls.omit_struct_return_reg #define TARGET_OMIT_STRUCT_RETURN_REG true
   bool omit_struct_return_reg;
   //原型targetm.calls.struct_value_rtx   #define TARGET_STRUCT_VALUE_RTX hook_rtx_tree_int_null
   rtx (*struct_value_rtx)(TargetCalls *self,tree fndecl,int incoming);
   //原型 targetm.calls.allocate_stack_slots_for_args #define TARGET_ALLOCATE_STACK_SLOTS_FOR_ARGS hook_bool_void_true
   bool (*allocate_stack_slots_for_args)(TargetCalls *self);
   //原型 targetm.calls.promote_function_mode (NULL_TREE, mode, punsignedp, funtype,for_return);#define TARGET_PROMOTE_FUNCTION_MODE default_promote_function_mode
   mtcs_mode (*promote_function_mode)(TargetCalls *self,const_tree type ATTRIBUTE_UNUSED, mtcs_mode mode,
               int *punsignedp ATTRIBUTE_UNUSED,const_tree funtype ATTRIBUTE_UNUSED,int for_return ATTRIBUTE_UNUSED);
   //原型 targetm.calls.return_in_memory (type, fntype) #define TARGET_RETURN_IN_MEMORY nvptx_return_in_memory
   bool (*return_in_memory)(TargetCalls *self,const_tree type, const_tree fntype);
   //原型targetm.calls.function_value (valtype, func ? func : fntype, outgoing); #define TARGET_FUNCTION_VALUE default_function_value
   rtx (*function_value)(TargetCalls *self,const_tree ret_type ATTRIBUTE_UNUSED,
   const_tree fn_decl_or_type, bool outgoing ATTRIBUTE_UNUSED);
   //原型 targetm.calls.libcall_value (mode, fun); #define TARGET_LIBCALL_VALUE default_libcall_value
   rtx (*libcall_value) (TargetCalls *self,machine_mode, const_rtx);
   //原型 targetm.calls.function_arg (args_so_far, ptr_arg);#define TARGET_FUNCTION_ARG default_function_arg
   rtx (*function_arg)(TargetCalls *self,cumulative_args_t arg, const mtcs_function_arg_info &info);
   //原型targetm.calls.function_arg_advance (args_so_far, ptr_arg);#define TARGET_FUNCTION_ARG_ADVANCE nvptx_function_arg_advance
   void (*function_arg_advance)(TargetCalls *self,cumulative_args_t cum_v, const mtcs_function_arg_info &info);
   //原型ttargetm.calls.arg_partial_bytes (args_so_far, arg);#define TARGET_ARG_PARTIAL_BYTES hook_int_CUMULATIVE_ARGS_arg_info_0
   int (*arg_partial_bytes)(TargetCalls *self,cumulative_args_t cum_v, const mtcs_function_arg_info &info);
   //原型targetm.calls.function_arg_padding (passed_mode, type);#define TARGET_FUNCTION_ARG_PADDING default_function_arg_padding
   enum pad_direction (*function_arg_padding)(TargetCalls *self,machine_mode mode, const_tree type);
   //原型targetm.calls.function_arg_boundary (passed_mode, type);#define TARGET_FUNCTION_ARG_BOUNDARY nvptx_function_arg_boundary
   unsigned (*function_arg_boundary)(TargetCalls *self,machine_mode mode ATTRIBUTE_UNUSED, const_tree type ATTRIBUTE_UNUSED);
   //原型targetm.calls.function_arg_round_boundary (passed_mode,type);#define TARGET_FUNCTION_ARG_ROUND_BOUNDARY default_function_arg_round_boundary
   unsigned (*function_arg_round_boundary)(TargetCalls *self,machine_mode mode ATTRIBUTE_UNUSED, const_tree type ATTRIBUTE_UNUSED);
   //原型 targetm.calls.function_arg_offset (passed_mode, type);#define TARGET_FUNCTION_ARG_OFFSET default_function_arg_offset
   HOST_WIDE_INT (*function_arg_offset)(TargetCalls *self,machine_mode mode ATTRIBUTE_UNUSED, const_tree type ATTRIBUTE_UNUSED);
   //原型 targetm.calls.push_argument(unsigned int );#define TARGET_PUSH_ARGUMENT default_push_argument
   bool     (*push_argument) (TargetCalls *self,unsigned int npush);
   //原型 targetm.calls.start_call_args (args_so_far); #define TARGET_START_CALL_ARGS hook_void_CUMULATIVE_ARGS
   void     (*start_call_args) (TargetCalls *self,cumulative_args_t args);
   //原型 targetm.calls.call_args (args_so_far, argvec[i].reg, NULL_TREE); #define TARGET_CALL_ARGS hook_void_CUMULATIVE_ARGS_rtx_tree
   void     (*call_args) (TargetCalls *self,cumulative_args_t cargs, rtx arg, tree fntype);
   //原型 targetm.calls.return_pops_args  #define TARGET_RETURN_POPS_ARGS default_return_pops_args
   poly_int64 (*return_pops_args)(TargetCalls *self,tree fundecl, tree funtype, poly_int64 size);
   //原型 targetm.calls.return_in_msb (tfom) #define TARGET_RETURN_IN_MSB hook_bool_const_tree_false
   bool     (*return_in_msb) (TargetCalls *self,const_tree valtype);
   //原型 targetm.calls.end_call_args (args_so_far); #define TARGET_END_CALL_ARGS hook_void_CUMULATIVE_ARGS
   void     (*end_call_args) (TargetCalls *self,cumulative_args_t cargs);
   //原型  targetm.calls.static_chain (fndecl_or_type, false); #define TARGET_STATIC_CHAIN default_static_chain
   rtx (*static_chain)(TargetCalls *self,const_tree ARG_UNUSED (fndecl_or_type), bool incoming_p);
   //原型 (targetm.calls.split_complex_arg) #define TARGET_SPLIT_COMPLEX_ARG hook_bool_const_tree_true
   bool     (*split_complex_arg) (TargetCalls *self,const_tree valtype);
   //原型 targetm.calls.strict_argument_naming (args_so_far) #define TARGET_STRICT_ARGUMENT_NAMING nvptx_strict_argument_naming
   bool     (*strict_argument_naming) (TargetCalls *self,cumulative_args_t cum_v);
   //原型  targetm.calls.pretend_outgoing_varargs_named (args_so_far) #define TARGET_PRETEND_OUTGOING_VARARGS_NAMED default_pretend_outgoing_varargs_named
   bool (*pretend_outgoing_varargs_named)(TargetCalls *self,cumulative_args_t ca ATTRIBUTE_UNUSED);
   //原型 targetm.calls.setup_incoming_varargs #define TARGET_SETUP_INCOMING_VARARGS default_setup_incoming_varargs
   void (*setup_incoming_varargs)(TargetCalls *self,cumulative_args_t cum_v, const mtcs_function_arg_info &arg,
   int *pretend_size ATTRIBUTE_UNUSED, int no_rtl);
   //原型 targetm.calls.callee_copies (pack_cumulative_args (ca), arg); #define TARGET_CALLEE_COPIES hook_bool_CUMULATIVE_ARGS_arg_info_false
   bool (*callee_copies)(TargetCalls *self,cumulative_args_t ca, const mtcs_function_arg_info &arg);
   //原型 targetm.calls.pass_by_reference (pack_cumulative_args (ca), arg); #define TARGET_PASS_BY_REFERENCE nvptx_pass_by_reference
   bool (*pass_by_reference)(TargetCalls *self,cumulative_args_t, const mtcs_function_arg_info &arg);
   //原型  targetm.calls.warn_parameter_passing_abi (args_so_far, type);#define TARGET_WARN_PARAMETER_PASSING_ABI hook_void_CUMULATIVE_ARGS_tree
   void (*warn_parameter_passing_abi)(TargetCalls *self,cumulative_args_t ca ATTRIBUTE_UNUSED, tree ATTRIBUTE_UNUSED);
   //原型 targetm.calls.function_incoming_arg #define TARGET_FUNCTION_INCOMING_ARG default_function_incoming_arg
   rtx (*function_incoming_arg)(TargetCalls *self,cumulative_args_t, const mtcs_function_arg_info &);
   //原型 targetm.calls.must_pass_in_stack (arg); #define TARGET_MUST_PASS_IN_STACK must_pass_in_stack_var_size_or_pad
   bool (*must_pass_in_stack)(TargetCalls *self,const mtcs_function_arg_info &arg);
   //原型 targetm.calls.fntype_abi
   mtcs_predefined_function_abi (*fntype_abi)(TargetCalls *self,const_tree type);
   //原型 targetm.calls.insn_callee_abi (insn);
   mtcs_function_abi (*insn_callee_abi)(TargetCalls *self,rtx_insn *insn);
   //原型 targetm.calls.internal_arg_pointer() #define TARGET_INTERNAL_ARG_POINTER default_internal_arg_pointer
   rtx (*internal_arg_pointer)(TargetCalls *self);
   //原型 targetm.calls.update_stack_boundary (); #define TARGET_UPDATE_STACK_BOUNDARY NULL
   void (*update_stack_boundary)(TargetCalls *self); //nvptx是空的
   //原型 targetm.calls.function_value_regno_p #define TARGET_FUNCTION_VALUE_REGNO_P nvptx_function_value_regno_p
   bool (*function_value_regno_p)(TargetCalls *self,const unsigned int regno);
   //原型 targetm.calls.get_raw_result_mode (regno) #define TARGET_GET_RAW_RESULT_MODE default_get_reg_raw_mode
   fixed_size_mode (*get_raw_result_mode)(TargetCalls *self,int regno);
   //原型 targetm.calls.get_raw_arg_mode (regno); #define TARGET_GET_RAW_ARG_MODE default_get_reg_raw_mode
   fixed_size_mode (*get_raw_arg_mode)(TargetCalls *self,int regno);
   //原型 targetm.calls.expand_builtin_saveregs () #define TARGET_EXPAND_BUILTIN_SAVEREGS default_expand_builtin_saveregs
   rtx (*expand_builtin_saveregs)(TargetCalls *self);
   //原型 targetm.calls.emit_call_builtin___clear_cache (begin, end);
   //#define TARGET_EMIT_CALL_BUILTIN___CLEAR_CACHE default_emit_call_builtin___clear_cache
   void (*emit_call_builtin___clear_cache)(TargetCalls *self,rtx begin, rtx end);
   //原型  targetm.calls.trampoline_init (m_tramp, t_func, r_chain) #define TARGET_TRAMPOLINE_INIT default_trampoline_init
   void (*trampoline_init)(TargetCalls *self,rtx ARG_UNUSED (m_tramp), tree ARG_UNUSED (t_func),rtx ARG_UNUSED (r_chain));
   //原型 targetm.calls.trampoline_adjust_address #define TARGET_TRAMPOLINE_ADJUST_ADDRESS NULL
   rtx (*trampoline_adjust_address)(TargetCalls *self,rtx addr);
   //原型 targetm.calls.empty_record_p (type) #define TARGET_EMPTY_RECORD_P hook_bool_const_tree_false
   bool (*empty_record_p)(TargetCalls *self,tree type);
   //原型 targetm.calls.get_drap_rtx #define TARGET_GET_DRAP_RTX nvptx_get_drap_rtx
   rtx (* get_drap_rtx) (TargetCalls *self);
   //原型 targetm.calls.zero_call_used_regs (selected_hardregs); #define TARGET_ZERO_CALL_USED_REGS default_zero_call_used_regs
   HardRegSet (* zero_call_used_regs)  (TargetCalls *self,HardRegSet *need_zeroed_hardregs);
   //原型 argetm.calls.call_offset_return_label #define TARGET_CALL_OFFSET_RETURN_LABEL hook_int_rtx_insn_0
   int (*call_offset_return_label) (TargetCalls *self,rtx_insn *x);

};

void  target_calls_init(TargetCalls *self);

//原型targetm.calls.struct_value_rtx   #define TARGET_STRUCT_VALUE_RTX hook_rtx_tree_int_null
rtx target_calls_struct_value_rtx (TargetCalls *self,tree fndecl,int incoming);
//原型 targetm.calls.allocate_stack_slots_for_args #define TARGET_ALLOCATE_STACK_SLOTS_FOR_ARGS hook_bool_void_true
bool target_calls_allocate_stack_slots_for_args (TargetCalls *self);
//原型 targetm.calls.promote_function_mode (NULL_TREE, mode, punsignedp, funtype,for_return);#define TARGET_PROMOTE_FUNCTION_MODE default_promote_function_mode
mtcs_mode target_calls_promote_function_mode (TargetCalls *self,const_tree type ATTRIBUTE_UNUSED, mtcs_mode mode,
                  int *punsignedp ATTRIBUTE_UNUSED,const_tree funtype ATTRIBUTE_UNUSED,int for_return ATTRIBUTE_UNUSED);
//原型 targetm.calls.return_in_memory (type, fntype) #define TARGET_RETURN_IN_MEMORY nvptx_return_in_memory
bool target_calls_return_in_memory (TargetCalls *self,const_tree type, const_tree fntype);
//原型targetm.calls.function_value (valtype, func ? func : fntype, outgoing); #define TARGET_FUNCTION_VALUE default_function_value
rtx target_calls_function_value (TargetCalls *self,const_tree ret_type ATTRIBUTE_UNUSED,
                  const_tree fn_decl_or_type, bool outgoing ATTRIBUTE_UNUSED);
//原型 targetm.calls.libcall_value (mode, fun); #define TARGET_LIBCALL_VALUE default_libcall_value
rtx target_calls_libcall_value (TargetCalls *self,machine_mode, const_rtx);
//原型 targetm.calls.function_arg (args_so_far, ptr_arg);#define TARGET_FUNCTION_ARG default_function_arg
rtx target_calls_function_arg (TargetCalls *self,cumulative_args_t arg, const mtcs_function_arg_info &info);
//原型targetm.calls.function_arg_advance (args_so_far, ptr_arg);#define TARGET_FUNCTION_ARG_ADVANCE nvptx_function_arg_advance
void target_calls_function_arg_advance (TargetCalls *self,cumulative_args_t cum_v, const mtcs_function_arg_info &info);
//原型ttargetm.calls.arg_partial_bytes (args_so_far, arg);#define TARGET_ARG_PARTIAL_BYTES hook_int_CUMULATIVE_ARGS_arg_info_0
int target_calls_arg_partial_bytes (TargetCalls *self,cumulative_args_t cum_v, const mtcs_function_arg_info &info);
//原型targetm.calls.function_arg_padding (passed_mode, type);#define TARGET_FUNCTION_ARG_PADDING default_function_arg_padding
enum pad_direction target_calls_function_arg_padding (TargetCalls *self,machine_mode mode, const_tree type);
//原型targetm.calls.function_arg_boundary (passed_mode, type);#define TARGET_FUNCTION_ARG_BOUNDARY nvptx_function_arg_boundary
unsigned target_calls_function_arg_boundary (TargetCalls *self,machine_mode mode ATTRIBUTE_UNUSED, const_tree type ATTRIBUTE_UNUSED);
//原型targetm.calls.function_arg_round_boundary (passed_mode,type);#define TARGET_FUNCTION_ARG_ROUND_BOUNDARY default_function_arg_round_boundary
unsigned target_calls_function_arg_round_boundary (TargetCalls *self,machine_mode mode ATTRIBUTE_UNUSED, const_tree type ATTRIBUTE_UNUSED);
//原型 targetm.calls.function_arg_offset (passed_mode, type);#define TARGET_FUNCTION_ARG_OFFSET default_function_arg_offset
HOST_WIDE_INT target_calls_function_arg_offset (TargetCalls *self,machine_mode mode ATTRIBUTE_UNUSED, const_tree type ATTRIBUTE_UNUSED);
//原型 targetm.calls.push_argument(unsigned int );#define TARGET_PUSH_ARGUMENT default_push_argument
bool     target_calls_push_argument (TargetCalls *self,unsigned int npush);
//原型 targetm.calls.start_call_args (args_so_far); #define TARGET_START_CALL_ARGS hook_void_CUMULATIVE_ARGS
void     target_calls_start_call_args (TargetCalls *self,cumulative_args_t args);
//原型 targetm.calls.call_args (args_so_far, argvec[i].reg, NULL_TREE); #define TARGET_CALL_ARGS hook_void_CUMULATIVE_ARGS_rtx_tree
void     target_calls_call_args (TargetCalls *self,cumulative_args_t cargs, rtx arg, tree fntype);
//原型 targetm.calls.return_pops_args  #define TARGET_RETURN_POPS_ARGS default_return_pops_args
poly_int64 target_calls_return_pops_args (TargetCalls *self,tree fundecl, tree funtype, poly_int64 size);
//原型 targetm.calls.return_in_msb (tfom) #define TARGET_RETURN_IN_MSB hook_bool_const_tree_false
bool     target_calls_return_in_msb (TargetCalls *self,const_tree valtype);
//原型 targetm.calls.end_call_args (args_so_far); #define TARGET_END_CALL_ARGS hook_void_CUMULATIVE_ARGS
void     target_calls_end_call_args (TargetCalls *self,cumulative_args_t cargs);
//原型  targetm.calls.static_chain (fndecl_or_type, false); #define TARGET_STATIC_CHAIN default_static_chain
rtx target_calls_static_chain (TargetCalls *self,const_tree ARG_UNUSED (fndecl_or_type), bool incoming_p);
//原型 (targetm.calls.split_complex_arg) #define TARGET_SPLIT_COMPLEX_ARG hook_bool_const_tree_true
bool     target_calls_split_complex_arg (TargetCalls *self,const_tree valtype);
//原型 targetm.calls.strict_argument_naming (args_so_far) #define TARGET_STRICT_ARGUMENT_NAMING nvptx_strict_argument_naming
bool     target_calls_strict_argument_naming (TargetCalls *self,cumulative_args_t cum_v);
//原型  targetm.calls.pretend_outgoing_varargs_named (args_so_far) #define TARGET_PRETEND_OUTGOING_VARARGS_NAMED default_pretend_outgoing_varargs_named
bool target_calls_pretend_outgoing_varargs_named (TargetCalls *self,cumulative_args_t ca ATTRIBUTE_UNUSED);
//原型 targetm.calls.setup_incoming_varargs #define TARGET_SETUP_INCOMING_VARARGS default_setup_incoming_varargs
void target_calls_setup_incoming_varargs (TargetCalls *self,cumulative_args_t cum_v, const mtcs_function_arg_info &arg,
                  int *pretend_size ATTRIBUTE_UNUSED, int no_rtl);
//原型 targetm.calls.callee_copies (pack_cumulative_args (ca), arg); #define TARGET_CALLEE_COPIES hook_bool_CUMULATIVE_ARGS_arg_info_false
bool target_calls_callee_copies (TargetCalls *self,cumulative_args_t ca, const mtcs_function_arg_info &arg);
//原型 targetm.calls.pass_by_reference (pack_cumulative_args (ca), arg); #define TARGET_PASS_BY_REFERENCE nvptx_pass_by_reference
bool target_calls_pass_by_reference (TargetCalls *self,cumulative_args_t, const mtcs_function_arg_info &arg);
//原型  targetm.calls.warn_parameter_passing_abi (args_so_far, type);#define TARGET_WARN_PARAMETER_PASSING_ABI hook_void_CUMULATIVE_ARGS_tree
void target_calls_warn_parameter_passing_abi (TargetCalls *self,cumulative_args_t ca ATTRIBUTE_UNUSED, tree ATTRIBUTE_UNUSED);
//原型 targetm.calls.function_incoming_arg #define TARGET_FUNCTION_INCOMING_ARG default_function_incoming_arg
rtx target_calls_function_incoming_arg (TargetCalls *self,cumulative_args_t, const mtcs_function_arg_info &);
//原型 targetm.calls.must_pass_in_stack (arg); #define TARGET_MUST_PASS_IN_STACK must_pass_in_stack_var_size_or_pad
bool target_calls_must_pass_in_stack (TargetCalls *self,const mtcs_function_arg_info &arg);
//原型 targetm.calls.fntype_abi
mtcs_predefined_function_abi target_calls_fntype_abi (TargetCalls *self,const_tree type);
//原型 targetm.calls.insn_callee_abi (insn);
mtcs_function_abi target_calls_insn_callee_abi (TargetCalls *self,rtx_insn *insn);
//原型 targetm.calls.internal_arg_pointer() #define TARGET_INTERNAL_ARG_POINTER default_internal_arg_pointer
rtx target_calls_internal_arg_pointer (TargetCalls *self);
//原型 targetm.calls.update_stack_boundary (); #define TARGET_UPDATE_STACK_BOUNDARY NULL
void target_calls_update_stack_boundary (TargetCalls *self); //nvptx是空的
//原型 targetm.calls.function_value_regno_p #define TARGET_FUNCTION_VALUE_REGNO_P nvptx_function_value_regno_p
bool target_calls_function_value_regno_p (TargetCalls *self,const unsigned int regno);
//原型 targetm.calls.get_raw_result_mode (regno) #define TARGET_GET_RAW_RESULT_MODE default_get_reg_raw_mode
fixed_size_mode target_calls_get_raw_result_mode (TargetCalls *self,int regno);
//原型 targetm.calls.get_raw_arg_mode (regno); #define TARGET_GET_RAW_ARG_MODE default_get_reg_raw_mode
fixed_size_mode target_calls_get_raw_arg_mode (TargetCalls *self,int regno);
//原型 targetm.calls.expand_builtin_saveregs () #define TARGET_EXPAND_BUILTIN_SAVEREGS default_expand_builtin_saveregs
rtx target_calls_expand_builtin_saveregs (TargetCalls *self);
//原型 targetm.calls.emit_call_builtin___clear_cache (begin, end);
//#define TARGET_EMIT_CALL_BUILTIN___CLEAR_CACHE default_emit_call_builtin___clear_cache
void target_calls_emit_call_builtin___clear_cache (TargetCalls *self,rtx begin, rtx end);
//原型  targetm.calls.trampoline_init (m_tramp, t_func, r_chain) #define TARGET_TRAMPOLINE_INIT default_trampoline_init
void target_calls_trampoline_init (TargetCalls *self,rtx ARG_UNUSED (m_tramp), tree ARG_UNUSED (t_func),rtx ARG_UNUSED (r_chain));
//原型 targetm.calls.trampoline_adjust_address #define TARGET_TRAMPOLINE_ADJUST_ADDRESS NULL
rtx target_calls_trampoline_adjust_address (TargetCalls *self,rtx addr);
//原型 targetm.calls.empty_record_p (type) #define TARGET_EMPTY_RECORD_P hook_bool_const_tree_false
bool target_calls_empty_record_p (TargetCalls *self,tree type);
//原型 targetm.calls.get_drap_rtx #define TARGET_GET_DRAP_RTX nvptx_get_drap_rtx
rtx target_calls_get_drap_rtx (TargetCalls *self);
//原型 targetm.calls.zero_call_used_regs (selected_hardregs); #define TARGET_ZERO_CALL_USED_REGS default_zero_call_used_regs
HardRegSet target_calls_zero_call_used_regs (TargetCalls *self,HardRegSet *need_zeroed_hardregs);
//原型 argetm.calls.call_offset_return_label #define TARGET_CALL_OFFSET_RETURN_LABEL hook_int_rtx_insn_0
int target_calls_call_offset_return_label (TargetCalls *self,rtx_insn *x);


void target_calls_set_custom_function_descriptors(TargetCalls *self,int value);


#endif

