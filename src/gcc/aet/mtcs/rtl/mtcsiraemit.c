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
 * base on ira-emit.cc
 */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "rtl.h"
#include "tree.h"
#include "predict.h"
#include "df.h"
#include "insn-config.h"
#include "regs.h"
#include "memmodel.h"
#include "ira.h"
#include "ira-int.h"
#include "cfgrtl.h"
#include "cfgbuild.h"
#include "expr.h"
#include "reload.h"
#include "cfgloop.h"


#include "mtcsiraemit.h"
#include "mtcsira.h"
#include "mtcsiraint.h"
#include "mtcsirabuild.h"
#include "mtcsiralives.h"
#include "../mtcstarget.h"



/* Allocate and initiate the emit data.  */
//原型 ira_initiate_emit_data ira-int.h ira-emit.cc
void mtcs_ira_emit_initiate_emit_data (MtcsIraEmit *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   MtcsIraAllocno * a;
   MtcsIraAllocnoIterator ai;

   self->ira_allocno_emit_data  = (MtcsIraEmitData *) ira_allocate (mtcsIraBuild->ira_allocnos_num* sizeof (MtcsIraEmitData));
   memset (self->ira_allocno_emit_data, 0, mtcsIraBuild->ira_allocnos_num * sizeof (MtcsIraEmitData));
   MTCS_FOR_EACH_ALLOCNO(mtcsIraBuild,a, ai)
      a->add_data = self->ira_allocno_emit_data + a->num;
   self->new_allocno_emit_data_vec.create (50);

}

/* Free the emit data.  */
//原型 ira_finish_emit_data ira-int.h ira-emit.cc
void mtcs_ira_emit_finish_emit_data (MtcsIraEmit *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   void * p;
   MtcsIraAllocno * a;
   MtcsIraAllocnoIterator ai;

   ira_free (self->ira_allocno_emit_data);
   MTCS_FOR_EACH_ALLOCNO(mtcsIraBuild,a, ai)
      a->add_data = NULL;
   for (;self->new_allocno_emit_data_vec.length () != 0;){
      p = self->new_allocno_emit_data_vec.pop ();
      ira_free (p);
   }
   self->new_allocno_emit_data_vec.release ();
}

/* Create and return a new allocno with given REGNO and
   LOOP_TREE_NODE.  Allocate emit data for it.  */
static MtcsIraAllocno * create_new_allocno (MtcsIraEmit *self,int regno, MtcsIraLoopTreeNode * loop_tree_node)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   MtcsIraAllocno * a;
   a = mtcs_ira_build_create_allocno/*!ira_create_allocno*/(mtcsIraBuild,regno, false, loop_tree_node);
   a->add_data = ira_allocate (sizeof (MtcsIraEmitData));
   memset (a->add_data, 0, sizeof (MtcsIraEmitData));
   self->new_allocno_emit_data_vec.safe_push (a->add_data);
   return a;
}
/* The structure represents an allocno move.  Both allocnos have the
   same original regno but different allocation.  */
struct _EmitMove
{
  /* The allocnos involved in the move.  */
  MtcsIraAllocno *from, *to;
  /* The next move in the move sequence.  */
  EmitMove * next;
  /* Used for finding dependencies.  */
  bool visited_p;
  /* The size of the following array. */
  int deps_num;
  /* Moves on which given move depends on.  Dependency can be cyclic.
     It means we need a temporary to generates the moves.  Sequence
     A1->A2, B1->B2 where A1 and B2 are assigned to reg R1 and A2 and
     B1 are assigned to reg R2 is an example of the cyclic
     dependencies.  */
  EmitMove **deps;
  /* First insn generated for the move.  */
  rtx_insn *insn;
};

/* Return new move of allocnos TO and FROM.  */
static EmitMove * create_move (MtcsIraEmit *self,MtcsIraAllocno * to, MtcsIraAllocno * from)
{
   EmitMove * move;
   move = (EmitMove *) ira_allocate (sizeof (EmitMove));
   move->deps = NULL;
   move->deps_num = 0;
   move->to = to;
   move->from = from;
   move->next = NULL;
   move->insn = NULL;
   move->visited_p = false;
   return move;
}

/* Free memory for MOVE and its dependencies.  */
static void free_move (MtcsIraEmit *self,EmitMove * move)
{
   if (move->deps != NULL)
      ira_free (move->deps);
   ira_free (move);
}

/* Free memory for list of the moves given by its HEAD.  */
static void free_move_list (MtcsIraEmit *self,EmitMove * head)
{
   EmitMove * next;
   for (; head != NULL; head = next){
      next = head->next;
      free_move(self,head);
   }
}

/* Return TRUE if the move list LIST1 and LIST2 are equal (two
   moves are equal if they involve the same allocnos).  */
static bool eq_move_lists_p (MtcsIraEmit *self,EmitMove * list1, EmitMove * list2)
{
   for (; list1 != NULL && list2 != NULL; list1 = list1->next, list2 = list2->next)
      if (list1->from != list2->from || list1->to != list2->to)
         return false;
   return list1 == list2;
}

/* Print move list LIST into file F.  */
static void print_move_list (MtcsIraEmit *self,FILE *f, EmitMove * list)
{
   for (; list != NULL; list = list->next)
      fprintf (f, " a%dr%d->a%dr%d", list->from->num, list->from->regno, list->to->num, list->to->regno);
   fprintf (f, "\n");
}

extern void ira_debug_move_list (EmitMove * list);

/* Print move list LIST into stderr.  */
void ira_debug_move_list (MtcsIraEmit *self,EmitMove * list)
{
  print_move_list(self,stderr, list);
}

/* This recursive function changes pseudo-registers in *LOC if it is
   necessary.  The function returns TRUE if a change was done.  */
