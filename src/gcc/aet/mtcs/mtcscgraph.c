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
 * base on cgraph.cc
 */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "predict.h"
#include "alloc-pool.h"
#include "gimple-ssa.h"
#include "cgraph.h"
#include "lto-streamer.h"
#include "fold-const.h"
#include "varasm.h"
#include "calls.h"
#include "print-tree.h"
#include "langhooks.h"
#include "intl.h"
#include "tree-eh.h"
#include "gimple-iterator.h"
#include "tree-cfg.h"
#include "tree-ssa.h"
#include "value-prof.h"
#include "ipa-utils.h"
#include "symbol-summary.h"
#include "tree-vrp.h"
#include "sreal.h"
#include "ipa-cp.h"
#include "ipa-prop.h"
#include "ipa-fnsummary.h"
#include "cfgloop.h"
#include "gimple-pretty-print.h"
#include "tree-dfa.h"
#include "profile.h"
#include "context.h"
#include "gimplify.h"
#include "stringpool.h"
#include "attribs.h"
#include "selftest.h"
#include "tree-into-ssa.h"
#include "ipa-inline.h"
#include "tree-nested.h"
#include "symtab-thunks.h"
#include "symtab-clones.h"
#include "toplev.h"
#include "cfghooks.h"
#include "regset.h"
#include "memmodel.h"
#include "emit-rtl.h"

#include "mtcscgraph.h"
#include "mtcstarget.h"

static void mtcsCgraphInit(MtcsCgraph *self)
{
   self->rtlInfoArray=n_ptr_array_new();
}

typedef struct _RtlInfoItem{
   tree fndecl;
   MtcsCgraphRtlInfo *rtlInfo;
   struct cgraph_rtl_info *hostInfo;
}RtlInfoItem;

static RtlInfoItem *find(MtcsCgraph *self,const_tree fndecl,struct cgraph_rtl_info *info)
{
    int i;
    for(i=0;i<self->rtlInfoArray->len;i++){
        RtlInfoItem *item=(RtlInfoItem*)n_ptr_array_index(self->rtlInfoArray,i);
        if(item->fndecl==fndecl && item->hostInfo==info)
            return item;
    }
    return NULL;
}
/* Return RTL info for the compiled function.  */
//原型   static struct cgraph_rtl_info *rtl_info (const_tree); cgraph.h cgraph.cc
MtcsCgraphRtlInfo *mtcs_cgraph_get_rtl_info (MtcsCgraph *self,const_tree fndecl)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

  //这里保存生成cgraph_node
  struct cgraph_rtl_info *info= cgraph_node::rtl_info (fndecl);
  if(info==NULL)
     return NULL;
  RtlInfoItem *item=find(self,fndecl,info);
  if(item!=NULL){
      return item->rtlInfo;
  }
  item=n_slice_alloc0 (sizeof(RtlInfoItem));
  item->fndecl=fndecl;
  item->hostInfo=info;
  item->rtlInfo=n_slice_alloc0 (sizeof(RtlInfoItem));
  item->rtlInfo->function_used_regs.count=mtcs_reg_get_hard_reg_element_count(mtcsReg);
  mtcs_reg_set_hard_reg_set/*!SET_HARD_REG_SET*/(&item->rtlInfo->function_used_regs);
  n_ptr_array_add(self->rtlInfoArray,item);
  return item->rtlInfo;
}


/* We always maintain first direct edge in the call site hash, if one
   exists.  E is going to be removed.  See if it is first one and update
   hash accordingly.  INDIRECT is the indirect edge of speculative call.
   We assume that INDIRECT->num_speculative_call_targets_p () is already
   updated for removal of E.  */
//原型 update_call_stmt_hash_for_removing_direct_edge cgraph.cc
//mtcs_cgraph_update_edge_in_call_site_hash 声明在cgraph.h
static void update_call_stmt_hash_for_removing_direct_edge (cgraph_edge *e,cgraph_edge *indirect)
{
  if (e->caller->call_site_hash){
      if (e->caller->get_edge (e->call_stmt) != e)
          ;
      else if (!indirect->num_speculative_call_targets_p ())
          mtcs_cgraph_update_edge_in_call_site_hash/*!cgraph_update_edge_in_call_site_hash*/(indirect);
      else{
          gcc_checking_assert (e->next_callee && e->next_callee->speculative && e->next_callee->call_stmt == e->call_stmt);
          mtcs_cgraph_update_edge_in_call_site_hash/*!cgraph_update_edge_in_call_site_hash*/(e->next_callee);
      }
  }
}


