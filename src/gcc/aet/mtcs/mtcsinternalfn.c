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
 * base on internal-fn.cc
 */


#include "config.h"
#define INCLUDE_MEMORY
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "predict.h"
#include "stringpool.h"
#include "tree-vrp.h"
#include "tree-ssanames.h"
#include "expmed.h"
#include "memmodel.h"
#include "optabs.h"
#include "emit-rtl.h"
#include "diagnostic-core.h"
#include "fold-const.h"
#include "internal-fn.h"
#include "stor-layout.h"
#include "dojump.h"
#include "expr.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "ubsan.h"
#include "recog.h"
#include "builtins.h"
#include "optabs-tree.h"
#include "gimple-ssa.h"
#include "tree-phinodes.h"
#include "ssa-iterators.h"
#include "explow.h"
#include "rtl-iter.h"
#include "gimple-range.h"
#include "fold-const-call.h"
#include "tree-ssa-live.h"
#include "tree-outof-ssa.h"
#include "gcc-urlifier.h"

/* For lang_hooks.types.type_for_mode.  */
#include "langhooks.h"

#include "mtcsinternalfn.h"
#include "mtcstarget.h"
#include "mtcsprintrtl.h"
#include "../aetprinttree.h"

/*
//原型 internal_fn_fnspec_array internal-fn.h internal-fn.cc
extern GTY(()) const_tree internal_fn_fnspec_array[IFN_LAST + 1];
//原型 lookup_internal_fn internal-fn.h internal-fn.cc 使用原来的
internal_fn lookup_internal_fn (const char *name)
//原型 lookup_hilo_internal_fn internal-fn.h internal-fn.cc 使用主机的
extern void lookup_hilo_internal_fn (internal_fn ifn, internal_fn *lo, internal_fn *hi)
//原型 lookup_evenodd_internal_fn internal-fn.h internal-fn.cc 使用主机的
extern void lookup_evenodd_internal_fn (internal_fn ifn, internal_fn *even, internal_fn *odd)
//原型 init_internal_fns internal-fn.h internal-fn.cc 使用主机的 internal_fn_fnspec_array
void init_internal_fns ()
//原型 direct_internal_fn_array internal-fn.h internal-fn.cc 使用主机的 direct_internal_fn_array
const direct_internal_fn_info direct_internal_fn_array[IFN_LAST + 1]
//原型 direct_internal_fn_types internal-fn.h internal-fn.cc 使用主机的 direct_internal_fn_array
tree_pair direct_internal_fn_types (internal_fn fn, tree return_type, tree *args)
tree_pair direct_internal_fn_types (internal_fn fn, gcall *call)
optab direct_internal_fn_optab (internal_fn fn, tree_pair types)
static optab direct_internal_fn_optab (internal_fn fn) 是static 保留

bool internal_load_fn_p (internal_fn fn)
bool internal_store_fn_p (internal_fn fn)
bool internal_gather_scatter_fn_p (internal_fn fn)
int internal_fn_len_index (internal_fn fn)
int internal_fn_mask_index (internal_fn fn)
int internal_fn_stored_value_index (internal_fn fn)
bool commutative_binary_fn_p (internal_fn fn)
bool commutative_ternary_fn_p (internal_fn fn)
bool associative_binary_fn_p (internal_fn fn)
int first_commutative_argument (internal_fn fn)
internal_fn get_conditional_internal_fn (tree_code code)
tree_code conditional_internal_fn_code (internal_fn ifn)
internal_fn get_conditional_len_internal_fn (tree_code code)
internal_fn get_conditional_internal_fn (internal_fn fn)
internal_fn get_len_internal_fn (internal_fn fn)
internal_fn get_unconditional_internal_fn (internal_fn ifn)
bool can_interpret_as_conditional_op_p (gimple *stmt, tree *cond_out,
               tree_code *code_out,
               tree (&ops)[3], tree *else_out,
               tree *len, tree *bias)
bool widening_fn_p (code_helper code)

*/


/* Like create_output_operand, but for callers that will use
   assign_call_lhs afterwards.  */

static void create_call_lhs_operand (expand_operand *op, rtx lhs_rtx, machine_mode mode)
{
   /* Do not assign directly to a promoted subreg, since there is no
   guarantee that the instruction will leave the upper bits of the
   register in the state required by SUBREG_PROMOTED_SIGN.  */
   rtx dest = lhs_rtx;
   if (dest && GET_CODE (dest) == SUBREG && SUBREG_PROMOTED_VAR_P (dest))
      dest = NULL_RTX;
   create_output_operand (op, dest, mode);
}

/* Move the result of an expanded instruction into the lhs of a gimple call.
   LHS is the lhs of the call, LHS_RTX is its expanded form, and OP is the
   result of the expanded instruction.  OP should have been set up by
   create_call_lhs_operand.  */

static void assign_call_lhs (MtcsInternalFn *self,tree lhs, rtx lhs_rtx, expand_operand *op)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   if (rtx_equal_p (lhs_rtx, op->value))
      return;

   /* If the return value has an integral type, convert the instruction
   result to that type.  This is useful for things that return an
   int regardless of the size of the input.  If the instruction result
   is smaller than required, assume that it is signed.

   If the return value has a nonintegral type, its mode must match
   the instruction result.  */
   if (GET_CODE (lhs_rtx) == SUBREG && SUBREG_PROMOTED_VAR_P (lhs_rtx)){
      /* If this is a scalar in a register that is stored in a wider
      mode than the declared mode, compute the result into its
      declared mode and then convert to the wider mode.  */
      gcc_checking_assert (INTEGRAL_TYPE_P (TREE_TYPE (lhs)));
      rtx tmp = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,GET_MODE (lhs_rtx), op->value, 0);
      mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,SUBREG_REG (lhs_rtx), tmp,SUBREG_PROMOTED_SIGN (lhs_rtx));
   }else if (GET_MODE (lhs_rtx) == GET_MODE (op->value))
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,lhs_rtx, op->value);
   else{
      gcc_checking_assert (INTEGRAL_TYPE_P (TREE_TYPE (lhs)));
      mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,lhs_rtx, op->value, 0);
   }
}

/* Expand STMT using instruction ICODE.  The instruction has NOUTPUTS
   output operands and NINPUTS input operands, where NOUTPUTS is either
   0 or 1.  The output operand (if any) comes first, followed by the
   NINPUTS input operands.  */

static void expand_fn_using_insn (MtcsInternalFn *self,gcall *stmt, insn_code icode, unsigned int noutputs,
            unsigned int ninputs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   gcc_assert (icode != CODE_FOR_nothing);

   expand_operand *ops = XALLOCAVEC (expand_operand, noutputs + ninputs);
   unsigned int opno = 0;
   rtx lhs_rtx = NULL_RTX;
   tree lhs = gimple_call_lhs (stmt);

   if (noutputs){
      gcc_assert (noutputs == 1);
      if (lhs)
         lhs_rtx = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
      create_call_lhs_operand (&ops[opno], lhs_rtx, mtcsOutput->insn_data[icode].operand[opno].mode);
      opno += 1;
   }else
      gcc_assert (!lhs);

   for (unsigned int i = 0; i < ninputs; ++i){
      tree rhs = gimple_call_arg (stmt, i);
      tree rhs_type = TREE_TYPE (rhs);
      rtx rhs_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,rhs);
      if (INTEGRAL_TYPE_P (rhs_type))
      create_convert_operand_from (&ops[opno], rhs_rtx,TYPE_MODE (rhs_type),TYPE_UNSIGNED (rhs_type));
      else if (TREE_CODE (rhs) == SSA_NAME && SSA_NAME_IS_DEFAULT_DEF (rhs) && VAR_P (SSA_NAME_VAR (rhs)))
         create_undefined_input_operand (&ops[opno], TYPE_MODE (rhs_type));
      else if (VECTOR_BOOLEAN_TYPE_P (rhs_type)
      && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,TYPE_MODE (rhs_type))
      && maybe_ne (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,TYPE_MODE (rhs_type)),
      TYPE_VECTOR_SUBPARTS (rhs_type).to_constant ())){
         /* Ensure that the vector bitmasks do not have excess bits.  */
         int nunits = TYPE_VECTOR_SUBPARTS (rhs_type).to_constant ();
         rtx tmp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
               TYPE_MODE (rhs_type), and_optab, rhs_rtx,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,
                     (HOST_WIDE_INT_1U << nunits) - 1), NULL_RTX, true, OPTAB_WIDEN);
         create_input_operand (&ops[opno], tmp, TYPE_MODE (rhs_type));
      }else
         create_input_operand (&ops[opno], rhs_rtx, TYPE_MODE (rhs_type));
      opno += 1;
   }

   gcc_assert (opno == noutputs + ninputs);
   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,icode, opno, ops);
   if (lhs_rtx)
      assign_call_lhs(self,lhs, lhs_rtx, &ops[0]);
}

/* ARRAY_TYPE is an array of vector modes.  Return the associated insn
   for load-lanes-style optab OPTAB, or CODE_FOR_nothing if none.  */

static enum insn_code get_multi_vector_move (MtcsInternalFn *self,tree array_type, convert_optab optab)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);

   machine_mode imode;
   machine_mode vmode;

   gcc_assert (TREE_CODE (array_type) == ARRAY_TYPE);
   imode = TYPE_MODE (array_type);
   vmode = TYPE_MODE (TREE_TYPE (array_type));

   return mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,optab, imode, vmode);
}

/* Add mask, else, and len arguments according to the STMT.  */

static unsigned int add_mask_else_and_len_args (MtcsInternalFn *self,expand_operand *ops, unsigned int opno, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   internal_fn ifn = gimple_call_internal_fn (stmt);
   int len_index = internal_fn_len_index (ifn);
   /* BIAS is always consecutive next of LEN.  */
   int bias_index = len_index + 1;
   int mask_index = internal_fn_mask_index (ifn);

   /* The order of arguments is always {mask, else, len, bias}.  */
   if (mask_index >= 0){
      tree mask = gimple_call_arg (stmt, mask_index);
      rtx mask_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,mask);

      tree mask_type = TREE_TYPE (mask);
      if (VECTOR_BOOLEAN_TYPE_P (mask_type)
      && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,TYPE_MODE (mask_type))
      && maybe_ne (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,TYPE_MODE (mask_type)),
      TYPE_VECTOR_SUBPARTS (mask_type).to_constant ())){
         /* Ensure that the vector bitmasks do not have excess bits.  */
         int nunits = TYPE_VECTOR_SUBPARTS (mask_type).to_constant ();
         mask_rtx = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
               TYPE_MODE (mask_type), and_optab, mask_rtx,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,
                     (HOST_WIDE_INT_1U << nunits) - 1),NULL_RTX, true, OPTAB_WIDEN);
         }

      create_input_operand (&ops[opno++], mask_rtx,TYPE_MODE (TREE_TYPE (mask)));
   }

   int els_index = internal_fn_else_index (ifn);
   if (els_index >= 0){
      tree els = gimple_call_arg (stmt, els_index);
      tree els_type = TREE_TYPE (els);
      if (TREE_CODE (els) == SSA_NAME && SSA_NAME_IS_DEFAULT_DEF (els) && VAR_P (SSA_NAME_VAR (els)))
         create_undefined_input_operand (&ops[opno++], TYPE_MODE (els_type));
      else{
         rtx els_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,els);
         create_input_operand (&ops[opno++], els_rtx, TYPE_MODE (els_type));
      }
   }
   if (len_index >= 0){
      tree len = gimple_call_arg (stmt, len_index);
      rtx len_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,len);
      create_convert_operand_from (&ops[opno++], len_rtx,TYPE_MODE (TREE_TYPE (len)),TYPE_UNSIGNED (TREE_TYPE (len)));
      tree biast = gimple_call_arg (stmt, bias_index);
      rtx bias = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,biast);
      create_input_operand (&ops[opno++], bias, mtcsMode->modes.M_QImode);
   }
   return opno;
}

/* Expand LOAD_LANES call STMT using optab OPTAB.  */
static void expand_load_lanes_optab_fn (MtcsInternalFn *self,internal_fn, gcall *stmt, convert_optab optab)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   class expand_operand ops[2];
   tree type, lhs, rhs;
   rtx target, mem;

   lhs = gimple_call_lhs (stmt);
   rhs = gimple_call_arg (stmt, 0);
   type = TREE_TYPE (lhs);

   target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   mem = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,rhs);

   gcc_assert (MEM_P (mem));
   mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,mem, TYPE_MODE (type));

   create_call_lhs_operand (&ops[0], target, TYPE_MODE (type));
   create_fixed_operand (&ops[1], mem);
   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,get_multi_vector_move(self,type, optab), 2, ops);
   assign_call_lhs(self,lhs, target, &ops[0]);
}

/* Expand STORE_LANES call STMT using optab OPTAB.  */
static void expand_store_lanes_optab_fn (MtcsInternalFn *self,internal_fn, gcall *stmt, convert_optab optab)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   class expand_operand ops[2];
   tree type, lhs, rhs;
   rtx target, reg;

   lhs = gimple_call_lhs (stmt);
   rhs = gimple_call_arg (stmt, 0);
   type = TREE_TYPE (rhs);

   target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   reg = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,rhs);

   gcc_assert (MEM_P (target));
   mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,target, TYPE_MODE (type));

   create_fixed_operand (&ops[0], target);
   create_input_operand (&ops[1], reg, TYPE_MODE (type));
   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,get_multi_vector_move(self,type, optab), 2, ops);
}

static void mtcs_expand_ANNOTATE (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This should get expanded in omp_device_lower pass.  */
static void mtcs_expand_GOMP_USE_SIMT (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This should get expanded in omp_device_lower pass.  */

static void mtcs_expand_GOMP_SIMT_ENTER (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* Allocate per-lane storage and begin non-uniform execution region.  */
static void mtcs_expand_GOMP_SIMT_ENTER_ALLOC (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   rtx target;
   tree lhs = gimple_call_lhs (stmt);
   if (lhs)
      target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   else
      target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mtcs_mode_get_Pmode(mtcsMode));
   rtx size = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,gimple_call_arg (stmt, 0));
   rtx align = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,gimple_call_arg (stmt, 1));
   class expand_operand ops[3];
   create_call_lhs_operand (&ops[0], target, mtcs_mode_get_Pmode(mtcsMode));
   create_input_operand (&ops[1], size, mtcs_mode_get_Pmode(mtcsMode));
   create_input_operand (&ops[2], align, mtcs_mode_get_Pmode(mtcsMode));
   gcc_assert (target_rtx_have_omp_simt_enter/*!targetm.have_omp_simt_enter*/(mtcsMachine->tmrtx));
   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,targetm.code_for_omp_simt_enter, 3, ops);
   assign_call_lhs(self,lhs, target, &ops[0]);
}

/* Deallocate per-lane storage and leave non-uniform execution region.  */
static void mtcs_expand_GOMP_SIMT_EXIT (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   gcc_checking_assert (!gimple_call_lhs (stmt));
   rtx arg = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,gimple_call_arg (stmt, 0));
   class expand_operand ops[1];
   create_input_operand (&ops[0], arg, mtcs_mode_get_Pmode(mtcsMode));
   gcc_assert (target_rtx_have_omp_simt_exit/*!targetm.have_omp_simt_exit*/(mtcsMachine->tmrtx));
   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,targetm.code_for_omp_simt_exit, 1, ops);
}

/* Lane index on SIMT targets: thread index in the warp on NVPTX.  On targets
   without SIMT execution this should be expanded in omp_device_lower pass.  */

static void mtcs_expand_GOMP_SIMT_LANE (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

   tree lhs = gimple_call_lhs (stmt);
   if (!lhs)
      return;

   rtx target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   gcc_assert (target_rtx_have_omp_simt_lane/*!targetm.have_omp_simt_lane*/(mtcsMachine->tmrtx));
   mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,
         target_rtx_gen_omp_simt_lane/*!targetm.gen_omp_simt_lane*/(mtcsMachine->tmrtx,target));
}

/* This should get expanded in omp_device_lower pass.  */

static void mtcs_expand_GOMP_SIMT_VF (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This should get expanded in omp_device_lower pass.  */

static void mtcs_expand_GOMP_MAX_VF (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This should get expanded in omp_device_lower pass.  */

static void mtcs_expand_GOMP_TARGET_REV (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* Lane index of the first SIMT lane that supplies a non-zero argument.
   This is a SIMT counterpart to GOMP_SIMD_LAST_LANE, used to represent the
   lane that executed the last iteration for handling OpenMP lastprivate.  */

static void mtcs_expand_GOMP_SIMT_LAST_LANE (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   tree lhs = gimple_call_lhs (stmt);
   if (!lhs)
      return;

   rtx target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   rtx cond = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,gimple_call_arg (stmt, 0));
   machine_mode mode = TYPE_MODE (TREE_TYPE (lhs));
   class expand_operand ops[2];
   create_call_lhs_operand (&ops[0], target, mode);
   create_input_operand (&ops[1], cond, mode);
   gcc_assert (target_rtx_have_omp_simt_last_lane/*!targetm.have_omp_simt_last_lane*/(mtcsMachine->tmrtx));
   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,targetm.code_for_omp_simt_last_lane, 2, ops);
   assign_call_lhs(self,lhs, target, &ops[0]);
}

/* Non-transparent predicate used in SIMT lowering of OpenMP "ordered".  */

static void mtcs_expand_GOMP_SIMT_ORDERED_PRED (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   tree lhs = gimple_call_lhs (stmt);
   if (!lhs)
      return;

   rtx target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   rtx ctr = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,gimple_call_arg (stmt, 0));
   machine_mode mode = TYPE_MODE (TREE_TYPE (lhs));
   class expand_operand ops[2];
   create_call_lhs_operand (&ops[0], target, mode);
   create_input_operand (&ops[1], ctr, mode);
   gcc_assert (target_rtx_have_omp_simt_ordered/*targetm.have_omp_simt_ordered*/(mtcsMachine->tmrtx));
   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,targetm.code_for_omp_simt_ordered, 2, ops);
   assign_call_lhs(self,lhs, target, &ops[0]);
}

/* "Or" boolean reduction across SIMT lanes: return non-zero in all lanes if
   any lane supplies a non-zero argument.  */

static void mtcs_expand_GOMP_SIMT_VOTE_ANY (MtcsInternalFn *self,internal_fn fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   tree lhs = gimple_call_lhs (stmt);
   if (!lhs)
      return;

   rtx target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   rtx cond = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,gimple_call_arg (stmt, 0));
   machine_mode mode = TYPE_MODE (TREE_TYPE (lhs));
   class expand_operand ops[2];
   create_call_lhs_operand (&ops[0], target, mode);
   create_input_operand (&ops[1], cond, mode);
   gcc_assert (target_rtx_have_omp_simt_vote_any/*!targetm.have_omp_simt_vote_any*/(mtcsMachine->tmrtx));
   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,targetm.code_for_omp_simt_vote_any, 2, ops);
   assign_call_lhs(self,lhs, target, &ops[0]);
}

/* Exchange between SIMT lanes with a "butterfly" pattern: source lane index
   is destination lane index XOR given offset.  */
static void mtcs_expand_GOMP_SIMT_XCHG_BFLY (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   tree lhs = gimple_call_lhs (stmt);
   if (!lhs)
      return;

   rtx target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   rtx src = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,gimple_call_arg (stmt, 0));
   rtx idx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,gimple_call_arg (stmt, 1));
   machine_mode mode = TYPE_MODE (TREE_TYPE (lhs));
   class expand_operand ops[3];
   create_call_lhs_operand (&ops[0], target, mode);
   create_input_operand (&ops[1], src, mode);
   create_input_operand (&ops[2], idx, SImode);
   gcc_assert (target_rtx_have_omp_simt_xchg_bfly/*!targetm.have_omp_simt_xchg_bfly*/(mtcsMachine->tmrtx));
   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,targetm.code_for_omp_simt_xchg_bfly, 3, ops);
   assign_call_lhs(self,lhs, target, &ops[0]);
}

/* Exchange between SIMT lanes according to given source lane index.  */
static void mtcs_expand_GOMP_SIMT_XCHG_IDX (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   tree lhs = gimple_call_lhs (stmt);
   if (!lhs)
   return;

   rtx target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   rtx src = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,gimple_call_arg (stmt, 0));
   rtx idx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,gimple_call_arg (stmt, 1));
   machine_mode mode = TYPE_MODE (TREE_TYPE (lhs));
   class expand_operand ops[3];
   create_call_lhs_operand (&ops[0], target, mode);
   create_input_operand (&ops[1], src, mode);
   create_input_operand (&ops[2], idx, SImode);
   gcc_assert (target_rtx_have_omp_simt_xchg_idx/*!targetm.have_omp_simt_xchg_idx*/(mtcsMachine->tmrtx));
   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,targetm.code_for_omp_simt_xchg_idx, 3, ops);
   assign_call_lhs(self,lhs, target, &ops[0]);
}

/* This should get expanded in adjust_simduid_builtins.  */
static void mtcs_expand_GOMP_SIMD_LANE (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This should get expanded in adjust_simduid_builtins.  */

static void mtcs_expand_GOMP_SIMD_VF (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This should get expanded in adjust_simduid_builtins.  */

static void mtcs_expand_GOMP_SIMD_LAST_LANE (internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This should get expanded in adjust_simduid_builtins.  */

static void mtcs_expand_GOMP_SIMD_ORDERED_START (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This should get expanded in adjust_simduid_builtins.  */

static void mtcs_expand_GOMP_SIMD_ORDERED_END (internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This should get expanded in gimplify_omp_dispatch.  */

static void mtcs_expand_GOMP_DISPATCH (MtcsInternalFn *self,internal_fn, gcall *)
{
  gcc_unreachable ();
}

/* This should get expanded in the sanopt pass.  */
static void mtcs_expand_UBSAN_NULL (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This should get expanded in the sanopt pass.  */

static void mtcs_expand_UBSAN_BOUNDS (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This should get expanded in the sanopt pass.  */

static void mtcs_expand_UBSAN_VPTR (MtcsInternalFn *self,internal_fn, gcall *)
{
  gcc_unreachable ();
}

/* This should get expanded in the sanopt pass.  */

static void mtcs_expand_UBSAN_PTR (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This should get expanded in the sanopt pass.  */

static void mtcs_expand_UBSAN_OBJECT_SIZE (internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This should get expanded in the sanopt pass.  */

static void mtcs_expand_HWASAN_CHECK (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* For hwasan stack tagging:
   Clear tags on the dynamically allocated space.
   For use after an object dynamically allocated on the stack goes out of
   scope.  */
static void mtcs_expand_HWASAN_ALLOCA_UNPOISON (MtcsInternalFn *self,internal_fn, gcall *gc)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   gcc_assert (mtcs_mode_get_Pmode(mtcsMode) == ptr_mode);
   tree restored_position = gimple_call_arg (gc, 0);
   rtx restored_rtx = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,restored_position, NULL_RTX, VOIDmode,EXPAND_NORMAL);
   rtx func = mtcs_libfuncs_init_one_libfunc/*!init_one_libfunc*/(mtcsLibfuncs,"__hwasan_tag_memory");
   rtx off = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
         mtcs_mode_get_Pmode(mtcsMode), MINUS, restored_rtx, mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL),
         NULL_RTX, 0,OPTAB_WIDEN);
   mtcs_calls_emit_library_call_value/*!emit_library_call_value*/(mtcsCalls,
         func, NULL_RTX, LCT_NORMAL, VOIDmode,virtual_stack_dynamic_rtx, mtcs_mode_get_Pmode(mtcsMode),
         HWASAN_STACK_BACKGROUND, QImode,off, mtcs_mode_get_Pmode(mtcsMode));
}

/* For hwasan stack tagging:
   Return a tag to be used for a dynamic allocation.  */
static void mtcs_expand_HWASAN_CHOOSE_TAG (MtcsInternalFn *self,internal_fn, gcall *gc)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   tree tag = gimple_call_lhs (gc);
   rtx target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,tag, NULL_RTX, VOIDmode, EXPAND_NORMAL);
   machine_mode mode = GET_MODE (target);
   gcc_assert (mode == mtcsMode->modes.M_QImode);

   rtx base_tag = targetm.memtag.extract_tag (hwasan_frame_base (), NULL_RTX);
   gcc_assert (base_tag);
   rtx tag_offset = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,hwasan_current_frame_tag (), mtcsMode->modes.M_QImode);
   rtx chosen_tag = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
         mtcsMode->modes.M_QImode, PLUS, base_tag, tag_offset,target, /* unsignedp = */1,OPTAB_WIDEN);
   chosen_tag = hwasan_truncate_to_tag_size (chosen_tag, target);

   /* Really need to put the tag into the `target` RTX.  */
   if (chosen_tag != target){
      rtx temp = chosen_tag;
      gcc_assert (GET_MODE (chosen_tag) == mode);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, temp);
   }

   hwasan_increment_frame_tag ();
}

/* For hwasan stack tagging:
   Tag a region of space in the shadow stack according to the base pointer of
   an object on the stack.  N.b. the length provided in the internal call is
   required to be aligned to HWASAN_TAG_GRANULE_SIZE.  */
static void mtcs_expand_HWASAN_MARK (MtcsInternalFn *self,internal_fn, gcall *gc)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);

   gcc_assert (ptr_mode == mtcs_mode_get_Pmode(mtcsMode));
   HOST_WIDE_INT flag = tree_to_shwi (gimple_call_arg (gc, 0));
   bool is_poison = ((asan_mark_flags)flag) == ASAN_MARK_POISON;

   tree base = gimple_call_arg (gc, 1);
   gcc_checking_assert (TREE_CODE (base) == ADDR_EXPR);
   rtx base_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,base);

   rtx tag = is_poison ? HWASAN_STACK_BACKGROUND : targetm.memtag.extract_tag (base_rtx, NULL_RTX);
   rtx address = targetm.memtag.untagged_pointer (base_rtx, NULL_RTX);

   tree len = gimple_call_arg (gc, 2);
   rtx r_len = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,len);

   rtx func = mtcs_libfuncs_init_one_libfunc/*!init_one_libfunc*/(mtcsLibfuncs,"__hwasan_tag_memory");
   mtcs_calls_emit_library_call/*!emit_library_call*/(mtcsCalls,func, LCT_NORMAL, VOIDmode, address, mtcs_mode_get_Pmode(mtcsMode),
   tag, mtcsMode->modes.M_QImode, r_len, mtcs_mode_get_Pmode(mtcsMode));
}

