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
 * base on dce.cc
 */


#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "rtl.h"
#include "tree.h"
#include "predict.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "emit-rtl.h"  /* FIXME: Can go away once crtl is moved to rtl.h.  */
#include "cfgrtl.h"
#include "cfgbuild.h"
#include "cfgcleanup.h"
#include "dce.h"
#include "valtrack.h"
#include "tree-pass.h"
#include "dbgcnt.h"
#include "rtl-iter.h"


#include "aet/aetprinttree.h"
#include "mtcsdce.h"
#include "mtcstarget.h"
#include "mtcsdfcore.h"
#include "mtcsdfproblems.h"
#include "mtcsprintrtl.h"


static void printbb(basic_block block)
{
   rtx_insn *insn;
   int i=0;
   FOR_BB_INSNS (block, insn){
      if (!INSN_P (insn))
         continue;
      n_debug("mtcsdce.c 打印块中的指令 i:%d  block:%p index:%d flags:%d insn:%p\n",
            i++,block,block->index,block->flags,insn);
      mtcs_print_rtl_single(stderr,insn);
   }
}

static void printrtl()
{
   return;
   basic_block bb;
   FOR_ALL_BB_FN (bb, cfun)
      printbb(bb);
}

static unsigned int df_reg_chain_mark (df_ref refs, unsigned int regno,
         bool is_def, bool is_eq_use)
{
   if(!refs)
      return 0;
   unsigned int count = 0;
   df_ref ref;
   for (ref = refs; ref; ref = DF_REF_NEXT_REG (ref)){
     // gcc_assert (!DF_REF_IS_REG_MARKED (ref));
      fprintf(stderr,"re is xxx :%p %d\n",ref,ref==0xffffffff);
      if(ref==0xffffffff)
         break;
      if(DF_REF_IS_REG_MARKED (ref))
         n_debug("mtcsdce.c DF_REF_IS_REG_MARKED (ref)=true 错误\n");

      /* If there are no def-use or use-def chains, make sure that all
      of the chains are clear.  */
      n_debug("mtcsdce.c df_reg_chain_mark 00 ref:%p flags:%d is_def:%d is_eq_use:%d df_chain:%p regno:%d %d\n",
            ref,ref->base.flags,is_def,is_eq_use,df_chain,regno,DF_REF_REGNO (ref));

//      if (!df_chain)
//         gcc_assert (!DF_REF_CHAIN (ref));
//      /* Check to make sure the ref is in the correct chain.  */
//      gcc_assert (DF_REF_REGNO (ref) == regno);
//      if (is_def)
//         gcc_assert (DF_REF_REG_DEF_P (ref));
//      else
//         gcc_assert (!DF_REF_REG_DEF_P (ref));
//     // n_debug("mtcsdfscan.c df_reg_chain_mark 11 ref:%p flags:%d is_def:%d is_eq_use:%d\n",ref,ref->base.flags,is_def,is_eq_use);
//
//      if (is_eq_use)
//         gcc_assert ((DF_REF_FLAGS (ref) & DF_REF_IN_NOTE));
//      else
//         gcc_assert ((DF_REF_FLAGS (ref) & DF_REF_IN_NOTE) == 0);
//      //n_debug("mtcsdfscan.c df_reg_chain_mark 22 ref:%p flags:%d is_def:%d is_eq_use:%d\n",ref,ref->base.flags,is_def,is_eq_use);
//
//      if (DF_REF_NEXT_REG (ref))
//         gcc_assert (DF_REF_PREV_REG (DF_REF_NEXT_REG (ref)) == ref);
//      count++;
//     // n_debug("mtcsdfscan.c df_reg_chain_mark 33 ref:%p flags:%d is_def:%d is_eq_use:%d\n",ref,ref->base.flags,is_def,is_eq_use);
//      DF_REF_REG_MARK (ref);
//     // n_debug("mtcsdfscan.c df_reg_chain_mark 44 ref:%p flags:%d is_def:%d is_eq_use:%d\n",ref,ref->base.flags,is_def,is_eq_use);

   }
   return count;
}

/* Return true if df_ref information for all insns in all blocks are
   correct and complete.  */
//原型 df_scan_verify df.h df-scan.cc
static void dfScanVerify (struct function *fn)
{
   return;
   if(!n_log_is_debug_file(NULL,NULL))
      return;
   tree fndecl = fn->decl;
   char *name=IDENTIFIER_POINTER(DECL_NAME(fndecl));
   if(!strstr(name,"setdata"))
      return;
   char *passName="dce";

   unsigned int i;
   basic_block bb;
   if (!df)
      return;
   for (i = 0; i < DF_REG_SIZE (df); i++){
      fprintf(stderr,"mtcsdce.c dfScanVerify --- i:%d %d pass:%s\n",i,DF_REG_DEF_COUNT (i),passName);
      df_reg_chain_mark (DF_REG_DEF_CHAIN (i), i, true, false);
     // gcc_assert (df_reg_chain_mark (DF_REG_DEF_CHAIN (i), i, true, false) == DF_REG_DEF_COUNT (i));
     // gcc_assert (df_reg_chain_mark (DF_REG_USE_CHAIN (i), i, false, false) == DF_REG_USE_COUNT (i));
     // gcc_assert (df_reg_chain_mark (DF_REG_EQ_USE_CHAIN (i), i, false, true) == DF_REG_EQ_USE_COUNT (i));
   }
}

static bool find_call_stack_args (MtcsDce *self,rtx_call_insn *, bool, bool, bitmap);

/* A subroutine for which BODY is part of the instruction being tested;
   either the top-level pattern, or an element of a PARALLEL.  The
   instruction is known not to be a bare USE or CLOBBER.  */
static bool deletable_insn_p_1 (MtcsDce *self,rtx body)
{
   switch (GET_CODE (body)){
      case PREFETCH:
      case TRAP_IF:
      /* The UNSPEC case was added here because the ia-64 claims that
      USEs do not work after reload and generates UNSPECS rather
      than USEs.  Since dce is run after reload we need to avoid
      deleting these even if they are dead.  If it turns out that
      USEs really do work after reload, the ia-64 should be
      changed, and the UNSPEC case can be removed.  */
      case UNSPEC:
         return false;

      default:
         n_debug("mtcsdce.c deletable_insn_p_1 00 body:%p %d\n",body,volatile_refs_p (body));

         return !volatile_refs_p (body);
   }
}

/* Don't delete calls that may throw if we cannot do so.  */
static bool can_delete_call (MtcsDce *self,rtx_insn *insn)
{
   if (cfun->can_delete_dead_exceptions && self->can_alter_cfg)
      return true;
   if (!insn_nothrow_p (insn))
      return false;
   if (self->can_alter_cfg)
      return true;
   /* If we can't alter cfg, even when the call can't throw exceptions, it
   might have EDGE_ABNORMAL_CALL edges and so we shouldn't delete such
   calls.  */
   gcc_assert (CALL_P (insn));
   if (BLOCK_FOR_INSN (insn) && BB_END (BLOCK_FOR_INSN (insn)) == insn){
      edge e;
      edge_iterator ei;

      FOR_EACH_EDGE (e, ei, BLOCK_FOR_INSN (insn)->succs)
         if ((e->flags & EDGE_ABNORMAL_CALL) != 0)
            return false;
   }
   return true;
}

/* Return true if INSN is a normal instruction that can be deleted by
   the DCE pass.  */
