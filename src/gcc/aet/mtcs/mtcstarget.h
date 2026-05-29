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

#ifndef __GCC_MTCS_TARGET__
#define __GCC_MTCS_TARGET__

#include "../nlib.h"
#include "mtcsreg.h"
#include "mtcsmode.h"
#include "mtcsrtl.h"
#include "mtcsoptions.h"
#include "mtcsrtldata.h"
#include "mtcsfunc.h"
#include "mtcspreds.h"
#include "mtcssimplifyrtx.h"
#include "mtcsemit.h"
#include "mtcsexpr.h"
#include "mtcsexplow.h"
#include "mtcsreal.h"
#include "mtcsoutput.h"
#include "mtcsargs.h"
#include "mtcsrecog.h"
#include "mtcsrtlanal.h"
#include "mtcsoptabs.h"
#include "mtcsreload.h"
#include "mtcsexpmed.h"
#include "mtcsrtlanal.h"

#include "mtcsdojump.h"
#include "mtcsasm.h"
#include "mtcsfinal.h"
#include "mtcsdebug.h"
#include "mtcsdwarf2codeview.h"
#include "mtcsdwarf2out.h"
#include "mtcsdwarf2cfi.h"
#include "mtcsdwarf2asm.h"
#include "mtcsdonothingdebug.h"
#include "mtcsdwarf2lineno.h"

#include "mtcsexcept.h"
#include "mtcsopinit.h"
#include "mtcsalign.h"
#include "mtcscodes.h"
#include "mtcslowersubreg.h"
#include "mtcscfgloopanal.h"
#include "mtcscalls.h"
#include "mtcsccmp.h"
#include "mtcscgraph.h"
#include "mtcsfuncabi.h"
#include "mtcslibfuncs.h"
#include "mtcspassmgr.h"
#include "mtcsconfig.h"
#include "mtcsopts.h"
#include "mtcsoptionsitem.h"
#include "mtcsexpand.h"
#include "mtcsvar.h"
#include "mtcsclones.h"
#include "mtcsgimple.h"
#include "mtcsbuiltins.h"
#include "mtcsstmt.h"
#include "mtcscfgrtl.h"
#include "mtcstraversetree.h"
#include "mtcstree.h"
#include "mtcslang.h"
#include "mtcsstorlayout.h"
#include "mtcsattribs.h"
#include "mtcscfgbuild.h"
#include "mtcscfgcleanup.h"
#include "mtcscse.h"
#include "mtcscfgcontext.h"
#include "mtcscfg.h"
#include "mtcsconst.h"
#include "mtcsdfa.h"
#include "mtcsfixed.h"
#include "mtcsaddr.h"
#include "mtcsunspec.h"
#include "mtcsinsnattr.h"
#include "mtcsdfscan.h"
#include "mtcscselib.h"
#include "mtcsoutofssa.h"

//引入mtcsdfcore.h 报错 ../../gcc-14/gcc/df.h:622:55: error: invalid use of incomplete type ‘struct basic_block_def’
//#include "mtcsdfcore.h"
//#include "mtcsdfproblems.h"
#include "mtcsalias.h"
#include "rtl/mtcsrtlpassmgr.h"
#include "rtl/mtcsiracommon.h"
#include "mtcsreload1.h"
#include "mtcsdce.h"
#include "mtcscfgloop.h"
#include "mtcsloopiv.h"
#include "mtcscfgloopmanip.h"
#include "rtl/mtcsloopinit.h"
#include "mtcspredict.h"
#include "mtcsinternalfn.h"
#include "mtcsport.h"
#include "mtcsgimpleexpr.h"
#include "mtcsssaaddress.h"
#include "machine/mtcsmachine.h"

/**
 * 引入#include "ipa/mtcsipaprop.h"
 * error: ‘ipa_edge_args_sum_t’ does not name a type; did you mean ‘gt_pch_nx_ipa_edge_args_sum_t’?
   36 |      ipa_edge_args_sum_t *ipa_edge_args_sum;
 *mtcsIpaProp 设为类型 npointer
 * 原型 targetm.section_type_flags  #define TARGET_SECTION_TYPE_FLAGS default_section_type_flags
 * 尝试用 mtcs_output_section_type_flags 替换
 * unsigned int (*section_type_flags) (MtcsTarget *self,tree decl, const char *name, int reloc);
 * 原型  targetm.encode_section_info (exp, rtl, true); #define TARGET_ENCODE_SECTION_INFO default_encode_section_info nvptx有实现
 * 尝试用 mtcs_output_encode_section_info 替换
 * void (*encode_section_info) (MtcsTarget *self,tree decl, rtx rtl, int first ATTRIBUTE_UNUSED);
 */
