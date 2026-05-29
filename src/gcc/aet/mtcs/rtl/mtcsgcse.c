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
 * base on gcse.cc
 */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "predict.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "print-rtl.h"
#include "regs.h"
#include "ira.h"
#include "recog.h"
#include "diagnostic-core.h"
#include "cfgrtl.h"
#include "cfganal.h"
#include "lcm.h"
#include "cfgcleanup.h"
#include "expr.h"
#include "intl.h"
#include "tree-pass.h"
#include "dbgcnt.h"
#include "gcse.h"
#include "gcse-common.h"
#include "function-abi.h"
#include "toplev.h"

#include "aet/aetprinttree.h"
#include "mtcsgcse.h"
#include "mtcsiracosts.h"
#include "mtcsira.h"
#include "../mtcscompile.h"
#include "../mtcstarget.h"
#include "../mtcsdfcore.h"
#include "../mtcsdfproblems.h"
#include "../mtcsprintrtl.h"

/* Set to non-zero if CSE should run after all GCSE optimizations are done.  */
//int flag_rerun_cse_after_global_opts; toplev.h中声明
struct pre_ldst_expr_hasher : nofree_ptr_hash <ls_expr>
{
  typedef value_type compare_type;
  static inline hashval_t hash (const ls_expr *);
  static inline bool equal (const ls_expr *, const ls_expr *);
};

/* Hashtable helpers.  */
inline hashval_t pre_ldst_expr_hasher::hash (const ls_expr *x)
{
  int do_not_record_p = 0;
  return     hash_rtx (x->pattern, GET_MODE (x->pattern), &do_not_record_p, NULL, false);
}

static bool expr_equiv_p (MtcsGcse *self,const_rtx, const_rtx);

inline bool pre_ldst_expr_hasher::equal (const ls_expr *ptr1,const ls_expr *ptr2)
{
   MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
   MtcsRtlPassMgr *mtcsRtlPassMgr=mtcs_target_get_rtl_pass_mgr(mtcsTarget);
   MtcsGcse *mtcsGcse =mtcs_rtl_pass_mgr_get_gcse(mtcsRtlPassMgr);
  return expr_equiv_p (mtcsGcse,ptr1->pattern, ptr2->pattern);
}

/* Data stored for each basic block.  */
struct bb_data
{
  /* Maximal register pressure inside basic block for given register class
     (defined only for the pressure classes).  */
  int max_reg_pressure[MAX_N_REG_CLASSES/*!N_REG_CLASSES足够大*/];
  /* Recorded register pressure of basic block before trying to hoist
     an expression.  Will be used to restore the register pressure
     if the expression should not be hoisted.  */
  int old_pressure;
  /* Recorded register live_in info of basic block during code hoisting
     process.  BACKUP is used to record live_in info before trying to
     hoist an expression, and will be used to restore LIVE_IN if the
     expression should not be hoisted.  */
  bitmap live_in, backup;
};

#define BB_DATA(bb) ((struct bb_data *) (bb)->aux)



static void compute_can_copy (MtcsGcse *self);
static void *gmalloc (MtcsGcse *self,size_t) ATTRIBUTE_MALLOC;
static void *gcalloc (MtcsGcse *self,size_t, size_t) ATTRIBUTE_MALLOC;
static void *gcse_alloc (MtcsGcse *self,unsigned long);
static void alloc_gcse_mem (MtcsGcse *self);
static void free_gcse_mem (MtcsGcse *self);
static void hash_scan_insn (MtcsGcse *self,rtx_insn *, struct gcse_hash_table_d *);
static void hash_scan_set (MtcsGcse *self,rtx, rtx_insn *, struct gcse_hash_table_d *);
static void hash_scan_clobber (rtx, rtx_insn *, struct gcse_hash_table_d *);
static void hash_scan_call (rtx, rtx_insn *, struct gcse_hash_table_d *);
static bool oprs_unchanged_p (MtcsGcse *self,const_rtx, const rtx_insn *, bool);
static bool oprs_anticipatable_p (MtcsGcse *self,const_rtx, const rtx_insn *);
static bool oprs_available_p (MtcsGcse *self,const_rtx, const rtx_insn *);
static void insert_expr_in_table (MtcsGcse *self,rtx, machine_mode, rtx_insn *, bool, bool,
              HOST_WIDE_INT, struct gcse_hash_table_d *);
static unsigned int hash_expr (MtcsGcse *self,const_rtx, machine_mode, int *, int);
static void record_last_reg_set_info (MtcsGcse *self,rtx_insn *, int);
static void record_last_mem_set_info (MtcsGcse *self,rtx_insn *);
static void record_last_set_info (rtx, const_rtx, void *);
static void compute_hash_table (MtcsGcse *self,struct gcse_hash_table_d *);
static void alloc_hash_table (MtcsGcse *self,struct gcse_hash_table_d *);
static void free_hash_table (struct gcse_hash_table_d *);
static void compute_hash_table_work (MtcsGcse *self,struct gcse_hash_table_d *);
static void dump_hash_table (FILE *, const char *, struct gcse_hash_table_d *);
static void compute_local_properties (MtcsGcse *self,sbitmap *, sbitmap *, sbitmap *,
                  struct gcse_hash_table_d *);
static void mems_conflict_for_gcse_p (rtx, const_rtx, void *);
static bool load_killed_in_block_p (MtcsGcse *self,const_basic_block, int, const_rtx, bool);
static void alloc_pre_mem (MtcsGcse *self,int, int);
static void free_pre_mem (MtcsGcse *self);
static struct edge_list *compute_pre_data (MtcsGcse *self);
static bool pre_expr_reaches_here_p (MtcsGcse *self,basic_block, struct gcse_expr *,
                 basic_block);
static void insert_insn_end_basic_block (MtcsGcse *self,struct gcse_expr *, basic_block);
static void pre_insert_copy_insn (MtcsGcse *self,struct gcse_expr *, rtx_insn *);
static void pre_insert_copies (MtcsGcse *self);
static bool pre_delete (MtcsGcse *self);
static bool pre_gcse (MtcsGcse *self,struct edge_list *);
static bool one_pre_gcse_pass (MtcsGcse *self);
static void add_label_notes (MtcsGcse *self,rtx, rtx_insn *);
static void alloc_code_hoist_mem (MtcsGcse *self,int, int);
static void free_code_hoist_mem (MtcsGcse *self);
static void compute_code_hoist_vbeinout (MtcsGcse *self);
static void compute_code_hoist_data (MtcsGcse *self);
static bool should_hoist_expr_to_dom (MtcsGcse *self,basic_block, struct gcse_expr *,
                  basic_block,
                  sbitmap, HOST_WIDE_INT, int *,
                  enum reg_class,
                  int *, bitmap, rtx_insn *);
static bool hoist_code (void);
static enum reg_class get_regno_pressure_class (MtcsGcse *self,int regno, int *nregs);
static enum reg_class get_pressure_class_and_nregs (MtcsGcse *self,rtx_insn *insn, int *nregs);
static bool one_code_hoisting_pass (MtcsGcse *self);
static rtx_insn *process_insert_insn (MtcsGcse *self,struct gcse_expr *);
static bool pre_edge_insert (MtcsGcse *self,struct edge_list *, struct gcse_expr **);
static bool pre_expr_reaches_here_p_work (MtcsGcse *self,basic_block, struct gcse_expr *,
                 basic_block, char *);
static struct ls_expr * ldst_entry (MtcsGcse *self,rtx);
static void free_ldst_entry (struct ls_expr *);
static void free_ld_motion_mems (MtcsGcse *self);
static void print_ldst_list (MtcsGcse *self,FILE *);
static struct ls_expr * find_rtx_in_ldst (MtcsGcse *self,rtx);
static bool simple_mem (MtcsGcse *self,const_rtx);
static void invalidate_any_buried_refs (MtcsGcse *self,rtx);
static void compute_ld_motion_mems (MtcsGcse *self);
static void trim_ld_motion_mems (MtcsGcse *self);
static void update_ld_motion_stores (MtcsGcse *self,struct gcse_expr *);
static void clear_modify_mem_tables (MtcsGcse *self);
static void free_modify_mem_tables (MtcsGcse *self);

#define GNEW(T)         ((T *) gmalloc(self,sizeof (T)))
#define GCNEW(T)     ((T *) gcalloc(self,1, sizeof (T)))

#define GNEWVEC(T, N)      ((T *) gmalloc(self,sizeof (T) * (N)))
#define GCNEWVEC(T, N)     ((T *) gcalloc(self,(N), sizeof (T)))

#define GNEWVAR(T, S)      ((T *) gmalloc(self,(S)))
#define GCNEWVAR(T, S)     ((T *) gcalloc(self,1, (S)))

#define GOBNEW(T)    ((T *) gcse_alloc(self,sizeof (T)))
#define GOBNEWVAR(T, S)    ((T *) gcse_alloc(self,(S)))

/* Compute which modes support reg/reg copy operations.  */
static void compute_can_copy (MtcsGcse *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   int i;

   // #ifndef AVOID_CCMODE_COPIES
   rtx reg;
   rtx_insn *insn;
   //  #endif
   memset (self->x_can_copy, 0, mtcs_mode_get_number/*!NUM_MACHINE_MODES*/(mtcsMode));

   mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
   for (i = 0; i < mtcs_mode_get_number/*!NUM_MACHINE_MODES*/(mtcsMode); i++)
      if (GET_MODE_CLASS (i) == MODE_CC){
         if(mtcs_config_ifdef(mtcsConfig,MTCS_AVOID_CCMODE_COPIES))
            //#ifdef AVOID_CCMODE_COPIES
            self->x_can_copy[i] = 0;
            //#else
         else{
            reg = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,(machine_mode) i,
                  mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg) + 1);
            insn = mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_rtx_SET (reg, reg));
            if (mtcs_recog_recog/*!recog*/(mtcsRecog,PATTERN (insn), insn, NULL) >= 0)
               self->x_can_copy[i] = 1;
         }
         //#endif
      }else
         self->x_can_copy[i] = 1;

   mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
}

/* Returns whether the mode supports reg/reg copy operations.  */
//原型 can_copy_p rtl.h gcse.cc
bool mtcs_gcse_can_copy_p (MtcsGcse *self,machine_mode mode)
{
   if (! self->x_can_copy_init_p){
      compute_can_copy(self);
      self->x_can_copy_init_p = true;
   }
   n_debug("mtcsgcse.c mtcs_gcse_can_copy_p --mode:%d MAX_NUM_MACHINE_MODES:%d\n",mode,MAX_NUM_MACHINE_MODES);
   return self->x_can_copy[mode] != 0;
}

/* Cover function to xmalloc to record bytes allocated.  */

static void *gmalloc (MtcsGcse *self,size_t size)
{
  self->bytes_used += size;
  return xmalloc (size);
}

/* Cover function to xcalloc to record bytes allocated.  */

static void *gcalloc (MtcsGcse *self,size_t nelem, size_t elsize)
{
  self->bytes_used += nelem * elsize;
  return xcalloc (nelem, elsize);
}

/* Cover function to obstack_alloc.  */

static void *gcse_alloc (MtcsGcse *self,unsigned long size)
{
  self->bytes_used += size;
  return obstack_alloc (&self->gcse_obstack, size);
}

/* Allocate memory for the reg/memory set tracking tables.
   This is called at the start of each pass.  */

static void alloc_gcse_mem (MtcsGcse *self)
{
   /* Allocate vars to track sets of regs.  */
   self->reg_set_bitmap = ALLOC_REG_SET (NULL);

   /* Allocate array to keep a list of insns which modify memory in each
   basic block.  The two typedefs are needed to work around the
   pre-processor limitation with template types in macro arguments.  */
   typedef vec<rtx_insn *> vec_rtx_heap;
   typedef vec<modify_pair> vec_modify_pair_heap;
   self->modify_mem_list = GCNEWVEC (vec_rtx_heap, last_basic_block_for_fn (cfun));
   self->canon_modify_mem_list = GCNEWVEC (vec_modify_pair_heap,last_basic_block_for_fn (cfun));
   self->modify_mem_list_set = BITMAP_ALLOC (NULL);
   self->blocks_with_calls = BITMAP_ALLOC (NULL);
}

/* Free memory allocated by alloc_gcse_mem.  */
static void free_gcse_mem (MtcsGcse *self)
{
  FREE_REG_SET (self->reg_set_bitmap);
  free_modify_mem_tables(self);
  BITMAP_FREE (self->modify_mem_list_set);
  BITMAP_FREE (self->blocks_with_calls);
}

/* Compute the local properties of each recorded expression.

   Local properties are those that are defined by the block, irrespective of
   other blocks.

   An expression is transparent in a block if its operands are not modified
   in the block.

   An expression is computed (locally available) in a block if it is computed
   at least once and expression would contain the same value if the
   computation was moved to the end of the block.

   An expression is locally anticipatable in a block if it is computed at
   least once and expression would contain the same value if the computation
   was moved to the beginning of the block.

   We call this routine for pre and code hoisting.  They all compute
   basically the same information and thus can easily share this code.

   TRANSP, COMP, and ANTLOC are destination sbitmaps for recording local
   properties.  If NULL, then it is not necessary to compute or record that
   particular property.

   TABLE controls which hash table to look at.  */

static void compute_local_properties (MtcsGcse *self,sbitmap *transp, sbitmap *comp, sbitmap *antloc,
           struct gcse_hash_table_d *table)
{
   unsigned int i;

   /* Initialize any bitmaps that were passed in.  */
   if (transp){
      bitmap_vector_ones (transp, last_basic_block_for_fn (cfun));
   }

   if (comp)
      bitmap_vector_clear (comp, last_basic_block_for_fn (cfun));
   if (antloc)
      bitmap_vector_clear (antloc, last_basic_block_for_fn (cfun));

   for (i = 0; i < table->size; i++){
      struct gcse_expr *expr;

      for (expr = table->table[i]; expr != NULL; expr = expr->next_same_hash){
         int indx = expr->bitmap_index;
         struct gcse_occr *occr;

         /* The expression is transparent in this block if it is not killed.
         We start by assuming all are transparent [none are killed], and
         then reset the bits for those that are.  */
         if (transp){
            compute_transp (expr->expr, indx, transp,
            self->blocks_with_calls,
            self->modify_mem_list_set,
            self->canon_modify_mem_list);
            if (self->doing_hardreg_pre_p){
               /* We also need to check whether the destination hardreg is
               set or call-clobbered in each BB.  We'll check for hardreg
               uses later.  */
               df_ref def;
               for (def = DF_REG_DEF_CHAIN (self->current_hardreg_regno);def;def = DF_REF_NEXT_REG (def))
                  bitmap_clear_bit (transp[DF_REF_BB (def)->index], indx);
            }
         }

         /* The occurrences recorded in antic_occr are exactly those that
         we want to set to nonzero in ANTLOC.  */
         if (antloc)
            for (occr = expr->antic_occr; occr != NULL; occr = occr->next){
               bitmap_set_bit (antloc[BLOCK_FOR_INSN (occr->insn)->index], indx);

               /* While we're scanning the table, this is a good place to
               initialize this.  */
               occr->deleted_p = 0;
            }

         /* The occurrences recorded in avail_occr are exactly those that
         we want to set to nonzero in COMP.  */
         if (comp)
            for (occr = expr->avail_occr; occr != NULL; occr = occr->next){
               bitmap_set_bit (comp[BLOCK_FOR_INSN (occr->insn)->index], indx);

               /* While we're scanning the table, this is a good place to
               initialize this.  */
               occr->copied_p = 0;
            }

         /* While we're scanning the table, this is a good place to
         initialize this.  */
         expr->reaching_reg = 0;
      }
   }
}


/* See whether X, the source of a set, is something we want to consider for
   GCSE.  */
static bool want_to_gcse_p (MtcsGcse *self,rtx x, machine_mode mode, HOST_WIDE_INT *max_distance_ptr)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   if(mtcs_config_ifdef(mtcsConfig,MTCS_STACK_REGS))

   //#ifdef STACK_REGS
   /* On register stack architectures, don't GCSE constants from the
   constant pool, as the benefits are often swamped by the overhead
   of shuffling the register stack between basic blocks.  */
      if (mtcs_mode_is_stack_mode/*!IS_STACK_MODE*/(mtcsMode,GET_MODE (x)))
         x = mtcs_simplify_rtx_avoid_constant_pool_reference/*!avoid_constant_pool_reference*/(mtcsSimplifyRtx,x);
   //#endif

   /* GCSE'ing constants:

   We do not specifically distinguish between constant and non-constant
   expressions in PRE and Hoist.  We use set_src_cost below to limit
   the maximum distance simple expressions can travel.

   Nevertheless, constants are much easier to GCSE, and, hence,
   it is easy to overdo the optimizations.  Usually, excessive PRE and
   Hoisting of constant leads to increased register pressure.

   RA can deal with this by rematerialing some of the constants.
   Therefore, it is important that the back-end generates sets of constants
   in a way that allows reload rematerialize them under high register
   pressure, i.e., a pseudo register with REG_EQUAL to constant
   is set only once.  Failing to do so will result in IRA/reload
   spilling such constants under high register pressure instead of
   rematerializing them.  */

   switch (GET_CODE (x)){
      case REG:
      case SUBREG:
         return self->doing_hardreg_pre_p;
      case CALL:
         return false;

      CASE_CONST_ANY:
         if (self->doing_hardreg_pre_p)
            return true;
         else if (!self->doing_code_hoisting_p)
            /* Do not PRE constants.  */
            return false;

      /* FALLTHRU */

      default:
         if (self->doing_code_hoisting_p)/* PRE doesn't implement max_distance restriction.  */{
            int cost;
            HOST_WIDE_INT max_distance;

            gcc_assert (!optimize_function_for_speed_p (cfun)
            && optimize_function_for_size_p (cfun));
            cost = mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,x, mode, 0);

            if (cost < COSTS_N_INSNS (mtcsOptionsItem->x_param_gcse_unrestricted_cost)){
               max_distance = ((HOST_WIDE_INT)mtcsOptionsItem->x_param_gcse_cost_distance_ratio * cost) / 10;
               if (max_distance == 0)
               return false;

               gcc_assert (max_distance > 0);
            }else
               max_distance = 0;

            if (max_distance_ptr)
               *max_distance_ptr = max_distance;
         }

         return mtcs_gcse_can_assign_to_reg_without_clobbers_p/*!can_assign_to_reg_without_clobbers_p*/(self,x, mode);
   }
}

