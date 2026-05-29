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
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "ira.h"
#include "ira-int.h"
#include "diagnostic-core.h"
#include "cfgrtl.h"
#include "cfgbuild.h"
#include "cfgcleanup.h"
#include "expr.h"
#include "tree-pass.h"
#include "output.h"
#include "reload.h"
#include "cfgloop.h"
#include "lra.h"
#include "dce.h"
#include "dbgcnt.h"
#include "rtl-iter.h"
#include "shrink-wrap.h"
#include "print-rtl.h"

#include "mtcsiraallocno.h"
#include "mtcsira.h"
#include "mtcsiraint.h"
#include "mtcsiracommon.h"
#include "../mtcstarget.h"

/**
 * 创建 Allocno时target已经完成了各个组件的创建，所以在初始化的地方可以引用组件。
 * 原型 ira_create_allocno ira-build.cc
 */
static void mtcsIraAllocnoInit(MtcsIraAllocno *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);

   self->cap = NULL;
   self->cap_member = NULL;
   // self->num = ira_allocnos_num; //由mtcsbuild 赋值
   //bitmap_set_bit (loop_tree_node->all_allocnos, self->num); //由mtcsbuild 赋值
   self->nrefs = 0;
   self->freq = 0;
   self->might_conflict_with_parent_p = false;
   mtcs_ira_allocno_set_register_filter (self, 0);
   self->hard_regno = -1;
   self->call_freq = 0;
   self->calls_crossed_num = 0;
   self->cheap_calls_crossed_num = 0;
   self->crossed_calls_abis = 0;
   mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&self->crossed_calls_clobbered_regs);
   if(mtcs_config_ifdef(mtcsConfig,MTCS_STACK_REGS)){
   //#ifdef STACK_REGS
      self->no_stack_reg_p = false;
      self->total_no_stack_reg_p = false;
   // #endif
   }
   self->dont_reassign_p = false;
   self->bad_spill_p = false;
   self->assigned_p = false;
   self->mode = (self->regno < 0 ? VOIDmode : mtcs_rtl_data_get_pseudo_regno_mode/*!PSEUDO_REGNO_MODE*/(mtcsRtlData,self->regno));
   self->wmode= self->mode;
   self->allocno_prefs = NULL;
   self->allocno_copies = NULL;
   self->hard_reg_costs = NULL;
   self->conflict_hard_reg_costs = NULL;
   self->updated_hard_reg_costs = NULL;
   self->updated_conflict_hard_reg_costs = NULL;
   self->aclass = NO_REGS;
   self->updated_class_cost = 0;
   self->class_cost = 0;
   self->memory_cost = 0;
   self->updated_memory_cost = 0;
   self->excess_pressure_points_num = 0;
   self->num_objects = 0;
   self->add_data = NULL;
}

//原型 ALLOCNO_SET_REGISTER_FILTERS ira-int.h
void mtcs_ira_allocno_set_register_filter(MtcsIraAllocno *self,int filter)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   if(mtcs_config_ifdef(mtcsConfig,MTCS_NUM_REGISTER_FILTERS))
      self->register_filters =0 ;
   else
      n_error("禁止设置NUM_REGISTER_FILTERS \n");
}

/* Update hard register conflict information for all objects associated with
   A to include the regs in SET.  */
//原型 ior_hard_reg_conflicts ira-int.h ira-build.cc
void mtcs_ira_allocno_ior_hard_reg_conflicts (MtcsIraAllocno *self, HardRegSet set)
{
   MtcsIraAllocnoObjectIterator i;
   MtcsIraObject *obj;

   MTCS_FOR_EACH_ALLOCNO_OBJECT (self, obj, i){
      obj ->conflict_hard_regs |=set;
      obj->total_conflict_hard_regs |= set;
   }
}

/* This recursive function outputs allocno A and if it is a cap the
   function outputs its members.  */
//原型 ira_print_expanded_allocno ira-int.h ira-build.cc
void mtcs_ira_allocno_print_expanded_allocno (MtcsIraAllocno *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   basic_block bb;
   FILE *dumpFile = mtcsIraGlobal->ira_dump_file;
   fprintf (dumpFile, " a%d(r%d", self->num, self->regno);
   if ((bb = self->loop_tree_node->bb) != NULL)
      fprintf (dumpFile, ",b%d", bb->index);
   else
      fprintf (dumpFile, ",l%d", self->loop_tree_node->loop_num);
   if (self->cap_member != NULL){
      fprintf (dumpFile, ":");
      mtcs_ira_allocno_print_expanded_allocno/*!ira_print_expanded_allocno*/(self->cap_member);
   }
   fprintf (dumpFile, ")");
}

/* Return the set of registers that would need a caller save if allocno A
   overlapped them.  */
