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
#include "function.h"

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
#include "options.h"
#include "expmed.h"
#include "expr.h"
#include "fold-const.h"
#include "dwarf2out.h"
#include "common/common-targhooks.h"
#include "symbol-summary.h"
#include "sreal.h"
#include "value-range.h"
#include "ipa-cp.h"
#include "ipa-prop.h"
#include "ipa-fnsummary.h"
#include "builtins.h"
#include "opts.h"
#include "gimple.h"
#include "gimple-ssa.h"
#include "tree-ssa-alias.h"
#include "tree-ssanames.h"
#include "tree-ssa-live.h"
#include "value-query.h"

#include "tree-ssa-operands.h"
#include "mtcsrange.h"
#include "aet/aetprinttree.h"
#include "mtcsdfcore.h"
#include "mtcsdfproblems.h"
#include "mtcsssapropagate.h"
#include "mtcsssacoalesce.h"

#include "mtcstarget.h"




/**
 * 实现缺省的TARGET_STRIP_NAME_ENCODING
 */
const char *stripNameEncoding_cb (const char *str)
{
   return str + (*str == '*');
}

/**
 * 来自varasm.cc
 * mtcsasm mtcsvarasm 都定义有
 */
static inline tree ultimate_transparent_alias_target (tree *alias)
{
  tree target = *alias;

  if (IDENTIFIER_TRANSPARENT_ALIAS (target))
  {
      gcc_assert (TREE_CHAIN (target));
      target = ultimate_transparent_alias_target (&TREE_CHAIN (target));
      gcc_assert (! IDENTIFIER_TRANSPARENT_ALIAS (target) && ! TREE_CHAIN (target));
      *alias = target;
  }
  return target;
}

/* Determine the debugging unwind mechanism for the target.  */
//原型 targetm.debug_unwind_info #define TARGET_DEBUG_UNWIND_INFO default_debug_unwind_info

static enum unwind_info_type debugUnwindInfo_cb(MtcsTarget *self)
{
   MtcsConfig *mtcsConfig = mtcs_target_get_config(self);
   MtcsOpts *mtcsOpts = mtcs_target_get_opts(self);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(self);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   /* If the target wants to force the use of dwarf2 unwind info, let it.  */
   /* ??? Change all users to the hook, then poison this.  */
   if(mtcs_config_ifdef(mtcsConfig,MTCS_DWARF2_FRAME_INFO)){/*!#ifdef DWARF2_FRAME_INFO*/
      if (mtcs_config_get_value(mtcsConfig,MTCS_DWARF2_FRAME_INFO)/*!DWARF2_FRAME_INFO*/)
         return UI_DWARF2;
   }//#endif

   /* Otherwise, only turn it on if dwarf2 debugging is enabled.  */
   if(mtcs_config_ifdef(mtcsConfig,MTCS_DWARF2_DEBUGGING_INFO)){/*!#ifdef DWARF2_DEBUGGING_INFO*/
      if (mtcs_opts_dwarf_debuginfo_p/*!dwarf_debuginfo_p*/(mtcsOpts,mtcsOptionsItem))
         return UI_DWARF2;
   }//#endif

   return UI_NONE;
}

/**
 * 原型 targetm.profile_before_prologue; TARGET_PROFILE_BEFORE_PROLOGUE default_profile_before_prologue
 *
 */
static bool profileBeforePrologue_cb (void)
{
#ifdef PROFILE_BEFORE_PROLOGUE
  return true;
#else
  return false;
#endif
}

//原型 targetm.have_conditional_execution #define TARGET_HAVE_CONDITIONAL_EXECUTION default_have_conditional_execution
static bool haveConditionalExecution_cb (MtcsTarget *self)
{
    //nvptx insn-config.h中HAVE_conditional_execution=1 host HAVE_conditional_execution=0;
  return 1;//HAVE_conditional_execution;
}

//原型 targetm.constant_alignment (DECL_INITIAL (decl), align); #define TARGET_CONSTANT_ALIGNMENT default_constant_alignment
HOST_WIDE_INT constanAlignment_cb (MtcsTarget *self,const_tree, HOST_WIDE_INT align)
{
  return align;
}

/* Subroutine of compute_reloc_for_rtx for leaf rtxes.  */
static int compute_reloc_for_rtx_1 (const_rtx x)
{
  switch (GET_CODE (x)){
    case SYMBOL_REF:
      return SYMBOL_REF_LOCAL_P (x) ? 1 : 2;
    case LABEL_REF:
      return 1;
    default:
      return 0;
  }
}

/* Like compute_reloc_for_constant, except for an RTX.  The return value
   is a mask for which bit 1 indicates a global relocation, and bit 0
   indicates a local relocation.  Used by default_select_rtx_section
   and default_elf_select_rtx_section.  */
static int compute_reloc_for_rtx (const_rtx x)
{
  switch (GET_CODE (x)){
    case SYMBOL_REF:
    case LABEL_REF:
      return compute_reloc_for_rtx_1 (x);

    case CONST:
      {
        int reloc = 0;
        subrtx_iterator::array_type array;
        FOR_EACH_SUBRTX (iter, array, x, ALL)
          reloc |= compute_reloc_for_rtx_1 (*iter);
        return reloc;
      }

    default:
      return 0;
  }
}

//原型targetm.binds_local_p (tem) #define TARGET_BINDS_LOCAL_P default_binds_local_p
static bool bindsLocalP_cb(MtcsTarget *self,const_tree exp)
{
    return default_binds_local_p(exp);
}

//原型targetm.in_small_data_p (decl) #define TARGET_IN_SMALL_DATA_P hook_bool_const_tree_false
static bool inSmallDataP_cb(MtcsTarget *self,const_tree decl)
{
    return false;
}


//原型targetm.use_blocks_for_decl_p (decl);#define TARGET_USE_BLOCKS_FOR_DECL_P hook_bool_const_tree_true
static bool useBlocksForDeclP_cb(MtcsTarget *self,const_tree decl)
{
    return true;
}

/* Determine whether or not a pointer mode is valid. Assume defaults
   of ptr_mode or Pmode - can be overridden.  */
//原型targetm.valid_pointer_mode (mode);#define TARGET_VALID_POINTER_MODE default_valid_pointer_mode
static bool validPointerMode_cb (MtcsTarget *self,scalar_int_mode mode)
{
  MtcsMode *mtcsMode=self->mtcsMode;
  return (mode == ptr_mode || mode == mtcs_mode_get_Pmode(mtcsMode));
}

//原型  targetm.expand_to_rtl_hook (); #define TARGET_EXPAND_TO_RTL_HOOK hook_void_void
static void expandToRtlHook_cb(MtcsTarget *self)
{

}

//第一个局部变量尾地址到 frame_pointer_rtx之间的偏移.
//原型 targetm.starting_frame_offset () #define TARGET_STARTING_FRAME_OFFSET hook_hwi_void_0
static HOST_WIDE_INT startingFrameOffset_cb (MtcsTarget *self)
{
  return 0;
}

//原型 targetm.use_pseudo_pic_reg () #define TARGET_USE_PSEUDO_PIC_REG hook_bool_void_false
static bool usePseudoPicReg_cb (MtcsTarget *self)
{
  return false;
}

//原型 targetm.dwarf_register_span (reg) #define TARGET_DWARF_REGISTER_SPAN hook_rtx_rtx_null
static rtx  dwarfRegisterSpan_cb (MtcsTarget *self,rtx reg)
{
   return NULL;
}

//原型 targetm.dwarf_frame_reg_mode #define TARGET_DWARF_FRAME_REG_MODE default_dwarf_frame_reg_mode
static  machine_mode dwarfFrameRegMode_cb (MtcsTarget *self,int regno)
{
   MtcsTarget *mtcsTarget = self;
   MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
   MtcsFuncAbi *mtcsFuncAbi = mtcs_target_get_func_abi(mtcsTarget);
   MtcsReg *mtcsReg = mtcs_target_get_reg(mtcsTarget);

   mtcs_predefined_function_abi *ehEdgeAbi = mtcs_func_abi_get_default(mtcsFuncAbi);

   machine_mode save_mode = mtcsReg->hardRegs.x_reg_raw_mode/*!reg_raw_mode*/[regno];

   if (mtcsTarget/*!targetm.hard_regno_call_part_clobbered*/->hard_regno_call_part_clobbered(mtcsTarget,
         ehEdgeAbi/*!eh_edge_abi.id ()*/->id(),regno, save_mode))
      save_mode = mtcs_reg_choose_hard_reg_mode/*!choose_hard_reg_mode*/(mtcsReg,regno, 1, ehEdgeAbi/*!&eh_edge_abi*/);
   return save_mode;
}

//原型 targetm.init_dwarf_reg_sizes_extra (address); #define TARGET_INIT_DWARF_REG_SIZES_EXTRA hook_void_tree
static void initDwarfRegSizesExtra_cb (MtcsTarget *self,tree t)
{

}

static bool legitimateConstantP_cb(MtcsTarget *self,machine_mode mode,rtx x)
{
   return true;
}

static rtx legitimateConstant_cb (MtcsTarget *self,rtx x, rtx orig_x ATTRIBUTE_UNUSED,
                mtcs_mode mode ATTRIBUTE_UNUSED, addr_space_t as ATTRIBUTE_UNUSED)
{
    return self->legitimize_address (self,x, orig_x, mode);
}

static rtx legitimateConstant_1_cb (MtcsTarget *self,rtx x, rtx orig_x ATTRIBUTE_UNUSED,mtcs_mode mode ATTRIBUTE_UNUSED)
{
    return x;
}

//原型 targetm.delegitimize_address (addr); #define TARGET_DELEGITIMIZE_ADDRESS delegitimize_mem_from_attrs
static rtx delegitimizeAddress_cb(MtcsTarget *self,rtx x)
{
    return mtcs_simplify_rtx_delegitimize_mem_from_attrs(self->mtcsSimplifyRtx,x);
}


