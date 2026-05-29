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
#include "predict.h"
#include "memmodel.h"
#include "tm_p.h"
#include "ssa.h"
#include "optabs.h"
#include "expmed.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "alias.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "attribs.h"
#include "varasm.h"
#include "except.h"
#include "insn-attr.h"
#include "dojump.h"
#include "explow.h"
#include "calls.h"
#include "stmt.h"
/* Include expr.h after insn-config.h so we get HAVE_conditional_move.  */
#include "expr.h"
#include "optabs-tree.h"
#include "libfuncs.h"
#include "reload.h"
#include "langhooks.h"
#include "common/common-target.h"
#include "tree-dfa.h"
#include "tree-ssa-live.h"
#include "tree-outof-ssa.h"
#include "tree-ssa-address.h"
#include "builtins.h"
#include "ccmp.h"
#include "gimple-iterator.h"
#include "gimple-fold.h"
#include "rtx-vector-builder.h"
#include "tree-pretty-print.h"
#include "flags.h"
#include "tree-cfg.h"
#include "cfganal.h"

#include "mtcsstmt.h"
#include "mtcstarget.h"
#include "aet/aetprinttree.h"
#include "aet/aetprintgimple.h"

/* Functions and data structures for expanding case statements.  */

/* Case label structure, used to hold info on labels within case
   statements.  We handle "range" labels; for a single-value label
   as in C, the high and low limits are the same.

   We start with a vector of case nodes sorted in ascending order, and
   the default label as the last element in the vector.

   Switch statements are expanded in jump table form.
*/
//原型 simple_case_node stmt.cc
class simple_case_node
{
public:
  simple_case_node (tree low, tree high, tree code_label):
    m_low (low), m_high (high), m_code_label (code_label)
  {}

  /* Lowest index value for this label.  */
  tree m_low;
  /* Highest index value for this label.  */
  tree m_high;
  /* Label to jump to when node matches.  */
  tree m_code_label;
};

static void mtcsStmtInit(MtcsStmt *self)
{

}

/* Generate code to jump to LABEL if OP0 and OP1 are equal in mode MODE. PROB
   is the probability of jumping to LABEL.  */
//原型 do_jump_if_equal stmt.cc
static void do_jump_if_equal (MtcsStmt *self,machine_mode mode, rtx op0, rtx op1, rtx_code_label *label,
          int unsignedp, profile_probability prob)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);

   mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,op0, op1, EQ, unsignedp, mode,
      NULL_RTX, NULL, label, prob);
}

/* Return the sum of probabilities of outgoing edges of basic block BB.  */
//原型 get_outgoing_edge_probs stmt.cc
static profile_probability get_outgoing_edge_probs (basic_block bb)
{
  edge e;
  edge_iterator ei;
  profile_probability prob_sum = profile_probability::never ();
  if (!bb)
    return profile_probability::never ();
  FOR_EACH_EDGE (e, ei, bb->succs)
    prob_sum += e->probability;
  return prob_sum;
}

/* Computes the conditional probability of jumping to a target if the branch
   instruction is executed.
   TARGET_PROB is the estimated probability of jumping to a target relative
   to some basic block BB.
   BASE_PROB is the probability of reaching the branch instruction relative
   to the same basic block BB.  */
//原型 conditional_probability stmt.cc
static inline profile_probability conditional_probability (profile_probability target_prob,
             profile_probability base_prob)
{
  return target_prob / base_prob;
}


/* Generate a dispatch tabler, switching on INDEX_EXPR and jumping to
   one of the labels in CASE_LIST or to the DEFAULT_LABEL.
   MINVAL, MAXVAL, and RANGE are the extrema and range of the case
   labels in CASE_LIST. STMT_BB is the basic block containing the statement.

   First, a jump insn is emitted.  First we try "casesi".  If that
   fails, try "tablejump".   A target *must* have one of them (or both).

   Then, a table with the target labels is emitted.

   The process is unaware of the CFG.  The caller has to fix up
   the CFG itself.  This is done in cfgexpand.cc.  */
