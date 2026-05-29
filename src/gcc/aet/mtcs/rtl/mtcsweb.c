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
 * base on web.cc
 */


/* Simple optimization pass that splits independent uses of each pseudo,
   increasing effectiveness of other optimizations.  The optimization can
   serve as an example of use for the dataflow module.

   TODO
    - We may use profile information and ignore infrequent use for the
      purpose of web unifying, inserting the compensation code later to
      implement full induction variable expansion for loops (currently
      we expand only if the induction variable is dead afterward, which
      is often the case).  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "rtl.h"
#include "df.h"
#include "insn-config.h"
#include "recog.h"

#include "mtcsweb.h"
#include "../mtcstarget.h"
#include "../mtcsdfcore.h"
#include "../mtcsdfproblems.h"

/* Find the root of unionfind tree (the representative of set).  */
//web.cc已实现
//web_entry_base *web_entry_base::unionfind_root ()
//{
//  web_entry_base *element = this, *element1 = this, *element2;
//
//  while (element->pred ())
//    element = element->pred ();
//  while (element1->pred ())
//    {
//      element2 = element1->pred ();
//      element1->set_pred (element);
//      element1 = element2;
//    }
//  return element;
//}

/* Union sets.
   Return true if FIRST and SECOND points to the same web entry structure and
   nothing is done.  Otherwise, return false.  */
//改为static
static bool mtcs_unionfind_union (web_entry_base *first, web_entry_base *second)
{
  first = first->unionfind_root ();
  second = second->unionfind_root ();
  if (first == second)
    return true;
  second->set_pred (first);
  return false;
}

struct web_entry : public web_entry_base
{
 private:
  rtx reg_pvt;

 public:
  rtx reg () { return reg_pvt; }
  void set_reg (rtx r) { reg_pvt = r; }
};

/* For INSN, union all defs and uses that are linked by match_dup.
   FUN is the function that does the union.  */

static void union_match_dups (MtcsWeb *self,rtx_insn *insn, web_entry *def_entry, web_entry *use_entry,
        bool (*fun) (web_entry_base *, web_entry_base *))
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   int firstPseudoRegister= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);


   struct df_insn_info *insn_info = DF_INSN_INFO_GET (insn);
   df_ref use_link = DF_INSN_INFO_USES (insn_info);
   df_ref def_link = DF_INSN_INFO_DEFS (insn_info);
   struct web_entry *dup_entry;
   int i;

   mtcs_recog_extract_insn/*!extract_insn*/(mtcsRecog,insn);

   for (i = 0; i < mtcsRecog->recog_data.n_dups; i++){
      int op = mtcsRecog->recog_data.dup_num[i];
      enum mtcs_op_type type = mtcsRecog->recog_data.operand_type[op];
      df_ref ref, dupref;
      struct web_entry *entry;

      dup_entry = use_entry;
      for (dupref = use_link; dupref; dupref = DF_REF_NEXT_LOC (dupref))
         if (DF_REF_LOC (dupref) == mtcsRecog->recog_data.dup_loc[i])
            break;

      if (dupref == NULL && type == OP_INOUT){
         dup_entry = def_entry;
         for (dupref = def_link; dupref; dupref = DF_REF_NEXT_LOC (dupref))
            if (DF_REF_LOC (dupref) == mtcsRecog->recog_data.dup_loc[i])
               break;
      }
      /* ??? *DUPREF can still be zero, because when an operand matches
      a memory, DF_REF_LOC (use_link[n]) points to the register part
      of the address, whereas mtcsRecog->recog_data.dup_loc[m] points to the
      entire memory ref, thus we fail to find the duplicate entry,
      even though it is there.
      Example: i686-pc-linux-gnu gcc.c-torture/compile/950607-1.c
      -O3 -fomit-frame-pointer -funroll-loops  */
      if (dupref == NULL || DF_REF_REGNO (dupref) < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
         continue;

      ref = type == OP_IN ? use_link : def_link;
      entry = type == OP_IN ? use_entry : def_entry;
      for (; ref; ref = DF_REF_NEXT_LOC (ref)){
         rtx *l = DF_REF_LOC (ref);
         if (l == mtcsRecog->recog_data.operand_loc[op])
            break;
         if (l && DF_REF_REAL_LOC (ref) == mtcsRecog->recog_data.operand_loc[op])
            break;
      }

      if (!ref && type == OP_INOUT){
         entry = use_entry;
         for (ref = use_link; ref; ref = DF_REF_NEXT_LOC (ref)){
            rtx *l = DF_REF_LOC (ref);
            if (l == mtcsRecog->recog_data.operand_loc[op])
               break;
            if (l && DF_REF_REAL_LOC (ref) == mtcsRecog->recog_data.operand_loc[op])
               break;
         }
      }

      gcc_assert (ref);
      (*fun) (dup_entry + DF_REF_ID (dupref), entry + DF_REF_ID (ref));
   }
}

