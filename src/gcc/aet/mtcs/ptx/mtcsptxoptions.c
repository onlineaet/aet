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
#include "options.h"
#include "expmed.h"
#include "expr.h"
#include "fold-const.h"
#include "dwarf2out.h"
#include "common/common-targhooks.h"

#include "options.h"
#include "tm.h"
#include "opts.h"
#include "intl.h"
#include "insn-attr-common.h"

#include "cpplib.h"
#include "cpplib.h"
#include "diagnostic-color.h"
#include "diagnostic-url.h"
#include "diagnostic.h"

#include "aet/aetprinttree.h"
#include "../mtcstarget.h"
#include "mtcsptxoptions.h"
#include "ptx-common.h"
#include "gen/ptx-optionsitem.h"

//来自每个平台的 options.h options.cc
int mtcs_ptx_isa_option=PTX_ISA_SM52;
int mtcs_ptx_version_option=PTX_VERSION_7_3;


/* Table of options enabled by default at different levels.
   Please keep this list sorted by level and alphabetized within
   each level; this makes it easier to keep the documentation
   in sync.  */
//原型 default_options_table opts.cc
 //在opts.cc default_options_table需要两个宏来创建 DELAY_SLOTS INSN_SCHEDULING 由子类调用该方法可以避免这个宏的使用