/* For hwasan stack tagging:
   Store a tag into a pointer.  */
static void mtcs_expand_HWASAN_SET_TAG (MtcsInternalFn *self,internal_fn, gcall *gc)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);

   gcc_assert (ptr_mode == mtcs_mode_get_Pmode(mtcsMode));
   tree g_target = gimple_call_lhs (gc);
   tree g_ptr = gimple_call_arg (gc, 0);
   tree g_tag = gimple_call_arg (gc, 1);

   rtx ptr = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,g_ptr);
   rtx tag = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,g_tag, NULL_RTX, mtcsMode->modes.M_QImode, EXPAND_NORMAL);
   rtx target = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,g_target);

   rtx untagged = targetm.memtag.untagged_pointer (ptr, target);
   rtx tagged_value = targetm.memtag.set_tag (untagged, tag, target);
   if (tagged_value != target)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, tagged_value);
}

/* This should get expanded in the sanopt pass.  */

static void mtcs_expand_ASAN_CHECK (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This should get expanded in the sanopt pass.  */
static void mtcs_expand_ASAN_MARK (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This should get expanded in the sanopt pass.  */
static void mtcs_expand_ASAN_POISON (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This should get expanded in the sanopt pass.  */
static void mtcs_expand_ASAN_POISON_USE (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This should get expanded in the tsan pass.  */
static void mtcs_expand_TSAN_FUNC_EXIT (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This should get expanded in the lower pass.  */
static void mtcs_expand_FALLTHROUGH (MtcsInternalFn *self,internal_fn, gcall *call)
{
  auto_urlify_attributes sentinel;
  error_at (gimple_location (call),"invalid use of attribute %<fallthrough%>");
}

/* Return minimum precision needed to represent all values
   of ARG in SIGNed integral type.  */
static int get_min_precision (tree arg, signop sign)
{
   int prec = TYPE_PRECISION (TREE_TYPE (arg));
   int cnt = 0;
   signop orig_sign = sign;
   if (TREE_CODE (arg) == INTEGER_CST){
      int p;
      if (TYPE_SIGN (TREE_TYPE (arg)) != sign){
         widest_int w = wi::to_widest (arg);
         w = wi::ext (w, prec, sign);
         p = wi::min_precision (w, sign);
      }else
         p = wi::min_precision (wi::to_wide (arg), sign);
      return MIN (p, prec);
   }
   while (CONVERT_EXPR_P (arg)
   && INTEGRAL_TYPE_P (TREE_TYPE (TREE_OPERAND (arg, 0)))
   && TYPE_PRECISION (TREE_TYPE (TREE_OPERAND (arg, 0))) <= prec){
      arg = TREE_OPERAND (arg, 0);
      if (TYPE_PRECISION (TREE_TYPE (arg)) < prec){
         if (TYPE_UNSIGNED (TREE_TYPE (arg)))
            sign = UNSIGNED;
         else if (sign == UNSIGNED && get_range_pos_neg (arg) != 1)
            return prec + (orig_sign != sign);
         prec = TYPE_PRECISION (TREE_TYPE (arg));
      }
      if (++cnt > 30)
         return prec + (orig_sign != sign);
   }
   if (CONVERT_EXPR_P (arg)
   && INTEGRAL_TYPE_P (TREE_TYPE (TREE_OPERAND (arg, 0)))
   && TYPE_PRECISION (TREE_TYPE (TREE_OPERAND (arg, 0))) > prec){
      /* We have e.g. (unsigned short) y_2 where int y_2 = (int) x_1(D);
      If y_2's min precision is smaller than prec, return that.  */
      int oprec = get_min_precision (TREE_OPERAND (arg, 0), sign);
      if (oprec < prec)
         return oprec + (orig_sign != sign);
   }
   if (TREE_CODE (arg) != SSA_NAME)
      return prec + (orig_sign != sign);
   int_range_max r;
   while (!get_global_range_query ()->range_of_expr (r, arg)  || r.varying_p () || r.undefined_p ()){
      gimple *g = SSA_NAME_DEF_STMT (arg);
      if (is_gimple_assign (g) && CONVERT_EXPR_CODE_P (gimple_assign_rhs_code (g))){
         tree t = gimple_assign_rhs1 (g);
         if (INTEGRAL_TYPE_P (TREE_TYPE (t))  && TYPE_PRECISION (TREE_TYPE (t)) <= prec){
            arg = t;
            if (TYPE_PRECISION (TREE_TYPE (arg)) < prec){
               if (TYPE_UNSIGNED (TREE_TYPE (arg)))
                  sign = UNSIGNED;
               else if (sign == UNSIGNED && get_range_pos_neg (arg) != 1)
                  return prec + (orig_sign != sign);
               prec = TYPE_PRECISION (TREE_TYPE (arg));
            }
            if (++cnt > 30)
               return prec + (orig_sign != sign);
            continue;
         }
      }
      return prec + (orig_sign != sign);
   }
   if (sign == TYPE_SIGN (TREE_TYPE (arg))){
      int p1 = wi::min_precision (r.lower_bound (), sign);
      int p2 = wi::min_precision (r.upper_bound (), sign);
      p1 = MAX (p1, p2);
      prec = MIN (prec, p1);
   }else if (sign == UNSIGNED && !wi::neg_p (r.lower_bound (), SIGNED)){
      int p = wi::min_precision (r.upper_bound (), UNSIGNED);
      prec = MIN (prec, p);
   }
   return prec + (orig_sign != sign);
}

/* Helper for expand_*_overflow.  Set the __imag__ part to true
   (1 except for signed:1 type, in which case store -1).  */

static void expand_arith_set_overflow (MtcsInternalFn *self,tree lhs, rtx target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   if (TYPE_PRECISION (TREE_TYPE (TREE_TYPE (lhs))) == 1
   && !TYPE_UNSIGNED (TREE_TYPE (TREE_TYPE (lhs))))
      mtcs_expr_write_complex_part/*!write_complex_part*/(mtcsExpr,target, constm1_rtx, true, false);
   else
      mtcs_expr_write_complex_part/*!write_complex_part*/(mtcsExpr,target, const1_rtx, true, false);
}

/* Helper for expand_*_overflow.  Store RES into the __real__ part
   of TARGET.  If RES has larger MODE than __real__ part of TARGET,
   set the __imag__ part to 1 if RES doesn't fit into it.  Similarly
   if LHS has smaller precision than its mode.  */

static void expand_arith_overflow_result_store (MtcsInternalFn *self,tree lhs, rtx target,
                scalar_int_mode mode, rtx res)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsExpmed   *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   scalar_int_mode tgtmode  = mtcs_mode_as_a <scalar_int_mode>(mtcsMode,GET_MODE_INNER (GET_MODE (target)));
   rtx lres = res;
   if (tgtmode != mode){
      rtx_code_label *done_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
      int uns = TYPE_UNSIGNED (TREE_TYPE (TREE_TYPE (lhs)));
      lres = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,tgtmode, mode, res, uns);
      gcc_assert (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,tgtmode) <
            mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode));
      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
            res, mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,mode, tgtmode, lres, uns),
            EQ, true, mode, NULL_RTX, NULL, done_label,profile_probability::very_likely ());
      expand_arith_set_overflow(self,lhs, target);
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,done_label);
   }
   int prec = TYPE_PRECISION (TREE_TYPE (TREE_TYPE (lhs)));
   int tgtprec = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,tgtmode);
   if (prec < tgtprec){
      rtx_code_label *done_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
      int uns = TYPE_UNSIGNED (TREE_TYPE (TREE_TYPE (lhs)));
      res = lres;
      if (uns){
         rtx mask = mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,
               wi::shifted_mask (0, prec, false, tgtprec),tgtmode);
         lres = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
               tgtmode, AND, res, mask, NULL_RTX, true, OPTAB_LIB_WIDEN);
      }else{
         lres = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,LSHIFT_EXPR, tgtmode, res, tgtprec - prec, NULL_RTX, 1);
         lres = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, tgtmode, lres, tgtprec - prec,NULL_RTX, 0);
      }
      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
            res, lres,EQ, true, tgtmode, NULL_RTX, NULL, done_label,profile_probability::very_likely ());
      expand_arith_set_overflow(self,lhs, target);
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,done_label);
   }
   mtcs_expr_write_complex_part/*!write_complex_part*/(mtcsExpr,target, lres, false, false);
}

/* Helper for expand_*_overflow.  Store RES into TARGET.  */
static void expand_ubsan_result_store (MtcsInternalFn *self,tree lhs, rtx target, scalar_int_mode mode,
            rtx res, rtx_code_label *do_error)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsExpmed   *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   if (TREE_CODE (TREE_TYPE (lhs)) == BITINT_TYPE
   && TYPE_PRECISION (TREE_TYPE (lhs)) < mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode)){
   int uns = TYPE_UNSIGNED (TREE_TYPE (lhs));
   int prec = TYPE_PRECISION (TREE_TYPE (lhs));
   int tgtprec = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode);
   rtx resc = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode), lres;
   mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,resc, res);
   if (uns){
      rtx mask = mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,wi::shifted_mask (0, prec, false, tgtprec),mode);
      lres = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,mode, AND, res, mask, NULL_RTX,true, OPTAB_LIB_WIDEN);
   }else{
      lres = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,LSHIFT_EXPR, mode, res, tgtprec - prec, NULL_RTX, 1);
      lres = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, mode, lres, tgtprec - prec,NULL_RTX, 0);
   }
   if (lres != res)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,res, lres);
      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
            res, resc, NE, true, mode, NULL_RTX, NULL, do_error,profile_probability::very_unlikely ());
   }
   if (GET_CODE (target) == SUBREG && SUBREG_PROMOTED_VAR_P (target))
      /* If this is a scalar in a register that is stored in a wider mode
      than the declared mode, compute the result into its declared mode
      and then convert to the wider mode.  Our value is the computed
      expression.  */
      mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,SUBREG_REG (target), res, SUBREG_PROMOTED_SIGN (target));
   else
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, res);
}

/* Add sub/add overflow checking to the statement STMT.
   CODE says whether the operation is +, or -.  */
//原型 expand_addsub_overflow internal-fn.h internal-fn.cc
void mtcs_internal_fn_expand_addsub_overflow (MtcsInternalFn *self,location_t loc, tree_code code, tree lhs,
         tree arg0, tree arg1, bool unsr_p, bool uns0_p,
         bool uns1_p, bool is_ubsan, tree *datap)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsExpmed   *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   rtx res, target = NULL_RTX;
   tree fn;
   rtx_code_label *done_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
   rtx_code_label *do_error = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
   mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
   rtx op0 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg0);
   rtx op1 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg1);
   scalar_int_mode mode = SCALAR_INT_TYPE_MODE (TREE_TYPE (arg0));
   int prec = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode);
   rtx sgn = mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,wi::min_value (prec, SIGNED), mode);
   bool do_xor = false;

   if (is_ubsan)
      gcc_assert (!unsr_p && !uns0_p && !uns1_p);

   if (lhs){
      target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
      if (!is_ubsan)
         mtcs_expr_write_complex_part/*!write_complex_part*/(mtcsExpr,target, const0_rtx, true, false);
   }

   /* We assume both operands and result have the same precision
   here (GET_MODE_BITSIZE (mode)), S stands for signed type
   with that precision, U for unsigned type with that precision,
   sgn for unsigned most significant bit in that precision.
   s1 is signed first operand, u1 is unsigned first operand,
   s2 is signed second operand, u2 is unsigned second operand,
   sr is signed result, ur is unsigned result and the following
   rules say how to compute result (which is always result of
   the operands as if both were unsigned, cast to the right
   signedness) and how to compute whether operation overflowed.

   s1 + s2 -> sr
   res = (S) ((U) s1 + (U) s2)
   ovf = s2 < 0 ? res > s1 : res < s1 (or jump on overflow)
   s1 - s2 -> sr
   res = (S) ((U) s1 - (U) s2)
   ovf = s2 < 0 ? res < s1 : res > s2 (or jump on overflow)
   u1 + u2 -> ur
   res = u1 + u2
   ovf = res < u1 (or jump on carry, but RTL opts will handle it)
   u1 - u2 -> ur
   res = u1 - u2
   ovf = res > u1 (or jump on carry, but RTL opts will handle it)
   s1 + u2 -> sr
   res = (S) ((U) s1 + u2)
   ovf = ((U) res ^ sgn) < u2
   s1 + u2 -> ur
   t1 = (S) (u2 ^ sgn)
   t2 = s1 + t1
   res = (U) t2 ^ sgn
   ovf = t1 < 0 ? t2 > s1 : t2 < s1 (or jump on overflow)
   s1 - u2 -> sr
   res = (S) ((U) s1 - u2)
   ovf = u2 > ((U) s1 ^ sgn)
   s1 - u2 -> ur
   res = (U) s1 - u2
   ovf = s1 < 0 || u2 > (U) s1
   u1 - s2 -> sr
   res = u1 - (U) s2
   ovf = u1 >= ((U) s2 ^ sgn)
   u1 - s2 -> ur
   t1 = u1 ^ sgn
   t2 = t1 - (U) s2
   res = t2 ^ sgn
   ovf = s2 < 0 ? (S) t2 < (S) t1 : (S) t2 > (S) t1 (or jump on overflow)
   s1 + s2 -> ur
   res = (U) s1 + (U) s2
   ovf = s2 < 0 ? (s1 | (S) res) < 0) : (s1 & (S) res) < 0)
   u1 + u2 -> sr
   res = (S) (u1 + u2)
   ovf = (U) res < u2 || res < 0
   u1 - u2 -> sr
   res = (S) (u1 - u2)
   ovf = u1 >= u2 ? res < 0 : res >= 0
   s1 - s2 -> ur
   res = (U) s1 - (U) s2
   ovf = s2 >= 0 ? ((s1 | (S) res) < 0) : ((s1 & (S) res) < 0)  */

   if (code == PLUS_EXPR && uns0_p && !uns1_p){
      /* PLUS_EXPR is commutative, if operand signedness differs,
      canonicalize to the first operand being signed and second
      unsigned to simplify following code.  */
      std::swap (op0, op1);
      std::swap (arg0, arg1);
      uns0_p = false;
      uns1_p = true;
   }

   /* u1 +- u2 -> ur  */
   if (uns0_p && uns1_p && unsr_p){
      insn_code icode =mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,code == PLUS_EXPR ? uaddv4_optab : usubv4_optab, mode);
      if (icode != CODE_FOR_nothing){
         class expand_operand ops[4];
         rtx_insn *last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);

         res = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
         create_output_operand (&ops[0], res, mode);
         create_input_operand (&ops[1], op0, mode);
         create_input_operand (&ops[2], op1, mode);
         create_fixed_operand (&ops[3], do_error);
         if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(mtcsOptabs,icode, 4, ops)){
            last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
            if (profile_status_for_fn (cfun) != PROFILE_ABSENT
            && JUMP_P (last)
            && any_condjump_p (last)
            && !find_reg_note (last, REG_BR_PROB, 0))
               add_reg_br_prob_note (last, profile_probability::very_unlikely ());
            mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,done_label);
            goto do_error_label;
         }

         mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
      }

      /* Compute the operation.  On RTL level, the addition is always
      unsigned.  */
      res = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
            mode, code == PLUS_EXPR ? add_optab : sub_optab, op0, op1, NULL_RTX, false, OPTAB_LIB_WIDEN);
      rtx tem = op0;
      /* For PLUS_EXPR, the operation is commutative, so we can pick
      operand to compare against.  For prec <= BITS_PER_WORD, I think
      preferring REG operand is better over CONST_INT, because
      the CONST_INT might enlarge the instruction or CSE would need
      to figure out we'd already loaded it into a register before.
      For prec > BITS_PER_WORD, I think CONST_INT might be more beneficial,
      as then the multi-word comparison can be perhaps simplified.  */
      if (code == PLUS_EXPR && (prec <= BITS_PER_WORD ? (CONST_SCALAR_INT_P (op0) && REG_P (op1)) : CONST_SCALAR_INT_P (op1)))
         tem = op1;
      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
            res, tem, code == PLUS_EXPR ? GEU : LEU,true, mode, NULL_RTX, NULL, done_label, profile_probability::very_likely ());
      goto do_error_label;
   }

   /* s1 +- u2 -> sr  */
   if (!uns0_p && uns1_p && !unsr_p){
      /* Compute the operation.  On RTL level, the addition is always
      unsigned.  */
      res = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
            mode, code == PLUS_EXPR ? add_optab : sub_optab, op0, op1, NULL_RTX, false, OPTAB_LIB_WIDEN);
      rtx tem = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
            mode, add_optab, code == PLUS_EXPR ? res : op0, sgn, NULL_RTX, false, OPTAB_LIB_WIDEN);
      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
            tem, op1, GEU, true, mode, NULL_RTX, NULL,done_label, profile_probability::very_likely ());
      goto do_error_label;
   }

   /* s1 + u2 -> ur  */
   if (code == PLUS_EXPR && !uns0_p && uns1_p && unsr_p){
      op1 = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
            mode, add_optab, op1, sgn, NULL_RTX, false,OPTAB_LIB_WIDEN);
      /* As we've changed op1, we have to avoid using the value range
      for the original argument.  */
      arg1 = error_mark_node;
      do_xor = true;
      goto do_signed;
   }

   /* u1 - s2 -> ur  */
   if (code == MINUS_EXPR && uns0_p && !uns1_p && unsr_p){
      op0 = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
            mode, add_optab, op0, sgn, NULL_RTX, false,OPTAB_LIB_WIDEN);
      /* As we've changed op0, we have to avoid using the value range
      for the original argument.  */
      arg0 = error_mark_node;
      do_xor = true;
      goto do_signed;
   }

   /* s1 - u2 -> ur  */
   if (code == MINUS_EXPR && !uns0_p && uns1_p && unsr_p){
      /* Compute the operation.  On RTL level, the addition is always
      unsigned.  */
      res = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,mode, sub_optab, op0, op1, NULL_RTX, false,OPTAB_LIB_WIDEN);
      int pos_neg = get_range_pos_neg (arg0);
      if (pos_neg == 2)
         /* If ARG0 is known to be always negative, this is always overflow.  */
         mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,do_error);
      else if (pos_neg == 3)
         /* If ARG0 is not known to be always positive, check at runtime.  */
         mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
               op0, const0_rtx, LT, false, mode, NULL_RTX, NULL, do_error, profile_probability::very_unlikely ());
      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
            op1, op0, LEU, true, mode, NULL_RTX, NULL, done_label, profile_probability::very_likely ());
      goto do_error_label;
   }

   /* u1 - s2 -> sr  */
   if (code == MINUS_EXPR && uns0_p && !uns1_p && !unsr_p){
      /* Compute the operation.  On RTL level, the addition is always
      unsigned.  */
      res = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
            mode, sub_optab, op0, op1, NULL_RTX, false, OPTAB_LIB_WIDEN);
      rtx tem = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
            mode, add_optab, op1, sgn, NULL_RTX, false, OPTAB_LIB_WIDEN);
      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
            op0, tem, LTU, true, mode, NULL_RTX, NULL,done_label, profile_probability::very_likely ());
      goto do_error_label;
   }

   /* u1 + u2 -> sr  */
   if (code == PLUS_EXPR && uns0_p && uns1_p && !unsr_p){
      /* Compute the operation.  On RTL level, the addition is always
      unsigned.  */
      res = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
            mode, add_optab, op0, op1, NULL_RTX, false,OPTAB_LIB_WIDEN);
      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
            res, const0_rtx, LT, false, mode, NULL_RTX, NULL, do_error, profile_probability::very_unlikely ());
      rtx tem = op1;
      /* The operation is commutative, so we can pick operand to compare
      against.  For prec <= BITS_PER_WORD, I think preferring REG operand
      is better over CONST_INT, because the CONST_INT might enlarge the
      instruction or CSE would need to figure out we'd already loaded it
      into a register before.  For prec > BITS_PER_WORD, I think CONST_INT
      might be more beneficial, as then the multi-word comparison can be
      perhaps simplified.  */
      if (prec <= BITS_PER_WORD? (CONST_SCALAR_INT_P (op1) && REG_P (op0)): CONST_SCALAR_INT_P (op0))
         tem = op0;
      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
            res, tem, GEU, true, mode, NULL_RTX, NULL,done_label, profile_probability::very_likely ());
      goto do_error_label;
   }

   /* s1 +- s2 -> ur  */
   if (!uns0_p && !uns1_p && unsr_p){
      /* Compute the operation.  On RTL level, the addition is always
      unsigned.  */
      res = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
            mode, code == PLUS_EXPR ? add_optab : sub_optab, op0, op1, NULL_RTX, false, OPTAB_LIB_WIDEN);
      int pos_neg = get_range_pos_neg (arg1);
      if (code == PLUS_EXPR){
         int pos_neg0 = get_range_pos_neg (arg0);
         if (pos_neg0 != 3 && pos_neg == 3){
            std::swap (op0, op1);
            pos_neg = pos_neg0;
         }
      }
      rtx tem;
      if (pos_neg != 3){
         tem = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,mode, ((pos_neg == 1) ^ (code == MINUS_EXPR))
               ? and_optab : ior_optab, op0, res, NULL_RTX, false, OPTAB_LIB_WIDEN);
         mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
               tem, const0_rtx, GE, false, mode, NULL,NULL, done_label, profile_probability::very_likely ());
      }else{
         rtx_code_label *do_ior_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
               op1, const0_rtx,code == MINUS_EXPR ? GE : LT, false, mode, NULL_RTX, NULL, do_ior_label,profile_probability::even ());
         tem = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,mode, and_optab, op0, res, NULL_RTX, false,OPTAB_LIB_WIDEN);
         mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
               tem, const0_rtx, GE, false, mode, NULL_RTX, NULL, done_label, profile_probability::very_likely ());
         mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,do_error);
         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,do_ior_label);
         tem = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
               mode, ior_optab, op0, res, NULL_RTX, false,OPTAB_LIB_WIDEN);
         mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
               tem, const0_rtx, GE, false, mode, NULL_RTX,NULL, done_label, profile_probability::very_likely ());
      }
      goto do_error_label;
   }

   /* u1 - u2 -> sr  */
   if (code == MINUS_EXPR && uns0_p && uns1_p && !unsr_p){
      /* Compute the operation.  On RTL level, the addition is always
      unsigned.  */
      res = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
            mode, sub_optab, op0, op1, NULL_RTX, false, OPTAB_LIB_WIDEN);
      rtx_code_label *op0_geu_op1 = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
            op0, op1, GEU, true, mode, NULL_RTX, NULL, op0_geu_op1, profile_probability::even ());
      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
            res, const0_rtx, LT, false, mode, NULL_RTX,NULL, done_label, profile_probability::very_likely ());
      mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,do_error);
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,op0_geu_op1);
      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
            res, const0_rtx, GE, false, mode, NULL_RTX,NULL, done_label, profile_probability::very_likely ());
      goto do_error_label;
   }

   gcc_assert (!uns0_p && !uns1_p && !unsr_p);

   /* s1 +- s2 -> sr  */