/* Return true if we can assign X to a pseudo register of mode MODE
   such that the resulting insn does not result in clobbering a hard
   register as a side-effect.

   Additionally, if the target requires it, check that the resulting insn
   can be copied.  If it cannot, this means that X is special and probably
   has hidden side-effects we don't want to mess with.

   This function is typically used by code motion passes, to verify
   that it is safe to insert an insn without worrying about clobbering
   maybe live hard regs.  */
//原型 can_assign_to_reg_without_clobbers_p rtl.h gcse.cc
bool mtcs_gcse_can_assign_to_reg_without_clobbers_p (MtcsGcse *self,rtx x, machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   int num_clobbers = 0;
   int icode;
   bool can_assign = false;

   /* If this is a valid operand, we are OK.  If it's VOIDmode, we aren't.  */
   if (mtcs_preds_general_operand/*!general_operand*/(mtcsPreds,x, mode))
      return true;
   else if (GET_MODE (x) == VOIDmode)
      return false;

   /* Otherwise, check if we can make a valid insn from it.  First initialize
   our test insn if we haven't already.  */
   if (self->test_insn == 0){
      self->test_insn = mtcs_emit_make_insn_raw/*!make_insn_raw*/(mtcsEmit,
            gen_rtx_SET (mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mtcsMode->word_mode,
            mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg) * 2),const0_rtx));
      SET_NEXT_INSN (self->test_insn) = SET_PREV_INSN (self->test_insn) = 0;
      INSN_LOCATION (self->test_insn) = UNKNOWN_LOCATION;
   }

   /* Now make an insn like the one we would make when GCSE'ing and see if
   valid.  */
   mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,SET_DEST (PATTERN (self->test_insn)), mode);
   SET_SRC (PATTERN (self->test_insn)) = x;

   icode = mtcs_recog_recog/*!recog*/(mtcsRecog,PATTERN (self->test_insn), self->test_insn, &num_clobbers);

   /* If the test insn is valid and doesn't need clobbers, and the target also
   has no objections, we're good.  */
   if (icode >= 0
   && (num_clobbers == 0 || !mtcs_recog_added_clobbers_hard_reg_p/*!added_clobbers_hard_reg_p*/(mtcsRecog,icode))
   && ! (mtcsTarget/*!targetm.cannot_copy_insn_p*/->cannot_copy_insn_p
   && mtcsTarget/*!targetm.cannot_copy_insn_p*/->cannot_copy_insn_p(mtcsTarget,self->test_insn)))
      can_assign = true;

   /* Make sure test_insn doesn't have any pointers into GC space.  */
   SET_SRC (PATTERN (self->test_insn)) = NULL_RTX;

   return can_assign;
}

static inline bool do_load_motion (MtcsGcse *self)
{
   return flag_gcse_lm && !self->doing_hardreg_pre_p;
}

/* Return true if the operands of expression X are unchanged from the
   start of INSN's basic block up to but not including INSN
   (if AVAIL_P == false), or from INSN to the end of INSN's basic block
   (if AVAIL_P == true).  */
static bool oprs_unchanged_p (MtcsGcse *self,const_rtx x, const rtx_insn *insn, bool avail_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   int i, j;
   enum rtx_code code;
   const char *fmt;

   if (x == 0)
      return true;

   code = GET_CODE (x);
   switch (code){
      case REG:
      {
         struct reg_avail_info *info = &self->reg_avail_info[REGNO (x)];

         if (info->last_bb != self->current_bb)
            return true;
         if (avail_p)
            return info->last_set < DF_INSN_LUID (insn);
         else
            return info->first_set >= DF_INSN_LUID (insn);
      }

      case MEM:
        // fprintf(stderr,"mtcsgcse.c oprs_unchanged_p 00 avail_p:%d\n",avail_p);
         //mtcs_print_rtl_single(stderr,x);
         if (! mtcsOptionsItem->x_flag_gcse_lm  || load_killed_in_block_p(self,self->current_bb, DF_INSN_LUID (insn),x, avail_p))
            return false;
         else
            return oprs_unchanged_p(self,XEXP (x, 0), insn, avail_p);

      case PRE_DEC:
      case PRE_INC:
      case POST_DEC:
      case POST_INC:
      case PRE_MODIFY:
      case POST_MODIFY:
         return false;

      case PC:
      case CONST:
      CASE_CONST_ANY:
      case SYMBOL_REF:
      case LABEL_REF:
      case ADDR_VEC:
      case ADDR_DIFF_VEC:
      return true;

      default:
         break;
   }

   for (i = GET_RTX_LENGTH (code) - 1, fmt = GET_RTX_FORMAT (code); i >= 0; i--){
      if (fmt[i] == 'e'){
         /* If we are about to do the last recursive call needed at this
         level, change it into iteration.  This function is called enough
         to be worth it.  */
         if (i == 0)
            return oprs_unchanged_p(self,XEXP (x, i), insn, avail_p);
         else if (! oprs_unchanged_p(self,XEXP (x, i), insn, avail_p))
            return false;
      }else if (fmt[i] == 'E')
            for (j = 0; j < XVECLEN (x, i); j++)
               if (! oprs_unchanged_p(self,XVECEXP (x, i, j), insn, avail_p))
                  return false;
   }

   return true;
}

/* Info passed from load_killed_in_block_p to mems_conflict_for_gcse_p.  */

struct mem_conflict_info
{
  /* A memory reference for a load instruction, mems_conflict_for_gcse_p will
     see if a memory store conflicts with this memory load.  */
  const_rtx mem;

  /* True if mems_conflict_for_gcse_p finds a conflict between two memory
     references.  */
  bool conflict;
  MtcsGcse *mtcsGcse;
};

/* DEST is the output of an instruction.  If it is a memory reference and
   possibly conflicts with the load found in DATA, then communicate this
   information back through DATA.  */

static void mems_conflict_for_gcse_p (rtx dest, const_rtx setter ATTRIBUTE_UNUSED,void *userData)
{
   struct mem_conflict_info *mci = (struct mem_conflict_info *) userData;
   MtcsGcse *self=mci->mtcsGcse;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);

   while (GET_CODE (dest) == SUBREG
   || GET_CODE (dest) == ZERO_EXTRACT
   || GET_CODE (dest) == STRICT_LOW_PART)
      dest = XEXP (dest, 0);

   /* If DEST is not a MEM, then it will not conflict with the load.  Note
   that function calls are assumed to clobber memory, but are handled
   elsewhere.  */
   if (! MEM_P (dest))
      return;

   /* If we are setting a MEM in our list of specially recognized MEMs,
   don't mark as killed this time.  */
   if (self->pre_ldst_mems != NULL && expr_equiv_p(self,dest, mci->mem)){
      if (!find_rtx_in_ldst(self,dest))
         mci->conflict = true;
      return;
   }
   //fprintf(stderr,"mtcsgcse.c mems_conflict_for_gcse_p 00\n");
   if (true_dependence (dest, GET_MODE (dest), mci->mem))
      mci->conflict = true;
   //if (mtcs_alias_true_dependence/*!true_dependence*/(mtcsAlias,dest, GET_MODE (dest), mci->mem))
     // mci->conflict = true;
}

/* Return true if the expression in X (a memory reference) is killed
   in block BB before or after the insn with the LUID in UID_LIMIT.
   AVAIL_P is true for kills after UID_LIMIT, and zero for kills
   before UID_LIMIT.

   To check the entire block, set UID_LIMIT to max_uid + 1 and
   AVAIL_P to false.  */

static bool load_killed_in_block_p (MtcsGcse *self,const_basic_block bb, int uid_limit, const_rtx x,
         bool avail_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   vec<rtx_insn *> list = self->modify_mem_list[bb->index];
   rtx_insn *setter;
   unsigned ix;

   /* If this is a readonly then we aren't going to be changing it.  */
   if (MEM_READONLY_P (x))
      return false;

   FOR_EACH_VEC_ELT_REVERSE (list, ix, setter){
      struct mem_conflict_info mci;

      /* Ignore entries in the list that do not apply.  */
      if ((avail_p  && DF_INSN_LUID (setter) < uid_limit) || (! avail_p   && DF_INSN_LUID (setter) > uid_limit))
         continue;

      /* If SETTER is a call everything is clobbered.  Note that calls
      to pure functions are never put on the list, so we need not
      worry about them.  */
      if (CALL_P (setter))
         return true;

      /* SETTER must be an INSN of some kind that sets memory.  Call
      note_stores to examine each hunk of memory that is modified.  */
      mci.mem = x;
      mci.conflict = false;
      mci.mtcsGcse=self;
      mtcs_rtlanal_note_stores/*!note_stores*/(mtcsRtlanal,setter, mems_conflict_for_gcse_p, &mci);
      if (mci.conflict)
         return true;
   }
   return false;
}

/* Return true if the operands of expression X are unchanged from
   the start of INSN's basic block up to but not including INSN.  */

static bool oprs_anticipatable_p (MtcsGcse *self,const_rtx x, const rtx_insn *insn)
{
  return oprs_unchanged_p(self,x, insn, false);
}

/* Return true if the operands of expression X are unchanged from
   INSN to the end of INSN's basic block.  */

static bool oprs_available_p (MtcsGcse *self,const_rtx x, const rtx_insn *insn)
{
  return oprs_unchanged_p(self,x, insn, true);
}

/* Hash expression X.

   MODE is only used if X is a CONST_INT.  DO_NOT_RECORD_P is a boolean
   indicating if a volatile operand is found or if the expression contains
   something we don't want to insert in the table.  HASH_TABLE_SIZE is
   the current size of the hash table to be probed.  */

static unsigned int hash_expr (MtcsGcse *self,const_rtx x, machine_mode mode, int *do_not_record_p,
      int hash_table_size)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCse *mtcsCse=mtcs_target_get_cse(mtcsTarget);

   unsigned int hash;
   *do_not_record_p = 0;
   hash = mtcs_cse_hash_rtx/*!hash_rtx*/(mtcsCse,x, mode, do_not_record_p, NULL, /*have_reg_qty=*/false);
   return hash % hash_table_size;
}

/* Return true if exp1 is equivalent to exp2.  */

static bool expr_equiv_p (MtcsGcse *self,const_rtx x, const_rtx y)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCse *mtcsCse=mtcs_target_get_cse(mtcsTarget);

   return mtcs_cse_exp_equiv_p/*!exp_equiv_p*/(mtcsCse,x, y, 0, true);
}

/* Insert expression X in INSN in the hash TABLE.
   If it is already present, record it as the last occurrence in INSN's
   basic block.

   MODE is the mode of the value X is being stored into.
   It is only used if X is a CONST_INT.

   ANTIC_P is true if X is an anticipatable expression.
   AVAIL_P is true if X is an available expression.

   MAX_DISTANCE is the maximum distance in instructions this expression can
   be moved.  */

static void insert_expr_in_table (MtcsGcse *self,rtx x, machine_mode mode, rtx_insn *insn,
            bool antic_p, bool avail_p, HOST_WIDE_INT max_distance,
            struct gcse_hash_table_d *table)
{
   bool found;
   int do_not_record_p;
   unsigned int hash;
   struct gcse_expr *cur_expr, *last_expr = NULL;
   struct gcse_occr *antic_occr, *avail_occr;

   hash = hash_expr(self,x, mode, &do_not_record_p, table->size);

   /* Do not insert expression in table if it contains volatile operands,
   or if hash_expr determines the expression is something we don't want
   to or can't handle.  */
   if (do_not_record_p)
   return;

   cur_expr = table->table[hash];
   found = false;

   while (cur_expr && (found = expr_equiv_p(self,cur_expr->expr, x)) == 0){
      /* If the expression isn't found, save a pointer to the end of
      the list.  */
      last_expr = cur_expr;
      cur_expr = cur_expr->next_same_hash;
   }

   if (! found){
      cur_expr = GOBNEW (struct gcse_expr);
      self->bytes_used += sizeof (struct gcse_expr);
      if (table->table[hash] == NULL)
         /* This is the first pattern that hashed to this index.  */
         table->table[hash] = cur_expr;
      else
         /* Add EXPR to end of this hash chain.  */
         last_expr->next_same_hash = cur_expr;

      /* Set the fields of the expr element.  */
      cur_expr->expr = x;
      cur_expr->bitmap_index = table->n_elems++;
      cur_expr->next_same_hash = NULL;
      cur_expr->antic_occr = NULL;
      cur_expr->avail_occr = NULL;
      gcc_assert (max_distance >= 0);
      cur_expr->max_distance = max_distance;
   }else
      gcc_assert (cur_expr->max_distance == max_distance);

   /* Now record the occurrence(s).  */
   if (antic_p){
      antic_occr = cur_expr->antic_occr;

      if (antic_occr && BLOCK_FOR_INSN (antic_occr->insn) != BLOCK_FOR_INSN (insn))
         antic_occr = NULL;

      if (antic_occr)
         /* Found another instance of the expression in the same basic block.
         Prefer the currently recorded one.  We want the first one in the
         block and the block is scanned from start to end.  */
         ; /* nothing to do */
      else{
         /* First occurrence of this expression in this basic block.  */
         antic_occr = GOBNEW (struct gcse_occr);
         self->bytes_used += sizeof (struct gcse_occr);
         antic_occr->insn = insn;
         antic_occr->next = cur_expr->antic_occr;
         antic_occr->deleted_p = 0;
         cur_expr->antic_occr = antic_occr;
      }
   }

   if (avail_p){
      avail_occr = cur_expr->avail_occr;

      if (avail_occr && BLOCK_FOR_INSN (avail_occr->insn) == BLOCK_FOR_INSN (insn)){
         /* Found another instance of the expression in the same basic block.
         Prefer this occurrence to the currently recorded one.  We want
         the last one in the block and the block is scanned from start
         to end.  */
         avail_occr->insn = insn;
      }else{
         /* First occurrence of this expression in this basic block.  */
         avail_occr = GOBNEW (struct gcse_occr);
         self->bytes_used += sizeof (struct gcse_occr);
         avail_occr->insn = insn;
         avail_occr->next = cur_expr->avail_occr;
         avail_occr->deleted_p = 0;
         cur_expr->avail_occr = avail_occr;
      }
   }
}

/* Scan SET present in INSN and add an entry to the hash TABLE.  */

