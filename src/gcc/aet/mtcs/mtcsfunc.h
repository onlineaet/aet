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


#ifndef __GCC_MTCS_FUNC__
#define __GCC_MTCS_FUNC__

#include "../nlib.h"
#include "mtcsrtldata.h"
#include "mtcscomponent.h"
#include "mtcsmicro.h"
#include "mtcspass.h"

typedef struct _MtcsFuncNode MtcsFuncNode;
struct _MtcsFuncNode{
    MtcsNode parent;
    cgraph_node *node;
    /* Interprocedural passes scheduled to have their transform functions
       applied next time we execute local pass on them.  We maintain it
       per-function in order to allow IPA passes to introduce new functions.  */
    //原型 ipa_transforms_to_apply cgraph.h
    vec<MtcsPass *, va_heap, vl_ptr> ipa_transforms_to_apply;
};

struct mtcs_temp_address_hasher ;
struct mtcs_insn_cache_hasher;

typedef struct _MtcsFunc MtcsFunc;
struct _MtcsFunc
{
   MtcsComponent parent;
   /* The control flow graph for this function.  */
   int frameGrowsDownward; //原型 FRAME_GROWS_DOWNWARD
   int stackGrowsDownward; //原型 STACK_GROWS_DOWNWARD
   int argsGrowsDownward;//原型 ARGS_GROW_DOWNWARD
   //原型 #define STACK_CHECK_PROBE_INTERVAL_EXP 12 defaults.h
   int stackCheckProbeInteralExp;
   //原型 #define STACK_CHECK_MAX_FRAME_SIZE ((1 << STACK_CHECK_PROBE_INTERVAL_EXP) - UNITS_PER_WORD) default.h
   int stackCheckMaxFrameSize;
   //原型 #define EXIT_IGNORE_STACK 0 defaults.h host=1
   int exitIgnoreStack;
   //原型 #define PREFERRED_STACK_BOUNDARY STACK_BOUNDARY
   int preferredStackBoundary;
   //原型 #define INCOMING_STACK_BOUNDARY PREFERRED_STACK_BOUNDARY
   int incomingStackBoundary;
   //原型 STACK_POINTER_OFFSET #define STACK_POINTER_OFFSET    0 defaults.h
   int stackPointerOffset;

   //原型 ctrl
   MtcsRtlData *mtcsRtlData;

   //原型 #define SUPPORTS_STACK_ALIGNMENT (MAX_STACK_ALIGNMENT > STACK_BOUNDARY)
   nboolean (*is_support_stack_alignment)(MtcsFunc *self);
   //原型 MAX_SUPPORTED_STACK_ALIGNMENT
   nuint    (*get_max_support_stack_alignment)(MtcsFunc *self);
   //原型 STACK_DYNAMIC_OFFSET (FNDECL) function.cc
   //INCOMING_REG_PARM_STACK_SPACE 在平台定义依赖 REG_PARM_STACK_SPACE nvptx没有定义 所以不需要实现在INCOMING_REG_PARM_STACK_SPACE下的 STACK_DYNAMIC_OFFSET
   poly_int64 (*get_stack_dynamic_offset)(MtcsFunc *self,tree fndecl);
   //原型 #ifndef STACK_CHECK_PROTECT
   int  (*get_stack_check_protect)(MtcsFunc *self);
   //原型 #ifndef STACK_OLD_CHECK_PROTECT
   int  (*get_stack_old_check_protect)(MtcsFunc *self);
   //原型 #ifndef STACK_CHECK_MOVING_SP
   int  (*get_stack_check_moving_sp)(MtcsFunc *self);
   //原型 init_machine_status function.h
   void *(*init_machine_status)(MtcsFunc *self);
   //原型 #define SETUP_FRAME_ADDRESSES() defaults.h i386.h ix86_setup_frame_addresses ()
   void (*setup_frame_addresses)(MtcsFunc *self);
   //原型 #define FUNCTION_ARG_REGNO_P(r) 0
   bool  (*is_function_arg_regno)(MtcsFunc *self,int regno);
   //原型 FIRST_PARM_OFFSET host=0 nvptx=0
   int (*get_first_parm_offset)(MtcsFunc *self,tree fndecl);
   //原型 #define FRAME_POINTER_CFA_OFFSET(FNDECL) ((void)(FNDECL), 0)
   int (*get_frame_pointer_cfa_offset)(MtcsFunc *self,tree fndecl);
   //原型 #define ARG_POINTER_CFA_OFFSET(FNDECL)  (FIRST_PARM_OFFSET (FNDECL) + crtl->args.pretend_args_size) defaults.h
   int (*get_arg_pointer_cfg_offset)(MtcsFunc *self,tree fndecl);
   //原型 #define EPILOGUE_USES(REG) false defaults.h
   bool (*epilogue_uses)(MtcsFunc *self,nuint regno);
   //原型 #define INITIAL_ELIMINATION_OFFSET(FROM, TO, OFFSET)
   HOST_WIDE_INT (*initial_elimination_offset)(MtcsFunc *self,int from, int to);
   //原型 #define DEFAULT_INCOMING_FRAME_SP_OFFSET 各平台定义 dwarf2cfi.cc定义缺省的。
   int (*get_default_incoming_frame_sp_offset)(MtcsFunc *self);
   //原型 #define INCOMING_FRAME_SP_OFFSET 0 各平台定义 defaults.h定义缺省的
   int (*get_incoming_frame_sp_offset)(MtcsFunc *self);

