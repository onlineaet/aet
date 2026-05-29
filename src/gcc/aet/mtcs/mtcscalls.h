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

#ifndef __GCC_MTCS_CALLS__
#define __GCC_MTCS_CALLS__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcsargs.h"
#include "mtcsmicro.h"

typedef struct _MtcsCalls MtcsCalls;
struct _MtcsCalls
{
    MtcsComponent parent;
    /* A vector of one char per byte of stack space.  A byte if nonzero if
       the corresponding stack location has been used.
       This vector is used to prevent a function call within an argument from
       clobbering any stack already set up.  */
     char *stack_usage_map;

    /* Size of STACK_USAGE_MAP.  */
     unsigned int highest_outgoing_arg_in_use;

    /* Assume that any stack location at this byte index is used,
       without checking the contents of stack_usage_map.  */
     unsigned HOST_WIDE_INT stack_usage_watermark = HOST_WIDE_INT_M1U;

    /* A bitmap of virtual-incoming stack space.  Bit is set if the corresponding
       stack location's tail call argument has been already stored into the stack.
       This bitmap is used to prevent sibling call optimization if function tries
       to use parent's incoming argument slots when they have been already
       overwritten with tail call arguments.  */
     sbitmap stored_args_map;

    /* Assume that any virtual-incoming location at this byte index has been
       stored, without checking the contents of stored_args_map.  */
     unsigned HOST_WIDE_INT stored_args_watermark;

    /* stack_arg_under_construction is nonzero when an argument may be
       initialized with a constructor call (including a C function that
       returns a BLKmode struct) and expand_call must take special action
       to make sure the object being constructed does not overlap the
       argument list for the constructor call.  */
     int stack_arg_under_construction;

     /* Internal state for internal_arg_pointer_based_exp and its helpers.  */
     struct{
       /* Last insn that has been scanned by internal_arg_pointer_based_exp_scan,
          or NULL_RTX if none has been scanned yet.  */
       rtx_insn *scan_start;
       /* Vector indexed by REGNO - FIRST_PSEUDO_REGISTER, recording if a pseudo is
          based on crtl->args.internal_arg_pointer.  The element is NULL_RTX if the
          pseudo isn't based on it, a CONST_INT offset if the pseudo is based on it
          with fixed offset, or PC if this is with variable or unknown offset.  */
       vec<rtx> cache;
     } internal_arg_pointer_exp_state;
};


class mtcs_function_arg_info
{
public:

    mtcs_function_arg_info ()
    {
        this->mtcsMode=NULL;
        type=NULL_TREE;
        mode=VOIDmode;
        named=false;
        pass_by_reference=false;
    }


    mtcs_function_arg_info (MtcsMode *mtcsMode)
  {
        this->mtcsMode=mtcsMode;
        type=NULL_TREE;
        mode=VOIDmode;
        named=false;
        pass_by_reference=false;
  }

  /* Initialize an argument of mode MODE, either before or after promotion.  */
    mtcs_function_arg_info (MtcsMode *mtcsMode,machine_mode mode, bool named)
  {
        this->mtcsMode=mtcsMode;
          type=NULL_TREE;
          this->mode=mode;
          this->named=named;
          pass_by_reference=false;

  }

  /* Initialize an unpromoted argument of type TYPE.  */
    mtcs_function_arg_info (MtcsMode *mtcsMode,tree type, bool named)
  {
        this->mtcsMode=mtcsMode;
        this->type=type;
          mode=TYPE_MODE (type);
          this->named=named;
          pass_by_reference=false;
  }

  /* Initialize an argument with explicit properties.  */
    mtcs_function_arg_info (MtcsMode *mtcsMode,tree type, machine_mode mode, bool named){
           this->mtcsMode=mtcsMode;
           this->type=type;
           this->mode=mode;
           this->named=named;
           pass_by_reference=false;
    }
    poly_int64 type_size_in_bytes () const {
       if (type)
         return int_size_in_bytes (type);
       return mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,mode);
   }

    poly_int64 promoted_size_in_bytes () const {
      if (mode == mtcsMode->modes.M_BLKmode)
        return int_size_in_bytes (type);
      return mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);
    }
    /* True if the argument represents the end of the argument list,
       as returned by end_marker ().  */
    bool end_marker_p () const { return mode == VOIDmode; }

    /* Return a function_arg_info that represents the end of the
       argument list.  */
    static mtcs_function_arg_info end_marker (MtcsMode *mtcsMode){
      return mtcs_function_arg_info (mtcsMode,void_type_node, /*named=*/true);
    }

    MtcsMode *mtcsMode;
    /* The type of the argument, or null if not known (which is true for
       libgcc support functions).  */
    tree type;
    /* The mode of the argument.  Depending on context, this might be
       the mode of the argument type or the mode after promotion.  */
    machine_mode mode;
    /* True if the argument is treated as a named argument, false if it is
       treated as an unnamed variadic argument (i.e. one passed through
       "...").  See also TARGET_STRICT_ARGUMENT_NAMING.  */
    unsigned int named : 1;
    /* True if we have decided to pass the argument by reference, in which case
       the function_arg_info describes a pointer to the original argument.  */
    unsigned int pass_by_reference : 1;
};