static void hash_scan_set (MtcsGcse *self,rtx set, rtx_insn *insn, struct gcse_hash_table_d *table)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   int firstPseudoRegister= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   rtx src = SET_SRC (set);
   rtx dest = SET_DEST (set);
   rtx note;

   if (GET_CODE (src) == CALL)
      hash_scan_call (src, insn, table);
   else if (REG_P (dest)){
      unsigned int regno = REGNO (dest);
      HOST_WIDE_INT max_distance = 0;

      /* See if a REG_EQUAL note shows this equivalent to a simpler expression.

      This allows us to do a single GCSE pass and still eliminate
      redundant constants, addresses or other expressions that are
      constructed with multiple instructions.

      However, keep the original SRC if INSN is a simple reg-reg move.
      In this case, there will almost always be a REG_EQUAL note on the
      insn that sets SRC.  By recording the REG_EQUAL value here as SRC
      for INSN, we miss copy propagation opportunities and we perform the
      same PRE GCSE operation repeatedly on the same REG_EQUAL value if we
      do more than one PRE GCSE pass.

      Note that this does not impede profitable constant propagations.  We
      "look through" reg-reg sets in lookup_avail_set.  */
      note = find_reg_equal_equiv_note (insn);
      if (note != 0  && REG_NOTE_KIND (note) == REG_EQUAL  && !REG_P (src)
      && want_to_gcse_p(self,XEXP (note, 0), GET_MODE (dest), NULL))
         src = XEXP (note, 0), set = gen_rtx_SET (dest, src);

      /* Only record sets of pseudo-regs in the hash table.  */
      if ((self->doing_hardreg_pre_p ? regno == self->current_hardreg_regno: regno >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
      /* Don't GCSE something if we can't do a reg/reg copy.  */
      && mtcs_gcse_can_copy_p/*!can_copy_p*/(self,GET_MODE (dest))
      /* GCSE commonly inserts instruction after the insn.  We can't
      do that easily for EH edges so disable GCSE on these for now.  */
      /* ??? We can now easily create new EH landing pads at the
      gimple level, for splitting edges; there's no reason we
      can't do the same thing at the rtl level.  */
      && !can_throw_internal (insn)
      /* Is SET_SRC something we want to gcse?  */
      && want_to_gcse_p(self,src, GET_MODE (dest), &max_distance)
      /* Don't CSE a nop.  */
      && ! mtcs_rtlanal_set_noop_p/*!set_noop_p*/(mtcsRtlanal,set)
      /* Don't GCSE if it has attached REG_EQUIV note.
      At this point this only function parameters should have
      REG_EQUIV notes and if the argument slot is used somewhere
      explicitly, it means address of parameter has been taken,
      so we should not extend the lifetime of the pseudo.  */
      && (note == NULL_RTX || ! MEM_P (XEXP (note, 0)))){
         /* An expression is not anticipatable if its operands are
         modified before this insn or if this is not the only SET in
         this insn.  The latter condition does not have to mean that
         SRC itself is not anticipatable, but we just will not be
         able to handle code motion of insns with multiple sets.  */
         bool antic_p = (oprs_anticipatable_p(self,src, insn) && !multiple_sets (insn));
         if (self->doing_hardreg_pre_p){
            /* An hardreg assignment is only available if the hardreg is
            not set later in the BB.  Uses of the hardreg are allowed. */
            auto info = self->reg_avail_info[self->current_hardreg_regno];
            if ((info.last_bb == self->current_bb  && info.first_set < DF_INSN_LUID (insn))
            || bitmap_bit_p (DF_LR_IN (self->current_bb), self->current_hardreg_regno))
               antic_p = false;
         }

         /* An expression is not available if its operands are
         subsequently modified, including this insn.  It's also not
         available if this is a branch, because we can't insert
         a set after the branch.  */
         bool avail_p = (oprs_available_p(self,src, insn) && ! JUMP_P (insn));
         if (self->doing_hardreg_pre_p){
            /* An hardreg assignment is only available if the hardreg is
            not set later in the BB.  Uses of the hardreg are allowed. */
            auto info = self->reg_avail_info[self->current_hardreg_regno];
            if (info.last_bb == self->current_bb && info.last_set > DF_INSN_LUID (insn))
               avail_p = false;
         }
         insert_expr_in_table(self,src, GET_MODE (dest), insn, antic_p, avail_p, max_distance, table);
      }
   }
   /* In case of store we want to consider the memory value as available in
   the REG stored in that memory. This makes it possible to remove
   redundant loads from due to stores to the same location.  */
   else if (mtcsOptionsItem->x_flag_gcse_las  && !self->doing_hardreg_pre_p && REG_P (src) && MEM_P (dest)){
      unsigned int regno = REGNO (src);
      HOST_WIDE_INT max_distance = 0;

      /* Only record sets of pseudo-regs in the hash table.  */
      if (regno >= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/
      /* Don't GCSE something if we can't do a reg/reg copy.  */
      && mtcs_gcse_can_copy_p/*!can_copy_p*/(self,GET_MODE (src))
      /* GCSE commonly inserts instruction after the insn.  We can't
      do that easily for EH edges so disable GCSE on these for now.  */
      && !can_throw_internal (insn)
      /* Is SET_DEST something we want to gcse?  */
      && want_to_gcse_p(self,dest, GET_MODE (dest), &max_distance)
      /* Don't CSE a nop.  */
      && ! mtcs_rtlanal_set_noop_p/*!set_noop_p*/(mtcsRtlanal,set)
      /* Don't GCSE if it has attached REG_EQUIV note.
      At this point this only function parameters should have
      REG_EQUIV notes and if the argument slot is used somewhere
      explicitly, it means address of parameter has been taken,
      so we should not extend the lifetime of the pseudo.  */
      && ((note = find_reg_note (insn, REG_EQUIV, NULL_RTX)) == 0 || ! MEM_P (XEXP (note, 0)))){
         /* Stores are never anticipatable.  */
         bool antic_p = 0;
         /* An expression is not available if its operands are
         subsequently modified, including this insn.  It's also not
         available if this is a branch, because we can't insert
         a set after the branch.  */
         bool avail_p = oprs_available_p(self,dest, insn) && ! JUMP_P (insn);

         /* Record the memory expression (DEST) in the hash table.  */
         insert_expr_in_table(self,dest, GET_MODE (dest), insn, antic_p, avail_p, max_distance, table);
      }
   }
}

static void hash_scan_clobber (rtx x ATTRIBUTE_UNUSED, rtx_insn *insn ATTRIBUTE_UNUSED,
         struct gcse_hash_table_d *table ATTRIBUTE_UNUSED)
{
  /* Currently nothing to do.  */
}

static void hash_scan_call (rtx x ATTRIBUTE_UNUSED, rtx_insn *insn ATTRIBUTE_UNUSED,
      struct gcse_hash_table_d *table ATTRIBUTE_UNUSED)
{
  /* Currently nothing to do.  */
}

/* Process INSN and add hash table entries as appropriate.  */

static void hash_scan_insn (MtcsGcse *self,rtx_insn *insn, struct gcse_hash_table_d *table)
{
   rtx pat = PATTERN (insn);
   int i;

   /* Pick out the sets of INSN and for other forms of instructions record
   what's been modified.  */
   if (GET_CODE (pat) == SET)
      hash_scan_set(self,pat, insn, table);
   else if (GET_CODE (pat) == CLOBBER)
      hash_scan_clobber (pat, insn, table);
   else if (GET_CODE (pat) == CALL)
      hash_scan_call (pat, insn, table);
   else if (GET_CODE (pat) == PARALLEL)
      for (i = 0; i < XVECLEN (pat, 0); i++){
         rtx x = XVECEXP (pat, 0, i);
         if (GET_CODE (x) == SET)
            hash_scan_set(self,x, insn, table);
         else if (GET_CODE (x) == CLOBBER)
            hash_scan_clobber (x, insn, table);
         else if (GET_CODE (x) == CALL)
            hash_scan_call (x, insn, table);
      }
}

/* Dump the hash table TABLE to file FILE under the name NAME.  */
//不移植
static void dump_hash_table (FILE *file, const char *name, struct gcse_hash_table_d *table)
{
   int i;
   /* Flattened out table, so it's printed in proper order.  */
   struct gcse_expr **flat_table;
   unsigned int *hash_val;
   struct gcse_expr *expr;

   flat_table = XCNEWVEC (struct gcse_expr *, table->n_elems);
   hash_val = XNEWVEC (unsigned int, table->n_elems);

   for (i = 0; i < (int) table->size; i++)
      for (expr = table->table[i]; expr != NULL; expr = expr->next_same_hash){
         flat_table[expr->bitmap_index] = expr;
         hash_val[expr->bitmap_index] = i;
      }

   fprintf (file, "%s hash table (%d buckets, %d entries)\n",name, table->size, table->n_elems);

   for (i = 0; i < (int) table->n_elems; i++)
      if (flat_table[i] != 0){
         expr = flat_table[i];
         fprintf (file, "Index %d (hash value %d; max distance "
               HOST_WIDE_INT_PRINT_DEC ")\n  ", expr->bitmap_index, hash_val[i], expr->max_distance);
         print_rtl (file, expr->expr);
         fprintf (file, "\n");
      }

   fprintf (file, "\n");

   free (flat_table);
   free (hash_val);
}

/* Record register first/last/block set information for REGNO in INSN.

   first_set records the first place in the block where the register
   is set and is used to compute "anticipatability".

   last_set records the last place in the block where the register
   is set and is used to compute "availability".

   last_bb records the block for which first_set and last_set are
   valid, as a quick test to invalidate them.  */
static void record_last_reg_set_info (MtcsGcse *self,rtx_insn *insn, int regno)
{
   struct reg_avail_info *info = &self->reg_avail_info[regno];
   int luid = DF_INSN_LUID (insn);

   info->last_set = luid;
   if (info->last_bb != self->current_bb){
      info->last_bb = self->current_bb;
      info->first_set = luid;
   }
}

/* Record memory modification information for INSN.  We do not actually care
   about the memory location(s) that are set, or even how they are set (consider
   a CALL_INSN).  We merely need to record which insns modify memory.  */

static void record_last_mem_set_info (MtcsGcse *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   if (! do_load_motion(self))
     return;

   mtcs_gcse_record_last_mem_set_info_common/*!record_last_mem_set_info_common*/(self,insn, self->modify_mem_list,
            self->canon_modify_mem_list,
            self->modify_mem_list_set,
            self->blocks_with_calls);
}

/* Called from compute_hash_table via note_stores to handle one
   SET or CLOBBER in an insn.  DATA is really the instruction in which
   the SET is taking place.  */

typedef struct _RcordLastData{
   MtcsGcse *mtcsGcse;
   rtx_insn *x;
}RecordLataData;

//替换 record_last_set_info
static void recordLastSetInfo_cb (rtx dest, const_rtx setter ATTRIBUTE_UNUSED, void *userData)
{
   RecordLataData *data=(RecordLataData *)userData;
   MtcsGcse *self=data->mtcsGcse;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);

   rtx_insn *last_set_insn = (rtx_insn *) data->x;

   if (GET_CODE (dest) == SUBREG)
      dest = SUBREG_REG (dest);

   if (REG_P (dest))
      record_last_reg_set_info(self, last_set_insn, REGNO (dest));
   else if (MEM_P (dest)
   /* Ignore pushes, they clobber nothing.  */
   && ! mtcs_preds_push_operand/*!push_operand*/(mtcsPreds,dest, GET_MODE (dest)))
      record_last_mem_set_info(self,last_set_insn);
}

/* Top level function to create an expression hash table.

   Expression entries are placed in the hash table if
   - they are of the form (set (pseudo-reg) src),
   - src is something we want to perform GCSE on,
   - none of the operands are subsequently modified in the block

   Currently src must be a pseudo-reg or a const_int.

   TABLE is the table computed.  */
static void compute_hash_table_work (MtcsGcse *self,struct gcse_hash_table_d *table)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsFuncAbi *mtcsFuncAbi=mtcs_target_get_func_abi(mtcsTarget);
   MtcsReg *mtcsReg =mtcs_target_get_reg(mtcsTarget);

   int i;
   int maxRegNum=mtcs_func_max_reg_num(mtcsFunc);

   /* re-Cache any INSN_LIST nodes we have allocated.  */
   clear_modify_mem_tables(self);
   /* Some working arrays used to track first and last set in each block.  */
   self->reg_avail_info = GNEWVEC (struct reg_avail_info, maxRegNum/*!max_reg_num ()*/);

   for (i = 0; i <  maxRegNum/*!max_reg_num ()*/; ++i)
      self->reg_avail_info[i].last_bb = NULL;

   FOR_EACH_BB_FN (self->current_bb, cfun){
      rtx_insn *insn;
      unsigned int regno;

      /* First pass over the instructions records information used to
      determine when registers and memory are first and last set.  */
      FOR_BB_INSNS (self->current_bb, insn){
         if (!NONDEBUG_INSN_P (insn))
            continue;

         if (CALL_P (insn)){
            hard_reg_set_iterator hrsi;
            n_debug("mtcsgcse.c compute_hash_table_work 11\n");
            /* We don't track modes of hard registers, so we need
            to be conservative and assume that partial kills
            are full kills.  */
            HardRegSet /*!HARD_REG_SET*/ callee_clobbers  =
                  mtcs_func_abi_insn_callee_abi/*!insn_callee_abi*/(mtcsFuncAbi,insn).full_and_partial_reg_clobbers ();
            MTCS_EXECUTE_IF_SET_IN_HARD_REG_SET/*!EXECUTE_IF_SET_IN_HARD_REG_SET*/(mtcsReg,&callee_clobbers, 0, regno, hrsi)
               record_last_reg_set_info(self,insn, regno);

            if (! RTL_CONST_OR_PURE_CALL_P (insn) || RTL_LOOPING_CONST_OR_PURE_CALL_P (insn)  || can_throw_external (insn))
               record_last_mem_set_info(self,insn);
         }
         RecordLataData userData={self,insn};
         mtcs_rtlanal_note_stores/*!note_stores*/(mtcsRtlanal,insn,
               recordLastSetInfo_cb/*!record_last_set_info*/, (void*)&userData/*!insn*/);
      }

      /* The next pass builds the hash table.  */
      FOR_BB_INSNS (self->current_bb, insn)
         if (NONDEBUG_INSN_P (insn))
            hash_scan_insn(self,insn, table);
   }

   free (self->reg_avail_info);
   self->reg_avail_info = NULL;
}

/* Allocate space for the set/expr hash TABLE.
   It is used to determine the number of buckets to use.  */

static void alloc_hash_table (MtcsGcse *self,struct gcse_hash_table_d *table)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   int n;
   n =mtcs_func_get_max_insn_count/*!get_max_insn_count*/(mtcsFunc);
   table->size = n / 4;
   if (table->size < 11)
      table->size = 11;
   /* Attempt to maintain efficient use of hash table.
   Making it an odd number is simplest for now.
   ??? Later take some measurements.  */
   table->size |= 1;
   n = table->size * sizeof (struct gcse_expr *);
   table->table = GNEWVAR (struct gcse_expr *, n);
}

/* Free things allocated by alloc_hash_table.  */
//不移植
static void free_hash_table (struct gcse_hash_table_d *table)
{
  free (table->table);
}

/* Compute the expression hash table TABLE.  */

static void compute_hash_table (MtcsGcse *self,struct gcse_hash_table_d *table)
{
   /* Initialize count of number of entries in hash table.  */
   table->n_elems = 0;
   memset (table->table, 0, table->size * sizeof (struct gcse_expr *));
   compute_hash_table_work(self,table);
}

/* Expression tracking support.  */
/* Clear self->canon_modify_mem_list and modify_mem_list tables.  */
static void clear_modify_mem_tables (MtcsGcse *self)
{
   unsigned i;
   bitmap_iterator bi;
   EXECUTE_IF_SET_IN_BITMAP (self->modify_mem_list_set, 0, i, bi){
      self->modify_mem_list[i].release ();
      self->canon_modify_mem_list[i].release ();
   }
   bitmap_clear (self->modify_mem_list_set);
   bitmap_clear (self->blocks_with_calls);
}

/* Release memory used by self->modify_mem_list_set.  */

static void free_modify_mem_tables (MtcsGcse *self)
{
  clear_modify_mem_tables(self);
  free (self->modify_mem_list);
  free (self->canon_modify_mem_list);
  self->modify_mem_list = 0;
  self->canon_modify_mem_list = 0;
}



/* Allocate vars used for PRE analysis.  */
static void alloc_pre_mem (MtcsGcse *self,int n_blocks, int n_exprs)
{
  self->transp = sbitmap_vector_alloc (n_blocks, n_exprs);
  self->comp = sbitmap_vector_alloc (n_blocks, n_exprs);
  self->antloc = sbitmap_vector_alloc (n_blocks, n_exprs);

  self->pre_optimal = NULL;
  self->pre_redundant = NULL;
  self->pre_insert_map = NULL;
  self->pre_delete_map = NULL;
  self->ae_kill = sbitmap_vector_alloc (n_blocks, n_exprs);

  /* pre_insert and pre_delete are allocated later.  */
}

/* Free vars used for PRE analysis.  */
static void free_pre_mem (MtcsGcse *self)
{
   sbitmap_vector_free (self->transp);
   sbitmap_vector_free (self->comp);
   /* ANTLOC and AE_KILL are freed just after pre_lcm finishes.  */
   if (self->pre_optimal)
      sbitmap_vector_free (self->pre_optimal);
   if (self->pre_redundant)
      sbitmap_vector_free (self->pre_redundant);
   if (self->pre_insert_map)
      sbitmap_vector_free (self->pre_insert_map);
   if (self->pre_delete_map)
      sbitmap_vector_free (self->pre_delete_map);

   self->transp = self->comp = NULL;
   self->pre_optimal = self->pre_redundant = self->pre_insert_map = self->pre_delete_map = NULL;
}

/* Remove certain expressions from anticipatable and transparent
   sets of basic blocks that have incoming abnormal edge.
   For PRE remove potentially trapping expressions to avoid placing
   them on abnormal edges.  For hoisting remove memory references that
   can be clobbered by calls.  */
static void prune_expressions (MtcsGcse *self,bool pre_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   struct gcse_expr *expr;
   unsigned int ui;
   basic_block bb;

   auto_sbitmap prune_exprs (self->expr_hash_table.n_elems);
   bitmap_clear (prune_exprs);
   for (ui = 0; ui < self->expr_hash_table.size; ui++){
      for (expr = self->expr_hash_table.table[ui]; expr; expr = expr->next_same_hash){
         /* Note potentially trapping expressions.  */
         if (mtcs_rtlanal_may_trap_p/*!may_trap_p*/(mtcsRtlanal,expr->expr)){
            bitmap_set_bit (prune_exprs, expr->bitmap_index);
            continue;
         }

         if (!pre_p && contains_mem_rtx_p (expr->expr))
         /* Note memory references that can be clobbered by a call.
         We do not split abnormal edges in hoisting, so would
         a memory reference get hoisted along an abnormal edge,
         it would be placed /before/ the call.  Therefore, only
         constant memory references can be hoisted along abnormal
         edges.  */
         {
            rtx x = expr->expr;

            /* Common cases where we might find the MEM which may allow us
            to avoid pruning the expression.  */
            while (GET_CODE (x) == ZERO_EXTEND || GET_CODE (x) == SIGN_EXTEND)
               x = XEXP (x, 0);

            /* If we found the MEM, go ahead and look at it to see if it has
            properties that allow us to avoid pruning its expression out
            of the tables.  */
            if (MEM_P (x)){
               if (GET_CODE (XEXP (x, 0)) == SYMBOL_REF  && CONSTANT_POOL_ADDRESS_P (XEXP (x, 0)))
                  continue;

               if (MEM_READONLY_P (x) && !MEM_VOLATILE_P (x) && MEM_NOTRAP_P (x))
               /* Constant memory reference, e.g., a PIC address.  */
                  continue;
            }
            /* ??? Optimally, we would use interprocedural alias
            analysis to determine if this mem is actually killed
            by this call.  */
            bitmap_set_bit (prune_exprs, expr->bitmap_index);
         }
      }
   }

   FOR_EACH_BB_FN (bb, cfun){
      edge e;
      edge_iterator ei;

      /* If the current block is the destination of an abnormal edge, we
      kill all trapping (for PRE) and memory (for hoist) expressions
      because we won't be able to properly place the instruction on
      the edge.  So make them neither anticipatable nor transparent.
      This is fairly conservative.

      ??? For hoisting it may be necessary to check for set-and-jump
      instructions here, not just for abnormal edges.  The general problem
      is that when an expression cannot not be placed right at the end of
      a basic block we should account for any side-effects of a subsequent
      jump instructions that could clobber the expression.  It would
      be best to implement this check along the lines of
      should_hoist_expr_to_dom where the target block is already known
      and, hence, there's no need to conservatively prune expressions on
      "intermediate" set-and-jump instructions.  */
      FOR_EACH_EDGE (e, ei, bb->preds)
         if ((e->flags & EDGE_ABNORMAL)  && (pre_p || CALL_P (BB_END (e->src)))) {
            bitmap_and_compl (self->antloc[bb->index], self->antloc[bb->index], prune_exprs);
            bitmap_and_compl (self->transp[bb->index],  self->transp[bb->index], prune_exprs);
            break;
         }
   }
}