/* For each use, all possible defs reaching it must come in the same
   register, union them.
   FUN is the function that does the union.

   In USED, we keep the DF_REF_ID of the first uninitialized uses of a
   register, so that all uninitialized uses of the register can be
   combined into a single web.  We actually offset it by 2, because
   the values 0 and 1 are reserved for use by entry_register.  */
//从公共方法改为 static
static void mtcs_union_defs (MtcsWeb *self,df_ref use, web_entry *def_entry,
       unsigned int *used, web_entry *use_entry,
       bool (*fun) (web_entry_base *, web_entry_base *))
{
   struct df_insn_info *insn_info = DF_REF_INSN_INFO (use);
   struct df_link *link = DF_REF_CHAIN (use);
   rtx set;

   if (insn_info){
      df_ref eq_use;

      set = single_set (insn_info->insn);
      FOR_EACH_INSN_INFO_EQ_USE (eq_use, insn_info)
         if (use != eq_use   && DF_REF_REAL_REG (use) == DF_REF_REAL_REG (eq_use))
            (*fun) (use_entry + DF_REF_ID (use), use_entry + DF_REF_ID (eq_use));
   }else
      set = NULL;

   /* Union all occurrences of the same register in reg notes.  */

   /* Recognize trivial noop moves and attempt to keep them as noop.  */

   if (set && SET_SRC (set) == DF_REF_REG (use) && SET_SRC (set) == SET_DEST (set)){
      df_ref def;

      FOR_EACH_INSN_INFO_DEF (def, insn_info)
         if (DF_REF_REAL_REG (use) == DF_REF_REAL_REG (def))
            (*fun) (use_entry + DF_REF_ID (use), def_entry + DF_REF_ID (def));
   }

   /* UD chains of uninitialized REGs are empty.  Keeping all uses of
   the same uninitialized REG in a single web is not necessary for
   correctness, since the uses are undefined, but it's wasteful to
   allocate one register or slot for each reference.  Furthermore,
   creating new pseudos for uninitialized references in debug insns
   (see PR 42631) causes -fcompare-debug failures.  We record the
   number of the first uninitialized reference we found, and merge
   with it any other uninitialized references to the same
   register.  */
   if (!link){
      int regno = REGNO (DF_REF_REAL_REG (use));
      if (used[regno])
         (*fun) (use_entry + DF_REF_ID (use), use_entry + used[regno] - 2);
      else
         used[regno] = DF_REF_ID (use) + 2;
   }

   while (link){
      (*fun) (use_entry + DF_REF_ID (use), def_entry + DF_REF_ID (link->ref));
      link = link->next;
   }

   /* A READ_WRITE use requires the corresponding def to be in the same
   register.  Find it and union.  */
   if (DF_REF_FLAGS (use) & DF_REF_READ_WRITE)
      if (insn_info){
         df_ref def;

         FOR_EACH_INSN_INFO_DEF (def, insn_info)
            if (DF_REF_REAL_REG (use) == DF_REF_REAL_REG (def))
               (*fun) (use_entry + DF_REF_ID (use), def_entry + DF_REF_ID (def));
      }
}

