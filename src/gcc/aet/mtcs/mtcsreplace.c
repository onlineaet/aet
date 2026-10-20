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
#include "calls.h"
#include "optabs-libfuncs.h"
#include "insn-config.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "varasm.h"
#include "stringpool.h"
#include "cfganal.h"
#include "ipa-reference.h"
#include "symbol-summary.h"
#include "tree-vrp.h"
#include "sreal.h"
#include "gimple-ssa.h"
#include "gimple-iterator.h"
#include "gimple-fold.h"
#include "gimplify.h"
#include "gimplify-me.h"
#include "tree-ssanames.h"
#include "gimple.h"
#include "gimple-walk.h"
#include "tree-into-ssa.h"
#include "tree-pretty-print.h"
#include "tree-switch-conversion.h"
#include "tree-cfg.h"
#include "gimple-pretty-print.h"
#include "attribs.h"
#include "tree-inline.h"
#include "tree-ssa.h"
#include "ssa.h"
#include "cfgloop.h"
#include "value-prof.h"
#include "tree-dfa.h"
#include "cfghooks.h"
#include "fold-const.h"
#include "cgraph.h"
#include "tree-cfgcleanup.h"
#include "symbol-summary.h"
#include "symtab-thunks.h"
#include "symtab-clones.h"
#include "tree-eh.h"
#include "gimple-predict.h"
#include "asan.h"
#include "tree-ssa-live.h"
#include "debug.h"

#include "mtcsreplace.h"
#include "mtcstarget.h"
#include "mtcscompile.h"
#include "../aetprinttree.h"
#include "../aetprintgimple.h"
#include "../mtcsinfo.h"
#include "../aetmicro.h"

//原型  remap_gimple_stmt tree-inline.cc
static gimple_seq remap_gimple_stmt (gimple *stmt, copy_body_data *id);
static int processing_debug_stmt = 0;

static void mtcsReplaceInit(MtcsReplace *self)
{

}

//从 _TSecond_parent__superFuncAddressArray取出TSecond类名。
static char *getClassName(char *src,char *suffix)
{
    char *ss=strstr(src,suffix);
    if(ss && strcmp(ss,suffix)==0){
       char className[255];
       memcpy(className,src+1,strlen(src)-1-strlen(suffix));
       className[strlen(src)-1-strlen(suffix)]='\0';
       return n_strdup(className);
    }
    return NULL;
}

//替换
//在主机设备函数中调用super会生成以下两条主机的gimple,现在替换为设备的gimple
//void __host__ __device__ setdata()
//{
//   n_debug("mtcsreplace.c this is hd func\n);
//}
//void __global__ getData()
//{
//    super$->setdata();
//}
//1.gimple_assign <var_decl, _TSecond_parent__superFuncAddressArray.2_1, _TSecond_parent__superFuncAddressArray, NULL, NULL>
//2.gimple_assign <mem_ref, _2, MEM[(long unsigned int *)_TSecond_parent__superFuncAddressArray.2_1 + 32B], NULL, NULL>
//为
//gimple_assign <array_ref, _1, _TSecond_parent__superDeviceAddressArray[4], NULL, NULL>
void mtcs_replace_parent_device_func_array(MtcsReplace *self,struct cgraph_node *newNode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   static char *suffix=NULL;
   //suffix="_parent__superFuncAddressArray";
   if(!suffix)
      suffix=n_strdup_printf("_parent_%s",AET_SUPER_FUNC_ADDRESS_ARRAY);
   tree fndecl=newNode->decl;
   struct function *nodeFun = DECL_STRUCT_FUNCTION (fndecl);
   gcc_assert(nodeFun==cfun);
   basic_block bb;
   gimple_stmt_iterator gsi;

   nboolean go=FALSE;
   FOR_EACH_BB_FN (bb, nodeFun){
      for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
         gimple *stmt = gsi_stmt (gsi);
         if (!is_gimple_assign (stmt))
            continue;
         tree lhs =  gimple_assign_lhs (stmt);
         tree rhs =  gimple_assign_rhs1 (stmt);
         if(VAR_P(rhs) && DECL_NAME(rhs)){
            char *name=IDENTIFIER_POINTER(DECL_NAME(rhs));
            char *className=getClassName(name,suffix);
            if(className){
               //移走 gimple_assign <var_decl, _TSecond_parent__superFuncAddressArray.2_1, _TSecond_parent__superFuncAddressArray, NULL, NULL>
               unlink_stmt_vdef (stmt);//要加 unlink_stmt_vdef release_defs 否则在verify_ssa 报 错误：定义缺失 bug 014
               gsi_remove (&gsi, true);
               release_defs (stmt);
               //stm1 是 gimple_assign <mem_ref, _2, MEM[(long unsigned int *)_TSecond_parent__superFuncAddressArray.2_1 + 32B], NULL, NULL>
               gimple *stmt1 = gsi_stmt (gsi);
               tree lhs1 =  gimple_assign_lhs (stmt1);
               tree rhs1 =  gimple_assign_rhs1 (stmt1);
               tree op0= TREE_OPERAND (rhs1, 0);
               tree op1= TREE_OPERAND (rhs1, 1);
               unsigned HOST_WIDE_INT bitIndex = tree_to_uhwi (op1);
               //，因为主机_TSecond_parent__superFuncAddressArray是指针，除8转成
               // _TSecond_parent__superDeviceAddressArray中的索引,该索引值是setdata作为主机函数在NPtrArray的序号。
               //NPtrArray *fields = getFields(self,className) 见supercall.c
               //setdata作为设备函数在类中的名字是setdata_device，在NPtrArray的序号比作为主机的setdata多1，所以作为设备
               //函数调用_TSecond_parent__superDeviceAddressArray的元素要加1
               tree index_expr=mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,integer_type_node,bitIndex/BITS_PER_UNIT+1);
               AetMediatorUser *mediatorUser =(AetMediatorUser *)mtcs_compile_get();
               AetMediator *mediator = mediatorUser->mediator;
               tree  parentDeviceVarDecl = aet_mediator_get_parent_device_array_decl(mediator,className,mediatorUser);

               tree ref = build4 (ARRAY_REF, long_unsigned_type_node,
                 parentDeviceVarDecl, index_expr, NULL_TREE, NULL_TREE);
               tree ssa=make_ssa_name (long_unsigned_type_node, NULL);
               gimple *newg= gimple_build_assign (ssa,ARRAY_REF,ref);
               //gimple_move_vops 重要否则 tree-into-ssa.c  update_ssa need_ssa_update_p 是真。
               //calculate_dominance_info checking_verify_dominators检查通不过
               gimple_move_vops (newg, stmt1);
               gsi_insert_before (&gsi, newg, GSI_SAME_STMT);
               unlink_stmt_vdef (stmt1);
               gsi_remove (&gsi, true);
               release_defs (stmt1);
               gimple *stmt2 = gsi_stmt (gsi);
               gimple_assign_set_rhs1(stmt2,ssa);
               update_stmt (stmt2);
            }
         }
      }
   }
}

/**
 * 主机根据
 * bool jump_table_cluster::is_enabled (void)
 * {
 *  ...
 *    if (!targetm.have_casesi () && !targetm.have_tablejump ())
 *       return false;
 *  ...
 *  }
 * 是否要做switch 跳转表
 * 在这里重设 targetm.have_tablejump 指向 mtcsHaveTableJump_cb
 * 做完 analyze_switch_statement后，再恢复主机的  have_tablejump
 */
static bool mtcsHaveTableJump_cb ()
{
   return false;
}

/**
 * 如果是gimple switch 在 ptx平台要转为 if else ，因为ptx不支持跳转表
 * 下面代码原型来自 tree-switch-conversion.cc 中的 pass switchlower
 */
void mtcs_replace_switch(MtcsReplace *self,struct cgraph_node *newNode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   if(strcmp(mtcsTarget->platformInfo.name,"cuda")){
      n_error("还未支持的平台:%s\n",mtcsTarget->platformInfo.name);
      return;
   }
   struct function *fun=DECL_STRUCT_FUNCTION (newNode->decl);
   //在mtcsclones.c 中调用 mtcs_func_push_cfun(mtcsFunc,DECL_STRUCT_FUNCTION (new_node->decl));
   //所以 cfun==func
   gcc_assert(fun==cfun);
   basic_block bb;
   bool expanded = false;
   void *backjumpfunc=(void *)targetm.have_tablejump;
   targetm.have_tablejump=mtcsHaveTableJump_cb;
   auto_vec<gimple *> switch_statements;
   switch_statements.create (1);

   FOR_EACH_BB_FN (bb, fun){
      if (gswitch *swtch = safe_dyn_cast <gswitch *> (*gsi_last_bb (bb))){
         group_case_labels_stmt (swtch);
         switch_statements.safe_push (swtch);
      }
   }

   for (unsigned i = 0; i < switch_statements.length (); i++){
      gimple *stmt = switch_statements[i];
      if (n_log_is_debug_file(NULL,NULL)){
         expanded_location loc = expand_location (gimple_location (stmt));
         fprintf (stderr, "beginning to process the following SWITCH statement (%s:%d) : ------- \n",loc.file, loc.line);
         print_gimple_stmt (stderr, stmt, 0, TDF_SLIM);
         putc ('\n', stderr);
      }
      gswitch *swtch = dyn_cast<gswitch *> (stmt);
      if (swtch){
         tree_switch_conversion::switch_decision_tree  dt (swtch);
         expanded |= dt.analyze_switch_statement ();
      }
   }

   if (expanded){
      free_dominance_info (CDI_DOMINATORS);
      free_dominance_info (CDI_POST_DOMINATORS);
      mark_virtual_operands_for_renaming (cfun);
   }
   //恢复主机的 have_tablejump 函数指针
   targetm.have_tablejump=backjumpfunc;
}


/* Return true if DECL is a parameter or a SSA_NAME for a parameter.  */
static bool is_parm (tree decl)
{
   if (TREE_CODE (decl) == SSA_NAME){
      decl = SSA_NAME_VAR (decl);
      if (!decl)
         return false;
   }
   return (TREE_CODE (decl) == PARM_DECL);
}

/* Remap the dependence CLIQUE from the source to the destination function
   as specified in ID.  */
static unsigned short remap_dependence_clique (copy_body_data *id, unsigned short clique)
{
   if (clique == 0 || processing_debug_stmt)
      return 0;
   if (!id->dependence_map)
      id->dependence_map = new hash_map<dependence_hash, unsigned short>;
   bool existed;
   unsigned short &newc = id->dependence_map->get_or_insert (clique, &existed);
   if (!existed){
      /* Clique 1 is reserved for local ones set by PTA.  */
      if (cfun->last_clique == 0)
         cfun->last_clique = 1;
      newc = get_new_clique (cfun);
   }
   return newc;
}

static tree remap_ssa_name (tree name, copy_body_data *id)
{
   tree new_tree, var;
   tree *n;
   n_debug("mtcsreplace.c remap_ssa_name 00aa\n");
   gcc_assert (TREE_CODE (name) == SSA_NAME);

   n = id->decl_map->get (name);
   if (n){
      n_debug("mtcsreplace.c remap_ssa_name 00\n");
      /* When we perform edge redirection as part of CFG copy, IPA-SRA can
      remove an unused LHS from a call statement.  Such LHS can however
      still appear in debug statements, but their value is lost in this
      function and we do not want to map them.  */
      if (id->killed_new_ssa_names  && id->killed_new_ssa_names->contains (*n)){
         gcc_assert (processing_debug_stmt);
         processing_debug_stmt = -1;
         return name;
      }
      return unshare_expr (*n);
   }

   if (processing_debug_stmt){
      n_debug("mtcsreplace.c remap_ssa_name 11\n");

      if (SSA_NAME_IS_DEFAULT_DEF (name)
      && TREE_CODE (SSA_NAME_VAR (name)) == PARM_DECL
      && id->entry_bb == NULL
      && single_succ_p (ENTRY_BLOCK_PTR_FOR_FN (cfun))){
         n_debug("mtcsreplace.c remap_ssa_name 22\n");

         gimple *def_temp;
         gimple_stmt_iterator gsi;
         tree val = SSA_NAME_VAR (name);

         n = id->decl_map->get (val);
         if (n != NULL)
            val = *n;
         if (TREE_CODE (val) != PARM_DECL  && !(VAR_P (val) && DECL_ABSTRACT_ORIGIN (val))){
            processing_debug_stmt = -1;
            return name;
         }
         n = id->decl_map->get (val);
         if (n && TREE_CODE (*n) == DEBUG_EXPR_DECL)
            return *n;
         tree vexpr = build_debug_expr_decl (TREE_TYPE (name));
         /* FIXME: Is setting the mode really necessary? */
         SET_DECL_MODE (vexpr, DECL_MODE (SSA_NAME_VAR (name)));
         def_temp = gimple_build_debug_source_bind (vexpr, val, NULL);
         gsi = gsi_after_labels (single_succ (ENTRY_BLOCK_PTR_FOR_FN (cfun)));
         gsi_insert_before (&gsi, def_temp, GSI_SAME_STMT);
         insert_decl_map (id, val, vexpr);
         return vexpr;
      }

      processing_debug_stmt = -1;
      return name;
   }
   n_debug("mtcsreplace.c remap_ssa_name 33aa\n");

   /* Remap anonymous SSA names or SSA names of anonymous decls.  */
   var = SSA_NAME_VAR (name);
   aet_print_tree(var);
   if (!var
   || (!SSA_NAME_IS_DEFAULT_DEF (name)
   && VAR_P (var)
   && !VAR_DECL_IS_VIRTUAL_OPERAND (var)
   && DECL_ARTIFICIAL (var)
   && DECL_IGNORED_P (var)
   && !DECL_NAME (var))){
      n_debug("mtcsreplace.c remap_ssa_name 33\n");

      struct ptr_info_def *pi;
      new_tree = make_ssa_name (remap_type (TREE_TYPE (name), id));
      if (!var && SSA_NAME_IDENTIFIER (name))
         SET_SSA_NAME_VAR_OR_IDENTIFIER (new_tree, SSA_NAME_IDENTIFIER (name));
      insert_decl_map (id, name, new_tree);
      SSA_NAME_OCCURS_IN_ABNORMAL_PHI (new_tree) = SSA_NAME_OCCURS_IN_ABNORMAL_PHI (name);
      /* At least IPA points-to info can be directly transferred.  */
      if (id->src_cfun->gimple_df
      && id->src_cfun->gimple_df->ipa_pta
      && POINTER_TYPE_P (TREE_TYPE (name))
      && (pi = SSA_NAME_PTR_INFO (name))
      && !pi->pt.anything){
         struct ptr_info_def *new_pi = get_ptr_info (new_tree);
         new_pi->pt = pi->pt;
      }
      /* So can range-info.  */
      if (!POINTER_TYPE_P (TREE_TYPE (name)) && SSA_NAME_RANGE_INFO (name))
         duplicate_ssa_name_range_info (new_tree, name);
      return new_tree;
   }
   n_debug("mtcsreplace.c remap_ssa_name 44aa\n");

   /* Do not set DEF_STMT yet as statement is not copied yet. We do that
   in copy_bb.  */
   new_tree = remap_decl (var, id);
   n_debug("mtcsreplace.c remap_ssa_name 44bb\n");

   /* We might've substituted constant or another SSA_NAME for
   the variable.

   Replace the SSA name representing RESULT_DECL by variable during
   inlining:  this saves us from need to introduce PHI node in a case
   return value is just partly initialized.  */
   if ((VAR_P (new_tree) || TREE_CODE (new_tree) == PARM_DECL)
   && (!SSA_NAME_VAR (name)
   || TREE_CODE (SSA_NAME_VAR (name)) != RESULT_DECL
   || !id->transform_return_to_modify)){
      n_debug("mtcsreplace.c remap_ssa_name 44\n");

      struct ptr_info_def *pi;
      new_tree = make_ssa_name (new_tree);
      insert_decl_map (id, name, new_tree);
      SSA_NAME_OCCURS_IN_ABNORMAL_PHI (new_tree) = SSA_NAME_OCCURS_IN_ABNORMAL_PHI (name);
      /* At least IPA points-to info can be directly transferred.  */
      if (id->src_cfun->gimple_df
      && id->src_cfun->gimple_df->ipa_pta
      && POINTER_TYPE_P (TREE_TYPE (name))
      && (pi = SSA_NAME_PTR_INFO (name))
      && !pi->pt.anything){
         struct ptr_info_def *new_pi = get_ptr_info (new_tree);
         new_pi->pt = pi->pt;
      }
      /* So can range-info.  */
      if (!POINTER_TYPE_P (TREE_TYPE (name))  && SSA_NAME_RANGE_INFO (name))
         duplicate_ssa_name_range_info (new_tree, name);
      if (SSA_NAME_IS_DEFAULT_DEF (name)){
         /* By inlining function having uninitialized variable, we might
         extend the lifetime (variable might get reused).  This cause
         ICE in the case we end up extending lifetime of SSA name across
         abnormal edge, but also increase register pressure.

         We simply initialize all uninitialized vars by 0 except
         for case we are inlining to very first BB.  We can avoid
         this for all BBs that are not inside strongly connected
         regions of the CFG, but this is expensive to test.  */
         if (id->entry_bb
         && SSA_NAME_OCCURS_IN_ABNORMAL_PHI (name)
         && (!SSA_NAME_VAR (name) || TREE_CODE (SSA_NAME_VAR (name)) != PARM_DECL)
         && (id->entry_bb != EDGE_SUCC (ENTRY_BLOCK_PTR_FOR_FN (cfun),0)->dest
         || EDGE_COUNT (id->entry_bb->preds) != 1)){
            n_debug("mtcsreplace.c remap_ssa_name 55\n");

            gimple_stmt_iterator gsi = gsi_last_bb (id->entry_bb);
            gimple *init_stmt;
            tree zero = build_zero_cst (TREE_TYPE (new_tree));

            init_stmt = gimple_build_assign (new_tree, zero);
            gsi_insert_after (&gsi, init_stmt, GSI_NEW_STMT);
            SSA_NAME_IS_DEFAULT_DEF (new_tree) = 0;
         }else{
            SSA_NAME_DEF_STMT (new_tree) = gimple_build_nop ();
            set_ssa_default_def (cfun, SSA_NAME_VAR (new_tree), new_tree);
         }
      }
   }else
      insert_decl_map (id, name, new_tree);
   return new_tree;
}

/* Remap the GIMPLE operand pointed to by *TP.  DATA is really a
   'struct walk_stmt_info *'.  DATA->INFO is a 'copy_body_data *'.
   WALK_SUBTREES is used to indicate walk_gimple_op whether to keep
   recursing into the children nodes of *TP.  */