//原型 targetm.libgcc_cmp_return_mode ();#define TARGET_LIBGCC_CMP_RETURN_MODE default_libgcc_cmp_return_mode
static scalar_int_mode libgccCmpReturnMode_cb (MtcsTarget *self)
{
  return word_mode;
}

/* Default version of cstore_mode.  */
static scalar_int_mode cstoreMode_cb (MtcsTarget *self,enum insn_code icode)
{
  MtcsOutput *mtcsOutput=self->mtcsOutput;
  return mtcs_mode_as_a <scalar_int_mode>(self->mtcsMode,mtcsOutput->insn_data[(int) icode].operand[0].mode);
}

static bool modeDependentAddressP_cb (MtcsTarget *self,const_rtx addr ATTRIBUTE_UNUSED,addr_space_t addrspace ATTRIBUTE_UNUSED)
{
  return false;
}

//原型targetm.modes_tieable_p  #define TARGET_MODES_TIEABLE_P hook_bool_mode_mode_true
static bool modesTieableP_cb (MtcsTarget *self,machine_mode mode1,machine_mode mode2)
{
  return true;
}

//原型targetm.rtx_costs  #define TARGET_RTX_COSTS aarch64_rtx_costs_wrapper
static bool rtxCosts_cb (MtcsTarget *self,rtx x, machine_mode mode, int outer ATTRIBUTE_UNUSED,
           int param ATTRIBUTE_UNUSED, int *cost, bool speed)
{
    return false;
}

//原型targetm.use_by_pieces_infrastructure_p #define TARGET_USE_BY_PIECES_INFRASTRUCTURE_P default_use_by_pieces_infrastructure_p
static bool useByPiecesInfrastructureP_cb(MtcsTarget *self,unsigned HOST_WIDE_INT size,
                    unsigned int alignment,enum by_pieces_operation op,bool speed_p)
{
    MtcsReg *mtcsReg=self->mtcsReg;
    MtcsExpr *mtcsExpr=self->mtcsExpr;
    nuint store_max_pieces=mtcs_reg_get_store_max_pieces(mtcsReg);
    nuint move_max_pieces=mtcs_reg_get_move_max_pieces(mtcsReg);
    nuint ompare_max_pieces=mtcs_reg_get_compare_max_pieces(mtcsReg);

    unsigned int max_size = 0;
    unsigned int ratio = 0;

     switch (op)
       {
       case CLEAR_BY_PIECES:
         max_size = store_max_pieces/*!STORE_MAX_PIECES*/;
         ratio = CLEAR_RATIO (speed_p);
         break;
       case MOVE_BY_PIECES:
         max_size = move_max_pieces/*!MOVE_MAX_PIECES*/;
         ratio = mtcs_reg_get_move_ratio/*!get_move_ratio*/(mtcsReg,speed_p);
         break;
       case SET_BY_PIECES:
         max_size = store_max_pieces/*!STORE_MAX_PIECES*/;
         ratio = SET_RATIO (speed_p);
         break;
       case STORE_BY_PIECES:
         max_size = store_max_pieces/*!STORE_MAX_PIECES*/;
         ratio = mtcs_reg_get_move_ratio/*!get_move_ratio*/(mtcsReg,speed_p);
         break;
       case COMPARE_BY_PIECES:
         max_size = ompare_max_pieces/*!COMPARE_MAX_PIECES*/;
         /* Pick a likely default, just as in get_move_ratio.  */
         ratio = speed_p ? 15 : 3;
         break;
     }
     return mtcs_expr_by_pieces_ninsns/*!by_pieces_ninsns*/(mtcsExpr,size, alignment, max_size + 1, op) < ratio;
}

//原型t targetm.narrow_volatile_bitfield () #define TARGET_NARROW_VOLATILE_BITFIELD hook_bool_void_false
static  bool narrowVolatileBitfield_cb(MtcsTarget *self,const_tree valtype)
{
    return false;
}

//原型 targetm.small_register_classes_for_mode_p (#define TARGET_SMALL_REGISTER_CLASSES_FOR_MODE_P hook_bool_mode_false
static  bool smallRegisterClassesForModeP_cb (MtcsTarget *self,machine_mode funmode)
{
    return false;
}

/* Choose the mode and rtx to use to zero REGNO, storing tem in PMODE and
   PREGNO_RTX and returning TRUE if successful, otherwise returning FALSE.  If
   the natural mode for REGNO doesn't work, attempt to group it with subsequent
   adjacent registers set in TOZERO.  */
static inline bool zcur_select_mode_rtx (MtcsTarget *self,unsigned int regno, machine_mode *pmode,
            rtx *pregno_rtx, HardRegSet *tozero)
{
   MtcsMode *mtcsMode=self->mtcsMode;
   MtcsTarget *mtcsTarget=(MtcsTarget *)self;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   rtx regno_rtx = mtcsRtlData->regno_reg_rtx/*!regno_reg_rtx*/[regno];
   machine_mode mode = GET_MODE (regno_rtx);

   /* If the natural mode doesn't work, try some wider mode.  */
   if (!mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,regno, mode)){
      bool found = false;
      for (int nregs = 2;  !found && nregs <= hard_regno_max_nregs
      && regno + nregs <= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/
      && mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(tozero, regno + nregs - 1); nregs++){
         mode = mtcs_reg_choose_hard_reg_mode/*!choose_hard_reg_mode*/(mtcsReg,regno, nregs, 0);
         if (mode == E_VOIDmode)
            continue;
         gcc_checking_assert (mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,regno, mode));
         regno_rtx = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mode, regno);
         found = true;
      }
      if (!found)
         return false;
   }

   *pmode = mode;
   *pregno_rtx = regno_rtx;
   return true;
}

//原型 targetm.overlap_op_by_pieces_p  #define TARGET_OVERLAP_OP_BY_PIECES_P hook_bool_void_false
static bool overlapOpByPiecesP_cb(MtcsTarget *self)
{
   return false;
}

//原型 targetm.mode_rep_extended (imode, omode)  #define TARGET_MODE_REP_EXTENDED default_mode_rep_extended
static int modeRepExtended_cb(MtcsTarget *self,scalar_int_mode, scalar_int_mode)
{
    return UNKNOWN;
}

//原型 targetm.compare_by_pieces_branch_ratio (mode) #define TARGET_COMPARE_BY_PIECES_BRANCH_RATIO default_compare_by_pieces_branch_ratio
static int compareByPiecesBranchRatio_cb(MtcsTarget *self,machine_mode)
{
    return 1;
}

//原型 targetm.conditional_register_usage (); #define TARGET_CONDITIONAL_REGISTER_USAGE hook_void_void
static void conditionalRegisterUsage_cb(MtcsTarget *self)
{

}

//原型 targetm.hard_regno_call_part_clobbered (m_id, regno, mode) #define TARGET_HARD_REGNO_CALL_PART_CLOBBERED hook_bool_uint_uint_mode_false
static   bool hardRegnoCallPartClobbered_cb(MtcsTarget *self,unsigned int abi_id, unsigned int regno,machine_mode mode)
{
   return false;
}

//原型 targetm.init_libfuncs ();#define TARGET_INIT_LIBFUNCS hook_void_void
static void initLibfuncs_cb(MtcsTarget *self)
{

}

//原型  targetm.default_short_enums ();#define TARGET_DEFAULT_SHORT_ENUMS hook_bool_void_false
static bool defaultShortEnums_cb(MtcsTarget *self)
{
    return false;
}

//原型 targetm.override_options_after_change (); #define TARGET_OVERRIDE_OPTIONS_AFTER_CHANGE hook_void_void
static void overrideOptionsAfterChange_cb(MtcsTarget *self)
{
}

//原型 targetm.stack_protect_guard #define TARGET_STACK_PROTECT_GUARD default_stack_protect_guard
static tree stackProtectGuard_cb(MtcsTarget *self)
{
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(self);
   MtcsTree *mtcsTree = mtcs_target_get_tree(self);

   tree t = self->stack_chk_guard_decl;
   if (t == NULL){
      rtx x;
      t = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,UNKNOWN_LOCATION,
            VAR_DECL, get_identifier ("__stack_chk_guard"),
      ptr_type_node);
      TREE_STATIC (t) = 1;
      TREE_PUBLIC (t) = 1;
      DECL_EXTERNAL (t) = 1;
      TREE_USED (t) = 1;
      TREE_THIS_VOLATILE (t) = 1;
      DECL_ARTIFICIAL (t) = 1;
      DECL_IGNORED_P (t) = 1;
      /* Do not share RTL as the declaration is visible outside of
      current function.  */
      x = mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,t);
      RTX_FLAG (x, used) = 1;
      self->stack_chk_guard_decl = t;
   }
   return t;
}

//原型 targetm.unwind_word_mode (), #define TARGET_UNWIND_WORD_MODE default_unwind_word_mode
static scalar_int_mode unwindWordMode_cb(MtcsTarget *self)
{
   MtcsMode *mtcsMode=self->mtcsMode;
   return mtcsMode->word_mode;
}

//原型 targetm.eh_return_filter_mode () #define TARGET_EH_RETURN_FILTER_MODE default_eh_return_filter_mode
static scalar_int_mode ehReturnFilterMode_cb(MtcsTarget *self)
{
   return unwindWordMode_cb(self);
}

//原型 targetm.libc_has_fast_function (BUILT_IN_MEMPCPY) #define TARGET_LIBC_HAS_FAST_FUNCTION default_libc_has_fast_function
static bool libcHasFastFunction_cb(MtcsTarget *self,int fcode)
{
   return false;
}

//原型 targetm.fn_abi_va_list (cfun->decl) #define TARGET_FN_ABI_VA_LIST std_fn_abi_va_list
static tree fnAbiVaList_cb(MtcsTarget *self,tree fndecl ATTRIBUTE_UNUSED)
{
   return std_fn_abi_va_list(fndecl);
}

//原型 argetm.canonical_va_list_type (TREE_TYPE (valist)); #define TARGET_CANONICAL_VA_LIST_TYPE std_canonical_va_list_type
static tree canonicalVaListType_cb(MtcsTarget *self,tree type)
{
   return std_canonical_va_list_type(type);
}