/* It may be necessary to insert a large number of insns on edges to
   make the existing occurrences of expressions fully redundant.  This
   routine examines the set of insertions and deletions and if the ratio
   of insertions to deletions is too high for a particular expression, then
   the expression is removed from the insertion/deletion sets.

   N_ELEMS is the number of elements in the hash table.  */
static void prune_insertions_deletions (MtcsGcse *self,int n_elems)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   sbitmap_iterator sbi;

   /* We always use I to iterate over blocks/edges and J to iterate over
   expressions.  */
   unsigned int i, j;

   /* Counts for the number of times an expression needs to be inserted and
   number of times an expression can be removed as a result.  */
   int *insertions = GCNEWVEC (int, n_elems);
   int *deletions = GCNEWVEC (int, n_elems);

   /* Set of expressions which require too many insertions relative to
   the number of deletions achieved.  We will prune these out of the
   insertion/deletion sets.  */
   auto_sbitmap prune_exprs (n_elems);
   bitmap_clear (prune_exprs);

   /* Iterate over the edges counting the number of times each expression
   needs to be inserted.  */
   for (i = 0; i < (unsigned) n_edges_for_fn (cfun); i++){
      EXECUTE_IF_SET_IN_BITMAP (self->pre_insert_map[i], 0, j, sbi)
            insertions[j]++;
   }

   /* Similarly for deletions, but those occur in blocks rather than on
   edges.  */
   for (i = 0; i < (unsigned) last_basic_block_for_fn (cfun); i++){
      EXECUTE_IF_SET_IN_BITMAP (self->pre_delete_map[i], 0, j, sbi)
            deletions[j]++;
   }

   /* Now that we have accurate counts, iterate over the elements in the
   hash table and see if any need too many insertions relative to the
   number of evaluations that can be removed.  If so, mark them in
   PRUNE_EXPRS.  */
   for (j = 0; j < (unsigned) n_elems; j++)
      if (deletions[j] && (insertions[j] / deletions[j]) > mtcsOptionsItem->x_param_max_gcse_insertion_ratio)
         bitmap_set_bit (prune_exprs, j);

   /* Now prune PRE_INSERT_MAP and PRE_DELETE_MAP based on PRUNE_EXPRS.  */
   EXECUTE_IF_SET_IN_BITMAP (prune_exprs, 0, j, sbi){
      for (i = 0; i < (unsigned) n_edges_for_fn (cfun); i++)
         bitmap_clear_bit (self->pre_insert_map[i], j);

      for (i = 0; i < (unsigned) last_basic_block_for_fn (cfun); i++)
         bitmap_clear_bit (self->pre_delete_map[i], j);
   }

   free (insertions);
   free (deletions);
}

/* A hardreg set is not transparent in a block if there are any uses of that
   hardreg.  This filters the results of compute_local_properties, after the
   result of that function has been used to define the kills bitmap.

   TRANSP is the destination sbitmap to be updated.

   TABLE controls which hash table to look at.  */

static void prune_hardreg_uses (MtcsGcse *self,sbitmap *transp, struct gcse_hash_table_d *table)
{
   unsigned int i;
   gcc_assert (self->doing_hardreg_pre_p);

   for (i = 0; i < table->size; i++){
      struct gcse_expr *expr;

      for (expr = table->table[i]; expr != NULL; expr = expr->next_same_hash){
         int indx = expr->bitmap_index;
         df_ref def;

         for (def = DF_REG_USE_CHAIN (self->current_hardreg_regno);  def; def = DF_REF_NEXT_REG (def))
            bitmap_clear_bit (transp[DF_REF_BB (def)->index], indx);
      }
   }
}


/* Top level routine to do the dataflow analysis needed by PRE.  */

static struct edge_list *compute_pre_data (MtcsGcse *self)
{
   struct edge_list *edge_list;
   basic_block bb;

   compute_local_properties(self,self->transp, self->comp, self->antloc, &self->expr_hash_table);
   prune_expressions(self,true);
   bitmap_vector_clear (self->ae_kill, last_basic_block_for_fn (cfun));
   /* Compute ae_kill for each basic block using:

   ~(TRANSP | COMP)
   */
   FOR_EACH_BB_FN (bb, cfun){
      bitmap_ior (self->ae_kill[bb->index], self->transp[bb->index], self->comp[bb->index]);
      bitmap_not (self->ae_kill[bb->index], self->ae_kill[bb->index]);
   }
   if (self->doing_hardreg_pre_p)
     prune_hardreg_uses(self,self->transp, &self->expr_hash_table);

   edge_list = pre_edge_lcm (self->expr_hash_table.n_elems, self->transp, self->comp, self->antloc,
         self->ae_kill, &self->pre_insert_map, &self->pre_delete_map);
   sbitmap_vector_free (self->antloc);
   self->antloc = NULL;
   sbitmap_vector_free (self->ae_kill);
   self->ae_kill = NULL;
   prune_insertions_deletions(self,self->expr_hash_table.n_elems);
   return edge_list;
}

/* PRE utilities */

/* Return true if an occurrence of expression EXPR in OCCR_BB would reach
   block BB.

   VISITED is a pointer to a working buffer for tracking which BB's have
   been visited.  It is NULL for the top-level call.

   We treat reaching expressions that go through blocks containing the same
   reaching expression as "not reaching".  E.g. if EXPR is generated in blocks
   2 and 3, INSN is in block 4, and 2->3->4, we treat the expression in block
   2 as not reaching.  The intent is to improve the probability of finding
   only one reaching expression and to reduce register lifetimes by picking
   the closest such expression.  */

static bool pre_expr_reaches_here_p_work (MtcsGcse *self,basic_block occr_bb, struct gcse_expr *expr,
               basic_block bb, char *visited)
{
   edge pred;
   edge_iterator ei;

   FOR_EACH_EDGE (pred, ei, bb->preds){
      basic_block pred_bb = pred->src;

      if (pred->src == ENTRY_BLOCK_PTR_FOR_FN (cfun) /* Has predecessor has already been visited?  */
      || visited[pred_bb->index])
         ;/* Nothing to do.  */

      /* Does this predecessor generate this expression?  */
      else if (bitmap_bit_p (self->comp[pred_bb->index], expr->bitmap_index)){
         /* Is this the occurrence we're looking for?
         Note that there's only one generating occurrence per block
         so we just need to check the block number.  */
         if (occr_bb == pred_bb)
            return true;

         visited[pred_bb->index] = 1;
      }
      /* Ignore this predecessor if it kills the expression.  */
      else if (! bitmap_bit_p (self->transp[pred_bb->index], expr->bitmap_index))
         visited[pred_bb->index] = 1;
      /* Neither gen nor kill.  */
      else{
         visited[pred_bb->index] = 1;
         if (pre_expr_reaches_here_p_work(self,occr_bb, expr, pred_bb, visited))
            return true;
      }
   }
   /* All paths have been checked.  */
   return false;
}

/* The wrapper for pre_expr_reaches_here_work that ensures that any
   memory allocated for that function is returned.  */

static bool pre_expr_reaches_here_p (MtcsGcse *self,basic_block occr_bb, struct gcse_expr *expr,
          basic_block bb)
{
   bool rval;
   char *visited = XCNEWVEC (char, last_basic_block_for_fn (cfun));
   rval = pre_expr_reaches_here_p_work(self,occr_bb, expr, bb, visited);
   free (visited);
   return rval;
}

/* Generate RTL to copy an EXP to REG and return it.  */
//原型 prepare_copy_insn rtl.h gcse.cc
rtx_insn * mtcs_gcse_prepare_copy_insn (MtcsGcse *self,rtx reg, rtx exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   rtx_insn *pat;

   mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
   /* If the expression is something that's an operand, like a constant,
   just copy it to a register.  */
   if (mtcs_preds_general_operand/*!general_operand*/(mtcsPreds,exp, GET_MODE (reg)))
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,reg, exp);

   /* Otherwise, make a new insn to compute this expression and make sure the
   insn will be recognized (this also adds any needed CLOBBERs).  */
   else{
      rtx_insn *insn = mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_rtx_SET (reg, exp));

      if (mtcs_recog_insn_invalid_p/*!insn_invalid_p*/(mtcsRecog,insn, false))
         gcc_unreachable ();
   }

   pat = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   return pat;
}

/* Generate RTL to copy an EXPR to its `reaching_reg' and return it.  */

static rtx_insn * process_insert_insn (MtcsGcse *self,struct gcse_expr *expr)
{
   rtx reg = expr->reaching_reg;
   /* Copy the expression to make sure we don't have any sharing issues.  */
   rtx exp = copy_rtx (expr->expr);

   return mtcs_gcse_prepare_copy_insn/*!prepare_copy_insn*/(self,reg, exp);
}

/* Return the INSN which is added at the end of the block BB with
   same instruction pattern with PAT.  */

rtx_insn * insert_insn_end_basic_block (MtcsGcse *self,rtx_insn *pat, basic_block bb)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   rtx_insn *insn = BB_END (bb);
   rtx_insn *new_insn;
   rtx_insn *pat_end;

   gcc_assert (pat && INSN_P (pat));

   pat_end = pat;
   while (NEXT_INSN (pat_end) != NULL_RTX)
      pat_end = NEXT_INSN (pat_end);

   /* If the last insn is a jump, insert EXPR in front.  Similarly we need to
   take care of trapping instructions in presence of non-call exceptions.  */

   if (JUMP_P (insn) || (NONJUMP_INSN_P (insn)
   && (!single_succ_p (bb) || single_succ_edge (bb)->flags & EDGE_ABNORMAL))){
   /* FIXME: What if something in jump uses value set in new insn?  */
      new_insn = mtcs_emit_emit_insn_before_noloc/*!emit_insn_before_noloc*/(mtcsEmit,pat, insn, bb);
   }
   /* Likewise if the last insn is a call, as will happen in the presence
   of exception handling.  */
   else if (CALL_P (insn) && (!single_succ_p (bb) || single_succ_edge (bb)->flags & EDGE_ABNORMAL)){
      /* Keeping in mind targets with small register classes and parameters
      in registers, we search backward and place the instructions before
      the first parameter is loaded.  Do this for everyone for consistency
      and a presumption that we'll get better code elsewhere as well.  */

      /* Since different machines initialize their parameter registers
      in different orders, assume nothing.  Collect the set of all
      parameter registers.  */
      insn = find_first_parameter_load (insn, BB_HEAD (bb));

      /* If we found all the parameter loads, then we want to insert
      before the first parameter load.

      If we did not find all the parameter loads, then we might have
      stopped on the head of the block, which could be a CODE_LABEL.
      If we inserted before the CODE_LABEL, then we would be putting
      the insn in the wrong basic block.  In that case, put the insn
      after the CODE_LABEL.  Also, respect NOTE_INSN_BASIC_BLOCK.  */
      while (LABEL_P (insn) || NOTE_INSN_BASIC_BLOCK_P (insn))
         insn = NEXT_INSN (insn);

      new_insn = mtcs_emit_emit_insn_before_noloc/*!emit_insn_before_noloc*/(mtcsEmit,pat, insn, bb);
   }else
      new_insn = mtcs_emit_emit_insn_after_noloc/*!emit_insn_after_noloc*/(mtcsEmit,pat, insn, bb);

   while (1){
      if (INSN_P (pat))
         add_label_notes(self,PATTERN (pat), new_insn);
      if (pat == pat_end)
         break;
      pat = NEXT_INSN (pat);
   }
   return new_insn;
}

/* Return the INSN which is added at the start of the block BB with
   same instruction pattern with PAT.  */

static rtx_insn *insert_insn_start_basic_block (MtcsGcse *self,rtx_insn *pat, basic_block bb)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   rtx_insn *insn = BB_HEAD (bb);
   rtx_insn *next_insn;

   gcc_assert (pat && INSN_P (pat));

   /* Insert after the last initial CODE_LABEL or NOTE_INSN_BASIC_BLOCK, before
   any other instructions.  */
   while ((next_insn = NEXT_INSN (insn))
   && (LABEL_P (next_insn) || NOTE_INSN_BASIC_BLOCK_P (insn)))
      insn = next_insn;

   rtx_insn *new_insn = mtcs_emit_emit_insn_after_noloc/*!emit_insn_after_noloc*/(mtcsEmit,pat, insn, bb);

   while (pat != NULL_RTX){
      if (INSN_P (pat))
         add_label_notes(self,PATTERN (pat), new_insn);
      pat = NEXT_INSN (pat);
   }

   return new_insn;
}

/* Add EXPR to the start of basic block BB.

   This is used by hardreg PRE.  */

static void insert_insn_start_basic_block (MtcsGcse *self,struct gcse_expr *expr, basic_block bb)
{
   rtx reg = expr->reaching_reg;
   int regno = REGNO (reg);

   rtx_insn *insn = process_insert_insn(self,expr);
   rtx_insn *new_insn = insert_insn_start_basic_block(self,insn, bb);

   self->gcse_create_count++;

   if (dump_file){
      fprintf (dump_file, "hardreg PRE: start of bb %d, insn %d, ", bb->index, INSN_UID (new_insn));
      fprintf (dump_file, "copying expression %d to reg %d\n", expr->bitmap_index, regno);
   }
}

/* Add EXPR to the end of basic block BB.

   This is used by both the PRE and code hoisting.  */

static void insert_insn_end_basic_block (MtcsGcse *self,struct gcse_expr *expr, basic_block bb)
{
   rtx reg = expr->reaching_reg;
   int regno = REGNO (reg);
   rtx_insn *insn = process_insert_insn(self,expr);
   rtx_insn *new_insn = insert_insn_end_basic_block(self,insn, bb);
   self->gcse_create_count++;
   if (dump_file){
      fprintf (dump_file, "PRE/HOIST: end of bb %d, insn %d, ",bb->index, INSN_UID (new_insn));
      fprintf (dump_file, "copying expression %d to reg %d\n", expr->bitmap_index, regno);
   }
}

/* Insert partially redundant expressions on edges in the CFG to make
   the expressions fully redundant.  */

static bool pre_edge_insert (MtcsGcse *self,struct edge_list *edge_list, struct gcse_expr **index_map)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);

   int e, i, j, num_edges, set_size;
   bool did_insert = false;
   sbitmap *inserted;

   /* Where PRE_INSERT_MAP is nonzero, we add the expression on that edge
   if it reaches any of the deleted expressions.  */

   set_size = self->pre_insert_map[0]->size;
   num_edges = NUM_EDGES (edge_list);
   inserted = sbitmap_vector_alloc (num_edges, self->expr_hash_table.n_elems);
   bitmap_vector_clear (inserted, num_edges);

   for (e = 0; e < num_edges; e++){
      int indx;
      basic_block pred_bb = INDEX_EDGE_PRED_BB (edge_list, e);
      basic_block succ_bb = INDEX_EDGE_SUCC_BB (edge_list, e);

      for (i = indx = 0; i < set_size; i++, indx += SBITMAP_ELT_BITS){
         SBITMAP_ELT_TYPE insert = self->pre_insert_map[e]->elms[i];

         for (j = indx; insert && j < (int) self->expr_hash_table.n_elems; j++, insert >>= 1)
         if ((insert & 1) != 0 && index_map[j]->reaching_reg != NULL_RTX){
            struct gcse_expr *expr = index_map[j];
            struct gcse_occr *occr;

            /* Now look at each deleted occurrence of this expression.  */
            for (occr = expr->antic_occr; occr != NULL; occr = occr->next){
               if (! occr->deleted_p)
                  continue;

               /* Insert this expression on this edge if it would
               reach the deleted occurrence in BB.  */
               if (!bitmap_bit_p (inserted[e], j)){
                  rtx_insn *insn;
                  edge eg = INDEX_EDGE (edge_list, e);

                  /* We can't insert anything on an abnormal and
                  critical edge, so we insert the insn at the end of
                  the previous block. There are several alternatives
                  detailed in Morgans book P277 (sec 10.5) for
                  handling this situation.  This one is easiest for
                  now.  */

                  if (eg->flags & EDGE_ABNORMAL){
                     if (self->doing_hardreg_pre_p)
                        insert_insn_start_basic_block(self,index_map[j],succ_bb);
                     else
                        insert_insn_end_basic_block(self,index_map[j], pred_bb);
                  }else{
                     insn = process_insert_insn(self,index_map[j]);
                     mtcs_cfg_rtl_insert_insn_on_edge/*!insert_insn_on_edge*/(mtcsCfgRtl,insn, eg);
                  }

                  if (dump_file){
                     fprintf (dump_file, "PRE: edge (%d,%d), ",
                            pred_bb->index,
                            succ_bb->index);
                     fprintf (dump_file, "copy expression %d\n",expr->bitmap_index);
                  }

                  update_ld_motion_stores(self,expr);
                  bitmap_set_bit (inserted[e], j);
                  did_insert = true;
                  self->gcse_create_count++;
               }
            }
         }
      }
   }

   sbitmap_vector_free (inserted);
   return did_insert;
}