static bool deletable_insn_p (MtcsDce *self,rtx_insn *insn, bool fast, bitmap arg_stores)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   rtx body, x;
   int i;
   df_ref def;
   n_debug("mtcsdce.c deletable_insn_p 00 insn:%p fast:%d\n",insn,fast);

   if (CALL_P (insn)
   /* We cannot delete calls inside of the recursive dce because
   this may cause basic blocks to be deleted and this messes up
   the rest of the stack of optimization passes.  */
   && (!self->df_in_progress)
   /* We cannot delete pure or const sibling calls because it is
   hard to see the result.  */
   && (!SIBLING_CALL_P (insn))
   /* We can delete dead const or pure calls as long as they do not
   infinite loop.  */
   && (RTL_CONST_OR_PURE_CALL_P (insn)
   && !RTL_LOOPING_CONST_OR_PURE_CALL_P (insn))
   /* Don't delete calls that may throw if we cannot do so.  */
   && can_delete_call(self,insn)){
      n_debug("mtcsdce.c deletable_insn_p 11 insn:%p\n",insn);
      return find_call_stack_args(self,as_a <rtx_call_insn *> (insn), false,fast, arg_stores);
   }

   /* Don't delete jumps, notes and the like.  */
   if (!NONJUMP_INSN_P (insn))
      return false;

   /* Don't delete insns that may throw if we cannot do so.  */
   if (!(cfun->can_delete_dead_exceptions && self->can_alter_cfg) && !insn_nothrow_p (insn))
      return false;
   n_debug("mtcsdce.c deletable_insn_p 22 insn:%p fast:%d\n",insn,fast);

   /* If INSN sets a global_reg, leave it untouched.  */
   FOR_EACH_INSN_DEF (def, insn){
      n_debug("mtcsdce.c deletable_insn_p 33 insn:%p fast:%d DF_REF_REGNO (def):%d\n",insn,fast,DF_REF_REGNO (def));

      if (mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,DF_REF_REGNO (def))
      && mtcsReg->global_regs/*!global_regs*/[DF_REF_REGNO (def)]){
         return false;
      /* Initialization of pseudo PIC register should never be removed.  */
      }else if (DF_REF_REG (def) == mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(mtcsRTL)
      && REGNO (mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(mtcsRTL))
      >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)){
         return false;
      }
   }
   /* Callee-save restores are needed.  */
   if (RTX_FRAME_RELATED_P (insn)  && mtcsRtlData/*!crtl*/->shrink_wrapped_separate && find_reg_note (insn, REG_CFA_RESTORE, NULL))
      return false;
   n_debug("mtcsdce.c deletable_insn_p 44 insn:%p fast:%d\n",insn,fast);

   body = PATTERN (insn);
   switch (GET_CODE (body)){
      case USE:
      case VAR_LOCATION:
         return false;

      case CLOBBER:
         if (fast){
            /* A CLOBBER of a dead pseudo register serves no purpose.
            That is not necessarily true for hard registers until
            after reload.  */
            x = XEXP (body, 0);
            n_debug("mtcsdce.c deletable_insn_p 55 insn:%p reload_completed:%d\n",insn,reload_completed);

            return REG_P (x) && (!mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,x) || reload_completed);
         }else
            /* Because of the way that use-def chains are built, it is not
            possible to tell if the clobber is dead because it can
            never be the target of a use-def chain.  */
            return false;

      case PARALLEL:
         for (i = XVECLEN (body, 0) - 1; i >= 0; i--)
            if (!deletable_insn_p_1(self,XVECEXP (body, 0, i)))
               return false;
         n_debug("mtcsdce.c deletable_insn_p 66 insn:%p\n",insn);

         return true;

      default:
         n_debug("mtcsdce.c deletable_insn_p 77 insn:%p body:%p\n",insn,body);

         return deletable_insn_p_1(self,body);
   }
}

/* Return true if INSN has been marked as needed.  */
static inline int marked_insn_p (MtcsDce *self,rtx_insn *insn)
{
   /* Artificial defs are always needed and they do not have an insn.
   We should never see them here.  */
   gcc_assert (insn);
   return bitmap_bit_p (self->marked, INSN_UID (insn));
}

/* If INSN has not yet been marked as needed, mark it now, and add it to
   the worklist.  */
static void mark_insn (MtcsDce *self,rtx_insn *insn, bool fast)
{
   if (!marked_insn_p(self,insn)){
      if (!fast)
         self->worklist.safe_push (insn);
      n_debug("mtcsdce.c mark_insn 00 insn:%p fast:%d\n",insn,fast);
      bitmap_set_bit (self->marked, INSN_UID (insn));
      if (dump_file)
         fprintf (dump_file, "  Adding insn %d to worklist\n", INSN_UID (insn));
      if (CALL_P (insn)
      && !self->df_in_progress
      && !SIBLING_CALL_P (insn)
      && (RTL_CONST_OR_PURE_CALL_P (insn)
      && !RTL_LOOPING_CONST_OR_PURE_CALL_P (insn))
      && can_delete_call(self,insn)){
         n_debug("mtcsdce.c mark_insn 11 insn:%p fast:%d\n",insn,fast);

         find_call_stack_args(self,as_a <rtx_call_insn *> (insn), true, fast, NULL);
      }
   }
}

typedef struct _MarkData
{
   MtcsDce *mtcsDce;
   rtx_insn *value;
}MarkData;

/* A note_stores callback used by mark_nonreg_stores.  DATA is the
   instruction containing DEST.  */
static void mark_nonreg_stores_1 (rtx dest, const_rtx pattern, void *userData)
{
   MarkData *markDataInfo=(MarkData *)userData;
   MtcsDce *self=markDataInfo->mtcsDce;
   n_debug("mtcsdce.c mark_nonreg_stores_1 %d %d dest:%p value:%p\n",GET_CODE (pattern) != CLOBBER,REG_P (dest),dest, markDataInfo->value);
   if (GET_CODE (pattern) != CLOBBER && !REG_P (dest))
      mark_insn(self,(rtx_insn *) markDataInfo->value, true);
}


/* A note_stores callback used by mark_nonreg_stores.  DATA is the
   instruction containing DEST.  */
static void mark_nonreg_stores_2 (rtx dest, const_rtx pattern, void *userData)
{
   MarkData *markDataInfo=(MarkData *)userData;
   MtcsDce *self=markDataInfo->mtcsDce;
   n_debug("mtcsdce.c mark_nonreg_stores_2 %d %d dest:%p value:%p\n",GET_CODE (pattern) != CLOBBER,REG_P (dest),dest, markDataInfo->value);

   if (GET_CODE (pattern) != CLOBBER && !REG_P (dest))
      mark_insn(self,(rtx_insn *) markDataInfo->value, false);
}

/* Mark INSN if it stores to a non-register destination.  */
static void mark_nonreg_stores (MtcsDce *self,rtx_insn *insn, bool fast)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   MarkData userData={self,insn};
   if (fast)
      mtcs_rtlanal_note_stores/*!note_stores*/(mtcsRtlanal,insn, mark_nonreg_stores_1, (void *)&userData/*!insn*/);
   else
      mtcs_rtlanal_note_stores/*!note_stores*/(mtcsRtlanal,insn, mark_nonreg_stores_2, (void *)&userData/*!insn*/);
}


/* Return true if a store to SIZE bytes, starting OFF bytes from stack pointer,
   is a call argument store, and clear corresponding bits from SP_BYTES
   bitmap if it is.  */
static bool check_argument_store (HOST_WIDE_INT size, HOST_WIDE_INT off,
            HOST_WIDE_INT min_sp_off, HOST_WIDE_INT max_sp_off,
            bitmap sp_bytes)
{
   HOST_WIDE_INT byte;
   for (byte = off; byte < off + size; byte++){
      if (byte < min_sp_off || byte >= max_sp_off || !bitmap_clear_bit (sp_bytes, byte - min_sp_off))
         return false;
   }
   return true;
}

/* If MEM has sp address, return 0, if it has sp + const address,
   return that const, if it has reg address where reg is set to sp + const
   and FAST is false, return const, otherwise return
   INTTYPE_MINUMUM (HOST_WIDE_INT).  */
