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
 * base on function-abi.cc
 */


#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "regs.h"
#include "varasm.h"
#include "cgraph.h"

#include "mtcstarget.h"
#include "mtcsfuncabi.h"

/* Initialize a predefined function ABI with the given values of
   ID and FULL_REG_CLOBBERS.  */
//原型 predefined_function_abi::initialize function-abi.h function-abi.cc reginfo.cc调用
void mtcs_predefined_function_abi::initialize (unsigned int id, const HardRegSet full_reg_clobbers)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsFuncAbi);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  HardRegSet result={mtcs_reg_get_hard_reg_element_count(mtcsReg)};
 //n_debug("mtcsfuncabi.c mtcs_predefined_function_abi::initialize 00:%d %d\n",full_reg_clobbers.count,full_reg_clobbers.elts[0]);
  m_id = id;
  m_initialized = true;
  m_full_reg_clobbers = full_reg_clobbers;
 //n_debug("mtcsfuncabi.c mtcs_predefined_function_abi::initialize 11:%d %d\n",m_full_reg_clobbers.count,m_full_reg_clobbers.elts[0]);

  int numMachineModes=mtcs_mode_get_number(mtcsMode);
  int firstPseudoRegister=mtcs_reg_get_first_pseudo_register(mtcsReg);
  /* Set up the value of m_full_and_partial_reg_clobbers.

     If the ABI specifies that part of a hard register R is call-clobbered,
     we should be able to find a single-register mode M for which
     targetm.hard_regno_call_part_clobbered (m_id, R, M) is true.
     In other words, it shouldn't be the case that R can hold all
     single-register modes across a call, but can't hold part of
     a multi-register mode.

     If that assumption doesn't hold for a future target, we would need
     to change the interface of TARGET_HARD_REGNO_CALL_PART_CLOBBERED so
     that it tells us which registers in a multi-register value are
     actually clobbered.  */
  m_full_and_partial_reg_clobbers = full_reg_clobbers;
  for (unsigned int i = 0; i <numMachineModes/*!NUM_MACHINE_MODES*/; ++i){
      machine_mode mode = (machine_mode) i;
      for (unsigned int regno = 0; regno < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; ++regno)
        if (mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,regno, mode)
            && mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,regno, mode) == 1
            && mtcsTarget/*!targetm.hard_regno_call_part_clobbered*/->hard_regno_call_part_clobbered(mtcsTarget,m_id, regno, mode))
            mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&m_full_and_partial_reg_clobbers, regno);
  }

  /* For each mode MODE, work out which registers are unable to hold
     any part of a MODE value across a call, i.e. those for which no
     overlapping call-preserved (reg:MODE REGNO) exists.

     We assume that this can be flipped around to say that a call
     preserves (reg:MODE REGNO) unless the register overlaps this set.
     The usual reason for this being true is that if (reg:MODE REGNO)
     contains a part-clobbered register, that register would be
     part-clobbered regardless of which part of MODE it holds.
     For example, if (reg:M 2) occupies two registers and if the
     register 3 portion of it is part-clobbered, (reg:M 3) is usually
     either invalid or also part-clobbered.  */
  for (unsigned int i = 0; i < numMachineModes/*!NUM_MACHINE_MODES*/; ++i){
      machine_mode mode = (machine_mode) i;
      m_mode_clobbers[i] = m_full_and_partial_reg_clobbers;
      for (unsigned int regno = 0; regno < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; ++regno)
        if (mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,regno, mode)
            && !mtcs_reg_overlaps_hard_reg_set_p/*!overlaps_hard_reg_set_p*/(mtcsReg,&m_full_reg_clobbers, mode, regno)
            && !mtcsTarget/*!targetm.hard_regno_call_part_clobbered*/->hard_regno_call_part_clobbered(mtcsTarget,m_id, regno, mode))
            mtcs_reg_remove_from_hard_reg_set/*!remove_from_hard_reg_set*/(mtcsReg,&m_mode_clobbers[i], mode, regno);
  }

  /* Check that the assumptions above actually hold, i.e. that testing
     for single-register modes makes sense, and that overlap tests for
     mode_clobbers work as expected.  */
  if (mtcsOptionsItem->x_flag_checking/*!flag_checking*/)
     for (unsigned int i = 0; i <  numMachineModes/*!NUM_MACHINE_MODES*/; ++i){
        machine_mode mode = (machine_mode) i;
        const HardRegSet all_clobbers = m_full_and_partial_reg_clobbers;
        for (unsigned int regno = 0; regno < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; ++regno)
          if (mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,regno, mode)
              && !mtcs_reg_overlaps_hard_reg_set_p/*!overlaps_hard_reg_set_p*/(mtcsReg,&m_full_reg_clobbers, mode, regno)
              && mtcsTarget/*!targetm.hard_regno_call_part_clobbered*/->hard_regno_call_part_clobbered(mtcsTarget,m_id, regno, mode))
            gcc_assert (mtcs_reg_overlaps_hard_reg_set_p/*!overlaps_hard_reg_set_p*/(mtcsReg,&all_clobbers, mode, regno)
                && mtcs_reg_overlaps_hard_reg_set_p/*!overlaps_hard_reg_set_p*/(mtcsReg,&m_mode_clobbers[i],mode, regno));
     }

 // n_debug("mtcsfuncabi.c mtcs_predefined_function_abi::initialize 22:%d %d\n",m_full_reg_clobbers.count,m_full_reg_clobbers.elts[0]);
 //n_debug("mtcsfuncabi.c mtcs_predefined_function_abi::initialize 33:%d %d\n",
        //  m_full_and_partial_reg_clobbers.count,m_full_and_partial_reg_clobbers.elts[0]);
  //for(int i=0;i<numMachineModes;i++){
    //n_debug("mtcsfuncabi.c mtcs_predefined_function_abi::initialize 44 i:%d :%d %d\n",
            //  i,m_mode_clobbers[i].count,m_mode_clobbers[i].elts[0]);
  //}


}