/* Copy the result of EXPR->EXPR generated by INSN to EXPR->REACHING_REG.
   Given "old_reg <- expr" (INSN), instead of adding after it
     reaching_reg <- old_reg
   it's better to do the following:
     reaching_reg <- expr
     old_reg      <- reaching_reg
   because this way copy propagation can discover additional PRE
   opportunities.  But if this fails, we try the old way.
   When "expr" is a store, i.e.
   given "MEM <- old_reg", instead of adding after it
     reaching_reg <- old_reg
   it's better to add it before as follows:
     reaching_reg <- old_reg
     MEM          <- reaching_reg.  */

static void pre_insert_copy_insn (MtcsGcse *self,struct gcse_expr *expr, rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   rtx reg = expr->reaching_reg;
   int regno = REGNO (reg);
   int indx = expr->bitmap_index;
   rtx pat = PATTERN (insn);
   rtx set, first_set;
   rtx_insn *new_insn;
   rtx old_reg;
   int i;

   /* This block matches the logic in hash_scan_insn.  */
   switch (GET_CODE (pat)){
      case SET:
         set = pat;
         break;

      case PARALLEL:
         /* Search through the parallel looking for the set whose
         source was the expression that we're interested in.  */
         first_set = NULL_RTX;
         set = NULL_RTX;
         for (i = 0; i < XVECLEN (pat, 0); i++){
            rtx x = XVECEXP (pat, 0, i);
            if (GET_CODE (x) == SET){
               /* If the source was a REG_EQUAL or REG_EQUIV note, we
               may not find an equivalent expression, but in this
               case the PARALLEL will have a single set.  */
               if (first_set == NULL_RTX)
                  first_set = x;
               if (expr_equiv_p(self,SET_SRC (x), expr->expr)){
                  set = x;
                  break;
               }
            }
         }
         gcc_assert (first_set);
         if (set == NULL_RTX)
            set = first_set;
         break;
      default:
         gcc_unreachable ();
   }

   if (REG_P (SET_DEST (set))){
      old_reg = SET_DEST (set);
      /* Check if we can modify the set destination in the original insn.  */
      n_debug("mtcsgcse.c pre_insert_copy_insn 00\n");
      mtcs_print_rtl_single(stderr,insn);
      mtcs_print_rtl_single(stderr,old_reg);
      mtcs_print_rtl_single(stderr,reg);

      if (mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, &SET_DEST (set), reg, 0)){
         new_insn = mtcs_expr_gen_move_insn/*!gen_move_insn*/(mtcsExpr,old_reg, reg);
         new_insn = mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,new_insn, insn);
      }else{
         new_insn = mtcs_expr_gen_move_insn/*!gen_move_insn*/(mtcsExpr,reg, old_reg);
         new_insn = mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,new_insn, insn);
      }
   }else /* This is possible only in case of a store to memory.  */{
      old_reg = SET_SRC (set);
      new_insn = mtcs_expr_gen_move_insn/*!gen_move_insn*/(mtcsExpr,reg, old_reg);

      /* Check if we can modify the set source in the original insn.  */
      if (mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, &SET_SRC (set), reg, 0))
         new_insn = mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,new_insn, insn);
      else
         new_insn = mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,new_insn, insn);
   }

   self->gcse_create_count++;

   if (dump_file)
      fprintf (dump_file, "PRE: bb %d, insn %d, copy expression %d in insn %d to reg %d\n",
            BLOCK_FOR_INSN (insn)->index, INSN_UID (new_insn), indx, INSN_UID (insn), regno);
}

/* Copy available expressions that reach the redundant expression
   to `reaching_reg'.  */

static void pre_insert_copies (MtcsGcse *self)
{
   unsigned int i;
   bool added_copy;
   struct gcse_expr *expr;
   struct gcse_occr *occr;
   struct gcse_occr *avail;

   /* For each available expression in the table, copy the result to
   `reaching_reg' if the expression reaches a deleted one.

   ??? The current algorithm is rather brute force.
   Need to do some profiling.  */
   for (i = 0; i < self->expr_hash_table.size; i++)
      for (expr = self->expr_hash_table.table[i]; expr; expr = expr->next_same_hash){
         /* If the basic block isn't reachable, PPOUT will be TRUE.  However,
         we don't want to insert a copy here because the expression may not
         really be redundant.  So only insert an insn if the expression was
         deleted.  This test also avoids further processing if the
         expression wasn't deleted anywhere.  */
         if (expr->reaching_reg == NULL)
            continue;

         /* Set when we add a copy for that expression.  */
         added_copy = false;

         for (occr = expr->antic_occr; occr != NULL; occr = occr->next){
            if (! occr->deleted_p)
               continue;

            for (avail = expr->avail_occr; avail != NULL; avail = avail->next){
               rtx_insn *insn = avail->insn;

               /* No need to handle this one if handled already.  */
               if (avail->copied_p)
                  continue;

               /* Don't handle this one if it's a redundant one.  */
               if (insn->deleted ())
                  continue;

               /* Or if the expression doesn't reach the deleted one.  */
               if (! pre_expr_reaches_here_p(self,BLOCK_FOR_INSN (avail->insn),expr, BLOCK_FOR_INSN (occr->insn)))
                  continue;

               added_copy = true;

               /* Copy the result of avail to reaching_reg.  */
               pre_insert_copy_insn(self,expr, insn);
               avail->copied_p = 1;
            }
         }

         if (added_copy)
            update_ld_motion_stores(self,expr);
      }
}

struct set_data
{
  rtx_insn *insn;
  const_rtx set;
  int nsets;
};

/* Increment number of sets and record set in DATA.  */
static void recordSetData_cb(rtx dest, const_rtx set, void *data)
{
   struct set_data *s = (struct set_data *)data;
   if (GET_CODE (set) == SET){
      /* We allow insns having multiple sets, where all but one are
      dead as single set insns.  In the common case only a single
      set is present, so we want to avoid checking for REG_UNUSED
      notes unless necessary.  */
      if (s->nsets == 1 && find_reg_note (s->insn, REG_UNUSED, SET_DEST (s->set)) && !side_effects_p (s->set))
         s->nsets = 0;

      if (!s->nsets){
         /* Record this set.  */
         s->nsets += 1;
         s->set = set;
      }else if (!find_reg_note (s->insn, REG_UNUSED, dest) || side_effects_p (set))
         s->nsets += 1;
   }
}

static const_rtx single_set_gcse (MtcsGcse *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   struct set_data s;
   rtx pattern;
   gcc_assert (INSN_P (insn));
   /* Optimize common case.  */
   pattern = PATTERN (insn);
   if (GET_CODE (pattern) == SET)
      return pattern;
   s.insn = insn;
   s.nsets = 0;
   mtcs_rtlanal_note_pattern_stores/*!note_pattern_stores*/(mtcsRtlanal,pattern, recordSetData_cb, &s);
   /* Considered invariant insns have exactly one set.  */
   gcc_assert (s.nsets == 1);
   return s.set;
}

/* Emit move from SRC to DEST noting the equivalence with expression computed
   in INSN.  */

static rtx_insn * gcse_emit_move_after (MtcsGcse *self,rtx dest, rtx src, rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   rtx_insn *new_rtx;
   const_rtx set = single_set_gcse(self,insn);
   rtx set2;
   rtx note;
   rtx eqv = NULL_RTX;

   /* This should never fail since we're creating a reg->reg copy
   we've verified to be valid.  */

   new_rtx = mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,
         mtcs_expr_gen_move_insn/*!gen_move_insn*/(mtcsExpr,dest, src), insn);

   /* Note the equivalence for local CSE pass.  Take the note from the old
   set if there was one.  Otherwise record the SET_SRC from the old set
   unless DEST is also an operand of the SET_SRC.  */
   set2 = single_set (new_rtx);
   if (!set2 || !rtx_equal_p (SET_DEST (set2), dest))
      return new_rtx;
   if ((note = find_reg_equal_equiv_note (insn)))
      eqv = XEXP (note, 0);
   else if (! REG_P (dest) || ! reg_mentioned_p (dest, SET_SRC (set)))
      eqv = SET_SRC (set);

   if (eqv != NULL_RTX)
      mtcs_rtl_set_unique_reg_note/*!set_unique_reg_note*/(mtcsRTL,new_rtx, REG_EQUAL, copy_insn_1 (eqv));

   return new_rtx;
}

/* Delete redundant computations.
   Deletion is done by changing the insn to copy the `reaching_reg' of
   the expression into the result of the SET.  It is left to later passes
   to propagate the copy or eliminate it.

   Return true if a change is made.  */
static bool pre_delete (MtcsGcse *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   unsigned int i;
   bool changed = false;
   struct gcse_expr *expr;
   struct gcse_occr *occr;

   for (i = 0; i < self->expr_hash_table.size; i++)
      for (expr = self->expr_hash_table.table[i]; expr; expr = expr->next_same_hash){
         int indx = expr->bitmap_index;

         /* We only need to search antic_occr since we require ANTLOC != 0.  */
         for (occr = expr->antic_occr; occr != NULL; occr = occr->next){
            rtx_insn *insn = occr->insn;
            rtx set;
            basic_block bb = BLOCK_FOR_INSN (insn);

            /* We only delete insns that have a single_set.  */
            if (bitmap_bit_p (self->pre_delete_map[bb->index], indx)
            && (set = single_set (insn)) != 0 && dbg_cnt (pre_insn)){
               rtx dest = SET_DEST (set);
               if (expr->reaching_reg == NULL){
                  if (self->doing_hardreg_pre_p)
                     /* Use the hardreg as the reaching register.  The
                     deleted sets will be replaced with noop moves.

                     This may change the value of the hardreg in some debug
                     instructions, so we will need to reset any debug uses
                     of the hardreg.  */
                     expr->reaching_reg = dest;
                  else
                     /* Create a pseudo-reg to store the result of reaching
                     expressions into.  Get the mode for the new pseudo from
                     the mode of the original destination pseudo.  */
                     expr->reaching_reg = mtcs_rtl_gen_reg_rtx_and_attrs/*!gen_reg_rtx_and_attrs*/(mtcsRTL,SET_DEST (set));
               }

               gcse_emit_move_after(self,SET_DEST (set), expr->reaching_reg, insn);
               mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,insn);
               occr->deleted_p = 1;
               changed = true;
               self->gcse_subst_count++;

               if (dump_file){
                  fprintf (dump_file,"PRE: redundant insn %d (expression %d) in ", INSN_UID (insn), indx);
                  fprintf (dump_file, "bb %d, reaching reg is %d\n", bb->index, REGNO (expr->reaching_reg));
               }
            }
         }
      }

   return changed;
}

/* Since hardreg PRE reuses the hardreg as the reaching register, we need to
   eliminate any existing uses in debug insns.  This is overly conservative,
   but there's currently no benefit to preserving the debug insns, so there's
   no point doing the work to retain them.  */
static void reset_hardreg_debug_uses (MtcsGcse *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);

   df_ref def;
   for (def = DF_REG_USE_CHAIN (self->current_hardreg_regno); def; def = DF_REF_NEXT_REG (def)){
      rtx_insn *insn = DF_REF_INSN (def);
      if (DEBUG_INSN_P (insn))
         mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,insn);
   }
}

/* Perform GCSE optimizations using PRE.
   This is called by one_pre_gcse_pass after all the dataflow analysis
   has been done.

   This is based on the original Morel-Renvoise paper Fred Chow's thesis, and
   lazy code motion from Knoop, Ruthing and Steffen as described in Advanced
   Compiler Design and Implementation.

   ??? A new pseudo reg is created to hold the reaching expression.  The nice
   thing about the classical approach is that it would try to use an existing
   reg.  If the register can't be adequately optimized [i.e. we introduce
   reload problems], one could add a pass here to propagate the new register
   through the block.

   ??? We don't handle single sets in PARALLELs because we're [currently] not
   able to copy the rest of the parallel when we insert copies to create full
   redundancies from partial redundancies.  However, there's no reason why we
   can't handle PARALLELs in the cases where there are no partial
   redundancies.  */

static bool pre_gcse (MtcsGcse *self,struct edge_list *edge_list)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);

   unsigned int i;
   bool did_insert, changed;
   struct gcse_expr **index_map;
   struct gcse_expr *expr;
   /* Compute a mapping from expression number (`bitmap_index') to
   hash table entry.  */
   index_map = XCNEWVEC (struct gcse_expr *, self->expr_hash_table.n_elems);
   for (i = 0; i < self->expr_hash_table.size; i++)
      for (expr = self->expr_hash_table.table[i]; expr; expr = expr->next_same_hash)
         index_map[expr->bitmap_index] = expr;

   /* Delete the redundant insns first so that
   - we know what register to use for the new insns and for the other
   ones with reaching expressions
   - we know which insns are redundant when we go to create copies  */
   changed = pre_delete(self);
   did_insert = pre_edge_insert(self,edge_list, index_map);
   /* In other places with reaching expressions, copy the expression to the
      specially allocated pseudo-reg that reaches the redundant expr.  This
      isn't needed for hardreg PRE.  */
   if (!self->doing_hardreg_pre_p)
      pre_insert_copies(self);
   if (did_insert){
      if (self->doing_hardreg_pre_p)
         reset_hardreg_debug_uses(self);
      mtcs_cfg_rtl_commit_edge_insertions/*!commit_edge_insertions*/(mtcsCfgRtl);
      changed = true;
   }
   free (index_map);
   return changed;
}

/* Top level routine to perform one PRE GCSE pass.

   Return true if a change was made.  */
static bool one_pre_gcse_pass (MtcsGcse *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   bool changed = false;

   self->gcse_subst_count = 0;
   self->gcse_create_count = 0;
   n_debug("mtcsgcse.c one_pre_gcse_pass 00\n");
   /* Return if there's nothing to do, or it is too expensive.  */
   if (n_basic_blocks_for_fn (cfun) <= NUM_FIXED_BLOCKS + 1
   || mtcs_gcse_gcse_or_cprop_is_too_expensive/*!gcse_or_cprop_is_too_expensive*/(self,_("PRE disabled")))
      return false;
   n_debug("mtcsgcse.c one_pre_gcse_pass 11\n");

   /* We need alias.  */
   mtcs_alias_init_alias_analysis/*!init_alias_analysis*/(mtcsAlias);

   self->bytes_used = 0;
   gcc_obstack_init (&self->gcse_obstack);
   alloc_gcse_mem(self);
   n_debug("mtcsgcse.c one_pre_gcse_pass 22\n");

   alloc_hash_table(self,&self->expr_hash_table);
   add_noreturn_fake_exit_edges ();
   if (do_load_motion(self))
      compute_ld_motion_mems(self);
   n_debug("mtcsgcse.c one_pre_gcse_pass 33\n");

   compute_hash_table(self,&self->expr_hash_table);
   n_debug("mtcsgcse.c one_pre_gcse_pass 44\n");

   if (do_load_motion(self))
      trim_ld_motion_mems(self);
   if (dump_file)
      dump_hash_table (dump_file, "Expression", &self->expr_hash_table);
   n_debug("mtcsgcse.c one_pre_gcse_pass 55\n");

   if (self->expr_hash_table.n_elems > 0){
      struct edge_list *edge_list;
      alloc_pre_mem(self,last_basic_block_for_fn (cfun), self->expr_hash_table.n_elems);
      edge_list = compute_pre_data(self);
      if (pre_gcse(self,edge_list))
         changed = true;
      free_edge_list (edge_list);
      free_pre_mem(self);
   }
   n_debug("mtcsgcse.c one_pre_gcse_pass 66\n");

   if (do_load_motion(self))
      free_ld_motion_mems(self);
   remove_fake_exit_edges ();
   free_hash_table (&self->expr_hash_table);

   free_gcse_mem(self);
   obstack_free (&self->gcse_obstack, NULL);

   /* We are finished with alias.  */
   mtcs_alias_end_alias_analysis/*!end_alias_analysis*/(mtcsAlias);

   if (dump_file){
      fprintf (dump_file, "PRE GCSE of %s, %d basic blocks, %d bytes needed, ",
            current_function_name (), n_basic_blocks_for_fn (cfun),self->bytes_used);
      fprintf (dump_file, "%d substs, %d insns created\n",self->gcse_subst_count, self->gcse_create_count);
   }

   return changed;
}

/* If X contains any LABEL_REF's, add REG_LABEL_OPERAND notes for them
   to INSN.  If such notes are added to an insn which references a
   CODE_LABEL, the LABEL_NUSES count is incremented.  We have to add
   that note, because the following loop optimization pass requires
   them.  */

/* ??? If there was a jump optimization pass after gcse and before loop,
   then we would not need to do this here, because jump would add the
   necessary REG_LABEL_OPERAND and REG_LABEL_TARGET notes.  */
static void add_label_notes (MtcsGcse *self,rtx x, rtx_insn *insn)
{
   enum rtx_code code = GET_CODE (x);
   int i, j;
   const char *fmt;

   if (code == LABEL_REF && !LABEL_REF_NONLOCAL_P (x)){
      /* This code used to ignore labels that referred to dispatch tables to
      avoid flow generating (slightly) worse code.

      We no longer ignore such label references (see LABEL_REF handling in
      mark_jump_label for additional information).  */

      /* There's no reason for current users to emit jump-insns with
      such a LABEL_REF, so we don't have to handle REG_LABEL_TARGET
      notes.  */
      gcc_assert (!JUMP_P (insn));
      add_reg_note (insn, REG_LABEL_OPERAND, label_ref_label (x));

      if (LABEL_P (label_ref_label (x)))
         LABEL_NUSES (label_ref_label (x))++;

      return;
   }

   for (i = GET_RTX_LENGTH (code) - 1, fmt = GET_RTX_FORMAT (code); i >= 0; i--){
      if (fmt[i] == 'e')
         add_label_notes(self,XEXP (x, i), insn);
      else if (fmt[i] == 'E')
         for (j = XVECLEN (x, i) - 1; j >= 0; j--)
            add_label_notes(self,XVECEXP (x, i, j), insn);
   }
}