//在nvptx中 //#if DELAY_SLOTS定义，但值=0 INSN_SCHEDULING宏未定义
static const struct default_options default_options_table[] =
  {
    /* -O1 and -Og optimizations.  */
    { OPT_LEVELS_1_PLUS, OPT_fcombine_stack_adjustments, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_fcompare_elim, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_fcprop_registers, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_fdefer_pop, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_fforward_propagate, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_fguess_branch_probability, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_fipa_profile, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_fipa_pure_const, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_fipa_reference, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_fipa_reference_addressable, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_fmerge_constants, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_fomit_frame_pointer, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_freorder_blocks, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_fshrink_wrap, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_fsplit_wide_types, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_fthread_jumps, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_ftree_builtin_call_dce, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_ftree_ccp, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_ftree_ch, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_ftree_coalesce_vars, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_ftree_copy_prop, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_ftree_dce, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_ftree_dominator_opts, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_ftree_fre, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_ftree_sink, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_ftree_slsr, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_ftree_ter, NULL, 1 },
    { OPT_LEVELS_1_PLUS, OPT_fvar_tracking, NULL, 1 },

    /* -O1 (and not -Og) optimizations.  */
    { OPT_LEVELS_1_PLUS_NOT_DEBUG, OPT_fbranch_count_reg, NULL, 1 },
//#if DELAY_SLOTS 定义为0
  //  { OPT_LEVELS_1_PLUS_NOT_DEBUG, OPT_fdelayed_branch, NULL, 1 },
//#endif
    { OPT_LEVELS_1_PLUS_NOT_DEBUG, OPT_fdse, NULL, 1 },
    { OPT_LEVELS_1_PLUS_NOT_DEBUG, OPT_fif_conversion, NULL, 1 },
    { OPT_LEVELS_1_PLUS_NOT_DEBUG, OPT_fif_conversion2, NULL, 1 },
    { OPT_LEVELS_1_PLUS_NOT_DEBUG, OPT_finline_functions_called_once, NULL, 1 },
    { OPT_LEVELS_1_PLUS_NOT_DEBUG, OPT_fmove_loop_invariants, NULL, 1 },
    { OPT_LEVELS_1_PLUS_NOT_DEBUG, OPT_fmove_loop_stores, NULL, 1 },
    { OPT_LEVELS_1_PLUS_NOT_DEBUG, OPT_fssa_phiopt, NULL, 1 },
    { OPT_LEVELS_1_PLUS_NOT_DEBUG, OPT_fipa_modref, NULL, 1 },
    { OPT_LEVELS_1_PLUS_NOT_DEBUG, OPT_ftree_bit_ccp, NULL, 1 },
    { OPT_LEVELS_1_PLUS_NOT_DEBUG, OPT_ftree_dse, NULL, 1 },
    { OPT_LEVELS_1_PLUS_NOT_DEBUG, OPT_ftree_pta, NULL, 1 },
    { OPT_LEVELS_1_PLUS_NOT_DEBUG, OPT_ftree_sra, NULL, 1 },

    /* -O2 and -Os optimizations.  */
    { OPT_LEVELS_2_PLUS, OPT_fcaller_saves, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_fcode_hoisting, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_fcrossjumping, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_fcse_follow_jumps, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_fdevirtualize, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_fdevirtualize_speculatively, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_fexpensive_optimizations, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_fgcse, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_fhoist_adjacent_loads, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_findirect_inlining, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_finline_small_functions, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_fipa_bit_cp, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_fipa_cp, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_fipa_icf, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_fipa_ra, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_fipa_sra, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_fipa_vrp, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_fisolate_erroneous_paths_dereference, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_flra_remat, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_foptimize_sibling_calls, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_fpartial_inlining, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_fpeephole2, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_freorder_functions, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_frerun_cse_after_loop, NULL, 1 },
//#ifdef INSN_SCHEDULING nvptx未定义
//    { OPT_LEVELS_2_PLUS, OPT_fschedule_insns2, NULL, 1 },
//#endif
    { OPT_LEVELS_2_PLUS, OPT_fstrict_aliasing, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_fstore_merging, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_ftree_pre, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_ftree_switch_conversion, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_ftree_tail_merge, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_ftree_vrp, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_fvect_cost_model_, NULL,
      VECT_COST_MODEL_VERY_CHEAP },
    { OPT_LEVELS_2_PLUS, OPT_finline_functions, NULL, 1 },
    { OPT_LEVELS_2_PLUS, OPT_ftree_loop_distribute_patterns, NULL, 1 },

    /* -O2 and above optimizations, but not -Os or -Og.  */
    { OPT_LEVELS_2_PLUS_SPEED_ONLY, OPT_falign_functions, NULL, 1 },
    { OPT_LEVELS_2_PLUS_SPEED_ONLY, OPT_falign_jumps, NULL, 1 },
    { OPT_LEVELS_2_PLUS_SPEED_ONLY, OPT_falign_labels, NULL, 1 },
    { OPT_LEVELS_2_PLUS_SPEED_ONLY, OPT_falign_loops, NULL, 1 },
    { OPT_LEVELS_2_PLUS_SPEED_ONLY, OPT_foptimize_strlen, NULL, 1 },
    { OPT_LEVELS_2_PLUS_SPEED_ONLY, OPT_freorder_blocks_algorithm_, NULL,
      REORDER_BLOCKS_ALGORITHM_STC },
    { OPT_LEVELS_2_PLUS_SPEED_ONLY, OPT_ftree_loop_vectorize, NULL, 1 },
    { OPT_LEVELS_2_PLUS_SPEED_ONLY, OPT_ftree_slp_vectorize, NULL, 1 },
    { OPT_LEVELS_2_PLUS_SPEED_ONLY, OPT_fopenmp_target_simd_clone_, NULL,
      OMP_TARGET_SIMD_CLONE_NOHOST },
//#ifdef INSN_SCHEDULING
  /* Only run the pre-regalloc scheduling pass if optimizing for speed.  */
 //   { OPT_LEVELS_2_PLUS_SPEED_ONLY, OPT_fschedule_insns, NULL, 1 },
//#endif

    /* -O3 and -Os optimizations.  */

    /* -O3 optimizations.  */
    { OPT_LEVELS_3_PLUS, OPT_fgcse_after_reload, NULL, 1 },
    { OPT_LEVELS_3_PLUS, OPT_fipa_cp_clone, NULL, 1 },
    { OPT_LEVELS_3_PLUS, OPT_floop_interchange, NULL, 1 },
    { OPT_LEVELS_3_PLUS, OPT_floop_unroll_and_jam, NULL, 1 },
    { OPT_LEVELS_3_PLUS, OPT_fpeel_loops, NULL, 1 },
    { OPT_LEVELS_3_PLUS, OPT_fpredictive_commoning, NULL, 1 },
    { OPT_LEVELS_3_PLUS, OPT_fsplit_loops, NULL, 1 },
    { OPT_LEVELS_3_PLUS, OPT_fsplit_paths, NULL, 1 },
    { OPT_LEVELS_3_PLUS, OPT_ftree_loop_distribution, NULL, 1 },
    { OPT_LEVELS_3_PLUS, OPT_ftree_partial_pre, NULL, 1 },
    { OPT_LEVELS_3_PLUS, OPT_funswitch_loops, NULL, 1 },
    { OPT_LEVELS_3_PLUS, OPT_fvect_cost_model_, NULL, VECT_COST_MODEL_DYNAMIC },
    { OPT_LEVELS_3_PLUS, OPT_fversion_loops_for_strides, NULL, 1 },

    /* -O3 parameters.  */
    { OPT_LEVELS_3_PLUS, OPT__param_max_inline_insns_auto_, NULL, 30 },
    { OPT_LEVELS_3_PLUS, OPT__param_early_inlining_insns_, NULL, 14 },
    { OPT_LEVELS_3_PLUS, OPT__param_inline_heuristics_hint_percent_, NULL, 600 },
    { OPT_LEVELS_3_PLUS, OPT__param_inline_min_speedup_, NULL, 15 },
    { OPT_LEVELS_3_PLUS, OPT__param_max_inline_insns_single_, NULL, 200 },

    /* -Ofast adds optimizations to -O3.  */
    { OPT_LEVELS_FAST, OPT_ffast_math, NULL, 1 },
    { OPT_LEVELS_FAST, OPT_fallow_store_data_races, NULL, 1 },
    { OPT_LEVELS_FAST, OPT_fsemantic_interposition, NULL, 0 },

    { OPT_LEVELS_NONE, 0, NULL, 0 }
  };