/* If the ABI has been initialized, add REGNO to the set of registers
   that can be completely altered by a call.  */
//原型 add_full_reg_clobber function-abi.h function-abi.cc
void mtcs_predefined_function_abi::add_full_reg_clobber (unsigned int regno)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsFuncAbi);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

  int numMachineModes=mtcs_mode_get_number(mtcsMode);

  if (!m_initialized)
    return;

  mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&m_full_reg_clobbers, regno);
  mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&m_full_and_partial_reg_clobbers, regno);
  for (unsigned int i = 0; i < numMachineModes/*!NUM_MACHINE_MODES*/; ++i)
      mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&m_mode_clobbers[i], regno);
}

/* Return the set of registers that the caller of the recorded functions must
   save in order to honor the requirements of CALLER_ABI.  */
//原型 caller_save_regs function-abi.h function-abi.cc
HardRegSet mtcs_function_abi_aggregator::caller_save_regs (const mtcs_function_abi &caller_abi) const
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsFuncAbi);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  int numMachineModes=mtcs_mode_get_number(mtcsMode);

  HardRegSet result={mtcs_reg_get_hard_reg_element_count(mtcsReg)};

  mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&result);

  for (unsigned int abi_id = 0; abi_id < MTCS_NUM_ABI_IDS; ++abi_id){
       const mtcs_predefined_function_abi &callee_abi = mtcsFuncAbi->x_function_abis[abi_id];

       /* Skip cases that clearly aren't problematic.  */
       if (abi_id == caller_abi.id ()  || mtcs_reg_hard_reg_set_empty_p/*!hard_reg_set_empty_p*/(&m_abi_clobbers[abi_id]))
          continue;

      /* Collect the set of registers that can be "more clobbered" by
     CALLEE_ABI than by CALLER_ABI.  */
      HardRegSet extra_clobbers={mtcs_reg_get_hard_reg_element_count(mtcsReg)};
      mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&extra_clobbers);

      for (unsigned int i = 0; i < numMachineModes/*!NUM_MACHINE_MODES*/; ++i){
          machine_mode mode = (machine_mode) i;
          extra_clobbers |= (callee_abi.mode_clobbers (mode) & ~caller_abi.mode_clobbers (mode));
      }

      /* Restrict it to the set of registers that we actually saw
       clobbers for (e.g. taking -fipa-ra into account).  */
      result |= (extra_clobbers & m_abi_clobbers[abi_id]);
  }
  return result;
}

/* Return the set of registers that cannot be used to hold a value of
   mode MODE across the calls in a region described by ABIS and MASK, where:

   * Bit ID of ABIS is set if the region contains a call with
     function_abi identifier ID.

   * MASK contains all the registers that are fully or partially
     clobbered by calls in the region.

   This is not quite as accurate as testing each individual call,
   but it's a close and conservatively-correct approximation.
   It's much better for some targets than just using MASK.  */