//原型 emit_case_dispatch_table stmt.cc
static void emit_case_dispatch_table (MtcsStmt *self,tree index_expr, tree index_type,
              auto_vec<simple_case_node> &case_list,
              rtx default_label,
              edge default_edge,  tree minval, tree maxval,
              tree range, basic_block stmt_bb)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
  MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *opts=mtcsOptions->global_options;

  int i, ncases;
  auto_vec<rtx> labelvec;
  rtx_insn *fallback_label = mtcs_stmt_label_rtx/*!label_rtx*/(self,case_list[0].m_code_label);
  rtx_code_label *table_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
  bool has_gaps = false;
  profile_probability default_prob = default_edge ? default_edge->probability
                          : profile_probability::never ();
  profile_probability base = get_outgoing_edge_probs (stmt_bb);
  bool try_with_tablejump = false;

  profile_probability new_default_prob = conditional_probability (default_prob,base);
  machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
  if (! mtcs_expr_try_casesi/*!try_casesi*/(mtcsExpr,index_type, index_expr, minval, range,
            table_label, default_label, fallback_label,new_default_prob)){
      /* Index jumptables from zero for suitable values of minval to avoid
     a subtraction.  For the rationale see:
     "http://gcc.gnu.org/ml/gcc-patches/2001-10/msg01234.html".  */
      if (optimize_insn_for_speed_p ()  && compare_tree_int (minval, 0) > 0  && compare_tree_int (minval, 3) < 0){
          minval = mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,index_type, 0);
          range = maxval;
          has_gaps = true;
      }
      try_with_tablejump = true;
  }

  /* Get table of labels to jump to, in order of case index.  */

  ncases = tree_to_shwi (range) + 1;
  labelvec.safe_grow_cleared (ncases);

  for (unsigned j = 0; j < case_list.length (); j++){
      simple_case_node *n = &case_list[j];
      /* Compute the low and high bounds relative to the minimum
     value since that should fit in a HOST_WIDE_INT while the
     actual values may not.  */
      HOST_WIDE_INT i_low = tree_to_uhwi (mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,MINUS_EXPR, index_type,
                     n->m_low, minval));
      HOST_WIDE_INT i_high = tree_to_uhwi (mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,MINUS_EXPR, index_type,
                     n->m_high, minval));
      HOST_WIDE_INT i;

      for (i = i_low; i <= i_high; i ++)
          labelvec[i]  = gen_rtx_LABEL_REF (pMode, mtcs_stmt_label_rtx/*!label_rtx*/(self,n->m_code_label));
  }

  /* The dispatch table may contain gaps, including at the beginning of
     the table if we tried to avoid the minval subtraction.  We fill the
     dispatch table slots associated with the gaps with the default case label.
     However, in the event the default case is unreachable, we then use
     any label from one of the case statements.  */
  rtx gap_label = (default_label) ? default_label : fallback_label;

  for (i = 0; i < ncases; i++)
     if (labelvec[i] == 0){
        has_gaps = true;
        labelvec[i] = gen_rtx_LABEL_REF (pMode, gap_label);
     }

  if (has_gaps && default_label){
      /* There is at least one entry in the jump table that jumps
         to default label. The default label can either be reached
         through the indirect jump or the direct conditional jump
         before that. Split the probability of reaching the
         default label among these two jumps.  */
      new_default_prob = conditional_probability (default_prob / 2, base);
      default_prob /= 2;
      base -= default_prob;
  }else{
      base -= default_prob;
      default_prob = profile_probability::never ();
  }

  if (default_edge)
    default_edge->probability = default_prob;

  /* We have altered the probability of the default edge. So the probabilities
     of all other edges need to be adjusted so that it sums up to
     REG_BR_PROB_BASE.  */
  if (base > profile_probability::never ()){
      edge e;
      edge_iterator ei;
      FOR_EACH_EDGE (e, ei, stmt_bb->succs)
         e->probability /= base;
  }

  if (try_with_tablejump){
      bool ok = mtcs_expr_try_tablejump/*!try_tablejump*/(mtcsExpr,index_type, index_expr, minval, range,
                               table_label, default_label, new_default_prob);
      gcc_assert (ok);
  }
  /* Output the table.  */
  mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,table_label);

  if (CASE_VECTOR_PC_RELATIVE || (opts->x_flag_pic
              && target_asm_out_generate_pic_addr_diff_vec/*!targetm.asm_out.generate_pic_addr_diff_vec*/(mtcsMachine->asmOut)))
      mtcs_emit_emit_jump_table_data/*!emit_jump_table_data*/(mtcsEmit,gen_rtx_ADDR_DIFF_VEC (CASE_VECTOR_MODE,
                         gen_rtx_LABEL_REF (pMode,table_label),
                         gen_rtvec_v (ncases, labelvec.address ()),
                         const0_rtx, const0_rtx));
  else
      mtcs_emit_emit_jump_table_data/*!emit_jump_table_data*/(mtcsEmit,gen_rtx_ADDR_VEC (CASE_VECTOR_MODE,
                        gen_rtvec_v (ncases, labelvec.address ())));
  /* Record no drop-through after the table.  */
  mtcs_emit_emit_barrier/*!emit_barrier*/(mtcsEmit);
}