MtcsCalls *mtcs_calls_new(MtcsMode *mtcsMode);
//原型 expand_call calls.h calls.cc
rtx mtcs_calls_expand_call (MtcsCalls *self,tree exp, rtx target, int ignore);
//原型 prepare_call_address calls.h calls.cc
rtx mtcs_calls_prepare_call_address (MtcsCalls *self,tree fndecl_or_type, rtx funexp, rtx static_chain_value,
              rtx *call_fusage, int reg_parm_seen, int flags);
//原型 shift_return_value calls.h calls.cc
bool mtcs_calls_shift_return_value(MtcsCalls *self,machine_mode mode, bool left_p, rtx value);
//原型 reference_callee_copied calls.h calls.cc
bool mtcs_calls_reference_callee_copied (MtcsCalls *self,MtcsCumulativeArgs/*!CUMULATIVE_ARGS*/ *ca, const mtcs_function_arg_info &arg);
//原型 emit_library_call_value_1 rtl.h calls.cc
rtx mtcs_calls_emit_library_call_value_1 (MtcsCalls *self,int retval, rtx orgfun, rtx value,
               enum libcall_type fn_type, machine_mode outmode, int nargs, mtcs_rtx_mode_t *args);

/* Output a library call and discard the returned value.  FUN is the
   address of the function, as a SYMBOL_REF rtx, and OUTMODE is the mode
   of the (discarded) return value.  FN_TYPE is LCT_NORMAL for `normal'
   calls, LCT_CONST for `const' calls, LCT_PURE for `pure' calls, or
   another LCT_ value for other types of library calls.

   There are different overloads of this function for different numbers
   of arguments.  In each case the argument value is followed by its mode.  */
//原型 emit_library_call rtl.h
inline void mtcs_calls_emit_library_call (MtcsCalls *self,rtx fun, libcall_type fn_type, machine_mode outmode)
{
    mtcs_calls_emit_library_call_value_1 (self,0, fun, NULL_RTX, fn_type, outmode, 0, NULL);
}

//原型 emit_library_call rtl.h 下同
inline void mtcs_calls_emit_library_call (MtcsCalls *self,rtx fun, libcall_type fn_type, machine_mode outmode,
           rtx arg1, machine_mode arg1_mode)
{
  mtcs_rtx_mode_t args[] = { mtcs_rtx_mode_t (arg1, arg1_mode) };
  mtcs_calls_emit_library_call_value_1 (self,0, fun, NULL_RTX, fn_type, outmode, 1, args);
}

inline void mtcs_calls_emit_library_call (MtcsCalls *self,rtx fun, libcall_type fn_type, machine_mode outmode,
           rtx arg1, machine_mode arg1_mode,
           rtx arg2, machine_mode arg2_mode)
{
  mtcs_rtx_mode_t args[] = {
        mtcs_rtx_mode_t (arg1, arg1_mode),
        mtcs_rtx_mode_t (arg2, arg2_mode)
  };
  mtcs_calls_emit_library_call_value_1 (self,0, fun, NULL_RTX, fn_type, outmode, 2, args);
}

inline void mtcs_calls_emit_library_call (MtcsCalls *self,rtx fun, libcall_type fn_type, machine_mode outmode,
           rtx arg1, machine_mode arg1_mode,
           rtx arg2, machine_mode arg2_mode,
           rtx arg3, machine_mode arg3_mode)
{
  mtcs_rtx_mode_t args[] = {
        mtcs_rtx_mode_t (arg1, arg1_mode),
        mtcs_rtx_mode_t (arg2, arg2_mode),
        mtcs_rtx_mode_t (arg3, arg3_mode)
  };
  mtcs_calls_emit_library_call_value_1 (self,0, fun, NULL_RTX, fn_type, outmode, 3, args);
}