static tree remap_gimple_op_r (tree *tp, int *walk_subtrees, void *data)
{
   struct walk_stmt_info *wi_p = (struct walk_stmt_info *) data;
   copy_body_data *id = (copy_body_data *) wi_p->info;
   tree fn = id->src_fn;

   /* For recursive invocations this is no longer the LHS itself.  */
   bool is_lhs = wi_p->is_lhs;
   wi_p->is_lhs = false;

   if (TREE_CODE (*tp) == SSA_NAME){
      n_debug("mtcsreplace.c remap_gimple_op_r 00\n");
      *tp = remap_ssa_name (*tp, id);
      n_debug("mtcsreplace.c remap_gimple_op_r 11\n");

      *walk_subtrees = 0;
      if (is_lhs)
         SSA_NAME_DEF_STMT (*tp) = wi_p->stmt;
      return NULL;
   }else if (auto_var_in_fn_p (*tp, fn)){
      n_debug("mtcsreplace.c remap_gimple_op_r 11\n");

      /* Local variables and labels need to be replaced by equivalent
      variables.  We don't want to copy static variables; there's
      only one of those, no matter how many times we inline the
      containing function.  Similarly for globals from an outer
      function.  */
      tree new_decl;

      /* Remap the declaration.  */
      new_decl = remap_decl (*tp, id);
      gcc_assert (new_decl);
      /* Replace this variable with the copy.  */
      STRIP_TYPE_NOPS (new_decl);
      /* ???  The C++ frontend uses void * pointer zero to initialize
      any other type.  This confuses the middle-end type verification.
      As cloned bodies do not go through gimplification again the fixup
      there doesn't trigger.  */
      if (TREE_CODE (new_decl) == INTEGER_CST
      && !useless_type_conversion_p (TREE_TYPE (*tp), TREE_TYPE (new_decl)))
         new_decl = fold_convert (TREE_TYPE (*tp), new_decl);
      *tp = new_decl;
      *walk_subtrees = 0;
   }else if (TREE_CODE (*tp) == STATEMENT_LIST)
      gcc_unreachable ();
   else if (TREE_CODE (*tp) == SAVE_EXPR)
      gcc_unreachable ();
   else if (TREE_CODE (*tp) == LABEL_DECL && (!DECL_CONTEXT (*tp)
   || decl_function_context (*tp) == id->src_fn))
      /* These may need to be remapped for EH handling.  */
      *tp = remap_decl (*tp, id);
   else if (TREE_CODE (*tp) == FIELD_DECL){
      n_debug("mtcsreplace.c remap_gimple_op_r 22\n");

      /* If the enclosing record type is variably_modified_type_p, the field
      has already been remapped.  Otherwise, it need not be.  */
      tree *n = id->decl_map->get (*tp);
      if (n)
         *tp = *n;
      *walk_subtrees = 0;
   }else if (TYPE_P (*tp))
      /* Types may need remapping as well.  */
      *tp = remap_type (*tp, id);
   else if (CONSTANT_CLASS_P (*tp)){
      n_debug("mtcsreplace.c remap_gimple_op_r 33\n");

      /* If this is a constant, we have to copy the node iff the type
      will be remapped.  copy_tree_r will not copy a constant.  */
      tree new_type = remap_type (TREE_TYPE (*tp), id);

      if (new_type == TREE_TYPE (*tp))
         *walk_subtrees = 0;

      else if (TREE_CODE (*tp) == INTEGER_CST)
         *tp = wide_int_to_tree (new_type, wi::to_wide (*tp));
      else{
         *tp = copy_node (*tp);
         TREE_TYPE (*tp) = new_type;
      }
   }else{
      n_debug("mtcsreplace.c remap_gimple_op_r 44\n");

      /* Otherwise, just copy the node.  Note that copy_tree_r already
      knows not to copy VAR_DECLs, etc., so this is safe.  */

      if (TREE_CODE (*tp) == MEM_REF && !id->do_not_fold){
         n_debug("mtcsreplace.c remap_gimple_op_r 55\n");

         /* We need to re-canonicalize MEM_REFs from inline substitutions
         that can happen when a pointer argument is an ADDR_EXPR.
         Recurse here manually to allow that.  */
         tree ptr = TREE_OPERAND (*tp, 0);
         tree type = remap_type (TREE_TYPE (*tp), id);
         tree old = *tp;
         walk_tree (&ptr, remap_gimple_op_r, data, NULL);
         *tp = fold_build2 (MEM_REF, type, ptr, TREE_OPERAND (*tp, 1));
         TREE_THIS_VOLATILE (*tp) = TREE_THIS_VOLATILE (old);
         TREE_SIDE_EFFECTS (*tp) = TREE_SIDE_EFFECTS (old);
         copy_warning (*tp, old);
         if (MR_DEPENDENCE_CLIQUE (old) != 0){
            MR_DEPENDENCE_CLIQUE (*tp) = remap_dependence_clique (id, MR_DEPENDENCE_CLIQUE (old));
            MR_DEPENDENCE_BASE (*tp) = MR_DEPENDENCE_BASE (old);
         }
         /* We cannot propagate the TREE_THIS_NOTRAP flag if we have
         remapped a parameter as the property might be valid only
         for the parameter itself.  */
         if (TREE_THIS_NOTRAP (old) && (!is_parm (TREE_OPERAND (old, 0))
         || (!id->transform_parameter && is_parm (ptr))))
            TREE_THIS_NOTRAP (*tp) = 1;
         REF_REVERSE_STORAGE_ORDER (*tp) = REF_REVERSE_STORAGE_ORDER (old);
         *walk_subtrees = 0;
         return NULL;
      }

      /* Here is the "usual case".  Copy this tree node, and then
      tweak some special cases.  */
      copy_tree_r (tp, walk_subtrees, NULL);

      if (TREE_CODE (*tp) != OMP_CLAUSE)
         TREE_TYPE (*tp) = remap_type (TREE_TYPE (*tp), id);

      if (TREE_CODE (*tp) == TARGET_EXPR && TREE_OPERAND (*tp, 3)){
         n_debug("mtcsreplace.c remap_gimple_op_r 66\n");

         /* The copied TARGET_EXPR has never been expanded, even if the
         original node was expanded already.  */
         TREE_OPERAND (*tp, 1) = TREE_OPERAND (*tp, 3);
         TREE_OPERAND (*tp, 3) = NULL_TREE;
      }else if (TREE_CODE (*tp) == ADDR_EXPR){
         n_debug("mtcsreplace.c remap_gimple_op_r 77\n");

         /* Variable substitution need not be simple.  In particular,
         the MEM_REF substitution above.  Make sure that
         TREE_CONSTANT and friends are up-to-date.  */
         int invariant = is_gimple_min_invariant (*tp);
         walk_tree (&TREE_OPERAND (*tp, 0), remap_gimple_op_r, data, NULL);
         recompute_tree_invariant_for_addr_expr (*tp);

         /* If this used to be invariant, but is not any longer,
         then regimplification is probably needed.  */
         if (invariant && !is_gimple_min_invariant (*tp))
            id->regimplify = true;

         *walk_subtrees = 0;
      }else if (TREE_CODE (*tp) == OMP_NEXT_VARIANT){

         /* Neither operand is interesting, and walking the selector
         causes problems because it's not an expression.  */
         gcc_assert (TREE_CODE (TREE_OPERAND (*tp, 0)) == INTEGER_CST);
         *walk_subtrees = 0;
      }
   }

   /* Update the TREE_BLOCK for the cloned expr.  */
   if (EXPR_P (*tp)){
      n_debug("mtcsreplace.c remap_gimple_op_r 88\n");

      tree new_block = id->remapping_type_depth == 0 ? id->block : NULL;
      tree old_block = TREE_BLOCK (*tp);
      if (old_block){
         tree *n;
         n = id->decl_map->get (TREE_BLOCK (*tp));
         if (n)
            new_block = *n;
      }
      TREE_SET_BLOCK (*tp, new_block);
   }

   /* Keep iterating.  */
   return NULL_TREE;
}

/* Helper for remap_gimple_stmt.  Given an EH region number for the
   source function, map that to the duplicate EH region number in
   the destination function.  */
static int remap_eh_region_nr (int old_nr, copy_body_data *id)
{
   eh_region old_r, new_r;
   old_r = get_eh_region_from_number_fn (id->src_cfun, old_nr);
   new_r = static_cast<eh_region> (*id->eh_map->get (old_r));

   return new_r->index;
}

/* Similar, but operate on INTEGER_CSTs.  */
static tree remap_eh_region_tree_nr (tree old_t_nr, copy_body_data *id)
{
   int old_nr, new_nr;
   old_nr = tree_to_shwi (old_t_nr);
   new_nr = remap_eh_region_nr (old_nr, id);
   return build_int_cst (integer_type_node, new_nr);
}

/* Decide if DECL can be put into BLOCK_NONLOCAL_VARs.  */

static bool can_be_nonlocal (tree decl, copy_body_data *id)
{
   /* We cannot duplicate function decls.  */
   if (TREE_CODE (decl) == FUNCTION_DECL)
      return true;

   /* Local static vars must be non-local or we get multiple declaration
   problems.  */
   if (VAR_P (decl) && !auto_var_in_fn_p (decl, id->src_fn))
      return true;

   return false;
}

static tree remap_decls (tree decls, vec<tree, va_gc> **nonlocalized_list,copy_body_data *id)
{
   tree old_var;
   tree new_decls = NULL_TREE;

   /* Remap its variables.  */
   for (old_var = decls; old_var; old_var = DECL_CHAIN (old_var)){
      tree new_var;

      if (can_be_nonlocal (old_var, id)){
         /* We need to add this variable to the local decls as otherwise
         nothing else will do so.  */
         if (VAR_P (old_var) && ! DECL_EXTERNAL (old_var) && cfun)
            add_local_decl (cfun, old_var);
         if ((!optimize || debug_info_level > DINFO_LEVEL_TERSE) && !DECL_IGNORED_P (old_var)  && nonlocalized_list)
            vec_safe_push (*nonlocalized_list, old_var);
         continue;
      }

      /* Remap the variable.  */
      new_var = remap_decl (old_var, id);
      /* If we didn't remap this variable, we can't mess with its
      TREE_CHAIN.  If we remapped this variable to the return slot, it's
      already declared somewhere else, so don't declare it here.  */
      if (new_var == old_var || new_var == id->retvar)
         ;
      else if (!new_var){
         if ((!optimize || debug_info_level > DINFO_LEVEL_TERSE)  && !DECL_IGNORED_P (old_var)  && nonlocalized_list)
            vec_safe_push (*nonlocalized_list, old_var);
      } else{
         gcc_assert (DECL_P (new_var));
         DECL_CHAIN (new_var) = new_decls;
         new_decls = new_var;

         /* Also copy value-expressions.  */
         if (VAR_P (new_var) && DECL_HAS_VALUE_EXPR_P (new_var)){
            tree tem = DECL_VALUE_EXPR (new_var);
            bool old_regimplify = id->regimplify;
            id->remapping_type_depth++;
            walk_tree (&tem, copy_tree_body_r, id, NULL);
            id->remapping_type_depth--;
            id->regimplify = old_regimplify;
            SET_DECL_VALUE_EXPR (new_var, tem);
         }
      }
   }
   return nreverse (new_decls);
}

/* Copy the BLOCK to contain remapped versions of the variables
   therein.  And hook the new block into the block-tree.  */
static void remap_block (tree *block, copy_body_data *id)
{
   tree old_block;
   tree new_block;

   /* Make the new block.  */
   old_block = *block;
   new_block = make_node (BLOCK);
   TREE_USED (new_block) = TREE_USED (old_block);
   BLOCK_ABSTRACT_ORIGIN (new_block) = BLOCK_ORIGIN (old_block);
   BLOCK_SOURCE_LOCATION (new_block) = BLOCK_SOURCE_LOCATION (old_block);
   BLOCK_NONLOCALIZED_VARS (new_block) = vec_safe_copy (BLOCK_NONLOCALIZED_VARS (old_block));
   *block = new_block;

   /* Remap its variables.  */
   BLOCK_VARS (new_block) = remap_decls (BLOCK_VARS (old_block),
         &BLOCK_NONLOCALIZED_VARS (new_block),id);

   /* Remember the remapped block.  */
   insert_decl_map (id, old_block, new_block);
}


static gimple_seq remap_gimple_seq (gimple_seq body, copy_body_data *id)
{
   gimple_stmt_iterator si;
   gimple_seq new_body = NULL;

   for (si = gsi_start (body); !gsi_end_p (si); gsi_next (&si)){
      gimple_seq new_stmts = remap_gimple_stmt (gsi_stmt (si), id);
      gimple_seq_add_seq (&new_body, new_stmts);
   }

   return new_body;
}

/* Copy a GIMPLE_BIND statement STMT, remapping all the symbols in its
   block using the mapping information in ID.  */
static gimple * copy_gimple_bind (gbind *stmt, copy_body_data *id)
{
   gimple *new_bind;
   tree new_block, new_vars;
   gimple_seq body, new_body;

   /* Copy the statement.  Note that we purposely don't use copy_stmt
   here because we need to remap statements as we copy.  */
   body = gimple_bind_body (stmt);
   new_body = remap_gimple_seq (body, id);

   new_block = gimple_bind_block (stmt);
   if (new_block)
      remap_block (&new_block, id);

   /* This will remap a lot of the same decls again, but this should be
   harmless.  */
   new_vars = gimple_bind_vars (stmt);
   if (new_vars)
      new_vars = remap_decls (new_vars, NULL, id);

   new_bind = gimple_build_bind (new_vars, new_body, new_block);

   return new_bind;
}

static gimple_seq remap_gimple_stmt (gimple *stmt, copy_body_data *id)
{
   gimple *copy = NULL;
   struct walk_stmt_info wi;
   bool skip_first = false;
   gimple_seq stmts = NULL;

   if (is_gimple_debug (stmt)
   && (gimple_debug_nonbind_marker_p (stmt)
   ? !DECL_STRUCT_FUNCTION (id->dst_fn)->debug_nonbind_markers
   : !opt_for_fn (id->dst_fn, flag_var_tracking_assignments)))
      return NULL;

   if (!is_gimple_debug (stmt)  && id->param_body_adjs
   && id->param_body_adjs->m_dead_stmts.contains (stmt)){
      tree *dval = id->param_body_adjs->m_dead_stmt_debug_equiv.get (stmt);
      if (!dval)
         return NULL;

      gcc_assert (is_gimple_assign (stmt));
      tree lhs = gimple_assign_lhs (stmt);
      tree *dvar = id->param_body_adjs->m_dead_ssa_debug_equiv.get (lhs);
      gdebug *bind = gimple_build_debug_bind (*dvar, *dval, stmt);
      if (id->reset_location)
         gimple_set_location (bind, input_location);
      id->debug_stmts.safe_push (bind);
      gimple_seq_add_stmt_without_update (&stmts, bind);
      return stmts;
   }
   n_debug("mtcsreplace.c remap_gimple_stmt 00\n");

   /* Begin by recognizing trees that we'll completely rewrite for the
   inlining context.  Our output for these trees is completely
   different from our input (e.g. RETURN_EXPR is deleted and morphs
   into an edge).  Further down, we'll handle trees that get
   duplicated and/or tweaked.  */

   /* When requested, GIMPLE_RETURN should be transformed to just the
   contained GIMPLE_ASSIGN.  The branch semantics of the return will
   be handled elsewhere by manipulating the CFG rather than the
   statement.  */
   if (gimple_code (stmt) == GIMPLE_RETURN && id->transform_return_to_modify){
      n_debug("mtcsreplace.c remap_gimple_stmt 11\n");
      tree retval = gimple_return_retval (as_a <greturn *> (stmt));

      /* If we're returning something, just turn that into an
      assignment to the equivalent of the original RESULT_DECL.
      If RETVAL is just the result decl, the result decl has
      already been set (e.g. a recent "foo (&result_decl, ...)");
      just toss the entire GIMPLE_RETURN.  Likewise for when the
      call doesn't want the return value.  */
      if (retval
      && (TREE_CODE (retval) != RESULT_DECL
      && (!id->call_stmt
      || gimple_call_lhs (id->call_stmt) != NULL_TREE)
      && (TREE_CODE (retval) != SSA_NAME
      || ! SSA_NAME_VAR (retval)
      || TREE_CODE (SSA_NAME_VAR (retval)) != RESULT_DECL))){
         copy = gimple_build_assign (id->do_not_unshare ? id->retvar : unshare_expr (id->retvar), retval);
         /* id->retvar is already substituted.  Skip it on later remapping.  */
         skip_first = true;
      }else
         return NULL;
   }else if (gimple_has_substatements (stmt)){
      gimple_seq s1, s2;
      n_debug("mtcsreplace.c remap_gimple_stmt 11\n");

      /* When cloning bodies from the C++ front end, we will be handed bodies
      in High GIMPLE form.  Handle here all the High GIMPLE statements that
      have embedded statements.  */
      switch (gimple_code (stmt)){
         case GIMPLE_BIND:
            copy = copy_gimple_bind (as_a <gbind *> (stmt), id);
            break;

         case GIMPLE_CATCH:
         {
            gcatch *catch_stmt = as_a <gcatch *> (stmt);
            s1 = remap_gimple_seq (gimple_catch_handler (catch_stmt), id);
            copy = gimple_build_catch (gimple_catch_types (catch_stmt), s1);
         }
            break;

         case GIMPLE_EH_FILTER:
            s1 = remap_gimple_seq (gimple_eh_filter_failure (stmt), id);
            copy = gimple_build_eh_filter (gimple_eh_filter_types (stmt), s1);
            break;

         case GIMPLE_TRY:
            s1 = remap_gimple_seq (gimple_try_eval (stmt), id);
            s2 = remap_gimple_seq (gimple_try_cleanup (stmt), id);
            copy = gimple_build_try (s1, s2, gimple_try_kind (stmt));
            break;

         case GIMPLE_WITH_CLEANUP_EXPR:
            s1 = remap_gimple_seq (gimple_wce_cleanup (stmt), id);
            copy = gimple_build_wce (s1);
            break;

         case GIMPLE_ASSUME:
            s1 = remap_gimple_seq (gimple_assume_body (stmt), id);
            copy = gimple_build_assume (gimple_assume_guard (stmt), s1);
            break;

         case GIMPLE_TRANSACTION:
         {
            gtransaction *old_trans_stmt = as_a <gtransaction *> (stmt);
            gtransaction *new_trans_stmt;
            s1 = remap_gimple_seq (gimple_transaction_body (old_trans_stmt),
            id);
            copy = new_trans_stmt = gimple_build_transaction (s1);
            gimple_transaction_set_subcode (new_trans_stmt,
            gimple_transaction_subcode (old_trans_stmt));
            gimple_transaction_set_label_norm (new_trans_stmt,
            gimple_transaction_label_norm (old_trans_stmt));
            gimple_transaction_set_label_uninst (new_trans_stmt,
            gimple_transaction_label_uninst (old_trans_stmt));
            gimple_transaction_set_label_over (new_trans_stmt,
            gimple_transaction_label_over (old_trans_stmt));
         }
            break;

         default:
            gcc_unreachable ();
      }
   }else{
      if (gimple_assign_single_p (stmt)
      && gimple_assign_lhs (stmt) == gimple_assign_rhs1 (stmt)
      && auto_var_in_fn_p (gimple_assign_lhs (stmt), id->src_fn)){
         /* Here we handle statements that are not completely rewritten.
         First we detect some inlining-induced bogosities for
         discarding.  */
         n_debug("mtcsreplace.c remap_gimple_stmt 22\n");

         /* Some assignments VAR = VAR; don't generate any rtl code
         and thus don't count as variable modification.  Avoid
         keeping bogosities like 0 = 0.  */
         tree decl = gimple_assign_lhs (stmt), value;
         tree *n;

         n = id->decl_map->get (decl);
         if (n){
            value = *n;
            STRIP_TYPE_NOPS (value);
            if (TREE_CONSTANT (value) || TREE_READONLY (value))
               return NULL;
         }
      }

      /* For *ptr_N ={v} {CLOBBER}, if ptr_N is SSA_NAME defined
      in a block that we aren't copying during tree_function_versioning,
      just drop the clobber stmt.  */
      if (id->blocks_to_copy && gimple_clobber_p (stmt)){
         n_debug("mtcsreplace.c remap_gimple_stmt 33\n");

         tree lhs = gimple_assign_lhs (stmt);
         if (TREE_CODE (lhs) == MEM_REF  && TREE_CODE (TREE_OPERAND (lhs, 0)) == SSA_NAME){
            gimple *def_stmt = SSA_NAME_DEF_STMT (TREE_OPERAND (lhs, 0));
            if (gimple_bb (def_stmt) && !bitmap_bit_p (id->blocks_to_copy,gimple_bb (def_stmt)->index))
               return NULL;
         }
      }

      /* We do not allow CLOBBERs of handled components.  In case
      returned value is stored via such handled component, remove
      the clobber so stmt verifier is happy.  */
      if (gimple_clobber_p (stmt) && TREE_CODE (gimple_assign_lhs (stmt)) == RESULT_DECL){
         n_debug("mtcsreplace.c remap_gimple_stmt 44\n");

         tree remapped = remap_decl (gimple_assign_lhs (stmt), id);
         if (!DECL_P (remapped)   && TREE_CODE (remapped) != MEM_REF)
            return NULL;
      }

      if (gimple_debug_bind_p (stmt)){
         n_debug("mtcsreplace.c remap_gimple_stmt 55\n");

         tree var = gimple_debug_bind_get_var (stmt);
         tree value = gimple_debug_bind_get_value (stmt);
         if (id->param_body_adjs && id->param_body_adjs->m_dead_stmts.contains (stmt)){
            value = unshare_expr_without_location (value);
            id->param_body_adjs->remap_with_debug_expressions (&value);
         }

         gdebug *copy = gimple_build_debug_bind (var, value, stmt);
         if (id->reset_location)
            gimple_set_location (copy, input_location);
         id->debug_stmts.safe_push (copy);
         gimple_seq_add_stmt_without_update (&stmts, copy);
         return stmts;
      }
      if (gimple_debug_source_bind_p (stmt)){
         n_debug("mtcsreplace.c remap_gimple_stmt 66\n");

         gdebug *copy = gimple_build_debug_source_bind (gimple_debug_source_bind_get_var (stmt),
               gimple_debug_source_bind_get_value (stmt),stmt);
         if (id->reset_location)
            gimple_set_location (copy, input_location);
         id->debug_stmts.safe_push (copy);
         gimple_seq_add_stmt_without_update (&stmts, copy);
         return stmts;
      }
      if (gimple_debug_nonbind_marker_p (stmt)){
         n_debug("mtcsreplace.c remap_gimple_stmt 77\n");

         /* If the inlined function has too many debug markers,
         don't copy them.  */
         if (id->src_cfun->debug_marker_count > param_max_debug_marker_count || id->reset_location)
            return stmts;

         gdebug *copy = as_a <gdebug *> (gimple_copy (stmt));
         id->debug_stmts.safe_push (copy);
         gimple_seq_add_stmt_without_update (&stmts, copy);
         return stmts;
      }

      /* Create a new deep copy of the statement.  */
      copy = gimple_copy (stmt);

      /* Clear flags that need revisiting.  */
      if (gcall *call_stmt = dyn_cast <gcall *> (copy)){
         n_debug("mtcsreplace.c remap_gimple_stmt 88\n");

         if (gimple_call_tail_p (call_stmt))
            gimple_call_set_tail (call_stmt, false);
         if (gimple_call_from_thunk_p (call_stmt))
            gimple_call_set_from_thunk (call_stmt, false);
         /* Silently clear musttail flag when inlining a function
         with must tail call from a non-musttail call.  The inlining
         removes one frame so acts like musttail's intent, and we
         can be inlining a function with musttail calls in the middle
         of caller where musttail will always error.  */
         if (gimple_call_must_tail_p (call_stmt)
         && id->call_stmt
         && !gimple_call_must_tail_p (id->call_stmt))
            gimple_call_set_must_tail (call_stmt, false);

      }
      n_debug("mtcsreplace.c remap_gimple_stmt 99 id->eh_map:%p\n",id->eh_map);

      /* Remap the region numbers for __builtin_eh_{pointer,filter},
      RESX and EH_DISPATCH.  */
      if (id->eh_map)
         switch (gimple_code (copy)){
            case GIMPLE_CALL:
            {
               tree r, fndecl = gimple_call_fndecl (copy);
               if (fndecl && fndecl_built_in_p (fndecl, BUILT_IN_NORMAL))
                  switch (DECL_FUNCTION_CODE (fndecl)){
                     case BUILT_IN_EH_COPY_VALUES:
                        r = gimple_call_arg (copy, 1);
                        r = remap_eh_region_tree_nr (r, id);
                        gimple_call_set_arg (copy, 1, r);
                        /* FALLTHRU */

                     case BUILT_IN_EH_POINTER:
                     case BUILT_IN_EH_FILTER:
                        r = gimple_call_arg (copy, 0);
                        r = remap_eh_region_tree_nr (r, id);
                        gimple_call_set_arg (copy, 0, r);
                        break;

                     default:
                        break;
                  }

               /* Reset alias info if we didn't apply measures to
               keep it valid over inlining by setting DECL_PT_UID.  */
               if (!id->src_cfun->gimple_df || !id->src_cfun->gimple_df->ipa_pta)
                  gimple_call_reset_alias_info (as_a <gcall *> (copy));
            }
               break;

            case GIMPLE_RESX:
            {
               gresx *resx_stmt = as_a <gresx *> (copy);
               int r = gimple_resx_region (resx_stmt);
               r = remap_eh_region_nr (r, id);
               gimple_resx_set_region (resx_stmt, r);
            }
               break;

            case GIMPLE_EH_DISPATCH:
            {
               geh_dispatch *eh_dispatch = as_a <geh_dispatch *> (copy);
               int r = gimple_eh_dispatch_region (eh_dispatch);
               r = remap_eh_region_nr (r, id);
               gimple_eh_dispatch_set_region (eh_dispatch, r);
            }
               break;

            default:
               break;
         }
   }

   /* If STMT has a block defined, map it to the newly constructed block.  */
   if (tree block = gimple_block (copy)){
      tree *n;
      n = id->decl_map->get (block);
      gcc_assert (n);
      gimple_set_block (copy, *n);
   }
   if (id->param_body_adjs){
      n_debug("mtcsreplace.c remap_gimple_stmt 101\n");

      gimple_seq extra_stmts = NULL;
      id->param_body_adjs->modify_gimple_stmt (&copy, &extra_stmts, stmt);
      if (!gimple_seq_empty_p (extra_stmts)) {
         memset (&wi, 0, sizeof (wi));
         wi.info = id;
         for (gimple_stmt_iterator egsi = gsi_start (extra_stmts); !gsi_end_p (egsi);  gsi_next (&egsi))
            walk_gimple_op (gsi_stmt (egsi), remap_gimple_op_r, &wi);
         gimple_seq_add_seq_without_update (&stmts, extra_stmts);
      }
   }

   if (id->reset_location)
      gimple_set_location (copy, input_location);

   /* Debug statements ought to be rebuilt and not copied.  */
   gcc_checking_assert (!is_gimple_debug (copy));
   n_debug("mtcsreplace.c remap_gimple_stmt 102 skip_first:%d\n",skip_first);

   /* Remap all the operands in COPY.  */
   memset (&wi, 0, sizeof (wi));
   wi.info = id;
   if (skip_first)
      walk_tree (gimple_op_ptr (copy, 1), remap_gimple_op_r, &wi, NULL);
   else
      walk_gimple_op (copy, remap_gimple_op_r, &wi);
   n_debug("mtcsreplace.c remap_gimple_stmt 103\n");

   /* Clear the copied virtual operands.  We are not remapping them here
   but are going to recreate them from scratch.  */
   if (gimple_has_mem_ops (copy)){
      n_debug("mtcsreplace.c remap_gimple_stmt 104\n");

      gimple_set_vdef (copy, NULL_TREE);
      gimple_set_vuse (copy, NULL_TREE);
   }

   if (cfun->can_throw_non_call_exceptions){
      n_debug("mtcsreplace.c remap_gimple_stmt 105\n");

      /* When inlining a function which does not have non-call exceptions
      enabled into a function that has (which only happens with
      always-inline) we have to fixup stmts that cannot throw.  */
      if (gcond *cond = dyn_cast <gcond *> (copy))
         if (gimple_could_trap_p (cond)){
            gassign *cmp = gimple_build_assign (make_ssa_name (boolean_type_node),
            gimple_cond_code (cond),
            gimple_cond_lhs (cond),
            gimple_cond_rhs (cond));
            gimple_seq_add_stmt_without_update (&stmts, cmp);
            gimple_cond_set_code (cond, NE_EXPR);
            gimple_cond_set_lhs (cond, gimple_assign_lhs (cmp));
            gimple_cond_set_rhs (cond, boolean_false_node);
         }
   }

   gimple_seq_add_stmt_without_update (&stmts, copy);
   return stmts;
}