//const unsigned int cl_enums_count = 59;

static int   targetSoftStack_cb(MtcsOptions *mtcsOptions);
//原型  init_options_struct opts.h opts.cc
static void initOptionsStruct_cb(MtcsOptions *mtcsOptions,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set);
//原型  void (*init_options_struct) (struct gcc_options *opts); langhooks.h  #define LANG_HOOKS_INIT_OPTIONS_STRUCT lto_init_options_struct
static void hooksInitOptionsStruct_cb(MtcsOptions *mtcsOptions);
//原型  lang_hooks.init_options (save_decoded_options_count, save_decoded_options); #define LANG_HOOKS_INIT_OPTIONS        lhd_init_options
static void hooksInitOptions_cb(MtcsOptions *mtcsOptions,unsigned int decoded_options_count,struct cl_decoded_option *decoded_options);
//原型 lang_hooks.complain_wrong_lang_p (option) #define LANG_HOOKS_COMPLAIN_WRONG_LANG_P lto_complain_wrong_lang_p
static bool hooksComplainWrongLangP_cb(MtcsOptions *self,const struct cl_option *option);
//原型  lang_hooks.handle_option #define LANG_HOOKS_HANDLE_OPTION lto_handle_option
static bool hooksHandleOption_cb(MtcsOptions *self,size_t scode, const char *arg,
           HOST_WIDE_INT value ATTRIBUTE_UNUSED, int kind ATTRIBUTE_UNUSED, location_t loc ATTRIBUTE_UNUSED,
           const struct mtcs_cl_option_handlers *handlers ATTRIBUTE_UNUSED);
//设备的opt_code转在主机的opt_code
static int optcodeDeviceToHost_cb(MtcsOptions *self,int deviceOptcode);
//主机的opt_code转设备的opt_code
static int optcodeHostToevice_cb(MtcsOptions *self,int hostOptcode);
//原型 cl_optimization_save optons.h opitons-save.cc
static void  clOptimizationSave_cb(MtcsOptions *self,struct cl_optimization *ptr, MtcsOptionsItem *opts,MtcsOptionsItem *opts_set);
//原型 cl_optimization_restore options.h options-save.cc
static void clOptimizationRestore_cb(MtcsOptions *self,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,struct cl_optimization *ptr);

//原型 cl_target_option_hash options.h
static hashval_t clTargetOptionsHash_cb(MtcsOptions *self,MtcsClTargetOption const *ptr ATTRIBUTE_UNUSED);
//原型 bool cl_target_option_eq (struct cl_target_option const *ptr1 ATTRIBUTE_UNUSED, struct cl_target_option const *ptr2 ATTRIBUTE_UNUSED)
static bool  clTargetOptionsEq_cb(MtcsOptions *self,MtcsClTargetOption const *ptr1 ATTRIBUTE_UNUSED,
        MtcsClTargetOption const *ptr2 ATTRIBUTE_UNUSED);

//用mtcsoptions中的global_opts和global_opts_set设置主机中的global_opts global_opts_set
static void overrideHostOptions_cb(MtcsOptions *mtcsOptions,struct gcc_options *hostOpts,struct gcc_options *hostOptsSet);
//原型 common_handle_option_auto options.h options.cc
static bool commonHandleOptionAuto_cb(MtcsOptions *mtcsOptions,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,
        const struct cl_decoded_option *decoded,unsigned int lang_mask,
        int kind,location_t loc, const struct mtcs_cl_option_handlers *handlers, diagnostic_context *dc);