typedef struct _MtcsTarget MtcsTarget;
struct _MtcsTarget
{
   //原型 targetm.gen_ccmp_first #define TARGET_GEN_CCMP_FIRST NULL
   rtx (*gen_ccmp_first)(MtcsTarget *self,rtx_insn **prep_seq, rtx_insn **gen_seq,rtx_code code, tree treeop0, tree treeop1);
   //原型 targetm.gen_ccmp_next #define TARGET_GEN_CCMP_NEXT NULL
   rtx (* gen_ccmp_next)(MtcsTarget *self,rtx_insn **prep_seq, rtx_insn **gen_seq, rtx prev,
               rtx_code cmp_code, tree treeop0, tree treeop1,rtx_code bit_code);
   //TARGET_STRIP_NAME_ENCODING 和 targetm.strip_name_encoding (XSTR (fun, 0)) 有缺省实现
   const char * (*strip_name_encoding)(MtcsTarget *self,const char *name);
   //原型 targetm.debug_unwind_info #define TARGET_DEBUG_UNWIND_INFO default_debug_unwind_info
   enum unwind_info_type (*debug_unwind_info)(MtcsTarget *self);
   //原型 #define FUNCTION_PROFILER(FILE, LABELNO)
   void (*function_profiler) (MtcsTarget *self,int labelno);
   //原型 targetm.profile_before_prologue; TARGET_PROFILE_BEFORE_PROLOGUE default_profile_before_prologue
   bool (*profile_before_prologue)(MtcsTarget *self);
   //#define DATA_ALIGNMENT nvptx_data_alignment
   unsigned int (*data_alignment)(MtcsTarget *self,const_tree type, unsigned int basic_align);
   //原型 targetm.constant_alignment (DECL_INITIAL (decl), align); #define TARGET_CONSTANT_ALIGNMENT default_constant_alignment
   HOST_WIDE_INT (*constant_alignment)(MtcsTarget *self,const_tree, HOST_WIDE_INT align);
   //原型ttargetm.slow_unaligned_access (DECL_MODE (decl),DECL_ALIGN (decl)) #define TARGET_SLOW_UNALIGNED_ACCESS default_slow_unaligned_access
   bool (*slow_unaligned_access)(MtcsTarget *self,machine_mode, unsigned int);
   //原型targetm.binds_local_p (tem) #define TARGET_BINDS_LOCAL_P default_binds_local_p
   bool (*binds_local_p)(MtcsTarget *self,const_tree exp);
   //原型targetm.in_small_data_p (decl) #define TARGET_IN_SMALL_DATA_P hook_bool_const_tree_false
   bool (*in_small_data_p)(MtcsTarget *self,const_tree decl);
   //原型targetm.use_blocks_for_decl_p (decl);#define TARGET_USE_BLOCKS_FOR_DECL_P hook_bool_const_tree_true
   bool (*use_blocks_for_decl_p)(MtcsTarget *self,const_tree decl);
   //原型targetm.valid_pointer_mode (mode);#define TARGET_VALID_POINTER_MODE default_valid_pointer_mode
   bool (*valid_pointer_mode)(MtcsTarget *self,scalar_int_mode mode);
   //原型 #define ASM_DECLARE_OBJECT_NAME(FILE, NAME, DECL)  nvptx_declare_object_name (FILE, NAME, DECL)
   void (*declare_object_name)(MtcsTarget *self, const char *name, const_tree decl);
   //原型 targetm.hard_regno_nregs (i, (machine_mode) j); #define TARGET_HARD_REGNO_NREGS nvptx_hard_regno_nregs
   unsigned int (*hard_regno_nregs)(MtcsTarget *self,unsigned int num, machine_mode mode);
   //原型 targetm.hard_regno_mode_ok (regno, mode) #define TARGET_HARD_REGNO_MODE_OK hook_bool_uint_mode_true
   bool (*hard_regno_mode_ok)(MtcsTarget *self,unsigned int num, machine_mode mode);
   //原型 targetm.scalar_mode_supported_p (TImode)) #define TARGET_SCALAR_MODE_SUPPORTED_P nvptx_scalar_mode_supported_p
   bool (*scalar_mode_supported_p)(MtcsTarget *self,mtcs_mode mode);
   //原型 targetm.libgcc_floating_mode_supported_p (mode) #define TARGET_LIBGCC_FLOATING_MODE_SUPPORTED_P
   bool (*libgcc_floating_mode_supported_p)(MtcsTarget *self,scalar_mode mode);
   //原型  targetm.expand_to_rtl_hook (); #define TARGET_EXPAND_TO_RTL_HOOK hook_void_void
   void (*expand_to_rtl_hook)(MtcsTarget *self);
   //原型 targetm.starting_frame_offset () #define TARGET_STARTING_FRAME_OFFSET hook_hwi_void_0
   HOST_WIDE_INT (*starting_frame_offset)(MtcsTarget *self);
   //原型 targetm.use_pseudo_pic_reg () #define TARGET_USE_PSEUDO_PIC_REG hook_bool_void_false
   bool (*use_pseudo_pic_reg)(MtcsTarget *self);
   //原型 targetm.can_change_mode_class (GET_MODE (x_inner), mode, ALL_REGS)) #define TARGET_CAN_CHANGE_MODE_CLASS hook_bool_mode_mode_reg_class_t_true
   bool (*can_change_mode_class)(MtcsTarget *self,machine_mode from, machine_mode to, reg_class_t rclass);
   //原型targetm.legitimate_constant_p (mode, y)#define TARGET_LEGITIMATE_CONSTANT_P hook_bool_mode_rtx_true
   bool (*legitimate_constant_p)(MtcsTarget *self,machine_mode mode,rtx x);
   //原型 targetm.delegitimize_address (addr); #define TARGET_DELEGITIMIZE_ADDRESS delegitimize_mem_from_attrs
   rtx (*delegitimize_address)(MtcsTarget *self,rtx x);
   //原型 targetm.legitimize_address ; #define TARGET_LEGITIMIZE_ADDRESS default_legitimize_address
   rtx (*legitimize_address)(MtcsTarget *self,rtx x, rtx oldx, machine_mode mode);
   //原型 targetm.cannot_force_const_mem (mode, x) #define TARGET_CANNOT_FORCE_CONST_MEM nvptx_cannot_force_const_mem
   bool (*cannot_force_const_mem)(MtcsTarget *self, machine_mode mode,rtx x);
   //原型 targetm.use_blocks_for_constant_p (mode, x)#define TARGET_USE_BLOCKS_FOR_CONSTANT_P hook_bool_mode_const_rtx_true
   bool (*use_blocks_for_constant_p)(MtcsTarget *self, machine_mode mode,const_rtx x);
   //原型 targetm.truly_noop_truncation (GET_MODE_PRECISION (MODE1),GET_MODE_PRECISION (MODE2)))#define TARGET_TRULY_NOOP_TRUNCATION nvptx_truly_noop_truncation
   bool (*truly_noop_truncation)(MtcsTarget *self, poly_uint64 outprec,poly_uint64 inprec);
   //原型 targetm.libgcc_cmp_return_mode ();#define TARGET_LIBGCC_CMP_RETURN_MODE default_libgcc_cmp_return_mode
   scalar_int_mode    (*libgcc_cmp_return_mode)(MtcsTarget *self);
   //原型 targetm.use_anchors_for_symbol_p (base)) #define TARGET_USE_ANCHORS_FOR_SYMBOL_P default_use_anchors_for_symbol_p
   bool (*use_anchors_for_symbol_p)(MtcsTarget *self,const_rtx symbol);
   //原型targetm.insn_cost (insn, speed);#define TARGET_INSN_COST NULL //host=NULL nvptx=NULL;
   int (*insn_cost)(MtcsTarget *self,rtx_insn *insn, bool speed);
   //原型 targetm.cstore_mode (icode);#define TARGET_CSTORE_MODE sparc_cstore_mode
   scalar_int_mode (*cstore_mode)(MtcsTarget *self,enum insn_code icode);
   //原型 targetm.mode_dependent_address_p (addr, addrspace); #define TARGET_MODE_DEPENDENT_ADDRESS_P default_mode_dependent_address_p
   bool (*mode_dependent_address_p)(MtcsTarget *self,const_rtx addr ATTRIBUTE_UNUSED,addr_space_t addrspace ATTRIBUTE_UNUSED);
   //原型 targetm.set_current_function(tree fndecl); #define TARGET_SET_CURRENT_FUNCTION hook_void_tree
   void (*set_current_function)(MtcsTarget *self,tree fndecl);
   //原型targetm.modes_tieable_p  #define TARGET_MODES_TIEABLE_P hook_bool_mode_mode_true
   bool (*modes_tieable_p)(MtcsTarget *self,machine_mode mode,machine_mode modex);
   //原型targetm.rtx_costs  #define TARGET_RTX_COSTS aarch64_rtx_costs_wrapper
   bool (*rtx_costs)(MtcsTarget *self,rtx x, machine_mode mode, int outer ATTRIBUTE_UNUSED,
                     int param ATTRIBUTE_UNUSED, int *cost, bool speed);
   //原型targetm.use_by_pieces_infrastructure_p #define TARGET_USE_BY_PIECES_INFRASTRUCTURE_P default_use_by_pieces_infrastructure_p
   bool (*use_by_pieces_infrastructure_p)(MtcsTarget *self,unsigned HOST_WIDE_INT size,
                     unsigned int alignment,enum by_pieces_operation op,bool speed_p);
   //原型 targetm.emit_epilogue_for_sibcall #define TARGET_EMIT_EPILOGUE_FOR_SIBCALL NULL
   void (* emit_epilogue_for_sibcall) (MtcsTarget *self,rtx_call_insn *sibcall);
   //原型targetm.vector_mode_supported_p (result_mode)) #define TARGET_VECTOR_MODE_SUPPORTED_P nvptx_vector_mode_supported
   bool (*  vector_mode_supported_p) (MtcsTarget *self,machine_mode mode);
   //原型 targetm.narrow_volatile_bitfield () #define TARGET_NARROW_VOLATILE_BITFIELD hook_bool_void_false
   bool (*  narrow_volatile_bitfield) (MtcsTarget *self);
   //原型 targetm.stack_protect_runtime_enabled_p () #define TARGET_STACK_PROTECT_RUNTIME_ENABLED_P hook_bool_void_true
   bool (* stack_protect_runtime_enabled_p) (MtcsTarget *self);
   //原型 targetm.small_register_classes_for_mode_p (#define TARGET_SMALL_REGISTER_CLASSES_FOR_MODE_P hook_bool_mode_false
   bool (* small_register_classes_for_mode_p) (MtcsTarget *self,machine_mode funmode);
   //原型 targetm.have_conditional_execution #define TARGET_HAVE_CONDITIONAL_EXECUTION default_have_conditional_execution
   bool (*have_conditional_execution) (MtcsTarget *self);
   //原型 targetm.precompute_tls_p (args[i].mode, args[i].value) #define TARGET_PRECOMPUTE_TLS_P hook_bool_mode_rtx_false
   bool (*precompute_tls_p)(MtcsTarget *self,machine_mode, rtx);
   //原型  targetm.class_likely_spilled_p #define TARGET_CLASS_LIKELY_SPILLED_P default_class_likely_spilled_p
   bool (*class_likely_spilled_p)(MtcsTarget *self,reg_class_t rclass);
   //原型 targetm.overlap_op_by_pieces_p  #define TARGET_OVERLAP_OP_BY_PIECES_P hook_bool_void_false
   bool (*overlap_op_by_pieces_p)(MtcsTarget *self);
   //原型 targetm.stack_protect_fail () #define TARGET_STACK_PROTECT_FAIL default_external_stack_protect_fail
   tree (*stack_protect_fail)(MtcsTarget *self);
   //原型 targetm.mode_rep_extended (imode, omode)  #define TARGET_MODE_REP_EXTENDED default_mode_rep_extended
   int (*mode_rep_extended)(MtcsTarget *self,scalar_int_mode, scalar_int_mode);
   //原型 targetm.compare_by_pieces_branch_ratio (mode) #define TARGET_COMPARE_BY_PIECES_BRANCH_RATIO default_compare_by_pieces_branch_ratio
   int (*compare_by_pieces_branch_ratio)(MtcsTarget *self,machine_mode);
   //原型 targetm.conditional_register_usage (); #define TARGET_CONDITIONAL_REGISTER_USAGE hook_void_void
   void (*conditional_register_usage)(MtcsTarget *self);
   //原型 targetm.hard_regno_call_part_clobbered (m_id, regno, mode) #define TARGET_HARD_REGNO_CALL_PART_CLOBBERED hook_bool_uint_uint_mode_false
   bool (*hard_regno_call_part_clobbered)(MtcsTarget *self,unsigned int abi_id, unsigned int regno,machine_mode mode);
   //原型 targetm.class_max_nregs ((reg_class_t) i, (machine_mode) m) #define TARGET_CLASS_MAX_NREGS default_class_max_nregs
   unsigned char(*class_max_nregs)(MtcsTarget *self,reg_class_t rclass ATTRIBUTE_UNUSED,machine_mode mode ATTRIBUTE_UNUSED);
   //原型 targetm.init_libfuncs ();#define TARGET_INIT_LIBFUNCS hook_void_void
   void (*init_libfuncs)(MtcsTarget *self);
   //原型  targetm.default_short_enums ();#define TARGET_DEFAULT_SHORT_ENUMS hook_bool_void_false
   bool (*default_short_enums)(MtcsTarget *self);
   //原型 targetm.override_options_after_change (); #define TARGET_OVERRIDE_OPTIONS_AFTER_CHANGE hook_void_void
   void (*override_options_after_change)(MtcsTarget *self);
   //原型 targetm.init_builtins ();#define TARGET_INIT_BUILTINS nvptx_init_builtins
   void (*init_builtins)(MtcsTarget *self);
   //原型 targetm.stack_protect_guard #define TARGET_STACK_PROTECT_GUARD default_stack_protect_guard
   tree  (*stack_protect_guard)(MtcsTarget *self);
   //原型 targetm.cannot_copy_insn_p #define TARGET_CANNOT_COPY_INSN_P nvptx_cannot_copy_insn_p
   bool (*cannot_copy_insn_p)(MtcsTarget *self,rtx_insn *insn);
   //原型 targetm.md_asm_adjust #define TARGET_MD_ASM_ADJUST NULL
   rtx_insn *(*md_asm_adjust)(MtcsTarget *self,vec<rtx> &outputs, vec<rtx> &inputs,
               vec<machine_mode> &input_modes,
               vec<const char *> &constraints, vec<rtx> &uses,
               vec<rtx> &clobbers, HardRegSet &clobbered_regs,
               location_t loc);
   //原型 targetm.expand_builtin (exp, target, subtarget, mode, ignore) #define TARGET_EXPAND_BUILTIN nvptx_expand_builtin
   rtx (*expand_builtin)(MtcsTarget *self,tree exp, rtx target, rtx ARG_UNUSED (subtarget),
   machine_mode mode, int ignore);
   //原型 targetm.libc_has_function #define TARGET_LIBC_HAS_FUNCTION nvptx_libc_has_function
   bool (*libc_has_function)(MtcsTarget *self,enum function_class fn_class, tree type);
   //原型 targetm.unwind_word_mode (), #define TARGET_UNWIND_WORD_MODE default_unwind_word_mode
   scalar_int_mode  (*unwind_word_mode)(MtcsTarget *self);
   //原型 targetm.eh_return_filter_mode () #define TARGET_EH_RETURN_FILTER_MODE default_eh_return_filter_mode
   scalar_int_mode  (*eh_return_filter_mode)(MtcsTarget *self);
   //原型 targetm.expand_builtin_va_start #define TARGET_EXPAND_BUILTIN_VA_START NULL
   void (*expand_builtin_va_start)(MtcsTarget *self,tree valist, rtx nextarg);
   //原型 targetm.fn_abi_va_list (cfun->decl) #define TARGET_FN_ABI_VA_LIST std_fn_abi_va_list
   tree (* fn_abi_va_list)(MtcsTarget *self,tree fndecl ATTRIBUTE_UNUSED);
   //原型 argetm.canonical_va_list_type (TREE_TYPE (valist)); #define TARGET_CANONICAL_VA_LIST_TYPE std_canonical_va_list_type
   tree (* canonical_va_list_type)(MtcsTarget *self,tree type);
   //原型 targetm.memmodel_check (val) #define TARGET_MEMMODEL_CHECK ix86_memmodel_check
   unsigned HOST_WIDE_INT (*memmodel_check)(MtcsTarget *self,unsigned HOST_WIDE_INT val);
   //原型 targetm.speculation_safe_value #define TARGET_SPECULATION_SAFE_VALUE default_speculation_safe_value
   rtx (*speculation_safe_value)(MtcsTarget *self,machine_mode mode ATTRIBUTE_UNUSED,rtx result, rtx val,rtx failval ATTRIBUTE_UNUSED);
   //原型 targetm.optab_supported_p (optab, to_mode, from_mode, opt_type) #define TARGET_OPTAB_SUPPORTED_P default_optab_supported_p
   bool (*optab_supported_p)(MtcsTarget *self,int op, machine_mode mode1, machine_mode,optimization_type opt_type);
   //原型 targetm.atomic_align_for_mode (QImode)  #define TARGET_ATOMIC_ALIGN_FOR_MODE hook_uint_mode_0
   unsigned int (*atomic_align_for_mode)(MtcsTarget *self,machine_mode mode);
   //原型 targetm.floatn_mode (n, extended) #define TARGET_FLOATN_MODE default_floatn_mode
   opt_scalar_float_mode (*floatn_mode)(MtcsTarget *self,int n, bool extended);
   //原型 targetm.decimal_float_supported_p() #define TARGET_DECIMAL_FLOAT_SUPPORTED_P default_decimal_float_supported_p
   bool (*decimal_float_supported_p)(MtcsTarget *self);
   //原型  targetm.build_builtin_va_list () #define TARGET_BUILD_BUILTIN_VA_LIST std_build_builtin_va_list
   tree (*build_builtin_va_list)(MtcsTarget *self);
   //原型  targetm.mangle_decl_assembler_name (decl, libname) #define TARGET_MANGLE_DECL_ASSEMBLER_NAME default_mangle_decl_assembler_name
   tree (*mangle_decl_assembler_name)(MtcsTarget *self,tree decl ATTRIBUTE_UNUSED,tree id);
   //原型 targetm.floatn_builtin_p ((int) ENUM) #define TARGET_FLOATN_BUILTIN_P default_floatn_builtin_p
   bool (*floatn_builtin_p)(MtcsTarget *self,int type);
   //原型 targetm.member_type_forces_blk (type, VOIDmode) #define TARGET_MEMBER_TYPE_FORCES_BLK default_member_type_forces_blk
   bool (*member_type_forces_blk)(MtcsTarget *self,const_tree type,machine_mode mode);
   //原型 targetm.array_mode (elem_mode, num_elems) #define TARGET_ARRAY_MODE hook_optmode_mode_uhwi_none
   opt_machine_mode (*array_mode)(MtcsTarget *self,machine_mode mode, unsigned HOST_WIDE_INT size);
   //原型 targetm.array_mode_supported_p (elem_mode, num_elems) #define TARGET_ARRAY_MODE_SUPPORTED_P hook_bool_mode_uhwi_false
   bool (*array_mode_supported_p)(MtcsTarget *self,machine_mode mode, unsigned HOST_WIDE_INT size);
   //原型 targetm.vector_alignment #define TARGET_VECTOR_ALIGNMENT nvptx_vector_alignment
   HOST_WIDE_INT (*vector_alignment)(MtcsTarget *self,const_tree type);
   //原型 targetm.ms_bitfield_layout_p (DECL_FIELD_CONTEXT (decl)) #define TARGET_MS_BITFIELD_LAYOUT_P hook_bool_const_tree_false
   bool (*ms_bitfield_layout_p)(MtcsTarget *self,const_tree t);
   //原型 targetm.vector_mode_supported_any_target_p (mode) #define TARGET_VECTOR_MODE_SUPPORTED_ANY_TARGET_P hook_bool_mode_true
   bool (*vector_mode_supported_any_target_p)(MtcsTarget *self,machine_mode mode);
   //原型  targetm.insert_attributes (*node, &attributes); #define TARGET_INSERT_ATTRIBUTES hook_void_tree_treeptr
   void (*insert_attributes)(MtcsTarget *self,tree fndecl, tree *attributes);
   //原型 targetm.comp_type_attributes (type1, type2) #define TARGET_COMP_TYPE_ATTRIBUTES hook_int_const_tree_const_tree_1
   int (*comp_type_attributes) (MtcsTarget *self,const_tree type1, const_tree type2);
   //原型targetm.static_rtx_alignment(align_mode); #define TARGET_STATIC_RTX_ALIGNMENT default_static_rtx_alignment
   HOST_WIDE_INT (*static_rtx_alignment) (MtcsTarget *self,machine_mode mode);
   //原型 targetm.cannot_modify_jumps_p () #define TARGET_CANNOT_MODIFY_JUMPS_P hook_bool_void_false
   bool (*cannot_modify_jumps_p)(MtcsTarget *self);
   //原型  targetm.instantiate_decls (); #define TARGET_INSTANTIATE_DECLS hook_void_void
   void (*instantiate_decls)(MtcsTarget *self);
   //原型  targetm.lra_p () #define TARGET_LRA_P default_lra_p
   bool (*lra_p)(MtcsTarget *self);
   //原型 targetm.can_follow_jump (insn, seq->insn (0)) #define TARGET_CAN_FOLLOW_JUMP hook_bool_const_rtx_insn_const_rtx_insn_true
   bool (*can_follow_jump) (MtcsTarget *self,const rtx_insn *follower, const rtx_insn *followee);
   //原型 targetm.unspec_may_trap_p (x, flags); #define TARGET_UNSPEC_MAY_TRAP_P default_unspec_may_trap_p
   int  (*unspec_may_trap_p) (MtcsTarget *self,const_rtx x, unsigned flags);
   //原型 targetm.extra_live_on_entry (entry_block_defs); #define TARGET_EXTRA_LIVE_ON_ENTRY hook_void_bitmap
   void(* extra_live_on_entry)(MtcsTarget *self,bitmap regs ATTRIBUTE_UNUSED);
   //原型 targetm.cc_modes_compatible (mode, set_mode); #define TARGET_CC_MODES_COMPATIBLE default_cc_modes_compatible
   machine_mode (* cc_modes_compatible)(MtcsTarget *self,machine_mode m1, machine_mode m2);
   //原型 targetm.fixed_condition_code_regs (&cc_regno_1, &cc_regno_2) #define TARGET_FIXED_CONDITION_CODE_REGS hook_bool_uintp_uintp_false
   bool (*fixed_condition_code_regs) (MtcsTarget *self,nuint *p1,nuint *p2);
   //原型 targetm.address_cost (x, mode, as, speed); #define TARGET_ADDRESS_COST default_address_cost
   int (*address_cost) (MtcsTarget *self,rtx x, machine_mode mode ,addr_space_t as ,bool speed);
   //原型 targetm.memory_move_cost (mode, rclass, in); #define TARGET_MEMORY_MOVE_COST default_memory_move_cost
   int (*memory_move_cost)(MtcsTarget *self,machine_mode mode ATTRIBUTE_UNUSED,
               reg_class_t rclass ATTRIBUTE_UNUSED, bool in ATTRIBUTE_UNUSED);
   //原型 targetm.register_move_cost (mode, from, to); #define TARGET_REGISTER_MOVE_COST default_register_move_cost
   int (*register_move_cost)(MtcsTarget *self,machine_mode mode ATTRIBUTE_UNUSED,
               reg_class_t from ATTRIBUTE_UNUSED,reg_class_t to ATTRIBUTE_UNUSED);
   //原型 #define TARGET_COMPUTE_PRESSURE_CLASSES NULL targetm.compute_pressure_classes
   int (*compute_pressure_classes) (MtcsTarget *self,reg_class *classes);
   //原型 targetm.keep_leaf_when_profiled () #define TARGET_KEEP_LEAF_WHEN_PROFILED default_keep_leaf_when_profiled
   bool (*keep_leaf_when_profiled) (MtcsTarget *self);
   //原型 targetm.frame_pointer_required () #define TARGET_FRAME_POINTER_REQUIRED hook_bool_void_false
   bool (*frame_pointer_required) (MtcsTarget *self);
   //原型  targetm.can_eliminate (eliminables[i].from, eliminables[i].to) #define TARGET_CAN_ELIMINATE hook_bool_const_int_const_int_true
   bool (*can_eliminate) (MtcsTarget *self,const int from, const int to);
   //原型 targetm.allocate_initial_value #define TARGET_ALLOCATE_INITIAL_VALUE NULL
   rtx (*allocate_initial_value) (MtcsTarget *self,rtx x);
   //原型 targetm.init_pic_reg (); #define TARGET_INIT_PIC_REG hook_void_void
   void (*init_pic_reg) (MtcsTarget *self);
   //原型 targetm.setjmp_preserves_nonvolatile_regs_p () #define TARGET_SETJMP_PRESERVES_NONVOLATILE_REGS_P hook_bool_void_false
   bool (*setjmp_preserves_nonvolatile_regs_p) (MtcsTarget *self);
   //原型 targetm.preferred_reload_class(x, rclass);#define TARGET_PREFERRED_RELOAD_CLASS default_preferred_reload_class
   reg_class_t (*preferred_reload_class) (MtcsTarget *self,rtx x, reg_class_t regclass);
   //原型 targetm.secondary_reload (to_p, x, rclass, mode, &sri); #define TARGET_SECONDARY_RELOAD default_secondary_reload
   reg_class_t (*secondary_reload)(MtcsTarget *self,bool in_p ATTRIBUTE_UNUSED, rtx x ATTRIBUTE_UNUSED,
               reg_class_t reload_class_i ATTRIBUTE_UNUSED, machine_mode reload_mode ATTRIBUTE_UNUSED, secondary_reload_info *sri);
   //原型 targetm.ira_change_pseudo_allocno_class  #define TARGET_IRA_CHANGE_PSEUDO_ALLOCNO_CLASS default_ira_change_pseudo_allocno_class
   reg_class_t (*ira_change_pseudo_allocno_class) (MtcsTarget *self,int regno ATTRIBUTE_UNUSED,
               reg_class_t cl,reg_class_t best_cl ATTRIBUTE_UNUSED);
   //原型 targetm.noce_conversion_profitable_p  #define TARGET_NOCE_CONVERSION_PROFITABLE_P default_noce_conversion_profitable_p
   bool (*noce_conversion_profitable_p) (MtcsTarget *self,rtx_insn *seq,  struct noce_if_info *if_info);
   //原型 targetm.max_noce_ifcvt_seq_cost (then_edge) #define TARGET_MAX_NOCE_IFCVT_SEQ_COST default_max_noce_ifcvt_seq_cost
   unsigned int (*max_noce_ifcvt_seq_cost) (MtcsTarget *self,edge e);
   //原型 targetm.canonicalize_comparison  #define TARGET_CANONICALIZE_COMPARISON default_canonicalize_comparison
   void (*canonicalize_comparison) (MtcsTarget *self,int *code, rtx *op0, rtx *op1,bool op0_preserve_value);
   //原型  targetm.legitimate_combined_insn (insn) #define TARGET_LEGITIMATE_COMBINED_INSN hook_bool_rtx_insn_true
   bool (*legitimate_combined_insn) (MtcsTarget *self,rtx_insn *insn);
   //原型 targetm.hard_regno_scratch_ok #define TARGET_HARD_REGNO_SCRATCH_OK default_hard_regno_scratch_ok
   bool (*hard_regno_scratch_ok) (MtcsTarget *self,unsigned int regno);
   //原型 targetm.use_late_prologue_epilogue() #define TARGET_USE_LATE_PROLOGUE_EPILOGUE hook_bool_void_false
   bool (*use_late_prologue_epilogue) (MtcsTarget *self);
   //原型 targetm.machine_dependent_reorg () #define TARGET_MACHINE_DEPENDENT_REORG nvptx_reorg
   void  (*machine_dependent_reorg) (MtcsTarget *self);
   //原型 targetm.dwarf_register_span (reg) #define TARGET_DWARF_REGISTER_SPAN hook_rtx_rtx_null
   rtx  (*dwarf_register_span) (MtcsTarget *self,rtx reg);
   //原型 targetm.dwarf_frame_reg_mode #define TARGET_DWARF_FRAME_REG_MODE default_dwarf_frame_reg_mode
   machine_mode (*dwarf_frame_reg_mode) (MtcsTarget *self,int regno);
   //原型 targetm.init_dwarf_reg_sizes_extra (address); #define TARGET_INIT_DWARF_REG_SIZES_EXTRA hook_void_tree
   void (*init_dwarf_reg_sizes_extra) (MtcsTarget *self,tree t);
   //原型 targetm.loop_unroll_adjust (nunroll, loop); #define TARGET_LOOP_UNROLL_ADJUST NULL
   unsigned (*loop_unroll_adjust) (MtcsTarget *self,unsigned nunroll, class loop *loop);
   //原型 targetm.expand_divmod_libfunc (libfunc, mode, op0, op1, &quotient, &remainder); #define TARGET_EXPAND_DIVMOD_LIBFUNC NULL
   void (* expand_divmod_libfunc) (MtcsTarget *self,rtx libfunc, machine_mode mode,rtx op0, rtx op1,rtx *quot_p, rtx *rem_p);
   //原型  targetm.has_ifunc_p () #define TARGET_HAS_IFUNC_P default_has_ifunc_p
   bool  (*has_ifunc_p) (MtcsTarget *self);
   //原型 #define TARGET_RESET_LOCATION_VIEW NULL
   int  (*reset_location_view) (MtcsTarget *self,rtx_insn *insn);
   //原型  targetm.dwarf_poly_indeterminate_value #define TARGET_DWARF_POLY_INDETERMINATE_VALUE default_dwarf_poly_indeterminate_value
   unsigned int (*dwarf_poly_indeterminate_value)(MtcsTarget *self,unsigned int i, unsigned int *factor,int *offset);
   //原型 targetm.const_not_ok_for_debug_p #define TARGET_CONST_NOT_OK_FOR_DEBUG_P default_const_not_ok_for_debug_p
   bool (*const_not_ok_for_debug_p) (MtcsTarget *self,rtx x);
   //原型 targetm.dwarf_calling_convention #define TARGET_DWARF_CALLING_CONVENTION hook_int_const_tree_0
   int (*dwarf_calling_convention)(MtcsTarget *self,const_tree func);
   //原型 targetm.shift_truncation_mask  #define TARGET_SHIFT_TRUNCATION_MASK default_shift_truncation_mask default_shift_truncation_mask
   unsigned HOST_WIDE_INT (*shift_truncation_mask)(MtcsTarget *self,machine_mode mode);
   //原型 targetm.fold_builtin #define TARGET_FOLD_BUILTIN hook_tree_tree_int_treep_bool_null
   tree  (*fold_builtin)(MtcsTarget *self,tree fndecl, int n_args,tree *args, bool ignore ATTRIBUTE_UNUSED);
   //原型 targetm.have_ccmp #define TARGET_HAVE_CCMP default_have_ccmp
   bool (*have_ccmp)(MtcsTarget *self);
   //原型 targetm.libc_has_fast_function (BUILT_IN_MEMPCPY) #define TARGET_LIBC_HAS_FAST_FUNCTION default_libc_has_fast_function
   bool (*libc_has_fast_function)(MtcsTarget *self,int fcode ATTRIBUTE_UNUSED);
   //原型 targetm.dw_cfi_oprnd1_desc (cfi, oprnd_type) #define TARGET_DW_CFI_OPRND1_DESC hook_bool_dwcfi_dwcfioprndtyperef_false
   bool (*dw_cfi_oprnd1_desc)(MtcsTarget *self,dwarf_call_frame_info,dw_cfi_oprnd_type &);

