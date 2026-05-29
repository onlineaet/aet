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
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "alloc-pool.h"
#include "timevar.h"
#include "memmodel.h"
#include "tm_p.h"
#include "optabs-libfuncs.h"
#include "insn-config.h"
#include "ira.h"
#include "recog.h"
#include "cgraph.h"
#include "coverage.h"
#include "diagnostic.h"
#include "varasm.h"
#include "tree-inline.h"
#include "realmpfr.h"   /* For GMP/MPFR/MPC versions, in print_version.  */
#include "version.h"
#include "flags.h"
#include "insn-attr.h"
#include "output.h"
#include "toplev.h"
#include "expr.h"
#include "intl.h"
#include "tree-diagnostic.h"
#include "reload.h"
#include "lra.h"
#include "dwarf2asm.h"
#include "debug.h"
#include "common/common-target.h"
#include "langhooks.h"
#include "cfgloop.h" /* for init_set_costs */
#include "hosthooks.h"
#include "opts.h"
#include "opts-diagnostic.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "tsan.h"
#include "plugin.h"
#include "context.h"
#include "pass_manager.h"
#include "auto-profile.h"
#include "dwarf2out.h"
#include "ipa-reference.h"
#include "symbol-summary.h"
#include "tree-vrp.h"
#include "sreal.h"
#include "ipa-cp.h"
#include "ipa-prop.h"
#include "ipa-utils.h"
#include "gcse.h"
#include "omp-offload.h"
#include "edit-context.h"
#include "tree-pass.h"
#include "dumpfile.h"
#include "ipa-fnsummary.h"
#include "dump-context.h"
#include "print-tree.h"
#include "optinfo-emit-json.h"
#include "ipa-modref-tree.h"
#include "ipa-modref.h"
#include "ipa-param-manipulation.h"
#include "dbgcnt.h"
#include "gcc-urlifier.h"
#include "value-prof.h"

#include "tree-pass.h"
#include "stringpool.h"
#include "gimple-ssa.h"
#include "cgraph.h"
#include "coverage.h"
#include "lto-streamer.h"
#include "fold-const.h"
#include "varasm.h"
#include "stor-layout.h"
#include "output.h"
#include "cfgcleanup.h"
#include "gimple-iterator.h"
#include "gimple-fold.h"
#include "gimplify.h"
#include "gimplify-me.h"
#include "tree-cfg.h"
#include "tree-into-ssa.h"
#include "tree-ssa.h"
#include "langhooks.h"
#include "toplev.h"
#include "debug.h"
#include "symbol-summary.h"
#include "tree-vrp.h"
#include "sreal.h"
#include "ipa-cp.h"
#include "ipa-prop.h"
#include "gimple-pretty-print.h"
#include "plugin.h"
#include "ipa-fnsummary.h"
#include "ipa-utils.h"
#include "except.h"
#include "cfgloop.h"
#include "context.h"
#include "pass_manager.h"
#include "tree-nested.h"
#include "dbgcnt.h"
#include "lto-section-names.h"
#include "stringpool.h"
#include "attribs.h"
#include "ipa-inline.h"
#include "omp-offload.h"
#include "symtab-thunks.h"
#include "ipa-reference.h"
#include "ipa-modref-tree.h"
#include "ipa-modref.h"
#include "cfganal.h"
#include "tree-ssa-operands.h"
#include "gimple-ssa.h"
#include "symtab-clones.h"
#include "tree-phinodes.h"
#include "tree-ssa-operands.h"
#include "ssa-iterators.h"
#include "cfghooks.h"

#include "mtcsadjustpass.h"
#include "mtcstarget.h"
#include "mtcscompile.h"
#include "../aetprinttree.h"
#include "../aetprintgimple.h"
#include "../mtcsinfo.h"

static unsigned int  proectedStackSaveStmt(function *fun);


static void mtcsAdjustInitInit(MtcsAdjustPass *self)
{

}