//原型 ira_need_caller_save_regs ira-int.h
HardRegSet mtcs_ira_allocno_need_caller_save_regs (MtcsIraAllocno *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFuncAbi *mtcsFuncAbi=mtcs_target_get_func_abi(mtcsTarget);

  return mtcs_func_abi_call_clobbers_in_region/*!call_clobbers_in_region*/(mtcsFuncAbi,
        self->crossed_calls_abis, self->crossed_calls_clobbered_regs,self->mode);
}

/* Print info about copies involving allocno A into file F.  */
//原型 static void print_allocno_copies (FILE *f, ira_allocno_t a) ira-build.cc
void mtcs_ira_allocno_print_allocno_copies (MtcsIraAllocno *self,FILE *f)
{
   MtcsIraAllocno *another_a;
   MtcsIraAllocnoCopy *cp, *next_cp;

   fprintf (f, " a%d(r%d):", self->num, self->regno);
   for (cp = self->allocno_copies; cp != NULL; cp = next_cp){
      if (cp->first == self){
         next_cp = cp->next_first_allocno_copy;
         another_a = cp->second;
      }else if (cp->second == self){
         next_cp = cp->next_second_allocno_copy;
         another_a = cp->first;
      }else
         gcc_unreachable ();
      fprintf (f, " cp%d:a%d(r%d)@%d", cp->num,another_a->num, another_a->regno, cp->freq);
   }
   fprintf (f, "\n");
}

//原型 debug ira-int.h ira-build.cc
DEBUG_FUNCTION void mtcs_ira_allocno_debug (MtcsIraAllocno *self)
{
  if (self)
     mtcs_ira_allocno_print_allocno_copies/*!debug*/(self,stderr);
  else
    fprintf (stderr, "<nil>\n");
}

/* Print info about copies involving allocno A into stderr.  */
//原型 ira_debug_allocno_copies ira-int.h ira-build.cc
void mtcs_ira_allocno_debug_allocno_copies (MtcsIraAllocno *self)
{
   mtcs_ira_allocno_print_allocno_copies (self,stderr);
}

MtcsIraAllocnoPref *mtcs_ira_allocno_pref_new()
{
   return    (MtcsIraAllocnoPref *) n_slice_alloc0 (sizeof(MtcsIraAllocnoPref));
}

/* Return pref for A and HARD_REGNO if any.  */
//原型 static ira_pref_t find_allocno_pref (ira_allocno_t a, int hard_regno) ira-build.cc
MtcsIraAllocnoPref *mtcs_ira_allocno_find_allocno_pref (MtcsIraAllocno *self, int hard_regno)
{
   MtcsIraAllocnoPref *pref;

   for (pref = self->allocno_prefs; pref != NULL; pref = pref->next_pref)
      if (pref->allocno == self && pref->hard_regno == hard_regno)
         return pref;
   return NULL;
}

/* Print info about prefs involving allocno A into file F.  */
//原型 static void print_allocno_prefs (FILE *f, ira_allocno_t a) ira-build.cc
static void print_allocno_prefs (MtcsIraAllocno *self,FILE *f)
{
   MtcsIraAllocnoPref *pref;

   fprintf (f, " a%d(r%d):", self->num, self->regno);
      for (pref = self->allocno_prefs; pref != NULL; pref = pref->next_pref)
   fprintf (f, " pref%d:hr%d@%d", pref->num, pref->hard_regno, pref->freq);
   fprintf (f, "\n");
}

/* Print info about prefs involving allocno A into stderr.  */
//原型 ira_debug_allocno_prefs ira-int.h ira-build.cc
void mtcs_ira_allocno_debug_allocno_prefs (MtcsIraAllocno *self)
{
  print_allocno_prefs (self,stderr);
}

/* Find the allocno that corresponds to A at a level one higher up in the
   loop tree.  Returns NULL if A is a cap, or if it has no parent.  */
//原型 extern ira_allocno_t ira_parent_allocno (ira_allocno_t); ira-int.h ira-build.cc
MtcsIraAllocno *mtcs_ira_allocno_parent_allocno (MtcsIraAllocno * self)
{
   MtcsIraLoopTreeNode * parent;
   if (self->cap != NULL)
      return NULL;
   parent = self->loop_tree_node->parent;
   if (parent == NULL)
      return NULL;
   return parent->regno_allocno_map[self->regno];
}

/* Find the allocno that corresponds to A at a level one higher up in the
   loop tree.  If ALLOCNO_CAP is set for A, return that.  */