/* Like expand_case but special-case for SJLJ exception dispatching.  */
/* Expand the dispatch to a short decrement chain if there are few cases
   to dispatch to.  Likewise if neither casesi nor tablejump is available,
   or if flag_jump_tables is set.  Otherwise, expand as a casesi or a
   tablejump.  The index mode is always the mode of integer_type_node.
   Trap if no case matches the index.

   DISPATCH_INDEX is the index expression to switch on.  It should be a
   memory or register operand.

   DISPATCH_TABLE is a set of case labels.  The set should be sorted in
   ascending order, be contiguous, starting with value 0, and contain only
   single-valued case labels.  */
//原型 expand_sjlj_dispatch_table stmt.h stmt.cc
void mtcs_stmt_expand_sjlj_dispatch_table (MtcsStmt *self,rtx dispatch_index,
                vec<tree> dispatch_table)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsBuiltins *mtcsBuiltins=mtcs_target_get_builtins(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *opts=mtcsOptions->global_options;

  tree index_type = integer_type_node;
  machine_mode index_mode = TYPE_MODE (index_type);

  int ncases = dispatch_table.length ();

  mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
  rtx_insn *before_case = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);

  /* Expand as a decrement-chain if there are 5 or fewer dispatch
     labels.  This covers more than 98% of the cases in libjava,
     and seems to be a reasonable compromise between the "old way"
     of expanding as a decision tree or dispatch table vs. the "new
     way" with decrement chain or dispatch table.  */
  if (dispatch_table.length () <= 5
      || (!target_rtx_have_casesi/*!targetm.have_casesi*/(mtcsMachine->tmrtx)
              && !target_rtx_have_tablejump/*!targetm.have_tablejump*/(mtcsMachine->tmrtx))
      || !opts->x_flag_jump_tables){
      /* Expand the dispatch as a decrement chain:

     "switch(index) {case 0: do_0; case 1: do_1; ...; case N: do_N;}"

     ==>

     if (index == 0) do_0; else index--;
     if (index == 0) do_1; else index--;
     ...
     if (index == 0) do_N; else index--;

     This is more efficient than a dispatch table on most machines.
     The last "index--" is redundant but the code is trivially dead
     and will be cleaned up by later passes.  */
      rtx index =mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,index_mode, dispatch_index);
      rtx zero = CONST0_RTX (index_mode);
      for (int i = 0; i < ncases; i++){
          tree elt = dispatch_table[i];
          rtx_code_label *lab = mtcs_stmt_jump_target_rtx/*!jump_target_rtx*/(self,CASE_LABEL (elt));
          do_jump_if_equal(self,index_mode, index, zero, lab, 0,
                    profile_probability::uninitialized ());
          mtcs_optabs_force_expand_binop/*!force_expand_binop*/(mtcsOptabs,index_mode, sub_optab,
                      index, CONST1_RTX (index_mode),
                      index, 0, OPTAB_DIRECT);
      }
  }else{
      /* Similar to expand_case, but much simpler.  */
      auto_vec<simple_case_node> case_list;
      tree index_expr = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,index_type, dispatch_index);
      tree minval = mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,index_type, 0);
      tree maxval = CASE_LOW (dispatch_table.last ());
      tree range = maxval;
      rtx_code_label *default_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);

      for (int i = ncases - 1; i >= 0; --i){
          tree elt = dispatch_table[i];
          tree high = CASE_HIGH (elt);
          if (high == NULL_TREE)
            high = CASE_LOW (elt);
          case_list.safe_push (simple_case_node (CASE_LOW (elt), high, CASE_LABEL (elt)));
      }

      emit_case_dispatch_table(self,index_expr, index_type,
                case_list, default_label, NULL,
                minval, maxval, range,
                BLOCK_FOR_INSN (before_case));
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,default_label);
  }

  /* Dispatching something not handled?  Trap!  */
  mtcs_builtins_expand_builtin_trap/*!expand_builtin_trap*/(mtcsBuiltins);

  mtcs_rtl_reorder_insns/*!reorder_insns*/(mtcsRTL,
        NEXT_INSN (before_case), mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData), before_case);

  mtcs_func_free_temp_slots/*!free_temp_slots*/(mtcsFunc);
}

