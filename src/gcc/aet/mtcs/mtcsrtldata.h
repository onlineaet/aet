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

#ifndef __GCC_MTCS_RTL_DATA__
#define __GCC_MTCS_RTL_DATA__

#include "../nlib.h"
#include "mtcsfuncabi.h"
//#include "emit-rtl.h"

//介绍machine_mode
//https://blog.csdn.net/wuhui_gdnt/article/details/5319053

class mtcs_temp_slot;
typedef class mtcs_temp_slot *mtcs_temp_slot_p;


/* Information mainlined about RTL representation of incoming arguments.  */
//原型 incoming_args emit-rtl.h
struct GTY(()) mtcs_incoming_args {
  /* Number of bytes of args popped by function being compiled on its return.
     Zero if no bytes are to be popped.
     May affect compilation of return insn or of function epilogue.  */
  poly_int64 pops_args;

  /* If function's args have a fixed size, this is that size, in bytes.
     Otherwise, it is -1.
     May affect compilation of return insn or of function epilogue.  */
  poly_int64 size;

  /* # bytes the prologue should push and pretend that the caller pushed them.
     The prologue must do this, but only if parms can be passed in
     registers.  */
  int pretend_args_size;

  /* This is the offset from the arg pointer to the place where the first
     anonymous arg can be found, if there is one.  */
  rtx arg_offset_rtx;

  /* Quantities of various kinds of registers
     used for the current function's args.  */
  void *info /*!CUMULATIVE_ARGS info*/;

  /* The arg pointer hard register, or the pseudo into which it was copied.  */
  rtx internal_arg_pointer;
};

namespace mtcs_rtl_ssa { class function_info; }

//原型 struct GTY(()) rtl_data  emit-rtl.h
typedef struct _MtcsRtlData MtcsRtlData;
struct _MtcsRtlData
{
    //void init_stack_alignment (); 被mtcs_rtl_data_init_stack_alignment替换

     struct expr_status expr;
     struct emit_status emit;
     struct varasm_status varasm;
     struct mtcs_incoming_args/*!incoming_args*/ args;
     struct function_subsections subsections;
     struct rtl_eh eh;

    /* Offset to end of allocated area of stack frame.
        If stack grows down, this is the address of the last stack slot allocated.
        If stack grows up, this is the address for the next slot.  */
     poly_int64 x_frame_offset;
     mtcs_rtl_ssa::function_info *GTY((skip)) ssa;

    /* The largest alignment needed on the stack, including requirement
       for outgoing stack alignment.  */
    //来自emit-rtl.cc rtl_data::init_stack_alignment ()
    unsigned int stack_alignment_needed;
    /* The largest alignment of slot allocated on the stack.  */
    unsigned int max_used_stack_slot_alignment;
    /* The stack alignment estimated before reload, with consideration of
       following factors:
       1. Alignment of local stack variables (max_used_stack_slot_alignment)
       2. Alignment requirement to call other functions
          (preferred_stack_boundary)
       3. Alignment of non-local stack variables but might be spilled in
          local stack.  */
    unsigned int stack_alignment_estimated;

    /* Preferred alignment of the end of stack frame, which is preferred
       to call other functions.  */
    unsigned int preferred_stack_boundary;

    /* When set, expand should optimize for speed.  */
    bool maybe_hot_insn_p;

    /* Nonzero if function stack realignment estimation is done, namely
       stack_realign_needed flag has been set before reload wrt estimated
       stack alignment info.  */
    bool stack_realign_processed;
    //原型 regno_reg_rtx function.h emit-rtl.cc
    rtx * regno_reg_rtx; //长度定义在emit中的x_reg_rtx_no

    /* List of all used temporaries allocated, by level.  */
    //原型 x_used_temp_slots emit-rtl.h #define used_temp_slots (crtl->x_used_temp_slots)
    vec<mtcs_temp_slot_p, va_gc> *x_used_temp_slots;

