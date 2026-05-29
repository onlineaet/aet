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
 * base on output.cc
 */


/* This file handles generation of all the assembler code
   *except* the instructions of a passestion.
   This includes declarations of variables and their initial values.

   We also output the assembler code for constants stored in memory
   and are responsible for combining constants with the same value.  */
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "cfghooks.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "ssa.h"
#include "emit-rtl.h"
#include "cgraph.h"
#include "lto-streamer.h"
#include "fold-const.h"
#include "varasm.h"
#include "output.h"
#include "graph.h"
#include "debug.h"
#include "cfgloop.h"
#include "value-prof.h"
#include "tree-cfg.h"
#include "tree-ssa-loop-manip.h"
#include "tree-into-ssa.h"
#include "tree-dfa.h"
#include "tree-ssa.h"
#include "tree-pass.h"
#include "plugin.h"
#include "ipa-utils.h"
#include "tree-pretty-print.h" /* for dump_function_header */
#include "context.h"
#include "pass_manager.h"
#include "cfgrtl.h"
#include "tree-ssa-live.h"  /* For remove_unused_locals.  */
#include "tree-cfgcleanup.h"
#include "insn-addr.h" /* for INSN_ADDRESSES_ALLOC.  */
#include "diagnostic-core.h" /* for fnotice */
#include "stringpool.h"
#include "attribs.h"
#include "opts.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "gimple-fold.h"
#include "symbol-summary.h"
#include "sreal.h"
#include "value-range.h"
#include "ipa-cp.h"
#include "ipa-prop.h"
#include "ipa-fnsummary.h"

#include "mtcspassmgr.h"
#include "mtcstarget.h"
#include "mtcspass.h"
#include "mtcsdfcore.h"
#include "mtcsdfproblems.h"
#include "mtcstool.h"
#include "mtcsprintrtl.h"

#include "aet/aetprintgimple.h"

using namespace gcc;

typedef struct _CallbackData
{
   void *obj;
   int  value;
}CallbackData;

static void  mtcsPassMgrInit(MtcsPassMgr *self)
{
    self->regularIpaPassArray=n_ptr_array_new();
    self->lateIpaPassArray=n_ptr_array_new();
    self->allPassArray=n_ptr_array_new();
    self->current_pass=NULL;
}

static void printbb(MtcsPass *pass,struct function *fn)
{
   if(!n_log_is_debug_file(NULL,NULL))
      return;
   if(!cfun || pass->type!=1 || strcmp(pass->name,"expand")==0 || strcmp(pass->name,"*clean_state")==0)
      return;
   tree fndecl = fn->decl;
   char *name=IDENTIFIER_POINTER(DECL_NAME(fndecl));
   if(!strstr(name,"setdata"))
      return;
   char *passName=pass->name;
   basic_block bb;
   FOR_ALL_BB_FN (bb, cfun){
      rtx_insn *insn;
      int i=0;
      FOR_BB_INSNS (bb, insn){
         if (!INSN_P (insn))
            continue;
         fprintf(stderr,"mtcspassmgr.c 打印块中的RTX i:%d passName:%s block:%p index:%d flags:%d insn:%p\n",
         i++,passName,bb,bb->index,bb->flags,insn);
         mtcs_print_rtl_single(stderr,insn);
      }
   }
}

static void printgimple(MtcsPass *pass,struct function *fn)
{
   if(!n_log_is_debug_file(NULL,NULL))
      return;
   if(!cfun || pass->type!=0)
      return;
   tree fndecl = fn->decl;
   char *name=IDENTIFIER_POINTER(DECL_NAME(fndecl));
   if(!strstr(name,"setdata"))
      return;
   char *passName=pass->name;
   if(strcmp(pass->name,"*warn_unused_result")==0
         || strcmp(pass->name,"omplower")==0
         || strcmp(pass->name,"lower")==0
         || strcmp(pass->name,"eh")==0
         || strcmp(pass->name,"cfg")==0)
      return;
   basic_block bb;
   FOR_EACH_BB_FN (bb, cfun){
      gimple_stmt_iterator gsi, seq_gsi;
      n_debug("mtcspasmgr.c 打印 setdata函数gimple 00 bb:%p pass:%s\n",bb,passName);
      for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
         gimple *stmt = gsi_stmt (gsi);
         aet_print_gimple(stmt);
      }
      n_debug("mtcspasmgr.c 打印 setdata函数gimple 11 bb:%p pass:%s\n",bb,passName);
   }
}

static void testprint(MtcsPass *pass,struct function *fn)
{
   return;
   if(!n_log_is_debug_file(NULL,NULL))
      return;
   gcc_assert(cfun==fn);
   if(cfun && pass->type==1 && strcmp(pass->name,"expand") && strcmp(pass->name,"*clean_state"))
      mtcs_print_func_rtl(cfun,pass->name);
}

/* Do profile consistency book-keeping for the pass with static number INDEX.
   RUN is true if the pass really runs, or FALSE
   if we are only book-keeping on passes that may have selectively disabled
   themselves on a given function.  */
static void check_profile_consistency (MtcsPassMgr *self,int index, bool run)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);

   pass_manager *passes = g->get_passes ();
   if (index == -1)
      return;
   if (!self->profile_record)
      self->profile_record = XCNEWVEC (struct profile_record,passes->passes_by_id_size);
   gcc_assert (index < passes->passes_by_id_size && index >= 0);
   self->profile_record[index].run |= run;
   mtcs_cfg_context_profile_record_check_consistency/*!profile_record_check_consistency*/(mtcsCfgContext,
         &self->profile_record[index]);
}


/* Returns true if PASS is explicitly enabled/disabled for FUNC.  */

static bool is_pass_explicitly_enabled_or_disabled (MtcsPassMgr *self,MtcsPass *pass,
                    tree func, vec<mtcs_uid_range *> tab)
{
   mtcs_uid_range *slot, *range;
   int cgraph_uid;
      const char *aname = NULL;

   if (!tab.exists () || (unsigned) pass->static_pass_number >= tab.length () || pass->static_pass_number == -1)
      return false;

   slot = tab[pass->static_pass_number];
   if (!slot)
      return false;

   cgraph_uid = func ? cgraph_node::get (func)->get_uid () : 0;
   if (func && DECL_ASSEMBLER_NAME_SET_P (func))
      aname = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (func));

   range = slot;
   while (range){
      if ((unsigned) cgraph_uid >= range->start  && (unsigned) cgraph_uid <= range->last)
         return true;
      if (range->assem_name && aname && !strcmp (range->assem_name, aname))
         return true;
      range = range->next;
   }

   return false;
}