/* Parse the output constraint pointed to by *CONSTRAINT_P.  It is the
   OPERAND_NUMth output operand, indexed from zero.  There are NINPUTS
   inputs and NOUTPUTS outputs to this extended-asm.  Upon return,
   *ALLOWS_MEM will be TRUE iff the constraint allows the use of a
   memory operand.  Similarly, *ALLOWS_REG will be TRUE iff the
   constraint allows the use of a register operand.  And, *IS_INOUT
   will be true if the operand is read-write, i.e., if it is used as
   an input as well as an output.  If *CONSTRAINT_P is not in
   canonical form, it will be made canonical.  (Note that `+' will be
   replaced with `=' as part of this process.)

   Returns TRUE if all went well; FALSE if an error occurred.  */
//原型 parse_output_constraint stmt.h stmt.cc
bool mtcs_stmt_parse_output_constraint (MtcsStmt *self,const char **constraint_p, int operand_num,
             int ninputs, int noutputs, bool *allows_mem, bool *allows_reg, bool *is_inout)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);

  const char *constraint = *constraint_p;
  const char *p;
  /* Assume the constraint doesn't allow the use of either a register
     or memory.  */
  *allows_mem = false;
  *allows_reg = false;
  /* Allow the `=' or `+' to not be at the beginning of the string,
     since it wasn't explicitly documented that way, and there is a
     large body of code that puts it last.  Swap the character to
     the front, so as not to uglify any place else.  */
  p = strchr (constraint, '=');
  if (!p)
     p = strchr (constraint, '+');
  /* If the string doesn't contain an `=', issue an error
     message.  */
  if (!p){
      error ("output operand constraint lacks %<=%>");
      return false;
  }
  /* If the constraint begins with `+', then the operand is both read
     from and written to.  */
  *is_inout = (*p == '+');
  /* Canonicalize the output constraint so that it begins with `='.  */
  if (p != constraint || *is_inout){
      char *buf;
      size_t c_len = strlen (constraint);
      if (p != constraint)
          warning (0, "output constraint %qc for operand %d is not at the beginning",*p, operand_num);

      /* Make a copy of the constraint.  */
      buf = XALLOCAVEC (char, c_len + 1);
      strcpy (buf, constraint);
      /* Swap the first character and the `=' or `+'.  */
      buf[p - constraint] = buf[0];
      /* Make sure the first character is an `='.  (Until we do this,
     it might be a `+'.)  */
      buf[0] = '=';
      /* Replace the constraint with the canonicalized string.  */
      *constraint_p = ggc_alloc_string (buf, c_len);
      constraint = *constraint_p;
  }

  /* Loop through the constraint string.  */
  for (p = constraint + 1; *p; ){
      switch (*p){
        case '+':
        case '=':
          error ("operand constraint contains incorrectly positioned %<+%> or %<=%>");
          return false;
        case '%':
          if (operand_num + 1 == ninputs + noutputs){
              error ("%<%%%> constraint used with last operand");
              return false;
          }
          break;

        case '?':  case '!':  case '*':  case '&':  case '#':
        case '$':  case '^':
        case 'E':  case 'F':  case 'G':  case 'H':
        case 's':  case 'i':  case 'n':
        case 'I':  case 'J':  case 'K':  case 'L':  case 'M':
        case 'N':  case 'O':  case 'P':  case ',':
          break;

        case '0':  case '1':  case '2':  case '3':  case '4':
        case '5':  case '6':  case '7':  case '8':  case '9':
        case '[':
          error ("matching constraint not valid in output operand");
          return false;

        case '<':  case '>':
          /* ??? Before flow, auto inc/dec insns are not supposed to exist,
             excepting those that expand_call created.  So match memory
             and hope.  */
          *allows_mem = true;
          break;
        case 'g':  case 'X':
          *allows_reg = true;
          *allows_mem = true;
          break;

        default:
          if (!ISALPHA (*p))
            break;
          enum constraint_num cn = mtcs_preds_lookup_constraint/*!lookup_constraint*/(mtcsPreds,p);
          if (mtcs_preds_reg_class_for_constraint/*!reg_class_for_constraint*/(mtcsPreds,cn) != NO_REGS
              || mtcs_preds_insn_extra_address_constraint/*!insn_extra_address_constraint*/(mtcsPreds,cn))
            *allows_reg = true;
          else if (mtcs_preds_insn_extra_memory_constraint/*!insn_extra_memory_constraint*/(mtcsPreds,cn))
            *allows_mem = true;
          else
              mtcs_preds_insn_extra_constraint_allows_reg_mem/*!insn_extra_constraint_allows_reg_mem*/(mtcsPreds,
                      cn, allows_reg, allows_mem);
          break;
      }

      for (size_t len = CONSTRAINT_LEN (*p, p); len; len--, p++)
        if (*p == '\0')
          break;
  }

  return true;
}