//原型 ira_allocno_t ira_parent_or_cap_allocno (ira_allocno_t);
MtcsIraAllocno * mtcs_ira_allocno_parent_or_cap_allocno (MtcsIraAllocno * self)
{
   if (self->cap != NULL)
      return self->cap;
   return mtcs_ira_allocno_parent_allocno/*!ira_parent_allocno*/(self);
}

/* Abbreviation for frequent emit data access.  */
//原型 allocno_emit_reg ira-int.h
rtx mtcs_ira_allocno_emit_reg (MtcsIraAllocno *self)
{
   MtcsIraEmitData *emitData=(MtcsIraEmitData *)self->add_data;
   return emitData->reg;
}

/* Return true if we would need to save allocno A around a call if we
   assigned hard register REGNO.  */
//原型 ira_need_caller_save_p ira-int.h
bool mtcs_ira_allocno_need_caller_save_p (MtcsIraAllocno *self, unsigned int regno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFuncAbi *mtcsFuncAbi=mtcs_target_get_func_abi(mtcsTarget);

   if (self->calls_crossed_num == 0)
      return false;
   return mtcs_func_abi_call_clobbered_in_region_p/*!call_clobbered_in_region_p*/(mtcsFuncAbi,self->crossed_calls_abis,
         self->crossed_calls_clobbered_regs,self->mode, regno);
}

/************************以下代码关于 MtcsIraAllocnoPref ********************/

/* Attach a pref PREF to the corresponding allocno.  */
//原型 static void add_allocno_pref_to_list (ira_pref_t pref) ira-build.cc
void mtcs_ira_allocno_add_allocno_pref_to_list (MtcsIraAllocnoPref *pref)
{
   MtcsIraAllocno *a = pref->allocno;
   pref->next_pref = a->allocno_prefs;
   a->allocno_prefs = pref;
}

/* Print info about PREF into file F.  */
//原型 static void print_pref (FILE *f, ira_pref_t pref) ira-build.cc
void mtcs_ira_allocno_print_pref (MtcsIraAllocnoPref *pref,FILE *f)
{
  fprintf (f, "  pref%d:a%d(r%d)<-hr%d@%d\n", pref->num,
     pref->allocno->num, pref->allocno->regno,pref->hard_regno, pref->freq);
}

/* Print info about PREF into stderr.  */
//原型 ira_debug_pref ira-int.h ira-build.cc
void mtcs_ira_allocno_debug_pref (MtcsIraAllocnoPref *pref)
{
   mtcs_ira_allocno_print_pref (pref,stderr);
}

//原型   pref_pool.remove (pref); ira-build.cc
void mtcs_ira_allocno_pref_free (MtcsIraAllocnoPref *pref)
{
   n_slice_free(MtcsIraAllocnoPref,pref);
}

/* Return copy connecting A1 and A2 and originated from INSN of
   LOOP_TREE_NODE if any.  */
//原型 static ira_copy_t find_allocno_copy (ira_allocno_t a1, i ... ira-build.cc
MtcsIraAllocnoCopy *mtcs_ira_allocno_find_allocno_copy (MtcsIraAllocno * self, MtcsIraAllocno *a2, rtx_insn *insn,
      MtcsIraLoopTreeNode *loop_tree_node)
{
   MtcsIraAllocnoCopy *cp, *next_cp;
   MtcsIraAllocno *another_a;

   for (cp = self->allocno_copies; cp != NULL; cp = next_cp){
      if (cp->first == self){
         next_cp = cp->next_first_allocno_copy;
         another_a = cp->second;
      }else if (cp->second == self){
         next_cp = cp->next_second_allocno_copy;
         another_a = cp->first;
      }else
         gcc_unreachable ();
      if (another_a == a2 && cp->insn == insn && cp->loop_tree_node == loop_tree_node)
         return cp;
   }
   return NULL;
}

/* Attach a copy CP to allocnos involved into the copy.  */
//原型 static void add_allocno_copy_to_list (MtcsIraAllocnoCopy *cp) ira-build.cc
void mtcs_ira_allocno_add_allocno_copy_to_list (MtcsIraAllocnoCopy *mtcsIraAllocnoCopy)
{
   MtcsIraAllocno *first = mtcsIraAllocnoCopy->first, *second = mtcsIraAllocnoCopy->second;

   mtcsIraAllocnoCopy->prev_first_allocno_copy = NULL;
   mtcsIraAllocnoCopy->prev_second_allocno_copy = NULL;
   mtcsIraAllocnoCopy->next_first_allocno_copy = first->allocno_copies;
   if (mtcsIraAllocnoCopy->next_first_allocno_copy != NULL){
      if (mtcsIraAllocnoCopy->next_first_allocno_copy->first == first)
         mtcsIraAllocnoCopy->next_first_allocno_copy->prev_first_allocno_copy = mtcsIraAllocnoCopy;
      else
         mtcsIraAllocnoCopy->next_first_allocno_copy->prev_second_allocno_copy = mtcsIraAllocnoCopy;
   }
   mtcsIraAllocnoCopy->next_second_allocno_copy = second->allocno_copies;
   if (mtcsIraAllocnoCopy->next_second_allocno_copy != NULL){
      if (mtcsIraAllocnoCopy->next_second_allocno_copy->second == second)
         mtcsIraAllocnoCopy->next_second_allocno_copy->prev_second_allocno_copy = mtcsIraAllocnoCopy;
      else
         mtcsIraAllocnoCopy->next_second_allocno_copy->prev_first_allocno_copy = mtcsIraAllocnoCopy;
   }
   first->allocno_copies = mtcsIraAllocnoCopy;
   second->allocno_copies = mtcsIraAllocnoCopy;
}