static tree copy_decl_maybe_to_var (tree decl, copy_body_data *id)
{
   if (TREE_CODE (decl) == PARM_DECL || TREE_CODE (decl) == RESULT_DECL)
      return copy_decl_to_var (decl, id);
   else
      return copy_decl_no_change (decl, id);
}


/**
 * 替换intrinsic调用
 * 把 调用intrinsic 函数声明 替换为 intrinsic实现
 * 也即  Target.ldcs((int4 *)ptr); 替换为 CudaIntrinsics.ldcs
 * public$ __attribute__((mii("cuda"))) class$  CudaIntrinsics{
   public$ static  __attribute__((used)) __device__ int4* ldcs(const int4 *ptr) {
      ...
   }
};

__attribute__((mic())) class$  Target{
#pragma mtcs intrinsic
   public$ static __device__ int4 * ldcs(const int4 *ptr);
};

class$ TFirst  {
};
impl$ TFirst{
    __global__ void test(float *ptr){
       Target.ldcs((int4 *)ptr);
    }
};
 */
nuint mtcs_replace_replace_intrinsic(MtcsReplace *self,gcall *call)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   AetMediatorUser *mediatorUser =(AetMediatorUser *)mtcs_compile_get();
   AetMediator *mediator = mediatorUser->mediator;

   tree micFndecl = gimple_call_fndecl (call);
   //miifndecl来自主机的
   tree miifndecl = aet_mediator_get_mii_function(mediator,micFndecl,mediatorUser);
   if(miifndecl){
      char *name = IDENTIFIER_POINTER(DECL_NAME(miifndecl));
      cgraph_node *cnode;
      int i=0;
      tree targetMiiFndecl = NULL_TREE;
      //在当前符号表中找出函数名相同的函数
      FOR_EACH_FUNCTION (cnode){
         n_debug("mtcsreplace.c mtcs_replace_replace_intrinsic 当前符号表中的 i:%d node:%p name:%s decl:%p init:%p 是否定义:%d\n",
               i++,cnode,cnode->name(),cnode->decl,DECL_INITIAL(cnode->decl),cnode->definition);
         n_debug("mtcsreplace.c mtcs_replace_replace_intrinsic 当前符号表中的 %p %s\n",miifndecl,
               IDENTIFIER_POINTER(DECL_NAME(miifndecl)));
         if(strcmp(name,cnode->name())==0){
            targetMiiFndecl = cnode->decl;
            break;
         }
      }
      tree  fn = build_fold_addr_expr(miifndecl);
      gimple_set_op (call, 1, fn);
      gimple_call_set_fntype (call, TREE_TYPE (TREE_TYPE (fn)));
      gimple_call_reset_alias_info (call);
      gimple_call_set_fndecl (call, miifndecl);
      cgraph_update_edges_for_call_stmt (call, miifndecl, call);
      return 1;
   }else{
      const char *platName =  mtcs_target_get_platform_name(mtcsTarget);
      const char *isa = mtcs_target_get_isa_string(mtcsTarget);
      error_at(gimple_location(call),"mic类中的 intrinsic 方法在平台%s(%s)中还没有实现，或参数与实现不区配。",platName,isa);
   }
   return 0;
}


//////////////////----------------------新的--------------------
static void prepend_lexical_block (tree current_block, tree new_block);
static void declare_inline_vars (tree block, tree vars);
static void insert_init_stmt (copy_body_data *id, basic_block bb, gimple *init_stmt);
static location_t remap_location (location_t locus, copy_body_data *id);

/* Call redirect_call_stmt_to_callee on all calls in BB.  */

static void redirect_all_calls_new (copy_body_data * id, basic_block bb)
{
  gimple_stmt_iterator si;
  gimple *last = last_nondebug_stmt (bb);
  for (si = gsi_start_bb (bb); !gsi_end_p (si); gsi_next (&si))
    {
      gimple *stmt = gsi_stmt (si);
      if (is_gimple_call (stmt))
   {
     struct cgraph_edge *edge = id->dst_node->get_edge (stmt);
     if (edge)
       {
         if (!id->killed_new_ssa_names)
      id->killed_new_ssa_names = new hash_set<tree> (16);
         cgraph_edge::redirect_call_stmt_to_callee (edge,
      id->killed_new_ssa_names);

         if (stmt == last && id->call_stmt && maybe_clean_eh_stmt (stmt))
      gimple_purge_dead_eh_edges (bb);
       }
   }
    }
}

/* Copy the debug STMT using ID.  We deal with these statements in a
   special way: if any variable in their VALUE expression wasn't
   remapped yet, we won't remap it, because that would get decl uids
   out of sync, causing codegen differences between -g and -g0.  If
   this arises, we drop the VALUE expression altogether.  */
static void copy_debug_stmt (gdebug *stmt, copy_body_data *id)
{
  tree t, *n;
  struct walk_stmt_info wi;

  if (tree block = gimple_block (stmt))
    {
      n = id->decl_map->get (block);
      gimple_set_block (stmt, n ? *n : id->block);
    }

  if (gimple_debug_nonbind_marker_p (stmt))
    {
      if (id->call_stmt && !gimple_block (stmt))
   {
     gimple_stmt_iterator gsi = gsi_for_stmt (stmt);
     gsi_remove (&gsi, true);
   }
      return;
    }

  /* Remap all the operands in COPY.  */
  memset (&wi, 0, sizeof (wi));
  wi.info = id;

  processing_debug_stmt = 1;

  if (gimple_debug_source_bind_p (stmt))
    t = gimple_debug_source_bind_get_var (stmt);
  else if (gimple_debug_bind_p (stmt))
    t = gimple_debug_bind_get_var (stmt);
  else
    gcc_unreachable ();

  if (TREE_CODE (t) == PARM_DECL
      && id->debug_map
      && (n = id->debug_map->get (t)))
    {
      gcc_assert (VAR_P (*n));
      t = *n;
    }
  else if (VAR_P (t) && !is_global_var (t) && !id->decl_map->get (t))
    /* T is a non-localized variable.  */;
  else
    walk_tree (&t, remap_gimple_op_r, &wi, NULL);

  if (gimple_debug_bind_p (stmt))
    {
      gimple_debug_bind_set_var (stmt, t);

      if (gimple_debug_bind_has_value_p (stmt))
   walk_tree (gimple_debug_bind_get_value_ptr (stmt),
         remap_gimple_op_r, &wi, NULL);

      /* Punt if any decl couldn't be remapped.  */
      if (processing_debug_stmt < 0)
   gimple_debug_bind_reset_value (stmt);
    }
  else if (gimple_debug_source_bind_p (stmt))
    {
      gimple_debug_source_bind_set_var (stmt, t);
      /* When inlining and source bind refers to one of the optimized
    away parameters, change the source bind into normal debug bind
    referring to the corresponding DEBUG_EXPR_DECL that should have
    been bound before the call stmt.  */
      t = gimple_debug_source_bind_get_value (stmt);
      if (t != NULL_TREE
     && TREE_CODE (t) == PARM_DECL
     && id->call_stmt)
   {
     vec<tree, va_gc> **debug_args = decl_debug_args_lookup (id->src_fn);
     unsigned int i;
     if (debug_args != NULL)
       {
         for (i = 0; i < vec_safe_length (*debug_args); i += 2)
      if ((**debug_args)[i] == DECL_ORIGIN (t)
          && TREE_CODE ((**debug_args)[i + 1]) == DEBUG_EXPR_DECL)
        {
          t = (**debug_args)[i + 1];
          stmt->subcode = GIMPLE_DEBUG_BIND;
          gimple_debug_bind_set_value (stmt, t);
          break;
        }
       }
   }
      if (gimple_debug_source_bind_p (stmt))
   walk_tree (gimple_debug_source_bind_get_value_ptr (stmt),
         remap_gimple_op_r, &wi, NULL);
    }

  processing_debug_stmt = 0;

  update_stmt (stmt);
}

/* Add to BINDINGS a debug stmt resetting SRCVAR if inlining might
   have brought in or introduced any debug stmts for SRCVAR.  */

static inline void
reset_debug_binding (copy_body_data *id, tree srcvar, gimple_seq *bindings)
{
  tree *remappedvarp = id->decl_map->get (srcvar);

  if (!remappedvarp)
    return;

  if (!VAR_P (*remappedvarp))
    return;

  if (*remappedvarp == id->retvar)
    return;

  tree tvar = target_for_debug_bind (*remappedvarp);
  if (!tvar)
    return;

  gdebug *stmt = gimple_build_debug_bind (tvar, NULL_TREE,
                 id->call_stmt);
  gimple_seq_add_stmt (bindings, stmt);
}



/* Walk all basic blocks created after FIRST and try to fold every statement
   in the STATEMENTS pointer set.  */

static void
fold_marked_statements (int first, hash_set<gimple *> *statements)
{
  auto_bitmap to_purge;
  auto_bitmap to_purge_abnormal;

  auto_vec<edge, 20> stack (n_basic_blocks_for_fn (cfun) + 2);
  auto_sbitmap visited (last_basic_block_for_fn (cfun));
  bitmap_clear (visited);

  stack.quick_push (single_succ_edge (ENTRY_BLOCK_PTR_FOR_FN (cfun)));
  while (!stack.is_empty ())
    {
      /* Look at the edge on the top of the stack.  */
      edge e = stack.pop ();
      basic_block dest = e->dest;

      if (dest == EXIT_BLOCK_PTR_FOR_FN (cfun)
     || bitmap_bit_p (visited, dest->index))
   continue;

      bitmap_set_bit (visited, dest->index);

      if (dest->index >= first)
   for (gimple_stmt_iterator gsi = gsi_start_bb (dest);
        !gsi_end_p (gsi); gsi_next (&gsi))
     {
       if (!statements->contains (gsi_stmt (gsi)))
         continue;

       gimple *old_stmt = gsi_stmt (gsi);
       bool can_make_abnormal_goto = false;
       tree old_decl = NULL_TREE;

       if (is_gimple_call (old_stmt))
         {
      old_decl = gimple_call_fndecl (old_stmt);
      if (stmt_can_make_abnormal_goto (old_stmt))
        can_make_abnormal_goto = true;
         }

       if (old_decl && fndecl_built_in_p (old_decl))
         {
      /* Folding builtins can create multiple instructions,
         we need to look at all of them.  */
      gimple_stmt_iterator i2 = gsi;
      gsi_prev (&i2);
      if (fold_stmt (&gsi))
        {
          gimple *new_stmt;
          /* If a builtin at the end of a bb folded into nothing,
             the following loop won't work.  */
          if (gsi_end_p (gsi))
            {
         cgraph_update_edges_for_call_stmt (old_stmt,
                        old_decl, NULL);
         if (can_make_abnormal_goto)
           bitmap_set_bit (to_purge_abnormal, dest->index);
         break;
            }
          if (gsi_end_p (i2))
            i2 = gsi_start_bb (dest);
          else
            gsi_next (&i2);
          while (1)
            {
         new_stmt = gsi_stmt (i2);
         update_stmt (new_stmt);
         cgraph_update_edges_for_call_stmt (old_stmt, old_decl,
                        new_stmt);

         if (new_stmt == gsi_stmt (gsi))
           {
             /* It is okay to check only for the very last
                of these statements.  If it is a throwing
                statement nothing will change.  If it isn't
                this can remove EH edges.  If that weren't
                correct then because some intermediate stmts
                throw, but not the last one.  That would mean
                we'd have to split the block, which we can't
                here and we'd loose anyway.  And as builtins
                probably never throw, this all
                is mood anyway.  */
             if (maybe_clean_or_replace_eh_stmt (old_stmt,
                        new_stmt))
               bitmap_set_bit (to_purge, dest->index);
             if (can_make_abnormal_goto
            && !stmt_can_make_abnormal_goto (new_stmt))
               bitmap_set_bit (to_purge_abnormal, dest->index);
             break;
           }
         gsi_next (&i2);
            }
        }
         }
       else if (fold_stmt (&gsi))
         {
      /* Re-read the statement from GSI as fold_stmt() may
         have changed it.  */
      gimple *new_stmt = gsi_stmt (gsi);
      update_stmt (new_stmt);

      if (is_gimple_call (old_stmt)
          || is_gimple_call (new_stmt))
        cgraph_update_edges_for_call_stmt (old_stmt, old_decl,
                       new_stmt);

      if (maybe_clean_or_replace_eh_stmt (old_stmt, new_stmt))
        bitmap_set_bit (to_purge, dest->index);
      if (can_make_abnormal_goto
          && !stmt_can_make_abnormal_goto (new_stmt))
        bitmap_set_bit (to_purge_abnormal, dest->index);
         }
     }

      if (EDGE_COUNT (dest->succs) > 0)
   {
     /* Avoid warnings emitted from folding statements that
        became unreachable because of inlined function parameter
        propagation.  */
     e = find_taken_edge (dest, NULL_TREE);
     if (e)
       stack.quick_push (e);
     else
       {
         edge_iterator ei;
         FOR_EACH_EDGE (e, ei, dest->succs)
      stack.safe_push (e);
       }
   }
    }

  gimple_purge_all_dead_eh_edges (to_purge);
  gimple_purge_all_dead_abnormal_call_edges (to_purge_abnormal);
}