static MtcsOptionsItem *createItem_cb(MtcsOptions *mtcsOptions);
static void freeItem_cb(MtcsOptions *mtcsOptions,MtcsOptionsItem *item);

static void mtcsPtxOptionsInit(MtcsPtxOptions *self)
{
    MtcsOptions *mtcsOptions=(MtcsOptions *)self;
   // mtcsOptions->x_flag_ltrans=0;//原型  #define flag_ltrans global_options.x_flag_ltrans
    //options.h中定义
   // mtcsOptions->x_flag_ipa_reference_addressable=0;//原型  #define flag_ipa_reference_addressable global_options.x_flag_ipa_reference_addressable
   // mtcsOptions->x_flag_whole_program=0;            //原型 #define flag_whole_program global_options.x_flag_whole_program
    //ENABLE_EXTRA_CHECKING auto-host.h host=1 nvptx=1
  //  mtcsOptions->x_flag_checking=CHECKING_P ? ENABLE_EXTRA_CHECKING ? 2 : 1 : 0;//原型  #define flag_checking global_options.x_flag_checking
  //  mtcsOptions->x_flag_ipa_profile=0;//原型  #define flag_ipa_profile global_options.x_flag_ipa_profile
    //mtcsOptions->x_param_hot_bb_count_ws_permille=990;//原型   #define param_hot_bb_count_ws_permille global_options.x_param_hot_bb_count_ws_permille
  // mtcsOptions->x_flag_ipa_icf_variables=0; //原型 #define flag_ipa_icf_variables global_options.x_flag_ipa_icf_variables
    //mtcsOptions->x_flag_ipa_icf_functions=0; //原型  #define flag_ipa_icf_functions global_options.x_flag_ipa_icf_functions
   // mtcsOptions->x_flag_lto=NULL; //原型  #define flag_lto global_options.x_flag_lto
  //  mtcsOptions->x_flag_generate_offload=0;//原型  #define flag_generate_offload global_options.x_flag_generate_offload
   // mtcsOptions->x_flag_wpa=NULL;//原型 #define flag_wpa global_options.x_flag_wpa
    mtcsOptions->target_soft_stack=targetSoftStack_cb;
//    mtcsOptions->x_flag_sanitize=0; //原型  #define flag_sanitize global_options.x_flag_sanitize

    self->x_nvptx_softstack_size=128;//原型 #define nvptx_softstack_size global_options.x_nvptx_softstack_size
    self->x_nvptx_alias=0;//定义 options.cc =0

    //以下是 ipa-inline.cc中需要的选项值
   // mtcsOptions->x_flag_live_patching=0;
 //   mtcsOptions->x_flag_wrapv=0;
   // mtcsOptions->x_flag_trapv=0;
   // mtcsOptions->x_flag_pcc_struct_return=0;
   // mtcsOptions->x_flag_rounding_math=0;
   // mtcsOptions->x_flag_trapping_math=1;
    //mtcsOptions->x_flag_unsafe_math_optimizations=0;
    //mtcsOptions->x_flag_finite_math_only=0;
   // mtcsOptions->x_flag_signaling_nans=0;
 //   mtcsOptions->x_flag_cx_limited_range=0;
  //  mtcsOptions->x_flag_signed_zeros=1;
   // mtcsOptions->x_flag_associative_math=0;
  //  mtcsOptions->x_flag_reciprocal_math=0;
  //  mtcsOptions->x_flag_fp_int_builtin_inexact=1;
   // mtcsOptions->x_flag_errno_math=1;
  //  mtcsOptions->x_flag_non_call_exceptions=0;
   // mtcsOptions->x_flag_devirtualize=1;
   // mtcsOptions->x_flag_lto=0;
  //  mtcsOptions->x_flag_auto_profile=0;
   // mtcsOptions->x_flag_inline_small_functions=1;
  //  mtcsOptions->x_flag_inline_functions=1;
   // mtcsOptions->x_flag_inline_functions_called_once=1;
  //  mtcsOptions->x_flag_guess_branch_prob=1;
 //   mtcsOptions->x_flag_branch_probabilities=0;
    //mtcsOptions->x_flag_checking=2;
 //   mtcsOptions->x_flag_indirect_inlining=1;
 //   mtcsOptions->x_flag_no_inline=0;
 //   mtcsOptions->x_flag_early_inlining=1;

    //原型  init_options_struct opts.h opts.cc
    mtcsOptions->init_options_struct=initOptionsStruct_cb;
    //原型  void (*init_options_struct) (struct gcc_options *opts); langhooks.h  #define LANG_HOOKS_INIT_OPTIONS_STRUCT lto_init_options_struct
    mtcsOptions->hooks_init_options_struct=hooksInitOptionsStruct_cb;
    //原型  lang_hooks.init_options (save_decoded_options_count, save_decoded_options); #define LANG_HOOKS_INIT_OPTIONS        lhd_init_options
    mtcsOptions->hooks_init_options=hooksInitOptions_cb;
    //原型 lang_hooks.complain_wrong_lang_p (option) #define LANG_HOOKS_COMPLAIN_WRONG_LANG_P lto_complain_wrong_lang_p
    mtcsOptions->hooks_complain_wrong_lang_p=hooksComplainWrongLangP_cb;
    //原型  lang_hooks.handle_option #define LANG_HOOKS_HANDLE_OPTION lto_handle_option
    mtcsOptions->hooks_handle_option=hooksHandleOption_cb;
    //设备的opt_code转在主机的opt_code
    mtcsOptions->optcode_device_to_host=optcodeDeviceToHost_cb;
    //主机的opt_code转设备的opt_code
    mtcsOptions->optcode_host_to_device=optcodeHostToevice_cb;
    //原型 cl_optimization_save optons.h options-save.cc
    mtcsOptions->cl_optimization_save=clOptimizationSave_cb;
    //原型 cl_optimization_restore options.h options-save.cc
    mtcsOptions->cl_optimization_restore=clOptimizationRestore_cb;
    //原型 cl_target_option_hash options.h
    mtcsOptions->cl_target_option_hash=clTargetOptionsHash_cb;
    //原型 bool cl_target_option_eq (struct cl_target_option const *ptr1 ATTRIBUTE_UNUSED, struct cl_target_option const *ptr2 ATTRIBUTE_UNUSED)
    mtcsOptions->cl_target_option_eq=clTargetOptionsEq_cb;
    //用mtcsoptions中的global_opts和global_opts_set设置主机中的global_opts global_opts_set
    mtcsOptions->override_host_options=overrideHostOptions_cb;
    //原型 common_handle_option_auto options.h options.cc
    mtcsOptions->common_handle_option_auto=commonHandleOptionAuto_cb;
    //创建MtcsOptionsItem 每个平台的mtcsOptonsItem都是MtcsOptionsItem的子类
    mtcsOptions->create_item=createItem_cb;
    mtcsOptions->free_item=freeItem_cb;

    //ptx_options_get_cl_option ptx_options_get_cl_count 声明在ptx-optionsitem.h 实现在ptx-options.c
    struct cl_option *clOptons=ptx_options_get_cl_option();
    int clOptionCount=ptx_options_get_cl_count();
    struct cl_enum *ptxClEnums = ptx_get_cl_enums();
    mtcs_options_set_cl_options(mtcsOptions,clOptons,clOptionCount);
    mtcs_options_set_cl_enum(mtcsOptions,ptxClEnums);
    int clLangCount=ptx_options_get_cl_lang_count();
    mtcs_options_set_cl_lang_count(mtcsOptions,clLangCount);
    PtxOptionsItem *options = n_slice_alloc0 (sizeof(PtxOptionsItem));
    PtxOptionsItem *options_set = n_slice_alloc0 (sizeof(PtxOptionsItem));
    mtcs_options_set_gcc_options_gcc_options_set(mtcsOptions,(MtcsOptionsItem*)options,(MtcsOptionsItem*)options_set);
    mtcs_options_set_default_options_table(mtcsOptions,default_options_table);//原型 default_options_table opts.cc
}