/* Make a copy CP a canonical copy where number of the
   first allocno is less than the second one.  */
//原型 static void swap_allocno_copy_ends_if_necessary (ira_copy_t cp) ira-build.cc
void mtcs_ira_allocno_swap_allocno_copy_ends_if_necessary (MtcsIraAllocnoCopy *mtcsIraAllocnoCopy)
{
   if (mtcsIraAllocnoCopy->first->num <= mtcsIraAllocnoCopy->second->num)
      return;

   std::swap (mtcsIraAllocnoCopy->first, mtcsIraAllocnoCopy->second);
   std::swap (mtcsIraAllocnoCopy->prev_first_allocno_copy, mtcsIraAllocnoCopy->prev_second_allocno_copy);
   std::swap (mtcsIraAllocnoCopy->next_first_allocno_copy, mtcsIraAllocnoCopy->next_second_allocno_copy);
}

/* Print info about copy CP into file F.  */
//原型 static void print_copy (MtcsIraAllocnoCopy *mtcsIraAllocnoCopy,FILE *f) ira-build.cc
void mtcs_ira_allocno_print_copy (MtcsIraAllocnoCopy *mtcsIraAllocnoCopy,FILE *f)
{
  fprintf (f, "  cp%d:a%d(r%d)<->a%d(r%d)@%d:%s\n", mtcsIraAllocnoCopy->num,
      mtcsIraAllocnoCopy->first->num, mtcsIraAllocnoCopy->first->regno,
      mtcsIraAllocnoCopy->second->num, mtcsIraAllocnoCopy->second->regno, mtcsIraAllocnoCopy->freq,
      mtcsIraAllocnoCopy->insn != NULL
      ? "move" : mtcsIraAllocnoCopy->constraint_p ? "constraint" : "shuffle");
}

//原型 DEBUG_FUNCTION void debug (ira_allocno_copy &ref) ira-int.h ira-build.cc
DEBUG_FUNCTION void mtcs_ira_alloc_copy_debug (MtcsIraAllocnoCopy *mtcsIraAllocnoCopy)
{
   mtcs_ira_allocno_print_copy/*!print_copy*/(mtcsIraAllocnoCopy,stderr);
}

//原型 DEBUG_FUNCTION void debug (ira_allocno_copy *ptr) ira-int.h ira-build.cc
DEBUG_FUNCTION void mtcs_ira_alloc_copy_debug_1(MtcsIraAllocnoCopy *mtcsIraAllocnoCopy)
{
  if (mtcsIraAllocnoCopy)
     mtcs_ira_allocno_print_copy/*!print_copy*/(mtcsIraAllocnoCopy,stderr);
  else
    fprintf (stderr, "<nil>\n");
}

/* Print info about copy CP into stderr.  */
//原型 ira_debug_copy ira-int.h ira-build.cc
void mtcs_ira_allocno_debug_copy (MtcsIraAllocnoCopy *mtcsIraAllocnoCopy)
{
   mtcs_ira_allocno_print_copy/*!print_copy*/(mtcsIraAllocnoCopy,stderr);
}

//原型   copy_pool.remove (cp);; ira-build.cc
void mtcs_ira_allocno_copy_free (MtcsIraAllocnoCopy *mtcsIraAllocnoCopy)
{
   n_slice_free(MtcsIraAllocnoCopy,mtcsIraAllocnoCopy);
}

MtcsIraAllocnoCopy *mtcs_ira_allocno_copy_new()
{
   return    (MtcsIraAllocnoCopy *) n_slice_alloc0 (sizeof(MtcsIraAllocnoCopy));
}

MtcsIraAllocno *mtcs_ira_allocno_new(MtcsMode *mtcsMode,int regno)
{
   MtcsIraAllocno *self = n_slice_alloc0 (sizeof(MtcsIraAllocno));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   self->regno = regno;
   mtcsIraAllocnoInit(self);
   return self;
}