/* Check if PASS is explicitly disabled or enabled and return
   the gate status.  FUNC is the function to be processed, and
   GATE_STATUS is the gate status determined by pass manager by
   default.  */
static bool override_gate_status (MtcsPassMgr *self,MtcsPass *pass, tree func, bool gate_status)
{
   bool explicitly_enabled = false;
   bool explicitly_disabled = false;
   explicitly_enabled = is_pass_explicitly_enabled_or_disabled (self,pass, func, self->enabled_pass_uid_range_tab);
   explicitly_disabled = is_pass_explicitly_enabled_or_disabled (self,pass, func, self->disabled_pass_uid_range_tab);
   gate_status = !explicitly_disabled && (gate_status || explicitly_enabled);
   return gate_status;
}

/* Account profile the pass with static number INDEX.
   RUN is true if the pass really runs, or FALSE
   if we are only book-keeping on passes that may have selectively disabled
   themselves on a given function.  */

static void account_profile (MtcsPassMgr *self,int index, bool run)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);

   pass_manager *passes = g->get_passes ();
   if (index == -1)
      return;
   if (!self->profile_record)
      self->profile_record = XCNEWVEC (struct profile_record,passes->passes_by_id_size);
   gcc_assert (index < passes->passes_by_id_size && index >= 0);
   self->profile_record[index].run |= run;
   mtcs_cfg_context_profile_record_account_profile/*!profile_record_account_profile*/(mtcsCfgContext,
         &self->profile_record[index]);
}

/* Account profile chnages to all passes in list starting in SUB.  */

static void account_profile_in_list (MtcsPassMgr *self,NPtrArray *passArray)
{
   int i;
   for(i=0;i<passArray->len;i++){
       MtcsPass *pass= (MtcsPass *)n_ptr_array_index(passArray,i);
      check_profile_consistency (self,pass->static_pass_number, false);
      account_profile (self,pass->static_pass_number, false);
      if (pass->childs->len>0)
          account_profile_in_list (self,pass->childs);
   }
}

/* Determine if PASS_NAME matches CRITERION.
   Not a pure predicate, since it can update CRITERION, to support
   matching the Nth invocation of a pass.
   Subroutine of should_skip_pass_p.  */
static bool determine_pass_name_match (const char *pass_name, char *criterion)
{
   size_t namelen = strlen (pass_name);
   if (! strncmp (pass_name, criterion, namelen)){
      /* The following supports starting with the Nth invocation
      of a pass (where N does not necessarily is equal to the
      dump file suffix).  */
      if (criterion[namelen] == '\0' || (criterion[namelen] == '1'  && criterion[namelen + 1] == '\0'))
         return true;
      else{
         if (criterion[namelen + 1] == '\0')
            --criterion[namelen];
         return false;
      }
   }else
      return false;
}

/* For skipping passes until "startwith" pass.
   Return true iff PASS should be skipped.
   Clear cfun->pass_startwith when encountering the "startwith" pass,
   so that all subsequent passes are run.  */
static bool should_skip_pass_p (MtcsPassMgr *self,MtcsPass *pass)
{
   if (!cfun)
      return false;
   if (!cfun->pass_startwith)
      return false;

   /* For __GIMPLE functions, we have to at least start when we leave
   SSA.  Hence, we need to detect the "expand" pass, and stop skipping
   when we encounter it.  A cheap way to identify "expand" is it to
   detect the destruction of PROP_ssa.
   For __RTL functions, we invoke "rest_of_compilation" directly, which
   is after "expand", and hence we don't reach this conditional.  */
   if (pass->properties_destroyed & PROP_ssa){
      n_debug("mtcspassmgr.c starting anyway when leaving SSA: %s\n", pass->name);
      cfun->pass_startwith = NULL;
      return false;
   }

   if (determine_pass_name_match (pass->name, cfun->pass_startwith)){
      n_debug("mtcspassmgr.c found starting pass: %s\n", pass->name);
      cfun->pass_startwith = NULL;
      return false;
   }

   /* For GIMPLE passes, run any property provider (but continue skipping
   afterwards).
   We don't want to force running RTL passes that are property providers:
   "expand" is covered above, and the only pass other than "expand" that
   provides a property is "into_cfglayout" (PROP_cfglayout), which does
   too much for a dumped __RTL function.  */
   if (pass->type == GIMPLE_PASS && pass->properties_provided != 0)
      return false;

   /* We need to (re-)build cgraph edges as needed.  */
   if (strstr (pass->name, "build_cgraph_edges") != NULL)
      return false;

   /* We need to run ISEL as that lowers VEC_COND_EXPR but doesn't provide
   a property.  */
   if (strstr (pass->name, "isel") != NULL)
      return false;

   /* Don't skip df init; later RTL passes need it.  */
   if (strstr (pass->name, "dfinit") != NULL || strstr (pass->name, "dfinish") != NULL)
      return false;

   n_debug("mtcspassmgr.c mtcspassmgr.c skipping pass: %s\n", pass->name);

   /* If we get here, then we have a "startwith" that we haven't seen yet;
   skip the pass.  */
   return true;
}

/* Skip the given pass, for handling passes before "startwith"
   in __GIMPLE and__RTL-marked functions.
   In theory, this ought to be a no-op, but some of the RTL passes
   need additional processing here.  */