//原型#define TARGET_SOFT_STACK ((target_flags & MASK_SOFT_STACK) != 0) //来自平台options.h
static int targetSoftStack_cb(MtcsOptions *mtcsOptions)
{
    MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
    return mtcsOptionsItem->x_target_flags & PTX_MASK_SOFT_STACK;
}

//原型  init_options_struct opts.h opts.cc
static void initOptionsStruct_cb(MtcsOptions *mtcsOptions,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set)
{
   MtcsPtxOptions *self=(MtcsPtxOptions *)mtcsOptions;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   /* Initialize OPTS and OPTS_SET before using them in parsing options.  */
   /* Ensure that opts_obstack has already been initialized by the time
   that we initialize any gcc_options instances (PR jit/68446).  */
   gcc_assert (mtcsOptions->opts_obstack.chunk_size > 0);
   //initGlobalOptions(self);
   ptx_options_item_init((PtxOptionsItem*)opts);//ptx_options_item_init声明在ptx-optionsitem.h中 ptx-optionsitem.h由mtcsgenoptions.c生成
   if(opts_set)
      memset (opts_set, 0, sizeof (*opts_set));
   /* Initialize whether `char' is signed.  */
   opts->x_flag_signed_char = PTX_DEFAULT_SIGNED_CHAR/*!DEFAULT_SIGNED_CHAR*/;
   /* Set this to a special "uninitialized" value.  The actual default
   is set after target options have been processed.  */
   opts->x_flag_short_enums = 2;

   /* Initialize target_flags before default_options_optimization
   so the latter can modify it.  */
   opts->x_target_flags =mtcsMachine->common->default_target_flags/*! targetm_common.default_target_flags*/;

   /* Some targets have ABI-specified unwind tables.  */
   opts->x_flag_unwind_tables = mtcsMachine->common->unwind_tables_default/*!targetm_common.unwind_tables_default*/;

   /* Some targets have other target-specific initialization.  */
   target_common_option_init_struct/*!targetm_common.option_init_struct*/(mtcsMachine->common,opts);
}