static HOST_WIDE_INT sp_based_mem_offset (MtcsDce *self,rtx_call_insn *call_insn, const_rtx mem, bool fast)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   HOST_WIDE_INT off = 0;
   rtx addr = XEXP (mem, 0);
   if (GET_CODE (addr) == PLUS  && REG_P (XEXP (addr, 0))  && CONST_INT_P (XEXP (addr, 1))){
      off = INTVAL (XEXP (addr, 1));
      addr = XEXP (addr, 0);
   }
   if (addr == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL))
      return off;

   if (!REG_P (addr) || fast)
      return INTTYPE_MINIMUM (HOST_WIDE_INT);

   /* If not fast, use chains to see if addr wasn't set to sp + offset.  */
   df_ref use;
   FOR_EACH_INSN_USE (use, call_insn)
      if (rtx_equal_p (addr, DF_REF_REG (use)))
         break;

   if (use == NULL)
      return INTTYPE_MINIMUM (HOST_WIDE_INT);

   struct df_link *defs;
   for (defs = DF_REF_CHAIN (use); defs; defs = defs->next)
      if (! DF_REF_IS_ARTIFICIAL (defs->ref))
         break;

   if (defs == NULL)
      return INTTYPE_MINIMUM (HOST_WIDE_INT);

   rtx set = single_set (DF_REF_INSN (defs->ref));
   if (!set)
      return INTTYPE_MINIMUM (HOST_WIDE_INT);

   if (GET_CODE (SET_SRC (set)) != PLUS
   || XEXP (SET_SRC (set), 0) != mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)
   || !CONST_INT_P (XEXP (SET_SRC (set), 1)))
      return INTTYPE_MINIMUM (HOST_WIDE_INT);

   off += INTVAL (XEXP (SET_SRC (set), 1));
   return off;
}

/* Data for check_argument_load called via note_uses.  */
struct check_argument_load_data {
  bitmap sp_bytes;
  HOST_WIDE_INT min_sp_off, max_sp_off;
  rtx_call_insn *call_insn;
  bool fast;
  bool load_found;
  MtcsDce *mtcsDce;
};

/* Helper function for find_call_stack_args.  Check if there are
   any loads from the argument slots in between the const/pure call
   and store to the argument slot, set LOAD_FOUND if any is found.  */
static void checkArgumentLoad_cb (rtx *loc, void *data)
{
   struct check_argument_load_data *d = (struct check_argument_load_data *) data;
   MtcsDce *self = d->mtcsDce;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   subrtx_iterator::array_type array;
   FOR_EACH_SUBRTX (iter, array, *loc, NONCONST){
      const_rtx mem = *iter;
      HOST_WIDE_INT size;
      if (MEM_P (mem)  && mtcs_rtl_is_mem_size_known_p/*!MEM_SIZE_KNOWN_P*/(mtcsRTL,mem)
      && mtcs_rtl_get_mem_size/*!MEM_SIZE*/(mtcsRTL,mem).is_constant (&size)){
         HOST_WIDE_INT off = sp_based_mem_offset(self,d->call_insn, mem, d->fast);
         if (off != INTTYPE_MINIMUM (HOST_WIDE_INT)  && off < d->max_sp_off   && off + size > d->min_sp_off)
            for (HOST_WIDE_INT byte = MAX (off, d->min_sp_off);  byte < MIN (off + size, d->max_sp_off); byte++)
               if (bitmap_bit_p (d->sp_bytes, byte - d->min_sp_off)){
                  d->load_found = true;
                  return;
               }
      }
   }
}

/* Try to find all stack stores of CALL_INSN arguments if
   ACCUMULATE_OUTGOING_ARGS.  If all stack stores have been found
   and it is therefore safe to eliminate the call, return true,
   otherwise return false.  This function should be first called
   with DO_MARK false, and only when the CALL_INSN is actually
   going to be marked called again with DO_MARK true.  */
static bool find_call_stack_args (MtcsDce *self,rtx_call_insn *call_insn, bool do_mark, bool fast,
            bitmap arg_stores)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   rtx p;
   rtx_insn *insn, *prev_insn;
   bool ret;
   HOST_WIDE_INT min_sp_off, max_sp_off;
   bitmap sp_bytes;

   gcc_assert (CALL_P (call_insn));
   if (!mtcs_func_is_accumulate_outgoing_args/*!ACCUMULATE_OUTGOING_ARGS*/(mtcsFunc))
      return true;

   if (!do_mark){
      gcc_assert (arg_stores);
      bitmap_clear (arg_stores);
   }

   min_sp_off = INTTYPE_MAXIMUM (HOST_WIDE_INT);
   max_sp_off = 0;

   /* First determine the minimum and maximum offset from sp for
   stored arguments.  */
   for (p = CALL_INSN_FUNCTION_USAGE (call_insn); p; p = XEXP (p, 1))
      if (GET_CODE (XEXP (p, 0)) == USE && MEM_P (XEXP (XEXP (p, 0), 0))){
         rtx mem = XEXP (XEXP (p, 0), 0);
         HOST_WIDE_INT size;
         if (!mtcs_rtl_is_mem_size_known_p/*!MEM_SIZE_KNOWN_P*/(mtcsRTL,mem)
         || !mtcs_rtl_get_mem_size/*!MEM_SIZE*/(mtcsRTL,mem).is_constant (&size))
            return false;
         HOST_WIDE_INT off = sp_based_mem_offset(self,call_insn, mem, fast);
         if (off == INTTYPE_MINIMUM (HOST_WIDE_INT))
            return false;
         min_sp_off = MIN (min_sp_off, off);
         max_sp_off = MAX (max_sp_off, off + size);
      }

   if (min_sp_off >= max_sp_off)
      return true;
   sp_bytes = BITMAP_ALLOC (NULL);

   /* Set bits in SP_BYTES bitmap for bytes relative to sp + min_sp_off
   which contain arguments.  Checking has been done in the previous
   loop.  */
   for (p = CALL_INSN_FUNCTION_USAGE (call_insn); p; p = XEXP (p, 1))
      if (GET_CODE (XEXP (p, 0)) == USE && MEM_P (XEXP (XEXP (p, 0), 0))){
         rtx mem = XEXP (XEXP (p, 0), 0);
         /* Checked in the previous iteration.  */
         HOST_WIDE_INT size = mtcs_rtl_get_mem_size/*!MEM_SIZE*/(mtcsRTL,mem).to_constant ();
         HOST_WIDE_INT off = sp_based_mem_offset(self,call_insn, mem, fast);
         gcc_checking_assert (off != INTTYPE_MINIMUM (HOST_WIDE_INT));
         for (HOST_WIDE_INT byte = off; byte < off + size; byte++)
         if (!bitmap_set_bit (sp_bytes, byte - min_sp_off))
            gcc_unreachable ();
      }

   /* Walk backwards, looking for argument stores.  The search stops
   when seeing another call, sp adjustment, memory store other than
   argument store or a read from an argument stack slot.  */
   struct check_argument_load_data data = { sp_bytes, min_sp_off, max_sp_off, call_insn, fast, false,self };
   ret = false;
   for (insn = PREV_INSN (call_insn); insn; insn = prev_insn){
      if (insn == BB_HEAD (BLOCK_FOR_INSN (call_insn)))
         prev_insn = NULL;
      else
         prev_insn = PREV_INSN (insn);

      if (CALL_P (insn))
         break;

      if (!NONDEBUG_INSN_P (insn))
         continue;

      rtx set = single_set (insn);
      if (!set || SET_DEST (set) == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL))
         break;

      note_uses (&PATTERN (insn), checkArgumentLoad_cb/*!check_argument_load*/, &data);
      if (data.load_found)
         break;

      if (!MEM_P (SET_DEST (set)))
         continue;

      rtx mem = SET_DEST (set);
      HOST_WIDE_INT off = sp_based_mem_offset(self,call_insn, mem, fast);
      if (off == INTTYPE_MINIMUM (HOST_WIDE_INT))
         break;

      HOST_WIDE_INT size;
      if (!mtcs_rtl_is_mem_size_known_p/*!MEM_SIZE_KNOWN_P*/(mtcsRTL,mem)
      || !mtcs_rtl_get_mem_size/*!MEM_SIZE*/(mtcsRTL,mem).is_constant (&size)
      || !check_argument_store (size, off, min_sp_off,  max_sp_off, sp_bytes))
         break;

      if (!deletable_insn_p(self,insn, fast, NULL))
         break;

      if (do_mark)
         mark_insn(self,insn, fast);
      else
         bitmap_set_bit (arg_stores, INSN_UID (insn));

      if (bitmap_empty_p (sp_bytes)){
         ret = true;
         break;
      }
   }

   BITMAP_FREE (sp_bytes);
   if (!ret && arg_stores)
      bitmap_clear (arg_stores);

   return ret;
}