static void skip_pass (MtcsPass *pass)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(pass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsAddr *mtcsAddr=mtcs_target_get_addr(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   /* Pass "reload" sets the global "reload_completed", and many
   things depend on this (e.g. instructions in .md files).  */
   if (strcmp (pass->name, "reload") == 0)
      reload_completed = 1;

   /* Similar for pass "pro_and_epilogue" and the "epilogue_completed" global
   variable.  */
   if (strcmp (pass->name, "pro_and_epilogue") == 0)
      epilogue_completed = 1;

   /* The INSN_ADDRESSES vec is normally set up by
   shorten_branches; set it up for the benefit of passes that
   run after this.  */
   if (strcmp (pass->name, "shorten") == 0)
      mtcs_addr_insn_addresses_alloc/*!INSN_ADDRESSES_ALLOC*/(mtcsAddr,mtcs_rtl_data_get_max_uid/*!get_max_uid*/(mtcsRtlData));

   /* Update the cfg hooks as appropriate.  */
   if (strcmp (pass->name, "into_cfglayout") == 0){
      mtcs_cfg_context_change_layout_state/*!cfg_layout_rtl_register_cfg_hooks*/(mtcsCfgContext);
      cfun->curr_properties |= PROP_cfglayout;
   }
   if (strcmp (pass->name, "outof_cfglayout") == 0){
      mtcs_cfg_context_change_rtl_state/*!rtl_register_cfg_hooks*/(mtcsCfgContext);
      cfun->curr_properties &= ~PROP_cfglayout;
   }
}

/* Helper function. Verify that the properties has been turn into the
   properties expected by the pass.  */

static void verify_curr_properties (function *fn, void *data)
{
   unsigned int props = (size_t)data;
   n_debug("mtcspassmgr.c verify_curr_properties %d %d %d\n",fn->curr_properties,props,(fn->curr_properties & props));
   gcc_assert ((fn->curr_properties & props) == props);
}

/* If we are in IPA mode (i.e., current_function_decl is NULL), call
   function CALLBACK for every function in the call graph.  Otherwise,
   call CALLBACK on the current function.  */

static void do_per_function (MtcsPassMgr *self,void (*callback) (function *, void *data), void *data)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   if (current_function_decl)
      callback (cfun, data);
   else{
      struct cgraph_node *node;
      FOR_EACH_DEFINED_FUNCTION (node){
         n_debug("mtcspassmgr.c do_per_function analyzed:%d %d in_lto_p:%d %d\n",
               node->analyzed,(gimple_has_body_p (node->decl) && !mtcsOptionsItem->x_in_lto_p),
               mtcsOptionsItem->x_in_lto_p,(!node->clone_of || node->decl != node->clone_of->decl));
         if (node->analyzed && (gimple_has_body_p (node->decl) && !mtcsOptionsItem->x_in_lto_p)
         && (!node->clone_of || node->decl != node->clone_of->decl))
            callback (DECL_STRUCT_FUNCTION (node->decl), data);
      }
   }
}

/* Clear the last verified flag.  */
static void clear_last_verified (function *fn, void *data ATTRIBUTE_UNUSED)
{
   fn->last_verified = 0;
}

/* After executing the pass, apply expected changes to the function
   properties. */
static void update_properties_after_pass (function *fn, void *data)
{
   MtcsPass *pass = (MtcsPass *) data;
   fn->curr_properties = (fn->curr_properties | pass->properties_provided)
         & ~pass->properties_destroyed;
}