//原型 redirect_call_stmt_to_callee cgraph.h cgraph.cc
//需要在cgraph.h中加入void mtcs_cgraph_update_edge_in_call_site_hash (cgraph_edge *e);
gimple *mtcs_cgraph_redirect_call_stmt_to_callee (MtcsCgraph *self,cgraph_edge *e,hash_set <tree> *killed_ssas)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsGimple *mtcsGimple=mtcs_target_get_gimple(mtcsTarget);

  tree decl = gimple_call_fndecl (e->call_stmt);
  gcall *new_stmt;
  if (e->speculative){
      /* If there already is an direct call (i.e. as a result of inliner's
      substitution), forget about speculating.  */
      if (decl)
          e = cgraph_edge::make_direct (e->speculative_call_indirect_edge (),cgraph_node::get (decl));
      else{
          /* Be sure we redirect all speculative targets before poking
          about indirect edge.  */
          gcc_checking_assert (e->callee);
          cgraph_edge *indirect = e->speculative_call_indirect_edge ();
          gcall *new_stmt;
          ipa_ref *ref;

          /* Expand speculation into GIMPLE code.  */
          if (dump_file){
              fprintf (dump_file, "Expanding speculative call of %s -> %s count: ", e->caller->dump_name (),e->callee->dump_name ());
              e->count.dump (dump_file);
              fprintf (dump_file, "\n");
          }
          mtcs_func_push_cfun/*!push_cfun*/(mtcsFunc,DECL_STRUCT_FUNCTION (e->caller->decl));

          profile_count all = indirect->count;
          for (cgraph_edge *e2 = e->first_speculative_call_target (); e2; e2 = e2->next_speculative_call_target ())
              all = all + e2->count;
          profile_probability prob = e->count.probability_in (all);
          if (!prob.initialized_p ())
              prob = profile_probability::even ();
          ref = e->speculative_call_target_ref ();
          new_stmt = gimple_ic (e->call_stmt,dyn_cast<cgraph_node *> (ref->referred), prob);
          e->speculative = false;
          if (indirect->num_speculative_call_targets_p ()){
              /* The indirect edge has multiple speculative targets, don't
              remove speculative until all related direct edges are
              redirected.  */
              indirect->indirect_info->num_speculative_call_targets--;
              if (!indirect->indirect_info->num_speculative_call_targets)
                  indirect->speculative = false;
          }else
              indirect->speculative = false;
          /* Indirect edges are not both in the call site hash.
          get it updated.  */
          update_call_stmt_hash_for_removing_direct_edge (e, indirect);
          cgraph_edge::set_call_stmt (e, new_stmt, false);
          e->count = gimple_bb (e->call_stmt)->count;

          /* Once we are done with expanding the sequence, update also indirect
          call probability.  Until then the basic block accounts for the
          sum of indirect edge and all non-expanded speculations.  */
          if (!indirect->speculative)
              indirect->count = gimple_bb (indirect->call_stmt)->count;
          ref->speculative = false;
          ref->stmt = NULL;
          mtcs_func_pop_cfun/*!pop_cfun*/ (mtcsFunc);
          /* Continue redirecting E to proper target.  */
      }
  }


  if (e->indirect_unknown_callee || decl == e->callee->decl)
      return e->call_stmt;

  if (decl && ipa_saved_clone_sources){
      tree *p = ipa_saved_clone_sources->get (e->callee);
      if (p && decl == *p){
         mtcs_gimple_call_set_fndecl/*!gimple_call_set_fndecl*/(mtcsGimple,e->call_stmt, e->callee->decl);
          return e->call_stmt;
      }
  }
  if (flag_checking && decl){
      if (cgraph_node *node = cgraph_node::get (decl)){
          clone_info *info = clone_info::get (node);
          gcc_assert (!info || !info->param_adjustments);
      }
  }

  clone_info *callee_info = clone_info::get (e->callee);
  if (symtab->dump_file){
      fprintf (symtab->dump_file, "updating call of %s -> %s: ",
      e->caller->dump_name (), e->callee->dump_name ());
      print_gimple_stmt (symtab->dump_file, e->call_stmt, 0, dump_flags);
      if (callee_info && callee_info->param_adjustments)
          callee_info->param_adjustments->dump (symtab->dump_file);
  }

  if (ipa_param_adjustments *padjs = callee_info ? callee_info->param_adjustments : NULL){
      /* We need to defer cleaning EH info on the new statement to
      fixup-cfg.  We may not have dominator information at this point
      and thus would end up with unreachable blocks and have no way
      to communicate that we need to run CFG cleanup then.  */
      int lp_nr = lookup_stmt_eh_lp (e->call_stmt);
      if (lp_nr != 0)
          remove_stmt_from_eh_lp (e->call_stmt);

      tree old_fntype = gimple_call_fntype (e->call_stmt);
      new_stmt = padjs->modify_call (e, false, killed_ssas);
      cgraph_node *origin = e->callee;
      while (origin->clone_of)
          origin = origin->clone_of;

      if ((origin->former_clone_of && old_fntype == TREE_TYPE (origin->former_clone_of)) || old_fntype == TREE_TYPE (origin->decl))
          gimple_call_set_fntype (new_stmt, TREE_TYPE (e->callee->decl));
      else{
          tree new_fntype = padjs->build_new_function_type (old_fntype, true);
          gimple_call_set_fntype (new_stmt, new_fntype);
      }

      if (lp_nr != 0)
          add_stmt_to_eh_lp (new_stmt, lp_nr);
  }else{
      if (flag_checking && !fndecl_built_in_p (e->callee->decl, BUILT_IN_UNREACHABLE, BUILT_IN_UNREACHABLE_TRAP))
          ipa_verify_edge_has_no_modifications (e);
      new_stmt = e->call_stmt;
      mtcs_gimple_call_set_fndecl/*!gimple_call_set_fndecl*/(mtcsGimple,new_stmt, e->callee->decl);
      update_stmt_fn (DECL_STRUCT_FUNCTION (e->caller->decl), new_stmt);
  }

  /* If changing the call to __cxa_pure_virtual or similar noreturn function,
  adjust gimple_call_fntype too.  */
  if (gimple_call_noreturn_p (new_stmt)
    && VOID_TYPE_P (TREE_TYPE (TREE_TYPE (e->callee->decl)))
    && TYPE_ARG_TYPES (TREE_TYPE (e->callee->decl))
    && (TREE_VALUE (TYPE_ARG_TYPES (TREE_TYPE (e->callee->decl))) == void_type_node))
      gimple_call_set_fntype (new_stmt, TREE_TYPE (e->callee->decl));

  /* If the call becomes noreturn, remove the LHS if possible.  */
  tree lhs = gimple_call_lhs (new_stmt);
  if (lhs  && gimple_call_noreturn_p (new_stmt)
    && (VOID_TYPE_P (TREE_TYPE (gimple_call_fntype (new_stmt)))
    || should_remove_lhs_p (lhs))){
      gimple_call_set_lhs (new_stmt, NULL_TREE);
      /* We need to fix up the SSA name to avoid checking errors.  */
      if (TREE_CODE (lhs) == SSA_NAME){
          tree var = create_tmp_reg_fn (DECL_STRUCT_FUNCTION (e->caller->decl),
          TREE_TYPE (lhs), NULL);
          SET_SSA_NAME_VAR_OR_IDENTIFIER (lhs, var);
          SSA_NAME_DEF_STMT (lhs) = gimple_build_nop ();
          set_ssa_default_def (DECL_STRUCT_FUNCTION (e->caller->decl),var, lhs);
      }
      update_stmt_fn (DECL_STRUCT_FUNCTION (e->caller->decl), new_stmt);
  }

  /* If new callee has no static chain, remove it.  */
  if (gimple_call_chain (new_stmt) && !DECL_STATIC_CHAIN (e->callee->decl)){
      gimple_call_set_chain (new_stmt, NULL);
      update_stmt_fn (DECL_STRUCT_FUNCTION (e->caller->decl), new_stmt);
  }

  maybe_remove_unused_call_args (DECL_STRUCT_FUNCTION (e->caller->decl),
  new_stmt);

  e->caller->set_call_stmt_including_clones (e->call_stmt, new_stmt, false);

  if (symtab->dump_file){
      fprintf (symtab->dump_file, "  updated to:");
      print_gimple_stmt (symtab->dump_file, e->call_stmt, 0, dump_flags);
  }
  return new_stmt;
}