/* ??? We could compute post dominators and run this algorithm in
   reverse to perform tail merging, doing so would probably be
   more effective than the tail merging code in jump.cc.

   It's unclear if tail merging could be run in parallel with
   code hoisting.  It would be nice.  */

/* Allocate vars used for code hoisting analysis.  */

static void alloc_code_hoist_mem (MtcsGcse *self,int n_blocks, int n_exprs)
{
   self->antloc = sbitmap_vector_alloc (n_blocks, n_exprs);
   self->transp = sbitmap_vector_alloc (n_blocks, n_exprs);
   self->comp = sbitmap_vector_alloc (n_blocks, n_exprs);

   self->hoist_vbein = sbitmap_vector_alloc (n_blocks, n_exprs);
   self->hoist_vbeout = sbitmap_vector_alloc (n_blocks, n_exprs);
}

/* Free vars used for code hoisting analysis.  */

static void free_code_hoist_mem (MtcsGcse *self)
{
   sbitmap_vector_free (self->antloc);
   sbitmap_vector_free (self->transp);
   sbitmap_vector_free (self->comp);

   sbitmap_vector_free (self->hoist_vbein);
   sbitmap_vector_free (self->hoist_vbeout);

   free_dominance_info (CDI_DOMINATORS);
}

/* Compute the very busy expressions at entry/exit from each block.

   An expression is very busy if all paths from a given point
   compute the expression.  */
static void compute_code_hoist_vbeinout (MtcsGcse *self)
{
   int changed, passes;
   basic_block bb;

   bitmap_vector_clear (self->hoist_vbeout, last_basic_block_for_fn (cfun));
   bitmap_vector_clear (self->hoist_vbein, last_basic_block_for_fn (cfun));

   passes = 0;
   changed = 1;

   while (changed){
      changed = 0;
      /* We scan the blocks in the reverse order to speed up
      the convergence.  */
      FOR_EACH_BB_REVERSE_FN (bb, cfun){
         if (bb->next_bb != EXIT_BLOCK_PTR_FOR_FN (cfun)) {
            bitmap_intersection_of_succs (self->hoist_vbeout[bb->index],self->hoist_vbein, bb);

            /* Include expressions in VBEout that are calculated
            in BB and available at its end.  */
            bitmap_ior (self->hoist_vbeout[bb->index],self->hoist_vbeout[bb->index], self->comp[bb->index]);
         }

         changed |= bitmap_or_and (self->hoist_vbein[bb->index],
         self->antloc[bb->index],
         self->hoist_vbeout[bb->index],
         self->transp[bb->index]);
      }
      passes++;
   }

   if (dump_file){
      fprintf (dump_file, "hoisting vbeinout computation: %d passes\n", passes);

      FOR_EACH_BB_FN (bb, cfun){
         fprintf (dump_file, "vbein (%d): ", bb->index);
         dump_bitmap_file (dump_file, self->hoist_vbein[bb->index]);
         fprintf (dump_file, "vbeout(%d): ", bb->index);
         dump_bitmap_file (dump_file, self->hoist_vbeout[bb->index]);
      }
   }
}

/* Top level routine to do the dataflow analysis needed by code hoisting.  */

static void compute_code_hoist_data (MtcsGcse *self)
{
   compute_local_properties(self,self->transp, self->comp, self->antloc, &self->expr_hash_table);
   prune_expressions(self,false);
   compute_code_hoist_vbeinout(self);
   calculate_dominance_info (CDI_DOMINATORS);
   if (dump_file)
      fprintf (dump_file, "\n");
}

/* Update register pressure for BB when hoisting an expression from
   instruction FROM, if live ranges of inputs are shrunk.  Also
   maintain live_in information if live range of register referred
   in FROM is shrunk.

   Return 0 if register pressure doesn't change, otherwise return
   the number by which register pressure is decreased.

   NOTE: Register pressure won't be increased in this function.  */

static int update_bb_reg_pressure (MtcsGcse *self,basic_block bb, rtx_insn *from)
{
   rtx dreg;
   rtx_insn *insn;
   basic_block succ_bb;
   df_ref use, op_ref;
   edge succ;
   edge_iterator ei;
   int decreased_pressure = 0;
   int nregs;
   enum reg_class pressure_class;

   FOR_EACH_INSN_USE (use, from){
      dreg = DF_REF_REAL_REG (use);
      /* The live range of register is shrunk only if it isn't:
      1. referred on any path from the end of this block to EXIT, or
      2. referred by insns other than FROM in this block.  */
      FOR_EACH_EDGE (succ, ei, bb->succs){
         succ_bb = succ->dest;
         if (succ_bb == EXIT_BLOCK_PTR_FOR_FN (cfun))
            continue;

         if (bitmap_bit_p (BB_DATA (succ_bb)->live_in, REGNO (dreg)))
            break;
      }
      if (succ != NULL)
         continue;

      op_ref = DF_REG_USE_CHAIN (REGNO (dreg));
      for (; op_ref; op_ref = DF_REF_NEXT_REG (op_ref)){
         if (!DF_REF_INSN_INFO (op_ref))
            continue;

         insn = DF_REF_INSN (op_ref);
         if (BLOCK_FOR_INSN (insn) == bb && NONDEBUG_INSN_P (insn) && insn != from)
            break;
      }

      pressure_class = get_regno_pressure_class(self,REGNO (dreg), &nregs);
      /* Decrease register pressure and update live_in information for
      this block.  */
      if (!op_ref && pressure_class != NO_REGS){
         decreased_pressure += nregs;
         BB_DATA (bb)->max_reg_pressure[pressure_class] -= nregs;
         bitmap_clear_bit (BB_DATA (bb)->live_in, REGNO (dreg));
      }
   }
   return decreased_pressure;
}

/* Determine if the expression EXPR should be hoisted to EXPR_BB up in
   flow graph, if it can reach BB unimpared.  Stop the search if the
   expression would need to be moved more than DISTANCE instructions.

   DISTANCE is the number of instructions through which EXPR can be
   hoisted up in flow graph.

   BB_SIZE points to an array which contains the number of instructions
   for each basic block.

   PRESSURE_CLASS and NREGS are register class and number of hard registers
   for storing EXPR.

   HOISTED_BBS points to a bitmap indicating basic blocks through which
   EXPR is hoisted.

   FROM is the instruction from which EXPR is hoisted.

   It's unclear exactly what Muchnick meant by "unimpared".  It seems
   to me that the expression must either be computed or transparent in
   *every* block in the path(s) from EXPR_BB to BB.  Any other definition
   would allow the expression to be hoisted out of loops, even if
   the expression wasn't a loop invariant.

   Contrast this to reachability for PRE where an expression is
   considered reachable if *any* path reaches instead of *all*
   paths.  */

static bool should_hoist_expr_to_dom (MtcsGcse *self,basic_block expr_bb, struct gcse_expr *expr,
           basic_block bb, sbitmap visited,
           HOST_WIDE_INT distance,
           int *bb_size, enum reg_class pressure_class,
           int *nregs, bitmap hoisted_bbs, rtx_insn *from)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);

   unsigned int i;
   edge pred;
   edge_iterator ei;
   sbitmap_iterator sbi;
   bool visited_allocated_locally = false;
   int decreased_pressure = 0;

   if (mtcsOptionsItem->x_flag_ira_hoist_pressure){
      /* Record old information of basic block BB when it is visited
      at the first time.  */
      if (!bitmap_bit_p (hoisted_bbs, bb->index)){
         struct bb_data *data = BB_DATA (bb);
         bitmap_copy (data->backup, data->live_in);
         data->old_pressure = data->max_reg_pressure[pressure_class];
      }
      decreased_pressure = update_bb_reg_pressure(self,bb, from);
   }
   /* Terminate the search if distance, for which EXPR is allowed to move,
   is exhausted.  */
   if (distance > 0){
      if (mtcsOptionsItem->x_flag_ira_hoist_pressure){
         /* Prefer to hoist EXPR if register pressure is decreased.  */
         if (decreased_pressure > *nregs)
            distance += bb_size[bb->index];
         /* Let EXPR be hoisted through basic block at no cost if one
         of following conditions is satisfied:

         1. The basic block has low register pressure.
         2. Register pressure won't be increases after hoisting EXPR.

         Constant expressions is handled conservatively, because
         hoisting constant expression aggressively results in worse
         code.  This decision is made by the observation of CSiBE
         on ARM target, while it has no obvious effect on other
         targets like x86, x86_64, mips and powerpc.  */
         else if (CONST_INT_P (expr->expr)    || (BB_DATA (bb)->max_reg_pressure[pressure_class]
         >=mtcsIra->x_ira_class_hard_regs_num[pressure_class]  && decreased_pressure < *nregs))
            distance -= bb_size[bb->index];
      }else
            distance -= bb_size[bb->index];

      if (distance <= 0)
            return 0;
   }else
      gcc_assert (distance == 0);

   if (visited == NULL){
      visited_allocated_locally = true;
      visited = sbitmap_alloc (last_basic_block_for_fn (cfun));
      bitmap_clear (visited);
   }

   FOR_EACH_EDGE (pred, ei, bb->preds){
      basic_block pred_bb = pred->src;

      if (pred->src == ENTRY_BLOCK_PTR_FOR_FN (cfun))
         break;
      else if (pred_bb == expr_bb)
         continue;
      else if (bitmap_bit_p (visited, pred_bb->index))
         continue;
      else if (! bitmap_bit_p (self->transp[pred_bb->index], expr->bitmap_index))
         break;
      /* Not killed.  */
      else{
         bitmap_set_bit (visited, pred_bb->index);
         if (! should_hoist_expr_to_dom(self,expr_bb, expr, pred_bb,
         visited, distance, bb_size, pressure_class, nregs,hoisted_bbs, from))
            break;
      }
   }
   if (visited_allocated_locally){
      /* If EXPR can be hoisted to expr_bb, record basic blocks through
      which EXPR is hoisted in hoisted_bbs.  */
      if (mtcsOptionsItem->x_flag_ira_hoist_pressure && !pred){
         /* Record the basic block from which EXPR is hoisted.  */
         bitmap_set_bit (visited, bb->index);
         EXECUTE_IF_SET_IN_BITMAP (visited, 0, i, sbi)
            bitmap_set_bit (hoisted_bbs, i);
      }
      sbitmap_free (visited);
   }

   return (pred == NULL);
}

/* Find occurrence in BB.  */

static struct gcse_occr * find_occr_in_bb (struct gcse_occr *occr, basic_block bb)
{
  /* Find the right occurrence of this expression.  */
  while (occr && BLOCK_FOR_INSN (occr->insn) != bb)
    occr = occr->next;

  return occr;
}

/* Actually perform code hoisting.

   The code hoisting pass can hoist multiple computations of the same
   expression along dominated path to a dominating basic block, like
   from b2/b3 to b1 as depicted below:

          b1      ------
          /\         |
         /  \        |
        bx   by   distance
       /      \      |
      /        \     |
     b2        b3 ------

   Unfortunately code hoisting generally extends the live range of an
   output pseudo register, which increases register pressure and hurts
   register allocation.  To address this issue, an attribute MAX_DISTANCE
   is computed and attached to each expression.  The attribute is computed
   from rtx cost of the corresponding expression and it's used to control
   how long the expression can be hoisted up in flow graph.  As the
   expression is hoisted up in flow graph, GCC decreases its DISTANCE
   and stops the hoist if DISTANCE reaches 0.  Code hoisting can decrease
   register pressure if live ranges of inputs are shrunk.

   Option "-fira-hoist-pressure" implements register pressure directed
   hoist based on upper method.  The rationale is:
     1. Calculate register pressure for each basic block by reusing IRA
   facility.
     2. When expression is hoisted through one basic block, GCC checks
   the change of live ranges for inputs/output.  The basic block's
   register pressure will be increased because of extended live
   range of output.  However, register pressure will be decreased
   if the live ranges of inputs are shrunk.
     3. After knowing how hoisting affects register pressure, GCC prefers
   to hoist the expression if it can decrease register pressure, by
   increasing DISTANCE of the corresponding expression.
     4. If hoisting the expression increases register pressure, GCC checks
   register pressure of the basic block and decrease DISTANCE only if
   the register pressure is high.  In other words, expression will be
   hoisted through at no cost if the basic block has low register
   pressure.
     5. Update register pressure information for basic blocks through
   which expression is hoisted.  */

static bool hoist_code (MtcsGcse *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   basic_block bb, dominated;
   unsigned int dom_tree_walk_index;
   unsigned int i, j, k;
   struct gcse_expr **index_map;
   struct gcse_expr *expr;
   int *to_bb_head;
   int *bb_size;
   bool changed = false;
   struct bb_data *data;
   /* Basic blocks that have occurrences reachable from BB.  */
   bitmap from_bbs;
   /* Basic blocks through which expr is hoisted.  */
   bitmap hoisted_bbs = NULL;
   bitmap_iterator bi;

   /* Compute a mapping from expression number (`bitmap_index') to
   hash table entry.  */

   index_map = XCNEWVEC (struct gcse_expr *, self->expr_hash_table.n_elems);
   for (i = 0; i < self->expr_hash_table.size; i++)
      for (expr = self->expr_hash_table.table[i]; expr; expr = expr->next_same_hash)
         index_map[expr->bitmap_index] = expr;

   /* Calculate sizes of basic blocks and note how far
   each instruction is from the start of its block.  We then use this
   data to restrict distance an expression can travel.  */

   to_bb_head = XCNEWVEC (int, mtcs_rtl_data_get_max_uid/*!get_max_uid*/(mtcsRtlData));
   bb_size = XCNEWVEC (int, last_basic_block_for_fn (cfun));

   FOR_EACH_BB_FN (bb, cfun){
      rtx_insn *insn;
      int to_head;
      to_head = 0;
      FOR_BB_INSNS (bb, insn){
         /* Don't count debug instructions to avoid them affecting
         decision choices.  */
         if (NONDEBUG_INSN_P (insn))
            to_bb_head[INSN_UID (insn)] = to_head++;
      }
      bb_size[bb->index] = to_head;
   }

   gcc_assert (EDGE_COUNT (ENTRY_BLOCK_PTR_FOR_FN (cfun)->succs) == 1
   && (EDGE_SUCC (ENTRY_BLOCK_PTR_FOR_FN (cfun), 0)->dest  == ENTRY_BLOCK_PTR_FOR_FN (cfun)->next_bb));

   from_bbs = BITMAP_ALLOC (NULL);
   if (mtcsOptionsItem->x_flag_ira_hoist_pressure)
      hoisted_bbs = BITMAP_ALLOC (NULL);

   auto_vec<basic_block> dom_tree_walk = get_all_dominated_blocks (CDI_DOMINATORS,ENTRY_BLOCK_PTR_FOR_FN (cfun)->next_bb);

   /* Walk over each basic block looking for potentially hoistable
   expressions, nothing gets hoisted from the entry block.  */
   FOR_EACH_VEC_ELT (dom_tree_walk, dom_tree_walk_index, bb){
      auto_vec<basic_block> domby = get_dominated_to_depth (CDI_DOMINATORS, bb, mtcsOptionsItem->x_param_max_hoist_depth);

      if (domby.length () == 0)
         continue;

      /* Examine each expression that is very busy at the exit of this
      block.  These are the potentially hoistable expressions.  */
      for (i = 0; i < SBITMAP_SIZE (self->hoist_vbeout[bb->index]); i++){
         if (bitmap_bit_p (self->hoist_vbeout[bb->index], i)){
            int nregs = 0;
            enum reg_class pressure_class = NO_REGS;
            /* Current expression.  */
            struct gcse_expr *expr = index_map[i];
            /* Number of occurrences of EXPR that can be hoisted to BB.  */
            int hoistable = 0;
            /* Occurrences reachable from BB.  */
            vec<occr_t> occrs_to_hoist = vNULL;
            /* We want to insert the expression into BB only once, so
            note when we've inserted it.  */
            int insn_inserted_p;
            occr_t occr;

            /* If an expression is computed in BB and is available at end of
            BB, hoist all occurrences dominated by BB to BB.  */
            if (bitmap_bit_p (self->comp[bb->index], i)){
               occr = find_occr_in_bb (expr->antic_occr, bb);

               if (occr){
                  /* An occurrence might've been already deleted
                  while processing a dominator of BB.  */
                  if (!occr->deleted_p){
                     gcc_assert (NONDEBUG_INSN_P (occr->insn));
                     hoistable++;
                  }
               }else
                  hoistable++;
            }

            /* We've found a potentially hoistable expression, now
            we look at every block BB dominates to see if it
            computes the expression.  */
            FOR_EACH_VEC_ELT (domby, j, dominated){
               HOST_WIDE_INT max_distance;
               /* Ignore self dominance.  */
               if (bb == dominated)
                  continue;
               /* We've found a dominated block, now see if it computes
               the busy expression and whether or not moving that
               expression to the "beginning" of that block is safe.  */
               if (!bitmap_bit_p (self->antloc[dominated->index], i))
                  continue;

               occr = find_occr_in_bb (expr->antic_occr, dominated);
               gcc_assert (occr);
               /* An occurrence might've been already deleted
               while processing a dominator of BB.  */
               if (occr->deleted_p)
                  continue;
               gcc_assert (NONDEBUG_INSN_P (occr->insn));

               max_distance = expr->max_distance;
               if (max_distance > 0)
                  /* Adjust MAX_DISTANCE to account for the fact that
                  OCCR won't have to travel all of DOMINATED, but
                  only part of it.  */
                  max_distance += (bb_size[dominated->index] - to_bb_head[INSN_UID (occr->insn)]);

               pressure_class = get_pressure_class_and_nregs(self,occr->insn, &nregs);
               /* Note if the expression should be hoisted from the dominated
               block to BB if it can reach DOMINATED unimpared.

               Keep track of how many times this expression is hoistable
               from a dominated block into BB.  */
               if (should_hoist_expr_to_dom(self,bb, expr, dominated, NULL,
                     max_distance, bb_size, pressure_class,   &nregs, hoisted_bbs, occr->insn)){
                  hoistable++;
                  occrs_to_hoist.safe_push (occr);
                  bitmap_set_bit (from_bbs, dominated->index);
               }
            }

            /* If we found more than one hoistable occurrence of this
            expression, then note it in the vector of expressions to
            hoist.  It makes no sense to hoist things which are computed
            in only one BB, and doing so tends to pessimize register
            allocation.  One could increase this value to try harder
            to avoid any possible code expansion due to register
            allocation issues; however experiments have shown that
            the vast majority of hoistable expressions are only movable
            from two successors, so raising this threshold is likely
            to nullify any benefit we get from code hoisting.  */
            if (hoistable > 1 && dbg_cnt (hoist_insn)){
               /* If (hoistable != vec::length), then there is
               an occurrence of EXPR in BB itself.  Don't waste
               time looking for LCA in this case.  */
               if ((unsigned) hoistable == occrs_to_hoist.length ()){
                  basic_block lca;
                  lca = nearest_common_dominator_for_set (CDI_DOMINATORS,from_bbs);
                  if (lca != bb)
                     /* Punt, it's better to hoist these occurrences to
                     LCA.  */
                     occrs_to_hoist.release ();
               }
            }else
               /* Punt, no point hoisting a single occurrence.  */
               occrs_to_hoist.release ();

            if (mtcsOptionsItem->x_flag_ira_hoist_pressure   && !occrs_to_hoist.is_empty ()){
               /* Increase register pressure of basic blocks to which
               expr is hoisted because of extended live range of
               output.  */
               data = BB_DATA (bb);
               data->max_reg_pressure[pressure_class] += nregs;
               EXECUTE_IF_SET_IN_BITMAP (hoisted_bbs, 0, k, bi){
                  data = BB_DATA (BASIC_BLOCK_FOR_FN (cfun, k));
                  data->max_reg_pressure[pressure_class] += nregs;
               }
            }else if (mtcsOptionsItem->x_flag_ira_hoist_pressure){
               /* Restore register pressure and live_in info for basic
               blocks recorded in hoisted_bbs when expr will not be
               hoisted.  */
               EXECUTE_IF_SET_IN_BITMAP (hoisted_bbs, 0, k, bi){
                  data = BB_DATA (BASIC_BLOCK_FOR_FN (cfun, k));
                  bitmap_copy (data->live_in, data->backup);
                  data->max_reg_pressure[pressure_class] = data->old_pressure;
               }
            }

            if (mtcsOptionsItem->x_flag_ira_hoist_pressure)
               bitmap_clear (hoisted_bbs);

            insn_inserted_p = 0;

            /* Walk through occurrences of I'th expressions we want
            to hoist to BB and make the transformations.  */
            FOR_EACH_VEC_ELT (occrs_to_hoist, j, occr){
               rtx_insn *insn;
               const_rtx set;
               gcc_assert (!occr->deleted_p);
               insn = occr->insn;
               set = single_set_gcse(self,insn);
               /* Create a pseudo-reg to store the result of reaching
               expressions into.  Get the mode for the new pseudo
               from the mode of the original destination pseudo.

               It is important to use new pseudos whenever we
               emit a set.  This will allow reload to use
               rematerialization for such registers.  */
               if (!insn_inserted_p)
                  expr->reaching_reg = mtcs_rtl_gen_reg_rtx_and_attrs/*!gen_reg_rtx_and_attrs*/(mtcsRTL,SET_DEST (set));

               gcse_emit_move_after(self,SET_DEST (set), expr->reaching_reg,insn);
               mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,insn);
               occr->deleted_p = 1;
               changed = true;
               self->gcse_subst_count++;

               if (!insn_inserted_p){
                  insert_insn_end_basic_block(self,expr, bb);
                  insn_inserted_p = 1;
               }
            }

            occrs_to_hoist.release ();
            bitmap_clear (from_bbs);
         }
      }
   }

   BITMAP_FREE (from_bbs);
   if (mtcsOptionsItem->x_flag_ira_hoist_pressure)
      BITMAP_FREE (hoisted_bbs);

   free (bb_size);
   free (to_bb_head);
   free (index_map);

   return changed;
}