    /* List of available temp slots.  */
    class mtcs_temp_slot *x_avail_temp_slots;

    /* Current nesting level for temporaries.  */
    int x_temp_slot_level;

    /* Nonzero if the current function uses the constant pool.  */
    bool uses_const_pool;
    /* # of bytes of outgoing arguments.  If ACCUMULATE_OUTGOING_ARGS is
       defined, the needed space is pushed by the prologue.  */
    poly_int64 outgoing_args_size;

    /* Nonzero if function stack realignment has been finalized, namely
       stack_realign_needed flag has been set and finalized after reload.  */
    bool stack_realign_finalized;
    /* Nonzero if function stack realignment is needed.  This flag may be
       set twice: before and after reload.  It is set before reload wrt
       stack alignment estimation before reload.  It will be changed after
       reload if by then criteria of stack realignment is different.
       The value set after reload is the accurate one and is finalized.  */
    bool stack_realign_needed;

    /* A variable living at the top of the frame that holds a known value.
       Used for detecting stack clobbers.  */
    tree stack_protect_guard;

    /* The __stack_chk_guard variable or expression holding the stack
       protector canary value.  */
    tree stack_protect_guard_decl;

    /* List (chain of EXPR_LISTs) of all stack slots in this function.
       Made for the sake of unshare_all_rtl.  */
    vec<rtx, va_gc> *x_stack_slot_list;

    /* List of empty areas in the stack frame.  */
    class frame_space *frame_space_list;

    /* Nonzero if function being compiled needs dynamic realigned
       argument pointer (drap) if stack needs realigning.  */
    bool need_drap;

    /* Nonzero if stack limit checking should be enabled in the current
       function.  */
    bool limit_stack;

    /* Set when the tail call has been produced.  */
    bool tail_call_emit;
    /* Nonzero if current function must be given a frame pointer.
       Set in reload1.cc or lra-eliminations.cc if anything is allocated
       on the stack there.  */
    //frame_pointer_needed //emit-rtl.h定义，
    bool x_frame_pointer_needed;

    /* Like regs_ever_live, but 1 if a reg is set or clobbered from an
       asm.  Unlike regs_ever_live, elements of this array corresponding
       to eliminable regs (like the frame pointer) are set if an asm
       sets them.  */
    HardRegSet asm_clobbers;
    /* The ABI of the function, i.e. the interface it presents to its callers.
       This is the ABI that should be queried to see which registers the
       function needs to save before it uses them.

       Other functions (including those called by this function) might use
       different ABIs.  */
    const mtcs_predefined_function_abi *GTY((skip)) abi;

    int stackBoundary;//宏STACK_BOUNDARY的值，由mtcsfunc给mtcsrtldata设定

    /* Nonzero if profiling code should be generated.  */
    //原型 profile emit-rtl.h struct GTY(()) rtl_data
    bool profile;

    /* Label that will go on function epilogue.
       Jumping to this label serves as a "return" instruction
       on machines which require execution of the epilogue on all returns.  */
    //原型 return_label emit-rtl.h rtl_data
    rtx_code_label *x_return_label;