///**
// * 在target-hooks-def.h中定义的关于向量的目标函数
// * #define TARGET_VECTORIZE \
//  { \
//    TARGET_VECTORIZE_BUILTIN_MASK_FOR_LOAD, 只有 rs6000有定义，其它平台都是空\
//    TARGET_VECTORIZE_BUILTIN_VECTORIZED_FUNCTION, gcn i386 mips rs6000有定义 nvptx=default_builtin_vectorized_function\
//    TARGET_VECTORIZE_BUILTIN_MD_VECTORIZED_FUNCTION, 只有 rs6000有定义 其它平台=default_builtin_md_vectorized_function \
//    TARGET_VECTORIZE_BUILTIN_VECTORIZATION_COST, aarch64 arm gcn i386 loongarch riscv rs6000 s390有定义 nvptx=default_builtin_vectorization_cost\
//    TARGET_VECTORIZE_PREFERRED_VECTOR_ALIGNMENT, aarch64 gcn riscv nvptx=default_preferred_vector_alignment \
//    \
//    TARGET_VECTORIZE_PREFERRED_DIV_AS_SHIFTS_OVER_MULT, 只有aarch64有定义  nvptx=default_preferred_div_as_shifts_over_mult\
//    TARGET_VECTORIZE_VECTOR_ALIGNMENT_REACHABLE, aarch64 arm epiphany,gcn,rs6000 有定义 nvptx=default_builtin_vector_alignment_reachable\
//    TARGET_VECTORIZE_VEC_PERM_CONST, aarch64 arm gcn i386 ia64 loongarch mips riscv rs6000 s390 sparc有定义 nvptx=NULL \
//    TARGET_VECTORIZE_SUPPORT_VECTOR_MISALIGNMENT, aarch64 arm epiphany gcn loongarch riscv rs6000 s390有定义 \
//                                                                      nvptx=default_builtin_support_vector_misalignment\
//    TARGET_VECTORIZE_PREFERRED_SIMD_MODE, 很多平台都有定义 nvptx=nvptx_preferred_simd_mode\
//    \
//    TARGET_VECTORIZE_SPLIT_REDUCTION, 只有i386有定义 nvptx=default_split_reduction \
//    TARGET_VECTORIZE_AUTOVECTORIZE_VECTOR_MODES,aarch64 arc arm gcn i386 loongarch mips riscv 有定义 nvptx=default_autovectorize_vector_modes \
//    TARGET_VECTORIZE_RELATED_MODE, aarch64 gcn riscv 有定义 nvptx=default_vectorize_related_mode\
//    TARGET_VECTORIZE_GET_MASK_MODE,aarch64 arm gcn i386 riscv sparc 有定义 nvptx= default_get_mask_mode \
//    TARGET_VECTORIZE_CONDITIONAL_OPERATION_IS_EXPENSIVE, 只有 aarch64有定义 nvptx=default_conditional_operation_is_expensive\
//    \
//    TARGET_VECTORIZE_EMPTY_MASK_IS_EXPENSIVE,aarch64 riscv 有定义 nvptx=default_empty_mask_is_expensive \
//    TARGET_VECTORIZE_BUILTIN_GATHER, 只有i386有定义 nptx=NULL\
//    TARGET_VECTORIZE_BUILTIN_SCATTER, 只有i386有定义 nptx=NULL\
//    TARGET_VECTORIZE_CREATE_COSTS, aarch64 i386 loongarch riscv rs6000有定义 nvptx=default_vectorize_create_costs\
//  }
//
//  #define TARGET_INITIALIZER \
//    ...
//    TARGET_VECTOR_MODE_SUPPORTED_P, nvptx=nvptx_vector_mode_supported \
//    TARGET_VECTOR_MODE_SUPPORTED_ANY_TARGET_P, aarch64 riscv 有定义 nvptx=hook_bool_mode_true\
//    TARGET_COMPATIBLE_VECTOR_TYPES_P, 只有 aarch64有定义 nvptx=hook_bool_const_tree_const_tree_true\
//    TARGET_VECTOR_ALIGNMENT, nvptx=nvptx_vector_alignment\
//    ...
// */
//
//typedef struct _BackTargetVectorize
//{
//   void *builtin_mask_for_load;
//   void *builtin_vectorized_function;
//   void *builtin_md_vectorized_function;
//   void *builtin_vectorization_cost;
//   void *preferred_vector_alignment;
//
//   void *preferred_div_as_shifts_over_mult;
//   void *vector_alignment_reachable;
//   void *vec_perm_const;
//   void *support_vector_misalignment;
//   void *preferred_simd_mode;
//
//   void *split_reduction;
//   void *autovectorize_vector_modes;
//   void *related_mode;
//   void *get_mask_mode;
//   void *conditional_operation_is_expensive;
//
//   void *empty_mask_is_expensive;
//   void *builtin_gather;
//   void *builtin_scatter;
//   void *create_costs;
//
//   void *vector_mode_supported_p;
//   void *vector_mode_supported_any_target_p;
//   void *compatible_vector_types_p;
//   void *vector_alignment;
//
//}BackTargetVectorize
//
//static void backTarget()
//{
//  void *daa[]={
//        (void*)targetm.vectorize.builtin_mask_for_load,
//        (void*)targetm.vectorize.builtin_vectorized_function,
//        (void*)targetm.vectorize.builtin_md_vectorized_function,
//        (void*)targetm.vectorize.builtin_vectorization_cost,
//        (void*)targetm.vectorize.preferred_vector_alignment,
//
//        (void*)targetm.vectorize.preferred_div_as_shifts_over_mult,
//        (void*)targetm.vectorize.vector_alignment_reachable,
//        (void*)targetm.vectorize.vec_perm_const,
//        (void*)targetm.vectorize.support_vector_misalignment,
//        (void*)targetm.vectorize.preferred_simd_mode,
//
//        (void*)targetm.vectorize.split_reduction,
//        (void*)targetm.vectorize.autovectorize_vector_modes,
//        (void*)targetm.vectorize.related_mode,
//        (void*)targetm.vectorize.get_mask_mode,
//        (void*)targetm.vectorize.conditional_operation_is_expensive,
//
//        (void*)targetm.vectorize.empty_mask_is_expensive,
//        (void*)targetm.vectorize.builtin_gather,
//        (void*)targetm.vectorize.builtin_scatter,
//        (void*)targetm.vectorize.create_costs,
//
//        (void*)targetm.vector_mode_supported_p,
//        (void*)targetm.vector_mode_supported_any_target_p,
//        (void*)targetm.compatible_vector_types_p,
//        (void*)targetm.vector_alignment,
//  };
//}