/* Account profile for IPA pass.  Callback for do_per_function.  */
static void account_profile_1 (function *fn, void *data)
{
   MtcsPass *pass=(MtcsPass *)data;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(pass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsPassMgr *self=mtcs_target_get_pass_mgr(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   mtcs_func_push_cfun/*!push_cfun */(mtcsFunc,fn);
   check_profile_consistency (self,pass->static_pass_number, true);
   account_profile (self,pass->static_pass_number, true);
   mtcs_func_pop_cfun/*!pop_cfun*/(mtcsFunc);
}


/* Verify invariants that should hold between passes.  This is a place
   to put simple sanity checks.  */
static void verify_interpass_invariants (void)
{
  gcc_checking_assert (!fold_deferring_overflow_warnings_p ());
}


/* Perform all TODO actions that ought to be done on each function.  */
static void execute_function_todo (function *fn, void *userData)
{
   CallbackData *data=(CallbackData *)userData;
   MtcsPassMgr *self = (MtcsPassMgr *)data->obj;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   MtcsPass *pass = self->current_pass;
   bool from_ipa_pass = (cfun == NULL);
   unsigned int flags = data->value;/*!(size_t)data*/;
   flags &= ~fn->last_verified;
   //n_debug("mtcspassmgr.c execute_function_todo 00 do_per_function 00 flags:%d fn:%p cfun:%p\n",flags,fn,cfun);
   if (!flags)
      return;
   mtcs_func_push_cfun/*!push_cfun */(mtcsFunc,fn);
   /* If we need to cleanup the CFG let it perform a needed SSA update.  */
   if (flags & TODO_cleanup_cfg){
      n_debug("mtcspassmgr.c execute_function_todo 11 flags & TODO_cleanup_cfg \n");
      cleanup_tree_cfg (flags & TODO_update_ssa_any);
   }else if (flags & TODO_update_ssa_any){
      n_debug("mtcspassmgr.c execute_function_todo 22 flags & TODO_update_ssa_any\n");
      update_ssa (flags & TODO_update_ssa_any);
   }
   n_debug("mtcspassmgr.c execute_function_todo 11xxx flags & TODO_cleanup_cfg \n");
   //testprint(pass,cfun);

   gcc_assert (!need_ssa_update_p (fn));

   if (mtcsOptionsItem->x_flag_tree_pta && (flags & TODO_rebuild_alias)){
      n_debug("mtcspassmgr.c execute_function_todo 33  compute_may_aliases()\n");
      compute_may_aliases ();
   }

   if (mtcsOptionsItem->x_optimize && (flags & TODO_update_address_taken)){
      n_debug("mtcspassmgr.c execute_function_todo 44  execute_update_addresses_taken ();\n");
      execute_update_addresses_taken ();
   }
   n_debug("mtcspassmgr.c execute_function_todo 11yyy flags & TODO_cleanup_cfg \n");
   //testprint(pass,cfun);
   if (flags & TODO_remove_unused_locals){
      n_debug("mtcspassmgr.c execute_function_todo 55  remove_unused_locals ();\n");
      remove_unused_locals ();
   }
   if (flags & TODO_rebuild_cgraph_edges){
      n_debug("mtcspassmgr.c execute_function_todo 66  remove_unused_locals ();\n");
      cgraph_edge::rebuild_edges ();
   }
   n_debug("mtcspassmgr.c execute_function_todo 11www flags & TODO_cleanup_cfg \n");
   //testprint(pass,cfun);
   gcc_assert (dom_info_state (fn, CDI_POST_DOMINATORS) == DOM_NONE);
   //n_debug("mtcspassmgr.c execute_function_todo 77\n");

   /* If we've seen errors do not bother running any verifiers.  */
   if (mtcsOptionsItem->x_flag_checking && !seen_error ()){
      dom_state pre_verify_state = dom_info_state (fn, CDI_DOMINATORS);
      dom_state pre_verify_pstate = dom_info_state (fn, CDI_POST_DOMINATORS);
     // n_debug("mtcspassmgr.c execute_function_todo 88\n");

      if (flags & TODO_verify_il){
        // n_debug("mtcspassmgr.c execute_function_todo 99\n");
         if (cfun->curr_properties & PROP_gimple){
            n_debug("mtcspassmgr.c execute_function_todo 100\n");
            n_debug("mtcspassmgr.c execute_function_todo 11zzz flags & TODO_cleanup_cfg \n");
            //testprint(pass,cfun);
            if (cfun->curr_properties & PROP_cfg)
               /* IPA passes leave stmts to be fixed up, so make sure to
               not verify stmts really throw.  */
               verify_gimple_in_cfg (cfun, !from_ipa_pass);
            else
               verify_gimple_in_seq (gimple_body (cfun->decl));
            n_debug("mtcspassmgr.c execute_function_todo 100uuu\n");

         }
         if (cfun->curr_properties & PROP_ssa){
            n_debug("mtcspassmgr.c execute_function_todo 101 verify_ssa () from_ipa_pass:%d\n",from_ipa_pass);
            /* IPA passes leave stmts to be fixed up, so make sure to
            not verify SSA operands whose verifier will choke on that.  */
            verify_ssa (true, !from_ipa_pass);
         }
         /* IPA passes leave basic-blocks unsplit, so make sure to
         not trip on that.  */
         if ((cfun->curr_properties & PROP_cfg)  && !from_ipa_pass){
            n_debug("mtcspassmgr.c execute_function_todo 102 mtcs_cfg_context_verify_flow_info ()\n");
            mtcs_cfg_context_verify_flow_info/*!verify_flow_info*/(mtcsCfgContext);
         }
         if (current_loops && ! loops_state_satisfies_p (LOOPS_NEED_FIXUP)){
            n_debug("mtcspassmgr.c execute_function_todo 103 verify_loop_structure  ()\n");
            verify_loop_structure ();
            if (loops_state_satisfies_p (LOOP_CLOSED_SSA)){
               n_debug("mtcspassmgr.c execute_function_todo 104 verify_loop_closed_ssa  ()\n");
               verify_loop_closed_ssa (false);
            }
         }
         if (cfun->curr_properties & PROP_rtl){
            n_debug("mtcspassmgr.c execute_function_todo 105 mtcs_emit_verify_rtl_sharing  ()\n");
            mtcs_emit_verify_rtl_sharing/*!verify_rtl_sharing*/(mtcsEmit);
         }
      }
      n_debug("mtcspassmgr.c execute_function_todo 106  ()\n");
      /* Make sure verifiers don't change dominator state.  */
      gcc_assert (dom_info_state (fn, CDI_DOMINATORS) == pre_verify_state);
      gcc_assert (dom_info_state (fn, CDI_POST_DOMINATORS) == pre_verify_pstate);
   }
   n_debug("mtcspassmgr.c execute_function_todo 107()\n");

   fn->last_verified = flags & TODO_verify_all;
   mtcs_func_pop_cfun/*!pop_cfun*/(mtcsFunc);
   /* For IPA passes make sure to release dominator info, it can be
   computed by non-verifying TODOs.  */
   if (from_ipa_pass){
      n_debug("mtcspassmgr.c execute_function_todo 108 \n");
      free_dominance_info (fn, CDI_DOMINATORS);
      free_dominance_info (fn, CDI_POST_DOMINATORS);
   }
}

/* Helper function to perform function body dump.  */
static void execute_function_dump (function *fn, void *data)
{
   MtcsPass *pass = (MtcsPass *)data;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(pass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   if (dump_file){
      mtcs_func_push_cfun/*!push_cfun */(mtcsFunc,fn);
      if (fn->curr_properties & PROP_gimple)
         dump_function_to_file (fn->decl, dump_file, dump_flags);
      else
         print_rtl_with_bb (dump_file, get_insns (), dump_flags);
      /* Flush the file.  If verification fails, we won't be able to
      close the file before aborting.  */
      fflush (dump_file);
      if ((fn->curr_properties & PROP_cfg)  && (dump_flags & TDF_GRAPH)){
         gcc::dump_manager *dumps = g->get_dumps ();
         struct dump_file_info *dfi  = dumps->get_dump_file_info (pass->static_pass_number);
         if (!dfi->graph_dump_initialized){
            clean_graph_dump_file (dump_file_name);
            dfi->graph_dump_initialized = true;
         }
         print_graph_cfg (dump_file_name, fn);
      }
      mtcs_func_pop_cfun/*!pop_cfun*/(mtcsFunc);
   }
}


/* Perform all TODO actions.  */
static void execute_todo(MtcsPassMgr *self,unsigned int flags)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  if (mtcsOptionsItem->x_flag_checking/*!flag_checking*/ && cfun && need_ssa_update_p (cfun)){
    n_debug("mtcspassmgr.c execute_todo 00 (flags & TODO_update_ssa_any):%d\n",(flags & TODO_update_ssa_any));
    gcc_assert (flags & TODO_update_ssa_any);
  }

  n_debug("mtcspassmgr.c execute_todo 11 flags:%d self->current_pass:%p\n",flags,self->current_pass);
  //不需要 statistics_fini_pass ();
  if (flags){
     n_debug("mtcspassmgr.c execute_todo 22 do_per_function flags:%d\n",flags);
     CallbackData userData={(void *)self,flags};
     do_per_function (self,execute_function_todo, &userData/*!(void *)(size_t) flags*/);
  }

  /* At this point we should not have any unreachable code in the
     CFG, so it is safe to flush the pending freelist for SSA_NAMES.  */
  if (cfun && cfun->gimple_df){
     n_debug("mtcspassmgr.c execute_todo 33 flush_ssaname_freelist\n");
    flush_ssaname_freelist ();
  }

  /* Always remove functions just as before inlining: IPA passes might be
     interested to see bodies of extern inline functions that are not inlined
     to analyze side effects.  The full removal is done just at the end
     of IPA pass queue.  */
  if (flags & TODO_remove_functions){
     n_debug("mtcspassmgr.c execute_todo 44 remove_unreachable_nodes %p\n",cfun);
      gcc_assert (!cfun);
      symtab->remove_unreachable_nodes (dump_file);
  }

  if ((flags & TODO_dump_symtab) && dump_file && !current_function_decl){
     n_debug("mtcspassmgr.c execute_todo 55 TODO_dump_symtab %p\n",cfun);
      gcc_assert (!cfun);
      symtab->dump (dump_file);
      /* Flush the file.  If verification fails, we won't be able to
     close the file before aborting.  */
      fflush (dump_file);
  }
  /* Now that the dumping has been done, we can get rid of the optional
     df problems.  */
  if (flags & TODO_df_finish){
     n_debug("mtcspassmgr.c execute_todo 66 TODO_df_finish %p type:%d name:%s\n",cfun,self->current_pass->type,self->current_pass->name);
     mtcs_dfcore_df_finish_pass/*!df_finish_pass*/(mtcsDfcore,(flags & TODO_df_verify) != 0);
  }
}

/* Execute PASS. */
//原型  execute_one_pass tree-pass.h passes.cc
bool mtcs_pass_mgr_execute_one_pass (MtcsPassMgr *self,MtcsPass *pass)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   unsigned int todo_after = 0;
   bool gate_status;
   /* IPA passes are executed on whole program, so cfun should be NULL.
   Other passes need function context set.  */
   if (pass->type == SIMPLE_IPA_PASS || pass->type == IPA_PASS)
      gcc_assert (!cfun && !current_function_decl);
   else
      gcc_assert (cfun && current_function_decl);

   self->current_pass = pass;
   /* Check whether gate check should be avoided.
   User controls the value of the gate through the parameter "gate_status". */
   gate_status = mtcs_pass_gate(pass,cfun);/*!pass->gate (cfun);*/
   gate_status = override_gate_status (self,pass, current_function_decl, gate_status);
   n_debug("mtcspassmgr.c mtcs_pass_gate type:%d name:%s %p gate_status:%d\n",pass->type,pass->name,pass,gate_status);

   /* Override gate with plugin.  */
   // invoke_plugin_callbacks (PLUGIN_OVERRIDE_GATE, &gate_status);

   if (!gate_status){
      /* Run so passes selectively disabling themselves on a given function
      are not miscounted.  */
      if (mtcsOptionsItem->x_profile_report && cfun && (cfun->curr_properties & PROP_cfg)
      && pass->type != IPA_PASS && pass->type != SIMPLE_IPA_PASS){
         check_profile_consistency (self,pass->static_pass_number, false);
         account_profile (self,pass->static_pass_number, false);
         if (pass->childs->len>0)
            account_profile_in_list (self,pass->childs);
      }
      self->current_pass = NULL;
      n_debug("mtcspassmgr.c execute_one_pass 00 !gate_status 返回 type:%d name:%s func:%s\n",pass->type,pass->name,current_function_name());
      return false;
   }
   if (should_skip_pass_p (self,pass)){
      skip_pass (pass);
      n_debug("mtcspassmgr.c execute_one_pass 11 should_skip_pass_p (pass) 返回 type:%d name:%s func:%s\n",pass->type,pass->name,current_function_name());
      return true;
   }
   //  /* Pass execution event trigger: useful to identify passes being
   //     executed.  */
   //  invoke_plugin_callbacks (PLUGIN_PASS_EXECUTION, pass);
   //  if (!quiet_flag && !cfun)
   //    n_debug("mtcspassmgr.c  <%s>", pass->name ? pass->name : "");

   /* Note that the folders should only create gimple expressions.
   This is a hack until the new folder is ready.  */
   in_gimple_form = (cfun && (cfun->curr_properties & PROP_gimple)) != 0;
   //pass_init_dump_file (pass);
   /* Run pre-pass verification.  */
   execute_todo(self,pass->todo_flags_start);
   if (mtcsOptionsItem->x_flag_checking){
      n_debug("mtcspassmgr.c execute_one_pass 22 do_per_function type:%d name:%s func:%s properties_required:%d\n",
      pass->type,pass->name,current_function_name(),pass->properties_required);
      do_per_function(self,verify_curr_properties,(void *)(size_t)pass->properties_required);
   }
   n_debug("mtcspassmgr.c execute_one_pass 33 开始 type:%d name:%s reg_chain:%p\n",
   pass->type,pass->name,df?df->def_regs[0]->reg_chain:NULL);
   testprint(pass,cfun);

   /* Do it!  */
  // if(pass->type==RTL_PASS)
    //  mtcs_tool_print_cfun_loop();
   //testprint(pass,cfun);
   todo_after = mtcs_pass_excute(pass,cfun);
   n_debug("mtcspassmgr.c execute_one_pass 44 完成execute type:%d name:%s func:%s reg_chain:%p \n",
         pass->type,pass->name,current_function_name(),df?df->def_regs[0]->reg_chain:NULL);
   testprint(pass,cfun);

   if (todo_after & TODO_discard_function){
      //pass_fini_dump_file (pass);
      n_debug("mtcspassmgr.c execute_one_pass 55 type:%d name:%s func:%s\n",pass->type,pass->name,current_function_name());
      gcc_assert (cfun);
      /* As cgraph_node::release_body expects release dominators info,
      we have to release it.  */
      if (dom_info_available_p (CDI_DOMINATORS))
         free_dominance_info (CDI_DOMINATORS);

      if (dom_info_available_p (CDI_POST_DOMINATORS))
         free_dominance_info (CDI_POST_DOMINATORS);

      if (cfun->assume_function){
         /* For assume functions, don't release body, keep it around.  */
         cfun->curr_properties |= PROP_assumptions_done;
         mtcs_func_pop_cfun/*!pop_cfun*/(mtcsFunc);
         self->current_pass = NULL;
         n_debug("mtcspassmgr.c execute_one_pass 66 返回 type:%d name:%s func:%s\n",pass->type,pass->name,current_function_name());
         return true;
      }

      tree fn = cfun->decl;
      mtcs_func_pop_cfun/*!pop_cfun*/(mtcsFunc);
      gcc_assert (!cfun);
      cgraph_node::get (fn)->release_body ();
      self->current_pass = NULL;
      redirect_edge_var_map_empty ();
      ggc_collect ();
      n_debug("mtcspassmgr.c execute_one_pass 77 返回 type:%d name:%s func:%s\n",pass->type,pass->name,current_function_name());
      return true;
   }
   n_debug("mtcspassmgr.c execute_one_pass 88 type:%d name:%s func:%s\n",pass->type,pass->name,current_function_name());
   //testprint(pass,cfun);

   do_per_function(self,clear_last_verified, NULL);
   do_per_function(self,update_properties_after_pass, pass);
   n_debug("mtcspassmgr.c execute_one_pass 99 type:%d name:%s func:%s\n",pass->type,pass->name,current_function_name());
   //testprint(pass,cfun);

   //      fprintf(stderr,"passes.cc execute_one_pass 77  type:%d name:%s func:%s\n",pass->type,pass->name,current_function_name());
   //      struct cgraph_node *nodex;
   //      FOR_EACH_FUNCTION(nodex){
   //          printf("passes.cc execute_one_pass 66 func :%s\n",nodex->name());
   //      }

   /* Run post-pass cleanup and verification.  */
   execute_todo(self,todo_after | pass->todo_flags_finish | TODO_verify_il);
   n_debug("mtcspassmgr.c execute_one_pass 100 type:%d name:%s func:%s\n",pass->type,pass->name,current_function_name());
   //testprint(pass,cfun);

   if (mtcsOptionsItem->x_profile_report){
      /* IPA passes are accounted at transform time.  */
      if (pass->type == IPA_PASS)
         ;
      else if (pass->type == SIMPLE_IPA_PASS){
         do_per_function (self,account_profile_1, pass);
      }else if (cfun && (cfun->curr_properties & PROP_cfg)){
         check_profile_consistency (self,pass->static_pass_number, true);
         account_profile (self,pass->static_pass_number, true);
      }
   }
   n_debug("mtcspassmgr.c execute_one_pass 101 type:%d name:%s func:%s\n",pass->type,pass->name,current_function_name());
  // testprint(pass,cfun);

   verify_interpass_invariants ();
   if (pass->type == IPA_PASS  && pass->function_transform){
      struct cgraph_node *node;
      FOR_EACH_FUNCTION_WITH_GIMPLE_BODY (node)
         if (!node->inlined_to){
            MtcsFuncNode *mtcsFuncNode=mtcs_func_get_node(mtcsFunc,node);
            n_debug("mtcspassmgr.c execute_one_pass 102 ipa_transforms_to_apply xx pass:%d %s mtcsFuncNode:%p\n",
                  pass->type,pass->name,mtcsFuncNode);
            /*!node->ipa_transforms_to_apply.safe_push ((ipa_opt_pass_d *)pass);*/
            mtcsFuncNode->ipa_transforms_to_apply.safe_push (pass);
         }
   }else if (dump_file)
      do_per_function(self,execute_function_dump, pass);

   n_debug("mtcspassmgr.c execute_one_pass 103 type:%d name:%s func:%s\n",pass->type,pass->name,current_function_name());
   //testprint(pass,cfun);

   if (!current_function_decl)
      symtab->process_new_functions ();
   //pass_fini_dump_file (pass);
   if (pass->type != SIMPLE_IPA_PASS && pass->type != IPA_PASS){
      gcc_assert (!(cfun->curr_properties & PROP_gimple) || pass->type != RTL_PASS);
   }

   MtcsPass *tempOld=self->current_pass;
   self->current_pass = NULL;
   redirect_edge_var_map_empty ();

   /* Signal this is a suitable GC collection point.  */
   if (!((todo_after | pass->todo_flags_finish) & TODO_do_not_ggc_collect)){
      //调用 ggc_collect会引起hash_table_mod1 (hashval_t hash, unsigned int index)中的参数 index变得很大 比如:2779096485
      //不能放开，引起各种段错误
      //ggc_collect ();
   }
   n_debug("mtcspassmgr.c execute_one_pass 104 完成 type:%d name:%s func:%s\n",pass->type,pass->name,current_function_name());
   testprint(pass,cfun);

   return true;
}

typedef struct _SubData{
    MtcsPassMgr *self;
    MtcsPass *mtcsPass;
}SubData;

static void executePassList(MtcsPassMgr *self,NPtrArray *passArray)
{
    int i;
    for(i=0;i<passArray->len;i++){
        MtcsPass *pass= (MtcsPass *)n_ptr_array_index(passArray,i);
        gcc_assert (pass->type == GIMPLE_PASS  || pass->type == RTL_PASS);
        if (cfun == NULL)
            return;
        n_debug("mtcspassmgr.c execute_pass_list_1 00 type:%d,name:%s \n",pass->type,pass->name);

        bool ok=mtcs_pass_mgr_execute_one_pass(self,pass);
        if(ok && pass->childs->len>0){
           n_debug("mtcspassmgr.c execute_pass_list_1 11 有子类 父 type:%d name:%s func:%s 子 :%d\n",
                    pass->type,pass->name,current_function_name(),pass->childs->len);
            executePassList(self,pass->childs);
        }
    }
}

//原型 execute_pass_list passes.h passes.cc
void mtcs_pass_mgr_execute_pass_list (MtcsPassMgr *self,function *fn, MtcsPass *pass)
{
    gcc_assert (fn == cfun);
    int level=0;
    executePassList (self,pass->childs);
    if (cfun && fn->cfg){
        free_dominance_info (CDI_DOMINATORS);
        free_dominance_info (CDI_POST_DOMINATORS);
    }
}

static void executePassList_cb(function *fn, void  *userData)
{
    SubData *data=(SubData *)userData;
    MtcsPassMgr *self=data->self;
    MtcsPass *pass=data->mtcsPass;
    mtcs_pass_mgr_execute_pass_list(self,fn,pass);
}

/* Same as execute_pass_list but assume that subpasses of IPA passes
   are local passes.  */
static void excuteIpa(MtcsPassMgr *self, NPtrArray *passArray)
{
    int i;
    for(i=0;i<passArray->len;i++){
        MtcsPass *pass= (MtcsPass *)n_ptr_array_index(passArray,i);
        gcc_assert (!current_function_decl);
        gcc_assert (!cfun);
        gcc_assert (pass->type == SIMPLE_IPA_PASS || pass->type == IPA_PASS);
        bool ok=mtcs_pass_mgr_execute_one_pass (self,pass);
        n_debug("mtcspassmgr.c excuteIpa  00 %d %s 完成了，如果ret=true 继续执行子pass: ok:%d \n",pass->type ,pass->name,ok);
        if (ok && pass->childs->len>0 /*!sub*/){
            MtcsPass *first=(MtcsPass *)n_ptr_array_index(pass->childs,0);
            n_debug("mtcspassmgr.c excuteIpa  11 first child:%d %s \n",first->type ,first->name);
            if (first->type == GIMPLE_PASS){
                //invoke_plugin_callbacks (PLUGIN_EARLY_GIMPLE_PASSES_START, NULL);
                SubData data={self,pass};
                do_per_function_toporder ((void (*)(function *, void *))executePassList_cb,&data);
                //invoke_plugin_callbacks (PLUGIN_EARLY_GIMPLE_PASSES_END, NULL);
            } else if (first->type == SIMPLE_IPA_PASS || first->type == IPA_PASS)
                excuteIpa (self,pass->childs);
            else
                gcc_unreachable ();
        }
        gcc_assert (!current_function_decl);
        symtab->process_new_functions ();
    }
}

/* Same as execute_pass_list but assume that subpasses of IPA passes
   are local passes.  */
//原型 execute_ipa_pass_list tree-pass.h passes.cc
void mtcs_pass_mgr_execute_regular_ipa (MtcsPassMgr *self)
{
     excuteIpa(self,self->regularIpaPassArray);
}

nboolean mtcs_pass_mgr_add_regular_ipa_pass(MtcsPassMgr *self,MtcsPass *pass)
{
    mtcs_pass_mgr_set_todo_flags_start(self,pass);
    n_ptr_array_add(self->regularIpaPassArray,pass);
    return TRUE;
}

//原型 execute_ipa_pass_list tree-pass.h passes.cc
void mtcs_pass_mgr_execute_late_ipa (MtcsPassMgr *self)
{
     excuteIpa(self,self->lateIpaPassArray);
}

nboolean mtcs_pass_mgr_add_late_ipa_pass(MtcsPassMgr *self,MtcsPass *pass)
{
    mtcs_pass_mgr_set_todo_flags_start(self,pass);
    n_ptr_array_add(self->lateIpaPassArray,pass);
    return TRUE;
}

nboolean mtcs_pass_mgr_add_all_pass(MtcsPassMgr *self,MtcsPass *pass)
{
    mtcs_pass_mgr_set_todo_flags_start(self,pass);
    n_ptr_array_add(self->allPassArray,pass);
    return TRUE;
}


/* Same as execute_pass_list but assume that subpasses of IPA passes
   are local passes.  */
//原型   execute_pass_list (cfun, g->get_passes ()->all_passes);cgraphunit.cc
void mtcs_pass_mgr_execute_all_pass (MtcsPassMgr *self,struct function *fn)
{
   gcc_assert (fn == cfun);
   executePassList (self,self->allPassArray);
   if (cfun && fn->cfg){
      free_dominance_info (CDI_DOMINATORS);
      free_dominance_info (CDI_POST_DOMINATORS);
   }
   n_debug("mtcspassmgr.c mtcs_pass_mgr_execute_all_pass 22 fn:%p\n",fn);

}

static MtcsPass *findPass(MtcsPassMgr *self,enum opt_pass_type type,char *name,NPtrArray *passArray)
{
    int i;
    for(i=0;i<passArray->len;i++){
          MtcsPass *pass=n_ptr_array_index(passArray,i);
          if(pass->type==type && strcmp(pass->name,name)==0)
              return pass;
          if(pass->childs->len>0){
              pass=findPass(self,type,name,pass->childs);
              if(pass)
                  return pass;
          }
     }
     return NULL;
}

MtcsPass *mtcs_pass_mgr_get_pass(MtcsPassMgr *self,enum opt_pass_type type,char *name)
{
    MtcsPass *ret=findPass(self,type,name,self->regularIpaPassArray);
    if(ret)
        return ret;
    ret=findPass(self,type,name,self->lateIpaPassArray);
    if(ret)
        return ret;
    ret=findPass(self,type,name,self->allPassArray);
    if(ret)
        return ret;
    return NULL;
}


/* Execute summary generation for all of the passes in IPA_PASS.  */
//原型 execute_ipa_summary_passes tree-pass.h passes.cc
void mtcs_pass_mgr_execute_ipa_summary_passes (MtcsPassMgr *self)
{
    int i;
    for(i=0;i<self->regularIpaPassArray->len;i++){
       MtcsPass *pass= (MtcsPass *)n_ptr_array_index(self->regularIpaPassArray,i);
       printf("mtcspassmgr.c mtcs_pass_mgr_execute_ipa_summary_passes index:%d type:%d name:%s generate_summary:%p gate:%d\n",
               i,pass->type,pass->name,pass->generate_summary,mtcs_pass_gate(pass,cfun));
       if(pass->type==IPA_PASS && mtcs_pass_gate(pass,cfun) && pass->generate_summary){
           self->current_pass = pass;
           pass->generate_summary(pass);
           //ggc_grow ();
       }
    }
}

/* Execute IPA_PASS function transform on NODE.  */
static void execute_one_ipa_transform_pass (MtcsPassMgr *self,struct cgraph_node *node,
                MtcsPass *ipa_pass, bool do_not_collect)
{
  MtcsPass *pass = ipa_pass;
  unsigned int todo_after = 0;

  self->current_pass = pass;
  if (!ipa_pass->function_transform)
    return;

  /* Note that the folders should only create gimple expressions.
     This is a hack until the new folder is ready.  */
  in_gimple_form = (cfun && (cfun->curr_properties & PROP_gimple)) != 0;

  //pass_init_dump_file (pass);

  /* If a timevar is present, start it.  */
  //if (pass->tv_id != TV_NONE)
 //   timevar_push (pass->tv_id);

  /* Run pre-pass verification.  */
  execute_todo (self,ipa_pass->function_transform_todo_flags_start);

  /* Do it!  */
  todo_after = ipa_pass->function_transform(ipa_pass,node);

  /* Run post-pass cleanup and verification.  */
  execute_todo (self,todo_after);
  verify_interpass_invariants ();

  /* Stop timevar.  */
 // if (pass->tv_id != TV_NONE)
 //   timevar_pop (pass->tv_id);

  if (dump_file)
    do_per_function(self,execute_function_dump, pass);
 // pass_fini_dump_file (pass);

  self->current_pass = NULL;
  redirect_edge_var_map_empty ();

  /* Signal this is a suitable GC collection point.  */
  if (!do_not_collect && !(todo_after & TODO_do_not_ggc_collect))
    ggc_collect ();
}


//原型  execute_all_ipa_transforms tree-pass.h passes.cc
void mtcs_pass_mgr_execute_all_ipa_transforms (MtcsPassMgr *self,nboolean do_not_collect)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

    MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
    MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
     struct cgraph_node *node;
     node = cgraph_node::get (current_function_decl);
     cgraph_node *next_clone;
     for (cgraph_node *n = node->clones; n; n = next_clone){
         next_clone = n->next_sibling_clone;
         if (n->decl != node->decl)
             n->materialize_clone ();
     }

     int j = 0;
     //gcc::pass_manager *passes = g->get_passes ();
     bool report = mtcsOptionsItem->x_profile_report && (cfun->curr_properties & PROP_gimple) != 0;
     MtcsFuncNode *mtcsFuncNode=mtcs_func_get_node(mtcsFunc,node);

     if (report)
       mtcs_func_push_cfun/*!push_cfun*/(mtcsFunc,DECL_STRUCT_FUNCTION (node->decl));

     for (auto p : mtcsFuncNode->ipa_transforms_to_apply/*!node->ipa_transforms_to_apply*/){
         /* To get consistent statistics, we need to account each functio
        to each IPA pass.  */
         if (report){
//             for (;j < p->static_pass_number; j++)
//                 if (passes->get_pass_for_id (j)  && passes->get_pass_for_id (j)->type == IPA_PASS
//                   && ((ipa_opt_pass_d *)passes->get_pass_for_id (j))->function_transform){
//                     check_profile_consistency(self,j, true);
//                     account_profile(self,j, true);
//                 }
//             gcc_checking_assert (passes->get_pass_for_id (j) == p);
         }
         n_debug("mtcspassmgr.c execute_all_ipa_transforms report:%d name:%s\n",report,p->name);
         execute_one_ipa_transform_pass(self,node, p, do_not_collect);
     }
     /* Account remaining IPA passes.  */
     if (report){
//         for (;!passes->get_pass_for_id (j) || passes->get_pass_for_id (j)->type != RTL_PASS; j++)
//             if (passes->get_pass_for_id (j)    && passes->get_pass_for_id (j)->type == IPA_PASS
//               && ((ipa_opt_pass_d *)passes->get_pass_for_id (j))->function_transform){
//                 check_profile_consistency(self,j, true);
//                 account_profile(self,j, true);
//             }
         mtcs_func_pop_cfun/*!pop_cfun*/(mtcsFunc);
     }
     mtcsFuncNode->ipa_transforms_to_apply.release ()/*!node->ipa_transforms_to_apply.release ()*/;
}