/* For each inlined variable for which we may have debug bind stmts,
   add before GSI a final debug stmt resetting it, marking the end of
   its life, so that var-tracking knows it doesn't have to compute
   further locations for it.  */

static inline void
reset_debug_bindings (copy_body_data *id, gimple_stmt_iterator gsi)
{
  tree var;
  unsigned ix;
  gimple_seq bindings = NULL;

  if (!gimple_in_ssa_p (id->src_cfun))
    return;

  if (!opt_for_fn (id->dst_fn, flag_var_tracking_assignments))
    return;

  for (var = DECL_ARGUMENTS (id->src_fn);
       var; var = DECL_CHAIN (var))
    reset_debug_binding (id, var, &bindings);

  FOR_EACH_LOCAL_DECL (id->src_cfun, ix, var)
    reset_debug_binding (id, var, &bindings);

  gsi_insert_seq_before_without_update (&gsi, bindings, GSI_SAME_STMT);
}


/* Process deferred debug stmts.  In order to give values better odds
   of being successfully remapped, we delay the processing of debug
   stmts until all other stmts that might require remapping are
   processed.  */
static void copy_debug_stmts (copy_body_data *id)
{
   if (!id->debug_stmts.exists ())
      return;
   for (gdebug *stmt : id->debug_stmts)
      copy_debug_stmt (stmt, id);
   id->debug_stmts.release ();
}


/* Helper function for copy_cfg_body.  Move debug stmts from the end
   of NEW_BB to the beginning of successor basic blocks when needed.  If the
   successor has multiple predecessors, reset them, otherwise keep
   their value.  */

static void
maybe_move_debug_stmts_to_successors (copy_body_data *id, basic_block new_bb)
{
  edge e;
  edge_iterator ei;
  gimple_stmt_iterator si = gsi_last_nondebug_bb (new_bb);

  if (gsi_end_p (si)
      || gsi_one_before_end_p (si)
      || !(stmt_can_throw_internal (cfun, gsi_stmt (si))
      || stmt_can_make_abnormal_goto (gsi_stmt (si))))
    return;

  FOR_EACH_EDGE (e, ei, new_bb->succs)
    {
      gimple_stmt_iterator ssi = gsi_last_bb (new_bb);
      gimple_stmt_iterator dsi = gsi_after_labels (e->dest);
      while (is_gimple_debug (gsi_stmt (ssi)))
   {
     gimple *stmt = gsi_stmt (ssi);
     gdebug *new_stmt;
     tree var;
     tree value;

     /* For the last edge move the debug stmts instead of copying
        them.  */
     if (ei_one_before_end_p (ei))
       {
         si = ssi;
         gsi_prev (&ssi);
         if (!single_pred_p (e->dest) && gimple_debug_bind_p (stmt))
      {
        gimple_debug_bind_reset_value (stmt);
        gimple_set_location (stmt, UNKNOWN_LOCATION);
      }
         gsi_remove (&si, false);
         gsi_insert_before (&dsi, stmt, GSI_NEW_STMT);
         continue;
       }

     if (gimple_debug_bind_p (stmt))
       {
         var = gimple_debug_bind_get_var (stmt);
         if (single_pred_p (e->dest))
      {
        value = gimple_debug_bind_get_value (stmt);
        value = unshare_expr (value);
        new_stmt = gimple_build_debug_bind (var, value, stmt);
      }
         else
      new_stmt = gimple_build_debug_bind (var, NULL_TREE, NULL);
       }
     else if (gimple_debug_source_bind_p (stmt))
       {
         var = gimple_debug_source_bind_get_var (stmt);
         value = gimple_debug_source_bind_get_value (stmt);
         new_stmt = gimple_build_debug_source_bind (var, value, stmt);
       }
     else if (gimple_debug_nonbind_marker_p (stmt))
       new_stmt = as_a <gdebug *> (gimple_copy (stmt));
     else
       gcc_unreachable ();
     gsi_insert_before (&dsi, new_stmt, GSI_NEW_STMT);
     id->debug_stmts.safe_push (new_stmt);
     gsi_prev (&ssi);
   }
    }
}


/* Copy the PHIs.  All blocks and edges are copied, some blocks
   was possibly split and new outgoing EH edges inserted.
   BB points to the block of original function and AUX pointers links
   the original and newly copied blocks.  */

static void
copy_phis_for_bb (basic_block bb, copy_body_data *id)
{
  basic_block const new_bb = (basic_block) bb->aux;
  edge_iterator ei;
  gphi *phi;
  gphi_iterator si;
  edge new_edge;
  bool inserted = false;

  for (si = gsi_start_phis (bb); !gsi_end_p (si); gsi_next (&si))
    {
      tree res, new_res;
      gphi *new_phi;

      phi = si.phi ();
      res = PHI_RESULT (phi);
      new_res = res;
      if (!virtual_operand_p (res)
     && (!id->param_body_adjs
         || !id->param_body_adjs->m_dead_stmts.contains (phi)))
   {
     walk_tree (&new_res, copy_tree_body_r, id, NULL);
     if (EDGE_COUNT (new_bb->preds) == 0)
       {
         /* Technically we'd want a SSA_DEFAULT_DEF here... */
         SSA_NAME_DEF_STMT (new_res) = gimple_build_nop ();
       }
     else
       {
         new_phi = create_phi_node (new_res, new_bb);
         FOR_EACH_EDGE (new_edge, ei, new_bb->preds)
      {
        edge old_edge = find_edge ((basic_block) new_edge->src->aux,
                    bb);
        tree arg;
        tree new_arg;
        edge_iterator ei2;
        location_t locus;

        /* When doing partial cloning, we allow PHIs on the entry
           block as long as all the arguments are the same.
           Find any input edge to see argument to copy.  */
        if (!old_edge)
          FOR_EACH_EDGE (old_edge, ei2, bb->preds)
            if (!old_edge->src->aux)
         break;

        arg = PHI_ARG_DEF_FROM_EDGE (phi, old_edge);
        new_arg = arg;
        walk_tree (&new_arg, copy_tree_body_r, id, NULL);
        gcc_assert (new_arg);
        /* With return slot optimization we can end up with
           non-gimple (foo *)&this->m, fix that here.  */
        if (TREE_CODE (new_arg) != SSA_NAME
            && TREE_CODE (new_arg) != FUNCTION_DECL
            && !is_gimple_val (new_arg))
          {
            gimple_seq stmts = NULL;
            new_arg = force_gimple_operand (new_arg, &stmts, true,
                        NULL);
            gsi_insert_seq_on_edge (new_edge, stmts);
            inserted = true;
          }
        locus = gimple_phi_arg_location_from_edge (phi, old_edge);
        if (id->reset_location)
          locus = input_location;
        else
          locus = remap_location (locus, id);
        add_phi_arg (new_phi, new_arg, new_edge, locus);
      }
       }
   }
    }

  /* Commit the delayed edge insertions.  */
  if (inserted)
    FOR_EACH_EDGE (new_edge, ei, new_bb->preds)
      gsi_commit_one_edge_insert (new_edge, NULL);
}


/* Make a copy of the sub-loops of SRC_PARENT and place them
   as siblings of DEST_PARENT.  */

static void
copy_loops (copy_body_data *id,
       class loop *dest_parent, class loop *src_parent)
{
  class loop *src_loop = src_parent->inner;
  while (src_loop)
    {
      if (!id->blocks_to_copy
     || bitmap_bit_p (id->blocks_to_copy, src_loop->header->index))
   {
     class loop *dest_loop = alloc_loop ();

     /* Assign the new loop its header and latch and associate
        those with the new loop.  */
     dest_loop->header = (basic_block)src_loop->header->aux;
     dest_loop->header->loop_father = dest_loop;
     if (src_loop->latch != NULL)
       {
         dest_loop->latch = (basic_block)src_loop->latch->aux;
         dest_loop->latch->loop_father = dest_loop;
       }

     /* Copy loop meta-data.  */
     copy_loop_info (src_loop, dest_loop);
     if (dest_loop->unroll)
       cfun->has_unroll = true;
     if (dest_loop->force_vectorize)
       cfun->has_force_vectorize_loops = true;
     if (id->src_cfun->last_clique != 0)
       dest_loop->owned_clique
         = remap_dependence_clique (id,
                src_loop->owned_clique
                ? src_loop->owned_clique : 1);

     /* Finally place it into the loop array and the loop tree.  */
     place_new_loop (cfun, dest_loop);
     flow_loop_tree_node_add (dest_parent, dest_loop);

     if (src_loop->simduid)
       {
         dest_loop->simduid = remap_decl (src_loop->simduid, id);
         cfun->has_simduid_loops = true;
       }

     /* Recurse.  */
     copy_loops (id, dest_loop, src_loop);
   }
      src_loop = src_loop->next;
    }
}


/* Insert clobbers for automatic variables of inlined ID->src_fn
   function at the start of basic block ID->eh_landing_pad_dest.  */

static void
add_clobbers_to_eh_landing_pad (copy_body_data *id)
{
  tree var;
  basic_block bb = id->eh_landing_pad_dest;
  live_vars_map *vars = NULL;
  unsigned int cnt = 0;
  unsigned int i;
  FOR_EACH_VEC_SAFE_ELT (id->src_cfun->local_decls, i, var)
    if (VAR_P (var)
   && !DECL_HARD_REGISTER (var)
   && !TREE_THIS_VOLATILE (var)
   && !DECL_HAS_VALUE_EXPR_P (var)
   && !is_gimple_reg (var)
   && auto_var_in_fn_p (var, id->src_fn)
   && !lookup_attribute ("omp simd array", DECL_ATTRIBUTES (var)))
      {
   tree *t = id->decl_map->get (var);
   if (!t)
     continue;
   tree new_var = *t;
   if (VAR_P (new_var)
       && !DECL_HARD_REGISTER (new_var)
       && !TREE_THIS_VOLATILE (new_var)
       && !DECL_HAS_VALUE_EXPR_P (new_var)
       && !is_gimple_reg (new_var)
       && auto_var_in_fn_p (new_var, id->dst_fn))
     {
       if (vars == NULL)
         vars = new live_vars_map;
            vars->put (DECL_UID (var), cnt++);
     }
      }
  if (vars == NULL)
    return;

  vec<bitmap_head> live = compute_live_vars (id->src_cfun, vars);
  FOR_EACH_VEC_SAFE_ELT (id->src_cfun->local_decls, i, var)
    if (VAR_P (var))
      {
   edge e;
   edge_iterator ei;
   bool needed = false;
   unsigned int *v = vars->get (DECL_UID (var));
   if (v == NULL)
     continue;
   FOR_EACH_EDGE (e, ei, bb->preds)
     if ((e->flags & EDGE_EH) != 0
         && e->src->index >= id->add_clobbers_to_eh_landing_pads)
       {
         basic_block src_bb = (basic_block) e->src->aux;

         if (bitmap_bit_p (&live[src_bb->index], *v))
      {
        needed = true;
        break;
      }
       }
   if (needed)
     {
       tree new_var = *id->decl_map->get (var);
       gimple_stmt_iterator gsi = gsi_after_labels (bb);
       tree clobber = build_clobber (TREE_TYPE (new_var));
       gimple *clobber_stmt = gimple_build_assign (new_var, clobber);
       gsi_insert_before (&gsi, clobber_stmt, GSI_NEW_STMT);
     }
      }
  destroy_live_vars (live);
  delete vars;
}

/* Inserting Single Entry Multiple Exit region in SSA form into code in SSA
   form is quite easy, since dominator relationship for old basic blocks does
   not change.

   There is however exception where inlining might change dominator relation
   across EH edges from basic block within inlined functions destinating
   to landing pads in function we inline into.

   The function fills in PHI_RESULTs of such PHI nodes if they refer
   to gimple regs.  Otherwise, the function mark PHI_RESULT of such
   PHI nodes for renaming.  For non-gimple regs, renaming is safe: the
   EH edges are abnormal and SSA_NAME_OCCURS_IN_ABNORMAL_PHI must be
   set, and this means that there will be no overlapping live ranges
   for the underlying symbol.

   This might change in future if we allow redirecting of EH edges and
   we might want to change way build CFG pre-inlining to include
   all the possible edges then.  */
static void
update_ssa_across_abnormal_edges (basic_block bb, basic_block ret_bb,
              bool can_throw, bool nonlocal_goto)
{
  edge e;
  edge_iterator ei;

  FOR_EACH_EDGE (e, ei, bb->succs)
    if (!e->dest->aux
   || ((basic_block)e->dest->aux)->index == ENTRY_BLOCK)
      {
   gphi *phi;
   gphi_iterator si;

   if (!nonlocal_goto)
     gcc_assert (e->flags & EDGE_EH);

   if (!can_throw)
     gcc_assert (!(e->flags & EDGE_EH));

   for (si = gsi_start_phis (e->dest); !gsi_end_p (si); gsi_next (&si))
     {
       edge re;

       phi = si.phi ();

       /* For abnormal goto/call edges the receiver can be the
          ENTRY_BLOCK.  Do not assert this cannot happen.  */

       gcc_assert ((e->flags & EDGE_EH)
         || SSA_NAME_OCCURS_IN_ABNORMAL_PHI (PHI_RESULT (phi)));

       re = find_edge (ret_bb, e->dest);
       gcc_checking_assert (re);
       gcc_assert ((re->flags & (EDGE_EH | EDGE_ABNORMAL))
         == (e->flags & (EDGE_EH | EDGE_ABNORMAL)));

       SET_USE (PHI_ARG_DEF_PTR_FROM_EDGE (phi, e),
           USE_FROM_PTR (PHI_ARG_DEF_PTR_FROM_EDGE (phi, re)));
     }
      }
}


/* Remap the location info pointed to by LOCUS.  */

static location_t remap_location (location_t locus, copy_body_data *id)
{
  if (LOCATION_BLOCK (locus))
    {
      tree *n = id->decl_map->get (LOCATION_BLOCK (locus));
      gcc_assert (n);
      if (*n)
   return set_block (locus, *n);
    }

  locus = LOCATION_LOCUS (locus);

  if (locus != UNKNOWN_LOCATION && id->block)
    return set_block (locus, id->block);

  return locus;
}

/* Copy edges from BB into its copy constructed earlier, scale profile
   accordingly.  Edges will be taken care of later.  Assume aux
   pointers to point to the copies of each BB.  Return true if any
   debug stmts are left after a statement that must end the basic block.  */

static bool
copy_edges_for_bb (basic_block bb, profile_count num, profile_count den,
         basic_block ret_bb, basic_block abnormal_goto_dest,
         copy_body_data *id)
{
  basic_block new_bb = (basic_block) bb->aux;
  edge_iterator ei;
  edge old_edge;
  gimple_stmt_iterator si;
  bool need_debug_cleanup = false;

  /* Use the indices from the original blocks to create edges for the
     new ones.  */
  FOR_EACH_EDGE (old_edge, ei, bb->succs)
    if (!(old_edge->flags & EDGE_EH))
      {
   edge new_edge;
   int flags = old_edge->flags;
   location_t locus = old_edge->goto_locus;

   /* Return edges do get a FALLTHRU flag when they get inlined.  */
   if (old_edge->dest->index == EXIT_BLOCK
       && !(flags & (EDGE_TRUE_VALUE|EDGE_FALSE_VALUE|EDGE_FAKE))
       && old_edge->dest->aux != EXIT_BLOCK_PTR_FOR_FN (cfun))
     flags |= EDGE_FALLTHRU;

   new_edge
     = make_edge (new_bb, (basic_block) old_edge->dest->aux, flags);
   new_edge->probability = old_edge->probability;
   if (!id->reset_location)
     new_edge->goto_locus = remap_location (locus, id);
      }

  if (bb->index == ENTRY_BLOCK || bb->index == EXIT_BLOCK)
    return false;

  /* When doing function splitting, we must decrease count of the return block
     which was previously reachable by block we did not copy.  */
  if (single_succ_p (bb) && single_succ_edge (bb)->dest->index == EXIT_BLOCK)
    FOR_EACH_EDGE (old_edge, ei, bb->preds)
      if (old_edge->src->index != ENTRY_BLOCK
     && !old_edge->src->aux)
   new_bb->count -= old_edge->count ().apply_scale (num, den);

  /* Walk stmts from end to start so that splitting will adjust the BB
     pointer for each stmt at most once, even when we split the block
     multiple times.  */
  bool seen_nondebug = false;
  for (si = gsi_last_bb (new_bb); !gsi_end_p (si);)
    {
      bool can_throw, nonlocal_goto;
      gimple *copy_stmt = gsi_stmt (si);

      /* Do this before the possible split_block.  */
      gsi_prev (&si);

      /* If this tree could throw an exception, there are two
         cases where we need to add abnormal edge(s): the
         tree wasn't in a region and there is a "current
         region" in the caller; or the original tree had
         EH edges.  In both cases split the block after the tree,
         and add abnormal edge(s) as needed; we need both
         those from the callee and the caller.
         We check whether the copy can throw, because the const
         propagation can change an INDIRECT_REF which throws
         into a COMPONENT_REF which doesn't.  If the copy
         can throw, the original could also throw.  */
      can_throw = stmt_can_throw_internal (cfun, copy_stmt);
      nonlocal_goto
   = (stmt_can_make_abnormal_goto (copy_stmt)
      && !computed_goto_p (copy_stmt));

      if (can_throw || nonlocal_goto)
   {
     /* If there's only debug insns after copy_stmt don't split
        the block but instead mark the block for cleanup.  */
     if (!seen_nondebug)
       need_debug_cleanup = true;
     else
       {
         /* Note that bb's predecessor edges aren't necessarily
       right at this point; split_block doesn't care.  */
         edge e = split_block (new_bb, copy_stmt);
         e->dest->aux = new_bb->aux;
         seen_nondebug = false;
       }
   }

      if (!is_gimple_debug (copy_stmt))
   seen_nondebug = true;

      bool update_probs = false;

      if (gimple_code (copy_stmt) == GIMPLE_EH_DISPATCH)
   {
     make_eh_dispatch_edges (as_a <geh_dispatch *> (copy_stmt));
     update_probs = true;
   }
      else if (can_throw)
   {
     make_eh_edge (copy_stmt);
     update_probs = true;
   }

      /* EH edges may not match old edges.  Copy as much as possible.  */
      if (update_probs)
   {
          edge e;
          edge_iterator ei;
     basic_block copy_stmt_bb = gimple_bb (copy_stmt);

          FOR_EACH_EDGE (old_edge, ei, bb->succs)
            if ((old_edge->flags & EDGE_EH)
      && (e = find_edge (copy_stmt_bb,
               (basic_block) old_edge->dest->aux))
      && (e->flags & EDGE_EH))
         e->probability = old_edge->probability;

          FOR_EACH_EDGE (e, ei, copy_stmt_bb->succs)
       if (e->flags & EDGE_EH)
         {
      if (!e->probability.initialized_p ())
        e->probability = profile_probability::never ();
      if (e->dest->index < id->add_clobbers_to_eh_landing_pads)
        {
          if (id->eh_landing_pad_dest == NULL)
            id->eh_landing_pad_dest = e->dest;
          else
            gcc_assert (id->eh_landing_pad_dest == e->dest);
        }
         }
        }


      /* If the call we inline cannot make abnormal goto do not add
         additional abnormal edges but only retain those already present
    in the original function body.  */
      if (abnormal_goto_dest == NULL)
   nonlocal_goto = false;
      if (nonlocal_goto)
   {
     basic_block copy_stmt_bb = gimple_bb (copy_stmt);

     if (get_abnormal_succ_dispatcher (copy_stmt_bb))
       nonlocal_goto = false;
     /* ABNORMAL_DISPATCHER (1) is for longjmp/setjmp or nonlocal gotos
        in OpenMP regions which aren't allowed to be left abnormally.
        So, no need to add abnormal edge in that case.  */
     else if (is_gimple_call (copy_stmt)
         && gimple_call_internal_p (copy_stmt)
         && (gimple_call_internal_fn (copy_stmt)
             == IFN_ABNORMAL_DISPATCHER)
         && gimple_call_arg (copy_stmt, 0) == boolean_true_node)
       nonlocal_goto = false;
     else
       {
         make_single_succ_edge (copy_stmt_bb, abnormal_goto_dest,
                 EDGE_ABNORMAL);
         gimple_call_set_ctrl_altering (copy_stmt, true);
       }
   }

      if ((can_throw || nonlocal_goto)
     && gimple_in_ssa_p (cfun))
   update_ssa_across_abnormal_edges (gimple_bb (copy_stmt), ret_bb,
                 can_throw, nonlocal_goto);
    }
  return need_debug_cleanup;
}