static bool change_regs (MtcsIraEmit *self,rtx *loc)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);
   int i, regno, result = false;
   const char *fmt;
   enum rtx_code code;
   rtx reg;

   if (*loc == NULL_RTX)
      return false;
   code = GET_CODE (*loc);
   if (code == REG){
      regno = REGNO (*loc);
      if (regno < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
         return false;
      if (regno >= self->max_regno_before_changing)
         /* It is a shared register which was changed already.  */
         return false;
      if (mtcsIraBuild->ira_curr_regno_allocno_map[regno] == NULL)
         return false;
      reg = mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(mtcsIraBuild->ira_curr_regno_allocno_map[regno]);
      if (reg == *loc)
         return false;
      *loc = reg;
      return true;
   }

   fmt = GET_RTX_FORMAT (code);
   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--){
      if (fmt[i] == 'e')
         result = change_regs(self,&XEXP (*loc, i)) || result;
      else if (fmt[i] == 'E'){
         int j;

         for (j = XVECLEN (*loc, i) - 1; j >= 0; j--)
            result = change_regs(self,&XVECEXP (*loc, i, j)) || result;
      }
   }
   return result;
}

static bool change_regs_in_insn (MtcsIraEmit *self,rtx_insn **insn_ptr)
{
   rtx rtx = *insn_ptr;
   bool result = change_regs(self,&rtx);
   *insn_ptr = as_a <rtx_insn *> (rtx);
   return result;
}

/* Attach MOVE to the edge E.  The move is attached to the head of the
   list if HEAD_P is TRUE.  */
static void add_to_edge_list (MtcsIraEmit *self,edge e, EmitMove * move, bool head_p)
{
   EmitMove * last;

   if (head_p || e->aux == NULL){
      move->next = (EmitMove *) e->aux;
      e->aux = move;
   }else{
      for (last = (EmitMove *) e->aux; last->next != NULL; last = last->next)
         ;
      last->next = move;
      move->next = NULL;
   }
}

/* Create and return new pseudo-register with the same attributes as
   ORIGINAL_REG.  */
//原型 ira_create_new_reg ira-int.h ira-emit.cc
rtx mtcs_ira_emit_create_new_reg (MtcsIraEmit *self,rtx original_reg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   rtx new_reg;

   new_reg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (original_reg));
   ORIGINAL_REGNO (new_reg) = ORIGINAL_REGNO (original_reg);
   REG_USERVAR_P (new_reg) = REG_USERVAR_P (original_reg);
   REG_POINTER (new_reg) = REG_POINTER (original_reg);
   REG_ATTRS (new_reg) = REG_ATTRS (original_reg);
   if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
      fprintf (mtcsIraGlobal->ira_dump_file, "      Creating newreg=%i from oldreg=%i\n",
            REGNO (new_reg), REGNO (original_reg));
   mtcs_ira_expand_reg_equiv/*!ira_expand_reg_equiv*/(mtcsIra);
   return new_reg;
}

/* Return TRUE if loop given by SUBNODE inside the loop given by
   NODE.  */
static bool subloop_tree_node_p (MtcsIraEmit *self,MtcsIraLoopTreeNode * subnode, MtcsIraLoopTreeNode * node)
{
   for (; subnode != NULL; subnode = subnode->parent)
      if (subnode == node)
         return true;
   return false;
}

/* Set up member `reg' to REG for allocnos which has the same regno as
   ALLOCNO and which are inside the loop corresponding to ALLOCNO. */
static void set_allocno_reg (MtcsIraEmit *self,MtcsIraAllocno * allocno, rtx reg)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   int regno;
   MtcsIraAllocno * a;
   MtcsIraLoopTreeNode * node;

   node = allocno->loop_tree_node;
   for (a = mtcsIraBuild->ira_regno_allocno_map[allocno->regno]; a != NULL; a =a->next_regno_allocno)
      if (subloop_tree_node_p(self,a->loop_tree_node, node))
         ALLOCNO_EMIT_DATA (a)->reg = reg;
   for (a = allocno->cap; a != NULL; a = a->cap)
      ALLOCNO_EMIT_DATA (a)->reg = reg;
   regno = allocno->regno;
   for (a = allocno;;){
      if (a == NULL || (a = a->cap) == NULL){
         node = node->parent;
         if (node == NULL)
            break;
         a = node->regno_allocno_map[regno];
      }
      if (a == NULL)
         continue;
      if (ALLOCNO_EMIT_DATA (a)->child_renamed_p)
         break;
      ALLOCNO_EMIT_DATA (a)->child_renamed_p = true;
   }
}

/* Return true if there is an entry to given loop not from its parent
   (or grandparent) block.  For example, it is possible for two
   adjacent loops inside another loop.  */
static bool entered_from_non_parent_p (MtcsIraEmit *self,MtcsIraLoopTreeNode * loop_node)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);

   MtcsIraLoopTreeNode * bb_node, *src_loop_node,*parent;
   edge e;
   edge_iterator ei;

   for (bb_node = loop_node->children;bb_node != NULL; bb_node = bb_node->next)
      if (bb_node->bb != NULL){
         FOR_EACH_EDGE (e, ei, bb_node->bb->preds)
            if (e->src != ENTRY_BLOCK_PTR_FOR_FN (cfun) && (src_loop_node = MTCS_IRA_BB_NODE (e->src)->parent) != loop_node){
               for (parent = src_loop_node->parent; parent != NULL; parent = parent->parent)
                  if (parent == loop_node)
                     break;
               if (parent != NULL)
                  /* That is an exit from a nested loop -- skip it.  */
                  continue;
               for (parent = loop_node->parent; parent != NULL; parent = parent->parent)
                  if (src_loop_node == parent)
                     break;
               if (parent == NULL)
                  return true;
            }
      }
   return false;
}

/* Set up ENTERED_FROM_NON_PARENT_P for each loop region.  */
static void setup_entered_from_non_parent_p (MtcsIraEmit *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   unsigned int i;
   loop_p loop;

   ira_assert (current_loops != NULL);
   FOR_EACH_VEC_SAFE_ELT (get_loops (cfun), i, loop)
      if (ira_loop_nodes[i].regno_allocno_map != NULL)
         mtcsIraBuild->ira_loop_nodes[i]->entered_from_non_parent_p =
               entered_from_non_parent_p(self,mtcsIraBuild->ira_loop_nodes[i]);
}