    /* The minimum alignment of parameter stack.  */
    //原型 parm_stack_boundary emit-rtl.h rtl_data
    unsigned int parm_stack_boundary;
    /* If nonzero, an RTL expression for the location at which the current
       function returns its result.  If the current function returns its
       result in a register, current_function_return_rtx will always be
       the hard register containing the result.  */
    //原型 return_rtx emit-rtl.h rtl_data
    rtx return_rtx;
    /* Dynamic Realign Argument Pointer used for realigning stack.  */
    //原型 drap_reg emit-rtl.h rtl_data
    rtx drap_reg;
    /* Nonzero if function being compiled has nonlocal gotos to parent
       function.  */
    //原型 has_nonlocal_goto emit-rtl.h rtl_data
    bool has_nonlocal_goto;
    /* Location at which to save the argument pointer if it will need to be
       referenced.  There are two cases where this is done: if nonlocal gotos
       exist, or if vars stored at an offset from the argument pointer will be
       needed by inner routines.  */
    //原型 x_arg_pointer_save_area emit-rtl.h rtl_data
    rtx x_arg_pointer_save_area;
    /* Nonzero if code to initialize arg_pointer_save_area has been emitted.  */
    //原型 arg_pointer_save_area_init emit-rtl.h rtl_data
    bool arg_pointer_save_area_init;
    /* Label that will go on the end of function epilogue.
       Jumping to this label serves as a "naked return" instruction
       on machines which require execution of the epilogue on all returns.  */
    //原型 x_naked_return_label emit-rtl.h rtl_data #define naked_return_label (crtl->x_naked_return_label)
    rtx_code_label *x_naked_return_label;
    /* Nonzero if the current function needs an lsda for exception handling.  */
    bool uses_eh_lsda;
    /* Nonzero if the function being compiled has undergone hot/cold partitioning
       (under flag_reorder_blocks_and_partition) and has at least one cold
       block.  */
    bool has_bb_partition;
    /* Nonzero if the function being compiled has completed the bb reordering
       pass.  */
    bool bb_reorder_complete;
    /* How many NOP insns to place at each function entry by default.  */
    unsigned short patch_area_size;

    /* How far the real asm entry point is into this area.  */
    unsigned short patch_area_entry;
    /* Vector of initial-value pairs.  Each pair consists of a pseudo
       register of approprite mode that stores the initial value a hard
       register REGNO, and that hard register itself.  */
    /* ??? This could be a VEC but there is currently no way to define an
       opaque VEC type.  */
    struct initial_value_struct *hard_reg_initial_vals;
    /* Nonzero if function being compiled has an asm statement.  */
    bool has_asm_statement;
    /* True if the stack pointer is clobbered by asm statement.  */
    bool sp_is_clobbered_by_asm;
    /* This bit is used by the exception handling logic.  It is set if all
       calls (if any) are sibling calls.  Such functions do not have to
       have EH tables generated, as they cannot throw.  A call to such a
       function, however, should be treated as throwing if any of its callees
       can throw.  */
    bool all_throwers_are_sibcalls;

    /* Nonzero if the function calls __builtin_eh_return.  */
    bool calls_eh_return;
    /* Nonzero if function saves all registers, e.g. if it has a nonlocal
       label that can reach the exit block via non-exceptional paths. */
    bool saves_all_registers;
    /* Nonzero if function being compiled called builtin_return_addr or
       builtin_frame_address with nonzero count.  */
    bool accesses_prior_frames;
    /* Nonzero if function stack realignment is tried.  This flag is set
       only once before reload.  It affects register elimination.  This
       is used to generate DWARF debug info for stack variables.  */
    bool stack_realign_tried;
    /* Nonzero if function being compiled doesn't contain any calls
        (ignoring the prologue and epilogue).  This is set prior to
        register allocation in IRA and is valid for the remaining
        compiler passes.  */
     bool is_leaf;
     /* Nonzero if the function being compiled is a leaf function which only
        uses leaf registers.  This is valid after reload (specifically after
        sched2) and is useful only if the port defines LEAF_REGISTERS.  */
     bool uses_only_leaf_regs;
     /* True if current function cannot throw.  Unlike
        TREE_NOTHROW (current_function_decl) it is set even for overwritable
        function where currently compiled version of it is nothrow.  */
     bool nothrow;
     /* True if dbr_schedule has already been called for this function.  */
     bool dbr_scheduled_p;
     /* True if we performed shrink-wrapping for the current function.  */
     bool shrink_wrapped;
     /* Nonzero if function being compiled doesn't modify the stack pointer
        (ignoring the prologue and epilogue).  This is only valid after
        pass_stack_ptr_mod has run.  */
     bool sp_is_unchanging;
     /* All hard registers that need to be zeroed at the return of the routine.  */
     HardRegSet must_be_zero_on_return;

