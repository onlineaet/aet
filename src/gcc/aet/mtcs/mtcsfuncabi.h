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

#ifndef __GCC_MTCS_FUNC_ABI__
#define __GCC_MTCS_FUNC_ABI__

#include "../nlib.h"
#include "mtcsmicro.h"
#include "mtcscomponent.h"

typedef struct _MtcsFuncAbi MtcsFuncAbi;

/* Information about one of the target's predefined ABIs.  */
//原型 class predefined_function_abi  function-abi.h
class mtcs_predefined_function_abi
{
public:
  /* A target-specific identifier for this ABI.  The value must be in
     the range [0, NUM_ABI_IDS - 1].  */
  unsigned int id () const { return m_id; }

  /* True if this ABI has been initialized.  */
  bool initialized_p () const { return m_initialized; }

  /* Return true if a function call is allowed to alter every bit of
     register REGNO, so that the register contains an arbitrary value
     on return.  If so, the register cannot hold any part of a value
     that is live across a call.  */
  bool  clobbers_full_reg_p (unsigned int regno) const;
//  {
//    return TEST_HARD_REG_BIT (m_full_reg_clobbers, regno);
//  }

  /* Return true if a function call is allowed to alter some or all bits
     of register REGNO.

     This is true whenever clobbers_full_reg_p (REGNO) is true.  It is
     also true if, for example, the ABI says that a call must preserve the
     low 32 or 64 bits of REGNO, but can clobber the upper bits of REGNO.
     In the latter case, it is possible for REGNO to hold values that
     are live across a call, provided that the value occupies only the
     call-preserved part of the register.  */
  bool  clobbers_at_least_part_of_reg_p (unsigned int regno) const;
//  {
//    return TEST_HARD_REG_BIT (m_full_and_partial_reg_clobbers, regno);
//  }

  /* Return true if a function call is allowed to clobber at least part
     of (reg:MODE REGNO).  If so, it is not possible for the register
     as a whole to be live across a call.  */
  bool  clobbers_reg_p (machine_mode mode, unsigned int regno) const;
//  {
//    return overlaps_hard_reg_set_p (m_mode_clobbers[mode], mode, regno);
//  }

  /* Return the set of registers that a function call is allowed to
     alter completely, so that the registers contain arbitrary values
     on return.  This doesn't include registers that a call can only
     partly clobber (as per TARGET_HARD_REGNO_CALL_PART_CLOBBERED).

     These registers cannot hold any part of a value that is live across
     a call.  */
  HardRegSet full_reg_clobbers () const { return m_full_reg_clobbers; }

  /* Return the set of registers that a function call is allowed to alter
     to some degree.  For example, if an ABI says that a call must preserve
     the low 32 or 64 bits of a register R, but can clobber the upper bits
     of R, R would be in this set but not in full_reg_clobbers ().

     This set is a superset of full_reg_clobbers ().  It is possible for a
     register in full_and_partial_reg_clobbers () & ~full_reg_clobbers ()
     to contain values that are live across a call, provided that the live
     value only occupies the call-preserved part of the register.  */
  HardRegSet  full_and_partial_reg_clobbers () const
  {
    return m_full_and_partial_reg_clobbers;
  }

  /* Return the set of registers that cannot be used to hold a value of
     mode MODE across a function call.  That is:

       (reg:REGNO MODE)

     might be clobbered by a call whenever:

       overlaps_hard_reg_set (mode_clobbers (MODE), MODE, REGNO)

     In allocation terms, the registers in the returned set conflict
     with any value of mode MODE that is live across a call.  */
  HardRegSet  mode_clobbers (machine_mode mode) const
  {
    return m_mode_clobbers[mode];
  }

  void setMtcsFuncAbi(MtcsFuncAbi *abi)
  {
      mtcsFuncAbi=abi;
  }

  void initialize (unsigned int, const HardRegSet);
  void add_full_reg_clobber (unsigned int);
  MtcsFuncAbi *mtcsFuncAbi;

private:
  unsigned int m_id : MTCS_NUM_ABI_IDS;
  unsigned int m_initialized : 1;
  HardRegSet m_full_reg_clobbers;
  HardRegSet m_full_and_partial_reg_clobbers;
  HardRegSet m_mode_clobbers[MAX_NUM_MACHINE_MODES/*!NUM_MACHINE_MODES*/];

};

/* Describes either a predefined ABI or the ABI of a particular function.
   In the latter case, the ABI might make use of extra function-specific
   information, such as for -fipa-ra.  */
//原型 class function_abi  function-abi.h
class mtcs_function_abi
{
public:
  /* Initialize the structure for a general function with the given ABI.  */
    mtcs_function_abi (const mtcs_predefined_function_abi &base_abi,MtcsFuncAbi *abi)
    : m_base_abi (&base_abi),
      m_mask (base_abi.full_and_partial_reg_clobbers ()),mtcsFuncAbi(abi) {}

  /* Initialize the structure for a function that has the given ABI and
     that is known not to clobber registers outside MASK.  */
    mtcs_function_abi (const mtcs_predefined_function_abi &base_abi,HardRegSet mask,MtcsFuncAbi *abi)
    :  m_base_abi (&base_abi), m_mask (mask),mtcsFuncAbi(abi) {}