/* Return TRUE if move of SRC_ALLOCNO (assigned to hard register) to
   DEST_ALLOCNO (assigned to memory) can be removed because it does
   not change value of the destination.  One possible reason for this
   is the situation when SRC_ALLOCNO is not modified in the
   corresponding loop.  */
static bool store_can_be_removed_p (MtcsIraEmit *self,MtcsIraAllocno * src_allocno, MtcsIraAllocno * dest_allocno)
{
   int regno, orig_regno;
   MtcsIraAllocno * a;
   MtcsIraLoopTreeNode * node;

   ira_assert (src_allocno->cap_member == NULL && dest_allocno->cap_member == NULL);
   orig_regno = src_allocno->regno;
   regno = REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(dest_allocno));
   for (node = src_allocno->loop_tree_node;node != NULL;node = node->parent){
      a = node->regno_allocno_map[orig_regno];
      ira_assert (a != NULL);
      if (REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(a)) == (unsigned) regno)
         /* We achieved the destination and everything is ok.  */
         return true;
      else if (bitmap_bit_p (node->modified_regnos, orig_regno))
         return false;
      else if (node->entered_from_non_parent_p)
         /* If there is a path from a destination loop block to the
         source loop header containing basic blocks of non-parents
         (grandparents) of the source loop, we should have checked
         modifications of the pseudo on this path too to decide
         about possibility to remove the store.  It could be done by
         solving a data-flow problem.  Unfortunately such global
         solution would complicate IR flattening.  Therefore we just
         prohibit removal of the store in such complicated case.  */
         return false;
   }
   /* It is actually a loop entry -- do not remove the store.  */
   return false;
}

/* Generate and attach moves to the edge E.  This looks at the final
   regnos of allocnos living on the edge with the same original regno
   to figure out when moves should be generated.  */
static void generate_edge_moves (MtcsIraEmit *self,edge e)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   MtcsIraLoopTreeNode * src_loop_node, *dest_loop_node;
   unsigned int regno;
   bitmap_iterator bi;
   MtcsIraAllocno *src_allocno, *dest_allocno, **src_map, **dest_map;
   EmitMove * move;
   bitmap regs_live_in_dest, regs_live_out_src;

   src_loop_node = MTCS_IRA_BB_NODE (e->src)->parent;
   dest_loop_node = MTCS_IRA_BB_NODE (e->dest)->parent;
   e->aux = NULL;
   if (src_loop_node == dest_loop_node)
      return;
   src_map = src_loop_node->regno_allocno_map;
   dest_map = dest_loop_node->regno_allocno_map;
   regs_live_in_dest = df_get_live_in (e->dest);
   regs_live_out_src = df_get_live_out (e->src);
   EXECUTE_IF_SET_IN_REG_SET (regs_live_in_dest,
   firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/, regno, bi)
   if (bitmap_bit_p (regs_live_out_src, regno)){
      src_allocno = src_map[regno];
      dest_allocno = dest_map[regno];
      if (REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(src_allocno))
            == REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(dest_allocno)))
         continue;
      /* Remove unnecessary stores at the region exit.  We should do
      this for readonly memory for sure and this is guaranteed by
      that we never generate moves on region borders (see
      checking in function change_loop).  */
      if (dest_allocno->hard_regno < 0 && src_allocno->hard_regno >= 0 && store_can_be_removed_p(self,src_allocno, dest_allocno)){
         MTCS_ALLOCNO_EMIT_DATA (src_allocno)->mem_optimized_dest = dest_allocno;
         MTCS_ALLOCNO_EMIT_DATA (dest_allocno)->mem_optimized_dest_p = true;
         if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
            fprintf (mtcsIraGlobal->ira_dump_file, "      Remove r%d:a%d->a%d(mem)\n",
               regno, src_allocno->num, dest_allocno->num);
         continue;
      }
      move = create_move(self,dest_allocno, src_allocno);
      add_to_edge_list(self,e, move, true);
   }
}


/* Change (if necessary) pseudo-registers inside loop given by loop
   tree node NODE.  */