     /* True if we performed shrink-wrapping for separate components for
        the current function.  */
     bool shrink_wrapped_separate;
     /* List (chain of INSN_LIST) of labels heading the current handlers for
        nonlocal gotos.  */
     rtx_insn_list *x_nonlocal_goto_handler_labels;
     /* The highest address seen during shorten_branches.  */
     int max_insn_address;

};

MtcsRtlData     *mtcs_rtl_data_new();
//宏STACK_BOUNDARY的值，由mtcsfunc给mtcsrtldata设定
void  mtcs_rtl_data_set_stack_boundary(MtcsRtlData *self,int value);
nuint  mtcs_rtl_data_get_stack_alignment_needed(MtcsRtlData *self);
//原型 get_last_insn emit-rtl.h
rtx_insn *mtcs_rtl_data_get_last_insn (MtcsRtlData *self);
//原型 get_insns emit-rtl.h
rtx_insn *mtcs_rtl_data_get_insns (MtcsRtlData *self);
//原型 set_first_insn emit-rtl.h 内联函数
void mtcs_rtl_data_set_first_insn (MtcsRtlData *self,rtx_insn *insn);
//原型 set_last_insn emit-rtl.h 内联函数
void mtcs_rtl_data_set_last_insn (MtcsRtlData *self,rtx_insn *insn);
//原型 struct GTY(()) rtl_data {void init_stack_alignment ();... emit-rtl.h emit-rtl.cc
void mtcs_rtl_data_init_stack_alignment (MtcsRtlData *self);
//原型 get_current_sequence emit-rtl.h 内联函数
struct sequence_stack *mtcs_rtl_data_get_current_sequence(MtcsRtlData *self);
void mtcs_rtl_data_ensure_regno_capacity (MtcsRtlData *self);
//原型 delete_insns_since rtl.h emit-rtl.cc
void mtcs_rtl_data_delete_insns_since (MtcsRtlData *self,rtx_insn *from);
//原型 PSEUDO_REGNO_MODE regs.h
machine_mode mtcs_rtl_data_get_pseudo_regno_mode(MtcsRtlData *self,int i);
 //原型 last_call_insn rtl.h emit-rtl.cc
rtx_call_insn *mtcs_rtl_data_last_call_insn (MtcsRtlData *self);
 //原型 REGNO_POINTER_ALIGN #define REGNO_POINTER_ALIGN(REGNO) (crtl->emit.regno_pointer_align[REGNO]) function.h
unsigned mtcs_rtl_data_get_regno_pointer_align(MtcsRtlData *self,int regnum);
void    mtcs_rtl_data_set_regno_pointer_align(MtcsRtlData *self,int regnum,int align);
//原型 int get_first_label_num (void) rtl.h emit-rtl.cc 在emit-rtl.cc定义宏#define first_label_num (crtl->emit.x_first_label_num)
//访问emit.x_first_label_num;
int mtcs_rtl_data_get_first_label_num (MtcsRtlData *self);
//原型 in_sequence_p rtl.h emit-rtl.cc
bool mtcs_rtl_data_in_sequence_p (MtcsRtlData *self);
//原型 free_after_compilation 中的  memset (crtl, 0, sizeof (struct rtl_data));
void mtcs_rtl_data_reset(MtcsRtlData *self);
//原型 get_max_uid emit-rtl.h
inline int mtcs_rtl_data_get_max_uid (MtcsRtlData *self)
{
   return self->emit.x_cur_insn_uid;
}

//原型 maybe_set_first_label_num rtl.h emit-rtl.cc
void mtcs_rtl_data_maybe_set_first_label_num (MtcsRtlData *self,rtx_code_label *x);

#endif