/* When doing LTO, read cgraph_node's body from disk if it is not already
   present.  Also perform any necessary clone materializations.  */
//原型 cgraph_node::get_untransformed_body cgraph.h cgraph.cc
bool mtcs_cgraph_get_untransformed_body(MtcsCgraph *self,struct cgraph_node *node)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *opts=mtcsOptions->global_options;

  const char *data, *name;
  size_t len;
  tree decl = node->decl;
    /* See if there is clone to be materialized.
  (inline clones does not need materialization, but we can be seeing
  an inline clone of real clone).  */
  cgraph_node *p = node;
  for (cgraph_node *c = node->clone_of; c; c = c->clone_of){
      if (c->decl != decl)
          p->materialize_clone ();
      p = c;
  }

  /* Check if body is already there.  Either we have gimple body or
  the function is thunk and in that case we set DECL_ARGUMENTS.  */
  if (DECL_ARGUMENTS (decl) || gimple_has_body_p (decl)){
     n_debug("mtcscgraph.c get_untransformed_body 00 已经有参数或函数体不需要从lto解析了 name:%s\n",name);
  }
  gcc_assert (opts->x_in_lto_p/*!&& !DECL_RESULT (decl)*/);
  name = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl));
  n_debug("mtcscgraph.c get_untransformed_body 00 name:%s\n",name);
  tree fn_decl = node->decl;
 // mtcs_func_push_struct_function/*!push_struct_function*/(mtcsFunc,fn_decl);
  mtcs_func_push_struct_function_no_create(mtcsFunc,fn_decl,true);
  mtcs_func_pop_cfun(mtcsFunc);
  return true;
}