   nboolean accumulate_outgoing_args;//原型ACCUMULATE_OUTGOING_ARGS
   int parm_boundary;//原型 PARM_BOUNDARY
   /* Nonzero if function being compiled can call setjmp.  */
  // unsigned int calls_setjmp : 1;
   /* Nonzero if function being compiled can call alloca,
      either as a subroutine or builtin.  */
   //unsigned int calls_alloca : 1;
   //tree nonlocal_goto_save_area;
   //原型 STACK_BOUNDARY
   int stackBoundary;
   //原型 #define FUNCTION_BOUNDARY 32
   int functionBoundary;
   //原型 STACK_PUSH_CODE default.h
   int stackPushCode;
   //原型    function_context_stack function.cc
   vec<function *> function_context_stack;
   //原型    cfun_stack function.cc
   vec<function *> cfun_stack;

   /* The currently compiled function.  */
   //原型 cfun function.h function.cc
   struct function *currentFun;
   tree current_function_decl;
   //原型 funcdef_no function.cc
   int funcdef_no;
   //原型 in_dummy_function function.cc
   bool in_dummy_function;
   //原型 virtuals_instantiated function.h function.cc
   int virtuals_instantiated;
   //MtcsFuncNode的集合，MtcsFuncNode 是 cgraph_node的同位体
   NPtrArray *funcArray;

   /* A table of addresses that represent a stack slot.  The table is a mapping
      from address RTXen to a temp slot.  */
   //原型  static GTY(()) hash_table<temp_address_hasher> *temp_slot_address_table; function.cc
   hash_table<mtcs_temp_address_hasher> *temp_slot_address_table;
   //原型 n_temp_slots_in_use function.cc
   size_t n_temp_slots_in_use;
   //原型 prologue_insn_hash funciton.cc
   GTY((cache))  hash_table<mtcs_insn_cache_hasher> *prologue_insn_hash;
   //原型 prologue_insn_hash funciton.cc
   GTY((cache))  hash_table<mtcs_insn_cache_hasher> *epilogue_insn_hash;
   //原型 currently_expanding_function_start function.h function.cc
   bool currently_expanding_function_start;
    //原型 #define RETURN_ADDR_OFFSET 0
   int returnAddrOffset;

   //原型 function.cc
    poly_int64 in_arg_offset;
    poly_int64 var_offset;
    poly_int64 dynamic_offset;
    poly_int64 out_arg_offset;
    poly_int64 cfa_offset;
};



void         mtcs_func_init(MtcsFunc *self);;
//原型 FRAME_GROWS_DOWNWARD
void         mtcs_func_set_frame_grows_downward(MtcsFunc *self,int upOrDown);
int          mtcs_func_get_frame_grows_downward(MtcsFunc *self);
//原型 STACK_GROWS_DOWNWARD
void         mtcs_func_set_stack_grows_downward(MtcsFunc *self,int upOrDown);
int          mtcs_func_get_stack_grows_downward(MtcsFunc *self);
//原型 ARGS_GROW_DOWNWARD default.h gcn.h
void         mtcs_func_set_args_grows_downward(MtcsFunc *self,int upOrDown);
int          mtcs_func_get_args_grows_downward(MtcsFunc *self);
 //原型 STACK_BOUNDARY