   //原型 targetm.arm_eabi_unwinder #define TARGET_ARM_EABI_UNWINDER false
   bool arm_eabi_unwinder;
   //原型 targetm.have_ctors_dtors #define TARGET_HAVE_CTORS_DTORS false
   bool have_ctors_dtors;
   //原型 targetm.dtors_from_cxa_atexit;#define TARGET_DTORS_FROM_CXA_ATEXIT false
   bool dtors_from_cxa_atexit;


   char *open_paren;
   char *close_paren;
   bool unwind_emit_before_insn;
   bool call_fusage_contains_non_callee_clobbers;
   bool have_srodata_section;
   bool have_switchable_bss_sections;
   //原型 targetm.have_shadow_call_stack #define TARGET_HAVE_SHADOW_CALL_STACK false
   bool have_shadow_call_stack;

   //原型 targetm.have_tls#ifndef TARGET_HAVE_TLS
   bool have_tls;
   struct {
      int   version;//目标版本
      int   isa;    //指令集架构
      char *name; //目标名字如cuda、gcn、spirv、opencl等
   }platformInfo;
   //原型  targetm.libfunc_gnu_prefix #define TARGET_LIBFUNC_GNU_PREFIX false
   bool libfunc_gnu_prefix;
   nuint max_anchor_offset;
   nuint min_anchor_offset;
   void /*!symbol_table*/ *symtab;//原型 extern GTY(()) symbol_table *symtab; cgraph.h
   int target_supports_aliases;//原型 TARGET_SUPPORTS_ALIASES defaults.h nvptx.h
   /* Stack protection related decls living in libgcc.  */
   //原型 stack_chk_guard_decl targhooks.cc
   tree stack_chk_guard_decl;
   //原型 targetm.atomic_test_and_set_trueval != 1 #define TARGET_ATOMIC_TEST_AND_SET_TRUEVAL 1
   int atomic_test_and_set_trueval;
   //原型 #define TARGET_ATTRIBUTE_TABLE nvptx_attribute_table
   scoped_attribute_specs *attribute_specs;
   //原型  targetm.flags_regnum #define TARGET_FLAGS_REGNUM INVALID_REGNUM
   unsigned flags_regnum;
   //原型 targetm.const_anchor #define TARGET_CONST_ANCHOR 0
   nuint const_anchor;
   //原型  #define TARGET_NO_REGISTER_ALLOCATION true nvptx=true
   bool no_register_allocation;
   //原型 targetm.want_debug_pub_sections #define TARGET_WANT_DEBUG_PUB_SECTIONS false
   bool want_debug_pub_sections;