static void  changeLoop_cb (MtcsIraLoopTreeNode * node,void *userData)
{
   MtcsIraEmit *self=(MtcsIraEmit *)userData;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsDfscan   *mtcsDfscan =mtcs_target_get_dfscan(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   bitmap_iterator bi;
   unsigned int i;
   int regno;
   bool used_p;
   MtcsIraAllocno * allocno, *parent_allocno, **map;
   rtx_insn *insn;
   rtx original_reg;
   enum reg_class aclass, pclass;
   MtcsIraLoopTreeNode * parent;

   if (node != mtcsIraBuild->ira_loop_tree_root){
      ira_assert (current_loops != NULL);

      if (node->bb != NULL){
         FOR_BB_INSNS (node->bb, insn)
            if (INSN_P (insn) && change_regs_in_insn(self,&insn)){
               mtcs_dfscan_df_insn_rescan/*!df_insn_rescan*/(mtcsDfscan,insn);
               mtcs_dfscan_df_notes_rescan/*!df_notes_rescan*/(mtcsDfscan,insn);
            }
         return;
      }

      if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file != NULL)
         fprintf (mtcsIraGlobal->ira_dump_file, "      Changing RTL for loop %d (header bb%d)\n",
               node->loop_num, node->loop->header->index);

      parent = mtcsIraBuild->ira_curr_loop_tree_node->parent;
      map = parent->regno_allocno_map;
      EXECUTE_IF_SET_IN_REG_SET (ira_curr_loop_tree_node->border_allocnos, 0, i, bi){
         allocno = mtcsIraBuild->ira_allocnos[i];
         regno = allocno->regno;
         aclass = allocno->aclass;
         pclass = mtcsIra->x_ira_pressure_class_translate[aclass];
         parent_allocno = map[regno];
         ira_assert (regno < mtcsIra->ira_reg_equiv_len);
         /* We generate the same hard register move because the
         reload pass can put an allocno into memory in this case
         we will have live range splitting.  If it does not happen
         such the same hard register moves will be removed.  The
         worst case when the both allocnos are put into memory by
         the reload is very rare.  */
         if (parent_allocno != NULL
         && (allocno->hard_regno  == parent_allocno->hard_regno)
         && (allocno->hard_regno < 0  || (parent->reg_pressure[pclass] + 1
         <= mtcsIra->x_ira_class_hard_regs_num[pclass])
         || mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(
         &mtcsIraInt->x_ira_prohibited_mode_move_regs[allocno->mode],allocno->hard_regno)
         /* don't create copies because reload can spill an
         allocno set by copy although the allocno will not
         get memory slot.  */
         || mtcs_ira_equiv_no_lvalue_p/*!ira_equiv_no_lvalue_p*/(mtcsIra,regno)
         || (mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(mtcsRTL) != NULL
         && (allocno->regno == (int) REGNO (mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(mtcsRTL))))))
            continue;
         original_reg = mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(allocno);
         if (parent_allocno == NULL
         || (REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(parent_allocno)) == REGNO (original_reg))){
            if (mtcsIraGlobal->internal_flag_ira_verbose > 3 && mtcsIraGlobal->ira_dump_file)
               fprintf (mtcsIraGlobal->ira_dump_file, "  %i vs parent %i:",allocno->hard_regno,parent_allocno->hard_regno);
            set_allocno_reg(self,allocno, mtcs_ira_emit_create_new_reg/*!ira_create_new_reg*/(self,original_reg));
         }
      }
   }
   /* Rename locals: Local allocnos with same regno in different loops
   might get the different hard register.  So we need to change
   ALLOCNO_REG.  */
   bitmap_and_compl (self->local_allocno_bitmap,mtcsIraBuild->ira_curr_loop_tree_node->all_allocnos,
         mtcsIraBuild->ira_curr_loop_tree_node->border_allocnos);
   EXECUTE_IF_SET_IN_REG_SET (self->local_allocno_bitmap, 0, i, bi){
      allocno = mtcsIraBuild->ira_allocnos[i];
      regno = allocno->regno;
      if (allocno->cap_member != NULL)
         continue;
      used_p = !bitmap_set_bit (self->used_regno_bitmap, regno);
      MTCS_ALLOCNO_EMIT_DATA (allocno)->somewhere_renamed_p = true;
      if (! used_p)
         continue;
      bitmap_set_bit (self->renamed_regno_bitmap, regno);
      set_allocno_reg(self,allocno, ira_create_new_reg (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(allocno)));
   }
}

/* Process to set up flag somewhere_renamed_p.  */
static void set_allocno_somewhere_renamed_p (MtcsIraEmit *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);

   unsigned int regno;
   MtcsIraAllocno * allocno;
   MtcsIraAllocnoIterator ai;

   MTCS_FOR_EACH_ALLOCNO(mtcsIraBuild,allocno, ai){
      regno = allocno->regno;
      if (bitmap_bit_p (self->renamed_regno_bitmap, regno)
      && REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(allocno)) == regno)
         MTCS_ALLOCNO_EMIT_DATA (allocno)->somewhere_renamed_p = true;
   }
}

/* Return TRUE if move lists on all edges given in vector VEC are
   equal.  */
static bool eq_edge_move_lists_p (MtcsIraEmit *self,vec<edge, va_gc> *vec)
{
   EmitMove * list;
   int i;

   list = (EmitMove *) EDGE_I (vec, 0)->aux;
   for (i = EDGE_COUNT (vec) - 1; i > 0; i--)
      if (! eq_move_lists_p(self,list, (EmitMove *) EDGE_I (vec, i)->aux))
         return false;
   return true;
}

/* Look at all entry edges (if START_P) or exit edges of basic block
   BB and put move lists at the BB start or end if it is possible.  In
   other words, this decreases code duplication of allocno moves.  */
static void unify_moves (MtcsIraEmit *self,basic_block bb, bool start_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgBuild *mtcsCfgBuild=mtcs_target_get_cfg_build(mtcsTarget);

   int i;
   edge e;
   EmitMove * list;
   vec<edge, va_gc> *vec;

   vec = (start_p ? bb->preds : bb->succs);
   if (EDGE_COUNT (vec) == 0 || ! eq_edge_move_lists_p(self,vec))
      return;
   e = EDGE_I (vec, 0);
   list = (EmitMove *) e->aux;
   if (! start_p && mtcs_cfg_build_control_flow_insn_p/*!control_flow_insn_p*/(mtcsCfgBuild,BB_END (bb)))
      return;
   e->aux = NULL;
   for (i = EDGE_COUNT (vec) - 1; i > 0; i--){
      e = EDGE_I (vec, i);
      free_move_list(self,(EmitMove *) e->aux);
      e->aux = NULL;
   }
   if (start_p)
      self->at_bb_start[bb->index] = list;
   else
      self->at_bb_end[bb->index] = list;
}



/* This recursive function traverses dependencies of MOVE and produces
   topological sorting (in depth-first order).  */
static void traverse_moves (MtcsIraEmit *self,EmitMove * move)
{
   int i;

   if (move->visited_p)
      return;
   move->visited_p = true;
   for (i = move->deps_num - 1; i >= 0; i--)
      traverse_moves(self,move->deps[i]);
   self->move_vec.safe_push (move);
}

/* Remove unnecessary moves in the LIST, makes topological sorting,
   and removes cycles on hard reg dependencies by introducing new
   allocnos assigned to memory and additional moves.  It returns the
   result move list.  */