//原型 targetm.speculation_safe_value #define TARGET_SPECULATION_SAFE_VALUE default_speculation_safe_value
static rtx speculationSafeValue_cb(MtcsTarget *self,machine_mode mode ATTRIBUTE_UNUSED,
      rtx result, rtx val,rtx failval ATTRIBUTE_UNUSED)
{
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(self);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(self);

   mtcs_expr_emit_move_insn(mtcsExpr,result, val);

 #ifdef HAVE_speculation_barrier
   /* Assume the target knows what it is doing: if it defines a
      speculation barrier, but it is not enabled, then assume that one
      isn't needed.  */
   if (HAVE_speculation_barrier)
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_speculation_barrier ());
 #endif

   return result;
}

//原型 targetm.optab_supported_p (optab, to_mode, from_mode, opt_type) #define TARGET_OPTAB_SUPPORTED_P default_optab_supported_p
static bool optabSupportedP_cb(MtcsTarget *self,int op, machine_mode mode1, machine_mode,optimization_type opt_type)
{
   return true;
}

//原型 targetm.atomic_align_for_mode (QImode)  #define TARGET_ATOMIC_ALIGN_FOR_MODE hook_uint_mode_0
static unsigned int atomicAlignForMode_cb(MtcsTarget *self,machine_mode mode)
{
   return 0;
}

//原型  targetm.build_builtin_va_list () #define TARGET_BUILD_BUILTIN_VA_LIST std_build_builtin_va_list
static tree buildBuiltinVaList_cb(MtcsTarget *self)
{
   MtcsTree *mtcsTree=mtcs_target_get_tree(self);
   return mtcsTree->global_trees[TI_PTR_TYPE]/*!ptr_type_node*/;
}

//原型  targetm.mangle_decl_assembler_name (decl, libname) #define TARGET_MANGLE_DECL_ASSEMBLER_NAME default_mangle_decl_assembler_name
static tree mangleDeclAssemblerName_cb(MtcsTarget *self,tree decl ATTRIBUTE_UNUSED,tree id)
{
   return id;
}

//原型 targetm.floatn_builtin_p ((int) ENUM) #define TARGET_FLOATN_BUILTIN_P default_floatn_builtin_p
static bool floatnBuiltinP_cb(MtcsTarget *self,int func)
{
   /*
   bool   default_floatn_builtin_p (int func ATTRIBUTE_UNUSED)
   {
     static bool first_time_p = true;
     static bool c_or_objective_c;

     if (first_time_p)
       {
         first_time_p = false;
         c_or_objective_c = lang_GNU_C () || lang_GNU_OBJC ();
       }

     return c_or_objective_c;
   }
   */
   return true;
}

//原型 targetm.member_type_forces_blk (type, VOIDmode) #define TARGET_MEMBER_TYPE_FORCES_BLK default_member_type_forces_blk
static bool memberTypeForcesBlk_cb(MtcsTarget *self,const_tree type, machine_mode mode)
{
   return false;
}

//原型 targetm.array_mode (elem_mode, num_elems) #define TARGET_ARRAY_MODE hook_optmode_mode_uhwi_none
static opt_machine_mode arrayMode_cb(MtcsTarget *self,machine_mode mode, unsigned HOST_WIDE_INT size)
{
   return opt_machine_mode();
}

//原型 targetm.array_mode_supported_p (elem_mode, num_elems) #define TARGET_ARRAY_MODE_SUPPORTED_P hook_bool_mode_uhwi_false
static bool arrayModeSupportedP_cb(MtcsTarget *self,machine_mode mode, unsigned HOST_WIDE_INT size)
{
   return false;
}

//原型 targetm.ms_bitfield_layout_p (DECL_FIELD_CONTEXT (decl)) #define TARGET_MS_BITFIELD_LAYOUT_P hook_bool_const_tree_false
static bool msBitfieldLayoutP_cb(MtcsTarget *self,const_tree t)
{
   return false;
}

//原型 targetm.vector_mode_supported_any_target_p (mode) #define TARGET_VECTOR_MODE_SUPPORTED_ANY_TARGET_P hook_bool_mode_true
static bool vectorModeSupportedAnyTargetP_cb(MtcsTarget *self,machine_mode mode)
{
   return true;
}

//原型  targetm.insert_attributes (*node, &attributes); #define TARGET_INSERT_ATTRIBUTES hook_void_tree_treeptr
static void insertAttributes_cb(MtcsTarget *self,tree fndecl, tree *attributes)
{

}

//原型 targetm.comp_type_attributes (type1, type2) #define TARGET_COMP_TYPE_ATTRIBUTES hook_int_const_tree_const_tree_1
static int compTypeAttributes_cb (MtcsTarget *self,const_tree type1, const_tree type2)
{
   return 1;
}

//原型targetm.static_rtx_alignment(align_mode); #define TARGET_STATIC_RTX_ALIGNMENT default_static_rtx_alignment
static HOST_WIDE_INT staticRtxAlignment_cb(MtcsTarget *self,machine_mode mode)
{
   MtcsMode *mtcsMode=self->mtcsMode;
   return mtcs_mode_get_alignment(mtcsMode,mode);
}

//原型 targetm.cannot_modify_jumps_p () #define TARGET_CANNOT_MODIFY_JUMPS_P hook_bool_void_false
static bool cannotModifyJumpsP_cb(MtcsTarget *self)
{
   return false;
}

//原型  targetm.instantiate_decls (); #define TARGET_INSTANTIATE_DECLS hook_void_void
static void instantiateDecls_cb(MtcsTarget *self)
{

}

//原型  targetm.lra_p () #define TARGET_LRA_P default_lra_p
static bool lraP_cb(MtcsTarget *self)
{
   return true;
}

//原型 targetm.can_follow_jump (insn, seq->insn (0)) #define TARGET_CAN_FOLLOW_JUMP hook_bool_const_rtx_insn_const_rtx_insn_true
static bool canFollowJump_cb (MtcsTarget *self,const rtx_insn *follower, const rtx_insn *followee)
{
   return true;
}

//原型 targetm.unspec_may_trap_p (x, flags); #define TARGET_UNSPEC_MAY_TRAP_P default_unspec_may_trap_p
static int  unspecMayTrapP_cb (MtcsTarget *self,const_rtx x, unsigned flags)
{
   MtcsTarget *mtcsTarget=self;
   MtcsMode *mtcsMode=self->mtcsMode;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   int i;

   /* Any floating arithmetic may trap.  */
   if ((mtcs_mode_is_scalar_float_p/*!SCALAR_FLOAT_MODE_P*/(mtcsMode,GET_MODE (x))
         && mtcsOptionsItem->x_flag_trapping_math))
      return 1;

   for (i = 0; i < XVECLEN (x, 0); ++i){
      if (mtcs_rtlanal_may_trap_p_1/*!may_trap_p_1*/(mtcsRtlanal,XVECEXP (x, 0, i), flags))
         return 1;
   }
   return 0;
}

//原型 targetm.extra_live_on_entry (entry_block_defs); #define TARGET_EXTRA_LIVE_ON_ENTRY hook_void_bitmap
static void extraLiveOnEntry_cb(MtcsTarget *self,bitmap regs ATTRIBUTE_UNUSED)
{

}

//原型 targetm.cc_modes_compatible (mode, set_mode); #define TARGET_CC_MODES_COMPATIBLE default_cc_modes_compatible
static machine_mode ccModesCompatible_cb (MtcsTarget *self,machine_mode m1, machine_mode m2)
{
  if (m1 == m2)
    return m1;
  return VOIDmode;
}

//原型 targetm.fixed_condition_code_regs (&cc_regno_1, &cc_regno_2) #define TARGET_FIXED_CONDITION_CODE_REGS hook_bool_uintp_uintp_false
static bool fixedConditionCodeRegs_cb (MtcsTarget *self,nuint *p1,nuint *p2)
{
   return false;
}

//原型targetm.address_cost (x, mode, as, speed); #define TARGET_ADDRESS_COST default_address_cost
static int addressCost_cb(MtcsTarget *self,rtx x, machine_mode mode ,addr_space_t as ,bool speed)
{
    MtcsMode *mtcsMode=self->mtcsMode;
    MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(self);
    return mtcs_rtlanal_rtx_cost/*!rtx_cost*/(mtcsRtlanal,x, mtcs_mode_get_Pmode(mtcsMode), MEM, 0, speed);
}

//原型 targetm.memory_move_cost (mode, rclass, in); #define TARGET_MEMORY_MOVE_COST default_memory_move_cost
static int memoryMoveCost_cb(MtcsTarget *self,machine_mode mode ATTRIBUTE_UNUSED,
           reg_class_t rclass ATTRIBUTE_UNUSED, bool in ATTRIBUTE_UNUSED)
{
   MtcsMode *mtcsMode=self->mtcsMode;
   MtcsConfig *mtcsConfig=mtcs_target_get_config(self);
   MtcsReload *mtcsReload=mtcs_target_get_reload(self);

   if(mtcs_config_ifndef(mtcsConfig,MTCS_MEMORY_MOVE_COST))
   //#ifndef MEMORY_MOVE_COST
      return (4 + mtcs_reload_memory_move_secondary_cost/*!memory_move_secondary_cost*/(mtcsReload,
            mode, (enum reg_class) rclass, in));
   //#else
   else
      n_error("MEMORY_MOVE_COST 未实现\n");//return MEMORY_MOVE_COST (MACRO_MODE (mode), (enum reg_class) rclass, in);
   //#endif
   return 0;
}

//原型 targetm.register_move_cost (mode, from, to); #define TARGET_REGISTER_MOVE_COST default_register_move_cost
static int registerMoveCost_cb(MtcsTarget *self,machine_mode mode ATTRIBUTE_UNUSED,
                            reg_class_t from ATTRIBUTE_UNUSED,reg_class_t to ATTRIBUTE_UNUSED)
{
   MtcsConfig *mtcsConfig=mtcs_target_get_config(self);

   if(mtcs_config_ifndef(mtcsConfig,MTCS_REGISTER_MOVE_COST))
   //#ifndef REGISTER_MOVE_COST
      return 2;
   //#else
   else
      n_error("REGISTER_MOVE_COST 未实现\n");//return REGISTER_MOVE_COST (MACRO_MODE (mode),(enum reg_class) from, (enum reg_class) to);
   //#endif
   return 2;
}