   MtcsFinal *mtcsFinal;
   MtcsAsm *mtcsAsm;
   MtcsDebug *currentDebug; //正在工作的debug
   MtcsDwarf2Codeview *mtcsDwarf2Codeview;
   MtcsDwarf2Out *mtcsDwarf2Out;
   MtcsDwarf2Asm *mtcsDwarf2Asm;
   MtcsDwarf2Cfi *mtcsDwarf2Cfi;
   MtcsDwarf2Lineno *mtcsDwarf2Lineno;
   MtcsDoNothingDebug *mtcsDoNothingDebug;

   MtcsExcept *mtcsExcept;
   MtcsReg *mtcsReg;//寄存器
   MtcsMode *mtcsMode;//machine_mode
   MtcsRTL  *mtcsRTL;//rtx
   MtcsOptions *mtcsOptions;
   MtcsRecog *mtcsRecog;
   MtcsPreds *mtcsPreds;
   MtcsAlign *mtcsAlign;
   MtcsFunc *mtcsFunc;
   MtcsOpinit *mtcsOpinit;
   MtcsEmit *mtcsEmit;
   MtcsSimplifyRtx *mtcsSimplifyRtx;
   MtcsExpr *mtcsExpr;
   MtcsExplow *mtcsExplow;
   MtcsReal *mtcsReal;
   MtcsOutput *mtcsOutput;
   MtcsArgs *mtcsArgs;
   MtcsRtlanal *mtcsRtlanal;
   MtcsOptabs *mtcsOptabs;
   MtcsReload *mtcsReload;
   MtcsExpmed *mtcsExpmed;
   MtcsDojump *mtcsDojump;
   MtcsCodes *mtcsCodes;
   MtcsLowerSubreg *mtcsLowerSubreg;
   MtcsCfgLoopanal *mtcsCfgLoopanal;
   MtcsCalls *mtcsCalls;
   MtcsCcmp *mtcsCcmp;
   MtcsCgraph *mtcsCgraph;
   MtcsFuncAbi *mtcsFuncAbi;
   MtcsLibfuncs *mtcsLibfuncs;
   MtcsPassMgr *mtcsPassMgr;
   MtcsConfig *mtcsConfig;
   MtcsOpts *mtcsOpts;
   MtcsExpand *mtcsExpand;
   //不再使用 2025-10-05 npointer *mtcsIpaProp;
   MtcsVar *mtcsVar;
   MtcsClones *mtcsClones;
   MtcsGimple *mtcsGimple;
   MtcsBuiltins *mtcsBuiltins;
   MtcsStmt *mtcsStmt;
   MtcsCfgRtl *mtcsCfgRtl;
   MtcsTraverseTree *mtcsTraverseTree;
   MtcsTree *mtcsTree;
   MtcsLang *mtcsLang;
   MtcsStorLayout *mtcsStorLayout;
   MtcsAttribs *mtcsAttribs;
   MtcsCfgBuild *mtcsCfgBuild;
   MtcsCfgCleanup *mtcsCfgCleanup;
   MtcsCse *mtcsCse;
   MtcsCfgContext *mtcsCfgContext;
   MtcsCfg *mtcsCfg;
   MtcsConst *mtcsConst;
   MtcsDfa *mtcsDfa;
   MtcsFixed *mtcsFixed;
   MtcsAddr *mtcsAddr;
   MtcsUnspec *mtcsUnspec;
   MtcsInsnAttr *mtcsInsnAttr;
   MtcsDfscan *mtcsDfscan;
   //MtcsDfcore *mtcsDfcore;
   void *mtcsDfcore;
   //MtcsDfproblems *mtcsDfproblems;
   void *mtcsDfproblems;