typedef struct _BackVect
{
   int  back_flag_tree_loop_vectorize;
   bool  has_force_vectorize_loops;
}BackVect;

static BackVect backVect;

static unsigned int back_flag_tree_slp_vectorize=0;

static bool commonGate(function *fun)
{
   tree decl=fun->decl;
   MtcsFuncType mtcsType = mtcs_info_get_func_type(decl);
   //char *name=IDENTIFIER_POINTER(DECL_NAME(decl));
   //fprintf(stderr,"mtcsadjustpass.c commonGate mtcsType:%d name:%s\n",mtcsType,name);
   return mtcsType!=MTCS_FUNC_NOT;
}

/**
 * 收集MTCS函数和变量的两个pass
 */
namespace
{
   const pass_data pass_data_mtcs_back_vect =
   {
      GIMPLE_PASS, /* type */
      "mtcs_back_vect", /* name */
      OPTGROUP_NONE, /* optinfo_flags */
      TV_NONE, /* tv_id */
      0, /* properties_required */
      0, /* properties_provided */
      0, /* properties_destroyed */
      0, /* todo_flags_start */
      0, /* todo_flags_finish */
   };

   class pass_mtcs_back_vect : public gimple_opt_pass
   {
   public:
      pass_mtcs_back_vect (gcc::context *ctxt)
      : gimple_opt_pass (pass_data_mtcs_back_vect, ctxt)
      {
      }

      bool gate (function *fun)
      {
         return commonGate(fun);
      }

      /* opt_pass methods: */
      unsigned int execute (function *fun) final override
      {
         tree decl=fun->decl;
         MtcsFuncType mtcsType = mtcs_info_get_func_type(decl);
         if(mtcsType==MTCS_FUNC_NOT)
            return 0;
         backVect.back_flag_tree_loop_vectorize=flag_tree_loop_vectorize;
         backVect.has_force_vectorize_loops=fun->has_force_vectorize_loops;
        // printf("mtcs_back_vect --- %d %d\n",flag_tree_loop_vectorize,fun->has_force_vectorize_loops);
         flag_tree_loop_vectorize=false;
         fun->has_force_vectorize_loops=false;
         return 0;
      }
   };