/* Remove all REG_EQUAL and REG_EQUIV notes referring to the registers INSN
   writes to.  */
static void remove_reg_equal_equiv_notes_for_defs (MtcsDce *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   df_ref def;

   FOR_EACH_INSN_DEF (def, insn)
      mtcs_rtlanal_remove_reg_equal_equiv_notes_for_regno/*!remove_reg_equal_equiv_notes_for_regno*/(mtcsRtlanal,
            DF_REF_REGNO (def));
}

/* Scan all BBs for debug insns and reset those that reference values
   defined in unmarked insns.  */

static void reset_unmarked_insns_debug_uses (MtcsDce *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);

   basic_block bb;
   rtx_insn *insn, *next;

   FOR_EACH_BB_REVERSE_FN (bb, cfun)
      FOR_BB_INSNS_REVERSE_SAFE (bb, insn, next)
         if (DEBUG_INSN_P (insn)){
            df_ref use;

            FOR_EACH_INSN_USE (use, insn){
               struct df_link *defs;
               for (defs = DF_REF_CHAIN (use); defs; defs = defs->next) {
                  rtx_insn *ref_insn;
                  if (DF_REF_IS_ARTIFICIAL (defs->ref))
                     continue;
                  ref_insn = DF_REF_INSN (defs->ref);
                  if (!marked_insn_p(self,ref_insn))
                     break;
               }
               if (!defs)
                  continue;
               /* ??? FIXME could we propagate the values assigned to
               each of the DEFs?  */
               INSN_VAR_LOCATION_LOC (insn) = gen_rtx_UNKNOWN_VAR_LOC ();
               mtcs_dfscan_df_insn_rescan_debug_internal/*!df_insn_rescan_debug_internal*/(mtcsDfscan,insn);
               break;
            }
         }
}

/* Delete every instruction that hasn't been marked.  */
static void delete_unmarked_insns (MtcsDce *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
   MtcsCfgCleanup *mtcsCfgCleanup=mtcs_target_get_cfg_cleanup(mtcsTarget);

   basic_block bb;
   rtx_insn *insn, *next;
   bool must_clean = false;

   FOR_EACH_BB_REVERSE_FN (bb, cfun)
      FOR_BB_INSNS_REVERSE_SAFE (bb, insn, next)
         if (NONDEBUG_INSN_P (insn)){
            rtx turn_into_use = NULL_RTX;
            n_debug("mtcsdec.c delete_unmarked_insns 00 NONDEBUG_INSN_P %d %d %d %d\n",
                  cfun->can_throw_non_call_exceptions,
                  cfun->can_delete_dead_exceptions,
                  self->can_alter_cfg,
                  insn_nothrow_p (insn));

            /* Always delete no-op moves.  */
            if (mtcs_rtlanal_noop_move_p/*!noop_move_p*/(mtcsRtlanal,insn)
            /* Unless the no-op move can throw and we are not allowed
            to alter cfg.  */
            && (!cfun->can_throw_non_call_exceptions
            || (cfun->can_delete_dead_exceptions && self->can_alter_cfg)
            || insn_nothrow_p (insn))){
               n_debug("mtcsdec.c delete_unmarked_insns 11 NONDEBUG_INSN_P\n");
               if (RTX_FRAME_RELATED_P (insn))
                  turn_into_use = find_reg_note (insn, REG_CFA_RESTORE, NULL);
               if (turn_into_use && REG_P (XEXP (turn_into_use, 0)))
                  turn_into_use = XEXP (turn_into_use, 0);
               else
                  turn_into_use = NULL_RTX;
            }
            /* Otherwise rely only on the DCE algorithm.  */
            else if (marked_insn_p(self,insn)){
               n_debug("mtcsdec.c delete_unmarked_insns 22 NONDEBUG_INSN_P\n");

               continue;
            }

            /* Beware that reaching a dbg counter limit here can result
            in miscompiled file.  This occurs when a group of insns
            must be deleted together, typically because the kept insn
            depends on the output from the deleted insn.  Deleting
            this insns in reverse order (both at the bb level and
            when looking at the blocks) minimizes this, but does not
            eliminate it, since it is possible for the using insn to
            be top of a block and the producer to be at the bottom of
            the block.  However, in most cases this will only result
            in an uninitialized use of an insn that is dead anyway.

            However, there is one rare case that will cause a
            miscompile: deletion of non-looping pure and constant
            calls on a machine where ACCUMULATE_OUTGOING_ARGS is true.
            In this case it is possible to remove the call, but leave
            the argument pushes to the stack.  Because of the changes
            to the stack pointer, this will almost always lead to a
            miscompile.  */
            if (!dbg_cnt (dce))
               continue;

            if (dump_file)
               fprintf (dump_file, "DCE: Deleting insn %d\n", INSN_UID (insn));
            n_debug("mtcsdec.c delete_unmarked_insns 33 NONDEBUG_INSN_P\n");

            /* Before we delete the insn we have to remove the REG_EQUAL notes
            for the destination regs in order to avoid dangling notes.  */
            remove_reg_equal_equiv_notes_for_defs(self,insn);

            if (turn_into_use){
               /* Don't remove frame related noop moves if they cary
               REG_CFA_RESTORE note, while we don't need to emit any code,
               we need it to emit the CFI restore note.  */
               n_debug("mtcsdec.c delete_unmarked_insns 44 NONDEBUG_INSN_P\n");

               PATTERN (insn) = gen_rtx_USE (GET_MODE (turn_into_use), turn_into_use);
               INSN_CODE (insn) = -1;
               mtcs_dfscan_df_insn_rescan/*!df_insn_rescan*/(mtcsDfscan,insn);
            }else{
               /* Now delete the insn.  */
               n_debug("mtcsdec.c delete_unmarked_insns 55 NONDEBUG_INSN_P delete insn\n");
               mtcs_print_rtl_single(stderr,insn);

               must_clean |= mtcs_cfg_rtl_delete_insn_and_edges/*!delete_insn_and_edges*/(mtcsCfgRtl,insn);
            }
         }

   /* Deleted a pure or const call.  */
   if (must_clean){
      n_debug("mtcsdce.c delete_unmarked_insns 66\n");
      gcc_assert (self->can_alter_cfg);
      mtcs_cfg_cleanup_delete_unreachable_blocks/*!delete_unreachable_blocks*/(mtcsCfgCleanup);
      free_dominance_info (CDI_DOMINATORS);
   }
}

/* Go through the instructions and mark those whose necessity is not
   dependent on inter-instruction information.  Make sure all other
   instructions are not marked.  */