   MtcsRtlPassMgr *mtcsRtlPassMgr;
   MtcsAlias *mtcsAlias;
   MtcsIraMgr *mtcsIraMgr;//声明在 ira/mtcsiracommon.h
   MtcsReload1 *mtcsReload1;
   MtcsDce *mtcsDce;
   MtcsCseLib *mtcsCseLib;
   //避免头文件引入 value-query.h
   //MtcsSsaPropagate *mtcsSsaPropagate;
   void *mtcsSsaPropagate;
   //不再使用 2025-10-05 MtcsSsaStrlen *mtcsSsaStrlen;
   //不再使用 2025-10-05 MtcsSsaSprintf *mtcsSsaSprintf;
   MtcsOutofSsa *mtcsOutofSsa;
   MtcsCfgLoop *mtcsCfgLoop;
   MtcsLoopIv *mtcsLoopIv;
   MtcsCfgLoopManip *mtcsCfgLoopManip;
   MtcsLoopinit *mtcsLoopinit;
   MtcsPredict *mtcsPredict;
   MtcsInternalFn *mtcsInternalFn;
   MtcsPort *mtcsPort;
   MtcsGimpleExpr *mtcsGimpleExpr;
   MtcsSsaAddress *mtcsSsaAddress;
   MtcsMachine *mtcsMachine;//包含各种target