   const pass_data pass_data_mtcs_restore_vect =
   {
      GIMPLE_PASS, /* type */
      "mtcs_restore_vect", /* name */
      OPTGROUP_NONE, /* optinfo_flags */
      TV_NONE, /* tv_id */
      0, /* properties_required */
      0, /* properties_provided */
      0, /* properties_destroyed */
      0, /* todo_flags_start */
      0, /* todo_flags_finish */
   };

   class pass_mtcs_restore_vect : public gimple_opt_pass
   {
   public:
      pass_mtcs_restore_vect (gcc::context *ctxt)
      : gimple_opt_pass (pass_data_mtcs_restore_vect, ctxt)
      {
      }

      bool gate (function *fun)
      {
         return commonGate(fun);
      }


      /* opt_pass methods: */
      unsigned int execute (function *fun) final override
      {
         tree decl=fun->decl;
         MtcsFuncType mtcsType = mtcs_info_get_func_type(decl);
         if(mtcsType==MTCS_FUNC_NOT)
            return 0;
         //printf("mtcs_restore_vect --- %d %d\n",flag_tree_loop_vectorize,fun->has_force_vectorize_loops);
         flag_tree_loop_vectorize= backVect.back_flag_tree_loop_vectorize;
         fun->has_force_vectorize_loops=  backVect.has_force_vectorize_loops;
         return 0;
      }
   };

   //---------------靠加入section 禁用fnsplit -------------------

   const pass_data pass_data_mtcs_no_fnsplit =
   {
      GIMPLE_PASS, /* type */
      "mtcs_no_fnsplit", /* name */
      OPTGROUP_NONE, /* optinfo_flags */
      TV_NONE, /* tv_id */
      0, /* properties_required */
      0, /* properties_provided */
      0, /* properties_destroyed */
      0, /* todo_flags_start */
      0, /* todo_flags_finish */
   };

   class pass_mtcs_no_fnsplit : public gimple_opt_pass
   {
   public:
      pass_mtcs_no_fnsplit (gcc::context *ctxt)
      : gimple_opt_pass (pass_data_mtcs_no_fnsplit, ctxt)
      {
      }
      bool gate (function *fun)
      {
         return commonGate(fun);
      }

      /* opt_pass methods: */
      unsigned int execute (function *fun) final override
      {
         tree decl=fun->decl;
         MtcsFuncType mtcsType = mtcs_info_get_func_type(decl);
         if(mtcsType==MTCS_FUNC_NOT)
            return 0;
         tree att=lookup_attribute ("section", DECL_ATTRIBUTES (decl));
         //printf("mtcs_no_fnsplit %p %s\n",att,IDENTIFIER_POINTER(DECL_NAME(decl)));

         if(att){
            //已经有section了，fnsplit不再执行，也不用加新的
         }else{
            DECL_ATTRIBUTES (decl) = tree_cons (get_identifier ("section"),
                  build_tree_list (NULL_TREE, build_string (5, "mtcs")), DECL_ATTRIBUTES (decl));
         }
         return 0;
      }
   };

   //---------------移走在 pass 中加入的 section -------------------
   const pass_data pass_data_mtcs_remove_section =
   {
      GIMPLE_PASS, /* type */
      "mtcs_remove_section", /* name */
      OPTGROUP_NONE, /* optinfo_flags */
      TV_NONE, /* tv_id */
      0, /* properties_required */
      0, /* properties_provided */
      0, /* properties_destroyed */
      0, /* todo_flags_start */
      0, /* todo_flags_finish */
   };

   class pass_mtcs_remove_section : public gimple_opt_pass
   {
   public:
      pass_mtcs_remove_section (gcc::context *ctxt)
      : gimple_opt_pass (pass_data_mtcs_remove_section, ctxt)
      {
      }

      bool gate (function *fun){
         return commonGate(fun);
      }