/* Copy basic block, scale profile accordingly.  Edges will be taken care of
   later  */
static basic_block copy_bb (copy_body_data *id, basic_block bb,
         profile_count num, profile_count den)
{
  gimple_stmt_iterator gsi, copy_gsi, seq_gsi;
  basic_block copy_basic_block;
  tree decl;
  basic_block prev;

  profile_count::adjust_for_ipa_scaling (&num, &den);

  /* Search for previous copied basic block.  */
  prev = bb->prev_bb;
  while (!prev->aux)
    prev = prev->prev_bb;

  /* create_basic_block() will append every new block to
     basic_block_info automatically.  */
  copy_basic_block = create_basic_block (NULL, (basic_block) prev->aux);
  copy_basic_block->count = bb->count.apply_scale (num, den);

  copy_gsi = gsi_start_bb (copy_basic_block);

  unsigned min_cond_uid = 0;
  if (id->src_cfun->cond_uids)
    {
      if (!cfun->cond_uids)
   cfun->cond_uids = new hash_map <gcond*, unsigned> ();

      for (auto itr : *id->src_cfun->cond_uids)
   if (itr.second >= min_cond_uid)
     min_cond_uid = itr.second + 1;
    }

  for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
    {
      gimple_seq stmts;
      gimple *stmt = gsi_stmt (gsi);
      gimple *orig_stmt = stmt;
      gimple_stmt_iterator stmts_gsi;
      bool stmt_added = false;

      id->regimplify = false;
      stmts = remap_gimple_stmt (stmt, id);

      if (gimple_seq_empty_p (stmts))
   continue;

      seq_gsi = copy_gsi;

      for (stmts_gsi = gsi_start (stmts);
      !gsi_end_p (stmts_gsi); )
   {
     stmt = gsi_stmt (stmts_gsi);

     /* Advance iterator now before stmt is moved to seq_gsi.  */
     gsi_next (&stmts_gsi);

     if (gimple_nop_p (stmt))
         continue;

     /* If -fcondition-coverage is used, register the inlined conditions
        in the cond->expression mapping of the caller.  The expression tag
        is shifted conditions from the two bodies are not mixed.  */
     if (id->src_cfun->cond_uids && is_a <gcond*> (stmt))
       {
         gcond *orig_cond = as_a <gcond*> (orig_stmt);
         gcond *cond = as_a <gcond*> (stmt);
         unsigned *v = id->src_cfun->cond_uids->get (orig_cond);
         if (v)
      cfun->cond_uids->put (cond, *v + min_cond_uid);
       }

     gimple_duplicate_stmt_histograms (cfun, stmt, id->src_cfun,
                   orig_stmt);

     gsi_insert_after (&seq_gsi, stmt, GSI_NEW_STMT);

     if (id->regimplify)
       gimple_regimplify_operands (stmt, &seq_gsi);

     stmt_added = true;
   }

      if (!stmt_added)
   continue;

      /* If copy_basic_block has been empty at the start of this iteration,
    call gsi_start_bb again to get at the newly added statements.  */
      if (gsi_end_p (copy_gsi))
   copy_gsi = gsi_start_bb (copy_basic_block);
      else
   gsi_next (&copy_gsi);

      /* Process the new statement.  The call to gimple_regimplify_operands
    possibly turned the statement into multiple statements, we
    need to process all of them.  */
      do
   {
     tree fn;
     gcall *call_stmt;

     stmt = gsi_stmt (copy_gsi);
     call_stmt = dyn_cast <gcall *> (stmt);
     if (call_stmt
         && gimple_call_va_arg_pack_p (call_stmt)
         && id->call_stmt
         && ! gimple_call_va_arg_pack_p (id->call_stmt))
       {
         /* __builtin_va_arg_pack () should be replaced by
       all arguments corresponding to ... in the caller.  */
         tree p;
         gcall *new_call;
         vec<tree> argarray;
         size_t nargs_caller = gimple_call_num_args (id->call_stmt);
         size_t nargs = nargs_caller;

         for (p = DECL_ARGUMENTS (id->src_fn); p; p = DECL_CHAIN (p))
      {
        /* Avoid crashing on invalid IL that doesn't have a
           varargs function or that passes not enough arguments.  */
        if (nargs == 0)
          break;
        nargs--;
      }

         /* Create the new array of arguments.  */
         size_t nargs_callee = gimple_call_num_args (call_stmt);
         size_t n = nargs + nargs_callee;
         argarray.create (n);
         argarray.safe_grow_cleared (n, true);

         /* Copy all the arguments before '...'  */
         if (nargs_callee)
      memcpy (argarray.address (),
         gimple_call_arg_ptr (call_stmt, 0),
         nargs_callee * sizeof (tree));

         /* Append the arguments passed in '...'  */
         if (nargs)
      memcpy (argarray.address () + nargs_callee,
         gimple_call_arg_ptr (id->call_stmt, 0)
         + (nargs_caller - nargs), nargs * sizeof (tree));

         new_call = gimple_build_call_vec (gimple_call_fn (call_stmt),
                  argarray);

         argarray.release ();

         /* Copy all GIMPLE_CALL flags, location and block, except
       GF_CALL_VA_ARG_PACK.  */
         gimple_call_copy_flags (new_call, call_stmt);
         gimple_call_set_va_arg_pack (new_call, false);
         gimple_call_set_fntype (new_call, gimple_call_fntype (call_stmt));
         /* location includes block.  */
         gimple_set_location (new_call, gimple_location (stmt));
         gimple_call_set_lhs (new_call, gimple_call_lhs (call_stmt));

         gsi_replace (&copy_gsi, new_call, false);
         stmt = new_call;
       }
     else if (call_stmt
         && id->call_stmt
         && (decl = gimple_call_fndecl (stmt))
         && fndecl_built_in_p (decl, BUILT_IN_VA_ARG_PACK_LEN))
       {
         /* __builtin_va_arg_pack_len () should be replaced by
       the number of anonymous arguments.  */
         size_t nargs = gimple_call_num_args (id->call_stmt);
         tree count, p;
         gimple *new_stmt;

         for (p = DECL_ARGUMENTS (id->src_fn); p; p = DECL_CHAIN (p))
      nargs--;

         if (!gimple_call_lhs (stmt))
      {
        /* Drop unused calls.  */
        gsi_remove (&copy_gsi, false);
        continue;
      }
         else if (!gimple_call_va_arg_pack_p (id->call_stmt))
      {
        count = build_int_cst (integer_type_node, nargs);
        new_stmt = gimple_build_assign (gimple_call_lhs (stmt), count);
        gsi_replace (&copy_gsi, new_stmt, false);
        stmt = new_stmt;
      }
         else if (nargs != 0)
      {
        tree newlhs = create_tmp_reg_or_ssa_name (integer_type_node);
        count = build_int_cst (integer_type_node, nargs);
        new_stmt = gimple_build_assign (gimple_call_lhs (stmt),
                    PLUS_EXPR, newlhs, count);
        gimple_call_set_lhs (stmt, newlhs);
        gsi_insert_after (&copy_gsi, new_stmt, GSI_NEW_STMT);
      }
       }
     else if (call_stmt
         && id->call_stmt
         && gimple_call_internal_p (stmt))
       switch (gimple_call_internal_fn (stmt))
         {
         case IFN_TSAN_FUNC_EXIT:
      /* Drop .TSAN_FUNC_EXIT () internal calls during inlining.  */
      gsi_remove (&copy_gsi, false);
      continue;
         case IFN_ASAN_MARK:
      /* Drop .ASAN_MARK internal calls during inlining into
         no_sanitize functions.  */
      if (!sanitize_flags_p (SANITIZE_ADDRESS, id->dst_fn)
          && !sanitize_flags_p (SANITIZE_HWADDRESS, id->dst_fn))
        {
          gsi_remove (&copy_gsi, false);
          continue;
        }
      break;
         default:
      break;
         }

     /* Statements produced by inlining can be unfolded, especially
        when we constant propagated some operands.  We can't fold
        them right now for two reasons:
        1) folding require SSA_NAME_DEF_STMTs to be correct
        2) we can't change function calls to builtins.
        So we just mark statement for later folding.  We mark
        all new statements, instead just statements that has changed
        by some nontrivial substitution so even statements made
        foldable indirectly are updated.  If this turns out to be
        expensive, copy_body can be told to watch for nontrivial
        changes.  */
     if (id->statements_to_fold)
       id->statements_to_fold->add (stmt);

     /* We're duplicating a CALL_EXPR.  Find any corresponding
        callgraph edges and update or duplicate them.  */
     if (gcall *call_stmt = dyn_cast <gcall *> (stmt))
       {
         struct cgraph_edge *edge;

         switch (id->transform_call_graph_edges)
      {
      case CB_CGE_DUPLICATE:
        edge = id->src_node->get_edge (orig_stmt);
        if (edge)
          {
            struct cgraph_edge *old_edge = edge;

            /* A speculative call is consist of multiple
          edges - indirect edge and one or more direct edges
          Duplicate the whole thing and distribute frequencies
          accordingly.  */
            if (edge->speculative)
         {
           int n = 0;
           profile_count direct_cnt
             = profile_count::zero ();

           /* First figure out the distribution of counts
              so we can re-scale BB profile accordingly.  */
           for (cgraph_edge *e = old_edge; e;
                e = e->next_speculative_call_target ())
             direct_cnt = direct_cnt + e->count;

           cgraph_edge *indirect
             = old_edge->speculative_call_indirect_edge ();
           profile_count indir_cnt = indirect->count;

           /* Next iterate all direct edges, clone it and its
              corresponding reference and update profile.  */
           for (cgraph_edge *e = old_edge;
                e;
                e = e->next_speculative_call_target ())
             {
               profile_count cnt = e->count;

               id->dst_node->clone_reference
             (e->speculative_call_target_ref (), stmt);
               edge = e->clone (id->dst_node, call_stmt,
                      gimple_uid (stmt), num, den,
                      true);
               profile_probability prob
             = cnt.probability_in (direct_cnt
                         + indir_cnt);
               edge->count
             = copy_basic_block->count.apply_probability
                (prob);
               n++;
             }
           gcc_checking_assert
             (indirect->num_speculative_call_targets_p ()
              == n);

           /* Duplicate the indirect edge after all direct edges
              cloned.  */
           indirect = indirect->clone (id->dst_node, call_stmt,
                        gimple_uid (stmt),
                        num, den,
                        true);

           profile_probability prob
              = indir_cnt.probability_in (direct_cnt
                      + indir_cnt);
           indirect->count
              = copy_basic_block->count.apply_probability (prob);
         }
            else
         {
           edge = edge->clone (id->dst_node, call_stmt,
                     gimple_uid (stmt),
                     num, den,
                     true);
           edge->count = copy_basic_block->count;
         }
          }
        break;

      case CB_CGE_MOVE_CLONES:
        id->dst_node->set_call_stmt_including_clones (orig_stmt,
                        call_stmt);
        edge = id->dst_node->get_edge (stmt);
        break;

      case CB_CGE_MOVE:
        edge = id->dst_node->get_edge (orig_stmt);
        if (edge)
          edge = cgraph_edge::set_call_stmt (edge, call_stmt);
        break;

      default:
        gcc_unreachable ();
      }

         /* Constant propagation on argument done during inlining
       may create new direct call.  Produce an edge for it.  */
         // n_debug("tree-inline.cc copy_bb 00 edge:%p %d id->dst_node->definition:%d %d\n",edge,
               // id->transform_call_graph_edges == CB_CGE_MOVE_CLONES,
              //  id->dst_node->definition,
              //  gimple_call_fndecl (stmt) != NULL);
         // if(edge)
           //  n_debug("tree-inline.cc copy_11 edge->indirect_inlining_edge:%d\n",edge->indirect_inlining_edge);

         if ((!edge
         || (edge->indirect_inlining_edge
             && id->transform_call_graph_edges == CB_CGE_MOVE_CLONES))
        && id->dst_node->definition
        && (fn = gimple_call_fndecl (stmt)) != NULL)
      {
        struct cgraph_node *dest = cgraph_node::get_create (fn);
        //n_debug("tree-inline.cc copy_bb 22 %p definition:%d %d\n",dest,dest->definition,dest->address_taken);
       // aet_print_gimple(stmt);
       // aet_print_tree(fn);
       // if(id && id->src_node){
        //    n_debug("tree-inline.cc copy_bb 33 src_node:%p %s %d dest_node:%p %s %d \n",
                 //id->src_node, id->src_node->name(),id->src_node->definition,id->dst_node,id->dst_node->name(),id->dst_node->definition);
        //}

        /* We have missing edge in the callgraph.  This can happen
           when previous inlining turned an indirect call into a
           direct call by constant propagating arguments or we are
           producing dead clone (for further cloning).  In all
           other cases we hit a bug (incorrect node sharing is the
           most common reason for missing edges).  */
        gcc_assert (!dest->definition
               || dest->address_taken
               || !id->src_node->definition
               || !id->dst_node->definition);
        if (id->transform_call_graph_edges == CB_CGE_MOVE_CLONES)
          id->dst_node->create_edge_including_clones
            (dest, orig_stmt, call_stmt, bb->count,
             CIF_ORIGINALLY_INDIRECT_CALL);
        else
          id->dst_node->create_edge (dest, call_stmt,
               bb->count)->inline_failed
            = CIF_ORIGINALLY_INDIRECT_CALL;
        if (dump_file)
          {
            fprintf (dump_file, "Created new direct edge to %s\n",
                dest->dump_name ());
          }
      }

         notice_special_calls (as_a <gcall *> (stmt));
       }

     maybe_duplicate_eh_stmt_fn (cfun, stmt, id->src_cfun, orig_stmt,
                  id->eh_map, id->eh_lp_nr);

     gsi_next (&copy_gsi);
   }
      while (!gsi_end_p (copy_gsi));

      copy_gsi = gsi_last_bb (copy_basic_block);
    }

  return copy_basic_block;
}

/* Wrapper for remap_decl so it can be used as a callback.  */

static tree
remap_decl_1 (tree decl, void *data)
{
  return remap_decl (decl, (copy_body_data *) data);
}


/* Make a copy of the body of FN so that it can be inserted inline in
   another function.  Walks FN via CFG, returns new fndecl.  */

static tree
copy_cfg_body (copy_body_data * id,
          basic_block entry_block_map, basic_block exit_block_map,
          basic_block new_entry)
{
  tree callee_fndecl = id->src_fn;
  /* Original cfun for the callee, doesn't change.  */
  struct function *src_cfun = DECL_STRUCT_FUNCTION (callee_fndecl);
  struct function *cfun_to_copy;
  basic_block bb;
  tree new_fndecl = NULL;
  bool need_debug_cleanup = false;
  int last;
  profile_count den = ENTRY_BLOCK_PTR_FOR_FN (src_cfun)->count;
  profile_count num = entry_block_map->count;

  cfun_to_copy = id->src_cfun = DECL_STRUCT_FUNCTION (callee_fndecl);

  /* Register specific tree functions.  */
  gimple_register_cfg_hooks ();

  /* If we are inlining just region of the function, make sure to connect
     new entry to ENTRY_BLOCK_PTR_FOR_FN (cfun).  Since new entry can be
     part of loop, we must compute frequency and probability of
     ENTRY_BLOCK_PTR_FOR_FN (cfun) based on the frequencies and
     probabilities of edges incoming from nonduplicated region.  */
  if (new_entry)
    {
      edge e;
      edge_iterator ei;
      den = profile_count::zero ();

      FOR_EACH_EDGE (e, ei, new_entry->preds)
   if (!e->src->aux)
     den += e->count ();
      ENTRY_BLOCK_PTR_FOR_FN (cfun)->count = den;
    }

  profile_count::adjust_for_ipa_scaling (&num, &den);

  /* Must have a CFG here at this point.  */
  gcc_assert (ENTRY_BLOCK_PTR_FOR_FN
         (DECL_STRUCT_FUNCTION (callee_fndecl)));


  ENTRY_BLOCK_PTR_FOR_FN (cfun_to_copy)->aux = entry_block_map;
  EXIT_BLOCK_PTR_FOR_FN (cfun_to_copy)->aux = exit_block_map;
  entry_block_map->aux = ENTRY_BLOCK_PTR_FOR_FN (cfun_to_copy);
  exit_block_map->aux = EXIT_BLOCK_PTR_FOR_FN (cfun_to_copy);

  /* Duplicate any exception-handling regions.  */
  if (cfun->eh)
    id->eh_map = duplicate_eh_regions (cfun_to_copy, NULL, id->eh_lp_nr,
                   remap_decl_1, id);

  /* Use aux pointers to map the original blocks to copy.  */
  FOR_EACH_BB_FN (bb, cfun_to_copy)
    if (!id->blocks_to_copy || bitmap_bit_p (id->blocks_to_copy, bb->index))
      {
   basic_block new_bb = copy_bb (id, bb, num, den);
   bb->aux = new_bb;
   new_bb->aux = bb;
   new_bb->loop_father = entry_block_map->loop_father;
      }

  last = last_basic_block_for_fn (cfun);

  /* Now that we've duplicated the blocks, duplicate their edges.  */
  basic_block abnormal_goto_dest = NULL;
  if (id->call_stmt
      && stmt_can_make_abnormal_goto (id->call_stmt))
    {
      gimple_stmt_iterator gsi = gsi_for_stmt (id->call_stmt);

      bb = gimple_bb (id->call_stmt);
      gsi_next (&gsi);
      if (gsi_end_p (gsi))
   abnormal_goto_dest = get_abnormal_succ_dispatcher (bb);
    }
  FOR_ALL_BB_FN (bb, cfun_to_copy)
    if (!id->blocks_to_copy
   || (bb->index > 0 && bitmap_bit_p (id->blocks_to_copy, bb->index)))
      need_debug_cleanup |= copy_edges_for_bb (bb, num, den, exit_block_map,
                      abnormal_goto_dest, id);

  if (id->eh_landing_pad_dest)
    {
      add_clobbers_to_eh_landing_pad (id);
      id->eh_landing_pad_dest = NULL;
    }

  if (new_entry)
    {
      edge e = make_edge (entry_block_map, (basic_block)new_entry->aux,
           EDGE_FALLTHRU);
      e->probability = profile_probability::always ();
    }

  /* Duplicate the loop tree, if available and wanted.  */
  if (loops_for_fn (src_cfun) != NULL
      && current_loops != NULL)
    {
      copy_loops (id, entry_block_map->loop_father,
        get_loop (src_cfun, 0));
      /* Defer to cfgcleanup to update loop-father fields of basic-blocks.  */
      loops_state_set (LOOPS_NEED_FIXUP);
    }

  /* If the loop tree in the source function needed fixup, mark the
     destination loop tree for fixup, too.  */
  if (loops_for_fn (src_cfun)->state & LOOPS_NEED_FIXUP)
    loops_state_set (LOOPS_NEED_FIXUP);

  if (gimple_in_ssa_p (cfun))
    FOR_ALL_BB_FN (bb, cfun_to_copy)
      if (!id->blocks_to_copy
     || (bb->index > 0 && bitmap_bit_p (id->blocks_to_copy, bb->index)))
   copy_phis_for_bb (bb, id);

  FOR_ALL_BB_FN (bb, cfun_to_copy)
    if (bb->aux)
      {
   if (need_debug_cleanup
       && bb->index != ENTRY_BLOCK
       && bb->index != EXIT_BLOCK)
     maybe_move_debug_stmts_to_successors (id, (basic_block) bb->aux);
   /* Update call edge destinations.  This cannot be done before loop
      info is updated, because we may split basic blocks.  */
   if (id->transform_call_graph_edges == CB_CGE_DUPLICATE
       && bb->index != ENTRY_BLOCK
       && bb->index != EXIT_BLOCK)
     redirect_all_calls_new (id, (basic_block)bb->aux);
   ((basic_block)bb->aux)->aux = NULL;
   bb->aux = NULL;
      }

  /* Zero out AUX fields of newly created block during EH edge
     insertion. */
  for (; last < last_basic_block_for_fn (cfun); last++)
    {
      if (need_debug_cleanup)
   maybe_move_debug_stmts_to_successors (id,
                     BASIC_BLOCK_FOR_FN (cfun, last));
      BASIC_BLOCK_FOR_FN (cfun, last)->aux = NULL;
      /* Update call edge destinations.  This cannot be done before loop
    info is updated, because we may split basic blocks.  */
      if (id->transform_call_graph_edges == CB_CGE_DUPLICATE)
   redirect_all_calls_new (id, BASIC_BLOCK_FOR_FN (cfun, last));
    }
  entry_block_map->aux = NULL;
  exit_block_map->aux = NULL;

  if (id->eh_map)
    {
      delete id->eh_map;
      id->eh_map = NULL;
    }
  if (id->dependence_map)
    {
      delete id->dependence_map;
      id->dependence_map = NULL;
    }

  return new_fndecl;
}