/* Prepare function body.  When doing LTO, read cgraph_node's body from disk
   if it is not already present.  When some IPA transformations are scheduled,
   apply them.  */
//原型 cgraph_node::get_body cgraph.h cgraph.cc
bool mtcs_cgraph_get_body(MtcsCgraph *self,struct cgraph_node *node)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsPassMgr *mtcsPassMgr=mtcs_target_get_pass_mgr(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *opts=mtcsOptions->global_options;

  bool updated;
  n_debug("mtcscgraph.c cgraph_node::get_body 00 funcName:%s %s\n",current_function_name(),node->name());
  updated = mtcs_cgraph_get_untransformed_body/*!get_untransformed_body*/(self,node);
  n_debug("mtcscgraph.c cgraph_node::get_body 11 get_untransformed_body updated:%d funcName:%s\n",updated,current_function_name());

  /* Getting transformed body makes no sense for inline clones;
     we should never use this on real clones because they are materialized
     early.
     TODO: Materializing clones here will likely lead to smaller LTRANS
     footprint. */
  gcc_assert (!node->inlined_to && !node->clone_of);
  MtcsFuncNode *mtcsFuncNode=mtcs_func_get_node(mtcsFunc,node);
  if (mtcsFuncNode->ipa_transforms_to_apply.exists ()){
      MtcsPass *saved_current_pass = mtcs_pass_mgr_get_current_pass(mtcsPassMgr);
      FILE *saved_dump_file = dump_file;
      const char *saved_dump_file_name = dump_file_name;
      dump_flags_t saved_dump_flags = dump_flags;
      dump_file_name = NULL;
      set_dump_file (NULL);
      n_debug("mtcscgraph.c cgraph_node::get_body 22 funcName:%s\n",current_function_name());

      mtcs_func_push_cfun/*!push_cfun*/(mtcsFunc,DECL_STRUCT_FUNCTION (node->decl));
      n_debug("mtcscgraph.c cgraph_node::get_body 33  funcName:%s\n",current_function_name());

      update_ssa (TODO_update_ssa_only_virtuals);
      n_debug("mtcscgraph.c cgraph_node::get_body 44 funcName:%s\n",current_function_name());

      mtcs_pass_mgr_execute_all_ipa_transforms/*!execute_all_ipa_transforms*/(mtcsPassMgr,true);
      n_debug("mtcscgraph.c cgraph_node::get_body 55 funcName:%s\n",current_function_name());

      cgraph_edge::rebuild_edges ();
      n_debug("mtcscgraph.c cgraph_node::get_body 66 funcName:%s\n",current_function_name());

      free_dominance_info (CDI_DOMINATORS);
      free_dominance_info (CDI_POST_DOMINATORS);
      n_debug("mtcscgraph.c cgraph_node::get_body 77 funcName:%s\n",current_function_name());
      mtcs_func_pop_cfun/*!pop_cfun*/(mtcsFunc);
      mtcsPassMgr->current_pass = saved_current_pass;
      set_dump_file (saved_dump_file);
      dump_file_name = saved_dump_file_name;
      dump_flags = saved_dump_flags;
   }
  n_debug("mtcscgraph.c cgraph_node::get_body 88 funcName:%s\n",current_function_name());
  return updated;
}

