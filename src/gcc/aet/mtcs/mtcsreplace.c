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
#include "cgraph.h"
#include "diagnostic.h"
#include "varasm.h"
#include "stringpool.h"
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
#include "tree-dfa.h"
#include "cfghooks.h"
#include "fold-const.h"

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
 * 内联mii函数到调用点
 * 核心底层：手动将 Callee（被调用者）的 GIMPLE 体嫁接到 Caller 的特定 Call 节点位置
 */
static void miiFuncInline(gimple_stmt_iterator *gsi_call, gcall *call_stmt, tree callee_decl)
{
   struct function *caller_fn = cfun;
   cgraph_node *callee_node = cgraph_node::get(callee_decl);
   tree caller_decl =  gimple_call_fndecl (call_stmt);
   struct function *callee_fn = DECL_STRUCT_FUNCTION(callee_decl);

   // 如果目标函数根本没有函数体，直接退出
   if (!callee_fn )
      return;
   basic_block bb_call = gsi_bb(*gsi_call);

   // 1.物理切分控制流
   // 将包含该 call 的基本块从中间一劈为二。call 之前的留在 bb_call，之后的分到 bb_post
   edge e_fallthru = split_block(bb_call, call_stmt);
   basic_block bb_post = e_fallthru->dest;

   // 2. 准备克隆上下文与参数映射
   // 利用 GCC 自带的 copy_body_data 机制来自动映射局部变量，防止两边局部符号冲突
   copy_body_data id;
   memset(&id, 0, sizeof(id));
   id.src_fn = callee_decl;
   id.dst_fn = caller_decl;
   id.src_cfun = callee_fn;
   // id.dst_cfun = caller_fn;
   id.transform_call_graph_edges = CB_CGE_DUPLICATE;
   // 关键：初始化局部变量符号映射哈希表
   id.decl_map = new hash_map<tree, tree>;
   id.copy_decl = copy_decl_maybe_to_var;

   // 绑定形参（Parameters）到实参（Arguments）的映射
   tree callee_parm = DECL_ARGUMENTS(callee_decl);
   unsigned int arg_i;
   for (arg_i = 0; arg_i < gimple_call_num_args(call_stmt) && callee_parm; arg_i++){
      tree arg_val = gimple_call_arg(call_stmt, arg_i);
      // 将 Callee 内使用的形参符号，强行重映射为 Caller 传进去的实参符号/SSA_NAME
      insert_decl_map(&id, callee_parm, arg_val);
      callee_parm = DECL_CHAIN(callee_parm);
   }

   // 3.【开始深度克隆 Callee 的各个 BB 语句并安全平铺嫁接】
   // 建立新基本块 bb_inlined，用来容纳克隆出来的指令体
   basic_block bb_inlined = create_basic_block(NULL, bb_call);
   add_bb_to_loop(bb_inlined, bb_call->loop_father); // 认祖归宗，防止缺失 loop_father 报错
   // 初始化目的地基本块的语句迭代器
   gimple_stmt_iterator gsi_dst = gsi_start_bb(bb_inlined);
   tree callee_retval = NULL_TREE;
   // 关键改变：遍历 Callee 函数体中所有的基本块
   basic_block callee_bb;
   FOR_EACH_BB_FN (callee_bb, callee_fn){
      gimple_stmt_iterator gsi_callee;
      // 遍历当前 Callee 基本块中的每一条 GIMPLE 语句
      for (gsi_callee = gsi_start_bb(callee_bb); !gsi_end_p(gsi_callee); gsi_next(&gsi_callee)){
         gimple *stmt = gsi_stmt(gsi_callee);

         // 特殊处理：如果是 Callee 的返回语句，不要盲目复制
         if (gimple_code(stmt) == GIMPLE_RETURN){
            greturn *ret_stmt = as_a<greturn *>(stmt);
            callee_retval = gimple_return_retval(ret_stmt);
            // 拿到其真正的返回表达式节点（可能是个 SSA_NAME 或局部变量）并映射到 Caller 的上下文中
            if (callee_retval)
               callee_retval = remap_decl(callee_retval, &id);
            // 遇到 return 代表这个基本块分支到头了（通常由于直线型函数只有单个返回，可直接跳过当前语句）
            continue;
         }
         // 克隆普通语句并映射其内部的所有操作数和局部符号
         gimple *cloned_stmt = gimple_copy(stmt);
         // 关键 强行将克隆语句的作用域块置为空
         // 这样可以彻底阻止 remap_gimple_stmt 去 id->decl_map 里查找旧 BLOCK，从而解除崩溃！
         gimple_set_block (cloned_stmt, NULL_TREE);
         gimple_seq new_stmts = remap_gimple_stmt(cloned_stmt, &id);
         if (gimple_seq_empty_p(new_stmts))
            continue;

         // (c) 通用强力洗白 遍历 new_stmts 里的每一条语句，不管它是 Assign 还是 Asm
         gimple_stmt_iterator gsi_seq;
         for (gsi_seq = gsi_start(new_stmts); !gsi_end_p(gsi_seq); gsi_next(&gsi_seq)){
            gimple *seq_stmt = gsi_stmt(gsi_seq);

            // 1.通用操作数洗白：利用 GCC 内核迭代器，扫描语句中暴露的所有普通树节点操作数（如 ASM 的 Output/Input 变量）
            unsigned int num_ops = gimple_num_ops(seq_stmt);
            for (unsigned int i = 0; i < num_ops; i++){
               tree *op_ptr = gimple_op_ptr(seq_stmt, i);
               if (op_ptr && *op_ptr){
                  tree val = *op_ptr;
                  // 剥离出真实的底层变量声明（支持普通变量、SSA_NAME、或者是带有 TREE_LIST 包装的 ASM 操作数）
                  tree var = val;
                  if (TREE_CODE(val) == TREE_LIST)
                     var = TREE_VALUE(val);
                  if (var && TREE_CODE(var) == SSA_NAME)
                     var = SSA_NAME_VAR(var);

                  // 如果抓到了漏网的局部变量，且其主人还是旧的 Callee，强行归化！
                  if (var && TREE_CODE(var) == VAR_DECL){
                     if (DECL_CONTEXT(var) != current_function_decl){
                        DECL_CONTEXT(var) = current_function_decl;
                        add_local_decl(cfun, var); // 强制合法合规登记入 Caller 栈帧
                     }
                  }
               }
            }

            // 2. 通用 SSA 变量洗白：扫描语句中所有的虚拟/真实 SSA 操作数，防患于未然
            ssa_op_iter op_iter;
            tree ssa_var;
            FOR_EACH_SSA_TREE_OPERAND (ssa_var, seq_stmt, op_iter, SSA_OP_ALL_USES | SSA_OP_ALL_DEFS){
               if (ssa_var && TREE_CODE(ssa_var) == SSA_NAME){
                  tree var_decl = SSA_NAME_VAR(ssa_var);
                  if (var_decl && TREE_CODE(var_decl) == VAR_DECL){
                     if (DECL_CONTEXT(var_decl) != current_function_decl){
                        DECL_CONTEXT(var_decl) = current_function_decl;
                        add_local_decl(cfun, var_decl);
                     }
                  }
               }
            }

            // 刷新语句
            update_stmt(seq_stmt);
         }

         gsi_insert_seq_after (&gsi_dst, new_stmts, GSI_NEW_STMT);
         update_stmt(new_stmts);
         // 刷新目的地迭代器到 bb_inlined 的最新尾部，确保下一条语句紧跟其后排列
         gsi_dst = gsi_last_bb(bb_inlined);
      }
   }

   // 4. 处理函数的返回值链接接力
   tree caller_lhs = gimple_call_lhs(call_stmt);
   if (caller_lhs && callee_retval){
      // 创建一条：caller_lhs = callee_retval; 的标准合规 GIMPLE 赋值语句
      gassign *ret_assign = gimple_build_assign(caller_lhs, callee_retval);
      gsi_dst = gsi_last_bb(bb_inlined);
      gsi_insert_after(&gsi_dst, ret_assign, GSI_NEW_STMT);
      update_stmt(ret_assign);
   }

   // 5. 重新物理组装控制流图（CFG）的边关系
   // (a) 摘除刚才 split_block 留下的过渡直通边
   remove_edge(e_fallthru);
   // (b) 重新接线：bb_call -> bb_inlined -> bb_post
   make_edge(bb_call, bb_inlined, EDGE_FALLTHRU);
   make_edge(bb_inlined, bb_post, EDGE_FALLTHRU);
   // 6. 彻底清除原始的旧 Call 节点并擦除内存占用
   gsi_remove(gsi_call, true);
   // 内存垃圾回收释放清理
   delete id.decl_map;
}

nuint mtcs_replace_replace_intrinsic(MtcsReplace *self,gimple_stmt_iterator *gsi,gcall *call)
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

      if(targetMiiFndecl){
         if(!cnode->definition){
            gimple_call_set_fndecl (call, targetMiiFndecl);
            cgraph_update_edges_for_call_stmt (call, targetMiiFndecl, call);
         }else{
            miiFuncInline(gsi, call , targetMiiFndecl);
         }
      }else{
         gimple_call_set_fndecl (call, miifndecl);
         cgraph_update_edges_for_call_stmt (call, miifndecl, call);
      }
      return 1;
   }else{
      const char *platName =  mtcs_target_get_platform_name(mtcsTarget);
      const char *isa = mtcs_target_get_isa_string(mtcsTarget);
      error_at(gimple_location(call),"mic类中的intrinsic 函数没有实现 平台:%s isa:%s",platName,isa);
   }
   return 0;
}


MtcsReplace *mtcs_replace_new(MtcsMode *mtcsMode)
{
   MtcsReplace *self = n_slice_alloc0 (sizeof(MtcsReplace));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsReplaceInit(self);
   return self;
}