void         mtcs_func_set_stack_boundary(MtcsFunc *self,int stackBoundary);
int          mtcs_func_get_stack_boundary(MtcsFunc *self);
//原型 #define PREFERRED_STACK_BOUNDARY STACK_BOUNDARY
void         mtcs_func_set_preferred_stack_boundary(MtcsFunc *self,int stackBoundary);
int          mtcs_func_get_preferred_stack_boundary(MtcsFunc *self);
//原型 #define INCOMING_STACK_BOUNDARY PREFERRED_STACK_BOUNDARY
void         mtcs_func_set_incoming_stack_boundary(MtcsFunc *self,int stackBoundary);
int          mtcs_func_get_incoming_stack_boundary(MtcsFunc *self);
//原型 STACK_POINTER_OFFSET #define STACK_POINTER_OFFSET    0 defaults.h
void         mtcs_func_set_stack_pointer_offset(MtcsFunc *self,int stackPointerOffset);
int          mtcs_func_get_stack_pointer_offset(MtcsFunc *self);

MtcsRtlData *mtcs_func_get_rtl_data(MtcsFunc *self);
//原型 get_frame_size function.h function.cc
poly_int64   mtcs_func_get_frame_size(MtcsFunc *self);
void         mtcs_func_init_emit(MtcsFunc *self);
//原型 rtl_profile_for_bb predict.h predict.cc
void        mtcs_func_rtl_profile_for_bb (MtcsFunc *self,basic_block bb);
//原型 SUPPORTS_STACK_ALIGNMENT default.h
nboolean    mtcs_func_is_support_stack_alignment(MtcsFunc *self);
//原型 MAX_SUPPORTED_STACK_ALIGNMENT
nuint       mtcs_func_get_max_support_stack_alignment(MtcsFunc *self);
//原型 function.cc use_register_for_decl
bool        mtcs_func_use_register_for_decl (MtcsFunc *self,const_tree decl);
//原型 function.h function.cc
void mtcs_func_update_temp_slot_address (MtcsFunc *self,rtx old_rtx, rtx new_rtx);
//原型 aggregate_value_p function.h function.cc
bool mtcs_func_aggregate_value_p (MtcsFunc *self,const_tree exp, const_tree fntype);
//原型 hard_function_value explow.h explow.cc
rtx mtcs_func_hard_function_value (MtcsFunc *self,const_tree valtype, const_tree func, const_tree fntype,
             int outgoing ATTRIBUTE_UNUSED);

//原型 ACCUMULATE_OUTGOING_ARGS host=0 nvptx=1
nboolean    mtcs_func_is_accumulate_outgoing_args (MtcsFunc *self);
//原型 ACCUMULATE_OUTGOING_ARGS host=0 nvptx=1
void    mtcs_func_set_accumulate_outgoing_args (MtcsFunc *self,nboolean is);
//原型 OUTGOING_REG_PARM_STACK_SPACE
//i386 #define OUTGOING_REG_PARM_STACK_SPACE(FNTYPE) \
//(TARGET_64BIT && ix86_function_type_abi (FNTYPE) == MS_ABI)
//缺省 default.h #define OUTGOING_REG_PARM_STACK_SPACE(FNTYPE)=0
nboolean    mtcs_func_is_outgoint_reg_parm_stack_space (MtcsFunc *self,tree type);
//原型 PARM_BOUNDARY host=BITS_PER_WORD nvptx=32
void    mtcs_func_set_parm_boundary(MtcsFunc *self,int parm_boundary);
//原型 PARM_BOUNDARY host=BITS_PER_WORD nvptx=32
int     mtcs_func_get_parm_boundary(MtcsFunc *self);
//原型 #define FUNCTION_BOUNDARY 32
void    mtcs_func_set_function_boundary(MtcsFunc *self,int boundary);
int     mtcs_func_get_function_boundary(MtcsFunc *self);