/* Make a copy of the body of FN so that it can be inserted inline in
   another function.  */

static tree  copy_body (copy_body_data *id,
      basic_block entry_block_map, basic_block exit_block_map,
      basic_block new_entry)
{
   tree fndecl = id->src_fn;
   tree body;

   /* If this body has a CFG, walk CFG and copy.  */
   gcc_assert (ENTRY_BLOCK_PTR_FOR_FN (DECL_STRUCT_FUNCTION (fndecl)));
   body = copy_cfg_body (id, entry_block_map, exit_block_map,new_entry);
   copy_debug_stmts (id);
   if (id->killed_new_ssa_names){
      ipa_release_ssas_in_hash (id->killed_new_ssa_names);
      delete id->killed_new_ssa_names;
      id->killed_new_ssa_names = NULL;
   }

   return body;
}

/* Add local variables from CALLEE to CALLER.  */
static inline void add_local_variables (struct function *callee, struct function *caller,
           copy_body_data *id)
{
   tree var;
   unsigned ix;

   FOR_EACH_LOCAL_DECL (callee, ix, var)
      if (!can_be_nonlocal (var, id)){
         tree new_var = remap_decl (var, id);

         /* Remap debug-expressions.  */
         if (VAR_P (new_var) && DECL_HAS_DEBUG_EXPR_P (var) && new_var != var){
            tree tem = DECL_DEBUG_EXPR (var);
            bool old_regimplify = id->regimplify;
            id->remapping_type_depth++;
            walk_tree (&tem, copy_tree_body_r, id, NULL);
            id->remapping_type_depth--;
            id->regimplify = old_regimplify;
            SET_DECL_DEBUG_EXPR (new_var, tem);
            DECL_HAS_DEBUG_EXPR_P (new_var) = 1;
         }
         add_local_decl (caller, new_var);
      }
}
/* Append to BB a debug annotation that binds VAR to VALUE, inheriting
   lexical block and line number information from base_stmt, if given,
   or from the last stmt of the block otherwise.  */

static gimple *
insert_init_debug_bind (copy_body_data *id,
         basic_block bb, tree var, tree value,
         gimple *base_stmt)
{
  gimple *note;
  gimple_stmt_iterator gsi;
  tree tracked_var;

  if (!gimple_in_ssa_p (id->src_cfun))
    return NULL;

  if (!opt_for_fn (id->dst_fn, flag_var_tracking_assignments))
    return NULL;

  tracked_var = target_for_debug_bind (var);
  if (!tracked_var)
    return NULL;

  if (bb)
    {
      gsi = gsi_last_bb (bb);
      if (!base_stmt && !gsi_end_p (gsi))
   base_stmt = gsi_stmt (gsi);
    }

  note = gimple_build_debug_bind (tracked_var,
              value == error_mark_node
              ? NULL_TREE : unshare_expr (value),
              base_stmt);

  if (bb)
    {
      if (!gsi_end_p (gsi))
   gsi_insert_after (&gsi, note, GSI_SAME_STMT);
      else
   gsi_insert_before (&gsi, note, GSI_SAME_STMT);
    }

  return note;
}


static void
insert_init_stmt (copy_body_data *id, basic_block bb, gimple *init_stmt)
{
  /* If VAR represents a zero-sized variable, it's possible that the
     assignment statement may result in no gimple statements.  */
  if (init_stmt)
    {
      gimple_stmt_iterator si = gsi_last_bb (bb);

      /* We can end up with init statements that store to a non-register
         from a rhs with a conversion.  Handle that here by forcing the
    rhs into a temporary.  gimple_regimplify_operands is not
    prepared to do this for us.  */
      if (!is_gimple_debug (init_stmt)
     && !is_gimple_reg (gimple_assign_lhs (init_stmt))
     && is_gimple_reg_type (TREE_TYPE (gimple_assign_lhs (init_stmt)))
     && gimple_assign_rhs_class (init_stmt) == GIMPLE_UNARY_RHS)
   {
     tree rhs = build1 (gimple_assign_rhs_code (init_stmt),
              TREE_TYPE (gimple_assign_lhs (init_stmt)),
              gimple_assign_rhs1 (init_stmt));
     rhs = force_gimple_operand_gsi (&si, rhs, true, NULL_TREE, false,
                 GSI_NEW_STMT);
     gimple_assign_set_rhs_code (init_stmt, TREE_CODE (rhs));
     gimple_assign_set_rhs1 (init_stmt, rhs);
   }
      gsi_insert_after (&si, init_stmt, GSI_NEW_STMT);
      if (!is_gimple_debug (init_stmt))
   {
     gimple_regimplify_operands (init_stmt, &si);

     tree def = gimple_assign_lhs (init_stmt);
     insert_init_debug_bind (id, bb, def, def, init_stmt);
   }
    }
}


/* Like copy_decl_to_var, but create a return slot object instead of a
   pointer variable for return by invisible reference.  */

static tree
copy_result_decl_to_var (tree decl, copy_body_data *id)
{
  tree copy, type;

  gcc_assert (TREE_CODE (decl) == PARM_DECL
         || TREE_CODE (decl) == RESULT_DECL);

  type = TREE_TYPE (decl);
  if (DECL_BY_REFERENCE (decl))
    type = TREE_TYPE (type);

  copy = build_decl (DECL_SOURCE_LOCATION (id->dst_fn),
           VAR_DECL, DECL_NAME (decl), type);
  if (DECL_PT_UID_SET_P (decl))
    SET_DECL_PT_UID (copy, DECL_PT_UID (decl));
  TREE_READONLY (copy) = TREE_READONLY (decl);
  TREE_THIS_VOLATILE (copy) = TREE_THIS_VOLATILE (decl);
  if (!DECL_BY_REFERENCE (decl))
    {
      TREE_ADDRESSABLE (copy) = TREE_ADDRESSABLE (decl);
      DECL_NOT_GIMPLE_REG_P (copy)
   = (DECL_NOT_GIMPLE_REG_P (decl)
      /* RESULT_DECLs are treated special by needs_to_live_in_memory,
         mirror that to the created VAR_DECL.  */
      || (TREE_CODE (decl) == RESULT_DECL
          && aggregate_value_p (decl, id->src_fn)));
    }

  return copy_decl_for_dup_finish (id, decl, copy);
}

/* Declare a return variable to replace the RESULT_DECL for the
   function we are calling.  An appropriate DECL_STMT is returned.
   The USE_STMT is filled to contain a use of the declaration to
   indicate the return value of the function.

   RETURN_SLOT, if non-null is place where to store the result.  It
   is set only for CALL_EXPR_RETURN_SLOT_OPT.  MODIFY_DEST, if non-null,
   was the LHS of the MODIFY_EXPR to which this call is the RHS.

   The return value is a (possibly null) value that holds the result
   as seen by the caller.  */

static tree
declare_return_variable (copy_body_data *id, tree return_slot, tree modify_dest,
          basic_block entry_bb)
{
  tree callee = id->src_fn;
  tree result = DECL_RESULT (callee);
  tree callee_type = TREE_TYPE (result);
  tree caller_type;
  tree var, use;

  /* Handle type-mismatches in the function declaration return type
     vs. the call expression.  */
  if (modify_dest)
    caller_type = TREE_TYPE (modify_dest);
  else if (return_slot)
    caller_type = TREE_TYPE (return_slot);
  else /* No LHS on the call.  */
    caller_type = TREE_TYPE (TREE_TYPE (callee));

  /* We don't need to do anything for functions that don't return anything.  */
  if (VOID_TYPE_P (callee_type))
    return NULL_TREE;

  /* If there was a return slot, then the return value is the
     dereferenced address of that object.  */
  if (return_slot)
    {
      /* The front end shouldn't have used both return_slot and
    a modify expression.  */
      gcc_assert (!modify_dest);
      if (DECL_BY_REFERENCE (result))
   {
     tree return_slot_addr = build_fold_addr_expr (return_slot);
     STRIP_USELESS_TYPE_CONVERSION (return_slot_addr);

     /* We are going to construct *&return_slot and we can't do that
        for variables believed to be not addressable.

        FIXME: This check possibly can match, because values returned
        via return slot optimization are not believed to have address
        taken by alias analysis.  */
     gcc_assert (TREE_CODE (return_slot) != SSA_NAME);
     var = return_slot_addr;
     mark_addressable (return_slot);
   }
      else
   {
     var = return_slot;
     gcc_assert (TREE_CODE (var) != SSA_NAME);
     if (TREE_ADDRESSABLE (result))
       mark_addressable (var);
   }
      if (DECL_NOT_GIMPLE_REG_P (result)
     && DECL_P (var))
   DECL_NOT_GIMPLE_REG_P (var) = 1;

      if (!useless_type_conversion_p (callee_type, caller_type))
   var = build1 (VIEW_CONVERT_EXPR, callee_type, var);

      use = NULL;
      goto done;
    }

  /* All types requiring non-trivial constructors should have been handled.  */
  gcc_assert (!TREE_ADDRESSABLE (callee_type));

  /* Attempt to avoid creating a new temporary variable.  */
  if (modify_dest
      && TREE_CODE (modify_dest) != SSA_NAME)
    {
      bool use_it = false;

      /* We can't use MODIFY_DEST if there's type promotion involved.  */
      if (!useless_type_conversion_p (callee_type, caller_type))
   use_it = false;

      /* ??? If we're assigning to a variable sized type, then we must
    reuse the destination variable, because we've no good way to
    create variable sized temporaries at this point.  */
      else if (!poly_int_tree_p (TYPE_SIZE_UNIT (caller_type)))
   use_it = true;

      /* If the callee cannot possibly modify MODIFY_DEST, then we can
    reuse it as the result of the call directly.  Don't do this if
    it would promote MODIFY_DEST to addressable.  */
      else if (TREE_ADDRESSABLE (result))
   use_it = false;
      else
   {
     tree base_m = get_base_address (modify_dest);

     /* If the base isn't a decl, then it's a pointer, and we don't
        know where that's going to go.  */
     if (!DECL_P (base_m))
       use_it = false;
     else if (is_global_var (base_m))
       use_it = false;
     else if (DECL_NOT_GIMPLE_REG_P (result)
         && !DECL_NOT_GIMPLE_REG_P (base_m))
       use_it = false;
     else if (!TREE_ADDRESSABLE (base_m))
       use_it = true;
   }

      if (use_it)
   {
     var = modify_dest;
     use = NULL;
     goto done;
   }
    }

  gcc_assert (poly_int_tree_p (TYPE_SIZE_UNIT (callee_type)));

  var = copy_result_decl_to_var (result, id);
  DECL_SEEN_IN_BIND_EXPR_P (var) = 1;

  /* Do not have the rest of GCC warn about this variable as it should
     not be visible to the user.  */
  suppress_warning (var /* OPT_Wuninitialized? */);

  declare_inline_vars (id->block, var);

  /* Build the use expr.  If the return type of the function was
     promoted, convert it back to the expected type.  */
  use = var;
  if (!useless_type_conversion_p (caller_type, TREE_TYPE (var)))
    {
      /* If we can match up types by promotion/demotion do so.  */
      if (fold_convertible_p (caller_type, var))
   use = fold_convert (caller_type, var);
      else
   {
     /* ???  For valid programs we should not end up here.
        Still if we end up with truly mismatched types here, fall back
        to using a MEM_REF to not leak invalid GIMPLE to the following
        passes.  */
     /* Prevent var from being written into SSA form.  */
     if (is_gimple_reg_type (TREE_TYPE (var)))
       DECL_NOT_GIMPLE_REG_P (var) = true;
     use = fold_build2 (MEM_REF, caller_type,
              build_fold_addr_expr (var),
              build_int_cst (ptr_type_node, 0));
   }
    }

  STRIP_USELESS_TYPE_CONVERSION (use);

  if (DECL_BY_REFERENCE (result))
    {
      TREE_ADDRESSABLE (var) = 1;
      var = build_fold_addr_expr (var);
    }

 done:
  /* Register the VAR_DECL as the equivalent for the RESULT_DECL; that
     way, when the RESULT_DECL is encountered, it will be
     automatically replaced by the VAR_DECL.

     When returning by reference, ensure that RESULT_DECL remaps to
     gimple_val.  */
  if (DECL_BY_REFERENCE (result)
      && !is_gimple_val (var))
    {
      tree temp = create_tmp_var (TREE_TYPE (result), "retvalptr");
      insert_decl_map (id, result, temp);
      /* When RESULT_DECL is in SSA form, we need to remap and initialize
    it's default_def SSA_NAME.  */
      if (gimple_in_ssa_p (id->src_cfun)
     && is_gimple_reg (result))
   if (tree default_def = ssa_default_def (id->src_cfun, result))
     {
       temp = make_ssa_name (temp);
       insert_decl_map (id, default_def, temp);
     }
      insert_init_stmt (id, entry_bb, gimple_build_assign (temp, var));
    }
  else
    insert_decl_map (id, result, var);

  /* Remember this so we can ignore it in remap_decls.  */
  id->retvar = var;
  return use;
}

/* Copy the whole block tree and root it in id->block.  */

/* Remap the block tree rooted at BLOCK to nothing.  */

static void
remap_blocks_to_null (tree block, copy_body_data *id)
{
  tree t;
  insert_decl_map (id, block, NULL_TREE);
  for (t = BLOCK_SUBBLOCKS (block); t ; t = BLOCK_CHAIN (t))
    remap_blocks_to_null (t, id);
}

static tree
remap_blocks (tree block, copy_body_data *id)
{
  tree t;
  tree new_tree = block;

  if (!block)
    return NULL;

  remap_block (&new_tree, id);
  gcc_assert (new_tree != block);
  for (t = BLOCK_SUBBLOCKS (block); t ; t = BLOCK_CHAIN (t))
    prepend_lexical_block (new_tree, remap_blocks (t, id));
  /* Blocks are in arbitrary order, but make things slightly prettier and do
     not swap order when producing a copy.  */
  BLOCK_SUBBLOCKS (new_tree) = blocks_nreverse (BLOCK_SUBBLOCKS (new_tree));
  return new_tree;
}

/* Declare the variables created by the inliner.  Add all the variables in
   VARS to BIND_EXPR.  */

static void declare_inline_vars (tree block, tree vars)
{
  tree t;
  for (t = vars; t; t = DECL_CHAIN (t))
    {
      DECL_SEEN_IN_BIND_EXPR_P (t) = 1;
      gcc_assert (!TREE_STATIC (t) && !TREE_ASM_WRITTEN (t));
      add_local_decl (cfun, t);
    }

  if (block)
    BLOCK_VARS (block) = chainon (BLOCK_VARS (block), vars);
}

/* Return true if VALUE is an ADDR_EXPR of an automatic variable
   defined in function FN, or of a data member thereof.  */

static bool
self_inlining_addr_expr (tree value, tree fn)
{
  tree var;

  if (TREE_CODE (value) != ADDR_EXPR)
    return false;

  var = get_base_address (TREE_OPERAND (value, 0));

  return var && auto_var_in_fn_p (var, fn);
}

/* Initialize parameter P with VALUE.  If needed, produce init statement
   at the end of BB.  When BB is NULL, we return init statement to be
   output later.  */
static gimple *
setup_one_parameter (copy_body_data *id, tree p, tree value, tree fn,
           basic_block bb, tree *vars)
{
  gimple *init_stmt = NULL;
  tree var;
  tree def = (gimple_in_ssa_p (cfun)
         ? ssa_default_def (id->src_cfun, p) : NULL);

  /* Make an equivalent VAR_DECL.  Note that we must NOT remap the type
     here since the type of this decl must be visible to the calling
     function.  */
  var = copy_decl_to_var (p, id);

  /* Declare this new variable.  */
  DECL_CHAIN (var) = *vars;
  *vars = var;

  /* Make gimplifier happy about this variable.  */
  DECL_SEEN_IN_BIND_EXPR_P (var) = 1;

  /* If the parameter is never assigned to, has no SSA_NAMEs created,
     we would not need to create a new variable here at all, if it
     weren't for debug info.  Still, we can just use the argument
     value.  */
  if (TREE_READONLY (p)
      && !TREE_ADDRESSABLE (p)
      && value
      && !TREE_SIDE_EFFECTS (value)
      && !def)
    {
      /* We may produce non-gimple trees by adding NOPs or introduce invalid
    sharing when the value is not constant or DECL.  And we need to make
    sure that it cannot be modified from another path in the callee.  */
      if (((is_gimple_min_invariant (value)
       /* When the parameter is used in a context that forces it to
          not be a GIMPLE register avoid substituting something that
          is not a decl there.  */
       && ! DECL_NOT_GIMPLE_REG_P (p))
      || (DECL_P (value) && TREE_READONLY (value))
      || (auto_var_in_fn_p (value, id->dst_fn)
          && !TREE_ADDRESSABLE (value)))
     && useless_type_conversion_p (TREE_TYPE (p), TREE_TYPE (value))
     /* We have to be very careful about ADDR_EXPR.  Make sure
        the base variable isn't a local variable of the inlined
        function, e.g., when doing recursive inlining, direct or
        mutually-recursive or whatever, which is why we don't
        just test whether fn == current_function_decl.  */
     && ! self_inlining_addr_expr (value, fn))
   {
     insert_decl_map (id, p, value);
     if (!id->debug_map)
       id->debug_map = new hash_map<tree, tree>;
     id->debug_map->put (p, var);
     return insert_init_debug_bind (id, bb, var, value, NULL);
   }
    }

  /* Register the VAR_DECL as the equivalent for the PARM_DECL;
     that way, when the PARM_DECL is encountered, it will be
     automatically replaced by the VAR_DECL.  */
  insert_decl_map (id, p, var);

  /* Even if P was TREE_READONLY, the new VAR should not be.  In the original
     code, we would have constructed a temporary, and then the function body
     would have never changed the value of P.  However, now, we will be
     constructing VAR directly.  Therefore, it must not be TREE_READONLY.  */
  TREE_READONLY (var) = 0;

  tree rhs = value;
  if (value
      && value != error_mark_node
      && !useless_type_conversion_p (TREE_TYPE (p), TREE_TYPE (value)))
    rhs = force_value_to_type (TREE_TYPE (p), value);

  /* If there is no setup required and we are in SSA, take the easy route
     replacing all SSA names representing the function parameter by the
     SSA name passed to function.

     We need to construct map for the variable anyway as it might be used
     in different SSA names when parameter is set in function.

     Do replacement at -O0 for const arguments replaced by constant.
     This is important for builtin_constant_p and other construct requiring
     constant argument to be visible in inlined function body.  */
  if (gimple_in_ssa_p (cfun) && rhs && def && is_gimple_reg (p)
      && (optimize
          || (TREE_READONLY (p)
         && is_gimple_min_invariant (rhs)))
      && (TREE_CODE (rhs) == SSA_NAME
     || is_gimple_min_invariant (rhs))
      && !SSA_NAME_OCCURS_IN_ABNORMAL_PHI (def))
    {
      insert_decl_map (id, def, rhs);
      return insert_init_debug_bind (id, bb, var, rhs, NULL);
    }

  /* If the value of argument is never used, don't care about initializing
     it.  */
  if (optimize && gimple_in_ssa_p (cfun) && !def && is_gimple_reg (p))
    {
      /* When there's a gross type mismatch between the passed value
    and the declared argument type drop it on the floor and do
    not bother to insert a debug bind.  */
      if (value && !is_gimple_reg_type (TREE_TYPE (value)))
   return NULL;
      return insert_init_debug_bind (id, bb, var, rhs, NULL);
    }

  /* Initialize this VAR_DECL from the equivalent argument.  Convert
     the argument to the proper type in case it was promoted.  */
  if (value)
    {
      if (rhs == error_mark_node)
   {
     insert_decl_map (id, p, var);
     return insert_init_debug_bind (id, bb, var, rhs, NULL);
   }

      STRIP_USELESS_TYPE_CONVERSION (rhs);

      /* If we are in SSA form properly remap the default definition.  */
      if (gimple_in_ssa_p (cfun) && is_gimple_reg (p))
   {
     if (def)
       {
         def = remap_ssa_name (def, id);
         init_stmt = gimple_build_assign (def, rhs);
         SSA_NAME_IS_DEFAULT_DEF (def) = 0;
         set_ssa_default_def (cfun, var, NULL);
       }
   }
      else if (!is_empty_type (TREE_TYPE (var)))
        init_stmt = gimple_build_assign (var, rhs);

      if (bb && init_stmt)
        insert_init_stmt (id, bb, init_stmt);
    }
  return init_stmt;
}