/* Expand function specified by node.  */
//原型 node->expad cgraph.h cgraphunit.cc
void mtcs_cgraph_node_expand(MtcsCgraph *self, struct cgraph_node *node)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsPassMgr *mtcsPassMgr=mtcs_target_get_pass_mgr(mtcsTarget);
  MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);

  location_t saved_loc;
  tree decl=node->decl;
  /* We ought to not compile any inline clones.  */
  gcc_assert (!node->inlined_to);

  /* __RTL functions are compiled as soon as they are parsed, so don't
     do it again.  */
  if (node->native_rtl_p ())
    return;
  announce_function (decl);
  node->process = 0;
  gcc_assert (node->lowered);

  /* Initialize the default bitmap obstack.  */
  bitmap_obstack_initialize (NULL);
  node->get_untransformed_body ();

  /* Generate RTL for the body of DECL.  */

  gcc_assert (symtab->global_info_ready);

  /* Initialize the RTL code for the function.  */
  saved_loc = input_location;
  input_location = DECL_SOURCE_LOCATION (decl);
  n_debug("mtcscgraph.c mtcs_cgraph_node_expand 00 decl:%p\n",decl);
  n_debug("mtcscgraph.c mtcs_cgraph_node_expand 11 DECL_STRUCT_FUNCTION (decl):%p\n",DECL_STRUCT_FUNCTION (decl));

  mtcs_cfg_context_change_gimple_state(mtcsCfgContext);
  n_debug("mtcscgraph.c mtcs_cgraph_node_expand 22 decl:%p\n",decl);

  gcc_assert (DECL_STRUCT_FUNCTION (decl));
  mtcs_func_push_cfun/*!push_cfun*/(mtcsFunc,DECL_STRUCT_FUNCTION (decl));
  n_debug("mtcscgraph.c mtcs_cgraph_node_expand 33 insn:%p\n",get_last_insn ());
  /*!init_function_start (decl);*/
  mtcs_func_init_function_start(mtcsFunc,decl);
  n_debug("mtcscgraph.c mtcs_cgraph_node_expand 44\n");
  gimple_register_cfg_hooks ();
  n_debug("mtcscgraph.c mtcs_cgraph_node_expand 55\n");
  bitmap_obstack_initialize (&reg_obstack); /* FIXME, only at RTL generation*/
  n_debug("mtcscgraph.c mtcs_cgraph_node_expand 66\n");
  update_ssa (TODO_update_ssa_only_virtuals);
  n_debug("mtcscgraph.c mtcs_cgraph_node_expand 77\n");
  MtcsFuncNode *funcNode=mtcs_func_get_node(mtcsFunc,node);
  //if (funcNode/*!node*/->ipa_transforms_to_apply.exists ()){
 //   fprintf(stderr,"cgraphunit.cc expand  expand 33 execute_all_ipa_transforms暂时屏蔽\n");
   // mtcs_pass_mgr_execute_all_ipa_transforms/*!execute_all_ipa_transforms*/(mtcsPassMgr,false);