//原型 call_clobbers_in_region funciton-abi.h function.cc
HardRegSet mtcs_func_abi_call_clobbers_in_region(MtcsFuncAbi *self,unsigned int abis, const HardRegSet mask,
             machine_mode mode)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
    HardRegSet result={mtcs_reg_get_hard_reg_element_count(mtcsReg)};
    mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&result);
    for (unsigned int id = 0; abis; abis >>= 1, ++id)
      if (abis & 1)
         result |= self->x_function_abis/*!function_abis*/[id].mode_clobbers(mode);
    return result & mask;
}

/* Return the predefined ABI used by functions with type TYPE.  */
//原型 fntype_abi function-abi.h function-abi.cc
const mtcs_predefined_function_abi &mtcs_func_abi_fntype_abi (MtcsFuncAbi *self,const_tree type)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  gcc_assert (FUNC_OR_METHOD_TYPE_P (type));
  if (mtcsMachine->calls->fntype_abi/*!targetm.calls.fntype_abi*/)
    return target_calls_fntype_abi/*!targetm.calls.fntype_abi*/(mtcsMachine->calls,type);
  return self->x_function_abis[0]/*!default_function_abi*/;
}

/* Return the ABI of function decl FNDECL.  */
//原型 fndecl_abi function-abi.h function-abi.cc
mtcs_function_abi mtcs_func_abi_fndecl_abi(MtcsFuncAbi *self,const_tree fndecl)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsCgraph *mtcsCgraph=mtcs_target_get_cgraph(mtcsTarget);

  gcc_assert (TREE_CODE (fndecl) == FUNCTION_DECL);
  const mtcs_predefined_function_abi &base_abi = mtcs_func_abi_fntype_abi/*!fntype_abi*/(self,TREE_TYPE (fndecl));

  if (mtcsOptionsItem->x_flag_ipa_ra && mtcs_asm_decl_binds_to_current_def_p/*!decl_binds_to_current_def_p*/(mtcsAsm,fndecl)){
      MtcsCgraphRtlInfo *info=mtcs_cgraph_get_rtl_info(mtcsCgraph,fndecl);
//      if (cgraph_rtl_info *info = cgraph_node::rtl_info (fndecl))
//           return function_abi (self,base_abi, info->function_used_regs);
      if(info){
          /*!return mtcs_function_abi (base_abi, info->function_used_regs);*/
          mtcs_function_abi ret(base_abi, info->function_used_regs,self);
          return ret;
      }
  }
  mtcs_function_abi ret(base_abi,self);
  return ret;
}

/* Return the ABI of the function called by INSN.  */
//原型 insn_callee_abi function-abi.h function-abi.cc
mtcs_function_abi mtcs_func_abi_insn_callee_abi (MtcsFuncAbi *self,const rtx_insn *insn)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  gcc_assert (insn && CALL_P (insn));
  n_debug("mtcsfuncabi.c mtcs_func_abi_insn_callee_abi 00 %p %p self:%p mtcsOptionsItem->x_flag_ipa_ra:%d\n",
        mtcsMode,mtcsTarget,self,mtcsOptionsItem->x_flag_ipa_ra);
  if (mtcsOptionsItem->x_flag_ipa_ra)
    if (tree fndecl = get_call_fndecl (insn))
      return mtcs_func_abi_fndecl_abi/*!fndecl_abi*/(self,fndecl);
  n_debug("mtcsfuncabi.cmtcs_func_abi_insn_callee_abi 11 %p %p\n",mtcsMode,mtcsTarget);

  if (mtcsMachine->calls->insn_callee_abi/*!targetm.calls.insn_callee_abi*/)
    return target_calls_insn_callee_abi/*!targetm.calls.insn_callee_abi*/(mtcsMachine->calls,insn);
  n_debug("mtcsfuncabi.c mtcs_func_abi_insn_callee_abi 22 %p %p %p\n",mtcsMode,mtcsTarget,self->x_function_abis[0].mtcsFuncAbi);
  mtcs_function_abi abi(self->x_function_abis[0],self);
  return abi;  /*!return self->x_function_abis[0]/*!default_function_abi*/;
}

/* Return the ABI of the function called by CALL_EXPR EXP.  Return the
   default ABI for erroneous calls.  */