//原型 targetm.keep_leaf_when_profiled () #define TARGET_KEEP_LEAF_WHEN_PROFILED default_keep_leaf_when_profiled
static bool keepLeafWhenProfiled_cb(MtcsTarget *self)
{
   return false;
}

//原型 targetm.frame_pointer_required () #define TARGET_FRAME_POINTER_REQUIRED hook_bool_void_false
static bool framePointerRequired_cb(MtcsTarget *self)
{
   return false;
}

//原型  targetm.can_eliminate (eliminables[i].from, eliminables[i].to) #define TARGET_CAN_ELIMINATE hook_bool_const_int_const_int_true
static bool canEliminate_cb (MtcsTarget *self,const int from, const int to)
{
   return true;
}

//原型 targetm.init_pic_reg (); #define TARGET_INIT_PIC_REG hook_void_void
static void initPicReg_cb (MtcsTarget *self)
{

}

//原型 targetm.setjmp_preserves_nonvolatile_regs_p () #define TARGET_SETJMP_PRESERVES_NONVOLATILE_REGS_P hook_bool_void_false
static bool setjmpPreservesNonvolatileRegsP_cb(MtcsTarget *self)
{
   return false;
}

//原型 targetm.preferred_reload_class(x, rclass);#define TARGET_PREFERRED_RELOAD_CLASS default_preferred_reload_class
static reg_class_t preferredReloadClass_cb(MtcsTarget *self,rtx x, reg_class_t regclass)
{
#ifdef PREFERRED_RELOAD_CLASS
  return (reg_class_t) PREFERRED_RELOAD_CLASS (x, (enum reg_class) rclass);
#else
  return regclass;
#endif
}

//原型 targetm.ira_change_pseudo_allocno_class  #define TARGET_IRA_CHANGE_PSEUDO_ALLOCNO_CLASS default_ira_change_pseudo_allocno_class
static reg_class_t iraChangePseudoAllocnoClass_cb(MtcsTarget *self,int regno ATTRIBUTE_UNUSED,
                reg_class_t cl,reg_class_t best_cl ATTRIBUTE_UNUSED)
{
   return cl;
}

//原型 targetm.noce_conversion_profitable_p  #define TARGET_NOCE_CONVERSION_PROFITABLE_P default_noce_conversion_profitable_p
static bool noceConversionProfitableP_cb (MtcsTarget *self,rtx_insn *seq,  struct noce_if_info *if_info)
{
   return false;
}

//原型 targetm.max_noce_ifcvt_seq_cost (then_edge) #define TARGET_MAX_NOCE_IFCVT_SEQ_COST default_max_noce_ifcvt_seq_cost
//原型 default_max_noce_ifcvt_seq_cost targhooks.cc
static unsigned int maxNoceIfcvtSeqCost_cb (MtcsTarget *self,edge e)
{
   MtcsOptions *mtcsOptions=mtcs_target_get_options(self);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(self);

   bool predictable_p = predictable_edge_p (e);

   if (predictable_p){
      if (mtcsOptionsItem->x_param_max_rtl_if_conversion_predictable_cost)
         return mtcsOptionsItem->x_param_max_rtl_if_conversion_predictable_cost;
   }else{
      if (mtcsOptionsItem->x_param_max_rtl_if_conversion_unpredictable_cost)
         return mtcsOptionsItem->x_param_max_rtl_if_conversion_unpredictable_cost;
   }
   return mtcs_emit_branch_cost/*!BRANCH_COST*/(mtcsEmit,true, predictable_p) * COSTS_N_INSNS (3);
}

//原型 targetm.canonicalize_comparison  #define TARGET_CANONICALIZE_COMPARISON default_canonicalize_comparison
static void canonicalizeComparison_cb (MtcsTarget *self,int *code, rtx *op0, rtx *op1,bool op0_preserve_value)
{

}

//原型  targetm.legitimate_combined_insn (insn) #define TARGET_LEGITIMATE_COMBINED_INSN hook_bool_rtx_insn_true
static bool legitimateCombineInsn_cb(MtcsTarget *self,rtx_insn *insn)
{
   return true;
}

//原型 targetm.hard_regno_scratch_ok #define TARGET_HARD_REGNO_SCRATCH_OK default_hard_regno_scratch_ok
static bool hardRegnoScratchOk_cb (MtcsTarget *self,unsigned int regno)
{
    return true;
}

//原型 targetm.use_late_prologue_epilogue() #define TARGET_USE_LATE_PROLOGUE_EPILOGUE hook_bool_void_false
static bool useLatePrologueEpilogue_cb (MtcsTarget *self)
{
    return false;
}

//原型  targetm.dwarf_poly_indeterminate_value #define TARGET_DWARF_POLY_INDETERMINATE_VALUE default_dwarf_poly_indeterminate_value
static unsigned int dwarfPolyIndeterminateValue_cb(MtcsTarget *self,unsigned int i, unsigned int *factor,int *offset)
{
   gcc_unreachable ();
}

//原型 targetm.const_not_ok_for_debug_p #define TARGET_CONST_NOT_OK_FOR_DEBUG_P default_const_not_ok_for_debug_p
static bool constNotOkForDebugP_cb (MtcsTarget *self,rtx x)
{
   if (GET_CODE (x) == UNSPEC)
      return true;
   return false;
}

//原型targetm.dwarf_calling_convention #define TARGET_DWARF_CALLING_CONVENTION hook_int_const_tree_0
static int dwarfCallingConvention_cb(MtcsTarget *self,const_tree func)
{
   return 0;
}

//原型 targetm.shift_truncation_mask  #define TARGET_SHIFT_TRUNCATION_MASK default_shift_truncation_mask default_shift_truncation_mask
static unsigned HOST_WIDE_INT shiftTruncationMask_cb(MtcsTarget *self,machine_mode mode)
{
   MtcsMode *mtcsMode=self->mtcsMode;
   fprintf(stderr,"shift_truncation_mask SHIFT_COUNT_TRUNCATED:%d\n",SHIFT_COUNT_TRUNCATED);
   return SHIFT_COUNT_TRUNCATED ? mtcs_mode_get_unit_bitsize/*!GET_MODE_UNIT_BITSIZE*/(mtcsMode,mode) - 1 : 0;
}

//原型 targetm_common.supports_split_stack (true, opts)#define TARGET_SUPPORTS_SPLIT_STACK hook_bool_bool_gcc_optionsp_false
static bool supportsSplitStack_cb(MtcsTarget *self,bool split, MtcsOptionsItem *opts)
{
    return false;
}

//原型 targetm.dw_cfi_oprnd1_desc (cfi, oprnd_type) #define TARGET_DW_CFI_OPRND1_DESC hook_bool_dwcfi_dwcfioprndtyperef_false
static bool dwCfiOprand1Desc_cb(MtcsTarget *self,dwarf_call_frame_info,dw_cfi_oprnd_type &)
{
   return false;
}

//原型 targetm.fold_builtin #define TARGET_FOLD_BUILTIN hook_tree_tree_int_treep_bool_null
static tree foldBuiltin_cb(MtcsTarget *self,tree fndecl, int n_args,tree *args, bool ignore ATTRIBUTE_UNUSED)
{
   return NULL;
}

//原型 targetm.have_ccmp #define TARGET_HAVE_CCMP default_have_ccmp
static bool haveCcmp_cb(MtcsTarget *self)
{
   return self->gen_ccmp_first != NULL;
}