static void recursion(NPtrArray *arrays,MtcsPass *newPass,int *find)
{
   if(*find==1)
      return ;
   int len=arrays->len;
   for(int i=0;i<len;i++){
      MtcsPass *pass=n_ptr_array_index(arrays,i);
      if(pass->type==newPass->type && !strcmp(pass->name,newPass->name)){
         //n_debug("mtcspassmgr.c -----找到类类似的pass i:%d type:%d name:%s \n",i,pass->type,pass->name);
         *find=1;
          return;
      }
      if(pass->childs){
        recursion(pass->childs,newPass,find);
      }
   }
}

void mtcs_pass_mgr_set_todo_flags_start(MtcsPassMgr *self,MtcsPass *newPass)
{
   int find=0;
   recursion(self->allPassArray,newPass,&find);
   if(find==1){
     //n_debug("mtcspassmgr.c -----在allPassArray找到类类似的pass 设置新pass的 todo_flag_start: type:%d name:%s\n",newPass->type,newPass->name);
      newPass->todo_flags_start &= ~TODO_mark_first_instance;
      return;
   }
   recursion(self->lateIpaPassArray,newPass,&find);
   if(find==1){
     //  n_debug("mtcspassmgr.c -----在lateIpaPassArray找到类类似的pass 设置新pass的 todo_flag_start: type:%d name:%s \n",newPass->type,newPass->name);
       newPass->todo_flags_start &= ~TODO_mark_first_instance;
       return;
    }

   recursion(self->regularIpaPassArray,newPass,&find);
   if(find==1){
       //n_debug("mtcspassmgr.c -----在regularIpaPassArray找到类类似的pass 设置新pass的 todo_flag_start: type:%d name:%s \n",newPass->type,newPass->name);
         newPass->todo_flags_start &= ~TODO_mark_first_instance;
         return;
   }
 //n_debug("mtcspassmgr.c -----第一次 pass todo_flag_start: type:%d name:%s \n",newPass->type,newPass->name);

   newPass->todo_flags_start |= TODO_mark_first_instance;
   newPass->static_pass_number = -1;

}


MtcsPass  *mtcs_pass_mgr_get_current_pass(MtcsPassMgr *self)
{
    return self->current_pass;
}

MtcsPassMgr    *mtcs_pass_mgr_new(MtcsMode *mtcsMode)
{
    MtcsPassMgr *self = n_slice_alloc0 (sizeof(MtcsPassMgr));
    mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
    mtcsPassMgrInit(self);
    return self;
}