static EmitMove * modify_move_list (MtcsIraEmit *self,EmitMove * list)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int i, n, nregs, hard_regno;
   MtcsIraAllocno * to, *from;
   EmitMove * move, *new_move, *set_move, *first, *last;

   if (list == NULL)
      return NULL;
   /* Create move deps.  */
   self->curr_tick++;
   for (move = list; move != NULL; move = move->next){
      to = move->to;
      if ((hard_regno = to->hard_regno) < 0)
         continue;
      nregs = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,hard_regno, to->mode);
      for (i = 0; i < nregs; i++){
         self->hard_regno_last_set[hard_regno + i] = move;
         self->hard_regno_last_set_check[hard_regno + i] = self->curr_tick;
      }
   }
   for (move = list; move != NULL; move = move->next){
      from = move->from;
      to = move->to;
      if ((hard_regno = from->hard_regno) >= 0){
         nregs = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,hard_regno, from->mode);
         for (n = i = 0; i < nregs; i++)
            if (self->hard_regno_last_set_check[hard_regno + i] == self->curr_tick
            && (self->hard_regno_last_set[hard_regno + i]->to->regno != from->regno))
               n++;
         move->deps = (EmitMove * *) ira_allocate (n * sizeof (EmitMove *));
         for (n = i = 0; i < nregs; i++)
            if (self->hard_regno_last_set_check[hard_regno + i] == self->curr_tick
            && (self->hard_regno_last_set[hard_regno + i]->to->regno != from->regno))
               move->deps[n++] = self->hard_regno_last_set[hard_regno + i];
         move->deps_num = n;
      }
   }
   /* Topological sorting:  */
   self->move_vec.truncate (0);
   for (move = list; move != NULL; move = move->next)
      traverse_moves(self,move);
   last = NULL;
   for (i = (int) self->move_vec.length () - 1; i >= 0; i--){
      move = self->move_vec[i];
      move->next = NULL;
      if (last != NULL)
         last->next = move;
      last = move;
   }
   first = self->move_vec.last ();
   /* Removing cycles:  */
   self->curr_tick++;
   self->move_vec.truncate (0);
   for (move = first; move != NULL; move = move->next){
   from = move->from;
   to = move->to;
   if ((hard_regno = from->hard_regno) >= 0){
      nregs = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,hard_regno, from->mode);
      for (i = 0; i < nregs; i++)
         if (self->hard_regno_last_set_check[hard_regno + i] == self->curr_tick
         && self->hard_regno_last_set[hard_regno + i]->to->hard_regno >= 0){
            int n, j;
            MtcsIraAllocno * new_allocno;

            set_move = self->hard_regno_last_set[hard_regno + i];
            /* It does not matter what loop_tree_node (of TO or
            FROM) to use for the new allocno because of
            subsequent IRA internal representation
            flattening.  */
            new_allocno = create_new_allocno(self,set_move->to->regno,
            set_move->to->loop_tree_node);
            new_allocno->mode = set_move->to->mode;
            mtcs_ira_build_set_allocno_class/*!ira_set_allocno_class*/(mtcsIraBuild,new_allocno,
             set_move->to->aclass);
            mtcs_ira_build_create_allocno_objects/*!ira_create_allocno_objects*/(mtcsIraBuild,new_allocno);
            new_allocno->assigned_p = true;
            new_allocno->hard_regno = -1;
            MTCS_ALLOCNO_EMIT_DATA (new_allocno)->reg
            = mtcs_ira_emit_create_new_reg/*!ira_create_new_reg*/(self,mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(set_move->to));

            /* Make it possibly conflicting with all earlier
            created allocnos.  Cases where temporary allocnos
            created to remove the cycles are quite rare.  */
            n = new_allocno->num_objects;
            gcc_assert (n == set_move->to->num_objects);
            for (j = 0; j < n; j++){
               MtcsIraObject * new_obj = new_allocno->objects[j];
               new_obj->min = 0;
               new_obj->max = mtcsIraBuild->ira_objects_num - 1;
            }

            new_move = create_move(self,set_move->to, new_allocno);
            set_move->to = new_allocno;
            self->move_vec.safe_push (new_move);
            mtcsIra->ira_move_loops_num++;
            if (mtcsIraGlobal->internal_flag_ira_verbose > 2 && mtcsIraGlobal->ira_dump_file != NULL)
               fprintf (mtcsIraGlobal->ira_dump_file, "    Creating temporary allocno a%dr%d\n",
                     new_allocno->num,REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(new_allocno)));
            }
         }
      if ((hard_regno = to->hard_regno) < 0)
         continue;
      nregs = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,hard_regno, to->mode);
      for (i = 0; i < nregs; i++){
         self->hard_regno_last_set[hard_regno + i] = move;
         self->hard_regno_last_set_check[hard_regno + i] = self->curr_tick;
      }
   }
   for (i = (int) self->move_vec.length () - 1; i >= 0; i--){
      move = self->move_vec[i];
      move->next = NULL;
      last->next = move;
      last = move;
   }
   return first;
}

/* Generate RTX move insns from the move list LIST.  This updates
   allocation cost using move execution frequency FREQ.  */