static void createComponent(MtcsTarget *self)
{
    MtcsMode *mtcsMode=self->mtcsMode;
    self->mtcsFinal=mtcs_final_new(mtcsMode);
    self->mtcsDwarf2Out=mtcs_dwarf2_out_new(mtcsMode);
    self->mtcsDwarf2Asm=mtcs_dwarf2_asm_new(mtcsMode);
    self->mtcsDwarf2Codeview=mtcs_dwarf2_codeview_new(mtcsMode);
    self->mtcsDwarf2Lineno=mtcs_dwarf2_lineno_new(mtcsMode);
    self->mtcsDoNothingDebug=mtcs_do_nothing_debug_new();
    self->currentDebug=(MtcsDebug *)self->mtcsDoNothingDebug;

    self->mtcsExcept=mtcs_except_new(mtcsMode);
    self->mtcsDwarf2Cfi=mtcs_dwarf2_cfi_new(mtcsMode);
    self->mtcsSimplifyRtx=mtcs_simplify_rtx_new(mtcsMode);
    self->mtcsExpr=mtcs_expr_new(mtcsMode);
    self->mtcsExplow=mtcs_explow_new(mtcsMode);
    self->mtcsRtlanal=mtcs_rtlanal_new(mtcsMode);
    self->mtcsOptabs=mtcs_optabs_new(mtcsMode);
    self->mtcsReload=mtcs_reload_new(mtcsMode);
    self->mtcsExpmed=mtcs_expmed_new(mtcsMode);
    self->mtcsDojump=mtcs_dojump_new(mtcsMode);
    self->mtcsLowerSubreg=mtcs_lower_subreg_new(mtcsMode);
    self->mtcsCfgLoopanal=mtcs_cfg_loopanal_new(mtcsMode);
    self->mtcsCalls=mtcs_calls_new(mtcsMode);
    self->mtcsCcmp=mtcs_ccmp_new(mtcsMode);
    self->mtcsCgraph=mtcs_cgraph_new(mtcsMode);
    self->mtcsFuncAbi=mtcs_func_abi_new(mtcsMode);
    self->mtcsLibfuncs=mtcs_libfuncs_new(mtcsMode);
    self->mtcsPassMgr=mtcs_pass_mgr_new(mtcsMode);
    self->mtcsOpts=mtcs_opts_new(mtcsMode);
    self->mtcsExpand=mtcs_expand_new(mtcsMode);
    //不再使用2025-10-05 self->mtcsIpaProp=(npointer)mtcs_ipa_prop_new(mtcsMode);
    self->mtcsVar=mtcs_var_new(mtcsMode);
    self->mtcsClones=mtcs_clones_new(mtcsMode);
    self->mtcsGimple=mtcs_gimple_new(mtcsMode);
    self->mtcsStmt=mtcs_stmt_new(mtcsMode);
    self->mtcsCfgRtl=mtcs_cfg_rtl_new(mtcsMode);
    self->mtcsTraverseTree=mtcs_traverse_tree_new(mtcsMode);
    self->mtcsLang=mtcs_lang_new(mtcsMode);
    self->mtcsStorLayout=mtcs_stor_layout_new(mtcsMode);
    self->mtcsCfgBuild=mtcs_cfg_build_new(mtcsMode);
    self->mtcsCfgCleanup=mtcs_cfg_cleanup_new(mtcsMode);
    self->mtcsCse=mtcs_cse_new(mtcsMode);
    self->mtcsCfgContext=mtcs_cfg_context_new(mtcsMode);
    self->mtcsCfg=mtcs_cfg_new(mtcsMode);
    self->mtcsConst=mtcs_const_new(mtcsMode);
    self->mtcsDfa=mtcs_dfa_new(mtcsMode);
    self->mtcsFixed=mtcs_fixed_new(mtcsMode);
    self->mtcsAddr=mtcs_addr_new(mtcsMode);
    self->mtcsDfscan=mtcs_dfscan_new(mtcsMode);
    self->mtcsDfcore=(void*)mtcs_dfcore_new(mtcsMode);
    self->mtcsDfproblems=(void*)mtcs_dfproblems_new(mtcsMode);
    self->mtcsRtlPassMgr=mtcs_rtl_pass_mgr_new(mtcsMode);
    self->mtcsAlias=mtcs_alias_new(mtcsMode);
    self->mtcsIraMgr = mtcs_ira_mgr_new(mtcsMode);//声明在 ira/mtcsiracommon.h
    self->mtcsReload1=mtcs_reload1_new(mtcsMode);
    self->mtcsDce=mtcs_dce_new(mtcsMode);
    self->mtcsCseLib = mtcs_cse_lib_new(mtcsMode);
    self->mtcsSsaPropagate = (void*)mtcs_ssa_propagate_new(mtcsMode);
    //self->mtcsSsaStrlen = mtcs_ssa_strlen_new(mtcsMode);不再使用2025-10-05
    //self->mtcsSsaSprintf = mtcs_ssa_sprintf_new(mtcsMode);不再使用2025-10-05
    self->mtcsOutofSsa = mtcs_outof_ssa_new(mtcsMode);
    self->mtcsCfgLoop = mtcs_cfg_loop_new(mtcsMode);
    self->mtcsLoopIv = mtcs_loop_iv_new(mtcsMode);
    self->mtcsCfgLoopManip = mtcs_cfg_loop_manip_new(mtcsMode);
    self->mtcsLoopinit = mtcs_loopinit_new(mtcsMode);
    self->mtcsPredict = mtcs_predict_new(mtcsMode);
    self->mtcsPort = mtcs_port_new(mtcsMode);
    self->mtcsGimpleExpr = mtcs_gimple_expr_new(mtcsMode);
    self->mtcsSsaAddress = mtcs_ssa_address_new(mtcsMode);
    self->mtcsSsaCoalesce =(void *) mtcs_ssa_coalesce_new(mtcsMode);
    self->mtcsMachine =(void *) mtcs_machine_new(mtcsMode);
}