      /* opt_pass methods: */
      unsigned int execute (function *fun) final override
      {
         tree decl=fun->decl;
         MtcsFuncType mtcsType = mtcs_info_get_func_type(decl);
         if(mtcsType==MTCS_FUNC_NOT)
         return 0;
         tree att=lookup_attribute ("section", DECL_ATTRIBUTES (decl));
        // printf("pass_mtcs_remove_section %p %s\n",att,IDENTIFIER_POINTER(DECL_NAME(decl)));
         if(att){
            tree value=TREE_VALUE (att) ;
            if(value && TREE_CODE(value)==TREE_LIST){
               tree mt=TREE_VALUE (value) ;
               if(mt && TREE_CODE(mt)==STRING_CST){
                  char *tag= TREE_STRING_POINTER (mt);
                  if(strcmp(tag,"mtcs")==0){
                     //printf("移走 section----11 %s\n",IDENTIFIER_POINTER(DECL_NAME(fun->decl)));
                     DECL_ATTRIBUTES (decl)=remove_attribute("section",DECL_ATTRIBUTES (decl));
                  }
               }
            }
         }
         return 0;
      }
   };

   //---------------------禁用 gimple pass "slp"---------------------
   const pass_data pass_data_mtcs_disable_slp =
   {
      GIMPLE_PASS, /* type */
      "mtcs_disable_slp", /* name */
      OPTGROUP_NONE, /* optinfo_flags */
      TV_NONE, /* tv_id */
      0, /* properties_required */
      0, /* properties_provided */
      0, /* properties_destroyed */
      0, /* todo_flags_start */
      0, /* todo_flags_finish */
   };

   class pass_mtcs_disable_slp : public gimple_opt_pass
   {
   public:
      pass_mtcs_disable_slp (gcc::context *ctxt)
      : gimple_opt_pass (pass_data_mtcs_disable_slp, ctxt)
      {
      }
      opt_pass * clone () final override { return new pass_mtcs_disable_slp (m_ctxt); }

      bool gate (function *fun)
      {
         return commonGate(fun);
      }

      /* opt_pass methods: */
      unsigned int execute (function *fun) final override
      {
         tree decl=fun->decl;
         MtcsFuncType mtcsType = mtcs_info_get_func_type(decl);
         if(mtcsType==MTCS_FUNC_NOT)
            return 0;
         back_flag_tree_slp_vectorize=flag_tree_slp_vectorize;
         flag_tree_slp_vectorize=0;
         //fprintf(stderr,"pass_mtcs_disable_slp --- %d %d\n",back_flag_tree_slp_vectorize);
         return 0;
      }
   };

   //---------------------禁用 gimple pass "slp"---------------------
   const pass_data pass_data_mtcs_enable_slp =
   {
      GIMPLE_PASS, /* type */
      "mtcs_enable_slp", /* name */
      OPTGROUP_NONE, /* optinfo_flags */
      TV_NONE, /* tv_id */
      0, /* properties_required */
      0, /* properties_provided */
      0, /* properties_destroyed */
      0, /* todo_flags_start */
      0, /* todo_flags_finish */
   };

   class pass_mtcs_enable_slp : public gimple_opt_pass
   {
   public:
      pass_mtcs_enable_slp (gcc::context *ctxt)
      : gimple_opt_pass (pass_data_mtcs_enable_slp, ctxt)
      {
      }
      opt_pass * clone () final override { return new pass_mtcs_enable_slp (m_ctxt); }

      bool gate (function *fun)
      {
         return commonGate(fun);
      }

      /* opt_pass methods: */
      unsigned int execute (function *fun) final override
      {
         tree decl=fun->decl;
         MtcsFuncType mtcsType = mtcs_info_get_func_type(decl);
         if(mtcsType==MTCS_FUNC_NOT)
            return 0;
         flag_tree_slp_vectorize=back_flag_tree_slp_vectorize;
         return 0;
      }
   };


   const pass_data pass_data_protect_stack_save =
   {
     GIMPLE_PASS, /* type */
     "protectstacksave", /* name */
     OPTGROUP_NONE, /* optinfo_flags */
     TV_NONE, /* tv_id */
     ( PROP_cfg | PROP_ssa ), /* properties_required */
     0, /* properties_provided */
     0, /* properties_destroyed */
     0, /* todo_flags_start */
     TODO_update_ssa, /* todo_flags_finish */
   };