/* Return pressure class and number of needed hard registers (through
   *NREGS) of register REGNO.  */
static enum reg_class get_regno_pressure_class (MtcsGcse *self,int regno, int *nregs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);

   if (regno >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)){
      enum reg_class pressure_class;

      pressure_class = mtcs_reg_reg_allocno_class/*!reg_allocno_class*/(mtcsReg,regno);
      pressure_class = mtcsIra->x_ira_pressure_class_translate[pressure_class];
      *nregs = mtcsIra->x_ira_reg_class_max_nregs[pressure_class]
                                                 [mtcs_rtl_data_get_pseudo_regno_mode/*!PSEUDO_REGNO_MODE*/(mtcsRtlData,regno)];
      return pressure_class;
   }else if (! mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&mtcsIra->x_ira_no_alloc_regs, regno)
   && !  mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&mtcsIra->eliminable_regset, regno)){
      *nregs = 1;
      return mtcsIra->x_ira_pressure_class_translate[mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,regno)];
   }else{
      *nregs = 0;
      return NO_REGS;
   }
}

/* Return pressure class and number of hard registers (through *NREGS)
   for destination of INSN. */
static enum reg_class get_pressure_class_and_nregs (MtcsGcse *self,rtx_insn *insn, int *nregs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);

   rtx reg;
   enum reg_class pressure_class;
   const_rtx set = single_set_gcse(self,insn);

   reg = SET_DEST (set);
   if (GET_CODE (reg) == SUBREG)
      reg = SUBREG_REG (reg);
   if (MEM_P (reg)){
      *nregs = 0;
      pressure_class = NO_REGS;
   }else{
      gcc_assert (REG_P (reg));
      pressure_class = mtcs_reg_reg_allocno_class/*!reg_allocno_class*/(mtcsReg,REGNO (reg));
      pressure_class = mtcsIra->x_ira_pressure_class_translate[pressure_class];
      *nregs =  mtcsIra->x_ira_reg_class_max_nregs[pressure_class][GET_MODE (SET_SRC (set))];
   }
   return pressure_class;
}

/* Increase (if INCR_P) or decrease current register pressure for
   register REGNO.  */
static void change_pressure (MtcsGcse *self,int regno, bool incr_p)
{
   int nregs;
   enum reg_class pressure_class;

   pressure_class = get_regno_pressure_class(self,regno, &nregs);
   if (! incr_p)
      self->curr_reg_pressure[pressure_class] -= nregs;
   else{
      self->curr_reg_pressure[pressure_class] += nregs;
      if (BB_DATA (self->curr_bb)->max_reg_pressure[pressure_class] < self->curr_reg_pressure[pressure_class])
         BB_DATA (self->curr_bb)->max_reg_pressure[pressure_class] = self->curr_reg_pressure[pressure_class];
   }
}

/* Calculate register pressure for each basic block by walking insns
   from last to first.  */
static void calculate_bb_reg_pressure (MtcsGcse *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);

   int i;
   unsigned int j;
   rtx_insn *insn;
   basic_block bb;
   bitmap curr_regs_live;
   bitmap_iterator bi;

   MtcsRegClass *mtcsRegClass =mtcs_reg_get_reg_class(mtcsReg);

   mtcs_ira_setup_eliminable_regset/*!ira_setup_eliminable_regset*/(mtcsIra);
   curr_regs_live = BITMAP_ALLOC (&reg_obstack);
   FOR_EACH_BB_FN (bb, cfun){
      self->curr_bb = bb;
      BB_DATA (bb)->live_in = BITMAP_ALLOC (NULL);
      BB_DATA (bb)->backup = BITMAP_ALLOC (NULL);
      bitmap_copy (BB_DATA (bb)->live_in, df_get_live_in (bb));
      bitmap_copy (curr_regs_live, df_get_live_out (bb));
      for (i = 0; i < mtcsIra->x_ira_pressure_classes_num; i++)
         self->curr_reg_pressure[mtcsIra->x_ira_pressure_classes/*!ira_pressure_classes*/[i]] = 0;
      EXECUTE_IF_SET_IN_BITMAP (curr_regs_live, 0, j, bi)
         change_pressure(self,j, true);

      FOR_BB_INSNS_REVERSE (bb, insn){
         rtx dreg;
         int regno;
         df_ref def, use;

         if (! NONDEBUG_INSN_P (insn))
            continue;

         FOR_EACH_INSN_DEF (def, insn){
            dreg = DF_REF_REAL_REG (def);
            gcc_assert (REG_P (dreg));
            regno = REGNO (dreg);
            if (!(DF_REF_FLAGS (def) & (DF_REF_PARTIAL | DF_REF_CONDITIONAL))){
               if (bitmap_clear_bit (curr_regs_live, regno))
                  change_pressure(self,regno, false);
            }
         }

         FOR_EACH_INSN_USE (use, insn){
            dreg = DF_REF_REAL_REG (use);
            gcc_assert (REG_P (dreg));
            regno = REGNO (dreg);
            if (bitmap_set_bit (curr_regs_live, regno))
               change_pressure(self,regno, true);
         }
      }
   }
   BITMAP_FREE (curr_regs_live);

   if (dump_file == NULL)
      return;

   fprintf (dump_file, "\nRegister Pressure: \n");
   FOR_EACH_BB_FN (bb, cfun){
      fprintf (dump_file, "  Basic block %d: \n", bb->index);
      for (i = 0; (int) i < mtcsIra->x_ira_pressure_classes_num; i++){
         enum reg_class pressure_class;

         pressure_class = mtcsIra->x_ira_pressure_classes[i];
         if (BB_DATA (bb)->max_reg_pressure[pressure_class] == 0)
            continue;

         fprintf (dump_file, "    %s=%d\n", mtcsRegClass/*!reg_class_names*/[pressure_class].name,
               BB_DATA (bb)->max_reg_pressure[pressure_class]);
      }
   }
   fprintf (dump_file, "\n");
}

/* Top level routine to perform one code hoisting (aka unification) pass

   Return true if a change was made.  */
static bool one_code_hoisting_pass (MtcsGcse *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);
   MtcsIraCosts *mtcsIraCosts = mtcs_ira_mgr_get_costs(mtcsIraMgr);

   bool changed = false;

   self->gcse_subst_count = 0;
   self->gcse_create_count = 0;

   /* Return if there's nothing to do, or it is too expensive.  */
   if (n_basic_blocks_for_fn (cfun) <= NUM_FIXED_BLOCKS + 1
   || mtcs_gcse_gcse_or_cprop_is_too_expensive/*!gcse_or_cprop_is_too_expensive*/(self,_("GCSE disabled")))
      return false;

   self->doing_code_hoisting_p = true;

   /* Calculate register pressure for each basic block.  */
   if (mtcsOptionsItem->x_flag_ira_hoist_pressure){
      mtcs_reg_regstat_init_n_sets_and_refs/*!regstat_init_n_sets_and_refs*/(mtcsReg);
      mtcs_ira_costs_set_pseudo_classes/*!ira_set_pseudo_classes*/(mtcsIraCosts,false, dump_file);
      alloc_aux_for_blocks (sizeof (struct bb_data));
      calculate_bb_reg_pressure(self);
      regstat_free_n_sets_and_refs ();
   }

   /* We need alias.  */
   mtcs_alias_init_alias_analysis/*!init_alias_analysis*/(mtcsAlias);

   self->bytes_used = 0;
   gcc_obstack_init (&self->gcse_obstack);
   alloc_gcse_mem(self);

   alloc_hash_table(self,&self->expr_hash_table);
   compute_hash_table(self,&self->expr_hash_table);
   if (dump_file)
      dump_hash_table (dump_file, "Code Hosting Expressions", &self->expr_hash_table);

   if (self->expr_hash_table.n_elems > 0){
      alloc_code_hoist_mem(self,last_basic_block_for_fn (cfun),
      self->expr_hash_table.n_elems);
      compute_code_hoist_data(self);
      changed = hoist_code(self);
      free_code_hoist_mem(self);
   }

   if (mtcsOptionsItem->x_flag_ira_hoist_pressure){
      free_aux_for_blocks ();
      mtcs_reg_free_reg_info/*!free_reg_info*/(mtcsReg);
   }
   free_hash_table (&self->expr_hash_table);
   free_gcse_mem(self);
   obstack_free (&self->gcse_obstack, NULL);

   /* We are finished with alias.  */
   mtcs_alias_end_alias_analysis/*!end_alias_analysis*/(mtcsAlias);

   if (dump_file){
      fprintf (dump_file, "HOIST of %s, %d basic blocks, %d bytes needed, ",
      current_function_name (), n_basic_blocks_for_fn (cfun),self->bytes_used);
      fprintf (dump_file, "%d substs, %d insns created\n",self->gcse_subst_count, self->gcse_create_count);
   }

   self->doing_code_hoisting_p = false;

   return changed;
}

/*  Here we provide the things required to do store motion towards the exit.
    In order for this to be effective, gcse also needed to be taught how to
    move a load when it is killed only by a store to itself.

       int i;
       float a[10];

       void foo(float scale)
       {
         for (i=0; i<10; i++)
      a[i] *= scale;
       }

    'i' is both loaded and stored to in the loop. Normally, gcse cannot move
    the load out since its live around the loop, and stored at the bottom
    of the loop.

      The 'Load Motion' referred to and implemented in this file is
    an enhancement to gcse which when using edge based LCM, recognizes
    this situation and allows gcse to move the load out of the loop.

      Once gcse has hoisted the load, store motion can then push this
    load towards the exit, and we end up with no loads or stores of 'i'
    in the loop.  */

/* This will search the ldst list for a matching expression. If it
   doesn't find one, we create one and initialize it.  */

static struct ls_expr *ldst_entry (MtcsGcse *self,rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCse *mtcsCse=mtcs_target_get_cse(mtcsTarget);

   int do_not_record_p = 0;
   struct ls_expr * ptr;
   unsigned int hash;
   ls_expr **slot;
   struct ls_expr e;

   hash =mtcs_cse_hash_rtx/*!hash_rtx*/(mtcsCse,x, GET_MODE (x), &do_not_record_p,  NULL,  /*have_reg_qty=*/false);

   e.pattern = x;
   slot = self->pre_ldst_table->find_slot_with_hash (&e, hash, INSERT);
   if (*slot)
   return *slot;

   ptr = XNEW (struct ls_expr);

   ptr->next         = self->pre_ldst_mems;
   ptr->expr         = NULL;
   ptr->pattern      = x;
   ptr->pattern_regs = NULL_RTX;
   ptr->stores.create (0);
   ptr->reaching_reg = NULL_RTX;
   ptr->invalid      = 0;
   ptr->index        = 0;
   ptr->hash_index   = hash;
   self->pre_ldst_mems     = ptr;
   *slot = ptr;

   return ptr;
}

/* Free up an individual ldst entry.  */

static void free_ldst_entry (struct ls_expr * ptr)
{
  ptr->stores.release ();
  free (ptr);
}

/* Free up all memory associated with the ldst list.  */

static void free_ld_motion_mems (MtcsGcse *self)
{
   delete self->pre_ldst_table;
   self->pre_ldst_table = NULL;
   while (self->pre_ldst_mems){
      struct ls_expr * tmp = self->pre_ldst_mems;
      self->pre_ldst_mems = self->pre_ldst_mems->next;
      free_ldst_entry (tmp);
   }
   self->pre_ldst_mems = NULL;
}

/* Dump debugging info about the ldst list.  */

static void print_ldst_list (MtcsGcse *self,FILE * file)
{
   struct ls_expr * ptr;
   fprintf (file, "LDST list: \n");
   for (ptr = self->pre_ldst_mems; ptr != NULL; ptr = ptr->next){
      fprintf (file, "  Pattern (%3d): ", ptr->index);

      print_rtl (file, ptr->pattern);
      fprintf (file, "\n   Stores : ");
      print_rtx_insn_vec (file, ptr->stores);
      fprintf (file, "\n\n");
   }
   fprintf (file, "\n");
}

/* Returns 1 if X is in the list of ldst only expressions.  */

static struct ls_expr *find_rtx_in_ldst (MtcsGcse *self,rtx x)
{
   struct ls_expr e;
   ls_expr **slot;
   if (!self->pre_ldst_table)
      return NULL;
   e.pattern = x;
   slot = self->pre_ldst_table->find_slot (&e, NO_INSERT);
   if (!slot || (*slot)->invalid)
      return NULL;
   return *slot;
}

/* Load Motion for loads which only kill themselves.  */

/* Return true if x, a MEM, is a simple access with no side effects.
   These are the types of loads we consider for the ld_motion list,
   otherwise we let the usual aliasing take care of it.  */

static bool simple_mem (MtcsGcse *self,const_rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   if (MEM_VOLATILE_P (x))
      return false;

   if (GET_MODE (x) ==mtcsMode->modes.M_BLKmode)
      return false;

   /* If we are handling exceptions, we must be careful with memory references
   that may trap.  If we are not, the behavior is undefined, so we may just
   continue.  */
   if (cfun->can_throw_non_call_exceptions && mtcs_rtlanal_may_trap_p/*!may_trap_p*/(mtcsRtlanal,x))
      return false;

   if (side_effects_p (x))
      return false;

   /* Do not consider function arguments passed on stack.  */
   if (reg_mentioned_p (mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL), x))
      return false;

   if (mtcsOptionsItem->x_flag_float_store && mtcs_mode_is_float_p/*!FLOAT_MODE_P*/(mtcsMode,GET_MODE (x)))
      return false;

   return true;
}