inline void mtcs_calls_emit_library_call (MtcsCalls *self,rtx fun, libcall_type fn_type, machine_mode outmode,
           rtx arg1, machine_mode arg1_mode,
           rtx arg2, machine_mode arg2_mode,
           rtx arg3, machine_mode arg3_mode,
           rtx arg4, machine_mode arg4_mode)
{
  mtcs_rtx_mode_t args[] = {
        mtcs_rtx_mode_t (arg1, arg1_mode),
        mtcs_rtx_mode_t (arg2, arg2_mode),
        mtcs_rtx_mode_t (arg3, arg3_mode),
        mtcs_rtx_mode_t (arg4, arg4_mode)
  };
  mtcs_calls_emit_library_call_value_1(self,0, fun, NULL_RTX, fn_type, outmode, 4, args);
}

/* Like emit_library_call, but return the value produced by the call.
   Use VALUE to store the result if it is nonnull, otherwise pick a
   convenient location.  */
//原型 emit_library_call_value rtl.h
inline rtx mtcs_calls_emit_library_call_value(MtcsCalls *self,rtx fun, rtx value, libcall_type fn_type,
             machine_mode outmode)
{
  return mtcs_calls_emit_library_call_value_1(self,1, fun, value, fn_type, outmode, 0, NULL);
}

inline rtx mtcs_calls_emit_library_call_value (MtcsCalls *self,rtx fun, rtx value, libcall_type fn_type,
             machine_mode outmode,rtx arg1, machine_mode arg1_mode)
{
  mtcs_rtx_mode_t args[] = { mtcs_rtx_mode_t (arg1, arg1_mode) };
  return mtcs_calls_emit_library_call_value_1(self,1, fun, value, fn_type, outmode, 1, args);
}

inline rtx mtcs_calls_emit_library_call_value (MtcsCalls *self,rtx fun, rtx value, libcall_type fn_type,
             machine_mode outmode,
             rtx arg1, machine_mode arg1_mode,
             rtx arg2, machine_mode arg2_mode)
{
  mtcs_rtx_mode_t args[] = {
        mtcs_rtx_mode_t (arg1, arg1_mode),
        mtcs_rtx_mode_t (arg2, arg2_mode)
  };
  return mtcs_calls_emit_library_call_value_1(self,1, fun, value, fn_type, outmode, 2, args);
}

inline rtx mtcs_calls_emit_library_call_value (MtcsCalls *self,rtx fun, rtx value, libcall_type fn_type,
             machine_mode outmode,
             rtx arg1, machine_mode arg1_mode,
             rtx arg2, machine_mode arg2_mode,
             rtx arg3, machine_mode arg3_mode)
{
  mtcs_rtx_mode_t args[] = {
        mtcs_rtx_mode_t (arg1, arg1_mode),
        mtcs_rtx_mode_t (arg2, arg2_mode),
        mtcs_rtx_mode_t (arg3, arg3_mode)
  };
  return mtcs_calls_emit_library_call_value_1(self,1, fun, value, fn_type, outmode, 3, args);
}

inline rtx mtcs_calls_emit_library_call_value (MtcsCalls *self,rtx fun, rtx value, libcall_type fn_type,
             machine_mode outmode,
             rtx arg1, machine_mode arg1_mode,
             rtx arg2, machine_mode arg2_mode,
             rtx arg3, machine_mode arg3_mode,
             rtx arg4, machine_mode arg4_mode)
{
  mtcs_rtx_mode_t args[] = {
        mtcs_rtx_mode_t (arg1, arg1_mode),
        mtcs_rtx_mode_t (arg2, arg2_mode),
        mtcs_rtx_mode_t (arg3, arg3_mode),
        mtcs_rtx_mode_t (arg4, arg4_mode)
  };
  return mtcs_calls_emit_library_call_value_1(self,1, fun, value, fn_type, outmode, 4, args);
}

//原型 pass_by_reference calls.h calls.cc
bool mtcs_calls_pass_by_reference (MtcsCalls *self,MtcsCumulativeArgs/*!CUMULATIVE_ARGS*/*ca, mtcs_function_arg_info arg);
//原型 apply_pass_by_reference_rules calls.h calls.cc
bool mtcs_calls_apply_pass_by_reference_rules (MtcsCalls *self,MtcsCumulativeArgs/*!CUMULATIVE_ARGS*/ *ca,
        mtcs_function_arg_info &arg);

//原型 must_pass_in_stack_var_size_or_pad calls.h calls.cc
bool mtcs_calls_must_pass_in_stack_var_size_or_pad (MtcsCalls *self,const mtcs_function_arg_info &arg);
//原型 fixup_tail_calls calls.h calls.cc
void mtcs_calls_fixup_tail_calls (MtcsCalls *self);
//原型 rtx_for_static_chain calls.h calls.cc
rtx mtcs_calls_rtx_for_static_chain (MtcsCalls *self,const_tree fndecl_or_type, bool incoming_p);

#endif