   class pass_protect_stack_save : public gimple_opt_pass
   {
   public:
      pass_protect_stack_save (gcc::context *ctxt)
       : gimple_opt_pass (pass_data_protect_stack_save, ctxt)
     {}

      bool gate (function *fun)
      {
          return commonGate(fun);
      }

     /* opt_pass methods: */
     unsigned int execute (function *fun) {
        return proectedStackSaveStmt(fun);
     }
   }; // class pass_fold_builtins
} // anon namespace


/**
 * 禁用 gimple pass "vect"
 */
void mtcs_adjust_pass_disable_vect_pass(MtcsAdjustPass *self)
{
   //加在pass "vect"前 "ifcvt"前解决BUG bug 038
    gimple_opt_pass *back = new pass_mtcs_back_vect (g);
    //register_pass_info backInfo = { back, "vect", 1, PASS_POS_INSERT_BEFORE };
    register_pass_info backInfo = { back, "ifcvt", 1, PASS_POS_INSERT_BEFORE };
    register_pass (&backInfo);

    gimple_opt_pass *restore = new pass_mtcs_restore_vect (g);
    register_pass_info restoreInfo = { restore, "vect", 1, PASS_POS_INSERT_AFTER };
    register_pass (&restoreInfo);
}

/**
 * 禁用 gimple pass "fnsplit"
 */
void mtcs_adjust_pass_disable_fnsplit_pass(MtcsAdjustPass *self)
{
   //加在pass "fnsplit"前 禁用了fnsplit
   gimple_opt_pass *mtcsNoFnSplit = new pass_mtcs_no_fnsplit (g);
   register_pass_info nofnSplitInfo = { mtcsNoFnSplit, "fnsplit", 1, PASS_POS_INSERT_BEFORE };
   register_pass (&nofnSplitInfo);
   //加在pass "fnsplit"后 恢复设置
   gimple_opt_pass *mtcsRemoveSection = new pass_mtcs_remove_section (g);
   register_pass_info removeSectionInfo = { mtcsRemoveSection, "fnsplit", 1, PASS_POS_INSERT_AFTER };
   register_pass (&removeSectionInfo);
}

/**
 * 禁用 gimple pass "slp"
 * 在pass-instances.def中有两个 slp pass register_pass_info ref_pass_instance_number = 0
 * 表示所有的slp都要加入 disableSlp1  enableSlp1 pass 还需要实现 clone接口。
 */
void mtcs_adjust_pass_disable_slp_pass(MtcsAdjustPass *self)
{
   gimple_opt_pass *disableSlp1 = new pass_mtcs_disable_slp (g);
   register_pass_info disableSlpInfo1 = { disableSlp1, "slp", 0, PASS_POS_INSERT_BEFORE };
   register_pass (&disableSlpInfo1);
   gimple_opt_pass *enableSlp1 = new pass_mtcs_enable_slp (g);
   register_pass_info enableSlpInfo1 = { enableSlp1, "slp", 0, PASS_POS_INSERT_AFTER };
   register_pass (&enableSlpInfo1);
}