/* Similar, but for input constraints.  */
//原型 parse_input_constraint stmt.h stmt.cc
bool mtcs_stmt_parse_input_constraint (MtcsStmt *self,const char **constraint_p, int input_num,
            int ninputs, int noutputs, int ninout, const char * const * constraints,
            bool *allows_mem, bool *allows_reg)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);

  const char *constraint = *constraint_p;
  const char *orig_constraint = constraint;
  size_t c_len = strlen (constraint);
  size_t j;
  bool saw_match = false;

  /* Assume the constraint doesn't allow the use of either
     a register or memory.  */
  *allows_mem = false;
  *allows_reg = false;

  /* Make sure constraint has neither `=', `+', nor '&'.  */

  for (j = 0; j < c_len; j += CONSTRAINT_LEN (constraint[j], constraint+j))
      switch (constraint[j]){
          case '+':  case '=':  case '&':
            if (constraint == orig_constraint){
                error ("input operand constraint contains %qc", constraint[j]);
                return false;
            }
            break;

          case '%':
            if (constraint == orig_constraint  && input_num + 1 == ninputs - ninout){
                error ("%<%%%> constraint used with last operand");
                return false;
            }
            break;

          case '<':  case '>':
          case '?':  case '!':  case '*':  case '#':
          case '$':  case '^':
          case 'E':  case 'F':  case 'G':  case 'H':
          case 's':  case 'i':  case 'n':
          case 'I':  case 'J':  case 'K':  case 'L':  case 'M':
          case 'N':  case 'O':  case 'P':  case ',':
              break;

        /* Whether or not a numeric constraint allows a register is
           decided by the matching constraint, and so there is no need
           to do anything special with them.  We must handle them in
           the default case, so that we don't unnecessarily force
           operands to memory.  */
          case '0':  case '1':  case '2':  case '3':  case '4':
          case '5':  case '6':  case '7':  case '8':  case '9':
            {
              char *end;
              unsigned long match;

              saw_match = true;

              match = strtoul (constraint + j, &end, 10);
              if (match >= (unsigned long) noutputs){
                  error ("matching constraint references invalid operand number");
                  return false;
              }

              /* Try and find the real constraint for this dup.  Only do this
                 if the matching constraint is the only alternative.  */
              if (*end == '\0' && (j == 0 || (j == 1 && constraint[0] == '%'))){
                  constraint = constraints[match];
                  *constraint_p = constraint;
                  c_len = strlen (constraint);
                  j = 0;
                  /* ??? At the end of the loop, we will skip the first part of
                 the matched constraint.  This assumes not only that the
                 other constraint is an output constraint, but also that
                 the '=' or '+' come first.  */
                  break;
              }else
                j = end - constraint;
              /* Anticipate increment at end of loop.  */
              j--;
            }
            /* Fall through.  */

          case 'g':  case 'X':
            *allows_reg = true;
            *allows_mem = true;
            break;

          default:
            if (! ISALPHA (constraint[j])){
                error ("invalid punctuation %qc in constraint", constraint[j]);
                return false;
            }
            enum constraint_num cn = mtcs_preds_lookup_constraint/*!lookup_constraint*/(mtcsPreds,constraint + j);
            if (mtcs_preds_reg_class_for_constraint/*!reg_class_for_constraint*/(mtcsPreds,cn) != NO_REGS
                || mtcs_preds_insn_extra_address_constraint/*!insn_extra_address_constraint*/(mtcsPreds,cn))
              *allows_reg = true;
            else if (mtcs_preds_insn_extra_memory_constraint/*!insn_extra_memory_constraint*/(mtcsPreds,cn)
                 || mtcs_preds_insn_extra_special_memory_constraint/*!insn_extra_special_memory_constraint*/(mtcsPreds,cn)
                 || mtcs_preds_insn_extra_relaxed_memory_constraint/*!insn_extra_relaxed_memory_constraint*/(mtcsPreds,cn))
              *allows_mem = true;
            else
                mtcs_preds_insn_extra_constraint_allows_reg_mem/*!insn_extra_constraint_allows_reg_mem*/(mtcsPreds,cn, allows_reg, allows_mem);
            break;
      }

  if (saw_match && !*allows_reg)
     warning (0, "matching constraint does not allow a register");

  return true;
}