do_signed:
   {
      insn_code icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,
            code == PLUS_EXPR ? addv4_optab : subv4_optab, mode);
      if (icode != CODE_FOR_nothing){
         class expand_operand ops[4];
         rtx_insn *last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);

         res = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
         create_output_operand (&ops[0], res, mode);
         create_input_operand (&ops[1], op0, mode);
         create_input_operand (&ops[2], op1, mode);
         create_fixed_operand (&ops[3], do_error);
         if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(mtcsOptabs,icode, 4, ops)){
            last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
            if (profile_status_for_fn (cfun) != PROFILE_ABSENT
            && JUMP_P (last)
            && any_condjump_p (last)
            && !find_reg_note (last, REG_BR_PROB, 0))
               add_reg_br_prob_note (last,profile_probability::very_unlikely ());
            mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,done_label);
            goto do_error_label;
         }

         mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
      }

      /* Compute the operation.  On RTL level, the addition is always
      unsigned.  */
      res = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
            mode, code == PLUS_EXPR ? add_optab : sub_optab,op0, op1, NULL_RTX, false, OPTAB_LIB_WIDEN);

      /* If we can prove that one of the arguments (for MINUS_EXPR only
      the second operand, as subtraction is not commutative) is always
      non-negative or always negative, we can do just one comparison
      and conditional jump.  */
      int pos_neg = get_range_pos_neg (arg1);
      if (code == PLUS_EXPR){
         int pos_neg0 = get_range_pos_neg (arg0);
         if (pos_neg0 != 3 && pos_neg == 3){
            std::swap (op0, op1);
            pos_neg = pos_neg0;
         }
      }

      /* Addition overflows if and only if the two operands have the same sign,
      and the result has the opposite sign.  Subtraction overflows if and
      only if the two operands have opposite sign, and the subtrahend has
      the same sign as the result.  Here 0 is counted as positive.  */
      if (pos_neg == 3){
         /* Compute op0 ^ op1 (operands have opposite sign).  */
         rtx op_xor = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
               mode, xor_optab, op0, op1, NULL_RTX, false, OPTAB_LIB_WIDEN);

         /* Compute res ^ op1 (result and 2nd operand have opposite sign).  */
         rtx res_xor = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
               mode, xor_optab, res, op1, NULL_RTX, false,OPTAB_LIB_WIDEN);

         rtx tem;
         if (code == PLUS_EXPR){
            /* Compute (res ^ op1) & ~(op0 ^ op1).  */
            tem = expand_unop (mode, one_cmpl_optab, op_xor, NULL_RTX, false);
            tem = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
                  mode, and_optab, res_xor, tem, NULL_RTX, false,OPTAB_LIB_WIDEN);
         }else{
            /* Compute (op0 ^ op1) & ~(res ^ op1).  */
            tem = mtcs_optabs_expand_unop/*!expand_unop*/(mtcsOptabs,
                  mode, one_cmpl_optab, res_xor, NULL_RTX, false);
            tem = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
                  mode, and_optab, op_xor, tem, NULL_RTX, false, OPTAB_LIB_WIDEN);
         }

         /* No overflow if the result has bit sign cleared.  */
         mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
               tem, const0_rtx, GE, false, mode, NULL_RTX, NULL, done_label, profile_probability::very_likely ());
      }
      /* Compare the result of the operation with the first operand.
      No overflow for addition if second operand is positive and result
      is larger or second operand is negative and result is smaller.
      Likewise for subtraction with sign of second operand flipped.  */
      else
         mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
               res, op0,(pos_neg == 1) ^ (code == MINUS_EXPR) ? GE : LE, false, mode, NULL_RTX, NULL,
                     done_label,profile_probability::very_likely ());
   }

do_error_label:
   mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,do_error);
   if (is_ubsan){
      /* Expand the ubsan builtin call.  */
      mtcs_func_push_temp_slots/*!push_temp_slots*/(mtcsFunc);
      fn = ubsan_build_overflow_builtin (code, loc, TREE_TYPE (arg0),arg0, arg1, datap);
      mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,fn);
      mtcs_func_pop_temp_slots/*!pop_temp_slots*/(mtcsFunc);
      mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
   }else if (lhs)
      expand_arith_set_overflow(self,lhs, target);

   /* We're done.  */
   mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,done_label);

   if (lhs){
      if (is_ubsan)
         expand_ubsan_result_store(self,lhs, target, mode, res, do_error);
      else{
         if (do_xor)
            res = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,mode, add_optab, res, sgn, NULL_RTX, false, OPTAB_LIB_WIDEN);

         expand_arith_overflow_result_store(self,lhs, target, mode, res);
      }
   }
}

/* Add negate overflow checking to the statement STMT.  */
static void expand_neg_overflow (MtcsInternalFn *self,location_t loc, tree lhs,
      tree arg1, bool is_ubsan,tree *datap)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   rtx res, op1;
   tree fn;
   rtx_code_label *done_label, *do_error;
   rtx target = NULL_RTX;

   done_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
   do_error = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);

   mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
   op1 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg1);

   scalar_int_mode mode = mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,TREE_TYPE (arg1));
   if (lhs){
      target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
      if (!is_ubsan)
         mtcs_expr_write_complex_part/*!write_complex_part*/(mtcsExpr,target, const0_rtx, true, false);
   }

   enum insn_code icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,negv3_optab, mode);
   if (icode != CODE_FOR_nothing){
      class expand_operand ops[3];
      rtx_insn *last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);

      res = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
      create_output_operand (&ops[0], res, mode);
      create_input_operand (&ops[1], op1, mode);
      create_fixed_operand (&ops[2], do_error);
      if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(mtcsOptabs,icode, 3, ops)){
         last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
         if (profile_status_for_fn (cfun) != PROFILE_ABSENT
         && JUMP_P (last)
         && any_condjump_p (last)
         && !find_reg_note (last, REG_BR_PROB, 0))
            add_reg_br_prob_note (last,profile_probability::very_unlikely ());
         mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,done_label);
      }else{
         mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
         icode = CODE_FOR_nothing;
      }
   }

   if (icode == CODE_FOR_nothing){
      /* Compute the operation.  On RTL level, the addition is always
      unsigned.  */
      res = mtcs_optabs_expand_unop/*!expand_unop*/(mtcsOptabs,mode, neg_optab, op1, NULL_RTX, false);

      /* Compare the operand with the most negative value.  */
      rtx minv = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,TYPE_MIN_VALUE (TREE_TYPE (arg1)));
      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
            op1, minv, NE, true, mode, NULL_RTX, NULL,done_label, profile_probability::very_likely ());
   }

   mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,do_error);
   if (is_ubsan){
      /* Expand the ubsan builtin call.  */
      mtcs_func_push_temp_slots/*!push_temp_slots*/(mtcsFunc);
      fn = ubsan_build_overflow_builtin (NEGATE_EXPR, loc, TREE_TYPE (arg1),arg1, NULL_TREE, datap);
      mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,fn);
      mtcs_func_pop_temp_slots/*!pop_temp_slots*/(mtcsFunc);
      mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
   }else if (lhs)
      expand_arith_set_overflow(self,lhs, target);

   /* We're done.  */
   mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,done_label);

   if (lhs){
      if (is_ubsan)
         expand_ubsan_result_store(self,lhs, target, mode, res, do_error);
      else
         expand_arith_overflow_result_store(self,lhs, target, mode, res);
   }
}

/* Return true if UNS WIDEN_MULT_EXPR with result mode WMODE and operand
   mode MODE can be expanded without using a libcall.  */
static bool can_widen_mult_without_libcall (MtcsInternalFn *self,scalar_int_mode wmode, scalar_int_mode mode,
            rtx op0, rtx op1, bool uns)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsExpmed   *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   if (mtcs_optabs_find_widening_optab_handler/*!find_widening_optab_handler*/(mtcsOptabs,
         umul_widen_optab, wmode, mode) != CODE_FOR_nothing)
   return true;

   if (mtcs_optabs_find_widening_optab_handler/*!find_widening_optab_handler*/(mtcsOptabs,
         smul_widen_optab, wmode, mode) != CODE_FOR_nothing)
   return true;

   rtx_insn *last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
   if (CONSTANT_P (op0))
      op0 = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,wmode, mode, op0, uns);
   else
      op0 = mtcs_rtl_gen_raw_REG/*!gen_raw_REG*/(mtcsRTL,wmode, LAST_VIRTUAL_REGISTER + 1);
   if (CONSTANT_P (op1))
      op1 = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,wmode, mode, op1, uns);
   else
      op1 = mtcs_rtl_gen_raw_REG/*!gen_raw_REG*/(mtcsRTL,wmode, LAST_VIRTUAL_REGISTER + 2);
   rtx ret = mtcs_expmed_expand_mult/*!expand_mult*/(mtcsExpmed,wmode, op0, op1, NULL_RTX, uns, true);
   mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
   return ret != NULL_RTX;
}

/* Add mul overflow checking to the statement STMT.  */
static void expand_mul_overflow (MtcsInternalFn *self,location_t loc, tree lhs, tree arg0, tree arg1,
           bool unsr_p, bool uns0_p, bool uns1_p, bool is_ubsan, tree *datap)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsExpmed   *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsExplow   *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsTree   *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   rtx res, op0, op1;
   tree fn, type;
   rtx_code_label *done_label, *do_error;
   rtx target = NULL_RTX;
   signop sign;
   enum insn_code icode;
   int save_flag_trapv = flag_trapv;

   /* We don't want any __mulv?i3 etc. calls from the expansion of
   these internal functions, so disable -ftrapv temporarily.  */
   flag_trapv = 0;
   done_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
   do_error = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);

   mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
   op0 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg0);
   op1 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg1);

   scalar_int_mode mode = mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,TREE_TYPE (arg0));
   bool uns = unsr_p;
   if (lhs){
      target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
      if (!is_ubsan)
         mtcs_expr_write_complex_part/*!write_complex_part*/(mtcsExpr,target, const0_rtx, true, false);
   }

   if (is_ubsan)
      gcc_assert (!unsr_p && !uns0_p && !uns1_p);

   /* We assume both operands and result have the same precision
   here (GET_MODE_BITSIZE (mode)), S stands for signed type
   with that precision, U for unsigned type with that precision,
   sgn for unsigned most significant bit in that precision.
   s1 is signed first operand, u1 is unsigned first operand,
   s2 is signed second operand, u2 is unsigned second operand,
   sr is signed result, ur is unsigned result and the following
   rules say how to compute result (which is always result of
   the operands as if both were unsigned, cast to the right
   signedness) and how to compute whether operation overflowed.
   main_ovf (false) stands for jump on signed multiplication
   overflow or the main algorithm with uns == false.
   main_ovf (true) stands for jump on unsigned multiplication
   overflow or the main algorithm with uns == true.

   s1 * s2 -> sr
   res = (S) ((U) s1 * (U) s2)
   ovf = main_ovf (false)
   u1 * u2 -> ur
   res = u1 * u2
   ovf = main_ovf (true)
   s1 * u2 -> ur
   res = (U) s1 * u2
   ovf = (s1 < 0 && u2) || main_ovf (true)
   u1 * u2 -> sr
   res = (S) (u1 * u2)
   ovf = res < 0 || main_ovf (true)
   s1 * u2 -> sr
   res = (S) ((U) s1 * u2)
   ovf = (S) u2 >= 0 ? main_ovf (false)
   : (s1 != 0 && (s1 != -1 || u2 != (U) res))
   s1 * s2 -> ur
   t1 = (s1 & s2) < 0 ? (-(U) s1) : ((U) s1)
   t2 = (s1 & s2) < 0 ? (-(U) s2) : ((U) s2)
   res = t1 * t2
   ovf = (s1 ^ s2) < 0 ? (s1 && s2) : main_ovf (true)  */

   if (uns0_p && !uns1_p){
      /* Multiplication is commutative, if operand signedness differs,
      canonicalize to the first operand being signed and second
      unsigned to simplify following code.  */
      std::swap (op0, op1);
      std::swap (arg0, arg1);
      uns0_p = false;
      uns1_p = true;
   }

   int pos_neg0 = get_range_pos_neg (arg0);
   int pos_neg1 = get_range_pos_neg (arg1);
   /* Unsigned types with smaller than mode precision, even if they have most
   significant bit set, are still zero-extended.  */
   if (uns0_p && TYPE_PRECISION (TREE_TYPE (arg0)) < mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode))
      pos_neg0 = 1;
   if (uns1_p && TYPE_PRECISION (TREE_TYPE (arg1)) < mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode))
      pos_neg1 = 1;

   /* s1 * u2 -> ur  */
   if (!uns0_p && uns1_p && unsr_p){
      switch (pos_neg0){
         case 1:
            /* If s1 is non-negative, just perform normal u1 * u2 -> ur.  */
            goto do_main;
         case 2:
            /* If s1 is negative, avoid the main code, just multiply and
            signal overflow if op1 is not 0.  */
            struct separate_ops ops;
            ops.code = MULT_EXPR;
            ops.type = TREE_TYPE (arg1);
            ops.op0 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,ops.type, op0);
            ops.op1 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,ops.type, op1);
            ops.op2 = NULL_TREE;
            ops.location = loc;
            res = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(mtcsExpr,&ops, NULL_RTX, mode, EXPAND_NORMAL);
            mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                  op1, const0_rtx, EQ, true, mode, NULL_RTX,NULL, done_label, profile_probability::very_likely ());
            goto do_error_label;
         case 3:
            if (get_min_precision (arg1, UNSIGNED) + get_min_precision (arg0, SIGNED) <=
                  mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode)){
               /* If the first operand is sign extended from narrower type, the
               second operand is zero extended from narrower type and
               the sum of the two precisions is smaller or equal to the
               result precision: if the first argument is at runtime
               non-negative, maximum result will be 0x7e81 or 0x7f..fe80..01
               and there will be no overflow, if the first argument is
               negative and the second argument zero, the result will be
               0 and there will be no overflow, if the first argument is
               negative and the second argument positive, the result when
               treated as signed will be negative (minimum -0x7f80 or
               -0x7f..f80..0) there will be always overflow.  So, do
               res = (U) (s1 * u2)
               ovf = (S) res < 0  */
               struct separate_ops ops;
               ops.code = MULT_EXPR;
               ops.type = mtcs_tree_build_nonstandard_integer_type/*!build_nonstandard_integer_type*/(mtcsTree,
                     mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode),1);
               ops.op0 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,ops.type, op0);
               ops.op1 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,ops.type, op1);
               ops.op2 = NULL_TREE;
               ops.location = loc;
               res = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(mtcsExpr,&ops, NULL_RTX, mode, EXPAND_NORMAL);
               mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                     res, const0_rtx, GE, false, mode, NULL_RTX, NULL, done_label, profile_probability::very_likely ());
               goto do_error_label;
            }
            rtx_code_label *do_main_label;
            do_main_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
            mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                  op0, const0_rtx, GE, false, mode, NULL_RTX, NULL, do_main_label, profile_probability::very_likely ());
            mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                  op1, const0_rtx, EQ, true, mode, NULL_RTX, NULL, do_main_label, profile_probability::very_likely ());
            expand_arith_set_overflow(self,lhs, target);
            mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,do_main_label);
            goto do_main;
         default:
            gcc_unreachable ();
      }
   }

   /* u1 * u2 -> sr  */
   if (uns0_p && uns1_p && !unsr_p){
      if ((pos_neg0 | pos_neg1) == 1){
         /* If both arguments are zero extended from narrower types,
         the MSB will be clear on both and so we can pretend it is
         a normal s1 * s2 -> sr multiplication.  */
         uns0_p = false;
         uns1_p = false;
      }else
         uns = true;
      /* Rest of handling of this case after res is computed.  */
      goto do_main;
   }

   /* s1 * u2 -> sr  */
   if (!uns0_p && uns1_p && !unsr_p){
      switch (pos_neg1){
         case 1:
            goto do_main;
         case 2:
            /* If (S) u2 is negative (i.e. u2 is larger than maximum of S,
            avoid the main code, just multiply and signal overflow
            unless 0 * u2 or -1 * ((U) Smin).  */
            struct separate_ops ops;
            ops.code = MULT_EXPR;
            ops.type = TREE_TYPE (arg1);
            ops.op0 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,ops.type, op0);
            ops.op1 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,ops.type, op1);
            ops.op2 = NULL_TREE;
            ops.location = loc;
            res = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(mtcsExpr,&ops, NULL_RTX, mode, EXPAND_NORMAL);
            mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                  op0, const0_rtx, EQ, true, mode, NULL_RTX,NULL, done_label, profile_probability::very_likely ());
            mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                  op0, constm1_rtx, NE, true, mode, NULL_RTX,NULL, do_error, profile_probability::very_unlikely ());
            int prec;
            prec = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode);
            rtx sgn;
            sgn = mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,wi::min_value (prec, SIGNED), mode);
            mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                  op1, sgn, EQ, true, mode, NULL_RTX,NULL, done_label, profile_probability::very_likely ());
            goto do_error_label;
         case 3:
            /* Rest of handling of this case after res is computed.  */
            goto do_main;
         default:
            gcc_unreachable ();
      }
   }

   /* s1 * s2 -> ur  */
   if (!uns0_p && !uns1_p && unsr_p){
      rtx tem;
      switch (pos_neg0 | pos_neg1){
         case 1: /* Both operands known to be non-negative.  */
            goto do_main;
         case 2: /* Both operands known to be negative.  */
            op0 = mtcs_optabs_expand_unop/*!expand_unop*/(mtcsOptabs,mode, neg_optab, op0, NULL_RTX, false);
            op1 = mtcs_optabs_expand_unop/*!expand_unop*/(mtcsOptabs,mode, neg_optab, op1, NULL_RTX, false);
            /* Avoid looking at arg0/arg1 ranges, as we've changed
            the arguments.  */
            arg0 = error_mark_node;
            arg1 = error_mark_node;
            goto do_main;
         case 3:
            if ((pos_neg0 ^ pos_neg1) == 3){
               /* If one operand is known to be negative and the other
               non-negative, this overflows always, unless the non-negative
               one is 0.  Just do normal multiply and set overflow
               unless one of the operands is 0.  */
               struct separate_ops ops;
               ops.code = MULT_EXPR;
               ops.type = mtcs_tree_build_nonstandard_integer_type/*!build_nonstandard_integer_type*/(mtcsTree,
                     mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode),1);
               ops.op0 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,ops.type, op0);
               ops.op1 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,ops.type, op1);
               ops.op2 = NULL_TREE;
               ops.location = loc;
               res = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(mtcsExpr,&ops, NULL_RTX, mode, EXPAND_NORMAL);
               mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                     pos_neg0 == 1 ? op0 : op1, const0_rtx, EQ, true, mode, NULL_RTX, NULL,
                           done_label,profile_probability::very_likely ());
               goto do_error_label;
            }
            if (get_min_precision (arg0, SIGNED) + get_min_precision (arg1, SIGNED) <=
                  mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode)){
               /* If both operands are sign extended from narrower types and
               the sum of the two precisions is smaller or equal to the
               result precision: if both arguments are at runtime
               non-negative, maximum result will be 0x3f01 or 0x3f..f0..01
               and there will be no overflow, if both arguments are negative,
               maximum result will be 0x40..00 and there will be no overflow
               either, if one argument is positive and the other argument
               negative, the result when treated as signed will be negative
               and there will be always overflow, and if one argument is
               zero and the other negative the result will be zero and no
               overflow.  So, do
               res = (U) (s1 * s2)
               ovf = (S) res < 0  */
               struct separate_ops ops;
               ops.code = MULT_EXPR;
               ops.type = mtcs_tree_build_nonstandard_integer_type/*!build_nonstandard_integer_type*/(mtcsTree,
                     mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode),
               1);
               ops.op0 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,ops.type, op0);
               ops.op1 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,ops.type, op1);
               ops.op2 = NULL_TREE;
               ops.location = loc;
               res = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(mtcsExpr,&ops, NULL_RTX, mode, EXPAND_NORMAL);
               mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                     res, const0_rtx, GE, false,mode, NULL_RTX, NULL, done_label,profile_probability::very_likely ());
               goto do_error_label;
            }
            /* The general case, do all the needed comparisons at runtime.  */
            rtx_code_label *do_main_label, *after_negate_label;
            rtx rop0, rop1;
            rop0 = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
            rop1 = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,rop0, op0);
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,rop1, op1);
            op0 = rop0;
            op1 = rop1;
            do_main_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
            after_negate_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
            tem = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
                  mode, and_optab, op0, op1, NULL_RTX, false,OPTAB_LIB_WIDEN);
            mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                  tem, const0_rtx, GE, false, mode, NULL_RTX, NULL, after_negate_label, profile_probability::very_likely ());
            /* Both arguments negative here, negate them and continue with
            normal unsigned overflow checking multiplication.  */
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,
                  op0, mtcs_optabs_expand_unop/*!expand_unop*/(mtcsOptabs,mode, neg_optab, op0,NULL_RTX, false));
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,
                  op1, mtcs_optabs_expand_unop/*!expand_unop*/(mtcsOptabs,mode, neg_optab, op1,NULL_RTX, false));
            /* Avoid looking at arg0/arg1 ranges, as we might have changed
            the arguments.  */
            arg0 = error_mark_node;
            arg1 = error_mark_node;
            mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,do_main_label);
            mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,after_negate_label);
            tem = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
                  mode, xor_optab, op0, op1, NULL_RTX, false, OPTAB_LIB_WIDEN);
            mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                  tem, const0_rtx, GE, false, mode, NULL_RTX,NULL, do_main_label,profile_probability::very_likely ());
            /* One argument is negative here, the other positive.  This
            overflows always, unless one of the arguments is 0.  But
            if e.g. s2 is 0, (U) s1 * 0 doesn't overflow, whatever s1
            is, thus we can keep do_main code oring in overflow as is.  */
            if (pos_neg0 != 2)
               mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                     op0, const0_rtx, EQ, true, mode, NULL_RTX,NULL, do_main_label,profile_probability::very_unlikely ());
            if (pos_neg1 != 2)
               mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                     op1, const0_rtx, EQ, true, mode, NULL_RTX, NULL, do_main_label, profile_probability::very_unlikely ());
            expand_arith_set_overflow(self,lhs, target);
            mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,do_main_label);
            goto do_main;
         default:
            gcc_unreachable ();
      }
   }