//原型  void (*init_options_struct) (struct gcc_options *opts); langhooks.h  #define LANG_HOOKS_INIT_OPTIONS_STRUCT lto_init_options_struct
static void hooksInitOptionsStruct_cb(MtcsOptions *mtcsOptions)
{
   n_debug("mtcsptxoptions.c hooksInitOptionsStruct_cb\n");
   MtcsOptionsItem *opts=mtcsOptions->global_options;
   opts->x_flag_complex_method = 2;
   opts->x_flag_default_complex_method = opts->x_flag_complex_method;
}

//原型  lang_hooks.init_options (save_decoded_options_count, save_decoded_options); #define LANG_HOOKS_INIT_OPTIONS        lhd_init_options
static void hooksInitOptions_cb(MtcsOptions *mtcsOptions,unsigned int decoded_options_count,struct cl_decoded_option *decoded_options)
{
   n_debug("mtcsptxoptions.c lang_hooks.init_options (save_decoded_options_count, save_decoded_options); ptx 进入这里\n");
}

//原型 lang_hooks.complain_wrong_lang_p (option) #define LANG_HOOKS_COMPLAIN_WRONG_LANG_P lto_complain_wrong_lang_p
static bool hooksComplainWrongLangP_cb(MtcsOptions *mtcsOptions,const struct cl_option *option)
{
    return false;
}

static const char *resolutionFileName;//原型 resolution_file_name lto.h lto-lang.cc
//原型  lang_hooks.handle_option #define LANG_HOOKS_HANDLE_OPTION lto_handle_option
static bool hooksHandleOption_cb(MtcsOptions *mtcsOptions,size_t scode, const char *arg,
           HOST_WIDE_INT value ATTRIBUTE_UNUSED, int kind ATTRIBUTE_UNUSED, location_t loc ATTRIBUTE_UNUSED,
           const struct mtcs_cl_option_handlers *handlers ATTRIBUTE_UNUSED)
{
   n_debug("mtcsptxoptions.c lang_hooks.handle_option #define LANG_HOOKS_HANDLE_OPTION lto_handle_option nvptx进这里 optcode:%d\n",scode);
   n_debug("mtcsptxoptions.c lto_handle_option OPT_fresolution_ :%d %d\n",OPT_fresolution_,PTX_OPT_fresolution_);
   n_debug("mtcsptxoptions.c lto_handle_option OPT_Wabi :%d %d\n",OPT_Wabi,PTX_OPT_Wabi);
   n_debug("mtcsptxoptions.c lto_handle_option OPT_fwpa :%d %d\n",OPT_fwpa,PTX_OPT_fwpa);
  enum opt_code code = (enum opt_code) scode;
  bool result = true;
  int hostOptcode=mtcs_options_optcode_device_to_host(mtcsOptions,scode);
  switch (hostOptcode/*!code*/){
    case OPT_fresolution_:
        resolutionFileName = arg;
      break;
    case OPT_Wabi:
      mtcsOptions->global_options->x_warn_psabi/*!warn_psabi*/ = value;
      break;
    case OPT_fwpa:
        mtcsOptions->global_options->x_flag_wpa/*!flag_wpa*/ = value ? "" : NULL;
      break;

    default:
      break;
  }
  return result;
}