static tree canDeleteStackSave (gimple_stmt_iterator i)
{
   tree callee;
   gimple *stmt;

   basic_block bb = gsi_bb (i);
   gimple *call = gsi_stmt (i);
   n_debug("mtcsadjustpass canDeleteStackSave 00 bb:%p\n",bb);
   if (gimple_code (call) != GIMPLE_CALL
   || gimple_call_num_args (call) != 1
   || TREE_CODE (gimple_call_arg (call, 0)) != SSA_NAME
   || !POINTER_TYPE_P (TREE_TYPE (gimple_call_arg (call, 0))))
      return NULL_TREE;
   n_debug("mtcsadjustpass canDeleteStackSave 11 bb:%p\n",bb);

   for (gsi_next (&i); !gsi_end_p (i); gsi_next (&i)){
      stmt = gsi_stmt (i);
      if (gimple_code (stmt) == GIMPLE_ASM)
         return NULL_TREE;
      if (gimple_code (stmt) != GIMPLE_CALL)
         continue;

      callee = gimple_call_fndecl (stmt);
      n_debug("mtcsadjustpass canDeleteStackSave 22 bb:%p\n",bb);
      aet_print_gimple(stmt);
      aet_print_tree(callee);
      if (!callee
      || !fndecl_built_in_p (callee, BUILT_IN_NORMAL)
      /* All regular builtins are ok, just obviously not alloca.  */
      || ALLOCA_FUNCTION_CODE_P (DECL_FUNCTION_CODE (callee))
      /* Do not remove stack updates before strub leave.  */
      || fndecl_built_in_p (callee, BUILT_IN___STRUB_LEAVE))
         return NULL_TREE;

      if (fndecl_built_in_p (callee, BUILT_IN_STACK_RESTORE))
         goto second_stack_restore;
   }

   n_debug("mtcsadjustpass canDeleteStackSave 33 bb:%p\n",bb);

   if (!gsi_end_p (i))
      return NULL_TREE;
   n_debug("mtcsadjustpass canDeleteStackSave 44 bb:%p EDGE_COUNT (bb->succs):%d\n",bb,EDGE_COUNT (bb->succs));

   /* Allow one successor of the exit block, or zero successors.  */
   switch (EDGE_COUNT (bb->succs)){
      case 0:
         break;
      case 1:
         n_debug("mtcsadjustpass canDeleteStackSave 44 55aa single_succ_edge (bb)->dest:%p %p\n",
               single_succ_edge (bb)->dest,EXIT_BLOCK_PTR_FOR_FN (cfun));
         if (single_succ_edge (bb)->dest != EXIT_BLOCK_PTR_FOR_FN (cfun)){
            n_debug("mtcsadjustpass canDeleteStackSave 55 不能删single_succ_edge (bb)->dest != EXIT_BLOCK_PTR_FOR_FN (cfun)\n");
            return NULL_TREE;
         }
         break;
      default:
         return NULL_TREE;
   }

second_stack_restore:
   n_debug("mtcsadjustpass canDeleteStackSave 66 进入 label second_stack_restore\n");

   /* If there's exactly one use, then zap the call to __builtin_stack_save.
   If there are multiple uses, then the last one should remove the call.
   In any case, whether the call to __builtin_stack_save can be removed
   or not is irrelevant to removing the call to __builtin_stack_restore.  */

   /* 如果只有一次调用，则删除对 __builtin_stack_save 的调用。
   如果有多次调用，则最后一次调用应将其删除。
   无论如何，对 __builtin_stack_save 的调用是否可以删除
   与删除对 __builtin_stack_restore 的调用无关。*/
   if (has_single_use (gimple_call_arg (call, 0))){
      gimple *stack_save = SSA_NAME_DEF_STMT (gimple_call_arg (call, 0));
      if (is_gimple_call (stack_save)){
         callee = gimple_call_fndecl (stack_save);
         if (callee && fndecl_built_in_p (callee, BUILT_IN_STACK_SAVE)){
            gimple_stmt_iterator stack_save_gsi;
            tree rhs;
            stack_save_gsi = gsi_for_stmt (stack_save);
            n_debug("mtcsadjustpass canDeleteStackSave 77 替换早前的  gimple_call <__builtin_stack_save, saved_stack.2_13> \n");
            aet_print_gimple(stack_save);
           // *replace=1;
            //rhs = build_int_cst (TREE_TYPE (gimple_call_arg (call, 0)), 0);
            //replace_call_with_value (&stack_save_gsi, rhs);
         }
      }
   }
   /* No effect, so the statement will be deleted.  */
   return integer_zero_node;
}