do_main:
   type = mtcs_tree_build_nonstandard_integer_type/*!build_nonstandard_integer_type*/(mtcsTree,
         mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode), uns);
   sign = uns ? UNSIGNED : SIGNED;
   icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,uns ? umulv4_optab : mulv4_optab, mode);
   if (uns && (integer_pow2p (arg0) || integer_pow2p (arg1))
   && (optimize_insn_for_speed_p () || icode == CODE_FOR_nothing)) {
      /* Optimize unsigned multiplication by power of 2 constant
      using 2 shifts, one for result, one to extract the shifted
      out bits to see if they are all zero.
      Don't do this if optimizing for size and we have umulv4_optab,
      in that case assume multiplication will be shorter.
      This is heuristics based on the single target that provides
      umulv4 right now (i?86/x86_64), if further targets add it, this
      might need to be revisited.
      Cases where both operands are constant should be folded already
      during GIMPLE, and cases where one operand is constant but not
      power of 2 are questionable, either the WIDEN_MULT_EXPR case
      below can be done without multiplication, just by shifts and adds,
      or we'd need to divide the result (and hope it actually doesn't
      really divide nor multiply) and compare the result of the division
      with the original operand.  */
      rtx opn0 = op0;
      rtx opn1 = op1;
      tree argn0 = arg0;
      tree argn1 = arg1;
      if (integer_pow2p (arg0)){
         std::swap (opn0, opn1);
         std::swap (argn0, argn1);
      }
      int cnt = tree_log2 (argn1);
      if (cnt >= 0 && cnt < mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode)){
         rtx upper = const0_rtx;
         res = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,LSHIFT_EXPR, mode, opn0, cnt, NULL_RTX, uns);
         if (cnt != 0)
            upper = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,
                  RSHIFT_EXPR, mode, opn0,mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode) - cnt, NULL_RTX, uns);
         mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
               upper, const0_rtx, EQ, true, mode,NULL_RTX, NULL, done_label,profile_probability::very_likely ());
         goto do_error_label;
      }
   }
   if (icode != CODE_FOR_nothing){
      class expand_operand ops[4];
      rtx_insn *last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);

      res = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
      create_output_operand (&ops[0], res, mode);
      create_input_operand (&ops[1], op0, mode);
      create_input_operand (&ops[2], op1, mode);
      create_fixed_operand (&ops[3], do_error);
      if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(mtcsOptabs,icode, 4, ops)){
         last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
         if (profile_status_for_fn (cfun) != PROFILE_ABSENT
         && JUMP_P (last)
         && any_condjump_p (last)
         && !find_reg_note (last, REG_BR_PROB, 0))
            add_reg_br_prob_note (last, profile_probability::very_unlikely ());
         mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,done_label);
      }else{
         mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
         icode = CODE_FOR_nothing;
      }
   }

   if (icode == CODE_FOR_nothing){
      struct separate_ops ops;
      int prec = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode);
      scalar_int_mode hmode, wmode;
      ops.op0 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,type, op0);
      ops.op1 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,type, op1);
      ops.op2 = NULL_TREE;
      ops.location = loc;

      /* Optimize unsigned overflow check where we don't use the
      multiplication result, just whether overflow happened.
      If we can do MULT_HIGHPART_EXPR, that followed by
      comparison of the result against zero is cheapest.
      We'll still compute res, but it should be DCEd later.  */
      use_operand_p use;
      gimple *use_stmt;
      if (!is_ubsan
      && lhs
      && uns
      && !(uns0_p && uns1_p && !unsr_p)
      && can_mult_highpart_p (mode, uns) == 1
      && single_imm_use (lhs, &use, &use_stmt)
      && is_gimple_assign (use_stmt)
      && gimple_assign_rhs_code (use_stmt) == IMAGPART_EXPR)
         goto highpart;

      if (GET_MODE_2XWIDER_MODE (mode).exists (&wmode)
      && targetm.scalar_mode_supported_p (wmode)
      && can_widen_mult_without_libcall(self,wmode, mode, op0, op1, uns)){
   twoxwider:
         ops.code = WIDEN_MULT_EXPR;
         ops.type = mtcs_tree_build_nonstandard_integer_type/*!build_nonstandard_integer_type*/(mtcsTree,
               mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,wmode), uns);

         res = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(mtcsExpr,&ops, NULL_RTX, wmode, EXPAND_NORMAL);
         rtx hipart = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, wmode, res, prec, NULL_RTX, uns);
         hipart = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,mode, wmode, hipart, uns);
         res = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,mode, wmode, res, uns);
         if (uns)
            /* For the unsigned multiplication, there was overflow if
            HIPART is non-zero.  */
            mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                  hipart, const0_rtx, EQ, true, mode, NULL_RTX, NULL, done_label,profile_probability::very_likely ());
         else{
            /* RES is used more than once, place it in a pseudo.  */
            res = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,mode, res);

            rtx signbit = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, mode, res, prec - 1, NULL_RTX, 0);
            /* RES is low half of the double width result, HIPART
            the high half.  There was overflow if
            HIPART is different from RES < 0 ? -1 : 0.  */
            mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                  signbit, hipart, EQ, true, mode,NULL_RTX, NULL, done_label,profile_probability::very_likely ());
         }
      }else if (can_mult_highpart_p (mode, uns) == 1){
   highpart:
         ops.code = MULT_HIGHPART_EXPR;
         ops.type = type;

         rtx hipart = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(mtcsExpr,&ops, NULL_RTX, mode,EXPAND_NORMAL);
         ops.code = MULT_EXPR;
         res = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(mtcsExpr,&ops, NULL_RTX, mode, EXPAND_NORMAL);
         if (uns)
            /* For the unsigned multiplication, there was overflow if
            HIPART is non-zero.  */
            mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                  hipart, const0_rtx, EQ, true, mode,NULL_RTX, NULL, done_label,profile_probability::very_likely ());
         else{
            rtx signbit = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, mode, res, prec - 1,NULL_RTX, 0);
            /* RES is low half of the double width result, HIPART
            the high half.  There was overflow if
            HIPART is different from RES < 0 ? -1 : 0.  */
            mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                  signbit, hipart, EQ, true, mode,NULL_RTX, NULL, done_label,profile_probability::very_likely ());
         }

      }else if (mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,prec / 2, 1).exists (&hmode)
            && 2 * mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,hmode) == prec){
         rtx_code_label *large_op0 = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         rtx_code_label *small_op0_large_op1 = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         rtx_code_label *one_small_one_large = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         rtx_code_label *both_ops_large = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         rtx_code_label *after_hipart_neg = uns ? NULL : mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         rtx_code_label *after_lopart_neg = uns ? NULL : mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         rtx_code_label *do_overflow = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         rtx_code_label *hipart_different = uns ? NULL : mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);

         unsigned int hprec = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,hmode);
         rtx hipart0 = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, mode, op0, hprec, NULL_RTX, uns);
         hipart0 = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,hmode, mode, hipart0, uns);
         rtx lopart0 = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,hmode, mode, op0, uns);
         rtx signbit0 = const0_rtx;
         if (!uns)
            signbit0 = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, hmode, lopart0, hprec - 1,NULL_RTX, 0);
         rtx hipart1 = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, mode, op1, hprec, NULL_RTX, uns);
         hipart1 = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,hmode, mode, hipart1, uns);
         rtx lopart1 = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,hmode, mode, op1, uns);
         rtx signbit1 = const0_rtx;
         if (!uns)
            signbit1 = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, hmode, lopart1, hprec - 1,NULL_RTX, 0);

         res = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

         /* True if op0 resp. op1 are known to be in the range of
         halfstype.  */
         bool op0_small_p = false;
         bool op1_small_p = false;
         /* True if op0 resp. op1 are known to have all zeros or all ones
         in the upper half of bits, but are not known to be
         op{0,1}_small_p.  */
         bool op0_medium_p = false;
         bool op1_medium_p = false;
         /* -1 if op{0,1} is known to be negative, 0 if it is known to be
         nonnegative, 1 if unknown.  */
         int op0_sign = 1;
         int op1_sign = 1;

         if (pos_neg0 == 1)
            op0_sign = 0;
         else if (pos_neg0 == 2)
            op0_sign = -1;
         if (pos_neg1 == 1)
            op1_sign = 0;
         else if (pos_neg1 == 2)
            op1_sign = -1;

         unsigned int mprec0 = prec;
         if (arg0 != error_mark_node)
            mprec0 = get_min_precision (arg0, sign);
         if (mprec0 <= hprec)
            op0_small_p = true;
         else if (!uns && mprec0 <= hprec + 1)
            op0_medium_p = true;
         unsigned int mprec1 = prec;
         if (arg1 != error_mark_node)
            mprec1 = get_min_precision (arg1, sign);
         if (mprec1 <= hprec)
            op1_small_p = true;
         else if (!uns && mprec1 <= hprec + 1)
            op1_medium_p = true;

         int smaller_sign = 1;
         int larger_sign = 1;
         if (op0_small_p){
            smaller_sign = op0_sign;
            larger_sign = op1_sign;
         }else if (op1_small_p){
            smaller_sign = op1_sign;
            larger_sign = op0_sign;
         }else if (op0_sign == op1_sign){
            smaller_sign = op0_sign;
            larger_sign = op0_sign;
         }

         if (!op0_small_p)
            mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                  signbit0, hipart0, NE, true, hmode,NULL_RTX, NULL, large_op0,profile_probability::unlikely ());

         if (!op1_small_p)
            mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                  signbit1, hipart1, NE, true, hmode,NULL_RTX, NULL, small_op0_large_op1,profile_probability::unlikely ());

         /* If both op0 and op1 are sign (!uns) or zero (uns) extended from
         hmode to mode, the multiplication will never overflow.  We can
         do just one hmode x hmode => mode widening multiplication.  */
         tree halfstype = mtcs_tree_build_nonstandard_integer_type/*!build_nonstandard_integer_type*/(mtcsTree,hprec, uns);
         ops.op0 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,halfstype, lopart0);
         ops.op1 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,halfstype, lopart1);
         ops.code = WIDEN_MULT_EXPR;
         ops.type = type;
         rtx thisres = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(mtcsExpr,&ops, NULL_RTX, mode, EXPAND_NORMAL);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,res, thisres);
         mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,done_label);

         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,small_op0_large_op1);

         /* If op0 is sign (!uns) or zero (uns) extended from hmode to mode,
         but op1 is not, just swap the arguments and handle it as op1
         sign/zero extended, op0 not.  */
         rtx larger = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
         rtx hipart = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,hmode);
         rtx lopart = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,hmode);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,larger, op1);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,hipart, hipart1);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,lopart, lopart0);
         mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,one_small_one_large);

         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,large_op0);

         if (!op1_small_p)
            mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                  signbit1, hipart1, NE, true, hmode,NULL_RTX, NULL, both_ops_large,profile_probability::unlikely ());

         /* If op1 is sign (!uns) or zero (uns) extended from hmode to mode,
         but op0 is not, prepare larger, hipart and lopart pseudos and
         handle it together with small_op0_large_op1.  */
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,larger, op0);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,hipart, hipart0);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,lopart, lopart1);

         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,one_small_one_large);

         /* lopart is the low part of the operand that is sign extended
         to mode, larger is the other operand, hipart is the
         high part of larger and lopart0 and lopart1 are the low parts
         of both operands.
         We perform lopart0 * lopart1 and lopart * hipart widening
         multiplications.  */
         tree halfutype = mtcs_tree_build_nonstandard_integer_type/*!build_nonstandard_integer_type*/(mtcsTree,hprec, 1);
         ops.op0 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,halfutype, lopart0);
         ops.op1 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,halfutype, lopart1);
         rtx lo0xlo1 = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(mtcsExpr,&ops, NULL_RTX, mode, EXPAND_NORMAL);

         ops.op0 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,halfutype, lopart);
         ops.op1 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,halfutype, hipart);
         rtx loxhi = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
         rtx tem = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(mtcsExpr,&ops, NULL_RTX, mode, EXPAND_NORMAL);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,loxhi, tem);

         if (!uns){
            /* if (hipart < 0) loxhi -= lopart << (bitsize / 2);  */
            if (larger_sign == 0)
               mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,after_hipart_neg);
            else if (larger_sign != -1)
               mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                     hipart, const0_rtx, GE, false, hmode, NULL_RTX, NULL, after_hipart_neg, profile_probability::even ());

            tem = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,mode, hmode, lopart, 1);
            tem = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,LSHIFT_EXPR, mode, tem, hprec, NULL_RTX, 1);
            tem = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,mode, MINUS, loxhi, tem, NULL_RTX,1, OPTAB_WIDEN);
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,loxhi, tem);

            mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,after_hipart_neg);

            /* if (lopart < 0) loxhi -= larger;  */
            if (smaller_sign == 0)
               mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,after_lopart_neg);
            else if (smaller_sign != -1)
               mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                     lopart, const0_rtx, GE, false, hmode,NULL_RTX, NULL, after_lopart_neg, profile_probability::even ());

            tem = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,mode, MINUS, loxhi, larger, NULL_RTX, 1, OPTAB_WIDEN);
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,loxhi, tem);

            mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,after_lopart_neg);
         }

         /* loxhi += (uns) lo0xlo1 >> (bitsize / 2);  */
         tem = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, mode, lo0xlo1, hprec, NULL_RTX, 1);
         tem = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,mode, PLUS, loxhi, tem, NULL_RTX,1, OPTAB_WIDEN);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,loxhi, tem);

         /* if (loxhi >> (bitsize / 2)
         == (hmode) loxhi >> (bitsize / 2 - 1))  (if !uns)
         if (loxhi >> (bitsize / 2) == 0       (if uns).  */
         rtx hipartloxhi = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, mode, loxhi, hprec,NULL_RTX, 0);
         hipartloxhi = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,hmode, mode, hipartloxhi, 0);
         rtx signbitloxhi = const0_rtx;
         if (!uns)
            signbitloxhi = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,
                  RSHIFT_EXPR, hmode, mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,
                        hmode, mode,loxhi, 0), hprec - 1, NULL_RTX, 0);

         mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
               signbitloxhi, hipartloxhi, NE, true, hmode, NULL_RTX, NULL, do_overflow, profile_probability::very_unlikely ());

         /* res = (loxhi << (bitsize / 2)) | (hmode) lo0xlo1;  */
         rtx loxhishifted = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,
               LSHIFT_EXPR, mode, loxhi, hprec, NULL_RTX, 1);
         tem = mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,
               mode, hmode,mtcs_expr_convert_modes/*!convert_modes*/(mtcsExpr,hmode, mode, lo0xlo1, 1), 1);

         tem = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,mode, IOR, loxhishifted, tem, res,1, OPTAB_WIDEN);
         if (tem != res)
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,res, tem);
         mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,done_label);

         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,both_ops_large);

         /* If both operands are large (not sign (!uns) or zero (uns)
         extended from hmode), then perform the full multiplication
         which will be the result of the operation.
         The only cases which don't overflow are for signed multiplication
         some cases where both hipart0 and highpart1 are 0 or -1.
         For unsigned multiplication when high parts are both non-zero
         this overflows always.  */
         ops.code = MULT_EXPR;
         ops.op0 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,type, op0);
         ops.op1 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,type, op1);
         tem = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(mtcsExpr,&ops, NULL_RTX, mode, EXPAND_NORMAL);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,res, tem);

         if (!uns){
            if (!op0_medium_p){
               tem = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
                     hmode, PLUS, hipart0, const1_rtx,NULL_RTX, 1, OPTAB_WIDEN);
               mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                     tem, const1_rtx, GTU, true, hmode,NULL_RTX, NULL, do_error,profile_probability::very_unlikely ());
            }

            if (!op1_medium_p){
               tem = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
                     hmode, PLUS, hipart1, const1_rtx,NULL_RTX, 1, OPTAB_WIDEN);
               mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                     tem, const1_rtx, GTU, true, hmode, NULL_RTX, NULL, do_error,profile_probability::very_unlikely ());
            }

            /* At this point hipart{0,1} are both in [-1, 0].  If they are
            the same, overflow happened if res is non-positive, if they
            are different, overflow happened if res is positive.  */
            if (op0_sign != 1 && op1_sign != 1 && op0_sign != op1_sign)
               mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,hipart_different);
            else if (op0_sign == 1 || op1_sign == 1)
               mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                     hipart0, hipart1, NE, true, hmode,NULL_RTX, NULL, hipart_different,profile_probability::even ());

            mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                  res, const0_rtx, LE, false, mode,NULL_RTX, NULL, do_error,profile_probability::very_unlikely ());
            mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,done_label);

            mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,hipart_different);

            mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                  res, const0_rtx, GE, false, mode,NULL_RTX, NULL, do_error,profile_probability::very_unlikely ());
            mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,done_label);
         }

         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,do_overflow);

         /* Overflow, do full multiplication and fallthru into do_error.  */
         ops.op0 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,type, op0);
         ops.op1 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,type, op1);
         tem = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(mtcsExpr,&ops, NULL_RTX, mode, EXPAND_NORMAL);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,res, tem);
      }else if (GET_MODE_2XWIDER_MODE (mode).exists (&wmode) && targetm.scalar_mode_supported_p (wmode))
         /* Even emitting a libcall is better than not detecting overflow
         at all.  */
         goto twoxwider;
      else{
         gcc_assert (!is_ubsan);
         ops.code = MULT_EXPR;
         ops.type = type;
         res = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(mtcsExpr,&ops, NULL_RTX, mode, EXPAND_NORMAL);
         mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,done_label);
      }
   }

do_error_label:
   mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,do_error);
   if (is_ubsan){
      /* Expand the ubsan builtin call.  */
      mtcs_func_push_temp_slots/*!push_temp_slots*/(mtcsFunc);
      fn = ubsan_build_overflow_builtin (MULT_EXPR, loc, TREE_TYPE (arg0),arg0, arg1, datap);
      mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,fn);
      mtcs_func_pop_temp_slots/*!pop_temp_slots*/(mtcsFunc);
      mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
   }else if (lhs)
      expand_arith_set_overflow(self,lhs, target);

   /* We're done.  */
   mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,done_label);

   /* u1 * u2 -> sr  */
   if (uns0_p && uns1_p && !unsr_p){
      rtx_code_label *all_done_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
            res, const0_rtx, GE, false, mode, NULL_RTX,NULL, all_done_label, profile_probability::very_likely ());
      expand_arith_set_overflow(self,lhs, target);
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,all_done_label);
   }

   /* s1 * u2 -> sr  */
   if (!uns0_p && uns1_p && !unsr_p && pos_neg1 == 3){
      rtx_code_label *all_done_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
      rtx_code_label *set_noovf = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,op1,
            const0_rtx, GE, false, mode, NULL_RTX,NULL, all_done_label, profile_probability::very_likely ());
      expand_arith_set_overflow(self,lhs, target);
      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
            op0, const0_rtx, EQ, true, mode, NULL_RTX, NULL, set_noovf, profile_probability::very_likely ());
      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
            op0, constm1_rtx, NE, true, mode, NULL_RTX,NULL, all_done_label, profile_probability::very_unlikely ());
      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
            op1, res, NE, true, mode, NULL_RTX, NULL,all_done_label, profile_probability::very_unlikely ());
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,set_noovf);
      mtcs_expr_write_complex_part/*!write_complex_part*/(mtcsExpr,target, const0_rtx, true, false);
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,all_done_label);
   }

   if (lhs){
      if (is_ubsan)
         expand_ubsan_result_store(self,lhs, target, mode, res, do_error);
      else
         expand_arith_overflow_result_store(self,lhs, target, mode, res);
   }
   flag_trapv = save_flag_trapv;
}

/* Expand UBSAN_CHECK_* internal function if it has vector operands.  */
static void expand_vector_ubsan_overflow (MtcsInternalFn *self,location_t loc, enum tree_code code, tree lhs,
               tree arg0, tree arg1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsTree   *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsConst   *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);

   poly_uint64 cnt = TYPE_VECTOR_SUBPARTS (TREE_TYPE (arg0));
   rtx_code_label *loop_lab = NULL;
   rtx cntvar = NULL_RTX;
   tree cntv = NULL_TREE;
   tree eltype = TREE_TYPE (TREE_TYPE (arg0));
   tree sz = TYPE_SIZE (eltype);
   tree data = NULL_TREE;
   tree resv = NULL_TREE;
   rtx lhsr = NULL_RTX;
   rtx resvr = NULL_RTX;
   unsigned HOST_WIDE_INT const_cnt = 0;
   bool use_loop_p = (!cnt.is_constant (&const_cnt) || const_cnt > 4);
   int save_flag_trapv = flag_trapv;

   /* We don't want any __mulv?i3 etc. calls from the expansion of
   these internal functions, so disable -ftrapv temporarily.  */
   flag_trapv = 0;
   if (lhs){
      optab op;
      lhsr = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
      if (!mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,GET_MODE (lhsr))
      || (op = optab_for_tree_code (code, TREE_TYPE (arg0),optab_default)) == unknown_optab
      || (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,op, TYPE_MODE (TREE_TYPE (arg0))) == CODE_FOR_nothing)){
         if (MEM_P (lhsr))
            resv = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,TREE_TYPE (lhs), lhsr);
         else{
            resvr = mtcs_func_assign_temp/*!assign_temp*/(mtcsFunc,TREE_TYPE (lhs), 1, 1);
            resv = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,TREE_TYPE (lhs), resvr);
         }
      }
   }
   if (use_loop_p){
      mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
      loop_lab = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
      cntvar = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,TYPE_MODE (sizetype));
      cntv = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,sizetype, cntvar);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,cntvar, const0_rtx);
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,loop_lab);
   }
   if (TREE_CODE (arg0) != VECTOR_CST){
      rtx arg0r = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg0);
      arg0 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,TREE_TYPE (arg0), arg0r);
   }
   if (TREE_CODE (arg1) != VECTOR_CST){
      rtx arg1r = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg1);
      arg1 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,TREE_TYPE (arg1), arg1r);
   }
   for (unsigned int i = 0; i < (use_loop_p ? 1 : const_cnt); i++){
      tree op0, op1, res = NULL_TREE;
      if (use_loop_p){
         tree atype = build_array_type_nelts (eltype, cnt);
         op0 = uniform_vector_p (arg0);
         if (op0 == NULL_TREE){
            op0 = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(mtcsConst,loc, VIEW_CONVERT_EXPR, atype, arg0);
            op0 = build4_loc (loc, ARRAY_REF, eltype, op0, cntv, NULL_TREE, NULL_TREE);
         }
         op1 = uniform_vector_p (arg1);
         if (op1 == NULL_TREE){
            op1 = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(mtcsConst,loc, VIEW_CONVERT_EXPR, atype, arg1);
            op1 = build4_loc (loc, ARRAY_REF, eltype, op1, cntv, NULL_TREE, NULL_TREE);
         }
         if (resv){
            res = mtcs_const_fold_build1_loc/*!fold_build1_loc*/(mtcsConst,loc, VIEW_CONVERT_EXPR, atype, resv);
            res = build4_loc (loc, ARRAY_REF, eltype, res, cntv,NULL_TREE, NULL_TREE);
         }
      }else{
         tree bitpos = mtcs_tree_bitsize_int/*!bitsize_int*/(mtcsTree,tree_to_uhwi (sz) * i);
         op0 = fold_build3_loc (loc, BIT_FIELD_REF, eltype, arg0, sz, bitpos);
         op1 = fold_build3_loc (loc, BIT_FIELD_REF, eltype, arg1, sz, bitpos);
         if (resv)
            res = fold_build3_loc (loc, BIT_FIELD_REF, eltype, resv, sz,bitpos);
      }
      switch (code){
         case PLUS_EXPR:
            mtcs_internal_fn_expand_addsub_overflow/*!expand_addsub_overflow*/(self,
                  loc, PLUS_EXPR, res, op0, op1,false, false, false, true, &data);
            break;
         case MINUS_EXPR:
            if (use_loop_p ? integer_zerop (arg0) : integer_zerop (op0))
               expand_neg_overflow(self,loc, res, op1, true, &data);
            else
               mtcs_internal_fn_expand_addsub_overflow/*!expand_addsub_overflow*/(self,
                     loc, MINUS_EXPR, res, op0, op1,false, false, false, true, &data);
            break;
         case MULT_EXPR:
            expand_mul_overflow(self,loc, res, op0, op1, false, false, false,true, &data);
            break;
         default:
            gcc_unreachable ();
      }
   }
   if (use_loop_p){
      struct separate_ops ops;
      ops.code = PLUS_EXPR;
      ops.type = TREE_TYPE (cntv);
      ops.op0 = cntv;
      ops.op1 = mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (cntv), 1);
      ops.op2 = NULL_TREE;
      ops.location = loc;
      rtx ret = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(mtcsExpr,&ops, cntvar, TYPE_MODE (sizetype),EXPAND_NORMAL);
      if (ret != cntvar)
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,cntvar, ret);
      rtx cntrtx = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,cnt, TYPE_MODE (sizetype));
      mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
            cntvar, cntrtx, NE, false,TYPE_MODE (sizetype), NULL_RTX, NULL, loop_lab,profile_probability::very_likely ());
   }
   if (lhs && resv == NULL_TREE){
      struct separate_ops ops;
      ops.code = code;
      ops.type = TREE_TYPE (arg0);
      ops.op0 = arg0;
      ops.op1 = arg1;
      ops.op2 = NULL_TREE;
      ops.location = loc;
      rtx ret = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(mtcsExpr,&ops, lhsr, TYPE_MODE (TREE_TYPE (arg0)),EXPAND_NORMAL);
      if (ret != lhsr)
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,lhsr, ret);
   }else if (resvr)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,lhsr, resvr);
   flag_trapv = save_flag_trapv;
}