//原型 locate_and_pad_parm function.h function.cc
void mtcs_func_locate_and_pad_parm (MtcsFunc *self,machine_mode passed_mode, tree type, int in_regs,int reg_parm_stack_space,
        int partial,tree fndecl ATTRIBUTE_UNUSED,struct args_size *initial_offset_ptr,struct locate_and_pad_arg_data *locate);
//原型 init_function_start function.h  function.c
void mtcs_func_init_function_start (MtcsFunc *self,tree subr);
//原型 default_rtl_profile predict.h predict.cc
void mtcs_func_default_rtl_profile (MtcsFunc *self);
//原型 assign_stack_temp function.h function.cc
rtx mtcs_func_assign_stack_temp (MtcsFunc *self,machine_mode mode, poly_int64 size);
//原型 assign_stack_temp_for_type function.h function.cc
rtx mtcs_func_assign_stack_temp_for_type (MtcsFunc *self,machine_mode mode, poly_int64 size, tree type);
//原型 assign_stack_local_1 function.h function.cc
rtx mtcs_func_assign_stack_local_1 (MtcsFunc *self,machine_mode mode, poly_int64 size,int align, int kind);
//原型 assign_stack_local function.h function.cc
rtx mtcs_func_assign_stack_local(MtcsFunc *self,machine_mode mode, poly_int64 size, int align);
//原型 frame_offset_overflow function.h function.cc
bool mtcs_func_frame_offset_overflow(MtcsFunc *self,poly_int64 offset, tree func);
//原型 assign_temp function.h functin.cc
rtx mtcs_func_assign_temp (MtcsFunc *self,tree type_or_decl, int memory_required,int dont_promote ATTRIBUTE_UNUSED);
//原型 record_final_call function.h function.cc
void mtcs_func_record_final_call (MtcsFunc *self,tree callee, location_t location);
//原型 push_temp_slots function.h function.cc
void mtcs_func_push_temp_slots (MtcsFunc *self);
//原型 free_temp_slots function.h function.cc
void mtcs_func_free_temp_slots (MtcsFunc *self);
//原型 pop_temp_slots function.h function.cc
void mtcs_func_pop_temp_slots (MtcsFunc *self);
//原型 STACK_DYNAMIC_OFFSET (FNDECL) function.cc
//INCOMING_REG_PARM_STACK_SPACE 在平台定义依赖 REG_PARM_STACK_SPACE nvptx没有定义 所以不需要实现在INCOMING_REG_PARM_STACK_SPACE下的 STACK_DYNAMIC_OFFSET
poly_int64 mtcs_func_get_stack_dynamic_offset(MtcsFunc *self, tree fndecl);

//原型 #ifndef STACK_CHECK_PROTECT
int  mtcs_func_get_stack_check_protect(MtcsFunc *self);
//原型 #ifndef STACK_OLD_CHECK_PROTECT
int  mtcs_func_get_stack_old_check_protect(MtcsFunc *self);
//原型 #ifndef STACK_CHECK_MOVING_SP
int  mtcs_func_get_stack_check_moving_sp(MtcsFunc *self);
//原型 #define DEFAULT_INCOMING_FRAME_SP_OFFSET 各平台定义 dwarf2cfi.cc定义缺省的。
int  mtcs_func_get_default_incoming_frame_sp_offset(MtcsFunc *self);
//原型 #define INCOMING_FRAME_SP_OFFSET 0 各平台定义 defaults.h定义缺省的
int  mtcs_func_get_incoming_frame_sp_offset(MtcsFunc *self);

//原型 stack_protect_epilogue function.h function.cc
void mtcs_func_stack_protect_epilogue (MtcsFunc *self);
//原型 preserve_temp_slots function.h function.cc
void mtcs_func_preserve_temp_slots (MtcsFunc *self,rtx x);
//原型 STACK_PUSH_CODE default.h
int mtcs_func_get_stack_push_code(MtcsFunc *self);
void mtcs_func_set_stack_push_code(MtcsFunc *self,int value);

//原型 #define STACK_CHECK_PROBE_INTERVAL_EXP 12 defaults.h
int mtcs_func_get_stack_check_probe_interval_exp(MtcsFunc *self);
void mtcs_func_set_stack_check_probe_interval_exp(MtcsFunc *self,int value);
//原型 #define STACK_CHECK_MAX_FRAME_SIZE ((1 << STACK_CHECK_PROBE_INTERVAL_EXP) - UNITS_PER_WORD) default.h
int mtcs_func_get_stack_check_max_frame_size(MtcsFunc *self);