/* Find the corresponding register for the given entry.  */
static rtx entry_register (MtcsWeb *self,web_entry *entry, df_ref ref, unsigned int *used)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   web_entry *root;
   rtx reg, newreg;

   /* Find the corresponding web and see if it has been visited.  */
   root = (web_entry *)entry->unionfind_root ();
   if (root->reg ())
      return root->reg ();

   /* We are seeing this web for the first time, do the assignment.  */
   reg = DF_REF_REAL_REG (ref);

   /* In case the original register is already assigned, generate new
   one.  Since we use USED to merge uninitialized refs into a single
   web, we might found an element to be nonzero without our having
   used it.  Test for 1, because union_defs saves it for our use,
   and there won't be any use for the other values when we get to
   this point.  */
   if (used[REGNO (reg)] != 1)
      newreg = reg, used[REGNO (reg)] = 1;
   else{
      newreg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (reg));
      REG_USERVAR_P (newreg) = REG_USERVAR_P (reg);
      REG_POINTER (newreg) = REG_POINTER (reg);
      REG_ATTRS (newreg) = REG_ATTRS (reg);
      if (dump_file)
         fprintf (dump_file, "Web oldreg=%i newreg=%i\n", REGNO (reg),
      REGNO (newreg));
   }

   root->set_reg (newreg);
   return newreg;
}

/* Replace the reference by REG.  */
static void replace_ref (MtcsWeb *self,df_ref ref, rtx reg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);

   rtx oldreg = DF_REF_REAL_REG (ref);
   rtx *loc = DF_REF_REAL_LOC (ref);
   unsigned int uid = DF_REF_INSN_UID (ref);
   if (oldreg == reg)
      return;
   if (dump_file)
      fprintf (dump_file, "Updating insn %i (%i->%i)\n", uid, REGNO (oldreg), REGNO (reg));
   *loc = reg;
   mtcs_dfscan_df_insn_rescan/*!df_insn_rescan*/(mtcsDfscan,DF_REF_INSN (ref));
}

//
//namespace {
//
//const pass_data pass_data_web =
//{
//  RTL_PASS, /* type */
//  "web", /* name */
//  OPTGROUP_NONE, /* optinfo_flags */
//  TV_WEB, /* tv_id */
//  0, /* properties_required */
//  0, /* properties_provided */
//  0, /* properties_destroyed */
//  0, /* todo_flags_start */
//  TODO_df_finish, /* todo_flags_finish */
//};
//
//class pass_web : public rtl_opt_pass
//{
//public:
//  pass_web (gcc::context *ctxt)
//    : rtl_opt_pass (pass_data_web, ctxt)
//  {}
//
//  /* opt_pass methods: */
//  bool gate (function *) final override { return (optimize > 0 && flag_web); }
//  unsigned int execute (function *) final override;
//
//}; // class pass_web