//设备的opt_code转在主机的opt_code
static int optcodeDeviceToHost_cb(MtcsOptions *self,int deviceOptcode)
{
    return ptx_options_optcode_device_to_host(deviceOptcode);
}

//主机的opt_code转设备的opt_code
static int optcodeHostToevice_cb(MtcsOptions *self,int hostOptcode)
{
    return ptx_options_optcode_host_to_device(hostOptcode);
}

//原型 cl_optimization_save optons.h
static void  clOptimizationSave_cb(MtcsOptions *self,struct cl_optimization *ptr, MtcsOptionsItem *opts,MtcsOptionsItem *opts_set)
{
    ptx_cl_optimization_save(ptr,opts,opts_set);
}

//原型 cl_optimization_restore options.h options-save.cc
static void clOptimizationRestore_cb(MtcsOptions *mtcsOptions,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,struct cl_optimization *ptr)
{
    MtcsPtxOptions *self=(MtcsPtxOptions *)mtcsOptions;
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsOptionsItem *global=mtcsOptions->global_options;
    n_debug("mtcsptxoptions.c clOptimizationRestore_cb 00 flag_ipa_profile:%d opts:%p opts_set:%p  global:%d %p\n",
            opts->x_flag_ipa_profile,opts,opts_set,global->x_flag_ipa_profile,global);
    ptx_cl_optimization_restore(opts,opts_set,ptr);
    n_debug("mtcsptxoptions.c clOptimizationRestore_cb 11 flag_ipa_profile:%d opts:%p opts_set:%p :%d %p\n",
            opts->x_flag_ipa_profile,opts,opts_set,global->x_flag_ipa_profile,global);
    mtcsTarget/*!targetm.override_options_after_change*/->override_options_after_change(mtcsTarget);

}

//原型 cl_target_option_hash options.h
static hashval_t clTargetOptionsHash_cb(MtcsOptions *self,MtcsClTargetOption const *ptr ATTRIBUTE_UNUSED)
{
    inchash::hash hstate;
    hstate.add_hwi (ptr->x_target_flags);
    hstate.add_hwi (ptr->explicit_mask_target_flags);
    return hstate.end ();
}


static bool  clTargetOptionsEq_cb(MtcsOptions *self,MtcsClTargetOption const *ptr1 ATTRIBUTE_UNUSED,
        MtcsClTargetOption const *ptr2 ATTRIBUTE_UNUSED)
{
    if (ptr1->x_target_flags != ptr2->x_target_flags)
      return false;
    if (ptr1->explicit_mask_target_flags != ptr2->explicit_mask_target_flags)
      return false;
    return true;
}

//用mtcsoptions中的global_opts和global_opts_set设置主机中的global_opts global_opts_set
static void overrideHostOptions_cb(MtcsOptions *mtcsOptions,struct gcc_options *hostOpts,struct gcc_options *hostOptsSet)
{
    MtcsOptionsItem *deviceOpts=mtcsOptions->global_options;
    MtcsOptionsItem *deviceOptsSet=mtcsOptions->global_options_set;
    ptx_options_set_gcc_options(deviceOpts,deviceOptsSet,hostOpts,hostOptsSet);
}

//转主机的optcode到设备的optcode
#define HOST2DEV(OPTCODE) mtcs_options_optcode_host_to_device(mtcsOptions,OPTCODE)