void     mtcs_target_init(MtcsTarget *self)
{
    //TARGET_STRIP_NAME_ENCODING 缺省实现
    self->strip_name_encoding=stripNameEncoding_cb;
    //原型 targetm.constant_alignment (DECL_INITIAL (decl), align); #define TARGET_CONSTANT_ALIGNMENT default_constant_alignment
    self->constant_alignment=constanAlignment_cb;
    //原型 targetm.debug_unwind_info #define TARGET_DEBUG_UNWIND_INFO default_debug_unwind_info
    self->debug_unwind_info=debugUnwindInfo_cb;
    //原型 targetm.profile_before_prologue; TARGET_PROFILE_BEFORE_PROLOGUE default_profile_before_prologue
    self->profile_before_prologue=profileBeforePrologue_cb;
    //原型 targetm.have_conditional_execution #define TARGET_HAVE_CONDITIONAL_EXECUTION default_have_conditional_execution
    self->have_conditional_execution=haveConditionalExecution_cb;
    //原型targetm.binds_local_p (tem) #define TARGET_BINDS_LOCAL_P default_binds_local_p
    self->binds_local_p=bindsLocalP_cb;
    //原型targetm.in_small_data_p (decl) #define TARGET_IN_SMALL_DATA_P hook_bool_const_tree_false
    self->in_small_data_p=inSmallDataP_cb;
    //原型targetm.use_blocks_for_decl_p (decl);#define TARGET_USE_BLOCKS_FOR_DECL_P hook_bool_const_tree_true
    self->use_blocks_for_decl_p=useBlocksForDeclP_cb;
    //原型targetm.valid_pointer_mode (mode);#define TARGET_VALID_POINTER_MODE default_valid_pointer_mode
    self->valid_pointer_mode=validPointerMode_cb;
    //原型  targetm.expand_to_rtl_hook (); #define TARGET_EXPAND_TO_RTL_HOOK hook_void_void
    self->expand_to_rtl_hook=expandToRtlHook_cb;
     //原型 targetm.starting_frame_offset () #define TARGET_STARTING_FRAME_OFFSET hook_hwi_void_0
     self->starting_frame_offset=startingFrameOffset_cb;
     //原型 targetm.use_pseudo_pic_reg () #define TARGET_USE_PSEUDO_PIC_REG hook_bool_void_false
     self->use_pseudo_pic_reg=usePseudoPicReg_cb;
     //原型 targetm.dwarf_register_span (reg) #define TARGET_DWARF_REGISTER_SPAN hook_rtx_rtx_null
     self->dwarf_register_span=dwarfRegisterSpan_cb;
     //原型 targetm.dwarf_frame_reg_mode #define TARGET_DWARF_FRAME_REG_MODE default_dwarf_frame_reg_mode
     self->dwarf_frame_reg_mode=dwarfFrameRegMode_cb;
     //原型 targetm.init_dwarf_reg_sizes_extra (address); #define TARGET_INIT_DWARF_REG_SIZES_EXTRA hook_void_tree
     self->init_dwarf_reg_sizes_extra=initDwarfRegSizesExtra_cb;
     //原型targetm.legitimate_constant_p (mode, y)#define TARGET_LEGITIMATE_CONSTANT_P hook_bool_mode_rtx_true
     self->legitimate_constant_p=legitimateConstantP_cb;
     //原型 targetm.delegitimize_address (addr); #define TARGET_DELEGITIMIZE_ADDRESS delegitimize_mem_from_attrs
     self->delegitimize_address=delegitimizeAddress_cb;
     //原型 targetm.legitimize_address ; #define TARGET_LEGITIMIZE_ADDRESS default_legitimize_address
     self->legitimize_address=legitimateConstant_1_cb;
     //原型 targetm.libgcc_cmp_return_mode ();#define TARGET_LIBGCC_CMP_RETURN_MODE default_libgcc_cmp_return_mode
     self->libgcc_cmp_return_mode=libgccCmpReturnMode_cb;
     //原型targetm.insn_cost (insn, speed);#define TARGET_INSN_COST NULL //host=NULL nvptx=NULL;
     self->insn_cost=NULL;
     //原型 targetm.cstore_mode (icode);#define TARGET_CSTORE_MODE sparc_cstore_mode
     self->cstore_mode=cstoreMode_cb;
     //原型 targetm.mode_dependent_address_p (addr, addrspace); #define TARGET_MODE_DEPENDENT_ADDRESS_P default_mode_dependent_address_p
     self->mode_dependent_address_p=modeDependentAddressP_cb;
     //原型targetm.modes_tieable_p  #define TARGET_MODES_TIEABLE_P hook_bool_mode_mode_true
     self->modes_tieable_p=modesTieableP_cb;
     //原型targetm.rtx_costs  #define TARGET_RTX_COSTS aarch64_rtx_costs_wrapper
     self->rtx_costs=rtxCosts_cb;
     //原型targetm.use_by_pieces_infrastructure_p #define TARGET_USE_BY_PIECES_INFRASTRUCTURE_P default_use_by_pieces_infrastructure_p
     self->use_by_pieces_infrastructure_p=useByPiecesInfrastructureP_cb;
     //原型t targetm.narrow_volatile_bitfield () #define TARGET_NARROW_VOLATILE_BITFIELD hook_bool_void_false
     self->narrow_volatile_bitfield=narrowVolatileBitfield_cb;
     //原型 targetm.small_register_classes_for_mode_p (#define TARGET_SMALL_REGISTER_CLASSES_FOR_MODE_P hook_bool_mode_false
     self->small_register_classes_for_mode_p=smallRegisterClassesForModeP_cb;
     //原型 targetm.overlap_op_by_pieces_p  #define TARGET_OVERLAP_OP_BY_PIECES_P hook_bool_void_false
     self->overlap_op_by_pieces_p=overlapOpByPiecesP_cb;
     //原型 targetm.mode_rep_extended (imode, omode)  #define TARGET_MODE_REP_EXTENDED default_mode_rep_extended
     self->mode_rep_extended=modeRepExtended_cb;
     //原型 targetm.compare_by_pieces_branch_ratio (mode) #define TARGET_COMPARE_BY_PIECES_BRANCH_RATIO default_compare_by_pieces_branch_ratio
     self->compare_by_pieces_branch_ratio=compareByPiecesBranchRatio_cb;
     //原型 targetm.conditional_register_usage (); #define TARGET_CONDITIONAL_REGISTER_USAGE hook_void_void
     self->conditional_register_usage=conditionalRegisterUsage_cb;
     //原型 targetm.hard_regno_call_part_clobbered (m_id, regno, mode) #define TARGET_HARD_REGNO_CALL_PART_CLOBBERED hook_bool_uint_uint_mode_false
     self->hard_regno_call_part_clobbered=hardRegnoCallPartClobbered_cb;
     //原型 targetm.init_libfuncs ();#define TARGET_INIT_LIBFUNCS hook_void_void
     self->init_libfuncs=initLibfuncs_cb;
     //原型  targetm.default_short_enums ();#define TARGET_DEFAULT_SHORT_ENUMS hook_bool_void_false
     self->default_short_enums=defaultShortEnums_cb;
     //原型 targetm.override_options_after_change (); #define TARGET_OVERRIDE_OPTIONS_AFTER_CHANGE hook_void_void
     self->override_options_after_change=overrideOptionsAfterChange_cb;
     //原型 targetm.stack_protect_guard #define TARGET_STACK_PROTECT_GUARD default_stack_protect_guard
     self->stack_protect_guard=stackProtectGuard_cb;
     //原型 targetm.gen_ccmp_first #define TARGET_GEN_CCMP_FIRST NULL
     self->gen_ccmp_first=NULL;
     //原型 targetm.unwind_word_mode (), #define TARGET_UNWIND_WORD_MODE default_unwind_word_mode
     self->unwind_word_mode=unwindWordMode_cb;
     //原型 targetm.eh_return_filter_mode () #define TARGET_EH_RETURN_FILTER_MODE default_eh_return_filter_mode
     self->eh_return_filter_mode=ehReturnFilterMode_cb;
     //原型 targetm.expand_builtin_va_start #define TARGET_EXPAND_BUILTIN_VA_START NULL
     self->expand_builtin_va_start=NULL;
     //原型 targetm.libc_has_fast_function (BUILT_IN_MEMPCPY) #define TARGET_LIBC_HAS_FAST_FUNCTION default_libc_has_fast_function
     self->libc_has_fast_function=libcHasFastFunction_cb;
     //原型 targetm.fn_abi_va_list (cfun->decl) #define TARGET_FN_ABI_VA_LIST std_fn_abi_va_list
     self->fn_abi_va_list=fnAbiVaList_cb;
     //原型 argetm.canonical_va_list_type (TREE_TYPE (valist)); #define TARGET_CANONICAL_VA_LIST_TYPE std_canonical_va_list_type
     self->canonical_va_list_type=canonicalVaListType_cb;
     //原型 targetm.memmodel_check (val) #define TARGET_MEMMODEL_CHECK ix86_memmodel_check
     self->memmodel_check=NULL;
     //原型 targetm.speculation_safe_value #define TARGET_SPECULATION_SAFE_VALUE default_speculation_safe_value
     self->speculation_safe_value=speculationSafeValue_cb;
     //原型 targetm.optab_supported_p (optab, to_mode, from_mode, opt_type) #define TARGET_OPTAB_SUPPORTED_P default_optab_supported_p
     self->optab_supported_p=optabSupportedP_cb;
     //原型 targetm.atomic_align_for_mode (QImode)  #define TARGET_ATOMIC_ALIGN_FOR_MODE hook_uint_mode_0
     self->atomic_align_for_mode=atomicAlignForMode_cb;
     //原型  targetm.build_builtin_va_list () #define TARGET_BUILD_BUILTIN_VA_LIST std_build_builtin_va_list
     self->build_builtin_va_list=buildBuiltinVaList_cb;
     //原型  targetm.mangle_decl_assembler_name (decl, libname) #define TARGET_MANGLE_DECL_ASSEMBLER_NAME default_mangle_decl_assembler_name
     self->mangle_decl_assembler_name=mangleDeclAssemblerName_cb;
     //原型 targetm.floatn_builtin_p ((int) ENUM) #define TARGET_FLOATN_BUILTIN_P default_floatn_builtin_p
     self->floatn_builtin_p=floatnBuiltinP_cb;
     //原型 targetm.member_type_forces_blk (type, VOIDmode) #define TARGET_MEMBER_TYPE_FORCES_BLK default_member_type_forces_blk
     self->member_type_forces_blk=memberTypeForcesBlk_cb;
     //原型 targetm.array_mode (elem_mode, num_elems) #define TARGET_ARRAY_MODE hook_optmode_mode_uhwi_none
     self->array_mode=arrayMode_cb;
     //原型 targetm.array_mode_supported_p (elem_mode, num_elems) #define TARGET_ARRAY_MODE_SUPPORTED_P hook_bool_mode_uhwi_false
     self->array_mode_supported_p=arrayModeSupportedP_cb;
     //原型 targetm.ms_bitfield_layout_p (DECL_FIELD_CONTEXT (decl)) #define TARGET_MS_BITFIELD_LAYOUT_P hook_bool_const_tree_false
     self->ms_bitfield_layout_p=msBitfieldLayoutP_cb;
     //原型 targetm.vector_mode_supported_any_target_p (mode) #define TARGET_VECTOR_MODE_SUPPORTED_ANY_TARGET_P hook_bool_mode_true
     self->vector_mode_supported_any_target_p=vectorModeSupportedAnyTargetP_cb;
     //原型  targetm.insert_attributes (*node, &attributes); #define TARGET_INSERT_ATTRIBUTES hook_void_tree_treeptr
     self->insert_attributes=insertAttributes_cb;
     //原型 targetm.comp_type_attributes (type1, type2) #define TARGET_COMP_TYPE_ATTRIBUTES hook_int_const_tree_const_tree_1
     self->comp_type_attributes=compTypeAttributes_cb;
     //原型targetm.static_rtx_alignment(align_mode); #define TARGET_STATIC_RTX_ALIGNMENT default_static_rtx_alignment
     self->static_rtx_alignment=staticRtxAlignment_cb;
     //原型 targetm.cannot_modify_jumps_p () #define TARGET_CANNOT_MODIFY_JUMPS_P hook_bool_void_false
     self->cannot_modify_jumps_p=cannotModifyJumpsP_cb;
     //原型  targetm.instantiate_decls (); #define TARGET_INSTANTIATE_DECLS hook_void_void
     self->instantiate_decls=instantiateDecls_cb;
     //原型  targetm.lra_p () #define TARGET_LRA_P default_lra_p
     self->lra_p=lraP_cb;
     //原型 targetm.can_follow_jump (insn, seq->insn (0)) #define TARGET_CAN_FOLLOW_JUMP hook_bool_const_rtx_insn_const_rtx_insn_true
     self->can_follow_jump=canFollowJump_cb;
     //原型 targetm.unspec_may_trap_p (x, flags); #define TARGET_UNSPEC_MAY_TRAP_P default_unspec_may_trap_p
     self->unspec_may_trap_p=unspecMayTrapP_cb;
     //原型 targetm.extra_live_on_entry (entry_block_defs); #define TARGET_EXTRA_LIVE_ON_ENTRY hook_void_bitmap
     self->extra_live_on_entry=extraLiveOnEntry_cb;
     //原型 targetm.cc_modes_compatible (mode, set_mode); #define TARGET_CC_MODES_COMPATIBLE default_cc_modes_compatible
     self->cc_modes_compatible=ccModesCompatible_cb;
     //原型 targetm.fixed_condition_code_regs (&cc_regno_1, &cc_regno_2) #define TARGET_FIXED_CONDITION_CODE_REGS hook_bool_uintp_uintp_false
     self->fixed_condition_code_regs=fixedConditionCodeRegs_cb;
     //原型targetm.address_cost (x, mode, as, speed); #define TARGET_ADDRESS_COST default_address_cost
     self->address_cost=addressCost_cb;
     //原型 targetm.memory_move_cost (mode, rclass, in); #define TARGET_MEMORY_MOVE_COST default_memory_move_cost
     self->memory_move_cost=memoryMoveCost_cb;
     //原型 targetm.register_move_cost (mode, from, to); #define TARGET_REGISTER_MOVE_COST default_register_move_cost
     self->register_move_cost=registerMoveCost_cb;
     //原型 #define TARGET_COMPUTE_PRESSURE_CLASSES NULL targetm.compute_pressure_classes
     self->compute_pressure_classes =NULL;
     //原型 targetm.keep_leaf_when_profiled () #define TARGET_KEEP_LEAF_WHEN_PROFILED default_keep_leaf_when_profiled
     self->keep_leaf_when_profiled=keepLeafWhenProfiled_cb;
     //原型 targetm.frame_pointer_required () #define TARGET_FRAME_POINTER_REQUIRED hook_bool_void_false
     self->frame_pointer_required=framePointerRequired_cb;
     //原型  targetm.can_eliminate (eliminables[i].from, eliminables[i].to) #define TARGET_CAN_ELIMINATE hook_bool_const_int_const_int_true
     self->can_eliminate=canEliminate_cb;
     //原型 targetm.allocate_initial_value #define TARGET_ALLOCATE_INITIAL_VALUE NULL
     self->allocate_initial_value=NULL;
     //原型 targetm.init_pic_reg (); #define TARGET_INIT_PIC_REG hook_void_void
     self->init_pic_reg=initPicReg_cb;
     //原型 targetm.setjmp_preserves_nonvolatile_regs_p () #define TARGET_SETJMP_PRESERVES_NONVOLATILE_REGS_P hook_bool_void_false
     self->setjmp_preserves_nonvolatile_regs_p=setjmpPreservesNonvolatileRegsP_cb;
     //原型 targetm.preferred_reload_class(x, rclass);#define TARGET_PREFERRED_RELOAD_CLASS default_preferred_reload_class
     self->preferred_reload_class=preferredReloadClass_cb;
     //原型 targetm.ira_change_pseudo_allocno_class  #define TARGET_IRA_CHANGE_PSEUDO_ALLOCNO_CLASS default_ira_change_pseudo_allocno_class
     self->ira_change_pseudo_allocno_class=iraChangePseudoAllocnoClass_cb;
     //原型 targetm.noce_conversion_profitable_p  #define TARGET_NOCE_CONVERSION_PROFITABLE_P default_noce_conversion_profitable_p
     self->noce_conversion_profitable_p=noceConversionProfitableP_cb;
     //原型 targetm.max_noce_ifcvt_seq_cost (then_edge) #define TARGET_MAX_NOCE_IFCVT_SEQ_COST default_max_noce_ifcvt_seq_cost
     self->max_noce_ifcvt_seq_cost=maxNoceIfcvtSeqCost_cb;
     //原型 targetm.canonicalize_comparison  #define TARGET_CANONICALIZE_COMPARISON default_canonicalize_comparison
     self->canonicalize_comparison=canonicalizeComparison_cb;
     //原型  targetm.legitimate_combined_insn (insn) #define TARGET_LEGITIMATE_COMBINED_INSN hook_bool_rtx_insn_true
     self->legitimate_combined_insn=legitimateCombineInsn_cb;
     //原型 targetm.hard_regno_scratch_ok #define TARGET_HARD_REGNO_SCRATCH_OK default_hard_regno_scratch_ok
     self->hard_regno_scratch_ok=hardRegnoScratchOk_cb;
     //原型 targetm.use_late_prologue_epilogue() #define TARGET_USE_LATE_PROLOGUE_EPILOGUE hook_bool_void_false
     self->use_late_prologue_epilogue=useLatePrologueEpilogue_cb;
     //原型  targetm.dwarf_poly_indeterminate_value #define TARGET_DWARF_POLY_INDETERMINATE_VALUE default_dwarf_poly_indeterminate_value
     self->dwarf_poly_indeterminate_value=dwarfPolyIndeterminateValue_cb;
     //原型 targetm.const_not_ok_for_debug_p #define TARGET_CONST_NOT_OK_FOR_DEBUG_P default_const_not_ok_for_debug_p
     self->const_not_ok_for_debug_p=constNotOkForDebugP_cb;
     //原型targetm.dwarf_calling_convention #define TARGET_DWARF_CALLING_CONVENTION hook_int_const_tree_0
     self->dwarf_calling_convention=dwarfCallingConvention_cb;
     //原型 targetm.shift_truncation_mask  #define TARGET_SHIFT_TRUNCATION_MASK default_shift_truncation_mask default_shift_truncation_mask
     self->shift_truncation_mask=shiftTruncationMask_cb;
     //原型 targetm.loop_unroll_adjust (nunroll, loop); #define TARGET_LOOP_UNROLL_ADJUST NULL
     self->loop_unroll_adjust=NULL;
     //原型 targetm.expand_divmod_libfunc (libfunc, mode, op0, op1, &quotient, &remainder); #define TARGET_EXPAND_DIVMOD_LIBFUNC NULL
     self->expand_divmod_libfunc=NULL;
     //原型 targetm.fold_builtin #define TARGET_FOLD_BUILTIN hook_tree_tree_int_treep_bool_null
     self->fold_builtin=foldBuiltin_cb;
     //原型 #define TARGET_RESET_LOCATION_VIEW NULL
     self->reset_location_view=NULL;
     //原型 targetm.have_ccmp #define TARGET_HAVE_CCMP default_have_ccmp
     self->have_ccmp=haveCcmp_cb;

    //原型 targetm.arm_eabi_unwinder #define TARGET_ARM_EABI_UNWINDER false
    self->arm_eabi_unwinder=false;
    self->open_paren="(";
    self->close_paren=")";
    self->have_srodata_section=false;
    self->have_switchable_bss_sections=false;
    self->have_tls=false;
    //原型  targetm.libfunc_gnu_prefix #define TARGET_LIBFUNC_GNU_PREFIX false
    self->libfunc_gnu_prefix=false;//host=false nvptx=false;
    ////原型 TARGET_SUPPORTS_ALIASES defaults.h nvptx.h
    self->target_supports_aliases=0;
    //原型 targetm.have_ctors_dtors #define TARGET_HAVE_CTORS_DTORS false
    self->have_ctors_dtors=false;
    //原型 targetm.dtors_from_cxa_atexit;#define TARGET_DTORS_FROM_CXA_ATEXIT false
    self->dtors_from_cxa_atexit=false;
    //原型 targetm.atomic_test_and_set_trueval != 1 #define TARGET_ATOMIC_TEST_AND_SET_TRUEVAL 1
    self->atomic_test_and_set_trueval=1;
    //原型  targetm.flags_regnum #define TARGET_FLAGS_REGNUM INVALID_REGNUM
    self->flags_regnum = INVALID_REGNUM;
    //原型 targetm.dw_cfi_oprnd1_desc (cfi, oprnd_type) #define TARGET_DW_CFI_OPRND1_DESC hook_bool_dwcfi_dwcfioprndtyperef_false
    self->dw_cfi_oprnd1_desc=dwCfiOprand1Desc_cb;
    //原型 targetm.const_anchor #define TARGET_CONST_ANCHOR 0
    self->const_anchor = 0;
    //原型  #define TARGET_NO_REGISTER_ALLOCATION true nvptx=true
    self->no_register_allocation = false;
    //原型 targetm.want_debug_pub_sections #define TARGET_WANT_DEBUG_PUB_SECTIONS false
    self->want_debug_pub_sections = false;
    //原型 targetm.have_shadow_call_stack #define TARGET_HAVE_SHADOW_CALL_STACK false
    self->have_shadow_call_stack =false;

    createComponent(self);
    //原型 general_init toplev.cc
    //self->symtab= new (ggc_alloc_no_dtor<symbol_table> ()) symbol_table ();
    self->symtab = new (ggc_alloc<symbol_table> ()) symbol_table ();
}