/* Generate code to initialize the parameters of the function at the
   top of the stack in ID from the GIMPLE_CALL STMT.  */

static void initialize_inlined_parameters (copy_body_data *id, gimple *stmt,
                tree fn, basic_block bb)
{
  tree parms;
  size_t i;
  tree p;
  tree vars = NULL_TREE;
  tree static_chain = gimple_call_chain (stmt);

  /* Figure out what the parameters are.  */
  parms = DECL_ARGUMENTS (fn);

  /* Loop through the parameter declarations, replacing each with an
     equivalent VAR_DECL, appropriately initialized.  */
  for (p = parms, i = 0; p; p = DECL_CHAIN (p), i++)
    {
      tree val;
      val = i < gimple_call_num_args (stmt) ? gimple_call_arg (stmt, i) : NULL;
      setup_one_parameter (id, p, val, fn, bb, &vars);
    }
  /* After remapping parameters remap their types.  This has to be done
     in a second loop over all parameters to appropriately remap
     variable sized arrays when the size is specified in a
     parameter following the array.  */
  for (p = parms, i = 0; p; p = DECL_CHAIN (p), i++)
    {
      tree *varp = id->decl_map->get (p);
      if (varp && VAR_P (*varp))
   {
     tree def = (gimple_in_ssa_p (cfun) && is_gimple_reg (p)
            ? ssa_default_def (id->src_cfun, p) : NULL);
     tree var = *varp;
     TREE_TYPE (var) = remap_type (TREE_TYPE (var), id);
     /* Also remap the default definition if it was remapped
        to the default definition of the parameter replacement
        by the parameter setup.  */
     if (def)
       {
         tree *defp = id->decl_map->get (def);
         if (defp
        && TREE_CODE (*defp) == SSA_NAME
        && SSA_NAME_VAR (*defp) == var)
      TREE_TYPE (*defp) = TREE_TYPE (var);
       }
     /* When not optimizing and the parameter is unused, assign to
        a dummy SSA name.  Do this after remapping the type above.  */
     else if (!optimize
         && is_gimple_reg (p)
         && i < gimple_call_num_args (stmt))
       {
         tree val = gimple_call_arg (stmt, i);
         if (val != error_mark_node)
      {
        if (!useless_type_conversion_p (TREE_TYPE (p),
                    TREE_TYPE (val)))
          val = force_value_to_type (TREE_TYPE (p), val);
        def = make_ssa_name (var);
        gimple *init_stmt = gimple_build_assign (def, val);
        insert_init_stmt (id, bb, init_stmt);
      }
       }
   }
    }

  /* Initialize the static chain.  */
  p = DECL_STRUCT_FUNCTION (fn)->static_chain_decl;
  gcc_assert (fn != current_function_decl);
  if (p)
    {
      /* No static chain?  Seems like a bug in tree-nested.cc.  */
      gcc_assert (static_chain);

      setup_one_parameter (id, p, static_chain, fn, bb, &vars);
    }

  /* Reverse so the variables appear in the correct order in DWARF
     debug info.  */
  vars = nreverse (vars);

  declare_inline_vars (id->block, vars);
}

static tree getInlineFunc(gcall *call)
{
   MtcsTarget *mtcsTarget = mtcs_compile_get()->currentMtcsTarget;
   AetMediatorUser *mediatorUser =(AetMediatorUser *)mtcs_compile_get();
   AetMediator *mediator = mediatorUser->mediator;
   tree micFndecl = gimple_call_fndecl (call);

   //miifndecl来自主机的
   tree miifndecl = aet_mediator_get_mii_function(mediator,micFndecl,mediatorUser);
   if(miifndecl){
      char *name = IDENTIFIER_POINTER(DECL_NAME(miifndecl));
      cgraph_node *cnode;
      int i=0;
      tree targetMiiFndecl = NULL_TREE;
      //在当前符号表中找出函数名相同的函数
      FOR_EACH_FUNCTION (cnode){
         n_debug("mtcsreplace.c getInlineFunc 当前符号表中的 i:%d node:%p name:%s decl:%p init:%p 是否定义:%d\n",
               i++,cnode,cnode->name(),cnode->decl,DECL_INITIAL(cnode->decl),cnode->definition);
         n_debug("mtcsreplace.c getInlineFunc 当前符号表中的 %p %s\n",miifndecl,
               IDENTIFIER_POINTER(DECL_NAME(miifndecl)));
         if(strcmp(name,cnode->name())==0){
            targetMiiFndecl = cnode->decl;
            break;
         }
      }

      if(targetMiiFndecl){
         if(cnode->definition){
            return targetMiiFndecl;
         }
      }
      printf("mtcsreplace.c getInlineFunc 没有mii的源代码，改为调用\n");
      gimple_call_set_fndecl (call, miifndecl);
      cgraph_update_edges_for_call_stmt (call, miifndecl, call);
   }else{
      const char *platName =  mtcs_target_get_platform_name(mtcsTarget);
      const char *isa = mtcs_target_get_isa_string(mtcsTarget);
      error_at(gimple_location(call),"mic类中的 intrinsic 方法在平台%s(%s)中还没有实现，或参数与实现不区配。",platName,isa);
   }
   return NULL_TREE;
}