   //避免头文件引入 tree-ssa-live.h
   //mtcsSsaCoalesce *mtcsSsaCoalesce;
   void   *mtcsSsaCoalesce;
   //获取需要link的函数名
   char *(*getLinkFuncName)(MtcsTarget *self);
   //由于同一个平台有多个版本号，往往这些版本号是全局的，所以在编译前用每个target的版本号设为全局变量。
   void (*publishVersion)(MtcsTarget *self);
};

void                mtcs_target_init(MtcsTarget *self);
void                mtcs_target_set_version(MtcsTarget *self,int version);
int                 mtcs_target_get_version(MtcsTarget *self);
void                mtcs_target_set_isa(MtcsTarget *self,int isa);
int                 mtcs_target_get_isa(MtcsTarget *self);
void                mtcs_target_set_platform_name(MtcsTarget *self,char *name);
const char         *mtcs_target_get_platform_name(MtcsTarget *self);
//原型 TARGET_SUPPORTS_ALIASES defaults.h nvptx.h
void                mtcs_target_set_supports_aliases(MtcsTarget *self,int supports);

MtcsReg            *mtcs_target_get_reg(MtcsTarget *self);
MtcsMode           *mtcs_target_get_mode(MtcsTarget *self);
MtcsFunc           *mtcs_target_create_func(MtcsTarget *self,tree decl,const char *fnname);
MtcsRTL            *mtcs_target_get_rtl(MtcsTarget *self);
MtcsRecog          *mtcs_target_get_recog(MtcsTarget *self);
MtcsPreds          *mtcs_target_get_preds(MtcsTarget *self);
MtcsAlign          *mtcs_target_get_align(MtcsTarget *self);
MtcsFunc           *mtcs_target_get_func(MtcsTarget *self);
MtcsOpinit         *mtcs_target_get_opinit(MtcsTarget *self);
MtcsEmit           *mtcs_target_get_emit(MtcsTarget *self);
MtcsAsm            *mtcs_target_get_asm(MtcsTarget *self);
MtcsSimplifyRtx    *mtcs_target_get_simplify_rtx(MtcsTarget *self);
MtcsExpr           *mtcs_target_get_expr(MtcsTarget *self);
MtcsExplow         *mtcs_target_get_explow(MtcsTarget *self);
MtcsReal           *mtcs_target_get_real(MtcsTarget *self);
MtcsOutput         *mtcs_target_get_output(MtcsTarget *self);
MtcsArgs           *mtcs_target_get_args(MtcsTarget *self);
MtcsOptions        *mtcs_target_get_options(MtcsTarget *self);
MtcsRtlanal        *mtcs_target_get_rtlanal(MtcsTarget *self);
MtcsOptabs         *mtcs_target_get_optabs(MtcsTarget *self);
MtcsReload         *mtcs_target_get_reload(MtcsTarget *self);
MtcsExpmed         *mtcs_target_get_expmed(MtcsTarget *self);
MtcsDojump         *mtcs_target_get_dojump(MtcsTarget *self);
MtcsFinal          *mtcs_target_get_final(MtcsTarget *self);
MtcsDebug          *mtcs_target_get_debug(MtcsTarget *self);
MtcsDwarf2Codeview *mtcs_target_get_dwarf2_codeview(MtcsTarget *self);
MtcsDwarf2Out      *mtcs_target_get_dwarf2_out(MtcsTarget *self);
//设置工作的的debug
void                mtcs_target_set_current_debug (MtcsTarget *self,MtcsDebug *debug);
MtcsDwarf2Lineno   *mtcs_target_get_dwarf2_lineno (MtcsTarget *self);
MtcsDoNothingDebug *mtcs_target_get_do_nothing_debug (MtcsTarget *self);