static rtx_insn * emit_move_list (MtcsIraEmit *self,EmitMove * list, int freq)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsReload1 *mtcsReload1=mtcs_target_get_reload1(mtcsTarget);
   MtcsExpr *mtcsExpr =mtcs_target_get_expr(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   rtx to, from, dest;
   int to_regno, from_regno, cost, regno;
   rtx_insn *result, *insn;
   rtx set;
   machine_mode mode;
   enum reg_class aclass;

   mtcs_reload1_grow_reg_equivs/*!grow_reg_equivs*/(mtcsReload1);
   mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
   for (; list != NULL; list = list->next){
      mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
      to = mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(list->to);
      to_regno = REGNO (to);
      from = mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(list->from);
      from_regno = REGNO (from);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,to, from);
      list->insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      for (insn = list->insn; insn != NULL_RTX; insn = NEXT_INSN (insn)){
         /* The reload needs to have set up insn codes.  If the
         reload sets up insn codes by itself, it may fail because
         insns will have hard registers instead of pseudos and
         there may be no machine insn with given hard
         registers.  */
         mtcs_recog_recog_memoized/*!recog_memoized*/(mtcsRecog,insn);
         /* Add insn to equiv init insn list if it is necessary.
         Otherwise reload will not remove this insn if it decides
         to use the equivalence.  */
         if ((set = single_set (insn)) != NULL_RTX){
            dest = SET_DEST (set);
            if (GET_CODE (dest) == SUBREG)
               dest = SUBREG_REG (dest);
            ira_assert (REG_P (dest));
            regno = REGNO (dest);
            if (regno >= mtcsIra->ira_reg_equiv_len
            || (mtcsIra->ira_reg_equiv[regno].invariant == NULL_RTX
            && mtcsIra->ira_reg_equiv[regno].constant == NULL_RTX))
               continue; /* regno has no equivalence.  */
            ira_assert ((int) reg_equivs->length () > regno);
            reg_equiv_init (regno) = gen_rtx_INSN_LIST (VOIDmode, insn, reg_equiv_init (regno));
         }
      }
      if (mtcsIra->ira_use_lra_p)
         mtcs_ira_update_equiv_info_by_shuffle_insn/*!ira_update_equiv_info_by_shuffle_insn*/(mtcsIra,
               to_regno, from_regno, list->insn);
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,list->insn);
      mode = list->to->mode;
      aclass = list->to->aclass;
      cost = 0;
      if (list->to->hard_regno < 0){
         if (list->from->hard_regno >= 0){
            cost = mtcsIra->x_ira_memory_move_cost[mode][aclass][0] * freq;
            mtcsIra->ira_store_cost += cost;
         }
      }else if (list->from->hard_regno < 0){
         if (list->to->hard_regno >= 0){
            cost = mtcsIra->x_ira_memory_move_cost[mode][aclass][0] * freq;
            mtcsIra->ira_load_cost += cost;
         }
      }else{
         mtcs_ira_int_init_register_move_cost_if_necessary/*!ira_init_register_move_cost_if_necessary*/(mtcsIraInt,mode);
         cost = mtcsIraInt->x_ira_register_move_cost[mode][aclass][aclass] * freq;
         mtcsIra->ira_shuffle_cost += cost;
      }
      mtcsIra->ira_overall_cost += cost;
   }
   result = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   return result;
}

/* Generate RTX move insns from move lists attached to basic blocks
   and edges.  */
static void emit_moves (MtcsIraEmit *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsCfgBuild *mtcsCfgBuild=mtcs_target_get_cfg_build(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);

   basic_block bb;
   edge_iterator ei;
   edge e;
   rtx_insn *insns, *tmp, *next;

   FOR_EACH_BB_FN (bb, cfun){
      if (self->at_bb_start[bb->index] != NULL){
         self->at_bb_start[bb->index] = modify_move_list(self,self->at_bb_start[bb->index]);
         insns = emit_move_list(self,self->at_bb_start[bb->index], REG_FREQ_FROM_BB (bb));
         tmp = BB_HEAD (bb);
         if (LABEL_P (tmp))
            tmp = NEXT_INSN (tmp);
         if (NOTE_INSN_BASIC_BLOCK_P (tmp))
            tmp = NEXT_INSN (tmp);
         /* Make sure to put the location of TMP or a subsequent instruction
         to avoid inheriting the location of the previous instruction.  */
         next = tmp;
         while (next && !NONDEBUG_INSN_P (next))
            next = NEXT_INSN (next);
         if (next)
            set_insn_locations (insns, INSN_LOCATION (next));
         if (tmp == BB_HEAD (bb))
            mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,insns, tmp);
         else if (tmp)
            mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,insns, PREV_INSN (tmp));
         else
            mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,insns, mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData));
      }

      if (self->at_bb_end[bb->index] != NULL){
         self->at_bb_end[bb->index] = modify_move_list(self,self->at_bb_end[bb->index]);
         insns = emit_move_list(self,self->at_bb_end[bb->index], REG_FREQ_FROM_BB (bb));
         ira_assert (! mtcs_cfg_build_control_flow_insn_p/*!control_flow_insn_p*/(mtcsCfgBuild,BB_END (bb)));
         mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,insns, BB_END (bb));
      }

      FOR_EACH_EDGE (e, ei, bb->succs){
         if (e->aux == NULL)
            continue;
         ira_assert ((e->flags & EDGE_ABNORMAL) == 0  || ! EDGE_CRITICAL_P (e));
         e->aux = modify_move_list(self,(EmitMove *) e->aux);
         mtcs_cfg_rtl_insert_insn_on_edge/*!insert_insn_on_edge*/(mtcsCfgRtl,
               emit_move_list(self,(EmitMove *) e->aux, REG_FREQ_FROM_EDGE_FREQ (EDGE_FREQUENCY (e))), e);
         if (e->src->next_bb != e->dest)
            mtcsIra->ira_additional_jumps_num++;
      }
   }
}

/* Update costs of A and corresponding allocnos on upper levels on the
   loop tree from reading (if READ_P) or writing A on an execution
   path with FREQ.  */
static void update_costs (MtcsIraEmit *self,MtcsIraAllocno * a, bool read_p, int freq)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);

   MtcsIraLoopTreeNode * parent;
   for (;;){
      a->nrefs++;
      a->freq += freq;
      a->memory_cost += (mtcsIra->x_ira_memory_move_cost[a->mode][a->aclass][read_p ? 1 : 0] * freq);
      if (a->cap != NULL)
         a = a->cap;
      else if ((parent = a->loop_tree_node->parent) == NULL || (a = parent->regno_allocno_map[a->regno]) == NULL)
         break;
   }
}

/* Process moves from LIST with execution FREQ to add ranges, copies,
   and modify costs for allocnos involved in the moves.  All regnos
   living through the list is in LIVE_THROUGH, and the loop tree node
   used to find corresponding allocnos is NODE.  */