  /* The predefined ABI from which this ABI is derived.  */
  const mtcs_predefined_function_abi &base_abi () const { return *m_base_abi; }

  /* The target-specific identifier of the predefined ABI.  */
  unsigned int id () const { return m_base_abi->id (); }

  /* See the corresponding predefined_function_abi functions for
     details about the following functions.  */

  HardRegSet  full_reg_clobbers () const
  {
    return m_mask & m_base_abi->full_reg_clobbers ();
  }

  HardRegSet   full_and_partial_reg_clobbers () const
  {
    return m_mask & m_base_abi->full_and_partial_reg_clobbers ();
  }

  HardRegSet   mode_clobbers (machine_mode mode) const
  {
    return m_mask & m_base_abi->mode_clobbers (mode);
  }

  bool  clobbers_full_reg_p (unsigned int regno) const;
//  {
//    return (TEST_HARD_REG_BIT (m_mask, regno)
//        & m_base_abi->clobbers_full_reg_p (regno));
//  }

  bool  clobbers_at_least_part_of_reg_p (unsigned int regno) const;
//  {
//    return (TEST_HARD_REG_BIT (m_mask, regno)
//        & m_base_abi->clobbers_at_least_part_of_reg_p (regno));
//  }

  bool  clobbers_reg_p (machine_mode mode, unsigned int regno) const;
//  {
//    return overlaps_hard_reg_set_p (mode_clobbers (mode), mode, regno);
//  }

  bool  operator== (const mtcs_function_abi &other) const
  {
    return m_base_abi == other.m_base_abi && m_mask == other.m_mask;
  }

  bool  operator!= (const mtcs_function_abi &other) const
  {
    return !operator== (other);
  }

  MtcsFuncAbi *mtcsFuncAbi;

protected:
  const mtcs_predefined_function_abi *m_base_abi;
  HardRegSet m_mask;
};

/* This class collects information about the ABIs of functions that are
   called in a particular region of code.  It is mostly intended to be
   used as a local variable during an IR walk.  */
class mtcs_function_abi_aggregator
{
public:
    mtcs_function_abi_aggregator (MtcsFuncAbi *abi) : m_abi_clobbers (),mtcsFuncAbi(abi) {}

  /* Record that the code region calls a function with the given ABI.  */
  void   note_callee_abi (const mtcs_function_abi &abi)
  {
    m_abi_clobbers[abi.id ()] |= abi.full_and_partial_reg_clobbers ();
  }

  HardRegSet caller_save_regs (const mtcs_function_abi &) const;

private:
  HardRegSet m_abi_clobbers[MTCS_NUM_ABI_IDS];
  MtcsFuncAbi *mtcsFuncAbi;

};


struct _MtcsFuncAbi
{
    MtcsComponent parent;
    //原型 struct target_function_abi_info 中的成员变量 predefined_function_abi x_function_abis function-abi.h
//#define function_abis \
//  (this_target_function_abi_info->x_function_abis)    等同于 mtcsFuncAbi->x_function_abis
//#define default_function_abi \
//  (this_target_function_abi_info->x_function_abis[0]) 等同于 mtcsFuncAbi->x_function_abis[0]
//#define eh_edge_abi default_function_abi              等同于 mtcsFuncAbi->x_function_abis[0]
    mtcs_predefined_function_abi x_function_abis[MTCS_NUM_ABI_IDS];

};


MtcsFuncAbi    *mtcs_func_abi_new(MtcsMode *mtcsMode);
//原型 fntype_abi function-abi.h function-abi.cc
const mtcs_predefined_function_abi &mtcs_func_abi_fntype_abi(MtcsFuncAbi *self,const_tree type);
//原型 fndecl_abi function-abi.h function-abi.cc
mtcs_function_abi mtcs_func_abi_fndecl_abi(MtcsFuncAbi *self,const_tree fndecl);
//原型 insn_callee_abi function-abi.h function-abi.cc
mtcs_function_abi mtcs_func_abi_insn_callee_abi (MtcsFuncAbi *self,const rtx_insn *insn);
//原型 expr_callee_abi function-abi.h function-abi.cc
mtcs_function_abi mtcs_func_abi_expr_callee_abi (MtcsFuncAbi *self,const_tree exp);
//原型 call_clobbered_in_region_p function-abi.h
bool mtcs_func_abi_call_clobbered_in_region_p (MtcsFuncAbi *self,unsigned int abis, const HardRegSet mask,
                machine_mode mode, unsigned int regno);
//原型 call_clobbers_in_region funciton-abi.h function.cc
HardRegSet mtcs_func_abi_call_clobbers_in_region(MtcsFuncAbi *self,unsigned int abis, const HardRegSet mask,
             machine_mode mode);

const mtcs_predefined_function_abi *mtcs_func_abi_get_default(MtcsFuncAbi *self);
//原型 call_clobbers_in_region function-abi.h function-abi.cc
HardRegSet mtcs_func_abi_call_clobbers_in_region (MtcsFuncAbi *self,unsigned int abis, HardRegSet mask,machine_mode mode);

#endif
