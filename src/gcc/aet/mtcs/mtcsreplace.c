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

#include "mtcsreplace.h"
#include "mtcstarget.h"
#include "mtcscompile.h"
#include "../aetprinttree.h"
#include "../aetprintgimple.h"
#include "../mtcsinfo.h"
#include "../aetmicro.h"


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
//   printf("this is hd func\n);
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

MtcsReplace *mtcs_replace_new(MtcsMode *mtcsMode)
{
   MtcsReplace *self = n_slice_alloc0 (sizeof(MtcsReplace));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsReplaceInit(self);
   return self;
}