/* Make sure there isn't a buried reference in this pattern anywhere.
   If there is, invalidate the entry for it since we're not capable
   of fixing it up just yet.. We have to be sure we know about ALL
   loads since the aliasing code will allow all entries in the
   ld_motion list to not-alias itself.  If we miss a load, we will get
   the wrong value since gcse might common it and we won't know to
   fix it up.  */

static void invalidate_any_buried_refs (MtcsGcse *self,rtx x)
{
   const char * fmt;
   int i, j;
   struct ls_expr * ptr;
   /* Invalidate it in the list.  */
   if (MEM_P (x) && simple_mem(self,x)){
      ptr = ldst_entry(self,x);
      ptr->invalid = 1;
   }
   /* Recursively process the insn.  */
   fmt = GET_RTX_FORMAT (GET_CODE (x));
   for (i = GET_RTX_LENGTH (GET_CODE (x)) - 1; i >= 0; i--){
      if (fmt[i] == 'e')
         invalidate_any_buried_refs(self,XEXP (x, i));
      else if (fmt[i] == 'E')
         for (j = XVECLEN (x, i) - 1; j >= 0; j--)
            invalidate_any_buried_refs(self,XVECEXP (x, i, j));
   }
}

/* Find all the 'simple' MEMs which are used in LOADs and STORES.  Simple
   being defined as MEM loads and stores to symbols, with no side effects
   and no registers in the expression.  For a MEM destination, we also
   check that the insn is still valid if we replace the destination with a
   REG, as is done in update_ld_motion_stores.  If there are any uses/defs
   which don't match this criteria, they are invalidated and trimmed out
   later.  */

static void compute_ld_motion_mems (MtcsGcse *self)
{
   struct ls_expr * ptr;
   basic_block bb;
   rtx_insn *insn;

   self->pre_ldst_mems = NULL;
   self->pre_ldst_table = new hash_table<pre_ldst_expr_hasher> (13);

   FOR_EACH_BB_FN (bb, cfun){
      FOR_BB_INSNS (bb, insn){
         if (NONDEBUG_INSN_P (insn)){
            if (GET_CODE (PATTERN (insn)) == SET){
               rtx src = SET_SRC (PATTERN (insn));
               rtx dest = SET_DEST (PATTERN (insn));

               /* Check for a simple load.  */
               if (MEM_P (src) && simple_mem(self,src)){
                  ptr = ldst_entry(self,src);
                  if (!REG_P (dest))
                     ptr->invalid = 1;
               }else{
                  /* Make sure there isn't a buried load somewhere.  */
                  invalidate_any_buried_refs(self,src);
               }

               /* Check for a simple load through a REG_EQUAL note.  */
               rtx note = find_reg_equal_equiv_note (insn), src_eq;
               if (note  && REG_NOTE_KIND (note) == REG_EQUAL  && (src_eq = XEXP (note, 0))
                     && !(MEM_P (src_eq) && simple_mem(self,src_eq)))
                  invalidate_any_buried_refs(self,src_eq);

               /* Check for stores. Don't worry about aliased ones, they
               will block any movement we might do later. We only care
               about this exact pattern since those are the only
               circumstance that we will ignore the aliasing info.  */
               if (MEM_P (dest) && simple_mem(self,dest)){
                  ptr = ldst_entry(self,dest);
                  machine_mode src_mode = GET_MODE (src);
                  if (! MEM_P (src)  && GET_CODE (src) != ASM_OPERANDS
                  /* Check for REG manually since want_to_gcse_p
                  returns 0 for all REGs.  */
                  && mtcs_gcse_can_assign_to_reg_without_clobbers_p/*!can_assign_to_reg_without_clobbers_p*/(self,src,src_mode))
                     ptr->stores.safe_push (insn);
                  else
                     ptr->invalid = 1;
               }
            }else{
               /* Invalidate all MEMs in the pattern and...  */
               invalidate_any_buried_refs(self,PATTERN (insn));

               /* ...in REG_EQUAL notes for PARALLELs with single SET.  */
               rtx note = find_reg_equal_equiv_note (insn), src_eq;
               if (note  && REG_NOTE_KIND (note) == REG_EQUAL  && (src_eq = XEXP (note, 0)))
                  invalidate_any_buried_refs(self,src_eq);
            }
         }
      }
   }
}

/* Remove any references that have been either invalidated or are not in the
   expression list for pre gcse.  */

static void trim_ld_motion_mems (MtcsGcse *self)
{
   struct ls_expr * * last = & self->pre_ldst_mems;
   struct ls_expr * ptr = self->pre_ldst_mems;

   while (ptr != NULL){
      struct gcse_expr * expr;

      /* Delete if entry has been made invalid.  */
      if (! ptr->invalid){
         /* Delete if we cannot find this mem in the expression list.  */
         unsigned int hash = ptr->hash_index % self->expr_hash_table.size;

         for (expr = self->expr_hash_table.table[hash]; expr != NULL; expr = expr->next_same_hash)
            if (expr_equiv_p(self,expr->expr, ptr->pattern))
               break;
      } else
         expr = (struct gcse_expr *) 0;

      if (expr){
         /* Set the expression field if we are keeping it.  */
         ptr->expr = expr;
         last = & ptr->next;
         ptr = ptr->next;
      }else{
         *last = ptr->next;
         self->pre_ldst_table->remove_elt_with_hash (ptr, ptr->hash_index);
         free_ldst_entry(ptr);
         ptr = * last;
      }
   }

   /* Show the world what we've found.  */
   if (dump_file && self->pre_ldst_mems != NULL)
      print_ldst_list(self,dump_file);
}

/* This routine will take an expression which we are replacing with
   a reaching register, and update any stores that are needed if
   that expression is in the ld_motion list.  Stores are updated by
   copying their SRC to the reaching register, and then storing
   the reaching register into the store location. These keeps the
   correct value in the reaching register for the loads.  */

static void update_ld_motion_stores (MtcsGcse *self,struct gcse_expr * expr)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);

   struct ls_expr * mem_ptr;

   if ((mem_ptr = find_rtx_in_ldst(self,expr->expr))){
      /* We can try to find just the REACHED stores, but is shouldn't
      matter to set the reaching reg everywhere...  some might be
      dead and should be eliminated later.  */

      /* We replace (set mem expr) with (set reg expr) (set mem reg)
      where reg is the reaching reg used in the load.  We checked in
      compute_ld_motion_mems that we can replace (set mem expr) with
      (set reg expr) in that insn.  */
      rtx_insn *insn;
      unsigned int i;
      FOR_EACH_VEC_ELT_REVERSE (mem_ptr->stores, i, insn){
         rtx pat = PATTERN (insn);
         rtx src = SET_SRC (pat);
         rtx reg = expr->reaching_reg;

         /* If we've already copied it, continue.  */
         if (expr->reaching_reg == src)
            continue;

         if (dump_file){
            fprintf (dump_file, "PRE:  store updated with reaching reg ");
            print_rtl (dump_file, reg);
            fprintf (dump_file, ":\n   ");
            mtcs_print_inline_rtx/*!print_inline_rtx*/(dump_file, insn, 8);
            fprintf (dump_file, "\n");
         }

         rtx_insn *copy = mtcs_expr_gen_move_insn/*!gen_move_insn*/(mtcsExpr,reg, copy_rtx (SET_SRC (pat)));
         mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,copy, insn);
         SET_SRC (pat) = reg;
         mtcs_dfscan_df_insn_rescan/*!df_insn_rescan*/(mtcsDfscan,insn);

         /* un-recognize this pattern since it's probably different now.  */
         INSN_CODE (insn) = -1;
         self->gcse_create_count++;
      }
   }
}

/* Return true if the graph is too expensive to optimize. PASS is the
   optimization about to be performed.  */
//原型 gcse_or_cprop_is_too_expensive gcse.h gcse.cc
bool mtcs_gcse_gcse_or_cprop_is_too_expensive (MtcsGcse *self,const char *pass)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   unsigned HOST_WIDE_INT memory_request = ((unsigned HOST_WIDE_INT)n_basic_blocks_for_fn (cfun)
               * SBITMAP_SET_SIZE (max_reg_num ()) * sizeof (SBITMAP_ELT_TYPE));

   /* Trying to perform global optimizations on flow graphs which have
   a high connectivity will take a long time and is unlikely to be
   particularly useful.

   In normal circumstances a cfg should have about twice as many
   edges as blocks.  But we do not want to punish small functions
   which have a couple switch statements.  Rather than simply
   threshold the number of blocks, uses something with a more
   graceful degradation.  */
   if (n_edges_for_fn (cfun) > 20000 + n_basic_blocks_for_fn (cfun) * 4){
      warning (OPT_Wdisabled_optimization,
         "%s: %d basic blocks and %d edges/basic block",
         pass, n_basic_blocks_for_fn (cfun),
         n_edges_for_fn (cfun) / n_basic_blocks_for_fn (cfun));

      return true;
   }

   /* If allocating memory for the dataflow bitmaps would take up too much
   storage it's better just to disable the optimization.  */
   if (memory_request / 1024 > (unsigned HOST_WIDE_INT)mtcsOptionsItem->x_param_max_gcse_memory){
      warning (OPT_Wdisabled_optimization,
         "%s: %d basic blocks and %d registers; "
         "increase %<--param max-gcse-memory%> above %wu",
         pass, n_basic_blocks_for_fn (cfun), max_reg_num (),
         memory_request / 1024);

      return true;
   }
   return false;
}

static unsigned int execute_rtl_pre (MtcsGcse *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsCfgCleanup *mtcsCfgCleanup=mtcs_target_get_cfg_cleanup(mtcsTarget);

   int changed;
   mtcs_cfg_cleanup_delete_unreachable_blocks/*!delete_unreachable_blocks*/(mtcsCfgCleanup);
   mtcs_dfcore_df_analyze/*!df_analyze*/(mtcsDfcore);
   n_debug("mtcsgcse.c execute_rtl_pre 00\n");
   changed = one_pre_gcse_pass(self);
   flag_rerun_cse_after_global_opts |= changed;
   if (changed)
      mtcs_cfg_cleanup_cleanup_cfg/*!cleanup_cfg*/(mtcsCfgCleanup,0);
   return 0;
}

static unsigned int execute_rtl_hoist (MtcsGcse *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsCfgCleanup *mtcsCfgCleanup=mtcs_target_get_cfg_cleanup(mtcsTarget);

   int changed;
   mtcs_cfg_cleanup_delete_unreachable_blocks/*!delete_unreachable_blocks*/(mtcsCfgCleanup);
   mtcs_dfcore_df_analyze/*!df_analyze*/(mtcsDfcore);
   changed = one_code_hoisting_pass(self);
   flag_rerun_cse_after_global_opts |= changed;
   if (changed)
      mtcs_cfg_cleanup_cleanup_cfg/*!cleanup_cfg*/(mtcsCfgCleanup,0);
   return 0;
}

/* Reset all state within gcse.cc so that we can rerun the compiler
   within the same process.  For use by toplev::finalize.  */
//原型 gcse_cc_finalize gcse.h gcse.cc
void mtcs_gcse_cc_finalize (MtcsGcse *self)
{
  self->test_insn = NULL;
}

typedef struct _NoteStoresCallBackData
{
  rtx_insn *insn;
  vec<modify_pair> *canon_mem_list;
  MtcsGcse *mtcsGcse;
}NoteStoresCallBackData;

static void canon_list_insert_cb (rtx dest, const_rtx x ATTRIBUTE_UNUSED, void *userData)
{
   NoteStoresCallBackData *data=(NoteStoresCallBackData *)userData;
   MtcsGcse *self=data->mtcsGcse;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);

   rtx dest_addr;
   int bb;
   modify_pair pair;

   while (GET_CODE (dest) == SUBREG
   || GET_CODE (dest) == ZERO_EXTRACT
   || GET_CODE (dest) == STRICT_LOW_PART)
      dest = XEXP (dest, 0);

   /* If DEST is not a MEM, then it will not conflict with a load.  Note
   that function calls are assumed to clobber memory, but are handled
   elsewhere.  */

   if (! MEM_P (dest))
      return;

   dest_addr = mtcs_alias_get_addr/*!get_addr*/(mtcsAlias,XEXP (dest, 0));
   dest_addr = mtcs_alias_canon_rtx/*!canon_rtx*/(mtcsAlias,dest_addr);
   rtx_insn *insn =data->insn;
   bb = BLOCK_FOR_INSN (insn)->index;

   pair.dest = dest;
   pair.dest_addr = dest_addr;
   vec<modify_pair> *canon_mem_list = data->canon_mem_list;
   canon_mem_list[bb].safe_push (pair);
}




/* Record memory modification information for INSN.  We do not actually care
   about the memory location(s) that are set, or even how they are set (consider
   a CALL_INSN).  We merely need to record which insns modify memory.  */
//原型 record_last_mem_set_info_common gcse-common.h gcse-common.cc
void mtcs_gcse_record_last_mem_set_info_common (MtcsGcse *self,rtx_insn *insn,
             vec<rtx_insn *> *modify_mem_list,
             vec<modify_pair> *canon_modify_mem_list,
             bitmap modify_mem_list_set,
             bitmap blocks_with_calls)

{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   int bb;

   bb = BLOCK_FOR_INSN (insn)->index;
   modify_mem_list[bb].safe_push (insn);
   bitmap_set_bit (modify_mem_list_set, bb);

   if (CALL_P (insn))
      bitmap_set_bit (blocks_with_calls, bb);
   else{
      NoteStoresCallBackData userData;
      userData.insn = insn;
      userData.canon_mem_list = canon_modify_mem_list;
      userData.mtcsGcse=self;
      mtcs_rtlanal_note_stores/*!note_stores*/(mtcsRtlanal,insn, canon_list_insert_cb, (void*) &userData);
   }
}

static void mtcsGcseInit(MtcsGcse *self)
{
   self->pre_ldst_mems = NULL;
   self->doing_code_hoisting_p = false;
   self->doing_hardreg_pre_p = false;
}

MtcsGcse *mtcs_gcse_new(MtcsMode *mtcsMode)
{
   MtcsGcse *self = n_slice_alloc0 (sizeof(MtcsGcse));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcsGcseInit(self);
   return self;
}

/************************以下是基于MtcsGcse的pass-----------------------------*/
//原型 NEXT_PASS (pass_rtl_pre, 1); RTL_PASS gcse.cc rtl pre  y 有条件执行 return optimize > 0 && flag_gcse...
static nboolean rtl_pre_gate_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   return mtcsOptionsItem->x_optimize>0 && mtcsOptionsItem->x_flag_gcse
         && !fun->calls_setjmp && optimize_function_for_speed_p (fun) && dbg_cnt (pre);
}

static nuint rtl_pre_execute_cb(MtcsPass *mtcsPass,function *func)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlPassMgr *mtcsRtlPassMgr=mtcs_target_get_rtl_pass_mgr(mtcsTarget);
   MtcsGcse *mtcsGcse=mtcs_rtl_pass_mgr_get_gcse(mtcsRtlPassMgr);
   return execute_rtl_pre (mtcsGcse);
}

static void mtcsPassRtlPreInit(MtcsPassRtlPre *self)
{
   MtcsPass *mtcsPass= (MtcsPass *)self;
   mtcsPass->execute =rtl_pre_execute_cb;
   mtcsPass->gate =rtl_pre_gate_cb;
   mtcs_pass_set_properties(mtcsPass,
      PROP_cfglayout,/* properties_required */
      0, /* properties_provided */
      0 /* properties_destroyed */);
   mtcs_pass_set_todo_flags(mtcsPass,
      0, /* todo_flags_start */
      TODO_df_finish /*todo_flags_finish */);
}

MtcsPassRtlPre *mtcs_pass_rtl_pre_new(MtcsMode *mtcsMode)
{
   MtcsPassRtlPre *self = n_slice_alloc0 (sizeof(MtcsPassRtlPre));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"rtl pre");
   mtcsPassRtlPreInit(self);
   return self;
}

//原型 NEXT_PASS (pass_rtl_hoist, 1); RTL_PASS gcse.cc hoist  y 有条件执行  optimize > 0 && flag_gcse...
static nboolean hoist_pre_gate_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   return mtcsOptionsItem->x_optimize > 0 && mtcsOptionsItem->x_flag_gcse
     && !fun->calls_setjmp
     /* It does not make sense to run code hoisting unless we are optimizing
        for code size -- it rarely makes programs faster, and can make then
        bigger if we did PRE (when optimizing for space, we don't run PRE).  */
     && optimize_function_for_size_p (cfun)
     && dbg_cnt (hoist);
}

static nuint hoist_execute_cb(MtcsPass *mtcsPass,function *func)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlPassMgr *mtcsRtlPassMgr=mtcs_target_get_rtl_pass_mgr(mtcsTarget);
   MtcsGcse *mtcsGcse=mtcs_rtl_pass_mgr_get_gcse(mtcsRtlPassMgr);
   return execute_rtl_hoist (mtcsGcse);
}


static void mtcsPassHoistInit(MtcsPassHoist *self)
{
   MtcsPass *mtcsPass= (MtcsPass *)self;
   mtcsPass->execute =hoist_execute_cb;
   mtcsPass->gate =hoist_pre_gate_cb;
   mtcs_pass_set_properties(mtcsPass,
      PROP_cfglayout,/* properties_required */
      0, /* properties_provided */
      0 /* properties_destroyed */);
   mtcs_pass_set_todo_flags(mtcsPass,
      0, /* todo_flags_start */
      TODO_df_finish /*todo_flags_finish */);
}

MtcsPassHoist *mtcs_pass_hoist_new(MtcsMode *mtcsMode)
{
   MtcsPassHoist *self = n_slice_alloc0 (sizeof(MtcsPassHoist));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"hoist");
   mtcsPassHoistInit(self);
   return self;
}