static unsigned int web_pass_execute (MtcsWeb *self,function *fun)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsDfproblems *mtcsDfproblems=mtcs_target_get_dfproblems(mtcsTarget);

   int firstPseudoRegister= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   web_entry *def_entry;
   web_entry *use_entry;
   unsigned int max = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);

   unsigned int *used;
   basic_block bb;
   unsigned int uses_num = 0;
   rtx_insn *insn;

   mtcs_dfcore_df_set_flags/*!df_set_flags*/(mtcsDfcore,DF_NO_HARD_REGS + DF_EQ_NOTES);
   mtcs_dfcore_df_set_flags/*!df_set_flags*/(mtcsDfcore,DF_RD_PRUNE_DEAD_DEFS);
   mtcs_dfproblems_df_chain_add_problem/*!df_chain_add_problem*/(mtcsDfproblems,DF_UD_CHAIN);
   mtcs_dfcore_df_analyze/*!df_analyze*/(mtcsDfcore);
   mtcs_dfcore_df_set_flags/*!df_set_flags*/(mtcsDfcore,DF_DEFER_INSN_RESCAN);

   /* Assign ids to the uses.  */
   FOR_ALL_BB_FN (bb, fun)
      FOR_BB_INSNS (bb, insn){
         if (NONDEBUG_INSN_P (insn)){
            struct df_insn_info *insn_info = DF_INSN_INFO_GET (insn);
            df_ref use;
            FOR_EACH_INSN_INFO_USE (use, insn_info)
               if (DF_REF_REGNO (use) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
                  DF_REF_ID (use) = uses_num++;
            FOR_EACH_INSN_INFO_EQ_USE (use, insn_info)
               if (DF_REF_REGNO (use) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
                  DF_REF_ID (use) = uses_num++;
         }
      }

   /* Record the number of uses and defs at the beginning of the optimization.  */
   def_entry = XCNEWVEC (web_entry, DF_DEFS_TABLE_SIZE ());
   used = XCNEWVEC (unsigned, max);
   use_entry = XCNEWVEC (web_entry, uses_num);

   /* Produce the web.  */
   FOR_ALL_BB_FN (bb, fun)
      FOR_BB_INSNS (bb, insn)
         if (NONDEBUG_INSN_P (insn)){
            struct df_insn_info *insn_info = DF_INSN_INFO_GET (insn);
            df_ref use;
            union_match_dups(self,insn, def_entry, use_entry, mtcs_unionfind_union);
            FOR_EACH_INSN_INFO_USE (use, insn_info)
               if (DF_REF_REGNO (use) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
                  mtcs_union_defs(self,use, def_entry, used, use_entry, mtcs_unionfind_union);
            FOR_EACH_INSN_INFO_EQ_USE (use, insn_info)
               if (DF_REF_REGNO (use) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
                  mtcs_union_defs(self,use, def_entry, used, use_entry, mtcs_unionfind_union);
         }

   /* Update the instruction stream, allocating new registers for split pseudos
   in progress.  */
   FOR_ALL_BB_FN (bb, fun)
      FOR_BB_INSNS (bb, insn)
         if (NONDEBUG_INSN_P (insn)
         /* Ignore naked clobber.  For example, reg 134 in the second insn
         of the following sequence will not be replaced.

         (insn (clobber (reg:SI 134)))

         (insn (set (reg:SI 0 r0) (reg:SI 134)))

         Thus the later passes can optimize them away.  */
         && GET_CODE (PATTERN (insn)) != CLOBBER){
            struct df_insn_info *insn_info = DF_INSN_INFO_GET (insn);
            df_ref def, use;
            FOR_EACH_INSN_INFO_USE (use, insn_info)
               if (DF_REF_REGNO (use) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
                  replace_ref(self,use, entry_register(self,use_entry + DF_REF_ID (use),use, used));
            FOR_EACH_INSN_INFO_EQ_USE (use, insn_info)
               if (DF_REF_REGNO (use) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
                  replace_ref(self,use, entry_register(self,use_entry + DF_REF_ID (use),use, used));
            FOR_EACH_INSN_INFO_DEF (def, insn_info)
               if (DF_REF_REGNO (def) >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
                  replace_ref(self,def, entry_register(self,def_entry + DF_REF_ID (def),def, used));
         }

   free (def_entry);
   free (use_entry);
   free (used);
   return 0;
}

//原型 NEXT_PASS (pass_rtl_loop_done, 1); RTL_PASS loop-init.cc loop2_done n 无条件执行 loop_optimizer_finalize  cleanup_cfg (0);
static nuint web_execute_cb(MtcsPass *mtcsPass,function *fun)
{
   return web_pass_execute((MtcsWeb *)mtcsPass,fun);
}

static nboolean web_gate_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   return (mtcsOptionsItem->x_optimize>0  && mtcsOptionsItem->x_flag_web);
}

static void mtcsWebInit(MtcsWeb *self)
{
   MtcsPass *mtcsPass= (MtcsPass *)self;
   mtcsPass->execute =web_execute_cb;
   mtcsPass->gate =web_gate_cb;

   mtcs_pass_set_properties(mtcsPass,
      0,/* properties_required */
      0, /* properties_provided */
      0 /* properties_destroyed */);
   mtcs_pass_set_todo_flags(mtcsPass,
      0, /* todo_flags_start */
      TODO_df_finish /*todo_flags_finish */);
}

MtcsWeb *mtcs_web_new(MtcsMode *mtcsMode)
{
   MtcsWeb *self = n_slice_alloc0 (sizeof(MtcsWeb));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"web");
   mtcsWebInit(self);
   return self;
}