/* Expand UBSAN_CHECK_ADD call STMT.  */
static void mtcs_expand_UBSAN_CHECK_ADD (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   location_t loc = gimple_location (stmt);
   tree lhs = gimple_call_lhs (stmt);
   tree arg0 = gimple_call_arg (stmt, 0);
   tree arg1 = gimple_call_arg (stmt, 1);
   if (VECTOR_TYPE_P (TREE_TYPE (arg0)))
      expand_vector_ubsan_overflow(self,loc, PLUS_EXPR, lhs, arg0, arg1);
   else
      mtcs_internal_fn_expand_addsub_overflow/*!expand_addsub_overflow*/(self,
            loc, PLUS_EXPR, lhs, arg0, arg1,false, false, false, true, NULL);
}

/* Expand UBSAN_CHECK_SUB call STMT.  */
static void mtcs_expand_UBSAN_CHECK_SUB (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   location_t loc = gimple_location (stmt);
   tree lhs = gimple_call_lhs (stmt);
   tree arg0 = gimple_call_arg (stmt, 0);
   tree arg1 = gimple_call_arg (stmt, 1);
   if (VECTOR_TYPE_P (TREE_TYPE (arg0)))
      expand_vector_ubsan_overflow(self,loc, MINUS_EXPR, lhs, arg0, arg1);
   else if (integer_zerop (arg0))
      expand_neg_overflow(self,loc, lhs, arg1, true, NULL);
   else
      mtcs_internal_fn_expand_addsub_overflow/*!expand_addsub_overflow*/(self,
            loc, MINUS_EXPR, lhs, arg0, arg1,false, false, false, true, NULL);
}

/* Expand UBSAN_CHECK_MUL call STMT.  */
static void mtcs_expand_UBSAN_CHECK_MUL (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   location_t loc = gimple_location (stmt);
   tree lhs = gimple_call_lhs (stmt);
   tree arg0 = gimple_call_arg (stmt, 0);
   tree arg1 = gimple_call_arg (stmt, 1);
   if (VECTOR_TYPE_P (TREE_TYPE (arg0)))
      expand_vector_ubsan_overflow(self,loc, MULT_EXPR, lhs, arg0, arg1);
   else
      expand_mul_overflow(self,loc, lhs, arg0, arg1, false, false, false, true,NULL);
}

/* Helper function for {ADD,SUB,MUL}_OVERFLOW call stmt expansion.  */
static void expand_arith_overflow (MtcsInternalFn *self,enum tree_code code, gimple *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsTree   *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsConst   *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree lhs = gimple_call_lhs (stmt);
   if (lhs == NULL_TREE)
      return;
   tree arg0 = gimple_call_arg (stmt, 0);
   tree arg1 = gimple_call_arg (stmt, 1);
   tree type = TREE_TYPE (TREE_TYPE (lhs));
   int uns0_p = TYPE_UNSIGNED (TREE_TYPE (arg0));
   int uns1_p = TYPE_UNSIGNED (TREE_TYPE (arg1));
   int unsr_p = TYPE_UNSIGNED (type);
   int prec0 = TYPE_PRECISION (TREE_TYPE (arg0));
   int prec1 = TYPE_PRECISION (TREE_TYPE (arg1));
   int precres = TYPE_PRECISION (type);
   location_t loc = gimple_location (stmt);
   if (!uns0_p && get_range_pos_neg (arg0) == 1)
      uns0_p = true;
   if (!uns1_p && get_range_pos_neg (arg1) == 1)
      uns1_p = true;
   int pr = get_min_precision (arg0, uns0_p ? UNSIGNED : SIGNED);
   prec0 = MIN (prec0, pr);
   pr = get_min_precision (arg1, uns1_p ? UNSIGNED : SIGNED);
   prec1 = MIN (prec1, pr);
   int save_flag_trapv = flag_trapv;

   /* We don't want any __mulv?i3 etc. calls from the expansion of
   these internal functions, so disable -ftrapv temporarily.  */
   flag_trapv = 0;
   /* If uns0_p && uns1_p, precop is minimum needed precision
   of unsigned type to hold the exact result, otherwise
   precop is minimum needed precision of signed type to
   hold the exact result.  */
   int precop;
   if (code == MULT_EXPR)
      precop = prec0 + prec1 + (uns0_p != uns1_p);
   else{
      if (uns0_p == uns1_p)
         precop = MAX (prec0, prec1) + 1;
      else if (uns0_p)
         precop = MAX (prec0 + 1, prec1) + 1;
      else
         precop = MAX (prec0, prec1 + 1) + 1;
   }
   int orig_precres = precres;

   do{
      if ((uns0_p && uns1_p) ? ((precop + !unsr_p) <= precres
      /* u1 - u2 -> ur can overflow, no matter what precision
      the result has.  */
      && (code != MINUS_EXPR || !unsr_p)) : (!unsr_p && precop <= precres)){
         /* The infinity precision result will always fit into result.  */
         rtx target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
         mtcs_expr_write_complex_part/*!write_complex_part*/(mtcsExpr,target, const0_rtx, true, false);
         scalar_int_mode mode = mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,type);
         struct separate_ops ops;
         ops.code = code;
         ops.type = type;
         ops.op0 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, type, arg0);
         ops.op1 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, type, arg1);
         ops.op2 = NULL_TREE;
         ops.location = loc;
         rtx tem = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(mtcsExpr,&ops, NULL_RTX, mode, EXPAND_NORMAL);
         expand_arith_overflow_result_store(self,lhs, target, mode, tem);
         flag_trapv = save_flag_trapv;
         return;
      }

      /* For operations with low precision, if target doesn't have them, start
      with precres widening right away, otherwise do it only if the most
      simple cases can't be used.  */
      const int min_precision = targetm.min_arithmetic_precision ();
      if (orig_precres == precres && precres < min_precision)
         ;
      else if ((uns0_p && uns1_p && unsr_p && prec0 <= precres  && prec1 <= precres)
      || ((!uns0_p || !uns1_p) && !unsr_p   && prec0 + uns0_p <= precres  && prec1 + uns1_p <= precres)){
         arg0 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, type, arg0);
         arg1 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, type, arg1);
         switch (code){
            case MINUS_EXPR:
               if (integer_zerop (arg0) && !unsr_p){
                  expand_neg_overflow(self,loc, lhs, arg1, false, NULL);
                  flag_trapv = save_flag_trapv;
                  return;
               }
            /* FALLTHRU */
            case PLUS_EXPR:
               mtcs_internal_fn_expand_addsub_overflow/*!expand_addsub_overflow*/(self,
                     loc, code, lhs, arg0, arg1, unsr_p,unsr_p, unsr_p, false, NULL);
               flag_trapv = save_flag_trapv;
               return;
            case MULT_EXPR:
               expand_mul_overflow(self,loc, lhs, arg0, arg1, unsr_p,unsr_p, unsr_p, false, NULL);
               flag_trapv = save_flag_trapv;
               return;
            default:
               gcc_unreachable ();
         }
      }

      /* For sub-word operations, retry with a wider type first.  */
      if (orig_precres == precres && precop <= BITS_PER_WORD){
         int p = MAX (min_precision, precop);
         scalar_int_mode m = mtcs_mode_smallest_int_mode_for_size/*!smallest_int_mode_for_size*/(mtcsMode,p);
         tree optype = mtcs_tree_build_nonstandard_integer_type/*!build_nonstandard_integer_type*/(mtcsTree,
               mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,m),  uns0_p && uns1_p && unsr_p);
         p = TYPE_PRECISION (optype);
         if (p > precres){
            precres = p;
            unsr_p = TYPE_UNSIGNED (optype);
            type = optype;
            continue;
         }
      }

      if (prec0 <= precres && prec1 <= precres){
         tree types[2];
         if (unsr_p){
            types[0] = mtcs_tree_build_nonstandard_integer_type/*!build_nonstandard_integer_type*/(mtcsTree,precres, 0);
            types[1] = type;
         }else{
            types[0] = type;
            types[1] = mtcs_tree_build_nonstandard_integer_type/*!build_nonstandard_integer_type*/(mtcsTree,precres, 1);
         }
         arg0 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, types[uns0_p], arg0);
         arg1 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, types[uns1_p], arg1);
         if (code != MULT_EXPR)
            mtcs_internal_fn_expand_addsub_overflow/*!expand_addsub_overflow*/(self,
                  loc, code, lhs, arg0, arg1, unsr_p,uns0_p, uns1_p, false, NULL);
         else
            expand_mul_overflow(self,loc, lhs, arg0, arg1, unsr_p,uns0_p, uns1_p, false, NULL);
         flag_trapv = save_flag_trapv;
         return;
      }

      /* Retry with a wider type.  */
      if (orig_precres == precres){
         int p = MAX (prec0, prec1);
         scalar_int_mode m = mtcs_mode_smallest_int_mode_for_size/*!smallest_int_mode_for_size*/(mtcsMode,p);
         tree optype = mtcs_tree_build_nonstandard_integer_type/*!build_nonstandard_integer_type*/(mtcsTree,
               mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,m), uns0_p && uns1_p && unsr_p);
         p = TYPE_PRECISION (optype);
         if (p > precres){
            precres = p;
            unsr_p = TYPE_UNSIGNED (optype);
            type = optype;
            continue;
         }
      }

      gcc_unreachable ();
   }while (1);
}

/* Expand ADD_OVERFLOW STMT.  */
static void mtcs_expand_ADD_OVERFLOW(MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   expand_arith_overflow(self,PLUS_EXPR, stmt);
}

/* Expand SUB_OVERFLOW STMT.  */
static void mtcs_expand_SUB_OVERFLOW (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   expand_arith_overflow(self,MINUS_EXPR, stmt);
}

/* Expand MUL_OVERFLOW STMT.  */
static void mtcs_expand_MUL_OVERFLOW (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   expand_arith_overflow(self,MULT_EXPR, stmt);
}

/* Expand UADDC STMT.  */
static void mtcs_expand_UADDC (MtcsInternalFn *self,internal_fn ifn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   tree lhs = gimple_call_lhs (stmt);
   tree arg1 = gimple_call_arg (stmt, 0);
   tree arg2 = gimple_call_arg (stmt, 1);
   tree arg3 = gimple_call_arg (stmt, 2);
   tree type = TREE_TYPE (arg1);
   machine_mode mode = TYPE_MODE (type);
   insn_code icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,
         ifn == IFN_UADDC? uaddc5_optab : usubc5_optab, mode);
   rtx op1 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg1);
   rtx op2 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg2);
   rtx op3 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg3);
   rtx target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   rtx re = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
   rtx im = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
   class expand_operand ops[5];
   create_output_operand (&ops[0], re, mode);
   create_output_operand (&ops[1], im, mode);
   create_input_operand (&ops[2], op1, mode);
   create_input_operand (&ops[3], op2, mode);
   create_input_operand (&ops[4], op3, mode);
   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,icode, 5, ops);
   mtcs_expr_write_complex_part/*!write_complex_part*/(mtcsExpr,target, re, false, false);
   mtcs_expr_write_complex_part/*!write_complex_part*/(mtcsExpr,target, im, true, false);
}

/* Expand USUBC STMT.  */
static void mtcs_expand_USUBC (MtcsInternalFn *self,internal_fn ifn, gcall *stmt)
{
   mtcs_expand_UADDC(self,ifn, stmt);
}

/* This should get folded in tree-vectorizer.cc.  */
static void mtcs_expand_LOOP_VECTORIZED (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This should get folded in tree-vectorizer.cc.  */
static void mtcs_expand_LOOP_DIST_ALIAS (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* Return a memory reference of type TYPE for argument INDEX of STMT.
   Use argument INDEX + 1 to derive the second (TBAA) operand.  */
static tree expand_call_mem_ref (MtcsInternalFn *self,tree type, gcall *stmt, int index)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree   *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree addr = gimple_call_arg (stmt, index);
   tree alias_ptr_type = TREE_TYPE (gimple_call_arg (stmt, index + 1));
   unsigned int align = tree_to_shwi (gimple_call_arg (stmt, index + 1));
   if (TYPE_ALIGN (type) != align)
      type = build_aligned_type (type, align);

   tree tmp = addr;
   if (TREE_CODE (tmp) == SSA_NAME){
      gimple *def = get_gimple_for_ssa_name (tmp);
      if (def && gimple_assign_single_p (def))
         tmp = gimple_assign_rhs1 (def);
   }

   if (TREE_CODE (tmp) == ADDR_EXPR){
      tree mem = TREE_OPERAND (tmp, 0);
      if (TREE_CODE (mem) == TARGET_MEM_REF && types_compatible_p (TREE_TYPE (mem), type)){
         tree offset = TMR_OFFSET (mem);
         if (type != TREE_TYPE (mem) || alias_ptr_type != TREE_TYPE (offset) || !integer_zerop (offset)){
            mem = copy_node (mem);
            TMR_OFFSET (mem) = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,alias_ptr_type, wi::to_poly_wide (offset));
            TREE_TYPE (mem) = type;
         }
         return mem;
      }
   }

   return mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,
         MEM_REF, type, addr, mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,alias_ptr_type, 0));
}

/* Expand MASK_LOAD{,_LANES}, MASK_LEN_LOAD or LEN_LOAD call STMT using optab
 * OPTAB.  */
static void expand_partial_load_optab_fn (MtcsInternalFn *self,internal_fn ifn, gcall *stmt, convert_optab optab)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   int i = 0;
   class expand_operand ops[6];
   tree type, lhs, rhs, maskt;
   rtx mem, target;
   insn_code icode;

   maskt = gimple_call_arg (stmt, internal_fn_mask_index (ifn));
   lhs = gimple_call_lhs (stmt);
   if (lhs == NULL_TREE)
   return;
   type = TREE_TYPE (lhs);
   rhs = expand_call_mem_ref(self,type, stmt, 0);

   if (optab == vec_mask_load_lanes_optab || optab == vec_mask_len_load_lanes_optab)
      icode = get_multi_vector_move(self,type, optab);
   else if (optab == len_load_optab)
      icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,optab, TYPE_MODE (type));
   else
      icode = mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,optab, TYPE_MODE (type),TYPE_MODE (TREE_TYPE (maskt)));

   mem = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,rhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   gcc_assert (MEM_P (mem));
   /* The built MEM_REF does not accurately reflect that the load
   is only partial.  Clear it.  */
   mtcs_rtl_set_mem_expr/*!set_mem_expr*/(mtcsRTL,mem, NULL_TREE);
   mtcs_rtl_clear_mem_offset/*!clear_mem_offset*/(mtcsRTL,mem);
   target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   create_call_lhs_operand (&ops[i++], target, TYPE_MODE (type));
   create_fixed_operand (&ops[i++], mem);
   i = add_mask_else_and_len_args(self,ops, i, stmt);
   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,icode, i, ops);

   assign_call_lhs(self,lhs, target, &ops[0]);
}

#define expand_mask_load_optab_fn expand_partial_load_optab_fn
#define expand_mask_load_lanes_optab_fn expand_mask_load_optab_fn
#define expand_len_load_optab_fn expand_partial_load_optab_fn
#define expand_mask_len_load_optab_fn expand_partial_load_optab_fn

/* Expand MASK_STORE{,_LANES}, MASK_LEN_STORE or LEN_STORE call STMT using optab
 * OPTAB.  */
static void expand_partial_store_optab_fn (MtcsInternalFn *self,internal_fn ifn, gcall *stmt, convert_optab optab)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);

   int i = 0;
   class expand_operand ops[5];
   tree type, lhs, rhs, maskt;
   rtx mem, reg;
   insn_code icode;

   maskt = gimple_call_arg (stmt, internal_fn_mask_index (ifn));
   rhs = gimple_call_arg (stmt, internal_fn_stored_value_index (ifn));
   type = TREE_TYPE (rhs);
   lhs = expand_call_mem_ref(self,type, stmt, 0);

   if (optab == vec_mask_store_lanes_optab || optab == vec_mask_len_store_lanes_optab)
      icode = get_multi_vector_move(self,type, optab);
   else if (optab == len_store_optab)
      icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,optab, TYPE_MODE (type));
   else
      icode = mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,
            optab, TYPE_MODE (type),TYPE_MODE (TREE_TYPE (maskt)));

   mem = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   gcc_assert (MEM_P (mem));
   /* The built MEM_REF does not accurately reflect that the store
   is only partial.  Clear it.  */
   mtcs_rtl_set_mem_expr/*!set_mem_expr*/(mtcsRTL,mem, NULL_TREE);
   mtcs_rtl_clear_mem_offset/*!clear_mem_offset*/(mtcsRTL,mem);
   reg = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,rhs);
   create_fixed_operand (&ops[i++], mem);
   create_input_operand (&ops[i++], reg, TYPE_MODE (type));
   i = add_mask_else_and_len_args(self,ops, i, stmt);
   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,icode, i, ops);
}

#define expand_mask_store_optab_fn expand_partial_store_optab_fn
#define expand_mask_store_lanes_optab_fn expand_mask_store_optab_fn
#define expand_len_store_optab_fn expand_partial_store_optab_fn
#define expand_mask_len_store_optab_fn expand_partial_store_optab_fn

/* Expand VCOND_MASK optab internal function.
   The expansion of STMT happens based on OPTAB table associated.  */
static void expand_vec_cond_mask_optab_fn (MtcsInternalFn *self,internal_fn, gcall *stmt, convert_optab optab)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);

   class expand_operand ops[4];

   tree lhs = gimple_call_lhs (stmt);
   tree op0 = gimple_call_arg (stmt, 0);
   tree op1 = gimple_call_arg (stmt, 1);
   tree op2 = gimple_call_arg (stmt, 2);
   tree vec_cond_type = TREE_TYPE (lhs);

   machine_mode mode = TYPE_MODE (vec_cond_type);
   machine_mode mask_mode = TYPE_MODE (TREE_TYPE (op0));
   enum insn_code icode = mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,optab, mode, mask_mode);
   rtx mask, rtx_op1, rtx_op2;

   gcc_assert (icode != CODE_FOR_nothing);

   mask = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,op0);
   rtx_op1 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,op1);
   rtx_op2 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,op2);

   rtx target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   create_call_lhs_operand (&ops[0], target, mode);
   create_input_operand (&ops[1], rtx_op1, mode);
   create_input_operand (&ops[2], rtx_op2, mode);
   create_input_operand (&ops[3], mask, mask_mode);
   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,icode, 4, ops);
   assign_call_lhs(self,lhs, target, &ops[0]);
}

/* Expand VEC_SET internal functions.  */
static void expand_vec_set_optab_fn (MtcsInternalFn *self,internal_fn, gcall *stmt, convert_optab optab)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   tree lhs = gimple_call_lhs (stmt);
   tree op0 = gimple_call_arg (stmt, 0);
   tree op1 = gimple_call_arg (stmt, 1);
   tree op2 = gimple_call_arg (stmt, 2);
   rtx target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   rtx src = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,op0);

   machine_mode outermode = TYPE_MODE (TREE_TYPE (op0));
   scalar_mode innermode = mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,outermode);

   rtx value = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,op1);
   rtx pos = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,op2);

   class expand_operand ops[3];
   enum insn_code icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,optab, outermode);

   if (icode != CODE_FOR_nothing){
      rtx temp = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,outermode);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,temp, src);

      create_fixed_operand (&ops[0], temp);
      create_input_operand (&ops[1], value, innermode);
      create_convert_operand_from (&ops[2], pos, TYPE_MODE (TREE_TYPE (op2)),true);
      if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(mtcsOptabs,icode, 3, ops)){
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, temp);
         return;
      }
   }
   gcc_unreachable ();
}

static void mtcs_expand_ABNORMAL_DISPATCHER (MtcsInternalFn *self,internal_fn, gcall *)
{
}

static void mtcs_expand_BUILTIN_EXPECT (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   /* When guessing was done, the hints should be already stripped away.  */
   gcc_assert (!flag_guess_branch_prob || optimize == 0 || seen_error ());

   rtx target;
   tree lhs = gimple_call_lhs (stmt);
   if (lhs)
      target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   else
      target = const0_rtx;
   rtx val = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,gimple_call_arg (stmt, 0), target, VOIDmode, EXPAND_NORMAL);
   if (lhs && val != target)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, val);
}

/* IFN_VA_ARG is supposed to be expanded at pass_stdarg.  So this dummy function
   should never be called.  */

static void mtcs_expand_VA_ARG (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* IFN_VEC_CONVERT is supposed to be expanded at pass_lower_vector.  So this
   dummy function should never be called.  */

static void mtcs_expand_VEC_CONVERT (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* Expand IFN_RAWMEMCHR internal function.  */
static void mtcs_expand_RAWMEMCHR (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);

   expand_operand ops[3];

   tree lhs = gimple_call_lhs (stmt);
   if (!lhs)
      return;
   machine_mode lhs_mode = TYPE_MODE (TREE_TYPE (lhs));
   rtx lhs_rtx = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   create_call_lhs_operand (&ops[0], lhs_rtx, lhs_mode);

   tree mem = gimple_call_arg (stmt, 0);
   rtx mem_rtx = get_memory_rtx (mem, NULL);
   create_fixed_operand (&ops[1], mem_rtx);

   tree pattern = gimple_call_arg (stmt, 1);
   machine_mode mode = TYPE_MODE (TREE_TYPE (pattern));
   rtx pattern_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,pattern);
   create_input_operand (&ops[2], pattern_rtx, mode);

   insn_code icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,rawmemchr_optab, mode);

   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,icode, 3, ops);
   assign_call_lhs(self,lhs, lhs_rtx, &ops[0]);
}