void    mtcs_target_set_version(MtcsTarget *self,int version)
{
    self->platformInfo.version=version;
}

int   mtcs_target_get_version(MtcsTarget *self)
{
    return self->platformInfo.version;
}

void    mtcs_target_set_isa(MtcsTarget *self,int isa)
{
    self->platformInfo.isa=isa;
}

int   mtcs_target_get_isa(MtcsTarget *self)
{
    return self->platformInfo.isa;
}

/**
 * 目标名字如cuda、gcn、spirv、opencl等
 */
void        mtcs_target_set_platform_name(MtcsTarget *self,char *name)
{
   if(self->platformInfo.name)
      return;
   self->platformInfo.name=n_strdup(name);
}

const char* mtcs_target_get_platform_name(MtcsTarget *self)
{
   return self->platformInfo.name;
}

//原型 TARGET_SUPPORTS_ALIASES defaults.h nvptx.h
void   mtcs_target_set_supports_aliases(MtcsTarget *self,int supports)
{
    self->target_supports_aliases=supports;
}

MtcsReg    *mtcs_target_get_reg(MtcsTarget *self)
{
    return self->mtcsReg;
}

MtcsMode    *mtcs_target_get_mode(MtcsTarget *self)
{
    return self->mtcsMode;
}

MtcsRTL    *mtcs_target_get_rtl(MtcsTarget *self)
{
    return self->mtcsRTL;
}

MtcsRecog  *mtcs_target_get_recog(MtcsTarget *self)
{
    return self->mtcsRecog;
}

MtcsPreds  *mtcs_target_get_preds(MtcsTarget *self)
{
    return self->mtcsPreds;
}

MtcsAlign  *mtcs_target_get_align(MtcsTarget *self)
{
    return self->mtcsAlign;
}

MtcsFunc   *mtcs_target_get_func(MtcsTarget *self)
{
    return self->mtcsFunc;
}

MtcsOpinit   *mtcs_target_get_opinit(MtcsTarget *self)
{
    return self->mtcsOpinit;
}

MtcsEmit   *mtcs_target_get_emit(MtcsTarget *self)
{
    return self->mtcsEmit;
}

MtcsAsm *mtcs_target_get_asm(MtcsTarget *self)
{
    return self->mtcsAsm;
}

MtcsSimplifyRtx *mtcs_target_get_simplify_rtx(MtcsTarget *self)
{
    return self->mtcsSimplifyRtx;
}

MtcsExpr *mtcs_target_get_expr(MtcsTarget *self)
{
    return self->mtcsExpr;
}

MtcsExplow *mtcs_target_get_explow(MtcsTarget *self)
{
    return self->mtcsExplow;
}

MtcsReal  *mtcs_target_get_real(MtcsTarget *self)
{
    return self->mtcsReal;
}

MtcsOutput *mtcs_target_get_output(MtcsTarget *self)
{
    return self->mtcsOutput;
}

MtcsArgs  *mtcs_target_get_args(MtcsTarget *self)
{
    return self->mtcsArgs;
}

MtcsOptions  *mtcs_target_get_options(MtcsTarget *self)
{
    return self->mtcsOptions;
}

MtcsRtlanal  *mtcs_target_get_rtlanal(MtcsTarget *self)
{
    return self->mtcsRtlanal;
}

MtcsOptabs  *mtcs_target_get_optabs(MtcsTarget *self)
{
    return self->mtcsOptabs;
}

MtcsReload  *mtcs_target_get_reload(MtcsTarget *self)
{
    return self->mtcsReload;
}

MtcsExpmed  *mtcs_target_get_expmed(MtcsTarget *self)
{
    return self->mtcsExpmed;
}

MtcsDojump  *mtcs_target_get_dojump(MtcsTarget *self)
{
    return self->mtcsDojump;
}

MtcsFinal  *mtcs_target_get_final(MtcsTarget *self)
{
    return self->mtcsFinal;
}

MtcsDebug *mtcs_target_get_debug(MtcsTarget *self)
{
    return self->currentDebug;
}

MtcsDwarf2Out *mtcs_target_get_dwarf2_out(MtcsTarget *self)
{
    return self->mtcsDwarf2Out;
}

MtcsDwarf2Codeview *mtcs_target_get_dwarf2_codeview(MtcsTarget *self)
{
    return self->mtcsDwarf2Codeview;
}

MtcsDwarf2Lineno *mtcs_target_get_dwarf2_lineno (MtcsTarget *self)
{
   return self->mtcsDwarf2Lineno;
}
MtcsDoNothingDebug *mtcs_target_get_do_nothing_debug (MtcsTarget *self)
{
   return self->mtcsDoNothingDebug;
}