//原型 common_handle_option_auto options.h options.cc
static bool commonHandleOptionAuto_cb(MtcsOptions *mtcsOptions,MtcsOptionsItem *opts,MtcsOptionsItem *opts_set,
        const struct cl_decoded_option *decoded,unsigned int lang_mask,
        int kind,location_t loc, const struct mtcs_cl_option_handlers *handlers, diagnostic_context *dc)
{
    MtcsPtxOptions *self=(MtcsPtxOptions *)mtcsOptions;
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsOpts *mtcsOpts=mtcs_target_get_opts(mtcsTarget);

    size_t scode = decoded->opt_index;
    HOST_WIDE_INT value = decoded->value;
    enum opt_code code = (enum opt_code) scode;
    gcc_assert (decoded->canonical_option_num_elements <= 2);

    int hostOptcode=mtcs_options_optcode_device_to_host(mtcsOptions,scode);

    switch (hostOptcode/*!code*/){
        case OPT_Wextra:
            if (!opts_set->x_warn_absolute_value)
                mtcs_opts_handle_generated_option/*!handle_generated_option*/(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wabsolute_value), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_cast_function_type)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wcast_function_type), NULL, value,
                        lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_clobbered)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wclobbered), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_empty_body)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wempty_body), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_cpp_warn_expansion_to_defined)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wexpansion_to_defined), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_ignored_qualifiers)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wignored_qualifiers), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_missing_field_initializers)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wmissing_field_initializers), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_missing_parameter_type)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wmissing_parameter_type), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_old_style_declaration)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wold_style_declaration), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_override_init)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Woverride_init), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_sign_compare)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wsign_compare), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_sized_deallocation)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wsized_deallocation), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_type_limits)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wtype_limits), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_uninitialized)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wuninitialized), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_unused_but_set_parameter)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wunused_but_set_parameter), NULL, (opts->x_warn_unused && opts->x_extra_warnings),
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_unused_parameter)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wunused_parameter), NULL, (opts->x_warn_unused && opts->x_extra_warnings),
                            lang_mask, kind, loc, handlers, true, dc);
            break;

        case OPT_Wpedantic:
            if (!opts_set->x_cpp_warn_expansion_to_defined)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wexpansion_to_defined), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            break;

            case OPT_Wuninitialized:
            if (!opts_set->x_warn_maybe_uninitialized)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wmaybe_uninitialized), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            break;

        case OPT_Wshadow:
            if (!opts_set->x_warn_shadow_ivar)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wshadow_ivar), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_shadow_local)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wshadow_local), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            break;

            case OPT_Wshadow_local:
            if (!opts_set->x_warn_shadow_compatible_local)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wshadow_compatible_local), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            break;

        case OPT_Wunused:
            if (!opts_set->x_warn_unused_but_set_parameter)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wunused_but_set_parameter), NULL, (opts->x_warn_unused && opts->x_extra_warnings),
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_unused_but_set_variable)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wunused_but_set_variable), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_unused_function)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wunused_function), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_unused_label)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wunused_label), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_unused_local_typedefs)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wunused_local_typedefs), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_unused_parameter)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wunused_parameter), NULL, (opts->x_warn_unused && opts->x_extra_warnings),
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_unused_value)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wunused_value), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_warn_unused_variable)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_Wunused_variable), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            break;

        case OPT_fnon_call_exceptions:
            if (!opts_set->x_flag_exceptions)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_fexceptions), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            break;

        case OPT_funroll_loops:
            if (!opts_set->x_flag_rename_registers)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_frename_registers), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_flag_web)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_fweb), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            break;

        case OPT_ftree_vectorize:
            if (!opts_set->x_flag_tree_loop_vectorize)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_ftree_loop_vectorize), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            if (!opts_set->x_flag_tree_slp_vectorize)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_ftree_slp_vectorize), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            break;

        case OPT_funroll_all_loops:
            if (!opts_set->x_flag_unroll_loops)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_funroll_loops), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            break;

        case OPT_fvar_tracking_uninit:
            if (!opts_set->x_flag_var_tracking)
                mtcs_opts_handle_generated_option(mtcsOpts,opts, opts_set,
                        HOST2DEV(OPT_fvar_tracking), NULL, value,
                            lang_mask, kind, loc, handlers, true, dc);
            break;

        default:
            break;
    }
    return true;
}

static MtcsOptionsItem *createItem_cb(MtcsOptions *mtcsOptions)
{
    return (MtcsOptionsItem*)n_slice_alloc0 (sizeof(PtxOptionsItem));
}

static void freeItem_cb(MtcsOptions *mtcsOptions,MtcsOptionsItem *item)
{
     n_slice_free(PtxOptionsItem,(PtxOptionsItem*)item);
}


MtcsPtxOptions *mtcs_ptx_options_new(MtcsMode *mtcsMode)
{
     MtcsPtxOptions *self = n_slice_alloc0 (sizeof(MtcsPtxOptions));
     mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
     mtcs_options_init((MtcsOptions*)self);
     mtcsPtxOptionsInit(self);
     return self;
}