static void prescan_insns_for_dce (MtcsDce *self,bool fast)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   basic_block bb;
   rtx_insn *insn, *prev;
   bitmap arg_stores = NULL;

   if (dump_file)
      fprintf (dump_file, "Finding needed instructions:\n");

   if (!self->df_in_progress && mtcs_func_is_accumulate_outgoing_args/*!ACCUMULATE_OUTGOING_ARGS*/(mtcsFunc))
      arg_stores = BITMAP_ALLOC (NULL);

   FOR_EACH_BB_FN (bb, cfun){
      FOR_BB_INSNS_REVERSE_SAFE (bb, insn, prev)
         if (NONDEBUG_INSN_P (insn)){
            /* Don't mark argument stores now.  They will be marked
            if needed when the associated CALL is marked.  */
            if (arg_stores && bitmap_bit_p (arg_stores, INSN_UID (insn)))
               continue;

            if (deletable_insn_p(self,insn, fast, arg_stores)){
               n_debug("mtcsdce.c prescan_insns_for_dce 00 fast:%d insn:%p\n",fast,insn);
               mtcs_print_rtl_single(stderr,insn);
               mark_nonreg_stores(self,insn, fast);
            }else{
               n_debug("mtcsdce.c prescan_insns_for_dce 11 mark_insn fast:%d insn:%p \n",fast,insn);
               mtcs_print_rtl_single(stderr,insn);
               mark_insn(self,insn, fast);
            }
         }
      /* find_call_stack_args only looks at argument stores in the
      same bb.  */
      if (arg_stores)
         bitmap_clear (arg_stores);
   }

   if (arg_stores)
      BITMAP_FREE (arg_stores);

   if (dump_file)
      fprintf (dump_file, "Finished finding needed instructions:\n");
}

/* UD-based DSE routines. */

/* Mark instructions that define artificially-used registers, such as
   the frame pointer and the stack pointer.  */
static void mark_artificial_uses (MtcsDce *self)
{
   basic_block bb;
   struct df_link *defs;
   df_ref use;
   int count=0;
   FOR_ALL_BB_FN (bb, cfun){
      FOR_EACH_ARTIFICIAL_USE (use, bb->index){
         for (defs = DF_REF_CHAIN (use); defs; defs = defs->next){
            n_debug("mtcsdce.c mark_artificial_uses: %d DF_REF_IS_ARTIFICIAL (defs->ref):%d\n",count++,DF_REF_IS_ARTIFICIAL (defs->ref));
            n_debug("mtcsdce.c defs->ref->base.insn_info:%p\n",defs->ref->base.insn_info);
             if (!DF_REF_IS_ARTIFICIAL (defs->ref)){
               n_debug("mtcsdce.c mtcsdce.c mark_artificial_uses: %d 非人工的 defs->ref:%p \n",count,DF_REF_INSN (defs->ref));
               mark_insn(self,DF_REF_INSN (defs->ref), false);
            }
         }
      }
   }
}

/* Mark every instruction that defines a register value that INSN uses.  */
static void mark_reg_dependencies (MtcsDce *self,rtx_insn *insn)
{
   struct df_link *defs;
   df_ref use;

   if (DEBUG_INSN_P (insn))
      return;
   n_debug("mtcsdce.c mark_reg_dependencies 00 insn: %d\n", INSN_UID (insn));
   mtcs_print_rtl_single(stderr,insn);

   FOR_EACH_INSN_USE (use, insn){
      if (dump_file){
         fprintf (dump_file, "Processing use of ");
         print_simple_rtl (dump_file, DF_REF_REG (use));
         fprintf (dump_file, " in insn %d:\n", INSN_UID (insn));
      }
      n_debug("mtcsdce.c Processing use of in insn %d\n",INSN_UID (insn));
      mtcs_print_rtl_single(stderr,DF_REF_REG (use));
      for (defs = DF_REF_CHAIN (use); defs; defs = defs->next){
         n_debug("mtcsdce.c mark_reg_dependencies xx %p\n",defs);
         n_debug("mtcsdce.c mark_reg_dependencies yy %p\n",defs->ref);
         n_debug("mtcsdce.c mark_reg_dependencies zz %p\n",defs->ref->base);
         n_debug("mtcsdce.c mark_reg_dependencies ww %p\n",defs->ref->base.cl);
         n_debug("mtcsdce.c mark_reg_dependencies 66 %p insn:%p\n",defs->ref->base.insn_info);
         if(defs->ref->base.insn_info)
            n_debug("mtcsdce.c mark_reg_dependencies 77 %p insn:%p\n",defs->ref->base.insn_info->insn,insn);
         n_debug("mtcsdce.c mark_reg_dependencies %d\n",DF_REF_IS_ARTIFICIAL (defs->ref));

         if (! DF_REF_IS_ARTIFICIAL (defs->ref)){
            mark_insn(self,DF_REF_INSN (defs->ref), false);
         }
      }
   }

}

/* Initialize global variables for a new DCE pass.  */
static void init_dce (MtcsDce *self,bool fast)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsDfproblems *mtcsDfproblems=mtcs_target_get_dfproblems(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   n_debug("mtcsdce.c init_dce 00 fast:%d df_in_progress:%d\n",fast,self->df_in_progress);
   if (!self->df_in_progress){
      if (!fast){
         mtcs_dfcore_df_set_flags/*!df_set_flags*/(mtcsDfcore,DF_RD_PRUNE_DEAD_DEFS);
         mtcs_dfproblems_df_chain_add_problem/*!df_chain_add_problem*/(mtcsDfproblems,DF_UD_CHAIN);
      }
      mtcs_dfcore_df_analyze/*!df_analyze*/(mtcsDfcore);
   }

   if (dump_file)
      mtcs_dfcore_df_dump/*!df_dump*/(mtcsDfcore,dump_file);

   if (fast){
      bitmap_obstack_initialize (&self->dce_blocks_bitmap_obstack);
      bitmap_obstack_initialize (&self->dce_tmp_bitmap_obstack);
      self->can_alter_cfg = false;
   }else
      self->can_alter_cfg = true;
   n_debug("mtcsdce.c init_dce 11 fast:%d df_in_progress:%d\n",fast,self->df_in_progress);

   self->marked = sbitmap_alloc (mtcs_rtl_data_get_max_uid/*!get_max_uid*/(mtcsRtlData) + 1);
   bitmap_clear (self->marked);
}

/* Free the data allocated by init_dce.  */
static void fini_dce (MtcsDce *self,bool fast)
{
   sbitmap_free (self->marked);
   if (fast){
      bitmap_obstack_release (&self->dce_blocks_bitmap_obstack);
      bitmap_obstack_release (&self->dce_tmp_bitmap_obstack);
   }
}

static void testuselink (rtx_insn *insn)
{
   struct df_link *defs;
   df_ref use;

   if (DEBUG_INSN_P (insn))
      return;
   fprintf (stderr, "mtcsdce.c testuselink 00 insn: %d\n", INSN_UID (insn));
   mtcs_print_rtl_single(stderr,insn);

   FOR_EACH_INSN_USE (use, insn){
      for (defs = DF_REF_CHAIN (use); defs; defs = defs->next){
         fprintf (stderr,"mtcsdce.c testuselink 11 %p insn:%p\n",defs,insn);
         fprintf (stderr,"mtcsdce.c testuselink 22 %p insn:%p\n",defs->ref,insn);
         fprintf (stderr,"mtcsdce.c testuselink 33 %p insn:%p\n",defs->ref->base,insn);
         fprintf (stderr,"mtcsdce.c testuselink 44 %p insn:%p\n",defs->ref->base.cl,insn);
         fprintf (stderr,"mtcsdce.c testuselink 55 %d insn:%p\n",DF_REF_IS_ARTIFICIAL (defs->ref),insn);
         fprintf (stderr,"mtcsdce.c testuselink 66 %p insn:%p\n",defs->ref->base.insn_info);
         if(defs->ref->base.insn_info)
            fprintf (stderr,"mtcsdce.c testuselink 77 %p insn:%p\n",defs->ref->base.insn_info->insn,insn);
      }
   }

}

static void testprintbb(basic_block block)
{
   rtx_insn *insn;
   int i=0;
   FOR_BB_INSNS (block, insn){
      if (!INSN_P (insn))
         continue;
      n_debug("mtcsdce.c 打印块中的指令 i:%d block:%p index:%d flags:%d insn:%p\n",i++,block,block->index,block->flags,insn);
      mtcs_print_rtl_single(stderr,insn);
      testuselink(insn);
   }
}