static unsigned int  proectedStackSaveStmt(function *fun)
{
   bool cfg_changed = false;
   basic_block bb;
   unsigned int todoflags = 0;
   n_debug("mtcsadjustpass.c 判断有没有 call __builtin_stack_save 00\n");
   nboolean haveStackSave=FALSE;
   nboolean haveStackRestore=FALSE;
   gimple_stmt_iterator lastRestore;
   FOR_EACH_BB_FN (bb, fun){
      gimple_stmt_iterator i;
      for (i = gsi_start_bb (bb); !gsi_end_p (i); ){
         gimple *stmt, *old_stmt;
         tree callee;
         enum built_in_function fcode;

         stmt = gsi_stmt (i);
         if (gimple_code (stmt) != GIMPLE_CALL){
            gsi_next (&i);
            continue;
         }

         callee = gimple_call_fndecl (stmt);
         fcode = DECL_FUNCTION_CODE (callee);
         if(fcode==BUILT_IN_STACK_SAVE){
            haveStackSave=TRUE;
         }
         if(fcode==BUILT_IN_STACK_RESTORE){
            //最后的一个 gsi;
            lastRestore=i;
            haveStackRestore=TRUE;
         }
         gsi_next (&i);
      }
   }

   if(haveStackRestore){
   //是否满足可以删除 gimple_call <__builtin_stack_save, saved_stack.2_13>的条件。
     n_debug("mtcsadjustpass 有最后一个 __builtin_stack_restore\n");
     //判断是否可以删除
     tree zero=canDeleteStackSave(lastRestore);
     if(zero!=NULL_TREE){
        n_debug("mtcsadjustpass 需要做保护 00  __builtin_stack_restore\n");
        gimple *call = gsi_stmt (lastRestore);
        gimple *stack_save = SSA_NAME_DEF_STMT (gimple_call_arg (call, 0));
        basic_block bb = gsi_bb (lastRestore);
        int count=EDGE_COUNT (bb->succs);
        basic_block exitbb= EXIT_BLOCK_PTR_FOR_FN (cfun);
        aet_print_gimple(stack_save);
        aet_print_gimple(call);
        n_debug("mtcsadjustpass 需要做保护  11 __builtin_stack_restore bb:%p exitbb:%p succs count:%d\n",bb,exitbb,count);
        gcc_assert(current_ir_type ()==IR_GIMPLE);
        basic_block newbb=create_basic_block(NULL,exitbb->prev_bb);
        auto_vec<tree, 5> args;
        args.quick_push (build_int_cst (integer_type_node, 1));
        args.quick_push (integer_zero_node);
        args.quick_push (integer_minus_one_node);
        gcall *newcall= gimple_build_call_internal_vec (IFN_UNIQUE, args);
        edge e = make_edge (bb, newbb, EDGE_FALLTHRU);
        e->probability = profile_probability::always ();
        n_debug("mtcsadjustpass 需要做保护  22 __builtin_stack_restore bb:%p exitbb:%p succs count:%d\n",bb,exitbb,EDGE_COUNT (bb->succs));
        e = make_edge (newbb, exitbb, 0);
        e->probability = profile_probability::always ();
        gimple_stmt_iterator gsi = gsi_start_bb (newbb);
        gsi_insert_after (&gsi, newcall, GSI_NEW_STMT);
     }
   }
   return 0;
}

//gimple pass "fab"
//1.gimple_call <__builtin_stack_save, saved_stack.2_13>
//2.gimple_call <__builtin_alloca_with_align, local.1_16, _2, 32>
//3.gimple_call <__builtin_stack_restore, NULL, saved_stack.2_13>
//这三句在fab中如果是主机编译 1,3被删，在nvptx 1，2，3被保留。在文件tree-ssa-ccp.cc中
//处理这三的方法是 optimize_stack_restore
//核心代码
//if (single_succ_edge (bb)->dest != EXIT_BLOCK_PTR_FOR_FN (cfun)){
//return NULL_TREE;
//}
//bb 是 gimple_call <__builtin_stack_restore, NULL, saved_stack.2_13> 所在的块。
//当 bb块的输出边的目标等于退出块
void mtcs_adjust_pass_protect_stack_save(MtcsAdjustPass *self)
{
   gimple_opt_pass *protectStackSaveStmt = new pass_protect_stack_save (g);
   register_pass_info info = { protectStackSaveStmt, "fab", 1, PASS_POS_INSERT_BEFORE };
   register_pass (&info);
}

MtcsAdjustPass   *mtcs_adjust_pass_new()
{
   MtcsAdjustPass *self = n_slice_alloc0 (sizeof(MtcsAdjustPass));
   mtcsAdjustInitInit(self);
   return self;
}