/* Expand the IFN_UNIQUE function according to its first argument.  */
static void mtcs_expand_UNIQUE (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

   rtx pattern = NULL_RTX;
   enum ifn_unique_kind kind = (enum ifn_unique_kind) TREE_INT_CST_LOW (gimple_call_arg (stmt, 0));

   switch (kind){
      default:
      gcc_unreachable ();

      case IFN_UNIQUE_UNSPEC:
         if (target_rtx_have_unique/*!targetm.have_unique*/(mtcsMachine->tmrtx))
            pattern = target_rtx_gen_unique/*!targetm.gen_unique*/(mtcsMachine->tmrtx);
         break;

      case IFN_UNIQUE_OACC_FORK:
      case IFN_UNIQUE_OACC_JOIN:
         if (target_rtx_have_oacc_fork/*!targetm.have_oacc_fork*/(mtcsMachine->tmrtx)
               && target_rtx_have_oacc_join/*!targetm.have_oacc_join*/(mtcsMachine->tmrtx)){
            tree lhs = gimple_call_lhs (stmt);
            rtx target = const0_rtx;

            if (lhs)
               target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);

            rtx data_dep = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,gimple_call_arg (stmt, 1));
            rtx axis = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,gimple_call_arg (stmt, 2));

            if (kind == IFN_UNIQUE_OACC_FORK)
               pattern = target_rtx_gen_oacc_fork/*!targetm.gen_oacc_fork*/(mtcsMachine->tmrtx,target, data_dep, axis);
            else
               pattern = target_rtx_gen_oacc_join/*!targetm.gen_oacc_join*/(mtcsMachine->tmrtx,target, data_dep, axis);
            }else
               gcc_unreachable ();
            break;
         }

   if (pattern)
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,pattern);
}

/* Expand the IFN_DEFERRED_INIT function:
   LHS = DEFERRED_INIT (SIZE of the DECL, INIT_TYPE, NAME of the DECL);

   Initialize the LHS with zero/pattern according to its second argument
   INIT_TYPE:
   if INIT_TYPE is AUTO_INIT_ZERO, use zeroes to initialize;
   if INIT_TYPE is AUTO_INIT_PATTERN, use 0xFE byte-repeatable pattern
     to initialize;
   The LHS variable is initialized including paddings.
   The reasons to choose 0xFE for pattern initialization are:
     1. It is a non-canonical virtual address on x86_64, and at the
   high end of the i386 kernel address space.
     2. It is a very large float value (-1.694739530317379e+38).
     3. It is also an unusual number for integers.  */
#define INIT_PATTERN_VALUE  0xFE
static void mtcs_expand_DEFERRED_INIT (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsTree   *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsConst   *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsBuiltins   *mtcsBuiltins=mtcs_target_get_builtins(mtcsTarget);

   tree lhs = gimple_call_lhs (stmt);
   tree var_size = gimple_call_arg (stmt, 0);
   enum auto_init_type init_type = (enum auto_init_type) TREE_INT_CST_LOW (gimple_call_arg (stmt, 1));
   bool reg_lhs = true;

   tree var_type = TREE_TYPE (lhs);
   gcc_assert (init_type > AUTO_INIT_UNINITIALIZED);

   if (TREE_CODE (lhs) == SSA_NAME)
      reg_lhs = true;
   else{
      tree lhs_base = lhs;
      while (handled_component_p (lhs_base))
         lhs_base = TREE_OPERAND (lhs_base, 0);
      reg_lhs = (mem_ref_refers_to_non_mem_p (lhs_base) || non_mem_decl_p (lhs_base));
      /* If this expands to a register and the underlying decl is wrapped in
      a MEM_REF that just serves as an access type change expose the decl
      if it is of correct size.  This avoids a situation as in PR103271
      if the target does not support a direct move to the registers mode.  */
      if (reg_lhs
      && TREE_CODE (lhs_base) == MEM_REF
      && TREE_CODE (TREE_OPERAND (lhs_base, 0)) == ADDR_EXPR
      && DECL_P (TREE_OPERAND (TREE_OPERAND (lhs_base, 0), 0))
      && integer_zerop (TREE_OPERAND (lhs_base, 1))
      && tree_fits_uhwi_p (var_size)
      && tree_int_cst_equal (var_size, DECL_SIZE_UNIT (TREE_OPERAND (TREE_OPERAND (lhs_base, 0), 0)))){
         lhs = TREE_OPERAND (TREE_OPERAND (lhs_base, 0), 0);
         var_type = TREE_TYPE (lhs);
      }
   }

   if (!reg_lhs){
      /* If the variable is not in register, expand to a memset
      to initialize it.  */
      mark_addressable (lhs);
      tree var_addr = mtcs_const_build_fold_addr_expr/*!build_fold_addr_expr*/(mtcsConst,lhs);

      tree value = (init_type == AUTO_INIT_PATTERN)
      ? mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,integer_type_node,INIT_PATTERN_VALUE): integer_zero_node;
      tree m_call =mtcs_tree_build_call_expr/*!build_call_expr*/(mtcsTree,
            builtin_decl_implicit (BUILT_IN_MEMSET), 3, var_addr, value, var_size);
      /* Expand this memset call.  */
      mtcs_builtins_expand_builtin_memset/*!expand_builtin_memset*/(mtcsBuiltins,m_call, NULL_RTX, TYPE_MODE (var_type));
   }else{
      /* If this variable is in a register use expand_assignment.
      For boolean scalars force zero-init.  */
      tree init;
      scalar_int_mode var_mode;
      if (TREE_CODE (TREE_TYPE (lhs)) != BOOLEAN_TYPE
      && tree_fits_uhwi_p (var_size)
      && (init_type == AUTO_INIT_PATTERN || !is_gimple_reg_type (var_type))
      && mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,tree_to_uhwi (var_size) * BITS_PER_UNIT,0).exists (&var_mode)
      && have_insn_for (SET, var_mode)){
         unsigned HOST_WIDE_INT total_bytes = tree_to_uhwi (var_size);
         unsigned char *buf = XALLOCAVEC (unsigned char, total_bytes);
         memset (buf, (init_type == AUTO_INIT_PATTERN ? INIT_PATTERN_VALUE : 0), total_bytes);
         tree itype = mtcs_tree_build_nonstandard_integer_type/*!build_nonstandard_integer_type*/(mtcsTree,total_bytes * BITS_PER_UNIT, 1);
         wide_int w = wi::from_buffer (buf, total_bytes);
         init = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,itype, w);
         /* Pun the LHS to make sure its type has constant size
         unless it is an SSA name where that's already known.  */
         if (TREE_CODE (lhs) != SSA_NAME)
            lhs = build1 (VIEW_CONVERT_EXPR, itype, lhs);
         else
            init = fold_build1 (VIEW_CONVERT_EXPR, TREE_TYPE (lhs), init);
      }else
         /* Use zero-init also for variable-length sizes.  */
         init = build_zero_cst (var_type);

      mtcs_expr_expand_assignment/*!expand_assignment*/(mtcsExpr,lhs, init, false);
   }
}

/* Expand the IFN_ACCESS_WITH_SIZE function:
   ACCESS_WITH_SIZE (REF_TO_OBJ, REF_TO_SIZE, CLASS_OF_SIZE,
           TYPE_OF_SIZE, ACCESS_MODE)
   which returns the REF_TO_OBJ same as the 1st argument;

   1st argument REF_TO_OBJ: The reference to the object;
   2nd argument REF_TO_SIZE: The reference to the size of the object,
   3rd argument CLASS_OF_SIZE: The size referenced by the REF_TO_SIZE represents
     0: the number of bytes.
     1: the number of the elements of the object type;
   4th argument TYPE_OF_SIZE: A constant 0 with its TYPE being the same as the TYPE
    of the object referenced by REF_TO_SIZE
   5th argument ACCESS_MODE:
    -1: Unknown access semantics
     0: none
     1: read_only
     2: write_only
     3: read_write
   6th argument: A constant 0 with the pointer TYPE to the original flexible
     array type.

   Both the return type and the type of the first argument of this
   function have been converted from the incomplete array type to
   the corresponding pointer type.

   For each call to a .ACCESS_WITH_SIZE, replace it with its 1st argument.  */

static void mtcs_expand_ACCESS_WITH_SIZE (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   tree lhs = gimple_call_lhs (stmt);
   tree ref_to_obj = gimple_call_arg (stmt, 0);
   if (lhs)
      mtcs_expr_expand_assignment/*!expand_assignment*/(mtcsExpr,lhs, ref_to_obj, false);
}

/* The size of an OpenACC compute dimension.  */

static void mtcs_expand_GOACC_DIM_SIZE (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   tree lhs = gimple_call_lhs (stmt);

   if (!lhs)
      return;

   rtx target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   if (target_rtx_have_oacc_dim_size/*!targetm.have_oacc_dim_size*/(mtcsMachine->tmrtx)){
      rtx dim = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,gimple_call_arg (stmt, 0), NULL_RTX,VOIDmode, EXPAND_NORMAL);
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,
            target_rtx_gen_oacc_dim_size/*!targetm.gen_oacc_dim_size*/(mtcsMachine->tmrtx,target, dim));
   }else
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,1));
}

/* The position of an OpenACC execution engine along one compute axis.  */
static void mtcs_expand_GOACC_DIM_POS (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   tree lhs = gimple_call_lhs (stmt);

   if (!lhs)
      return;

   rtx target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   if (target_rtx_have_oacc_dim_pos/*!targetm.have_oacc_dim_pos*/(mtcsMachine->tmrtx)){
      rtx dim = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,gimple_call_arg (stmt, 0), NULL_RTX, VOIDmode, EXPAND_NORMAL);
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,
            target_rtx_gen_oacc_dim_pos/*!targetm.gen_oacc_dim_pos*/(mtcsMachine->tmrtx,target, dim));
   }else
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, const0_rtx);
}

/* This is expanded by oacc_device_lower pass.  */
static void mtcs_expand_GOACC_LOOP (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This is expanded by oacc_device_lower pass.  */
static void mtcs_expand_GOACC_REDUCTION (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* This is expanded by oacc_device_lower pass.  */

static void mtcs_expand_GOACC_TILE (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* Set errno to EDOM.  */
static void mtcs_expand_SET_EDOM (MtcsInternalFn *self,internal_fn, gcall *)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

#ifdef TARGET_EDOM
   #ifdef GEN_ERRNO_RTX
      rtx errno_rtx = GEN_ERRNO_RTX;
   #else
      rtx errno_rtx = gen_rtx_MEM (word_mode, gen_rtx_SYMBOL_REF (mtcs_mode_get_Pmode(mtcsMode), "errno"));
   #endif
   mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,errno_rtx,
   mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,TARGET_EDOM, GET_MODE (errno_rtx)));
#else
   gcc_unreachable ();
#endif
}

/* Expand atomic bit test and set.  */
static void mtcs_expand_ATOMIC_BIT_TEST_AND_SET (MtcsInternalFn *self,internal_fn, gcall *call)
{
   expand_ifn_atomic_bit_test_and (call);
}

/* Expand atomic bit test and complement.  */
static void mtcs_expand_ATOMIC_BIT_TEST_AND_COMPLEMENT (MtcsInternalFn *self,internal_fn, gcall *call)
{
   expand_ifn_atomic_bit_test_and (call);
}

/* Expand atomic bit test and reset.  */
static void mtcs_expand_ATOMIC_BIT_TEST_AND_RESET (MtcsInternalFn *self,internal_fn, gcall *call)
{
   expand_ifn_atomic_bit_test_and (call);
}

/* Expand atomic bit test and set.  */
static void mtcs_expand_ATOMIC_COMPARE_EXCHANGE (MtcsInternalFn *self,internal_fn, gcall *call)
{
   expand_ifn_atomic_compare_exchange (call);
}

/* Expand atomic add fetch and cmp with 0.  */

static void mtcs_expand_ATOMIC_ADD_FETCH_CMP_0 (MtcsInternalFn *self,internal_fn, gcall *call)
{
   expand_ifn_atomic_op_fetch_cmp_0 (call);
}

/* Expand atomic sub fetch and cmp with 0.  */

static void mtcs_expand_ATOMIC_SUB_FETCH_CMP_0 (MtcsInternalFn *self,internal_fn, gcall *call)
{
   expand_ifn_atomic_op_fetch_cmp_0 (call);
}

/* Expand atomic and fetch and cmp with 0.  */
static void mtcs_expand_ATOMIC_AND_FETCH_CMP_0 (MtcsInternalFn *self,internal_fn, gcall *call)
{
   expand_ifn_atomic_op_fetch_cmp_0 (call);
}

/* Expand atomic or fetch and cmp with 0.  */
static void mtcs_expand_ATOMIC_OR_FETCH_CMP_0 (MtcsInternalFn *self,internal_fn, gcall *call)
{
   expand_ifn_atomic_op_fetch_cmp_0 (call);
}

/* Expand atomic xor fetch and cmp with 0.  */

static void mtcs_expand_ATOMIC_XOR_FETCH_CMP_0 (MtcsInternalFn *self,internal_fn, gcall *call)
{
   expand_ifn_atomic_op_fetch_cmp_0 (call);
}

/* Expand LAUNDER to assignment, lhs = arg0.  */
static void mtcs_expand_LAUNDER (MtcsInternalFn *self,internal_fn, gcall *call)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   tree lhs = gimple_call_lhs (call);
   if (!lhs)
      return;
   mtcs_expr_expand_assignment/*!expand_assignment*/(mtcsExpr,lhs, gimple_call_arg (call, 0), false);
}

/* Expand {MASK_,}SCATTER_STORE{S,U} call CALL using optab OPTAB.  */
static void expand_scatter_store_optab_fn (MtcsInternalFn *self,internal_fn, gcall *stmt, direct_optab optab)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);

   internal_fn ifn = gimple_call_internal_fn (stmt);
   int rhs_index = internal_fn_stored_value_index (ifn);
   tree base = gimple_call_arg (stmt, 0);
   tree offset = gimple_call_arg (stmt, 1);
   tree scale = gimple_call_arg (stmt, 2);
   tree rhs = gimple_call_arg (stmt, rhs_index);

   rtx base_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,base);
   rtx offset_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,offset);
   HOST_WIDE_INT scale_int = tree_to_shwi (scale);
   rtx rhs_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,rhs);

   class expand_operand ops[8];
   int i = 0;
   create_address_operand (&ops[i++], base_rtx);
   create_input_operand (&ops[i++], offset_rtx, TYPE_MODE (TREE_TYPE (offset)));
   mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[i++], TYPE_UNSIGNED (TREE_TYPE (offset)));
   mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[i++], scale_int);
   create_input_operand (&ops[i++], rhs_rtx, TYPE_MODE (TREE_TYPE (rhs)));
   i = add_mask_else_and_len_args(self,ops, i, stmt);

   insn_code icode = mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,
         optab, TYPE_MODE (TREE_TYPE (rhs)),TYPE_MODE (TREE_TYPE (offset)));
   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,icode, i, ops);
}

/* Expand {MASK_,}GATHER_LOAD call CALL using optab OPTAB.  */
static void expand_gather_load_optab_fn (MtcsInternalFn *self,internal_fn, gcall *stmt, direct_optab optab)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);

   tree lhs = gimple_call_lhs (stmt);
   tree base = gimple_call_arg (stmt, 0);
   tree offset = gimple_call_arg (stmt, 1);
   tree scale = gimple_call_arg (stmt, 2);

   rtx lhs_rtx = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   rtx base_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,base);
   rtx offset_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,offset);
   HOST_WIDE_INT scale_int = tree_to_shwi (scale);

   int i = 0;
   class expand_operand ops[9];
   create_call_lhs_operand (&ops[i++], lhs_rtx, TYPE_MODE (TREE_TYPE (lhs)));
   create_address_operand (&ops[i++], base_rtx);
   create_input_operand (&ops[i++], offset_rtx, TYPE_MODE (TREE_TYPE (offset)));
   mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[i++], TYPE_UNSIGNED (TREE_TYPE (offset)));
   mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[i++], scale_int);
   i = add_mask_else_and_len_args(self,ops, i, stmt);
   insn_code icode = mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,
         optab, TYPE_MODE (TREE_TYPE (lhs)),TYPE_MODE (TREE_TYPE (offset)));
   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,icode, i, ops);
   assign_call_lhs(self,lhs, lhs_rtx, &ops[0]);
}

/* Expand MASK_LEN_STRIDED_LOAD call CALL by optab OPTAB.  */
static void expand_strided_load_optab_fn (MtcsInternalFn *self,ATTRIBUTE_UNUSED internal_fn, gcall *stmt,
               direct_optab optab)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);

   tree lhs = gimple_call_lhs (stmt);
   tree base = gimple_call_arg (stmt, 0);
   tree stride = gimple_call_arg (stmt, 1);

   rtx lhs_rtx = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   rtx base_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,base);
   rtx stride_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,stride);

   unsigned i = 0;
   class expand_operand ops[7];
   machine_mode mode = TYPE_MODE (TREE_TYPE (lhs));

   create_output_operand (&ops[i++], lhs_rtx, mode);
   create_address_operand (&ops[i++], base_rtx);
   create_address_operand (&ops[i++], stride_rtx);

   i = add_mask_else_and_len_args(self,ops, i, stmt);
   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,
         mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,optab, mode), i, ops);

   if (!rtx_equal_p (lhs_rtx, ops[0].value))
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,lhs_rtx, ops[0].value);
}

/* Expand MASK_LEN_STRIDED_STORE call CALL by optab OPTAB.  */
static void expand_strided_store_optab_fn (MtcsInternalFn *self,ATTRIBUTE_UNUSED internal_fn, gcall *stmt,
                direct_optab optab)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);

   internal_fn fn = gimple_call_internal_fn (stmt);
   int rhs_index = internal_fn_stored_value_index (fn);

   tree base = gimple_call_arg (stmt, 0);
   tree stride = gimple_call_arg (stmt, 1);
   tree rhs = gimple_call_arg (stmt, rhs_index);

   rtx base_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,base);
   rtx stride_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,stride);
   rtx rhs_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,rhs);

   unsigned i = 0;
   class expand_operand ops[6];
   machine_mode mode = TYPE_MODE (TREE_TYPE (rhs));

   create_address_operand (&ops[i++], base_rtx);
   create_address_operand (&ops[i++], stride_rtx);
   create_input_operand (&ops[i++], rhs_rtx, mode);

   i = add_mask_else_and_len_args(self,ops, i, stmt);
   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,
         mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,optab, mode), i, ops);
}

/* Helper for expand_DIVMOD.  Return true if the sequence starting with
   INSN contains any call insns or insns with {,U}{DIV,MOD} rtxes.  */

static bool contains_call_div_mod (rtx_insn *insn)
{
   subrtx_iterator::array_type array;
   for (; insn; insn = NEXT_INSN (insn))
      if (CALL_P (insn))
         return true;
      else if (INSN_P (insn))
         FOR_EACH_SUBRTX (iter, array, PATTERN (insn), NONCONST)
            switch (GET_CODE (*iter)){
               case CALL:
               case DIV:
               case UDIV:
               case MOD:
               case UMOD:
                  return true;
               default:
                  break;
            }
   return false;
 }

/* Expand DIVMOD() using:
 a) optab handler for udivmod/sdivmod if it is available.
 b) If optab_handler doesn't exist, generate call to
    target-specific divmod libfunc.  */
static void mtcs_expand_DIVMOD (MtcsInternalFn *self,internal_fn, gcall *call_stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsPredict   *mtcsPredict =mtcs_target_get_predict(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
   MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);

   tree lhs = gimple_call_lhs (call_stmt);
   tree arg0 = gimple_call_arg (call_stmt, 0);
   tree arg1 = gimple_call_arg (call_stmt, 1);

   gcc_assert (TREE_CODE (TREE_TYPE (lhs)) == COMPLEX_TYPE);
   tree type = TREE_TYPE (TREE_TYPE (lhs));
   machine_mode mode = TYPE_MODE (type);
   bool unsignedp = TYPE_UNSIGNED (type);
   optab tab = (unsignedp) ? udivmod_optab : sdivmod_optab;

   rtx op0 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg0);
   rtx op1 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg1);

   n_debug("mtcsinternalfn.c expand_DIVMOD xx remainder tab:%d mode:%d unsignedp:%d\n",tab,mode,unsignedp);
   mtcs_print_rtl(stderr,op1);

   rtx target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);

   rtx quotient = NULL_RTX, remainder = NULL_RTX;
   rtx_insn *insns = NULL;

   if (TREE_CODE (arg1) == INTEGER_CST){
      /* For DIVMOD by integral constants, there could be efficient code
      expanded inline e.g. using shifts and plus/minus.  Try to expand
      the division and modulo and if it emits any library calls or any
      {,U}{DIV,MOD} rtxes throw it away and use a divmod optab or
      divmod libcall.  */
      scalar_int_mode int_mode;
      if (remainder == NULL_RTX
      && optimize
      && CONST_INT_P (op1)
      && !pow2p_hwi (INTVAL (op1))
      && mtcs_mode_is_int_mode/*!is_int_mode*/(mtcsMode,TYPE_MODE (type), &int_mode)
      && mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,int_mode) == 2 * UNITS_PER_WORD
      && mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,and_optab, word_mode) != CODE_FOR_nothing
      && mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,add_optab, word_mode) != CODE_FOR_nothing
      && mtcs_predict_optimize_insn_for_speed_p/*!optimize_insn_for_speed_p*/(mtcsPredict)){
         rtx_insn *last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
         remainder = NULL_RTX;
         quotient = mtcs_optabs_expand_doubleword_divmod/*!expand_doubleword_divmod*/(mtcsOptabs,
               int_mode, op0, op1, &remainder,TYPE_UNSIGNED (type));
         if (quotient != NULL_RTX){
            if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,mov_optab, int_mode) != CODE_FOR_nothing){
               rtx_insn *move = mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,quotient, quotient);
               mtcs_rtl_set_dst_reg_note/*!set_dst_reg_note*/(mtcsRTL,
                     move, REG_EQUAL, gen_rtx_fmt_ee (TYPE_UNSIGNED (type)? UDIV : DIV, int_mode, copy_rtx (op0), op1),quotient);
               move = mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,remainder, remainder);
               mtcs_rtl_set_dst_reg_note/*!set_dst_reg_note*/(mtcsRTL,
                     move, REG_EQUAL, gen_rtx_fmt_ee (TYPE_UNSIGNED (type)? UMOD : MOD, int_mode, copy_rtx (op0), op1), quotient);
            }
         }else
            mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
      }

      if (remainder == NULL_RTX){
         struct separate_ops ops;
         ops.code = TRUNC_DIV_EXPR;
         ops.type = type;
         ops.op0 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,ops.type, op0);
         ops.op1 = arg1;
         ops.op2 = NULL_TREE;
         ops.location = gimple_location (call_stmt);
         mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
         quotient = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(mtcsExpr,&ops, NULL_RTX, mode, EXPAND_NORMAL);
         if (contains_call_div_mod (mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData)))
            quotient = NULL_RTX;
         else{
            ops.code = TRUNC_MOD_EXPR;
            remainder = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(mtcsExpr,&ops, NULL_RTX, mode,EXPAND_NORMAL);
            if (contains_call_div_mod (mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData)))
               remainder = NULL_RTX;
         }
         if (remainder)
            insns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
         mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      }
   }

   if (remainder){
      n_debug("mtcsinternalfn.c expand_DIVMOD 00 remainder tab:%d mode:%d\n",tab,mode);
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,insns);
   /* Check if optab_handler exists for divmod_optab for given mode.  */
   }else if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,tab, mode) != CODE_FOR_nothing){
      n_debug("mtcsinternalfn.c expand_DIVMOD 11  optab_handler (tab, mode) != CODE_FOR_nothing tab:%d mode:%d insncode:%d\n",
                tab,mode,optab_handler (tab, mode));
      quotient = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
      remainder = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
      mtcs_optabs_expand_twoval_binop/*!expand_twoval_binop*/(mtcsOptabs,tab, op0, op1, quotient, remainder, unsignedp);
   }
   /* Generate call to divmod libfunc if it exists.  */
   else if (rtx libfunc = mtcs_libfuncs_optab_libfunc/*!optab_libfunc*/(mtcsLibfuncs,tab, mode)){
      n_debug("mtcsinternalfn.c expand_DIVMOD 22 rtx libfunc = optab_libfunc (tab, mode) tab:%d mode:%d FIRST_NORM_OPTAB:%d\n",
            tab,mode,FIRST_NORM_OPTAB);
      mtcsTarget/*!targetm.expand_divmod_libfunc*/->expand_divmod_libfunc(mtcsTarget,
            libfunc, mode, op0, op1, &quotient, &remainder);
   }else
      gcc_unreachable ();

   n_debug("mtcsinternalfn.c expand_DIVMOD 33 remainder tab:%d mode:%d\n",tab,mode);

   /* Wrap the return value (quotient, remainder) within COMPLEX_EXPR.  */
   mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,build2 (COMPLEX_EXPR, TREE_TYPE (lhs),
                                       mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,TREE_TYPE (arg0), quotient),
                                       mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,TREE_TYPE (arg1), remainder)),
                                       target, VOIDmode, EXPAND_NORMAL);
}