static void testprint()
{
   return;
   if(!cfun )
      return;
   basic_block bb;
   FOR_ALL_BB_FN (bb, cfun)
      testprintbb(bb);
}


/* UD-chain based DCE.  */
static unsigned int rest_of_handle_ud_dce (MtcsDce *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);

   rtx_insn *insn;
   n_debug("mtcsdce.c rest_of_handle_ud_dce 00\n");
   testprint();
   init_dce(self,false);
   n_debug("mtcsdce.c rest_of_handle_ud_dce 11\n");
   testprint();
   prescan_insns_for_dce(self,false);
   n_debug("mtcsdce.c rest_of_handle_ud_dce 22\n");
   testprint();

   mark_artificial_uses(self);
   n_debug("mtcsdce.c rest_of_handle_ud_dce 33\n");
   testprint();

   while (self->worklist.length () > 0){
      insn = self->worklist.pop ();
      n_debug("mtcsdce.c rest_of_handle_ud_dce 44\n");
      mtcs_print_rtl_single(stderr,insn);
      mark_reg_dependencies(self,insn);
   }
   self->worklist.release ();

   if (MAY_HAVE_DEBUG_BIND_INSNS)
      reset_unmarked_insns_debug_uses(self);
   /* Before any insns are deleted, we must remove the chains since
   they are not bidirectional.  */
   mtcs_dfcore_df_remove_problem/*!df_remove_problem*/(mtcsDfcore,df_chain);
   delete_unmarked_insns(self);
   fini_dce(self,false);
   return 0;
}

/* -------------------------------------------------------------------------
   Fast DCE functions
   ------------------------------------------------------------------------- */

/* Process basic block BB.  Return true if the live_in set has
   changed. REDO_OUT is true if the info at the bottom of the block
   needs to be recalculated before starting.  AU is the proper set of
   artificial uses.  Track global substitution of uses of dead pseudos
   in debug insns using GLOBAL_DEBUG.  */
static bool word_dce_process_block (MtcsDce *self,basic_block bb, bool redo_out,
         struct dead_debug_global *global_debug)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsDfproblems *mtcsDfproblems=mtcs_target_get_dfproblems(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsCfgBuild *mtcsCfgBuild=mtcs_target_get_cfg_build(mtcsTarget);

   bitmap local_live = BITMAP_ALLOC (&self->dce_tmp_bitmap_obstack);
   rtx_insn *insn;
   bool block_changed;
   struct dead_debug_local debug;

   if (redo_out){
      /* Need to redo the live_out set of this block if when one of
      the succs of this block has had a change in it live in
      set.  */
      edge e;
      edge_iterator ei;
      df_confluence_function_n con_fun_n = df_word_lr->problem->con_fun_n;
      bitmap_clear (DF_WORD_LR_OUT (bb));
      FOR_EACH_EDGE (e, ei, bb->succs)
         (*con_fun_n) (e);
   }

   if (dump_file){
      fprintf (dump_file, "processing block %d live out = ", bb->index);
      mtcs_dfcore_df_print_word_regset/*!df_print_word_regset*/(mtcsDfcore,dump_file, DF_WORD_LR_OUT (bb));
   }

   bitmap_copy (local_live, DF_WORD_LR_OUT (bb));
   dead_debug_local_init (&debug, NULL, global_debug);

   FOR_BB_INSNS_REVERSE (bb, insn)
      if (DEBUG_INSN_P (insn)){
         df_ref use;
         FOR_EACH_INSN_USE (use, insn)
            if (DF_REF_REGNO (use) >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
            && known_eq (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (DF_REF_REAL_REG (use))),2 * UNITS_PER_WORD)
            && !bitmap_bit_p (local_live, 2 * DF_REF_REGNO (use))
            && !bitmap_bit_p (local_live, 2 * DF_REF_REGNO (use) + 1))
               dead_debug_add (&debug, use, DF_REF_REGNO (use));
      }else if (INSN_P (insn)){
         bool any_changed;

         /* No matter if the instruction is needed or not, we remove
         any regno in the defs from the live set.  */
         any_changed = mtcs_dfproblems_df_word_lr_simulate_defs/*!df_word_lr_simulate_defs*/(mtcsDfproblems,insn, local_live);
         if (any_changed)
            mark_insn(self,insn, true);

         /* On the other hand, we do not allow the dead uses to set
         anything in local_live.  */
         if (marked_insn_p(self,insn))
            mtcs_dfproblems_df_word_lr_simulate_uses/*!df_word_lr_simulate_uses*/(mtcsDfproblems,insn, local_live);

         /* Insert debug temps for dead REGs used in subsequent debug
         insns.  We may have to emit a debug temp even if the insn
         was marked, in case the debug use was after the point of
         death.  */
         if (debug.used && !bitmap_empty_p (debug.used)){
            df_ref def;

            FOR_EACH_INSN_DEF (def, insn)
               dead_debug_insert_temp (&debug, DF_REF_REGNO (def), insn,
                     marked_insn_p(self,insn)
                     && !mtcs_cfg_build_control_flow_insn_p/*!control_flow_insn_p*/(mtcsCfgBuild,insn)
                     ? DEBUG_TEMP_AFTER_WITH_REG_FORCE : DEBUG_TEMP_BEFORE_WITH_VALUE);
         }

         if (dump_file){
            fprintf (dump_file, "finished processing insn %d live out = ",INSN_UID (insn));
            mtcs_dfcore_df_print_word_regset/*!df_print_word_regset*/(mtcsDfcore,dump_file, local_live);
         }
      }

   block_changed = !bitmap_equal_p (local_live, DF_WORD_LR_IN (bb));
   if (block_changed)
      bitmap_copy (DF_WORD_LR_IN (bb), local_live);

   dead_debug_local_finish (&debug, NULL);
   BITMAP_FREE (local_live);
   return block_changed;
}


/* Process basic block BB.  Return true if the live_in set has
   changed. REDO_OUT is true if the info at the bottom of the block
   needs to be recalculated before starting.  AU is the proper set of
   artificial uses.  Track global substitution of uses of dead pseudos
   in debug insns using GLOBAL_DEBUG.  */