/* If STMT is a GIMPLE_CALL, replace it with its inline expansion.  */
//原型 expand_call_inline tree-inline.cc
//需要改3处地方才能完成编译
//1.需要 cg_edge->callee->inlined_to = id->dst_node;否则 cg_edge->callee->verify ();失败
//2.需要 cg_edge->callee->force_output = 0; 否则 cg_edge->callee->verify ();失败
//3.if(id->src_cfun->after_inlining)
//    id->src_cfun->after_inlining = 0;
static bool expand_call_inline (basic_block bb, gimple *stmt, copy_body_data *id,bitmap to_purge)
{
   tree use_retvar;
   tree fn;
   hash_map<tree, tree> *dst;
   hash_map<tree, tree> *st = NULL;
   tree return_slot;
   tree modify_dest;
   struct cgraph_edge *cg_edge;
   cgraph_inline_failed_t reason;
   basic_block return_block;
   edge e;
   gimple_stmt_iterator gsi, stmt_gsi;
   bool successfully_inlined = false;
   bool purge_dead_abnormal_edges;
   gcall *call_stmt;
   unsigned int prop_mask, src_properties;
   struct function *dst_cfun;
   tree simduid;
   use_operand_p use;
   gimple *simtenter_stmt = NULL;
   vec<tree> *simtvars_save;
   tree save_stack = NULL_TREE;

   /* The gimplifier uses input_location in too many places, such as
   internal_get_tmp_var ().  */
   location_t saved_location = input_location;
   input_location = gimple_location (stmt);

   /* From here on, we're only interested in CALL_EXPRs.  */
   call_stmt = dyn_cast <gcall *> (stmt);
   if (!call_stmt)
      goto egress;
   n_debug("mtcsreplace.c expand_call_inline 00 id->dst_node:%p\n",id->dst_node);
   aet_print_gimple(call_stmt);
   cg_edge = id->dst_node->get_edge (stmt);
   gcc_checking_assert (cg_edge);
   /* First, see if we can figure out what function is being called.
   If we cannot, then there is no hope of inlining the function.  */
   if (cg_edge->indirect_unknown_callee)
      goto egress;
   fn  = cg_edge->callee->decl;
   if(!fn)
      goto egress;

   gcc_checking_assert (fn);
   n_debug("mtcsreplace.c expand_call_inline 11\n");

   /* If FN is a declaration of a function in a nested scope that was
   globally declared inline, we don't set its DECL_INITIAL.
   However, we can't blindly follow DECL_ABSTRACT_ORIGIN because the
   C++ front-end uses it for cdtors to refer to their internal
   declarations, that are not real functions.  Fortunately those
   don't have trees to be saved, so we can tell by checking their
   gimple_body.  */
   if (!DECL_INITIAL (fn) && DECL_ABSTRACT_ORIGIN (fn) && gimple_has_body_p (DECL_ABSTRACT_ORIGIN (fn)))
      fn = DECL_ABSTRACT_ORIGIN (fn);
   n_debug("mtcsreplace.c expand_call_inline 22 inline_failed:%d\n",cg_edge->inline_failed);
   //强制改为零，否则不能内联
   cg_edge->inline_failed = 0;
   /* Don't try to inline functions that are not well-suited to inlining.  */
   if (cg_edge->inline_failed){
      reason = cg_edge->inline_failed;
      /* If this call was originally indirect, we do not want to emit any
      inlining related warnings or sorry messages because there are no
      guarantees regarding those.  */
      if (cg_edge->indirect_inlining_edge)
         goto egress;

      if (lookup_attribute ("always_inline", DECL_ATTRIBUTES (fn))
      /* For extern inline functions that get redefined we always
      silently ignored always_inline flag. Better behavior would
      be to be able to keep both bodies and use extern inline body
      for inlining, but we can't do that because frontends overwrite
      the body.  */
      && !cg_edge->callee->redefined_extern_inline
      /* During early inline pass, report only when optimization is
      not turned on.  */
      && (symtab->global_info_ready
      || !optimize
      || cgraph_inline_failed_type (reason) == CIF_FINAL_ERROR)
      /* PR 20090218-1_0.c. Body can be provided by another module. */
      && (reason != CIF_BODY_NOT_AVAILABLE || !flag_generate_lto)) {
         error ("inlining failed in call to %<always_inline%> %q+F: %s", fn,cgraph_inline_failed_string (reason));
         if (gimple_location (stmt) != UNKNOWN_LOCATION)
            inform (gimple_location (stmt), "called from here");
         else if (DECL_SOURCE_LOCATION (cfun->decl) != UNKNOWN_LOCATION)
            inform (DECL_SOURCE_LOCATION (cfun->decl),"called from this function");
      }else if (opt_for_fn (fn, warn_inline)
      && DECL_DECLARED_INLINE_P (fn)
      && !DECL_NO_INLINE_WARNING_P (fn)
      && !DECL_IN_SYSTEM_HEADER (fn)
      && reason != CIF_UNSPECIFIED
      && !lookup_attribute ("noinline", DECL_ATTRIBUTES (fn))
      /* Do not warn about not inlined recursive calls.  */
      && !cg_edge->recursive_p ()
      /* Avoid warnings during early inline pass. */
      && symtab->global_info_ready){
         auto_diagnostic_group d;
         if (warning (OPT_Winline, "inlining failed in call to %q+F: %s",fn, cgraph_inline_failed_string (reason))){
            if (gimple_location (stmt) != UNKNOWN_LOCATION)
               inform (gimple_location (stmt), "called from here");
            else if (DECL_SOURCE_LOCATION (cfun->decl) != UNKNOWN_LOCATION)
               inform (DECL_SOURCE_LOCATION (cfun->decl),"called from this function");
         }
      }
      goto egress;
   }
   id->src_node = cg_edge->callee;
   n_debug("mtcsreplace.c expand_call_inline 33 inline_failed:%d %p\n",cg_edge->inline_failed,id->src_node->thunk);

   /* If callee is thunk, all we need is to adjust the THIS pointer
   and redirect to function being thunked.  */
   if (id->src_node->thunk){
      cgraph_edge *edge;
      tree virtual_offset = NULL;
      profile_count count = cg_edge->count;
      tree op;
      gimple_stmt_iterator iter = gsi_for_stmt (stmt);
      thunk_info *info = thunk_info::get (id->src_node);

      cgraph_edge::remove (cg_edge);
      edge = id->src_node->callees->clone (id->dst_node, call_stmt,
           gimple_uid (stmt),profile_count::one (),profile_count::one (),true);
      edge->count = count;
      if (info->virtual_offset_p)
         virtual_offset = size_int (info->virtual_value);
      op = create_tmp_reg_fn (cfun, TREE_TYPE (gimple_call_arg (stmt, 0)),NULL);
      gsi_insert_before (&iter, gimple_build_assign (op,gimple_call_arg (stmt, 0)),GSI_NEW_STMT);
      gcc_assert (info->this_adjusting);
      op = thunk_adjust (&iter, op, 1, info->fixed_offset,virtual_offset, info->indirect_offset);

      gimple_call_set_arg (stmt, 0, op);
      gimple_call_set_fndecl (stmt, edge->callee->decl);
      update_stmt (stmt);
      id->src_node->remove ();
      successfully_inlined = expand_call_inline (bb, stmt, id, to_purge);
      maybe_remove_unused_call_args (cfun, stmt);
      /* This used to return true even though we do fail to inline in
      some cases.  See PR98525.  */
      goto egress;
   }


   n_debug("mtcsreplace.c expand_call_inline 44 inline_failed:%d %p %p %p %p %s %s\n",
         cg_edge->inline_failed,id->dst_node->decl,cg_edge->callee->decl,id->dst_node,cg_edge->callee,
         cg_edge->callee->name(),id->dst_node->name());

   fn = cg_edge->callee->decl;
   cg_edge->callee->get_untransformed_body ();
   //需要 cg_edge->callee->inlined_to = id->dst_node;否则 cg_edge->callee->verify ();失败
   if(!cg_edge->callee->inlined_to){
      cg_edge->callee->inlined_to = id->dst_node;
      n_debug("被调函数的是 inlined_to 是空的，%s force_output:%d\n",cg_edge->callee->name(),cg_edge->callee->force_output);
   }
   //需要 cg_edge->callee->force_output = 0; 否则 cg_edge->callee->verify ();失败
   if(cg_edge->callee->force_output){
      cg_edge->callee->force_output = 0;
   }

   if (flag_checking && cg_edge->callee->decl != id->dst_node->decl)
      cg_edge->callee->verify ();

   fprintf(stderr,"tree-inline.c expand call_inline 55 fn:%p id->src_cfun:%p id->src_cfun->after_inlining:%d\n",
         fn,id->src_cfun,id->src_cfun?id->src_cfun->after_inlining:0);

   /* We will be inlining this callee.  */
   id->eh_lp_nr = lookup_stmt_eh_lp (stmt);

   /* Update the callers EH personality.  */
   if (DECL_FUNCTION_PERSONALITY (fn))
      DECL_FUNCTION_PERSONALITY (cg_edge->caller->decl) = DECL_FUNCTION_PERSONALITY (fn);

   /* Split the block before the GIMPLE_CALL.  */
   stmt_gsi = gsi_for_stmt (stmt);
   gsi_prev (&stmt_gsi);
   e = split_block (bb, gsi_end_p (stmt_gsi) ? NULL : gsi_stmt (stmt_gsi));
   bb = e->src;
   return_block = e->dest;
   remove_edge (e);

   /* If the GIMPLE_CALL was in the last statement of BB, it may have
   been the source of abnormal edges.  In this case, schedule
   the removal of dead abnormal edges.  */
   gsi = gsi_start_bb (return_block);
   gsi_next (&gsi);
   purge_dead_abnormal_edges = gsi_end_p (gsi);

   stmt_gsi = gsi_start_bb (return_block);

   /* Build a block containing code to initialize the arguments, the
   actual inline expansion of the body, and a label for the return
   statements within the function to jump to.  The type of the
   statement expression is the return type of the function call.
   ???  If the call does not have an associated block then we will
   remap all callee blocks to NULL, effectively dropping most of
   its debug information.  This should only happen for calls to
   artificial decls inserted by the compiler itself.  We need to
   either link the inlined blocks into the caller block tree or
   not refer to them in any way to not break GC for locations.  */
   if (tree block = gimple_block (stmt)){
      /* We do want to assign a not UNKNOWN_LOCATION BLOCK_SOURCE_LOCATION
      to make inlined_function_outer_scope_p return true on this BLOCK.  */
      location_t loc = LOCATION_LOCUS (gimple_location (stmt));
      if (loc == UNKNOWN_LOCATION)
         loc = LOCATION_LOCUS (DECL_SOURCE_LOCATION (fn));
      if (loc == UNKNOWN_LOCATION)
         loc = BUILTINS_LOCATION;
      id->block = make_node (BLOCK);
      BLOCK_ABSTRACT_ORIGIN (id->block) = DECL_ORIGIN (fn);
      BLOCK_SOURCE_LOCATION (id->block) = loc;
      prepend_lexical_block (block, id->block);
   }

   /* Local declarations will be replaced by their equivalents in this map.  */
   st = id->decl_map;
   id->decl_map = new hash_map<tree, tree>;
   dst = id->debug_map;
   id->debug_map = NULL;
   if (flag_stack_reuse != SR_NONE)
      id->add_clobbers_to_eh_landing_pads = last_basic_block_for_fn (cfun);

   /* Record the function we are about to inline.  */
   id->src_fn = fn;
   id->src_cfun = DECL_STRUCT_FUNCTION (fn);
   id->reset_location = DECL_IGNORED_P (fn);
   id->call_stmt = call_stmt;
   cfun->cfg->full_profile &= id->src_cfun->cfg->full_profile;
   fprintf(stderr,"tree-inline.c expand call_inline 66 fn:%p id->src_cfun:%p id->src_cfun->after_inlining:%d\n",
         fn,id->src_cfun,id->src_cfun?id->src_cfun->after_inlining:0);
   if(id->src_cfun->after_inlining)
      id->src_cfun->after_inlining = 0;
   /* When inlining into an OpenMP SIMD-on-SIMT loop, arrange for new automatic
   variables to be added to IFN_GOMP_SIMT_ENTER argument list.  */
   dst_cfun = DECL_STRUCT_FUNCTION (id->dst_fn);
   simtvars_save = id->dst_simt_vars;
   if (!(dst_cfun->curr_properties & PROP_gimple_lomp_dev)
   && (simduid = bb->loop_father->simduid) != NULL_TREE
   && (simduid = ssa_default_def (dst_cfun, simduid)) != NULL_TREE
   && single_imm_use (simduid, &use, &simtenter_stmt)
   && is_gimple_call (simtenter_stmt)
   && gimple_call_internal_p (simtenter_stmt, IFN_GOMP_SIMT_ENTER))
      vec_alloc (id->dst_simt_vars, 0);
   else
      id->dst_simt_vars = NULL;

   if (profile_status_for_fn (id->src_cfun) == PROFILE_ABSENT)
      profile_status_for_fn (dst_cfun) = PROFILE_ABSENT;

   fprintf(stderr,"tree-inline.c expand call_inline 77 fn:%p id->src_cfun:%p id->src_cfun->after_inlining:%d\n",
         fn,id->src_cfun,id->src_cfun?id->src_cfun->after_inlining:0);
   /* If the src function contains an IFN_VA_ARG, then so will the dst
   function after inlining.  Likewise for IFN_GOMP_USE_SIMT.  */
   prop_mask = PROP_gimple_lva | PROP_gimple_lomp_dev;
   src_properties = id->src_cfun->curr_properties & prop_mask;
   if (src_properties != prop_mask)
      dst_cfun->curr_properties &= src_properties | ~prop_mask;
   dst_cfun->calls_eh_return |= id->src_cfun->calls_eh_return;
   id->dst_node->has_omp_variant_constructs |= id->src_node->has_omp_variant_constructs;

   fprintf(stderr,"tree-inline.c expand call_inline 88 fn:%p id->src_cfun:%p id->src_cfun->after_inlining:%d\n",
         fn,id->src_cfun,id->src_cfun?id->src_cfun->after_inlining:0);

   gcc_assert (!id->src_cfun->after_inlining);

   id->entry_bb = bb;
   if (lookup_attribute ("cold", DECL_ATTRIBUTES (fn))){
      gimple_stmt_iterator si = gsi_last_bb (bb);
      gsi_insert_after (&si, gimple_build_predict (PRED_COLD_FUNCTION,NOT_TAKEN),GSI_NEW_STMT);
   }
   initialize_inlined_parameters (id, stmt, fn, bb);
   if (debug_nonbind_markers_p && debug_inline_points && id->block
   && inlined_function_outer_scope_p (id->block)){
      gimple_stmt_iterator si = gsi_last_bb (bb);
      gsi_insert_after (&si, gimple_build_debug_inline_entry (id->block,
            DECL_SOURCE_LOCATION (id->src_fn)), GSI_NEW_STMT);
   }

   /* If function to be inlined calls alloca, wrap the inlined function
   in between save_stack = __builtin_stack_save (); and
   __builtin_stack_restore (save_stack); calls.  */
   if (id->src_cfun->calls_alloca && !gimple_call_noreturn_p (stmt))
      /* Don't do this for VLA allocations though, just for user alloca
      calls.  */
      for (struct cgraph_edge *e = id->src_node->callees; e; e = e->next_callee)
         if (gimple_maybe_alloca_call_p (e->call_stmt) && !gimple_call_alloca_for_var_p (e->call_stmt)){
            tree fn = builtin_decl_implicit (BUILT_IN_STACK_SAVE);
            fprintf(stderr,"tree-inline.cc 创建 BUILT_IN_STACK_SAVE\n");
            gcall *call = gimple_build_call (fn, 0);
            save_stack = make_ssa_name (ptr_type_node);
            gimple_call_set_lhs (call, save_stack);
            gimple_stmt_iterator si = gsi_last_bb (bb);
            gsi_insert_after (&si, call, GSI_NEW_STMT);
            struct cgraph_node *dest = cgraph_node::get_create (fn);
            id->dst_node->create_edge (dest, call, bb->count)->inline_failed = CIF_BODY_NOT_AVAILABLE;
            break;
         }

   if (DECL_INITIAL (fn)){
      if (gimple_block (stmt)){
         tree *var;

         prepend_lexical_block (id->block,
         remap_blocks (DECL_INITIAL (fn), id));
         gcc_checking_assert (BLOCK_SUBBLOCKS (id->block)
         && (BLOCK_CHAIN (BLOCK_SUBBLOCKS (id->block)) == NULL_TREE));
         /* Move vars for PARM_DECLs from DECL_INITIAL block to id->block,
         otherwise for DWARF DW_TAG_formal_parameter will not be children of
         DW_TAG_inlined_subroutine, but of a DW_TAG_lexical_block
         under it.  The parameters can be then evaluated in the debugger,
         but don't show in backtraces.  */
         for (var = &BLOCK_VARS (BLOCK_SUBBLOCKS (id->block)); *var; )
            if (TREE_CODE (DECL_ORIGIN (*var)) == PARM_DECL){
               tree v = *var;
               *var = TREE_CHAIN (v);
               TREE_CHAIN (v) = BLOCK_VARS (id->block);
               BLOCK_VARS (id->block) = v;
            }else
               var = &TREE_CHAIN (*var);
      }else
         remap_blocks_to_null (DECL_INITIAL (fn), id);
   }

   /* Return statements in the function body will be replaced by jumps
   to the RET_LABEL.  */
   gcc_assert (DECL_INITIAL (fn));
   gcc_assert (TREE_CODE (DECL_INITIAL (fn)) == BLOCK);

   /* Find the LHS to which the result of this call is assigned.  */
   return_slot = NULL;
   if (gimple_call_lhs (stmt)){
      modify_dest = gimple_call_lhs (stmt);

      /* The function which we are inlining might not return a value,
      in which case we should issue a warning that the function
      does not return a value.  In that case the optimizers will
      see that the variable to which the value is assigned was not
      initialized.  We do not want to issue a warning about that
      uninitialized variable.  */
      if (DECL_P (modify_dest))
         suppress_warning (modify_dest, OPT_Wuninitialized);

      /* If we have a return slot, we can assign it the result directly,
      except in the case where it is a global variable that is only
      written to because, the callee being permitted to read or take
      the address of its DECL_RESULT, this could invalidate the flag
      on the global variable; instead we preventively remove the store,
      which would have happened later if the call was not inlined.  */
      if (gimple_call_return_slot_opt_p (call_stmt)){
         tree base = get_base_address (modify_dest);

         if (VAR_P (base) && (TREE_STATIC (base) || DECL_EXTERNAL (base)) && varpool_node::get (base)->writeonly)
            return_slot = NULL;
         else
            return_slot = modify_dest;

         modify_dest = NULL;
      }
   }else
      modify_dest = NULL;

   /* If we are inlining a call to the C++ operator new, we don't want
   to use type based alias analysis on the return value.  Otherwise
   we may get confused if the compiler sees that the inlined new
   function returns a pointer which was just deleted.  See bug
   33407.  */
   if (DECL_IS_OPERATOR_NEW_P (fn)){
      return_slot = NULL;
      modify_dest = NULL;
   }

   /* Declare the return variable for the function.  */
   use_retvar = declare_return_variable (id, return_slot, modify_dest, bb);

   /* Add local vars in this inlined callee to caller.  */
   add_local_variables (id->src_cfun, cfun, id);

   if (dump_enabled_p ()){
      char buf[128];
      snprintf (buf, sizeof(buf), "%4.2f",
      cg_edge->sreal_frequency ().to_double ());
      dump_printf_loc (MSG_NOTE | MSG_PRIORITY_INTERNALS,
      call_stmt,
      "Inlining %C to %C with frequency %s\n",
      id->src_node, id->dst_node, buf);
      if (dump_file && (dump_flags & TDF_DETAILS)){
         id->src_node->dump (dump_file);
         id->dst_node->dump (dump_file);
      }
   }

   /* This is it.  Duplicate the callee body.  Assume callee is
   pre-gimplified.  Note that we must not alter the caller
   function in any way before this point, as this CALL_EXPR may be
   a self-referential call; if we're calling ourselves, we need to
   duplicate our body before altering anything.  */
   copy_body (id, bb, return_block, NULL);

   reset_debug_bindings (id, stmt_gsi);

   if (flag_stack_reuse != SR_NONE)
      for (tree p = DECL_ARGUMENTS (id->src_fn); p; p = DECL_CHAIN (p))
         if (!TREE_THIS_VOLATILE (p)){
            /* The value associated with P is a local temporary only if
            there is no value associated with P in the debug map.  */
            tree *varp = id->decl_map->get (p);
            if (varp  && VAR_P (*varp) && !is_gimple_reg (*varp)
            && !(id->debug_map && id->debug_map->get (p))){
               tree clobber = build_clobber (TREE_TYPE (*varp),CLOBBER_STORAGE_END);
               gimple *clobber_stmt;
               clobber_stmt = gimple_build_assign (*varp, clobber);
               gimple_set_location (clobber_stmt, gimple_location (stmt));
               gsi_insert_before (&stmt_gsi, clobber_stmt, GSI_SAME_STMT);
            }
         }

   if (save_stack){
      tree fn = builtin_decl_implicit (BUILT_IN_STACK_RESTORE);
      gcall *call = gimple_build_call (fn, 1, save_stack);
      gsi_insert_before (&stmt_gsi, call, GSI_SAME_STMT);
      struct cgraph_node *dest = cgraph_node::get_create (fn);
      id->dst_node->create_edge (dest, call,return_block->count)->inline_failed= CIF_BODY_NOT_AVAILABLE;
   }

   /* Reset the escaped solution.  */
   if (cfun->gimple_df){
      pt_solution_reset (&cfun->gimple_df->escaped);
      pt_solution_reset (&cfun->gimple_df->escaped_return);
   }

   /* Add new automatic variables to IFN_GOMP_SIMT_ENTER arguments.  */
   if (id->dst_simt_vars && id->dst_simt_vars->length () > 0){
      size_t nargs = gimple_call_num_args (simtenter_stmt);
      vec<tree> *vars = id->dst_simt_vars;
      auto_vec<tree> newargs (nargs + vars->length ());
      for (size_t i = 0; i < nargs; i++)
         newargs.quick_push (gimple_call_arg (simtenter_stmt, i));
      for (tree *pvar = vars->begin (); pvar != vars->end (); pvar++){
         tree ptrtype = build_pointer_type (TREE_TYPE (*pvar));
         newargs.quick_push (build1 (ADDR_EXPR, ptrtype, *pvar));
      }
      gcall *g = gimple_build_call_internal_vec (IFN_GOMP_SIMT_ENTER, newargs);
      gimple_call_set_lhs (g, gimple_call_lhs (simtenter_stmt));
      gimple_stmt_iterator gsi = gsi_for_stmt (simtenter_stmt);
      gsi_replace (&gsi, g, false);
   }
   vec_free (id->dst_simt_vars);
   id->dst_simt_vars = simtvars_save;

   /* Clean up.  */
   if (id->debug_map){
      delete id->debug_map;
      id->debug_map = dst;
   }
   delete id->decl_map;
   id->decl_map = st;

   /* Unlink the calls virtual operands before replacing it.  */
   unlink_stmt_vdef (stmt);
   if (gimple_vdef (stmt) && TREE_CODE (gimple_vdef (stmt)) == SSA_NAME)
      release_ssa_name (gimple_vdef (stmt));

   /* If the inlined function returns a result that we care about,
   substitute the GIMPLE_CALL with an assignment of the return
   variable to the LHS of the call.  That is, if STMT was
   'a = foo (...)', substitute the call with 'a = USE_RETVAR'.  */
   if (use_retvar && gimple_call_lhs (stmt)){
      gimple *old_stmt = stmt;
      stmt = gimple_build_assign (gimple_call_lhs (stmt), use_retvar);
      gimple_set_location (stmt, gimple_location (old_stmt));
      gsi_replace (&stmt_gsi, stmt, false);
      maybe_clean_or_replace_eh_stmt (old_stmt, stmt);
      /* Append a clobber for id->retvar if easily possible.  */
      if (flag_stack_reuse != SR_NONE
      && id->retvar
      && VAR_P (id->retvar)
      && id->retvar != return_slot
      && id->retvar != modify_dest
      && !TREE_THIS_VOLATILE (id->retvar)
      && !is_gimple_reg (id->retvar)
      && !stmt_ends_bb_p (stmt)){
         tree clobber = build_clobber (TREE_TYPE (id->retvar),CLOBBER_STORAGE_END);
         gimple *clobber_stmt;
         clobber_stmt = gimple_build_assign (id->retvar, clobber);
         gimple_set_location (clobber_stmt, gimple_location (old_stmt));
         gsi_insert_after (&stmt_gsi, clobber_stmt, GSI_SAME_STMT);
      }
   }else{
      /* Handle the case of inlining a function with no return
      statement, which causes the return value to become undefined.  */
      if (gimple_call_lhs (stmt) && TREE_CODE (gimple_call_lhs (stmt)) == SSA_NAME){
         tree name = gimple_call_lhs (stmt);
         tree var = SSA_NAME_VAR (name);
         tree def = var ? ssa_default_def (cfun, var) : NULL;

         if (def){
            /* If the variable is used undefined, make this name
            undefined via a move.  */
            stmt = gimple_build_assign (gimple_call_lhs (stmt), def);
            gsi_replace (&stmt_gsi, stmt, true);
         }else{
            if (!var){
               var = create_tmp_reg_fn (cfun, TREE_TYPE (name), NULL);
               SET_SSA_NAME_VAR_OR_IDENTIFIER (name, var);
            }
            /* Otherwise make this variable undefined.  */
            gsi_remove (&stmt_gsi, true);
            set_ssa_default_def (cfun, var, name);
            SSA_NAME_DEF_STMT (name) = gimple_build_nop ();
         }
      }
      /* Replace with a clobber for id->retvar.  */
      else if (flag_stack_reuse != SR_NONE
      && id->retvar
      && VAR_P (id->retvar)
      && id->retvar != return_slot
      && id->retvar != modify_dest
      && !TREE_THIS_VOLATILE (id->retvar)
      && !is_gimple_reg (id->retvar)){
         tree clobber = build_clobber (TREE_TYPE (id->retvar));
         gimple *clobber_stmt;
         clobber_stmt = gimple_build_assign (id->retvar, clobber);
         gimple_set_location (clobber_stmt, gimple_location (stmt));
         gsi_replace (&stmt_gsi, clobber_stmt, false);
         maybe_clean_or_replace_eh_stmt (stmt, clobber_stmt);
      }else
         gsi_remove (&stmt_gsi, true);
   }

   if (purge_dead_abnormal_edges)
      bitmap_set_bit (to_purge, return_block->index);

   /* If the value of the new expression is ignored, that's OK.  We
   don't warn about this for CALL_EXPRs, so we shouldn't warn about
   the equivalent inlined version either.  */
   if (is_gimple_assign (stmt)){
      gcc_assert (gimple_assign_single_p (stmt) || CONVERT_EXPR_CODE_P (gimple_assign_rhs_code (stmt)));
      TREE_USED (gimple_assign_rhs1 (stmt)) = 1;
   }

   id->add_clobbers_to_eh_landing_pads = 0;

   /* Output the inlining info for this abstract function, since it has been
   inlined.  If we don't do this now, we can lose the information about the
   variables in the function when the blocks get blown away as soon as we
   remove the cgraph node.  */
   if (gimple_block (stmt))
      (*debug_hooks->outlining_inline_function) (fn);

   /* Update callgraph if needed.  */
   cg_edge->callee->remove ();

   id->block = NULL_TREE;
   id->retvar = NULL_TREE;
   successfully_inlined = true;

egress:
   input_location = saved_location;
   return successfully_inlined;
}



/* Expand call statements reachable from STMT_P.
   We can only have CALL_EXPRs as the "toplevel" tree code or nested
   in a MODIFY_EXPR.  */

static bool gimple_expand_calls_inline (basic_block bb, copy_body_data *id,bitmap to_purge)
{
   gimple_stmt_iterator gsi;
   bool inlined = false;

   for (gsi = gsi_last_bb (bb); !gsi_end_p (gsi);){
      gimple *stmt = gsi_stmt (gsi);
      gsi_prev (&gsi);
      if (is_gimple_call (stmt)  && !gimple_call_internal_p (stmt)){
         tree callee = gimple_call_fndecl (stmt);
         MtcsFuncType callleeType =  mtcs_info_get_func_type(callee);
         if(callleeType==MTCS_FUNC_DEVICE  && mtcs_info_is_intrinsic_function_impl(callee)){
            n_debug("mtcsreplace.c gimple_expand_calls_inline 00\n");
            aet_print_gimple(stmt);
            inlined |= expand_call_inline (bb, stmt, id, to_purge);
         }
      }
   }

   return inlined;
}

/* Install new lexical TREE_BLOCK underneath 'current_block'.  */
static void prepend_lexical_block (tree current_block, tree new_block)
{
  BLOCK_CHAIN (new_block) = BLOCK_SUBBLOCKS (current_block);
  BLOCK_SUBBLOCKS (current_block) = new_block;
  BLOCK_SUPERCONTEXT (new_block) = current_block;
}

static unsigned int optimize_inline_calls_new (tree fn)
{
   copy_body_data id;
   basic_block bb;
   int last = n_basic_blocks_for_fn (cfun);
   bool inlined_p = false;

   /* Clear out ID.  */
   memset (&id, 0, sizeof (id));

   id.src_node = id.dst_node = cgraph_node::get (fn);
   n_debug("mtcsreplace.c optimize_inline_calls 00 %p\n",id.src_node);
   gcc_assert (id.dst_node->definition);
   id.dst_fn = fn;
   /* Or any functions that aren't finished yet.  */
   if (current_function_decl)
      id.dst_fn = current_function_decl;

   id.copy_decl = copy_decl_maybe_to_var;
   id.transform_call_graph_edges = CB_CGE_DUPLICATE;
   id.transform_new_cfg = false;
   id.transform_return_to_modify = true;
   id.transform_parameter = true;
   id.statements_to_fold = new hash_set<gimple *>;

   push_gimplify_context ();

   /* We make no attempts to keep dominance info up-to-date.  */
   free_dominance_info (CDI_DOMINATORS);
   free_dominance_info (CDI_POST_DOMINATORS);

   /* Register specific gimple functions.  */
   gimple_register_cfg_hooks ();
   n_debug("mtcsreplace.c optimize_inline_calls 11 %p\n",id.src_node);

   /* Reach the trees by walking over the CFG, and note the
   enclosing basic-blocks in the call edges.  */
   /* We walk the blocks going forward, because inlined function bodies
   will split id->current_basic_block, and the new blocks will
   follow it; we'll trudge through them, processing their CALL_EXPRs
   along the way.  */
   auto_bitmap to_purge;
   FOR_EACH_BB_FN (bb, cfun)
      inlined_p |= gimple_expand_calls_inline (bb, &id, to_purge);

   n_debug("mtcsreplace.c optimize_inline_calls 22 %p\n",id.src_node);

   pop_gimplify_context (NULL);

   if (flag_checking){
      struct cgraph_edge *e;

      id.dst_node->verify ();

      /* Double check that we inlined everything we are supposed to inline.  */
      for (e = id.dst_node->callees; e; e = e->next_callee)
         gcc_assert (e->inline_failed);
   }

   /* If we didn't inline into the function there is nothing to do.  */
   if (!inlined_p){
      delete id.statements_to_fold;
      return 0;
   }

   /* Fold queued statements.  */
   update_max_bb_count ();
   fold_marked_statements (last, id.statements_to_fold);
   delete id.statements_to_fold;

   /* Finally purge EH and abnormal edges from the call stmts we inlined.
   We need to do this after fold_marked_statements since that may walk
   the SSA use-def chain.  */
   unsigned i;
   bitmap_iterator bi;
   EXECUTE_IF_SET_IN_BITMAP (to_purge, 0, i, bi){
      basic_block bb = BASIC_BLOCK_FOR_FN (cfun, i);
      if (bb){
         gimple_purge_dead_eh_edges (bb);
         gimple_purge_dead_abnormal_call_edges (bb);
      }
   }

   gcc_assert (!id.debug_stmts.exists ());

   /* Renumber the lexical scoping (non-code) blocks consecutively.  */
   number_blocks (fn);

   delete_unreachable_blocks_update_callgraph (id.dst_node, false);
   id.dst_node->calls_comdat_local = id.dst_node->check_calls_comdat_local_p ();

   if (flag_checking)
      id.dst_node->verify ();

   /* It would be nice to check SSA/CFG/statement consistency here, but it is
   not possible yet - the IPA passes might make various functions to not
   throw and they don't care to proactively update local EH info.  This is
   done later in fixup_cfg pass that also execute the verification.  */
   return (TODO_update_ssa
      | TODO_cleanup_cfg
      | (gimple_in_ssa_p (cfun) ? TODO_remove_unused_locals : 0)
      | (gimple_in_ssa_p (cfun) ? TODO_update_address_taken : 0));
}

/**
 * 内联mii 函数
 */
nuint mtcs_replace_inline_intrinsic(MtcsReplace *self,struct cgraph_node *newNode,tree fndecl)
{
   gcc_assert(cfun==DECL_STRUCT_FUNCTION (newNode->decl) && newNode->decl==fndecl);
   //printf("mtcs_replace_inline_intrinsic newNode:%p current_function:%p %p\n",newNode,current_function_decl,fndecl);
   optimize_inline_calls_new(fndecl);
   return 0;
}


MtcsReplace *mtcs_replace_new(MtcsMode *mtcsMode)
{
   MtcsReplace *self = n_slice_alloc0 (sizeof(MtcsReplace));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsReplaceInit(self);
   return self;
}