/* Expand a NOP.  */
static void mtcs_expand_NOP (MtcsInternalFn *self,internal_fn, gcall *)
{
  /* Nothing.  But it shouldn't really prevail.  */
}

/* Coroutines, all should have been processed at this stage.  */
static void mtcs_expand_CO_FRAME (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

static void mtcs_expand_CO_YIELD (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

static void mtcs_expand_CO_SUSPN (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

static void mtcs_expand_CO_ACTOR (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

/* Expand a call to FN using the operands in STMT.  FN has a single
   output operand and NARGS input operands.  */
static void expand_direct_optab_fn (MtcsInternalFn *self,internal_fn fn, gcall *stmt, direct_optab optab,
         unsigned int nargs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);

   tree_pair types = direct_internal_fn_types (fn, stmt);
   insn_code icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,optab, TYPE_MODE (types.first));

   n_debug("mtcsinternalfn-fn.cc expand_direct_optab_fn --fn:%d %s icode:%d optab:%d nargs:%d mode:%d\n",
         fn,internal_fn_name_array[fn],icode,optab,nargs,TYPE_MODE (types.first));
   expand_fn_using_insn(self,stmt, icode, 1, nargs);
}

/* Expand WHILE_ULT call STMT using optab OPTAB.  */
static void expand_while_optab_fn (MtcsInternalFn *self,internal_fn, gcall *stmt, convert_optab optab)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);

   expand_operand ops[4];
   tree rhs_type[2];

   tree lhs = gimple_call_lhs (stmt);
   tree lhs_type = TREE_TYPE (lhs);
   rtx lhs_rtx = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   create_call_lhs_operand (&ops[0], lhs_rtx, TYPE_MODE (lhs_type));

   for (unsigned int i = 0; i < 2; ++i){
      tree rhs = gimple_call_arg (stmt, i);
      rhs_type[i] = TREE_TYPE (rhs);
      rtx rhs_rtx = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,rhs);
      create_input_operand (&ops[i + 1], rhs_rtx, TYPE_MODE (rhs_type[i]));
   }

   int opcnt;
   if (!mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,TYPE_MODE (lhs_type))){
      /* When the mask is an integer mode the exact vector length may not
      be clear to the backend, so we pass it in operand[3].
      Use the vector in arg2 for the most reliable intended size.  */
      tree type = TREE_TYPE (gimple_call_arg (stmt, 2));
      mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[3], TYPE_VECTOR_SUBPARTS (type));
      opcnt = 4;
   }else
      /* The mask has a vector type so the length operand is unnecessary.  */
      opcnt = 3;

   insn_code icode = mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,
         optab, TYPE_MODE (rhs_type[0]),TYPE_MODE (lhs_type));

   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,icode, opcnt, ops);
   assign_call_lhs(self,lhs, lhs_rtx, &ops[0]);
}

/* Expand a call to a convert-like optab using the operands in STMT.
   FN has a single output operand and NARGS input operands.  */

static void expand_convert_optab_fn (MtcsInternalFn *self,internal_fn fn, gcall *stmt, convert_optab optab,
          unsigned int nargs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);

   tree_pair types = direct_internal_fn_types (fn, stmt);
   insn_code icode = mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,
         optab, TYPE_MODE (types.first),TYPE_MODE (types.second));
   expand_fn_using_insn(self,stmt, icode, 1, nargs);
}

static void generateReflectingCode_cb(rtx *op,void *userData)
{
   MtcsInternalFn *self = (MtcsInternalFn *)userData;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   mtcs_expr_generate_reflecting_code_standard(mtcsExpr,op);
}
/* Expand CRC call STMT.  */
static void expand_crc_optab_fn (MtcsInternalFn *self,internal_fn fn, gcall *stmt, convert_optab optab)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   tree lhs = gimple_call_lhs (stmt);
   tree rhs1 = gimple_call_arg (stmt, 0); // crc
   tree rhs2 = gimple_call_arg (stmt, 1); // data
   tree rhs3 = gimple_call_arg (stmt, 2); // polynomial

   tree result_type = TREE_TYPE (lhs);
   tree data_type = TREE_TYPE (rhs2);

   gcc_assert (TYPE_MODE (result_type) >= TYPE_MODE (data_type));

   rtx dest = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   rtx crc = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,rhs1);
   rtx data = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,rhs2);

   rtx polynomial;
   if (TREE_CODE (rhs3) != INTEGER_CST){
      error ("third argument to %<crc%> builtins must be a constant");
      polynomial = const0_rtx;
   }else
      polynomial = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,TYPE_MODE (result_type),
            mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,rhs3), 0);



   /* Use target specific expansion if it exists.
   Otherwise, generate table-based CRC.  */
   if (mtcs_internal_fn_direct_internal_fn_supported_p/*!direct_internal_fn_supported_p*/(self,
         fn, tree_pair (data_type, result_type),OPTIMIZE_FOR_SPEED)){
      class expand_operand ops[4];

      if (dump_file && (dump_flags & TDF_DETAILS)){
         fprintf (dump_file, ";; using optab for crc_%u_polynomial_"HOST_WIDE_INT_PRINT_HEX "\n",
               mtcs_mode_get_bitsize_poly/*!GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (dest)).to_constant (),TREE_INT_CST_LOW (rhs3));
      }

      create_call_lhs_operand (&ops[0], dest, TYPE_MODE (result_type));
      create_input_operand (&ops[1], crc, TYPE_MODE (result_type));
      create_input_operand (&ops[2], data, TYPE_MODE (data_type));
      create_input_operand (&ops[3], polynomial, TYPE_MODE (result_type));
      insn_code icode = mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,
            optab, TYPE_MODE (data_type),TYPE_MODE (result_type));
      mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,icode, 4, ops);
      assign_call_lhs(self,lhs, dest, &ops[0]);
   }else{
      /* We're bypassing all the operand conversions that are done in the
      case when we get an icode, operands and pass that off to expand_insn.

      That path has special case handling for promoted return values which
      we must emulate here (is the same kind of special treatment ever
      needed for input arguments here?).

      In particular we do not want to store directly into a promoted
      SUBREG destination, instead store into a suitably sized pseudo.  */
      rtx orig_dest = dest;
      if (SUBREG_P (dest) && SUBREG_PROMOTED_VAR_P (dest))
         dest = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (dest));

      /* If it's IFN_CRC generate bit-forward CRC.  */
      if (fn == IFN_CRC)
         mtcs_expr_expand_crc_table_based/*!expand_crc_table_based*/(mtcsExpr,dest, crc, data, polynomial,TYPE_MODE (data_type));
      else
         /* If it's IFN_CRC_REV generate bit-reversed CRC.  */
         mtcs_expr_expand_reversed_crc_table_based/*!expand_reversed_crc_table_based*/(mtcsExpr,
               dest, crc, data, polynomial, TYPE_MODE (data_type),generate_reflecting_code_standard,(void *)self);

      /* Now get the return value where it needs to be, taking care to
      ensure it's promoted appropriately if the ABI demands it.

      Re-use assign_call_lhs to handle the details.  */
      class expand_operand ops[4];
      create_call_lhs_operand (&ops[0], dest, TYPE_MODE (result_type));
      ops[0].value = dest;
      assign_call_lhs(self,lhs, orig_dest, &ops[0]);
   }
}

/* Expanders for optabs that can use expand_direct_optab_fn.  */

#define expand_unary_optab_fn(SELF,FN, STMT, OPTAB) \
  expand_direct_optab_fn (SELF,FN, STMT, OPTAB, 1)

#define expand_binary_optab_fn(SELF,FN, STMT, OPTAB) \
  expand_direct_optab_fn (SELF,FN, STMT, OPTAB, 2)

#define expand_ternary_optab_fn(SELF,FN, STMT, OPTAB) \
  expand_direct_optab_fn (SELF,FN, STMT, OPTAB, 3)

#define expand_cond_unary_optab_fn(SELF,FN, STMT, OPTAB) \
  expand_direct_optab_fn (SELF,FN, STMT, OPTAB, 3)

#define expand_cond_binary_optab_fn(SELF,FN, STMT, OPTAB) \
  expand_direct_optab_fn (SELF,FN, STMT, OPTAB, 4)

#define expand_cond_ternary_optab_fn(SELF,FN, STMT, OPTAB) \
  expand_direct_optab_fn (SELF,FN, STMT, OPTAB, 5)

#define expand_cond_len_unary_optab_fn(SELF,FN, STMT, OPTAB) \
  expand_direct_optab_fn (SELF,FN, STMT, OPTAB, 5)

#define expand_cond_len_binary_optab_fn(SELF,FN, STMT, OPTAB) \
  expand_direct_optab_fn (SELF,FN, STMT, OPTAB, 6)

#define expand_cond_len_ternary_optab_fn(SELF,FN, STMT, OPTAB) \
  expand_direct_optab_fn (SELF,FN, STMT, OPTAB, 7)

#define expand_fold_extract_optab_fn(SELF,FN, STMT, OPTAB) \
  expand_direct_optab_fn (SELF,FN, STMT, OPTAB, 3)

#define expand_fold_len_extract_optab_fn(SELF,FN, STMT, OPTAB) \
  expand_direct_optab_fn (SELF,FN, STMT, OPTAB, 5)

#define expand_fold_left_optab_fn(SELF,FN, STMT, OPTAB) \
  expand_direct_optab_fn (SELF,FN, STMT, OPTAB, 2)

#define expand_mask_fold_left_optab_fn(SELF,FN, STMT, OPTAB) \
  expand_direct_optab_fn (SELF,FN, STMT, OPTAB, 3)

#define expand_mask_len_fold_left_optab_fn(SELF,FN, STMT, OPTAB) \
  expand_direct_optab_fn (SELF,FN, STMT, OPTAB, 5)

#define expand_check_ptrs_optab_fn(SELF,FN, STMT, OPTAB) \
  expand_direct_optab_fn (SELF,FN, STMT, OPTAB, 4)

/* Expanders for optabs that can use expand_convert_optab_fn.  */

#define expand_unary_convert_optab_fn(SELF,FN, STMT, OPTAB) \
  expand_convert_optab_fn (SELF,FN, STMT, OPTAB, 1)

#define expand_vec_extract_optab_fn(SELF,FN, STMT, OPTAB) \
  expand_convert_optab_fn (SELF,FN, STMT, OPTAB, 2)

/* Return true if OPTAB is supported for TYPES (whose modes should be
   the same) when the optimization type is OPT_TYPE.  Used for simple
   direct optabs.  */
static bool direct_optab_supported_p (MtcsInternalFn *self, direct_optab optab, tree_pair types,
           optimization_type opt_type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);

   machine_mode mode = TYPE_MODE (types.first);
   gcc_checking_assert (mode == TYPE_MODE (types.second));
   n_debug("mtcsinternalfn.c direct_optab_supported_p 00 optab:%d mode:%d mtcsTarget:%p\n",optab,mode,mtcsTarget);
   return mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,optab, mode, opt_type) != CODE_FOR_nothing;
}

/* Return true if OPTAB is supported for TYPES, where the first type
   is the destination and the second type is the source.  Used for
   convert optabs.  */

static bool convert_optab_supported_p (MtcsInternalFn *self,convert_optab optab, tree_pair types,
            optimization_type opt_type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);

   return (mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,
         optab, TYPE_MODE (types.first),TYPE_MODE (types.second), opt_type) != CODE_FOR_nothing);
}

/* Return true if load/store lanes optab OPTAB is supported for
   array type TYPES.first when the optimization type is OPT_TYPE.  */

static bool multi_vector_optab_supported_p (MtcsInternalFn *self, convert_optab optab, tree_pair types,
            optimization_type opt_type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);

   gcc_assert (TREE_CODE (types.first) == ARRAY_TYPE);
   machine_mode imode = TYPE_MODE (types.first);
   machine_mode vmode = TYPE_MODE (TREE_TYPE (types.first));
   return (mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,optab, imode, vmode, opt_type) != CODE_FOR_nothing);
}

#define direct_unary_optab_supported_p direct_optab_supported_p
#define direct_unary_convert_optab_supported_p convert_optab_supported_p
#define direct_binary_optab_supported_p direct_optab_supported_p
#define direct_ternary_optab_supported_p direct_optab_supported_p
#define direct_cond_unary_optab_supported_p direct_optab_supported_p
#define direct_cond_binary_optab_supported_p direct_optab_supported_p
#define direct_cond_ternary_optab_supported_p direct_optab_supported_p
#define direct_cond_len_unary_optab_supported_p direct_optab_supported_p
#define direct_cond_len_binary_optab_supported_p direct_optab_supported_p
#define direct_cond_len_ternary_optab_supported_p direct_optab_supported_p
#define direct_crc_optab_supported_p convert_optab_supported_p
#define direct_mask_load_optab_supported_p convert_optab_supported_p
#define direct_load_lanes_optab_supported_p multi_vector_optab_supported_p
#define direct_mask_load_lanes_optab_supported_p multi_vector_optab_supported_p
#define direct_gather_load_optab_supported_p convert_optab_supported_p
#define direct_strided_load_optab_supported_p direct_optab_supported_p
#define direct_len_load_optab_supported_p direct_optab_supported_p
#define direct_mask_len_load_optab_supported_p convert_optab_supported_p
#define direct_mask_store_optab_supported_p convert_optab_supported_p
#define direct_store_lanes_optab_supported_p multi_vector_optab_supported_p
#define direct_mask_store_lanes_optab_supported_p multi_vector_optab_supported_p
#define direct_vec_cond_mask_optab_supported_p convert_optab_supported_p
#define direct_vec_cond_optab_supported_p convert_optab_supported_p
#define direct_scatter_store_optab_supported_p convert_optab_supported_p
#define direct_strided_store_optab_supported_p direct_optab_supported_p
#define direct_len_store_optab_supported_p direct_optab_supported_p
#define direct_mask_len_store_optab_supported_p convert_optab_supported_p
#define direct_while_optab_supported_p convert_optab_supported_p
#define direct_fold_extract_optab_supported_p direct_optab_supported_p
#define direct_fold_len_extract_optab_supported_p direct_optab_supported_p
#define direct_fold_left_optab_supported_p direct_optab_supported_p
#define direct_mask_fold_left_optab_supported_p direct_optab_supported_p
#define direct_mask_len_fold_left_optab_supported_p direct_optab_supported_p
#define direct_check_ptrs_optab_supported_p direct_optab_supported_p
#define direct_vec_set_optab_supported_p direct_optab_supported_p
#define direct_vec_extract_optab_supported_p convert_optab_supported_p

/* Return the optab used by internal function FN.  */
static optab direct_internal_fn_optab (internal_fn fn)
{
   switch (fn){
#define DEF_INTERNAL_FN(CODE, FLAGS, FNSPEC) \
      case IFN_##CODE: break;
#define DEF_INTERNAL_OPTAB_FN(CODE, FLAGS, OPTAB, TYPE) \
      case IFN_##CODE: return OPTAB##_optab;
      #include "internal-fn.def"

      case IFN_LAST:
         break;
   }
   gcc_unreachable ();
}

/* Return true if TYPE's mode has the same format as TYPE, and if there is
   a 1:1 correspondence between the values that the mode can store and the
   values that the type can store.  */
static bool type_strictly_matches_mode_p (MtcsInternalFn *self,const_tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsTree   *mtcsTree =mtcs_target_get_tree(mtcsTarget);

   /* The masked vector operations have both vector data operands and vector
   boolean operands.  The vector data operands are expected to have a vector
   mode,  but the vector boolean operands can be an integer mode rather than
   a vector mode,  depending on how TARGET_VECTORIZE_GET_MASK_MODE is
   defined.  PR116103.  */
   if (VECTOR_BOOLEAN_TYPE_P (type)
   && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,TYPE_MODE (type))
   && TYPE_PRECISION (TREE_TYPE (type)) == 1)
      return true;

   if (VECTOR_TYPE_P (type))
      return mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,TYPE_MODE (type));

   if (INTEGRAL_TYPE_P (type))
      return mtcs_tree_type_has_mode_precision_p/*!type_has_mode_precision_p*/(mtcsTree,type);

   if (SCALAR_FLOAT_TYPE_P (type) || COMPLEX_FLOAT_TYPE_P (type))
      return true;

   return false;
}

/* Returns true if both types of TYPE_PAIR strictly match their modes,
   else returns false.  */
static bool type_pair_strictly_matches_mode_p (MtcsInternalFn *self,tree_pair type_pair)
{
  return type_strictly_matches_mode_p(self,type_pair.first)
    && type_strictly_matches_mode_p(self,type_pair.second);
}

/* Return true if FN is supported for the types in TYPES when the
   optimization type is OPT_TYPE.  The types are those associated with
   the "type0" and "type1" fields of FN's direct_internal_fn_info
   structure.  */
//原型 direct_internal_fn_supported_p internal-fn.h internal-fn.cc
bool mtcs_internal_fn_direct_internal_fn_supported_p (MtcsInternalFn *self,internal_fn fn, tree_pair types,
            optimization_type opt_type)
{
   n_debug("mtcs_internal_fn_direct_internal_fn_supported_p 00 fn:%d\n",fn);
   if (!type_pair_strictly_matches_mode_p(self,types))
      return false;
   n_debug("mtcs_internal_fn_direct_internal_fn_supported_p 11 fn:%d\n",fn);

   switch (fn){
#define DEF_INTERNAL_FN(CODE, FLAGS, FNSPEC) \
      case IFN_##CODE: break;
#define DEF_INTERNAL_OPTAB_FN(CODE, FLAGS, OPTAB, TYPE) \
      case IFN_##CODE: \
      return direct_##TYPE##_optab_supported_p (self,OPTAB##_optab, types, \
      opt_type);
#define DEF_INTERNAL_SIGNED_OPTAB_FN(CODE, FLAGS, SELECTOR, SIGNED_OPTAB, \
      UNSIGNED_OPTAB, TYPE)    \
      case IFN_##CODE:                   \
      {                          \
      optab which_optab = (TYPE_UNSIGNED (types.SELECTOR)      \
      ? UNSIGNED_OPTAB ## _optab        \
      : SIGNED_OPTAB ## _optab);        \
      return direct_##TYPE##_optab_supported_p(self,which_optab, types,  \
      opt_type);      \
      }
      #include "internal-fn.def"

      case IFN_LAST:
         break;
   }
   gcc_unreachable ();
}

/* Return true if FN is supported for type TYPE when the optimization
   type is OPT_TYPE.  The caller knows that the "type0" and "type1"
   fields of FN's direct_internal_fn_info structure are the same.  */
//原型 direct_internal_fn_supported_p internal-fn.h internal-fn.cc
bool mtcs_internal_fn_direct_internal_fn_supported_p (MtcsInternalFn *self,internal_fn fn, tree type,
            optimization_type opt_type)
{
   const direct_internal_fn_info &info = direct_internal_fn (fn); //用上面的static direct_internal_fn 方法
   gcc_checking_assert (info.type0 == info.type1);
   return mtcs_internal_fn_direct_internal_fn_supported_p/*!direct_internal_fn_supported_p*/(self,
         fn, tree_pair (type, type), opt_type);
}

/* Return true if the STMT is supported when the optimization type is OPT_TYPE,
   given that STMT is a call to a direct internal function.  */
//原型 direct_internal_fn_supported_p internal-fn.h internal-fn.cc

bool mtcs_internal_fn_direct_internal_fn_supported_p (MtcsInternalFn *self,gcall *stmt, optimization_type opt_type)
{
   internal_fn fn = gimple_call_internal_fn (stmt);
   tree_pair types = direct_internal_fn_types (fn, stmt);//用主机的
   return mtcs_internal_fn_direct_internal_fn_supported_p/*!direct_internal_fn_supported_p*/(self,fn, types, opt_type);
}

/* Return true if IFN_SET_EDOM is supported.  */
//保留internal-fn.cc中的实现，不改动。
//bool set_edom_supported_p (void)
//{
//#ifdef TARGET_EDOM
//  return true;
//#else
//  return false;
//#endif
//}

/*
* 生成expand_XXX的地方
* 例如:internal-fn.def有声明如下:
* DEF_INTERNAL_COND_FN (FMS, ECF_CONST, fms, ternary)
* 生成的函数定义是这样
* static void mtcs_expand_FMS (MtcsInternalFn *self,internal_fn fn, gcall *stmt)
* {
*   expand_ternary_optab_fn (self,fn, stmt, fms_optab);
* }
*/
#define DEF_INTERNAL_OPTAB_FN(CODE, FLAGS, OPTAB, TYPE) \
  static void                 \
  mtcs_expand_##CODE (MtcsInternalFn *self,internal_fn fn, gcall *stmt)      \
  {                     \
    expand_##TYPE##_optab_fn (self,fn, stmt, OPTAB##_optab);  \
  }
#define DEF_INTERNAL_INT_EXT_FN(CODE, FLAGS, OPTAB, TYPE)
#define DEF_INTERNAL_SIGNED_OPTAB_FN(CODE, FLAGS, SELECTOR, SIGNED_OPTAB, \
                 UNSIGNED_OPTAB, TYPE)    \
  static void                       \
  mtcs_expand_##CODE (MtcsInternalFn *self,internal_fn fn, gcall *stmt)            \
  {                           \
    tree_pair types = direct_internal_fn_types (fn, stmt);     \
    optab which_optab = direct_internal_fn_optab (fn, types);     \
    expand_##TYPE##_optab_fn (self,fn, stmt, which_optab);       \
  }
#include "internal-fn.def"


/* Store all supported else values for the optab referred to by ICODE
   in ELSE_VALS.  The index of the else operand must be specified in
   ELSE_INDEX.  */
//原型 get_supported_else_vals internal-fn.h internal-fn.cc
void mtcs_internal_fn_get_supported_else_vals (MtcsInternalFn *self,enum insn_code icode, unsigned else_index,vec<int> &else_vals)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

   const struct insn_data_d *data = &mtcsOutput->insn_data[icode];
   if ((char)else_index >= data->n_operands)
      return;

   machine_mode else_mode = data->operand[else_index].mode;

   else_vals.truncate (0);

   /* For now we only support else values of 0, -1, and "undefined".  */
   if (mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(mtcsOptabs,icode, else_index, CONST0_RTX (else_mode)))
      else_vals.safe_push (MASK_LOAD_ELSE_ZERO);

   if (mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(mtcsOptabs,icode, else_index, gen_rtx_SCRATCH (else_mode)))
      else_vals.safe_push (MASK_LOAD_ELSE_UNDEFINED);

   if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,else_mode) == MODE_VECTOR_INT
   && mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(mtcsOptabs,icode, else_index, CONSTM1_RTX (else_mode)))
      else_vals.safe_push (MASK_LOAD_ELSE_M1);
}

/* Return true if the else value ELSE_VAL (one of MASK_LOAD_ELSE_ZERO,
   MASK_LOAD_ELSE_M1, and MASK_LOAD_ELSE_UNDEFINED) is valid fo the optab
   referred to by ICODE.  The index of the else operand must be specified
   in ELSE_INDEX.  */