static bool dce_process_block (MtcsDce *self,basic_block bb, bool redo_out, bitmap au,
         struct dead_debug_global *global_debug)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsDfproblems *mtcsDfproblems=mtcs_target_get_dfproblems(mtcsTarget);
   MtcsCfgBuild *mtcsCfgBuild=mtcs_target_get_cfg_build(mtcsTarget);

   bitmap local_live = BITMAP_ALLOC (&self->dce_tmp_bitmap_obstack);
   n_debug("mtcsdce.c dce_process_block 00 local_live:%p redo_out:%d au:%d\n",local_live,redo_out,au);
   rtx_insn *insn;
   bool block_changed;
   df_ref def;
   struct dead_debug_local debug;

   if (redo_out){
      /* Need to redo the live_out set of this block if when one of
      the succs of this block has had a change in it live in
      set.  */
      edge e;
      edge_iterator ei;
      df_confluence_function_n con_fun_n = df_lr->problem->con_fun_n;
      bitmap_clear (DF_LR_OUT (bb));
      FOR_EACH_EDGE (e, ei, bb->succs)
         (*con_fun_n) (e);
   }

   if (dump_file){
      fprintf (dump_file, "processing block %d lr out = ", bb->index);
      mtcs_dfcore_df_print_regset/*!df_print_regset*/(mtcsDfcore,dump_file, DF_LR_OUT (bb));
   }
   bitmap_copy (local_live, DF_LR_OUT (bb));
   n_debug("mtcsdce.c dce_process_block 11 local_live:%p redo_out:%d au:%d\n",local_live,redo_out,au);
   mtcs_dfproblems_df_simulate_initialize_backwards/*!df_simulate_initialize_backwards*/(mtcsDfproblems,bb, local_live);
   dead_debug_local_init (&debug, NULL, global_debug);

   FOR_BB_INSNS_REVERSE (bb, insn)
      if (DEBUG_INSN_P (insn)){
         df_ref use;
         n_debug("mtcsdce.c dce_process_block 22 needed %d\n");
         mtcs_print_rtl_single(stderr,insn);

         FOR_EACH_INSN_USE (use, insn)
            if (!bitmap_bit_p (local_live, DF_REF_REGNO (use)) && !bitmap_bit_p (au, DF_REF_REGNO (use)))
               dead_debug_add (&debug, use, DF_REF_REGNO (use));
      }else if (INSN_P (insn)){
         bool needed = marked_insn_p(self,insn);
         n_debug("mtcsdce.c dce_process_block 33 bb:%p insn:%p needed %d\n",bb,insn,needed);
         mtcs_print_rtl_single(stderr,insn);

         /* The insn is needed if there is someone who uses the output.  */
         if (!needed)
            FOR_EACH_INSN_DEF (def, insn){
               n_debug("mtcsdce.c dce_process_block 44 bb:%p insn:%p needed %d %d %d\n",bb,insn,
                  needed,bitmap_bit_p (local_live, DF_REF_REGNO (def)),bitmap_bit_p (au, DF_REF_REGNO (def)));
               if (bitmap_bit_p (local_live, DF_REF_REGNO (def)) || bitmap_bit_p (au, DF_REF_REGNO (def))){
                  needed = true;
                  mark_insn(self,insn, true);

                  mtcs_print_rtl_single(stderr,insn);

                  break;
               }
            }
         n_debug("mtcsdce.c dce_process_block 44 aa打印  local_live need:%d insn:%p\n",needed,insn);
         if(n_log_is_debug_file(NULL,NULL))
            dump_bitmap (stderr, local_live);
         /* No matter if the instruction is needed or not, we remove
         any regno in the defs from the live set.  */
         mtcs_dfproblems_df_simulate_defs/*!df_simulate_defs*/(mtcsDfproblems,insn, local_live);
         n_debug("mtcsdce.c  dce_process_block 44 bb打印 local_live insn:%p\n",insn);
         if(n_log_is_debug_file(NULL,NULL))
            dump_bitmap (stderr, local_live);
         /* On the other hand, we do not allow the dead uses to set
         anything in local_live.  */
         if (needed)
            mtcs_dfproblems_df_simulate_uses/*!df_simulate_uses*/(mtcsDfproblems,insn, local_live);
         n_debug("mtcsdce.c   dce_process_block 44 cc打印  local_live insn:%p\n",insn);
         if(n_log_is_debug_file(NULL,NULL))
            dump_bitmap (stderr, local_live);
         /* Insert debug temps for dead REGs used in subsequent debug
         insns.  We may have to emit a debug temp even if the insn
         was marked, in case the debug use was after the point of
         death.  */
         if (debug.used && !bitmap_empty_p (debug.used))
            FOR_EACH_INSN_DEF (def, insn)
               dead_debug_insert_temp (&debug, DF_REF_REGNO (def), insn,
                     needed && !mtcs_cfg_build_control_flow_insn_p/*!control_flow_insn_p*/(mtcsCfgBuild,insn)
                     ? DEBUG_TEMP_AFTER_WITH_REG_FORCE : DEBUG_TEMP_BEFORE_WITH_VALUE);
      }//end   if (DEBUG_INSN_P (insn))



   dead_debug_local_finish (&debug, NULL);
   mtcs_dfproblems_df_simulate_finalize_backwards/*!df_simulate_finalize_backwards*/(mtcsDfproblems,bb, local_live);
   n_debug("mtcsdce.c  dce_process_block 44 dd打印  local_live \n");
   if(n_log_is_debug_file(NULL,NULL))
      dump_bitmap (stderr, local_live);

   block_changed = !bitmap_equal_p (local_live, DF_LR_IN (bb));
   if (block_changed)
      bitmap_copy (DF_LR_IN (bb), local_live);

   BITMAP_FREE (local_live);
   return block_changed;
}


/* Perform fast DCE once initialization is done.  If WORD_LEVEL is
   true, use the word level dce, otherwise do it at the pseudo
   level.  */
static void fast_dce (MtcsDce *self,bool word_level)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);

   int *postorder = mtcs_dfcore_df_get_postorder/*!df_get_postorder*/(mtcsDfcore,DF_BACKWARD);
   int n_blocks =mtcs_dfcore_df_get_n_blocks/*!df_get_n_blocks*/(mtcsDfcore,DF_BACKWARD);
   n_debug("mtcsdce.c fast_dce 00 word_level:%d postorder:%p n_blocks:%d\n",word_level,postorder,n_blocks);
   dfScanVerify(cfun);
   /* The set of blocks that have been seen on this iteration.  */
   bitmap processed = BITMAP_ALLOC (&self->dce_blocks_bitmap_obstack);
   /* The set of blocks that need to have the out vectors reset because
   the in of one of their successors has changed.  */
   bitmap redo_out = BITMAP_ALLOC (&self->dce_blocks_bitmap_obstack);
   bitmap all_blocks = BITMAP_ALLOC (&self->dce_blocks_bitmap_obstack);
   bool global_changed = true;
   /* These regs are considered always live so if they end up dying
   because of some def, we need to bring the back again.  Calling
   df_simulate_fixup_sets has the disadvantage of calling
   bb_has_eh_pred once per insn, so we cache the information
   here.  */
   bitmap au = &df->regular_block_artificial_uses;
   bitmap au_eh = &df->eh_block_artificial_uses;
   int i;
   struct dead_debug_global global_debug;
   n_debug("mtcsdce.c fast_dce 11 word_level:%d postorder:%p n_blocks:%d\n",word_level,postorder,n_blocks);
   dfScanVerify(cfun);

   prescan_insns_for_dce(self,true);

   for (i = 0; i < n_blocks; i++)
      bitmap_set_bit (all_blocks, postorder[i]);

   dead_debug_global_init (&global_debug, NULL);
   n_debug("mtcsdce.c fast_dce 22 word_level:%d global_changed:%d\n",word_level,global_changed);
   dfScanVerify(cfun);

   while (global_changed){
      global_changed = false;

      for (i = 0; i < n_blocks; i++){
         int index = postorder[i];
         basic_block bb = BASIC_BLOCK_FOR_FN (cfun, index);
         bool local_changed;
         n_debug("mtcsdce.c fast_dce 33 n_blocks:%d i:%d processed:%p index:%d\n",n_blocks,i,processed,index);
         dfScanVerify(cfun);

         if (index < NUM_FIXED_BLOCKS){
            n_debug("mtcsdce.c fast_dce 44 n_blocks:%d i:%d processed:%p index:%d\n",n_blocks,i,processed,index);
            bitmap_set_bit (processed, index);
            continue;
         }

         if (word_level){
            n_debug("mtcsdce.c fast_dce 44aa n_blocks:%d i:%d processed:%p index:%d\n",n_blocks,i,processed,index);

            dfScanVerify(cfun);

            local_changed = word_dce_process_block(self,bb, bitmap_bit_p (redo_out, index), &global_debug);
            n_debug("mtcsdce.c fast_dce 44bb n_blocks:%d i:%d processed:%p index:%d\n",n_blocks,i,processed,index);

            dfScanVerify(cfun);

         }else{
            n_debug("mtcsdce.c fast_dce 44cc n_blocks:%d i:%d processed:%p index:%d\n",n_blocks,i,processed,index);

                      dfScanVerify(cfun);
            local_changed = dce_process_block(self,bb, bitmap_bit_p (redo_out, index),bb_has_eh_pred (bb) ? au_eh : au, &global_debug);
            n_debug("mtcsdce.c fast_dce 44dd n_blocks:%d i:%d processed:%p index:%d\n",n_blocks,i,processed,index);

                 dfScanVerify(cfun);
         }
         n_debug("mtcsdce.c fast_dce 55 n_blocks:%d i:%d processed:%p index:%d local_changed:%d\n",
               n_blocks,i,processed,index,local_changed);
         dfScanVerify(cfun);

         bitmap_set_bit (processed, index);

         if (local_changed){
            edge e;
            edge_iterator ei;
            FOR_EACH_EDGE (e, ei, bb->preds)
               if (bitmap_bit_p (processed, e->src->index)){
                  n_debug("mtcsdce.c fast_dce 66 n_blocks:%d i:%d processed:%p index:%d local_changed:%d\n",
                               n_blocks,i,processed,index,local_changed);
                  /* Be tricky about when we need to iterate the
                  analysis.  We only have redo the analysis if the
                  bitmaps change at the top of a block that is the
                  entry to a loop.  */
                  global_changed = true;
               }else{
                  n_debug("mtcsdce.c fast_dce 77 n_blocks:%d i:%d processed:%p index:%d redo_out:%p\n",
                               n_blocks,i,processed,index,redo_out);
                  bitmap_set_bit (redo_out, e->src->index);
               }
         }
      }
      n_debug("mtcsdce.c fast_dce 77aa\n");
      dfScanVerify(cfun);

      if (global_changed){
         /* Turn off the RUN_DCE flag to prevent recursive calls to
         dce.  */
         int old_flag = mtcs_dfcore_df_clear_flags/*!df_clear_flags*/(mtcsDfcore,DF_LR_RUN_DCE);
         n_debug("mtcsdce.c fast_dce 88 old_flag:%d word_level:%d\n",old_flag,word_level);
         dfScanVerify(cfun);

         /* So something was deleted that requires a redo.  Do it on
         the cheap.  */
         delete_unmarked_insns(self);
         bitmap_clear (self->marked);
         bitmap_clear (processed);
         bitmap_clear (redo_out);

         /* We do not need to rescan any instructions.  We only need
         to redo the dataflow equations for the blocks that had a
         change at the top of the block.  Then we need to redo the
         iteration.  */

         if (word_level)
            mtcs_dfcore_df_analyze_problem/*!df_analyze_problem*/(mtcsDfcore,df_word_lr, all_blocks, postorder, n_blocks);
         else
            mtcs_dfcore_df_analyze_problem/*!df_analyze_problem*/(mtcsDfcore,df_lr, all_blocks, postorder, n_blocks);

         if (old_flag & DF_LR_RUN_DCE)
            mtcs_dfcore_df_set_flags/*!df_set_flags*/(mtcsDfcore,DF_LR_RUN_DCE);

         prescan_insns_for_dce(self,true);
      }
   }
   n_debug("mtcsdce.c fast_dce 99 word_level:%d global_changed:%d %p %p %p\n",
         word_level,global_changed,processed,redo_out,all_blocks);
   dfScanVerify(cfun);

   //printrtl();

   dead_debug_global_finish (&global_debug, NULL);
   n_debug("mtcsdce.c fast_dce 100 word_level:%d global_changed:%d %p %p %p\n",
         word_level,global_changed,processed,redo_out,all_blocks);
   dfScanVerify(cfun);
   delete_unmarked_insns(self);
   n_debug("mtcsdce.c fast_dce 101 word_level:%d global_changed:%d %p %p %p\n",
         word_level,global_changed,processed,redo_out,all_blocks);
   dfScanVerify(cfun);
   BITMAP_FREE (processed);
   BITMAP_FREE (redo_out);
   BITMAP_FREE (all_blocks);
   /* Both forms of DCE should make further DCE unnecessary.  */
   df_lr_dce->solutions_dirty = false;
}