typedef struct _OverlapsHardRegSetPData
{
    MtcsStmt *mtcsStmt;
    HardRegSet *data;
}OverlapsHardRegSetPData;

/* Return DECL iff there's an overlap between *REGS and DECL, where DECL
   can be an asm-declared register.  Called via walk_tree.  */
//原型 decl_overlaps_hard_reg_set_p stmt.cc
static tree decl_overlaps_hard_reg_set_p_cb (tree *declp, int *walk_subtrees ATTRIBUTE_UNUSED,void *data)
{
  OverlapsHardRegSetPData *cb=(OverlapsHardRegSetPData *)data;
  MtcsStmt *self=(MtcsStmt *)cb->mtcsStmt;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

  tree decl = *declp;
  const HardRegSet *const regs = (const HardRegSet *) cb->data;
  int firstPseudoRegister= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);
  if (VAR_P (decl)){
      if (DECL_HARD_REGISTER (decl)  && REG_P (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl))
              && REGNO (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl)) <firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/){
          rtx reg = mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl);
          if (mtcs_reg_overlaps_hard_reg_set_p/*!overlaps_hard_reg_set_p*/(mtcsReg,regs, GET_MODE (reg), REGNO (reg)))
            return decl;
      }
      walk_subtrees = 0;
  }else if (TYPE_P (decl) || TREE_CODE (decl) == PARM_DECL)
    walk_subtrees = 0;
  return NULL_TREE;
}

/* If there is an overlap between *REGS and DECL, return the first overlap
   found.  */
//原型 tree_overlaps_hard_reg_set stmt.h stmt.cc
tree mtcs_stmt_tree_overlaps_hard_reg_set (MtcsStmt *self,tree decl, HardRegSet *regs)
{
  OverlapsHardRegSetPData data={self,regs};
  return walk_tree (&decl, decl_overlaps_hard_reg_set_p_cb, &data, NULL);
}

//原型 expand_naked_return rtl.h stmt.cc
void mtcs_stmt_expand_naked_return (MtcsStmt *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);

   rtx_code_label *end_label;
   mtcs_dojump_clear_pending_stack_adjust/*!clear_pending_stack_adjust*/(mtcsDojump);
   mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
   //原型 naked_return_label #define naked_return_label (crtl->x_naked_return_label)
   end_label = mtcsRtlData/*!crtl*/->x_naked_return_label/*!naked_return_label*/;
   if (end_label == 0)
      end_label = mtcsRtlData/*!crtl*/->x_naked_return_label/*!naked_return_label*/ =
                 mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);

   mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,end_label);
}

/* Return the rtx-label that corresponds to a LABEL_DECL,
   creating it if necessary.  */
//原型 label_rtx stmt.h stmt.cc
rtx_insn * mtcs_stmt_label_rtx (MtcsStmt *self,tree label)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm   *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   gcc_assert (TREE_CODE (label) == LABEL_DECL);

   if (!DECL_RTL_SET_P (label)){
      rtx_code_label *r = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
      mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,label, r);
      if (FORCED_LABEL (label) || DECL_NONLOCAL (label))
         LABEL_PRESERVE_P (r) = 1;
   }
   return as_a <rtx_insn *> (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,label));
}

/* As above, but also put it on the forced-reference list of the
   function that contains it.  */
//原型 force_label_rtx stmt.h stmt.cc
rtx_insn * mtcs_stmt_force_label_rtx (MtcsStmt *self,tree label)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsCfg  *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   rtx_insn *ref = mtcs_stmt_label_rtx/*!label_rtx*/(self,label);
   tree function = decl_function_context (label);

   gcc_assert (function);

   vec_safe_push (mtcsRtlData/*!crtl*/->expr.x_forced_labels/*!forced_labels*/, ref);
   return ref;
}

/* As label_rtx, but ensures (in check build), that returned value is
   an existing label (i.e. rtx with code CODE_LABEL).  */
//原型 jump_target_rtx stmt.h stmt.cc
rtx_code_label * mtcs_stmt_jump_target_rtx (MtcsStmt *self,tree label)
{
  return as_a <rtx_code_label *> (mtcs_stmt_label_rtx/*!label_rtx*/(self,label));
}

/* Handle goto statements and the labels that they can go to.  */

/* Specify the location in the RTL code of a label LABEL,
   which is a LABEL_DECL tree node.

   This is used for the kind of label that the user can jump to with a
   goto statement, and for alternatives of a switch or case statement.
   RTL labels generated for loops and conditionals don't go through here;
   they are generated directly at the RTL level, by other functions below.

   Note that this has nothing to do with defining label *names*.
   Languages vary in how they do that and what that even means.  */