//原型 supported_else_val_p internal-fn.h internal-fn.cc
bool mtcs_internal_fn_supported_else_val_p (MtcsInternalFn *self,enum insn_code icode, unsigned else_index, int else_val)
{
   if (else_val != MASK_LOAD_ELSE_ZERO && else_val != MASK_LOAD_ELSE_M1  && else_val != MASK_LOAD_ELSE_UNDEFINED)
      gcc_unreachable ();

   auto_vec<int> else_vals;
   mtcs_internal_fn_get_supported_else_vals/*!get_supported_else_vals*/(self,icode, else_index, else_vals);
   return else_vals.contains (else_val);
}

/* Return true if the target supports gather load or scatter store function
   IFN.  For loads, VECTOR_TYPE is the vector type of the load result,
   while for stores it is the vector type of the stored data argument.
   MEMORY_ELEMENT_TYPE is the type of the memory elements being loaded
   or stored.  OFFSET_VECTOR_TYPE is the vector type that holds the
   offset from the shared base address of each loaded or stored element.
   SCALE is the amount by which these offsets should be multiplied
   *after* they have been extended to address width.
   If the target supports the gather load the supported else values
   will be added to the vector ELSVAL points to if it is nonzero.  */
//原型 internal_gather_scatter_fn_supported_p internal-fn.h internal-fn.cc
bool mtcs_internal_fn_internal_gather_scatter_fn_supported_p (MtcsInternalFn *self,internal_fn ifn, tree vector_type,
               tree memory_element_type, tree offset_vector_type, int scale, vec<int> *elsvals)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   if (!tree_int_cst_equal (TYPE_SIZE (TREE_TYPE (vector_type)),TYPE_SIZE (memory_element_type)))
      return false;
   if (maybe_ne (TYPE_VECTOR_SUBPARTS (vector_type), TYPE_VECTOR_SUBPARTS (offset_vector_type)))
      return false;
   optab optab = direct_internal_fn_optab (ifn);//用mtcs的
   insn_code icode = mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,
         optab, TYPE_MODE (vector_type),TYPE_MODE (offset_vector_type));
   int output_ops = internal_load_fn_p (ifn) ? 1 : 0;
   bool unsigned_p = TYPE_UNSIGNED (TREE_TYPE (offset_vector_type));
   bool ok = icode != CODE_FOR_nothing
   && mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(mtcsOptabs,
         icode, 2 + output_ops, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,unsigned_p))
   && mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(mtcsOptabs,
         icode, 3 + output_ops, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,scale));

   /* For gather the optab's operand indices do not match the IFN's because
   the latter does not have the extension operand (operand 3).  It is
   implicitly added during expansion so we use the IFN's else index + 1.
   */
   if (ok && elsvals)
      mtcs_internal_fn_get_supported_else_vals/*!get_supported_else_vals*/(self,icode, internal_fn_else_index (IFN_MASK_GATHER_LOAD) + 1, *elsvals);

   return ok;
}

/* Return true if the target supports IFN_CHECK_{RAW,WAR}_PTRS function IFN
   for pointers of type TYPE when the accesses have LENGTH bytes and their
   common byte alignment is ALIGN.  */
//原型 internal_check_ptrs_fn_supported_p internal-fn.h internal-fn.cc
bool mtcs_internal_fn_internal_check_ptrs_fn_supported_p (MtcsInternalFn *self,internal_fn ifn, tree type,
                poly_uint64 length, unsigned int align)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   machine_mode mode = TYPE_MODE (type);
   optab optab = direct_internal_fn_optab (ifn);
   insn_code icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,optab, mode);
   if (icode == CODE_FOR_nothing)
      return false;
   rtx length_rtx = mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,length, mode);
   return (mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(mtcsOptabs,icode, 3, length_rtx)
   && mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(mtcsOptabs,icode, 4, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,align)));
}

/* Return the supported bias for IFN which is either IFN_{LEN_,MASK_LEN_,}LOAD
   or IFN_{LEN_,MASK_LEN_,}STORE.  For now we only support the biases of 0 and
   -1 (in case 0 is not an allowable length for {len_,mask_len_}load or
   {len_,mask_len_}store). If none of the biases match what the backend
   provides, return VECT_PARTIAL_BIAS_UNSUPPORTED.  */
//原型 internal_len_load_store_bias internal-fn.h internal-fn.cc
signed char mtcs_internal_fn_internal_len_load_store_bias (MtcsInternalFn *self,internal_fn ifn, machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsMachine  *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   optab optab = direct_internal_fn_optab (ifn);
   insn_code icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,optab, mode);
   int bias_no = 3;

   if (icode == CODE_FOR_nothing){
      machine_mode mask_mode;
      if (!target_vectorize_get_mask_mode/*!targetm.vectorize.get_mask_mode*/(mtcsMachine->vectorize,mode).exists (&mask_mode))
         return VECT_PARTIAL_BIAS_UNSUPPORTED;
      if (ifn == IFN_LEN_LOAD){
         /* Try MASK_LEN_LOAD.  */
         optab = direct_internal_fn_optab (IFN_MASK_LEN_LOAD);
      }else{
         /* Try MASK_LEN_STORE.  */
         optab = direct_internal_fn_optab (IFN_MASK_LEN_STORE);
      }
      icode = mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,optab, mode, mask_mode);
      bias_no = 4;
   }

   if (icode != CODE_FOR_nothing){
      /* For now we only support biases of 0 or -1.  Try both of them.  */
      if (mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(mtcsOptabs,
            icode, bias_no, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,0)))
         return 0;
      if (mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(mtcsOptabs,
            icode, bias_no, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,-1)))
         return -1;
   }

   return VECT_PARTIAL_BIAS_UNSUPPORTED;
}

/* If TYPE is a vector type, return true if IFN is a direct internal
   function that is supported for that type.  If TYPE is a scalar type,
   return true if IFN is a direct internal function that is supported for
   the target's preferred vector version of TYPE.  */
//原型 vectorized_internal_fn_supported_p internal-fn.h internal-fn.cc
bool mtcs_internal_fn_vectorized_internal_fn_supported_p (MtcsInternalFn *self,internal_fn ifn, tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsTree   *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsMachine   *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   if (mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,TYPE_MODE (type)))
      return mtcs_internal_fn_direct_internal_fn_supported_p/*!direct_internal_fn_supported_p*/(self,ifn, type, OPTIMIZE_FOR_SPEED);

   scalar_mode smode;
   if (VECTOR_TYPE_P (type) || !mtcs_mode_is_a <scalar_mode>(mtcsMode,TYPE_MODE (type), &smode))
      return false;

   machine_mode vmode = target_vectorize_preferred_simd_mode/*!targetm.vectorize.preferred_simd_mode*/(mtcsMachine->vectorize,smode);
   if (mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,vmode)){
      tree vectype = mtcs_tree_build_vector_type_for_mode/*!build_vector_type_for_mode*/(mtcsTree,type, vmode);
      if (mtcs_internal_fn_direct_internal_fn_supported_p/*!direct_internal_fn_supported_p*/(self,ifn, vectype, OPTIMIZE_FOR_SPEED))
         return true;
   }

   auto_vector_modes vector_modes;
   target_vectorize_autovectorize_vector_modes/*!targetm.vectorize.autovectorize_vector_modes*/(mtcsMachine->vectorize,&vector_modes, true);
   for (machine_mode base_mode : vector_modes)
      if (mtcs_mode_related_vector_mode/*!related_vector_mode*/(mtcsMode,base_mode, smode).exists (&vmode)){
         tree vectype = mtcs_tree_build_vector_type_for_mode/*!build_vector_type_for_mode*/(mtcsTree,type, vmode);
         if (mtcs_internal_fn_direct_internal_fn_supported_p/*!direct_internal_fn_supported_p*/(self,ifn, vectype, OPTIMIZE_FOR_SPEED))
            return true;
      }

   return false;
}

static void mtcs_expand_SHUFFLEVECTOR (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

static void mtcs_expand_PHI (MtcsInternalFn *self,internal_fn, gcall *)
{
   gcc_unreachable ();
}

static void mtcs_expand_SPACESHIP (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);

   tree lhs = gimple_call_lhs (stmt);
   tree rhs1 = gimple_call_arg (stmt, 0);
   tree rhs2 = gimple_call_arg (stmt, 1);
   tree rhs3 = gimple_call_arg (stmt, 2);
   tree type = TREE_TYPE (rhs1);

   mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);

   rtx target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   rtx op1 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,rhs1);
   rtx op2 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,rhs2);
   rtx op3 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,rhs3);

   class expand_operand ops[4];
   create_call_lhs_operand (&ops[0], target, TYPE_MODE (TREE_TYPE (lhs)));
   create_input_operand (&ops[1], op1, TYPE_MODE (type));
   create_input_operand (&ops[2], op2, TYPE_MODE (type));
   create_input_operand (&ops[3], op3, TYPE_MODE (TREE_TYPE (rhs3)));
   insn_code icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,spaceship_optab, TYPE_MODE (type));
   mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,icode, 4, ops);
   assign_call_lhs(self,lhs, target, &ops[0]);
}

//原型 expand_ASSUME internal-fn.h internal-fn.cc
static void mtcs_expand_ASSUME (MtcsInternalFn *self,internal_fn, gcall *)
{
}

//原型 expand_MASK_CALL internal-fn.h internal-fn.cc
static void mtcs_expand_MASK_CALL (MtcsInternalFn *self,internal_fn, gcall *)
{
  /* This IFN should only exist between ifcvt and vect passes.  */
  gcc_unreachable ();
}

//原型 expand_MULBITINT internal-fn.h internal-fn.cc
static void mtcs_expand_MULBITINT (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);

  mtcs_rtx_mode_t args[6];
  for (int i = 0; i < 6; i++)
    args[i] = mtcs_rtx_mode_t (mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,gimple_call_arg (stmt, i)),
           (i & 1) ? mtcsMode->modes.M_SImode : ptr_mode);
  rtx fun = mtcs_libfuncs_init_one_libfunc/*!init_one_libfunc*/(mtcsLibfuncs,"__mulbitint3");
  mtcs_calls_emit_library_call_value_1/*!emit_library_call_value_1*/(mtcsCalls,0, fun, NULL_RTX, LCT_NORMAL, VOIDmode, 6, args);
}

//原型 expand_DIVMODBITINT internal-fn.h internal-fn.cc
static void mtcs_expand_DIVMODBITINT (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);

   mtcs_rtx_mode_t args[8];
   for (int i = 0; i < 8; i++)
      args[i] = mtcs_rtx_mode_t (mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,
            gimple_call_arg (stmt, i)),(i & 1) ? mtcsMode->modes.M_SImode : ptr_mode);
   rtx fun = mtcs_libfuncs_init_one_libfunc/*!init_one_libfunc*/(mtcsLibfuncs,"__divmodbitint4");
   mtcs_calls_emit_library_call_value_1/*!emit_library_call_value_1*/(mtcsCalls,
         0, fun, NULL_RTX, LCT_NORMAL, VOIDmode, 8, args);
}

//原型 expand_DIVMODBITINT internal-fn.h internal-fn.cc
static void mtcs_expand_FLOATTOBITINT (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);

   machine_mode mode = TYPE_MODE (TREE_TYPE (gimple_call_arg (stmt, 2)));
   rtx arg0 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,gimple_call_arg (stmt, 0));
   rtx arg1 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,gimple_call_arg (stmt, 1));
   rtx arg2 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,gimple_call_arg (stmt, 2));
   const char *mname = mtcs_mode_get_name/*!GET_MODE_NAME*/(mtcsMode,mode);
   unsigned mname_len = strlen (mname);
   int len = 12 + mname_len;
   if (mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,mode))
      len += 4;
   char *libfunc_name = XALLOCAVEC (char, len);
   char *p = libfunc_name;
   const char *q;
   if (mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,mode)){
      #if ENABLE_DECIMAL_BID_FORMAT
         memcpy (p, "__bid_fix", 9);
      #else
         memcpy (p, "__dpd_fix", 9);
      #endif
      p += 9;
   }else{
      memcpy (p, "__fix", 5);
      p += 5;
   }
   for (q = mname; *q; q++)
      *p++ = TOLOWER (*q);
   memcpy (p, "bitint", 7);
   rtx fun = mtcs_libfuncs_init_one_libfunc/*!init_one_libfunc*/(mtcsLibfuncs,libfunc_name);
   mtcs_calls_emit_library_call/*!emit_library_call*/(mtcsCalls,
         fun, LCT_NORMAL, VOIDmode, arg0, ptr_mode, arg1,mtcsMode->modes.M_SImode, arg2, mode);
}

//原型 expand_BITINTTOFLOAT internal-fn.h internal-fn.cc
static void mtcs_expand_BITINTTOFLOAT (MtcsInternalFn *self,internal_fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);

   tree lhs = gimple_call_lhs (stmt);
   if (!lhs)
      return;
   machine_mode mode = TYPE_MODE (TREE_TYPE (lhs));
   rtx arg0 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,gimple_call_arg (stmt, 0));
   rtx arg1 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,gimple_call_arg (stmt, 1));
   const char *mname = mtcs_mode_get_name/*!GET_MODE_NAME*/(mtcsMode,mode);
   unsigned mname_len = strlen (mname);
   int len = 14 + mname_len;
   if (mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,mode))
      len += 4;
   char *libfunc_name = XALLOCAVEC (char, len);
   char *p = libfunc_name;
   const char *q;
   if (mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,mode)){
      #if ENABLE_DECIMAL_BID_FORMAT
         memcpy (p, "__bid_floatbitint", 17);
      #else
         memcpy (p, "__dpd_floatbitint", 17);
      #endif
      p += 17;
   }else{
      memcpy (p, "__floatbitint", 13);
      p += 13;
   }
   for (q = mname; *q; q++)
      *p++ = TOLOWER (*q);
   *p = '\0';
   rtx fun = mtcs_libfuncs_init_one_libfunc/*!init_one_libfunc*/(mtcsLibfuncs,libfunc_name);
   rtx target = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,lhs, NULL_RTX, VOIDmode, EXPAND_WRITE);
   rtx val = mtcs_calls_emit_library_call_value/*!emit_library_call_value*/(mtcsCalls,fun, target, LCT_PURE, mode,arg0, ptr_mode, arg1, SImode);
   if (val != target)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target, val);
}

static bool expand_bitquery (MtcsInternalFn *self,internal_fn fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);

   tree lhs = gimple_call_lhs (stmt);
   if (lhs == NULL_TREE)
      return false;
   tree arg = gimple_call_arg (stmt, 0);
   if (TREE_CODE (arg) == INTEGER_CST){
      tree ret = fold_const_call (as_combined_fn (fn), TREE_TYPE (arg), arg);
      gcc_checking_assert (ret && TREE_CODE (ret) == INTEGER_CST);
      mtcs_expr_expand_assignment/*!expand_assignment*/(mtcsExpr,lhs, ret, false);
      return false;
   }
   return true;
}

//原型 expand_CLRSB internal-fn.h internal-fn.cc
static void mtcs_expand_CLRSB (MtcsInternalFn *self,internal_fn fn, gcall *stmt)
{
  if (expand_bitquery(self,fn, stmt))
    expand_unary_optab_fn(self,fn, stmt, clrsb_optab);
}

//原型 expand_CLZ internal-fn.h internal-fn.cc
static void mtcs_expand_CLZ (MtcsInternalFn *self,internal_fn fn, gcall *stmt)
{
  if (expand_bitquery(self,fn, stmt))
    expand_unary_optab_fn(self,fn, stmt, clz_optab);
}

//原型 expand_CTZ internal-fn.h internal-fn.cc
static void mtcs_expand_CTZ (MtcsInternalFn *self,internal_fn fn, gcall *stmt)
{
  if (expand_bitquery(self,fn, stmt))
    expand_unary_optab_fn(self,fn, stmt, ctz_optab);
}

//原型 expand_FFS internal-fn.h internal-fn.cc
static void mtcs_expand_FFS (MtcsInternalFn *self,internal_fn fn, gcall *stmt)
{
  if (expand_bitquery(self,fn, stmt))
    expand_unary_optab_fn(self,fn, stmt, ffs_optab);
}

//原型 expand_PARITY internal-fn.h internal-fn.cc
static void mtcs_expand_PARITY (MtcsInternalFn *self,internal_fn fn, gcall *stmt)
{
  if (expand_bitquery(self,fn, stmt))
    expand_unary_optab_fn(self,fn, stmt, parity_optab);
}

//原型 expand_POPCOUNT internal-fn.h internal-fn.cc
static void mtcs_expand_POPCOUNT (MtcsInternalFn *self,internal_fn fn, gcall *stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsDojump   *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsExpmed   *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsPredict   *mtcsPredict =mtcs_target_get_predict(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   if (!expand_bitquery(self,fn, stmt))
      return;
   if (gimple_call_num_args (stmt) == 1){
      expand_unary_optab_fn(self,fn, stmt, popcount_optab);
      return;
   }
   /* If .POPCOUNT call has 2 arguments, match_single_bit_test marked it
   because the result is only used in an equality comparison against 1.
   Use rtx costs in that case to determine if .POPCOUNT (arg) == 1
   or (arg ^ (arg - 1)) > arg - 1 is cheaper.
   If .POPCOUNT second argument is 0, we additionally know that arg
   is non-zero, so use arg & (arg - 1) == 0 instead.
   If .POPCOUNT second argument is -1, the comparison was either `<= 1`
   or `> 1`.  */
   bool speed_p = mtcs_predict_optimize_insn_for_speed_p/*!optimize_insn_for_speed_p*/(mtcsPredict);
   tree lhs = gimple_call_lhs (stmt);
   tree arg = gimple_call_arg (stmt, 0);
   bool nonzero_arg = integer_zerop (gimple_call_arg (stmt, 1));
   bool was_le = integer_minus_onep (gimple_call_arg (stmt, 1));
   if (was_le)
      nonzero_arg = true;
   tree type = TREE_TYPE (arg);
   machine_mode mode = TYPE_MODE (type);
   machine_mode lhsmode = TYPE_MODE (TREE_TYPE (lhs));
   mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
   mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
   expand_unary_optab_fn(self,fn, stmt, popcount_optab);
   rtx_insn *popcount_insns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
   rtx plhs = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,lhs);
   rtx pcmp = mtcs_expmed_emit_store_flag/*!emit_store_flag*/(mtcsExpmed,NULL_RTX, EQ, plhs, const1_rtx, lhsmode, 0, 0);
   if (pcmp == NULL_RTX){
fail:
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,popcount_insns);
      return;
   }
   rtx_insn *popcount_cmp_insns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
   rtx op0 = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,arg);
   rtx argm1 = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,mode, PLUS, op0, constm1_rtx, NULL_RTX, 1, OPTAB_WIDEN);
   if (argm1 == NULL_RTX)
      goto fail;
   rtx argxorargm1 = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
         mode, nonzero_arg ? AND : XOR, op0,argm1, NULL_RTX, 1, OPTAB_WIDEN);
   if (argxorargm1 == NULL_RTX)
      goto fail;
   rtx cmp;
   if (nonzero_arg)
      cmp = mtcs_expmed_emit_store_flag/*!emit_store_flag*/(mtcsExpmed,NULL_RTX, EQ, argxorargm1, const0_rtx, mode, 1, 1);
   else
      cmp = mtcs_expmed_emit_store_flag/*!emit_store_flag*/(mtcsExpmed,NULL_RTX, GTU, argxorargm1, argm1, mode, 1, 1);
   if (cmp == NULL_RTX)
      goto fail;
   rtx_insn *cmp_insns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   unsigned popcount_cost = (mtcs_rtlanal_seq_cost/*!seq_cost*/(mtcsRtlanal,popcount_insns, speed_p)
         + mtcs_rtlanal_seq_cost/*!seq_cost*/(mtcsRtlanal,popcount_cmp_insns, speed_p));
   unsigned cmp_cost = mtcs_rtlanal_seq_cost/*!seq_cost*/(mtcsRtlanal,cmp_insns, speed_p);

   if (dump_file && (dump_flags & TDF_DETAILS))
      fprintf(dump_file, "popcount == 1: popcount cost: %u; cmp cost: %u\n",popcount_cost, cmp_cost);

   if (popcount_cost <= cmp_cost)
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,popcount_insns);
   else{
      mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,cmp_insns);
      plhs = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,lhs);
      if (GET_MODE (cmp) != GET_MODE (plhs))
         cmp = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,GET_MODE (plhs), cmp, 1);
      /* For `<= 1`, we need to produce `2 - cmp` or `cmp ? 1 : 2` as that
      then gets compared against 1 and we need the false case to be 2.  */
      if (was_le){
         cmp = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
               GET_MODE (cmp), MINUS, const2_rtx,cmp, NULL_RTX, 1, OPTAB_WIDEN);
         if (!cmp)
            goto fail;
      }
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,plhs, cmp);
      rtx_insn *all_insns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,all_insns);
   }
}

/* Routines to expand each internal function, indexed by function number.
   Each routine has the prototype:

       expand_<NAME> (gcall *stmt)

   where STMT is the statement that performs the call. */
static void (*const internal_fn_expanders[]) (MtcsInternalFn *,internal_fn, gcall *) = {

#define DEF_INTERNAL_FN(CODE, FLAGS, FNSPEC) mtcs_expand_##CODE,
#include "internal-fn.def"
  0
};

/* Expand STMT as though it were a call to internal function FN.  */
//原型 expand_internal_call internal-fn.h internal-fn.cc
void mtcs_internal_fn_expand_internal_call (MtcsInternalFn *self,internal_fn fn, gcall *stmt)
{
   n_debug("mtcsinternalfn.c mtcs_internal_fn_expand_internal_call fn:%d %s\n",fn,internal_fn_name_array[fn]);
   internal_fn_expanders[fn] (self,fn, stmt);
}

/* Expand STMT, which is a call to internal function FN.  */
//原型 expand_internal_call internal-fn.h internal-fn.cc
void mtcs_internal_fn_expand_internal_call (MtcsInternalFn *self,gcall *stmt)
{
   mtcs_internal_fn_expand_internal_call/*!expand_internal_call*/(self,gimple_call_internal_fn (stmt), stmt);
}

/**
 * 是否支持divmod
 */
//原型 target_supports_divmod_p tree-ssa-math-opts.cc
bool mtcs_internal_fn_supports_divmod_p(MtcsInternalFn *self,bool unsign, machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit   *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);

   optab divmod_optab, div_optab;
   if (unsign){
      divmod_optab = udivmod_optab;
      div_optab = udiv_optab;
   }else{
      divmod_optab = sdivmod_optab;
      div_optab = sdiv_optab;
   }

   /* If target supports hardware divmod insn, use it for divmod.  */
   if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,divmod_optab, mode) != CODE_FOR_nothing)
      return true;

   /* Check if libfunc for divmod is available.  */
   rtx libfunc = mtcs_libfuncs_optab_libfunc/*!optab_libfunc*/(mtcsLibfuncs,divmod_optab, mode);
   if (libfunc != NULL_RTX){
      /* If optab_handler exists for div_optab, perhaps in a wider mode,
      we don't want to use the libfunc even if it exists for given mode.  */
      machine_mode div_mode;
      FOR_EACH_MODE_FROM (div_mode, mode)
         if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,div_optab, div_mode) != CODE_FOR_nothing)
            return false;

      return mtcsTarget/*!targetm.expand_divmod_libfunc*/->expand_divmod_libfunc != NULL;
   }
   return false;
}

void mtcs_internal_fn_divmod(MtcsInternalFn *self,rtx q,rtx a,rtx b,rtx r,rtx tmp,machine_mode mode,int unsignedp)
{
   if(self->expandDIVMOD)
      self->expandDIVMOD(self,q,a,b,r,tmp,mode,unsignedp);
}