//原型 expr_callee_abi function-abi.h function-abi.cc
mtcs_function_abi mtcs_func_abi_expr_callee_abi (MtcsFuncAbi *self,const_tree exp)
{
  gcc_assert (TREE_CODE (exp) == CALL_EXPR);

  if (tree fndecl = get_callee_fndecl (exp))
    return mtcs_func_abi_fndecl_abi/*!fndecl_abi*/(self,fndecl);

  tree callee = CALL_EXPR_FN (exp);
  if (callee == error_mark_node){
     mtcs_function_abi ret(self->x_function_abis[0],self);
     return ret;/*!return self->x_function_abis[0]*//*!default_function_abi*/;
  }
  tree type = TREE_TYPE (callee);
  if (type == error_mark_node){
     mtcs_function_abi ret(self->x_function_abis[0],self);
     return ret;/*!return self->x_function_abis[0]*//*!default_function_abi*/;
  }
  gcc_assert (POINTER_TYPE_P (type));
  const mtcs_predefined_function_abi &baseAbi = mtcs_func_abi_fntype_abi/*!fntype_abi*/(self,TREE_TYPE (type));
  mtcs_function_abi ret(baseAbi,self);
  return ret;
}

/* Return true if (reg:MODE REGNO) might be clobbered by one of the
   calls in a region described by ABIS and MASK, where:

   * Bit ID of ABIS is set if the region contains a call with
     function_abi identifier ID.

   * MASK contains all the registers that are fully or partially
     clobbered by calls in the region.

   This is not quite as accurate as testing each individual call,
   but it's a close and conservatively-correct approximation.
   It's much better for some targets than:

     overlaps_hard_reg_set_p (MASK, MODE, REGNO).  */
//原型 call_clobbered_in_region_p function-abi.h
bool mtcs_func_abi_call_clobbered_in_region_p (MtcsFuncAbi *self,unsigned int abis, const HardRegSet mask,
                machine_mode mode, unsigned int regno)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

  HardRegSet clobbers = mtcs_func_abi_call_clobbers_in_region/*!call_clobbers_in_region*/(self,abis, mask, mode);
  return mtcs_reg_overlaps_hard_reg_set_p/*!overlaps_hard_reg_set_p*/(mtcsReg,&clobbers, mode, regno);
}


static void mtcsFuncAbiInit(MtcsFuncAbi *self)
{
    int i;
    for(i=0;i<MTCS_NUM_ABI_IDS;i++){
       //n_debug("mtcsfuncabi.c mtcsFuncAbiInit:%p\n",self);
        self->x_function_abis[i].setMtcsFuncAbi(self);
    }
}

bool  mtcs_predefined_function_abi::clobbers_full_reg_p (unsigned int regno) const
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsFuncAbi);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
    bool ret= mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&m_full_reg_clobbers, regno);
   //n_debug("mtcsfuncabi.c function-abi.h clobbers_full_reg_p regno:%d m_full_reg_clobbers:%d %d ret:%d\n",
           // regno,m_full_reg_clobbers.count,m_full_reg_clobbers.elts[0],ret);
    return ret;

}

bool  mtcs_predefined_function_abi::clobbers_at_least_part_of_reg_p (unsigned int regno) const
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsFuncAbi);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
    return  mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&m_full_and_partial_reg_clobbers, regno);
}

bool  mtcs_predefined_function_abi::clobbers_reg_p (machine_mode mode, unsigned int regno) const
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsFuncAbi);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
    return mtcs_reg_overlaps_hard_reg_set_p/*!overlaps_hard_reg_set_p*/(mtcsReg,&m_mode_clobbers[mode], mode, regno);
}

bool  mtcs_function_abi::clobbers_full_reg_p (unsigned int regno) const
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsFuncAbi);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

    return (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&m_mask, regno)
      & m_base_abi->clobbers_full_reg_p (regno));
}

bool  mtcs_function_abi::clobbers_at_least_part_of_reg_p (unsigned int regno) const
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsFuncAbi);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
    return (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&m_mask, regno)
      & m_base_abi->clobbers_at_least_part_of_reg_p (regno));
}

bool   mtcs_function_abi::clobbers_reg_p (machine_mode mode, unsigned int regno) const
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsFuncAbi);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
    HardRegSet c=mode_clobbers (mode);
    return  mtcs_reg_overlaps_hard_reg_set_p/*!overlaps_hard_reg_set_p*/(mtcsReg,&c, mode, regno);
}

const mtcs_predefined_function_abi *mtcs_func_abi_get_default(MtcsFuncAbi *self)
{
   n_debug("mtcsfuncabi.c mtcs_func_abi_get_default:x_function_abis[0]:%p\n",&self->x_function_abis[0]);
    return &self->x_function_abis[0];
}

MtcsFuncAbi    *mtcs_func_abi_new(MtcsMode *mtcsMode)
{
    MtcsFuncAbi *self = n_slice_alloc0 (sizeof(MtcsFuncAbi));
    mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
    mtcsFuncAbiInit(self);
    return self;
}