//原型 expand_label stmt.h stmt.cc
void mtcs_stmt_expand_label (MtcsStmt *self,tree label)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsBuiltins *mtcsBuiltins=mtcs_target_get_builtins(mtcsTarget);

   rtx_code_label *label_r = mtcs_stmt_jump_target_rtx/*!jump_target_rtx*/(self,label);

   mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
   mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label_r);
   if (DECL_NAME (label))
      LABEL_NAME (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,label)) = IDENTIFIER_POINTER (DECL_NAME (label));

   if (DECL_NONLOCAL (label)){
      mtcs_builtins_expand_builtin_setjmp_receiver/*!expand_builtin_setjmp_receiver*/(mtcsBuiltins,NULL);
      mtcsRtlData/*!nonlocal_goto_handler_labels*/->x_nonlocal_goto_handler_labels
          = gen_rtx_INSN_LIST (VOIDmode, label_r, mtcsRtlData/*!nonlocal_goto_handler_labels*/->x_nonlocal_goto_handler_labels);
   }

   if (FORCED_LABEL (label))
      vec_safe_push<rtx_insn *> (mtcsRtlData/*!crtl*/->expr.x_forced_labels/*!forced_labels*/, label_r);

   if (DECL_NONLOCAL (label) || FORCED_LABEL (label))
      mtcs_rtl_data_maybe_set_first_label_num/*!maybe_set_first_label_num*/(mtcsRtlData,label_r);
}

static void printbb(basic_block bb)
{
   if(!bb)
      return;
   gimple_stmt_iterator gsi;
   for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
      gimple *stmt = gsi_stmt (gsi);

     //location_t loc = gimple_location (stmt);
    //tree block = LOCATION_BLOCK (loc);
     n_debug("mtcsstmt.c priintbb 打印bb中的gimple语句 stmt:%p bb:%p\n",stmt,bb);
     aet_print_gimple(stmt);
   }
}

/* Terminate a case Ada or switch (C) statement
   in which ORIG_INDEX is the expression to be tested.
   If ORIG_TYPE is not NULL, it is the original ORIG_INDEX
   type as given in the source before any compiler conversions.
   Generate the code to test it and jump to the right place.  */