MtcsExcept         *mtcs_target_get_except(MtcsTarget *self);
MtcsDwarf2Asm      *mtcs_target_get_dwarf2_asm(MtcsTarget *self);
MtcsDwarf2Cfi      *mtcs_target_get_dwarf2_cfi(MtcsTarget *self);
MtcsCodes          *mtcs_target_get_codes(MtcsTarget *self);
MtcsLowerSubreg    *mtcs_target_get_lower_subreg(MtcsTarget *self);
MtcsCfgLoopanal    *mtcs_target_get_cfg_loopanal(MtcsTarget *self);
MtcsCalls          *mtcs_target_get_calls(MtcsTarget *self);
MtcsCcmp           *mtcs_target_get_ccmp(MtcsTarget *self);
MtcsCgraph         *mtcs_target_get_cgraph(MtcsTarget *self);
MtcsFuncAbi        *mtcs_target_get_func_abi(MtcsTarget *self);
MtcsLibfuncs       *mtcs_target_get_libfuncs(MtcsTarget *self);
MtcsPassMgr        *mtcs_target_get_pass_mgr(MtcsTarget *self);
MtcsConfig         *mtcs_target_get_config(MtcsTarget *self);
MtcsOpts           *mtcs_target_get_opts(MtcsTarget *self);
MtcsExpand         *mtcs_target_get_expand(MtcsTarget *self);
MtcsVar            *mtcs_target_get_var(MtcsTarget *self);
MtcsClones         *mtcs_target_get_clones(MtcsTarget *self);
//通过mtcsgimple获取gimple组件
//不再使用2025-10-05 MtcsTreeCfg *mtcs_target_get_tree_cfg(MtcsTarget *self);
//不再使用2025-10-05 MtcsTreeEh *mtcs_target_get_tree_eh(MtcsTarget *self);
MtcsBuiltins       *mtcs_target_get_builtins(MtcsTarget *self);
MtcsStmt           *mtcs_target_get_stmt(MtcsTarget *self);
MtcsCfgRtl         *mtcs_target_get_cfg_rtl(MtcsTarget *self);
MtcsTraverseTree   *mtcs_target_get_traverse_tree(MtcsTarget *self);
MtcsTree           *mtcs_target_get_tree(MtcsTarget *self);
MtcsLang           *mtcs_target_get_lang(MtcsTarget *self);
MtcsStorLayout     *mtcs_target_get_stor_layout(MtcsTarget *self);
MtcsAttribs        *mtcs_target_get_attribs(MtcsTarget *self);
MtcsCfgBuild       *mtcs_target_get_cfg_build(MtcsTarget *self);
MtcsCfgCleanup     *mtcs_target_get_cfg_cleanup(MtcsTarget *self);
MtcsCse            *mtcs_target_get_cse(MtcsTarget *self);
MtcsCfgContext     *mtcs_target_get_cfg_context(MtcsTarget *self);
MtcsCfg            *mtcs_target_get_cfg(MtcsTarget *self);
MtcsConst          *mtcs_target_get_const(MtcsTarget *self);
MtcsDfa            *mtcs_target_get_dfa(MtcsTarget *self);
MtcsFixed          *mtcs_target_get_fixed(MtcsTarget *self);
MtcsAddr           *mtcs_target_get_addr(MtcsTarget *self);
MtcsUnspec         *mtcs_target_get_unspec(MtcsTarget *self);
MtcsInsnAttr       *mtcs_target_get_insn_attr(MtcsTarget *self);
MtcsDfscan         *mtcs_target_get_dfscan(MtcsTarget *self);
//引入mtcsdfcore.h 报错 ../../gcc-14/gcc/df.h:622:55: error: invalid use of incomplete type ‘struct basic_block_def’
//改为宏定义
#define             mtcs_target_get_dfcore(mtcsTarget)  (MtcsDfcore *)mtcsTarget->mtcsDfcore;
#define             mtcs_target_get_dfproblems(mtcsTarget)  (MtcsDfproblems *)mtcsTarget->mtcsDfproblems;