static void add_range_and_copies_from_move_list (MtcsIraEmit *self,EmitMove * list, MtcsIraLoopTreeNode * node,
                 bitmap live_through, int freq)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIraLives *mtcsIraLives=mtcs_ira_mgr_get_lives(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   int start, n;
   unsigned int regno;
   EmitMove * move;
   MtcsIraAllocno * a;
   MtcsIraAllocnoCopy * cp;
   MtcsLiveRange * r;
   bitmap_iterator bi;
   HardRegSet /*!HARD_REG_SET*/ hard_regs_live = {mtcs_reg_get_hard_reg_element_count(mtcsReg)};

   if (list == NULL)
      return;
   n = 0;
   EXECUTE_IF_SET_IN_BITMAP (live_through,firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/, regno, bi)
      n++;
   mtcs_reg_reg_set_to_hard_reg_set/*!REG_SET_TO_HARD_REG_SET*/(mtcsReg,&hard_regs_live, live_through);
   /* This is a trick to guarantee that new ranges is not merged with
   the old ones.  */
   mtcsIraLives->ira_max_point++;
   start = mtcsIraLives->ira_max_point;
   for (move = list; move != NULL; move = move->next){
      MtcsIraAllocno * from = move->from;
      MtcsIraAllocno * to = move->to;
      int nr, i;

      bitmap_clear_bit (live_through, from->regno);
      bitmap_clear_bit (live_through, to->regno);

      nr = to->num_objects;
      for (i = 0; i < nr; i++){
         MtcsIraObject * to_obj = to->objects[i];
         if (to_obj->conflicts_array == NULL){
            if (internal_flag_ira_verbose > 2 && ira_dump_file != NULL)
               fprintf (ira_dump_file, "    Allocate conflicts for a%dr%d\n",to->num,
                     REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(to)));
            mtcs_ira_object_allocate_object_conflicts/*!ira_allocate_object_conflicts*/(to_obj, n);
         }
      }
      mtcs_ira_allocno_ior_hard_reg_conflicts/*!ior_hard_reg_conflicts*/(from, hard_regs_live);
      mtcs_ira_allocno_ior_hard_reg_conflicts/*!ior_hard_reg_conflicts*/(to, hard_regs_live);

      update_costs(self,from, true, freq);
      update_costs(self,to, false, freq);
      cp = mtcs_ira_build_add_allocno_copy/*!ira_add_allocno_copy*/(mtcsIraBuild,from, to, freq, false, move->insn, NULL);
      if (mtcsIraGlobal->internal_flag_ira_verbose > 2 && mtcsIraGlobal->ira_dump_file != NULL)
         fprintf (mtcsIraGlobal->ira_dump_file, "    Adding cp%d:a%dr%d-a%dr%d\n",
                  cp->num, cp->first->num,
                  REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(cp->first)),
                  cp->second->num,
                  REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(cp->second)));

      nr = from->num_objects;
      for (i = 0; i < nr; i++){
         MtcsIraObject * from_obj = from->objects[i];
         r = from_obj->live_ranges;
         if (r == NULL || r->finish >= 0){
            mtcs_ira_object_add_live_range_to_object/*!ira_add_live_range_to_object*/(from_obj, start, mtcsIraLives->ira_max_point);
            if (mtcsIraGlobal->internal_flag_ira_verbose > 2 && mtcsIraGlobal->ira_dump_file != NULL)
               fprintf (mtcsIraGlobal->ira_dump_file,"    Adding range [%d..%d] to allocno a%dr%d\n",
                     start, mtcsIraLives->ira_max_point, from->num,
            REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(from)));
         }else{
            r->finish = mtcsIraLives->ira_max_point;
            if (mtcsIraGlobal->internal_flag_ira_verbose > 2 && mtcsIraGlobal->ira_dump_file != NULL)
               fprintf (mtcsIraGlobal->ira_dump_file,"    Adding range [%d..%d] to allocno a%dr%d\n",
                     r->start, mtcsIraLives->ira_max_point, from->num,
            REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(from)));
         }
      }
      mtcsIraLives->ira_max_point++;
      nr = to->num_objects;
      for (i = 0; i < nr; i++){
         MtcsIraObject * to_obj = to->objects[i];
         mtcs_ira_object_add_live_range_to_object/*!ira_add_live_range_to_object*/(to_obj, mtcsIraLives->ira_max_point, -1);
      }
      mtcsIraLives->ira_max_point++;
   }

   for (move = list; move != NULL; move = move->next){
      int nr, i;
      nr = move->to->num_objects;
      for (i = 0; i < nr; i++){
         MtcsIraObject * to_obj = move->to->objects[i];
         r = to_obj->live_ranges;
         if (r->finish < 0){
            r->finish = mtcsIraLives->ira_max_point - 1;
            if (mtcsIraGlobal->internal_flag_ira_verbose > 2 && mtcsIraGlobal->ira_dump_file != NULL)
               fprintf (mtcsIraGlobal->ira_dump_file,"    Adding range [%d..%d] to allocno a%dr%d\n",
                     r->start, r->finish, move->to->num,REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(move->to)));
         }
      }
   }

   EXECUTE_IF_SET_IN_BITMAP (live_through, firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/, regno, bi){
      MtcsIraAllocno * to;
      int nr, i;

      a = node->regno_allocno_map[regno];
      if ((to = MTCS_ALLOCNO_EMIT_DATA (a)->mem_optimized_dest) != NULL)
         a = to;
      nr = a->num_objects;
      for (i = 0; i < nr; i++){
         MtcsIraObject * obj = a->objects[i];
         mtcs_ira_object_add_live_range_to_object/*!ira_add_live_range_to_object*/(obj, start, mtcsIraLives->ira_max_point - 1);
      }
      if (mtcsIraGlobal->internal_flag_ira_verbose > 2 && mtcsIraGlobal->ira_dump_file != NULL)
         fprintf(mtcsIraGlobal->ira_dump_file,"    Adding range [%d..%d] to live through %s allocno a%dr%d\n",
               start, mtcsIraLives->ira_max_point - 1, to != NULL ? "upper level" : "",
                     a->num, REGNO (mtcs_ira_allocno_emit_reg/*!allocno_emit_reg*/(a)));
   }
}

/* Process all move list to add ranges, conflicts, copies, and modify
   costs for allocnos involved in the moves.  */