//原型 push_function_context funciton.h function.cc
void mtcs_func_push_function_context (MtcsFunc *self);
//原型 pop_function_context function.h function.cc
void mtcs_func_pop_function_context (MtcsFunc *self);
//原型 allocate_struct_function function.h function.cc
void mtcs_func_allocate_struct_function (MtcsFunc *self,tree fndecl, bool abstract_p);
//原型 set_cfun function.h funciton.cc
void mtcs_func_set_cfun (MtcsFunc *self,struct function *new_cfun, bool force=false);
//原型 pop_cfun function.h function.cc
void mtcs_func_pop_cfun (MtcsFunc *self);
//原型 push_cfun function.h function.cc
void mtcs_func_push_cfun (MtcsFunc *self,struct function *new_cfun);
void mtcs_func_set_current_function_decl(MtcsFunc *self,tree t);
//原型 push_struct_function function.h function.cc
void mtcs_func_push_struct_function (MtcsFunc *self,tree fndecl, bool abstract_p=false);
//原型 push_struct_function function.h function.cc
void mtcs_func_push_struct_function_no_create(MtcsFunc *self,tree fndecl, bool abstract_p);
//从mtccompile收集的函数加入到funcArray中
void mtcs_func_add_mtcs_node(MtcsFunc *self,MtcsFuncNode *node);
//通过struct cgraph_node *node 获到同位体 MtcsFuncNode;
MtcsFuncNode *mtcs_func_get_node(MtcsFunc *self,struct cgraph_node *node);
//原型 push_dummy_function function.h function.cc
void mtcs_func_push_dummy_function (MtcsFunc *self,bool with_decl);
//原型 init_dummy_function_start function.h function.cc
void mtcs_func_init_dummy_function_start (MtcsFunc *self);
//原型 init_temp_slots function.h function.cc
void mtcs_func_init_temp_slots (MtcsFunc *self);
//原型 expand_dummy_function_end function.h function.cc
void mtcs_func_expand_dummy_function_end (MtcsFunc *self);
//原型 free_after_compilation function.h function.cc
void mtcs_func_free_after_compilation (MtcsFunc *self,struct function *f);
//原型 pop_dummy_function function.h function.cc
void mtcs_func_pop_dummy_function (MtcsFunc *self);
//原型 expand_function_start function.h function.cc
void mtcs_func_expand_function_start (MtcsFunc *self,tree subr);
//原型 expand_function_end function.h function.cc
void mtcs_func_expand_function_end (MtcsFunc *self);
//原型 get_arg_pointer_save_area function.h function.cc
rtx mtcs_func_get_arg_pointer_save_area (MtcsFunc *self);
//原型 clobber_return_register function.h funciton.cc
void mtcs_func_clobber_return_register (MtcsFunc *self);
//原型 diddle_return_value function.h function.cc
void  mtcs_func_diddle_return_value (MtcsFunc *self,void (*doit) (rtx, void *), void *arg);
//原型 #define EXIT_IGNORE_STACK 0 defaults.h host=1
void mtcs_func_set_exit_ignore_stack(MtcsFunc *self,int value);
int  mtcs_func_get_exit_ignore_stack(MtcsFunc *self);
//原型 emit_initial_value_sets function.h function.cc
void mtcs_func_emit_initial_value_sets (MtcsFunc *self);
//原型 #define SETUP_FRAME_ADDRESSES() defaults.h i386.h ix86_setup_frame_addresses ()
void mtcs_func_setup_frame_addresses(MtcsFunc *self);
//原型 #define RETURN_ADDR_OFFSET 0
int  mtcs_func_get_return_addr_offset(MtcsFunc *self);
void mtcs_func_set_return_addr_offset(MtcsFunc *self,int value);
//原型 #define FUNCTION_ARG_REGNO_P(r) 0
bool mtcs_func_is_function_arg_regno(MtcsFunc *self,int regno);
//原型 max_reg_num rtl.h emit-rtl.cc
int mtcs_func_max_reg_num(MtcsFunc *self);
//原型 FIRST_PARM_OFFSET host=0 nvptx=0
int mtcs_func_get_first_parm_offset(MtcsFunc *self,tree fndecl);
//原型 instantiate_decl_rtl function.h function.cc
void mtcs_func_instantiate_decl_rtl (MtcsFunc *self,rtx x);
//原型 instantiate_virtual_regs function.cc instantiate_virtual_regs是rtl pass vregs的excute方法 改为公共，
//MtcsPassInstantiateVirtualRegs 可以调用
void mtcs_func_instantiate_virtual_regs (MtcsFunc *self);
//原型 #define EPILOGUE_USES(REG) false defaults.h
bool mtcs_func_epilogue_uses(MtcsFunc *self,nuint regno);
//原型 #define INITIAL_ELIMINATION_OFFSET(FROM, TO, OFFSET)
HOST_WIDE_INT mtcs_func_initial_elimination_offset(MtcsFunc *self,int from, int to);
//原型 maybe_copy_prologue_epilogue_insn function.h funciton.cc
void mtcs_func_maybe_copy_prologue_epilogue_insn (MtcsFunc *self,rtx insn, rtx copy);
//原型 get_max_insn_count rtl.h emit-rtl.cc
int mtcs_func_get_max_insn_count (MtcsFunc *self);
//原型 initial_value_entry function.h function.cc
bool mtcs_func_initial_value_entry (MtcsFunc *self,int i, rtx *hreg, rtx *preg);
//原型 thread_prologue_and_epilogue_insns function.h function.cc
void mtcs_func_thread_prologue_and_epilogue_insns (MtcsFunc *self);
//原型 ADD_PARM_SIZE function.h
void mtcs_func_add_parm_size(MtcsFunc *self,struct args_size *to,tree var);
//原型 SUB_PARM_SIZE function.h 原型传的是变量不是指针
void mtcs_func_sub_parm_size(MtcsFunc *self,struct args_size *to,tree var);
//原型 #define ARGS_SIZE_TREE(SIZE) function.h
tree mtcs_func_args_size_tree(MtcsFunc *self,struct args_size size);
//原型 #define ARGS_SIZE_RTX(SIZE) function.h
rtx mtcs_func_args_size_rtx(MtcsFunc *self,struct args_size size);