//设置工作的的debug
void mtcs_target_set_current_debug (MtcsTarget *self,MtcsDebug *debug)
{
   self->currentDebug = debug;
}


MtcsExcept *mtcs_target_get_except(MtcsTarget *self)
{
    return self->mtcsExcept;
}

MtcsDwarf2Asm *mtcs_target_get_dwarf2_asm(MtcsTarget *self)
{
    return self->mtcsDwarf2Asm;
}

MtcsDwarf2Cfi *mtcs_target_get_dwarf2_cfi(MtcsTarget *self)
{
    return self->mtcsDwarf2Cfi;
}

MtcsCodes *mtcs_target_get_codes(MtcsTarget *self)
{
    return self->mtcsCodes;
}

MtcsLowerSubreg *mtcs_target_get_lower_subreg(MtcsTarget *self)
{
    return self->mtcsLowerSubreg;
}

MtcsCfgLoopanal *mtcs_target_get_cfg_loopanal(MtcsTarget *self)
{
    return self->mtcsCfgLoopanal;
}

MtcsCalls   *mtcs_target_get_calls(MtcsTarget *self)
{
    return self->mtcsCalls;
}

MtcsCcmp   *mtcs_target_get_ccmp(MtcsTarget *self)
{
    return self->mtcsCcmp;
}


MtcsCgraph   *mtcs_target_get_cgraph(MtcsTarget *self)
{
    return self->mtcsCgraph;
}

MtcsFuncAbi   *mtcs_target_get_func_abi(MtcsTarget *self)
{
    return self->mtcsFuncAbi;
}

MtcsLibfuncs   *mtcs_target_get_libfuncs(MtcsTarget *self)
{
    return self->mtcsLibfuncs;
}

MtcsPassMgr   *mtcs_target_get_pass_mgr(MtcsTarget *self)
{
    return self->mtcsPassMgr;
}

MtcsPass *mtcs_target_get_pass(MtcsTarget *self,enum opt_pass_type type,char *name)
{
    return mtcs_pass_mgr_get_pass(self->mtcsPassMgr,type,name);
}


MtcsConfig *mtcs_target_get_config(MtcsTarget *self)
{
    return self->mtcsConfig;
}



MtcsOpts *mtcs_target_get_opts(MtcsTarget *self)
{
    return self->mtcsOpts;
}

MtcsExpand *mtcs_target_get_expand(MtcsTarget *self)
{
    return self->mtcsExpand;
}

MtcsVar *mtcs_target_get_var(MtcsTarget *self)
{
    return self->mtcsVar;
}

MtcsClones *mtcs_target_get_clones(MtcsTarget *self)
{
    return self->mtcsClones;
}

/**
 * 通过mtcsgimple获取gimple组件
 */
//不再使用2025-10-05
//MtcsTreeCfg *mtcs_target_get_tree_cfg(MtcsTarget *self)
//{
//    return mtcs_gimple_get_tree_cfg(self->mtcsGimple);
//}

//不再使用2025-10-05
//MtcsTreeEh *mtcs_target_get_tree_eh(MtcsTarget *self)
//{
//    return mtcs_gimple_get_tree_eh(self->mtcsGimple);
//}

MtcsGimple *mtcs_target_get_gimple(MtcsTarget *self)
{
    return self->mtcsGimple;
}

MtcsBuiltins *mtcs_target_get_builtins(MtcsTarget *self)
{
    return self->mtcsBuiltins;
}

MtcsStmt *mtcs_target_get_stmt(MtcsTarget *self)
{
    return self->mtcsStmt;
}

MtcsCfgRtl *mtcs_target_get_cfg_rtl(MtcsTarget *self)
{
    return self->mtcsCfgRtl;
}

MtcsTraverseTree *mtcs_target_get_traverse_tree(MtcsTarget *self)
{
    return self->mtcsTraverseTree;
}

MtcsTree *mtcs_target_get_tree(MtcsTarget *self)
{
   return self->mtcsTree;
}

MtcsLang *mtcs_target_get_lang(MtcsTarget *self)
{
   return self->mtcsLang;
}

MtcsStorLayout *mtcs_target_get_stor_layout(MtcsTarget *self)
{
   return self->mtcsStorLayout;
}

MtcsAttribs *mtcs_target_get_attribs(MtcsTarget *self)
{
   return self->mtcsAttribs;
}

MtcsCfgBuild *mtcs_target_get_cfg_build(MtcsTarget *self)
{
   return self->mtcsCfgBuild;
}

MtcsCfgCleanup *mtcs_target_get_cfg_cleanup(MtcsTarget *self)
{
   return self->mtcsCfgCleanup;
}

MtcsCse *mtcs_target_get_cse(MtcsTarget *self)
{
   return self->mtcsCse;
}

MtcsCfgContext *mtcs_target_get_cfg_context(MtcsTarget *self)
{
   return self->mtcsCfgContext;

}

MtcsCfg *mtcs_target_get_cfg(MtcsTarget *self)
{
   return self->mtcsCfg;
}

MtcsConst *mtcs_target_get_const(MtcsTarget *self)
{
   return self->mtcsConst;
}

MtcsDfa *mtcs_target_get_dfa(MtcsTarget *self)
{
   return self->mtcsDfa;
}

MtcsFixed *mtcs_target_get_fixed(MtcsTarget *self)
{
   return self->mtcsFixed;
}

MtcsAddr *mtcs_target_get_addr(MtcsTarget *self)
{
   return self->mtcsAddr;
}

MtcsUnspec *mtcs_target_get_unspec(MtcsTarget *self)
{
   return self->mtcsUnspec;
}

MtcsInsnAttr *mtcs_target_get_insn_attr(MtcsTarget *self)
{
   return self->mtcsInsnAttr;
}

MtcsDfscan *mtcs_target_get_dfscan(MtcsTarget *self)
{
   return self->mtcsDfscan;
}

/*引入mtcsdfproblems.h 报错 ../../gcc-14/gcc/df.h:622:55: error: invalid use of incomplete type ‘struct basic_block_def’
MtcsDfproblems  *mtcs_target_get_dfproblems(MtcsTarget *self)
{
   return self->mtcsDfproblems;
}
*/

/*引入mtcsdfcore.h 报错 ../../gcc-14/gcc/df.h:622:55: error: invalid use of incomplete type ‘struct basic_block_def’
MtcsDfcore *mtcs_target_get_dfcore(MtcsTarget *self)
{
   return self->mtcsDfcore;
}
*/

MtcsRtlPassMgr *mtcs_target_get_rtl_pass_mgr(MtcsTarget *self)
{
   return self->mtcsRtlPassMgr;
}

MtcsAlias   *mtcs_target_get_alias(MtcsTarget *self)
{
   return self->mtcsAlias;
}

MtcsIraMgr *mtcs_target_get_ira_mgr(MtcsTarget *self)
{
  return self->mtcsIraMgr;
}

MtcsReload1 *mtcs_target_get_reload1(MtcsTarget *self)
{
  return self->mtcsReload1;
}

MtcsDce *mtcs_target_get_dce(MtcsTarget *self)
{
   return self->mtcsDce;
}

MtcsCseLib *mtcs_target_get_cse_lib(MtcsTarget *self)
{
   return self->mtcsCseLib;
}

//不再使用 2025-10-05
//MtcsSsaStrlen *mtcs_target_get_ssa_strlen(MtcsTarget *self)
//{
//   return self->mtcsSsaStrlen;
//}

//不再使用2025-10-05
//MtcsSsaSprintf *mtcs_target_get_ssa_sprintf(MtcsTarget *self)
//{
//   return self->mtcsSsaSprintf;
//}

MtcsOutofSsa *mtcs_target_get_outof_ssa(MtcsTarget *self)
{
   return self->mtcsOutofSsa;
}

MtcsCfgLoop *mtcs_target_get_cfg_loop(MtcsTarget *self)
{
   return self->mtcsCfgLoop;
}

MtcsLoopIv  *mtcs_target_get_loop_iv(MtcsTarget *self)
{
   return self->mtcsLoopIv;
}

MtcsCfgLoopManip  *mtcs_target_get_cfg_loop_manip(MtcsTarget *self)
{
   return self->mtcsCfgLoopManip;
}

MtcsLoopinit  *mtcs_target_get_loopinit(MtcsTarget *self)
{
   return self->mtcsLoopinit;
}

MtcsPredict  *mtcs_target_get_predict(MtcsTarget *self)
{
   return self->mtcsPredict;
}

MtcsInternalFn  *mtcs_target_get_internal_fn(MtcsTarget *self)
{
   return self->mtcsInternalFn;
}

MtcsPort  *mtcs_target_get_port(MtcsTarget *self)
{
   return self->mtcsPort;
}

MtcsGimpleExpr  *mtcs_target_get_gimple_expr(MtcsTarget *self)
{
   return self->mtcsGimpleExpr;
}

MtcsSsaAddress  *mtcs_target_get_ssa_address(MtcsTarget *self)
{
   return self->mtcsSsaAddress;
}

MtcsMachine  *mtcs_target_get_machine(MtcsTarget *self)
{
   return self->mtcsMachine;
}


//原型 target_default_pointer_address_modes_p target.h targhooks.cc
bool mtcs_target_target_default_pointer_address_modes_p (MtcsTarget *self)
{
   //host nvptx都是指向 default todo
//   if (targetm.addr_space.address_mode != default_addr_space_address_mode)
//     return false;
//   if (targetm.addr_space.pointer_mode != default_addr_space_pointer_mode)
//     return false;

   return true;
}

/**
 * 获取需要链接的函数名
 */
char *mtcs_target_get_link_funcname(MtcsTarget *self)
{
   if(self->getLinkFuncName)
      return self->getLinkFuncName(self);
   return NULL;
}

//由于同一个平台有多个版本号，往往这些版本号是全局的，所以在编译前用每个target的版本号设为全局变量。
void mtcs_target_publish_version(MtcsTarget *self)
{
   if(self->publishVersion)
      self->publishVersion(self);
}