static void add_ranges_and_copies (MtcsIraEmit *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild = mtcs_ira_mgr_get_build(mtcsIraMgr);

   basic_block bb;
   edge_iterator ei;
   edge e;
   MtcsIraLoopTreeNode * node;
   bitmap live_through;

   live_through = ira_allocate_bitmap ();
   FOR_EACH_BB_FN (bb, cfun){
   /* It does not matter what loop_tree_node (of source or
   destination block) to use for searching allocnos by their
   regnos because of subsequent IR flattening.  */
   node = MTCS_IRA_BB_NODE (bb)->parent;
      bitmap_copy (live_through, df_get_live_in (bb));
      add_range_and_copies_from_move_list(self,self->at_bb_start[bb->index], node, live_through, REG_FREQ_FROM_BB (bb));
      bitmap_copy (live_through, df_get_live_out (bb));
      add_range_and_copies_from_move_list(self,self->at_bb_end[bb->index], node, live_through, REG_FREQ_FROM_BB (bb));
      FOR_EACH_EDGE (e, ei, bb->succs){
         bitmap_and (live_through,df_get_live_in (e->dest), df_get_live_out (bb));
         add_range_and_copies_from_move_list(self,(EmitMove *) e->aux, node, live_through,REG_FREQ_FROM_EDGE_FREQ (EDGE_FREQUENCY (e)));
      }
   }
   ira_free_bitmap (live_through);
}

/* The entry function changes code and generates shuffling allocnos on
   region borders for the regional (LOOPS_P is TRUE in this case)
   register allocation.  */
//原型 ira_emit ira-int.h ira-emit.cc
void mtcs_ira_emit_emit (MtcsIraEmit *self,bool loops_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsIraMgr *mtcsIraMgr =mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIraBuild *mtcsIraBuild=mtcs_ira_mgr_get_build(mtcsIraMgr);
   MtcsIra *mtcsIra=mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraInt *mtcsIraInt=mtcs_ira_mgr_get_int(mtcsIraMgr);
   MtcsIraGlobal *mtcsIraGlobal = mtcs_ira_mgr_get_global(mtcsIraMgr);

   basic_block bb;
   rtx_insn *insn;
   edge_iterator ei;
   edge e;
   MtcsIraAllocno * a;
   MtcsIraAllocnoIterator ai;
   size_t sz;

   int maxRegNum=mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);

   MTCS_FOR_EACH_ALLOCNO(mtcsIraBuild,a, ai)
      MTCS_ALLOCNO_EMIT_DATA (a)->reg = mtcsRtlData->regno_reg_rtx/*! regno_reg_rtx*/[a->regno];
   if (! loops_p)
      return;
   sz = sizeof (EmitMove *) * last_basic_block_for_fn (cfun);
   self->at_bb_start = (EmitMove * *) ira_allocate (sz);
   memset (self->at_bb_start, 0, sz);
   self->at_bb_end = (EmitMove * *) ira_allocate (sz);
   memset (self->at_bb_end, 0, sz);
   self->local_allocno_bitmap = ira_allocate_bitmap ();
   self->used_regno_bitmap = ira_allocate_bitmap ();
   self->renamed_regno_bitmap = ira_allocate_bitmap ();
   self->max_regno_before_changing = maxRegNum/*!max_reg_num ()*/;
   mtcs_ira_build_traverse_loop_tree/*!ira_traverse_loop_tree*/(mtcsIraBuild,
         true, mtcsIraBuild->ira_loop_tree_root, changeLoop_cb, NULL,(void*)self);
   set_allocno_somewhere_renamed_p(self);
   ira_free_bitmap (self->used_regno_bitmap);
   ira_free_bitmap (self->renamed_regno_bitmap);
   ira_free_bitmap (self->local_allocno_bitmap);
   setup_entered_from_non_parent_p(self);
   FOR_EACH_BB_FN (bb, cfun){
      self->at_bb_start[bb->index] = NULL;
      self->at_bb_end[bb->index] = NULL;
      FOR_EACH_EDGE (e, ei, bb->succs)
         if (e->dest != EXIT_BLOCK_PTR_FOR_FN (cfun))
            generate_edge_moves(self,e);
   }
   self->allocno_last_set = (EmitMove * *) ira_allocate (sizeof (EmitMove *) * maxRegNum/*!max_reg_num ()*/);
   self->allocno_last_set_check = (int *) ira_allocate (sizeof (int) * maxRegNum/*!max_reg_num ()*/);
   memset (self->allocno_last_set_check, 0, sizeof (int) * maxRegNum/*!max_reg_num ()*/);
   memset (self->hard_regno_last_set_check, 0, sizeof (self->hard_regno_last_set_check));
   self->curr_tick = 0;
   FOR_EACH_BB_FN (bb, cfun)
      unify_moves(self,bb, true);
   FOR_EACH_BB_FN (bb, cfun)
      unify_moves(self,bb, false);
   self->move_vec.create (mtcsIraBuild->ira_allocnos_num);
   emit_moves(self);
   add_ranges_and_copies(self);
   /* Clean up: */
   FOR_EACH_BB_FN (bb, cfun){
      free_move_list(self,self->at_bb_start[bb->index]);
      free_move_list(self,self->at_bb_end[bb->index]);
      FOR_EACH_EDGE (e, ei, bb->succs){
         free_move_list(self,(EmitMove *) e->aux);
         e->aux = NULL;
      }
   }
   self->move_vec.release ();
   ira_free (self->allocno_last_set_check);
   ira_free (self->allocno_last_set);
   mtcs_cfg_rtl_commit_edge_insertions/*!commit_edge_insertions*/(mtcsCfgRtl);
   /* Fix insn codes.  It is necessary to do it before reload because
   reload assumes initial insn codes defined.  The insn codes can be
   invalidated by CFG infrastructure for example in jump
   redirection.  */
   FOR_EACH_BB_FN (bb, cfun)
      FOR_BB_INSNS_REVERSE (bb, insn)
         if (INSN_P (insn))
            mtcs_recog_recog_memoized/*!recog_memoized*/(mtcsRecog,insn);
   ira_free (self->at_bb_end);
   ira_free (self->at_bb_start);
}

static void mtcsIraEmitInit(MtcsIraEmit *self)
{

}


MtcsIraEmit *mtcs_ira_emit_new(MtcsMode *mtcsMode)
{
   MtcsIraEmit *self = n_slice_alloc0 (sizeof(MtcsIraEmit));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsIraEmitInit(self);
   return self;
}