///MtcsDfcore *mtcs_target_get_dfcore(MtcsTarget *self);
MtcsRtlPassMgr     *mtcs_target_get_rtl_pass_mgr(MtcsTarget *self);
MtcsAlias          *mtcs_target_get_alias(MtcsTarget *self);
MtcsIraMgr         *mtcs_target_get_ira_mgr(MtcsTarget *self);
MtcsReload1        *mtcs_target_get_reload1(MtcsTarget *self);
MtcsDce            *mtcs_target_get_dce(MtcsTarget *self);
MtcsCseLib         *mtcs_target_get_cse_lib(MtcsTarget *self);

#define            mtcs_target_get_ssa_propagate(mtcsTarget)  (MtcsSsaPropagate *)mtcsTarget->mtcsSsaPropagate;

MtcsOutofSsa       *mtcs_target_get_outof_ssa(MtcsTarget *self);
MtcsCfgLoop        *mtcs_target_get_cfg_loop(MtcsTarget *self);
MtcsLoopIv         *mtcs_target_get_loop_iv(MtcsTarget *self);
MtcsCfgLoopManip   *mtcs_target_get_cfg_loop_manip(MtcsTarget *self);
MtcsLoopinit       *mtcs_target_get_loopinit(MtcsTarget *self);
MtcsPredict        *mtcs_target_get_predict(MtcsTarget *self);
MtcsInternalFn     *mtcs_target_get_internal_fn(MtcsTarget *self);
MtcsPort           *mtcs_target_get_port(MtcsTarget *self);
MtcsGimpleExpr     *mtcs_target_get_gimple_expr(MtcsTarget *self);
#define             mtcs_target_get_ssa_coalesce(mtcsTarget)  (MtcsSsaCoalesce *)mtcsTarget->mtcsSsaCoalesce;
MtcsSsaAddress     *mtcs_target_get_ssa_address(MtcsTarget *self);
MtcsMachine        *mtcs_target_get_machine(MtcsTarget *self);
/**
 * 引入#include "ipa/mtcsipaprop.h"
 * error: ‘ipa_edge_args_sum_t’ does not name a type; did you mean ‘gt_pch_nx_ipa_edge_args_sum_t’?
   36 |      ipa_edge_args_sum_t *ipa_edge_args_sum;
 *mtcsIpaProp 设为类型 npointer
 *用宏，在调用者不用强转为MtcsIpaProp *
 */
//不再使用2025-10-05 #define    mtcs_target_get_ipa_prop(mtcsTarget)  (MtcsIpaProp *)mtcsTarget->mtcsIpaProp;
MtcsPass           *mtcs_target_get_pass(MtcsTarget *self,enum opt_pass_type type,char *name);
MtcsGimple         *mtcs_target_get_gimple(MtcsTarget *self);
//原型 target_default_pointer_address_modes_p target.h targhooks.cc
bool                mtcs_target_target_default_pointer_address_modes_p (MtcsTarget *self);
//获取需要链接的函数名
char               *mtcs_target_get_link_funcname(MtcsTarget *self);
//由于同一个平台有多个版本号，往往这些版本号是全局的，所以在编译前用每个target的版本号设为全局变量。
void                mtcs_target_publish_version(MtcsTarget *self);

#endif