//  }
  /* Perform all tree transforms and optimizations.  */
  /* Signal the start of passes.  */
  /*!不需要 invoke_plugin_callbacks (PLUGIN_ALL_PASSES_START, NULL);*/
  n_debug("mtcscgraph.c mtcs_cgraph_node_expand 88 execute_pass_list all_passes 有gimple也有rtl insn:%p\n",get_last_insn ());
   mtcs_pass_mgr_execute_all_pass(mtcsPassMgr,cfun); /*!execute_pass_list (cfun, g->get_passes ()->all_passes);*/
   n_debug("mtcscgraph.c mtcs_cgraph_node_expand 99 execute_pass_list all_passes 有gimple也有rtl 完成\n");

  /* Signal the end of passes.  */
  /*!不需要 invoke_plugin_callbacks (PLUGIN_ALL_PASSES_END, NULL);*/
  bitmap_obstack_release (&reg_obstack);

  /* Release the default bitmap obstack.  */
  bitmap_obstack_release (NULL);

  /* If requested, warn about function definitions where the function will
     return a value (usually of some struct or union type) which itself will
     take up a lot of stack space.  */
  if (!DECL_EXTERNAL (decl) && TREE_TYPE (decl)){
      tree ret_type = TREE_TYPE (TREE_TYPE (decl));

      if (ret_type && TYPE_SIZE_UNIT (ret_type)  && TREE_CODE (TYPE_SIZE_UNIT (ret_type)) == INTEGER_CST
         && compare_tree_int (TYPE_SIZE_UNIT (ret_type), warn_larger_than_size) > 0){
          unsigned int size_as_int = TREE_INT_CST_LOW (TYPE_SIZE_UNIT (ret_type));

          if (compare_tree_int (TYPE_SIZE_UNIT (ret_type), size_as_int) == 0)
            warning (OPT_Wlarger_than_, "size of return value of %q+D is %u bytes", decl, size_as_int);
          else
            warning (OPT_Wlarger_than_,"size of return value of %q+D is larger than %wu bytes", decl, warn_larger_than_size);
      }
  }

  gimple_set_body (decl, NULL);
  if (DECL_STRUCT_FUNCTION (decl) == 0){
      /* Stop pointing to the local nodes about to be freed.
     But DECL_INITIAL must remain nonzero so we know this
     was an actual function definition.  */
      if (DECL_INITIAL (decl) != 0)
          DECL_INITIAL (decl) = error_mark_node;
  }

  input_location = saved_loc;
  //下面 ggc_collect 引起很多内存问题，屏蔽
  //ggc_collect ();

  if (DECL_STRUCT_FUNCTION (decl)  && DECL_STRUCT_FUNCTION (decl)->assume_function){
      /* Assume functions aren't expanded into RTL, on the other side
     we don't want to release their body.  */
      if (cfun)
          mtcs_func_pop_cfun/*!pop_cfun*/(mtcsFunc);
      return;
  }
  //临时测试加的
  TREE_ASM_WRITTEN (decl) = 1;

  /* Make sure that BE didn't give up on compiling.  */
  gcc_assert (TREE_ASM_WRITTEN (decl));
  if (cfun)
      mtcs_func_pop_cfun/*!pop_cfun*/(mtcsFunc);

  /* It would make a lot more sense to output thunks before function body to
     get more forward and fewer backward jumps.  This however would need
     solving problem with comdats.  See PR48668.  Also aliases must come after
     function itself to make one pass assemblers, like one on AIX, happy.
     See PR 50689.
     FIXME: Perhaps thunks should be move before function IFF they are not in
     comdat groups.  */
  node->assemble_thunks_and_aliases ();
  node->release_body ();
}

MtcsCgraph *mtcs_cgraph_new(MtcsMode *mtcsMode)
{
    MtcsCgraph *self = n_slice_alloc0 (sizeof(MtcsCgraph));
    mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
    mtcsCgraphInit(self);
    return self;
}