/******-------------------下面代码是基于mtcsfunc的rtl pass--------------------************************/
//原型 NEXT_PASS (pass_instantiate_virtual_regs, 1);    RTL_PASS   function.cc   vregs   y  无条件执行 instantiate_virtual_regs
typedef struct _MtcsPassInstantiateVirtualRegs MtcsPassInstantiateVirtualRegs;
struct _MtcsPassInstantiateVirtualRegs
{
   MtcsPass parent;
};
MtcsPassInstantiateVirtualRegs *mtcs_pass_instantiate_virtual_regs_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_match_asm_constraints, 1);  RTL_PASS  function.cc  asmcons   y  无条件执行 ...match_asm_constraints_1..
typedef struct _MtcsPassMatchAsmConstraints MtcsPassMatchAsmConstraints;
struct _MtcsPassMatchAsmConstraints
{
   MtcsPass parent;
};
MtcsPassMatchAsmConstraints *mtcs_pass_match_asm_constraints_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_late_thread_prologue_and_epilogue, 1);  RTL_PASS  function.cc  late_pro_and_epilogue   y 有条件执行 targetm.use_late_prologue_epilogue
typedef struct _MtcsPassLateThreadPrologueAndEpilogue MtcsPassLateThreadPrologueAndEpilogue;
struct _MtcsPassLateThreadPrologueAndEpilogue
{
   MtcsPass parent;
};
MtcsPassLateThreadPrologueAndEpilogue *mtcs_pass_late_thread_prologue_and_epilogue_new (MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_zero_call_used_regs, 1);  RTL_PASS  function.cc  zero_call_used_regs   y 无条件执行 tree attr_zero_regs
typedef struct _MtcsPassZeroCallUsedRegs MtcsPassZeroCallUsedRegs;
struct _MtcsPassZeroCallUsedRegs
{
   MtcsPass parent;
};
MtcsPassZeroCallUsedRegs *mtcs_pass_zero_call_use_regs_new (MtcsMode *mtcsMode);

#endif