//原型 expand_case stmt.h stmt.cc
void mtcs_stmt_expand_case (MtcsStmt *self,gswitch *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsConst  *mtcsConst =mtcs_target_get_const(mtcsTarget);
   MtcsTree  *mtcsTree =mtcs_target_get_tree(mtcsTarget);
   MtcsCfgContext  *mtcsCfgContext =mtcs_target_get_cfg_context(mtcsTarget);
   MtcsRTL  *mtcsRTL =mtcs_target_get_rtl(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   tree minval = NULL_TREE, maxval = NULL_TREE, range = NULL_TREE;
   rtx_code_label *default_label;
   unsigned int count;
   int i;
   int ncases = gimple_switch_num_labels (stmt);
   tree index_expr = gimple_switch_index (stmt);
   tree index_type = TREE_TYPE (index_expr);
   tree elt;
   basic_block bb = gimple_bb (stmt);
   gimple *def_stmt;

   auto_vec<simple_case_node> case_list;

   /* An ERROR_MARK occurs for various reasons including invalid data type.
   ??? Can this still happen, with GIMPLE and all?  */
   if (index_type == error_mark_node)
      return;

   /* cleanup_tree_cfg removes all SWITCH_EXPR with their index
   expressions being INTEGER_CST.  */
   gcc_assert (TREE_CODE (index_expr) != INTEGER_CST);

   /* Optimization of switch statements with only one label has already
   occurred, so we should never see them at this point.  */
   gcc_assert (ncases > 1);

   mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);

   /* Find the default case target label.  */
   tree default_lab = CASE_LABEL (gimple_switch_default_label (stmt));
   n_debug("mtcsstmt.c expand_case 00 default_lab:%p\n",default_lab);
   aet_print_gimple(stmt);
   aet_print_tree(default_lab);
   default_label = mtcs_stmt_jump_target_rtx/*!jump_target_rtx*/(self,default_lab);
   basic_block default_bb = label_to_block (cfun, default_lab);
   n_debug("mtcsstmt.c expand_case 11 打印 default_bb:%p default_label:%p\n",default_bb,default_label);
   printbb(default_bb);
   n_debug("mtcsstmt.c expand_case 22 打印 bb:%p\n",bb);
   edge default_edge = find_edge (bb, default_bb);

   /* Get upper and lower bounds of case values.  */
   elt = gimple_switch_label (stmt, 1);
   minval = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,index_type, CASE_LOW (elt));
   elt = gimple_switch_label (stmt, ncases - 1);
   if (CASE_HIGH (elt))
      maxval = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,index_type, CASE_HIGH (elt));
   else
      maxval = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,index_type, CASE_LOW (elt));

   /* Try to narrow the index type if it's larger than a word.
   That is mainly for -O0 where an equivalent optimization
   done by forward propagation is not run and is aimed at
   avoiding a call to a comparison routine of libgcc.  */
   if (TYPE_PRECISION (index_type) > BITS_PER_WORD
   && TREE_CODE (index_expr) == SSA_NAME
   && (def_stmt = SSA_NAME_DEF_STMT (index_expr))
   && is_gimple_assign (def_stmt)
   && gimple_assign_rhs_code (def_stmt) == NOP_EXPR){
      tree inner_index_expr = gimple_assign_rhs1 (def_stmt);
      tree inner_index_type = TREE_TYPE (inner_index_expr);

      if (INTEGRAL_TYPE_P (inner_index_type)
      && TYPE_PRECISION (inner_index_type) <= BITS_PER_WORD
      && int_fits_type_p (minval, inner_index_type)
      && int_fits_type_p (maxval, inner_index_type)){
         index_expr = inner_index_expr;
         index_type = inner_index_type;
         minval = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,index_type, minval);
         maxval = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,index_type, maxval);
      }
   }

   /* Compute span of values.  */
   range = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,MINUS_EXPR, index_type, maxval, minval);

   /* Listify the labels queue and gather some numbers to decide
   how to expand this switch().  */
   count = 0;

   for (i = ncases - 1; i >= 1; --i){
      elt = gimple_switch_label (stmt, i);
      tree low = CASE_LOW (elt);
      gcc_assert (low);
      tree high = CASE_HIGH (elt);
      gcc_assert (! high || tree_int_cst_lt (low, high));
      tree lab = CASE_LABEL (elt);

      /* Count the elements.
      A range counts double, since it requires two compares.  */
      count++;
      if (high)
         count++;

      /* The bounds on the case range, LOW and HIGH, have to be converted
      to case's index type TYPE.  Note that the original type of the
      case index in the source code is usually "lost" during
      gimplification due to type promotion, but the case labels retain the
      original type.  Make sure to drop overflow flags.  */
      low = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,index_type, low);
      if (TREE_OVERFLOW (low))
         low = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,index_type, wi::to_wide (low));

      /* The canonical from of a case label in GIMPLE is that a simple case
      has an empty CASE_HIGH.  For the casesi and tablejump expanders,
      the back ends want simple cases to have high == low.  */
      if (! high)
         high = low;
      high = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,index_type, high);
      if (TREE_OVERFLOW (high))
         high = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,index_type, wi::to_wide (high));

      case_list.safe_push (simple_case_node (low, high, lab));
   }

   /* cleanup_tree_cfg removes all SWITCH_EXPR with a single
   destination, such as one with a default case only.
   It also removes cases that are out of range for the switch
   type, so we should never get a zero here.  */
   gcc_assert (count > 0);

   rtx_insn *before_case = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);

   /* If the default case is unreachable, then set default_label to NULL
   so that we omit the range check when generating the dispatch table.
   We also remove the edge to the unreachable default case.  The block
   itself will be automatically removed later.  */
   if (EDGE_COUNT (default_edge->dest->succs) == 0
   && gimple_seq_unreachable_p (bb_seq (default_edge->dest))){
      default_label = NULL;
      mtcs_cfg_context_remove_edge/*!remove_edge*/(mtcsCfgContext,default_edge);
      default_edge = NULL;
   }

   emit_case_dispatch_table(self,index_expr, index_type,
         case_list, default_label, default_edge,minval, maxval, range, bb);

   mtcs_rtl_reorder_insns/*!reorder_insns*/(mtcsRTL,
         NEXT_INSN (before_case), mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData), before_case);

   mtcs_func_free_temp_slots/*!free_temp_slots*/(mtcsFunc);
}


MtcsStmt *mtcs_stmt_new(MtcsMode *mtcsMode)
{
     MtcsStmt *self = n_slice_alloc0 (sizeof(MtcsStmt));
     mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
     mtcsStmtInit(self);
     return self;
}