/* Fast register level DCE.  */

static unsigned int rest_of_handle_fast_dce (MtcsDce *self)
{
  init_dce(self,true);
  fast_dce(self,false);
  fini_dce(self,true);
  return 0;
}


/* Fast byte level DCE.  */
//原型 run_word_dce dce.h dce.cc
void mtcs_dce_run_word_dce (MtcsDce *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsDfproblems *mtcsDfproblems=mtcs_target_get_dfproblems(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   int old_flags;

   if (!mtcsOptionsItem->x_flag_dce)
      return;

   old_flags = mtcs_dfcore_df_clear_flags/*!df_clear_flags*/(mtcsDfcore,DF_DEFER_INSN_RESCAN + DF_NO_INSN_RESCAN);
   mtcs_dfproblems_df_word_lr_add_problem/*!df_word_lr_add_problem*/(mtcsDfproblems);
   init_dce(self,true);
   fast_dce(self,true);
   fini_dce(self,true);
   mtcs_dfcore_df_set_flags/*!df_set_flags*/(mtcsDfcore,old_flags);
}


/* This is an internal call that is used by the df live register
   problem to run fast dce as a side effect of creating the live
   information.  The stack is organized so that the lr problem is run,
   this pass is run, which updates the live info and the df scanning
   info, and then returns to allow the rest of the problems to be run.

   This can be called by elsewhere but it will not update the bit
   vectors for any other problems than LR.  */
//原型 run_fast_df_dce dce.h dce.cc
void mtcs_dce_run_fast_df_dce (MtcsDce *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   if (mtcsOptionsItem->x_flag_dce){
      /* If dce is able to delete something, it has to happen
      immediately.  Otherwise there will be problems handling the
      eq_notes.  */
      n_debug("mtcsdce.c mtcs_dce_run_fast_df_dce 00\n");
      int old_flags = mtcs_dfcore_df_clear_flags/*!df_clear_flags*/(mtcsDfcore,DF_DEFER_INSN_RESCAN + DF_NO_INSN_RESCAN);
     // printrtl();
      self->df_in_progress = true;
      rest_of_handle_fast_dce(self);
      self->df_in_progress = false;
      n_debug("mtcsdce.c mtcs_dce_run_fast_df_dce 11 old_flags:%d\n",old_flags);
     // printrtl();
      dfScanVerify(cfun);
      mtcs_dfcore_df_set_flags/*!df_set_flags*/(mtcsDfcore,old_flags);
      n_debug("mtcsdce.c mtcs_dce_run_fast_df_dce 22\n");
   }
}


/* Run a fast DCE pass.  */
//原型 run_fast_dce dce.h dce.cc
void mtcs_dce_run_fast_dce (MtcsDce *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   if (mtcsOptionsItem->x_flag_dce)
      rest_of_handle_fast_dce(self);
}

static void mtcsDceInit(MtcsDce *self)
{
   self->df_in_progress=false;
   self->can_alter_cfg=false;
}

MtcsDce *mtcs_dce_new(MtcsMode *mtcsMode)
{
   MtcsDce *self = n_slice_alloc0 (sizeof(MtcsDce));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsDceInit(self);
   return self;
}

/**********************以下是基于mtcsdce的rtl pass-------*/

//原型 NEXT_PASS (pass_ud_rtl_dce, 1);  RTL_PASS  ud_dce dce.cc y 有条件执行 optimize > 1 && flag_dce && dbg_cnt (dce_ud);rest_of_handle_ud_dce ()
static nuint ud_dce_execute_cb(MtcsPass *mtcsPass,function *func)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDce *mtcsDce = mtcs_target_get_dce(mtcsTarget);
   return rest_of_handle_ud_dce (mtcsDce);
}

static nboolean ud_dce_gate_cb(MtcsPass *mtcsPass,function *fun)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
    MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
    return mtcsOptionsItem->x_optimize>0 && mtcsOptionsItem->x_flag_dce  && dbg_cnt (dce_ud);
}

static void mtcsPassUdDceInit (MtcsPassUdDce *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =ud_dce_execute_cb;
    mtcsPass->gate=ud_dce_gate_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          TODO_df_finish /*todo_flags_finish */);
}

MtcsPassUdDce *mtcs_pass_ud_dce_new(MtcsMode *mtcsMode)
{
   MtcsPassUdDce *self = n_slice_alloc0 (sizeof(MtcsPassUdDce));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"ud_dce");
   mtcsPassUdDceInit(self);
   return self;
}
